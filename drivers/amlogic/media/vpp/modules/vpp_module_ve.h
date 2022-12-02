/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_VE_H__
#define __VPP_MODULE_VE_H__

enum ve_blkext_param_e {
	EN_BLKEXT_START = 0,
	EN_BLKEXT_SLOPE1,
	EN_BLKEXT_MIDPT,
	EN_BLKEXT_SLOPE2,
	EN_BLKEXT_MAX,
};

enum ve_ccoring_param_e {
	EN_CCORING_SLOPE = 0,
	EN_CCORING_TH,
	EN_CCORING_BYPASS_YTH,
	EN_CCORING_MAX,
};

enum ve_blue_stretch_param_e {
	EN_BLUE_STRETCH_EN_SEL = 0,
	EN_BLUE_STRETCH_CB_INC,
	EN_BLUE_STRETCH_CR_INC,
	EN_BLUE_STRETCH_GAIN,
	EN_BLUE_STRETCH_GAIN_CB4CR,
	EN_BLUE_STRETCH_LUMA_HIGH,
	EN_BLUE_STRETCH_ERR_CRP,
	EN_BLUE_STRETCH_ERR_SIGN3,
	EN_BLUE_STRETCH_ERR_CRP_INV,
	EN_BLUE_STRETCH_ERR_CRN,
	EN_BLUE_STRETCH_ERR_SIGN2,
	EN_BLUE_STRETCH_ERR_CRN_INV,
	EN_BLUE_STRETCH_ERR_CBP,
	EN_BLUE_STRETCH_ERR_SIGN1,
	EN_BLUE_STRETCH_ERR_CBP_INV,
	EN_BLUE_STRETCH_ERR_CBN,
	EN_BLUE_STRETCH_ERR_SIGN0,
	EN_BLUE_STRETCH_ERR_CBN_INV,
	EN_BLUE_STRETCH_MAX,
};

enum ve_clock_type_e {
	EN_CLK_BLUE_STRETCH = 0,
	EN_CLK_CM,
	EN_CLK_CCORING,
	EN_CLK_BLKEXT,
	EN_CLK_MAX,
};

int vpp_module_ve_init(struct vpp_dev_s *pdev);
void vpp_module_ve_blkext_en(bool enable);
void vpp_module_ve_set_blkext_params(unsigned int *pdata);
void vpp_module_ve_set_blkext_param(enum ve_blkext_param_e type,
	int val);
void vpp_module_ve_ccoring_en(bool enable);
void vpp_module_ve_set_ccoring_params(unsigned int *pdata);
void vpp_module_ve_set_ccoring_param(enum ve_ccoring_param_e type,
	int val);
void vpp_module_ve_demo_center_bar_en(bool enable);
void vpp_module_ve_set_demo_left_top_screen_width(int val);
void vpp_module_ve_blue_stretch_en(bool enable);
void vpp_module_ve_set_blue_stretch_params(unsigned int *pdata);
void vpp_module_ve_set_blue_stretch_param(enum ve_blue_stretch_param_e type,
	int val);
void vpp_module_ve_clip_range_en(bool enable);
void vpp_module_ve_get_clip_range(int *top, int *bottom);
void vpp_module_ve_set_clock_ctrl(enum ve_clock_type_e type,
	int val);

void vpp_module_ve_dump_info(enum vpp_dump_module_info_e info_type);

#endif

