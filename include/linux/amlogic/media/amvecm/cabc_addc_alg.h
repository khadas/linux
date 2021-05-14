/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CABC_AADC_ALG_H
#define CABC_AADC_ALG_H

#define PSTHIST_NRM 6
#define PSTHIST_NUM BIT(PSTHIST_NRM)
#define LED_BL_BW	12

struct aad_param_s {
	//0:Y, R/G, B/G, 1:rgb->xyz->lms, 2:rgb gain lut
	int aad_sensor_mode;
	int aad_mode;
	int *aad_xyz2rgb_matrix;
	int *aad_rgb2xyz_matrix;
	int *aad_xyz2lms_matrix;
	int *aad_lms2xyz_matrix;
	int *aad_rgb2lms_matrix;
	int *aad_lms2rgb_matrix;
	int *aad_LUT_Y_gain;
	int *aad_LUT_RG_gain;
	int *aad_LUT_BG_gain;
	int aad_Y_gain_min;
	int aad_Y_gain_max;
	int aad_RG_gain_min;
	int aad_RG_gain_max;
	int aad_BG_gain_min;
	int aad_BG_gain_max;
	int aad_tf_en;
	int aad_tf_alpha;
	int aad_dist_mode;
	int aad_force_gain_en;
	int (*aad_gain_lut)[3];
	int (*aad_xy_lut)[2];
	int aad_r_gain;
	int aad_g_gain;
	int aad_b_gain;
	int aad_min_dist_th;
	int *sensor_input;
};

struct cabc_param_s {
	int cabc_max95_ratio;
	int cabc_hist_mode;
	int cabc_hist_blend_alpha;
	int cabc_init_bl_min;
	int cabc_init_bl_max;
	int cabc_tf_alpha;
	int cabc_tf_en;
	int cabc_sc_flag;
	int cabc_sc_hist_diff_thd;
	int cabc_sc_apl_diff_thd;
	int cabc_en;
	int cabc_bl_map_mode;
	int cabc_bl_map_en;
	int *o_bl_cv;
	int *maxbin_bl_cv;
	int cabc_patch_bl_th;
	int cabc_patch_on_alpha;
	int cabc_patch_bl_off_th;
	int cabc_patch_off_alpha;
	int cabc_temp_proc;
};

struct pre_gam_param_s {
	int pre_gamma_gain_ratio;
};

struct cabc_debug_param_s {
	int dbg_cabc_gain;
	int avg;
	int max95;
	int tf_bl;
};

struct aad_debug_param_s {
	int y_val;
	int rg_val;
	int bg_val;
	int cur_frm_gain;
};

struct aad_fw_param_s {
	int fw_aad_en;
	int fw_aad_status;
	char *fw_aad_ver;
	struct aad_param_s *aad_param;
	int *cur_gain;
	int aad_debug_mode;
	struct aad_debug_param_s *dbg_param;

	void (*aad_alg)(struct aad_fw_param_s *fw_aad_param,
		int *aad_final_gain);
};

struct cabc_fw_param_s {
	int fw_cabc_en;
	int fw_cabc_status;
	char *fw_cabc_ver;
	struct cabc_param_s *cabc_param;
	int *i_hist;
	int cur_bl;
	int tgt_bl;
	int cabc_debug_mode;
	struct cabc_debug_param_s *dbg_param;

	void (*cabc_alg)(struct cabc_fw_param_s *fw_cabc_param,
		int *cabc_final_gain);
};

struct pgm_param_s {
	int fw_pre_gamma_en;
	struct pre_gam_param_s *pre_gam_param;
	int *aad_gain;
	int *cabc_gain;

	void (*pre_gamma_proc)(struct pgm_param_s *pgm_param, int (*final_pre_gamma)[65]);
};

struct aad_fw_param_s *aad_fw_param_get(void);
struct cabc_fw_param_s *cabc_fw_param_get(void);
struct pgm_param_s *pregam_fw_param_get(void);
#endif
