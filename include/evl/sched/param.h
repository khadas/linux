/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_SCHED_PARAM_H
#define _EVL_SCHED_PARAM_H

struct evl_idle_param {
	int prio;
};

struct evl_weak_param {
	int prio;
};

struct evl_fifo_param {
	int prio;
};

struct evl_quota_param {
	int prio;
	int tgid;	/* thread group id. */
};

struct evl_tp_param {
	int prio;
	int ptid;	/* partition id. */
};

union evl_sched_param {
	struct evl_idle_param idle;
	struct evl_fifo_param fifo;
	struct evl_weak_param weak;
#ifdef CONFIG_EVL_SCHED_QUOTA
	struct evl_quota_param quota;
#endif
#ifdef CONFIG_EVL_SCHED_TP
	struct evl_tp_param tp;
#endif
};

#endif /* !_EVL_SCHED_PARAM_H */
