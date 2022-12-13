// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/amlogic/iomap.h>
//#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "deinterlace.h"

#include "di_data_l.h"
#include "reg_decontour.h"
#include "reg_decontour_t3.h"
#include "register.h"
#include "di_prc.h"
#include "di_vframe.h"
#include "di_dbg.h"

#define DCT_PRE_LST_EN	DI_BIT31
#define DCT_PRE_LST_ISR	DI_BIT30
#define DCT_PRE_LST_CVS	DI_BIT29
#define DCT_PRE_LST_LINEAR	DI_BIT28

#define DCT_PRE_LS_ACT	DI_BIT31
#define DCT_PRE_LS_CH	DI_BIT30
#define DCT_PRE_LS_MEM	DI_BIT29

static DEFINE_SPINLOCK(dct_pre);

static void ini_dcntr_pre(int hsize, int vsize, int grd_num_mode, u32 ratio)
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
	const struct reg_acc *op = &di_pre_regset;

	op->wr(DCTR_BGRID_TOP_FSIZE, (hsize << 13) | (vsize));

	op->wr(DCNTR_GRID_GEN_REG, 1 << 29);

	op->wr(DCTR_BGRID_TOP_CTRL0, (1 << 30) |
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

	grd_num = op->rd(DCTR_BGRID_PARAM1_PRE);

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

	op->wr(DCTR_BGRID_PATH_PRE, (grd_path << 4));

	op->wr(DCTR_DS_PRE,
		(0 << 11) |
		(0 << 9) |
		(8 << 5) |
		(0 << 4) |
		(reg_in_ds_rate_x << 2) |
		reg_in_ds_rate_y);

	op->wr(DCTR_BGRID_PARAM2_PRE, (reg_grd_xsize << 24) |
		(reg_grd_ysize << 16) |
		(48 << 8) |
		(22));

	op->wr(DCTR_BGRID_PARAM3_PRE, (reg_grd_xnum_use << 16) |
		(reg_grd_ynum_use));

	op->wr(DCTR_BGRID_PARAM4_PRE, (reg_grd_xsize_ds << 16) |
		(reg_grd_ysize_ds));

	op->wr(DCTR_BGRID_WRAP_CTRL, 1);

	op->wr(DCTR_BGRID_PARAM5_PRE_0,
		(reg_grd_vbin_gb_0 << 16) | reg_grd_vbin_gb_1);
	op->wr(DCTR_BGRID_PARAM5_PRE_1,
		(reg_grd_vbin_gb_2 << 16) | reg_grd_vbin_gb_3);
	op->wr(DCTR_BGRID_PARAM5_PRE_2,
		(reg_grd_vbin_gb_4 << 16) | reg_grd_vbin_gb_5);
	op->wr(DCTR_BGRID_PARAM5_PRE_3,
		(reg_grd_vbin_gb_6 << 16) | reg_grd_vbin_gb_7);
	op->wr(DCTR_BGRID_PARAM5_PRE_4,
		(reg_grd_vbin_gb_8 << 16) | reg_grd_vbin_gb_9);
	op->wr(DCTR_BGRID_PARAM5_PRE_5,
		(reg_grd_vbin_gb_10 << 16) | reg_grd_vbin_gb_11);
	op->wr(DCTR_BGRID_PARAM5_PRE_6,
		(reg_grd_vbin_gb_12 << 16) | reg_grd_vbin_gb_13);
	op->wr(DCTR_BGRID_PARAM5_PRE_7,
		(reg_grd_vbin_gb_14 << 16) | reg_grd_vbin_gb_15);
	op->wr(DCTR_BGRID_PARAM5_PRE_8,
		(reg_grd_vbin_gb_16 << 16) | reg_grd_vbin_gb_17);
	op->wr(DCTR_BGRID_PARAM5_PRE_9,
		(reg_grd_vbin_gb_18 << 16) | reg_grd_vbin_gb_19);
	op->wr(DCTR_BGRID_PARAM5_PRE_10,
		(reg_grd_vbin_gb_20 << 16) | reg_grd_vbin_gb_21);
}

static void set_dcntr_grid_mif(u32 x_start,
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
	const struct reg_acc *op = &di_pre_regset;

	op->wr(DCNTR_GRID_GEN_REG,
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

	op->wr(DCNTR_GRID_CANVAS0,
		(canvas_addr2 << 16) |
		(canvas_addr1 << 8) |
		(canvas_addr0 << 0)
	);

	op->wr(DCNTR_GRID_LUMA_X0, (x_end << 16)  |
		(x_start << 0)
	);
	op->wr(DCNTR_GRID_LUMA_Y0, (y_end << 16)    |
		(y_start << 0)
	);

	if (mode > 1) {
		op->wr(DCNTR_GRID_CHROMA_X0, ((((x_end + 1) >> 1) - 1) << 16) |
			((x_start + 1) >> 1));
		op->wr(DCNTR_GRID_CHROMA_Y0, ((((y_end + 1) >> 1) - 1) << 16) |
			((y_start + 1) >> 1));
	}

	if (pic_struct == 0) {
		op->wr(DCNTR_GRID_RPT_LOOP, (0 << 24) |
			(0 << 16)             |
			(0 << 8)              |
			(0 << 0));
		op->wr(DCNTR_GRID_LUMA0_RPT_PAT,      0x0);
		op->wr(DCNTR_GRID_CHROMA0_RPT_PAT,    0x0);
	} else if ((pic_struct == 3) || (pic_struct == 6)) {
		op->wr(DCNTR_GRID_RPT_LOOP, (0 << 24)             |
			(0 << 16)             |
			(0 << 8)              |
			(0 << 0));

		if (pic_struct == 6)
			value = 0xa;
		else if (pic_struct == 3)
			value = 0x8;

		op->wr(DCNTR_GRID_LUMA0_RPT_PAT,      value);
		/*0x8:2 line read 1; 0xa:4 line read 1*/
		op->wr(DCNTR_GRID_CHROMA0_RPT_PAT,    value);
	} else if ((pic_struct == 2) || (pic_struct == 5)) {
		op->wr(DCNTR_GRID_RPT_LOOP,  (0 << 24)               |
			(0 << 16)               |
			(0x11 << 8)             |
			(0x11 << 0));

		if (pic_struct == 5)
			value = 0xa0;
		else if (pic_struct == 2)
			value = 0x80;

		op->wr(DCNTR_GRID_LUMA0_RPT_PAT,      value);
		/*0x80:2 line read 1; 0xa0:4 line read 1*/
		op->wr(DCNTR_GRID_CHROMA0_RPT_PAT,    value);
	} else if (pic_struct == 4) {
		op->wr(DCNTR_GRID_RPT_LOOP,  (0 << 24)               |
			(0 << 16)               |
			(0x0 << 8)             |
			(0x11 << 0));
		op->wr(DCNTR_GRID_LUMA0_RPT_PAT,      0x80);
		op->wr(DCNTR_GRID_CHROMA0_RPT_PAT,    0x0);
	}

	op->wr(DCNTR_GRID_DUMMY_PIXEL,   0x00808000);

	if (mode == 2)
		op->wr(DCNTR_GRID_GEN_REG2, 1);
	/*0:NOT NV12 or NV21;1:NV12 (CbCr);2:NV21 (CrCb)*/
	else
		op->wr(DCNTR_GRID_GEN_REG2, 0);

	op->wr(DCNTR_GRID_GEN_REG3, 5);

	op->wr(DCNTR_GRID_GEN_REG, op->rd(DCNTR_GRID_GEN_REG) | (1 << 0));
}

static void set_dcntr_grid_fmt(u32 hfmt_en,
	u32 hz_yc_ratio,
	u32 hz_ini_phase,
	u32 vfmt_en,
	u32 vt_yc_ratio,
	u32 vt_ini_phase,
	u32 y_length)
{
	u32 vt_phase_step = (16 >> vt_yc_ratio);
	u32 vfmt_w = (y_length >> hz_yc_ratio);
	const struct reg_acc *op = &di_pre_regset;

	op->wr(DCNTR_GRID_FMT_CTRL,
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

	op->wr(DCNTR_GRID_FMT_W, (y_length << 16) | (vfmt_w << 0));
}

static void dcntr_grid_rdmif(int canvas_id0,
	int canvas_id1,
	int canvas_id2,
	unsigned long canvas_baddr0,
	unsigned long canvas_baddr1,
	unsigned long canvas_baddr2,
	int src_hsize,
	int src_vsize,
	int src_fmt, /*1=RGB/YCBCR, 0=422 (2 bytes/pixel) */
	/*2:420,two canvas(nv21) 3:420,three*/
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
	const struct reg_acc *op = &di_pre_regset;

	if (src_fmt == 0)
		canvas_w = 2;
	else if (src_fmt == 1)
		canvas_w = 3;
	else if (src_fmt == 2)
		canvas_w = 1;
	else
		canvas_w = 1;

	if (DIM_IS_IC_EF(T3)) {
		op->wr(DCNTR_GRID_BADDR_Y, canvas_baddr0 >> 4);
		op->wr(DCNTR_GRID_BADDR_CB, canvas_baddr1 >> 4);
		op->wr(DCNTR_GRID_BADDR_CR, canvas_baddr2 >> 4);

		if (src_fmt == 0)
			stride_mif_y = (src_hsize * 8 * 2 + 127) >> 7;
		/*422 one plane*/
		else if (src_fmt == 1)
			stride_mif_y = (src_hsize * 8 * 3 + 127) >> 7;
		/*444 one plane*/
		else if (src_fmt == 2)
			stride_mif_y = (src_hsize * 8 + 127) >> 7;
		/*420 two planes, nv21/nv12*/
		else
			stride_mif_y = (src_hsize * 8 + 127) >> 7;
		/*444 three planes*/

		if (src_fmt == 0)
			stride_mif_c = stride_mif_y; /*422 one plane*/
		else if (src_fmt == 1)
			stride_mif_c = stride_mif_y; /*444 one plane*/
		else if (src_fmt == 2)
			stride_mif_c = stride_mif_y; /*420 two planes*/
		else
			stride_mif_c = stride_mif_y; /*444 three planes*/

		op->wr(DCNTR_GRID_STRIDE_0,
		       (stride_mif_c << 16) | stride_mif_y);
		op->wr(DCNTR_GRID_STRIDE_1, (1 << 16) | stride_mif_c);
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

static void dcntr_grid_wrmif(int mif_index,
	int mem_mode,
	int src_fmt,
	int canvas_id,
	int mif_x_start,
	int mif_x_end,
	int mif_y_start,
	int mif_y_end,
	int swap_64bit,
	int mif_reverse,
	unsigned long linear_baddr,
	int linear_length)
{
	u32 INT_WMIF_CTRL1;
	u32 INT_WMIF_CTRL2;
	u32 INT_WMIF_CTRL3;
	u32 INT_WMIF_CTRL4;
	u32 INT_WMIF_SCOPE_X;
	u32 INT_WMIF_SCOPE_Y;
	const struct reg_acc *op = &di_pre_regset;

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

	op->wr(INT_WMIF_CTRL1,
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

	op->wr(INT_WMIF_CTRL3,
		((mem_mode == 0) << 16) |
		(linear_length << 0));
	if (DIM_IS_IC_EF(T3))
		op->wr(INT_WMIF_CTRL4, linear_baddr >> 4);
	else
		op->wr(INT_WMIF_CTRL4, linear_baddr);
	op->wr(INT_WMIF_SCOPE_X, (mif_x_end << 16) | mif_x_start);
	op->wr(INT_WMIF_SCOPE_Y, (mif_y_end << 16) | mif_y_start);
}

/**************************************************************/
#define DCNTR_PRE_POOL_SIZE 3

#define DCT_PRE_PRINT_INFO       0X1
#define DCT_PRE_BYPASS           0X2
#define DCT_PRE_USE_DS_SCALE     0X4
#define DCT_PRE_DUMP_REG         0X8
#define DCT_PRE_PROCESS_I        0X10
#define DCT_PRE_REWRITE_REG      0X20

struct di_pre_dct_s {
	unsigned long decontour_addr;
	int dump_grid;
	int i_do_decontour;
	struct dcntr_mem_s dcntr_mem_info[DCNTR_PRE_POOL_SIZE];
	unsigned int do_cnt;
};

//static
bool dct_pre_try_alloc(struct di_ch_s *pch)
{
	struct di_hdct_s  *dct;

	dct = &get_datal()->hw_dct;

	if (dct->busy)
		return false;

	dct->busy = true;
	dct->owner = pch->ch_id;
	PR_INF("dct:pre:ch[%d]:alloc:ok\n", pch->ch_id);
	return true;
}

//static
void dct_pre_release(struct di_ch_s *pch)
{
	struct di_hdct_s  *dct;

	dct = &get_datal()->hw_dct;

	if (dct->busy) {
		if (pch->ch_id == dct->owner) {
			dct->owner = 0xff;
			dct->busy = false;
		} else {
			PR_ERR("%s:%d->%d\n",
			       __func__, pch->ch_id, dct->owner);
		}
	}

	if (pch->en_hf_buf) {
		get_datal()->hf_src_cnt--;
		pch->en_hf_buf = false;
	}
}

static struct di_pre_dct_s *dim_pdct(struct di_ch_s *pch)
{
	return (struct di_pre_dct_s *)pch->dct_pre;
}

static struct dcntr_mem_s *mem_peek_free(struct di_ch_s *pch)
{
	struct di_pre_dct_s *chdct;
	int i;

	if (!pch || !pch->dct_pre)
		return NULL;

	chdct = (struct di_pre_dct_s *)pch->dct_pre;
	for (i = 0; i < DCNTR_PRE_POOL_SIZE; i++) {
		if (chdct->dcntr_mem_info[i].free)
			return &chdct->dcntr_mem_info[i];
	}

	return NULL;
}

//static
unsigned int mem_cnt_free(struct di_ch_s *pch)
{
	struct di_pre_dct_s *chdct;
	int i;
	unsigned int cnt = 0;

	if (!pch || !pch->dct_pre)
		return 0;

	chdct = (struct di_pre_dct_s *)pch->dct_pre;
	for (i = 0; i < DCNTR_PRE_POOL_SIZE; i++) {
		if (chdct->dcntr_mem_info[i].free)
			cnt++;
	}

	return cnt;
}

static struct dcntr_mem_s *mem_get_free(struct di_ch_s *pch)
{
	struct di_pre_dct_s *chdct;
	int i;

	if (!pch || !pch->dct_pre)
		return NULL;

	chdct = (struct di_pre_dct_s *)pch->dct_pre;
	for (i = 0; i < DCNTR_PRE_POOL_SIZE; i++) {
		if (chdct->dcntr_mem_info[i].free) {
			chdct->dcntr_mem_info[i].free = false;
			return &chdct->dcntr_mem_info[i];
		}
	}

	return NULL;
}

static void mem_put_free(struct dcntr_mem_s *dmem)
{
	if (!dmem) {
		PR_ERR("%s:no?\n", __func__);
		return;
	}
	if (dmem->free) {
		PR_ERR("%s:have free?%px\n", __func__,  dmem);
		return;
	}
	dmem->free = true;
}

static const struct dcntr_mem_s dim_dctp_default = {
	.grd_addr = 0,
	.yds_addr = 0,
	.cds_addr = 0,
	.grd_size = 0,
	.yds_size = 0,
	.cds_size = 0,
	.yds_canvas_mode = 0,
	.yds_canvas_mode = 0,
	.ds_ratio = 0,
	.free = false,
	.use_org = true,
	.pre_out_fmt = VIDTYPE_VIU_NV21,
	.grd_swap_64bit = false,
	.yds_swap_64bit = false,
	.cds_swap_64bit = false,
	.grd_little_endian = true,
	.yds_little_endian = true,
	.cds_little_endian = true,
};

static bool is_decontour_supported(struct di_ch_s *pch)
{
	bool ret = false;
	struct di_pre_dct_s *pdct;

	if (!pch)
		return false;

	pdct = dim_pdct(pch);

	if (pdct)
		ret = true;

	return ret;
}

static void decontour_init(struct di_ch_s *pch)
{
	int mem_flags;
	int grd_size = SZ_512K;             /*(81*8*16 byte)*45 = 455K*/
	int yds_size = SZ_512K + SZ_32K;    /*960 * 576 = 540K*/
	int cds_size = yds_size / 2;        /*960 * 576 / 2= 270K*/
	int total_size = grd_size + yds_size + cds_size;
	int buffer_count = DCNTR_PRE_POOL_SIZE;
	int i;
	bool is_tvp = false;
	struct di_pre_dct_s *pdct = NULL;
	struct di_hdct_s  *dct;
	unsigned char cnt_ch, nub_ch;

	/* check if enable dct */
	nub_ch = cfgg(DCT);
	dct = &get_datal()->hw_dct;
	cnt_ch = dct->src_cnt;
	cnt_ch++;

	if (cnt_ch > nub_ch) {
		PR_INF("%s:ch[%d]:[%d][%d]\n",
		       "dct disable", pch->ch_id, cnt_ch, nub_ch);
		return;
	}
	PR_INF("%s:ch[%d]\n", "dct", pch->ch_id);
	dct->src_cnt = cnt_ch;
	/***********************/
	dct->statusx[pch->ch_id] |= DCT_PRE_LS_ACT;
	/**/
	if (!pch->dct_pre)
		pdct = vmalloc(sizeof(*pdct));

	if (!pdct) {
		PR_WARN("%s:vmalloc\n", __func__);
		return;
	}

	pch->dct_pre = pdct;

	dct->statusx[pch->ch_id] |= DCT_PRE_LS_CH;

	memset(pdct, 0, sizeof(*pdct));
	//pdct->i_do_decontour = true;

	mem_flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;
	if (pch->is_tvp == 2) {
		mem_flags |= CODEC_MM_FLAGS_TVP;
		is_tvp = true;
	}
	dbg_timer(pch->ch_id, EDBG_TIMER_DCT_B);
	pdct->decontour_addr = codec_mm_alloc_for_dma("di-decontour",
				buffer_count * total_size / PAGE_SIZE,
				0, mem_flags);
	dbg_timer(pch->ch_id, EDBG_TIMER_DCT_E);
	if (pdct->decontour_addr == 0) {
		PR_ERR("dctp: alloc fail\n");
		return;
	}
	dct->statusx[pch->ch_id] |= DCT_PRE_LS_MEM;

	dbg_dctp("dctp:alloc success %lx, is_tvp=%d\n",
		 pdct->decontour_addr, is_tvp);

	for (i = 0; i < buffer_count; i++) {
		memcpy(&pdct->dcntr_mem_info[i],
		       &dim_dctp_default,
		       sizeof(pdct->dcntr_mem_info[i]));
		pdct->dcntr_mem_info[i].index = i;
	}

	for (i = 0; i < buffer_count; i++) {
		pdct->dcntr_mem_info[i].grd_addr =
			pdct->decontour_addr + i * total_size;
		pdct->dcntr_mem_info[i].yds_addr =
			pdct->dcntr_mem_info[i].grd_addr + grd_size;
		pdct->dcntr_mem_info[i].cds_addr =
			pdct->dcntr_mem_info[i].yds_addr + yds_size;
		pdct->dcntr_mem_info[i].grd_size = grd_size;
		pdct->dcntr_mem_info[i].yds_size = yds_size;
		pdct->dcntr_mem_info[i].cds_size = cds_size;
		pdct->dcntr_mem_info[i].free = true;
	}

	if (dct->state == EDI_DCT_EXIT) {
		dct->last_w = 0;
		dct->last_h = 0;
		dct->last_type = 0;
		dct->state = EDI_DCT_IDLE;
		di_tout_int(&dct->tout, 20);
	}
	dbg_reg("dctp: init\n");
}

//static
void decontour_buf_reset(struct di_ch_s *pch)
{
	int i;
	struct di_pre_dct_s *pdct = dim_pdct(pch);

	if (!pdct ||
	    !pdct->decontour_addr)
		return;

	pr_info("decontour buf reset\n");
	for (i = 0; i < DCNTR_PRE_POOL_SIZE; i++)
		pdct->dcntr_mem_info[i].free = true;
}

static void decontour_uninit(struct di_ch_s *pch)
{
	struct di_pre_dct_s *pdct = dim_pdct(pch);
	struct di_hdct_s  *dct;

	if (!pdct)
		return;
	dct = &get_datal()->hw_dct;

	if (pdct->decontour_addr)
		codec_mm_free_for_dma("di-decontour",
			pdct->decontour_addr);
	dct->statusx[pch->ch_id] &= (~DCT_PRE_LS_MEM);
	vfree(pdct);
	pch->dct_pre = NULL;
	dct->statusx[pch->ch_id] &= (~DCT_PRE_LS_CH);
	dct->src_cnt--;
	dct->statusx[pch->ch_id] &= (~DCT_PRE_LS_ACT);
	dbg_dctp("decontour: uninit\n");
}

irqreturn_t dct_pre_isr(int irq, void *dev_id)
{
	struct di_hdct_s  *dct = &get_datal()->hw_dct;
	ulong flags = 0;
	unsigned int nub = 0;

	spin_lock_irqsave(&dct_pre, flags);
	if (!atomic_dec_and_test(&dct->irq_wait)) {
		PR_ERR("%s:%d\n", "irq_dct", atomic_read(&dct->irq_wait));
		spin_unlock_irqrestore(&dct_pre, flags);
		task_send_ready(25);
		return IRQ_HANDLED;
	}
	if (dct->curr_nins)
		nub = dct->curr_nins->c.cnt;
	dim_tr_ops.irq_dct(nub);
	spin_unlock_irqrestore(&dct_pre, flags);

	dbg_dctp("decontour: isr %d\n", atomic_read(&dct->irq_wait));
	task_send_ready(24);
	return IRQ_HANDLED;
}

static void decontour_dump_reg(void)
{
	u32 value;
	int i;
	const struct reg_acc *op = &di_pre_regset;

	for (i = 0x4a00; i < 0x4a12; i++) {
		value = op->rd(i);
		PR_INF("reg=%x, value= %x\n", i, value);
	}
	value = op->rd(DCNTR_GRD_WMIF_CTRL1);
	PR_INF("cds: DCNTR_GRD_WMIF_CTRL1: %x\n", value);
	value = op->rd(DCNTR_GRD_WMIF_CTRL3);
	PR_INF("cds: DCNTR_GRD_WMIF_CTRL3: %x\n", value);
	value = op->rd(DCNTR_GRD_WMIF_CTRL4);
	PR_INF("cds: DCNTR_GRD_WMIF_CTRL4: %x\n", value);
	value = op->rd(DCNTR_GRD_WMIF_SCOPE_X);
	PR_INF("cds: DCNTR_GRD_WMIF_SCOPE_X: %x\n", value);
	value = op->rd(DCNTR_GRD_WMIF_SCOPE_Y);
	PR_INF("cds: DCNTR_GRD_WMIF_SCOPE_Y: %x\n", value);

	value = op->rd(DCNTR_YDS_WMIF_CTRL1);
	PR_INF("grd: DCNTR_YDS_WMIF_CTRL1: %x\n", value);
	value = op->rd(DCNTR_YDS_WMIF_CTRL3);
	PR_INF("grd: DCNTR_YDS_WMIF_CTRL3: %x\n", value);
	value = op->rd(DCNTR_YDS_WMIF_CTRL4);
	PR_INF("grd: DCNTR_YDS_WMIF_CTRL4: %x\n", value);
	value = op->rd(DCNTR_YDS_WMIF_SCOPE_X);
	PR_INF("grd: DCNTR_YDS_WMIF_SCOPE_X: %x\n", value);
	value = op->rd(DCNTR_YDS_WMIF_SCOPE_Y);
	PR_INF("grd: DCNTR_YDS_WMIF_SCOPE_Y: %x\n", value);

	value = op->rd(DCNTR_CDS_WMIF_CTRL1);
	PR_INF("yds: DCNTR_CDS_WMIF_CTRL1: %x\n", value);
	value = op->rd(DCNTR_CDS_WMIF_CTRL3);
	PR_INF("yds: DCNTR_CDS_WMIF_CTRL3: %x\n", value);
	value = op->rd(DCNTR_CDS_WMIF_CTRL4);
	PR_INF("yds: DCNTR_CDS_WMIF_CTRL4: %x\n", value);
	value = op->rd(DCNTR_CDS_WMIF_SCOPE_X);
	PR_INF("yds: DCNTR_CDS_WMIF_SCOPE_X: %x\n", value);
	value = op->rd(DCNTR_CDS_WMIF_SCOPE_Y);
	PR_INF("yds: DCNTR_CDS_WMIF_SCOPE_Y: %x\n", value);

	value = op->rd(DCNTR_PRE_ARB_MODE);
	PR_INF("DCNTR_PRE_ARB_MODE: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_REQEN_SLV);
	PR_INF("DCNTR_PRE_ARB_REQEN_SLV: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_WEIGH0_SLV);
	PR_INF("DCNTR_PRE_ARB_WEIGH0_SLV: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_WEIGH1_SLV);
	PR_INF("DCNTR_PRE_ARB_WEIGH1_SLV: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_UGT);
	PR_INF("DCNTR_PRE_ARB_UGT: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_LIMT0);
	PR_INF("DCNTR_PRE_ARB_LIMT0: %x\n", value);

	value = op->rd(DCNTR_PRE_ARB_STATUS);
	PR_INF("DCNTR_PRE_ARB_STATUS: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_DBG_CTRL);
	PR_INF("DCNTR_PRE_ARB_DBG_CTRL: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_PROT);
	PR_INF("DCNTR_PRE_ARB_PROT: %x\n", value);

	value = op->rd(DCTR_BGRID_TOP_FSIZE);
	PR_INF("DCTR_BGRID_TOP_FSIZE: %x\n", value);
	value = op->rd(DCTR_BGRID_TOP_HDNUM);
	PR_INF("DCTR_BGRID_TOP_HDNUM: %x\n", value);
	value = op->rd(DCTR_BGRID_TOP_CTRL0);
	PR_INF("DCTR_BGRID_TOP_CTRL0: %x\n", value);
	value = op->rd(DCTR_BGRID_TOP_FMT);
	PR_INF("DCTR_BGRID_TOP_FMT: %x\n", value);
	value = op->rd(DCTR_BGRID_TOP_GCLK);
	PR_INF("DCTR_BGRID_TOP_GCLK: %x\n", value);
	value = op->rd(DCTR_BGRID_TOP_HOLD);
	PR_INF("DCTR_BGRID_TOP_HOLD: %x\n", value);

	value = op->rd(DCNTR_GRID_GEN_REG); //grd_num_use
	PR_INF("dct: DCNTR_GRID_GEN_REG=%x\n", value);
	value = op->rd(DCNTR_GRID_GEN_REG2); //grd_num_use
	PR_INF("dct: DCNTR_GRID_GEN_REG2=%x\n", value);
	value = op->rd(DCNTR_GRID_CANVAS0); //grd_num_use
	PR_INF("dct: DCNTR_GRID_CANVAS0=%x\n", value);
	value = op->rd(DCNTR_GRID_LUMA_X0); //grd_num_use
	PR_INF("dct: DCNTR_GRID_LUMA_X0=%x\n", value);
	value = op->rd(DCNTR_GRID_LUMA_Y0); //grd_num_use
	PR_INF("dct: DCNTR_GRID_LUMA_Y0=%x\n", value);
	value = op->rd(DCNTR_GRID_RPT_LOOP); //grd_num_use
	PR_INF("dct: DCNTR_GRID_RPT_LOOP=%x\n", value);
	value = op->rd(DCNTR_GRID_LUMA0_RPT_PAT); //grd_num_use
	PR_INF("dct: DCNTR_GRID_LUMA0_RPT_PAT=%x\n", value);
	value = op->rd(DCNTR_GRID_CHROMA0_RPT_PAT); //grd_num_use
	PR_INF("dct: DCNTR_GRID_CHROMA0_RPT_PAT=%x\n", value);
	value = op->rd(DCNTR_GRID_DUMMY_PIXEL); //grd_num_use
	PR_INF("dct: DCNTR_GRID_DUMMY_PIXEL=%x\n", value);

	value = op->rd(DCNTR_GRID_LUMA_FIFO_SIZE); //grd_num_use
	PR_INF("dct: DCNTR_GRID_LUMA_FIFO_SIZE=%x\n", value);

	value = op->rd(DCNTR_GRID_RANGE_MAP_Y); //grd_num_use
	PR_INF("dct: DCNTR_GRID_RANGE_MAP_Y=%x\n", value);
	value = op->rd(DCNTR_GRID_RANGE_MAP_CB); //grd_num_use
	PR_INF("dct: DCNTR_GRID_RANGE_MAP_CB=%x\n", value);
	value = op->rd(DCNTR_GRID_RANGE_MAP_CR); //grd_num_use
	PR_INF("dct: DCNTR_GRID_RANGE_MAP_CR=%x\n", value);
	value = op->rd(DCNTR_GRID_URGENT_CTRL); //grd_num_use
	PR_INF("dct: DCNTR_GRID_URGENT_CTRL=%x\n", value);

	value = op->rd(DCNTR_GRID_GEN_REG3); //grd_num_use
	PR_INF("dct: DCNTR_GRID_GEN_REG3=%x\n", value);
	value = op->rd(DCNTR_GRID_AXI_CMD_CNT); //grd_num_use
	PR_INF("dct: DCNTR_GRID_AXI_CMD_CNT=%x\n", value);
	value = op->rd(DCNTR_GRID_AXI_RDAT_CNT); //grd_num_use
	PR_INF("dct: DCNTR_GRID_AXI_RDAT_CNT=%x\n", value);
	value = op->rd(DCNTR_GRID_FMT_CTRL); //grd_num_use
	PR_INF("dct: DCNTR_GRID_FMT_CTRL=%x\n", value);
	value = op->rd(DCNTR_GRID_FMT_W); //grd_num_use
	PR_INF("dct: DCNTR_GRID_FMT_W=%x\n", value);

	if (DIM_IS_IC(T5))
		return;

	value = op->rd(DCNTR_GRID_BADDR_Y);
	PR_INF("dct: DCNTR_GRID_BADDR_Y=%x\n", value);
	value = op->rd(DCNTR_GRID_BADDR_CB);
	PR_INF("dct: DCNTR_GRID_BADDR_CB=%x\n", value);
	value = op->rd(DCNTR_GRID_BADDR_CR);
	PR_INF("dct: DCNTR_GRID_BADDR_CR=%x\n", value);

	value = op->rd(DCNTR_GRID_STRIDE_0);
	PR_INF("dct: DCNTR_GRID_STRIDE_0=%x\n", value);
	value = op->rd(DCNTR_GRID_STRIDE_1);
	PR_INF("dct: DCNTR_GRID_STRIDE_1=%x\n", value);

	value = op->rd(DCNTR_GRID_BADDR_Y_F1);
	PR_INF("dct: DCNTR_GRID_BADDR_Y_F1=%x\n", value);
	value = op->rd(DCNTR_GRID_BADDR_CB_F1);
	PR_INF("dct: DCNTR_GRID_BADDR_CB_F1=%x\n", value);
	value = op->rd(DCNTR_GRID_BADDR_CR_F1);
	PR_INF("dct: DCNTR_GRID_BADDR_CR_F1=%x\n", value);
	value = op->rd(DCNTR_GRID_STRIDE_0_F1);
	PR_INF("dct: DCNTR_GRID_STRIDE_0_F1=%x\n", value);
	value = op->rd(DCNTR_GRID_STRIDE_1_F1);
	PR_INF("dct: DCNTR_GRID_STRIDE_1_F1=%x\n", value);
}

//static
int decontour_dump_output(u32 w, u32 h,
			  struct dcntr_mem_s *dcntr_mem,
			  int skip)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	char name_buf[32];
	int write_size;
	u8 *data;

	if (w == 0 || h == 0)
		return -7;
	write_size = dcntr_mem->grd_size;
	PR_INF("addr =%lx, size=%d\n", dcntr_mem->grd_addr, write_size);
	snprintf(name_buf, sizeof(name_buf), "/data/tmp/%d-%d.grid",
		w, h);
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);

	if (IS_ERR(fp))
		return -1;
	PR_INF("decontour:dump_2: name_buf=%s\n", name_buf);
	data = codec_mm_vmap(dcntr_mem->grd_addr, write_size);
	PR_INF("(ulong)data =%lx\n", (ulong)data);
	if (!data)
		return -2;
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, data, write_size, &pos);
	vfs_fsync(fp, 0);
	PR_INF("decontour: write %u size to addr%p\n", write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);

	PR_INF("decontour:dump_2.0/2");
	write_size = w * h;
	PR_INF("decontour:dump_4:yds_addr =%lx, write_size=%d\n",
		dcntr_mem->yds_addr, write_size);
	PR_INF("decontour:dump_4:cds_addr =%lx\n", dcntr_mem->cds_addr);
	snprintf(name_buf, sizeof(name_buf), "/data/tmp/ds_out.yuv");

	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp)) {
		PR_ERR("decontour: create %s fail.\n", name_buf);
		return -4;
	}
	PR_INF("decontour:dump_4: name_buf=%s\n", name_buf);

	data = codec_mm_vmap(dcntr_mem->yds_addr, dcntr_mem->yds_size);
	PR_INF("decontour:dump_3\n");
	if (!data)
		return -5;
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, data, write_size, &pos);
	vfs_fsync(fp, 0);
	PR_INF("decontour: write %u size to file.addr%p\n", write_size, data);
	codec_mm_unmap_phyaddr(data);
	pos = write_size;

	write_size = w * h / 2;
	data = codec_mm_vmap(dcntr_mem->cds_addr, dcntr_mem->cds_size);
	PR_INF("decontour:dump_4\n");
	if (!data)
		return -6;
	vfs_write(fp, data, write_size, &pos);
	vfs_fsync(fp, 0);
	PR_INF("decontour: write %u size to file.addr%p\n", write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
	set_fs(fs);
	return 0;
}

enum DDIM_DCT_BYPASS_REASON {
	DDIM_DCT_BYPASS_BY_NONE,
	DDIM_DCT_BYPASS_BY_PRE = 1, /*di is in bypass mode*/
	DDIM_DCT_BYPASS_BY_CMD,
	DDIM_DCT_BYPASS_BY_NO_INT,
	DDIM_DCT_BYPASS_BY_NO_MEM,

	DDIM_DCT_BYPASS_BY_V_I,
	DDIM_DCT_BYPASS_BY_V_COMP,
	DDIM_DCT_BYPASS_BY_V_SIZE,
	DDIM_DCT_BYPASS_BY_V_EOS,
	DDIM_DCT_BYPASS_BY_PRE_BASE = 0x20,
};

static unsigned int dct_check_vfm_bypass(struct vframe_s *vf)
{
	unsigned int breason = 0;
	unsigned int x, y;
	struct di_hdct_s	*hdct = &get_datal()->hw_dct;

	if (!vf)
		return 0;
	if (vf->type & VIDTYPE_V4L_EOS)
		breason = DDIM_DCT_BYPASS_BY_V_EOS;
	else if (!hdct->i_do_decontour && VFMT_IS_I(vf->type))
		breason = DDIM_DCT_BYPASS_BY_V_I;
	else if (IS_COMP_MODE(vf->type) &&
		 (vf->type & VIDTYPE_NO_DW	||
		  vf->canvas0Addr == 0))
		breason = DDIM_DCT_BYPASS_BY_V_COMP;
	if (breason)
		return breason;

	dim_vf_x_y(vf, &x, &y);
	if ((x * y) < (160 * 120))
		breason = DDIM_DCT_BYPASS_BY_V_SIZE;

	return breason;
}

/* */
static bool dct_check_need_bypass(struct di_ch_s *pch, struct vframe_s *vframe)
{
	unsigned int breason = 0;
	struct di_pre_dct_s *chdct;

	if (!pch->dct_pre) {
		breason = DDIM_DCT_BYPASS_BY_NO_INT;
		goto BYPASS_DCT;
	}
	chdct = (struct di_pre_dct_s *)pch->dct_pre;

	if (!chdct->decontour_addr)
		breason = DDIM_DCT_BYPASS_BY_NO_MEM;
	else if (di_bypass_state_get(pch->ch_id))
		breason = DDIM_DCT_BYPASS_BY_PRE;
	else if (get_datal()->hw_dct.cmd_bypass)
		breason = DDIM_DCT_BYPASS_BY_CMD;

	if (breason)
		goto BYPASS_DCT;

	breason = dct_check_vfm_bypass(vframe);

	if (breason)
		goto BYPASS_DCT;

	return false;
BYPASS_DCT:
	get_datal()->hw_dct.sbypass_reason = breason;

	return true;
}

/* to-do */
static bool dct_check_no_rsc(struct di_ch_s *pch, struct vframe_s *vframe)
{
	if (!mem_peek_free(pch))
		return true;
	return false;
}

/* */
static void dct_do_bypass(struct di_ch_s *pch)
{
	struct dim_nins_s *nins;

	nins = nins_dct_get_bypass(pch);
	nins->c.vfm_cp.decontour_pre = NULL;
}

#define DCT_SFT_BYPSS_BIT	DI_BIT31
#define DCT_SFT_WAIT_BIT	DI_BIT30
#define DCT_SFT_DO_DCT		0

/************************************************
 * return:
 *	0: prepare ok, do dct -> dct_hw_set //get
 *	1: need bypass:		//get
 *	other: need wait;	//not get
 *
 ************************************************/
static unsigned int dct_sft_prepare(struct di_ch_s *pch,
				    struct dim_nins_s **pnin_out)
{
	u32 vf_org_width;
	u32 vf_org_height;
	struct dcntr_mem_s *dcntr_mem;
	bool unsupported_resolution = false;
	u32 is_interlace = 0;
	struct di_hdct_s	*hdct = &get_datal()->hw_dct;
	struct dim_nins_s *nins = NULL;
	struct vframe_s *vf, *vf_get;

	*pnin_out = NULL; //
	/* get vfm_in*/
	nins = nins_peek(pch);
	if (!nins)
		return DCT_SFT_WAIT_BIT | 1;
	/* check mem */
	if (!mem_peek_free(pch))
		return DCT_SFT_WAIT_BIT | 5;
	vf = &nins->c.vfm_cp;

	/*??*/
	hdct->ds_ratio = 0;
	hdct->src_fmt = 2;
	hdct->skip = 0;
	hdct->need_ds = false;
	hdct->ds_out_width = 0; /*dct ds output*/
	hdct->ds_out_height = 0;
	if ((vf->type & VIDTYPE_VIU_422) && !(vf->type & 0x10000000)) {
		hdct->src_fmt = 0;
		hdct->need_ds = true;
	/*422 is one plane, post not support, need pre out nv21*/
	} else if ((vf->type & VIDTYPE_VIU_NV21) ||
		   (vf->type & 0x10000000)) {
		/*hdmi in dw is nv21 VIDTYPE_DW_NV21*/
		hdct->src_fmt = 2;
	} else {
		dbg_dctp("not support format vf->type =%x\n", vf->type);
		/*current one support 422 nv21*/
		return DCT_SFT_BYPSS_BIT | (DDIM_DCT_BYPASS_BY_PRE_BASE + 1);
	}

	if (vf->type & VIDTYPE_COMPRESS) {
		vf_org_width = vf->compWidth;
		vf_org_height = vf->compHeight;
	} else {
		vf_org_width = vf->width;
		vf_org_height = vf->height;
	}

	hdct->pic_struct = 0;
	hdct->h_avg = 0;
	hdct->mif_out_width = vf->width;
	hdct->mif_out_height = vf->height;
	hdct->mif_read_width = vf->width;
	hdct->mif_read_height = vf->height;

	if (vf->type & VIDTYPE_INTERLACE) {
		is_interlace = 1;
		if (hdct->src_fmt == 2) {/*decoder output, top in odd rows*/
			if ((vf->type & VIDTYPE_INTERLACE_BOTTOM) == 0x3)
				hdct->pic_struct = 6;/*4 line read 1*/
			else
				hdct->pic_struct = 5;/*4 line read 1*/
			hdct->h_avg = 1;
			hdct->skip = 1;
			hdct->mif_out_width = vf->width >> 1;
			hdct->mif_out_height = vf->height >> 2;
		} else if (hdct->src_fmt == 0) {
		/*hdmiin output, In the first half of the line*/
			hdct->mif_out_width = vf->width;
			hdct->mif_out_height = vf->height >> 1;
			hdct->mif_read_height = vf->height >> 1;
			hdct->need_ds = 1;
			if (hdct->mif_out_width > 960 ||
			    hdct->mif_out_height > 540)
				hdct->ds_ratio = 1;
		}
	} else {
		if (vf->width > 1920 || vf->height > 1080) {
			hdct->ds_ratio = 1;
			hdct->skip = 1;
			hdct->mif_out_width = vf->width >> 1;
			hdct->mif_out_height = vf->height >> 1;
		} else if (vf->width > 960 || vf->height > 540) {
			if ((hdct->debug_decontour & DCT_PRE_USE_DS_SCALE) ||
			    hdct->src_fmt == 0) {
				/*hdmi in always use ds*/
				hdct->ds_ratio = 1;
			} else {
				/*decoder use mif skip for save ddr*/
				hdct->ds_ratio = 0;
				hdct->skip = 1;
				dbg_dctp("1080p use mif skip\n");
				hdct->mif_out_width = vf->width >> 1;
				hdct->mif_out_height = vf->height >> 1;
			}
		}

		if (hdct->skip && hdct->src_fmt == 2) {
			hdct->pic_struct = 4;
			hdct->h_avg = 1;
		}
	}

	hdct->ds_out_width = hdct->mif_out_width >> hdct->ds_ratio;
	hdct->ds_out_height = hdct->mif_out_height >> hdct->ds_ratio;

	if (hdct->ds_out_width < 16 || hdct->ds_out_height < 120) {
		dbg_dctp("not supported: vf:%d * %d", vf->width, vf->height);
		return DCT_SFT_BYPSS_BIT | (DDIM_DCT_BYPASS_BY_PRE_BASE + 2);
	}

	dbg_dctp("src_fmt=%d, pic_struct=%d, h_avg =%d, bitdepth=%x\n",
		 hdct->src_fmt, hdct->pic_struct, hdct->h_avg, vf->bitdepth);

	dbg_dctp("vf:%d*%d;mif_rd:%d*%d;mif_out:%d*%d;ds:%d*%d,ds_out:%d*%d\n",
		 vf->width, vf->height,
		 hdct->mif_read_width, hdct->mif_read_height,
		 hdct->mif_out_width, hdct->mif_out_height,
		 hdct->ds_ratio, hdct->skip,
		 hdct->ds_out_width, hdct->ds_out_height);
	dbg_dctp("ds_ratio=%d, skip=%d\n",
		 hdct->ds_ratio, hdct->skip);

	dcntr_mem = mem_get_free(pch);
	if (!dcntr_mem) { /* pre check */
		PR_ERR("%s:no mem?\n", __func__);
		return DCT_SFT_WAIT_BIT | 3;
	}
	nins = nins_dct_get(pch);
	if (!nins) { /* pre check */
		PR_ERR("%s:no input?\n", __func__);
		mem_put_free(dcntr_mem);
		return DCT_SFT_WAIT_BIT | 2;
	}

	vf_get = &nins->c.vfm_cp;
	if (vf_get->type != vf->type) {
		PR_ERR("vf_get need check 0x%x, 0x%x\n",
		       vf->type, vf_get->type);
		mem_put_free(dcntr_mem);
		return DCT_SFT_BYPSS_BIT | (DDIM_DCT_BYPASS_BY_PRE_BASE + 3);
	}

	*pnin_out = nins;
	hdct->curr_nins = nins;
	dcntr_mem->use_org = true;
	dcntr_mem->ds_ratio = 0;
	dcntr_mem->ori_w = vf->width;
	dcntr_mem->ori_h = vf->height;
	if (hdct->ds_ratio || hdct->skip || hdct->need_ds)
		dcntr_mem->use_org = false;

	if (!dcntr_mem->use_org) {
		dcntr_mem->ds_ratio = hdct->ds_ratio;
		if (hdct->skip)
			dcntr_mem->ds_ratio = dcntr_mem->ds_ratio + 1;

		if (hdct->need_ds && (vf->type & VIDTYPE_COMPRESS))
			dcntr_mem->ds_ratio = (vf->compWidth / vf->width) >> 1;
	}

	if (dcntr_mem->use_org) {
		if (vf->type & VIDTYPE_COMPRESS) {
			if ((vf->compWidth / vf->width) * vf->width !=
			    vf->compWidth)
				unsupported_resolution = true;
			if ((vf->compHeight / vf->height) * vf->height !=
			    vf->compHeight)
				unsupported_resolution = true;
		}
	} else {
		if ((hdct->ds_out_width << dcntr_mem->ds_ratio) != vf_org_width)
			unsupported_resolution = true;
		if ((hdct->ds_out_height << dcntr_mem->ds_ratio) !=
		    (vf_org_height >> is_interlace))
			unsupported_resolution = true;
	}
	if (unsupported_resolution) {
		//ary use free to release dcntr_mem->free = true;
		dbg_dctp("unsupported %d*%d, com %d*%d, ds_ratio=%d\n",
			vf->width, vf->height,
			vf->compWidth, vf->compHeight,
			dcntr_mem->ds_ratio);
		mem_put_free(dcntr_mem);
		return DCT_SFT_BYPSS_BIT | (DDIM_DCT_BYPASS_BY_PRE_BASE + 4);
	}
	dcntr_mem->cnt_in = nins->c.cnt;
	vf->decontour_pre = (void *)dcntr_mem;

	return DCT_SFT_DO_DCT;
}

static void dct_hw_set(struct dim_nins_s *pnin)
{
	int canvas_width;
	int canvas_height;
	unsigned long phy_addr_0, phy_addr_1, phy_addr_2;
	struct canvas_s src_cs0, src_cs1, src_cs2;
	int canvas_id_0, canvas_id_1, canvas_id_2;
	u32 grd_num;
	u32 reg_grd_xnum;
	u32 reg_grd_ynum;
	u32 grd_xsize;
	u32 grd_ysize;
	u32 grid_wrmif_length;
	u32 yflt_wrmif_length;
	u32 cflt_wrmif_length;
	int mif_reverse;
	int swap_64bit;
	u32 burst_len = 2;
	u32 bit_mode;
	bool format_changed = false;
	//u32 is_interlace = 0;
	u32 block_mode;
	u32 endian;
	u32 pic_32byte_aligned = 0;
	//bool support_canvas = false;
	int src_hsize;
	int src_vsize;
	struct dcntr_mem_s *dcntr_mem;
	struct di_hdct_s	*hdct = &get_datal()->hw_dct;
	const struct reg_acc *op = &di_pre_regset;
	struct vframe_s *vf;

	if (!pnin) {
		PR_ERR("%s:no pnin?\n", __func__);
		return;
	}
	vf = &pnin->c.vfm_cp;
	dcntr_mem = (struct dcntr_mem_s *)vf->decontour_pre;

	if (vf->canvas0Addr == (u32)-1) {
		phy_addr_0 = vf->canvas0_config[0].phy_addr;
		phy_addr_1 = vf->canvas0_config[1].phy_addr;
		phy_addr_2 = vf->canvas0_config[2].phy_addr;
		canvas_config_config(hdct->cvs_dct[0],
			&vf->canvas0_config[0]);
		canvas_config_config(hdct->cvs_dct[1],
			&vf->canvas0_config[1]);
		canvas_config_config(hdct->cvs_dct[2],
			&vf->canvas0_config[2]);
		canvas_id_0 = hdct->cvs_dct[0];
		canvas_id_1 = hdct->cvs_dct[1];
		canvas_id_2 = hdct->cvs_dct[2];
		canvas_width = vf->canvas0_config[0].width;
		canvas_height = vf->canvas0_config[0].height;
		block_mode = vf->canvas0_config[0].block_mode;
		endian = vf->canvas0_config[0].endian;
	} else {
		dbg_dctp("source is canvas\n");
		canvas_read(vf->canvas0Addr & 0xff, &src_cs0);
		canvas_read(vf->canvas0Addr >> 8 & 0xff, &src_cs1);
		canvas_read(vf->canvas0Addr >> 16 & 0xff, &src_cs2);
		phy_addr_0 = src_cs0.addr;
		phy_addr_1 = src_cs1.addr;
		phy_addr_2 = src_cs2.addr;
		canvas_id_0 = vf->canvas0Addr & 0xff;
		canvas_id_1 = vf->canvas0Addr >> 8 & 0xff;
		canvas_id_2 = vf->canvas0Addr >> 16 & 0xff;
		canvas_width = src_cs0.width;
		canvas_height = src_cs0.height;
		block_mode = src_cs0.blkmode;
		endian = src_cs0.endian;
	}
	dbg_dctp("block_mode=%d, endian=%d\n", block_mode, endian);

	if (hdct->last_w != vf->width ||
		hdct->last_h != vf->height ||
		hdct->last_type != vf->type ||
		(hdct->debug_decontour & DCT_PRE_REWRITE_REG))
		format_changed = true;

	if (!format_changed) {
		dcntr_mem->grd_swap_64bit = hdct->info_last.grd_swap_64bit;
		dcntr_mem->yds_swap_64bit = hdct->info_last.yds_swap_64bit;
		dcntr_mem->cds_swap_64bit = hdct->info_last.cds_swap_64bit;
		dcntr_mem->grd_little_endian =
			hdct->info_last.grd_little_endian;
		dcntr_mem->yds_little_endian =
			hdct->info_last.yds_little_endian;
		dcntr_mem->cds_little_endian =
			hdct->info_last.cds_little_endian;
		dcntr_mem->yflt_wrmif_length =
			hdct->info_last.yflt_wrmif_length;
		dcntr_mem->cflt_wrmif_length =
			hdct->info_last.cflt_wrmif_length;
		goto SET_ADDR;
	}
	hdct->last_w = vf->width;
	hdct->last_h = vf->height;
	hdct->last_type = vf->type;

	if (hdct->src_fmt == 0) {
		src_hsize = canvas_width >> 1;
		src_vsize = canvas_height;
	} else {
		src_hsize = canvas_width;
		src_vsize = canvas_height;
	}
	ini_dcntr_pre(hdct->mif_out_width,
		      hdct->mif_out_height, 2, hdct->ds_ratio);
	grd_num = op->rd(DCTR_BGRID_PARAM3_PRE);
	reg_grd_xnum = (grd_num >> 16) & (0x3ff);
	reg_grd_ynum = grd_num & (0x3ff);

	grd_xsize = reg_grd_xnum << 3;
	grd_ysize = reg_grd_ynum;

	grid_wrmif_length = 81 * 46 * 8;
	yflt_wrmif_length = hdct->ds_out_width;
	cflt_wrmif_length = hdct->ds_out_width;

	dbg_dctp("cvs_w=%d, cvs_h=%d, src_hsize=%d, src_vsize=%d\n",
		canvas_width, canvas_height,
		src_hsize, src_vsize);

	if (canvas_width % 32)
		burst_len = 0;
	else if (canvas_width % 64)
		burst_len = 1;

	if (block_mode && !hdct->support_canvas)
		burst_len = block_mode;
	dcntr_grid_rdmif(canvas_id_0,	      /*int canvas_id0,*/
		canvas_id_1,	     /*int canvas_id1,*/
		canvas_id_2,	     /*int canvas_id2,*/
		phy_addr_0,  /*int canvas_baddr0,*/
		phy_addr_1,	      /*int canvas_baddr1,*/
		phy_addr_2,	      /*int canvas_baddr2,*/
		src_hsize,   /*int src_hsize,*/
		src_vsize,   /*int src_vsize,*/
		hdct->src_fmt, /*1 = RGB/YCBCR(3 bytes/pixel), */
		/*0=422 (2 bytes/pixel) 2:420 (two canvas)*/
		0,	     /*int mif_x_start,*/
		hdct->mif_read_width - 1, /*int mif_x_end  ,*/
		0,	     /*int mif_y_start,*/
		hdct->mif_read_height - 1, /*int mif_y_end  ,*/
		0,	      /*int mif_reverse  // 0 : no reverse*/
		hdct->pic_struct,
		/*0 : frame; 2:top_field; 3:bot_field; 4:y skip, uv no skip;*/
				/*5:top_field 1/4; 6 bot_field 1/4*/
		hdct->h_avg);    /*0 : no avg 1:y_avg  2:c_avg  3:y&c avg*/

	mif_reverse = 1;
	swap_64bit = 0;
	if (mif_reverse == 1) {
		dcntr_mem->grd_swap_64bit = false;
		dcntr_mem->yds_swap_64bit = false;
		dcntr_mem->cds_swap_64bit = false;
		dcntr_mem->grd_little_endian = true;
		dcntr_mem->yds_little_endian = true;
		dcntr_mem->cds_little_endian = true;
	}
	dcntr_grid_wrmif(1,  /*int mif_index,  //0:cds	1:grd	2:yds*/
		0,  /*int mem_mode*/
		/*0:linear address mode  1:canvas mode*/
		5,  /*int src_fmt ,*/
		/*0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit*/
		hdct->cvs_dct[3],  /*int canvas_id*/
		0,  /*int mif_x_start*/
		grd_xsize - 1,	/*int mif_x_end*/
		0,  /*int mif_y_start*/
		grd_ysize - 1,	/*int mif_y_end*/
		swap_64bit,
		mif_reverse,  /*int mif_reverse, /0 : no reverse*/
		dcntr_mem->grd_addr,  /*int linear_baddr*/
		grd_xsize);  /*int linear_length*/
		/* DDR read length = linear_length*128 bits*/

	if (hdct->ds_ratio || hdct->skip || hdct->need_ds) {
		yflt_wrmif_length = yflt_wrmif_length * 8 / 128;
		cflt_wrmif_length = cflt_wrmif_length * 8 / 128;
		dcntr_mem->yflt_wrmif_length = yflt_wrmif_length;
		dcntr_mem->cflt_wrmif_length = cflt_wrmif_length;

		dcntr_grid_wrmif(2,  /*int mif_index, 0:cds; 1:grd; 2:yds*/
			0,  /*int mem_mode*/
			/*0:linear address mode  1:canvas mode*/
			1,  /*int src_fmt*/
			/*0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit*/
			hdct->cvs_dct[4],  /*int canvas_id*/
			0,  /*int mif_x_start*/
			hdct->ds_out_width - 1,  /*int mif_x_end*/
			0,  /*int mif_y_start*/
			hdct->ds_out_height - 1,  /*int mif_y_end*/
			swap_64bit,
			mif_reverse,  /*int mif_reverse*/ /*0 : no reverse*/
			dcntr_mem->yds_addr,  /*int linear_baddr*/
			yflt_wrmif_length);
		/*DDR read length = linear_length*128 bits*/

		dcntr_grid_wrmif(0,  /*int mif_index,*/
			/*0:cds	1:grd	2:yds*/
			0,  /*int mem_mode,*/
			/*0:linear address mode  1:canvas mode*/
			2,  /*int src_fmt,*/
			/* 0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit*/
			hdct->cvs_dct[5],
			/*int canvas_id*/
			0,  /*int mif_x_start*/
			(hdct->ds_out_width >> 1) - 1,
			/*int mif_x_end*/
			0,  /*int mif_y_start*/
			(hdct->ds_out_height >> 1) - 1,
			/*int mif_y_end*/
			swap_64bit,
			mif_reverse,
			/*int mif_reverse   ,  //0 : no reverse*/
			dcntr_mem->cds_addr,
			/*int linear_baddr*/
			cflt_wrmif_length);
			/*DDR read length = linear_length*128 bits*/
	}

	op->bwr(DCTR_BGRID_TOP_CTRL0, 1, 2, 2);
	/*reg_din_sel: 1:dos 2:vdin  0/3:disable*/

	if (hdct->ds_ratio || hdct->skip || hdct->need_ds) {
		op->bwr(DCTR_BGRID_TOP_CTRL0, 1, 1, 1);
		/*reg_ds_mif_en: 1=on 0=off*/
		op->bwr(DCTR_BGRID_TOP_FMT, 2, 19, 2);
		/*reg_fmt_mode, 2=420, 1=422, 0=444*/

		if (hdct->ds_ratio == 0)
			op->bwr(DCTR_BGRID_PATH_PRE, 1, 4, 1);
			/*reg_grd_path 0=ds 1=ori*/
		else
			op->bwr(DCTR_BGRID_PATH_PRE, 0, 4, 1);
			/*reg_grd_path 0=ds 1=ori*/

		if (hdct->ds_ratio == 1)
			op->bwr(DCTR_DS_PRE, 0x5, 0, 4);
			/*reg_ds_rate_xy*/
		else if (hdct->ds_ratio == 0)
			op->bwr(DCTR_DS_PRE, 0x0, 0, 4);
			/*reg_ds_rate_xy*/
	} else {
		op->bwr(DCTR_BGRID_TOP_CTRL0, 0, 1, 1);
		/*reg_ds_mif_en: 1=on 0=off*/
		op->bwr(DCTR_BGRID_PATH_PRE, 1, 4, 1);
		/*reg_grd_path 0=ds 1=ori*/
		op->bwr(DCTR_DS_PRE, 0x0, 0, 4);
		/*reg_ds_rate_xy*/
	}

	if (hdct->src_fmt == 0 && (vf->bitdepth & BITDEPTH_Y10) &&
		((vf->type & VIDTYPE_COMPRESS) == 0)) {/*all dw is 8bit*/
		if (vf->bitdepth & FULL_PACK_422_MODE)
			bit_mode = 3;
		else
			bit_mode = 1;
		op->bwr(DCNTR_GRID_GEN_REG3, bit_mode, 8, 2);
		/*0->8bit; 1->10bit422; 2->10bit444*/
		op->bwr(DCNTR_GRID_GEN_REG3, 0x3, 4, 3);
		/*cntl_blk_len: vd1 default is 3*/
	}
	op->bwr(DCNTR_GRID_GEN_REG3, (burst_len & 0x3), 1, 2);
	hdct->info_last = *dcntr_mem;

SET_ADDR:
	if (hdct->support_canvas) {
		op->wr(DCNTR_GRID_CANVAS0,
			(canvas_id_2 << 16) |
			(canvas_id_1 << 8) |
			(canvas_id_0 << 0));
		op->wr(DCNTR_GRD_WMIF_CTRL4,
			(unsigned int)dcntr_mem->grd_addr);
		op->wr(DCNTR_YDS_WMIF_CTRL4,
			(unsigned int)dcntr_mem->yds_addr);
		op->wr(DCNTR_CDS_WMIF_CTRL4,
			(unsigned int)dcntr_mem->cds_addr);
	} else {
		op->wr(DCNTR_GRID_BADDR_Y,
			(unsigned int)(phy_addr_0 >> 4));
		op->wr(DCNTR_GRID_BADDR_CB,
			(unsigned int)(phy_addr_1 >> 4));
		op->wr(DCNTR_GRID_BADDR_CR,
			(unsigned int)(phy_addr_2 >> 4));

		op->wr(DCNTR_GRD_WMIF_CTRL4,
			(unsigned int)(dcntr_mem->grd_addr >> 4));
		op->wr(DCNTR_YDS_WMIF_CTRL4,
			(unsigned int)(dcntr_mem->yds_addr >> 4));
		op->wr(DCNTR_CDS_WMIF_CTRL4,
			(unsigned int)(dcntr_mem->cds_addr >> 4));

		op->bwr(DCNTR_GRID_GEN_REG3, block_mode, 12, 2);
		op->bwr(DCNTR_GRID_GEN_REG3, block_mode, 14, 2);
		if (block_mode)
			pic_32byte_aligned = 7;
		op->bwr(DCNTR_GRID_GEN_REG3,
			(pic_32byte_aligned << 7) |
			(block_mode << 4) |
			(block_mode << 2) |
			(block_mode << 0),
			18, 9);
		if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR) {
			op->bwr(DCNTR_GRID_GEN_REG3, 0, 0, 1);
			dbg_dctp("LINEAR:flag=%x\n", vf->flag);
		} else {
			op->bwr(DCNTR_GRID_GEN_REG3, 1, 0, 1);
			dbg_dctp("block_mode:flag=%x\n", vf->flag);
		}
	}

	if (hdct->debug_decontour & DCT_PRE_DUMP_REG)
		decontour_dump_reg();
	if (hdct->debug_decontour & DCT_PRE_PRINT_INFO)
		dim_dbg_dct_info(vf->decontour_pre);

	atomic_set(&hdct->irq_wait, 1);
	dim_tr_ops.dct_set(hdct->curr_nins->c.cnt);
	dbg_dctp("decontour:start_1\n");
	op->bwr(DCTR_BGRID_TOP_CTRL0, 1, 31, 1);
	dbg_dctp("decontour:start_2\n");
}

/* wait or finish */
static bool dct_wait_int(void)
{
	struct di_hdct_s	*hdct = &get_datal()->hw_dct;
	bool state;
	struct di_ch_s *pch;

	pch = get_chdata(hdct->curr_ch);
	dbg_dctp("t_int:irq_wait[%d]\n", atomic_read(&hdct->irq_wait));
	if (atomic_read(&hdct->irq_wait) <= 0) {
		/* done */
		dbg_dctp("t_int:done\n");
		state = false; /* no need wait */
		di_tout_contr(EDI_TOUT_CONTR_FINISH, &hdct->tout);
		nins_dct_2_done(pch, hdct->curr_nins);
		hdct->curr_nins = NULL;
	} else if (di_tout_contr(EDI_TOUT_CONTR_CHECK, &hdct->tout)) {
		dbg_dctp("t_int:time\n");
		if (!atomic_dec_and_test(&hdct->irq_wait)) {
			PR_WARN("%s:ch[%d]dct timeout and irq?\n", __func__,
				hdct->curr_ch);
			di_tout_contr(EDI_TOUT_CONTR_EN, &hdct->tout);
			state = true; /*need wait */
		} else {
			state = false;/* no need wait */
			/* time out */
			PR_WARN("dct timeout:ch[%d]\n", hdct->curr_ch);
			mem_put_free(hdct->curr_nins->c.vfm_cp.decontour_pre);
			hdct->curr_nins->c.vfm_cp.decontour_pre = NULL;
			nins_dct_2_done(pch, hdct->curr_nins);
			hdct->curr_nins = NULL;
		}
	} else {
		dbg_dctp("t_int:wait\n");
		state = true; /*need wait */
	}

	return state;
}

/***************************************************************/

/*use do_table:*/
static unsigned int dct_t_check(void *data)
{
	unsigned int ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
	struct di_hdct_s  *dct = &get_datal()->hw_dct;
	struct di_ch_s *pch;
	unsigned int prepare;
	struct dim_nins_s *pnin_out = NULL;
	struct di_pre_dct_s *pdct;

	pch = get_chdata(dct->curr_ch);
	/**/
	prepare = dct_sft_prepare(pch, &pnin_out);
	if (prepare & DCT_SFT_BYPSS_BIT) {
		//dct_do_bypass(pch);
		if (!pnin_out)
			dct_do_bypass(pch);
		else
			nins_dct_2_done(pch, pnin_out);
		dbg_dctp("t_check:bypass:0x%x\n", prepare);
		dct->sbypass_reason = (prepare & (~DCT_SFT_BYPSS_BIT));
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
	} else if (prepare & DCT_SFT_WAIT_BIT) {
		dbg_dctp("t_check:wait:\n");
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
	} else {
		dbg_dctp("t_check:set:\n");
		dct->sbypass_reason = 0;
		pdct = dim_pdct(pch);
		pdct->do_cnt++;
		dct_hw_set(pnin_out);
		di_tout_contr(EDI_TOUT_CONTR_EN, &dct->tout);
		ret = K_DO_R_FINISH;
	}

	return ret;
}

static unsigned int dct_t_wait_int(void *data)
{
	bool wait;
	unsigned int ret = K_DO_R_NOT_FINISH;

	wait = dct_wait_int();
	if (wait)
		ret = K_DO_R_NOT_FINISH;
	else
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);

	return ret;
}

enum EDI_DCT_MT {
	EDI_DCT_MT_CHECK = K_DO_TABLE_ID_START,
	//EDI_DCT_MT_SET,
	EDI_DCT_MT_WAIT_INT,
};

static const struct do_table_ops_s dct_hw_process[] = {
	/*fix*/
	[K_DO_TABLE_ID_PAUSE] = {
	.id = K_DO_TABLE_ID_PAUSE,
	.mark = 0,
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "pause",
	},
	[K_DO_TABLE_ID_STOP] = {
	.id = K_DO_TABLE_ID_STOP,
	.mark = 0,
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "stop",
	},
	/******************/
	[K_DO_TABLE_ID_START] = {	/*EDI_PRE_MT_CHECK*/
	.id = K_DO_TABLE_ID_START,
	.mark = 0,
	.con = NULL,
	.do_op = dct_t_check,
	.do_stop_op = NULL,
	.name = "start-check_t",
	},
	[EDI_DCT_MT_WAIT_INT] = {
	.id = EDI_DCT_MT_WAIT_INT,
	.mark = 0,
	.con = NULL,
	.do_op = dct_t_wait_int,
	.do_stop_op = NULL,
	.name = "wait_int",
	}
};

/***************************************************************/
static bool dct_m_idle(void)
{
	struct di_hdct_s  *dct = &get_datal()->hw_dct;
	bool reflesh = false;
	unsigned int ch;
	enum EDI_SUB_ID /*lst_ch,*/ nch, lch;
	int i;

	if (dct->idle_cnt >= DI_CHANNEL_NUB) {
		dct->idle_cnt = 0;
		//dbg_dctp("%s:idle_cnt=%d\n", __func__, dct->idle_cnt);
		return false;
	}
	ch = dct->curr_ch;
	nch = 0xffff;
	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		lch = ch + i + 1;
		if (lch >= DI_CHANNEL_NUB)
			lch -= DI_CHANNEL_NUB;
		//dbg_dctp("%s:i=%d,lch=%d\n", __func__, i, lch);
		if (get_reg_flag(lch)		&&
		    !get_flag_trig_unreg(lch)	&&
		    !is_bypss2_complete(lch)	&&
		    is_decontour_supported(get_chdata(lch))) {
			nch = lch;
			break;
		}
	}
	//dbg_dctp("%s:nch=%d\n", __func__, nch);

	/* not find next channel */
	if (nch >= DI_CHANNEL_NUB)
		return false;

	dct->curr_ch = nch;

	/*state*/
	dct->state++;
	reflesh = true;
	//dbg_dctp("%s:ch[%d]\n", __func__, dct->curr_ch);

	return reflesh;
}

static bool dct_m_check(void)
{
	struct di_hdct_s  *dct = &get_datal()->hw_dct;
	bool reflesh = false;
	struct di_ch_s *pch;
	struct vframe_s *vf;
	//unsigned int prepare;

	/*get vframe and select mode
	 * now: fix use total table
	 */

	pch = get_chdata(dct->curr_ch);

	if (pre_run_flag == DI_RUN_FLAG_RUN	||
	    pre_run_flag == DI_RUN_FLAG_STEP) {
		if (pre_run_flag == DI_RUN_FLAG_STEP)
			pre_run_flag = DI_RUN_FLAG_STEP_DONE;
		vf = nins_peekvfm(pch);//pw_vf_peek(ch);
	} else {
		vf = NULL;
	}

	dct->idle_cnt++;

	if (!vf) {
		dct->state--;
		return true;
	}

	if (dct_check_need_bypass(pch, vf)) {
		dbg_dbg("m_check:bypass:0x%x\n",
			get_datal()->hw_dct.sbypass_reason);
		dct_do_bypass(pch);
		dct->state--;
		return true;
	}

	/* after check bypass */
	if (dct_check_no_rsc(pch, vf)) {
		dct->state--;
		return true;
	}

	dct->idle_cnt = 0;
	do_table_init(&dct->sdt_mode,
		      &dct_hw_process[0],
		      ARRAY_SIZE(dct_hw_process));
	dct->sdt_mode.name = "dct";

	do_table_cmd(&dct->sdt_mode, EDO_TABLE_CMD_START);

	/*state*/
	dct->state++;
	reflesh = true;

	return reflesh;
}

static bool dct_m_do_table(void)
{
	struct di_hdct_s  *dct = &get_datal()->hw_dct;
	bool reflesh = false;

	do_table_working(&dct->sdt_mode);

	if (do_table_is_crr(&dct->sdt_mode, K_DO_TABLE_ID_STOP)) {
		dct->state = EDI_DCT_IDLE;
		reflesh = true;
	}
	return reflesh;
}

static const struct di_func_tab_s di_dct_func[] = {
	{EDI_DCT_EXIT, NULL},
	{EDI_DCT_IDLE, dct_m_idle},
	{EDI_DCT_CHECK, dct_m_check},
	{EDI_DCT_DO_TABLE, dct_m_do_table},
};

static const char * const di_dct_state_name[] = {
	"EXIT",
	"IDLE",	/*switch to next channel?*/
	"CHECK",
	"DO_TABLE",
};

static bool dct_m_step(void)
{
	struct di_hdct_s  *dct = &get_datal()->hw_dct;
	enum EDI_DCT_STATE dct_st = dct->state;

	if (dct_st <= EDI_DCT_DO_TABLE	&&
	    di_dct_func[dct_st].func) {
		return di_dct_func[dct_st].func();
	}

	return false;
}

static void dct_main_process(void)
{
	bool reflesh;

	reflesh = true;

	while (reflesh) {
		reflesh = dct_m_step();
		#ifdef MARK_HIS	/*debug only*/
		dct = &get_datal()->hw_dct;
		dbg_tsk("ch[%d]:st[%s]r[%d]\n", dct->curr_ch,
			di_dct_state_name[dct->state], reflesh);
		#endif
	}
}

//there is no channel active
static void dct_unreg_all(void)
{
	struct di_hdct_s  *dct;

	dct = &get_datal()->hw_dct;

	dct->last_type = 0;
	dct->last_h = 0;
	dct->last_w = 0;

	dct->src_cnt = 0;
	dct->state = EDI_DCT_EXIT;
}

static const struct di_dct_ops_s dim_dct_ops = {
	.main_process = dct_main_process,
	.mem_put_free = mem_put_free,
	.reg		= decontour_init,
	.unreg		= decontour_uninit,
	.unreg_all	= dct_unreg_all,
	.is_en		= is_decontour_supported,
};

//get_datal()->dct_op->is_en(pch);
void dct_pre_prob(struct platform_device *pdev)
{
	struct di_hdct_s  *dct;
	int ret = 0;
#ifdef	PRINT_BASIC
	int i;
#endif

	if (IS_IC_SUPPORT(DECONTOUR)) {
		dct = &get_datal()->hw_dct;

		memset(dct, 0, sizeof(*dct));
		dct->statust |= DCT_PRE_LST_EN;

		/* irq */
		dct->irq = -ENXIO;

		dct->irq = platform_get_irq_byname(pdev, "dct_irq");
		if (dct->irq  == -ENXIO) {
			PR_ERR("cannot get dct_irq\n");
			return;
		}
		dct->statust |= DCT_PRE_LST_ISR;
		dbg_reg("dct_irq:%d\n", dct->irq);

		/* cvs */
		if (ops_ext()->cvs_alloc_table("di_dct",
				       &dct->cvs_dct[0],
				       6,
				       CANVAS_MAP_TYPE_1)) {
			/* error */
			PR_ERR("%s cvs\n", __func__);
			return;
		}
		dct->statust |= DCT_PRE_LST_CVS;
		/*dbg*/
#ifdef	PRINT_BASIC
		PR_INF("cvs:\n");
		for (i = 0; i < 6; i++)
			PR_INF("\t%d:0x%x\n", i, dct->cvs_dct[i]);
#endif
		get_datal()->dct_op = &dim_dct_ops;

		if (cfgg(LINEAR))
			get_datal()->hw_dct.support_canvas = false;
		else
			get_datal()->hw_dct.support_canvas = true;

		/* irq */
		ret = devm_request_irq(&pdev->dev, dct->irq,
				       &dct_pre_isr,
				       IRQF_SHARED,
				       "dct_pre",
				       (void *)"dct_pre");
		PR_INF("dct_pre _irq ok\n");
		get_datal()->hw_dct.i_do_decontour = true;
		#ifdef DBG_DCT_PRINT
		get_datal()->hw_dct.debug_decontour |= DCT_PRE_DUMP_REG;
		get_datal()->hw_dct.debug_decontour |= DCT_PRE_PRINT_INFO;
		#endif
		return;
	}
	PR_INF("%s:not support\n", __func__);
	get_datal()->dct_op = NULL;
}

void dct_pre_remove(struct platform_device *pdev)
{
	struct di_hdct_s  *dct;

	dct = &get_datal()->hw_dct;
	if (dct->statust & DCT_PRE_LST_CVS) {
		ops_ext()->cvs_free_table(&dct->cvs_dct[0], 6);
		dct->statust &= (~DCT_PRE_LST_CVS);
	}
}

void dim_dbg_dct_info_show(struct seq_file *s, void *v,
			   struct dcntr_mem_s *pprecfg)
{
	if (!pprecfg)
		return;
	seq_printf(s, "index[%d],free[%d],cnt[%d]\n",
			  pprecfg->index, pprecfg->free, pprecfg->cnt_in);

	seq_printf(s, "use_org[%d],ration[%d]\n",
		  pprecfg->use_org, pprecfg->ds_ratio);
	seq_printf(s, "grd_addr[0x%lx],y_addr[0x%lx], c_addr[0x%lx]\n",
		  pprecfg->grd_addr,
		  pprecfg->yds_addr,
		  pprecfg->cds_addr);
	seq_printf(s, "grd_size[%d],yds_size[%d], cds_size[%d]\n",
		  pprecfg->grd_size,
		  pprecfg->yds_size,
		  pprecfg->cds_size);
	seq_printf(s, "out_fmt[0x%x],y_len[%d],c_len[%d]\n",
		  pprecfg->pre_out_fmt,
		  pprecfg->yflt_wrmif_length,
		  pprecfg->cflt_wrmif_length);
	seq_printf(s, "yswap_64 little[%d,%d],c:[%d,%d],grd:[%d,%d]\n",
		  pprecfg->yds_swap_64bit,
		  pprecfg->yds_little_endian,
		  pprecfg->cds_swap_64bit,
		  pprecfg->cds_little_endian,
		  pprecfg->grd_swap_64bit,
		  pprecfg->grd_little_endian);
	seq_printf(s, "yds_canvas_mode[%d],cds_canvas_mode[%d]\n",
		pprecfg->yds_canvas_mode,
		pprecfg->cds_canvas_mode);
}

int dct_pre_show(struct seq_file *s, void *v)
{
	struct di_hdct_s  *dct;
	int i;

	dct = &get_datal()->hw_dct;

	seq_printf(s, "%s:\n", __func__);
	seq_printf(s, "\tstatust<0x%x>:\n",
		   dct->statust);
	for (i = 0; i < DI_CHANNEL_MAX; i++)
		seq_printf(s, "\t%d:statusx<0x%x>:\n",
			   i,
			   dct->statusx[i]);
	if (!IS_IC_SUPPORT(DECONTOUR))
		return 0;

	seq_printf(s, "\t%s<0x%x>:\n", "state",
		   dct->state);
	seq_printf(s, "\t%s<0x%x>:\n", "curr_ch",
			   dct->curr_ch);
	seq_printf(s, "\t%s<0x%x>:\n", "idle_cnt",
				   dct->idle_cnt);
	seq_printf(s, "\t%s<0x%x>:\n", "last_w",
				   dct->last_w);
	seq_printf(s, "\t%s<0x%x>:\n", "last_h",
				   dct->last_h);
	seq_printf(s, "\t%s<0x%x>:\n", "last_type",
				   dct->last_type);

	seq_printf(s, "\t%s<0x%x>:\n", "sbypass_reason",
				   dct->sbypass_reason);
	seq_printf(s, "\t%s<0x%x>:\n", "debug_decontour",
				   dct->debug_decontour);
	seq_printf(s, "\t%s<%d>:\n", "support_canvas",
				   dct->support_canvas);
	seq_printf(s, "\t%s<%d>:\n", "i_do_decontour",
				   dct->i_do_decontour);
	seq_printf(s, "\t%s<%d>:\n", "cmd_bypass",
				   dct->cmd_bypass);
	seq_printf(s, "\t%s<%d>:\n", "nbypass",
				   dct->nbypass);

	seq_printf(s, "\t%s<0x%x>:\n", "irq",
		   dct->irq);
	seq_printf(s, "%s:\n", "cvs:");
	for (i = 0; i < 6; i++)
		seq_printf(s, "\t%d<0x%x>:\n", i,
			   dct->cvs_dct[i]);
	seq_printf(s, "\t%s<0x%x>:\n", "src_fmt",
		   dct->src_fmt);
	seq_printf(s, "\t%s<0x%x>:\n", "mif_out_width",
		   dct->mif_out_width);
	seq_printf(s, "\t%s<0x%x>:\n", "mif_out_height",
		   dct->mif_out_height);
	seq_printf(s, "\t%s<0x%x>:\n", "ds_ratio",
		   dct->ds_ratio);
	seq_printf(s, "\t%s<0x%x>:\n", "mif_read_width",
		   dct->mif_read_width);
	seq_printf(s, "\t%s<0x%x>:\n", "mif_read_height",
		   dct->mif_read_height);
	seq_printf(s, "\t%s<0x%x>:\n", "pic_struct",
		   dct->pic_struct);
	seq_printf(s, "\t%s<0x%x>:\n", "h_avg",
		   dct->h_avg);
	seq_printf(s, "\t%s<0x%x>:\n", "ds_out_width",
		   dct->ds_out_width);
	seq_printf(s, "\t%s<0x%x>:\n", "ds_out_height",
		   dct->ds_out_height);
	seq_printf(s, "\t%s<0x%x>:\n", "skip",
		   dct->skip);
	seq_printf(s, "\t%s<0x%x>:\n", "need_ds",
		   dct->need_ds);

	return 0;
}

int dct_pre_reg_show(struct seq_file *s, void *v)
{
	u32 value;
	int i;
	const struct reg_acc *op = &di_pre_regset;

	if (!IS_IC_SUPPORT(DECONTOUR)) {
		seq_printf(s, "%s\n", "no dct");
		return 0;
	}
	for (i = 0x4a00; i < 0x4a12; i++) {
		value = op->rd(i);
		seq_printf(s, "reg=%x, value= %x\n", i, value);
	}
	value = op->rd(DCNTR_GRD_WMIF_CTRL1);
	seq_printf(s, "cds: DCNTR_GRD_WMIF_CTRL1: %x\n", value);
	value = op->rd(DCNTR_GRD_WMIF_CTRL3);
	seq_printf(s, "cds: DCNTR_GRD_WMIF_CTRL3: %x\n", value);
	value = op->rd(DCNTR_GRD_WMIF_CTRL4);
	seq_printf(s, "cds: DCNTR_GRD_WMIF_CTRL4: %x\n", value);
	value = op->rd(DCNTR_GRD_WMIF_SCOPE_X);
	seq_printf(s, "cds: DCNTR_GRD_WMIF_SCOPE_X: %x\n", value);
	value = op->rd(DCNTR_GRD_WMIF_SCOPE_Y);
	seq_printf(s, "cds: DCNTR_GRD_WMIF_SCOPE_Y: %x\n", value);

	value = op->rd(DCNTR_YDS_WMIF_CTRL1);
	seq_printf(s, "grd: DCNTR_YDS_WMIF_CTRL1: %x\n", value);
	value = op->rd(DCNTR_YDS_WMIF_CTRL3);
	seq_printf(s, "grd: DCNTR_YDS_WMIF_CTRL3: %x\n", value);
	value = op->rd(DCNTR_YDS_WMIF_CTRL4);
	seq_printf(s, "grd: DCNTR_YDS_WMIF_CTRL4: %x\n", value);
	value = op->rd(DCNTR_YDS_WMIF_SCOPE_X);
	seq_printf(s, "grd: DCNTR_YDS_WMIF_SCOPE_X: %x\n", value);
	value = op->rd(DCNTR_YDS_WMIF_SCOPE_Y);
	seq_printf(s, "grd: DCNTR_YDS_WMIF_SCOPE_Y: %x\n", value);

	value = op->rd(DCNTR_CDS_WMIF_CTRL1);
	seq_printf(s, "yds: DCNTR_CDS_WMIF_CTRL1: %x\n", value);
	value = op->rd(DCNTR_CDS_WMIF_CTRL3);
	seq_printf(s, "yds: DCNTR_CDS_WMIF_CTRL3: %x\n", value);
	value = op->rd(DCNTR_CDS_WMIF_CTRL4);
	seq_printf(s, "yds: DCNTR_CDS_WMIF_CTRL4: %x\n", value);
	value = op->rd(DCNTR_CDS_WMIF_SCOPE_X);
	seq_printf(s, "yds: DCNTR_CDS_WMIF_SCOPE_X: %x\n", value);
	value = op->rd(DCNTR_CDS_WMIF_SCOPE_Y);
	seq_printf(s, "yds: DCNTR_CDS_WMIF_SCOPE_Y: %x\n", value);

	value = op->rd(DCNTR_PRE_ARB_MODE);
	seq_printf(s, "DCNTR_PRE_ARB_MODE: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_REQEN_SLV);
	seq_printf(s, "DCNTR_PRE_ARB_REQEN_SLV: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_WEIGH0_SLV);
	seq_printf(s, "DCNTR_PRE_ARB_WEIGH0_SLV: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_WEIGH1_SLV);
	seq_printf(s, "DCNTR_PRE_ARB_WEIGH1_SLV: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_UGT);
	seq_printf(s, "DCNTR_PRE_ARB_UGT: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_LIMT0);
	seq_printf(s, "DCNTR_PRE_ARB_LIMT0: %x\n", value);

	value = op->rd(DCNTR_PRE_ARB_STATUS);
	seq_printf(s, "DCNTR_PRE_ARB_STATUS: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_DBG_CTRL);
	seq_printf(s, "DCNTR_PRE_ARB_DBG_CTRL: %x\n", value);
	value = op->rd(DCNTR_PRE_ARB_PROT);
	seq_printf(s, "DCNTR_PRE_ARB_PROT: %x\n", value);

	value = op->rd(DCTR_BGRID_TOP_FSIZE);
	seq_printf(s, "DCTR_BGRID_TOP_FSIZE: %x\n", value);
	value = op->rd(DCTR_BGRID_TOP_HDNUM);
	seq_printf(s, "DCTR_BGRID_TOP_HDNUM: %x\n", value);
	value = op->rd(DCTR_BGRID_TOP_CTRL0);
	seq_printf(s, "DCTR_BGRID_TOP_CTRL0: %x\n", value);
	value = op->rd(DCTR_BGRID_TOP_FMT);
	seq_printf(s, "DCTR_BGRID_TOP_FMT: %x\n", value);
	value = op->rd(DCTR_BGRID_TOP_GCLK);
	seq_printf(s, "DCTR_BGRID_TOP_GCLK: %x\n", value);
	value = op->rd(DCTR_BGRID_TOP_HOLD);
	seq_printf(s, "DCTR_BGRID_TOP_HOLD: %x\n", value);

	value = op->rd(DCNTR_GRID_GEN_REG); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_GEN_REG=%x\n", value);
	value = op->rd(DCNTR_GRID_GEN_REG2); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_GEN_REG2=%x\n", value);
	value = op->rd(DCNTR_GRID_CANVAS0); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_CANVAS0=%x\n", value);
	value = op->rd(DCNTR_GRID_LUMA_X0); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_LUMA_X0=%x\n", value);
	value = op->rd(DCNTR_GRID_LUMA_Y0); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_LUMA_Y0=%x\n", value);
	value = op->rd(DCNTR_GRID_RPT_LOOP); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_RPT_LOOP=%x\n", value);
	value = op->rd(DCNTR_GRID_LUMA0_RPT_PAT); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_LUMA0_RPT_PAT=%x\n", value);
	value = op->rd(DCNTR_GRID_CHROMA0_RPT_PAT); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_CHROMA0_RPT_PAT=%x\n", value);
	value = op->rd(DCNTR_GRID_DUMMY_PIXEL); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_DUMMY_PIXEL=%x\n", value);

	value = op->rd(DCNTR_GRID_LUMA_FIFO_SIZE); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_LUMA_FIFO_SIZE=%x\n", value);

	value = op->rd(DCNTR_GRID_RANGE_MAP_Y); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_RANGE_MAP_Y=%x\n", value);
	value = op->rd(DCNTR_GRID_RANGE_MAP_CB); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_RANGE_MAP_CB=%x\n", value);
	value = op->rd(DCNTR_GRID_RANGE_MAP_CR); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_RANGE_MAP_CR=%x\n", value);
	value = op->rd(DCNTR_GRID_URGENT_CTRL); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_URGENT_CTRL=%x\n", value);

	value = op->rd(DCNTR_GRID_GEN_REG3); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_GEN_REG3=%x\n", value);
	value = op->rd(DCNTR_GRID_AXI_CMD_CNT); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_AXI_CMD_CNT=%x\n", value);
	value = op->rd(DCNTR_GRID_AXI_RDAT_CNT); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_AXI_RDAT_CNT=%x\n", value);
	value = op->rd(DCNTR_GRID_FMT_CTRL); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_FMT_CTRL=%x\n", value);
	value = op->rd(DCNTR_GRID_FMT_W); //grd_num_use
	seq_printf(s, "dct: DCNTR_GRID_FMT_W=%x\n", value);

	if (DIM_IS_IC(T5))
		return 0;

	value = op->rd(DCNTR_GRID_BADDR_Y);
	seq_printf(s, "dct: DCNTR_GRID_BADDR_Y=%x\n", value);
	value = op->rd(DCNTR_GRID_BADDR_CB);
	seq_printf(s, "dct: DCNTR_GRID_BADDR_CB=%x\n", value);
	value = op->rd(DCNTR_GRID_BADDR_CR);
	seq_printf(s, "dct: DCNTR_GRID_BADDR_CR=%x\n", value);

	value = op->rd(DCNTR_GRID_STRIDE_0);
	seq_printf(s, "dct: DCNTR_GRID_STRIDE_0=%x\n", value);
	value = op->rd(DCNTR_GRID_STRIDE_1);
	seq_printf(s, "dct: DCNTR_GRID_STRIDE_1=%x\n", value);

	value = op->rd(DCNTR_GRID_BADDR_Y_F1);
	seq_printf(s, "dct: DCNTR_GRID_BADDR_Y_F1=%x\n", value);
	value = op->rd(DCNTR_GRID_BADDR_CB_F1);
	seq_printf(s, "dct: DCNTR_GRID_BADDR_CB_F1=%x\n", value);
	value = op->rd(DCNTR_GRID_BADDR_CR_F1);
	seq_printf(s, "dct: DCNTR_GRID_BADDR_CR_F1=%x\n", value);
	value = op->rd(DCNTR_GRID_STRIDE_0_F1);
	seq_printf(s, "dct: DCNTR_GRID_STRIDE_0_F1=%x\n", value);
	value = op->rd(DCNTR_GRID_STRIDE_1_F1);
	seq_printf(s, "dct: DCNTR_GRID_STRIDE_1_F1=%x\n", value);
	return 0;
}

int dct_pre_ch_show(struct seq_file *s, void *v)
{
	unsigned int ch;
	int *ch_pos;
	struct di_ch_s *pch;
	struct di_pre_dct_s *pdct;
	int i;
	char *spltb = "---------------------------";

	ch_pos = (int *)s->private;
	ch = *ch_pos;
	seq_printf(s, "%s:ch[%d]\n", __func__, ch);

	pch = get_chdata(ch);

	pdct = (struct di_pre_dct_s *)pch->dct_pre;

	if (!pdct) {
		seq_printf(s, "%s,no dct pre\n", __func__);
		return 0;
	}

	seq_printf(s, "%s:0x%lx\n", "addr:", pdct->decontour_addr);
	seq_printf(s, "%s:%d\n", "dump_grid:", pdct->dump_grid);
	seq_printf(s, "%s:%d\n", "i_do_decontour:", pdct->i_do_decontour);
	seq_printf(s, "%s:%d\n", "do_cnt:", pdct->do_cnt);

	for (i = 0; i < DCNTR_PRE_POOL_SIZE; i++) {
		seq_printf(s, "%s\n", spltb);
		seq_printf(s, "index[%d]\n", i);
		dim_dbg_dct_info_show(s, v, &pdct->dcntr_mem_info[i]);
	}

	return 0;
}

