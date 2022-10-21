/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001, 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/kernel.h>
#include <evl/timer.h>
#include <evl/clock.h>
#include <evl/sched.h>
#include <evl/thread.h>
#include <evl/mutex.h>
#include <evl/monitor.h>
#include <evl/wait.h>
#include <uapi/evl/signal.h>
#include <trace/events/evl.h>

#define for_each_evl_mutex_waiter(__pos, __mutex)	\
	list_for_each_entry(__pos, &(__mutex)->wchan.wait_list, wait_next)

enum evl_boost_modes {
	evl_pi_adjust,		/* Adjust the PI chain. */
	evl_pi_check,		/* Check PI chain only (no change). */
};

static inline
void double_thread_lock(struct evl_thread *t1, struct evl_thread *t2)
{
	EVL_WARN_ON_ONCE(CORE, t1 == t2);
	EVL_WARN_ON_ONCE(CORE, !hard_irqs_disabled());

	/*
	 * Prevent ABBA deadlock, always lock threads in address
	 * order. The caller guarantees t1 and t2 are distinct.
	 */
	if (t1 < t2) {
		raw_spin_lock(&t1->lock);
		raw_spin_lock(&t2->lock);
	} else {
		raw_spin_lock(&t2->lock);
		raw_spin_lock(&t1->lock);
	}
}

static inline int get_ceiling_value(struct evl_mutex *mutex)
{
	/*
	 * The ceiling priority value is stored in user-writable
	 * memory, make sure to constrain it within valid bounds for
	 * evl_sched_fifo before using it.
	 */
	return clamp(*mutex->ceiling_ref, 1U, (u32)EVL_FIFO_MAX_PRIO);
}

static inline void disable_inband_switch(struct evl_thread *curr)
{
	/*
	 * Track mutex locking depth: 1) to prevent weak threads from
	 * being switched back to in-band context on return from OOB
	 * syscalls, 2) when locking consistency is being checked.
	 */
	if (curr->state & (T_WEAK|T_WOLI))
		atomic_inc(&curr->inband_disable_count);
}

static inline bool enable_inband_switch(struct evl_thread *curr)
{
	if (likely(!(curr->state & (T_WEAK|T_WOLI))))
		return true;

	if (likely(atomic_dec_return(&curr->inband_disable_count) >= 0))
		return true;

	atomic_set(&curr->inband_disable_count, 0);
	if (curr->state & T_WOLI)
		evl_notify_thread(curr, EVL_HMDIAG_LKIMBALANCE, evl_nil);

	return false;
}

/* owner->lock held, irqs off. */
static void raise_boost_flag(struct evl_thread *owner)
{
	assert_hard_lock(&owner->lock);

	raw_spin_lock(&owner->rq->lock);

	/* Backup the base priority at first boost only. */
	if (!(owner->state & T_BOOST)) {
		owner->bprio = owner->cprio;
		owner->state |= T_BOOST;
	}

	raw_spin_unlock(&owner->rq->lock);
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
static int walk_pi_chain(struct evl_wait_channel *orig_wchan,
			struct evl_thread *orig_waiter,
			enum evl_boost_modes mode)
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

		double_thread_lock(waiter, owner);
		evl_put_element(&waiter->element);

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
			/* owner.pri <- waiter.pri */
			evl_track_thread_policy(owner, waiter);
			do_reorder = true;
		}

		next_wchan = owner->wchan;
		/*
		 * FIXME: next_wchan might become unsafe as a result of
		 * unlocking the owner.
		 */
		evl_get_element(&owner->element);
		raw_spin_unlock(&owner->lock);
		if (!next_wchan) {
			evl_put_element(&owner->element);
			break;
		}

		raw_spin_unlock(&waiter->lock);
		raw_spin_unlock(&wchan->lock);

		waiter = owner;
		wchan = next_wchan;
		raw_spin_lock(&wchan->lock);
		raw_spin_lock(&waiter->lock);
		evl_put_element(&waiter->element);

		/*
		 * Detect and drop overriden PI adjustment walks. This
		 * might have happened since we drop wchan->lock as we
		 * progress down the PI chain.
		 */
		if (mode == evl_pi_adjust && serial - wchan->pi_serial < 0)
			break;

		wchan->pi_serial = serial;
	}

	if (waiter != orig_waiter) {
		raw_spin_unlock(&waiter->lock);
		raw_spin_unlock(&wchan->lock);
		raw_spin_lock(&orig_wchan->lock);
		raw_spin_lock(&orig_waiter->lock);
	}

	return ret;
}

/*
 * Requeue a thread which changed priority in the wait channel it
 * pends on, then propagate the change down the PI chain.
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
	raw_spin_unlock(&thread->lock);

	if (!wchan)
		return;

	owner = wchan->owner;
	/* Careful: waitqueues have no owner. */
	if (owner) {
		double_thread_lock(thread, owner);
		wchan->requeue_wait(wchan, thread);
		raw_spin_unlock(&owner->lock);
		walk_pi_chain(wchan, thread, evl_pi_adjust);
	} else {
		raw_spin_lock(&thread->lock);
		wchan->requeue_wait(wchan, thread);
	}

	raw_spin_unlock(&thread->lock);
	evl_put_thread_wchan(wchan);
}

/* owner->lock + contender->lock held, irqs off */
static int inherit_thread_priority(struct evl_thread *owner,
				struct evl_thread *contender,
				struct evl_thread *originator)
{
	struct evl_wait_channel *wchan;
	int ret = 0;

	assert_hard_lock(&owner->lock);
	assert_hard_lock(&contender->lock);

	/* Apply the scheduling policy of @contender to @owner */
	evl_track_thread_policy(owner, contender);

	/*
	 * @owner may be blocked on a mutex, the reordering handler
	 * propagates the priority update along the PI chain if so.
	 */
	wchan = owner->wchan;
	if (wchan)
		ret = wchan->reorder_wait(owner, originator);

	return ret;
}

/* Perform a locking op. to non-origin mutex only. */
#define cond_lock_op(__op, __this_mutex, __origin_mutex)		\
	do {								\
		if ((__this_mutex) != (__origin_mutex))			\
			raw_spin_ ## __op(&(__this_mutex)->wchan.lock);	\
	} while (0)

/* origin->wchan.owner->lock + contender->lock + origin->wchan.lock held, irqs off */
static int old_adjust_boost(struct evl_mutex *origin,
			    struct evl_thread *contender,
			    struct evl_thread *originator)
{
	struct evl_thread *owner = origin->wchan.owner;
	struct evl_wait_channel *wchan;
	struct evl_mutex *mutex;
	int pprio, ret = 0;

	/*
	 * Adjust the priority of the owner of the @origin mutex as a
	 * result of a change in the wait list of the latter.  If no
	 * contender is specified on entry, we track the priority of
	 * the thread leading the wait list of the mutex leading the
	 * booster list of the specified owner.  In any case, we know
	 * a valid contender cannot go stale since it is part of the
	 * wait list of the mutex being considered, which is locked
	 * (i.e. mutex->wchan.lock must be held to read/update
	 * mutex->wchan.wait_list).
	 *
	 * @originator is the thread which initially triggered this PI
	 * walk originally targeting the @origin mutex - we use this
	 * information specifically for deadlock detection, making
	 * sure that @originator never appears in the dependency chain
	 * more than once.
	 *
	 * The safe locking order during the PI chain traversal is:
	 *
	 * ... (start of traversal) ...
	 * +-> owner->lock
	 * |     mutex->wchan.lock
	 * |
	 * |  owner := mutex->wchan.owner
	 * |  mutex := owner->wchan -+
	 * |                         |
	 * +-------------------------+
	 *
	 * At each stage, wchan->reorder_wait() fixes up the priority
	 * for @owner before walking deeper into the PI chain.
	 */
	assert_hard_lock(&owner->lock);
	assert_hard_lock(&origin->wchan.lock);

	/*
	 * CAUTION: we may have PI and PP-enabled mutexes among the
	 * boosters, considering the leader of mutex->wchan.wait_list
	 * is therefore NOT enough for determining the next boost
	 * priority, since PP is tracked lazily on acquisition, not
	 * immediately when a contention is detected. Check the head
	 * of the booster list instead.
	 */
	mutex = list_first_entry(&owner->boosters,
				struct evl_mutex, next_booster);
	cond_lock_op(lock, mutex, origin);
	if (mutex->wprio == owner->wprio) {
		cond_lock_op(unlock, mutex, origin);
		return 0;
	}

	if (mutex->flags & EVL_MUTEX_PP) {
		pprio = get_ceiling_value(mutex);
		/*
		 * Raise @owner priority to the ceiling value, this
		 * implicitly switches it to SCHED_FIFO if need be.
		 */
		evl_protect_thread_priority(owner, pprio);
		wchan = owner->wchan;
		if (wchan)
			ret = wchan->reorder_wait(owner, originator);
		cond_lock_op(unlock, mutex, origin);
	} else {
		if (EVL_WARN_ON(CORE, list_empty(&mutex->wchan.wait_list))) {
			cond_lock_op(unlock, mutex, origin);
			return 0;
		}
		if (contender == NULL) {
			contender = list_first_entry(&mutex->wchan.wait_list,
						struct evl_thread, wait_next);
			raw_spin_lock(&contender->lock);
			ret = inherit_thread_priority(owner, contender,
						originator);
			raw_spin_unlock(&contender->lock);
		} else {
			/* Otherwise @contender is already locked. */
			ret = inherit_thread_priority(owner, contender,
						originator);
		}
		cond_lock_op(unlock, mutex, origin);
	}

	return ret;
}

/* mutex->wchan.lock held, irqs off */
static void untrack_owner(struct evl_mutex *mutex)
{
	struct evl_thread *owner = mutex->wchan.owner;

	assert_hard_lock(&mutex->wchan.lock);

	if (owner) {
		raw_spin_lock(&owner->lock);
		list_del(&mutex->next_owned);
		mutex->wchan.owner = NULL;
		raw_spin_unlock(&owner->lock);
		evl_put_element(&owner->element);
	}
}

/* mutex->wchan.lock held, irqs off. */
static void track_owner(struct evl_mutex *mutex,
			struct evl_thread *owner)
{
	struct evl_thread *prev = mutex->wchan.owner;

	assert_hard_lock(&mutex->wchan.lock);

	if (EVL_WARN_ON_ONCE(CORE, prev != NULL))
		return;

	raw_spin_lock(&owner->lock);
	list_add(&mutex->next_owned, &owner->owned_mutexes);
	mutex->wchan.owner = owner;
	raw_spin_unlock(&owner->lock);
}

/* mutex->wchan.lock held, irqs off. */
static inline void ref_and_track_owner(struct evl_mutex *mutex,
				struct evl_thread *owner)
{
	assert_hard_lock(&mutex->wchan.lock);

	if (mutex->wchan.owner != owner) {
		evl_get_element(&owner->element);
		track_owner(mutex, owner);
	}
}

static inline int fast_mutex_is_claimed(fundle_t handle)
{
	return (handle & EVL_MUTEX_FLCLAIM) != 0;
}

static inline fundle_t mutex_fast_claim(fundle_t handle)
{
	return handle | EVL_MUTEX_FLCLAIM;
}

static inline fundle_t mutex_fast_ceil(fundle_t handle)
{
	return handle | EVL_MUTEX_FLCEIL;
}

/*
 * owner->lock held, irqs off (mutex needs not be locked, the ceiling
 * value is a static property).
 */
static void adjust_pp(struct evl_thread *owner, struct evl_mutex *mutex)
{
	/* Get the (SCHED_FIFO) priority (not the weighted one). */
	int pprio = get_ceiling_value(mutex);
	/*
	 * Set @owner priority to the ceiling value, this implicitly
	 * switches it to SCHED_FIFO if need be.
	 */
	evl_protect_thread_priority(owner, pprio);
}

/* mutex->wchan.lock + owner->lock held, irqs off. */
static void track_mutex_owner(struct evl_mutex *mutex, struct evl_thread *owner)
{
	struct evl_thread *prev = mutex->wchan.owner;

	assert_hard_lock(&mutex->wchan.lock);
	assert_hard_lock(&owner->lock);

	if (prev) {
		if (EVL_WARN_ON_ONCE(CORE, prev == owner))
			return;
		list_del(&mutex->next_owned);
		evl_put_element(&prev->element);
	}

	list_add(&mutex->next_owned, &owner->owned_mutexes);
	mutex->wchan.owner = owner;
}

/*
 * mutex.wchan->lock + owner->lock held, irqs off.
 *
 * Update the owner field of a mutex, boosting the owner if priority
 * protection is active on the latter. On return, this routne tells
 * the caller if evl_adjust_wait_priority() should be called for the
 * current owner as a result of a priority boost.
 *
 * NOTE: calling set_mutex_owner() per se is not enough to require a
 * finalization call to evl_schedule(), since the owner priority can
 * only be raised. However, calling evl_adjust_wait_priority() to
 * complete the priority update would require this, though.
 */
static bool set_mutex_owner(struct evl_mutex *mutex,
			struct evl_thread *owner)
{
	assert_hard_lock(&mutex->wchan.lock);
	assert_hard_lock(&owner->lock);

	/*
	 * Update the owner information, and apply priority protection
	 * for PP mutexes. We may only get there if owner is current,
	 * or blocked (and no way to wake it up under us since we hold
	 * owner->lock).
	 */
	if (mutex->wchan.owner != owner) {
		track_mutex_owner(mutex, owner);
		evl_get_element(&owner->element);
	}

	/*
	 * In case of a PP mutex: if the ceiling value is lower than
	 * the current effective priority, we must not adjust the
	 * latter.  BEWARE: not only this restriction is required to
	 * keep the PP logic right, but this is also a basic
	 * assumption made by all callers of
	 * evl_commit_monitor_ceiling() which won't check for any
	 * rescheduling opportunity upon return.
	 *
	 * However we do want the mutex to be linked to the booster
	 * list as long as its owner is boosted as a result of holding
	 * it, and T_BOOST must appear in the current thread status as
	 * well.
	 *
	 * This way, setparam() won't be allowed to decrease the
	 * current weighted priority below the ceiling value, until we
	 * eventually release this mutex.
	 */
	if (mutex->flags & EVL_MUTEX_PP) {
		int wprio = evl_calc_weighted_prio(&evl_sched_fifo,
						get_ceiling_value(mutex));
		mutex->wprio = wprio;
		list_add_priff(mutex, &owner->boosters, wprio, next_booster);
		raise_boost_flag(owner);
		mutex->flags |= EVL_MUTEX_CEILING;
		if (wprio > owner->wprio) {
			adjust_pp(owner, mutex);
			return true; /* evl_adjust_wait_priority() is needed. */
		}
	}

	return false;
}

static inline
fundle_t get_owner_handle(fundle_t ownerh, struct evl_mutex *mutex)
{
	/*
	 * On acquisition from kernel space, the fast lock handle
	 * should bear the FLCEIL bit for PP mutexes, so that userland
	 * takes the slow path on release, jumping to the kernel for
	 * dropping the ceiling priority boost.
	 */
	if (mutex->flags & EVL_MUTEX_PP)
		ownerh = mutex_fast_ceil(ownerh);

	return ownerh;
}

/*
 * Fast path: try to give mutex to the current thread. We hold no lock
 * on entry, irqs are on.
 */
static int fast_grab_mutex(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();
	fundle_t h, currh = fundle_of(curr);
	unsigned long flags;

	h = atomic_cmpxchg(mutex->fastlock, EVL_NO_HANDLE,
			get_owner_handle(currh, mutex));
	if (h != EVL_NO_HANDLE)
		return evl_get_index(h) == currh ? -EDEADLK : -EBUSY;

	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);
	raw_spin_lock(&curr->lock);
	/*
	 * Update the owner information into mutex so that it belongs
	 * to the current thread, applying any PP boost if
	 * applicable. Since the owner is current, there is no way it
	 * could be pending on a wait channel, so don't bother
	 * branching to evl_adjust_wait_priority().
	 */
	set_mutex_owner(mutex, curr);
	raw_spin_unlock(&curr->lock);
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);

	disable_inband_switch(curr);

	return 0;
}

/*
 * Adjust the priority of a thread owning some mutex(es) to the
 * required minimum for avoiding priority inversion.
 *
 * On entry: irqs off.
 */
static void adjust_owner_boost(struct evl_thread *owner)
{
	struct evl_thread *top_waiter;
	struct evl_mutex *mutex;

	raw_spin_lock(&owner->lock);

	EVL_WARN_ON_ONCE(CORE, list_empty(&owner->boosters));

	/*
	 * Fetch the mutex currently queuing the top waiter, among all
	 * mutexes being claimed/pp-active held by @owner.
	 */
	mutex = list_first_entry(&owner->boosters,
				struct evl_mutex, next_booster);

	if (mutex->flags & EVL_MUTEX_PP) {
		adjust_pp(owner, mutex);
		raw_spin_unlock(&owner->lock);
	} else {
		raw_spin_unlock(&owner->lock);
		/*
		 * Again, correct locking order is:
		 * wchan -> (owner, waiter) [by address]
		 */
		raw_spin_lock(&mutex->wchan.lock);
		if (EVL_WARN_ON(CORE, list_empty(&mutex->wchan.wait_list))) {
			raw_spin_unlock(&mutex->wchan.lock);
			return;
		}
		top_waiter = list_first_entry(&mutex->wchan.wait_list,
					struct evl_thread, wait_next);
		double_thread_lock(owner, top_waiter);
		/* Prevent spurious round-robin effect in runqueue. */
		if (top_waiter->wprio != owner->wprio)
			evl_track_thread_policy(owner, top_waiter);
		raw_spin_unlock(&top_waiter->lock);
		raw_spin_unlock(&owner->lock);
		raw_spin_unlock(&mutex->wchan.lock);
	}
}

/* mutex->wchan.lock held (temporarily dropped), irqs off */
static void drop_booster(struct evl_mutex *mutex)
{
	struct evl_thread *owner;

	assert_hard_lock(&mutex->wchan.lock);
	owner = mutex->wchan.owner;

	if (EVL_WARN_ON(CORE, !owner))
		return;

	/*
	 * Unlink the mutex from the list of boosters which cause a
	 * priority boost for @owner. If this list becomes empty as a
	 * result, then we know for sure that @owner does not hold any
	 * claimed PI or PP mutex anymore, therefore it should be
	 * deboosted.
	 *
	 * Caution: EVL_MUTEX_CLAIMED/EVL_MUTEX_CEILING may have been
	 * cleared earlier by the caller.
	 */
	evl_get_element(&owner->element);
	raw_spin_lock(&owner->lock);
	list_del(&mutex->next_booster);	/* owner->boosters */
	if (list_empty(&owner->boosters)) {
		raw_spin_lock(&owner->rq->lock);
		owner->state &= ~T_BOOST;
		raw_spin_unlock(&owner->rq->lock);
		evl_track_thread_policy(owner, owner); /* Reset to cprio. */
		raw_spin_unlock(&owner->lock);
		raw_spin_unlock(&mutex->wchan.lock);
	} else {
		/*
		 * The owner still holds PI/PP mutex(es) which may
		 * cause a priority boost: go adjust its priority to
		 * the new required minimum after dropping @mutex.
		 *
		 * Careful:
		 *
		 * - adjust_owner_boost() may attempt to lock the next
		 * mutex in line for boosting the owner; fortunately
		 * all callers allow us to release mutex->lock
		 * temporarily to avoid ABBA, since the mutex being
		 * dropped cannot go stale under us.
		 *
		 * - we may drop both wchan->owner->lock and
		 * wchan->lock temporarily at the same time, without
		 * @owner going stale, because we maintain a reference
		 * on it (evl_get_element()).
		 */
		raw_spin_unlock(&owner->lock);
		raw_spin_unlock(&mutex->wchan.lock);
		adjust_owner_boost(owner);
	}

	/*
	 * Since we dropped both owner->lock and wchan->lock, the
	 * owner might have been removed from a wait channel
	 * (thread->wchan) in the meantime, which would be detected
	 * during the wait priority adjustment.
	 *
	 * Because we cannot hold owner->lock across both the thread
	 * priority adjustment _and_ its wait priority adjustment
	 * (i.e. requeuing and PI chain walk), the following might
	 * happen:
	 *
	 * CPU0(thread A)                           CPU1(thread B)
	 * --------------                           --------------
	 *
	 * owner->wchan = &foo;
	 *
	 * drop_booster(mutex)
	 *    adjust_owner_boost(owner)[XX]
	 *                                          acquire(foo)
	 *    evl_adjust_wait_priority(owner)
	 *
	 *
	 * This is still correct, despite the owner was not requeued
	 * in its wait channel on CPU0 before some thread on CPU1
	 * managed to grab 'foo', because drop_booster() can never
	 * result in the owner being given a priority upgrade. At
	 * worst, drop_booster() would drop the mutex heading the
	 * owner's booster list, leading to a priority downgrade,
	 * otherwise the owner priority would not change.  IOW, CPU1
	 * could never steal a resource away unduly from the owner as
	 * a result of a delayed requeuing in the 'foo' wait channel,
	 * because either thread B now has higher priority than thread
	 * A in which case it should grab 'foo' first, or it still
	 * does not and 'foo' would be transferred to A anyway.
	 *
	 * Conversely, if ownership of 'foo' is transferred to thread
	 * A, it would be removed from the wait channel and boosted
	 * accordingly to the PI requirement as usual.
	 *
	 * Also note that even in case of a priority upgrade, the
	 * logic would still be right provided the current CPU does
	 * not evl_schedule() until evl_adjust_wait_priority() has
	 * run: any thread receiving ownership of the lock before
	 * thread A does would be boosted by
	 * evl_adjust_wait_priority() without unbounded delay,
	 * preventing priority inversion across CPUs. This property is
	 * used by callers of set_mutex_owner().
	 *
	 * [XX] evl_track_thread_policy() would reset the priority to
	 * thread->cprio, which can only be lower than the effective
	 * priority inherited during PI, so the reasoning about
	 * adjust_owner_boost() applies in this case too.
	 */
	evl_adjust_wait_priority(owner);
	evl_put_element(&owner->element);
	raw_spin_lock(&mutex->wchan.lock);
}

/*
 * Detect when current which is running out-of-band is about to sleep
 * on a mutex currently owned by another thread running in-band.
 *
 * mutex->wchan.lock held, irqs off, curr == this_evl_rq()->curr.
 */
static void detect_inband_owner(struct evl_mutex *mutex,
				struct evl_thread *curr)
{
	struct evl_thread *owner = mutex->wchan.owner;

	/*
	 * @curr == this_evl_rq()->curr so no need to grab
	 * @curr->lock.
	 */
	raw_spin_lock(&curr->rq->lock);

	if (curr->info & T_PIALERT) {
		curr->info &= ~T_PIALERT;
	} else if (owner->state & T_INBAND) {
		curr->info |= T_PIALERT;
		raw_spin_unlock(&curr->rq->lock);
		evl_notify_thread(curr, EVL_HMDIAG_LKDEPEND, evl_nil);
		return;
	}

	raw_spin_unlock(&curr->rq->lock);
}

/*
 * Detect when current is about to switch in-band while holding a
 * mutex which is causing an active PI or PP boost. Since such a
 * dependency on in-band would cause a priority inversion for the
 * waiter(s), the latter is sent a HM notification if T_WOLI is set.
 */
void evl_detect_boost_drop(void)
{
	struct evl_thread *curr = evl_current();
	struct evl_thread *waiter;
	struct evl_mutex *mutex;
	unsigned long flags;

	raw_spin_lock_irqsave(&curr->lock, flags);

	/*
	 * Iterate over waiters of each mutex we got boosted for due
	 * to PI/PP.
	 */
	for_each_evl_booster(mutex, curr) {
		raw_spin_lock(&mutex->wchan.lock);
		for_each_evl_mutex_waiter(waiter, mutex) {
			if (!(waiter->state & T_WOLI))
				continue;
			raw_spin_lock(&waiter->rq->lock);
			waiter->info |= T_PIALERT;
			raw_spin_unlock(&waiter->rq->lock);
			evl_notify_thread(waiter, EVL_HMDIAG_LKDEPEND, evl_nil);
		}
		raw_spin_unlock(&mutex->wchan.lock);
	}

	raw_spin_unlock_irqrestore(&curr->lock, flags);
}

void __evl_init_mutex(struct evl_mutex *mutex,
		struct evl_clock *clock,
		atomic_t *fastlock, u32 *ceiling_ref,
		const char *name)
{
	int type = ceiling_ref ? EVL_MUTEX_PP : EVL_MUTEX_PI;
	unsigned long flags __maybe_unused;

	mutex->fastlock = fastlock;
	atomic_set(fastlock, EVL_NO_HANDLE);
	mutex->flags = type & ~(EVL_MUTEX_CLAIMED|EVL_MUTEX_CEILING);
	mutex->wprio = -1;
	mutex->ceiling_ref = ceiling_ref;
	mutex->clock = clock;
	mutex->wchan.pi_serial = 0;
	mutex->wchan.owner = NULL;
	mutex->wchan.reorder_wait = evl_reorder_mutex_wait;
	mutex->wchan.requeue_wait = evl_requeue_mutex_wait;
	mutex->wchan.name = name;
	INIT_LIST_HEAD(&mutex->wchan.wait_list);
	raw_spin_lock_init(&mutex->wchan.lock);
#ifdef CONFIG_PROVE_LOCKING
	lockdep_register_key(&mutex->wchan.lock_key);
	lockdep_set_class_and_name(&mutex->wchan.lock, &mutex->wchan.lock_key, name);
	local_irq_save(flags);
	might_lock(&mutex->wchan.lock);
	local_irq_restore(flags);
#endif
}
EXPORT_SYMBOL_GPL(__evl_init_mutex);

/* mutex->wchan.lock held, irqs off */
static void flush_mutex_locked(struct evl_mutex *mutex, int reason)
{
	struct evl_thread *waiter, *tmp;

	assert_hard_lock(&mutex->wchan.lock);

	if (list_empty(&mutex->wchan.wait_list)) {
		EVL_WARN_ON(CORE, mutex->flags & EVL_MUTEX_CLAIMED);
	} else {
		list_for_each_entry_safe(waiter, tmp,
					&mutex->wchan.wait_list, wait_next) {
			list_del_init(&waiter->wait_next);
			evl_wakeup_thread(waiter, T_PEND, reason);
		}

		if (mutex->flags & EVL_MUTEX_CLAIMED) {
			mutex->flags &= ~EVL_MUTEX_CLAIMED;
			drop_booster(mutex);
		}
	}
}

void evl_flush_mutex(struct evl_mutex *mutex, int reason)
{
	unsigned long flags;

	trace_evl_mutex_flush(mutex);
	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);
	flush_mutex_locked(mutex, reason);
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
}

void evl_destroy_mutex(struct evl_mutex *mutex)
{
	unsigned long flags;

	trace_evl_mutex_destroy(mutex);
	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);
	flush_mutex_locked(mutex, T_RMID);
	untrack_owner(mutex);
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
	lockdep_unregister_key(&mutex->wchan.lock_key);
}
EXPORT_SYMBOL_GPL(evl_destroy_mutex);

int evl_trylock_mutex(struct evl_mutex *mutex)
{
	oob_context_only();

	trace_evl_mutex_trylock(mutex);

	return fast_grab_mutex(mutex);
}
EXPORT_SYMBOL_GPL(evl_trylock_mutex);

static int wait_mutex_schedule(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;
	int ret = 0, info;

	evl_schedule();

	info = evl_current()->info;
	if (info & T_RMID)
		return -EIDRM;

	if (info & (T_TIMEO|T_BREAK)) {
		raw_spin_lock_irqsave(&mutex->wchan.lock, flags);
		if (!list_empty(&curr->wait_next)) {
			list_del_init(&curr->wait_next);
			if (info & T_TIMEO)
				ret = -ETIMEDOUT;
			else if (info & T_BREAK)
				ret = -EINTR;
		}
		raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
	} else if (IS_ENABLED(CONFIG_EVL_DEBUG_CORE)) {
		bool empty;
		raw_spin_lock_irqsave(&mutex->wchan.lock, flags);
		empty = list_empty(&curr->wait_next);
		raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
		EVL_WARN_ON_ONCE(CORE, !empty);
	}

	return ret;
}

/* mutex->wchan.lock held (dropped temporarily), irqs off */
static void finish_mutex_wait(struct evl_mutex *mutex)
{
	struct evl_thread *owner, *top_waiter;

	/*
	 * Do all the necessary housekeeping chores to stop current
	 * from waiting on a mutex. Doing so may require to update a
	 * PI chain. Careful: we got there as a result of waking up
	 * from a mutex sleep, may have been granted that mutex, or we
	 * might have received an error while waiting. For this
	 * reason, this_evl_rq()->curr might not be owning the mutex.
	 */
	assert_hard_lock(&mutex->wchan.lock);

	/*
	 * Only a waiter leaving a PI chain triggers an update.
	 * NOTE: PP mutexes never bear the CLAIMED bit.
	 */
	if (!(mutex->flags & EVL_MUTEX_CLAIMED))
		return;

	/* Might be NULL or different from this_evl_rq()->curr. */
	owner = mutex->wchan.owner;
	if (!owner)
		return;		/* Dropped by __evl_unlock_mutex() */

	if (list_empty(&mutex->wchan.wait_list)) {
		/* No more waiters: drop this PI booster. */
		mutex->flags &= ~EVL_MUTEX_CLAIMED;
		drop_booster(mutex);
		return;
	}

	/*
	 * Reorder the booster queue since the wait list was updated,
	 * then set the priority of the current owner to the new
	 * minimum required to prevent inversion with the waiter
	 * leading the wait queue by priority order.
	 */
	top_waiter = list_first_entry(&mutex->wchan.wait_list,
				struct evl_thread, wait_next);

	double_thread_lock(owner, top_waiter);
	mutex->wprio = top_waiter->wprio;
	list_del(&mutex->next_booster);	/* owner->boosters */
	list_add_priff(mutex, &owner->boosters, wprio, next_booster);
	raw_spin_unlock(&top_waiter->lock);
	/* Guard against owner going stale as we drop owner->lock. */
	evl_get_element(&owner->element);
	raw_spin_unlock(&owner->lock);
	/* The caller allows us to drop mutex->wchan.lock temporarily. */
	raw_spin_unlock(&mutex->wchan.lock);
	adjust_owner_boost(owner);
	evl_adjust_wait_priority(owner);
	evl_put_element(&owner->element);
	raw_spin_lock(&mutex->wchan.lock);
}

/* mutex->wchan.lock held, irqs off. */
static void finish_slow_grab(struct evl_mutex *mutex, fundle_t currh)
{
	if (!list_empty(&mutex->wchan.wait_list)) /* any waiters? */
		currh = mutex_fast_claim(currh);

	atomic_set(mutex->fastlock, get_owner_handle(currh, mutex));
}

int evl_lock_mutex_timeout(struct evl_mutex *mutex, ktime_t timeout,
			enum evl_tmode timeout_mode)
{
	struct evl_thread *curr = evl_current(), *owner;
	atomic_t *lockp = mutex->fastlock;
	fundle_t currh, h, oldh;
	unsigned long flags;
	int ret;

	oob_context_only();

	currh = fundle_of(curr);
	trace_evl_mutex_lock(mutex);
redo:
	ret = fast_grab_mutex(mutex); /* Detects recursion. */
	if (likely(ret != -EBUSY))
		return ret;

	/*
	 * Well, no luck, mutex is locked and/or claimed already. This
	 * is the start of the slow path.
	 */
	ret = 0;

	/*
	 * As long as mutex->wchan.lock is held and FLCLAIM is set in the
	 * atomic handle, the thread who might currently own the mutex
	 * cannot release it, and no thread but the current one can
	 * acquire it, excluding the fast acquire/release logic in
	 * userland too.
	 */
	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);

	/*
	 * Set claimed bit.  In case it appears to be set already,
	 * re-read its state under mutex->wchan.lock so that we don't miss
	 * any change between the lock-less read and here. But also
	 * try to avoid cmpxchg where possible. Only if it appears not
	 * to be set, start with cmpxchg directly.
	 */
	h = atomic_read(lockp);
	if (h == EVL_NO_HANDLE) {
		raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
		goto redo;
	}

	if (fast_mutex_is_claimed(h)) {
		oldh = h;
		goto test_no_owner;
	}

	do {
		oldh = atomic_cmpxchg(lockp, h, mutex_fast_claim(h));
		if (likely(oldh == h))
			break;
	test_no_owner:
		if (oldh == EVL_NO_HANDLE) {
			/* Lock released from another cpu. */
			raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
			goto redo;
		}
		h = oldh;
	} while (!fast_mutex_is_claimed(h));

	/* Fetch the owner as userland sees it. */
	owner = evl_get_factory_element_by_fundle(&evl_thread_factory,
					evl_get_index(h),
					struct evl_thread);
	/*
	 * The tracked owner disappeared, clear the stale tracking
	 * data, then fail with -EOWNERDEAD. There is no point in
	 * trying to clean up that mess any further for userland, the
	 * logic protected by that lock is dead in the water anyway.
	 */
	if (owner == NULL) {
		untrack_owner(mutex);
		raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
		return -EOWNERDEAD;
	}

	/*
	 * If the owner information present in the mutex descriptor
	 * does not match the one available from the atomic handle, it
	 * means that such mutex was acquired using a fast locking
	 * operation from userland without involving the kernel,
	 * therefore we need to reconcile the in-kernel descriptor
	 * with the shared handle which has the accurate value. If
	 * both match though, evl_get_factory_element_by_fundle() got
	 * us a reference on @owner which the original call to
	 * track_mutex_owner() already obtained for that thread, so we
	 * need to drop it to rebalance the refcount.
	 *
	 * The consistency of this information is guaranteed, because
	 * we just raised the claim bit atomically for this contended
	 * lock, therefore userland would have to jump to the kernel
	 * for releasing it, instead of doing a fast unlock. Since we
	 * currently own the mutex lock, consistency wrt
	 * transfer_ownership() is guaranteed through serialization.
	 *
	 * CAUTION: in this particular case, the only assumption we
	 * may safely make is that *owner is valid and not current on
	 * this CPU.
	 */
	if (mutex->wchan.owner != owner) {
		raw_spin_lock(&owner->lock);
		track_mutex_owner(mutex, owner);
		raw_spin_unlock(&owner->lock);
	} else {
		evl_put_element(&owner->element);
	}

	if (unlikely(curr->state & T_WOLI))
		detect_inband_owner(mutex, curr);

	double_thread_lock(curr, owner);

	if (curr->wprio > owner->wprio) {
		/*
		 * Attempt to steal the mutex from a thread to which
		 * it was granted (transfer_ownership()), but which
		 * did not resume execution yet.
		 */
		if ((owner->info & T_WAKEN) && owner->wwake == &mutex->wchan) {
			raw_spin_lock(&owner->rq->lock);
			owner->info |= T_ROBBED;
			raw_spin_unlock(&owner->rq->lock);
			raw_spin_unlock(&owner->lock);
			  /*
			   * If a PP boost was applied, drop all locks
			   * before propagating a priority change to
			   * the wait channel, but do keep preemption
			   * off on the current CPU until done!
			   */
			if (set_mutex_owner(mutex, curr)) {
				raw_spin_unlock(&curr->lock);
				disable_inband_switch(curr);
				finish_slow_grab(mutex, currh);
				raw_spin_unlock(&mutex->wchan.lock);
				evl_adjust_wait_priority(curr);
				hard_local_irq_restore(flags);
				evl_schedule();
				return 0;
			}
			goto grab;
		}

		list_add_priff(curr, &mutex->wchan.wait_list, wprio, wait_next);

		if (mutex->flags & EVL_MUTEX_PI) {
			raise_boost_flag(owner);
			/*
			 * The CLAIMED bit is raised in a PI mutex
			 * which is causing a priority boost for its
			 * owner. PP mutexes never have this bit set.
			 */
			if (mutex->flags & EVL_MUTEX_CLAIMED)
				list_del(&mutex->next_booster); /* owner->boosters */
			else
				mutex->flags |= EVL_MUTEX_CLAIMED;

			mutex->wprio = curr->wprio;
			list_add_priff(mutex, &owner->boosters, wprio, next_booster);
			raw_spin_unlock(&owner->lock);
			ret = walk_pi_chain(&mutex->wchan, curr, evl_pi_adjust);
			raw_spin_unlock(&curr->lock);
		} else {
			raw_spin_unlock(&owner->lock);
			ret = walk_pi_chain(&mutex->wchan, curr, evl_pi_check);
			raw_spin_unlock(&curr->lock);
		}
	} else {
		list_add_priff(curr, &mutex->wchan.wait_list, wprio, wait_next);
		raw_spin_unlock(&owner->lock);
		ret = walk_pi_chain(&mutex->wchan, curr, evl_pi_check);
		raw_spin_unlock(&curr->lock);
	}

	/*
	 * Walking the PI chain is preemptible by other CPUs, so we
	 * might have been transferred ownership of wchan while busy
	 * progressing down this chain. Check for such event before
	 * deciding to sleep.
	 */
	if (likely(mutex->wchan.owner != curr && !ret)) {
		evl_sleep_on(timeout, timeout_mode, mutex->clock, &mutex->wchan);
		raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
		ret = wait_mutex_schedule(mutex);
		raw_spin_lock_irqsave(&mutex->wchan.lock, flags);
	}

	finish_mutex_wait(mutex);
	raw_spin_lock(&curr->lock);
	curr->wwake = NULL;
	raw_spin_lock(&curr->rq->lock);
	curr->info &= ~T_WAKEN;

	if (ret) {
		raw_spin_unlock(&curr->rq->lock);
		raw_spin_unlock(&curr->lock);
		EVL_WARN_ON_ONCE(CORE, curr == mutex->wchan.owner);
		goto out;
	}

	if (curr->info & T_ROBBED) {
		/*
		 * Kind of spurious wakeup: we were given the
		 * ownership but somebody stole it away from us while
		 * we were waiting for the CPU: we should redo waiting
		 * for the mutex, unless we know for sure it's too
		 * late.
		 */
		raw_spin_unlock(&curr->rq->lock);
		if (timeout_mode != EVL_REL ||
			timeout_infinite(timeout) ||
			evl_get_stopped_timer_delta(&curr->rtimer) != 0) {
			raw_spin_unlock(&curr->lock);
			raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
			goto redo;
		}
		ret = -ETIMEDOUT;
		raw_spin_unlock(&curr->lock);
		goto out;
	}

	raw_spin_unlock(&curr->rq->lock);
grab:
	raw_spin_unlock(&curr->lock);
	disable_inband_switch(curr);
	finish_slow_grab(mutex, currh);
out:
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
	evl_schedule();	  /* Because of evl_adjust_wait_priority(). */

	return ret;
}
EXPORT_SYMBOL_GPL(evl_lock_mutex_timeout);

/* mutex->wchan.lock, irqs off */
static bool transfer_ownership(struct evl_mutex *mutex)
{
	struct evl_thread *n_owner;
	bool ret = false;

	assert_hard_lock(&mutex->wchan.lock);

	/* Update the tracking list of the previous owner. */
	untrack_owner(mutex);

	if (list_empty(&mutex->wchan.wait_list)) {
clear:
		EVL_WARN_ON_ONCE(CORE, mutex->wchan.owner);
		return false;
	}

	/*
	 * FLCEIL may only be raised by the owner, or when the owner
	 * is blocked waiting for the mutex (ownership transfer). In
	 * addition, only the current owner of a mutex may release it,
	 * therefore we can't race while testing FLCEIL locklessly.
	 * All updates to FLCLAIM are covered by the mutex lock.
	 *
	 * Therefore, clearing the fastlock racelessly in this routine
	 * without leaking FLCEIL/FLCLAIM updates can be achieved
	 * locklessly.
	 *
	 * NOTE: FLCLAIM might be set although the wait list is empty,
	 * that's ok, we'll clear it along with the rest of the handle
	 * information. The opposite would be wrong though, so we have
	 * an assertion for this case.
	 */
	EVL_WARN_ON(CORE, !(atomic_read(mutex->fastlock) & EVL_MUTEX_FLCLAIM));

	n_owner = list_first_entry(&mutex->wchan.wait_list,
				struct evl_thread, wait_next);
next:
	raw_spin_lock(&n_owner->lock);

	/*
	 * A thread which has been forcibly unblocked while waiting
	 * for a mutex might still be linked to the wait list, until
	 * it resumes in wait_mutex_schedule() eventually. We can
	 * detect this rare case by testing the wait channel it pends
	 * on, since evl_wakeup_thread() clears it.
	 *
	 * CAUTION: a basic invariant is that a thread is removed from
	 * the wait list only when unblocked on a successful request
	 * (i.e. the awaited resource was granted), this way the
	 * opposite case can be detected by checking for
	 * !list_empty(&thread->wait_next) when resuming. So
	 * unfortunately, we have to keep that thread in the wait list
	 * not to break this assumption, until it resumes and figures
	 * out.
	 */
	if (!n_owner->wchan) {
		raw_spin_unlock(&n_owner->lock);
		n_owner = list_next_entry(n_owner, wait_next);
		if (&n_owner->wait_next == &mutex->wchan.wait_list)
			goto clear;
		goto next;
	}

	n_owner->wwake = &mutex->wchan;
	list_del_init(&n_owner->wait_next);
	/*
	 * Update mutex->wchan.owner, update the tracking data and
	 * apply any PP boost if need be.
	 */
	ret = set_mutex_owner(mutex, n_owner);
	/*
	 * Get a ref. on owner to allow for safe access after
	 * unlocking, caller will drop it.
	 */
	evl_get_element(&n_owner->element);

	raw_spin_unlock(&n_owner->lock);

	return ret;
}

void __evl_unlock_mutex(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current(), *n_owner;
	atomic_t *lockp = mutex->fastlock;
	unsigned long flags;
	fundle_t n_ownerh;
	bool adjust_wait;

	trace_evl_mutex_unlock(mutex);

	if (!enable_inband_switch(curr)) {
		WARN_ON_ONCE(1);
		return;
	}

	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);

	/*
	 * Priority boost for PP mutex is applied to the owner, unlike
	 * PI boost which is applied to waiters. Therefore, we have to
	 * adjust the boost for an owner releasing a PP mutex which
	 * got boosted (EVL_MUTEX_CEILING).
	 *
	 * Likewise, PI de-boosting must be applied at mutex release
	 * time, so that the correct priority applies when our caller
	 * reschedules the thread dropping it.
	 */
	if (mutex->flags & EVL_MUTEX_CEILING) {
		EVL_WARN_ON_ONCE(CORE, mutex->flags & EVL_MUTEX_CLAIMED);
		mutex->flags &= ~EVL_MUTEX_CEILING;
		drop_booster(mutex);
	} else if (mutex->flags & EVL_MUTEX_CLAIMED) {
		EVL_WARN_ON_ONCE(CORE, mutex->flags & EVL_MUTEX_CEILING);
		mutex->flags &= ~EVL_MUTEX_CLAIMED;
		drop_booster(mutex);
	}

	adjust_wait = transfer_ownership(mutex);

	n_owner = mutex->wchan.owner;
	if (n_owner) {
		/* Update the atomic handle shared with user-space. */
		n_ownerh = get_owner_handle(fundle_of(n_owner), mutex);
		if (!list_empty(&mutex->wchan.wait_list)) /* any waiters? */
			n_ownerh = mutex_fast_claim(n_ownerh);

		atomic_set(lockp, n_ownerh);
	} else {
		atomic_set(lockp, EVL_NO_HANDLE);
	}

	raw_spin_unlock(&mutex->wchan.lock);

	/*
	 * Ok, now let's consider the outcome of ownership transfer:
	 * if we have a new owner, first requeue it to the wait
	 * channel it pends on if any, then wake it up. Do the
	 * refcount dance on n_owner with transfer_ownership() to stay
	 * safe since we dropped both mutex->wchan.lock and we do not
	 * hold n_owner->lock.
	 *
	 * Caveat: keep local preemption disabled until adjustment
	 * took place, so that we cannot be preempted on the local CPU
	 * between the priority boost (set_mutex_owner()) and the wait
	 * channel requeuing (evl_adjust_wait_priority()).
	 */
	if (n_owner) {
		/*
		 * Adjust prior to calling evl_wakeup_thread(), since
		 * the latter clears n_owner->wchan.
		 */
		if (adjust_wait)
			evl_adjust_wait_priority(n_owner);

		evl_wakeup_thread(n_owner, T_PEND, T_WAKEN);
		evl_put_element(&n_owner->element);
	}

	hard_local_irq_restore(flags);
}

void evl_unlock_mutex(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();
	fundle_t currh = fundle_of(curr), h;

	oob_context_only();

	h = evl_get_index(atomic_read(mutex->fastlock));
	if (EVL_WARN_ON_ONCE(CORE, h != currh))
		return;

	__evl_unlock_mutex(mutex);
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_unlock_mutex);

/*
 * Release all mutexes the current (exiting) thread owns.
 */
void evl_drop_current_ownership(void)
{
	struct evl_thread *curr = evl_current();
	struct evl_mutex *mutex;
	unsigned long flags;

	raw_spin_lock_irqsave(&curr->lock, flags);

	while (!list_empty(&curr->owned_mutexes)) {
		mutex = list_first_entry(&curr->owned_mutexes,
					struct evl_mutex, next_owned);
		raw_spin_unlock_irqrestore(&curr->lock, flags);
		/* This removes @mutex from curr->owned_mutexes. */
		__evl_unlock_mutex(mutex);
		raw_spin_lock_irqsave(&curr->lock, flags);
	}

	raw_spin_unlock_irqrestore(&curr->lock, flags);
}

static inline struct evl_mutex *
wchan_to_mutex(struct evl_wait_channel *wchan)
{
	return container_of(wchan, struct evl_mutex, wchan);
}

/* waiter->lock held, irqs off */
int evl_reorder_mutex_wait(struct evl_thread *waiter,
			struct evl_thread *originator)
{
	struct evl_mutex *mutex = wchan_to_mutex(waiter->wchan);
	struct evl_thread *owner;
	int ret;

	assert_hard_lock(&waiter->lock);

	raw_spin_lock(&mutex->wchan.lock);

	owner = mutex->wchan.owner;
	if (owner == originator) {
		ret = -EDEADLK;
		goto out;
	}

	/*
	 * Update the position in the wait list of a thread waiting
	 * for a lock. This routine propagates the change throughout
	 * the PI chain if required.
	 */
	list_del(&waiter->wait_next);
	list_add_priff(waiter, &mutex->wchan.wait_list, wprio, wait_next);

	if (!(mutex->flags & EVL_MUTEX_PI)) {
		raw_spin_unlock(&mutex->wchan.lock);
		return 0;
	}

	/* Update the PI chain. */

	mutex->wprio = waiter->wprio;
	raw_spin_lock(&owner->lock);

	if (mutex->flags & EVL_MUTEX_CLAIMED) {
		list_del(&mutex->next_booster);
	} else {
		mutex->flags |= EVL_MUTEX_CLAIMED;
		raise_boost_flag(owner);
	}

	list_add_priff(mutex, &owner->boosters, wprio, next_booster);
	ret = old_adjust_boost(mutex, waiter, originator);
	raw_spin_unlock(&owner->lock);
out:
	raw_spin_unlock(&mutex->wchan.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_reorder_mutex_wait);

/* wchan->lock + wchan->owner->lock + waiter->lock held, irqs off. */
void evl_requeue_mutex_wait(struct evl_wait_channel *wchan,
			    struct evl_thread *waiter)
{
	struct evl_thread *owner = wchan->owner, *top_waiter;
	struct evl_mutex *mutex = wchan_to_mutex(wchan);

	assert_hard_lock(&wchan->lock);
	assert_hard_lock(&owner->lock);
	assert_hard_lock(&waiter->lock);

	/*
	 * Reorder the wait list according to the (updated) priority
	 * of the waiter.
	 */
	list_del(&waiter->wait_next);
	list_add_priff(waiter, &wchan->wait_list, wprio, wait_next);
	top_waiter = list_first_entry(&mutex->wchan.wait_list,
				struct evl_thread, wait_next);

	if (mutex->wprio != top_waiter->wprio) {
		mutex->wprio = top_waiter->wprio;
		if (mutex->flags & EVL_MUTEX_CLAIMED) {
			list_del(&mutex->next_booster);
			list_add_priff(mutex, &owner->boosters, wprio, next_booster);
		}
	}
}
EXPORT_SYMBOL_GPL(evl_requeue_mutex_wait);

void evl_commit_mutex_ceiling(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();
	atomic_t *lockp = mutex->fastlock;
	unsigned long flags;
	fundle_t oldh, h;

	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);

	/*
	 * For PP locks, userland does, in that order:
	 *
	 * -- LOCK
	 * 1. curr->u_window->pp_pending = fundle_of(mutex)
	 *    barrier();
	 * 2. atomic_cmpxchg(lockp, EVL_NO_HANDLE, fundle_of(curr));
	 *
	 * -- UNLOCK
	 * 1. atomic_cmpxchg(lockp, fundle_of(curr), EVL_NO_HANDLE); [unclaimed]
	 *    barrier();
	 * 2. curr->u_window->pp_pending = EVL_NO_HANDLE
	 *
	 * Make sure we have not been caught in a rescheduling in
	 * between those steps. If we did, then we won't be holding
	 * the lock as we schedule away, therefore no priority update
	 * must take place.
	 *
	 * We might be called multiple times for committing a lazy
	 * ceiling for the same mutex, e.g. if userland is preempted
	 * in the middle of a recursive locking sequence.
	 *
	 * This would stem from the fact that userland has to update
	 * ->pp_pending prior to trying to grab the lock atomically,
	 * at which point it can figure out whether a recursive
	 * locking happened. We get out of this trap by testing the
	 * EVL_MUTEX_CEILING flag.
	 */
	if (!evl_is_mutex_owner(lockp, fundle_of(curr)) ||
		(mutex->flags & EVL_MUTEX_CEILING))
		goto out;

	raw_spin_lock(&curr->lock);
	set_mutex_owner(mutex, curr);
	raw_spin_unlock(&curr->lock);
	/*
	 * Raise FLCEIL, which indicates a kernel entry will be
	 * required for releasing this resource.
	 */
	do {
		h = atomic_read(lockp);
		oldh = atomic_cmpxchg(lockp, h, mutex_fast_ceil(h));
	} while (oldh != h);
out:
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
}
