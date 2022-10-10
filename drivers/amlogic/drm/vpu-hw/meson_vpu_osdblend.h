/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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

#define VIU_OSD_BLEND_CTRL_S5                      0x60b0
#define VIU_OSD_BLEND_DIN0_SCOPE_H_S5              0x60b1
#define VIU_OSD_BLEND_DIN0_SCOPE_V_S5              0x60b2
#define VIU_OSD_BLEND_DIN1_SCOPE_H_S5              0x60b3
#define VIU_OSD_BLEND_DIN1_SCOPE_V_S5              0x60b4
#define VIU_OSD_BLEND_DIN2_SCOPE_H_S5              0x60b5
#define VIU_OSD_BLEND_DIN2_SCOPE_V_S5              0x60b6
#define VIU_OSD_BLEND_DIN3_SCOPE_H_S5              0x60b7
#define VIU_OSD_BLEND_DIN3_SCOPE_V_S5              0x60b8
#define VIU_OSD_BLEND_DUMMY_DATA0_S5               0x60b9
#define VIU_OSD_BLEND_DUMMY_ALPHA_S5               0x60ba
#define VIU_OSD_BLEND_BLEND0_SIZE_S5               0x60bb
#define VIU_OSD_BLEND_BLEND1_SIZE_S5               0x60bc
#define VIU_OSD_BLEND_RO_CURRENT_XY_S5             0x60bf
#define VIU_OSD_BLEND_CTRL1_S5                     0x60c0
#define VIU_OSD_HOLD_LINE_HIGH_BITS_S5             0x60c1

/* add for osd dv core2 */
#define DOLBY_CORE2A_SWAP_CTRL1	                   0x3434
#define DOLBY_CORE2A_SWAP_CTRL2	                   0x3435
#define DOLBY_CORE2A_SWAP_CTRL1_S5                 0x0b34
#define DOLBY_CORE2A_SWAP_CTRL2_S5                 0x0b35

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
	OSD_CHANNEL4,
	OSD_CHANNEL_NUM = 0,
};

struct osd_dummy_data_s {
	unsigned int channel0;/*y*/
	unsigned int channel1;/*cb*/
	unsigned int channel2;/*cr*/
};
#endif
