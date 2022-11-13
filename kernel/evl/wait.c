/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001, 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evl/sched.h>
#include <evl/wait.h>
#include <evl/thread.h>
#include <evl/clock.h>
#include <uapi/evl/signal.h>
#include <trace/events/evl.h>

void __evl_init_wait(struct evl_wait_queue *wq,
		struct evl_clock *clock, int wq_flags,
		const char *name,
		struct lock_class_key *lock_key)
{
	unsigned long flags __maybe_unused;

	wq->flags = wq_flags;
	wq->clock = clock;
	wq->wchan.pi_serial = 0;
	wq->wchan.owner = NULL;
	wq->wchan.requeue_wait = evl_requeue_wait;
	wq->wchan.name = name;
	INIT_LIST_HEAD(&wq->wchan.wait_list);
	raw_spin_lock_init(&wq->wchan.lock);
#ifdef CONFIG_LOCKDEP
	if (lock_key) {
		wq->lock_key_addr.addr = lock_key;
		lockdep_set_class_and_name(&wq->wchan.lock, lock_key, name);
	} else {
		lock_key = &wq->wchan.lock_key;
		wq->lock_key_addr.addr = lock_key;
		lockdep_register_key(lock_key);
		lockdep_set_class_and_name(&wq->wchan.lock, lock_key, name);
		/*
		 * might_lock() forces lockdep to pre-register a
		 * dynamic lock class, instead of waiting lazily for
		 * the first lock acquisition, which might happen oob
		 * for us. Since that registration depends on RCU, we
		 * need to make sure this happens in-band.
		 */
		local_irq_save(flags);
		might_lock(&wq->wchan.lock);
		local_irq_restore(flags);
	}
#endif
}
EXPORT_SYMBOL_GPL(__evl_init_wait);

void evl_destroy_wait(struct evl_wait_queue *wq)
{
	evl_flush_wait(wq, T_RMID);
	evl_schedule();
#ifdef CONFIG_LOCKDEP
	/* Drop dynamic key. */
	if (wq->lock_key_addr.addr == &wq->wchan.lock_key)
		lockdep_unregister_key(&wq->wchan.lock_key);
#endif
}
EXPORT_SYMBOL_GPL(evl_destroy_wait);

/* wq->wchan.lock held, hard irqs off */
static void __evl_add_wait_queue(struct evl_thread *curr,
				struct evl_wait_queue *wq,
				ktime_t timeout, enum evl_tmode timeout_mode)
{
	assert_hard_lock(&wq->wchan.lock);

	trace_evl_wait(&wq->wchan);

	if (!(wq->flags & EVL_WAIT_PRIO))
		list_add_tail(&curr->wait_next, &wq->wchan.wait_list);
	else
		list_add_priff(curr, &wq->wchan.wait_list, wprio, wait_next);

	evl_sleep_on(timeout, timeout_mode, wq->clock, &wq->wchan);
}

void evl_add_wait_queue(struct evl_wait_queue *wq, ktime_t timeout,
			enum evl_tmode timeout_mode)
{
	struct evl_thread *curr = evl_current();

	if ((curr->state & T_WOLI) &&
		atomic_read(&curr->held_mutex_count) > 0)
		evl_notify_thread(curr, EVL_HMDIAG_LKSLEEP, evl_nil);

	__evl_add_wait_queue(curr, wq, timeout, timeout_mode);

}
EXPORT_SYMBOL_GPL(evl_add_wait_queue);

void evl_add_wait_queue_unchecked(struct evl_wait_queue *wq, ktime_t timeout,
				enum evl_tmode timeout_mode)
{
	struct evl_thread *curr = evl_current();

	__evl_add_wait_queue(curr, wq, timeout, timeout_mode);
}

/* wq->wchan.lock held, hard irqs off */
struct evl_thread *evl_wake_up(struct evl_wait_queue *wq,
			struct evl_thread *waiter,
			int reason)
{
	assert_hard_lock(&wq->wchan.lock);

	trace_evl_wake_up(&wq->wchan);

	if (list_empty(&wq->wchan.wait_list)) {
		waiter = NULL;
	} else {
		if (waiter == NULL)
			waiter = list_first_entry(&wq->wchan.wait_list,
						struct evl_thread, wait_next);
		list_del_init(&waiter->wait_next);
		evl_wakeup_thread(waiter, T_PEND, reason);
	}

	return waiter;
}
EXPORT_SYMBOL_GPL(evl_wake_up);

/* wq->wchan.lock held, hard irqs off */
void evl_flush_wait_locked(struct evl_wait_queue *wq, int reason)
{
	struct evl_thread *waiter, *tmp;

	assert_hard_lock(&wq->wchan.lock);

	trace_evl_flush_wait(&wq->wchan);

	list_for_each_entry_safe(waiter, tmp, &wq->wchan.wait_list, wait_next) {
		list_del_init(&waiter->wait_next);
		evl_wakeup_thread(waiter, T_PEND, reason);
	}
}
EXPORT_SYMBOL_GPL(evl_flush_wait_locked);

void evl_flush_wait(struct evl_wait_queue *wq, int reason)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&wq->wchan.lock, flags);
	evl_flush_wait_locked(wq, reason);
	raw_spin_unlock_irqrestore(&wq->wchan.lock, flags);
}
EXPORT_SYMBOL_GPL(evl_flush_wait);

static inline struct evl_wait_queue *
wchan_to_wait_queue(struct evl_wait_channel *wchan)
{
	return container_of(wchan, struct evl_wait_queue, wchan);
}

/* wchan->lock + waiter->lock held, irqs off. */
void evl_requeue_wait(struct evl_wait_channel *wchan, struct evl_thread *waiter)
{
	struct evl_wait_queue *wq = wchan_to_wait_queue(wchan);

	assert_hard_lock(&wchan->lock);
	assert_hard_lock(&waiter->lock);

	if (wq->flags & EVL_WAIT_PRIO) {
		list_del(&waiter->wait_next);
		list_add_priff(waiter, &wq->wchan.wait_list, wprio, wait_next);
	}
}
EXPORT_SYMBOL_GPL(evl_requeue_wait);

int __evl_wait_schedule(struct evl_wait_channel *wchan)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;
	int ret = 0, info;

	evl_schedule();

	trace_evl_finish_wait(wchan);

	/*
	 * Upon return from schedule, we may or may not have been
	 * unlinked from the wait channel, depending on whether we
	 * actually resumed as a result of receiving a wakeup signal
	 * from evl_wake_up() or evl_flush_wait(). The following logic
	 * applies in order, depending on the information flags:
	 *
	 * - if T_RMID is set, evl_flush_wait() removed us from the
	 * waitqueue before the wait channel got destroyed, and
	 * therefore cannot be referred to anymore since it may be
	 * stale: -EIDRM is returned.
	 *
	 * - if neither T_TIMEO or T_BREAK are set, we got a wakeup
	 * and success (zero) or -ENOMEM is returned, depending on
	 * whether T_NOMEM is set (i.e. the operation was aborted due
	 * to a memory shortage). In addition, the caller may need to
	 * check for T_BCAST if the signal is not paired with a
	 * condition but works as a pulse instead.
	 *
	 * - otherwise, if any of T_TIMEO or T_BREAK is set:
	 *
	 *   + if we are still linked to the waitqueue, the wait was
	 * aborted prior to receiving any wakeup so we translate the
	 * information bit to the corresponding error status,
	 * i.e. -ETIMEDOUT or -EINTR respectively.
	 *
	 *  + in the rare case where we have been unlinked and we also
	 * got any of T_TIMEO|T_BREAK, then both the wakeup signal and
	 * some abort condition have occurred simultaneously on
	 * different cores, in which case we ignore the latter. In the
	 * particular case of T_BREAK caused by
	 * handle_sigwake_event(), T_KICKED will be detected on the
	 * return path from the OOB syscall, yielding -ERESTARTSYS as
	 * expected.
	 */
	info = evl_current()->info;
	if (info & T_RMID)
		return -EIDRM;

	if (info & T_NOMEM)
		return -ENOMEM;

	if (info & (T_TIMEO|T_BREAK)) {
		raw_spin_lock_irqsave(&wchan->lock, flags);
		if (!list_empty(&curr->wait_next)) {
			list_del_init(&curr->wait_next);
			if (info & T_TIMEO)
				ret = -ETIMEDOUT;
			else if (info & T_BREAK)
				ret = -EINTR;
		}
		raw_spin_unlock_irqrestore(&wchan->lock, flags);
	} else if (IS_ENABLED(CONFIG_EVL_DEBUG_CORE)) {
		bool empty;
		raw_spin_lock_irqsave(&wchan->lock, flags);
		empty = list_empty(&curr->wait_next);
		raw_spin_unlock_irqrestore(&wchan->lock, flags);
		EVL_WARN_ON_ONCE(CORE, !empty);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(__evl_wait_schedule);

struct evl_thread *evl_wait_head(struct evl_wait_queue *wq)
{
	assert_hard_lock(&wq->wchan.lock);
	return list_first_entry_or_null(&wq->wchan.wait_list,
					struct evl_thread, wait_next);
}
EXPORT_SYMBOL_GPL(evl_wait_head);

/*
 * Requeue a thread which changed priority in the wait channel it
 * pends on, then propagate the change down the PI chain.
 *
 * On entry: irqs off.
 */
void evl_adjust_wait_priority(struct evl_thread *thread,
			      enum evl_walk_mode mode)
{
	struct evl_wait_channel *wchan;
	struct evl_thread *owner;

	EVL_WARN_ON_ONCE(CORE, !hard_irqs_disabled());

	raw_spin_lock(&thread->lock);
	wchan = evl_get_thread_wchan(thread);
	raw_spin_unlock(&thread->lock);

	if (!wchan)
		return;

	owner = wchan->owner;
	/* Careful: waitqueues have no owner. */
	if (owner) {
		evl_double_thread_lock(thread, owner);
		wchan->requeue_wait(wchan, thread);
		raw_spin_unlock(&owner->lock);
		evl_walk_pi_chain(wchan, thread, mode);
	} else {
		raw_spin_lock(&thread->lock);
		wchan->requeue_wait(wchan, thread);
	}

	raw_spin_unlock(&thread->lock);
	evl_put_thread_wchan(wchan);
}

/*
 * Walking the PI chain deals with the following items:
 *
 * waiter := the thread which waits on wchan
 * wchan := the wait channel being processed
 * owner := the current owner of wchan
 *
 * In order to go fine-grained and escape ABBA situations, we hold
 * three locks at most while walking the PI chain, keeping a
 * consistent locking sequence among them as follows:
 *
 * lock(wchan)
 *      lock(waiter, owner)
 *
 * Rules:
 *
 * 1. holding wchan->lock guarantees that wchan->owner is stable.
 *
 * 2. holding thread->lock guarantees that thread->wchan is stable.
 *
 * 3. there is no rule #3.
 *
 * On entry:
 * orig_wchan->lock held + orig_waiter->lock held, irqs off.
 */
int evl_walk_pi_chain(struct evl_wait_channel *orig_wchan,
		struct evl_thread *orig_waiter,
		enum evl_walk_mode mode)
{
	static atomic64_t pi_serial;
	struct evl_thread *waiter = orig_waiter, *curr = evl_current(), *owner;
	struct evl_wait_channel *wchan = orig_wchan, *next_wchan;
	s64 serial = atomic64_inc_return(&pi_serial);
	bool do_reorder = false;
	int ret = 0;

	assert_hard_lock(&orig_wchan->lock);
	assert_hard_lock(&orig_waiter->lock);

	/*
	 * The loop is always exited with two, and only two locks
	 * held, namely: wchan->lock and waiter->lock. Also, *wchan
	 * advances in lock-step with *waiter.
	 *
	 * Since we drop locks inside the PI walk, we might have
	 * concurrent walks on the same chain from different CPUs,
	 * contradicting each other when it comes to the priority to
	 * set for a thread. We solve this issue as follows:
	 *
	 * - a unique serial number is drawn at each invocation of
	 * this routine (pi_serial). A wait channel maintains a cached
	 * copy of this serial number, which is updated under lock.
	 *
	 * - before updating the priority of the owner of a wait
	 * channel, the serial number drawn on entry (pi_serial) is
	 * compared to the cached value: if this value is lower than
	 * pi_serial, the change to wchan->owner can proceed, and the
	 * cached copy is updated into the wait channel. Otherwise,
	 * this means that a concurrent walk started later must
	 * override the effect of the current one, in which case the
	 * current PI walk is aborted.
	 *
	 * IOW, we ensure that the latest PI walk always overrides the
	 * effect of walks started earlier. The initial serialization
	 * is granted by orig_wchan->lock, which is held on entry to
	 * this routine.
	 */
	for (;;) {
		owner = wchan->owner;
		if (!owner) /* End of PI chain, we are done. */
			break;

		if (owner == orig_waiter) {
			ret = -EDEADLK;
			break;
		}

		/*
		 * Lock both the waiter and owner by increasing
		 * address order to escape the ABBA issue.  To this
		 * end, we have to drop waiter->lock temporarily
		 * first, so that we can re-lock it at the right place
		 * in the sequence. In the meantime, the waiter is
		 * prevented to go stale by holding a reference on it.
		 */
		evl_get_element(&waiter->element);
		raw_spin_unlock(&waiter->lock);

		evl_double_thread_lock(waiter, owner);
		evl_put_element(&waiter->element);

		/*
		 * Multiple PI walks may happen concurrently, detect
		 * and abort the oldest ones based on their serial
		 * number, ensuring the latest overrides them.
		 */
		if (mode != evl_pi_check && serial - wchan->pi_serial < 0)
			break;

		wchan->pi_serial = serial;

		/*
		 * Make sure the waiter did not stop waiting on wchan
		 * while unlocked (we have been holding wchan->lock
		 * across the unlocked section, so we know that wchan
		 * did not go stale). If the waiter is current, it is
		 * certainly about to block on wchan, so this check
		 * does not apply (besides, waiter->wchan does not
		 * point at wchan yet).
		 */
		if (waiter != curr && waiter->wchan != wchan)
			break;

		/*
		 * The current wait channel may need to reorder its
		 * internal wait queue due to a priority change for
		 * the waiter during the previous iteration.
		 *
		 * This also means that for a short while, a waiter
		 * may benefit from a priority boost not yet accounted
		 * for in the wait queue of the channel its pends on,
		 * which is ok since there cannot be any rescheduling
		 * on the current CPU between both events.
		 */
		if (do_reorder) {
			wchan->requeue_wait(wchan, waiter);
			do_reorder = false;
		}

		if (mode == evl_pi_adjust) {
			/*
			 * If priorities already match, there is no
			 * more change to propagate downstream. Stop
			 * the walk now.
			 */
			if (waiter->wprio == owner->wprio) {
				raw_spin_unlock(&owner->lock);
				break;
			}
			/* owner.cprio <- waiter.cprio */
			evl_track_thread_policy(owner, waiter);
			do_reorder = true;
		} else if (mode == evl_pi_reset) {
			/* owner.cprio <- owner.bprio */
			if (owner->state & T_BOOST)
				evl_track_thread_policy(owner, owner);
			do_reorder = true;
		}

		/*
		 * Drop the lock on the current wait channel we don't
		 * need anymore, so that we don't risk entering an
		 * ABBA pattern lockdep would complain about as a
		 * result of locking owner->wchan in
		 * evl_get_thread_wchan().
		 */
		raw_spin_unlock(&waiter->lock);
		raw_spin_unlock(&wchan->lock);
		/* Now acquire owner->wchan if non-NULL. */
		next_wchan = evl_get_thread_wchan(owner);
		if (!next_wchan) {
			raw_spin_unlock(&owner->lock);
			raw_spin_lock(&orig_wchan->lock);
			raw_spin_lock(&orig_waiter->lock);
			return 0;
		}

		waiter = owner;
		wchan = next_wchan;
	}

	if (waiter != orig_waiter) {
		raw_spin_unlock(&waiter->lock);
		raw_spin_unlock(&wchan->lock);
		raw_spin_lock(&orig_wchan->lock);
		raw_spin_lock(&orig_waiter->lock);
	}

	return ret;
}
