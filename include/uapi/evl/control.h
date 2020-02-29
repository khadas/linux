/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_CONTROL_H
#define _EVL_UAPI_CONTROL_H

#include <linux/types.h>
#include <uapi/evl/sched.h>

/* Earliest ABI level we support. */
#define EVL_ABI_BASE   19
/*
 * Current/latest ABI level we support. We may decouple the base and
 * current ABI levels by providing backward compatibility from the
 * latter to the former. CAUTION: a litteral value is required for the
 * current ABI definition (scripts reading this may be naive).
 */
#define EVL_ABI_LEVEL  19

#define EVL_CONTROL_DEV  "/dev/evl/control"

struct evl_core_info {
	__u32 abi_base;
	__u32 abi_current;
	__u32 fpu_features;
	__u64 shm_size;
};

struct evl_cpu_state {
	__u32 cpu;
	__u32 *state;
};

#define EVL_CONTROL_IOCBASE	'C'

#define EVL_CTLIOC_GET_COREINFO		_IOR(EVL_CONTROL_IOCBASE, 0, struct evl_core_info)
#define EVL_CTLIOC_SCHEDCTL		_IOWR(EVL_CONTROL_IOCBASE, 1, struct evl_sched_ctlreq)
#define EVL_CTLIOC_GET_CPUSTATE		_IOR(EVL_CONTROL_IOCBASE, 2, struct evl_cpu_state)

#endif /* !_EVL_UAPI_CONTROL_H */
