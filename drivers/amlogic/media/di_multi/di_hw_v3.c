// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/deinterlace/sc2/di_hw_v3.c
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
#include <linux/amlogic/tee.h>
#include "deinterlace.h"
#include "di_data_l.h"

//#include "sc2_ucode/cbus_register.h"
//#include "sc2_ucode/deint_new.h"

#include "di_reg_v3.h"
#include "di_hw_v3.h"
#include "di_reg_v2.h"

#include "register.h"
#include "register_nr4.h"

//#include "sc2_ucode/f2v.h"

#define stimulus_display PR_INF
#define stimulus_display2	PR_INF
#define stimulus_print		PR_INF

/* ary */
static u32 sc2_dbg;
module_param_named(sc2_dbg, sc2_dbg, uint, 0664);

/********************
 * BIT0: enable pre irq
 * BIT1: enable pst irq
 ********************/

static u32 sc2_dbg_cnt_pre;
module_param_named(sc2_dbg_cnt_pre, sc2_dbg_cnt_pre, uint, 0664);

static u32 sc2_dbg_cnt_pst;
module_param_named(sc2_dbg_cnt_pst, sc2_dbg_cnt_pst, uint, 0664);

void sc2_dbg_set(unsigned int val)
{
	sc2_dbg = val;
}

bool sc2_dbg_is_en_pre_irq(void)
{
	if (sc2_dbg & DI_BIT0)
		return true;

	return false;
}

void sc2_dbg_pre_info(unsigned int val)
{
	if ((sc2_dbg_cnt_pre % 32) == 0) {
		PR_ERR("%s:di not work %d:0x%x\n",
		       __func__, sc2_dbg_cnt_pre, val);
	}
	sc2_dbg_cnt_pre++;
}

void sc2_dbg_pst_info(unsigned int val)
{
	if ((sc2_dbg_cnt_pst % 32) == 0) {
		PR_ERR("%s:di not work %d:0x%x\n",
		       __func__, sc2_dbg_cnt_pst, val);
	}
	sc2_dbg_cnt_pst++;
}

bool sc2_dbg_is_en_pst_irq(void)
{
	if (sc2_dbg & DI_BIT1)
		return true;
	return false;
}

/*dbg setting: */
static u32 sc2_reg_mask;
module_param_named(sc2_reg_mask, sc2_reg_mask, uint, 0664);
/* */

bool is_mask(unsigned int cmd)
{
	bool ret = false;

	switch (cmd) {
	case SC2_REG_MSK_GEN_PRE:
		if (sc2_reg_mask & DI_BIT0)
			ret = true;
		break;
	case SC2_REG_MSK_GEN_PST:
		if (sc2_reg_mask & DI_BIT1)
			ret = true;
		break;
	case SC2_REG_MSK_nr:
		if (sc2_reg_mask & DI_BIT2)
			ret = true;
		break;
	case SC2_ROT_WR:
		if (sc2_reg_mask & DI_BIT3)
			ret = true;
		break;
	case SC2_ROT_PST:
		if (sc2_reg_mask & DI_BIT4)
			ret = true;
		break;
	case SC2_MEM_CPY:
		if (sc2_reg_mask & DI_BIT5)
			ret = true;
		break;
	case SC2_BYPASS_RESET:
		if (sc2_reg_mask & DI_BIT6)
			ret = true;
		break;
	case SC2_DISABLE_CHAN2:
		if (sc2_reg_mask & DI_BIT7)
			ret = true;
		break;
	/*bit 15->bit 8*/
	case SC2_DW_EN:
		if (sc2_reg_mask & DI_BIT15)
			ret = true;
		break;
	case SC2_DW_SHOW:
		if ((sc2_reg_mask & DI_BIT15) &&
		    (sc2_reg_mask & DI_BIT14))
			ret = true;
		break;
	case SC2_DW_SHRK_EN:
		if (sc2_reg_mask & DI_BIT13)
			ret = true;
		break;

	case SC2_POST_TRIG: /*bit 23:bit 16*/
		if (sc2_reg_mask & DI_BIT16)
			ret = true;
		break;
	case SC2_POST_TRIG_MSK1:
		if (sc2_reg_mask & DI_BIT17)
			ret = true;
		break;
	case SC2_POST_TRIG_MSK2:
		if (sc2_reg_mask & DI_BIT18)
			ret = true;
		break;
	case SC2_POST_TRIG_MSK3:
		if (sc2_reg_mask & DI_BIT19)
			ret = true;
		break;
	case SC2_POST_TRIG_MSK4:
		if (sc2_reg_mask & DI_BIT20)
			ret = true;
		break;
	case SC2_POST_TRIG_MSK5:
		if (sc2_reg_mask & DI_BIT21)
			ret = true;
		break;
	case SC2_POST_TRIG_MSK6:
		if (sc2_reg_mask & DI_BIT22)
			ret = true;
		break;
	case SC2_POST_TRIG_MSK7:
		if (sc2_reg_mask & DI_BIT23)
			ret = true;
		break;
	case SC2_LOG_POST_REG_OUT:
		if (sc2_reg_mask & DI_BIT24)
			ret = true;
		break;
	default:
		break;
	}
	return ret;
}

static unsigned int di_mif_add_get_offset_v3(enum DI_MIF0_ID mif_index);

/*********************************
 ******* linear address  *******
 ********************************/
/* DI_MIF0_t -> DI_MIF_S*/
static void di_mif0_stride(struct DI_MIF_S *mif,
			   unsigned int  *stride_y,
			   unsigned int  *stride_cb,
			   unsigned int  *stride_cr
	)
{
	unsigned int burst_stride_0;
	unsigned int burst_stride_1;
	unsigned int burst_stride_2;

	//if support scope,need change this to real hsize
	//unsigned int pic_hsize = mif->luma_x_end0 - mif->luma_x_start0 + 1;
	unsigned int pic_hsize = mif->buf_crop_en ?
			mif->buf_hsize : mif->luma_x_end0 - mif->luma_x_start0 + 1;

	// 0:8 bits 1:10 bits 422(old mode,12bit) 2: 10bit 444 3:10bit 422(full pack) or 444
	unsigned int comp_bits = (mif->bit_mode == 0) ? 8 :
				(mif->bit_mode == 1) ? 12 : 10;

	//00: 4:2:0; 01: 4:2:2; 10: 4:4:4
	unsigned int comp_num  = (mif->video_mode == 2) ? 3 : 2;

	// 00 : one canvas; 01 : 3 canvas(old 4:2:0).  10: 2 canvas. (NV21).
	if (mif->set_separate_en == 0) {
		burst_stride_0 = (pic_hsize * comp_num * comp_bits + 127) >> 7;//burst
		burst_stride_1 =  0;
		burst_stride_2 =  0;
	} else if (mif->set_separate_en == 1) {
		burst_stride_0 =  (pic_hsize * comp_bits + 127) >> 7;//burst
		burst_stride_1 =  (((pic_hsize + 1) >> 1) *
				   comp_bits + 127) >> 7;//burst
		burst_stride_2 =  (((pic_hsize + 1) >> 1) *
				   comp_bits + 127) >> 7;//burst
	} else {
		burst_stride_0 =  (pic_hsize * comp_bits + 127) >> 7;//burst
		burst_stride_1 =  (pic_hsize * comp_bits + 127) >> 7;//burst
		burst_stride_2 =  0;
	}
	//stimulus_display("di_mif0_stride: burst_stride_0    = %x\n",burst_stride_0);

	*stride_y  = ((burst_stride_0 +  3) >> 2) << 2;//now need 64bytes aligned
	*stride_cb = ((burst_stride_1 + 3) >> 2) << 2;//now need 64bytes aligned
	*stride_cr = ((burst_stride_2 + 3) >> 2) << 2;//now need 64bytes aligned

	//stimulus_display("di_mif0_stride: stride_y    = %x\n",*stride_y);
}

static void di_mif0_stride_input(struct DI_MIF_S *mif,
				 unsigned int  *stride_y,
				 unsigned int  *stride_cb,
				 unsigned int  *stride_cr)
{
	if (mif->set_separate_en == 2) {
		//nv21 ?
		*stride_y = (mif->cvs0_w + 15) >> 4;
		*stride_cb = (mif->cvs1_w + 15) >> 4;
		*stride_cr = 0;
	} else if (mif->set_separate_en == 1) {
		*stride_y = (mif->cvs0_w + 15) >> 4;
		*stride_cb = (mif->cvs1_w + 15) >> 4;
		*stride_cr = (mif->cvs2_w + 15) >> 4;
	} else {
		*stride_y = (mif->cvs0_w + 15) >> 4;
		*stride_cb = 0;
		*stride_cr = 0;
	}

	dim_print("%s: stride_y = %d %d %d\n",
		__func__,
		*stride_y,
		*stride_cb,
		*stride_cr);
}

/* DI_MIF0_t -> DI_MIF_S*/
void di_mif0_linear_rd_cfg(struct DI_MIF_S *mif, int mif_index, const struct reg_acc *ops)
{
	unsigned int stride_y;
	unsigned int stride_cb;
	unsigned int stride_cr;
	const struct reg_acc *op;
	unsigned int off;

	if (!ops)
		op = &di_pre_regset;
	else
		op = ops;

	off = di_mif_add_get_offset_v3(mif_index);
	if (off == DIM_ERR) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	dbg_ic("%s:%d:off[0x%x],0x%lx,0x%lx,0x%lx\n",
		__func__,
		mif_index,
		off,
		mif->addr0,
		mif->addr1,
		mif->addr2);
	if (mif_index == DI_MIF0_ID_INP)
		di_mif0_stride_input(mif, &stride_y, &stride_cb, &stride_cr);
	else
		di_mif0_stride(mif, &stride_y, &stride_cb, &stride_cr);

	op->bwr(off + RDMIFXN_STRIDE_1, 1, 16, 1);//linear mode

	op->bwr(off + RDMIFXN_BADDR_Y, mif->addr0 >> 4, 0, 32);//linear address
	op->bwr(off + RDMIFXN_BADDR_CB, mif->addr1 >> 4, 0, 32);//linear address
	op->bwr(off + RDMIFXN_BADDR_CR, mif->addr2 >> 4, 0, 32);//linear address

	dbg_ic("\ty[%d]cb[%d]cr[%d]\n", stride_y, stride_cb, stride_cr);
	dbg_ic("\ty[0x%lx]cb[0x%lx]cr[0x%lx]\n", mif->addr0, mif->addr1, mif->addr2);

//	op->bwr(off + RDMIFXN_STRIDE_0, stride_y, 0, 13);//stride
//	op->bwr(off + RDMIFXN_STRIDE_0, stride_cb, 16, 13);//stride
//	op->bwr(off + RDMIFXN_STRIDE_1, stride_cr, 0, 13);//stride
	op->bwr(off + RDMIFXN_STRIDE_0, (stride_cb << 16) | stride_y, 0, 32);//stride
	op->bwr(off + RDMIFXN_STRIDE_1, (1 << 16) | stride_cr, 0, 32);//stride

	dbg_ic("\t:reg:0x%x= 0x%x\n", off + RDMIFXN_BADDR_Y, op->rd(off + RDMIFXN_BADDR_Y));
	dbg_ic("\t:reg:0x%x= 0x%x\n", off + RDMIFXN_BADDR_CB, op->rd(off + RDMIFXN_BADDR_CB));
	dbg_ic("\t:reg:0x%x= 0x%x\n", off + RDMIFXN_BADDR_CR, op->rd(off + RDMIFXN_BADDR_CR));
	dbg_ic("\t:reg:0x%x= 0x%x\n", off + RDMIFXN_STRIDE_0, op->rd(off + RDMIFXN_STRIDE_0));
	dbg_ic("\t:reg:0x%x= 0x%x\n", off + RDMIFXN_STRIDE_1, op->rd(off + RDMIFXN_STRIDE_1));
}

/* DI_MIF0_t -> DI_MIF_S*/
void di_mif0_linear_wr_cfg(struct DI_MIF_S *mif, int mif_index, const struct reg_acc *ops)
{
	unsigned int WRMIF_BADDR0;
	unsigned int WRMIF_BADDR1;
	unsigned int WRMIF_STRIDE0;
	unsigned int WRMIF_STRIDE1;
	unsigned int stride_y;
	unsigned int stride_cb;
	unsigned int stride_cr;
	const struct reg_acc *op;

	if (!ops)
		op = &di_pre_regset;
	else
		op = ops;

	dbg_ic("%s:%d:0x%lx,0x%lx,0x%lx\n",
			__func__,
			mif_index,
			mif->addr0,
			mif->addr1,
			mif->addr2);

	if (mif_index == 0) {
		WRMIF_BADDR0          = DI_NRWR_BADDR0;
		WRMIF_BADDR1          = DI_NRWR_BADDR1;
		WRMIF_STRIDE0         = DI_NRWR_STRIDE0;
		WRMIF_STRIDE1         = DI_NRWR_STRIDE1;
	} else if (mif_index == 1) {
		WRMIF_BADDR0          = DI_DIWR_BADDR0;
		WRMIF_BADDR1          = DI_DIWR_BADDR1;
		WRMIF_STRIDE0         = DI_DIWR_STRIDE0;
		WRMIF_STRIDE1         = DI_DIWR_STRIDE1;
	} else {
		PR_ERR("ERROR:WR_MIF WRONG!!!\n");
		return;
	}

	di_mif0_stride(mif, &stride_y, &stride_cb, &stride_cr);

	op->wr(WRMIF_BADDR0, mif->addr0	>> 4);
	op->wr(WRMIF_BADDR1, mif->addr1 >> 4);
	op->wr(WRMIF_STRIDE0, stride_y);
	op->wr(WRMIF_STRIDE1, stride_cb);
}

static void di_mif0_stride2(struct DI_SIM_MIF_s *mif,
			    unsigned int  *stride_y,
			    unsigned int  *stride_cb,
			    unsigned int  *stride_cr)
{
	unsigned int burst_stride_0;
	unsigned int burst_stride_1;
	unsigned int burst_stride_2;

	//if support scope,need change this to real hsize
	//unsigned int pic_hsize = mif->luma_x_end0 - mif->luma_x_start0 + 1;
	//unsigned int pic_hsize = mif->end_x - mif->start_x + 1;
	unsigned int pic_hsize = mif->buf_crop_en ?
		mif->buf_hsize : mif->end_x - mif->start_x + 1;
	// 0:8 bits 1:10 bits 422(old mode,12bit)
	// 2: 10bit 444 3:10bit 422(full pack) or 444
	unsigned int comp_bits = (mif->bit_mode == 0) ? 8 :
				(mif->bit_mode == 1) ? 12 : 10;

	//00: 4:2:0; 01: 4:2:2; 10: 4:4:4
	unsigned int comp_num  = (mif->video_mode == 2) ? 3 : 2;

	// 00 : one canvas; 01 : 3 canvas(old 4:2:0).  10: 2 canvas. (NV21).
	if (mif->set_separate_en == 0) {
		burst_stride_0 = (pic_hsize * comp_num * comp_bits + 127) >> 7;
		//burst
		burst_stride_1 =  0;
		burst_stride_2 =  0;
	} else if (mif->set_separate_en == 1) {
		burst_stride_0 =  (pic_hsize * comp_bits + 127) >> 7;//burst
		burst_stride_1 =  (((pic_hsize + 1) >> 1) *
				   comp_bits + 127) >> 7;//burst
		burst_stride_2 =  (((pic_hsize + 1) >> 1) *
				   comp_bits + 127) >> 7;//burst
	} else {
		burst_stride_0 =  (pic_hsize * comp_bits + 127) >> 7;//burst
		burst_stride_1 =  (pic_hsize * comp_bits + 127) >> 7;//burst
		burst_stride_2 =  0;
	}
	dbg_ic("di_mif0_stride: burst_stride_0    = %x\n", burst_stride_0);

	*stride_y  = ((burst_stride_0 +  3) >> 2) << 2;//now need 64bytes aligned
	*stride_cb = ((burst_stride_1 + 3) >> 2) << 2;//now need 64bytes aligned
	*stride_cr = ((burst_stride_2 + 3) >> 2) << 2;//now need 64bytes aligned

	dbg_ic("\t: y[%d] cb[%d] cr[%d]\n", *stride_y, *stride_cb, *stride_cr);
}

/* DI_MIF0_t -> DI_SIM_MIF_s*/
void di_mif0_linear_wr_cfg2(struct DI_SIM_MIF_s *mif, int mif_index)
{
	unsigned int WRMIF_BADDR0;
	unsigned int WRMIF_BADDR1;
	unsigned int WRMIF_STRIDE0;
	unsigned int WRMIF_STRIDE1;
	unsigned int stride_y;
	unsigned int stride_cb;
	unsigned int stride_cr;
	const struct reg_acc *op = &di_pre_regset;

	dbg_ic("%s:%d:0x%lx,0x%lx,0x%lx\n",
			__func__,
			mif_index,
			mif->addr,
			mif->addr1,
			mif->addr2);

	if (mif_index == 0) {
		WRMIF_BADDR0          = DI_NRWR_BADDR0;
		WRMIF_BADDR1          = DI_NRWR_BADDR1;
		WRMIF_STRIDE0         = DI_NRWR_STRIDE0;
		WRMIF_STRIDE1         = DI_NRWR_STRIDE1;
	} else if (mif_index == 1) {
		WRMIF_BADDR0          = DI_DIWR_BADDR0;
		WRMIF_BADDR1          = DI_DIWR_BADDR1;
		WRMIF_STRIDE0         = DI_DIWR_STRIDE0;
		WRMIF_STRIDE1         = DI_DIWR_STRIDE1;
	} else {
		PR_ERR("ERROR:WR_MIF WRONG!!!\n");
		return;
	}

	di_mif0_stride2(mif, &stride_y, &stride_cb, &stride_cr);

	op->wr(WRMIF_BADDR0, mif->addr	>> 4);
	op->wr(WRMIF_BADDR1, mif->addr1 >> 4);
	op->wr(WRMIF_STRIDE0, stride_y);
	op->wr(WRMIF_STRIDE1, stride_cb);
	dbg_ic("\t:reg:0x%x= 0x%x\n", WRMIF_BADDR0, op->rd(WRMIF_BADDR0));
	dbg_ic("\t:reg:0x%x= 0x%x\n", WRMIF_BADDR1, op->rd(WRMIF_BADDR1));
	dbg_ic("\t:reg:0x%x= 0x%x\n", WRMIF_STRIDE0, op->rd(WRMIF_STRIDE0));
	dbg_ic("\t:reg:0x%x= 0x%x\n", WRMIF_STRIDE1, op->rd(WRMIF_STRIDE1));
}

/* struct DI_MIF1_t -> DI_SIM_MIF_s*/
static void di_mif1_stride2(unsigned int per_bits,
			unsigned int pic_hsize,
			unsigned int  *stride)
{
	//if support scope,need change this to real hsize
	//unsigned int pic_hsize = mif->end_x - mif->start_x + 1;

	*stride  = (pic_hsize * per_bits + 511) >> 9;//burst
	*stride  = (*stride) << 2;
	//now need 64bytes aligned,
	//just set 32bytes aligned(DDR change to burst2 align)
	//because of previous data are 32bytes aligned
}

void di_mif1_linear_rd_cfg(struct DI_SIM_MIF_s *mif,
			unsigned int CTRL1,
			unsigned int CTRL2,
			unsigned int BADDR)
{
	unsigned int stride;
	const struct reg_acc *op = &di_pre_regset;

	dbg_ic("%s:\n", __func__);
	//di_mif1_stride(mif, &stride);
	di_mif1_stride2(mif->per_bits,  (mif->end_x - mif->start_x + 1), &stride);
	op->bwr(CTRL1, 1, 3, 1);//linear_mode
	op->bwr(CTRL2, stride, 0, 13);//stride
	op->wr(BADDR, mif->addr >> 4);//base_addr
	dbg_ic("stride[%d],per_bits[%d]\n", stride, mif->per_bits);
	dbg_ic("reg[0x%x] = 0x%x\n", CTRL1, op->rd(CTRL1));
	dbg_ic("reg[0x%x] = 0x%x\n", CTRL2, op->rd(CTRL2));
	dbg_ic("reg[0x%x] = 0x%x\n", BADDR, op->rd(BADDR));
}

static void di_mif1_linear_wr_cfg(struct DI_SIM_MIF_s *mif,
			   unsigned int STRIDE,
			   unsigned int BADDR)
{
	unsigned int stride;
	const struct reg_acc *op = &di_pre_regset;

	dbg_ic("%s:\n", __func__);
	//di_mif1_stride(mif, &stride);
	di_mif1_stride2(mif->per_bits, (mif->end_x - mif->start_x + 1), &stride);
	op->wr(STRIDE, (0 << 31) | stride);//stride
	op->wr(BADDR, mif->addr >> 4);//base_addr
	dbg_ic("\tstride[%d],per_bits[%d]\n", stride, mif->per_bits);
	dbg_ic("\treg[0x%x] = 0x%x\n", STRIDE, op->rd(STRIDE));
	dbg_ic("\treg[0x%x] = 0x%x\n", BADDR, op->rd(BADDR));
}

void di_mcmif_linear_rd_cfg(struct DI_MC_MIF_s *mif,
			unsigned int CTRL1,
			unsigned int CTRL2,
			unsigned int BADDR)
{
	unsigned int stride;
	const struct reg_acc *op = &di_pre_regset;

	dbg_ic("%s:\n", __func__);
	//di_mif1_stride(mif, &stride);
	di_mif1_stride2(mif->per_bits, mif->size_x, &stride);
	op->bwr(CTRL1, 1, 3, 1);//linear_mode
	op->bwr(CTRL2, stride, 0, 13);//stride
	op->wr(BADDR, mif->addr >> 4);//base_addr

	dbg_ic("stride[%d],per_bits[%d]\n", stride, mif->per_bits);
	dbg_ic("reg[0x%x] = 0x%x\n", CTRL1, op->rd(CTRL1));
	dbg_ic("reg[0x%x] = 0x%x\n", CTRL2, op->rd(CTRL2));
	dbg_ic("reg[0x%x] = 0x%x\n", BADDR, op->rd(BADDR));
}

static void di_mcmif_linear_wr_cfg(struct DI_MC_MIF_s *mif,
			   unsigned int STRIDE,
			   unsigned int BADDR)
{
	unsigned int stride;
	const struct reg_acc *op = &di_pre_regset;

	dbg_ic("%s:\n", __func__);
	//di_mif1_stride(mif, &stride);
	di_mif1_stride2(mif->per_bits, mif->size_x, &stride);
	op->wr(STRIDE, (0 << 31) | stride);//stride
	op->wr(BADDR, mif->addr >> 4);//base_addr
	dbg_ic("\tstride[%d],per_bits[%d]\n", stride, mif->per_bits);
	dbg_ic("\treg[0x%x] = 0x%x\n", STRIDE, op->rd(STRIDE));
	dbg_ic("\treg[0x%x] = 0x%x\n", BADDR, op->rd(BADDR));
}

//set_ma_pre_mif_g12
static void set_ma_pre_mif_t7(void *pre,
			       unsigned short urgent)
{
	struct DI_SIM_MIF_s *mtnwr_mif;
	struct DI_SIM_MIF_s *contprd_mif;
	struct DI_SIM_MIF_s *contp2rd_mif;
	struct DI_SIM_MIF_s *contwr_mif;
	const struct reg_acc *op = &di_pre_regset;
	struct di_pre_stru_s *ppre = (struct di_pre_stru_s *)pre;

	dbg_ic("%s:\n", __func__);
	mtnwr_mif	= &ppre->di_mtnwr_mif;
	contp2rd_mif	= &ppre->di_contp2rd_mif;
	contprd_mif	= &ppre->di_contprd_mif;
	contwr_mif	= &ppre->di_contwr_mif;

	mtnwr_mif->per_bits	= 4;
	contp2rd_mif->per_bits	= 4;
	contprd_mif->per_bits	= 4;
	contwr_mif->per_bits	= 4;

	op->bwr(CONTRD_SCOPE_X, contprd_mif->start_x, 0, 13);
	op->bwr(CONTRD_SCOPE_X, contprd_mif->end_x, 16, 13);
	op->bwr(CONTRD_SCOPE_Y, contprd_mif->start_y, 0, 13);
	op->bwr(CONTRD_SCOPE_Y, contprd_mif->end_y, 16, 13);
	//DIM_RDMA_WR_BITS(CONTRD_CTRL1, contprd_mif->canvas_num, 16, 8);
	op->bwr(CONTRD_CTRL1, 2, 8, 2);
	op->bwr(CONTRD_CTRL1, 0, 0, 3);
	di_mif1_linear_rd_cfg(contprd_mif,
			      CONTRD_CTRL1,
			      CONTRD_CTRL2,
			      CONTRD_BADDR);

	op->bwr(CONT2RD_SCOPE_X, contp2rd_mif->start_x, 0, 13);
	op->bwr(CONT2RD_SCOPE_X, contp2rd_mif->end_x, 16, 13);
	op->bwr(CONT2RD_SCOPE_Y, contp2rd_mif->start_y, 0, 13);
	op->bwr(CONT2RD_SCOPE_Y, contp2rd_mif->end_y, 16, 13);
	//DIM_RDMA_WR_BITS(CONT2RD_CTRL1, contp2rd_mif->canvas_num, 16, 8);
	op->bwr(CONT2RD_CTRL1, 2, 8, 2);
	op->bwr(CONT2RD_CTRL1, 0, 0, 3);
	di_mif1_linear_rd_cfg(contp2rd_mif,
			      CONT2RD_CTRL1,
			      CONT2RD_CTRL2,
			      CONT2RD_BADDR);

	/* current field mtn canvas index. */
	op->bwr(MTNWR_X, mtnwr_mif->start_x, 16, 13);
	op->bwr(MTNWR_X, mtnwr_mif->end_x, 0, 13);
	op->bwr(MTNWR_X, 2, 30, 2);
	op->bwr(MTNWR_Y, mtnwr_mif->start_y, 16, 13);
	op->bwr(MTNWR_Y, mtnwr_mif->end_y, 0, 13);
	//DIM_RDMA_WR_BITS(MTNWR_CTRL, mtnwr_mif->canvas_num, 0, 8);
	op->bwr(MTNWR_CAN_SIZE,
			 (mtnwr_mif->end_y - mtnwr_mif->start_y), 0, 13);
	op->bwr(MTNWR_CAN_SIZE,
			 (mtnwr_mif->end_x - mtnwr_mif->start_x), 16, 13);
	di_mif1_linear_wr_cfg(mtnwr_mif, MTNWR_STRIDE, MTNWR_BADDR);

	op->bwr(CONTWR_X, contwr_mif->start_x, 16, 13);
	op->bwr(CONTWR_X, contwr_mif->end_x, 0, 13);
	op->bwr(CONTWR_X, 2, 30, 2);
	op->bwr(CONTWR_Y, contwr_mif->start_y, 16, 13);
	op->bwr(CONTWR_Y, contwr_mif->end_y, 0, 13);
	//DIM_RDMA_WR_BITS(CONTWR_CTRL, contwr_mif->canvas_num, 0, 8);
	op->bwr(CONTWR_CAN_SIZE,
			 (contwr_mif->end_y - contwr_mif->start_y), 0, 13);
	op->bwr(CONTWR_CAN_SIZE,
			 (contwr_mif->end_x - contwr_mif->start_x), 16, 13);
	di_mif1_linear_wr_cfg(contwr_mif, CONTWR_STRIDE, CONTWR_BADDR);
}

//set_post_mtnrd_mif_g12
static void set_post_mtnrd_mif_t7(struct DI_SIM_MIF_s *mtnprd_mif)
{
	dbg_ic("%s:", __func__);
	DIM_VSYNC_WR_MPEG_REG(MTNRD_SCOPE_X,
			      (mtnprd_mif->end_x << 16) |
			      (mtnprd_mif->start_x));
	DIM_VSYNC_WR_MPEG_REG(MTNRD_SCOPE_Y,
			      (mtnprd_mif->end_y << 16) |
			      (mtnprd_mif->start_y));
	//DIM_VSC_WR_MPG_BT(MTNRD_CTRL1, mtnprd_mif->canvas_num, 16, 8);
	DIM_VSC_WR_MPG_BT(MTNRD_CTRL1, 0, 0, 3);
	di_mif1_linear_rd_cfg(mtnprd_mif, MTNRD_CTRL1, MTNRD_CTRL2, MTNRD_BADDR);
}

//dimh_enable_mc_di_pre_g12
static void pre_enable_mc_t7(struct DI_MC_MIF_s *mcinford_mif,
			       struct DI_MC_MIF_s *mcinfowr_mif,
			       struct DI_MC_MIF_s *mcvecwr_mif,
			       unsigned char mcdi_en)
{
	dbg_ic("%s:", __func__);
	DIM_RDMA_WR_BITS(MCDI_MOTINEN, (mcdi_en ? 3 : 0), 0, 2);

	if (is_meson_g12a_cpu()	||
	    is_meson_g12b_cpu()	||
	    is_meson_sm1_cpu()) {
		DIM_RDMA_WR(MCDI_CTRL_MODE, (mcdi_en ? 0x1bfef7ff : 0));
	} else if (DIM_IS_IC_EF(SC2)) {//from vlsi yanling
		if (mcdi_en) {
			DIM_RDMA_WR_BITS(MCDI_CTRL_MODE, 0xf7ff, 0, 16);
			DIM_RDMA_WR_BITS(MCDI_CTRL_MODE, 0xdff, 17, 15);
		} else {
			DIM_RDMA_WR(MCDI_CTRL_MODE, 0);
		}
	} else {
		DIM_RDMA_WR(MCDI_CTRL_MODE, (mcdi_en ? 0x1bfff7ff : 0));
	}

	mcinford_mif->per_bits	= 16;
	mcinfowr_mif->per_bits	= 16;
	mcvecwr_mif->per_bits	= 16;
	DIM_RDMA_WR_BITS(DI_PRE_CTRL, (mcdi_en ? 3 : 0), 16, 2);

	DIM_RDMA_WR_BITS(MCINFRD_SCOPE_X, mcinford_mif->size_x, 16, 13);
	DIM_RDMA_WR_BITS(MCINFRD_SCOPE_Y, mcinford_mif->size_y, 16, 13);
	//DIM_RDMA_WR_BITS(MCINFRD_CTRL1, mcinford_mif->canvas_num, 16, 8);
	DIM_RDMA_WR_BITS(MCINFRD_CTRL1, 2, 0, 3);
	di_mcmif_linear_rd_cfg(mcinford_mif,
			      MCINFRD_CTRL1,
			      MCINFRD_CTRL2,
			      MCINFRD_BADDR);

	DIM_RDMA_WR_BITS(MCVECWR_X, mcvecwr_mif->size_x, 0, 13);
	DIM_RDMA_WR_BITS(MCVECWR_Y, mcvecwr_mif->size_y, 0, 13);
	//DIM_RDMA_WR_BITS(MCVECWR_CTRL, mcvecwr_mif->canvas_num, 0, 8);
	DIM_RDMA_WR_BITS(MCVECWR_CAN_SIZE, mcvecwr_mif->size_y, 0, 13);
	DIM_RDMA_WR_BITS(MCVECWR_CAN_SIZE, mcvecwr_mif->size_x, 16, 13);
	di_mcmif_linear_wr_cfg(mcvecwr_mif, MCVECWR_STRIDE, MCVECWR_BADDR);

	DIM_RDMA_WR_BITS(MCINFWR_X, mcinfowr_mif->size_x, 0, 13);
	DIM_RDMA_WR_BITS(MCINFWR_Y, mcinfowr_mif->size_y, 0, 13);
	//DIM_RDMA_WR_BITS(MCINFWR_CTRL, mcinfowr_mif->canvas_num, 0, 8);
	DIM_RDMA_WR_BITS(MCINFWR_CAN_SIZE, mcinfowr_mif->size_y, 0, 13);
	DIM_RDMA_WR_BITS(MCINFWR_CAN_SIZE, mcinfowr_mif->size_x, 16, 13);
	di_mcmif_linear_wr_cfg(mcinfowr_mif, MCINFWR_STRIDE, MCINFWR_BADDR);
}

unsigned int dw_get_h(void)
{	//bit 15: enable dw
	//bit 14: show
	//bit 13: shrk en
	return ((sc2_reg_mask >> 8) & 0x3);
}

/*************************************************/

#define DI_SCALAR_DISABLE	(1)

unsigned int get_afbcd_offset(enum EAFBC_DEC dec)
{
	int i, offset;

	i = 0;
	switch (dec) {
	case EAFBC_DEC0:
		i = -24;
		break;
	case EAFBC_DEC1:
		i = -23;
		break;
	case EAFBC_DEC2_DI:
		i = 0;
		break;
	case EAFBC_DEC3_MEM:
		i = 2;
		break;
	case EAFBC_DEC_CHAN2:
		i = 1;
		break;
	case EAFBC_DEC_IF0:
		i = 4;
		break;
	case EAFBC_DEC_IF1:
		i = 3;
		break;
	case EAFBC_DEC_IF2:
		i = 5;
		break;
	}
	offset = i * 0x80;

	return offset;
}

//--------------------display.c----------------------------

static unsigned int set_afbcd_mult_simple(int index,
					  struct AFBCD_S *inp_afbcd,
					  const struct reg_acc *opin)
{
	u32 module_sel	= index;
	// 3 bits 0-5: select module m0-m5 6:vd1 7:vd2
	u32 hold_line_num	= inp_afbcd->hold_line_num;
	// 7 bits
	u32 blk_mem_mode	= inp_afbcd->blk_mem_mode;
	// 1 bits 1-12x128bit/blk32x4, 0-16x128bit/blk32x4
	u32 compbits_y	= inp_afbcd->compbits;
	// 2 bits 0-8bits 1-9bits 2-10bits
	u32 compbits_u	= inp_afbcd->compbits;
	// 2 bits 0-8bits 1-9bits 2-10bits
	u32 compbits_v	= inp_afbcd->compbits;
	// 2 bits 0-8bits 1-9bits 2-10bits
	u32 hsize_in	= inp_afbcd->hsize;
	//13 bits dec source pic hsize
	u32 vsize_in	= inp_afbcd->vsize;
	//13 bits dec source pic vsize
	u32 mif_info_baddr	= inp_afbcd->head_baddr; //32 bits
	u32 mif_data_baddr	= inp_afbcd->body_baddr; //32 bits
	u32 out_horz_bgn	= inp_afbcd->out_horz_bgn; //13 bits
	u32 out_vert_bgn	= inp_afbcd->out_vert_bgn; //13 bits
	u32 out_horz_end	= inp_afbcd->hsize - 1; //13 bits
	u32 out_vert_end	= inp_afbcd->vsize - 1; //13 bits

	u32 hz_ini_phase	= inp_afbcd->hz_ini_phase; // 4 bits
	u32 vt_ini_phase	= inp_afbcd->vt_ini_phase; // 4 bits
	u32 hz_rpt_fst0_en	= inp_afbcd->hz_rpt_fst0_en; // 1 bits
	u32 vt_rpt_fst0_en	= inp_afbcd->vt_rpt_fst0_en; // 1 bits

	u32 rev_mode	= inp_afbcd->rev_mode;
	// 2 bits [1]: vertical rev; [0]: horzontal rev
	u32 vert_skip_y	= inp_afbcd->v_skip_en;
	// 2 bits 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
	u32 horz_skip_y	= inp_afbcd->h_skip_en;
	// 2 bits 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
	u32 vert_skip_uv = inp_afbcd->v_skip_en;
	// 2 bits 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
	u32 horz_skip_uv = inp_afbcd->h_skip_en;
	// 2 bits 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2

#ifdef ARY_MARK
	u32 def_color_y	= 0; //10 bits
	u32 def_color_u	= (inp_afbcd->compbits == 0) ?
		0x200 : (inp_afbcd->compbits == 1) ? 0x100 : 0x80; //10 bits
#else //ary change
	u32 def_color_y	= 0x3ff;	//ary 0; //10 bits
	//u32 def_color_u	= (inp_afbcd->compbits == 0) ?
	//	0x200 : (inp_afbcd->compbits == 1) ? 0x100 : 0x80; //10 bits
	u32 def_color_u	= (inp_afbcd->compbits == 0) ?
		0x80 : (inp_afbcd->compbits == 1) ? 0x100 : 0x200; //10 bits
#endif
	u32 def_color_v;
	//ary	= def_color_u     ; //10 bits
	u32 reg_lossy_en	= inp_afbcd->reg_lossy_en; // 2 bits
	u32 ddr_sz_mode	= inp_afbcd->ddr_sz_mode;
	// 1 bits   1:mmu mode
	u32 fmt_mode	= inp_afbcd->fmt_mode;
	// 2 bits   default = 2, 0:yuv444 1:yuv422 2:yuv420
	u32 reg_fmt444_comb	= inp_afbcd->fmt444_comb;
	// 1 bits   fmt444 8bit pack enable
	u32 reg_dos_uncomp_mode	= inp_afbcd->dos_uncomp; //1 bits

	u32 pip_src_mode	= inp_afbcd->pip_src_mode;
	// 1 bits   0: src from vdin/dos  1:src from pip
	u32 rot_ofmt_mode	= inp_afbcd->rot_ofmt_mode;
	// 2 bits   default = 2, 0:yuv444 1:yuv422 2:yuv420
	u32 rot_ocompbit	= inp_afbcd->rot_ocompbit;
	// 2 bits rot output bit width,0:8bit 1:9bit 2:10bit
	u32 rot_drop_mode	= inp_afbcd->rot_drop_mode;
	//rot_drop_mode
	u32 rot_vshrk	= inp_afbcd->rot_vshrk; //rot_vshrk
	u32 rot_hshrk	= inp_afbcd->rot_hshrk; //rot_hshrk
	u32 rot_hbgn	= inp_afbcd->rot_hbgn; //5 bits
	u32 rot_vbgn	= inp_afbcd->rot_vbgn; //2 bits
	u32 rot_en		= inp_afbcd->rot_en;	       //1 bits

/*input end *****************************************************/
	u32 compbits_yuv;
	u32 conv_lbuf_len; //12 bits
	u32 dec_lbuf_depth; //12 bits
	u32 mif_lbuf_depth; //12 bits
	u32 mif_blk_bgn_h; //10 bits
	u32 mif_blk_bgn_v; //12 bits
	u32 mif_blk_end_h; //10 bits
	u32 mif_blk_end_v; //12 bits
	u32 dec_pixel_bgn_h; //13 bits
	u32 dec_pixel_bgn_v; //13 bits
	u32 dec_pixel_end_h; //13 bits
	u32 dec_pixel_end_v; //13 bits

	u32 dec_hsize_proc; //13 bits
	u32 dec_vsize_proc; //13 bits
	//ary no use    uint32_t hsize_out      ; //13 bits
	//ary no use    uint32_t vsize_out      ; //13 bits
	u32 out_end_dif_h; //13 bits
	u32 out_end_dif_v; //13 bits
	u32 rev_mode_h;
	u32 rev_mode_v;

	u32 fmt_size_h ; //13 bits
	u32 fmt_size_v; //13 bits
	u32 hfmt_en	= 0;
	u32 vfmt_en	= 0;
	u32 uv_vsize_in	= 0;
	u32 vt_yc_ratio	= 0;
	u32 hz_yc_ratio = 0;

	s32 compbits_eq8;
	s32 regs_ofst;
	u32 vt_phase_step;	//ary add for compile
	u32 vfmt_w;		//ary add for compile
	const struct reg_acc *op;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;

	def_color_v	= def_color_u; //10 bits //ary
#ifdef MARK_SC2
	regs_ofst = (module_sel == 6) ? -24 : //vd1      0x48 0x0-0x7f
			(module_sel == 7) ? -23 : //vd2    0x48 0x80-0xff
			module_sel;           //di_m0-m5 0x54-0x56
	regs_ofst = 128 * 4 * regs_ofst;
#endif
	regs_ofst = get_afbcd_offset(index);
	dim_print("%s:0x%x\n", __func__, (regs_ofst + AFBCDM_ENABLE));
	rev_mode_h = rev_mode & 0x1;
	rev_mode_v = rev_mode & 0x2;

	compbits_yuv = ((compbits_v & 0x3) << 4) |
		((compbits_u & 0x3) << 2) |
		((compbits_y & 0x3) << 0);

	compbits_eq8 = (compbits_y == 0 && compbits_u == 0 && compbits_v == 0);
	if ((hsize_in > 1024 &&
	     fmt_mode == 0 && compbits_eq8 == 0) || hsize_in > 2048)
		conv_lbuf_len = 256;
	else
		conv_lbuf_len = 128;

	//if(hsize_in>2048) dec_lbuf_depth = 128; //todo
	//else              dec_lbuf_depth = 64 ;
	//if(hsize_in>2048) mif_lbuf_depth = 128; //todo
	//else              mif_lbuf_depth = 64 ;

	dec_lbuf_depth = 128;
	mif_lbuf_depth = 128;

	dec_hsize_proc  = ((hsize_in >> 5) +
		((hsize_in & 0x1f) != 0)) * 32;  // unit: pixel
	dec_vsize_proc  = ((vsize_in >> 2) +
		((vsize_in & 0x3) != 0)) * 4;  // unit: pixel

	mif_blk_bgn_h   = out_horz_bgn >> 5;
	mif_blk_bgn_v   = out_vert_bgn >> 2;
	mif_blk_end_h   = out_horz_end >> 5;
	mif_blk_end_v   = out_vert_end >> 2;

	out_end_dif_h   = out_horz_end - out_horz_bgn;
	out_end_dif_v   = out_vert_end - out_vert_bgn;

	dec_pixel_bgn_h = (out_horz_bgn & 0x1f);
	dec_pixel_bgn_v = (out_vert_bgn & 0x03);
	dec_pixel_end_h = (dec_pixel_bgn_h + out_end_dif_h);
	dec_pixel_end_v = (dec_pixel_bgn_v + out_end_dif_v);

	fmt_size_h   = ((out_horz_end >> 1) << 1) + 1 -
		((out_horz_bgn >> 1) << 1);
	fmt_size_v   = ((out_vert_end >> 1) << 1) + 1 -
		((out_vert_bgn >> 1) << 1);
	fmt_size_h   = horz_skip_y != 0 ? (fmt_size_h >> 1) + 1 :
		fmt_size_h + 1;
	fmt_size_v   = vert_skip_y != 0 ? (fmt_size_v >> 1) + 1 :
		fmt_size_v + 1;

	//stimulus_display("jsen_debug ofst=%x\n", regs_ofst);
	//stimulus_display("jsen_debug reg_addr=%x\n", (regs_ofst+AFBCDM_MODE));
	dbg_afbc("\n=====afbcd module %x==========\n", module_sel);
	dbg_afbc("fmt=%d      bit=%d\n", fmt_mode, compbits_y);
	dbg_afbc("hsize=%d    vsize=%d\n", hsize_in, vsize_in);
	dbg_afbc("win_hbgn=%d win_hend=%d\n", out_horz_bgn,
		 out_horz_end);
	dbg_afbc("win_vbgn=%d win_vend=%d\n", out_vert_bgn,
		 out_vert_end);
	dbg_afbc("dos=%d      mmu=%d\n", reg_dos_uncomp_mode,
		 ddr_sz_mode);
	dbg_afbc("rot_en=%d   pip_src=%d\n", rot_en, pip_src_mode);
	dbg_afbc("rev_mode=%d\n", rev_mode);
	dbg_afbc("===========================\n");
	hold_line_num = (hold_line_num > 4) ? (hold_line_num - 4) : 0;
	//ary: below maybe set_di_afbc_dec_base===========
	op->wr((regs_ofst + AFBCDM_MODE),
		((ddr_sz_mode  & 0x1) << 29) |  // ddr_sz_mode
		((blk_mem_mode & 0x1) << 28) |  // blk_mem_mode
		((rev_mode     & 0x3) << 26) |  // rev_mode
		((3)                 << 24) |  // mif_urgent
		((hold_line_num & 0x7f) << 16) |  // hold_line_num
		((2) << 14) |  // burst_len
		((compbits_yuv & 0x3f) << 8) |  // compbits_yuv
		((vert_skip_y  & 0x3) << 6) |  // vert_skip_y
		((horz_skip_y  & 0x3) << 4) |  // horz_skip_y
		((vert_skip_uv & 0x3) << 2) |  // vert_skip_uv
		((horz_skip_uv & 0x3) << 0)    // horz_skip_uv
		);

	op->wr((regs_ofst + AFBCDM_SIZE_IN),
		((hsize_in & 0x1fff) << 16) |  // hsize_in
		((vsize_in & 0x1fff) << 0)    // vsize_in
		);
	op->wr((regs_ofst + AFBCDM_DEC_DEF_COLOR),
		((def_color_y & 0x3ff) << 20) |  // def_color_y
		((def_color_u & 0x3ff) << 10) |  // def_color_u
		((def_color_v & 0x3ff) << 0)    // def_color_v
		);

	op->wr((regs_ofst + AFBCDM_CONV_CTRL),
		((fmt_mode      & 0x3) << 12) | // fmt_mode
		((conv_lbuf_len & 0xfff) << 0)   // conv_lbuf_len
		);

	op->wr((regs_ofst + AFBCDM_LBUF_DEPTH),
		((dec_lbuf_depth & 0xfff) << 16) |  // dec_lbuf_depth
		((mif_lbuf_depth & 0xfff) << 0)    // mif_lbuf_depth
		);

	op->wr((regs_ofst + AFBCDM_HEAD_BADDR), mif_info_baddr);
	op->wr((regs_ofst + AFBCDM_BODY_BADDR), mif_data_baddr);

	op->wr((regs_ofst + AFBCDM_MIF_HOR_SCOPE),
		((mif_blk_bgn_h & 0x3ff) << 16) |  //
		((mif_blk_end_h & 0x3ff) << 0)    //
		);

	op->wr((regs_ofst + AFBCDM_MIF_VER_SCOPE),
		((mif_blk_bgn_v & 0xfff) << 16) |  //
		((mif_blk_end_v & 0xfff) << 0)    //
		);

	op->wr((regs_ofst + AFBCDM_PIXEL_HOR_SCOPE),
		((dec_pixel_bgn_h & 0x1fff) << 16) |  //
		((dec_pixel_end_h & 0x1fff) << 0)    //
		);

	op->wr((regs_ofst + AFBCDM_PIXEL_VER_SCOPE),
		((dec_pixel_bgn_v & 0x1fff) << 16) |  //
		((dec_pixel_end_v & 0x1fff) << 0)    //
		);

	op->wr((regs_ofst + AFBCDM_ENABLE),
		((1                   & 0x1) << 21) |  //reg_addr_link_en
		((reg_fmt444_comb     & 0x1) << 20) |  //reg_fmt444_comb
		((reg_dos_uncomp_mode & 0x1) << 19) |  //reg_dos_uncomp_mode
		((0x43700             & 0x7ffff) << 0)    //
		);
	//ary end of set_di_afbc_dec_base=================================

	if (fmt_mode == 2) { //420
		hfmt_en = ((horz_skip_y != 0) && (horz_skip_uv == 0)) ? 0 : 1;
		vfmt_en = ((vert_skip_y != 0) && (vert_skip_uv == 0)) ? 0 : 1;
	} else if (fmt_mode == 1) { //422
		hfmt_en = ((horz_skip_y != 0) && (horz_skip_uv == 0)) ? 0 : 1;
		vfmt_en = ((vert_skip_y == 0) && (vert_skip_uv != 0)) ? 1 : 0;
	} else if (fmt_mode == 0) { //444
		hfmt_en = ((horz_skip_y == 0) && (horz_skip_uv != 0)) ? 1 : 0;
		vfmt_en = ((vert_skip_y == 0) && (vert_skip_uv != 0)) ? 1 : 0;
	}

	if (fmt_mode == 2) { //420
		if (vfmt_en) {
			if (vert_skip_uv != 0) {
				uv_vsize_in = (fmt_size_v >> 2);
				vt_yc_ratio = vert_skip_y == 0 ? 2 : 1;
			} else {
				uv_vsize_in = (fmt_size_v >> 1);
				vt_yc_ratio = 1;
			}
		} else {
			uv_vsize_in = fmt_size_v;
			vt_yc_ratio = 0;
		}

		if (hfmt_en) {
			if (horz_skip_uv != 0)
				hz_yc_ratio = horz_skip_y == 0 ? 2 : 1;
			else
				hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	} else if (fmt_mode == 1) {//422
		if (vfmt_en) {
			if (vert_skip_uv != 0) {
				uv_vsize_in = (fmt_size_v >> 1);
				vt_yc_ratio = vert_skip_y == 0 ? 1 : 0;
			} else {
				uv_vsize_in = (fmt_size_v >> 1);
				vt_yc_ratio = 1;
			}
		} else {
			uv_vsize_in = fmt_size_v;
			vt_yc_ratio = 0;
		}

		if (hfmt_en) {
			if (horz_skip_uv != 0)
				hz_yc_ratio = horz_skip_y == 0 ? 2 : 1;
			else
				hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	} else if (fmt_mode == 0) {//444
		if (vfmt_en) { //vert_skip_y==0,vert_skip_uv!=0
			uv_vsize_in = (fmt_size_v >> 1);
			vt_yc_ratio = 1;
		} else {
			uv_vsize_in = fmt_size_v;
			vt_yc_ratio = 0;
		}

		if (hfmt_en) {//horz_skip_y==0,horz_skip_uv!=0
			hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	}

	//stimulus_display("===uv_vsize_in= %0x\n",uv_vsize_in );
	//stimulus_display("===vt_yc_ratio= %0x\n",vt_yc_ratio );
	//stimulus_display("===hz_yc_ratio= %0x\n",hz_yc_ratio );
	//ary below is set_di_afbc_fmt

	vt_phase_step = (16 >> vt_yc_ratio);
	vfmt_w = (fmt_size_h >> hz_yc_ratio);

	op->wr((regs_ofst + AFBCDM_VD_CFMT_CTRL),
		(0 << 28)            |     //hz rpt pixel
		(hz_ini_phase << 24) |     //hz ini phase
		(hz_rpt_fst0_en << 23) |     //repeat p0 enable
		(hz_yc_ratio << 21)  |     //hz yc ratio
		(hfmt_en << 20)      |     //hz enable
		(1 << 17)            |     //nrpt_phase0 enable
		(vt_rpt_fst0_en << 16) |  //repeat l0 enable
		(0 << 12)         |        //skip line num
		(vt_ini_phase << 8)  |     //vt ini phase
		(vt_phase_step << 1) |     //vt phase step (3.4)
		(vfmt_en << 0)             //vt enable
		);

	op->wr((regs_ofst + AFBCDM_VD_CFMT_W),
		(fmt_size_h << 16)     |     //hz format width
		(vfmt_w << 0)              //vt format width
		);

	op->wr((regs_ofst + AFBCDM_VD_CFMT_H), uv_vsize_in);
	//ary set_di_afbc_fmt end
	//===============================================

	//ary below is in set_di_afbc_dec_base same
	op->bwr((regs_ofst + AFBCDM_IQUANT_ENABLE),
		(reg_lossy_en & 0x1), 0, 1);//lossy_luma_en
	op->bwr((regs_ofst + AFBCDM_IQUANT_ENABLE),
		(reg_lossy_en & 0x2), 4, 1);//lossy_chrm_en

	//===============================================
	//ary below is new

	op->wr((regs_ofst + AFBCDM_ROT_CTRL),
		((pip_src_mode  & 0x1) << 27) |  //pip_src_mode
		((rot_drop_mode & 0x7) << 24) |  //rot_uv_vshrk_drop_mode
		((rot_drop_mode & 0x7) << 20) |  //rot_uv_hshrk_drop_mode
		((rot_vshrk     & 0x3) << 18) |  //rot_uv_vshrk_ratio
		((rot_hshrk     & 0x3) << 16) |  //rot_uv_hshrk_ratio
		((rot_drop_mode & 0x7) << 12) |  //rot_y_vshrk_drop_mode
		((rot_drop_mode & 0x7) << 8) |  //rot_y_hshrk_drop_mode
		((rot_vshrk     & 0x3) << 6) |  //rot_y_vshrk_ratio
		((rot_hshrk     & 0x3) << 4) |  //rot_y_hshrk_ratio
		((0             & 0x3) << 2) |  //rot_uv422_drop_mode
		((0             & 0x3) << 1) |  //rot_uv422_omode
		((rot_en        & 0x1) << 0)    //rot_enable
		);

	op->wr((regs_ofst + AFBCDM_ROT_SCOPE),
		((1            & 0x1) << 16) | //reg_rot_ifmt_force444
		((rot_ofmt_mode & 0x3) << 14) | //reg_rot_ofmt_mode
		((rot_ocompbit & 0x3) << 12) | //reg_rot_compbits_out_y
		((rot_ocompbit & 0x3) << 10) | //reg_rot_compbits_out_uv
		((rot_vbgn     & 0x3) << 8) | //rot_wrbgn_v
		((rot_hbgn     & 0x1f) << 0)  //rot_wrbgn_h
		);

	//todo
	//Wr_reg_bits(VD1_AFBCD0_MISC_CTRL, 1, 22, 1);  // select afbc mem
	//Wr_reg_bits(VD1_AFBCD0_MISC_CTRL, 1, 10, 1);  //
	//Wr_reg_bits(VD1_AFBCD0_MISC_CTRL, 1, 12, 1);  // select afbc_dec0
	//Wr_reg_bits(VD2_AFBCD1_MISC_CTRL, 1, 22, 1);  // select afbc mem
	//Wr_reg_bits(VD2_AFBCD1_MISC_CTRL, 1, 10, 1);  //
	//Wr_reg_bits(VD2_AFBCD1_MISC_CTRL, 1, 12, 1);  // select afbc_dec0
	return 0;
} /* set_afbcd_mult*/

static const unsigned int reg_afbc_e_v3[AFBC_ENC_V3_NUB][DIM_AFBCE_V3_NUB] = {
	{
		DI_AFBCE_ENABLE,
		DI_AFBCE_MODE,
		DI_AFBCE_SIZE_IN,
		DI_AFBCE_BLK_SIZE_IN,
		DI_AFBCE_HEAD_BADDR,
		DI_AFBCE_MIF_SIZE,
		DI_AFBCE_PIXEL_IN_HOR_SCOPE,
		DI_AFBCE_PIXEL_IN_VER_SCOPE,
		DI_AFBCE_CONV_CTRL,
		DI_AFBCE_MIF_HOR_SCOPE,
		DI_AFBCE_MIF_VER_SCOPE,
		DI_AFBCE_STAT1,
		DI_AFBCE_STAT2,
		DI_AFBCE_FORMAT,
		DI_AFBCE_MODE_EN,
		DI_AFBCE_DWSCALAR,
		DI_AFBCE_DEFCOLOR_1,
		DI_AFBCE_DEFCOLOR_2,
		DI_AFBCE_QUANT_ENABLE,
		DI_AFBCE_IQUANT_LUT_1,
		DI_AFBCE_IQUANT_LUT_2,
		DI_AFBCE_IQUANT_LUT_3,
		DI_AFBCE_IQUANT_LUT_4,
		DI_AFBCE_RQUANT_LUT_1,
		DI_AFBCE_RQUANT_LUT_2,
		DI_AFBCE_RQUANT_LUT_3,
		DI_AFBCE_RQUANT_LUT_4,
		DI_AFBCE_YUV_FORMAT_CONV_MODE,
		DI_AFBCE_DUMMY_DATA,
		DI_AFBCE_CLR_FLAG,
		DI_AFBCE_STA_FLAGT,
		DI_AFBCE_MMU_NUM,
		DI_AFBCE_MMU_RMIF_CTRL1,
		DI_AFBCE_MMU_RMIF_CTRL2,
		DI_AFBCE_MMU_RMIF_CTRL3,
		DI_AFBCE_MMU_RMIF_CTRL4,
		DI_AFBCE_MMU_RMIF_SCOPE_X,
		DI_AFBCE_MMU_RMIF_SCOPE_Y,
		DI_AFBCE_MMU_RMIF_RO_STAT,
		DI_AFBCE_PIP_CTRL,
		DI_AFBCE_ROT_CTRL,
	},
	{
		DI_AFBCE_ENABLE,
		DI_AFBCE_MODE,
		DI_AFBCE_SIZE_IN,
		DI_AFBCE_BLK_SIZE_IN,
		DI_AFBCE_HEAD_BADDR,
		DI_AFBCE_MIF_SIZE,
		DI_AFBCE_PIXEL_IN_HOR_SCOPE,
		DI_AFBCE_PIXEL_IN_VER_SCOPE,
		DI_AFBCE_CONV_CTRL,
		DI_AFBCE_MIF_HOR_SCOPE,
		DI_AFBCE_MIF_VER_SCOPE,
		DI_AFBCE_STAT1,
		DI_AFBCE_STAT2,
		DI_AFBCE_FORMAT,
		DI_AFBCE_MODE_EN,
		DI_AFBCE_DWSCALAR,
		DI_AFBCE_DEFCOLOR_1,
		DI_AFBCE_DEFCOLOR_2,
		DI_AFBCE_QUANT_ENABLE,
		DI_AFBCE_IQUANT_LUT_1,
		DI_AFBCE_IQUANT_LUT_2,
		DI_AFBCE_IQUANT_LUT_3,
		DI_AFBCE_IQUANT_LUT_4,
		DI_AFBCE_RQUANT_LUT_1,
		DI_AFBCE_RQUANT_LUT_2,
		DI_AFBCE_RQUANT_LUT_3,
		DI_AFBCE_RQUANT_LUT_4,
		DI_AFBCE_YUV_FORMAT_CONV_MODE,
		DI_AFBCE_DUMMY_DATA,
		DI_AFBCE_CLR_FLAG,
		DI_AFBCE_STA_FLAGT,
		DI_AFBCE_MMU_NUM,
		DI_AFBCE_MMU_RMIF_CTRL1,
		DI_AFBCE_MMU_RMIF_CTRL2,
		DI_AFBCE_MMU_RMIF_CTRL3,
		DI_AFBCE_MMU_RMIF_CTRL4,
		DI_AFBCE_MMU_RMIF_SCOPE_X,
		DI_AFBCE_MMU_RMIF_SCOPE_Y,
		DI_AFBCE_MMU_RMIF_RO_STAT,
		DI_AFBCE_PIP_CTRL,
		DI_AFBCE_ROT_CTRL,
	},
	{
		DI_AFBCE1_ENABLE,
		DI_AFBCE1_MODE,
		DI_AFBCE1_SIZE_IN,
		DI_AFBCE1_BLK_SIZE_IN,
		DI_AFBCE1_HEAD_BADDR,
		DI_AFBCE1_MIF_SIZE,
		DI_AFBCE1_PIXEL_IN_HOR_SCOPE,
		DI_AFBCE1_PIXEL_IN_VER_SCOPE,
		DI_AFBCE1_CONV_CTRL,
		DI_AFBCE1_MIF_HOR_SCOPE,
		DI_AFBCE1_MIF_VER_SCOPE,
		DI_AFBCE1_STAT1,
		DI_AFBCE1_STAT2,
		DI_AFBCE1_FORMAT,
		DI_AFBCE1_MODE_EN,
		DI_AFBCE1_DWSCALAR,
		DI_AFBCE1_DEFCOLOR_1,
		DI_AFBCE1_DEFCOLOR_2,
		DI_AFBCE1_QUANT_ENABLE,
		DI_AFBCE1_IQUANT_LUT_1,
		DI_AFBCE1_IQUANT_LUT_2,
		DI_AFBCE1_IQUANT_LUT_3,
		DI_AFBCE1_IQUANT_LUT_4,
		DI_AFBCE1_RQUANT_LUT_1,
		DI_AFBCE1_RQUANT_LUT_2,
		DI_AFBCE1_RQUANT_LUT_3,
		DI_AFBCE1_RQUANT_LUT_4,
		DI_AFBCE1_YUV_FORMAT_CONV_MODE,
		DI_AFBCE1_DUMMY_DATA,
		DI_AFBCE1_CLR_FLAG,
		DI_AFBCE1_STA_FLAGT,
		DI_AFBCE1_MMU_NUM,
		DI_AFBCE1_MMU_RMIF_CTRL1,
		DI_AFBCE1_MMU_RMIF_CTRL2,
		DI_AFBCE1_MMU_RMIF_CTRL3,
		DI_AFBCE1_MMU_RMIF_CTRL4,
		DI_AFBCE1_MMU_RMIF_SCOPE_X,
		DI_AFBCE1_MMU_RMIF_SCOPE_Y,
		DI_AFBCE1_MMU_RMIF_RO_STAT,
		DI_AFBCE1_PIP_CTRL,
		DI_AFBCE1_ROT_CTRL,

	},
};

static unsigned int set_afbce_cfg_v1(int index,
				     //0:vdin_afbce 1:di_afbce0 2:di_afbce1
				     int enable,
				     //open nbit of afbce
				     struct AFBCE_S  *afbce,
				     const struct reg_acc *opin)
{
	const unsigned int *reg;
	int hold_line_num  = 0; /* version 0523 is 2*/
	int lbuf_depth     = 256;

	int hsize_buf = afbce->reg_pip_mode ?
		afbce->hsize_bgnd : afbce->hsize_in;
	int vsize_buf = afbce->reg_pip_mode ?
		afbce->vsize_bgnd : afbce->vsize_in;

	int hblksize_buf = (hsize_buf    + 31) >> 5;
	int vblksize_buf = (vsize_buf    + 3) >> 2;

	int blk_out_end_h =  (afbce->enc_win_end_h + 31) >> 5;
	//output blk scope
	int blk_out_bgn_h = afbce->enc_win_bgn_h >> 5;
	//output blk scope
	int blk_out_end_v =  (afbce->enc_win_end_v + 3) >> 2;
	//output blk scope
	int blk_out_bgn_v = afbce->enc_win_bgn_v      >> 2;
	//output blk scope

	int        lossy_luma_en;
	int        lossy_chrm_en;
	int        reg_fmt444_comb;//calculate
	int        uncmp_size;//calculate
	int        uncmp_bits;//calculate
	int        sblk_num;//calculate
	int cur_mmu_used = 0;//mmu info linear addr
	const struct reg_acc *op;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;

	reg = &reg_afbc_e_v3[index][0];

	if (afbce->din_swt) {
		index = (index == 1) ? 2 :
			(index == 2) ? 1 : index;//todo
	}
	op->bwr(DI_TOP_CTRL1, (afbce->din_swt ? 3 : 0), 0, 2);
	//only di afbce have ram_comb

	if (index == 0) { //Vdin_afbce

	} else if (index == 1) {
		op->bwr(DI_TOP_CTRL1, afbce->reg_ram_comb, 2, 1);
		//only di afbce have ram_comb
	} else if (index == 2) {
		op->bwr(DI_TOP_CTRL1, afbce->reg_ram_comb, 2, 1);
		//only di afbce have ram_comb
	} else {
		PR_ERR("ERROR:AFBCE INDEX WRONG!!!\n");
	}

	///////////////////////////////////////////////////////////////////////
	//////Afbce configure logic
	///////////////////////////////////////////////////////////////////////

	reg_fmt444_comb = (afbce->reg_format_mode == 0) &&
		(afbce->force_444_comb);
		//yuv444 can only support 8bit,and must use comb_mode

	sblk_num        = (afbce->reg_format_mode == 2) ? 12 :
	//4*4 subblock number in every 32*4 mblk
			(afbce->reg_format_mode == 1) ? 16 : 24;

	uncmp_bits = afbce->reg_compbits_y > afbce->reg_compbits_c ?
		afbce->reg_compbits_y : afbce->reg_compbits_c;

	uncmp_size = (((((16 * uncmp_bits *
			  sblk_num) + 7) >> 3) + 31) / 32) << 1;
			  //bit size of uncompression mode

	if (afbce->loosy_mode == 0) { //chose loosy mode of luma and chroma
		lossy_luma_en = 0;
		lossy_chrm_en = 0;
	} else if (afbce->loosy_mode == 1) {
		lossy_luma_en = 1;
		lossy_chrm_en = 0;
	} else if (afbce->loosy_mode == 2) {
		lossy_luma_en = 0;
		lossy_chrm_en = 1;
	} else {
		lossy_luma_en = 1;
		lossy_chrm_en = 1;
	}
	dim_print("%s:0x%x\n", __func__, reg[AFBCEX_ENABLE]);
	dim_print("\t:w[%d],h[%d]\n", afbce->hsize_in, afbce->vsize_in);
	///////////////////////////////////////////////////////////////////////
	//////Afbce configure registers
	///////////////////////////////////////////////////////////////////////

	op->wr(reg[AFBCEX_MODE],
		(0                & 0x7) << 29 |
		(afbce->rev_mode  & 0x3) << 26 |
		(3                & 0x3) << 24 |
		(hold_line_num    & 0x7f) << 16 |
		(2                & 0x3) << 14 |
		(reg_fmt444_comb  & 0x1));

	op->bwr(reg[AFBCEX_QUANT_ENABLE],
		(lossy_luma_en & 0x1), 0, 1);//loosy
	op->bwr(reg[AFBCEX_QUANT_ENABLE],
		(lossy_chrm_en & 0x1), 4, 1);//loosy

	op->wr(reg[AFBCEX_SIZE_IN],
		((hsize_buf & 0x1fff) << 16) |  // hsize_in of afbc input
		((vsize_buf & 0x1fff) << 0)    // vsize_in of afbc input
		);

	op->wr(reg[AFBCEX_BLK_SIZE_IN],
		((hblksize_buf & 0x1fff) << 16) |  // out blk hsize
		((vblksize_buf & 0x1fff) << 0)    // out blk vsize
		);

	op->wr(reg[AFBCEX_HEAD_BADDR], afbce->head_baddr);
	//head addr of compressed data

	op->bwr(reg[AFBCEX_MIF_SIZE],
		(uncmp_size & 0x1fff), 16, 5);//uncmp_size

	op->wr(reg[AFBCEX_PIXEL_IN_HOR_SCOPE],
	       ((afbce->enc_win_end_h & 0x1fff) << 16) |
	       // scope of hsize_in ,should be a integer multiple of 32
	       ((afbce->enc_win_bgn_h & 0x1fff) << 0)
	       // scope of vsize_in ,should be a integer multiple of 4
	       );

	op->wr(reg[AFBCEX_PIXEL_IN_VER_SCOPE],
	       ((afbce->enc_win_end_v & 0x1fff) << 16) |
	       // scope of hsize_in ,should be a integer multiple of 32
	       ((afbce->enc_win_bgn_v & 0x1fff) << 0));
	       // scope of vsize_in ,should be a integer multiple of 4

	op->wr(reg[AFBCEX_CONV_CTRL], lbuf_depth);//fix 256

	op->wr(reg[AFBCEX_MIF_HOR_SCOPE],
	       ((blk_out_end_h & 0x3ff) << 16) |  // scope of out blk hsize
	       ((blk_out_bgn_h & 0x3ff) << 0));    // scope of out blk vsize

	op->wr(reg[AFBCEX_MIF_VER_SCOPE],
	       ((blk_out_end_v & 0xfff) << 16) |  // scope of out blk hsize
	       ((blk_out_bgn_v & 0xfff) << 0));    // scope of out blk vsize

	op->wr(reg[AFBCEX_FORMAT],
	       (afbce->reg_format_mode  & 0x3) << 8 |
	       (afbce->reg_compbits_c   & 0xf) << 4 |
	       (afbce->reg_compbits_y   & 0xf));

	op->wr(reg[AFBCEX_DEFCOLOR_1],
	       ((afbce->def_color_3 & 0xfff) << 12) |  // def_color_a
	       ((afbce->def_color_0 & 0xfff) << 0));    // def_color_y

	op->wr(reg[AFBCEX_DEFCOLOR_2],
	       ((afbce->def_color_2 & 0xfff) << 12) |  // def_color_v
	       ((afbce->def_color_1 & 0xfff) << 0));    // def_color_u

//ary temp	cur_mmu_used += op->rd(AFBCE_MMU_NUM);
//4k addr have used in every frame;

	op->wr(reg[AFBCEX_MMU_RMIF_CTRL4], afbce->mmu_info_baddr);
	op->bwr(reg[AFBCEX_MMU_RMIF_CTRL1], 0x1, 6, 1);//litter_endia
	if (afbce->reg_pip_mode)
		op->bwr(reg[AFBCEX_MMU_RMIF_SCOPE_X], 0x0, 0, 13);
	else
		op->bwr(reg[AFBCEX_MMU_RMIF_SCOPE_X], cur_mmu_used, 0, 12);
	op->bwr(reg[AFBCEX_MMU_RMIF_SCOPE_X], 0x1ffe, 16, 13);
	op->bwr(reg[AFBCEX_MMU_RMIF_CTRL3], 0x1fff, 0, 13);

	op->bwr(reg[AFBCEX_PIP_CTRL], afbce->reg_init_ctrl, 1, 1);//pii_moide
	op->bwr(reg[AFBCEX_PIP_CTRL], afbce->reg_pip_mode, 0, 1);

	op->bwr(reg[AFBCEX_ROT_CTRL], afbce->rot_en, 4, 1);

	op->bwr(reg[AFBCEX_ENABLE], 0, 12, 1);//go_line_cnt start
	op->bwr(reg[AFBCEX_ENABLE], enable, 8, 1);//enable afbce
	op->bwr(reg[AFBCEX_ENABLE], enable, 0, 1);//enable afbce
	return 0;

} /* set_afbce_cfg_v1 */

static void set_shrk(struct SHRK_S *srkcfg, const struct reg_acc *opin)
{
	const struct reg_acc *op;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;

	op->wr((DI_DIWR_SHRK_CTRL),
		((srkcfg->h_shrk_mode & 0x3) << 8)  |
		((srkcfg->v_shrk_mode & 0x3) << 6)  |
		((srkcfg->shrk_en & 0x1)     << 0)
		);

/*	stimulus_display("==hshrk_mode== %0x\n", srkcfg->h_shrk_mode);*/
/*	stimulus_display("==vshrk_mode== %0x\n", srkcfg->v_shrk_mode);*/

	op->wr((DI_DIWR_SHRK_SIZE),
		((srkcfg->hsize_in & 0x1fff) << 13) |  // reg_frm_hsize
		((srkcfg->vsize_in & 0x1fff) << 0)    // reg_frm_vsize
		);
	op->bwr(DI_DIWR_SHRK_CTRL, srkcfg->frm_rst, 1, 1);
}

static void set_shrk_disable(void)
{
	const struct reg_acc *op = &di_pre_regset;

	op->bwr(DI_DIWR_SHRK_CTRL, 0, 0, 1);
}

static void set_mcdi_mif(struct DI_MIF1_S *di_inf_default,
			 int hsize, int vsize, const struct reg_acc *op)
{
	int addr_x = (hsize + 4) / 5 - 1;
	//(di_inf_default->start_x << 16)| ((di_inf_default->end_x+4)/5);
	int addr_y = vsize - 1;
	int addrinfo_x = (vsize) / 2 - 1;
	//(di_inf_default->start_y << 16)|
	//((di_inf_default->end_y-di_inf_default->start_y) / 2);
	int addrinfo_y = 1;
	//(di_inf_default->start_y << 16)| (di_inf_default->start_y+1);

	// mcdi canvas
	op->wr(MCVECWR_X, (2 << 30) | addr_x);
	op->wr(MCVECWR_Y, addr_y);
	op->wr(MCINFWR_X, (2 << 30) | addrinfo_x);
	op->wr(MCINFWR_Y, addrinfo_y);
	op->wr(MCVECRD_SCOPE_X, (addr_x) << 16);
	op->wr(MCVECRD_SCOPE_Y, (addr_y) << 16);
	op->wr(MCINFRD_SCOPE_X, (addrinfo_x) << 16);
	op->wr(MCINFRD_SCOPE_Y, (addrinfo_y) << 16);
	op->wr(MCVECWR_CAN_SIZE, (addr_x << 16) | addr_y);
	op->wr(MCINFWR_CAN_SIZE, (addrinfo_x << 16) | addrinfo_y);

	op->wr(MCVECWR_CTRL, op->rd(MCVECWR_CTRL) | (0 << 14)
		// sync latch en
		| (0 << 12));           // enable
	op->wr(MCINFWR_CTRL, op->rd(MCINFWR_CTRL) | (0 << 14)
		// sync latch en
		| (0 << 12));           // enable
}

static void set_mcdi_def(struct DI_MIF1_S *di_inf_default,
			 int hsizem1, int vsizem1,
			 const struct reg_acc *op)
{
	int hsize = hsizem1 + 1;
	//(di_inf_default->end_x - di_inf_default->start_x)+ 1;
	int vsize = vsizem1 + 1;
	//(di_inf_default->end_y - di_inf_default->start_y)+ 1;
	int blkhsize = (hsize + 4) / 5;

	op->wr(MCDI_HV_SIZEIN, ((hsize << 16) + vsize));
	op->wr(MCDI_HV_BLKSIZEIN, ((blkhsize << 16) + vsize));
	op->wr(MCDI_BLKTOTAL, blkhsize * vsize);
	op->wr(MCDI_MOTINEN, 1 << 1);
	//enable motin refinement ary:in dimh_enable_mc_di_pre_g12
	if (!DIM_IS_IC(SC2))
		op->wr(MCDI_REF_MV_NUM, 2);
	//ary : in mc_di_param_init
	op->wr(MCDI_CTRL_MODE, op->rd(MCDI_CTRL_MODE) |
	//ary : dimh_mc_pre_mv_irq
		(0 << 28) |   // close linf
		(1 << 16) |   // qme
		(1 << 9));   // ref
	//stimulus_print("MCDI DEFAULT SETTING\n");
	op->wr(MCDI_MC_CRTL, op->rd(MCDI_MC_CRTL) & 0xfffffffc);  //close mc
	set_mcdi_mif(di_inf_default, hsize, vsize, op);
}

static void set_mcdi_pre(struct DI_MIF1_S *di_mcinford_mif,
			 struct DI_MIF1_S *di_mcvecwr_mif,
			 struct DI_MIF1_S *di_mcinfowr_mif,
			 const struct reg_acc *op)
{
	op->wr(MCINFRD_CTRL1, di_mcinford_mif->canvas_num << 16 | //canvas index
		2 << 8 | //burst len = 2
		0 << 6 | //little endian
		2 << 0);//pack mode
	op->wr(MCINFWR_CTRL, (op->rd(MCINFWR_CTRL) & 0xffffff00) |
		di_mcinfowr_mif->canvas_num |  // canvas index.
		(1 << 12));       // req enable
	op->wr(MCVECWR_CTRL, (op->rd(MCVECWR_CTRL) & 0xffffff00) |
		di_mcvecwr_mif->canvas_num |  // canvas index.
		(1 << 12));       // req enable

	op->bwr(DI_PRE_CTRL, 3, 16, 2);// me enable; auto enable
}

static void set_mcdi_post(struct DI_MIF1_S *di_mcvecrd_mif,
			  const struct reg_acc *op)
{
	op->wr(MCVECRD_CTRL1, di_mcvecrd_mif->canvas_num << 16 | //canvas index
		2 << 8 | //burst len = 2
		0 << 6 | //little endian
		2 << 0);//pack mode
}

static void set_cont_mif(struct DI_MIF1_S *di_contprd_mif,
			 struct DI_MIF1_S *di_contp2rd_mif,
			 struct DI_MIF1_S *di_contwr_mif,
			 const struct reg_acc *op)
{
	int contwr_hsize = di_contwr_mif->end_x - di_contwr_mif->start_x + 1;
	int contwr_vsize = di_contwr_mif->end_y - di_contwr_mif->start_y + 1;

	//stimulus_print("enter cont bus config\n");
	op->wr(CONTWR_X, (2 << 30) | (di_contwr_mif->start_x << 16) |
	       (di_contwr_mif->end_x));   // start_x 0 end_x 719.
	op->wr(CONTWR_Y, (di_contwr_mif->start_y << 16) |
	       (di_contwr_mif->end_y));   // start_y 0 end_y 239.
	op->wr(CONTWR_CAN_SIZE, ((contwr_hsize - 1) << 16) |
	       (contwr_vsize - 1));
	op->wr(CONTWR_CTRL,  di_contwr_mif->canvas_num |  // canvas index.
		(1 << 12));       // req_en.

	op->wr(CONTRD_SCOPE_X, (di_contprd_mif->start_x)  |
	       (di_contprd_mif->end_x << 16));   // start_x 0 end_x 719.
	op->wr(CONTRD_SCOPE_Y, (di_contprd_mif->start_y)  |
	       (di_contprd_mif->end_y << 16));   // start_y 0 end_y 239.
	op->wr(CONT2RD_SCOPE_X, (di_contp2rd_mif->start_x) |
	       (di_contp2rd_mif->end_x << 16));   // start_x 0 end_x 719.
	op->wr(CONT2RD_SCOPE_Y, (di_contp2rd_mif->start_y) |
	       (di_contp2rd_mif->end_y << 16)); // start_y 0 end_y 239.
	op->wr(CONTRD_CTRL1, di_contprd_mif->canvas_num << 16 |
	       //canvas index
	       2 << 8 | //burst len = 2
	       0 << 6 | //little endian
	       0 << 0);//pack mode
	op->wr(CONT2RD_CTRL1, di_contp2rd_mif->canvas_num << 16 |
	       //canvas index
	       2 << 8 | //burst len = 2
	       0 << 6 | //little endian
	       0 << 0);//pack mode
}

static unsigned int di_mif_add_get_offset_v3(enum DI_MIF0_ID mif_index)
{
	unsigned int addr = 0;
	unsigned int index = 0;

	switch (mif_index) {
	case DI_MIF0_ID_INP:
		index = 0;
		break;
	case DI_MIF0_ID_CHAN2:
		index = 1;
		break;
	case DI_MIF0_ID_MEM:
		index = 2;
		break;
	case DI_MIF0_ID_IF1:
		index = 3;
		break;
	case DI_MIF0_ID_IF0:
		index = 4;
		break;
	case DI_MIF0_ID_IF2:
		index = 5;
		break;

		break;
	default:
		addr = DIM_ERR;
	};

	if (addr == DIM_ERR)
		return addr;

	//addr = (index<<2)*0x80;
	addr = index * 0x80;

	return addr;
}

static const unsigned int mif_contr_reg_v3[MIF_REG_NUB] = {
		RDMIFXN_GEN_REG,
		RDMIFXN_GEN_REG2,
		RDMIFXN_GEN_REG3,
		RDMIFXN_CANVAS0,
		RDMIFXN_LUMA_X0,
		RDMIFXN_LUMA_Y0,
		RDMIFXN_CHROMA_X0,
		RDMIFXN_CHROMA_Y0,
		RDMIFXN_RPT_LOOP,
		RDMIFXN_LUMA0_RPT_PAT,
		RDMIFXN_CHROMA0_RPT_PAT,
		RDMIFXN_DUMMY_PIXEL,
		RDMIFXN_CFMT_CTRL,
		RDMIFXN_CFMT_W,
		RDMIFXN_LUMA_FIFO_SIZE,
};

static const unsigned int *mif_reg_get_v3(void)
{
	return &mif_contr_reg_v3[0];
}

static const unsigned int mif_contr_reg[MIF_NUB][MIF_REG_NUB] = {
	[DI_MIF0_ID_INP] = {
		DI_SC2_INP_GEN_REG,
		DI_SC2_INP_GEN_REG2,
		DI_SC2_INP_GEN_REG3,
		DI_SC2_INP_CANVAS0,
		DI_SC2_INP_LUMA_X0,
		DI_SC2_INP_LUMA_Y0,
		DI_SC2_INP_CHROMA_X0,
		DI_SC2_INP_CHROMA_Y0,
		DI_SC2_INP_RPT_LOOP,
		DI_SC2_INP_LUMA0_RPT_PAT,
		DI_SC2_INP_CHROMA0_RPT_PAT,
		DI_SC2_INP_DUMMY_PIXEL,
		DI_SC2_INP_CFMT_CTRL,
		DI_SC2_INP_CFMT_W,
		DI_SC2_INP_LUMA_FIFO_SIZE},
	[DI_MIF0_ID_MEM] = {
		DI_SC2_MEM_GEN_REG,
		DI_SC2_MEM_GEN_REG2,
		DI_SC2_MEM_GEN_REG3,
		DI_SC2_MEM_CANVAS0,
		DI_SC2_MEM_LUMA_X0,
		DI_SC2_MEM_LUMA_Y0,
		DI_SC2_MEM_CHROMA_X0,
		DI_SC2_MEM_CHROMA_Y0,
		DI_SC2_MEM_RPT_LOOP,
		DI_SC2_MEM_LUMA0_RPT_PAT,
		DI_SC2_MEM_CHROMA0_RPT_PAT,
		DI_SC2_MEM_DUMMY_PIXEL,
		DI_SC2_MEM_CFMT_CTRL,
		DI_SC2_MEM_CFMT_W,
		DI_SC2_MEM_LUMA_FIFO_SIZE
		},
	[DI_MIF0_ID_CHAN2] = {
		DI_SC2_CHAN2_GEN_REG,
		DI_SC2_CHAN2_GEN_REG2,
		DI_SC2_CHAN2_GEN_REG3,
		DI_SC2_CHAN2_CANVAS0,
		DI_SC2_CHAN2_LUMA_X0,
		DI_SC2_CHAN2_LUMA_Y0,
		DI_SC2_CHAN2_CHROMA_X0,
		DI_SC2_CHAN2_CHROMA_Y0,
		DI_SC2_CHAN2_RPT_LOOP,
		DI_SC2_CHAN2_LUMA0_RPT_PAT,
		DI_SC2_CHAN2_CHROMA0_RPT_PAT,
		DI_SC2_CHAN2_DUMMY_PIXEL,
		DI_SC2_CHAN2_CFMT_CTRL,
		DI_SC2_CHAN2_CFMT_W,
		DI_SC2_CHAN2_LUMA_FIFO_SIZE
		},
	[DI_MIF0_ID_IF0] = {
		DI_SC2_IF0_GEN_REG,
		DI_SC2_IF0_GEN_REG2,
		DI_SC2_IF0_GEN_REG3,
		DI_SC2_IF0_CANVAS0,
		DI_SC2_IF0_LUMA_X0,
		DI_SC2_IF0_LUMA_Y0,
		DI_SC2_IF0_CHROMA_X0,
		DI_SC2_IF0_CHROMA_Y0,
		DI_SC2_IF0_RPT_LOOP,
		DI_SC2_IF0_LUMA0_RPT_PAT,
		DI_SC2_IF0_CHROMA0_RPT_PAT,
		DI_SC2_IF0_DUMMY_PIXEL,
		DI_SC2_IF0_CFMT_CTRL,
		DI_SC2_IF0_CFMT_W,
		DI_SC2_IF0_LUMA_FIFO_SIZE,
		},
	[DI_MIF0_ID_IF1] = {
		DI_SC2_IF1_GEN_REG,
		DI_SC2_IF1_GEN_REG2,
		DI_SC2_IF1_GEN_REG3,
		DI_SC2_IF1_CANVAS0,
		DI_SC2_IF1_LUMA_X0,
		DI_SC2_IF1_LUMA_Y0,
		DI_SC2_IF1_CHROMA_X0,
		DI_SC2_IF1_CHROMA_Y0,
		DI_SC2_IF1_RPT_LOOP,
		DI_SC2_IF1_LUMA0_RPT_PAT,
		DI_SC2_IF1_CHROMA0_RPT_PAT,
		DI_SC2_IF1_DUMMY_PIXEL,
		DI_SC2_IF1_CFMT_CTRL,
		DI_SC2_IF1_CFMT_W,
		DI_SC2_IF1_LUMA_FIFO_SIZE
		},
	[DI_MIF0_ID_IF2] = {
		DI_SC2_IF2_GEN_REG,
		DI_SC2_IF2_GEN_REG2,
		DI_SC2_IF2_GEN_REG3,
		DI_SC2_IF2_CANVAS0,
		DI_SC2_IF2_LUMA_X0,
		DI_SC2_IF2_LUMA_Y0,
		DI_SC2_IF2_CHROMA_X0,
		DI_SC2_IF2_CHROMA_Y0,
		DI_SC2_IF2_RPT_LOOP,
		DI_SC2_IF2_LUMA0_RPT_PAT,
		DI_SC2_IF2_CHROMA0_RPT_PAT,
		DI_SC2_IF2_DUMMY_PIXEL,
		DI_SC2_IF2_CFMT_CTRL,
		DI_SC2_IF2_CFMT_W,
		DI_SC2_IF2_LUMA_FIFO_SIZE
		},
};

const unsigned int *mif_reg_getb_v3(enum DI_MIF0_ID mif_index)
{
	if (mif_index > DI_MIF0_ID_IF2)
		return NULL;

	return &mif_contr_reg[mif_index][0];
}

static void set_di_mif_v1(struct DI_MIF_S *mif,
			  enum DI_MIF0_ID mif_index, const struct reg_acc *op)
{
	u32 bytes_per_pixel = 0;
	//0 = 1 byte per pixel, 1 = 2 bytes per pixel, 2 = 3 bytes per pixel
	u32 demux_mode			= 0;
	u32 chro_rpt_lastl_ctrl	= 0;
	u32 luma0_rpt_loop_start	= 0;
	u32 luma0_rpt_loop_end		= 0;
	u32 luma0_rpt_loop_pat		= 0;
	u32 chroma0_rpt_loop_start	= 0;
	u32 chroma0_rpt_loop_end	= 0;
	u32 chroma0_rpt_loop_pat	= 0;
	int      hfmt_en      = 0;
	int      hz_yc_ratio  = 0;
	int      hz_ini_phase = 0;
	int      vfmt_en      = 0;
	int      vt_yc_ratio  = 0;
	int      vt_ini_phase = 0;
	int      y_length     = 0;
	int      c_length     = 0;
	int      hz_rpt       = 0;
	int vt_phase_step	= 0;// = (16 >> vt_yc_ratio);
	int urgent = mif->urgent;
	int hold_line = mif->hold_line;
	unsigned int off;

	off = di_mif_add_get_offset_v3(mif_index);
	if (off == DIM_ERR) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	if (mif->set_separate_en != 0 && mif->src_field_mode == 1) {
		if (mif->video_mode == 0)
			chro_rpt_lastl_ctrl = 1;
		else
			chro_rpt_lastl_ctrl = 0;

		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 1;
		chroma0_rpt_loop_end = 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x80;
	} else if (mif->set_separate_en != 0 && mif->src_field_mode == 0) {
		if (mif->video_mode == 0)
			chro_rpt_lastl_ctrl = 1;
		else
			chro_rpt_lastl_ctrl = 0;

		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x0;
		chroma0_rpt_loop_pat = 0x0;
	} else if (mif->set_separate_en == 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x00;
		op->wr(off + RDMIFXN_LUMA_FIFO_SIZE, 0xc0);
	} else {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x00;
		chroma0_rpt_loop_pat = 0x00;
		op->wr(off + RDMIFXN_LUMA_FIFO_SIZE, 0xc0);
	}

	bytes_per_pixel = (mif->set_separate_en) ?
		0 : ((mif->video_mode == 2) ? 2 : 1);

	bytes_per_pixel = mif->bit_mode == 3 ?
		1 : bytes_per_pixel;// 10bit full pack or not

	demux_mode = (mif->set_separate_en == 0) ?
		((mif->video_mode == 1) ? 0 :  1) : 0;

	// ----------------------
	// General register
	// ----------------------
	op->wr(off + RDMIFXN_GEN_REG3,
		mif->bit_mode << 8 |    // bits_mode
		3 << 4 |    // block length
		2 << 1 |    // use bst4
		1 << 0);   //64 bit swap

	op->wr(off + RDMIFXN_GEN_REG,
		(urgent << 28)             | // chroma urgent bit
		(urgent << 27)             | // luma urgent bit.
		(1 << 25)                  | // no dummy data.
		(hold_line << 19)          | // hold lines
		(1 << 18)                  | // push dummy pixel
		(demux_mode << 16)         | // demux_mode
		(bytes_per_pixel << 14)    |
		(mif->burst_size_cr << 12) |
		(mif->burst_size_cb << 10) |
		(mif->burst_size_y << 8)   |
		(chro_rpt_lastl_ctrl << 6) |
		((mif->set_separate_en != 0) << 1)      |
		(1 << 0)                     // cntl_enable
		);

	if (mif->set_separate_en == 2) {
		// Enable NV12 Display
		op->wr(off + RDMIFXN_GEN_REG2, 1);
	}

	// reverse X and Y
	op->bwr(off + RDMIFXN_GEN_REG2, ((mif->rev_y << 1) |
		(mif->rev_x)), 2, 2);

	// ----------------------
	// Canvas
	// ----------------------
	if (mif->linear)
		di_mif0_linear_rd_cfg(mif, mif_index, op);
	else
		op->wr(off + RDMIFXN_CANVAS0, (mif->canvas0_addr2 << 16)     |
		 // cntl_canvas0_addr2
			(mif->canvas0_addr1 << 8)      | // cntl_canvas0_addr1
			(mif->canvas0_addr0 << 0)        // cntl_canvas0_addr0
			);

	// ----------------------
	// Picture 0 X/Y start,end
	// ----------------------
	op->wr(off + RDMIFXN_LUMA_X0, (mif->luma_x_end0 << 16)       |
	 // cntl_luma_x_end0
		(mif->luma_x_start0 << 0)        // cntl_luma_x_start0
		);
	op->wr(off + RDMIFXN_LUMA_Y0, (mif->luma_y_end0 << 16)       |
	 // cntl_luma_y_end0
		(mif->luma_y_start0 << 0)        // cntl_luma_y_start0
		);
	op->wr(off + RDMIFXN_CHROMA_X0, (mif->chroma_x_end0 << 16)      |
		(mif->chroma_x_start0 << 0)
		);
	op->wr(off + RDMIFXN_CHROMA_Y0, (mif->chroma_y_end0 << 16)      |
		(mif->chroma_y_start0 << 0)
		);

	// ----------------------
	// Repeat or skip
	// ----------------------
	op->wr(off + RDMIFXN_RPT_LOOP,        (0 << 28) |
		(0   << 24) |
		(0   << 20) |
		(0     << 16) |
		(chroma0_rpt_loop_start << 12) |
		(chroma0_rpt_loop_end   << 8)  |
		(luma0_rpt_loop_start   << 4)  |
		(luma0_rpt_loop_end     << 0));

	op->wr(off + RDMIFXN_LUMA0_RPT_PAT,      luma0_rpt_loop_pat);
	op->wr(off + RDMIFXN_CHROMA0_RPT_PAT,    chroma0_rpt_loop_pat);

	// Dummy pixel value
	op->wr(off + RDMIFXN_DUMMY_PIXEL,   0x00808000);
	if (mif->video_mode == 0)   {// 4:2:0 block mode.
		hfmt_en      = 1;
		hz_yc_ratio  = 1;
		hz_ini_phase = 0;
		vfmt_en      = 1;
		vt_yc_ratio  = 1;
		vt_ini_phase = 0;
		y_length     = mif->luma_x_end0 - mif->luma_x_start0 + 1;
		c_length     = mif->chroma_x_end0 - mif->chroma_x_start0 + 1;
		hz_rpt       = 0;
	} else if (mif->video_mode == 1) {
		hfmt_en      = 1;
		hz_yc_ratio  = 1;
		hz_ini_phase = 0;
		vfmt_en      = 0;
		vt_yc_ratio  = 0;
		vt_ini_phase = 0;
		y_length     = mif->luma_x_end0 - mif->luma_x_start0 + 1;
		c_length     = ((mif->luma_x_end0 >> 1) -
			(mif->luma_x_start0 >> 1) + 1);
		hz_rpt       = 0;
	} else if (mif->video_mode == 2) {
		hfmt_en      = 0;
		hz_yc_ratio  = 1;
		hz_ini_phase = 0;
		vfmt_en      = 0;
		vt_yc_ratio  = 0;
		vt_ini_phase = 0;
		y_length     = mif->luma_x_end0 - mif->luma_x_start0 + 1;
		c_length     = mif->luma_x_end0 - mif->luma_x_start0 + 1;
		hz_rpt       = 0;
	}
	vt_phase_step = (16 >> vt_yc_ratio);
	op->wr(off + RDMIFXN_CFMT_CTRL,
		(hz_rpt << 28)       |     //hz rpt pixel
		(hz_ini_phase << 24) |     //hz ini phase
		(0 << 23)         |        //repeat p0 enable
		(hz_yc_ratio << 21)  |     //hz yc ratio
		(hfmt_en << 20)   |        //hz enable
		(1 << 17)         |        //nrpt_phase0 enable
		(0 << 16)         |        //repeat l0 enable
		(0 << 12)         |        //skip line num
		(vt_ini_phase << 8)  |     //vt ini phase
		(vt_phase_step << 1) |     //vt phase step (3.4)
		(vfmt_en << 0)             //vt enable
		);

	op->wr(off + RDMIFXN_CFMT_W,    (y_length << 16)        |
		//hz format width
		(c_length << 0)                  //vt format width
		);
}

static const unsigned int reg_wrmif_v3
	[DIM_WRMIF_MIF_V3_NUB][DIM_WRMIF_SET_V3_NUB] = {
	[EDI_MIFSM_NR] = {
		DI_SC2_NRWR_X,
		DI_SC2_NRWR_Y,
		DI_SC2_NRWR_CTRL,
		DI_NRWR_URGENT,
		DI_NRWR_CANVAS,
		NRWR_DBG_AXI_CMD_CNT,
		NRWR_DBG_AXI_DAT_CNT,
	},
	[EDI_MIFSM_WR] = {
		DI_SC2_DIWR_X,
		DI_SC2_DIWR_Y,
		DI_SC2_DIWR_CTRL,
		DI_DIWR_URGENT,
		DI_DIWR_CANVAS,
		DIWR_DBG_AXI_CMD_CNT,
		DIWR_DBG_AXI_DAT_CNT,
	},
};

/* keep order with ENR_MIF_INDEX*/
static const struct regs_t reg_bits_wr[] = {
	{WRMIF_X,  16,  16, ENR_MIF_INDEX_X_ST, "x_start"},
	{WRMIF_X,  0,  16, ENR_MIF_INDEX_X_END, "x_end"},
	{WRMIF_Y,  16,  16, ENR_MIF_INDEX_Y_ST, "y_start"},
	{WRMIF_Y,  0,  16, ENR_MIF_INDEX_Y_END, "y_end"},
	{WRMIF_CANVAS, 0,  32, ENR_MIF_INDEX_CVS, "canvas"},
	{WRMIF_CTRL, 0,  1, ENR_MIF_INDEX_EN, "mif_en"},
	{WRMIF_CTRL, 1,  1, ENR_MIF_INDEX_BIT_MODE, "10bit mode"},
	{WRMIF_CTRL, 2,  1, ENR_MIF_INDEX_ENDIAN, "endian"},
	{WRMIF_CTRL, 16,  1, ENR_MIF_INDEX_URGENT, "urgent"},
	{WRMIF_CTRL, 17,  1, ENR_MIF_INDEX_CBCR_SW, "cbcr_sw"},
	{WRMIF_CTRL, 18,  2, ENR_MIF_INDEX_VCON, "vcon"},
	{WRMIF_CTRL, 20,  2, ENR_MIF_INDEX_HCON, "hcon"},
	{WRMIF_CTRL, 22,  3, ENR_MIF_INDEX_RGB_MODE, "rgb_mode"},
	/*below for sc2*/
	{WRMIF_DBG_AXI_CMD_CNT, 22,  3,
	 ENR_MIF_INDEX_DBG_CMD_CNT, "dbg_cmd_cnt"},
	{WRMIF_DBG_AXI_DAT_CNT, 22,  3,
	 ENR_MIF_INDEX_DBG_DAT_CNT, "dbg_dat_cnt"},
	{TABLE_FLG_END, TABLE_FLG_END, 0xff, 0xff, "end"}
};

static const struct reg_t rtab_sc2_contr_bits_tab[] = {
	/*--------------------------*/
	{DI_TOP_PRE_CTRL, 0, 2, 0, "DI_TOP_PRE_CTRL",
		"nrwr_path_sel",
		"0: none; 1: normal;2: afbce"},
	{DI_TOP_PRE_CTRL, 30, 2, 0, "",
			"pre_frm_sel",
			"0:internal  1:pre-post link  2:viu  3:vcp(vdin)"},
	{DI_TOP_PRE_CTRL, 2, 2, 0, "",
			"afbce_path_se",
			""},
	{DI_TOP_PRE_CTRL, 4, 1, 0, "",
			"afbc_vd_sel:inp",
			"0:mif; 1:afbc dec"},
	{DI_TOP_PRE_CTRL, 5, 1, 0, "",
			"afbc_vd_sel:chan2",
			"0:mif; 1:afbc dec"},
	{DI_TOP_PRE_CTRL, 6, 1, 0, "",
			"afbc_vd_sel:mem",
			"0:mif; 1:afbc dec"},
	{DI_TOP_PRE_CTRL, 7, 1, 0, "",
			"di inp afbc dec 4K size",
			""},
	{DI_TOP_PRE_CTRL, 8, 1, 0, "",
			"di chan2 afbc dec 4K size",
			""},
	{DI_TOP_PRE_CTRL, 9, 1, 0, "",
			"di mem afbc dec 4K size",
			""},
	{DI_TOP_PRE_CTRL, 10, 1, 0, "",
			"nr_ch0_en",
			"1:normal?"},
	{DI_TOP_PRE_CTRL, 12, 3, 0, "",
			"pre_bypass_sel",
			"1:inp; 2:chan2; 3mem; other:"},
	{DI_TOP_PRE_CTRL, 16, 3, 0, "",
			"pre_bypass_mode",
			"0:din0;1:din1;2:din2"},
	{DI_TOP_PRE_CTRL, 19, 1, 0, "",
			"pre_bypass_en",
			""},
	{DI_TOP_PRE_CTRL, 20, 2, 0, "",
			"fix_disable_pre",
			"0"},
	/***********************************************/
	{DI_TOP_POST_CTRL, 0, 2, 0, "DI_TOP_POST_CTRL",
			"diwr_path_sel",
			"0: none; 1: normal;2: afbce"},
	{DI_TOP_POST_CTRL, 30, 2, 0, "",
			"post_frm_sel",
			"0:viu  1:internal  2:pre-post link (post timming)"},
	{DI_TOP_POST_CTRL, 4, 3, 0, "",
			"afbc_vd_sel",
			"0:normal; 7:afb_en"},
	{DI_TOP_POST_CTRL, 12, 8, 0, "",
			"post_bypass_ctrl",
			"0"},
	{DI_TOP_POST_CTRL, 20, 2, 0, "",
			"fix_disable_post",
			"0"},
	/***********************************************/
	{DI_PRE_CTRL, 11, 2, 0, "DI_PRE_CTRL",
			"tfbf_en",
			"1: only tfbf; 2: normal;3: normal + tfbf"},
	{DI_PRE_CTRL, 10, 1, 0, "",
			"nr_wr_by",
			"?"},
	{DI_PRE_CTRL, 7, 1, 0, "",
			"nr_wr_by",
			"?"},
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
	{DI_POST_CTRL, 0, 1, 0, "DI_POST_CTRL",
			"post_en",
			""},
	{DI_POST_CTRL, 1, 1, 0, "",
			"blend_en",
			""},
	{DI_POST_CTRL, 2, 1, 0, "",
			"ei_en",
			""},
	{DI_POST_CTRL, 3, 1, 0, "",
			"mux_en",
			""},
	{DI_POST_CTRL, 4, 1, 0, "",
			"ddr_en",
			"wr_bk_en"},
	{DI_POST_CTRL, 5, 1, 0, "",
			"vpp_out_en",
			""},
	{DI_POST_CTRL, 6, 1, 0, "",
			"post mb en",
			"0"},
	/***********************************************/
	{DI_TOP_CTRL, 0, 1, 0, "DI_TOP_CTRL",
			"Vpp path sel",
			"0:post to vpp; 1: pre to vpp"},
	{DI_TOP_CTRL, 4, 3, 0, "",
			"inp",
			""},
	{DI_TOP_CTRL, 7, 3, 0, "",
			"chan2",
			""},
	{DI_TOP_CTRL, 10, 3, 0, "",
			"mem",
			""},
	{DI_TOP_CTRL, 13, 3, 0, "",
			"if1",
			""},
	{DI_TOP_CTRL, 16, 3, 0, "",
			"if0",
			""},
	{DI_TOP_CTRL, 19, 3, 0, "",
			"if2",
			""},
	{TABLE_FLG_END, 0, 0, 0, "end", "end", ""},

};

static void set_wrmif_simple
			(int index,
			 int enable,
			 struct DI_MIF_S  *wr_mif, const struct reg_acc *opin)
{
	const unsigned int *reg;
	const struct reg_acc *op;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;

	////////////////////////////
	//////Afbce addreess mux
	////////////////////////////
	if (index > (DIM_WRMIF_MIF_V3_NUB - 1)) {
		stimulus_print("ERROR:WR_MIF WRONG!!!\n");
		return;
	}

	reg = &reg_wrmif_v3[index][0];

	//////////////////////////////
	//////Afbce Write registers
	/////////////////////////////

	// set wr mif interface.
	//Wr(WRMIF_X     ,
	//Rd(WRMIF_X) | (wr_mif->luma_x_start0 <<16) | (wr_mif->luma_x_end0));
	// start_x 0 end_x 719.
	//Wr(WRMIF_Y     ,
	//Rd(WRMIF_Y) | (wr_mif->luma_y_start0 <<16) | (wr_mif->luma_y_end0));
	// start_y 0 end_y 239.
	op->wr(reg[WRMIF_X],
	       (wr_mif->luma_x_start0 << 16) | (wr_mif->luma_x_end0));
	  // start_x 0 end_x 719.
	op->wr(reg[WRMIF_Y],
	       (wr_mif->luma_y_start0 << 16) | (wr_mif->luma_y_end0));
	  // start_y 0 end_y 239.
	//printf("wr_mif_scope = %d %d %d %d",
	//wr_mif->luma_x_start0,wr_mif->luma_x_end0,
	//wr_mif->luma_y_start0,wr_mif->luma_y_end0);

	op->wr(reg[WRMIF_CANVAS], wr_mif->canvas0_addr0);
	op->wr(reg[WRMIF_CTRL], (enable << 0) |   // write mif en.
		(0      << 1) |   // bit10 mode
		(0      << 2) |   // little endian
		(0      << 3) |   // data ext enable
		(5      << 4) |   // word limit
		(0      << 16) |   // urgent
		(0 << 17) |   // swap cbcrworking in rgb mode =2: swap cbcr
		(0      << 18) |   // vconv working in rgb mode =2:
		(0      << 20) |   // hconv. output even pixel
		(1      << 22) |   // rgb mode =0, 422 YCBCR to one canvas.
		(0      << 24) |   // no gate clock
		(0      << 25) |   // canvas_sync enable
		(2      << 26) |   // burst lim
		(1      << 30));   // 64-bits swap enable
}

static void set_wrmif_simple_v3(struct DI_SIM_MIF_s *mif,
				const struct reg_acc *ops,
				enum EDI_MIFSM mifsel)
{
	const unsigned int *reg;
	unsigned int bits_mode, rgb_mode;
	const struct reg_acc *op;

	////////////////////////////
	//////Afbce addreess mux
	////////////////////////////
	if (mifsel > (DIM_WRMIF_MIF_V3_NUB - 1)) {
		PR_ERR("%s:%d\n", __func__, mifsel);
		return;
	}
	if (!ops)
		op = &di_pre_regset;
	else
		op = ops;

	if (is_mask(SC2_REG_MSK_nr))
		op = &sc2reg;

	mif->en = 1; //ary add temp;
	bits_mode = mif->bit_mode != 0;
	//1: 10bits 422(old mode)   0:8bits   2:10bit 444  3:10bit full-pack
	// rgb_mode	0: 422 to one canvas
	//1: 444 to one canvas
	//2: 8bit Y to one canvas, 16bit UV to another canvas
	//3: 422 full pack mode (10bit)
	#ifdef MARK_SC2
	rgb_mode = mif->bits_mode == 3 ? 3 :	     //422 10bit
		(mif->set_separate_en == 0 ? (mif->video_mode == 1 ? 0 : 1) :
		((mif->video_mode == 1) ? 2 : 3)); //two canvas
	#endif
	if (mif->bit_mode == 3) {
		rgb_mode = 3;
	} else {
		if (mif->set_separate_en == 0) {
			if (mif->video_mode == 1)
				rgb_mode = 0;
			else
				rgb_mode = 1;
		} else {
			if (mif->video_mode == 1)
				rgb_mode = 3;//2;
			else
				rgb_mode = 2;
		}
	}
	reg = &reg_wrmif_v3[mifsel][0];

	//////////////////////////////
	//////Afbce Write registers
	/////////////////////////////

	// set wr mif interface.
	//Wr(WRMIF_X, Rd(WRMIF_X) | (wr_mif->luma_x_start0 <<16) |
	//(wr_mif->luma_x_end0));   // start_x 0 end_x 719.
	//Wr(WRMIF_Y, Rd(WRMIF_Y) | (wr_mif->luma_y_start0 <<16) |
	//(wr_mif->luma_y_end0));   // start_y 0 end_y 239.

	op->wr(reg[WRMIF_X],  (mif->start_x << 16) | (mif->end_x));
	// start_x 0 end_x 719.
	op->wr(reg[WRMIF_Y],  (mif->start_y << 16) | (mif->end_y));
	// start_y 0 end_y 239.
	//printf("wr_mif_scope = %d %d %d %d",wr_mif->luma_x_start0,
		 //wr_mif->luma_x_end0,wr_mif->luma_y_start0,
		 //wr_mif->luma_y_end0);
	if (mif->linear)
		di_mif0_linear_wr_cfg2(mif, mifsel);
	else
		op->wr(reg[WRMIF_CANVAS], mif->canvas_num);
	if (mif->set_separate_en == 2) {
		op->wr(reg[WRMIF_CTRL],
			(mif->en << 0) |   // write mif en.
			(bits_mode << 1) |   // bit10 mode
			(mif->l_endian << 2) |   // little endian
			(1 << 3) |   // data ext enable
			(3 << 4) |   // word limit ?
			(mif->urgent << 16) |
			// urgent // ary default is 0 ?
			(mif->cbcr_swap << 17) |
			//swap cbcrworking in rgb mode =2: swap cbcr
			//(0      << 18) |   // vconv working in rgb mode =2:
			//(0      << 20) |   // hconv. output even pixel
			/* vcon working in rgb mode =2: 3 : output all.*/
		       (((mif->video_mode == 0) ? 0 : 3) << 18) |
		       /* hconv. output even pixel */
		       (((mif->video_mode == 2) ? 3 : 0) << 20) |
			(rgb_mode << 22) |
			// rgb mode =0, 422 YCBCR to one canvas.
			(0 << 24) |   // no gate clock
			(0 << 25) |   // canvas_sync enable
			(2 << 26) |   // burst lim
			(mif->reg_swap << 30));   // 64-bits swap enable
	} else {
		op->wr(reg[WRMIF_CTRL],
		       (mif->en << 0) |   // write mif en.
		       (bits_mode << 1) |   // bit10 mode
		       (mif->l_endian << 2) |   // little endian
		       (0      << 3) |   // data ext enable
		       (5      << 4) |   // word limit ?
		       (mif->urgent << 16) |   // urgent // ary default is 0 ?
		       (mif->cbcr_swap << 17) |
		       // swap cbcrworking in rgb mode =2: swap cbcr
		       (0      << 18) |   // vconv working in rgb mode =2:
		       (0      << 20) |   // hconv. output even pixel
		       (rgb_mode      << 22) |
		       // rgb mode =0, 422 YCBCR to one canvas.
		       (0      << 24) |   // no gate clock
		       (0      << 25) |   // canvas_sync enable
		       (2      << 26) |   // burst lim
		       (mif->reg_swap      << 30));   // 64-bits swap enable
	}
}

/* ref to config_di_wr_mif */
//static
void wr_mif_cfg_v3(struct DI_SIM_MIF_s *wr_mif,
		   enum EDI_MIFSM index,
		   /*struct di_buf_s *di_buf,*/
		   void *di_vf,
		   struct di_win_s *win)
{
	struct di_buf_s *di_buf;
	struct vframe_s *vf;

	if (index == EDI_MIFSM_NR) {
		di_buf = (struct di_buf_s *)di_vf;
		vf = di_buf->vframe;
		if (!di_buf->flg_nv21)
			wr_mif->canvas_num = di_buf->nr_canvas_idx;

	} else {
		vf = (struct vframe_s *)di_vf;
		di_buf = (struct di_buf_s *)vf->private_data;
		if (!di_buf) {
			PR_ERR("%s:no di_buf\n", __func__);
			return;
		}
	}

	wr_mif->start_x = 0;
	wr_mif->end_x = vf->width - 1;
	wr_mif->start_y = 0;
	//tmp for t7:
	wr_mif->buf_crop_en	= 1;
	//wr_mif->buf_hsize	= 1920;
	wr_mif->buf_hsize	= di_buf->buf_hsize;

	if (vf->bitdepth & BITDEPTH_Y10) {
		if (vf->type & VIDTYPE_VIU_444) {
			wr_mif->bit_mode = (vf->bitdepth & FULL_PACK_422_MODE) ?
						3 : 2;
		} else {
			wr_mif->bit_mode = (vf->bitdepth & FULL_PACK_422_MODE) ?
						3 : 1;
		}
	} else {
		wr_mif->bit_mode = 0;
	}

	/* video mode */
	if (vf->type & (VIDTYPE_VIU_444 | VIDTYPE_RGB_444))
		wr_mif->video_mode = 2;
	else if (vf->type & VIDTYPE_VIU_422)
		wr_mif->video_mode = 1;
	else /* nv21 or nv12 */
		wr_mif->video_mode = 0;

	/* separate */
	if (vf->type & VIDTYPE_VIU_422)
		wr_mif->set_separate_en = 0;
	else if (vf->type & (VIDTYPE_VIU_NV12 | VIDTYPE_VIU_NV21))
		wr_mif->set_separate_en = 2; /*nv12 ? nv 21?*/

	if (index == EDI_MIFSM_NR) {
		if (wr_mif->src_i)
			wr_mif->end_y = vf->height / 2 - 1;
		else
			wr_mif->end_y = vf->height - 1;
	} else {
		if (dim_dbg_cfg_post_byapss())
			wr_mif->end_y = vf->height / 2 - 1;
		else
			wr_mif->end_y = vf->height - 1;
	}
}

static void set_di_pre_write(u32 pre_path_sel,
			     struct DI_MIF_S *pre_mif,
			     struct AFBCE_S   *pre_afbce,
			     const struct reg_acc *op)
{
	u32 pre_wrmif_enable  = (pre_path_sel  >> 0) & 0x1;
	u32 pre_afbce_enable  = (pre_path_sel  >> 1) & 0x1;

	op->bwr(DI_TOP_PRE_CTRL, (pre_path_sel & 0x3), 0, 2);
	//post_path_sel

	////////////////////////////////
	//////Cofigure Wrmif and Afbce
	////////////////////////////////

	if (pre_wrmif_enable) {
		set_wrmif_simple(0,
				 //int        index    ,
				 pre_wrmif_enable,
				 //int        enable   ,
				 pre_mif,
				 //DI_MIF0_t  *wr_mif
				 op);
	}

	if (pre_afbce_enable) {
		set_afbce_cfg_v1(1,
				 //int       index       ,
				 //0:vdin_afbce 1:di_afbce0 2:di_afbce1
				 pre_afbce_enable,
				 //int       enable      ,
				 //open nbit of afbce
				 pre_afbce,
				 //AFBCE_t  *afbce
				 op);
	}
}

static void set_di_post_write(u32 post_path_sel,
			      struct DI_MIF_S *post_mif,
			      struct AFBCE_S   *post_afbce,
			      const struct reg_acc *op)
{
	u32 post_wrmif_enable = (post_path_sel >> 0) & 0x1;
	u32 post_afbce_enable = (post_path_sel >> 1) & 0x1;

	op->bwr(DI_TOP_POST_CTRL, (post_path_sel & 0x3), 0, 2);
	//post_path_sel

	////////////////////////////////
	//////Cofigure Wrmif and Afbce
	////////////////////////////////

	if (post_wrmif_enable) {
		set_wrmif_simple(1,//int        index    ,
				 post_wrmif_enable,//int        enable   ,
				 post_mif, //DI_MIF0_t  *wr_mif
				 op);
	}
	if (post_afbce_enable) {
		set_afbce_cfg_v1(2, post_afbce_enable, post_afbce, op);
			//int       index       ,
			//0:vdin_afbce 1:di_afbce0 2:di_afbce1
			//int       enable      ,
			//open nbit of afbce
			//AFBCE_t  *afbce
	}
}

static void set_di_mult_write(struct DI_MULTI_WR_S *mwcfg,
			      const struct reg_acc *opin)
{
	const struct reg_acc *op;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;

	////////////////////////////////
	//////Cofigure Wrmif and Afbce
	////////////////////////////////
	set_di_pre_write(mwcfg->pre_path_sel,//uint32_t  pre_path_sel  ,
			 mwcfg->pre_mif,//DI_MIF0_t  *pre_mif      ,
			 mwcfg->pre_afbce, //AFBCE_t   *pre_afbce    ,
			 op);

	set_di_post_write(mwcfg->post_path_sel,//uint32_t  pre_path_sel  ,
			  mwcfg->post_mif,//DI_MIF0_t  *pre_mif      ,
			  mwcfg->post_afbce,//AFBCE_t   *pre_afbce    ,
			  op);
}

static void set_di_pre(struct DI_PRE_S *pcfg, const struct reg_acc *opin)
{
	int p1_en, p2_en;
	int p1_mif_en, p2_mif_en;//chan2 and mem
	int p1_hsize, p1_vsize;
	int p2_hsize, p2_vsize;
	int pre_hsize_i, pre_vsize_i;
	int pre_hsize_o, pre_vsize_o;
	int bits_mode, rgb_mode;
#ifndef DI_SCALAR_DISABLE	//ary add
	int inp_hsc_en, pps_en;
#endif
	int hold_line_new;
	const struct reg_acc *op;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;

	if (pcfg->pre_viu_link == 0)
		op->wr(DI_SC2_PRE_GL_CTRL, 0xc0200001);

	//====================================//
	// MIF configration
	//====================================//
	p1_en = pcfg->mcdi_en || pcfg->mtn_en ||
		pcfg->pd32_check_en || pcfg->pd22_check_en ||
		pcfg->hist_check_en || pcfg->cue_en;
	p2_en = pcfg->nr_en || pcfg->mcdi_en || pcfg->mtn_en;

	if (pcfg->afbc_en) {
		pre_hsize_i =
			pcfg->inp_afbc->hsize >> pcfg->inp_afbc->h_skip_en;
		pre_vsize_i =
			pcfg->inp_afbc->vsize >> pcfg->inp_afbc->v_skip_en;
		pre_hsize_o = pcfg->nrwr_afbc->hsize_in;
		pre_vsize_o = pcfg->nrwr_afbc->vsize_in;

		p1_hsize =
			pcfg->chan2_afbc->hsize >> pcfg->chan2_afbc->h_skip_en;
		p1_vsize =
			pcfg->chan2_afbc->vsize >> pcfg->chan2_afbc->v_skip_en;
		p2_hsize = pcfg->mem_afbc->hsize >> pcfg->mem_afbc->h_skip_en;
		p2_vsize = pcfg->mem_afbc->vsize >> pcfg->mem_afbc->v_skip_en;

		p1_mif_en = p1_en && (p1_hsize == pre_hsize_o) &&
				(p1_vsize == pre_vsize_o);
		p2_mif_en = p2_en && (p2_hsize == pre_hsize_o) &&
				(p2_vsize == pre_vsize_o);

		set_afbcd_mult_simple(EAFBC_DEC2_DI,//di_inp_afbc->index,
				      pcfg->inp_afbc,
				      op);//AFBCD_t *afbcd
		if (p2_mif_en) {
			set_afbcd_mult_simple(EAFBC_DEC3_MEM,
					      //di_mem_afbc->index,
					      pcfg->mem_afbc,
					      op); //AFBCD_t *afbcd
		}
		if (p1_mif_en) {
			set_afbcd_mult_simple(EAFBC_DEC_CHAN2,
					      //di_chan2_afbc->index,
					      pcfg->chan2_afbc,
					      op);//AFBCD_t *afbcd
		}

		if (pcfg->nr_en) {
			set_afbce_cfg_v1(1, 1, pcfg->nrwr_afbc, op);
				//int       index       ,
				//0:vdin_afbce 1:di_afbce0 2:di_afbce1
				//int       enable      ,
				//open nbit of afbce
				//AFBCE_t  *afbce
		}
	} else {
		pre_hsize_i = pcfg->inp_mif->luma_x_end0 -
			pcfg->inp_mif->luma_x_start0 + 1;
		pre_vsize_i = pcfg->inp_mif->luma_y_end0 -
			pcfg->inp_mif->luma_y_start0 + 1;
		pre_hsize_o = pcfg->nrwr_mif->luma_x_end0 -
			pcfg->nrwr_mif->luma_x_start0 + 1;
		pre_vsize_o = pcfg->nrwr_mif->luma_y_end0 -
			pcfg->nrwr_mif->luma_y_start0 + 1;

		p1_hsize = pcfg->chan2_mif->luma_x_end0 -
			pcfg->chan2_mif->luma_x_start0 + 1;
		p1_vsize = pcfg->chan2_mif->luma_y_end0 -
			pcfg->chan2_mif->luma_y_start0 + 1;
		p2_hsize = pcfg->mem_mif->luma_x_end0 -
			pcfg->mem_mif->luma_x_start0 + 1;
		p2_vsize = pcfg->mem_mif->luma_y_end0 -
			pcfg->mem_mif->luma_y_start0 + 1;

		p1_mif_en = p1_en && (p1_hsize == pre_hsize_o) &&
				(p1_vsize == pre_vsize_o);
		p2_mif_en = p2_en && (p2_hsize == pre_hsize_o) &&
				(p2_vsize == pre_vsize_o);

		hold_line_new = pcfg->pre_viu_link ?
			(pcfg->hold_line > 4 ? pcfg->hold_line - 4 : 0) :
			pcfg->hold_line;

		pcfg->inp_mif->urgent = 0;	//ary move
		pcfg->inp_mif->hold_line = hold_line_new; //ary move
		set_di_mif_v1(pcfg->inp_mif, DI_MIF0_ID_INP, op);
		if (p2_mif_en) {
			pcfg->mem_mif->urgent = 0;	//ary move
			pcfg->mem_mif->hold_line = hold_line_new; //ary move
			set_di_mif_v1(pcfg->mem_mif, DI_MIF0_ID_MEM, op);
		}
		if (p1_mif_en) {
			pcfg->chan2_mif->urgent = 0;	//ary move
			pcfg->chan2_mif->hold_line = hold_line_new; //ary move
			set_di_mif_v1(pcfg->chan2_mif, DI_MIF0_ID_CHAN2, op);
		}
		// set nr wr mif interface.
		if (pcfg->nr_en) {
			op->wr(DI_SC2_NRWR_X,  op->rd(DI_SC2_NRWR_X) |
			       (pcfg->nrwr_mif->luma_x_start0 << 16) |
			       (pcfg->nrwr_mif->luma_x_end0));
			       // start_x 0 end_x 719.
			op->wr(DI_SC2_NRWR_Y,  op->rd(DI_SC2_NRWR_Y) |
			       (pcfg->nrwr_mif->luma_y_start0 << 16) |
			       (pcfg->nrwr_mif->luma_y_end0));
			       // start_y 0 end_y 239.
			if (pcfg->nrwr_mif->linear)
				di_mif0_linear_wr_cfg(pcfg->nrwr_mif, 0, op);
			else
				op->wr(DI_NRWR_CANVAS,
				       pcfg->nrwr_mif->canvas0_addr0 |
				       (pcfg->nrwr_mif->canvas0_addr1 << 8));
			bits_mode = pcfg->nrwr_mif->bit_mode != 0;
			//1: 10bits 422(old mode)
			//   0:8bits   2:10bit 444  3:10bit full-pack
			// rgb_mode 0: 422 to one canvas
			//          1: 444 to one canvas
			//2: 8bit Y to one canvas, 16bit UV to another canvas
			//          3: 422 full pack mode (10bit)
			rgb_mode = pcfg->nrwr_mif->bit_mode == 3 ? 3 :
				//422 10bit
				(pcfg->nrwr_mif->set_separate_en == 0 ?
				(pcfg->nrwr_mif->video_mode == 1 ? 0 : 1) :
				((pcfg->nrwr_mif->video_mode == 1) ? 2 : 3));
				//two canvas
			op->wr(DI_SC2_NRWR_CTRL, (pcfg->nr_en << 0)   |
			       // write mif en.
			       (bits_mode << 1) |   // bit10 mode
			       (0 << 2) |   // little endian
			       (0 << 3) |   // data ext enable
			       (5 << 4) |   // word limit
			       (0 << 16) |   // urgent
			       (0 << 17) |
			       // swap cbcrworking in rgb mode =2: swap cbcr
			       (0 << 18) |   // vconv working in rgb mode =2:
			       (0 << 20) |   // hconv. output even pixel
			       (rgb_mode << 22) |
			       // rgb mode =0, 422 YCBCR to one canvas.
			       (0 << 24) |   // no gate clock
			       (0 << 25) |   // canvas_sync enable
			       (2 << 26) |   // burst lim
			       (1 << 30));   // 64-bits swap enable
		}
	}
	if (pcfg->nr_en) {
		op->wr(NR4_TOP_CTRL,
			(0 << 20) |
			//reg_gclk_ctrl             = reg_top_ctrl[31:20];
			(1 << 19) |
			//reg_text_en               = reg_top_ctrl[19]   ;
			(1 << 18) |
			//reg_nr4_mcnr_en           = reg_top_ctrl[18]   ;
			(1 << 17) |
			//reg_nr2_en                = reg_top_ctrl[17]   ;
			(1 << 16) |
			//reg_nr4_en                = reg_top_ctrl[16]   ;
			(1 << 15) |
			//reg_nr2_proc_en           = reg_top_ctrl[15]   ;
			(0 << 14) |
			//reg_det3d_en              = reg_top_ctrl[14]   ;
			(0 << 13) |
			//di_polar_en               = reg_top_ctrl[13]   ;
			(0 << 12) |
			//reg_cfr_enable            = reg_top_ctrl[12]   ;
			(7 << 9) |
			//reg_3dnr_en_l             = reg_top_ctrl[11:9] ;// u3
			(7 << 6) |
			//reg_3dnr_en_r             = reg_top_ctrl[8:6]  ;// u3
			(1 << 5) |
			//reg_nr4_inbuf_ctrl        = reg_top_ctrl[5]    ;
			(1 << 4) |
			//reg_nr4_snr2_en           = reg_top_ctrl[4]    ;
			(0 << 3) |
			//reg_nr4_scene_change_en   = reg_top_ctrl[3]    ;
			(1 << 2) |
			//nr2_sw_en                 = reg_top_ctrl[2]    ;
			(1 << 1) |
			//reg_cue_en                = reg_top_ctrl[1]    ;
			(0 << 0));
			//reg_nr4_scene_change_flg  = reg_top_ctrl[0]    ;
	}

#ifndef DI_SCALAR_DISABLE	//ary add

	// scaler
	inp_hsc_en = (pre_vsize_i == pre_vsize_o) &&
			(pre_hsize_i != pre_hsize_o);
	pps_en = (pre_vsize_i != pre_vsize_o);
	if (inp_hsc_en)
		set_inp_hsc(pre_hsize_i, pre_vsize_i, pre_hsize_o, pre_vsize_o);
	else if (pps_en)
		enable_di_scale(pre_hsize_i,
				pre_vsize_i,
				pre_hsize_o, pre_vsize_o, 1, 0);
#endif
	// config motion detect.
	if (pcfg->mtn_en) {
		op->wr(MTNWR_X, (2 << 30) |
		       (pcfg->mtnwr_mif->start_x << 16) |
		       (pcfg->mtnwr_mif->end_x));   // start_x 0 end_x 719.
		op->wr(MTNWR_Y, (pcfg->mtnwr_mif->start_y << 16) |
		       (pcfg->mtnwr_mif->end_y));   // start_y 0 end_y 239.
		op->wr(MTNWR_CAN_SIZE,
		       ((pcfg->mtnwr_mif->end_x - pcfg->mtnwr_mif->start_x) <<
		       16) | (pcfg->mtnwr_mif->end_y -
		       pcfg->mtnwr_mif->start_y));
		op->wr(MTNWR_CTRL, pcfg->mtnwr_mif->canvas_num |
		       // canvas index.
		       (1 << 12));       // req_en.

		set_cont_mif(pcfg->contp1_mif,
			     pcfg->contp2_mif, pcfg->contwr_mif, op);
		op->bwr(DI_MTN_CTRL1, pcfg->cont_ini_en, 30, 1);
	}

	// config mcdi
	if (pcfg->mcdi_en == 1) {
		set_mcdi_def(pcfg->mcvecwr_mif, pre_hsize_o - 1,
			     pre_vsize_o - 1, op);
		set_mcdi_pre(pcfg->mcinfrd_mif,
			     pcfg->mcvecwr_mif,
			     pcfg->mcinfwr_mif, op);
		op->bwr(MCDI_CTRL_MODE, pcfg->mcinfo_rd_en, 28, 1);//
	}

	op->bwr(DNR_CTRL, 0, 9, 1); //disable chroma
	op->bwr(DNR_CTRL, pcfg->dnr_en, 16, 1);
	op->bwr(DNR_DM_CTRL, 0, 8, 1);

	op->wr(DI_PRE_SIZE, ((pre_vsize_o - 1) << 16) | (pre_hsize_o - 1));

	op->wr(DI_PRE_CTRL,
	       (pcfg->nr_en         << 0)  |
	       //  nr_en                = pre_ctrl[0];
	       (pcfg->mtn_en        << 1)  |
	       //  mtn_en               = pre_ctrl[1];
	       (pcfg->pd32_check_en << 2)  |
	       //  check322p_en         = pre_ctrl[2];
	       (pcfg->pd22_check_en << 3)  |
	       //  check222p_en         = pre_ctrl[3];
	       (1             << 4)  |
	       //  check_after_nr       = pre_ctrl[4];
	       (pcfg->hist_check_en << 5)  |
	       //  chan2_hist_en        = pre_ctrl[5];
	       (1             << 6)  |
	       //  mtn_after_nr         = pre_ctrl[6];
	       (0             << 7)  |
	       //  pd_mtn_swap          = pre_ctrl[7];
	       (p2_mif_en     << 8)  |
	       //  di_buf3_en           = pre_ctrl[8]; //mem,   p2 field
	       (p1_mif_en     << 9)  |
	       //  di_buf2_en           = pre_ctrl[9]; //chan2, p1 field
	       (0             << 10)  |
	       //  nr_wr_by             = pre_ctrl[10];
	       (2             << 11)  |
	       //  reg_tfbf_en          = pre_ctrl[12:11];
	       //reg_tfbf_en : bit0:tfbf enable  bit1:normal path
	       (pcfg->mcdi_en       << 16)  |
	       //  reg_me_en            = pre_ctrl[16];//mcdi
	       (pcfg->mcdi_en       << 17)  |
	       //  reg_me_autoen        = pre_ctrl[17];
	       (0             << 19)  |
	       //  pre_field_num_mcdi   = pre_ctrl[19] ^ pre_field_num;
	       (0             << 20)  |
	       //  pre_field_num_pulldow= pre_ctrl[20] ^ pre_field_num;
	       (0             << 21)  |
	       //  pre_field_num_nr     = pre_ctrl[21] ^ pre_field_num;
	       (0             << 23)  |
	       //  mode_422c444         = pre_ctrl[24:23];
	       (0             << 26)  |
	       //  mode_444c422         = pre_ctrl[27:26];
	       (pcfg->pre_field_num << 29));
	       // pre_field_num         = pre_ctrl[29];

	op->wr(DI_TOP_PRE_CTRL,
	       ((pcfg->afbc_en ? 2 : 1)    << 0) | // nrwr_path_sel
	       (0  << 2) |               // afbce_path_sel
	       ((pcfg->afbc_en ? 7 : 0) << 4) |    // afbc_vd_sel
	       (1  << 10) |               // nr_ch0_en
	       (0  << 12) |               // pre_bypass_ctrl
	       (0  << 20) |               // fix_disable_pre
	       ((pcfg->pre_viu_link ? 2 : 0) << 30));
	       // pre_frm_sel = top_pre_ctrl[31:30];
	       //0:internal  1:pre-post link  2:viu  3:vcp(vdin)

	op->bwr(DI_TOP_CTRL, pcfg->pre_viu_link, 0, 1);

	if (pre_hsize_o > 1368)
		op->bwr(LBUF_TOP_CTRL, 0, 17, 1);// fmt444 disable

	op->bwr(DI_SC2_PRE_GL_THD, pcfg->hold_line, 16, 6);
	if (pcfg->pre_viu_link == 0)
		op->wr(DI_SC2_PRE_GL_CTRL, 0x80200001);
}

//===========================================
// enable di post module for separate test.
//===========================================
static void set_di_post(struct DI_PST_S *ptcfg, const struct reg_acc *opin)
{
	int ei_only;
	int weave_en;
	int m2m_en;
	int bits_mode, rgb_mode;
	int post_hsize1 = 0, post_vsize1 = 0; //before scaler
	int post_hsize2 = 0, post_vsize2 = 0; //after  scaler
#ifndef DI_SCALAR_DISABLE	//ary add
	int pps_en;
#endif
	int hold_line_new;
	const struct reg_acc *op;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;

	ei_only  = ptcfg->post_en && ptcfg->mux_en &&
		ptcfg->ei_en && !ptcfg->blend_en;//weave
	weave_en = ptcfg->post_en &&
		ptcfg->mux_en && !ptcfg->ei_en && !ptcfg->blend_en;//bob
	m2m_en   = ptcfg->post_en &&
		ptcfg->ddr_en && !ptcfg->mux_en;//memory copy

	if (ptcfg->ddr_en)
		op->wr(DI_SC2_POST_GL_CTRL, 0x40200001);

	if (ptcfg->post_en) {
		if (ptcfg->afbc_en) {
			set_afbcd_mult_simple(EAFBC_DEC_IF0,
					      //di_buf0_afbc->index,
					      ptcfg->buf0_afbc, op);
					      //AFBCD *afbcd

			if (weave_en || ptcfg->blend_en) {
				set_afbcd_mult_simple(EAFBC_DEC_IF1,
						      //di_buf1_afbc->index,
						      ptcfg->buf1_afbc, op);
						      //AFBCD *afbcd
			}
			if (ptcfg->blend_en) {
				set_afbcd_mult_simple(EAFBC_DEC_IF2,
						      //di_buf2_afbc->index,
						      ptcfg->buf2_afbc, op);
						      //AFBCD *afbcd
			}

			post_hsize1 = ptcfg->buf0_afbc->hsize;
			post_vsize1 = m2m_en ? ptcfg->buf0_afbc->vsize :
				ptcfg->buf0_afbc->vsize * 2;
			post_hsize1 =
				post_hsize1 >> ptcfg->buf0_afbc->h_skip_en;
			post_vsize1 =
				post_vsize1 >> ptcfg->buf0_afbc->v_skip_en;

		} else {
			hold_line_new = ptcfg->vpp_en ?
				(ptcfg->hold_line > 4 ?
				ptcfg->hold_line - 4 : 0) : ptcfg->hold_line;
			ptcfg->buf0_mif->urgent	= 0;//ary move;
			ptcfg->buf0_mif->hold_line = hold_line_new;//ary move;
			set_di_mif_v1(ptcfg->buf0_mif, DI_MIF0_ID_IF0, op);
			if (weave_en || ptcfg->blend_en) {
				ptcfg->buf1_mif->urgent	= 0;
				//ary move;
				ptcfg->buf1_mif->hold_line = hold_line_new;
				//ary move;
				set_di_mif_v1(ptcfg->buf1_mif,
					      DI_MIF0_ID_IF1, op);
			}
			if (ptcfg->blend_en) {
				ptcfg->buf2_mif->urgent	= 0;
				//ary move;
				ptcfg->buf2_mif->hold_line = hold_line_new;
				//ary move;
				set_di_mif_v1(ptcfg->buf2_mif,
					      DI_MIF0_ID_IF2, op);
			}

			post_hsize1 = ptcfg->buf0_mif->luma_x_end0 -
				ptcfg->buf0_mif->luma_x_start0 + 1;
			post_vsize1 = m2m_en ?
				(ptcfg->buf0_mif->luma_y_end0 -
				ptcfg->buf0_mif->luma_y_start0 + 1) :
				(ptcfg->buf0_mif->luma_y_end0 * 2 -
				ptcfg->buf0_mif->luma_y_start0 * 2 + 2);
		}
	}
	// motion for current display field.
	if (ptcfg->blend_en) {
		if (DIM_IS_IC(SC2))
			op->wr(MCDI_LMV_GAINTHD,  (3 << 20));//normal di
		else
			op->wr(DI_BLEND_CTRL,  (3 << 20));//normal di

		op->wr(MTNRD_SCOPE_X,
		       (ptcfg->mtn_mif->start_x) |
		       (ptcfg->mtn_mif->end_x << 16));
		       // start_x 0 end_x 719.
		op->wr(MTNRD_SCOPE_Y,
		       (ptcfg->mtn_mif->start_y) |
		       (ptcfg->mtn_mif->end_y << 16));
		       // start_y 0 end_y 239.
		op->wr(MTNRD_CTRL1, ptcfg->mtn_mif->canvas_num << 16 |
		       //canvas index
		       2 << 8 | //burst len = 2
		       0 << 6 | //little endian
		       0 << 0);//pack mode
		//mc vector read mif
		if (ptcfg->mc_en) {
			set_mcdi_post(ptcfg->mcvec_mif, op);
			op->bwr(MCDI_MC_CRTL, 1, 0, 2);
		} else {
			op->bwr(MCDI_MC_CRTL, 0, 0, 2);
		}
	}

	if (ptcfg->ddr_en) {
		if (ptcfg->afbc_en) {
			set_afbce_cfg_v1(2, 1, ptcfg->wr_afbc, op);
				//int       index       ,
				//0:vdin_afbce 1:di_afbce0 2:di_afbce1
				//int       enable      ,
				//open nbit of afbce
				//AFBCE_t  *afbce
			post_hsize2 = ptcfg->wr_afbc->hsize_in;
			post_vsize2 = ptcfg->wr_afbc->vsize_in;
		} else {
			op->wr(DI_SC2_DIWR_X,
			       (ptcfg->wr_mif->luma_x_start0 << 16) |
			       (ptcfg->wr_mif->luma_x_end0));
			       // start_x 0 end_x 719.
			op->wr(DI_SC2_DIWR_Y,
			       (ptcfg->wr_mif->luma_y_start0 << 16) |
			       (ptcfg->wr_mif->luma_y_end0));
			       // start_y 0 end_y 239.
			if (ptcfg->wr_mif->linear)
				di_mif0_linear_wr_cfg(ptcfg->wr_mif, 1, op);
			else
				op->wr(DI_DIWR_CANVAS, ptcfg->wr_mif->canvas0_addr0 |
				       (ptcfg->wr_mif->canvas0_addr1 << 8));
			bits_mode = ptcfg->wr_mif->bit_mode != 0;
			//1: 10bits   0:8bits
			// rgb_mode 0: 422 to one canvas
			//1: 444 to one canvas
			//2: 8bit Y to one canvas, 16bit UV to another canvas
			//3: 422 full pack mode (10bit)
			rgb_mode = ptcfg->wr_mif->bit_mode == 3 ? 3 ://422 10bit
					(ptcfg->wr_mif->set_separate_en == 0 ?
					(ptcfg->wr_mif->video_mode == 1 ?
					 0 : 1) :
					((ptcfg->wr_mif->video_mode == 1)
					? 2 : 3)); //two canvas
			op->wr(DI_SC2_DIWR_CTRL,  (1 << 0) |   // write mif en.
			       (bits_mode << 1) |   // bit10 mode
			       (0 << 2) |   // little endian
			       (0 << 3) |   // data ext enable
			       (5 << 4) |   // word limit
			       (0 << 16) |   // urgent
			       (0 << 17) |
			       // swap cbcrworking in rgb mode =2: swap cbcr
			       (0 << 18) |   // vconv working in rgb mode =2:
			       (0 << 20) |   // hconv. output even pixel
			       (rgb_mode << 22) |
			       // rgb mode =0, 422 YCBCR to one canvas.
			       (0 << 24) |   // no gate clock
			       (0 << 25) |   // canvas_sync enable
			       (2 << 26) |   // burst lim
			       (0 << 30));   // 64-bits swap enable
			post_hsize2 =
				ptcfg->wr_mif->luma_x_end0 -
				ptcfg->wr_mif->luma_x_start0 + 1;
			post_vsize2 =
				ptcfg->wr_mif->luma_y_end0 -
				ptcfg->wr_mif->luma_y_start0 + 1;
		}

#ifndef DI_SCALAR_DISABLE	//ary add
		pps_en = (post_hsize1 != post_hsize2) ||
			(post_vsize1 != post_vsize2);
		if (pps_en)
			enable_di_scale(post_hsize1, post_vsize1,
					post_hsize2, post_vsize2, 0, 0);
#endif
	}

	op->bwr(DI_SC2_POST_GL_THD, ptcfg->hold_line, 16, 6);
	op->wr(DI_POST_SIZE, (post_vsize1 - 1) << 16 | (post_hsize1 - 1));

	op->wr(DI_POST_CTRL,
	       (ptcfg->post_en  << 0) | // di_post_en      = post_ctrl[0];
	       (ptcfg->blend_en << 1) | // di_blend_en     = post_ctrl[1];
	       (ptcfg->ei_en    << 2) | // di_ei_en        = post_ctrl[2];
	       (ptcfg->mux_en   << 3) | // di_mux_en       = post_ctrl[3];
	       (ptcfg->ddr_en   << 4) | // di_wr_bk_en     = post_ctrl[4];
	       (ptcfg->vpp_en   << 5) | // di_vpp_out_en   = post_ctrl[5];
	       (0           << 6) | // reg_post_mb_en  = post_ctrl[6];
	       (0           << 10) | // di_post_drop_1st= post_ctrl[10];
	       (0           << 11) | // di_post_repeat  = post_ctrl[11];
	       (ptcfg->post_field_num << 29));
	       // post_field_num  = post_ctrl[29];

	op->wr(DI_TOP_POST_CTRL,
	       ((ptcfg->afbc_en ? 2 : 1) << 0)  |
	       // diwr_path_sel     =top_post_ctrl[1:0];
	       ((ptcfg->afbc_en ? 7 : 0) << 4)   |
	       // afbc_vd_sel[5:3]  =top_post_ctrl[6:4];
	       ((ptcfg->ddr_en ? 1 : 0) << 30));
	       // post_frm_sel =top_post_ctrl[3];
	       //0:viu  1:internal  2:pre-post link (post timming)

	//Wr_reg_bits(DI_TOP_CTRL, !di_vpp_en, 0, 1);

	if (ptcfg->ddr_en)
		op->wr(DI_SC2_POST_GL_CTRL, 0x80200001);
}

// the input data is 4:2:2,  saved in field mode video.
static void enable_prepost_link(struct DI_PREPST_S *ppcfg,
				const struct reg_acc *opin)
{
	int pre_hsize;
	int pre_vsize;
	int hold_line_new;
	const struct reg_acc *op;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;

	// clock gate
	if (ppcfg->link_vpp == 0)
		op->wr(DI_SC2_PRE_GL_CTRL, 0xc0200001);

	/**********************************/
	/****   DI PRE MIF CONFIG  ********/
	/**********************************/
	pre_hsize = ppcfg->nrwr_mif->luma_x_end0 -
		ppcfg->nrwr_mif->luma_x_start0 + 1;
	pre_vsize = ppcfg->nrwr_mif->luma_y_end0 -
		ppcfg->nrwr_mif->luma_y_start0 + 1;

	//config read mif
	hold_line_new = ppcfg->link_vpp ?
		(ppcfg->hold_line > 4 ? ppcfg->hold_line - 4 : 0) :
		ppcfg->hold_line;
	//ary move:
	ppcfg->inp_mif->urgent = 0;
	ppcfg->inp_mif->hold_line = hold_line_new;
	ppcfg->mem_mif->urgent = 0;
	ppcfg->mem_mif->hold_line = hold_line_new;
	ppcfg->chan2_mif->urgent = 0;
	ppcfg->chan2_mif->hold_line = hold_line_new;
	ppcfg->if1_mif->urgent = 0;
	ppcfg->if1_mif->hold_line = hold_line_new;
	set_di_mif_v1(ppcfg->inp_mif, DI_MIF0_ID_INP, op);
	set_di_mif_v1(ppcfg->mem_mif, DI_MIF0_ID_MEM, op);
	set_di_mif_v1(ppcfg->chan2_mif, DI_MIF0_ID_CHAN2, op);
	set_di_mif_v1(ppcfg->if1_mif, DI_MIF0_ID_IF1, op);

	// set nr wr mif interface.
	op->wr(DI_SC2_NRWR_X, op->rd(DI_SC2_NRWR_X) |
	       (ppcfg->nrwr_mif->luma_x_start0 << 16) |
	       (ppcfg->nrwr_mif->luma_x_end0));   // start_x 0 end_x 719.
	op->wr(DI_SC2_NRWR_Y,  op->rd(DI_SC2_NRWR_Y) |
	       (ppcfg->nrwr_mif->luma_y_start0 << 16) |
	       (ppcfg->nrwr_mif->luma_y_end0));   // start_y 0 end_y 239.
	if (ppcfg->nrwr_mif->linear)
		di_mif0_linear_wr_cfg(ppcfg->nrwr_mif, 0, op);
	else
		op->wr(DI_NRWR_CANVAS, ppcfg->nrwr_mif->canvas0_addr0);
	op->wr(DI_SC2_NRWR_CTRL, (1 << 0) |   // write mif en.
	       (0 << 1) |   // bit10 mode
	       (0 << 2) |   // little endian
	       (0 << 3) |   // data ext enable
	       (5 << 4) |   // word limit
	       (0 << 16) |   // urgent
	       (0 << 17) |   // swap cbcrworking in rgb mode =2: swap cbcr
	       (0 << 18) |   // vconv working in rgb mode =2:
	       (0 << 20) |   // hconv. output even pixel
	       (0 << 22) |   // rgb mode =0, 422 YCBCR to one canvas.
	       (0 << 24) |   // no gate clock
	       (0 << 25) |   // canvas_sync enable
	       (2 << 26) |   // burst lim
	       (1 << 30));   // 64-bits swap enable
	op->wr(NR4_TOP_CTRL,
	       (0 << 20) |  //reg_gclk_ctrl             = reg_top_ctrl[31:20];
	       (1 << 19) |  //reg_text_en               = reg_top_ctrl[19]   ;
	       (1 << 18) |  //reg_nr4_mcnr_en           = reg_top_ctrl[18]   ;
	       (1 << 17) |  //reg_nr2_en                = reg_top_ctrl[17]   ;
	       (1 << 16) |  //reg_nr4_en                = reg_top_ctrl[16]   ;
	       (1 << 15) |  //reg_nr2_proc_en           = reg_top_ctrl[15]   ;
	       (0 << 14) |  //reg_det3d_en              = reg_top_ctrl[14]   ;
	       (0 << 13) |  //di_polar_en               = reg_top_ctrl[13]   ;
	       (0 << 12) |  //reg_cfr_enable            = reg_top_ctrl[12]   ;
	       (7 << 9) |  //reg_3dnr_en_l        = reg_top_ctrl[11:9] ;  // u3
	       (7 << 6) |  //reg_3dnr_en_r        = reg_top_ctrl[8:6]  ;  // u3
	       (1 << 5) |  //reg_nr4_inbuf_ctrl        = reg_top_ctrl[5]    ;
	       (1 << 4) |  //reg_nr4_snr2_en           = reg_top_ctrl[4]    ;
	       (0 << 3) |  //reg_nr4_scene_change_en   = reg_top_ctrl[3]    ;
	       (1 << 2) |  //nr2_sw_en                 = reg_top_ctrl[2]    ;
	       (1 << 1) |  //reg_cue_en                = reg_top_ctrl[1]    ;
	       (0 << 0)); //reg_nr4_scene_change_flg  = reg_top_ctrl[0]    ;

	// motion write mif
	op->wr(MTNWR_X, (2 << 30) |   (ppcfg->mtnwr_mif->start_x << 16) |
	       (ppcfg->mtnwr_mif->end_x));   // start_x 0 end_x 719.
	op->wr(MTNWR_Y, (ppcfg->mtnwr_mif->start_y << 16) |
	       (ppcfg->mtnwr_mif->end_y));   // start_y 0 end_y 239.
	op->wr(MTNWR_CAN_SIZE,
	       ((ppcfg->mtnwr_mif->end_x - ppcfg->mtnwr_mif->start_x) << 16) |
	       (ppcfg->mtnwr_mif->end_y - ppcfg->mtnwr_mif->start_y));
	op->wr(MTNWR_CTRL, ppcfg->mtnwr_mif->canvas_num |  // canvas index.
	       (1 << 12));       // req_en.

	// config 1bit motion and mcdi mif (PRE)
	set_mcdi_def(ppcfg->mcvecwr_mif, pre_hsize - 1, pre_vsize - 1, op);
	set_mcdi_pre(ppcfg->mcinford_mif,
		     ppcfg->mcvecwr_mif, ppcfg->mcinfowr_mif, op);
	set_cont_mif(ppcfg->contprd_mif,
		     ppcfg->contp2rd_mif, ppcfg->contwr_mif, op);
	op->bwr(DI_MTN_CTRL1, ppcfg->cont_ini_en, 30, 1);
	op->bwr(MCDI_CTRL_MODE, ppcfg->mcinfo_rd_en, 28, 1);

	/**********************************/
	/****   DI POST MIF CONFIG ********/
	/**********************************/
	//motion read mif
	op->wr(MTNRD_SCOPE_X,
	       (ppcfg->mtnrd_mif->start_x) | (ppcfg->mtnrd_mif->end_x << 16));
	       // start_x 0 end_x 719.
	op->wr(MTNRD_SCOPE_Y,
	       (ppcfg->mtnrd_mif->start_y) | (ppcfg->mtnrd_mif->end_y << 16));
	       // start_y 0 end_y 239.
	op->wr(MTNRD_CTRL1, ppcfg->mtnrd_mif->canvas_num << 16 | //canvas index
		2 << 8 | //burst len = 2
		0 << 6 | //little endian
		0 << 0);//pack mode

	//mc vector read mif
	set_mcdi_post(ppcfg->mcvecrd_mif, op);

	// di post write
	op->wr(DI_SC2_DIWR_X, (ppcfg->diwr_mif->luma_x_start0 << 16) |
	       (ppcfg->diwr_mif->luma_x_end0));   // start_x 0 end_x 719.
	op->wr(DI_SC2_DIWR_Y, (ppcfg->diwr_mif->luma_y_start0 << 16) |
	       (ppcfg->diwr_mif->luma_y_end0));   // start_y 0 end_y 239.
	if (ppcfg->diwr_mif->linear)
		di_mif0_linear_wr_cfg(ppcfg->diwr_mif, 1, op);
	else
		op->wr(DI_DIWR_CANVAS,
		       ppcfg->diwr_mif->canvas0_addr0 |
		       (ppcfg->diwr_mif->canvas0_addr1 << 8));
	op->wr(DI_SC2_DIWR_CTRL,     (1 << 0) |   // write mif en.
	       (0 << 1) |   // bit10 mode
	       (0 << 2) |   // little endian
	       (0 << 3) |   // data ext enable
	       (5 << 4) |   // word limit
	       (0 << 16) |   // urgent
	       (0 << 17) |   // swap cbcrworking in rgb mode =2: swap cbcr
	       (0 << 18) |   // vconv working in rgb mode =2:
	       (0 << 20) |   // hconv. output even pixel
	       (0 << 22) |   // rgb mode =0, 422 YCBCR to one canvas.
	       (0 << 24) |   // no gate clock
	       (0 << 25) |   // canvas_sync enable
	       (2 << 26) |   // burst lim
	       (0 << 30));   // 64-bits swap enable

	/**********************************/
	/****** control REG config  *******/
	/**********************************/
	op->bwr(LBUF_TOP_CTRL, 0, 20, 6);
	op->bwr(LBUF_TOP_CTRL, 0, 17, 1);// fmt444 disable

	op->wr(DI_PRE_SIZE,   (pre_hsize - 1) | ((pre_vsize - 1) << 16));
	op->wr(DI_POST_SIZE,  (pre_hsize - 1) | ((pre_vsize * 2  - 1) << 16));

	op->wr(DI_INTR_CTRL, (0 << 16) |       // mask nrwr interrupt.
	       (0 << 17) |       // enable mtnwr interrupt.
	       (0 << 18) |       // mask diwr interrupt.
	       (1 << 19) |       // mask hist check interrupt.
	       (0 << 20) |       // mask cont interrupt.
	       (1 << 21) |       // mask medi interrupt.
	       (0 << 22) |       // mask vecwr interrupt.
	       (0 << 23) |       // mask infwr interrupt.
	       (1 << 24) |       // mask det3d interrupt.
	       (1 << 25) |       // mask nrds write.
	       (1 << 30) |       // mask both pre and post write done
	       0x3ff);            // clean all pending interrupt bits.

	op->wr(DI_PRE_CTRL,
	       (1 << 0)  |  //  nr_en                = pre_ctrl[0];
	       (1 << 1)  |  //  mtn_en               = pre_ctrl[1];
	       (1 << 2)  |  //  check322p_en         = pre_ctrl[2];
	       (1 << 3)  |  //  check222p_en         = pre_ctrl[3];
	       (1 << 4)  |  //  check_after_nr       = pre_ctrl[4];
	       (0 << 5)  |  //  chan2_hist_en        = pre_ctrl[5];
	       (1 << 6)  |  //  mtn_after_nr         = pre_ctrl[6];
	       (0 << 7)  |  //  pd_mtn_swap          = pre_ctrl[7];
	       (1 << 8)  |
	       //  di_buf3_en           = pre_ctrl[8]; //mem,   p2 field
	       (1 << 9)  |
	       //  di_buf2_en           = pre_ctrl[9]; //chan2, p1 field
	       (0 << 10)  |
	       //  nr_wr_by             = pre_ctrl[10];
	       (2 << 11)  |
	       //  reg_tfbf_en          = pre_ctrl[12:11];//reg_tfbf_en
	       (1 << 16)  |
	       //  reg_me_en            = pre_ctrl[16];//mcdi
	       (1 << 17)  |
	       //  reg_me_autoen        = pre_ctrl[17];
	       (0 << 19)  |
	       //  pre_field_num_mcdi   = pre_ctrl[19] ^ pre_field_num;
	       (0 << 20)  |
	       //  pre_field_num_pulldow= pre_ctrl[20] ^ pre_field_num;
	       (0 << 21)  |
	       //  pre_field_num_nr     = pre_ctrl[21] ^ pre_field_num;
	       (0 << 23)  |
	       //  mode_422c444         = pre_ctrl[24:23];
	       (0 << 26)  |
	       //  mode_444c422         = pre_ctrl[27:26];
	       (ppcfg->pre_field_num << 29));
	       // pre_field_num         = pre_ctrl[29];

	if (DIM_IS_IC(SC2))
		op->wr(MCDI_LMV_GAINTHD,  (3 << 20));
	else
		op->wr(DI_BLEND_CTRL,  (3 << 20));

	op->wr(DI_POST_CTRL,
	       (1 << 0) | // di_post_en      = post_ctrl[0];
	       (1 << 1) | // di_blend_en     = post_ctrl[1];
	       (1 << 2) | // di_ei_en        = post_ctrl[2];
	       (1 << 3) | // di_mux_en       = post_ctrl[3];
	       (ppcfg->post_wr_en << 4) | // di_wr_bk_en     = post_ctrl[4];
	       (ppcfg->link_vpp   << 5) | // di_vpp_out_en   = post_ctrl[5];
	       (0 << 6) | // reg_post_mb_en  = post_ctrl[6];
	       (0 << 10) | // di_post_drop_1st= post_ctrl[10];
	       (0 << 11) | // di_post_repeat  = post_ctrl[11];
	       (!ppcfg->pre_field_num << 29));
	       // post_field_num  = post_ctrl[29];

	op->wr(MCDI_MC_CRTL, 0xc0a01);

	op->bwr(DI_SC2_PRE_GL_THD, ppcfg->hold_line, 16, 6);
	op->bwr(DI_SC2_POST_GL_THD, ppcfg->hold_line, 16, 6);

	op->wr(DI_TOP_PRE_CTRL,
	       (1  << 0) |  // nrwr_path_sel     = top_pre_ctrl[1:0];
	       (0  << 2) |  // afbce_path_sel    = top_pre_ctrl[3:2];
	       (0  << 4) |  // afbc_vd_sel[2:0]  = top_pre_ctrl[6:4];
	       (1  << 10) |  // nr_ch0_en         = top_pre_ctrl[10];
	       (0  << 12) |  // pre_bypass_ctrl   = top_pre_ctrl[19:12];
	       (0  << 20) |  // fix_disable_pre   = top_pre_ctrl[21:20];
	       (1  << 30)); // pre_frm_sel      = top_pre_ctrl[31:30];
	       //0:internal  1:pre-post link(pre timming)  2:viu  3:vcp(vdin)

	op->wr(DI_TOP_POST_CTRL,
	       (1 << 0)  |
	       // diwr_path_sel     =top_post_ctrl[1:0];
	       (0 << 4)   |
	       // afbc_vd_sel[5:3]  =top_post_ctrl[6:4];
	       (0  << 12) |
	       // post_bypass_ctrl  = top_post_ctrl[19:12];
	       (0  << 20) |
	       // fix_disable_post  = top_post_ctrl[21:20];
	       ((ppcfg->link_vpp ? 0 : 1) << 30));
	       // post_frm_sel    =top_post_ctrl[3];
	       //0:viu  1:internal  2:pre-post link (post timming)

	//Wr_reg_bits(DI_TOP_CTRL,!di_link_vpp,0,1);

	if (ppcfg->link_vpp == 0)
		op->wr(DI_SC2_PRE_GL_CTRL, 0x80200001);
}

// the input data is 4:2:2,  saved in field mode video.
static void enable_prepost_link_afbce(struct DI_PREPST_AFBC_S *pafcfg,
				      const struct reg_acc *opin)
{
	int pre_hsize;
	int pre_vsize;
	const struct reg_acc *op;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;

	// clock gate
	if (pafcfg->link_vpp == 0)
		op->wr(DI_SC2_PRE_GL_CTRL, 0xc0200001);

	//open all mif
	op->bwr(DI_TOP_CTRL, 0x2c688, 4, 18);

	/****   DI PRE MIF CONFIG  ********/

	pre_hsize = pafcfg->nrwr_afbc->enc_win_end_h -
		pafcfg->nrwr_afbc->enc_win_bgn_h + 1;//todo
	pre_vsize = pafcfg->nrwr_afbc->enc_win_end_v -
		pafcfg->nrwr_afbc->enc_win_bgn_v + 1;//todo

	//printf("pre_scop : %d %d %d %d\n",di_nrwr_afbc->enc_win_bgn_h,
		 //di_nrwr_afbc->enc_win_end_h,
		 //di_nrwr_afbc->enc_win_bgn_v,di_nrwr_afbc->enc_win_end_v);

	//config read mif
	set_afbcd_mult_simple(EAFBC_DEC2_DI, pafcfg->inp_afbc, op);
	set_afbcd_mult_simple(EAFBC_DEC3_MEM, pafcfg->mem_afbc, op);
	set_afbcd_mult_simple(EAFBC_DEC_CHAN2, pafcfg->chan2_afbc, op);
	set_afbcd_mult_simple(EAFBC_DEC_IF1, pafcfg->if1_afbc, op);

	// set nr wr mif interface.
	set_afbce_cfg_v1(1, 1, pafcfg->nrwr_afbc, op);
			 //int index,
			 //0:vdin_afbce 1:di_afbce0 2:di_afbce1
			 //int       enable      ,//open  afbce
			 //AFBCE_t  *afbce

	op->wr(NR4_TOP_CTRL,
		(0 << 20) |  //reg_gclk_ctrl             = reg_top_ctrl[31:20];
		(1 << 19) |  //reg_text_en               = reg_top_ctrl[19]   ;
		(1 << 18) |  //reg_nr4_mcnr_en           = reg_top_ctrl[18]   ;
		(1 << 17) |  //reg_nr2_en                = reg_top_ctrl[17]   ;
		(1 << 16) |  //reg_nr4_en                = reg_top_ctrl[16]   ;
		(1 << 15) |  //reg_nr2_proc_en           = reg_top_ctrl[15]   ;
		(0 << 14) |  //reg_det3d_en              = reg_top_ctrl[14]   ;
		(0 << 13) |  //di_polar_en               = reg_top_ctrl[13]   ;
		(0 << 12) |  //reg_cfr_enable            = reg_top_ctrl[12]   ;
		(7 << 9) |  //reg_3dnr_en_l      = reg_top_ctrl[11:9] ;  // u3
		(7 << 6) |  //reg_3dnr_en_r      = reg_top_ctrl[8:6]  ;  // u3
		(1 << 5) |  //reg_nr4_inbuf_ctrl        = reg_top_ctrl[5]    ;
		(1 << 4) |  //reg_nr4_snr2_en           = reg_top_ctrl[4]    ;
		(0 << 3) |  //reg_nr4_scene_change_en   = reg_top_ctrl[3]    ;
		(1 << 2) |  //nr2_sw_en                 = reg_top_ctrl[2]    ;
		(1 << 1) |  //reg_cue_en                = reg_top_ctrl[1]    ;
		(0 << 0)); //reg_nr4_scene_change_flg  = reg_top_ctrl[0]    ;

	// motion write mif
	op->wr(MTNWR_X, (2 << 30) | (pafcfg->mtnwr_mif->start_x << 16) |
	       (pafcfg->mtnwr_mif->end_x));   // start_x 0 end_x 719.
	op->wr(MTNWR_Y, (pafcfg->mtnwr_mif->start_y << 16) |
	       (pafcfg->mtnwr_mif->end_y));   // start_y 0 end_y 239.
	op->wr(MTNWR_CAN_SIZE,
	       ((pafcfg->mtnwr_mif->end_x - pafcfg->mtnwr_mif->start_x) << 16) |
	       (pafcfg->mtnwr_mif->end_y - pafcfg->mtnwr_mif->start_y));
	op->wr(MTNWR_CTRL, pafcfg->mtnwr_mif->canvas_num |  // canvas index.
	       (1 << 12));       // req_en.

	// config 1bit motion and mcdi mif (PRE)
	set_mcdi_def(pafcfg->mcvecwr_mif,
		     pre_hsize - 1, pre_vsize - 1, op);
	set_mcdi_pre(pafcfg->mcinford_mif,
		     pafcfg->mcvecwr_mif, pafcfg->mcinfowr_mif, op);
	set_cont_mif(pafcfg->contprd_mif,
		     pafcfg->contp2rd_mif, pafcfg->contwr_mif, op);
	op->bwr(DI_MTN_CTRL1, pafcfg->cont_ini_en, 30, 1);
	op->bwr(MCDI_CTRL_MODE, pafcfg->mcinfo_rd_en, 28, 1);

	/**********************************/
	/****   DI POST MIF CONFIG ********/
	/**********************************/
	//motion read mif
	op->wr(MTNRD_SCOPE_X, (pafcfg->mtnrd_mif->start_x) |
	       (pafcfg->mtnrd_mif->end_x << 16));   // start_x 0 end_x 719.
	op->wr(MTNRD_SCOPE_Y, (pafcfg->mtnrd_mif->start_y) |
	       (pafcfg->mtnrd_mif->end_y << 16));   // start_y 0 end_y 239.
	op->wr(MTNRD_CTRL1, pafcfg->mtnrd_mif->canvas_num << 16 | //canvas index
		2 << 8 | //burst len = 2
		0 << 6 | //little endian
		0 << 0);//pack mode

	//mc vector read mif
	set_mcdi_post(pafcfg->mcvecrd_mif, op);

	//enable DNR, and disable DNR chroma
	//Wr_reg_bits(DNR_CTRL,0,9,1); //disable chroma
	//Wr_reg_bits(DNR_CTRL,1,16,1);
	//Wr_reg_bits(DNR_DM_CTRL,0,8,1);

	// di post write
	//Wr(DI_DIWR_X,
	     //(di_diwr_mif->luma_x_start0 <<16) | (di_diwr_mif->luma_x_end0));
	     // start_x 0 end_x 719.
	//Wr(DI_DIWR_Y,
	     //(di_diwr_mif->luma_y_start0 <<16) | (di_diwr_mif->luma_y_end0));
	     // start_y 0 end_y 239.
	//Wr(DI_DIWR_CANVAS,di_diwr_mif->canvas0_addr0 |
	     //(di_diwr_mif->canvas0_addr1<<8));
	set_afbce_cfg_v1(2, 1, pafcfg->diwr_afbc, op);
		//int       index       ,
		//0:vdin_afbce 1:di_afbce0 2:di_afbce1
		//int       enable      ,
		//open  afbce
		//AFBCE_t  *afbce

	op->wr(DI_SC2_DIWR_CTRL, (1 << 0) |   // write mif en.
	       (0 << 1) |   // bit10 mode
	       (0 << 2) |   // little endian
	       (0 << 3) |   // data ext enable
	       (5 << 4) |   // word limit
	       (0 << 16) |   // urgent
	       (0 << 17) |   // swap cbcrworking in rgb mode =2: swap cbcr
	       (0 << 18) |   // vconv working in rgb mode =2:
	       (0 << 20) |   // hconv. output even pixel
	       (0 << 22) |   // rgb mode =0, 422 YCBCR to one canvas.
	       (0 << 24) |   // no gate clock
	       (0 << 25) |   // canvas_sync enable
	       (2 << 26) |   // burst lim
	       (0 << 30));   // 64-bits swap enable

	/**********************************/
	/****** control REG config  *******/
	/**********************************/
	op->bwr(LBUF_TOP_CTRL, 0, 20, 6);
	op->bwr(LBUF_TOP_CTRL, 0, 17, 1);// fmt444 disable

	op->wr(DI_PRE_SIZE,   (pre_hsize - 1) | ((pre_vsize - 1) << 16));
	op->wr(DI_POST_SIZE,  (pre_hsize - 1) | ((pre_vsize * 2 - 1) << 16));

	op->wr(DI_INTR_CTRL, (0 << 16) |       // mask nrwr interrupt.
	       (0 << 17) |       // enable mtnwr interrupt.
	       (0 << 18) |       // mask diwr interrupt.
	       (1 << 19) |       // mask hist check interrupt.
	       (0 << 20) |       // mask cont interrupt.
	       (1 << 21) |       // mask medi interrupt.
	       (0 << 22) |       // mask vecwr interrupt.
	       (0 << 23) |       // mask infwr interrupt.
	       (1 << 24) |       // mask det3d interrupt.
	       (1 << 25) |       // mask nrds write.
	       (1 << 30) |       // mask both pre and post write done
	       0x3ff);            // clean all pending interrupt bits.

	op->wr(DI_PRE_CTRL,
	       (1 << 0)  |  //  nr_en                = pre_ctrl[0];
	       (1 << 1)  |  //  mtn_en               = pre_ctrl[1];
	       (1 << 2)  |  //  check322p_en         = pre_ctrl[2];
	       (1 << 3)  |  //  check222p_en         = pre_ctrl[3];
	       (1 << 4)  |  //  check_after_nr       = pre_ctrl[4];
	       (0 << 5)  |  //  chan2_hist_en        = pre_ctrl[5];
	       (1 << 6)  |  //  mtn_after_nr         = pre_ctrl[6];
	       (0 << 7)  |  //  pd_mtn_swap          = pre_ctrl[7];
	       (1 << 8)  |
	       //  di_buf3_en           = pre_ctrl[8]; //mem,   p2 field
	       (1 << 9)  |
	       //  di_buf2_en           = pre_ctrl[9]; //chan2, p1 field
	       (0 << 10)  |
	       //  nr_wr_by             = pre_ctrl[10];
	       (2 << 11)  |
	       //  reg_tfbf_en          = pre_ctrl[12:11];//reg_tfbf_en
	       (1 << 16)  |
	       //  reg_me_en            = pre_ctrl[16];//mcdi
	       (1 << 17)  |
	       //  reg_me_autoen        = pre_ctrl[17];
	       (0 << 19)  |
	       //  pre_field_num_mcdi   = pre_ctrl[19] ^ pre_field_num;
	       (0 << 20)  |
	       //  pre_field_num_pulldow= pre_ctrl[20] ^ pre_field_num;
	       (0 << 21)  |
	       //  pre_field_num_nr     = pre_ctrl[21] ^ pre_field_num;
	       (0 << 23)  |
	       //  mode_422c444         = pre_ctrl[24:23];
	       (0 << 26)  |
	       //  mode_444c422         = pre_ctrl[27:26];
	       (pafcfg->pre_field_num << 29));
	       // pre_field_num         = pre_ctrl[29];

	//Wr(DI_TOP_PRE_CTRL,
	//   (2  << 0  ) | // nrwr_path_sel
	//   (7  << 4  ) | // afbc_vd_sel
	//   (1  << 10 ) | // nr_ch0_en
	//   (0  << 12 ) | // afbc_gclk_ctrl
	//   (0  << 24 ) | // afbce_gclk_ctrl
	//   ((di_link_vpp?2:0)<< 30 ) );
	// pre_frm_sel = top_pre_ctrl[31:30];
	//0:internal  1:pre-post link  2:viu  3:vcp(vdin)

	if (DIM_IS_IC(SC2))
		op->wr(MCDI_LMV_GAINTHD,  (3 << 20));
	else
		op->wr(DI_BLEND_CTRL,  (3 << 20));

	op->wr(DI_POST_CTRL,
	       (1 << 0) | // di_post_en      = post_ctrl[0];
	       (1 << 1) | // di_blend_en     = post_ctrl[1];
	       (1 << 2) | // di_ei_en        = post_ctrl[2];
	       (1 << 3) | // di_mux_en       = post_ctrl[3];
	       (pafcfg->post_wr_en << 4) |
	       // di_wr_bk_en     = post_ctrl[4];
	       (pafcfg->link_vpp   << 5) |
	       // di_vpp_out_en   = post_ctrl[5];
	       (0 << 6) | // reg_post_mb_en  = post_ctrl[6];
	       (0 << 10) | // di_post_drop_1st= post_ctrl[10];
	       (0 << 11) | // di_post_repeat  = post_ctrl[11];
	       (!pafcfg->pre_field_num << 29));
	       // post_field_num  = post_ctrl[29];

	op->wr(MCDI_MC_CRTL, 0xc0a01);

	op->bwr(DI_SC2_PRE_GL_THD, pafcfg->hold_line, 16, 6);
	op->bwr(DI_SC2_POST_GL_THD, pafcfg->hold_line, 16, 6);

	op->wr(DI_TOP_PRE_CTRL,
	       (2  << 0) |  // nrwr_path_sel     = top_pre_ctrl[1:0];
	       (7  << 4) |  // afbc_vd_sel[2:0]  = top_pre_ctrl[6:4];
	       (1  << 10) |  // nr_ch0_en         = top_pre_ctrl[10];
	       (0  << 12) |  // pre_bypass_ctrl   = top_pre_ctrl[19:12];
	       (0  << 20) |  // fix_disable_pre   = top_pre_ctrl[21:20];
	       (1  << 30));
	       // pre_frm_sel= top_pre_ctrl[31:30];
	       //0:internal  1:pre-post link  2:viu  3:vcp(vdin)

	op->wr(DI_TOP_POST_CTRL,
	       (2 << 0)  |
	       // diwr_path_sel     =top_post_ctrl[1:0];
	       (7 << 4)   |
	       // afbc_vd_sel[5:3]  =top_post_ctrl[6:4];
	       (0 << 12) |
	       // post_bypass_ctrl  = top_post_ctrl[19:12];
	       (0 << 20) |
	       // fix_disable_post  = top_post_ctrl[21:20];
	       ((pafcfg->link_vpp ? 0 : 1) << 30));
		// post_frm_sel =top_post_ctrl[3];
		//0:viu  1:internal  2:pre-post link (post timming)

	//Wr_reg_bits(DI_TOP_CTRL,!di_link_vpp,0,1);

	if (pafcfg->link_vpp == 0)
		op->wr(DI_SC2_PRE_GL_CTRL, 0x80200001);
}

void set_di_memcpy_rot(struct mem_cpy_s *cfg)
{
	struct AFBCD_S    *in_afbcd;
	struct AFBCE_S    *out_afbce;
	struct DI_MIF_S  *in_rdmif;
	struct DI_SIM_MIF_s  *out_wrmif;
	bool         afbc_en;
	bool	afbcd_rot = false;
//	unsigned int         hold_line = cfg->hold_line;
	const struct reg_acc *op;
	bool is_4k = false;

	//------------------
	unsigned int hsize_int;
	unsigned int vsize_int;
	//ary no use    uint32_t m3_index;
	unsigned int afbc_vd_sel = 0;

	if (!cfg) {
		//PR_ERR("%s:cfg is null\n", __func__);
		return;
	}
	in_afbcd	= cfg->in_afbcd;
	out_afbce	= cfg->out_afbce;
	in_rdmif	= cfg->in_rdmif;
	out_wrmif = cfg->out_wrmif;
	afbc_en	= cfg->afbc_en;

	if (!cfg->opin)
		op = &di_pre_regset;
	else
		op = cfg->opin;

	dim_print("%s:\n", __func__);
	op->wr(DI_SC2_POST_GL_CTRL, 0xc0200001);
#ifdef ARY_MARK
	//disable all read mif
	op->bwr(DI_TOP_CTRL, 0x3ffff, 4, 18);
#endif
	//dim_print("%s:2\n", __func__);

	if (afbc_en) {
		if (!in_afbcd || !out_afbce) {
			//PR_ERR("%s:afbc_en[%d],noinput\n", __func__, afbc_en);
			afbc_en = 0;
		}
	} else {
		if (!in_rdmif || !out_wrmif) {
			//PR_ERR("%s:2:input none\n", __func__);
			return;
		}
	}
	//dim_print("%s:3\n", __func__);
	// config memory interface
	if (afbc_en) {
		hsize_int = out_afbce->enc_win_end_h -
			out_afbce->enc_win_bgn_h + 1;
		vsize_int = out_afbce->enc_win_end_v -
			out_afbce->enc_win_bgn_v + 1;

		if (hsize_int > 1920 || vsize_int > 1920)
			is_4k = true;
		//set_afbcd_mult_simple_v1(3,in_afbcd);
		//set_afbcd_mult_simple_v1(in_afbcd->index,in_afbcd);

		opl2()->afbcd_set(in_afbcd->index, in_afbcd, op);

		afbcd_rot = in_afbcd->rot_en;
		set_afbce_cfg_v1(2, 1, out_afbce, op);
			/*index, //0:vdin_afbce 1:di_afbce0 2:di_afbce1 */
			/*enable, //open  afbce */
	} else {
		set_di_mif_v1(in_rdmif, in_rdmif->mif_index, op);

		hsize_int = out_wrmif->end_x - out_wrmif->start_x + 1;
		vsize_int = out_wrmif->end_y - out_wrmif->start_y + 1;
		set_wrmif_simple_v3(out_wrmif, op, EDI_MIFSM_WR);
	}
	//dim_print("%s:4:\n", __func__);
#ifdef ARY_MARK
	op->wr(DI_INTR_CTRL, (1 << 16) |       // mask nrwr interrupt.
		(1 << 17) |       // enable mtnwr interrupt.
		(0 << 18) |       // mask diwr interrupt.
		(1 << 19) |       // mask hist check interrupt.
		(1 << 20) |       // mask cont interrupt.
		(1 << 21) |       // mask medi interrupt.
		(1 << 22) |       // mask vecwr interrupt.
		(1 << 23) |       // mask infwr interrupt.
		(1 << 24) |       // mask det3d interrupt.
		(1 << 25) |       // mask nrds write.
		(1 << 30) |       // mask both pre and post write done
		0x3ff);         // clean all pending interrupt bits.
#endif
	op->wr(DI_POST_SIZE,  (hsize_int - 1) | ((vsize_int - 1) << 16));
#ifdef ARY_MARK
	afbc_vd_sel = afbc_en ? (1 << in_afbcd->index) : 0;
#else
	if (in_afbcd) {
		switch (in_afbcd->index) {
		case EAFBC_DEC_IF0:
			afbc_vd_sel = 0x02 << 3;
			break;
		case EAFBC_DEC_IF1:
			afbc_vd_sel = 0x01 << 3;
			break;
		case EAFBC_DEC_IF2:
			afbc_vd_sel = 0x04 << 3;
			break;
		default:
			//PR_ERR("%s:index\n", __func__);
			afbc_vd_sel = 0x02 << 3;
			break;
		}
	}
#endif
	//bypass en
	op->wr(DI_POST_CTRL,
		(1 << 0)	| // di_post_en      = post_ctrl[0];
		(0 << 1)	| // di_blend_en     = post_ctrl[1];
		(0 << 2)	| // di_ei_en        = post_ctrl[2];
		(0 << 3)	| // di_mux_en       = post_ctrl[3];
		(1 << 4)	| // di_wr_bk_en     = post_ctrl[4];
		(0 << 5)	| // di_vpp_out_en   = post_ctrl[5];
		(0 << 6)	| // reg_post_mb_en  = post_ctrl[6];
		(0 << 10)	| // di_post_drop_1st= post_ctrl[10];
		(0 << 11)	| // di_post_repeat  = post_ctrl[11];
		(0 << 29)); // post_field_num  = post_ctrl[29];

	op->wr(DI_TOP_POST_CTRL,
		/* diwr_path_sel    = top_post_ctrl[1:0]; */
		((afbc_en ? 2 : 1)			<< 0) |
		/* afbc_vd_sel[5:3] = top_post_ctrl[6:4]; */
		((afbc_en ? (afbc_vd_sel >> 3) : 0)	<< 4) |
		/* afbcd_m[5:3] is_4k = top_post_ctrl[9:7] */
		(((afbcd_rot | is_4k) ? 1 : 0)		<< 7) |
		/* post_bypass_ctrl = top_post_ctrl[19:12]; */
		(cfg->port	<< 12) |
		/* fix_disable_post = top_post_ctrl[21:20]; */
		(0		<< 20) |
		/* post_frm_sel   =top_post_ctrl[3];//0:viu  1:internal */
		(1		<< 30));
	if (is_4k)
		dim_sc2_4k_set(2);
	#ifdef ARY_MARK
	else
		dim_sc2_4k_set(0);
	#endif
#ifdef ARY_MARK
	/* closr pr/post_link */
	op->bwr(DI_TOP_PRE_CTRL, 0, 30, 2);

	/* pre afbc_vd_sel[2:0] */
	op->bwr(DI_TOP_PRE_CTRL, afbc_vd_sel & 0x7, 4, 3);

	op->bwr(DI_TOP_CTRL, in_afbcd->index, 13, 3);
#endif

	// post start
	op->wr(DI_SC2_POST_GL_CTRL, 0x80200001);
}

/* ary add this for maybe from is non-afbc and out is afbc
 * copy from set_di_memcpy_rot
 */
void set_di_memcpy(struct mem_cpy_s *cfg)
{
	struct AFBCD_S    *in_afbcd;
	struct AFBCE_S    *out_afbce;
	struct DI_MIF_S  *in_rdmif;
	struct DI_SIM_MIF_s  *out_wrmif;
	bool         afbc_en	= false;
	bool	afbcd_rot = false;
//	unsigned int         hold_line = cfg->hold_line;
	const struct reg_acc *op;
	bool is_4k = false;

	//------------------
	unsigned int hsize_int;
	unsigned int vsize_int;
	//ary no use    uint32_t m3_index;
	unsigned int afbc_vd_sel = 0;

	if (!cfg) {
		//PR_ERR("%s:cfg is null\n", __func__);
		return;
	}
	in_afbcd	= cfg->in_afbcd;
	out_afbce	= cfg->out_afbce;
	in_rdmif	= cfg->in_rdmif;
	out_wrmif	= cfg->out_wrmif;

	if (out_afbce)
		afbc_en		= true;

	if (!cfg->opin)
		op = &di_pre_regset;
	else
		op = cfg->opin;

	dim_print("%s:\n", __func__);
	op->wr(DI_SC2_POST_GL_CTRL, 0xc0200001);

	//dim_print("%s:2\n", __func__);

	// config memory interface
	/* input */
	if (in_afbcd) {
		//dbg_copy("inp:afbcd:\n");
		opl2()->afbcd_set(in_afbcd->index, in_afbcd, op);
	} else {
		dbg_copy("inp:mif:%d\n", in_rdmif->mif_index);
		set_di_mif_v1(in_rdmif, in_rdmif->mif_index, op);
	}

	if (out_afbce) {
		dbg_copy("out:afbce:\n");
		hsize_int = out_afbce->enc_win_end_h -
			out_afbce->enc_win_bgn_h + 1;
		vsize_int = out_afbce->enc_win_end_v -
			out_afbce->enc_win_bgn_v + 1;

		set_afbce_cfg_v1(2, 1, out_afbce, op);
			/* index, 0:vdin_afbce 1:di_afbce0 2:di_afbce1 */
			/* enable, open afbce */
	} else {
		dbg_copy("out:mif:\n");
		hsize_int = out_wrmif->end_x - out_wrmif->start_x + 1;
		vsize_int = out_wrmif->end_y - out_wrmif->start_y + 1;
		set_wrmif_simple_v3(out_wrmif, op, EDI_MIFSM_WR);
	}
	//dbg_copy("%s:4:\n", __func__);

	if (hsize_int > 1920 || vsize_int > 1920)
		is_4k = true;

	op->wr(DI_POST_SIZE, (hsize_int - 1) | ((vsize_int - 1) << 16));

	if (in_afbcd) {
		switch (in_afbcd->index) {
		case EAFBC_DEC_IF0:
			afbc_vd_sel = 0x02 << 3;
			break;
		case EAFBC_DEC_IF1:
			afbc_vd_sel = 0x01 << 3;
			break;
		case EAFBC_DEC_IF2:
			afbc_vd_sel = 0x04 << 3;
			break;
		default:
			//PR_ERR("%s:index\n", __func__);
			afbc_vd_sel = 0x02 << 3;
			break;
		}
	}

	//bypass en
	op->wr(DI_POST_CTRL,
		(1 << 0)	| // di_post_en      = post_ctrl[0];
		(0 << 1)	| // di_blend_en     = post_ctrl[1];
		(0 << 2)	| // di_ei_en        = post_ctrl[2];
		(0 << 3)	| // di_mux_en       = post_ctrl[3];
		(1 << 4)	| // di_wr_bk_en     = post_ctrl[4];
		(0 << 5)	| // di_vpp_out_en   = post_ctrl[5];
		(0 << 6)	| // reg_post_mb_en  = post_ctrl[6];
		(0 << 10)	| // di_post_drop_1st= post_ctrl[10];
		(0 << 11)	| // di_post_repeat  = post_ctrl[11];
		(0 << 29)); // post_field_num  = post_ctrl[29];

	op->wr(DI_TOP_POST_CTRL,
		/* diwr_path_sel    = top_post_ctrl[1:0]; */
		((afbc_en ? 2 : 1)			<< 0)	|
		/* afbc_vd_sel[5:3] = top_post_ctrl[6:4]; */
		((afbc_en ? (afbc_vd_sel >> 3) : 0)	<< 4)	|
		/* afbcd_m[5:3] is_4k = top_post_ctrl[9:7] */
		(((afbcd_rot | is_4k) ? 1 : 0)		<< 7)	|
		/* post_bypass_ctrl = top_post_ctrl[19:12]; */
		(cfg->port	<< 12)	|
		/* fix_disable_post = top_post_ctrl[21:20];*/
		(0		<< 20)	|
		/* post_frm_sel   =top_post_ctrl[3];//0:viu  1:internal*/
		(1		<< 30));

	if (is_4k)
		dim_sc2_4k_set(2);

	// post start
	op->wr(DI_SC2_POST_GL_CTRL, 0x80200001);
}

/*ary change for test on g12a*/
void set_di_mif_v3(struct DI_MIF_S *mif, enum DI_MIF0_ID mif_index,
		   const struct reg_acc *opin)
{
	u32 bytes_per_pixel = 0;
	//0 = 1 byte per pixel,1 = 2 bytes per pixel,2 = 3 bytes per pixel
	u32 demux_mode			= 0;
	u32 reset_bit			= 1;
	u32 chro_rpt_lastl_ctrl	= 0;
	u32 luma0_rpt_loop_start	= 0;
	u32 luma0_rpt_loop_end		= 0;
	u32 luma0_rpt_loop_pat		= 0;
	u32 vfmt_rpt_first = 0;
	u32 chroma0_rpt_loop_start	= 0;
	u32 chroma0_rpt_loop_end	= 0;
	u32 chroma0_rpt_loop_pat	= 0;
	int      hfmt_en      = 1;
	int      hz_yc_ratio  = 0;
	int      hz_ini_phase = 0;
	int      vfmt_en      = 0;
	int      vt_yc_ratio  = 0;
	int      vt_ini_phase = 0;
	int      y_length     = 0;
	int      c_length     = 0;
	int      hz_rpt       = 0;
	int vt_phase_step	= 0;// = (16 >> vt_yc_ratio);
	int urgent = mif->urgent;
	int hold_line = mif->hold_line;
	unsigned int off;
	const unsigned int *reg;
	const struct reg_acc *op;
	int nrpt_phase0_en = 1;
	unsigned int burst_len = 2;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;

	if (is_mask(SC2_REG_MSK_nr)) { /* dbg */
		op = &sc2reg;
		PR_INF("%s:%s:\n", __func__, dim_get_mif_id_name(mif_index));
	}

	reg = mif_reg_get_v3();
	off = di_mif_add_get_offset_v3(mif_index);

	if (off == DIM_ERR || !reg) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	dbg_ic("%s:id[%d]\n", __func__, mif_index);
	if (mif->set_separate_en != 0 && mif->src_field_mode == 1) {
		if (mif->video_mode == 0)
			chro_rpt_lastl_ctrl = 1;
		else
			chro_rpt_lastl_ctrl = 0;

		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 1;
		chroma0_rpt_loop_end = 1;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x80;
	} else if (mif->set_separate_en != 0 && mif->src_field_mode == 0) {
		if (mif->video_mode == 0)
			chro_rpt_lastl_ctrl = 1;
		else
			chro_rpt_lastl_ctrl = 0;

		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x0;
		chroma0_rpt_loop_pat = 0x0;
	} else if (mif->set_separate_en == 0 && mif->src_field_mode == 1) {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 1;
		luma0_rpt_loop_end = 1;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x80;
		chroma0_rpt_loop_pat = 0x00;
		op->wr(off + reg[MIF_LUMA_FIFO_SIZE], 0xC0);
	} else {
		chro_rpt_lastl_ctrl = 0;
		luma0_rpt_loop_start = 0;
		luma0_rpt_loop_end = 0;
		chroma0_rpt_loop_start = 0;
		chroma0_rpt_loop_end = 0;
		luma0_rpt_loop_pat = 0x00;
		chroma0_rpt_loop_pat = 0x00;
		op->wr(off + reg[MIF_LUMA_FIFO_SIZE], 0xC0);
	}

	bytes_per_pixel = (mif->set_separate_en) ?
		0 : ((mif->video_mode == 2) ? 2 : 1);

	bytes_per_pixel = mif->bit_mode == 3 ? 1 : bytes_per_pixel;
	// 10bit full pack or not

	demux_mode = (mif->set_separate_en == 0) ?
			((mif->video_mode == 1) ? 0 :  1) : 0;

	// ----------------------
	// General register
	// ----------------------
	if (mif_index == DI_MIF0_ID_INP) {
		if (mif->canvas_w % 32)
			burst_len = 0;
		else if (mif->canvas_w % 64)
			burst_len = 1;

		if (mif->block_mode) {
			if (burst_len >= 1)
				burst_len = 1;
		} else {
			if (burst_len >= 2)
				burst_len = 2;
		}
	}
	dim_print("burst_len=%d\n", burst_len);
	if (mif->linear) {
		op->wr(off + reg[MIF_GEN_REG3],
			7 << 24		|
			mif->block_mode << 22 |
			mif->block_mode	<< 20 |
			mif->block_mode << 18 |
			burst_len << 14	  |	//2 << 1 |    // use bst4
			burst_len << 12	  |	//2 << 1 |    // use bst4
		       mif->bit_mode << 8 |    // bits_mode
		       3 << 4		  |    // block length
		       burst_len << 1	  |	//2 << 1 |    // use bst4
		       mif->reg_swap << 0);   //64 bit swap
	} else {
		op->wr(off + reg[MIF_GEN_REG3],
		       mif->bit_mode << 8 |    // bits_mode
		       3 << 4		  |    // block length
		       burst_len << 1	  |	//2 << 1 |    // use bst4
		       mif->reg_swap << 0);   //64 bit swap
	}

	if (!is_mask(SC2_REG_MSK_GEN_PRE)) {
		op->wr(off + reg[MIF_GEN_REG],
			(reset_bit << 29)          | // reset on go field
			(urgent << 28)             | // chroma urgent bit
			(urgent << 27)             | // luma urgent bit.
			(1 << 25)                  | // no dummy data.
			(hold_line << 19)          | // hold lines
			(1 << 18)                  | // push dummy pixel
			(demux_mode << 16)         | // demux_mode
			(bytes_per_pixel << 14)    |
			(mif->burst_size_cr << 12) |
			(mif->burst_size_cb << 10) |
			(mif->burst_size_y << 8)   |
			(chro_rpt_lastl_ctrl << 6) |
			(mif->l_endian << 4)		| /* 2020-12-29 ?*/
			((mif->set_separate_en != 0) << 1)      |
			(1 << 0)                     // cntl_enable
			);
	}
	if (mif->set_separate_en == 2) {
		// Enable NV12 Display
		op->wr(off + reg[MIF_GEN_REG2], 1);
		if (mif->cbcr_swap)
			op->bwr(off + reg[MIF_GEN_REG2], 2, 0, 2);
	} else {
		op->wr(off + reg[MIF_GEN_REG2], 0);
	}

	// reverse X and Y
	op->bwr(off + reg[MIF_GEN_REG2], ((mif->rev_y << 1) |
		(mif->rev_x)), 2, 2);

	// ----------------------
	// Canvas
	// ----------------------
	if (mif->linear)
		di_mif0_linear_rd_cfg(mif, mif_index, op);
	else
		op->wr(off + reg[MIF_CANVAS0], (mif->canvas0_addr2 << 16) |
			// cntl_canvas0_addr2
			(mif->canvas0_addr1 << 8)      | // cntl_canvas0_addr1
			(mif->canvas0_addr0 << 0)        // cntl_canvas0_addr0
			);

	// ----------------------
	// Picture 0 X/Y start,end
	// ----------------------
	op->wr(off + reg[MIF_LUMA_X0], (mif->luma_x_end0 << 16) |
	// cntl_luma_x_end0
		(mif->luma_x_start0 << 0)        // cntl_luma_x_start0
		);
	op->wr(off + reg[MIF_LUMA_Y0], (mif->luma_y_end0 << 16) |
	// cntl_luma_y_end0
		(mif->luma_y_start0 << 0)        // cntl_luma_y_start0
		);
	op->wr(off + reg[MIF_CHROMA_X0], (mif->chroma_x_end0 << 16) |
		(mif->chroma_x_start0 << 0)
		);
	op->wr(off + reg[MIF_CHROMA_Y0], (mif->chroma_y_end0 << 16) |
		(mif->chroma_y_start0 << 0)
		);

	// ----------------------
	// Repeat or skip
	// ----------------------
	op->wr(off + reg[MIF_RPT_LOOP],        (0 << 28) |
		(0   << 24) |
		(0   << 20) |
		(0     << 16) |
		(chroma0_rpt_loop_start << 12) |
		(chroma0_rpt_loop_end   << 8)  |
		(luma0_rpt_loop_start   << 4)  |
		(luma0_rpt_loop_end << 0));

	op->wr(off + reg[MIF_LUMA0_RPT_PAT],      luma0_rpt_loop_pat);
	op->wr(off + reg[MIF_CHROMA0_RPT_PAT],    chroma0_rpt_loop_pat);

	// Dummy pixel value
	op->wr(off + reg[MIF_DUMMY_PIXEL],   0x00808000);
	if (mif->video_mode == 0)   {// 4:2:0 block mode.
		hfmt_en      = 1;
		hz_yc_ratio  = 1;
		hz_ini_phase = 0;
		vfmt_en      = 1;
		vt_yc_ratio  = 1;
		vt_ini_phase = 0;
		y_length     = mif->luma_x_end0 - mif->luma_x_start0 + 1;
		c_length     = mif->chroma_x_end0 - mif->chroma_x_start0 + 1;
		hz_rpt       = 0;
	} else if (mif->video_mode == 1) {
		hfmt_en      = 1;
		hz_yc_ratio  = 1;
		hz_ini_phase = 0;
		vfmt_en      = 0;
		vt_yc_ratio  = 0;
		vt_ini_phase = 0;
		y_length     = mif->luma_x_end0 - mif->luma_x_start0 + 1;
		c_length     = ((mif->luma_x_end0 >> 1) -
				(mif->luma_x_start0 >> 1) + 1);
		hz_rpt       = 0;
	} else if (mif->video_mode == 2) {
		hfmt_en      = 0;
		hz_yc_ratio  = 1;
		hz_ini_phase = 0;
		vfmt_en      = 0;
		vt_yc_ratio  = 0;
		vt_ini_phase = 0;
		y_length     = mif->luma_x_end0 - mif->luma_x_start0 + 1;
		c_length     = mif->luma_x_end0 - mif->luma_x_start0 + 1;
		hz_rpt       = 0;
	}
	vt_phase_step = (16 >> vt_yc_ratio);

	if (mif->set_separate_en != 0 && mif->src_field_mode == 1 && off == 0) {
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
		nrpt_phase0_en = 0;
	}

	op->wr(off + reg[MIF_FMT_CTRL],
		(hz_rpt << 28)       |     //hz rpt pixel
		(hz_ini_phase << 24) |     //hz ini phase
		(0 << 23)         |        //repeat p0 enable
		(hz_yc_ratio << 21)  |     //hz yc ratio
		(hfmt_en << 20)   |        //hz enable
		(nrpt_phase0_en << 17)         |        //nrpt_phase0 enable
		(vfmt_rpt_first << 16)         |        //repeat l0 enable
		(0 << 12)         |        //skip line num
		(vt_ini_phase << 8)  |     //vt ini phase
		(vt_phase_step << 1) |     //vt phase step (3.4)
		(vfmt_en << 0)             //vt enable
		);

	op->wr(off + reg[MIF_FMT_W],    (y_length << 16) |
		//hz format width
	       (c_length << 0)); //vt format width
}

/**************************************************/
void mif_reg_wr_v3(enum DI_MIF0_ID mif_index, const struct reg_acc *op,
		   unsigned int reg, unsigned int val)
{
	unsigned int off;

	off = di_mif_add_get_offset_v3(mif_index);
	op->wr((off + reg), val);
}

unsigned int mif_reg_rd_v3(enum DI_MIF0_ID mif_index, const struct reg_acc *op,
			   unsigned int reg)
{
	unsigned int off;

	off = di_mif_add_get_offset_v3(mif_index);
	return op->rd(off + reg);
}

static void hw_init_v3(void)
{
	unsigned short fifo_size_vpp = 0xc0;
	unsigned short fifo_size_di = 0xc0;
	const struct reg_acc *op = &di_pre_regset;
	unsigned int path_sel;

	if (DIM_IS_IC_EF(TXL)) {
		/* vpp fifo max size on txl :128*3=384[0x180] */
		/* di fifo max size on txl :96*3=288[0x120] */
		fifo_size_vpp = 0x180;
		fifo_size_di = 0x120;
	}

	if (DIM_IS_IC_EF(SC2)) {
		/*pre*/
		op->wr(DI_SC2_INP_LUMA_FIFO_SIZE, fifo_size_di);
		op->wr(DI_SC2_MEM_LUMA_FIFO_SIZE, fifo_size_di);
		op->wr(DI_SC2_CHAN2_LUMA_FIFO_SIZE, fifo_size_di);

		/*post*/
		op->wr(DI_SC2_IF0_LUMA_FIFO_SIZE, fifo_size_di);
		op->wr(DI_SC2_IF1_LUMA_FIFO_SIZE, fifo_size_di);
		op->wr(DI_SC2_IF2_LUMA_FIFO_SIZE, fifo_size_di);

		path_sel = 1;
		op->bwr(DI_TOP_PRE_CTRL, (path_sel & 0x3), 0, 2);
		//post_path_sel
		op->bwr(DI_TOP_POST_CTRL, (path_sel & 0x3), 0, 2);
		//post_path_sel
		//op->wr(DI_INTR_CTRL,0xcfff0000);
		op->wr(DI_INTR_CTRL, 0xcffe0000);

		PR_INF("%s:\n", __func__);
		PR_INF("\t0x%x:0x%x\n", DI_TOP_PRE_CTRL,
		       op->rd(DI_TOP_PRE_CTRL));
		PR_INF("\t0x%x:0x%x\n", DI_TOP_POST_CTRL,
		       op->rd(DI_TOP_POST_CTRL));
		PR_INF("\t0x%x:0x%x\n", DI_INTR_CTRL, op->rd(DI_INTR_CTRL));

	} else if (DIM_IS_IC_EF(G12A)) {
		/* pre */
		DIM_DI_WR(DI_INP_LUMA_FIFO_SIZE, fifo_size_di);
		/* 17d8 is DI_INP_luma_fifo_size */
		DIM_DI_WR(DI_MEM_LUMA_FIFO_SIZE, fifo_size_di);
		/* 17e5 is DI_MEM_luma_fifo_size */
		DIM_DI_WR(DI_CHAN2_LUMA_FIFO_SIZE, fifo_size_di);

		DIM_DI_WR(DI_IF1_LUMA_FIFO_SIZE, fifo_size_di);
		/* 17f2 is  DI_IF1_luma_fifo_size */
		DIM_DI_WR(DI_IF2_LUMA_FIFO_SIZE, fifo_size_di);
		/* 201a is if2 fifo size */

	} else {
		/* video */
		DIM_DI_WR(VD1_IF0_LUMA_FIFO_SIZE, fifo_size_vpp);
		DIM_DI_WR(VD2_IF0_LUMA_FIFO_SIZE, fifo_size_vpp);
		/* 1a83 is vd2_if0_luma_fifo_size */

		/* pre */
		DIM_DI_WR(DI_INP_LUMA_FIFO_SIZE, fifo_size_di);
		/* 17d8 is DI_INP_luma_fifo_size */
		DIM_DI_WR(DI_MEM_LUMA_FIFO_SIZE, fifo_size_di);
		/* 17e5 is DI_MEM_luma_fifo_size */
		DIM_DI_WR(DI_CHAN2_LUMA_FIFO_SIZE, fifo_size_di);

		/* post */
		DIM_DI_WR(DI_IF1_LUMA_FIFO_SIZE, fifo_size_di);
		/* 17f2 is  DI_IF1_luma_fifo_size */
		if (DIM_IS_IC_EF(TXL))
			DIM_DI_WR(DI_IF2_LUMA_FIFO_SIZE, fifo_size_di);
			/* 201a is if2 fifo size */
	}
}

static void di_pre_data_mif_ctrl_v3(bool enable)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	if (enable) {
		if (dim_afds() && dim_afds()->is_used())
			dim_afds()->inp_sw(true);

		if (dim_afds() && !dim_afds()->is_used_chan2())
			op->bwr(DI_SC2_CHAN2_GEN_REG, 1, 0, 1);

		if (dim_afds() && !dim_afds()->is_used_mem())
			op->bwr(DI_SC2_MEM_GEN_REG, 1, 0, 1);

		if (dim_afds() && !dim_afds()->is_used_inp())
			op->bwr(DI_SC2_INP_GEN_REG, 1, 0, 1);

		/* nrwr no clk gate en=0 */
		/*DIM_RDMA_WR_BITS(DI_NRWR_CTRL, 0, 24, 1);*/
	} else {
		/* nrwr no clk gate en=1 */
		/*DIM_RDMA_WR_BITS(DI_NRWR_CTRL, 1, 24, 1);*/
		/* nr wr req en =0 */
		op->bwr(DI_PRE_CTRL, 0, 0, 1);

		/* disable input mif*/

		op->bwr(DI_SC2_CHAN2_GEN_REG, 0, 0, 1);

		op->bwr(DI_SC2_MEM_GEN_REG, 0, 0, 1);

		op->bwr(DI_SC2_INP_GEN_REG, 0, 0, 1);

		/* disable AFBC input */
		//if (afbc_is_used()) {
		if (dim_afds() && dim_afds()->is_used()) {
			//afbc_input_sw(false);
			if (dim_afds())
				dim_afds()->inp_sw(false);
		}
	}
}

/*below for post */
static void post_mif_sw_v3(bool on, enum DI_MIF0_SEL sel)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	if (on) {
		if (sel & DI_MIF0_SEL_IF0)
			op->bwr(DI_SC2_IF0_GEN_REG, 1, 0, 1);
		if (sel & DI_MIF0_SEL_IF1)
			op->bwr(DI_SC2_IF1_GEN_REG, 1, 0, 1);
		if (sel & DI_MIF0_SEL_IF2)
			op->bwr(DI_SC2_IF2_GEN_REG, 1, 0, 1);

		if ((sel & DI_MIF0_SEL_PST_ALL) == DI_MIF0_SEL_PST_ALL)
			op->bwr(DI_POST_CTRL, 1, 4, 1); /*di_wr_bk_en*/
	} else {
		if (sel & DI_MIF0_SEL_IF0)
			op->bwr(DI_SC2_IF0_GEN_REG, 0, 0, 1);
		if (sel & DI_MIF0_SEL_IF1)
			op->bwr(DI_SC2_IF1_GEN_REG, 0, 0, 1);
		if (sel & DI_MIF0_SEL_IF2)
			op->bwr(DI_SC2_IF2_GEN_REG, 0, 0, 1);

		if ((sel & DI_MIF0_SEL_PST_ALL) == DI_MIF0_SEL_PST_ALL)
			op->bwr(DI_POST_CTRL, 0, 4, 1);	/*di_wr_bk_en*/
	}
	dim_print("%s:%d\n", __func__, on);
}

static void post_mif_rst_v3(enum DI_MIF0_SEL sel)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	//bit 31: enable free clk
	//bit 30: sw reset : pulse bit
	if (sel & DI_MIF0_SEL_IF0)
		op->wr(DI_SC2_IF0_GEN_REG, 0x3 << 30);

	if (sel & DI_MIF0_SEL_IF1)
		op->wr(DI_SC2_IF1_GEN_REG, 0x3 << 30);
	if (sel & DI_MIF0_SEL_IF2)
		op->wr(DI_SC2_IF2_GEN_REG, 0x3 << 30);
}

static void post_mif_rev_v3(bool rev, enum DI_MIF0_SEL sel)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	if (rev) {
		if (sel & DI_MIF0_SEL_IF0)
			op->bwr(DI_SC2_IF0_GEN_REG2, 3, 2, 2);
		if (sel & DI_MIF0_SEL_IF1)
			op->bwr(DI_SC2_IF1_GEN_REG2, 3, 2, 2);
		if (sel & DI_MIF0_SEL_IF2)
			op->bwr(DI_SC2_IF2_GEN_REG2,  3, 2, 2);
		#ifdef MARK_SC2	// don't set vd1/vd2
		if (sel & DI_MIF0_SEL_VD1_IF0)
			DIM_VSC_WR_MPG_BT(VD1_IF0_GEN_REG2, 0xf, 2, 4);
		if (sel & DI_MIF0_SEL_VD2_IF0)
			DIM_VSC_WR_MPG_BT(VD2_IF0_GEN_REG2, 0xf, 2, 4);
		#endif
	} else {
		if (sel & DI_MIF0_SEL_IF0)
			op->bwr(DI_SC2_IF0_GEN_REG2, 0, 2, 2);
		if (sel & DI_MIF0_SEL_IF1)
			op->bwr(DI_SC2_IF1_GEN_REG2, 0, 2, 2);
		if (sel & DI_MIF0_SEL_IF2)
			op->bwr(DI_SC2_IF2_GEN_REG2,  0, 2, 2);
		#ifdef MARK_SC2
		if (sel & DI_MIF0_SEL_VD1_IF0)
			DIM_VSC_WR_MPG_BT(VD1_IF0_GEN_REG2, 0, 2, 4);
		if (sel & DI_MIF0_SEL_VD2_IF0)
			DIM_VSC_WR_MPG_BT(VD2_IF0_GEN_REG2, 0, 2, 4);
		#endif
	}
}

static void pst_mif_update_canvasid_v3(struct DI_MIF_S *mif,
				       enum DI_MIF0_ID mif_index,
				       const struct reg_acc *opin)
{
	//const struct reg_acc *op = &sc2reg;
	const struct reg_acc *op;

	if (!opin)
		op = &di_pre_regset;
	else
		op = opin;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	if (mif->linear) {
		di_mif0_linear_rd_cfg(mif, mif_index, op);
		dbg_ic("%s:linar\n", __func__);
		return;
	}
	switch (mif_index) {
	case DI_MIF0_ID_IF1:
		op->wr(DI_SC2_IF1_CANVAS0,
			      (mif->canvas0_addr2 << 16) |
			      (mif->canvas0_addr1 << 8)  |
			      (mif->canvas0_addr0 << 0));

		break;
	case DI_MIF0_ID_IF0:

		op->wr(DI_SC2_IF0_CANVAS0,
			      (mif->canvas0_addr2 << 16) |
			      (mif->canvas0_addr1 << 8)	|
			      (mif->canvas0_addr0 << 0));

		break;
	case DI_MIF0_ID_IF2:
		op->wr(DI_SC2_IF2_CANVAS0,
					      (mif->canvas0_addr2 <<
					      16)	|
					      (mif->canvas0_addr1 <<
					      8)	|
					      (mif->canvas0_addr0 <<
					      0));
		break;
	default:
		PR_ERR("%s:overflow[%d]\n", __func__, mif_index);
		break;
	}
}

static void post_bit_mode_cfg_v3(unsigned char if0,
				 unsigned char if1,
				 unsigned char if2,
				 unsigned char post_wr)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	op->bwr(DI_SC2_IF0_GEN_REG3, if0 & 0x3, 8, 2);

	op->bwr(DI_SC2_IF1_GEN_REG3, if1 & 0x3, 8, 2);
	op->bwr(DI_SC2_IF2_GEN_REG3, if2 & 0x3, 8, 2);
}

#ifdef MARK_SC2
void dim_hw_init_reg_sc2(void)/* this is debug for check speed*/
{
	unsigned short fifo_size_post = 0x120;/*feijun 08-02*/

	if (DIM_IS_IC_EF(SC2)) {
		DIM_DI_WR(DI_IF1_LUMA_FIFO_SIZE, fifo_size_post);
		/* 17f2 is  DI_IF1_luma_fifo_size */
		DIM_DI_WR(DI_IF2_LUMA_FIFO_SIZE, fifo_size_post);
		DIM_DI_WR(DI_IF0_LUMA_FIFO_SIZE, fifo_size_post);
	}

	PR_INF("%s, 0x%x\n", __func__, DIM_RDMA_RD(DI_IF0_LUMA_FIFO_SIZE));
}

#endif

static void dbg_reg_pre_mif_print_v3(void)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	pr_info("DI_INP_GEN_REG=0x%x\n", op->rd(DI_SC2_INP_GEN_REG));
	pr_info("DI_MEM_GEN_REG=0x%x\n", op->rd(DI_SC2_MEM_GEN_REG));
	pr_info("DI_CHAN2_GEN_REG=0x%x\n", op->rd(DI_SC2_CHAN2_GEN_REG));
}

static void dbg_reg_pst_mif_print_v3(void)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	pr_info("DI_IF0_GEN_REG=0x%x\n", op->rd(DI_SC2_IF0_GEN_REG));
	pr_info("DI_IF1_GEN_REG=0x%x\n", op->rd(DI_SC2_IF1_GEN_REG));
	pr_info("DI_IF2_GEN_REG=0x%x\n", op->rd(DI_SC2_IF2_GEN_REG));
}

static void dbg_reg_pre_mif_v3_show(struct seq_file *s)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	seq_printf(s, "DI_INP_GEN_REG=0x%x\n", op->rd(DI_SC2_INP_GEN_REG));
	seq_printf(s, "DI_MEM_GEN_REG=0x%x\n", op->rd(DI_SC2_MEM_GEN_REG));
	seq_printf(s, "DI_CHAN2_GEN_REG=0x%x\n", op->rd(DI_SC2_CHAN2_GEN_REG));
}

static void dbg_reg_pst_mif_v3_show(struct seq_file *s)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	seq_printf(s, "DI_IF0_GEN_REG=0x%x\n", op->rd(DI_SC2_IF0_GEN_REG));
	seq_printf(s, "DI_IF1_GEN_REG=0x%x\n", op->rd(DI_SC2_IF1_GEN_REG));
	seq_printf(s, "DI_IF2_GEN_REG=0x%x\n", op->rd(DI_SC2_IF2_GEN_REG));
}

static void dbg_reg_pre_mif_print2_v3(void)
{
	unsigned int i = 0;
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	//op->bwr(DI_SC2_INP_GEN_REG3, 3, 10, 2);
	//op->bwr(DI_SC2_MEM_GEN_REG3, 3, 10, 2);
	//op->bwr(DI_SC2_CHAN2_GEN_REG3, 3, 10, 2);
	pr_info("mif:inp:\n");
	pr_info("\tDI_INP_GEN_REG2=0x%x.\n", op->rd(DI_SC2_INP_GEN_REG2));
	pr_info("\tDI_INP_GEN_REG3=0x%x.\n", op->rd(DI_SC2_INP_GEN_REG3));
	for (i = 0; i < 10; i++)
		pr_info("\t\t0x%8x=0x%8x.\n", DI_SC2_INP_GEN_REG + i,
			op->rd(DI_SC2_INP_GEN_REG + i));
	pr_info("mif:mem:\n");
	pr_info("\tDI_MEM_GEN_REG2=0x%x.\n", op->rd(DI_SC2_MEM_GEN_REG2));
	pr_info("\tDI_MEM_GEN_REG3=0x%x.\n", op->rd(DI_SC2_MEM_GEN_REG3));
	pr_info("\tDI_MEM_LUMA_FIFO_SIZE=0x%x.\n",
		op->rd(DI_SC2_MEM_LUMA_FIFO_SIZE));
	for (i = 0; i < 10; i++)
		pr_info("\t\t0x%8x=0x%8x.\n", DI_SC2_MEM_GEN_REG + i,
			op->rd(DI_SC2_MEM_GEN_REG + i));
	pr_info("\tDI_CHAN2_GEN_REG2=0x%x.\n", op->rd(DI_SC2_CHAN2_GEN_REG2));
	pr_info("\tDI_CHAN2_GEN_REG3=0x%x.\n", op->rd(DI_SC2_CHAN2_GEN_REG3));
	pr_info("\tDI_CHAN2_LUMA_FIFO_SIZE=0x%x.\n",
		op->rd(DI_SC2_CHAN2_LUMA_FIFO_SIZE));
	for (i = 0; i < 10; i++)
		pr_info("\t\t0x%8x=0x%8x.\n", DI_SC2_CHAN2_GEN_REG + i,
			op->rd(DI_SC2_CHAN2_GEN_REG + i));
}

static void dbg_reg_pst_mif_print2_v3(void)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	pr_info("VIU_MISC_CTRL0=0x%x\n", op->rd(VIU_MISC_CTRL0));
#ifdef MARK_SC2	/* */
	pr_info("VD1_IF0_GEN_REG=0x%x\n", op->rd(VD1_IF0_GEN_REG));
	pr_info("VD1_IF0_GEN_REG2=0x%x\n", op->rd(VD1_IF0_GEN_REG2));
	pr_info("VD1_IF0_GEN_REG3=0x%x\n", op->rd(VD1_IF0_GEN_REG3));
	pr_info("VD1_IF0_LUMA_X0=0x%x\n", op->rd(VD1_IF0_LUMA_X0));
	pr_info("VD1_IF0_LUMA_Y0=0x%x\n", op->rd(VD1_IF0_LUMA_Y0));
	pr_info("VD1_IF0_CHROMA_X0=0x%x\n", op->rd(VD1_IF0_CHROMA_X0));
	pr_info("VD1_IF0_CHROMA_Y0=0x%x\n", op->rd(VD1_IF0_CHROMA_Y0));
	pr_info("VD1_IF0_LUMA_X1=0x%x\n", op->rd(VD1_IF0_LUMA_X1));
	pr_info("VD1_IF0_LUMA_Y1=0x%x\n", op->rd(VD1_IF0_LUMA_Y1));
	pr_info("VD1_IF0_CHROMA_X1=0x%x\n", op->rd(VD1_IF0_CHROMA_X1));
	pr_info("VD1_IF0_CHROMA_Y1=0x%x\n", op->rd(VD1_IF0_CHROMA_Y1));
	pr_info("VD1_IF0_REPEAT_LOOP=0x%x\n", op->rd(VD1_IF0_RPT_LOOP));
	pr_info("VD1_IF0_LUM0_RPT_PAT=0x%x\n", op->rd(VD1_IF0_LUMA0_RPT_PAT));
	pr_info("VD1_IF0_CHM0_RPT_PAT=0x%x\n", op->rd(VD1_IF0_CHROMA0_RPT_PAT));
	pr_info("VD1_IF0_LUMA_PSEL=0x%x\n", op->rd(VD1_IF0_LUMA_PSEL));
	pr_info("VD1_IF0_CHROMA_PSEL=0x%x\n", op->rd(VD1_IF0_CHROMA_PSEL));
	pr_info("VIU_VD1_FMT_CTRL=0x%x\n", op->rd(VIU_VD1_FMT_CTRL));
	pr_info("VIU_VD1_FMT_W=0x%x\n", op->rd(VIU_VD1_FMT_W));
#endif
	pr_info("DI_IF1_GEN_REG=0x%x\n", op->rd(DI_SC2_IF1_GEN_REG));
	pr_info("DI_IF1_GEN_REG2=0x%x\n", op->rd(DI_SC2_IF1_GEN_REG2));
	pr_info("DI_IF1_GEN_REG3=0x%x\n", op->rd(DI_SC2_IF1_GEN_REG3));
	pr_info("DI_IF1_CANVAS0=0x%x\n", op->rd(DI_SC2_IF1_CANVAS0));
	pr_info("DI_IF1_LUMA_X0=0x%x\n", op->rd(DI_SC2_IF1_LUMA_X0));
	pr_info("DI_IF1_LUMA_Y0=0x%x\n", op->rd(DI_SC2_IF1_LUMA_Y0));
	pr_info("DI_IF1_CHROMA_X0=0x%x\n", op->rd(DI_SC2_IF1_CHROMA_X0));
	pr_info("DI_IF1_CHROMA_Y0=0x%x\n", op->rd(DI_SC2_IF1_CHROMA_Y0));
	pr_info("DI_IF1_LMA0_RPT_PAT=0x%x\n", op->rd(DI_SC2_IF1_LUMA0_RPT_PAT));
	pr_info("DI_IF1_CHM0_RPT_PAT=0x%x\n", op->rd(DI_SC2_IF1_LUMA0_RPT_PAT));
	pr_info("DI_IF1_FMT_CTRL=0x%x\n", op->rd(DI_SC2_IF1_CFMT_CTRL));
	pr_info("DI_IF1_FMT_W=0x%x\n", op->rd(DI_SC2_IF1_CFMT_W));

	pr_info("DI_IF2_GEN_REG=0x%x\n", op->rd(DI_SC2_IF2_GEN_REG));
	pr_info("DI_IF2_GEN_REG2=0x%x\n", op->rd(DI_SC2_IF2_GEN_REG2));
	pr_info("DI_IF2_GEN_REG3=0x%x\n", op->rd(DI_SC2_IF2_GEN_REG3));
	pr_info("DI_IF2_CANVAS0=0x%x\n", op->rd(DI_SC2_IF2_CANVAS0));
	pr_info("DI_IF2_LUMA_X0=0x%x\n", op->rd(DI_SC2_IF2_LUMA_X0));
	pr_info("DI_IF2_LUMA_Y0=0x%x\n", op->rd(DI_SC2_IF2_LUMA_Y0));
	pr_info("DI_IF2_CHROMA_X0=0x%x\n", op->rd(DI_SC2_IF2_CHROMA_X0));
	pr_info("DI_IF2_CHROMA_Y0=0x%x\n", op->rd(DI_SC2_IF2_CHROMA_Y0));
	pr_info("DI_IF2_LUM0_RPT_PAT=0x%x\n", op->rd(DI_SC2_IF2_LUMA0_RPT_PAT));
	pr_info("DI_IF2_CHM0_RPT_PAT=0x%x\n", op->rd(DI_SC2_IF2_LUMA0_RPT_PAT));
	pr_info("DI_IF2_FMT_CTRL=0x%x\n", op->rd(DI_SC2_IF2_CFMT_CTRL));
	pr_info("DI_IF2_FMT_W=0x%x\n", op->rd(DI_SC2_IF2_CFMT_W));

	pr_info("DI_DIWR_Y=0x%x\n", op->rd(DI_SC2_DIWR_Y));
	pr_info("DI_DIWR_CTRL=0x%x", op->rd(DI_SC2_DIWR_CTRL));
	pr_info("DI_DIWR_X=0x%x.\n", op->rd(DI_SC2_DIWR_X));
}

/* this is for pre mif */
void config_di_mif_v3(struct DI_MIF_S *di_mif,
		      enum DI_MIF0_ID mif_index,
		      struct di_buf_s *di_buf,
		      unsigned int ch)
{
	struct di_pre_stru_s *ppre = get_pre_stru(ch);

	if (!di_buf)
		return;

	di_mif->canvas0_addr0 =
		di_buf->vframe->canvas0Addr & 0xff;
	di_mif->canvas0_addr1 =
		(di_buf->vframe->canvas0Addr >> 8) & 0xff;
	di_mif->canvas0_addr2 =
		(di_buf->vframe->canvas0Addr >> 16) & 0xff;

	if (dip_is_linear()) {//ary tmp, need add nv21
		//dbg_ic("%s:%d:linear\n", __func__, );
		di_mif->linear = 1;
		//di_mif->addr0 = di_buf->nr_adr;
		if (mif_index == DI_MIF0_ID_INP) {
			dbg_ic("%s:inp not change addr\n", __func__);
		} else {
			di_mif->addr0 = di_buf->nr_adr;
			di_mif->buf_crop_en = 1;
			//di_mif->buf_hsize = 1920; //tmp
			di_mif->buf_hsize = di_buf->buf_hsize;

			di_mif->block_mode = 0;
			dbg_ic("%s:addr:0x%lx,hsize[%d]\n", __func__,
				di_mif->addr0,
				di_mif->buf_hsize);
		}
	}
	//dbg_ic("%s:%d:linear:%d\n", __func__, mif_index, di_mif->linear);
	di_mif->nocompress = (di_buf->vframe->type & VIDTYPE_COMPRESS) ? 0 : 1;

	if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
		if (di_buf->vframe->type & VIDTYPE_VIU_444)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ?
			3 : 2;
		else if (di_buf->vframe->type & VIDTYPE_VIU_422)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ?
			3 : 1;
	} else {
		di_mif->bit_mode = 0;
	}
	if (di_buf->vframe->type & VIDTYPE_VIU_422) {
		/* from vdin or local vframe */
		if ((!IS_PROG(di_buf->vframe->type))	||
		    ppre->prog_proc_type) {
			di_mif->video_mode = 1;
			di_mif->set_separate_en = 0;
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0;
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 =
				di_buf->vframe->width - 1;
			di_mif->luma_y_start0 = 0;
			if (ppre->prog_proc_type)
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 1;
			else
				di_mif->luma_y_end0 =
					di_buf->vframe->height / 2 - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 = 0;
			di_mif->chroma_y_start0 = 0;
			di_mif->chroma_y_end0 = 0;
			di_mif->canvas0_addr0 =
				di_buf->vframe->canvas0Addr & 0xff;
			di_mif->canvas0_addr1 =
				(di_buf->vframe->canvas0Addr >> 8) & 0xff;
			di_mif->canvas0_addr2 =
				(di_buf->vframe->canvas0Addr >> 16) & 0xff;
		}
		di_mif->reg_swap = 1;
		di_mif->l_endian = 0;
		di_mif->cbcr_swap = 0;
	} else {
		if (di_buf->vframe->type & VIDTYPE_VIU_444)
			di_mif->video_mode = 2;
		else
			di_mif->video_mode = 0;
		if (di_buf->vframe->type &
		    (VIDTYPE_VIU_NV21 | VIDTYPE_VIU_NV12))
			di_mif->set_separate_en = 2;
		else
			di_mif->set_separate_en = 1;

		if (IS_PROG(di_buf->vframe->type) && ppre->prog_proc_type) {
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0; /* top */
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 =
				di_buf->vframe->width - 1;
			di_mif->luma_y_start0 = 0;
			di_mif->luma_y_end0 =
				di_buf->vframe->height - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 =
				di_buf->vframe->width / 2 - 1;
			di_mif->chroma_y_start0 = 0;
			di_mif->chroma_y_end0 =
				(di_buf->vframe->height + 1) / 2 - 1;
		} else if ((ppre->cur_inp_type & VIDTYPE_INTERLACE) &&
				(ppre->cur_inp_type & VIDTYPE_VIU_FIELD)) {
			di_mif->src_prog = 0;
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0; /* top */
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 =
				di_buf->vframe->width - 1;
			di_mif->luma_y_start0 = 0;
			di_mif->luma_y_end0 =
				di_buf->vframe->height / 2 - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 =
				di_buf->vframe->width / 2 - 1;
			di_mif->chroma_y_start0 = 0;
			di_mif->chroma_y_end0 =
				di_buf->vframe->height / 4 - 1;
		} else {
			/*move to mp	di_mif->src_prog = force_prog?1:0;*/
			if (ppre->cur_inp_type  & VIDTYPE_INTERLACE)
				di_mif->src_prog = 0;
			else
				di_mif->src_prog =
				dimp_get(edi_mp_force_prog) ? 1 : 0;
			di_mif->src_field_mode = 1;
			if ((di_buf->vframe->type & VIDTYPE_TYPEMASK) ==
			    VIDTYPE_INTERLACE_TOP) {
				di_mif->output_field_num = 0; /* top */
				di_mif->luma_x_start0 = 0;
				di_mif->luma_x_end0 =
					di_buf->vframe->width - 1;
				di_mif->luma_y_start0 = 0;
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 1;
				di_mif->chroma_x_start0 = 0;
				di_mif->chroma_x_end0 =
					di_buf->vframe->width / 2 - 1;
				di_mif->chroma_y_start0 = 0;
				di_mif->chroma_y_end0 =
					(di_buf->vframe->height + 1) / 2 - 1;
			} else {
				di_mif->output_field_num = 1;
				/* bottom */
				di_mif->luma_x_start0 = 0;
				di_mif->luma_x_end0 =
					di_buf->vframe->width - 1;
				di_mif->luma_y_start0 = 1;
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 1;
				di_mif->chroma_x_start0 = 0;
				di_mif->chroma_x_end0 =
					di_buf->vframe->width / 2 - 1;
				di_mif->chroma_y_start0 =
					(di_mif->src_prog ? 0 : 1);
				di_mif->chroma_y_end0 =
					(di_buf->vframe->height + 1) / 2 - 1;
			}
		}
	}
}

static void post_dbg_contr_v3(void)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	/* bit [11:10]:cntl_dbg_mode*/
	op->bwr(DI_SC2_IF0_GEN_REG3, 1, 11, 1);
	op->bwr(DI_SC2_IF1_GEN_REG3, 1, 11, 1);
	op->bwr(DI_SC2_IF2_GEN_REG3, 1, 11, 1);
}

static void di_post_set_flow_v3(unsigned int post_wr_en,
				enum EDI_POST_FLOW step)
{
	unsigned int val;
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	if (!post_wr_en)
		return;

	switch (step) {
	case EDI_POST_FLOW_STEP1_STOP:
		/*val = (0xc0200000 | line_num_post_frst);*/
		val = (0xc0000000 | 1);
		op->wr(DI_SC2_POST_GL_CTRL, val);
		break;
	case EDI_POST_FLOW_STEP2_START:
		/*val = (0x80200000 | line_num_post_frst);*/
		val = (0x80200000 | 1);
		op->wr(DI_SC2_POST_GL_CTRL, val);
		break;
	case EDI_POST_FLOW_STEP3_IRQ:
		op->wr(DI_SC2_POST_GL_CTRL, 0x1);
		/* DI_POST_CTRL
		 *	disable wr back avoid pps sreay in g12a
		 *	[7]: set 0;
		 */
		op->wr(DI_POST_CTRL, 0x80000001); /*ary sc2 ??*/
		op->wr(DI_SC2_POST_GL_CTRL, 0xc0000001);
		break;
	case EDI_POST_FLOW_STEP4_CP_START:
		op->wr(DI_POST_CTRL,
		(1 << 0)	| /* di_post_en      = post_ctrl[0]; */
		(0 << 1)	| /* di_blend_en     = post_ctrl[1]; */
		(0 << 2)	| /* di_ei_en        = post_ctrl[2]; */
		(0 << 3)	| /* di_mux_en       = post_ctrl[3]; */
		(1 << 4)	| /* di_wr_bk_en     = post_ctrl[4]; */
		(0 << 5)	| /* di_vpp_out_en   = post_ctrl[5]; */
		(0 << 6)	| /* reg_post_mb_en  = post_ctrl[6]; */
		(0 << 10)	| /* di_post_drop_1st= post_ctrl[10]; */
		(0 << 11)	| /* di_post_repeat  = post_ctrl[11]; */
		(0 << 29));	/* post_field_num  = post_ctrl[29]; */
		op->wr(DI_SC2_POST_GL_CTRL, 0x80200001);
		break;
	}
}

static void hpre_gl_sw_v3(bool on)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	if (on)
		op->wr(DI_SC2_PRE_GL_CTRL,
			    0x80200000 | dimp_get(edi_mp_line_num_pre_frst));
	else
		op->wr(DI_SC2_PRE_GL_CTRL, 0xc0000000);
}

void hpre_timeout_read(void)
{
	if (!DIM_IS_IC_EF(SC2))
		return;
	dim_print("gl:0x%x:0x%x\n", DI_SC2_PRE_GL_CTRL,
		  DIM_RDMA_RD(DI_SC2_PRE_GL_CTRL));
	dim_print("c:0x%x:0x%x\n", DI_RO_PRE_DBG, DIM_RDMA_RD(DI_RO_PRE_DBG));
	dim_print("c:0x%x:0x%x\n", DI_RO_PRE_DBG, DIM_RDMA_RD(DI_RO_PRE_DBG));
	dim_print("c:0x%x:0x%x\n", DI_RO_PRE_DBG, DIM_RDMA_RD(DI_RO_PRE_DBG));
	dim_print("c:0x%x:0x%x\n", DI_RO_PRE_DBG, DIM_RDMA_RD(DI_RO_PRE_DBG));
	dim_print("c:0x%x:0x%x\n", DI_RO_PRE_DBG, DIM_RDMA_RD(DI_RO_PRE_DBG));
	dim_print("c:0x%x:0x%x\n", DI_RO_PRE_DBG, DIM_RDMA_RD(DI_RO_PRE_DBG));
	dim_print("c:0x%x:0x%x\n", DI_RO_PRE_DBG, DIM_RDMA_RD(DI_RO_PRE_DBG));
	dim_print("c:0x%x:0x%x\n", DI_RO_PRE_DBG, DIM_RDMA_RD(DI_RO_PRE_DBG));
	dim_print("c:0x%x:0x%x\n", DI_RO_PRE_DBG, DIM_RDMA_RD(DI_RO_PRE_DBG));
}

static void hpre_gl_thd_v3(void)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	op->bwr(DI_SC2_PRE_GL_THD,
				 dimp_get(edi_mp_pre_hold_line), 16, 6);
}

static void hpost_gl_thd_v3(unsigned int hold_line)
{
	const struct reg_acc *op = &di_pre_regset;

	if (DIM_IS_IC_BF(SC2)) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	op->bwr(DI_SC2_POST_GL_THD, hold_line, 16, 5);
}

void dim_sc2_contr_pre(union hw_sc2_ctr_pre_s *cfg)
{
	const struct reg_acc *op = &di_pre_regset;
	unsigned int val;

	if (is_mask(SC2_REG_MSK_nr)) {
		PR_INF("%s:\n", __func__);
		op = &sc2reg;
	}
	val = op->rd(DI_TOP_PRE_CTRL);

	/*clear*/
	val &= ~((3	<< 0)	|
		(7	<< 4)	|
		(7	<< 7)	| /* bit[9:7] */
		(3	<< 30));

	val |= ((cfg->b.mif_en		<< 0)	|
		(cfg->b.afbc_nr_en	<< 1)	|
		(cfg->b.afbc_inp		<< 4)	|
		(cfg->b.afbc_chan2	<< 5)	|
		(cfg->b.afbc_mem		<< 6)	|
		//((cfg->b.is_4k ? 7 : 0)	<< 7)	|
		(cfg->b.is_inp_4k		<< 7)	|
		(cfg->b.is_chan2_4k		<< 8)	|
		(cfg->b.is_mem_4k		<< 9)	|
		(cfg->b.nr_ch0_en		<< 10)	|
		(cfg->b.pre_frm_sel	<< 30));

	dim_print("%s:%s:0x%x:0x%x\n", __func__,
		  "DI_TOP_PRE_CTRL",
		  DI_TOP_PRE_CTRL,
		  val);
	op->wr(DI_TOP_PRE_CTRL, val);
	if (DIM_IS_IC(T7)) {
		op->bwr(AFBCDM_INP_CTRL0, cfg->b.is_inp_4k,  14, 1);
		//reg_use_4kram
		op->bwr(AFBCDM_INP_CTRL0, cfg->b.afbc_inp, 13, 1);
		//reg_afbc_vd_sel //1:afbc_dec 0:nor_rdmif

		op->bwr(AFBCDM_CHAN2_CTRL0, cfg->b.is_chan2_4k, 14, 1);
		//reg_use_4kram
		op->bwr(AFBCDM_CHAN2_CTRL0, cfg->b.afbc_chan2, 13, 1);
		//reg_afbc_vd_sel //1:afbc_dec 0:nor_rdmif

		op->bwr(AFBCDM_MEM_CTRL0, cfg->b.is_mem_4k, 14, 1);
		//reg_use_4kram
		op->bwr(AFBCDM_MEM_CTRL0, cfg->b.afbc_mem, 13, 1);
		//reg_afbc_vd_sel //1:afbc_dec 0:nor_rdmif
	}
	dbg_ic("%s:afbc_mem[%d]\n", __func__, cfg->b.afbc_mem);
	//dbg_reg_mem(40);
}

/*
 * 0 off:
 * 1: pre afbce 4k
 * 2: post afbce 4k
 */
void dim_sc2_4k_set(unsigned int mode_4k)
{
	const struct reg_acc *op = &di_pre_regset;

	//dim_print("%s:mode[%d]\n", __func__);
	if (!mode_4k)
		op->wr(DI_TOP_CTRL1, 0x00000008); /*default*/
	else if (mode_4k == 1)
		op->wr(DI_TOP_CTRL1, 0x00000004); /*default*/
	else if (mode_4k == 2)
		op->wr(DI_TOP_CTRL1, 0x0000000c); /*default*/
}

void dim_sc2_afbce_rst(unsigned int ec_nub)
{
	const struct reg_acc *op = &di_pre_regset;

	//PR_INF("%s:[%d]\n", __func__, ec_nub);
	if (ec_nub == 0)  {
		//bit 25:
		op->bwr(DI_TOP_CTRL1, 1, 25, 1); /*default*/
		op->bwr(DI_TOP_CTRL1, 0, 25, 1); /*default*/
	} else {
		op->bwr(DI_TOP_CTRL1, 1, 23, 1); /*default*/
		op->bwr(DI_TOP_CTRL1, 0, 23, 1); /*default*/
	}
}

void dim_secure_pre_en(unsigned char ch)
{
	if (get_datal()->ch_data[ch].is_tvp == 2) {
		if (DIM_IS_IC_EF(SC2))
			DIM_DI_WR(DI_PRE_SEC_IN, 0x3F);//secure
		else
			tee_config_device_state(16, 1);
		get_datal()->ch_data[ch].is_secure_pre = 2;
		//dbg_mem2("%s:tvp3 pre SECURE:%d\n", __func__, ch);
	} else {
		if (DIM_IS_IC_EF(SC2))
			DIM_DI_WR(DI_PRE_SEC_IN, 0x0);
		else
			tee_config_device_state(16, 0);
		get_datal()->ch_data[ch].is_secure_pre = 1;
		//dbg_mem2("%s:tvp3 pre NOSECURE:%d\n", __func__, ch);
	}
}

void dim_secure_sw_pre(unsigned char ch)
{
	if (DIM_IS_IC_BF(G12A))
		return;
	dbg_mem2("%s:tvp3 pre:%d\n", __func__, ch);

	if (get_datal()->ch_data[ch].is_secure_pre == 0)//first set
		dim_secure_pre_en(ch);
	else if (get_datal()->ch_data[ch].is_tvp !=
		 get_datal()->ch_data[ch].is_secure_pre)
		dim_secure_pre_en(ch);
}

void dim_secure_pst_en(unsigned char ch)
{
	if (get_datal()->ch_data[ch].is_tvp == 2) {
		if (DIM_IS_IC_EF(SC2))
			DIM_DI_WR(DI_POST_SEC_IN, 0x1F);//secure
		else
			tee_config_device_state(17, 1);
		get_datal()->ch_data[ch].is_secure_pst = 2;
		//dbg_mem2("%s:tvp4 PST SECURE:%d\n", __func__, ch);
	} else {
		if (DIM_IS_IC_EF(SC2))
			DIM_DI_WR(DI_POST_SEC_IN, 0x0);
		else
			tee_config_device_state(17, 0);
		get_datal()->ch_data[ch].is_secure_pst = 1;
		//dbg_mem2("%s:tvp4 pST NOSECURE:%d\n", __func__, ch);
	}
}

void dim_secure_sw_post(unsigned char ch)
{
	if (DIM_IS_IC_BF(G12A))
		return;
	dbg_mem2("%s:tvp4 post:%d\n", __func__, ch);

	if (get_datal()->ch_data[ch].is_secure_pst == 0)//first set
		dim_secure_pst_en(ch);
	else if (get_datal()->ch_data[ch].is_tvp !=
		 get_datal()->ch_data[ch].is_secure_pst)
		dim_secure_pst_en(ch);
}

void dim_sc2_contr_pst(union hw_sc2_ctr_pst_s *cfg)
{
	const struct reg_acc *op = &di_pre_regset;
	unsigned int val;

	if (is_mask(SC2_REG_MSK_nr)) {
		PR_INF("%s:\n", __func__);
		op = &sc2reg;
	}

	val = op->rd(DI_TOP_POST_CTRL);

	/*clear*/
	val &= ~((3	<< 0)	|
		(7	<< 4)	|
		(7	<< 7)	| /* bit[9:7] */
		(3	<< 30));

	val |= ((cfg->b.mif_en		<< 0)	|
		(cfg->b.afbc_wr		<< 1)	|
		(cfg->b.afbc_if1		<< 4)	|
		(cfg->b.afbc_if0		<< 5)	|
		(cfg->b.afbc_if2		<< 6)	|
		//((cfg->b.is_4k ? 7 : 0)	<< 7)	|
		(cfg->b.is_if1_4k		<< 7)	|
		(cfg->b.is_if0_4k		<< 8)	|
		(cfg->b.is_if2_4k		<< 9)	|
		(cfg->b.post_frm_sel	<< 30));

	op->wr(DI_TOP_POST_CTRL, val);
	dim_print("%s:0x%x:0x%x\n",
		  "DI_TOP_POST_CTRL",
		  DI_TOP_POST_CTRL,
		  val);
	if (DIM_IS_IC(T7)) {
		op->bwr(AFBCDM_IF0_CTRL0, cfg->b.is_if0_4k, 14, 1);
		//reg_use_4kram
		op->bwr(AFBCDM_IF0_CTRL0, cfg->b.afbc_if0, 13, 1);
		//reg_afbc_vd_sel //1:afbc_dec 0:nor_rdmif

		op->bwr(AFBCDM_IF1_CTRL0, cfg->b.is_if1_4k, 14, 1);
		//reg_use_4kram
		op->bwr(AFBCDM_IF1_CTRL0, cfg->b.afbc_if1, 13, 1);
		//reg_afbc_vd_sel //1:afbc_dec 0:nor_rdmif

		op->bwr(AFBCDM_IF2_CTRL0, cfg->b.is_if2_4k, 14, 1);
		//reg_use_4kram
		op->bwr(AFBCDM_IF2_CTRL0, cfg->b.afbc_if2, 13, 1);
		//reg_afbc_vd_sel
		//1:afbc_dec 0:nor_rdmif
	}
}

int cnt_mm_info_simple_p(struct mm_size_out_s *info)
{
	struct mm_size_in_s	*in;
	struct mm_size_p_nv21	*pnv21;
	struct mm_size_p	*po;

	unsigned int tmpa, tmpb;
	unsigned int height;
	unsigned int width;
	unsigned int canvas_height;

	unsigned int nr_width;
	unsigned int canvas_align_width = 32;

	in = &info->info_in;

	height	= in->h;
	canvas_height = roundup(height, 32);
	width	= in->w;
	nr_width = width;

	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	if (in->mode == EDPST_MODE_422_10BIT_PACK)
		nr_width = (width * 5) / 4;
	else if (in->mode == EDPST_MODE_422_10BIT)
		nr_width = (width * 3) / 2;
	else
		nr_width = width;

	if (in->is_i || in->p_as_i) {
		//PR_ERR("%s:have i flg\n", __func__);
		return -1;
	}
	/* p */
	nr_width = roundup(nr_width, canvas_align_width);

	if (in->mode == EDPST_MODE_NV21_8BIT) {
		in->o_mode = 1;
		pnv21 = &info->nv21;
		tmpa = (nr_width * canvas_height) >> 1;/*uv*/
		tmpb = roundup(tmpa, PAGE_SIZE);
		tmpa = roundup(nr_width * canvas_height, PAGE_SIZE);

		pnv21->size_buf_uv	= tmpb;
		pnv21->size_buf		= tmpa;
		pnv21->off_y		= tmpa;
		pnv21->size_total = tmpa + tmpb;
		pnv21->size_page = pnv21->size_total >> PAGE_SHIFT;
		pnv21->cvs_w	= nr_width;
		pnv21->cvs_h	= canvas_height;
	} else {
		/* 422 */
		in->o_mode = 2;
		po = &info->p;
		tmpa = roundup(nr_width * canvas_height * 2, PAGE_SIZE);
		po->size_buf = tmpa;

		po->size_total = po->size_buf;
		po->size_page = po->size_total >> PAGE_SHIFT;
		po->cvs_w	= nr_width << 1;
		po->cvs_h	= canvas_height;
	}

	return 0;
}

/*****************************************************
 * double write test
 *****************************************************/

static struct dw_s dw_d;
static struct dw_s *pdw;

static const struct mm_size_in_s cdw_info = {
	.w	= 960,
	.h	= 540,
	.p_as_i	= 0,
	.is_i	= 0,
	.en_afbce	= 0,
	.mode	= EDPST_MODE_422_10BIT_PACK,
};

static const struct SHRK_S cdw_sk = {
	.hsize_in	= 960,
	.vsize_in	= 540,
	.h_shrk_mode	= 0,
	.v_shrk_mode	= 0,
	.shrk_en	= 1,
	.frm_rst	= 0,
};

void dw_int(void)
{
	pdw = &dw_d;

	memcpy(&pdw->size_info.info_in, &cdw_info,
	       sizeof(pdw->size_info.info_in));
	cnt_mm_info_simple_p(&pdw->size_info);

	memcpy(&pdw->shrk_cfg, &cdw_sk, sizeof(pdw->shrk_cfg));

	pdw->shrk_cfg.h_shrk_mode = dw_get_h();
	pdw->shrk_cfg.v_shrk_mode = dw_get_h();
}

struct dw_s *dim_getdw(void)
{
	return &dw_d;
}

void dw_fill_outvf(struct vframe_s *vfm,
		   struct di_buf_s *di_buf)
{
	struct canvas_config_s *cvsp;
//	unsigned int cvsh, cvsv, csize;

	memcpy(vfm, di_buf->vframe, sizeof(*vfm));

	/* canvas */
	vfm->canvas0Addr = (u32)-1;

	vfm->plane_num = 1;
	cvsp = &vfm->canvas0_config[0];
	cvsp->phy_addr = di_buf->dw_adr;
	cvsp->block_mode = 0;
	cvsp->endian = 0;
	cvsp->width = pdw->size_info.p.cvs_w;
	cvsp->height = pdw->size_info.p.cvs_h;

	pdw->shrk_cfg.hsize_in = vfm->width;
	pdw->shrk_cfg.vsize_in = vfm->height;

	vfm->height = vfm->height >> (pdw->shrk_cfg.v_shrk_mode + 1);
	vfm->width = vfm->width >> (pdw->shrk_cfg.h_shrk_mode + 1);
#ifdef NV21_DBG
	if (cfg_vf)
		vfm->type = cfg_vf;
#endif
}

const struct dim_hw_opsv_s dim_ops_l1_v3 = {
	.info = {
		.name = "l1_sc2",
		.update = "2020-06-01",
		.main_version	= 3,
		.sub_version	= 1,
	},
	.pre_mif_set = set_di_mif_v3,
	.pst_mif_set = set_di_mif_v3,
	.pst_mif_update_csv	= pst_mif_update_canvasid_v3,
	.pre_mif_sw	= di_pre_data_mif_ctrl_v3,
	.pst_mif_sw	= post_mif_sw_v3,
	.pst_mif_rst	= post_mif_rst_v3,
	.pst_mif_rev	= post_mif_rev_v3,
	.pst_dbg_contr	= post_dbg_contr_v3,
	.pst_set_flow	= di_post_set_flow_v3,
	.pst_bit_mode_cfg	= post_bit_mode_cfg_v3,
	.wr_cfg_mif	= wr_mif_cfg_v3,
	.wrmif_set	= set_wrmif_simple_v3,
	.wrmif_sw_buf	= NULL,
	.wrmif_trig	= NULL,
	.wr_rst_protect	= NULL,
	.hw_init	= hw_init_v3,
	.pre_hold_block_txlx = NULL,
	.pre_cfg_mif	= config_di_mif_v3,
	.dbg_reg_pre_mif_print	= dbg_reg_pre_mif_print_v3,
	.dbg_reg_pst_mif_print	= dbg_reg_pst_mif_print_v3,
	.dbg_reg_pre_mif_print2	= dbg_reg_pre_mif_print2_v3,
	.dbg_reg_pst_mif_print2	= dbg_reg_pst_mif_print2_v3,
	.dbg_reg_pre_mif_show	= dbg_reg_pre_mif_v3_show,
	.dbg_reg_pst_mif_show	= dbg_reg_pst_mif_v3_show,
	/*contrl*/
	.pre_gl_sw		= hpre_gl_sw_v3,
	.pre_gl_thd		= hpre_gl_thd_v3,
	.pst_gl_thd	= hpost_gl_thd_v3,
	.reg_mif_tab	= {
		[DI_MIF0_ID_INP] = &mif_contr_reg[DI_MIF0_ID_INP][0],
		[DI_MIF0_ID_CHAN2] = &mif_contr_reg[DI_MIF0_ID_CHAN2][0],
		[DI_MIF0_ID_MEM] = &mif_contr_reg[DI_MIF0_ID_MEM][0],
		[DI_MIF0_ID_IF1] = &mif_contr_reg[DI_MIF0_ID_IF1][0],
		[DI_MIF0_ID_IF0] = &mif_contr_reg[DI_MIF0_ID_IF0][0],
		[DI_MIF0_ID_IF2] = &mif_contr_reg[DI_MIF0_ID_IF2][0],
	},
};

const struct dim_hw_opsv_s dim_ops_l1_v4 = { //for t7
	.info = {
		.name = "l1_t7",
		.update = "2020-12-28",
		.main_version	= 4,
		.sub_version	= 1,
	},
	.pre_mif_set = set_di_mif_v3,
	.pst_mif_set = set_di_mif_v3,
	.pst_mif_update_csv	= pst_mif_update_canvasid_v3,
	.pre_mif_sw	= di_pre_data_mif_ctrl_v3,
	.pst_mif_sw	= post_mif_sw_v3,
	.pst_mif_rst	= post_mif_rst_v3,
	.pst_mif_rev	= post_mif_rev_v3,
	.pst_dbg_contr	= post_dbg_contr_v3,
	.pst_set_flow	= di_post_set_flow_v3,
	.pst_bit_mode_cfg	= post_bit_mode_cfg_v3,
	.wr_cfg_mif	= wr_mif_cfg_v3,
	.wrmif_set	= set_wrmif_simple_v3,
	.wrmif_sw_buf	= NULL,
	.pre_ma_mif_set	= set_ma_pre_mif_t7,
	.post_mtnrd_mif_set = set_post_mtnrd_mif_t7,
	.pre_enable_mc	= pre_enable_mc_t7,
	.wrmif_trig	= NULL,
	.wr_rst_protect	= NULL,
	.hw_init	= hw_init_v3,
	.pre_hold_block_txlx = NULL,
	.pre_cfg_mif	= config_di_mif_v3,
	.dbg_reg_pre_mif_print	= dbg_reg_pre_mif_print_v3,
	.dbg_reg_pst_mif_print	= dbg_reg_pst_mif_print_v3,
	.dbg_reg_pre_mif_print2	= dbg_reg_pre_mif_print2_v3,
	.dbg_reg_pst_mif_print2	= dbg_reg_pst_mif_print2_v3,
	.dbg_reg_pre_mif_show	= dbg_reg_pre_mif_v3_show,
	.dbg_reg_pst_mif_show	= dbg_reg_pst_mif_v3_show,
	/*contrl*/
	.pre_gl_sw		= hpre_gl_sw_v3,
	.pre_gl_thd		= hpre_gl_thd_v3,
	.pst_gl_thd	= hpost_gl_thd_v3,
	.reg_mif_tab	= {
		[DI_MIF0_ID_INP] = &mif_contr_reg[DI_MIF0_ID_INP][0],
		[DI_MIF0_ID_CHAN2] = &mif_contr_reg[DI_MIF0_ID_CHAN2][0],
		[DI_MIF0_ID_MEM] = &mif_contr_reg[DI_MIF0_ID_MEM][0],
		[DI_MIF0_ID_IF1] = &mif_contr_reg[DI_MIF0_ID_IF1][0],
		[DI_MIF0_ID_IF0] = &mif_contr_reg[DI_MIF0_ID_IF0][0],
		[DI_MIF0_ID_IF2] = &mif_contr_reg[DI_MIF0_ID_IF2][0],
	},
	.reg_mif_wr_tab	= {
		[EDI_MIFSM_NR] = &reg_wrmif_v3[EDI_MIFSM_NR][0],
		[EDI_MIFSM_WR] = &reg_wrmif_v3[EDI_MIFSM_WR][0],
	},
	.reg_mif_wr_bits_tab = &reg_bits_wr[0],
	.rtab_contr_bits_tab = &rtab_sc2_contr_bits_tab[0],
};

static const struct hw_ops_s dim_hw_v3_ops = {
	.info	= {
		.name	= "sc2",
		.update	= "2020-06-01",
		.version_main	= 3,
		.version_sub	= 1,
	},
	.afbcd_set	= set_afbcd_mult_simple,
	.afbce_set	= set_afbce_cfg_v1,
	.shrk_set	= set_shrk,
	.shrk_disable	= set_shrk_disable,
	.wrmif_set	= set_wrmif_simple,
	.mult_wr	= set_di_mult_write,
	.pre_set	= set_di_pre,
	.post_set	= set_di_post,
	.prepost_link	= enable_prepost_link,
	.prepost_link_afbc	= enable_prepost_link_afbce,
	.memcpy_rot	= set_di_memcpy_rot,
	.memcpy		= set_di_memcpy,
};

bool di_attach_ops_v3(const struct hw_ops_s **ops)
{
	*ops = &dim_hw_v3_ops;
	return true;
}

