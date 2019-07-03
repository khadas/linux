/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evl/sched.h>

static void evl_fifo_init(struct evl_rq *rq)
{
	evl_init_schedq(&rq->fifo.runnable);
}

static void evl_fifo_rotate(struct evl_rq *rq,
			const union evl_sched_param *p)
{
	struct evl_thread *thread, *curr;

	if (evl_schedq_is_empty(&rq->fifo.runnable))
		return;	/* No runnable thread in this class. */

	curr = rq->curr;
	thread = evl_lookup_schedq(&rq->fifo.runnable, p->fifo.prio);
	if (thread == NULL)
		return;

	/*
	 * In case we picked the current thread, we have to make sure
	 * not to move it back to the run queue if it was blocked
	 * before we were called. The same goes if the current thread
	 * holds the scheduler lock.
	 */
	if (thread != curr ||
		(!(curr->state & EVL_THREAD_BLOCK_BITS) &&
			evl_preempt_count() == 0))
		evl_putback_thread(thread);
}

static void evl_fifo_tick(struct evl_rq *rq)
{
	/*
	 * The round-robin time credit is only consumed by a running
	 * thread that neither holds the scheduler lock nor was
	 * blocked before entering this callback. As the time slice is
	 * exhausted for the running thread, move it back to the
	 * run queue at the end of its priority group.
	 */
	evl_putback_thread(rq->curr);
}

static int evl_fifo_chkparam(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	return __evl_chk_fifo_schedparam(thread, p);
}

static bool evl_fifo_setparam(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	return __evl_set_fifo_schedparam(thread, p);
}

static void evl_fifo_getparam(struct evl_thread *thread,
			union evl_sched_param *p)
{
	__evl_get_fifo_schedparam(thread, p);
}

static void evl_fifo_trackprio(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	__evl_track_fifo_priority(thread, p);
}

static void evl_fifo_ceilprio(struct evl_thread *thread, int prio)
{
	__evl_ceil_fifo_priority(thread, prio);
}

static ssize_t evl_fifo_show(struct evl_thread *thread,
			char *buf, ssize_t count)
{
	if (thread->state & T_RRB)
		return snprintf(buf, count, "%Ld\n",
				ktime_to_ns(thread->rrperiod));

	return 0;
}

struct evl_sched_class evl_sched_fifo = {
	.sched_init		=	evl_fifo_init,
	.sched_pick		=	evl_fifo_pick,
	.sched_tick		=	evl_fifo_tick,
	.sched_rotate		=	evl_fifo_rotate,
	.sched_chkparam		=	evl_fifo_chkparam,
	.sched_setparam		=	evl_fifo_setparam,
	.sched_trackprio	=	evl_fifo_trackprio,
	.sched_ceilprio		=	evl_fifo_ceilprio,
	.sched_getparam		=	evl_fifo_getparam,
	.sched_show		=	evl_fifo_show,
	.weight			=	EVL_CLASS_WEIGHT(4),
	.policy			=	SCHED_FIFO,
	.name			=	"fifo"
};
EXPORT_SYMBOL_GPL(evl_sched_fifo);
