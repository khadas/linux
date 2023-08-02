// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "amdv.h"
#include "amdv_pq_config.h"

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/ctype.h>/* for parse_para_pq */
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/arm-smccc.h>

static u32 tv_max_lin = 200;
static u16 tv_max_pq = 2372;
static u8 current_vsvdb[7];

#define MAX_PARAM   8

static unsigned int panel_max_lumin = 350;

struct pq_config *bin_to_cfg;
struct dv_cfg_info_s cfg_info[MAX_DV_PICTUREMODES];

/* end-user calibration config info */
struct dv_user_target_config user_target_config[MAX_DV_PICTUREMODES];
struct dv_user_cfg_info user_cfg_info[MAX_DV_PICTUREMODES];
bool dv_user_cfg_flag;

static s16 pq_center[MAX_DV_PICTUREMODES][4];
static struct dv_pq_range_s pq_range[4];

#define USE_CENTER_0 0

static int num_picture_mode;
static int default_pic_mode = 1;/*bright(standard) mode as default*/
int cur_pic_mode;/*current picture mode id*/

bool load_bin_config;

static const s16 EXTER_MIN_PQ = -256;
static const s16 EXTER_MAX_PQ = 256;
static const s16 INTER_MIN_PQ = -4096;
static const s16 INTER_MAX_PQ = 4095;

#define LEFT_RANGE_BRIGHTNESS  (-3074) /*limit the range*/
#define RIGHT_RANGE_BRIGHTNESS (3074)
#define LEFT_RANGE_CONTRAST    (-3074)
#define RIGHT_RANGE_CONTRAST   (3074)
#define LEFT_RANGE_COLORSHIFT  (-3074)
#define RIGHT_RANGE_COLORSHIFT (3074)
#define LEFT_RANGE_SATURATION  (-3074)
#define RIGHT_RANGE_SATURATION (3074)

const char *pq_item_str[] = {"brightness",
			     "contrast",
			     "colorshift",
			     "saturation",
			     "darkdetail"};

const char *eotf_str[] = {"bt1886", "pq", "power"};

static u32 cur_debug_tprimary[3][2];
static int debug_tprimary;

/*0: use exter pq [-256,256]. 1:use inter pq [-4096,4095]*/
static int use_inter_pq;

#define MAX_LINE_SIZE 1536

static unsigned int force_hdr_tonemapping;
module_param(force_hdr_tonemapping, uint, 0664);
MODULE_PARM_DESC(force_hdr_tonemapping, "\n force_hdr_tonemapping\n");

static u32 last_front_lux;

u16 L2PQ_100_500[] = {
	2081, /* 100 */
	2157, /* 120 */
	2221, /* 140 */
	2277, /* 160 */
	2327, /* 180 */
	2372, /* 200 */
	2413, /* 220 */
	2450, /* 240 */
	2485, /* 260 */
	2517, /* 280 */
	2547, /* 300 */
	2575, /* 320 */
	2602, /* 340 */
	2627, /* 360 */
	2651, /* 380 */
	2673, /* 400 */
	2694, /* 420 */
	2715, /* 440 */
	2734, /* 460 */
	2753, /* 480 */
	2771, /* 500 */
};

u16 L2PQ_500_4000[] = {
	2771, /* 500 */
	2852, /* 600 */
	2920, /* 700 */
	2980, /* 800 */
	3032, /* 900 */
	3079, /* 1000 */
	3122, /* 1100 */
	3161, /* 1200 */
	3197, /* 1300 */
	3230, /* 1400 */
	3261, /* 1500 */
	3289, /* 1600 */
	3317, /* 1700 */
	3342, /* 1800 */
	3366, /* 1900 */
	3389, /* 2000 */
	3411, /* 2100 */
	3432, /* 2200 */
	3451, /* 2300 */
	3470, /* 2400 */
	3489, /* 2500 */
	3506, /* 2600 */
	3523, /* 2700 */
	3539, /* 2800 */
	3554, /* 2900 */
	3570, /* 3000 */
	3584, /* 3100 */
	3598, /* 3200 */
	3612, /* 3300 */
	3625, /* 3400 */
	3638, /* 3500 */
	3651, /* 3600 */
	3662, /* 3700 */
	3674, /* 3800 */
	3686, /* 3900 */
	3697, /* 4000 */
};

u16 USER_MAX_PQ_100_500[] = {
	2081, /* 100 */
	2120, /* 110 */
	2156, /* 120 */
	2189, /* 130 */
	2221, /* 140 */
	2249, /* 150 */
	2277, /* 160 */
	2302, /* 170 */
	2327, /* 180 */
	2350, /* 190 */
	2372, /* 200 */
	2392, /* 210 */
	2413, /* 220 */
	2432, /* 230 */
	2450, /* 240 */
	2467, /* 250 */
	2485, /* 260 */
	2501, /* 270 */
	2517, /* 280 */
	2532, /* 290 */
	2547, /* 300 */
	2561, /* 310 */
	2575, /* 320 */
	2588, /* 330 */
	2602, /* 340 */
	2614, /* 350 */
	2627, /* 360 */
	2638, /* 370 */
	2651, /* 380 */
	2661, /* 390 */
	2673, /* 400 */
	2683, /* 410 */
	2694, /* 420 */
	2704, /* 430 */
	2715, /* 440 */
	2724, /* 450 */
	2734, /* 460 */
	2743, /* 470 */
	2753, /* 480 */
	2762, /* 490 */
	2771, /* 500 */
};

u16 USER_MAX_PQ_500_4000[] = {
	2771, /* 500 */
	2813, /* 550 */
	2851, /* 600 */
	2887, /* 650 */
	2920, /* 700 */
	2950, /* 750 */
	2979, /* 800 */
	3006, /* 850 */
	3032, /* 900 */
	3056, /* 950 */
	3079, /* 1000 */
	3100, /* 1050 */
	3122, /* 1100 */
	3141, /* 1150 */
	3161, /* 1200 */
	3178, /* 1250 */
	3197, /* 1300 */
	3213, /* 1350 */
	3230, /* 1400 */
	3245, /* 1450 */
	3261, /* 1500 */
	3275, /* 1550 */
	3289, /* 1600 */
	3302, /* 1650 */
	3317, /* 1700 */
	3329, /* 1750 */
	3342, /* 1800 */
	3354, /* 1850 */
	3366, /* 1900 */
	3377, /* 1950 */
	3389, /* 2000 */
	3399, /* 2050 */
	3411, /* 2100 */
	3421, /* 2150 */
	3432, /* 2200 */
	3441, /* 2250 */
	3451, /* 2300 */
	3460, /* 2350 */
	3470, /* 2400 */
	3479, /* 2450 */
	3489, /* 2500 */
	3497, /* 2550 */
	3506, /* 2600 */
	3514, /* 2650 */
	3523, /* 2700 */
	3530, /* 2750 */
	3539, /* 2800 */
	3546, /* 2850 */
	3554, /* 2900 */
	3561, /* 2950 */
	3570, /* 3000 */
	3576, /* 3050 */
	3584, /* 3100 */
	3590, /* 3150 */
	3598, /* 3200 */
	3604, /* 3250 */
	3612, /* 3300 */
	3618, /* 3350 */
	3625, /* 3400 */
	3631, /* 3450 */
	3638, /* 3500 */
	3643, /* 3550 */
	3651, /* 3600 */
	3656, /* 3650 */
	3662, /* 3700 */
	3668, /* 3750 */
	3674, /* 3800 */
	3679, /* 3850 */
	3686, /* 3900 */
	3690, /* 3950 */
	3697, /* 4000 */
};

u16 USER_MIN_PQ_1_4[] = {
	26,  /* 0.001 */
	38,  /* 0.002 */
	47,  /* 0.003 */
	55,  /* 0.004 */
	62,  /* 0.005 */
	68,  /* 0.006 */
	73,  /* 0.007 */
	79,  /* 0.008 */
	83,  /* 0.009 */
	88,  /* 0.010 */
	92,  /* 0.011 */
	96,  /* 0.012 */
	100, /* 0.013 */
	104, /* 0.014 */
	108, /* 0.015 */
	111, /* 0.016 */
	114, /* 0.017 */
	117, /* 0.018 */
	121, /* 0.019 */
	124, /* 0.020 */
	126, /* 0.021 */
	129, /* 0.022 */
	132, /* 0.023 */
	135, /* 0.024 */
	137, /* 0.025 */
	140, /* 0.026 */
	142, /* 0.027 */
	145, /* 0.028 */
	147, /* 0.029 */
	150, /* 0.030 */
	152, /* 0.031 */
	154, /* 0.032 */
	156, /* 0.033 */
	159, /* 0.034 */
	161, /* 0.035 */
	163, /* 0.036 */
	165, /* 0.037 */
	167, /* 0.038 */
	169, /* 0.039 */
	171, /* 0.040 */
};

u16 USER_MIN_PQ_4_10[] = {
	171, /* 0.040 */
	175, /* 0.042 */
	179, /* 0.044 */
	182, /* 0.046 */
	185, /* 0.048 */
	189, /* 0.050 */
	192, /* 0.052 */
	195, /* 0.054 */
	199, /* 0.056 */
	202, /* 0.058 */
	205, /* 0.060 */
	208, /* 0.062 */
	211, /* 0.064 */
	213, /* 0.066 */
	216, /* 0.068 */
	219, /* 0.070 */
	222, /* 0.072 */
	224, /* 0.074 */
	227, /* 0.076 */
	230, /* 0.078 */
	232, /* 0.080 */
	235, /* 0.082 */
	239, /* 0.084 */
	242, /* 0.086 */
	230, /* 0.088 */
	244, /* 0.090 */
	246, /* 0.092 */
	249, /* 0.094 */
	251, /* 0.096 */
	253, /* 0.098 */
	255, /* 0.100 */
};

/*update flag=>bit0: front,bit1: rear, bit2:whitexy, bit3:mode,bit4:darkdetail*/
#define AMBIENT_CFG_FRAMES 46
struct ambient_cfg_s ambient_test_cfg[AMBIENT_CFG_FRAMES] = {
	/* update_flag, ambient, rear, front, whitex, whitey,dark_detail */
	{ 12, 65536, 0, 0, 0, 0, 0},
	{ 4, 0, 0, 0, 32768, 0, 0},
	{ 4, 0, 0, 0, 0, 32768, 0},
	{ 4, 0, 0, 0, 32768, 32768, 0},
	{ 6, 0, 0, 0, 10246, 10780, 0},
	{ 2, 0, 10000, 0, 0, 0, 0},
	{ 3, 0, 5, 0, 0, 0, 0},
	{ 1, 0, 0, 30000, 0, 0, 0},
	{ 8, 0, 0, 0, 0, 0, 0},
	{ 8, 65536, 0, 0, 0, 0, 0},
	{ 7, 0, 2204, 23229, 589, 30638, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 1, 0, 0, 24942, 0, 0, 0},
	{ 7, 0, 4646, 25899, 31686, 9764, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 4, 0, 0, 0, 491, 30113, 0},
	{ 4, 0, 0, 0, 11632, 13402, 0},
	{ 4, 0, 0, 0, 22282, 31162, 0},
	{ 6, 0, 6276, 0, 98, 16056, 0},
	{ 6, 0, 5276, 0, 8454, 6946, 0},
	{ 2, 0, 6558, 0, 0, 0, 0},
	{ 2, 0, 355, 0, 0, 0, 0},
	{ 7, 0, 6898, 9651, 30539, 17104, 0},
	{ 3, 0, 5558, 838, 0, 0, 0},
	{ 5, 0, 0, 8462, 7438, 655, 0},
	{ 8, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 9, 65536, 0, 12792, 0, 0, 0},
	{ 4, 0, 0, 0, 30179, 14909, 0},
	{ 3, 0, 1287, 16711, 0, 0, 0},
	{ 7, 0, 991, 9667, 14450, 9699, 0},
	{ 4, 0, 0, 0, 12746, 5636, 0},
	{ 2, 0, 1382, 0, 0, 0, 0},
	{ 1, 0, 0, 14647, 0, 0, 0},
	{ 2, 0, 9080, 0, 0, 0, 0},
	{ 4, 0, 0, 0, 21921, 16056, 0},
	{ 3, 0, 6840, 13834, 0, 0, 0},
	{ 5, 0, 0, 21597, 25755, 26673, 0},
	{ 0, 0, 0, 0, 0, 0, 0},
	{ 5, 0, 0, 19217, 17825, 17989, 0},
	{ 6, 0, 2185, 0, 13828, 5406, 0},
	{ 2, 0, 3587, 0, 0, 0, 0},
	{ 5, 0, 0, 17788, 3571, 28508, 0},
	{ 4, 0, 0, 0, 32636, 5799, 0},
};

struct ambient_cfg_s ambient_test_cfg_2[AMBIENT_CFG_FRAMES] = {
	/* update_flag, ambient, rear, front, whitex, whitey,dark_detail */
	{31, 65536, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{28, 65536, 5, 24942, 0, 0, 1},
	{31, 65536, 5, 0, 32768, 0, 1},
	{31, 65536, 5, 30000, 0, 32768, 1},
	{31, 65536, 5, 24942, 32768, 32768, 1},
	{30, 65536, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{31, 65536, 2204, 23229, 589, 30638, 1},
	{31, 65536, 4646, 25899, 31686, 9764, 1},
	{28, 65536, 4646, 25899, 491, 30113, 1},
	{28, 65536, 4646, 25899, 11632, 13402, 1},
	{31, 65536, 6276, 23229, 98, 16056, 1},
	{14, 65536, 5276, 23229, 8454, 6946, 1},
	{18, 65536, 6558, 23229, 8454, 6946, 0},
	{18, 65536, 355, 23229, 8454, 6946, 1},
	{31, 65536, 6898, 9651, 30539, 17104, 1},
	{11, 65536, 5558, 838, 30539, 17104, 1},
	{29, 65536, 5558, 8462, 7438, 655, 1},
	{8, 0, 5558, 8462, 7438, 655, 1},
	{0, 0, 5558, 8462, 7438, 655, 1},
	{0, 0, 5558, 8462, 7438, 655, 1},
	{9, 65536, 5558, 12792, 7438, 655, 1},
	{4, 65536, 5558, 12792, 30179, 14909, 1},
	{11, 65536, 1287, 16711, 30179, 14909, 1},
	{15, 65536, 991, 9667, 14450, 9699, 1},
	{12, 65536, 991, 9667, 12746, 5636, 1},
	{10, 65536, 1382, 9667, 12746, 5636, 1},
	{9, 65536, 1382, 14647, 12746, 5636, 1},
	{10, 65536, 9080, 14647, 12746, 5636, 1},
	{12, 65536, 9080, 14647, 21921, 16056, 1},
	{11, 65536, 6840, 13834, 21921, 16056, 1},
	{13, 65536, 6840, 21597, 25755, 26673, 1},
	{5, 65536, 6840, 17788, 3571, 28508, 1},
};

struct ambient_cfg_s ambient_test_cfg_3[AMBIENT_CFG_FRAMES] = {
	/* update_flag, ambient, rear, front, whitex, whitey,dark_detail */
	{31, 65536, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 0},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{24, 0, 5, 24942, 32768, 32768, 1},
	{28, 65536, 5, 24942, 0, 0, 1},
	{31, 65536, 5, 0, 32768, 0, 1},
	{31, 65536, 5, 30000, 0, 32768, 1},
	{31, 65536, 5, 24942, 32768, 32768, 1},
	{30, 65536, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 0},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{24, 0, 5276, 24942, 8454, 6946, 1},
	{31, 65536, 2204, 23229, 589, 30638, 1},
	{31, 65536, 4646, 25899, 31686, 9764, 1},
	{28, 65536, 4646, 25899, 491, 30113, 1},
	{28, 65536, 4646, 25899, 11632, 13402, 1},
	{31, 65536, 6276, 23229, 98, 16056, 1},
	{14, 65536, 5276, 23229, 8454, 6946, 1},
	{18, 65536, 6558, 23229, 8454, 6946, 0},
	{18, 65536, 355, 23229, 8454, 6946, 1},
	{31, 65536, 6898, 9651, 30539, 17104, 1},
	{11, 65536, 5558, 838, 30539, 17104, 1},
	{29, 65536, 5558, 8462, 7438, 655, 1},
	{8, 0, 5558, 8462, 7438, 655, 1},
	{0, 0, 5558, 8462, 7438, 655, 1},
	{0, 0, 5558, 8462, 7438, 655, 1},
	{9, 65536, 5558, 12792, 7438, 655, 1},
	{4, 65536, 5558, 12792, 30179, 14909, 1},
	{11, 65536, 1287, 16711, 30179, 14909, 1},
	{15, 65536, 991, 9667, 14450, 9699, 1},
	{12, 65536, 991, 9667, 12746, 5636, 1},
	{10, 65536, 1382, 9667, 12746, 5636, 1},
	{9, 65536, 1382, 14647, 12746, 5636, 1},
	{10, 65536, 9080, 14647, 12746, 5636, 1},
	{12, 65536, 9080, 14647, 21921, 16056, 1},
	{11, 65536, 6840, 13834, 21921, 16056, 1},
	{13, 65536, 6840, 21597, 25755, 26673, 1},
	{5, 65536, 6840, 17788, 3571, 28508, 1},
};

struct ambient_cfg_s lightsense_test_cfg[2] = {
	/* update_flag, ambient, rear, front, whitex, whitey,dark_detail */
	{ 9, 65536, 0, 242, 0, 0, 0},
	{ 9, 65536, 0, 62768, 0, 0, 0},
};

struct target_config def_tgt_display_cfg_bestpq = {
	36045,
	2,
	0,
	2920,
	88,
	2920,
	2621,
	183500800,
	183500800,
	{
	45305196,
	20964810,
	17971754,
	43708004,
	10415296,
	3180960,
	20984942,
	22078816
	},
	2048,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1229,
	0,
	0,
	0,
	4095,
	0,
	0,
	0,
	{
		0,
		131,
		183500800,
		13107200,
		82897208,
		4095,
		2048,
		100,
		1,
		{
			{5961, 20053, 2024},
			{-3287, -11053, 14340},
			{14340, -13026, -1313}
		},
		15,
		1,
		{2048, 16384, 16384},
		580280,
		82897,
		0,
		17,
		2920,
		1803,
		0,
		0,
		50,
		3276,
		3276,
		200,
		444596224,
		{0, 0, 0}
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		{0}
	},
	{
		{0, 0, 0},
		0,
		0,
		65536,
		0,
		4096,
		5,
		4096,
		{10247, 10781},
		32768,
		3277,
		0,
		1365,
		1365
	},
	{4, 216, 163, 84, 218, 76, 153},
	0,
	2,
	0,
	{0},
	0,
	{
		{
			{18835, -15103, 363},
			{-3457, 8025, -472},
			{155, -605, 4546}
		},
		12,
		{0, 0, 0},
		0,
		{0, 0, 0}
	},
	4096,
	1,
	1,
	{50, 100, 300, 500, 800, 1000, 2000, 3000},
	{65536, 65536, 65536, 65536, 65536, 65536, 65536, 65536},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0},
	0,
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0} /* padding */
};

/*tv cert: DV LL case 5051 and 5055 use this cfg */
struct target_config def_tgt_display_cfg_ll = {
	36045,
	2,
	0,
	2920,
	88,
	2920,
	2621,
	183500800,
	183500800,
	{
	45305196,
	20964810,
	17971754,
	43708004,
	10415296,
	3180960,
	20984942,
	22078816
	},
	2048,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	{
		1,
		187,
		183500800,
		13107200,
		69356784,
		4095,
		2048,
		100,
		1,
		{
			{5961, 20053, 2024},
			{-3287, -11053, 14340},
			{14340, -13026, -1313}
		},
		15,
		1,
		{2048, 16384, 16384},
		693567,
		99081,
		0,
		21,
		2920,
		1803,
		0,
		0,
		50,
		3276,
		3276,
		200,
		1170210816u,
		{0, 0, 0}
	},
	{
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		{0}
	},
	{
		{0, 0, 0},
		0,
		0,
		0,
		0,
		4096,
		5,
		4096,
		{10247, 10781},
		32768,
		3277,
		0,
		1365,
		1365
	},
	{42, 25, 25, 241, 138, 78, 108},
	1,
	2,
	0,
	{0},
	0,
	{
		{
			{18835, -15103, 363},
			{-3457, 8025, -472},
			{155, -605, 4546}
		},
		12,
		{0, 0, 0},
		0,
		{0, 0, 0}
	},
	4096,
	1,
	1,
	{50, 100, 300, 500, 800, 1000, 2000, 3000},
	{65536, 65536, 65536, 65536, 65536, 65536, 65536, 65536},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0},
	0,
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0} /* padding */
};

void calculate_panel_max_pq(enum signal_format_enum src_format,
			    const struct vinfo_s *vinfo,
			    struct target_config *config)
{
	u32 max_lin = tv_max_lin;
	u16 max_pq = tv_max_pq;
	u32 panel_max = tv_max_lin;

	if (use_target_lum_from_cfg &&
	    !(src_format == FORMAT_HDR10 && force_hdr_tonemapping))
		return;
	if (dolby_vision_flags & FLAG_CERTIFICATION)
		return;
	if (panel_max_lumin)
		panel_max = panel_max_lumin;
	else if (vinfo->mode == VMODE_LCD)
		panel_max = vinfo->hdr_info.lumi_max;
	if (panel_max < 100)
		panel_max = 100;
	else if (panel_max > 4000)
		panel_max = 4000;
	if (src_format == FORMAT_HDR10 && force_hdr_tonemapping) {
		if (force_hdr_tonemapping >= hdr10_param.max_display_mastering_lum)
			max_lin = hdr10_param.max_display_mastering_lum;
		else
			max_lin = force_hdr_tonemapping;
	} else {
		max_lin = panel_max;
	}

	if (max_lin != tv_max_lin) {
		if (max_lin < 500) {
			max_lin = max_lin - 100 + 10;
			max_lin = (max_lin / 20) * 20 + 100;
			max_pq = L2PQ_100_500[(max_lin - 100) / 20];
		} else {
			max_lin = max_lin - 500 + 50;
			max_lin = (max_lin / 100) * 100 + 500;
			max_pq = L2PQ_500_4000[(max_lin - 500) / 100];
		}
		if (tv_max_lin != max_lin || tv_max_pq != max_pq)
			pr_dv_dbg("panel max lumin changed from %d(%d) to %d(%d)\n",
				     tv_max_lin, tv_max_pq, max_lin, max_pq);
		tv_max_lin = max_lin;
		tv_max_pq = max_pq;

		config->max_lin = tv_max_lin << 18;
		config->max_lin_dm3 =
			config->max_lin;

		config->max_pq = tv_max_pq;
		config->max_pq_dm3 = config->max_pq;
	}
}

void calculate_user_pq_config(void)
{
	int i = 0;
	u16 tmax = 0;
	u16 max_pq = 0;
	u32 tmin = 0;
	u32 min_pq = 0;

	tmax = user_cfg_info[cur_pic_mode].tmax;
	if (tmax < 500) {
		tmax = tmax - 100 + 5;
		tmax = (tmax / 10) * 10 + 100;
		max_pq = USER_MAX_PQ_100_500[(tmax - 100) / 10];
	} else {
		tmax = tmax - 500 + 25;
		tmax = (tmax / 50) * 50 + 500;
		max_pq = USER_MAX_PQ_500_4000[(tmax - 500) / 50];
	}

	tmin = user_cfg_info[cur_pic_mode].tmin;
	if (tmin < 40) {
		tmin = tmin - 1;
		min_pq = USER_MIN_PQ_1_4[tmin];
	} else {
		tmin = tmin - 40 + 1;
		tmin = (tmin / 2) * 2 + 40;
		min_pq = USER_MIN_PQ_4_10[(tmin - 40) / 2];
	}

	user_target_config[cur_pic_mode].max_pq =
		user_target_config[cur_pic_mode].max_pq_dm3 = max_pq;
	user_target_config[cur_pic_mode].min_pq = min_pq;
	user_target_config[cur_pic_mode].max_lin =
		user_target_config[cur_pic_mode].max_lin_dm3 =
		user_cfg_info[cur_pic_mode].tmax * (1 << 18);
	user_target_config[cur_pic_mode].min_lin =
		user_cfg_info[cur_pic_mode].tmin * (1 << 18) / 1000;
	user_target_config[cur_pic_mode].gamma =
		user_cfg_info[cur_pic_mode].tgamma * 16384 / 10;
	for (i = 0; i < 8; i++) {
		user_target_config[cur_pic_mode].t_primaries[i] =
			user_cfg_info[cur_pic_mode].tprimaries[i] * (1 << 26) / 10000;
	}
	dv_user_cfg_flag = true;
}

/*to do*/
static void update_vsvdb_to_rx(void)
{
	u8 *p = cfg_info[cur_pic_mode].vsvdb;

	if (memcmp(&current_vsvdb[0], p, sizeof(current_vsvdb))) {
		memcpy(&current_vsvdb[0], p, sizeof(current_vsvdb));
		pr_info("%s: %d %d %d %d %d %d %d\n",
			__func__, p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
	}
}

void update_cp_cfg(void)
{
	struct target_config *tdc;
	int i = 0;

	if (cur_pic_mode >= num_picture_mode || num_picture_mode == 0 ||
	    !bin_to_cfg) {
		pr_info("%s, invalid para %d/%d, bin_to_cfg %p",
			__func__, cur_pic_mode, num_picture_mode, bin_to_cfg);
		return;
	}

	memcpy(pq_config_fake,
	       &bin_to_cfg[cur_pic_mode],
	       sizeof(struct pq_config));
	tdc = &(((struct pq_config *)pq_config_fake)->tdc);
	tdc->d_brightness = cfg_info[cur_pic_mode].brightness;
	tdc->d_contrast = cfg_info[cur_pic_mode].contrast;
	tdc->d_color_shift = cfg_info[cur_pic_mode].colorshift;
	tdc->d_saturation = cfg_info[cur_pic_mode].saturation;

	if (dv_user_cfg_flag) {
		tdc->gamma = user_target_config[cur_pic_mode].gamma;
		tdc->max_lin = user_target_config[cur_pic_mode].max_lin;
		tdc->max_lin_dm3 = user_target_config[cur_pic_mode].max_lin_dm3;
		tdc->max_pq = user_target_config[cur_pic_mode].max_pq;
		tdc->max_pq_dm3 = user_target_config[cur_pic_mode].max_pq_dm3;
		tdc->min_lin = user_target_config[cur_pic_mode].min_lin;
		tdc->min_pq = user_target_config[cur_pic_mode].min_pq;
		for (i = 0; i < 8; i++) {
			tdc->t_primaries[i] =
				user_target_config[cur_pic_mode].t_primaries[i];
		}
	}

	if (debug_tprimary) {
		tdc->t_primaries[0] = cur_debug_tprimary[0][0]; /*rx*/
		tdc->t_primaries[1] = cur_debug_tprimary[0][1]; /*ry*/
		tdc->t_primaries[2] = cur_debug_tprimary[1][0]; /*gx*/
		tdc->t_primaries[3] = cur_debug_tprimary[1][1]; /*gy*/
		tdc->t_primaries[4] = cur_debug_tprimary[2][0]; /*bx*/
		tdc->t_primaries[5] = cur_debug_tprimary[2][1]; /*by*/
	}

	set_update_cfg(true);
}

void update_ambient_lightsense(struct ambient_cfg_s *p_ambient)
{
	int print_flag = 0;

	if (cfg_info[cur_pic_mode].light_sense == 0)
		return;

	/*for idk 1.6.1.4*/
	if (cur_pic_mode == 0 ||
		cur_pic_mode == 2 ||
		apo_value.content_type == 1) {
		if (debug_dolby & 0x200)
			pr_dv_dbg("not support lightsense, cur_pic_mode %d, content_type %d\n",
				cur_pic_mode, apo_value.content_type);
		return;
	}

	p_ambient->update_flag |= 9;
	p_ambient->ambient = (1 << 16);
	p_ambient->t_frontLux = cfg_info[cur_pic_mode].t_front_lux;
	print_flag = (p_ambient->t_frontLux - last_front_lux > 1000) ? 1 : 0;

	if ((debug_dolby & 0x200) && print_flag)
		pr_dv_dbg("%s: sensor frontLux %d, last frontLux %d\n",
			__func__, p_ambient->t_frontLux, last_front_lux);
	last_front_lux = p_ambient->t_frontLux;
}

/*0: reset picture mode and reset pq for all picture mode*/
/*1: reset pq for all picture mode*/
/*2: reset pq for current picture mode */
void restore_dv_pq_setting(enum pq_reset_e pq_reset)
{
	int mode = 0;

	if (pq_reset == RESET_ALL)
		cur_pic_mode = default_pic_mode;

	for (mode = 0; mode < num_picture_mode; mode++) {
		if (pq_reset == RESET_PQ_FOR_CUR && mode != cur_pic_mode)
			continue;
		cfg_info[mode].brightness =
			bin_to_cfg[mode].tdc.d_brightness;
		cfg_info[mode].contrast =
			bin_to_cfg[mode].tdc.d_contrast;
		cfg_info[mode].colorshift =
			bin_to_cfg[mode].tdc.d_color_shift;
		cfg_info[mode].saturation =
			bin_to_cfg[mode].tdc.d_saturation;
		cfg_info[mode].dark_detail =
			bin_to_cfg[mode].tdc.ambient_config.dark_detail;
		memcpy(cfg_info[mode].vsvdb,
		       bin_to_cfg[mode].tdc.vsvdb,
		       sizeof(cfg_info[mode].vsvdb));
	}
	cur_debug_tprimary[0][0] =
		bin_to_cfg[0].tdc.t_primaries[0];
	cur_debug_tprimary[0][1] =
		bin_to_cfg[0].tdc.t_primaries[1];
	cur_debug_tprimary[1][0] =
		bin_to_cfg[0].tdc.t_primaries[2];
	cur_debug_tprimary[1][1] =
		bin_to_cfg[0].tdc.t_primaries[3];
	cur_debug_tprimary[2][0] =
		bin_to_cfg[0].tdc.t_primaries[4];
	cur_debug_tprimary[2][1] =
		bin_to_cfg[0].tdc.t_primaries[5];

	update_cp_cfg();
	if (debug_dolby & 0x200)
		pr_info("reset pq %d\n", pq_reset);
}

static void set_dv_pq_center(void)
{
	int mode = 0;
#if USE_CENTER_0
	for (mode = 0; mode < num_picture_mode; mode++) {
		pq_center[mode][0] = 0;
		pq_center[mode][1] = 0;
		pq_center[mode][2] = 0;
		pq_center[mode][3] = 0;
	}
#else
	for (mode = 0; mode < num_picture_mode; mode++) {
		pq_center[mode][0] =
			bin_to_cfg[mode].tdc.d_brightness;
		pq_center[mode][1] =
			bin_to_cfg[mode].tdc.d_contrast;
		pq_center[mode][2] =
			bin_to_cfg[mode].tdc.d_color_shift;
		pq_center[mode][3] =
			bin_to_cfg[mode].tdc.d_saturation;
	}
#endif
}

static void set_dv_pq_range(void)
{
	pq_range[PQ_BRIGHTNESS].left = LEFT_RANGE_BRIGHTNESS;
	pq_range[PQ_BRIGHTNESS].right = RIGHT_RANGE_BRIGHTNESS;
	pq_range[PQ_CONTRAST].left = LEFT_RANGE_CONTRAST;
	pq_range[PQ_CONTRAST].right = RIGHT_RANGE_CONTRAST;
	pq_range[PQ_COLORSHIFT].left = LEFT_RANGE_COLORSHIFT;
	pq_range[PQ_COLORSHIFT].right = RIGHT_RANGE_COLORSHIFT;
	pq_range[PQ_SATURATION].left = LEFT_RANGE_SATURATION;
	pq_range[PQ_SATURATION].right = RIGHT_RANGE_SATURATION;
}

static void remove_comments(char *p_buf)
{
	if (!p_buf)
		return;
	while (*p_buf && *p_buf != '#' && *p_buf != '%')
		p_buf++;

	*p_buf = 0;
}

#define MAX_READ_SIZE 256
static char cur_line[MAX_READ_SIZE];

/*read one line from cfg, return eof flag*/
static bool get_one_line(char **cfg_buf, char *line_buf, bool ignore_comments)
{
	char *line_end;
	size_t line_len = 0;
	bool eof_flag = false;

	if (!cfg_buf || !(*cfg_buf) || !line_buf)
		return true;

	line_buf[0] = '\0';
	while (*line_buf == '\0') {
		if (debug_dolby & 0x2)
			pr_info("*cfg_buf: %s\n", *cfg_buf);
		line_end = strnchr(*cfg_buf, MAX_READ_SIZE, '\n');
		if (debug_dolby & 0x2)
			pr_info("line_end: %s\n", line_end);
		if (!line_end) {
			line_len = strlen(*cfg_buf);
			eof_flag = true;
		} else {
			line_len = (size_t)(line_end - *cfg_buf);
		}
		memcpy(line_buf, *cfg_buf, line_len);
		line_buf[line_len] = '\0';
		if (line_len > 0 && line_buf[line_len - 1] == '\r')
			line_buf[line_len - 1] = '\0';

		*cfg_buf = *cfg_buf + line_len + 1;
		if (ignore_comments)
			remove_comments(line_buf);
		while (isspace(*line_buf))
			line_buf++;

		if (**cfg_buf == '\0')
			eof_flag = true;

		if (eof_flag)
			break;
	}
	if (debug_dolby & 0x200)
		pr_info("get line: %s\n", line_buf);
	return eof_flag;
}

static void get_picture_mode_info(char *cfg_buf)
{
	char *ptr_line;
	char name[32];
	int picmode_count = 0;
	int ret = 0;
	bool eof_flag = false;
	char *p_cfg_buf = cfg_buf;
	s32 bri_off_1 = 0;
	s32 bri_off_2 = 0;

	while (!eof_flag) {
		eof_flag = get_one_line(&cfg_buf, (char *)&cur_line, true);
		ptr_line = cur_line;
		if (eof_flag && (strlen(cur_line) == 0))
			break;
		if ((strncmp(ptr_line, "PictureModeName",
			strlen("PictureModeName")) == 0)) {
			ret = sscanf(ptr_line, "PictureModeName = %s", name);
			if (ret == 1) {
				cfg_info[picmode_count].id =
					picmode_count;
				strcpy(cfg_info[picmode_count].pic_mode_name,
					name);
				if (debug_dolby & 0x200)
					pr_info("update cfg_info[%d]: %s\n",
					picmode_count, name);
				picmode_count++;
				if (picmode_count >= num_picture_mode)
					break;
			}
		}
	}

	eof_flag = false;
	while (!eof_flag) {
		eof_flag = get_one_line(&p_cfg_buf, (char *)&cur_line, false);
		ptr_line = cur_line;
		if (eof_flag && (strlen(cur_line) == 0))
			break;
		if ((strncmp(ptr_line, "#Amlogic_inside DV OTT",
			strlen("#Amlogic_inside DV OTT")) == 0)) {
			ret = sscanf(ptr_line, "#Amlogic_inside DV OTT{%d, %d}",
				     &bri_off_1, &bri_off_2);
			if (ret == 2) {
				brightness_off[0][0] = bri_off_1;
				brightness_off[0][1] = bri_off_2;
				pr_info("update DV OTT bri off: %d %d\n",
					bri_off_1, bri_off_2);
			}
		} else if ((strncmp(ptr_line, "#Amlogic_inside Sink-led",
			strlen("#Amlogic_inside Sink-led")) == 0)) {
			ret = sscanf(ptr_line,
				     "#Amlogic_inside Sink-led{%d, %d}",
				     &bri_off_1, &bri_off_2);
			if (ret == 2) {
				brightness_off[1][0] = bri_off_1;
				brightness_off[1][1] = bri_off_2;
				pr_info("update Sink-led bri off: %d %d\n",
					bri_off_1, bri_off_2);
			}
		} else if ((strncmp(ptr_line, "#Amlogic_inside Source-led",
			strlen("#Amlogic_inside Source-led")) == 0)) {
			ret = sscanf(ptr_line,
				     "#Amlogic_inside Source-led{%d, %d}",
				     &bri_off_1, &bri_off_2);
			if (ret == 2) {
				brightness_off[2][0] = bri_off_1;
				brightness_off[2][1] = bri_off_2;
				pr_info("update Source-led bri off: %d %d\n",
					bri_off_1, bri_off_2);
			}
		} else if ((strncmp(ptr_line, "#Amlogic_inside HDR OTT",
			strlen("#Amlogic_inside HDR OTT")) == 0)) {
			ret = sscanf(ptr_line,
				     "#Amlogic_inside HDR OTT{%d, %d}",
				     &bri_off_1, &bri_off_2);
			if (ret == 2) {
				brightness_off[3][0] = bri_off_1;
				brightness_off[3][1] = bri_off_2;
				pr_info("update HDR OTT bri off: %d %d\n",
					bri_off_1, bri_off_2);
			}
		} else if ((strncmp(ptr_line, "#Amlogic_inside HLG OTT",
			strlen("#Amlogic_inside HLG OTT")) == 0)) {
			ret = sscanf(ptr_line,
				     "#Amlogic_inside HLG OTT{%d, %d}",
				     &bri_off_1, &bri_off_2);
			if (ret == 2) {
				brightness_off[4][0] = bri_off_1;
				brightness_off[4][1] = bri_off_2;
				pr_info("update HLG OTT bri off: %d %d\n",
					bri_off_1, bri_off_2);
			}
		}
	}
}

/*cp bin data*/
static int cp_bin_data(void)
{
	int ret = 0;
	unsigned int bin_len = sizeof(struct pq_config) * MAX_DV_PICTUREMODES;
	int i = 0;
	unsigned int length = 0;

	if (!bin_to_cfg)
		bin_to_cfg = vmalloc(bin_len);
	if (!bin_to_cfg)
		return false;

	length = (bin_size > bin_len) ? bin_len : bin_size;
	num_picture_mode = length / sizeof(struct pq_config);

	if (num_picture_mode >
	    sizeof(cfg_info) / sizeof(struct dv_cfg_info_s)) {
		pr_dv_dbg("dv_config.bin size(%d) larger than cfg_info(%d)\n",
			num_picture_mode,
			(int)(sizeof(cfg_info) / sizeof(struct dv_cfg_info_s)));
		num_picture_mode =
			sizeof(cfg_info) / sizeof(struct dv_cfg_info_s);
	}
	if (num_picture_mode == 1)
		default_pic_mode = 0;

	memcpy((char *)bin_to_cfg, bin_data, length);

	for (i = 0; i < num_picture_mode; i++) {
		cfg_info[i].id = i;
		strcpy(cfg_info[i].pic_mode_name, "uninitialized");
	}

	pr_dv_dbg("cp bin size: %d\n", length);

	return ret;
}

/*cp cfg file*/
static int cp_cfg_data(void)
{
	int ret = 0;
	char *txt_buf = NULL;

	txt_buf = vmalloc(cfg_size);
	memset(txt_buf, 0, cfg_size);

	if (!txt_buf) {
		ret = -ENOMEM;
		goto release;
	}
	memcpy(txt_buf, cfg_data, cfg_size);
	get_picture_mode_info(txt_buf);

	pr_dv_dbg("cp cfg size: %d\n", cfg_size);

release:
	if (txt_buf)
		vfree(txt_buf);
	return ret;
}

static void get_user_cfg_info(char *cfg_buf)
{
	int ret = 0;
	int count = 0;
	int value = 0;
	int val = 0;
	int pri[8] = {0};
	bool eof_flag = false;
	char *u_cfg_buf = cfg_buf;
	char *ptr_line;

	eof_flag = false;
	while (!eof_flag) {
		eof_flag = get_one_line(&u_cfg_buf, (char *)&cur_line, true);
		ptr_line = cur_line;
		if (eof_flag && (strlen(cur_line) == 0))
			break;
		if ((strncmp(ptr_line, "[PictureMode",
			strlen("[PictureMode")) == 0)) {
			ret = sscanf(ptr_line, "[PictureMode = %d]", &count);
			if (ret == 1)
				user_cfg_info[count].id = count;
		}
		if ((strncmp(ptr_line, "Tmax",
			strlen("Tmax")) == 0)) {
			ret = sscanf(ptr_line, "Tmax = %d", &value);
			if (ret == 1)
				user_cfg_info[count].tmax = value;
		}
		if ((strncmp(ptr_line, "Tmin",
			strlen("Tmin")) == 0)) {
			ret = sscanf(ptr_line, "Tmin = 0.%d", &value);
			if (ret == 1)
				user_cfg_info[count].tmin = value;
		}
		if ((strncmp(ptr_line, "Tgamma",
			strlen("Tgamma")) == 0)) {
			ret = sscanf(ptr_line, "Tgamma = %d.%d", &value, &val);
			if (ret == 2)
				user_cfg_info[count].tgamma = value * 10 + val;
		}
		if ((strncmp(ptr_line, "TPrimaries",
			strlen("TPrimaries")) == 0)) {
			ret = sscanf(ptr_line,
				"TPrimaries = 0.%d 0.%d 0.%d 0.%d 0.%d 0.%d 0.%d 0.%d",
				&pri[0], &pri[1], &pri[2], &pri[3],
				&pri[4], &pri[5], &pri[6], &pri[7]);
			if (ret == 8) {
				user_cfg_info[count].tprimaries[0] = pri[0];
				user_cfg_info[count].tprimaries[1] = pri[1];
				user_cfg_info[count].tprimaries[2] = pri[2];
				user_cfg_info[count].tprimaries[3] = pri[3];
				user_cfg_info[count].tprimaries[4] = pri[4];
				user_cfg_info[count].tprimaries[5] = pri[5];
				user_cfg_info[count].tprimaries[6] = pri[6];
				user_cfg_info[count].tprimaries[7] = pri[7];
			}
		}
	}
}

bool load_dv_pq_config_data(char *bin_path, char *txt_path)
{
	struct file *filp = NULL;
	struct file *filp_txt = NULL;
	bool ret = true;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	struct kstat stat;
	unsigned int length = 0;
	unsigned int cfg_len = sizeof(struct pq_config) * MAX_DV_PICTUREMODES;
	char *txt_buf = NULL;
	int i = 0;
	int error;

	if (!bin_to_cfg)
		bin_to_cfg = vmalloc(cfg_len);
	if (!bin_to_cfg)
		return false;

	set_fs(KERNEL_DS);
	filp = filp_open(bin_path, O_RDONLY, 0444);
	if (IS_ERR(filp)) {
		ret = false;
		pr_info("[%s] failed to open file: |%s|\n", __func__, bin_path);
		goto LOAD_END;
	}

	error = vfs_stat(bin_path, &stat);
	if (error < 0) {
		ret = false;
		filp_close(filp, NULL);
		goto LOAD_END;
	}
	length = (stat.size > cfg_len) ? cfg_len : stat.size;
	num_picture_mode = length / sizeof(struct pq_config);

	if (num_picture_mode >
	    sizeof(cfg_info) / sizeof(struct dv_cfg_info_s)) {
		pr_info("dv_config.bin size(%d) bigger than cfg_info(%d)\n",
			num_picture_mode,
			(int)(sizeof(cfg_info) / sizeof(struct dv_cfg_info_s)));
		num_picture_mode =
			sizeof(cfg_info) / sizeof(struct dv_cfg_info_s);
	}
	if (num_picture_mode == 1)
		default_pic_mode = 0;
	vfs_read(filp, (char *)bin_to_cfg, length, &pos);

	for (i = 0; i < num_picture_mode; i++) {
		cfg_info[i].id = i;
		strcpy(cfg_info[i].pic_mode_name, "uninitialized");
	}

	/***read cfg txt to get picture mode name****/
	filp_txt = filp_open(txt_path, O_RDONLY, 0444);
	pos = 0;
	if (IS_ERR(filp_txt)) {
		ret = false;
		pr_info("[%s] failed to open file: |%s|\n", __func__, txt_path);
		filp_close(filp, NULL);
		goto LOAD_END;
	} else {
		error = vfs_stat(txt_path, &stat);
		if (error < 0) {
			ret = false;
			filp_close(filp, NULL);
			filp_close(filp_txt, NULL);
			goto LOAD_END;
		}

		length = stat.size;
		txt_buf = vmalloc(length + 2);
		memset(txt_buf, 0, length + 2);

		if (txt_buf) {
			vfs_read(filp_txt, txt_buf, length, &pos);
			get_picture_mode_info(txt_buf);
		}
	}
	/*************read cfg txt done***********/

	set_dv_pq_center();
	set_dv_pq_range();
	restore_dv_pq_setting(RESET_ALL);
	update_vsvdb_to_rx();

	filp_close(filp, NULL);
	filp_close(filp_txt, NULL);

	use_target_lum_from_cfg = true;
	load_bin_config = true;
	pr_info("DV config: load %d picture mode\n", num_picture_mode);

LOAD_END:
	if (txt_buf)
		vfree(txt_buf);
	set_fs(old_fs);
	return ret;
}

bool cp_dv_pq_config_data(void)
{
	if (cp_bin_data() >= 0 && cp_cfg_data() >= 0) {
		set_dv_pq_center();
		set_dv_pq_range();
		restore_dv_pq_setting(RESET_ALL);
		update_vsvdb_to_rx();
		use_target_lum_from_cfg = true;
		load_bin_config = true;
		pr_info("DV config: load %d picture mode\n", num_picture_mode);
	}
	return true;
}

/*load user cfg file*/
int load_user_pq_config_data(char *cfg_path)
{
	struct file *filp = NULL;
	bool ret = true;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	struct kstat stat;
	unsigned int length = 0;
	char *txt_buf = NULL;
	int error;

	/***read user cfg txt to get user cfg info****/
	set_fs(KERNEL_DS);
	filp = filp_open(cfg_path, O_RDONLY, 0444);
	if (IS_ERR(filp)) {
		ret = false;
		pr_info("[%s] failed to open file: |%s|\n", __func__, cfg_path);
		filp_close(filp, NULL);
		goto LOAD_END;
	} else {
		error = vfs_stat(cfg_path, &stat);
		if (error < 0) {
			ret = false;
			filp_close(filp, NULL);
			goto LOAD_END;
		}

		length = stat.size;
		txt_buf = vmalloc(length + 2);
		memset(txt_buf, 0, length + 2);

		if (txt_buf) {
			vfs_read(filp, txt_buf, length, &pos);
			get_user_cfg_info(txt_buf);
		}
	}
	/*************read user cfg txt done***********/

	calculate_user_pq_config();
	restore_dv_pq_setting(RESET_ALL);

	filp_close(filp, NULL);

LOAD_END:
	if (txt_buf)
		vfree(txt_buf);
	set_fs(old_fs);
	return ret;
}

bool user_cfg_to_bin(char *user_cfg_data, int user_cfg_size)
{
	int ret = 0;
	char *cfg_buf = NULL;

	cfg_buf = vmalloc(user_cfg_size + 2);
	memset(cfg_buf, 0, user_cfg_size + 2);

	if (!cfg_buf) {
		ret = -ENOMEM;
		goto release;
	}
	memcpy(cfg_buf, user_cfg_data, user_cfg_size);
	get_user_cfg_info(cfg_buf);
	calculate_user_pq_config();
	restore_dv_pq_setting(RESET_ALL);

release:
	if (cfg_buf)
		vfree(cfg_buf);
	return ret;
}

static inline s16 clamps(s16 value, s16 v_min, s16 v_max)
{
	if (value > v_max)
		return v_max;
	if (value < v_min)
		return v_min;
	return value;
}

/*internal range -> external range*/
static s16 map_pq_inter_to_exter
	(enum pq_item_e pq_item,
	 s16 inter_value)
{
	s16 inter_pq_min;
	s16 inter_pq_max;
	s16 exter_pq_min;
	s16 exter_pq_max;
	s16 exter_value;
	s16 inter_range;
	s16 exter_range;
	s16 tmp;
	s16 left_range = pq_range[pq_item].left;
	s16 right_range = pq_range[pq_item].right;
	s16 center = pq_center[cur_pic_mode][pq_item];

	if (inter_value >= center) {
		exter_pq_min = 0;
		exter_pq_max = EXTER_MAX_PQ;
		inter_pq_min = center;
		inter_pq_max = right_range;
	} else {
		exter_pq_min = EXTER_MIN_PQ;
		exter_pq_max = 0;
		inter_pq_min = left_range;
		inter_pq_max = center;
	}
	inter_pq_min = clamps(inter_pq_min, INTER_MIN_PQ, INTER_MAX_PQ);
	inter_pq_max = clamps(inter_pq_max, INTER_MIN_PQ, INTER_MAX_PQ);
	tmp = clamps(inter_value, inter_pq_min, inter_pq_max);

	inter_range = inter_pq_max - inter_pq_min;
	exter_range = exter_pq_max - exter_pq_min;

	if (inter_range == 0) {
		inter_range = INTER_MAX_PQ;
		pr_info("invalid inter_range, set to INTER_MAX_PQ\n");
	}

	exter_value = exter_pq_min +
		(tmp - inter_pq_min) * exter_range / inter_range;

	if (debug_dolby & 0x200)
		pr_info("%s: %s [%d]->[%d]\n",
			__func__, pq_item_str[pq_item],
			inter_value, exter_value);
	return exter_value;
}

/*external range -> internal range*/
static s16 map_pq_exter_to_inter
	(enum pq_item_e pq_item,
	 s16 exter_value)
{
	s16 inter_pq_min;
	s16 inter_pq_max;
	s16 exter_pq_min;
	s16 exter_pq_max;
	s16 inter_value;
	s16 inter_range;
	s16 exter_range;
	s16 tmp;
	s16 left_range = pq_range[pq_item].left;
	s16 right_range = pq_range[pq_item].right;
	s16 center = pq_center[cur_pic_mode][pq_item];

	if (exter_value >= 0) {
		exter_pq_min = 0;
		exter_pq_max = EXTER_MAX_PQ;
		inter_pq_min = center;
		inter_pq_max = right_range;
	} else {
		exter_pq_min = EXTER_MIN_PQ;
		exter_pq_max = 0;
		inter_pq_min = left_range;
		inter_pq_max = center;
	}

	inter_pq_min = clamps(inter_pq_min, INTER_MIN_PQ, INTER_MAX_PQ);
	inter_pq_max = clamps(inter_pq_max, INTER_MIN_PQ, INTER_MAX_PQ);
	tmp = clamps(exter_value, exter_pq_min, exter_pq_max);

	inter_range = inter_pq_max - inter_pq_min;
	exter_range = exter_pq_max - exter_pq_min;

	inter_value = inter_pq_min +
		(tmp - exter_pq_min) * inter_range / exter_range;

	if (debug_dolby & 0x200)
		pr_info("%s: %s [%d]->[%d]\n",
			__func__, pq_item_str[pq_item],
			exter_value, inter_value);
	return inter_value;
}

static bool is_valid_pic_mode(int mode)
{
	if (mode < num_picture_mode && mode >= 0)
		return true;
	else
		return false;
}

static bool is_valid_pq_exter_value(s16 exter_value)
{
	if (exter_value <= EXTER_MAX_PQ && exter_value >= EXTER_MIN_PQ)
		return true;

	pr_info("pq %d is out of range[%d, %d]\n",
		exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
		return false;
}

static bool is_valid_pq_inter_value(s16 exter_value)
{
	if (exter_value <= INTER_MAX_PQ && exter_value >= INTER_MIN_PQ)
		return true;

	pr_info("pq %d is out of range[%d, %d]\n",
		exter_value, INTER_MIN_PQ, INTER_MAX_PQ);
	return false;
}

void set_pic_mode(int mode)
{
	if (cur_pic_mode != mode) {
		cur_pic_mode = mode;
		if (dv_user_cfg_flag)
			calculate_user_pq_config();
		update_cp_cfg();
		update_vsvdb_to_rx();
		update_pwm_control();
	}
}

int get_pic_mode_num(void)
{
	return num_picture_mode;
}

int get_pic_mode(void)
{
	return cur_pic_mode;
}

char *get_pic_mode_name(int mode)
{
	if (!is_valid_pic_mode(mode))
		return "errmode";

	return cfg_info[mode].pic_mode_name;
}

char *get_cur_pic_mode_name(void)
{
	return cfg_info[cur_pic_mode].pic_mode_name;
}

s16 get_single_pq_value(int mode, enum pq_item_e item)
{
	s16 exter_value = 0;
	s16 inter_value = 0;

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return exter_value;
	}
	switch (item) {
	case PQ_BRIGHTNESS:
		inter_value = cfg_info[mode].brightness;
		break;
	case PQ_CONTRAST:
		inter_value = cfg_info[mode].contrast;
		break;
	case PQ_COLORSHIFT:
		inter_value = cfg_info[mode].colorshift;
		break;
	case PQ_SATURATION:
		inter_value = cfg_info[mode].saturation;
		break;
	default:
		pr_err("err pq_item %d\n", item);
		break;
	}

	exter_value = map_pq_inter_to_exter(item, inter_value);

	if (debug_dolby & 0x200)
		pr_info("%s: mode:%d, item:%s, [inter: %d, exter: %d]\n",
			__func__, mode, pq_item_str[item],
			inter_value, exter_value);
	return exter_value;
}

struct dv_full_pq_info_s get_full_pq_value(int mode)
{
	s16 exter_value = 0;
	struct dv_full_pq_info_s full_pq_info = {mode, 0, 0, 0, 0};

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return full_pq_info;
	}

	exter_value = map_pq_inter_to_exter(PQ_BRIGHTNESS,
					    cfg_info[mode].brightness);
	full_pq_info.brightness = exter_value;

	exter_value = map_pq_inter_to_exter(PQ_CONTRAST,
					    cfg_info[mode].contrast);
	full_pq_info.contrast = exter_value;

	exter_value = map_pq_inter_to_exter(PQ_COLORSHIFT,
					    cfg_info[mode].colorshift);
	full_pq_info.colorshift = exter_value;

	exter_value = map_pq_inter_to_exter(PQ_SATURATION,
					    cfg_info[mode].saturation);
	full_pq_info.saturation = exter_value;

	if (debug_dolby & 0x200) {
		pr_info("----------%s: mode:%d-----------\n", __func__, mode);
		pr_info("%s: value[inter: %d, exter: %d]\n",
			pq_item_str[PQ_BRIGHTNESS],
			cfg_info[mode].brightness,
			full_pq_info.brightness);
		pr_info("%s: value[inter: %d, exter: %d]\n",
			pq_item_str[PQ_CONTRAST],
			cfg_info[mode].contrast,
			full_pq_info.contrast);
		pr_info("%s: value[inter: %d, exter: %d]\n",
			pq_item_str[PQ_COLORSHIFT],
			cfg_info[mode].colorshift,
			full_pq_info.colorshift);
		pr_info("%s: value[inter: %d, exter: %d]\n",
			pq_item_str[PQ_SATURATION],
			cfg_info[mode].saturation,
			full_pq_info.saturation);
	}

	return full_pq_info;
}

void set_single_pq_value(int mode, enum pq_item_e item, s16 value)
{
	s16 inter_value = 0;
	s16 exter_value = value;
	bool pq_changed = false;

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return;
	}
	if (!use_inter_pq) {
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
				clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps %s to %d\n",
				pq_item_str[item], exter_value);
		}
		inter_value = map_pq_exter_to_inter(item, exter_value);
		if (debug_dolby & 0x200) {
			pr_info("%s: mode:%d, item:%s, [inter:%d, exter:%d]\n",
				__func__, mode, pq_item_str[item],
				inter_value, exter_value);
		}
	} else {
		inter_value = value;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
				clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps %s to %d\n",
				pq_item_str[item], inter_value);
		}
		if (debug_dolby & 0x200) {
			exter_value = map_pq_inter_to_exter(item, inter_value);
			pr_info("%s: mode:%d, item:%s, [inter:%d, exter:%d]\n",
				__func__, mode, pq_item_str[item],
				inter_value, exter_value);
		}
	}
	switch (item) {
	case PQ_BRIGHTNESS:
		if (cfg_info[cur_pic_mode].brightness != inter_value) {
			cfg_info[cur_pic_mode].brightness = inter_value;
			pq_changed = true;
		}
		break;
	case PQ_CONTRAST:
		if (cfg_info[cur_pic_mode].contrast != inter_value) {
			cfg_info[cur_pic_mode].contrast = inter_value;
			pq_changed = true;
		}
		break;
	case PQ_COLORSHIFT:
		if (cfg_info[cur_pic_mode].colorshift != inter_value) {
			cfg_info[cur_pic_mode].colorshift = inter_value;
			pq_changed = true;
		}
		break;
	case PQ_SATURATION:
		if (cfg_info[cur_pic_mode].saturation != inter_value) {
			cfg_info[cur_pic_mode].saturation = inter_value;
			pq_changed = true;
		}
		break;
	default:
		pr_err("err pq_item %d\n", item);
		break;
	}
	if (pq_changed)
		update_cp_cfg();
}

void set_full_pq_value(struct dv_full_pq_info_s full_pq_info)
{
	s16 inter_value = 0;
	s16 exter_value = 0;
	int mode = full_pq_info.pic_mode_id;

	if (!is_valid_pic_mode(mode)) {
		pr_err("err picture mode %d\n", mode);
		return;
	}
	/*------------set brightness----------*/
	if (!use_inter_pq) {
		exter_value = full_pq_info.brightness;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
				clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps brightness from %d to %d\n",
				full_pq_info.brightness, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_BRIGHTNESS, exter_value);
	} else {
		inter_value = full_pq_info.brightness;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
				clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps brightness from %d to %d\n",
				full_pq_info.brightness, inter_value);
		}
	}
	cfg_info[cur_pic_mode].brightness = inter_value;

	/*-------------set contrast-----------*/
	if (!use_inter_pq) {
		exter_value = full_pq_info.contrast;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
				clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps contrast from %d to %d\n",
				full_pq_info.contrast, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_CONTRAST, exter_value);
	} else {
		inter_value = full_pq_info.contrast;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
				clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps contrast from %d to %d\n",
				full_pq_info.contrast, inter_value);
		}
	}
	cfg_info[cur_pic_mode].contrast = inter_value;

	/*-------------set colorshift-----------*/
	if (!use_inter_pq) {
		exter_value = full_pq_info.colorshift;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
				clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps colorshift from %d to %d\n",
				full_pq_info.colorshift, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_COLORSHIFT, exter_value);
	} else {
		inter_value = full_pq_info.colorshift;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
				clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps colorshift from %d to %d\n",
				full_pq_info.colorshift, inter_value);
		}
	}
	cfg_info[cur_pic_mode].colorshift = inter_value;

	/*-------------set colorshift-----------*/
	if (!use_inter_pq) {
		exter_value = full_pq_info.saturation;
		if (!is_valid_pq_exter_value(exter_value)) {
			exter_value =
				clamps(exter_value, EXTER_MIN_PQ, EXTER_MAX_PQ);
			pr_info("clamps saturation from %d to %d\n",
				full_pq_info.saturation, exter_value);
		}
		inter_value = map_pq_exter_to_inter(PQ_SATURATION, exter_value);
	} else {
		inter_value = full_pq_info.saturation;
		if (!is_valid_pq_inter_value(inter_value)) {
			inter_value =
				clamps(inter_value, INTER_MIN_PQ, INTER_MAX_PQ);
			pr_info("clamps saturation from %d to %d\n",
				full_pq_info.saturation, inter_value);
		}
	}
	cfg_info[cur_pic_mode].saturation = inter_value;

	update_cp_cfg();

	if (debug_dolby & 0x200) {
		pr_info("----------%s: mode:%d----------\n", __func__, mode);
		exter_value = map_pq_inter_to_exter(PQ_BRIGHTNESS,
						    cfg_info[mode].brightness);
		pr_info("%s: [inter:%d, exter:%d]\n",
			pq_item_str[PQ_BRIGHTNESS],
			cfg_info[mode].brightness,
			exter_value);

		exter_value = map_pq_inter_to_exter(PQ_CONTRAST,
						    cfg_info[mode].contrast);
		pr_info("%s: [inter:%d, exter:%d]\n",
			pq_item_str[PQ_CONTRAST],
			cfg_info[mode].contrast,
			exter_value);

		exter_value = map_pq_inter_to_exter(PQ_COLORSHIFT,
						    cfg_info[mode].colorshift);
		pr_info("%s: [inter:%d, exter:%d]\n",
			pq_item_str[PQ_COLORSHIFT],
			cfg_info[mode].colorshift,
			exter_value);

		exter_value = map_pq_inter_to_exter(PQ_SATURATION,
						    cfg_info[mode].saturation);
		pr_info("%s: [inter:%d, exter:%d]\n",
			pq_item_str[PQ_SATURATION],
			cfg_info[mode].saturation,
			exter_value);
	}
}

int get_dv_pq_info(char *buf)
{
	int pos = 0;
	int exter_value = 0;
	int inter_value = 0;
	int i = 0;
	const char *pq_usage_str = {
	"\n"
	"------------------------- set pq usage ------------------------\n"
	"echo picture_mode value > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo brightness value   > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo contrast value     > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo colorshift value   > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo saturation value   > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo darkdetail value   > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo lightsense value   > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo all v1 v2 v3 v4 v5 > /sys/class/amdolby_vision/dv_pq_info;\n"
	"echo reset value        > /sys/class/amdolby_vision/dv_pq_info;\n"
	"\n"
	"picture_mode range: [0, num_picture_mode-1]\n"
	"brightness   range: [-256, 256]\n"
	"contrast     range: [-256, 256]\n"
	"colorshift   range: [-256, 256]\n"
	"saturation   range: [-256, 256]\n"
	"darkdetail   range: [0, 1]\n"
	"lightsense   range: [0, 1]\n"
	"reset            0: reset pict mode/all pq for all pict mode]\n"
	"                 1: reset pq for all picture mode]\n"
	"                 2: reset pq for current picture mode]\n"
	"---------------------------------------------------------------\n"
	};

	if (!load_bin_config) {
		pr_info("no load_bin_config\n");
		return 0;
	}

	if (!buf)
		return 0;

	pos += sprintf(buf + pos,
		       "\n==== show current picture mode info ====\n");

	pos += sprintf(buf + pos,
		       "num_picture_mode:          [%d]\n", num_picture_mode);
	for (i = 0; i < num_picture_mode; i++) {
		pos += sprintf(buf + pos,
			       "picture_mode   %d:          [%s]\n",
			       cfg_info[i].id, cfg_info[i].pic_mode_name);
	}
	pos += sprintf(buf + pos, "\n\n");
	pos += sprintf(buf + pos,
		       "current picture mode:      [%d]\n", cur_pic_mode);

	pos += sprintf(buf + pos,
		       "current picture mode name: [%s]\n",
		       cfg_info[cur_pic_mode].pic_mode_name);

	inter_value = cfg_info[cur_pic_mode].brightness;
	exter_value = map_pq_inter_to_exter(PQ_BRIGHTNESS, inter_value);
	pos += sprintf(buf + pos,
		       "current brightness:        [inter:%d][exter:%d]\n",
		       inter_value, exter_value);

	inter_value = cfg_info[cur_pic_mode].contrast;
	exter_value = map_pq_inter_to_exter(PQ_CONTRAST, inter_value);
	pos += sprintf(buf + pos,
		       "current contrast:          [inter:%d][exter:%d]\n",
		       inter_value, exter_value);

	inter_value = cfg_info[cur_pic_mode].colorshift;
	exter_value = map_pq_inter_to_exter(PQ_COLORSHIFT, inter_value);
	pos += sprintf(buf + pos,
		       "current colorshift:        [inter:%d][exter:%d]\n",
		       inter_value, exter_value);

	inter_value = cfg_info[cur_pic_mode].saturation;
	exter_value = map_pq_inter_to_exter(PQ_SATURATION, inter_value);
	pos += sprintf(buf + pos,
		       "current saturation:        [inter:%d][exter:%d]\n",
		       inter_value, exter_value);

	pos += sprintf(buf + pos,
		       "current darkdetail:        [%d]\n",
		       cfg_info[cur_pic_mode].dark_detail);

	pos += sprintf(buf + pos,
		       "current lightsense:        [%d]\n",
		       cfg_info[cur_pic_mode].light_sense);

	if (dv_user_cfg_flag) {
		pos += sprintf(buf + pos,
				"\n==== show end-user calibration cfg info ====\n");
		pos += sprintf(buf + pos,
					"current picture mode:      [%d]\n", cur_pic_mode);
		pos += sprintf(buf + pos,
					"current picture mode name: [%s]\n",
					cfg_info[cur_pic_mode].pic_mode_name);
		pos += sprintf(buf + pos,
					"user cfg Tgamma:           [%d/10]\n",
					user_cfg_info[cur_pic_mode].tgamma);
		pos += sprintf(buf + pos,
					"user cfg Tmax:             [%d]\n",
					user_cfg_info[cur_pic_mode].tmax);
		pos += sprintf(buf + pos,
					"user cfg Tmin:             [%d/1000]\n",
					user_cfg_info[cur_pic_mode].tmin);
		pos += sprintf(buf + pos,
					"user cfg TPrimaries:       ");
		pos += sprintf(buf + pos, "[0.%d][0.%d][0.%d][0.%d][0.%d][0.%d][0.%d][0.%d]\n\n",
					user_cfg_info[cur_pic_mode].tprimaries[0],
					user_cfg_info[cur_pic_mode].tprimaries[1],
					user_cfg_info[cur_pic_mode].tprimaries[2],
					user_cfg_info[cur_pic_mode].tprimaries[3],
					user_cfg_info[cur_pic_mode].tprimaries[4],
					user_cfg_info[cur_pic_mode].tprimaries[5],
					user_cfg_info[cur_pic_mode].tprimaries[6],
					user_cfg_info[cur_pic_mode].tprimaries[7]);
		pos += sprintf(buf + pos,
					"user bin gamma:            [%d]\n",
					user_target_config[cur_pic_mode].gamma);
		pos += sprintf(buf + pos,
					"user bin max_lin:          [%d]\n",
					user_target_config[cur_pic_mode].max_lin);
		pos += sprintf(buf + pos,
					"user bin max_line_dm3:     [%d]\n",
					user_target_config[cur_pic_mode].max_lin_dm3);
		pos += sprintf(buf + pos,
					"user bin max_pq:           [%d]\n",
					user_target_config[cur_pic_mode].max_pq);
		pos += sprintf(buf + pos,
					"user bin max_pq_dm3:       [%d]\n",
					user_target_config[cur_pic_mode].max_pq_dm3);
		pos += sprintf(buf + pos,
					"user bin min_lin:          [%d]\n",
					user_target_config[cur_pic_mode].min_lin);
		pos += sprintf(buf + pos,
					"user bin min_pq:           [%d]\n",
					user_target_config[cur_pic_mode].min_pq);
		pos += sprintf(buf + pos,
					"user bin tPrimaries:       ");
		pos += sprintf(buf + pos, "[%d][%d][%d][%d][%d][%d][%d][%d]\n",
					user_target_config[cur_pic_mode].t_primaries[0],
					user_target_config[cur_pic_mode].t_primaries[1],
					user_target_config[cur_pic_mode].t_primaries[2],
					user_target_config[cur_pic_mode].t_primaries[3],
					user_target_config[cur_pic_mode].t_primaries[4],
					user_target_config[cur_pic_mode].t_primaries[5],
					user_target_config[cur_pic_mode].t_primaries[6],
					user_target_config[cur_pic_mode].t_primaries[7]);
	}
	pos += sprintf(buf + pos, "========================================\n");

	pos += sprintf(buf + pos, "%s\n", pq_usage_str);

	return pos;
}

int set_dv_pq_info(const char *buf, size_t count)
{
	char *buf_orig;
	char *parm[MAX_PARAM] = {NULL};
	struct dv_full_pq_info_s full_pq_info;
	enum pq_reset_e pq_reset;
	int val = 0;
	int val1 = 0;
	int val2 = 0;
	int val3 = 0;
	int val4 = 0;
	int val5 = 0;
	int item = -1;
	int ret = count;

	if (!load_bin_config)
		return -EINVAL;
	if (!buf)
		return -EINVAL;

	buf_orig = kstrdup(buf, GFP_KERNEL);

	pr_info("%s: cmd: %s\n", __func__, buf_orig);
	parse_param_amdv(buf_orig, (char **)&parm);
	if (!parm[0] || !parm[1]) {
		ret = -EINVAL;
		goto ERR;
	}

	if (!strcmp(parm[0], "picture_mode")) {
		if (kstrtoint(parm[1], 10, &val) != 0) {
			ret = -EINVAL;
			goto ERR;
		}
		if (is_valid_pic_mode(val))
			set_pic_mode(val);
		else
			pr_info("err picture_mode\n");
	} else if (!strcmp(parm[0], "brightness")) {
		if (kstrtoint(parm[1], 10, &val) != 0) {
			ret = -EINVAL;
			goto ERR;
		}

		item = PQ_BRIGHTNESS;
	} else if (!strcmp(parm[0], "contrast")) {
		if (kstrtoint(parm[1], 10, &val) != 0) {
			ret = -EINVAL;
			goto ERR;
		}

		item = PQ_CONTRAST;
	} else if (!strcmp(parm[0], "colorshift")) {
		if (kstrtoint(parm[1], 10, &val) != 0) {
			ret = -EINVAL;
			goto ERR;
		}

		item = PQ_COLORSHIFT;
	} else if (!strcmp(parm[0], "saturation")) {
		if (kstrtoint(parm[1], 10, &val) != 0) {
			ret = -EINVAL;
			goto ERR;
		}
		item = PQ_SATURATION;
	} else if (!strcmp(parm[0], "all")) {
		if (kstrtoint(parm[1], 10, &val1) != 0 ||
		    kstrtoint(parm[2], 10, &val2) != 0 ||
		    kstrtoint(parm[3], 10, &val3) != 0 ||
		    kstrtoint(parm[4], 10, &val4) != 0 ||
		    kstrtoint(parm[5], 10, &val5) != 0) {
			ret = -EINVAL;
			goto ERR;
		}
		full_pq_info.pic_mode_id = val1;
		full_pq_info.brightness = val2;
		full_pq_info.contrast = val3;
		full_pq_info.colorshift = val4;
		full_pq_info.saturation = val5;
		set_full_pq_value(full_pq_info);
	} else if (!strcmp(parm[0], "reset")) {
		if (kstrtoint(parm[1], 10, &val) != 0) {
			ret = -EINVAL;
			goto ERR;
		}
		pq_reset = val;
		restore_dv_pq_setting(pq_reset);
	} else if (!strcmp(parm[0], "darkdetail")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;
		if (debug_dolby & 0x200)
			pr_info("[DV]: set mode %d darkdetail %d\n",
				cur_pic_mode, val);
		val = val > 0 ? 1 : 0;
		if (val != cfg_info[cur_pic_mode].dark_detail) {
			set_update_cfg(true);
			cfg_info[cur_pic_mode].dark_detail = val;
		}
	} else if (!strcmp(parm[0], "lightsense")) {
		if (kstrtoint(parm[1], 10, &val) != 0)
			goto ERR;
		if (debug_dolby & 0x200)
			pr_info("[DV]: set mode %d light sense %d\n",
				cur_pic_mode, val);
		val = val > 0 ? 1 : 0;
		if (val != cfg_info[cur_pic_mode].light_sense) {
			set_update_cfg(true);
			cfg_info[cur_pic_mode].light_sense = val;
		}
	} else {
		pr_info("unsupport cmd\n");
	}
	if (item != -1)
		set_single_pq_value(cur_pic_mode, item, val);
ERR:
	kfree(buf_orig);
	return ret;
}

void get_dv_bin_config(void)
{
	int mode = 0;
	struct pq_config *pq_config = NULL;
	struct target_config *config = NULL;

	if (!load_bin_config) {
		pr_info("no load_bin_config\n");
		return;
	}

	pr_info("There are %d picture modes\n", num_picture_mode);
	for (mode = 0; mode < num_picture_mode; mode++) {
		pr_info("================ Picture Mode: %s =================\n",
			cfg_info[mode].pic_mode_name);
		pq_config = &bin_to_cfg[mode];
		config = &pq_config->tdc;
		pr_info("gamma:                %d\n",
			config->gamma);
		pr_info("eotf:                 %d-%s\n",
			config->eotf,
			eotf_str[config->eotf]);
		pr_info("range_spec:           %d\n",
			config->range_spec);
		pr_info("max_pq:               %d\n",
			config->max_pq);
		pr_info("min_pq:               %d\n",
			config->min_pq);
		pr_info("max_pq_dm3:           %d\n",
			config->max_pq_dm3);
		pr_info("min_lin:              %d\n",
			config->min_lin);
		pr_info("max_lin:              %d\n",
			config->max_lin);
		pr_info("max_lin_dm3:          %d\n",
			config->max_lin_dm3);
		pr_info("tPrimaries:           %d,%d,%d,%d,%d,%d,%d,%d\n",
			config->t_primaries[0],
			config->t_primaries[1],
			config->t_primaries[2],
			config->t_primaries[3],
			config->t_primaries[4],
			config->t_primaries[5],
			config->t_primaries[6],
			config->t_primaries[7]);
		pr_info("m_sweight:             %d\n",
			config->m_sweight);
		pr_info("trim_slope_bias:       %d\n",
			config->trim_slope_bias);
		pr_info("trim_offset_bias:      %d\n",
			config->trim_offset_bias);
		pr_info("trim_power_bias:       %d\n",
			config->trim_power_bias);
		pr_info("ms_weight_bias:        %d\n",
			config->ms_weight_bias);
		pr_info("chroma_weight_bias:    %d\n",
			config->chroma_weight_bias);
		pr_info("saturation_gain_bias:  %d\n",
			config->saturation_gain_bias);
		pr_info("d_brightness:          %d\n",
			config->d_brightness);
		pr_info("d_contrast:            %d\n",
			config->d_contrast);
		pr_info("d_color_shift:         %d\n",
			config->d_color_shift);
		pr_info("d_saturation:          %d\n",
			config->d_saturation);
		pr_info("d_backlight:           %d\n",
			config->d_backlight);
		pr_info("dbg_exec_paramsprint:  %d\n",
			config->dbg_exec_params_print_period);
		pr_info("dbg_dm_md_print_period:%d\n",
			config->dbg_dm_md_print_period);
		pr_info("dbg_dmcfg_print_period:%d\n",
			config->dbg_dm_cfg_print_period);
		pr_info("\n");
		pr_info("------ Global Dimming configuration ------\n");
		pr_info("gd_enable:            %d\n",
			config->gd_config.gd_enable);
		pr_info("gd_wmin:              %d\n",
			config->gd_config.gd_wmin);
		pr_info("gd_wmax:              %d\n",
			config->gd_config.gd_wmax);
		pr_info("gd_wmm:               %d\n",
			config->gd_config.gd_wmm);
		pr_info("gd_wdyn_rng_sqrt:     %d\n",
			config->gd_config.gd_wdyn_rng_sqrt);
		pr_info("gd_weight_mean:       %d\n",
			config->gd_config.gd_weight_mean);
		pr_info("gd_weight_std:        %d\n",
			config->gd_config.gd_weight_std);
		pr_info("gd_delay_ms_hdmi:     %d\n",
			config->gd_config.gd_delay_msec_hdmi);
		pr_info("gd_rgb2yuv_ext:       %d\n",
			config->gd_config.gd_rgb2yuv_ext);
		pr_info("gd_wdyn_rng_sqrt:     %d\n",
			config->gd_config.gd_wdyn_rng_sqrt);
		pr_info("gd_m33_rgb2yuv:       %d,%d,%d\n",
			config->gd_config.gd_m33_rgb2yuv[0][0],
			config->gd_config.gd_m33_rgb2yuv[0][1],
			config->gd_config.gd_m33_rgb2yuv[0][2]);
		pr_info("                      %d,%d,%d\n",
			config->gd_config.gd_m33_rgb2yuv[1][0],
			config->gd_config.gd_m33_rgb2yuv[1][1],
			config->gd_config.gd_m33_rgb2yuv[1][2]);
		pr_info("                      %d,%d,%d\n",
			config->gd_config.gd_m33_rgb2yuv[2][0],
			config->gd_config.gd_m33_rgb2yuv[2][1],
			config->gd_config.gd_m33_rgb2yuv[2][2]);
		pr_info("gd_m33_rgb2yuv_scale2p:%d\n",
			config->gd_config.gd_m33_rgb2yuv_scale2p);
		pr_info("gd_rgb2yuv_off_ext:    %d\n",
			config->gd_config.gd_rgb2yuv_off_ext);
		pr_info("gd_rgb2yuv_off:        %d,%d,%d\n",
			config->gd_config.gd_rgb2yuv_off[0],
			config->gd_config.gd_rgb2yuv_off[1],
			config->gd_config.gd_rgb2yuv_off[2]);
		pr_info("gd_up_bound:           %d\n",
			config->gd_config.gd_up_bound);
		pr_info("gd_low_bound:          %d\n",
			config->gd_config.gd_low_bound);
		pr_info("last_max_pq:           %d\n",
			config->gd_config.last_max_pq);
		pr_info("gd_wmin_pq:            %d\n",
			config->gd_config.gd_wmin_pq);
		pr_info("gd_wmax_pq:            %d\n",
			config->gd_config.gd_wmax_pq);
		pr_info("gd_wm_pq:              %d\n",
			config->gd_config.gd_wm_pq);
		pr_info("gd_trigger_period:     %d\n",
			config->gd_config.gd_trigger_period);
		pr_info("gd_trigger_lin_thresh: %d\n",
			config->gd_config.gd_trigger_lin_thresh);
		pr_info("gdDelayMilliSec_ott:   %d\n",
			config->gd_config.gd_delay_msec_ott);
		pr_info("gd_rise_weight:        %d\n",
			config->gd_config.gd_rise_weight);
		pr_info("gd_fall_weight:        %d\n",
			config->gd_config.gd_fall_weight);
		pr_info("gd_delay_msec_ll:      %d\n",
			config->gd_config.gd_delay_msec_ll);
		pr_info("gd_contrast:           %d\n",
			config->gd_config.gd_contrast);
		pr_info("\n");

		pr_info("------ Adaptive boost configuration ------\n");
		pr_info("ab_enable:             %d\n",
			config->ab_config.ab_enable);
		pr_info("ab_highest_tmax:       %d\n",
			config->ab_config.ab_highest_tmax);
		pr_info("ab_lowest_tmax:        %d\n",
			config->ab_config.ab_lowest_tmax);
		pr_info("ab_rise_weight:        %d\n",
			config->ab_config.ab_rise_weight);
		pr_info("ab_fall_weight:        %d\n",
			config->ab_config.ab_fall_weight);
		pr_info("ab_delay_msec_hdmi:    %d\n",
			config->ab_config.ab_delay_msec_hdmi);
		pr_info("ab_delay_msec_ott:     %d\n",
			config->ab_config.ab_delay_msec_ott);
		pr_info("ab_delay_msec_ll:      %d\n",
			config->ab_config.ab_delay_msec_ll);
		pr_info("\n");

		pr_info("------- Ambient light configuration ------\n");
		pr_info("dark_detail:        %d\n",
			config->ambient_config.dark_detail);
		pr_info("dark_detail_complum:%d\n",
			config->ambient_config.dark_detail_complum);
		pr_info("ambient:            %d\n",
			config->ambient_config.ambient);
		pr_info("t_front_lux:        %d\n",
			config->ambient_config.t_front_lux);
		pr_info("t_front_lux_scale:  %d\n",
			config->ambient_config.t_front_lux_scale);
		pr_info("t_rear_lum:         %d\n",
			config->ambient_config.t_rear_lum);
		pr_info("t_rear_lum_scale:   %d\n",
			config->ambient_config.t_rear_lum_scale);
		pr_info("t_whitexy:          %d, %d\n",
			config->ambient_config.t_whitexy[0],
			config->ambient_config.t_whitexy[1]);
		pr_info("t_surround_reflection:%d\n",
			config->ambient_config.t_surround_reflection);
		pr_info("t_screen_reflection:%d\n",
			config->ambient_config.t_screen_reflection);
		pr_info("al_delay:           %d\n",
			config->ambient_config.al_delay);
		pr_info("al_rise:            %d\n",
			config->ambient_config.al_rise);
		pr_info("al_fall:            %d\n",
			config->ambient_config.al_fall);

		pr_info("vsvdb:              %x,%x,%x,%x,%x,%x,%x\n",
			config->vsvdb[0],
			config->vsvdb[1],
			config->vsvdb[2],
			config->vsvdb[3],
			config->vsvdb[4],
			config->vsvdb[5],
			config->vsvdb[6]);
		pr_info("\n");
		pr_info("---------------- ocsc_config --------------\n");
		pr_info("lms2rgb_mat:        %d,%d,%d\n",
			config->ocsc_config.lms2rgb_mat[0][0],
			config->ocsc_config.lms2rgb_mat[0][1],
			config->ocsc_config.lms2rgb_mat[0][2]);
		pr_info("                    %d,%d,%d\n",
			config->ocsc_config.lms2rgb_mat[1][0],
			config->ocsc_config.lms2rgb_mat[1][1],
			config->ocsc_config.lms2rgb_mat[1][2]);
		pr_info("                    %d,%d,%d\n",
			config->ocsc_config.lms2rgb_mat[2][0],
			config->ocsc_config.lms2rgb_mat[2][1],
			config->ocsc_config.lms2rgb_mat[2][2]);
		pr_info("lms2rgb_mat_scale:  %d\n",
			config->ocsc_config.lms2rgb_mat_scale);
		pr_info("\n");

		pr_info("dm31_avail:         %d\n",
			config->dm31_avail);
		pr_info("bright_preservation:%d\n",
			config->bright_preservation);
		pr_info("total_viewing_modes:%d\n",
			config->total_viewing_modes_num);
		pr_info("viewing_mode_valid: %d\n",
			config->viewing_mode_valid);
		pr_info("\n");
	}
}

int set_dv_debug_tprimary(const char *buf, size_t count)
{
	char *buf_orig;
	char *parm[MAX_PARAM] = {NULL};

	if (!buf)
		return -EINVAL;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amdv(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "primary_r")) {
		if (kstrtoint(parm[1], 10, &cur_debug_tprimary[0][0]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoint(parm[2], 10, &cur_debug_tprimary[0][1]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	} else if (!strcmp(parm[0], "primary_g")) {
		if (kstrtoint(parm[1], 10, &cur_debug_tprimary[1][0]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoint(parm[2], 10, &cur_debug_tprimary[1][1]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	} else if (!strcmp(parm[0], "primary_b")) {
		if (kstrtoint(parm[1], 10, &cur_debug_tprimary[2][0]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoint(parm[2], 10, &cur_debug_tprimary[2][1]) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	} else if (!strcmp(parm[0], "debug_tprimary")) {
		if (kstrtoint(parm[1], 10, &debug_tprimary) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	}
	kfree(buf_orig);
	update_cp_cfg();
	return count;
}

int get_dv_debug_tprimary(char *buf)
{
	return snprintf(buf, 100, "debug %d, cur_debug_tprimary %d %d %d %d %d %d\n",
		debug_tprimary,
		cur_debug_tprimary[0][0], cur_debug_tprimary[0][1],
		cur_debug_tprimary[1][0], cur_debug_tprimary[1][1],
		cur_debug_tprimary[2][0], cur_debug_tprimary[2][1]);
}

int get_inter_pq_flag(void)
{
	return use_inter_pq;
}

void set_inter_pq_flag(int flag)
{
	use_inter_pq = flag;
}

bool get_load_config_status(void)
{
	return load_bin_config;
}

void set_load_config_status(bool flag)
{
	load_bin_config = flag;
}

module_param(panel_max_lumin, uint, 0664);
MODULE_PARM_DESC(panel_max_lumin, "\n panel_max_lumin\n");

