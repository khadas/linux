// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/hdr/am_cuva_hdr_tm.c
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/amvecm/cuva_alg.h>
#include "am_cuva_hdr_tm.h"
#include "am_hdr10_plus.h"

uint cuva_sw_dbg;
module_param(cuva_sw_dbg, uint, 0664);
MODULE_PARM_DESC(cuva_sw_dbg, "\n cuva_sw_dbg\n");

unsigned int num_max_output_lum = 2;
/* [0]: cuva->sdr output luminance,
 *      used for iptv sdr 100/200/300/400nit output
 *      default 0, for tv get luma from vinfo
 * [1]: cvua->hdr10 output luminance
 *      used for iptv 1200/1000/800/500nit output
 *      default 0 use metadata luminance out
 */
unsigned int max_output_lum[] = {
	0, 0
};
module_param_array(max_output_lum, uint, &num_max_output_lum, 0664);
MODULE_PARM_DESC(max_output_lum, "\n set max output luminance");

static s64 lut_ogain[149];
static s64 lut_cgain[65];
char cuva_ver[] = "cuva-2022-02-19, update cuva hdr2sdr parameters";

#define LUT_SHIFT 520
static u16 linear_e_to_o[504] = {
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

const char *cuva_func_char[] = {
	"CUVAHDR_TO_SDR",
	"CUVAHDR_TO_HDR10",
	"CUVAHLG_TO_SDR",
	"CUVAHLG_TO_HLG",
	"CUVAHLG_TO_HDR10"
};

struct aml_gain_reg vm_reg = {
	.ogain_lut = lut_ogain,
	.cgain_lut = lut_cgain
};

static struct aml_static_reg_sw aml_static_reg = {
	.max_display_mastering_luminance = 4000,
	.min_display_mastering_luminance = 1000,
	.max_content_light_level = 800,
	.max_picture_average_light_level = 700
};

static struct aml_cuva_reg_sw aml_cuva_reg_hdr = {
	.itp = 0,
	.cuva_en = 1,
	.maxlum_adj_min = 2080,
	.maxlum_adj_a = 409,
	.maxlum_adj_b = 205,
	.base_curve_generate_0_pvh0 = 14336,
	.base_curve_generate_0_pvl0 = 16384,
	.base_curve_generate_0_tph0 = 2458,
	.base_curve_generate_0_tpl0 = 1229,
	.base_curve_generate_0_pdh1 = 2458,
	.base_curve_generate_0_pdl1 = 0,
	.base_curve_generate_0_tph1 = 3686,
	.base_curve_generate_0_tpl1 = 3072,
	.base_curve_generate_0_n = 10,
	.base_curve_generate_0_m = 10,
	.base_curve_adj_1_n = 5,
	.base_curve_adj_2_n = 10,
	.spline1_generate_0_hlmaxh2 = 2458,
	.spline1_generate_0_hlmaxl2 = 1229,
	.spline1_generate_0_tdmaxh2 = 1024,
	.spline1_generate_0_tdmaxl2 = 410,
	.spline1_generate_0_avmaxh3 = 2458,
	.spline1_generate_0_avmaxl3 = 1229,
	.spline1_generate_0_sdmaxh3 = 4096,
	.spline1_generate_0_sdmaxl3 = 3932,
	.spline1_generate_0_n = 10,
	.spline1_generate_0_m = 10,
	.spline1_adj_0_n1 = 10,
	.spline1_adj_0_n2 = 10,
	.spline3_generate_0_b = 614,
	.spline3_generate_0_c = 2048,
	.spline3_generate_0_d = 2048,
	.spline1_generate_sdr_0_avmaxh6 = 2458,
	.spline1_generate_sdr_0_avmaxl6 = 1229,
	.spline1_generate_sdr_0_sdmaxh6 = 4096,
	.spline1_generate_sdr_0_sdmaxl6 = 3686,
	.spline1_generate_sdr_0_n = 10,
	.spline3_generate_sdr_0_b = 2048,
	.spline3_generate_sdr_0_c = 2048,
	.spline3_generate_sdr_0_d = 2048,
	.sca_adj_a = 0,
	.sca_adj_b = 0,
	.sca_satr = 4000,
	.hdrsdrth = 0
};

static struct aml_cuva_reg_sw aml_cuva_reg_sdr = {
	.itp = 0,
	.cuva_en = 1,
	.maxlum_adj_min = 2080,
	.maxlum_adj_a = 409,
	.maxlum_adj_b = 205,
	.base_curve_generate_0_pvh0 = 14336,
	.base_curve_generate_0_pvl0 = 24576,
	.base_curve_generate_0_tph0 = 2458,
	.base_curve_generate_0_tpl0 = 410,
	.base_curve_generate_0_pdh1 = 2457,
	.base_curve_generate_0_pdl1 = 1229,
	.base_curve_generate_0_tph1 = 3071,
	.base_curve_generate_0_tpl1 = 2774,
	.base_curve_generate_0_n = 10,
	.base_curve_generate_0_m = 10,
	.base_curve_adj_1_n = 5,
	.base_curve_adj_2_n = 10,
	.spline1_generate_0_hlmaxh2 = 2458,
	.spline1_generate_0_hlmaxl2 = 1229,
	.spline1_generate_0_tdmaxh2 = 1024,
	.spline1_generate_0_tdmaxl2 = 410,
	.spline1_generate_0_avmaxh3 = 2458,
	.spline1_generate_0_avmaxl3 = 1229,
	.spline1_generate_0_sdmaxh3 = 4096,
	.spline1_generate_0_sdmaxl3 = 3932,
	.spline1_generate_0_n = 10,
	.spline1_generate_0_m = 10,
	.spline1_adj_0_n1 = 10,
	.spline1_adj_0_n2 = 10,
	.spline3_generate_0_b = 614,
	.spline3_generate_0_c = 2048,
	.spline3_generate_0_d = 2048,
	.spline1_generate_sdr_0_avmaxh6 = 2458,
	.spline1_generate_sdr_0_avmaxl6 = 1229,
	.spline1_generate_sdr_0_sdmaxh6 = 4096,
	.spline1_generate_sdr_0_sdmaxl6 = 3686,
	.spline1_generate_sdr_0_n = 10,
	.spline3_generate_sdr_0_b = 614,
	.spline3_generate_sdr_0_c = 2048,
	.spline3_generate_sdr_0_d = 2048,
	.sca_adj_a = 0,
	.sca_adj_b = 0,
	.sca_satr = 4000,
	.hdrsdrth = 1
};

struct aml_gain_reg *get_gain_lut(void)
{
	return &vm_reg;
}

struct aml_cuva_data_s cuva_data = {
	.ic_type = IC_OTHER,
	.max_panel_e = 657,
	.min_panel_e = 0,
	.cuva_reg = &aml_cuva_reg_hdr,
	.static_reg = &aml_static_reg,
	.aml_vm_regs = &vm_reg,
	.cuva_md = &cuva_metadata,
	.cuva_hdr_alg = NULL
};

struct aml_cuva_data_s *get_cuva_data(void)
{
	return &cuva_data;
}
EXPORT_SYMBOL(get_cuva_data);

static int get_maxl_e(int maxl)
{
	int i;
	int maxl_e = 0;

	if (maxl > 10000) {
		if (cuva_sw_dbg & DEBUG_PROC_INFO)
			pr_info("maxl over 10000. invalid maxl\n");
		maxl_e = 1023;
		return maxl_e;
	}

	for (i = 0; i < 504; i++) {
		if (linear_e_to_o[i] >= maxl) {
			maxl_e = i + LUT_SHIFT;
			break;
		}
	}

	if (cuva_sw_dbg & DEBUG_PROC_INFO)
		pr_info("maxl = %d, maxl_e = %d\n", maxl, maxl_e);

	return maxl_e;
}

static void get_cuva_tm_maxl(enum cuva_func_e tm_func,
	struct vframe_master_display_colour_s *p)
{
	int maxl = 400;
	int minl = 0;
	int static_maxl = 4000;
	struct vinfo_s *vinfo = get_current_vinfo();
	struct aml_cuva_data_s *cd = get_cuva_data();

	if (p->luminance[0] > 10000)
		static_maxl = p->luminance[0] / 10000;
	else if (p->luminance[0] >= 1000)
		static_maxl = p->luminance[0];

	if (static_maxl < 1000) /*avoid wrong metadata*/
		static_maxl = 4000;

	cd->static_reg->max_display_mastering_luminance = static_maxl;

	switch (tm_func) {
	case CUVA_HDR2SDR:
	case CUVA_HLG2SDR:
		if (vinfo->hdr_info.lumi_max >= 150) /*support lowest panel 150nit*/
			maxl = vinfo->hdr_info.lumi_max;

		minl = 64; /*e domin 64 : minl = 0.1*/

		/*force output luminance for iptv select*/
		if (max_output_lum[0] != 0)
			maxl = max_output_lum[0];
		cd->max_panel_e = get_maxl_e(maxl);
		cd->min_panel_e = minl;
		break;

	case CUVA_HDR2HDR10:
	case CUVA_HLG2HLG:
	case CUVA_HLG2HDR10:
		/* if (p->luminance[0] > 10000)
		 *	maxl = p->luminance[0] / 10000;
		 * else if (p->luminance[0] > 0)
		 *	maxl = p->luminance[0];
		 * else
		 *	maxl = 1000;
		 */

		/* because static maxl usually 10000,
		 * in fact hdr10 output 1000 should be better
		 * force output 1000 as default
		 */
		maxl = 1000;

		minl = 47; /* e domin 47 : minl = 0.05*/

		/* static maxl may need add into max_display_mastering_luminance
		 * max_display_mastering_luminance now force 4000 as default,
		 * max_display_mastering_luminance may need set to static maxl
		 * need confirm after test
		 */

		/*force output luminance for iptv select*/
		if (max_output_lum[1] != 0)
			maxl = max_output_lum[1];
		cd->max_panel_e = get_maxl_e(maxl);
		cd->min_panel_e = minl;
		break;
	default:
		break;
	}
}

void cuva_tm_func(enum cuva_func_e tm_func,
	struct vframe_master_display_colour_s *p)
{
	struct aml_cuva_data_s *cd = get_cuva_data();

	switch (tm_func) {
	case CUVA_HDR2SDR:
		cd->cuva_reg = &aml_cuva_reg_sdr;
		cd->cuva_reg->itp = CUVA_HDR2SDR;
		break;
	case CUVA_HLG2SDR:
		cd->cuva_reg = &aml_cuva_reg_sdr;
		cd->cuva_reg->itp = CUVA_HLG2SDR;
		break;
	case CUVA_HDR2HDR10:
		cd->cuva_reg = &aml_cuva_reg_hdr;
		cd->cuva_reg->itp = CUVA_HDR2HDR10;
		break;
	case CUVA_HLG2HLG:
		cd->cuva_reg = &aml_cuva_reg_hdr;
		cd->cuva_reg->itp = CUVA_HLG2HLG;
		break;
	case CUVA_HLG2HDR10:
		cd->cuva_reg->itp = CUVA_HLG2HDR10;
		cd->cuva_reg = &aml_cuva_reg_hdr;
		break;
	default:
		break;
	}

	if (cuva_sw_dbg & DEBUG_PROC_INFO)
		pr_info("%s\n", cuva_func_char[tm_func]);

	get_cuva_tm_maxl(tm_func, p);
}

int cuva_hdr_dbg(void)
{
	struct aml_cuva_data_s *cuva_data = get_cuva_data();

	pr_info("max_panel_e = %d\n", cuva_data->max_panel_e);
	pr_info("min_panel_e = %d\n", cuva_data->min_panel_e);
	pr_info("max_display_mastering_luminance = %lld\n",
		cuva_data->static_reg->max_display_mastering_luminance);
	pr_info("cuva_hdr_alg = %p\n", cuva_data->cuva_hdr_alg);
	pr_info(" ver: %s\n", cuva_ver);

	return 0;
}

