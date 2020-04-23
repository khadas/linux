/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_OBSERVABLE_H
#define _EVL_UAPI_OBSERVABLE_H

#include <uapi/evl/types.h>

#define EVL_OBSERVABLE_DEV	"observable"

/* __evl_notification.flags */
#define EVL_NOTIFY_ALWAYS	(0 << 0)
#define EVL_NOTIFY_ONCHANGE	(1 << 0)
#define EVL_NOTIFY_MASK		EVL_NOTIFY_ONCHANGE

struct evl_notice {
	__u32 tag;
	union evl_value event;
};

/* Notice tags below this value are reserved to the core. */
#define EVL_NOTICE_USER  64

struct evl_subscription {
	__u32 backlog_count;
	__u32 flags;
};

struct __evl_notification {
	__u32 tag;
	__u32 serial;
	__s32 issuer;
	union evl_value event;
	struct __evl_timespec date;
};

#define EVL_OBSERVABLE_IOCBASE	'o'

#define EVL_OBSIOC_SUBSCRIBE		_IOW(EVL_OBSERVABLE_IOCBASE, 0, struct evl_subscription)
#define EVL_OBSIOC_UNSUBSCRIBE		_IO(EVL_OBSERVABLE_IOCBASE, 1)

#endif /* !_EVL_UAPI_OBSERVABLE_H */
