/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_POLLER_H
#define _EVENLESS_UAPI_POLLER_H

#define EVL_POLLER_IOCBASE	'p'

#define EVL_POLLER_CTLADD   0
#define EVL_POLLER_CTLDEL   1
#define EVL_POLLER_CTLMOD   2

struct evl_poller_ctlreq {
	__u32 action;
	__u32 fd;
	__u32 events;
};

struct evl_poll_event {
	__u32 fd;
	__u32 events;
};

struct evl_poller_waitreq {
	struct timespec timeout;
	struct evl_poll_event *pollset;
	int nrset;
};

#define EVL_POLIOC_CTL	_IOW(EVL_POLLER_IOCBASE, 0, struct evl_poller_ctlreq)
#define EVL_POLIOC_WAIT	_IOWR(EVL_POLLER_IOCBASE, 1, struct evl_poller_waitreq)

#endif /* !_EVENLESS_UAPI_POLLER_H */
