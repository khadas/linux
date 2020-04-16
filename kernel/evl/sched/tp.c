/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evl/sched.h>
#include <evl/memory.h>
#include <uapi/evl/sched.h>

static void tp_schedule_next(struct evl_sched_tp *tp)
{
	struct evl_tp_window *w;
	struct evl_rq *rq;
	ktime_t t, now;
	int p_next;

	rq = container_of(tp, struct evl_rq, tp);
	assert_evl_lock(&rq->lock);

	for (;;) {
		/*
		 * Switch to the next partition. Time holes in a
		 * global time frame are defined as partition windows
		 * assigned to part# -1, in which case the (always
		 * empty) idle queue will be polled for runnable
		 * threads.  Therefore, we may assume that a window
		 * begins immediately after the previous one ends,
		 * which simplifies the implementation a lot.
		 */
		w = &tp->gps->pwins[tp->wnext];
		p_next = w->w_part;
		tp->tps = p_next < 0 ? &tp->idle : &tp->partitions[p_next];

		/* Schedule tick to advance to the next window. */
		tp->wnext = (tp->wnext + 1) % tp->gps->pwin_nr;
		w = &tp->gps->pwins[tp->wnext];
		t = ktime_add(tp->tf_start, w->w_offset);

		/*
		 * If we are late, make sure to remain within the
		 * bounds of a valid time frame before advancing to
		 * the next window. Otherwise, fix up by advancing to
		 * the next time frame immediately.
		 */
		now = evl_read_clock(&evl_mono_clock);
		if (ktime_compare(now, t) > 0) {
			t = ktime_add(tp->tf_start, tp->gps->tf_duration);
			if (ktime_compare(now, t) > 0) {
				tp->tf_start = t;
				tp->wnext = 0;
			}
		}
		evl_start_timer(&tp->tf_timer, t, EVL_INFINITE);
	}

	evl_set_resched(rq);
}

static void tp_tick_handler(struct evl_timer *timer)
{
	struct evl_rq *rq = container_of(timer, struct evl_rq, tp.tf_timer);
	struct evl_sched_tp *tp = &rq->tp;

	evl_spin_lock(&rq->lock);

	/*
	 * Advance beginning date of time frame by a full period if we
	 * are processing the last window.
	 */
	if (tp->wnext + 1 == tp->gps->pwin_nr)
		tp->tf_start = ktime_add(tp->tf_start, tp->gps->tf_duration);

	tp_schedule_next(tp);

	evl_spin_unlock(&rq->lock);
}

static void tp_init(struct evl_rq *rq)
{
	struct evl_sched_tp *tp = &rq->tp;
	int n;

	for (n = 0; n < CONFIG_EVL_SCHED_TP_NR_PART; n++)
		evl_init_schedq(&tp->partitions[n].runnable);

	tp->tps = NULL;
	tp->gps = NULL;
	INIT_LIST_HEAD(&tp->threads);
	evl_init_schedq(&tp->idle.runnable);
	evl_init_timer_on_rq(&tp->tf_timer, &evl_mono_clock, tp_tick_handler,
			rq, EVL_TIMER_IGRAVITY);
	evl_set_timer_name(&tp->tf_timer, "[tp-tick]");
}

static bool tp_setparam(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	struct evl_rq *rq = evl_thread_rq(thread);

	thread->tps = &rq->tp.partitions[p->tp.ptid];
	thread->state &= ~T_WEAK;

	return evl_set_effective_thread_priority(thread, p->tp.prio);
}

static void tp_getparam(struct evl_thread *thread,
			union evl_sched_param *p)
{
	p->tp.prio = thread->cprio;
	p->tp.ptid = thread->tps - evl_thread_rq(thread)->tp.partitions;
}

static void tp_trackprio(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	/*
	 * The assigned partition never changes internally due to PI
	 * (see evl_track_thread_policy()), since this would be pretty
	 * wrong with respect to TP scheduling: i.e. we may not allow
	 * a thread from another partition to consume CPU time from
	 * the current one, despite this would help enforcing PI (see
	 * note). In any case, introducing resource contention between
	 * threads that belong to different partitions is utterly
	 * wrong in the first place.  Only an explicit call to
	 * evl_set_thread_policy() may change the partition assigned
	 * to a thread. For that reason, a policy reset action only
	 * boils down to reinstating the base priority.
	 *
	 * NOTE: we do allow threads from lower scheduling classes to
	 * consume CPU time from the current window as a result of a
	 * PI boost, since this is aimed at speeding up the release of
	 * a synchronization object a TP thread needs.
	 */
	if (p) {
		/* We should never cross partition boundaries. */
		EVL_WARN_ON(CORE,
			thread->base_class == &evl_sched_tp &&
			thread->tps - evl_thread_rq(thread)->tp.partitions
			!= p->tp.ptid);
		thread->cprio = p->tp.prio;
	} else
		thread->cprio = thread->bprio;
}

static void tp_ceilprio(struct evl_thread *thread, int prio)
{
  	if (prio > EVL_TP_MAX_PRIO)
		prio = EVL_TP_MAX_PRIO;

	thread->cprio = prio;
}

static int tp_chkparam(struct evl_thread *thread,
		const union evl_sched_param *p)
{
	struct evl_sched_tp *tp = &evl_thread_rq(thread)->tp;

	if (tp->gps == NULL ||
		p->tp.prio < EVL_TP_MIN_PRIO ||
		p->tp.prio > EVL_TP_MAX_PRIO ||
		p->tp.ptid < 0 ||
		p->tp.ptid >= CONFIG_EVL_SCHED_TP_NR_PART)
		return -EINVAL;

	return 0;
}

static int tp_declare(struct evl_thread *thread,
		const union evl_sched_param *p)
{
	struct evl_rq *rq = evl_thread_rq(thread);

	list_add_tail(&thread->tp_link, &rq->tp.threads);

	return 0;
}

static void tp_forget(struct evl_thread *thread)
{
	list_del(&thread->tp_link);
	thread->tps = NULL;
}

static void tp_enqueue(struct evl_thread *thread)
{
	evl_add_schedq_tail(&thread->tps->runnable, thread);
}

static void tp_dequeue(struct evl_thread *thread)
{
	evl_del_schedq(&thread->tps->runnable, thread);
}

static void tp_requeue(struct evl_thread *thread)
{
	evl_add_schedq(&thread->tps->runnable, thread);
}

static struct evl_thread *tp_pick(struct evl_rq *rq)
{
	/* Never pick a thread if we don't schedule partitions. */
	if (!evl_timer_is_running(&rq->tp.tf_timer))
		return NULL;

	return evl_get_schedq(&rq->tp.tps->runnable);
}

static void tp_migrate(struct evl_thread *thread, struct evl_rq *rq)
{
	union evl_sched_param param;
	/*
	 * Since our partition schedule is a per-rq property, it
	 * cannot apply to a thread that moves to another CPU
	 * anymore. So we upgrade that thread to the FIFO class when a
	 * CPU migration occurs. A subsequent call to
	 * evl_set_thread_schedparam_locked() may move it back to TP
	 * scheduling, with a partition assignment that fits the
	 * remote CPU's partition schedule.
	 */
	param.fifo.prio = thread->cprio;
	evl_set_thread_schedparam_locked(thread, &evl_sched_fifo, &param);
}

static ssize_t tp_show(struct evl_thread *thread,
		char *buf, ssize_t count)
{
	int ptid = thread->tps - evl_thread_rq(thread)->tp.partitions;

	return snprintf(buf, count, "%d\n", ptid);
}

static void start_tp_schedule(struct evl_rq *rq)
{
	struct evl_sched_tp *tp = &rq->tp;

	assert_evl_lock(&rq->lock);

	if (tp->gps == NULL)
		return;

	tp->wnext = 0;
	tp->tf_start = evl_read_clock(&evl_mono_clock);
	tp_schedule_next(tp);
}

static void stop_tp_schedule(struct evl_rq *rq)
{
	struct evl_sched_tp *tp = &rq->tp;

	assert_evl_lock(&rq->lock);

	if (tp->gps)
		evl_stop_timer(&tp->tf_timer);
}

static struct evl_tp_schedule *
set_tp_schedule(struct evl_rq *rq, struct evl_tp_schedule *gps)
{
	struct evl_sched_tp *tp = &rq->tp;
	struct evl_thread *thread, *tmp;
	struct evl_tp_schedule *old_gps;
	union evl_sched_param param;

	assert_evl_lock(&rq->lock);

	if (EVL_WARN_ON(CORE, gps != NULL &&
		(gps->pwin_nr <= 0 || gps->pwins[0].w_offset != 0)))
		return tp->gps;

	stop_tp_schedule(rq);

	/*
	 * Move all TP threads on this scheduler to the FIFO class,
	 * until we call evl_set_thread_schedparam_locked() for them again.
	 */
	if (list_empty(&tp->threads))
		goto done;

	list_for_each_entry_safe(thread, tmp, &tp->threads, tp_link) {
		param.fifo.prio = thread->cprio;
		evl_set_thread_schedparam_locked(thread, &evl_sched_fifo, &param);
	}
done:
	old_gps = tp->gps;
	tp->gps = gps;

	return old_gps;
}

static struct evl_tp_schedule *
get_tp_schedule(struct evl_rq *rq)
{
	struct evl_tp_schedule *gps = rq->tp.gps;

	if (gps == NULL)
		return NULL;

	atomic_inc(&gps->refcount);

	return gps;
}

static void put_tp_schedule(struct evl_tp_schedule *gps)
{
	if (atomic_dec_and_test(&gps->refcount))
		evl_free(gps);
}

static int tp_control(int cpu, union evl_sched_ctlparam *ctlp,
		union evl_sched_ctlinfo *infp)
{
	struct evl_tp_ctlparam *pt = &ctlp->tp;
	ktime_t offset, duration, next_offset;
	struct evl_tp_schedule *gps, *ogps;
	struct __evl_tp_window *p, *pp;
	struct evl_tp_window *w, *pw;
	struct evl_tp_ctlinfo *it;
	unsigned long flags;
	struct evl_rq *rq;
	int n;

	if (cpu < 0 || !cpu_present(cpu) || !is_threading_cpu(cpu))
		return -EINVAL;

	rq = evl_cpu_rq(cpu);

	evl_spin_lock_irqsave(&rq->lock, flags);

	switch (pt->op) {
	case evl_tp_install:
		if (pt->nr_windows > 0)
			goto install_schedule;
		/* fall-through. */
	case evl_tp_uninstall:
		gps = NULL;
		goto switch_schedule;
	case evl_tp_start:
		start_tp_schedule(rq);
		evl_spin_unlock_irqrestore(&rq->lock, flags);
		return 0;
	case evl_tp_stop:
		stop_tp_schedule(rq);
		evl_spin_unlock_irqrestore(&rq->lock, flags);
		return 0;
	case evl_tp_get:
		break;
	default:
		return -EINVAL;
	}

	gps = get_tp_schedule(rq);
	evl_spin_unlock_irqrestore(&rq->lock, flags);
	if (gps == NULL)
		return 0;

	if (infp == NULL) {
		put_tp_schedule(gps);
		return -EINVAL;
	}

	it = &infp->tp;
	if (it->nr_windows < gps->pwin_nr) {
		put_tp_schedule(gps);
		return -ENOSPC;
	}

	it->nr_windows = gps->pwin_nr;
	for (n = 0, pp = p = it->windows, pw = w = gps->pwins;
	     n < gps->pwin_nr; pp = p, p++, pw = w, w++, n++) {
		p->offset = ktime_to_u_timespec(w->w_offset);
		pp->duration = ktime_to_u_timespec(
				     ktime_sub(w->w_offset, pw->w_offset));
		p->ptid = w->w_part;
	}

	pp->duration = ktime_to_u_timespec(
		     ktime_sub(gps->tf_duration, pw->w_offset));

	put_tp_schedule(gps);

	return 0;

install_schedule:
	evl_spin_unlock_irqrestore(&rq->lock, flags);

	gps = evl_alloc(sizeof(*gps) + pt->nr_windows * sizeof(*w));
	if (gps == NULL)
		return -ENOMEM;

	for (n = 0, p = pt->windows, w = gps->pwins, next_offset = 0;
	     n < pt->nr_windows; n++, p++, w++) {
		/*
		 * Time windows must be strictly contiguous. Holes may
		 * be defined using windows assigned to the pseudo
		 * partition #-1.
		 */
		offset = u_timespec_to_ktime(p->offset);
		if (offset != next_offset)
			goto fail;

		duration = u_timespec_to_ktime(p->duration);
		if (duration <= 0)
			goto fail;

		if (p->ptid < -1 ||
		    p->ptid >= CONFIG_EVL_SCHED_TP_NR_PART)
			goto fail;

		w->w_offset = next_offset;
		w->w_part = p->ptid;
		next_offset = ktime_add(next_offset, duration);
	}

	atomic_set(&gps->refcount, 1);
	gps->pwin_nr = n;
	gps->tf_duration = next_offset;
	evl_spin_lock_irqsave(&rq->lock, flags);
switch_schedule:
	ogps = set_tp_schedule(rq, gps);
	evl_spin_unlock_irqrestore(&rq->lock, flags);

	if (ogps)
		put_tp_schedule(ogps);

	return 0;
fail:
	evl_free(gps);

	return -EINVAL;
}

struct evl_sched_class evl_sched_tp = {
	.sched_init		=	tp_init,
	.sched_enqueue		=	tp_enqueue,
	.sched_dequeue		=	tp_dequeue,
	.sched_requeue		=	tp_requeue,
	.sched_pick		=	tp_pick,
	.sched_migrate		=	tp_migrate,
	.sched_chkparam		=	tp_chkparam,
	.sched_setparam		=	tp_setparam,
	.sched_getparam		=	tp_getparam,
	.sched_trackprio	=	tp_trackprio,
	.sched_ceilprio		=	tp_ceilprio,
	.sched_declare		=	tp_declare,
	.sched_forget		=	tp_forget,
	.sched_show		=	tp_show,
	.sched_control		=	tp_control,
	.weight			=	EVL_CLASS_WEIGHT(3),
	.policy			=	SCHED_TP,
	.name			=	"tp"
};
EXPORT_SYMBOL_GPL(evl_sched_tp);
