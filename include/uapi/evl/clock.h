/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_CLOCK_H
#define _EVL_UAPI_CLOCK_H

#define EVL_CLOCK_MONOTONIC_DEV		"monotonic"
#define EVL_CLOCK_REALTIME_DEV		"realtime"
#define EVL_CLOCK_DEV			"clock"

#define EVL_CLOCK_MONOTONIC  (-CLOCK_MONOTONIC)
#define EVL_CLOCK_REALTIME   (-CLOCK_REALTIME)

#define EVL_CLOCK_IOCBASE	'c'

struct evl_clock_sleepreq {
	struct timespec timeout;
};

#define EVL_CLKIOC_SLEEP	_IOWR(EVL_CLOCK_IOCBASE, 0, struct evl_clock_sleepreq)
#define EVL_CLKIOC_GET_RES	_IOR(EVL_CLOCK_IOCBASE, 1, struct timespec)
#define EVL_CLKIOC_GET_TIME	_IOR(EVL_CLOCK_IOCBASE, 2, struct timespec)
#define EVL_CLKIOC_SET_TIME	_IOR(EVL_CLOCK_IOCBASE, 3, struct timespec)
#define EVL_CLKIOC_ADJ_TIME	_IOR(EVL_CLOCK_IOCBASE, 4, struct timex)
#define EVL_CLKIOC_NEW_TIMER	_IO(EVL_CLOCK_IOCBASE, 5)

/* Set operation flag for timers. */
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

#endif /* !_EVL_UAPI_CLOCK_H */
