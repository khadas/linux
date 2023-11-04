/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001, 2019, 2022 Philippe Gerum  <rpm@xenomai.org>
 * Copyright (C) 2008, 2009 Jan Kiszka <jan.kiszka@siemens.com>.
 */

#include <linux/kernel.h>
#include <evl/timer.h>
#include <evl/thread.h>
#include <evl/mutex.h>
#include <evl/lock.h>
#include <uapi/evl/signal.h>
#include <trace/events/evl.h>

#define for_each_evl_mutex_waiter(__pos, __mutex)			\
	list_for_each_entry(__pos, &(__mutex)->wchan.wait_list, wait_next)

void __evl_init_mutex(struct evl_mutex *mutex,
		struct evl_clock *clock,
		atomic_t *fastlock, u32 *ceiling,
		const char *name,
		struct lock_class_key *lock_key)
{
	int type = ceiling ? EVL_MUTEX_PP : EVL_MUTEX_PI;

	mutex->fastlock = fastlock;
	atomic_set(fastlock, EVL_NO_HANDLE);
	mutex->flags = type;
	mutex->boost.wprio = -1;
	mutex->boost.sched_class = NULL;
	mutex->ceiling_addr = ceiling;
	mutex->clock = clock;
	mutex->wchan.owner = NULL;
	mutex->wchan.requeue_wait = evl_requeue_mutex_wait;
	mutex->wchan.name = name;
	INIT_LIST_HEAD(&mutex->wchan.wait_list);
	raw_spin_lock_init(&mutex->wchan.lock);
	lockdep_set_class_and_name(&mutex->wchan.lock, lock_key, name);
	might_hard_lock(&mutex->wchan.lock);
}
EXPORT_SYMBOL_GPL(__evl_init_mutex);

/* mutex->wchan.lock held, irqs off. */
static inline
void disable_inband_switch(struct evl_thread *curr, struct evl_mutex *mutex)
{
	/*
	 * Track mutex locking depth: 1) to prevent weak threads from
	 * being switched back to in-band context on return from OOB
	 * syscalls, 2) when locking consistency is being checked.
	 */
	if (unlikely(curr->state & (EVL_T_WEAK|EVL_T_WOLI))) {
		atomic_inc(&curr->held_mutex_count);
		mutex->flags |= EVL_MUTEX_COUNTED;
	}
}

/* mutex->wchan.lock held, irqs off. */
static inline
void enable_inband_switch(struct evl_thread *curr, struct evl_mutex *mutex)
{
	if (unlikely(mutex->flags & EVL_MUTEX_COUNTED)) {
		mutex->flags &= ~EVL_MUTEX_COUNTED;
		if (atomic_dec_return(&curr->held_mutex_count) < 0) {
			atomic_set(&curr->held_mutex_count, 0);
			EVL_WARN_ON(CORE, 1);
		}
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

/* mutex->wchan.lock + owner->lock held, irqs off. */
static void track_mutex_owner(struct evl_mutex *mutex, struct evl_thread *owner)
{
	struct evl_thread *prev = mutex->wchan.owner;

	assert_hard_lock(&mutex->wchan.lock);
	assert_hard_lock(&owner->lock);

	if (prev) {
		if (EVL_WARN_ON(CORE, prev == owner))
			return;
		list_del(&mutex->next_owned);
		evl_put_element(&prev->element);
	}

	list_add(&mutex->next_owned, &owner->owned_mutexes);
	mutex->wchan.owner = owner;
}

/* mutex->wchan.lock held, irqs off */
static void untrack_mutex_owner(struct evl_mutex *mutex)
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

/*
 * mutex.wchan->lock, irqs off.
 *
 * Make the owner field of a mutex refer to the current thread,
 * applying the PP boost to the latter if need be.
 */
static void set_mutex_owner(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();

	assert_hard_lock(&mutex->wchan.lock);

	raw_spin_lock(&curr->lock);

	if (mutex->wchan.owner != curr) {
		track_mutex_owner(mutex, curr);
		evl_get_element(&curr->element);
	}

	/*
	 * In case of a PP mutex, adjustment takes place only upon
	 * priority increase.  BEWARE: not only this restriction is
	 * required to keep the PP logic right, but this is also a
	 * basic assumption made by all callers of
	 * evl_commit_monitor_ceiling() which won't check for any
	 * rescheduling opportunity upon return. However, we still do
	 * want the mutex to be linked to the new owner's booster
	 * list.
	 *
	 * Since the owner is current, there is no way it could be
	 * pending on a wait channel, so don't bother branching to
	 * evl_adjust_wait_priority().
	 */
	if (mutex->flags & EVL_MUTEX_PP) {
		int wprio = evl_calc_weighted_prio(&evl_sched_fifo,
						evl_ceiling_priority(mutex));
		mutex->boost.wprio = wprio;
		mutex->boost.sched_class = NULL; /* Unused for PP. */
		list_add_priff(mutex, &curr->boosters, boost.wprio, next_booster);
		mutex->flags |= EVL_MUTEX_CEILING;
		if (wprio > curr->wprio)
			evl_adjust_thread_boost(curr);
	}

	raw_spin_unlock(&curr->lock);
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
static int fast_grab_mutex(struct evl_mutex *mutex, fundle_t *oldh)
{
	struct evl_thread *curr = evl_current();
	fundle_t currh = fundle_of(curr), newh;
	unsigned long flags;

	/*
	 * Try grabbing the mutex via CAS. Upon success, we will be
	 * tracking it, and other threads might be waiting for us to
	 * release it as well. So we have to raise FLCLAIM in the
	 * atomic handle, so that user-space does not succeed in fast
	 * unlocking but jumps back to the kernel instead, allowing us
	 * to do the required housekeeping chores upon unlock.
	 */
	newh = mutex_fast_claim(get_owner_handle(currh, mutex));
	*oldh = atomic_cmpxchg(mutex->fastlock, EVL_NO_HANDLE, newh);
	if (*oldh != EVL_NO_HANDLE)
		return evl_get_index(*oldh) == currh ? -EDEADLK : -EBUSY;

	/* Success, we have ownership now. */
	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);

	/*
	 * Update the owner information for mutex so that it belongs
	 * to the current thread, applying any PP boost if applicable.
	 */
	set_mutex_owner(mutex);

	disable_inband_switch(curr, mutex);

	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);

	return 0;
}

/* mutex->wchan.lock held (temporarily dropped), irqs off */
static void drop_booster(struct evl_mutex *mutex, bool release)
{
	struct evl_thread *owner;
	bool adjusted;

	assert_hard_lock(&mutex->wchan.lock);
	owner = mutex->wchan.owner;

	if (EVL_WARN_ON(CORE, !owner))
		return;

	/*
	 * Unlink the mutex from the list of boosters held by @owner,
	 * adjusting its priority according to the boosters it still
	 * holds.
	 */
	evl_get_element(&owner->element);

	if (release)	/* Drop upon release sequence? */
		untrack_mutex_owner(mutex);

	raw_spin_lock(&owner->lock);
	list_del(&mutex->next_booster);	/* owner->boosters */
	mutex->boost.sched_class = NULL;
	raw_spin_unlock(&mutex->wchan.lock);
	adjusted = evl_adjust_thread_boost(owner);
	raw_spin_unlock(&owner->lock);

	if (adjusted)
		evl_adjust_wait_priority(owner);

	evl_put_element(&owner->element);
	raw_spin_lock(&mutex->wchan.lock);
}

/*
 * Detect when current is about to switch in-band while owning a
 * mutex, which is plain wrong since this would create a priority
 * inversion. EVL_T_WOLI is set for current.
 */
void evl_check_no_mutex(void)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;
	bool notify;

	raw_spin_lock_irqsave(&curr->lock, flags);
	if (!(curr->info & EVL_T_PIALERT)) {
		notify = !list_empty(&curr->owned_mutexes);
		if (notify) {
			raw_spin_lock(&curr->rq->lock);
			curr->info |= EVL_T_PIALERT;
			raw_spin_unlock(&curr->rq->lock);
		}
	}
	raw_spin_unlock_irqrestore(&curr->lock, flags);

	if (notify)
		evl_notify_thread(curr, EVL_HMDIAG_LKDEPEND, evl_nil);
}

/* mutex->wchan.lock held, irqs off */
static void flush_mutex_locked(struct evl_mutex *mutex, int reason)
{
	struct evl_thread *waiter, *tmp;

	assert_hard_lock(&mutex->wchan.lock);

	if (list_empty(&mutex->wchan.wait_list)) {
		EVL_WARN_ON(CORE, mutex->flags & EVL_MUTEX_PIBOOST);
	} else {
		list_for_each_entry_safe(waiter, tmp,
					&mutex->wchan.wait_list, wait_next) {
			list_del_init(&waiter->wait_next);
			evl_wakeup_thread(waiter, EVL_T_PEND, reason);
		}

		if (mutex->flags & EVL_MUTEX_PIBOOST) {
			mutex->flags &= ~EVL_MUTEX_PIBOOST;
			drop_booster(mutex, false);
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
	evl_schedule();
}

void evl_destroy_mutex(struct evl_mutex *mutex)
{
	unsigned long flags;

	trace_evl_mutex_destroy(mutex);
	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);
	flush_mutex_locked(mutex, EVL_T_RMID);
	untrack_mutex_owner(mutex);
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_destroy_mutex);

int evl_trylock_mutex(struct evl_mutex *mutex)
{
	fundle_t oldh;

	oob_context_only();

	trace_evl_mutex_trylock(mutex);

	return fast_grab_mutex(mutex, &oldh);
}
EXPORT_SYMBOL_GPL(evl_trylock_mutex);

/*
 * Undo the effect of a previous lock chain walk due to an aborted
 * wait. The mutex state might have changed under us since we dropped
 * mutex->wchan.lock during the walk. If we still have an owner at
 * this point, and the mutex is still part of its booster list, then
 * we need to consider two cases:
 *
 * - the mutex still has waiters, in which case we need to adjust the
 * boost value to the top waiter priority.
 *
 * - nobody waits for this mutex anymore, so we may drop it from the
 * owner's booster list, adjusting the boost value accordingly too.
 *
 * mutex->wchan.lock held (dropped at exit), irqs off
 *
 * NOTE: the caller MUST reschedule.
 */
static void undo_pi_walk(struct evl_mutex *mutex)
{
	struct evl_thread *owner = mutex->wchan.owner;

	if (owner && mutex->flags & EVL_MUTEX_PIBOOST) {
		if (list_empty(&mutex->wchan.wait_list)) {
			mutex->flags &= ~EVL_MUTEX_PIBOOST;
			drop_booster(mutex, false);
		} else {
			raw_spin_lock(&owner->lock);
			raw_spin_unlock(&mutex->wchan.lock);
			evl_adjust_thread_boost(owner);
			raw_spin_unlock(&owner->lock);
			return;
		}
	}

	raw_spin_unlock(&mutex->wchan.lock);
}

/* mutex->wchan.lock + mutex->wchan.owner->lock + waiter->lock held, irqs off. */
static void enqueue_booster(struct evl_mutex *mutex, struct evl_thread *waiter)
{
	struct evl_thread *owner = mutex->wchan.owner;

	assert_hard_lock(&mutex->wchan.lock);
	assert_hard_lock(&owner->lock);
	assert_hard_lock(&waiter->lock);

	mutex->boost.wprio = waiter->wprio;
	mutex->boost.sched_class = waiter->sched_class;
	raw_spin_lock(&waiter->rq->lock);
	evl_get_schedparam(waiter, &mutex->boost.param);
	raw_spin_unlock(&waiter->rq->lock);
	list_add_priff(mutex, &owner->boosters, boost.wprio, next_booster);
}

int evl_lock_mutex_timeout(struct evl_mutex *mutex, ktime_t timeout,
			enum evl_tmode timeout_mode)
{
	struct evl_thread *curr = evl_current(), *owner;
	atomic_t *lockp = mutex->fastlock;
	fundle_t currh, h, oldh;
	unsigned long flags;
	bool check_dep_only;
	int ret;

	oob_context_only();

	currh = fundle_of(curr);
	trace_evl_mutex_lock(mutex);
retry:
	ret = fast_grab_mutex(mutex, &h); /* This detects recursion. */
	if (likely(ret != -EBUSY))
		return ret;

	/*
	 * Well, no luck, mutex is locked and/or claimed already. This
	 * is the start of the slow path.
	 */
	ret = 0;

	/*
	 * As long as mutex->wchan.lock is held and FLCLAIM is set in
	 * the atomic handle, the thread who might currently own the
	 * mutex cannot release it directly via the fast release logic
	 * from user-space, and will therefore have to serialize with
	 * the current thread in kernel space for doing so.
	 */
	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);

	/*
	 * Set claimed bit.  In case it appears to be set already,
	 * re-read its state under mutex->wchan.lock so that we don't
	 * miss any change between the fast grab attempt and this
	 * point. But also try to avoid cmpxchg where possible. Only
	 * if it appears not to be set, start with cmpxchg directly.
	 */
	if (fast_mutex_is_claimed(h)) {
		oldh = atomic_read(lockp);
		goto check_if_free;
	}

	do {
		oldh = atomic_cmpxchg(lockp, h, mutex_fast_claim(h));
		if (likely(oldh == h))
			break;
	check_if_free:
		if (oldh == EVL_NO_HANDLE) {
			/* Lock released from another CPU. */
			raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
			goto retry;
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
		untrack_mutex_owner(mutex);
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
	 * us another reference on @owner like set_mutex_owner()
	 * already obtained earlier for that thread, so we need to
	 * drop it to rebalance the refcount.
	 *
	 * The consistency of this information is guaranteed, because
	 * we just raised the claim bit atomically for this contended
	 * lock, therefore userland would have to jump to the kernel
	 * for releasing it, instead of doing a fast unlock. Since we
	 * currently hold mutex->wchan.lock, consistency wrt
	 * __evl_unlock_mutex() is guaranteed through explicit
	 * serialization.
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

	evl_double_thread_lock(curr, owner);

	check_dep_only = true;	/* Only check for cyclic dependency. */
	if (mutex->flags & EVL_MUTEX_PI && curr->wprio > owner->wprio) {
		if (mutex->flags & EVL_MUTEX_PIBOOST)
			list_del(&mutex->next_booster); /* owner->boosters */
		else
			mutex->flags |= EVL_MUTEX_PIBOOST;

		enqueue_booster(mutex, curr);
		check_dep_only = false; /* Adjust priorities too. */
	}

	/* Walking the chain drops owner->lock. */
	ret = evl_walk_lock_chain(&mutex->wchan, curr, check_dep_only);
	assert_hard_lock(&mutex->wchan.lock);
	assert_hard_lock(&curr->lock);
	if (ret) {
		raw_spin_unlock(&curr->lock);
		goto fail;
	}

	/*
	 * If the latest owner of this mutex dropped it while we were
	 * busy walking the lock chain, we should not wait but retry
	 * acquiring it from the beginning instead. Otherwise, let's
	 * wait for __evl_unlock_mutex() to notify us of the release.
	 */
	owner = mutex->wchan.owner;
	if (unlikely(!owner)) {
		/*
		 * Since the mutex has no owner, there is no way that
		 * it could be part of anyone's booster list anymore.
		 */
		raw_spin_unlock(&curr->lock);
		raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
		goto retry;
	}

	raw_spin_unlock(&curr->lock);
	list_add_priff(curr, &mutex->wchan.wait_list, wprio, wait_next);
	evl_sleep_on(timeout, timeout_mode, mutex->clock, &mutex->wchan);
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
	ret = __evl_wait_schedule(&mutex->wchan);
	/* If something went wrong while sleeping, bail out. */
	if (ret) {
		raw_spin_lock_irqsave(&mutex->wchan.lock, flags);
		goto fail;
	}

	/*
	 * Otherwise, this means __evl_unlock_mutex() unblocked us, so
	 * we just need to retry grabbing the mutex. Keep in mind that
	 * __evl_unlock_mutex() dropped the mutex from the previous
	 * owner's booster list before unblocking us, so we do not
	 * have to revert the effects of our lock chain walk.
	 */
	evl_schedule();
	goto retry;
fail:
	/*
	 * On error, we may have done a partial boost of the lock
	 * chain, so we need to carefully revert this, then reschedule
	 * to apply any priority change.
	 */
	undo_pi_walk(mutex);
	hard_local_irq_enable();
	evl_schedule();

	return ret;
}
EXPORT_SYMBOL_GPL(evl_lock_mutex_timeout);

/* wchan->lock held, irqs off. */
static void wakeup_and_release(struct evl_mutex *mutex)
{
	struct evl_thread *top_waiter;

	/*
	 * Allow the top waiter in line to retry acquiring the mutex.
	 */
	if (!list_empty(&mutex->wchan.wait_list)) {
		top_waiter = list_get_entry_init(&mutex->wchan.wait_list,
						struct evl_thread, wait_next);
		evl_wakeup_thread(top_waiter, EVL_T_PEND, 0);
	}

	/*
	 * Make the change visible to everyone including user-space
	 * once the kernel state is in sync.
	 */
	atomic_set(mutex->fastlock, EVL_NO_HANDLE);
}

void __evl_unlock_mutex(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;

	trace_evl_mutex_unlock(mutex);

	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);

	/*
	 * We might have acquired the lock earlier from user-space via
	 * the fast procedure, releasing it now from kernel space -
	 * this pattern typically happens with the event monitor,
	 * e.g. fast_lock(x) -> signal(event) -> slow_unlock(x). In
	 * this case, the owner field into the wait channel might be
	 * unset if the lock was uncontended, although our caller has
	 * checked that the atomic handle does match current's
	 * fundle. If so, skip the PI/PP deboosting, no boost can be
	 * in effect for such lock.
	 */
	if (mutex->wchan.owner == NULL) {
		wakeup_and_release(mutex);
		goto out;
	}

	if (EVL_WARN_ON(CORE, mutex->wchan.owner != curr))
		goto out;

	enable_inband_switch(curr, mutex);

	/*
	 * The released mutex can no longer be a booster for the
	 * current thread, remove it from its booster list if queued
	 * there.
	 */
	if (mutex->flags & EVL_MUTEX_CEILING) {
		EVL_WARN_ON(CORE, mutex->flags & EVL_MUTEX_PIBOOST);
		mutex->flags &= ~EVL_MUTEX_CEILING;
		wakeup_and_release(mutex);
		drop_booster(mutex, true);
	} else if (mutex->flags & EVL_MUTEX_PIBOOST) {
		EVL_WARN_ON(CORE, mutex->flags & EVL_MUTEX_CEILING);
		mutex->flags &= ~EVL_MUTEX_PIBOOST;
		wakeup_and_release(mutex);
		drop_booster(mutex, true);
	} else {
		/* Clear the owner information only. */
		untrack_mutex_owner(mutex);
		wakeup_and_release(mutex);
	}

out:
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);

	/*
	 * Check for spuriously lingering PI/PP boosts. This check
	 * happens here and in the rescheduling procedure too in order
	 * to cover cases which would -wrongly- skip the in-kernel
	 * unlock path.
	 */
	if (IS_ENABLED(CONFIG_EVL_DEBUG_CORE)) {
		bool bad;
		raw_spin_lock_irqsave(&curr->lock, flags);
		bad = curr->state & EVL_T_BOOST && list_empty(&curr->boosters);
		raw_spin_unlock_irqrestore(&curr->lock, flags);
		EVL_WARN_ON_ONCE(CORE, bad);
	}
}

void evl_unlock_mutex(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();
	fundle_t currh = fundle_of(curr), h;

	oob_context_only();

	h = evl_get_index(atomic_read(mutex->fastlock));
	if (h != currh) {
		if (curr->state & EVL_T_USER) {
			if (curr->state & EVL_T_WOLI)
				evl_notify_thread(curr, EVL_HMDIAG_LKIMBALANCE, evl_nil);
		} else {
			EVL_WARN_ON(CORE, 1);
		}
		return;
	}

	__evl_unlock_mutex(mutex);
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_unlock_mutex);

/*
 * Release all mutexes the current (exiting) thread owns.
 *
 * No lock held, irqs on.
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

	evl_schedule();
}

static inline struct evl_mutex *
wchan_to_mutex(struct evl_wait_channel *wchan)
{
	return container_of(wchan, struct evl_mutex, wchan);
}

/* wchan->lock + waiter->lock held, irqs off. */
bool evl_requeue_mutex_wait(struct evl_wait_channel *wchan,
			    struct evl_thread *waiter)
{
	struct evl_thread *owner = wchan->owner, *top_waiter;
	struct evl_mutex *mutex = wchan_to_mutex(wchan);
	bool ret = true;

	assert_hard_lock(&wchan->lock);
	assert_hard_lock(&waiter->lock);

	/*
	 * Make sure the waiter did not abort wait on wchan while
	 * unlocked; this might happen since evl_wakeup_thread() would
	 * be allowed to run, clearing waiter->wchan when called upon
	 * a timeout/break/rmid condition, before such thread resumes
	 * and dequeues itself from wchan eventually (in
	 * __evl_wait_schedule()).
	 *
	 * We have been holding wchan->lock across the unlocked
	 * section, so we know that wchan did not go stale though.
	 */
	if (waiter->wchan != wchan)
		return false;

	/*
	 * __evl_unlock_mutex() may run concurrently to us, dropping
	 * ownership. Make sure we still have an owner.
	 */
	if (owner == NULL) {
		list_del(&waiter->wait_next);
		list_add_priff(waiter, &wchan->wait_list, wprio, wait_next);
		return false;
	}

	/*
	 * To prevent ABBA, we may drop the waiter lock prior to
	 * reacquiring it in a double locking sequence along with
	 * wchan->owner->lock which we need to requeue the booster.
	 */
	raw_spin_unlock(&waiter->lock);
	evl_double_thread_lock(waiter, owner);

	/*
	 * Reorder the wait list according to the (updated) priority
	 * of the waiter, then requeue the booster accordingly.
	 */
	list_del(&waiter->wait_next);
	list_add_priff(waiter, &wchan->wait_list, wprio, wait_next);
	if (mutex->flags & EVL_MUTEX_PIBOOST) {
		top_waiter = list_first_entry(&mutex->wchan.wait_list,
					struct evl_thread, wait_next);
		if (mutex->boost.wprio != top_waiter->wprio) {
			/* Requeue booster at the right place. */
			list_del(&mutex->next_booster);
			enqueue_booster(mutex, top_waiter);
		}
	}

	raw_spin_unlock(&owner->lock);

	return ret;
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

	set_mutex_owner(mutex);
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
