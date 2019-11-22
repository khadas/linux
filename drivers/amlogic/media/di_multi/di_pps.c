/*
 * drivers/amlogic/media/di_multi/di_pps.c
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/amlogic/media/registers/regs/di_regs.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include "di_pps.h"
#include "register.h"

#include <linux/seq_file.h>

#if 0
/* pps filter coefficients */
#define COEF_BICUBIC         0
#define COEF_3POINT_TRIANGLE 1
#define COEF_4POINT_TRIANGLE 2
#define COEF_BILINEAR        3
#define COEF_2POINT_BILINEAR 4
#define COEF_BICUBIC_SHARP   5
#define COEF_3POINT_TRIANGLE_SHARP   6
#define COEF_3POINT_BSPLINE  7
#define COEF_4POINT_BSPLINE  8
#define COEF_3D_FILTER       9
#define COEF_NULL            0xff
#define TOTAL_FILTERS        10

#define MAX_NONLINEAR_FACTOR    0x40

const u32 vpp_filter_coefs_bicubic_sharp[] = {
	3,
	33 | 0x8000,
	/* 0x01f80090, 0x01f80100, 0xff7f0200, 0xfe7f0300, */
	0x01fa008c, 0x01fa0100, 0xff7f0200, 0xfe7f0300,
	0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
	0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
	0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
	0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
	0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	0xf84848f8
};

const u32 vpp_filter_coefs_bicubic[] = {
	4,
	33,
	0x00800000, 0x007f0100, 0xff7f0200, 0xfe7f0300,
	0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
	0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
	0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
	0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
	0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
	0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
	0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	0xf84848f8
};

const u32 vpp_filter_coefs_bilinear[] = {
	4,
	33,
	0x00800000, 0x007e0200, 0x007c0400, 0x007a0600,
	0x00780800, 0x00760a00, 0x00740c00, 0x00720e00,
	0x00701000, 0x006e1200, 0x006c1400, 0x006a1600,
	0x00681800, 0x00661a00, 0x00641c00, 0x00621e00,
	0x00602000, 0x005e2200, 0x005c2400, 0x005a2600,
	0x00582800, 0x00562a00, 0x00542c00, 0x00522e00,
	0x00503000, 0x004e3200, 0x004c3400, 0x004a3600,
	0x00483800, 0x00463a00, 0x00443c00, 0x00423e00,
	0x00404000
};

const u32 vpp_3d_filter_coefs_bilinear[] = {
	2,
	33,
	0x80000000, 0x7e020000, 0x7c040000, 0x7a060000,
	0x78080000, 0x760a0000, 0x740c0000, 0x720e0000,
	0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
	0x68180000, 0x661a0000, 0x641c0000, 0x621e0000,
	0x60200000, 0x5e220000, 0x5c240000, 0x5a260000,
	0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
	0x50300000, 0x4e320000, 0x4c340000, 0x4a360000,
	0x48380000, 0x463a0000, 0x443c0000, 0x423e0000,
	0x40400000
};

const u32 vpp_filter_coefs_3point_triangle[] = {
	3,
	33,
	0x40400000, 0x3f400100, 0x3d410200, 0x3c410300,
	0x3a420400, 0x39420500, 0x37430600, 0x36430700,
	0x35430800, 0x33450800, 0x32450900, 0x31450a00,
	0x30450b00, 0x2e460c00, 0x2d460d00, 0x2c470d00,
	0x2b470e00, 0x29480f00, 0x28481000, 0x27481100,
	0x26491100, 0x25491200, 0x24491300, 0x234a1300,
	0x224a1400, 0x214a1500, 0x204a1600, 0x1f4b1600,
	0x1e4b1700, 0x1d4b1800, 0x1c4c1800, 0x1b4c1900,
	0x1a4c1a00
};

/* point_num =4, filt_len =4, group_num = 64, [1 2 1] */
const u32 vpp_filter_coefs_4point_triangle[] = {
	4,
	33,
	0x20402000, 0x20402000, 0x1f3f2101, 0x1f3f2101,
	0x1e3e2202, 0x1e3e2202, 0x1d3d2303, 0x1d3d2303,
	0x1c3c2404, 0x1c3c2404, 0x1b3b2505, 0x1b3b2505,
	0x1a3a2606, 0x1a3a2606, 0x19392707, 0x19392707,
	0x18382808, 0x18382808, 0x17372909, 0x17372909,
	0x16362a0a, 0x16362a0a, 0x15352b0b, 0x15352b0b,
	0x14342c0c, 0x14342c0c, 0x13332d0d, 0x13332d0d,
	0x12322e0e, 0x12322e0e, 0x11312f0f, 0x11312f0f,
	0x10303010
};

/*
 *4th order (cubic) b-spline
 *filt_cubic point_num =4, filt_len =4, group_num = 64, [1 5 1]
 */
const u32 vpp_filter_coefs_4point_bspline[] = {
	4,
	33,
	0x15561500, 0x14561600, 0x13561700, 0x12561800,
	0x11551a00, 0x11541b00, 0x10541c00, 0x0f541d00,
	0x0f531e00, 0x0e531f00, 0x0d522100, 0x0c522200,
	0x0b522300, 0x0b512400, 0x0a502600, 0x0a4f2700,
	0x094e2900, 0x084e2a00, 0x084d2b00, 0x074c2c01,
	0x074b2d01, 0x064a2f01, 0x06493001, 0x05483201,
	0x05473301, 0x05463401, 0x04453601, 0x04433702,
	0x04423802, 0x03413a02, 0x03403b02, 0x033f3c02,
	0x033d3d03
};

/*3rd order (quadratic) b-spline*/
/*filt_quadratic, point_num =3, filt_len =3, group_num = 64, [1 6 1] */
const u32 vpp_filter_coefs_3point_bspline[] = {
	3,
	33,
	0x40400000, 0x3e420000, 0x3c440000, 0x3a460000,
	0x38480000, 0x364a0000, 0x344b0100, 0x334c0100,
	0x314e0100, 0x304f0100, 0x2e500200, 0x2c520200,
	0x2a540200, 0x29540300, 0x27560300, 0x26570300,
	0x24580400, 0x23590400, 0x215a0500, 0x205b0500,
	0x1e5c0600, 0x1d5c0700, 0x1c5d0700, 0x1a5e0800,
	0x195e0900, 0x185e0a00, 0x175f0a00, 0x15600b00,
	0x14600c00, 0x13600d00, 0x12600e00, 0x11600f00,
	0x10601000
};

/*filt_triangle, point_num =3, filt_len =2.6, group_num = 64, [1 7 1] */
const u32 vpp_filter_coefs_3point_triangle_sharp[] = {
	3,
	33,
	0x40400000, 0x3e420000, 0x3d430000, 0x3b450000,
	0x3a460000, 0x38480000, 0x37490000, 0x354b0000,
	0x344c0000, 0x324e0000, 0x314f0000, 0x2f510000,
	0x2e520000, 0x2c540000, 0x2b550000, 0x29570000,
	0x28580000, 0x265a0000, 0x245c0000, 0x235d0000,
	0x215f0000, 0x20600000, 0x1e620000, 0x1d620100,
	0x1b620300, 0x19630400, 0x17630600, 0x15640700,
	0x14640800, 0x12640a00, 0x11640b00, 0x0f650c00,
	0x0d660d00
};

const u32 vpp_filter_coefs_2point_binilear[] = {
	2,
	33,
	0x80000000, 0x7e020000, 0x7c040000, 0x7a060000,
	0x78080000, 0x760a0000, 0x740c0000, 0x720e0000,
	0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
	0x68180000, 0x661a0000, 0x641c0000, 0x621e0000,
	0x60200000, 0x5e220000, 0x5c240000, 0x5a260000,
	0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
	0x50300000, 0x4e320000, 0x4c340000, 0x4a360000,
	0x48380000, 0x463a0000, 0x443c0000, 0x423e0000,
	0x40400000
};

static const u32 *filter_table[] = {
	vpp_filter_coefs_bicubic,
	vpp_filter_coefs_3point_triangle,
	vpp_filter_coefs_4point_triangle,
	vpp_filter_coefs_bilinear,
	vpp_filter_coefs_2point_binilear,
	vpp_filter_coefs_bicubic_sharp,
	vpp_filter_coefs_3point_triangle_sharp,
	vpp_filter_coefs_3point_bspline,
	vpp_filter_coefs_4point_bspline,
	vpp_3d_filter_coefs_bilinear
};

static int chroma_filter_table[] = {
	COEF_BICUBIC, /* bicubic */
	COEF_3POINT_TRIANGLE,
	COEF_4POINT_TRIANGLE,
	COEF_4POINT_TRIANGLE, /* bilinear */
	COEF_2POINT_BILINEAR,
	COEF_3POINT_TRIANGLE, /* bicubic_sharp */
	COEF_3POINT_TRIANGLE, /* 3point_triangle_sharp */
	COEF_3POINT_TRIANGLE, /* 3point_bspline */
	COEF_4POINT_TRIANGLE, /* 4point_bspline */
	COEF_3D_FILTER		  /* can not change */
};

static unsigned int vert_scaler_filter = 0xff;
module_param(vert_scaler_filter, uint, 0664);
MODULE_PARM_DESC(vert_scaler_filter, "vert_scaler_filter");

static unsigned int vert_chroma_scaler_filter = 0xff;
module_param(vert_chroma_scaler_filter, uint, 0664);
MODULE_PARM_DESC(vert_chroma_scaler_filter, "vert_chroma_scaler_filter");

static unsigned int horz_scaler_filter = 0xff;
module_param(horz_scaler_filter, uint, 0664);
MODULE_PARM_DESC(horz_scaler_filter, "horz_scaler_filter");

bool pre_scaler_en = true;
module_param(pre_scaler_en, bool, 0664);
MODULE_PARM_DESC(pre_scaler_en, "pre_scaler_en");
#endif
/*bicubic*/
static const unsigned int di_filt_coef0[] = {
	0x00800000,
	0x007f0100,
	0xff7f0200,
	0xfe7f0300,
	0xfd7e0500,
	0xfc7e0600,
	0xfb7d0800,
	0xfb7c0900,
	0xfa7b0b00,
	0xfa7a0dff,
	0xf9790fff,
	0xf97711ff,
	0xf87613ff,
	0xf87416fe,
	0xf87218fe,
	0xf8701afe,
	0xf76f1dfd,
	0xf76d1ffd,
	0xf76b21fd,
	0xf76824fd,
	0xf76627fc,
	0xf76429fc,
	0xf7612cfc,
	0xf75f2ffb,
	0xf75d31fb,
	0xf75a34fb,
	0xf75837fa,
	0xf7553afa,
	0xf8523cfa,
	0xf8503ff9,
	0xf84d42f9,
	0xf84a45f9,
	0xf84848f8
};

/* 2 point bilinear */
static const unsigned int di_filt_coef1[] = {
	0x00800000,
	0x007e0200,
	0x007c0400,
	0x007a0600,
	0x00780800,
	0x00760a00,
	0x00740c00,
	0x00720e00,
	0x00701000,
	0x006e1200,
	0x006c1400,
	0x006a1600,
	0x00681800,
	0x00661a00,
	0x00641c00,
	0x00621e00,
	0x00602000,
	0x005e2200,
	0x005c2400,
	0x005a2600,
	0x00582800,
	0x00562a00,
	0x00542c00,
	0x00522e00,
	0x00503000,
	0x004e3200,
	0x004c3400,
	0x004a3600,
	0x00483800,
	0x00463a00,
	0x00443c00,
	0x00423e00,
	0x00404000
};

/* 2 point bilinear, bank_length == 2*/
static const unsigned int di_filt_coef2[] = {
	 0x80000000,
	 0x7e020000,
	 0x7c040000,
	 0x7a060000,
	 0x78080000,
	 0x760a0000,
	 0x740c0000,
	 0x720e0000,
	 0x70100000,
	 0x6e120000,
	 0x6c140000,
	 0x6a160000,
	 0x68180000,
	 0x661a0000,
	 0x641c0000,
	 0x621e0000,
	 0x60200000,
	 0x5e220000,
	 0x5c240000,
	 0x5a260000,
	 0x58280000,
	 0x562a0000,
	 0x542c0000,
	 0x522e0000,
	 0x50300000,
	 0x4e320000,
	 0x4c340000,
	 0x4a360000,
	 0x48380000,
	 0x463a0000,
	 0x443c0000,
	 0x423e0000,
	 0x40400000
};

#define ZOOM_BITS       20
#define PHASE_BITS      16

static enum f2v_vphase_type_e top_conv_type = F2V_P2P;
static enum f2v_vphase_type_e bot_conv_type = F2V_P2P;
static unsigned int prehsc_en;
static unsigned int prevsc_en;

static const unsigned char f2v_420_in_pos_luma[F2V_TYPE_MAX] = {
0, 2, 0, 2, 0, 0, 0, 2, 0};
#if 0
static const unsigned char f2v_420_in_pos_chroma[F2V_TYPE_MAX] = {
	1, 5, 1, 5, 2, 2, 1, 5, 2};
#endif
static const unsigned char f2v_420_out_pos[F2V_TYPE_MAX] = {
0, 2, 2, 0, 0, 2, 0, 0, 0};

static void f2v_get_vertical_phase(unsigned int zoom_ratio,
				   enum f2v_vphase_type_e type,
				   unsigned char bank_length,
				   struct pps_f2v_vphase_s *vphase)
{
	int offset_in, offset_out;

    /* luma */
	offset_in = f2v_420_in_pos_luma[type] << PHASE_BITS;
	offset_out = (f2v_420_out_pos[type] * zoom_ratio)
		>> (ZOOM_BITS - PHASE_BITS);

	vphase->rcv_num = bank_length;
	if (bank_length == 4 || bank_length == 3)
		vphase->rpt_num = 1;
	else
		vphase->rpt_num = 0;

	if (offset_in > offset_out) {
		vphase->rpt_num = vphase->rpt_num + 1;
		vphase->phase =
			((4 << PHASE_BITS) + offset_out - offset_in) >> 2;
	} else {
		while ((offset_in + (4 << PHASE_BITS)) <= offset_out) {
			if (vphase->rpt_num == 1)
				vphase->rpt_num = 0;
			else
				vphase->rcv_num++;
			offset_in += 4 << PHASE_BITS;
		}
		vphase->phase = (offset_out - offset_in) >> 2;
	}
}

/*
 * patch 1: inp scaler 0: di wr scaler
 * support: TM2
 * not support: SM1
 */
void dim_pps_config(unsigned char path, int src_w, int src_h,
		    int dst_w, int dst_h)
{
	struct pps_f2v_vphase_s vphase;

	int i;
	int hsc_en = 0, vsc_en = 0;
	int vsc_double_line_mode;
	unsigned int p_src_w, p_src_h;
	unsigned int vert_phase_step, horz_phase_step;
	unsigned char top_rcv_num, bot_rcv_num;
	unsigned char top_rpt_num, bot_rpt_num;
	unsigned short top_vphase, bot_vphase;
	unsigned char is_frame;
	int vert_bank_length = 4;

	const unsigned int *filt_coef0 = di_filt_coef0;
	/*unsigned int *filt_coef1 = di_filt_coef1;*/
	const unsigned int *filt_coef2 = di_filt_coef2;

	vsc_double_line_mode = 0;

	if (src_h != dst_h)
		vsc_en = 1;
	if (src_w != dst_w)
		hsc_en = 1;
	/* config hdr size */
	Wr_reg_bits(DI_HDR_IN_HSIZE, dst_w, 0, 13);
	Wr_reg_bits(DI_HDR_IN_VSIZE, dst_h, 0, 13);
	p_src_w = (prehsc_en ? ((src_w + 1) >> 1) : src_w);
	p_src_h = prevsc_en ? ((src_h + 1) >> 1) : src_h;

	Wr(DI_SC_HOLD_LINE, 0x10);

	if (p_src_w > 2048) {
		/*force vert bank length = 2*/
		vert_bank_length = 2;
		vsc_double_line_mode = 1;
	}

	/*write vert filter coefs*/
	Wr(DI_SC_COEF_IDX, 0x0000);
	for (i = 0; i < 33; i++) {
		if (vert_bank_length == 2)
			Wr(DI_SC_COEF, filt_coef2[i]); /*bilinear*/
		else
			Wr(DI_SC_COEF, filt_coef0[i]); /*bicubic*/
	}

	/*write horz filter coefs*/
	Wr(DI_SC_COEF_IDX, 0x0100);
	for (i = 0; i < 33; i++)
		Wr(DI_SC_COEF, filt_coef0[i]); /*bicubic*/

	if (p_src_h > 2048)
		vert_phase_step = ((p_src_h << 18) / dst_h) << 2;
	else
		vert_phase_step = (p_src_h << 20) / dst_h;
	if (p_src_w > 2048)
		horz_phase_step = ((p_src_w << 18) / dst_w) << 2;
	else
		horz_phase_step = (p_src_w << 20) / dst_w;

	is_frame = ((top_conv_type == F2V_IT2P) ||
			(top_conv_type == F2V_IB2P) ||
			(top_conv_type == F2V_P2P));

	if (is_frame) {
		f2v_get_vertical_phase(vert_phase_step, top_conv_type,
				       vert_bank_length, &vphase);
		top_rcv_num = vphase.rcv_num;
		top_rpt_num = vphase.rpt_num;
		top_vphase  = vphase.phase;

		bot_rcv_num = 0;
		bot_rpt_num = 0;
		bot_vphase  = 0;
	} else {
		f2v_get_vertical_phase(vert_phase_step, top_conv_type,
				       vert_bank_length, &vphase);
		top_rcv_num = vphase.rcv_num;
		top_rpt_num = vphase.rpt_num;
		top_vphase = vphase.phase;

		f2v_get_vertical_phase(vert_phase_step, bot_conv_type,
				       vert_bank_length, &vphase);
		bot_rcv_num = vphase.rcv_num;
		bot_rpt_num = vphase.rpt_num;
		bot_vphase = vphase.phase;
	}
	vert_phase_step = (vert_phase_step << 4);
	horz_phase_step = (horz_phase_step << 4);

	Wr(DI_SC_LINE_IN_LENGTH, src_w);
	Wr(DI_SC_PIC_IN_HEIGHT, src_h);
	Wr(DI_VSC_REGION12_STARTP,  0);
	Wr(DI_VSC_REGION34_STARTP, ((dst_h << 16) | dst_h));
	Wr(DI_VSC_REGION4_ENDP, (dst_h - 1));

	Wr(DI_VSC_START_PHASE_STEP, vert_phase_step);
	Wr(DI_VSC_REGION0_PHASE_SLOPE, 0);
	Wr(DI_VSC_REGION1_PHASE_SLOPE, 0);
	Wr(DI_VSC_REGION3_PHASE_SLOPE, 0);
	Wr(DI_VSC_REGION4_PHASE_SLOPE, 0);

	Wr(DI_VSC_PHASE_CTRL,
	   ((vsc_double_line_mode << 17)	|
	   (!is_frame) << 16)			|
	   (0 << 15)				|
	   (bot_rpt_num << 13)			|
	   (bot_rcv_num << 8)			|
	   (0 << 7)				|
	   (top_rpt_num << 5)			|
	   (top_rcv_num));
	Wr(DI_VSC_INI_PHASE, (bot_vphase << 16) | top_vphase);
	Wr(DI_HSC_REGION12_STARTP, 0);
	Wr(DI_HSC_REGION34_STARTP, (dst_w << 16) | dst_w);
	Wr(DI_HSC_REGION4_ENDP, dst_w - 1);

	Wr(DI_HSC_START_PHASE_STEP, horz_phase_step);
	Wr(DI_HSC_REGION0_PHASE_SLOPE, 0);
	Wr(DI_HSC_REGION1_PHASE_SLOPE, 0);
	Wr(DI_HSC_REGION3_PHASE_SLOPE, 0);
	Wr(DI_HSC_REGION4_PHASE_SLOPE, 0);

	Wr(DI_HSC_PHASE_CTRL, (1 << 21) | (4 << 16) | 0);
	Wr_reg_bits(DI_SC_TOP_CTRL, (path ? 3 : 0), 29, 2);
	Wr(DI_SC_MISC,
	   (prevsc_en << 21)	|
	   (prehsc_en << 20)	|	/* prehsc_en */
	   (prevsc_en << 19)	|	/* prevsc_en */
	   (vsc_en << 18)	|	/* vsc_en */
	   (hsc_en << 17)	|	/* hsc_en */
	   ((vsc_en | hsc_en) << 16)	|	/* sc_top_en */
	   (1 << 15)		|	/* vd1 sc out enable */
	   (0 << 12)		|	/* horz nonlinear 4region enable */
	   (4 << 8)		|	/* horz scaler bank length */
	   (0 << 5)		|	/* vert scaler phase field enable */
	   (0 << 4)		|	/* vert nonlinear 4region enable */
	   (vert_bank_length << 0)	/* vert scaler bank length */
	   );

	pr_info("[pps] %s input %d %d output %d %d.\n",
		path ? "pre" : "post", src_w, src_h, dst_w, dst_h);
}

/*
 * 0x374e ~ 0x376d, 20 regs
 */
void dim_dump_pps_reg(unsigned int base_addr)
{
	unsigned int i = 0x374e;

	pr_info("-----dump pps start-----\n");
	for (i = 0x374e; i < 0x376e; i++) {
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + (i << 2),
			i, dim_RDMA_RD(i));
	}
	pr_info("-----dump pps end-----\n");
}

/*
 * di pre h scaling down function
 * only have h scaling down
 * support: sm1 tm2 ...
 * 0x37b0 ~ 0x37b5
 */
void dim_inp_hsc_setting(u32 src_w, u32 dst_w)
{
	u32 i;
	u32 hsc_en;
	u32 horz_phase_step;
	const int *filt_coef0 = di_filt_coef0;
	/*int *filt_coef1 = di_filt_coef1;*/
	/*int *filt_coef2 = di_filt_coef2;*/

	if (src_w == dst_w) {
		hsc_en = 0;
	} else {
		hsc_en = 1;
		/*write horz filter coefs*/
		dim_RDMA_WR(DI_VIU_HSC_COEF_IDX, 0x0100);
		for (i = 0; i < 33; i++)	/*bicubic*/
			dim_RDMA_WR(DI_VIU_HSC_COEF, filt_coef0[i]);

		horz_phase_step = (src_w << 20) / dst_w;
		horz_phase_step = (horz_phase_step << 4);
		dim_RDMA_WR(DI_VIU_HSC_WIDTHM1,
			    (src_w - 1) << 16 | (dst_w - 1));
		dim_RDMA_WR(DI_VIU_HSC_PHASE_STEP, horz_phase_step);
		dim_RDMA_WR(DI_VIU_HSC_PHASE_CTRL, 0);
	}
	dim_RDMA_WR(DI_VIU_HSC_CTRL,
		    (4 << 20)	|	/* initial receive number*/
		    (0 << 12)	|	/* initial pixel ptr*/
		    (1 << 10)	|	/* repeat first pixel number*/
		    (0 << 8)	|	/* sp422 mode*/
		    (4 << 4)	|	/* horz scaler bank length*/
		    (0 << 2)	|	/* phase0 always en*/
		    (0 << 1)	|	/* nearest_en*/
		    (hsc_en << 0));	/* hsc_en*/
}

/*
 * 0x37b0 ~ 0x37b5
 */
void dim_dump_hdownscler_reg(unsigned int base_addr)
{
	unsigned int i = 0x374e;

	pr_info("-----dump hdownscler start-----\n");
	for (i = 0x37b0; i < 0x37b5; i++) {
		pr_info("[0x%x][0x%x]=0x%x\n",
			base_addr + (i << 2),
			i, dim_RDMA_RD(i));
	}
	pr_info("-----dump hdownscler end-----\n");
}

int dim_seq_file_module_para_pps(struct seq_file *seq)
{
	seq_puts(seq, "pps---------------\n");

	return 0;
}

