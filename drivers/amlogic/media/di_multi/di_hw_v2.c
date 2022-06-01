// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/di_hw_v2.c
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

#include <linux/string.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/atomic.h>

#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

#include "deinterlace.h"
#include "di_data_l.h"

#include "deinterlace_hw.h"
#include "register.h"
#include "register_nr4.h"
#ifdef DET3D
#include "detect3d.h"
#endif
#include "di_reg_v2.h"

#include "di_api.h"
#include "di_reg_tab.h"
#include "di_prc.h"

#include <linux/seq_file.h>

//#if 1
/******************************************************************************
 * before sc2
 ******************************************************************************/
static void set_di_inp_fmt_more(const struct reg_acc *op,
		unsigned int repeat_l0_en,
				int hz_yc_ratio,	/* 2bit */
				int hz_ini_phase,	/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,	/* 2bit */
				int vt_ini_phase,	/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt		/* 1bit */
				);

static void set_di_inp_mif(struct DI_MIF_S  *mif, const struct reg_acc *op);

static void set_di_mem_fmt_more(const struct reg_acc *op, int hfmt_en,
				int hz_yc_ratio,	/* 2bit */
				int hz_ini_phase,	/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,	/* 2bit */
				int vt_ini_phase,	/* 4bit */
				int y_length,
				int c_length,
				int hz_rpt	/* 1bit */
	);

static void set_di_mem_mif(struct DI_MIF_S *mif, const struct reg_acc *op);

static void set_di_chan2_mif(struct DI_MIF_S *mif, const struct reg_acc *op);

static void set_di_inp_fmt_more(const struct reg_acc *op,
				unsigned int repeat_l0_en,
				int hz_yc_ratio,		/* 2bit */
				int hz_ini_phase,		/* 4bit */
				int vfmt_en,
				int vt_yc_ratio,		/* 2bit */
				int vt_ini_phase,	       /* 4bit */
				int y_length,
				int c_length,
				int hz_rpt      /* 1bit */
	       )
{
	int hfmt_en = 1, nrpt_phase0_en = 0;
	int vt_phase_step = (16 >> vt_yc_ratio);

	op->wr(DI_INP_FMT_CTRL,
		(hz_rpt << 28)	       | /* hz rpt pixel */
		(hz_ini_phase << 24)        | /* hz ini phase */
		(0 << 23)		       | /* repeat p0 enable */
		(hz_yc_ratio << 21)	       | /* hz yc ratio */
		(hfmt_en << 20)	       | /* hz enable */
		(nrpt_phase0_en << 17)      | /* nrpt_phase0 enable */
		(repeat_l0_en << 16)        | /* repeat l0 enable */
		(0 << 12)		       | /* skip line num */
		(vt_ini_phase << 8)	       | /* vt ini phase */
		(vt_phase_step << 1)        | /* vt phase step (3.4) */
		(vfmt_en << 0)		 /* vt enable */
		);

	op->wr(DI_INP_FMT_W,
		(y_length << 16) |  /* hz format width */
		(c_length << 0)     /* vt format width */
		);
}

static void set_di_inp_mif(struct DI_MIF_S *mif, const struct reg_acc *op)
{
	unsigned int bytes_per_pixel;
	unsigned int demux_mode;
	unsigned int chro_rpt_lastl_ctrl, vfmt_rpt_first = 0;
	unsigned int luma0_rpt_loop_start;
	unsigned int luma0_rpt_loop_end;
	unsigned int luma0_rpt_loop_pat;
	unsigned int chroma0_rpt_loop_start;
	unsigned int chroma0_rpt_loop_end;
	unsigned int chroma0_rpt_loop_pat;
	unsigned int vt_ini_phase = 0;
	unsigned int reset_on_gofield;
	unsigned int burst_len = 2;
	int urgent = mif->urgent;
	int hold_line = mif->hold_line;
	unsigned int tm;

	if (mif->set_separate_en != 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = mif->src_prog ? 0 : 1;
		chroma0_rpt_loop_end = mif->src_prog ? 0 : 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = mif->src_prog ? 0 : 0x80;

		vfmt_rpt_first = 1;
		if (mif->output_field_num == 0)
			vt_ini_phase = 0xe;
		else
			vt_ini_phase = 0xa;

		if (mif->src_prog) {
			if (mif->output_field_num == 0) {
				vt_ini_phase = 0xc;
			} else {
				vt_ini_phase = 0x4;
				vfmt_rpt_first = 0;
			}
		}

	} else if (mif->set_separate_en != 0 && mif->src_field_mode == 0) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x0;
		chroma0_rpt_loop_pat = 0x0;
	} else if (mif->set_separate_en == 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 1;
		chroma0_rpt_loop_end = 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x80;
	} else {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x00;
		chroma0_rpt_loop_pat = 0x00;
	}
#ifdef HIS_CODE
	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	demux_mode = mif->video_mode;
#else
	bytes_per_pixel = (mif->set_separate_en != 0) ?
			0 : ((mif->video_mode == 2) ? 2 : 1);

	demux_mode = (mif->set_separate_en == 0) ?
			((mif->video_mode == 1) ? 0 :  1) : 0;
#endif

	/* ---------------------- */
	/* General register */
	/* ---------------------- */
	reset_on_gofield = 1;/* default enable according to vlsi */
	//if (dim_is_pre_link_l())
	//	hold_line = 0;
	tm = (reset_on_gofield	<< 29)	|
		(urgent		<< 28)	| /* chroma urgent bit */
		(urgent		<< 27)	| /* luma urgent bit. */
		(1			<< 25)	| /* no dummy data. */
		(hold_line		<< 19)	| /* hold lines */
		(1			<< 18)	| /* push dummy pixel */
		(demux_mode		<< 16)	| /* demux_mode */
		(bytes_per_pixel	<< 14)	|
		(1			<< 12)	| /*burst_size_cr*/
		(1			<< 10)	| /*burst_size_cb*/
		(3			<< 8)	| /*burst_size_y*/
		(mif->l_endian << 4)		|
		(chro_rpt_lastl_ctrl << 6)	|
		((mif->set_separate_en != 0) << 1) |
		(0 << 0);
	dim_print("%s:%d:0x%x\n", __func__, demux_mode, tm);
	op->wr(DI_INP_GEN_REG,
		(reset_on_gofield	<< 29)	|
		(urgent		<< 28)	| /* chroma urgent bit */
		(urgent		<< 27)	| /* luma urgent bit. */
		(1			<< 25)	| /* no dummy data. */
		(hold_line		<< 19)	| /* hold lines */
		(1			<< 18)	| /* push dummy pixel */
		(demux_mode		<< 16)	| /* demux_mode */
		(bytes_per_pixel	<< 14)	|
		(1			<< 12)	| /*burst_size_cr*/
		(1			<< 10)	| /*burst_size_cb*/
		(3			<< 8)	| /*burst_size_y*/
		(mif->l_endian << 4)		|
		(chro_rpt_lastl_ctrl << 6)	|
		((mif->set_separate_en != 0) << 1) |
		(0 << 0)/* cntl_enable */
		);
	dim_print("%s:2:0x%x\n", __func__, op->rd(DI_INP_GEN_REG));
	if (mif->set_separate_en == 2) {
		/* Enable NV12 Display */
		if (mif->cbcr_swap)
			op->bwr(DI_INP_GEN_REG2, 2, 0, 2);
		else
			op->bwr(DI_INP_GEN_REG2, 1, 0, 2);
	} else {
		op->bwr(DI_INP_GEN_REG2, 0, 0, 2);
	}
	if (mif->reg_swap == 1)
		op->bwr(DI_INP_GEN_REG3, 1, 0, 1);
	else
		op->bwr(DI_INP_GEN_REG3, 0, 0, 1);

	if (mif->canvas_w % 32)
		burst_len = 0;
	else if (mif->canvas_w % 64)
		burst_len = 1;
	dim_print("burst_len=%d\n", burst_len);
	op->bwr(DI_INP_GEN_REG3, burst_len & 0x3, 1, 2);
	op->bwr(DI_INP_GEN_REG3, mif->bit_mode & 0x3, 8, 2);
	op->wr(DI_INP_CANVAS0,
		(mif->canvas0_addr2 << 16)  | /* cntl_canvas0_addr2 */
		(mif->canvas0_addr1 << 8)   | /* cntl_canvas0_addr1 */
		(mif->canvas0_addr0 << 0)	 /* cntl_canvas0_addr0 */
		);

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	op->wr(DI_INP_LUMA_X0, (mif->luma_x_end0 << 16) |
		/* cntl_luma_x_end0 */
		(mif->luma_x_start0 << 0)/* cntl_luma_x_start0 */
		);
	op->wr(DI_INP_LUMA_Y0, (mif->luma_y_end0 << 16) |
		/* cntl_luma_y_end0 */
		(mif->luma_y_start0 << 0) /* cntl_luma_y_start0 */
		);
	op->wr(DI_INP_CHROMA_X0, (mif->chroma_x_end0 << 16) |
		(mif->chroma_x_start0 << 0));
	op->wr(DI_INP_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
		(mif->chroma_y_start0 << 0));

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	op->wr(DI_INP_RPT_LOOP,
		(0 << 28) |
		(0 << 24) |
		(0 << 20) |
		(0 << 16) |
		(chroma0_rpt_loop_start << 12)      |
		(chroma0_rpt_loop_end << 8)	       |
		(luma0_rpt_loop_start << 4)	       |
		(luma0_rpt_loop_end << 0)
		);

	op->wr(DI_INP_LUMA0_RPT_PAT, luma0_rpt_loop_pat);
	op->wr(DI_INP_CHROMA0_RPT_PAT, chroma0_rpt_loop_pat);

	/* Dummy pixel value */
	op->wr(DI_INP_DUMMY_PIXEL, 0x00808000);
	if (mif->set_separate_en != 0) {/* 4:2:0 block mode.*/
		set_di_inp_fmt_more(op, vfmt_rpt_first,/* hfmt_en */
			1,/* hz_yc_ratio */
			0,/* hz_ini_phase */
			1,/* vfmt_en */
			mif->src_prog ? 0 : 1,/* vt_yc_ratio */
			vt_ini_phase,/* vt_ini_phase */
			mif->luma_x_end0 - mif->luma_x_start0 + 1,
			/* y_length */
			mif->chroma_x_end0 - mif->chroma_x_start0 + 1,
			/* c length */
			0); /* hz repeat. */
	} else {
		set_di_inp_fmt_more(op, vfmt_rpt_first,     /* hfmt_en */
			1,  /* hz_yc_ratio */
			0,  /* hz_ini_phase */
			0,  /* vfmt_en */
			0,  /* vt_yc_ratio */
			0,  /* vt_ini_phase */
			mif->luma_x_end0 - mif->luma_x_start0 + 1,
			((mif->luma_x_end0 >> 1) -
			(mif->luma_x_start0 >> 1) + 1),
			0); /* hz repeat. */
	}
}

static void set_di_mem_fmt_more(const struct reg_acc *op, int hfmt_en,
			       int hz_yc_ratio,        /* 2bit */
			       int hz_ini_phase,       /* 4bit */
			       int vfmt_en,
			       int vt_yc_ratio,        /* 2bit */
			       int vt_ini_phase,       /* 4bit */
			       int y_length,
			       int c_length,
			       int hz_rpt      /* 1bit */
	       )
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	op->wr(DI_MEM_FMT_CTRL,
		(hz_rpt << 28)	       | /* hz rpt pixel */
		(hz_ini_phase << 24)        | /* hz ini phase */
		(0 << 23)		       | /* repeat p0 enable */
		(hz_yc_ratio << 21)	       | /* hz yc ratio */
		(hfmt_en << 20)	       | /* hz enable */
		(1 << 17)		       | /* nrpt_phase0 enable */
		(0 << 16)		       | /* repeat l0 enable */
		(0 << 12)		       | /* skip line num */
		(vt_ini_phase << 8)	       | /* vt ini phase */
		(vt_phase_step << 1)        | /* vt phase step (3.4) */
		(vfmt_en << 0)	       /* vt enable */
		);

	op->wr(DI_MEM_FMT_W,
		(y_length << 16) |  /* hz format width */
		(c_length << 0)     /* vt format width */
		);
}

static void set_di_chan2_fmt_more(const struct reg_acc *op, int hfmt_en,
				 int hz_yc_ratio,/* 2bit */
				 int hz_ini_phase,/* 4bit */
				 int vfmt_en,
				 int vt_yc_ratio,/* 2bit */
				 int vt_ini_phase,/* 4bit */
				 int y_length,
				 int c_length,
				 int hz_rpt    /* 1bit */
				 )
{
	int vt_phase_step = (16 >> vt_yc_ratio);

	op->wr(DI_CHAN2_FMT_CTRL,
		(hz_rpt << 28)	       | /* hz rpt pixel */
		(hz_ini_phase << 24)        | /* hz ini phase */
		(0 << 23)		       | /* repeat p0 enable */
		(hz_yc_ratio << 21)	       | /* hz yc ratio */
		(hfmt_en << 20)	       | /* hz enable */
		(1 << 17)		       | /* nrpt_phase0 enable */
		(0 << 16)		       | /* repeat l0 enable */
		(0 << 12)		       | /* skip line num */
		(vt_ini_phase << 8)	       | /* vt ini phase */
		(vt_phase_step << 1)        | /* vt phase step (3.4) */
		(vfmt_en << 0)	       /* vt enable */
		);

	op->wr(DI_CHAN2_FMT_W, (y_length << 16) | /* hz format width */
		(c_length << 0) /* vt format width */
		);
}

static void set_di_mem_mif(struct DI_MIF_S *mif, const struct reg_acc *op)
{
	unsigned int bytes_per_pixel;
	unsigned int demux_mode;
	unsigned int chro_rpt_lastl_ctrl;
	unsigned int luma0_rpt_loop_start;
	unsigned int luma0_rpt_loop_end;
	unsigned int luma0_rpt_loop_pat;
	unsigned int chroma0_rpt_loop_start;
	unsigned int chroma0_rpt_loop_end;
	unsigned int chroma0_rpt_loop_pat;
	unsigned int reset_on_gofield;
	int urgent = mif->urgent;
	int hold_line = mif->hold_line;

	if (mif->set_separate_en != 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 1;
		chroma0_rpt_loop_end = 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x80;
	} else if (mif->set_separate_en != 0 && mif->src_field_mode == 0) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x0;
		chroma0_rpt_loop_pat = 0x0;
	} else if (mif->set_separate_en == 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x00;
	} else {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x00;
		chroma0_rpt_loop_pat = 0x00;
	}

#ifdef HIS_CODE //ary copy from ucode:
	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
	demux_mode = mif->video_mode;
#else
	bytes_per_pixel = (mif->set_separate_en != 0) ?
			0 : ((mif->video_mode ==  2) ? 2 : 1);

	// demux_mode : 0 = 4:2:2 demux, 1 = RGB demuxing from a single FIFO
	demux_mode = (mif->set_separate_en == 0) ?
			((mif->video_mode == 1) ? 0 : 1) : 0;
#endif
	/* ---------------------- */
	/* General register */
	/* ---------------------- */
	reset_on_gofield = 1;/* default enable according to vlsi */
	//if (dim_is_pre_link_l())
	//	       hold_line = 0;

	op->wr(DI_MEM_GEN_REG,
		(reset_on_gofield << 29)    | /* reset on go field */
		(urgent << 28)	       | /* urgent bit. */
		(urgent << 27)	       | /* urgent bit. */
		(1 << 25)		       | /* no dummy data. */
		(hold_line << 19)	       | /* hold lines */
		(1 << 18)		       | /* push dummy pixel */
		(demux_mode << 16)	       | /* demux_mode */
		(bytes_per_pixel << 14)     |
		(1 << 12)		       | /*burst_size_cr*/
		(1 << 10)		       | /*burst_size_cb*/
		(3 << 8)		       | /*burst_size_y*/
		(mif->l_endian << 4)		|
		(chro_rpt_lastl_ctrl << 6)  |
		((mif->set_separate_en != 0) << 1)  |
		(0 << 0)		       /* cntl_enable */
		);
	if (mif->set_separate_en == 2) {
		if (mif->cbcr_swap)
			op->bwr(DI_MEM_GEN_REG2, 2, 0, 2);
		else
			op->bwr(DI_MEM_GEN_REG2, 1, 0, 2);
	} else {
		op->bwr(DI_MEM_GEN_REG2, 0, 0, 2);
	}
	op->bwr(DI_MEM_GEN_REG3, mif->bit_mode & 0x3, 8, 2);
	if (mif->reg_swap == 1)
		op->bwr(DI_MEM_GEN_REG3, 1, 0, 1);
	else
		op->bwr(DI_MEM_GEN_REG3, 0, 0, 1);
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	op->wr(DI_MEM_CANVAS0,
		(mif->canvas0_addr2 << 16)  |
		/* cntl_canvas0_addr2 */
		(mif->canvas0_addr1 << 8)   |
		(mif->canvas0_addr0 << 0));

	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	op->wr(DI_MEM_LUMA_X0,
		(mif->luma_x_end0 << 16) |
		(mif->luma_x_start0 << 0) /* cntl_luma_x_start0 */
		);
	op->wr(DI_MEM_LUMA_Y0,
		(mif->luma_y_end0 << 16) |
		(mif->luma_y_start0 << 0) /* cntl_luma_y_start0 */
		);
	op->wr(DI_MEM_CHROMA_X0,
		(mif->chroma_x_end0 << 16) |
		(mif->chroma_x_start0 << 0)
		);
	op->wr(DI_MEM_CHROMA_Y0,
		(mif->chroma_y_end0 << 16) |
		(mif->chroma_y_start0 << 0)
		);

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	op->wr(DI_MEM_RPT_LOOP, (0 << 28) |
		(0      << 24) |
		(0      << 20) |
		(0	 << 16) |
		(chroma0_rpt_loop_start << 12) |
		(chroma0_rpt_loop_end << 8) |
		(luma0_rpt_loop_start << 4) |
		(luma0_rpt_loop_end << 0)
		);

	op->wr(DI_MEM_LUMA0_RPT_PAT, luma0_rpt_loop_pat);
	op->wr(DI_MEM_CHROMA0_RPT_PAT, chroma0_rpt_loop_pat);

	/* Dummy pixel value */
	op->wr(DI_MEM_DUMMY_PIXEL, 0x00808000);
	if (mif->set_separate_en != 0) {/* 4:2:0 block mode.*/
		set_di_mem_fmt_more(op,
		1,  /* hfmt_en */
		1,  /* hz_yc_ratio */
		0,  /* hz_ini_phase */
		1,  /* vfmt_en */
		1,  /* vt_yc_ratio */
		0,  /* vt_ini_phase */
		mif->luma_x_end0 - mif->luma_x_start0 + 1,
		/* y_length */
		mif->chroma_x_end0 - mif->chroma_x_start0 + 1,
		/* c length */
		0);     /* hz repeat. */
	} else {
		set_di_mem_fmt_more(op, 1,  /* hfmt_en */
		1,  /* hz_yc_ratio */
		0,  /* hz_ini_phase */
		0,  /* vfmt_en */
		0,  /* vt_yc_ratio */
		0,  /* vt_ini_phase */
		mif->luma_x_end0 - mif->luma_x_start0 + 1,
		((mif->luma_x_end0 >> 1)
		- (mif->luma_x_start0 >> 1) + 1),
		0); /* hz repeat. */
	}
}

static void set_di_chan2_mif(struct DI_MIF_S *mif, const struct reg_acc *op)
{
	unsigned int bytes_per_pixel;
	unsigned int demux_mode;
	unsigned int chro_rpt_lastl_ctrl;
	unsigned int luma0_rpt_loop_start;
	unsigned int luma0_rpt_loop_end;
	unsigned int luma0_rpt_loop_pat;
	unsigned int chroma0_rpt_loop_start;
	unsigned int chroma0_rpt_loop_end;
	unsigned int chroma0_rpt_loop_pat;
	unsigned int reset_on_gofield;
	int urgent = mif->urgent;
	int hold_line = mif->hold_line;

	if (mif->set_separate_en != 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 1;
		chroma0_rpt_loop_end = 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x80;
	} else if (mif->set_separate_en != 0 && mif->src_field_mode == 0) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x0;
		chroma0_rpt_loop_pat = 0x0;
	} else if (mif->set_separate_en == 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 1;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x00;
	} else {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x00;
		chroma0_rpt_loop_pat = 0x00;
	}
#ifdef MARK_HIS
	bytes_per_pixel = mif->set_separate_en ? 0 : (mif->video_mode ? 2 : 1);
#else
	if (mif->set_separate_en) {
		bytes_per_pixel = 0;
	} else {
		if (mif->video_mode)
			bytes_per_pixel = 2;
		else
			bytes_per_pixel = 1;
	}
#endif
	demux_mode = mif->video_mode;

	/* ---------------------- */
	/* General register */
	/* ---------------------- */
	reset_on_gofield = 1;/* default enable according to vlsi */
	op->wr(DI_CHAN2_GEN_REG,
		(reset_on_gofield << 29)    |
		(urgent << 28)	       |       /* urgent */
		(urgent << 27)	       |       /* luma urgent */
		(1 << 25)		       |       /* no dummy data. */
		(hold_line << 19)	       |       /* hold lines */
		(1 << 18)		       |       /* push dummy pixel */
		(demux_mode << 16)	       |
		(bytes_per_pixel << 14)     |
		(1 << 12)		       |       /*burst_size_cr*/
		(1 << 10)		       |       /*burst_size_cb*/
		(3 << 8)		       |       /*burst_size_y*/
		(chro_rpt_lastl_ctrl << 6)  |
		((mif->set_separate_en != 0) << 1)  |
		(0 << 0)		       /* cntl_enable */
		);
	/* ---------------------- */
	/* Canvas */
	/* ---------------------- */
	if (mif->set_separate_en == 2) {
		/* Enable NV12 Display */
		op->bwr(DI_CHAN2_GEN_REG2, 1, 0, 1);
	} else {
		op->bwr(DI_CHAN2_GEN_REG2, 0, 0, 1);
	}
	op->bwr(DI_CHAN2_GEN_REG3, mif->bit_mode & 0x3, 8, 2);
	op->wr(DI_CHAN2_CANVAS0, (mif->canvas0_addr2 << 16) |
	(mif->canvas0_addr1 << 8) |
	(mif->canvas0_addr0 << 0));
	/* ---------------------- */
	/* Picture 0 X/Y start,end */
	/* ---------------------- */
	op->wr(DI_CHAN2_LUMA_X0, (mif->luma_x_end0 << 16) |
		/* cntl_luma_x_end0 */
		(mif->luma_x_start0 << 0));
	op->wr(DI_CHAN2_LUMA_Y0, (mif->luma_y_end0 << 16) |
		(mif->luma_y_start0 << 0));
	op->wr(DI_CHAN2_CHROMA_X0, (mif->chroma_x_end0 << 16) |
		(mif->chroma_x_start0 << 0));
	op->wr(DI_CHAN2_CHROMA_Y0, (mif->chroma_y_end0 << 16) |
		(mif->chroma_y_start0 << 0));

	/* ---------------------- */
	/* Repeat or skip */
	/* ---------------------- */
	op->wr(DI_CHAN2_RPT_LOOP,
		(0 << 28)		       |
		(0	<< 24)		       |
		(0	<< 20)		       |
		(0	<< 16)		       |
		(0	<< 12)		       |
		(0	<< 8)		       |
		(luma0_rpt_loop_start << 4) |
		(luma0_rpt_loop_end << 0)
		);

	op->wr(DI_CHAN2_LUMA0_RPT_PAT, luma0_rpt_loop_pat);

	/* Dummy pixel value */
	op->wr(DI_CHAN2_DUMMY_PIXEL, 0x00808000);

	if (mif->set_separate_en != 0) {        /* 4:2:0 block mode. */
		set_di_chan2_fmt_more(op, 1,        /* hfmt_en */
		1,        /* hz_yc_ratio */
		0,        /* hz_ini_phase */
		1,        /* vfmt_en */
		1,        /* vt_yc_ratio */
		0,        /* vt_ini_phase */
		mif->luma_x_end0 -
		mif->luma_x_start0 + 1,   /* y_length */
		mif->chroma_x_end0 -
		mif->chroma_x_start0 + 1,/* c length */
		0);       /* hz repeat. */
	} else {
		set_di_chan2_fmt_more(op, 1,        /* hfmt_en */
		1,        /* hz_yc_ratio */
		0,        /* hz_ini_phase */
		0,        /* vfmt_en */
		0,        /* vt_yc_ratio */
		0,        /* vt_ini_phase */
		mif->luma_x_end0 -
		mif->luma_x_start0 + 1,   /* y_length */
		((mif->luma_x_end0 >> 1) -
		(mif->luma_x_start0 >> 1) + 1),
		0);       /* hz repeat. */
	}
}

static void set_di_mif_pre_v2(struct DI_MIF_S *mif,
			enum DI_MIF0_ID mif_index,
			const struct reg_acc *op)
{
	if (mif_index > DI_MIF0_ID_MEM) {
		PR_ERR("%s:overflow:%d\n", __func__, mif_index);
		return;
	}

	mif->urgent	= dimp_get(edi_mp_pre_urgent);
	mif->hold_line	= dimp_get(edi_mp_pre_hold_line);
	switch (mif_index) {
	case DI_MIF0_ID_INP:
		set_di_inp_mif(mif, op);
		break;
	case DI_MIF0_ID_CHAN2:
		set_di_chan2_mif(mif, op);
		break;
	case DI_MIF0_ID_MEM:
		set_di_mem_mif(mif, op);
		break;
	default:
		break;
	}
}

static const unsigned int mif_contr_reg[MIF_NUB][MIF_REG_NUB] = {
	[DI_MIF0_ID_INP] = {
		DI_INP_GEN_REG,
		DI_INP_GEN_REG2,
		DI_INP_GEN_REG3,
		DI_INP_CANVAS0,
		DI_INP_LUMA_X0,
		DI_INP_LUMA_Y0,
		DI_INP_CHROMA_X0,
		DI_INP_CHROMA_Y0,
		DI_INP_RPT_LOOP,
		DI_INP_LUMA0_RPT_PAT,
		DI_INP_CHROMA0_RPT_PAT,
		DI_INP_DUMMY_PIXEL,
		DI_INP_FMT_CTRL,
		DI_INP_FMT_W,
		DI_INP_LUMA_FIFO_SIZE},
	[DI_MIF0_ID_MEM] = {
		DI_MEM_GEN_REG,
		DI_MEM_GEN_REG2,
		DI_MEM_GEN_REG3,
		DI_MEM_CANVAS0,
		DI_MEM_LUMA_X0,
		DI_MEM_LUMA_Y0,
		DI_MEM_CHROMA_X0,
		DI_MEM_CHROMA_Y0,
		DI_MEM_RPT_LOOP,
		DI_MEM_LUMA0_RPT_PAT,
		DI_MEM_CHROMA0_RPT_PAT,
		DI_MEM_DUMMY_PIXEL,
		DI_MEM_FMT_CTRL,
		DI_MEM_FMT_W,
		DI_MEM_LUMA_FIFO_SIZE
		},
	[DI_MIF0_ID_CHAN2] = {
		DI_CHAN2_GEN_REG,
		DI_CHAN2_GEN_REG2,
		DI_CHAN2_GEN_REG3,
		DI_CHAN2_CANVAS0,
		DI_CHAN2_LUMA_X0,
		DI_CHAN2_LUMA_Y0,
		DI_CHAN2_CHROMA_X0,
		DI_CHAN2_CHROMA_Y0,
		DI_CHAN2_RPT_LOOP,
		DI_CHAN2_LUMA0_RPT_PAT,
		DI_CHAN2_CHROMA0_RPT_PAT,
		DI_CHAN2_DUMMY_PIXEL,
		DI_CHAN2_FMT_CTRL,
		DI_CHAN2_FMT_W,
		DI_CHAN2_LUMA_FIFO_SIZE
		},
	[DI_MIF0_ID_IF0] = {
		DI_IF0_GEN_REG,
		DI_IF0_GEN_REG2,
		DI_IF0_GEN_REG3,
		DI_IF0_CANVAS0,
		DI_IF0_LUMA_X0,
		DI_IF0_LUMA_Y0,
		DI_IF0_CHROMA_X0,
		DI_IF0_CHROMA_Y0,
		DI_IF0_REPEAT_LOOP,
		DI_IF0_LUMA0_RPT_PAT,
		DI_IF0_CHROMA0_RPT_PAT,
		DI_IF0_DUMMY_PIXEL,
		DI_IF0_FMT_CTRL,
		DI_IF0_FMT_W,
		DI_IF0_LUMA_FIFO_SIZE,
		},
	[DI_MIF0_ID_IF1] = {
		DI_IF1_GEN_REG,
		DI_IF1_GEN_REG2,
		DI_IF1_GEN_REG3,
		DI_IF1_CANVAS0,
		DI_IF1_LUMA_X0,
		DI_IF1_LUMA_Y0,
		DI_IF1_CHROMA_X0,
		DI_IF1_CHROMA_Y0,
		DI_IF1_RPT_LOOP,
		DI_IF1_LUMA0_RPT_PAT,
		DI_IF1_CHROMA0_RPT_PAT,
		DI_IF1_DUMMY_PIXEL,
		DI_IF1_FMT_CTRL,
		DI_IF1_FMT_W,
		DI_IF1_LUMA_FIFO_SIZE
		},
	[DI_MIF0_ID_IF2] = {
		DI_IF2_GEN_REG,
		DI_IF2_GEN_REG2,
		DI_IF2_GEN_REG3,
		DI_IF2_CANVAS0,
		DI_IF2_LUMA_X0,
		DI_IF2_LUMA_Y0,
		DI_IF2_CHROMA_X0,
		DI_IF2_CHROMA_Y0,
		DI_IF2_RPT_LOOP,
		DI_IF2_LUMA0_RPT_PAT,
		DI_IF2_CHROMA0_RPT_PAT,
		DI_IF2_DUMMY_PIXEL,
		DI_IF2_FMT_CTRL,
		DI_IF2_FMT_W,
		DI_IF2_LUMA_FIFO_SIZE
		},

};

const unsigned int *mif_reg_get_v2(enum DI_MIF0_ID mif_index)
{
	if (mif_index > DI_MIF0_ID_IF2)
		return NULL;

	return &mif_contr_reg[mif_index][0];
}

//#ifdef DIM_OUT_NV21 /* move to di_hw_v2.c */
/**********************************************************
 * rebuild setting
 **********************************************************/

static const unsigned int reg_mifs[EDI_MIFS_NUB][EDI_MIFS_REG_NUB] = {
	{ /* pre nr mif */
		DI_NRWR_X,
		DI_NRWR_Y,
		DI_NRWR_CTRL,	/*write only once*/
	},
	{ /* post wr mif */
		DI_DIWR_X,
		DI_DIWR_Y,
		DI_DIWR_CTRL,
	},
};

static void set_wrmif_simple_pp_addr_only_v2(struct DI_MIF_S *mif,
				const struct reg_acc *ops,
				enum EDI_MIFSM mifsel)
{
	const unsigned int *reg;
//	unsigned int bits_mode, rgb_mode;
	const struct reg_acc *op;
	unsigned int ctr;

	if (!ops)
		op = &di_pre_regset;
	else
		op = ops;

	reg = &reg_mifs[mifsel][0];

	ctr = op->rd(reg[WRMIF_CTRL]);
	ctr &= (~0xffff);
	ctr |= mif->canvas0_addr0;
	ctr |= mif->canvas0_addr1 << 8;
	op->wr(reg[WRMIF_CTRL], ctr);
}

static void set_wrmif_simple_pp_v2(struct DI_MIF_S *mif,
	const struct reg_acc *ops,
	enum EDI_MIFSM mifsel)
{
	const unsigned int *reg;
	unsigned int ctr;

	reg = &reg_mifs[mifsel][0];

	//dimh_enable_di_post_2
	/*set all*/

	//if (!mif->ddr_en)
	///	return;

	ops->wr(reg[WRMIF_X],
		(mif->l_endian		<< 31)	|
		(mif->luma_x_start0	<< 16)	|	/* [29:16]*/
		(mif->luma_x_end0));			/* [13:0] */
	ops->wr(reg[WRMIF_Y],
		(3			<< 30)	|
		(mif->luma_y_start0	<< 16)	|
		/* wr ext en from gxtvbb */
		(1			<< 15)	|
		((mif->bit_mode & 0x1) << 14)	|
		(mif->luma_y_end0));

	/* MIFS_CTRL */
	ctr = 0;
	ctr |= (2 << 26)		|
	       (mif->urgent << 16)	|	/*urgent*/
	       /* swap cbcrworking in rgb mode =2: swap cbcr */
	       (mif->cbcr_swap << 17)	|
	       (0 << 18)		| /*vcon working in rgb mode =2:*/
	       (0 << 20)		| /* hconv. output even pixel*/
	       /*rgb mode =0, 422 YCBCR to one canvas.*/
	       (0 << 22)		|
	       (0 << 24)		|
	       (mif->reg_swap << 30);
	if (mif->set_separate_en == 0) {
		ctr |= (mif->canvas0_addr0 & 0xff);	/* canvas index.*/
	} else if (mif->set_separate_en == 2) {
		ctr |= (mif->canvas0_addr0 & 0x00ff)	| /* Y canvas index.*/
			/*CBCR canvas index*/
		       (mif->canvas0_addr1 << 8)	|
		       /* vcon working in rgb mode =2: 3 : output all.*/
		       (((mif->video_mode == 0) ? 0 : 3) << 18) |
		       /* hconv. output even pixel */
		       (((mif->video_mode == 2) ? 3 : 0) << 20) |
		       (2 << 22);      //enable auto clock gating in nrwr_mif.
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL) &&
	    ((mif->bit_mode & 0x3) == 0x3))
		ctr = ctr | (3 << 22);

	ops->wr(reg[WRMIF_CTRL], ctr);

	dim_print("bit_mode[%d] 0x%x=[0x%x]\n",
		  mif->bit_mode, reg[WRMIF_Y], RD(reg[WRMIF_Y]));
	dim_print("0x%x=[0x%x]\n", reg[WRMIF_CTRL], RD(reg[WRMIF_CTRL]));
}

static const struct regs_t reg_bits_wr[] = {
	{WRMIF_X,  16,  14, ENR_MIF_INDEX_X_ST, "x_start"},
	{WRMIF_X,  0,  14, ENR_MIF_INDEX_X_END, "x_end"},
	{WRMIF_Y,  16,  13, ENR_MIF_INDEX_Y_ST, "y_start"},
	{WRMIF_Y,  0,  13, ENR_MIF_INDEX_Y_END, "y_end"},
	{WRMIF_CTRL, 0,  16, ENR_MIF_INDEX_CVS, "canvas"},
	{WRMIF_CTRL, 0,  1, ENR_MIF_INDEX_EN, "nothing"},
	{WRMIF_Y, 14,  1, ENR_MIF_INDEX_BIT_MODE, "10bit mode"},
	{WRMIF_X, 31,  1, ENR_MIF_INDEX_ENDIAN, "endian"},
	{WRMIF_CTRL, 16,  1, ENR_MIF_INDEX_URGENT, "urgent"},
	{WRMIF_CTRL, 17,  1, ENR_MIF_INDEX_CBCR_SW, "cbcr_sw"},
	{WRMIF_CTRL, 22,  3, ENR_MIF_INDEX_RGB_MODE, "rgb_mode"},
	{TABLE_FLG_END, TABLE_FLG_END, 0xff, 0xff, "end"},
};

static const struct reg_t rtab_g12_contr_bits_tab[] = {
	/*--------------------------*/

	/***********************************************/
	{DI_PRE_CTRL, 29, 1, 0, "",
			"pre_field_num",
			""},
	{DI_PRE_CTRL, 26, 2, 0, "",
			"mode_444c422",
			""},
	{DI_PRE_CTRL, 25, 1, 0, "",
			"di_cont_read_en",
			""},
	{DI_PRE_CTRL, 23, 2, 0, "",
			"mode_422c444",
			""},
	{DI_PRE_CTRL, 21, 1, 0, "",
			"pre field num for nr",
			""},
	{DI_PRE_CTRL, 20, 1, 0, "",
			"pre field num for pulldown",
			""},
	{DI_PRE_CTRL, 19, 1, 0, "",
			"pre field num for mcdi",
			""},
	{DI_PRE_CTRL, 17, 1, 0, "",
			"reg_me_autoen",
			""},
	{DI_PRE_CTRL, 16, 1, 0, "",
			"reg_me_en",
			""},
	{DI_PRE_CTRL, 9, 1, 0, "",
			"di_buf2_en,chan2",
			""},
	{DI_PRE_CTRL, 8, 1, 0, "",
			"di_buf3_en,mem",
			""},
	{DI_PRE_CTRL, 5, 1, 0, "",
			"hist_check_en",
			""},
	{DI_PRE_CTRL, 4, 1, 0, "",
			"check_after_nr",
			""},
	{DI_PRE_CTRL, 3, 1, 0, "",
			"check222p_en",
			""},
	{DI_PRE_CTRL, 2, 1, 0, "",
			"check322p_en",
			""},
	{DI_PRE_CTRL, 1, 1, 0, "",
			"mtn_en",
			""},
	{DI_PRE_CTRL, 0, 1, 0, "",
			"nr_en",
			""},
	/***********************************************/

	{DI_POST_CTRL, 5, 1, 0, "",
			"blend_en",
			""},
	{DI_POST_CTRL, 2, 1, 0, "",
			"ei_en",
			""},
	{DI_POST_CTRL, 6, 1, 0, "",
			"mux_en",
			"1: only tfbf; 2: normal;3: normal + tfbf"},
	{DI_POST_CTRL, 7, 1, 0, "",
			"ddr_en",
			"wr_bk_en"},
	{DI_POST_CTRL, 8, 1, 0, "",
			"vpp_out_en",
			""},
	{DI_BLEND_CTRL, 28, 1, 0, "",
			"post mb en",
			"0"},
	{TABLE_FLG_END, 0, 0, 0, "end", "end", ""},

};

static void set_di_mif_v2_addr_only(struct DI_MIF_S *mif, enum DI_MIF0_ID mif_index,
		   const struct reg_acc *opin)
{
	unsigned int off;
	const unsigned int *reg;
	const struct reg_acc *op;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;
#ifdef HIS_CODE
	if (is_mask(SC2_REG_MSK_nr)) { /* dbg */
		op = &sc2reg;
		PR_INF("%s:%s:\n", __func__, dim_get_mif_id_name(mif_index));
	}
#endif
	reg = mif_reg_get_v2(mif_index);
	off = 0;/*di_mif_add_get_offset_v3(mif_index);*/

	if (!reg) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	dbg_ic("%s:id[%d]\n", __func__, mif_index);

	// ----------------------
	// Canvas
	// ----------------------
	if (mif->linear) {
		PR_ERR("%s:linear?\n", __func__);
		//di_mif0_linear_rd_cfg_addr_only(mif, mif_index, op);
	} else {
		op->wr(off + reg[MIF_CANVAS0], (mif->canvas0_addr2 << 16) |
			// cntl_canvas0_addr2
			(mif->canvas0_addr1 << 8)      | // cntl_canvas0_addr1
			(mif->canvas0_addr0 << 0)        // cntl_canvas0_addr0
			);
	}
}

const struct dim_hw_opsv_s dim_ops_l1_v2 = {
	.info = {
		.name = "l1_before_sc2",
		.update = "2020-06-01",
		.main_version	= 2,
		.sub_version	= 1,
	},

	.pre_mif_set	= set_di_mif_pre_v2,
	.mif_rd_update_addr = set_di_mif_v2_addr_only,
	.set_wrmif_pp = set_wrmif_simple_pp_v2,
	.wrmif_update_addr = set_wrmif_simple_pp_addr_only_v2,
#ifdef HIS_CODE
	.wrmif_set	= dimh_wrmif_set,

	.pst_mif_set	= set_di_mif_post_v2,
	.pst_mif_update_csv	= pst_mif_update_canvasid_v2,
	.pre_mif_sw	= di_pre_data_mif_ctrl,
	.pst_mif_sw	= post_mif_sw,
	.pst_mif_rst	= post_mif_rst,
	.pst_mif_rev	= post_mif_rev,
	.pst_dbg_contr	= post_dbg_contr,
	.pst_set_flow	= di_post_set_flow,
	.pst_bit_mode_cfg	= post_bit_mode_config,

	.wr_cfg_mif	= wr_mif_cfg_v2,

	.wrmif_sw_buf	= dimh_wr_mif_update,
	.wrmif_trig	= dimh_wr_mif_trig,
	.wr_rst_protect	= dim_rst_protect,
	.hw_init	= hw_init_v2,
	.pre_hold_block_txlx = pre_hold_block_txlx,

	.pre_cfg_mif	= config_di_mif_v2,
	.dbg_reg_pre_mif_print	= dbg_reg_pre_mif_print,
	.dbg_reg_pst_mif_print	= dbg_reg_pst_mif_print,
	.dbg_reg_pre_mif_print2	= dbg_reg_pre_mif_print2,
	.dbg_reg_pst_mif_print2	= dbg_reg_pst_mif_print2,
	.dbg_reg_pre_mif_show	= dbg_reg_pre_mif_show,
	.dbg_reg_pst_mif_show	= dbg_reg_pst_mif_show,
	.pre_gl_sw	= hpre_gl_sw,
	.pre_gl_thd	= hpre_gl_thd_v2,
	.pst_gl_thd	= hpost_gl_thd_v2,
#endif
	.reg_mif_tab	= {
		[DI_MIF0_ID_INP] = &mif_contr_reg[DI_MIF0_ID_INP][0],
		[DI_MIF0_ID_CHAN2] = &mif_contr_reg[DI_MIF0_ID_CHAN2][0],
		[DI_MIF0_ID_MEM] = &mif_contr_reg[DI_MIF0_ID_MEM][0],
		[DI_MIF0_ID_IF1] = &mif_contr_reg[DI_MIF0_ID_IF1][0],
		[DI_MIF0_ID_IF0] = &mif_contr_reg[DI_MIF0_ID_IF0][0],
		[DI_MIF0_ID_IF2] = &mif_contr_reg[DI_MIF0_ID_IF2][0],
	},
	.reg_mif_wr_tab	= {
		[EDI_MIFSM_NR] = &reg_mifs[EDI_MIFSM_NR][0],
		[EDI_MIFSM_WR] = &reg_mifs[EDI_MIFSM_WR][0],
	},
	.reg_mif_wr_bits_tab = &reg_bits_wr[0],
	.rtab_contr_bits_tab = &rtab_g12_contr_bits_tab[0],
};

const struct dim_hw_opsv_s  *opl1_v2(void)
{
	return &dim_ops_l1_v2;
}

