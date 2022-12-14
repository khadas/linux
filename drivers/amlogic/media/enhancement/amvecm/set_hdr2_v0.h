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
#ifndef AM_SET_HDR2_V0
#define AM_SET_HDR2_V0

#include <linux/types.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/amvecm/hdr2_ext.h>
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

enum hdr_lut_sel {
	HDR_EOTF_LUT = 1,
	HDR_OOTF_LUT = 2,
	HDR_OETF_LUT = 3,
	HDR_CGAIN_LUT = 4,
	HDR_LUT_MAX
};

enum hdr_hist_sel {
	HIST_E_RGBMAX = 0,
	HIST_E_LUMA = 1,
	HIST_E_SAT = 2,
	HIST_O_BEFORE = 4,
	HIST_O_AFTER = 6,
	HIST_MAX
};

#define OO_BITS			12
#define OO_GAIN_SHIFT	3
#define OO_NOR			(OO_BITS - OO_GAIN_SHIFT)

enum hdr_process_sel hdr_func(enum hdr_module_sel module_sel,
			      u32 hdr_process_select,
			      struct vinfo_s *vinfo,
			      struct matrix_s *gmt_mtx,
			      enum vpp_index_e vpp_index);

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
			   struct hdr10pgen_param_s *hdr10pgen_param,
			   enum vpp_index_e vpp_index);
enum hdr_process_sel hdr10p_func(enum hdr_module_sel module_sel,
				 u32 hdr_process_select,
				 struct vinfo_s *vinfo,
				 struct matrix_s *gmt_mtx,
				 enum vpp_index_e vpp_index);
void set_ootf_lut(enum hdr_module_sel module_sel,
		  struct hdr_proc_lut_param_s *hdr_lut_param,
		  enum vpp_index_e vpp_index);
extern struct hdr_proc_lut_param_s hdr_lut_param;
extern int oo_y_lut_hdr_sdr_def[HDR2_OOTF_LUT_SIZE];
extern int oo_y_lut_hdr_sdr[HDR2_OOTF_LUT_SIZE];
extern int oo_y_lut_hlg_sdr[HDR2_OOTF_LUT_SIZE];
void eo_clip_proc(struct vframe_master_display_colour_s *master_info,
		  unsigned int eo_sel);
int hdr10_tm_update(enum hdr_module_sel module_sel,
		    enum hdr_process_sel hdr_process_select,
		    enum vpp_index_e vpp_index);
extern int cgain_lut_bypass[HDR2_CGAIN_LUT_SIZE];
extern unsigned int hdr10_pr;
extern unsigned int hdr10_clip_disable;
extern unsigned int hdr10_clip_luma;
extern unsigned int hdr10_clip_margin;
void get_hist(enum vd_path_e vd_path,
	      enum hdr_hist_sel hist_sel);
#define NUM_HDR_HIST 16
extern u32 hdr_hist[NUM_HDR_HIST][128];
extern u32 percentile[9];
extern u32 disable_flush_flag;
extern u32 cuva_static_hlg_en;
int cuva_hdr_update(enum hdr_module_sel module_sel,
	enum hdr_process_sel hdr_process_select, enum vpp_index_e vpp_index);
void hdr_reg_dump(unsigned int offset);
int calc_gmut_shift(struct hdr_proc_mtx_param_s *hdr_mtx_param);

#define LDIM_STTS_DMA_ID 0
#define FG0_DMA_ID		 1
#define FG1_DMA_ID		 2
#define FG2_DMA_ID		 3
#define FG3_DMA_ID		 4
#define FG4_DMA_ID		 5
#define FG5_DMA_ID		 6
#define HDR_DMA_ID		 7
#define LUT3D_DMA_ID	 8

enum LUT_DMA_ID_e {
	LDIM_DMA_ID = LDIM_STTS_DMA_ID,
	DI_FG_DMA_ID = FG0_DMA_ID,
	VD1S0_FG_DMA_ID = FG1_DMA_ID,
	VD1S1_FG_DMA_ID = FG2_DMA_ID,
	VD1S2_FG_DMA_ID = FG3_DMA_ID,
	VD1S3_FG_DMA_ID = FG4_DMA_ID,
	VD2_FG_DMA_ID = FG5_DMA_ID,
	HDR2_DMA_ID = HDR_DMA_ID,
	VPP_LUT3D_DMA_ID = LUT3D_DMA_ID
};

struct VPU_LUT_DMA_t {
	enum LUT_DMA_ID_e dma_id;
	//reg_hdr_dma_sel:
	u32 reg_hdr_dma_sel_vd1s0;//4bits, default 1
	u32 reg_hdr_dma_sel_vd1s1;//4bits, default 2
	u32 reg_hdr_dma_sel_vd1s2;//4bits, default 3
	u32 reg_hdr_dma_sel_vd1s3;//4bits, default 4
	u32 reg_hdr_dma_sel_vd2  ;//4bits, default 5
	u32 reg_hdr_dma_sel_osd1 ;//4bits, default 6
	u32 reg_hdr_dma_sel_osd2 ;//4bits, default 7
	u32 reg_hdr_dma_sel_osd3 ;//4bits, default 8
	//reg_dma_mode : 1: cfg hdr2 lut with lut_dma, 0: with cbus mode
	u32 reg_vd1s0_hdr_dma_mode;//1bit
	u32 reg_vd1s1_hdr_dma_mode;//1bit
	u32 reg_vd1s2_hdr_dma_mode;//1bit
	u32 reg_vd1s3_hdr_dma_mode;//1bit
	u32 reg_vd2_hdr_dma_mode;//1bit
	u32 reg_osd1_hdr_dma_mode;//1bit
	u32 reg_osd2_hdr_dma_mode;//1bit
	u32 reg_osd3_hdr_dma_mode;//1bit

	//mif info : TODO more params
	u32 rd_wr_sel;
	u32 mif_baddr[16][4];
	u32 chan_rd_bytes_num[16];//rdmif rd num
	u32 chan_sel_src_num[16];//channel select interrupt source num
	u32 chan_little_endian[16];
	u32 chan_swap_64bit[16];
};
#endif
