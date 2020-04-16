/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2006, 2016 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <stdarg.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/irq_work.h>
#include <linux/sched/signal.h>
#include <linux/sched/types.h>
#include <linux/sched/task.h>
#include <linux/jiffies.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <linux/ptrace.h>
#include <linux/math64.h>
#include <uapi/linux/sched/types.h>
#include <evl/sched.h>
#include <evl/timer.h>
#include <evl/wait.h>
#include <evl/clock.h>
#include <evl/stat.h>
#include <evl/assert.h>
#include <evl/lock.h>
#include <evl/thread.h>
#include <evl/sched.h>
#include <evl/memory.h>
#include <evl/file.h>
#include <evl/factory.h>
#include <evl/monitor.h>
#include <evl/mutex.h>
#include <evl/poll.h>
#include <evl/flag.h>
#include <asm/evl/syscall.h>
#include <trace/events/evl.h>

int evl_nrthreads;

LIST_HEAD(evl_thread_list);

static DEFINE_HARD_SPINLOCK(thread_list_lock);

static DECLARE_WAIT_QUEUE_HEAD(join_all);

static void inband_task_wakeup(struct irq_work *work);

static void skip_ptsync(struct evl_thread *thread);

static void timeout_handler(struct evl_timer *timer) /* oob stage stalled */
{
	struct evl_thread *thread = container_of(timer, struct evl_thread, rtimer);

	evl_wakeup_thread(thread, T_DELAY|T_PEND, T_TIMEO);
}

static void periodic_handler(struct evl_timer *timer) /* oob stage stalled */
{
	struct evl_thread *thread =
		container_of(timer, struct evl_thread, ptimer);

	evl_wakeup_thread(thread, T_WAIT, T_TIMEO);
}

static inline void enqueue_new_thread(struct evl_thread *thread)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&thread_list_lock, flags);
	list_add_tail(&thread->next, &evl_thread_list);
	evl_nrthreads++;
	raw_spin_unlock_irqrestore(&thread_list_lock, flags);
}

static inline void dequeue_old_thread(struct evl_thread *thread)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&thread_list_lock, flags);
	if (!list_empty(&thread->next)) {
		list_del(&thread->next);
		evl_nrthreads--;
	}
	raw_spin_unlock_irqrestore(&thread_list_lock, flags);
}

static inline void set_oob_threadinfo(struct evl_thread *thread)
{
	struct oob_thread_state *p;

	p = dovetail_current_state();
	p->thread = thread;
}

static inline void set_oob_mminfo(struct evl_thread *thread)
{
	thread->oob_mm = dovetail_mm_state();
}

static inline void add_u_cap(struct evl_thread *thread,
			struct cred *newcap,
			int cap)
{
	if (!capable(cap)) {
		cap_raise(newcap->cap_effective, cap);
		cap_raise(thread->raised_cap, cap);
	}
}

static inline void drop_u_cap(struct evl_thread *thread,
			struct cred *newcap,
			int cap)
{
	if (cap_raised(thread->raised_cap, cap)) {
		cap_lower(newcap->cap_effective, cap);
		cap_lower(thread->raised_cap, cap);
	}
}

static void pin_to_initial_cpu(struct evl_thread *thread)
{
	struct task_struct *p = current;
	unsigned long flags;
	struct evl_rq *rq;
	int cpu;

	/*
	 * @thread is the EVL extension of the current in-band
	 * task. If the current CPU is part of the affinity mask of
	 * this thread, pin the latter on this CPU. Otherwise pin it
	 * to the first CPU of that mask.
	 */
	cpu = task_cpu(p);
	if (!cpumask_test_cpu(cpu, &thread->affinity))
		cpu = cpumask_first(&thread->affinity);

	set_cpus_allowed_ptr(p, cpumask_of(cpu));
	/*
	 * @thread is still unstarted EVL-wise, we are in the process
	 * of mapping the current in-band task to it. Therefore
	 * evl_migrate_thread() can be called for pinning it on an
	 * out-of-band CPU.
	 */
	rq = evl_cpu_rq(cpu);
	evl_spin_lock_irqsave(&thread->lock, flags);
	evl_migrate_thread(thread, rq);
	evl_spin_unlock_irqrestore(&thread->lock, flags);
}

int evl_init_thread(struct evl_thread *thread,
		const struct evl_init_thread_attr *iattr,
		struct evl_rq *rq,
		const char *fmt, ...)
{
	int flags = iattr->flags & ~T_SUSP, ret, gravity;
	cpumask_var_t affinity;
	va_list args;

	inband_context_only();

	if (!(flags & T_ROOT))
		flags |= T_DORMANT | T_INBAND;

	if ((flags & T_USER) && IS_ENABLED(CONFIG_EVL_DEBUG_WOLI))
		flags |= T_WOLI;

	/*
	 * If no rq was given, pick an initial CPU for the new thread
	 * which is part of its affinity mask, and therefore also part
	 * of the supported CPUs. This CPU may change in
	 * pin_to_initial_cpu().
	 */
	if (!rq) {
		if (!alloc_cpumask_var(&affinity, GFP_KERNEL))
			return -ENOMEM;
		cpumask_and(affinity, iattr->affinity, &evl_cpu_affinity);
		if (!cpumask_empty(affinity))
			rq = evl_cpu_rq(cpumask_first(affinity));
		free_cpumask_var(affinity);
		if (!rq)
			return -EINVAL;
	}

	va_start(args, fmt);
	thread->name = kvasprintf(GFP_KERNEL, fmt, args);
	va_end(args);
	if (thread->name == NULL)
		return -ENOMEM;

	cpumask_and(&thread->affinity, iattr->affinity, &evl_cpu_affinity);
	thread->rq = rq;
	thread->state = flags;
	thread->info = 0;
	thread->local_info = 0;
	thread->wprio = EVL_IDLE_PRIO;
	thread->cprio = EVL_IDLE_PRIO;
	thread->bprio = EVL_IDLE_PRIO;
	thread->rrperiod = EVL_INFINITE;
	thread->wchan = NULL;
	thread->wwake = NULL;
	thread->wait_data = NULL;
	thread->u_window = NULL;
	atomic_set(&thread->inband_disable_count, 0);
	memset(&thread->poll_context, 0, sizeof(thread->poll_context));
	memset(&thread->stat, 0, sizeof(thread->stat));
	memset(&thread->altsched, 0, sizeof(thread->altsched));
	init_irq_work(&thread->inband_work, inband_task_wakeup);

	INIT_LIST_HEAD(&thread->next);
	INIT_LIST_HEAD(&thread->boosters);
	INIT_LIST_HEAD(&thread->trackers);
	raw_spin_lock_init(&thread->tracking_lock);
	evl_spin_lock_init(&thread->lock);
	init_completion(&thread->exited);
	INIT_LIST_HEAD(&thread->ptsync_next);
	thread->oob_mm = NULL;

	gravity = flags & T_USER ? EVL_TIMER_UGRAVITY : EVL_TIMER_KGRAVITY;
	evl_init_timer_on_rq(&thread->rtimer, &evl_mono_clock, timeout_handler,
			rq, gravity);
	evl_set_timer_name(&thread->rtimer, thread->name);
	evl_set_timer_priority(&thread->rtimer, EVL_TIMER_HIPRIO);
	evl_init_timer_on_rq(&thread->ptimer, &evl_mono_clock, periodic_handler,
			rq, gravity);
	evl_set_timer_name(&thread->ptimer, thread->name);
	evl_set_timer_priority(&thread->ptimer, EVL_TIMER_HIPRIO);

	thread->base_class = NULL; /* evl_set_thread_policy() sets it. */
	ret = evl_init_rq_thread(thread);
	if (ret)
		goto err_out;

	ret = evl_set_thread_policy(thread, iattr->sched_class,
				&iattr->sched_param);
	if (ret)
		goto err_out;

	trace_evl_init_thread(thread, iattr, ret);

	return 0;

err_out:
	evl_destroy_timer(&thread->rtimer);
	evl_destroy_timer(&thread->ptimer);
	trace_evl_init_thread(thread, iattr, ret);
	kfree(thread->name);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_init_thread);

/* Undoes evl_init_thread(), and only that. */
static void uninit_thread(struct evl_thread *thread)
{
	unsigned long flags;
	struct evl_rq *rq;

	evl_destroy_timer(&thread->rtimer);
	evl_destroy_timer(&thread->ptimer);

	rq = evl_get_thread_rq(thread, flags);
	evl_forget_thread(thread);
	evl_put_thread_rq(thread, rq, flags);

	kfree(thread->name);
}

static void do_cleanup_current(struct evl_thread *curr)
{
	struct cred *newcap;
	unsigned long flags;
	struct evl_rq *rq;

	/*
	 * Drop trackers first since this may alter the rq state for
	 * current.
	 */
	evl_drop_tracking_mutexes(curr);

	evl_unindex_element(&curr->element);

	if (curr->state & T_USER) {
		evl_free_chunk(&evl_shared_heap, curr->u_window);
		curr->u_window = NULL;
		evl_drop_poll_table(curr);
		newcap = prepare_creds();
		if (newcap) {
			drop_u_cap(curr, newcap, CAP_SYS_NICE);
			drop_u_cap(curr, newcap, CAP_IPC_LOCK);
			drop_u_cap(curr, newcap, CAP_SYS_RAWIO);
			commit_creds(newcap);
		}
	}

	dequeue_old_thread(curr);

	rq = evl_get_thread_rq(curr, flags);

	if (curr->state & T_READY) {
		EVL_WARN_ON(CORE, (curr->state & EVL_THREAD_BLOCK_BITS));
		evl_dequeue_thread(curr);
		curr->state &= ~T_READY;
	}

	curr->state |= T_ZOMBIE;

	evl_put_thread_rq(curr, rq, flags);
	uninit_thread(curr);
}

static void cleanup_current_thread(void)
{
	struct oob_thread_state *p = dovetail_current_state();
	struct evl_thread *curr = evl_current();

	/*
	 * We are called for exiting kernel and user threads over the
	 * in-band context.
	 */
	trace_evl_thread_unmap(curr);
	dovetail_stop_altsched();
	do_cleanup_current(curr);

	p->thread = NULL;	/* evl_current() <- NULL */
}

static void put_current_thread(void)
{
	struct evl_thread *curr = evl_current();

	if (curr->state & T_USER)
		skip_ptsync(curr);

	cleanup_current_thread();
	evl_put_element(&curr->element);
}

static void wakeup_kthread_parent(struct irq_work *irq_work)
{
	struct evl_kthread *kthread;
	kthread = container_of(irq_work, struct evl_kthread, irq_work);
	complete(&kthread->done);
}

static int map_kthread_self(struct evl_kthread *kthread)
{
	struct evl_thread *curr = &kthread->thread;

	pin_to_initial_cpu(curr);

	dovetail_init_altsched(&curr->altsched);
	set_oob_threadinfo(curr);
	dovetail_start_altsched();
	evl_release_thread(curr, T_DORMANT, 0);

	trace_evl_thread_map(curr);

	/*
	 * Upon -weird- error from evl_switch_oob() for an emerging
	 * kernel thread, we still finalize the registration but the
	 * caller should self-cancel eventually.
	 */
	kthread->status = evl_switch_oob();

	/*
	 * We are now running OOB, therefore __evl_run_kthread() can't
	 * start us before we enter the dormant state because
	 * irq_work_queue() schedules the in-band wakeup request on
	 * the current CPU. If we fail switching to OOB context,
	 * kthread->status tells __evl_run_kthread() not to start but
	 * cancel us instead.
	 */
	init_irq_work(&kthread->irq_work, wakeup_kthread_parent);
	irq_work_queue(&kthread->irq_work);

	enqueue_new_thread(curr);
	evl_hold_thread(curr, T_DORMANT);

	return kthread->status;
}

static int kthread_trampoline(void *arg)
{
	struct evl_kthread *kthread = arg;
	struct evl_thread *curr = &kthread->thread;
	struct sched_param param;
	int policy, prio, ret;

	/*
	 * It makes sense to schedule EVL kthreads either in the
	 * SCHED_FIFO or SCHED_NORMAL policy only. So anything that is
	 * not based on EVL's FIFO class is assumed to belong to the
	 * in-band SCHED_NORMAL class.
	 */
	if (curr->sched_class != &evl_sched_fifo) {
		policy = SCHED_NORMAL;
		prio = 0;
	} else {
		policy = SCHED_FIFO;
		prio = curr->cprio;
		/* Normalize priority linux-wise. */
		if (prio >= MAX_RT_PRIO)
			prio = MAX_RT_PRIO - 1;
	}

	param.sched_priority = prio;
	sched_setscheduler(current, policy, &param);

	ret = map_kthread_self(kthread);
	if (!ret) {
		trace_evl_kthread_entry(curr);
		kthread->threadfn(kthread);
	}

	/* Handles nitty-gritty details like in-band switch. */
	evl_cancel_thread(curr);

	return 0;
}

int __evl_run_kthread(struct evl_kthread *kthread)
{
	struct evl_thread *thread = &kthread->thread;
	struct task_struct *p;
	int ret;

	ret = evl_init_element(&thread->element, &evl_thread_factory);
	if (ret)
		goto fail_element;

	ret = evl_create_element_device(&thread->element,
					&evl_thread_factory,
					thread->name);
	if (ret)
		goto fail_device;

	p = kthread_run(kthread_trampoline, kthread, "%s", thread->name);
	if (IS_ERR(p)) {
		ret = PTR_ERR(p);
		goto fail_spawn;
	}

	evl_index_element(&thread->element);
	wait_for_completion(&kthread->done);
	if (kthread->status)
		return kthread->status;

	evl_release_thread(thread, T_DORMANT, 0);
	evl_schedule();

	return 0;

fail_spawn:
	evl_remove_element_device(&thread->element);
fail_device:
	evl_destroy_element(&thread->element);
fail_element:
	uninit_thread(thread);

	return ret;
}
EXPORT_SYMBOL_GPL(__evl_run_kthread);

/* evl_current()->lock + evl_current()->rq->lock held, oob stalled. */
void evl_sleep_on_locked(ktime_t timeout, enum evl_tmode timeout_mode,
		struct evl_clock *clock,
		struct evl_wait_channel *wchan)
{
	struct evl_thread *curr = evl_current();
	struct evl_rq *rq = curr->rq;
	unsigned long oldstate;

	assert_evl_lock(&curr->lock);
	assert_evl_lock(&rq->lock);

	trace_evl_sleep_on(timeout, timeout_mode, clock, wchan);

	oldstate = curr->state;

	/*
	 * If a request to switch to in-band context is pending
	 * (T_KICKED), raise T_BREAK then return immediately.
	 */
	if (likely(!(oldstate & EVL_THREAD_BLOCK_BITS))) {
		if (curr->info & T_KICKED) {
			curr->info &= ~(T_RMID|T_TIMEO);
			curr->info |= T_BREAK;
			return;
		}
		curr->info &= ~EVL_THREAD_INFO_MASK;
	}

	/*
	 *  wchan + timeout: timed wait for a resource (T_PEND|T_DELAY)
	 *  wchan + !timeout: unbounded sleep on resource (T_PEND)
	 * !wchan + timeout: timed sleep (T_DELAY)
	 * !wchan + !timeout: periodic wait (T_WAIT)
	 */
	if (timeout_mode != EVL_REL || !timeout_infinite(timeout)) {
		evl_prepare_timed_wait(&curr->rtimer, clock,
				evl_thread_rq(curr));
		if (timeout_mode == EVL_REL)
			timeout = evl_abs_timeout(&curr->rtimer, timeout);
		else if (timeout <= evl_read_clock(clock)) {
			curr->info |= T_TIMEO;
			return;
		}
		evl_start_timer(&curr->rtimer, timeout, EVL_INFINITE);
		curr->state |= T_DELAY;
	} else if (!wchan) {
		evl_prepare_timed_wait(&curr->ptimer, clock,
				evl_thread_rq(curr));
		curr->state |= T_WAIT;
	}

	if (oldstate & T_READY) {
		evl_dequeue_thread(curr);
		curr->state &= ~T_READY;
	}

	if (wchan) {
		curr->wchan = wchan;
		curr->state |= T_PEND;
	}

	evl_set_resched(rq);
}

void evl_sleep_on(ktime_t timeout, enum evl_tmode timeout_mode,
		struct evl_clock *clock,
		struct evl_wait_channel *wchan)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;
	struct evl_rq *rq;

	oob_context_only();

	rq = evl_get_thread_rq(curr, flags);
	evl_sleep_on_locked(timeout, timeout_mode, clock, wchan);
	evl_put_thread_rq(curr, rq, flags);
}

/* thread->lock + thread->rq->lock held, irqs off */
static void evl_wakeup_thread_locked(struct evl_thread *thread,
				int mask, int info)
{
	struct evl_rq *rq = thread->rq;
	unsigned long oldstate;

	assert_evl_lock(&thread->lock);
	assert_evl_lock(&thread->rq->lock);

	if (EVL_WARN_ON(CORE, mask & ~(T_DELAY|T_PEND|T_WAIT)))
		return;

	trace_evl_wakeup_thread(thread, mask, info);

	oldstate = thread->state;
	if (likely(oldstate & mask)) {
		/* Clear T_DELAY along w/ T_PEND in state. */
		if (mask & T_PEND)
			mask |= T_DELAY;

		thread->state &= ~mask;

		if (mask & (T_DELAY|T_PEND))
			evl_stop_timer(&thread->rtimer);

		if (mask & T_PEND & oldstate)
			thread->wchan = NULL;

		thread->info |= info;

		if (!(thread->state & EVL_THREAD_BLOCK_BITS)) {
			evl_enqueue_thread(thread);
			thread->state |= T_READY;
			evl_set_resched(rq);
			if (rq != this_evl_rq())
				evl_inc_counter(&thread->stat.rwa);
		}
	}
}

void evl_wakeup_thread(struct evl_thread *thread, int mask, int info)
{
	unsigned long flags;
	struct evl_rq *rq;

	rq = evl_get_thread_rq(thread, flags);
	evl_wakeup_thread_locked(thread, mask, info);
	evl_put_thread_rq(thread, rq, flags);
}

void evl_hold_thread(struct evl_thread *thread, int mask)
{
	unsigned long oldstate, flags;
	struct evl_rq *rq;

	if (EVL_WARN_ON(CORE, mask & ~(T_SUSP|T_HALT|T_DORMANT)))
		return;

	trace_evl_hold_thread(thread, mask);

	rq = evl_get_thread_rq(thread, flags);

	oldstate = thread->state;

	/*
	 * If a request to switch to in-band context is pending for
	 * the target thread (T_KICKED), raise T_BREAK for it then
	 * return immediately.
	 */
	if (likely(!(oldstate & EVL_THREAD_BLOCK_BITS))) {
		if (thread->info & T_KICKED) {
			thread->info &= ~(T_RMID|T_TIMEO);
			thread->info |= T_BREAK;
			goto out;
		}
		if (thread == rq->curr)
			thread->info &= ~EVL_THREAD_INFO_MASK;
	}

	if (oldstate & T_READY) {
		evl_dequeue_thread(thread);
		thread->state &= ~T_READY;
	}

	thread->state |= mask;

	/*
	 * If the thread is current on its CPU, we need to raise
	 * RQ_SCHED on the target runqueue.
	 *
	 * If the target thread runs in-band in userland on a remote
	 * CPU, request it to call us back next time it transitions
	 * from kernel to user mode.
	 */
	if (likely(thread == rq->curr))
		evl_set_resched(rq);
	else if (((oldstate & (EVL_THREAD_BLOCK_BITS|T_USER)) == (T_INBAND|T_USER)))
		dovetail_request_ucall(thread->altsched.task);
 out:
	evl_put_thread_rq(thread, rq, flags);
}

/* thread->lock + thread->rq->lock held, irqs off */
static void evl_release_thread_locked(struct evl_thread *thread,
				int mask, int info)
{
	struct evl_rq *rq = thread->rq;
	unsigned long oldstate;

	assert_evl_lock(&thread->lock);
	assert_evl_lock(&thread->rq->lock);

	if (EVL_WARN_ON(CORE, mask & ~(T_SUSP|T_HALT|T_INBAND|T_DORMANT|T_PTSYNC)))
		return;

	trace_evl_release_thread(thread, mask, info);

	oldstate = thread->state;
	if (oldstate & mask) {
		thread->state &= ~mask;
		thread->info |= info;

		if (thread->state & EVL_THREAD_BLOCK_BITS)
			return;

		if (unlikely((oldstate & mask) & (T_HALT|T_PTSYNC))) {
			/* Requeue at head of priority group. */
			evl_requeue_thread(thread);
			goto ready;
		}
	} else if (oldstate & T_READY)
		/* Ends up in round-robin (group rotation). */
		evl_dequeue_thread(thread);

	/* Enqueue at the tail of priority group. */
	evl_enqueue_thread(thread);
ready:
	thread->state |= T_READY;
	evl_set_resched(rq);
	if (rq != this_evl_rq())
		evl_inc_counter(&thread->stat.rwa);
}

void evl_release_thread(struct evl_thread *thread, int mask, int info)
{
	unsigned long flags;
	struct evl_rq *rq;

	rq = evl_get_thread_rq(thread, flags);
	evl_release_thread_locked(thread, mask, info);
	evl_put_thread_rq(thread, rq, flags);
}

static void inband_task_wakeup(struct irq_work *work)
{
	struct evl_thread *thread;
	struct task_struct *p;

	thread = container_of(work, struct evl_thread, inband_work);
	p = thread->altsched.task;
	trace_evl_inband_wakeup(p);
	wake_up_process(p);
}

void evl_set_kthread_priority(struct evl_kthread *kthread, int priority)
{
	union evl_sched_param param = { .fifo = { .prio = priority } };

	evl_set_thread_schedparam(&kthread->thread, &evl_sched_fifo, &param);
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_set_kthread_priority);

ktime_t evl_get_thread_timeout(struct evl_thread *thread)
{
	struct evl_timer *timer;
	ktime_t timeout, now;

	if (!(thread->state & T_DELAY))
		return 0LL;

	if (evl_timer_is_running(&thread->rtimer))
		timer = &thread->rtimer;
	else if (evl_timer_is_running(&thread->ptimer))
		timer = &thread->ptimer;
	else
		return 0;

	now = evl_ktime_monotonic();
	timeout = evl_get_timer_date(timer);
	if (timeout <= now)
		return ktime_set(0, 1);

	return ktime_sub(timeout, now);
}
EXPORT_SYMBOL_GPL(evl_get_thread_timeout);

ktime_t evl_get_thread_period(struct evl_thread *thread)
{
	ktime_t period = 0;
	/*
	 * The current thread period might be:
	 * - the value of the timer interval for periodic threads (ns/ticks)
	 * - or, the value of the alloted round-robin quantum (ticks)
	 * - or zero, meaning "no periodic activity".
	 */
	if (evl_timer_is_running(&thread->ptimer))
		period = thread->ptimer.interval;
	else if (thread->state & T_RRB)
		period = thread->rrperiod;

	return period;
}
EXPORT_SYMBOL_GPL(evl_get_thread_period);

ktime_t evl_delay_thread(ktime_t timeout, enum evl_tmode timeout_mode,
			struct evl_clock *clock)
{
	struct evl_thread *curr = evl_current();
	ktime_t rem = 0;

	evl_sleep_on(timeout, timeout_mode, clock, NULL);
	evl_schedule();

	if (curr->info & T_BREAK)
		rem = __evl_get_stopped_timer_delta(&curr->rtimer);

	return rem;
}
EXPORT_SYMBOL_GPL(evl_delay_thread);

int evl_sleep_until(ktime_t timeout)
{
	ktime_t rem;

	if (!EVL_ASSERT(CORE, !evl_cannot_block()))
		return -EPERM;

	rem = evl_delay_thread(timeout, EVL_ABS, &evl_mono_clock);

	return rem ? -EINTR : 0;
}
EXPORT_SYMBOL_GPL(evl_sleep_until);

int evl_sleep(ktime_t delay)
{
	ktime_t end = ktime_add(evl_read_clock(&evl_mono_clock), delay);
	return evl_sleep_until(end);
}
EXPORT_SYMBOL_GPL(evl_sleep);

int evl_set_thread_period(struct evl_clock *clock,
			ktime_t idate, ktime_t period)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;
	int ret = 0;

	if (curr == NULL)
		return -EPERM;

	if (clock == NULL || period == EVL_INFINITE) {
		evl_stop_timer(&curr->ptimer);
		return 0;
	}

	/*
	 * LART: detect periods which are shorter than the target
	 * clock gravity for kernel thread timers. This can't work,
	 * caller must have messed up arguments.
	 */
	if (period < evl_get_clock_gravity(clock, kernel))
		return -EINVAL;

	evl_spin_lock_irqsave(&curr->lock, flags);

	evl_prepare_timed_wait(&curr->ptimer, clock, evl_thread_rq(curr));

	if (timeout_infinite(idate))
		idate = evl_abs_timeout(&curr->ptimer, period);

	evl_start_timer(&curr->ptimer, idate, period);

	evl_spin_unlock_irqrestore(&curr->lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_set_thread_period);

int evl_wait_thread_period(unsigned long *overruns_r)
{
	unsigned long overruns, flags;
	struct evl_thread *curr;
	struct evl_clock *clock;
	ktime_t now;

	curr = evl_current();
	if (unlikely(!evl_timer_is_running(&curr->ptimer)))
		return -EAGAIN;

	trace_evl_thread_wait_period(curr);

	flags = hard_local_irq_save();
	clock = curr->ptimer.clock;
	now = evl_read_clock(clock);
	if (likely(now < evl_get_timer_next_date(&curr->ptimer))) {
		evl_sleep_on(EVL_INFINITE, EVL_REL, clock, NULL); /* T_WAIT */
		hard_local_irq_restore(flags);
		evl_schedule();
		if (unlikely(curr->info & T_BREAK))
			return -EINTR;
	} else
		hard_local_irq_restore(flags);

	overruns = evl_get_timer_overruns(&curr->ptimer);
	if (overruns) {
		if (likely(overruns_r != NULL))
			*overruns_r = overruns;
		trace_evl_thread_missed_period(curr);
		return -ETIMEDOUT;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(evl_wait_thread_period);

void evl_cancel_thread(struct evl_thread *thread)
{
	unsigned long flags;
	struct evl_rq *rq;

	if (EVL_WARN_ON(CORE, thread->state & T_ROOT))
		return;

	rq = evl_get_thread_rq(thread, flags);

	if (thread->state & T_ZOMBIE) {
		evl_put_thread_rq(thread, rq, flags);
		return;
	}

	if (thread->info & T_CANCELD)
		goto check_self_cancel;

	trace_evl_thread_cancel(thread);

	thread->info |= T_CANCELD;

	/*
	 * If @thread is not started yet, fake a start request,
	 * raising the kicked condition bit to make sure it reaches
	 * evl_test_cancel() on its wakeup path.
	 *
	 * NOTE: if T_DORMANT and !T_INBAND, then some not-yet-mapped
	 * emerging thread is self-cancelling due to an early error in
	 * the prep work.
	 */
	if ((thread->state & (T_DORMANT|T_INBAND)) == (T_DORMANT|T_INBAND)) {
		evl_release_thread_locked(thread, T_DORMANT, T_KICKED);
		evl_put_thread_rq(thread, rq, flags);
		goto out;
	}

check_self_cancel:
	evl_put_thread_rq(thread, rq, flags);

	if (evl_current() == thread) {
		evl_test_cancel();
		/*
		 * May return if on behalf of some IRQ handler which
		 * interrupted @thread.
		 */
		return;
	}

	/*
	 * Force the non-current thread to exit:
	 *
	 * - unblock a user thread, switch it to weak scheduling,
	 * then send it SIGTERM.
	 *
	 * - just unblock a kernel thread, it is expected to reach a
	 * cancellation point soon after (i.e. evl_test_cancel()).
	 */
	if (thread->state & T_USER) {
		evl_demote_thread(thread);
		evl_signal_thread(thread, SIGTERM, 0);
	} else
		evl_kick_thread(thread, 0);
out:
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_cancel_thread);

int evl_detach_self(void)
{
	if (evl_current() == NULL)
		return -EPERM;

	put_current_thread();

	return 0;
}

int evl_join_thread(struct evl_thread *thread, bool uninterruptible)
{
	struct evl_thread *curr = evl_current();
	bool switched = false;
	unsigned long flags;
	struct evl_rq *rq;
	int ret = 0;

	if (EVL_WARN_ON(CORE, thread->state & T_ROOT))
		return -EINVAL;

	if (thread == curr)
		return -EDEADLK;

	rq = evl_get_thread_rq(thread, flags);

	/*
	 * We allow multiple callers to join @thread, this is purely a
	 * synchronization mechanism with no resource collection.
	 */
	if (thread->info & T_DORMANT) {
		evl_put_thread_rq(thread, rq, flags);
		return 0;
	}

	trace_evl_thread_join(thread);

	if (curr && !(curr->state & T_INBAND)) {
		evl_put_thread_rq(thread, rq, flags);
		evl_switch_inband(SIGDEBUG_NONE);
		switched = true;
	} else {
		evl_put_thread_rq(thread, rq, flags);
	}

	/*
	 * Wait until the joinee is fully dismantled in
	 * thread_factory_dispose(), which guarantees safe module
	 * removal afterwards if applicable. After this point, @thread
	 * is invalid.
	 */
	if (uninterruptible)
		wait_for_completion(&thread->exited);
	else {
		ret = wait_for_completion_interruptible(&thread->exited);
		if (ret < 0)
			return -EINTR;
	}

	if (switched)
		ret = evl_switch_oob();

	return ret;
}
EXPORT_SYMBOL_GPL(evl_join_thread);

int evl_set_thread_schedparam(struct evl_thread *thread,
			struct evl_sched_class *sched_class,
			const union evl_sched_param *sched_param)
{
	unsigned long flags;
	struct evl_rq *rq;
	int ret;

	rq = evl_get_thread_rq(thread, flags);
	ret = evl_set_thread_schedparam_locked(thread, sched_class, sched_param);
	evl_put_thread_rq(thread, rq, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_set_thread_schedparam);

int evl_set_thread_schedparam_locked(struct evl_thread *thread,
				     struct evl_sched_class *sched_class,
				     const union evl_sched_param *sched_param)
{
	int old_wprio, new_wprio, ret;

	assert_thread_pinned(thread);

	old_wprio = thread->wprio;

	ret = evl_set_thread_policy_locked(thread, sched_class, sched_param);
	if (ret)
		return ret;

	new_wprio = thread->wprio;

	/*
	 * If the thread is sleeping on a wait channel, update its
	 * position in the corresponding wait list, unless the
	 * (weighted) priority has not changed (to prevent spurious
	 * round-robin effects).
	 */
	if (old_wprio != new_wprio && (thread->state & T_PEND))
		thread->wchan->reorder_wait(thread, thread);

	thread->info |= T_SCHEDP;
	/* Ask the target thread to call back if in-band. */
	if ((thread->state & (T_INBAND|T_USER)) == (T_INBAND|T_USER))
		dovetail_request_ucall(thread->altsched.task);

	return ret;
}

void __evl_test_cancel(struct evl_thread *curr)
{
	/*
	 * Just in case evl_test_cancel() is called from an IRQ
	 * handler, in which case we may not take the exit path.
	 *
	 * NOTE: curr->rq is stable from our POV and can't change
	 * under our feet.
	 */
	if (curr->rq->local_flags & RQ_IRQ)
		return;

	if (!(curr->state & T_INBAND))
		evl_switch_inband(SIGDEBUG_NONE);

	do_exit(0);
	/* ... won't return ... */
	EVL_WARN_ON(CORE, 1);
}
EXPORT_SYMBOL_GPL(__evl_test_cancel);

void __evl_propagate_schedparam_change(struct evl_thread *curr)
{
	int kpolicy = SCHED_FIFO, kprio, ret;
	struct task_struct *p = current;
	struct sched_param param;
	unsigned long flags;
	struct evl_rq *rq;

	/*
	 * Test-set race for T_SCHEDP is ok, the propagation is meant
	 * to be done asap but not guaranteed to be carried out
	 * immediately, and the request will remain pending until it
	 * is eventually handled. We just have to protect against a
	 * set-clear race.
	 */
	rq = evl_get_thread_rq(curr, flags);
	kprio = curr->bprio;
	curr->info &= ~T_SCHEDP;

	/*
	 * Map our policies/priorities to the regular kernel's
	 * (approximated).
	 */
	if ((curr->state & T_WEAK) && kprio == 0)
		kpolicy = SCHED_NORMAL;
	else if (kprio > EVL_FIFO_MAX_PRIO)
		kprio = EVL_FIFO_MAX_PRIO;

	evl_put_thread_rq(curr, rq, flags);

	if (p->policy != kpolicy || (kprio > 0 && p->rt_priority != kprio)) {
		param.sched_priority = kprio;
		ret = sched_setscheduler_nocheck(p, kpolicy, &param);
		EVL_WARN_ON(CORE, ret != 0);
	}
}

void evl_unblock_thread(struct evl_thread *thread, int reason)
{
	trace_evl_unblock_thread(thread);

	/*
	 * We must not raise the T_BREAK bit if the target thread was
	 * already runnable at the time of this call, so that
	 * downstream code does not get confused by some "successful
	 * but interrupted syscall" condition. IOW, a break state
	 * raised here must always trigger an error code downstream,
	 * and a wait which went to completion should not be marked as
	 * interrupted.
	 *
	 * evl_wakeup_thread() guarantees this by updating the info
	 * bits only if any of the mask bits is set.
	 */
	evl_wakeup_thread(thread, T_DELAY|T_PEND|T_WAIT, reason|T_BREAK);
}
EXPORT_SYMBOL_GPL(evl_unblock_thread);

void evl_kick_thread(struct evl_thread *thread, int info)
{
	struct task_struct *p = thread->altsched.task;
	unsigned long flags;
	struct evl_rq *rq;

	rq = evl_get_thread_rq(thread, flags);

	if (thread->state & T_INBAND)
		goto out;

	/*
	 * We might get T_PTSIG on top of T_KICKED, never filter out
	 * the former.
	 */
	if (!(info & T_PTSIG) && thread->info & T_KICKED)
		goto out;

	/* See comment in evl_unblock_thread(). */
	evl_wakeup_thread_locked(thread, T_DELAY|T_PEND|T_WAIT,
				T_KICKED|T_BREAK);

	/*
	 * If @thread receives multiple ptrace-stop requests, ensure
	 * that disabling T_PTJOIN has precedence over enabling for
	 * the whole set.
	 */
	if (thread->info & T_PTSTOP) {
		if (thread->info & T_PTJOIN)
			thread->info &= ~T_PTJOIN;
		else
			info &= ~T_PTJOIN;
	}

	/*
	 * CAUTION: we must NOT raise T_BREAK when clearing a forcible
	 * block state, such as T_SUSP, T_HALT. The caller of
	 * evl_sleep_on() we unblock shall proceed as for a normal
	 * return, until it traverses a cancellation point if
	 * T_CANCELD was raised earlier, or calls evl_sleep_on() again
	 * which will detect T_KICKED and act accordingly.
	 *
	 * Rationale: callers of evl_sleep_on() may assume that
	 * receiving T_BREAK implicitly means that the awaited event
	 * was NOT received in the meantime. Therefore, in case only
	 * T_SUSP remains set for the thread on entry to
	 * evl_kick_thread(), after T_PEND was lifted earlier when the
	 * wait went to successful completion (i.e. no timeout), then
	 * we want the kicked thread to know that it did receive the
	 * requested resource, not finding T_BREAK in its state word.
	 *
	 * Callers of evl_sleep_on() may inquire for T_KICKED locally
	 * to detect forcible unblocks from T_SUSP, T_HALT, if they
	 * should act upon this case specifically.
	 *
	 * If @thread was frozen by an ongoing ptrace sync sequence
	 * (T_PTSYNC), release it so that it can reach the next
	 * in-band switch point (either from the EVL syscall return
	 * path, or from the mayday trap).
	 */
	evl_release_thread_locked(thread, T_SUSP|T_HALT|T_PTSYNC, T_KICKED);

	/*
	 * We may send mayday signals to userland threads only.
	 * However, no need to run a mayday trap if the current thread
	 * kicks itself out of OOB context: it will switch to in-band
	 * context on its way back to userland via the current syscall
	 * epilogue. Otherwise, we want that thread to enter the
	 * mayday trap asap.
	 */
	if ((thread->state & T_USER) && thread != this_evl_rq_thread())
		dovetail_send_mayday(p);

	/*
	 * Tricky cases:
	 *
	 * - a thread which was ready on entry wasn't actually
	 * running, but nevertheless waits for the CPU in OOB context,
	 * so we have to make sure that it will be notified of the
	 * pending break condition as soon as it enters a blocking EVL
	 * call.
	 *
	 * - a ready/readied thread on exit may be prevented from
	 * running by the scheduling policy module it belongs
 	 * to. Typically, policies enforcing a runtime budget do not
	 * block threads with no budget, but rather keep them out of
	 * their run queue, so that ->sched_pick() won't elect
	 * them. We tell the policy handler about the fact that we do
	 * want such thread to run until it switches to in-band
	 * context, whatever this entails internally for the
	 * implementation.
	 *
	 * - if the thread is running on the CPU, raising T_KICKED is
	 * enough to force a switch to in-band context on the next
	 * return to user.
	 */
	thread->info |= T_KICKED;

	if (thread->state & T_READY) {
		evl_force_thread(thread);
		evl_set_resched(thread->rq);
	}

	if (info)
		thread->info |= info;
out:
	evl_put_thread_rq(thread, rq, flags);
}
EXPORT_SYMBOL_GPL(evl_kick_thread);

void evl_demote_thread(struct evl_thread *thread)
{
	struct evl_sched_class *sched_class;
	union evl_sched_param param;
	unsigned long flags;
	struct evl_rq *rq;

	rq = evl_get_thread_rq(thread, flags);

	/*
	 * First demote @thread to the weak class, which still has
	 * access to EVL resources, but won't compete for real-time
	 * scheduling anymore. This will prevent @thread from keeping
	 * the CPU busy in out-of-band context once kicked out from
	 * wait.
	 */
	param.weak.prio = 0;
	sched_class = &evl_sched_weak;
	evl_set_thread_schedparam_locked(thread, sched_class, &param);

	evl_put_thread_rq(thread, rq, flags);

	/* Then unblock it from any wait state. */
	evl_kick_thread(thread, 0);
}
EXPORT_SYMBOL_GPL(evl_demote_thread);

struct inband_signal {
	fundle_t fundle;
	int signo, sigval;
	struct irq_work work;
};

static void inband_task_signal(struct irq_work *work)
{
	struct evl_thread *thread;
	struct inband_signal *req;
	struct kernel_siginfo si;
	struct task_struct *p;
	int signo;

	req = container_of(work, struct inband_signal, work);
	thread = evl_get_element_by_fundle(&evl_thread_factory,
					req->fundle, struct evl_thread);
	if (thread == NULL)
		goto done;

	p = thread->altsched.task;
	signo = req->signo;
	trace_evl_inband_signal(p, signo);

	if (signo == SIGDEBUG) {
		memset(&si, '\0', sizeof(si));
		si.si_signo = signo;
		si.si_code = SI_QUEUE;
		si.si_int = req->sigval;
		send_sig_info(signo, &si, p);
	} else
		send_sig(signo, p, 1);

	evl_put_element(&thread->element);
done:
	evl_free(req);
}

void evl_signal_thread(struct evl_thread *thread, int sig, int arg)
{
	struct inband_signal *req;

	if (EVL_WARN_ON(CORE, !(thread->state & T_USER)))
		return;

	req = evl_alloc(sizeof(*req));
	init_irq_work(&req->work, inband_task_signal);
	req->fundle = fundle_of(thread);
	req->signo = sig;
	req->sigval = sig == SIGDEBUG ? arg | sigdebug_marker : arg;
	irq_work_queue(&req->work);
}
EXPORT_SYMBOL_GPL(evl_signal_thread);

#ifdef CONFIG_MMU

static inline int commit_process_memory(void)
{
	struct task_struct *p = current;

	if (!(p->mm->def_flags & VM_LOCKED))
		return -EINVAL;

	return force_commit_memory();
}

#else /* !CONFIG_MMU */

static inline int commit_process_memory(void)
{
	return 0;
}

#endif /* !CONFIG_MMU */

int evl_killall(int mask)
{
	int nrkilled = 0, nrthreads, count;
	struct evl_thread *t, *n;
	LIST_HEAD(kill_list);
	unsigned long flags;
	long ret;

	inband_context_only();

	if (evl_current())
		return -EPERM;

	raw_spin_lock_irqsave(&thread_list_lock, flags);

	nrthreads = evl_nrthreads;

	for_each_evl_thread(t) {
		if ((t->state & T_ROOT) || (t->state & mask) != mask)
			continue;

		if (EVL_DEBUG(CORE))
			printk(EVL_INFO "terminating %s[%d]\n",
				t->name, evl_get_inband_pid(t));

		evl_get_element(&t->element);
		list_add(&t->kill_next, &kill_list);
	}

	raw_spin_unlock_irqrestore(&thread_list_lock, flags);

	list_for_each_entry_safe(t, n, &kill_list, kill_next) {
		list_del(&t->kill_next);
		nrkilled++;
		evl_cancel_thread(t);
		evl_put_element(&t->element);
	}

	count = nrthreads - nrkilled;
	if (EVL_DEBUG(CORE))
		printk(EVL_INFO "waiting for %d threads to exit\n",
			nrkilled);

	ret = wait_event_interruptible(join_all,
				evl_nrthreads == count);

	if (EVL_DEBUG(CORE))
		printk(EVL_INFO "joined %d threads\n",
			count + nrkilled - evl_nrthreads);

	return ret < 0 ? -EINTR : 0;
}
EXPORT_SYMBOL_GPL(evl_killall);

int evl_update_mode(__u32 mask, bool set)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;
	struct evl_rq *rq;

	if (curr == NULL)
		return -EPERM;

	if (mask & ~(T_WOSS|T_WOLI|T_WOSX))
		return -EINVAL;

	trace_evl_thread_update_mode(mask, set);

	rq = evl_get_thread_rq(curr, flags);

	if (set)
		curr->state |= mask;
	else
		curr->state &= ~mask;

	evl_put_thread_rq(curr, rq, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(evl_update_mode);

pid_t evl_get_inband_pid(struct evl_thread *thread)
{
	if (thread->state & (T_ROOT|T_DORMANT|T_ZOMBIE))
		return 0;

	if (thread->altsched.task == NULL)
		return -1;	/* weird */

	return task_pid_nr(thread->altsched.task);
}

int activate_oob_mm_state(struct oob_mm_state *p)
{
	/*
	 * A bit silly but we need a dynamic allocation for the EVL
	 * wait queue only to work around some inclusion hell when
	 * defining EVL's version of struct oob_mm_state.
	 */
	p->ptsync_barrier = kmalloc(sizeof(*p->ptsync_barrier), GFP_KERNEL);
	if (p->ptsync_barrier == NULL)
		return -ENOMEM;

	evl_init_wait(p->ptsync_barrier, &evl_mono_clock, EVL_WAIT_PRIO);
	INIT_LIST_HEAD(&p->ptrace_sync);
	smp_mb__before_atomic();
	set_bit(EVL_MM_ACTIVE_BIT, &p->flags);

	return 0;
}

static void flush_oob_mm_state(struct oob_mm_state *p)
{
	/*
	 * We are called for every mm dropped. Since every oob state
	 * is zeroed before use by the in-band kernel, processes with
	 * no active out-of-band state will escape this cleanup work
	 * on test_and_clear_bit().
	 */
	if (test_and_clear_bit(EVL_MM_ACTIVE_BIT, &p->flags)) {
		EVL_WARN_ON(CORE, !list_empty(&p->ptrace_sync));
		evl_destroy_wait(p->ptsync_barrier);
		kfree(p->ptsync_barrier);
	}
}

void arch_inband_task_init(struct task_struct *tsk)
{
	struct oob_thread_state *p = dovetail_task_state(tsk);

	evl_init_thread_state(p);
}

static inline void note_trap(struct evl_thread *curr,
		unsigned int trapnr, struct pt_regs *regs,
		const char *msg)
{
	if (user_mode(regs))
		printk(EVL_WARNING
			"%s %s [pid=%d, excpt=%d, user_pc=%#lx]\n",
			curr->name, msg,
			evl_get_inband_pid(curr),
			trapnr,
			instruction_pointer(regs));
	else
		printk(EVL_WARNING
			"%s %s [pid=%d, excpt=%d, %pS]\n",
			curr->name, msg,
			evl_get_inband_pid(curr),
			trapnr,
			(void *)instruction_pointer(regs));
}

/* oob stalled. */
void handle_oob_trap_entry(unsigned int trapnr, struct pt_regs *regs)
{
	struct evl_thread *curr;
	bool is_bp = false;

	curr = evl_current();
	if (curr->local_info & T_INFAULT) {
		note_trap(curr, trapnr, regs, "recursive fault");
		return;
	}

	oob_context_only();

	curr->local_info |= T_INFAULT;

	trace_evl_thread_fault(trapnr, regs);

	if (current->ptrace & PT_PTRACED)
		is_bp = evl_is_breakpoint(trapnr);

	if ((EVL_DEBUG(CORE) || (curr->state & T_WOSS)) && !is_bp)
		note_trap(curr, trapnr, regs, "switching in-band");

	/*
	 * We received a trap on the oob stage, switch to in-band
	 * before handling the exception.
	 */
	evl_switch_inband(is_bp ? SIGDEBUG_TRAP : SIGDEBUG_MIGRATE_FAULT);
}

/* hard irqs on. */
void handle_oob_trap_exit(unsigned int trapnr, struct pt_regs *regs)
{
	struct evl_thread *curr = evl_current();

	curr->local_info &= ~T_INFAULT;

	/*
	 * Switch back to the oob stage only after recovering from a
	 * trap in kernel space, which ensures a consistent execution
	 * state, e.g. if the current task is holding an EVL mutex or
	 * stax. If the trap occurred in user space, we can leave it
	 * to the common lazy stage switching strategy since the
	 * syscall barrier is there to reinstate the proper stage if
	 * need be.
	 */
	if (!user_mode(regs) && !evl_is_breakpoint(trapnr)) {
		evl_switch_oob();
		note_trap(curr, trapnr, regs, "resuming out-of-band");
	}
}

void handle_oob_mayday(struct pt_regs *regs)
{
	struct evl_thread *curr = evl_current();

	if (EVL_WARN_ON(CORE, !(curr->state & T_USER)))
		return;

	/*
	 * It might happen that a thread gets a mayday trap right
	 * after it switched to in-band mode while returning from a
	 * syscall. Filter this case out.
	 */
	if (!(curr->state & T_INBAND))
		evl_switch_inband(SIGDEBUG_NONE);
}

static void handle_migration_event(struct dovetail_migration_data *d)
{
#ifdef CONFIG_SMP
	struct task_struct *p = d->task;
	struct evl_thread *thread;

	thread = evl_thread_from_task(p);
	if (thread == NULL)
		return;

	/*
	 * Detect an EVL thread sleeping in OOB context which is
	 * required to migrate to another CPU by the in-band kernel.
	 *
	 * We may NOT fix up thread->sched immediately using the
	 * migration call, because the latter always has to take place
	 * on behalf of the target thread itself while running
	 * in-band. Therefore, that thread needs to switch to in-band
	 * context first, so that check_cpu_affinity() may do the
	 * fixup at the next transition to OOB. We expedite such
	 * transition for user threads by requesting them to call back
	 * asap via the RETUSER event.
	 */
	if (thread->state & (EVL_THREAD_BLOCK_BITS & ~T_INBAND)) {
		evl_kick_thread(thread, 0);
		evl_schedule();
		if (thread->state & T_USER)
			dovetail_request_ucall(thread->altsched.task);
	}
#endif
}

static void handle_sigwake_event(struct task_struct *p)
{
	struct evl_thread *thread;
	sigset_t sigpending;
	bool ptsync = false;
	int info = 0;

	thread = evl_thread_from_task(p);
	if (thread == NULL)
		return;

	if (thread->state & T_USER && p->ptrace & PT_PTRACED) {
		/* We already own p->sighand->siglock. */
		sigorsets(&sigpending,
			&p->pending.signal,
			&p->signal->shared_pending.signal);

		if (sigismember(&sigpending, SIGINT) ||
			sigismember(&sigpending, SIGTRAP)) {
			info = T_PTSIG|T_PTSTOP;
			ptsync = true;
		}
		/*
		 * CAUTION: we want T_JOIN to appear whenever SIGSTOP
		 * is present, regardless of other signals which might
		 * be pending.
		 */
		if (sigismember(&sigpending, SIGSTOP))
			info |= T_PTSIG|T_PTSTOP|T_PTJOIN;
	}

	/*
	 * A thread running on the oob stage may not be picked by the
	 * in-band scheduler as it bears the _TLF_OFFSTAGE flag. We
	 * need to force that thread to switch to in-band context,
	 * which will clear that flag. If we got there due to a ptrace
	 * signal, then setting T_PTSTOP ensures that @thread will be
	 * released from T_PTSYNC and will not receive any WOSS alert
	 * next time it switches in-band.
	 */
	evl_kick_thread(thread, info);

	/*
	 * Start a ptrace sync sequence if @thread is the initial stop
	 * target and runs oob. It is important to do this asap, so
	 * that sibling threads from the same process which also run
	 * oob cannot delay the in-band ptrace chores on this CPU,
	 * moving too far away from the stop point.
	 */
	if (ptsync)
		evl_start_ptsync(thread);

	evl_schedule();
}

/* curr locked, curr->rq locked. */
static void join_ptsync(struct evl_thread *curr)
{
	struct oob_mm_state *oob_mm = curr->oob_mm;

	evl_spin_lock(&oob_mm->ptsync_barrier->lock);

	/* In non-stop mode, no ptsync sequence is started. */
	if (test_bit(EVL_MM_PTSYNC_BIT, &oob_mm->flags) &&
		list_empty(&curr->ptsync_next))
		list_add_tail(&curr->ptsync_next, &oob_mm->ptrace_sync);

	evl_spin_unlock(&oob_mm->ptsync_barrier->lock);
}

static int leave_ptsync(struct evl_thread *leaver)
{
	struct oob_mm_state *oob_mm = leaver->oob_mm;
	unsigned long flags;
	int ret = 0;

	evl_spin_lock_irqsave(&oob_mm->ptsync_barrier->lock, flags);

	if (!test_bit(EVL_MM_PTSYNC_BIT, &oob_mm->flags))
		goto out;

	ret = -1;
	if (!list_empty(&leaver->ptsync_next))
		list_del_init(&leaver->ptsync_next);

	if (list_empty(&oob_mm->ptrace_sync)) {
		clear_bit(EVL_MM_PTSYNC_BIT, &oob_mm->flags);
		ret = 1;
	}
out:
	evl_spin_unlock_irqrestore(&oob_mm->ptsync_barrier->lock, flags);

	return ret;
}

static void skip_ptsync(struct evl_thread *thread)
{
	struct oob_mm_state *oob_mm = thread->oob_mm;

	if (test_bit(EVL_MM_ACTIVE_BIT, &oob_mm->flags) &&
		leave_ptsync(thread) > 0) {
		evl_flush_wait(oob_mm->ptsync_barrier, 0);
		evl_schedule();
	}
}

static void handle_ptstop_event(void)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;
	struct evl_rq *rq;

	/*
	 * T_PTRACE denotes a stopped state as defined by ptrace()
	 * which means blocked in ptrace_stop(). Our T_PTSTOP bit has
	 * a broader scope which starts from the in-band request to
	 * stop (handle_sigwake_event()), then ends after the tracee
	 * switched back to oob context via RETUSER handler.
	 */
	rq = evl_get_thread_rq(curr, flags);

	curr->state |= T_PTRACE;

	/*
	 * If we were running out-of-band when SIGSTOP reached us, we
	 * have to join the ptsync queue.
	 */
	if (curr->info & T_PTJOIN) {
		join_ptsync(curr);
		curr->info &= ~T_PTJOIN;
	}

	evl_put_thread_rq(curr, rq, flags);
}

static void handle_ptstep_event(struct task_struct *task)
{
	struct evl_thread *tracee = evl_thread_from_task(task);

	/*
	 * The ptracer might have switched focus, (single-)stepping a
	 * thread which did not hit the latest breakpoint
	 * (i.e. bearing T_PTJOIN). For this reason, we do need to
	 * listen to PTSTEP events to remove that thread from the
	 * ptsync queue.
	 */
	skip_ptsync(tracee);
}

static void handle_ptcont_event(void)
{
	struct evl_thread *curr = evl_current();

	if (curr->state & T_PTRACE) {
		/*
		 * Since we stopped executing due to ptracing, any
		 * ongoing periodic timeline is now lost: disable
		 * overrun detection for the next round.
		 */
		curr->local_info |= T_IGNOVR;

		/*
		 * Request to receive INBAND_TASK_RETUSER on the
		 * return path to user mode so that we can switch back
		 * to out-of-band mode for synchronizing on the ptsync
		 * barrier.
		 */
		dovetail_request_ucall(current);
	}
}

/* oob stage, hard irqs on. */
static int ptrace_sync(void)
{
	struct evl_thread *curr = evl_current();
	struct oob_mm_state *oob_mm = curr->oob_mm;
	struct evl_rq *this_rq = curr->rq;
	unsigned long flags;
	bool sigpending;
	int ret;

	/*
	 * The last thread resuming from a ptsync to switch back to
	 * out-of-band mode has to release the others which have been
	 * waiting for this event on the ptrace sync barrier.
	 */
	sigpending = signal_pending(current);
	ret = leave_ptsync(curr);
	if (ret > 0) {
		evl_flush_wait(oob_mm->ptsync_barrier, 0);
		ret = 0;
	} else if (ret < 0)
		ret = sigpending ? -ERESTARTSYS :
			evl_wait_event(oob_mm->ptsync_barrier,
				list_empty(&oob_mm->ptrace_sync));

	evl_spin_lock_irqsave(&this_rq->lock, flags);

	/*
	 * If we got interrupted while waiting on the ptsync barrier,
	 * make sure pick_next_thread() will let us slip through again
	 * by keeping T_PTSTOP set.
	 */
	if (!ret && !(curr->info & T_PTSIG)) {
		curr->info &= ~T_PTSTOP;
		curr->state &= ~T_PTRACE;
	}

	evl_spin_unlock_irqrestore(&this_rq->lock, flags);

	return ret ? -ERESTARTSYS : 0;
}

static void handle_retuser_event(void)
{
	struct evl_thread *curr = evl_current();
	int ret;

	ret = evl_switch_oob();
	if (ret) {
		/* Ask for retry until we succeed. */
		dovetail_request_ucall(current);
		return;
	}

	if (!(curr->state & T_PTRACE))
		return;

	ret = ptrace_sync();
	if (ret)
		dovetail_request_ucall(current);

	evl_schedule();

	if ((curr->state & T_WEAK) &&
		atomic_read(&curr->inband_disable_count) == 0)
		evl_switch_inband(SIGDEBUG_NONE);
}

static void handle_cleanup_event(struct mm_struct *mm)
{
	struct evl_thread *curr = evl_current();

	/*
	 * This event is fired whenever a user task is dropping its
	 * mm.
	 *
	 * Detect an EVL thread running exec(), i.e. still attached to
	 * the current in-band task but not bearing PF_EXITING, which
	 * indicates that it did not call do_exit(). In this case, we
	 * emulate a task exit, since the EVL binding shall not
	 * survive the exec() syscall.  We may get there after
	 * cleanup_current_thread() already ran, so check @curr for
	 * NULL.
	 *
	 * Otherwise, release the oob state for the dropped mm if
	 * any. We may or may not have one, EVL_MM_ACTIVE_BIT tells us
	 * so.
	 */
	if (curr && !(current->flags & PF_EXITING))
		put_current_thread();
	else
		flush_oob_mm_state(&mm->oob_state);
}

void handle_inband_event(enum inband_event_type event, void *data)
{
	switch (event) {
	case INBAND_TASK_SIGNAL:
		handle_sigwake_event(data);
		break;
	case INBAND_TASK_EXIT:
		put_current_thread();
		break;
	case INBAND_TASK_MIGRATION:
		handle_migration_event(data);
		break;
	case INBAND_TASK_RETUSER:
		handle_retuser_event();
		break;
	case INBAND_TASK_PTSTOP:
		handle_ptstop_event();
		break;
	case INBAND_TASK_PTCONT:
		handle_ptcont_event();
		break;
	case INBAND_TASK_PTSTEP:
		handle_ptstep_event(data);
		break;
	case INBAND_PROCESS_CLEANUP:
		handle_cleanup_event(data);
		break;
	}
}

/* thread->lock + thread->rq->lock held, irqs off */
static int set_time_slice(struct evl_thread *thread, ktime_t quantum)
{
	struct evl_rq *rq = thread->rq;

	assert_evl_lock(&thread->lock);
	assert_evl_lock(&rq->lock);

	thread->rrperiod = quantum;

	if (!timeout_infinite(quantum)) {
		if (quantum <= evl_get_clock_gravity(&evl_mono_clock, user))
			return -EINVAL;

		if (thread->base_class->sched_tick == NULL)
			return -EINVAL;

		thread->state |= T_RRB;
		if (rq->curr == thread)
			evl_start_timer(&rq->rrbtimer,
					evl_abs_timeout(&rq->rrbtimer, quantum),
					EVL_INFINITE);
	} else {
		thread->state &= ~T_RRB;
		if (rq->curr == thread)
			evl_stop_timer(&rq->rrbtimer);
	}

	return 0;
}

static int set_sched_attrs(struct evl_thread *thread,
			const struct evl_sched_attrs *attrs)
{
	struct evl_sched_class *sched_class;
	union evl_sched_param param;
	unsigned long flags;
	struct evl_rq *rq;
	ktime_t tslice;
	int ret;

	trace_evl_thread_setsched(thread, attrs);

	rq = evl_get_thread_rq(thread, flags);

	tslice = thread->rrperiod;
	sched_class = evl_find_sched_class(&param, attrs, &tslice);
	if (IS_ERR(sched_class)) {
		ret = PTR_ERR(sched_class);
		goto out;
	}

	ret = set_time_slice(thread, tslice);
	if (ret)
		goto out;

	ret = evl_set_thread_schedparam_locked(thread, sched_class, &param);
out:
	evl_put_thread_rq(thread, rq, flags);

	evl_schedule();

	return ret;
}

static void __get_sched_attrs(struct evl_sched_class *sched_class,
			struct evl_thread *thread,
			struct evl_sched_attrs *attrs)
{
	union evl_sched_param param;

	assert_thread_pinned(thread);

	attrs->sched_policy = sched_class->policy;

	sched_class->sched_getparam(thread, &param);

	if (sched_class == &evl_sched_fifo) {
		if (thread->state & T_RRB) {
			attrs->sched_rr_quantum =
				ktime_to_u_timespec(thread->rrperiod);
			attrs->sched_policy = SCHED_RR;
		}
		goto out;
	}

#ifdef CONFIG_EVL_SCHED_QUOTA
	if (sched_class == &evl_sched_quota) {
		attrs->sched_quota_group = param.quota.tgid;
		goto out;
	}
#endif

#ifdef CONFIG_EVL_SCHED_TP
	if (sched_class == &evl_sched_tp) {
		attrs->sched_tp_partition = param.tp.ptid;
		goto out;
	}
#endif

out:
	trace_evl_thread_getsched(thread, attrs);
}

static void get_sched_attrs(struct evl_thread *thread,
			struct evl_sched_attrs *attrs)
{
	unsigned long flags;
	struct evl_rq *rq;

	rq = evl_get_thread_rq(thread, flags);
	/* Get the base scheduling attributes. */
	attrs->sched_priority = thread->bprio;
	__get_sched_attrs(thread->base_class, thread, attrs);
	evl_put_thread_rq(thread, rq, flags);
}

void evl_get_thread_state(struct evl_thread *thread,
			struct evl_thread_state *statebuf)
{
	unsigned long flags;
	struct evl_rq *rq;

	rq = evl_get_thread_rq(thread, flags);
	/* Get the effective scheduling attributes. */
	statebuf->eattrs.sched_priority = thread->cprio;
	__get_sched_attrs(thread->sched_class, thread, &statebuf->eattrs);
	statebuf->cpu = evl_rq_cpu(thread->rq);
	statebuf->state = evl_rq_cpu(thread->rq);
	statebuf->isw = evl_get_counter(&thread->stat.isw);
	statebuf->csw = evl_get_counter(&thread->stat.csw);
	statebuf->sc = evl_get_counter(&thread->stat.sc);
	statebuf->rwa = evl_get_counter(&thread->stat.rwa);
	statebuf->xtime = ktime_to_ns(evl_get_account_total(
					&thread->stat.account));
	evl_put_thread_rq(thread, rq, flags);
}
EXPORT_SYMBOL_GPL(evl_get_thread_state);

static long thread_common_ioctl(struct evl_thread *thread,
				unsigned int cmd, unsigned long arg)
{
	struct evl_thread_state statebuf;
	struct evl_sched_attrs attrs;
	long ret;

	switch (cmd) {
	case EVL_THRIOC_SET_SCHEDPARAM:
		ret = raw_copy_from_user(&attrs,
					(struct evl_sched_attrs *)arg, sizeof(attrs));
		if (ret)
			return -EFAULT;
		ret = set_sched_attrs(thread, &attrs);
		break;
	case EVL_THRIOC_GET_SCHEDPARAM:
		get_sched_attrs(thread, &attrs);
		ret = raw_copy_to_user((struct evl_sched_attrs *)arg,
				&attrs, sizeof(attrs));
		if (ret)
			return -EFAULT;
		break;
	case EVL_THRIOC_GET_STATE:
		evl_get_thread_state(thread, &statebuf);
		ret = raw_copy_to_user((struct evl_thread_state *)arg,
				&statebuf, sizeof(statebuf));
		if (ret)
			return -EFAULT;
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static long thread_oob_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_thread *thread = element_of(filp, struct evl_thread);
	struct evl_thread *curr = evl_current();
	long ret = -EPERM;
	__u32 monfd, mask;

	if (thread->state & T_ZOMBIE)
		return -ESTALE;

	switch (cmd) {
	case EVL_THRIOC_SWITCH_OOB:
		if (thread == curr)
			ret = 0;	/* Already there. */
		break;
	case EVL_THRIOC_SWITCH_INBAND:
		if (thread == curr) {
			evl_switch_inband(SIGDEBUG_NONE);
			ret = 0;
		}
		break;
	case EVL_THRIOC_SIGNAL:
		ret = raw_get_user(monfd, (__u32 *)arg);
		if (ret)
			return -EFAULT;
		ret = evl_signal_monitor_targeted(thread, monfd);
		break;
	case EVL_THRIOC_SET_MODE:
	case EVL_THRIOC_CLEAR_MODE:
		if (thread == curr) {
			ret = raw_get_user(mask, (__u32 *)arg);
			if (ret)
				return -EFAULT;
			ret = evl_update_mode(mask, cmd == EVL_THRIOC_SET_MODE);
		}
		break;
	default:
		ret = thread_common_ioctl(thread, cmd, arg);
	}

	return ret;
}

static long thread_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_thread *thread = element_of(filp, struct evl_thread);
	struct evl_thread *curr = evl_current();
	long ret = -EPERM;

	if (thread->state & T_ZOMBIE)
		return -ESTALE;

	switch (cmd) {
	case EVL_THRIOC_SWITCH_INBAND:
		if (thread == curr)
			ret = 0;
		break;
	case EVL_THRIOC_DETACH_SELF:
		ret = evl_detach_self();
		break;
	case EVL_THRIOC_JOIN:
		ret = evl_join_thread(thread, false);
		break;
	default:
		ret = thread_common_ioctl(thread, cmd, arg);
	}

	return ret;
}

static const struct file_operations thread_fops = {
	.open		= evl_open_element,
	.release	= evl_release_element,
	.unlocked_ioctl	= thread_ioctl,
	.oob_ioctl	= thread_oob_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = compat_ptr_ioctl,
	.compat_oob_ioctl  = compat_ptr_oob_ioctl,
#endif
};

static int map_uthread_self(struct evl_thread *thread)
{
	struct evl_user_window *u_window;
	struct cred *newcap;
	int ret;

	ret = commit_process_memory();
	if (ret)
		return ret;

	u_window = evl_zalloc_chunk(&evl_shared_heap, sizeof(*u_window));
	if (u_window == NULL)
		return -ENOMEM;

	/*
	 * Raise capababilities of user threads when attached to the
	 * core. Filtering access to /dev/evl/control can be used to
	 * restrict attachment.
	 */
	thread->raised_cap = CAP_EMPTY_SET;
	newcap = prepare_creds();
	if (newcap == NULL)
		return -ENOMEM;

	add_u_cap(thread, newcap, CAP_SYS_NICE);
	add_u_cap(thread, newcap, CAP_IPC_LOCK);
	add_u_cap(thread, newcap, CAP_SYS_RAWIO);
	commit_creds(newcap);

	/*
	 * CAUTION: From that point, we assume the mapping won't fail,
	 * therefore there is no added capability to drop in
	 * discard_unmapped_uthread().
	 */
	thread->u_window = u_window;
	pin_to_initial_cpu(thread);
	trace_evl_thread_map(thread);

	dovetail_init_altsched(&thread->altsched);
	set_oob_threadinfo(thread);
	set_oob_mminfo(thread);

	/*
	 * CAUTION: we enable dovetailing only when *thread is
	 * consistent, so that we won't trigger false positive in
	 * debug code from handle_schedule_event() and friends.
	 */
	dovetail_start_altsched();

	/*
	 * A user-space thread is already started EVL-wise since we
	 * have an underlying in-band context for it, so we can
	 * enqueue it now.
	 */
	enqueue_new_thread(thread);
	evl_release_thread(thread, T_DORMANT, 0);
	evl_sync_uwindow(thread);

	return 0;
}

/*
 * Deconstruct a thread we just failed to map over a userland task.
 * Since the former must be dormant, it can't be part of any runqueue.
 */
static void discard_unmapped_uthread(struct evl_thread *thread)
{
	evl_destroy_timer(&thread->rtimer);
	evl_destroy_timer(&thread->ptimer);
	dequeue_old_thread(thread);

	if (thread->u_window)
		evl_free_chunk(&evl_shared_heap, thread->u_window);

	kfree(thread);
}

static struct evl_element *
thread_factory_build(struct evl_factory *fac, const char *name,
		void __user *u_attrs, u32 *state_offp)
{
	struct task_struct *tsk = current;
	struct evl_init_thread_attr iattr;
	struct evl_thread *curr;
	int ret;

	if (evl_current())
		return ERR_PTR(-EBUSY);

	/* @current must open the control device first. */
	if (!test_bit(EVL_MM_ACTIVE_BIT, &dovetail_mm_state()->flags))
		return ERR_PTR(-EPERM);

	curr = kzalloc(sizeof(*curr), GFP_KERNEL);
	if (curr == NULL)
		return ERR_PTR(-ENOMEM);

	ret = evl_init_element(&curr->element, &evl_thread_factory);
	if (ret) {
		kfree(curr);
		return ERR_PTR(ret);
	}

	iattr.flags = T_USER;
	iattr.affinity = cpu_possible_mask;
	iattr.sched_class = &evl_sched_weak;
	iattr.sched_param.weak.prio = 0;
	ret = evl_init_thread(curr, &iattr, NULL, "%s", name);
	if (ret) {
		evl_destroy_element(&curr->element);
		kfree(curr);
		return ERR_PTR(ret);
	}

	ret = map_uthread_self(curr);
	if (ret) {
		evl_destroy_element(&curr->element);
		discard_unmapped_uthread(curr);
		return ERR_PTR(ret);
	}

	*state_offp = evl_shared_offset(curr->u_window);
	evl_index_element(&curr->element);

	/*
	 * Unlike most elements, a thread may exist in absence of any
	 * file reference, so we get a reference on the emerging
	 * thread here to block automatic disposal on last file
	 * release. put_current_thread() drops this reference when the
	 * thread exits, or voluntarily detaches by sending the
	 * EVL_THRIOC_DETACH_SELF control request.
	 */
	evl_get_element(&curr->element);

	strncpy(tsk->comm, name, sizeof(tsk->comm));
	tsk->comm[sizeof(tsk->comm) - 1] = '\0';

	return &curr->element;
}

static void thread_factory_dispose(struct evl_element *e)
{
	struct evl_thread *thread;
	int state;

	thread = container_of(e, struct evl_thread, element);
	state = thread->state;

	/*
	 * Two ways to get into the disposal handler: either
	 * open_factory_node() failed creating a device for @thread
	 * which is current, or after the last file reference to
	 * @thread was dropped after exit. T_ZOMBIE cleared denotes
	 * the first case, otherwise @thread has existed, is now dead
	 * and no more reachable, so we can wakeup joiners if any.
	 */
	if (likely(state & T_ZOMBIE)) {
		evl_destroy_element(&thread->element);
		complete_all(&thread->exited);	 /* evl_join_thread() */
		if (waitqueue_active(&join_all)) /* evl_killall() */
			wake_up(&join_all);
	} else {
		if (EVL_WARN_ON(CORE, evl_current() != thread))
			return;
		cleanup_current_thread();
		evl_destroy_element(&thread->element);
	}

	if (state & T_USER)
		kfree_rcu(thread, element.rcu);
}

static ssize_t state_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct evl_thread *thread;
	ssize_t ret;

	thread = evl_get_element_by_dev(dev, struct evl_thread);
	if (thread == NULL)
		return -EIO;

	ret = snprintf(buf, PAGE_SIZE, "%#x\n", thread->state);

	evl_put_element(&thread->element);

	return ret;
}
static DEVICE_ATTR_RO(state);

static ssize_t sched_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct evl_sched_class *sched_class;
	struct evl_thread *thread;
	int bprio, cprio, cpu;
	unsigned long flags;
	ssize_t ret, _ret;

	thread = evl_get_element_by_dev(dev, struct evl_thread);
	if (thread == NULL)
		return -EIO;

	evl_spin_lock_irqsave(&thread->lock, flags);

	sched_class = thread->sched_class;
	bprio = thread->bprio;
	cprio = thread->cprio;
	cpu = evl_rq_cpu(thread->rq);

	ret = snprintf(buf, PAGE_SIZE, "%d %d %d %s ",
		cpu, bprio, cprio, sched_class->name);

	if (sched_class->sched_show) {
		_ret = sched_class->sched_show(thread, buf + ret,
					PAGE_SIZE - ret);
		if (_ret > 0) {
			ret += _ret;
			goto out;
		}
	}

	/* overwrites trailing whitespace */
	buf[ret - 1] = '\n';
out:
	evl_spin_unlock_irqrestore(&thread->lock, flags);
	evl_put_element(&thread->element);

	return ret;
}
static DEVICE_ATTR_RO(sched);

#ifdef CONFIG_EVL_RUNSTATS

static ssize_t stats_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	ktime_t period, exectime, account, total;
	struct evl_thread *thread;
	unsigned long flags;
	struct evl_rq *rq;
	ssize_t ret;
	int usage;

	thread = evl_get_element_by_dev(dev, struct evl_thread);
	if (thread == NULL)
		return -EIO;

	evl_spin_lock_irqsave(&thread->lock, flags);

	rq = evl_thread_rq(thread);

	period = rq->last_account_switch - thread->stat.lastperiod.start;
	if (period == 0 && thread == rq->curr) {
		exectime = ktime_set(0, 1);
		account = ktime_set(0, 1);
	} else {
		exectime = thread->stat.account.total -
			thread->stat.lastperiod.total;
		account = period;
	}

	total = thread->stat.account.total;
	thread->stat.lastperiod.total = total;
	thread->stat.lastperiod.start = rq->last_account_switch;

	evl_spin_unlock_irqrestore(&thread->lock, flags);

	if (account) {
		while (account > 0xffffffffUL) {
			exectime >>= 16;
			account >>= 16;
		}

		exectime = ns_to_ktime(ktime_to_ns(exectime) * 1000LL +
				ktime_to_ns(account) / 2);
		usage = ktime_divns(exectime, ktime_to_ns(account));
	} else
		usage = 0;

	ret = snprintf(buf, PAGE_SIZE, "%lu %lu %lu %lu %Lu %d\n",
		thread->stat.isw.counter,
		thread->stat.csw.counter,
		thread->stat.sc.counter,
		thread->stat.rwa.counter,
		total,
		usage);

	evl_put_element(&thread->element);

	return ret;
}

#else

static ssize_t stats_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0 0 0 0 0\n");
}

#endif	/* !CONFIG_EVL_RUNSTATS */

static DEVICE_ATTR_RO(stats);

static ssize_t timeout_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct evl_thread *thread;
	ssize_t ret;

	thread = evl_get_element_by_dev(dev, struct evl_thread);
	if (thread == NULL)
		return -EIO;

	ret = snprintf(buf, PAGE_SIZE, "%Lu\n",
		ktime_to_ns(evl_get_thread_timeout(thread)));

	evl_put_element(&thread->element);

	return ret;
}
static DEVICE_ATTR_RO(timeout);

static ssize_t pid_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct evl_thread *thread;
	ssize_t ret;

	thread = evl_get_element_by_dev(dev, struct evl_thread);
	if (thread == NULL)
		return -EIO;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", evl_get_inband_pid(thread));

	evl_put_element(&thread->element);

	return ret;
}
static DEVICE_ATTR_RO(pid);

static struct attribute *thread_attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_sched.attr,
	&dev_attr_timeout.attr,
	&dev_attr_stats.attr,
	&dev_attr_pid.attr,
	NULL,
};
ATTRIBUTE_GROUPS(thread);

struct evl_factory evl_thread_factory = {
	.name	=	EVL_THREAD_DEV,
	.fops	=	&thread_fops,
	.build	=	thread_factory_build,
	.dispose =	thread_factory_dispose,
	.nrdev	=	CONFIG_EVL_NR_THREADS,
	.attrs	=	thread_groups,
	.flags	=	EVL_FACTORY_CLONE,
};
