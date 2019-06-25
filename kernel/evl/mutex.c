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
#include <uapi/evl/signal.h>
#include <trace/events/evl.h>

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
		evl_signal_thread(curr, SIGDEBUG, SIGDEBUG_MUTEX_IMBALANCE);

	return false;
}

static inline void raise_boost_flag(struct evl_thread *owner)
{
	/* Backup the base priority at first boost only. */
	if (!(owner->state & T_BOOST)) {
		owner->bprio = owner->cprio;
		owner->state |= T_BOOST;
	}
}

static void inherit_thread_priority(struct evl_thread *owner,
				struct evl_thread *target)
{
	struct evl_wait_channel *wchan;

	if (owner->state & T_ZOMBIE)
		return;

	/* Apply the scheduling policy of "target" to "owner" */
	evl_track_thread_policy(owner, target);

	/*
	 * Owner may be sleeping, propagate priority update through
	 * the PI chain if needed.
	 */
	wchan = owner->wchan;
	if (wchan)
		wchan->reorder_wait(owner);
}

static void __ceil_owner_priority(struct evl_thread *owner, int prio)
{
	struct evl_wait_channel *wchan;

	if (owner->state & T_ZOMBIE)
		return;
	/*
	 * Raise owner priority to the ceiling value, this implicitly
	 * selects SCHED_FIFO for the owner.
	 */
	evl_protect_thread_priority(owner, prio);

	wchan = owner->wchan;
	if (wchan)
		wchan->reorder_wait(owner);
}

static void adjust_boost(struct evl_thread *owner, struct evl_thread *target)
{
	struct evl_mutex *mutex;

	/*
	 * CAUTION: we may have PI and PP-enabled mutexes among the
	 * boosters, considering the leader of mutex->wait_list is
	 * therefore NOT enough for determining the next boost
	 * priority, since PP is tracked lazily on acquisition, not
	 * immediately when a contention is detected. Check the head
	 * of the booster list instead.
	 */
	mutex = list_first_entry(&owner->boosters, struct evl_mutex, next);
	if (mutex->wprio == owner->wprio)
		return;

	if (mutex->flags & EVL_MUTEX_PP)
		__ceil_owner_priority(owner, get_ceiling_value(mutex));
	else {
		if (EVL_WARN_ON(CORE, list_empty(&mutex->wait_list)))
			return;
		if (target == NULL)
			target = list_first_entry(&mutex->wait_list,
						struct evl_thread, wait_next);
		inherit_thread_priority(owner, target);
	}
}

static void ceil_owner_priority(struct evl_mutex *mutex)
{
	struct evl_thread *owner = mutex->owner;
	int wprio;

	/* PP ceiling values are implicitly based on the FIFO class. */
	wprio = evl_calc_weighted_prio(&evl_sched_fifo,
				get_ceiling_value(mutex));
	mutex->wprio = wprio;
	list_add_priff(mutex, &owner->boosters, wprio, next);
	raise_boost_flag(owner);
	mutex->flags |= EVL_MUTEX_CEILING;

	/*
	 * If the ceiling value is lower than the current effective
	 * priority, we must not adjust the latter.  BEWARE: not only
	 * this restriction is required to keep the PP logic right,
	 * but this is also a basic assumption made by all
	 * evl_commit_monitor_ceiling() callers which won't check for any
	 * rescheduling opportunity upon return.
	 *
	 * However we do want the mutex to be linked to the booster
	 * list, and T_BOOST must appear in the current thread status.
	 *
	 * This way, setparam() won't be allowed to decrease the
	 * current weighted priority below the ceiling value, until we
	 * eventually release this mutex.
	 */
	if (wprio > owner->wprio)
		adjust_boost(owner, NULL);
}

static inline
void track_owner(struct evl_mutex *mutex,
		struct evl_thread *owner)
{
	mutex->owner = owner;
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

static inline  /* nklock held, irqs off */
void set_current_owner_locked(struct evl_mutex *mutex,
			struct evl_thread *owner)
{
	/*
	 * Update the owner information, and apply priority protection
	 * for PP mutexes. We may only get there if owner is current,
	 * or blocked.
	 */
	track_owner(mutex, owner);
	if (mutex->flags & EVL_MUTEX_PP)
		ceil_owner_priority(mutex);
}

static inline
void set_current_owner(struct evl_mutex *mutex,
		struct evl_thread *owner)
{
	unsigned long flags;

	track_owner(mutex, owner);
	if (mutex->flags & EVL_MUTEX_PP) {
		xnlock_get_irqsave(&nklock, flags);
		ceil_owner_priority(mutex);
		xnlock_put_irqrestore(&nklock, flags);
	}
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

static void drop_booster(struct evl_mutex *mutex, struct evl_thread *owner)
{
	list_del(&mutex->next);	/* owner->boosters */

	if (list_empty(&owner->boosters)) {
		owner->state &= ~T_BOOST;
		inherit_thread_priority(owner, owner);
	} else
		adjust_boost(owner, NULL);
}

static inline void clear_pi_boost(struct evl_mutex *mutex,
				struct evl_thread *owner)
{	/* nklock held, irqs off */
	mutex->flags &= ~EVL_MUTEX_CLAIMED;
	drop_booster(mutex, owner);
}

static inline void clear_pp_boost(struct evl_mutex *mutex,
				struct evl_thread *owner)
{	/* nklock held, irqs off */
	mutex->flags &= ~EVL_MUTEX_CEILING;
	drop_booster(mutex, owner);
}

/*
 * Detect when an out-of-band thread is about to sleep on a mutex
 * currently owned by another thread running in-band.
 */
static void detect_inband_owner(struct evl_mutex *mutex,
				struct evl_thread *waiter)
{
	if (waiter->info & T_PIALERT) {
		waiter->info &= ~T_PIALERT;
		return;
	}

	if (mutex->owner->state & T_INBAND) {
		waiter->info |= T_PIALERT;
		evl_signal_thread(waiter, SIGDEBUG,
				SIGDEBUG_MIGRATE_PRIOINV);
	}
}

/*
 * Detect when a thread is about to switch in-band while holding a
 * mutex which is causing an active PI or PP boost. Since this would
 * cause a priority inversion, any thread waiting for this mutex
 * bearing the T_WOLI bit receives a SIGDEBUG notification in this
 * case.
 */
void evl_detect_boost_drop(struct evl_thread *owner)
{
	struct evl_thread *waiter;
	struct evl_mutex *mutex;
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);

	for_each_evl_booster(mutex, owner) {
		evl_for_each_mutex_waiter(waiter, mutex) {
			if (waiter->state & T_WOLI) {
				waiter->info |= T_PIALERT;
				evl_signal_thread(waiter, SIGDEBUG,
						SIGDEBUG_MIGRATE_PRIOINV);
			}
		}
	}

	xnlock_put_irqrestore(&nklock, flags);
}

static void init_mutex(struct evl_mutex *mutex,
		struct evl_clock *clock, int flags,
		atomic_t *fastlock, u32 *ceiling_ref)
{
	mutex->fastlock = fastlock;
	atomic_set(fastlock, EVL_NO_HANDLE);
	mutex->flags = flags & ~EVL_MUTEX_CLAIMED;
	mutex->owner = NULL;
	mutex->wprio = -1;
	mutex->ceiling_ref = ceiling_ref;
	mutex->clock = clock;
	INIT_LIST_HEAD(&mutex->wait_list);
	mutex->wchan.abort_wait = evl_abort_mutex_wait;
	mutex->wchan.reorder_wait = evl_reorder_mutex_wait;
	raw_spin_lock_init(&mutex->wchan.lock);
}

void evl_init_mutex_pi(struct evl_mutex *mutex,
		struct evl_clock *clock, atomic_t *fastlock)
{
	init_mutex(mutex, clock, EVL_MUTEX_PI, fastlock, NULL);
}
EXPORT_SYMBOL_GPL(evl_init_mutex_pi);

void evl_init_mutex_pp(struct evl_mutex *mutex,
		struct evl_clock *clock,
		atomic_t *fastlock, u32 *ceiling_ref)
{
	init_mutex(mutex, clock, EVL_MUTEX_PP, fastlock, ceiling_ref);
}
EXPORT_SYMBOL_GPL(evl_init_mutex_pp);

void evl_flush_mutex(struct evl_mutex *mutex, int reason)
{
	struct evl_thread *waiter, *tmp;
	unsigned long flags;

	trace_evl_mutex_flush(mutex);

	xnlock_get_irqsave(&nklock, flags);

	if (list_empty(&mutex->wait_list))
		EVL_WARN_ON(CORE, mutex->flags & EVL_MUTEX_CLAIMED);
	else {
		list_for_each_entry_safe(waiter, tmp, &mutex->wait_list, wait_next)
			evl_wakeup_thread(waiter, T_PEND, reason);

		if (mutex->flags & EVL_MUTEX_CLAIMED)
			clear_pi_boost(mutex, mutex->owner);
	}

	xnlock_put_irqrestore(&nklock, flags);
}

void evl_destroy_mutex(struct evl_mutex *mutex)
{
	trace_evl_mutex_destroy(mutex);
	evl_flush_mutex(mutex, T_RMID);
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

	xnlock_get_irqsave(&nklock, flags);

	/*
	 * Set claimed bit.  In case it appears to be set already,
	 * re-read its state under nklock so that we don't miss any
	 * change between the lock-less read and here. But also try to
	 * avoid cmpxchg where possible. Only if it appears not to be
	 * set, start with cmpxchg directly.
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
			xnlock_put_irqrestore(&nklock, flags);
			goto redo;
		}
		h = oldh;
	} while (!fast_mutex_is_claimed(h));

	owner = evl_get_element_by_fundle(&evl_thread_factory, h,
					struct evl_thread);
	/*
	 * If the handle is broken, pretend that the mutex was deleted
	 * to signal an error.
	 */
	if (owner == NULL) {
		xnlock_put_irqrestore(&nklock, flags);
		return T_RMID;
	}

	/*
	 * This is the contended path. We just detected an earlier
	 * syscall-less fast locking, fix up the state information
	 * accordingly.
	 *
	 * The consistency of such information is guaranteed, because
	 * we just raised the claim bit atomically for this contended
	 * lock, therefore userland will have to jump to the kernel
	 * when releasing it, instead of doing a fast unlock. Since we
	 * currently own the superlock, consistency wrt
	 * transfer_ownership() is guaranteed through serialization.
	 *
	 * CAUTION: in this particular case, the only assumptions we
	 * can safely make is that *owner is valid but not current on
	 * this CPU.
	 */
	track_owner(mutex, owner);

	if (unlikely(curr->state & T_WOLI))
		detect_inband_owner(mutex, curr);

	if (curr->wprio > owner->wprio) {
		if ((owner->info & T_WAKEN) && owner->wwake == &mutex->wchan) {
			/* Ownership is still pending, steal the resource. */
			set_current_owner_locked(mutex, curr);
			owner->info |= T_ROBBED;
			ret = 0;
			goto grab;
		}

		list_add_priff(curr, &mutex->wait_list, wprio, wait_next);

		if (mutex->flags & EVL_MUTEX_PI) {
			raise_boost_flag(owner);

			if (mutex->flags & EVL_MUTEX_CLAIMED)
				list_del(&mutex->next); /* owner->boosters */
			else
				mutex->flags |= EVL_MUTEX_CLAIMED;

			mutex->wprio = curr->wprio;
			list_add_priff(mutex, &owner->boosters, wprio, next);
			/*
			 * curr->wprio > owner->wprio implies that
			 * mutex must be leading the booster list
			 * after insertion, so we may call
			 * inherit_thread_priority() for tracking
			 * current's priority directly without going
			 * through adjust_boost().
			 */
			inherit_thread_priority(owner, curr);
		}
	} else
		list_add_priff(curr, &mutex->wait_list, wprio, wait_next);

	evl_sleep_on(timeout, timeout_mode, mutex->clock, &mutex->wchan);
	evl_schedule();
	ret = curr->info & (T_RMID|T_TIMEO|T_BREAK);
	curr->wwake = NULL;
	curr->info &= ~T_WAKEN;

	if (ret)
		goto out;

	if (curr->info & T_ROBBED) {
		/*
		 * Somebody stole us the ownership while we were ready
		 * to run, waiting for the CPU: we need to wait again
		 * for the resource.
		 */
		if (timeout_mode != EVL_REL || timeout_infinite(timeout)) {
			xnlock_put_irqrestore(&nklock, flags);
			goto redo;
		}
		timeout = evl_get_stopped_timer_delta(&curr->rtimer);
		if (timeout) { /* Otherwise, it's too late. */
			xnlock_put_irqrestore(&nklock, flags);
			goto redo;
		}
		ret = T_TIMEO;
		goto out;
	}
grab:
	disable_inband_switch(curr);

	if (!list_empty(&mutex->wait_list)) /* any waiters? */
		currh = mutex_fast_claim(currh);

	/* Set new ownership. */
	atomic_set(lockp, get_owner_handle(currh, mutex));
out:
	xnlock_put_irqrestore(&nklock, flags);

	evl_put_element(&owner->element);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_lock_mutex_timeout);

/* nklock held, irqs off */
static void transfer_ownership(struct evl_mutex *mutex,
			struct evl_thread *lastowner)
{
	struct evl_thread *n_owner;
	fundle_t n_ownerh;
	atomic_t *lockp;

	lockp = mutex->fastlock;

	/*
	 * Our caller checked for contention locklessly, so we do have
	 * to check again under lock in a different way.
	 */
	if (list_empty(&mutex->wait_list)) {
		mutex->owner = NULL;
		atomic_set(lockp, EVL_NO_HANDLE);
		return;
	}

	n_owner = list_first_entry(&mutex->wait_list, struct evl_thread, wait_next);
	/*
	 * We clear the wait channel early on - instead of waiting for
	 * evl_wakeup_thread() to do so - because we want to hide
	 * n_owner from the PI/PP adjustment which takes place over
	 * set_current_owner_locked(). Because of that, we also have
	 * to unlink the thread from the wait list manually since the
	 * abort_wait() handler won't be called. NOTE: we do want
	 * set_current_owner_locked() to run before
	 * evl_wakeup_thread() is called.
	 */
	n_owner->wchan = NULL;
	list_del(&n_owner->wait_next);
	n_owner->wwake = &mutex->wchan;
	set_current_owner_locked(mutex, n_owner);
	evl_wakeup_thread(n_owner, T_PEND, T_WAKEN);

	if (mutex->flags & EVL_MUTEX_CLAIMED)
		clear_pi_boost(mutex, lastowner);

	n_ownerh = get_owner_handle(fundle_of(n_owner), mutex);
	if (!list_empty(&mutex->wait_list)) /* any waiters? */
		n_ownerh = mutex_fast_claim(n_ownerh);

	atomic_set(lockp, n_ownerh);
}

void __evl_unlock_mutex(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();
	unsigned long flags;
	fundle_t currh, h;
	atomic_t *lockp;

	trace_evl_mutex_unlock(mutex);

	if (!enable_inband_switch(curr))
		return;

	lockp = mutex->fastlock;
	currh = fundle_of(curr);
	if (evl_get_index(atomic_read(lockp)) != currh)
		return;

	/*
	 * FLCEIL may only be raised by the owner, or when the owner
	 * is blocked waiting for the mutex (ownership transfer). In
	 * addition, only the current owner of a mutex may release it,
	 * therefore we can't race while testing FLCEIL locklessly.
	 * All updates to FLCLAIM are covered by the superlock.
	 *
	 * Therefore, clearing the fastlock racelessly in this routine
	 * without leaking FLCEIL/FLCLAIM updates can be achieved
	 * locklessly.
	 */
	xnlock_get_irqsave(&nklock, flags);

	if (mutex->flags & EVL_MUTEX_CEILING)
		clear_pp_boost(mutex, curr);

	h = atomic_cmpxchg(lockp, currh, EVL_NO_HANDLE);
	if ((h & ~EVL_MUTEX_FLCEIL) != currh)
		/* FLCLAIM set, mutex is contended. */
		transfer_ownership(mutex, curr);
	else if (h != currh)	/* FLCEIL set, FLCLAIM clear. */
		atomic_set(lockp, EVL_NO_HANDLE);

	xnlock_put_irqrestore(&nklock, flags);
}

void evl_unlock_mutex(struct evl_mutex *mutex)
{
	__evl_unlock_mutex(mutex);
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_unlock_mutex);

static inline struct evl_mutex *
wchan_to_mutex(struct evl_wait_channel *wchan)
{
	return container_of(wchan, struct evl_mutex, wchan);
}

/* nklock held, irqs off */
void evl_abort_mutex_wait(struct evl_thread *thread,
			struct evl_wait_channel *wchan)
{
	struct evl_mutex *mutex = wchan_to_mutex(wchan);
	struct evl_thread *owner, *target;

	/*
	 * Do all the necessary housekeeping chores to stop a thread
	 * from waiting on a mutex. Doing so may require to update a
	 * PI chain.
	 */
	list_del(&thread->wait_next); /* mutex->wait_list */

	/*
	 * Only a waiter leaving a PI chain triggers an update.
	 * NOTE: PP mutexes never bear the CLAIMED bit.
	 */
	if (!(mutex->flags & EVL_MUTEX_CLAIMED))
		return;

	owner = mutex->owner;

	if (list_empty(&mutex->wait_list)) {
		/* No more waiters: clear the PI boost. */
		clear_pi_boost(mutex, owner);
		return;
	}

	/*
	 * Reorder the booster queue of the current owner after we
	 * left the wait list, then set its priority to the new
	 * required minimum required to prevent priority inversion.
	 */
	target = list_first_entry(&mutex->wait_list,
				struct evl_thread, wait_next);
	mutex->wprio = target->wprio;
	list_del(&mutex->next);	/* owner->boosters */
	list_add_priff(mutex, &owner->boosters, wprio, next);
	adjust_boost(owner, target);
}

/* nklock held, irqs off */
void evl_reorder_mutex_wait(struct evl_thread *thread)
{
	struct evl_mutex *mutex = wchan_to_mutex(thread->wchan);
	struct evl_thread *owner;

	/*
	 * Update the position in the wait list of a thread waiting
	 * for a lock. This routine propagates the change throughout
	 * the PI chain if required.
	 */
	list_del(&thread->wait_next);
	list_add_priff(thread, &mutex->wait_list, wprio, wait_next);

	if (!(mutex->flags & EVL_MUTEX_PI))
		return;

	/* Update the PI chain. */

	owner = mutex->owner;
	mutex->wprio = thread->wprio;
	if (mutex->flags & EVL_MUTEX_CLAIMED)
		list_del(&mutex->next);
	else {
		mutex->flags |= EVL_MUTEX_CLAIMED;
		raise_boost_flag(owner);
	}

	list_add_priff(mutex, &owner->boosters, wprio, next);
	adjust_boost(owner, thread);
}

void evl_commit_mutex_ceiling(struct evl_mutex *mutex)
{
	struct evl_thread *curr = evl_current();
	fundle_t oldh, h;
	atomic_t *lockp;

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
	 * Hypothetical case: we might be called multiple times for
	 * committing a lazy ceiling for the same mutex, e.g. if
	 * userland is preempted in the middle of a recursive locking
	 * sequence. Since this mutex implementation does not support
	 * recursive locking on purpose, what has just been described
	 * cannot happen yet though.
	 *
	 * This would stem from the fact that userland has to update
	 * ->pp_pending prior to trying to grab the lock atomically,
	 * at which point it can figure out whether a recursive
	 * locking happened. We get out of this trap by testing the
	 * EVL_MUTEX_CEILING flag.
	 */
	if (!evl_is_mutex_owner(mutex->fastlock, fundle_of(curr)) ||
		(mutex->flags & EVL_MUTEX_CEILING))
		return;

	track_owner(mutex, curr);
	ceil_owner_priority(mutex);
	/*
	 * Raise FLCEIL, which indicates a kernel entry will be
	 * required for releasing this resource.
	 */
	lockp = mutex->fastlock;
	do {
		h = atomic_read(lockp);
		oldh = atomic_cmpxchg(lockp, h, mutex_fast_ceil(h));
	} while (oldh != h);
}
