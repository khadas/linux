/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_VE_H__
#define __VPP_MODULE_VE_H__

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
void vpp_module_ve_set_blkext_param(enum vpp_blkext_param_e type,
	int val);
void vpp_module_ve_ccoring_en(bool enable);
void vpp_module_ve_set_ccoring_params(unsigned int *pdata);
void vpp_module_ve_set_ccoring_param(enum vpp_ccoring_params_e type,
	int val);
void vpp_module_ve_demo_center_bar_en(bool enable);
void vpp_module_ve_set_demo_left_top_screen_width(int val);
void vpp_module_ve_blue_stretch_en(bool enable);
void vpp_module_ve_set_blue_stretch_params(unsigned int *pdata);
void vpp_module_ve_set_blue_stretch_param(enum vpp_blue_stretch_param_e type,
	int val);
void vpp_module_ve_clip_range_en(bool enable);
void vpp_module_ve_get_clip_range(int *top, int *bottom);
void vpp_module_ve_set_clock_ctrl(enum ve_clock_type_e type,
	int val);

void vpp_module_ve_dump_info(enum vpp_dump_module_info_e info_type);

#endif

