/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_CM_H__
#define __VPP_MODULE_CM_H__

enum cm_tuning_param_e {
	EN_PARAM_GLB_HUE = 0,
	EN_PARAM_GLB_SAT,
};

enum cm_sub_module_e {
	EN_SUB_MD_LUMA_ADJ = 0,
	EN_SUB_MD_SAT_ADJ,
	EN_SUB_MD_HUE_ADJ,
	EN_SUB_MD_FILTER,
};

struct cm_cfg_param_s {
	int frm_height;
	int frm_width;
};

int vpp_module_cm_init(struct vpp_dev_s *pdev);
int vpp_module_cm_en(bool enable);
void vpp_module_cm_set_cm2_luma(int *pdata);
void vpp_module_cm_set_cm2_sat(int *pdata);
void vpp_module_cm_set_cm2_sat_by_l(int *pdata);
void vpp_module_cm_set_cm2_sat_by_hl(int *pdata);
void vpp_module_cm_set_cm2_hue(int *pdata);
void vpp_module_cm_set_cm2_hue_by_hs(int *pdata);
void vpp_module_cm_set_cm2_hue_by_hl(int *pdata);
void vpp_module_cm_set_demo_mode(bool enable, bool left_side);
void vpp_module_cm_set_cfg_param(struct cm_cfg_param_s *pparam);
void vpp_module_cm_set_tuning_param(enum cm_tuning_param_e type,
	int *pdata);
void vpp_module_cm_on_vs(void);

#endif

