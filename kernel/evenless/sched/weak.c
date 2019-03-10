/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2013, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evenless/sched.h>
#include <uapi/evenless/sched.h>

static void weak_init(struct evl_rq *rq)
{
	evl_init_schedq(&rq->weak.runnable);
}

static void weak_requeue(struct evl_thread *thread)
{
	evl_add_schedq(&thread->rq->weak.runnable, thread);
}

static void weak_enqueue(struct evl_thread *thread)
{
	evl_add_schedq_tail(&thread->rq->weak.runnable, thread);
}

static void weak_dequeue(struct evl_thread *thread)
{
	evl_del_schedq(&thread->rq->weak.runnable, thread);
}

static struct evl_thread *weak_pick(struct evl_rq *rq)
{
	return evl_get_schedq(&rq->weak.runnable);
}

static int weak_chkparam(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	if (p->weak.prio < EVL_WEAK_MIN_PRIO ||
		p->weak.prio > EVL_WEAK_MAX_PRIO)
		return -EINVAL;

	return 0;
}

static bool weak_setparam(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	if (!(thread->state & T_BOOST))
		thread->state |= T_WEAK;

	return evl_set_effective_thread_priority(thread, p->weak.prio);
}

static void weak_getparam(struct evl_thread *thread,
			union evl_sched_param *p)
{
	p->weak.prio = thread->cprio;
}

static void weak_trackprio(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	if (p)
		thread->cprio = p->weak.prio;
	else
		thread->cprio = thread->bprio;
}

static void weak_ceilprio(struct evl_thread *thread, int prio)
{
	if (prio > EVL_WEAK_MAX_PRIO)
		prio = EVL_WEAK_MAX_PRIO;

	thread->cprio = prio;
}

static int weak_declare(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	if (p->weak.prio < EVL_WEAK_MIN_PRIO ||
		p->weak.prio > EVL_WEAK_MAX_PRIO)
		return -EINVAL;

	return 0;
}

struct evl_sched_class evl_sched_weak = {
	.sched_init		=	weak_init,
	.sched_enqueue		=	weak_enqueue,
	.sched_dequeue		=	weak_dequeue,
	.sched_requeue		=	weak_requeue,
	.sched_pick		=	weak_pick,
	.sched_declare		=	weak_declare,
	.sched_chkparam		=	weak_chkparam,
	.sched_setparam		=	weak_setparam,
	.sched_trackprio	=	weak_trackprio,
	.sched_ceilprio		=	weak_ceilprio,
	.sched_getparam		=	weak_getparam,
	.weight			=	EVL_CLASS_WEIGHT(1),
	.policy			=	SCHED_WEAK,
	.name			=	"weak"
};
EXPORT_SYMBOL_GPL(evl_sched_weak);
