/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_SR_H__
#define __VPP_MODULE_SR_H__

#define FMETER_HCNT_MAX (4)

enum sr_mode_e {
	EN_MODE_SR_0 = 0,
	EN_MODE_SR_1,
	EN_MODE_SR_MAX,
};

enum sr_sub_module_e {
	EN_SUB_MD_PK_NR = 0,
	EN_SUB_MD_PK,
	EN_SUB_MD_HCTI,
	EN_SUB_MD_HLTI,
	EN_SUB_MD_VLTI,
	EN_SUB_MD_VCTI,
	EN_SUB_MD_DEJAGGY,
	EN_SUB_MD_DRTLPF,
	EN_SUB_MD_DRTLPF_THETA,
	EN_SUB_MD_DERING,
	EN_SUB_MD_DEBAND,
	EN_SUB_MD_FMETER,
};

struct sr_fmeter_report_s {
	unsigned int hcnt[EN_MODE_SR_MAX][FMETER_HCNT_MAX];
	int score[EN_MODE_SR_MAX];
};

struct sr_vs_param_s {
	unsigned int vf_width;
	unsigned int vf_height;
	unsigned int sps_w_in;
	unsigned int sps_h_in;
};

struct sr_ai_pq_param_s {
	int hp_final_gain[EN_MODE_SR_MAX];
	int bp_final_gain[EN_MODE_SR_MAX];
};

int vpp_module_sr_init(struct vpp_dev_s *pdev);
void vpp_module_sr_en(enum sr_mode_e mode, bool enable);
void vpp_module_sr_sub_module_en(enum sr_mode_e mode,
	enum sr_sub_module_e sub_module, bool enable);
void vpp_module_sr_set_demo_mode(bool enable, bool left_side);
void vpp_module_sr_set_osd_gain(enum sr_mode_e mode,
	int hp_val, int bp_val);
bool vpp_module_sr_get_fmeter_support(void);
void vpp_module_sr_fetch_fmeter_report(void);
struct sr_fmeter_report_s *vpp_module_sr_get_fmeter_report(void);
void vpp_module_sr_on_vs(struct sr_vs_param_s *pvs_param);

void vpp_module_sr_get_ai_pq_base(struct sr_ai_pq_param_s *pparam);
void vpp_module_sr_set_ai_pq_offset(struct sr_ai_pq_param_s *pparam);

void vpp_module_sr_dump_info(enum vpp_dump_module_info_e info_type);

#endif

