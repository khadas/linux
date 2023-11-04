/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001, 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evl/lock.h>
#include <evl/wait.h>
#include <evl/thread.h>
#include <uapi/evl/signal.h>
#include <trace/events/evl.h>

void __evl_init_wait(struct evl_wait_queue *wq,
		struct evl_clock *clock, int wq_flags,
		const char *name,
		struct lock_class_key *lock_key)
{
	wq->flags = wq_flags;
	wq->clock = clock;
	wq->wchan.owner = NULL;
	wq->wchan.requeue_wait = evl_requeue_wait;
	wq->wchan.name = name;
	INIT_LIST_HEAD(&wq->wchan.wait_list);
	raw_spin_lock_init(&wq->wchan.lock);
	lockdep_set_class_and_name(&wq->wchan.lock, lock_key, name);
	might_hard_lock(&wq->wchan.lock);
}
EXPORT_SYMBOL_GPL(__evl_init_wait);

void evl_destroy_wait(struct evl_wait_queue *wq)
{
	evl_flush_wait(wq, EVL_T_RMID);
	evl_schedule();
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

	if ((curr->state & EVL_T_WOLI) &&
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
		evl_wakeup_thread(waiter, EVL_T_PEND, reason);
	}

	return waiter;
}
EXPORT_SYMBOL_GPL(evl_wake_up);

/* wq->wchan.lock held, hard irqs off */
int evl_flush_wait_locked(struct evl_wait_queue *wq, int reason)
{
	struct evl_thread *waiter, *tmp;
	int ret = 0;

	assert_hard_lock(&wq->wchan.lock);

	trace_evl_flush_wait(&wq->wchan);

	list_for_each_entry_safe(waiter, tmp, &wq->wchan.wait_list, wait_next) {
		list_del_init(&waiter->wait_next);
		evl_wakeup_thread(waiter, EVL_T_PEND, reason);
		ret++;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(evl_flush_wait_locked);

int evl_flush_wait(struct evl_wait_queue *wq, int reason)
{
	unsigned long flags;
	int ret;

	raw_spin_lock_irqsave(&wq->wchan.lock, flags);
	ret = evl_flush_wait_locked(wq, reason);
	raw_spin_unlock_irqrestore(&wq->wchan.lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_flush_wait);

static inline struct evl_wait_queue *
wchan_to_wait_queue(struct evl_wait_channel *wchan)
{
	return container_of(wchan, struct evl_wait_queue, wchan);
}

/* wchan->lock + waiter->lock held, irqs off. */
bool evl_requeue_wait(struct evl_wait_channel *wchan, struct evl_thread *waiter)
{
	struct evl_wait_queue *wq = wchan_to_wait_queue(wchan);

	assert_hard_lock(&wchan->lock);
	assert_hard_lock(&waiter->lock);

	if (wq->flags & EVL_WAIT_PRIO) {
		list_del(&waiter->wait_next);
		list_add_priff(waiter, &wq->wchan.wait_list, wprio, wait_next);
	}

	return true;
}
EXPORT_SYMBOL_GPL(evl_requeue_wait);

int __evl_wait_schedule(struct evl_wait_channel *wchan)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;
	int info;

	evl_schedule();

	trace_evl_finish_wait(wchan);

	/*
	 * Upon return from a wait state, the following logic applies
	 * depending on the information flags:
	 *
	 * - if none of EVL_T_RMID, EVL_T_NOMEM, EVL_T_TIMEO or EVL_T_BREAK is set, we
	 * got a wakeup upon a successful operation. In this case, we
	 * should not be linked to the waitqueue anymore. NOTE: the
	 * caller may need to check for EVL_T_BCAST if the signal is not
	 * paired with a condition but works as a pulse instead.
	 *
	 * - if EVL_T_RMID is set, evl_flush_wait() removed us from the
	 * waitqueue before the wait channel got destroyed, and
	 * therefore cannot be referred to anymore since it may be
	 * stale: -EIDRM is returned.
	 *
	 * - otherwise, we may still be linked to the waitqueue if the
	 * wait was aborted prior to receiving any wakeup, in which
	 * case we have to drop out from there. Timeout or break
	 * condition might be followed by a normal wakeup, in which
	 * case the former prevails wrt the status returned to the
	 * caller.
	 *
	 * INVARIANT: only the sleep-and-retry scheme to grab a
	 * resource is safe and assumed throughout the
	 * implementation. On the contrary, passing a resource from
	 * the thread which releases it directly to the one it is
	 * being granted to would be UNSAFE, since we could have a
	 * stale resource left over upon a timeout or break condition
	 * returned to the caller.
	 */
	info = evl_current()->info;
	if (likely(!(info & (EVL_T_RMID|EVL_T_NOMEM|EVL_T_TIMEO|EVL_T_BREAK)))) { /* Fast path. */
		if (IS_ENABLED(CONFIG_EVL_DEBUG_CORE)) {
			bool empty;
			raw_spin_lock_irqsave(&wchan->lock, flags);
			empty = list_empty(&curr->wait_next);
			raw_spin_unlock_irqrestore(&wchan->lock, flags);
			EVL_WARN_ON_ONCE(CORE, !empty);
		}
		return 0;
	}

	if (info & EVL_T_RMID)
		return -EIDRM;

	raw_spin_lock_irqsave(&wchan->lock, flags);

	if (!list_empty(&curr->wait_next))
		list_del_init(&curr->wait_next);

	raw_spin_unlock_irqrestore(&wchan->lock, flags);

	if (info & EVL_T_NOMEM)
		return -ENOMEM;

	return info & EVL_T_TIMEO ? -ETIMEDOUT : -EINTR;
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
 * pends on, then propagate the change down the lock chain.
 *
 * On entry: irqs off.
 */
void evl_adjust_wait_priority(struct evl_thread *thread)
{
	struct evl_wait_channel *wchan;
	struct evl_thread *owner;

	EVL_WARN_ON_ONCE(CORE, !hard_irqs_disabled());

	raw_spin_lock(&thread->lock);
	wchan = evl_get_thread_wchan(thread);
	if (!wchan) {
		raw_spin_unlock(&thread->lock);
		return;
	}

	/*
	 * Careful: mere waitqueues have no owner, only mutexes may
	 * be owned.
	 */
	owner = wchan->owner;
	if (owner) {
		wchan->requeue_wait(wchan, thread);
		raw_spin_unlock(&thread->lock);
		evl_double_thread_lock(thread, owner);
		/* Walking the chain drops owner->lock. */
		evl_walk_lock_chain(wchan, thread, false);
	} else {
		wchan->requeue_wait(wchan, thread);
	}

	raw_spin_unlock(&thread->lock);
	evl_put_thread_wchan(wchan);
}

/*
 * Walking the lock chain deals with the following items:
 *
 * waiter := the thread which waits on wchan
 * wchan := the wait channel being processed
 * owner := the current owner of wchan
 *
 * In order to go fine-grained and escape ABBA situations, we hold
 * three locks at most while walking the lock chain, keeping a
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
 *
 * (orig_wchan->lock + orig_wchan->owner->lock + orig_waiter->lock)
 * held, irqs off. All these locks may be dropped temporarily during
 * the walk. orig_wchan->owner->lock is NOT reacquired at exit.
 */
int evl_walk_lock_chain(struct evl_wait_channel *orig_wchan,
			struct evl_thread *orig_waiter,
			bool check_only)
{
	struct evl_thread *waiter = orig_waiter, *owner = orig_wchan->owner;
	struct evl_wait_channel *wchan = orig_wchan, *next_wchan;
	int ret = 0;

	assert_hard_lock(&wchan->lock);
	assert_hard_lock(&waiter->lock);

	/* We must have a valid owner for the origin wchan. */
	if (EVL_WARN_ON_ONCE(CORE, !owner))
		return -EIO;

	assert_hard_lock(&owner->lock);

	/* The chain must be sane on entry. */
	if (EVL_WARN_ON_ONCE(CORE, owner == orig_waiter)) {
		raw_spin_unlock(&owner->lock);
		return -EDEADLK;
	}

	evl_get_element(&orig_waiter->element);
	raw_spin_unlock(&orig_waiter->lock);

	/*
	 * Since we drop locks inside the loop, we might have
	 * concurrent walks on the same chain from different CPUs,
	 * contradicting each other when it comes to the priority to
	 * set for a thread. This issue is solved by
	 * evl_adjust_thread_boost() which solely considers the
	 * current state of a thread's booster list in order to give
	 * it the proper priority at any point in time.
	 */
	for (;;) {
		/*
		 * We already hold wchan->owner->lock, so dropping
		 * wchan->lock is safe.
		 */
		raw_spin_unlock(&wchan->lock);

		/*
		 * If no adjustment was deemed necessary, we may stop
		 * the walk right here, since there would be no
		 * priority change to propagate further down the
		 * chain.
		 */
		if (!check_only && !evl_adjust_thread_boost(owner)) {
			raw_spin_unlock(&owner->lock);
			break;
		}

		next_wchan = evl_get_thread_wchan(owner);
		if (!next_wchan) {
			raw_spin_unlock(&owner->lock);
			break;
		}

		/*
		 * Progress down the lock chain, at this point we hold
		 * owner->lock and next_wchan->lock.
		 */
		waiter = owner;
		wchan = next_wchan;

		/*
		 * If the priority of the last owner we visited has
		 * changed, update its position into the wait channel
		 * it sleeps on.
		 */
		if (!check_only && !wchan->requeue_wait(wchan, waiter))
			goto unlock_out;

		owner = wchan->owner;
		if (!owner) /* End of lock chain, we are done. */
			goto unlock_out;

		if (owner == orig_waiter) {
			ret = -EDEADLK;
			goto unlock_out;
		}

		raw_spin_unlock(&waiter->lock);
		/*
		 * We must grab this lock before we drop wchan->lock
		 * at the next iteration.
		 */
		raw_spin_lock(&owner->lock);
	}
out:
	raw_spin_lock(&orig_wchan->lock);
	raw_spin_lock(&orig_waiter->lock);
	evl_put_element(&orig_waiter->element);

	return ret;

unlock_out:
	raw_spin_unlock(&waiter->lock);
	raw_spin_unlock(&wchan->lock);
	goto out;
}
