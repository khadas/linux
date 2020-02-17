/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/set_hdr2_v0.h
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

#include <linux/types.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include "hdr/am_hdr10_plus_ootf.h"
#include "amcsc.h"

#ifndef MAX
#define MAX(x1, x2) ({ \
	typeof(x1) _x1 = x1; \
	typeof(x2) _x2 = x2; \
	(double)(_x1 > _x2 ? _x1 : _x2); })
#endif

#ifndef POW
#define POW(x1, x2) (int64_t)(pow((double)x1, (double)x2))
#endif

#ifndef LOG2
#define LOG2(x)  ({ \
	typeof(x) _x = x; \
	(int)((_x) == 0 ? 0 : log2((long long)(_x)))
#endif

#define FLTZERO 0xfc000/*float zero*/

#define peak_out  10000/* luma out*/
#define peak_in   1000/* luma in*/
/* 0:hdr10 peak_in input to hdr10 peak_out,*/
/*1:hdr peak_in->gamma,2:gamma->hdr peak out*/
#define fmt_io       2

#define precision  14/* freeze*/
/*input data bitwidth : 12 (VD1 OSD1 VD2)*/
/*10 (VDIN & DI)*/
#define IE_BW      12
#define OE_BW      12/*same IE_BW*/
#define O_BW       32/*freeze*/
#define maxbit     33/*freeze*/
#define OGAIN_BW   12/*freeze*/

/*
 *s64 FloatRev(s64 iA);
 *s64 FloatCon(s64 iA, int MOD);
 */

enum hdr_module_sel {
	VD1_HDR = 1,
	VD2_HDR = 2,
	OSD1_HDR = 3,
	VDIN0_HDR = 4,
	VDIN1_HDR = 5,
	DI_HDR = 6,
	HDR_MAX
};

enum hdr_matrix_sel {
	HDR_IN_MTX = 1,
	HDR_GAMUT_MTX = 2,
	HDR_OUT_MTX = 3,
	HDR_MTX_MAX
};

enum hdr_lut_sel {
	HDR_EOTF_LUT = 1,
	HDR_OOTF_LUT = 2,
	HDR_OETF_LUT = 3,
	HDR_CGAIN_LUT = 4,
	HDR_LUT_MAX
};

enum hdr_process_sel {
	HDR_BYPASS = 1,
	HDR_SDR = 2,
	SDR_HDR = 3,
	HLG_BYPASS = 4,
	HLG_SDR = 5,
	HLG_HDR = 6,
	SDR_HLG = 7,
	SDR_IPT = 8,
	HDR_IPT = 9,
	HLG_IPT = 10,
	HDR_HLG = 11,
	RGB_YUV = 12,
	RGB_HDR = 13,
	RGB_HLG = 14,
	HDR10P_SDR = 15,
	SDR_GMT_CONVERT = 16,
	RGB_YUVF = 17,
	SDR_RGB_GMT_CONV = 18,
	HDR_p_MAX
};

enum hdr_hist_sel {
	HIST_E_RGBMAX = 0,
	HIST_E_LUMA = 1,
	HIST_E_SAT = 2,
	HIST_O_BEFORE = 4,
	HIST_O_AFTER = 6,
	HIST_MAX
};

#define MTX_ON  1
#define MTX_OFF 0

#define MTX_ONLY  1
#define HDR_ONLY  0

#define LUT_ON  1
#define LUT_OFF 0

#define HDR2_EOTF_LUT_SIZE 143
#define HDR2_OOTF_LUT_SIZE 149
#define HDR2_OETF_LUT_SIZE 149
#define HDR2_CGAIN_LUT_SIZE 65

#define MTX_NUM_PARAM 16
struct hdr_proc_mtx_param_s {
	int mtx_only;
	int mtx_in[MTX_NUM_PARAM];
	int mtx_gamut[9];
	int mtx_gamut_mode;
	int mtx_cgain[MTX_NUM_PARAM];
	int mtx_ogain[MTX_NUM_PARAM];
	int mtx_out[MTX_NUM_PARAM];
	int mtxi_pre_offset[3];
	int mtxi_pos_offset[3];
	int mtxo_pre_offset[3];
	int mtxo_pos_offset[3];
	unsigned int mtx_on;
	enum hdr_process_sel p_sel;
};

#define OO_BITS			12
#define OO_GAIN_SHIFT	3
#define OO_NOR			(OO_BITS - OO_GAIN_SHIFT)

struct hdr_proc_lut_param_s {
	s64 eotf_lut[143];
	s64 oetf_lut[149];
	s64 ogain_lut[149];
	s64 cgain_lut[65];
	unsigned int lut_on;
	unsigned int bitdepth;
	unsigned int cgain_en;
	unsigned int hist_en;
};

/*
 *typedef int64_t(*menufun)(int64_t);
 *void eotf_float_gen(s64 *o_out, menufun eotf);
 *void oetf_float_gen(s64 *bin_e, menufun oetf);
 *void nolinear_lut_gen(s64 *bin_c, menufun cgain);
 */
enum hdr_process_sel hdr_func(enum hdr_module_sel module_sel,
			      enum hdr_process_sel hdr_process_select,
			      struct vinfo_s *vinfo,
			      struct matrix_s *gmt_mtx);
/*G12A vpp matrix*/
enum vpp_matrix_e {
	VD1_MTX = 0x1,
	POST2_MTX = 0x2,
	POST_MTX = 0x4
};

enum mtx_csc_e {
	MATRIX_NULL = 0,
	MATRIX_RGB_YUV601 = 0x1,
	MATRIX_RGB_YUV601F = 0x2,
	MATRIX_RGB_YUV709 = 0x3,
	MATRIX_RGB_YUV709F = 0x4,
	MATRIX_YUV601_RGB = 0x10,
	MATRIX_YUV601_YUV601F = 0x11,
	MATRIX_YUV601_YUV709 = 0x12,
	MATRIX_YUV601_YUV709F = 0x13,
	MATRIX_YUV601F_RGB = 0x14,
	MATRIX_YUV601F_YUV601 = 0x15,
	MATRIX_YUV601F_YUV709 = 0x16,
	MATRIX_YUV601F_YUV709F = 0x17,
	MATRIX_YUV709_RGB = 0x20,
	MATRIX_YUV709_YUV601 = 0x21,
	MATRIX_YUV709_YUV601F = 0x22,
	MATRIX_YUV709_YUV709F = 0x23,
	MATRIX_YUV709F_RGB = 0x24,
	MATRIX_YUV709F_YUV601 = 0x25,
	MATRIX_YUV709F_YUV709 = 0x26,
	MATRIX_BT2020YUV_BT2020RGB = 0x40,
	MATRIX_BT2020RGB_709RGB,
	MATRIX_BT2020RGB_CUSRGB,
};

void mtx_setting(enum vpp_matrix_e mtx_sel,
		 enum mtx_csc_e mtx_csc,
	int mtx_on);

unsigned int _log2(unsigned int value);
int hdr10p_ebzcurve_update(enum hdr_module_sel module_sel,
			   enum hdr_process_sel hdr_process_select,
			   struct hdr10pgen_param_s *hdr10pgen_param);
enum hdr_process_sel hdr10p_func(enum hdr_module_sel module_sel,
				 enum hdr_process_sel hdr_process_select,
				 struct vinfo_s *vinfo,
				 struct matrix_s *gmt_mtx);
void set_ootf_lut(enum hdr_module_sel module_sel,
		  struct hdr_proc_lut_param_s *hdr_lut_param);
extern struct hdr_proc_lut_param_s hdr_lut_param;
extern int oo_y_lut_hdr_sdr_def[149];
extern int oo_y_lut_hdr_sdr[149];
void hdr_highclip_by_luma(struct vframe_master_display_colour_s *master_info);
int hdr10_tm_update(enum hdr_module_sel module_sel,
		    enum hdr_process_sel hdr_process_select);
extern int cgain_lut_bypass[65];
extern unsigned int hdr10_pr;
extern unsigned int hdr10_clip_disable;
extern unsigned int hdr10_force_clip;
extern unsigned int hdr10_clip_luma;
extern unsigned int hdr10_clip_margin;
extern unsigned int hdr10_clip_mode;
void get_hist(enum hdr_module_sel module_sel,
	      enum hdr_hist_sel hist_sel);
#define NUM_HDR_HIST 16
extern u32 hdr_hist[NUM_HDR_HIST][128];
extern u32 percentile[9];
