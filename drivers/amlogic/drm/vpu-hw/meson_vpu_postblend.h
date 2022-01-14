/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_VPU_POSTBLEND_H_
#define _MESON_VPU_POSTBLEND_H_

#define VPP_OSD1_IN_SIZE                           0x1df1
#define VPP_OSD1_BLD_H_SCOPE                       0x1df5
#define VPP_OSD1_BLD_V_SCOPE                       0x1df6
#define VPP_OSD2_BLD_H_SCOPE                       0x1df7
#define VPP_OSD2_BLD_V_SCOPE                       0x1df8

#define VD1_BLEND_SRC_CTRL                         0x1dfb
#define VD2_BLEND_SRC_CTRL                         0x1dfc
#define OSD1_BLEND_SRC_CTRL                        0x1dfd
#define OSD2_BLEND_SRC_CTRL                        0x1dfe

#define VPP_POST_BLEND_BLEND_DUMMY_DATA            0x3968
#define VPP_POST_BLEND_DUMMY_ALPHA                 0x3969

/* vpp crc */
#define VPP_RO_CRCSUM           0x1db2
#define VPP_CRC_CHK             0x1db3

#define VPP_POST_BLEND_REF_ZORDER                  128

struct postblend_reg_s {
	u32 vpp_osd1_bld_h_scope;
	u32 vpp_osd1_bld_v_scope;
	u32 vpp_osd2_bld_h_scope;
	u32 vpp_osd2_bld_v_scope;
	u32 vd1_blend_src_ctrl;
	u32 vd2_blend_src_ctrl;
	u32 osd1_blend_src_ctrl;
	u32 osd2_blend_src_ctrl;
	u32 vpp_osd1_in_size;
};

enum vpp_blend_e {
	VPP_PREBLEND = 0,
	VPP_POSTBLEND,
};

enum vpp_blend_src_e {
	VPP_NULL = 0,
	VPP_VD1,
	VPP_VD2,
	VPP_OSD1,
	VPP_OSD2
};

void set_video_zorder(u32 zorder, u32 index);
#endif
