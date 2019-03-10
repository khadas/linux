/*
 * SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
 *
 * Derived from Xenomai Cobalt's autotune driver, https://xenomai.org/
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_UAPI_DEVICES_LATMUS_H
#define _EVL_UAPI_DEVICES_LATMUS_H

/* Latmus context types. */
#define EVL_LAT_IRQ   0
#define EVL_LAT_KERN  1
#define EVL_LAT_USER  2

struct latmus_setup {
	__u32 type;
	__u64 period;
	__s32 priority;
	__u32 cpu;
	union {
		struct {
			__u32 verbosity;
		} tune;
		struct {
			__u32 xfd;
			__u32 hcells;
		} measure;
	} u;
};

struct latmus_result {
	__s32 *data;
	__u32 len;
};

/*
 * The measurement record which the driver sends to userland each
 * second through an xbuf channel.
 */
struct latmus_measurement {
	__u64 sum_lat;
	__s32 min_lat;
	__s32 max_lat;
	__u32 overruns;
	__u32 samples;
};

#define EVL_LATMUS_IOCBASE	'L'

#define EVL_LATIOC_TUNE		_IOWR(EVL_LATMUS_IOCBASE, 0, struct latmus_setup)
#define EVL_LATIOC_MEASURE	_IOWR(EVL_LATMUS_IOCBASE, 1, struct latmus_setup)
#define EVL_LATIOC_RUN		_IOR(EVL_LATMUS_IOCBASE, 2, struct latmus_result)
#define EVL_LATIOC_PULSE	_IOW(EVL_LATMUS_IOCBASE, 3, __u64)
#define EVL_LATIOC_RESET	_IO(EVL_LATMUS_IOCBASE, 4)

#endif /* !_EVL_UAPI_DEVICES_LATMUS_H */
