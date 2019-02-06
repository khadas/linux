/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_UAPI_MONITOR_H
#define _EVENLESS_UAPI_MONITOR_H

#include <uapi/evenless/types.h>
#include <uapi/evenless/factory.h>

#define EVL_MONITOR_EV  0	/* Event monitor. */
#define EVL_MONITOR_PI  1	/* Gate with priority inheritance. */
#define EVL_MONITOR_PP  2	/* Gate with priority protection (ceiling). */

struct evl_monitor_attrs {
	__u32 clockfd;
	__u32 type : 2,
	      ceiling : 30;
};

/* State flags. */
#define EVL_MONITOR_SIGNALED   0x1 /* Gate/Event */
#define EVL_MONITOR_BROADCAST  0x2 /* Event */
#define EVL_MONITOR_TARGETED   0x4 /* Event */

#define EVL_MONITOR_NOGATE  -1U

struct evl_monitor_state {
	__u32 flags;
	union {
		struct {
			atomic_t owner;
			__u32 ceiling;
		} gate;
		__u32 gate_offset;
	} u;
};

struct evl_monitor_lockreq {
	struct timespec timeout;
};

struct evl_monitor_waitreq {
	struct timespec timeout;
	__u32 gatefd;
	__s32 status;
};

struct evl_monitor_unwaitreq {
	__u32 gatefd;
};

struct evl_monitor_binding {
	__u32 type;
	struct evl_element_ids eids;
};

#define EVL_MONITOR_IOCBASE	'm'

#define EVL_MONIOC_ENTER	_IOW(EVL_MONITOR_IOCBASE, 0, struct evl_monitor_lockreq)
#define EVL_MONIOC_EXIT		_IO(EVL_MONITOR_IOCBASE, 1)
#define EVL_MONIOC_WAIT		_IOWR(EVL_MONITOR_IOCBASE, 2, struct evl_monitor_waitreq)
#define EVL_MONIOC_UNWAIT	_IOWR(EVL_MONITOR_IOCBASE, 3, struct evl_monitor_unwaitreq)
#define EVL_MONIOC_BIND		_IOR(EVL_MONITOR_IOCBASE, 4, struct evl_monitor_binding)

#endif /* !_EVENLESS_UAPI_MONITOR_H */
