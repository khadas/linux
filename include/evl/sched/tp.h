/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2008, 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_SCHED_TP_H
#define _EVL_SCHED_TP_H

#ifndef _EVL_SCHED_H
#error "please don't include evl/sched/tp.h directly"
#endif

#ifdef CONFIG_EVL_SCHED_TP

#define EVL_TP_MIN_PRIO  1
#define EVL_TP_MAX_PRIO  EVL_CORE_MAX_PRIO
#define EVL_TP_NR_PRIO	 (EVL_TP_MAX_PRIO - EVL_TP_MIN_PRIO + 1)

extern struct evl_sched_class evl_sched_tp;

struct evl_tp_window {
	ktime_t w_offset;
	int w_part;
};

struct evl_tp_schedule {
	int pwin_nr;
	ktime_t tf_duration;
	atomic_t refcount;
	struct evl_tp_window pwins[0];
};

struct evl_sched_tp {
	struct evl_tp_rq {
		struct evl_multilevel_queue runnable;
	} partitions[CONFIG_EVL_SCHED_TP_NR_PART];
	struct evl_tp_rq idle;
	struct evl_tp_rq *tps;
	struct evl_timer tf_timer;
	struct evl_tp_schedule *gps;
	int wnext;
	ktime_t tf_start;
	struct list_head threads;
};

static inline int evl_tp_init_thread(struct evl_thread *thread)
{
	thread->tps = NULL;

	return 0;
}

#endif /* !CONFIG_EVL_SCHED_TP */

#endif /* !_EVL_SCHED_TP_H */
