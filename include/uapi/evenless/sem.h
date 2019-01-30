/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_SEM_H
#define _EVENLESS_UAPI_SEM_H

#include <uapi/evenless/factory.h>

struct timespec;

#define EVL_SEM_VALUE_MAX	INT_MAX

/* Attribute flags. */
#define EVL_SEM_FIFO      0x0
#define EVL_SEM_PRIO      0x1
#define EVL_SEM_PULSE     0x4

struct evl_sem_attrs {
	__u32 clockfd;
	__u32 flags;
	__s32 initval;
};

struct evl_sem_state {
	atomic_t value;
	__u32 flags;
};

struct evl_sem_waitreq {
	struct timespec timeout;
	__s32 count;
};

#define EVL_SEM_IOCBASE	's'

#define EVL_SEMIOC_GET		_IOW(EVL_SEM_IOCBASE, 0, __u32)
#define EVL_SEMIOC_PUT		_IOW(EVL_SEM_IOCBASE, 1, __u32)
#define EVL_SEMIOC_BCAST	_IO(EVL_SEM_IOCBASE, 2)
#define EVL_SEMIOC_BIND		_IOR(EVL_SEM_IOCBASE, 3, struct evl_element_ids)

#endif /* !_EVENLESS_UAPI_SEM_H */
