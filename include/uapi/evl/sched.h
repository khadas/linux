/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2005, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_SCHED_H
#define _EVL_UAPI_SCHED_H

#define SCHED_EVL	42
#define SCHED_WEAK	43

#define sched_rr_quantum	sched_u.rr.__sched_rr_quantum

struct __sched_rr_param {
	struct timespec __sched_rr_quantum;
};

#define SCHED_QUOTA		44
#define sched_quota_group	sched_u.quota.__sched_group

struct __sched_quota_param {
	int __sched_group;
};

struct evl_quota_ctlparam {
	enum {
		evl_quota_add,
		evl_quota_remove,
		evl_quota_force_remove,
		evl_quota_set,
		evl_quota_get,
	} op;
	union {
		struct {
			int tgid;
		} remove;
		struct {
			int tgid;
			int quota;
			int quota_peak;
		} set;
		struct {
			int tgid;
		} get;
	} u;
};

struct evl_quota_ctlinfo {
	int tgid;
	int quota;
	int quota_peak;
	int quota_sum;
};

#define SCHED_TP		45
#define sched_tp_partition	sched_u.tp.__sched_partition

struct __sched_tp_param {
	int __sched_partition;
};

#define EVL_TP_IDLE	-1	/* Idle pseudo-partition */

struct evl_tp_ctlparam {
	enum {
		evl_install_tp,
		evl_uninstall_tp,
		evl_start_tp,
		evl_stop_tp,
		evl_get_tp,
	} op;
	int nr_windows;
	struct __sched_tp_window {
		struct timespec offset;
		struct timespec duration;
		int ptid;
	} windows[0];
};

struct evl_tp_ctlinfo {
	int nr_windows;
	struct __sched_tp_window windows[0];
};

#define evl_tp_paramlen(__p)	\
	(sizeof(*__p) + (__p)->nr_windows * sizeof((__p)->windows))

struct evl_sched_attrs {
	int sched_policy;
	int sched_priority;
	union {
		struct __sched_rr_param rr;
		struct __sched_quota_param quota;
		struct __sched_tp_param tp;
	} sched_u;
};

union evl_sched_ctlparam {
	struct evl_quota_ctlparam quota;
	struct evl_tp_ctlparam tp;
};

union evl_sched_ctlinfo {
	struct evl_quota_ctlinfo quota;
	struct evl_tp_ctlinfo tp;
};

struct evl_sched_ctlreq {
	int policy;
	int cpu;
	union evl_sched_ctlparam *param;
	union evl_sched_ctlinfo *info;
};

#endif /* !_EVL_UAPI_SCHED_H */
