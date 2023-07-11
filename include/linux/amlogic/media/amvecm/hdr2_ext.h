/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef AM_HDR2_EXT
#define AM_HDR2_EXT
#include <linux/types.h>

enum hdr_module_sel {
	VD1_HDR = 1,
	VD2_HDR = 2,
	VD3_HDR = 3,
	OSD1_HDR = 4,
	OSD2_HDR = 5,
	VDIN0_HDR = 6,
	VDIN1_HDR = 7,
	DI_HDR = 8,
	DI_M2M_HDR = 9,
	OSD3_HDR = 10,
	S5_VD1_SLICE1 = 11,
	S5_VD1_SLICE2 = 12,
	S5_VD1_SLICE3 = 13,
	VICP_HDR = 14,
	HDR_MAX
};

enum hdr_process_sel {
	HDR_BYPASS = BIT(0),
	HDR_SDR = BIT(1),
	SDR_HDR = BIT(2),
	HLG_BYPASS = BIT(3),
	HLG_SDR = BIT(4),
	HLG_HDR = BIT(5),
	SDR_HLG = BIT(6),
	SDR_IPT = BIT(7),
	HDR_IPT = BIT(8),
	HLG_IPT = BIT(9),
	HDR_HLG = BIT(10),
	HDR10P_SDR = BIT(11),
	SDR_GMT_CONVERT = BIT(12),
	IPT_MAP = BIT(13),
	IPT_SDR = BIT(14),
	CUVA_BYPASS = BIT(15),
	CUVA_SDR = BIT(16),
	CUVA_HDR = BIT(17),
	CUVA_HLG = BIT(18),
	CUVA_IPT = BIT(19),
	SDR_CUVA = BIT(20),
	HDR_CUVA = BIT(21),
	HLG_CUVA = BIT(22),
	CUVAHLG_SDR = BIT(23),
	CUVAHLG_HDR = BIT(24),
	CUVAHLG_HLG = BIT(25),
	CUVAHLG_CUVA = BIT(26),
	SDR_AC_ENH = BIT(27),
	HDR_HDR = BIT(28),
	/* reserved  several bits for additional info */
	RGB_OSD = BIT(29),
	RGB_VDIN = BIT(30),
	FULL_VDIN = BIT(31)
};

enum hdr_matrix_sel {
	HDR_IN_MTX = 1,
	HDR_GAMUT_MTX = 2,
	HDR_OUT_MTX = 3,
	HDR_MTX_MAX
};

#define MTX_ON  1
#define MTX_OFF 0

#define MTX_ONLY  1
#define HDR_ONLY  0

#define LUT_ON  1
#define LUT_OFF 0

#define HDR2_EOTF_LUT_SIZE 143
#define HDR2_OOTF_LUT_SIZE 149
#define HDR2_OETF_LUT_SIZE 149
#define HDR2_CGAIN_LUT_SIZE 65

#define MTX_NUM_PARAM 16

struct hdr_proc_mtx_param_s {
	int mtx_only;
	int mtx_in[MTX_NUM_PARAM];
	int mtx_gamut[9];
	int ncl_prmy_panel[9];
	int mtx_gamut_mode;
	int mtx_cgain[MTX_NUM_PARAM];
	int mtx_ogain[MTX_NUM_PARAM];
	int mtx_out[MTX_NUM_PARAM];
	int mtxi_pre_offset[3];
	int mtxi_pos_offset[3];
	int mtxo_pre_offset[3];
	int mtxo_pos_offset[3];
	int mtx_cgain_offset[3];
	unsigned int mtx_on;
	enum hdr_process_sel p_sel;
	unsigned int gmt_bit_mode;
	int gmut_shift;
};

struct hdr_proc_lut_param_s {
	s64 eotf_lut[HDR2_EOTF_LUT_SIZE];
	s64 oetf_lut[HDR2_OETF_LUT_SIZE];
	s64 cgain_lut[HDR2_CGAIN_LUT_SIZE];
	s32 ogain_lut[HDR2_OOTF_LUT_SIZE];

	int ys_coef[3];
	unsigned int adp_scal_x_shift;
	unsigned int adp_scal_y_shift;

	unsigned int lut_on;
	unsigned int bitdepth;
	unsigned int cgain_en;
	unsigned int hist_en;
};

struct hdr_proc_clip_param_s {
	int clip_en;
	int clip_max;
};

struct hdr_proc_adpscl_param_s {
	int adpscl_mode;
	int adpscl_bypass[3];
	int adpscl_alpha[3];
	int adpscl_ys_coef[3];
	int adpscl_beta[3];
	int adpscl_beta_s[3];
	int adpscl_shift[3];
};

#define HDR_UPDATE_IN_MTX		BIT(0)
#define HDR_UPDATE_LUT_EOTF		BIT(1)
#define HDR_UPDATE_GAMUT_MTX	BIT(2)
#define HDR_UPDATE_LUT_OOTF		BIT(3)
#define HDR_UPDATE_LUT_OETF		BIT(4)
#define HDR_UPDATE_OUT_MTX		BIT(5)
#define HDR_UPDATE_CGAIN		BIT(6)
#define HDR_UPDATE_HIST			BIT(7)
#define HDR_UPDATE_CLIP			BIT(8)
#define HDR_UPDATE_ADPSCL		BIT(9)
#define HDR_UPDATE_ALL			0xff

enum hdr_params_op {
	HDR_FULL_SETTING = 0,
	HDR10P_CURVE_SETTING = 1,
	HDR_TM_SETTING = 2,
	HDR_MAX_SETTING
};

struct hdr_proc_setting_param_s {
	struct hdr_proc_mtx_param_s hdr_mtx_param;
	struct hdr_proc_lut_param_s hdr_lut_param;
	struct hdr_proc_clip_param_s hdr_clip_param;
	struct hdr_proc_adpscl_param_s hdr_adpscl_param;
	/* struct matrix_s mtx; */
	u32 update_flag;
};

int get_hdr_setting(enum hdr_module_sel module_sel,
				enum hdr_process_sel hdr_process_select,
				struct hdr_proc_setting_param_s *hdr_params,
				enum hdr_params_op op);
#endif
