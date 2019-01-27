/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evenless/sched.h>

static void evl_rt_init(struct evl_rq *rq)
{
	evl_init_schedq(&rq->rt.runnable);
}

static void evl_rt_requeue(struct evl_thread *thread)
{
	/*
	 * Put back at same place: i.e. requeue to head of current
	 * priority group (i.e. LIFO, used for preemption handling).
	 */
	__evl_requeue_rt_thread(thread);
}

static void evl_rt_enqueue(struct evl_thread *thread)
{
	/*
	 * Enqueue for next pick: i.e. move to end of current priority
	 * group (i.e. FIFO).
	 */
	__evl_enqueue_rt_thread(thread);
}

static void evl_rt_dequeue(struct evl_thread *thread)
{
	/*
	 * Pull from the runnable thread queue.
	 */
	__evl_dequeue_rt_thread(thread);
}

static void evl_rt_rotate(struct evl_rq *rq,
			  const union evl_sched_param *p)
{
	struct evl_thread *thread, *curr;

	if (evl_schedq_is_empty(&rq->rt.runnable))
		return;	/* No runnable thread in this class. */

	curr = rq->curr;
	thread = evl_lookup_schedq(&rq->rt.runnable, p->rt.prio);
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

static void evl_rt_tick(struct evl_rq *rq)
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

static int evl_rt_chkparam(struct evl_thread *thread,
			   const union evl_sched_param *p)
{
	return __evl_chk_rt_schedparam(thread, p);
}

static bool evl_rt_setparam(struct evl_thread *thread,
			    const union evl_sched_param *p)
{
	return __evl_set_rt_schedparam(thread, p);
}

static void evl_rt_getparam(struct evl_thread *thread,
			    union evl_sched_param *p)
{
	__evl_get_rt_schedparam(thread, p);
}

static void evl_rt_trackprio(struct evl_thread *thread,
			     const union evl_sched_param *p)
{
	__evl_track_rt_priority(thread, p);
}

static void evl_rt_ceilprio(struct evl_thread *thread, int prio)
{
	__evl_ceil_rt_priority(thread, prio);
}

static ssize_t evl_rt_show(struct evl_thread *thread,
			   char *buf, ssize_t count)
{
	if (thread->state & T_RRB)
		return snprintf(buf, count, "%Ld\n",
				ktime_to_ns(thread->rrperiod));

	return 0;
}

struct evl_sched_class evl_sched_rt = {
	.sched_init		=	evl_rt_init,
	.sched_enqueue		=	evl_rt_enqueue,
	.sched_dequeue		=	evl_rt_dequeue,
	.sched_requeue		=	evl_rt_requeue,
	.sched_pick		=	evl_rt_pick,
	.sched_tick		=	evl_rt_tick,
	.sched_rotate		=	evl_rt_rotate,
	.sched_chkparam		=	evl_rt_chkparam,
	.sched_setparam		=	evl_rt_setparam,
	.sched_trackprio	=	evl_rt_trackprio,
	.sched_ceilprio		=	evl_rt_ceilprio,
	.sched_getparam		=	evl_rt_getparam,
	.sched_show		=	evl_rt_show,
	.weight			=	EVL_CLASS_WEIGHT(3),
	.policy			=	SCHED_FIFO,
	.name			=	"rt"
};
EXPORT_SYMBOL_GPL(evl_sched_rt);
