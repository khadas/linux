// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/local_contrast.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#define SKIP_IO_TRACE
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/uaccess.h>
#include "arch/vpp_regs.h"
#include "reg_helper.h"
#include "local_contrast.h"

/*may define in other module*/
#define CURV_NODES 6
/*curve_node_total*/
#define LC_HIST_SIZE 1632
/*curve_node_total*/
#define LC_CURV_SIZE 580
/*history message delay*/
#define N 4
/*hist bin num*/
#define HIST_BIN 16

int amlc_debug;
#define pr_amlc_dbg(fmt, args...)\
	do {\
		if (amlc_debug & 0x1)\
			pr_info("AMVE: " fmt, ## args);\
	} while (0)

int lc_en = 1;
int lc_bitdepth = 10;
int lc_curve_isr_defined;
int use_lc_curve_isr = 1;
int lc_demo_mode;
int lc_en_chflg = 0xff;
static int lc_flag = 0xff;
static int lc_bypass_flag = 0xff;
int osd_iir_en = 1;
int amlc_iir_debug_en;
/*osd related setting */
int vnum_start_below = 5;
int vnum_end_below = 6;
int vnum_start_above = 1;
int vnum_end_above = 2;
int invalid_blk = 2;
/*u10,7000/21600=0.324*1024=331 */
int min_bv_percent_th = 331;
/*control the refresh speed*/
int alpha1 = 512;
int alpha2 = 512;
int refresh_bit = 12;
int ts = 6;
/*need tuning according to real situation ! (0~512)*/
int scene_change_th = 50;

/*chose block to get hist*/
unsigned int lc_hist_vs;
unsigned int lc_hist_ve;
unsigned int lc_hist_hs;
unsigned int lc_hist_he;
/*lc curve data and hist data*/
int *lc_szcurve;/*12*8*6+4*/
int *curve_nodes_cur;
static int *curve_nodes_pre;
static s64 *curve_nodes_pre_raw;
int *lc_hist;/*12*8*17*/
static bool lc_malloc_ok;
/*print one or more frame data*/
unsigned int lc_hist_prcnt;
unsigned int lc_node_prcnt;
unsigned int lc_node_pos_h;
unsigned int lc_node_pos_v;
unsigned int lc_curve_prcnt;
bool lc_curve_fresh = true;

struct ve_lc_curve_parm_s lc_curve_parm_load;
struct lc_alg_param_s lc_alg_parm;
struct lc_curve_tune_param_s lc_tune_curve;

/*lc saturation gain, low parameters*/
/*static unsigned int lc_satur_gain[63] = {
 *	51, 104, 158, 213, 269, 325, 382, 440, 498,
 *	556, 615, 674, 734, 794, 854, 915, 976, 1037,
 *	1099, 1161, 1223, 1286, 1348, 1411, 1475, 1538,
 *	1602, 1666, 1730, 1795, 1859, 1924, 1989, 2054
 *	2120, 2186, 2251, 2318, 2384, 2450, 2517, 2584,
 *	2651, 2718, 2785, 2853, 2921, 2988, 3057, 3125,
 *	3193, 3262, 3330, 3399, 3468, 3537, 3607, 3676,
 *	3746, 3815, 3885, 3955, 4026
 *};
 */

/*lc saturation gain, off parameters*/
static unsigned int lc_satur_off[63] = {
	64, 128, 192, 256, 320, 384, 448, 512, 576, 640,
	704, 768, 832, 896, 960, 1024, 1088, 1152, 1216,
	1280, 1344, 1408, 1472, 1536, 1600, 1664, 1728,
	1792, 1856, 1920, 1984, 2048, 2112, 2176, 2240,
	2304, 2368, 2432, 2496, 2560, 2624, 2688, 2752,
	2816, 2880, 2944, 3008, 3072, 3136, 3200, 3264,
	3328, 3392, 3456, 3520, 3584, 3648, 3712, 3776,
	3840, 3904, 3968, 4032
};

/*lc saturation gain, low parameters*/
/*static unsigned int lc_satur_low[63] = {*/
/*	84, 167, 249, 330, 411, 490, 568, 646, 723, 798,*/
/*	873, 947, 1020, 1092, 1163, 1234, 1303, 1372, 1440,*/
/*	1506, 1572, 1638, 1702, 1766, 1828, 1890, 1951,*/
/*	2011, 2071, 2130, 2187, 2245, 2301, 2356, 2411,*/
/*	2465, 2518, 2571, 2623, 2674, 2724, 2774, 2822,*/
/*	2871, 2919, 2967, 3016, 3065, 3115, 3165, 3217,*/
/*	3271, 3326, 3384, 3443, 3506, 3571, 3639, 3710,*/
/*	3785, 3863, 3945, 4032*/
/*};*/

int tune_curve_en = 2;
int detect_signal_range_en = 2;
int detect_signal_range_threshold_black = 1200;
int detect_signal_range_threshold_white = 3200;

/*local contrast begin*/
static void lc_mtx_set(enum lc_mtx_sel_e mtx_sel,
		       enum lc_mtx_csc_e mtx_csc,
	int mtx_en,
	int bitdepth)
{
	unsigned int matrix_coef00_01 = 0;
	unsigned int matrix_coef02_10 = 0;
	unsigned int matrix_coef11_12 = 0;
	unsigned int matrix_coef20_21 = 0;
	unsigned int matrix_coef22 = 0;
	unsigned int matrix_clip = 0;
	unsigned int matrix_offset0_1 = 0;
	unsigned int matrix_offset2 = 0;
	unsigned int matrix_pre_offset0_1 = 0;
	unsigned int matrix_pre_offset2 = 0;
	unsigned int matrix_en_ctrl = 0;

	switch (mtx_sel) {
	case INP_MTX:
		matrix_coef00_01 = SRSHARP1_LC_YUV2RGB_MAT_0_1;
		matrix_coef02_10 = SRSHARP1_LC_YUV2RGB_MAT_2_3;
		matrix_coef11_12 = SRSHARP1_LC_YUV2RGB_MAT_4_5;
		matrix_coef20_21 = SRSHARP1_LC_YUV2RGB_MAT_6_7;
		matrix_coef22 = SRSHARP1_LC_YUV2RGB_MAT_8;
		matrix_pre_offset0_1 = SRSHARP1_LC_YUV2RGB_OFST;
		matrix_clip = SRSHARP1_LC_YUV2RGB_CLIP;
		break;
	case OUTP_MTX:
		matrix_coef00_01 = SRSHARP1_LC_RGB2YUV_MAT_0_1;
		matrix_coef02_10 = SRSHARP1_LC_RGB2YUV_MAT_2_3;
		matrix_coef11_12 = SRSHARP1_LC_RGB2YUV_MAT_4_5;
		matrix_coef20_21 = SRSHARP1_LC_RGB2YUV_MAT_6_7;
		matrix_coef22 = SRSHARP1_LC_RGB2YUV_MAT_8;
		matrix_offset0_1 = SRSHARP1_LC_RGB2YUV_OFST;
		matrix_clip = SRSHARP1_LC_RGB2YUV_CLIP;
		break;
	case STAT_MTX:
		matrix_coef00_01 = LC_STTS_MATRIX_COEF00_01;
		matrix_coef02_10 = LC_STTS_MATRIX_COEF02_10;
		matrix_coef11_12 = LC_STTS_MATRIX_COEF11_12;
		matrix_coef20_21 = LC_STTS_MATRIX_COEF20_21;
		matrix_coef22 = LC_STTS_MATRIX_COEF22;
		matrix_offset0_1 = LC_STTS_MATRIX_OFFSET0_1;
		matrix_offset2 = LC_STTS_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = LC_STTS_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = LC_STTS_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = LC_STTS_CTRL0;
		break;
	default:
		break;
	}

	if (mtx_sel & STAT_MTX)
		WRITE_VPP_REG_BITS(matrix_en_ctrl, mtx_en, 2, 1);

	if (!mtx_en)
		return;

	switch (mtx_csc) {
	case LC_MTX_RGB_YUV601L:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG(matrix_coef00_01, 0x1070204);
			WRITE_VPP_REG(matrix_coef02_10, 0x640f68);
			WRITE_VPP_REG(matrix_coef11_12, 0xed601c2);
			WRITE_VPP_REG(matrix_coef20_21, 0x01c20e87);
			WRITE_VPP_REG(matrix_coef22, 0x0000fb7);
			if (bitdepth == 10) {
				WRITE_VPP_REG(matrix_offset0_1, 0x00400200);
				WRITE_VPP_REG(matrix_clip, 0x3ff);
			} else {
				WRITE_VPP_REG(matrix_offset0_1, 0x01000800);
				WRITE_VPP_REG(matrix_clip, 0xfff);
			}
		} else if (mtx_sel & STAT_MTX) {
			WRITE_VPP_REG(matrix_coef00_01, 0x00bb0275);
			WRITE_VPP_REG(matrix_coef02_10, 0x003f1f99);
			WRITE_VPP_REG(matrix_coef11_12, 0x1ea601c2);
			WRITE_VPP_REG(matrix_coef20_21, 0x01c21e67);
			WRITE_VPP_REG(matrix_coef22, 0x00001fd7);
			WRITE_VPP_REG(matrix_offset0_1, 0x00400200);
			WRITE_VPP_REG(matrix_offset2, 0x00000200);
			WRITE_VPP_REG(matrix_pre_offset0_1, 0x0);
			WRITE_VPP_REG(matrix_pre_offset2, 0x0);
		}
		break;
	case LC_MTX_YUV601L_RGB:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			if (bitdepth == 10) {
				if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
				    get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
				    get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
				    get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x04A80000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x066204a8);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1e701cbf);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x04a80812);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				} else {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x012a0000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x198012a);
					WRITE_VPP_REG(matrix_coef11_12,
						      0xf9c0f30);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x12a0204);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				}
				WRITE_VPP_REG(matrix_pre_offset0_1, 0x00400200);
				WRITE_VPP_REG(matrix_clip, 0x3ff);
			} else {
				if (cpu_after_eq_t7()) {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x04A80000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x066204a8);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1e701cbf);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x04a80812);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				} else if (is_meson_tm2_cpu()) {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x012a0000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x198012a);
					WRITE_VPP_REG(matrix_coef11_12,
						      0xf9c0f30);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x12a0204);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				} else {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x04A80000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x066204a8);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1e701cbf);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x04a80812);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				}
				WRITE_VPP_REG(matrix_pre_offset0_1, 0x01000800);
				WRITE_VPP_REG(matrix_clip, 0xfff);
			}
		} else if (mtx_sel & STAT_MTX) {
			WRITE_VPP_REG(matrix_coef00_01, 0x04A80000);
			WRITE_VPP_REG(matrix_coef02_10, 0x072C04A8);
			WRITE_VPP_REG(matrix_coef11_12, 0x1F261DDD);
			WRITE_VPP_REG(matrix_coef20_21, 0x04A80876);
			WRITE_VPP_REG(matrix_coef22, 0x0);
			WRITE_VPP_REG(matrix_offset0_1, 0x0);
			WRITE_VPP_REG(matrix_offset2, 0x0);
			WRITE_VPP_REG(matrix_pre_offset0_1, 0x7c00600);
			WRITE_VPP_REG(matrix_pre_offset2, 0x00000600);
		}
		break;
	case LC_MTX_RGB_YUV709L:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG(matrix_coef00_01, 0x00bb0275);
			WRITE_VPP_REG(matrix_coef02_10, 0x003f1f99);
			WRITE_VPP_REG(matrix_coef11_12, 0x1ea601c2);
			WRITE_VPP_REG(matrix_coef20_21, 0x01c21e67);
			WRITE_VPP_REG(matrix_coef22, 0x00001fd7);
			if (bitdepth == 10) {
				WRITE_VPP_REG(matrix_offset0_1, 0x00400200);
				WRITE_VPP_REG(matrix_clip, 0x3ff);
			} else {
				WRITE_VPP_REG(matrix_offset0_1, 0x01000800);
				WRITE_VPP_REG(matrix_clip, 0xfff);
			}
		} else if (mtx_sel & STAT_MTX) {
			WRITE_VPP_REG(matrix_coef00_01, 0x00bb0275);
			WRITE_VPP_REG(matrix_coef02_10, 0x003f1f99);
			WRITE_VPP_REG(matrix_coef11_12, 0x1ea601c2);
			WRITE_VPP_REG(matrix_coef20_21, 0x01c21e67);
			WRITE_VPP_REG(matrix_coef22, 0x00001fd7);
			WRITE_VPP_REG(matrix_offset0_1, 0x00400200);
			WRITE_VPP_REG(matrix_offset2, 0x00000200);
			WRITE_VPP_REG(matrix_pre_offset0_1, 0x0);
			WRITE_VPP_REG(matrix_pre_offset2, 0x0);
		}
		break;
	case LC_MTX_YUV709L_RGB:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			if (bitdepth == 10) {
				if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
				    get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
				    get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
				    get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x04A80000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x072C04A8);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1F261DDD);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x04A80876);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				} else {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x012a0000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x01cb012a);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1fc90f77);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x012a021d);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				}
				WRITE_VPP_REG(matrix_pre_offset0_1, 0x400200);
				WRITE_VPP_REG(matrix_clip, 0x3ff);
			} else {
				if (cpu_after_eq_t7()) {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x04A80000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x072C04A8);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1F261DDD);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x04A80876);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				} else if (is_meson_tm2_cpu()) {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x012a0000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x01cb012a);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1fc90f77);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x012a021d);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				} else {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x04A80000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x072C04A8);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1F261DDD);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x04A80876);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				}
				WRITE_VPP_REG(matrix_pre_offset0_1, 0x01000800);
				WRITE_VPP_REG(matrix_clip, 0xfff);
			}
		} else if (mtx_sel & STAT_MTX) {
			WRITE_VPP_REG(matrix_coef00_01, 0x04A80000);
			WRITE_VPP_REG(matrix_coef02_10, 0x072C04A8);
			WRITE_VPP_REG(matrix_coef11_12, 0x1F261DDD);
			WRITE_VPP_REG(matrix_coef20_21, 0x04A80876);
			WRITE_VPP_REG(matrix_coef22, 0x0);
			WRITE_VPP_REG(matrix_offset0_1, 0x0);
			WRITE_VPP_REG(matrix_offset2, 0x0);
			WRITE_VPP_REG(matrix_pre_offset0_1, 0x7c00600);
			WRITE_VPP_REG(matrix_pre_offset2, 0x00000600);
		}
		break;
	case LC_MTX_RGB_YUV709:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG(matrix_coef00_01, 0x00da02dc);
			WRITE_VPP_REG(matrix_coef02_10, 0x004a1f8a);
			WRITE_VPP_REG(matrix_coef11_12, 0x1e760200);
			WRITE_VPP_REG(matrix_coef20_21, 0x02001e2f);
			WRITE_VPP_REG(matrix_coef22, 0x00001fd1);
			if (bitdepth == 10) {
				WRITE_VPP_REG(matrix_offset0_1, 0x200);
				WRITE_VPP_REG(matrix_clip, 0x3ff);
			} else {
				WRITE_VPP_REG(matrix_offset0_1, 0x800);
				WRITE_VPP_REG(matrix_clip, 0xfff);
			}
		} else if (mtx_sel & STAT_MTX) {
			WRITE_VPP_REG(matrix_coef00_01, 0x00bb0275);
			WRITE_VPP_REG(matrix_coef02_10, 0x003f1f99);
			WRITE_VPP_REG(matrix_coef11_12, 0x1ea601c2);
			WRITE_VPP_REG(matrix_coef20_21, 0x01c21e67);
			WRITE_VPP_REG(matrix_coef22, 0x00001fd7);
			WRITE_VPP_REG(matrix_offset0_1, 0x00400200);
			WRITE_VPP_REG(matrix_offset2, 0x00000200);
			WRITE_VPP_REG(matrix_pre_offset0_1, 0x0);
			WRITE_VPP_REG(matrix_pre_offset2, 0x0);
		}
		break;
	case LC_MTX_YUV709_RGB:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			if (bitdepth == 10) {
				if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
				    get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
				    get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
				    get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x04000000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x064d0400);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1f411e21);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x0400076d);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				} else {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x01000000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x01930100);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1fd01f88);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x010001db);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				}
				WRITE_VPP_REG(matrix_pre_offset0_1, 0x200);
				WRITE_VPP_REG(matrix_clip, 0x3ff);
			} else {
				if (cpu_after_eq_t7()) {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x04000000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x064d0400);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1f411e21);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x0400076d);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				} else if (is_meson_tm2_cpu()) {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x01000000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x01930100);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1fd01f88);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x010001db);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				} else {
					WRITE_VPP_REG(matrix_coef00_01,
						      0x04000000);
					WRITE_VPP_REG(matrix_coef02_10,
						      0x064d0400);
					WRITE_VPP_REG(matrix_coef11_12,
						      0x1f411e21);
					WRITE_VPP_REG(matrix_coef20_21,
						      0x0400076d);
					WRITE_VPP_REG(matrix_coef22, 0x0);
				}
				WRITE_VPP_REG(matrix_pre_offset0_1, 0x800);
				WRITE_VPP_REG(matrix_clip, 0xfff);
			}
		} else if (mtx_sel & STAT_MTX) {
			WRITE_VPP_REG(matrix_coef00_01, 0x04000000);
			WRITE_VPP_REG(matrix_coef02_10, 0x064d0400);
			WRITE_VPP_REG(matrix_coef11_12, 0x1f411e21);
			WRITE_VPP_REG(matrix_coef20_21, 0x0400076d);
			WRITE_VPP_REG(matrix_coef22, 0x0);
			WRITE_VPP_REG(matrix_offset0_1, 0x0);
			WRITE_VPP_REG(matrix_offset2, 0x0);
			WRITE_VPP_REG(matrix_pre_offset0_1, 0x00000600);
			WRITE_VPP_REG(matrix_pre_offset2, 0x00000600);
		}
		break;
	case LC_MTX_NULL:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG(matrix_coef00_01, 0x04000000);
			WRITE_VPP_REG(matrix_coef02_10, 0x0);
			WRITE_VPP_REG(matrix_coef11_12, 0x04000000);
			WRITE_VPP_REG(matrix_coef20_21, 0x0);
			WRITE_VPP_REG(matrix_coef22, 0x400);
			WRITE_VPP_REG(matrix_offset0_1, 0x0);
		} else if (mtx_sel & STAT_MTX) {
			WRITE_VPP_REG(matrix_coef00_01, 0x04000000);
			WRITE_VPP_REG(matrix_coef02_10, 0x0);
			WRITE_VPP_REG(matrix_coef11_12, 0x04000000);
			WRITE_VPP_REG(matrix_coef20_21, 0x0);
			WRITE_VPP_REG(matrix_coef22, 0x400);
			WRITE_VPP_REG(matrix_offset0_1, 0x0);
			WRITE_VPP_REG(matrix_offset2, 0x0);
			WRITE_VPP_REG(matrix_pre_offset0_1, 0x0);
			WRITE_VPP_REG(matrix_pre_offset2, 0x0);
		}
		break;
	default:
		break;
	}
}

static void lc_stts_blk_config(int enable,
			       unsigned int height, unsigned int width)
{
	int h_num, v_num;
	int blk_height,  blk_width;
	int row_start, col_start;
	int data32;
	int hend0, hend1, hend2, hend3, hend4, hend5, hend6;
	int hend7, hend8, hend9, hend10, hend11;
	int vend0, vend1, vend2, vend3, vend4, vend5, vend6, vend7;

	row_start = 0;
	col_start = 0;
	h_num = 12;
	v_num = 8;
	blk_height = height / v_num;
	blk_width = width / h_num;

	hend0 = col_start + blk_width - 1;
	hend1 = hend0 + blk_width;
	hend2 = hend1 + blk_width;
	hend3 = hend2 + blk_width;
	hend4 = hend3 + blk_width;
	hend5 = hend4 + blk_width;
	hend6 = hend5 + blk_width;
	hend7 = hend6 + blk_width;
	hend8 = hend7 + blk_width;
	hend9 = hend8 + blk_width;
	hend10 = hend9 + blk_width;
	hend11 = width - 1;

	vend0 = row_start + blk_height - 1;
	vend1 = vend0 + blk_height;
	vend2 = vend1 + blk_height;
	vend3 = vend2 + blk_height;
	vend4 = vend3 + blk_height;
	vend5 = vend4 + blk_height;
	vend6 = vend5 + blk_height;
	vend7 = height - 1;

	data32 = READ_VPP_REG(LC_STTS_HIST_REGION_IDX);
	WRITE_VPP_REG(LC_STTS_HIST_REGION_IDX, 0xffe0ffff & data32);
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		      ((((row_start & 0x1fff) << 16) & 0xffff0000) |
		(col_start & 0x1fff)));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		      ((hend1 & 0x1fff) << 16) | (hend0 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		      ((vend1 & 0x1fff) << 16) | (vend0 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		      ((hend3 & 0x1fff) << 16) | (hend2 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		      ((vend3 & 0x1fff) << 16) | (vend2 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		      ((hend5 & 0x1fff) << 16) | (hend4 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		      ((vend5 & 0x1fff) << 16) | (vend4 & 0x1fff));

	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		      ((hend7 & 0x1fff) << 16) | (hend6 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		      ((vend7 & 0x1fff) << 16) | (vend6 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		      ((hend9 & 0x1fff) << 16) | (hend8 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
		      ((hend11 & 0x1fff) << 16) | (hend10 & 0x1fff));
	WRITE_VPP_REG(LC_STTS_HIST_SET_REGION, h_num);
}

static void lc_stts_en(int enable,
		       unsigned int height,
	unsigned int width,
	int pix_drop_mode,
	int eol_en,
	int hist_mode,
	int lpf_en,
	int din_sel,
	int bitdepth,
	int flag,
	int flag_full)
{
	int data32;

	WRITE_VPP_REG(LC_STTS_GCLK_CTRL0, 0x0);
	WRITE_VPP_REG(LC_STTS_WIDTHM1_HEIGHTM1,
		      ((width - 1) << 16) | (height - 1));
	data32 = 0x80000000 |  ((pix_drop_mode & 0x3) << 29);
	data32 = data32 | ((eol_en & 0x1) << 28);
	data32 = data32 | ((hist_mode & 0x3) << 22);
	data32 = data32 | ((lpf_en & 0x1) << 21);
	WRITE_VPP_REG(LC_STTS_HIST_REGION_IDX, data32);

	if (flag == 0x3) {
		lc_mtx_set(STAT_MTX, LC_MTX_YUV601L_RGB,
			   enable, bitdepth);
	} else {
		if (flag_full == 1)
			lc_mtx_set(STAT_MTX, LC_MTX_YUV709_RGB,
				   enable, bitdepth);
		else
			lc_mtx_set(STAT_MTX, LC_MTX_YUV709L_RGB,
				   enable, bitdepth);
	}

	WRITE_VPP_REG_BITS(LC_STTS_CTRL0, din_sel, 3, 3);
	/*lc input probe enable*/
	WRITE_VPP_REG_BITS(LC_STTS_CTRL0, 1, 10, 1);
	/*lc hist stts enable*/
	WRITE_VPP_REG_BITS(LC_STTS_HIST_REGION_IDX, enable, 31, 1);
	WRITE_VPP_REG_BITS(LC_STTS_BLACK_INFO,
			   lc_tune_curve.lc_reg_thd_black,
			   0, 8);
}

static void lc_curve_ctrl_config(int enable,
				 unsigned int height, unsigned int width)
{
	unsigned int lc_histvld_thrd;
	unsigned int lc_blackbar_mute_thrd;
	unsigned int lmtrat_minmax;
	int h_num, v_num;

	h_num = 12;
	v_num = 8;

	lmtrat_minmax = (READ_VPP_REG(LC_CURVE_LMT_RAT) >> 8) & 0xff;
	lc_histvld_thrd =  ((height * width) /
		(h_num * v_num) * lmtrat_minmax) >> 10;
	lc_blackbar_mute_thrd = ((height * width) / (h_num * v_num)) >> 3;

	if (!enable) {
		WRITE_VPP_REG_BITS(LC_CURVE_CTRL, enable, 0, 1);
	} else {
		WRITE_VPP_REG(LC_CURVE_HV_NUM, (h_num << 8) | v_num);
		WRITE_VPP_REG(LC_CURVE_HISTVLD_THRD, lc_histvld_thrd);
		WRITE_VPP_REG(LC_CURVE_BB_MUTE_THRD, lc_blackbar_mute_thrd);

		WRITE_VPP_REG_BITS(LC_CURVE_CTRL, enable, 0, 1);
		WRITE_VPP_REG_BITS(LC_CURVE_CTRL, enable, 31, 1);
	}
}

static void lc_blk_bdry_config(unsigned int height, unsigned int width)
{
	unsigned int value;

	width /= 12;
	height /= 8;

	/*lc curve mapping block IDX default 4k panel*/
	value = width & GET_BITS(0, 14);
	value |= (0 << 16) & GET_BITS(16, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_HIDX_0_1, value);
	value = (width * 3) & GET_BITS(0, 14);
	value |= ((width * 2) << 16) & GET_BITS(16, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_HIDX_2_3, value);
	value = (width * 5) & GET_BITS(0, 14);
	value |= ((width * 4) << 16) & GET_BITS(16, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_HIDX_4_5, value);
	value = (width * 7) & GET_BITS(0, 14);
	value |= ((width * 6) << 16) & GET_BITS(16, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_HIDX_6_7, value);
	value = (width * 9) & GET_BITS(0, 14);
	value |= ((width * 8) << 16) & GET_BITS(16, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_HIDX_8_9, value);
	value = (width * 11) & GET_BITS(0, 14);
	value |= ((width * 10) << 16) & GET_BITS(16, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_HIDX_10_11, value);
	value = width & GET_BITS(0, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_HIDX_12, value);

	value = height & GET_BITS(0, 14);
	value |= (0 << 16) & GET_BITS(16, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_VIDX_0_1, value);
	value = (height * 3) & GET_BITS(0, 14);
	value |= ((height * 2) << 16) & GET_BITS(16, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_VIDX_2_3, value);
	value = (height * 5) & GET_BITS(0, 14);
	value |= ((height * 4) << 16) & GET_BITS(16, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_VIDX_4_5, value);
	value = (height * 7) & GET_BITS(0, 14);
	value |= ((height * 6) << 16) & GET_BITS(16, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_VIDX_6_7, value);
	value = height & GET_BITS(0, 14);
	WRITE_VPP_REG(SRSHARP1_LC_CURVE_BLK_VIDX_8, value);
}

static void lc_top_config(int enable, int h_num, int v_num,
			  unsigned int height, unsigned int width, int bitdepth,
	int flag, int flag_full)
{
	/*lcinput_ysel*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_INPUT_MUX, 5, 4, 3);
	/*lcinput_csel*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_INPUT_MUX, 5, 0, 3);

	/*lc ram write h num*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_HV_NUM, h_num, 8, 5);
	/*lc ram write v num*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_HV_NUM, v_num, 0, 5);

	/*lc hblank*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, 8, 8, 8);
	/*lc blend mode,default 1*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, 1, 0, 1);
	/*lc curve mapping  config*/
	lc_blk_bdry_config(height, width);
	/*LC sync ctl*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, 0, 16, 1);
	/*lc enable need set at last*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, enable, 4, 1);

	if (flag == 0x3) {
		/* bt601 use 601 matrix */
		lc_mtx_set(INP_MTX, LC_MTX_YUV601L_RGB, 1, bitdepth);
		lc_mtx_set(OUTP_MTX, LC_MTX_RGB_YUV601L, 1, bitdepth);
	} else {
		/* all other cases use 709 by default */
		/* to do, should we handle bg2020 separately? */
		/* for special signal, keep full range to avoid clipping */
		if (flag_full == 1) {
			lc_mtx_set(INP_MTX, LC_MTX_YUV709_RGB, 1, bitdepth);
			lc_mtx_set(OUTP_MTX, LC_MTX_RGB_YUV709, 1, bitdepth);
		} else {
			lc_mtx_set(INP_MTX, LC_MTX_YUV709L_RGB, 1, bitdepth);
			lc_mtx_set(OUTP_MTX, LC_MTX_RGB_YUV709L, 1, bitdepth);
		}
	}
}

void lc_disable(void)
{
	/*lc enable need set at last*/
	WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, 0, 4, 1);
	WRITE_VPP_REG_BITS(LC_CURVE_CTRL, 0, 0, 1);
	WRITE_VPP_REG_BITS(LC_CURVE_RAM_CTRL, 0, 0, 1);
	/*lc hist stts enable*/
	WRITE_VPP_REG_BITS(LC_STTS_HIST_REGION_IDX, 0, 31, 1);
	if (!lc_malloc_ok) {
		lc_en_chflg = 0x0;
		return;
	}
	memset(lc_hist, 0, LC_HIST_SIZE * sizeof(int));
	memset(lc_szcurve, 0, LC_CURV_SIZE * sizeof(int));
	memset(curve_nodes_cur, 0, LC_CURV_SIZE * sizeof(int));
	memset(curve_nodes_pre, 0, LC_CURV_SIZE * sizeof(int));
	memset(curve_nodes_pre_raw, 0, LC_CURV_SIZE * sizeof(int64_t));
	lc_en_chflg = 0x0;
}

/* detect super white and super black currently */
static int signal_detect(unsigned short *hist)
{
	unsigned short bin0, bin1, bin62, bin63;

	bin0 = hist[0];
	bin1 = hist[1];
	bin62 = hist[62];
	bin63 = hist[63];

	if (amlc_debug == 0xe) {
		pr_info("bin0=%d bin1=%d bin62=%d bin63=%d\n",
			bin0, bin1, bin62, bin63);
		amlc_debug = 0x0;
	}

	if (((bin0 + bin1) > detect_signal_range_threshold_black) ||
	    ((bin62 + bin63) > detect_signal_range_threshold_white))
		return 1;

	return 0;
}

static void lc_config(int enable,
		      struct vframe_s *vf,
	unsigned int sps_h_en,
	unsigned int sps_v_en,
	unsigned int sps_w_in,
	unsigned int sps_h_in,
	int bitdepth)
{
	int h_num, v_num;
	unsigned int height, width;
	static unsigned int vf_height, vf_width, flag_full_pre;
	static unsigned int sps_w_in_pre, sps_h_in_pre;
	unsigned int flag, flag_full;

	h_num = 12;
	v_num = 8;

	if (!vf) {
		vf_height = 0;
		vf_width = 0;
		return;
	}

	/* try to detect out of spec signal level */
	flag_full = 0;
	if (detect_signal_range_en == 2) {
		flag_full = signal_detect(vf->prop.hist.vpp_gamma);
		if (vf->type & VIDTYPE_RGB_444)
			flag_full = 1;
	} else {
		flag_full = detect_signal_range_en;
	}

	if (flag_full != flag_full_pre ||
		sps_h_in_pre != sps_h_in ||
		sps_w_in_pre != sps_w_in) {
		pr_amlc_dbg("signal/sps_w_in/sps_h_in changed, flag_full:%d->%d sps_w_in:%d->%d sps_h_in:%d->%d\n",
				flag_full_pre,
				flag_full,
				sps_w_in_pre,
				sps_w_in,
				sps_h_in_pre,
				sps_h_in);
	}

	if (vf_height == vf->height &&
	    vf_width == vf->width &&
	    flag_full == flag_full_pre &&
	    sps_w_in_pre == sps_w_in &&
	    sps_h_in_pre == sps_h_in &&
	    lc_en_chflg) {
		return;
	}

	flag_full_pre = flag_full;
	height = sps_h_in << sps_h_en;
	width = sps_w_in << sps_v_en;
	sps_w_in_pre = sps_w_in;
	sps_h_in_pre = sps_h_in;

	vf_height = vf->height;
	vf_width = vf->width;

	lc_en_chflg = 0xff;

/*
 *	bit 29: present_flag
 *	bit 23-16: color_primaries
 *	"unknown", "bt709", "undef", "bt601",
 *	"bt470m", "bt470bg", "smpte170m", "smpte240m",
 *	"film", "bt2020"
 */
	if (vf->signal_type & (1 << 29))
		/* this field is present */
		flag = ((vf->signal_type >> 16) & 0xff);
	else
		/* signal_type is not present */
		/* use default value bt709 */
		flag = 0x1;
	lc_top_config(enable, h_num, v_num, height,
		      width, bitdepth, flag, flag_full);

	width = sps_w_in;
	height = sps_h_in;

	lc_curve_ctrl_config(enable, height, width);
	lc_stts_blk_config(enable, height, width);
	lc_stts_en(enable, height, width, 0, 0, 1, 1, 4,
		   bitdepth, flag, flag_full);
}

static void tune_nodes_patch_init(void)
{
	lc_tune_curve.lc_reg_lmtrat_sigbin = 1024;
	lc_tune_curve.lc_reg_lmtrat_thd_max = 950;
	lc_tune_curve.lc_reg_lmtrat_thd_black = 200;
	lc_tune_curve.lc_reg_thd_black = 0x20;
	lc_tune_curve.ypkbv_black_thd = 500;
	lc_tune_curve.yminv_black_thd = 256;
}

static void tune_nodes_patch(int *omap, int *ihistogram, int i, int j)
{
	int yminv_org, minbv_org, pkbv_org;
	int maxbv_org, ymaxv_org, ypkbv_org;
	int yminv, ymaxv, ypkbv;
	int k, alpha, bin_pk, idx_pk, bin_pk2nd;
	int pksum, pksum_2nd, dist;
	int amount;
	int thrd_sglbin;   /*u24 maybe 80% or 90%*/
	int thrd_sglbin_thd_max;   /*u24 maybe 80% or 90%*/
	int thrd_sglbin_thd_black;
	int alpha_sgl;
	int tmp_peak_val;/*pksum; bin_pk*/
	int idx_pk2nd;

	yminv_org = omap[0];
	minbv_org = omap[1];
	pkbv_org = omap[2];
	maxbv_org = omap[3];
	ymaxv_org = omap[4];
	ypkbv_org = omap[5];
	if (amlc_debug == 0xa) {
		pr_info("minBV_org=%d yminV_org=%d pkBV_org=%d ypkBV_org=%d maxBV_org=%d ymaxV_org=%d\n",
			minbv_org, yminv_org, pkbv_org, ypkbv_org,
			maxbv_org, ymaxv_org);
		amlc_debug = 0x0;
	}
	idx_pk = (pkbv_org + 32) >> 6;
	bin_pk2nd = 0;
	amount = 0;
	bin_pk = 0;
	idx_pk2nd = 0;

	for (k = 0; k < HIST_BIN; k++) {
		amount += ihistogram[k];
		if (ihistogram[k] > bin_pk) {
			bin_pk = ihistogram[k];
			idx_pk = k;
		}
	}
	pksum = (idx_pk <= 1 || idx_pk >= 14) ? ihistogram[idx_pk] :
		(ihistogram[idx_pk] + ihistogram[max(min(idx_pk - 1, 14), 1)]
			+ ihistogram[max(min(idx_pk + 1, 14), 1)]);

	for (k = 0; k < HIST_BIN; k++) {
		if (k < (idx_pk - 1) || k > (idx_pk + 1)) {
			if (ihistogram[k] > bin_pk2nd) {
				bin_pk2nd = ihistogram[k];
				idx_pk2nd = k;
			}
		}
	}
	pksum_2nd = (idx_pk2nd <= 1 || idx_pk2nd >= 14) ?
		ihistogram[idx_pk2nd] :
		(ihistogram[idx_pk2nd] +
			ihistogram[max(min(idx_pk2nd - 1, 14), 1)] +
			ihistogram[max(min(idx_pk2nd + 1, 14), 1)]);

	dist = abs(idx_pk - idx_pk2nd);
	if (bin_pk2nd == 0) {
		alpha = 0;
	} else {
		if ((pksum_2nd >= (pksum * 4 / 16)) && dist > 2)
			alpha = min(dist * 8, 64);
		else
			alpha = 0;
	}
	yminv = yminv_org + (minbv_org - yminv_org) * alpha / 64;
	ymaxv = ymaxv_org + (maxbv_org - ymaxv_org) * alpha / 64;

	/*2. tune ypkBV if single peak
	 * int reg_lmtrat_sigbin = 922; 95% 1024*0.95 = 973 / 90% 1024*0.9 = 922
	 * 85% 1024*0.85 = 870 / 80% 1024*0.8 = 819
	 */

	 /*u24 maybe 80% or 90%*/
	thrd_sglbin = (lc_tune_curve.lc_reg_lmtrat_sigbin * amount) >> 10;
	thrd_sglbin_thd_max =
		(lc_tune_curve.lc_reg_lmtrat_thd_max * amount) >> 10;
	alpha_sgl = 0;
	tmp_peak_val = bin_pk;/*pksum; bin_pk;*/
	if (tmp_peak_val > thrd_sglbin) {
		alpha_sgl = (tmp_peak_val - thrd_sglbin) * 1024 /
			(thrd_sglbin_thd_max - thrd_sglbin);
		if (alpha_sgl > 1024)
			alpha_sgl = 1024;
	}

	ypkbv = ypkbv_org + (pkbv_org - ypkbv_org) * alpha_sgl / 1024;
	yminv = yminv_org + (minbv_org - yminv_org) * alpha_sgl / 1024;

	/*3. check black info, global checking
	 */
	thrd_sglbin_thd_black =
		(lc_tune_curve.lc_reg_lmtrat_thd_black * amount) >> 10;
	if (lc_tune_curve.lc_reg_black_count >= thrd_sglbin_thd_black &&
	    minbv_org < lc_tune_curve.yminv_black_thd) {
		yminv = minbv_org;
	}

	if (lc_tune_curve.lc_reg_black_count >= thrd_sglbin_thd_black &&
	    pkbv_org < lc_tune_curve.ypkbv_black_thd) {
		ypkbv = pkbv_org;
	}

	omap[0] = yminv;
	omap[4] = ymaxv;
	omap[5] = ypkbv;
	if (amlc_debug == 0xc &&
	    lc_node_prcnt > 0) {
		if (i == lc_node_pos_v && j == lc_node_pos_h)
			pr_info("black_count=%x thd_black=%x amount %x\n",
				lc_tune_curve.lc_reg_black_count,
				thrd_sglbin_thd_black, amount);

		if (i == lc_node_pos_v && j == lc_node_pos_h)
			pr_info("pos:%d %d yminV_org=%d yminV=%d ymaxV_org=%d ymaxV=%d\n",
				i, j, yminv_org, yminv, ymaxv_org, ymaxv);

		if (i == lc_node_pos_v && j == lc_node_pos_h)
			pr_info("ypkBV_org=%d ypkBV=%d\n", ypkbv_org, ypkbv);
	}
}

static void linear_nodes_patch(int *omap)
{
	omap[0] = 0;
	omap[1] = 0;
	omap[2] = 512;
	omap[3] = 1023;
	omap[4] = 1023;
	omap[5] = 512;
}

static void lc_demo_wr_curve(int h_num, int v_num)
{
	int i, j, temp1, temp2;

	for (i = 0; i < v_num; i++) {
		for (j = 0; j < h_num / 2; j++) {
			temp1 = lc_szcurve[6 * (i * h_num + j) + 0] |
				(lc_szcurve[6 *
					(i * h_num + j) + 1] << 10) |
				(lc_szcurve[6 *
					(i * h_num + j) + 2] << 20);
			temp2 = lc_szcurve[6 * (i * h_num + j) + 3] |
				(lc_szcurve[6 *
					(i * h_num + j) + 4] << 10) |
				(lc_szcurve[6 *
					(i * h_num + j) + 5] << 20);
			VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA, temp1);
			VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA, temp2);
		}
		for (j = h_num / 2; j < h_num; j++) {
			VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
					    (0 | (0 << 10) | (512 << 20)));
			VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
					    (1023 | (1023 << 10) | (512 << 20)));
		}
	}
}

static int lc_demo_check_curve(int h_num, int v_num)
{
	int i, j, temp, temp1, flag;

	flag = 0;
	for (i = 0; i < v_num; i++) {
		for (j = 0; j < h_num / 2; j++) {
			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			temp1 = lc_szcurve[6 * (i * h_num + j) + 0] |
				(lc_szcurve[6 * (i * h_num + j) + 1] << 10) |
				(lc_szcurve[6 * (i * h_num + j) + 2] << 20);
			if (temp != temp1)
				flag = (2 * (i * h_num + j) + 0) | (1 << 31);

			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			temp1 = lc_szcurve[6 * (i * h_num + j) + 3] |
				(lc_szcurve[6 * (i * h_num + j) + 4] << 10) |
				(lc_szcurve[6 * (i * h_num + j) + 5] << 20);
			if (temp != temp1)
				flag = (2 * (i * h_num + j) + 1) | (1 << 31);
		}
		for (j = h_num / 2; j < h_num; j++) {
			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			if (temp != (0 | (0 << 10) | (512 << 20)))
				flag = (2 * (i * h_num + j) + 0) | (1 << 31);
			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			if (temp != (1023 | (1023 << 10) | (512 << 20)))
				flag = (2 * (i * h_num + j) + 1) | (1 << 31);
		}
	}
	return flag;
}

static int set_lc_curve(int binit, int bcheck)
{
	int i, h_num, v_num;
	unsigned int hvtemp;
	int rflag;
	int temp, temp1;

	rflag = 0;
	hvtemp = READ_VPP_REG(SRSHARP1_LC_HV_NUM);
	h_num = (hvtemp >> 8) & 0x1f;
	v_num = hvtemp & 0x1f;

	/*data sequence: ymin/minBv/pkBv/maxBv/ymaxv/ypkBv*/
	if (binit) {
		WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_CTRL, 1);
		WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_ADDR, 0);
		for (i = 0; i < h_num * v_num; i++) {
			WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
				      (0 | (0 << 10) | (512 << 20)));
			WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
				      (1023 | (1023 << 10) | (512 << 20)));
		}
		WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_CTRL, 0);
	} else {
		VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_CTRL, 1);
		VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_ADDR, 0);
		if (lc_demo_mode)
			lc_demo_wr_curve(h_num, v_num);
		else
			for (i = 0; i < h_num * v_num; i++) {
				VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
						    lc_szcurve[6 * i + 0] |
					(lc_szcurve[6 * i + 1] << 10) |
					(lc_szcurve[6 * i + 2] << 20));
				VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA,
						    lc_szcurve[6 * i + 3] |
					(lc_szcurve[6 * i + 4] << 10) |
					(lc_szcurve[6 * i + 5] << 20));
			}
		VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_CTRL, 0);
	}

	if (bcheck) {
		if (binit) {
			WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_CTRL, 1);
			WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_ADDR,
				      0 | (1 << 31));
			for (i = 0; i < h_num * v_num; i++) {
				temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
				if (temp != (0 | (0 << 10) | (512 << 20)))
					rflag = (2 * i + 0) | (1 << 31);
				temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
				if (temp != (1023 | (1023 << 10) | (512 << 20)))
					rflag = (2 * i + 1) | (1 << 31);
			}
			WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_CTRL, 0);
		} else {
			VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_CTRL, 1);
			VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_ADDR,
					    0 | (1 << 31));
			if (lc_demo_mode)
				rflag = lc_demo_check_curve(h_num, v_num);
			else
				for (i = 0; i < h_num * v_num; i++) {
					temp =
					READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
					temp1 =
					lc_szcurve[6 * i + 0] |
					(lc_szcurve[6 * i + 1] << 10) |
					(lc_szcurve[6 * i + 2] << 20);
					if (temp != temp1)
						rflag =
						(2 * i + 0) | (1 << 31);
					temp =
					READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
					temp1 =
					lc_szcurve[6 * i + 3] |
					(lc_szcurve[6 * i + 4] << 10) |
					(lc_szcurve[6 * i + 5] << 20);
					if (temp != temp1)
						rflag =
						(2 * i + 1) | (1 << 31);
				}
			VSYNC_WRITE_VPP_REG(SRSHARP1_LC_MAP_RAM_CTRL, 0);
		}
	}

	return rflag;
}

/*
 *function: detect osd and provide osd signal status
 *output:
 *	osd_flag_cnt: signal indicate osd exist (process)
 *	frm_cnt: signal that record the sudden
 *		moment osd appear or disappear
 */
static int osd_det_func(int *osd_flag_cnt,
			int *frm_cnt,
						int blk_hist_sum,
						int *lc_hist,
						int blk_hnum,
						int osd_vnum_strt,
						int osd_vnum_end)
{
	int i, j;
	int min_bin_value, min_bin_percent;
	int osd_flag;
	int valid_blk;
	int percent_norm = (1 << 10) - 1;
	static int osd_hist_cnt;

	osd_flag = 0;
	min_bin_value = 0;

	/*1.1, case 1: osd below,e.g. vnum=5,6(0~7)*/
	osd_hist_cnt = 0;
	for (i = osd_vnum_strt; i <= osd_vnum_end; i++) {/*two line*/
		for (j = 0; j < blk_hnum; j++) {
			min_bin_value =
				lc_hist[(i * blk_hnum + j) *
				(HIST_BIN + 1) + 0];/*first bin*/
			/*black osd means first bin value very large*/
			min_bin_percent =
				(min_bin_value * percent_norm) /
				(blk_hist_sum + 1);
			if (min_bin_percent > min_bv_percent_th)
				osd_hist_cnt++;/*black bin count*/
			if (amlc_debug == 0x2)
				pr_info("v2-1:osd_hist_cnt:%d, min_bin_percent:%d, (th:%d)\n",
					osd_hist_cnt,
					min_bin_percent,
					min_bv_percent_th);
		}
	}
	/*how many block osd occupy*/
	valid_blk = (osd_vnum_end - osd_vnum_strt + 1) * blk_hnum -
		invalid_blk;
	/*we suppose when osd appear,
	 * 1)it came in certain area,
	 * 2)and almost all those area are black
	 */
	if (osd_hist_cnt > valid_blk)
		osd_flag_cnt[1] = 1;
	else
		osd_flag_cnt[1] = 0;
	/*detect the moment osd appear and disappear:*/
	osd_flag = ((osd_flag_cnt[1] & (~(osd_flag_cnt[0]))) +
		((~(osd_flag_cnt[1])) & osd_flag_cnt[0]));/*a^b*/
	/*set osd appear and disappear heavy iir time*/
	if (osd_flag && osd_iir_en)
		*frm_cnt = 60 * ts;

	/*debug*/
	if (amlc_iir_debug_en == 10 || amlc_debug == 0x2) {
		pr_info("osd_log v2:osd_flag_cnt[0]:%d,osd_flag_cnt[1]:%d,osd_flag:%d,frm_cnt:%d\n",
			osd_flag_cnt[0],
			osd_flag_cnt[1],
			osd_flag,
			*frm_cnt);
		if (*frm_cnt <= 0)
			amlc_iir_debug_en = 0;
	}

	return 0;
}

/*
 *note 1: osd appear in 5,6(0,1,2,...7) line in Vnum = 8 situation
 *		if Vnum changed, debug osd location, find
 * vnum_start_below, vnum_end_below(osd below situation,above situation is same)
 *
 * note 2: here just consider 2 situation: osd appear below or appear above
 *
 * function: 2 situation osd detect and provide
 *		osd status signal(osd_flag_cnt&frm_cnt)
 */
static int osd_detect(int *osd_flag_cnt_above,
		      int *osd_flag_cnt_below,
						int *frm_cnt_above,
						int *frm_cnt_below,
						int *lc_hist,
						int blk_hnum)
{
	int k;
	unsigned long blk_hist_sum = 0;

	for (k = 0; k < HIST_BIN; k++)
		blk_hist_sum +=
		lc_hist[(0 * blk_hnum + 0) *
			(HIST_BIN + 1) + k];/*use blk[0,0],16bin*/
	/*above situation*/
	osd_det_func(osd_flag_cnt_above,/*out*/
				frm_cnt_above,/*out*/
				blk_hist_sum,
				lc_hist,
				blk_hnum,
				vnum_start_above,
				vnum_end_above);
	/*below situation*/
	osd_det_func(osd_flag_cnt_below,/*out*/
				frm_cnt_below,/*out*/
				blk_hist_sum,
				lc_hist,
				blk_hnum,
				vnum_start_below,
				vnum_end_below);

	return 0;
}

/*just temporarily define here to avoid grammar error*/
static int video_scene_change_flag_en;
static int video_scene_change_flag;
/*function: detect global scene change signal
 *output: scene_change_flag
 */
int global_scene_change(int *curve_nodes_cur,
			int *curve_nodes_pre,
						int blk_vnum,
						int blk_hnum,
						int *osd_flag_cnt_above,
						int *osd_flag_cnt_below)
{
	int scene_change_flag;
	static int scene_dif[N];
	static int frm_dif[N];
	/*store frame valid block for frm diff calc*/
	static int valid_blk_num[N];
	int frm_dif_osd, vnum_osd;
	int addr_curv1;
	int apl_cur, apl_pre;
	int i, j;

	/*history message delay*/
	for (i = 0; i < N - 1; i++) {
		scene_dif[i] = scene_dif[i + 1];
		frm_dif[i] = frm_dif[i + 1];
		valid_blk_num[i] = valid_blk_num[i + 1];
	}

	if (video_scene_change_flag_en) {
		scene_change_flag =
			video_scene_change_flag;/*2.1 flag from front module*/
	} else {
		/* 2.2.1 use block APL to calculate frame dif:
		 * omap[5]: (yminV), minBV, pkBV, maxBV, (ymaxV),ypkBV
		 */
		frm_dif[N - 1] = 0;/*update current result*/
		scene_dif[N - 1] = 0;
		valid_blk_num[N - 1] = 0;
		frm_dif_osd = 0;
		vnum_osd = 0;
		for (i = 0; i < blk_vnum; i++) {
			for (j = 0; j < blk_hnum; j++) {
				addr_curv1 = (i * blk_hnum + j);
				apl_cur = curve_nodes_cur[addr_curv1 *
					CURV_NODES + 2];/*apl value*/
				apl_pre = curve_nodes_pre[addr_curv1 *
					CURV_NODES + 2];
				frm_dif[N - 1] +=
					abs(apl_cur - apl_pre);/*frame motion*/

				/*when have osd,
				 *	remove them to calc frame motion
				 */
				if (osd_flag_cnt_below[1] &&
				    i >= vnum_start_below &&
				    i <= vnum_end_below) {
					frm_dif_osd += abs(apl_cur - apl_pre);
					vnum_osd =
						vnum_end_below -
						vnum_start_below + 1;
				}

				if (osd_flag_cnt_above[1] &&
				    i >= vnum_start_above &&
				    i <= vnum_end_above) {
					frm_dif_osd += abs(apl_cur - apl_pre);
					vnum_osd =
						vnum_end_above -
						vnum_start_above + 1;
				}
			}
		}
		/*remove osd to calc frame motion */
		frm_dif[N - 1] = frm_dif[N - 1] - frm_dif_osd;
		valid_blk_num[N - 1] = (blk_vnum - vnum_osd) * blk_hnum;
		/*debug*/
		if (amlc_debug == 0x4) {
			pr_info("#vnum_osd = %d;\n", vnum_osd);
			pr_info("#valid_blk_num[%d] =  %d\n",
				N - 1, valid_blk_num[N - 1]);
			pr_info("#valid_blk_num[%d] =  %d\n",
				N - 2, valid_blk_num[N - 2]);
		}

		/*2.2.2motion dif.if motion dif too large,
		 *	we think scene changed
		 */
		scene_dif[N - 1] =
			abs((frm_dif[N - 1] / (valid_blk_num[N - 1] + 1)) -
			(frm_dif[N - 2] / (valid_blk_num[N - 2] + 1)));

		if (scene_dif[N - 1] > scene_change_th)
			scene_change_flag = 1;
		else
			scene_change_flag = 0;

		/*debug*/
		if (scene_dif[N - 1] > scene_change_th && amlc_iir_debug_en) {
			for (i = 0; i < N; i++)
				pr_info("valid_blk_num[%d] = %d,\n",
					i, valid_blk_num[i]);
			for (i = 0; i < N; i++)
				pr_info("frm_dif[%d] = %d,\n",
					i, frm_dif[i]);
			pr_info("\n\n");
			for (i = 0; i < N; i++)
				pr_info("scene_dif[%d] = %d,\n",
					i, scene_dif[i]);
			pr_info("scene_change_flag =%d\n\n",
				scene_change_flag);
		}
	}
	return scene_change_flag;
}

/*
 *function: set tiir alpha based on different situation
 *input: scene_change_flag, frm_cnt(frm_cnt_above,frm_cnt_below)
 *out:refresh_alpha[96]
 */
int cal_iir_alpha(int *refresh_alpha,
		  int blk_vnum,
					int blk_hnum,
					int refresh,
					int scene_change_flag,
					int frm_cnt_above,
					int frm_cnt_below)
{
	int addr_curv1;
	int osd_local_p, osd_local_m;
	int i, j, k;

	/* 3.1 global scene change,highest priority */
	if (scene_change_flag) {/*only use current curve*/
		for (i = 0; i < blk_vnum; i++) {
			for (j = 0; j < blk_hnum; j++) {
				for (k = 0; k < CURV_NODES; k++) {
					addr_curv1 = (i * blk_hnum + j);
					refresh_alpha[addr_curv1] = refresh;
				}
			}
		}
	} else {/*time domain alpha blend, may need optimize*/
		for (i = 0; i < blk_vnum; i++) {
			for (j = 0; j < blk_hnum; j++) {
				addr_curv1 = (i * blk_hnum + j);
				refresh_alpha[addr_curv1] =
					alpha1;/*normal iir*/
			}
		}
	}

	/*3.2 osd situation-1, osd below */
	if (frm_cnt_below > 0) {
		osd_local_p = max(vnum_start_below - 1, 0);
		osd_local_m = min(vnum_end_below + 1, blk_vnum);

		for (i = osd_local_p; i <= osd_local_m; i++) {
			for (j = 0; j < blk_hnum; j++) {
				addr_curv1 = (i * blk_hnum + j);
				/*osd around(-1,osd,+1) use heavy iir*/
				refresh_alpha[addr_curv1] = alpha2;
			}
		}
	}
	/*3.2 osd situation-2, osd above */
	if (frm_cnt_above > 0) {
		osd_local_p = max(vnum_start_below - 1, 0);
		osd_local_m = min(vnum_end_below + 1, blk_vnum);

		for (i = osd_local_p; i <= osd_local_m; i++) {
			for (j = 0; j < blk_hnum; j++) {
				addr_curv1 = (i * blk_hnum + j);
				/*osd around use heavy iir*/
				refresh_alpha[addr_curv1] = alpha2;
			}
		}
	}

	return 0;
}

/*function: curve iir process
 *	out: curve_nodes_cur(after iir)
 */
int cal_curv_iir(int *curve_nodes_cur,
		 int *curve_nodes_pre,
				int *refresh_alpha,
				int blk_vnum,
				int blk_hnum,
				int refresh)
{
	int i, j, k;
	s64 tmap[CURV_NODES], node_pre_raw;
	int addr_curv1, addr_curv2;
	int node_cur;

	for (i = 0; i < blk_vnum; i++) {
		for (j = 0; j < blk_hnum; j++) {
			addr_curv1 = (i * blk_hnum + j);
			for (k = 0; k < CURV_NODES; k++) {
				addr_curv2 =
					(i * blk_hnum + j) * CURV_NODES + k;
				node_cur =/*u12 calc*/
					(curve_nodes_cur[addr_curv2] << 2);
				node_pre_raw =
					curve_nodes_pre_raw[addr_curv2];
				tmap[k] =
					(node_cur * refresh_alpha[addr_curv1])
					+ ((node_pre_raw *
					(refresh - refresh_alpha[addr_curv1]))
						>> refresh_bit);
				curve_nodes_pre_raw[addr_curv2] =
					tmap[k];
				tmap[k] = (tmap[k] + (1 << (1 + refresh_bit)))
							>> (2 + refresh_bit);
				/*output the iir result*/
				curve_nodes_cur[addr_curv2] =
					(int)(tmap[k]);/*back to u10*/
				/*delay for next iir*/
				curve_nodes_pre[addr_curv2] =
					(int)(tmap[k]);
			}
		}
	}
	return 0;
}

/*
 * attention !
 * note 1:
 *		video_scene_change_flag_en
 *		video_scene_change_flag
 *should from front module,indicate scene changed;
 *
 * Note 2: OSD appear location
 *      vnum_start_below
 *      vnum_end_below (case1)
 *      vnum_start_above
 *      vnum_end_above (case2)
 */

 /* iir algorithm top level path */
static void lc_fw_curve_iir(struct vframe_s *vf,
			    int *lc_hist,
							int *lc_szcurve,
							int blk_vnum,
							int blk_hnum)
{
	int i;
	int scene_change_flag;
	/* osd detect */
	static int frm_cnt_below, frm_cnt_above;
	static int osd_flag_cnt_below[2];
	static int osd_flag_cnt_above[2];
	/* alpha blend init */
	int refresh_alpha[96] = {0};/*96=vnum*hnum*/
	int refresh = 1 << refresh_bit;
	int temp1, temp2;

	if (!vf)
		return;

	if (!lc_curve_fresh)
		goto stop_curvefresh;
	/* pre: get curve nodes from szCurveInfo and save to curve_nodes_cur*/
	for (i = 0; i < 580; i++)/*12*8*6+4*/
		curve_nodes_cur[i] = lc_szcurve[i];
	/* pre: osd flag delay*/
	osd_flag_cnt_below[0] = osd_flag_cnt_below[1];
	osd_flag_cnt_above[0] = osd_flag_cnt_above[1];

	if (amlc_debug == 0xc &&
	    lc_node_prcnt > 0) {
		temp1 =
		curve_nodes_cur[(lc_node_pos_v * 12 + lc_node_pos_h) * 6 + 5];
		temp2 =
		curve_nodes_pre[(lc_node_pos_v * 12 + lc_node_pos_h) * 6 + 5];
		pr_info("before iir ypkBV[%d][%d]=%d, ypkBVpre[%d][%d]=%d\n",
			lc_node_pos_v,
			lc_node_pos_h,
			temp1,
			lc_node_pos_v,
			lc_node_pos_h,
			temp2);
	}

	/* step 1: osd detect*/
	osd_detect(osd_flag_cnt_above,/*out*/
		  osd_flag_cnt_below,/*out*/
		  &frm_cnt_above,/*out*/
		  &frm_cnt_below,/*out: osd case heavy iir time */
		  lc_hist,
		  blk_hnum);
	/*step 2: scene change signal get: two method*/
	scene_change_flag = global_scene_change(curve_nodes_cur,
						curve_nodes_pre,
						blk_vnum,
						blk_hnum,
						osd_flag_cnt_above,
						osd_flag_cnt_below);
	if (scene_change_flag) {
		memset(osd_flag_cnt_below, 0, sizeof(int) * 2);
		memset(osd_flag_cnt_above, 0, sizeof(int) * 2);
		frm_cnt_below = 0;
		frm_cnt_above = 0;
	}

	/* step 3: set tiir alpha based on different situation */
	cal_iir_alpha(refresh_alpha,/*out*/
		      blk_vnum,
		      blk_hnum,
		      refresh,
		      scene_change_flag,
		      frm_cnt_above,
		      frm_cnt_below);

	/* step 4: iir filter */
	cal_curv_iir(curve_nodes_cur,/*out: iir-ed curve*/
		     /*out: store previous frame curve */
		     curve_nodes_pre,
		     refresh_alpha,
		     blk_vnum,
		     blk_hnum,
		     refresh);

	if (amlc_debug == 0xc &&
	    lc_node_prcnt > 0) {
		temp1 =
		curve_nodes_cur[(lc_node_pos_v * 12 + lc_node_pos_h) * 6 + 5];
		temp2 =
		curve_nodes_pre[(lc_node_pos_v * 12 + lc_node_pos_h) * 6 + 5];
		pr_info("after iir ypkBV[%d][%d]=%d, ypkBVpre[%d][%d]=%d\n",
			lc_node_pos_v,
			lc_node_pos_h,
			temp1,
			lc_node_pos_v,
			lc_node_pos_h,
			temp2);
	}

	for (i = 0; i < 580; i++)
		lc_szcurve[i] = curve_nodes_cur[i];/*output*/

	frm_cnt_below--;
	if (frm_cnt_below < 0)
		frm_cnt_below = 0;

	frm_cnt_above--;
	if (frm_cnt_above < 0)
		frm_cnt_above = 0;
	return;

stop_curvefresh:
	for (i = 0; i < 580; i++)
		lc_szcurve[i] = curve_nodes_cur[i];/*output*/
}

void lc_read_region(int blk_vnum, int blk_hnum)
{
	int i, j, k;
	int data32;
	int rgb_min, rgb_max;
	unsigned int temp1, temp2, cur_block;

	lc_tune_curve.lc_reg_black_count =
		READ_VPP_REG_BITS(LC_STTS_BLACK_INFO, 8, 24);
	lc_tune_curve.lc_reg_black_count =
		lc_tune_curve.lc_reg_black_count / 96;

	WRITE_VPP_REG_BITS(LC_STTS_HIST_REGION_IDX, 1, 14, 1);
	data32 = READ_VPP_REG(LC_STTS_HIST_START_RD_REGION);

	WRITE_VPP_REG(LC_CURVE_RAM_CTRL, 1);
	WRITE_VPP_REG(LC_CURVE_RAM_ADDR, 0);
	for (i = 0; i < blk_vnum; i++) {
		for (j = 0; j < blk_hnum; j++) {
			cur_block = i * blk_hnum + j;
			/*part1: get lc curve node*/
			temp1 = READ_VPP_REG(LC_CURVE_RAM_DATA);
			temp2 = READ_VPP_REG(LC_CURVE_RAM_DATA);
			lc_szcurve[cur_block * 6 + 0] =
				temp1 & 0x3ff;/*bit0:9*/
			lc_szcurve[cur_block * 6 + 1] =
				(temp1 >> 10) & 0x3ff;/*bit10:19*/
			lc_szcurve[cur_block * 6 + 2] =
				(temp1 >> 20) & 0x3ff;/*bit20:29*/
			lc_szcurve[cur_block * 6 + 3] =
				temp2 & 0x3ff;/*bit0:9*/
			lc_szcurve[cur_block * 6 + 4] =
				(temp2 >> 10) & 0x3ff;/*bit10:19*/
			lc_szcurve[cur_block * 6 + 5] =
				(temp2 >> 20) & 0x3ff;/*bit20:29*/

			/*part2: get lc hist*/
			data32 = READ_VPP_REG(LC_STTS_HIST_START_RD_REGION);
			if (i >= lc_hist_vs && i <= lc_hist_ve &&
			    j >= lc_hist_hs && j <= lc_hist_he &&
			    amlc_debug == 0x6 &&
			    lc_hist_prcnt)
				pr_info("========[r,c](%2d,%2d)======\n", i, j);

			for (k = 0; k < 17; k++) {
				data32 = READ_VPP_REG(LC_STTS_HIST_READ_REGION);
				lc_hist[cur_block * 17 + k] = data32;
				if (i >= lc_hist_vs && i <= lc_hist_ve &&
				    j >= lc_hist_hs && j <= lc_hist_he &&
				    amlc_debug == 0x6 &&
				    lc_hist_prcnt) {
					/*print chosen hist*/
					if (k == 16) {/*last bin*/
						rgb_min =
							(data32 >> 10) & 0x3ff;
						rgb_max = (data32) & 0x3ff;
						pr_info("[%2d]:%d,%d\n",
							k, rgb_min, rgb_max);
					} else {
						pr_info("[%2d]:%d\n",
							k, data32);
					}
				}
			}

			/*part3: add tune curve node patch--by vlsi-guopan*/
			if (tune_curve_en == 2) {
				tune_nodes_patch(&lc_szcurve[cur_block * 6],
						 &lc_hist[cur_block * 17],
						 i, j);
			} else if (tune_curve_en == 1) {
				linear_nodes_patch(&lc_szcurve[cur_block * 6]);
			}
		}
	}
	WRITE_VPP_REG(LC_CURVE_RAM_CTRL, 0);

	if (amlc_debug == 0x8 && lc_hist_prcnt)/*print all hist data*/
		for (i = 0; i < 8 * 12 * 17; i++)
			pr_info("%x\n", lc_hist[i]);
	if (lc_hist_prcnt > 0)
		lc_hist_prcnt--;
}

static void lc_prt_curve(void)
{/*print curve node*/
	int i, j;
	int dwtemp, blk_hnum, blk_vnum;

	dwtemp = READ_VPP_REG(LC_CURVE_HV_NUM);
	blk_hnum = (dwtemp >> 8) & 0x1f;
	blk_vnum = (dwtemp) & 0x1f;
	pr_amlc_dbg("======lc_prt curve node=======\n");
	for (i = 0; i < blk_hnum * blk_vnum; i++) {
		for (j = 0; j < 6 ; j++)
			pr_amlc_dbg("%d\n", lc_szcurve[i * 6 + j]);
		pr_amlc_dbg("\n");
	}
}

void lc_init(int bitdepth)
{
	int h_num, v_num;
	unsigned int height, width;
	const struct vinfo_s *vinfo = get_current_vinfo();
	int i, tmp, tmp1, tmp2;

	tune_nodes_patch_init();
	lc_bitdepth = bitdepth;

	height = vinfo->height;
	width = vinfo->width;
	h_num = 12;
	v_num = 8;

	lc_szcurve = kcalloc(LC_CURV_SIZE, sizeof(int), GFP_KERNEL);
	if (!lc_szcurve)
		return;
	curve_nodes_cur = kcalloc(LC_CURV_SIZE, sizeof(int), GFP_KERNEL);
	if (!curve_nodes_cur) {
		kfree(lc_szcurve);
		return;
	}
	curve_nodes_pre = kcalloc(LC_CURV_SIZE, sizeof(int), GFP_KERNEL);
	if (!curve_nodes_pre) {
		kfree(lc_szcurve);
		kfree(curve_nodes_cur);
		return;
	}
	lc_hist = kcalloc(LC_HIST_SIZE, sizeof(int), GFP_KERNEL);
	if (!lc_hist) {
		kfree(lc_szcurve);
		kfree(curve_nodes_cur);
		kfree(curve_nodes_pre);
		return;
	}
	curve_nodes_pre_raw = kcalloc(LC_CURV_SIZE,
				      sizeof(int64_t),
					GFP_KERNEL);
	if (!curve_nodes_pre_raw) {
		kfree(lc_szcurve);
		kfree(curve_nodes_cur);
		kfree(curve_nodes_pre);
		kfree(lc_hist);
		return;
	}
	lc_malloc_ok = 1;
	if (!lc_en)
		return;

	lc_top_config(0, h_num, v_num, height, width, bitdepth, 1, 0);
	WRITE_VPP_REG_BITS(LC_CURVE_RAM_CTRL, 0, 0, 1);

	/*default LC low parameters*/
	WRITE_VPP_REG(LC_CURVE_CONTRAST_LH, 0x000b000b);
	WRITE_VPP_REG(LC_CURVE_CONTRAST_SCL_LH, 0x00000b0b);
	WRITE_VPP_REG(LC_CURVE_MISC0, 0x00023028);
	WRITE_VPP_REG(LC_CURVE_YPKBV_RAT, 0x8cc0c060);
	WRITE_VPP_REG(LC_CURVE_YPKBV_SLP_LMT, 0x00000b3a);

	if (cpu_after_eq_t7()) {
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1, 0x0030005d);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_2_3, 0x00830091);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_4_5, 0x00a000c4);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_6_7, 0x00e00100);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_8_9, 0x01200140);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_10_11, 0x01600190);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_12_13, 0x01b001d0);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_14_15, 0x01f00210);

		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_0_1, 0x004400b4);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_2_3, 0x00fb0123);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_4_5, 0x015901a2);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_6_7, 0x01d90208);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_8_9, 0x02400280);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_10_11, 0x02d70310);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_12_13, 0x03400380);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_14_15, 0x03c003ff);

		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_0_1, 0x004400b4);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_2_3, 0x00fb0123);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_4_5, 0x015901a2);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_6_7, 0x01d90208);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_8_9, 0x02400280);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_10_11, 0x02d70310);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_12_13, 0x03400380);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_14_15, 0x03c003ff);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1, 0x0030005d);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_2_3, 0x00830091);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_4_5, 0x00a000c4);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_6_7, 0x00e00100);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_8_9, 0x01200140);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_10_11, 0x01600190);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_12_13, 0x01b001d0);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_14_15, 0x01f00210);

		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_0_1, 0x004400b4);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_2_3, 0x00fb0123);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_4_5, 0x015901a2);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_6_7, 0x01d90208);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_8_9, 0x02400280);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_10_11, 0x02d70310);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_12_13, 0x03400380);
		WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_14_15, 0x03c003ff);

		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_0_1, 0x004400b4);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_2_3, 0x00fb0123);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_4_5, 0x015901a2);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_6_7, 0x01d90208);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_8_9, 0x02400280);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_10_11, 0x02d70310);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_12_13, 0x03400380);
		WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_14_15, 0x03c003ff);
	} else {
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1, 0x0030005d);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_2_3, 0x00830091);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_4_5, 0x00a000c4);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_6_7, 0x00e00100);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_8_9, 0x01200140);
		WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_10_11, 0x01600190);

		WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_0_1, 0x004400b4);
		WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_2_3, 0x00fb0123);
		WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_4_5, 0x015901a2);
		WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_6_7, 0x01d90208);
		WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_8_9, 0x02400280);
		WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_10_11, 0x02d70310);
	}

	for (i = 0; i < 31 ; i++) {
		tmp1 = *(lc_satur_off + 2 * i);
		tmp2 = *(lc_satur_off + 2 * i + 1);
		tmp = ((tmp1 & 0xfff) << 16) | (tmp2 & 0xfff);
		WRITE_VPP_REG(SRSHARP1_LC_SAT_LUT_0_1 + i, tmp);
	}
	tmp = (*(lc_satur_off + 62)) & 0xfff;
	WRITE_VPP_REG(SRSHARP1_LC_SAT_LUT_62, tmp);
	/*end*/

	if (set_lc_curve(1, 0))
		pr_info("%s: init fail", __func__);
}

bool lc_curve_ctrl_reg_set_flag(unsigned int addr)
{
	bool lc_config_set_flag = false;

	if (addr == LC_CURVE_CTRL) {
		if (lc_en && lc_flag > 1)
			lc_config_set_flag = true;
		else
			lc_config_set_flag = false;
	}

	return lc_config_set_flag;
}

void lc_process(struct vframe_s *vf,
		unsigned int sps_h_en,
	unsigned int sps_v_en,
	unsigned int sps_w_in,
	unsigned int sps_h_in)
{
	int blk_hnum, blk_vnum, dwtemp;

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_TL1)
		return;

	if (!lc_malloc_ok) {
		pr_amlc_dbg("%s: lc malloc fail", __func__);
		return;
	}

	if (!lc_en) {
		lc_disable();
		return;
	}
	if (!vf) {
		if (lc_flag == 0xff) {
			lc_disable();
			lc_flag = 0x0;
			lc_bypass_flag = 0x0;
		}
		return;
	}

	if (lc_flag <= 1) {
		lc_flag++;
		return;
	}
	dwtemp = READ_VPP_REG(LC_CURVE_HV_NUM);
	blk_hnum = (dwtemp >> 8) & 0x1f;
	blk_vnum = (dwtemp) & 0x1f;
	lc_config(lc_en, vf, sps_h_en, sps_v_en,
		  sps_w_in, sps_h_in, lc_bitdepth);

	if (lc_bypass_flag <= 0) {
		set_lc_curve(1, 0);
		lc_bypass_flag++;
		return;
	}

	/*get hist & curve node*/
	if (!use_lc_curve_isr || !lc_curve_isr_defined)
		lc_read_region(blk_vnum, blk_hnum);

	/*do time domain iir*/
	lc_fw_curve_iir(vf, lc_hist,
			lc_szcurve, blk_vnum, blk_hnum);
	if (lc_curve_prcnt > 0) { /*debug lc curve node*/
		lc_prt_curve();
		lc_curve_prcnt--;
	}
	if (set_lc_curve(0, 0))
		pr_amlc_dbg("%s: set lc curve fail", __func__);

	if (amlc_debug == 0xc &&
	    lc_node_prcnt > 0) {
		lc_node_prcnt--;
		if (lc_node_prcnt == 0)
			amlc_debug = 0x0;
	}

	lc_flag = 0xff;
	lc_bypass_flag = 0xff;
}

void lc_free(void)
{
	kfree(lc_szcurve);
	kfree(curve_nodes_cur);
	kfree(curve_nodes_pre);
	kfree(curve_nodes_pre_raw);
	kfree(lc_hist);
	lc_malloc_ok = 0;
}
