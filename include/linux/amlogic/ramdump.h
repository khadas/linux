/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __RAMDUMP_H__
#define __RAMDUMP_H__

#define SET_REBOOT_REASON		0x82000049

#define AMLOGIC_KERNEL_BOOTED		0x8000
#define RAMDUMP_STICKY_DATA_MASK	0xFFFF

void ramdump_sync_data(void);

#endif /* __RAMDUMP_H__ */
