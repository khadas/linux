/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_TIMERFD_H
#define _EVENLESS_UAPI_TIMERFD_H

struct evl_timerfd_attrs {
	__u32 clockfd;
};

/* Set operation flag. */
#define EVL_TIMERFD_ABSTIME  0x1

struct evl_timerfd_setreq {
	struct itimerspec *value;
	struct itimerspec *ovalue;
};

struct evl_timerfd_getreq {
	struct itimerspec *value;
};

#define EVL_TIMERFD_IOCBASE	't'

#define EVL_TFDIOC_SET	_IOWR(EVL_TIMERFD_IOCBASE, 0, struct evl_timerfd_setreq)
#define EVL_TFDIOC_GET	_IOR(EVL_TIMERFD_IOCBASE, 1, struct evl_timerfd_getreq)

#endif /* !_EVENLESS_UAPI_TIMERFD_H */
