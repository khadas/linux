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

enum lc_cfg_param_e {
	EN_LC_INPUT_CSEL = 0,
	EN_LC_INPUT_YSEL,
	EN_LC_BLKBLEND_MODE,
	EN_LC_HBLANK,
	EN_LC_BLK_VNUM,
	EN_LC_BLK_HNUM,
	EN_LC_CFG_PARAM_MAX,
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

struct lc_curve_tune_param_s {
	int lmtrat_sigbin;
	int lmtrat_thd_max;
	int lmtrat_thd_black;
	int thd_black;
	int yminv_black_thd;
	int ypkbv_black_thd;
	int black_count;/*read back black pixel count*/
};

int vpp_module_lc_init(struct vpp_dev_s *pdev);
void vpp_module_lc_deinit(void);
int vpp_module_lc_en(bool enable);
void vpp_module_lc_write_lut(enum lc_lut_type_e type, int *pdata);
void vpp_module_lc_set_demo_mode(bool enable, bool left_side);
void vpp_module_lc_set_tune_mode(enum lc_curve_tune_mode_e mode);
void vpp_module_lc_set_tune_param(struct lc_curve_tune_param_s *pparam);
void vpp_module_lc_set_cfg_param(enum lc_cfg_param_e index, int val);
void vpp_module_lc_set_isr_def(int val);
void vpp_module_lc_set_isr_use(int val);
bool vpp_module_lc_get_support(void);
void vpp_module_lc_on_vs(int *phist, struct lc_vs_param_s *pvs_param);

#endif

