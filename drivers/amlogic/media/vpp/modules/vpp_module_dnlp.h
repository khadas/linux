/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_DNLP_H__
#define __VPP_MODULE_DNLP_H__

struct dnlp_ai_pq_param_s {
	int final_gain;
};

struct dnlp_dbg_parse_cmd_s {
	char *parse_string;
	unsigned int *value;
	unsigned int data_len;
};

int vpp_module_dnlp_init(struct vpp_dev_s *pdev);
void vpp_module_dnlp_en(bool enable);
void vpp_module_dnlp_on_vs(int hist_luma_sum, unsigned short *phist_data);
void vpp_module_dnlp_get_sat_compensation(bool *psat_comp, int *psat_val);
void vpp_module_dnlp_set_param(struct vpp_dnlp_curve_param_s *pdata);
bool vpp_module_dnlp_get_insmod_status(void);
struct dnlp_dbg_parse_cmd_s *vpp_module_dnlp_get_dbg_info(void);
struct dnlp_dbg_parse_cmd_s *vpp_module_dnlp_get_dbg_ro_info(void);
struct dnlp_dbg_parse_cmd_s *vpp_module_dnlp_get_dbg_cv_info(void);

void vpp_module_dnlp_get_ai_pq_base(struct dnlp_ai_pq_param_s *pparam);
void vpp_module_dnlp_set_ai_pq_offset(struct dnlp_ai_pq_param_s *pparam);

void vpp_module_dnlp_dump_info(enum vpp_dump_module_info_e info_type);

#endif

