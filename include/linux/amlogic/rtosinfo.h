/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _RTOSINFO_H_
#define _RTOSINFO_H_

#define VERSION_1_0	0x10

enum ertosstate {
	ertosstat_off = 0,
	ertosstat_initializing,
	ertosstat_working,
	ertosstat_done
};

struct xrtosinfo_t {
	u32	version;
	u32	status;
	u32	cpumask;
	u32	flags;
	u32	logbuf_len;
	u32	logbuf_phy;
};

#endif

