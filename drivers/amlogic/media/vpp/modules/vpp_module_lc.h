/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_LC_H__
#define __VPP_MODULE_LC_H__

enum lc_lut_type_e {
	EN_LC_SAT = 0,
	EN_LC_YMIN_LMT,
	EN_LC_YPKBV_YMAX_LMT,
	EN_LC_YMAX_LMT,
	EN_LC_YPKBV_LMT,
	EN_LC_YPKBV_RAT,
	EN_LC_YPKBV_SLP_LMT,
	EN_LC_CNTST_LMT,
	EN_LC_MAX,
};

enum lc_curve_tune_mode_e {
	EN_LC_TUNE_OFF = 0,
	EN_LC_TUNE_LINEAR,
	EN_LC_TUNE_CORRECT,
};

enum lc_config_param_e {
	EN_LC_INPUT_CSEL = 0,
	EN_LC_INPUT_YSEL,
	EN_LC_BLKBLEND_MODE,
	EN_LC_HBLANK,
	EN_LC_BLK_VNUM,
	EN_LC_BLK_HNUM,
	EN_LC_CFG_PARAM_MAX,
};

enum lc_algorithm_param_e {
	EN_LC_BITDEPTH = 0,
	EN_LC_DET_RANGE_MODE,
	EN_LC_DET_RANGE_TH_BLK,
	EN_LC_DET_RANGE_TH_WHT,
	EN_LC_MIN_BV_PERCENT_TH,
	EN_LC_INVALID_BLK,
	EN_LC_OSD_IIR_EN,
	EN_LC_TS,
	EN_LC_VNUM_START_ABOVE,
	EN_LC_VNUM_END_ABOVE,
	EN_LC_VNUM_START_BELOW,
	EN_LC_VNUM_END_BELOW,
	EN_LC_SCENE_CHANGE_TH, /*need tuning (0~512)*/
	EN_LC_ALPHA1, /*control the refresh speed*/
	EN_LC_ALPHA2,
	EN_LC_REFRESH_BIT,
	EN_LC_ALG_PARAM_MAX,
};

enum lc_curve_tune_param_e {
	EN_LC_LMTRAT_SIGBIN = 0,
	EN_LC_LMTRAT_THD_MAX,
	EN_LC_LMTRAT_THD_BLACK,
	EN_LC_THD_BLACK,
	EN_LC_YMINV_BLACK_THD,
	EN_LC_YPKBV_BLACK_THD,
	EN_LC_BLACK_COUNT, /*read back black pixel count*/
	EN_LC_CURVE_TUNE_PARAM_MAX,
};

struct lc_vs_param_s {
	unsigned int vf_type;
	unsigned int vf_signal_type;
	unsigned int vf_height;
	unsigned int vf_width;
	unsigned int sps_h_en;
	unsigned int sps_v_en;
	unsigned int sps_w_in;
	unsigned int sps_h_in;
};

int vpp_module_lc_init(struct vpp_dev_s *pdev);
void vpp_module_lc_deinit(void);
int vpp_module_lc_en(bool enable);
void vpp_module_lc_write_lut(enum lc_lut_type_e type, int *pdata);
void vpp_module_lc_set_demo_mode(bool enable, bool left_side);
void vpp_module_lc_set_tune_mode(enum lc_curve_tune_mode_e mode);
void vpp_module_lc_set_tune_param(enum lc_curve_tune_param_e index, int val);
void vpp_module_lc_set_cfg_param(enum lc_config_param_e index, int val);
void vpp_module_lc_set_alg_param(enum lc_algorithm_param_e index, int val);
void vpp_module_lc_set_isr_def(int val);
void vpp_module_lc_set_isr_use(int val);
void vpp_module_lc_set_isr(void);
bool vpp_module_lc_get_support(void);
int vpp_module_lc_read_lut(enum lc_lut_type_e type, int *pdata, int len);
int vpp_module_lc_get_tune_param(enum lc_curve_tune_param_e index);
int vpp_module_lc_get_cfg_param(enum lc_config_param_e index);
int vpp_module_lc_get_alg_param(enum lc_algorithm_param_e index);
void vpp_module_lc_on_vs(unsigned short *pdata_hist, struct lc_vs_param_s *pvs_param);

void vpp_module_lc_dump_info(enum vpp_dump_module_info_e info_type);

#endif

