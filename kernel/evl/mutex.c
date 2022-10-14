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

#define for_each_evl_mutex_waiter(__pos, __mutex) \
	list_for_each_entry(__pos, &(__mutex)->wchan.wait_list, wait_next)

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

/* origin->owner->lock + contender->lock + origin->wchan.lock held, irqs off */
static int adjust_boost(struct evl_mutex *origin,
			struct evl_thread *contender,
			struct evl_thread *originator)
{
	struct evl_thread *owner = origin->owner;
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
	 * |  owner := mutex->owner
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
static void ceil_owner_priority(struct evl_mutex *mutex,
				struct evl_thread *originator)
{
	struct evl_thread *owner = mutex->owner;
	int wprio;

	assert_hard_lock(&mutex->wchan.lock);

	/* PP ceiling values are implicitly based on the FIFO class. */
	wprio = evl_calc_weighted_prio(&evl_sched_fifo,
				get_ceiling_value(mutex));
	mutex->wprio = wprio;

	/*
	 * If the ceiling value is lower than the current effective
	 * priority, we must not adjust the latter.  BEWARE: not only
	 * this restriction is required to keep the PP logic right,
	 * but this is also a basic assumption made by all callers of
	 * evl_commit_monitor_ceiling() which won't check for any
	 * rescheduling opportunity upon return.
	 *
	 * However we do want the mutex to be linked to the booster
	 * list, and T_BOOST must appear in the current thread status.
	 *
	 * This way, setparam() won't be allowed to decrease the
	 * current weighted priority below the ceiling value, until we
	 * eventually release this mutex.
	 */
	raw_spin_lock(&owner->lock);

	list_add_priff(mutex, &owner->boosters, wprio, next_booster);
	raise_boost_flag(owner);
	mutex->flags |= EVL_MUTEX_CEILING;

	if (wprio > owner->wprio)
		adjust_boost(mutex, NULL, originator);

	raw_spin_unlock(&owner->lock);
}

/* mutex->wchan.lock held, irqs off */
static void untrack_owner(struct evl_mutex *mutex)
{
	struct evl_thread *prev = mutex->owner;

	assert_hard_lock(&mutex->wchan.lock);

	if (prev) {
		raw_spin_lock(&prev->tracking_lock);
		list_del(&mutex->next_tracker);
		raw_spin_unlock(&prev->tracking_lock);
		evl_put_element(&prev->element);
		mutex->owner = NULL;
	}
}

/* mutex->wchan.lock held, irqs off. */
static void track_owner(struct evl_mutex *mutex,
			struct evl_thread *owner)
{
	struct evl_thread *prev = mutex->owner;
	unsigned long flags;

	assert_hard_lock(&mutex->wchan.lock);

	if (EVL_WARN_ON_ONCE(CORE, prev == owner))
		return;

	raw_spin_lock_irqsave(&owner->tracking_lock, flags);
	if (prev) {
		list_del(&mutex->next_tracker);
		smp_wmb();
		evl_put_element(&prev->element);
	}
	list_add(&mutex->next_tracker, &owner->trackers);
	raw_spin_unlock_irqrestore(&owner->tracking_lock, flags);
	mutex->owner = owner;
}

/* mutex->wchan.lock held, irqs off. */
static inline void ref_and_track_owner(struct evl_mutex *mutex,
				struct evl_thread *owner)
{
	assert_hard_lock(&mutex->wchan.lock);

	if (mutex->owner != owner) {
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

/* mutex->wchan.lock held, irqs off */
static void set_current_owner_locked(struct evl_mutex *mutex,
				struct evl_thread *owner)
{
	assert_hard_lock(&mutex->wchan.lock);

	/*
	 * Update the owner information, and apply priority protection
	 * for PP mutexes. We may only get there if owner is current,
	 * or blocked.
	 */
	ref_and_track_owner(mutex, owner);
	if (mutex->flags & EVL_MUTEX_PP)
		ceil_owner_priority(mutex, owner);
}

/* mutex->wchan.lock held, irqs off */
static inline
void set_current_owner(struct evl_mutex *mutex,
		struct evl_thread *owner)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);
	set_current_owner_locked(mutex, owner);
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
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

/* mutex->wchan.lock + mutex->owner->lock held, irqs off */
static void clear_boost_locked(struct evl_mutex *mutex, int flag)
{
	struct evl_thread *owner;

	assert_hard_lock(&mutex->wchan.lock);
	owner = mutex->owner;
	assert_hard_lock(&owner->lock);

	mutex->flags &= ~flag;

	list_del(&mutex->next_booster);	/* owner->boosters */
	if (list_empty(&owner->boosters)) {
		raw_spin_lock(&owner->rq->lock);
		owner->state &= ~T_BOOST;
		raw_spin_unlock(&owner->rq->lock);
		inherit_thread_priority(owner, owner, owner);
	} else {
		adjust_boost(mutex, NULL, owner);
	}
}

/* mutex->wchan.lock held, irqs off */
static void clear_boost(struct evl_mutex *mutex, int flag)
{
	assert_hard_lock(&mutex->wchan.lock);

	raw_spin_lock(&mutex->owner->lock);
	clear_boost_locked(mutex, flag);
	raw_spin_unlock(&mutex->owner->lock);
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
	struct evl_thread *owner = mutex->owner;

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
	mutex->flags = type & ~EVL_MUTEX_CLAIMED;
	mutex->owner = NULL;
	mutex->wprio = -1;
	mutex->ceiling_ref = ceiling_ref;
	mutex->clock = clock;
	mutex->wchan.reorder_wait = evl_reorder_mutex_wait;
	mutex->wchan.follow_depend = evl_follow_mutex_depend;
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

		if (mutex->flags & EVL_MUTEX_CLAIMED)
			clear_boost(mutex, EVL_MUTEX_CLAIMED);
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
	untrack_owner(mutex);
	flush_mutex_locked(mutex, T_RMID);
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
	lockdep_unregister_key(&mutex->wchan.lock_key);
}
EXPORT_SYMBOL_GPL(evl_destroy_mutex);

int evl_trylock_mutex(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();
	atomic_t *lockp = mutex->fastlock;
	fundle_t h;

	oob_context_only();

	trace_evl_mutex_trylock(mutex);

	h = atomic_cmpxchg(lockp, EVL_NO_HANDLE,
			get_owner_handle(fundle_of(curr), mutex));
	if (h != EVL_NO_HANDLE)
		return evl_get_index(h) == fundle_of(curr) ?
			-EDEADLK : -EBUSY;

	set_current_owner(mutex, curr);
	disable_inband_switch(curr);

	return 0;
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

/* mutex->wchan.lock held, irqs off */
static void finish_mutex_wait(struct evl_mutex *mutex)
{
	struct evl_thread *owner, *contender;

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

	owner = mutex->owner;	/* Might not be this_evl_rq()->curr. */

	if (list_empty(&mutex->wchan.wait_list)) {
		/* No more waiters: clear the PI boost. */
		clear_boost(mutex, EVL_MUTEX_CLAIMED);
		return;
	}

	/*
	 * Reorder the booster queue since the wait list was updated,
	 * then set the priority of the current owner to the new
	 * minimum required to prevent inversion with any remaining
	 * contender.
	 */
	contender = list_first_entry(&mutex->wchan.wait_list,
				struct evl_thread, wait_next);

	double_thread_lock(owner, contender);
	mutex->wprio = contender->wprio;
	list_del(&mutex->next_booster);	/* owner->boosters */
	list_add_priff(mutex, &owner->boosters, wprio, next_booster);
	adjust_boost(mutex, contender, owner);
	raw_spin_unlock(&contender->lock);
	raw_spin_unlock(&owner->lock);
}

/* owner->lock (dropped at exit), irqs off */
static int check_lock_chain(struct evl_thread *owner,
			struct evl_thread *originator)
{
	struct evl_wait_channel *wchan;
	struct evl_thread *t = owner;

	assert_hard_lock(&owner->lock);

	for (;;) {
		wchan = t->wchan;
		if (!wchan || !wchan->follow_depend)
			break;
		t = wchan->follow_depend(t, originator);
		if (!t)
			return -EDEADLK;
	}

	raw_spin_unlock(&t->lock);

	return 0;
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
	/* Basic form of evl_trylock_mutex(). */
	h = atomic_cmpxchg(lockp, EVL_NO_HANDLE,
			get_owner_handle(currh, mutex));
	if (likely(h == EVL_NO_HANDLE)) {
		set_current_owner(mutex, curr);
		disable_inband_switch(curr);
		return 0;
	}

	if (unlikely(evl_get_index(h) == currh))
		return -EDEADLK;

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
	if (fast_mutex_is_claimed(h)) {
		oldh = atomic_read(lockp);
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
	 * This is the contended path. If mutex->owner does not match
	 * (is NULL actually) the information we retrieved from the
	 * atomic handle, it means that such mutex was acquired using
	 * a fast locking operation: mutex->owner needs update based
	 * on the atomic handle value, we also have to add the mutex
	 * to the tracking list of the owner thread.
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
	if (mutex->owner != owner)
		track_owner(mutex, owner);
	else
		/*
		 * evl_get_factory_element_by_fundle() got us an
		 * extraneous reference on @owner which an earlier
		 * call to track_owner() already obtained, drop the
		 * former.
		 */
		evl_put_element(&owner->element);

	if (unlikely(curr->state & T_WOLI))
		detect_inband_owner(mutex, curr);

	double_thread_lock(curr, owner);

	if (curr->wprio > owner->wprio) {
		if ((owner->info & T_WAKEN) && owner->wwake == &mutex->wchan) {
			/*
			 * Ownership was granted but the current owner
			 * did not resume execution yet. Since we take
			 * priority over that thread, steal the
			 * resource from it.
			 */
			raw_spin_lock(&owner->rq->lock);
			owner->info |= T_ROBBED;
			raw_spin_unlock(&owner->rq->lock);
			raw_spin_unlock(&owner->lock);
			raw_spin_unlock(&curr->lock);
			set_current_owner_locked(mutex, curr);
			goto grab;
		}

		list_add_priff(curr, &mutex->wchan.wait_list, wprio, wait_next);

		if (mutex->flags & EVL_MUTEX_PI) {
			raise_boost_flag(owner);

			if (mutex->flags & EVL_MUTEX_CLAIMED)
				list_del(&mutex->next_booster); /* owner->boosters */
			else
				mutex->flags |= EVL_MUTEX_CLAIMED;

			mutex->wprio = curr->wprio;
			list_add_priff(mutex, &owner->boosters, wprio, next_booster);
			/*
			 * Since boosters are ordered on curr->wprio,
			 * having curr->wprio > owner->wprio implies
			 * that mutex must be leading the booster list
			 * of the owner after insertion. Therefore we
			 * may call inherit_thread_priority() for
			 * tracking current's priority directly
			 * without going through adjust_boost().
			 */
			ret = inherit_thread_priority(owner, curr, curr);
			raw_spin_unlock(&curr->lock);
			raw_spin_unlock(&owner->lock);
		} else {
			raw_spin_unlock(&curr->lock);
			ret = check_lock_chain(owner, curr); /* drops owner->lock */
		}
	} else {
		list_add_priff(curr, &mutex->wchan.wait_list, wprio, wait_next);
		raw_spin_unlock(&curr->lock);
		ret = check_lock_chain(owner, curr);
	}

	if (likely(!ret)) {
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
	raw_spin_unlock(&curr->lock);
grab:
	disable_inband_switch(curr);

	if (!list_empty(&mutex->wchan.wait_list)) /* any waiters? */
		currh = mutex_fast_claim(currh);

	atomic_set(lockp, get_owner_handle(currh, mutex));
out:
	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_lock_mutex_timeout);

/* mutex->wchan.lock, irqs off */
static void transfer_ownership(struct evl_mutex *mutex)
{
	atomic_t *lockp = mutex->fastlock;
	struct evl_thread *n_owner;
	fundle_t n_ownerh;

	assert_hard_lock(&mutex->wchan.lock);

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
	if (list_empty(&mutex->wchan.wait_list)) {
		untrack_owner(mutex);
		atomic_set(lockp, EVL_NO_HANDLE);
		return;
	}

	EVL_WARN_ON(CORE, !(atomic_read(lockp) & EVL_MUTEX_FLCLAIM));

	n_owner = list_first_entry(&mutex->wchan.wait_list,
				struct evl_thread, wait_next);
	/*
	 * We clear the wait channel early on - instead of waiting for
	 * evl_wakeup_thread() to do so - because we want to hide
	 * n_owner from the PI/PP adjustment which takes place over
	 * set_current_owner_locked().
	 *
	 * Beware: we do want set_current_owner_locked() to run before
	 * evl_wakeup_thread() is called.
	 */
	raw_spin_lock(&n_owner->lock);
	n_owner->wwake = &mutex->wchan;
	n_owner->wchan = NULL;
	list_del_init(&n_owner->wait_next);
	raw_spin_unlock(&n_owner->lock);
	/*
	 * FIXME: could n_owner go stale now?
	 */
	set_current_owner_locked(mutex, n_owner);
	evl_wakeup_thread(n_owner, T_PEND, T_WAKEN);

	n_ownerh = get_owner_handle(fundle_of(n_owner), mutex);
	if (!list_empty(&mutex->wchan.wait_list)) /* any waiters? */
		n_ownerh = mutex_fast_claim(n_ownerh);

	atomic_set(lockp, n_ownerh);
}

void __evl_unlock_mutex(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;
	int bflag;

	trace_evl_mutex_unlock(mutex);

	if (!enable_inband_switch(curr))
		return;

	raw_spin_lock_irqsave(&mutex->wchan.lock, flags);

	bflag = mutex->flags & (EVL_MUTEX_CEILING|EVL_MUTEX_CLAIMED);

	raw_spin_lock(&curr->lock);

	if (bflag) {
		EVL_WARN_ON_ONCE(CORE,
			bflag == (EVL_MUTEX_CEILING|EVL_MUTEX_CLAIMED));
		clear_boost_locked(mutex, bflag);
	}

	raw_spin_unlock(&curr->lock);

	transfer_ownership(mutex);

	raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
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

void evl_drop_tracking_mutexes(struct evl_thread *curr)
{
	struct evl_mutex *mutex;
	unsigned long flags;
	fundle_t h;

	raw_spin_lock_irqsave(&curr->tracking_lock, flags);

	/* Release all mutexes tracking @curr. */
	while (!list_empty(&curr->trackers)) {
		/*
		 * Either __evl_unlock_mutex() or untrack_owner() will
		 * unlink @mutex from the curr's tracker list.
		 */
		mutex = list_first_entry(&curr->trackers,
					struct evl_mutex, next_tracker);
		raw_spin_unlock_irqrestore(&curr->tracking_lock, flags);
		h = evl_get_index(atomic_read(mutex->fastlock));
		if (h == fundle_of(curr)) {
			__evl_unlock_mutex(mutex);
		} else {
			raw_spin_lock_irqsave(&mutex->wchan.lock, flags);
			if (mutex->owner == curr)
				untrack_owner(mutex);
			raw_spin_unlock_irqrestore(&mutex->wchan.lock, flags);
		}
		raw_spin_lock_irqsave(&curr->tracking_lock, flags);
	}

	raw_spin_unlock_irqrestore(&curr->tracking_lock, flags);
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

	owner = mutex->owner;
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
	ret = adjust_boost(mutex, waiter, originator);
	raw_spin_unlock(&owner->lock);
out:
	raw_spin_unlock(&mutex->wchan.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_reorder_mutex_wait);

/* prev->lock (dropped), irqs off. */
struct evl_thread *
evl_follow_mutex_depend(struct evl_thread *prev,
 			struct evl_thread *originator)
{
	struct evl_mutex *mutex = wchan_to_mutex(prev->wchan);
	struct evl_thread *next;

	/*
	 * Progress into the lock chain by traversing dependent locks
	 * through the wait channels. On entry, we receive a locked
	 * waiter, which gives us safe access to its active wait
	 * channel.
	 *
	 * Holding the lock on a mutex prevents any pending waiter to
	 * leave the wait, so we may drop the thread lock once that
	 * mutex is acquired. Likewise, we may access and lock its
	 * owner which we do before dropping the mutex lock.
	 */
	raw_spin_lock(&mutex->wchan.lock);
	raw_spin_unlock(&prev->lock);

	next = mutex->owner;
	if (next == originator) { /* Deadlock? */
		raw_spin_unlock(&mutex->wchan.lock);
		return NULL;
	}

	raw_spin_lock(&next->lock);
	raw_spin_unlock(&mutex->wchan.lock);

	return next;
}
EXPORT_SYMBOL_GPL(evl_follow_mutex_depend);

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

	set_current_owner_locked(mutex, curr);	
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
