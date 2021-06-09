/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_H__
#define __HDMI_H__
enum hdcp_ver_e {
	HDCPVER_NONE = 0,
	HDCPVER_14,
	HDCPVER_22,
};

#define MAX_KSV_LISTS 127
struct hdcprp14_topo {
	unsigned char max_cascade_exceeded:1;
	unsigned char depth:3;
	unsigned char rsvd : 4;
	unsigned char max_devs_exceeded:1;
	unsigned char device_count:7; /* 1 ~ 127 */
	unsigned char ksv_list[MAX_KSV_LISTS * 5];
} __packed;

struct hdcprp22_topo {
	unsigned int depth;
	unsigned int device_count;
	unsigned int v1_X_device_down;
	unsigned int v2_0_repeater_down;
	unsigned int max_devs_exceeded;
	unsigned int max_cascade_exceeded;
	unsigned char id_num;
	unsigned char id_lists[MAX_KSV_LISTS * 5];
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

