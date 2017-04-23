/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Based on the Xenomai Cobalt core:
 * Copyright (C) 2005, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_SCHED_H
#define _EVENLESS_UAPI_SCHED_H

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

struct evl_sched_attrs {
	int sched_policy;
	int sched_priority;
	union {
		struct __sched_rr_param rr;
		struct __sched_quota_param quota;
	} sched_u;
};

#endif /* !_EVENLESS_UAPI_SCHED_H */
