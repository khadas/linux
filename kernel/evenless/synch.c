/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2001, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <stdarg.h>
#include <linux/signal.h>
#include <linux/kernel.h>
#include <evenless/sched.h>
#include <evenless/synch.h>
#include <evenless/thread.h>
#include <evenless/monitor.h>
#include <evenless/clock.h>
#include <uapi/evenless/signal.h>
#include <trace/events/evenless.h>

static inline int get_ceiling_value(struct evl_syn *synch)
{
	/*
	 * The ceiling priority value is stored in user-writable
	 * memory, make sure to constrain it within valid bounds for
	 * evl_sched_rt before using it.
	 */
	return clamp(*synch->ceiling_ref, 1U, (u32)EVL_CORE_MAX_PRIO);
}

static inline void get_thread_resource(struct evl_thread *curr)
{
	/*
	 * Track resource locking depth, to prevent weak threads from
	 * being switched back to in-band mode on return from OOB
	 * syscalls.
	 */
	if (curr->state & (T_WEAK|T_DEBUG))
		curr->res_count++;
}

static inline bool put_thread_resource(struct evl_thread *curr)
{
	if ((curr->state & T_WEAK) ||
	    IS_ENABLED(CONFIG_EVENLESS_DEBUG_MONITOR_SLEEP)) {
		if (unlikely(curr->res_count == 0)) {
			if (curr->state & T_WARN)
				evl_signal_thread(curr, SIGDEBUG,
						  SIGDEBUG_RESCNT_IMBALANCE);
			return false;
		}
		curr->res_count--;
	}

	return true;
}

void evl_init_syn(struct evl_syn *synch, int flags,
		  struct evl_clock *clock, atomic_t *fastlock)
{
	if (flags & (EVL_SYN_PI|EVL_SYN_PP))
		flags |= EVL_SYN_PRIO | EVL_SYN_OWNER;

	synch->status = flags & ~EVL_SYN_CLAIMED;
	synch->owner = NULL;
	synch->wprio = -1;
	synch->ceiling_ref = NULL;
	synch->clock = clock;
	INIT_LIST_HEAD(&synch->wait_list);

	if (flags & EVL_SYN_OWNER) {
		synch->fastlock = fastlock;
		atomic_set(fastlock, EVL_NO_HANDLE);
	} else
		synch->fastlock = NULL;
}
EXPORT_SYMBOL_GPL(evl_init_syn);

void evl_init_syn_protect(struct evl_syn *synch,
			  struct evl_clock *clock,
			  atomic_t *fastlock, u32 *ceiling_ref)
{
	evl_init_syn(synch, EVL_SYN_PP, clock, fastlock);
	synch->ceiling_ref = ceiling_ref;
}

bool evl_destroy_syn(struct evl_syn *synch)
{
	bool ret;

	ret = evl_flush_syn(synch, T_RMID);
	EVL_WARN_ON(CORE, synch->status & EVL_SYN_CLAIMED);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_destroy_syn);

static inline
int block_thread_timed(ktime_t timeout, enum evl_tmode timeout_mode,
		       struct evl_clock *clock,
		       struct evl_syn *wchan)
{
	struct evl_thread *curr = evl_current_thread();

	evl_block_thread_timeout(curr, T_PEND, timeout, timeout_mode,
				 clock, wchan);
	evl_schedule();

	return curr->info & (T_RMID|T_TIMEO|T_BREAK);
}

int evl_sleep_on_syn(struct evl_syn *synch, ktime_t timeout,
		     enum evl_tmode timeout_mode)
{
	struct evl_thread *curr;
	unsigned long flags;
	int ret;

	oob_context_only();

	if (EVL_WARN_ON(CORE, synch->status & EVL_SYN_OWNER))
		return T_BREAK;

	curr = evl_current_thread();

	if (IS_ENABLED(CONFIG_EVENLESS_DEBUG_MONITOR_SLEEP) &&
	    curr->res_count > 0 && (curr->state & T_WARN))
		evl_signal_thread(curr, SIGDEBUG, SIGDEBUG_MONITOR_SLEEP);

	xnlock_get_irqsave(&nklock, flags);

	trace_evl_synch_sleepon(synch);

	if (!(synch->status & EVL_SYN_PRIO)) /* i.e. FIFO */
		list_add_tail(&curr->syn_next, &synch->wait_list);
	else /* i.e. priority-sorted */
		list_add_priff(curr, &synch->wait_list, wprio, syn_next);

	ret = block_thread_timed(timeout, timeout_mode, synch->clock, synch);

	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_sleep_on_syn);

struct evl_thread *evl_wake_up_syn(struct evl_syn *synch)
{
	struct evl_thread *thread;
	unsigned long flags;

	if (EVL_WARN_ON(CORE, synch->status & EVL_SYN_OWNER))
		return NULL;

	xnlock_get_irqsave(&nklock, flags);

	if (list_empty(&synch->wait_list)) {
		thread = NULL;
		goto out;
	}

	trace_evl_synch_wakeup(synch);
	thread = list_first_entry(&synch->wait_list, struct evl_thread, syn_next);
	list_del(&thread->syn_next);
	thread->wchan = NULL;
	evl_resume_thread(thread, T_PEND);
out:
	xnlock_put_irqrestore(&nklock, flags);

	return thread;
}
EXPORT_SYMBOL_GPL(evl_wake_up_syn);

int evl_wake_up_nr_syn(struct evl_syn *synch, int nr)
{
	struct evl_thread *thread, *tmp;
	unsigned long flags;
	int nwakeups = 0;

	if (EVL_WARN_ON(CORE, synch->status & EVL_SYN_OWNER))
		return 0;

	xnlock_get_irqsave(&nklock, flags);

	if (list_empty(&synch->wait_list))
		goto out;

	trace_evl_synch_wakeup_many(synch);

	list_for_each_entry_safe(thread, tmp, &synch->wait_list, syn_next) {
		if (nwakeups++ >= nr)
			break;
		list_del(&thread->syn_next);
		thread->wchan = NULL;
		evl_resume_thread(thread, T_PEND);
	}
out:
	xnlock_put_irqrestore(&nklock, flags);

	return nwakeups;
}
EXPORT_SYMBOL_GPL(evl_wake_up_nr_syn);

void evl_wake_up_targeted_syn(struct evl_syn *synch,
			      struct evl_thread *waiter)
{
	unsigned long flags;

	if (EVL_WARN_ON(CORE, synch->status & EVL_SYN_OWNER))
		return;

	xnlock_get_irqsave(&nklock, flags);

	trace_evl_synch_wakeup(synch);
	list_del(&waiter->syn_next);
	waiter->wchan = NULL;
	evl_resume_thread(waiter, T_PEND);

	xnlock_put_irqrestore(&nklock, flags);
}
EXPORT_SYMBOL_GPL(evl_wake_up_targeted_syn);

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
	if (owner->state & T_ZOMBIE)
		return;

	/* Apply the scheduling policy of "target" to "thread" */
	evl_track_thread_policy(owner, target);

	/*
	 * Owner may be sleeping, propagate priority update through
	 * the PI chain if needed.
	 */
	if (owner->wchan)
		evl_requeue_syn_waiter(owner);
}

static void __ceil_owner_priority(struct evl_thread *owner, int prio)
{
	if (owner->state & T_ZOMBIE)
		return;
	/*
	 * Raise owner priority to the ceiling value, this implicitly
	 * selects SCHED_FIFO for the owner.
	 */
	evl_protect_thread_priority(owner, prio);

	if (owner->wchan)
		evl_requeue_syn_waiter(owner);
}

static void adjust_boost(struct evl_thread *owner, struct evl_thread *target)
{
	struct evl_syn *synch;

	/*
	 * CAUTION: we may have PI and PP-enabled objects among the
	 * boosters, considering the leader of synch->wait_list is
	 * therefore NOT enough for determining the next boost
	 * priority, since PP is tracked on acquisition, not on
	 * contention. Check the head of the booster list instead.
	 */
	synch = list_first_entry(&owner->boosters, struct evl_syn, next);
	if (synch->wprio == owner->wprio)
		return;

	if (synch->status & EVL_SYN_PP)
		__ceil_owner_priority(owner, get_ceiling_value(synch));
	else {
		if (EVL_WARN_ON(CORE, list_empty(&synch->wait_list)))
			return;
		if (target == NULL)
			target = list_first_entry(&synch->wait_list,
						  struct evl_thread, syn_next);
		inherit_thread_priority(owner, target);
	}
}

static void ceil_owner_priority(struct evl_syn *synch)
{
	struct evl_thread *owner = synch->owner;
	int wprio;

	/* PP ceiling values are implicitly based on the RT class. */
	wprio = evl_calc_weighted_prio(&evl_sched_rt,
				       get_ceiling_value(synch));
	synch->wprio = wprio;
	list_add_priff(synch, &owner->boosters, wprio, next);
	raise_boost_flag(owner);
	synch->status |= EVL_SYN_CEILING;

	/*
	 * If the ceiling value is lower than the current effective
	 * priority, we must not adjust the latter.  BEWARE: not only
	 * this restriction is required to keep the PP logic right,
	 * but this is also a basic assumption made by all
	 * evl_commit_monitor_ceiling() callers which won't check for any
	 * rescheduling opportunity upon return.
	 *
	 * However we do want the object to be linked to the booster
	 * list, and T_BOOST must appear in the current thread status.
	 *
	 * This way, setparam() won't be allowed to decrease the
	 * current weighted priority below the ceiling value, until we
	 * eventually release this object.
	 */
	if (wprio > owner->wprio)
		adjust_boost(owner, NULL);
}

static inline
void track_owner(struct evl_syn *synch, struct evl_thread *owner)
{
	synch->owner = owner;
}

static inline  /* nklock held, irqs off */
void set_current_owner_locked(struct evl_syn *synch, struct evl_thread *owner)
{
	/*
	 * Update the owner information, and apply priority protection
	 * for PP objects. We may only get there if owner is current,
	 * or blocked.
	 */
	track_owner(synch, owner);
	if (synch->status & EVL_SYN_PP)
		ceil_owner_priority(synch);
}

static inline
void set_current_owner(struct evl_syn *synch, struct evl_thread *owner)
{
	unsigned long flags;

	track_owner(synch, owner);
	if (synch->status & EVL_SYN_PP) {
		xnlock_get_irqsave(&nklock, flags);
		ceil_owner_priority(synch);
		xnlock_put_irqrestore(&nklock, flags);
	}
}

static inline
fundle_t get_owner_handle(fundle_t ownerh, struct evl_syn *synch)
{
	/*
	 * On acquisition from kernel space, the fast lock handle
	 * should bear the FLCEIL bit for PP objects, so that userland
	 * takes the slow path on release, jumping to the kernel for
	 * dropping the ceiling priority boost.
	 */
	if (synch->status & EVL_SYN_PP)
		ownerh = evl_syn_fast_ceil(ownerh);

	return ownerh;
}

void evl_commit_syn_ceiling(struct evl_syn *synch,
			    struct evl_thread *curr)
{
	fundle_t oldh, h;
	atomic_t *lockp;

	/*
	 * For PP locks, userland does, in that order:
	 *
	 * -- LOCK
	 * 1. curr->u_window->pp_pending = fundle_of(monitor)
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
	 * committing a lazy ceiling for the same object, e.g. if
	 * userland is preempted in the middle of a recursive locking
	 * sequence. Since the only synchronization object we have is
	 * the monitor, and that one does not support recursive
	 * locking, what has just been described cannot happen yet
	 * though.
	 *
	 * This would stem from the fact that userland has to update
	 * ->pp_pending prior to trying to grab the lock atomically,
	 * at which point it can figure out whether a recursive
	 * locking happened. We get out of this trap by testing the
	 * EVL_SYN_CEILING flag.
	 */
	if (!evl_is_syn_owner(synch->fastlock, fundle_of(curr)) ||
	    (synch->status & EVL_SYN_CEILING))
		return;

	track_owner(synch, curr);
	ceil_owner_priority(synch);
	/*
	 * Raise FLCEIL, which indicates a kernel entry will be
	 * required for releasing this resource.
	 */
	lockp = synch->fastlock;
	do {
		h = atomic_read(lockp);
		oldh = atomic_cmpxchg(lockp, h, evl_syn_fast_ceil(h));
	} while (oldh != h);
}

int evl_try_acquire_syn(struct evl_syn *synch)
{
	struct evl_thread *curr;
	atomic_t *lockp;
	fundle_t h;

	oob_context_only();

	if (EVL_WARN_ON(CORE, !(synch->status & EVL_SYN_OWNER)))
		return -EINVAL;

	curr = evl_current_thread();
	lockp = synch->fastlock;
	trace_evl_synch_try_acquire(synch);

	h = atomic_cmpxchg(lockp, EVL_NO_HANDLE,
			   get_owner_handle(fundle_of(curr), synch));
	if (h != EVL_NO_HANDLE)
		return evl_get_index(h) == fundle_of(curr) ?
			-EDEADLK : -EBUSY;

	set_current_owner(synch, curr);
	get_thread_resource(curr);

	return 0;
}
EXPORT_SYMBOL_GPL(evl_try_acquire_syn);

#ifdef CONFIG_EVENLESS_DEBUG_MONITOR_INBAND

/*
 * Detect when a thread is about to wait on a synchronization
 * object currently owned by someone running in-band.
 */
static void detect_inband_owner(struct evl_syn *synch,
				struct evl_thread *waiter)
{
	if ((waiter->state & T_WARN) &&
	    !(waiter->info & T_PIALERT) &&
	    (synch->owner->state & T_INBAND)) {
		waiter->info |= T_PIALERT;
		evl_signal_thread(waiter, SIGDEBUG,
				  SIGDEBUG_MIGRATE_PRIOINV);
	} else
		waiter->info &= ~T_PIALERT;
}

/*
 * Detect when a thread is about to switch to in-band context while
 * holding booster(s) (claimed PI or active PP object), which denotes
 * a potential priority inversion. In such an event, any waiter
 * bearing the T_WARN bit will receive a SIGDEBUG notification.
 */
void evl_detect_boost_drop(struct evl_thread *owner)
{
	struct evl_thread *waiter;
	struct evl_syn *synch;
	unsigned long flags;

	xnlock_get_irqsave(&nklock, flags);

	for_each_evl_booster(synch, owner) {
		evl_for_each_syn_waiter(waiter, synch) {
			if (waiter->state & T_WARN) {
				waiter->info |= T_PIALERT;
				evl_signal_thread(waiter, SIGDEBUG,
						  SIGDEBUG_MIGRATE_PRIOINV);
			}
		}
	}

	xnlock_put_irqrestore(&nklock, flags);
}

#else /* !CONFIG_EVENLESS_DEBUG_MONITOR_INBAND */

static inline
void detect_inband_owner(struct evl_syn *synch,
			 struct evl_thread *waiter) { }

#endif /* !CONFIG_EVENLESS_DEBUG_MONITOR_INBAND */

int evl_acquire_syn(struct evl_syn *synch, ktime_t timeout,
		    enum evl_tmode timeout_mode)
{
	struct evl_thread *curr, *owner;
	fundle_t currh, h, oldh;
	unsigned long flags;
	atomic_t *lockp;
	int ret;

	oob_context_only();

	if (EVL_WARN_ON(CORE, !(synch->status & EVL_SYN_OWNER)))
		return T_BREAK;

	curr = evl_current_thread();
	currh = fundle_of(curr);
	lockp = synch->fastlock;
	trace_evl_synch_acquire(synch);
redo:
	/* Basic form of evl_try_acquire_syn(). */
	h = atomic_cmpxchg(lockp, EVL_NO_HANDLE,
			   get_owner_handle(currh, synch));
	if (likely(h == EVL_NO_HANDLE)) {
		set_current_owner(synch, curr);
		get_thread_resource(curr);
		return 0;
	}

	xnlock_get_irqsave(&nklock, flags);

	/*
	 * Set claimed bit.  In case it appears to be set already,
	 * re-read its state under nklock so that we don't miss any
	 * change between the lock-less read and here. But also try to
	 * avoid cmpxchg where possible. Only if it appears not to be
	 * set, start with cmpxchg directly.
	 */
	if (evl_fast_syn_is_claimed(h)) {
		oldh = atomic_read(lockp);
		goto test_no_owner;
	}

	do {
		oldh = atomic_cmpxchg(lockp, h, evl_syn_fast_claim(h));
		if (likely(oldh == h))
			break;
	test_no_owner:
		if (oldh == EVL_NO_HANDLE) {
			/* Lock released from another cpu. */
			xnlock_put_irqrestore(&nklock, flags);
			goto redo;
		}
		h = oldh;
	} while (!evl_fast_syn_is_claimed(h));

	owner = evl_get_element_by_fundle(&evl_thread_factory, h,
					  struct evl_thread);
	/*
	 * If the handle is broken, pretend that the synch object was
	 * deleted to signal an error.
	 */
	if (owner == NULL) {
		xnlock_put_irqrestore(&nklock, flags);
		return T_RMID;
	}

	/*
	 * This is the contended path. We just detected an earlier
	 * syscall-less fast locking from userland, fix up the
	 * in-kernel state information accordingly.
	 *
	 * The consistency of the state information is guaranteed,
	 * because we just raised the claim bit atomically for this
	 * contended lock, therefore userland will have to jump to the
	 * kernel when releasing it, instead of doing a fast
	 * unlock. Since we currently own the superlock, consistency
	 * wrt transfer_ownership() is guaranteed through
	 * serialization.
	 *
	 * CAUTION: in this particular case, the only assumptions we
	 * can safely make is that *owner is valid but not current on
	 * this CPU.
	 */
	track_owner(synch, owner);
	detect_inband_owner(synch, curr);

	if (!(synch->status & EVL_SYN_PRIO)) { /* i.e. FIFO */
		list_add_tail(&curr->syn_next, &synch->wait_list);
		goto block;
	}

	if (curr->wprio > owner->wprio) {
		if ((owner->info & T_WAKEN) && owner->wwake == synch) {
			/* Ownership is still pending, steal the resource. */
			set_current_owner_locked(synch, curr);
			owner->info |= T_ROBBED;
			ret = 0;
			goto grab;
		}

		list_add_priff(curr, &synch->wait_list, wprio, syn_next);

		if (synch->status & EVL_SYN_PI) {
			raise_boost_flag(owner);

			if (synch->status & EVL_SYN_CLAIMED)
				list_del(&synch->next); /* owner->boosters */
			else
				synch->status |= EVL_SYN_CLAIMED;

			synch->wprio = curr->wprio;
			list_add_priff(synch, &owner->boosters, wprio, next);
			/*
			 * curr->wprio > owner->wprio implies that
			 * synch must be leading the booster list
			 * after insertion, so we may call
			 * inherit_thread_priority() for tracking
			 * current's priority directly without going
			 * through adjust_boost().
			 */
			inherit_thread_priority(owner, curr);
		}
	} else
		list_add_priff(curr, &synch->wait_list, wprio, syn_next);
block:
	ret = block_thread_timed(timeout, timeout_mode, synch->clock, synch);
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
	get_thread_resource(curr);

	if (evl_syn_has_waiter(synch))
		currh = evl_syn_fast_claim(currh);

	/* Set new ownership for this object. */
	atomic_set(lockp, get_owner_handle(currh, synch));
out:
	xnlock_put_irqrestore(&nklock, flags);

	evl_put_element(&owner->element);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_acquire_syn);

static void drop_booster(struct evl_syn *synch, struct evl_thread *owner)
{
	list_del(&synch->next);	/* owner->boosters */

	if (list_empty(&owner->boosters)) {
		owner->state &= ~T_BOOST;
		inherit_thread_priority(owner, owner);
	} else
		adjust_boost(owner, NULL);
}

static inline void clear_pi_boost(struct evl_syn *synch,
				  struct evl_thread *owner)
{	/* nklock held, irqs off */
	synch->status &= ~EVL_SYN_CLAIMED;
	drop_booster(synch, owner);
}

static inline void clear_pp_boost(struct evl_syn *synch,
				  struct evl_thread *owner)
{	/* nklock held, irqs off */
	synch->status &= ~EVL_SYN_CEILING;
	drop_booster(synch, owner);
}

static bool transfer_ownership(struct evl_syn *synch,
			       struct evl_thread *lastowner)
{				/* nklock held, irqs off */
	struct evl_thread *n_owner;
	fundle_t n_ownerh;
	atomic_t *lockp;

	lockp = synch->fastlock;

	/*
	 * Our caller checked for contention locklessly, so we do have
	 * to check again under lock in a different way.
	 */
	if (list_empty(&synch->wait_list)) {
		synch->owner = NULL;
		atomic_set(lockp, EVL_NO_HANDLE);
		return false;
	}

	n_owner = list_first_entry(&synch->wait_list, struct evl_thread, syn_next);
	list_del(&n_owner->syn_next);
	n_owner->wchan = NULL;
	n_owner->wwake = synch;
	set_current_owner_locked(synch, n_owner);
	n_owner->info |= T_WAKEN;
	evl_resume_thread(n_owner, T_PEND);

	if (synch->status & EVL_SYN_CLAIMED)
		clear_pi_boost(synch, lastowner);

	n_ownerh = get_owner_handle(fundle_of(n_owner), synch);
	if (evl_syn_has_waiter(synch))
		n_ownerh = evl_syn_fast_claim(n_ownerh);

	atomic_set(lockp, n_ownerh);

	return true;
}

bool evl_release_syn(struct evl_syn *synch)
{
	struct evl_thread *curr = evl_current_thread();
	bool need_resched = false;
	unsigned long flags;
	fundle_t currh, h;
	atomic_t *lockp;

	if (EVL_WARN_ON(CORE, !(synch->status & EVL_SYN_OWNER)))
		return false;

	trace_evl_synch_release(synch);

	if (!put_thread_resource(curr))
		return false;

	lockp = synch->fastlock;
	currh = fundle_of(curr);
	/*
	 * FLCEIL may only be raised by the owner, or when the owner
	 * is blocked waiting for the synch (ownership transfer). In
	 * addition, only the current owner of a synch may release it,
	 * therefore we can't race while testing FLCEIL locklessly.
	 * All updates to FLCLAIM are covered by the superlock.
	 *
	 * Therefore, clearing the fastlock racelessly in this routine
	 * without leaking FLCEIL/FLCLAIM updates can be achieved by
	 * holding the superlock.
	 */
	xnlock_get_irqsave(&nklock, flags);

	if (synch->status & EVL_SYN_CEILING) {
		clear_pp_boost(synch, curr);
		need_resched = true;
	}

	h = atomic_cmpxchg(lockp, currh, EVL_NO_HANDLE);
	if ((h & ~EVL_SYN_FLCEIL) != currh)
		/* FLCLAIM set, synch is contended. */
		need_resched = transfer_ownership(synch, curr);
	else if (h != currh)	/* FLCEIL set, FLCLAIM clear. */
		atomic_set(lockp, EVL_NO_HANDLE);

	xnlock_put_irqrestore(&nklock, flags);

	return need_resched;
}
EXPORT_SYMBOL_GPL(evl_release_syn);

void evl_requeue_syn_waiter(struct evl_thread *thread)
{				/* nklock held, irqs off */
	struct evl_syn *synch = thread->wchan;
	struct evl_thread *owner;

	if (EVL_WARN_ON(CORE, !(synch->status & EVL_SYN_PRIO)))
		return;

	/*
	 * Update the position in the pend queue of a thread waiting
	 * for a lock. This routine propagates the change throughout
	 * the PI chain if required.
	 */
	list_del(&thread->syn_next);
	list_add_priff(thread, &synch->wait_list, wprio, syn_next);
	owner = synch->owner;

	/* Only PI-enabled objects are of interest here. */
	if (!(synch->status & EVL_SYN_PI))
		return;

	synch->wprio = thread->wprio;
	if (synch->status & EVL_SYN_CLAIMED)
		list_del(&synch->next);
	else {
		synch->status |= EVL_SYN_CLAIMED;
		raise_boost_flag(owner);
	}

	list_add_priff(synch, &owner->boosters, wprio, next);
	adjust_boost(owner, thread);
}
EXPORT_SYMBOL_GPL(evl_requeue_syn_waiter);

struct evl_thread *evl_syn_wait_head(struct evl_syn *synch)
{
	return list_first_entry_or_null(&synch->wait_list,
					struct evl_thread, syn_next);
}
EXPORT_SYMBOL_GPL(evl_syn_wait_head);

bool evl_flush_syn(struct evl_syn *synch, int reason)
{
	struct evl_thread *waiter, *tmp;
	unsigned long flags;
	bool ret;

	xnlock_get_irqsave(&nklock, flags);

	trace_evl_synch_flush(synch);

	if (list_empty(&synch->wait_list)) {
		EVL_WARN_ON(CORE, synch->status & EVL_SYN_CLAIMED);
		ret = false;
	} else {
		ret = true;
		list_for_each_entry_safe(waiter, tmp, &synch->wait_list, syn_next) {
			list_del(&waiter->syn_next);
			waiter->info |= reason;
			waiter->wchan = NULL;
			evl_resume_thread(waiter, T_PEND);
		}
		if (synch->status & EVL_SYN_CLAIMED)
			clear_pi_boost(synch, synch->owner);
	}

	xnlock_put_irqrestore(&nklock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_flush_syn);

void evl_forget_syn_waiter(struct evl_thread *thread)
{				/* nklock held, irqs off */
	struct evl_syn *synch = thread->wchan;
	struct evl_thread *owner, *target;

	/*
	 * Do all the necessary housekeeping chores to stop a thread
	 * from waiting on a given synchronization object. Doing so
	 * may require to update a PI chain.
	 */
	trace_evl_synch_forget(synch);

	thread->state &= ~T_PEND;
	thread->wchan = NULL;
	list_del(&thread->syn_next); /* synch->wait_list */

	/*
	 * Only a waiter leaving a PI chain triggers an update.
	 * NOTE: PP objects never bear the CLAIMED bit.
	 */
	if (!(synch->status & EVL_SYN_CLAIMED))
		return;

	owner = synch->owner;

	if (list_empty(&synch->wait_list)) {
		/* No more waiters: clear the PI boost. */
		clear_pi_boost(synch, owner);
		return;
	}

	/*
	 * Reorder the booster queue of the current owner after we
	 * left the wait list, then set its priority to the new
	 * required minimum required to prevent priority inversion.
	 */
	target = list_first_entry(&synch->wait_list, struct evl_thread, syn_next);
	synch->wprio = target->wprio;
	list_del(&synch->next);	/* owner->boosters */
	list_add_priff(synch, &owner->boosters, wprio, next);
	adjust_boost(owner, target);
}
EXPORT_SYMBOL_GPL(evl_forget_syn_waiter);
