/*
 * amvecm char device driver.
 *
 * Copyright (c) 2010 Frank Zhao<frank.zhao@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
/* #include <linux/amlogic/aml_common.h> */
#include <linux/ctype.h>/* for parse_para_pq */
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amvecm/amvecm.h>
#include <linux/amlogic/vout/vout_notify.h>
#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "arch/cm_regs.h"
#include "../tvin/tvin_global.h"
#include "arch/vpp_hdr_regs.h"

#include "amve.h"
#include "amcm.h"

#define pr_amvecm_dbg(fmt, args...)\
	do {\
		if (debug_amvecm)\
			pr_info("AMVECM: " fmt, ## args);\
	} while (0)
#define pr_amvecm_error(fmt, args...)\
	pr_error("AMVECM: " fmt, ## args)

#define AMVECM_NAME               "amvecm"
#define AMVECM_DRIVER_NAME        "amvecm"
#define AMVECM_MODULE_NAME        "amvecm"
#define AMVECM_DEVICE_NAME        "amvecm"
#define AMVECM_CLASS_NAME         "amvecm"
struct amvecm_dev_s {
	dev_t                       devt;
	struct cdev                 cdev;
	dev_t                       devno;
	struct device               *dev;
	struct class                *clsp;
};

struct matrix_s {
	u16 pre_offset[3];
	u16 matrix[3][3];
	u16 offset[3];
	u16 right_shift;
};

static struct vframe_s *dbg_vf;
static struct master_display_info_s dbg_hdr_send;
static struct hdr_info receiver_hdr_info;

static int hue_pre;  /*-25~25*/
static int saturation_pre;  /*-128~127*/
static int hue_post;  /*-25~25*/
static int saturation_post;  /*-128~127*/
static signed int vd1_brightness = 0, vd1_contrast;
static signed int vd1_contrast_offset;
static struct amvecm_dev_s amvecm_dev;
unsigned int sr1_reg_val[101];
unsigned int sr1_ret_val[101];
struct vpp_hist_param_s vpp_hist_param;
static unsigned int pre_hist_height, pre_hist_width;

void __iomem *amvecm_hiu_reg_base;/* = *ioremap(0xc883c000, 0x2000); */

static bool debug_amvecm;
module_param(debug_amvecm, bool, 0664);
MODULE_PARM_DESC(debug_amvecm, "\n debug_amvecm\n");

unsigned int vecm_latch_flag;
module_param(vecm_latch_flag, uint, 0664);
MODULE_PARM_DESC(vecm_latch_flag, "\n vecm_latch_flag\n");

unsigned int pq_load_en = 1;/* load pq table enable/disable */
module_param(pq_load_en, uint, 0664);
MODULE_PARM_DESC(pq_load_en, "\n pq_load_en\n");

bool gamma_en;  /* wb_gamma_en enable/disable */
module_param(gamma_en, bool, 0664);
MODULE_PARM_DESC(gamma_en, "\n gamma_en\n");

bool wb_en;  /* wb_en enable/disable */
module_param(wb_en, bool, 0664);
MODULE_PARM_DESC(wb_en, "\n wb_en\n");

static int pq_on_off = 2; /* 1 :on    0 :off */
module_param(pq_on_off, uint, 0664);
MODULE_PARM_DESC(pq_on_off, "\n pq_on_off\n");

static int cm_on_off = 2; /* 1 :on    0 :off */
module_param(cm_on_off, uint, 0664);
MODULE_PARM_DESC(cm_on_off, "\n cm_on_off\n");

static int dnlp_on_off = 2; /* 1 :on    0 :off */
module_param(dnlp_on_off, uint, 0664);
MODULE_PARM_DESC(dnlp_on_off, "\n dnlp_on_off\n");

static int sharpness_on_off = 2; /* 1 :on    0 :off */
module_param(sharpness_on_off, uint, 0664);
MODULE_PARM_DESC(sharpness_on_off, "\n sharpness_on_off\n");

static int wb_on_off = 2; /* 1 :on    0 :off */
module_param(wb_on_off, uint, 0664);
MODULE_PARM_DESC(wb_on_off, "\n wb_on_off\n");

unsigned int probe_ok;/* probe ok or not */
module_param(probe_ok, uint, 0664);
MODULE_PARM_DESC(probe_ok, "\n probe_ok\n");

static unsigned int sr1_index;/* for sr1 read */
module_param(sr1_index, uint, 0664);
MODULE_PARM_DESC(sr1_index, "\n sr1_index\n");

static enum vframe_source_type_e pre_src_type = VFRAME_SOURCE_TYPE_COMP;
static uint cur_csc_type = 0xffff;
module_param(cur_csc_type, uint, 0444);
MODULE_PARM_DESC(cur_csc_type, "\n current color space convert type\n");

static uint hdr_mode = 2; /* 0: hdr->hdr, 1:hdr->sdr, 2:auto */
module_param(hdr_mode, uint, 0664);
MODULE_PARM_DESC(hdr_mode, "\n set hdr_mode\n");

static uint hdr_process_mode = 1; /* 0: hdr->hdr, 1:hdr->sdr */
static uint cur_hdr_process_mode = 2; /* 0: hdr->hdr, 1:hdr->sdr */
module_param(hdr_process_mode, uint, 0444);
MODULE_PARM_DESC(hdr_process_mode, "\n current hdr_process_mode\n");

static uint force_csc_type = 0xff;
module_param(force_csc_type, uint, 0664);
MODULE_PARM_DESC(force_csc_type, "\n force colour space convert type\n");

static uint cur_hdr_support;
module_param(cur_hdr_support, uint, 0664);
MODULE_PARM_DESC(cur_hdr_support, "\n cur_hdr_support\n");

#define MAX_KNEE_SETTING	11
/* recommended setting for 100 nits panel: */
/* 0,16,96,224,320,544,720,864,1000,1016,1023 */
/* knee factor = 256 */
static int num_knee_setting = MAX_KNEE_SETTING;
static int knee_setting[MAX_KNEE_SETTING] = {
	/* 0, 16, 96, 224, 320, 544, 720, 864, 1000, 1016, 1023 */
	0, 16, 96, 204, 320, 512, 720, 864, 980, 1016, 1023
};

static int num_knee_linear_setting = MAX_KNEE_SETTING;
static int knee_linear_setting[MAX_KNEE_SETTING] = {
	0x000,
	0x010,
	0x08c,
	0x108,
	0x184,
	0x200,
	0x27c,
	0x2f8,
	0x374,
	0x3f0,
	0x3ff
};

static int knee_factor; /* 0 ~ 256, 128 = 0.5 */
static int knee_interpolation_mode = 1; /* 0: linear, 1: cubic */

module_param_array(knee_setting, int, &num_knee_setting, 0664);
MODULE_PARM_DESC(knee_setting, "\n knee_setting, 256=1.0\n");

module_param_array(knee_linear_setting, int, &num_knee_linear_setting, 0444);
MODULE_PARM_DESC(knee_linear_setting, "\n reference linear knee_setting\n");

module_param(knee_factor, int, 0664);
MODULE_PARM_DESC(knee_factor, "\n knee_factor, 255=1.0\n");

module_param(knee_interpolation_mode, int, 0664);
MODULE_PARM_DESC(knee_interpolation_mode, "\n 0: linear, 1: cubic\n");

#define NUM_MATRIX_PARAM 16
static uint num_customer_matrix_param = NUM_MATRIX_PARAM;
static uint customer_matrix_param[NUM_MATRIX_PARAM] = {
	0, 0, 0,
	0x0b77, 0x1c75, 0x0014,
	0x1f90, 0x07ed, 0x0083,
	0x0046, 0x1fb4, 0x0806,
	0, 0, 0,
	1
};

static bool customer_matrix_en;

#define INORM	50000
#define BL		16

static u32 bt709_primaries[3][2] = {
	{0.30 * INORM + 0.5, 0.60 * INORM + 0.5},	/* G */
	{0.15 * INORM + 0.5, 0.06 * INORM + 0.5},	/* B */
	{0.64 * INORM + 0.5, 0.33 * INORM + 0.5},	/* R */
};

static u32 bt709_white_point[2] = {
	0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

static u32 bt2020_primaries[3][2] = {
	{0.17 * INORM + 0.5, 0.797 * INORM + 0.5},	/* G */
	{0.131 * INORM + 0.5, 0.046 * INORM + 0.5},	/* B */
	{0.708 * INORM + 0.5, 0.292 * INORM + 0.5},	/* R */
};

static u32 bt2020_white_point[2] = {
	0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

module_param(customer_matrix_en, bool, 0664);
MODULE_PARM_DESC(customer_matrix_en, "\n if enable customer matrix\n");

module_param_array(customer_matrix_param, uint,
	&num_customer_matrix_param, 0664);
MODULE_PARM_DESC(customer_matrix_param,
	 "\n matrix from source primary to panel primary\n");

/* norm to 128 as 1, LUT can be changed */
static uint num_extra_con_lut = 5;
static uint extra_con_lut[] = {144, 136, 132, 130, 128};
module_param_array(extra_con_lut, uint,
	&num_extra_con_lut, 0664);
MODULE_PARM_DESC(extra_con_lut,
	 "\n lookup table for contrast match source luminance.\n");

#define clip(y, ymin, ymax) ((y > ymax) ? ymax : ((y < ymin) ? ymin : y))
static const int coef[] = {
	  0,   256,     0,     0, /* phase 0  */
	 -2,   256,     2,     0, /* phase 1  */
	 -4,   256,     4,     0, /* phase 2  */
	 -5,   254,     7,     0, /* phase 3  */
	 -7,   254,    10,    -1, /* phase 4  */
	 -8,   252,    13,    -1, /* phase 5  */
	-10,   251,    16,    -1, /* phase 6  */
	-11,   249,    19,    -1, /* phase 7  */
	-12,   247,    23,    -2, /* phase 8  */
	-13,   244,    27,    -2, /* phase 9  */
	-14,   242,    31,    -3, /* phase 10 */
	-15,   239,    35,    -3, /* phase 11 */
	-16,   236,    40,    -4, /* phase 12 */
	-17,   233,    44,    -4, /* phase 13 */
	-17,   229,    49,    -5, /* phase 14 */
	-18,   226,    53,    -5, /* phase 15 */
	-18,   222,    58,    -6, /* phase 16 */
	-18,   218,    63,    -7, /* phase 17 */
	-19,   214,    68,    -7, /* phase 18 */
	-19,   210,    73,    -8, /* phase 19 */
	-19,   205,    79,    -9, /* phase 20 */
	-19,   201,    83,    -9, /* phase 21 */
	-19,   196,    89,   -10, /* phase 22 */
	-19,   191,    94,   -10, /* phase 23 */
	-19,   186,   100,   -11, /* phase 24 */
	-18,   181,   105,   -12, /* phase 25 */
	-18,   176,   111,   -13, /* phase 26 */
	-18,   171,   116,   -13, /* phase 27 */
	-18,   166,   122,   -14, /* phase 28 */
	-17,   160,   127,   -14, /* phase 28 */
	-17,   155,   133,   -15, /* phase 30 */
	-16,   149,   138,   -15, /* phase 31 */
	-16,   144,   144,   -16  /* phase 32 */
};

int cubic_interpolation(int y0, int y1, int y2, int y3, int mu)
{
	int c0, c1, c2, c3;
	int d0, d1, d2, d3;

	if (mu <= 32) {
		c0 = coef[(mu << 2) + 0];
		c1 = coef[(mu << 2) + 1];
		c2 = coef[(mu << 2) + 2];
		c3 = coef[(mu << 2) + 3];
		d0 = y0; d1 = y1; d2 = y2; d3 = y3;
	} else {
		c0 = coef[((64 - mu) << 2) + 0];
		c1 = coef[((64 - mu) << 2) + 1];
		c2 = coef[((64 - mu) << 2) + 2];
		c3 = coef[((64 - mu) << 2) + 3];
		d0 = y3; d1 = y2; d2 = y1; d3 = y0;
	}
	return (d0 * c0 + d1 * c1 + d2 * c2 + d3 * c3 + 128) >> 8;
}

static int knee_lut_on;
static int cur_knee_factor = -1;
static void load_knee_lut(int on)
{
	int i, j, k;
	int value;
	int final_knee_setting[MAX_KNEE_SETTING];

	if (cur_knee_factor != knee_factor) {
		pr_amvecm_dbg("Knee_factor changed from %d to %d\n",
			cur_knee_factor, knee_factor);
		for (i = 0; i < MAX_KNEE_SETTING; i++) {
			final_knee_setting[i] =
				knee_linear_setting[i] + (((knee_setting[i]
				- knee_linear_setting[i]) * knee_factor) >> 8);
			if (final_knee_setting[i] > 0x3ff)
				final_knee_setting[i] = 0x3ff;
			else if (final_knee_setting[i] < 0)
				final_knee_setting[i] = 0;
		}
		WRITE_VPP_REG(XVYCC_LUT_CTL, 0x0);
		for (j = 0; j < 3; j++) {
			for (i = 0; i < 16; i++) {
				WRITE_VPP_REG(XVYCC_LUT_R_ADDR_PORT + 2 * j, i);
				value = final_knee_setting[0]
					+ (((final_knee_setting[1]
					- final_knee_setting[0]) * i) >> 4);
				value = clip(value, 0, 0x3ff);
				WRITE_VPP_REG(XVYCC_LUT_R_DATA_PORT + 2 * j,
						value);
				if (j == 0)
					pr_amvecm_dbg("xvycc_lut[%1d][%3d] = 0x%03x\n",
							j, i, value);
			}
			for (i = 16; i < 272; i++) {
				k = 1 + ((i - 16) >> 5);
				WRITE_VPP_REG(XVYCC_LUT_R_ADDR_PORT + 2 * j, i);
				if (knee_interpolation_mode == 0)
					value = final_knee_setting[k]
						+ (((final_knee_setting[k+1]
						- final_knee_setting[k])
						* ((i - 16) & 0x1f)) >> 5);
				else
					value = cubic_interpolation(
						final_knee_setting[k-1],
						final_knee_setting[k],
						final_knee_setting[k+1],
						final_knee_setting[k+2],
						((i - 16) & 0x1f) << 1);
				value = clip(value, 0, 0x3ff);
				WRITE_VPP_REG(XVYCC_LUT_R_DATA_PORT + 2 * j,
						value);
				if (j == 0)
					pr_amvecm_dbg("xvycc_lut[%1d][%3d] = 0x%03x\n",
							j, i, value);
			}
			for (i = 272; i < 289; i++) {
				k = MAX_KNEE_SETTING - 2;
				WRITE_VPP_REG(XVYCC_LUT_R_ADDR_PORT + 2 * j, i);
				value = final_knee_setting[k]
					+ (((final_knee_setting[k+1]
					- final_knee_setting[k])
					* (i - 272)) >> 4);
				value = clip(value, 0, 0x3ff);
				WRITE_VPP_REG(XVYCC_LUT_R_DATA_PORT + 2 * j,
						value);
				if (j == 0)
					pr_amvecm_dbg("xvycc_lut[%1d][%3d] = 0x%03x\n",
							j, i, value);
			}
		}
		cur_knee_factor = knee_factor;
	}
	if (on) {
		WRITE_VPP_REG(XVYCC_LUT_CTL, 0x7f);
		knee_lut_on = 1;
	} else {
		WRITE_VPP_REG(XVYCC_LUT_CTL, 0x0);
		knee_lut_on = 0;
	}
}


/* extern unsigned int cm_size; */
/* extern unsigned int ve_size; */
/* extern unsigned int cm2_patch_flag; */
/* extern struct ve_dnlp_s am_ve_dnlp; */
/* extern struct ve_dnlp_table_s am_ve_new_dnlp; */
/* extern int cm_en; //0:disabel;1:enable */
/* extern struct tcon_gamma_table_s video_gamma_table_r; */
/* extern struct tcon_gamma_table_s video_gamma_table_g; */
/* extern struct tcon_gamma_table_s video_gamma_table_b; */
/* extern struct tcon_gamma_table_s video_gamma_table_r_adj; */
/* extern struct tcon_gamma_table_s video_gamma_table_g_adj; */
/* extern struct tcon_gamma_table_s video_gamma_table_b_adj; */
/* extern struct tcon_rgb_ogo_s     video_rgb_ogo; */

/* csc mode:
	0: 601 limit to RGB
		vd1 for ycbcr to rgb
	1: 601 full to RGB
		vd1 for ycbcr to rgb
	2: 709 limit to RGB
		vd1 for ycbcr to rgb
	3: 709 full to RGB
		vd1 for ycbcr to rgb
	4: 2020 limit to RGB
		vd1 for ycbcr to rgb
		post for rgb to r'g'b'
	5: 2020(G:33c2,86c4 B:1d4c,0bb8 R:84d0,3e80) limit to RGB
		vd1 for ycbcr to rgb
		post for rgb to r'g'b'
	6: customer matrix calculation according to src and dest primary
		vd1 for ycbcr to rgb
		post for rgb to r'g'b' */
static void vpp_set_matrix(
		enum vpp_matrix_sel_e vd1_or_vd2_or_post,
		unsigned int on,
		enum vpp_matrix_csc_e csc_mode,
		struct matrix_s *m)
{
	if (force_csc_type != 0xff)
		csc_mode = force_csc_type;

	if (vd1_or_vd2_or_post == VPP_MATRIX_VD1) {
		/* vd1 matrix */
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, on, 5, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 8, 2);
	} else if (vd1_or_vd2_or_post == VPP_MATRIX_VD2) {
		/* vd2 matrix */
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, on, 4, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 2, 8, 2);
	} else {
		/* post matrix */
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, on, 0, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 0, 8, 2);
		/* saturation enable for 601 & 709 limited input */
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 0, 1, 2);
	}

	if (!on)
		return;

/*	TODO: need to adjust +/-64 for VPP_MATRIX_PRE_OFFSET0
	which was set in vpp_vd_adj1_brightness;
	should get current vd1_brightness(-1024~1023) here

	if (vd1_or_vd2_or_post == 0) {
		if ((csc_mode == 0) ||
			(csc_mode == 2) ||
			(csc_mode >= 4))
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1,
				(((vd1_brightness - 64) & 0xfff) << 16) |
				0xe00);
		else
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1,
				(((vd1_brightness - 64) & 0xfff) << 16) |
				0xe00);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0xe00);
	}
*/

	if (csc_mode == VPP_MATRIX_YUV601_RGB) {
		/* ycbcr limit, 601 to RGB */
		/*  -16  1.164   0       1.596   0
		    -128 1.164   -0.392  -0.813  0
		    -128 1.164   2.017   0       0 */
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x04A80000);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x066204A8);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1e701cbf);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x04A80812);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x00000000);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x00000000);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x00000000);
		if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x0fc00e00);
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2  , 0xe00);
		}
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	} else if (csc_mode == VPP_MATRIX_YUV601F_RGB) {
		/* ycbcr full range, 601F to RGB */
		/*  0    1    0           1.402    0
		   -128  1   -0.34414    -0.71414  0
		   -128  1    1.772       0        0 */
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, (0x400 << 16) | 0);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, (0x59c << 16) | 0x400);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, (0x1ea0 << 16) | 0x1d24);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, (0x400 << 16) | 0x718);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x0);
		if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x00000e00);
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2  , 0xe00);
		}
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	} else if (csc_mode == VPP_MATRIX_YUV709_RGB) {
		/* ycbcr limit range, 709 to RGB */
		/* -16      1.164  0      1.793  0 */
		/* -128     1.164 -0.213 -0.534  0 */
		/* -128     1.164  2.115  0      0 */
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x04A80000);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x072C04A8);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1F261DDD);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x04A80876);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x0);
		if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x0fc00e00);
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2  , 0xe00);
		}
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	} else if (csc_mode == VPP_MATRIX_YUV709F_RGB) {
		/* ycbcr full range, 709F to RGB */
		/*  0    1      0       1.575   0
		   -128  1     -0.187  -0.468   0
		   -128  1      1.856   0       0 */
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x04000000);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x064D0400);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1F411E21);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x0400076D);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x0);
		if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x00000e00);
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2  , 0xe00);
		}
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	} else if (csc_mode == VPP_MATRIX_NULL) {
		/* bypass matrix */
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x04000000);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x04000000);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x00000000);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x00000400);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x0);
		if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x0);
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0);
		}
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	} else if (csc_mode >= VPP_MATRIX_BT2020YUV_BT2020RGB) {
		if (vd1_or_vd2_or_post == VPP_MATRIX_VD1) {
			/* bt2020 limit to bt2020 RGB  */
			WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x4ad0000);
			WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x6e50492);
			WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1f3f1d63);
			WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x492089a);
			WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x0);
			WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x0);
			WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x0);
			if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
					WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1,
								0x0fc00e00);
					WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2,
								0xe00);
			}
			WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
		}
		if (vd1_or_vd2_or_post == VPP_MATRIX_POST) {
			if (csc_mode == VPP_MATRIX_BT2020YUV_BT2020RGB) {
				/* 2020 RGB to R'G'B */
				WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x0);
				WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0);
				WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0xd491b4d);
				WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x1f6b1f01);
				WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x9101fef);
				WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x1fdb1f32);
				WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x108f3);
				WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x0);
				WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x0);
				WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 1, 5, 3);
#if 0 /* disable this case after calculate mtx on the fly*/
			} else if (csc_mode == VPP_MATRIX_BT2020RGB_709RGB) {
				/* 2020 RGB(G:33c2,86c4 B:1d4c,0bb8 R:84d0,3e80)
				to R'G'B' */
				WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x0);
				WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0);
				/* from Jason */
				WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x9cd1e33);
				WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x00001faa);
				WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x8560000);
				WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x1fd81f5f);
				WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x108c9);
				WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x0);
				WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x0);
				WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 1, 5, 3);
#endif
			} else if (csc_mode == VPP_MATRIX_BT2020RGB_CUSRGB) {
				/* customer matrix 2020 RGB to R'G'B' */
				WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1,
					(m->pre_offset[0] << 16)
					| (m->pre_offset[1] & 0xffff));
				WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2,
					m->pre_offset[2] & 0xffff);
				WRITE_VPP_REG(VPP_MATRIX_COEF00_01,
					(m->matrix[0][0] << 16)
					| (m->matrix[0][1] & 0xffff));
				WRITE_VPP_REG(VPP_MATRIX_COEF02_10,
					(m->matrix[0][2] << 16)
					| (m->matrix[1][0] & 0xffff));
				WRITE_VPP_REG(VPP_MATRIX_COEF11_12,
					(m->matrix[1][1] << 16)
					| (m->matrix[1][2] & 0xffff));
				WRITE_VPP_REG(VPP_MATRIX_COEF20_21,
					(m->matrix[2][0] << 16)
					| (m->matrix[2][1] & 0xffff));
				WRITE_VPP_REG(VPP_MATRIX_COEF22,
					(m->right_shift << 16)
					| (m->matrix[2][2] & 0xffff));
				WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1,
					(m->offset[0] << 16)
					| (m->offset[1] & 0xffff));
				WRITE_VPP_REG(VPP_MATRIX_OFFSET2,
					m->offset[2] & 0xffff);
				WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
					m->right_shift, 5, 3);
			}
		}
	}
}

/* matrix betreen xvycclut and linebuffer*/
static uint cur_csc_mode = 0xff;
static void vpp_set_matrix3(
		unsigned int on,
		enum vpp_matrix_csc_e csc_mode)
{
	WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, on, 6, 1);

	if (!on)
		return;

	if (cur_csc_mode == csc_mode)
		return;

	WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 3, 8, 2);
	if (csc_mode == VPP_MATRIX_RGB_YUV709F) {
		/* RGB -> 709F*/
		/*WRITE_VPP_REG(VPP_MATRIX_CTRL, 0x7360);*/

		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0xda02dc);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x4a1f8a);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1e760200);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x2001e2f);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x1fd1);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x200);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x200);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0);
	}
	cur_csc_mode = csc_mode;
}

static uint cur_signal_type = 0xffffffff;
module_param(cur_signal_type, uint, 0664);
MODULE_PARM_DESC(cur_signal_type, "\n cur_signal_type\n");

static struct vframe_master_display_colour_s cur_master_display_colour = {
	0,
	{
		{0, 0},
		{0, 0},
		{0, 0},
	},
	{0, 0},
	{0, 0}
};

#define SIG_CS_CHG	0x01
#define SIG_SRC_CHG	0x02
#define SIG_PRI_INFO	0x04
#define SIG_KNEE_FACTOR	0x08
#define SIG_HDR_MODE	0x10
#define SIG_HDR_SUPPORT	0x20
int signal_type_changed(struct vframe_s *vf, struct vinfo_s *vinfo)
{
	u32 signal_type = 0;
	u32 default_signal_type;
	int change_flag = 0;
	int i, j;
	struct vframe_master_display_colour_s *p_cur;
	struct vframe_master_display_colour_s *p_new;

	if ((vf->source_type == VFRAME_SOURCE_TYPE_TUNER) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_CVBS) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_COMP) ||
		(vf->source_type == VFRAME_SOURCE_TYPE_HDMI)) {
		default_signal_type =
			/* default 709 full */
			  (1 << 29)	/* video available */
			| (5 << 26)	/* unspecified */
			| (1 << 25)	/* full */
			| (1 << 24)	/* color available */
			| (1 << 16)	/* bt709 */
			| (1 << 8)	/* bt709 */
			| (1 << 0);	/* bt709 */
	} else { /* for local play */
		if (vf->height >= 720)
			default_signal_type =
				/* HD default 709 limit */
				  (1 << 29)	/* video available */
				| (5 << 26)	/* unspecified */
				| (0 << 25)	/* limit */
				| (1 << 24)	/* color available */
				| (1 << 16)	/* bt709 */
				| (1 << 8)	/* bt709 */
				| (1 << 0);	/* bt709 */
		else
			default_signal_type =
				/* SD default 601 limited */
				  (1 << 29)	/* video available */
				| (5 << 26)	/* unspecified */
				| (0 << 25)	/* limited */
				| (1 << 24)	/* color available */
				| (3 << 16)	/* bt601 */
				| (3 << 8)	/* bt601 */
				| (3 << 0);	/* bt601 */
	}
	if (vf->signal_type & (1 << 29))
		signal_type = vf->signal_type;
	else
		signal_type = default_signal_type;

	p_new = &vf->prop.master_display_colour;
	p_cur = &cur_master_display_colour;
	if (p_new->present_flag) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++) {
				if (p_cur->primaries[i][j]
				 != p_new->primaries[i][j])
					change_flag |= SIG_PRI_INFO;
				p_cur->primaries[i][j]
					= p_new->primaries[i][j];
			}
		for (i = 0; i < 2; i++) {
			if (p_cur->white_point[i]
			 != p_new->white_point[i])
				change_flag |= SIG_PRI_INFO;
			p_cur->white_point[i]
				= p_new->white_point[i];
		}
		for (i = 0; i < 2; i++) {
			if (p_cur->luminance[i]
			 != p_new->luminance[i])
				change_flag |= SIG_PRI_INFO;
			p_cur->luminance[i]
				= p_new->luminance[i];
		}
		if (!p_cur->present_flag) {
			p_cur->present_flag = 1;
			change_flag |= SIG_PRI_INFO;
		}
	} else if (p_cur->present_flag) {
		p_cur->present_flag = 0;
		change_flag |= SIG_PRI_INFO;
	}
	if (change_flag & SIG_PRI_INFO)
		pr_amvecm_dbg("Master_display_colour changed.\n");

	if (signal_type != cur_signal_type) {
		pr_amvecm_dbg("Signal type changed from 0x%x to 0x%x.\n",
			cur_signal_type, signal_type);
		change_flag |= SIG_CS_CHG;
		cur_signal_type = signal_type;
	}
	if (pre_src_type != vf->source_type) {
		pr_amvecm_dbg("Signal source changed from 0x%x to 0x%x.\n",
			pre_src_type, vf->source_type);
		change_flag |= SIG_SRC_CHG;
		pre_src_type = vf->source_type;
	}
	if (cur_knee_factor != knee_factor) {
		pr_amvecm_dbg("Knee factor changed.\n");
		change_flag |= SIG_KNEE_FACTOR;
	}
	if (cur_hdr_process_mode != hdr_process_mode) {
		pr_amvecm_dbg("HDR mode changed.\n");
		change_flag |= SIG_HDR_MODE;
		cur_hdr_process_mode = hdr_process_mode;
	}
	if (cur_hdr_support != (vinfo->hdr_info.hdr_support & 0x4)) {
		pr_amvecm_dbg("Tx HDR support changed.\n");
		change_flag |= SIG_HDR_SUPPORT;
		cur_hdr_support = vinfo->hdr_info.hdr_support & 0x4;
	}

	return change_flag;
}

#define signal_range ((cur_signal_type >> 25) & 1)
#define signal_color_primaries ((cur_signal_type >> 16) & 0xff)
#define signal_transfer_characteristic ((cur_signal_type >> 8) & 0xff)
enum vpp_matrix_csc_e get_csc_type(void)
{
	enum vpp_matrix_csc_e csc_type = VPP_MATRIX_NULL;
	if (signal_color_primaries == 1) {
		if (signal_range == 0)
			csc_type = VPP_MATRIX_YUV709_RGB;
		else
			csc_type = VPP_MATRIX_YUV709F_RGB;
	} else if (signal_color_primaries == 3) {
		if (signal_range == 0)
			csc_type = VPP_MATRIX_YUV601_RGB;
		else
			csc_type = VPP_MATRIX_YUV601F_RGB;
	} else if (signal_color_primaries == 9) {
		if (signal_transfer_characteristic == 16) {
			/* smpte st-2084 */
			if (signal_range == 0)
				csc_type = VPP_MATRIX_BT2020YUV_BT2020RGB;
			else {
				pr_amvecm_dbg("\tWARNING: full range HDR!!!\n");
				csc_type = VPP_MATRIX_BT2020YUV_BT2020RGB;
			}
		} else if (signal_transfer_characteristic == 14) {
			/* bt2020-10 */
			pr_amvecm_dbg("\tWARNING: bt2020-10 HDR!!!\n");
			if (signal_range == 0)
				csc_type = VPP_MATRIX_YUV709_RGB;
			else
				csc_type = VPP_MATRIX_YUV709F_RGB;
		} else if (signal_transfer_characteristic == 15) {
			/* bt2020-12 */
			pr_amvecm_dbg("\tWARNING: bt2020-12 HDR!!!\n");
			if (signal_range == 0)
				csc_type = VPP_MATRIX_YUV709_RGB;
			else
				csc_type = VPP_MATRIX_YUV709F_RGB;
		} else {
			/* unknown transfer characteristic */
			pr_amvecm_dbg("\tWARNING: unknown HDR!!!\n");
			if (signal_range == 0)
				csc_type = VPP_MATRIX_YUV709_RGB;
			else
				csc_type = VPP_MATRIX_YUV709F_RGB;
		}
	} else {
		pr_amvecm_dbg("\tWARNING: unsupported colour space!!!\n");
		if (signal_range == 0)
			csc_type = VPP_MATRIX_YUV601_RGB;
		else
			csc_type = VPP_MATRIX_YUV601F_RGB;
	}
	return csc_type;
}

static void mtx_dot_mul(
	int64_t (*a)[3], int64_t (*b)[3],
	int64_t (*out)[3], int32_t norm)
{
	int i, j;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			out[i][j] = (a[i][j] * b[i][j] + (norm >> 1)) / norm;
}

static void mtx_mul(int64_t (*a)[3], int64_t *b, int64_t *out, int32_t norm)
{
	int j, k;

	for (j = 0; j < 3; j++) {
		out[j] = 0;
		for (k = 0; k < 3; k++)
			out[j] += a[k][j] * b[k];
		out[j] = (out[j] + (norm >> 1)) / norm;
	}
}

static void mtx_mul_mtx(
	int64_t (*a)[3], int64_t (*b)[3],
	int64_t (*out)[3], int32_t norm)
{
	int i, j, k;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++) {
			out[i][j] = 0;
			for (k = 0; k < 3; k++)
				out[i][j] += a[k][j] * b[i][k];
			out[i][j] = (out[i][j] + (norm >> 1)) / norm;
		}
}

static void inverse_3x3(
	int64_t (*in)[3], int64_t (*out)[3],
	int32_t norm, int32_t obl)
{
	int i, j;
	int64_t determinant = 0;

	for (i = 0; i < 3; i++)
		determinant +=
			in[0][i] * (in[1][(i + 1) % 3] * in[2][(i + 2) % 3]
			- in[1][(i + 2) % 3] * in[2][(i + 1) % 3]);
	determinant = (determinant + 1) >> 1;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			out[j][i] = (in[(i + 1) % 3][(j + 1) % 3]
				* in[(i + 2) % 3][(j + 2) % 3]);
			out[j][i] -= (in[(i + 1) % 3][(j + 2) % 3]
				* in[(i + 2) % 3][(j + 1) % 3]);
			out[j][i] = (out[j][i] * norm) << (obl - 1);
			out[j][i] =
				(out[j][i] + (determinant >> 1)) / determinant;
		}
	}
}

static void calc_T(
	int32_t (*prmy)[2], int64_t (*Tout)[3],
	int32_t norm, int32_t obl)
{
	int i, j;
	int64_t z[4];
	int64_t A[3][3];
	int64_t B[3];
	int64_t C[3];
	int64_t D[3][3];
	int64_t E[3][3];

	for (i = 0; i < 4; i++)
		z[i] = norm - prmy[i][0] - prmy[i][1];

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 2; j++)
			A[i][j] = prmy[i][j];
		A[i][2] = z[i];
	}
	B[0] = (norm * prmy[3][0] * 2 / prmy[3][1] + 1) >> 1;
	B[1] = norm;
	B[2] = (norm * z[3] * 2 / prmy[3][1] + 1) >> 1;
	inverse_3x3(A, D, norm, obl);
	mtx_mul(D, B, C, norm);
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			E[i][j] = C[i];
	mtx_dot_mul(A, E, Tout, norm);
}

static void gamut_mtx(
	int32_t (*src_prmy)[2], int32_t (*dst_prmy)[2],
	int64_t (*Tout)[3], int32_t norm, int32_t obl)
{
	int64_t Tsrc[3][3];
	int64_t Tdst[3][3];
	int64_t out[3][3];

	calc_T(src_prmy, Tsrc, norm, obl);
	calc_T(dst_prmy, Tdst, norm, obl);
	inverse_3x3(Tdst, out, 1 << obl, obl);
	mtx_mul_mtx(out, Tsrc, Tout, 1 << obl);
}

static void N2C(int64_t (*in)[3], int32_t ibl, int32_t obl)
{
	int i, j;
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++) {
			in[i][j] =
				(in[i][j] + (1 << (ibl - 12))) >> (ibl - 11);
			if (in[i][j] < 0)
				in[i][j] += 1 << obl;
		}
}

static void cal_mtx_seting(
	int64_t (*in)[3],
	int32_t ibl, int32_t obl,
	struct matrix_s *m)
{
	int i, j;
	N2C(in, ibl, obl);
	pr_amvecm_dbg("\tHDR color convert matrix:\n");
	for (i = 0; i < 3; i++) {
		m->pre_offset[i] = 0;
		for (j = 0; j < 3; j++)
			m->matrix[i][j] = in[j][i];
		m->offset[i] = 0;
		pr_amvecm_dbg("\t\t%04x %04x %04x\n",
			(int)(m->matrix[i][0] & 0xffff),
			(int)(m->matrix[i][1] & 0xffff),
			(int)(m->matrix[i][2] & 0xffff));
	}
	m->right_shift = 1;
}

static int check_primaries(
	/* src primaries and white point */
	uint (*p)[3][2],
	uint (*w)[2],
	/* dst display info from vinfo */
	const struct vinfo_s *v,
	/* prepare src and dst color info array */
	int32_t (*si)[4][2], int32_t (*di)[4][2])
{
	int i, j;
	int need_calculate_mtx = 0;
	const struct master_display_info_s *d;

	/* check source */
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 2; j++) {
			(*si)[i][j] = (*p)[(i + 2) % 3][j];
			if ((*si)[i][j] != bt2020_primaries[(i + 2) % 3][j])
				need_calculate_mtx = 1;
		}
	}
	for (i = 0; i < 2; i++) {
		(*si)[3][i] = (*w)[i];
		if ((*si)[3][i] != bt2020_white_point[i])
			need_calculate_mtx = 1;
	}

	if (((*si)[0][0] == 0) &&
		((*si)[0][1] == 0) &&
		((*si)[1][0] == 0) &&
		((*si)[1][1] == 0) &&
		((*si)[2][0] == 0) &&
		((*si)[2][1] == 0) &&
		((*si)[3][0] == 0) &&
		((*si)[3][0] == 0))
		/* if primaries is 0, set default mtx*/
		need_calculate_mtx = 0;

	/* check display */
	if (v->master_display_info.present_flag) {
		d = &v->master_display_info;
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 2; j++) {
				(*di)[i][j] = d->primaries[(i + 2) % 3][j];
				if ((*di)[i][j] !=
				bt709_primaries[(i + 2) % 3][j])
					need_calculate_mtx = 1;
			}
		}
		for (i = 0; i < 2; i++) {
			(*di)[3][i] = d->white_point[i];
			if ((*di)[3][i] != bt709_white_point[i])
				need_calculate_mtx = 1;
		}
	} else {
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 2; j++)
				(*di)[i][j] = bt709_primaries[(i + 2) % 3][j];
		}
		for (i = 0; i < 2; i++)
			(*di)[3][i] = bt709_white_point[i];
	}
	return need_calculate_mtx;
}

enum vpp_matrix_csc_e prepare_customer_matrix(
	u32 (*s)[3][2],	/* source primaries */
	u32 (*w)[2],	/* source white point */
	const struct vinfo_s *v, /* vinfo carry display primaries */
	struct matrix_s *m)
{
	int32_t prmy_src[4][2];
	int32_t prmy_dst[4][2];
	int64_t out[3][3];
	int i, j;

	if (customer_matrix_en) {
		for (i = 0; i < 3; i++) {
			m->pre_offset[i] =
				customer_matrix_param[i];
			for (j = 0; j < 3; j++)
				m->matrix[i][j] =
					customer_matrix_param[3 + i * 3 + j];
			m->offset[i] =
				customer_matrix_param[12 + i];
		}
		m->right_shift =
			customer_matrix_param[15];
		return VPP_MATRIX_BT2020RGB_CUSRGB;
	} else {
		if (check_primaries(s, w, v, &prmy_src, &prmy_dst)) {
			gamut_mtx(prmy_src, prmy_dst, out, INORM, BL);
			cal_mtx_seting(out, BL, 13, m);
			return VPP_MATRIX_BT2020RGB_CUSRGB;
		}
	}
	return VPP_MATRIX_BT2020YUV_BT2020RGB;
}

/* Max luminance lookup table for contrast */
static const int maxLuma_thrd[5] = {512, 1024, 2048, 4096, 8192};
static int calculate_contrast_adj(int max_lumin)
{
	int k;
	int left, right, norm, alph;
	int ratio, target_contrast;

	if (max_lumin < maxLuma_thrd[0])
		k = 0;
	else if (max_lumin < maxLuma_thrd[1])
		k = 1;
	else if (max_lumin < maxLuma_thrd[2])
		k = 2;
	else if (max_lumin < maxLuma_thrd[3])
		k = 3;
	else if (max_lumin < maxLuma_thrd[4])
		k = 4;
	else
		k = 5;

	if (k == 0)
		ratio = extra_con_lut[0];
	else if (k == 5)
		ratio = extra_con_lut[4];
	else {
		left = extra_con_lut[k - 1];
		right = extra_con_lut[k];
		norm = maxLuma_thrd[k] - maxLuma_thrd[k - 1];
		alph = max_lumin - maxLuma_thrd[k - 1];
		ratio = left + (alph * (right - left) + (norm >> 1)) / norm;
	}
	target_contrast = ((vd1_contrast + 1024) * ratio + 64) >> 7;
	target_contrast = clip(target_contrast, 0, 2047);
	target_contrast -= 1024;
	return target_contrast - vd1_contrast;
}

static void print_primaries_info(struct vframe_master_display_colour_s *p)
{
	int i, j;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 2; j++)
			pr_amvecm_dbg(
				"\t\tprimaries[%1d][%1d] = %04x\n",
				i, j,
				p->primaries[i][j]);
	pr_amvecm_dbg("\t\twhite_point = (%04x, %04x)\n",
		p->white_point[0], p->white_point[1]);
	pr_amvecm_dbg("\t\tmax,min luminance = %08x, %08x\n",
		p->luminance[0], p->luminance[1]);
}

static void amvecm_cp_hdr_info(struct master_display_info_s *hdr_data,
		struct vframe_s *vf)
{
	int i, j;

	if (((hdr_data->features >> 16) & 0xff) == 9) {
		if (vf->prop.master_display_colour.present_flag) {

			memcpy(hdr_data->primaries,
				vf->prop.master_display_colour.primaries,
				sizeof(u32)*6);
			memcpy(hdr_data->white_point,
				vf->prop.master_display_colour.white_point,
				sizeof(u32)*2);
			hdr_data->luminance[0] =
				vf->prop.master_display_colour.luminance[0];
			hdr_data->luminance[1] =
				vf->prop.master_display_colour.luminance[1];
		} else {
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++)
					hdr_data->primaries[i][j] =
							bt2020_primaries[i][j];
			hdr_data->white_point[0] = bt709_white_point[0];
			hdr_data->white_point[1] = bt709_white_point[1];
			/* default luminance
			 * (got from exodus uhd hdr exodus draft.mp4) */
			hdr_data->luminance[0] = 0xb71b00;
			hdr_data->luminance[1] = 0xc8;
		}
		hdr_data->luminance[0] = hdr_data->luminance[0] / 10000;
		hdr_data->present_flag = 1;
	}
	/* hdr send information debug */
	memcpy(&dbg_hdr_send, hdr_data,
			sizeof(struct master_display_info_s));

}
static void vpp_matrix_update(struct vframe_s *vf)
{
	struct vinfo_s *vinfo;
	enum vpp_matrix_csc_e csc_type = VPP_MATRIX_NULL;
	int signal_change_flag = 0;
	struct vframe_master_display_colour_s *p;
	struct matrix_s m = {
		{0, 0, 0},
		{
			{0x0d49, 0x1b4d, 0x1f6b},
			{0x1f01, 0x0910, 0x1fef},
			{0x1fdb, 0x1f32, 0x08f3},
		},
		{0, 0, 0},
		1
	};
	struct master_display_info_s send_info;
	int need_adjust_contrast = 0;

	if ((get_cpu_type() < MESON_CPU_MAJOR_ID_GXTVBB) ||
		is_meson_gxl_package_905M2())
		return;

	/* debug vframe info backup */
	dbg_vf = vf;

	vinfo = get_current_vinfo();

	/* Tx hdr information */
	memcpy(&receiver_hdr_info, &vinfo->hdr_info,
			sizeof(struct hdr_info));

	/* check hdr support info from Tx or Panel */
	if (hdr_mode == 2) { /* auto */
		if (vinfo->hdr_info.hdr_support & 0x4)
			hdr_process_mode = 0; /* hdr->hdr*/
		else
			hdr_process_mode = 1; /* hdr->sdr*/
	} else
		hdr_process_mode = hdr_mode;

	signal_change_flag = signal_type_changed(vf, vinfo);

	if ((!signal_change_flag) && (force_csc_type == 0xff))
		return;

	vecm_latch_flag |= FLAG_MATRIX_UPDATE;

	if (force_csc_type != 0xff)
		csc_type = force_csc_type;
	else
		csc_type = get_csc_type();

	/* send signal info to tx
	 * TODO: need discuss with TX driver
	 */
	if (vinfo->viu_color_fmt != TVIN_RGB444) {
		/* bypass mode */
		if ((hdr_process_mode == 0) &&
			(csc_type >= VPP_MATRIX_BT2020YUV_BT2020RGB)) {
			/* source is hdr, send hdr info */
			/* use the features to discribe source info */
			send_info.features =
					  (1 << 29)	/* video available */
					| (5 << 26)	/* unspecified */
					| (0 << 25)	/* limit */
					| (1 << 24)	/* color available */
					| (9 << 16)	/* bt2020 */
					| (14 << 8)	/* bt2020-10 */
					| (10 << 0);	/* bt2020c */
		} else {
			/* sdr source send normal info
			 * use the features to discribe source info */
			send_info.features =
					/* default 709 full */
					  (1 << 29)	/* video available */
					| (5 << 26)	/* unspecified */
					| (1 << 25)	/* full */
					| (1 << 24)	/* color available */
					| (1 << 16)	/* bt709 */
					| (1 << 8)	/* bt709 */
					| (1 << 0);	/* bt709 */
		}
		amvecm_cp_hdr_info(&send_info, vf);
		if (vinfo->fresh_tx_hdr_pkt)
			vinfo->fresh_tx_hdr_pkt(&send_info);
	}

	if ((cur_csc_type != csc_type)
	|| (signal_change_flag
	& (SIG_PRI_INFO | SIG_KNEE_FACTOR | SIG_HDR_MODE))) {
		/* check output format(Panel or Tx)  */
		if ((vinfo->viu_color_fmt != TVIN_RGB444)
		&& (get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB))
			vpp_set_matrix3(CSC_ON, VPP_MATRIX_RGB_YUV709F);
		else
			vpp_set_matrix3(CSC_OFF, VPP_MATRIX_NULL);
		/* decided by edid or panel info or user setting */
		if (!hdr_process_mode) {
			/* hdr->hdr */
			if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
				WRITE_VPP_REG_BITS(VIU_OSD1_BLK0_CFG_W0,
					1, 7, 1);
				vpp_set_matrix(VPP_MATRIX_VD1, CSC_OFF,
						csc_type, NULL);
				vpp_set_matrix3(CSC_OFF, VPP_MATRIX_NULL);
			} else {
			if (csc_type == VPP_MATRIX_BT2020YUV_BT2020RGB) {
					/* bypass hdr processing*/
					/* TODO: use post matrix to do
					   rgb to yuv instead of matrix3 */
					csc_type = VPP_MATRIX_YUV709_RGB;
					vpp_set_matrix(VPP_MATRIX_VD1, CSC_ON,
							csc_type, NULL);
				} else {
					/* vd1 matrix on */
					vpp_set_matrix(VPP_MATRIX_VD1, CSC_ON,
							csc_type, NULL);
				}
			}
			/* post matrix off */
			vpp_set_matrix(VPP_MATRIX_POST, CSC_OFF,
				csc_type, NULL);
			/* xvycc lut off */
			load_knee_lut(CSC_OFF);
			/* osd YUV blend */
		} else {
			/* hdr->sdr */
			if (csc_type == VPP_MATRIX_BT2020YUV_BT2020RGB) {
				/* bt2020 to RGB */
				if ((signal_change_flag
				& (SIG_PRI_INFO | SIG_KNEE_FACTOR
				| SIG_HDR_MODE)) || (cur_csc_type
				< VPP_MATRIX_BT2020YUV_BT2020RGB)) {
					p = &cur_master_display_colour;
					if (p->present_flag) {
						pr_amvecm_dbg("\tMaster_display_colour available.\n");
						print_primaries_info(p);
						csc_type =
							prepare_customer_matrix(
								&p->primaries,
								&p->white_point,
								vinfo, &m);
						need_adjust_contrast = 1;
					} else {
						/* use bt2020 primaries */
						pr_amvecm_dbg("\tNo master_display_colour.\n");
						csc_type =
						prepare_customer_matrix(
							&bt2020_primaries,
							&bt2020_white_point,
							vinfo, &m);
						need_adjust_contrast = 0;
					}
					/* turn vd1 matrix on */
					vpp_set_matrix(VPP_MATRIX_VD1, CSC_ON,
						csc_type, NULL);
					/* turn post matrix on */
					vpp_set_matrix(VPP_MATRIX_POST, CSC_ON,
						csc_type, &m);
					/* xvycc lut on */
					load_knee_lut(CSC_ON);
					if (get_cpu_type() >
					MESON_CPU_MAJOR_ID_GXTVBB) {
						vpp_set_matrix3(CSC_ON,
						VPP_MATRIX_RGB_YUV709F);
						WRITE_VPP_REG_BITS(
							VIU_OSD1_BLK0_CFG_W0,
							0, 7, 1);
					}
				}
			} else {
				/* vd1 matrix on */
				if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB)
					vpp_set_matrix(VPP_MATRIX_VD1, CSC_OFF,
						csc_type, NULL);
				else
					vpp_set_matrix(VPP_MATRIX_VD1, CSC_ON,
						csc_type, NULL);
				/* post matrix off */
				vpp_set_matrix(VPP_MATRIX_POST, CSC_OFF,
						csc_type, NULL);
				/* xvycc lut off */
				load_knee_lut(CSC_OFF);
				if (get_cpu_type() >
				MESON_CPU_MAJOR_ID_GXTVBB) {
					vpp_set_matrix3(CSC_OFF,
						VPP_MATRIX_NULL);
					WRITE_VPP_REG_BITS(
						VIU_OSD1_BLK0_CFG_W0,
						1, 7, 1);
				}
			}
		}
		if (need_adjust_contrast) {
			vd1_contrast_offset =
				calculate_contrast_adj(p->luminance[0] / 10000);
			vecm_latch_flag |= FLAG_VADJ1_CON;
		} else{
			vd1_contrast_offset = 0;
			vecm_latch_flag |= FLAG_VADJ1_CON;
		}
		if (cur_csc_type != csc_type) {
			pr_amvecm_dbg("CSC from 0x%x to 0x%x.\n",
				cur_csc_type, csc_type);
			pr_amvecm_dbg("contrast offset = %d.\n",
				vd1_contrast_offset);
			cur_csc_type = csc_type;
		}
	}
	vecm_latch_flag &= ~FLAG_MATRIX_UPDATE;
}

static void amvecm_size_patch(void)
{
	unsigned int hs, he, vs, ve;
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) {
		hs = READ_VPP_REG_BITS(VPP_HSC_REGION12_STARTP, 16, 12);
		he = READ_VPP_REG_BITS(VPP_HSC_REGION4_ENDP, 0, 12);

		vs = READ_VPP_REG_BITS(VPP_VSC_REGION12_STARTP, 16, 12);
		ve = READ_VPP_REG_BITS(VPP_VSC_REGION4_ENDP, 0, 12);
		ve_frame_size_patch(he-hs+1, ve-vs+1);
	}
	hs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 16, 12);
	he = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 0, 12);

	vs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 16, 12);
	ve = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 0, 12);
	cm2_frame_size_patch(he-hs+1, ve-vs+1);
}

/* video adj1 */
static ssize_t video_adj1_brightness_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	s32 val = (READ_VPP_REG(VPP_VADJ1_Y) >> 8) & 0x1ff;

	val = (val << 23) >> 23;

	return sprintf(buf, "%d\n", val);
}

static ssize_t video_adj1_brightness_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d", &val);
	if ((r != 1) || (val < -255) || (val > 255))
		return -EINVAL;

	WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val, 8, 9);
	WRITE_VPP_REG(VPP_VADJ_CTRL, VPP_VADJ1_EN);

	return count;
}

static ssize_t video_adj1_contrast_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ1_Y) & 0xff) - 0x80);
}

static ssize_t video_adj1_contrast_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d", &val);
	if ((r != 1) || (val < -127) || (val > 127))
		return -EINVAL;

	val += 0x80;

	WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val, 0, 8);
	WRITE_VPP_REG(VPP_VADJ_CTRL, VPP_VADJ1_EN);

	return count;
}

/* video adj2 */
static ssize_t video_adj2_brightness_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	s32 val = (READ_VPP_REG(VPP_VADJ2_Y) >> 8) & 0x1ff;

	val = (val << 23) >> 23;

	return sprintf(buf, "%d\n", val);
}

static ssize_t video_adj2_brightness_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d", &val);
	if ((r != 1) || (val < -255) || (val > 255))
		return -EINVAL;

	WRITE_VPP_REG_BITS(VPP_VADJ2_Y, val, 8, 9);
	WRITE_VPP_REG(VPP_VADJ_CTRL, VPP_VADJ2_EN);

	return count;
}

static ssize_t video_adj2_contrast_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ2_Y) & 0xff) - 0x80);
}

static ssize_t video_adj2_contrast_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d", &val);
	if ((r != 1) || (val < -127) || (val > 127))
		return -EINVAL;

	val += 0x80;

	WRITE_VPP_REG_BITS(VPP_VADJ2_Y, val, 0, 8);
	WRITE_VPP_REG(VPP_VADJ_CTRL, VPP_VADJ2_EN);

	return count;
}

static ssize_t amvecm_usage_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	pr_info("Usage:");
	pr_info("brightness_val range:-255~255\n");
	pr_info("contrast_val range:-127~127\n");
	pr_info("saturation_val range:-128~128\n");
	pr_info("hue_val range:-25~25\n");
	pr_info("************video brightness & contrast & saturation_hue adj as flow*************\n");
	pr_info("echo brightness_val > /sys/class/amvecm/brightness1\n");
	pr_info("echo contrast_val > /sys/class/amvecm/contrast1\n");
	pr_info("echo saturation_val hue_val > /sys/class/amvecm/saturation_hue_pre\n");
	pr_info("************after video+osd blender, brightness & contrast & saturation_hue adj as flow*************\n");
	pr_info("echo brightness_val > /sys/class/amvecm/brightness2\n");
	pr_info("echo contrast_val > /sys/class/amvecm/contrast2\n");
	pr_info("echo saturation_val hue_val > /sys/class/amvecm/saturation_hue_post\n");
	return 0;
}

static void parse_param_amvecm(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;

	ps = buf_orig;
	while (1) {
		token = strsep(&ps, " \n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}
static ssize_t amvecm_3d_sync_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned int sync_h_start, sync_h_end, sync_v_start,
		sync_v_end, sync_polarity,
		sync_out_inv, sync_en;

	if (!is_meson_g9tv_cpu()) {
		len += sprintf(buf+len,
				"\n chip does not support 3D sync process!!!\n");
		return len;
	}

	sync_h_start = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 0, 13);
	sync_h_end = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 16, 13);
	sync_v_start = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 13);
	sync_v_end = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 16, 13);
	sync_polarity = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 29, 1);
	sync_out_inv = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 15, 1);
	sync_en = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 31, 1);
	len += sprintf(buf+len, "\n current 3d sync state:\n");
	len += sprintf(buf+len, "sync_h_start:%d\n", sync_h_start);
	len += sprintf(buf+len, "sync_h_end:%d\n", sync_h_end);
	len += sprintf(buf+len, "sync_v_start:%d\n", sync_v_start);
	len += sprintf(buf+len, "sync_v_end:%d\n", sync_v_end);
	len += sprintf(buf+len, "sync_polarity:%d\n", sync_polarity);
	len += sprintf(buf+len, "sync_out_inv:%d\n", sync_out_inv);
	len += sprintf(buf+len, "sync_en:%d\n", sync_en);
	len += sprintf(buf+len,
			"echo hstart val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
			"echo hend val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
			"echo vstart val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
			"echo vend val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
			"echo pola val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf+len,
			"echo inv val(D) > /sys/class/amvecm/sync_3d\n");
	return len;
}

static ssize_t amvecm_3d_sync_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val;

	if (!buf)
		return count;

	if (!is_meson_g9tv_cpu()) {
		pr_info("\n chip does not support 3D sync process!!!\n");
		return count;
	}

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "hstart", 6)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_h_start = val&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_start, 0, 13);
	} else if (!strncmp(parm[0], "hend", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_h_end = val&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_end, 16, 13);
	} else if (!strncmp(parm[0], "vstart", 6)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_v_start = val&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_start, 0, 13);
	} else if (!strncmp(parm[0], "vend", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_v_end = val&0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_end, 16, 13);
	} else if (!strncmp(parm[0], "pola", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_polarity = val&0x1;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_polarity, 29, 1);
	} else if (!strncmp(parm[0], "inv", 3)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		sync_3d_out_inv = val&0x1;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_out_inv, 15, 1);
	}
	kfree(buf_orig);
	return count;
}

/* #endif */

void pq_enable_disable(void)
{

	if (pq_on_off == 1) {
		pq_on_off = 2;
		/* open dnlp clock gate */
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL1, 0, 0, 2);
		dnlp_en = 1;
		ve_enable_dnlp();
		/* open cm clock gate */
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL0, 0, 4, 2);
		cm_en = 1;
		amcm_enable();
 /* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
		if (is_meson_gxtvbb_cpu()) {
			/* open sharpness clock gate */
			/*WRITE_VPP_REG_BITS(VPP_GCLK_CTRL0, 0, 30, 2);*/
			/* sharpness on */
			WRITE_VPP_REG_BITS(
				SRSHARP0_SHARP_PK_NR_ENABLE,
				1, 1, 1);
			WRITE_VPP_REG_BITS(
				SRSHARP1_SHARP_PK_NR_ENABLE,
				1, 1, 1);
			/* wb on */
			wb_en = 1;
			WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 1, 31, 1);
			/* gamma on */
			vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
		}
/* #endif */
	} else if (pq_on_off == 0) {
		pq_on_off = 2;

		dnlp_en = 0;
		ve_disable_dnlp();
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL1, 1, 0, 2);
		cm_en = 0;
		amcm_disable();
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL0, 1, 4, 2);
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
		if (is_meson_gxtvbb_cpu()) {
			WRITE_VPP_REG_BITS(
				SRSHARP0_SHARP_PK_NR_ENABLE,
				0, 1, 1);
			WRITE_VPP_REG_BITS(
				SRSHARP1_SHARP_PK_NR_ENABLE,
				0, 1, 1);
			/*WRITE_VPP_REG_BITS(VPP_GCLK_CTRL0, 1, 30, 2);*/
			wb_en = 0;
			WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 0, 31, 1);
			vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
		}
/* #endif */
	}

	if (cm_on_off == 1) {
		cm_on_off = 2;
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL0, 0, 4, 2);
		cm_en = 1;
		amcm_enable();
	} else if (cm_on_off == 0) {
		cm_on_off = 2;
		cm_en = 0;
		amcm_disable();
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL0, 1, 4, 2);
	}

	if (dnlp_on_off == 1) {
		dnlp_on_off = 2;
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL1, 0, 0, 2);
		dnlp_en = 1;
		ve_enable_dnlp();
	} else if (dnlp_on_off == 0) {
		dnlp_on_off = 2;
		dnlp_en = 0;
		ve_disable_dnlp();
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL1, 1, 0, 2);
	}

/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_gxtvbb_cpu()) {
		if (sharpness_on_off == 1) {
			sharpness_on_off = 2;
			WRITE_VPP_REG_BITS(VPP_GCLK_CTRL0, 0, 30, 2);
			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 1, 1);
		} else if (sharpness_on_off == 0) {
			sharpness_on_off = 2;
			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 1, 1);
			WRITE_VPP_REG_BITS(VPP_GCLK_CTRL0, 1, 30, 2);
		}

		if (wb_on_off == 1) {
			wb_on_off = 2;
			wb_en = 1;
			WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 1, 31, 1);
		} else if (wb_on_off == 0) {
			wb_on_off = 2;
			wb_en = 0;
			WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 0, 31, 1);
		}
	}

/* #endif */

}

static void vpp_backup_histgram(struct vframe_s *vf)
{
	unsigned int i = 0;

	vpp_hist_param.vpp_hist_pow = vf->prop.hist.hist_pow;
	vpp_hist_param.vpp_luma_sum = vf->prop.hist.vpp_luma_sum;
	vpp_hist_param.vpp_pixel_sum = vf->prop.hist.vpp_pixel_sum;
	for (i = 0; i < 64; i++)
		vpp_hist_param.vpp_histgram[i] = vf->prop.hist.vpp_gamma[i];
}

static void vdin_dump_histgram(void)
{
	uint i;
	pr_info("%s:\n", __func__);
	for (i = 0; i < 64; i++) {
		pr_info("[%d]0x%-8x\t", i, vpp_hist_param.vpp_histgram[i]);
		if ((i+1)%8 == 0)
			pr_info("\n");
	}
}

void vpp_get_hist_en(void)
{
	wr_bits(0, VI_HIST_CTRL, 0x1, 11, 3);
	wr_bits(0, VI_HIST_CTRL, 0x1, 0, 1);
	wr(0, VI_HIST_GCLK_CTRL, 0xffffffff);
	wr_bits(0, VI_HIST_CTRL, 2, VI_HIST_POW_BIT, VI_HIST_POW_WID);
}

void vpp_get_vframe_hist_info(struct vframe_s *vf)
{
	unsigned int hist_height, hist_width;

	hist_height = rd_bits(0, VPP_IN_H_V_SIZE, 0, 13);
	hist_width = rd_bits(0, VPP_IN_H_V_SIZE, 16, 13);

	if ((hist_height != pre_hist_height) ||
		(hist_width != pre_hist_width)) {
		pre_hist_height = hist_height;
		pre_hist_width = hist_width;
		wr_bits(0, VI_HIST_PIC_SIZE, hist_height, 16, 13);
		wr_bits(0, VI_HIST_PIC_SIZE, hist_width, 0, 13);
	}
	/* fetch hist info */
	/* vf->prop.hist.luma_sum   = READ_CBUS_REG_BITS(VDIN_HIST_SPL_VAL,
	 * HIST_LUMA_SUM_BIT,    HIST_LUMA_SUM_WID   ); */
	vf->prop.hist.hist_pow   = rd_bits(0, VI_HIST_CTRL,
			VI_HIST_POW_BIT, VI_HIST_POW_WID);
	vf->prop.hist.vpp_luma_sum   = rd(0, VI_HIST_SPL_VAL);
	/* vf->prop.hist.chroma_sum = READ_CBUS_REG_BITS(VDIN_HIST_CHROMA_SUM,
	 * HIST_CHROMA_SUM_BIT,  HIST_CHROMA_SUM_WID ); */
	vf->prop.hist.vpp_chroma_sum = rd(0, VI_HIST_CHROMA_SUM);
	vf->prop.hist.vpp_pixel_sum  = rd_bits(0, VI_HIST_SPL_PIX_CNT,
			VI_HIST_PIX_CNT_BIT, VI_HIST_PIX_CNT_WID);
	vf->prop.hist.vpp_height     = rd_bits(0, VI_HIST_PIC_SIZE,
			VI_HIST_PIC_HEIGHT_BIT, VI_HIST_PIC_HEIGHT_WID);
	vf->prop.hist.vpp_width      = rd_bits(0, VI_HIST_PIC_SIZE,
			VI_HIST_PIC_WIDTH_BIT, VI_HIST_PIC_WIDTH_WID);
	vf->prop.hist.vpp_luma_max   = rd_bits(0, VI_HIST_MAX_MIN,
			VI_HIST_MAX_BIT, VI_HIST_MAX_WID);
	vf->prop.hist.vpp_luma_min   = rd_bits(0, VI_HIST_MAX_MIN,
			VI_HIST_MIN_BIT, VI_HIST_MIN_WID);
	vf->prop.hist.vpp_gamma[0]   = rd_bits(0, VI_DNLP_HIST00,
			VI_HIST_ON_BIN_00_BIT, VI_HIST_ON_BIN_00_WID);
	vf->prop.hist.vpp_gamma[1]   = rd_bits(0, VI_DNLP_HIST00,
			VI_HIST_ON_BIN_01_BIT, VI_HIST_ON_BIN_01_WID);
	vf->prop.hist.vpp_gamma[2]   = rd_bits(0, VI_DNLP_HIST01,
			VI_HIST_ON_BIN_02_BIT, VI_HIST_ON_BIN_02_WID);
	vf->prop.hist.vpp_gamma[3]   = rd_bits(0, VI_DNLP_HIST01,
			VI_HIST_ON_BIN_03_BIT, VI_HIST_ON_BIN_03_WID);
	vf->prop.hist.vpp_gamma[4]   = rd_bits(0, VI_DNLP_HIST02,
			VI_HIST_ON_BIN_04_BIT, VI_HIST_ON_BIN_04_WID);
	vf->prop.hist.vpp_gamma[5]   = rd_bits(0, VI_DNLP_HIST02,
			VI_HIST_ON_BIN_05_BIT, VI_HIST_ON_BIN_05_WID);
	vf->prop.hist.vpp_gamma[6]   = rd_bits(0, VI_DNLP_HIST03,
			VI_HIST_ON_BIN_06_BIT, VI_HIST_ON_BIN_06_WID);
	vf->prop.hist.vpp_gamma[7]   = rd_bits(0, VI_DNLP_HIST03,
			VI_HIST_ON_BIN_07_BIT, VI_HIST_ON_BIN_07_WID);
	vf->prop.hist.vpp_gamma[8]   = rd_bits(0, VI_DNLP_HIST04,
			VI_HIST_ON_BIN_08_BIT, VI_HIST_ON_BIN_08_WID);
	vf->prop.hist.vpp_gamma[9]   = rd_bits(0, VI_DNLP_HIST04,
			VI_HIST_ON_BIN_09_BIT, VI_HIST_ON_BIN_09_WID);
	vf->prop.hist.vpp_gamma[10]  = rd_bits(0, VI_DNLP_HIST05,
			VI_HIST_ON_BIN_10_BIT, VI_HIST_ON_BIN_10_WID);
	vf->prop.hist.vpp_gamma[11]  = rd_bits(0, VI_DNLP_HIST05,
			VI_HIST_ON_BIN_11_BIT, VI_HIST_ON_BIN_11_WID);
	vf->prop.hist.vpp_gamma[12]  = rd_bits(0, VI_DNLP_HIST06,
			VI_HIST_ON_BIN_12_BIT, VI_HIST_ON_BIN_12_WID);
	vf->prop.hist.vpp_gamma[13]  = rd_bits(0, VI_DNLP_HIST06,
			VI_HIST_ON_BIN_13_BIT, VI_HIST_ON_BIN_13_WID);
	vf->prop.hist.vpp_gamma[14]  = rd_bits(0, VI_DNLP_HIST07,
			VI_HIST_ON_BIN_14_BIT, VI_HIST_ON_BIN_14_WID);
	vf->prop.hist.vpp_gamma[15]  = rd_bits(0, VI_DNLP_HIST07,
			VI_HIST_ON_BIN_15_BIT, VI_HIST_ON_BIN_15_WID);
	vf->prop.hist.vpp_gamma[16]  = rd_bits(0, VI_DNLP_HIST08,
			VI_HIST_ON_BIN_16_BIT, VI_HIST_ON_BIN_16_WID);
	vf->prop.hist.vpp_gamma[17]  = rd_bits(0, VI_DNLP_HIST08,
			VI_HIST_ON_BIN_17_BIT, VI_HIST_ON_BIN_17_WID);
	vf->prop.hist.vpp_gamma[18]  = rd_bits(0, VI_DNLP_HIST09,
			VI_HIST_ON_BIN_18_BIT, VI_HIST_ON_BIN_18_WID);
	vf->prop.hist.vpp_gamma[19]  = rd_bits(0, VI_DNLP_HIST09,
			VI_HIST_ON_BIN_19_BIT, VI_HIST_ON_BIN_19_WID);
	vf->prop.hist.vpp_gamma[20]  = rd_bits(0, VI_DNLP_HIST10,
			VI_HIST_ON_BIN_20_BIT, VI_HIST_ON_BIN_20_WID);
	vf->prop.hist.vpp_gamma[21]  = rd_bits(0, VI_DNLP_HIST10,
			VI_HIST_ON_BIN_21_BIT, VI_HIST_ON_BIN_21_WID);
	vf->prop.hist.vpp_gamma[22]  = rd_bits(0, VI_DNLP_HIST11,
			VI_HIST_ON_BIN_22_BIT, VI_HIST_ON_BIN_22_WID);
	vf->prop.hist.vpp_gamma[23]  = rd_bits(0, VI_DNLP_HIST11,
			VI_HIST_ON_BIN_23_BIT, VI_HIST_ON_BIN_23_WID);
	vf->prop.hist.vpp_gamma[24]  = rd_bits(0, VI_DNLP_HIST12,
			VI_HIST_ON_BIN_24_BIT, VI_HIST_ON_BIN_24_WID);
	vf->prop.hist.vpp_gamma[25]  = rd_bits(0, VI_DNLP_HIST12,
			VI_HIST_ON_BIN_25_BIT, VI_HIST_ON_BIN_25_WID);
	vf->prop.hist.vpp_gamma[26]  = rd_bits(0, VI_DNLP_HIST13,
			VI_HIST_ON_BIN_26_BIT, VI_HIST_ON_BIN_26_WID);
	vf->prop.hist.vpp_gamma[27]  = rd_bits(0, VI_DNLP_HIST13,
			VI_HIST_ON_BIN_27_BIT, VI_HIST_ON_BIN_27_WID);
	vf->prop.hist.vpp_gamma[28]  = rd_bits(0, VI_DNLP_HIST14,
			VI_HIST_ON_BIN_28_BIT, VI_HIST_ON_BIN_28_WID);
	vf->prop.hist.vpp_gamma[29]  = rd_bits(0, VI_DNLP_HIST14,
			VI_HIST_ON_BIN_29_BIT, VI_HIST_ON_BIN_29_WID);
	vf->prop.hist.vpp_gamma[30]  = rd_bits(0, VI_DNLP_HIST15,
			VI_HIST_ON_BIN_30_BIT, VI_HIST_ON_BIN_30_WID);
	vf->prop.hist.vpp_gamma[31]  = rd_bits(0, VI_DNLP_HIST15,
			VI_HIST_ON_BIN_31_BIT, VI_HIST_ON_BIN_31_WID);
	vf->prop.hist.vpp_gamma[32]  = rd_bits(0, VI_DNLP_HIST16,
			VI_HIST_ON_BIN_32_BIT, VI_HIST_ON_BIN_32_WID);
	vf->prop.hist.vpp_gamma[33]  = rd_bits(0, VI_DNLP_HIST16,
			VI_HIST_ON_BIN_33_BIT, VI_HIST_ON_BIN_33_WID);
	vf->prop.hist.vpp_gamma[34]  = rd_bits(0, VI_DNLP_HIST17,
			VI_HIST_ON_BIN_34_BIT, VI_HIST_ON_BIN_34_WID);
	vf->prop.hist.vpp_gamma[35]  = rd_bits(0, VI_DNLP_HIST17,
			VI_HIST_ON_BIN_35_BIT, VI_HIST_ON_BIN_35_WID);
	vf->prop.hist.vpp_gamma[36]  = rd_bits(0, VI_DNLP_HIST18,
			VI_HIST_ON_BIN_36_BIT, VI_HIST_ON_BIN_36_WID);
	vf->prop.hist.vpp_gamma[37]  = rd_bits(0, VI_DNLP_HIST18,
			VI_HIST_ON_BIN_37_BIT, VI_HIST_ON_BIN_37_WID);
	vf->prop.hist.vpp_gamma[38]  = rd_bits(0, VI_DNLP_HIST19,
			VI_HIST_ON_BIN_38_BIT, VI_HIST_ON_BIN_38_WID);
	vf->prop.hist.vpp_gamma[39]  = rd_bits(0, VI_DNLP_HIST19,
			VI_HIST_ON_BIN_39_BIT, VI_HIST_ON_BIN_39_WID);
	vf->prop.hist.vpp_gamma[40]  = rd_bits(0, VI_DNLP_HIST20,
			VI_HIST_ON_BIN_40_BIT, VI_HIST_ON_BIN_40_WID);
	vf->prop.hist.vpp_gamma[41]  = rd_bits(0, VI_DNLP_HIST20,
			VI_HIST_ON_BIN_41_BIT, VI_HIST_ON_BIN_41_WID);
	vf->prop.hist.vpp_gamma[42]  = rd_bits(0, VI_DNLP_HIST21,
			VI_HIST_ON_BIN_42_BIT, VI_HIST_ON_BIN_42_WID);
	vf->prop.hist.vpp_gamma[43]  = rd_bits(0, VI_DNLP_HIST21,
			VI_HIST_ON_BIN_43_BIT, VI_HIST_ON_BIN_43_WID);
	vf->prop.hist.vpp_gamma[44]  = rd_bits(0, VI_DNLP_HIST22,
			VI_HIST_ON_BIN_44_BIT, VI_HIST_ON_BIN_44_WID);
	vf->prop.hist.vpp_gamma[45]  = rd_bits(0, VI_DNLP_HIST22,
			VI_HIST_ON_BIN_45_BIT, VI_HIST_ON_BIN_45_WID);
	vf->prop.hist.vpp_gamma[46]  = rd_bits(0, VI_DNLP_HIST23,
			VI_HIST_ON_BIN_46_BIT, VI_HIST_ON_BIN_46_WID);
	vf->prop.hist.vpp_gamma[47]  = rd_bits(0, VI_DNLP_HIST23,
			VI_HIST_ON_BIN_47_BIT, VI_HIST_ON_BIN_47_WID);
	vf->prop.hist.vpp_gamma[48]  = rd_bits(0, VI_DNLP_HIST24,
			VI_HIST_ON_BIN_48_BIT, VI_HIST_ON_BIN_48_WID);
	vf->prop.hist.vpp_gamma[49]  = rd_bits(0, VI_DNLP_HIST24,
			VI_HIST_ON_BIN_49_BIT, VI_HIST_ON_BIN_49_WID);
	vf->prop.hist.vpp_gamma[50]  = rd_bits(0, VI_DNLP_HIST25,
			VI_HIST_ON_BIN_50_BIT, VI_HIST_ON_BIN_50_WID);
	vf->prop.hist.vpp_gamma[51]  = rd_bits(0, VI_DNLP_HIST25,
			VI_HIST_ON_BIN_51_BIT, VI_HIST_ON_BIN_51_WID);
	vf->prop.hist.vpp_gamma[52]  = rd_bits(0, VI_DNLP_HIST26,
			VI_HIST_ON_BIN_52_BIT, VI_HIST_ON_BIN_52_WID);
	vf->prop.hist.vpp_gamma[53]  = rd_bits(0, VI_DNLP_HIST26,
			VI_HIST_ON_BIN_53_BIT, VI_HIST_ON_BIN_53_WID);
	vf->prop.hist.vpp_gamma[54]  = rd_bits(0, VI_DNLP_HIST27,
			VI_HIST_ON_BIN_54_BIT, VI_HIST_ON_BIN_54_WID);
	vf->prop.hist.vpp_gamma[55]  = rd_bits(0, VI_DNLP_HIST27,
			VI_HIST_ON_BIN_55_BIT, VI_HIST_ON_BIN_55_WID);
	vf->prop.hist.vpp_gamma[56]  = rd_bits(0, VI_DNLP_HIST28,
			VI_HIST_ON_BIN_56_BIT, VI_HIST_ON_BIN_56_WID);
	vf->prop.hist.vpp_gamma[57]  = rd_bits(0, VI_DNLP_HIST28,
			VI_HIST_ON_BIN_57_BIT, VI_HIST_ON_BIN_57_WID);
	vf->prop.hist.vpp_gamma[58]  = rd_bits(0, VI_DNLP_HIST29,
			VI_HIST_ON_BIN_58_BIT, VI_HIST_ON_BIN_58_WID);
	vf->prop.hist.vpp_gamma[59]  = rd_bits(0, VI_DNLP_HIST29,
			VI_HIST_ON_BIN_59_BIT, VI_HIST_ON_BIN_59_WID);
	vf->prop.hist.vpp_gamma[60]  = rd_bits(0, VI_DNLP_HIST30,
			VI_HIST_ON_BIN_60_BIT, VI_HIST_ON_BIN_60_WID);
	vf->prop.hist.vpp_gamma[61]  = rd_bits(0, VI_DNLP_HIST30,
			VI_HIST_ON_BIN_61_BIT, VI_HIST_ON_BIN_61_WID);
	vf->prop.hist.vpp_gamma[62]  = rd_bits(0, VI_DNLP_HIST31,
			VI_HIST_ON_BIN_62_BIT, VI_HIST_ON_BIN_62_WID);
	vf->prop.hist.vpp_gamma[63]  = rd_bits(0, VI_DNLP_HIST31,
			VI_HIST_ON_BIN_63_BIT, VI_HIST_ON_BIN_63_WID);
}


void amvecm_video_latch(void)
{
	cm_latch_process();
	amvecm_size_patch();
	ve_dnlp_latch_process();
	ve_lcd_gamma_process();
	lvds_freq_process();
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_g9tv_cpu()) {
		amvecm_3d_sync_process();
		amvecm_3d_black_process();
	}
/* #endif */
}

static struct vframe_s *last_vf;
static int null_vf_cnt;

unsigned int null_vf_max = 5;
module_param(null_vf_max, uint, 0664);
MODULE_PARM_DESC(null_vf_max, "\n null_vf_max\n");
void amvecm_matrix_process(struct vframe_s *vf)
{
	struct vframe_s fake_vframe;

	if (vf == last_vf)
		return;

	if (vf != NULL) {
		vpp_matrix_update(vf);
		last_vf = vf;
		null_vf_cnt = 0;
	} else {
		/* check last signal type */
		if ((last_vf != NULL) &&
			((last_vf->signal_type >> 16) & 0xff) == 9)
			null_vf_cnt++;
		if (((READ_VPP_REG(VPP_MISC) & (1<<10)) == 0)
			&& (null_vf_cnt > null_vf_max)) {
			/* send a faked vframe to switch matrix
			   from 2020 to 601 when video disabled */
			fake_vframe.source_type = VFRAME_SOURCE_TYPE_OTHERS;
			fake_vframe.signal_type = 0;
			fake_vframe.width = 720;
			fake_vframe.height = 480;
			fake_vframe.prop.master_display_colour.present_flag = 0;
			vpp_matrix_update(&fake_vframe);
			last_vf = vf;
			null_vf_cnt = 0;
		}
	}
}

void amvecm_on_vs(struct vframe_s *vf)
{
	if (probe_ok == 0)
		return;
	amvecm_video_latch();

	if (vf != NULL) {
		/* matrix ajust */
		amvecm_matrix_process(vf);

		amvecm_bricon_process(
			vd1_brightness,
			vd1_contrast  + vd1_contrast_offset, vf);
	} else
		amvecm_matrix_process(NULL);

	pq_enable_disable();
}
EXPORT_SYMBOL(amvecm_on_vs);


void refresh_on_vs(struct vframe_s *vf)
{
	if (vf != NULL) {
		vpp_get_vframe_hist_info(vf);
		ve_on_vs(vf);
		vpp_backup_histgram(vf);
	}
}
EXPORT_SYMBOL(refresh_on_vs);

static int amvecm_open(struct inode *inode, struct file *file)
{
	struct amvecm_dev_s *devp;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct amvecm_dev_s, cdev);
	file->private_data = devp;
	return 0;
}

static int amvecm_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
static struct am_regs_s amregs_ext;

static long amvecm_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp;

	pr_amvecm_dbg("[amvecm..] %s: cmd_nr = 0x%x\n",
			__func__, _IOC_NR(cmd));

	if (probe_ok == 0)
		return ret;
	switch (cmd) {
	case AMVECM_IOC_LOAD_REG:
		if (pq_load_en == 0) {
			ret = -EBUSY;
			pr_amvecm_dbg("[amvecm..] pq ioctl function disabled !!\n");
			return ret;
		}

		if ((vecm_latch_flag & FLAG_REG_MAP0) &&
			(vecm_latch_flag & FLAG_REG_MAP1) &&
			(vecm_latch_flag & FLAG_REG_MAP2) &&
			(vecm_latch_flag & FLAG_REG_MAP3) &&
			(vecm_latch_flag & FLAG_REG_MAP4) &&
			(vecm_latch_flag & FLAG_REG_MAP5)) {
			ret = -EBUSY;
			pr_amvecm_dbg("load regs error: loading regs, please wait\n");
			break;
		}
		if (copy_from_user(&amregs_ext,
				(void __user *)arg, sizeof(struct am_regs_s))) {
			pr_amvecm_dbg("0x%x load reg errors: can't get buffer lenght\n",
					FLAG_REG_MAP0);
			ret = -EFAULT;
		} else
			ret = cm_load_reg(&amregs_ext);
		break;
	case AMVECM_IOC_VE_DNLP_EN:
		vecm_latch_flag |= FLAG_VE_DNLP_EN;
		break;
	case AMVECM_IOC_VE_DNLP_DIS:
		vecm_latch_flag |= FLAG_VE_DNLP_DIS;
		break;
	case AMVECM_IOC_VE_DNLP:
		if (copy_from_user(&am_ve_dnlp,
				(void __user *)arg,
				sizeof(struct ve_dnlp_s)))
			ret = -EFAULT;
		else
			ve_dnlp_param_update();
		break;
	case AMVECM_IOC_VE_NEW_DNLP:
		if (copy_from_user(&am_ve_new_dnlp,
				(void __user *)arg,
				sizeof(struct ve_dnlp_table_s)))
			ret = -EFAULT;
		else
			ve_new_dnlp_param_update();
		break;
	case AMVECM_IOC_G_HIST_AVG:
		argp = (void __user *)arg;
		if ((video_ve_hist.height == 0) || (video_ve_hist.width == 0))
			ret = -EFAULT;
		else if (copy_to_user(argp,
					&video_ve_hist,
					sizeof(struct ve_hist_s)))
				ret = -EFAULT;
		break;
	case AMVECM_IOC_G_HIST_BIN:
		argp = (void __user *)arg;
		if (vpp_hist_param.vpp_pixel_sum == 0)
			ret = -EFAULT;
		else if (copy_to_user(argp, &vpp_hist_param,
					sizeof(struct vpp_hist_param_s)))
			ret = -EFAULT;
		break;
	/**********************************************************************
	gamma ioctl
	**********************************************************************/
	case AMVECM_IOC_GAMMA_TABLE_EN:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
		break;
	case AMVECM_IOC_GAMMA_TABLE_DIS:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
		break;
	case AMVECM_IOC_GAMMA_TABLE_R:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_r,
				(void __user *)arg,
				sizeof(struct tcon_gamma_table_s)))
			ret = -EFAULT;
		else
			vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
		break;
	case AMVECM_IOC_GAMMA_TABLE_G:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_g,
				(void __user *)arg,
				sizeof(struct tcon_gamma_table_s)))
			ret = -EFAULT;
		else
			vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
		break;
	case AMVECM_IOC_GAMMA_TABLE_B:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_b,
				(void __user *)arg,
				sizeof(struct tcon_gamma_table_s)))
			ret = -EFAULT;
		else
			vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
		break;
	case AMVECM_IOC_S_RGB_OGO:
		if (!wb_en)
			return -EINVAL;

		if (copy_from_user(&video_rgb_ogo,
				(void __user *)arg,
				sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;
		else
			ve_ogo_param_update();
		break;
	case AMVECM_IOC_G_RGB_OGO:
		if (!wb_en)
			return -EINVAL;

		if (copy_to_user((void __user *)arg,
				&video_rgb_ogo, sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;

		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
#ifdef CONFIG_COMPAT
static long amvecm_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;
	arg = (unsigned long)compat_ptr(arg);
	ret = amvecm_ioctl(file, cmd, arg);
	return ret;
}
#endif
static ssize_t amvecm_dnlp_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n",
			(am_ve_dnlp.en << 28) | (am_ve_dnlp.rt << 24) |
			(am_ve_dnlp.rl << 16) | (am_ve_dnlp.black << 8) |
			(am_ve_dnlp.white << 0));
}
/* [   28] en    0~1 */
/* [27:20] rt    0~16 */
/* [19:16] rl-1  0~15 */
/* [15: 8] black 0~16 */
/* [ 7: 0] white 0~16 */
static ssize_t amvecm_dnlp_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;
	s32 val;

	r = sscanf(buf, "0x%x", &val);
	if ((r != 1) || (vecm_latch_flag & FLAG_VE_DNLP))
		return -EINVAL;
	am_ve_dnlp.en    = (val & 0xf0000000) >> 28;
	am_ve_dnlp.rt    =  (val & 0x0f000000) >> 24;
	am_ve_dnlp.rl    = (val & 0x00ff0000) >> 16;
	am_ve_dnlp.black =  (val & 0x0000ff00) >>  8;
	am_ve_dnlp.white = (val & 0x000000ff) >>  0;
	if (am_ve_dnlp.en >  1)
		am_ve_dnlp.en    =  1;
	if (am_ve_dnlp.rl > 64)
		am_ve_dnlp.rl    = 64;
	if (am_ve_dnlp.black > 16)
		am_ve_dnlp.black = 16;
	if (am_ve_dnlp.white > 16)
		am_ve_dnlp.white = 16;
	vecm_latch_flag |= FLAG_VE_DNLP;
	return count;
}

static ssize_t amvecm_brightness_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vd1_brightness);
}

static ssize_t amvecm_brightness_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d", &val);
	if ((r != 1) || (val < -1024) || (val > 1024))
		return -EINVAL;

	vd1_brightness = val;
	/*vecm_latch_flag |= FLAG_BRI_CON;*/
	vecm_latch_flag |= FLAG_VADJ1_BRI;
	return count;
}

static ssize_t amvecm_contrast_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vd1_contrast);
}

static ssize_t amvecm_contrast_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;
	int val;
	r = sscanf(buf, "%d", &val);
	if ((r != 1) || (val < -1024) || (val > 1024))
		return -EINVAL;

	vd1_contrast = val;
	/*vecm_latch_flag |= FLAG_BRI_CON;*/
	vecm_latch_flag |= FLAG_VADJ1_CON;
	return count;
}

static ssize_t amvecm_saturation_hue_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", READ_VPP_REG(VPP_VADJ1_MA_MB));
}

static ssize_t amvecm_saturation_hue_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	size_t r;
	s32 mab = 0;
	s16 mc = 0, md = 0;

	r = sscanf(buf, "0x%x", &mab);
	if ((r != 1) || (mab&0xfc00fc00))
		return -EINVAL;

	WRITE_VPP_REG(VPP_VADJ1_MA_MB, mab);
	mc = (s16)((mab<<22)>>22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc <  -512)
		mc = -512;
	md = (s16)((mab<<6)>>22);  /* md =  ma; */
	mab = ((mc&0x3ff)<<16)|(md&0x3ff);
	WRITE_VPP_REG(VPP_VADJ1_MC_MD, mab);
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	pr_amvecm_dbg("%s set video_saturation_hue OK!!!\n", __func__);
	return count;
}

static int parse_para_pq(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token)
				|| !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if (len == 0)
			break;
		ret = kstrtoint(token, 0, &res);
		if (ret < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}


static ssize_t amvecm_saturation_hue_pre_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d %d\n", saturation_pre, hue_pre);
}

static ssize_t amvecm_saturation_hue_pre_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int parsed[2];
	int i, ma, mb, mab, mc, md;
	int hue_cos[] = {
			/*0~12*/
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250, 248, 247, 245,
		/*13~25*/
		243, 241, 239, 237, 234, 231, 229, 226, 223, 220, 216, 213, 209
	};
	int hue_sin[] = {
		/*-25~-13*/
		-147, -142, -137, -132, -126, -121, -115, -109, -104,
		 -98,  -92,  -86,  -80,  -74,  -68,  -62,  -56,  -50,
		 -44,  -38,  -31,  -25, -19, -13,  -6,      /*-12~-1*/
		0,  /*0*/
		 /*1~12*/
		6,   13,   19,	25,   31,   38,   44,	50,   56,  62,
		68,  74,   80,   86,   92,	98,  104,  109,  115,  121,
		126,  132, 137, 142, 147 /*13~25*/
	};
	if (likely(parse_para_pq(buf, 2, parsed) != 2))
		return -EINVAL;

	if ((parsed[0] < -128) || (parsed[0] > 128) ||
		(parsed[1] < -25) || (parsed[1] > 25)) {
		return -EINVAL;
	}
	saturation_pre = parsed[0];
	hue_pre = parsed[1];
	i = (hue_pre > 0) ? hue_pre : -hue_pre;
	ma = (hue_cos[i]*(saturation_pre + 128)) >> 7;
	mb = (hue_sin[25+hue_pre]*(saturation_pre + 128)) >> 7;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);
	pr_info("\n[amvideo..] saturation_pre:%d hue_pre:%d mab:%x\n",
			saturation_pre, hue_pre, mab);
	WRITE_VPP_REG(VPP_VADJ2_MA_MB, mab);
	mc = (s16)((mab<<22)>>22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16)((mab<<6)>>22);  /* md =	ma; */
	mab = ((mc&0x3ff)<<16)|(md&0x3ff);
	WRITE_VPP_REG(VPP_VADJ1_MC_MD, mab);
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	return count;
}

static ssize_t amvecm_saturation_hue_post_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d %d\n", saturation_post, hue_post);
}

static ssize_t amvecm_saturation_hue_post_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int parsed[2];
	int i, ma, mb, mab, mc, md;
	int hue_cos[] = {
		/*0~12*/
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250,
		248, 247, 245, 243, 241, 239, 237, 234, 231, 229,
		226, 223, 220, 216, 213, 209  /*13~25*/
	};
	int hue_sin[] = {
		-147, -142, -137, -132, -126, -121, -115, -109, -104,
		-98, -92, -86, -80, /*-25~-13*/-74,  -68,  -62,  -56,
		-50,  -44,  -38,  -31,  -25, -19, -13,  -6, /*-12~-1*/
		0, /*0*/
		6,   13,   19,	25,   31,   38,   44,	50,   56,
		62,	68,  74,      /*1~12*/	80,   86,   92,	98,  104,
		109,  115,  121,  126,  132, 137, 142, 147 /*13~25*/
	};
	if (likely(parse_para_pq(buf, 2, parsed) != 2))
		return -EINVAL;

	if ((parsed[0] < -128) ||
		(parsed[0] > 128) ||
		(parsed[1] < -25) ||
		(parsed[1] > 25)) {
		return -EINVAL;
	}
	saturation_post = parsed[0];
	hue_post = parsed[1];
	i = (hue_post > 0) ? hue_post : -hue_post;
	ma = (hue_cos[i]*(saturation_post + 128)) >> 7;
	mb = (hue_sin[25+hue_post]*(saturation_post + 128)) >> 7;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);
	pr_info("\n[amvideo..] saturation_post:%d hue_post:%d mab:%x\n",
			saturation_post, hue_post, mab);
	WRITE_VPP_REG(VPP_VADJ2_MA_MB, mab);
	mc = (s16)((mab<<22)>>22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16)((mab<<6)>>22);  /* md =	ma; */
	mab = ((mc&0x3ff)<<16)|(md&0x3ff);
	WRITE_VPP_REG(VPP_VADJ2_MC_MD, mab);
	WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);
	return count;
}

static ssize_t amvecm_cm2_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	pr_info("Usage:");
	pr_info(" echo wm addr data0 data1 data2 data3 data4 ");
	pr_info("> /sys/class/amvecm/cm2\n");
	pr_info(" echo rm addr > /sys/class/amvecm/cm2\n");
	return 0;
}

static ssize_t amvecm_cm2_store(struct class *cls,
		 struct class_attribute *attr,
		 const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[7];
	u32 addr;
	int data[5] = {0};
	unsigned int addr_port = VPP_CHROMA_ADDR_PORT;/* 0x1d70; */
	unsigned int data_port = VPP_CHROMA_DATA_PORT;/* 0x1d71; */
	long val;

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, " \n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if ((parm[0][0] == 'w') && parm[0][1] == 'm') {
		if (n != 7) {
			pr_info("read: invalid parameter\n");
			pr_info("please: cat /sys/class/amvecm/cm2\n");
			kfree(buf_orig);
			return count;
		}
		if (kstrtol(parm[1], 16, &val) < 0)
			return -EINVAL;
		addr = val;
		addr = addr - addr%8;
		if (kstrtol(parm[2], 16, &val) < 0)
			return -EINVAL;
		data[0] = val;
		if (kstrtol(parm[3], 16, &val) < 0)
			return -EINVAL;
		data[1] = val;
		if (kstrtol(parm[4], 16, &val) < 0)
			return -EINVAL;
		data[2] = val;
		if (kstrtol(parm[5], 16, &val) < 0)
			return -EINVAL;
		data[3] = val;
		if (kstrtol(parm[6], 16, &val) < 0)
			return -EINVAL;
		data[4] = val;
		WRITE_VPP_REG(addr_port, addr);
		WRITE_VPP_REG(data_port, data[0]);
		WRITE_VPP_REG(addr_port, addr + 1);
		WRITE_VPP_REG(data_port, data[1]);
		WRITE_VPP_REG(addr_port, addr + 2);
		WRITE_VPP_REG(data_port, data[2]);
		WRITE_VPP_REG(addr_port, addr + 3);
		WRITE_VPP_REG(data_port, data[3]);
		WRITE_VPP_REG(addr_port, addr + 4);
		WRITE_VPP_REG(data_port, data[4]);
		pr_info("wm: [0x%x] <-- 0x0\n", addr);
	} else if ((parm[0][0] == 'r') && parm[0][1] == 'm') {
		if (n != 2) {
			pr_info("read: invalid parameter\n");
			pr_info("please: cat /sys/class/amvecm/cm2\n");
			kfree(buf_orig);
			return count;
		}
		if (kstrtol(parm[1], 16, &val) < 0)
			return -EINVAL;
		addr = val;
		addr = addr - addr%8;
		WRITE_VPP_REG(addr_port, addr);
		data[0] = READ_VPP_REG(data_port);
		data[0] = READ_VPP_REG(data_port);
		data[0] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+1);
		data[1] = READ_VPP_REG(data_port);
		data[1] = READ_VPP_REG(data_port);
		data[1] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+2);
		data[2] = READ_VPP_REG(data_port);
		data[2] = READ_VPP_REG(data_port);
		data[2] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+3);
		data[3] = READ_VPP_REG(data_port);
		data[3] = READ_VPP_REG(data_port);
		data[3] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr+4);
		data[4] = READ_VPP_REG(data_port);
		data[4] = READ_VPP_REG(data_port);
		data[4] = READ_VPP_REG(data_port);
		pr_info("rm:[0x%x]-->[0x%x][0x%x][0x%x][0x%x][0x%x]\n",
				addr, data[0], data[1],
				data[2], data[3], data[4]);
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/bit");
	}
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_pq_en_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int len = 0;
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	int sharpness_en_val = 0, gamma_en_val = 0;
	sharpness_en_val = READ_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 1);
	gamma_en_val = READ_VPP_REG_BITS(L_GAMMA_CNTL_PORT, GAMMA_EN, 1);
/* #endif */
	len += sprintf(buf+len, "dnlp_en = %d\n", dnlp_en);
	len += sprintf(buf+len, "cm_en = %d\n", cm_en);
	len += sprintf(buf+len, "wb_en = %d\n", wb_en);
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_gxtvbb_cpu()) {
		len += sprintf(buf+len,
				"sharpness_en = %d\n", sharpness_en_val);
		len += sprintf(buf+len,
				"gamma_en = %d\n", gamma_en_val);
	}
/* #endif */
	return len;
}

static ssize_t amvecm_pq_en_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;
	r = sscanf(buf, "%d", &val);
	if ((r != 1) || ((val != 1) && (val != 0)))
		return -EINVAL;

	if (val == 1) {
		pq_on_off = 1;
		pr_amvecm_dbg("dnlp_en = 1 [0x1da1][bit2] = 1\n");
		pr_amvecm_dbg("cm_en = 1  [0x1d26][bit28] = 1\n");
		pr_amvecm_dbg("sharpness0_en = 1 [0x3227][bit1] = 1\n");
		pr_amvecm_dbg("sharpness1_en = 1 [0x32a7][bit1] = 1\n");
		pr_amvecm_dbg("wb_en = 1 [0x1d6a][bit31] = 1\n");
		pr_amvecm_dbg("gamma_en = 1 [0x1400][bit0] = 1\n");
	} else {
		pq_on_off = 0;
		pr_amvecm_dbg("dnlp_en = 0 [0x1da1][bit2] = 0\n");
		pr_amvecm_dbg("cm_en = 0  [0x1d26][bit28] = 0\n");
		pr_amvecm_dbg("sharpness0_en = 0 [0x3227][bit1] = 0\n");
		pr_amvecm_dbg("sharpness1_en = 0 [0x32a7][bit1] = 0\n");
		pr_amvecm_dbg("wb_en = 0 [0x1d6a][bit31] = 0\n");
		pr_amvecm_dbg("gamma_en = 0 [0x1400][bit0] = 0\n");
	}
	return count;
}

static ssize_t amvecm_cm_en_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "cm_en = %d\n", cm_en);
}

static ssize_t amvecm_cm_en_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;
	r = sscanf(buf, "%d", &val);
	if ((r != 1) || ((val != 1) && (val != 0)))
		return -EINVAL;

	if (val == 1)
		cm_on_off = 1;
	else
		cm_on_off = 0;
	return count;

}

static ssize_t amvecm_dnlp_en_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "dnlp_en = %d\n", dnlp_en);
}

static ssize_t amvecm_dnlp_en_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;
	r = sscanf(buf, "%d", &val);
	if ((r != 1) || ((val != 1) && (val != 0)))
		return -EINVAL;

	if (val == 1)
		dnlp_on_off = 1;
	else
		dnlp_on_off = 0;
	return count;

}

static ssize_t amvecm_sharpness_en_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int val = READ_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 1);

	return sprintf(buf, "sharpness_en = %d\n", val);
}

static ssize_t amvecm_sharpness_en_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;
	r = sscanf(buf, "%d", &val);
	if ((r != 1) || ((val != 1) && (val != 0)))
		return -EINVAL;

	if (val == 1)
		sharpness_on_off = 1;
	else
		sharpness_on_off = 0;
	return count;

}

static ssize_t amvecm_gamma_en_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int val = READ_VPP_REG_BITS(L_GAMMA_CNTL_PORT, GAMMA_EN, 1);

	return sprintf(buf, "gamma_en = %d\n", val);
}

static ssize_t amvecm_gamma_en_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf,
		size_t count)
{
	size_t r;
	int val;
	r = sscanf(buf, "%d", &val);
	if ((r != 1) || ((val != 1) && (val != 0)))
		return -EINVAL;

	if (val == 1)
		vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;	/* gamma off */
	else
		vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;	/* gamma off */
	return count;

}

static ssize_t amvecm_wb_en_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int val = READ_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 31, 1);

	return sprintf(buf, "sharpness_en = %d\n", val);
}

static ssize_t amvecm_wb_en_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;
	r = sscanf(buf, "%d", &val);
	if ((r != 1) || ((val != 1) && (val != 0)))
		return -EINVAL;

	if (val == 1)
		wb_on_off = 1;
	else
		wb_on_off = 0;
	return count;

}


static ssize_t amvecm_gamma_show(struct class *cls,
			struct class_attribute *attr,
			char *buf)
{
	pr_info("Usage:");
	pr_info("	echo sgr|sgg|sgb xxx...xx > /sys/class/amvecm/gamma\n");
	pr_info("Notes:");
	pr_info("	if the string xxx......xx is less than 256*3,");
	pr_info("	then the remaining will be set value 0\n");
	pr_info("	if the string xxx......xx is more than 256*3, ");
	pr_info("	then the remaining will be ignored\n");
	return 0;
}

static ssize_t amvecm_gamma_store(struct class *cls,
			struct class_attribute *attr,
			const char *buffer, size_t count)
{

	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4];
	unsigned short *gammaR, *gammaG, *gammaB;
	unsigned int gamma_count;
	char gamma[4];
	int i = 0;
	long val;

	/* to avoid the bellow warning message while compiling:
	 * warning: the frame size of 1576 bytes is larger than 1024 bytes
	 */
	gammaR = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);
	gammaG = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);
	gammaB = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, " \n");
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if ((parm[0][0] == 's') && (parm[0][1] == 'g')) {
		memset(gammaR, 0, 256 * sizeof(unsigned short));
		gamma_count = (strlen(parm[1]) + 2) / 3;
		if (gamma_count > 256)
			gamma_count = 256;

		for (i = 0; i < gamma_count; ++i) {
			gamma[0] = parm[1][3 * i + 0];
			gamma[1] = parm[1][3 * i + 1];
			gamma[2] = parm[1][3 * i + 2];
			gamma[3] = '\0';
			if (kstrtol(gamma, 16, &val) < 0)
				return -EINVAL;
			gammaR[i] = val;

		}

		switch (parm[0][2]) {
		case 'r':
			vpp_set_lcd_gamma_table(gammaR, H_SEL_R);
			break;

		case 'g':
			vpp_set_lcd_gamma_table(gammaR, H_SEL_G);
			break;

		case 'b':
			vpp_set_lcd_gamma_table(gammaR, H_SEL_B);
			break;
		default:
			break;
		}
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/gamma");

	}
	kfree(buf_orig);
	kfree(gammaR);
	kfree(gammaG);
	kfree(gammaB);
	return count;
}

static ssize_t amvecm_set_post_matrix_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", (int)(READ_VPP_REG(VPP_MATRIX_CTRL)));
}
static ssize_t amvecm_set_post_matrix_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;
	r = sscanf(buf, "0x%x", &val);
	if ((r != 1)  || (val & 0xffff0000))
		return -EINVAL;

	WRITE_VPP_REG(VPP_MATRIX_CTRL, val);
	return count;
}

static ssize_t amvecm_post_matrix_pos_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n",
			(int)(READ_VPP_REG(VPP_MATRIX_PROBE_POS)));
}
static ssize_t amvecm_post_matrix_pos_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;
	r = sscanf(buf, "0x%x", &val);
	if ((r != 1)  || (val & 0xf000f000))
		return -EINVAL;

	WRITE_VPP_REG(VPP_MATRIX_PROBE_POS, val);
	return count;
}

static ssize_t amvecm_post_matrix_data_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int len = 0 , val1 = 0, val2 = 0;
	val1 = READ_VPP_REG(VPP_MATRIX_PROBE_COLOR);
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	val2 = READ_VPP_REG(VPP_MATRIX_PROBE_COLOR1);
/* #endif */
	len += sprintf(buf+len, "VPP_MATRIX_PROBE_COLOR %x\n", val1);
	len += sprintf(buf+len, "VPP_MATRIX_PROBE_COLOR %x\n", val2);
	return len;
}

static ssize_t amvecm_post_matrix_data_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_sr1_reg_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	unsigned int addr;
	addr = ((sr1_index+0x3280) << 2) | 0xd0100000;
	return sprintf(buf, "0x%x = 0x%x\n",
			addr, sr1_ret_val[sr1_index]);
}

static ssize_t amvecm_sr1_reg_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	unsigned int addr, off_addr = 0;
	r = sscanf(buf, "0x%x", &addr);
	addr = (addr&0xffff) >> 2;
	if ((r != 1)  || (addr > 0x32e4) || (addr < 0x3280))
		return -EINVAL;
	off_addr = addr - 0x3280;
	sr1_index = off_addr;
	sr1_ret_val[off_addr] = sr1_reg_val[off_addr];

	return count;

}

static ssize_t amvecm_write_sr1_reg_val_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t amvecm_write_sr1_reg_val_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	unsigned int val;
	r = sscanf(buf, "0x%x", &val);
	if (r != 1)
		return -EINVAL;
	sr1_reg_val[sr1_index] = val;

	return count;

}

static ssize_t amvecm_dump_reg_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	unsigned int addr;

	pr_info("----dump sharpness0 reg----\n");
	for (addr = 0x3200;
		addr <= 0x3264; addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump sharpness1 reg----\n");
	for (addr = (0x3200+0x80);
		addr <= (0x3264+0x80); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump vd1 IF0 reg----\n");
	for (addr = (0x1a50);
		addr <= (0x1a69); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump vpp1 part1 reg----\n");
	for (addr = (0x1d00);
		addr <= (0x1d6e); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));

	pr_info("----dump vpp1 part2 reg----\n");
	for (addr = (0x1d72);
		addr <= (0x1de4); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));

	pr_info("----dump ndr reg----\n");
	for (addr = (0x2d00);
		addr <= (0x2d78); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump nr3 reg----\n");
	for (addr = (0x2ff0);
		addr <= (0x2ff6); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump vlock reg----\n");
	for (addr = (0x3000);
		addr <= (0x3020); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump super scaler0 reg----\n");
	for (addr = (0x3100);
		addr <= (0x3115); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump super scaler1 reg----\n");
	for (addr = (0x3118);
		addr <= (0x312e); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump xvycc reg----\n");
	for (addr = (0x3158);
		addr <= (0x3179); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(0xd0100000+(addr<<2)), addr,
				READ_VPP_REG(addr));
	pr_info("----dump reg done----\n");
	return 0;
}
static ssize_t amvecm_dump_reg_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	return 0;
}
static ssize_t amvecm_dump_vpp_hist_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	vdin_dump_histgram();
	return 0;
}

static ssize_t amvecm_dump_vpp_hist_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_hdr_dbg_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int i, j;

	if (dbg_vf == NULL)
		goto hdr_dump;
	pr_info("\n----vframe info----\n");
	pr_info("vframe:%p\n", dbg_vf);
	pr_info("index:%d, type:0x%x, type_backup:0x%x, blend_mode:%d\n",
			dbg_vf->index, dbg_vf->type,
			dbg_vf->type_backup, dbg_vf->blend_mode);
	pr_info("duration:%d, duration_pulldown:%d, pts:%d, flag:0x%x\n",
			dbg_vf->duration, dbg_vf->duration_pulldown,
			dbg_vf->pts, dbg_vf->flag);
	pr_info("canvas0Addr:0x%x, canvas1Addr:0x%x, bufWidth:%d\n",
			dbg_vf->canvas0Addr, dbg_vf->canvas1Addr,
			dbg_vf->bufWidth);
	pr_info("width:%d, height:%d, ratio_control:0x%x, bitdepth:%d\n",
			dbg_vf->width, dbg_vf->height,
			dbg_vf->ratio_control, dbg_vf->bitdepth);
	pr_info("signal_type:%x, orientation:%d, video_angle:0x%x\n",
			dbg_vf->signal_type, dbg_vf->orientation,
			dbg_vf->video_angle);
	pr_info("source_type:%d, phase:%d, soruce_mode:%d, sig_fmt:0x%x\n",
			dbg_vf->source_type, dbg_vf->phase,
			dbg_vf->source_mode, dbg_vf->sig_fmt);
	/*
	pr_info(
		"trans_fmt 0x%x, lefteye(%d %d %d %d), righteye(%d %d %d %d)\n",
		vf->trans_fmt, vf->left_eye.start_x, vf->left_eye.start_y,
		vf->left_eye.width, vf->left_eye.height,
		vf->right_eye.start_x, vf->right_eye.start_y,
		vf->right_eye.width, vf->right_eye.height);
	pr_info("mode_3d_enable %d",
		vf->mode_3d_enable);
	pr_info("early_process_fun 0x%p, process_fun 0x%p,
	private_data %p\n",
		vf->early_process_fun, vf->process_fun, vf->private_data);

	pr_info("hist_pow %d, luma_sum %d, chroma_sum %d, pixel_sum %d\n",
		vf->prop.hist.hist_pow, vf->prop.hist.luma_sum,
		vf->prop.hist.chroma_sum, vf->prop.hist.pixel_sum);

	pr_info("height %d, width %d, luma_max %d, luma_min %d\n",
		vf->prop.hist.hist_pow, vf->prop.hist.hist_pow,
		vf->prop.hist.hist_pow, vf->prop.hist.hist_pow);

	pr_info("vpp_luma_sum %d, vpp_chroma_sum %d, vpp_pixel_sum %d\n",
		vf->prop.hist.vpp_luma_sum, vf->prop.hist.vpp_chroma_sum,
		vf->prop.hist.vpp_pixel_sum);

	pr_info("vpp_height %d, vpp_width %d, vpp_luma_max %d,
	 vpp_luma_min %d\n",
		vf->prop.hist.vpp_height, vf->prop.hist.vpp_width,
		vf->prop.hist.vpp_luma_max, vf->prop.hist.vpp_luma_min);

	pr_info("vs_span_cnt %d, vs_cnt %d, hs_cnt0 %d, hs_cnt1 %d\n",
		vf->prop.meas.vs_span_cnt, vf->prop.meas.vs_cnt,
		vf->prop.meas.hs_cnt0, vf->prop.meas.hs_cnt1);

	pr_info("hs_cnt2 %d, vs_cnt %d, hs_cnt3 %d,
	 vs_cycle %d, vs_stamp %d\n",
		vf->prop.meas.hs_cnt2, vf->prop.meas.hs_cnt3,
		vf->prop.meas.vs_cycle, vf->prop.meas.vs_stamp);
	*/
	pr_info("\t\tdisplay colour present_flag:%d\n",
		dbg_vf->prop.master_display_colour.present_flag);
	for (i = 0; i < 3; i++)
		for (j = 0; j < 2; j++)
			pr_info(
				"\t\t primaries[%1d][%1d] = %04x\n",
			i, j,
			dbg_vf->prop.master_display_colour.primaries[i][j]);
	pr_info("\t\twhite_point = (%04x, %04x)\n",
			dbg_vf->prop.master_display_colour.white_point[0],
			dbg_vf->prop.master_display_colour.white_point[1]);
	pr_info("\t\tmax,min luminance = %08x, %08x\n",
			dbg_vf->prop.master_display_colour.luminance[0],
			dbg_vf->prop.master_display_colour.luminance[1]);

	pr_info("pixel_ratio:%d list:%p\n",
			dbg_vf->pixel_ratio, &dbg_vf->list);

	pr_info("ready_jiffies64:%lld, frame_dirty %d\n",
			dbg_vf->ready_jiffies64, dbg_vf->frame_dirty);
hdr_dump:
	pr_info("\n-------HDR process info-------\n");

	pr_info("hdr_mode:%d, hdr_process_mode:%d, force_csc_type:0x%x\n",
			hdr_mode, hdr_process_mode, force_csc_type);
	pr_info("cur_signal_type:0x%x, cur_csc_mode:0x%x, cur_csc_type:0x%x\n",
			cur_signal_type, cur_csc_mode, cur_csc_type);

	pr_info("knee_lut_on:%d,knee_interpolation_mode:%d,cur_knee_factor:%d\n",
			knee_lut_on, knee_interpolation_mode, cur_knee_factor);

	pr_info("\n-------Tx HDR process info-------\n");
	pr_info("hdr_support:0x%x,lumi_max:%d,lumi_avg:%d,lumi_min:%d\n",
			receiver_hdr_info.hdr_support,
			receiver_hdr_info.lumi_max,
			receiver_hdr_info.lumi_avg,
			receiver_hdr_info.lumi_min);

	pr_info("\n----Send master display info----\n");
	for (i = 0; i < 3; i++)
		for (j = 0; j < 2; j++)
			pr_info(
				"\t\t primaries[%1d][%1d] = %04x\n",
				i, j,
				dbg_hdr_send.primaries[i][j]);
	pr_info("\t\twhite_point = (%04x, %04x)\n",
			dbg_hdr_send.white_point[0],
			dbg_hdr_send.white_point[1]);
	pr_info("\t\tmax,min luminance = %08x, %08x\n",
			dbg_hdr_send.luminance[0], dbg_hdr_send.luminance[1]);

	return 0;
}

static ssize_t amvecm_hdr_dbg_store(struct class *cla,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	return 0;
}
/* #if (MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV) */
void init_sharpness(void)
{
	/*probe close sr0 peaking for switch on video*/
	WRITE_VPP_REG_BITS(VPP_SRSHARP0_CTRL, 1, 0, 1);
	/*WRITE_VPP_REG_BITS(VPP_SRSHARP1_CTRL, 1,0,1);*/
	WRITE_VPP_REG_BITS(SRSHARP0_SHARP_PK_NR_ENABLE, 0, 1, 1);
/* WRITE_VPP_REG_BITS(SRSHARP1_SHARP_PK_NR_ENABLE, 0,1,1);*/

/* WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1,1,1); */
/* WRITE_VPP_REG(NR_GAUSSIAN_MODE, 0x0); */
/* WRITE_VPP_REG(PK_HVCON_LPF_MODE, 0x11111111); */
/* WRITE_VPP_REG(PK_CON_2CIRHPGAIN_LIMIT, 0x05600500); */
/* WRITE_VPP_REG(PK_CON_2CIRBPGAIN_LIMIT, 0x05280500); */
/* WRITE_VPP_REG(PK_CON_2DRTHPGAIN_LIMIT, 0x05600500); */
/* WRITE_VPP_REG(PK_CON_2DRTBPGAIN_LIMIT, 0x05280500); */
/*  */
/* WRITE_VPP_REG(PK_CIRFB_BLEND_GAIN, 0x8f808f80); */
/* WRITE_VPP_REG(NR_ALP0_MIN_MAX, 0x003f003f); */
/* WRITE_VPP_REG(PK_ALP2_MIERR_CORING, 0x00010101); */
/* WRITE_VPP_REG(PK_ALP2_ERR2CURV_TH_RATE, 0x50504010); */
/* WRITE_VPP_REG(PK_FINALGAIN_HP_BP, 0x00002820); */
/* WRITE_VPP_REG(PK_OS_STATIC, 0x22014014); */
/* WRITE_VPP_REG(PK_DRT_SAD_MISC, 0x18180418); */
/* WRITE_VPP_REG(NR_TI_DNLP_BLEND, 0x00000406); */
/* WRITE_VPP_REG(LTI_CTI_DF_GAIN, 0x18181818); */
/* WRITE_VPP_REG(LTI_CTI_DIR_AC_DBG, 0x57ff0000); */
/* WRITE_VPP_REG(HCTI_FLT_CLP_DC, 0x1a555310); */
/* WRITE_VPP_REG(HCTI_BST_CORE, 0x05050503); */
/* WRITE_VPP_REG(HCTI_CON_2_GAIN_0, 0x28193c00); */
/* WRITE_VPP_REG(HLTI_FLT_CLP_DC, 0x19552104); */
/* WRITE_VPP_REG(HLTI_BST_GAIN, 0x20201c0c); */
/* WRITE_VPP_REG(HLTI_CON_2_GAIN_0, 0x24193c5a); */
/* WRITE_VPP_REG(VLTI_FLT_CON_CLP, 0x00006a90); */
/* WRITE_VPP_REG(VLTI_CON_2_GAIN_0, 0x193c0560); */
/* WRITE_VPP_REG(VCTI_FLT_CON_CLP, 0x00006a90); */
/* WRITE_VPP_REG(VCTI_BST_GAIN, 0x00101010); */
/* WRITE_VPP_REG(VCTI_BST_CORE, 0x00050503); */
/* WRITE_VPP_REG(PK_CIRFB_BP_CORING, 0x00043f04); */
/* WRITE_VPP_REG(PK_DRTFB_HP_CORING, 0x00043f04); */
/* WRITE_VPP_REG(SHARP_HVBLANK_NUM, 0x00003c3c); */
/* pr_info("**********sharpness init ok!*********\n"); */
}
/* #endif*/

static void amvecm_gamma_init(bool en)
{
	unsigned int i;
	unsigned short data[256];

	if (en) {
		for (i = 0; i < 256; i++)
			data[i] = i << 2;
		vpp_set_lcd_gamma_table(
					data,
					H_SEL_R);
		vpp_set_lcd_gamma_table(
					data,
					H_SEL_G);
		vpp_set_lcd_gamma_table(
					data,
					H_SEL_B);
	}
	WRITE_VPP_REG_BITS(L_GAMMA_CNTL_PORT,
			en, GAMMA_EN, 1);
}
static void amvecm_wb_init(bool en)
{
	if (en) {
		WRITE_VPP_REG(VPP_GAINOFF_CTRL0,
			(1024 << 16) | 1024);
		WRITE_VPP_REG(VPP_GAINOFF_CTRL1,
			(1024 << 16));
	}

	WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, en, 31, 1);
}

static struct class_attribute amvecm_class_attrs[] = {
	__ATTR(dnlp, S_IRUGO | S_IWUSR,
		amvecm_dnlp_show, amvecm_dnlp_store),
	__ATTR(brightness, S_IRUGO | S_IWUSR,
		amvecm_brightness_show, amvecm_brightness_store),
	__ATTR(contrast, S_IRUGO | S_IWUSR,
		amvecm_contrast_show, amvecm_contrast_store),
	__ATTR(saturation_hue, S_IRUGO | S_IWUSR,
		amvecm_saturation_hue_show,
		amvecm_saturation_hue_store),
	__ATTR(saturation_hue_pre, S_IRUGO | S_IWUSR,
		amvecm_saturation_hue_pre_show,
		amvecm_saturation_hue_pre_store),
	__ATTR(saturation_hue_post, S_IRUGO | S_IWUSR,
		amvecm_saturation_hue_post_show,
		amvecm_saturation_hue_post_store),
	__ATTR(cm2, S_IRUGO | S_IWUSR,
		amvecm_cm2_show,
		amvecm_cm2_store),
	__ATTR(gamma, S_IRUGO | S_IWUSR,
		amvecm_gamma_show,
		amvecm_gamma_store),
	__ATTR(brightness1, S_IRUGO | S_IWUSR,
		video_adj1_brightness_show,
		video_adj1_brightness_store),
	__ATTR(contrast1, S_IRUGO | S_IWUSR,
		video_adj1_contrast_show, video_adj1_contrast_store),
	__ATTR(brightness2, S_IRUGO | S_IWUSR,
		video_adj2_brightness_show, video_adj2_brightness_store),
	__ATTR(contrast2, S_IRUGO | S_IWUSR,
		video_adj2_contrast_show, video_adj2_contrast_store),
	__ATTR(help, S_IRUGO | S_IWUSR,
		amvecm_usage_show, NULL),
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	__ATTR(sync_3d, S_IRUGO | S_IWUSR,
		amvecm_3d_sync_show,
		amvecm_3d_sync_store),
	__ATTR(sharpness_on_off, S_IRUGO | S_IWUSR,
		amvecm_sharpness_en_show, amvecm_sharpness_en_store),
	__ATTR(gamma_on_off, S_IRUGO | S_IWUSR,
		amvecm_gamma_en_show, amvecm_gamma_en_store),
	__ATTR(wb_on_off, S_IRUGO | S_IWUSR,
		amvecm_wb_en_show, amvecm_wb_en_store),
/* #endif */
	__ATTR(pq_on_off, S_IRUGO | S_IWUSR,
		amvecm_pq_en_show, amvecm_pq_en_store),
	__ATTR(cm_on_off, S_IRUGO | S_IWUSR,
		amvecm_cm_en_show, amvecm_cm_en_store),
	__ATTR(dnlp_on_off, S_IRUGO | S_IWUSR,
		amvecm_dnlp_en_show, amvecm_dnlp_en_store),
	__ATTR(matrix_set, S_IRUGO | S_IWUSR,
		amvecm_set_post_matrix_show, amvecm_set_post_matrix_store),
	__ATTR(matrix_pos, S_IRUGO | S_IWUSR,
		amvecm_post_matrix_pos_show, amvecm_post_matrix_pos_store),
	__ATTR(matrix_data, S_IRUGO | S_IWUSR,
		amvecm_post_matrix_data_show, amvecm_post_matrix_data_store),
	__ATTR(dump_reg, S_IRUGO | S_IWUSR,
		amvecm_dump_reg_show, amvecm_dump_reg_store),
	__ATTR(sr1_reg, S_IRUGO | S_IWUSR,
		amvecm_sr1_reg_show, amvecm_sr1_reg_store),
	__ATTR(write_sr1_reg_val, S_IRUGO | S_IWUSR,
		amvecm_write_sr1_reg_val_show, amvecm_write_sr1_reg_val_store),
	__ATTR(dump_vpp_hist, S_IRUGO | S_IWUSR,
		amvecm_dump_vpp_hist_show, amvecm_dump_vpp_hist_store),
	__ATTR(hdr_dbg, S_IRUGO | S_IWUSR,
			amvecm_hdr_dbg_show, amvecm_hdr_dbg_store),
	__ATTR_NULL
};

static const struct file_operations amvecm_fops = {
	.owner   = THIS_MODULE,
	.open    = amvecm_open,
	.release = amvecm_release,
	.unlocked_ioctl   = amvecm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amvecm_compat_ioctl,
#endif
};
static void aml_vecm_dt_parse(struct platform_device *pdev)
{
	struct device_node *node;
	unsigned int val;
	int ret;
	node = pdev->dev.of_node;
	/* get interger value */
	if (node) {
		ret = of_property_read_u32(node, "gamma_en", &val);
		if (ret)
			pr_info("Can't find  gamma_en.\n");
		else
			gamma_en = val;
		ret = of_property_read_u32(node, "wb_en", &val);
		if (ret)
			pr_info("Can't find  wb_en.\n");
		else
			wb_en = val;
		ret = of_property_read_u32(node, "cm_en", &val);
		if (ret)
			pr_info("Can't find  cm_en.\n");
		else
			cm_en = val;
	}
	/* init module status */
	amvecm_wb_init(wb_en);
	amvecm_gamma_init(gamma_en);
	WRITE_VPP_REG_BITS(VPP_MISC, cm_en, 28, 1);
}

static int aml_vecm_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct amvecm_dev_s *devp = &amvecm_dev;

	memset(devp, 0, (sizeof(struct amvecm_dev_s)));
	pr_info("\n VECM probe start\n");
	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMVECM_NAME);
	if (ret < 0)
		goto fail_alloc_region;

	devp->clsp = class_create(THIS_MODULE, AMVECM_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}
	for (i = 0; amvecm_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->clsp, &amvecm_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}
	cdev_init(&devp->cdev, &amvecm_fops);
	devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devp->cdev, devp->devno, 1);
	if (ret)
		goto fail_add_cdev;

	devp->dev = device_create(devp->clsp, NULL, devp->devno,
			NULL, AMVECM_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}
	/* #if (MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV) */
	if (is_meson_gxtvbb_cpu())
		init_sharpness();
	/* #endif */
	vpp_get_hist_en();

	memset(&vpp_hist_param.vpp_histgram[0],
		0, sizeof(unsigned short) * 64);

	aml_vecm_dt_parse(pdev);
	probe_ok = 1;
	pr_info("%s: ok\n", __func__);
	return 0;

fail_create_device:
	pr_info("[amvecm.] : amvecm device create error.\n");
	cdev_del(&devp->cdev);
fail_add_cdev:
	pr_info("[amvecm.] : amvecm add device error.\n");
	kfree(devp);
fail_class_create_file:
	pr_info("[amvecm.] : amvecm class create file error.\n");
	for (i = 0; amvecm_class_attrs[i].attr.name; i++) {
		class_remove_file(devp->clsp,
		&amvecm_class_attrs[i]);
	}
	class_destroy(devp->clsp);
fail_create_class:
	pr_info("[amvecm.] : amvecm class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
fail_alloc_region:
	pr_info("[amvecm.] : amvecm alloc error.\n");
	pr_info("[amvecm.] : amvecm_init.\n");
	return ret;
}

static int __exit aml_vecm_remove(struct platform_device *pdev)
{
	struct amvecm_dev_s *devp = &amvecm_dev;
	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
	kfree(devp);
	probe_ok = 0;
	pr_info("[amvecm.] : amvecm_exit.\n");
	return 0;
}

#ifdef CONFIG_PM
static int amvecm_drv_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	if (probe_ok == 1)
		probe_ok = 0;
	pr_info("amvecm: suspend module\n");
	return 0;
}

static int amvecm_drv_resume(struct platform_device *pdev)
{
	if (probe_ok == 0)
		probe_ok = 1;

	pr_info("amvecm: resume module\n");
	return 0;
}
#endif


static const struct of_device_id aml_vecm_dt_match[] = {
	{
		.compatible = "amlogic, vecm",
	},
	{},
};

static struct platform_driver aml_vecm_driver = {
	.driver = {
		.name = "aml_vecm",
		.owner = THIS_MODULE,
		.of_match_table = aml_vecm_dt_match,
	},
	.probe = aml_vecm_probe,
	.remove = __exit_p(aml_vecm_remove),
#ifdef CONFIG_PM
	.suspend    = amvecm_drv_suspend,
	.resume     = amvecm_drv_resume,
#endif

};

static int __init aml_vecm_init(void)
{
	pr_info("module init\n");
	/* remap the hiu bus */
	amvecm_hiu_reg_base = ioremap(0xc883c000, 0x2000);
	if (platform_driver_register(&aml_vecm_driver)) {
		pr_err("failed to register bl driver module\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit aml_vecm_exit(void)
{
	pr_info("module exit\n");
	platform_driver_unregister(&aml_vecm_driver);
}

module_init(aml_vecm_init);
module_exit(aml_vecm_exit);

MODULE_DESCRIPTION("AMLOGIC amvecm driver");
MODULE_LICENSE("GPL");

