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

struct evl_clone_req {
	const char *name;
	void *attrs;
	struct evl_element_ids eids;
};

#define EVL_IOC_CLONE	_IOWR(EVL_FACTORY_IOCBASE, 0, struct evl_clone_req)

#endif /* !_EVL_UAPI_FACTORY_H */
