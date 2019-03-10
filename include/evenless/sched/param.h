/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2008, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_SCHED_PARAM_H
#define _EVENLESS_SCHED_PARAM_H

struct evl_idle_param {
	int prio;
};

struct evl_weak_param {
	int prio;
};

struct evl_rt_param {
	int prio;
};

struct evl_quota_param {
	int prio;
	int tgid;	/* thread group id. */
};

union evl_sched_param {
	struct evl_idle_param idle;
	struct evl_rt_param rt;
	struct evl_weak_param weak;
#ifdef CONFIG_EVENLESS_SCHED_QUOTA
	struct evl_quota_param quota;
#endif
};

#endif /* !_EVENLESS_SCHED_PARAM_H */
