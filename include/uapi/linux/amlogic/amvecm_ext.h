/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AMVECM_H_
#define AMVECM_H_
#include <linux/types.h>

#define DNLP_SCURV_LEN 65
#define GAIN_VAR_LUT_LEN 49
#define WEXT_GAIN_LEN 48
#define ADP_THRD_LEN 33
#define REG_BLK_BOOST_LEN 13
#define REG_ADP_OFSET_LEN 20
#define REG_MONO_PROT_LEN 6
#define TREND_WHT_EXP_LUT_LEN 9
#define C_HIST_GAIN_LEN 65
#define S_HIST_GAIN_LEN 65
#define DNLP_PARM_MAX_NUM 100
#define DNLP_VPP_HIST_BIN_NUM 64
#define HDR_HIST_BIN_NUM 128
#define HUE_HIST_BIN_NUM 32
#define SAT_HIST_BIN_NUM 32

#define FREESYNC_DYNAMIC_GAMMA_NUM 10
#define FREESYNC_DYNAMIC_GAMMA_CHANNEL 3

#define UNKNOWN_SOURCE		0
#define HDR10_SOURCE		1
#define HDR10PLUS_SOURCE	2
#define DOVI_SOURCE			3
#define PRIMESL_SOURCE		4
#define HLG_SOURCE			5
#define SDR_SOURCE			6
#define MVC_SOURCE           7
#define CUVA_HDR_SOURCE      8
#define CUVA_HLG_SOURCE      9
#define MAX_SOURCE      10

#define _VE_CM  'C'
#define _DI_	'D'

/* Register table structure */
struct am_reg_s {
	unsigned int type; /* 32-bits; 0: CBUS; 1: APB BUS... */
	unsigned int addr; /* 32-bits; Register address */
	unsigned int mask; /* 32-bits; Valid bits */
	unsigned int  val; /* 32-bits; Register Value */
};

#define am_reg_size 900
struct am_regs_s {
	unsigned int    length; /* Length of total am_reg */
	struct am_reg_s am_reg[am_reg_size];
};

struct ve_hist_s {
	unsigned int sum;
	int width;
	int height;
	int ave;
};

struct ve_dnlp_curve_param_s {
	unsigned int ve_dnlp_scurv_low[DNLP_SCURV_LEN];
	unsigned int ve_dnlp_scurv_mid1[DNLP_SCURV_LEN];
	unsigned int ve_dnlp_scurv_mid2[DNLP_SCURV_LEN];
	unsigned int ve_dnlp_scurv_hgh1[DNLP_SCURV_LEN];
	unsigned int ve_dnlp_scurv_hgh2[DNLP_SCURV_LEN];
	unsigned int ve_gain_var_lut49[GAIN_VAR_LUT_LEN];
	unsigned int ve_wext_gain[WEXT_GAIN_LEN];
	unsigned int ve_adp_thrd[ADP_THRD_LEN];
	unsigned int ve_reg_blk_boost_12[REG_BLK_BOOST_LEN];
	unsigned int ve_reg_adp_ofset_20[REG_ADP_OFSET_LEN];
	unsigned int ve_reg_mono_protect[REG_MONO_PROT_LEN];
	unsigned int ve_reg_trend_wht_expand_lut8[TREND_WHT_EXP_LUT_LEN];
	unsigned int ve_c_hist_gain[C_HIST_GAIN_LEN];
	unsigned int ve_s_hist_gain[S_HIST_GAIN_LEN];
	unsigned int param[DNLP_PARM_MAX_NUM];
};

struct vpp_hist_param_s {
	unsigned int vpp_hist_pow;
	unsigned int vpp_luma_sum;
	unsigned int vpp_pixel_sum;
	unsigned short vpp_histgram[DNLP_VPP_HIST_BIN_NUM];
	unsigned int hdr_histgram[HDR_HIST_BIN_NUM];
	unsigned int hue_histgram[HUE_HIST_BIN_NUM];
	unsigned int sat_histgram[SAT_HIST_BIN_NUM];
};

struct hdr_metadata_info_s {
	__u32 primaries[3][2];		/* normalized 50000 in G,B,R order */
	__u32 white_point[2];		/* normalized 50000 */
	__u32 luminance[2];		/* max/min lumin, normalized 10000 */
};

enum color_primary_e {
	VPP_COLOR_PRI_NULL = 0,
	VPP_COLOR_PRI_BT601,
	VPP_COLOR_PRI_BT709,
	VPP_COLOR_PRI_BT2020,
	VPP_COLOR_PRI_MAX,
};

struct tcon_gamma_table_s {
	__u16 data[257];
} /*tcon_gamma_table_t */;

struct tcon_rgb_ogo_s {
	unsigned int en;

	int r_pre_offset;	/* s11.0, range -1024~+1023, default is 0 */
	int g_pre_offset;	/* s11.0, range -1024~+1023, default is 0 */
	int b_pre_offset;	/* s11.0, range -1024~+1023, default is 0 */
	unsigned int r_gain;   /* u1.10, range 0~2047, default is 1024 (1.0x) */
	unsigned int g_gain;   /* u1.10, range 0~2047, default is 1024 (1.0x) */
	unsigned int b_gain;   /* u1.10, range 0~2047, default is 1024 (1.0x) */
	int r_post_offset;	/* s11.0, range -1024~+1023, default is 0 */
	int g_post_offset;	/* s11.0, range -1024~+1023, default is 0 */
	int b_post_offset;	/* s11.0, range -1024~+1023, default is 0 */
} /*tcon_rgb_ogo_t */;

struct gm_tbl_s {
	struct tcon_gamma_table_s gm_tb[FREESYNC_DYNAMIC_GAMMA_NUM][FREESYNC_DYNAMIC_GAMMA_CHANNEL];
};

enum pq_table_name_e {
	TABLE_NAME_SHARPNESS0 = 0x1,/*in vpp*/
	TABLE_NAME_SHARPNESS1 = 0x2,/*in vpp*/
	TABLE_NAME_DNLP = 0x4,		/*in vpp*/
	TABLE_NAME_CM = 0x8,		/*in vpp*/
	TABLE_NAME_BLK_BLUE_EXT = 0x10,/*in vpp*/
	TABLE_NAME_BRIGHTNESS = 0x20,/*in vpp*/
	TABLE_NAME_CONTRAST = 0x40,	/*in vpp*/
	TABLE_NAME_SATURATION_HUE = 0x80,/*in vpp*/
	TABLE_NAME_CVD2 = 0x100,		/*in tvafe*/
	TABLE_NAME_DI = 0x200,		/*in di*/
	TABLE_NAME_NR = 0x400,		/*in di*/
	TABLE_NAME_MCDI = 0x800,	/*in di*/
	TABLE_NAME_DEBLOCK = 0x1000,	/*in di*/
	TABLE_NAME_DEMOSQUITO = 0x2000,/*in di*/
	TABLE_NAME_WB = 0X4000,		/*in vpp*/
	TABLE_NAME_GAMMA = 0X8000,	/*in vpp*/
	TABLE_NAME_XVYCC = 0x10000,	/*in vpp*/
	TABLE_NAME_HDR = 0x20000,	/*in vpp*/
	TABLE_NAME_DOLBY_VISION = 0x40000,/*in vpp*/
	TABLE_NAME_OVERSCAN = 0x80000,
	TABLE_NAME_SMOOTHPLUS = 0x100000, /*in di*/
	TABLE_NAME_RESERVED2 = 0x200000,
	TABLE_NAME_RESERVED3 = 0x400000,
	TABLE_NAME_RESERVED4 = 0x800000,
	TABLE_NAME_MAX,
};

struct ve_pq_load_s {
	enum pq_table_name_e param_id;
	unsigned int length;
	union {
	void *param_ptr;
	long long param_ptr_len;
	};
	union {
	void *reserved;
	long long reserved_len;
	};
};

struct am_pq_parm_s {
	unsigned int table_name;
	unsigned int table_len;
	union {
	void *table_ptr;
	long long l_table;
	};
	union {
	void *reserved;
	long long l_reserved;
	};
};

enum dnlp_state_e {
	DNLP_OFF = 0,
	DNLP_ON,
};

enum pc_mode_e {
	PCMODE_OFF = 0,
	PCMODE_ON,
};

enum vpp_matrix_csc_e {
	VPP_MATRIX_NULL = 0,
	VPP_MATRIX_RGB_YUV601 = 0x1,
	VPP_MATRIX_RGB_YUV601F = 0x2,
	VPP_MATRIX_RGB_YUV709 = 0x3,
	VPP_MATRIX_RGB_YUV709F = 0x4,
	VPP_MATRIX_YUV601_RGB = 0x10,
	VPP_MATRIX_YUV601_YUV601F = 0x11,
	VPP_MATRIX_YUV601_YUV709 = 0x12,
	VPP_MATRIX_YUV601_YUV709F = 0x13,
	VPP_MATRIX_YUV601F_RGB = 0x14,
	VPP_MATRIX_YUV601F_YUV601 = 0x15,
	VPP_MATRIX_YUV601F_YUV709 = 0x16,
	VPP_MATRIX_YUV601F_YUV709F = 0x17,
	VPP_MATRIX_YUV709_RGB = 0x20,
	VPP_MATRIX_YUV709_YUV601 = 0x21,
	VPP_MATRIX_YUV709_YUV601F = 0x22,
	VPP_MATRIX_YUV709_YUV709F = 0x23,
	VPP_MATRIX_YUV709F_RGB = 0x24,
	VPP_MATRIX_YUV709F_YUV601 = 0x25,
	VPP_MATRIX_YUV709F_YUV709 = 0x26,
	VPP_MATRIX_YUV601L_YUV709L = 0x27,
	VPP_MATRIX_YUV709L_YUV601L = 0x28,
	VPP_MATRIX_YUV709F_YUV601F = 0x29,
	VPP_MATRIX_BT2020YUV_BT2020RGB = 0x40,
	VPP_MATRIX_BT2020RGB_709RGB,
	VPP_MATRIX_BT2020RGB_CUSRGB,
	VPP_MATRIX_BT2020YUV_BT2020RGB_DYNAMIC = 0x50,
	VPP_MATRIX_BT2020YUV_BT2020RGB_CUVA = 0x51,
	VPP_MATRIX_DEFAULT_CSCTYPE = 0xffff,
};

struct am_vdj_mode_s {
	int flag;
	int brightness;
	int brightness2;
	int saturation_hue;
	int saturation_hue_post;
	int contrast;
	int contrast2;
	int vadj1_en;  /*vadj1 enable: 1 enable  0 disable*/
	int vadj2_en;
};

enum hdr_type_e {
	HDRTYPE_NONE = UNKNOWN_SOURCE,
	HDRTYPE_SDR = SDR_SOURCE,
	HDRTYPE_HDR10 = HDR10_SOURCE,
	HDRTYPE_HLG = HLG_SOURCE,
	HDRTYPE_HDR10PLUS = HDR10PLUS_SOURCE,
	HDRTYPE_DOVI = DOVI_SOURCE,
	HDRTYPE_MVC = MVC_SOURCE,
	HDRTYPE_CUVA_HDR = CUVA_HDR_SOURCE,
	HDRTYPE_CUVA_HLG = CUVA_HLG_SOURCE,
	HDRTYPE_PRIMESL = PRIMESL_SOURCE,
};

struct ve_lc_curve_parm_s {
	unsigned int ve_lc_saturation[63];
	unsigned int ve_lc_yminval_lmt[16];
	unsigned int ve_lc_ypkbv_ymaxval_lmt[16];
	unsigned int ve_lc_ymaxval_lmt[16];
	unsigned int ve_lc_ypkbv_lmt[16];
	unsigned int ve_lc_ypkbv_ratio[4];
	unsigned int param[100];
};

enum lut_type_e {
	HLG_LUT = 1,
	HDR_LUT = 2,
	LUT_MAX
};

/*tone mapping struct*/
struct hdr_tone_mapping_s       {
	enum lut_type_e lut_type;
	unsigned int lutlength;
	union {
	void *tm_lut;
	long long tm_lut_len;
	};
};

enum ecm2colormode {
	ecm2colormode_purple = 0,
	ecm2colormode_red,
	ecm2colormode_skin,
	ecm2colormode_yellow,
	ecm2colormode_yellow_green,
	ecm2colormode_green,
	ecm2colormode_blue_green,
	ecm2colormode_cyan,
	ecm2colormode_blue,
	ecm2colormode_max,
};

enum ecm_color_type {
	cm_9_color = 0,
	cm_14_color,
	cm_color_max,
};

enum ecm_14_color_md {
	cm_14_ecm2colormode_blue_purple = 0,
	cm_14_ecm2colormode_purple,
	cm_14_ecm2colormode_purple_red,
	cm_14_ecm2colormode_red,
	cm_14_ecm2colormode_skin_cheeks,
	cm_14_ecm2colormode_skin_hair_cheeks,
	cm_14_ecm2colormode_skin_yellow,
	cm_14_ecm2colormode_yellow,
	cm_14_ecm2colormode_yellow_green,
	cm_14_ecm2colormode_green,
	cm_14_ecm2colormode_green_cyan,
	cm_14_ecm2colormode_cyan,
	cm_14_ecm2colormode_cyan_blue,
	cm_14_ecm2colormode_blue,
	cm_14_ecm2colormode_max,
};

struct cm_color_md {
	enum ecm_color_type	color_type;//0: 9 color; 1: 14 color
	enum ecm2colormode	cm_9_color_md;
	enum ecm_14_color_md cm_14_color_md;
	int color_value;
};

struct vpp_pq_ctrl_s {
	unsigned int length;
	union {
		void *ptr;/*point to pq_ctrl_s*/
		long long ptr_length;
	};
};

enum meson_cpu_ver_e {
	VER_NULL = 0,
	VER_A,
	VER_B,
	VER_C,
	VER_MAX
};

struct aipq_load_s {
	unsigned int height;
	unsigned int width;
	union {
		void *table_ptr;
		long long table_len;
	};
};

struct pre_gamma_table_s {
	unsigned int en;
	unsigned int lut_r[65];
	unsigned int lut_g[65];
	unsigned int lut_b[65];
};

/*G12A vpp matrix*/
enum vpp_matrix_e {
	MTX_NULL = 0,
	VD1_MTX = 0x1,
	POST2_MTX = 0x2,
	POST_MTX = 0x4,
	VPP1_POST2_MTX = 0x8,
	VPP2_POST2_MTX = 0x10
};

struct matrix_coef_s {
	__u16 pre_offset[3];
	__u16 matrix_coef[3][3];
	__u16 post_offset[3];
	__u16 right_shift;
	__u16 en;
};

struct vpp_mtx_info_s {
	enum vpp_matrix_e mtx_sel;
	struct matrix_coef_s mtx_coef;
};

struct hdr_tmo_sw {
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
	int reg_ratio;           //u10
	int reg_max_th3;         //s11
	int oo_init_lut[13];        //u10
};

struct db_cabc_aad_param_s {
	unsigned int length;
	union {
		void *cabc_aad_param_ptr;
		long long cabc_aad_param_ptr_len;
	};
};

struct db_cabc_param_s {
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
	struct db_cabc_aad_param_s db_o_bl_cv;
	struct db_cabc_aad_param_s db_maxbin_bl_cv;
};

struct db_aad_param_s {
	int aad_param_cabc_aad_en;
	int aad_param_aad_en;
	int aad_param_tf_en;
	int aad_param_force_gain_en;
	int aad_param_sensor_mode;
	int aad_param_mode;
	int aad_param_dist_mode;
	int aad_param_tf_alpha;
	int aad_param_sensor_input[3];
	struct db_cabc_aad_param_s	 db_LUT_Y_gain;
	struct db_cabc_aad_param_s	 db_LUT_RG_gain;
	struct db_cabc_aad_param_s	 db_LUT_BG_gain;
	struct db_cabc_aad_param_s	 db_gain_lut;
	struct db_cabc_aad_param_s	 db_xy_lut;
};

struct eye_protect_s {
	int en;
	int mtx_ep[4][4];
};

/*Freerun type ioctl enum*/
enum freerun_type_e {
	GAME_MODE = 0,
	FREERUN_MODE,
	FREERUN_TYPE_MAX
};

struct blue_str_parm_s {
	int blue_stretch_en;
	int blue_stretch_cr_inc;
	int blue_stretch_cb_inc;
	int blue_stretch_gain;
	int blue_stretch_gain_cb4cr;
	int blue_stretch_error_crp;
	int blue_stretch_error_crp_inv;
	int blue_stretch_error_crn;
	int blue_stretch_error_crn_inv;
	int blue_stretch_error_cbp;
	int blue_stretch_error_cbp_inv;
	int blue_stretch_error_cbn;
	int blue_stretch_error_cbn_inv;
	int blue_stretch_luma_high;
};

struct color_tune_parm_s {
	int en;
	int rgain_r;
	int rgain_g;
	int rgain_b;

	int ggain_r;
	int ggain_g;
	int ggain_b;

	int bgain_r;
	int bgain_g;
	int bgain_b;

	int cgain_r;
	int cgain_g;
	int cgain_b;

	int mgain_r;
	int mgain_g;
	int mgain_b;

	int ygain_r;
	int ygain_g;
	int ygain_b;
};

struct primary_s {
	__u32 src[8];
	__u32 dest[8];
};

enum gamut_conv_enable_e {
	gamut_conv_off,
	gamut_conv_on,
};

struct video_color_matrix {
	__u32 data[3][3];
};

struct ve_ble_whe_param_s {
	int blk_adj_en;
	int blk_end;
	int blk_slp;
	int brt_adj_en;
	int brt_start;
	int brt_slp;
};

#define AMVECM_IOC_G_HIST_AVG			_IOW(_VE_CM, 0x22, struct ve_hist_s)
#define AMVECM_IOC_VE_DNLP_EN			_IO(_VE_CM, 0x23)
#define AMVECM_IOC_VE_DNLP_DIS			_IO(_VE_CM, 0x24)
#define AMVECM_IOC_VE_NEW_DNLP			_IOW(_VE_CM, 0x25, struct ve_dnlp_curve_param_s)
#define AMVECM_IOC_G_HIST_BIN			_IOW(_VE_CM, 0x26, struct vpp_hist_param_s)
#define AMVECM_IOC_G_HDR_METADATA		_IOW(_VE_CM, 0x27, struct hdr_metadata_info_s)
/*vpp get color primary*/
#define AMVECM_IOC_G_COLOR_PRI			_IOR(_VE_CM, 0x28, enum color_primary_e)
/* VPP.CM IOCTL command list */
#define AMVECM_IOC_LOAD_REG			_IOW(_VE_CM, 0x30, struct am_regs_s)
/* VPP.GAMMA IOCTL command list */
#define AMVECM_IOC_GAMMA_TABLE_EN		_IO(_VE_CM, 0x40)
#define AMVECM_IOC_GAMMA_TABLE_DIS		_IO(_VE_CM, 0x41)
#define AMVECM_IOC_GAMMA_TABLE_R		_IOW(_VE_CM, 0x42, struct tcon_gamma_table_s)
#define AMVECM_IOC_GAMMA_TABLE_G		_IOW(_VE_CM, 0x43, struct tcon_gamma_table_s)
#define AMVECM_IOC_GAMMA_TABLE_B		_IOW(_VE_CM, 0x44, struct tcon_gamma_table_s)
#define AMVECM_IOC_S_RGB_OGO			_IOW(_VE_CM, 0x45, struct tcon_rgb_ogo_s)
#define AMVECM_IOC_G_RGB_OGO			_IOR(_VE_CM, 0x46, struct tcon_rgb_ogo_s)
/*VPP.VLOCK IOCTL command list*/
#define AMVECM_IOC_VLOCK_EN			_IO(_VE_CM, 0x47)
#define AMVECM_IOC_VLOCK_DIS			_IO(_VE_CM, 0x48)
/*VPP.3D-SYNC IOCTL command list*/
#define AMVECM_IOC_3D_SYNC_EN			_IO(_VE_CM, 0x49)
#define AMVECM_IOC_GAMMA_SET			_IOW(_VE_CM, 0X4a, struct gm_tbl_s)
#define AMVECM_IOC_3D_SYNC_DIS			_IO(_VE_CM, 0x50)
#define AMDI_IOC_SET_PQ_PARM			_IOW(_DI_, 0x51, struct am_pq_parm_s)
#define AMVECM_IOC_SET_OVERSCAN			_IOW(_VE_CM, 0x52, struct ve_pq_load_s)
/*DNLP IOCTL command list*/
#define AMVECM_IOC_G_DNLP_STATE			_IOR(_VE_CM, 0x53, enum dnlp_state_e)
#define AMVECM_IOC_S_DNLP_STATE			_IOW(_VE_CM, 0x54, enum dnlp_state_e)
/*PCMODE IOCTL command list*/
#define AMVECM_IOC_G_PQMODE			_IOR(_VE_CM, 0x55, enum pc_mode_e)
#define AMVECM_IOC_S_PQMODE			_IOW(_VE_CM, 0x56, enum pc_mode_e)
/*CUR_CSCTYPE IOCTL command list*/
#define AMVECM_IOC_G_CSCTYPE			_IOR(_VE_CM, 0x57, enum vpp_matrix_csc_e)
#define AMVECM_IOC_S_CSCTYPE			_IOW(_VE_CM, 0x58, enum vpp_matrix_csc_e)
/*PIC_MODE IOCTL command list*/
#define AMVECM_IOC_G_PIC_MODE			_IOR(_VE_CM, 0x59, struct am_vdj_mode_s)
#define AMVECM_IOC_S_PIC_MODE			_IOW(_VE_CM, 0x60, struct am_vdj_mode_s)
/*HDR TYPE command list*/
#define AMVECM_IOC_G_HDR_TYPE			_IOR(_VE_CM, 0x61, enum hdr_type_e)
/*Local contrast command list*/
#define AMVECM_IOC_S_LC_CURVE			_IOW(_VE_CM, 0x62, struct ve_lc_curve_parm_s)
#define AMVECM_IOC_S_HDR_TM			_IOW(_VE_CM, 0x63, struct hdr_tone_mapping_s)
#define AMVECM_IOC_G_HDR_TM			_IOR(_VE_CM, 0x64, struct hdr_tone_mapping_s)
#define AMVECM_IOC_S_CMS_LUMA			_IOW(_VE_CM, 0x65, struct cm_color_md)
#define AMVECM_IOC_S_CMS_SAT			_IOW(_VE_CM, 0x66, struct cm_color_md)
#define AMVECM_IOC_S_CMS_HUE			_IOW(_VE_CM, 0x67, struct cm_color_md)
#define AMVECM_IOC_S_CMS_HUE_HS			_IOW(_VE_CM, 0x68, struct cm_color_md)
#define AMVECM_IOC_S_PQ_CTRL			_IOW(_VE_CM, 0x69, struct vpp_pq_ctrl_s)
#define AMVECM_IOC_G_PQ_CTRL			_IOR(_VE_CM, 0x6a, struct vpp_pq_ctrl_s)
/*cpu ver ioc*/
#define AMVECM_IOC_S_MESON_CPU_VER		_IOW(_VE_CM, 0x6b, enum meson_cpu_ver_e)
#define AMVECM_IOC_S_AIPQ_TABLE			_IOW(_VE_CM, 0x6c, struct aipq_load_s)
#define AMVECM_IOC_SET_3D_LUT			_IO(_VE_CM, 0x6d)
#define AMVECM_IOC_LOAD_3D_LUT			_IO(_VE_CM, 0x6e)
#define AMVECM_IOC_SET_3D_LUT_ORDER		_IO(_VE_CM, 0x6f)
#define AMVECM_IOC_S_MTX_COEF			_IOW(_VE_CM, 0x70, struct vpp_mtx_info_s)
#define AMVECM_IOC_G_MTX_COEF			_IOR(_VE_CM, 0x71, struct vpp_mtx_info_s)
#define AMVECM_IOC_S_PRE_GAMMA			_IOW(_VE_CM, 0x72, struct pre_gamma_table_s)
#define AMVECM_IOC_G_PRE_GAMMA			_IOR(_VE_CM, 0x73, struct pre_gamma_table_s)
/*hdr10_tmo ioc*/
#define AMVECM_IOC_S_HDR_TMO			_IOW(_VE_CM, 0x74, struct hdr_tmo_sw)
#define AMVECM_IOC_G_HDR_TMO			_IOR(_VE_CM, 0x75, struct hdr_tmo_sw)
/*cabc command list*/
#define AMVECM_IOC_S_CABC_PARAM			_IOW(_VE_CM, 0x76, struct db_cabc_param_s)
/*aad command list*/
#define AMVECM_IOC_S_AAD_PARAM			_IOW(_VE_CM, 0x77, struct db_aad_param_s)
#define AMVECM_IOC_S_EYE_PROT			_IOW(_VE_CM, 0x78, struct eye_protect_s)
#define AMVECM_IOC_S_FREERUN_TYPE		_IOW(_VE_CM, 0x79, enum freerun_type_e)
#define AMVECM_IOC_S_BLUE_STR			_IOW(_VE_CM, 0x7a, struct blue_str_parm_s)
#define AMVECM_IOC_S_COLOR_TUNE			_IOW(_VE_CM, 0x7b, struct color_tune_parm_s)
#define AMVECM_IOC_3D_LUT_EN			_IO(_VE_CM, 0x7c)
#define AMVECM_IOC_COLOR_PRI_EN			_IO(_VE_CM, 0x7d)
#define AMVECM_IOC_COLOR_PRIMARY		_IOW(_VE_CM, 0x7e, struct primary_s)
#define AMVECM_IOC_S_GAMUT_CONV_EN		_IOW(_VE_CM, 0x7f, enum gamut_conv_enable_e)
#define AMVECM_IOC_COLOR_MTX_EN			_IO(_VE_CM, 0x80)
#define AMVECM_IOC_S_COLOR_MATRIX_DATA		_IOW(_VE_CM, 0x81, struct video_color_matrix)
#define AMVECM_IOC_G_COLOR_MATRIX_DATA		_IOR(_VE_CM, 0x82, struct video_color_matrix)
#define AMVECM_IOC_S_BLE_WHE			_IOW(_VE_CM, 0x83, struct ve_ble_whe_param_s)

/*t7 vpp1 command list*/
#define AMVECM_IOC_S_RGB_OGO_SUB _IOW(_VE_CM, 0x84, struct tcon_rgb_ogo_s)
#define AMVECM_IOC_G_RGB_OGO_SUB _IOR(_VE_CM, 0x85, struct tcon_rgb_ogo_s)
#define AMVECM_IOC_GAMMA_TABLE_EN_SUB _IO(_VE_CM, 0x86)
#define AMVECM_IOC_GAMMA_TABLE_DIS_SUB _IO(_VE_CM, 0x87)
#define AMVECM_IOC_GAMMA_TABLE_R_SUB _IOW(_VE_CM, 0x88, struct tcon_gamma_table_s)
#define AMVECM_IOC_GAMMA_TABLE_G_SUB _IOW(_VE_CM, 0x89, struct tcon_gamma_table_s)
#define AMVECM_IOC_GAMMA_TABLE_B_SUB _IOW(_VE_CM, 0x8a, struct tcon_gamma_table_s)

#endif

