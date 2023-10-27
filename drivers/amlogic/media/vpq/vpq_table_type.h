/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _VPQ_PQTABLE_TYPE_H_
#define _VPQ_PQTABLE_TYPE_H_

#define PQ_INDEX_TABLE_SIZE                 (25)
#define GAMMA_TABLE_NUM_MAX                 (20)
#define CTEMP_TABLE_NUM_MAX                 (4)
#define DNLP_TABLE_NUM_MAX                  (2)
#define HDR_TMO_TABLE_NUM_MAX               (3)
#define AI_PQ_TABLE_NUM_MAX                 (6)
#define LC_TABLE_NUM_MAX                    (6)
#define CCORING_TABLE_NUM_MAX               (9)
#define BLKEXT_TABLE_NUM_MAX                (9)
#define BLUE_STR_TABLE_NUM_MAX              (9)
#define COLOR_BASE_TABLE_NUM_MAX            (6)
#define AI_SR_TABLE_NUM_MAX                 (2)

#define VPQ_DNLP_SCURV_LEN					(65)
#define VPQ_DNLP_GAIN_VAR_LUT_LEN			(49)
#define VPQ_DNLP_WEXT_GAIN_LEN				(48)
#define VPQ_DNLP_ADP_THRD_LEN				(33)
#define VPQ_DNLP_REG_BLK_BOOST_LEN			(13)
#define VPQ_DNLP_REG_ADP_OFSET_LEN			(20)
#define VPQ_DNLP_REG_MONO_PROT_LEN			(6)
#define VPQ_DNLP_TREND_WHT_EXP_LUT_LEN		(9)
#define VPQ_DNLP_HIST_GAIN_LEN				(65)

#define VPQ_GAMMA_TABLE_LEN                 (256)

struct TABLE_VER_PQ {
	char ProjectVersion[64];
	char ChipVersion[64];
	char TableVersion[64];
	char oem_model[64];
	char PanelIndex[64];
	char reserved[64];
};

struct TABLE_PQ_MODULE_CFG {
	unsigned char pq_en;          //pq.AllPQModule.en
	unsigned char vadj1_en;       //pq.amvecm.basic.en
	unsigned char vd1_ctrst_en;   //pq.contrast.rgb.en
	unsigned char vadj2_en;       //pq.amvecm.basic.withOSD.en
	unsigned char post_ctrst_en;  //pq.contrast.rgb.withOSD.en
	unsigned char pregamma_en;
	unsigned char gamma_en;       //pq.gamma.en
	unsigned char wb_en;          //pq.whitebalance.en
	unsigned char dnlp_en;        //pq.dnlp.en
	unsigned char lc_en;          //pq.LocalContrast.en
	unsigned char black_ext_en;   //pq.BlackExtension.en
	unsigned char blue_stretch_en;//pq.BlueStretch.en
	unsigned char chroma_cor_en;  //pq.chroma.cor.en
	unsigned char sharpness0_en;  //pq.sharpness0.en
	unsigned char sharpness1_en;  //pq.sharpness1.en
	unsigned char cm_en;          //pq.cm2.en
	unsigned char lut3d_en;
	unsigned char dejaggy_sr0_en;
	unsigned char dejaggy_sr1_en;
	unsigned char dering_sr0_en;
	unsigned char dering_sr1_en;
	unsigned char reserved;
};

struct gamma_table_s {
	unsigned int R[VPQ_GAMMA_TABLE_LEN];
	unsigned int G[VPQ_GAMMA_TABLE_LEN];
	unsigned int B[VPQ_GAMMA_TABLE_LEN];
};

struct pq_tcon_gamma_table_s {
	struct gamma_table_s gamma_data[GAMMA_TABLE_NUM_MAX];
};

struct ctemp_table_s {
	unsigned short R[VPQ_GAMMA_TABLE_LEN];
	unsigned short G[VPQ_GAMMA_TABLE_LEN];
	unsigned short B[VPQ_GAMMA_TABLE_LEN];
};

struct pq_ctemp_gamma_table_s {
	struct ctemp_table_s ctemp_data[CTEMP_TABLE_NUM_MAX];
};

enum pq_index_table_index_e {
	PQ_INDEX_DI = 0,
	PQ_INDEX_DNLP,
	PQ_INDEX_HDR_TMO,
	PQ_INDEX_LC,
	PQ_INDEX_AIPQ,
	PQ_INDEX_CM2,
	PQ_INDEX_SR,
	PQ_INDEX_3DLUT,
	PQ_INDEX_BLACKEXT,
	PQ_INDEX_NR,
	PQ_INDEX_GAMMA,
	PQ_INDEX_CVD2NEW,
	PQ_INDEX_DEMOSQUITO,
	PQ_INDEX_DEBLOCK,
	PQ_INDEX_BLUE_STRETCH,
	PQ_INDEX_MCDI,
	PQ_INDEX_SHARPNESS,
	PQ_INDEX_SMOOTHPLUS,
	PQ_INDEX_CHROMA_CORING,
	PQ_INDEX_FILM,
	PQ_INDEX_AISR,
	PQ_INDEX_RESOLVED1,
	PQ_INDEX_RESOLVED2,
	PQ_INDEX_RESOLVED3,
	PQ_INDEX_RESOLVED4,
	PQ_INDEX_MAX,
};

enum pq_source_timing_e {
	PQ_SRC_INDEX_VGA = 0,

	PQ_SRC_INDEX_ATV_NTSC, /*1*/
	PQ_SRC_INDEX_ATV_PAL,
	PQ_SRC_INDEX_ATV_PAL_M,
	PQ_SRC_INDEX_ATV_SECAN,
	PQ_SRC_INDEX_ATV_NTSC443,
	PQ_SRC_INDEX_ATV_PAL60,
	PQ_SRC_INDEX_ATV_NTSC50,
	PQ_SRC_INDEX_ATV_PAL_N,

	PQ_SRC_INDEX_AV_NTSC, /*9*/
	PQ_SRC_INDEX_AV_PAL,
	PQ_SRC_INDEX_AV_PAL_M,
	PQ_SRC_INDEX_AV_SECAN,
	PQ_SRC_INDEX_AV_NTSC443,
	PQ_SRC_INDEX_AV_PAL60,
	PQ_SRC_INDEX_AV_NTSC50,
	PQ_SRC_INDEX_AV_PAL_N,

	PQ_SRC_INDEX_SV_NTSC, /*17*/
	PQ_SRC_INDEX_SV_PAL,
	PQ_SRC_INDEX_SV_PAL_M,
	PQ_SRC_INDEX_SV_SECAM,

	PQ_SRC_INDEX_YCBCR_480I, /*21*/
	PQ_SRC_INDEX_YCBCR_576I,
	PQ_SRC_INDEX_YCBCR_480P,
	PQ_SRC_INDEX_YCBCR_576P,
	PQ_SRC_INDEX_YCBCR_720P,
	PQ_SRC_INDEX_YCBCR_1080I,
	PQ_SRC_INDEX_YCBCR_1080P,

	PQ_SRC_INDEX_HDMI_480I, /*28*/
	PQ_SRC_INDEX_HDMI_576I,
	PQ_SRC_INDEX_HDMI_480P,
	PQ_SRC_INDEX_HDMI_576P,
	PQ_SRC_INDEX_HDMI_720P,
	PQ_SRC_INDEX_HDMI_1080I,
	PQ_SRC_INDEX_HDMI_1080P,
	PQ_SRC_INDEX_HDMI_4K2KI,
	PQ_SRC_INDEX_HDMI_4K2KP,
	PQ_SRC_INDEX_HDR10_HDMI_480I,
	PQ_SRC_INDEX_HDR10_HDMI_576I,
	PQ_SRC_INDEX_HDR10_HDMI_480P,
	PQ_SRC_INDEX_HDR10_HDMI_576P,
	PQ_SRC_INDEX_HDR10_HDMI_720P,
	PQ_SRC_INDEX_HDR10_HDMI_1080I,
	PQ_SRC_INDEX_HDR10_HDMI_1080P,
	PQ_SRC_INDEX_HDR10_HDMI_4K2KI,
	PQ_SRC_INDEX_HDR10_HDMI_4K2KP,
	PQ_SRC_INDEX_HLG_HDMI_480I,
	PQ_SRC_INDEX_HLG_HDMI_576I,
	PQ_SRC_INDEX_HLG_HDMI_480P,
	PQ_SRC_INDEX_HLG_HDMI_576P,
	PQ_SRC_INDEX_HLG_HDMI_720P,
	PQ_SRC_INDEX_HLG_HDMI_1080I,
	PQ_SRC_INDEX_HLG_HDMI_1080P,
	PQ_SRC_INDEX_HLG_HDMI_4K2KI,
	PQ_SRC_INDEX_HLG_HDMI_4K2KP,
	PQ_SRC_INDEX_DV_HDMI_480I,
	PQ_SRC_INDEX_DV_HDMI_576I,
	PQ_SRC_INDEX_DV_HDMI_480P,
	PQ_SRC_INDEX_DV_HDMI_576P,
	PQ_SRC_INDEX_DV_HDMI_720P,
	PQ_SRC_INDEX_DV_HDMI_1080I,
	PQ_SRC_INDEX_DV_HDMI_1080P,
	PQ_SRC_INDEX_DV_HDMI_4K2KI,
	PQ_SRC_INDEX_DV_HDMI_4K2KP,
	PQ_SRC_INDEX_HDR10p_HDMI_480I,
	PQ_SRC_INDEX_HDR10p_HDMI_576I,
	PQ_SRC_INDEX_HDR10p_HDMI_480P,
	PQ_SRC_INDEX_HDR10p_HDMI_576P,
	PQ_SRC_INDEX_HDR10p_HDMI_720P,
	PQ_SRC_INDEX_HDR10p_HDMI_1080I,
	PQ_SRC_INDEX_HDR10p_HDMI_1080P,
	PQ_SRC_INDEX_HDR10p_HDMI_4K2KI,
	PQ_SRC_INDEX_HDR10p_HDMI_4K2KP,

	PQ_SRC_INDEX_DTV_480I, /*73*/
	PQ_SRC_INDEX_DTV_576I,
	PQ_SRC_INDEX_DTV_480P,
	PQ_SRC_INDEX_DTV_576P,
	PQ_SRC_INDEX_DTV_720P,
	PQ_SRC_INDEX_DTV_1080I,
	PQ_SRC_INDEX_DTV_1080P,
	PQ_SRC_INDEX_DTV_4K2KI,
	PQ_SRC_INDEX_DTV_4K2KP,
	PQ_SRC_INDEX_HDR10_DTV_480I,
	PQ_SRC_INDEX_HDR10_DTV_576I,
	PQ_SRC_INDEX_HDR10_DTV_480P,
	PQ_SRC_INDEX_HDR10_DTV_576P,
	PQ_SRC_INDEX_HDR10_DTV_720P,
	PQ_SRC_INDEX_HDR10_DTV_1080I,
	PQ_SRC_INDEX_HDR10_DTV_1080P,
	PQ_SRC_INDEX_HDR10_DTV_4K2KI,
	PQ_SRC_INDEX_HDR10_DTV_4K2KP,
	PQ_SRC_INDEX_HLG_DTV_480I,
	PQ_SRC_INDEX_HLG_DTV_576I,
	PQ_SRC_INDEX_HLG_DTV_480P,
	PQ_SRC_INDEX_HLG_DTV_576P,
	PQ_SRC_INDEX_HLG_DTV_720P,
	PQ_SRC_INDEX_HLG_DTV_1080I,
	PQ_SRC_INDEX_HLG_DTV_1080P,
	PQ_SRC_INDEX_HLG_DTV_4K2KI,
	PQ_SRC_INDEX_HLG_DTV_4K2KP,
	PQ_SRC_INDEX_DV_DTV_480I,
	PQ_SRC_INDEX_DV_DTV_576I,
	PQ_SRC_INDEX_DV_DTV_480P,
	PQ_SRC_INDEX_DV_DTV_576P,
	PQ_SRC_INDEX_DV_DTV_720P,
	PQ_SRC_INDEX_DV_DTV_1080I,
	PQ_SRC_INDEX_DV_DTV_1080P,
	PQ_SRC_INDEX_DV_DTV_4K2KI,
	PQ_SRC_INDEX_DV_DTV_4K2KP,
	PQ_SRC_INDEX_HDR10p_DTV_480I,
	PQ_SRC_INDEX_HDR10p_DTV_576I,
	PQ_SRC_INDEX_HDR10p_DTV_480P,
	PQ_SRC_INDEX_HDR10p_DTV_576P,
	PQ_SRC_INDEX_HDR10p_DTV_720P,
	PQ_SRC_INDEX_HDR10p_DTV_1080I,
	PQ_SRC_INDEX_HDR10p_DTV_1080P,
	PQ_SRC_INDEX_HDR10p_DTV_4K2KI,
	PQ_SRC_INDEX_HDR10p_DTV_4K2KP,

	PQ_SRC_INDEX_MPEG_480I, /*118*/
	PQ_SRC_INDEX_MPEG_576I,
	PQ_SRC_INDEX_MPEG_480P,
	PQ_SRC_INDEX_MPEG_576P,
	PQ_SRC_INDEX_MPEG_720P,
	PQ_SRC_INDEX_MPEG_1080I,
	PQ_SRC_INDEX_MPEG_1080P,
	PQ_SRC_INDEX_MPEG_4K2KI,
	PQ_SRC_INDEX_MPEG_4K2KP,
	PQ_SRC_INDEX_HDR10_MPEG_480I,
	PQ_SRC_INDEX_HDR10_MPEG_576I,
	PQ_SRC_INDEX_HDR10_MPEG_480P,
	PQ_SRC_INDEX_HDR10_MPEG_576P,
	PQ_SRC_INDEX_HDR10_MPEG_720P,
	PQ_SRC_INDEX_HDR10_MPEG_1080I,
	PQ_SRC_INDEX_HDR10_MPEG_1080P,
	PQ_SRC_INDEX_HDR10_MPEG_4K2KI,
	PQ_SRC_INDEX_HDR10_MPEG_4K2KP,
	PQ_SRC_INDEX_HLG_MPEG_480I,
	PQ_SRC_INDEX_HLG_MPEG_576I,
	PQ_SRC_INDEX_HLG_MPEG_480P,
	PQ_SRC_INDEX_HLG_MPEG_576P,
	PQ_SRC_INDEX_HLG_MPEG_720P,
	PQ_SRC_INDEX_HLG_MPEG_1080I,
	PQ_SRC_INDEX_HLG_MPEG_1080P,
	PQ_SRC_INDEX_HLG_MPEG_4K2KI,
	PQ_SRC_INDEX_HLG_MPEG_4K2KP,
	PQ_SRC_INDEX_DV_MPEG_480I,
	PQ_SRC_INDEX_DV_MPEG_576I,
	PQ_SRC_INDEX_DV_MPEG_480P,
	PQ_SRC_INDEX_DV_MPEG_576P,
	PQ_SRC_INDEX_DV_MPEG_720P,
	PQ_SRC_INDEX_DV_MPEG_1080I,
	PQ_SRC_INDEX_DV_MPEG_1080P,
	PQ_SRC_INDEX_DV_MPEG_4K2KI,
	PQ_SRC_INDEX_DV_MPEG_4K2KP,
	PQ_SRC_INDEX_HDR10p_MPEG_480I,
	PQ_SRC_INDEX_HDR10p_MPEG_576I,
	PQ_SRC_INDEX_HDR10p_MPEG_480P,
	PQ_SRC_INDEX_HDR10p_MPEG_576P,
	PQ_SRC_INDEX_HDR10p_MPEG_720P,
	PQ_SRC_INDEX_HDR10p_MPEG_1080I,
	PQ_SRC_INDEX_HDR10p_MPEG_1080P,
	PQ_SRC_INDEX_HDR10p_MPEG_4K2KI,
	PQ_SRC_INDEX_HDR10p_MPEG_4K2KP,

	PQ_SRC_INDEX_MAX, /*163*/
};

enum pq_dnlp_param_e {
	PQ_DNLP_SMHIST_CK = 0,
	PQ_DNLP_MVREFLSH,
	PQ_DNLP_CUVBLD_MIN,
	PQ_DNLP_CUVBLD_MAX,
	PQ_DNLP_BBD_RATIO_LOW,
	PQ_DNLP_BBD_RATIO_HIG,
	PQ_DNLP_BLK_CCTR,
	PQ_DNLP_BRGT_CTRL,
	PQ_DNLP_BRGT_RANGE,
	PQ_DNLP_BRGHT_ADD,
	PQ_DNLP_BRGHT_MAX,
	PQ_DNLP_AUTO_RNG,
	PQ_DNLP_LOWRANGE,
	PQ_DNLP_HGHRANGE,
	PQ_DNLP_SATUR_RAT,
	PQ_DNLP_SATUR_MAX,
	PQ_DNLP_SBGNBND,
	PQ_DNLP_SENDBND,
	PQ_DNLP_CLASHBGN,
	PQ_DNLP_CLASHEND,
	PQ_DNLP_CLAHE_GAIN_NEG,
	PQ_DNLP_CLAHE_GAIN_POS,
	PQ_DNLP_MTDBLD_RATE,
	PQ_DNLP_ADPMTD_LBND,
	PQ_DNLP_ADPMTD_HBND,
	PQ_DNLP_BLKEXT_OFST,
	PQ_DNLP_WHTEXT_OFST,
	PQ_DNLP_BLKEXT_RATE,
	PQ_DNLP_WHTEXT_RATE,
	PQ_DNLP_BWEXT_DIV4X_MIN,
	PQ_DNLP_IRGNBGN,
	PQ_DNLP_IRGNEND,
	PQ_DNLP_FINAL_GAIN,
	PQ_DNLP_CLIPRATE_V3,
	PQ_DNLP_CLIPRATE_MIN,
	PQ_DNLP_ADPCRAT_LBND,
	PQ_DNLP_ADPCRAT_HBND,
	PQ_DNLP_SCURV_LOW_TH,
	PQ_DNLP_SCURV_MID1_TH,
	PQ_DNLP_SCURV_MID2_TH,
	PQ_DNLP_SCURV_HGH1_TH,
	PQ_DNLP_SCURV_HGH2_TH,
	PQ_DNLP_MTDRATE_ADP_EN,
	PQ_DNLP_CLAHE_METHOD,
	PQ_DNLP_BLE_EN,
	PQ_DNLP_NORM,
	PQ_DNLP_SCN_CHG_TH,
	PQ_DNLP_IIR_STEP_MUX,
	PQ_DNLP_SINGLE_BIN_BW,
	PQ_DNLP_SINGLE_BIN_METHOD,
	PQ_DNLP_REG_MAX_SLOP_1ST,
	PQ_DNLP_REG_MAX_SLOP_MID,
	PQ_DNLP_REG_MAX_SLOP_FIN,
	PQ_DNLP_REG_MIN_SLOP_1ST,
	PQ_DNLP_REG_MIN_SLOP_MID,
	PQ_DNLP_REG_MIN_SLOP_FIN,
	PQ_DNLP_REG_TREND_WHT_EXPAND_MODE,
	PQ_DNLP_REG_TREND_BLK_EXPAND_MODE,
	PQ_DNLP_HIST_CUR_GAIN,
	PQ_DNLP_HIST_CUR_GAIN_PRECISE,
	PQ_DNLP_REG_MONO_BINRANG_ST,
	PQ_DNLP_REG_MONO_BINRANG_ED,
	PQ_DNLP_C_HIST_GAIN_BASE,
	PQ_DNLP_S_HIST_GAIN_BASE,
	PQ_DNLP_MVREFLSH_OFFSET,
	PQ_DNLP_LUMA_AVG_TH,

	PQ_DNLP_ENABLE, //moren than "vpp_dnlp_param_e"
	PQ_DNLP_PARAM_MAX,
};

struct pq_dnlp_curve_param_s {
	unsigned int dnlp_scurv_low[VPQ_DNLP_SCURV_LEN];
	unsigned int dnlp_scurv_mid1[VPQ_DNLP_SCURV_LEN];
	unsigned int dnlp_scurv_mid2[VPQ_DNLP_SCURV_LEN];
	unsigned int dnlp_scurv_hgh1[VPQ_DNLP_SCURV_LEN];
	unsigned int dnlp_scurv_hgh2[VPQ_DNLP_SCURV_LEN];
	unsigned int gain_var_lut49[VPQ_DNLP_GAIN_VAR_LUT_LEN];
	unsigned int wext_gain[VPQ_DNLP_WEXT_GAIN_LEN];
	unsigned int adp_thrd[VPQ_DNLP_ADP_THRD_LEN];
	unsigned int reg_blk_boost_12[VPQ_DNLP_REG_BLK_BOOST_LEN];
	unsigned int reg_adp_ofset_20[VPQ_DNLP_REG_ADP_OFSET_LEN];
	unsigned int reg_mono_protect[VPQ_DNLP_REG_MONO_PROT_LEN];
	unsigned int reg_trend_wht_expand_lut8[VPQ_DNLP_TREND_WHT_EXP_LUT_LEN];
	unsigned int c_hist_gain[VPQ_DNLP_HIST_GAIN_LEN];
	unsigned int s_hist_gain[VPQ_DNLP_HIST_GAIN_LEN];
	unsigned int param[PQ_DNLP_PARAM_MAX];
};

enum pq_dnlp_level_e {
	DNLP_LV_OFF,
	DNLP_LV_LOW,
	DNLP_LV_MID,
	DNLP_LV_HIGH,
	DNLP_LV_MAX,
};

struct pq_lc_curve_parm_s {
	unsigned int lc_saturation[63];
	unsigned int lc_yminval_lmt[16];
	unsigned int lc_ypkbv_ymaxval_lmt[16];
	unsigned int lc_ymaxval_lmt[16];
	unsigned int lc_ypkbv_lmt[16];
	unsigned int lc_ypkbv_ratio[4];
	unsigned int param[100];
};

struct pq_lc_reg_parm_s {
	unsigned int lc_enable;
	unsigned int lc_curve_nodes_vlpf;
	unsigned int lc_curve_nodes_hlpf;
	unsigned int lmtrat_valid;
	unsigned int lmtrat_minmax;
	unsigned int lc_cntst_gain_hig;
	unsigned int lc_cntst_gain_low;
	unsigned int lc_cntst_lmt_hig_h;
	unsigned int lc_cntst_lmt_low_h;
	unsigned int lc_cntst_lmt_hig_l;
	unsigned int lc_cntst_lmt_low_l;
	unsigned int lc_cntst_scale_hig;
	unsigned int lc_cntst_scale_low;
	unsigned int lc_cntstbvn_hig;
	unsigned int lc_cntstbvn_low;
	unsigned int lc_slope_max_face;
	unsigned int lc_num_m_coring;
	unsigned int lc_slope_max;
	unsigned int lc_slope_min;
};

struct pq_hdr_tmo_sw_s {
	int tmo_en;              // 0 1
	int reg_highlight;       //u10: control overexposure level
	int reg_hist_th;         //u7
	int reg_light_th;
	int reg_highlight_th1;
	int reg_highlight_th2;
	int reg_display_e;       //u10
	int reg_middle_a;        //u7
	int reg_middle_a_adj;    //u10
	int reg_middle_b;        //u7
	int reg_middle_s;        //u7
	int reg_max_th1;          //u10
	int reg_middle_th;          //u10
	int reg_thold1;          //u10
	int reg_thold2;          //u10
	int reg_thold3;          //u10
	int reg_thold4;          //u10
	int reg_max_th2;          //u10
	int reg_pnum_th;          //u16
	int reg_hl0;
	int reg_hl1;             //u7
	int reg_hl2;             //u7
	int reg_hl3;             //u7
	int reg_display_adj;     //u7
	int reg_avg_th;
	int reg_avg_adj;
	int reg_low_adj;         //u7
	int reg_high_en;         //u3
	int reg_high_adj1;       //u7
	int reg_high_adj2;       //u7
	int reg_high_maxdiff;    //u7
	int reg_high_mindiff;    //u7
	unsigned int alpha;
};

enum pq_hdr_tmo_level_e {
	HDR_TMO_LV_OFF,
	HDR_TMO_LV_LOW,
	HDR_TMO_LV_MID,
	HDR_TMO_LV_HIGH,
	HDR_TMO_LV_MAX,
};

enum ai_pq_rows_e {
	AI_PQ_ROWS_SKIN = 0,
	AI_PQ_ROWS_BLUESKY,
	AI_PQ_ROWS_FOODS,
	AI_PQ_ROWS_ARCHITECTURE,
	AI_PQ_ROWS_GRASS,
	AI_PQ_ROWS_NIGHT,
	AI_PQ_ROWS_DOCUMENT,
	AI_PQ_ROWS_MAX,
};

enum ai_pq_cols_e {
	AI_PQ_COLS_BLUE = 0,
	AI_PQ_COLS_GREEN,
	AI_PQ_COLS_SKINTONE,
	AI_PQ_COLS_PEAKING,
	AI_PQ_COLS_SAT,
	AI_PQ_COLS_CONTRAST,
	AI_PQ_COLS_MAX,
};

struct pq_ai_pq_size_s {
	unsigned int height;
	unsigned int width;
};

struct pq_ai_pq_para_s {
	struct pq_ai_pq_size_s ai_size;
	unsigned int ai_pq_table[AI_PQ_ROWS_MAX][AI_PQ_COLS_MAX];
};

enum pq_lc_level_e {
	LC_LV_OFF,
	LC_LV_LOW,
	LC_LV_MID,
	LC_LV_HIGH,
	LC_LV_MAX,
};

struct pq_ccoring_parm_s {
	unsigned int chroma_coring_enable;
	unsigned int chroma_coring_slope;
	unsigned int chroma_coring_threshold;
	unsigned int chroma_coring_y_threshold;
};

enum pq_ccoring_level_e {
	CCORING_LV_OFF,
	CCORING_LV_LOW,
	CCORING_LV_MID,
	CCORING_LV_HIGH,
	CCORING_LV_MAX,
};

struct pq_blkext_parm_s {
	int blackext_enable;
	int blackext_start;
	int blackext_slope1;
	int blackext_mid_point;
	int blackext_slope2;
};

enum pq_blkext_level_e {
	BLKEXT_LV_OFF,
	BLKEXT_LV_LOW,
	BLKEXT_LV_MID,
	BLKEXT_LV_HIGH,
	BLKEXT_LV_MAX,
};

struct pq_blue_stretch_parm_s {
	int bluestretch_enable;
	int bluestretch_en_sel;
	int bluestretch_cb_inc;
	int bluestretch_cr_inc;
	int bluestretch_gain;
	int bluestretch_gain_cb4cr;
	int bluestretch_luma_high;
	int bluestretch_error_crp;
	int bluestretch_error_sign3;
	int bluestretch_error_crp_inv;
	int bluestretch_error_crn;
	int bluestretch_error_sign2;
	int bluestretch_error_crn_inv;
	int bluestretch_error_cbp;
	int bluestretch_error_sign1;
	int bluestretch_error_cbp_inv;
	int bluestretch_error_cbn;
	int bluestretch_error_sign0;
	int bluestretch_error_cbn_inv;
};

enum pq_blue_stretch_level_e {
	BLUESTR_LV_OFF,
	BLUESTR_LV_LOW,
	BLUESTR_LV_MID,
	BLUESTR_LV_HIGH,
	BLUESTR_LV_MAX,
};

struct pq_cm_base_parm_s {
	int satbyy_0[9];
	int lumabyhue_0[32];
	int satbyhs_0[32];
	int satbyhs_1[32];
	int satbyhs_2[32];
	int huebyhue_0[32];
	int huebyhy_0[32];
	int huebyhy_1[32];
	int huebyhy_2[32];
	int huebyhy_3[32];
	int huebyhy_4[32];
	int huebyhs_0[32];
	int huebyhs_1[32];
	int huebyhs_2[32];
	int huebyhs_3[32];
	int huebyhs_4[32];
	int satbyhy_0[32];
	int satbyhy_1[32];
	int satbyhy_2[32];
	int satbyhy_3[32];
	int satbyhy_4[32];
};

enum pq_color_base_level_e {
	CM_LV_OFF,
	CM_LV_LOW,
	CM_LV_MID,
	CM_LV_HIGH,
	CM_LV_MAX,
};

struct pq_aisr_parm_s {
	unsigned int reg_qt_clip_value;
	unsigned int reg_dering_en;
	unsigned int reg_dering_prot_thd;
	unsigned int reg_dering_thd0;
	unsigned int reg_dering_thd1;
	unsigned int reg_dering_slope;
	unsigned int reg_dering_coring_en;
	unsigned int reg_dering_coring_thd;
	unsigned int reg_adj_gain_neg;
	unsigned int reg_adj_gain_pos;
	unsigned int reg_adj_pos_en;
	unsigned int reg_adj_pos_thd0;
	unsigned int reg_adj_pos_thd1;
	unsigned int reg_adj_pos_thd2;
	unsigned int reg_adj_pos_top0;
	unsigned int reg_adj_pos_top1;
	unsigned int reg_adj_pos_top2;
	unsigned int reg_adj_pos_top3;
	unsigned int reg_adj_pos_slope0;
	unsigned int reg_adj_pos_slope1;
	unsigned int reg_adj_pos_slope2;
	unsigned int reg_adj_pos_slope3;
	unsigned int reg_adj_neg_en;
	unsigned int reg_adj_neg_thd0;
	unsigned int reg_adj_neg_thd1;
	unsigned int reg_adj_neg_thd2;
	unsigned int reg_adj_neg_top0;
	unsigned int reg_adj_neg_top1;
	unsigned int reg_adj_neg_top2;
	unsigned int reg_adj_neg_top3;
	unsigned int reg_adj_neg_slope0;
	unsigned int reg_adj_neg_slope1;
	unsigned int reg_adj_neg_slope2;
	unsigned int reg_adj_neg_slope3;
	unsigned int reg_adp_coring_en;
	unsigned int reg_adp_coring_thd;
	unsigned int reg_adp_glb_coring_thd;
};

struct pq_aisr_nn_parm_s {
	unsigned int reg_lrhf_hf_gain_neg;
	unsigned int reg_lrhf_hf_gain_pos;
	unsigned int reg_lrhf_lpf_coeff00;
	unsigned int reg_lrhf_lpf_coeff01;
	unsigned int reg_lrhf_lpf_coeff02;
	unsigned int reg_lrhf_lpf_coeff10;
	unsigned int reg_lrhf_lpf_coeff11;
	unsigned int reg_lrhf_lpf_coeff12;
	unsigned int reg_lrhf_lpf_coeff20;
	unsigned int reg_lrhf_lpf_coeff21;
	unsigned int reg_lrhf_lpf_coeff22;
};

struct pq_ai_sr_parm_s {
	struct pq_aisr_parm_s aisr_pram;
	struct pq_aisr_nn_parm_s aisr_nn_parm;
};

enum pq_ai_sr_level_e {
	AISR_LV_OFF,
	AISR_LV_LOW,
	AISR_LV_MID,
	AISR_LV_HIGH,
	AISR_LV_MAX,
};

enum pq_ai_sr_type_e {
	AISR_TYPE_X2,
	AISR_TYPE_X3_576,
	AISR_TYPE_X3_720,
	AISR_TYPE_X4,
	AISR_TYPE_MAX,
};

struct pq_cabc_aad_param_s {
	unsigned int length;
	union {
		void *cabc_aad_param_ptr;
		long long cabc_aad_param_ptr_len;
	};
};

struct pq_cabc_param_s {
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
	struct pq_cabc_aad_param_s db_o_bl_cv;
	struct pq_cabc_aad_param_s db_maxbin_bl_cv;
};

struct pq_aad_param_s {
	int aad_param_cabc_aad_en;
	int aad_param_aad_en;
	int aad_param_tf_en;
	int aad_param_force_gain_en;
	int aad_param_sensor_mode;
	int aad_param_mode;
	int aad_param_dist_mode;
	int aad_param_tf_alpha;
	int aad_param_sensor_input[3];
	struct pq_cabc_aad_param_s db_LUT_Y_gain;
	struct pq_cabc_aad_param_s db_LUT_RG_gain;
	struct pq_cabc_aad_param_s db_LUT_BG_gain;
	struct pq_cabc_aad_param_s db_gain_lut;
	struct pq_cabc_aad_param_s db_xy_lut;
};

struct PQ_TABLE_PARAM {
	unsigned char pq_index_table0[PQ_SRC_INDEX_MAX][PQ_INDEX_MAX];
	unsigned char pq_index_table1[PQ_SRC_INDEX_MAX][PQ_INDEX_MAX];
	struct pq_dnlp_curve_param_s dnlp_table[DNLP_TABLE_NUM_MAX][DNLP_LV_MAX];
	struct pq_hdr_tmo_sw_s hdr_tmo_table[HDR_TMO_TABLE_NUM_MAX][HDR_TMO_LV_MAX];
	struct pq_ai_pq_para_s aipq_table[AI_PQ_TABLE_NUM_MAX];
	struct pq_lc_curve_parm_s lc_node_table[LC_TABLE_NUM_MAX][LC_LV_MAX];
	struct pq_lc_reg_parm_s lc_reg_table[LC_TABLE_NUM_MAX][LC_LV_MAX];
	struct pq_ccoring_parm_s ccoring_table[CCORING_TABLE_NUM_MAX][CCORING_LV_MAX];
	struct pq_blkext_parm_s blkext_table[BLKEXT_TABLE_NUM_MAX][BLKEXT_LV_MAX];
	struct pq_blue_stretch_parm_s blue_str_table[BLUE_STR_TABLE_NUM_MAX][BLUESTR_LV_MAX];
	struct pq_cm_base_parm_s cm_base_table[COLOR_BASE_TABLE_NUM_MAX][CM_LV_MAX];
	struct pq_ai_sr_parm_s ai_sr_table[AI_SR_TABLE_NUM_MAX][AISR_TYPE_MAX][AISR_LV_MAX];
};

#endif //_VPQ_PQTABLE_TYPE_H_
