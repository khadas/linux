/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2013, 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_TYPES_H
#define _EVL_UAPI_TYPES_H

#include <linux/types.h>

typedef __u32 fundle_t;

#define EVL_NO_HANDLE		((fundle_t)0x00000000)

/* Reserved status bits */
#define EVL_MUTEX_FLCLAIM	((fundle_t)0x80000000) /* Contended. */
#define EVL_MUTEX_FLCEIL	((fundle_t)0x40000000) /* Ceiling active. */
#define EVL_HANDLE_INDEX_MASK	(EVL_MUTEX_FLCLAIM|EVL_MUTEX_FLCEIL)

/*
 * Strip all reserved bits from the handle, only retaining the fast
 * index value.
 */
static inline fundle_t evl_get_index(fundle_t handle)
{
	return handle & ~EVL_HANDLE_INDEX_MASK;
}

/*
 * Y2038 safety. Match the kernel ABI definitions of __kernel_timespec
 * and __kernel_itimerspec.
 */
typedef long long __evl_time64_t;

struct __evl_timespec {
	__evl_time64_t tv_sec;
	long long      tv_nsec;
};

struct __evl_itimerspec {
	struct __evl_timespec it_interval;
	struct __evl_timespec it_value;
};

union evl_value {
	__s32 val;
	__s64 lval;
	void *ptr;
};

#define evl_intval(__val)	((union evl_value){ .lval = (__val) })
#define evl_ptrval(__ptr)	((union evl_value){ .ptr = (__ptr) })
#define evl_nil			evl_intval(0)

#endif /* !_EVL_UAPI_TYPES_H */
