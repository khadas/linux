// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/di_decont.c
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include "deinterlace.h"

#include "di_data_l.h"
#include "reg_decontour.h"
#include "reg_decontour_t3.h"
#include "register.h"
#include "di_prc.h"

#ifdef DIM_DCT_BORDER_DETECT
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#endif
/************************************************
 * dbg_dct
 *	bit 0: disable dct pre //used for enable decontour
 *	bit 1: disable dct post
 *	bit 2: disable: di_pq_db_setting
 *	bit 4: grid_use_fix
 *	bit 5: nset
 *	bit 6:8: pq ?
 *		dcntr dynamic used dbg_dct BIT:6-8
 *	bit 12:13: demo mode (old is bit 8:9)
 *	bit 14: border detect simulation only use when
 *		DIM_DCT_BORDER_SIMULATION enable
 *	bit 15: border detect disable
 ************************************************/
/*bit 4: grid use fix */
/*bit 9:8: demo left /right */
static unsigned int dbg_dct;
module_param_named(dbg_dct, dbg_dct, uint, 0664);

bool disable_ppmng(void)
{
	if (dbg_dct & DI_BIT0)
		return true;
	return false;
}
EXPORT_SYMBOL(disable_ppmng);

bool disable_di_decontour(void)
{
	if (dbg_dct & DI_BIT1)
		return true;
	return false;
}

void di_decontour_disable(bool on)
{
	if (on)
		bset(&dbg_dct, 1);
	else
		bclr(&dbg_dct, 1);
}

/* dcntr dynamic used dbg_dct BIT:6-8*/
bool dcntr_dynamic_alpha_1(void)
{
	if (dbg_dct & DI_BIT6)
		return true;
	return false;
}

bool dcntr_dynamic_alpha_2(void)
{
	if (dbg_dct & DI_BIT7)
		return true;
	return false;
}

bool dcntr_dynamic_disable(void)
{
	if (dbg_dct & DI_BIT8)
		return true;
	return false;
}

bool dcntr_border_simulation_have(void)
{
	if (dbg_dct & DI_BIT14)
		return true;
	return false;
}

bool dcntr_border_detect_disable(void)
{
	if (dbg_dct & DI_BIT15)
		return true;
	return false;
}

enum ECNTR_MIF_IDX {
	ECNTR_MIF_IDX_DIVR,
	ECNTR_MIF_IDX_GRID,
	ECNTR_MIF_IDX_YFLT,
	ECNTR_MIF_IDX_CFLT,
};

enum E1INT_RMIF_REG {
	EINT_RMIF_CTRL1,
	EINT_RMIF_CTRL2,
	EINT_RMIF_CTRL3,
	EINT_RMIF_CTRL4,
	EINT_RMIF_SCOPE_X,
	EINT_RMIF_SCOPE_Y
};

enum EBITS_MIF {
	EBITS_MIF_SYNC_SEL,
	EBITS_MIF_CVS_ID,
	EBITS_MIF_CMD_INTR_LEN,
	EBITS_MIF_CMD_REQ_SIZE,
	EBITS_MIF_BRST_LEN,
	EBITS_MIF_SWAP_64,
	EBITS_MIF_LITTLE_ENDIAN,
	EBITS_MIF_Y_REV,
	EBITS_MIF_X_REV,
	EBITS_MIF_SRC_FMT,
	EBITS_MIF_SW_RST,
	EBITS_MIF_VSTEP,
	EBITS_MIF_INT_CLR,
	EBITS_MIF_GCLK_CTRL,
	EBITS_MIF_URGENT_CTRL,
	EBITS_MIF_MEM_MODE,
	EBITS_MIF_LINEAR_LTH,
	EBITS_MIF_ADDR,
	EBITS_MIF_X_START,
	EBITS_MIF_X_END,
	EBITS_MIF_Y_START,
	EBITS_MIF_Y_END,
};

#define DCNTR_NUB_MIF	4
#define DCNTR_NUB_REG	6
#define DCNTR_NUB_PARA8 22
static const unsigned int reg_mif[DCNTR_NUB_MIF][DCNTR_NUB_REG] = {
	[ECNTR_MIF_IDX_DIVR] = { //0:divr  1:grid  2:yflt  3:cflt
			DCNTR_DIVR_RMIF_CTRL1,
			DCNTR_DIVR_RMIF_CTRL2,
			DCNTR_DIVR_RMIF_CTRL3,
			DCNTR_DIVR_RMIF_CTRL4,
			DCNTR_DIVR_RMIF_SCOPE_X,
			DCNTR_DIVR_RMIF_SCOPE_Y},
	[ECNTR_MIF_IDX_GRID] = {
			DCNTR_GRID_RMIF_CTRL1,
			DCNTR_GRID_RMIF_CTRL2,
			DCNTR_GRID_RMIF_CTRL3,
			DCNTR_GRID_RMIF_CTRL4,
			DCNTR_GRID_RMIF_SCOPE_X,
			DCNTR_GRID_RMIF_SCOPE_Y},
	[ECNTR_MIF_IDX_YFLT] = {
			DCNTR_YFLT_RMIF_CTRL1,
			DCNTR_YFLT_RMIF_CTRL2,
			DCNTR_YFLT_RMIF_CTRL3,
			DCNTR_YFLT_RMIF_CTRL4,
			DCNTR_YFLT_RMIF_SCOPE_X,
			DCNTR_YFLT_RMIF_SCOPE_Y},
	[ECNTR_MIF_IDX_CFLT] = {
			DCNTR_CFLT_RMIF_CTRL1,
			DCNTR_CFLT_RMIF_CTRL2,
			DCNTR_CFLT_RMIF_CTRL3,
			DCNTR_CFLT_RMIF_CTRL4,
			DCNTR_CFLT_RMIF_SCOPE_X,
			DCNTR_CFLT_RMIF_SCOPE_Y
		},
};

static const unsigned int reg_mif_t3[DCNTR_NUB_MIF][DCNTR_NUB_REG] = {
	[ECNTR_MIF_IDX_DIVR] = { //0:divr  1:grid  2:yflt  3:cflt
			DCNTR_T3_DIVR_RMIF_CTRL1,
			DCNTR_T3_DIVR_RMIF_CTRL2,
			DCNTR_T3_DIVR_RMIF_CTRL3,
			DCNTR_T3_DIVR_RMIF_CTRL4,
			DCNTR_T3_DIVR_RMIF_SCOPE_X,
			DCNTR_T3_DIVR_RMIF_SCOPE_Y},
	[ECNTR_MIF_IDX_GRID] = {
			DCNTR_T3_GRID_RMIF_CTRL1,
			DCNTR_T3_GRID_RMIF_CTRL2,
			DCNTR_T3_GRID_RMIF_CTRL3,
			DCNTR_T3_GRID_RMIF_CTRL4,
			DCNTR_T3_GRID_RMIF_SCOPE_X,
			DCNTR_T3_GRID_RMIF_SCOPE_Y},
	[ECNTR_MIF_IDX_YFLT] = {
			DCNTR_T3_YFLT_RMIF_CTRL1,
			DCNTR_T3_YFLT_RMIF_CTRL2,
			DCNTR_T3_YFLT_RMIF_CTRL3,
			DCNTR_T3_YFLT_RMIF_CTRL4,
			DCNTR_T3_YFLT_RMIF_SCOPE_X,
			DCNTR_T3_YFLT_RMIF_SCOPE_Y},
	[ECNTR_MIF_IDX_CFLT] = {
			DCNTR_T3_CFLT_RMIF_CTRL1,
			DCNTR_T3_CFLT_RMIF_CTRL2,
			DCNTR_T3_CFLT_RMIF_CTRL3,
			DCNTR_T3_CFLT_RMIF_CTRL4,
			DCNTR_T3_CFLT_RMIF_SCOPE_X,
			DCNTR_T3_CFLT_RMIF_SCOPE_Y
		},
};

struct dcntr_mif_s {
	unsigned int mem_mode	: 2,  //0:linear address mode	1:canvas mode
		mif_reverse	: 2, // 0 : no reverse //ary: no use?
		src_fmt		: 4,
		// 0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit
		canvas_id	: 8,
		vstep		: 8, //
		little		: 1,
		sw_64bit	: 1,
		burst		: 2,
		reverse2	: 4;
	int mif_x_start;
	int mif_x_end;
	int mif_y_start;
	int mif_y_end;
	int linear_baddr;
	int linear_length;
};

struct dcntr_in_s {
	unsigned int x_size;
	unsigned int y_size;
	unsigned int ds_x;
	unsigned int ds_y;
	unsigned int grd_x;
	unsigned int grd_y;

	//int	grd_num_mode;
	unsigned int DS_RATIO;//  0  //0:(1:1)  1:(1:2)  2:(1:4)
	unsigned int addr[4];	//0:divr  1:grid  2:yflt  3:cflt
	unsigned int len[4];
	unsigned int	use_cvs		: 1, /*ary add*/
			in_ds_mode	: 1, /*default is 0*/
			sig_path	: 2, /*default is 0*/
			grd_num_mode	: 2,
			divrsmap_blk0_sft :2, /* tmp */

			grid_use_fix	: 1, /* tmp */
			rsv1		: 23;
};

struct dcntr_core_s {
	unsigned int flg_int	: 1,
			support	: 1,
			st_set	: 1,
			st_pause : 1,

			st_off	: 1,
			n_set	: 1,
			n_up	: 1,
			n_rp	: 1,

			cvs_y	: 8,
			cvs_uv	: 8,
			demo	: 2,
			n_demo	: 1,
			n_bypass : 1, // for post can't do
			burst	: 2, /* for cvs */
			rev	: 2;
	unsigned int l_xsize;
	unsigned int l_ysize;
	unsigned int l_ds_x;
	unsigned int l_ds_y;
	struct dcntr_in_s in;
	struct dcntr_mem_s in_cfg;
	struct dcntr_mif_s pstr[4];
	int grd_vbin[DCNTR_NUB_PARA8]; //reg_grd_vbin_gs_0 ~ reg_grd_vbin_gs_21
	const unsigned int	*reg_mif_tab[DCNTR_NUB_MIF];
	const struct regs_t	*reg_mif_bits_tab;
	const struct reg_t	*reg_contr_bits;
	struct dcntr_mem_s *p_in_cfg; /* for move dct */
	struct di_ch_s *pch;	/* for move dct */
};

void dbg_dct_core_other(struct dcntr_core_s *pcore)
{
	if (!pcore)
		return;
	if (!(di_dbg & DBG_M_DCT))
		return;
	dim_print("%s:\n", __func__);

	dim_print("\tlsize<%d,%d,%d,%d>:\n",
		  pcore->l_xsize, pcore->l_ysize,
		  pcore->l_ds_x, pcore->l_ds_y);
	dim_print("\tint[%d],sup[%d],st_set[%d],st_pause[%d]\n",
		  pcore->flg_int, pcore->support,
		  pcore->st_set, pcore->st_pause);
	dim_print("\tst_off[%d],n_set[%d],n_up[%d],cvs_y[%d],cvs_uv[%d]\n",
		  pcore->st_off, pcore->n_set, pcore->n_up,
		  pcore->cvs_y, pcore->cvs_uv);
	dim_print("\t:bypass:%d\n", pcore->n_bypass);
}

void dbg_dct_in(struct dcntr_in_s *pin)
{
	int i;

	if (!pin)
		return;
	if (!(di_dbg & DBG_M_DCT))
		return;
	dim_print("%s:\n", __func__);
	dim_print("\tsize<%d,%d,%d,%d>:\n", pin->x_size, pin->y_size,
		  pin->ds_x, pin->ds_y);
	dim_print("\tgrd_x[%d],y[%d],ration[%d]:\n", pin->grd_x, pin->grd_y,
		  pin->DS_RATIO);
	for (i = 0; i < 4; i++) {
		dim_print("\t:%d:addr:0x%x, len:%d\n", i,
			  pin->addr[i],
			  pin->len[i]);
	}
	dim_print("\tuse_cvs[%d],in_ds_mode[%d],sig_path[%d]\n",
		  pin->use_cvs,
		  pin->in_ds_mode,
		  pin->sig_path);
	dim_print("grd_num_mode[%d],blk0[%d]\n",
		  pin->grd_num_mode,
		  pin->divrsmap_blk0_sft);
}

void dbg_dct_mif(struct dcntr_mif_s *pmif)
{
	if (!pmif)
		return;
	if (!(di_dbg & DBG_M_DCT))
		return;
	dim_print("%s:\n", __func__);
	dim_print("\tsize<%d,%d,%d,%d>:\n",
		  pmif->mif_x_start, pmif->mif_x_end,
		  pmif->mif_y_start, pmif->mif_y_end);
	dim_print("\tmem_mod[%d], src_fmt[%d],cvs_id[%d],vstep[%d]\n",
		  pmif->mem_mode, pmif->src_fmt, pmif->canvas_id,
		  pmif->vstep);
	dim_print("\taddr[0x%x],len[%d]\n", pmif->linear_baddr,
		  pmif->linear_length);
}

static struct dcntr_core_s di_dcnt;
//static struct dcntr_core_s *pdcnt;
const struct dcntr_mif_s pstr_default = {
	.mem_mode	= 0,  //0:linear address mode	1:canvas mode
	.mif_reverse	= 0, // 0 : no reverse //ary: no use?
	.src_fmt	= 1, // 0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit
	.canvas_id	= 0,
	.vstep		= 1, //
	.reverse2	= 0,
	.mif_x_start	= 0,
	.mif_y_start	= 0,
	.linear_length	= 0
};

static const struct regs_t reg_bits_mif[] = {
	{EINT_RMIF_CTRL1,  24,  2, EBITS_MIF_SYNC_SEL, "reg_sync_sel"},
	{EINT_RMIF_CTRL1,  16,  8, EBITS_MIF_CVS_ID, "reg_canvas_id"},
	{EINT_RMIF_CTRL1,  12,  3, EBITS_MIF_CMD_INTR_LEN, "reg_cmd_intr_len"},
	{EINT_RMIF_CTRL1,  10,  2, EBITS_MIF_SWAP_64, "reg_cmd_req_size"},
	{EINT_RMIF_CTRL1,  8,  2, EBITS_MIF_BRST_LEN, "reg_burst_len"},
	{EINT_RMIF_CTRL1,  7,  1, EBITS_MIF_SWAP_64, "reg_swap_64bit"},
	{EINT_RMIF_CTRL1,  6,  1, EBITS_MIF_LITTLE_ENDIAN, "reg_little_endian"},
	{EINT_RMIF_CTRL1,  5,  1, EBITS_MIF_Y_REV, "reg_y_rev"},
	{EINT_RMIF_CTRL1,  4,  1, EBITS_MIF_X_REV, "reg_x_rev"},
	{EINT_RMIF_CTRL1,  0,  3, EBITS_MIF_SRC_FMT, "pack_mode"},

	{EINT_RMIF_CTRL2,  30,  2, EBITS_MIF_SW_RST, "reg_sw_rst"},
	{EINT_RMIF_CTRL2,  22,  4, EBITS_MIF_VSTEP, "reg_vstep"},
	{EINT_RMIF_CTRL2,  20,  2, EBITS_MIF_INT_CLR, "reg_int_clr"},
	{EINT_RMIF_CTRL2,  18,  2, EBITS_MIF_GCLK_CTRL, "reg_gclk_ctrl"},
	{EINT_RMIF_CTRL2,  0,  17, EBITS_MIF_URGENT_CTRL, "reg_urgent_ctrl"},

	{EINT_RMIF_CTRL3,  16,  16, EBITS_MIF_MEM_MODE, "mem_mode"},
	{EINT_RMIF_CTRL3,  0,  16, EBITS_MIF_LINEAR_LTH, "linear_length"},

	{EINT_RMIF_SCOPE_X,  0,  16, EBITS_MIF_X_START, "x_start"},
	{EINT_RMIF_SCOPE_X,  16,  16, EBITS_MIF_X_END, "x_end"},
	{EINT_RMIF_SCOPE_Y,  0,  16, EBITS_MIF_Y_START, "y_start"},
	{EINT_RMIF_SCOPE_Y,  16,  16, EBITS_MIF_Y_END, "y_end"},

	{TABLE_FLG_END, TABLE_FLG_END, 0xff, 0xff, "end"}
};

static const struct reg_t rtab_t5_dcntr_bits_tab[] = {
	/*--------------------------*/
	{INTRP_PARAM, 21, 5, 0, "INTRP_PARAM",
		"intep_phs_x_rtl",
		"xphase used, could be negative"},
	{INTRP_PARAM, 16, 5, 0, "",
		"intep_phs_x_use",
		"xphase used, could be negative"},
	{INTRP_PARAM, 5, 5, 0, "",
		"intep_phs_y_rtl",
		"yphase used, could be negative"},
	{INTRP_PARAM, 0, 5, 0, "",
		"intep_phs_y_use",
		"yphase used, could be negative"},
	/***********************************************/
	{DCTR_DIVR4, 28, 3, 1, "DCTR_DIVR4",
		"divrsmap_blk0_sft",
		"ds block 0"},
	{DCTR_DIVR4, 24, 3, 2, "",
		"divrsmap_blk1_sft",
		"ds block 0"},
	{DCTR_DIVR4, 20, 3, 3, "",
		"divrsmap_blk2_sft",
		"ds block 0"},
	/***********************************************/
	{DCTR_SIGFIL, 16, 8, 64, "DCTR_SIGFIL",
		"reg_sig_thr",
		"of sigma filtering"},
	{DCTR_SIGFIL, 8, 7, 2, "",
		"reg_sig_win_h",
		"window of sigma filtering"},
	{DCTR_SIGFIL, 4, 4, 1, "",
		"reg_sig_win_v",
		"window of sigma filtering"},
	{DCTR_SIGFIL, 2, 2, 0, "",
		"reg_sig_ds_r_x",
		"ratio for AVG"},
	{DCTR_SIGFIL, 0, 2, 0, "",
		"reg_sig_ds_r_y",
		"ratio for AVG"},
	/***********************************************/
	{DCTR_PATH, 25, 1, 0, "DCTR_PATH",
		"reg_grd_path",
		"0:DS input, 1: Ori input"},
	{DCTR_PATH, 16, 2, 2, "",
		"reg_in_ds_rate_x",
		"real rate is 2^reg_in_ds_rate"},
	{DCTR_PATH, 0, 2, 2, "",
		"reg_in_ds_rate_y",
		"real rate is 2^reg_in_ds_rate"},
	/***********************************************/
	{DCTR_BGRID_PARAM2, 24, 8, 48, "DCTR_BGRID_PARAM2",
		"reg_grd_xsize",
		""},
	{DCTR_BGRID_PARAM2, 16, 8, 48, "",
		"reg_grd_ysize",
		""},
	{DCTR_BGRID_PARAM2, 8, 8, 48, "",
		"reg_grd_valsz",
		""},
	{DCTR_BGRID_PARAM2, 0, 8, 22, "",
		"reg_grd_vnum",
		""},
	/***********************************************/
	{DCTR_BGRID_PARAM3, 16, 10, 80, "DCTR_BGRID_PARAM3",
		"reg_grd_xnum_use",
		""},
	{DCTR_BGRID_PARAM3, 0, 10, 45, "",
		"reg_grd_ynum_use",
		""},
	/***********************************************/
	{DCTR_BGRID_PARAM4, 16, 8, 12, "DCTR_BGRID_PARAM4",
		"reg_grd_xsize_ds",
		""},
	{DCTR_BGRID_PARAM4, 0, 8, 12, "",
		"reg_grd_ysize_ds",
		""},
	/***********************************************/
	{DCTR_BGRID_PARAM5, 0, 17, 0, "DCTR_BGRID_PARAM5",
		"reg_grd_xidx_div",
		""},
	/***********************************************/
	{DCTR_BGRID_PARAM6, 0, 17, 0, "DCTR_BGRID_PARAM5",
		"reg_grd_yidx_div",
		""},
	/***********************************************/
	{TABLE_FLG_END, 0, 0, 0, "end", "end", ""},

};

static const struct reg_t rtab_t3_dcntr_bits_tab[] = {
	/*--------------------------*/
	{INTRP_T3_PARAM, 21, 5, 0, "INTRP_PARAM",
		"intep_phs_x_rtl",
		"xphase used, could be negative"},
	{INTRP_T3_PARAM, 16, 5, 0, "",
		"intep_phs_x_use",
		"xphase used, could be negative"},
	{INTRP_T3_PARAM, 5, 5, 0, "",
		"intep_phs_y_rtl",
		"yphase used, could be negative"},
	{INTRP_T3_PARAM, 0, 5, 0, "",
		"intep_phs_y_use",
		"yphase used, could be negative"},
	/***********************************************/
	{DCTR_T3_DIVR4, 28, 3, 1, "DCTR_DIVR4",
		"divrsmap_blk0_sft",
		"ds block 0"},
	{DCTR_T3_DIVR4, 24, 3, 2, "",
		"divrsmap_blk1_sft",
		"ds block 0"},
	{DCTR_T3_DIVR4, 20, 3, 3, "",
		"divrsmap_blk2_sft",
		"ds block 0"},
	/***********************************************/
	{DCTR_T3_SIGFIL, 16, 8, 64, "DCTR_SIGFIL",
		"reg_sig_thr",
		"of sigma filtering"},
	{DCTR_T3_SIGFIL, 8, 7, 2, "",
		"reg_sig_win_h",
		"window of sigma filtering"},
	{DCTR_T3_SIGFIL, 4, 4, 1, "",
		"reg_sig_win_v",
		"window of sigma filtering"},
	{DCTR_T3_SIGFIL, 2, 2, 0, "",
		"reg_sig_ds_r_x",
		"ratio for AVG"},
	{DCTR_T3_SIGFIL, 0, 2, 0, "",
		"reg_sig_ds_r_y",
		"ratio for AVG"},
	/***********************************************/
	{DCTR_T3_PATH, 25, 1, 0, "DCTR_PATH",
		"reg_grd_path",
		"0:DS input, 1: Ori input"},
	{DCTR_T3_PATH, 16, 2, 2, "",
		"reg_in_ds_rate_x",
		"real rate is 2^reg_in_ds_rate"},
	{DCTR_T3_PATH, 0, 2, 2, "",
		"reg_in_ds_rate_y",
		"real rate is 2^reg_in_ds_rate"},
	/***********************************************/
	{DCTR_T3_BGRID_PARAM2, 24, 8, 48, "DCTR_BGRID_PARAM2",
		"reg_grd_xsize",
		""},
	{DCTR_T3_BGRID_PARAM2, 16, 8, 48, "",
		"reg_grd_ysize",
		""},
	{DCTR_T3_BGRID_PARAM2, 8, 8, 48, "",
		"reg_grd_valsz",
		""},
	{DCTR_T3_BGRID_PARAM2, 0, 8, 22, "",
		"reg_grd_vnum",
		""},
	/***********************************************/
	{DCTR_T3_BGRID_PARAM3, 16, 10, 80, "DCTR_BGRID_PARAM3",
		"reg_grd_xnum_use",
		""},
	{DCTR_T3_BGRID_PARAM3, 0, 10, 45, "",
		"reg_grd_ynum_use",
		""},
	/***********************************************/
	{DCTR_T3_BGRID_PARAM4, 16, 8, 12, "DCTR_BGRID_PARAM4",
		"reg_grd_xsize_ds",
		""},
	{DCTR_T3_BGRID_PARAM4, 0, 8, 12, "",
		"reg_grd_ysize_ds",
		""},
	/***********************************************/
	{DCTR_T3_BGRID_PARAM5, 0, 17, 0, "DCTR_BGRID_PARAM5",
		"reg_grd_xidx_div",
		""},
	/***********************************************/
	{DCTR_T3_BGRID_PARAM6, 0, 17, 0, "DCTR_BGRID_PARAM5",
		"reg_grd_yidx_div",
		""},
	/***********************************************/
	{TABLE_FLG_END, 0, 0, 0, "end", "end", ""},

};

static unsigned int get_mif_addr(unsigned int mif_index,
				 enum E1INT_RMIF_REG reg_index)
{
	unsigned int reg_addr = DCNTR_DIVR_RMIF_CTRL4;
	const unsigned int *reg;

	if (!di_dcnt.flg_int || mif_index >= DCNTR_NUB_MIF) {
		PR_ERR("%s:%d:%d\n", __func__, di_dcnt.flg_int, mif_index);
		return reg_addr;
	}

	reg = di_dcnt.reg_mif_tab[mif_index];

	return reg[reg_index];
}

static void dcntr_post_rdmif(int mif_index, //0:divr  1:grid  2:yflt  3:cflt
			     struct dcntr_core_s *pcfg)
{
	const struct reg_acc *op = &di_pre_regset;
	int mem_mode;  //0:linear address mode  1:canvas mode
	int canvas_id;
	int src_fmt; // 0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit
	int mif_x_start;
	int mif_x_end;
	int mif_y_start;
	int mif_y_end;
	int mif_reverse; // 0 : no reverse
	int vstep; //
	int linear_baddr;
	int linear_length;
	struct dcntr_mif_s *rdcfg;
	unsigned int burst;
	const unsigned int *reg;
	unsigned int off = 0; //for t3

	if (!pcfg->flg_int || mif_index >= DCNTR_NUB_MIF)
		return;
	if (DIM_IS_IC_EF(T3))
		off = 0x200;
	rdcfg = &pcfg->pstr[mif_index];
	reg = pcfg->reg_mif_tab[mif_index];

	mem_mode	= rdcfg->mem_mode;
	//0:linear address mode  1:canvas mode
	canvas_id	= rdcfg->canvas_id;
	src_fmt		= rdcfg->src_fmt;
	// 0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit
	mif_x_start	= rdcfg->mif_x_start;
	mif_x_end	= rdcfg->mif_x_end;
	mif_y_start	= rdcfg->mif_y_start;
	mif_y_end	= rdcfg->mif_y_end;
	mif_reverse	= rdcfg->mif_reverse; // 0 : no reverse
	vstep		= rdcfg->vstep; //
	linear_baddr	= rdcfg->linear_baddr;
	linear_length	= rdcfg->linear_length;
	burst		= rdcfg->burst;

	op->wr(reg[EINT_RMIF_CTRL1],
		(0 << 24) | //reg_sync_sel      <= am_spdat[25:24];
		(canvas_id << 16) | //reg_canvas_id     <= am_spdat[23:16];
		(1 << 12) | //reg_cmd_intr_len  <= am_spdat[14:12];
		(1 << 10) | //reg_cmd_req_size  <= am_spdat[11:10];
		(burst << 8) | //reg_burst_len     <= am_spdat[9:8];
		(rdcfg->sw_64bit << 7) | //reg_swap_64bit    <= am_spdat[7];
		(rdcfg->little   << 6) | //reg_little_endian <= am_spdat[6];
		(0 << 5) | //reg_y_rev         <= am_spdat[5];
		(0 << 4) | //reg_x_rev         <= am_spdat[4];
		(src_fmt << 0));//reg_pack_mode     <= am_spdat[2:0];
	op->wr(reg[EINT_RMIF_CTRL2],
		(0 << 30) | //reg_sw_rst      <= am_spdat[31:30];
		(vstep << 22) | //reg_vstep       <= am_spdat[25:22];
		(0 << 20) | //reg_int_clr     <= am_spdat[21:20];
		(0 << 18) | //reg_gclk_ctrl   <= am_spdat[19:18];
		(0 << 0));//reg_urgent_ctrl <= am_spdat[16:0];
	op->wr(reg[EINT_RMIF_CTRL3], ((mem_mode == 0) << 16) | linear_length);
	op->wr(reg[EINT_RMIF_CTRL4], linear_baddr);
	op->wr(reg[EINT_RMIF_SCOPE_X], (mif_x_end << 16) | mif_x_start);
	op->wr(reg[EINT_RMIF_SCOPE_Y], (mif_y_end << 16) | mif_y_start);

	dim_print("%s:0x%x->0x%x\n", "post", reg[EINT_RMIF_CTRL1],
		  op->rd(reg[EINT_RMIF_CTRL1]));
	dim_print("rd:0x%x,0x%x\n", DCNTR_GRID_RMIF_CTRL2 + off,
		  op->rd(DCNTR_GRID_RMIF_CTRL2 + off));
}

static void dt_set_change(void)
{
	unsigned int grd_num, reg_grd_xnum, reg_grd_ynum, grd_xsize, grd_ysize;
	int i;
	struct dcntr_mif_s *pmif;
	const struct reg_acc *op = &di_pre_regset;
	struct dcntr_core_s *pcfg = &di_dcnt;
	unsigned int off = 0; //for t3

	/*************************/
	if (DIM_IS_IC_EF(T3))
		off = 0x200;
	grd_num = op->rd(DCTR_BGRID_PARAM3 + off); //grd_num_use
	reg_grd_xnum = (grd_num >> 16) & (0x3ff);
	reg_grd_ynum = grd_num       & (0x3ff);

	grd_xsize = reg_grd_xnum << 3;
	grd_ysize = reg_grd_ynum;

	for (i = 0; i < 4; i++)
		memcpy(&pcfg->pstr[i], &pstr_default, sizeof(pcfg->pstr[i]));

	/* mif 0:divr */
	pmif = &pcfg->pstr[0];
	if (pcfg->in.use_cvs) {
		pmif->mem_mode = 1;
		pmif->canvas_id = pcfg->cvs_y;
	} else {
		//if(DIM_IS_IC_EF(T7))
		//	pmif->linear_baddr = pcfg->in.addr[0] >> 4;
		//else
		pmif->linear_baddr = pcfg->in.addr[0];
	}
	pmif->linear_length  = pcfg->in.len[0];
	pmif->sw_64bit	= pcfg->in_cfg.yds_swap_64bit;
	pmif->little	= pcfg->in_cfg.yds_little_endian;
	pmif->src_fmt = 1;
	pmif->mif_x_end = ((pcfg->in.x_size) >> pcfg->in.DS_RATIO) - 1;
	pmif->mif_y_end = ((pcfg->in.y_size) >> pcfg->in.DS_RATIO) - 1;
	pmif->burst	= pcfg->burst;
	if (pcfg->in.divrsmap_blk0_sft == 0)
		pmif->vstep = 1;
	else if (pcfg->in.divrsmap_blk0_sft == 1)
		pmif->vstep = 2;
	else
		pmif->vstep = 4;

	/* mif 1:grid */
	pmif = &pcfg->pstr[1];

	pmif->src_fmt = 5;
	pmif->mif_x_end = grd_xsize - 1;//pcfg->in.grd_x - 1;//
	pmif->mif_y_end = grd_ysize - 1;//pcfg->in.grd_y - 1;//
	//if(DIM_IS_IC_EF(T7))
	//	pmif->linear_baddr = pcfg->in.addr[1] >> 4;
	//else
	pmif->linear_baddr = pcfg->in.addr[1];
	pmif->linear_length  = grd_xsize;
	pcfg->in.len[1] = grd_xsize;
	pmif->sw_64bit	= pcfg->in_cfg.grd_swap_64bit;
	pmif->little	= pcfg->in_cfg.grd_little_endian;
	pmif->burst	= 2;
	if (pcfg->in.grid_use_fix) {
		pmif->mif_x_end = 81 * 8 - 1;
		pmif->mif_y_end = 46 - 1;
		pmif->linear_length = 81 * 8;
		pcfg->in.len[1] = pmif->linear_length;
	}
	/* mif 2:yflt */
	pmif = &pcfg->pstr[2];
	if (pcfg->in.use_cvs) {
		pmif->mem_mode = 1;
		pmif->canvas_id = pcfg->cvs_y;
	} else {
	//	if(DIM_IS_IC_EF(T7))
	//		pmif->linear_baddr = pcfg->in.addr[2] >> 4;
	//	else
		pmif->linear_baddr = pcfg->in.addr[2];
	}
	pmif->linear_length  = pcfg->in.len[2];
	pmif->sw_64bit	= pcfg->in_cfg.yds_swap_64bit;
	pmif->little	= pcfg->in_cfg.yds_little_endian;
	pmif->src_fmt = 1;
	pmif->mif_x_end = ((pcfg->in.x_size) >> pcfg->in.DS_RATIO) - 1;
	pmif->mif_y_end = ((pcfg->in.y_size) >> pcfg->in.DS_RATIO) - 1;
	pmif->burst	= pcfg->burst;

	/* mif 3:cflt */
	pmif = &pcfg->pstr[3];
	if (pcfg->in.use_cvs) {
		pmif->mem_mode = 1;
		pmif->canvas_id = pcfg->cvs_uv;
	} else {
	//	if(DIM_IS_IC_EF(T7))
	//		pmif->linear_baddr = pcfg->in.addr[3] >> 4;
	//	else
		pmif->linear_baddr = pcfg->in.addr[3];
	}
	pmif->linear_length  = pcfg->in.len[3];
	pmif->sw_64bit	= pcfg->in_cfg.cds_swap_64bit;
	pmif->little	= pcfg->in_cfg.cds_little_endian;
	pmif->src_fmt = 2;
	pmif->mif_x_end = ((pcfg->in.x_size) >> (pcfg->in.DS_RATIO + 1)) - 1;
	pmif->mif_y_end = ((pcfg->in.y_size) >> (pcfg->in.DS_RATIO + 1)) - 1;
	pmif->burst	= 2;

	for (i = 0; i < 4; i++) {
		dcntr_post_rdmif(i, pcfg);
		dbg_dct_mif(&pcfg->pstr[i]);
	}

	dbg_dct_core_other(pcfg);
}

//initial decontour post - core
static void dcntr_post(void)
{
	struct dcntr_core_s *pcfg = &di_dcnt;
	const struct reg_acc *op = &di_pre_regset;
	int hsize;
	int vsize;
	int grd_num_mode;
	int sig_path;	//0;
	//int reg_map_path; //ary add set same as sig_path
	//int reg_sig_path //ary add
	int in_ds_mode;	//0;

	int xsize;
	int ysize;

	int in_ds_rate;

	int in_ds_r_x;
	int in_ds_r_y;
	int intep_phs_x_use, intep_phs_y_use, intep_phs_x_rtl, intep_phs_y_rtl;
	int phs_x = 0;
	int phs_y = 0;

	int divrsmap_blk0_sft;
	int divrsmap_blk1_sft;
	int divrsmap_blk2_sft;

	int sig_ds_r;
	/* define for slicing */
	int reg_in_ds_rate_x	= 0; //ary add default setting
	int reg_in_ds_rate_y	= 0;//ary add default setting
	int ds_r_sft_x;
	int ds_r_sft_y;

	int xds;
	int yds;

	unsigned int grd_num;
	unsigned int reg_grd_xnum;
	unsigned int reg_grd_ynum;

	unsigned int reg_grd_xsize_ds = 0;	//ary add default setting
	unsigned int reg_grd_ysize_ds = 0;	//ary add default setting
	unsigned int reg_grd_xnum_use;
	unsigned int reg_grd_ynum_use;
	unsigned int reg_grd_xsize;
	unsigned int reg_grd_ysize;
	unsigned int tmp;
	int grd_path;
	/* ary ?? */
	int32_t reg_grd_xidx_div;
	int32_t reg_grd_yidx_div;
	int i;
	unsigned int off = 0; //for t3

	/****************************************/
	if (!pcfg->flg_int || !pcfg->n_set)
		return;
	op->bwr(VIUB_GCLK_CTRL3, 0x3f, 16, 6);
	op->bwr(DI_PRE_CTRL, 1, 15, 1);// decontour enable

	if (DIM_IS_IC_EF(T3))
		off = 0x200;
	/* int */
	hsize		= pcfg->in.x_size;
	vsize		= pcfg->in.y_size;
	grd_num_mode	= pcfg->in.grd_num_mode;
	sig_path	= pcfg->in.sig_path;	//0;
	//no use reg_map_path	= sig_path; //ary add
	in_ds_mode	= pcfg->in.in_ds_mode;	//0;

	xsize = hsize;
	ysize = vsize;

	#ifdef HIS_CODE
	in_ds_rate = (hsize > 1920) ? 2 :
			(hsize > 960) ? 1 : 0;
	#else
	in_ds_rate = pcfg->in.DS_RATIO;
	#endif

	in_ds_r_x = 1 << in_ds_rate;
	in_ds_r_y = 1 << in_ds_rate;
	/**************************************************/

	//interp phase calc
	if (sig_path == 0) {//DS mode
		if (in_ds_mode == 0)  {
			intep_phs_x_use = -(2 * in_ds_r_x - 2);
			intep_phs_y_use = -(2 * in_ds_r_y - 2);
			//AVG DS, need phase align
		} else {
			intep_phs_x_use = -(phs_x * 4);
			intep_phs_y_use = -(phs_y * 4);
		}  //Decimation DS, start point is point 0. or other scale
	// SW also can changed to other phases
	// according to Dos's down-sample phase setting
	} else {//non-DS mode
		intep_phs_x_use = 0;
		intep_phs_y_use = 0;
	}

	if (in_ds_r_x == 2)
		intep_phs_x_rtl = 2 * intep_phs_x_use;
	else
		intep_phs_x_rtl = intep_phs_x_use;

	if (in_ds_r_y == 2)
		intep_phs_y_rtl = 2 * intep_phs_y_use;
	else
		intep_phs_y_rtl = intep_phs_y_use;

	op->wr(INTRP_PARAM + off,
		((intep_phs_x_rtl & 0x1f) << 21)	|
		((intep_phs_x_use & 0x1f) << 16)	|
		((intep_phs_y_rtl & 0x1f) << 5)		|
		((intep_phs_y_use & 0x1f) << 0)
	);

	#ifdef TEST_ONLY
	int divrsmap_blk0_sft = 1;
	int divrsmap_blk1_sft = divrsmap_blk0_sft + 1;
	int divrsmap_blk2_sft = divrsmap_blk0_sft + 2;
	#else
	divrsmap_blk0_sft = pcfg->in.divrsmap_blk0_sft;
	divrsmap_blk1_sft = divrsmap_blk0_sft + 1;
	divrsmap_blk2_sft = divrsmap_blk0_sft + 2;
	#endif
	tmp = op->rd(DCTR_DIVR4 + off) & 0x0fffff;
	op->wr(DCTR_DIVR4 + off,
		(divrsmap_blk0_sft << 28) |
		(divrsmap_blk1_sft << 24) |
		(divrsmap_blk2_sft << 20) |
		tmp);

	#ifdef HIS_CODE
	sig_ds_r = (hsize > 1920) ? 0 :
			(hsize > 960) ? 1 : 2;
	#else
	sig_ds_r = (2 - pcfg->in.DS_RATIO);
	#endif
	op->wr(DCTR_SIGFIL + off,
	       (64 << 16) |
	       (2 << 8)		|
	       (1 << 4)		|
	       (sig_ds_r << 2)	|
	       (sig_ds_r)
	);

	//====================================================
	// slicing
	//====================================================
	#ifdef HIS_CODE
	reg_in_ds_rate_x = (hsize > 1920) ? 2 :
				(hsize > 960) ? 1 : 0;

	reg_in_ds_rate_y = (vsize > 1080) ? 2 :
				(vsize > 540) ? 1 : 0;
	#else
	reg_in_ds_rate_x = pcfg->in.DS_RATIO;
	reg_in_ds_rate_y = pcfg->in.DS_RATIO;
	#endif

	ds_r_sft_x	= reg_in_ds_rate_x;
	ds_r_sft_y	= reg_in_ds_rate_y;
	in_ds_r_x	= 1 << ds_r_sft_x;
	in_ds_r_y	= 1 << ds_r_sft_y;
	xds		= (hsize + in_ds_r_x - 1) >> ds_r_sft_x;
	yds		= (vsize + in_ds_r_y - 1) >> ds_r_sft_y;

	grd_num = op->rd(DCTR_BGRID_PARAM1 + off);
	//= (grd_num>>10) & 0x3ff;
	reg_grd_xnum = (grd_num_mode == 0) ? 40 :
			(grd_num_mode == 1) ? 60 : 80;
	//= grd_num       & 0x3ff;
	reg_grd_ynum = (grd_num_mode == 0) ? 23 :
			(grd_num_mode == 1) ? 34 : 45;

	grd_path   = (reg_in_ds_rate_x == 0) && (reg_in_ds_rate_y == 0);

	if (grd_path == 0) {//grid build ds mode
		reg_grd_xsize_ds = (xds + reg_grd_xnum - 1) / (reg_grd_xnum);
		reg_grd_ysize_ds = (yds + reg_grd_ynum - 1) / (reg_grd_ynum);
		reg_grd_xnum_use = ((xds - reg_grd_xsize_ds / 2) +
			reg_grd_xsize_ds - 1) / (reg_grd_xsize_ds) + 1;
		reg_grd_ynum_use = ((yds - reg_grd_ysize_ds / 2) +
			reg_grd_ysize_ds - 1) / (reg_grd_ysize_ds) + 1;
		reg_grd_xsize   = reg_grd_xsize_ds * in_ds_r_x;
		reg_grd_ysize   = reg_grd_ysize_ds * in_ds_r_y;
	} else {
		//grid build non-ds mode
		reg_grd_xsize   = (xsize + reg_grd_xnum - 1) / (reg_grd_xnum);
		reg_grd_ysize   = (ysize + reg_grd_ynum - 1) / (reg_grd_ynum);
		reg_grd_xnum_use = ((xsize - reg_grd_xsize / 2) +
				    reg_grd_xsize - 1) / (reg_grd_xsize) + 1;
		reg_grd_ynum_use = ((ysize - reg_grd_ysize / 2) +
				    reg_grd_ysize - 1) / (reg_grd_ysize) + 1;
	}

	//Wr(DCTR_PATH,(grd_path<<25));
	op->wr(DCTR_PATH + off,
	       (grd_path << 25) |
	       (3  << 29) | /* reg_decontour_enable_0 */
	       (3  << 27) | /* reg_decontour_enable_1 */
	       (10 << 19) |
	       /* reg_bit_in : 10bit or 12bit source input bit-width */
	       (1  << 18) | /* reg_dc_en : general enable bit*/
	       (8  << 3) |  /* */
	       (0  << 2) |
	       (reg_in_ds_rate_x << 16) |
	       (reg_in_ds_rate_y)
	);
	op->wr(DCTR_BGRID_PARAM2 + off,
			(reg_grd_xsize << 24) |
			(reg_grd_ysize << 16) |
			(48            << 8) | //valsz
			(22));//vnum

	op->wr(DCTR_BGRID_PARAM3 + off,
			(reg_grd_xnum_use << 16)	|
			(reg_grd_ynum_use));

	op->wr(DCTR_BGRID_PARAM4 + off,
			(reg_grd_xsize_ds << 16)	|
			(reg_grd_ysize_ds));

	/*ary:??*/
	//17bits = 7bits for original implement + 10 bit for precision
	reg_grd_xidx_div = (1 << 17) / (reg_grd_xsize);
	//17bits = 7bits for original implement + 10 bit for precision
	reg_grd_yidx_div = (1 << 17) / (reg_grd_ysize);

	op->wr(DCTR_BGRID_PARAM5 + off, reg_grd_xidx_div);
	op->wr(DCTR_BGRID_PARAM6 + off, reg_grd_yidx_div);

	for (i = 0; i < DCNTR_NUB_PARA8; i += 2) {
		tmp = (pcfg->grd_vbin[i] << 16) | pcfg->grd_vbin[i + 1];
		op->wr(DCTR_BGRID_PARAM8_0 + (i >> 1) + off, tmp);
	}
	dbg_dct_core_other(pcfg);
	dt_set_change();

	// cflt 420 to 444
	op->wr(DCNTR_POST_FMT_CTRL + off,
	       (0 << 31) |    //reg_cfmt_gclk_bit_dis	<= pwdata[31];
	       (0 << 30) |    //reg_cfmt_soft_rst_bit	<= pwdata[30];
	       (0 << 28) |    //reg_chfmt_rpt_pix  <= pwdata[28];
	       (0 << 24) |    //reg_chfmt_ini_phase  <= pwdata[27:24];
	       (0 << 23) |    //reg_chfmt_rpt_p0_en <= pwdata[23];
	       (1 << 21) |    //reg_chfmt_yc_ratio  <= pwdata[22:21];
	       (1 << 20) |    //reg_chfmt_en	     <= pwdata[20];
	       (0 << 19) |    //reg_cvfmt_phase0_always_en  <= pwdata[19];
	       (0 << 18) |    //reg_cvfmt_rpt_last_dis	<= pwdata[18];
	       (1 << 17) |    //reg_cvfmt_phase0_nrpt_en <= pwdata[17];
	       (0 << 16) |    //reg_cvfmt_rpt_line0_en  <= pwdata[16];
	       (0 << 12) |    //reg_cvfmt_skip_line_num <= pwdata[15:12];
	       (0 << 8) |    //reg_cvfmt_ini_phase   <= pwdata[11:8];
	       (8 << 1) |    //reg_cvfmt_phase_step  <= pwdata[7:1];
	       (1 << 0));   //reg_cvfmt_en	       <= pwdata[0];
	op->wr(DCNTR_POST_FMT_W + off,
	       ((xsize >> pcfg->in.DS_RATIO) << 16)	|
		(xsize >> (pcfg->in.DS_RATIO + 1)));
	op->wr(DCNTR_POST_FMT_H + off,
	       ((ysize >> pcfg->in.DS_RATIO) << 16)	|
		(ysize >> (pcfg->in.DS_RATIO + 1)));
	pcfg->st_set	= 1;
	//pcfg->st_pause	= 1;
	pcfg->n_set	= 0;
	dim_print("rd:0x%x,0x%x\n", DCNTR_GRID_RMIF_CTRL2 + off,
		  op->rd(DCNTR_GRID_RMIF_CTRL2 + off));
}

static void dcntr_update(void)
{
	const struct reg_acc *op = &di_pre_regset;
	int i;
//	const unsigned int *reg;
	unsigned int reg_add;
	struct dcntr_core_s *pcfg = &di_dcnt;
	unsigned int off = 0; //for t3

	if (!pcfg->st_set || !pcfg->n_up)
		return;
	if (DIM_IS_IC_EF(T3))
		off = 0x200;
	if (pcfg->in.use_cvs) {
		reg_add = get_mif_addr(ECNTR_MIF_IDX_GRID, EINT_RMIF_CTRL4);
		dim_print("%s:reg:0x%x\n", __func__, reg_add);
		op->wr(reg_add, pcfg->in.addr[ECNTR_MIF_IDX_GRID]);
	} else {
		for (i = 0; i < 4; i++) {
			reg_add = get_mif_addr(i, EINT_RMIF_CTRL4);
			op->wr(reg_add, pcfg->in.addr[i]);
		}
	}

	dim_print("rd:0x%x,0x%x\n",
		  DCNTR_GRID_RMIF_CTRL2 + off, op->rd(DCNTR_GRID_RMIF_CTRL2 + off));

	if ((dbg_dct & DI_BIT0) == 0) {
		if ((dbg_dct & DI_BIT2) == 0)
			di_pq_db_setting(DIM_DB_SV_DCT_BL2);

		op->bwr(DI_PRE_CTRL, 1, 15, 1);// decontour enable
	}
	pcfg->n_up = 0;
}

void dcntr_dynamic_setting(struct dim_rpt_s *rpt)
{
	u64 map_0;
	u64 map_1;
	u64 map_2;
	u64 map_3;
	u64 map_15;
	u64 map_count;
	unsigned int bld_2;
	unsigned int val_db;
	unsigned int pdate[2];
	unsigned int alpha;
	unsigned int thr = 60; /* map_count default 60*/
	unsigned int target = 256; /*max 256*/
	struct db_save_s *dbp;
	const struct reg_acc *op = &di_pre_regset;
	unsigned int off = 0; //for t3

	if (!rpt || dcntr_dynamic_disable()) {
		dbg_pq("%s rpt is null or suspend dcntr dynamic.\n", __func__);
		return;
	}
	if (DIM_IS_IC_EF(T3))
		off = 0x200;
	/*get val form db*/
	dbp = &get_datal()->db_save[DIM_DB_SV_DCT_BL2];
	if (!dbp) {
		dbg_pq("val form db failed, default set 0.\n");
		val_db = 0;
	} else {
		/*bits[16-24] is bld value*/
		val_db = (dbp->val_db & dbp->mask) >> 16;
		dbg_pq("val:%d form db.\n", val_db);
	}
	/*debug alpha dtc bit6:1, bit7:9*/
	if (dcntr_dynamic_alpha_1())
		alpha = 1;
	else if (dcntr_dynamic_alpha_2())
		alpha = 9;
	else
		alpha = 3;

	map_0 = rpt->dct_map_0;
	map_1 = rpt->dct_map_1;
	map_2 = rpt->dct_map_2;
	map_3 = rpt->dct_map_3;
	map_15 = rpt->dct_map_15;
	bld_2 = rpt->dct_bld_2;
	map_count = (map_0 + map_1 + map_2 + map_3) * 10000;
	dbg_pq("bits[0x%x],mp0-3[%lld,%lld,%lld,%lld],mp15[%lld],count[%lld],bld[0x%x]\n",
		rpt->spt_bits,  map_0, map_1, map_2, map_3, map_15,
		map_count,  rpt->dct_bld_2 << 16);

	if (map_count < thr * map_15) {
		if (bld_2 == target)
			return;
		/*+7 is compensation for loss of accuracy*/
		bld_2 = alpha * target + (10 - alpha) * bld_2 + 7;
		pdate[0] = (bld_2 / 10);

		if (pdate[0] > target)
			pdate[0] = target;
		dbg_pq("case:1, pdate:%x\n", pdate[0]);
	} else {
		if (bld_2 == val_db)
			return;
		/*db value default 0, function: bld2 = db_val*a + (10-a)*bld_2 */
		bld_2 = val_db * alpha + (10 - alpha) * bld_2;
		pdate[0] = (bld_2 / 10);

		if (pdate[0] < val_db)
			pdate[0] = val_db;
		dbg_pq("case:0, pdate:%x\n", pdate[0]);
	}
	op->bwr(DCTR_BLENDING2 + off, pdate[0], 16, 9);
}

#ifdef DIM_DCT_BORDER_DETECT
static void dct_border_tune(const struct reg_acc *op, unsigned int off)
{
	bool have_border = false;
	bool simulation = false;
#ifdef	DIM_DCT_BORDER_DBG
	static bool last;
#endif
	if (dcntr_border_detect_disable())
		return;
#ifdef DIM_DCT_BORDER_SIMULATION
	simulation = true;
	if (dcntr_border_simulation_have())
		have_border = true;
#else
	if (aml_ldim_get_bbd_state() == 1)
		have_border = true;
#endif
	if (have_border)
		op->bwr(DCTR_PMEM_MAP1 + off, 1, 30, 1);
	else
		op->bwr(DCTR_PMEM_MAP1 + off, 0, 30, 1);
#ifdef	DIM_DCT_BORDER_DBG
	if (last != have_border) {
		PR_INF("%s:%d->%d\n", __func__, last, have_border);
		last = have_border;
		PR_INF("\t:reg:0x%x=0x%x:%d\n",
		       DCTR_PMEM_MAP1 + off, op->rd(DCTR_PMEM_MAP1 + off),
		       simulation);
	}
#endif
}

#endif /* DIM_DCT_BORDER_DETECT */

void dcntr_pq_tune(struct dim_rpt_s *rpt)
{
	const struct reg_acc *op = &di_pre_regset;
//	unsigned int tmp[3];
	struct dcntr_core_s *pcfg = &di_dcnt;
	unsigned int off = 0; //for t3

	if (!pcfg->n_rp)
		return;
	if (DIM_IS_IC_EF(T3))
		off = 0x200;
	rpt->spt_bits |= DI_BIT0;
	rpt->dct_map_0 = op->rd(DCTR_MAP_HIST_0 + off);
	rpt->dct_map_1 = op->rd(DCTR_MAP_HIST_1 + off);
	rpt->dct_map_2 = op->rd(DCTR_MAP_HIST_2 + off);
	rpt->dct_map_3 = op->rd(DCTR_MAP_HIST_3 + off);
	rpt->dct_map_15 = op->rd(DCTR_MAP_HIST_15 + off);
	rpt->dct_bld_2 = op->brd(DCTR_BLENDING2 + off, 16, 9);
	pcfg->n_rp = 0;
	dim_print("%s:0x%x\n", __func__, rpt->dct_map_0);

	dcntr_dynamic_setting(rpt);
#ifdef DIM_DCT_BORDER_DETECT
	dct_border_tune(op, off);
#endif
}

void dcntr_dis(void)
{
	const struct reg_acc *op = &di_pre_regset;
	struct dcntr_core_s *pcfg = &di_dcnt;

	if (pcfg->st_pause) {
		dim_print("%s\n", __func__);
		op->bwr(DI_PRE_CTRL, 0, 15, 1);// decontour enable
		pcfg->st_pause	= 0;
		pcfg->n_rp	= 1;
	}
	if (pcfg->p_in_cfg && get_datal()->dct_op) {
		get_datal()->dct_op->mem_put_free(pcfg->p_in_cfg);
		pcfg->p_in_cfg = NULL;
	}
}

void dcntr_set(void)
{
//	const struct reg_acc *op = &di_pre_regset;
	struct dcntr_core_s *pcfg = &di_dcnt;
	const struct reg_acc *op = &di_pre_regset;
	unsigned int dval;
	unsigned int off = 0; //for t3

	if (!pcfg->flg_int || pcfg->st_off)
		return;
	if (DIM_IS_IC_EF(T3))
		off = 0x200;
	if (pcfg->n_demo) {
		dval = op->rd(DCTR_DEMO + off) & 0xfffc0000;
		if (!pcfg->demo)
			op->bwr(DCTR_DEMO + off, 0, 16, 2);
		else if (pcfg->demo == 1)
			op->wr(DCTR_DEMO + off,
			       dval	|
			       (1	<< 16) |
			       (pcfg->in.x_size >> 1));
		else
			op->wr(DCTR_DEMO + off,
			       dval	|
			       (2	<< 16) |
			       (pcfg->in.x_size >> 1));
		pcfg->n_demo = 0;
	}
	if (pcfg->n_set) {
		dbg_dctp("%s:set\n", __func__);
		dcntr_post();
		pcfg->st_pause	= 1;
	} else if (pcfg->n_up) {
		dbg_dctp("%s:update\n", __func__);
		dcntr_update();
		pcfg->st_pause	= 1;
	}
}

void dim_dbg_dct_info(struct dcntr_mem_s *pprecfg)
{
	if (!pprecfg)
		return;
	dim_print("index[%d],free[%d]\n",
		  pprecfg->index, pprecfg->free);

	dim_print("use_org[%d],ration[%d]\n",
		  pprecfg->use_org, pprecfg->ds_ratio);
	dim_print("grd_addr[0x%lx],y_addr[0x%lx], c_addr[0x%lx]\n",
		  pprecfg->grd_addr,
		  pprecfg->yds_addr,
		  pprecfg->cds_addr);
	dim_print("grd_size[%d],yds_size[%d], cds_size[%d]\n",
		  pprecfg->grd_size,
		  pprecfg->yds_size,
		  pprecfg->cds_size);
	dim_print("out_fmt[0x%x],y_len[%d],c_len[%d]\n",
		  pprecfg->pre_out_fmt,
		  pprecfg->yflt_wrmif_length,
		  pprecfg->cflt_wrmif_length);
	dim_print("yswap_64 little[%d,%d],c:[%d,%d],grd:[%d,%d]\n",
		  pprecfg->yds_swap_64bit,
		  pprecfg->yds_little_endian,
		  pprecfg->cds_swap_64bit,
		  pprecfg->cds_little_endian,
		  pprecfg->grd_swap_64bit,
		  pprecfg->grd_little_endian);
	dim_print("yds_canvas_mode[%d],cds_canvas_mode[%d]\n",
		pprecfg->yds_canvas_mode,
		pprecfg->cds_canvas_mode);
	dim_print("ori_w[%d],ori_h[%d]\n",
		pprecfg->ori_w,
		pprecfg->ori_h);
}

struct linear_para_s {
	ulong y_addr;
	ulong c_addr;
	unsigned int	y_stride;
	unsigned int	c_stride;
	unsigned int	y_cvs_w;
	unsigned int	c_cvs_w;
	bool	reg_swap;
	bool	l_endian;
	bool	block_mode;
	bool	cbcr_swap;
};

static void linear_info_get(struct vframe_s *vfm, struct linear_para_s *opara)
{
	if (!vfm || !opara)
		return;
	if (!IS_NV21_12(vfm->type)) {
		PR_ERR("%s:0x%x\n", __func__, vfm->type);
		return;
	}
	opara->y_addr = vfm->canvas0_config[0].phy_addr;
	opara->c_addr = vfm->canvas0_config[1].phy_addr;
	opara->y_cvs_w	= vfm->canvas0_config[0].width;
	opara->c_cvs_w = vfm->canvas0_config[1].width;

	opara->y_stride = (opara->y_cvs_w + 15) >> 4;
	opara->c_stride = (opara->c_cvs_w + 15) >> 4;
	if ((vfm->flag & VFRAME_FLAG_VIDEO_LINEAR)) {
		opara->reg_swap = 0;
		opara->l_endian = 1;

	} else {
		opara->reg_swap = 1;
		opara->l_endian = 0;
	}

	if (vfm->type & VIDTYPE_VIU_NV12)
		opara->cbcr_swap = 1;
	else
		opara->cbcr_swap = 0;
}

void dcntr_check_bypass(struct vframe_s *vfm)
{
	struct dcntr_mem_s	*pdcn;

	pdcn = (struct dcntr_mem_s *)vfm->decontour_pre;

	if (pdcn && get_datal()->dct_op) {
		get_datal()->dct_op->mem_put_free(pdcn);
		dbg_dbg("%s:\n", __func__);
	}
}

void dcntr_check(struct vframe_s *vfm)
{
	struct dcntr_core_s	*pcfg = &di_dcnt;
	struct dcntr_mem_s	*pdcn;
	bool chg = false;
	unsigned int x, y, ds_x, ds_y, ratio;
	//unsigned int in_ds_mode;/*default is 0*/
	//unsigned int sig_path;/*default is 0*/
	//unsigned int grd_num_mode;
	unsigned int divrsmap_blk0_sft, yflt_wrmif_length, cflt_wrmif_length;
	unsigned int xy, demo;
	unsigned long ds_addy = 0, ds_addc = 0, grd_add = 0;
	unsigned int cvs_y, cvs_uv;
	struct di_cvs_s *cvss;
	unsigned int cvs_w;
	struct linear_para_s opara;
	bool check_burst = false;
	//pdcn = (struct dcntr_mem_s *)vfm->vf_ext;
	pdcn = (struct dcntr_mem_s *)vfm->decontour_pre;

	if (!pcfg->flg_int) {
		if (pdcn && get_datal()->dct_op)
			get_datal()->dct_op->mem_put_free(pdcn);

		return;
	}
	/*dbg*/
	if (dbg_dct & DI_BIT4)
		pcfg->in.grid_use_fix = 1;

	pcfg->n_set	= 0;
	pcfg->n_up	= 0;

	if (disable_di_decontour() && !pcfg->st_off) {
		pcfg->st_off = 1;
		PR_INF("dt:off\n");
	} else if (!disable_di_decontour() && pcfg->st_off) {
		pcfg->st_off = 0;
		PR_INF("dt:on\n");
	}
	if (!pdcn || pcfg->st_off) {
		if (pdcn && get_datal()->dct_op)
			get_datal()->dct_op->mem_put_free(pdcn);

		pcfg->p_in_cfg = NULL;
		return;
	}

	dim_dbg_dct_info(pdcn);
	memcpy(&pcfg->in_cfg, pdcn, sizeof(pcfg->in_cfg));

	pcfg->p_in_cfg = pdcn;

	if (IS_COMP_MODE(vfm->type)) {
		x = vfm->compWidth;
		y = vfm->compHeight;

	} else {
		x = pdcn->ori_w;//vfm->width;
		y = pdcn->ori_h;//vfm->height;
	}

	if (pdcn->use_org) {
		ds_x = pdcn->ori_w;//vfm->width;
		ds_y = pdcn->ori_h;//vfm->height;
		if (DIM_IS_IC(T5)) {
			pcfg->in.use_cvs = 1;
		} else if (cfgg(LINEAR)) {
			//change in_cfg:
			linear_info_get(vfm, &opara);
			pcfg->in_cfg.yds_addr = opara.y_addr;
			pcfg->in_cfg.cds_addr = opara.c_addr;
			pcfg->in_cfg.yflt_wrmif_length = opara.y_stride;
			pcfg->in_cfg.cflt_wrmif_length = opara.c_stride;
			pcfg->in_cfg.yds_swap_64bit = opara.reg_swap;
			pcfg->in_cfg.cds_swap_64bit = opara.reg_swap;
			pcfg->in_cfg.yds_little_endian = opara.l_endian;
			pcfg->in_cfg.cds_little_endian = opara.l_endian;
			pcfg->in.use_cvs = 0;
			ds_addy = opara.y_addr;
			ds_addc = opara.c_addr;
			check_burst = true;
		}
	} else {
		ds_x = pdcn->ori_w >> pdcn->ds_ratio;
		ds_y = pdcn->ori_h >> pdcn->ds_ratio;

		ds_addy = pdcn->yds_addr;
		ds_addc = pdcn->cds_addr;
		pcfg->in.use_cvs = 0;
	}

	if (IS_I_SRC(vfm->type)) {
		y	= (y >> 1);
		ds_y	= (ds_y >> 1);
	}

	grd_add = pdcn->grd_addr;

	if (pdcn->use_org) {
		if (ds_x == x)
			ratio = 0;
		else if (ds_x >= (x >> 1))
			ratio = 1;
		else if (ds_x >= (x >> 2))
			ratio = 2;
		else// if (ds_x >= (x>>3))
			ratio = 3;
		dim_print("%s:ratio1:%d", __func__, ratio);
	} else {
		ratio = pdcn->ds_ratio;
		dim_print("%s:ratio2:%d", __func__, ratio);
	}

	/* check ds_x */
	if (ds_x & DI_BIT0)
		pcfg->n_bypass = 1;
	else
		pcfg->n_bypass = 0;

	if (ds_x == x && ds_y == y)
		pcfg->in.sig_path = 1;

	if (pcfg->l_xsize != x ||
	    pcfg->l_ysize != y ||
	    pcfg->l_ds_x  != ds_x ||
	    pcfg->l_ds_y  != ds_y) {
		chg = true;
		pcfg->in.x_size = x;
		pcfg->in.y_size = y;
		pcfg->in.ds_x	= ds_x;
		pcfg->in.ds_y	= ds_y;
		pcfg->in.DS_RATIO = ratio;

		pcfg->l_xsize	= x;
		pcfg->l_ysize	= y;
		pcfg->l_ds_x	= ds_x;
		pcfg->l_ds_y	= ds_y;

		if (pcfg->in.use_cvs) {
			cvss = &get_datal()->cvs;
			cvs_y = cvss->post_idx[1][1]; //note: use by copy function
			cvs_uv = cvss->post_idx[1][5];
			pcfg->cvs_y = (unsigned char)cvs_y;
			pcfg->cvs_uv = (unsigned char)cvs_uv;
		}
	}

	demo = (dbg_dct & 0x3000) >> 12;
	if (pcfg->demo != demo || chg) {
		pcfg->demo = demo;
		pcfg->n_demo = 1;
	}
	if (chg) {
		xy = x * y;
		/* divr map */
		if (xy > (1920 * 1080))
			divrsmap_blk0_sft = 1;
		else if (xy > (960 * 540))
			divrsmap_blk0_sft = 1;
		else if (x > 480)
			divrsmap_blk0_sft = 1;
		else
			divrsmap_blk0_sft = 0;

		pcfg->in.divrsmap_blk0_sft = divrsmap_blk0_sft;
		yflt_wrmif_length = ((x >> ratio) * 8) >> 7;
		cflt_wrmif_length = ((x >> ratio) * 16) >> 7; //

		if (pcfg->in.use_cvs) {
			pcfg->in.len[ECNTR_MIF_IDX_YFLT] = yflt_wrmif_length;
			pcfg->in.len[ECNTR_MIF_IDX_CFLT] = cflt_wrmif_length;
		} else {
			pcfg->in.len[ECNTR_MIF_IDX_YFLT] =
				pcfg->in_cfg.yflt_wrmif_length;
			pcfg->in.len[ECNTR_MIF_IDX_CFLT] =
				pcfg->in_cfg.cflt_wrmif_length;
		}
		pcfg->in.len[ECNTR_MIF_IDX_DIVR] =
			pcfg->in.len[ECNTR_MIF_IDX_YFLT];
		pcfg->in.len[ECNTR_MIF_IDX_GRID] = 0;//not use this value

		dim_print("%s:y len:[%d:%d]\n", __func__,
			  yflt_wrmif_length, pcfg->in_cfg.yflt_wrmif_length);
		dim_print("%s:c len:[%d:%d]\n", __func__,
			  cflt_wrmif_length, pcfg->in_cfg.cflt_wrmif_length);
	}
	pcfg->burst = 2;
	if (pcfg->in.use_cvs || check_burst) {
		canvas_config_config((u32)pcfg->cvs_y, &vfm->canvas0_config[0]);
		canvas_config_config((u32)pcfg->cvs_uv,
				     &vfm->canvas0_config[1]);
		cvs_w = vfm->canvas0_config[0].width;
		if (cvs_w % 32)
			pcfg->burst = 0;
		else if (cvs_w % 64)
			pcfg->burst = 1;

		dim_print("%s:cvsy:add:0x%lx,c:0x%lx\n",
			  __func__,
			  (unsigned long)vfm->canvas0_config[0].phy_addr,
			  (unsigned long)vfm->canvas0_config[1].phy_addr);
	}

	if (!pcfg->in.use_cvs) {
		if (DIM_IS_IC_EF(T7)) {
			pcfg->in.addr[ECNTR_MIF_IDX_DIVR] =
				(unsigned int)(ds_addy >> 4);
			pcfg->in.addr[ECNTR_MIF_IDX_YFLT] =
				(unsigned int)(ds_addy >> 4);
			pcfg->in.addr[ECNTR_MIF_IDX_CFLT] =
				(unsigned int)(ds_addc >> 4);
		} else {
			pcfg->in.addr[ECNTR_MIF_IDX_DIVR] =
				(unsigned int)ds_addy;
			pcfg->in.addr[ECNTR_MIF_IDX_YFLT] =
				(unsigned int)ds_addy;
			pcfg->in.addr[ECNTR_MIF_IDX_CFLT] =
				(unsigned int)ds_addc;
		}
	}
	if (DIM_IS_IC_EF(T7))
		pcfg->in.addr[ECNTR_MIF_IDX_GRID] =
			(unsigned int)(grd_add >> 4);
	else
		pcfg->in.addr[ECNTR_MIF_IDX_GRID] =
			(unsigned int)grd_add;

	if (pcfg->n_bypass) {
		pcfg->n_set	= 0;
		pcfg->n_up	= 0;
	} else {
		if (chg || (dbg_dct & DI_BIT5))
			pcfg->n_set	= 1;
		else
			pcfg->n_up	= 1;
	}

	dbg_dct_in(&pcfg->in);
}

void dcntr_reg(unsigned int on)
{
	struct dcntr_core_s *pcfg;

	pcfg = &di_dcnt;

	if (!pcfg->flg_int)
		return;

	if (on) {
		memset(&pcfg->in, 0, sizeof(pcfg->in));
		pcfg->l_ds_x	= 0;
		pcfg->l_ds_y	= 0;
		pcfg->l_xsize	= 0;
		pcfg->l_ysize	= 0;

		pcfg->st_set	= 0;
		pcfg->st_pause	= 0;
		pcfg->n_set	= 0;
		pcfg->n_up	= 0;
		pcfg->n_rp	= 0;

		pcfg->in.grd_num_mode	= 2;
		pcfg->in.use_cvs	= 1;
	}
}

void dcntr_prob(void)
{
	struct dcntr_core_s *pcfg;
	int i;

	pcfg = &di_dcnt;

	dbg_reg("%s\n", __func__);
	memset(pcfg, 0, sizeof(*pcfg));

	if (DIM_IS_IC(T5) || DIM_IS_IC(T3))
		pcfg->support = 1;
	else
		pcfg->support = 0;

	if (!pcfg->support)
		return;

	for (i = 0; i < 22; i++)
		pcfg->grd_vbin[i] = 48 + 48 * i;
	for (i = 0; i < DCNTR_NUB_MIF; i++) {
		if (DIM_IS_IC_EF(T3))
			pcfg->reg_mif_tab[i] = &reg_mif_t3[i][0];
		else
			pcfg->reg_mif_tab[i] = &reg_mif[i][0];
	}

	pcfg->reg_mif_bits_tab	= &reg_bits_mif[0];
	if (DIM_IS_IC_EF(T3))
		pcfg->reg_contr_bits	= &rtab_t3_dcntr_bits_tab[0];
	else
		pcfg->reg_contr_bits	= &rtab_t5_dcntr_bits_tab[0];
	pcfg->flg_int = 1;
	PR_INF("%s:end\n", __func__);
}

int  dbg_dct_mif_show(struct seq_file *s, void *v)
{
	struct dcntr_core_s *pcfg;
	int i;

	pcfg = &di_dcnt;
	if (!pcfg->support || !pcfg->flg_int) {
		seq_printf(s, "%s:\n", "no dct");
		return 0;
	}
	for (i = 0; i < 4; i++) {
		seq_printf(s, "dump dct mif[%d]\n", i);
		dbg_regs_tab(s, pcfg->reg_mif_bits_tab, pcfg->reg_mif_tab[i]);
	}
	return 0;
}

int dbg_dct_core_show(struct seq_file *s, void *v)
{
	struct dcntr_core_s *pcore;
	struct db_save_s *dbp;

	pcore = &di_dcnt;

	seq_printf(s, "%s:\n", __func__);

	seq_printf(s, "\tlsize<%d,%d,%d,%d>:\n",
		   pcore->l_xsize, pcore->l_ysize,
		   pcore->l_ds_x, pcore->l_ds_y);
	seq_printf(s, "\tint[%d],sup[%d],st_set[%d],st_pause[%d]\n",
		   pcore->flg_int, pcore->support, pcore->st_set,
		   pcore->st_pause);
	seq_printf(s, "\tst_off[%d],n_set[%d],n_up[%d],cvs_y[%d],cvs_uv[%d]\n",
		   pcore->st_off, pcore->n_set, pcore->n_up,
		   pcore->cvs_y, pcore->cvs_uv);
	seq_printf(s, "\t:bypass:%d\n", pcore->n_bypass);

	seq_printf(s, "%s:\n", "dct_bl2");
	dbp = &get_datal()->db_save[DIM_DB_SV_DCT_BL2];
	seq_printf(s, "\t:spt:%d;update:%d;add[0x%x];mask[0x%x]\n",
		   dbp->support,
		   dbp->update,
		   dbp->addr,
		   dbp->mask);
	seq_printf(s, "\t:db:en:%d,val[0x%x],pq:%d,0x%x\n",
		   dbp->en_db,
		   dbp->val_db,
		   dbp->en_pq,
		   dbp->val_pq);
	return 0;
}

int dbg_dct_contr_show(struct seq_file *s, void *v)
{
	struct dcntr_core_s *pcfg;

	pcfg = &di_dcnt;
	if (!pcfg->support || !pcfg->flg_int) {
		seq_printf(s, "%s:\n", "no dct");
		return 0;
	}

	dbg_reg_tab(s, pcfg->reg_contr_bits);

	return 0;
}

