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

int vpp_module_ve_init(struct vpp_dev_s *pdev);
void vpp_module_ve_blkext_en(bool enable);
void vpp_module_ve_set_blkext_param(enum ve_blkext_param_e type,
	int val);
void vpp_module_ve_ccoring_en(bool enable);
void vpp_module_ve_set_ccoring_param(enum ve_ccoring_param_e type,
	int val);
void vpp_module_ve_demo_center_bar_en(bool enable);
void vpp_module_ve_set_demo_left_top_screen_width(int val);

#endif

