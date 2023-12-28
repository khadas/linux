/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _INC_AML_LDIM_ALG_H_
#define _INC_AML_LDIM_ALG_H_

enum ldc_chip_type_e {
	LDC_T7		= 0x00,
	LDC_T3		= 0x01,
	LDC_T3X		= 0x02,
};

enum ldc_dbg_type_e {
	LDC_DBG_ATTR		= 0x01,
	LDC_DBG_MEM			= 0x02,
	LDC_DBG_REG			= 0x03,
	LDC_DBG_DBGREG		= 0x04,
	LDC_DBG_DEBUG		= 0x05,
	LDC_DBG_PARA		= 0x06,
};

struct ldim_seg_hist_s {
	unsigned int weight_avg;
	unsigned int weight_avg_95;
	unsigned int max_index;
	unsigned int min_index;
};

struct ldim_stts_s {
	unsigned int *global_hist;
	unsigned int global_hist_sum;
	unsigned int global_hist_cnt;
	unsigned int global_apl;
	struct ldim_seg_hist_s *seg_hist;
};

struct ldim_profile_s {
	unsigned int mode;
	unsigned int profile_k;
	unsigned int profile_bits;
	char file_path[256];
};

struct ldim_rmem_s {
	unsigned char flag; //0:none, 1:ldc_cma, 2:sys_cma_pool, 3:kmalloc

	void *rsv_mem_vaddr;
	phys_addr_t rsv_mem_paddr;
	unsigned int rsv_mem_size;
};

struct ldim_fw_config_s {
	unsigned short hsize;
	unsigned short vsize;
	unsigned char func_en;
	unsigned char remap_en;
};

struct fw_pq_s {
	unsigned int func_en;
	unsigned int remapping_en;

	/* switch fw, use for custom fw. 0=aml_hw_fw, 1=aml_sw_fw */
	unsigned int fw_sel;

	/* fw parameters */
	unsigned int ldc_hist_mode;
	unsigned int ldc_hist_blend_mode;
	unsigned int ldc_hist_blend_alpha;
	unsigned int ldc_hist_adap_blend_max_gain;
	unsigned int ldc_hist_adap_blend_diff_th1;
	unsigned int ldc_hist_adap_blend_diff_th2;
	unsigned int ldc_hist_adap_blend_th0;
	unsigned int ldc_hist_adap_blend_thn;
	unsigned int ldc_hist_adap_blend_gain_0;
	unsigned int ldc_hist_adap_blend_gain_1;
	unsigned int ldc_init_bl_min;
	unsigned int ldc_init_bl_max;

	unsigned int ldc_sf_mode;
	unsigned int ldc_sf_gain_up;
	unsigned int ldc_sf_gain_dn;
	unsigned int ldc_sf_tsf_3x3;
	unsigned int ldc_sf_tsf_5x5;

	unsigned int ldc_bs_bl_mode;
	//unsigned int ldc_glb_apl; //read only
	unsigned int ldc_bs_glb_apl_gain;
	unsigned int ldc_bs_dark_scene_bl_th;
	unsigned int ldc_bs_gain;
	unsigned int ldc_bs_limit_gain;
	unsigned int ldc_bs_loc_apl_gain;
	unsigned int ldc_bs_loc_max_min_gain;
	unsigned int ldc_bs_loc_dark_scene_bl_th;

	unsigned int ldc_tf_en;
	//unsigned int ldc_tf_sc_flag; //read only
	unsigned int ldc_tf_low_alpha;
	unsigned int ldc_tf_high_alpha;
	unsigned int ldc_tf_low_alpha_sc;
	unsigned int ldc_tf_high_alpha_sc;

	unsigned int ldc_dimming_curve_en;
	unsigned int ldc_sc_hist_diff_th;
	unsigned int ldc_sc_apl_diff_th;
	unsigned int bl_remap_curve[17];
	unsigned int post_bl_remap_curve[17];

	/* comp parameters */
	unsigned int ldc_bl_buf_diff;
	unsigned int ldc_glb_gain;
	unsigned int ldc_dth_en;
	unsigned int ldc_dth_bw;
	unsigned int ldc_gain_lut[16][64];
	unsigned int ldc_min_gain_lut[64];
};

struct fw_pqdata_s {
	struct fw_pq_s pqdata[4];
};

struct ldim_fw_s {
	/* header */
	unsigned int para_ver;
	unsigned int para_size;
	char alg_ver[20];
	unsigned char chip_type;
	unsigned char fw_sel;
	unsigned char valid;
	unsigned char flag;

	unsigned char seg_col;
	unsigned char seg_row;
	unsigned char func_en;
	unsigned char remap_en;
	unsigned char res_update;
	unsigned int fw_ctrl;
	unsigned int fw_state;

	unsigned int bl_matrix_dbg;
	unsigned char fw_hist_print;
	unsigned int fw_print_frequent;
	unsigned int fw_print_lv;

	struct ldim_fw_config_s *conf;
	struct ldim_rmem_s *rmem;
	struct ldim_stts_s *stts;
	struct ldim_profile_s *profile;

	unsigned int *bl_matrix;

	void (*fw_alg_frm)(struct ldim_fw_s *fw);
	void (*fw_alg_para_print)(struct ldim_fw_s *fw);
	void (*fw_init)(struct ldim_fw_s *fw);
	void (*fw_info_update)(struct ldim_fw_s *fw);
	void (*fw_pq_set)(struct fw_pq_s *pq);
	void (*fw_profile_set)(unsigned char *buf, unsigned int len);
	void (*fw_rmem_duty_get)(struct ldim_fw_s *fw);
	void (*fw_rmem_duty_set)(unsigned int *bl_matrix);
	ssize_t (*fw_debug_show)(struct ldim_fw_s *fw,
		enum ldc_dbg_type_e type, char *buf);
	ssize_t (*fw_debug_store)(struct ldim_fw_s *fw,
		enum ldc_dbg_type_e type, char *buf, size_t len);
};

struct ldim_fw_custom_s {
	/* header */
	unsigned char valid;/*if ld firmware is ready*/

	unsigned char seg_col;
	unsigned char seg_row;
	unsigned int global_hist_bin_num;
	unsigned char comp_en;//use cus_sw duty for compensation, default 0
	unsigned char pq_update;//when modify pqdata, set pq_update = 1;
	struct fw_pq_s *pqdata;

	unsigned int fw_print_frequent;/*debug print frequent, count of frames*/
	unsigned int fw_print_lv;/*debug print level, range at 200 - 300*/

	unsigned int *param; /*custom fw parameters store in panel ini*/

	unsigned int *bl_matrix;/*backlight matrix output*/

	/*function for backlight matrix algorithm*/
	void (*fw_alg_frm)(struct ldim_fw_custom_s *fw_cus,
		struct ldim_stts_s *stts);
	void (*fw_alg_para_print)(struct ldim_fw_custom_s *fw_cus);
};

/* if struct ldim_fw_s changed, FW_PARA_VER must be update */
/*20221118 version 1*/
/*20230915 version 2*/
#define FW_PARA_VER    2

struct ldim_fw_s *aml_ldim_get_fw(void);
struct ldim_fw_custom_s *aml_ldim_get_fw_cus(void);

#endif
