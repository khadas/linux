/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_COMMON_DEF_H__
#define __VPP_COMMON_DEF_H__

#define _VPP_TYPE  'V'

#define VPP_GAMMA_TABLE_LEN       (256)
#define VPP_PRE_GAMMA_TABLE_LEN   (65)
#define VPP_MTRX_COEF_LEN         (9)
#define VPP_MTRX_OFFSET_LEN       (3)
#define VPP_HIST_BIN_COUNT        (64)
#define VPP_COLOR_HIST_BIN_COUNT  (32)

#define VPP_DNLP_SCURV_LEN             (65)
#define VPP_DNLP_GAIN_VAR_LUT_LEN      (49)
#define VPP_DNLP_WEXT_GAIN_LEN         (48)
#define VPP_DNLP_ADP_THRD_LEN          (33)
#define VPP_DNLP_REG_BLK_BOOST_LEN     (13)
#define VPP_DNLP_REG_ADP_OFSET_LEN     (20)
#define VPP_DNLP_REG_MONO_PROT_LEN     (6)
#define VPP_DNLP_TREND_WHT_EXP_LUT_LEN (9)
#define VPP_DNLP_HIST_GAIN_LEN         (65)

/*Common enum*/
enum vpp_rgb_mode_e {
	EN_MODE_R = 0,
	EN_MODE_G,
	EN_MODE_B,
	EN_MODE_RGB_MAX,
};

enum vpp_level_mode_e {
	EN_LEVEL_MODE_OFF = 0,
	EN_LEVEL_MODE_LOW,
	EN_LEVEL_MODE_MID,
	EN_LEVEL_MODE_HIGH,
	EN_LEVEL_MODE_AUTO,
};

enum vpp_pc_mode_e {
	EN_PC_MODE_OFF = 0,
	EN_PC_MODE_ON,
};

enum vpp_page_module_e {
	EN_MODULE_SHARP_0 = 0,
	EN_MODULE_SHARP_1,
	EN_MODULE_INDEX_MAX,
};

enum vpp_module_e {
	EN_MODULE_VADJ1 = 0,
	EN_MODULE_VADJ2,
	EN_MODULE_PREGAMMA,
	EN_MODULE_GAMMA,
	EN_MODULE_WB,
	EN_MODULE_DNLP,
	EN_MODULE_CCORING,
	EN_MODULE_SR0,
	EN_MODULE_SR0_DNLP,
	EN_MODULE_SR1,
	EN_MODULE_SR1_DNLP,
	EN_MODULE_LC,
	EN_MODULE_CM,
	EN_MODULE_BLE,
	EN_MODULE_BLS,
	EN_MODULE_LUT3D,
	EN_MODULE_ALL,
};

enum vpp_color_primary_e {
	EN_COLOR_PRI_NULL = 0,
	EN_COLOR_PRI_BT601,
	EN_COLOR_PRI_BT709,
	EN_COLOR_PRI_BT2020,
	EN_COLOR_PRI_MAX,
};

enum vpp_hdr_lut_type_e {
	EN_HDR_LUT_TYPE_HLG = 1,
	EN_HDR_LUT_TYPE_HDR,
	EN_HDR_LUT_TYPE_MAX,
};

enum vpp_hdr_type_e {
	EN_TYPE_NONE = 0,
	EN_TYPE_SDR,
	EN_TYPE_HDR10,
	EN_TYPE_HLG,
	EN_TYPE_HDR10PLUS,
	EN_TYPE_DOBVI,
	EN_TYPE_MVC,
	EN_TYPE_CUVA_HDR,
	EN_TYPE_CUVA_HLG,
};

enum vpp_dnlp_rt_e {
	EN_DNLP_RT_0S = 0,
	EN_DNLP_RT_1S = 6,
	EN_DNLP_RT_2S,
	EN_DNLP_RT_4S,
	EN_DNLP_RT_8S,
	EN_DNLP_RT_16S,
	EN_DNLP_RT_32S,
	EN_DNLP_RT_64S,
	EN_DNLP_RT_FREEZE,
};

enum vpp_dnlp_rl_e {
	EN_DNLP_RL_01 = 1, /*max_contrast = 1.0625x*/
	EN_DNLP_RL_02,     /*max_contrast = 1.1250x*/
	EN_DNLP_RL_03,     /*max_contrast = 1.1875x*/
	EN_DNLP_RL_04,     /*max_contrast = 1.2500x*/
	EN_DNLP_RL_05,     /*max_contrast = 1.3125x*/
	EN_DNLP_RL_06,     /*max_contrast = 1.3750x*/
	EN_DNLP_RL_07,     /*max_contrast = 1.4375x*/
	EN_DNLP_RL_08,     /*max_contrast = 1.5000x*/
	EN_DNLP_RL_09,     /*max_contrast = 1.5625x*/
	EN_DNLP_RL_10,     /*max_contrast = 1.6250x*/
	EN_DNLP_RL_11,     /*max_contrast = 1.6875x*/
	EN_DNLP_RL_12,     /*max_contrast = 1.7500x*/
	EN_DNLP_RL_13,     /*max_contrast = 1.8125x*/
	EN_DNLP_RL_14,     /*max_contrast = 1.8750x*/
	EN_DNLP_RL_15,     /*max_contrast = 1.9375x*/
	EN_DNLP_RL_16,     /*max_contrast = 2.0000x*/
};

enum vpp_dnlp_ext_e {
	EN_DNLP_EXT_00 = 0, /*weak*/
	EN_DNLP_EXT_01,
	EN_DNLP_EXT_02,
	EN_DNLP_EXT_03,
	EN_DNLP_EXT_04,
	EN_DNLP_EXT_05,
	EN_DNLP_EXT_06,
	EN_DNLP_EXT_07,
	EN_DNLP_EXT_08,
	EN_DNLP_EXT_09,
	EN_DNLP_EXT_10,
	EN_DNLP_EXT_11,
	EN_DNLP_EXT_12,
	EN_DNLP_EXT_13,
	EN_DNLP_EXT_14,
	EN_DNLP_EXT_15,
	EN_DNLP_EXT_16,     /*strong*/
};

enum vpp_csc_type_e {
	EN_CSC_MATRIX_NULL                = 0,
	EN_CSC_MATRIX_RGB_YUV601          = 0x1,
	EN_CSC_MATRIX_RGB_YUV601F         = 0x2,
	EN_CSC_MATRIX_RGB_YUV709          = 0x3,
	EN_CSC_MATRIX_RGB_YUV709F         = 0x4,
	EN_CSC_MATRIX_YUV601_RGB          = 0x10,
	EN_CSC_MATRIX_YUV601_YUV601F      = 0x11,
	EN_CSC_MATRIX_YUV601_YUV709       = 0x12,
	EN_CSC_MATRIX_YUV601_YUV709F      = 0x13,
	EN_CSC_MATRIX_YUV601F_RGB         = 0x14,
	EN_CSC_MATRIX_YUV601F_YUV601      = 0x15,
	EN_CSC_MATRIX_YUV601F_YUV709      = 0x16,
	EN_CSC_MATRIX_YUV601F_YUV709F     = 0x17,
	EN_CSC_MATRIX_YUV709_RGB          = 0x20,
	EN_CSC_MATRIX_YUV709_YUV601       = 0x21,
	EN_CSC_MATRIX_YUV709_YUV601F      = 0x22,
	EN_CSC_MATRIX_YUV709_YUV709F      = 0x23,
	EN_CSC_MATRIX_YUV709F_RGB         = 0x24,
	EN_CSC_MATRIX_YUV709F_YUV601      = 0x25,
	EN_CSC_MATRIX_YUV709F_YUV709      = 0x26,
	EN_CSC_MATRIX_YUV601L_YUV709L     = 0x27,
	EN_CSC_MATRIX_YUV709L_YUV601L     = 0x28,
	EN_CSC_MATRIX_YUV709F_YUV601F     = 0x29,
	EN_CSC_MATRIX_BT2020YUV_BT2020RGB = 0x40,
	EN_CSC_MATRIX_BT2020RGB_709RGB,
	EN_CSC_MATRIX_BT2020RGB_CUSRGB,
	EN_CSC_MATRIX_DEFAULT_CSCTYPE     = 0xffff,
};

enum vpp_mtrx_type_e {
	EN_MTRX_VD1 = 0,
	EN_MTRX_POST,
	EN_MTRX_POST2,
	EN_MTRX_MAX,
};

enum vpp_dnlp_param_e {
	EN_DNLP_SMHIST_CK = 0,
	EN_DNLP_MVREFLSH,
	EN_DNLP_CUVBLD_MIN,
	EN_DNLP_CUVBLD_MAX,
	EN_DNLP_BBD_RATIO_LOW,
	EN_DNLP_BBD_RATIO_HIG,
	EN_DNLP_BLK_CCTR,
	EN_DNLP_BRGT_CTRL,
	EN_DNLP_BRGT_RANGE,
	EN_DNLP_BRGHT_ADD,
	EN_DNLP_BRGHT_MAX,
	EN_DNLP_AUTO_RNG,
	EN_DNLP_LOWRANGE,
	EN_DNLP_HGHRANGE,
	EN_DNLP_SATUR_RAT,
	EN_DNLP_SATUR_MAX,
	EN_DNLP_SBGNBND,
	EN_DNLP_SENDBND,
	EN_DNLP_CLASHBGN,
	EN_DNLP_CLASHEND,
	EN_DNLP_CLAHE_GAIN_NEG,
	EN_DNLP_CLAHE_GAIN_POS,
	EN_DNLP_MTDBLD_RATE,
	EN_DNLP_ADPMTD_LBND,
	EN_DNLP_ADPMTD_HBND,
	EN_DNLP_BLKEXT_OFST,
	EN_DNLP_WHTEXT_OFST,
	EN_DNLP_BLKEXT_RATE,
	EN_DNLP_WHTEXT_RATE,
	EN_DNLP_BWEXT_DIV4X_MIN,
	EN_DNLP_IRGNBGN,
	EN_DNLP_IRGNEND,
	EN_DNLP_FINAL_GAIN,
	EN_DNLP_CLIPRATE_V3,
	EN_DNLP_CLIPRATE_MIN,
	EN_DNLP_ADPCRAT_LBND,
	EN_DNLP_ADPCRAT_HBND,
	EN_DNLP_SCURV_LOW_TH,
	EN_DNLP_SCURV_MID1_TH,
	EN_DNLP_SCURV_MID2_TH,
	EN_DNLP_SCURV_HGH1_TH,
	EN_DNLP_SCURV_HGH2_TH,
	EN_DNLP_MTDRATE_ADP_EN,
	EN_DNLP_CLAHE_METHOD,
	EN_DNLP_BLE_EN,
	EN_DNLP_NORM,
	EN_DNLP_SCN_CHG_TH,
	EN_DNLP_IIR_STEP_MUX,
	EN_DNLP_SINGLE_BIN_BW,
	EN_DNLP_SINGLE_BIN_METHOD,
	EN_DNLP_REG_MAX_SLOP_1ST,
	EN_DNLP_REG_MAX_SLOP_MID,
	EN_DNLP_REG_MAX_SLOP_FIN,
	EN_DNLP_REG_MIN_SLOP_1ST,
	EN_DNLP_REG_MIN_SLOP_MID,
	EN_DNLP_REG_MIN_SLOP_FIN,
	EN_DNLP_REG_TREND_WHT_EXPAND_MODE,
	EN_DNLP_REG_TREND_BLK_EXPAND_MODE,
	EN_DNLP_HIST_CUR_GAIN,
	EN_DNLP_HIST_CUR_GAIN_PRECISE,
	EN_DNLP_REG_MONO_BINRANG_ST,
	EN_DNLP_REG_MONO_BINRANG_ED,
	EN_DNLP_C_HIST_GAIN_BASE,
	EN_DNLP_S_HIST_GAIN_BASE,
	EN_DNLP_MVREFLSH_OFFSET,
	EN_DNLP_LUMA_AVG_TH,
	EN_DNLP_PARAM_MAX,
};

enum vpp_data_src_e {
	EN_SRC_VGA = 0,
	EN_SRC_ATV_NTSC,
	EN_SRC_ATV_PAL,
	EN_SRC_ATV_PAL_M,
	EN_SRC_ATV_SECAM,
	EN_SRC_ATV_NTSC443,
	EN_SRC_ATV_PAL_N60,
	EN_SRC_ATV_NTSC50,
	EN_SRC_ATV_PAL_N,
	EN_SRC_AV_NTSC,
	EN_SRC_AV_PAL, /*10*/
	EN_SRC_AV_PAL_M,
	EN_SRC_AV_SECAM,
	EN_SRC_AV_NTSC443,
	EN_SRC_AV_PAL_N60,
	EN_SRC_AV_NTSC50,
	EN_SRC_AV_PAL_N,
	EN_SRC_SV_NTSC,
	EN_SRC_SV_PAL,
	EN_SRC_SV_PAL_M,
	EN_SRC_SV_SECAM, /*20*/
	EN_SRC_SCART_AV,
	EN_SRC_SCART_RGB,
	EN_SRC_COMPONENT_480I,
	EN_SRC_COMPONENT_480P,
	EN_SRC_COMPONENT_576I,
	EN_SRC_COMPONENT_576P,
	EN_SRC_COMPONENT_720P,
	EN_SRC_COMPONENT_1080I,
	EN_SRC_COMPONENT_1080P,
	EN_SRC_HDMI_480I, /*30*/
	EN_SRC_HDMI_480P,
	EN_SRC_HDMI_576I,
	EN_SRC_HDMI_576P,
	EN_SRC_HDMI_720P,
	EN_SRC_HDMI_1080I,
	EN_SRC_HDMI_1080P,
	EN_SRC_HDMI_4K,
	EN_SRC_HDMI_8K,
	EN_SRC_MM_480I,
	EN_SRC_MM_480P, /*40*/
	EN_SRC_MM_576I,
	EN_SRC_MM_576P,
	EN_SRC_MM_720P,
	EN_SRC_MM_1080I,
	EN_SRC_MM_1080P,
	EN_SRC_MM_4K,
	EN_SRC_MM_8K,
	EN_SRC_DTV_480I,
	EN_SRC_DTV_480P,
	EN_SRC_DTV_576I, /*50*/
	EN_SRC_DTV_576P,
	EN_SRC_DTV_720P,
	EN_SRC_DTV_1080I,
	EN_SRC_DTV_1080P,
	EN_SRC_DTV_4K,
	EN_SRC_DTV_8K,
	EN_SRC_HDMI_HDR10,
	EN_SRC_HDMI_HLG,
	EN_SRC_HDMI_HDR10P,
	EN_SRC_HDMI_CUVA_HDR, /*60*/
	EN_SRC_HDMI_CUVA_HLG,
	EN_SRC_HDMI_DOBVI,
	EN_SRC_MM_HDR10,
	EN_SRC_MM_HLG,
	EN_SRC_MM_HDR10P,
	EN_SRC_MM_CUVA_HDR,
	EN_SRC_MM_CUVA_HLG,
	EN_SRC_MM_DOBVI,
	EN_SRC_DTV_HDR10,
	EN_SRC_DTV_HLG, /*70*/
	EN_SRC_DTV_HDR10P,
	EN_SRC_DTV_CUVA_HDR,
	EN_SRC_DTV_CUVA_HLG,
	EN_SRC_DTV_DOBVI,
	EN_SRC_INDEX_MAX,
};

/*Commom struct*/
struct vpp_pq_ctrl_s {
	unsigned char vadj1_en;    /*control video brightness contrast saturation hue*/
	unsigned char vd1_ctrst_en;
	unsigned char vadj2_en;    /*control video+osd brightness contrast saturation hue*/
	unsigned char post_ctrst_en;
	unsigned char pregamma_en;
	unsigned char gamma_en;
	unsigned char wb_en;
	unsigned char dnlp_en;
	unsigned char lc_en;
	unsigned char black_ext_en;
	unsigned char chroma_cor_en;
	unsigned char sharpness0_en;
	unsigned char sharpness1_en;
	unsigned char cm_en;
	unsigned char reserved;
};

struct vpp_pq_state_s {
	int pq_en;
	struct vpp_pq_ctrl_s pq_cfg;
};

struct vpp_white_balance_s {
	int r_pre_offset;     /*s11.0, range -1024~+1023, default is 0*/
	int g_pre_offset;     /*s11.0, range -1024~+1023, default is 0*/
	int b_pre_offset;     /*s11.0, range -1024~+1023, default is 0*/
	unsigned int r_gain;  /*u1.10, range 0~2047, default is 1024 (1.0x)*/
	unsigned int g_gain;  /*u1.10, range 0~2047, default is 1024 (1.0x)*/
	unsigned int b_gain;  /*u1.10, range 0~2047, default is 1024 (1.0x)*/
	int r_post_offset;    /*s11.0, range -1024~+1023, default is 0*/
	int g_post_offset;    /*s11.0, range -1024~+1023, default is 0*/
	int b_post_offset;    /*s11.0, range -1024~+1023, default is 0*/
};

struct vpp_pre_gamma_table_s {
	unsigned int r_data[VPP_PRE_GAMMA_TABLE_LEN];
	unsigned int g_data[VPP_PRE_GAMMA_TABLE_LEN];
	unsigned int b_data[VPP_PRE_GAMMA_TABLE_LEN];
};

struct vpp_gamma_table_s {
	unsigned int r_data[VPP_GAMMA_TABLE_LEN];
	unsigned int g_data[VPP_GAMMA_TABLE_LEN];
	unsigned int b_data[VPP_GAMMA_TABLE_LEN];
};

struct vpp_mtrx_param_s {
	unsigned int pre_offset[VPP_MTRX_OFFSET_LEN];
	unsigned int matrix_coef[VPP_MTRX_COEF_LEN];
	unsigned int post_offset[VPP_MTRX_OFFSET_LEN];
	unsigned int right_shift;
};

struct vpp_mtrx_info_s {
	enum vpp_mtrx_type_e mtrx_sel;
	struct vpp_mtrx_param_s mtrx_param;
};

struct vpp_histgm_ave_s {
	unsigned int sum;
	int width;
	int height;
	int ave;
};

struct vpp_histgm_param_s {
	unsigned int hist_pow;
	unsigned int luma_sum;
	unsigned int pixel_sum;
	unsigned int histgm[VPP_HIST_BIN_COUNT];
	unsigned int hue_histgm[VPP_COLOR_HIST_BIN_COUNT];
	unsigned int sat_histgm[VPP_COLOR_HIST_BIN_COUNT];
};

/*master_display_info for display device*/
struct vpp_hdr_metadata_s {
	u32 primaries[3][2]; /*normalized 50000 in G,B,R order*/
	u32 white_point[2];  /*normalized 50000*/
	u32 luminance[2];    /*max/min luminance, normalized 10000*/
};

struct vpp_hdr_tone_mapping_s {
	enum vpp_hdr_lut_type_e lut_type;
	unsigned int lutlength;
	union {
		void *tm_lut;
		long long tm_lut_len;
	};
};

struct vpp_tmo_param_s {
	int tmo_en;
	int reg_highlight;
	int reg_hist_th;
	int reg_light_th;
	int reg_highlight_th1;
	int reg_highlight_th2;
	int reg_display_e;
	int reg_middle_a;
	int reg_middle_a_adj;
	int reg_middle_b;
	int reg_middle_s;
	int reg_max_th1;
	int reg_middle_th;
	int reg_thold1;
	int reg_thold2;
	int reg_thold3;
	int reg_thold4;
	int reg_max_th2;
	int reg_pnum_th;
	int reg_hl0;
	int reg_hl1;
	int reg_hl2;
	int reg_hl3;
	int reg_display_adj;
	int reg_avg_th;
	int reg_avg_adj;
	int reg_low_adj;
	int reg_high_en;
	int reg_high_adj1;
	int reg_high_adj2;
	int reg_high_maxdiff;
	int reg_high_mindiff;
	unsigned int alpha;
};

struct vpp_lc_curve_s {
	unsigned int lc_saturation[63];
	unsigned int lc_yminval_lmt[16];
	unsigned int lc_ypkbv_ymaxval_lmt[16];
	unsigned int lc_ymaxval_lmt[16];
	unsigned int lc_ypkbv_lmt[16];
	unsigned int lc_ypkbv_ratio[4];
	unsigned int param[100];
};

struct vpp_cabc_aad_param_s {
	unsigned int length;
	union {
		void *cabc_aad_param_ptr;
		long long cabc_aad_param_ptr_len;
	};
};

struct vpp_aad_param_s {
	int aad_param_cabc_aad_en;
	int aad_param_aad_en;
	int aad_param_tf_en;
	int aad_param_force_gain_en;
	int aad_param_sensor_mode;
	int aad_param_mode;
	int aad_param_dist_mode;
	int aad_param_tf_alpha;
	int aad_param_sensor_input[3];
	struct vpp_cabc_aad_param_s db_LUT_Y_gain;
	struct vpp_cabc_aad_param_s db_LUT_RG_gain;
	struct vpp_cabc_aad_param_s db_LUT_BG_gain;
	struct vpp_cabc_aad_param_s db_gain_lut;
	struct vpp_cabc_aad_param_s db_xy_lut;
};

struct vpp_cabc_param_s {
	int cabc_param_cabc_en;
	int cabc_param_hist_mode;
	int cabc_param_tf_en;
	int cabc_param_sc_flag;
	int cabc_param_bl_map_mode;
	int cabc_param_bl_map_en;
	int cabc_param_temp_proc;
	int cabc_param_max95_ratio;
	int cabc_param_hist_blend_alpha;
	int cabc_param_init_bl_min;
	int cabc_param_init_bl_max;
	int cabc_param_tf_alpha;
	int cabc_param_sc_hist_diff_thd;
	int cabc_param_sc_apl_diff_thd;
	int cabc_param_patch_bl_th;
	int cabc_param_patch_on_alpha;
	int cabc_param_patch_bl_off_th;
	int cabc_param_patch_off_alpha;
	struct vpp_cabc_aad_param_s db_o_bl_cv;
	struct vpp_cabc_aad_param_s db_maxbin_bl_cv;
};

struct vpp_dnlp_curve_param_s {
	unsigned int dnlp_scurv_low[VPP_DNLP_SCURV_LEN];
	unsigned int dnlp_scurv_mid1[VPP_DNLP_SCURV_LEN];
	unsigned int dnlp_scurv_mid2[VPP_DNLP_SCURV_LEN];
	unsigned int dnlp_scurv_hgh1[VPP_DNLP_SCURV_LEN];
	unsigned int dnlp_scurv_hgh2[VPP_DNLP_SCURV_LEN];
	unsigned int gain_var_lut49[VPP_DNLP_GAIN_VAR_LUT_LEN];
	unsigned int wext_gain[VPP_DNLP_WEXT_GAIN_LEN];
	unsigned int adp_thrd[VPP_DNLP_ADP_THRD_LEN];
	unsigned int reg_blk_boost_12[VPP_DNLP_REG_BLK_BOOST_LEN];
	unsigned int reg_adp_ofset_20[VPP_DNLP_REG_ADP_OFSET_LEN];
	unsigned int reg_mono_protect[VPP_DNLP_REG_MONO_PROT_LEN];
	unsigned int reg_trend_wht_expand_lut8[VPP_DNLP_TREND_WHT_EXP_LUT_LEN];
	unsigned int c_hist_gain[VPP_DNLP_HIST_GAIN_LEN];
	unsigned int s_hist_gain[VPP_DNLP_HIST_GAIN_LEN];
	unsigned int param[EN_DNLP_PARAM_MAX];
};

struct vpp_module_ctrl_s {
	enum vpp_module_e module_type;
	int status;
};

struct vpp_pq_tuning_reg_s {
	unsigned char reg_addr;
	unsigned char mask_type;
	unsigned int val;
};

struct vpp_pq_tuning_page_s {
	unsigned char page_addr;
	int page_idx;
	int reg_count;
	struct vpp_pq_tuning_reg_s *preg_list;
};

struct vpp_pq_tuning_table_s {
	int page_count;
	struct vpp_pq_tuning_page_s *ppage_list;
};

/*IOCtl*/
#define VPP_IOC_SET_BRIGHTNESS      _IOW(_VPP_TYPE, 0x01, int)
#define VPP_IOC_SET_CONTRAST        _IOW(_VPP_TYPE, 0x02, int)
#define VPP_IOC_SET_SATURATION      _IOW(_VPP_TYPE, 0x03, int)
#define VPP_IOC_SET_HUE             _IOW(_VPP_TYPE, 0x04, int)
#define VPP_IOC_SET_SHARPNESS       _IOW(_VPP_TYPE, 0x05, int)
#define VPP_IOC_SET_BRIGHTNESS_POST _IOW(_VPP_TYPE, 0x06, int)
#define VPP_IOC_SET_CONTRAST_POST   _IOW(_VPP_TYPE, 0x07, int)
#define VPP_IOC_SET_SATURATION_POST _IOW(_VPP_TYPE, 0x08, int)
#define VPP_IOC_SET_HUE_POST        _IOW(_VPP_TYPE, 0x09, int)
#define VPP_IOC_SET_WB              _IOW(_VPP_TYPE, 0x0a, struct vpp_white_balance_s)
#define VPP_IOC_SET_PRE_GAMMA_DATA  _IOW(_VPP_TYPE, 0x0b, struct vpp_pre_gamma_table_s)
#define VPP_IOC_SET_GAMMA_DATA      _IOW(_VPP_TYPE, 0x0c, struct vpp_gamma_table_s)
#define VPP_IOC_SET_MATRIX_PARAM    _IOW(_VPP_TYPE, 0x0d, struct vpp_mtrx_info_s)
#define VPP_IOC_SET_MODULE_STATUS   _IOW(_VPP_TYPE, 0x0e, struct vpp_module_ctrl_s)
#define VPP_IOC_SET_PQ_STATE        _IOW(_VPP_TYPE, 0x0f, struct vpp_pq_state_s)
#define VPP_IOC_SET_PC_MODE         _IOW(_VPP_TYPE, 0x10, enum vpp_pc_mode_e)

#define VPP_IOC_SET_DNLP_PARAM      _IOW(_VPP_TYPE, 0x11, struct vpp_dnlp_curve_param_s)
#define VPP_IOC_SET_LC_CURVE        _IOW(_VPP_TYPE, 0x12, struct vpp_lc_curve_s)
#define VPP_IOC_SET_CSC_TYPE        _IOW(_VPP_TYPE, 0x13 enum vpp_csc_type_e)
#define VPP_IOC_SET_3DLUT_DATA      _IOW(_VPP_TYPE, 0x14, int)

#define VPP_IOC_GET_PC_MODE         _IOR(_VPP_TYPE, 0x80, enum vpp_pc_mode_e)
#define VPP_IOC_GET_CSC_TYPE        _IOR(_VPP_TYPE, 0x81, enum vpp_csc_type_e)
#define VPP_IOC_GET_HDR_TYPE        _IOR(_VPP_TYPE, 0x82, enum vpp_hdr_type_e)
#define VPP_IOC_GET_COLOR_PRIM      _IOR(_VPP_TYPE, 0x83, enum vpp_color_primary_e)
#define VPP_IOC_GET_HDR_METADATA    _IOR(_VPP_TYPE, 0x84, struct vpp_hdr_metadata_s)
#define VPP_IOC_GET_HIST_AVG        _IOR(_VPP_TYPE, 0x85, struct vpp_histgm_ave_s)
#define VPP_IOC_GET_HIST_BIN        _IOR(_VPP_TYPE, 0x86, struct vpp_histgm_param_s)
#define VPP_IOC_GET_PQ_STATE        _IOR(_VPP_TYPE, 0x87, struct vpp_pq_state_s)

#endif

