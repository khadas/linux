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

/* viu global ctrl */
#define VPP_OSD1_SCALE_CTRL                        0x1a73
#define VPP_OSD2_SCALE_CTRL                        0x1a74
#define VPP_OSD3_SCALE_CTRL                        0x1a75
#define VPP_OSD4_SCALE_CTRL                        0x1a76

#define VPP_VD1_DSC_CTRL                           0x1a83
#define VPP_VD2_DSC_CTRL                           0x1a84
#define VPP_VD3_DSC_CTRL                           0x1a85

#define MALI_AFBCD1_TOP_CTRL                       0x1a55
#define MALI_AFBCD2_TOP_CTRL                       0x1a56
#define PATH_START_SEL                             0x1a8a

#define VIU_VD1_PATH_CTRL                          0x1a73
#define VIU_VD2_PATH_CTRL                          0x1a74

#define VIU_OSD1_PATH_CTRL                         0x1a76
#define VIU_OSD2_PATH_CTRL                         0x1a77
#define VIU_OSD3_PATH_CTRL                         0x1a78

#define OSD1_HDR_IN_SIZE                           0x1a5a
#define OSD2_HDR_IN_SIZE                           0x1a5b
#define OSD3_HDR_IN_SIZE                           0x1a5c
#define OSD4_HDR_IN_SIZE                           0x1a5d
#define OSD1_HDR2_CTRL                             0x38a0
#define OSD2_HDR2_CTRL                             0x5b00
#define OSD3_HDR2_CTRL                             0x5b50
#define OSD4_HDR2_CTRL                             0x5ba0
#define OSD2_HDR2_MATRIXI_EN_CTRL                  0x5b3b
#define OSD2_HDR2_MATRIXO_EN_CTRL                  0x5b3c
#define OSD3_HDR2_MATRIXI_EN_CTRL                  0x5b8b
#define OSD3_HDR2_MATRIXO_EN_CTRL                  0x5b8c
#define OSD4_HDR2_MATRIXI_EN_CTRL                  0x5bdb

/* vpp1 & vpp2 */
#define VPP1_BLD_CTRL                              0x5985
#define VPP1_BLD_OUT_SIZE                          0x5986
#define VPP1_BLD_DIN0_HSCOPE                       0x5987
#define VPP1_BLD_DIN0_VSCOPE                       0x5988
#define VPP1_BLD_DIN1_HSCOPE                       0x5989
#define VPP1_BLD_DIN1_VSCOPE                       0x598a
#define VPP1_BLD_DIN2_HSCOPE                       0x598b
#define VPP1_BLD_DIN2_VSCOPE                       0x598c
#define VPP1_BLEND_BLEND_DUMMY_DATA                0x59a9
#define VPP1_BLEND_DUMMY_ALPHA                     0x59aa

#define VPP2_BLD_CTRL                              0x59c5
#define VPP2_BLD_OUT_SIZE                          0x59c6
#define VPP2_BLD_DIN0_HSCOPE                       0x59c7
#define VPP2_BLD_DIN0_VSCOPE                       0x59c8
#define VPP2_BLD_DIN1_HSCOPE                       0x59c9
#define VPP2_BLD_DIN1_VSCOPE                       0x59ca
#define VPP2_BLD_DIN2_HSCOPE                       0x59cb
#define VPP2_BLD_DIN2_VSCOPE                       0x59cc
#define VPP2_BLEND_BLEND_DUMMY_DATA                0x59e9
#define VPP2_BLEND_DUMMY_ALPHA                     0x59ea

/* S5 vpp register */
#define VPP_OSD1_BLD_H_SCOPE_S5                       0x1d09
#define VPP_OSD1_BLD_V_SCOPE_S5                      0x1d0a
#define VPP_OSD2_BLD_H_SCOPE_S5                       0x1d0b
#define VPP_OSD2_BLD_V_SCOPE_S5                       0x1d0c

#define VD1_BLEND_SRC_CTRL_S5                         0x1d0d
#define VD2_BLEND_SRC_CTRL_S5                         0x1d0e
#define OSD1_BLEND_SRC_CTRL_S5                        0x1d10
#define OSD2_BLEND_SRC_CTRL_S5                        0x1d11

#define VPP_POST_BLEND_BLEND_DUMMY_DATA_S5            0x1d1a
#define VPP_POST_BLEND_DUMMY_ALPHA_S5                 0x1d1b
#define VPP_POSTBLND_CTRL_S5                          0x1d02

#define VPP_INTF_OSD3_CTRL             0x4107

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
	VPP_OSD2,
};

struct postblend1_reg_s {
	u32 vpp_bld_din0_hscope;
	u32 vpp_bld_din0_vscope;
	u32 vpp_bld_din1_hscope;
	u32 vpp_bld_din1_vscope;
	u32 vpp_bld_din2_hscope;
	u32 vpp_bld_din2_vscope;
	u32 vpp_bld_ctrl;
	u32 vpp_bld_out_size;
	u32 vpp_bld_dummy_data;
	u32 vpp_bld_dummy_alpha;
};

enum vpp_blend_5mux_e {
	VPP_5MUX_NULL = 0,
	VPP_5MUX_VD1,
	VPP_5MUX_VD2,
	VPP_5MUX_VD3,
	VPP_5MUX_OSD1,
	VPP_5MUX_OSD2,
};

void set_video_zorder(u32 zorder, u32 index);
#endif
