/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_HDCP_H__
#define __HDMI_HDCP_H__

enum hdcp_ver_e {
	HDCPVER_NONE = 0,
	HDCPVER_14,
	HDCPVER_22,
};

#define MAX_KSV_LISTS 127
struct hdcprp14_topo {
	u8 max_cascade_exceeded:1;
	u8 depth:3;
	u8 rsvd : 4;
	u8 max_devs_exceeded:1;
	u8 device_count:7; /* 1 ~ 127 */
	u8 ksv_list[MAX_KSV_LISTS * 5];
} __packed;

struct hdcprp22_topo {
	u32 depth;
	u32 device_count;
	u32 v1_X_device_down;
	u32 v2_0_repeater_down;
	u32 max_devs_exceeded;
	u32 max_cascade_exceeded;
	u8 id_num;
	u8 id_lists[MAX_KSV_LISTS * 5];
};

struct hdcprp_topo {
	/* hdcp_ver currently used */
	enum hdcp_ver_e hdcp_ver;
	union {
		struct hdcprp14_topo topo14;
		struct hdcprp22_topo topo22;
	} topo;
};

#endif
