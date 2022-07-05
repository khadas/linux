/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_COMMON_H__
#define __VPP_COMMON_H__

/*Standard Linux headers*/
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#include "vpp_reg_io.h"
#include "vpp_drv.h"

/* Commom define */
#define PR_SYS    (0)
#define PR_IOC    (1)
#define PR_DEBUG  (2)

extern unsigned int pr_lvl;

#define pr_vpp(level, fmt, args ...)\
	do {\
		if ((level) & pr_lvl)\
			pr_info("vpp:" fmt, ##args);\
	} while (0)

#define PR_ERR(fmt, args ...)  pr_info("vpp_err:" fmt, ##args)
#define PR_DRV(fmt, args ...)  pr_info("vpp_drv:" fmt, ##args)

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

#ifndef MIN
#define MIN(a, b) \
	({typeof(a) x = (a);\
	typeof(b) y = (b);\
	(x < y) ? (x) : (y);\
	})
#endif

#ifndef MAX
#define MAX(a, b) \
	({typeof(a) x = (a);\
	typeof(b) y = (b);\
	(x > y) ? (x) : (y);\
	})
#endif

#ifndef XOR
#define XOR(a, b) \
	({typeof(a) x = (a);\
	typeof(b) y = (b);\
	(y & (~x)) + ((~y) & x);\
	})
#endif

/*Common enum*/
enum vpp_rgb_mode_e {
	MODE_R = 0,
	MODE_G,
	MODE_B,
	MODE_RGB_MAX,
};

enum vpp_level_mode_e {
	LEVEL_MODE_OFF = 0,
	LEVEL_MODE_LOW,
	LEVEL_MODE_MID,
	LEVEL_MODE_HIGH,
	LEVEL_MODE_AUTO,
};

enum vpp_module_e {
	MODULE_VADJ1 = 0,
	MODULE_VADJ2,
	MODULE_PREGAMMA,
	MODULE_GAMMA,
	MODULE_WB,
	MODULE_DNLP,
	MODULE_CCORING,
	MODULE_SR0,
	MODULE_SR0_DNLP,
	MODULE_SR1,
	MODULE_SR1_DNLP,
	MODULE_LC,
	MODULE_CM,
	MODULE_BLE,
	MODULE_BLS,
	MODULE_LUT3D,
	MODULE_ALL,
};

enum vpp_hdr_lut_type_e {
	HDR_LUT_TYPE_HLG = 1,
	HDR_LUT_TYPE_HDR,
	HDR_LUT_TYPE_MAX,
};

enum vpp_hdr_type_e {
	HDR_TYPE_NONE = 0,
	HDR_TYPE_HDR10 = 1,
	HDR_TYPE_HDR10PLUS = 2,
	HDR_TYPE_DOBVI = 3,
	HDR_TYPE_PRIMESL = 4,
	HDR_TYPE_HLG = 5,
	HDR_TYPE_SDR = 6,
	HDR_TYPE_MVC = 7,
};

enum vpp_dnlp_rt_e {
	DNLP_RT_0S = 0,
	DNLP_RT_1S = 6,
	DNLP_RT_2S,
	DNLP_RT_4S,
	DNLP_RT_8S,
	DNLP_RT_16S,
	DNLP_RT_32S,
	DNLP_RT_64S,
	DNLP_RT_FREEZE,
};

enum vpp_dnlp_rl_e {
	DNLP_RL_01 = 1, /*max_contrast = 1.0625x*/
	DNLP_RL_02,     /*max_contrast = 1.1250x*/
	DNLP_RL_03,     /*max_contrast = 1.1875x*/
	DNLP_RL_04,     /*max_contrast = 1.2500x*/
	DNLP_RL_05,     /*max_contrast = 1.3125x*/
	DNLP_RL_06,     /*max_contrast = 1.3750x*/
	DNLP_RL_07,     /*max_contrast = 1.4375x*/
	DNLP_RL_08,     /*max_contrast = 1.5000x*/
	DNLP_RL_09,     /*max_contrast = 1.5625x*/
	DNLP_RL_10,     /*max_contrast = 1.6250x*/
	DNLP_RL_11,     /*max_contrast = 1.6875x*/
	DNLP_RL_12,     /*max_contrast = 1.7500x*/
	DNLP_RL_13,     /*max_contrast = 1.8125x*/
	DNLP_RL_14,     /*max_contrast = 1.8750x*/
	DNLP_RL_15,     /*max_contrast = 1.9375x*/
	DNLP_RL_16,     /*max_contrast = 2.0000x*/
};

enum vpp_dnlp_ext_e {
	DNLP_EXT_00 = 0, /*weak*/
	DNLP_EXT_01,
	DNLP_EXT_02,
	DNLP_EXT_03,
	DNLP_EXT_04,
	DNLP_EXT_05,
	DNLP_EXT_06,
	DNLP_EXT_07,
	DNLP_EXT_08,
	DNLP_EXT_09,
	DNLP_EXT_10,
	DNLP_EXT_11,
	DNLP_EXT_12,
	DNLP_EXT_13,
	DNLP_EXT_14,
	DNLP_EXT_15,
	DNLP_EXT_16,     /*strong*/
};

enum vpp_csc_type_e {
	CSC_MATRIX_NULL                  = 0,
	CSC_MATRIX_RGB_YUV601            = 0x1,
	CSC_MATRIX_RGB_YUV601F           = 0x2,
	CSC_MATRIX_RGB_YUV709            = 0x3,
	CSC_MATRIX_RGB_YUV709F           = 0x4,
	CSC_MATRIX_YUV601_RGB            = 0x10,
	CSC_MATRIX_YUV601_YUV601F        = 0x11,
	CSC_MATRIX_YUV601_YUV709         = 0x12,
	CSC_MATRIX_YUV601_YUV709F        = 0x13,
	CSC_MATRIX_YUV601F_RGB           = 0x14,
	CSC_MATRIX_YUV601F_YUV601        = 0x15,
	CSC_MATRIX_YUV601F_YUV709        = 0x16,
	CSC_MATRIX_YUV601F_YUV709F       = 0x17,
	CSC_MATRIX_YUV709_RGB            = 0x20,
	CSC_MATRIX_YUV709_YUV601         = 0x21,
	CSC_MATRIX_YUV709_YUV601F        = 0x22,
	CSC_MATRIX_YUV709_YUV709F        = 0x23,
	CSC_MATRIX_YUV709F_RGB           = 0x24,
	CSC_MATRIX_YUV709F_YUV601        = 0x25,
	CSC_MATRIX_YUV709F_YUV709        = 0x26,
	CSC_MATRIX_YUV601L_YUV709L       = 0x27,
	CSC_MATRIX_YUV709L_YUV601L       = 0x28,
	CSC_MATRIX_YUV709F_YUV601F       = 0x29,
	CSC_MATRIX_BT2020YUV_BT2020RGB   = 0x40,
	CSC_MATRIX_BT2020RGB_709RGB,
	CSC_MATRIX_BT2020RGB_CUSRGB,
	CSC_MATRIX_DEFAULT_CSCTYPE       = 0xffff,
};

enum vpp_mtrx_type_e {
	MTRX_VD1 = 0,
	MTRX_POST,
	MTRX_POST2,
	MTRX_MAX,
};

enum vpp_dnlp_param_e {
	DNLP_SMHIST_CK = 0,
	DNLP_MVREFLSH,
	DNLP_CUVBLD_MIN,
	DNLP_CUVBLD_MAX,
	DNLP_BBD_RATIO_LOW,
	DNLP_BBD_RATIO_HIG,
	DNLP_BLK_CCTR,
	DNLP_BRGT_CTRL,
	DNLP_BRGT_RANGE,
	DNLP_BRGHT_ADD,
	DNLP_BRGHT_MAX,
	DNLP_AUTO_RNG,
	DNLP_LOWRANGE,
	DNLP_HGHRANGE,
	DNLP_SATUR_RAT,
	DNLP_SATUR_MAX,
	DNLP_SBGNBND,
	DNLP_SENDBND,
	DNLP_CLASHBGN,
	DNLP_CLASHEND,
	DNLP_CLAHE_GAIN_NEG,
	DNLP_CLAHE_GAIN_POS,
	DNLP_MTDBLD_RATE,
	DNLP_ADPMTD_LBND,
	DNLP_ADPMTD_HBND,
	DNLP_BLKEXT_OFST,
	DNLP_WHTEXT_OFST,
	DNLP_BLKEXT_RATE,
	DNLP_WHTEXT_RATE,
	DNLP_BWEXT_DIV4X_MIN,
	DNLP_IRGNBGN,
	DNLP_IRGNEND,
	DNLP_FINAL_GAIN,
	DNLP_CLIPRATE_V3,
	DNLP_CLIPRATE_MIN,
	DNLP_ADPCRAT_LBND,
	DNLP_ADPCRAT_HBND,
	DNLP_SCURV_LOW_TH,
	DNLP_SCURV_MID1_TH,
	DNLP_SCURV_MID2_TH,
	DNLP_SCURV_HGH1_TH,
	DNLP_SCURV_HGH2_TH,
	DNLP_MTDRATE_ADP_EN,
	DNLP_CLAHE_METHOD,
	DNLP_BLE_EN,
	DNLP_NORM,
	DNLP_SCN_CHG_TH,
	DNLP_IIR_STEP_MUX,
	DNLP_SINGLE_BIN_BW,
	DNLP_SINGLE_BIN_METHOD,
	DNLP_REG_MAX_SLOP_1ST,
	DNLP_REG_MAX_SLOP_MID,
	DNLP_REG_MAX_SLOP_FIN,
	DNLP_REG_MIN_SLOP_1ST,
	DNLP_REG_MIN_SLOP_MID,
	DNLP_REG_MIN_SLOP_FIN,
	DNLP_REG_TREND_WHT_EXPAND_MODE,
	DNLP_REG_TREND_BLK_EXPAND_MODE,
	DNLP_HIST_CUR_GAIN,
	DNLP_HIST_CUR_GAIN_PRECISE,
	DNLP_REG_MONO_BINRANG_ST,
	DNLP_REG_MONO_BINRANG_ED,
	DNLP_C_HIST_GAIN_BASE,
	DNLP_S_HIST_GAIN_BASE,
	DNLP_MVREFLSH_OFFSET,
	DNLP_LUMA_AVG_TH,
	DNLP_PARAM_MAX,
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
	unsigned int param[DNLP_PARAM_MAX];
};

/*Common functions*/
int vpp_check_range(int val, int down, int up);
int vpp_mask_int(int val, int start, int len);
int vpp_insert_int(int src_val, int insert_val, int start, int len);

/*IOCtrl*/
#define VPP_IOC_GET_PQ_STATE _IOR(_VPP_TYPE, 0x1, struct vpp_pq_state_s)

#endif

