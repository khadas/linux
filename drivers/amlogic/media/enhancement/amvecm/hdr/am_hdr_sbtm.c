// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/amlogic/media/amvecm/hdr10_tmo_alg.h>

#include "am_hdr_sbtm.h"
#include "gamut_convert.h"
#include "../set_hdr2_v0.h"
//#include "../../../../drm/meson_plane.h"
uint pr_sbtm_en;
module_param(pr_sbtm_en, int, 0664);
MODULE_PARM_DESC(pr_sbtm_en, "\n pr_sbtm_en\n");

#define pr_sbtm_dbg(fmt, args...)\
	do {\
		if (pr_sbtm_en)\
			pr_info("SBTM: " fmt, ##args);\
	} while (0)

#define NORM 50000
static u32 std_bt2020_prmy[3][2] = {
	{0.17 * NORM + 0.5, 0.797 * NORM + 0.5},	/* G */
	{0.131 * NORM + 0.5, 0.046 * NORM + 0.5},	/* B */
	{0.708 * NORM + 0.5, 0.292 * NORM + 0.5},	/* R */
};

static u32 std_bt2020_white_point[2] = {
	0.3127 * NORM + 0.5, 0.3290 * NORM + 0.5
};

static u32 std_p3_primaries[3][2] = {
	{0.265 * NORM + 0.5, 0.69 * NORM + 0.5},	/* G */
	{0.15 * NORM + 0.5, 0.06 * NORM + 0.5},		/* B */
	{0.68 * NORM + 0.5, 0.32 * NORM + 0.5},		/* R */
};

static u32 std_p3_white_point[2] = {
	0.3127 * NORM + 0.5, 0.3290 * NORM + 0.5
};

static u32 std_bt709_prmy[3][2] = {
	{0.30 * NORM + 0.5, 0.60 * NORM + 0.5},	/* G */
	{0.15 * NORM + 0.5, 0.06 * NORM + 0.5},	/* B */
	{0.64 * NORM + 0.5, 0.33 * NORM + 0.5},	/* R */
};

static u32 std_bt709_white_point[2] = {
	0.3127 * NORM + 0.5, 0.3290 * NORM + 0.5
};

int ncl_709_2020_sbtm[9] = {1285, 674, 89, 142, 1883, 23, 34, 180, 1834};

#define LUT_SHIFT 520
static u16 linear_e_to_o_sbtm[504] = {
	100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
	110, 111, 112, 113, 114, 115, 117, 118, 119, 120,
	121, 122, 123, 125, 126, 127, 128, 130, 131, 132,
	133, 135, 136, 137, 139, 140, 141, 143, 144, 145,
	147, 148, 150, 151, 152, 154, 155, 157, 158, 160,
	161, 163, 165, 166, 168, 169, 171, 172, 174, 176,
	177, 179, 181, 183, 184, 186, 188, 190, 191, 193,
	195, 197, 199, 201, 202, 204, 206, 208, 210, 212,
	214, 216, 218, 220, 222, 224, 226, 229, 231, 233,
	235, 237, 239, 242, 244, 246, 249, 251, 253, 256,
	258, 260, 263, 265, 268, 270, 273, 275, 278, 280,
	283, 286, 288, 291, 294, 296, 299, 302, 305, 307,
	310, 313, 316, 319, 322, 325, 328, 331, 334, 337,
	340, 343, 346, 350, 353, 356, 359, 363, 366, 369,
	373, 376, 380, 383, 387, 390, 394, 397, 401, 405,
	408, 412, 416, 420, 424, 427, 431, 435, 439, 443,
	447, 451, 456, 460, 464, 468, 472, 477, 481, 485,
	490, 494, 499, 503, 508, 513, 517, 522, 527, 532,
	536, 541, 546, 551, 556, 561, 566, 571, 577, 582,
	587, 592, 598, 603, 609, 614, 620, 625, 631, 637,
	643, 648, 654, 660, 666, 672, 678, 684, 691, 697,
	703, 710, 716, 722, 729, 736, 742, 749, 756, 763,
	769, 776, 783, 791, 798, 805, 812, 819, 827, 834,
	842, 849, 857, 865, 873, 881, 888, 896, 905, 913,
	921, 929, 938, 946, 955, 963, 972, 981, 990, 998,
	1007, 1017, 1026, 1035, 1044, 1054, 1063, 1073, 1082, 1092,
	1102, 1112, 1122, 1132, 1142, 1152, 1163, 1173, 1184, 1195,
	1205, 1216, 1227, 1238, 1249, 1261, 1272, 1283, 1295, 1307,
	1318, 1330, 1342, 1354, 1366, 1379, 1391, 1404, 1416, 1429,
	1442, 1455, 1468, 1481, 1495, 1508, 1522, 1535, 1549, 1563,
	1577, 1591, 1606, 1620, 1635, 1649, 1664, 1679, 1694, 1710,
	1725, 1741, 1756, 1772, 1788, 1804, 1820, 1837, 1853, 1870,
	1887, 1904, 1921, 1938, 1956, 1974, 1991, 2009, 2027, 2046,
	2064, 2083, 2102, 2121, 2140, 2159, 2178, 2198, 2218, 2238,
	2258, 2279, 2299, 2320, 2341, 2362, 2383, 2405, 2427, 2448,
	2471, 2493, 2515, 2538, 2561, 2584, 2608, 2631, 2655, 2679,
	2703, 2728, 2753, 2777, 2803, 2828, 2854, 2879, 2905, 2932,
	2958, 2985, 3012, 3040, 3067, 3095, 3123, 3151, 3180, 3209,
	3238, 3267, 3297, 3327, 3357, 3388, 3418, 3449, 3481, 3512,
	3544, 3577, 3609, 3642, 3675, 3708, 3742, 3776, 3811, 3845,
	3880, 3916, 3951, 3987, 4024, 4061, 4098, 4135, 4173, 4211,
	4249, 4288, 4327, 4367, 4407, 4447, 4487, 4528, 4570, 4612,
	4654, 4696, 4739, 4783, 4827, 4871, 4915, 4961, 5006, 5052,
	5098, 5145, 5192, 5240, 5288, 5337, 5386, 5435, 5485, 5535,
	5586, 5638, 5690, 5742, 5795, 5848, 5902, 5956, 6011, 6067,
	6123, 6179, 6236, 6294, 6352, 6410, 6470, 6529, 6590, 6651,
	6712, 6774, 6837, 6900, 6964, 7028, 7094, 7159, 7226, 7293,
	7360, 7429, 7498, 7567, 7637, 7708, 7780, 7852, 7925, 7999,
	8074, 8149, 8225, 8301, 8379, 8457, 8536, 8615, 8696, 8777,
	8859, 8941, 9025, 9109, 9195, 9281, 9367, 9455, 9544, 9633,
	9723, 9815, 9907, 10000
};

/* SDR convert to 310 gain=512*310/10000 lumin HDR, 512 as 1.0 */
int oo_y_lut_sdr_hdr_300_sbtm[HDR2_OOTF_LUT_SIZE] = {
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16
};

/* SDR convert to 1000 gain=512*1000/10000 lumin HDR, 512 as 1.0 */
int oo_y_lut_sdr_hdr_1000_sbtm[HDR2_OOTF_LUT_SIZE] = {
	51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
	51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
	51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
	51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
	51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
	51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
	51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
	51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
	51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
	51, 51, 51, 51, 51
};

int oo_y_lut_bypass_sbtm[HDR2_OOTF_LUT_SIZE] = {
	512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
	512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
	512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
	512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
	512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
	512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
	512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
	512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
	512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
	512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
	512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512, 512,
	512, 512, 512, 512, 512, 512
};

static u32 oo_lut[HDR2_OOTF_LUT_SIZE] = {0};
static int num_oo_y_lut_sbtm = HDR2_OOTF_LUT_SIZE;
int oo_y_lut_sbtm[HDR2_OOTF_LUT_SIZE] = {
	1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600,
	1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600,
	1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600,
	1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600,
	1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600,
	1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600,
	1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600,
	1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600,
	1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600,
	1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1600, 1532,
	1440, 1287, 1165, 1065, 982, 911, 850, 798, 752, 675,
	613, 563, 520, 484, 454, 427, 403, 364, 332, 306,
	283, 265, 248, 234, 221, 199, 182, 167, 154, 144,
	134, 126, 119, 113, 107, 102, 97, 93, 89, 86,
	82, 79, 77, 74, 72, 69, 67, 65, 64
};
module_param_array(oo_y_lut_sbtm, int, &num_oo_y_lut_sbtm, 0664);
MODULE_PARM_DESC(oo_y_lut_sbtm, "\n oo_y_lut_sbtm\n");

static int num_oo_y_lut_sbtm_osd = HDR2_OOTF_LUT_SIZE;
int oo_y_lut_sbtm_osd[HDR2_OOTF_LUT_SIZE] = {
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16
};
module_param_array(oo_y_lut_sbtm_osd, int, &num_oo_y_lut_sbtm_osd, 0664);
MODULE_PARM_DESC(oo_y_lut_sbtm_osd, "\n oo_y_lut_sbtm_osd\n");

static unsigned int eo_lut_28_sbtm[143] = {
	0, 1, 4, 7, 12, 17, 24, 31, 39, 49, 59, 71, 83, 97,
	111, 127, 144, 320, 578, 931, 1392, 1977, 2701, 3582,
	4638, 5891, 7362, 9076, 11058, 13337, 15942, 18907,
	22266, 26058, 30324, 35106, 40454, 46418, 53052, 60415,
	68572, 77590, 87542, 98508, 110570, 123821, 138356, 154282,
	171708, 190757, 211556, 234243, 258967, 285886, 315170, 347002,
	381578, 419105, 459810, 503932, 551729, 603476, 659470, 720026,
	785484, 856207, 932584, 1015032, 1103997, 1199956, 1303424,
	1414947, 1535113, 1664552, 1803938, 1953994, 2115494, 2289267,
	2476202, 2677251, 2893435, 3125848, 3375662, 3644136, 3932616,
	4242547, 4575481, 4933078, 5317122, 5729526, 6172341, 6647771,
	7158178, 7706100, 8294261, 8925586, 9603216, 10330526, 11111139,
	11948953, 12848153, 13813241, 14849055, 15960799, 17154072, 18434897,
	19809755, 21285624, 22870015, 24571018, 26397348, 28358395, 30464279,
	32725910, 35155053, 37764399, 40567642, 43579559, 46816107, 50294518,
	54033406, 58052888, 62374710, 67022388, 72021358, 77399146, 83185550,
	89412833, 96115952, 103332784, 111104394, 119475320, 128493881, 138212527,
	148688209, 159982795, 172163524, 185303500, 199482248, 214786307,
	231309901, 249155666, 268435456
};

int init_lut_sbtm[13] = {
	0, 128, 256, 384, 512, 576, 640, 704, 768, 832, 896, 960, 1024
};

/*
 * static struct aml_tmo_reg_sw_hdr tmo_reg_sbtm_hdrfunc = {
 * .pre_hdr10_tmo_alg = NULL,
 * };
 */

static struct aml_tmo_reg_sw tmo_reg_sbtm = {
	.tmo_en = 1,
	.reg_highlight = 185,
	.reg_light_th = 110,
	.reg_hist_th = 120,
	.reg_highlight_th1 = 70,
	.reg_highlight_th2 = 12,
	.reg_display_e = 700,
	.reg_middle_a = 102,
	.reg_middle_a_adj = 315,
	.reg_middle_b = 27,
	.reg_middle_s = 64,
	.reg_max_th1 = 50,
	.reg_middle_th = 500,
	.reg_thold1 = 300,
	.reg_thold2 = 30,
	.reg_thold3 = 100,
	.reg_thold4 = 250,
	.reg_max_th2 = 18,
	.reg_pnum_th = 5000,
	.reg_hl0 = 60,
	.reg_hl1 = 38,
	.reg_hl2 = 102,
	.reg_hl3 = 80,
	.reg_display_adj = 115,
	.reg_low_adj = 38,
	.reg_high_en = 2,
	.reg_high_adj1 = 13,
	.reg_high_adj2 = 77,
	.reg_high_maxdiff = 64,
	.reg_high_mindiff = 8,
	.reg_avg_th = 0,
	.reg_avg_adj = 150,
	.alpha = 250,
	.reg_ratio = 870,
	.reg_max_th3 = -200,
	.eo_lut = eo_lut_28_sbtm,
	.oo_init_lut = init_lut_sbtm,

	/*param for alg func*/
	.w = 3840,
	.h = 2160,
	.oo_gain = NULL,
	.pre_hdr10_tmo_alg = NULL,
};

static struct sbtm_info sbtmdb_reg = {
	.sbtm_support = 0,
	.max_sbtm_ver = 0,
	.grdm_support = 2,
	.drdm_ind = 1,
	.hgig_cat_drdm_sel = 0,
	.use_hgig_drdm = 0,
	.maxrgb = 1,
	.gamut = 0,
	.red_x = 0x8a48,
	.red_y = 0x3908,
	.green_x = 0x2134,
	.green_y = 0x9baa,
	.blue_x = 0x1996,
	.blue_y = 0x8fc,
	.white_x = 0x3d13,
	.white_y = 0x4042,
	.min_bright_10 = 0,
	.peak_bright_100 = 0x94,
	.p0_exp = 0,
	.p0_mant = 0x10,
	.peak_bright_p0 = 0x9f,
	.p1_exp = 0,
	.p1_mant = 0x08,
	.peak_bright_p1 = 0xa7,
	.p2_exp = 1,
	.p2_mant = 0x20,
	.peak_bright_p2 = 0xb8,
	.p3_exp = 1,
	.p3_mant = 0x08,
	.peak_bright_p3 = 0xc0,
};

struct sbtmdb_tgt_parm sbtmdb_reg_tgt = {
	{{0, 0}, {0, 0}, {0, 0}, {0, 0}}, 1000,
	{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
	0, 1000, 0};

struct vtem_sbtm_st sbtmem_reg = {
	.sbtm_ver = 0,
	.sbtm_mode = 0,
	.sbtm_type = 3,
	.grdm_min = 0,
	.grdm_lum = 0,
	.frmpblimitint = 0x0,
};

u8 sbtm_hdr_process;

uint sbtmdb_auto_update = 1;/* sbtmdb update use edid info */
module_param(sbtmdb_auto_update, uint, 0664);
MODULE_PARM_DESC(sbtmdb_auto_update, "\n sbtmdb_auto_update\n");

uint sbtm_tmo_static;/* sbtm_tmo_static 0/1 dynamic/static */
module_param(sbtm_tmo_static, uint, 0664);
MODULE_PARM_DESC(sbtm_tmo_static, "\n sbtm_tmo_static\n");

uint sbtm_en;/* sbtm_en enable/disable */
module_param(sbtm_en, uint, 0664);
MODULE_PARM_DESC(sbtm_en, "\n sbtm_en\n");

void sbtm_set_sbtm_enable(unsigned int enable)
{
	sbtm_en = enable;
}
EXPORT_SYMBOL(sbtm_set_sbtm_enable);

uint cur_sbtm_mode = SBTM_MODE_OFF;
uint sbtm_mode = SBTM_MODE_OFF;
module_param(sbtm_mode, uint, 0664);
MODULE_PARM_DESC(sbtm_mode, "\n sbtm_mode\n");

void sbtm_mode_set(uint mode)
{
	if (!sbtmdb_reg.drdm_ind && !sbtmdb_reg.grdm_support)
		sbtm_mode = SBTM_MODE_OFF;
	else if (sbtmdb_reg.drdm_ind && !sbtmdb_reg.grdm_support)
		sbtm_mode = SBTM_MODE_DRDM;
	else if (!sbtmdb_reg.drdm_ind && sbtmdb_reg.grdm_support)
		sbtm_mode = SBTM_MODE_GRDM;
	else
		sbtm_mode = mode;
}
EXPORT_SYMBOL(sbtm_mode_set);

unsigned int hdr10_tmo_debug_sbtm;
module_param(hdr10_tmo_debug_sbtm, uint, 0664);
MODULE_PARM_DESC(hdr10_tmo_debug_sbtm, "\n hdr10_tmo_debug_sbtm\n");

unsigned int tmo_alpha = 250;
module_param(tmo_alpha, uint, 0664);
MODULE_PARM_DESC(tmo_alpha, "\n tmo_alpha\n");

static int _get_maxl_e(int maxl)
{
	int i;
	int maxl_e = 0;

	if (maxl > 10000) {
		pr_sbtm_dbg("maxl over 10000. invalid maxl\n");
		maxl_e = 1023;
		return maxl_e;
	}

	for (i = 0; i < 504; i++) {
		if (linear_e_to_o_sbtm[i] >= maxl) {
			maxl_e = i + LUT_SHIFT;
			break;
		}
	}

	/* pr_sbtm_dbg("maxl = %d, maxl_e = %d\n", maxl, maxl_e);*/

	return maxl_e;
}

static int _get_maxl_8b(int maxl_e_8bit)
{
	int idx;
	int maxl = 0;

	if (maxl_e_8bit < 130) {
		pr_sbtm_dbg("maxl lower than 100, use 100 nits.\n");
		maxl  = 100;
		return maxl;
	}

	idx = (maxl_e_8bit << 2) - 520;
	if (idx >= 504)
		idx = 504 - 1;

	maxl = linear_e_to_o_sbtm[idx];

	return maxl;
}

void sbtm_dest_primary_update(void)
{
	int i, j;
	u8 gamut_type;

	if (sbtm_mode == SBTM_MODE_GRDM) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++) {
				sbtmdb_reg_tgt.dest_primary[i][j] =
					std_bt2020_prmy[(i + 2) % 3][j];
				sbtmdb_reg_tgt.dest_primary[3][j] =
					std_bt2020_white_point[j];
			}
	} else if (sbtm_mode == SBTM_MODE_DRDM) {
		gamut_type = sbtmdb_reg.gamut;
		switch (gamut_type) {
		case SBTM_DRDM_GAMUT_SPECIFIED:
			sbtmdb_reg_tgt.dest_primary[0][0] = sbtmdb_reg.red_x;
			sbtmdb_reg_tgt.dest_primary[0][1] = sbtmdb_reg.red_y;
			sbtmdb_reg_tgt.dest_primary[1][0] = sbtmdb_reg.green_x;
			sbtmdb_reg_tgt.dest_primary[1][1] = sbtmdb_reg.green_y;
			sbtmdb_reg_tgt.dest_primary[2][0] = sbtmdb_reg.blue_x;
			sbtmdb_reg_tgt.dest_primary[2][1] = sbtmdb_reg.blue_y;
			sbtmdb_reg_tgt.dest_primary[3][0] = sbtmdb_reg.white_x;
			sbtmdb_reg_tgt.dest_primary[3][1] = sbtmdb_reg.white_y;
			break;
		case SBTM_DRDM_GAMUT_709:
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++) {
					sbtmdb_reg_tgt.dest_primary[i][j] =
						std_bt709_prmy[(i + 2) % 3][j];
					sbtmdb_reg_tgt.dest_primary[3][j] =
						std_bt709_white_point[j];
				}
			break;
		case SBTM_DRDM_GAMUT_P3:
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++) {
					sbtmdb_reg_tgt.dest_primary[i][j] =
						std_p3_primaries[(i + 2) % 3][j];
					sbtmdb_reg_tgt.dest_primary[3][j] =
						std_p3_white_point[j];
				}
			break;
		case SBTM_DRDM_GAMUT_2020:
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++) {
					sbtmdb_reg_tgt.dest_primary[i][j] =
						std_bt2020_prmy[(i + 2) % 3][j];
					sbtmdb_reg_tgt.dest_primary[3][j] =
						std_bt2020_white_point[j];
				}
			break;
		default:
			break;
		}
	}
	pr_sbtm_dbg("%s: gamut type = %d.\n", __func__, sbtmdb_reg.gamut);
}

void sbtm_grdm_peaklum_update(void)
{
	if (sbtmdb_reg.grdm_support == SBTM_GRDM_SDR)
		sbtmdb_reg_tgt.tgt_maxl = 300;
	else if (sbtmdb_reg.grdm_support == SBTM_GRDM_HDR)
		sbtmdb_reg_tgt.tgt_maxl = 1000;
	else
		sbtmdb_reg_tgt.tgt_maxl = 300;
	tmo_reg_sbtm.reg_display_e = _get_maxl_e(sbtmdb_reg_tgt.tgt_maxl);
/*
 * pr_sbtm_dbg("%s:tgt_maxl = %d, reg_display_e = %d\n",
 * __func__, sbtmdb_reg_tgt.tgt_maxl, tmo_reg_sbtm.reg_display_e);
 */
}

u32 sbtm_cal_pbpct(u32 p_exp, u32 p_mant)
{
	u32 pbpct;
	int tmp_exp_idx;
	u32 divisor_exp = 0;
	u64 tmp_p_mant;

	tmp_exp_idx = p_exp;
	tmp_p_mant = p_mant;

	while (tmp_exp_idx)
		divisor_exp = 10 * tmp_exp_idx--;
	if (divisor_exp == 0)
		divisor_exp = 1;

	/*pct * 10000*/
	pbpct = div64_u64(div64_u64(tmp_p_mant * 10000, 64) + (divisor_exp >> 1), divisor_exp);

	return pbpct;
}

void sbtm_pb_pct_nits_update(void)
{
	u8 hgig_sel;
	int i;

	if (sbtmdb_reg.use_hgig_drdm) {
		hgig_sel = sbtmdb_reg.hgig_cat_drdm_sel;
		switch (hgig_sel) {
		case SBTM_DRDM_HGIG_SEL_0:
			sbtmdb_reg_tgt.numpb = 4;
			sbtmdb_reg_tgt.pbpct[0] = sbtm_cal_pbpct(1, 0x08);
			sbtmdb_reg_tgt.pbnits[0] = _get_maxl_8b(0xc0);
			sbtmdb_reg_tgt.pbpct[1] = sbtm_cal_pbpct(1, 0x20);
			sbtmdb_reg_tgt.pbnits[1] = _get_maxl_8b(0xb8);
			sbtmdb_reg_tgt.pbpct[2] = sbtm_cal_pbpct(0, 0x08);
			sbtmdb_reg_tgt.pbnits[2] = _get_maxl_8b(0xa7);
			sbtmdb_reg_tgt.pbpct[3] = sbtm_cal_pbpct(0, 0x10);
			sbtmdb_reg_tgt.pbnits[3] = _get_maxl_8b(0x9f);
			sbtmdb_reg_tgt.pbpct[4] = 10000;
			sbtmdb_reg_tgt.pbnits[4] = _get_maxl_8b(0x94);
			sbtmdb_reg_tgt.minnits = 0;
			break;
		case SBTM_DRDM_HGIG_SEL_1:
			sbtmdb_reg_tgt.numpb = 0;
			sbtmdb_reg_tgt.pbpct[0] = 10000;
			sbtmdb_reg_tgt.pbnits[0] = 600;
			for (i = 1; i < 5; i++) {
				sbtmdb_reg_tgt.pbpct[i] = 0;
				sbtmdb_reg_tgt.pbnits[i] = 0;
			}
			sbtmdb_reg_tgt.minnits = 16;
			break;
		case SBTM_DRDM_HGIG_SEL_2:
			sbtmdb_reg_tgt.numpb = 1;
			sbtmdb_reg_tgt.pbpct[0] = 1000;
			sbtmdb_reg_tgt.pbnits[0] = 1000;
			sbtmdb_reg_tgt.pbpct[1] = 10000;
			sbtmdb_reg_tgt.pbnits[1] = 600;
			for (i = 2; i < 5; i++) {
				sbtmdb_reg_tgt.pbpct[i] = 0;
				sbtmdb_reg_tgt.pbnits[i] = 0;
			}
			sbtmdb_reg_tgt.minnits = 16;
			break;
		case SBTM_DRDM_HGIG_SEL_3:
			sbtmdb_reg_tgt.numpb = 1;
			sbtmdb_reg_tgt.pbpct[0] = 1000;
			sbtmdb_reg_tgt.pbnits[0] = 4000;
			sbtmdb_reg_tgt.pbpct[1] = 10000;
			sbtmdb_reg_tgt.pbnits[1] = 600;
			for (i = 2; i < 5; i++) {
				sbtmdb_reg_tgt.pbpct[i] = 0;
				sbtmdb_reg_tgt.pbnits[i] = 0;
			}
			sbtmdb_reg_tgt.minnits = 16;
			break;
		case SBTM_DRDM_HGIG_SEL_4:
			sbtmdb_reg_tgt.numpb = 1;
			sbtmdb_reg_tgt.pbpct[0] = 1000;
			sbtmdb_reg_tgt.pbnits[0] = 10000;
			sbtmdb_reg_tgt.pbpct[1] = 10000;
			sbtmdb_reg_tgt.pbnits[1] = 600;
			for (i = 2; i < 5; i++) {
				sbtmdb_reg_tgt.pbpct[i] = 0;
				sbtmdb_reg_tgt.pbnits[i] = 0;
			}
			sbtmdb_reg_tgt.minnits = 0;
			break;
		}
	} else {
		if (sbtmdb_reg.p0_exp == 0 &&
			sbtmdb_reg.p0_mant == 0 &&
			sbtmdb_reg.peak_bright_p0 == 0) {
			sbtmdb_reg_tgt.numpb = 0;
			sbtmdb_reg_tgt.pbpct[0] = 10000;
			sbtmdb_reg_tgt.pbnits[0] = _get_maxl_8b(sbtmdb_reg.peak_bright_100);
			sbtmdb_reg_tgt.minnits = 16;
		} else if (sbtmdb_reg.p1_exp == 0 &&
			sbtmdb_reg.p1_mant == 0 &&
			sbtmdb_reg.peak_bright_p1 == 0) {
			sbtmdb_reg_tgt.numpb = 1;
			sbtmdb_reg_tgt.pbpct[0] =
				sbtm_cal_pbpct(sbtmdb_reg.p0_exp, sbtmdb_reg.p0_mant);
			sbtmdb_reg_tgt.pbnits[0] = _get_maxl_8b(sbtmdb_reg.peak_bright_p0);
			sbtmdb_reg_tgt.pbpct[1] = 10000;
			sbtmdb_reg_tgt.pbnits[1] = _get_maxl_8b(sbtmdb_reg.peak_bright_100);
			sbtmdb_reg_tgt.minnits = sbtmdb_reg.min_bright_10;
		} else if (sbtmdb_reg.p2_exp == 0 &&
			sbtmdb_reg.p2_mant == 0 &&
			sbtmdb_reg.peak_bright_p2 == 0) {
			sbtmdb_reg_tgt.numpb = 2;
			sbtmdb_reg_tgt.pbpct[0] =
				sbtm_cal_pbpct(sbtmdb_reg.p1_exp, sbtmdb_reg.p1_mant);
			sbtmdb_reg_tgt.pbnits[0] = _get_maxl_8b(sbtmdb_reg.peak_bright_p1);
			sbtmdb_reg_tgt.pbpct[1] =
				sbtm_cal_pbpct(sbtmdb_reg.p0_exp, sbtmdb_reg.p0_mant);
			sbtmdb_reg_tgt.pbnits[1] = _get_maxl_8b(sbtmdb_reg.peak_bright_p0);
			sbtmdb_reg_tgt.pbpct[2] = 10000;
			sbtmdb_reg_tgt.pbnits[2] = _get_maxl_8b(sbtmdb_reg.peak_bright_100);
			sbtmdb_reg_tgt.minnits = sbtmdb_reg.min_bright_10;
		} else if (sbtmdb_reg.p3_exp == 0 &&
			sbtmdb_reg.p3_mant == 0 &&
			sbtmdb_reg.peak_bright_p3 == 0) {
			sbtmdb_reg_tgt.numpb = 3;
			sbtmdb_reg_tgt.pbpct[0] =
				sbtm_cal_pbpct(sbtmdb_reg.p2_exp, sbtmdb_reg.p2_mant);
			sbtmdb_reg_tgt.pbnits[0] = _get_maxl_8b(sbtmdb_reg.peak_bright_p2);
			sbtmdb_reg_tgt.pbpct[1] =
				sbtm_cal_pbpct(sbtmdb_reg.p1_exp, sbtmdb_reg.p1_mant);
			sbtmdb_reg_tgt.pbnits[1] = _get_maxl_8b(sbtmdb_reg.peak_bright_p1);
			sbtmdb_reg_tgt.pbpct[2] =
				sbtm_cal_pbpct(sbtmdb_reg.p0_exp, sbtmdb_reg.p0_mant);
			sbtmdb_reg_tgt.pbnits[2] = _get_maxl_8b(sbtmdb_reg.peak_bright_p0);
			sbtmdb_reg_tgt.pbpct[3] = 10000;
			sbtmdb_reg_tgt.pbnits[3] = _get_maxl_8b(sbtmdb_reg.peak_bright_100);
			sbtmdb_reg_tgt.minnits = sbtmdb_reg.min_bright_10;
		} else {
			sbtmdb_reg_tgt.numpb = 4;
			sbtmdb_reg_tgt.pbpct[0] =
				sbtm_cal_pbpct(sbtmdb_reg.p3_exp, sbtmdb_reg.p3_mant);
			sbtmdb_reg_tgt.pbnits[0] = _get_maxl_8b(sbtmdb_reg.peak_bright_p3);
			sbtmdb_reg_tgt.pbpct[1] =
				sbtm_cal_pbpct(sbtmdb_reg.p2_exp, sbtmdb_reg.p2_mant);
			sbtmdb_reg_tgt.pbnits[1] = _get_maxl_8b(sbtmdb_reg.peak_bright_p2);
			sbtmdb_reg_tgt.pbpct[2] =
				sbtm_cal_pbpct(sbtmdb_reg.p1_exp, sbtmdb_reg.p1_mant);
			sbtmdb_reg_tgt.pbnits[2] = _get_maxl_8b(sbtmdb_reg.peak_bright_p1);
			sbtmdb_reg_tgt.pbpct[3] =
				sbtm_cal_pbpct(sbtmdb_reg.p0_exp, sbtmdb_reg.p0_mant);
			sbtmdb_reg_tgt.pbnits[3] = _get_maxl_8b(sbtmdb_reg.peak_bright_p0);
			sbtmdb_reg_tgt.pbpct[4] = 10000;
			sbtmdb_reg_tgt.pbnits[4] = _get_maxl_8b(sbtmdb_reg.peak_bright_100);
			sbtmdb_reg_tgt.minnits = sbtmdb_reg.min_bright_10;
		}
	}
	pr_sbtm_dbg("%s: sbtmdb_reg_tgt.numpb = %d\n", __func__, sbtmdb_reg_tgt.numpb);
}

u32 sbtm_cal_frmpblimit(s32 pbright, s32 *pbpct, s32 *pbnits, u8 numpb)
{
	s32 pbidx;
	s64 pctdiff, pbdiff, pbrange;
	s64 frmpblilit = 0;
	u32 frmpblilitint = 0;

	if (pbright < pbpct[0]) {
		frmpblilit = pbnits[0];
	} else {
		for (pbidx = 1; pbidx <= numpb; pbidx++)
			if (pbright <= pbpct[pbidx]) {
				pctdiff = pbpct[pbidx] - pbpct[pbidx - 1];
				pbdiff = pbright - pbpct[pbidx - 1];
				pbrange = div64_s64(pbdiff << 10, pctdiff);
				frmpblilit = pbnits[pbidx - 1] +
					(pbrange * (pbnits[pbidx] - pbnits[pbidx - 1]) >> 10);
				break;
			}
	}
	frmpblilitint = frmpblilit;
	return frmpblilitint;
}

u32 sbtm_cal_pbright(u32 *hist, u32 pb_numpb)
{
	u32 pbright = 10000;
	int pb_th;
	int i, thidx;
	u64 sumlight = 0;
	u64 exceedlight = 0;

	if (!hist) {
		pr_sbtm_dbg("%s: HDR hist is NULL, pbright is pct 100.\n", __func__);
		return pbright;
	}

	pb_th = _get_maxl_e(pb_numpb);
	thidx = pb_th >> 3;

	for (i = 0; i < 128; i++) {
		sumlight += hist[i];
		if (i > thidx)
			exceedlight += hist[i];
		}
	pbright = div64_u64(exceedlight * 10000 + (sumlight >> 1), sumlight);

	if (hdr10_tmo_debug_sbtm)
		pr_sbtm_dbg("%s: pbright = %d, sumlight = %lld, exceedlight = %lld.\n",
			__func__, pbright, sumlight, exceedlight);

	return pbright;
}

void sbtm_drdm_frmpblimit_update(void)
{
	u32 frmpblilit;
	u32 pbright = 10;

	if (sbtm_hdr_process) {
		pbright = sbtm_cal_pbright(hdr_hist[15],
			sbtmdb_reg_tgt.pbnits[sbtmdb_reg_tgt.numpb]);
		frmpblilit = sbtm_cal_frmpblimit(pbright, &sbtmdb_reg_tgt.pbpct[0],
		&sbtmdb_reg_tgt.pbnits[0], sbtmdb_reg_tgt.numpb);

		frmpblilit = sbtmdb_reg_tgt.pbnits[0];
		if (hdr10_tmo_debug_sbtm)
			pr_sbtm_dbg("%s: frmpblimit = %d.\n", __func__, frmpblilit);
	} else {
		frmpblilit = sbtmdb_reg_tgt.pbnits[0];
	}

	sbtmdb_reg_tgt.frmpblimit = frmpblilit;
	tmo_reg_sbtm.reg_display_e = _get_maxl_e(sbtmdb_reg_tgt.frmpblimit);
}

void sbtm_peak_lum_update(void)
{
	if (sbtm_mode == SBTM_MODE_GRDM) {
		sbtm_grdm_peaklum_update();
	} else if (sbtm_mode == SBTM_MODE_DRDM) {
		sbtm_drdm_frmpblimit_update();
	} else {
		sbtmdb_reg_tgt.tgt_maxl = 0;
		sbtmdb_reg_tgt.frmpblimit = 0;
		tmo_reg_sbtm.reg_display_e = _get_maxl_e(1000);
	}
}

void sbtm_tgt_param_update(void)
{
	sbtm_dest_primary_update();
	sbtm_pb_pct_nits_update();
	sbtm_peak_lum_update();
}

void sbtm_sbtmdb_reg_set(struct sbtm_info *sbtmdb_info)
{
	if (!sbtmdb_info) {
		pr_sbtm_dbg("sbtmdb_info is NULL\n");
		return;
	}

	if (sbtmdb_info->sbtm_support == 0) {
		sbtm_en = 0;
		sbtm_mode = SBTM_MODE_OFF;
		sbtmdb_reg.sbtm_support = 0;
		return;
	}

	sbtm_en = 1;
	if (sbtmdb_reg.sbtm_support != sbtmdb_info->sbtm_support ||
		sbtmdb_reg.max_sbtm_ver != sbtmdb_info->max_sbtm_ver ||
		sbtmdb_reg.grdm_support != sbtmdb_info->grdm_support ||
		sbtmdb_reg.drdm_ind != sbtmdb_info->drdm_ind ||
		sbtmdb_reg.hgig_cat_drdm_sel != sbtmdb_info->hgig_cat_drdm_sel ||
		sbtmdb_reg.use_hgig_drdm != sbtmdb_info->use_hgig_drdm ||
		sbtmdb_reg.maxrgb != sbtmdb_info->maxrgb ||
		sbtmdb_reg.gamut != sbtmdb_info->gamut ||
		sbtmdb_reg.red_x != sbtmdb_info->red_x ||
		sbtmdb_reg.red_y != sbtmdb_info->red_y ||
		sbtmdb_reg.green_x != sbtmdb_info->green_x ||
		sbtmdb_reg.green_y != sbtmdb_info->green_y ||
		sbtmdb_reg.blue_x != sbtmdb_info->blue_x ||
		sbtmdb_reg.blue_y != sbtmdb_info->blue_y ||
		sbtmdb_reg.white_x != sbtmdb_info->white_x ||
		sbtmdb_reg.white_y != sbtmdb_info->white_y ||
		sbtmdb_reg.min_bright_10 != sbtmdb_info->min_bright_10 ||
		sbtmdb_reg.peak_bright_100 != sbtmdb_info->peak_bright_100 ||
		sbtmdb_reg.p0_exp != sbtmdb_info->p0_exp ||
		sbtmdb_reg.p0_mant != sbtmdb_info->p0_mant ||
		sbtmdb_reg.peak_bright_p0 != sbtmdb_info->peak_bright_p0 ||
		sbtmdb_reg.p1_exp != sbtmdb_info->p1_exp ||
		sbtmdb_reg.p1_mant != sbtmdb_info->p1_mant ||
		sbtmdb_reg.peak_bright_p1 != sbtmdb_info->peak_bright_p1 ||
		sbtmdb_reg.p2_exp != sbtmdb_info->p2_exp ||
		sbtmdb_reg.p2_mant != sbtmdb_info->p2_mant ||
		sbtmdb_reg.peak_bright_p2 != sbtmdb_info->peak_bright_p2 ||
		sbtmdb_reg.p3_exp != sbtmdb_info->p3_exp ||
		sbtmdb_reg.p3_mant != sbtmdb_info->p3_mant ||
		sbtmdb_reg.peak_bright_p3 != sbtmdb_info->peak_bright_p3) {
		pr_sbtm_dbg("sbtm_support = %d\n", sbtmdb_info->sbtm_support);
		pr_sbtm_dbg("max_sbtm_ver = %d\n", sbtmdb_info->max_sbtm_ver);
		pr_sbtm_dbg("grdm_support = %d\n", sbtmdb_info->grdm_support);
		pr_sbtm_dbg("drdm_ind = %d\n", sbtmdb_info->drdm_ind);
		pr_sbtm_dbg("hgig_cat_drdm_sel = %d\n", sbtmdb_info->hgig_cat_drdm_sel);
		pr_sbtm_dbg("use_hgig_drdm = %d\n", sbtmdb_info->use_hgig_drdm);
		pr_sbtm_dbg("maxrgb = %d\n", sbtmdb_info->maxrgb);
		pr_sbtm_dbg("gamut = %d\n", sbtmdb_info->gamut);
		pr_sbtm_dbg("red_x = %d\n", sbtmdb_info->red_x);
		pr_sbtm_dbg("red_y = %d\n", sbtmdb_info->red_y);
		pr_sbtm_dbg("green_x = %d\n", sbtmdb_info->green_x);
		pr_sbtm_dbg("green_y = %d\n", sbtmdb_info->green_y);
		pr_sbtm_dbg("blue_x = %d\n", sbtmdb_info->blue_x);
		pr_sbtm_dbg("blue_y = %d\n", sbtmdb_info->blue_y);
		pr_sbtm_dbg("white_x = %d\n", sbtmdb_info->white_x);
		pr_sbtm_dbg("white_y = %d\n", sbtmdb_info->white_y);
		pr_sbtm_dbg("min_bright_10 = %d\n", sbtmdb_info->min_bright_10);
		pr_sbtm_dbg("peak_bright_100 = %d\n", sbtmdb_info->peak_bright_100);
		pr_sbtm_dbg("p0_exp = %d\n", sbtmdb_info->p0_exp);
		pr_sbtm_dbg("p0_mant = %d\n", sbtmdb_info->p0_mant);
		pr_sbtm_dbg("peak_bright_p0 = %d\n", sbtmdb_info->peak_bright_p0);
		pr_sbtm_dbg("p1_exp = %d\n", sbtmdb_info->p1_exp);
		pr_sbtm_dbg("p1_mant = %d\n", sbtmdb_info->p1_mant);
		pr_sbtm_dbg("peak_bright_p1 = %d\n", sbtmdb_info->peak_bright_p1);
		pr_sbtm_dbg("p2_exp = %d\n", sbtmdb_info->p2_exp);
		pr_sbtm_dbg("p2_mant = %d\n", sbtmdb_info->p2_mant);
		pr_sbtm_dbg("peak_bright_p2 = %d\n", sbtmdb_info->peak_bright_p2);
		pr_sbtm_dbg("p3_exp = %d\n", sbtmdb_info->p3_exp);
		pr_sbtm_dbg("p3_mant = %d\n", sbtmdb_info->p3_mant);
		pr_sbtm_dbg("peak_bright_p3 = %d\n", sbtmdb_info->peak_bright_p3);
		pr_sbtm_dbg("drdm_ind = %d\n", sbtmdb_info->drdm_ind);

		sbtmdb_reg.sbtm_support = sbtmdb_info->sbtm_support;
		sbtmdb_reg.max_sbtm_ver = sbtmdb_info->max_sbtm_ver;
		sbtmdb_reg.grdm_support = sbtmdb_info->grdm_support;
		sbtmdb_reg.drdm_ind = sbtmdb_info->drdm_ind;
		sbtmdb_reg.hgig_cat_drdm_sel = sbtmdb_info->hgig_cat_drdm_sel;
		sbtmdb_reg.use_hgig_drdm = sbtmdb_info->use_hgig_drdm;
		sbtmdb_reg.maxrgb = sbtmdb_info->maxrgb;
		sbtmdb_reg.gamut = sbtmdb_info->gamut;
		sbtmdb_reg.red_x = sbtmdb_info->red_x;
		sbtmdb_reg.red_y = sbtmdb_info->red_y;
		sbtmdb_reg.green_x = sbtmdb_info->green_x;
		sbtmdb_reg.green_y = sbtmdb_info->green_y;
		sbtmdb_reg.blue_x = sbtmdb_info->blue_x;
		sbtmdb_reg.blue_y = sbtmdb_info->blue_y;
		sbtmdb_reg.white_x = sbtmdb_info->white_x;
		sbtmdb_reg.white_y = sbtmdb_info->white_y;
		sbtmdb_reg.min_bright_10 = sbtmdb_info->min_bright_10;
		sbtmdb_reg.peak_bright_100 = sbtmdb_info->peak_bright_100;
		sbtmdb_reg.p0_exp = sbtmdb_info->p0_exp;
		sbtmdb_reg.p0_mant = sbtmdb_info->p0_mant;
		sbtmdb_reg.peak_bright_p0 = sbtmdb_info->peak_bright_p0;
		sbtmdb_reg.p1_exp = sbtmdb_info->p1_exp;
		sbtmdb_reg.p1_mant = sbtmdb_info->p1_mant;
		sbtmdb_reg.peak_bright_p1 = sbtmdb_info->peak_bright_p1;
		sbtmdb_reg.p2_exp = sbtmdb_info->p2_exp;
		sbtmdb_reg.p2_mant = sbtmdb_info->p2_mant;
		sbtmdb_reg.peak_bright_p2 = sbtmdb_info->peak_bright_p2;
		sbtmdb_reg.p3_exp = sbtmdb_info->p3_exp;
		sbtmdb_reg.p3_mant = sbtmdb_info->p3_mant;
		sbtmdb_reg.peak_bright_p3 = sbtmdb_info->peak_bright_p3;

		sbtm_tgt_param_update();
	} else {
		sbtm_peak_lum_update();
	}
}

void sbtm_sbtmdb_set(struct vinfo_s *vinfo)
{
	struct sbtm_info *sbtmdb = NULL;

	sbtmdb = &vinfo->hdr_info.sbtm_info;
	if (!sbtmdb) {
		pr_sbtm_dbg("sbtm_info is NULL\n");
		return;
	}

	if (sbtmdb_auto_update)
		sbtm_sbtmdb_reg_set(sbtmdb);
}

/* -1: invalid osd index
 *  0: osd is disabled
 *  1: osd is enabled
 */
int (*get_osd_enable)(u32 index);

u8 sbtm_content_type_update(void)
{
	u8 sbtmtype;
	int osd_status = 0;

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
		if (get_osd_enable)
			osd_status = get_osd_enable(SBTM_OSD1) ||
				get_osd_enable(SBTM_OSD2) ||
				get_osd_enable(SBTM_OSD3);
		else
			osd_status = 0;

	} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_T7 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_S5) {
		if (get_osd_enable)
			osd_status = get_osd_enable(SBTM_OSD1) ||
				get_osd_enable(SBTM_OSD3);
		else
			osd_status = 0;
	} else {
		if (get_osd_enable)
			osd_status = get_osd_enable(SBTM_OSD1);
		else
			osd_status = 0;
	}

	if (osd_status == 1)
		sbtmtype = SBTM_TYPE_COMPOSITED;
	else
		sbtmtype = SBTM_TYPE_VIDEO;

	return sbtmtype;
}

int register_osd_status_cb(int (*get_osd_enable_status)(u32 index))
{
	pr_info("%s: SBTM register osd enable status func\n", __func__);
	get_osd_enable = get_osd_enable_status;
	return 0;
}
EXPORT_SYMBOL(register_osd_status_cb);

void unregister_osd_status_cb(void)
{
	pr_info("SBTM unregister osd enable status func\n");
	get_osd_enable = NULL;
}
EXPORT_SYMBOL(unregister_osd_status_cb);

void sbtm_send_sbtmem_pkt(struct vinfo_s *vinfo)
{
	struct vtem_sbtm_st *sbtmem = &sbtmem_reg;

	if (!sbtm_en)
		return;

	sbtmem->sbtm_mode = sbtm_mode;
	if (sbtm_mode == SBTM_MODE_GRDM) {
		sbtmem->grdm_min = sbtmdb_reg.grdm_support;
		sbtmem->grdm_lum = sbtmdb_reg.grdm_support;
	} else {
		sbtmem->grdm_min = 0;
		sbtmem->grdm_lum = 0;
	}

	if (sbtm_mode == SBTM_MODE_DRDM)
		sbtmem->frmpblimitint = sbtmdb_reg_tgt.frmpblimit;
	else
		sbtmem->frmpblimitint = 0;

	sbtmem->sbtm_type = sbtm_content_type_update();

	if (vinfo && vinfo->vout_device &&
		vinfo->vout_device->fresh_tx_sbtm_pkt)
		vinfo->vout_device->fresh_tx_sbtm_pkt(sbtmem);

	/*pr_sbtm_dbg("%s: fresh_tx_sbtm_pkt.\n", __func__);*/
}
EXPORT_SYMBOL(sbtm_send_sbtmem_pkt);

/*
 * struct aml_tmo_reg_sw_hdr *sbtm_tmo_param_get(void)
 * {
 * return &tmo_reg_sbtm_hdrfunc;
 * };
 * EXPORT_SYMBOL(sbtm_tmo_param_get);
 * void sbtm_hdr10_tmo_gen_data(int metadata_lum_e, u32 *oo_gain)
 * {
 * struct aml_tmo_reg_sw_hdr *pre_tmo_reg;
 * pre_tmo_reg = sbtm_tmo_param_get();
 * if (!pre_tmo_reg->pre_hdr10_tmo_alg)
 * pr_sbtm_dbg("%s: hdr10_tmo alg func is NULL\n", __func__);
 * else
 * tmo_reg_sbtm_hdrfunc.pre_hdr10_tmo_alg(&tmo_reg_sbtm,
 * oo_gain, hdr_hist[15], metadata_lum_e);
 * }
 */

void sbtm_tmo_alg_sdr2hdr(uint lum_tgr, int *oolut)
{
	int i = 0;

	for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++)
		oo_lut[i] = (512 * lum_tgr + (10000 >> 1)) / 10000;
	memcpy(oolut, oo_lut, sizeof(int) * HDR2_OOTF_LUT_SIZE);
}

void sbtm_tmo_alg_hdr2hdr_static(struct vinfo_s *vinfo, uint lum_tgr, int *oolut)
{
	int i = 0;
	int metadate_maxl = 1000;
	struct master_display_info_s *p = NULL;

	if (vinfo->master_display_info.present_flag) {
		p = &vinfo->master_display_info;
		metadate_maxl = p->luminance[1];
	}

	for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++)
		oo_lut[i] = (64 * lum_tgr + (metadate_maxl >> 1)) / metadate_maxl;
	pr_sbtm_dbg("%s: hdr2hdr_tmo convert OK.  metadate_maxl = %d, lum_tgr = %d, oo_lut[10] = %d\n",
		__func__, metadate_maxl, lum_tgr, oo_lut[10]);

	memcpy(oolut, oo_lut, sizeof(int) * HDR2_OOTF_LUT_SIZE);
}

static s64 _pqeotf(s64 e, unsigned int *eo_lut)
{
	int x = 0;
	int e_mid = 0;
	s64 o;

	e_mid = e >> 6; //12bit

	if (e_mid == 4096) {
		o = eo_lut[142];
	} else if (e_mid > 64) {
		x = (e_mid >> 5) + 14; //to find e in which interval
		if (x > 141)
			x = 141;
		e_mid = (e_mid >> 5) << 11 ;  //18bit
		o = eo_lut[x] + ((((e - e_mid) * (eo_lut[x + 1] - eo_lut[x]) >> 5) + 32) >> 6);
	} else {
		x = e_mid >> 2; //to find e in which interval
		e_mid = (e_mid >> 2) << 8;
		o = eo_lut[x] + ((((e - e_mid) * (eo_lut[x + 1] - eo_lut[x]) >> 2) + 32) >> 6);
	}
	return o;
}

void tmo_hdr_dynamic(struct aml_vm_reg *reg_vm_st,
			struct aml_tmo_reg_sw *reg_tmo,
			int w, int h, int *hist,
			int protect_lum_e)
{
	s64 pix_num = 0;
	s64 display_e;
	s64 display_o;
	s64 high_light_num_o;
	s64 display_o_l;
	s64 high_light_num_o_l;
	s64 protect_e;
	s64 protect_lum_o = 500;
	s64 protect_lum_o_l;
	int i;
	s64 high_light_num = 1020;

	for (i = 127; i >= 0; i--) {
		if (pix_num < ((s64)w * h * reg_tmo->reg_highlight >> 14)) {/*1% percent*/
			if (hist[i] > (reg_tmo->reg_hist_th << 10) && i > reg_tmo->reg_light_th) {
				high_light_num = high_light_num - 8;
			} else {
				pix_num = pix_num + hist[i];
				high_light_num = high_light_num - 8;
			}
		}
	}
	if (high_light_num < 8)
		high_light_num = 8;

	protect_e = protect_lum_e;
	high_light_num_o = _pqeotf((high_light_num << 8), reg_tmo->eo_lut) >> 16;
	high_light_num_o_l = high_light_num_o * 10000 >> 12;
	protect_lum_o =  _pqeotf((protect_e << 8), reg_tmo->eo_lut) >> 16;
	protect_lum_o_l = protect_lum_o * 10000 >> 12;

	if (hdr10_tmo_debug_sbtm) {
		pr_info("high_light_num = %lld, high_light_num_o = %lld, high_light_num_o_l = %lld\n",
			high_light_num, high_light_num_o, high_light_num_o_l);
		pr_info("protect_lum_e = %lld, protect_lum_o = %lld, protect_lum_o_l = %lld\n",
			protect_e, protect_lum_o, protect_lum_o_l);
	}

	if (high_light_num < protect_e)
		high_light_num = protect_e;

	display_e = reg_tmo->reg_display_e;
	display_o = _pqeotf((display_e << 8), reg_tmo->eo_lut) >> 16;
	high_light_num_o = _pqeotf((high_light_num << 8), reg_tmo->eo_lut) >> 16;

	display_o_l = display_o * 10000 >> 12;
	high_light_num_o_l = high_light_num_o * 10000 >> 12;
	if (hdr10_tmo_debug_sbtm) {
		pr_info("final: high_light_num = %lld, high_light_num_o = %lld, high_light_num_o_l = %lld\n",
			high_light_num, high_light_num_o, high_light_num_o_l);
		pr_info("final: display_e = %lld, display_o = %lld, display_o_l = %lld\n",
			display_e, display_o, display_o_l);
	}

	/* linear gain*/
	for (i = 1; i < 149; i++)
		reg_vm_st->ogain_lut[i] =
			div64_s64(64 * display_o_l + (high_light_num_o_l >> 1), high_light_num_o_l);
}

void tmo_gen_hdr_dynamic(struct aml_tmo_reg_sw *reg_tmo,
	u32 *oo_gain, int *hdr_hist, int protect_lum_e)
{
	int i;
	int w = reg_tmo->w;
	int h = reg_tmo->h;
	static int tmo_en_chg = 1;
	struct aml_vm_reg gain_tbl;
	static int first_frame = 1;
	static unsigned int blend_gain[149];

	if (hdr10_tmo_debug_sbtm) {
		pr_info("%s --------hdr hist[128]---------\n", __func__);
		for (i = 0; i < 8; i++)
			pr_info("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
			hdr_hist[i * 16 + 0], hdr_hist[i * 16 + 1],
			hdr_hist[i * 16 + 2], hdr_hist[i * 16 + 3],
			hdr_hist[i * 16 + 4], hdr_hist[i * 16 + 5],
			hdr_hist[i * 16 + 6], hdr_hist[i * 16 + 7],
			hdr_hist[i * 16 + 8], hdr_hist[i * 16 + 9],
			hdr_hist[i * 16 + 10], hdr_hist[i * 16 + 11],
			hdr_hist[i * 16 + 12], hdr_hist[i * 16 + 13],
			hdr_hist[i * 16 + 14], hdr_hist[i * 16 + 15]);
	}

	tmo_hdr_dynamic(&gain_tbl, reg_tmo, w, h, hdr_hist, protect_lum_e);

	if (first_frame)
		memcpy(blend_gain, gain_tbl.ogain_lut, sizeof(int) * 149);

	if (tmo_en_chg != reg_tmo->tmo_en) {
		memcpy(blend_gain, gain_tbl.ogain_lut, sizeof(int) * 149);
		tmo_en_chg = reg_tmo->tmo_en;
		if (tmo_en_chg == 2)
			tmo_alpha = 0;
		else if (tmo_en_chg == 1)
			tmo_alpha = reg_tmo->alpha;
		pr_info("%s tmo_en_chg = %d\n", __func__, tmo_en_chg);
	}

	for (i = 0; i < 149; i++) {
		if (gain_tbl.ogain_lut[i] > 4095)
			gain_tbl.ogain_lut[i] = 4095;
		blend_gain[i] = (blend_gain[i] * tmo_alpha +
			((1 << 8) - tmo_alpha) * gain_tbl.ogain_lut[i]) >> 8;
		if (blend_gain[i] > 4095)
			blend_gain[i] = 4095;
	}

	memcpy(oo_gain, blend_gain, sizeof(int) * 149);

	if (hdr10_tmo_debug_sbtm) {
		pr_info("%s -----------ogain lut[149]----------\n", __func__);
		for (i = 0; i < 14; i++)
			pr_info("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
			gain_tbl.ogain_lut[i * 10 + 0], gain_tbl.ogain_lut[i * 10 + 1],
			gain_tbl.ogain_lut[i * 10 + 2], gain_tbl.ogain_lut[i * 10 + 3],
			gain_tbl.ogain_lut[i * 10 + 4], gain_tbl.ogain_lut[i * 10 + 5],
			gain_tbl.ogain_lut[i * 10 + 6], gain_tbl.ogain_lut[i * 10 + 7],
			gain_tbl.ogain_lut[i * 10 + 8], gain_tbl.ogain_lut[i * 10 + 9]);
		pr_info("%d, %d, %d, %d, %d, %d, %d, %d, %d\n",
			gain_tbl.ogain_lut[140], gain_tbl.ogain_lut[141],
			gain_tbl.ogain_lut[142], gain_tbl.ogain_lut[143],
			gain_tbl.ogain_lut[144], gain_tbl.ogain_lut[145],
			gain_tbl.ogain_lut[146], gain_tbl.ogain_lut[147],
			gain_tbl.ogain_lut[148]);

		pr_info("%s -----------blend ogain lut[149]----------\n", __func__);
		for (i = 0; i < 14; i++)
			pr_info("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
			blend_gain[i * 10 + 0], blend_gain[i * 10 + 1],
			blend_gain[i * 10 + 2], blend_gain[i * 10 + 3],
			blend_gain[i * 10 + 4], blend_gain[i * 10 + 5],
			blend_gain[i * 10 + 6], blend_gain[i * 10 + 7],
			blend_gain[i * 10 + 8], blend_gain[i * 10 + 9]);
		pr_info("%d, %d, %d, %d, %d, %d, %d, %d, %d\n",
			blend_gain[140], blend_gain[141], blend_gain[142],
			blend_gain[143], blend_gain[144], blend_gain[145],
			blend_gain[146], blend_gain[147], blend_gain[148]);
		hdr10_tmo_debug_sbtm--;
	}

	first_frame = 0;
}

void tmo_alg_hdr_dynamic(struct aml_tmo_reg_sw *tmo_reg,
	u32 *oo_gain, int *hist, int metadata_lum_e)
{
	if (tmo_reg && tmo_reg->tmo_en)
		tmo_gen_hdr_dynamic(tmo_reg, oo_gain, hist, metadata_lum_e);
	else
		pr_info("hdr10_tmo tmo_en is 0\n");
}

void sbtm_tmo_alg_hdr2hdr_dynamic(struct vinfo_s *vinfo, int *oolut)
{
	int metadata_l_e = 770;

/*
 * int metadate_l = 1000;
 * struct master_display_info_s *p = NULL;
 * if (vinfo->master_display_info.present_flag) {
 *     p = &vinfo->master_display_info;
 *     metadate_l = p->luminance[1];
 * }
 * metadata_l_e = sbtm_get_maxl_e(metadate_l);
 */

	metadata_l_e = _get_maxl_e(500);
	tmo_alg_hdr_dynamic(&tmo_reg_sbtm, oo_lut, hdr_hist[15], metadata_l_e);
	memcpy(oolut, oo_lut, sizeof(int) * HDR2_OOTF_LUT_SIZE);
}

int sbtm_grdm_tmo_convert_proc(enum sbtm_hdr_proc_sel sbtm_proc_sel,
			  struct vinfo_s *vinfo)
{
	int ret = 0;

	switch (sbtm_proc_sel) {
	case SBTM_OSD_PROC:
	case SBTM_SDR_HDR_PROC:
		if (sbtmdb_reg.grdm_support == SBTM_GRDM_HDR)
			memcpy(oo_y_lut_sbtm, oo_y_lut_sdr_hdr_1000_sbtm,
				sizeof(int) * HDR2_OOTF_LUT_SIZE);
		else
			memcpy(oo_y_lut_sbtm, oo_y_lut_sdr_hdr_300_sbtm,
				sizeof(int) * HDR2_OOTF_LUT_SIZE);
		break;
	case SBTM_HDR_HDR_PROC:
		if (sbtm_tmo_static)
			sbtm_tmo_alg_hdr2hdr_static(vinfo, sbtmdb_reg_tgt.tgt_maxl, oo_y_lut_sbtm);
		else
			sbtm_tmo_alg_hdr2hdr_dynamic(vinfo, oo_y_lut_sbtm);
		sbtm_tmo_alg_sdr2hdr(sbtmdb_reg_tgt.tgt_maxl, oo_y_lut_sbtm_osd);
		break;
	default:
		break;
	}
	return ret;
}

int sbtm_drdm_tmo_convert_proc(enum sbtm_hdr_proc_sel sbtm_proc_sel,
			  struct vinfo_s *vinfo)
{
	int ret = 0;

	switch (sbtm_proc_sel) {
	case SBTM_OSD_PROC:
		sbtm_drdm_frmpblimit_update();
		sbtm_tmo_alg_sdr2hdr(sbtmdb_reg_tgt.frmpblimit, oo_y_lut_sbtm);
		break;
	case SBTM_SDR_HDR_PROC:
		sbtm_hdr_process = 0;
		sbtm_drdm_frmpblimit_update();
		sbtm_tmo_alg_sdr2hdr(sbtmdb_reg_tgt.frmpblimit, oo_y_lut_sbtm);
		break;
	case SBTM_HDR_HDR_PROC:
		sbtm_hdr_process = 1;
		sbtm_drdm_frmpblimit_update();
		if (sbtm_tmo_static)
			sbtm_tmo_alg_hdr2hdr_static(vinfo,
				sbtmdb_reg_tgt.frmpblimit, oo_y_lut_sbtm);
		else
			sbtm_tmo_alg_hdr2hdr_dynamic(vinfo, oo_y_lut_sbtm);
		sbtm_tmo_alg_sdr2hdr(sbtmdb_reg_tgt.frmpblimit, oo_y_lut_sbtm_osd);
		/*pr_sbtm_dbg("%s:lum_tgr = %d, oo_lut[80] = %d\n", */
			/*__func__, sbtmdb_reg_tgt.frmpblimit, oo_y_lut_sbtm_osd[80]); */
		break;
	default:
		break;
	}
	return ret;
}

int sbtm_tmo_hdr2hdr_process(struct vinfo_s *vinfo)
{
	if (sbtm_mode == SBTM_MODE_DRDM)
		sbtm_drdm_tmo_convert_proc(SBTM_HDR_HDR_PROC, vinfo);
	else
		sbtm_grdm_tmo_convert_proc(SBTM_HDR_HDR_PROC, vinfo);
	return 0;
}
EXPORT_SYMBOL(sbtm_tmo_hdr2hdr_process);

int sbtm_gamut_convert_proc(enum sbtm_hdr_proc_sel sbtm_proc_sel,
			  struct matrix_s *mtx,
			  int mtx_depth)
{
	int i, j, ret;
	s64 src_prmy[4][2];
	s64 dest_prmy[4][2];

	if (sbtm_mode == SBTM_MODE_OFF) {
		mtx = NULL;
		pr_sbtm_dbg("%s: mtx use NULL.\n", __func__);
		return 0;
	}

	/*default 11bit*/
	if (mtx_depth == 0)
		mtx_depth = 11;

	if (sbtm_proc_sel == SBTM_OSD_PROC ||
		sbtm_proc_sel == SBTM_SDR_HDR_PROC) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++) {
				src_prmy[i][j] = std_bt709_prmy[(i + 2) % 3][j];
				src_prmy[3][j] = std_bt709_white_point[j];
			}
	} else if (sbtm_proc_sel == SBTM_HDR_HDR_PROC) {
		if (get_primary_policy() == PRIMARIES_AUTO) {
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++) {
					src_prmy[i][j] = std_p3_primaries[(i + 2) % 3][j];
					src_prmy[3][j] = std_p3_white_point[j];
				}
		} else {
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++) {
					src_prmy[i][j] = std_bt2020_prmy[(i + 2) % 3][j];
					src_prmy[3][j] = std_bt2020_white_point[j];
				}
		}
	}

	for (i = 0; i < 4; i++)
		for (j = 0; j < 2; j++)
			dest_prmy[i][j] = sbtmdb_reg_tgt.dest_primary[i][j];

	ret = gamut_convert(&src_prmy[0][0], &dest_prmy[0][0], mtx, mtx_depth);

	return ret;
}

int sbtm_convert_process(enum sbtm_hdr_proc_sel sbtm_proc_sel,
			  struct vinfo_s *vinfo,
			  struct matrix_s *mtx,
			  int mtx_depth)
{
	if (cur_sbtm_mode != sbtm_mode) {
		cur_sbtm_mode = sbtm_mode;
		sbtm_tgt_param_update();
	}

	pr_sbtm_dbg("%s: sbtm_proc_sel = %x, sbtm_mode = %d.\n",
		__func__, sbtm_proc_sel, sbtm_mode);

	sbtm_gamut_convert_proc(sbtm_proc_sel, mtx, mtx_depth);

	if (sbtm_mode == SBTM_MODE_DRDM) {
		if (sbtm_proc_sel == SBTM_HDR_HDR_PROC &&
			!sbtm_tmo_static)
			pr_sbtm_dbg("DRDM HDR_HDR tmo_dynamic.\n");
		else
			sbtm_drdm_tmo_convert_proc(sbtm_proc_sel, vinfo);
	} else {
		if (sbtm_proc_sel == SBTM_HDR_HDR_PROC &&
			!sbtm_tmo_static)
			pr_sbtm_dbg("GRDM HDR_HDR tmo_dynamic.\n");
		else
			sbtm_grdm_tmo_convert_proc(sbtm_proc_sel, vinfo);
	}

	return 0;
}
EXPORT_SYMBOL(sbtm_convert_process);

void sbtm_tmo_change_param(int val)
{
	if (val == 0 || val == 1) {
		tmo_reg_sbtm.reg_highlight = 185;
		tmo_reg_sbtm.reg_middle_a = 102;
		tmo_reg_sbtm.reg_low_adj = 38;
		tmo_reg_sbtm.reg_hl0 = 60;
		tmo_reg_sbtm.reg_hl1 = 38;
		tmo_reg_sbtm.reg_hl2 = 102;
		tmo_reg_sbtm.reg_hl3 = 80;
	} else if (val == 2) {
		tmo_reg_sbtm.reg_highlight = 832;
		tmo_reg_sbtm.reg_middle_a = 80;
		tmo_reg_sbtm.reg_low_adj = 30;
		tmo_reg_sbtm.reg_hl0 = 40;
		tmo_reg_sbtm.reg_hl1 = 60;
		tmo_reg_sbtm.reg_hl2 = 80;
		tmo_reg_sbtm.reg_hl3 = 90;
	}
	pr_info("tmo_reg_sbtm changed from %d to %d\n", tmo_reg_sbtm.tmo_en, val);
}

void sbtm_hdr10_tmo_parm_show(void)
{
	pr_info("tmo_en = %d\n", tmo_reg_sbtm.tmo_en);
	pr_info("reg_highlight = %d\n", tmo_reg_sbtm.reg_highlight);
	pr_info("reg_light_th = %d\n", tmo_reg_sbtm.reg_light_th);
	pr_info("reg_hist_th = %d\n", tmo_reg_sbtm.reg_hist_th);
	pr_info("reg_highlight_th1 = %d\n", tmo_reg_sbtm.reg_highlight_th1);
	pr_info("reg_highlight_th2 = %d\n", tmo_reg_sbtm.reg_highlight_th2);
	pr_info("reg_display_e = %d\n", tmo_reg_sbtm.reg_display_e);
	pr_info("reg_middle_a = %d\n", tmo_reg_sbtm.reg_middle_a);
	pr_info("reg_middle_a_adj = %d\n", tmo_reg_sbtm.reg_middle_a_adj);
	pr_info("reg_middle_b = %d\n", tmo_reg_sbtm.reg_middle_b);
	pr_info("reg_middle_s = %d\n", tmo_reg_sbtm.reg_middle_s);
	pr_info("reg_max_th1 = %d\n", tmo_reg_sbtm.reg_max_th1);
	pr_info("reg_middle_th = %d\n", tmo_reg_sbtm.reg_middle_th);
	pr_info("reg_thold2 = %d\n", tmo_reg_sbtm.reg_thold2);
	pr_info("reg_thold3 = %d\n", tmo_reg_sbtm.reg_thold3);
	pr_info("reg_thold4 = %d\n", tmo_reg_sbtm.reg_thold4);
	pr_info("reg_thold1 = %d\n", tmo_reg_sbtm.reg_thold1);
	pr_info("reg_max_th2 = %d\n", tmo_reg_sbtm.reg_max_th2);
	pr_info("reg_pnum_th = %d\n", tmo_reg_sbtm.reg_pnum_th);
	pr_info("reg_hl0 = %d\n", tmo_reg_sbtm.reg_hl0);
	pr_info("reg_hl1 = %d\n", tmo_reg_sbtm.reg_hl1);
	pr_info("reg_hl2 = %d\n", tmo_reg_sbtm.reg_hl2);
	pr_info("reg_hl3 = %d\n", tmo_reg_sbtm.reg_hl3);
	pr_info("reg_display_adj = %d\n", tmo_reg_sbtm.reg_display_adj);
	pr_info("reg_low_adj = %d\n", tmo_reg_sbtm.reg_low_adj);
	pr_info("reg_high_en = %d\n", tmo_reg_sbtm.reg_high_en);
	pr_info("reg_high_adj1 = %d\n", tmo_reg_sbtm.reg_high_adj1);
	pr_info("reg_high_adj2 = %d\n", tmo_reg_sbtm.reg_high_adj2);
	pr_info("reg_high_maxdiff = %d\n", tmo_reg_sbtm.reg_high_maxdiff);
	pr_info("reg_high_mindiff = %d\n", tmo_reg_sbtm.reg_high_mindiff);
	pr_info("reg_avg_th = %d\n", tmo_reg_sbtm.reg_avg_th);
	pr_info("reg_avg_adj = %d\n", tmo_reg_sbtm.reg_avg_adj);
	pr_info("alpha = %d\n", tmo_reg_sbtm.alpha);
	pr_info("reg_ratio = %d\n", tmo_reg_sbtm.reg_ratio);
	pr_info("reg_max_th3 = %d\n", tmo_reg_sbtm.reg_max_th3);
	pr_info("oo_init_lut = %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		tmo_reg_sbtm.oo_init_lut[0], tmo_reg_sbtm.oo_init_lut[1],
		tmo_reg_sbtm.oo_init_lut[2], tmo_reg_sbtm.oo_init_lut[3],
		tmo_reg_sbtm.oo_init_lut[4], tmo_reg_sbtm.oo_init_lut[5],
		tmo_reg_sbtm.oo_init_lut[6], tmo_reg_sbtm.oo_init_lut[7],
		tmo_reg_sbtm.oo_init_lut[8], tmo_reg_sbtm.oo_init_lut[9],
		tmo_reg_sbtm.oo_init_lut[10], tmo_reg_sbtm.oo_init_lut[11],
		tmo_reg_sbtm.oo_init_lut[12]);
}

int sbtm_hdr10_tmo_dbg(char **parm)
{
	long val;
	int idx;

	if (!strcmp(parm[1], "tmo_en"))  {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		if (tmo_reg_sbtm.tmo_en != (int)val)
			sbtm_tmo_change_param((int)val);
		tmo_reg_sbtm.tmo_en = (int)val;
		pr_sbtm_dbg("tmo_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "highlight")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_highlight = (int)val;
		pr_sbtm_dbg("reg_highlight = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "light_th")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_light_th = (int)val;
		pr_sbtm_dbg("reg_light_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "hist_th")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_hist_th = (int)val;
		pr_sbtm_dbg("reg_hist_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "highlight_th1")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_highlight_th1 = (int)val;
		pr_sbtm_dbg("reg_highlight_th1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "highlight_th2")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_highlight_th2 = (int)val;
		pr_sbtm_dbg("reg_highlight_th2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "display_e")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_display_e = (int)val;
		pr_sbtm_dbg("reg_display_e = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "middle_a")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_middle_a = (int)val;
		pr_sbtm_dbg("reg_middle_a = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "middle_a_adj")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_middle_a_adj = (int)val;
		pr_sbtm_dbg("reg_middle_a_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "middle_b")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_middle_b = (int)val;
		pr_sbtm_dbg("reg_middle_b = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "middle_s")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_middle_s = (int)val;
		pr_sbtm_dbg("reg_middle_s = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "max_th1")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_max_th1 = (int)val;
		pr_sbtm_dbg("reg_max_th1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "max_th2")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_max_th2 = (int)val;
		pr_sbtm_dbg("reg_max_th2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "middle_th")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_middle_th = (int)val;
		pr_sbtm_dbg("reg_middle_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "thold1")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_thold1 = (int)val;
		pr_sbtm_dbg("reg_thold1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "thold2")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_thold2 = (int)val;
		pr_sbtm_dbg("reg_thold2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "thold3")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_thold3 = (int)val;
		pr_sbtm_dbg("reg_thold3 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "thold4")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_thold4 = (int)val;
		pr_sbtm_dbg("reg_thold4 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "pnum_th")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_pnum_th = (int)val;
		pr_sbtm_dbg("reg_pnum_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "hl0")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_hl0 = (int)val;
		pr_sbtm_dbg("reg_hl0 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "hl1")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_hl1 = (int)val;
		pr_sbtm_dbg("reg_hl1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "hl2")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_hl2 = (int)val;
		pr_sbtm_dbg("reg_hl2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "hl3")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_hl3 = (int)val;
		pr_sbtm_dbg("reg_hl3 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "display_adj")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_display_adj = (int)val;
		pr_sbtm_dbg("reg_display_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "low_adj")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_low_adj = (int)val;
		pr_sbtm_dbg("reg_low_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "high_en")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_high_en = (int)val;
		pr_sbtm_dbg("reg_high_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "high_adj1")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_high_adj1 = (int)val;
		pr_sbtm_dbg("reg_high_adj1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "high_adj2")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_high_adj2 = (int)val;
		pr_sbtm_dbg("reg_high_adj2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "high_maxdiff")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_high_maxdiff = (int)val;
		pr_sbtm_dbg("reg_high_maxdiff = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "high_mindiff")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_high_mindiff = (int)val;
		pr_sbtm_dbg("reg_high_mindiff = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "avg_th")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_avg_th = (int)val;
		pr_sbtm_dbg("reg_avg_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "avg_adj")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_avg_adj = (int)val;
		pr_sbtm_dbg("reg_avg_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "alpha")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.alpha = (int)val;
		pr_sbtm_dbg("alpha = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "ratio")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_ratio = (int)val;
		pr_sbtm_dbg("reg_ratio = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "max_th3")) {
		if (kstrtol(parm[2], 10, &val) < 0)
			goto error;
		tmo_reg_sbtm.reg_max_th3 = (int)val;
		pr_sbtm_dbg("reg_max_th3 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "oo_init_lut")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		idx = (int)val;
		if (kstrtoul(parm[3], 10, &val) < 0)
			goto error;
		if (idx > 12)
			goto error;
		tmo_reg_sbtm.oo_init_lut[idx] = (int)val;
		pr_sbtm_dbg("oo_init_lut[%d] = %d\n", idx, (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "read_param")) {
		sbtm_hdr10_tmo_parm_show();
	}

	return 0;

error:
	return -1;
}

void sbtm_reg_show(void)
{
	int i;
	struct sbtm_info *sbtmdb_info = &sbtmdb_reg;
	struct sbtmdb_tgt_parm *sbtmdb_tgt = &sbtmdb_reg_tgt;
	struct vtem_sbtm_st *sbtmem_info = &sbtmem_reg;

	pr_sbtm_dbg("SBTMDB info.\n");
	pr_sbtm_dbg("sbtm_support = %d\n", sbtmdb_info->sbtm_support);
	pr_sbtm_dbg("max_sbtm_ver = %d\n", sbtmdb_info->max_sbtm_ver);
	pr_sbtm_dbg("grdm_support = %d\n", sbtmdb_info->grdm_support);
	pr_sbtm_dbg("drdm_ind = %d\n", sbtmdb_info->drdm_ind);
	pr_sbtm_dbg("hgig_cat_drdm_sel = %d\n", sbtmdb_info->hgig_cat_drdm_sel);
	pr_sbtm_dbg("use_hgig_drdm = %d\n", sbtmdb_info->use_hgig_drdm);
	pr_sbtm_dbg("maxrgb = %d\n", sbtmdb_info->maxrgb);
	pr_sbtm_dbg("gamut = %d\n", sbtmdb_info->gamut);
	pr_sbtm_dbg("red_x = %d\n", sbtmdb_info->red_x);
	pr_sbtm_dbg("red_y = %d\n", sbtmdb_info->red_y);
	pr_sbtm_dbg("green_x = %d\n", sbtmdb_info->green_x);
	pr_sbtm_dbg("green_y = %d\n", sbtmdb_info->green_y);
	pr_sbtm_dbg("blue_x = %d\n", sbtmdb_info->blue_x);
	pr_sbtm_dbg("blue_y = %d\n", sbtmdb_info->blue_y);
	pr_sbtm_dbg("white_x = %d\n", sbtmdb_info->white_x);
	pr_sbtm_dbg("white_y = %d\n", sbtmdb_info->white_y);
	pr_sbtm_dbg("min_bright_10 = %d\n", sbtmdb_info->min_bright_10);
	pr_sbtm_dbg("peak_bright_100 = %d\n", sbtmdb_info->peak_bright_100);
	pr_sbtm_dbg("p0_exp = %d\n", sbtmdb_info->p0_exp);
	pr_sbtm_dbg("p0_mant = %d\n", sbtmdb_info->p0_mant);
	pr_sbtm_dbg("peak_bright_p0 = %d\n", sbtmdb_info->peak_bright_p0);
	pr_sbtm_dbg("p1_exp = %d\n", sbtmdb_info->p1_exp);
	pr_sbtm_dbg("p1_mant = %d\n", sbtmdb_info->p1_mant);
	pr_sbtm_dbg("peak_bright_p1 = %d\n", sbtmdb_info->peak_bright_p1);
	pr_sbtm_dbg("p2_exp = %d\n", sbtmdb_info->p2_exp);
	pr_sbtm_dbg("p2_mant = %d\n", sbtmdb_info->p2_mant);
	pr_sbtm_dbg("peak_bright_p2 = %d\n", sbtmdb_info->peak_bright_p2);
	pr_sbtm_dbg("p3_exp = %d\n", sbtmdb_info->p3_exp);
	pr_sbtm_dbg("p3_mant = %d\n", sbtmdb_info->p3_mant);
	pr_sbtm_dbg("peak_bright_p3 = %d\n", sbtmdb_info->peak_bright_p3);

	pr_sbtm_dbg("\nSBTMDB target info.\n");
	pr_sbtm_dbg("tgt_maxl = %d\n", sbtmdb_tgt->tgt_maxl);
	pr_sbtm_dbg("target_paimary: %d, %d, %d, %d, %d, %d, %d, %d\n",
		sbtmdb_tgt->dest_primary[0][0], sbtmdb_tgt->dest_primary[0][1],
		sbtmdb_tgt->dest_primary[1][0], sbtmdb_tgt->dest_primary[1][1],
		sbtmdb_tgt->dest_primary[2][0], sbtmdb_tgt->dest_primary[2][1],
		sbtmdb_tgt->dest_primary[3][0], sbtmdb_tgt->dest_primary[3][1]);
	for (i = 0; i < 5; i++)
		pr_sbtm_dbg("pbpct[%d] = %d, pbnits[%d] = %d\n",
			i, sbtmdb_tgt->pbpct[i], i, sbtmdb_tgt->pbnits[i]);
	pr_sbtm_dbg("numpb = %d\n", sbtmdb_tgt->numpb);
	pr_sbtm_dbg("frmpblimit = %d\n", sbtmdb_tgt->frmpblimit);
	pr_sbtm_dbg("minnits = %d\n", sbtmdb_tgt->minnits);

	pr_sbtm_dbg("\nSBTMEM info.\n");
	pr_sbtm_dbg("sbtm_ver = %d\n", sbtmem_info->sbtm_ver);
	pr_sbtm_dbg("sbtm_mode = %d\n", sbtmem_info->sbtm_mode);
	pr_sbtm_dbg("sbtm_type = %d\n", sbtmem_info->sbtm_type);
	pr_sbtm_dbg("grdm_min = %d\n", sbtmem_info->grdm_min);
	pr_sbtm_dbg("grdm_lum = %d\n", sbtmem_info->grdm_lum);
	pr_sbtm_dbg("frmpblimitint = %d\n", sbtmem_info->frmpblimitint);
}

int sbtm_sbtmdb_reg_dbg(char **parm)
{
	long val;

	if (!strcmp(parm[1], "sbtm_support"))  {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.sbtm_support = (int)val;
		pr_sbtm_dbg("sbtm_support = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "max_sbtm_ver")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.max_sbtm_ver = (int)val;
		pr_sbtm_dbg("max_sbtm_ver = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "grdm_support")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.grdm_support = (int)val;
		pr_sbtm_dbg("grdm_support = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "drdm_ind")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.drdm_ind = (int)val;
		pr_sbtm_dbg("drdm_ind = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "hgig_cat_drdm_sel")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.hgig_cat_drdm_sel = (int)val;
		pr_sbtm_dbg("hgig_cat_drdm_sel = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "use_hgig_drdm")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.use_hgig_drdm = (int)val;
		pr_sbtm_dbg("use_hgig_drdm = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "maxrgb")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.maxrgb = (int)val;
		pr_sbtm_dbg("maxrgb = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "gamut")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.gamut = (int)val;
		pr_sbtm_dbg("gamut = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "red_x")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.red_x = (int)val;
		pr_sbtm_dbg("red_x = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "red_y")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.red_y = (int)val;
		pr_sbtm_dbg("red_y = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "green_x")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.green_x = (int)val;
		pr_sbtm_dbg("green_x = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "green_y")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.green_y = (int)val;
		pr_sbtm_dbg("green_y = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "blue_x")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.blue_x = (int)val;
		pr_sbtm_dbg("blue_x = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "blue_y")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.blue_y = (int)val;
		pr_sbtm_dbg("blue_y = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "white_x")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.white_x = (int)val;
		pr_sbtm_dbg("white_x = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "white_y")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.white_y = (int)val;
		pr_sbtm_dbg("white_y = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "min_bright_10")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.min_bright_10 = (int)val;
		pr_sbtm_dbg("min_bright_10 = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "peak_bright_100")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.peak_bright_100 = (int)val;
		pr_sbtm_dbg("peak_bright_100 = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "p0_exp")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p0_exp = (int)val;
		pr_sbtm_dbg("p0_exp = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "p0_mant")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p0_mant = (int)val;
		pr_sbtm_dbg("p0_mant = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "peak_bright_p0")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.peak_bright_p0 = (int)val;
		pr_sbtm_dbg("peak_bright_p0 = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "p1_exp")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p1_exp = (int)val;
		pr_sbtm_dbg("p1_exp = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "p1_mant")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p1_mant = (int)val;
		pr_sbtm_dbg("p1_mant = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "peak_bright_p1")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.peak_bright_p1 = (int)val;
		pr_sbtm_dbg("peak_bright_p1 = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "p2_exp")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p2_exp = (int)val;
		pr_sbtm_dbg("p2_exp = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "p2_mant")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p2_mant = (int)val;
		pr_sbtm_dbg("p2_mant = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "peak_bright_p2")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.peak_bright_p2 = (int)val;
		pr_sbtm_dbg("peak_bright_p2 = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "p3_exp")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p3_exp = (int)val;
		pr_sbtm_dbg("p3_exp = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "p3_mant")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p3_mant = (int)val;
		pr_sbtm_dbg("p3_mant = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "peak_bright_p3")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.peak_bright_p3 = (int)val;
		pr_sbtm_dbg("peak_bright_p3 = %d\n", (int)val);
		pr_info("\n");
		sbtm_tgt_param_update();
	} else if (!strcmp(parm[1], "reg_dump")) {
		sbtm_reg_show();
	}

	return 0;

error:
	return -1;
}

