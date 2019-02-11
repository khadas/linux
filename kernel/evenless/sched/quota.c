/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2013, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/bitmap.h>
#include <asm/div64.h>
#include <evenless/sched.h>
#include <evenless/memory.h>
#include <uapi/evenless/sched.h>

/*
 * With this policy, each per-CPU runqueue maintains a list of active
 * thread groups for the sched_rt class.
 *
 * Each time a thread is picked from the runqueue, we check whether we
 * still have budget for running it, looking at the group it belongs
 * to. If so, a timer is armed to elapse when that group has no more
 * budget, would the incoming thread run unpreempted until then
 * (i.e. evl_quota->limit_timer).
 *
 * Otherwise, if no budget remains in the group for running the
 * candidate thread, we move the latter to a local expiry queue
 * maintained by the group. This process is done on the fly as we pull
 * from the runqueue.
 *
 * Updating the remaining budget is done each time the EVL core asks
 * for replacing the current thread with the next runnable one,
 * i.e. evl_quota_pick(). There we charge the elapsed run time of the
 * outgoing thread to the relevant group, and conversely, we check
 * whether the incoming thread has budget.
 *
 * Finally, a per-CPU timer (evl_quota->refill_timer) periodically
 * ticks in the background, in accordance to the defined quota
 * interval. Thread group budgets get replenished by its handler in
 * accordance to their respective share, pushing all expired threads
 * back to the run queue in the same move.
 *
 * NOTE: since the core logic enforcing the budget entirely happens in
 * evl_quota_pick(), applying a budget change can be done as simply as
 * forcing the rescheduling procedure to be invoked asap. As a result
 * of this, the EVL core will ask for the next thread to run, which
 * means calling evl_quota_pick() eventually.
 *
 * CAUTION: evl_quota_group->nr_active does count both the threads
 * from that group linked to the sched_rt runqueue, _and_ the threads
 * moved to the local expiry queue. As a matter of fact, the expired
 * threads - those for which we consumed all the per-group budget -
 * are still seen as runnable (i.e. not blocked/suspended) by the EVL
 * core. This only means that the SCHED_QUOTA policy won't pick them
 * until the corresponding budget is replenished.
 */
static DECLARE_BITMAP(group_map, CONFIG_EVENLESS_SCHED_QUOTA_NR_GROUPS);

static inline int group_is_active(struct evl_quota_group *tg)
{
	struct evl_thread *curr = tg->rq->curr;

	if (tg->nr_active)
		return 1;

	/*
	 * Check whether the current thread belongs to the group, and
	 * is still in running state (T_READY denotes a thread linked
	 * to the runqueue, in which case tg->nr_active already
	 * accounts for it).
	 */
	if (curr->quota == tg &&
		(curr->state & (T_READY|EVL_THREAD_BLOCK_BITS)) == 0)
		return 1;

	return 0;
}

static inline void replenish_budget(struct evl_sched_quota *qs,
				struct evl_quota_group *tg)
{
	ktime_t budget, credit;

	if (tg->quota == tg->quota_peak) {
		/*
		 * Fast path: we don't accumulate runtime credit.
		 * This includes groups with no runtime limit
		 * (i.e. quota off: quota >= period && quota == peak).
		 */
		tg->run_budget = tg->quota;
		return;
	}

	/*
	 * We have to deal with runtime credit accumulation, as the
	 * group may consume more than its base quota during a single
	 * interval, up to a peak duration though (not to monopolize
	 * the CPU).
	 *
	 * - In the simplest case, a group is allotted a new full
	 * budget plus the unconsumed portion of the previous budget,
	 * provided the sum does not exceed the peak quota.
	 *
	 * - When there is too much budget for a single interval
	 * (i.e. above peak quota), we spread the extra time over
	 * multiple intervals through a credit accumulation mechanism.
	 *
	 * - The accumulated credit is dropped whenever a group has no
	 * runnable threads.
	 */
	if (!group_is_active(tg)) {
		/* Drop accumulated credit. */
		tg->run_credit = 0;
		tg->run_budget = tg->quota;
		return;
	}

	budget = ktime_add(tg->run_budget, tg->quota);
	if (budget > tg->quota_peak) {
		/* Too much budget, spread it over intervals. */
		tg->run_credit =
			ktime_add(tg->run_credit,
				ktime_sub(budget, tg->quota_peak));
		tg->run_budget = tg->quota_peak;
	} else if (tg->run_credit) {
		credit = ktime_sub(tg->quota_peak, budget);
		/* Consume the accumulated credit. */
		if (tg->run_credit >= credit)
			tg->run_credit =
				ktime_sub(tg->run_credit, credit);
		else {
			credit = tg->run_credit;
			tg->run_credit = 0;
		}
		/* Allot extended budget, limited to peak quota. */
		tg->run_budget = ktime_add(budget, credit);
	} else
		/* No credit, budget was below peak quota. */
		tg->run_budget = budget;
}

static void quota_refill_handler(struct evl_timer *timer) /* hard irqs off */
{
	struct evl_quota_group *tg;
	struct evl_thread *thread, *tmp;
	struct evl_sched_quota *qs;
	struct evl_rq *rq;

	qs = container_of(timer, struct evl_sched_quota, refill_timer);
	rq = container_of(qs, struct evl_rq, quota);

	xnlock_get(&nklock);

	list_for_each_entry(tg, &qs->groups, next) {
		/* Allot a new runtime budget for the group. */
		replenish_budget(qs, tg);

		if (tg->run_budget == 0 || list_empty(&tg->expired))
			continue;
		/*
		 * For each group living on this CPU, move all expired
		 * threads back to the runqueue. Since those threads
		 * were moved out of the runqueue as we were
		 * considering them for execution, we push them back
		 * in LIFO order to their respective priority group.
		 * The expiry queue is FIFO to keep ordering right
		 * among expired threads.
		 */
		list_for_each_entry_safe_reverse(thread, tmp, &tg->expired, quota_expired) {
			list_del_init(&thread->quota_expired);
			evl_add_schedq(&rq->rt.runnable, thread);
		}
	}

	evl_set_self_resched(evl_get_timer_rq(timer));

	xnlock_put(&nklock);
}

static void quota_limit_handler(struct evl_timer *timer) /* hard irqs off */
{
	struct evl_rq *rq;

	rq = container_of(timer, struct evl_rq, quota.limit_timer);
	/*
	 * Force a rescheduling on the return path of the current
	 * interrupt, so that the budget is re-evaluated for the
	 * current group in evl_quota_pick().
	 */
	xnlock_get(&nklock);
	evl_set_self_resched(rq);
	xnlock_put(&nklock);
}

static int quota_sum_all(struct evl_sched_quota *qs)
{
	struct evl_quota_group *tg;
	int sum;

	if (list_empty(&qs->groups))
		return 0;

	sum = 0;
	list_for_each_entry(tg, &qs->groups, next)
		sum += tg->quota_percent;

	return sum;
}

static void quota_init(struct evl_rq *rq)
{
	struct evl_sched_quota *qs = &rq->quota;

	qs->period = CONFIG_EVENLESS_SCHED_QUOTA_PERIOD * 1000ULL;
	INIT_LIST_HEAD(&qs->groups);

	evl_init_timer(&qs->refill_timer,
		&evl_mono_clock, quota_refill_handler, rq,
		EVL_TIMER_IGRAVITY);
	evl_set_timer_name(&qs->refill_timer, "[quota-refill]");

	evl_init_timer(&qs->limit_timer,
		&evl_mono_clock, quota_limit_handler, rq,
		EVL_TIMER_IGRAVITY);
	evl_set_timer_name(&qs->limit_timer, "[quota-limit]");
}

static bool quota_setparam(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	struct evl_quota_group *tg;
	struct evl_sched_quota *qs;
	bool effective;

	thread->state &= ~T_WEAK;
	effective = evl_set_effective_thread_priority(thread, p->quota.prio);

	qs = &thread->rq->quota;
	list_for_each_entry(tg, &qs->groups, next) {
		if (tg->tgid != p->quota.tgid)
			continue;
		if (thread->quota) {
			/* Dequeued earlier by our caller. */
			list_del(&thread->quota_next);
			thread->quota->nr_threads--;
		}
		thread->quota = tg;
		list_add(&thread->quota_next, &tg->members);
		tg->nr_threads++;
		return effective;
	}

	return false;		/* not reached. */
}

static void quota_getparam(struct evl_thread *thread,
			union evl_sched_param *p)
{
	p->quota.prio = thread->cprio;
	p->quota.tgid = thread->quota->tgid;
}

static void quota_trackprio(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	if (p) {
		/* We should not cross groups during PI boost. */
		EVL_WARN_ON(CORE,
			thread->base_class == &evl_sched_quota &&
			thread->quota->tgid != p->quota.tgid);
		thread->cprio = p->quota.prio;
	} else
		thread->cprio = thread->bprio;
}

static void quota_ceilprio(struct evl_thread *thread, int prio)
{
	if (prio > EVL_QUOTA_MAX_PRIO)
		prio = EVL_QUOTA_MAX_PRIO;

	thread->cprio = prio;
}

static int quota_chkparam(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	struct evl_quota_group *tg;
	struct evl_sched_quota *qs;
	int tgid;

	if (p->quota.prio < EVL_QUOTA_MIN_PRIO ||
		p->quota.prio > EVL_QUOTA_MAX_PRIO)
		return -EINVAL;

	tgid = p->quota.tgid;
	if (tgid < 0 || tgid >= CONFIG_EVENLESS_SCHED_QUOTA_NR_GROUPS)
		return -EINVAL;

	/*
	 * The group must be managed on the same CPU the thread
	 * currently runs on.
	 */
	qs = &thread->rq->quota;
	list_for_each_entry(tg, &qs->groups, next) {
		if (tg->tgid == tgid)
			return 0;
	}

	/*
	 * If that group exists nevertheless, we give userland a
	 * specific error code.
	 */
	if (test_bit(tgid, group_map))
		return -EPERM;

	return -EINVAL;
}

static void quota_forget(struct evl_thread *thread)
{
	thread->quota->nr_threads--;
	EVL_WARN_ON_ONCE(CORE, thread->quota->nr_threads < 0);
	list_del(&thread->quota_next);
	thread->quota = NULL;
}

static void quota_kick(struct evl_thread *thread)
{
	struct evl_quota_group *tg = thread->quota;
	struct evl_rq *rq = thread->rq;

	/*
	 * Allow a kicked thread to be elected for running until it
	 * switches to in-band context, even if the group it belongs
	 * to lacks runtime budget.
	 */
	if (tg->run_budget == 0 && !list_empty(&thread->quota_expired)) {
		list_del_init(&thread->quota_expired);
		evl_add_schedq_tail(&rq->rt.runnable, thread);
	}
}

static inline int thread_is_runnable(struct evl_thread *thread)
{
	return thread->quota->run_budget > 0 || (thread->info & T_KICKED);
}

static void quota_enqueue(struct evl_thread *thread)
{
	struct evl_quota_group *tg = thread->quota;
	struct evl_rq *rq = thread->rq;

	if (!thread_is_runnable(thread))
		list_add_tail(&thread->quota_expired, &tg->expired);
	else
		evl_add_schedq_tail(&rq->rt.runnable, thread);

	tg->nr_active++;
}

static void quota_dequeue(struct evl_thread *thread)
{
	struct evl_quota_group *tg = thread->quota;
	struct evl_rq *rq = thread->rq;

	if (!list_empty(&thread->quota_expired))
		list_del_init(&thread->quota_expired);
	else
		evl_del_schedq(&rq->rt.runnable, thread);

	tg->nr_active--;
}

static void quota_requeue(struct evl_thread *thread)
{
	struct evl_quota_group *tg = thread->quota;
	struct evl_rq *rq = thread->rq;

	if (!thread_is_runnable(thread))
		list_add(&thread->quota_expired, &tg->expired);
	else
		evl_add_schedq(&rq->rt.runnable, thread);

	tg->nr_active++;
}

static struct evl_thread *quota_pick(struct evl_rq *rq)
{
	struct evl_thread *next, *curr = rq->curr;
	struct evl_sched_quota *qs = &rq->quota;
	struct evl_quota_group *otg, *tg;
	ktime_t now, elapsed;

	now = evl_read_clock(&evl_mono_clock);
	otg = curr->quota;
	if (otg == NULL)
		goto pick;
	/*
	 * Charge the time consumed by the outgoing thread to the
	 * group it belongs to.
	 */
	elapsed = ktime_sub(now, otg->run_start);
	if (elapsed < otg->run_budget)
		otg->run_budget = ktime_sub(otg->run_budget, elapsed);
	else
		otg->run_budget = 0;
pick:
	next = evl_get_schedq(&rq->rt.runnable);
	if (next == NULL) {
		evl_stop_timer(&qs->limit_timer);
		return NULL;
	}

	/*
	 * As we basically piggyback on the SCHED_FIFO runqueue, make
	 * sure to detect non-quota threads.
	 */
	tg = next->quota;
	if (tg == NULL)
		return next;

	tg->run_start = now;

	/*
	 * Don't consider budget if kicked, we have to allow this
	 * thread to run until it eventually switches to in-band
	 * context.
	 */
	if (next->info & T_KICKED) {
		evl_stop_timer(&qs->limit_timer);
		goto out;
	}

	if (ktime_to_ns(tg->run_budget) == 0) {
		/* Flush expired group members as we go. */
		list_add_tail(&next->quota_expired, &tg->expired);
		goto pick;
	}

	if (otg == tg && evl_timer_is_running(&qs->limit_timer))
		/* Same group, leave the running timer untouched. */
		goto out;

	/* Arm limit timer for the new running group. */
	evl_start_timer(&qs->limit_timer,
			ktime_add(now, tg->run_budget),
			EVL_INFINITE);
out:
	tg->nr_active--;

	return next;
}

static void quota_migrate(struct evl_thread *thread, struct evl_rq *rq)
{
	union evl_sched_param param;
	/*
	 * Runtime quota groups are defined per-CPU, so leaving the
	 * current CPU means exiting the group. We do this by moving
	 * the target thread to the plain RT class.
	 */
	param.rt.prio = thread->cprio;
	__evl_set_thread_schedparam(thread, &evl_sched_rt, &param);
}

static ssize_t quota_show(struct evl_thread *thread,
			char *buf, ssize_t count)
{
	return snprintf(buf, count, "%d\n",
			thread->quota->tgid);
}

int evl_quota_create_group(struct evl_quota_group *tg,
			struct evl_rq *rq,
			int *quota_sum_r)
{
	int tgid, nr_groups = CONFIG_EVENLESS_SCHED_QUOTA_NR_GROUPS;
	struct evl_sched_quota *qs = &rq->quota;

	requires_ugly_lock();

	tgid = find_first_zero_bit(group_map, nr_groups);
	if (tgid >= nr_groups)
		return -ENOSPC;

	__set_bit(tgid, group_map);
	tg->tgid = tgid;
	tg->rq = rq;
	tg->run_budget = qs->period;
	tg->run_credit = 0;
	tg->quota_percent = 100;
	tg->quota_peak_percent = 100;
	tg->quota = qs->period;
	tg->quota_peak = qs->period;
	tg->nr_active = 0;
	tg->nr_threads = 0;
	INIT_LIST_HEAD(&tg->members);
	INIT_LIST_HEAD(&tg->expired);

	if (list_empty(&qs->groups))
		evl_start_timer(&qs->refill_timer,
				evl_abs_timeout(&qs->refill_timer, qs->period),
				qs->period);

	list_add(&tg->next, &qs->groups);
	*quota_sum_r = quota_sum_all(qs);

	return 0;
}
EXPORT_SYMBOL_GPL(evl_quota_create_group);

int evl_quota_destroy_group(struct evl_quota_group *tg,
			int force, int *quota_sum_r)
{
	struct evl_sched_quota *qs = &tg->rq->quota;
	struct evl_thread *thread, *tmp;
	union evl_sched_param param;

	requires_ugly_lock();

	if (!list_empty(&tg->members)) {
		if (!force)
			return -EBUSY;
		/* Move group members to the rt class. */
		list_for_each_entry_safe(thread, tmp, &tg->members, quota_next) {
			param.rt.prio = thread->cprio;
			__evl_set_thread_schedparam(thread, &evl_sched_rt, &param);
		}
	}

	list_del(&tg->next);
	__clear_bit(tg->tgid, group_map);

	if (list_empty(&qs->groups))
		evl_stop_timer(&qs->refill_timer);

	*quota_sum_r = quota_sum_all(qs);

	return 0;
}
EXPORT_SYMBOL_GPL(evl_quota_destroy_group);

void evl_quota_set_limit(struct evl_quota_group *tg,
			int quota_percent, int quota_peak_percent,
			int *quota_sum_r)
{
	struct evl_sched_quota *qs = &tg->rq->quota;
	ktime_t now, elapsed, consumed;
	ktime_t old_quota = tg->quota;
	u64 n;

	requires_ugly_lock();

	if (quota_percent < 0 || quota_percent > 100) { /* Quota off. */
		quota_percent = 100;
		tg->quota = qs->period;
	} else {
		n = qs->period * quota_percent;
		do_div(n, 100);
		tg->quota = n;
	}

	if (quota_peak_percent < quota_percent)
		quota_peak_percent = quota_percent;

	if (quota_peak_percent < 0 || quota_peak_percent > 100) {
		quota_peak_percent = 100;
		tg->quota_peak = qs->period;
	} else {
		n = qs->period * quota_peak_percent;
		do_div(n, 100);
		tg->quota_peak = n;
	}

	tg->quota_percent = quota_percent;
	tg->quota_peak_percent = quota_peak_percent;

	if (group_is_active(tg)) {
		now = evl_read_clock(&evl_mono_clock);

		elapsed = now - tg->run_start;
		if (elapsed < tg->run_budget)
			tg->run_budget -= elapsed;
		else
			tg->run_budget = 0;

		tg->run_start = now;
		evl_stop_timer(&qs->limit_timer);
	}

	if (tg->run_budget <= old_quota)
		consumed = old_quota - tg->run_budget;
	else
		consumed = 0;

	if (tg->quota >= consumed)
		tg->run_budget = tg->quota - consumed;
	else
		tg->run_budget = 0;

	tg->run_credit = 0;	/* Drop accumulated credit. */

	*quota_sum_r = quota_sum_all(qs);

	/*
	 * Apply the new budget immediately, in case a member of this
	 * group is currently running.
	 */
	evl_set_resched(tg->rq);
	evl_schedule();
}
EXPORT_SYMBOL_GPL(evl_quota_set_limit);

struct evl_quota_group *
evl_quota_find_group(struct evl_rq *rq, int tgid)
{
	struct evl_quota_group *tg;

	requires_ugly_lock();

	if (list_empty(&rq->quota.groups))
		return NULL;

	list_for_each_entry(tg, &rq->quota.groups, next) {
		if (tg->tgid == tgid)
			return tg;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(evl_quota_find_group);

int evl_quota_sum_all(struct evl_rq *rq)
{
	struct evl_sched_quota *qs = &rq->quota;

	requires_ugly_lock();

	return quota_sum_all(qs);
}
EXPORT_SYMBOL_GPL(evl_quota_sum_all);

struct evl_sched_class evl_sched_quota = {
	.sched_init		=	quota_init,
	.sched_enqueue		=	quota_enqueue,
	.sched_dequeue		=	quota_dequeue,
	.sched_requeue		=	quota_requeue,
	.sched_pick		=	quota_pick,
	.sched_migrate		=	quota_migrate,
	.sched_chkparam		=	quota_chkparam,
	.sched_setparam		=	quota_setparam,
	.sched_getparam		=	quota_getparam,
	.sched_trackprio	=	quota_trackprio,
	.sched_ceilprio		=	quota_ceilprio,
	.sched_forget		=	quota_forget,
	.sched_kick		=	quota_kick,
	.sched_show		=	quota_show,
	.weight			=	EVL_CLASS_WEIGHT(2),
	.policy			=	SCHED_QUOTA,
	.name			=	"quota"
};
EXPORT_SYMBOL_GPL(evl_sched_quota);
