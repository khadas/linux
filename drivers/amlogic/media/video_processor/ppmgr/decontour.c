// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/ppmgr/decontour.c
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
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/registers/cpu_version.h>

#include "decontour.h"
#define uint32_t unsigned int

#define Wr(adr, val) aml_write_vcbus(adr, val)
#define Rd(adr) aml_read_vcbus(adr)

void ini_dcntr_pre(int hsize, int vsize, int grd_num_mode, u32 ratio)
{
	int xsize = hsize;
	int ysize = vsize;
	int reg_in_ds_rate_x;
	int reg_in_ds_rate_y;
	int ds_r_sft_x;
	int ds_r_sft_y;
	int in_ds_r_x;
	int in_ds_r_y;
	int xds;
	int yds;

	u32 grd_num;
	u32 reg_grd_xnum;
	u32 reg_grd_ynum;
	u32 reg_grd_xsize_ds = 0;
	u32 reg_grd_ysize_ds = 0;
	u32 reg_grd_xnum_use;
	u32 reg_grd_ynum_use;
	u32 reg_grd_xsize;
	u32 reg_grd_ysize;
	int grd_path;

	int reg_grd_vbin_gb_0 = 24 + 48 * 0;
	int reg_grd_vbin_gb_1 = 24 + 48 * 1;
	int reg_grd_vbin_gb_2 = 24 + 48 * 2;
	int reg_grd_vbin_gb_3 = 24 + 48 * 3;
	int reg_grd_vbin_gb_4 = 24 + 48 * 4;
	int reg_grd_vbin_gb_5 = 24 + 48 * 5;
	int reg_grd_vbin_gb_6 = 24 + 48 * 6;
	int reg_grd_vbin_gb_7 = 24 + 48 * 7;
	int reg_grd_vbin_gb_8 = 24 + 48 * 8;
	int reg_grd_vbin_gb_9 = 24 + 48 * 9;
	int reg_grd_vbin_gb_10 = 24 + 48 * 10;
	int reg_grd_vbin_gb_11 = 24 + 48 * 11;
	int reg_grd_vbin_gb_12 = 24 + 48 * 12;
	int reg_grd_vbin_gb_13 = 24 + 48 * 13;
	int reg_grd_vbin_gb_14 = 24 + 48 * 14;
	int reg_grd_vbin_gb_15 = 24 + 48 * 15;
	int reg_grd_vbin_gb_16 = 24 + 48 * 16;
	int reg_grd_vbin_gb_17 = 24 + 48 * 17;
	int reg_grd_vbin_gb_18 = 24 + 48 * 18;
	int reg_grd_vbin_gb_19 = 24 + 48 * 19;
	int reg_grd_vbin_gb_20 = 24 + 48 * 20;
	int reg_grd_vbin_gb_21 = 24 + 48 * 21;

	Wr(DCTR_BGRID_TOP_FSIZE, (hsize << 13) | (vsize));

	Wr(DCNTR_GRID_GEN_REG, 1 << 29);

	Wr(DCTR_BGRID_TOP_CTRL0, (1 << 30) |
		(1 << 29) |
		(2 << 16) |
		(0 << 4)  |
		(1 << 2)  |
		(1 << 1)  |
		(1 << 0));

	/*reg_in_ds_rate_x = (hsize > 1920) ? 2 : (hsize > 960) ? 1 : 0;*/
	/*reg_in_ds_rate_y = (vsize > 1080) ? 2 : (vsize > 540) ? 1 : 0;*/

	reg_in_ds_rate_x = ratio;
	reg_in_ds_rate_y = ratio;

	ds_r_sft_x = reg_in_ds_rate_x;
	ds_r_sft_y = reg_in_ds_rate_y;
	in_ds_r_x = 1 << ds_r_sft_x;
	in_ds_r_y = 1 << ds_r_sft_y;
	xds = (hsize + in_ds_r_x - 1) >> ds_r_sft_x;
	yds = (vsize + in_ds_r_y - 1) >> ds_r_sft_y;

	grd_num = Rd(DCTR_BGRID_PARAM1_PRE);

	reg_grd_xnum = grd_num_mode == 0 ? 40 : grd_num_mode == 1 ? 60 : 80;

	reg_grd_ynum = grd_num_mode == 0 ? 23 : grd_num_mode == 1 ? 34 : 45;

	grd_path = reg_in_ds_rate_x == 0 && reg_in_ds_rate_y == 0;

	if (grd_path == 0) {
		reg_grd_xsize_ds = (xds + reg_grd_xnum - 1) / (reg_grd_xnum);
		reg_grd_ysize_ds = (yds + reg_grd_ynum - 1) / (reg_grd_ynum);
		reg_grd_xnum_use = ((xds - reg_grd_xsize_ds / 2)
			+ reg_grd_xsize_ds - 1)
			/ (reg_grd_xsize_ds) + 1;
		reg_grd_ynum_use = ((yds - reg_grd_ysize_ds / 2)
			+ reg_grd_ysize_ds - 1)
			/ (reg_grd_ysize_ds) + 1;
		reg_grd_xsize = reg_grd_xsize_ds * in_ds_r_x;
		reg_grd_ysize = reg_grd_ysize_ds * in_ds_r_y;
	} else {
		reg_grd_xsize = (xsize + reg_grd_xnum - 1) / (reg_grd_xnum);
		reg_grd_ysize = (ysize + reg_grd_ynum - 1) / (reg_grd_ynum);
		reg_grd_xnum_use = ((xsize - reg_grd_xsize / 2)
			+ reg_grd_xsize - 1) / (reg_grd_xsize) + 1;
		reg_grd_ynum_use = ((ysize - reg_grd_ysize / 2)
			+ reg_grd_ysize - 1) / (reg_grd_ysize) + 1;
	}

	Wr(DCTR_BGRID_PATH_PRE, (grd_path << 4));

	Wr(DCTR_DS_PRE,
		(0 << 11) |
		(0 << 9) |
		(8 << 5) |
		(0 << 4) |
		(reg_in_ds_rate_x << 2) |
		reg_in_ds_rate_y);

	Wr(DCTR_BGRID_PARAM2_PRE, (reg_grd_xsize << 24) |
		(reg_grd_ysize << 16) |
		(48 << 8) |
		(22));

	Wr(DCTR_BGRID_PARAM3_PRE, (reg_grd_xnum_use << 16) |
		(reg_grd_ynum_use));

	Wr(DCTR_BGRID_PARAM4_PRE, (reg_grd_xsize_ds << 16) |
		(reg_grd_ysize_ds));

	Wr(DCTR_BGRID_WRAP_CTRL, 1);

	Wr(DCTR_BGRID_PARAM5_PRE_0,
		(reg_grd_vbin_gb_0 << 16) | reg_grd_vbin_gb_1);
	Wr(DCTR_BGRID_PARAM5_PRE_1,
		(reg_grd_vbin_gb_2 << 16) | reg_grd_vbin_gb_3);
	Wr(DCTR_BGRID_PARAM5_PRE_2,
		(reg_grd_vbin_gb_4 << 16) | reg_grd_vbin_gb_5);
	Wr(DCTR_BGRID_PARAM5_PRE_3,
		(reg_grd_vbin_gb_6 << 16) | reg_grd_vbin_gb_7);
	Wr(DCTR_BGRID_PARAM5_PRE_4,
		(reg_grd_vbin_gb_8 << 16) | reg_grd_vbin_gb_9);
	Wr(DCTR_BGRID_PARAM5_PRE_5,
		(reg_grd_vbin_gb_10 << 16) | reg_grd_vbin_gb_11);
	Wr(DCTR_BGRID_PARAM5_PRE_6,
		(reg_grd_vbin_gb_12 << 16) | reg_grd_vbin_gb_13);
	Wr(DCTR_BGRID_PARAM5_PRE_7,
		(reg_grd_vbin_gb_14 << 16) | reg_grd_vbin_gb_15);
	Wr(DCTR_BGRID_PARAM5_PRE_8,
		(reg_grd_vbin_gb_16 << 16) | reg_grd_vbin_gb_17);
	Wr(DCTR_BGRID_PARAM5_PRE_9,
		(reg_grd_vbin_gb_18 << 16) | reg_grd_vbin_gb_19);
	Wr(DCTR_BGRID_PARAM5_PRE_10,
		(reg_grd_vbin_gb_20 << 16) | reg_grd_vbin_gb_21);
}

void set_dcntr_grid_mif(u32 x_start,
	u32 x_end,
	u32 y_start,
	u32 y_end,
	u32 mode,
	u32 canvas_addr0,
	u32 canvas_addr1,
	u32 canvas_addr2,
	u32 pic_struct,
	u32 h_avg)
{
	u32 demux_mode = (mode > 1) ? 0 : mode;
	u32 bytes_per_pixel = (mode > 1) ? 0 : ((mode  == 1) ? 2 : 1);
	u32 burst_size_cr = 0;
	u32 burst_size_cb = 0;
	u32 burst_size_y = 3;
	u32 st_separate_en  = (mode > 1);
	u32 value = 0;

	Wr(DCNTR_GRID_GEN_REG,
		(4 << 19)               |
		(0 << 18)               |
		(demux_mode << 16)      |
		(bytes_per_pixel << 14) |
		(burst_size_cr << 12)   |
		(burst_size_cb << 10)   |
		(burst_size_y << 8)     |
		(0 << 6)                |
		(h_avg << 2)            |
		(st_separate_en << 1)   |
		(0 << 0)
	);

	Wr(DCNTR_GRID_CANVAS0,
		(canvas_addr2 << 16) |
		(canvas_addr1 << 8) |
		(canvas_addr0 << 0)
	);

	Wr(DCNTR_GRID_LUMA_X0, (x_end << 16)  |
		(x_start << 0)
	);
	Wr(DCNTR_GRID_LUMA_Y0, (y_end << 16)    |
		(y_start << 0)
	);

	if (mode > 1) {
		Wr(DCNTR_GRID_CHROMA_X0, ((((x_end + 1) >> 1) - 1) << 16) |
			((x_start + 1) >> 1));
		Wr(DCNTR_GRID_CHROMA_Y0, ((((y_end + 1) >> 1) - 1) << 16) |
			((y_start + 1) >> 1));
	}

	if (pic_struct == 0) {
		Wr(DCNTR_GRID_RPT_LOOP, (0 << 24) |
			(0 << 16)             |
			(0 << 8)              |
			(0 << 0));
		Wr(DCNTR_GRID_LUMA0_RPT_PAT,      0x0);
		Wr(DCNTR_GRID_CHROMA0_RPT_PAT,    0x0);
	} else if ((pic_struct == 3) || (pic_struct == 6)) {
		Wr(DCNTR_GRID_RPT_LOOP, (0 << 24)             |
			(0 << 16)             |
			(0 << 8)              |
			(0 << 0));

		if (pic_struct == 6)
			value = 0xa;
		else if (pic_struct == 3)
			value = 0x8;

		Wr(DCNTR_GRID_LUMA0_RPT_PAT,      value);/*0x8:2 line read 1; 0xa:4 line read 1*/
		Wr(DCNTR_GRID_CHROMA0_RPT_PAT,    value);
	} else if ((pic_struct == 2) || (pic_struct == 5)) {
		Wr(DCNTR_GRID_RPT_LOOP,  (0 << 24)               |
			(0 << 16)               |
			(0x11 << 8)             |
			(0x11 << 0));

		if (pic_struct == 5)
			value = 0xa0;
		else if (pic_struct == 2)
			value = 0x80;

		Wr(DCNTR_GRID_LUMA0_RPT_PAT,      value);/*0x80:2 line read 1; 0xa0:4 line read 1*/
		Wr(DCNTR_GRID_CHROMA0_RPT_PAT,    value);
	} else if (pic_struct == 4) {
		Wr(DCNTR_GRID_RPT_LOOP,  (0 << 24)               |
			(0 << 16)               |
			(0x0 << 8)             |
			(0x11 << 0));
		Wr(DCNTR_GRID_LUMA0_RPT_PAT,      0x80);
		Wr(DCNTR_GRID_CHROMA0_RPT_PAT,    0x0);
	}

	Wr(DCNTR_GRID_DUMMY_PIXEL,   0x00808000);

	if (mode == 2)
		Wr(DCNTR_GRID_GEN_REG2, 1);/*0:NOT NV12 or NV21;1:NV12 (CbCr);2:NV21 (CrCb)*/
	else
		Wr(DCNTR_GRID_GEN_REG2, 0);

	Wr(DCNTR_GRID_GEN_REG3, 5);

	Wr(DCNTR_GRID_GEN_REG, Rd(DCNTR_GRID_GEN_REG) | (1 << 0));
}

void set_dcntr_grid_fmt(u32 hfmt_en,
	u32 hz_yc_ratio,
	u32 hz_ini_phase,
	u32 vfmt_en,
	u32 vt_yc_ratio,
	u32 vt_ini_phase,
	u32 y_length)
{
	u32 vt_phase_step = (16 >> vt_yc_ratio);
	u32 vfmt_w = (y_length >> hz_yc_ratio);

	Wr(DCNTR_GRID_FMT_CTRL,
		(0 << 28)       |
		(hz_ini_phase << 24) |
		(0 << 23)         |
		(hz_yc_ratio << 21)  |
		(hfmt_en << 20)   |
		(1 << 17)         |
		(0 << 16)         |
		(0 << 12)         |
		(vt_ini_phase << 8)  |
		(vt_phase_step << 1) |
		(vfmt_en << 0));

	Wr(DCNTR_GRID_FMT_W, (y_length << 16) | (vfmt_w << 0));
}

void dcntr_grid_rdmif(int canvas_id0,
	int canvas_id1,
	int canvas_id2,
	int canvas_baddr0,
	int canvas_baddr1,
	int canvas_baddr2,
	int src_hsize,
	int src_vsize,
	int src_fmt, /*1=RGB/YCBCR, 0=422 (2 bytes/pixel) 2:420,two canvas(nv21) 3:420,three*/
	int mif_x_start,
	int mif_x_end,
	int mif_y_start,
	int mif_y_end,
	int mif_reverse,
	int pic_struct,
	int h_avg)
{
	int hfmt_en;
	int hz_yc_ratio;
	int vfmt_en;
	int vt_yc_ratio;
	int fmt_hsize;
	int canvas_w;
	int stride_mif_y;
	int stride_mif_c;

	if (src_fmt == 0)
		canvas_w = 2;
	else if (src_fmt == 1)
		canvas_w = 3;
	else if (src_fmt == 2)
		canvas_w = 1;
	else
		canvas_w = 1;

	if (get_cpu_type() > MESON_CPU_MAJOR_ID_T5) {
		Wr(DCNTR_GRID_BADDR_Y, canvas_baddr0 >> 4);
		Wr(DCNTR_GRID_BADDR_CB, canvas_baddr1 >> 4);
		Wr(DCNTR_GRID_BADDR_CR, canvas_baddr2 >> 4);

		if (src_fmt == 0)
			stride_mif_y = (src_hsize * 8 * 2 + 127) >> 7; /*422 one plane*/
		else if (src_fmt == 1)
			stride_mif_y = (src_hsize * 8 * 3 + 127) >> 7; /*444 one plane*/
		else if (src_fmt == 2)
			stride_mif_y = (src_hsize * 8 + 127) >> 7; /*420 two planes, nv21/nv12*/
		else
			stride_mif_y = (src_hsize * 8 + 127) >> 7; /*444 three planes*/

		if (src_fmt == 0)
			stride_mif_c = stride_mif_y; /*422 one plane*/
		else if (src_fmt == 1)
			stride_mif_c = stride_mif_y; /*444 one plane*/
		else if (src_fmt == 2)
			stride_mif_c = stride_mif_y; /*420 two planes*/
		else
			stride_mif_c = stride_mif_y; /*444 three planes*/

		Wr(DCNTR_GRID_STRIDE_0, (stride_mif_c << 16) | stride_mif_y);
		Wr(DCNTR_GRID_STRIDE_1, (1 << 16) | stride_mif_c);
	}

	if (src_fmt == 0) {
		hfmt_en = h_avg == 1 ? 0 : 1;
		hz_yc_ratio = 1;
		vfmt_en = 0;
		vt_yc_ratio = 0;
	} else if (src_fmt > 1) {
		hfmt_en = h_avg == 1 ? 0 : 1;
		hz_yc_ratio = 1;
		vfmt_en = pic_struct == 4 ? 0 : 1;
		vt_yc_ratio = 1;
	} else {
		hfmt_en = 0;
		hz_yc_ratio = 0;
		vfmt_en = 0;
		vt_yc_ratio = 0;
	}

	fmt_hsize = mif_x_end - mif_x_start + 1;
	if (pic_struct == 4)
		fmt_hsize = fmt_hsize >> h_avg;

	set_dcntr_grid_mif(mif_x_start,
		mif_x_end,
		mif_y_start,
		mif_y_end,
		src_fmt,
		canvas_id0,
		canvas_id1,
		canvas_id2,
		pic_struct,
		h_avg);

	set_dcntr_grid_fmt(hfmt_en,
		hz_yc_ratio,
		0,
		vfmt_en,
		vt_yc_ratio,
		0,
		fmt_hsize);
}

void dcntr_grid_wrmif(int mif_index,
	int mem_mode,
	int src_fmt,
	int canvas_id,
	int mif_x_start,
	int mif_x_end,
	int mif_y_start,
	int mif_y_end,
	int swap_64bit,
	int mif_reverse,
	int linear_baddr,
	int linear_length)
{
	u32 INT_WMIF_CTRL1;
	u32 INT_WMIF_CTRL2;
	u32 INT_WMIF_CTRL3;
	u32 INT_WMIF_CTRL4;
	u32 INT_WMIF_SCOPE_X;
	u32 INT_WMIF_SCOPE_Y;

	if (mif_index == 0) {
		INT_WMIF_CTRL1 = DCNTR_CDS_WMIF_CTRL1;
		INT_WMIF_CTRL2 = DCNTR_CDS_WMIF_CTRL2;
		INT_WMIF_CTRL3 = DCNTR_CDS_WMIF_CTRL3;
		INT_WMIF_CTRL4  = DCNTR_CDS_WMIF_CTRL4;
		INT_WMIF_SCOPE_X = DCNTR_CDS_WMIF_SCOPE_X;
		INT_WMIF_SCOPE_Y = DCNTR_CDS_WMIF_SCOPE_Y;
	} else if (mif_index == 1) {
		INT_WMIF_CTRL1 = DCNTR_GRD_WMIF_CTRL1;
		INT_WMIF_CTRL2 = DCNTR_GRD_WMIF_CTRL2;
		INT_WMIF_CTRL3 = DCNTR_GRD_WMIF_CTRL3;
		INT_WMIF_CTRL4 = DCNTR_GRD_WMIF_CTRL4;
		INT_WMIF_SCOPE_X = DCNTR_GRD_WMIF_SCOPE_X;
		INT_WMIF_SCOPE_Y = DCNTR_GRD_WMIF_SCOPE_Y;
	} else {
		INT_WMIF_CTRL1 = DCNTR_YDS_WMIF_CTRL1;
		INT_WMIF_CTRL2 = DCNTR_YDS_WMIF_CTRL2;
		INT_WMIF_CTRL3 = DCNTR_YDS_WMIF_CTRL3;
		INT_WMIF_CTRL4 = DCNTR_YDS_WMIF_CTRL4;
		INT_WMIF_SCOPE_X = DCNTR_YDS_WMIF_SCOPE_X;
		INT_WMIF_SCOPE_Y = DCNTR_YDS_WMIF_SCOPE_Y;
	}

	Wr(INT_WMIF_CTRL1,
		(0 << 24) |
		(canvas_id << 16) |
		(1 << 12) |
		(1 << 10) |
		(2 << 8) |
		(swap_64bit << 7) |
		(mif_reverse << 6) |
		(0 << 5) |
		(0 << 4) |
		(src_fmt << 0));

	Wr(INT_WMIF_CTRL3,
		((mem_mode == 0) << 16) |
		(linear_length << 0));
	if (get_cpu_type() > MESON_CPU_MAJOR_ID_T5)
		Wr(INT_WMIF_CTRL4, linear_baddr >> 4);
	else
		Wr(INT_WMIF_CTRL4, linear_baddr);
	Wr(INT_WMIF_SCOPE_X, (mif_x_end << 16) | mif_x_start);
	Wr(INT_WMIF_SCOPE_Y, (mif_y_end << 16) | mif_y_start);
}

