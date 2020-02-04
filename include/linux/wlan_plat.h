/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _LINUX_WLAN_PLAT_H_
#define _LINUX_WLAN_PLAT_H_

#define WLAN_PLAT_NODFS_FLAG	0x01

struct wifi_platform_data {
	int (*set_power)(int val);
	int (*set_reset)(int val);
	int (*set_carddetect)(int val);
	void *(*mem_prealloc)(int section, unsigned long size);
	int (*get_mac_addr)(unsigned char *buf);
	int (*get_wake_irq)(void);
	void *(*get_country_code)(char *ccode, u32 flags);
};

#endif
