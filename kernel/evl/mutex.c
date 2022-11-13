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

#define for_each_evl_mutex_waiter(__pos, __mutex)			\
	list_for_each_entry(__pos, &(__mutex)->wchan.wait_list, wait_next)

static inline int get_ceiling_value(struct evl_mutex *mutex)
{
	/*
	 * The ceiling priority value is stored in user-writable
	 * memory, make sure to constrain it within valid bounds for
	 * evl_sched_fifo before using it.
	 */
	return clamp(*mutex->ceiling_ref, 1U, (u32)EVL_FIFO_MAX_PRIO);
}

/* mutex->wchan.lock held, irqs off. */
static inline
void disable_inband_switch(struct evl_thread *curr, struct evl_mutex *mutex)
{
	/*
	 * Track mutex locking depth: 1) to prevent weak threads from
	 * being switched back to in-band context on return from OOB
	 * syscalls, 2) when locking consistency is being checked.
	 */
	if (unlikely(curr->state & (T_WEAK|T_WOLI))) {
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
	 * it.
	 */
	if (mutex->flags & EVL_MUTEX_PP) {
		int wprio = evl_calc_weighted_prio(&evl_sched_fifo,
						get_ceiling_value(mutex));
		mutex->wprio = wprio;
		list_add_priff(mutex, &owner->boosters, wprio, next_booster);
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
	 * to the current thread, applying any PP boost if
	 * applicable. Since the owner is current, there is no way it
	 * could be pending on a wait channel, so don't bother
	 * branching to evl_adjust_wait_priority().
	 */
	raw_spin_lock(&curr->lock);
	set_mutex_owner(mutex, curr);
	raw_spin_unlock(&curr->lock);

	disable_inband_switch(curr, mutex);

	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);

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

	EVL_WARN_ON(CORE, list_empty(&owner->boosters));

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
		evl_double_thread_lock(owner, top_waiter);
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
	enum evl_walk_mode mode;

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
	 */
	evl_get_element(&owner->element);
	raw_spin_lock(&owner->lock);
	list_del(&mutex->next_booster);	/* owner->boosters */
	if (list_empty(&owner->boosters)) {
		evl_track_thread_policy(owner, owner); /* Reset to base priority. */
		raw_spin_unlock(&owner->lock);
		raw_spin_unlock(&mutex->wchan.lock);
		mode = evl_pi_reset;
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
		 * all callers allow us to release mutex->wchan.lock
		 * temporarily to avoid ABBA, since this mutex cannot
		 * go stale under us.
		 *
		 * - we may drop both mutex.wchan->owner->lock and
		 * mutex->wchan.lock temporarily at the same time,
		 * without @owner going stale, because we maintain a
		 * reference on it (evl_get_element()).
		 */
		raw_spin_unlock(&owner->lock);
		raw_spin_unlock(&mutex->wchan.lock);
		adjust_owner_boost(owner);
		mode = evl_pi_adjust;
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
	 * does not and 'foo' would be granted to A anyway.
	 *
	 * Conversely, if ownership of 'foo' is granted to thread A,
	 * it would be removed from the wait channel and boosted
	 * accordingly to the PI requirement.
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
	evl_adjust_wait_priority(owner, mode);
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
	mutex->flags = type;
	mutex->wprio = -1;
	mutex->ceiling_ref = ceiling_ref;
	mutex->clock = clock;
	mutex->wchan.pi_serial = 0;
	mutex->wchan.owner = NULL;
	mutex->wchan.requeue_wait = evl_requeue_mutex_wait;
	mutex->wchan.name = name;
	INIT_LIST_HEAD(&mutex->wchan.wait_list);
	raw_spin_lock_init(&mutex->wchan.lock);
#ifdef CONFIG_LOCKDEP
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
		EVL_WARN_ON(CORE, mutex->flags & EVL_MUTEX_PIBOOST);
	} else {
		list_for_each_entry_safe(waiter, tmp,
					&mutex->wchan.wait_list, wait_next) {
			list_del_init(&waiter->wait_next);
			evl_wakeup_thread(waiter, T_PEND, reason);
		}

		if (mutex->flags & EVL_MUTEX_PIBOOST) {
			mutex->flags &= ~EVL_MUTEX_PIBOOST;
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
	evl_schedule();
}

void evl_destroy_mutex(struct evl_mutex *mutex)
{
	unsigned long flags;

	trace_evl_mutex_destroy(mutex);
	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);
	flush_mutex_locked(mutex, T_RMID);
	untrack_mutex_owner(mutex);
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
	evl_schedule();
	lockdep_unregister_key(&mutex->wchan.lock_key);
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
 * Undo a PI chain walk due to a locking abort. The mutex state might
 * have changed under us since we dropped mutex->wchan.lock during the
 * walk. If we still have an owner at this point, and the mutex is
 * still part its booster list, then we need to consider two cases:
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
	struct evl_thread *owner = mutex->wchan.owner, *top_waiter;

	if (owner && mutex->flags & EVL_MUTEX_PIBOOST) {
		if (list_empty(&mutex->wchan.wait_list)) {
			mutex->flags &= ~EVL_MUTEX_PIBOOST;
			drop_booster(mutex);
		} else {
			top_waiter = list_first_entry(&mutex->wchan.wait_list,
						struct evl_thread, wait_next);
			if (mutex->wprio != top_waiter->wprio) {
				mutex->wprio = top_waiter->wprio;
				evl_get_element(&owner->element);
				raw_spin_unlock(&mutex->wchan.lock);
				adjust_owner_boost(owner);
				evl_put_element(&owner->element);
				return;
			}
		}
	}

	raw_spin_unlock(&mutex->wchan.lock);
}

int evl_lock_mutex_timeout(struct evl_mutex *mutex, ktime_t timeout,
			enum evl_tmode timeout_mode)
{
	struct evl_thread *curr = evl_current(), *owner;
	atomic_t *lockp = mutex->fastlock;
	enum evl_walk_mode walk_mode;
	fundle_t currh, h, oldh;
	unsigned long flags;
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
	 * us a reference on @owner which the original call to
	 * track_mutex_owner() already obtained for that thread, so we
	 * need to drop it to rebalance the refcount.
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

	if (unlikely(curr->state & T_WOLI))
		detect_inband_owner(mutex, curr);

	evl_double_thread_lock(curr, owner);

	walk_mode = evl_pi_check;
	if (mutex->flags & EVL_MUTEX_PI && curr->wprio > owner->wprio) {
		if (mutex->flags & EVL_MUTEX_PIBOOST)
			list_del(&mutex->next_booster); /* owner->boosters */
		else
			mutex->flags |= EVL_MUTEX_PIBOOST;

		mutex->wprio = curr->wprio;
		list_add_priff(mutex, &owner->boosters, wprio, next_booster);
		walk_mode = evl_pi_adjust;
	}

	raw_spin_unlock(&owner->lock);

	ret = evl_walk_pi_chain(&mutex->wchan, curr, walk_mode);
	assert_hard_lock(&mutex->wchan.lock);
	assert_hard_lock(&curr->lock);
	if (ret) {
		raw_spin_unlock(&curr->lock);
		goto fail;
	}

	/*
	 * If the latest owner of this mutex dropped it while we were
	 * busy walking the PI chain, we should not wait but retry
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
	 * have to revert the effects of our PI walk.
	 */
	evl_schedule();
	goto retry;
fail:
	/*
	 * On error, we may have done a partial boost of the PI chain,
	 * so we need to carefully revert this, then reschedule to
	 * apply any priority change.
	 */
	undo_pi_walk(mutex);
	hard_local_irq_enable();
	evl_schedule();

	return ret;
}
EXPORT_SYMBOL_GPL(evl_lock_mutex_timeout);

void __evl_unlock_mutex(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current(), *top_waiter;
	unsigned long flags;

	trace_evl_mutex_unlock(mutex);

	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);

	enable_inband_switch(curr, mutex);

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
		EVL_WARN_ON(CORE, mutex->flags & EVL_MUTEX_PIBOOST);
		mutex->flags &= ~EVL_MUTEX_CEILING;
		drop_booster(mutex);
	} else if (mutex->flags & EVL_MUTEX_PIBOOST) {
		EVL_WARN_ON(CORE, mutex->flags & EVL_MUTEX_CEILING);
		mutex->flags &= ~EVL_MUTEX_PIBOOST;
		drop_booster(mutex);
	}

	/* Clear the owner information. */
	untrack_mutex_owner(mutex);

	/*
	 * Allow the first waiter in line to retry acquiring the
	 * mutex.
	 */
	if (!list_empty(&mutex->wchan.wait_list)) {
		top_waiter = list_get_entry_init(&mutex->wchan.wait_list,
				struct evl_thread, wait_next);
		evl_wakeup_thread(top_waiter, T_PEND, 0);
	}

	/*
	 * Make the change visible to everyone including user-space
	 * once the kernel state is in sync.
	 */
	atomic_set(mutex->fastlock, EVL_NO_HANDLE);

	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
}

void evl_unlock_mutex(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();
	fundle_t currh = fundle_of(curr), h;

	oob_context_only();

	h = evl_get_index(atomic_read(mutex->fastlock));
	if (h != currh) {
		if (curr->state & T_USER) {
			if (curr->state & T_WOLI)
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
		if (mutex->flags & EVL_MUTEX_PIBOOST) {
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
