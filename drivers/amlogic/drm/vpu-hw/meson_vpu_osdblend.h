/*
 * drivers/amlogic/drm/vpu-hw/meson_vpu_osdblend.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _MESON_VPU_OSDBLEND_H_
#define _MESON_VPU_OSDBLEND_H_

#define OSD_LEND_OUT_PORT0 0
#define OSD_LEND_OUT_PORT1 1
#define OSD_SUB_BLEND0 0
#define OSD_SUB_BLEND1 1
#define OSD_LEND_MAX_IN_NUM_PORT0 4
#define OSD_LEND_MAX_IN_NUM_PORT1 2

#define VIU_OSD_BLEND_CTRL                         0x39b0
#define VIU_OSD_BLEND_DIN0_SCOPE_H                 0x39b1
#define VIU_OSD_BLEND_DIN0_SCOPE_V                 0x39b2
#define VIU_OSD_BLEND_DIN1_SCOPE_H                 0x39b3
#define VIU_OSD_BLEND_DIN1_SCOPE_V                 0x39b4
#define VIU_OSD_BLEND_DIN2_SCOPE_H                 0x39b5
#define VIU_OSD_BLEND_DIN2_SCOPE_V                 0x39b6
#define VIU_OSD_BLEND_DIN3_SCOPE_H                 0x39b7
#define VIU_OSD_BLEND_DIN3_SCOPE_V                 0x39b8
#define VIU_OSD_BLEND_DUMMY_DATA0                  0x39b9
#define VIU_OSD_BLEND_DUMMY_ALPHA                  0x39ba
#define VIU_OSD_BLEND_BLEND0_SIZE                  0x39bb
#define VIU_OSD_BLEND_BLEND1_SIZE                  0x39bc
#define VIU_OSD_BLEND_RO_CURRENT_XY                0x39bf
#define VIU_OSD_BLEND_CTRL1                        0x39c0

struct osdblend_reg_s {
	u32 viu_osd_blend_ctrl;
	u32 viu_osd_blend_din0_scope_h;
	u32 viu_osd_blend_din0_scope_v;
	u32 viu_osd_blend_din1_scope_h;
	u32 viu_osd_blend_din1_scope_v;
	u32 viu_osd_blend_din2_scope_h;
	u32 viu_osd_blend_din2_scope_v;
	u32 viu_osd_blend_din3_scope_h;
	u32 viu_osd_blend_din3_scope_v;
	u32 viu_osd_blend_dummy_data0;
	u32 viu_osd_blend_dummy_alpha;
	u32 viu_osd_blend0_size;
	u32 viu_osd_blend1_size;
	u32 viu_osd_blend_ctrl1;
};

/*input mif channel*/
enum osd_channel_e {
	OSD_CHANNEL1 = 1,
	OSD_CHANNEL2,
	OSD_CHANNEL3,
	OSD_CHANNEL_NUM
};

struct osd_dummy_data_s {
	unsigned int channel0;/*y*/
	unsigned int channel1;/*cb*/
	unsigned int channel2;/*cr*/
};
#endif
