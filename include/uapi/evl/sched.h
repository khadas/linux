/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2005, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_SCHED_H
#define _EVL_UAPI_SCHED_H

#include <uapi/evl/types.h>

#define EVL_CPU_OOB      (1 << 0)
#define EVL_CPU_ISOL     (1 << 1)
#define EVL_CPU_OFFLINE  (1 << 2)

#define SCHED_WEAK	43

#define sched_rr_quantum	sched_u.rr.__sched_rr_quantum

struct __evl_rr_param {
	struct __evl_timespec __sched_rr_quantum;
};

#define SCHED_QUOTA		44
#define sched_quota_group	sched_u.quota.__sched_group

struct __evl_quota_param {
	int __sched_group;
};

enum evl_quota_ctlop {
	evl_quota_add,
	evl_quota_remove,
	evl_quota_force_remove,
	evl_quota_set,
	evl_quota_get,
};

struct evl_quota_ctlparam {
	enum evl_quota_ctlop op;
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

struct __evl_tp_param {
	int __sched_partition;
};

#define EVL_TP_IDLE	-1	/* Idle pseudo-partition */

struct __evl_tp_window {
	struct __evl_timespec offset;
	struct __evl_timespec duration;
	int ptid;
};

enum evl_tp_ctlop {
	evl_tp_install,
	evl_tp_uninstall,
	evl_tp_start,
	evl_tp_stop,
	evl_tp_get,
};

struct evl_tp_ctlparam {
	enum evl_tp_ctlop op;
	int nr_windows;
	struct __evl_tp_window windows[0];
};

struct evl_tp_ctlinfo {
	int nr_windows;
	struct __evl_tp_window windows[0];
};

#define evl_tp_paramlen(__nr_windows)		\
	(sizeof(struct evl_tp_ctlparam) +	\
		__nr_windows * sizeof(struct __evl_tp_window))

struct evl_sched_attrs {
	int sched_policy;
	int sched_priority;
	union {
		struct __evl_rr_param rr;
		struct __evl_quota_param quota;
		struct __evl_tp_param tp;
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
	__u64 param_ptr; /* (const union evl_sched_ctlparam *param) */
	__u64 info_ptr;	 /* (union evl_sched_ctlinfo *info) */
};

#endif /* !_EVL_UAPI_SCHED_H */
