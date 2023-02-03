/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_CM_H__
#define __VPP_MODULE_CM_H__

#define CM2_CURVE_SIZE (32)

enum cm_tuning_param_e {
	EN_PARAM_GLB_HUE = 0,
	EN_PARAM_GLB_SAT,
	EN_PARAM_FINAL_GAIN_LUMA,
	EN_PARAM_FINAL_GAIN_SAT,
	EN_PARAM_FINAL_GAIN_HUE,
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

struct cm_ai_pq_param_s {
	int sat[CM2_CURVE_SIZE * 3];
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
void vpp_module_cm_sub_module_en(enum cm_sub_module_e sub_module,
	bool enable);
void vpp_module_cm_set_cm2_offset_luma(char *pdata);
void vpp_module_cm_set_cm2_offset_sat(char *pdata);
void vpp_module_cm_set_cm2_offset_hue(char *pdata);
void vpp_module_cm_set_cm2_offset_hue_by_hs(char *pdata);
int vpp_module_cm_get_cm2_luma(int *pdata);
int vpp_module_cm_get_cm2_sat(int *pdata);
int vpp_module_cm_get_cm2_sat_by_l(int *pdata);
int vpp_module_cm_get_cm2_sat_by_hl(int *pdata);
int vpp_module_cm_get_cm2_hue(int *pdata);
int vpp_module_cm_get_cm2_hue_by_hs(int *pdata);
int vpp_module_cm_get_cm2_hue_by_hl(int *pdata);
bool vpp_module_cm_get_final_gain_support(void);
void vpp_module_cm_on_vs(void);

void vpp_module_cm_set_reg(unsigned int addr, int val);
int vpp_module_cm_get_reg(unsigned int addr);

void vpp_module_cm_get_ai_pq_base(struct cm_ai_pq_param_s *pparam);
void vpp_module_cm_set_ai_pq_offset(struct cm_ai_pq_param_s *pparam);

void vpp_module_cm_dump_info(enum vpp_dump_module_info_e info_type);

#endif

