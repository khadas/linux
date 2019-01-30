/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <evenless/sched.h>

static struct evl_thread *evl_idle_pick(struct evl_rq *rq)
{
	return &rq->root_thread;
}

static bool evl_idle_setparam(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	return __evl_set_idle_schedparam(thread, p);
}

static void evl_idle_getparam(struct evl_thread *thread,
			union evl_sched_param *p)
{
	__evl_get_idle_schedparam(thread, p);
}

static void evl_idle_trackprio(struct evl_thread *thread,
			const union evl_sched_param *p)
{
	__evl_track_idle_priority(thread, p);
}

static void evl_idle_ceilprio(struct evl_thread *thread, int prio)
{
	__evl_ceil_idle_priority(thread, prio);
}

struct evl_sched_class evl_sched_idle = {
	.sched_pick		=	evl_idle_pick,
	.sched_setparam		=	evl_idle_setparam,
	.sched_getparam		=	evl_idle_getparam,
	.sched_trackprio	=	evl_idle_trackprio,
	.sched_ceilprio		=	evl_idle_ceilprio,
	.weight			=	EVL_CLASS_WEIGHT(0),
	.policy			=	SCHED_IDLE,
	.name			=	"idle"
};
