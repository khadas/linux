/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_FACTORY_H
#define _EVL_UAPI_FACTORY_H

#include <linux/types.h>

#define EVL_FACTORY_IOCBASE	'f'

struct evl_element_ids {
	__u32 minor;
	__u32 fundle;
	__u32 state_offset;
};

/* The core only uses bits 16-31, rest is available to libevl. */
#define EVL_CLONE_PUBLIC	(1 << 16)
#define EVL_CLONE_PRIVATE	(0 << 16)
#define EVL_CLONE_OBSERVABLE	(1 << 17)
#define EVL_CLONE_NONBLOCK	(1 << 18)
#define EVL_CLONE_MASTER	(1 << 19)
#define EVL_CLONE_COREDEV	(1 << 31)
#define EVL_CLONE_MASK		(((__u32)-1 << 16) & ~EVL_CLONE_COREDEV)

struct evl_clone_req {
	__u64 name_ptr;		/* (const char *name) */
	__u64 attrs_ptr;	/* (void *attrs) */
	__u32 clone_flags;
	/* Output on success: */
	struct evl_element_ids eids;
	__u32 efd;
};

#define EVL_IOC_CLONE	_IOWR(EVL_FACTORY_IOCBASE, 0, struct evl_clone_req)

#endif /* !_EVL_UAPI_FACTORY_H */
