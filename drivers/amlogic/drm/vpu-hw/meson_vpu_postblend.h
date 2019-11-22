/*
 * drivers/amlogic/drm/vpu-hw/meson_vpu_postblend.h
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
#endif
