/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_VF_PROC_H__
#define __VPP_VF_PROC_H__

enum vpp_vd_path_e {
	EN_VD1_PATH = 0,
	EN_VD2_PATH,
	EN_VD3_PATH,
	EN_VD_PATH_MAX,
};

enum vpp_vf_top_e {
	EN_VF_TOP0 = 0,
	EN_VF_TOP1,
	EN_VF_TOP2,
	EN_VF_TOP_MAX,
};

enum vpp_pw_state_e {
	EN_PW_ON = 0,
	EN_PW_OFF,
	EN_PW_MAX,
};

enum vpp_dump_data_type_e {
	EN_DUMP_DATA_OVERSCAN = 0,
	EN_DUMP_DATA_MAX,
};

struct vpp_vf_signal_info_s {
	unsigned int format;
	unsigned int range;
	unsigned int color_primaries;
	unsigned int transfer_characteristic;
};

struct vpp_vf_param_s {
	unsigned int sps_h_en;
	unsigned int sps_v_en;
	unsigned int sps_w_in;
	unsigned int sps_h_in;
	unsigned int cm_in_w;
	unsigned int cm_in_h;
};

void vpp_vf_set_pc_mode(int val);
void vpp_vf_set_overscan_mode(int val);
void vpp_vf_set_overscan_reset(int val);
void vpp_vf_set_overscan_table(unsigned int length,
	struct vpp_overscan_table_s *load_table);

void vpp_vf_get_signal_info(enum vpp_vd_path_e vd_path,
	struct vpp_vf_signal_info_s *pinfo);
unsigned int vpp_vf_get_signal_type(enum vpp_vd_path_e vd_path);
enum vpp_data_src_e vpp_vf_get_src_type(enum vpp_vd_path_e vd_path);
enum vpp_hdr_type_e vpp_vf_get_hdr_type(void);
enum vpp_color_primary_e vpp_vf_get_color_primary(void);
struct vpp_hdr_metadata_s *vpp_vf_get_hdr_metadata(void);
enum vpp_csc_type_e vpp_vf_get_csc_type(enum vpp_vd_path_e vd_path);
int vpp_vf_get_vinfo_lcd_support(void);
void vpp_vf_dump_data(enum vpp_dump_data_type_e type);

void vpp_vf_refresh(struct vframe_s *pvf, struct vframe_s *prpt_vf);
void vpp_vf_proc(struct vframe_s *pvf,
	struct vframe_s *ptoggle_vf,
	struct vpp_vf_param_s *pvf_param,
	int flags,
	enum vpp_vd_path_e vd_path,
	enum vpp_vf_top_e vpp_top);

#endif

