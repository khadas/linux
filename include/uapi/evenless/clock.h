/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_CLOCK_H
#define _EVENLESS_UAPI_CLOCK_H

#define EVL_CLOCK_MONOTONIC  (-CLOCK_MONOTONIC)
#define EVL_CLOCK_REALTIME   (-CLOCK_REALTIME)

#define EVL_CLOCK_IOCBASE	'c'

struct evl_clock_delayreq {
	struct timespec timeout;
	struct timespec *remain;
};

#define EVL_CLKIOC_DELAY	_IOWR(EVL_CLOCK_IOCBASE, 0, struct evl_clock_delayreq)
#define EVL_CLKIOC_GET_RES	_IOR(EVL_CLOCK_IOCBASE, 1, struct timespec)
#define EVL_CLKIOC_GET_TIME	_IOR(EVL_CLOCK_IOCBASE, 2, struct timespec)
#define EVL_CLKIOC_SET_TIME	_IOR(EVL_CLOCK_IOCBASE, 3, struct timespec)
#define EVL_CLKIOC_ADJ_TIME	_IOR(EVL_CLOCK_IOCBASE, 4, struct timex)

#endif /* !_EVENLESS_UAPI_CLOCK_H */
