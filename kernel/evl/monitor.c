/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/irq_work.h>
#include <evl/thread.h>
#include <evl/mutex.h>
#include <evl/thread.h>
#include <evl/memory.h>
#include <evl/factory.h>
#include <evl/monitor.h>
#include <evl/poll.h>
#include <evl/uaccess.h>
#include <trace/events/evl.h>

static __always_inline  atomic_t *__ATOMIC32(__u32 *ptr)
{
	return (atomic_t *)ptr;
}

struct evl_monitor {
	struct evl_element element;
	struct evl_monitor_state *state;
	int type : 2,
	    protocol : 4;
	union {
		struct {
			struct evl_mutex mutex;
			struct list_head events;
			hard_spinlock_t lock;
		};
		struct {
			/* Out-of-band wait queue. */
			struct evl_wait_queue wait_queue;
			/* In-band wait queue (read side). */
			wait_queue_head_t inband_wait_r;
			/* In-band wait queue (write side). */
			wait_queue_head_t inband_wait_w;
			/* In-band wake up work (read side). */
			struct irq_work inband_wake_r;
			/* In-band wake up work (write side). */
			struct irq_work inband_wake_w;
			/* Gate (valid during active wait only). */
			struct evl_monitor *gate;
			/* Out-of-band poll head. */
			struct evl_poll_head poll_head;
			/* in ->events */
			struct list_head next;
		};
	};
};

static const struct file_operations monitor_fops;

static struct evl_monitor *get_monitor_by_fd(int efd, struct evl_file **efilpp)
{
	struct evl_file *efilp = evl_get_file(efd);

	if (efilp && efilp->filp->f_op == &monitor_fops) {
		*efilpp = efilp;
		return element_of(efilp->filp, struct evl_monitor);
	}

	return NULL;
}

int evl_signal_monitor_targeted(struct evl_thread *target, int monfd)
{
	struct evl_wait_channel *wchan;
	struct evl_monitor *event;
	struct evl_file *efilp;
	unsigned long flags;
	int ret = 0;

	event = get_monitor_by_fd(monfd, &efilp);
	if (event == NULL)
		return -EINVAL;

	if (event->type != EVL_MONITOR_EVENT) {
		ret = -EINVAL;
		goto out;
	}

	/*
	 * Current must hold the gate lock before calling us; if not,
	 * we might race updating state->flags, possibly loosing
	 * events. Too bad.
	 */
	raw_spin_lock_irqsave(&target->lock, flags);

	wchan = evl_get_thread_wchan(target);
	if (wchan == &event->wait_queue.wchan) {
		event->state->flags |= (EVL_MONITOR_TARGETED|
					EVL_MONITOR_SIGNALED);
		raw_spin_lock(&target->rq->lock);
		target->info |= EVL_T_SIGNAL;
		raw_spin_unlock(&target->rq->lock);
	}

	if (wchan)
		evl_put_thread_wchan(wchan);

	raw_spin_unlock_irqrestore(&target->lock, flags);
out:
	evl_put_file(efilp);

	return ret;
}

void __evl_commit_monitor_ceiling(void)
{
	struct evl_thread *curr = evl_current();
	struct evl_monitor *gate;

	/*
	 * curr->u_window has to be valid since curr bears EVL_T_USER.  If
	 * pp_pending is a bad handle, just skip ceiling.
	 */
	gate = evl_get_factory_element_by_fundle(&evl_monitor_factory,
						curr->u_window->pp_pending,
						struct evl_monitor);
	if (gate == NULL)
		goto out;

	if (gate->protocol == EVL_GATE_PP)
		evl_commit_mutex_ceiling(&gate->mutex);

	evl_put_element(&gate->element);
out:
	curr->u_window->pp_pending = EVL_NO_HANDLE;
}

/* gate->lock + event->wait_queue.wchan.lock held, irqs off */
static void untrack_event(struct evl_monitor *event, struct evl_monitor *gate)
{
	assert_hard_lock(&gate->lock);
	assert_hard_lock(&event->wait_queue.wchan.lock);

	/*
	 * If no more waiter is pending on this event, have the gate
	 * stop tracking it.
	 */
	if (event->gate == gate && !evl_wait_active(&event->wait_queue)) {
		list_del(&event->next);
		event->gate = NULL;
		event->state->u.event.gate_offset = EVL_MONITOR_NOGATE;
	}
}

/* gate->lock + event->wait_queue.wchan.lock held, irqs off */
static void wakeup_waiters(struct evl_monitor *event, struct evl_monitor *gate)
{
	struct evl_monitor_state *state = event->state;
	struct evl_thread *waiter, *n;
	bool bcast;

	bcast = !!(state->flags & EVL_MONITOR_BROADCAST);

	/*
	 * We are called upon exiting a gate which serializes access
	 * to a signaled event. Unblock the thread(s) satisfied by the
	 * signal, either all of them, a designated set or the first
	 * waiter in line, depending on whether this is due to a
	 * broadcast, targeted or regular notification.
	 *
	 * Precedence order for event delivery is as follows:
	 * broadcast > targeted > regular.  This means that a
	 * broadcast notification is considered first and applied if
	 * detected. Otherwise, and in presence of a targeted wake up
	 * request, only target threads are resumed. Otherwise, the
	 * thread heading the wait queue is readied.
	 */
	if (evl_wait_active(&event->wait_queue)) {
		if (bcast) {
			evl_flush_wait_locked(&event->wait_queue, 0);
		} else if (state->flags & EVL_MONITOR_TARGETED) {
			evl_for_each_waiter_safe(waiter, n,
						&event->wait_queue) {
				if (waiter->info & EVL_T_SIGNAL)
					evl_wake_up(&event->wait_queue,
						waiter, 0);
			}
		} else {
			evl_wake_up_head(&event->wait_queue);
		}
		untrack_event(event, gate);
	} /* Otherwise, spurious wakeup (fine, might happen). */

	state->flags &= ~(EVL_MONITOR_SIGNALED|
			EVL_MONITOR_BROADCAST|
			EVL_MONITOR_TARGETED);
}

static int __enter_monitor(struct evl_monitor *gate,
			   struct timespec64 *ts64)
{
	ktime_t timeout = EVL_INFINITE;
	enum evl_tmode tmode;

	if (ts64)
		timeout = timespec64_to_ktime(*ts64);

	tmode = timeout ? EVL_ABS : EVL_REL;

	return evl_lock_mutex_timeout(&gate->mutex, timeout, tmode);
}

static int enter_monitor(struct evl_monitor *gate,
			 struct timespec64 *ts64)
{
	struct evl_thread *curr = evl_current();

	if (gate->type != EVL_MONITOR_GATE)
		return -EINVAL;

	if (evl_is_mutex_owner(gate->mutex.fastlock, fundle_of(curr)))
		return -EDEADLK; /* Deny recursive locking. */

	evl_commit_monitor_ceiling();

	return __enter_monitor(gate, ts64);
}

static int tryenter_monitor(struct evl_monitor *gate)
{
	if (gate->type != EVL_MONITOR_GATE)
		return -EINVAL;

	evl_commit_monitor_ceiling();

	return evl_trylock_mutex(&gate->mutex);
}

static void __exit_monitor(struct evl_monitor *gate,
			struct evl_thread *curr)
{
	/*
	 * If we are about to release the lock which is still pending
	 * PP (i.e. we never got scheduled out while holding it),
	 * clear the lazy handle.
	 */
	if (fundle_of(gate) == curr->u_window->pp_pending)
		curr->u_window->pp_pending = EVL_NO_HANDLE;

	__evl_unlock_mutex(&gate->mutex);
}

static int exit_monitor(struct evl_monitor *gate)
{
	struct evl_monitor_state *state = gate->state;
	struct evl_thread *curr = evl_current();
	struct evl_monitor *event, *n;
	unsigned long flags;

	if (gate->type != EVL_MONITOR_GATE)
		return -EINVAL;

	if (!evl_is_mutex_owner(gate->mutex.fastlock, fundle_of(curr)))
		return -EPERM;

	/*
	 * Locking order is gate lock first, depending event lock(s)
	 * next.
	 */
	raw_spin_lock_irqsave(&gate->lock, flags);

	/*
	 * While gate.mutex is still held by current, we can
	 * manipulate the state flags racelessly.
	 */
	if (state->flags & EVL_MONITOR_SIGNALED) {
		state->flags &= ~EVL_MONITOR_SIGNALED;
		list_for_each_entry_safe(event, n, &gate->events, next) {
			raw_spin_lock(&event->wait_queue.wchan.lock);
			if (event->state->flags & EVL_MONITOR_SIGNALED)
				wakeup_waiters(event, gate);
			raw_spin_unlock(&event->wait_queue.wchan.lock);
		}
	}

	/*
	 * The whole wakeup+exit sequence must appear as atomic, drop
	 * the gate lock last so that we are fully covered until the
	 * monitor is released.
	 */
	__exit_monitor(gate, curr);

	raw_spin_unlock_irqrestore(&gate->lock, flags);

	evl_schedule();

	return 0;
}

static int trywait_count(struct evl_monitor *event)
{
	struct evl_monitor_state *state = event->state;
	int ret = 0, val;

	/* atomic_dec_unless_zero_or_negative */
	val = atomic_read(__ATOMIC32(&state->u.event.value));
	do {
		if (unlikely(val <= 0)) {
			ret = -EAGAIN;
			break;
		}
	} while (!atomic_try_cmpxchg(__ATOMIC32(&state->u.event.value), &val, val - 1));

	return ret;
}

static int wait_count(struct file *filp,
		ktime_t timeout,
		enum evl_tmode tmode)
{
	struct evl_monitor *event = element_of(filp, struct evl_monitor);
	struct evl_monitor_state *state = event->state;
	unsigned long flags;
	int ret = 0;

	if (filp->f_flags & O_NONBLOCK) {
		ret = trywait_count(event);
	} else {
		/*
		 * CAUTION: we must fully serialize with
		 * post_count(). Since user-space is expected to
		 * trywait first before branching here, we are most
		 * likely going to wait anyway.
		 */
		raw_spin_lock_irqsave(&event->wait_queue.wchan.lock, flags);
		if (atomic_dec_return(__ATOMIC32(&state->u.event.value)) < 0) {
			evl_add_wait_queue(&event->wait_queue,
					timeout, tmode);
			raw_spin_unlock_irqrestore(&event->wait_queue.wchan.lock,
						flags);
			ret = evl_wait_schedule(&event->wait_queue);
			if (ret) { /* Rollback decrement if failed. */
				atomic_inc(__ATOMIC32(&state->u.event.value));
			} else {
				/*
				 * If waking up on a broadcast, we did
				 * not actually receive a free pass,
				 * make the caller notice by returning
				 * -EAGAIN.
				 */
				if (evl_current()->info & EVL_T_BCAST)
					ret = -EAGAIN;
			}
		} else {
			raw_spin_unlock_irqrestore(&event->wait_queue.wchan.lock,
						flags);
		}
	}

	return ret;
}

static int post_count(struct evl_monitor *event, s32 sigval,
		bool bcast)
{
	struct evl_monitor_state *state = event->state;
	bool pollable = true;
	unsigned long flags;
	int ret = 0, val;

	/*
	 * We may receive a null sigval for the purpose of triggering
	 * a poll notification without updating the count.
	 *
	 * Otherwise, we can either post a single waiter or broadcast
	 * the semaphore which unblocks all waiters at once, making
	 * them know they were not granted anything in the process
	 * though. Broadcasting here is equivalent to a flush
	 * operation.
	 */
	if (sigval) {
		raw_spin_lock_irqsave(&event->wait_queue.wchan.lock, flags);

		if (bcast) {
			val = evl_flush_wait_locked(&event->wait_queue, EVL_T_BCAST);
			/*
			 * Each waiter found sleeping on the wait
			 * queue counts for 1 semaphore unit to be
			 * added back to the count.
			 */
			if (val > 0)
				atomic_add(val, __ATOMIC32(&state->u.event.value));
			/* Userland might have slipped in, re-check. */
			pollable = atomic_read(__ATOMIC32(&state->u.event.value)) > 0;
		} else {
			if (atomic_inc_return(__ATOMIC32(&state->u.event.value)) <= 0) {
				evl_wake_up_head(&event->wait_queue);
				pollable = false;
			}
		}

		raw_spin_unlock_irqrestore(&event->wait_queue.wchan.lock, flags);
	}

	if (pollable)
		evl_signal_poll_events(&event->poll_head, POLLIN|POLLRDNORM);

	evl_schedule();

	return ret;
}

static void inband_wake_r_irqwork(struct irq_work *work) /* in-band */
{
	struct evl_monitor *event;

	event = container_of(work, struct evl_monitor, inband_wake_r);
	wake_up(&event->inband_wait_r);
	evl_put_element(&event->element);
}

static void inband_wake_w_irqwork(struct irq_work *work) /* in-band */
{
	struct evl_monitor *event;

	event = container_of(work, struct evl_monitor, inband_wake_w);
	wake_up(&event->inband_wait_w);
	evl_put_element(&event->element);
}

/* event->wait_queue.wchan.lock held, dropped on success, irqs off. */
static bool __trywait_mask(struct evl_monitor *event,
			s32 match_value,
			bool exact_match,
			s32 *r_value,
			unsigned long flags)
{
	struct evl_monitor_state *state = event->state;
	int testval;

	*r_value = atomic_read(__ATOMIC32(&state->u.event.value)) & match_value;
	testval = exact_match ? match_value : *r_value;
	if (*r_value && *r_value == testval) {
		atomic_andnot(*r_value, __ATOMIC32(&state->u.event.value));
		testval = atomic_read(__ATOMIC32(&state->u.event.value));
		raw_spin_unlock_irqrestore(&event->wait_queue.wchan.lock, flags);
		if (!testval) {
			evl_signal_poll_events(&event->poll_head, POLLOUT|POLLWRNORM);
			evl_schedule();
		}
		return true;
	}

	return false;
}

static int trywait_mask_oob(struct evl_monitor *event,
			s32 match_value,
			bool exact_match,
			s32 *r_value)
{
	unsigned long flags;

	/*
	 * Waiting for no bit is interpreted as waiting for any/all of
	 * them (depending on exact_match).
	 */
	if (match_value == 0)
		match_value = -1;

	raw_spin_lock_irqsave(&event->wait_queue.wchan.lock, flags);
	if (!__trywait_mask(event, match_value, exact_match, r_value, flags)) {
		raw_spin_unlock_irqrestore(&event->wait_queue.wchan.lock, flags);
		return -EAGAIN;
	}

	return 0;
}

struct evl_mask_wait {
	/* Value to wait for. */
	int value;
	/* i.e. Conjunctive/AND/ALL match */
	bool exact_match;
};

static int wait_mask_oob(struct file *filp,
		ktime_t timeout,
		enum evl_tmode tmode,
		s32 match_value,
		bool exact_match,
		s32 *r_value)
{
	struct evl_monitor *event = element_of(filp, struct evl_monitor);
	struct evl_thread *curr = evl_current();
	struct evl_mask_wait w;
	unsigned long flags;
	int ret;

	if (match_value == 0)	/* See trywait_mask(). */
		match_value = -1;

	raw_spin_lock_irqsave(&event->wait_queue.wchan.lock, flags);

	if (__trywait_mask(event, match_value, exact_match, r_value, flags))
		return 0;	/* oob lock already dropped on success. */

	if (filp->f_flags & O_NONBLOCK) {
		raw_spin_unlock_irqrestore(&event->wait_queue.wchan.lock, flags);
		return -EAGAIN;
	}

	w.exact_match = exact_match;
	w.value = match_value;
	curr->wait_data = &w;
	evl_add_wait_queue(&event->wait_queue, timeout, tmode);

	raw_spin_unlock_irqrestore(&event->wait_queue.wchan.lock, flags);

	ret = evl_wait_schedule(&event->wait_queue);
	if (!ret)
		*r_value = w.value;

	return ret;
}

static int post_mask(struct evl_monitor *event, int bits, bool bcast)
{
	struct evl_monitor_state *state = event->state;
	int waitval, testval, consumed = 0, val;
	struct evl_thread *waiter, *tmp;
	struct evl_mask_wait *w;
	unsigned long flags;

	raw_spin_lock_irqsave(&event->wait_queue.wchan.lock, flags);

	/*
	 * NOTE: the only reasons we still have state->u.event.value
	 * as an atomic value ATM is strictly for ABI preservation,
	 * and allow for peeking at the mask value directly from
	 * userland (which is hardly a common operation and could be
	 * done differently anyway). We may turn this into a plain
	 * word when an ABI jump is required from applications for
	 * some compelling reason.
	 */
	atomic_or(bits, __ATOMIC32(&state->u.event.value));

	evl_for_each_waiter_safe(waiter, tmp, &event->wait_queue) {
		w = waiter->wait_data;
		waitval = w->value & atomic_read(__ATOMIC32(&state->u.event.value));
		testval = w->exact_match ? w->value : waitval;
		if (waitval && waitval == testval) {
			w->value = waitval;
			consumed |= waitval;
			evl_wake_up(&event->wait_queue, waiter, 0);
			if (!bcast)
				break;
		}
	}

	if (consumed)
		atomic_andnot(consumed, __ATOMIC32(&state->u.event.value));

	val = atomic_read(__ATOMIC32(&state->u.event.value));

	raw_spin_unlock_irqrestore(&event->wait_queue.wchan.lock, flags);

	evl_signal_poll_events(&event->poll_head,
			val ? POLLIN|POLLRDNORM : POLLOUT|POLLWRNORM);

	evl_schedule();

	/*
	 * Now wake up any in-band waiter if we still have events to
	 * consume.
	 */
	smp_mb();
	if (val && waitqueue_active(&event->inband_wait_r)) {
		if (running_inband()) {
			wake_up(&event->inband_wait_r);
		} else {
			evl_get_element(&event->element);
			if (!irq_work_queue(&event->inband_wake_r))
				evl_put_element(&event->element);
		}
	}

	return 0;
}

static int wait_gated_event(struct evl_monitor *event,
			struct evl_monitor_waitreq *req,
			ktime_t timeout,
			enum evl_tmode tmode,
			s32 *r_op_ret)
{
	struct evl_thread *curr = evl_current();
	struct evl_monitor *gate;
	int ret = 0, op_ret = 0;
	struct evl_file *efilp;
	unsigned long flags;
	struct evl_rq *rq;

	if (event->protocol != EVL_EVENT_GATED) {
		op_ret = -EINVAL;
		goto out;
	}

	/* Find the gate monitor protecting us. */
	gate = get_monitor_by_fd(req->gatefd, &efilp);
	if (gate == NULL) {
		op_ret = -EINVAL;
		goto out;
	}

	if (gate->type != EVL_MONITOR_GATE) {
		op_ret = -EINVAL;
		goto put;
	}

	/* Make sure we actually passed the gate. */
	if (!evl_is_mutex_owner(gate->mutex.fastlock, fundle_of(curr))) {
		op_ret = -EPERM;
		goto put;
	}

	raw_spin_lock_irqsave(&gate->lock, flags);

	/*
	 * Track event monitors the gate protects. When multiple
	 * threads issue concurrent wait requests on the same event
	 * monitor, they must use the same gate to serialize. Don't
	 * trust userland for maintaining sane tracking info in
	 * gate_offset, keep event->gate on the kernel side for this.
	 */
	if (event->gate == NULL) {
		list_add_tail(&event->next, &gate->events);
		event->gate = gate;
		event->state->u.event.gate_offset = evl_shared_offset(gate->state);
	} else if (event->gate != gate) {
		raw_spin_unlock_irqrestore(&gate->lock, flags);
		op_ret = -EBADFD;
		goto put;
	}

	/*
	 * Since we still hold the mutex until __exit_monitor() is
	 * called later on, do not perform the WOLI checks when
	 * enqueuing.
	 */
	raw_spin_lock(&event->wait_queue.wchan.lock);
	evl_add_wait_queue_unchecked(&event->wait_queue, timeout, tmode);
	raw_spin_unlock(&event->wait_queue.wchan.lock);

	rq = evl_get_thread_rq_noirq(curr);
	curr->info &= ~EVL_T_SIGNAL;
	evl_put_thread_rq_noirq(curr, rq);

	__exit_monitor(gate, curr); /* See comment in exit_monitor(). */

	raw_spin_unlock_irqrestore(&gate->lock, flags);

	/*
	 * Actually wait on the event. If a break condition is raised
	 * such as an inband signal pending, do not attempt to
	 * reacquire the gate lock just yet as this might block
	 * indefinitely (in theory) and we want the inband signal to
	 * be handled asap. So exit to user mode, allowing any pending
	 * signal to be handled during the transition, then expect
	 * userland to issue UNWAIT to recover (or exit, whichever
	 * comes first).
	 *
	 * Consequently, disable syscall restart from kernel upon
	 * interrupted wait, because the caller does not hold the
	 * mutex until UNWAIT happens.
	 */
	ret = evl_wait_schedule(&event->wait_queue);
	if (ret) {
		raw_spin_lock_irqsave(&gate->lock, flags);
		raw_spin_lock(&event->wait_queue.wchan.lock);
		untrack_event(event, gate);
		raw_spin_unlock(&event->wait_queue.wchan.lock);
		raw_spin_unlock_irqrestore(&gate->lock, flags);

		/*
		 * Disable syscall restart upon signal (only), user
		 * receives -EINTR and a zero status in this case. If
		 * the caller was forcibly unblocked for any other
		 * reason, both the return value and the status word
		 * are set to -EINTR.
		 */
		if (ret == -EINTR && signal_pending(current)) {
			curr->local_info |= EVL_T_NORST;
			goto put;
		}
		op_ret = ret;
		if (ret == -EIDRM)
			goto put;
	}

	ret = __enter_monitor(gate, NULL);
	if (ret == -EINTR) {
		if (signal_pending(current))
			curr->local_info |= EVL_T_NORST;
		op_ret = -EAGAIN;
	}
put:
	evl_put_file(efilp);
out:
	*r_op_ret = op_ret;

	return ret;
}

static int wait_monitor(struct file *filp,
			struct evl_monitor_waitreq *req,
			struct timespec64 *ts64,
			s32 *r_op_ret,
			s32 *r_value,
			bool exact_match)
{
	struct evl_monitor *event = element_of(filp, struct evl_monitor);
	enum evl_tmode tmode;
	ktime_t timeout;

	if (event->type != EVL_MONITOR_EVENT) {
		*r_op_ret = -EINVAL;
		return 0;
	}

	timeout = timespec64_to_ktime(*ts64);
	tmode = timeout ? EVL_ABS : EVL_REL;

	if (req->gatefd < 0) {
		switch (event->protocol) {
		case EVL_EVENT_COUNT:
			*r_op_ret = wait_count(filp, timeout, tmode);
			break;
		case EVL_EVENT_MASK:
			*r_op_ret = wait_mask_oob(filp, timeout, tmode, req->value,
						exact_match, r_value);
			break;
		default:
			*r_op_ret = -EINVAL;
		}
		return *r_op_ret;
	}

	return wait_gated_event(event, req, timeout, tmode, r_op_ret);
}

static int unwait_monitor(struct evl_monitor *event,
			struct evl_monitor_unwaitreq *req)
{
	struct evl_monitor *gate;
	struct evl_file *efilp;
	int ret;

	if (event->type != EVL_MONITOR_EVENT)
		return -EINVAL;

	/* Find the gate monitor we need to re-acquire. */
	gate = get_monitor_by_fd(req->gatefd, &efilp);
	if (gate == NULL)
		return -EINVAL;

	ret = enter_monitor(gate, NULL);

	evl_put_file(efilp);

	return ret;
}

static long monitor_common_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct evl_monitor *event = element_of(filp, struct evl_monitor);
	struct evl_monitor_trywaitreq twreq, __user *u_twreq;
	bool bcast = false, exact_match = false;
	__s32 value;
	int ret;

	if (event->type != EVL_MONITOR_EVENT)
		return -EINVAL;

	switch (cmd) {
	case EVL_MONIOC_BROADCAST:
		bcast = true;
		fallthrough;
	case EVL_MONIOC_SIGNAL:
		if (raw_get_user(value, (__s32 __user *)arg))
			return -EFAULT;
		switch (event->protocol) {
		case EVL_EVENT_COUNT:
			ret = post_count(event, value, bcast);
			break;
		case EVL_EVENT_MASK:
			ret = post_mask(event, value, bcast);
			break;
		default:
			ret = -EINVAL;
		}
		break;
	case EVL_MONIOC_TRYWAIT_EXACT:
		exact_match = true;
		fallthrough;
	case EVL_MONIOC_TRYWAIT:
		u_twreq = (typeof(u_twreq))arg;
		ret = raw_copy_from_user(&twreq, u_twreq, sizeof(twreq));
		if (ret)
			return -EFAULT;
		switch (event->protocol) {
		case EVL_EVENT_COUNT:
			ret = trywait_count(event);
			break;
		case EVL_EVENT_MASK:
			ret = trywait_mask_oob(event, twreq.value, exact_match, &value);
			if (!ret)
				raw_put_user(value, &u_twreq->value);
			break;
		default:
			ret = -EINVAL;
		}
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static long monitor_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_monitor *mon = element_of(filp, struct evl_monitor);
	struct evl_monitor_binding bind, __user *u_bind;

	if (cmd != EVL_MONIOC_BIND)
		return monitor_common_ioctl(filp, cmd, arg);

	bind.type = mon->type;
	bind.protocol = mon->protocol;
	bind.eids.minor = mon->element.minor;
	bind.eids.state_offset = evl_shared_offset(mon->state);
	bind.eids.fundle = fundle_of(mon);
	u_bind = (typeof(u_bind))arg;

	return copy_to_user(u_bind, &bind, sizeof(bind)) ? -EFAULT : 0;
}

static long monitor_oob_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_monitor *mon = element_of(filp, struct evl_monitor);
	struct evl_monitor_unwaitreq uwreq, __user *u_uwreq;
	struct evl_monitor_waitreq wreq, __user *u_wreq;
	struct __evl_timespec __user *u_uts;
	struct __evl_timespec uts = {
		.tv_sec = 0,
		.tv_nsec = 0,
	};
	bool exact_match = false;
	struct timespec64 ts64;
	s32 op_ret, value = 0;
	long ret;

	switch (cmd) {
	case EVL_MONIOC_WAIT_EXACT:
		exact_match = true;
		fallthrough;
	case EVL_MONIOC_WAIT:
		u_wreq = (typeof(u_wreq))arg;
		ret = raw_copy_from_user(&wreq, u_wreq, sizeof(wreq));
		if (ret)
			return -EFAULT;
		u_uts = evl_valptr64(wreq.timeout_ptr, struct __evl_timespec);
		ret = raw_copy_from_user(&uts, u_uts, sizeof(uts));
		if (ret)
			return -EFAULT;
		if ((unsigned long)uts.tv_nsec >= ONE_BILLION)
			return -EINVAL;
		ts64 = u_timespec_to_timespec64(uts);
		ret = wait_monitor(filp, &wreq, &ts64, &op_ret, &value, exact_match);
		raw_put_user(op_ret, &u_wreq->status);
		if (!ret && !op_ret)
			raw_put_user(value, &u_wreq->value);
		break;
	case EVL_MONIOC_UNWAIT:
		u_uwreq = (typeof(u_uwreq))arg;
		ret = raw_copy_from_user(&uwreq, u_uwreq, sizeof(uwreq));
		if (ret)
			return -EFAULT;
		ret = unwait_monitor(mon, &uwreq);
		break;
	case EVL_MONIOC_ENTER:
		u_uts = (typeof(u_uts))arg;
		ret = raw_copy_from_user(&uts, u_uts, sizeof(uts));
		if (ret)
			return -EFAULT;
		if ((unsigned long)uts.tv_nsec >= ONE_BILLION)
			return -EINVAL;
		ts64 = u_timespec_to_timespec64(uts);
		ret = enter_monitor(mon, &ts64);
		break;
	case EVL_MONIOC_TRYENTER:
		ret = tryenter_monitor(mon);
		break;
	case EVL_MONIOC_EXIT:
		ret = exit_monitor(mon);
		break;
	default:
		ret = monitor_common_ioctl(filp, cmd, arg);
	}

	return ret;
}

static void monitor_unwatch(struct evl_poll_head *head)
{
	struct evl_monitor *mon;

	mon = container_of(head, struct evl_monitor, poll_head);
	atomic_dec(__ATOMIC32(&mon->state->u.event.pollrefs));
}

static __poll_t monitor_oob_poll(struct file *filp,
				struct oob_poll_wait *wait)
{
	struct evl_monitor *mon = element_of(filp, struct evl_monitor);
	struct evl_monitor_state *state = mon->state;
	__poll_t ret = 0;
	int val;

	/*
	 * NOTE: for ungated events, we close a race window by queuing
	 * the caller into the poll queue _before_ incrementing the
	 * pollrefs count which userland checks.
	 */
	switch (mon->type) {
	case EVL_MONITOR_EVENT:
		switch (mon->protocol) {
		case EVL_EVENT_COUNT:
			evl_poll_watch(&mon->poll_head, wait, monitor_unwatch);
			atomic_inc(__ATOMIC32(&state->u.event.pollrefs));
			if (atomic_read(__ATOMIC32(&state->u.event.value)) > 0)
				ret = POLLIN|POLLRDNORM;
			break;
		case EVL_EVENT_MASK:
			evl_poll_watch(&mon->poll_head, wait, monitor_unwatch);
			/*
			 * We need to keep the pollrefs count up to
			 * date as long as we support legacy ABIs
			 * (pre-32).
			 */
			atomic_inc(__ATOMIC32(&state->u.event.pollrefs));
			val = atomic_read(__ATOMIC32(&state->u.event.value));
			/*
			 * Return POLLIN when some bits are present,
			 * ready for consumption, or POLLOUT when the
			 * mask is entirely clear, i.e. no bit to
			 * consume.
			 */
			if (val)
				ret = POLLIN|POLLRDNORM;
			else
				ret = POLLOUT|POLLWRNORM;
			break;
		case EVL_EVENT_GATED:
			/*
			 * The poll interface does not cope with the
			 * gated event semantics, since we could not
			 * release the gate protecting the event and
			 * enter a poll wait atomically to prevent
			 * missed wakeups.  Therefore, polling a gated
			 * event leads to an error.
			 */
			ret = POLLERR;
			break;
		}
		break;
	case EVL_MONITOR_GATE:
		/*
		 * A mutex should be held only for a short period of
		 * time, with the locked state appearing as a discrete
		 * event to users. Assume a gate lock is always
		 * readable (as "unlocked") then. If this is about
		 * probing for a mutex state from userland then
		 * trylock() should be used instead of poll().
		 */
		ret = POLLIN|POLLRDNORM;
		break;
	}

	return ret;
}

static int monitor_release(struct inode *inode, struct file *filp)
{
	struct evl_monitor *mon = element_of(filp, struct evl_monitor);

	if (mon->type == EVL_MONITOR_EVENT)
		evl_flush_wait(&mon->wait_queue, EVL_T_RMID);
	else
		evl_flush_mutex(&mon->mutex, EVL_T_RMID);

	return evl_release_element(inode, filp);
}

static ssize_t monitor_read(struct file *filp, char __user *u_buf,
			size_t count, loff_t *ppos)
{
	struct evl_monitor *event = element_of(filp, struct evl_monitor);
	struct wait_queue_entry wq_entry;
	unsigned long flags, ib_flags;
	int ret = 0;
	s32 val;

	if (event->type != EVL_MONITOR_EVENT || event->protocol != EVL_EVENT_MASK)
		return -EINVAL;

	if (count != sizeof(s32))
		return -EINVAL;

	init_wait_entry(&wq_entry, 0);

	for (;;) {
		spin_lock_irqsave(&event->inband_wait_r.lock, ib_flags);
		raw_spin_lock_irqsave(&event->wait_queue.wchan.lock, flags);

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		if (list_empty(&wq_entry.entry))
			__add_wait_queue(&event->inband_wait_r, &wq_entry);

		/* Disjunctive operation. */
		if (__trywait_mask(event, -1, false, &val, flags))
			goto unlocked;

		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		set_current_state(TASK_INTERRUPTIBLE);
		raw_spin_unlock_irqrestore(&event->wait_queue.wchan.lock, flags);
		spin_unlock_irqrestore(&event->inband_wait_r.lock, ib_flags);
		schedule();
	}

	raw_spin_unlock_irqrestore(&event->wait_queue.wchan.lock, flags);
unlocked:
	list_del(&wq_entry.entry);

	spin_unlock_irqrestore(&event->inband_wait_r.lock, ib_flags);

	/*
	 * If we have successfully collected all the bits, send a
	 * POLLOUT wakeup.
	 */
	smp_mb();	/* Matches mb in set_current_state() */
	if (!ret && waitqueue_active(&event->inband_wait_w))
		wake_up(&event->inband_wait_w);

	return copy_to_user(u_buf, &val, sizeof(val)) ? -EFAULT : sizeof(val);
}

static ssize_t monitor_write(struct file *filp, const char __user *u_buf,
			size_t count, loff_t *ppos)
{
	struct evl_monitor *event = element_of(filp, struct evl_monitor);
	s32 val;
	int ret;

	if (event->type != EVL_MONITOR_EVENT || event->protocol != EVL_EVENT_MASK)
		return -EINVAL;

	if (count != sizeof(s32))
		return -EINVAL;

	ret = copy_from_user(&val, u_buf, sizeof(val));
	if (ret)
		return -EFAULT;

	/* Unicast operation. */
	return post_mask(event, val, false) ?: sizeof(val);
}

static ssize_t monitor_oob_read(struct file *filp,
				char __user *u_buf, size_t count)
{
	struct evl_monitor *event = element_of(filp, struct evl_monitor);
	s32 val;
	int ret;

	if (event->type != EVL_MONITOR_EVENT || event->protocol != EVL_EVENT_MASK)
		return -EINVAL;

	if (count != sizeof(s32))
		return -EINVAL;

	/* Disjunctive operation. */
	ret = wait_mask_oob(filp, EVL_INFINITE, EVL_REL, -1, false, &val);
	if (ret)
		return ret;

	return copy_to_user(u_buf, &val, sizeof(val)) ? -EFAULT : sizeof(val);
}

static ssize_t monitor_oob_write(struct file *filp,
				const char __user *u_buf, size_t count)
{
	struct evl_monitor *event = element_of(filp, struct evl_monitor);
	s32 val;
	int ret;

	if (event->type != EVL_MONITOR_EVENT || event->protocol != EVL_EVENT_MASK)
		return -EINVAL;

	if (count != sizeof(s32))
		return -EINVAL;

	ret = copy_from_user(&val, u_buf, sizeof(val));
	if (ret)
		return -EFAULT;

	/* Unicast operation. */
	return post_mask(event, val, false) ?: sizeof(val);
}

static __poll_t monitor_poll(struct file *filp, poll_table *wait)
{
	struct evl_monitor *event = element_of(filp, struct evl_monitor);
	__poll_t ret = 0;
	int val;

	if (event->type != EVL_MONITOR_EVENT || event->protocol != EVL_EVENT_MASK)
		return -EINVAL;

	poll_wait(filp, &event->inband_wait_r, wait);
	poll_wait(filp, &event->inband_wait_w, wait);

	val = atomic_read(__ATOMIC32(&event->state->u.event.value));
	if (val)
		ret |= POLLIN|POLLRDNORM;
	else
		ret |= POLLOUT|POLLWRNORM;

	return ret;
}

static const struct file_operations monitor_fops = {
	.open		= evl_open_element,
	.release	= monitor_release,
	.read		= monitor_read,
	.write		= monitor_write,
	.poll		= monitor_poll,
	.unlocked_ioctl	= monitor_ioctl,
	.oob_read	= monitor_oob_read,
	.oob_write	= monitor_oob_write,
	.oob_ioctl	= monitor_oob_ioctl,
	.oob_poll	= monitor_oob_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_ptr_ioctl,
	.compat_oob_ioctl = compat_ptr_oob_ioctl,
#endif
};

static struct evl_element *
monitor_factory_build(struct evl_factory *fac, const char __user *u_name,
		void __user *u_attrs, int clone_flags, u32 *state_offp)
{
	struct evl_monitor_state *state;
	struct evl_monitor_attrs attrs;
	struct evl_monitor *mon;
	struct evl_clock *clock;
	int ret;

	if (clone_flags & ~EVL_CLONE_PUBLIC)
		return ERR_PTR(-EINVAL);

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	switch (attrs.type) {
	case EVL_MONITOR_GATE:
		switch (attrs.protocol) {
		case EVL_GATE_PP:
			if (attrs.initval == 0 ||
				attrs.initval > EVL_FIFO_MAX_PRIO)
				return ERR_PTR(-EINVAL);
			break;
		case EVL_GATE_PI:
			if (attrs.initval)
				return ERR_PTR(-EINVAL);
			break;
		default:
			return ERR_PTR(-EINVAL);
		}
		break;
	case EVL_MONITOR_EVENT:
		switch (attrs.protocol) {
		case EVL_EVENT_GATED:
		case EVL_EVENT_COUNT:
		case EVL_EVENT_MASK:
			break;
		default:
			return ERR_PTR(-EINVAL);
		}
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	clock = evl_get_clock_by_fd(attrs.clockfd);
	if (clock == NULL)
		return ERR_PTR(-EINVAL);

	mon = kzalloc(sizeof(*mon), GFP_KERNEL);
	if (mon == NULL) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	ret = evl_init_user_element(&mon->element, &evl_monitor_factory,
				u_name, clone_flags);
	if (ret)
		goto fail_element;

	state = evl_zalloc_chunk(&evl_shared_heap, sizeof(*state));
	if (state == NULL) {
		ret = -ENOMEM;
		goto fail_heap;
	}

	switch (attrs.type) {
	case EVL_MONITOR_GATE:
		switch (attrs.protocol) {
		case EVL_GATE_PP:
			state->u.gate.ceiling = attrs.initval;
			evl_init_mutex_pp(&mon->mutex, clock,
					__ATOMIC32(&state->u.gate.owner),
					&state->u.gate.ceiling);
			break;
		case EVL_GATE_PI:
			evl_init_mutex_pi(&mon->mutex, clock,
					__ATOMIC32(&state->u.gate.owner));
			break;
		}
		raw_spin_lock_init(&mon->lock);
		INIT_LIST_HEAD(&mon->events);
		break;
	case EVL_MONITOR_EVENT:
		evl_init_wait(&mon->wait_queue, clock, EVL_WAIT_PRIO);
		state->u.event.gate_offset = EVL_MONITOR_NOGATE;
		atomic_set(__ATOMIC32(&state->u.event.value), attrs.initval);
		evl_init_poll_head(&mon->poll_head);
		init_waitqueue_head(&mon->inband_wait_r);
		init_waitqueue_head(&mon->inband_wait_w);
		init_irq_work(&mon->inband_wake_r, inband_wake_r_irqwork);
		init_irq_work(&mon->inband_wake_w, inband_wake_w_irqwork);
	}

	/*
	 * The type information is critical for the kernel sanity,
	 * don't allow userland to mess with it, so don't trust the
	 * shared state for this.
	 */
	mon->type = attrs.type;
	mon->protocol = attrs.protocol;
	mon->state = state;
	*state_offp = evl_shared_offset(state);
	evl_index_factory_element(&mon->element);

	return &mon->element;

fail_heap:
	evl_destroy_element(&mon->element);
fail_element:
	kfree(mon);
fail_alloc:
	evl_put_clock(clock);

	return ERR_PTR(ret);
}

static void monitor_factory_dispose(struct evl_element *e)
{
	struct evl_monitor *mon;
	unsigned long flags;

	mon = container_of(e, struct evl_monitor, element);

	evl_unindex_factory_element(&mon->element);

	if (mon->type == EVL_MONITOR_EVENT) {
		evl_put_clock(mon->wait_queue.clock);
		evl_destroy_wait(&mon->wait_queue);
		if (mon->gate) {
			raw_spin_lock_irqsave(&mon->gate->lock, flags);
			list_del(&mon->next);
			raw_spin_unlock_irqrestore(&mon->gate->lock, flags);
		}
	} else {
		evl_put_clock(mon->mutex.clock);
		evl_destroy_mutex(&mon->mutex);
	}

	evl_free_chunk(&evl_shared_heap, mon->state);
	evl_destroy_element(&mon->element);
	kfree_rcu(mon, element.rcu);
}

static ssize_t state_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct evl_monitor_state *state;
	struct evl_thread *owner = NULL;
	struct evl_monitor *mon;
	ssize_t ret = 0;
	fundle_t fun;

	mon = evl_get_element_by_dev(dev, struct evl_monitor);
	if (mon == NULL)
		return -EIO;

	state = mon->state;

	if (mon->type == EVL_MONITOR_EVENT) {
		switch (mon->protocol) {
		case EVL_EVENT_MASK:
			ret = snprintf(buf, PAGE_SIZE, "%#x\n",
			       atomic_read(__ATOMIC32(&state->u.event.value)));
			break;
		case EVL_EVENT_COUNT:
			ret = snprintf(buf, PAGE_SIZE, "%d\n",
			       atomic_read(__ATOMIC32(&state->u.event.value)));
			break;
		case EVL_EVENT_GATED:
			ret = snprintf(buf, PAGE_SIZE, "%#x\n",
				state->flags);
			break;
		}
	} else {
		fun = atomic_read(__ATOMIC32(&state->u.gate.owner));
		if (fun != EVL_NO_HANDLE) {
			owner = evl_get_factory_element_by_fundle(&evl_thread_factory,
						evl_get_index(fun),
						struct evl_thread);
			if (!owner)
				goto no_owner;
			ret = snprintf(buf, PAGE_SIZE, "%s(%d) %u %u\n",
				evl_element_name(&owner->element),
				evl_get_inband_pid(owner),
				state->u.gate.ceiling,
				state->u.gate.recursive ? state->u.gate.nesting : 1);
			evl_put_element(&owner->element);
		} else {
		no_owner:
			ret = snprintf(buf, PAGE_SIZE, "-1 %u 0\n",
				state->u.gate.ceiling);
		}
	}

	evl_put_element(&mon->element);

	return ret;
}
static DEVICE_ATTR_RO(state);

static struct attribute *monitor_attrs[] = {
	&dev_attr_state.attr,
	NULL,
};
ATTRIBUTE_GROUPS(monitor);

struct evl_factory evl_monitor_factory = {
	.name	=	EVL_MONITOR_DEV,
	.fops	=	&monitor_fops,
	.build =	monitor_factory_build,
	.dispose =	monitor_factory_dispose,
	.nrdev	=	CONFIG_EVL_NR_MONITORS,
	.attrs	=	monitor_groups,
	.flags	=	EVL_FACTORY_CLONE,
};
