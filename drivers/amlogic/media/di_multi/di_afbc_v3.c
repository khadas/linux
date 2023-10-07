// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/deinterlace/sc2/di_afbc_v3.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/of_fdt.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/of_device.h>

#include <linux/amlogic/media/vfm/vframe.h>

/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "deinterlace_dbg.h"
#include "deinterlace.h"
#include "di_data_l.h"

#include "register.h"
#include "nr_downscale.h"
#include "deinterlace_hw.h"
#include "register_nr4.h"
#include "di_reg_v3.h"
#include "di_prc.h"

#include "di_afbc_v3.h"
#include "di_hw_v3.h"
#include "di_dbg.h"

//#include "di_pqa.h"
#include "../deinterlace/di_pqa.h"

/*di multi*/
#include "di_sys.h"
#include <linux/amlogic/media/vpu/vpu.h>

#define DI_BIT0		0x00000001
#define DI_BIT1		0x00000002
#define DI_BIT2		0x00000004
#define DI_BIT3		0x00000008
#define DI_BIT4		0x00000010
#define DI_BIT5		0x00000020
#define DI_BIT6		0x00000040
#define DI_BIT7		0x00000080

#define di_print	dim_print
#define di_vmap		dim_vmap
#define di_unmap_phyaddr	dim_unmap_phyaddr
//#define DBG_AFBCD_SET		(1)

struct enc_cfg_s {
	int enable;
	int loosy_mode;
	/* loosy_mode:
	 * 0:close 1:luma loosy 2:chrma loosy 3: luma & chrma loosy
	 */
	ulong head_baddr;/*head addr*/
	ulong mmu_info_baddr;/*mmu info linear addr*/
	int reg_format_mode;/*0:444 1:422 2:420*/
	int reg_compbits_y;/*bits num after compression*/
	int reg_compbits_c;/*bits num after compression*/

	int hsize_in;/*input hsize*/
	int vsize_in;/*input hsize*/
	int enc_win_bgn_h;/*input scope*/
	int enc_win_end_h;/*input scope*/
	int enc_win_bgn_v;/*input scope*/
	int enc_win_end_v;/*input scope*/

	/*from sc2*/
	u32 reg_init_ctrl;//pip init frame flag
	u32 reg_pip_mode;//pip open bit
	u32 reg_ram_comb;//ram split bit open in di mult write case
	u32 hsize_bgnd;//hsize of background
	u32 vsize_bgnd;//hsize of background
	u32 rev_mode;//0:normal mode
	u32 def_color_0;//def_color
	u32 def_color_1;//def_color
	u32 def_color_2;//def_color
	u32 def_color_3;//def_color
	u32 force_444_comb;
	u32 rot_en;
	u32 din_swt;
};

static void afbce_sw(enum EAFBC_ENC enc, bool on, const struct reg_acc *op);//tmp
static void ori_afbce_cfg(struct enc_cfg_s *cfg,
			  const struct reg_acc *op,
			  enum EAFBC_ENC enc);

static unsigned int reg_rd(unsigned int addr)
{
	return aml_read_vcbus(addr);
}

static unsigned int reg_rdb(unsigned int adr, unsigned int start,
			    unsigned int len)
{
	return rd_reg_bits(adr, start, len);
}

static void reg_wr(unsigned int addr, unsigned int val)
{
	WR(addr, val);
}

static unsigned int reg_wrb(unsigned int adr, unsigned int val,
			    unsigned int start, unsigned int len)
{
	wr_reg_bits(adr, val, start, len);
	return 1;
}

static const struct reg_acc di_normal_regset = {
	.wr = reg_wr,
	.rd = reg_rd,
	.bwr = reg_wrb,
	.brd = reg_rdb,
};

/*tmp*/
/*static struct afd_s di_afd;*/
#define BITS_EAFBC_CFG_DISABLE	   DI_BIT0
#define BITS_EAFBC_CFG_INP_AFBC	   DI_BIT1
#define BITS_EAFBC_CFG_EMODE       DI_BIT2
#define BITS_EAFBC_CFG_ETEST       DI_BIT3
#define BITS_EAFBC_CFG_4K          DI_BIT4
#define BITS_EAFBC_CFG_PRE_LINK    DI_BIT5
#define BITS_EAFBC_CFG_PAUSE       DI_BIT6
#define BITS_EAFBC_CFG_LEVE3       DI_BIT7

#define BITS_EAFBC_CFG_6DEC_2ENC   DI_BIT8
#define BITS_EAFBC_CFG_6DEC_2ENC2  DI_BIT9
#define BITS_EAFBC_CFG_6DEC_1ENC3  DI_BIT10
#define BITS_EAFBC_CFG_FORCE_MEM      DI_BIT15
#define BITS_EAFBC_CFG_FORCE_CHAN2     DI_BIT14
#define BITS_EAFBC_CFG_FORCE_NR       DI_BIT13
#define BITS_EAFBC_CFG_FORCE_IF1       DI_BIT12

#define BITS_EAFBC_CFG_MEM	   DI_BIT31

/************************************
 * bit[0]: on/off
 * bit[1]: p -> need proce p use 2 i buf
 * bit[2]: mode
 ************************************/
static u32 afbc_cfg;// =  BITS_EAFBC_CFG_4K;
/************************************
 * base function:
 *	0x3:	afbcd x  1
 *	0x07:	afbcd x 2 + afbce x 1
 *	0x101:
 ************************************/
module_param_named(afbc_cfg, afbc_cfg, uint, 0664);

#ifdef DBG_AFBC
static u32 afbc_cfg_vf;
module_param_named(afbc_cfg_vf, afbc_cfg_vf, uint, 0664);
static u32 afbc_cfg_bt;
module_param_named(afbc_cfg_bt, afbc_cfg_bt, uint, 0664);
#endif
static struct afbcd_ctr_s *di_get_afd_ctr(void);

/************************************************
 * [0]: enable afbc
 * [1]: p use 2 ibuf mode
 * [2]: 2afbcd + 1afbce mode ([1] must 1)
 * [3]: 2afbcd + 1afbce test mode (mem use inp vf)
 * [4]: 4k test mode; i need bypass;
 * [5]: pre_link mode
 * [6]: stop when first frame display
 * [7]: change level always 3
 * [8]: 6afbcd + 2afbce
 * [31]: afbce enable (get memory)
 ***********************************************/

static bool is_cfg(enum EAFBC_CFG cfg_cmd)
{
	bool ret;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (pafd_ctr->fb.ver < AFBCD_V4) {
		if (cfg_cmd > EAFBC_CFG_INP_AFBC)
			return false;

	} else if (pafd_ctr->fb.ver == AFBCD_V4) {
		if (cfg_cmd > EAFBC_CFG_MEM)
			return false;
	}

	ret = false;
	switch (cfg_cmd) {
	case EAFBC_CFG_DISABLE:
		if (afbc_cfg & BITS_EAFBC_CFG_DISABLE)
			ret = true;
		break;
	case EAFBC_CFG_INP_AFBC:
		if (afbc_cfg & BITS_EAFBC_CFG_INP_AFBC)
			ret = true;
		break;
	case EAFBC_CFG_ETEST2:
		if (afbc_cfg & BITS_EAFBC_CFG_EMODE)
			ret = true;
		break;
	case EAFBC_CFG_ETEST:
		if (afbc_cfg & BITS_EAFBC_CFG_ETEST)
			ret = true;
		break;
	case EAFBC_CFG_4K:
		if (afbc_cfg & BITS_EAFBC_CFG_4K)
			ret = true;
		break;
	case EAFBC_CFG_PRE_LINK:
		if (afbc_cfg & BITS_EAFBC_CFG_PRE_LINK)
			ret = true;
		break;
	case EAFBC_CFG_PAUSE:
		if (afbc_cfg & BITS_EAFBC_CFG_PAUSE)
			ret = true;
		break;
	case EAFBC_CFG_LEVE3:
		if (afbc_cfg & BITS_EAFBC_CFG_LEVE3)
			ret = true;
		break;
	case EAFBC_CFG_FORCE_MEM:
		if (afbc_cfg & BITS_EAFBC_CFG_FORCE_MEM)
			ret = true;
		break;
	case EAFBC_CFG_FORCE_CHAN2:
		if (afbc_cfg & BITS_EAFBC_CFG_FORCE_CHAN2)
			ret = true;
		break;
	case EAFBC_CFG_FORCE_NR:
		if (afbc_cfg & BITS_EAFBC_CFG_FORCE_NR)
			ret = true;
		break;
	case EAFBC_CFG_FORCE_IF1:
		if (afbc_cfg & BITS_EAFBC_CFG_FORCE_IF1)
			ret = true;
		break;
	case EAFBC_CFG_6DEC_2ENC:
		if (afbc_cfg & BITS_EAFBC_CFG_6DEC_2ENC)
			ret = true;
		break;
	case EAFBC_CFG_6DEC_2ENC2:
		if (afbc_cfg & BITS_EAFBC_CFG_6DEC_2ENC2)
			ret = true;
		break;
	case EAFBC_CFG_6DEC_1ENC3:
		if (afbc_cfg & BITS_EAFBC_CFG_6DEC_1ENC3)
			ret = true;
		break;
	case EAFBC_CFG_MEM:
		if (afbc_cfg & (BITS_EAFBC_CFG_MEM	|
				BITS_EAFBC_CFG_EMODE		|
				BITS_EAFBC_CFG_6DEC_2ENC		|
				BITS_EAFBC_CFG_6DEC_2ENC2		|
				BITS_EAFBC_CFG_6DEC_1ENC3))
			ret = true;
		break;
	}
	return ret;
}

static bool is_status(enum EAFBC_STS status)
{
	bool ret = false;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	switch (status) {
	case EAFBC_MEM_NEED:
		ret = pafd_ctr->fb.mem_alloc;
		break;
	case EAFBC_MEMI_NEED:
		ret = pafd_ctr->fb.mem_alloci;
		break;
	case EAFBC_EN_6CH:
		if (pafd_ctr->fb.mode >= AFBC_WK_6D)
			ret = true;
		break;
	case EAFBC_EN_ENC:
		if (pafd_ctr->fb.mode >= AFBC_WK_P)
			ret = true;
		break;
	}

	return ret;
}

//static bool prelink_status;

bool dbg_di_prelink_v3(void)
{
	if (is_cfg(EAFBC_CFG_PRE_LINK))
		return true;

	return false;
}
EXPORT_SYMBOL(dbg_di_prelink_v3);

#ifdef HIS_CODE
void dbg_di_prelink_reg_check_v3(void)
{
	unsigned int val;

	if (DIM_IS_IC_BF(TM2B)) {
		dim_print("%s:check return;\n", __func__);
		return;
	}

	if (!prelink_status && is_cfg(EAFBC_CFG_PRE_LINK)) {
		/* set on*/
		if (DIM_IS_IC_EF(SC2)) {
			/* ? */
		} else {
			reg_wrb(DI_AFBCE_CTRL, 1, 3, 1);
			reg_wr(VD1_AFBCD0_MISC_CTRL, 0x100100);
		}
		prelink_status = true;
	} else if (prelink_status && !is_cfg(EAFBC_CFG_PRE_LINK)) {
		/* set off */
		if (DIM_IS_IC_EF(SC2))
			di_pr_info("%s\n", __func__);
		else
			reg_wrb(DI_AFBCE_CTRL, 0, 3, 1);

		prelink_status = false;
	}

	if (!is_cfg(EAFBC_CFG_PRE_LINK))
		return;

	val = reg_rd(VD1_AFBCD0_MISC_CTRL);
	if (val != 0x100100) {
		di_pr_info("%s:change 0x%x\n", __func__, val);
		reg_wr(VD1_AFBCD0_MISC_CTRL, 0x100100);
	}
}
#endif

static struct afd_s	*di_afdp;

static struct afbcd_ctr_s *di_get_afd_ctr(void)
{
	//return &di_afd.ctr;
	return &di_afdp->ctr;
}

static struct afd_s *get_afdp(void)
{
	return di_afdp;
}

static bool is_src_i(struct vframe_s *vf)
{
	bool ret = false;
	struct di_buf_s *di_buf;

	if (!vf->private_data)
		return false;

	di_buf = (struct di_buf_s *)vf->private_data;
	if (di_buf->afbc_info & DI_BIT0)
		ret = true;

	return ret;
}

static bool is_src_real_i(struct vframe_s *vf)
{
	bool ret = false;
	struct di_buf_s *di_buf;

	if (!vf->private_data)
		return false;

	di_buf = (struct di_buf_s *)vf->private_data;
	if (di_buf->afbc_info & DI_BIT1)
		ret = true;

	return ret;
}

static bool src_i_set(struct vframe_s *vf)
{
	bool ret = false;
	struct di_buf_s *di_buf;

	if (!vf->private_data) {
#ifdef PRINT_BASIC
		PR_ERR("%s:novf\n", __func__);
#endif
		return false;
	}

	di_buf = (struct di_buf_s *)vf->private_data;
	di_buf->afbc_info |= DI_BIT0;

	ret = true;

	return ret;
}

static const unsigned int reg_afbc[AFBC_DEC_NUB][AFBC_REG_INDEX_NUB] = {
	{
		AFBC_ENABLE,
		AFBC_MODE,
		AFBC_SIZE_IN,
		AFBC_DEC_DEF_COLOR,
		AFBC_CONV_CTRL,
		AFBC_LBUF_DEPTH,
		AFBC_HEAD_BADDR,
		AFBC_BODY_BADDR,
		AFBC_SIZE_OUT,
		AFBC_OUT_YSCOPE,
		AFBC_STAT,
		AFBC_VD_CFMT_CTRL,
		AFBC_VD_CFMT_W,
		AFBC_MIF_HOR_SCOPE,
		AFBC_MIF_VER_SCOPE,
		AFBC_PIXEL_HOR_SCOPE,
		AFBC_PIXEL_VER_SCOPE,
		AFBC_VD_CFMT_H,
	},
	{
		VD2_AFBC_ENABLE,
		VD2_AFBC_MODE,
		VD2_AFBC_SIZE_IN,
		VD2_AFBC_DEC_DEF_COLOR,
		VD2_AFBC_CONV_CTRL,
		VD2_AFBC_LBUF_DEPTH,
		VD2_AFBC_HEAD_BADDR,
		VD2_AFBC_BODY_BADDR,
		VD2_AFBC_OUT_XSCOPE,
		VD2_AFBC_OUT_YSCOPE,
		VD2_AFBC_STAT,
		VD2_AFBC_VD_CFMT_CTRL,
		VD2_AFBC_VD_CFMT_W,
		VD2_AFBC_MIF_HOR_SCOPE,
		VD2_AFBC_MIF_VER_SCOPE,
		VD2_AFBC_PIXEL_HOR_SCOPE,
		VD2_AFBC_PIXEL_VER_SCOPE,
		VD2_AFBC_VD_CFMT_H,

	},
	{
		DI_INP_AFBC_ENABLE,
		DI_INP_AFBC_MODE,
		DI_INP_AFBC_SIZE_IN,
		DI_INP_AFBC_DEC_DEF_COLOR,
		DI_INP_AFBC_CONV_CTRL,
		DI_INP_AFBC_LBUF_DEPTH,
		DI_INP_AFBC_HEAD_BADDR,
		DI_INP_AFBC_BODY_BADDR,
		DI_INP_AFBC_SIZE_OUT,
		DI_INP_AFBC_OUT_YSCOPE,
		DI_INP_AFBC_STAT,
		DI_INP_AFBC_VD_CFMT_CTRL,
		DI_INP_AFBC_VD_CFMT_W,
		DI_INP_AFBC_MIF_HOR_SCOPE,
		DI_INP_AFBC_MIF_VER_SCOPE,
		DI_INP_AFBC_PIXEL_HOR_SCOPE,
		DI_INP_AFBC_PIXEL_VER_SCOPE,
		DI_INP_AFBC_VD_CFMT_H,

	},
	{
		DI_MEM_AFBC_ENABLE,
		DI_MEM_AFBC_MODE,
		DI_MEM_AFBC_SIZE_IN,
		DI_MEM_AFBC_DEC_DEF_COLOR,
		DI_MEM_AFBC_CONV_CTRL,
		DI_MEM_AFBC_LBUF_DEPTH,
		DI_MEM_AFBC_HEAD_BADDR,
		DI_MEM_AFBC_BODY_BADDR,
		DI_MEM_AFBC_SIZE_OUT,
		DI_MEM_AFBC_OUT_YSCOPE,
		DI_MEM_AFBC_STAT,
		DI_MEM_AFBC_VD_CFMT_CTRL,
		DI_MEM_AFBC_VD_CFMT_W,
		DI_MEM_AFBC_MIF_HOR_SCOPE,
		DI_MEM_AFBC_MIF_VER_SCOPE,
		DI_MEM_AFBC_PIXEL_HOR_SCOPE,
		DI_MEM_AFBC_PIXEL_VER_SCOPE,
		DI_MEM_AFBC_VD_CFMT_H,
	},

};

static const unsigned int reg_afbc_v5[AFBC_DEC_NUB_V5][AFBC_REG_INDEX_NUB] = {
	[EAFBC_DEC0] = {
		AFBCDM_VD1_ENABLE,
		AFBCDM_VD1_MODE,
		AFBCDM_VD1_SIZE_IN,
		AFBCDM_VD1_DEC_DEF_COLOR,
		AFBCDM_VD1_CONV_CTRL,
		AFBCDM_VD1_LBUF_DEPTH,
		AFBCDM_VD1_HEAD_BADDR,
		AFBCDM_VD1_BODY_BADDR,
		AFBCDM_VD1_SIZE_OUT,
		AFBCDM_VD1_OUT_YSCOPE,
		AFBCDM_VD1_STAT,
		AFBCDM_VD1_VD_CFMT_CTRL,
		AFBCDM_VD1_VD_CFMT_W,
		AFBCDM_VD1_MIF_HOR_SCOPE,
		AFBCDM_VD1_MIF_VER_SCOPE,
		AFBCDM_VD1_PIXEL_HOR_SCOPE,
		AFBCDM_VD1_PIXEL_VER_SCOPE,
		AFBCDM_VD1_VD_CFMT_H,
	},
	[EAFBC_DEC1] = {
		AFBCDM_VD1_ENABLE,
		AFBCDM_VD1_MODE,
		AFBCDM_VD1_SIZE_IN,
		AFBCDM_VD1_DEC_DEF_COLOR,
		AFBCDM_VD1_CONV_CTRL,
		AFBCDM_VD1_LBUF_DEPTH,
		AFBCDM_VD1_HEAD_BADDR,
		AFBCDM_VD1_BODY_BADDR,
		AFBCDM_VD1_SIZE_OUT,
		AFBCDM_VD1_OUT_YSCOPE,
		AFBCDM_VD1_STAT,
		AFBCDM_VD1_VD_CFMT_CTRL,
		AFBCDM_VD1_VD_CFMT_W,
		AFBCDM_VD1_MIF_HOR_SCOPE,
		AFBCDM_VD1_MIF_VER_SCOPE,
		AFBCDM_VD1_PIXEL_HOR_SCOPE,
		AFBCDM_VD1_PIXEL_VER_SCOPE,
		AFBCDM_VD1_VD_CFMT_H,

	},
	[EAFBC_DEC2_DI] = {
		AFBCDM_INP_ENABLE,
		AFBCDM_INP_MODE,
		AFBCDM_INP_SIZE_IN,
		AFBCDM_INP_DEC_DEF_COLOR,
		AFBCDM_INP_CONV_CTRL,
		AFBCDM_INP_LBUF_DEPTH,
		AFBCDM_INP_HEAD_BADDR,
		AFBCDM_INP_BODY_BADDR,
		AFBCDM_INP_SIZE_OUT,
		AFBCDM_INP_OUT_YSCOPE,
		AFBCDM_INP_STAT,
		AFBCDM_INP_VD_CFMT_CTRL,
		AFBCDM_INP_VD_CFMT_W,
		AFBCDM_INP_MIF_HOR_SCOPE,
		AFBCDM_INP_MIF_VER_SCOPE,
		AFBCDM_INP_PIXEL_HOR_SCOPE,
		AFBCDM_INP_PIXEL_VER_SCOPE,
		AFBCDM_INP_VD_CFMT_H,

	},
	[EAFBC_DEC_CHAN2] = {
		AFBCDM_CHAN2_ENABLE,
		AFBCDM_CHAN2_MODE,
		AFBCDM_CHAN2_SIZE_IN,
		AFBCDM_CHAN2_DEC_DEF_COLOR,
		AFBCDM_CHAN2_CONV_CTRL,
		AFBCDM_CHAN2_LBUF_DEPTH,
		AFBCDM_CHAN2_HEAD_BADDR,
		AFBCDM_CHAN2_BODY_BADDR,
		AFBCDM_CHAN2_SIZE_OUT,
		AFBCDM_CHAN2_OUT_YSCOPE,
		AFBCDM_CHAN2_STAT,
		AFBCDM_CHAN2_VD_CFMT_CTRL,
		AFBCDM_CHAN2_VD_CFMT_W,
		AFBCDM_CHAN2_MIF_HOR_SCOPE,
		AFBCDM_CHAN2_MIF_VER_SCOPE,
		AFBCDM_CHAN2_PIXEL_HOR_SCOPE,
		AFBCDM_CHAN2_PIXEL_VER_SCOPE,
		AFBCDM_CHAN2_VD_CFMT_H,
	},
	[EAFBC_DEC3_MEM] = {
		AFBCDM_MEM_ENABLE,
		AFBCDM_MEM_MODE,
		AFBCDM_MEM_SIZE_IN,
		AFBCDM_MEM_DEC_DEF_COLOR,
		AFBCDM_MEM_CONV_CTRL,
		AFBCDM_MEM_LBUF_DEPTH,
		AFBCDM_MEM_HEAD_BADDR,
		AFBCDM_MEM_BODY_BADDR,
		AFBCDM_MEM_SIZE_OUT,
		AFBCDM_MEM_OUT_YSCOPE,
		AFBCDM_MEM_STAT,
		AFBCDM_MEM_VD_CFMT_CTRL,
		AFBCDM_MEM_VD_CFMT_W,
		AFBCDM_MEM_MIF_HOR_SCOPE,
		AFBCDM_MEM_MIF_VER_SCOPE,
		AFBCDM_MEM_PIXEL_HOR_SCOPE,
		AFBCDM_MEM_PIXEL_VER_SCOPE,
		AFBCDM_MEM_VD_CFMT_H,
	},
	[EAFBC_DEC_IF1] = {
		AFBCDM_IF1_ENABLE,
		AFBCDM_IF1_MODE,
		AFBCDM_IF1_SIZE_IN,
		AFBCDM_IF1_DEC_DEF_COLOR,
		AFBCDM_IF1_CONV_CTRL,
		AFBCDM_IF1_LBUF_DEPTH,
		AFBCDM_IF1_HEAD_BADDR,
		AFBCDM_IF1_BODY_BADDR,
		AFBCDM_IF1_SIZE_OUT,
		AFBCDM_IF1_OUT_YSCOPE,
		AFBCDM_IF1_STAT,
		AFBCDM_IF1_VD_CFMT_CTRL,
		AFBCDM_IF1_VD_CFMT_W,
		AFBCDM_IF1_MIF_HOR_SCOPE,
		AFBCDM_IF1_MIF_VER_SCOPE,
		AFBCDM_IF1_PIXEL_HOR_SCOPE,
		AFBCDM_IF1_PIXEL_VER_SCOPE,
		AFBCDM_IF1_VD_CFMT_H,
	},
	[EAFBC_DEC_IF0] = {
		AFBCDM_IF0_ENABLE,
		AFBCDM_IF0_MODE,
		AFBCDM_IF0_SIZE_IN,
		AFBCDM_IF0_DEC_DEF_COLOR,
		AFBCDM_IF0_CONV_CTRL,
		AFBCDM_IF0_LBUF_DEPTH,
		AFBCDM_IF0_HEAD_BADDR,
		AFBCDM_IF0_BODY_BADDR,
		AFBCDM_IF0_SIZE_OUT,
		AFBCDM_IF0_OUT_YSCOPE,
		AFBCDM_IF0_STAT,
		AFBCDM_IF0_VD_CFMT_CTRL,
		AFBCDM_IF0_VD_CFMT_W,
		AFBCDM_IF0_MIF_HOR_SCOPE,
		AFBCDM_IF0_MIF_VER_SCOPE,
		AFBCDM_IF0_PIXEL_HOR_SCOPE,
		AFBCDM_IF0_PIXEL_VER_SCOPE,
		AFBCDM_IF0_VD_CFMT_H,
	},
	[EAFBC_DEC_IF2] = {
		AFBCDM_IF2_ENABLE,
		AFBCDM_IF2_MODE,
		AFBCDM_IF2_SIZE_IN,
		AFBCDM_IF2_DEC_DEF_COLOR,
		AFBCDM_IF2_CONV_CTRL,
		AFBCDM_IF2_LBUF_DEPTH,
		AFBCDM_IF2_HEAD_BADDR,
		AFBCDM_IF2_BODY_BADDR,
		AFBCDM_IF2_SIZE_OUT,
		AFBCDM_IF2_OUT_YSCOPE,
		AFBCDM_IF2_STAT,
		AFBCDM_IF2_VD_CFMT_CTRL,
		AFBCDM_IF2_VD_CFMT_W,
		AFBCDM_IF2_MIF_HOR_SCOPE,
		AFBCDM_IF2_MIF_VER_SCOPE,
		AFBCDM_IF2_PIXEL_HOR_SCOPE,
		AFBCDM_IF2_PIXEL_VER_SCOPE,
		AFBCDM_IF2_VD_CFMT_H,
	},

};

/*keep order with struct afbce_bits_s*/
static const unsigned int reg_afbc_e[AFBC_ENC_NUB_V5][DIM_AFBCE_NUB] = {
	{
		DI_AFBCE_ENABLE,		/*  0 */
		DI_AFBCE_MODE,			/*  1 */
		DI_AFBCE_SIZE_IN,		/*  2 */
		DI_AFBCE_BLK_SIZE_IN,		/*  3 */
		DI_AFBCE_HEAD_BADDR,		/*  4 */

		DI_AFBCE_MIF_SIZE,		/*  5 */
		DI_AFBCE_PIXEL_IN_HOR_SCOPE,	/*  6 */
		DI_AFBCE_PIXEL_IN_VER_SCOPE,	/*  7 */
		DI_AFBCE_CONV_CTRL,		/*  8 */
		DI_AFBCE_MIF_HOR_SCOPE,		/*  9 */
		DI_AFBCE_MIF_VER_SCOPE,		/* 10 */
		/**/
		DI_AFBCE_FORMAT,		/* 11 */
		/**/
		DI_AFBCE_DEFCOLOR_1,		/* 12 */
		DI_AFBCE_DEFCOLOR_2,		/* 13 */
		DI_AFBCE_QUANT_ENABLE,		/* 14 */
		//unsigned int mmu_num,
		DI_AFBCE_MMU_RMIF_CTRL1,	/* 15 */
		DI_AFBCE_MMU_RMIF_CTRL2,	/* 16 */
		DI_AFBCE_MMU_RMIF_CTRL3,	/* 17 */
		DI_AFBCE_MMU_RMIF_CTRL4,	/* 18 */
		DI_AFBCE_MMU_RMIF_SCOPE_X,	/* 19 */
		DI_AFBCE_MMU_RMIF_SCOPE_Y,	/* 20 */

		/**********************/
		DI_AFBCE_MODE_EN,
		DI_AFBCE_DWSCALAR,
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
		DI_AFBCE_STAT1,		/*read only*/
		DI_AFBCE_STAT2,
		DI_AFBCE_MMU_RMIF_RO_STAT,
		/*add for sc2*/
		DI_AFBCE_PIP_CTRL,
		DI_AFBCE_ROT_CTRL,
		DI_AFBCE_DIMM_CTRL,
		DI_AFBCE_BND_DEC_MISC,
		DI_AFBCE_RD_ARB_MISC,
	},
	{
		DI_AFBCE1_ENABLE,		/*  0 */
		DI_AFBCE1_MODE,			/*  1 */
		DI_AFBCE1_SIZE_IN,		/*  2 */
		DI_AFBCE1_BLK_SIZE_IN,		/*  3 */
		DI_AFBCE1_HEAD_BADDR,		/*  4 */

		DI_AFBCE1_MIF_SIZE,		/*  5 */
		DI_AFBCE1_PIXEL_IN_HOR_SCOPE,	/*  6 */
		DI_AFBCE1_PIXEL_IN_VER_SCOPE,	/*  7 */
		DI_AFBCE1_CONV_CTRL,		/*  8 */
		DI_AFBCE1_MIF_HOR_SCOPE,		/*  9 */
		DI_AFBCE1_MIF_VER_SCOPE,		/* 10 */
		/**/
		DI_AFBCE1_FORMAT,		/* 11 */
		/**/
		DI_AFBCE1_DEFCOLOR_1,		/* 12 */
		DI_AFBCE1_DEFCOLOR_2,		/* 13 */
		DI_AFBCE1_QUANT_ENABLE,		/* 14 */
		//unsigned int mmu_num,
		DI_AFBCE1_MMU_RMIF_CTRL1,	/* 15 */
		DI_AFBCE1_MMU_RMIF_CTRL2,	/* 16 */
		DI_AFBCE1_MMU_RMIF_CTRL3,	/* 17 */
		DI_AFBCE1_MMU_RMIF_CTRL4,	/* 18 */
		DI_AFBCE1_MMU_RMIF_SCOPE_X,	/* 19 */
		DI_AFBCE1_MMU_RMIF_SCOPE_Y,	/* 20 */

		/**********************/
		DI_AFBCE1_MODE_EN,
		DI_AFBCE1_DWSCALAR,
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
		DI_AFBCE1_STAT1,		/*read only*/
		DI_AFBCE1_STAT2,
		DI_AFBCE1_MMU_RMIF_RO_STAT,
		/*from sc2*/
		DI_AFBCE1_PIP_CTRL,
		DI_AFBCE1_ROT_CTRL,
		DI_AFBCE1_DIMM_CTRL,
		DI_AFBCE1_BND_DEC_MISC,
		DI_AFBCE1_RD_ARB_MISC,
	},
};

static const unsigned int *afbc_get_addrp(enum EAFBC_DEC eidx)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (pafd_ctr->fb.ver < AFBCD_V5)
		return &reg_afbc[eidx][0];

	return &reg_afbc_v5[eidx][0];
}

static void afbcd_reg_bwr(enum EAFBC_DEC eidx, enum EAFBC_REG adr_idx,
			  unsigned int val,
			  unsigned int start, unsigned int len)
{
	const unsigned int *reg;// = afbc_get_regbase();
//	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	unsigned int reg_addr;

	reg = afbc_get_addrp(eidx);
	reg_addr = reg[adr_idx];

	//di_print("%s:reg[0x%x] sw[%d]\n", __func__, reg_AFBC_ENABLE, on);

	reg_wrb(reg_addr, val, start, len);
}

static const unsigned int *afbce_get_addrp(enum EAFBC_ENC eidx)
{
	return &reg_afbc_e[eidx][0];
}

static void dump_afbcd_reg(void)
{
	u32 i;
	u32 afbc_reg;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	pr_info("---- dump afbc EAFBC_DEC0 reg -----\n");
	for (i = 0; i < AFBC_REG_INDEX_NUB; i++) {
		if (pafd_ctr->fb.ver < AFBCD_V5)
			afbc_reg = reg_afbc[EAFBC_DEC0][i];
		else
			afbc_reg = reg_afbc_v5[EAFBC_DEC0][i];
		pr_info("reg 0x%x val:0x%x\n", afbc_reg, reg_rd(afbc_reg));
	}
	pr_info("---- dump afbc EAFBC_DEC1 reg -----\n");
	for (i = 0; i < AFBC_REG_INDEX_NUB; i++) {
		if (pafd_ctr->fb.ver < AFBCD_V5)
			afbc_reg = reg_afbc[EAFBC_DEC1][i];
		else
			afbc_reg = reg_afbc_v5[EAFBC_DEC1][i];
		pr_info("reg 0x%x val:0x%x\n", afbc_reg, reg_rd(afbc_reg));
	}
	pr_info("reg 0x%x val:0x%x\n",
		VD1_AFBCD0_MISC_CTRL, reg_rd(VD1_AFBCD0_MISC_CTRL));
	pr_info("reg 0x%x val:0x%x\n",
		VD2_AFBCD1_MISC_CTRL, reg_rd(VD2_AFBCD1_MISC_CTRL));
}

static void dbg_afbc_blk(struct seq_file *s, union afbc_blk_s *blk, char *name)
{
	seq_printf(s, "%10s:[%d]\n", name, blk->d8);
	seq_printf(s, "\t%5s:[%d],\n", "inp", blk->b.inp);
	seq_printf(s, "\t%5s:[%d],\n", "mem", blk->b.mem);
	seq_printf(s, "\t%5s:[%d],\n", "chan2", blk->b.chan2);
	seq_printf(s, "\t%5s:[%d],\n", "if0", blk->b.if0);
	seq_printf(s, "\t%5s:[%d],\n", "if1", blk->b.if1);
	seq_printf(s, "\t%5s:[%d],\n", "if2", blk->b.if2);
	seq_printf(s, "\t%5s:[%d],\n", "enc_nr", blk->b.enc_nr);
	seq_printf(s, "\t%5s:[%d],\n", "enc_wr", blk->b.enc_wr);
}

static const char *afbc_get_version(void);

int dbg_afbc_cfg_v3_show(struct seq_file *s, void *v)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!pafd_ctr)
		return 0;

	seq_printf(s, "%10s:%d:%s\n",
		   "version", pafd_ctr->fb.ver,
		   afbc_get_version());

	dbg_afbc_blk(s, &pafd_ctr->fb.sp, "support");

	seq_printf(s, "%10s:%d\n", "mode", pafd_ctr->fb.mode);
	seq_printf(s, "%10s:inp[%d],mem[%d]\n", "dec",
		   pafd_ctr->fb.pre_dec,
		   pafd_ctr->fb.mem_dec);

	seq_printf(s, "%10s:%d\n", "int_flg", pafd_ctr->b.int_flg);
	seq_printf(s, "%10s:%d\n", "en", pafd_ctr->b.en);
	seq_printf(s, "%10s:%d\n", "level", pafd_ctr->b.chg_level);

	dbg_afbc_blk(s, &pafd_ctr->en_cfg, "cfg");
	dbg_afbc_blk(s, &pafd_ctr->en_sgn, "sgn");
	dbg_afbc_blk(s, &pafd_ctr->en_set, "set");
	return 0;
}
EXPORT_SYMBOL(dbg_afbc_cfg_v3_show);

static u32 di_request_afbc(bool onoff)
{
	u32 afbc_busy;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!pafd_ctr || pafd_ctr->fb.ver >= AFBCD_V4) {
		afbc_busy = 0;
		return afbc_busy;
	}

	if (onoff)
		afbc_busy = di_request_afbc_hw(pafd_ctr->fb.pre_dec, true);
	else
		afbc_busy = di_request_afbc_hw(pafd_ctr->fb.pre_dec, false);

	di_pr_info("%s:busy:%d\n", __func__, afbc_busy);
	return afbc_busy;
}

static u32 dbg_request_afbc(bool onoff)
{
	u32 afbc_busy;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!pafd_ctr)
		return 0;
	if (onoff)
		afbc_busy = di_request_afbc_hw(pafd_ctr->fb.pre_dec, true);
	else
		afbc_busy = di_request_afbc_hw(pafd_ctr->fb.pre_dec, false);

	return afbc_busy;
}

const unsigned int *afbc_get_inp_base(void)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!pafd_ctr)
		return NULL;
	if (pafd_ctr->fb.ver < AFBCD_V5)
		return &reg_afbc[pafd_ctr->fb.pre_dec][0];

	return &reg_afbc_v5[pafd_ctr->fb.pre_dec][0];
}

static bool afbc_is_supported(void)
{
	bool ret = false;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!pafd_ctr || is_cfg(EAFBC_CFG_DISABLE)) {
#ifdef PRINT_BASIC
		dim_print("%s:false\n", __func__);
#endif
		return false;
	}
	if (pafd_ctr->fb.ver != AFBCD_NONE)
		ret = true;

	return ret;
}

static bool afbc_is_supported_for_plink(void)
{
	bool ret = false;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!pafd_ctr)
		return false;
	if (is_cfg(EAFBC_CFG_DISABLE) && !pafd_ctr->en_ponly_afbcd)
		return false;

	if (pafd_ctr->fb.ver != AFBCD_NONE)
		ret = true;

	return ret;
}

static const char *afbc_ver_name[6] = {
	"_none",
	"_gxl",
	"_g12a",
	"_tl1",
	"_tm2_vb",
	"_sc2",
};

static const char *afbc_get_version(void)
{
	unsigned int version;

	version = di_get_afd_ctr()->fb.ver;

	if (version >= ARRAY_SIZE(afbc_ver_name))
		return "overflow";

	return afbc_ver_name[version];
}

static const struct afbc_fb_s cafbc_v5_sc2 = {
	.ver = AFBCD_V5,
	.sp.b.inp		= 1,
	.sp.b.mem		= 1,
	.sp.b.chan2		= 1,
	.sp.b.if0		= 1,
	.sp.b.if1		= 1,
	.sp.b.if2		= 1,
	.sp.b.enc_nr		= 1,
	.sp.b.enc_wr		= 1,
	.pre_dec = EAFBC_DEC2_DI,
	.mem_dec = EAFBC_DEC3_MEM,
	.ch2_dec = EAFBC_DEC_CHAN2,
	.if0_dec = EAFBC_DEC_IF0,
	.if1_dec = EAFBC_DEC_IF1,
	.if2_dec = EAFBC_DEC_IF2,
};

static const struct afbc_fb_s cafbc_v4_tm2 = {
	.ver = AFBCD_V4,
	.sp.b.inp		= 1,
	.sp.b.mem		= 1,
	.sp.b.chan2		= 0,
	.sp.b.if0		= 0,
	.sp.b.if1		= 0,
	.sp.b.if2		= 0,
	.sp.b.enc_nr		= 1,
	.sp.b.enc_wr		= 0,
	.pre_dec = EAFBC_DEC2_DI,
	.mem_dec = EAFBC_DEC3_MEM,
	.ch2_dec = 0,
	.if0_dec = 0,
	.if1_dec = 0,
	.if2_dec = 0,
};

/* t5d vb afbcd as tm2 */
static const struct afbc_fb_s cafbc_v4_t5dvb = {
	.ver = AFBCD_V4,
	.sp.b.inp		= 1,
	.sp.b.mem		= 0,
	.sp.b.chan2		= 0,
	.sp.b.if0		= 0,
	.sp.b.if1		= 0,
	.sp.b.if2		= 0,
	.sp.b.enc_nr		= 0,
	.sp.b.enc_wr		= 0,
	.pre_dec = EAFBC_DEC0,
	.mem_dec = 0,
	.ch2_dec = 0,
	.if0_dec = 0,
	.if1_dec = 0,
	.if2_dec = 0
};

static const struct afbc_fb_s cafbc_v3_tl1 = {
	.ver = AFBCD_V3,
	.sp.b.inp		= 1,
	.sp.b.mem		= 0,
	.sp.b.chan2		= 0,
	.sp.b.if0		= 0,
	.sp.b.if1		= 0,
	.sp.b.if2		= 0,
	.sp.b.enc_nr		= 1,
	.sp.b.enc_wr		= 0,
	.pre_dec = EAFBC_DEC2_DI,
	.mem_dec = 0,
	.ch2_dec = 0,
	.if0_dec = 0,
	.if1_dec = 0,
	.if2_dec = 0
};

static const union afbc_blk_s cafbc_cfg_mode[] = {
	[AFBC_WK_NONE] = {
		.b.inp		= 0,
		.b.mem		= 0,
		.b.chan2		= 0,
		.b.if0		= 0,
		.b.if1		= 0,
		.b.if2		= 0,
		.b.enc_nr		= 0,
		.b.enc_wr		= 0,
	},
	[AFBC_WK_IN] = {
		.b.inp		= 1,
		.b.mem		= 0,
		.b.chan2		= 0,
		.b.if0		= 0,
		.b.if1		= 0,
		.b.if2		= 0,
		.b.enc_nr		= 0,
		.b.enc_wr		= 0,
	},
	[AFBC_WK_P] = {
		.b.inp		= 1,
		.b.mem		= 1,
		.b.chan2		= 0,
		.b.if0		= 0,
		.b.if1		= 0,
		.b.if2		= 0,
		.b.enc_nr		= 1,
		.b.enc_wr		= 0,
	},
	[AFBC_WK_6D] = {
		.b.inp		= 1,
		.b.mem		= 1,
		.b.chan2		= 1,
		.b.if0		= 1,
		.b.if1		= 1,
		.b.if2		= 1,
		.b.enc_nr		= 1,
		.b.enc_wr		= 1,
	},
	[AFBC_WK_6D_ALL] = {
		.b.inp		= 1,
		.b.mem		= 1,
		.b.chan2		= 1,
		.b.if0		= 1,
		.b.if1		= 1,
		.b.if2		= 1,
		.b.enc_nr		= 1,
		.b.enc_wr		= 1,
	},
	[AFBC_WK_6D_NV21] = {
		.b.inp		= 1,
		.b.mem		= 1,
		.b.chan2		= 1,
		.b.if0		= 1,
		.b.if1		= 1,
		.b.if2		= 1,
		.b.enc_nr		= 1,
		.b.enc_wr		= 0,
	}
};

static const union afbc_blk_s cafbc_cfg_sgn[] = {
	[AFBC_SGN_BYPSS] = {
		.b.inp		= 0,
		.b.mem		= 0,
		.b.chan2		= 0,
		.b.if0		= 0,
		.b.if1		= 0,
		.b.if2		= 0,
		.b.enc_nr		= 0,
		.b.enc_wr		= 0,
	},
	[AFBC_SGN_P_H265] = {
		.b.inp		= 1,
		.b.mem		= 1,
		.b.chan2		= 0,
		.b.if0		= 0,
		.b.if1		= 0,
		.b.if2		= 0,
		.b.enc_nr		= 1,
		.b.enc_wr		= 0,
	},
	[AFBC_SGN_P] = {
		.b.inp		= 0,
		.b.mem		= 1,
		.b.chan2		= 0,
		.b.if0		= 0,
		.b.if1		= 0,
		.b.if2		= 0,
		.b.enc_nr		= 1,
		.b.enc_wr		= 0,
	},
	[AFBC_SGN_P_SINP] = {
		.b.inp		= 0,
		.b.mem		= 0,
		.b.chan2		= 0,
		.b.if0		= 0,
		.b.if1		= 0,
		.b.if2		= 0,
		.b.enc_nr		= 1,
		.b.enc_wr		= 0,
	},
	[AFBC_SGN_P_H265_AS_P] = {
		.b.inp		= 1,
		.b.mem		= 0,
		.b.chan2		= 0,
		.b.if0		= 0,
		.b.if1		= 0,
		.b.if2		= 0,
		.b.enc_nr		= 0,
		.b.enc_wr		= 0,
	},
	[AFBC_SGN_H265_SINP] = {
		.b.inp		= 1,
		.b.mem		= 1,
		.b.chan2		= 0,
		.b.if0		= 0,
		.b.if1		= 0,
		.b.if2		= 0,
		.b.enc_nr		= 0,
		.b.enc_wr		= 0,
	},
	[AFBC_SGN_I] = {
		.b.inp		= 0,
		.b.mem		= 1,
		.b.chan2		= 1,
		.b.if0		= 1,
		.b.if1		= 1,
		.b.if2		= 1,
		.b.enc_nr		= 1,
		.b.enc_wr		= 1,
	},
	[AFBC_SGN_I_H265] = {
		.b.inp		= 1,
		.b.mem		= 1,
		.b.chan2		= 0,
		.b.if0		= 1,
		.b.if1		= 1,
		.b.if2		= 1,
		.b.enc_nr		= 1,
		.b.enc_wr		= 1,
	},
	[AFBC_SGN_I_H265_SINP] = {
		.b.inp		= 1,
		.b.mem		= 0,
		.b.chan2		= 0,
		.b.if0		= 0,
		.b.if1		= 0,
		.b.if2		= 0,
		.b.enc_nr		= 0,
		.b.enc_wr		= 0,
	}
};

#ifdef HIS_CODE
static void afbc_sgn_mode_set(unsigned int sgn_mode)
{
	struct afbcd_ctr_s *pafd_ctr;

	pafd_ctr = &di_afdp->ctr;

	if (sgn_mode <= AFBC_SGN_I_H265_SINP) {
		memcpy(&pafd_ctr->en_sgn,
		       &cafbc_cfg_sgn[sgn_mode], sizeof(pafd_ctr->en_sgn));
	} else {
#ifdef PRINT_BASIC
		PR_ERR("%s:overflow %d\n", __func__, sgn_mode);
#endif
		pafd_ctr->en_sgn.d8 = 0;
	}
}
#else
static void afbc_sgn_mode_set(unsigned char *sgn_mode, enum EAFBC_SNG_SET cmd)
{
	union afbc_blk_s *en_cfg;

	en_cfg  = (union afbc_blk_s *)sgn_mode;
	switch (cmd) {
	case EAFBC_SNG_CLR_NR:
		en_cfg->b.enc_nr = 0;
		break;
	case EAFBC_SNG_CLR_WR:
		en_cfg->b.enc_wr = 0;
		break;
	case EAFBC_SNG_SET_NR:
		en_cfg->b.enc_nr = 1;
		break;
	case EAFBC_SNG_SET_WR:
		en_cfg->b.enc_wr = 1;
		break;
	};
}

#endif
static unsigned char afbc_cnt_sgn_mode(unsigned int sgn)
{
//	struct afbcd_ctr_s *pafd_ctr;
	unsigned char sgn_mode;

	if (sgn <= AFBC_SGN_I_H265_SINP) {
		//memcpy(&sgn_mode, &cafbc_cfg_sgn[sgn], sizeof(sgn_mode));
		sgn_mode = cafbc_cfg_sgn[sgn].d8;
	} else {
		//PR_ERR("%s:overflow %d\n", __func__, sgn);
		sgn_mode = 0;
	}

	return sgn_mode;
}

static void afbc_cfg_mode_set(unsigned int mode, union afbc_blk_s *en_cfg)
{
	if (!en_cfg)
		return;

	if (is_cfg(EAFBC_CFG_DISABLE)) {
		en_cfg->d8 = 0;
		return;
	}

	en_cfg->d8 = 0;
	if (mode <= AFBC_WK_6D_NV21)
		en_cfg->d8 = cafbc_cfg_mode[mode].d8;

	//dim_print("%s:0x%x\n", __func__, en_cfg->d8);
}

static void afbc_get_mode_from_cfg(void)
{
	struct afbcd_ctr_s *pafd_ctr;

	pafd_ctr = &di_afdp->ctr;
	/*******************************
	 * cfg for debug:
	 ******************************/
	if (!afbc_cfg)
		return;
	if (is_cfg(EAFBC_CFG_DISABLE)) {
		pafd_ctr->fb.mode = 0;
	} else if (afbc_cfg & (BITS_EAFBC_CFG_INP_AFBC |
			BITS_EAFBC_CFG_6DEC_2ENC |
			BITS_EAFBC_CFG_6DEC_2ENC2 |
			BITS_EAFBC_CFG_6DEC_1ENC3)) {
		if (is_cfg(EAFBC_CFG_INP_AFBC))
			pafd_ctr->fb.mode = AFBC_WK_IN;
		else if (is_cfg(EAFBC_CFG_6DEC_2ENC))
			pafd_ctr->fb.mode = AFBC_WK_6D;
		else if (is_cfg(EAFBC_CFG_6DEC_2ENC2))
			pafd_ctr->fb.mode = AFBC_WK_6D_ALL;
		else if (is_cfg(EAFBC_CFG_6DEC_1ENC3))
			pafd_ctr->fb.mode = AFBC_WK_6D_NV21;
	} else {
		if (pafd_ctr->fb.ver >= AFBCD_V5)
			pafd_ctr->fb.mode = AFBC_WK_6D_NV21;
		else if (pafd_ctr->fb.ver >= AFBCD_V4)
			pafd_ctr->fb.mode = AFBC_WK_P;
		else if (pafd_ctr->fb.ver >= AFBCD_V1)
			pafd_ctr->fb.mode = AFBC_WK_IN;
		else
			pafd_ctr->fb.mode = AFBC_WK_NONE;
	}
	dim_print("%s:mode[%d]:cfg[0x%x]:\n",
		  __func__,
		  pafd_ctr->fb.mode, afbc_cfg);
}

static void afbc_prob(unsigned int cid, struct afd_s *p)
{
	struct afbcd_ctr_s *pafd_ctr;// = di_get_afd_ctr();

	afbc_cfg = BITS_EAFBC_CFG_DISABLE;
	di_afdp = p;

	if (!di_afdp) {
		pr_error("%s:no data\n", __func__);
		return;
	}
	pafd_ctr = &di_afdp->ctr;
	pafd_ctr->en_ponly_afbcd = false;

	if (IS_IC_EF(cid, SC2)) {
		//afbc_cfg = BITS_EAFBC_CFG_4K;
		afbc_cfg = 0;
		memcpy(&pafd_ctr->fb, &cafbc_v5_sc2, sizeof(pafd_ctr->fb));
		if (DIM_IS_IC(S4))
			pafd_ctr->fb.mode = AFBC_WK_IN;//JUST 1afbc D
		else
			pafd_ctr->fb.mode = AFBC_WK_P;
		//AFBC_WK_6D_ALL;//AFBC_WK_IN;
	} else if (IS_IC(cid, T5DB)) {
		if (cfgg(T5DB_AFBCD_EN))
			afbc_cfg = 0;
		else
			afbc_cfg = BITS_EAFBC_CFG_DISABLE;
		if (afbc_cfg && IS_IC_SUPPORT(PRE_VPP_LINK) &&
		    cfgg(EN_PRE_LINK))
			pafd_ctr->en_ponly_afbcd = true;

		memcpy(&pafd_ctr->fb, &cafbc_v4_t5dvb, sizeof(pafd_ctr->fb));
		pafd_ctr->fb.mode = AFBC_WK_IN;
	} else if (IS_IC_EF(cid, T5D)) { //unsupport afbc
		pafd_ctr->fb.ver = AFBCD_NONE;
		pafd_ctr->fb.sp.b.inp = 0;
		pafd_ctr->fb.sp.b.mem = 0;
		pafd_ctr->fb.pre_dec = EAFBC_DEC0;
		pafd_ctr->fb.mode = AFBC_WK_NONE;
	} else if (IS_IC_EF(cid, T5)) { //afbc config same with tm2b
		afbc_cfg = 0;
		memcpy(&pafd_ctr->fb, &cafbc_v4_tm2, sizeof(pafd_ctr->fb));
		pafd_ctr->fb.mode = AFBC_WK_P;
	} else if (IS_IC_EF(cid, TM2B)) {
		afbc_cfg = 0;
		memcpy(&pafd_ctr->fb, &cafbc_v4_tm2, sizeof(pafd_ctr->fb));
		pafd_ctr->fb.mode = AFBC_WK_P;
	} else if (IS_IC_EF(cid, TL1)) {
		memcpy(&pafd_ctr->fb, &cafbc_v3_tl1, sizeof(pafd_ctr->fb));
		pafd_ctr->fb.mode = AFBC_WK_IN;
	} else if (IS_IC_EF(cid, G12A)) {
		pafd_ctr->fb.ver = AFBCD_V2;
		pafd_ctr->fb.sp.b.inp = 1;
		pafd_ctr->fb.sp.b.mem = 0;
		pafd_ctr->fb.pre_dec = EAFBC_DEC1;
		pafd_ctr->fb.mode = AFBC_WK_IN;
	} else if (IS_IC_EF(cid, GXL)) {
		pafd_ctr->fb.ver = AFBCD_V1;
		pafd_ctr->fb.sp.b.inp = 1;
		pafd_ctr->fb.sp.b.mem = 0;
		pafd_ctr->fb.pre_dec = EAFBC_DEC0;
		pafd_ctr->fb.mode = AFBC_WK_IN;
	} else {
		pafd_ctr->fb.ver = AFBCD_NONE;
		pafd_ctr->fb.sp.b.inp = 0;
		pafd_ctr->fb.sp.b.mem = 0;
		pafd_ctr->fb.pre_dec = EAFBC_DEC0;
		pafd_ctr->fb.mode = AFBC_WK_NONE;
	}
	/*******************************
	 * cfg for debug:
	 ******************************/
	afbc_get_mode_from_cfg();

	//afbc_cfg_mode_set(pafd_ctr->fb.mode);
	/******************************/
	if (pafd_ctr->fb.mode >= AFBC_WK_6D) {
		pafd_ctr->fb.mem_alloc	= 1;
		pafd_ctr->fb.mem_alloci	= 1;
	} else if (pafd_ctr->fb.mode >= AFBC_WK_P) {
		pafd_ctr->fb.mem_alloc = 1;
		pafd_ctr->fb.mem_alloci	= 0;
	} else if (is_cfg(EAFBC_CFG_MEM)) {
		pafd_ctr->fb.mem_alloc = 1;
		pafd_ctr->fb.mem_alloci	= 1;
	} else {
		pafd_ctr->fb.mem_alloc	= 0;
		pafd_ctr->fb.mem_alloci	= 0;
	}

	//afbc_cfg = BITS_EAFBC_CFG_DISABLE;
	dbg_mem("%s:ver[%d],%s,mode[%d]\n", __func__, pafd_ctr->fb.ver,
	       afbc_get_version(), pafd_ctr->fb.mode);
	PR_INF("%s:ok:en_ponly_afbcd[%d]\n", __func__,
		pafd_ctr->en_ponly_afbcd);
}

/*
 * after g12a, framereset will not reset simple
 * wr mif of pre such as mtn&cont&mv&mcinfo wr
 */

static void afbc_reg_sw(bool on);

//unsigned int test_afbc_en;
static void afbc_sw(bool on);
#define AFBCP	(1)

static void afbc_pre_check(struct vframe_s *vf, void *a)/*pch*/
{
	union hw_sc2_ctr_pre_s *cfg;
	struct afbcd_ctr_s *pctr;
	union afbc_blk_s *pblk;
	struct di_buf_s *di_buf;
	union afbc_blk_s	*en_set_pre;
	union afbc_blk_s	*en_cfg_pre;
	struct di_ch_s *pch;

	if (!vf ||
	    !get_afdp()	||
	    !a)
		return;

	pch = (struct di_ch_s *)a;

	di_buf = (struct di_buf_s *)vf->private_data;
	if (!di_buf)
		return;

//	cfg = di_afdp->top_cfg_pre;

	pctr = di_get_afd_ctr();

	pctr->en_sgn.d8 = di_buf->afbc_sgn_cfg;

	en_cfg_pre = &pch->rse_ori.di_pre_stru.en_cfg;
	en_set_pre = &pch->rse_ori.di_pre_stru.en_set;
	en_set_pre->d8 = en_cfg_pre->d8 & pctr->en_sgn.d8;
	pctr->en_set.d8 = en_set_pre->d8;
	pctr->en_cfg.d8 = en_cfg_pre->d8;

	cfg = &pch->rse_ori.di_pre_stru.pre_top_cfg;

	pblk = &pctr->en_set;
	if (pblk->b.inp) {
		cfg->b.afbc_inp = 1;
		if (di_buf->is_4k)
			cfg->b.is_inp_4k = 1;
		else
			cfg->b.is_inp_4k = 0;
	} else {
		cfg->b.afbc_inp = 0;
		cfg->b.is_inp_4k = 0;
	}

	if (pblk->b.mem) {
		cfg->b.afbc_mem = 1;

		if (di_buf->is_4k)
			cfg->b.is_mem_4k = 1;
		else
			cfg->b.is_mem_4k = 0;
	} else {
		cfg->b.afbc_mem = 0;
		cfg->b.is_mem_4k = 0;
	}

	if (pblk->b.chan2) {
		cfg->b.afbc_chan2 = 1;

		if (di_buf->is_4k)
			cfg->b.is_chan2_4k = 1;
		else
			cfg->b.is_chan2_4k = 0;
	} else {
		cfg->b.afbc_chan2 = 0;
		cfg->b.is_chan2_4k = 0;
	}
	cfg->b.mode_4k = 0;
	if (pblk->b.enc_nr) {
		cfg->b.afbc_nr_en	= 1;
		if (di_buf->dw_have)
			cfg->b.mif_en	= 1;
		else
			cfg->b.mif_en	= 0;
		if (di_buf->is_4k)
			cfg->b.mode_4k = 1;
	} else {
		cfg->b.afbc_nr_en	= 0;
		cfg->b.mif_en		= 1;
	}

	//dim_print("%s:\n", __func__);
}

static void afbc_pst_check(struct vframe_s *vf, void *a)
{
	union hw_sc2_ctr_pst_s *cfg;
	struct afbcd_ctr_s *pctr;
	struct di_buf_s *di_buf;
	struct di_ch_s *pch;
	union afbc_blk_s	*en_set_pst;
	union afbc_blk_s	*en_cfg_pst;

	if (!vf ||
	    !di_afdp ||
	    !a)
		return;

	pch = (struct di_ch_s *)a;
	di_buf = (struct di_buf_s *)vf->private_data;

	if (!di_buf)
		return;

	//cfg = di_afdp->top_cfg_pst;
	pctr = di_get_afd_ctr();

	pctr->en_sgn_pst.d8 = di_buf->afbc_sgn_cfg;
	en_set_pst = &pch->rse_ori.di_post_stru.en_set;
	en_cfg_pst = &pch->rse_ori.di_post_stru.en_cfg;
	en_set_pst->d8 = pctr->en_sgn_pst.d8 & en_cfg_pst->d8;

	pctr->en_set_pst.d8	= en_set_pst->d8;
	pctr->en_cfg.d8		= en_cfg_pst->d8;
	cfg = &pch->rse_ori.di_post_stru.pst_top_cfg;

	if (pctr->en_set_pst.b.if0)
		cfg->b.afbc_if0	= 1;
	else
		cfg->b.afbc_if0	= 0;

	if (pctr->en_set_pst.b.if1)
		cfg->b.afbc_if1	= 1;
	else
		cfg->b.afbc_if1	= 0;

	if (pctr->en_set_pst.b.if2)
		cfg->b.afbc_if2	= 1;
	else
		cfg->b.afbc_if2	= 0;

	if (pctr->en_set_pst.b.enc_wr)
		cfg->b.afbc_wr	= 1;
	else
		cfg->b.afbc_wr	= 0;

	//dim_print("%s:\n", __func__);
	//dim_print("%s:0x%x=0x%x\n", __func__,
		    //AFBCDM_MEM_HEAD_BADDR, reg_rd(AFBCDM_MEM_HEAD_BADDR));
}

static void afbc_check_chg_level(struct vframe_s *vf,
				 struct vframe_s *mem_vf,
				 struct vframe_s *chan2_vf,
				 struct afbcd_ctr_s *pctr)
{
#ifdef AFBCP
	struct di_buf_s *di_buf = NULL;
#endif
	/*check support*/
	if (pctr->fb.ver == AFBCD_NONE)
		return;
	if (is_cfg(EAFBC_CFG_DISABLE))
		return;

	if (pctr->fb.ver < AFBCD_V4 || pctr->fb.mode < AFBC_WK_P) {
		if (!(vf->type & VIDTYPE_COMPRESS)) {
			if (pctr->b.en) {
				/*from en to disable*/
				pctr->b.en = 0;
			}
			//dim_print("%s:not compress\n", __func__);
			return;
		}
	}
	/* patch for not mask nv21 */
	if ((vf->type & AFBC_VTYPE_MASK_CHG) !=
	    (pctr->l_vtype & AFBC_VTYPE_MASK_CHG)	||
	    vf->height != pctr->l_h		||
	    vf->width != pctr->l_w		||
	    vf->bitdepth != pctr->l_bitdepth) {
		pctr->b.chg_level = 3;
		pctr->l_vtype = (vf->type & AFBC_VTYPE_MASK_SAV);
		pctr->l_h = vf->height;
		pctr->l_w = vf->width;
		pctr->l_bitdepth = vf->bitdepth;
	} else {
		if (vf->type & VIDTYPE_INTERLACE) {
			pctr->b.chg_level = 2;
			pctr->l_vtype = (vf->type & AFBC_VTYPE_MASK_SAV);
		} else {
			pctr->b.chg_level = 1;
		}
	}
	if (is_cfg(EAFBC_CFG_LEVE3))
		pctr->b.chg_level = 3;
	pctr->addr_h = vf->compHeadAddr;
	pctr->addr_b = vf->compBodyAddr;
#ifdef MARK_SC2
	di_print("%s:\n", __func__);
	di_print("\tvf:type[0x%x],[0x%x]\n",
		 vf->type,
		 AFBC_VTYPE_MASK_SAV);
	di_print("\t\t:body[0x%x] inf[0x%x]\n",
		 vf->compBodyAddr, vf->compHeadAddr);
	di_print("\tinfo:type[0x%x]\n", pctr->l_vtype);

	di_print("\t\th[%d],w[%d],b[0x%x]\n", pctr->l_h,
		 pctr->l_w, pctr->l_bitdepth);
	di_print("\t\tchg_level[%d],is_srci[%d]\n",
		 pctr->b.chg_level, is_src_i(vf));
#endif
	if (!pctr->fb.mode || !mem_vf) {
		pctr->b.chg_mem = 0;
		pctr->l_vt_mem = 0;
		di_print("\t not check mem");
		return;
	}
#ifdef AFBCP
	/* mem */
	if (pctr->fb.mode >= AFBC_WK_P) {
		if ((mem_vf->type & AFBC_VTYPE_MASK_CHG) !=
		    (pctr->l_vt_mem & AFBC_VTYPE_MASK_CHG)) {
			pctr->b.chg_mem = 3;
			pctr->l_vt_mem = (mem_vf->type & AFBC_VTYPE_MASK_SAV);
		} else {
			pctr->b.chg_mem = 1;
		}
		di_buf = (struct di_buf_s *)mem_vf->private_data;
		if (di_buf && di_buf->type == VFRAME_TYPE_LOCAL) {
			#ifdef AFBC_MODE1
			di_print("buf t[%d]:nr_adr[0x%lx], afbc_adr[0x%lx]\n",
				 di_buf->index,
				 di_buf->nr_adr,
				 di_buf->afbc_adr);
			#endif
			//mem_vf->compBodyAddr = di_buf->nr_adr;
			//mem_vf->compHeadAddr = di_buf->afbc_adr;
		}
	}
#endif
#ifdef MARK_SC2
	di_print("\tmem:type[0x%x]\n", mem_vf->type);
	di_print("\t\t:body[0x%x] inf[0x%x]\n",
		 mem_vf->compBodyAddr, mem_vf->compHeadAddr);
	di_print("\t\th[%d],w[%d],b[0x%x]\n", mem_vf->height,
		 mem_vf->width, mem_vf->bitdepth);

	di_print("\t\tchg_mem[%d],is_srci[%d]\n",
		 pctr->b.chg_mem, is_src_i(vf));
#endif
	/* chan2 */
	if (!chan2_vf)
		return;
	if (pctr->en_set.b.chan2) {
		if ((chan2_vf->type & AFBC_VTYPE_MASK_CHG) !=
		    (pctr->l_vt_chan2 & AFBC_VTYPE_MASK_CHG)) {
			pctr->b.chg_chan2 = 3;
			pctr->l_vt_chan2 =
				(chan2_vf->type & AFBC_VTYPE_MASK_SAV);
		} else {
			pctr->b.chg_chan2 = 1;
		}
	}
#ifdef MARK_SC2
	di_print("\tchan2:type[0x%x]\n", chan2_vf->type);
	di_print("\t\t:body[0x%x] inf[0x%x]\n",
		 chan2_vf->compBodyAddr, chan2_vf->compHeadAddr);
	di_print("\t\th[%d],w[%d],b[0x%x]\n", chan2_vf->height,
		 chan2_vf->width, chan2_vf->bitdepth);

	di_print("\t\tchg[%d],is_srci[%d]\n",
		 pctr->b.chg_chan2, is_src_i(vf));
#endif
}

static void afbc_update_level1(struct vframe_s *vf, enum EAFBC_DEC dec)
{
	const unsigned int *reg = afbc_get_addrp(dec);

	//di_print("%s:%d:0x%x\n", __func__, dec, vf->type);
	//dim_print("%s:head2:0x%x=0x%x\n", __func__,
	//AFBCDM_MEM_HEAD_BADDR, reg_rd(AFBCDM_MEM_HEAD_BADDR));
	reg_wr(reg[EAFBC_HEAD_BADDR], vf->compHeadAddr >> 4);
	reg_wr(reg[EAFBC_BODY_BADDR], vf->compBodyAddr >> 4);
}

void dbg_afbc_update_level1(struct vframe_s *vf, enum EAFBC_DEC dec)
{
	afbc_update_level1(vf, dec);
}

/* ary add for v5 sc2 */
struct AFBCD_CFG_S {
	unsigned int pip_src_mode;
	// 1 bits   0: src from vdin/dos  1:src from pip
	unsigned int rot_ofmt_mode;
	// 2 bits   default = 2, 0:yuv444 1:yuv422 2:yuv420
	unsigned int rot_ocompbit;
	// 2 bits rot output bit width,0:8bit 1:9bit 2:10bit
	unsigned int rot_drop_mode; //rot_drop_mode
	unsigned int rot_vshrk; //rot_vshrk
	unsigned int rot_hshrk; //rot_hshrk
	unsigned int rot_hbgn; //5 bits
	unsigned int rot_vbgn; //2 bits
	unsigned int rot_en; //1 bits

	unsigned int reg_lossy_en;	/* def 0*/
};

static unsigned int afbcd_v5_get_offset(enum EAFBC_DEC dec)
{
	unsigned int module_sel;
	unsigned int regs_ofst;

	switch (dec) {
	case EAFBC_DEC0:
		module_sel = 6;
		break;
	case EAFBC_DEC1:
		module_sel = 7;
		break;
	case EAFBC_DEC2_DI:
		module_sel = 0;
		break;
	case EAFBC_DEC3_MEM:
		module_sel = 2;
		break;
	case EAFBC_DEC_CHAN2:
		module_sel = 1;
		break;
	case EAFBC_DEC_IF0:
		module_sel = 4;
		break;
	case EAFBC_DEC_IF1:
		module_sel = 3;
		break;
	case EAFBC_DEC_IF2:
		module_sel = 5;
		break;
	default:
		PR_ERR("%s:err\n", __func__);
		module_sel = 6;
		break;
	}

	regs_ofst = module_sel == 6 ? -24 : //vd1      0x48 0x0-0x7f
		module_sel == 7 ? -23 : //vd2      0x48 0x80-0xff
		module_sel;//di_m0-m5 0x54-0x56
	//regs_ofst = 128 * 4 * regs_ofst;
	regs_ofst = 0x80 * regs_ofst;
	return regs_ofst;
}

/**********************************************
 * s4dw:
 * phase_step is replace by vt_ini_phase
 * 8 or 16 before s4dw
 * input VIDTYPE_COMPRESS_LOSS ,di input 0x5452 bit0/4/10/11 set 1
 * if di out loss 0x5552 bit0/4/10/11 set 1 from vlsi feijun (same vd1)
 **********************************************/
static u32 enable_afbc_input_local(struct vframe_s *vf, enum EAFBC_DEC dec,
				   struct AFBCD_CFG_S *cfg)
{
	unsigned int r, u, v, w_aligned, h_aligned, vfw, vfh;
	const unsigned int *reg = afbc_get_addrp(dec);
	unsigned int vfmt_rpt_first = 1, vt_ini_phase = 0;
	unsigned int out_height = 0;
	/*ary add*/
	unsigned int cvfmt_en = 0;
	unsigned int cvfm_h, rpt_pix, /*phase_step = 16, 0322*/hold_line = 8;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	/* add from sc2 */
	unsigned int fmt_mode = 0;
	unsigned int compbits_yuv;	// 2 bits 0-8bits 1-9bits 2-10bits
	unsigned int compbits_eq8 ;	/* add for sc2 */
	unsigned int conv_lbuf_len;
	unsigned int regs_ofst;
	/* */
	unsigned int dec_lbuf_depth  ; //12 bits
	unsigned int mif_lbuf_depth  ; //12 bits
//ary tmp	unsigned int fmt_size_h     ; //13 bits
//ary tmp	unsigned int fmt_size_v     ; //13 bits
	char vert_skip_y	= 0;//inp_afbcd->v_skip_en;
	// 2 bits 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
	char horz_skip_y	= 0;//inp_afbcd->h_skip_en;
	// 2 bits 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
	char vert_skip_uv = 0;//inp_afbcd->v_skip_en;
	// 2 bits 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
	char horz_skip_uv = 0;//inp_afbcd->h_skip_en;
	// 2 bits 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
	u32 fmt_size_h ; //13 bits
	u32 fmt_size_v; //13 bits
	u32 hfmt_en	= 0;
	u32 vfmt_en	= 0;
	u32 uv_vsize_in	= 0;
	u32 vt_yc_ratio	= 0;
	u32 hz_yc_ratio = 0;
	u32 vt_phase_step;
	u32 vfmt_w;
	bool skip_en = false;

	static unsigned int lst_vfw, lst_vfh;

	di_print("afbcd:[%d]:vf typ[0x%x] 0x%lx, 0x%lx\n",
		 dec,
		 vf->type,
		 vf->compHeadAddr,
		 vf->compBodyAddr);
	vfw = vf->width;
	vfh = vf->height;
	if (IS_FIELD_I_SRC(vf->type))
		vfh = (vfh + 1) >> 1;
	dbg_copy("afbcdin:0x%px, pulldown[%d]\n", vf, vf->di_pulldown);

	/* reset inp afbcd */
	if (pafd_ctr->fb.ver >= AFBCD_V5) {
		u32 reset_bit = 0;

		if (dec == EAFBC_DEC2_DI)
			reset_bit = 1 << 12;
		if (reset_bit) {
			reg_wr(VIUB_SW_RESET, reset_bit);
			reg_wr(VIUB_SW_RESET, 0);
		}
	}

	if (DIM_IS_IC_EF(T7)) {
		union hw_sc2_ctr_pre_s *pre_top_cfg;
		u32 val, reg = 0;
		u8 is_4k, is_afbc;

		pre_top_cfg = &get_hw_pre()->pre_top_cfg;
		if (dec == EAFBC_DEC2_DI) {
			reg = AFBCDM_INP_CTRL0;
			is_4k = pre_top_cfg->b.is_inp_4k ? 1 : 0;
			is_afbc = pre_top_cfg->b.afbc_inp ? 1 : 0;
		} else if (dec == EAFBC_DEC3_MEM) {
			reg = AFBCDM_MEM_CTRL0;
			is_4k = pre_top_cfg->b.is_mem_4k ? 1 : 0;
			is_afbc = pre_top_cfg->b.afbc_mem ? 1 : 0;
		} else if (dec == EAFBC_DEC_CHAN2) {
			reg = AFBCDM_CHAN2_CTRL0;
			is_4k = pre_top_cfg->b.is_chan2_4k ? 1 : 0;
			is_afbc = pre_top_cfg->b.afbc_chan2 ? 1 : 0;
		}
		if (reg) {
			val = reg_rd(reg);
			val &= ~(3 << 13);
			//reg_use_4kram
			val |= is_4k << 14;
			//reg_afbc_vd_sel //1:afbc_dec 0:nor_rdmif
			val |= is_afbc << 13;
			reg_wr(reg, val);
		}
	}

	/*use vf->di_pulldown as skip flag */
	if (dip_cfg_afbc_skip() || vf->di_pulldown == 1)
		skip_en = true;

	if (skip_en) {
		vert_skip_y = 1;
		horz_skip_y = 1;
		vert_skip_uv = 1;
		horz_skip_uv = 1;
	}
	if (vfw != lst_vfw || vfh != lst_vfh) {//dbg only
		dbg_copy("afbcd: in<%d,%d> -> <%d,%d>\n",
			lst_vfw, lst_vfh, vfw, vfh);
		lst_vfw = vfw;
		lst_vfh = vfh;
	}
	w_aligned = round_up((vfw), 32);

	fmt_size_h   = (((vfw - 1) >> 1) << 1) + 1;
	//((out_horz_end >> 1) << 1) + 1 - ((out_horz_bgn >> 1) << 1);
	fmt_size_v   = (((vfh - 1) >> 1) << 1) + 1;
	//((out_vert_end >> 1) << 1) + 1 - ((out_vert_bgn >> 1) << 1);
	fmt_size_h   = horz_skip_y != 0 ? (fmt_size_h >> 1) + 1 :
		fmt_size_h + 1;
	fmt_size_v   = vert_skip_y != 0 ? (fmt_size_v >> 1) + 1 :
		fmt_size_v + 1;
	/* add from sc2 */

	/* TL1 add bit[13:12]: fmt_mode; 0:yuv444; 1:yuv422; 2:yuv420
	 * di does not support yuv444, so for fmt yuv444 di will bypass+
	 */
	if (pafd_ctr->fb.ver >= AFBCD_V3) {
		if (vf->type & VIDTYPE_VIU_444)
			fmt_mode = 0;
		else if (vf->type & VIDTYPE_VIU_422)
			fmt_mode = 1;
		else
			fmt_mode = 2;
	}

	compbits_yuv = (vf->bitdepth & BITDEPTH_MASK) >> BITDEPTH_Y_SHIFT;
	compbits_eq8 = (compbits_yuv == 0);

	/* 2021-03-09 from tm2 vb, set 128 or 256 */
	//if (pafd_ctr->fb.ver <= AFBCD_V4 && !DIM_IS_IC(T5D)) { //for t5d tmp
	if (pafd_ctr->fb.ver <= AFBCD_V3) {
		conv_lbuf_len = 256;
	} else {
		if ((w_aligned > 1024 && fmt_mode == 0 &&
		     compbits_eq8 == 0)	||
		    w_aligned > 2048)
			conv_lbuf_len = 256;
		else
			conv_lbuf_len = 128;
	}
	dec_lbuf_depth = 128;
	mif_lbuf_depth = 128;
	/*****************************************/
	/*if (di_pre_stru.cur_inp_type & VIDTYPE_INTERLACE)*/
	if (((vf->type & VIDTYPE_INTERLACE) || is_src_i(vf)) &&
	    (vf->type & VIDTYPE_VIU_422)) /*from vdin and is i */
		h_aligned = round_up((vfh / 2), 4);
	else
		h_aligned = round_up((vfh), 4);

	/*AFBCD working mode config*/
	r = (3 << 24) |
	    (hold_line << 16) |		/* hold_line_num : 2020 from 10 to 8*/
	    (2 << 14) | /*burst1 1:2020:ary change from 1 to 2*/
	    (vf->bitdepth & BITDEPTH_MASK)	|
	    ((vert_skip_y  & 0x3) << 6) |  // vert_skip_y
	    ((horz_skip_y  & 0x3) << 4) |  // horz_skip_y
	    ((vert_skip_uv & 0x3) << 2) |  // vert_skip_uv
	    ((horz_skip_uv & 0x3) << 0);    // horz_skip_uv;

	if (vf->bitdepth & BITDEPTH_SAVING_MODE)
		r |= (1 << 28); /* mem_saving_mode */
	if (vf->type & VIDTYPE_SCATTER)
		r |= (1 << 29);

	out_height = h_aligned;
	if (!(vf->type & VIDTYPE_VIU_422) && !(vf->type & VIDTYPE_VIU_FIELD)) {
		/*from dec, process P as i*/
		if ((vf->type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
			r |= 0x40;
			vt_ini_phase = 0xc;
			vfmt_rpt_first = 1;
			out_height = h_aligned >> 1;
		} else if ((vf->type & VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_BOTTOM) {
			r |= 0x80;
			vt_ini_phase = 0x4;
			vfmt_rpt_first = 0;
			out_height = h_aligned >> 1;
		}
	}

	if ((vf->type & (VIDTYPE_VIU_422 | VIDTYPE_VIU_444)) == 0) {
		cvfmt_en = 1;
		vt_ini_phase = 0xc;
		cvfm_h = out_height >> 1;
		rpt_pix = 1;
		/*phase_step = 8; 0322*/
	} else {
		cvfm_h = out_height;
		rpt_pix = 0;
	}
	reg_wr(reg[EAFBC_MODE], r);

	r = 0x1600;
	//if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
	if (pafd_ctr->fb.ver >= AFBCD_V3) {
	/* un compress mode data from vdin bit block order is
	 * different with from dos
	 */
		if (!(vf->type & VIDTYPE_VIU_422) &&
		    ((dec == EAFBC_DEC2_DI) ||
		     (dec == EAFBC_DEC0)))
		    /* ary note: frame afbce, set 0*/
			r |= (1 << 19); /* dos_uncomp */

		if (vf->type & VIDTYPE_COMB_MODE)
			r |= (1 << 20);
	}
	#ifdef MARK_SC2 /* ary: tm2 no this setting */
	if (pafd_ctr->fb.ver >= AFBCD_V4) {
		r |= (1 << 21); /*reg_addr_link_en*/
		r |= (0x43700 << 0); /*??*/
	}
	#endif
	reg_wr(reg[EAFBC_ENABLE], r);

	/*pr_info("AFBC_ENABLE:0x%x\n", reg_rd(reg[eAFBC_ENABLE]));*/

	/**/

	//r = 0x100;
	#ifdef MARK_SC2 /* add  fmt_mode */
	/* TL1 add bit[13:12]: fmt_mode; 0:yuv444; 1:yuv422; 2:yuv420
	 * di does not support yuv444, so for fmt yuv444 di will bypass+
	 */
	//if (is_meson_tl1_cpu() || is_meson_tm2_cpu()) {

	if (pafd_ctr->fb.ver >= AFBCD_V3) {
		if (vf->type & VIDTYPE_VIU_444)
			r |= (0 << 12);
		else if (vf->type & VIDTYPE_VIU_422)
			r |= (1 << 12);
		else
			r |= (2 << 12);
	}
	#endif
	//r |= fmt_mode << 12;

	reg_wr(reg[EAFBC_CONV_CTRL],
	       fmt_mode << 12 |
	       conv_lbuf_len << 0);/*fmt_mode before sc2 is 0x100*/

	u = (vf->bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
	v = (vf->bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
	reg_wr(reg[EAFBC_DEC_DEF_COLOR],
	       0x3FF00000	| /*Y,bit20+*/
	       0x80 << (u + 10)	|
	       0x80 << v);

	/*******************************************/
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

	dim_print("===uv_vsize_in= 0x%0x, fmt_mod=%d, fmt_sizeh=%d,%d\n",
		  uv_vsize_in,
		  fmt_mode, fmt_size_h, hfmt_en);
	dim_print("===vt_yc_ratio= 0x%0x\n", vt_yc_ratio);
	dim_print("===hz_yc_ratio= 0x%0x\n", hz_yc_ratio);
	//ary below is set_di_afbc_fmt

	vt_phase_step = (16 >> vt_yc_ratio);
	vfmt_w = (fmt_size_h >> hz_yc_ratio);
	/*******************************************/
	/* chroma formatter */
	reg_wr(reg[EAFBC_VD_CFMT_CTRL],
	       (rpt_pix << 28)	|
	       (hz_yc_ratio << 21)  |     //hz yc ratio
	       (hfmt_en << 20)      |     //hz enable
	       (vfmt_rpt_first << 16)	| /* VFORMATTER_RPTLINE0_EN */
	       (vt_ini_phase << 8)	|
	       (/*phase_step*/vt_phase_step << 1)	| /* VFORMATTER_PHASE_BIT */
	       cvfmt_en);/* different with inp */

	/*if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) { *//*ary add for g12a*/
	if (pafd_ctr->fb.ver >= AFBCD_V3) {
		if (!skip_en) {
			if (vf->type & VIDTYPE_VIU_444)
				reg_wr(reg[EAFBC_VD_CFMT_W],
				       (w_aligned << 16) | (w_aligned / 2));
			else
				reg_wr(reg[EAFBC_VD_CFMT_W],
				       (w_aligned << 16) | (w_aligned));
		} else {
			reg_wr(reg[EAFBC_VD_CFMT_W],
			       (fmt_size_h << 16) | vfmt_w);
		}
	} else {	/*ary add for g12a*/
		reg_wr(reg[EAFBC_VD_CFMT_W],
		       (w_aligned << 16) | (w_aligned / 2));
	}
	reg_wr(reg[EAFBC_MIF_HOR_SCOPE],
	       (0 << 16) | ((w_aligned >> 5) - 1));
	reg_wr(reg[EAFBC_MIF_VER_SCOPE],
	       (0 << 16) | ((h_aligned >> 2) - 1));

	reg_wr(reg[EAFBC_PIXEL_HOR_SCOPE],
	       (0 << 16) | (vfw - 1));
	reg_wr(reg[EAFBC_PIXEL_VER_SCOPE],
	       0 << 16 | (vfh - 1));
	/*s4dw: uv_vsize_in replace cvfm_h*/
	reg_wr(reg[EAFBC_VD_CFMT_H], /*out_height*//*cvfm_h 0322*/uv_vsize_in);

	reg_wr(reg[EAFBC_SIZE_IN], (vfh) | w_aligned << 16);
	di_print("\t:size:%d\n", vfh);
	if (pafd_ctr->fb.ver <= AFBCD_V4)
		reg_wr(reg[EAFBC_SIZE_OUT], out_height | w_aligned << 16);

	reg_wr(reg[EAFBC_HEAD_BADDR], vf->compHeadAddr >> 4);
	reg_wr(reg[EAFBC_BODY_BADDR], vf->compBodyAddr >> 4);
	if (pafd_ctr->fb.ver == AFBCD_V4) {
		if (dec == EAFBC_DEC2_DI) {
			if (vf->type & VIDTYPE_COMPRESS_LOSS)
				reg_wr(DI_INP_AFBC_IQUANT_ENABLE, 0xc11);
			else
				reg_wr(DI_INP_AFBC_IQUANT_ENABLE, 0x0);
		}
		if (dec == EAFBC_DEC3_MEM) {
			if (cfgg(AFBCE_LOSS_EN) == 1 ||
			    ((vf->type & VIDTYPE_COMPRESS_LOSS) &&
			     cfgg(AFBCE_LOSS_EN) == 2))
				reg_wr(DI_MEM_AFBC_IQUANT_ENABLE, 0xc11);
			else
				reg_wr(DI_MEM_AFBC_IQUANT_ENABLE, 0x0);
		}
	}
	//dim_print("dec=%d:type=0x%x,ver=0x%x,reg=0x%x\n", dec,vf->type,
		//pafd_ctr->fb.ver,reg_rd(0X1812));
	if (pafd_ctr->fb.ver >= AFBCD_V5 && cfg) {
		regs_ofst = afbcd_v5_get_offset(dec);
		reg_wrb((regs_ofst + AFBCDM_IQUANT_ENABLE),
			cfg->reg_lossy_en, 0, 1);//lossy_luma_en
		reg_wrb((regs_ofst + AFBCDM_IQUANT_ENABLE),
			cfg->reg_lossy_en, 4, 1);//lossy_chrm_en
		reg_wrb((regs_ofst + AFBCDM_IQUANT_ENABLE),
			cfg->reg_lossy_en, 10, 1);//lossy_luma_en extern
		reg_wrb((regs_ofst + AFBCDM_IQUANT_ENABLE),
			cfg->reg_lossy_en, 11, 1);//lossy_chrm_en extern

		reg_wr((regs_ofst + AFBCDM_ROT_CTRL),
		       ((cfg->pip_src_mode  & 0x1) << 27) |
		       //pip_src_mode
		       ((cfg->rot_drop_mode & 0x7) << 24) |
		       //rot_uv_vshrk_drop_mode
		       ((cfg->rot_drop_mode & 0x7) << 20) |
		       //rot_uv_hshrk_drop_mode
		       ((cfg->rot_vshrk	  & 0x3) << 18) |
		       //rot_uv_vshrk_ratio
		       ((cfg->rot_hshrk	  & 0x3) << 16) |
		       //rot_uv_hshrk_ratio
		       ((cfg->rot_drop_mode & 0x7) << 12) |
		       //rot_y_vshrk_drop_mode
		       ((cfg->rot_drop_mode & 0x7) << 8) |
		       //rot_y_hshrk_drop_mode
		       ((cfg->rot_vshrk	  & 0x3) << 6) |
		       //rot_y_vshrk_ratio
		       ((cfg->rot_hshrk	  & 0x3) << 4) |
		       //rot_y_hshrk_ratio
		       ((0		  & 0x3) << 2) |
		       //rot_uv422_drop_mode
		       ((0		  & 0x3) << 1) |
		       //rot_uv422_omode
		       ((cfg->rot_en	  & 0x1) << 0)
		       //rot_enable
		       );

		reg_wr((regs_ofst + AFBCDM_ROT_SCOPE),
		       ((1 & 0x1) << 16) | //reg_rot_ifmt_force444
		       ((cfg->rot_ofmt_mode & 0x3) << 14) |
		       //reg_rot_ofmt_mode
		       ((cfg->rot_ocompbit & 0x3) << 12) |
		       //reg_rot_compbits_out_y
		       ((cfg->rot_ocompbit & 0x3) << 10) |
		       //reg_rot_compbits_out_uv
		       ((cfg->rot_vbgn & 0x3) << 8) | //rot_wrbgn_v
		       ((cfg->rot_hbgn & 0x1f) << 0) //rot_wrbgn_h
		       );
	}
	return true;
}

static void afbc_update_level2_inp(struct afbcd_ctr_s *pctr)
{
	const unsigned int *reg = afbc_get_addrp(pctr->fb.pre_dec);
	unsigned int vfmt_rpt_first = 1, vt_ini_phase = 12;
	unsigned int old_mode, old_cfmt_ctrl;

	di_print("afbcd:up:%d\n", pctr->fb.pre_dec);
	old_mode = reg_rd(reg[EAFBC_MODE]);
	old_cfmt_ctrl = reg_rd(reg[EAFBC_VD_CFMT_CTRL]);
	old_mode &= (~(0x03 << 6));
	if (!(pctr->l_vtype & VIDTYPE_VIU_422) &&
	    !(pctr->l_vtype & VIDTYPE_VIU_FIELD)) {
		/*from dec, process P as i*/
		if ((pctr->l_vtype & VIDTYPE_TYPEMASK) ==
		    VIDTYPE_INTERLACE_TOP) {
			old_mode |= 0x40;

			vt_ini_phase = 0xc;
			vfmt_rpt_first = 1;
			//out_height = h_aligned>>1;
		} else if ((pctr->l_vtype & VIDTYPE_TYPEMASK) ==
			   VIDTYPE_INTERLACE_BOTTOM) {
			old_mode |= 0x80;
			vt_ini_phase = 0x4;
			vfmt_rpt_first = 0;
			//out_height = h_aligned>>1;
		} else { //for p as p?
			//out_height = h_aligned>>1;
		}
	}
	reg_wr(reg[EAFBC_MODE], old_mode);
	/* chroma formatter */
	reg_wr(reg[EAFBC_VD_CFMT_CTRL],
	       old_cfmt_ctrl		|
	       (vfmt_rpt_first << 16)	| /* VFORMATTER_RPTLINE0_EN */
	       (vt_ini_phase << 8)); /* different with inp */

	reg_wr(reg[EAFBC_HEAD_BADDR], pctr->addr_h >> 4);
	reg_wr(reg[EAFBC_BODY_BADDR], pctr->addr_b >> 4);
}

static void afbce_set(struct vframe_s *vf, enum EAFBC_ENC enc);
static void afbce_update_level1(struct vframe_s *vf,
				const struct reg_acc *op,
				enum EAFBC_ENC enc);
static void afbc_tm2_sw_inp(bool on, const struct reg_acc *op);
/* only for tm2, sc2 is chang*/
static void afbc_tm2_sw_mem(bool on, const struct reg_acc *op);
/* only for tm2, sc2 is chang*/
static void afbce_tm2_sw(bool on, const struct reg_acc *op);

static u32 enable_afbc_input(struct vframe_s *inp_vf,
			     struct vframe_s *mem_vf,
			     struct vframe_s *chan2_vf,
			     struct vframe_s *nr_vf)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	//enum eAFBC_DEC dec = afbc_get_decnub();
	struct vframe_s *mem_vf2, *inp_vf2;
	struct AFBCD_CFG_S cfg;
	struct AFBCD_CFG_S *pcfg;

	if (!afbc_is_supported())
		return false;

	inp_vf2 = inp_vf;
	mem_vf2 = mem_vf;

	if (is_cfg(EAFBC_CFG_ETEST))
		mem_vf2 = inp_vf;
	else if (is_cfg(EAFBC_CFG_ETEST2))
		inp_vf2 = mem_vf;

	if (!inp_vf2) {
		PR_ERR("%s:no input\n", __func__);
		return 0;
	}

	if (pafd_ctr->fb.ver < AFBCD_V4) {
		if (inp_vf2->type & VIDTYPE_COMPRESS) {
			afbc_sw(true);
		} else {
			afbc_sw(false);
			return false;
		}
	} else {
		/* AFBCD_V5 */
		if (!pafd_ctr->en_set.d8) {
			dim_print("%s:%d\n", __func__, pafd_ctr->en_set.d8);
			afbc_sw(false);
			return false;
		}
		afbc_sw(true);
	}
#ifdef MARK_SC2
	if (is_cfg(EAFBC_CFG_ETEST))
		mem_vf2 = inp_vf;
	else
		mem_vf2 = mem_vf;
#endif
	if (pafd_ctr->fb.ver >= AFBCD_V5) {
		pcfg = &cfg;
		memset(pcfg, 0, sizeof(*pcfg));
	} else {
		pcfg = NULL;
	}

	afbc_check_chg_level(inp_vf2, mem_vf, chan2_vf, pafd_ctr);
	if (is_src_real_i(mem_vf))
		//dim_print("%s:real i\n", __func__);
		pafd_ctr->b.chg_mem = 3;

	if (pafd_ctr->b.chg_level == 3 || pafd_ctr->b.chg_mem == 3 ||
	    pafd_ctr->b.chg_chan2 == 3) {
		if (pafd_ctr->fb.ver == AFBCD_V4) {
			if (pafd_ctr->en_set.b.inp)
				afbc_tm2_sw_inp(true, &di_normal_regset);
			else
				afbc_tm2_sw_inp(false, &di_normal_regset);
			if (mem_vf2 && pafd_ctr->en_set.b.mem)
				afbc_tm2_sw_mem(true, &di_normal_regset);
			else
				afbc_tm2_sw_mem(false, &di_normal_regset);

			if (pafd_ctr->en_set.b.enc_nr)
				afbce_tm2_sw(true, &di_normal_regset);
			else
				afbce_tm2_sw(false, &di_normal_regset);
		}
		/*inp*/
		if (pafd_ctr->en_set.b.inp) {
			if (inp_vf2->type & VIDTYPE_COMPRESS_LOSS)
				cfg.reg_lossy_en = 1;
			enable_afbc_input_local(inp_vf2,
						pafd_ctr->fb.pre_dec, pcfg);
		}
		if (is_src_i(inp_vf2) || VFMT_IS_I(inp_vf2->type))
			src_i_set(nr_vf);
			//dim_print("%s:set srci\n", __func__);

		if (mem_vf2 && pafd_ctr->en_set.b.mem) {
			/* mem */
			if (cfgg(AFBCE_LOSS_EN) == 1 ||
			    ((mem_vf2->type & VIDTYPE_COMPRESS_LOSS) &&
			     cfgg(AFBCE_LOSS_EN) == 2)) {//di loss
				cfg.reg_lossy_en = 1;
				nr_vf->type |= VIDTYPE_COMPRESS_LOSS;
			} else {
				cfg.reg_lossy_en = 0;
				nr_vf->type &= ~VIDTYPE_COMPRESS_LOSS;
			}
			enable_afbc_input_local(mem_vf2,
						pafd_ctr->fb.mem_dec,
						pcfg);
		}
		if (chan2_vf && pafd_ctr->en_set.b.chan2)
			/* chan2 */
			enable_afbc_input_local(chan2_vf,
						pafd_ctr->fb.ch2_dec,
						pcfg);

		/*nr*/
		if (pafd_ctr->en_set.b.enc_nr)
			afbce_set(nr_vf, EAFBC_ENC0);

	} else if (pafd_ctr->b.chg_level == 2) {
		if (pafd_ctr->en_set.b.inp)
			afbc_update_level2_inp(pafd_ctr);

		if (is_src_i(inp_vf2) || VFMT_IS_I(inp_vf2->type)) {
			src_i_set(nr_vf);
			dim_print("%s:set srci\n", __func__);
		}

		/*mem*/
		if (pafd_ctr->en_set.b.mem) {
			if (is_cfg(EAFBC_CFG_FORCE_MEM))
				enable_afbc_input_local(mem_vf2,
							pafd_ctr->fb.mem_dec,
							pcfg);
			else
				afbc_update_level1(mem_vf2,
						   pafd_ctr->fb.mem_dec);
#ifdef MARK_SC2
			if (is_cfg(EAFBC_CFG_FORCE_IF1)) {
				enable_afbc_input_local(mem_vf2,
							pafd_ctr->fb.if1_dec,
							pcfg);
				afbcd_reg_bwr(EAFBC_DEC_IF1,
					      EAFBC_ENABLE, 1, 8, 1);
			}
#endif
		}

		if (pafd_ctr->en_set.b.chan2 && chan2_vf) {
			if (is_cfg(EAFBC_CFG_FORCE_CHAN2))
				enable_afbc_input_local
					(chan2_vf,
					 pafd_ctr->fb.ch2_dec,
					 pcfg);
			else
				afbc_update_level1(chan2_vf,
						   pafd_ctr->fb.ch2_dec);
		}

		/*nr*/
		if (pafd_ctr->en_set.b.enc_nr) {
			if (is_cfg(EAFBC_CFG_FORCE_NR))
				afbce_set(nr_vf, EAFBC_ENC0);
			else
				afbce_update_level1(nr_vf,
						    &di_normal_regset,
						    EAFBC_ENC0);
		}
	} else if (pafd_ctr->b.chg_level == 1) {
		/*inp*/
		if (pafd_ctr->en_set.b.inp)
			afbc_update_level1(inp_vf2, pafd_ctr->fb.pre_dec);

		/*mem*/
		if (pafd_ctr->en_set.b.mem)
			afbc_update_level1(mem_vf2, pafd_ctr->fb.mem_dec);

		if (pafd_ctr->en_set.b.chan2 && chan2_vf)
			afbc_update_level1(chan2_vf, pafd_ctr->fb.ch2_dec);

		/*nr*/
		if (cfgg(AFBCE_LOSS_EN) == 1 ||
		    ((mem_vf2->type & VIDTYPE_COMPRESS_LOSS) &&
		     cfgg(AFBCE_LOSS_EN) == 2))
			nr_vf->type |= VIDTYPE_COMPRESS_LOSS;
		else
			nr_vf->type &= ~VIDTYPE_COMPRESS_LOSS;

		if (pafd_ctr->en_set.b.enc_nr)
			afbce_update_level1(nr_vf,
					    &di_normal_regset, EAFBC_ENC0);
	}
	return 0;
}

/* post */
static void afbc_pst_check_chg_l(struct vframe_s *if0_vf,
				 struct vframe_s *if1_vf,
				 struct vframe_s *if2_vf,
				 struct afbcd_ctr_s *pctr)
{
	/*check support*/
	if (pctr->fb.ver == AFBCD_NONE)
		return;

	if (!(if0_vf->type & VIDTYPE_COMPRESS)) {
	#ifdef MARK_SC2
		if (pctr->b.en) {
			/*from en to disable*/
			pctr->b.en = 0;
		}
	#endif
		pctr->b.pst_chg_level	= 0;
		pctr->b.pst_chg_if1	= 0;
		pctr->b.pst_chg_if2	= 0;
		return;
	}
	/* if0 check */
	if ((if0_vf->type & AFBC_VTYPE_MASK_CHG) !=
	    (pctr->pst_in_vtp & AFBC_VTYPE_MASK_CHG)	||
	    if0_vf->height != pctr->pst_in_h		||
	    if0_vf->width != pctr->pst_in_w		||
	    if0_vf->bitdepth != pctr->pst_in_bitdepth) {
		pctr->b.pst_chg_level	= 3;
		pctr->b.pst_chg_if1	= 3;
		pctr->b.pst_chg_if2	= 3;

		pctr->pst_in_vtp = (if0_vf->type & AFBC_VTYPE_MASK_SAV);
		pctr->pst_in_h = if0_vf->height;
		pctr->pst_in_w = if0_vf->width;
		pctr->pst_in_bitdepth = if0_vf->bitdepth;
	} else {
		pctr->b.pst_chg_level	= 1;
		//pctr->b.pst_chg_if1	= 1;
		//pctr->b.pst_chg_if2	= 1;
	}
	/* if1 check */
	if (!if1_vf) {
		pctr->b.pst_chg_if1	= 0;
	} else if (!pctr->b.pst_chg_if1) {
		/* no to have */
		pctr->b.pst_chg_if1	= 3;
	} else if (pctr->b.pst_chg_if1 == 3) {
	} else {
		pctr->b.pst_chg_if1	= 1;
	}

	/* if2 check */
	if (!if2_vf) {
		pctr->b.pst_chg_if2	= 0;
	} else if (!pctr->b.en_pst_if2) {
		/* no to have */
		pctr->b.pst_chg_if2	= 3;
	} else if (pctr->b.pst_chg_if2 == 3) {
	} else {
		pctr->b.pst_chg_if2	= 1;
	}

	if (is_cfg(EAFBC_CFG_LEVE3)) {
		pctr->b.chg_level = 3;

		pctr->b.pst_chg_level	= 3;
		pctr->b.pst_chg_if1	= 3;
		pctr->b.pst_chg_if2	= 3;
	}
#ifdef PRINT_BASIC
	di_print("%s:\n", __func__);
	di_print("\tif0:type[0x%x]\n", if0_vf->type);
	di_print("\t\t:body[0x%x] inf[0x%x]\n",
		 if0_vf->compBodyAddr, if0_vf->compHeadAddr);
	di_print("\t\th[%d],w[%d],b[0x%x]\n", if0_vf->height,
		 if0_vf->width, if0_vf->bitdepth);

	di_print("\t\tchg[%d],is_srci[%d]\n",
		 pctr->b.pst_chg_level, is_src_i(if0_vf));

	if (if1_vf) {
		di_print("\tif1:type[0x%x]\n", if1_vf->type);
		di_print("\t\t:body[0x%x] inf[0x%x]\n",
			 if1_vf->compBodyAddr, if1_vf->compHeadAddr);
		di_print("\t\th[%d],w[%d],b[0x%x]\n", if1_vf->height,
			 if1_vf->width, if1_vf->bitdepth);

		di_print("\t\tchg[%d],is_srci[%d]\n",
			 pctr->b.pst_chg_if1, is_src_i(if1_vf));
	}

	if (if2_vf) {
		di_print("\tif2:type[0x%x]\n", if2_vf->type);
		di_print("\t\t:body[0x%x] inf[0x%x]\n",
			 if2_vf->compBodyAddr, if2_vf->compHeadAddr);
		di_print("\t\th[%d],w[%d],b[0x%x]\n", if2_vf->height,
			 if2_vf->width, if2_vf->bitdepth);

		di_print("\t\tchg[%d],is_srci[%d]\n",
			 pctr->b.pst_chg_if2, is_src_i(if2_vf));

	} else {
		di_print("%s:no if2?\n", __func__);
	}

	di_print("\t:if0_vf->type[0x%x]\n", pctr->pst_in_vtp);

	di_print("\th[%d],w[%d],b[0x%x]\n",
		 pctr->pst_in_h,
		 pctr->pst_in_w,
		 pctr->pst_in_bitdepth);
	di_print("\tchg_level[%d] if1[%d], if2[%d]\n",
		 pctr->b.pst_chg_level,
		 pctr->b.pst_chg_if1,
		 pctr->b.pst_chg_if2);
#endif
}

void afbc_sw_pst(bool sw)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!pafd_ctr->b.en_pst && sw) {
		/*on*/
		pafd_ctr->b.en_pst = 1;
	} else if (pafd_ctr->b.en_pst && !sw) {
		pafd_ctr->b.en_pst = 0;
	}
	//dim_print("%s:%d\n", __func__, sw);
}

static u32 afbc_pst_set(struct vframe_s *if0_vf,
			struct vframe_s *if1_vf,
			struct vframe_s *if2_vf,
			struct vframe_s *wr_vf)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	//enum eAFBC_DEC dec = afbc_get_decnub();
	struct vframe_s *if1_vf2, *if2_vf2;
	//struct AFBCD_CFG_S cfg;
	struct AFBCD_CFG_S *pcfg = NULL;
	struct AFBCD_CFG_S cfg_afbcd;
	union hw_sc2_ctr_pst_s *cfg;

	dim_print("%s:\n", __func__);

	if (!afbc_is_supported()	||
	    pafd_ctr->fb.ver < AFBCD_V5)
		return false;

	if (if0_vf->type & VIDTYPE_COMPRESS) {
		afbc_sw_pst(true); //ary need check
	} else {
		afbc_sw_pst(false);
		return false;
	}
	pcfg = &cfg_afbcd;
	memset(pcfg, 0, sizeof(*pcfg));

	cfg = di_afdp->top_cfg_pst;

	if (is_cfg(EAFBC_CFG_ETEST)) {
		if1_vf2 = if0_vf;
		if2_vf2 = if0_vf;
	} else {
		if1_vf2 = if1_vf;
		if2_vf2 = if2_vf;
	}
//	dim_print("%s:1 0x%x=0x%x\n", __func__,
		  //AFBCDM_MEM_HEAD_BADDR, reg_rd(AFBCDM_MEM_HEAD_BADDR));
	afbc_pst_check_chg_l(if0_vf, if1_vf2, if2_vf2, pafd_ctr);

//	dim_print("%s:0x%x=0x%x\n", __func__,
		  //AFBCDM_MEM_HEAD_BADDR, reg_rd(AFBCDM_MEM_HEAD_BADDR));

	if (pafd_ctr->en_set_pst.b.if1 &&
	    pafd_ctr->b.pst_chg_if1 == 3 && if1_vf2) {
		dim_print("%s:if1:chg 3\n", __func__);
	//	cfg->b.afbc_if1 = 1;
	//	reg_wrb(DI_TOP_POST_CTRL, 1, 4, 1);
		enable_afbc_input_local(if1_vf2, pafd_ctr->fb.if1_dec, pcfg);
		afbcd_reg_bwr(EAFBC_DEC_IF1, EAFBC_ENABLE, 1, 8, 1);
		pafd_ctr->b.en_pst_if1 = 1;
	} else if (pafd_ctr->en_set_pst.b.if1 &&
		   pafd_ctr->b.pst_chg_if1 == 1 && if1_vf2) {
		dim_print("%s:if1:chg 2\n", __func__);
		afbc_update_level1(if1_vf2, pafd_ctr->fb.if1_dec);
	} else if (!if1_vf2 || !pafd_ctr->en_set_pst.b.if1) {
		/* coverity[overrun-call] */
		afbcd_reg_bwr(EAFBC_DEC_IF1, EAFBC_ENABLE, 0, 8, 1);
		pafd_ctr->b.en_pst_if1 = 0;
	//	cfg->b.afbc_if1 = 0;
	//	reg_wrb(DI_TOP_POST_CTRL, 0, 4, 1);
		di_print("%s:%d\n", __func__, pafd_ctr->en_set_pst.b.if1);
	}

	if (pafd_ctr->en_set_pst.b.if2 &&
	    pafd_ctr->b.pst_chg_if2 == 3 && if2_vf2) {
		dim_print("%s:if2:chg 3\n", __func__);
	//	cfg->b.afbc_if2 = 1;
	//	reg_wrb(DI_TOP_POST_CTRL, 1, 6, 1);
		enable_afbc_input_local(if2_vf2, pafd_ctr->fb.if2_dec, pcfg);
		afbcd_reg_bwr(EAFBC_DEC_IF2, EAFBC_ENABLE, 1, 8, 1);
		pafd_ctr->b.en_pst_if2 = 1;

	} else if (pafd_ctr->en_set_pst.b.if2 &&
		   pafd_ctr->b.pst_chg_if2 == 1 && if2_vf2) {
		dim_print("%s:if2:chg 2\n", __func__);
		afbc_update_level1(if2_vf2, pafd_ctr->fb.if2_dec);
	} else if (!if2_vf2 || !pafd_ctr->en_set_pst.b.if2) {
		afbcd_reg_bwr(EAFBC_DEC_IF2, EAFBC_ENABLE, 0, 8, 1);
		pafd_ctr->b.en_pst_if2 = 0;
	//	cfg->b.afbc_if2 = 0;
	//	reg_wrb(DI_TOP_POST_CTRL, 0, 6, 1);
	}

	if ((pafd_ctr->b.pst_chg_level == 3 ||
	     pafd_ctr->b.pst_chg_if1 == 3)	&&
	     pafd_ctr->en_set_pst.b.if0) {
		dim_print("%s:if0:chg 3\n", __func__);
	//	cfg->b.afbc_if0 = 1;

	//	reg_wrb(DI_TOP_POST_CTRL, 1, 5, 1);
		enable_afbc_input_local(if0_vf, pafd_ctr->fb.if0_dec, pcfg);
		afbcd_reg_bwr(EAFBC_DEC_IF0, EAFBC_ENABLE, 1, 8, 1);

		if (pafd_ctr->en_set_pst.b.enc_wr) {
			/* double write */
			if (is_mask(SC2_DW_EN))
				reg_wrb(DI_TOP_POST_CTRL, 1, 0, 1);
				/*mif disable temp*/
			else
				reg_wrb(DI_TOP_POST_CTRL, 0, 0, 1);
				/*mif disable temp*/

	//		reg_wrb(DI_TOP_POST_CTRL, 1, 1, 1); /*afbce*/
			/*wr*/
			afbce_set(wr_vf, EAFBC_ENC1);
		}
	} else if (pafd_ctr->en_set_pst.b.if0) {
		dim_print("%s:if0:other\n", __func__);
		afbc_update_level1(if0_vf, pafd_ctr->fb.if0_dec);

		if (pafd_ctr->en_set_pst.b.enc_wr)
			/*wr*/
			afbce_update_level1(wr_vf, &di_normal_regset,
					    EAFBC_ENC1);

	} else {
		dim_print("%s:noif0,0x%x\n", __func__, pafd_ctr->en_set_pst.d8);
	}

	return 0;
}

/*************************************************/
/* only for tm2, sc2 is chang*/
static void afbc_tm2_sw_inp(bool on, const struct reg_acc *op)
{
	if (!op) {
		PR_ERR("%s:no op\n", __func__);
		return;
	}
	if (DIM_IS_IC(T5DB)) {
		//PR_INF("%s:sw:%d\n", __func__, on);
		if (on)
			op->bwr(VIUB_MISC_CTRL0, 1, 16, 1);
		else
			op->bwr(VIUB_MISC_CTRL0, 0, 16, 1);
		return;
	}
	if (on)
		op->bwr(DI_AFBCE_CTRL, 0x03, 10, 2);
	else
		op->bwr(DI_AFBCE_CTRL, 0x00, 10, 2);
}

/* only for tm2, sc2 is chang*/
static void afbc_tm2_sw_mem(bool on, const struct reg_acc *op)
{
	if (DIM_IS_IC(T5DB))
		return;
	if (!op) {
		PR_ERR("%s:no op\n", __func__);
		return;
	}
	if (on)
		op->bwr(DI_AFBCE_CTRL, 0x03, 12, 2);
	else
		op->bwr(DI_AFBCE_CTRL, 0x00, 12, 2);
}

/* only for tm2, sc2 is chang*/
static void afbce_tm2_sw(bool on, const struct reg_acc *op)
{
	if (!op) {
		PR_ERR("%s:no op\n", __func__);
		return;
	}
	if (on) {
		/*1: nr channel 0 to afbce*/
		op->bwr(DI_AFBCE_CTRL, 0x01, 0, 1);
		/* nr_en: important! 1:enable nr write to DDR; */
		op->bwr(DI_AFBCE_CTRL, 0x01, 4, 1);
	} else {
		op->bwr(DI_AFBCE_CTRL, 0x00, 0, 1);
		op->bwr(DI_AFBCE_CTRL, 0x01, 4, 1);
	}

	/*for t5 is afbce ram enable, must set 1 if use afbce*/
	if (DIM_IS_IC(T5)) {
		if (on)
			op->bwr(DI_AFBCE_CTRL, 0x01, 30, 1);
		else
			op->bwr(DI_AFBCE_CTRL, 0x00, 30, 1);
	}
}

static void afbcx_sw(bool on, const struct reg_acc *op)	/*g12a*/
{
	unsigned int tmp;
	unsigned int mask;
	unsigned int reg_ctrl, reg_en;
	enum EAFBC_DEC dec_sel;
	const unsigned int *reg = afbc_get_inp_base();
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!op) {
		PR_ERR("%s:no op\n", __func__);
		return;
	}

	dec_sel = pafd_ctr->fb.pre_dec;

	if (dec_sel == EAFBC_DEC0)
		reg_ctrl = VD1_AFBCD0_MISC_CTRL;
	else
		reg_ctrl = VD2_AFBCD1_MISC_CTRL;

	reg_en = reg[EAFBC_ENABLE];

	mask = (3 << 20) | (1 << 12) | (1 << 9);
	/*clear*/
	tmp = reg_rd(reg_ctrl) & (~mask);

	if (on) {
		tmp = tmp
			/*0:go_file 1:go_filed_pre*/
			| (2 << 20)
			/*0:afbc0 mif to axi 1:vd1 mif to axi*/
			| (1 << 12)
			/*0:afbc0 to vpp 1:afbc0 to di*/
			| (1 << 9);
		op->wr(reg_ctrl, tmp);
		/*0:vd1 to di	1:vd2 to di */
		op->bwr(VD2_AFBCD1_MISC_CTRL,
			(reg_ctrl == VD1_AFBCD0_MISC_CTRL) ? 0 : 1, 8, 1);
		/*reg_wr(reg_en, 0x1600);*/
		op->bwr(VIUB_MISC_CTRL0, 1, 16, 1);
		/*TL1 add mem control bit */
		//if (is_meson_tl1_cpu() || is_meson_tm2_cpu())
		if (pafd_ctr->fb.ver == AFBCD_V3)
			op->bwr(VD1_AFBCD0_MISC_CTRL, 1, 22, 1);
	} else {
		op->wr(reg_ctrl, tmp);
		op->wr(reg_en, 0x1600);
		op->bwr(VIUB_MISC_CTRL0, 0, 16, 1);
		//if (is_meson_tl1_cpu() || is_meson_tm2_cpu())
		if (pafd_ctr->fb.ver == AFBCD_V3)
			op->bwr(VD1_AFBCD0_MISC_CTRL, 0, 22, 1);
	}
}

static void afbc_sw_old(bool on, const struct reg_acc *op)/*txlx*/
{
	enum EAFBC_DEC dec_sel;
	unsigned int reg_en;
	const unsigned int *reg = afbc_get_inp_base();
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!op) {
		PR_ERR("%s:no op\n", __func__);
		return;
	}
	dec_sel = pafd_ctr->fb.pre_dec;
	reg_en = reg[EAFBC_ENABLE];

	if (on) {
		/* DI inp(current data) switch to AFBC */
		if (op->brd(VIU_MISC_CTRL0, 29, 1) != 1)
			op->bwr(VIU_MISC_CTRL0, 1, 29, 1);
		if (op->brd(VIUB_MISC_CTRL0, 16, 1) != 1)
			op->bwr(VIUB_MISC_CTRL0, 1, 16, 1);
		if (op->brd(VIU_MISC_CTRL1, 0, 1) != 1)
			op->bwr(VIU_MISC_CTRL1, 1, 0, 1);
		if (dec_sel == EAFBC_DEC0) {
			/*gxl only?*/
			if (op->brd(VIU_MISC_CTRL0, 19, 1) != 1)
				op->bwr(VIU_MISC_CTRL0, 1, 19, 1);
		}
		if (reg_rd(reg_en) != 0x1600)
			op->wr(reg_en, 0x1600);

	} else {
		op->wr(reg_en, 0);
		/* afbc to vpp(replace vd1) enable */
		if (op->brd(VIU_MISC_CTRL1, 0, 1)	!= 0 ||
		    op->brd(VIUB_MISC_CTRL0, 16, 1)	!= 0) {
			op->bwr(VIU_MISC_CTRL1, 0, 0, 1);
			op->bwr(VIUB_MISC_CTRL0, 0, 16, 1);
		}
	}
}

static bool afbc_is_used(void)
{
	bool ret = false;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (pafd_ctr->en_set.d8)
		ret = true;

	return ret;
}

static bool afbc_is_used_inp(void)
{
	bool ret = false;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (pafd_ctr->en_set.b.inp)
		ret = true;

	return ret;
}

static bool afbc_is_used_mem(void)
{
	bool ret = false;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (pafd_ctr->en_set.b.mem)
		ret = true;

	return ret;
}

static bool afbc_is_used_chan2(void)
{
	bool ret = false;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (pafd_ctr->en_set.b.chan2)
		ret = true;

	return ret;
}

static bool afbc_is_free(void)
{
	bool sts = 0;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	u32 afbc_num = pafd_ctr->fb.pre_dec;

	if (afbc_num == EAFBC_DEC0)
		sts = reg_rdb(VD1_AFBCD0_MISC_CTRL, 8, 2);
	else if (afbc_num == EAFBC_DEC1)
		sts = reg_rdb(VD2_AFBCD1_MISC_CTRL, 8, 2);

	if (sts)
		return true;
	else
		return false;

	return sts;
}

static void afbc_power_sw(bool on, const struct reg_acc *op)
{
	/*afbc*/
	enum EAFBC_DEC dec_sel;
	unsigned int vpu_sel;
	unsigned int reg_ctrl;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!op) {
		PR_ERR("%s:no op\n", __func__);
		return;
	}

	dec_sel = pafd_ctr->fb.pre_dec;
	if (dec_sel == EAFBC_DEC0)
		vpu_sel = VPU_AFBC_DEC;
	else
		vpu_sel = VPU_AFBC_DEC1;

	ops_ext()->switch_vpu_mem_pd_vmod
		(vpu_sel, on ? VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if (dec_sel == EAFBC_DEC0)
			reg_ctrl = VD1_AFBCD0_MISC_CTRL;
		else
			reg_ctrl = VD2_AFBCD1_MISC_CTRL;
		if (on)
			op->bwr(reg_ctrl, 0, 0, 8);
		else
			op->bwr(reg_ctrl, 0x55, 0, 8);
	}
		/*afbcx_power_sw(dec_sel, on);*/
}

//int afbc_reg_unreg_flag;

static void afbc_sw_tl2(bool en, const struct reg_acc *op_in)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (pafd_ctr->en_ponly_afbcd) { //only for t5dvb
		afbc_tm2_sw_inp(en, op_in);
		return;
	}
	if (pafd_ctr->fb.mode == AFBC_WK_IN) {
		afbc_tm2_sw_inp(en, op_in);
	} else if (pafd_ctr->fb.mode == AFBC_WK_P) {
	#ifdef MARK_SC2
		afbc_tm2_sw_inp(en);
		afbc_tm2_sw_mem(en);
		afbce_tm2_sw(en);
	#else
		if (!en) {
			afbc_tm2_sw_inp(en, op_in);
			afbc_tm2_sw_mem(en, op_in);
			afbce_tm2_sw(en, op_in);
		}
	#endif
	}
}

static void afbc_sw_sc2(bool en)
{
#ifdef MARK_SC2
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!pafd_ctr->fb.mode)
		PR_INF("%s:%d\n", __func__, en);
#endif
}

static void afbc_sw(bool on)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	bool act = false;

	/**/
	if (pafd_ctr->b.en && !on)
		act = true;
	else if (!pafd_ctr->b.en && on)
		act = true;

	if (act) {
		if (pafd_ctr->fb.ver == AFBCD_V1)
			afbc_sw_old(on, &di_normal_regset);
		else if (pafd_ctr->fb.ver <= AFBCD_V3)
			afbcx_sw(on, &di_normal_regset);
		else if (pafd_ctr->fb.ver == AFBCD_V4)
			afbc_sw_tl2(on, &di_normal_regset);
		else if (pafd_ctr->fb.ver == AFBCD_V5)
			afbc_sw_sc2(on);

		pafd_ctr->b.en = on;
		pr_info("di:%s:%d\n", __func__, on);
	}
}

void dbg_afbc_sw_v3(bool on)
{
	afbc_sw(on);
}

static void afbc_input_sw(bool on);

static void afbc_reg_variable(void *a)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	union afbc_blk_s	*en_cfg;
	struct di_ch_s *pch;
	unsigned char cfg_val;

	pch = (struct di_ch_s *)a;
	/*******************************
	 * cfg for debug:
	 ******************************/
	afbc_get_mode_from_cfg();

	en_cfg = &pch->rse_ori.di_pre_stru.en_cfg;

	afbc_cfg_mode_set(pafd_ctr->fb.mode, en_cfg);

	/******************************/
	if (pafd_ctr->fb.mode >= AFBC_WK_6D) {
		pafd_ctr->fb.mem_alloc	= 1;
		pafd_ctr->fb.mem_alloci	= 1;
	} else if (pafd_ctr->fb.mode >= AFBC_WK_P) {
		pafd_ctr->fb.mem_alloc = 1;
		pafd_ctr->fb.mem_alloci	= 0;
	} else if (is_cfg(EAFBC_CFG_MEM)) {
		pafd_ctr->fb.mem_alloc = 1;
		pafd_ctr->fb.mem_alloci	= 1;
	} else {
		pafd_ctr->fb.mem_alloc	= 0;
		pafd_ctr->fb.mem_alloci	= 0;
	}

	dim_print("%s:en_cfg:0x%x,mode[%d]\n", __func__, en_cfg->d8, pafd_ctr->fb.mode);
	/**********************************/
	en_cfg->d8 = en_cfg->d8 & pafd_ctr->fb.sp.d8;
	cfg_val = en_cfg->d8;
	/* post save as pre */
	en_cfg = &pch->rse_ori.di_post_stru.en_cfg;
	en_cfg->d8 = cfg_val;
}

static void afbc_reg_variable_prelink(void *a) //tmp
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	union afbc_blk_s	*en_cfg;
	struct dimn_itf_s *itf;
	//unsigned char cfg_val;

	itf = (struct dimn_itf_s *)a;
	/*******************************
	 * cfg for debug:
	 ******************************/
	afbc_get_mode_from_cfg();

	en_cfg = &itf->ds->afbc_en_cfg;

	afbc_cfg_mode_set(pafd_ctr->fb.mode, en_cfg);

	/******************************/
	if (pafd_ctr->fb.mode >= AFBC_WK_6D) {
		pafd_ctr->fb.mem_alloc	= 1;
		pafd_ctr->fb.mem_alloci	= 1;
	} else if (pafd_ctr->fb.mode >= AFBC_WK_P) {
		pafd_ctr->fb.mem_alloc = 1;
		pafd_ctr->fb.mem_alloci	= 0;
	} else if (is_cfg(EAFBC_CFG_MEM)) {
		pafd_ctr->fb.mem_alloc = 1;
		pafd_ctr->fb.mem_alloci	= 1;
	} else {
		pafd_ctr->fb.mem_alloc	= 0;
		pafd_ctr->fb.mem_alloci	= 0;
	}

	PR_INF("%s:en_cfg:0x%x,mode[%d]\n", __func__, en_cfg->d8, pafd_ctr->fb.mode);
	/**********************************/
	en_cfg->d8 = en_cfg->d8 & pafd_ctr->fb.sp.d8;
	//cfg_val = en_cfg->d8;
	/* post save as pre */
	//en_cfg = &pch->rse_ori.di_post_stru.en_cfg;
	//en_cfg->d8 = cfg_val;
}

static void afbc_val_reset_newformat(void)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	//PR_INF("%s\n", __func__);
	pafd_ctr->l_vtype = 0;
	pafd_ctr->l_vt_mem = 0;
	pafd_ctr->l_vt_chan2 = 0;
	pafd_ctr->l_h = 0;
	pafd_ctr->l_w = 0;
	pafd_ctr->pst_in_vtp = 0;
	pafd_ctr->pst_in_h = 0;
	pafd_ctr->pst_in_w = 0;
}

void disable_afbcd_t5dvb(void)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	const unsigned int *reg;
	unsigned int reg_AFBC_ENABLE;

	if (!afbc_is_supported() || !DIM_IS_IC(T5DB) || cfgg(EN_PRE_LINK))
		return;
	dbg_mem2("%s\n", __func__);
	reg = afbc_get_addrp(pafd_ctr->fb.pre_dec);
	reg_AFBC_ENABLE = reg[EAFBC_ENABLE];

	dim_print("%s:reg=0x%x:sw=off\n", __func__, reg_AFBC_ENABLE);
	reg_wrb(reg_AFBC_ENABLE, 0, 8, 1);
}

void afbcd_enable_only_t5dvb(const struct reg_acc *op, bool vpp_link)
{
	unsigned int val;
	bool en = false;

	if (!DIM_IS_IC(T5DB))
		return;
	if (vpp_link && afbc_is_supported_for_plink())
		en = true;
	else if (!vpp_link && afbc_is_supported())
		en = true;

	if (en) {
		PR_INF("t5dvb afbcd on\n");
		/* afbcd is shared */
		val = op->rd(VD1_AFBCD0_MISC_CTRL);
		val |= (DI_BIT1 | DI_BIT10 | DI_BIT12 | DI_BIT22);
		op->wr(VD1_AFBCD0_MISC_CTRL, val);
#ifdef HIS_CODE
		op->bwr(VD1_AFBCD0_MISC_CTRL, 0x01, 22, 1);
		op->bwr(VD1_AFBCD0_MISC_CTRL, 0x01, 10, 1);
		op->bwr(VD1_AFBCD0_MISC_CTRL, 0x01, 12, 1);
		op->bwr(VD1_AFBCD0_MISC_CTRL, 0x01, 1, 1);
#endif
		dbg_reg("%s:t5d vb on\n 0x%x,0x%x\n",
			__func__,
			VD1_AFBCD0_MISC_CTRL,
			reg_rd(VD1_AFBCD0_MISC_CTRL));
	}
}

static void afbc_reg_sw(bool on)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	struct afbc_fb_s tmp;

	if (!afbc_is_supported())
		return;
	dbg_pl("%s:sw[%d]\n", __func__, on);
	if (on) {
		if (pafd_ctr->fb.ver <= AFBCD_V3)
			afbc_power_sw(true, &di_normal_regset);
		else if (pafd_ctr->fb.ver == AFBCD_V4) {
			reg_wrb(DI_AFBCE_CTRL, 0x01, 4, 1);
#ifdef _HIS_CODE_ //move to afbcd_enable_only_t5dvb
			if (DIM_IS_IC(T5DB) && afbc_is_supported()) {
				/* afbcd is shared */
				reg_wrb(VD1_AFBCD0_MISC_CTRL, 0x01, 22, 1);
				reg_wrb(VD1_AFBCD0_MISC_CTRL, 0x01, 10, 1);
				reg_wrb(VD1_AFBCD0_MISC_CTRL, 0x01, 12, 1);
				reg_wrb(VD1_AFBCD0_MISC_CTRL, 0x01, 1, 1);
				dbg_reg("%s:t5d vb on\n 0x%x,0x%x\n",
					__func__,
					VD1_AFBCD0_MISC_CTRL,
					reg_rd(VD1_AFBCD0_MISC_CTRL));
			}
#endif
		}
	}
	if (!on) {
		if (pafd_ctr->fb.ver <= AFBCD_V3) {
			/*input*/
			afbc_input_sw(false);

			afbc_sw(false);

			afbc_power_sw(false, &di_normal_regset);
		} else {
			/*AFBCD_V4*/
			afbc_sw(false);
		}
		memcpy(&tmp, &pafd_ctr->fb, sizeof(tmp));
		memset(pafd_ctr, 0, sizeof(*pafd_ctr));
		memcpy(&pafd_ctr->fb, &tmp, sizeof(tmp));
	}
}

/*********************************************
 * size v2 2020-08-24
 ********************************************/
static unsigned int afbc_count_info_size(unsigned int w, unsigned int h,
					 unsigned int *blk_total)
{
	unsigned int length = 0;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	unsigned int blk_num_total, blk_hsize, blk_vsize;
	unsigned int blk_hsize_align, blk_vsize_align;

	if (!afbc_is_supported() ||
	    ((pafd_ctr->fb.mode < AFBC_WK_P) && (!cfgg(FIX_BUF)))) {
		pafd_ctr->blk_nub_total	= 0;
		pafd_ctr->size_info	= 0;
		return 0;
	}
	blk_hsize  = ((w + 31) >> 5);//hsize 32 ?? ??32
	blk_vsize  = ((h + 3) >> 2);//size  4  ?? ??4

	blk_hsize_align   = ((blk_hsize + 1) >> 1) << 1;//2??
	blk_vsize_align   = ((blk_vsize + 15) >> 4) << 4;//8??
	blk_num_total = blk_hsize_align * blk_vsize_align;

	length	= blk_num_total * 4;
	length	= PAGE_ALIGN(length); //ary add 2020-08-26
	pafd_ctr->blk_nub_total = blk_num_total;
	*blk_total = blk_num_total;
	pafd_ctr->size_info = length;
	return length;
}

static unsigned int v2_afbc_count_buffer_size(unsigned int format,
					      unsigned int blk_nub_total)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	unsigned int sblk_num, src_bits, each_blk_bytes, body_buffer_size;
	unsigned int fmt_mode, compbits;

	if (!afbc_is_supported() ||
	    ((pafd_ctr->fb.mode < AFBC_WK_P) && (!cfgg(FIX_BUF)))) {
		pafd_ctr->size_afbc_buf	= 0;
		return 0;
	}

	fmt_mode = format & 0x0f;
	compbits = (format >> 4) & 0x0f;
	if (!fmt_mode) /*4:2:0*/
		sblk_num = 12;
	else if (fmt_mode == 1)	/*4:2:2*/
		sblk_num = 16;
	else			/*4:4:4*/
		sblk_num = 24;

	if (compbits == 0)
		src_bits = 8;
	else if (compbits == 1)
		src_bits = 9;
	else
		src_bits = 10;

	each_blk_bytes = ((((sblk_num * 16 * src_bits + 7) >> 3) +
			   63) >> 6) << 6;
	body_buffer_size = ((blk_nub_total * each_blk_bytes) * 113 + 99) / 100;
	dbg_reg("%s:size=0x%x\n", __func__, body_buffer_size);
	pafd_ctr->size_afbc_buf = body_buffer_size;

	return body_buffer_size;
}

static unsigned int afbc_count_tab_size(unsigned int buf_size)
{
	unsigned int length = 0;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	/* tmp: large */
	if (!afbc_is_supported() ||
	    ((pafd_ctr->fb.mode < AFBC_WK_P) && (!cfgg(FIX_BUF))))
		return 0;

	length = PAGE_ALIGN(((buf_size + 0xfff) >> 12) *
			    sizeof(unsigned int));

	pafd_ctr->size_tab = length;
	return length;
}

#ifdef HIST_CODE
static unsigned int afbc_count_info_size(unsigned int w, unsigned int h)
{
	unsigned int length = 0;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if ((!afbc_is_supported()) ||
	    ((pafd_ctr->fb.mode < AFBC_WK_P) && (!cfgg(FIX_BUF))))
		return 0;

	length = PAGE_ALIGN((roundup(w * 2, 32) *
			    roundup(h, 4)) / 32);//tmplarge

	pafd_ctr->size_info = length;
	return length;
}

static unsigned int afbc_count_tab_size(unsigned int buf_size)
{
	unsigned int length = 0;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	/* tmp: large */
	if ((!afbc_is_supported()) ||
	    ((pafd_ctr->fb.mode < AFBC_WK_P) && (!cfgg(FIX_BUF))))
		return 0;

	length = PAGE_ALIGN(((buf_size * 2 + 0xfff) >> 12) *
			    sizeof(unsigned int));

	pafd_ctr->size_tab = length;
	return length;
}
#endif

static unsigned int afbc_int_tab(struct device *dev,
				 struct afbce_map_s *pcfg)
{
	bool flg = 0;
	unsigned int *p;
	unsigned int i, cnt, last;
	unsigned int body;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	unsigned int crc = 0;
	unsigned long crc_tmp = 0;
	//struct div2_mm_s *mm = dim_mm_get(ch);
	//struct di_dev_s *de_devp = get_dim_de_devp();

	if (!afbc_is_supported()	||
	    !pcfg			||
	    !dev)
		return 0;
	if (pafd_ctr->fb.mode < AFBC_WK_P && (!cfgg(FIX_BUF)))
		return 0;

	dma_sync_single_for_device(dev,
				   pcfg->tabadd,
				   pcfg->size_tab,
				   DMA_TO_DEVICE);

	p = (unsigned int *)dim_vmap(pcfg->tabadd, pcfg->size_tab, &flg);
	if (!p) {
		pafd_ctr->b.enc_err = 1;
		pr_error("%s:vmap:0x%lx\n", __func__, pcfg->tabadd);
		return 0;
	}

	cnt = (pcfg->size_buf + 0xfff) >> 12;
	body = (unsigned int)(pcfg->bodyadd >> 12);
	for (i = 0; i < cnt; i++) {
		*(p + i) = body;
		crc_tmp += body;
		body++;
	}

	crc = (unsigned int)crc_tmp;
	last = pcfg->size_tab - (cnt * sizeof(unsigned int));

	memset((p + cnt), 0, last);
	//memset((p + cnt), body, last);

	/*debug*/
	#ifdef DBG_AFBC
	di_pr_info("%s:tab:[0x%lx]: body[0x%lx];cnt[%d];last[%d]\n",
		   __func__,
		   pcfg->tabadd, pcfg->bodyadd, cnt, last);
	#endif
	dma_sync_single_for_device(dev,
				   pcfg->tabadd,
				   pcfg->size_tab,
				   DMA_TO_DEVICE);
	//PR_INF("%s:%d\n", __func__, flg);
	if (flg)
		di_unmap_phyaddr((u8 *)p);

	return crc;
}

static unsigned int afbc_tab_cnt_crc(struct device *dev,
				     struct afbce_map_s *pcfg,
				     unsigned int check_mode,
				     unsigned int crc_old)
{
	bool flg = 0;
	unsigned int *p;
	int i, cnt;
	unsigned int body;
	unsigned int crc = 0;
	unsigned long crc_tmp = 0;
	unsigned int err_cnt = 0;
	unsigned int fist_err_pos = 0, fist_err = 0, first_right = 0;
	unsigned int fist_s_pos = 0, right_cnt = 0;

	//p = (unsigned int *)dim_vmap(pcfg->tabadd, pcfg->size_buf, &flg);
	dma_sync_single_for_cpu(dev,
				pcfg->tabadd,
				pcfg->size_tab,
				DMA_FROM_DEVICE);
	p = (unsigned int *)dim_vmap(pcfg->tabadd, pcfg->size_tab, &flg);
	if (!p) {
		pr_error("%s:vmap:0x%lx\n", __func__, pcfg->tabadd);
		return 0;
	}
	if (!pcfg->size_tab)
		return 0;
	cnt = (pcfg->size_buf + 0xfff) >> 12;
	body = (unsigned int)(pcfg->bodyadd >> 12);
	for (i = 0; i < cnt; i++) {
		if (check_mode == 1) {
			if (*(p + i) != body) {
				if (err_cnt == 0) {
					fist_err_pos = i;
					fist_err = *(p + i);
					first_right = body;
				}

				err_cnt++;
			} else {
				right_cnt++;
				if (right_cnt > 3)
					fist_s_pos = i;
			}
		}
		crc_tmp += *(p + i);

		body++;
	}
	crc = (unsigned int)crc_tmp;
	/*debug*/
	if (crc_old != crc || err_cnt > 0) {
		PR_ERR("%s:crc[0x%x], err[%d]\n",
		       __func__,
		       crc, err_cnt);
		PR_ERR("pos[%d],er1[0x%x],[0x%x],rightpos[%d]\n",
		       fist_err_pos, fist_err, first_right, fist_s_pos);
	}
	//dma_sync_single_for_device(dev,
	//			   pcfg->tabadd,
	//			   pcfg->size_tab,
	//			   DMA_TO_DEVICE);
	//PR_INF("%s:%d\n", __func__, flg);
	if (flg)
		di_unmap_phyaddr((u8 *)p);

	return crc;
}

bool dbg_checkcrc(struct di_buf_s *di_buf, unsigned int cnt)
{
	struct di_dev_s *devp = get_dim_de_devp();
	struct afbce_map_s pcfg;
	unsigned int crc;

	pcfg.bodyadd	= di_buf->nr_adr;
	pcfg.tabadd	= di_buf->afbct_adr;

	pcfg.size_buf	= di_buf->nr_size;
	pcfg.size_tab	= di_buf->tab_size;

	if (!pcfg.size_tab && !di_buf->afbc_crc)
		return true;
	else if (!pcfg.size_tab)
		return false;

	crc = afbc_tab_cnt_crc(&devp->pdev->dev, &pcfg, 1, di_buf->afbc_crc);

	if (crc != di_buf->afbc_crc) {
		PR_ERR("%s:cnt[%d],0x%x->0x%x\n",
		       __func__, cnt, crc, di_buf->afbc_crc);
		PR_INF("\t:bodyadd[0x%lx],tabadd[0x%lx]\n",
		       pcfg.bodyadd, pcfg.tabadd);
		PR_INF("\t:sizebuf[0x%x], sizetab[0x%x]\n",
		       pcfg.size_buf, pcfg.size_tab);

		return false;
	}
	return true;
}

static void afbc_input_sw(bool on)
{
	const unsigned int *reg;// = afbc_get_regbase();
	unsigned int reg_AFBC_ENABLE;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	if (!afbc_is_supported())
		return;

	if (pafd_ctr->en_set.b.inp) {
		reg = afbc_get_addrp(pafd_ctr->fb.pre_dec);
		reg_AFBC_ENABLE = reg[EAFBC_ENABLE];

		dim_print("%s:reg=0x%x:sw=%d\n", __func__, reg_AFBC_ENABLE, on);
		if (on)
			reg_wrb(reg_AFBC_ENABLE, 1, 8, 1);
		else
			;//reg_wrb(reg_AFBC_ENABLE, 0, 8, 1);
	}

	if (pafd_ctr->en_set.b.mem) {
		/*mem*/
		reg = afbc_get_addrp(pafd_ctr->fb.mem_dec);
		reg_AFBC_ENABLE = reg[EAFBC_ENABLE];
		if (on)
			reg_wrb(reg_AFBC_ENABLE, 1, 8, 1);
		else
			reg_wrb(reg_AFBC_ENABLE, 0, 8, 1);
	}

	if (pafd_ctr->en_set.b.chan2) {
		/* chan2 */
		reg = afbc_get_addrp(pafd_ctr->fb.ch2_dec);
		reg_AFBC_ENABLE = reg[EAFBC_ENABLE];
		if (on)
			reg_wrb(reg_AFBC_ENABLE, 1, 8, 1);
		else
			reg_wrb(reg_AFBC_ENABLE, 0, 8, 1);
	}
}

void dbg_afd_reg_v3(struct seq_file *s, enum EAFBC_DEC eidx)
{
	int i;
	unsigned int addr;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	unsigned int regs_ofst;

	seq_printf(s, "dump reg:afd[%d]\n", eidx);

	if (pafd_ctr->fb.ver < AFBCD_V5) {
		if (eidx > EAFBC_DEC3_MEM) {
			seq_printf(s, "eidx[%d] is overflow for ver[%d]\n",
				   eidx, pafd_ctr->fb.ver);
			return;
		}
		for (i = 0; i < AFBC_REG_INDEX_NUB; i++) {
			addr = reg_afbc[eidx][i];
			seq_printf(s, "reg[0x%x]=0x%x.\n", addr, reg_rd(addr));
		}
	} else {
		for (i = 0; i < AFBC_REG_INDEX_NUB; i++) {
			addr = reg_afbc_v5[eidx][i];
			seq_printf(s, "reg[0x%x]=0x%x.\n", addr, reg_rd(addr));
		}

		if (pafd_ctr->fb.ver >= AFBCD_V5) {
			regs_ofst = afbcd_v5_get_offset(eidx);

			addr = regs_ofst + AFBCDM_ROT_CTRL;
			seq_printf(s, "reg[0x%x]=0x%x.\n", addr, reg_rd(addr));

			addr = regs_ofst + AFBCDM_ROT_SCOPE;
			seq_printf(s, "reg[0x%x]=0x%x.\n", addr, reg_rd(addr));
		}
	}
}
EXPORT_SYMBOL(dbg_afd_reg_v3);

void dbg_afe_reg_v3(struct seq_file *s, enum EAFBC_ENC eidx)
{
	int i;
	unsigned int addr;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	seq_printf(s, "dump reg:afe[%d]\n", eidx);

	if (pafd_ctr->fb.ver < AFBCD_V5 && eidx > 0) {
		PR_ERR("%s:eidx[%d] overflow\n", __func__, eidx);
		return;
	}

	for (i = 0; i < DIM_AFBCE_NUB; i++) {
		addr = reg_afbc_e[eidx][i];
		seq_printf(s, "reg[0x%x]=0x%x.\n", addr, reg_rd(addr));
	}
}
EXPORT_SYMBOL(dbg_afe_reg_v3);
/******************************************/
//for pre-vpp link
/* copy from is_src_real_i */
static bool is_src_real_i_dvfm(struct dvfm_s *vf)
{
	bool ret = false;
#ifdef HIS_CODE		//tmp
	struct di_buf_s *di_buf;

	if (!vf->private_data)
		return false;

	di_buf = (struct di_buf_s *)vf->private_data;
	if (di_buf->afbc_info & DI_BIT1)
		ret = true;
#endif
	return ret;
}

/* copy from is_src_i */
static bool is_src_i_dvfm(struct dvfm_s *vf)
{
	bool ret = false;
#ifdef HIS_CODE	//tmp
	struct di_buf_s *di_buf;

	if (!vf->private_data)
		return false;

	di_buf = (struct di_buf_s *)vf->private_data;
	if (di_buf->afbc_info & DI_BIT0)
		ret = true;
#endif
	return ret;
}

static bool src_i_set_dvfm(struct dvfm_s *vf)
{
	bool ret = false;
#ifdef HIS_CODE
	struct di_buf_s *di_buf;

	if (!vf->private_data) {
#ifdef PRINT_BASIC
		PR_ERR("%s:novf\n", __func__);
#endif
		return false;
	}

	di_buf = (struct di_buf_s *)vf->private_data;
	di_buf->afbc_info |= DI_BIT0;

	ret = true;
#endif /* HIS_CODE */
	return ret;
}

/* copy from afbc_sw */
static void afbc_sw_op(bool on, const struct reg_acc *op_in)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	bool act = false;

	/**/
	if (pafd_ctr->b.en && !on)
		act = true;
	else if (!pafd_ctr->b.en && on)
		act = true;

	if (act) {
		if (pafd_ctr->fb.ver == AFBCD_V1)
			afbc_sw_old(on, op_in);
		else if (pafd_ctr->fb.ver <= AFBCD_V3)
			afbcx_sw(on, op_in);
		else if (pafd_ctr->fb.ver == AFBCD_V4)
			afbc_sw_tl2(on, op_in);
		else if (pafd_ctr->fb.ver == AFBCD_V5)
			afbc_sw_sc2(on);

		pafd_ctr->b.en = on;
		PR_INF("%s:%d:ver[%d]\n", __func__, on, pafd_ctr->fb.ver);
	}
}

/*copy from afbc_input_sw */
static void afbc_input_sw_op(bool on, const struct reg_acc *op)
{
	const unsigned int *reg;// = afbc_get_regbase();
	unsigned int reg_AFBC_ENABLE;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	u32 reg_val;

	if (!afbc_is_supported_for_plink())
		return;

	if (pafd_ctr->en_set.b.inp) {
		reg = afbc_get_addrp(pafd_ctr->fb.pre_dec);
		reg_AFBC_ENABLE = reg[EAFBC_ENABLE];

		dim_print("%s:reg=0x%x:sw=%d\n", __func__, reg_AFBC_ENABLE, on);
		if (on) {
			reg_val = op->rd(reg_AFBC_ENABLE);
			reg_val |= 1 << 8;
			op->wr(reg_AFBC_ENABLE, reg_val);
		} else {
			;//reg_wrb(reg_AFBC_ENABLE, 0, 8, 1);
		}
	}

	if (pafd_ctr->en_set.b.mem) {
		/*mem*/
		reg = afbc_get_addrp(pafd_ctr->fb.mem_dec);
		reg_AFBC_ENABLE = reg[EAFBC_ENABLE];
		reg_val = op->rd(reg_AFBC_ENABLE);
		if (on)
			reg_val |= 1 << 8;
		else
			reg_val &= ~(1 << 8);
		op->wr(reg_AFBC_ENABLE, reg_val);
	}

	if (pafd_ctr->en_set.b.chan2) {
		/* chan2 */
		reg = afbc_get_addrp(pafd_ctr->fb.ch2_dec);
		reg_AFBC_ENABLE = reg[EAFBC_ENABLE];
		reg_val = op->rd(reg_AFBC_ENABLE);
		if (on)
			reg_val |= 1 << 8;
		else
			reg_val &= ~(1 << 8);
		op->wr(reg_AFBC_ENABLE, reg_val);
	}
}

/* copy afbc_reg_sw */
static void afbc_reg_sw_op(bool on, const struct reg_acc *op)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	struct afbc_fb_s tmp;

	if (!afbc_is_supported_for_plink())
		return;
	if (!op) {
		PR_ERR("%s:no op\n", __func__);
		return;
	}
	dbg_pl("%s:sw[%d]\n", __func__, on);
	if (on) {
		if (pafd_ctr->fb.ver <= AFBCD_V3) {
			afbc_power_sw(true, op);
		} else if (pafd_ctr->fb.ver == AFBCD_V4) {
			op->bwr(DI_AFBCE_CTRL, 0x01, 4, 1);
#ifdef _HIS_CODE_ //move to afbcd_enable_only_t5dvb
			if (DIM_IS_IC(T5DB) && afbc_is_supported()) {
				/* afbcd is shared */
				op->bwr(VD1_AFBCD0_MISC_CTRL, 0x01, 22, 1);
				op->bwr(VD1_AFBCD0_MISC_CTRL, 0x01, 10, 1);
				op->bwr(VD1_AFBCD0_MISC_CTRL, 0x01, 12, 1);
				op->bwr(VD1_AFBCD0_MISC_CTRL, 0x01, 1, 1);
				dbg_reg("%s:t5d vb on\n 0x%x,0x%x\n",
					__func__,
					VD1_AFBCD0_MISC_CTRL,
					op->rd(VD1_AFBCD0_MISC_CTRL));
			}
#endif
		}
	}
	if (!on) {
		if (pafd_ctr->fb.ver <= AFBCD_V3) {
			/*input*/
			afbc_input_sw_op(false, op);

			afbc_sw_op(false, op);

			afbc_power_sw(false, op);
		} else {
			/*AFBCD_V4*/
			afbc_sw_op(false, op);
		}
		memcpy(&tmp, &pafd_ctr->fb, sizeof(tmp));
		memset(pafd_ctr, 0, sizeof(*pafd_ctr));
		memcpy(&pafd_ctr->fb, &tmp, sizeof(tmp));
	}
}

static void tl1_set_afbcd_crop(struct dvfm_s *vf,
				   const unsigned int *reg,
				   struct di_win_s *win,
				   const struct reg_acc *op)
{
	int crop_left, crop_top;
	int vsize_in, hsize_in;
	int mif_blk_bgn_h, mif_blk_end_h;
	int mif_blk_bgn_v, mif_blk_end_v;
	int pix_bgn_h, pix_end_h;
	int pix_bgn_v, pix_end_v;
	//struct hw_afbc_reg_s *vd_afbc_reg;

	//vd_afbc_reg = setting->p_vd_afbc_reg;
	/* afbc horizontal setting */
	crop_left = win->x_st;	//setting->start_x_lines;
	hsize_in = round_up((vf->src_w - 1) + 1, 32);
	mif_blk_bgn_h = crop_left / 32;
	mif_blk_end_h = (crop_left + win->x_end -
		win->x_st) / 32;
	pix_bgn_h = crop_left - mif_blk_bgn_h * 32;
	pix_end_h = pix_bgn_h + win->x_end -
		win->x_st;

	op->wr(reg[EAFBC_MIF_HOR_SCOPE],
		(mif_blk_bgn_h << 16) |
		mif_blk_end_h);
	op->wr(reg[EAFBC_PIXEL_HOR_SCOPE],
		((pix_bgn_h << 16) |
		pix_end_h));

	/* afbc vertical setting */
	crop_top = win->y_st;
	vsize_in = round_up((vf->src_h - 1) + 1, 4);
	mif_blk_bgn_v = crop_top / 4;
	mif_blk_end_v = (crop_top + win->y_end -
		win->y_st) / 4;
	pix_bgn_v = crop_top - mif_blk_bgn_v * 4;
	pix_end_v = pix_bgn_v + win->y_end -
		win->y_st;

	op->wr(reg[EAFBC_MIF_VER_SCOPE],
		(mif_blk_bgn_v << 16) |
		mif_blk_end_v);

	op->wr(reg[EAFBC_PIXEL_VER_SCOPE],
		(pix_bgn_v << 16) |
		pix_end_v);

	op->wr(reg[EAFBC_SIZE_IN],
		(hsize_in << 16) |
		(vsize_in & 0xffff));
#ifdef DBG_AFBCD_SET
	PR_INF("%s:debug:\n", __func__);
	dim_print_win(win, "afbcd");
	PR_INF("\t:%s,<%d,%d>\n", "crop", crop_left, crop_top);
	PR_INF("\t:%s,<%d,%d>\n", "in", hsize_in, vsize_in);
	PR_INF("\t:%s,<%d,%d,%d,%d>\n", "h crop",
		mif_blk_bgn_h, mif_blk_end_h,
		pix_bgn_h, pix_end_h);
	PR_INF("\t:%s,<%d,%d,%d,%d>\n", "v crop",
		mif_blk_bgn_v, mif_blk_end_v,
		pix_bgn_v, pix_end_v);
#endif
}

/* copy from afbc_check_chg_level*/
static void afbc_check_chg_level_dvfm(struct dvfm_s *vf,
				 struct dvfm_s *mem_vf,
				 struct dvfm_s *chan2_vf,
				 struct afbcd_ctr_s *pctr,
				 const struct reg_acc *op_in)
{
	//tmp struct di_buf_s *di_buf = NULL;

	/*check support*/
	if (pctr->fb.ver == AFBCD_NONE)
		return;
#ifdef HIS_CODE
	if (is_cfg(EAFBC_CFG_DISABLE))
		return;
#endif
	if (pctr->fb.ver < AFBCD_V4 || pctr->fb.mode < AFBC_WK_P) {
		if (!(vf->vfs.type & VIDTYPE_COMPRESS)) {
			if (pctr->b.en) {
				/*from en to disable*/
				pctr->b.en = 0;
			}
			//dim_print("%s:not compress\n", __func__);
			return;
		}
	}
	/* patch for not mask nv21 */
	if ((vf->vfs.type & AFBC_VTYPE_MASK_CHG) !=
	    (pctr->l_vtype & AFBC_VTYPE_MASK_CHG)	||
	    vf->vfs.height != pctr->l_h		||
	    vf->vfs.width != pctr->l_w		||
	    vf->vfs.bitdepth != pctr->l_bitdepth) {
		pctr->b.chg_level = 3;
		pctr->l_vtype = (vf->vfs.type & AFBC_VTYPE_MASK_SAV);
		pctr->l_h = vf->vfs.height;
		pctr->l_w = vf->vfs.width;
		pctr->l_bitdepth = vf->vfs.bitdepth;
	} else {
		if (vf->vfs.type & VIDTYPE_INTERLACE) {
			pctr->b.chg_level = 2;
			pctr->l_vtype = (vf->vfs.type & AFBC_VTYPE_MASK_SAV);
		} else {
			pctr->b.chg_level = 1;
		}
	}
	if (is_cfg(EAFBC_CFG_LEVE3))
		pctr->b.chg_level = 3;
	pctr->addr_h = vf->vfs.compHeadAddr;
	pctr->addr_b = vf->vfs.compBodyAddr;
#ifdef MARK_SC2
	di_print("%s:\n", __func__);
	di_print("\tvf:type[0x%x],[0x%x]\n",
		 vf->vfs.type,
		 AFBC_VTYPE_MASK_SAV);
	di_print("\t\t:body[0x%x] inf[0x%x]\n",
		 vf->vfs.compBodyAddr, vf->vfs.compHeadAddr);
	di_print("\tinfo:type[0x%x]\n", pctr->l_vtype);

	di_print("\t\th[%d],w[%d],b[0x%x]\n", pctr->l_h,
		 pctr->l_w, pctr->l_bitdepth);
	di_print("\t\tchg_level[%d],is_srci[%d]\n",
		 pctr->b.chg_level, is_src_i_dvfm(vf));
#endif
	if (!pctr->fb.mode || !mem_vf) {
		pctr->b.chg_mem = 0;
		pctr->l_vt_mem = 0;
		di_print("\t not check mem");
		return;
	}
	/* mem */
	if (pctr->fb.mode >= AFBC_WK_P) {
		if ((mem_vf->vfs.type & AFBC_VTYPE_MASK_CHG) !=
		    (pctr->l_vt_mem & AFBC_VTYPE_MASK_CHG)) {
			pctr->b.chg_mem = 3;
			pctr->l_vt_mem = (mem_vf->vfs.type & AFBC_VTYPE_MASK_SAV);
		} else {
			pctr->b.chg_mem = 1;
		}
		#ifdef HIS_CODE  //tmp
		di_buf = (struct di_buf_s *)mem_vf->private_data;
		if (di_buf && di_buf->type == VFRAME_TYPE_LOCAL) {
			#ifdef AFBC_MODE1
			di_print("buf t[%d]:nr_adr[0x%lx], afbc_adr[0x%lx]\n",
				 di_buf->index,
				 di_buf->nr_adr,
				 di_buf->afbc_adr);
			#endif
			//mem_vf->compBodyAddr = di_buf->nr_adr;
			//mem_vf->compHeadAddr = di_buf->afbc_adr;
		}
		#endif /* HIS_CODE*/
	}

#ifdef MARK_SC2
	di_print("\tmem:type[0x%x]\n", mem_vf->vfs.type);
	di_print("\t\t:body[0x%x] inf[0x%x]\n",
		 mem_vf->vfs.compBodyAddr, mem_vf->vfs.compHeadAddr);
	di_print("\t\th[%d],w[%d],b[0x%x]\n", mem_vf->vfs.height,
		 mem_vf->vfs.width, mem_vf->vfs.bitdepth);

	di_print("\t\tchg_mem[%d],is_srci[%d]\n",
		 pctr->b.chg_mem, is_src_i_dvfm(vf));
#endif
	/* chan2 */
	if (!chan2_vf)
		return;
	if (pctr->en_set.b.chan2) {
		if ((chan2_vf->vfs.type & AFBC_VTYPE_MASK_CHG) !=
		    (pctr->l_vt_chan2 & AFBC_VTYPE_MASK_CHG)) {
			pctr->b.chg_chan2 = 3;
			pctr->l_vt_chan2 =
				(chan2_vf->vfs.type & AFBC_VTYPE_MASK_SAV);
		} else {
			pctr->b.chg_chan2 = 1;
		}
	}
#ifdef MARK_SC2
	di_print("\tchan2:type[0x%x]\n", chan2_vf->vfs.type);
	di_print("\t\t:body[0x%x] inf[0x%x]\n",
		 chan2_vf->vfs.compBodyAddr, chan2_vf->vfs.compHeadAddr);
	di_print("\t\th[%d],w[%d],b[0x%x]\n", chan2_vf->vfs.height,
		 chan2_vf->vfs.width, chan2_vf->vfs.bitdepth);

	di_print("\t\tchg[%d],is_srci[%d]\n",
		 pctr->b.chg_chan2, is_src_i_dvfm(vf));
#endif
}

/* copy from enable_afbc_input_local */
static u32 enable_afbc_input_local_dvfm(struct dim_prevpp_ds_s *ds,
				   struct dvfm_s *vf, enum EAFBC_DEC dec,
				   struct AFBCD_CFG_S *cfg,
				   struct di_win_s *win,
				   const struct reg_acc *op)
{
	unsigned int r, u, v, w_aligned, h_aligned;
	const unsigned int *reg = afbc_get_addrp(dec);
	unsigned int vfmt_rpt_first = 1, vt_ini_phase = 0;
	unsigned int out_height = 0;
	/*ary add*/
	unsigned int cvfmt_en = 0;
	unsigned int cvfm_h, rpt_pix, phase_step = 16, hold_line = 8;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	/* add from sc2 */
	unsigned int fmt_mode = 0;
	unsigned int compbits_yuv;	// 2 bits 0-8bits 1-9bits 2-10bits
	unsigned int compbits_eq8 ;	/* add for sc2 */
	unsigned int conv_lbuf_len;
	unsigned int regs_ofst;
	/* */
	unsigned int dec_lbuf_depth  ; //12 bits
	unsigned int mif_lbuf_depth  ; //12 bits
//ary tmp	unsigned int fmt_size_h     ; //13 bits
//ary tmp	unsigned int fmt_size_v     ; //13 bits

	di_print("afbcd:[%d]:vf typ[0x%x] 0x%lx, 0x%lx\n",
		 dec,
		 vf->vfs.type,
		 vf->vfs.compHeadAddr,
		 vf->vfs.compBodyAddr);
	if (!op)
		return false;

	/* reset inp afbcd */
	if (pafd_ctr->fb.ver >= AFBCD_V5) {
		u32 reset_bit = 0;

		if (dec == EAFBC_DEC2_DI)
			reset_bit = 1 << 12;
		if (reset_bit) {
			op->wr(VIUB_SW_RESET, reset_bit);
			op->wr(VIUB_SW_RESET, 0);
		}
	}

	if (DIM_IS_IC_EF(T7)) {
		union hw_sc2_ctr_pre_s *pre_top_cfg;
		u32 val, reg = 0;
		u8 is_4k, is_afbc;

		pre_top_cfg = &ds->pre_top_cfg;
		if (dec == EAFBC_DEC2_DI) {
			reg = AFBCDM_INP_CTRL0;
			is_4k = pre_top_cfg->b.is_inp_4k ? 1 : 0;
			is_afbc = pre_top_cfg->b.afbc_inp ? 1 : 0;
		} else if (dec == EAFBC_DEC3_MEM) {
			reg = AFBCDM_MEM_CTRL0;
			is_4k = pre_top_cfg->b.is_mem_4k ? 1 : 0;
			is_afbc = pre_top_cfg->b.afbc_mem ? 1 : 0;
		} else if (dec == EAFBC_DEC_CHAN2) {
			reg = AFBCDM_CHAN2_CTRL0;
			is_4k = pre_top_cfg->b.is_chan2_4k ? 1 : 0;
			is_afbc = pre_top_cfg->b.afbc_chan2 ? 1 : 0;
		}
		if (reg) {
			val = op->rd(reg);
			val &= ~(3 << 13);
			//reg_use_4kram
			val |= is_4k << 14;
			//reg_afbc_vd_sel //1:afbc_dec 0:nor_rdmif
			val |= is_afbc << 13;
			op->wr(reg, val);
		}
	}

	w_aligned = round_up((vf->vfs.width), 32);
	/* add from sc2 */

	/* TL1 add bit[13:12]: fmt_mode; 0:yuv444; 1:yuv422; 2:yuv420
	 * di does not support yuv444, so for fmt yuv444 di will bypass+
	 */
	if (pafd_ctr->fb.ver >= AFBCD_V3) {
		if (vf->vfs.type & VIDTYPE_VIU_444)
			fmt_mode = 0;
		else if (vf->vfs.type & VIDTYPE_VIU_422)
			fmt_mode = 1;
		else
			fmt_mode = 2;
	}

	compbits_yuv = (vf->vfs.bitdepth & BITDEPTH_MASK) >> BITDEPTH_Y_SHIFT;
	compbits_eq8 = (compbits_yuv == 0);

	/* 2021-03-09 from tm2 vb, set 128 or 256 */
	//if (pafd_ctr->fb.ver <= AFBCD_V4 && !DIM_IS_IC(T5D)) { //for t5d tmp
	if (pafd_ctr->fb.ver <= AFBCD_V3) {
		conv_lbuf_len = 256;
	} else {
		if ((w_aligned > 1024 && fmt_mode == 0 &&
		     compbits_eq8 == 0)	||
		    w_aligned > 2048)
			conv_lbuf_len = 256;
		else
			conv_lbuf_len = 128;
	}
	dec_lbuf_depth = 128;
	mif_lbuf_depth = 128;
	/*****************************************/
	/*if (di_pre_stru.cur_inp_type & VIDTYPE_INTERLACE)*/
	if (((vf->vfs.type & VIDTYPE_INTERLACE) || is_src_i_dvfm(vf)) &&
	    (vf->vfs.type & VIDTYPE_VIU_422)) /*from vdin and is i */
		h_aligned = round_up((vf->vfs.height / 2), 4);
	else
		h_aligned = round_up((vf->vfs.height), 4);

	/*AFBCD working mode config*/
	r = (3 << 24) |
	    (hold_line << 16) |		/* hold_line_num : 2020 from 10 to 8*/
	    (2 << 14) | /*burst1 1:2020:ary change from 1 to 2*/
	    (vf->vfs.bitdepth & BITDEPTH_MASK);

	if (vf->vfs.bitdepth & BITDEPTH_SAVING_MODE)
		r |= (1 << 28); /* mem_saving_mode */
	if (vf->vfs.type & VIDTYPE_SCATTER)
		r |= (1 << 29);

	out_height = h_aligned;
	if (!(vf->vfs.type & VIDTYPE_VIU_422)) {
		/*from dec, process P as i*/
		if ((vf->vfs.type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP) {
			r |= 0x40;
			vt_ini_phase = 0xc;
			vfmt_rpt_first = 1;
			out_height = h_aligned >> 1;
		} else if ((vf->vfs.type & VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_BOTTOM) {
			r |= 0x80;
			vt_ini_phase = 0x4;
			vfmt_rpt_first = 0;
			out_height = h_aligned >> 1;
		}
	}

	if (IS_420P_SRC(vf->vfs.type)) {
		cvfmt_en = 1;
		vt_ini_phase = 0xc;
		cvfm_h = out_height >> 1;
		/* under prelink mode, need enable upsample filter */
		rpt_pix = 0;
		phase_step = 8;
	} else {
		cvfm_h = out_height;
		rpt_pix = 0;
	}
	op->wr(reg[EAFBC_MODE], r);

	r = 0x1600;
	//if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
	if (pafd_ctr->fb.ver >= AFBCD_V3) {
	/* un compress mode data from vdin bit block order is
	 * different with from dos
	 */
		if (!(vf->vfs.type & VIDTYPE_VIU_422) &&
		    (dec == EAFBC_DEC2_DI ||
		     dec == EAFBC_DEC0))
		    /* ary note: frame afbce, set 0*/
			r |= (1 << 19); /* dos_uncomp */

		if (vf->vfs.type & VIDTYPE_COMB_MODE)
			r |= (1 << 20);
	}
	#ifdef MARK_SC2 /* ary: tm2 no this setting */
	if (pafd_ctr->fb.ver >= AFBCD_V4) {
		r |= (1 << 21); /*reg_addr_link_en*/
		r |= (0x43700 << 0); /*??*/
	}
	#endif
	op->wr(reg[EAFBC_ENABLE], r);

	/*pr_info("AFBC_ENABLE:0x%x\n", reg_rd(reg[eAFBC_ENABLE]));*/

	/**/

	//r = 0x100;
	#ifdef MARK_SC2 /* add  fmt_mode */
	/* TL1 add bit[13:12]: fmt_mode; 0:yuv444; 1:yuv422; 2:yuv420
	 * di does not support yuv444, so for fmt yuv444 di will bypass+
	 */
	//if (is_meson_tl1_cpu() || is_meson_tm2_cpu()) {

	if (pafd_ctr->fb.ver >= AFBCD_V3) {
		if (vf->vfs.type & VIDTYPE_VIU_444)
			r |= (0 << 12);
		else if (vf->vfs.type & VIDTYPE_VIU_422)
			r |= (1 << 12);
		else
			r |= (2 << 12);
	}
	#endif
	//r |= fmt_mode << 12;

	op->wr(reg[EAFBC_CONV_CTRL],
	       fmt_mode << 12 |
	       conv_lbuf_len << 0);/*fmt_mode before sc2 is 0x100*/

	u = (vf->vfs.bitdepth >> (BITDEPTH_U_SHIFT)) & 0x3;
	v = (vf->vfs.bitdepth >> (BITDEPTH_V_SHIFT)) & 0x3;
	op->wr(reg[EAFBC_DEC_DEF_COLOR],
	       0x3FF00000	| /*Y,bit20+*/
	       0x80 << (u + 10)	|
	       0x80 << v);

	/* chroma formatter */
	op->wr(reg[EAFBC_VD_CFMT_CTRL],
	       (rpt_pix << 28)	|
	       (1 << 21)	| /* HFORMATTER_YC_RATIO_2_1 */
	       (1 << 20)	| /* HFORMATTER_EN */
	       (vfmt_rpt_first << 16)	| /* VFORMATTER_RPTLINE0_EN */
	       (vt_ini_phase << 8)	|
	       (phase_step << 1)	| /* VFORMATTER_PHASE_BIT */
	       cvfmt_en);/* different with inp */

	/*if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) { *//*ary add for g12a*/
	if (pafd_ctr->fb.ver >= AFBCD_V3) {
		if (vf->vfs.type & VIDTYPE_VIU_444)
			op->wr(reg[EAFBC_VD_CFMT_W],
			       (w_aligned << 16) | (w_aligned / 2));
		else
			op->wr(reg[EAFBC_VD_CFMT_W],
			       (w_aligned << 16) | (w_aligned));
	} else {	/*ary add for g12a*/
		op->wr(reg[EAFBC_VD_CFMT_W],
		       (w_aligned << 16) | (w_aligned / 2));
	}
	if (pafd_ctr->fb.ver >= AFBCD_V3 && win) {
		tl1_set_afbcd_crop(vf, reg, win, op);
	} else {
		op->wr(reg[EAFBC_MIF_HOR_SCOPE],
		       (0 << 16) | ((w_aligned >> 5) - 1));
		op->wr(reg[EAFBC_MIF_VER_SCOPE],
		       (0 << 16) | ((h_aligned >> 2) - 1));

		op->wr(reg[EAFBC_PIXEL_HOR_SCOPE],
		       (0 << 16) | (vf->vfs.width - 1));
		op->wr(reg[EAFBC_PIXEL_VER_SCOPE],
		       0 << 16 | (vf->vfs.height - 1));
		op->wr(reg[EAFBC_SIZE_IN], (vf->vfs.height) | w_aligned << 16);
	}
	op->wr(reg[EAFBC_VD_CFMT_H], /*out_height*/cvfm_h);

	di_print("\t:size:%d\n", vf->vfs.height);
	if (pafd_ctr->fb.ver <= AFBCD_V4)
		op->wr(reg[EAFBC_SIZE_OUT], out_height | w_aligned << 16);

	op->wr(reg[EAFBC_HEAD_BADDR], vf->vfs.compHeadAddr >> 4);
	op->wr(reg[EAFBC_BODY_BADDR], vf->vfs.compBodyAddr >> 4);

	if (pafd_ctr->fb.ver >= AFBCD_V5 && cfg) {
		u32 reg_val;

		regs_ofst = afbcd_v5_get_offset(dec);
		//op->bwr((regs_ofst + AFBCDM_IQUANT_ENABLE),
		//	cfg->reg_lossy_en, 0, 1);//lossy_luma_en
		//op->bwr((regs_ofst + AFBCDM_IQUANT_ENABLE),
		//	cfg->reg_lossy_en, 4, 1);//lossy_chrm_en
		//op->bwr((regs_ofst + AFBCDM_IQUANT_ENABLE),
		//	cfg->reg_lossy_en, 10, 1);
		//op->bwr((regs_ofst + AFBCDM_IQUANT_ENABLE),
		//	cfg->reg_lossy_en, 11, 1);
		reg_val = op->rd(regs_ofst + AFBCDM_IQUANT_ENABLE);
		if (cfg->reg_lossy_en)
			reg_val |=
				((1 << 11) | //lossy_luma_en
				 (1 << 10) | //lossy_chrm_en
				 (1 << 4) |
				 (1 << 0));
		else
			reg_val &=
				~((1 << 11) | //lossy_luma_en
				 (1 << 10) | //lossy_chrm_en
				 (1 << 4) |
				 (1 << 0));
		op->wr((regs_ofst + AFBCDM_IQUANT_ENABLE), reg_val);
		op->wr((regs_ofst + AFBCDM_ROT_CTRL),
		       ((cfg->pip_src_mode  & 0x1) << 27) |
		       //pip_src_mode
		       ((cfg->rot_drop_mode & 0x7) << 24) |
		       //rot_uv_vshrk_drop_mode
		       ((cfg->rot_drop_mode & 0x7) << 20) |
		       //rot_uv_hshrk_drop_mode
		       ((cfg->rot_vshrk	  & 0x3) << 18) |
		       //rot_uv_vshrk_ratio
		       ((cfg->rot_hshrk	  & 0x3) << 16) |
		       //rot_uv_hshrk_ratio
		       ((cfg->rot_drop_mode & 0x7) << 12) |
		       //rot_y_vshrk_drop_mode
		       ((cfg->rot_drop_mode & 0x7) << 8) |
		       //rot_y_hshrk_drop_mode
		       ((cfg->rot_vshrk	  & 0x3) << 6) |
		       //rot_y_vshrk_ratio
		       ((cfg->rot_hshrk	  & 0x3) << 4) |
		       //rot_y_hshrk_ratio
		       ((0		  & 0x3) << 2) |
		       //rot_uv422_drop_mode
		       ((0		  & 0x3) << 1) |
		       //rot_uv422_omode
		       ((cfg->rot_en	  & 0x1) << 0)
		       //rot_enable
		       );

		op->wr((regs_ofst + AFBCDM_ROT_SCOPE),
		       ((1 & 0x1) << 16) | //reg_rot_ifmt_force444
		       ((cfg->rot_ofmt_mode & 0x3) << 14) |
		       //reg_rot_ofmt_mode
		       ((cfg->rot_ocompbit & 0x3) << 12) |
		       //reg_rot_compbits_out_y
		       ((cfg->rot_ocompbit & 0x3) << 10) |
		       //reg_rot_compbits_out_uv
		       ((cfg->rot_vbgn & 0x3) << 8) | //rot_wrbgn_v
		       ((cfg->rot_hbgn & 0x1f) << 0) //rot_wrbgn_h
		       );
	}
	return true;
}

/* copy afbc_update_level1 */
static void afbc_update_level1_dvfm(struct dvfm_s *vf, enum EAFBC_DEC dec, const struct reg_acc *op)
{
	const unsigned int *reg = afbc_get_addrp(dec);

	//di_print("%s:%d:0x%x\n", __func__, dec, vf->type);
	//dim_print("%s:head2:0x%x=0x%x\n", __func__,
	//AFBCDM_MEM_HEAD_BADDR, reg_rd(AFBCDM_MEM_HEAD_BADDR));
	if (!op) {
		PR_ERR("%s:no op\n", __func__);
		return;
	}
	op->wr(reg[EAFBC_HEAD_BADDR], vf->vfs.compHeadAddr >> 4);
	op->wr(reg[EAFBC_BODY_BADDR], vf->vfs.compBodyAddr >> 4);
}

/* copy afbc_update_level2_inp */
static void afbc_update_level2_inp_dvfm(struct afbcd_ctr_s *pctr, const struct reg_acc *op)
{
	const unsigned int *reg = afbc_get_addrp(pctr->fb.pre_dec);
	unsigned int vfmt_rpt_first = 1, vt_ini_phase = 12;
	unsigned int old_mode, old_cfmt_ctrl;

	if (!op) {
		PR_ERR("%s:no op\n", __func__);
		return;
	}

	di_print("afbcd:up:%d\n", pctr->fb.pre_dec);
	old_mode = op->rd(reg[EAFBC_MODE]);
	old_cfmt_ctrl = op->rd(reg[EAFBC_VD_CFMT_CTRL]);
	old_mode &= (~(0x03 << 6));
	if (!(pctr->l_vtype & VIDTYPE_VIU_422)) {
		/*from dec, process P as i*/
		if ((pctr->l_vtype & VIDTYPE_TYPEMASK) ==
		    VIDTYPE_INTERLACE_TOP) {
			old_mode |= 0x40;

			vt_ini_phase = 0xc;
			vfmt_rpt_first = 1;
			//out_height = h_aligned>>1;
		} else if ((pctr->l_vtype & VIDTYPE_TYPEMASK) ==
			   VIDTYPE_INTERLACE_BOTTOM) {
			old_mode |= 0x80;
			vt_ini_phase = 0x4;
			vfmt_rpt_first = 0;
			//out_height = h_aligned>>1;
		} else { //for p as p?
			//out_height = h_aligned>>1;
		}
	}
	op->wr(reg[EAFBC_MODE], old_mode);
	/* chroma formatter */
	op->wr(reg[EAFBC_VD_CFMT_CTRL],
	       old_cfmt_ctrl		|
	       (vfmt_rpt_first << 16)	| /* VFORMATTER_RPTLINE0_EN */
	       (vt_ini_phase << 8)); /* different with inp */

	op->wr(reg[EAFBC_HEAD_BADDR], pctr->addr_h >> 4);
	op->wr(reg[EAFBC_BODY_BADDR], pctr->addr_b >> 4);
}

/* copy from vf_set_for_com */
static void vf_set_for_com_dvf(struct dvfm_s *vf)
{
	//struct vframe_s *vf;

	///vf = di_buf->vframe;

	//vf->canvas0_config[0].phy_addr = di_buf->nr_adr;
	vf->vfs.type |= (VIDTYPE_COMPRESS | VIDTYPE_SCATTER);
	//vf->canvas0_config[0].width = di_buf->canvas_width[NR_CANVAS],
	//vf->canvas0_config[0].height = di_buf->canvas_height;
	//vf->canvas0_config[0].block_mode = 0;
	//vf->plane_num = 1;
	//vf->canvas0Addr = -1;
	//vf->canvas1Addr = -1;
	//vf->compHeadAddr = di_buf->afbc_adr;
	vf->vfs.compBodyAddr = 0;//di_buf->nr_adr;
	//vf->compHeight = vf->height;
	//vf->compWidth  = vf->width;
	vf->vfs.bitdepth |= (BITDEPTH_U10 | BITDEPTH_V10);

	if (vf->afbce_out_yuv420_10) {
		vf->vfs.type &= ~VFMT_COLOR_MSK;
		vf->vfs.type |= VIDTYPE_VIU_FIELD;
		vf->vfs.bitdepth &= ~(FULL_PACK_422_MODE);
	}
	//dim_print("%s:%px:0x%x\n", __func__,  vf, vf->type);
#ifdef DBG_AFBC

	if (afbc_cfg_vf)
		vf->vfs.type |= afbc_cfg_vf;
	if (afbc_cfg_bt)
		vf->vfs.bitdepth |= afbc_cfg_bt;
#endif
}

/* copy from afbce_set */
static void afbce_set_dvfm(struct dvfm_s *vf, enum EAFBC_ENC enc, const struct reg_acc *op)
{
	struct enc_cfg_s cfg_data;
	struct enc_cfg_s *cfg = &cfg_data;
	//struct di_buf_s	*di_buf;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	bool flg_v5 = false;
	#ifdef DBG_CRC
	bool crc_right;
	#endif

	if (!vf) {
		pr_error("%s:0:no vf\n", __func__);
		return;
	}
#ifdef HIS_CODE
	di_buf = (struct di_buf_s *)vf->private_data;

	if (!di_buf) {
		pr_error("%s:1:no di buf\n", __func__);
		return;
	}
	#ifdef DBG_CRC
	crc_right = dbg_checkcrc(di_buf);
	if (!crc_right /*&& (!dim_dbg_is_cnt())*/) {
		PR_INF("afbce[%d]s:t[%d:%d],inf[0x%lx],adr[0x%lx],vty[0x%x]\n",
		       enc,
		       di_buf->type, di_buf->index,
		       di_buf->afbc_adr, di_buf->afbct_adr, vf->type);
	}
	#endif
#endif /*HIS_CODE*/
	vf->flg_afbce_set = 1;
	cfg->enable = 1;
	/* 0:close 1:luma lossy 2:chrma lossy 3: luma & chrma lossy*/
	cfg->loosy_mode = 0;
	cfg->head_baddr = vf->vfs.compHeadAddr;//head_baddr_enc;/*head addr*/
	cfg->mmu_info_baddr = vf->afbct_adr;
	//vf->compHeadAddr = cfg->head_baddr;
	//vf->compBodyAddr = di_buf->nr_adr;
	#ifdef MARK_SC2
	if (enc == 1) {
		di_buf->vframe->height = di_buf->win.y_size;
		di_buf->vframe->width	= di_buf->win.x_size;
	}
	#endif
	vf_set_for_com_dvf(vf);
	if (cfgg(AFBCE_LOSS_EN) == 2 ||
	    ((vf->vfs.type & VIDTYPE_COMPRESS_LOSS) &&
	     cfgg(AFBCE_LOSS_EN) == 1))
		cfg->loosy_mode = 0x3;
#ifdef AFBCP
	di_print("%s:head[0x%lx],info[0x%lx]\n",
		 __func__,
		 //di_buf->index,
		 cfg->head_baddr,
		 cfg->mmu_info_baddr);
#endif
	if (vf->afbce_out_yuv420_10)
		cfg->reg_format_mode = 2;
	else
		cfg->reg_format_mode = 1;/*0:444 1:422 2:420*/
	cfg->reg_compbits_y = 10;//8;/*bits num after compression*/
	cfg->reg_compbits_c = 10;//8;/*bits num after compression*/

	/*input size*/
	cfg->hsize_in = vf->vfs.width;//src_w;
	if ((is_src_i_dvfm(vf) && enc == 0) ||
	    (is_src_i_dvfm(vf) && is_mask(SC2_MEM_CPY)))
		cfg->vsize_in = vf->vfs.height >> 1;
	else
		cfg->vsize_in = vf->vfs.height;//src_h;
	/*input scope*/
	cfg->enc_win_bgn_h = 0;
	cfg->enc_win_end_h = vf->vfs.width - 1;
	cfg->enc_win_bgn_v = 0;
	cfg->enc_win_end_v = cfg->vsize_in - 1; //vf->height - 1;
	if (is_mask(SC2_ROT_WR) && enc == 1)
		cfg->rot_en = 1;
	else
		cfg->rot_en = 0;

	dim_print("%s:win:<%d><%d><%d><%d><%d><%d><%d><%d>\n",
		  __func__,
		  cfg->enc_win_bgn_h,
		  cfg->enc_win_bgn_v,
		  cfg->enc_win_end_h,
		  cfg->enc_win_end_v,
		  cfg->hsize_in,
		  cfg->vsize_in,
		  vf->vfs.width,
		  vf->vfs.height);
	/*for sc2*/
	if (pafd_ctr->fb.ver >= AFBCD_V5)
		flg_v5 = true;
#ifdef MARK_SC2
	if (enc == 1) {
		cfg->reg_init_ctrl  = 1;//pip init frame flag
		cfg->reg_pip_mode   = 1;//pip open bit
		cfg->hsize_bgnd     = 1920;//hsize of background
		cfg->vsize_bgnd     = 1080;//hsize of background
	} else {
		cfg->reg_init_ctrl  = 0;//pip init frame flag
		cfg->reg_pip_mode   = 0;//pip open bit
		cfg->hsize_bgnd     = 0;//hsize of background
		cfg->vsize_bgnd     = 0;//hsize of background
	}
#endif
	cfg->reg_init_ctrl  = 0;//pip init frame flag
	cfg->reg_pip_mode   = 0;//pip open bit
	cfg->hsize_bgnd     = 0;//hsize of background
	cfg->vsize_bgnd     = 0;//hsize of background
	cfg->reg_ram_comb   = 0;//ram split bit open in di mult write case

	cfg->rev_mode	    = 0;//0:normal mode
	cfg->def_color_0    = 0;//def_color
	cfg->def_color_1    = 0;//def_color
	cfg->def_color_2    = 0;//def_color
	cfg->def_color_3    = 0;//def_color
	cfg->force_444_comb = 0;
	//cfg->rot_en	    = 0;
	cfg->din_swt	    = 0;

	ori_afbce_cfg(cfg, op, enc);
}

/* from afbce_update_level1 */
static void afbce_update_level1_dvfm(struct dvfm_s *vf,
				const struct reg_acc *op,
				enum EAFBC_ENC enc)
{
	const unsigned int *reg;
//	struct di_buf_s *di_buf;
	unsigned int cur_mmu_used = 0;
	#ifdef DBG_CRC
	bool crc_right;
	#endif
#ifdef HIS_CODE
	di_buf = (struct di_buf_s *)vf->private_data;

	if (!di_buf) {
		pr_error("%s:1:no di buf\n", __func__);
		return;
	}
	#ifdef DBG_CRC // DBG_TEST_CRC
	crc_right = dbg_checkcrc(di_buf);
	if (!crc_right) {
		PR_INF("afbce[%d]s:t[%d:%d],inf[0x%lx],adr[0x%lx],vty[0x%x]\n",
		       enc,
		       di_buf->type, di_buf->index,
		       di_buf->afbc_adr, di_buf->afbct_adr, vf->type);
	}
	#endif
#endif /* HIS_CODE */
	reg = &reg_afbc_e[enc][0];
	dim_print("afbce:up:%d\n", enc);

	#ifdef MARK_SC2
	if (dim_dbg_is_cnt()) {
		PR_INF("afbce[%d]up:t[%d:%d],inf[0x%lx],adr[0x%lx],vty[0x%x]\n",
		       enc,
		       di_buf->type, di_buf->index,
		       di_buf->afbc_adr, di_buf->afbct_adr, vf->type);
	}
	#endif
	vf->flg_afbce_set = 1;

	//vf->compHeadAddr = di_buf->afbc_adr;
	//vf->compBodyAddr = di_buf->nr_adr;
	vf_set_for_com_dvf(vf);

	//head addr of compressed data
	if (DIM_IS_IC_EF(T7) || DIM_IS_IC(S4)) {
		op->wr(reg[EAFBCE_HEAD_BADDR], vf->vfs.compHeadAddr >> 4);
		op->wr(reg[EAFBCE_MMU_RMIF_CTRL4], vf->afbct_adr >> 4);

	} else {
		op->wr(reg[EAFBCE_HEAD_BADDR], vf->vfs.compHeadAddr);
		op->wr(reg[EAFBCE_MMU_RMIF_CTRL4], vf->afbct_adr);
	}
	op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], cur_mmu_used, 0, 12);
	op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], 0x1ffe, 16, 13);
	op->bwr(reg[EAFBCE_MMU_RMIF_CTRL3], 0x1fff, 0, 13);
	afbce_sw(enc, true, op);
}

/* copy from enable_afbc_input
 */
static u32 enable_afbc_input_dvfm(void *ds_in, void *nvfm_in,
				const struct reg_acc *op_in)
{
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();

	struct dvfm_s *mem_vf2, *inp_vf2, *chan2_vf;
	struct AFBCD_CFG_S cfg;
	struct AFBCD_CFG_S *pcfg;
	struct dim_prevpp_ds_s *ds;
	struct dimn_dvfm_s *ndvfm;
	struct dvfm_s *nr_vf;
	struct di_win_s *win_in, *win_mem;
	//struct dvfm_s *in, *mem;

	if (!afbc_is_supported_for_plink())
		return false;
	if (!ds_in || !nvfm_in || !op_in)
		return false;
	ds = (struct dim_prevpp_ds_s *)ds_in;
	ndvfm = (struct dimn_dvfm_s *)nvfm_in;

	//inp_vf2 = inp_vf;
	//mem_vf2 = mem_vf;
	inp_vf2 = &ndvfm->c.in_dvfm_crop;
	mem_vf2 = &ndvfm->c.mem_dvfm;
	nr_vf	= &ndvfm->c.nr_wr_dvfm;
	chan2_vf = NULL; //tmp
	win_in = &ds->mifpara_in.win;
	win_mem = &ds->mifpara_mem.win;
	if (is_cfg(EAFBC_CFG_ETEST)) {
		mem_vf2 = inp_vf2;
		win_mem = &ds->mifpara_in.win;
	} else if (is_cfg(EAFBC_CFG_ETEST2)) {
		inp_vf2 = mem_vf2;
		win_in = &ds->mifpara_mem.win;
	}

	if (!inp_vf2) {
		PR_ERR("%s:no input\n", __func__);
		return 0;
	}

	if (pafd_ctr->fb.ver < AFBCD_V4) {
		if (inp_vf2->vfs.type & VIDTYPE_COMPRESS) {
			afbc_sw_op(true, op_in);
		} else {
			afbc_sw_op(false, op_in);
			return false;
		}
	} else {
		/* AFBCD_V5 */
		if (!pafd_ctr->en_set.d8) {
			PR_INF("%s:%d:0x%x\n", __func__, ndvfm->c.cnt_in,
				pafd_ctr->en_set.d8);
			afbc_sw_op(false, op_in);
			return false;
		}
		afbc_sw_op(true, op_in);
	}

	if (pafd_ctr->fb.ver >= AFBCD_V5) {
		pcfg = &cfg;
		memset(pcfg, 0, sizeof(*pcfg));
	} else {
		pcfg = NULL;
	}

	afbc_check_chg_level_dvfm(inp_vf2, mem_vf2, chan2_vf, pafd_ctr, op_in);

	if (is_src_real_i_dvfm(mem_vf2))
		//dim_print("%s:real i\n", __func__);
		pafd_ctr->b.chg_mem = 3;

	if (pafd_ctr->b.chg_level == 3 || pafd_ctr->b.chg_mem == 3 ||
	    pafd_ctr->b.chg_chan2 == 3) {
		PR_INF("%s:%d:chg:%d,%d,%d, op:%px\n", __func__,
		       ndvfm->c.cnt_in,
		       pafd_ctr->b.chg_level,
		       pafd_ctr->b.chg_mem,
		       pafd_ctr->b.chg_chan2, op_in);
		if (pafd_ctr->fb.ver == AFBCD_V4) {
			if (pafd_ctr->en_set.b.inp)
				afbc_tm2_sw_inp(true, op_in);
			else
				afbc_tm2_sw_inp(false, op_in);
			if (mem_vf2 && pafd_ctr->en_set.b.mem)
				afbc_tm2_sw_mem(true, op_in);
			else
				afbc_tm2_sw_mem(false, op_in);

			if (pafd_ctr->en_set.b.enc_nr)
				afbce_tm2_sw(true, op_in);
			else
				afbce_tm2_sw(false, op_in);
		}
		/*inp*/
		if (pafd_ctr->en_set.b.inp) {
			if (cfgg(AFBCE_LOSS_EN) == 2 ||
				(inp_vf2->vfs.type & VIDTYPE_COMPRESS_LOSS &&
				cfgg(AFBCE_LOSS_EN) == 1)) {
				cfg.reg_lossy_en = 1;
				nr_vf->vfs.type |= VIDTYPE_COMPRESS_LOSS;
			}
			enable_afbc_input_local_dvfm(ds, inp_vf2,
						pafd_ctr->fb.pre_dec, pcfg,
						win_in,
						op_in);
		}
		if (is_src_i_dvfm(inp_vf2) || VFMT_IS_I(inp_vf2->vfs.type))
			src_i_set_dvfm(nr_vf);
			//dim_print("%s:set srci\n", __func__);

		if (mem_vf2 && pafd_ctr->en_set.b.mem && ndvfm->c.set_cfg.b.en_mem_afbcd) {
			/* mem */
			if (cfgg(AFBCE_LOSS_EN) == 2 ||
			    (mem_vf2->vfs.type & VIDTYPE_COMPRESS_LOSS &&
			     cfgg(AFBCE_LOSS_EN) == 1)) {
				cfg.reg_lossy_en = 1;
				nr_vf->vfs.type |= VIDTYPE_COMPRESS_LOSS;
			} else {
				cfg.reg_lossy_en = 0;
			}
			enable_afbc_input_local_dvfm(ds, mem_vf2,
						pafd_ctr->fb.mem_dec,
						pcfg,
						win_mem,
						op_in);
		} else if (!is_src_real_i_dvfm(mem_vf2) &&
			pafd_ctr->en_set.b.inp &&
			pafd_ctr->en_set.b.enc_nr) {
			/**/
			if (mem_vf2->vfs.type & VIDTYPE_COMPRESS_LOSS)
				cfg.reg_lossy_en = 1;
			dim_print("%s:test\n", __func__);
			enable_afbc_input_local_dvfm(ds, inp_vf2,
						pafd_ctr->fb.mem_dec,
						pcfg,
						win_in,
						op_in);
		}
#ifdef MARK_DEADCODE_HIS
		/*chan2_vf is write dead so comment the following codes*/
		if (chan2_vf && pafd_ctr->en_set.b.chan2)
			/* chan2 */
			enable_afbc_input_local_dvfm(ds, chan2_vf,
						pafd_ctr->fb.ch2_dec,
						pcfg,
						NULL,
						op_in);
#endif
		/*nr*/
		if (pafd_ctr->en_set.b.enc_nr)
			afbce_set_dvfm(nr_vf, EAFBC_ENC0, op_in);

	} else if (pafd_ctr->b.chg_level == 2) {
		if (pafd_ctr->en_set.b.inp)
			afbc_update_level2_inp_dvfm(pafd_ctr, op_in);

		if (is_src_i_dvfm(inp_vf2) || VFMT_IS_I(inp_vf2->vfs.type)) {
			src_i_set_dvfm(nr_vf);
			dim_print("%s:set srci\n", __func__);
		}

		/*mem*/
		if (pafd_ctr->en_set.b.mem) {
			if (is_cfg(EAFBC_CFG_FORCE_MEM))
				enable_afbc_input_local_dvfm(ds, mem_vf2,
							pafd_ctr->fb.mem_dec,
							pcfg,
							win_mem,
							op_in);
			else
				afbc_update_level1_dvfm(mem_vf2,
						   pafd_ctr->fb.mem_dec,
						   op_in);
		}

#ifdef MARK_DEADCODE_HIS
	/* the following codes are commented result in chan2_vf is false*/
		if (pafd_ctr->en_set.b.chan2 && chan2_vf) {
			if (is_cfg(EAFBC_CFG_FORCE_CHAN2))
				enable_afbc_input_local_dvfm
					(ds, chan2_vf,
					 pafd_ctr->fb.ch2_dec,
					 pcfg,
					 NULL,
					 op_in);
			else
				afbc_update_level1_dvfm(chan2_vf,
						   pafd_ctr->fb.ch2_dec,
						   op_in);
		}
#endif
		/*nr*/
		if (pafd_ctr->en_set.b.enc_nr) {
			if (is_cfg(EAFBC_CFG_FORCE_NR))
				afbce_set_dvfm(nr_vf, EAFBC_ENC0, op_in);
			else
				afbce_update_level1_dvfm(nr_vf,
						    op_in,
						    EAFBC_ENC0);
		}
	} else if (pafd_ctr->b.chg_level == 1) {
		/*inp*/
		if (pafd_ctr->en_set.b.inp)
			afbc_update_level1_dvfm(inp_vf2, pafd_ctr->fb.pre_dec, op_in);

		/*mem*/
		if (pafd_ctr->en_set.b.mem)
			afbc_update_level1_dvfm(mem_vf2, pafd_ctr->fb.mem_dec, op_in);

#ifdef MARK_DEADCODE_HIS
		/*chan2_vf is NULL the follow condition cannot be true*/
		if (pafd_ctr->en_set.b.chan2 && chan2_vf)
			afbc_update_level1_dvfm(chan2_vf, pafd_ctr->fb.ch2_dec, op_in);
#endif
		/*nr*/
		if (pafd_ctr->en_set.b.enc_nr)
			afbce_update_level1_dvfm(nr_vf,
					    op_in, EAFBC_ENC0);
	}
	return 0;
}

/************************************************
 * afbc_pre_check_dvfm
 *	copy from afbc_pre_check
 *	for pre-vpp link
 ************************************************/
static void afbc_pre_check_dvfm(void *ds_in, void *vfm) /* struct dimn_dvfm_s */
{
	union hw_sc2_ctr_pre_s *cfg;
	struct afbcd_ctr_s *pctr;
	union afbc_blk_s *pblk;
	//struct di_buf_s *di_buf;
	union afbc_blk_s	*en_set_pre;
	union afbc_blk_s	*en_cfg_pre;
//	struct di_ch_s *pch;
	struct dimn_dvfm_s *nvfm;
	struct dim_prevpp_ds_s *ds;

	if (!ds_in	||
	    !vfm	||
	    !get_afdp())
		return;

//	cfg = di_afdp->top_cfg_pre;
	nvfm = (struct dimn_dvfm_s *)vfm;
	ds	= (struct dim_prevpp_ds_s *)ds_in;
	pctr = di_get_afd_ctr();

	pctr->en_sgn.d8 = nvfm->c.afbc_sgn_cfg.d8;

	en_cfg_pre = &ds->afbc_en_cfg;
	en_set_pre = &ds->afbc_en_set;
	en_set_pre->d8 = en_cfg_pre->d8 & pctr->en_sgn.d8;
	pctr->en_set.d8 = en_set_pre->d8;
	pctr->en_cfg.d8 = en_cfg_pre->d8;

	cfg = &ds->pre_top_cfg;

	pblk = &pctr->en_set;
	if (pblk->b.inp) {
		cfg->b.afbc_inp = 1;
		if (nvfm->c.is_inp_4k)
			cfg->b.is_inp_4k = 1;
		else
			cfg->b.is_inp_4k = 0;
	} else {
		cfg->b.afbc_inp = 0;
		cfg->b.is_inp_4k = 0;
	}
	if (nvfm->c.is_inp_4k)
		cfg->b.is_inp_4k = 1;
	else
		cfg->b.is_inp_4k = 0;

	if (pblk->b.mem) {
		cfg->b.afbc_mem = 1;

		if (nvfm->c.is_out_4k)
			cfg->b.is_mem_4k = 1;
		else
			cfg->b.is_mem_4k = 0;
	} else {
		cfg->b.afbc_mem = 0;
		cfg->b.is_mem_4k = 0;
	}
	//not support i for pvpp_link
	cfg->b.afbc_chan2 = 0;
	cfg->b.is_chan2_4k = 0;

	cfg->b.mode_4k = 0;
	if (pblk->b.enc_nr) {
		cfg->b.afbc_nr_en	= 1;
		if (nvfm->c.dw_have)
			cfg->b.mif_en	= 1;
		else
			cfg->b.mif_en	= 0;
		if (nvfm->c.is_out_4k)
			cfg->b.mode_4k = 1;
	} else {
		cfg->b.afbc_nr_en	= 0;
		cfg->b.mif_en		= 1;
	}

	//dim_print("%s:\n", __func__);
}

struct afd_ops_s di_afd_ops_v3 = {
	.prob			= afbc_prob,
	.is_supported		= afbc_is_supported,
	.en_pre_set		= enable_afbc_input,
	.en_pst_set		= afbc_pst_set,
	.inp_sw			= afbc_input_sw,
	.reg_sw			= afbc_reg_sw,
	.reg_val		= afbc_reg_variable,
	.rest_val		= afbc_val_reset_newformat,
	.is_used		= afbc_is_used,
	.is_used_inp		= afbc_is_used_inp,
	.is_used_mem		= afbc_is_used_mem,
	.is_used_chan2		= afbc_is_used_chan2,
	.is_free		= afbc_is_free,
	.cnt_info_size		= afbc_count_info_size,
	.cnt_tab_size		= afbc_count_tab_size,
	.cnt_buf_size		= v2_afbc_count_buffer_size,
	.dump_reg		= dump_afbcd_reg,
	.rqst_share		= di_request_afbc,
	.get_d_addrp		= afbc_get_addrp,
	.get_e_addrp		= afbce_get_addrp,
	.dbg_rqst_share		= dbg_request_afbc,
	.dbg_afbc_blk		= dbg_afbc_blk,
	.int_tab		= afbc_int_tab,
	.tab_cnt_crc		= afbc_tab_cnt_crc,
	.is_cfg			= is_cfg,
	.pre_check		= afbc_pre_check,
	.pst_check		= afbc_pst_check,
	//.pre_check2		= afbc_pre_check2,
	.is_sts			= is_status,
	.sgn_mode_set		= afbc_sgn_mode_set,
	.cnt_sgn_mode		= afbc_cnt_sgn_mode,
	.cfg_mode_set		= afbc_cfg_mode_set,
	.is_supported_plink	= afbc_is_supported_for_plink,
	.reg_sw_op		= afbc_reg_sw_op,
	.inp_sw_op		= afbc_input_sw_op,
	.pvpp_reg_val		= afbc_reg_variable_prelink,
	.pvpp_sw_setting_op	= afbc_sw_op,
	.pvpp_pre_check_dvfm	= afbc_pre_check_dvfm,
	.pvpp_en_pre_set	= enable_afbc_input_dvfm,
};

bool di_attach_ops_afd_v3(const struct afd_ops_s **ops)
{
	*ops = &di_afd_ops_v3;
	return true;
}
EXPORT_SYMBOL(di_attach_ops_afd_v3);

/*afbc e**********************************************************************/
/*****************************************************************************/

/*don't change order*/

static const unsigned int  afbce_default_val[DIM_AFBCE_UP_NUB] = {
	/*need add*/
	0x00000000,
	0x03044000,
	0x07800438,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
};

static void vf_set_for_com(struct di_buf_s *di_buf)
{
	struct vframe_s *vf;

	vf = di_buf->vframe;

	vf->canvas0_config[0].phy_addr = di_buf->nr_adr;
	vf->type |= (VIDTYPE_COMPRESS | VIDTYPE_SCATTER);
	vf->canvas0_config[0].width = di_buf->canvas_width[NR_CANVAS],
	vf->canvas0_config[0].height = di_buf->canvas_height;
	vf->canvas0_config[0].block_mode = 0;
	vf->plane_num = 1;
	vf->canvas0Addr = -1;
	vf->canvas1Addr = -1;
	vf->compHeadAddr = di_buf->afbc_adr;
	vf->compBodyAddr = di_buf->nr_adr;
	vf->compHeight = vf->height;
	vf->compWidth  = vf->width;
	vf->bitdepth |= (BITDEPTH_U10 | BITDEPTH_V10);

	if (di_buf->afbce_out_yuv420_10) {
		vf->type &= ~VFMT_COLOR_MSK;
		vf->type |= VIDTYPE_VIU_FIELD;
		vf->bitdepth &= ~(FULL_PACK_422_MODE);
	}
	//dim_print("%s:%px:0x%x\n", __func__,  vf, vf->type);
#ifdef DBG_AFBC

	if (afbc_cfg_vf)
		vf->type |= afbc_cfg_vf;
	if (afbc_cfg_bt)
		vf->bitdepth |= afbc_cfg_bt;
#endif
}

/*enc: set_di_afbce_cfg ******************/
static void ori_afbce_cfg(struct enc_cfg_s *cfg,
			  const struct reg_acc *op,
			  enum EAFBC_ENC enc)
{
	const unsigned int *reg;
	int hold_line_num	= 4;
	int lbuf_depth		= 256;
	int rev_mode		= 0;
	int def_color_0		= 0x3ff;
	int def_color_1		= 0x80;
	int def_color_2		= 0x80;
	int def_color_3		= 0;
	int hblksize_out;//sc2	= (cfg->hsize_in + 31) >> 5;
	int vblksize_out;//sc2	= (cfg->vsize_in + 3) >> 2;
	/* output blk scope */
	int blk_out_end_h	=  cfg->enc_win_bgn_h >> 5;
	/* output blk scope */
	int blk_out_bgn_h	= (cfg->enc_win_end_h + 31) >> 5;
	/*output blk scope */
	int blk_out_end_v	=  cfg->enc_win_bgn_v >> 2;
	/* output blk scope */
	int blk_out_bgn_v	= (cfg->enc_win_end_v + 3) >> 2;

	int lossy_luma_en;
	int lossy_chrm_en;
	int cur_mmu_used	= 0;/*mmu info linear addr*/

	int reg_fmt444_comb;
	int uncmp_size       ;/*calculate*/
	int uncmp_bits       ;/*calculate*/
	int sblk_num         ;/*calculate*/
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	bool flg_v5 = false;
	unsigned int	   hsize_buf;
	unsigned int	   vsize_buf;

	/* sc2 */
	if (pafd_ctr->fb.ver >= AFBCD_V5)
		flg_v5 = true;

	if (flg_v5) {
		if (cfg->din_swt)
			enc = 1 - enc;
		op->bwr(DI_TOP_CTRL1, (cfg->din_swt ? 3 : 0), 0, 2);
		//only di afbce have ram_comb

		hold_line_num = 0;

		/* yuv444 can only support 8bit,and must use comb_mode */
		reg_fmt444_comb = (cfg->reg_format_mode == 0) &&
			(cfg->force_444_comb);
	} else {
		/*yuv444 can only support 8bit,and must use comb_mode*/
		if (cfg->reg_format_mode == 0 && cfg->hsize_in > 2048)
			reg_fmt444_comb = 1;
		else
			reg_fmt444_comb = 0;
	}
	/* for before sc2, pip mode is fix 0 */
	hsize_buf = cfg->reg_pip_mode ? cfg->hsize_bgnd : cfg->hsize_in;
	vsize_buf = cfg->reg_pip_mode ? cfg->vsize_bgnd : cfg->vsize_in;

	hblksize_out	= (hsize_buf + 31) >> 5;
	vblksize_out	= (vsize_buf + 3) >> 2;

	/***********************/

	reg = &reg_afbc_e[enc][0];

#ifdef MARK_SC2
	/*yuv444 can only support 8bit,and must use comb_mode*/
	if (cfg->reg_format_mode == 0 && cfg->hsize_in > 2048)
		reg_fmt444_comb = 1;
	else
		reg_fmt444_comb = 0;
#endif
	/* 4*4 subblock number in every 32*4 mblk */
	if (cfg->reg_format_mode == 2)
		sblk_num = 12;
	else if (cfg->reg_format_mode == 1)
		sblk_num = 16;
	else
		sblk_num = 24;

	if (cfg->reg_compbits_y > cfg->reg_compbits_c)
		uncmp_bits = cfg->reg_compbits_y;
	else
		uncmp_bits = cfg->reg_compbits_c;

	/*bit size of uncompression mode*/
	uncmp_size = (((((16 * uncmp_bits * sblk_num) + 7) >> 3) +
			31) / 32) << 1;

	/*chose loosy mode of luma and chroma */
	if (cfg->loosy_mode == 0) {
		lossy_luma_en = 0;
		lossy_chrm_en = 0;
	} else if (cfg->loosy_mode == 1) {
		lossy_luma_en = 1;
		lossy_chrm_en = 0;
	} else if (cfg->loosy_mode == 2) {
		lossy_luma_en = 0;
		lossy_chrm_en = 1;
	} else if (cfg->loosy_mode == 3) {
		lossy_luma_en = 1;
		lossy_chrm_en = 1;
	} else {
		lossy_luma_en = 0;
		lossy_chrm_en = 0;
	}

	op->wr(reg[EAFBCE_MODE],
	       (0                & 0x7)		<< 29 |
	       (rev_mode         & 0x3)		<< 26 |
	       (3                & 0x3)		<< 24 |
	       (hold_line_num    & 0x7f)	<< 16 |
	       (2                & 0x3)		<< 14 |
	       (reg_fmt444_comb  & 0x1));
	/* loosy */
	op->bwr(reg[EAFBCE_QUANT_ENABLE], lossy_luma_en, 0, 1);
	op->bwr(reg[EAFBCE_QUANT_ENABLE], lossy_chrm_en, 4, 1);
	op->bwr(reg[EAFBCE_QUANT_ENABLE], lossy_luma_en, 10, 1);
	op->bwr(reg[EAFBCE_QUANT_ENABLE], lossy_chrm_en, 11, 1);

	/* hsize_in of afbc input*/
	/* vsize_in of afbc input*/
	if (flg_v5) {
		op->wr(reg[EAFBCE_SIZE_IN],
		       ((hsize_buf & 0x1fff) << 16) |
		       ((vsize_buf & 0x1fff) << 0));

	} else {
		op->wr(reg[EAFBCE_SIZE_IN],
		       ((cfg->hsize_in & 0x1fff) << 16) |
		       ((cfg->vsize_in & 0x1fff) << 0));
	}
	/* out blk hsize*/
	/* out blk vsize*/
	op->wr(reg[EAFBCE_BLK_SIZE_IN],
	       ((hblksize_out & 0x1fff) << 16) |
	       ((vblksize_out & 0x1fff) << 0)
	);
	if (DIM_IS_IC_EF(T7) || DIM_IS_IC(S4))
		op->wr(reg[EAFBCE_HEAD_BADDR], cfg->head_baddr >> 4);
	else
	/*head addr of compressed data*/
		op->wr(reg[EAFBCE_HEAD_BADDR], cfg->head_baddr);

	/*uncmp_size*/
	op->bwr(reg[EAFBCE_MIF_SIZE], (uncmp_size & 0x1fff), 16, 5);

	/* scope of hsize_in ,should be a integer multiple of 32*/
	/* scope of vsize_in ,should be a integer multiple of 4*/
	op->wr(reg[EAFBCE_PIXEL_IN_HOR_SCOPE],
	       ((cfg->enc_win_end_h & 0x1fff) << 16) |
	       ((cfg->enc_win_bgn_h & 0x1fff) << 0));

	/* scope of hsize_in ,should be a integer multiple of 32*/
	/* scope of vsize_in ,should be a integer multiple of 4*/
	op->wr(reg[EAFBCE_PIXEL_IN_VER_SCOPE],
	       ((cfg->enc_win_end_v & 0x1fff) << 16) |
	       ((cfg->enc_win_bgn_v & 0x1fff) << 0)
	);

	/*fix 256*/
	op->wr(reg[EAFBCE_CONV_CTRL], lbuf_depth);

	/* scope of out blk hsize*/
	/* scope of out blk vsize*/
	op->wr(reg[EAFBCE_MIF_HOR_SCOPE],
	       ((blk_out_bgn_h & 0x3ff) << 16) |
	       ((blk_out_end_h & 0x3ff) << 0)
	);

	/* scope of out blk hsize*/
	/* scope of out blk vsize*/
	op->wr(reg[EAFBCE_MIF_VER_SCOPE],
	       ((blk_out_bgn_v & 0xfff) << 16) |
	       ((blk_out_end_v & 0xfff) << 0)
	);

	op->wr(reg[EAFBCE_FORMAT],
	       (cfg->reg_format_mode	& 0x3) << 8 |
	       (cfg->reg_compbits_c	& 0xf) << 4 |
	       (cfg->reg_compbits_y	& 0xf));

	/* def_color_a*/
	/* def_color_y*/
	op->wr(reg[EAFBCE_DEFCOLOR_1],
	       ((def_color_3 & 0xfff) << 12) |
	       ((def_color_0 & 0xfff) << 0));

	/* def_color_v*/
	/* def_color_u*/
	if (cfg->reg_compbits_c == 8)
		def_color_2 = 0x80;
	else if (cfg->reg_compbits_c == 9)
		def_color_2 = 0x100;
	else
		def_color_2 = 0x200;

	def_color_1 = def_color_2;
	op->wr(reg[EAFBCE_DEFCOLOR_2],
	       ((def_color_2 & 0xfff) << 12) |
	       ((def_color_1 & 0xfff) << 0));
	op->wr(reg[EAFBCE_DUMMY_DATA],
	       ((def_color_2 & 0x3ff) << 10) |
	       ((def_color_2 & 0x3ff) << 0));
	/*4k addr have used in every frame;*/
	/*cur_mmu_used += Rd(DI_AFBCE_MMU_NUM);*/

	if (DIM_IS_IC_EF(T7) || DIM_IS_IC(S4))
		op->wr(reg[EAFBCE_MMU_RMIF_CTRL4], cfg->mmu_info_baddr >> 4);
	else
		op->wr(reg[EAFBCE_MMU_RMIF_CTRL4], cfg->mmu_info_baddr);

	/**/
	if (flg_v5)
		op->bwr(reg[EAFBCE_MMU_RMIF_CTRL1], 0x1, 6, 1);
		//litter_endia ary ??
	if (cfg->reg_pip_mode)
		op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], 0x0, 0, 13); /* ary ? */
	else
		op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], cur_mmu_used, 0, 12);

	/**/
	//op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], cur_mmu_used, 0, 12);
	op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], 0x1ffe, 16, 13);
	op->bwr(reg[EAFBCE_MMU_RMIF_CTRL3], 0x1fff, 0, 13);

	if (flg_v5) {
		op->bwr(reg[EAFBCE_PIP_CTRL], cfg->reg_init_ctrl, 1, 1);
		//pii_mode
		op->bwr(reg[EAFBCE_PIP_CTRL], cfg->reg_pip_mode, 0, 1);

		op->bwr(reg[EAFBCE_ROT_CTRL], cfg->rot_en, 4, 1);

		op->bwr(reg[EAFBCE_ENABLE], 0, 2, 1);//go_line_cnt start
		op->bwr(reg[EAFBCE_ENABLE], cfg->enable, 8, 1);//enable afbce
		op->bwr(reg[EAFBCE_ENABLE], cfg->enable, 0, 1);//enable afbce
		/* ary : not setting DI_AFBCE_CTRL ?*/

	} else {
		if (DIM_IS_IC(TM2B))//dis afbce mode from vlsi xianjun.fan
			op->bwr(reg[EAFBCE_MODE_EN], 0x01, 18, 1);
		op->bwr(reg[EAFBCE_ENABLE], cfg->enable, 8, 1);//enable afbce
		op->bwr(DI_AFBCE_CTRL, cfg->enable, 0, 1);//di pre to afbce
		//op->bwr(DI_AFBCE_CTRL, cfg->enable, 4, 1);//di pre to afbce
	}
}

static void afbce_sw(enum EAFBC_ENC enc, bool on, const struct reg_acc *op)
{
	const unsigned int *reg;
	//const struct reg_acc *op = &di_normal_regset;

	if (!op) {
		PR_ERR("%s:no op\n", __func__);
		return;
	}

	reg = &reg_afbc_e[enc][0];

	if (on) {
		op->bwr(reg[EAFBCE_ENABLE], 1, 8, 1);//enable afbce
		op->bwr(reg[EAFBCE_ENABLE], 1, 0, 1);//enable afbce
	} else {
		op->bwr(reg[EAFBCE_ENABLE], 0, 8, 1);//enable afbce
		op->bwr(reg[EAFBCE_ENABLE], 0, 0, 1);//enable afbce
	}
}

unsigned int afbce_read_used(enum EAFBC_ENC enc)
{
	const unsigned int *reg;
	const struct reg_acc *op = &di_normal_regset;
	unsigned int nub;

	reg = &reg_afbc_e[enc][0];

	nub = op->rd(reg[EAFBCE_MMU_NUM]);
	return nub;
}

/* set_di_afbce_cfg */
static void afbce_set(struct vframe_s *vf, enum EAFBC_ENC enc)
{
	struct enc_cfg_s cfg_data;
	struct enc_cfg_s *cfg = &cfg_data;
	struct di_buf_s	*di_buf;
	struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	bool flg_v5 = false;
	#ifdef DBG_CRC
	bool crc_right;
	#endif

	if (!vf) {
		pr_error("%s:0:no vf\n", __func__);
		return;
	}
	di_buf = (struct di_buf_s *)vf->private_data;

	if (!di_buf) {
		pr_error("%s:1:no di buf\n", __func__);
		return;
	}
	#ifdef DBG_CRC
	crc_right = dbg_checkcrc(di_buf);
	if (!crc_right /*&& (!dim_dbg_is_cnt())*/) {
		PR_INF("afbce[%d]s:t[%d:%d],inf[0x%lx],adr[0x%lx],vty[0x%x]\n",
		       enc,
		       di_buf->type, di_buf->index,
		       di_buf->afbc_adr, di_buf->afbct_adr, vf->type);
	}
	#endif
	di_buf->flg_afbce_set = 1;
	cfg->enable = 1;
	/* 0:close 1:luma lossy 2:chrma lossy 3: luma & chrma lossy*/
	cfg->head_baddr = di_buf->afbc_adr;//head_baddr_enc;/*head addr*/
	cfg->mmu_info_baddr = di_buf->afbct_adr;
	//vf->compHeadAddr = cfg->head_baddr;
	//vf->compBodyAddr = di_buf->nr_adr;
	#ifdef MARK_SC2
	if (enc == 1) {
		di_buf->vframe->height = di_buf->win.y_size;
		di_buf->vframe->width	= di_buf->win.x_size;
	}
	#endif
	vf_set_for_com(di_buf);
	if (cfgg(AFBCE_LOSS_EN) == 1 ||
	    ((di_buf->vframe->type & VIDTYPE_COMPRESS_LOSS) &&
	     cfgg(AFBCE_LOSS_EN) == 2))
		cfg->loosy_mode = 0x3;
#ifdef AFBCP
	di_print("%s:buf[%d],head[0x%lx],info[0x%lx]\n",
		 __func__,
		 di_buf->index,
		 cfg->head_baddr,
		 cfg->mmu_info_baddr);
#endif
	if (di_buf->afbce_out_yuv420_10)
		cfg->reg_format_mode = 2;
	else
		cfg->reg_format_mode = 1;/*0:444 1:422 2:420*/
	cfg->reg_compbits_y = 10;//8;/*bits num after compression*/
	cfg->reg_compbits_c = 10;//8;/*bits num after compression*/
#ifdef MARK_SC2
	if (enc == 1) {
		cfg->hsize_in = di_buf->win.x_size;
		cfg->vsize_in = di_buf->win.y_size;

		/*input scope*/
		cfg->enc_win_bgn_h = di_buf->win.x_st;
		cfg->enc_win_end_h = di_buf->win.x_st + cfg->hsize_in - 1;
		cfg->enc_win_bgn_v = di_buf->win.y_st;
		cfg->enc_win_end_v = di_buf->win.y_st + cfg->vsize_in - 1;
		//vf->height - 1;

	} else {
		/*input size*/
		cfg->hsize_in = vf->width;//src_w;
		if (is_src_i(vf) && enc == 0)
			cfg->vsize_in = vf->height >> 1;
		else
			cfg->vsize_in = vf->height;//src_h;
		/*input scope*/
		cfg->enc_win_bgn_h = 0;
		cfg->enc_win_end_h = vf->width - 1;
		cfg->enc_win_bgn_v = 0;
		cfg->enc_win_end_v = cfg->vsize_in - 1; //vf->height - 1;
	}
#endif
	/*input size*/
	cfg->hsize_in = vf->width;//src_w;
	if ((is_src_i(vf) && enc == 0) ||
	    (is_src_i(vf) && is_mask(SC2_MEM_CPY)))
		cfg->vsize_in = vf->height >> 1;
	else
		cfg->vsize_in = vf->height;//src_h;
	/*input scope*/
	cfg->enc_win_bgn_h = 0;
	cfg->enc_win_end_h = vf->width - 1;
	cfg->enc_win_bgn_v = 0;
	cfg->enc_win_end_v = cfg->vsize_in - 1; //vf->height - 1;
	if (is_mask(SC2_ROT_WR) && enc == 1)
		cfg->rot_en = 1;
	else
		cfg->rot_en = 0;

	dim_print("%s:win:<%d><%d><%d><%d><%d><%d>\n",
		  __func__,
		  cfg->enc_win_bgn_h,
		  cfg->hsize_in,
		  cfg->enc_win_bgn_v,
		  cfg->vsize_in,
		  cfg->enc_win_end_h,
		  cfg->enc_win_end_v);
	/*for sc2*/
	if (pafd_ctr->fb.ver >= AFBCD_V5)
		flg_v5 = true;
#ifdef MARK_SC2
	if (enc == 1) {
		cfg->reg_init_ctrl  = 1;//pip init frame flag
		cfg->reg_pip_mode   = 1;//pip open bit
		cfg->hsize_bgnd     = 1920;//hsize of background
		cfg->vsize_bgnd     = 1080;//hsize of background
	} else {
		cfg->reg_init_ctrl  = 0;//pip init frame flag
		cfg->reg_pip_mode   = 0;//pip open bit
		cfg->hsize_bgnd     = 0;//hsize of background
		cfg->vsize_bgnd     = 0;//hsize of background
	}
#endif
	cfg->reg_init_ctrl  = 0;//pip init frame flag
	cfg->reg_pip_mode   = 0;//pip open bit
	cfg->hsize_bgnd     = 0;//hsize of background
	cfg->vsize_bgnd     = 0;//hsize of background
	cfg->reg_ram_comb   = 0;//ram split bit open in di mult write case

	cfg->rev_mode	    = 0;//0:normal mode
	cfg->def_color_0    = 0;//def_color
	cfg->def_color_1    = 0;//def_color
	cfg->def_color_2    = 0;//def_color
	cfg->def_color_3    = 0;//def_color
	cfg->force_444_comb = 0;
	//cfg->rot_en	    = 0;
	cfg->din_swt	    = 0;

	ori_afbce_cfg(cfg, &di_normal_regset, enc);
}

static void afbce_update_level1(struct vframe_s *vf,
				const struct reg_acc *op,
				enum EAFBC_ENC enc)
{
	const unsigned int *reg;
	struct di_buf_s *di_buf;
	unsigned int cur_mmu_used = 0;
	#ifdef DBG_CRC
	bool crc_right;
	#endif
	di_buf = (struct di_buf_s *)vf->private_data;

	if (!di_buf) {
		pr_error("%s:1:no di buf\n", __func__);
		return;
	}
	#ifdef DBG_CRC // DBG_TEST_CRC
	crc_right = dbg_checkcrc(di_buf);
	if (!crc_right) {
		PR_INF("afbce[%d]s:t[%d:%d],inf[0x%lx],adr[0x%lx],vty[0x%x]\n",
		       enc,
		       di_buf->type, di_buf->index,
		       di_buf->afbc_adr, di_buf->afbct_adr, vf->type);
	}
	#endif
	reg = &reg_afbc_e[enc][0];
	dim_print("afbce:up:%d\n", enc);

	#ifdef MARK_SC2
	if (dim_dbg_is_cnt()) {
		PR_INF("afbce[%d]up:t[%d:%d],inf[0x%lx],adr[0x%lx],vty[0x%x]\n",
		       enc,
		       di_buf->type, di_buf->index,
		       di_buf->afbc_adr, di_buf->afbct_adr, vf->type);
	}
	#endif
	di_buf->flg_afbce_set = 1;

	//vf->compHeadAddr = di_buf->afbc_adr;
	//vf->compBodyAddr = di_buf->nr_adr;
	vf_set_for_com(di_buf);

	//head addr of compressed data
	if (DIM_IS_IC_EF(T7) || DIM_IS_IC(S4)) {
		op->wr(reg[EAFBCE_HEAD_BADDR], di_buf->afbc_adr >> 4);
		op->wr(reg[EAFBCE_MMU_RMIF_CTRL4], di_buf->afbct_adr >> 4);

	} else {
		op->wr(reg[EAFBCE_HEAD_BADDR], di_buf->afbc_adr);
		op->wr(reg[EAFBCE_MMU_RMIF_CTRL4], di_buf->afbct_adr);
	}
	op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], cur_mmu_used, 0, 12);
	op->bwr(reg[EAFBCE_MMU_RMIF_SCOPE_X], 0x1ffe, 16, 13);
	op->bwr(reg[EAFBCE_MMU_RMIF_CTRL3], 0x1fff, 0, 13);
	afbce_sw(enc, true, op);
}

void dbg_afbce_update_level1(struct vframe_s *vf,
			     const struct reg_acc *op,
			     enum EAFBC_ENC enc)
{
	afbce_update_level1(vf, op, enc);
}

