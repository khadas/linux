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
#include <linux/errno.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amvecm/amvecm.h>
#include <linux/amlogic/vout/vout_notify.h>
#include "arch/vpp_regs.h"
#include "../tvin/tvin_global.h"
#include "arch/vpp_hdr_regs.h"

#include "amcsc.h"

#define pr_csc(fmt, args...)\
	do {\
		if (debug_csc)\
			pr_info("AMCSC: " fmt, ## args);\
	} while (0)

signed int vd1_contrast_offset;

static struct vframe_s *dbg_vf;
static struct master_display_info_s dbg_hdr_send;
static struct hdr_info receiver_hdr_info;

static bool debug_csc;
module_param(debug_csc, bool, 0664);
MODULE_PARM_DESC(debug_csc, "\n debug_csc\n");

static bool skip_csc_en;
module_param(skip_csc_en, bool, 0664);
MODULE_PARM_DESC(skip_csc_en, "\n skip_csc_en\n");

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
		pr_csc("Knee_factor changed from %d to %d\n",
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
					pr_csc("xvycc_lut[%1d][%3d] = 0x%03x\n",
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
					pr_csc("xvycc_lut[%1d][%3d] = 0x%03x\n",
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
					pr_csc("xvycc_lut[%1d][%3d] = 0x%03x\n",
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

/***************************** gxl hdr ****************************/

#define EOTF_LUT_SIZE 33
static unsigned int num_osd_eotf_r_mapping = EOTF_LUT_SIZE;
static unsigned int osd_eotf_r_mapping[EOTF_LUT_SIZE] = {
	0x0000,	0x0200,	0x0400, 0x0600,
	0x0800, 0x0a00, 0x0c00, 0x0e00,
	0x1000, 0x1200, 0x1400, 0x1600,
	0x1800, 0x1a00, 0x1c00, 0x1e00,
	0x2000, 0x2200, 0x2400, 0x2600,
	0x2800, 0x2a00, 0x2c00, 0x2e00,
	0x3000, 0x3200, 0x3400, 0x3600,
	0x3800, 0x3a00, 0x3c00, 0x3e00,
	0x4000
};

static unsigned int num_osd_eotf_g_mapping = EOTF_LUT_SIZE;
static unsigned int osd_eotf_g_mapping[EOTF_LUT_SIZE] = {
	0x0000,	0x0200,	0x0400, 0x0600,
	0x0800, 0x0a00, 0x0c00, 0x0e00,
	0x1000, 0x1200, 0x1400, 0x1600,
	0x1800, 0x1a00, 0x1c00, 0x1e00,
	0x2000, 0x2200, 0x2400, 0x2600,
	0x2800, 0x2a00, 0x2c00, 0x2e00,
	0x3000, 0x3200, 0x3400, 0x3600,
	0x3800, 0x3a00, 0x3c00, 0x3e00,
	0x4000
};

static unsigned int num_osd_eotf_b_mapping = EOTF_LUT_SIZE;
static unsigned int osd_eotf_b_mapping[EOTF_LUT_SIZE] = {
	0x0000,	0x0200,	0x0400, 0x0600,
	0x0800, 0x0a00, 0x0c00, 0x0e00,
	0x1000, 0x1200, 0x1400, 0x1600,
	0x1800, 0x1a00, 0x1c00, 0x1e00,
	0x2000, 0x2200, 0x2400, 0x2600,
	0x2800, 0x2a00, 0x2c00, 0x2e00,
	0x3000, 0x3200, 0x3400, 0x3600,
	0x3800, 0x3a00, 0x3c00, 0x3e00,
	0x4000
};

static unsigned int num_video_eotf_r_mapping = EOTF_LUT_SIZE;
static unsigned int video_eotf_r_mapping[EOTF_LUT_SIZE] = {
	0x0000,	0x0200,	0x0400, 0x0600,
	0x0800, 0x0a00, 0x0c00, 0x0e00,
	0x1000, 0x1200, 0x1400, 0x1600,
	0x1800, 0x1a00, 0x1c00, 0x1e00,
	0x2000, 0x2200, 0x2400, 0x2600,
	0x2800, 0x2a00, 0x2c00, 0x2e00,
	0x3000, 0x3200, 0x3400, 0x3600,
	0x3800, 0x3a00, 0x3c00, 0x3e00,
	0x4000
};

static unsigned int num_video_eotf_g_mapping = EOTF_LUT_SIZE;
static unsigned int video_eotf_g_mapping[EOTF_LUT_SIZE] = {
	0x0000,	0x0200,	0x0400, 0x0600,
	0x0800, 0x0a00, 0x0c00, 0x0e00,
	0x1000, 0x1200, 0x1400, 0x1600,
	0x1800, 0x1a00, 0x1c00, 0x1e00,
	0x2000, 0x2200, 0x2400, 0x2600,
	0x2800, 0x2a00, 0x2c00, 0x2e00,
	0x3000, 0x3200, 0x3400, 0x3600,
	0x3800, 0x3a00, 0x3c00, 0x3e00,
	0x4000
};

static unsigned int num_video_eotf_b_mapping = EOTF_LUT_SIZE;
static unsigned int video_eotf_b_mapping[EOTF_LUT_SIZE] = {
	0x0000,	0x0200,	0x0400, 0x0600,
	0x0800, 0x0a00, 0x0c00, 0x0e00,
	0x1000, 0x1200, 0x1400, 0x1600,
	0x1800, 0x1a00, 0x1c00, 0x1e00,
	0x2000, 0x2200, 0x2400, 0x2600,
	0x2800, 0x2a00, 0x2c00, 0x2e00,
	0x3000, 0x3200, 0x3400, 0x3600,
	0x3800, 0x3a00, 0x3c00, 0x3e00,
	0x4000
};

#define EOTF_COEFF_NORM(a) ((int)((((a) * 4096.0) + 1) / 2))
#define EOTF_COEFF_SIZE 10
#define EOTF_COEFF_RIGHTSHIFT 1
static unsigned int num_osd_eotf_coeff = EOTF_COEFF_SIZE;
static int osd_eotf_coeff[EOTF_COEFF_SIZE] = {
	EOTF_COEFF_NORM(1.0), EOTF_COEFF_NORM(0.0), EOTF_COEFF_NORM(0.0),
	EOTF_COEFF_NORM(0.0), EOTF_COEFF_NORM(1.0), EOTF_COEFF_NORM(0.0),
	EOTF_COEFF_NORM(0.0), EOTF_COEFF_NORM(0.0), EOTF_COEFF_NORM(1.0),
	EOTF_COEFF_RIGHTSHIFT /* right shift */
};

static unsigned int num_video_eotf_coeff = EOTF_COEFF_SIZE;
static int video_eotf_coeff[EOTF_COEFF_SIZE] = {
	EOTF_COEFF_NORM(1.0), EOTF_COEFF_NORM(0.0), EOTF_COEFF_NORM(0.0),
	EOTF_COEFF_NORM(0.0), EOTF_COEFF_NORM(1.0), EOTF_COEFF_NORM(0.0),
	EOTF_COEFF_NORM(0.0), EOTF_COEFF_NORM(0.0), EOTF_COEFF_NORM(1.0),
	EOTF_COEFF_RIGHTSHIFT /* right shift */
};

static unsigned int reload_mtx;
static unsigned int reload_lut;

/******************** osd oetf **************/

#define OSD_OETF_LUT_SIZE 41
static unsigned int num_osd_oetf_r_mapping = OSD_OETF_LUT_SIZE;
static unsigned int osd_oetf_r_mapping[OSD_OETF_LUT_SIZE] = {
		0, 150, 250, 330,
		395, 445, 485, 520,
		544, 632, 686, 725,
		756, 782, 803, 822,
		839, 854, 868, 880,
		892, 902, 913, 922,
		931, 939, 947, 954,
		961, 968, 974, 981,
		986, 993, 998, 1003,
		1009, 1014, 1018, 1023,
		0
};

static unsigned int num_osd_oetf_g_mapping = OSD_OETF_LUT_SIZE;
static unsigned int osd_oetf_g_mapping[OSD_OETF_LUT_SIZE] = {
		0, 0, 0, 0,
		0, 32, 64, 96,
		128, 160, 196, 224,
		256, 288, 320, 352,
		384, 416, 448, 480,
		512, 544, 576, 608,
		640, 672, 704, 736,
		768, 800, 832, 864,
		896, 928, 960, 992,
		1023, 1023, 1023, 1023,
		1023
};

static unsigned int num_osd_oetf_b_mapping = OSD_OETF_LUT_SIZE;
static unsigned int osd_oetf_b_mapping[OSD_OETF_LUT_SIZE] = {
		0, 0, 0, 0,
		0, 32, 64, 96,
		128, 160, 196, 224,
		256, 288, 320, 352,
		384, 416, 448, 480,
		512, 544, 576, 608,
		640, 672, 704, 736,
		768, 800, 832, 864,
		896, 928, 960, 992,
		1023, 1023, 1023, 1023,
		1023
};

/************ video oetf ***************/

#define VIDEO_OETF_LUT_SIZE 289
static unsigned int num_video_oetf_r_mapping = VIDEO_OETF_LUT_SIZE;
static unsigned int video_oetf_r_mapping[VIDEO_OETF_LUT_SIZE] = {
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   4,    8,   12,   16,   20,   24,   28,   32,
	  36,   40,   44,   48,   52,   56,   60,   64,
	  68,   72,   76,   80,   84,   88,   92,   96,
	 100,  104,  108,  112,  116,  120,  124,  128,
	 132,  136,  140,  144,  148,  152,  156,  160,
	 164,  168,  172,  176,  180,  184,  188,  192,
	 196,  200,  204,  208,  212,  216,  220,  224,
	 228,  232,  236,  240,  244,  248,  252,  256,
	 260,  264,  268,  272,  276,  280,  284,  288,
	 292,  296,  300,  304,  308,  312,  316,  320,
	 324,  328,  332,  336,  340,  344,  348,  352,
	 356,  360,  364,  368,  372,  376,  380,  384,
	 388,  392,  396,  400,  404,  408,  412,  416,
	 420,  424,  428,  432,  436,  440,  444,  448,
	 452,  456,  460,  464,  468,  472,  476,  480,
	 484,  488,  492,  496,  500,  504,  508,  512,
	 516,  520,  524,  528,  532,  536,  540,  544,
	 548,  552,  556,  560,  564,  568,  572,  576,
	 580,  584,  588,  592,  596,  600,  604,  608,
	 612,  616,  620,  624,  628,  632,  636,  640,
	 644,  648,  652,  656,  660,  664,  668,  672,
	 676,  680,  684,  688,  692,  696,  700,  704,
	 708,  712,  716,  720,  724,  728,  732,  736,
	 740,  744,  748,  752,  756,  760,  764,  768,
	 772,  776,  780,  784,  788,  792,  796,  800,
	 804,  808,  812,  816,  820,  824,  828,  832,
	 836,  840,  844,  848,  852,  856,  860,  864,
	 868,  872,  876,  880,  884,  888,  892,  896,
	 900,  904,  908,  912,  916,  920,  924,  928,
	 932,  936,  940,  944,  948,  952,  956,  960,
	 964,  968,  972,  976,  980,  984,  988,  992,
	 996, 1000, 1004, 1008, 1012, 1016, 1020, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023
};

static unsigned int num_video_oetf_g_mapping = VIDEO_OETF_LUT_SIZE;
static unsigned int video_oetf_g_mapping[VIDEO_OETF_LUT_SIZE] = {
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   4,    8,   12,   16,   20,   24,   28,   32,
	  36,   40,   44,   48,   52,   56,   60,   64,
	  68,   72,   76,   80,   84,   88,   92,   96,
	 100,  104,  108,  112,  116,  120,  124,  128,
	 132,  136,  140,  144,  148,  152,  156,  160,
	 164,  168,  172,  176,  180,  184,  188,  192,
	 196,  200,  204,  208,  212,  216,  220,  224,
	 228,  232,  236,  240,  244,  248,  252,  256,
	 260,  264,  268,  272,  276,  280,  284,  288,
	 292,  296,  300,  304,  308,  312,  316,  320,
	 324,  328,  332,  336,  340,  344,  348,  352,
	 356,  360,  364,  368,  372,  376,  380,  384,
	 388,  392,  396,  400,  404,  408,  412,  416,
	 420,  424,  428,  432,  436,  440,  444,  448,
	 452,  456,  460,  464,  468,  472,  476,  480,
	 484,  488,  492,  496,  500,  504,  508,  512,
	 516,  520,  524,  528,  532,  536,  540,  544,
	 548,  552,  556,  560,  564,  568,  572,  576,
	 580,  584,  588,  592,  596,  600,  604,  608,
	 612,  616,  620,  624,  628,  632,  636,  640,
	 644,  648,  652,  656,  660,  664,  668,  672,
	 676,  680,  684,  688,  692,  696,  700,  704,
	 708,  712,  716,  720,  724,  728,  732,  736,
	 740,  744,  748,  752,  756,  760,  764,  768,
	 772,  776,  780,  784,  788,  792,  796,  800,
	 804,  808,  812,  816,  820,  824,  828,  832,
	 836,  840,  844,  848,  852,  856,  860,  864,
	 868,  872,  876,  880,  884,  888,  892,  896,
	 900,  904,  908,  912,  916,  920,  924,  928,
	 932,  936,  940,  944,  948,  952,  956,  960,
	 964,  968,  972,  976,  980,  984,  988,  992,
	 996, 1000, 1004, 1008, 1012, 1016, 1020, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023
};

static unsigned int num_video_oetf_b_mapping = VIDEO_OETF_LUT_SIZE;
static unsigned int video_oetf_b_mapping[VIDEO_OETF_LUT_SIZE] = {
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   4,    8,   12,   16,   20,   24,   28,   32,
	  36,   40,   44,   48,   52,   56,   60,   64,
	  68,   72,   76,   80,   84,   88,   92,   96,
	 100,  104,  108,  112,  116,  120,  124,  128,
	 132,  136,  140,  144,  148,  152,  156,  160,
	 164,  168,  172,  176,  180,  184,  188,  192,
	 196,  200,  204,  208,  212,  216,  220,  224,
	 228,  232,  236,  240,  244,  248,  252,  256,
	 260,  264,  268,  272,  276,  280,  284,  288,
	 292,  296,  300,  304,  308,  312,  316,  320,
	 324,  328,  332,  336,  340,  344,  348,  352,
	 356,  360,  364,  368,  372,  376,  380,  384,
	 388,  392,  396,  400,  404,  408,  412,  416,
	 420,  424,  428,  432,  436,  440,  444,  448,
	 452,  456,  460,  464,  468,  472,  476,  480,
	 484,  488,  492,  496,  500,  504,  508,  512,
	 516,  520,  524,  528,  532,  536,  540,  544,
	 548,  552,  556,  560,  564,  568,  572,  576,
	 580,  584,  588,  592,  596,  600,  604,  608,
	 612,  616,  620,  624,  628,  632,  636,  640,
	 644,  648,  652,  656,  660,  664,  668,  672,
	 676,  680,  684,  688,  692,  696,  700,  704,
	 708,  712,  716,  720,  724,  728,  732,  736,
	 740,  744,  748,  752,  756,  760,  764,  768,
	 772,  776,  780,  784,  788,  792,  796,  800,
	 804,  808,  812,  816,  820,  824,  828,  832,
	 836,  840,  844,  848,  852,  856,  860,  864,
	 868,  872,  876,  880,  884,  888,  892,  896,
	 900,  904,  908,  912,  916,  920,  924,  928,
	 932,  936,  940,  944,  948,  952,  956,  960,
	 964,  968,  972,  976,  980,  984,  988,  992,
	 996, 1000, 1004, 1008, 1012, 1016, 1020, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023
};

#define COEFF_NORM(a) ((int)((((a) * 2048.0) + 1) / 2))
#define MATRIX_5x3_COEF_SIZE 24
/******* osd1 matrix0 *******/
/* default rgb to yuv_limit */
static unsigned int num_osd_matrix_coeff = MATRIX_5x3_COEF_SIZE;
static int osd_matrix_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(0.2126),	COEFF_NORM(0.7152),	COEFF_NORM(0.0722),
	COEFF_NORM(-0.11457),	COEFF_NORM(-0.38543),	COEFF_NORM(0.5),
	COEFF_NORM(0.5),	COEFF_NORM(-0.45415),	COEFF_NORM(-0.045847),
	0, 0, 0, /* 30/31/32 */
	0, 0, 0, /* 40/41/42 */
	0, 512, 512, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

static unsigned int num_vd1_matrix_coeff = MATRIX_5x3_COEF_SIZE;
static int vd1_matrix_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(1.0),	COEFF_NORM(0.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(1.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(0.0),	COEFF_NORM(1.0),
	0, 0, 0, /* 30/31/32 */
	0, 0, 0, /* 40/41/42 */
	0, 0, 0, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

static unsigned int num_vd2_matrix_coeff = MATRIX_5x3_COEF_SIZE;
static int vd2_matrix_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(1.0),	COEFF_NORM(0.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(1.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(0.0),	COEFF_NORM(1.0),
	0, 0, 0, /* 30/31/32 */
	0, 0, 0, /* 40/41/42 */
	0, 0, 0, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

static unsigned int num_post_matrix_coeff = MATRIX_5x3_COEF_SIZE;
static int post_matrix_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(1.0),	COEFF_NORM(0.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(1.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(0.0),	COEFF_NORM(1.0),
	0, 0, 0, /* 30/31/32 */
	0, 0, 0, /* 40/41/42 */
	0, 0, 0, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

static unsigned int num_xvycc_matrix_coeff = MATRIX_5x3_COEF_SIZE;
static int xvycc_matrix_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(1.0),	COEFF_NORM(0.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(1.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(0.0),	COEFF_NORM(1.0),
	0, 0, 0, /* 30/31/32 */
	0, 0, 0, /* 40/41/42 */
	0, 0, 0, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

/****************** osd eotf ********************/
module_param_array(osd_eotf_coeff, int,
	&num_osd_eotf_coeff, 0664);
MODULE_PARM_DESC(osd_eotf_coeff, "\n matrix for osd eotf\n");

module_param_array(osd_eotf_r_mapping, uint,
	&num_osd_eotf_r_mapping, 0664);
MODULE_PARM_DESC(osd_eotf_r_mapping, "\n lut for osd r eotf\n");

module_param_array(osd_eotf_g_mapping, uint,
	&num_osd_eotf_g_mapping, 0664);
MODULE_PARM_DESC(osd_eotf_g_mapping, "\n lut for osd g eotf\n");

module_param_array(osd_eotf_b_mapping, uint,
	&num_osd_eotf_b_mapping, 0664);
MODULE_PARM_DESC(osd_eotf_b_mapping, "\n lut for osd b eotf\n");

module_param_array(osd_matrix_coeff, int,
	&num_osd_matrix_coeff, 0664);
MODULE_PARM_DESC(osd_matrix_coeff, "\n coef for osd matrix\n");

/****************** video eotf ********************/
module_param_array(video_eotf_coeff, int,
	&num_video_eotf_coeff, 0664);
MODULE_PARM_DESC(video_eotf_coeff, "\n matrix for video eotf\n");

module_param_array(video_eotf_r_mapping, uint,
	&num_video_eotf_r_mapping, 0664);
MODULE_PARM_DESC(video_eotf_r_mapping, "\n lut for video r eotf\n");

module_param_array(video_eotf_g_mapping, uint,
	&num_video_eotf_g_mapping, 0664);
MODULE_PARM_DESC(video_eotf_g_mapping, "\n lut for video g eotf\n");

module_param_array(video_eotf_b_mapping, uint,
	&num_video_eotf_b_mapping, 0664);
MODULE_PARM_DESC(video_eotf_b_mapping, "\n lut for video b eotf\n");

/****************** osd oetf ********************/
module_param_array(osd_oetf_r_mapping, uint,
	&num_osd_oetf_r_mapping, 0664);
MODULE_PARM_DESC(osd_oetf_r_mapping, "\n lut for osd r oetf\n");

module_param_array(osd_oetf_g_mapping, uint,
	&num_osd_oetf_g_mapping, 0664);
MODULE_PARM_DESC(osd_oetf_g_mapping, "\n lut for osd g oetf\n");

module_param_array(osd_oetf_b_mapping, uint,
	&num_osd_oetf_b_mapping, 0664);
MODULE_PARM_DESC(osd_oetf_b_mapping, "\n lut for osd b oetf\n");

/****************** video oetf ********************/
module_param_array(video_oetf_r_mapping, uint,
	&num_video_oetf_r_mapping, 0664);
MODULE_PARM_DESC(video_oetf_r_mapping, "\n lut for video r oetf\n");

module_param_array(video_oetf_g_mapping, uint,
	&num_video_oetf_g_mapping, 0664);
MODULE_PARM_DESC(video_oetf_g_mapping, "\n lut for video g oetf\n");

module_param_array(video_oetf_b_mapping, uint,
	&num_video_oetf_b_mapping, 0664);
MODULE_PARM_DESC(video_oetf_b_mapping, "\n lut for video b oetf\n");

/****************** vpp matrix ********************/

module_param_array(vd1_matrix_coeff, int,
	&num_vd1_matrix_coeff, 0664);
MODULE_PARM_DESC(vd1_matrix_coeff, "\n vd1 matrix\n");

module_param_array(vd2_matrix_coeff, int,
	&num_vd2_matrix_coeff, 0664);
MODULE_PARM_DESC(vd2_matrix_coeff, "\n vd2 matrix\n");

module_param_array(post_matrix_coeff, int,
	&num_post_matrix_coeff, 0664);
MODULE_PARM_DESC(post_matrix_coeff, "\n post matrix\n");

module_param_array(xvycc_matrix_coeff, int,
	&num_xvycc_matrix_coeff, 0664);
MODULE_PARM_DESC(xvycc_matrix_coeff, "\n xvycc matrix\n");

/****************** matrix/lut reload********************/

module_param(reload_mtx, uint, 0664);
MODULE_PARM_DESC(reload_mtx, "\n reload matrix coeff\n");

module_param(reload_lut, uint, 0664);
MODULE_PARM_DESC(reload_lut, "\n reload lut settings\n");


static int RGB709_to_YUV709l_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(0.181873),	COEFF_NORM(0.611831),	COEFF_NORM(0.061765),
	COEFF_NORM(-0.100251),	COEFF_NORM(-0.337249),	COEFF_NORM(0.437500),
	COEFF_NORM(0.437500),	COEFF_NORM(-0.397384),	COEFF_NORM(-0.040116),
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	0, 512, 512, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

#if 0
static int YUV709l_to_RGB709_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, -512, -512, /* pre offset */
	COEFF_NORM(1.16895),	COEFF_NORM(0.00000),	COEFF_NORM(1.79977),
	COEFF_NORM(1.16895),	COEFF_NORM(-0.21408),	COEFF_NORM(-0.53500),
	COEFF_NORM(1.16895),	COEFF_NORM(2.12069),	COEFF_NORM(0.00000),
	0, 0, 0, /* 30/31/32 */
	0, 0, 0, /* 40/41/42 */
	0, 0, 0, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

/*  eotf matrix: RGB2020 to RGB709 */
static int eotf_RGB2020_to_RGB709_coeff[EOTF_COEFF_SIZE] = {
	EOTF_COEFF_NORM(1.6607056/2), EOTF_COEFF_NORM(-0.5877533/2),
	EOTF_COEFF_NORM(-0.0729065/2),
	EOTF_COEFF_NORM(-0.1245575/2), EOTF_COEFF_NORM(1.1329346/2),
	EOTF_COEFF_NORM(-0.0083771/2),
	EOTF_COEFF_NORM(-0.0181122/2), EOTF_COEFF_NORM(-0.1005249/2),
	EOTF_COEFF_NORM(1.1186371/2),
	EOTF_COEFF_RIGHTSHIFT
};
#endif

static int bypass_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(1.0),	COEFF_NORM(0.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(1.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(0.0),	COEFF_NORM(1.0),
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	0, 0, 0, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

/*  eotf matrix: bypass */
static int eotf_bypass_coeff[EOTF_COEFF_SIZE] = {
	EOTF_COEFF_NORM(1.0),	EOTF_COEFF_NORM(0.0),	EOTF_COEFF_NORM(0.0),
	EOTF_COEFF_NORM(0.0),	EOTF_COEFF_NORM(1.0),	EOTF_COEFF_NORM(0.0),
	EOTF_COEFF_NORM(0.0),	EOTF_COEFF_NORM(0.0),	EOTF_COEFF_NORM(1.0),
	EOTF_COEFF_RIGHTSHIFT /* right shift */
};

/* post matrix: YUV2020 limit to RGB2020 */
static int YUV2020l_to_RGB2020_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, -512, -512, /* pre offset */
	COEFF_NORM(1.16895),	COEFF_NORM(0.00000),	COEFF_NORM(1.68526),
	COEFF_NORM(1.16895),	COEFF_NORM(-0.18806),	COEFF_NORM(-0.65298),
	COEFF_NORM(1.16895),	COEFF_NORM(2.15017),	COEFF_NORM(0.00000),
	0, 0, 0, /* 30/31/32 */
	0, 0, 0, /* 40/41/42 */
	0, 0, 0, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

/* eotf lut: linear */
static unsigned int eotf_33_linear_mapping[EOTF_LUT_SIZE] = {
	0x0000,	0x0200,	0x0400, 0x0600,
	0x0800, 0x0a00, 0x0c00, 0x0e00,
	0x1000, 0x1200, 0x1400, 0x1600,
	0x1800, 0x1a00, 0x1c00, 0x1e00,
	0x2000, 0x2200, 0x2400, 0x2600,
	0x2800, 0x2a00, 0x2c00, 0x2e00,
	0x3000, 0x3200, 0x3400, 0x3600,
	0x3800, 0x3a00, 0x3c00, 0x3e00,
	0x4000
};

/* eotf lut: 2084 src_lum_max = 800 nits, maximum of 10000 nits */
static unsigned int eotf_33_2084_mapping[EOTF_LUT_SIZE] = {
	    0,     0,     2,     6,    12,    23,    40,    67,
	  106,   161,   240,   351,   503,   712,   994,  1376,
	 1889,  2575,  3491,  4709,  6328,  8477, 11329, 15114,
	20140, 21164, 22188, 23212, 24236, 25260, 26284, 27308,
	28332
};

/* osd oetf lut: linear */
static unsigned int oetf_41_linear_mapping[OSD_OETF_LUT_SIZE] = {
	0, 0, 0, 0,
	0, 32, 64, 96,
	128, 160, 196, 224,
	256, 288, 320, 352,
	384, 416, 448, 480,
	512, 544, 576, 608,
	640, 672, 704, 736,
	768, 800, 832, 864,
	896, 928, 960, 992,
	1023, 1023, 1023, 1023,
	1023
};

static unsigned int oetf_289_hlg_mapping[VIDEO_OETF_LUT_SIZE] = {
	    0,     7,    14,    21,    28,    36,    43,    50,
	   57,    64,    71,    78,    85,    92,   100,   107,
	  114,   121,   128,   157,   181,   202,   222,   239,
	  256,   272,   286,   300,   314,   326,   339,   351,
	  362,   373,   384,   395,   405,   415,   425,   434,
	  443,   453,   462,   470,   479,   487,   496,   504,
	  512,   520,   527,   535,   541,   548,   555,   561,
	  567,   573,   578,   584,   589,   594,   599,   604,
	  609,   614,   618,   623,   627,   631,   635,   639,
	  643,   647,   651,   655,   658,   662,   665,   669,
	  672,   675,   679,   682,   685,   688,   691,   694,
	  697,   700,   703,   706,   708,   711,   714,   716,
	  719,   722,   724,   727,   729,   731,   734,   736,
	  739,   741,   743,   745,   748,   750,   752,   754,
	  756,   758,   760,   762,   765,   767,   769,   770,
	  772,   774,   776,   778,   780,   782,   784,   785,
	  787,   789,   791,   792,   794,   796,   798,   799,
	  801,   803,   804,   806,   807,   809,   811,   812,
	  814,   815,   817,   818,   820,   821,   823,   824,
	  826,   827,   828,   830,   831,   833,   834,   835,
	  837,   838,   840,   841,   842,   843,   845,   846,
	  847,   849,   850,   851,   852,   854,   855,   856,
	  857,   859,   860,   861,   862,   863,   864,   866,
	  867,   868,   869,   870,   871,   872,   874,   875,
	  876,   877,   878,   879,   880,   881,   882,   883,
	  884,   885,   886,   887,   889,   890,   891,   892,
	  893,   894,   895,   896,   897,   898,   898,   899,
	  900,   901,   902,   903,   904,   905,   906,   907,
	  908,   909,   910,   911,   912,   912,   913,   914,
	  915,   916,   917,   918,   919,   920,   920,   921,
	  922,   923,   924,   925,   925,   926,   927,   928,
	  929,   930,   930,   931,   932,   933,   934,   934,
	  935,   936,   937,   938,   938,   939,   940,   941,
	  941,   942,   943,   944,   945,   945,   946,   947,
	  948,   952,   957,   961,   966,   970,   975,   979,
	  984,   988,   993,   997,  1002,  1006,  1011,  1015,
	 1020
};

/* video oetf: linear */
#if 0
static unsigned int oetf_289_linear_mapping[VIDEO_OETF_LUT_SIZE] = {
	   0,    0,    0,    0,    0,    0,    0,    0,
	   0,    0,    0,    0,    0,    0,    0,    0,
	   4,    8,   12,   16,   20,   24,   28,   32,
	  36,   40,   44,   48,   52,   56,   60,   64,
	  68,   72,   76,   80,   84,   88,   92,   96,
	 100,  104,  108,  112,  116,  120,  124,  128,
	 132,  136,  140,  144,  148,  152,  156,  160,
	 164,  168,  172,  176,  180,  184,  188,  192,
	 196,  200,  204,  208,  212,  216,  220,  224,
	 228,  232,  236,  240,  244,  248,  252,  256,
	 260,  264,  268,  272,  276,  280,  284,  288,
	 292,  296,  300,  304,  308,  312,  316,  320,
	 324,  328,  332,  336,  340,  344,  348,  352,
	 356,  360,  364,  368,  372,  376,  380,  384,
	 388,  392,  396,  400,  404,  408,  412,  416,
	 420,  424,  428,  432,  436,  440,  444,  448,
	 452,  456,  460,  464,  468,  472,  476,  480,
	 484,  488,  492,  496,  500,  504,  508,  512,
	 516,  520,  524,  528,  532,  536,  540,  544,
	 548,  552,  556,  560,  564,  568,  572,  576,
	 580,  584,  588,  592,  596,  600,  604,  608,
	 612,  616,  620,  624,  628,  632,  636,  640,
	 644,  648,  652,  656,  660,  664,  668,  672,
	 676,  680,  684,  688,  692,  696,  700,  704,
	 708,  712,  716,  720,  724,  728,  732,  736,
	 740,  744,  748,  752,  756,  760,  764,  768,
	 772,  776,  780,  784,  788,  792,  796,  800,
	 804,  808,  812,  816,  820,  824,  828,  832,
	 836,  840,  844,  848,  852,  856,  860,  864,
	 868,  872,  876,  880,  884,  888,  892,  896,
	 900,  904,  908,  912,  916,  920,  924,  928,
	 932,  936,  940,  944,  948,  952,  956,  960,
	 964,  968,  972,  976,  980,  984,  988,  992,
	 996, 1000, 1004, 1008, 1012, 1016, 1020, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023
};

/* eotf lut: 2084 */
static unsigned int eotf_33_2084_mapping[EOTF_LUT_SIZE] = {
	0, 0, 0, 0,
	1, 2, 3, 5,
	8, 13, 19, 28,
	40, 57, 80, 110,
	151, 206, 279, 377,
	506, 678, 906, 1209,
	1611, 2146, 2857, 3807,
	5077, 6781, 9075, 12175,
	16384,
};

/* video oetf: 709 */
static unsigned int oetf_289_709_mapping[VIDEO_OETF_LUT_SIZE] = {
	   0,   2,   4,   6,   8,  10,  12,  14,
	  16,  18,  20,  22,  24,  26,  28,  30,
	  32,  34,  36,  54,  72,  90, 106, 121,
	 135, 148, 160, 172, 183, 193, 203, 213,
	 222, 231, 239, 248, 256, 264, 272, 279,
	 286, 294, 301, 308, 314, 321, 327, 334,
	 340, 346, 352, 358, 364, 370, 376, 381,
	 387, 392, 398, 403, 408, 413, 418, 423,
	 428, 433, 438, 443, 448, 453, 457, 462,
	 467, 471, 476, 480, 484, 489, 493, 497,
	 502, 506, 510, 514, 518, 522, 527, 531,
	 535, 538, 542, 546, 550, 554, 558, 562,
	 565, 569, 573, 577, 580, 584, 587, 591,
	 595, 598, 602, 605, 609, 612, 616, 619,
	 622, 626, 629, 633, 636, 639, 642, 646,
	 649, 652, 655, 659, 662, 665, 668, 671,
	 674, 678, 681, 684, 687, 690, 693, 696,
	 699, 702, 705, 708, 711, 714, 717, 720,
	 722, 725, 728, 731, 734, 737, 740, 742,
	 745, 748, 751, 754, 756, 759, 762, 765,
	 767, 770, 773, 775, 778, 781, 783, 786,
	 789, 791, 794, 797, 799, 802, 804, 807,
	 809, 812, 815, 817, 820, 822, 825, 827,
	 830, 832, 835, 837, 840, 842, 845, 847,
	 849, 852, 854, 857, 859, 861, 864, 866,
	 869, 871, 873, 876, 878, 880, 883, 885,
	 887, 890, 892, 894, 897, 899, 901, 903,
	 906, 908, 910, 912, 915, 917, 919, 921,
	 924, 926, 928, 930, 932, 935, 937, 939,
	 941, 943, 945, 948, 950, 952, 954, 956,
	 958, 960, 963, 965, 967, 969, 971, 973,
	 975, 977, 979, 981, 984, 986, 988, 990,
	 992,  994,  996,  998, 1000, 1002, 1004, 1006,
	1008, 1010, 1012, 1014, 1016, 1018, 1020, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023
};
#endif

#define SIGN(a) ((a < 0) ? "-" : "+")
#define DECI(a) ((a) / 1024)
#define FRAC(a) ((((a) >= 0) ? \
	((a) & 0x3ff) : ((~(a) + 1) & 0x3ff)) * 10000 / 1024)

const char matrix_name[7][16] = {
	"OSD",
	"VD1",
	"POST",
	"XVYCC",
	"EOTF",
	"OSD_EOTF",
	"VD2"
};
static void print_vpp_matrix(int m_select, int *s, int on)
{
	unsigned int size;
	if (s == NULL)
		return;
	if (m_select == VPP_MATRIX_OSD)
		size = MATRIX_5x3_COEF_SIZE;
	else if (m_select == VPP_MATRIX_POST)
		size = MATRIX_5x3_COEF_SIZE;
	else if (m_select == VPP_MATRIX_VD1)
		size = MATRIX_5x3_COEF_SIZE;
	else if (m_select == VPP_MATRIX_VD2)
		size = MATRIX_5x3_COEF_SIZE;
	else if (m_select == VPP_MATRIX_XVYCC)
		size = MATRIX_5x3_COEF_SIZE;
	else if (m_select == VPP_MATRIX_EOTF)
		size = EOTF_COEFF_SIZE;
	else if (m_select == VPP_MATRIX_OSD_EOTF)
		size = EOTF_COEFF_SIZE;
	else
		return;

	pr_csc("%s matrix %s:\n", matrix_name[m_select],
		on ? "on" : "off");

if (size == MATRIX_5x3_COEF_SIZE) {
	pr_csc("\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\n",
			SIGN(s[0]), DECI(s[0]), FRAC(s[0]),
			SIGN(s[3]), DECI(s[3]), FRAC(s[3]),
			SIGN(s[4]), DECI(s[4]), FRAC(s[4]),
			SIGN(s[5]), DECI(s[5]), FRAC(s[5]),
			SIGN(s[18]), DECI(s[18]), FRAC(s[18]));
	pr_csc("\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\n",
			SIGN(s[1]), DECI(s[1]), FRAC(s[1]),
			SIGN(s[6]), DECI(s[6]), FRAC(s[6]),
			SIGN(s[7]), DECI(s[7]), FRAC(s[7]),
			SIGN(s[8]), DECI(s[8]), FRAC(s[8]),
			SIGN(s[19]), DECI(s[19]), FRAC(s[19]));
	pr_csc("\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\n",
			SIGN(s[2]), DECI(s[2]), FRAC(s[2]),
			SIGN(s[9]), DECI(s[9]), FRAC(s[9]),
			SIGN(s[10]), DECI(s[10]), FRAC(s[10]),
			SIGN(s[11]), DECI(s[11]), FRAC(s[11]),
			SIGN(s[20]), DECI(s[20]), FRAC(s[20]));
		if (s[21]) {
			pr_csc("\t\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\n",
				SIGN(s[12]), DECI(s[12]), FRAC(s[12]),
				SIGN(s[13]), DECI(s[13]), FRAC(s[13]),
				SIGN(s[14]), DECI(s[14]), FRAC(s[14]));
			pr_csc("\t\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\n",
				SIGN(s[15]), DECI(s[15]), FRAC(s[15]),
				SIGN(s[16]), DECI(s[16]), FRAC(s[16]),
				SIGN(s[17]), DECI(s[17]), FRAC(s[17]));
		}
		if (s[22])
			pr_csc("\tright shift=%d\n", s[22]);
	} else {
		pr_csc("\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\n",
			SIGN(s[0]), DECI(s[0]), FRAC(s[0]),
			SIGN(s[1]), DECI(s[1]), FRAC(s[1]),
			SIGN(s[2]), DECI(s[2]), FRAC(s[2]));
		pr_csc("\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\n",
			SIGN(s[3]), DECI(s[3]), FRAC(s[3]),
			SIGN(s[4]), DECI(s[4]), FRAC(s[4]),
			SIGN(s[5]), DECI(s[5]), FRAC(s[5]));
		pr_csc("\t%s%1d.%04d\t%s%1d.%04d\t%s%1d.%04d\n",
			SIGN(s[6]), DECI(s[6]), FRAC(s[6]),
			SIGN(s[7]), DECI(s[7]), FRAC(s[7]),
			SIGN(s[8]), DECI(s[8]), FRAC(s[8]));
		if (s[9])
			pr_csc("\tright shift=%d\n", s[9]);
	}
	pr_csc("\n");
}

void set_vpp_matrix(int m_select, int *s, int on)
{
	int *m = NULL;
	int size = 0;
	int i;

	print_vpp_matrix(m_select, s, on);
	if (m_select == VPP_MATRIX_OSD) {
		m = osd_matrix_coeff;
		size = MATRIX_5x3_COEF_SIZE;
	} else if (m_select == VPP_MATRIX_POST)	{
		m = post_matrix_coeff;
		size = MATRIX_5x3_COEF_SIZE;
	} else if (m_select == VPP_MATRIX_VD1) {
		m = vd1_matrix_coeff;
		size = MATRIX_5x3_COEF_SIZE;
	} else if (m_select == VPP_MATRIX_VD2) {
		m = vd2_matrix_coeff;
		size = MATRIX_5x3_COEF_SIZE;
	} else if (m_select == VPP_MATRIX_XVYCC) {
		m = xvycc_matrix_coeff;
		size = MATRIX_5x3_COEF_SIZE;
	} else if (m_select == VPP_MATRIX_EOTF) {
		m = video_eotf_coeff;
		size = EOTF_COEFF_SIZE;
	} else if (m_select == VPP_MATRIX_OSD_EOTF) {
		m = osd_eotf_coeff;
		size = EOTF_COEFF_SIZE;
	} else
		return;

	if (s)
		for (i = 0; i < size; i++)
			m[i] = s[i];
	else
		reload_mtx &= ~(1 << m_select);

	if (m_select == VPP_MATRIX_OSD) {
		/* osd matrix, VPP_MATRIX_0 */
		WRITE_VPP_REG(VIU_OSD1_MATRIX_PRE_OFFSET0_1,
			((m[0] & 0xfff) << 16) | (m[1] & 0xfff));
		WRITE_VPP_REG(VIU_OSD1_MATRIX_PRE_OFFSET2,
			m[2] & 0xfff);
		WRITE_VPP_REG(VIU_OSD1_MATRIX_COEF00_01,
			((m[3] & 0x1fff) << 16) | (m[4] & 0x1fff));
		WRITE_VPP_REG(VIU_OSD1_MATRIX_COEF02_10,
			((m[5] & 0x1fff) << 16) | (m[6] & 0x1fff));
		WRITE_VPP_REG(VIU_OSD1_MATRIX_COEF11_12,
			((m[7] & 0x1fff) << 16) | (m[8] & 0x1fff));
		WRITE_VPP_REG(VIU_OSD1_MATRIX_COEF20_21,
			((m[9] & 0x1fff) << 16) | (m[10] & 0x1fff));
		if (m[21]) {
			WRITE_VPP_REG(VIU_OSD1_MATRIX_COEF22_30,
				((m[11] & 0x1fff) << 16) | (m[12] & 0x1fff));
			WRITE_VPP_REG(VIU_OSD1_MATRIX_COEF31_32,
				((m[13] & 0x1fff) << 16) | (m[14] & 0x1fff));
			WRITE_VPP_REG(VIU_OSD1_MATRIX_COEF40_41,
				((m[15] & 0x1fff) << 16) | (m[16] & 0x1fff));
			WRITE_VPP_REG(VIU_OSD1_MATRIX_COLMOD_COEF42,
				m[17] & 0x1fff);
		} else {
			WRITE_VPP_REG(VIU_OSD1_MATRIX_COEF22_30,
				(m[11] & 0x1fff) << 16);
		}
		WRITE_VPP_REG(VIU_OSD1_MATRIX_OFFSET0_1,
			((m[18] & 0xfff) << 16) | (m[19] & 0xfff));
		WRITE_VPP_REG(VIU_OSD1_MATRIX_OFFSET2,
			m[20] & 0xfff);
		WRITE_VPP_REG_BITS(VIU_OSD1_MATRIX_COLMOD_COEF42,
			m[21], 30, 2);
		WRITE_VPP_REG_BITS(VIU_OSD1_MATRIX_COLMOD_COEF42,
			m[22], 16, 3);
		/* 23 reserved for clipping control */
		WRITE_VPP_REG_BITS(VIU_OSD1_MATRIX_CTRL, on, 0, 1);
		WRITE_VPP_REG_BITS(VIU_OSD1_MATRIX_CTRL, 0, 1, 1);
	} else if (m_select == VPP_MATRIX_EOTF) {
		/* eotf matrix, VPP_MATRIX_EOTF */
		for (i = 0; i < 5; i++)
			WRITE_VPP_REG(VIU_EOTF_CTL + i + 1,
				((m[i * 2] & 0x1fff) << 16)
				| (m[i * 2 + 1] & 0x1fff));

		WRITE_VPP_REG_BITS(VIU_EOTF_CTL, on, 30, 1);
		WRITE_VPP_REG_BITS(VIU_EOTF_CTL, on, 31, 1);
	} else if (m_select == VPP_MATRIX_OSD_EOTF) {
		/* osd eotf matrix, VPP_MATRIX_OSD_EOTF */
		for (i = 0; i < 5; i++)
			WRITE_VPP_REG(VIU_OSD1_EOTF_CTL + i + 1,
				((m[i * 2] & 0x1fff) << 16)
				| (m[i * 2 + 1] & 0x1fff));

		WRITE_VPP_REG_BITS(VIU_OSD1_EOTF_CTL, on, 30, 1);
		WRITE_VPP_REG_BITS(VIU_OSD1_EOTF_CTL, on, 31, 1);
	} else {
		/* vd1 matrix, VPP_MATRIX_1 */
		/* post matrix, VPP_MATRIX_2 */
		/* xvycc matrix, VPP_MATRIX_3 */
		/* vd2 matrix, VPP_MATRIX_6 */
		if (m_select == VPP_MATRIX_POST) {
			/* post matrix */
			m = post_matrix_coeff;
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, on, 0, 1);
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 0, 8, 2);
		} else if (m_select == VPP_MATRIX_VD1) {
			/* vd1 matrix */
			m = vd1_matrix_coeff;
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, on, 5, 1);
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 8, 2);
		} else if (m_select == VPP_MATRIX_VD2) {
			/* vd2 matrix */
			m = vd2_matrix_coeff;
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, on, 4, 1);
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 2, 8, 2);
		} else if (m_select == VPP_MATRIX_XVYCC) {
			/* xvycc matrix */
			m = xvycc_matrix_coeff;
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, on, 6, 1);
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 3, 8, 2);
		}
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1,
			((m[0] & 0xfff) << 16) | (m[1] & 0xfff));
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2,
			m[2] & 0xfff);
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01,
			((m[3] & 0x1fff) << 16) | (m[4] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10,
			((m[5]  & 0x1fff) << 16) | (m[6] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12,
			((m[7] & 0x1fff) << 16) | (m[8] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21,
			((m[9] & 0x1fff) << 16) | (m[10] & 0x1fff));
		WRITE_VPP_REG(VPP_MATRIX_COEF22,
			m[11] & 0x1fff);
		if (m[21]) {
			WRITE_VPP_REG(VPP_MATRIX_COEF13_14,
				((m[12] & 0x1fff) << 16) | (m[13] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF15_25,
				((m[14] & 0x1fff) << 16) | (m[17] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF23_24,
				((m[15] & 0x1fff) << 16) | (m[16] & 0x1fff));
		}
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1,
			((m[18] & 0xfff) << 16) | (m[19] & 0xfff));
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2,
			m[20] & 0xfff);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
			m[21], 3, 2);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
			m[22], 5, 3);
	}
}

const char lut_name[4][16] = {
	"OSD_EOTF",
	"OSD_OETF",
	"EOTF",
	"OETF",
};
/* VIDEO_OETF_LUT_SIZE 289 >
	OSD_OETF_LUT_SIZE 41 >
	OSD_OETF_LUT_SIZE 33 */
static void print_vpp_lut(
	enum vpp_lut_sel_e lut_sel,
	int on)
{
	unsigned short r_map[VIDEO_OETF_LUT_SIZE];
	unsigned short g_map[VIDEO_OETF_LUT_SIZE];
	unsigned short b_map[VIDEO_OETF_LUT_SIZE];
	unsigned int addr_port;
	unsigned int data_port;
	unsigned int ctrl_port;
	unsigned int data;
	int i;

	if (lut_sel == VPP_LUT_OSD_EOTF) {
		addr_port = VIU_OSD1_EOTF_LUT_ADDR_PORT;
		data_port = VIU_OSD1_EOTF_LUT_DATA_PORT;
		ctrl_port = VIU_OSD1_EOTF_CTL;
	} else if (lut_sel == VPP_LUT_EOTF) {
		addr_port = VIU_EOTF_LUT_ADDR_PORT;
		data_port = VIU_EOTF_LUT_DATA_PORT;
		ctrl_port = VIU_EOTF_CTL;
	} else if (lut_sel == VPP_LUT_OSD_OETF) {
		addr_port = VIU_OSD1_OETF_LUT_ADDR_PORT;
		data_port = VIU_OSD1_OETF_LUT_DATA_PORT;
		ctrl_port = VIU_OSD1_OETF_CTL;
	} else if (lut_sel == VPP_LUT_OETF) {
		addr_port = XVYCC_LUT_R_ADDR_PORT;
		data_port = XVYCC_LUT_R_DATA_PORT;
		ctrl_port = XVYCC_LUT_CTL;
	} else
		return;
	if (lut_sel == VPP_LUT_OSD_OETF) {
		WRITE_VPP_REG(addr_port, 0);
		for (i = 0; i < 20; i++) {
			data = READ_VPP_REG(data_port);
			r_map[i * 2] = data & 0xffff;
			r_map[i * 2 + 1] = (data >> 16) & 0xffff;
		}
		data = READ_VPP_REG(data_port);
		r_map[OSD_OETF_LUT_SIZE - 1] = data & 0xffff;
		g_map[0] = (data >> 16) & 0xffff;
		for (i = 0; i < 20; i++) {
			data = READ_VPP_REG(data_port);
			g_map[i * 2 + 1] = data & 0xffff;
			g_map[i * 2 + 2] = (data >> 16) & 0xffff;
		}
		for (i = 0; i < 20; i++) {
			data = READ_VPP_REG(data_port);
			b_map[i * 2] = data & 0xffff;
			b_map[i * 2 + 1] = (data >> 16) & 0xffff;
		}
		data = READ_VPP_REG(data_port);
		b_map[OSD_OETF_LUT_SIZE - 1] = data & 0xffff;
		pr_csc("%s lut %s:\n", lut_name[lut_sel], on ? "on" : "off");
		for (i = 0; i < OSD_OETF_LUT_SIZE; i++) {
			pr_csc("\t[%d] = 0x%04x 0x%04x 0x%04x\n",
				i, r_map[i], g_map[i], b_map[i]);
		}
		pr_csc("\n");
	} else if ((lut_sel == VPP_LUT_OSD_EOTF) || (lut_sel == VPP_LUT_EOTF)) {
		WRITE_VPP_REG(addr_port, 0);
		for (i = 0; i < 16; i++) {
			data = READ_VPP_REG(data_port);
			r_map[i * 2] = data & 0xffff;
			r_map[i * 2 + 1] = (data >> 16) & 0xffff;
		}
		data = READ_VPP_REG(data_port);
		r_map[EOTF_LUT_SIZE - 1] = data & 0xffff;
		g_map[0] = (data >> 16) & 0xffff;
		for (i = 0; i < 16; i++) {
			data = READ_VPP_REG(data_port);
			g_map[i * 2 + 1] = data & 0xffff;
			g_map[i * 2 + 2] = (data >> 16) & 0xffff;
		}
		for (i = 0; i < 16; i++) {
			data = READ_VPP_REG(data_port);
			b_map[i * 2] = data & 0xffff;
			b_map[i * 2 + 1] = (data >> 16) & 0xffff;
		}
		data = READ_VPP_REG(data_port);
		b_map[EOTF_LUT_SIZE - 1] = data & 0xffff;
		pr_csc("%s lut %s:\n", lut_name[lut_sel], on ? "on" : "off");
		for (i = 0; i < EOTF_LUT_SIZE; i++) {
			pr_csc("\t[%d] = 0x%04x 0x%04x 0x%04x\n",
				i, r_map[i], g_map[i], b_map[i]);
		}
		pr_csc("\n");
	} else if (lut_sel == VPP_LUT_OETF) {
		WRITE_VPP_REG(ctrl_port, 0x0);
		WRITE_VPP_REG(addr_port, 0);
		for (i = 0; i < VIDEO_OETF_LUT_SIZE; i++)
			r_map[i] = READ_VPP_REG(data_port) & 0x3ff;
		WRITE_VPP_REG(addr_port + 2, 0);
		for (i = 0; i < VIDEO_OETF_LUT_SIZE; i++)
			g_map[i] = READ_VPP_REG(data_port + 2) & 0x3ff;
		WRITE_VPP_REG(addr_port + 4, 0);
		for (i = 0; i < VIDEO_OETF_LUT_SIZE; i++)
			b_map[i] = READ_VPP_REG(data_port + 4) & 0x3ff;
		pr_csc("%s lut %s:\n", lut_name[lut_sel], on ? "on" : "off");
		for (i = 0; i < VIDEO_OETF_LUT_SIZE; i++) {
			pr_csc("\t[%d] = 0x%04x 0x%04x 0x%04x\n",
				i, r_map[i], g_map[i], b_map[i]);
		}
		pr_csc("\n");
		if (on)
			WRITE_VPP_REG(ctrl_port, 0x7f);
	}
}

void set_vpp_lut(
	enum vpp_lut_sel_e lut_sel,
	unsigned int *r,
	unsigned int *g,
	unsigned int *b,
	int on)
{
	unsigned int *r_map = NULL;
	unsigned int *g_map = NULL;
	unsigned int *b_map = NULL;
	unsigned int addr_port;
	unsigned int data_port;
	unsigned int ctrl_port;
	int i;

	if (reload_lut & (1 << lut_sel))
		reload_lut &= ~(1 << lut_sel);
	if (lut_sel == VPP_LUT_OSD_EOTF) {
		r_map = osd_eotf_r_mapping;
		g_map = osd_eotf_g_mapping;
		b_map = osd_eotf_b_mapping;
		addr_port = VIU_OSD1_EOTF_LUT_ADDR_PORT;
		data_port = VIU_OSD1_EOTF_LUT_DATA_PORT;
		ctrl_port = VIU_OSD1_EOTF_CTL;
	} else if (lut_sel == VPP_LUT_EOTF) {
		r_map = video_eotf_r_mapping;
		g_map = video_eotf_g_mapping;
		b_map = video_eotf_b_mapping;
		addr_port = VIU_EOTF_LUT_ADDR_PORT;
		data_port = VIU_EOTF_LUT_DATA_PORT;
		ctrl_port = VIU_EOTF_CTL;
	} else if (lut_sel == VPP_LUT_OSD_OETF) {
		r_map = osd_oetf_r_mapping;
		g_map = osd_oetf_g_mapping;
		b_map = osd_oetf_b_mapping;
		addr_port = VIU_OSD1_OETF_LUT_ADDR_PORT;
		data_port = VIU_OSD1_OETF_LUT_DATA_PORT;
		ctrl_port = VIU_OSD1_OETF_CTL;
	} else if (lut_sel == VPP_LUT_OETF) {
#if 0
		load_knee_lut(on);
		return;
#else
		r_map = video_oetf_r_mapping;
		g_map = video_oetf_g_mapping;
		b_map = video_oetf_b_mapping;
		addr_port = XVYCC_LUT_R_ADDR_PORT;
		data_port = XVYCC_LUT_R_DATA_PORT;
		ctrl_port = XVYCC_LUT_CTL;
#endif
	} else
		return;

	if (lut_sel == VPP_LUT_OSD_OETF) {
		if (r && r_map)
			for (i = 0; i < OSD_OETF_LUT_SIZE; i++)
				r_map[i] = r[i];
		if (g && g_map)
			for (i = 0; i < OSD_OETF_LUT_SIZE; i++)
				g_map[i] = g[i];
		if (r && r_map)
			for (i = 0; i < OSD_OETF_LUT_SIZE; i++)
				b_map[i] = b[i];
		WRITE_VPP_REG(addr_port, 0);
		for (i = 0; i < 20; i++)
			WRITE_VPP_REG(data_port,
				r_map[i * 2]
				| (r_map[i * 2 + 1] << 16));
		WRITE_VPP_REG(data_port,
			r_map[OSD_OETF_LUT_SIZE - 1]
			| (g_map[0] << 16));
		for (i = 0; i < 20; i++)
			WRITE_VPP_REG(data_port,
				g_map[i * 2 + 1]
				| (g_map[i * 2 + 2] << 16));
		for (i = 0; i < 20; i++)
			WRITE_VPP_REG(data_port,
				b_map[i * 2]
				| (b_map[i * 2 + 1] << 16));
		WRITE_VPP_REG(data_port,
			b_map[OSD_OETF_LUT_SIZE - 1]);
		if (on)
			WRITE_VPP_REG_BITS(ctrl_port, 7, 29, 3);
		else
			WRITE_VPP_REG_BITS(ctrl_port, 0, 29, 3);
	} else if ((lut_sel == VPP_LUT_OSD_EOTF) || (lut_sel == VPP_LUT_EOTF)) {
		if (r && r_map)
			for (i = 0; i < EOTF_LUT_SIZE; i++)
				r_map[i] = r[i];
		if (g && g_map)
			for (i = 0; i < EOTF_LUT_SIZE; i++)
				g_map[i] = g[i];
		if (r && r_map)
			for (i = 0; i < EOTF_LUT_SIZE; i++)
				b_map[i] = b[i];
		WRITE_VPP_REG(addr_port, 0);
		for (i = 0; i < 16; i++)
			WRITE_VPP_REG(data_port,
				r_map[i * 2]
				| (r_map[i * 2 + 1] << 16));
		WRITE_VPP_REG(data_port,
			r_map[EOTF_LUT_SIZE - 1]
			| (g_map[0] << 16));
		for (i = 0; i < 16; i++)
			WRITE_VPP_REG(data_port,
				g_map[i * 2 + 1]
				| (b_map[i * 2 + 2] << 16));
		for (i = 0; i < 16; i++)
			WRITE_VPP_REG(data_port,
				b_map[i * 2]
				| (b_map[i * 2 + 1] << 16));
		WRITE_VPP_REG(data_port, b_map[EOTF_LUT_SIZE - 1]);
		if (on)
			WRITE_VPP_REG_BITS(ctrl_port, 7, 27, 3);
		else
			WRITE_VPP_REG_BITS(ctrl_port, 0, 27, 3);
		WRITE_VPP_REG_BITS(ctrl_port, 1, 31, 1);
	} else if (lut_sel == VPP_LUT_OETF) {
		if (r && r_map)
			for (i = 0; i < VIDEO_OETF_LUT_SIZE; i++)
				r_map[i] = r[i];
		if (g && g_map)
			for (i = 0; i < VIDEO_OETF_LUT_SIZE; i++)
				g_map[i] = g[i];
		if (r && r_map)
			for (i = 0; i < VIDEO_OETF_LUT_SIZE; i++)
				b_map[i] = b[i];
		WRITE_VPP_REG(ctrl_port, 0x0);
		WRITE_VPP_REG(addr_port, 0);
		for (i = 0; i < VIDEO_OETF_LUT_SIZE; i++)
			WRITE_VPP_REG(data_port, r_map[i]);
		WRITE_VPP_REG(addr_port + 2, 0);
		for (i = 0; i < VIDEO_OETF_LUT_SIZE; i++)
			WRITE_VPP_REG(data_port + 2, g_map[i]);
		WRITE_VPP_REG(addr_port + 4, 0);
		for (i = 0; i < VIDEO_OETF_LUT_SIZE; i++)
			WRITE_VPP_REG(data_port + 4, b_map[i]);
		if (on) {
			WRITE_VPP_REG(ctrl_port, 0x7f);
			knee_lut_on = 1;
		} else {
			WRITE_VPP_REG(ctrl_port, 0x0);
			knee_lut_on = 0;
		}
		cur_knee_factor = knee_factor;
	}
	print_vpp_lut(lut_sel, on);
}

/***************************** end of gxl hdr **************************/

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
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0xe00);
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0xe00);
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
				WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0xe00);
				WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0xe00);
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
		pr_csc("Master_display_colour changed.\n");

	if (signal_type != cur_signal_type) {
		pr_csc("Signal type changed from 0x%x to 0x%x.\n",
			cur_signal_type, signal_type);
		change_flag |= SIG_CS_CHG;
		cur_signal_type = signal_type;
	}
	if (pre_src_type != vf->source_type) {
		pr_csc("Signal source changed from 0x%x to 0x%x.\n",
			pre_src_type, vf->source_type);
		change_flag |= SIG_SRC_CHG;
		pre_src_type = vf->source_type;
	}
	if (cur_knee_factor != knee_factor) {
		pr_csc("Knee factor changed.\n");
		change_flag |= SIG_KNEE_FACTOR;
	}
	if (cur_hdr_process_mode != hdr_process_mode) {
		pr_csc("HDR mode changed.\n");
		change_flag |= SIG_HDR_MODE;
		cur_hdr_process_mode = hdr_process_mode;
	}
	if (cur_hdr_support != (vinfo->hdr_info.hdr_support & 0x4)) {
		pr_csc("Tx HDR support changed.\n");
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
				pr_csc("\tWARNING: full range HDR!!!\n");
				csc_type = VPP_MATRIX_BT2020YUV_BT2020RGB;
			}
		} else if (signal_transfer_characteristic == 14) {
			/* bt2020-10 */
			pr_csc("\tWARNING: bt2020-10 HDR!!!\n");
			if (signal_range == 0)
				csc_type = VPP_MATRIX_YUV709_RGB;
			else
				csc_type = VPP_MATRIX_YUV709F_RGB;
		} else if (signal_transfer_characteristic == 15) {
			/* bt2020-12 */
			pr_csc("\tWARNING: bt2020-12 HDR!!!\n");
			if (signal_range == 0)
				csc_type = VPP_MATRIX_YUV709_RGB;
			else
				csc_type = VPP_MATRIX_YUV709F_RGB;
		} else {
			/* unknown transfer characteristic */
			pr_csc("\tWARNING: unknown HDR!!!\n");
			if (signal_range == 0)
				csc_type = VPP_MATRIX_YUV709_RGB;
			else
				csc_type = VPP_MATRIX_YUV709F_RGB;
		}
	} else {
		pr_csc("\tWARNING: unsupported colour space!!!\n");
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
	pr_csc("\tHDR color convert matrix:\n");
	for (i = 0; i < 3; i++) {
		m->pre_offset[i] = 0;
		for (j = 0; j < 3; j++)
			m->matrix[i][j] = in[j][i];
		m->offset[i] = 0;
		pr_csc("\t\t%04x %04x %04x\n",
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
			pr_csc(
				"\t\tprimaries[%1d][%1d] = %04x\n",
				i, j,
				p->primaries[i][j]);
	pr_csc("\t\twhite_point = (%04x, %04x)\n",
		p->white_point[0], p->white_point[1]);
	pr_csc("\t\tmax,min luminance = %08x, %08x\n",
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
	} else
		memset(hdr_data->primaries, 0, 10 * sizeof(unsigned int));

	/* hdr send information debug */
	memcpy(&dbg_hdr_send, hdr_data,
			sizeof(struct master_display_info_s));

}

static int hdr_process(
	enum vpp_matrix_csc_e csc_type,
	struct vinfo_s *vinfo,
	struct vframe_master_display_colour_s *master_info)
{
	int need_adjust_contrast = 0;
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
	int mtx[EOTF_COEFF_SIZE] = {
		EOTF_COEFF_NORM(1.6607056/2), EOTF_COEFF_NORM(-0.5877533/2),
		EOTF_COEFF_NORM(-0.0729065/2),
		EOTF_COEFF_NORM(-0.1245575/2), EOTF_COEFF_NORM(1.1329346/2),
		EOTF_COEFF_NORM(-0.0083771/2),
		EOTF_COEFF_NORM(-0.0181122/2), EOTF_COEFF_NORM(-0.1005249/2),
		EOTF_COEFF_NORM(1.1186371/2),
		EOTF_COEFF_RIGHTSHIFT,
	};
	int i, j;

	if (master_info->present_flag) {
		pr_csc("\tMaster_display_colour available.\n");
		print_primaries_info(master_info);
		csc_type =
			prepare_customer_matrix(
				&master_info->primaries,
				&master_info->white_point,
				vinfo, &m);
		need_adjust_contrast = 1;
	} else {
		/* use bt2020 primaries */
		pr_csc("\tNo master_display_colour.\n");
		csc_type =
		prepare_customer_matrix(
			&bt2020_primaries,
			&bt2020_white_point,
			vinfo, &m);
		need_adjust_contrast = 0;
	}

	if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
		/************** OSD ***************/
		/* RGB to YUV */
		/* not using old RGB2YUV convert HW */
		/* use new 10bit OSD convert matrix */
		WRITE_VPP_REG_BITS(VIU_OSD1_BLK0_CFG_W0,
			0, 7, 1);

		/* eotf lut bypass */
		set_vpp_lut(VPP_LUT_OSD_EOTF,
			eotf_33_linear_mapping, /* R */
			eotf_33_linear_mapping, /* G */
			eotf_33_linear_mapping, /* B */
			CSC_ON);

		/* eotf matrix bypass */
		set_vpp_matrix(VPP_MATRIX_OSD_EOTF,
			eotf_bypass_coeff,
			CSC_ON);

		/* oetf lut bypass */
		set_vpp_lut(VPP_LUT_OSD_OETF,
			oetf_41_linear_mapping, /* R */
			oetf_41_linear_mapping, /* G */
			oetf_41_linear_mapping, /* B */
			CSC_ON);

		/* osd matrix RGB709 to YUV709 limit */
		set_vpp_matrix(VPP_MATRIX_OSD,
			RGB709_to_YUV709l_coeff,
			CSC_ON);

		/************** VIDEO **************/
		/* vd1 matrix bypass */
		set_vpp_matrix(VPP_MATRIX_VD1,
			bypass_coeff,
			CSC_ON);

		/* post matrix YUV2020 to RGB2020 */
		set_vpp_matrix(VPP_MATRIX_POST,
			YUV2020l_to_RGB2020_coeff,
			CSC_ON);

		/* eotf lut bypass */
		set_vpp_lut(VPP_LUT_EOTF,
			eotf_33_2084_mapping, /* R */
			eotf_33_2084_mapping, /* G */
			eotf_33_2084_mapping, /* B */
			CSC_ON);

		/* eotf matrix RGB2020 to RGB709 */
		mtx[EOTF_COEFF_SIZE - 1] = m.right_shift;
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++) {
				if (m.matrix[i][j] & 0x1000)
					mtx[i * 3 + j] =
					-(((~m.matrix[i][j]) & 0xfff) + 1);
				else
					mtx[i * 3 + j] = m.matrix[i][j];
			}
		set_vpp_matrix(VPP_MATRIX_EOTF,
			mtx,
			CSC_ON);

		/* oetf lut bypass */
		set_vpp_lut(VPP_LUT_OETF,
			oetf_289_hlg_mapping,
			oetf_289_hlg_mapping,
			oetf_289_hlg_mapping,
			CSC_ON);

		/* xvycc matrix RGB709 to YUV709 limit */
		set_vpp_matrix(VPP_MATRIX_XVYCC,
			RGB709_to_YUV709l_coeff,
			CSC_ON);

		/* not adjust contrast in gxl for now */
		need_adjust_contrast = 0;
	} else {

		/* turn vd1 matrix on */
		vpp_set_matrix(VPP_MATRIX_VD1, CSC_ON,
			csc_type, NULL);
		/* turn post matrix on */
		vpp_set_matrix(VPP_MATRIX_POST, CSC_ON,
			csc_type, &m);
		/* xvycc lut on */
		load_knee_lut(CSC_ON);
		/* if GXTVBB HDMI output(YUV) case */
		/* xvyccc matrix3: RGB to YUV */
		/* other cases */
		/* xvyccc matrix3: bypass */
		if ((vinfo->viu_color_fmt != TVIN_RGB444) &&
			(get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB))
			vpp_set_matrix3(CSC_ON, VPP_MATRIX_RGB_YUV709F);
		else
			vpp_set_matrix3(CSC_OFF, VPP_MATRIX_NULL);
	}
	return need_adjust_contrast;
}

static void bypass_hdr_process(
	enum vpp_matrix_csc_e csc_type,
	struct vinfo_s *vinfo)
{
	if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
		/************** OSD ***************/
		/* RGB to YUV */
		/* not using old RGB2YUV convert HW */
		/* use new 10bit OSD convert matrix */
		WRITE_VPP_REG_BITS(VIU_OSD1_BLK0_CFG_W0,
			0, 7, 1);

		/* eotf lut bypass */
		set_vpp_lut(VPP_LUT_OSD_EOTF,
			eotf_33_linear_mapping, /* R */
			eotf_33_linear_mapping, /* G */
			eotf_33_linear_mapping, /* B */
			CSC_ON);

		/* eotf matrix bypass */
		set_vpp_matrix(VPP_MATRIX_OSD_EOTF,
			eotf_bypass_coeff,
			CSC_ON);

		/* oetf lut bypass */
		set_vpp_lut(VPP_LUT_OSD_OETF,
			oetf_41_linear_mapping, /* R */
			oetf_41_linear_mapping, /* G */
			oetf_41_linear_mapping, /* B */
			CSC_ON);

		/* osd matrix RGB709 to YUV709 limit */
		set_vpp_matrix(VPP_MATRIX_OSD,
			RGB709_to_YUV709l_coeff,
			CSC_ON);

		/************** VIDEO **************/
		/* vd1 matrix bypass */
		set_vpp_matrix(VPP_MATRIX_VD1,
			bypass_coeff,
			CSC_ON);

		/* post matrix bypass */
		set_vpp_matrix(VPP_MATRIX_POST,
			bypass_coeff,
			CSC_ON);

		/* eotf lut bypass */
		set_vpp_lut(VPP_LUT_EOTF,
			eotf_33_linear_mapping, /* R */
			eotf_33_linear_mapping, /* G */
			eotf_33_linear_mapping, /* B */
			CSC_ON);

		/* eotf matrix bypass */
		set_vpp_matrix(VPP_MATRIX_EOTF,
			eotf_bypass_coeff,
			CSC_ON);

		/* oetf lut bypass */
		set_vpp_lut(VPP_LUT_OETF,
			NULL,
			NULL,
			NULL,
			CSC_OFF);

		/* xvycc matrix bypass */
		set_vpp_matrix(VPP_MATRIX_XVYCC,
			bypass_coeff,
			CSC_ON);
	} else {
		/* OSD */
		/* keep RGB */

		/* VIDEO */
		if (csc_type == VPP_MATRIX_BT2020YUV_BT2020RGB) {
			/* vd1 matrix: convert YUV to RGB */
			csc_type = VPP_MATRIX_YUV709_RGB;
		}
		/* vd1 matrix on to convert YUV to RGB */
		vpp_set_matrix(VPP_MATRIX_VD1, CSC_ON,
			csc_type, NULL);
		/* post matrix off */
		vpp_set_matrix(VPP_MATRIX_POST, CSC_OFF,
			csc_type, NULL);
		/* xvycc lut off */
		load_knee_lut(CSC_OFF);
		/* if GXTVBB HDMI output(YUV) case */
		/* xvyccc matrix3: RGB to YUV */
		/* other cases */
		/* xvyccc matrix3: bypass */
		if ((vinfo->viu_color_fmt != TVIN_RGB444) &&
			(get_cpu_type() == MESON_CPU_MAJOR_ID_GXTVBB))
			vpp_set_matrix3(CSC_ON, VPP_MATRIX_RGB_YUV709F);
		else
			vpp_set_matrix3(CSC_OFF, VPP_MATRIX_NULL);
	}
}

static void vpp_matrix_update(struct vframe_s *vf)
{
	struct vinfo_s *vinfo;
	enum vpp_matrix_csc_e csc_type = VPP_MATRIX_NULL;
	int signal_change_flag = 0;
	struct vframe_master_display_colour_s *p = &cur_master_display_colour;
	struct master_display_info_s send_info;
	int need_adjust_contrast = 0;

	if ((get_cpu_type() < MESON_CPU_MAJOR_ID_GXTVBB) ||
		is_meson_gxl_package_905M2() ||
		skip_csc_en)
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

	if ((vinfo->viu_color_fmt != TVIN_RGB444) &&
		(vinfo->hdr_info.hdr_support & 0x4)) {
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
		/* decided by edid or panel info or user setting */
		if ((csc_type == VPP_MATRIX_BT2020YUV_BT2020RGB) &&
			hdr_process_mode) {
			/* hdr->sdr */
			if ((signal_change_flag &
					(SIG_PRI_INFO |
						SIG_KNEE_FACTOR |
					SIG_HDR_MODE)
				) ||
				(cur_csc_type <
					VPP_MATRIX_BT2020YUV_BT2020RGB)) {
				need_adjust_contrast =
					hdr_process(csc_type, vinfo, p);
			}
		} else {
			/* for gxtvbb and gxl HDR bypass process */
			bypass_hdr_process(csc_type, vinfo);
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
			pr_csc("CSC from 0x%x to 0x%x.\n",
				cur_csc_type, csc_type);
			pr_csc("contrast offset = %d.\n",
				vd1_contrast_offset);
			cur_csc_type = csc_type;
		}
	}
	vecm_latch_flag &= ~FLAG_MATRIX_UPDATE;
}

static struct vframe_s *last_vf;
static int null_vf_cnt;

unsigned int null_vf_max = 5;
module_param(null_vf_max, uint, 0664);
MODULE_PARM_DESC(null_vf_max, "\n null_vf_max\n");
void amvecm_matrix_process(struct vframe_s *vf)
{
	struct vframe_s fake_vframe;
	int i;

	if (reload_mtx) {
		for (i = 0; i < NUM_MATRIX; i++)
			if (reload_mtx & (1 << i))
				set_vpp_matrix(i, NULL, CSC_ON);
	}

	if (reload_lut) {
		for (i = 0; i < NUM_LUT; i++)
			if (reload_lut & (1 << i))
				set_vpp_lut(i,
					NULL, /* R */
					NULL, /* G */
					NULL, /* B */
					CSC_ON);
	}

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

int amvecm_hdr_dbg(void)
{
	int i, j;

	if (dbg_vf == NULL)
		goto hdr_dump;
	pr_err("\n----vframe info----\n");
	pr_err("vframe:%p\n", dbg_vf);
	pr_err("index:%d, type:0x%x, type_backup:0x%x, blend_mode:%d\n",
		dbg_vf->index, dbg_vf->type,
		dbg_vf->type_backup, dbg_vf->blend_mode);
	pr_err("duration:%d, duration_pulldown:%d, pts:%d, flag:0x%x\n",
		dbg_vf->duration, dbg_vf->duration_pulldown,
		dbg_vf->pts, dbg_vf->flag);
	pr_err("canvas0Addr:0x%x, canvas1Addr:0x%x, bufWidth:%d\n",
		dbg_vf->canvas0Addr, dbg_vf->canvas1Addr,
		dbg_vf->bufWidth);
	pr_err("width:%d, height:%d, ratio_control:0x%x, bitdepth:%d\n",
		dbg_vf->width, dbg_vf->height,
		dbg_vf->ratio_control, dbg_vf->bitdepth);
	pr_err("signal_type:%x, orientation:%d, video_angle:0x%x\n",
		dbg_vf->signal_type, dbg_vf->orientation,
		dbg_vf->video_angle);
	pr_err("source_type:%d, phase:%d, soruce_mode:%d, sig_fmt:0x%x\n",
		dbg_vf->source_type, dbg_vf->phase,
		dbg_vf->source_mode, dbg_vf->sig_fmt);
	/*
	pr_err(
		"trans_fmt 0x%x, lefteye(%d %d %d %d), righteye(%d %d %d %d)\n",
		vf->trans_fmt, vf->left_eye.start_x, vf->left_eye.start_y,
		vf->left_eye.width, vf->left_eye.height,
		vf->right_eye.start_x, vf->right_eye.start_y,
		vf->right_eye.width, vf->right_eye.height);
	pr_err("mode_3d_enable %d",
		vf->mode_3d_enable);
	pr_err("early_process_fun 0x%p, process_fun 0x%p,
	private_data %p\n",
		vf->early_process_fun, vf->process_fun, vf->private_data);

	pr_err("hist_pow %d, luma_sum %d, chroma_sum %d, pixel_sum %d\n",
		vf->prop.hist.hist_pow, vf->prop.hist.luma_sum,
		vf->prop.hist.chroma_sum, vf->prop.hist.pixel_sum);

	pr_err("height %d, width %d, luma_max %d, luma_min %d\n",
		vf->prop.hist.hist_pow, vf->prop.hist.hist_pow,
		vf->prop.hist.hist_pow, vf->prop.hist.hist_pow);

	pr_err("vpp_luma_sum %d, vpp_chroma_sum %d, vpp_pixel_sum %d\n",
		vf->prop.hist.vpp_luma_sum, vf->prop.hist.vpp_chroma_sum,
		vf->prop.hist.vpp_pixel_sum);

	pr_err("vpp_height %d, vpp_width %d, vpp_luma_max %d,
	 vpp_luma_min %d\n",
		vf->prop.hist.vpp_height, vf->prop.hist.vpp_width,
		vf->prop.hist.vpp_luma_max, vf->prop.hist.vpp_luma_min);

	pr_err("vs_span_cnt %d, vs_cnt %d, hs_cnt0 %d, hs_cnt1 %d\n",
		vf->prop.meas.vs_span_cnt, vf->prop.meas.vs_cnt,
		vf->prop.meas.hs_cnt0, vf->prop.meas.hs_cnt1);

	pr_err("hs_cnt2 %d, vs_cnt %d, hs_cnt3 %d,
	 vs_cycle %d, vs_stamp %d\n",
		vf->prop.meas.hs_cnt2, vf->prop.meas.hs_cnt3,
		vf->prop.meas.vs_cycle, vf->prop.meas.vs_stamp);
	*/
	pr_err("pixel_ratio:%d list:%p\n",
		dbg_vf->pixel_ratio, &dbg_vf->list);
	pr_err("ready_jiffies64:%lld, frame_dirty %d\n",
		dbg_vf->ready_jiffies64, dbg_vf->frame_dirty);

	pr_err("\n----Source HDR info----\n");
	pr_err("\t\tsignal_type:0x%x, present_flag:%d\n",
		dbg_vf->signal_type,
		dbg_vf->prop.master_display_colour.present_flag);
	for (i = 0; i < 3; i++)
		for (j = 0; j < 2; j++)
			pr_err(
				"\t\t primaries[%1d][%1d] = %04x\n",
			i, j,
			dbg_vf->prop.master_display_colour.primaries[i][j]);
	pr_err("\t\twhite_point = (%04x, %04x)\n",
		dbg_vf->prop.master_display_colour.white_point[0],
		dbg_vf->prop.master_display_colour.white_point[1]);
	pr_err("\t\tmax,min luminance = %08x, %08x\n",
		dbg_vf->prop.master_display_colour.luminance[0],
		dbg_vf->prop.master_display_colour.luminance[1]);
hdr_dump:
	pr_err("\n----HDR process info----\n");

	pr_err("hdr_mode:%d, hdr_process_mode:%d, force_csc_type:0x%x\n",
		hdr_mode, hdr_process_mode, force_csc_type);
	pr_err("cur_signal_type:0x%x, cur_csc_mode:0x%x, cur_csc_type:0x%x\n",
		cur_signal_type, cur_csc_mode, cur_csc_type);

	pr_err("knee_lut_on:%d,knee_interpolation_mode:%d,cur_knee_factor:%d\n",
		knee_lut_on, knee_interpolation_mode, cur_knee_factor);

	pr_err("\n----TV EDID info----\n");
	pr_err("hdr_support:0x%x,lumi_max:%d,lumi_avg:%d,lumi_min:%d\n",
		receiver_hdr_info.hdr_support,
		receiver_hdr_info.lumi_max,
		receiver_hdr_info.lumi_avg,
		receiver_hdr_info.lumi_min);

	pr_err("\n----Tx HDR package info----\n");
	pr_err("\t\t features = 0x%08x\n", dbg_hdr_send.features);
	for (i = 0; i < 3; i++)
		for (j = 0; j < 2; j++)
			pr_err(
				"\t\t primaries[%1d][%1d] = %04x\n",
				i, j,
				dbg_hdr_send.primaries[i][j]);
	pr_err("\t\twhite_point = (%04x, %04x)\n",
		dbg_hdr_send.white_point[0],
		dbg_hdr_send.white_point[1]);
	pr_err("\t\tmax,min luminance = %08x, %08x\n",
		dbg_hdr_send.luminance[0], dbg_hdr_send.luminance[1]);

	return 0;
}
