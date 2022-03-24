// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/di_prc.c
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
#include <linux/seq_file.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include "deinterlace.h"

#include "di_data_l.h"
#include "di_data.h"
#include "di_dbg.h"
#include "di_sys.h"
#include "di_vframe.h"
#include "di_que.h"
#include "di_task.h"

#include "di_prc.h"
#include "di_pre.h"
#include "di_post.h"
#include "di_api.h"
#include "di_hw_v3.h"
#include "reg_decontour.h"
#include "reg_decontour_t3.h"
#include "deinterlace_dbg.h"

#include <linux/amlogic/media/di/di.h>
//#include "../deinterlace/di_pqa.h"
bool di_forc_pq_load_later = true;
module_param(di_forc_pq_load_later, bool, 0664);
MODULE_PARM_DESC(di_forc_pq_load_later, "debug pq");

bool dim_dbg_is_force_later(void)
{
	return di_forc_pq_load_later;
}
unsigned int di_dbg = DBG_M_EVENT/*|DBG_M_IC|DBG_M_MEM2|DBG_M_RESET_PRE*/;
module_param(di_dbg, uint, 0664);
MODULE_PARM_DESC(di_dbg, "debug print");

/************************************************
 * dim_cfg
 *	[0] bypass_all_p
 ***********************************************/
#ifdef TEST_DISABLE_BYPASS_P
static unsigned int dim_cfg;
#else
static unsigned int dim_cfg = 1;
#endif
module_param_named(dim_cfg, dim_cfg, uint, 0664);

bool dim_dbg_cfg_post_byapss(void)
{
	if (dim_cfg & DI_BIT8)
		return true;
	return false;
}

bool dim_dbg_cfg_disable_arb(void)
{
	if (dim_cfg & DI_BIT9)
		return true;
	return false;
}
static unsigned int dim_dbg_dec21;
module_param_named(dim_dbg_dec21, dim_dbg_dec21, uint, 0644);
unsigned int dim_get_dbg_dec21(void)
{
	return dim_dbg_dec21;
}

bool dim_in_linear(void)
{
	return (dim_dbg_dec21 & 0x200) ? true : false;
}
/**************************************
 *
 * cfg ctr top
 *	bool
 **************************************/
static unsigned int dbg_unreg_flg;

const struct di_cfg_ctr_s di_cfg_top_ctr[K_DI_CFG_NUB] = {
	/*same order with enum eDI_DBG_CFG*/
	/* cfg for top */
	[EDI_CFG_BEGIN]  = {"cfg top begin ", EDI_CFG_BEGIN, 0,
			K_DI_CFG_T_FLG_NONE},
	[EDI_CFG_MEM_FLAG]  = {"flag_cma", EDI_CFG_MEM_FLAG,
				EDI_MEM_M_CMA,
				K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_FIRST_BYPASS]  = {"first_bypass",
				EDI_CFG_FIRST_BYPASS,
				0,
				K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_REF_2]  = {"ref_2",
		EDI_CFG_REF_2, 0, K_DI_CFG_T_FLG_NOTHING},
	[EDI_CFG_PMODE]  = {"pmode:0:as p;1:as i;2:use 2 i buf",
			EDI_CFG_PMODE,
			1,
			K_DI_CFG_T_FLG_NOTHING},
	[EDI_CFG_KEEP_CLEAR_AUTO]  = {"keep_buf clear auto",
		/* 0: default */
		/* 1: releas keep buffer when next reg */
		/* 2: not keep buffer */
			EDI_CFG_KEEP_CLEAR_AUTO,
			0,
			K_DI_CFG_T_FLG_NOTHING},
	[EDI_CFG_MEM_RELEASE_BLOCK_MODE]  = {"release mem use block mode",
			EDI_CFG_MEM_RELEASE_BLOCK_MODE,
			1,
			K_DI_CFG_T_FLG_NOTHING},
	[EDI_CFG_FIX_BUF]  = {"fix buf",
			EDI_CFG_FIX_BUF,
			0,
			K_DI_CFG_T_FLG_NOTHING},
	[EDI_CFG_PAUSE_SRC_CHG]  = {"pause when source chang",
			EDI_CFG_PAUSE_SRC_CHG,
			0,
			K_DI_CFG_T_FLG_NOTHING},
	[EDI_CFG_4K]  = {"en_4k",
			EDI_CFG_4K,
			/* 0: not support 4K;	*/
			/* 1: enable 4K		*/
			/* 2: dynamic: vdin: 4k enable, other, disable	*/
			/* bit[2:0]: for 4k enable		*/
			/* bit[3]: pps enable for 4k only	*/
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_POUT_FMT]  = {"po_fmt",
	/* 0:default; 1: nv21; 2: nv12; 3:afbce */
	/* 4: dynamic change p out put;	 4k:afbce, other:yuv422 10*/
	/* 5: dynamic: 4k: nv21, other yuv422 10bit */
	/* 6: dynamic: like 4: 4k: afbce yuv420, other:yuv422 10 */
			EDI_CFG_POUT_FMT,
			3,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_DAT]  = {"en_dat",/*bit 0: pst dat; bit 1: idat */
			EDI_CFG_DAT,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_ALLOC_SCT]  = {"alloc_sct",
		/* 0:not support;	*/
		/* bit 0: for 4k	*/
		/* bit 1: for 1080p	*/
			EDI_CFG_ALLOC_SCT,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_KEEP_DEC_VF]  = {"keep_dec_vf",
			/* 0:not keep; 1: keep dec vf for p; 2: dynamic*/
			EDI_CFG_KEEP_DEC_VF,
			1,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_POST_NUB]  = {"post_nub",
			/* 0:not config post nub;*/
			EDI_CFG_POST_NUB,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_BYPASS_MEM]  = {"bypass_mem",
			/* 0:not bypass;	*/
			/* 1:bypass;		*/
			/* 2: when 4k bypass	*/
			/* 3: when 4k bypass,but not from vdin */
			EDI_CFG_BYPASS_MEM,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_IOUT_FMT]  = {"io_fmt",
	/* 0:default; 1: nv21; 2: nv12; 3:afbce */
			EDI_CFG_IOUT_FMT,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_TMODE_1]  = {"tmode1",
			EDI_CFG_TMODE_1,
			2,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_TMODE_2]  = {"tmode2",
			EDI_CFG_TMODE_2,
			2,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_TMODE_3]  = {"tmode3",
			EDI_CFG_TMODE_3,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_LINEAR]  = {"linear",
			/* 0:canvans;	*/
			/* 1:linear;		*/
			EDI_CFG_LINEAR,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_PONLY_MODE]  = {"ponly_mode",
			/* 0:check the ponly flag in vf */
			/* 1:check the vf->type in first vf */
			EDI_CFG_PONLY_MODE,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_HF]  = {"hf",
			/* 0:not en;	*/
			/* 1:en;		*/
			EDI_CFG_HF,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_PONLY_BP_THD]  = {"bp_thd",
			/**/
			EDI_CFG_PONLY_BP_THD,
			2,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_T5DB_P_NOTNR_THD]  = {"t5db_nr_thd",
			/* bypass mem for p */
			EDI_CFG_T5DB_P_NOTNR_THD,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_DCT]  = {"dct",
			/* 0:not en;	*/
			/* 1:en;		*/
			EDI_CFG_DCT,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_T5DB_AFBCD_EN]  = {"t5db_afbcd_en",
			/* afbcd_en for t5db */
			/* 0:disable;	*/
			/* 1:enable;	*/
			EDI_CFG_T5DB_AFBCD_EN,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_HDR_EN]  = {"hdr_en",
			/* 0:disable;	*/
			/* 1:enable;	*/
			EDI_CFG_HDR_EN,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_DW_EN]  = {"dw_en",
			/* 0:disable;	*/
			/* bit 1:enable for 4k */
			/* bit 2:enable for 1080p to-do */
			EDI_CFG_DW_EN,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_SUB_V]  = {"sub_v",
			/* 0:major;	*/
			/* 1:sub */
			EDI_CFG_SUB_V,
			0,
			K_DI_CFG_T_FLG_DTS},
	[EDI_CFG_END]  = {"cfg top end ", EDI_CFG_END, 0,
			K_DI_CFG_T_FLG_NONE},

};

void di_cfg_set(enum ECFG_DIM idx, unsigned int data)
{
	if (idx == ECFG_DIM_BYPASS_P && !data)
		dim_cfg &= (~DI_BIT0);
}

char *di_cfg_top_get_name(enum EDI_CFG_TOP_IDX idx)
{
	return di_cfg_top_ctr[idx].dts_name;
}

void di_cfg_top_get_info(unsigned int idx, char **name)
{
	if (di_cfg_top_ctr[idx].id != idx)
		PR_ERR("%s:err:idx not map [%d->%d]\n", __func__,
		       idx, di_cfg_top_ctr[idx].id);

	*name = di_cfg_top_ctr[idx].dts_name;
}

bool di_cfg_top_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(di_cfg_top_ctr);
	if (idx >= tsize) {
		PR_ERR("%s:err:overflow:%d->%d\n",
		       __func__, idx, tsize);
		return false;
	}
	if (di_cfg_top_ctr[idx].flg & K_DI_CFG_T_FLG_NONE)
		return false;
	if (idx != di_cfg_top_ctr[idx].id) {
		PR_ERR("%s:%s:err:not map:%d->%d\n",
		       __func__,
		       di_cfg_top_ctr[idx].dts_name,
		       idx,
		       di_cfg_top_ctr[idx].id);
		return false;
	}
	return true;
}

void di_cfg_top_init_val(void)
{
	int i;
	union di_cfg_tdata_u *pd;
	const struct di_cfg_ctr_s *pt;

	dbg_reg("%s:\n", __func__);
	for (i = EDI_CFG_BEGIN; i < EDI_CFG_END; i++) {
		if (!di_cfg_top_check(i))
			continue;
		pd = &get_datal()->cfg_en[i];
		pt = &di_cfg_top_ctr[i];
		/*di_cfg_top_set(i, di_cfg_top_ctr[i].default_val);*/
		pd->d32 = 0;/*clear*/
		pd->b.val_df = pt->default_val;
		pd->b.val_c = pd->b.val_df;
	}
	dbg_reg("%s:finish\n", __func__);
}

void di_cfg_top_dts(void)
{
	struct platform_device *pdev = get_dim_de_devp()->pdev;
	int i;
	union di_cfg_tdata_u *pd;
	const struct di_cfg_ctr_s *pt;
	int ret;
	unsigned int uval;

	if (!pdev) {
		PR_ERR("%s:no pdev\n", __func__);
		return;
	}
	dbg_reg("%s\n", __func__);
	for (i = EDI_CFG_BEGIN; i < EDI_CFG_END; i++) {
		if (!di_cfg_top_check(i))
			continue;
		if (!(di_cfg_top_ctr[i].flg & K_DI_CFG_T_FLG_DTS))
			continue;
		pd = &get_datal()->cfg_en[i];
		pt = &di_cfg_top_ctr[i];
		pd->b.dts_en = 1;
		ret = of_property_read_u32(pdev->dev.of_node,
					   pt->dts_name,
					   &uval);
		if (ret)
			continue;
		dbg_reg("\t%s:%d\n", pt->dts_name, uval);
		pd->b.dts_have = 1;
		pd->b.val_dts = uval;
		pd->b.val_c = pd->b.val_dts;
	}
	//PR_INF("%s end\n", __func__);
	if (cfgg(4K) &&
	    (DIM_IS_IC_BF(TM2)	||
	     DIM_IS_IC(T5D)	||
	     DIM_IS_IC(T5DB)	||
	     DIM_IS_IC(S4))) {
		cfgs(4K, 0);
		PR_WARN("not support 4k\n");
	}
	if (DIM_IS_IC_EF(T7)) {
		cfgs(LINEAR, 1);
		dbg_reg("from t7 linear mode\n");
	}
	if (DIM_IS_IC(S4) && (cfgg(POUT_FMT) == 3)) {
		cfgs(POUT_FMT, 0);
		dbg_reg("s4 not support AFBCE\n");
	}
	if (cfgg(HF)) {
		if (DIM_IS_IC_EF(T3)) {
			dbg_reg("en:HF\n");
		} else {
			PR_WARN("not support:HF\n");
			cfgs(HF, 0);
		}
	}

	/* dat */
	/*bit 0: pst dat; bit 1: idat */
	pd = &get_datal()->cfg_en[EDI_CFG_DAT];
	pt = &di_cfg_top_ctr[EDI_CFG_DAT];
	if (DIM_IS_IC(TM2B)	||
	    DIM_IS_IC(SC2) || DIM_IS_IC(T5) ||
	    DIM_IS_IC(T7) ||
	    DIM_IS_IC(T3)) {
		if (!pd->b.dts_have) {
			pd->b.val_c = 0x3;
			//pd->b.val_c = 0x0;//test
		}
	} else {
		pd->b.val_c = 0;
	}
	dbg_reg("%s:%s:0x%x\n", __func__, pt->dts_name, pd->b.val_c);
	/* afbce and pout */
	if (!DIM_IS_IC(TM2B)	&&
	    !DIM_IS_IC(T5)	&&
	    DIM_IS_IC_BF(SC2)) {
		if (cfgg(ALLOC_SCT)) {
			PR_WARN("alloc_sct:not support:%d->0\n", cfgg(ALLOC_SCT));
			cfgs(ALLOC_SCT, 0);
		}
		if (cfgg(POUT_FMT) >= 3) {
			PR_WARN("pout:from:%d->0\n", cfgg(POUT_FMT));
			cfgs(POUT_FMT, 0);
		}
	}
	/* dw */
	if (cfgg(DW_EN) && !IS_IC_SUPPORT(DW)) {
		PR_WARN("dw not support\n");
		cfgs(DW_EN, 0);
	}
}

static void di_cfgt_show_item_one(struct seq_file *s, unsigned int index)
{
	union di_cfg_tdata_u *pd;
	const struct di_cfg_ctr_s *pt;

	if (!di_cfg_top_check(index))
		return;
	pd = &get_datal()->cfg_en[index];
	pt = &di_cfg_top_ctr[index];
	seq_printf(s, "id:%2d:%-10s\n", index, pt->dts_name);
	seq_printf(s, "\t%-5s:0x%2x[%d]\n",
		   "tdf", pt->default_val, pt->default_val);
	seq_printf(s, "\t%-5s:%d\n",
		   "tdts", pt->flg & K_DI_CFG_T_FLG_DTS);
	seq_printf(s, "\t%-5s:0x%-4x\n", "d32", pd->d32);
	seq_printf(s, "\t%-5s:0x%2x[%d]\n",
		   "vdf", pd->b.val_df, pd->b.val_df);
	seq_printf(s, "\t%-5s:0x%2x[%d]\n",
		   "vdts", pd->b.val_dts, pd->b.val_dts);
	seq_printf(s, "\t%-5s:0x%2x[%d]\n",
		   "vdbg", pd->b.val_dbg, pd->b.val_dbg);
	seq_printf(s, "\t%-5s:0x%2x[%d]\n", "vc", pd->b.val_c, pd->b.val_c);
	seq_printf(s, "\t%-5s:%d\n", "endts", pd->b.dts_en);
	seq_printf(s, "\t%-5s:%d\n", "hdts", pd->b.dts_have);
	seq_printf(s, "\t%-5s:%d\n", "hdbg", pd->b.dbg_have);
}

void di_cfgt_show_item_sel(struct seq_file *s)
{
	int i = get_datal()->cfg_sel;

	di_cfgt_show_item_one(s, i);
}

void di_cfgt_set_sel(unsigned int dbg_mode, unsigned int id)
{
	if (!di_cfg_top_check(id)) {
		PR_ERR("%s:%d is overflow\n", __func__, id);
		return;
	}
	get_datal()->cfg_sel = id;
	get_datal()->cfg_dbg_mode = dbg_mode;
}

void di_cfgt_show_item_all(struct seq_file *s)
{
	int i;

	for (i = EDI_CFG_BEGIN; i < EDI_CFG_END; i++)
		di_cfgt_show_item_one(s, i);
}

static void di_cfgt_show_val_one(struct seq_file *s, unsigned int index)
{
	union di_cfg_tdata_u *pd;
	const struct di_cfg_ctr_s *pt;

	if (!di_cfg_top_check(index))
		return;
	pd = &get_datal()->cfg_en[index];
	pt = &di_cfg_top_ctr[index];
	seq_printf(s, "id:%2d:%-10s\n", index, pt->dts_name);
	seq_printf(s, "\t%-5s:0x%-4x\n", "d32", pd->d32);
	seq_printf(s, "\t%-5s:0x%2x[%d]\n", "vc", pd->b.val_c, pd->b.val_c);
}

void di_cfgt_show_val_sel(struct seq_file *s)
{
	unsigned int i = get_datal()->cfg_sel;

	di_cfgt_show_val_one(s, i);
}

void di_cfgt_show_val_all(struct seq_file *s)
{
	int i;

	for (i = EDI_CFG_BEGIN; i < EDI_CFG_END; i++)
		di_cfgt_show_val_one(s, i);
}

unsigned int di_cfg_top_get(enum EDI_CFG_TOP_IDX id)
{
	union di_cfg_tdata_u *pd;

	pd = &get_datal()->cfg_en[id];
	return pd->b.val_c;
}

void di_cfg_top_set(enum EDI_CFG_TOP_IDX id, unsigned int val)
{
	union di_cfg_tdata_u *pd;

	pd = &get_datal()->cfg_en[id];
	pd->b.val_dbg = val;
	pd->b.dbg_have = 1;
	pd->b.val_c = val;
}

void di_cfg_cp_ch(struct di_ch_s *pch)
{
	int i;

	for (i = 0; i < EDI_CFG_END; i++)
		pch->cfg_cp[i] = di_cfg_top_get(i);
}

unsigned char di_cfg_cp_get(struct di_ch_s *pch, enum EDI_CFG_TOP_IDX id)
{
	return pch->cfg_cp[id];
}

void di_cfg_cp_set(struct di_ch_s *pch,
		   enum EDI_CFG_TOP_IDX id,
		   unsigned char val)
{
	pch->cfg_cp[id] = val;
}
/**************************************
 *
 * cfg ctr x
 *	bool
 **************************************/

const struct di_cfgx_ctr_s di_cfgx_ctr[K_DI_CFGX_NUB] = {
	/*same order with enum eDI_DBG_CFG*/

	/* cfg channel x*/
	[EDI_CFGX_BEGIN]  = {"cfg x begin ", EDI_CFGX_BEGIN, 0},
	/* bypass_all */
	[EDI_CFGX_BYPASS_ALL]  = {"bypass_all", EDI_CFGX_BYPASS_ALL, 0},
	[EDI_CFGX_END]  = {"cfg x end ", EDI_CFGX_END, 0},

	/* debug cfg x */
	[EDI_DBG_CFGX_BEGIN]  = {"cfg dbg begin ", EDI_DBG_CFGX_BEGIN, 0},
	[EDI_DBG_CFGX_IDX_VFM_IN] = {"vfm_in", EDI_DBG_CFGX_IDX_VFM_IN, 1},
	[EDI_DBG_CFGX_IDX_VFM_OT] = {"vfm_out", EDI_DBG_CFGX_IDX_VFM_OT, 1},
	[EDI_DBG_CFGX_END]    = {"cfg dbg end", EDI_DBG_CFGX_END, 0},
};

char *di_cfgx_get_name(enum EDI_CFGX_IDX idx)
{
	return di_cfgx_ctr[idx].name;
}

void di_cfgx_get_info(enum EDI_CFGX_IDX idx, char **name)
{
	if (di_cfgx_ctr[idx].id != idx)
		PR_ERR("%s:err:idx not map [%d->%d]\n", __func__,
		       idx, di_cfgx_ctr[idx].id);

	*name = di_cfgx_ctr[idx].name;
}

bool di_cfgx_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(di_cfgx_ctr);
	if (idx >= tsize) {
		PR_ERR("%s:err:overflow:%d->%d\n",
		       __func__, idx, tsize);
		return false;
	}
	if (idx != di_cfgx_ctr[idx].id) {
		PR_ERR("%s:err:not map:%d->%d\n",
		       __func__, idx, di_cfgx_ctr[idx].id);
		return false;
	}
	pr_info("\t%-15s=%d\n", di_cfgx_ctr[idx].name,
		di_cfgx_ctr[idx].default_val);
	return true;
}

void di_cfgx_init_val(void)
{
	int i, ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		for (i = EDI_CFGX_BEGIN; i < EDI_DBG_CFGX_END; i++)
			di_cfgx_set(ch, i, di_cfgx_ctr[i].default_val);
	}
}

bool di_cfgx_get(unsigned int ch, enum EDI_CFGX_IDX idx)
{
	return get_datal()->ch_data[ch].cfgx_en[idx];
}

void di_cfgx_set(unsigned int ch, enum EDI_CFGX_IDX idx, bool en)
{
	get_datal()->ch_data[ch].cfgx_en[idx] = en;
}

/**************************************
 *
 * module para top
 *	int
 **************************************/

const struct di_mp_uit_s di_mp_ui_top[] = {
	/*same order with enum eDI_MP_UI*/
	/* for top */
	[EDI_MP_UI_T_BEGIN]  = {"module para top begin ",
			EDI_MP_UI_T_BEGIN, 0},
	/**************************************/
	[EDI_MP_SUB_DI_B]  = {"di begin ",
			EDI_MP_SUB_DI_B, 0},
	[edi_mp_force_prog]  = {"bool:force_prog:1",
			edi_mp_force_prog, 1},
	[edi_mp_combing_fix_en]  = {"bool:combing_fix_en,def:1",
			edi_mp_combing_fix_en, 1},
	[edi_mp_cur_lev]  = {"int cur_lev,def:2",
			edi_mp_cur_lev, 2},
	[edi_mp_pps_dstw]  = {"pps_dstw:int",
			edi_mp_pps_dstw, 0},
	[edi_mp_pps_dsth]  = {"pps_dsth:int",
			edi_mp_pps_dsth, 0},
	[edi_mp_pps_en]  = {"pps_en:bool",
			edi_mp_pps_en, 0},
	[edi_mp_pps_position]  = {"pps_position:uint:def:1",
			edi_mp_pps_position, 1},
	[edi_mp_pre_enable_mask]  = {"pre_enable_mask:bit0:ma;bit1:mc:def:3",
			edi_mp_pre_enable_mask, 3},
	[edi_mp_post_refresh]  = {"post_refresh:bool",
			edi_mp_post_refresh, 0},
	[edi_mp_nrds_en]  = {"nrds_en:bool",
			edi_mp_nrds_en, 0},
	[edi_mp_bypass_3d]  = {"bypass_3d:int:def:1",
			edi_mp_bypass_3d, 1},
	[edi_mp_bypass_trick_mode]  = {"bypass_trick_mode:int:def:1",
			edi_mp_bypass_trick_mode, 1},
	[edi_mp_invert_top_bot]  = {"invert_top_bot:int",
			edi_mp_invert_top_bot, 0},
	[edi_mp_skip_top_bot]  = {"skip_top_bot:int:",
			/*1or2: may affect atv when bypass di*/
			edi_mp_skip_top_bot, 0},
	[edi_mp_force_width]  = {"force_width:int",
			edi_mp_force_width, 0},
	[edi_mp_force_height]  = {"force_height:int",
			edi_mp_force_height, 0},
	[edi_mp_prog_proc_config]  = {"prog_proc_config:int:def:0x",
/* prog_proc_config,
 * bit[2:1]: when two field buffers are used,
 * 0 use vpp for blending ,
 * 1 use post_di module for blending
 * 2 debug mode, bob with top field
 * 3 debug mode, bot with bot field
 * bit[0]:
 * 0 "prog vdin" use two field buffers,
 * 1 "prog vdin" use single frame buffer
 * bit[4]:
 * 0 "prog frame from decoder/vdin" use two field buffers,
 * 1 use single frame buffer
 * bit[5]:
 * when two field buffers are used for decoder (bit[4] is 0):
 * 1,handle prog frame as two interlace frames
 * bit[6]:(bit[4] is 0,bit[5] is 0,use_2_interlace_buff is 0): 0,
 * process progress frame as field,blend by post;
 * 1, process progress frame as field,process by normal di
 */
			edi_mp_prog_proc_config, ((1 << 5) | (1 << 1) | 1)},
	[edi_mp_start_frame_drop_count]  = {"start_frame_drop_count:int:2",
			edi_mp_start_frame_drop_count, 0},
	[edi_mp_same_field_top_count]  = {"same_field_top_count:long?",
			edi_mp_same_field_top_count, 0},
	[edi_mp_same_field_bot_count]  = {"same_field_bot_count:long?",
			edi_mp_same_field_bot_count, 0},
	[edi_mp_vpp_3d_mode]  = {"vpp_3d_mode:int",
			edi_mp_vpp_3d_mode, 0},
	[edi_mp_force_recovery_count]  = {"force_recovery_count:uint",
			edi_mp_force_recovery_count, 0},
	[edi_mp_pre_process_time]  = {"pre_process_time:int",
			edi_mp_pre_process_time, 0},
	[edi_mp_bypass_post]  = {"bypass_post:int",
			edi_mp_bypass_post, 0},
	[edi_mp_post_wr_en]  = {"post_wr_en:bool:1",
			edi_mp_post_wr_en, 1},
	[edi_mp_post_wr_support]  = {"post_wr_support:uint",
			edi_mp_post_wr_support, 0},
	[edi_mp_bypass_post_state]  = {"bypass_post_state:int",
/* 0, use di_wr_buf still;
 * 1, call dim_pre_de_done_buf_clear to clear di_wr_buf;
 * 2, do nothing
 */
			edi_mp_bypass_post_state, 0},
	[edi_mp_use_2_interlace_buff]  = {"use_2_interlace_buff:int",
			edi_mp_use_2_interlace_buff, 0},
	[edi_mp_debug_blend_mode]  = {"debug_blend_mode:int:-1",
			edi_mp_debug_blend_mode, -1},
	[edi_mp_nr10bit_support]  = {"nr10bit_support:uint",
		/* 0: not support nr10bit, 1: support nr10bit */
			edi_mp_nr10bit_support, 0},
	[edi_mp_di_stop_reg_flag]  = {"di_stop_reg_flag:uint",
			edi_mp_di_stop_reg_flag, 0},
	[edi_mp_mcpre_en]  = {"mcpre_en:bool:true",
			edi_mp_mcpre_en, 1},
	[edi_mp_check_start_drop]  = {"check_start_drop_prog:bool",
			edi_mp_check_start_drop, 0},
	[edi_mp_overturn]  = {"overturn:bool:?",
			edi_mp_overturn, 0},
	[edi_mp_full_422_pack]  = {"full_422_pack:bool",
			edi_mp_full_422_pack, 0},
	[edi_mp_cma_print]  = {"cma_print:bool:1",
			edi_mp_cma_print, 0},
	[edi_mp_pulldown_enable]  = {"pulldown_enable:bool:1",
			edi_mp_pulldown_enable, 1},
	[edi_mp_di_force_bit_mode]  = {"di_force_bit_mode:uint:10",
			edi_mp_di_force_bit_mode, 10},
	[edi_mp_calc_mcinfo_en]  = {"calc_mcinfo_en:bool:1",
			edi_mp_calc_mcinfo_en, 1},
	[edi_mp_colcfd_thr]  = {"colcfd_thr:uint:128",
			edi_mp_colcfd_thr, 128},
	[edi_mp_post_blend]  = {"post_blend:uint",
			edi_mp_post_blend, 0},
	[edi_mp_post_ei]  = {"post_ei:uint",
			edi_mp_post_ei, 0},
	[edi_mp_post_cnt]  = {"post_cnt:uint",
			edi_mp_post_cnt, 0},
	[edi_mp_di_log_flag]  = {"di_log_flag:uint",
			edi_mp_di_log_flag, 0},
	[edi_mp_di_debug_flag]  = {"di_debug_flag:uint",
			edi_mp_di_debug_flag, 0},
	[edi_mp_buf_state_log_threshold]  = {"buf_state_log_threshold:unit:16",
			edi_mp_buf_state_log_threshold, 16},
	[edi_mp_di_vscale_skip_enable]  = {"di_vscale_skip_enable:int",
/*
 * bit[2]: enable bypass all when skip
 * bit[1:0]: enable bypass post when skip
 */
			edi_mp_di_vscale_skip_enable, 0},
	[edi_mp_di_vscale_skip_count]  = {"di_vscale_skip_count:int",
			edi_mp_di_vscale_skip_count, 0},
	[edi_mp_di_vscale_skip_real]  = {"di_vscale_skip_count_real:int",
			edi_mp_di_vscale_skip_real, 0},
	[edi_mp_det3d_en]  = {"det3d_en:bool",
			edi_mp_det3d_en, 0},
	[edi_mp_post_hold_line]  = {"post_hold_line:int:8",
			edi_mp_post_hold_line, 8},
	[edi_mp_post_urgent]  = {"post_urgent:int:1",
			edi_mp_post_urgent, 1},
	[edi_mp_di_printk_flag]  = {"di_printk_flag:uint",
			edi_mp_di_printk_flag, 0},
	[edi_mp_force_recovery]  = {"force_recovery:uint:1",
			edi_mp_force_recovery, 1},
	[edi_mp_di_dbg_mask]  = {"di_dbg_mask:uint:0x02",
			edi_mp_di_dbg_mask, 2},
	[edi_mp_nr_done_check_cnt]  = {"nr_done_check_cnt:uint:5",
			edi_mp_nr_done_check_cnt, 5},
	[edi_mp_pre_hsc_down_en]  = {"pre_hsc_down_en:bool:0",
			edi_mp_pre_hsc_down_en, 0},
	[edi_mp_pre_hsc_down_width]  = {"pre_hsc_down_width:int:480",
				edi_mp_pre_hsc_down_width, 480},
	[edi_mp_show_nrwr]  = {"show_nrwr:bool:0",
				edi_mp_show_nrwr, 0},

	/******deinterlace_hw.c**********/
	[edi_mp_pq_load_dbg]  = {"pq_load_dbg:uint",
			edi_mp_pq_load_dbg, 0},
	[edi_mp_lmv_lock_win_en]  = {"lmv_lock_win_en:bool",
			edi_mp_lmv_lock_win_en, 0},
	[edi_mp_lmv_dist]  = {"lmv_dist:short:5",
			edi_mp_lmv_dist, 5},
	[edi_mp_pr_mcinfo_cnt]  = {"pr_mcinfo_cnt:ushort",
			edi_mp_pr_mcinfo_cnt, 0},
	[edi_mp_offset_lmv]  = {"offset_lmv:short:100",
			edi_mp_offset_lmv, 100},
	[edi_mp_post_ctrl]  = {"post_ctrl:uint",
			edi_mp_post_ctrl, 0},
	[edi_mp_if2_disable]  = {"if2_disable:bool",
			edi_mp_if2_disable, 0},
	[edi_mp_pre]  = {"pre_flag:ushort:2",
			edi_mp_pre, 2},
	[edi_mp_pre_mif_gate]  = {"pre_mif_gate:bool",
			edi_mp_pre_mif_gate, 0},
	[edi_mp_pre_urgent]  = {"pre_urgent:ushort",
			edi_mp_pre_urgent, 0},
	[edi_mp_pre_hold_line]  = {"pre_hold_line:ushort:10",
			edi_mp_pre_hold_line, 10},
	[edi_mp_pre_ctrl]  = {"pre_ctrl:uint",
			edi_mp_pre_ctrl, 0},
	[edi_mp_line_num_post_frst]  = {"line_num_post_frst:ushort:5",
			edi_mp_line_num_post_frst, 5},
	[edi_mp_line_num_pre_frst]  = {"line_num_pre_frst:ushort:5",
			edi_mp_line_num_pre_frst, 5},
	[edi_mp_pd22_flg_calc_en]  = {"pd22_flg_calc_en:bool:true",
			edi_mp_pd22_flg_calc_en, 1},
	[edi_mp_mcen_mode]  = {"mcen_mode:ushort:1",
			edi_mp_mcen_mode, 1},
	[edi_mp_mcuv_en]  = {"mcuv_en:ushort:1",
			edi_mp_mcuv_en, 1},
	[edi_mp_mcdebug_mode]  = {"mcdebug_mode:ushort",
			edi_mp_mcdebug_mode, 0},
	[edi_mp_pldn_ctrl_rflsh]  = {"pldn_ctrl_rflsh:uint:1",
			edi_mp_pldn_ctrl_rflsh, 1},
	[edi_mp_pstcrc_ctrl]  = {"edi_mp_pstcrc_ctrl:uint:1",
			edi_mp_pstcrc_ctrl, 1},
	[edi_mp_hdr_en]  = {"hdr_en:bool:0",
			edi_mp_hdr_en, 0},
	[edi_mp_hdr_mode]  = {"hdr_mode:uint:1:HDR_BYPASS",
			edi_mp_hdr_mode, 1},
	[edi_mp_hdr_ctrl]  = {"hdr_ctrl:uint:0",
			edi_mp_hdr_ctrl, 0},
	[edi_mp_clock_low_ratio]  = {"clock_low_ratio:0:not set",
				edi_mp_clock_low_ratio, 60000000},
	/********************
	 * edi_mp_shr_cfg
	 * b[31]	cfg enable
	 * b[7:0]	mode;
	 *	[7]:	en
	 *	[4:5]:	v
	 *	[0:1]:	h
	 * b[15:8]:	o mode; [15]: en;
	 * b[16]:	show dw;
	 ********************/
	[edi_mp_shr_cfg]  = {"shr_mode_w:uint:0:disable;0",
				edi_mp_shr_cfg, 0x0},
	[EDI_MP_SUB_DI_E]  = {"di end-------",
				EDI_MP_SUB_DI_E, 0},
	/**************************************/
	[EDI_MP_SUB_NR_B]  = {"nr begin",
			EDI_MP_SUB_NR_B, 0},
	[edi_mp_dnr_en]  = {"dnr_en:bool:true",
			edi_mp_dnr_en, 1},
	[edi_mp_nr2_en]  = {"nr2_en:uint:1",
			edi_mp_nr2_en, 1},
	[edi_mp_cue_en]  = {"cue_en:bool:true",
			edi_mp_cue_en, 1},
	[edi_mp_invert_cue_phase]  = {"invert_cue_phase:bool",
			edi_mp_invert_cue_phase, 0},
	[edi_mp_cue_pr_cnt]  = {"cue_pr_cnt:uint",
			edi_mp_cue_pr_cnt, 0},
	[edi_mp_cue_glb_mot_check_en]  = {"cue_glb_mot_check_en:bool:true",
			edi_mp_cue_glb_mot_check_en, 1},
	[edi_mp_glb_fieldck_en]  = {"glb_fieldck_en:bool:true",
			edi_mp_glb_fieldck_en, 1},
	[edi_mp_dnr_pr]  = {"dnr_pr:bool",
			edi_mp_dnr_pr, 0},
	[edi_mp_dnr_dm_en]  = {"dnr_dm_en:bool",
			edi_mp_dnr_dm_en, 0},
	[EDI_MP_SUB_NR_E]  = {"nr end-------",
			EDI_MP_SUB_NR_E, 0},
	/**************************************/
	[EDI_MP_SUB_PD_B]  = {"pd begin",
			EDI_MP_SUB_PD_B, 0},
	[edi_mp_flm22_ratio]  = {"flm22_ratio:uint:200",
			edi_mp_flm22_ratio, 200},
	[edi_mp_pldn_cmb0]  = {"pldn_cmb0:uint:1",
			edi_mp_pldn_cmb0, 1},
	[edi_mp_pldn_cmb1]  = {"pldn_cmb1:uint",
			edi_mp_pldn_cmb1, 0},
	[edi_mp_flm22_sure_num]  = {"flm22_sure_num:uint:100",
			edi_mp_flm22_sure_num, 100},
	[edi_mp_flm22_glbpxlnum_rat]  = {"flm22_glbpxlnum_rat:uint:4",
			edi_mp_flm22_glbpxlnum_rat, 4},
	[edi_mp_flag_di_weave]  = {"flag_di_weave:int:1",
			edi_mp_flag_di_weave, 1},
	[edi_mp_flm22_glbpxl_maxrow]  = {"flm22_glbpxl_maxrow:uint:16",
			edi_mp_flm22_glbpxl_maxrow, 16},
	[edi_mp_flm22_glbpxl_minrow]  = {"flm22_glbpxl_minrow:uint:3",
			edi_mp_flm22_glbpxl_minrow, 3},
	[edi_mp_cmb_3point_rnum]  = {"cmb_3point_rnum:uint",
			edi_mp_cmb_3point_rnum, 0},
	[edi_mp_cmb_3point_rrat]  = {"cmb_3point_rrat:unit:32",
			edi_mp_cmb_3point_rrat, 32},
	/********************************/
	[edi_mp_pr_pd]  = {"pr_pd:uint",
			edi_mp_pr_pd, 0},
	[edi_mp_prt_flg]  = {"prt_flg:bool",
			edi_mp_prt_flg, 0},
	[edi_mp_flmxx_maybe_num]  = {"flmxx_maybe_num:uint:15",
	/* if flmxx level > flmxx_maybe_num */
	/* mabye flmxx: when 2-2 3-2 not detected */
			edi_mp_flmxx_maybe_num, 15},
	[edi_mp_flm32_mim_frms]  = {"flm32_mim_frms:int:6",
			edi_mp_flm32_mim_frms, 6},
	[edi_mp_flm22_dif01a_flag]  = {"flm22_dif01a_flag:int:1",
			edi_mp_flm22_dif01a_flag, 1},
	[edi_mp_flm22_mim_frms]  = {"flm22_mim_frms:int:60",
			edi_mp_flm22_mim_frms, 60},
	[edi_mp_flm22_mim_smfrms]  = {"flm22_mim_smfrms:int:40",
			edi_mp_flm22_mim_smfrms, 40},
	[edi_mp_flm32_f2fdif_min0]  = {"flm32_f2fdif_min0:int:11",
			edi_mp_flm32_f2fdif_min0, 11},
	[edi_mp_flm32_f2fdif_min1]  = {"flm32_f2fdif_min1:int:11",
			edi_mp_flm32_f2fdif_min1, 11},
	[edi_mp_flm32_chk1_rtn]  = {"flm32_chk1_rtn:int:25",
			edi_mp_flm32_chk1_rtn, 25},
	[edi_mp_flm32_ck13_rtn]  = {"flm32_ck13_rtn:int:8",
			edi_mp_flm32_ck13_rtn, 8},
	[edi_mp_flm32_chk2_rtn]  = {"flm32_chk2_rtn:int:16",
			edi_mp_flm32_chk2_rtn, 16},
	[edi_mp_flm32_chk3_rtn]  = {"flm32_chk3_rtn:int:16",
			edi_mp_flm32_chk3_rtn, 16},
	[edi_mp_flm32_dif02_ratio]  = {"flm32_dif02_ratio:int:8",
			edi_mp_flm32_dif02_ratio, 8},
	[edi_mp_flm22_chk20_sml]  = {"flm22_chk20_sml:int:6",
			edi_mp_flm22_chk20_sml, 6},
	[edi_mp_flm22_chk21_sml]  = {"flm22_chk21_sml:int:6",
			edi_mp_flm22_chk21_sml, 6},
	[edi_mp_flm22_chk21_sm2]  = {"flm22_chk21_sm2:int:10",
			edi_mp_flm22_chk21_sm2, 10},
	[edi_mp_flm22_lavg_sft]  = {"flm22_lavg_sft:int:4",
			edi_mp_flm22_lavg_sft, 4},
	[edi_mp_flm22_lavg_lg]  = {"flm22_lavg_lg:int:24",
			edi_mp_flm22_lavg_lg, 24},
	[edi_mp_flm22_stl_sft]  = {"flm22_stl_sft:int:7",
			edi_mp_flm22_stl_sft, 7},
	[edi_mp_flm22_chk5_avg]  = {"flm22_chk5_avg:int:50",
			edi_mp_flm22_chk5_avg, 50},
	[edi_mp_flm22_chk6_max]  = {"flm22_chk6_max:int:20",
			edi_mp_flm22_chk6_max, 20},
	[edi_mp_flm22_anti_chk1]  = {"flm22_anti_chk1:int:61",
			edi_mp_flm22_anti_chk1, 61},
	[edi_mp_flm22_anti_chk3]  = {"flm22_anti_chk3:int:140",
			edi_mp_flm22_anti_chk3, 140},
	[edi_mp_flm22_anti_chk4]  = {"flm22_anti_chk4:int:128",
			edi_mp_flm22_anti_chk4, 128},
	[edi_mp_flm22_anti_ck140]  = {"flm22_anti_ck140:int:32",
			edi_mp_flm22_anti_ck140, 32},
	[edi_mp_flm22_anti_ck141]  = {"flm22_anti_ck141:int:80",
			edi_mp_flm22_anti_ck141, 80},
	[edi_mp_flm22_frmdif_max]  = {"flm22_frmdif_max:int:50",
			edi_mp_flm22_frmdif_max, 50},
	[edi_mp_flm22_flddif_max]  = {"flm22_flddif_max:int:100",
			edi_mp_flm22_flddif_max, 100},
	[edi_mp_flm22_minus_cntmax]  = {"flm22_minus_cntmax:int:2",
			edi_mp_flm22_minus_cntmax, 2},
	[edi_mp_flagdif01chk]  = {"flagdif01chk:int:1",
			edi_mp_flagdif01chk, 1},
	[edi_mp_dif01_ratio]  = {"dif01_ratio:int:10",
			edi_mp_dif01_ratio, 10},
	/*************vof_soft_top**************/
	[edi_mp_cmb32_blw_wnd]  = {"cmb32_blw_wnd:int:180",
			edi_mp_cmb32_blw_wnd, 180},
	[edi_mp_cmb32_wnd_ext]  = {"cmb32_wnd_ext:int:11",
			edi_mp_cmb32_wnd_ext, 11},
	[edi_mp_cmb32_wnd_tol]  = {"cmb32_wnd_tol:int:4",
			edi_mp_cmb32_wnd_tol, 4},
	[edi_mp_cmb32_frm_nocmb]  = {"cmb32_frm_nocmb:int:40",
			edi_mp_cmb32_frm_nocmb, 40},
	[edi_mp_cmb32_min02_sft]  = {"cmb32_min02_sft:int:7",
			edi_mp_cmb32_min02_sft, 7},
	[edi_mp_cmb32_cmb_tol]  = {"cmb32_cmb_tol:int:10",
			edi_mp_cmb32_cmb_tol, 10},
	[edi_mp_cmb32_avg_dff]  = {"cmb32_avg_dff:int:48",
			edi_mp_cmb32_avg_dff, 48},
	[edi_mp_cmb32_smfrm_num]  = {"cmb32_smfrm_num:int:4",
			edi_mp_cmb32_smfrm_num, 4},
	[edi_mp_cmb32_nocmb_num]  = {"cmb32_nocmb_num:int:20",
			edi_mp_cmb32_nocmb_num, 20},
	[edi_mp_cmb22_gcmb_rnum]  = {"cmb22_gcmb_rnum:int:8",
			edi_mp_cmb22_gcmb_rnum, 8},
	[edi_mp_flmxx_cal_lcmb]  = {"flmxx_cal_lcmb:int:1",
			edi_mp_flmxx_cal_lcmb, 1},
	[edi_mp_flm2224_stl_sft]  = {"flm2224_stl_sft:int:7",
			edi_mp_flm2224_stl_sft, 7},
	[EDI_MP_SUB_PD_E]  = {"pd end------",
			EDI_MP_SUB_PD_E, 0},
	/**************************************/
	[EDI_MP_SUB_MTN_B]  = {"mtn begin",
			EDI_MP_SUB_MTN_B, 0},
	[edi_mp_force_lev]  = {"force_lev:int:0xff",
			edi_mp_force_lev, 0xff},
	[edi_mp_dejaggy_flag]  = {"dejaggy_flag:int:-1",
			edi_mp_dejaggy_flag, -1},
	[edi_mp_dejaggy_enable]  = {"dejaggy_enable:int:1",
			edi_mp_dejaggy_enable, 1},
	[edi_mp_cmb_adpset_cnt]  = {"cmb_adpset_cnt:int",
			edi_mp_cmb_adpset_cnt, 0},
	[edi_mp_cmb_num_rat_ctl4]  = {"cmb_num_rat_ctl4:int:64:0~255",
			edi_mp_cmb_num_rat_ctl4, 64},
	[edi_mp_cmb_rat_ctl4_minthd]  = {"cmb_rat_ctl4_minthd:int:64",
			edi_mp_cmb_rat_ctl4_minthd, 64},
	[edi_mp_small_local_mtn]  = {"small_local_mtn:uint:70",
			edi_mp_small_local_mtn, 70},
	[edi_mp_di_debug_readreg]  = {"di_debug_readreg:int",
			edi_mp_di_debug_readreg, 0},
	[EDI_MP_SUB_MTN_E]  = {"mtn end----",
			EDI_MP_SUB_MTN_E, 0},
	/**************************************/
	[EDI_MP_SUB_3D_B]  = {"3d begin",
			EDI_MP_SUB_3D_B, 0},
	[edi_mp_chessbd_vrate]  = {"chessbd_vrate:int:29",
				edi_mp_chessbd_vrate, 29},
	[edi_mp_det3d_debug]  = {"det3d_debug:bool:0",
				edi_mp_det3d_debug, 0},
	[EDI_MP_SUB_3D_E]  = {"3d begin",
				EDI_MP_SUB_3D_E, 0},

	/**************************************/
	[EDI_MP_UI_T_END]  = {"module para top end ", EDI_MP_UI_T_END, 0},
};

bool di_mp_uit_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(di_mp_ui_top);
	if (idx >= tsize) {
		PR_ERR("%s:err:overflow:%d->%d\n",
		       __func__, idx, tsize);
		return false;
	}
	if (idx != di_mp_ui_top[idx].id) {
		PR_ERR("%s:err:not map:%d->%d\n",
		       __func__, idx, di_mp_ui_top[idx].id);
		return false;
	}
	dbg_init("\t%-15s=%d\n", di_mp_ui_top[idx].name,
		 di_mp_ui_top[idx].default_val);
	return true;
}

char *di_mp_uit_get_name(enum EDI_MP_UI_T idx)
{
	return di_mp_ui_top[idx].name;
}

void di_mp_uit_init_val(void)
{
	int i;

	for (i = EDI_MP_UI_T_BEGIN; i < EDI_MP_UI_T_END; i++) {
		if (!di_mp_uit_check(i))
			continue;
		di_mp_uit_set(i, di_mp_ui_top[i].default_val);
	}
}

int di_mp_uit_get(enum EDI_MP_UI_T idx)
{
	return get_datal()->mp_uit[idx];
}

void di_mp_uit_set(enum EDI_MP_UI_T idx, int val)
{
	get_datal()->mp_uit[idx] = val;
}

/************************************************
 * asked by pq tune
 ************************************************/
static bool dimpulldown_enable = true;
module_param_named(dimpulldown_enable, dimpulldown_enable, bool, 0664);

static bool dimmcpre_en = true;
module_param_named(dimmcpre_en, dimmcpre_en, bool, 0664);

static unsigned int dimmcen_mode = 1;
module_param_named(dimmcen_mode, dimmcen_mode, uint, 0664);
/************************************************/

void dim_mp_update_reg(void)
{
	int val;

	val = dimp_get(edi_mp_pulldown_enable);
	if (dimpulldown_enable != val) {
		PR_INF("mp:pulldown_enable: %d -> %d\n",
		       val, dimpulldown_enable);
		dimp_set(edi_mp_pulldown_enable, dimpulldown_enable);
	}

	val = dimp_get(edi_mp_mcpre_en);
	if (dimmcpre_en != val) {
		PR_INF("mp:dimmcpre_en: %d -> %d\n",
		       val, dimmcpre_en);
		dimp_set(edi_mp_mcpre_en, dimmcpre_en);
	}

	val = dimp_get(edi_mp_mcen_mode);
	if (dimmcen_mode != val) {
		PR_INF("mp:dimmcen_mode: %d -> %d\n",
		       val, dimmcen_mode);
		dimp_set(edi_mp_mcen_mode, dimmcen_mode);
	}
}

void dim_mp_update_post(void)
{
	int val;

	val = dimp_get(edi_mp_mcen_mode);
	if (dimmcen_mode != val) {
		PR_INF("mp:dimmcen_mode: %d -> %d\n",
		       val, dimmcen_mode);
		dimp_set(edi_mp_mcen_mode, dimmcen_mode);
	}
}

/**************************************
 *
 * module para x
 *	unsigned int
 **************************************/
const struct di_mp_uix_s di_mpx[] = {
	/*same order with enum eDI_MP_UI*/

	/* module para for channel x*/
	[EDI_MP_UIX_BEGIN]  = {"module para x begin ", EDI_MP_UIX_BEGIN, 0},
	/*debug:	run_flag*/
	[EDI_MP_UIX_RUN_FLG]  = {"run_flag(0:run;1:pause;2:step)",
				EDI_MP_UIX_RUN_FLG, DI_RUN_FLAG_RUN},
	[EDI_MP_UIX_END]  = {"module para x end ", EDI_MP_UIX_END, 0},

};

bool di_mp_uix_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(di_mpx);
	if (idx >= tsize) {
		PR_ERR("%s:err:overflow:%d->%d\n",
		       __func__, idx, tsize);
		return false;
	}
	if (idx != di_mpx[idx].id) {
		PR_ERR("%s:err:not map:%d->%d\n",
		       __func__, idx, di_mpx[idx].id);
		return false;
	}
	dbg_init("\t%-15s=%d\n", di_mpx[idx].name, di_mpx[idx].default_val);

	return true;
}

char *di_mp_uix_get_name(enum EDI_MP_UIX_T idx)
{
	return di_mpx[idx].name;
}

void di_mp_uix_init_val(void)
{
	int i, ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		dbg_init("%s:ch[%d]\n", __func__, ch);
		for (i = EDI_MP_UIX_BEGIN; i < EDI_MP_UIX_END; i++) {
			if (ch == 0) {
				if (!di_mp_uix_check(i))
					continue;
			}
			di_mp_uix_set(ch, i, di_mpx[i].default_val);
		}
	}
}

unsigned int di_mp_uix_get(unsigned int ch, enum EDI_MP_UIX_T idx)
{
	return get_datal()->ch_data[ch].mp_uix[idx];
}

void di_mp_uix_set(unsigned int ch, enum EDI_MP_UIX_T idx, unsigned int val)
{
	get_datal()->ch_data[ch].mp_uix[idx] = val;
}

bool di_is_pause(unsigned int ch)
{
	unsigned int run_flag;

	run_flag = di_mp_uix_get(ch, EDI_MP_UIX_RUN_FLG);

	if (run_flag == DI_RUN_FLAG_PAUSE	||
	    run_flag == DI_RUN_FLAG_STEP_DONE)
		return true;

	return false;
}

void di_pause_step_done(unsigned int ch)
{
	unsigned int run_flag;

	run_flag = di_mp_uix_get(ch, EDI_MP_UIX_RUN_FLG);
	if (run_flag == DI_RUN_FLAG_STEP) {
		di_mp_uix_set(ch, EDI_MP_UIX_RUN_FLG,
			      DI_RUN_FLAG_STEP_DONE);
	}
}

void di_pause(unsigned int ch, bool on)
{
	pr_info("%s:%d\n", __func__, on);
	if (on)
		di_mp_uix_set(ch, EDI_MP_UIX_RUN_FLG,
			      DI_RUN_FLAG_PAUSE);
	else
		di_mp_uix_set(ch, EDI_MP_UIX_RUN_FLG,
			      DI_RUN_FLAG_RUN);
}

/**************************************
 *
 * summmary variable
 *
 **************************************/
const struct di_sum_s di_sum_tab[] = {
	/*video_peek_cnt*/
	[EDI_SUM_O_PEEK_CNT] = {"o_peek_cnt", EDI_SUM_O_PEEK_CNT, 0},
	/*di_reg_unreg_cnt*/
	[EDI_SUM_REG_UNREG_CNT] = {
			"di_reg_unreg_cnt", EDI_SUM_REG_UNREG_CNT, 100},

	[EDI_SUM_NUB] = {"end", EDI_SUM_NUB, 0},
};

unsigned int di_sum_get_tab_size(void)
{
	return ARRAY_SIZE(di_sum_tab);
}

bool di_sum_check(unsigned int ch, enum EDI_SUM id)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(di_sum_tab);

	if (id >= tsize) {
		PR_ERR("%s:err:overflow:tsize[%d],id[%d]\n",
		       __func__, tsize, id);
		return false;
	}
	if (di_sum_tab[id].index != id) {
		PR_ERR("%s:err:table:id[%d],tab_id[%d]\n",
		       __func__, id, di_sum_tab[id].index);
		return false;
	}
	return true;
}

void di_sum_reg_init(unsigned int ch)
{
	unsigned int tsize;
	int i;

	tsize = ARRAY_SIZE(di_sum_tab);

	dbg_init("%s:ch[%d]\n", __func__, ch);
	for (i = 0; i < tsize; i++) {
		if (!di_sum_check(ch, i))
			continue;
		dbg_init("\t:%d:name:%s,%d\n", i, di_sum_tab[i].name,
			 di_sum_tab[i].default_val);
		di_sum_set_l(ch, i, di_sum_tab[i].default_val);
	}
}

void di_sum_get_info(unsigned int ch,  enum EDI_SUM id, char **name,
		     unsigned int *pval)
{
	*name = di_sum_tab[id].name;
	*pval = di_sum_get(ch, id);
}

void di_sum_set(unsigned int ch, enum EDI_SUM id, unsigned int val)
{
	if (!di_sum_check(ch, id))
		return;

	di_sum_set_l(ch, id, val);
}

unsigned int di_sum_inc(unsigned int ch, enum EDI_SUM id)
{
	if (!di_sum_check(ch, id))
		return 0;
	return di_sum_inc_l(ch, id);
}

unsigned int di_sum_get(unsigned int ch, enum EDI_SUM id)
{
	if (!di_sum_check(ch, id))
		return 0;
	return di_sum_get_l(ch, id);
}

void dim_sumx_clear(unsigned int ch)
{
	struct dim_sum_s *psumx = get_sumx(ch);

	memset(psumx, 0, sizeof(*psumx));
}

void dim_sumx_set(struct di_ch_s *pch)
{
	//struct di_ch_s *pch;
	struct dim_sum_s *psumx;// = get_sumx(ch);
	unsigned int ch;

	//pch = get_chdata(ch);
	psumx = &pch->sumx;
	ch = pch->ch_id;
	psumx->b_pre_free	= list_count(ch, QUEUE_LOCAL_FREE);
	psumx->b_pre_ready	= di_que_list_count(ch, QUE_PRE_READY);
	psumx->b_pst_free	= di_que_list_count(ch, QUE_POST_FREE);
	psumx->b_pst_ready	= ndrd_cnt(pch);//di_que_list_count(ch, QUE_POST_READY);
	psumx->b_recyc		= list_count(ch, QUEUE_RECYCLE);
	psumx->b_display	= list_count(ch, QUEUE_DISPLAY);
	psumx->b_nin		= nins_cnt(pch, QBF_NINS_Q_CHECK);
	psumx->b_in_free	= di_que_list_count(ch, QUE_IN_FREE);

	if (psumx->b_nin &&
	    psumx->b_pst_free &&
	    psumx->b_in_free >= 2) {
		if (psumx->need_local && psumx->b_pre_free)
			bset(&pch->self_trig_need, 0);
		else if (!psumx->need_local)
			bset(&pch->self_trig_need, 0);
		else
			bclr(&pch->self_trig_need, 0);
	} else {
		bclr(&pch->self_trig_need, 0);
	}
	if (di_bypass_state_get(ch) == 1	&&
	    psumx->b_nin			&&
	    psumx->flg_rebuild == 0		&&
	    psumx->b_in_free) {
		task_send_ready(30);
	}
}

/****************************/
/*call by event*/
/****************************/
void dip_even_reg_init_val(unsigned int ch)
{
}

void dip_even_unreg_val(unsigned int ch)
{
}

/****************************/
static void dip_cma_init_val(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		/* CMA state */
		atomic_set(&pbm->cma_mem_state[ch], EDI_CMA_ST_IDL);

		/* CMA reg/unreg cmd */
		/* pbm->cma_reg_cmd[ch] = 0; */
		pbm->cma_wqsts[ch] = 0;
	}
	pbm->cma_flg_run = 0;
}

void dip_cma_close(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();

	if (dip_cma_st_is_idl_all())
		return;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
#ifdef MARK_SC2
		if (!dip_cma_st_is_idle(ch)) {
			dim_cma_top_release(ch);
			pr_info("%s:force release ch[%d]", __func__, ch);
			atomic_set(&pbm->cma_mem_state[ch], EDI_CMA_ST_IDL);

			/*pbm->cma_reg_cmd[ch] = 0;*/
			pbm->cma_wqsts[ch] = 0;
		}
#endif
	}
	pbm->cma_flg_run = 0;
}

bool dip_cma_st_is_idle(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();
	bool ret = false;

	if (atomic_read(&pbm->cma_mem_state[ch]) == EDI_CMA_ST_IDL)
		ret = true;

	return ret;
}

bool dip_cma_st_is_idl_all(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();
	bool ret = true;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		if (atomic_read(&pbm->cma_mem_state[ch]) != EDI_CMA_ST_IDL) {
			ret = true;
			break;
		}
	}
	return ret;
}

enum EDI_CMA_ST dip_cma_get_st(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();

	return atomic_read(&pbm->cma_mem_state[ch]);
}

const char * const di_cma_state_name[] = {
	"IDLE",
	"do_alloc",
	"READY",
	"do_release",
	"PART",
};

const char *di_cma_dbg_get_st_name(unsigned int ch)
{
	enum EDI_CMA_ST st = dip_cma_get_st(ch);
	const char *p = "overflow";

	if (st < ARRAY_SIZE(di_cma_state_name))
		p = di_cma_state_name[st];
	return p;
}

void dip_cma_st_set_ready_all(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++)
		atomic_set(&pbm->cma_mem_state[ch], EDI_CMA_ST_READY);
}

/****************************/
/*channel STATE*/
/****************************/
void dip_chst_set(unsigned int ch, enum EDI_TOP_STATE chst)
{
	struct di_mng_s *pbm = get_bufmng();

	atomic_set(&pbm->ch_state[ch], chst);
}

enum EDI_TOP_STATE dip_chst_get(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();

	return atomic_read(&pbm->ch_state[ch]);
}

void dip_chst_init(void)
{
	unsigned int ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++)
		dip_chst_set(ch, EDI_TOP_STATE_IDLE);
}

static void dip_process_reg_after(struct di_ch_s *pch);
//ol dim_process_reg(struct di_ch_s *pch);
//ol dim_process_unreg(struct di_ch_s *pch);

void dip_chst_process_ch(void)
{
	unsigned int ch;
	unsigned int chst;
	struct vframe_s *vframe;
	struct di_pre_stru_s *ppre;// = get_pre_stru(ch);
//	struct di_mng_s *pbm = get_bufmng();
//ary 2020-12-09	ulong flags = 0;
	struct di_ch_s *pch;
	struct di_mng_s *pbm = get_bufmng();

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pch = get_chdata(ch);
		if (atomic_read(&pbm->trig_unreg[ch]))
			dim_process_unreg(pch);
		else if (atomic_read(&pbm->trig_reg[ch]))
			dim_process_reg(pch);


		dip_process_reg_after(pch);

		chst = dip_chst_get(ch);
		ppre = get_pre_stru(ch);
		task_polling_cmd_keep(ch, chst);

		switch (chst) {
		case EDI_TOP_STATE_REG_STEP2:

			break;
		case EDI_TOP_STATE_UNREG_STEP1:

			break;
		case EDI_TOP_STATE_UNREG_STEP2:

			break;
		case EDI_TOP_STATE_READY:
			dip_itf_vf_op_polling(pch);
			dip_itf_back_input(pch);
//ary 2020-12-09			spin_lock_irqsave(&plist_lock, flags);
			dim_post_keep_back_recycle(pch);
//ary 2020-12-09			spin_unlock_irqrestore(&plist_lock, flags);
			//move dim_sumx_set(ch);
			//dbg_nins_check_id(pch);
//			tst_release(pch);
#ifdef SC2_NEW_FLOW
			ins_in_vf(pch);
#endif
			break;
		case EDI_TOP_STATE_BYPASS:
			if (dip_itf_is_ins_exbuf(pch)) {
				PR_ERR("%s:bypss:extbuf\n", __func__);
				break;
			}
			vframe = pw_vf_peek(pch->ch_id);//nins_peekvfm(pch);

			if (!vframe)
				break;
			if (dim_need_bypass(ch, vframe))
				break;

			di_reg_variable(ch, vframe);
			if (!ppre->bypass_flag) {
				set_bypass2_complete(ch, false);
				/*this will cause first local buf not alloc*/
				/*dim_bypass_first_frame(ch);*/
				dip_chst_set(ch, EDI_TOP_STATE_REG_STEP2);
			}
			break;

		default:
			break;
		}
	}
}

void dip_sum_post_ch(void)
{
	unsigned int ch;
	unsigned int chst;
	struct di_ch_s *pch;
	unsigned int cnt;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pch = get_chdata(ch);
		chst = dip_chst_get(ch);
		switch (chst) {
		case EDI_TOP_STATE_READY:
			dim_sumx_set(pch);
			bclr(&pch->self_trig_mask, 30);
			break;
		default:
			bset(&pch->self_trig_mask, 30);
			break;
		}
		if (chst == EDI_TOP_STATE_IDLE	||
		    chst == EDI_TOP_STATE_READY	||
		    chst == EDI_TOP_STATE_BYPASS) {
			cnt = ndkb_cnt(pch);
			if (cnt)
				task_send_ready(3);
		}
	}
}

void dip_out_ch(void)
{
	int i;
	struct di_ch_s *pch;

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		pch = get_chdata(i);

		if (pch->itf.op_ready_out)
			pch->itf.op_ready_out(pch);
	}
}
/************************************************
 * new reg and unreg
 ***********************************************/
/************************************************
 * dim_api_reg
 *	block
 ***********************************************/

bool dim_api_reg(enum DIME_REG_MODE rmode, struct di_ch_s *pch)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();
	bool ret = false;
	unsigned int cnt;
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (!pch) {
#ifdef PRINT_BASIC
		PR_ERR("%s:no pch\n", __func__);
#endif
		return false;
	}
	ch = pch->ch_id;

	dip_even_reg_init_val(ch);
	if (de_devp->flags & DI_SUSPEND_FLAG) {
#ifdef PRINT_BASIC
		PR_ERR("reg event device hasn't resumed\n");
#endif
		return false;
	}
	if (get_reg_flag(ch)) {
		PR_ERR("no muti instance.\n");
		return false;
	}
	task_delay(50);

	di_bypass_state_set(ch, false);

	atomic_set(&pbm->trig_reg[ch], 1);

	dbg_timer(ch, EDBG_TIMER_REG_B);
	task_send_ready(4);

	cnt = 0; /* 500us x 10 = 5ms */
	while (atomic_read(&pbm->trig_reg[ch]) && cnt < 10) {
		usleep_range(500, 501);
		cnt++;
	}

	task_send_ready(5);

	cnt = 0; /* 3ms x 2000 = 6s */
	while (atomic_read(&pbm->trig_reg[ch]) && cnt < 2000) {
		usleep_range(3000, 3001);
		cnt++;
	}

	if (!atomic_read(&pbm->trig_reg[ch]))
		ret = true;
	else
		PR_ERR("%s:ch[%d]:failed\n", __func__, ch);

	dbg_timer(ch, EDBG_TIMER_REG_E);
	dbg_reg("ch[%d]:reg end\n", ch);
	return ret;
}

void dim_trig_unreg(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();

	atomic_set(&pbm->trig_unreg[ch], 1);
}

bool dim_api_unreg(enum DIME_REG_MODE rmode, struct di_ch_s *pch)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();
	bool ret = false;
	unsigned int cnt;

	if (!pch) {
		//PR_ERR("%s:no pch\n", __func__);
		return false;
	}
	ch = pch->ch_id;

	/*dbg_ev("%s:unreg\n", __func__);*/
	dip_even_unreg_val(ch);	/*new*/
	di_bypass_state_set(ch, true);

	dbg_timer(ch, EDBG_TIMER_UNREG_B);
	task_delay(100);

	task_send_ready(6);

	cnt = 0; /* 500us x 10 = 5ms */
	while (atomic_read(&pbm->trig_unreg[ch]) && cnt < 10) {
		usleep_range(500, 501);
		cnt++;
	}

	task_send_ready(7);

	cnt = 0; /* 3ms x 2000 = 6s */
	while (atomic_read(&pbm->trig_unreg[ch]) && cnt < 2000) {
		usleep_range(3000, 3001);
		cnt++;
	}

	if (!atomic_read(&pbm->trig_unreg[ch]))
		ret = true;
	else
		PR_ERR("%s:ch[%d]:failed\n", __func__, ch);

	dbg_timer(ch, EDBG_TIMER_UNREG_E);
	//dbg_ev("ch[%d]unreg end\n", ch);

	return ret;
}

/* from dip_event_reg_chst */
bool dim_process_reg(struct di_ch_s *pch)
{
	enum EDI_TOP_STATE chst;
	unsigned int ch = pch->ch_id;
//	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	bool ret = false;
	struct di_mng_s *pbm = get_bufmng();

	dbg_dbg("%s:ch[%d]\n", __func__, ch);

	chst = dip_chst_get(ch);
	//trace_printk("%s,%d\n", __func__, chst);

	switch (chst) {
	case EDI_TOP_STATE_IDLE:
		queue_init2(ch);
		di_que_init(ch);
		bufq_nin_reg(pch);
		//move to event di_vframe_reg(ch);
		di_cfg_cp_ch(pch);

		dip_chst_set(ch, EDI_TOP_STATE_REG_STEP1);
		task_send_cmd(LCMD1(ECMD_REG, ch));
		ret = true;
		dbg_dbg("reg ok\n");
		break;
	case EDI_TOP_STATE_REG_STEP1:
	case EDI_TOP_STATE_REG_STEP1_P1:
	case EDI_TOP_STATE_REG_STEP2:
	case EDI_TOP_STATE_READY:
	case EDI_TOP_STATE_BYPASS:
		PR_WARN("have reg\n");
		ret = true;
		break;
	case EDI_TOP_STATE_UNREG_STEP1:
	case EDI_TOP_STATE_UNREG_STEP2:
		PR_ERR("%s:in UNREG_STEP1/2\n", __func__);
		ret = false;
		break;
	case EDI_TOP_STATE_NOPROB:
	default:
		ret = false;
		PR_ERR("err: not prob[%d]\n", chst);

		break;
	}

	atomic_set(&pbm->trig_reg[ch], 0);
	//trace_printk("%s end\n", __func__);
	return ret;
}

/*from: dip_event_unreg_chst*/
bool dim_process_unreg(struct di_ch_s *pch)
{
	enum EDI_TOP_STATE chst, chst2;
	unsigned int ch = pch->ch_id;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	bool ret = false;
	struct di_mng_s *pbm = get_bufmng();

	chst = dip_chst_get(ch);
	dbg_reg("%s:ch[%d]:%s\n", __func__, ch, dip_chst_get_name(chst));

	if (chst > EDI_TOP_STATE_NOPROB)
		set_flag_trig_unreg(ch, true);

	//trace_printk("%s:%d\n", __func__, chst);
	switch (chst) {
	case EDI_TOP_STATE_READY:
		//move di_vframe_unreg(ch);
		/*trig unreg*/
		dip_chst_set(ch, EDI_TOP_STATE_UNREG_STEP1);
		//task_send_cmd(LCMD1(ECMD_UNREG, ch));
		/*debug only di_dbg = di_dbg|DBG_M_TSK;*/

		/*wait*/
		ppre->unreg_req_flag_cnt = 0;
		chst2 = dip_chst_get(ch);

		/*debug only di_dbg = di_dbg & (~DBG_M_TSK);*/
		dbg_reg("%s:ch[%d] ready end\n", __func__, ch);
		task_delay(100);

		break;
	case EDI_TOP_STATE_BYPASS:
		/*from bypass complet to unreg*/
		//move di_vframe_unreg(ch);
		di_unreg_variable(ch);

		set_reg_flag(ch, false);
		set_reg_setting(ch, false);
		if ((!get_reg_flag_all()) &&
		    (!get_reg_setting_all())) {
			dbg_pl("ch[%d]:unreg1,bypass:\n", ch);
			di_unreg_setting();
			dpre_init();
			dpost_init();
		}
		dip_chst_set(ch, EDI_TOP_STATE_IDLE);
		ret = true;

		break;
	case EDI_TOP_STATE_IDLE:
		PR_WARN("have unreg\n");
		ret = true;
		break;
	case EDI_TOP_STATE_REG_STEP2:
		di_unreg_variable(ch);
		set_reg_flag(ch, false);
		set_reg_setting(ch, false);
		if ((!get_reg_flag_all()) &&
		    (!get_reg_setting_all())) {
			dbg_pl("ch[%d]:unreg2,step2:\n", ch);
			di_unreg_setting();
			dpre_init();
			dpost_init();
		}
		dbg_dbg("%s:in reg step2\n", __func__);
		set_reg_flag(ch, false);
		dip_chst_set(ch, EDI_TOP_STATE_IDLE);

		ret = true;
		break;
		/*note: no break;*/
	case EDI_TOP_STATE_REG_STEP1:
	case EDI_TOP_STATE_REG_STEP1_P1:
		dbg_dbg("%s:in reg step1\n", __func__);
		//move di_vframe_unreg(ch);
		set_reg_flag(ch, false);
		dip_chst_set(ch, EDI_TOP_STATE_IDLE);

		ret = true;
		break;
	case EDI_TOP_STATE_UNREG_STEP1:
		if (dpre_can_exit(ch) && dpst_can_exit(ch)) {
			dip_chst_set(ch, EDI_TOP_STATE_UNREG_STEP2);
			set_reg_flag(ch, false);
			set_reg_setting(ch, false);
			//reflesh = true;
		}
		break;
	case EDI_TOP_STATE_UNREG_STEP2:
		di_unreg_variable(ch);
		if ((!get_reg_flag_all()) &&
		    (!get_reg_setting_all())) {
			dbg_pl("ch[%d]:unreg3,step2:\n", ch);
			di_unreg_setting();
			dpre_init();
			dpost_init();
		}

		dip_chst_set(ch, EDI_TOP_STATE_IDLE);
		ret = true;
		break;
	case EDI_TOP_STATE_NOPROB:
	default:
		PR_ERR("err: not prob[%d]\n", chst);
		ret = true;
		break;
	}

	if (ret) {
		task_delay(1);
		//trace_printk("%s end\n", __func__);
		dbg_pl("unreg:%s\n", dip_chst_get_name(chst));
		atomic_set(&pbm->trig_unreg[ch], 0);
	} else {
		//trace_printk("%s step end\n", __func__);
	}

	return ret;
}

/* from dip_chst_process_reg */
static void dip_process_reg_after(struct di_ch_s *pch)
{
	enum EDI_TOP_STATE chst;
	struct vframe_s *vframe;
	unsigned int ch = pch->ch_id;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	bool reflesh = true;
//	struct di_mng_s *pbm = get_bufmng();
//	ulong flags = 0;

	while (reflesh) {
		reflesh = false;

	chst = dip_chst_get(ch);

	//dbg_reg("%s:ch[%d]%s\n", __func__, ch, dip_chst_get_name(chst));

	switch (chst) {
	case EDI_TOP_STATE_NOPROB:
	case EDI_TOP_STATE_IDLE:
		break;
	case EDI_TOP_STATE_REG_STEP1:/*wait peek*/
		dip_itf_vf_op_polling(pch);
		vframe = nins_peekvfm(pch);

		if (vframe) {
			dbg_timer(ch, EDBG_TIMER_FIRST_PEEK);
			dim_tr_ops.pre_get(vframe->index_disp);

			set_flag_trig_unreg(ch, false);

			dip_chst_set(ch, EDI_TOP_STATE_REG_STEP1_P1);

			reflesh = true;
			//trace_printk("step1:peek\n");
		}
		break;
	case EDI_TOP_STATE_REG_STEP1_P1:
		vframe = nins_peekvfm(pch);
		if (!vframe) {
			PR_ERR("%s:p1 vfm nop\n", __func__);
			dip_chst_set(ch, EDI_TOP_STATE_REG_STEP1);
			reflesh = true;
			break;
		}

		di_reg_variable(ch, vframe);
		/*di_reg_process_irq(ch);*/ /*check if bypass*/
		set_reg_setting(ch, true);

		/*?how  about bypass ?*/
		if (ppre->bypass_flag) {
			/* complete bypass */
			set_bypass2_complete(ch, true);
			if (!get_reg_flag_all()) {
				/*first channel reg*/
				dpre_init();
				dpost_init();
				di_reg_setting(ch, vframe);
			}
			dip_chst_set(ch, EDI_TOP_STATE_BYPASS);
			set_reg_flag(ch, true);
		} else {
			set_bypass2_complete(ch, false);
			if (!get_reg_flag_all()) {
				/*first channel reg*/
				dpre_init();
				dpost_init();
				di_reg_setting(ch, vframe);
				di_reg_setting_working(pch, vframe);
			}
			/*this will cause first local buf not alloc*/
			/*dim_bypass_first_frame(ch);*/
			dip_chst_set(ch, EDI_TOP_STATE_REG_STEP2);
			/*set_reg_flag(ch, true);*/
		}
		dbg_reg("%s:p1 end\n", __func__);
		reflesh = true;
		break;
	case EDI_TOP_STATE_REG_STEP2:/**/

		pch = get_chdata(ch);
#ifdef	SC2_NEW_FLOW
		if (memn_get(pch)) {
#else
		//if (mem_cfg(pch)) {
		mem_cfg_pre(pch);
		mem_cfg_2local(pch);
		mem_cfg_2pst(pch);
		PR_INF("ch[%d]:reg:mem cfg[%d][%d][%d]\n",
		       pch->ch_id,
		       pch->sts_mem_pre_cfg,
		       pch->sts_mem_2_local,
		       pch->sts_mem_2_pst);
		if (di_pst_afbct_check(pch) &&
		    di_i_dat_check(pch)	/*	&&*/
		    /*mem_alloc_check(pch)*/) {
			//mem_cfg(pch);
#endif
			//mem_cfg_realloc_wait(pch);
			//sct_mng_working(pch);
			//sct_alloc_in_poling(pch->ch_id);
			sct_polling(pch, 1);
			PR_INF("s2_1\n");
			if (di_cfg_top_get(EDI_CFG_FIRST_BYPASS) &&
			    pch->itf.etype == EDIM_NIN_TYPE_VFM) {
				if (get_sum_g(ch) == 0) {
					if (dip_itf_vf_sop(pch) &&
					    dip_itf_vf_sop(pch)->vf_m_bypass_first_frame)
						dip_itf_vf_sop(pch)->vf_m_bypass_first_frame(pch);
				} else {
					PR_INF("ch[%d],g[%d]\n",
					       ch, get_sum_g(ch));
				}
			}
			dbg_timer(ch, EDBG_TIMER_READY);
			dip_chst_set(ch, EDI_TOP_STATE_READY);
			set_reg_flag(ch, true);
		} else {
			PR_INF("s2_wait\n");
		}

		break;
	case EDI_TOP_STATE_READY:

		break;
	case EDI_TOP_STATE_BYPASS:
	case EDI_TOP_STATE_UNREG_STEP1:
	case EDI_TOP_STATE_UNREG_STEP2:
		/*do nothing;*/
		break;
	}
	}
}

/****************************************************************
 * tmode
 ****************************************************************/
void dim_tmode_preset(void)
{
	struct di_mng_s *pbm = get_bufmng();
	unsigned int ch;
	unsigned int cnt;

	/*EDIM_TMODE_1_PW_VFM*/
	cnt = min_t(size_t, DI_CHANNEL_NUB, cfgg(TMODE_1));
	cnt = min_t(size_t, DIM_K_VFM_IN_LIMIT, cnt);
	for (ch = 0; ch < cnt; ch++)
		pbm->tmode_pre[ch] = EDIM_TMODE_1_PW_VFM;

	/*EDIM_TMODE_2_PW_OUT*/
	cnt += cfgg(TMODE_2);
	cnt = min_t(size_t, DI_CHANNEL_NUB, cnt);
	for (; ch < cnt; ch++)
		pbm->tmode_pre[ch] = EDIM_TMODE_2_PW_OUT;

	/*EDIM_TMODE_3_PW_LOCAL*/
	cnt += cfgg(TMODE_3);
	cnt = min_t(size_t, DI_CHANNEL_NUB, cnt);
	for (; ch < cnt; ch++)
		pbm->tmode_pre[ch] = EDIM_TMODE_3_PW_LOCAL;

	if (!(di_dbg & DBG_M_REG))
		return;
	/* dbg only */
	PR_INF("%s:\n", __func__);
	for (ch = 0; ch < DI_CHANNEL_NUB; ch++)
		PR_INF("\tch[%d]:tmode[%d]\n", ch, pbm->tmode_pre[ch]);
}

void dim_tmode_set(unsigned int ch, enum EDIM_TMODE tmode)
{
	struct di_mng_s *pbm = get_bufmng();

	if (ch >= DI_CHANNEL_NUB)
		return;

	pbm->tmode_pre[ch] = tmode;
}

bool dim_tmode_is_localpost(unsigned int ch)
{
//	struct di_mng_s *pbm = get_bufmng();
	struct di_ch_s *pch;

	if (ch >= DI_CHANNEL_NUB) {
		PR_ERR("%s:ch[%d]\n", __func__, ch);
		return false;
	}
	pch = get_chdata(ch);

	if (pch->itf.tmode == EDIM_TMODE_2_PW_OUT)
		return false;
	return true;
}

/************************************************/
void dip_hw_process(void)
{
	di_dbg_task_flg = 5;
	if (get_datal()->dct_op)
		get_datal()->dct_op->main_process();
	dpre_process();
	di_dbg_task_flg = 6;
	pre_mode_setting();
	di_dbg_task_flg = 7;
	dpst_process();
	di_dbg_task_flg = 8;
	dpre_after_do_table();
}

const char * const di_top_state_name[] = {
	"NOPROB",
	"IDLE",
	"REG_STEP1",
	"REG_P1",
	"REG_STEP2",
	"READY",
	"BYPASS",
	"UNREG_STEP1",
	"UNREG_STEP2",
};

const char *dip_chst_get_name_curr(unsigned int ch)
{
	const char *p = "";
	enum EDI_TOP_STATE chst;

	chst = dip_chst_get(ch);

	if (chst < ARRAY_SIZE(di_top_state_name))
		p = di_top_state_name[chst];

	return p;
}

const char *dip_chst_get_name(enum EDI_TOP_STATE chst)
{
	const char *p = "";

	if (chst < ARRAY_SIZE(di_top_state_name))
		p = di_top_state_name[chst];

	return p;
}

static const struct di_mm_cfg_s c_mm_cfg_normal = {
	.di_h	=	1088,
	.di_w	=	1920,
	.num_local	=	MAX_LOCAL_BUF_NUM,
	.num_post	=	POST_BUF_NUM,
	.num_step1_post = 1,
};

static const struct di_mm_cfg_s c_mm_cfg_normal_ponly = {
	.di_h	=	1088,
	.di_w	=	1920,
	.num_local	=	0,
	.num_post	=	POST_BUF_NUM,
	.num_step1_post = 1,
};

static const struct di_mm_cfg_s c_mm_cfg_4k = {
	.di_h	=	2160,
	.di_w	=	3840,
	.num_local	=	0,
	.num_post	=	POST_BUF_NUM,
	.num_step1_post = 1,
};

static const struct di_mm_cfg_s c_mm_cfg_fix_4k = {
	.di_h	=	2160,
	.di_w	=	3840,
	.num_local	=	MAX_LOCAL_BUF_NUM,
	.num_post	=	POST_BUF_NUM,
	.num_step1_post = 1,
};

static const struct di_mm_cfg_s c_mm_cfg_bypass = {
	.di_h	=	2160,
	.di_w	=	3840,
	.num_local	=	0,
	.num_post	=	0,
	.num_step1_post = 0,
};

const struct di_mm_cfg_s *di_get_mm_tab(unsigned int is_4k,
					struct di_ch_s *pch)
{
	if (is_4k)
		return &c_mm_cfg_4k;
	else if (pch->ponly)
		return &c_mm_cfg_normal_ponly;
	else
		return &c_mm_cfg_normal;
}

/**********************************/
/* TIME OUT CHEKC api*/
/**********************************/

void di_tout_int(struct di_time_out_s *tout, unsigned int thd)
{
	tout->en = false;
	tout->timer_start = 0;
	tout->timer_thd = thd;
}

bool di_tout_contr(enum EDI_TOUT_CONTR cmd, struct di_time_out_s *tout)
{
	unsigned long ctimer;
	unsigned long diff;
	bool ret = false;

	ctimer = cur_to_msecs();

	switch (cmd) {
	case EDI_TOUT_CONTR_EN:
		tout->en = true;
		tout->timer_start = ctimer;
		break;
	case EDI_TOUT_CONTR_FINISH:
		if (tout->en) {
			diff = ctimer - tout->timer_start;

			if (diff > tout->timer_thd) {
				tout->over_flow_cnt++;

				if (tout->over_flow_cnt > 0xfffffff0) {
					tout->over_flow_cnt = 0;
					tout->flg_over = 1;
				}
		#ifdef MARK_HIS
				if (tout->do_func)
					tout->do_func();

		#endif
				ret = true;
			}
			tout->en = false;
		}
		break;

	case EDI_TOUT_CONTR_CHECK:	/*if time is overflow, disable timer*/
		if (tout->en) {
			diff = ctimer - tout->timer_start;

			if (diff > tout->timer_thd) {
				tout->over_flow_cnt++;

				if (tout->over_flow_cnt > 0xfffffff0) {
					tout->over_flow_cnt = 0;
					tout->flg_over = 1;
				}
				#ifdef MARK_HIS
				if (tout->do_func)
					tout->do_func();

				#endif
				ret = true;
				tout->en = false;
			}
		}
		break;
	case EDI_TOUT_CONTR_CLEAR:
		tout->en = false;
		tout->timer_start = ctimer;
		break;
	case EDI_TOUT_CONTR_RESET:
		tout->en = true;
		tout->timer_start = ctimer;
		break;
	}

	return ret;
}

const unsigned int di_ch2mask_table[DI_CHANNEL_MAX] = {
	DI_BIT0,
	DI_BIT1,
	DI_BIT2,
	DI_BIT3,
};

/****************************************
 *bit control
 ****************************************/
static const unsigned int bit_tab[32] = {
	DI_BIT0,
	DI_BIT1,
	DI_BIT2,
	DI_BIT3,
	DI_BIT4,
	DI_BIT5,
	DI_BIT6,
	DI_BIT7,
	DI_BIT8,
	DI_BIT9,
	DI_BIT10,
	DI_BIT11,
	DI_BIT12,
	DI_BIT13,
	DI_BIT14,
	DI_BIT15,
	DI_BIT16,
	DI_BIT17,
	DI_BIT18,
	DI_BIT19,
	DI_BIT20,
	DI_BIT21,
	DI_BIT22,
	DI_BIT23,
	DI_BIT24,
	DI_BIT25,
	DI_BIT26,
	DI_BIT27,
	DI_BIT28,
	DI_BIT29,
	DI_BIT30,
	DI_BIT31,
};

void bset(unsigned int *p, unsigned int bitn)
{
	*p = *p | bit_tab[bitn];
}

void bclr(unsigned int *p, unsigned int bitn)
{
	*p = *p & (~bit_tab[bitn]);
}

bool bget(unsigned int *p, unsigned int bitn)
{
	return (*p & bit_tab[bitn]) ? true : false;
}

/****************************************/
/* do_table				*/
/****************************************/

/*for do_table_working*/
#define K_DO_TABLE_LOOP_MAX	15

const struct do_table_s do_table_def = {
	.ptab = NULL,
	.data = NULL,
	.size = 0,
	.op_lst = K_DO_TABLE_ID_STOP,
	.op_crr = K_DO_TABLE_ID_STOP,
	.do_stop = 0,
	.flg_stop = 0,
	.do_pause = 0,
	.do_step = 0,
	.flg_repeat = 0,

};

void do_table_init(struct do_table_s *pdo,
		   const struct do_table_ops_s *ptable,
		   unsigned int size_tab)
{
	memcpy(pdo, &do_table_def, sizeof(struct do_table_s));

	if (ptable) {
		pdo->ptab = ptable;
		pdo->size = size_tab;
	}
}

/*if change to async?*/
/* now only call in same thread */
void do_talbe_cmd(struct do_table_s *pdo, enum EDO_TABLE_CMD cmd)
{
	switch (cmd) {
	case EDO_TABLE_CMD_NONE:
		pr_info("test:%s\n", __func__);
		break;
	case EDO_TABLE_CMD_STOP:
		pdo->do_stop = true;
		break;
	case EDO_TABLE_CMD_START:
		if (pdo->op_crr == K_DO_TABLE_ID_STOP) {
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr = K_DO_TABLE_ID_START;
			pdo->do_stop = false;
			pdo->flg_stop = false;
		} else if (pdo->op_crr == K_DO_TABLE_ID_PAUSE) {
			pdo->op_crr = pdo->op_lst;
			pdo->op_lst = K_DO_TABLE_ID_PAUSE;
			pdo->do_pause = false;
		} else {
			pr_info("crr is [%d], not start\n", pdo->op_crr);
		}
		break;
	case EDO_TABLE_CMD_PAUSE:
		if (pdo->op_crr <= K_DO_TABLE_ID_STOP) {
			/*do nothing*/
		} else {
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr = K_DO_TABLE_ID_PAUSE;
			pdo->do_pause = true;
		}
		break;
	case EDO_TABLE_CMD_STEP:
		pdo->do_step = true;
		break;
	case EDO_TABLE_CMD_STEP_BACK:
		pdo->do_step = false;
		break;
	default:
		break;
	}
}

bool do_table_is_crr(struct do_table_s *pdo, unsigned int state)
{
	if (pdo->op_crr == state)
		return true;
	return false;
}

void do_table_working(struct do_table_s *pdo)
{
	const struct do_table_ops_s *pcrr;
	unsigned int ret = 0;
	unsigned int next = 0;
	bool flash = false;
	unsigned int cnt = 0;	/*proction*/
	unsigned int lst_id;	/*dbg only*/
	char *name = "";	/*dbg only*/
	bool need_pr = false;	/*dbg only*/

	if (!pdo)
		return;

	if (!pdo->ptab		||
	    pdo->op_crr >= pdo->size) {
		PR_ERR("di:err:%s:ovflow:0x%p,0x%p,crr=%d,size=%d\n",
		       __func__,
		       pdo, pdo->ptab,
		       pdo->op_crr,
		       pdo->size);
		return;
	}

	pcrr = pdo->ptab + pdo->op_crr;

	if (pdo->name)
		name = pdo->name;
	/*stop ?*/
	if (pdo->do_stop &&
	    (pcrr->mark & K_DO_TABLE_CAN_STOP)) {
		dbg_dt("%s:do stop\n", name);

		/*do stop*/
		if (pcrr->do_stop_op)
			pcrr->do_stop_op(pdo->data);
		/*set status*/
		pdo->op_lst = pdo->op_crr;
		pdo->op_crr = K_DO_TABLE_ID_STOP;
		pdo->flg_stop = true;
		pdo->do_stop = false;

		return;
	}

	/*pause?*/
	if (pdo->op_crr == K_DO_TABLE_ID_STOP	||
	    pdo->op_crr == K_DO_TABLE_ID_PAUSE) {
		lst_id = pdo->op_crr;
		return;
	}

	do {
		flash = false;
		cnt++;
		if (cnt > K_DO_TABLE_LOOP_MAX) {
			PR_ERR("di:err:%s:loop more %d\n", name, cnt);
			break;
		}

		/*stop again? */
		if (pdo->do_stop &&
		    (pcrr->mark & K_DO_TABLE_CAN_STOP)) {
			/*do stop*/
			dbg_dt("%s: do stop in loop\n", name);
			if (pcrr->do_stop_op)
				pcrr->do_stop_op(pdo->data);
			/*set status*/
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr = K_DO_TABLE_ID_STOP;
			pdo->flg_stop = true;
			pdo->do_stop = false;

			break;
		}

		/*debug:*/
		lst_id = pdo->op_crr;
		need_pr = true;

		if (pcrr->con) {
			if (pcrr->con(pdo->data))
				ret = pcrr->do_op(pdo->data);
			else
				break;

		} else {
			ret = pcrr->do_op(pdo->data);
			dbg_dt("do_table:do:%d:ret=0x%x\n", pcrr->id, ret);
		}

		/*not finish, keep current status*/
		if ((ret & K_DO_TABLE_R_B_FINISH) == 0) {
			dbg_dt("%s:not finish,wait\n", __func__);
			break;
		}

		/*fix to next */
		if (ret & K_DO_TABLE_R_B_NEXT) {
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr++;
			if (pdo->op_crr >= pdo->size) {
				pdo->op_crr = pdo->flg_repeat ?
					K_DO_TABLE_ID_START
					: K_DO_TABLE_ID_STOP;
				dbg_dt("%s:to end,%d\n", __func__,
				       pdo->op_crr);
				break;
			}
			/*return;*/
			flash = true;
		} else {
			next = ((ret & K_DO_TABLE_R_B_OTHER) >>
					K_DO_TABLE_R_B_OTHER_SHIFT);
			if (next < pdo->size) {
				pdo->op_lst = pdo->op_crr;
				pdo->op_crr = next;
				if (next > K_DO_TABLE_ID_STOP)
					flash = true;
				else
					flash = false;
			} else {
				PR_ERR("%s: next[%d] err:\n",
				       __func__, next);
			}
		}
		/*debug 1:*/
		need_pr = false;
		if (lst_id != pdo->op_crr) {
			dbg_dt("do_table:%s:%s->%s\n", pdo->name,
			       pdo->ptab[lst_id].name,
			       pdo->ptab[pdo->op_crr].name);
		}

		pcrr = pdo->ptab + pdo->op_crr;
		dbg_dt("next[%d] op[%d] flash[%d]\n", next, pdo->op_crr, flash);
	} while (flash && !pdo->do_step);

	/*debug 2:*/
	if (need_pr) {
		if (lst_id != pdo->op_crr) {
			dbg_dt("do_table2:%s:%s->%s\n", pdo->name,
			       pdo->ptab[lst_id].name,
			       pdo->ptab[pdo->op_crr].name);
		}
	}
}

/***********************************************/
void dim_bypass_st_clear(struct di_ch_s *pch)
{
	pch->bypass.d32 = 0;
}

void dim_bypass_set(struct di_ch_s *pch, bool which, unsigned int reason)
{
	bool on = false;
	struct dim_policy_s *pp = get_dpolicy();

	if (reason)
		on = true;

	if (!which) {
		if (pch->bypass.b.lst_n != on) {
			dbg_pl("ch[%d]:bypass change:n:%d->%d\n",
			       pch->ch_id,
			       pch->bypass.b.lst_n,
			       on);
			pch->bypass.b.lst_n = on;
		}
		if (on) {
			pch->bypass.b.need_bypass = 1;
			pch->bypass.b.reason_n = reason;
			pp->ch[pch->ch_id] = 0;
		} else {
			pch->bypass.b.need_bypass = 0;
			pch->bypass.b.reason_n = 0;
		}
	} else {
		if (pch->bypass.b.lst_i != on) {
			dbg_pl("ch[%d]:bypass change:i:%d->%d\n",
			       pch->ch_id,
			       pch->bypass.b.lst_i,
			       on);
			pch->bypass.b.lst_i = on;
		}
		if (on) {
			pch->bypass.b.is_bypass = 1;
			pch->bypass.b.reason_i = reason;
			pp->ch[pch->ch_id] = 0;
		} else {
			pch->bypass.b.is_bypass = 0;
			pch->bypass.b.reason_i = 0;
		}
	}
}

#define DIM_POLICY_STD_OLD	(125)
#define DIM_POLICY_STD		(250)
#define DIM_POLICY_NOT_LIMIT	(2000)
#define DIM_POLICY_SHIFT_H	(7)
#define DIM_POLICY_SHIFT_W	(6)

void dim_polic_cfg_local(unsigned int cmd, bool on)
{
	struct dim_policy_s *pp;

	if (dil_get_diffver_flag() != 1)
		return;

	pp = get_dpolicy();
	switch (cmd) {
	case K_DIM_BYPASS_CLEAR_ALL:
		pp->cfg_d32 = 0x0;
		break;
	case K_DIM_I_FIRST:
		pp->cfg_b.i_first = on;
		break;
	case K_DIM_BYPASS_ALL_P:
#ifdef TEST_DISABLE_BYPASS_P
		PR_INF("%s:get bypass p cmd, do nothing\n", __func__);
#else
		pp->cfg_b.bypass_all_p = on;
#endif
		break;
	default:
		PR_WARN("%s:cmd is overflow[%d]\n", __func__, cmd);
		break;
	}
}

//EXPORT_SYMBOL(dim_polic_cfg);

void dim_polic_prob(void)
{
	struct dim_policy_s *pp = get_dpolicy();
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (DIM_IS_IC_EF(SC2) || DIM_IS_IC(TM2B) || DIM_IS_IC(T5)) {
		if (de_devp->clkb_max_rate >= 340000000)
			pp->std = DIM_POLICY_NOT_LIMIT;
		else if (de_devp->clkb_max_rate > 300000000)
			pp->std = DIM_POLICY_STD;
		else
			pp->std = DIM_POLICY_STD_OLD;
	} else if (DIM_IS_IC(T5D) || DIM_IS_IC(T5DB)) {
#ifdef TEST_DISABLE_BYPASS_P
		pp->std = DIM_POLICY_NOT_LIMIT;
#else
			pp->std = DIM_POLICY_STD;
#endif
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
#ifdef TEST_DISABLE_BYPASS_P
		pp->std = DIM_POLICY_NOT_LIMIT;
#else
		if (de_devp->clkb_max_rate >= 340000000)
			pp->std = DIM_POLICY_STD;
		else
			pp->std = DIM_POLICY_STD_OLD;
#endif
	} else {
		pp->std = DIM_POLICY_STD_OLD;
	}

	pp->cfg_b.i_first = 1;
}

void dim_polic_unreg(struct di_ch_s *pch)
{
	struct dim_policy_s *pp	= get_dpolicy();
	unsigned int ch;

	ch = pch->ch_id;

	pp->ch[ch] = 0;
	pp->order_i &= ~(1 << ch);
}

unsigned int dim_polic_is_bypass(struct di_ch_s *pch, struct vframe_s *vf)
{
	struct dim_policy_s *pp	= get_dpolicy();
	unsigned int reason = 0;
	unsigned int ptt, pcu, i;
	unsigned int ch;
	unsigned int x, y;

	ch = pch->ch_id;

	/* cfg */
	if (pp->cfg_d32) {
		if (!VFMT_IS_I(vf->type)) {
			if (dim_cfg & DI_BIT0) {
				reason = 0x60;
			} else if (pp->cfg_b.bypass_all_p) {
				reason = 0x61;
			} else if (pp->cfg_b.i_first && pp->order_i) {
				reason = 0x62;
				dbg_pl("ch[%d],bapass for order\n",
				       ch);
			}
		}
	}
	if (reason)
		return reason;

	if (!vf) {
		pr_info("no_vf\n");
		dump_stack();
		return reason;
	}
	dim_vf_x_y(vf, &x, &y);
	/*count total*/
	ptt = 0;
	for (i = 0; i < DI_CHANNEL_NUB; i++)
		ptt += pp->ch[i];

	ptt -= pp->ch[ch];

	/*count current*/
	pcu = (y >> DIM_POLICY_SHIFT_H) *
		(x >> DIM_POLICY_SHIFT_W);
	if (VFMT_IS_I(vf->type))
		pcu >>= 1;

	/*check bypass*/
	if ((ptt + pcu) > pp->std) {
		/* bypass */
		reason = 0x63;
		pp->ch[ch] = 0;
	} else {
		pp->ch[ch] = pcu;
	}

	if (pp->cfg_b.i_first	&&
	    VFMT_IS_I(vf->type)	&&
	    reason) {
		pp->order_i |= (1 << ch);
		dbg_pl("ch[%d],bapass order[1]\n", ch);
	} else {
		pp->order_i &= ~(1 << ch);
	}
	return reason;
}

unsigned int dim_get_trick_mode(void)
{
	unsigned int trick_mode;

	if (dimp_get(edi_mp_bypass_trick_mode)) {
		int trick_mode_fffb = 0;
		int trick_mode_i = 0;

		if (dimp_get(edi_mp_bypass_trick_mode) & 0x1)
			query_video_status(0, &trick_mode_fffb);
		if (dimp_get(edi_mp_bypass_trick_mode) & 0x2)
			query_video_status(1, &trick_mode_i);
		trick_mode =
			trick_mode_fffb | (trick_mode_i << 1);

		return trick_mode;
	}
	return 0;
}

static enum EDPST_MODE dim_cnt_mode(struct di_ch_s *pch)
{
	enum EDPST_MODE mode;

	if (dim_cfg_nv21()) {
		mode = EDPST_MODE_NV21_8BIT;
	} else {
		if (dimp_get(edi_mp_nr10bit_support)) {
			if (dimp_get(edi_mp_full_422_pack))
				mode = EDPST_MODE_422_10BIT_PACK;
			else
				mode = EDPST_MODE_422_10BIT;
		} else {
			mode = EDPST_MODE_422_8BIT;
		}
	}
	return mode;
}

/****************************/
bool dip_is_support_4k(unsigned int ch)
{
	struct di_ch_s *pch = get_chdata(ch);
	unsigned int val_4k;

	val_4k = cfggch(pch, 4K) & 0x7; //bit[2:0]: for 4k enable

	if (val_4k == 1 ||
	    (val_4k == 2 && IS_VDIN_SRC(pch->src_type)))
		return true;
	return false;
}

bool dip_cfg_is_pps_4k(unsigned int ch)
{
	struct di_ch_s *pch = get_chdata(ch);

	return (cfggch(pch, 4K) & 0x8) ? true : false;
}

void dip_pps_cnt_hv(unsigned int *w_in, unsigned int *h_in)
{
	unsigned int w, h;
	unsigned int nw, nh;
	unsigned int div;

	//3840 x 2160
	w = *w_in;
	h = *h_in;

	div = (1920 << 10) / w; //(w << 10) / 1920;
	nh = (div * h) >> 10 ;//(div * h) >> 10;
	if (nh > 1088) {
		div = (1088 << 10) / h;
		nw = (div * w) >> 10;
		nh = 1088;
		if (nw % 2)
			nw--;
	} else {
		nw = 1920;
		if (nh % 2)
			nh--;
	}
	PR_INF("%s:<%u,%u>-><%u,%u>\n", "pps:", *w_in, *h_in, nw, nh);
	*w_in	= nw;
	*h_in	= nh;
}

bool dip_is_support_nv2110(unsigned int ch)
{
	struct di_ch_s *pch = get_chdata(ch);

	if ((cfggch(pch, POUT_FMT) == 6) && (!IS_VDIN_SRC(pch->src_type)))
		return true;
	return false;
}

bool dip_is_4k_sct_mem(struct di_ch_s *pch)
{
	if ((cfggch(pch, ALLOC_SCT) & DI_BIT0))
		return true;

	return false;
}

bool dip_is_ponly_sct_mem(struct di_ch_s *pch)
{
	if ((cfggch(pch, ALLOC_SCT) & DI_BIT1))
		return true;

	return false;
}

void dip_init_value_reg(unsigned int ch, struct vframe_s *vframe)
{
	struct di_post_stru_s *ppost;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	struct di_ch_s *pch = get_chdata(ch);
	struct div2_mm_s *mm;
	enum EDI_SGN sgn;
	unsigned int post_nub;
	bool ponly_enable = false;
	bool ponly_by_firstp = false;

	dbg_reg("%s:ch[%d]\n", __func__, ch);

	/*post*/
	ppost = get_post_stru(ch);
	/*keep buf:*/
	/*keep_post_buf = ppost->keep_buf_post;*/

	memset(ppost, 0, sizeof(struct di_post_stru_s));
	ppost->next_canvas_id = 1;

	/*pre*/
	memset(ppre, 0, sizeof(struct di_pre_stru_s));

	/* bypass state */
	dim_bypass_st_clear(pch);
	if (pch->itf.op_cfg_ch_set)
		pch->itf.op_cfg_ch_set(pch);
	di_hf_reg(pch);
	dim_dw_reg(pch);
	/* check format */
	if (!dip_itf_is_ins_exbuf(pch)) {
		if (cfggch(pch, POUT_FMT) < 3 &&
		    cfggch(pch, IOUT_FMT) < 3 &&
		    cfggch(pch, ALLOC_SCT)) {
			cfgsch(pch, ALLOC_SCT, 0);
			PR_INF("%s:chang alloc_sct\n", __func__);
		}
	}

	if (cfgg(PMODE) == 2) {
		//prog_proc_config = 3;
		dimp_set(edi_mp_prog_proc_config, 3);
		//use_2_interlace_buff = 2;
		dimp_set(edi_mp_use_2_interlace_buff, 2);

	} else {
		//prog_proc_config = 0x23;
		//use_2_interlace_buff = 1;
		dimp_set(edi_mp_prog_proc_config, 0x23);
		dimp_set(edi_mp_use_2_interlace_buff, 1);
	}
	pch->src_type = vframe->source_type;
	if ((vframe->flag & VFRAME_FLAG_DI_P_ONLY) || bget(&dim_cfg, 1))
		ponly_enable = true;

	if (!ponly_enable &&
	    cfggch(pch, PONLY_MODE) == 1 &&
	    (vframe->type & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE) {
		ponly_enable = true;
		ponly_by_firstp = true;
	}
	if (ponly_enable) {
		pch->ponly = true;
		pch->rsc_bypass.b.ponly_fst_cnt = cfggch(pch, PONLY_BP_THD);
	} else {
		pch->ponly = false;
		pch->rsc_bypass.b.ponly_fst_cnt = 0;
	}

	if (dim_afds())
		di_set_default(ch);

	mm = dim_mm_get(ch);

	sgn = di_vframe_2_sgn(vframe);
	ppre->sgn_lv = sgn;

	if (cfgg(FIX_BUF)) {
		if (dim_afds()	&&
		    dip_is_support_4k(ch)) {
			memcpy(&mm->cfg, &c_mm_cfg_fix_4k,
			       sizeof(struct di_mm_cfg_s));
		} else {
			memcpy(&mm->cfg, &c_mm_cfg_normal,
			       sizeof(struct di_mm_cfg_s));
		}
		mm->cfg.fix_buf = 1;
	} else if (pch->ponly && dip_is_ponly_sct_mem(pch)) {
		if (!dip_is_support_4k(ch))
			memcpy(&mm->cfg, &c_mm_cfg_normal, sizeof(struct di_mm_cfg_s));
		else
			memcpy(&mm->cfg, &c_mm_cfg_4k, sizeof(struct di_mm_cfg_s));
	} else if (!dip_is_support_4k(ch)) {
		memcpy(&mm->cfg, &c_mm_cfg_normal, sizeof(struct di_mm_cfg_s));
	} else if (sgn <= EDI_SGN_HD) {
		memcpy(&mm->cfg, &c_mm_cfg_normal, sizeof(struct di_mm_cfg_s));
		if ((cfggch(pch, POUT_FMT) == 4) || (cfggch(pch, POUT_FMT) == 6))
			mm->cfg.dis_afbce = 1;
	} else if ((sgn <= EDI_SGN_4K)	&&
		 dim_afds()		&&
		 dip_is_support_4k(ch)) {
		memcpy(&mm->cfg, &c_mm_cfg_4k, sizeof(struct di_mm_cfg_s));
	} else {
		memcpy(&mm->cfg, &c_mm_cfg_bypass, sizeof(struct di_mm_cfg_s));
	}

	if (pch->ponly && dip_is_ponly_sct_mem(pch))
		mm->cfg.dis_afbce = 0;
	else if ((cfggch(pch, POUT_FMT) <= 2))
		mm->cfg.dis_afbce = 1;

	if (cfgg(FIX_BUF))
		mm->cfg.fix_buf = 1;
	else
		mm->cfg.fix_buf = 0;

	if (pch->ponly)
		mm->cfg.num_local = 0;

	post_nub = cfggch(pch, POST_NUB);
	if ((post_nub) && post_nub < POST_BUF_NUM)
		mm->cfg.num_post = post_nub;

	PR_INF("%s:ch[%d]:fix_buf:%d;ponly <%d,%d>\n",
	       "value reg",
	       ch,
	       mm->cfg.fix_buf,
	       pch->ponly, ponly_by_firstp);

	pch->mode = dim_cnt_mode(pch);
}

enum EDI_SGN di_vframe_2_sgn(struct vframe_s *vframe)
{
	unsigned int h, v;
	enum EDI_SGN sgn;

#ifdef HIS_CODE
	if (IS_COMP_MODE(vframe->type)) {
		h = vframe->compWidth;
		v = vframe->compHeight;
	} else {
		h = vframe->width;
		v = vframe->height;
	}
#endif
	dim_vf_x_y(vframe, &h, &v);

	if (h <= 1280 && v <= 720) {
		sgn = EDI_SGN_SD;
	} else if (h <= 1920 && v <= 1088) {
		sgn = EDI_SGN_HD;
	} else if (h <= 3840 && v <= 2660 &&
		   IS_PROG(vframe->type)) {
		sgn = EDI_SGN_4K;
	} else {
		sgn = EDI_SGN_OTHER;
	}

	return sgn;
}

/************************************************
 *
 ************************************************/
static void pq_sv_db_ini(void)
{
	struct db_save_s *dbp;
	/* dct */
	if (DIM_IS_IC(T5)) {
		dbp = &get_datal()->db_save[DIM_DB_SV_DCT_BL2];
		dbp->support = 1;
		dbp->addr	= DCTR_BLENDING2;
	} else if (DIM_IS_IC_EF(T3)) {
		dbp = &get_datal()->db_save[DIM_DB_SV_DCT_BL2];
		dbp->support = 1;
		dbp->addr	= DCTR_T3_BLENDING2;
	}
}

void dim_pq_db_setreg(unsigned int nub, unsigned int *preg)
{
	unsigned int i;
	struct db_save_s *dbp;
	unsigned int nubl;

	if (dil_get_diffver_flag() != 1)
		return;

	nubl = nub;
	if (nubl > DIM_DB_SAVE_NUB) {
		PR_ERR("%s:nub is overflow %d\n",
			 __func__, nub);
		nubl = DIM_DB_SAVE_NUB;
	}

	for (i = 0; i < nubl; i++) {
		dbp = &get_datal()->db_save[i];
		dbp->addr = preg[i];
		PR_INF("%s:reg:0x%x\n", __func__, preg[i]);
	}
}
EXPORT_SYMBOL(dim_pq_db_setreg);

/**************************************************
 * pdate:
 *	value / mask
 *	need keep same order with di_patch_mov_setreg
 **************************************************/
bool dim_pq_db_sel(unsigned int idx, unsigned int mode, unsigned int *pdate)
{
	struct db_save_s *dbp;
	//int i;
	bool ret = true;

	if (dil_get_diffver_flag() != 1)
		return false;

	if (idx >= DIM_DB_SAVE_NUB)
		return false;

	dbp = &get_datal()->db_save[idx];

	if (!dbp->support)
		return false;

	switch (mode) {
	case 0:/*setting from db*/
		dbp->mode = 0;
		dbp->update = 1;
		break;
	case 1:/*setting from pq*/
		dbp->update = 0;

		dbp->val_pq = pdate[0];
		dbp->mask = pdate[1];
		dbp->en_pq = true;

		dbp->mode = 1;
		dbp->update = true;

		break;
	default:
		ret = false;
		break;
	}
	return ret;
}
EXPORT_SYMBOL(dim_pq_db_sel);

void di_pq_db_setting(enum DIM_DB_SV idx)
{
	unsigned int val, vall;
	struct db_save_s *dbp;
	const struct reg_acc *op = &di_pre_regset;

	if (idx >= DIM_DB_SAVE_NUB)
		return;

	dbp = &get_datal()->db_save[idx];

	if (!dbp->support ||
	    dbp->mode < 0	||
	    dbp->mode > 1	||
	    !dbp->update)
		return;
	dbg_pq("%s:1\n", __func__);
	if (dbp->mode == 0 && dbp->en_db) {
		val = dbp->val_db;
	} else if ((dbp->mode == 1) && (dbp->en_pq)) {
		val = dbp->val_pq;
	} else {
		dbg_pq("%s:mode[%d], en[%d,%d]\n",
			__func__, dbp->mode,
			dbp->en_db,
			dbp->en_pq);
		return;
	}

	if (dbp->mask != 0xffffffff) {
		vall = ((op->rd(dbp->addr) &
			(~(dbp->mask))) |
			(val & dbp->mask));
	} else {
		vall = val;
	}
	op->wr(dbp->addr, vall);
	dbg_pq("%s:0x%x,0x%x\n", __func__, dbp->addr, val);
	dbp->update = 0;
}

struct dim_rpt_s *dim_api_getrpt(struct vframe_s *vfm)
{
	struct di_buf_s *di_buf;

	if (dil_get_diffver_flag() != 1)
		return NULL;

	if (!vfm)
		return NULL;
	if (!vfm->private_data ||
	    ((vfm->type & VIDTYPE_DI_PW) == 0))
		return NULL;

	di_buf = (struct di_buf_s *)vfm->private_data;
	return &di_buf->pq_rpt;
}
EXPORT_SYMBOL(dim_api_getrpt);

void dim_pqrpt_init(struct dim_rpt_s *rpt)
{
	if (!rpt)
		return;

	dim_print("%s:\n", __func__);
	memset(rpt, 0, sizeof(*rpt));
	rpt->version = 1;
}

/*******************************************/
static bool dip_init_value(void)
{
	unsigned int ch;
	struct di_post_stru_s *ppost;
	struct div2_mm_s *mm;
	struct dim_mm_t_s *mmt = dim_mmt_get();
	struct di_ch_s *pch;
	bool ret = false;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pch = get_chdata(ch);
		pch->ch_id = ch;
		ppost = get_post_stru(ch);
		memset(ppost, 0, sizeof(struct di_post_stru_s));
		ppost->next_canvas_id = 1;

		/*que*/
		ret = di_que_alloc(ch);
		if (ret) {
//			pw_queue_clear(ch, QUE_POST_KEEP);
//			pw_queue_clear(ch, QUE_POST_KEEP_BACK);
		}
		mm = dim_mm_get(ch);
		memcpy(&mm->cfg, &c_mm_cfg_normal, sizeof(struct di_mm_cfg_s));
	}
	mmt->mem_start = 0;
	mmt->mem_size = 0;
	mmt->total_pages = NULL;
	set_current_channel(0);

	return ret;
}

/************************************************
 * ins
 ************************************************/
static const struct que_creat_s qbf_nin_cfg_q[] = {//TST_Q_IN_NUB
	[QBF_NINS_Q_IDLE] = {
		.name	= "QBF_NINS_Q_IDLE",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_NINS_Q_CHECK] = {
		.name	= "QBF_NINS_Q_CHECK",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_NINS_Q_USED] = {
		.name	= "QBF_NINS_Q_USED",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_NINS_Q_RECYCL] = {
		.name	= "QBF_NINS_Q_RECYCL",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_NINS_Q_USEDB] = {
		.name	= "QBF_NINS_Q_USEDB",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_NINS_Q_DCT] = {
		.name	= "QBF_NINS_Q_DCT",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_NINS_Q_DCT_DOING] = {
		.name	= "QBF_NINS_Q_DCT_DOING",
		.type	= Q_T_FIFO,
		.lock	= 0,
	}
};

static const struct qbuf_creat_s qbf_nin_cfg_qbuf = {
	.name	= "qbuf_nin",
	.nub_que	= QBF_NINS_Q_NUB,
	.nub_buf	= DIM_NINS_NUB,
	.code		= CODE_NIN,
};

void bufq_nin_int(struct di_ch_s *pch)
{
	struct dim_nins_s *nin;
	struct buf_que_s *pbufq;
	int i;

	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	/* clear buf*/
	nin = &pch->nin_bf[0];
	memset(nin, 0, sizeof(*nin) * DIM_NINS_NUB);

	/* set buf's header */
	for (i = 0; i < DIM_NINS_NUB; i++) {
		nin = &pch->nin_bf[i];

		nin->header.code	= CODE_NIN;
		nin->header.index	= i;
		nin->header.ch	= pch->ch_id;
	}

	pbufq = &pch->nin_qb;
	/*clear bq */
	memset(pbufq, 0, sizeof(*pbufq));

	/* point to resource */
	for (i = 0; i < QBF_NINS_Q_NUB; i++)
		pbufq->pque[i] = &pch->nin_q[i];

	for (i = 0; i < DIM_NINS_NUB; i++)
		pbufq->pbuf[i].qbc = &pch->nin_bf[i].header;

	qbuf_int(pbufq, &qbf_nin_cfg_q[0], &qbf_nin_cfg_qbuf);

	/* all to idle */
	qbuf_in_all(pbufq, QBF_NINS_Q_IDLE);
	dbg_reg("%s:\n", __func__);
}

void bufq_nin_exit(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;

	PR_INF("%s:\n", __func__);
	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	pbufq = &pch->nin_qb;

	qbuf_release_que(pbufq);
}

/*********************************
 * dependent:
 *	pch->itf->etype
 * Be dependent:
 *	vfm input
 *********************************/
void bufq_nin_reg(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	int i;
	struct dim_nins_s *nin;

	dbg_reg("%s:\n", __func__);
	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	pbufq = &pch->nin_qb;

	qbuf_reset(pbufq);
	/* all to idle */
	qbuf_in_all(pbufq, QBF_NINS_Q_IDLE);

	for (i = 0; i < DIM_NINS_NUB; i++) {
		nin = &pch->nin_bf[i];
		/* set etype */
		nin->etype = pch->itf.etype;
		memset(&nin->c, 0, sizeof(nin->c));
	}
	//qbuf_dbg_checkid(pbufq, 1);
}

struct dim_nins_s *nins_peek(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;
	bool ret;

	pbufq = &pch->nin_qb;
	//qbuf_peek_s(pbufq, QBF_INS_Q_IN, q_buf);
	if (get_datal()->dct_op && get_datal()->dct_op->is_en(pch))
		ret = qbufp_peek(pbufq, QBF_NINS_Q_DCT, &q_buf);
	else
		ret = qbufp_peek(pbufq, QBF_NINS_Q_CHECK, &q_buf);
	if (!ret || !q_buf.qbc)
		return NULL;
	ins = (struct dim_nins_s *)q_buf.qbc;

	return ins;
}

struct dim_nins_s *nins_peek_pre(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;
	bool ret;

	pbufq = &pch->nin_qb;
	//qbuf_peek_s(pbufq, QBF_INS_Q_IN, q_buf);
	ret = qbufp_peek(pbufq, QBF_NINS_Q_CHECK, &q_buf);
	if (!ret || !q_buf.qbc)
		return NULL;
	ins = (struct dim_nins_s *)q_buf.qbc;

	return ins;
}

struct vframe_s *nins_peekvfm(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;
	struct vframe_s *vfm;
	bool ret;

	pbufq = &pch->nin_qb;
	//qbuf_peek_s(pbufq, QBF_INS_Q_IN, q_buf);
	if (get_datal()->dct_op && get_datal()->dct_op->is_en(pch))
		ret = qbufp_peek(pbufq, QBF_NINS_Q_DCT, &q_buf);
	else
		ret = qbufp_peek(pbufq, QBF_NINS_Q_CHECK, &q_buf);
	if (!ret || !q_buf.qbc)
		return NULL;
	ins = (struct dim_nins_s *)q_buf.qbc;
	vfm = &ins->c.vfm_cp;

	return vfm;
}

struct vframe_s *nins_peekvfm_pre(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;
	struct vframe_s *vfm;
	bool ret;

	pbufq = &pch->nin_qb;

	ret = qbufp_peek(pbufq, QBF_NINS_Q_CHECK, &q_buf);
	if (!ret || !q_buf.qbc)
		return NULL;
	ins = (struct dim_nins_s *)q_buf.qbc;
	vfm = &ins->c.vfm_cp;

	return vfm;
}

/* from check to used */
struct dim_nins_s *nins_get(struct di_ch_s *pch)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;

	pbufq = &pch->nin_qb;

	ret = qbuf_out(pbufq, QBF_NINS_Q_CHECK, &index);
	if (!ret)
		return NULL;

	q_buf = pbufq->pbuf[index];
	ins = (struct dim_nins_s *)q_buf.qbc;

	qbuf_in(pbufq, QBF_NINS_Q_USED, index);
	//qbuf_dbg_checkid(pbufq, 2);

	return ins;
}

struct dim_nins_s *nins_dct_get(struct di_ch_s *pch)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;

	pbufq = &pch->nin_qb;

	ret = qbuf_out(pbufq, QBF_NINS_Q_DCT, &index);
	if (!ret)
		return NULL;

	q_buf = pbufq->pbuf[index];
	ins = (struct dim_nins_s *)q_buf.qbc;

//	qbuf_in(pbufq, QBF_NINS_Q_DCT_DOING, index);
	//qbuf_dbg_checkid(pbufq, 2);

	return ins;
}

/* check-> done */
struct dim_nins_s *nins_dct_get_bypass(struct di_ch_s *pch)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;

	pbufq = &pch->nin_qb;

	ret = qbuf_out(pbufq, QBF_NINS_Q_DCT, &index);
	if (!ret)
		return NULL;

	q_buf = pbufq->pbuf[index];
	ins = (struct dim_nins_s *)q_buf.qbc;

	qbuf_in(pbufq, QBF_NINS_Q_CHECK, index);
	//qbuf_dbg_checkid(pbufq, 2);

	return ins;
}

bool nins_dct_2_done(struct di_ch_s *pch, struct dim_nins_s *nins)
{
	bool ret;
	struct buf_que_s *pbufq;

	pbufq = &pch->nin_qb;

	ret = qbuf_in(pbufq, QBF_NINS_Q_CHECK, nins->header.index);

	return ret;
}

/* in_used to recycle*/
bool nins_out_some(struct di_ch_s *pch,
		   struct dim_nins_s *ins,
		   unsigned int q)
{
//	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;

	pbufq = &pch->nin_qb;
	q_buf.qbc = &ins->header;

	ret = qbuf_out_some(pbufq, q, q_buf);
	if (!ret) {
		PR_ERR("%s:\n", __func__);
//		dbg_hd_print(&ins->header);
//		dbg_q_listid_print(pbufq);
	}
	//qbuf_dbg_checkid(pbufq, 3);

	return ret;
}

/* in_used to recycle*/
bool nins_used_some_to_recycle(struct di_ch_s *pch, struct dim_nins_s *ins)
{
//	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;

	pbufq = &pch->nin_qb;
	q_buf.qbc = &ins->header;
	//qbuf_dbg_checkid(pbufq, 5);

	if (ins->header.index >= DIM_NINS_NUB) {
		PR_ERR("%s:%d:overflow\n", "nin_some", ins->header.index);
		return false;
	}
	ret = qbuf_out_some(pbufq, QBF_NINS_Q_USED, q_buf);
	if (!ret) {
		PR_ERR("%s:\n", __func__);
		dbg_hd_print(&ins->header);
//		dbg_q_listid_print(pbufq);
		return false;
	}
	#ifdef MARK_HIS
	PR_INF("%s:%d:0x%x\n", __func__,
		ins->header.index,
		ins->header.code);
	#endif
	ret = qbuf_in(pbufq, QBF_NINS_Q_RECYCL, ins->header.index);
	if (!ret) {
		PR_ERR("%s:in failed\n", __func__);
		return false;
	}
	//qbuf_dbg_checkid(pbufq, 4);

	return true;
}

/* from check to used */
struct dim_nins_s *nins_move(struct di_ch_s *pch,
			     unsigned int qf,
			     unsigned int qt)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_nins_s *ins;

	pbufq = &pch->nin_qb;
	if (qf >= pbufq->nub_que ||
	    qt >= pbufq->nub_que ||
	    qf == qt) {
		PR_ERR("%s:%d->%d\n", __func__, qf, qt);
		return NULL;
	}
	ret = qbuf_out(pbufq, qf, &index);
	if (!ret) {
		PR_ERR("%s:out:%d->%d\n", __func__, qf, qt);
		return NULL;
	}

	q_buf = pbufq->pbuf[index];
	ins = (struct dim_nins_s *)q_buf.qbc;

	qbuf_in(pbufq, qt, index);
	qbuf_dbg_checkid(pbufq, 5);

	return ins;
}

unsigned int nins_cnt_used_all(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	unsigned int len_check, len_used, len_rc;

	pbufq = &pch->nin_qb;
	len_check	= qbufp_count(pbufq, QBF_NINS_Q_CHECK);
	len_used	= qbufp_count(pbufq, QBF_NINS_Q_USED);
	len_rc		= qbufp_count(pbufq, QBF_NINS_Q_RECYCL);

	return len_check + len_used + len_rc;
}

unsigned int nins_cnt(struct di_ch_s *pch, unsigned int q)
{
	struct buf_que_s *pbufq;

	pbufq = &pch->nin_qb;
	return qbufp_count(pbufq, q);
}

void dbg_nins_log_buf(struct di_buf_s *di_buf, unsigned int dbgid)
{
	struct dim_nins_s *ins;

	if (di_buf && di_buf->c.in) {
		ins = (struct dim_nins_s *)di_buf->c.in;
	} else {
		PR_INF("no in\n");
		return;
	}

	PR_INF("%s:%d:%d->%d,0x%x,0x%px\n", "nins",
		dbgid, di_buf->index,
		ins->header.index,
		ins->header.code,
		ins);
}

void dbg_nins_check_id(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;

	pbufq = &pch->nin_qb;
	qbuf_dbg_checkid(pbufq, 10);
}

static bool di_buf_clear(struct di_ch_s *pch, struct di_buf_s *di_buf);

/************************************************
 * new display
 ************************************************/
static const struct que_creat_s qbf_ndis_cfg_q[] = {//TST_Q_IN_NUB
	[QBF_NDIS_Q_IDLE] = {
		.name	= "QBF_NDIS_Q_IDLE",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_NDIS_Q_USED] = {
		.name	= "QBF_NDIS_Q_USED",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
	[QBF_NDIS_Q_DISPLAY] = {
		.name	= "QBF_NDIS_Q_DISPLAY",
		.type	= Q_T_N,
		.lock	= 1,
	},
#ifdef MARK_HIS
	[QBF_NDIS_Q_BACK] = {
		.name	= "QBF_NDIS_Q_BACK",
		.type	= Q_T_FIFO,
		.lock	= 0,
	},
#endif
	[QBF_NDIS_Q_KEEP] = {
		.name	= "QBF_NDIS_Q_KEEP",
		.type	= Q_T_N,
		.lock	= 0,
	}
};

static const struct qbuf_creat_s qbf_ndis_cfg_qbuf = {
	.name	= "qbuf_ndis",
	.nub_que	= QBF_NDIS_Q_NUB,
	.nub_buf	= DIM_NDIS_NUB,
	.code		= CODE_NDIS,
};

void bufq_ndis_int(struct di_ch_s *pch)
{
	struct dim_ndis_s *dis;
	struct buf_que_s *pbufq;
	int i;

	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}

	/* clear buf*/
	dis = &pch->ndis_bf[0];
	memset(dis, 0, sizeof(*dis) * DIM_NDIS_NUB);

	/* set buf's header */
	for (i = 0; i < DIM_NDIS_NUB; i++) {
		dis = &pch->ndis_bf[i];

		dis->header.code	= CODE_NDIS;
		dis->header.index	= i;
		dis->header.ch	= pch->ch_id;
	}

	pbufq = &pch->ndis_qb;
	/*clear bq */
	memset(pbufq, 0, sizeof(*pbufq));

	/* point to resource */
	for (i = 0; i < QBF_NDIS_Q_NUB; i++)
		pbufq->pque[i] = &pch->ndis_q[i];

	for (i = 0; i < DIM_NDIS_NUB; i++)
		pbufq->pbuf[i].qbc = &pch->ndis_bf[i].header;

	qbuf_int(pbufq, &qbf_ndis_cfg_q[0], &qbf_ndis_cfg_qbuf);

	/* all to idle */
	qbuf_in_all(pbufq, QBF_NDIS_Q_IDLE);
	dbg_reg("%s:\n", __func__);
}

void bufq_ndis_exit(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;

	PR_INF("%s:\n", __func__);
	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	pbufq = &pch->ndis_qb;

	qbuf_release_que(pbufq);
}

struct dim_ndis_s *ndisq_peek(struct di_ch_s *pch, unsigned int que)
{
//	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_ndis_s *ndis;

	pbufq = &pch->ndis_qb;

	ret = qbufp_peek(pbufq, que, &q_buf);
	if (!ret)
		return NULL;

	ndis = (struct dim_ndis_s *)q_buf.qbc;

	return ndis;
}

unsigned int ndis_cnt(struct di_ch_s *pch, unsigned int que)
{
	struct buf_que_s *pbufq;
	unsigned int len;

	pbufq = &pch->ndis_qb;
	len	= qbufp_count(pbufq, que);

	return len;
}

struct dim_ndis_s *ndisq_out(struct di_ch_s *pch, unsigned int que)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_ndis_s *ndis;

	pbufq = &pch->ndis_qb;

	ret = qbuf_out(pbufq, que, &index);
	if (!ret)
		return NULL;

	q_buf = pbufq->pbuf[index];
	ndis = (struct dim_ndis_s *)q_buf.qbc;

	return ndis;
}

struct dim_ndis_s *ndis_get_fromid(struct di_ch_s *pch, unsigned int idx)
{
	struct dim_ndis_s *ndis;
	struct buf_que_s *pbufq;

	pbufq = &pch->ndis_qb;
	if (idx >= pbufq->nub_buf) {
		PR_ERR("%s:ch[%d], %d, overflow\n", __func__, pch->ch_id, idx);
		return NULL;
	}

	ndis = (struct dim_ndis_s *)pbufq->pbuf[idx].qbc;
	dbg_nq("%s:%d\n", __func__, idx);

	return ndis;
}

struct dim_ndis_s *ndis_move(struct di_ch_s *pch,
			     unsigned int qf,
			     unsigned int qt)
{
	unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;
	struct dim_ndis_s *ndis;

	pbufq = &pch->ndis_qb;
	if (qf >= pbufq->nub_que ||
	    qt >= pbufq->nub_que ||
	    qf == qt) {
		PR_ERR("%s:%d->%d\n", __func__, qf, qt);
		return NULL;
	}
	ret = qbuf_out(pbufq, qf, &index);
	if (!ret) {
		PR_ERR("%s:out:%d->%d\n", __func__, qf, qt);
		return NULL;
	}

	q_buf = pbufq->pbuf[index];
	ndis = (struct dim_ndis_s *)q_buf.qbc;

	qbuf_in(pbufq, qt, index);
	if (dbg_unreg_flg)
		PR_WARN("%s:after unreg:%d->%d,%d\n", __func__, qf, qt, index);

	//qbuf_dbg_checkid(pbufq, 5);

	return ndis;
}

/* @ary_note: this will clear, and not used back */
bool ndis_move_display2idle(struct di_ch_s *pch, struct dim_ndis_s *ndis)
{
	//unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;

	pbufq = &pch->ndis_qb;

	q_buf.qbc = &ndis->header;
	ret = qbuf_out_some(pbufq, QBF_NDIS_Q_DISPLAY, q_buf);
	if (!ret) {
		PR_ERR("%s:out:%d\n", __func__, ndis->header.index);
		return NULL;
	}

	/*clear */
	memset(&ndis->c, 0, sizeof(ndis->c));

	qbuf_in(pbufq, QBF_NDIS_Q_IDLE, ndis->header.index);
	//qbuf_dbg_checkid(pbufq, 5);
	dbg_nq("%s:%d\n", __func__, ndis->header.index);
	return ret;
}

bool ndis_is_in_display(struct di_ch_s *pch, struct dim_ndis_s *ndis)
{
	struct buf_que_s *pbufq;
	bool ret;
	union q_buf_u q_buf;

	pbufq = &pch->ndis_qb;
	q_buf.qbc = &ndis->header;

	ret = qbuf_n_is_in(pbufq, QBF_NDIS_Q_DISPLAY, q_buf);
	return ret;
}

bool ndis_is_in_keep(struct di_ch_s *pch, struct dim_ndis_s *ndis)
{
	struct buf_que_s *pbufq;
	bool ret;
	union q_buf_u q_buf;

	pbufq = &pch->ndis_qb;
	q_buf.qbc = &ndis->header;

	ret = qbuf_n_is_in(pbufq, QBF_NDIS_Q_KEEP, q_buf);
	return ret;
}

bool ndis_move_keep2idle(struct di_ch_s *pch, struct dim_ndis_s *ndis)
{
	//unsigned int index;
	bool ret;
	struct buf_que_s *pbufq;
	union q_buf_u q_buf;

	pbufq = &pch->ndis_qb;

	q_buf.qbc = &ndis->header;
	ret = qbuf_out_some(pbufq, QBF_NDIS_Q_KEEP, q_buf);
	if (!ret) {
		PR_ERR("%s:out:%d\n", __func__, ndis->header.index);
		return NULL;
	}

	/*clear */
	memset(&ndis->c, 0, sizeof(ndis->c));

	qbuf_in(pbufq, QBF_NDIS_Q_IDLE, ndis->header.index);
	//qbuf_dbg_checkid(pbufq, 5);
	dbg_keep("%s:%d\n", __func__, ndis->header.index);
	return ret;
}

#ifdef MARK_HIS //no used
/* */
void ndis_back2_idle(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	int i;
	struct dim_ndis_s *ndis;
	unsigned int len;
	bool ret;
//	unsigned int index;
	union q_buf_u q_buf;

	pbufq = &pch->ndis_qb;

	len	= qbufp_count(pbufq, QBF_NDIS_Q_BACK);
	if (!len)
		return;

	for (i = 0; i < len; i++) {
		/* clear */
		ndis = ndisq_out(pch, QBF_NDIS_Q_BACK);
		if (!ndis) {
			PR_ERR("%s:no dis[%d]\n", __func__, i);
			continue;
		}
		q_buf.qbc = &ndis->header;
		ret = qbuf_out_some(pbufq, QBF_NDIS_Q_DISPLAY, q_buf);
		if (!ret)
			PR_ERR("%s, o some [%d]\n", __func__, i);
		qbuf_in(pbufq, QBF_NDIS_Q_IDLE, ndis->header.index);
	}
}
#endif
/* this keep is tmp will clear when unreg */
unsigned int ndis_2keep(struct di_ch_s *pch,
		struct dim_mm_blk_s **blk,
		unsigned int len_max,
		unsigned int disable_mirror)
{
	struct buf_que_s *pbufq;
	int i;
	struct dim_ndis_s *ndis;
	unsigned int len;
//	unsigned int mask;
	bool ret;
	unsigned int index;
	struct di_buf_s *p;
	unsigned int ch;
	unsigned int cnt = 0;

	pbufq = &pch->ndis_qb;

	len	= qbufp_count(pbufq, QBF_NDIS_Q_DISPLAY);

	ch = pch->ch_id;
	if (len) {
		for (i = 0; i < len; i++) {
			ret = qbuf_out(pbufq, QBF_NDIS_Q_DISPLAY, &index);
			if (!ret) {
				PR_ERR("%s:out display [%d]\n", __func__, i);
				continue;
			}
			//PR_INF("%s:out from display %d\n", __func__, index);
			ndis = (struct dim_ndis_s *)pbufq->pbuf[index].qbc;
			p = ndis->c.di_buf;
			ndis->c.blk = p->blk_buf;
			ndis->c.di_buf = NULL;

			if (!p->blk_buf) {
				PR_ERR("%s:no blk:%d\n", __func__, p->index);
				continue;
			}
			//blk[i] = p->blk_buf;
			/* dec vf keep */
			if (p->in_buf) {
				if (p->in_buf->c.in) {
					//nins_used_some_to_recycle(pch,
					//	(struct dim_nins_s*)p->in_buf->c.in);
					p->in_buf->c.in = NULL;
				}

				queue_in(ch, p->in_buf, QUEUE_RECYCLE);
				p->in_buf = NULL;
			}

			/* @ary_note: clear di_buf */
			di_buf_clear(pch, p);
			di_que_in(pch->ch_id, QUE_PST_NO_BUF, p);

			//memset(&ndis->c, 0, sizeof(ndis->c));
			//PR_INF("%s:to keep %d\n", __func__,
			//	ndis->header.index);
			if (disable_mirror) {
				/* clear */
				memset(&ndis->c, 0, sizeof(ndis->c));
				ndis->etype = EDIM_NIN_TYPE_NONE;
				qbuf_in(pbufq, QBF_NDIS_Q_IDLE, ndis->header.index);
			} else {
				ndis_dbg_print2(ndis, "to keep");
				qbuf_in(pbufq, QBF_NDIS_Q_KEEP, ndis->header.index);
				cnt++;
			}
		}
	}
	len	= qbufp_count(pbufq, QBF_NDIS_Q_KEEP);

	if (!len)
		return 0;

	if (len > len_max)
		len = len_max;
	qbufp_list(pbufq, QBF_NDIS_Q_KEEP);
	cnt = 0;
	for (i = 0; i < len; i++) {
		if (pbufq->list_id[i] >= pbufq->nub_buf) {
			PR_ERR("%s:cnt list err:%d,%d\n", __func__,
				len, i);
			break;
		}
		index = pbufq->list_id[i];
		ndis = (struct dim_ndis_s *)pbufq->pbuf[index].qbc;
		if (!ndis->c.blk) {
			PR_ERR("%s:no blk %d, %d\n", __func__, len, i);
			continue;
		}

		blk[i] = ndis->c.blk;
		cnt++;
	}

	//PR_INF("%s:cnt[%d]\n", __func__, cnt);
	pch->sts_unreg_dis2keep = (unsigned char)cnt;
	return cnt;
}

void bufq_ndis_unreg(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	int i;
	struct dim_ndis_s *ndis;
	unsigned int len;
//	union q_buf_u q_buf;
//	bool ret;

	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	pbufq = &pch->ndis_qb;

	/* clear used */
	len = qbufp_count(pbufq, QBF_NDIS_Q_USED);
	if (len) {
		for (i = 0; i < len; i++) {
			ndis = ndisq_out(pch, QBF_NDIS_Q_USED);
			if (!ndis) {
				PR_ERR("%s:no dis[%d]\n", __func__, i);
				continue;
			}
			/* clear */
			memset(&ndis->c, 0, sizeof(ndis->c));
			ndis->etype = EDIM_NIN_TYPE_NONE;
			qbuf_in(pbufq, QBF_NDIS_Q_IDLE, ndis->header.index);
			PR_INF("%s:used 2 idle %d\n", __func__, ndis->header.index);
		}
	}
	//dbg_unreg_flg = 1;
	dbg_reg("%s:%d\n", __func__, len);

	/* check keep */

	//qbuf_reset(pbufq);
	/* all to idle */
	//qbuf_in_all(pbufq, QBF_NDIS_Q_IDLE);
#ifdef MARK_HIS
	for (i = 0; i < DIM_NDIS_NUB; i++) {
		ndis = &pch->ndis_bf[i];
		/* set etype */
		ndis->etype = EDIM_NIN_TYPE_NONE;
		memset(&ndis->c, 0, sizeof(ndis->c));
	}
#endif
	//qbuf_dbg_checkid(pbufq, 1);
}

void ndis_dbg_content(struct seq_file *s, struct dim_ndis_s *ndis)
{
	if (!ndis) {
		seq_printf(s, "\t%s\n", "no_ndis");
		return;
	}
	if (ndis->c.di_buf) {
		seq_printf(s, "\t%s\n", "have di_buf");
		seq_printf(s, "\t\t type[%d] index[%d]\n",
				ndis->c.di_buf->type, ndis->c.di_buf->index);
	} else {
		seq_printf(s, "\t%s\n", "no di_buf");
	}
	if (ndis->c.blk) {
		seq_printf(s, "\t%s\n", "have blk");
		seq_printf(s, "\t\t index[%d]\n",
				ndis->c.blk->header.index);
	} else {
		seq_printf(s, "\t%s\n", "no blk");
	}

	if (ndis->c.vfm.flag & VFRAME_FLAG_HF) {
		dim_dbg_seq_hf(ndis->c.vfm.hf_info, s);
	}

}

void ndis_dbg_print2(struct dim_ndis_s *ndis, char *name)
{
	struct dim_pat_s *pat_buf;//dbg only

	if ((di_dbg & DBG_M_KEEP) == 0)
		return;
	if (!ndis) {
		PR_INF("%s:no ndis\n", __func__);
		return;
	}
	PR_INF("%s:%d\n", name, ndis->header.index);
	if (ndis->c.di_buf) {
		PR_INF("di_buf:t[%d]:%d\n",
			ndis->c.di_buf->type,
			ndis->c.di_buf->index);
	}
	if (ndis->c.blk) {
		PR_INF("blk:%d\n", ndis->c.blk->header.index);
		if (ndis->c.blk->pat_buf) {
			pat_buf = ndis->c.blk->pat_buf;
			PR_INF("pat:%d\n", pat_buf->header.index);
		}
	}
}

void ndis_dbg_qbuf_detail(struct seq_file *s, struct di_ch_s *pch)
{
	unsigned int j;
	union q_buf_u	pbuf;
	//struct qs_buf_s *header;
	struct buf_que_s *pqbuf;
	struct dim_ndis_s *ndis;
	char *splt = "---------------------------";

	pqbuf = &pch->ndis_qb;

	seq_printf(s, "%s:\n", pqbuf->name);
	for (j = 0; j < pqbuf->nub_buf; j++) {
		pbuf = pqbuf->pbuf[j];
		if (!pbuf.qbc)
			break;
		//header = pbuf->qbc;
		dbg_hd(s, pbuf.qbc);
		ndis = (struct dim_ndis_s *)pbuf.qbc;
		ndis_dbg_content(s, ndis);
		seq_printf(s, "%s\n", splt);
	}
}

void ndrd_int(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;

	pq = &pch->ndis_que_ready;

	qfp_int(pq, "np_ready", 0); /* unlock */

	pq = &pch->ndis_que_kback;
	qfp_int(pq, "np_kback", 0); /* unlock */
}

void ndrd_exit(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;

	pq = &pch->ndis_que_ready;
	qfp_release(pq);

	pq = &pch->ndis_que_kback;
	qfp_release(pq);
}

void ndrd_reset(struct di_ch_s *pch)
{
	struct qs_cls_s *pq;

	pq = &pch->ndis_que_ready;
	pq->ops.reset(NULL, pq);
}

/* vfm is vframe or di_buffer */
void ndrd_qin(struct di_ch_s *pch, void *vfm)
{
	struct qs_cls_s	*pq;
	union q_buf_u ubuf;

	if (!vfm)
		return;
	pq = &pch->ndis_que_ready;
	ubuf.qbc = (struct qs_buf_s *)vfm;
	pq->ops.in(NULL, pq, ubuf);
}

/* @ary_note: only one caller: vfm peek */
struct vframe_s *ndrd_qpeekvfm(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;
	bool ret;
	union q_buf_u pbuf;
	struct vframe_s *vfm;

	pq = &pch->ndis_que_ready;

	ret = pq->ops.peek(NULL, pq, &pbuf);
	if (!ret)
		return NULL;
	vfm = (struct vframe_s *)pbuf.qbc;
	return vfm;
}

/* @ary_note: debug ? */
struct di_buf_s *ndrd_qpeekbuf(struct di_ch_s *pch)
{
	struct qs_cls_s *pq;
	bool ret;
	union q_buf_u pbuf;
	struct vframe_s *vfm;
	struct dim_ndis_s *ndis;
	struct di_buf_s *di_buf;
	struct di_buffer *buffer;

	pq = &pch->ndis_que_ready;

	ret = pq->ops.peek(NULL, pq, &pbuf);
	if (!ret)
		return NULL;
	if (dip_itf_is_vfm(pch)) {
		vfm = (struct vframe_s *)pbuf.qbc;
		if (!vfm->private_data)
			return NULL;
		ndis = (struct dim_ndis_s *)vfm->private_data;
		if (!ndis->c.di_buf)
			return NULL;

		di_buf = ndis->c.di_buf;
	} else {
		buffer = (struct di_buffer *)pbuf.qbc;
		ndis = (struct dim_ndis_s *)buffer->private_data;
		if (!ndis || !ndis->c.di_buf)
			return NULL;
		di_buf = ndis->c.di_buf;
	}

	return di_buf;
}

/* */
struct vframe_s *ndrd_qout(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;
	bool ret;
	union q_buf_u pbuf;
	struct vframe_s *vfm;

	pq = &pch->ndis_que_ready;

	ret = pq->ops.peek(NULL, pq, &pbuf);
	if (!ret)
		return NULL;
	ret = pq->ops.out(NULL, pq, &pbuf);

	vfm = (struct vframe_s *)pbuf.qbc;
	return vfm;
}

void dip_itf_ndrd_ins_m2_out(struct di_ch_s *pch)
{
	unsigned int cnt_ready;
	int i;
	struct qs_cls_s *pq;
	union q_buf_u pbuf;
	bool ret;
	struct di_buffer *buffer;
	struct dim_ndis_s *ndis1, *ndis2;

	if (dip_itf_is_vfm(pch))
		return;
	cnt_ready = ndrd_cnt(pch);
	if (!cnt_ready)
		return;

	pq = &pch->ndis_que_ready;
	for (i = 0; i < cnt_ready; i++) {
		ret = pq->ops.peek(NULL, pq, &pbuf);
		if (!ret) {
			//return NULL;
			PR_ERR("%s:%d:%d\n", __func__, cnt_ready, i);
			break;
		}
		ret = pq->ops.out(NULL, pq, &pbuf);
		if (!ret || !pbuf.qbc) {
			//return NULL;
			PR_ERR("%s:out:%d:%d\n", __func__, cnt_ready, i);
			break;
		}
		buffer = (struct di_buffer *)pbuf.qbc;
		ndis1 = (struct dim_ndis_s *)buffer->private_data;
		if (ndis1) {
			ndis2 = ndis_move(pch, QBF_NDIS_Q_USED, QBF_NDIS_Q_DISPLAY);
			if (ndis1 != ndis2)
				PR_ERR("%s:\n", __func__);
		}
		#ifdef MARK_HIS
		if (dip_itf_is_ins_exbuf(pch)) {
			buffer->private_data = NULL;
			ndis1->c.pbuff = NULL;
			task_send_cmd2(pch->ch_id,
			LCMD2(ECMD_RL_KEEP,
			     pch->ch_id,
			     ndis1->header.index));
		}
		#endif
		didbg_vframe_out_save(pch->ch_id, buffer->vf, 1);
		if (buffer->flag & DI_FLAG_EOS)
			dbg_reg("%s:ch[%d]:eos\n", __func__, pch->ch_id);
		pch->itf.u.dinst.parm.ops.fill_output_done(buffer);
		sum_pst_g_inc(pch->ch_id);
	}
}

void dip_itf_ndrd_ins_m1_out(struct di_ch_s *pch)
{
	unsigned int cnt_ready;
	int i;
	struct qs_cls_s *pq;
	bool ret;
	union q_buf_u pbuf;
	struct di_buffer *buffer;

	if (!dip_itf_is_ins_exbuf(pch))
		return;
	cnt_ready = ndrd_cnt(pch);
	if (!cnt_ready)
		return;

	pq = &pch->ndis_que_ready;
	for (i = 0; i < cnt_ready; i++) {
		ret = pq->ops.peek(NULL, pq, &pbuf);
		if (!ret) {
			//return NULL;
			PR_ERR("%s:%d:%d\n", __func__, cnt_ready, i);
			break;
		}
		ret = pq->ops.out(NULL, pq, &pbuf);
		if (!ret || !pbuf.qbc) {
			//return NULL;
			PR_ERR("%s:out:%d:%d\n", __func__, cnt_ready, i);
			break;
		}
		buffer = (struct di_buffer *)pbuf.qbc;
		didbg_vframe_out_save(pch->ch_id, buffer->vf, 2);
		if (buffer->flag & DI_FLAG_EOS)
			PR_INF("%s:ch[%d]:eos\n", __func__, pch->ch_id);
		pch->itf.u.dinst.parm.ops.fill_output_done(buffer);
		sum_pst_g_inc(pch->ch_id);
	}
}

unsigned int ndrd_cnt(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;

	pq = &pch->ndis_que_ready;

	return pq->ops.count(NULL, pq);
}

/* crash ?? */
void ndrd_dbg_list_buf(struct seq_file *s, struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;
	void *list[MAX_FIFO_SIZE];
	unsigned int len;
	int i;
	struct dim_ndis_s *ndis;
	struct di_buf_s *p;

	pq = &pch->ndis_que_ready;

	len = qfp_list(pq, MAX_FIFO_SIZE, &list[0]);
	if (!len) {
		seq_printf(s, "%s:non\n", __func__);
		return;
	}

	for (i = 0; i < len; i++) {
		ndis = (struct dim_ndis_s *)list[i];
		ndis_dbg_content(s, ndis);
		if (ndis->c.di_buf) {
			p = ndis->c.di_buf;
			print_di_buf_seq(p, 2, s);
			print_di_buf_seq(p->di_buf[0], 1, s);
			print_di_buf_seq(p->di_buf[1], 1, s);
		}
	}
}

/****/
/* @ary_note keep back que */

void ndkb_reset(struct di_ch_s *pch)
{
	struct qs_cls_s *pq;

	pq = &pch->ndis_que_kback;
	pq->ops.reset(NULL, pq);
}

void ndkb_qin(struct di_ch_s *pch, struct dim_ndis_s *ndis)
{
	struct qs_cls_s	*pq;
	union q_buf_u ubuf;

	if (!ndis)
		return;
	pq = &pch->ndis_que_kback;
	ubuf.qbc = &ndis->header;
	pq->ops.in(NULL, pq, ubuf);
	PR_INF("%s:%d\n", __func__, ndis->header.index);
}

void ndkb_qin_byidx(struct di_ch_s *pch, unsigned int idx)
{
	struct qs_cls_s	*pq;
	union q_buf_u ubuf;
	struct buf_que_s *pbufq;
//	int i;
	struct dim_ndis_s *ndis;

	pbufq = &pch->ndis_qb;
	if (idx >= pbufq->nub_buf) {
		PR_ERR("%s:overflow %d\n", __func__, idx);
		return;
	}
	ndis = (struct dim_ndis_s *)pbufq->pbuf[idx].qbc;

	pq = &pch->ndis_que_kback;
	ubuf.qbc = &ndis->header;
	pq->ops.in(NULL, pq, ubuf);
	dbg_keep("%s:%d\n", __func__, ndis->header.index);
}

struct dim_ndis_s *ndkb_qout(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;
	bool ret;
	union q_buf_u pbuf;
	struct dim_ndis_s *ndis;

	pq = &pch->ndis_que_kback;

	ret = pq->ops.peek(NULL, pq, &pbuf);
	if (!ret)
		return NULL;
	ret = pq->ops.out(NULL, pq, &pbuf);

	ndis = (struct dim_ndis_s *)pbuf.qbc;
	dbg_keep("%s:%d\n", __func__, ndis->header.index);
	return ndis;
}

unsigned int ndkb_cnt(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;

	pq = &pch->ndis_que_kback;
	return pq->ops.count(NULL, pq);
}

void ndkb_dbg_list(struct seq_file *s, struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;
	void *list[MAX_FIFO_SIZE];
	unsigned int len;
	int i;
	struct dim_ndis_s *ndis;

	pq = &pch->ndis_que_kback;

	len = qfp_list(pq, MAX_FIFO_SIZE, &list[0]);
	if (!len) {
		seq_printf(s, "%s:non\n", __func__);
		return;
	}

	for (i = 0; i < len; i++) {
		ndis = (struct dim_ndis_s *)list[i];
		ndis_dbg_content(s, ndis);
	}
}

void npst_int(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;

	pq = &pch->npst_que;

	qfp_int(pq, "npst", 0); /* unlock */
}

void npst_exit(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;

	pq = &pch->npst_que;
	qfp_release(pq);
}

void npst_reset(struct di_ch_s *pch)
{
	struct qs_cls_s *pq;

	pq = &pch->npst_que;
	pq->ops.reset(NULL, pq);
}

/*  */
void npst_qin(struct di_ch_s *pch, void *buffer)
{
	struct qs_cls_s	*pq;
	union q_buf_u ubuf;
	bool ret;

	if (!buffer) {
		PR_ERR("%s:no buffer\n", __func__);
		return;
	}
	pq = &pch->npst_que;
	ubuf.qbc = (struct qs_buf_s *)buffer;
	ret = pq->ops.in(NULL, pq, ubuf);
	if (!ret)
		PR_ERR("%s:in\n", __func__);
}

/* @ary_note: only one caller: vfm peek */
struct di_buffer *npst_qpeek(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;
	bool ret;
	union q_buf_u pbuf;
	struct di_buffer *buffer;

	pq = &pch->npst_que;

	ret = pq->ops.peek(NULL, pq, &pbuf);
	if (!ret)
		return NULL;
	buffer = (struct di_buffer *)pbuf.qbc;
	return buffer;
}

struct di_buffer *npst_qout(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;
	bool ret;
	union q_buf_u pbuf;
	struct di_buffer *buffer;

	pq = &pch->npst_que;

	ret = pq->ops.peek(NULL, pq, &pbuf);
	if (!ret)
		return NULL;
	ret = pq->ops.out(NULL, pq, &pbuf);

	buffer = (struct di_buffer *)pbuf.qbc;
	return buffer;
}

unsigned int npst_cnt(struct di_ch_s *pch)
{
	struct qs_cls_s	*pq;

	pq = &pch->npst_que;

	return pq->ops.count(NULL, pq);
}

void dbg_log_pst_buffer(struct di_buf_s *di_buf, unsigned int dbgid)
{
//	struct di_buffer *buffer;

	PR_INF("log_pst:%d:di_buf[%d],cvsw[%d]\n",
		dbgid, di_buf->flg_null,
		di_buf->canvas_width[0]);
	if (di_buf->c.buffer)
		PR_INF("log_pst:%d:%px\n", dbgid, di_buf->c.buffer);
	else
		PR_INF("log_pst:%d:no\n", dbgid);
}

static bool di_buf_clear(struct di_ch_s *pch, struct di_buf_s *di_buf)
{
	struct vframe_s *vfm;
	int index;
	int type, queue_index;
	unsigned int canvas_config_flag;
	unsigned int canvas_height;
	unsigned int canvas_width[3];/* nr/mtn/mv */
	unsigned int channel;
	int i;

	/* back */
	vfm = di_buf->vframe;
	index = di_buf->index;
	type = di_buf->type;
	canvas_config_flag = di_buf->canvas_config_flag;
	canvas_height = di_buf->canvas_height;
	channel	= di_buf->channel;
	queue_index = di_buf->queue_index;
//	is_i	= di_buf->buf_is_i;
	for (i = 0; i < 3; i++)
		canvas_width[i] = di_buf->canvas_width[i];

	memset(di_buf, 0, sizeof(*di_buf));
	di_buf->vframe = vfm;
	di_buf->index = index;
	di_buf->type = type;
	di_buf->canvas_config_flag = canvas_config_flag;

	di_buf->canvas_height = canvas_height;
	di_buf->channel	= channel;
	di_buf->queue_index = queue_index;
//	di_buf->buf_is_i = is_i;
	for (i = 0; i < 3; i++)
		di_buf->canvas_width[i] = canvas_width[i];

	return true;
}

static bool ndis_fill_ready_bypass(struct di_ch_s *pch, struct di_buf_s *di_buf)
{
	struct dim_nins_s *nins;
	void *in_ori;
	struct di_buf_s *ibuf;
//	struct dim_nins_s	*nins =NULL;
	struct di_buffer *buffer;

	if (di_buf->is_nbypass) {
		//check:
		if (di_buf->type != VFRAME_TYPE_POST) {
			PR_ERR("%s:bypass not post ?\n", __func__);
			return true;
		}
		ibuf = di_buf->di_buf_dup_p[0];
		if (!ibuf) {
			PR_ERR("%s:bypass no in ?\n", __func__);
			return true;
		}
		nins = (struct dim_nins_s *)ibuf->c.in;
		if (!nins) {
			PR_ERR("%s:no in ori ?\n", __func__);
			return true;
		}
		in_ori = nins->c.ori;

		/* recycle */
		nins->c.ori = NULL;
		nins_used_some_to_recycle(pch, nins);

		ibuf->c.in = NULL;
		//di_buf->queue_index = -1;
		//di_que_in(pch->ch_id, QUE_POST_BACK, di_buf);
		di_buf_clear(pch, di_buf);
		di_que_in(pch->ch_id, QUE_PST_NO_BUF, di_buf);
		queue_in(pch->ch_id, ibuf, QUEUE_RECYCLE);

		/* to ready buffer */
		if (dip_itf_is_ins(pch)) {
			/* need mark bypass flg*/
			buffer = (struct di_buffer *)in_ori;
			buffer->flag |= DI_FLAG_BUF_BY_PASS;
		}

		ndrd_qin(pch, in_ori);
		return true;
	}
	return false;
}

static bool ndis_fill_ready_pst(struct di_ch_s *pch, struct di_buf_s *di_buf)
{
	struct dim_ndis_s *dis;
	struct di_buffer *buffer;

	//dis = ndisq_peek(pch, QBF_NDIS_Q_IDLE);
	dis = ndis_move(pch, QBF_NDIS_Q_IDLE, QBF_NDIS_Q_USED);
	if (!dis) {
		PR_ERR("%s:no idle\n", __func__);
		return false;
	}
	/* clear dis */
	memset(&dis->c, 0, sizeof(dis->c));
	dis->etype = pch->itf.etype;

	atomic_set(&di_buf->di_cnt, 1);
	dis->c.di_buf = di_buf;
	if (dis->etype == EDIM_NIN_TYPE_VFM) {
		memcpy(&dis->c.vfm, di_buf->vframe, sizeof(dis->c.vfm));
		dis->c.vfm.private_data = dis;
		if (di_buf->is_bypass_pst) {
			PR_INF("%s:vfm:%px,t:0x%x\n",
				__func__, &dis->c.vfm, dis->c.vfm.type);
		}
		if (dis->c.vfm.flag & VFRAME_FLAG_HF) { /*hf*/
			memcpy(&dis->c.hf, dis->c.vfm.hf_info, sizeof(dis->c.hf));
			dis->c.vfm.hf_info = &dis->c.hf;
			if (di_dbg & DBG_M_IC)
				dim_print_hf(dis->c.vfm.hf_info);
		}
		/* to ready buffer */
		ndrd_qin(pch, &dis->c.vfm);
	} else {
		/* @ary_note: only for new_interface mode self buff */
		if (dip_itf_is_ins_lbuf(pch)) {
			//PR_INF("%s:lbuf\n", __func__);
			/*int dis's buffer*/
			dis->c.dbuff.mng.ch = pch->ch_id;
			dis->c.dbuff.mng.code = CODE_INS_LBF;
			dis->c.dbuff.mng.index = dis->header.index;
			/* */
			memcpy(&dis->c.vfm, di_buf->vframe, sizeof(dis->c.vfm));
			if (dis->c.vfm.flag & VFRAME_FLAG_HF) { /*hf*/
				memcpy(&dis->c.hf, dis->c.vfm.hf_info, sizeof(dis->c.hf));
				dis->c.vfm.hf_info = &dis->c.hf;
				if (di_dbg & DBG_M_IC)
					dim_print_hf(dis->c.vfm.hf_info);
			}
			dis->c.dbuff.vf = &dis->c.vfm;
			dis->c.dbuff.private_data = dis;
			dis->c.dbuff.caller_data = pch->itf.u.dinst.parm.caller_data;
			if (di_buf->c.src_is_i)
				dis->c.dbuff.flag |= DI_FLAG_I;
			else
				dis->c.dbuff.flag |= DI_FLAG_P;
			dis->c.dbuff.crcout = di_buf->datacrc;
			dis->c.dbuff.nrcrcout = di_buf->nrcrc;
			dbg_post_ref("00%s: %x\n", __func__, di_buf->datacrc);
			dbg_post_ref("01%s: %x\n", __func__, di_buf->nrcrc);
			ndrd_qin(pch, &dis->c.dbuff);
			dim_tr_ops.post_get(dis->c.dbuff.vf->index_disp);
		} else {
			PR_INF("%s:ext buf\n", __func__);
			dis->c.pbuff = di_buf->c.buffer;
			if (!dis->c.pbuff) {
				PR_ERR("%s:no out buffer\n", __func__);
				return false;
			}
			di_buf->c.buffer = NULL;
			buffer = dis->c.pbuff;
			if (!buffer->vf) {
				PR_WARN("%s:no buffer vf\n", __func__);
				return false;
			}
			memcpy(dis->c.pbuff->vf,
			       di_buf->vframe, sizeof(*buffer->vf));
			//memcpy(&dis->c.pbuff->vf,
			//di_buf->vframe, sizeof(*dis->c.pbuff->vf));
			dis->c.pbuff->private_data = dis;
			dis->c.pbuff->caller_data = pch->itf.u.dinst.parm.caller_data;
			dbg_post_ref("02%s: %x\n", __func__, di_buf->datacrc);
			dbg_post_ref("03%s: %x\n", __func__, di_buf->nrcrc);
			dis->c.dbuff.crcout = di_buf->datacrc;
			dis->c.dbuff.nrcrcout = di_buf->nrcrc;
			ndrd_qin(pch, &dis->c.pbuff);
		}
	}
	return true;
}

/* for local post buffer */
bool ndis_fill_ready(struct di_ch_s *pch, struct di_buf_s *di_buf)
{
	bool ret;
	struct dim_itf_s *itf;
	struct dev_vfm_s *pvfmc;

	ret = ndis_fill_ready_bypass(pch, di_buf);

	if (!ret)
		ret = ndis_fill_ready_pst(pch, di_buf);

	itf = &pch->itf;
	if (itf->etype == EDIM_NIN_TYPE_VFM) {
		pvfmc = &itf->u.dvfmc;
		if (pvfmc->vf_m_fill_ready)
			pvfmc->vf_m_fill_ready(pch);
	}

	return true;
}

static bool ndrd_m1_fill_ready_bypass(struct di_ch_s *pch, struct di_buf_s *di_buf)
{
	struct di_buffer *buffer_in, *buffer_o;
	struct di_buf_s *buf_pst;
	struct di_buf_s *ibuf;
	struct dim_nins_s *nins;
	//void *in_ori;
	struct vframe_s *dec_vfm;
	bool is_eos;

	if (!di_buf->is_nbypass)
		return false;
	//check:
	if (di_buf->type != VFRAME_TYPE_POST) {
		PR_ERR("%s:bypass not post ?\n", __func__);
		return true;
	}
	ibuf = di_buf->di_buf_dup_p[0];
	if (!ibuf) {
		PR_ERR("%s:bypass no in ?\n", __func__);
		return true;
	}
	nins = (struct dim_nins_s *)ibuf->c.in;
	if (!nins) {
		PR_ERR("%s:no in ori ?\n", __func__);
		return true;
	}
	/* get out buffer */
	buf_pst = di_que_out_to_di_buf(pch->ch_id, QUE_POST_FREE);
	if (!buf_pst) {
		PR_ERR("%s:no post free\n", __func__);
		return true;
	}

	buffer_o = buf_pst->c.buffer;
	if (!buffer_o) {
		PR_ERR("%s:no buffer_o\n", __func__);
		return true;
	}

	buffer_in = (struct di_buffer *)nins->c.ori;
	buffer_in->flag |= DI_FLAG_BUF_BY_PASS;
	dec_vfm = buffer_in->vf;
	/* recycle nins */
	//no need nins->c.ori = NULL;
	//nins_used_some_to_recycle(pch, nins);

	//ibuf->c.in = NULL;
	queue_in(pch->ch_id, ibuf, QUEUE_RECYCLE);

	is_eos = di_buf->is_eos;
	di_buf_clear(pch, di_buf);
	di_que_in(pch->ch_id, QUE_PST_NO_BUF, di_buf);
	di_buf_clear(pch, buf_pst);
	di_que_in(pch->ch_id, QUE_PST_NO_BUF_WAIT, buf_pst);

	buffer_o->vf->vf_ext = dec_vfm;
	if (!dec_vfm)
		dbg_bypass("%s:no in vfm\n", __func__);
	else
		dim_print("%s:vfm:0x%px, %d\n", __func__, dec_vfm, dec_vfm->index);

	buffer_o->flag |= DI_FLAG_BUF_BY_PASS;
	if (is_eos)
		buffer_o->flag |= DI_FLAG_EOS;

	ndrd_qin(pch, buffer_o);

	return true;
}
/* @ary_note: use exbuf only */

static bool ndrd_m1_fill_ready_pst(struct di_ch_s *pch, struct di_buf_s *di_buf)
{
	struct di_buffer *buffer;
	struct canvas_config_s *cvp_ori, *cvp_di;

	if (!dip_itf_is_ins_exbuf(pch))
		return false;
	if (!di_buf->c.buffer) {
		PR_WARN("%s:no buffer\n", __func__);
		return false;
	}
	buffer = di_buf->c.buffer;
	if (!buffer->vf) {
		PR_WARN("%s:no buffer vf\n", __func__);
		return false;
	}
	di_buf->c.buffer = NULL;
	cvp_di	= &di_buf->vframe->canvas0_config[0];
	cvp_ori	= &buffer->vf->canvas0_config[0];

	if (cvp_di->phy_addr != cvp_ori->phy_addr) {
		#ifdef CVS_UINT
		PR_ERR("%s:0x%x->0x%x\n", __func__,
			cvp_ori->phy_addr, cvp_di->phy_addr);
		#else
		PR_ERR("%s:0x%lx->0x%lx\n", __func__,
			cvp_ori->phy_addr, cvp_di->phy_addr);
		#endif
		return false;
	}
	memcpy(buffer->vf, di_buf->vframe, sizeof(*buffer->vf));
	buffer->caller_data = pch->itf.u.dinst.parm.caller_data;
	if (di_buf->c.src_is_i)
		buffer->flag |= DI_FLAG_I;
	else
		buffer->flag |= DI_FLAG_P;
	buffer->crcout = di_buf->datacrc;
	buffer->nrcrcout = di_buf->nrcrc;
	dbg_post_ref("%s: %x, %x\n", __func__, di_buf->datacrc, di_buf->nrcrc);
	di_buf_clear(pch, di_buf);
	di_que_in(pch->ch_id, QUE_PST_NO_BUF_WAIT, di_buf);
	ndrd_qin(pch, buffer);
	dim_tr_ops.post_get(buffer->vf->index_disp);
	return true;
}

bool ndrd_m1_fill_ready(struct di_ch_s *pch, struct di_buf_s *di_buf)
{
	if (!dip_itf_is_ins_exbuf(pch))
		return false;

	if (ndrd_m1_fill_ready_bypass(pch, di_buf))
		return true;
	ndrd_m1_fill_ready_pst(pch, di_buf);

	return true;
}

/******************************************
 *	new interface
 *****************************************/

static void dip_itf_prob(struct di_ch_s *pch)
{
	struct dim_itf_s *itf;

	if (!pch)
		return;
	itf = &pch->itf;
	mutex_init(&itf->lock_reg);
}

void dip_itf_vf_op_polling(struct di_ch_s *pch)
{
	struct dev_vfm_s *vfmc;

	if (!pch || pch->itf.etype != EDIM_NIN_TYPE_VFM)
		return;

	vfmc = &pch->itf.u.dvfmc;
	vfmc->vf_m_fill_polling(pch);
}

void dip_itf_back_input(struct di_ch_s *pch)
{
	if (!pch ||
	    !pch->itf.opins_m_back_in)
		return;

	pch->itf.opins_m_back_in(pch);
}

bool dip_itf_is_vfm(struct di_ch_s *pch)
{
	return (pch->itf.etype == EDIM_NIN_TYPE_VFM) ? true : false;
}

bool dip_itf_is_ins(struct di_ch_s *pch)
{
	return (pch->itf.etype == EDIM_NIN_TYPE_INS) ? true : false;
}

bool dip_itf_is_ins_lbuf(struct di_ch_s *pch)
{
	bool ret = false;

	if (pch->itf.etype == EDIM_NIN_TYPE_INS &&
	    pch->itf.tmode == EDIM_TMODE_3_PW_LOCAL)
		ret = true;

	return ret;
}

bool dip_itf_is_o_linear(struct di_ch_s *pch)
{
	bool ret = false;

	if (dip_itf_is_ins(pch)) {
		if (pch->itf.u.dinst.parm.output_format & DI_OUTPUT_LINEAR)
			ret = true;
	}

	if (dim_dbg_dec21 & 0x100)
		ret = true;
	return ret;
}

void dbg_itf_tmode(struct di_ch_s *pch, unsigned int pos)
{
	PR_INF("%s:ch[%d]:%d:%d\n", __func__,
		pch->ch_id, pos, pch->itf.tmode);
}

bool dip_itf_is_ins_exbuf(struct di_ch_s *pch)
{
	bool ret = false;

	if (pch->itf.etype == EDIM_NIN_TYPE_INS &&
	    pch->itf.tmode == EDIM_TMODE_2_PW_OUT)
		ret = true;
	return ret;
}

struct dev_vfm_s *dip_itf_vf_sop(struct di_ch_s *pch)
{
	if (pch->itf.etype != EDIM_NIN_TYPE_VFM)
		return NULL;
	return &pch->itf.u.dvfmc;
}

void set_bypass2_complete(unsigned int ch, bool on)
{
	struct di_ch_s *pch;

	pch = get_chdata(ch);

	//set_bypass_complete(pvfm, on);
	if (on)
		pch->itf.bypass_complete = true;
	else
		pch->itf.bypass_complete = false;
}

bool is_bypss2_complete(unsigned int ch)
{
	struct di_ch_s *pch;

	pch = get_chdata(ch);

	return pch->itf.bypass_complete;
}

/******************************************
 *	pq ops
 *****************************************/
void dip_init_pq_ops(void)
{
	unsigned  int ic_id;
	struct di_dev_s *di_devp = get_dim_de_devp();

	ic_id = get_datal()->mdata->ic_id;
	di_attach_ops_pulldown(&get_datal()->ops_pd);
	di_attach_ops_3d(&get_datal()->ops_3d);
	di_attach_ops_nr(&get_datal()->ops_nr);
	di_attach_ops_mtn(&get_datal()->ops_mtn);
	dil_attch_ext_api(&get_datal()->ops_ext);

	dim_attach_to_local();

	/*pd_device_files_add*/
	get_ops_pd()->prob(di_devp->dev);

	get_ops_nr()->nr_drv_init(di_devp->dev);

	di_attach_ops_afd_v3(&get_datal()->afds);
	if (dim_afds()) {
		get_datal()->di_afd.top_cfg_pre =
			&get_datal()->hw_pre.pre_top_cfg;
		get_datal()->di_afd.top_cfg_pst =
			&get_datal()->hw_pst.pst_top_cfg;
		dim_afds()->prob(ic_id,
				 &get_datal()->di_afd);
	} else {
		PR_ERR("%s:no afds\n", __func__);
	}

	/* hw l1 ops*/
	if (IS_IC_EF(ic_id, SC2)) {
		if (IS_IC_EF(ic_id, T7))
			get_datal()->hop_l1 = &dim_ops_l1_v4;
		else
			get_datal()->hop_l1 = &dim_ops_l1_v3;
		di_attach_ops_v3(&get_datal()->hop_l2);
	}
	PR_INF("%s:%d:%s\n", "init ops", ic_id, opl1()->info.name);
	pq_sv_db_ini();
}

bool dip_is_linear(void)
{
	bool ret = false;

	if (cfgg(LINEAR))
		ret = true;

	return ret;
}

unsigned int dim_int_tab(struct device *dev,
				 struct afbce_map_s *pcfg)
{
	bool flg = 0;
	unsigned int *p;
	unsigned int crc = 0;

	if (!pcfg			||
	    !dev)
		return 0;

	dma_sync_single_for_device(dev,
				   pcfg->tabadd,
				   pcfg->size_tab,
				   DMA_TO_DEVICE);

	p = (unsigned int *)dim_vmap(pcfg->tabadd, pcfg->size_tab, &flg);
	if (!p) {
		pr_error("%s:vmap:0x%lx\n", __func__, pcfg->tabadd);
		return 0;
	}

	memset(p, 0, pcfg->size_tab);

	/*debug*/
	#ifdef PRINT_BASIC
	di_pr_info("%s:tab:[0x%lx]: cnt[%d]\n",
		   __func__,
		   pcfg->tabadd, pcfg->size_tab);
	#endif
	dma_sync_single_for_device(dev,
				   pcfg->tabadd,
				   pcfg->size_tab,
				   DMA_TO_DEVICE);

	if (flg)
		dim_unmap_phyaddr((u8 *)p);

	return crc;
}

static bool dim_slt_mode;
module_param_named(dim_slt_mode, dim_slt_mode, bool, 0664);

bool dim_is_slt_mode(void)
{
	return dim_slt_mode;
}

void dim_slt_init(void)
{
	if (!dim_is_slt_mode())
		return;
	dimp_set(edi_mp_debug_blend_mode, 4);
	dimp_set(edi_mp_pstcrc_ctrl, 1);
	dimp_set(edi_mp_pq_load_dbg, 1);
	cfgs(KEEP_CLEAR_AUTO, 2);

	di_decontour_disable(true);
	PR_INF("%s:\n", __func__);
}

/************************************************
 * ic list for support crc cts test
 ***********************************************/

bool dim_config_crc_ic(void)
{
	struct di_dev_s  *de_devp = get_dim_de_devp();

	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	else
		return de_devp->is_crc_ic;
}
EXPORT_SYMBOL(dim_config_crc_ic);
/************************************************
 * aisr lrhf
 ************************************************/
unsigned int di_hf_cnt_size(unsigned int w, unsigned int h, bool is_4k)
{
	unsigned int canvas_align_width = 64;
	unsigned int width, size;
	/**/

	width = roundup(w, canvas_align_width);
	size = width * h;
	size	= PAGE_ALIGN(size);

	dbg_mem2("%s:size:0x%x\n", __func__, size);
	return size;
}

bool di_hf_hw_try_alloc(unsigned int who)
{
	struct di_hpre_s  *pre = get_hw_pre();

	if (pre->hf_busy) {
		dim_print("hf:alloc:%d no\n", who);
		return false;
	}
	pre->hf_busy = true;
	pre->hf_owner = who;
	dim_print("hf:alloc:%d:ok\n", who);
	return true;
}

void di_hf_hw_release(unsigned int who)
{
	struct di_hpre_s  *pre = get_hw_pre();

	dbg_ic("hf:release:%d\n", who);
	if (who == 0xff) {
		pre->hf_busy = 0;
		pre->hf_owner = 0xff;
	} else	if (who == pre->hf_owner) {
		pre->hf_owner = 0xff;
		pre->hf_busy = 0;
	} else {
		PR_WARN("%s:%d->%d\n", __func__, who, pre->hf_owner);
	}
}

bool di_hf_t_try_alloc(struct di_ch_s *pch)
{
	if (get_datal()->hf_busy)
		return false;

	get_datal()->hf_busy = true;
	get_datal()->hf_owner = pch->ch_id;
	PR_INF("hf:a:ch[%d]:alloc:ok\n", pch->ch_id);
	return true;
}

void di_hf_t_release(struct di_ch_s *pch)
{
	if (pch->en_hf) {
		if (pch->ch_id == get_datal()->hf_owner) {
			get_datal()->hf_owner = 0xff;
			get_datal()->hf_busy = false;
		} else {
			PR_ERR("%s:%d->%d\n",
			       __func__, pch->ch_id, get_datal()->hf_owner);
		}
		pch->en_hf = false;
	}

	if (pch->en_hf_buf) {
		get_datal()->hf_src_cnt--;
		pch->en_hf_buf = false;
	}
	PR_INF("hf:r:ch[%d]:cnt[%d]\n",
		pch->ch_id,
		get_datal()->hf_src_cnt);
}

void di_hf_reg(struct di_ch_s *pch)
{
	unsigned char hf_cnt, hf_nub;

	pch->en_hf	= false;
	pch->en_hf_buf	= false;

	hf_nub = cfgg(HF);
	if (!hf_nub || dip_itf_is_ins_exbuf(pch))
		return;

	hf_cnt = get_datal()->hf_src_cnt;
	hf_cnt++;
	if (hf_cnt > hf_nub)
		return;

	pch->en_hf_buf = true;
	get_datal()->hf_src_cnt = hf_cnt;

	if (di_hf_t_try_alloc(pch))
		pch->en_hf = true;
}

void di_hf_polling_active(struct di_ch_s *pch)
{
	if (pch->en_hf_buf && !pch->en_hf) {
		if (di_hf_t_try_alloc(pch))
			pch->en_hf = true;
	}
}

static DEFINE_SPINLOCK(di_hf_lock);

void di_hf_lock_blend_buffer_pre(struct di_buf_s *di_buf)
{
	unsigned long hf_flg;
	struct di_hpre_s  *pre = get_hw_pre();
	bool ret = true;
	u64 timerus;

	spin_lock_irqsave(&di_hf_lock, hf_flg);
	if (pre->hf_owner == 1) {
		di_buf->hf_irq = false;
		pre->hf_w_buf = di_buf;
	} else {
		ret = false;
		timerus = cur_to_usecs();
	}
	spin_unlock_irqrestore(&di_hf_lock, hf_flg);

	if (!ret)
		PR_ERR("%s:%llu\n", __func__, timerus);
}

void di_hf_lock_blend_buffer_pst(struct di_buf_s *di_buf)
{
	unsigned long hf_flg;
	struct di_hpre_s  *pre = get_hw_pre();
	bool ret = true;
	u64 timerus;

	spin_lock_irqsave(&di_hf_lock, hf_flg);
	if (pre->hf_owner == 2) {
		di_buf->hf_irq = false;
		pre->hf_w_buf = di_buf;
	} else {
		ret = false;
		timerus = cur_to_usecs();
	}
	spin_unlock_irqrestore(&di_hf_lock, hf_flg);

	if (!ret)
		PR_ERR("%s:%llu\n", __func__, timerus);
}

void di_hf_lock_irq_flg(void)
{
	unsigned long hf_flg;
	struct di_hpre_s  *pre = get_hw_pre();
	struct di_buf_s *di_buf;
	bool ret = true;
	u64 timerus;

	spin_lock_irqsave(&di_hf_lock, hf_flg);
	if (pre->hf_owner == 2 || pre->hf_owner == 1) {
		di_buf = (struct di_buf_s *)pre->hf_w_buf;
		di_buf->hf_irq = true;
	} else {
		ret = false;
		timerus = cur_to_usecs();
	}
	spin_unlock_irqrestore(&di_hf_lock, hf_flg);

	if (!ret)
		PR_ERR("%s:%llu\n", __func__, timerus);
}

bool di_hf_hw_is_busy(void)
{
	return get_hw_pre()->hf_busy;
}

bool di_hf_set_buffer(struct di_buf_s *di_buf, struct div2_mm_s *mm)
{
	struct hf_info_t *hf;

	if (!di_buf)
		return false;
	//if (!di_buf->hf) {
	//	PR_ERR("%s:t[%d]:d[%d]\n", __func__, di_buf->type, di_buf->index);
	//	return false;
	//}
	hf = &di_buf->hf;
	hf->height = mm->cfg.hf_vsize;
	hf->width = mm->cfg.hf_hsize;
	hf->phy_addr = di_buf->hf_adr;

	return true;
}

bool di_hf_buf_size_set(struct DI_SIM_MIF_s *hf_mif)
{
	unsigned int x_size =  hf_mif->end_x + 1;
	unsigned int y_size = hf_mif->end_y + 1;

	if (x_size <= 960 && y_size <= 540) {
		hf_mif->buf_hsize = 960;
		hf_mif->addr2	= 540;
	} else if (x_size <= 1280 && y_size <= 720) {
		hf_mif->buf_hsize = 1280;
		hf_mif->addr2 = 720;
	} else if (x_size <= 1920 && y_size <= 1080) {
		hf_mif->buf_hsize = 1920;
		hf_mif->addr2 = 1080;
	} else {
		PR_ERR("%s:not support\n", __func__);
		return false;
	}

	return true;
}

bool di_hf_size_check(struct DI_SIM_MIF_s *w_mif)
{
	unsigned int x_size =  w_mif->end_x + 1;
	unsigned int y_size = w_mif->end_y + 1;
	bool ret = false;

	if (!w_mif->is_dw	&&
	    x_size <= 1920	&&
	    y_size <= 1080)
		ret = true;

	return ret;
}

void dim_print_hf(struct hf_info_t *hf)
{
	if (!hf) {
		PR_INF("NULL\n");
		return;
	}
	PR_INF("hf:print %d:%px\n", hf->index, hf);
	PR_INF("\t<%d,%d>\n", hf->width, hf->height);
	PR_INF("\t<%d,%d>\n", hf->buffer_w, hf->buffer_h);
	PR_INF("\t0x%lx\n", hf->phy_addr);
	PR_INF("\t0x%x\n", hf->revert_mode);
}

void dim_dbg_seq_hf(struct hf_info_t *hf, struct seq_file *seq)
{
	if (!hf) {
		seq_puts(seq, "NULL\n");
		return;
	}
	seq_printf(seq, "hf:print %d:%px\n", hf->index, hf);
	seq_printf(seq, "\t<%d,%d>\n", hf->width, hf->height);
	seq_printf(seq, "\t<%d,%d>\n", hf->buffer_w, hf->buffer_h);
	seq_printf(seq, "\t0x%lx\n", hf->phy_addr);
}

/**********************************/
void dim_dvf_cp(struct dvfm_s *dvfm, struct vframe_s *vfm, unsigned int indx)
{
	if (!dvfm || !vfm)
		return;
	dvfm->type = vfm->type;
	dvfm->canvas0Addr = vfm->canvas0Addr;
	dvfm->canvas1Addr = vfm->canvas1Addr;
	dvfm->compHeadAddr	= vfm->compHeadAddr;
	dvfm->compBodyAddr	= vfm->compBodyAddr;
	dvfm->plane_num		= vfm->plane_num;
	memcpy(&dvfm->canvas0_config[0], &vfm->canvas0_config[0],
	      sizeof(vfm->canvas0_config[0]) * 2);

	dvfm->width = vfm->width;
	dvfm->height = vfm->height;
	dvfm->bitdepth = vfm->bitdepth;
	dvfm->decontour_pre = vfm->decontour_pre;
}

/* 0:default; 1: nv21; 2: nv12;*/
void dim_dvf_type_p_chage(struct dvfm_s *dvfm, unsigned int type)
{
	if (!dvfm)
		return;
	/* set vframe bit info */
	dvfm->bitdepth &= ~(BITDEPTH_MASK);
	dvfm->bitdepth &= ~(FULL_PACK_422_MODE);

	if (type == 0) {
		if (dimp_get(edi_mp_di_force_bit_mode) == 10) {
			dvfm->bitdepth |= (BITDEPTH_Y10 |
					   BITDEPTH_U10 |
					   BITDEPTH_V10);
			if (dimp_get(edi_mp_full_422_pack))
				dvfm->bitdepth |= (FULL_PACK_422_MODE);
		} else {
			dvfm->bitdepth |= (BITDEPTH_Y8 |
					   BITDEPTH_U8 |
					   BITDEPTH_V8);
		}
		dvfm->type = VIDTYPE_PROGRESSIVE |
			       VIDTYPE_VIU_422 |
			       VIDTYPE_VIU_SINGLE_PLANE |
			       VIDTYPE_VIU_FIELD;
	} else {
		/* nv 21 */
		if (type == 1)
			dvfm->type = VIDTYPE_VIU_NV21 |
			       VIDTYPE_VIU_FIELD;
		else
			dvfm->type = VIDTYPE_VIU_NV12 |
			       VIDTYPE_VIU_FIELD;
		dvfm->bitdepth |= (BITDEPTH_Y8	|
				  BITDEPTH_U8	|
				  BITDEPTH_V8);
	}
}

void dim_dvf_config_canvas(struct dvfm_s *dvfm)
{
	int i;

	if (dvfm->plane_num > 2) {
		PR_ERR("%s:plan overflow %d\n", __func__, dvfm->plane_num);
		return;
	}

	for (i = 0; i < dvfm->plane_num; i++)
		canvas_config_config(dvfm->cvs_nu[i],
				     &dvfm->canvas0_config[i]);
}

/*****************************************************
 * double write test
 *****************************************************/

static const struct mm_size_in_s cdw_info = {
	.w	= 960,
	.h	= 540,
	.p_as_i	= 0,
	.is_i	= 0,
	.en_afbce	= 0,
	.mode	= EDPST_MODE_NV21_8BIT,
	.out_format = 1 /* yuv 422 10 bit */
};

static const struct SHRK_S cdw_sk = {
	.hsize_in	= 960,
	.vsize_in	= 540,
	.h_shrk_mode	= 1,
	.v_shrk_mode	= 1,
	.shrk_en	= 1,
	.frm_rst	= 0,
};

struct dw_s *dim_getdw(void)
{
	return &get_datal()->dw_d;
}

static int cnt_mm_info_simple_p(struct mm_size_out_s *info)
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
	//nr_width = roundup(nr_width, canvas_align_width);

	if (in->mode == EDPST_MODE_NV21_8BIT) {
		nr_width = roundup(nr_width, canvas_align_width);
		in->o_mode = 1;
		pnv21 = &info->nv21;
		tmpa = (nr_width * canvas_height) >> 1;/*uv*/
		tmpb = roundup(tmpa, PAGE_SIZE);
		tmpa = roundup(nr_width * canvas_height, PAGE_SIZE);

		pnv21->size_buf_uv	= tmpb;
		pnv21->off_y		= tmpa;
		pnv21->size_buf		= tmpa;
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

void dw_fill_outvf(struct vframe_s *vfm,
		   struct di_buf_s *di_buf)
{
	struct canvas_config_s *cvsp;
	struct dw_s *pdw;

	pdw = dim_getdw();

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

void dim_dw_prob(void)
{
	struct dw_s *pdw;

	if (!IS_IC_SUPPORT(DW))
		return;

	pdw = dim_getdw();
	memcpy(&pdw->size_info.info_in, &cdw_info,
	       sizeof(pdw->size_info.info_in));
	cnt_mm_info_simple_p(&pdw->size_info);

	memcpy(&pdw->shrk_cfg, &cdw_sk, sizeof(pdw->shrk_cfg));
	pdw->state_en = true;
}

void dim_dw_reg(struct di_ch_s *pch)
{
	unsigned int dbg_cfg;
	struct dw_s *pdw;

	pch->en_dw_mem = false;
	if (IS_IC_SUPPORT(DW)	&&
	    !pch->cst_no_dw	&&
	    cfggch(pch, DW_EN))
		pch->en_dw = true;
	else
		pch->en_dw = false;

	if (!pch->en_dw)
		return;
	/* cfg dbg */
	dbg_cfg = di_mp_uit_get(edi_mp_shr_cfg);
	pdw = dim_getdw();
	if (dbg_cfg & DI_BIT31) {
		/********************
		 * edi_mp_shr_cfg
		 * b[31]	cfg enable
		 * b[7:0]	mode;
		 *	[7]:	en
		 *	[4:5]:	v
		 *	[0:1]:	h
		 * b[15:8]:	o mode; [15]: en;
		 * b[16]:	show dw;
		 ********************/
		if (dbg_cfg & DI_BIT7) {
			pdw->shrk_cfg.h_shrk_mode = dbg_cfg & 0x03;
			pdw->shrk_cfg.v_shrk_mode = (dbg_cfg & 0x30) >> 4;
			PR_INF("dw dbg:mode:%d,%d\n",
			       pdw->shrk_cfg.h_shrk_mode,
			       pdw->shrk_cfg.v_shrk_mode);
		}
		if (dbg_cfg & DI_BIT15) {
			pdw->size_info.info_in.out_format =
				(dbg_cfg & 0x300) >> 8;
			PR_INF("dw dbg:out:%d\n",
			       pdw->size_info.info_in.out_format);
		}
		pdw->dbg_show_dw = (dbg_cfg & DI_BIT16) ? true : false;
		PR_INF("dw dbg:show:%d\n",
		       pdw->dbg_show_dw);
		pdw->state_cfg_by_dbg = true;
	}

	dbg_reg("%s:[%d]\n", __func__, pch->en_dw);
}

void dim_dw_unreg_setting(void)
{
	if (IS_IC_SUPPORT(DW))
		opl1()->shrk_disable(&di_pre_regset);
}

/* used for signel change */
void dim_dw_pre_para_init(struct di_ch_s *pch, struct dim_nins_s *nins)
{
	struct SHRK_S *shrk_cfg;
	struct dvfm_s *wdvfm;
	struct di_pre_stru_s *ppre;
	struct canvas_config_s *pcvs;
	unsigned int h_mode, v_mode;
	struct di_cvs_s *cvss;

	if (!pch || !nins)
		return;

	cvss = &get_datal()->cvs;
	ppre = get_pre_stru(pch->ch_id);
	shrk_cfg = &ppre->shrk_cfg;
	memcpy(shrk_cfg, &dim_getdw()->shrk_cfg, sizeof(*shrk_cfg));
	wdvfm	= &ppre->dw_wr_dvfm;

	shrk_cfg->shrk_en = 1;
	shrk_cfg->pre_post = 1;
	shrk_cfg->hsize_in = ppre->cur_width;
	shrk_cfg->vsize_in = ppre->cur_height;
	h_mode = dim_getdw()->shrk_cfg.h_shrk_mode + 1;
	v_mode = dim_getdw()->shrk_cfg.v_shrk_mode + 1;
	shrk_cfg->out_h = ppre->cur_width >> h_mode;
	shrk_cfg->out_v = ppre->cur_height >> v_mode;
	//memcpy(wdvfm, nins->c.dvfm, sizeof(*wdvfm));
	wdvfm->width = shrk_cfg->out_h;
	wdvfm->height = shrk_cfg->out_v;

	wdvfm->buf_hsize = dim_getdw()->size_info.info_in.w;

	pcvs = &wdvfm->canvas0_config[0];
	if (dim_getdw()->size_info.info_in.mode
		>= EDPST_MODE_422_10BIT_PACK) {
		wdvfm->plane_num = 1;
		pcvs->width = dim_getdw()->size_info.p.cvs_w;
		pcvs->height = dim_getdw()->size_info.p.cvs_h;
		pcvs->block_mode = 0;
		pcvs->endian = 0;
		wdvfm->cvs_nu[0] = cvss->pre_idx[0][4];
	} else {
		/*nv21 | nv 12 */
		wdvfm->plane_num = 2;
		pcvs->width = dim_getdw()->size_info.nv21.cvs_w;
		pcvs->height = dim_getdw()->size_info.nv21.cvs_h;
		/*temp*/
		pcvs->block_mode = 0;
		pcvs->endian = 0;

		pcvs = &wdvfm->canvas0_config[1];
		pcvs->width = dim_getdw()->size_info.nv21.cvs_w;
		pcvs->height = dim_getdw()->size_info.nv21.cvs_h;
		wdvfm->cvs_nu[0] = cvss->pre_idx[0][4];
		wdvfm->cvs_nu[1] = cvss->post_idx[1][2];
	}
	dim_dvf_type_p_chage(&ppre->dw_wr_dvfm,
			dim_getdw()->size_info.info_in.out_format);
}

/* use wr_di buffer to fill dvmf phy address */
void dw_pre_sync_addr(struct dvfm_s *wdvfm, struct di_buf_s *di_buf)
{
	if (!wdvfm || !di_buf) {
		PR_ERR("%s:is null\n", __func__);
		return;
	}
	wdvfm->canvas0_config[0].phy_addr = di_buf->dw_adr;
	if (wdvfm->plane_num == 1)
		return;
	wdvfm->canvas0_config[1].phy_addr = di_buf->dw_adr +
		dim_getdw()->size_info.nv21.off_y;
}

/**********************************/

void dip_clean_value(void)
{
	unsigned int ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		/*que*/
		di_que_release(ch);
	}
}

bool dip_prob(void)
{
	bool ret = true;
	int i;
	struct di_ch_s *pch;

	ret = dip_init_value();
	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		pch = get_chdata(i);
		bufq_blk_int(pch);
		bufq_mem_int(pch);
		bufq_pat_int(pch);
		bufq_iat_int(pch);
	}

	di_cfgx_init_val();
	di_cfg_top_init_val();
	di_mp_uit_init_val();
	di_mp_uix_init_val();

	dim_tmode_preset(); /*need before vframe*/
	dev_vframe_init();
	didbg_fs_init();
//	dip_wq_prob();
	dip_cma_init_val();
	dip_chst_init();

	dpre_init();
	dpost_init();

	//dip_init_pq_ops();
	/*dim_polic_prob();*/

	return ret;
}

void dip_exit(void)
{
	int i;
	struct di_ch_s *pch;

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		pch = get_chdata(i);
		bufq_blk_exit(pch);
		bufq_mem_exit(pch);
		bufq_pat_exit(pch);
		bufq_iat_exit(pch);
		bufq_sct_exit(pch);

		bufq_nin_exit(pch);
		bufq_ndis_exit(pch);
		ndrd_exit(pch);
		npst_exit(pch);
	}
	dim_release_canvas();
//	dip_wq_ext();
	dev_vframe_exit();
	dip_clean_value();
	didbg_fs_exit();
	#ifdef TST_NEW_INS_INTERFACE
	dtst_exit();
	#endif
}

void dip_prob_ch(void)
{
	unsigned int ch;
	struct di_ch_s *pch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pch = get_chdata(ch);
		bufq_sct_int(pch);
		sct_prob(pch);

		/**/
		bufq_nin_int(pch);
		bufq_ndis_int(pch);
		ndrd_int(pch);
		npst_int(pch);
		dip_itf_prob(pch);
	}

	#ifdef TST_NEW_INS_INTERFACE
	dtst_prob();
	#endif
}

bool dbg_src_change_simple(unsigned int ch/*struct di_ch_s *pch*/)
{
	struct vframe_s *vfm;
	//unsigned int ch;
	static unsigned int last_w;
	static enum EDI_SGN sgn;
	struct di_ch_s *pch;
	unsigned int x, y;

	if (!cfgg(PAUSE_SRC_CHG))
		return false;

	//ch = pch->ch_id;
	pch = get_chdata(ch);
	vfm = nins_peekvfm_pre(pch);//pw_vf_peek(ch);
	if (!vfm)
		return false;

	dim_vf_x_y(vfm, &x, &y);
	if (cfgg(PAUSE_SRC_CHG) == 1) {
		if (last_w != x) {
			PR_INF("%s:%d->%d,0x%x\n", __func__, last_w, x, vfm->type);
			last_w = x;
			pre_run_flag = DI_RUN_FLAG_PAUSE;
		}
	} else if (cfgg(PAUSE_SRC_CHG) == 2) {
	/**/
		sgn = di_vframe_2_sgn(vfm);
		if (last_w != sgn) {
			PR_INF("%s:%d->%d\n", __func__, last_w, sgn);
			last_w = sgn;
			pre_run_flag = DI_RUN_FLAG_PAUSE;
		}
	}

	return true;
}

/**********************************************/
/**********************************************************
 * cvsi_cfg
 *	set canvas_config_config by config | planes | index
 *
 **********************************************************/
void cvsi_cfg(struct dim_cvsi_s	*pcvsi)
{
	int i;
	unsigned char *canvas_index = &pcvsi->cvs_id[0];
	//unsigned int shift;
	struct canvas_config_s *cfg = &pcvsi->cvs_cfg[0];
	u32 planes = pcvsi->plane_nub;

	if (planes > 3) {
		//PR_ERR("%s:planes overflow[%d]\n", __func__, planes);
		return;
	}
	//dim_print("%s:p[%d]\n", __func__, planes);

	for (i = 0; i < planes; i++, canvas_index++, cfg++) {
		canvas_config_config(*canvas_index, cfg);
		dim_print("\tw[%d],h[%d],cid[%d]\n",
			  cfg->width, cfg->height, *canvas_index);
	}
}

#ifdef TEST_PIP

#define src0_fmt          2   //0:444 1:422 2:420
#define src0_bits         2   //0:8bit 1:9bit 2:10bit
#define comb444           0   //need set 1,when hsize>2k, fmt444,8bit,
#define PIP_FMT           2   //
#ifdef ARY_TEST //ary want change background color, but failed
#define DEF_COLOR0        0x0
#define DEF_COLOR1        0x200
#define DEF_COLOR2        0x200
#else
#define DEF_COLOR0        0x3ff
#define DEF_COLOR1        0x200
#define DEF_COLOR2        0x200

#endif
#define PIP_BIT           10  //
//int32_t PIP_BIT_ENC = PIP_BIT==8 ? 0 : PIP_BIT==9 ? 1 : 2;
#define PIP_HSIZE         (2160 / 2) //1/2 rot shrink
#define PIP_VSIZE         (4096 / 2) //1/2 rot shrink

struct AFBCD_S di_afbcd_din_def = {
	.index		= 0,
	.hsize		= 1920,
	.vsize		= 1080,
	.head_baddr	= 0,
	/* index, hsize, vsize, head_baddr, body_baddr*/
	.body_baddr	= 0,
	.compbits	= 0,
	.fmt_mode	= 0,
	.ddr_sz_mode	= 1,
	.fmt444_comb	= comb444,
	/* compbits, fmt_mode, ddr_sz_mode, fmt444_comb, dos_uncomp*/
	.dos_uncomp	= 0,
	.rot_en		= 0,
	.rot_hbgn	= 0,
	.rot_vbgn	= 0,
	.h_skip_en	= 0,
	/* rot_en, rot_hbgn, rot_vbgn, h_skip_en, v_skip_en*/
	.v_skip_en	= 0,
	.rev_mode	= 0,
	/* rev_mode,lossy_en,*/
	.lossy_en	= 0,
	/*def_color_y,def_color_u,def_color_v*/
	.def_color_y	= DEF_COLOR0,
	.def_color_u	= DEF_COLOR1,
	.def_color_v	= DEF_COLOR2,
	.win_bgn_h	= 0,
	.win_end_h	= 1919,
	.win_bgn_v	= 0,
	/* win_bgn_h, win_end_h, win_bgn_v, win_end_v*/
	.win_end_v	= 1079,
	.rot_vshrk	= 0,
	.rot_hshrk	= 0,
	.rot_drop_mode	= 0,
	.rot_ofmt_mode	= PIP_FMT,
	.rot_ocompbit	= 2,//PIP_BIT_ENC,
	.pip_src_mode	= 0,
	.hold_line_num	= 8,
};     // rot_vshrk

struct AFBCE_S di_afbce_out_def = {
	.head_baddr		= 0,
	.mmu_info_baddr		= 0,
	.reg_init_ctrl		= 0,
	.reg_pip_mode		= 0,
	.reg_ram_comb		= 0,
	/* reg_init_ctrl, reg_pip_mode, reg_ram_comb, reg_format_mode */
	.reg_format_mode	= PIP_FMT,
	.reg_compbits_y		= PIP_BIT,
	/* reg_compbits_y, reg_compbits_c */
	.reg_compbits_c		= PIP_BIT,
	.hsize_in		= 1920,
	.vsize_in		= 1080,
	.hsize_bgnd		= PIP_HSIZE,
	/* hsize_in, vsize_in, hsize_bgnd, vsize_bgnd,*/
	.vsize_bgnd		= PIP_VSIZE,
	.enc_win_bgn_h		= 0,
	.enc_win_end_h		= 1919,
	.enc_win_bgn_v		= 0,
	/* enc_win_bgn_h,enc_win_end_h,enc_win_bgn_v,enc_win_end_v,*/
	.enc_win_end_v		= 1079,
	.loosy_mode		= 0,
	/* loosy_mode,rev_mode,*/
	.rev_mode		= 0,
	/* def_color_0,def_color_1,def_color_2,def_color_3 */
	.def_color_0		= DEF_COLOR0,
	.def_color_1		= DEF_COLOR1,
	.def_color_2		= DEF_COLOR2,
	.def_color_3		= 0,
	.force_444_comb		= comb444,
	.rot_en			= 0,
};                    // force_444_comb, rot_en

int dim_post_process_copy_input(struct di_ch_s *pch,
				struct vframe_s *vfm_in,
				struct di_buf_s *di_buf)
{
	struct di_hpst_s  *pst = get_hw_pst();
	unsigned int ch;
	struct mem_cpy_s	*cfg_cpy;
	struct di_post_stru_s *ppost;
	//struct vframe_s		*vfm_in;
	struct dim_wmode_s	wmode;
	struct dim_wmode_s	*pwm;
	//struct di_buf_s		*di_buf;

	//struct dim_fmt_s	*fmt_in;
	//struct dim_fmt_s	*fmt_out;
	//struct dim_cvsi_s	*cvsi_in;
	//struct dim_cvsi_s	*cvsi_out;
	struct AFBCD_S		*in_afbcd;
	struct AFBCE_S		*o_afbce;
	//struct DI_SIM_MIF_s	*o_mif;
	unsigned int h, v;
	//bool err = false;
	//bool flg_in_mif = false, flg_out_mif = false;
	//struct pst_cfg_afbc_s *acfg;
	//struct di_buf_s		*in_buf;
//	struct di_cvs_s *cvss;
	enum DI_MIF0_ID mif_index;
	enum EAFBC_DEC	dec_index = EAFBC_DEC_IF1;
//	unsigned int cvsw, cvsh;
	union hw_sc2_ctr_pre_s cfg;
	bool is_4k = false;

	ch	= pch->ch_id;
	ppost	= &pch->rse_ori.di_post_stru;
	cfg_cpy = &ppost->cfg_cpy;
	pwm	= &wmode;
	pst->cfg_from = DI_SRC_ID_AFBCD_IF1;
	cfg_cpy->afbcd_vf	= NULL;
	//pst->cfg_rot	= 1;

	memset(pwm, 0, sizeof(*pwm));
	pwm->src_h = vfm_in->height;
	pwm->src_w = vfm_in->width;

	if (IS_COMP_MODE(vfm_in->type)) {
		pwm->is_afbc = 1;
		pwm->src_h = vfm_in->compHeight;
		pwm->src_w = vfm_in->compWidth;
	}
	if (pwm->src_w > 1920 || pwm->src_h > 1080)
		is_4k = true;

	if (IS_PROG(vfm_in->type))
		pwm->is_i = 0;
	else
		pwm->is_i = 1;
	/**********************/
	/* afbcd */
	in_afbcd = &ppost->in_afbcd;
	cfg_cpy->in_afbcd = in_afbcd;
	cfg_cpy->in_rdmif = NULL;

	memcpy(in_afbcd, &di_afbcd_din_def, sizeof(*in_afbcd));
	in_afbcd->hsize = pwm->src_w;
	in_afbcd->vsize = pwm->src_h;
	in_afbcd->head_baddr = vfm_in->compHeadAddr >> 4;
	in_afbcd->body_baddr = vfm_in->compBodyAddr >> 4;
	if (pst->cfg_rot) {
		if (pst->cfg_rot == 1) {
			/*180*/
			in_afbcd->rev_mode	= 3;
			in_afbcd->rot_en	= 0;
		} else if (pst->cfg_rot == 2) {
			/*90*/
			in_afbcd->rev_mode	= 2;
			in_afbcd->rot_en	= 1;
		} else if (pst->cfg_rot == 3) {
			/*270*/
			in_afbcd->rev_mode	= 1;
			in_afbcd->rot_en	= 1;
		}
	} else {
		in_afbcd->rot_en	= 0;
		in_afbcd->rev_mode	= 0;
	}
	dbg_copy("%s:rev_mode =%d\n", __func__, in_afbcd->rev_mode);
	//in_afbcd->rot_en	= pst->cfg_rot;
	in_afbcd->dos_uncomp	= 1; //tmp for

	if (vfm_in->type & VIDTYPE_VIU_422)
		in_afbcd->fmt_mode = 1;
	else if (vfm_in->type & VIDTYPE_VIU_444)
		in_afbcd->fmt_mode = 0;
	else
		in_afbcd->fmt_mode = 2;/* 420 ?*/

	if (vfm_in->bitdepth & BITDEPTH_Y10)
		in_afbcd->compbits = 2;	// 10 bit
	else
		in_afbcd->compbits = 0; // 8bit

	if (vfm_in->bitdepth & BITDEPTH_SAVING_MODE) {
		//r |= (1 << 28); /* mem_saving_mode */
		in_afbcd->blk_mem_mode = 1;
	} else {
		in_afbcd->blk_mem_mode = 0;
	}
	if (vfm_in->type & VIDTYPE_SCATTER) {
		//r |= (1 << 29);
		in_afbcd->ddr_sz_mode = 1;
	} else {
		in_afbcd->ddr_sz_mode = 0;
	}

	#ifdef ARY_TEST
	if (IS_420P_SRC(vfm_in->type)) {
		cvfmt_en = 1;
		vt_ini_phase = 0xc;
		cvfm_h = out_height >> 1;
		rpt_pix = 1;
		phase_step = 8;
	} else {
		cvfm_h = out_height;
		rpt_pix = 0;
	}
	#endif
	in_afbcd->win_bgn_h = 0;
	in_afbcd->win_end_h = in_afbcd->win_bgn_h + in_afbcd->hsize - 1;
	in_afbcd->win_bgn_v = 0;
	in_afbcd->win_end_v = in_afbcd->win_bgn_v + in_afbcd->vsize - 1;
	in_afbcd->index = dec_index;

	/**********************/
	/* afbce */
	/*wr afbce */
	//flg_out_mif = false;
	o_afbce = &ppost->out_afbce;
	cfg_cpy->out_afbce = o_afbce;

	//di_buf->di_buf_post = di_que_out_to_di_buf(ch, QUE_POST_FREE);

	memcpy(o_afbce, &di_afbce_out_def, sizeof(*o_afbce));
	o_afbce->reg_init_ctrl = 0;
	o_afbce->head_baddr = di_buf->afbc_adr;
	o_afbce->mmu_info_baddr = di_buf->afbct_adr;
	o_afbce->reg_format_mode = 1; /* 422 tmp */
	//o_afbce->reg_compbits_y = 8;
	//o_afbce->reg_compbits_c = 8; /* 8 bit */
	if (pst->cfg_rot == 2 || pst->cfg_rot == 3) {
		h = pwm->src_h;
		v = pwm->src_w;
		//di_cnt_post_buf_rotation(pch, &cvsw, &cvsh);
		//dim_print("rotation:cvsw[%d],cvsh[%d]\n", cvsw, cvsh);
		//di_buf->canvas_width[0] = cvsw;
		//di_buf->canvas_height	= cvsh;
		o_afbce->rot_en = 1;
	} else {
		h = pwm->src_w;
		v = pwm->src_h;
		o_afbce->rot_en = 0;
	}
//		v = v / 2;
	o_afbce->hsize_in = h;
	o_afbce->vsize_in = v;
	o_afbce->enc_win_bgn_h = 0;
	o_afbce->enc_win_end_h = o_afbce->enc_win_bgn_h + h - 1;
	o_afbce->enc_win_bgn_v = 0;
	o_afbce->enc_win_end_v = o_afbce->enc_win_bgn_v + v - 1;

	//o_afbce->rot_en = pst->cfg_rot;

	cfg_cpy->afbc_en = 1;
	cfg_cpy->out_wrmif = NULL;

	//sync with afbce
	if (o_afbce->rot_en) {
		/*sync with afbce 422*/
		in_afbcd->rot_ofmt_mode = o_afbce->reg_format_mode;
		if (o_afbce->reg_compbits_y == 8)
			in_afbcd->rot_ocompbit	= 0;
		else if (o_afbce->reg_compbits_y == 9)
			in_afbcd->rot_ocompbit	= 1;
		else/* if (o_afbce->reg_compbits_y == 10) */
			in_afbcd->rot_ocompbit	= 2;
	}

	/* set vf */
	memcpy(di_buf->vframe, vfm_in, sizeof(struct vframe_s));
	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->compHeight	= v;
	di_buf->vframe->compWidth	= h;
	di_buf->vframe->height	= v;
	di_buf->vframe->width	= h;
	di_buf->vframe->compHeadAddr = di_buf->afbc_adr;
	di_buf->vframe->compBodyAddr = di_buf->nr_adr;
	#ifdef ARY_TEST
	di_buf->vframe->bitdepth |= (BITDEPTH_U10 | BITDEPTH_V10);
	di_buf->vframe->type |= (VIDTYPE_COMPRESS	|
				 VIDTYPE_SCATTER	|
				 VIDTYPE_VIU_422);
	#else
		di_buf->vframe->bitdepth &= ~(BITDEPTH_MASK);
		di_buf->vframe->bitdepth &= ~(FULL_PACK_422_MODE);
		#ifdef ARY_TEST
		di_buf->vframe->bitdepth |= (BITDEPTH_Y8	|
				  BITDEPTH_U8	|
				  BITDEPTH_V8);
		#else
		di_buf->vframe->bitdepth |= (BITDEPTH_Y10	|
				  BITDEPTH_U10	|
				  BITDEPTH_V10	|
				  FULL_PACK_422_MODE);
		#endif
		/*clear*/
		di_buf->vframe->type &= ~(VIDTYPE_VIU_NV12	|
			       VIDTYPE_VIU_444	|
			       VIDTYPE_VIU_NV21	|
			       VIDTYPE_VIU_422	|
			       VIDTYPE_VIU_SINGLE_PLANE	|
			       VIDTYPE_COMPRESS	|
			       VIDTYPE_PRE_INTERLACE);
		di_buf->vframe->type |= (VIDTYPE_COMPRESS	|
					 VIDTYPE_SCATTER	|
					 VIDTYPE_VIU_422	|
					 VIDTYPE_VIU_SINGLE_PLANE);
	#endif

	#ifdef ARY_TEST
	dimpst_fill_outvf(&pst->vf_post, di_buf, EDPST_OUT_MODE_DEF);

	pst->vf_post.compWidth = h;
	pst->vf_post.compHeight = v;
	pst->vf_post.type |= (VIDTYPE_COMPRESS | VIDTYPE_SCATTER);
	pst->vf_post.compHeadAddr = di_buf->afbc_adr;
	pst->vf_post.compBodyAddr = di_buf->nr_adr;
	pst->vf_post.bitdepth |= (BITDEPTH_U10 | BITDEPTH_V10);
	#endif
	di_buf->vframe->early_process_fun = dim_do_post_wr_fun;
	di_buf->vframe->process_fun = NULL;

	/* 2019-04-22 Suggestions from brian.zhu*/
	di_buf->vframe->mem_handle = NULL;
	di_buf->vframe->type |= VIDTYPE_DI_PW;
	di_buf->in_buf	= NULL;
	memcpy(&pst->vf_post, di_buf->vframe, sizeof(pst->vf_post));

	//dim_print("%s:v[%d]\n", __func__, v);
	dim_print("\tblk[%d]:head[0x%lx], body[0x%lx]\n",
		  di_buf->blk_buf->header.index,
		  di_buf->afbct_adr,
		  di_buf->nr_adr);

	/**********************/

	if (pst->cfg_from == DI_SRC_ID_MIF_IF0 ||
	    pst->cfg_from == DI_SRC_ID_AFBCD_IF0) {
		//in_buf = acfg->buf_mif[0];
		mif_index = DI_MIF0_ID_IF0;
		dec_index = EAFBC_DEC_IF0;
		cfg_cpy->port = 0x81;
	} else if (pst->cfg_from == DI_SRC_ID_MIF_IF1	||
		 pst->cfg_from == DI_SRC_ID_AFBCD_IF1) {
		//in_buf = acfg->buf_mif[1];
		mif_index = DI_MIF0_ID_IF1;
		dec_index = EAFBC_DEC_IF1;
		cfg_cpy->port = 0x92;
	} else {
		//in_buf = acfg->buf_mif[2];
		mif_index = DI_MIF0_ID_IF2;
		dec_index = EAFBC_DEC_IF2;
		cfg_cpy->port = 0xa3;
	}

	cfg_cpy->hold_line = 4;
	cfg_cpy->opin	= &di_pre_regset;

	dbg_copy("is_4k[%d]\n", is_4k);

	if (is_4k)
		dim_sc2_4k_set(2);
	else
		dim_sc2_4k_set(0);

	cfg.d32 = 0;
	dim_sc2_contr_pre(&cfg);
	if (cfg_cpy->in_afbcd)
		dim_print("1:afbcd:0x%px\n", cfg_cpy->in_afbcd);
	if (cfg_cpy->in_rdmif)
		dim_print("2:in_rdmif:0x%px\n", cfg_cpy->in_rdmif);
	if (cfg_cpy->out_afbce)
		dim_print("3:out_afbce:0x%px\n", cfg_cpy->out_afbce);
	if (cfg_cpy->out_wrmif)
		dim_print("4:out_wrmif:0x%px\n", cfg_cpy->out_wrmif);

	//dbg_sc2_4k_set(1);
	opl2()->memcpy_rot(cfg_cpy);
	//dbg_sc2_4k_set(2);
	pst->pst_tst_use	= 1;
	pst->flg_int_done	= false;

	return 0;
}

/*
 * input use vframe set
 * out use pst cfg
 */
int dim_post_process_copy_only(struct di_ch_s *pch,
			       struct vframe_s *vfm_in,
			       struct di_buf_s *di_buf)
{
	struct di_hpst_s  *pst = get_hw_pst();
	unsigned int ch;
	struct mem_cpy_s	*cfg_cpy;
	struct di_post_stru_s *ppost;
	struct dim_wmode_s	wmode;
	struct dim_wmode_s	*pwm;
	struct dim_cvsi_s	*cvsi_out;
	struct AFBCD_S		*in_afbcd	= NULL;
	struct AFBCE_S		*o_afbce	= NULL;
	struct di_buf_s		*in_buf		= NULL;
	struct DI_MIF_S		*i_mif		= NULL;
	struct DI_SIM_MIF_s	*o_mif;
	unsigned int h, v;
	//bool err = false;
	//bool flg_in_mif = false, flg_out_mif = false;
	//struct pst_cfg_afbc_s *acfg;
	//struct di_buf_s		*in_buf;
	struct di_cvs_s *cvss;
	enum DI_MIF0_ID mif_index;
	enum EAFBC_DEC	dec_index = EAFBC_DEC_IF1;
//	unsigned int cvsw, cvsh;
	union hw_sc2_ctr_pre_s cfg;
	bool is_4k = false;
//	unsigned int tmp_mask;
	ch	= pch->ch_id;
	ppost	= &pch->rse_ori.di_post_stru;
	cfg_cpy = &ppost->cfg_cpy;
	pwm	= &wmode;
	pst->cfg_from = DI_SRC_ID_AFBCD_IF1;
	cfg_cpy->afbcd_vf	= NULL;
	cvsi_out = &ppost->cvsi_out;
	cvss	= &get_datal()->cvs;

	memset(pwm, 0, sizeof(*pwm));
	pwm->src_h = vfm_in->height;
	pwm->src_w = vfm_in->width;

	if (IS_COMP_MODE(vfm_in->type)) {
		pwm->is_afbc = 1;
		pwm->src_h = vfm_in->compHeight;
		pwm->src_w = vfm_in->compWidth;
	}
	if (pwm->src_w > 1920 || pwm->src_h > 1920)
		is_4k = true;

	if (IS_PROG(vfm_in->type))
		pwm->is_i = 0;
	else
		pwm->is_i = 1;
	/**********************/
	if (pwm->is_afbc) {
		/* afbcd */
		in_afbcd = &ppost->in_afbcd;
		cfg_cpy->in_afbcd = in_afbcd;
		cfg_cpy->in_rdmif = NULL;

		memcpy(in_afbcd, &di_afbcd_din_def, sizeof(*in_afbcd));
		in_afbcd->hsize = pwm->src_w;
		in_afbcd->vsize = pwm->src_h;
		in_afbcd->head_baddr = vfm_in->compHeadAddr >> 4;
		in_afbcd->body_baddr = vfm_in->compBodyAddr >> 4;
		if (pst->cfg_rot) {
			if (pst->cfg_rot == 1) {
				/*180*/
				in_afbcd->rev_mode	= 3;
				in_afbcd->rot_en	= 0;
			} else if (pst->cfg_rot == 2) {
				/*90*/
				in_afbcd->rev_mode	= 2;
				in_afbcd->rot_en	= 1;
			} else if (pst->cfg_rot == 3) {
				/*270*/
				in_afbcd->rev_mode	= 1;
				in_afbcd->rot_en	= 1;
			}
		} else {
			in_afbcd->rot_en	= 0;
			in_afbcd->rev_mode	= 0;
		}
		dbg_copy("%s:rev_mode =%d\n", __func__, in_afbcd->rev_mode);
		//in_afbcd->rot_en	= pst->cfg_rot;
		in_afbcd->dos_uncomp	= 1; //tmp for

		if (vfm_in->type & VIDTYPE_VIU_422)
			in_afbcd->fmt_mode = 1;
		else if (vfm_in->type & VIDTYPE_VIU_444)
			in_afbcd->fmt_mode = 0;
		else
			in_afbcd->fmt_mode = 2;/* 420 ?*/

		if (vfm_in->bitdepth & BITDEPTH_Y10)
			in_afbcd->compbits = 2;	// 10 bit
		else
			in_afbcd->compbits = 0; // 8bit

		if (vfm_in->bitdepth & BITDEPTH_SAVING_MODE) {
			//r |= (1 << 28); /* mem_saving_mode */
			in_afbcd->blk_mem_mode = 1;
		} else {
			in_afbcd->blk_mem_mode = 0;
		}
		if (vfm_in->type & VIDTYPE_SCATTER) {
			//r |= (1 << 29);
			in_afbcd->ddr_sz_mode = 1;
		} else {
			in_afbcd->ddr_sz_mode = 0;
		}

		in_afbcd->win_bgn_h = 0;
		in_afbcd->win_end_h = in_afbcd->win_bgn_h + in_afbcd->hsize - 1;
		in_afbcd->win_bgn_v = 0;
		in_afbcd->win_end_v = in_afbcd->win_bgn_v + in_afbcd->vsize - 1;
		in_afbcd->index = dec_index;
	} else {
		i_mif = &ppost->di_buf1_mif;
		cfg_cpy->in_afbcd = NULL;
		cfg_cpy->in_rdmif = i_mif;

		in_buf = &ppost->in_buf;
		memset(in_buf, 0, sizeof(*in_buf));

		in_buf->vframe = &ppost->in_buf_vf;
		memcpy(in_buf->vframe, vfm_in, sizeof(struct vframe_s));
		in_buf->vframe->private_data = in_buf;

		pre_cfg_cvs(in_buf->vframe);
		config_di_mif_v3(i_mif, DI_MIF0_ID_IF1, in_buf, ch);
		i_mif->mif_index = DI_MIF0_ID_IF1;
	}

	/* set vf */
	memcpy(di_buf->vframe, vfm_in, sizeof(struct vframe_s));
	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->compHeight	= pwm->src_h;
	di_buf->vframe->compWidth	= pwm->src_w;
	di_buf->vframe->height	= pwm->src_h;
	di_buf->vframe->width	= pwm->src_w;
	di_buf->vframe->bitdepth &= ~(BITDEPTH_MASK);
	di_buf->vframe->bitdepth &= ~(FULL_PACK_422_MODE);
	/*clear*/
	di_buf->vframe->type &= ~(VIDTYPE_VIU_NV12	|
		       VIDTYPE_VIU_444	|
		       VIDTYPE_VIU_NV21 |
		       VIDTYPE_VIU_422	|
		       VIDTYPE_VIU_SINGLE_PLANE |
		       VIDTYPE_COMPRESS |
		       VIDTYPE_PRE_INTERLACE);

	//dbg_copy("di:typ[0x%x]:\n", di_buf->vframe->type);
	if (pst->cfg_out_enc)
		di_buf->vframe->type |= (VIDTYPE_COMPRESS	|
					 VIDTYPE_SCATTER	|
					 VIDTYPE_VIU_SINGLE_PLANE);
	if (pst->cfg_out_fmt == 0) {
		di_buf->vframe->type |= (VIDTYPE_VIU_444 |
					 VIDTYPE_VIU_SINGLE_PLANE);
	} else if (pst->cfg_out_fmt == 1) {
		di_buf->vframe->type |= (VIDTYPE_VIU_422 |
					 VIDTYPE_VIU_SINGLE_PLANE);
	} else {
		if (!pst->cfg_out_enc)
			di_buf->vframe->type |= VIDTYPE_VIU_NV21;
		else
			di_buf->vframe->type |= VIDTYPE_VIU_SINGLE_PLANE;
	}
	//dbg_copy("di:typ[0x%x]:\n", di_buf->vframe->type);
	if (pst->cfg_out_bit == 0)
		di_buf->vframe->bitdepth |= (BITDEPTH_Y8	|
				  BITDEPTH_U8	|
				  BITDEPTH_V8);
	else if (pst->cfg_out_bit == 2)
		di_buf->vframe->bitdepth |= (BITDEPTH_Y10	|
				  BITDEPTH_U10	|
				  BITDEPTH_V10	|
				  FULL_PACK_422_MODE);

	if (pst->cfg_out_enc) {
		/**********************/
		/* afbce */
		/*wr afbce */
		//flg_out_mif = false;
		o_afbce = &ppost->out_afbce;
		cfg_cpy->out_afbce = o_afbce;
		cfg_cpy->afbc_en = 1;
		cfg_cpy->out_wrmif = NULL;

		memcpy(o_afbce, &di_afbce_out_def, sizeof(*o_afbce));
		o_afbce->reg_init_ctrl = 0;
		o_afbce->head_baddr = di_buf->afbc_adr;
		o_afbce->mmu_info_baddr = di_buf->afbct_adr;
		o_afbce->reg_format_mode = 1; /* 422 tmp */
		//o_afbce->reg_compbits_y = 8;
		//o_afbce->reg_compbits_c = 8; /* 8 bit */
		if (pst->cfg_rot == 2 || pst->cfg_rot == 3) {
			h = pwm->src_h;
			v = pwm->src_w;
			//di_cnt_post_buf_rotation(pch, &cvsw, &cvsh);
			//dim_print("rotation:cvsw[%d],cvsh[%d]\n", cvsw, cvsh);
			//di_buf->canvas_width[0] = cvsw;
			//di_buf->canvas_height = cvsh;
			o_afbce->rot_en = 1;
		} else {
			h = pwm->src_w;
			v = pwm->src_h;
			o_afbce->rot_en = 0;
		}
	//		v = v / 2;
		o_afbce->hsize_in = h;
		o_afbce->vsize_in = v;
		o_afbce->enc_win_bgn_h = 0;
		o_afbce->enc_win_end_h = o_afbce->enc_win_bgn_h + h - 1;
		o_afbce->enc_win_bgn_v = 0;
		o_afbce->enc_win_end_v = o_afbce->enc_win_bgn_v + v - 1;

		//o_afbce->rot_en = pst->cfg_rot;

		//sync with afbce
		if (o_afbce->rot_en && cfg_cpy->in_afbcd) {
			/*sync with afbce 422*/
			in_afbcd->rot_ofmt_mode = o_afbce->reg_format_mode;
			if (o_afbce->reg_compbits_y == 8)
				in_afbcd->rot_ocompbit	= 0;
			else if (o_afbce->reg_compbits_y == 9)
				in_afbcd->rot_ocompbit	= 1;
			else/* if (o_afbce->reg_compbits_y == 10)*/
				in_afbcd->rot_ocompbit	= 2;
		}
		di_buf->vframe->compHeight	= v;
		di_buf->vframe->compWidth	= h;
		di_buf->vframe->height	= v;
		di_buf->vframe->width	= h;
		di_buf->vframe->compHeadAddr = di_buf->afbc_adr;
		di_buf->vframe->compBodyAddr = di_buf->nr_adr;

		//dim_print("%s:v[%d]\n", __func__, v);
		dim_print("\tblk[%d]:head[0x%lx], body[0x%lx]\n",
			  di_buf->blk_buf->header.index,
			  di_buf->afbct_adr,
			  di_buf->nr_adr);
	} else {
		o_mif = &ppost->di_diwr_mif;
		cfg_cpy->out_wrmif = o_mif;
		cfg_cpy->out_afbce = NULL;
		cvsi_out->cvs_id[0] = (unsigned char)cvss->post_idx[1][0];
		cvsi_out->plane_nub = 1;
		cvsi_out->cvs_cfg[0].phy_addr = di_buf->nr_adr;
		cvsi_out->cvs_cfg[0].width	= di_buf->canvas_width[0];
		cvsi_out->cvs_cfg[0].height	= di_buf->canvas_height;
		cvsi_out->cvs_cfg[0].block_mode	= 0;

		cvsi_out->cvs_cfg[0].endian	= 0;

		o_mif->canvas_num	= (unsigned short)cvsi_out->cvs_id[0];
		memcpy(&di_buf->vframe->canvas0_config[0],
		       &cvsi_out->cvs_cfg[0],
		       sizeof(di_buf->vframe->canvas0_config[0]));
		di_buf->vframe->plane_num = cvsi_out->plane_nub;
		di_buf->vframe->canvas0Addr = (u32)o_mif->canvas_num;

		/* set cvs */
		cvsi_cfg(cvsi_out);

		opl1()->wr_cfg_mif(o_mif, EDI_MIFSM_WR, di_buf->vframe, NULL);
	}

	di_buf->vframe->early_process_fun = dim_do_post_wr_fun;
	di_buf->vframe->process_fun = NULL;

	/* 2019-04-22 Suggestions from brian.zhu*/
	di_buf->vframe->mem_handle = NULL;
	di_buf->vframe->type |= VIDTYPE_DI_PW;
	di_buf->in_buf	= NULL;
	memcpy(&pst->vf_post, di_buf->vframe, sizeof(pst->vf_post));

	/**********************/

	if (pst->cfg_from == DI_SRC_ID_MIF_IF0 ||
	    pst->cfg_from == DI_SRC_ID_AFBCD_IF0) {
		//in_buf = acfg->buf_mif[0];
		mif_index = DI_MIF0_ID_IF0;
		dec_index = EAFBC_DEC_IF0;
		cfg_cpy->port = 0x81;
	} else if (pst->cfg_from == DI_SRC_ID_MIF_IF1	||
		 pst->cfg_from == DI_SRC_ID_AFBCD_IF1) {
		//in_buf = acfg->buf_mif[1];
		mif_index = DI_MIF0_ID_IF1;
		dec_index = EAFBC_DEC_IF1;
		cfg_cpy->port = 0x92;
	} else {
		//in_buf = acfg->buf_mif[2];
		mif_index = DI_MIF0_ID_IF2;
		dec_index = EAFBC_DEC_IF2;
		cfg_cpy->port = 0xa3;
	}

	cfg_cpy->hold_line = 4;
	cfg_cpy->opin	= &di_pre_regset;

	dbg_copy("is_4k[%d]\n", is_4k);

	if (is_4k)
		dim_sc2_4k_set(2);
	else
		dim_sc2_4k_set(0);

	cfg.d32 = 0;
	dim_sc2_contr_pre(&cfg);
	if (cfg_cpy->in_afbcd)
		dim_print("1:afbcd:0x%px\n", cfg_cpy->in_afbcd);
	if (cfg_cpy->in_rdmif)
		dim_print("2:in_rdmif:0x%px\n", cfg_cpy->in_rdmif);
	if (cfg_cpy->out_afbce)
		dim_print("3:out_afbce:0x%px\n", cfg_cpy->out_afbce);
	if (cfg_cpy->out_wrmif)
		dim_print("4:out_wrmif:0x%px\n", cfg_cpy->out_wrmif);

	opl2()->memcpy(cfg_cpy);

	pst->pst_tst_use	= 1;
	pst->flg_int_done	= false;

	return 0;
}

void dim_post_copy_update(struct di_ch_s *pch,
			  struct vframe_s *vfm_in,
			  struct di_buf_s *di_buf)
{
	struct di_hpst_s  *pst = get_hw_pst();
//	const struct reg_acc *op = di_pre_regset;

	memcpy(di_buf->vframe, &pst->vf_post, sizeof(struct vframe_s));
	di_buf->vframe->private_data = di_buf;

	di_buf->vframe->compHeadAddr = di_buf->afbc_adr;
	di_buf->vframe->compBodyAddr = di_buf->nr_adr;
	//di_buf->vframe->early_process_fun = dim_do_post_wr_fun;
	//di_buf->vframe->process_fun = NULL;

	/* 2019-04-22 Suggestions from brian.zhu*/
	di_buf->vframe->mem_handle = NULL;
	di_buf->vframe->type |= VIDTYPE_DI_PW;
	di_buf->in_buf	= NULL;

	dbg_afbc_update_level1(vfm_in, EAFBC_DEC_IF1);
	dbg_afbce_update_level1(di_buf->vframe, &di_pre_regset, EAFBC_ENC1);
	pst->pst_tst_use	= 1;
	pst->flg_int_done	= false;
	opl1()->pst_set_flow(1, EDI_POST_FLOW_STEP4_CP_START);
}

void dbg_cp_4k(struct di_ch_s *pch, unsigned int mode)
{
	struct vframe_s	*vfm_in = NULL;
	struct di_buf_s	*di_buf;
	unsigned int ch;
	struct di_hpst_s  *pst = get_hw_pst();
	unsigned int time_cnt = 0;
	u64 t_st, t_c;
	unsigned int us_diff[5], diffa;
	int i;
	unsigned int flg_mode, mode_rotation, mode_copy;
	char *mname = "nothing";
	struct dim_nins_s *nins;

//	struct di_hpst_s  *pst = get_hw_pst();
	if (mode & 0x0f) {
		flg_mode = 1; /*rotation*/
		mode_rotation	= mode & 0xf;
		mode_copy	= 0;

	} else if (mode & 0xf0) {
		flg_mode = 2; /*mem copy only */
		mode_rotation	= 0;
		mode_copy	= mode & 0xf0;
	} else {
		mode_copy	= 0;
		mode_rotation	= 0;
		//PR_ERR("mode[%d] is overflow?\n", mode);
		return;
	}

	ch	= pch->ch_id;
	/**********************/
	pst->cfg_rot = (mode_rotation & (DI_BIT1 | DI_BIT2)) >> 1;
	/**********************/
	/* vfm */
	vfm_in = nins_peekvfm(pch);//pw_vf_peek(ch);
	if (!vfm_in) {
		dbg_copy("no input");
		return;
	}
	dbg_copy("vf:%px\n", vfm_in);
	/**********************/
	if (di_que_is_empty(ch, QUE_POST_FREE))
		return;
	/**********************/
	t_st = cur_to_usecs();
	//vfm_in	= pw_vf_get(ch);
	nins = nins_get(pch);
	vfm_in = &nins->c.vfm_cp;
	di_buf	= di_que_out_to_di_buf(ch, QUE_POST_FREE);
	t_c = cur_to_usecs();
	us_diff[0] = (unsigned int)(t_c - t_st);
	/**********************/
	/* rotation */
	if (mode_rotation) {
		mname = "rotation";
		if (mode_rotation & DI_BIT0)
			dim_post_process_copy_input(pch, vfm_in, di_buf);
		else
			dim_post_copy_update(pch, vfm_in, di_buf);
	} else if (mode_copy == 0x10) {
		mname = "copy:2 mif";
		pst->cfg_out_bit = 2;
		pst->cfg_out_enc = 0;
		pst->cfg_out_fmt = 1;
		dim_post_process_copy_only(pch, vfm_in, di_buf);
	} else if (mode_copy == 0x20) {
		mname = "copy:2 afbc";
		pst->cfg_out_bit = 2;
		pst->cfg_out_enc = 1;
		pst->cfg_out_fmt = 1;
		dim_post_process_copy_only(pch, vfm_in, di_buf);
	}

	t_c = cur_to_usecs();
	us_diff[1] = (unsigned int)(t_c - t_st);

	/* wait finish */
	while (time_cnt < 50) {
		if (pst->flg_int_done)
			break;
		//msleep(3);
		usleep_range(2000, 2001);
		time_cnt++;
	}
	if (!pst->flg_int_done) {
		PR_ERR("%s:copy failed\n", __func__);
		pw_vf_put(nins->c.ori, ch);
		nins_move(pch, QBF_NINS_Q_USED, QBF_NINS_Q_IDLE);
		return;
	}
	t_c = cur_to_usecs();
	us_diff[2] = (unsigned int)(t_c - t_st);

	//di_que_in(ch, QUE_POST_READY, di_buf);
	pch->itf.op_fill_ready(pch, di_buf);
	dbg_copy("di:typ2[0x%x]:\n", di_buf->vframe->type);
	pw_vf_put(nins->c.ori, ch);
	nins_move(pch, QBF_NINS_Q_USED, QBF_NINS_Q_IDLE);

	dbg_copy("%s:timer:%d:%px:%s\n", __func__, time_cnt, vfm_in, mname);
	diffa = 0;
	for (i = 0; i < 5; i++) {
		diffa = us_diff[i] - diffa;
		dbg_copy("\t:[%d]:%d:%d\n", i, us_diff[i], diffa);
		diffa = us_diff[i];
	}
}

#define DIM_PIP_WIN_NUB (16)
static const struct di_win_s win_pip[DIM_PIP_WIN_NUB] = {
	[0] = {
	.x_st = 0,
	.y_st = 0,
	},
	[1] = {
	.x_st = 1920,
	.y_st = 0,
	},
	[2] = {
	.x_st = 960,
	.y_st = 540,
	},
	[3] = {
	.x_st = 1920,
	.y_st = 1080,
	},
};

static const struct di_win_s win_pip_16[DIM_PIP_WIN_NUB] = { /*960 x 540 */
	[0] = {
	.x_st = 0,
	.y_st = 0,
	},
	[1] = {
	.x_st = 960,
	.y_st = 0,
	},
	[2] = {
	.x_st = 1920,
	.y_st = 0,
	},
	[3] = {
	.x_st = 2880,
	.y_st = 0,
	},
	[4] = { /*line 1*/
	.x_st = 0,
	.y_st = 540,
	},
	[5] = {
	.x_st = 960,
	.y_st = 540,
	},
	[6] = {
	.x_st = 1920,
	.y_st = 540,
	},
	[7] = {
	.x_st = 2880,
	.y_st = 540,
	},
	[8] = { /*line 2*/
	.x_st = 0,
	.y_st = 1080,
	},
	[9] = {
	.x_st = 960,
	.y_st = 1080,
	},
	[10] = {
	.x_st = 1920,
	.y_st = 1080,
	},
	[11] = {
	.x_st = 2880,
	.y_st = 1080,
	},
	[12] = { /*line 3*/
	.x_st = 0,
	.y_st = 1620,
	},
	[13] = {
	.x_st = 960,
	.y_st = 1620,
	},
	[14] = {
	.x_st = 1920,
	.y_st = 1620,
	},
	[15] = {
	.x_st = 2880,
	.y_st = 1620,
	},
};

int dim_post_copy_pip(struct di_ch_s *pch,
		      struct vframe_s *vfm_in,
		      struct di_buf_s *di_buf,
		      unsigned int winmode)
{
	struct di_hpst_s  *pst = get_hw_pst();
	unsigned int ch;
	struct mem_cpy_s	*cfg_cpy;
	struct di_post_stru_s *ppost;
	struct dim_wmode_s	wmode;
	struct dim_wmode_s	*pwm;
	struct dim_cvsi_s	*cvsi_out;
	struct AFBCD_S		*in_afbcd	= NULL;
	struct AFBCE_S		*o_afbce	= NULL;
	struct di_buf_s		*in_buf		= NULL;
	struct DI_MIF_S		*i_mif		= NULL;
	struct DI_SIM_MIF_s	*o_mif;
	unsigned int h, v;
	struct di_cvs_s *cvss;
	enum DI_MIF0_ID mif_index;
	enum EAFBC_DEC	dec_index = EAFBC_DEC_IF1;
	union hw_sc2_ctr_pre_s cfg;
	bool is_4k = false;

	struct di_win_s	win[DIM_PIP_WIN_NUB];
	struct di_win_s *pwin;
	unsigned int pip_nub = pst->cfg_pip_nub + 1;
	int i;
	unsigned int time_cnt = 0;
	u64 t_st, t_c;
	unsigned int us_diff[DIM_PIP_WIN_NUB + 2], diffa;
	unsigned int bgrh, bgrv;
	static unsigned int pstiong_shift, shif_cnt;

	ch	= pch->ch_id;
	ppost	= &pch->rse_ori.di_post_stru;
	cfg_cpy = &ppost->cfg_cpy;
	pwm	= &wmode;
	pst->cfg_from = DI_SRC_ID_AFBCD_IF1;
	cfg_cpy->afbcd_vf	= NULL;
	cvsi_out = &ppost->cvsi_out;
	cvss	= &get_datal()->cvs;
	dbg_copy("%s:pip_nub[%d]\n", __func__, pip_nub);
	memset(pwm, 0, sizeof(*pwm));
	pwm->src_h = vfm_in->height;
	pwm->src_w = vfm_in->width;

	if (winmode & DI_BIT3) {
		bgrh = 1920;
		bgrv = 1080;
	} else {
		bgrh = 3840;
		bgrv = 2160;
	}

	if (IS_PROG(vfm_in->type))
		pwm->is_i = 0;
	else
		pwm->is_i = 1;

	if (IS_COMP_MODE(vfm_in->type)) {
		pwm->is_afbc = 1;
		pwm->src_h = vfm_in->compHeight;
		pwm->src_w = vfm_in->compWidth;
	}

	if (pwm->is_i)
		pwm->src_h = pwm->src_h >> 1;
	if (pwm->src_w > 1920 || pwm->src_h > 1920)
		is_4k = true;

	/**********************/
	if (pwm->is_afbc) {
		/* afbcd */
		in_afbcd = &ppost->in_afbcd;
		cfg_cpy->in_afbcd = in_afbcd;
		cfg_cpy->in_rdmif = NULL;

		memcpy(in_afbcd, &di_afbcd_din_def, sizeof(*in_afbcd));
		in_afbcd->hsize = pwm->src_w;
		in_afbcd->vsize = pwm->src_h;
		in_afbcd->head_baddr = vfm_in->compHeadAddr >> 4;
		in_afbcd->body_baddr = vfm_in->compBodyAddr >> 4;
		if (pst->cfg_rot) {
			if (pst->cfg_rot == 1) {
				/*180*/
				in_afbcd->rev_mode	= 3;
				in_afbcd->rot_en	= 0;
			} else if (pst->cfg_rot == 2) {
				/*90*/
				in_afbcd->rev_mode	= 2;
				in_afbcd->rot_en	= 1;
			} else if (pst->cfg_rot == 3) {
				/*270*/
				in_afbcd->rev_mode	= 1;
				in_afbcd->rot_en	= 1;
			}
		} else {
			in_afbcd->rot_en	= 0;
			in_afbcd->rev_mode	= 0;
		}
		dbg_copy("%s:rev_mode =%d\n", __func__, in_afbcd->rev_mode);
		//in_afbcd->rot_en	= pst->cfg_rot;
		in_afbcd->dos_uncomp	= 1; //tmp for

		if (vfm_in->type & VIDTYPE_VIU_422)
			in_afbcd->fmt_mode = 1;
		else if (vfm_in->type & VIDTYPE_VIU_444)
			in_afbcd->fmt_mode = 0;
		else
			in_afbcd->fmt_mode = 2;/* 420 ?*/

		if (vfm_in->bitdepth & BITDEPTH_Y10)
			in_afbcd->compbits = 2;	// 10 bit
		else
			in_afbcd->compbits = 0; // 8bit

		if (vfm_in->bitdepth & BITDEPTH_SAVING_MODE) {
			//r |= (1 << 28); /* mem_saving_mode */
			in_afbcd->blk_mem_mode = 1;
		} else {
			in_afbcd->blk_mem_mode = 0;
		}
		if (vfm_in->type & VIDTYPE_SCATTER) {
			//r |= (1 << 29);
			in_afbcd->ddr_sz_mode = 1;
		} else {
			in_afbcd->ddr_sz_mode = 0;
		}

		in_afbcd->win_bgn_h = 0;
		in_afbcd->win_end_h = in_afbcd->win_bgn_h + in_afbcd->hsize - 1;
		in_afbcd->win_bgn_v = 0;
		in_afbcd->win_end_v = in_afbcd->win_bgn_v + in_afbcd->vsize - 1;
		in_afbcd->index = dec_index;
	} else {
		i_mif = &ppost->di_buf1_mif;
		cfg_cpy->in_afbcd = NULL;
		cfg_cpy->in_rdmif = i_mif;

		in_buf = &ppost->in_buf;
		memset(in_buf, 0, sizeof(*in_buf));

		in_buf->vframe = &ppost->in_buf_vf;
		memcpy(in_buf->vframe, vfm_in, sizeof(struct vframe_s));
		in_buf->vframe->private_data = in_buf;
		i_mif->dbg_from_dec = 1; //data from decoder, tmp
		pre_cfg_cvs(in_buf->vframe);
		pre_inp_mif_w(i_mif, in_buf->vframe);
		//config_di_mif_v3_test_pip(i_mif, DI_MIF0_ID_IF1, in_buf, ch);
		opl1()->pre_cfg_mif(i_mif, DI_MIF0_ID_IF1, in_buf, ch);
		i_mif->mif_index = DI_MIF0_ID_IF1;
		i_mif->burst_size_y	= 3;
		i_mif->burst_size_cb	= 1;
		i_mif->burst_size_cr	= 1;
		i_mif->hold_line	= 0x0a;
		if (di_dbg & DBG_M_COPY)
			dim_dump_mif_state(i_mif, "if1");
	}

	/* set vf */
	memcpy(di_buf->vframe, vfm_in, sizeof(struct vframe_s));
	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->compHeight	= bgrv;	//pwm->src_h;
	di_buf->vframe->compWidth	= bgrh;	//pwm->src_w;
	di_buf->vframe->height	= bgrv;	//pwm->src_h;
	di_buf->vframe->width	= bgrh;	//pwm->src_w;
	di_buf->vframe->bitdepth &= ~(BITDEPTH_MASK);
	di_buf->vframe->bitdepth &= ~(FULL_PACK_422_MODE);
	/*clear*/
	di_buf->vframe->type &= ~(VIDTYPE_VIU_NV12	|
		       VIDTYPE_VIU_444	|
		       VIDTYPE_VIU_NV21 |
		       VIDTYPE_VIU_422	|
		       VIDTYPE_VIU_SINGLE_PLANE |
		       VIDTYPE_COMPRESS |
		       VIDTYPE_PRE_INTERLACE);

	//dbg_copy("di:typ[0x%x]:\n", di_buf->vframe->type);
	if (pst->cfg_out_enc)
		di_buf->vframe->type |= (VIDTYPE_COMPRESS	|
					 VIDTYPE_SCATTER	|
					 VIDTYPE_VIU_SINGLE_PLANE);
	if (pst->cfg_out_fmt == 0) {
		di_buf->vframe->type |= (VIDTYPE_VIU_444 |
					 VIDTYPE_VIU_SINGLE_PLANE);
	} else if (pst->cfg_out_fmt == 1) {
		di_buf->vframe->type |= (VIDTYPE_VIU_422 |
					 VIDTYPE_VIU_SINGLE_PLANE);
	} else {
		if (!pst->cfg_out_enc)
			di_buf->vframe->type |= VIDTYPE_VIU_NV21;
		else
			di_buf->vframe->type |= VIDTYPE_VIU_SINGLE_PLANE;
	}
	//dbg_copy("di:typ[0x%x]:\n", di_buf->vframe->type);
	if (pst->cfg_out_bit == 0)
		di_buf->vframe->bitdepth |= (BITDEPTH_Y8	|
				  BITDEPTH_U8	|
				  BITDEPTH_V8);
	else if (pst->cfg_out_bit == 2)
		di_buf->vframe->bitdepth |= (BITDEPTH_Y10	|
				  BITDEPTH_U10	|
				  BITDEPTH_V10	|
				  FULL_PACK_422_MODE);

	if (pst->cfg_out_enc) {
		/**********************/
		/* afbce */
		/*wr afbce */
		//flg_out_mif = false;
		o_afbce = &ppost->out_afbce;
		cfg_cpy->out_afbce = o_afbce;
		cfg_cpy->afbc_en = 1;
		cfg_cpy->out_wrmif = NULL;

		memcpy(o_afbce, &di_afbce_out_def, sizeof(*o_afbce));
		o_afbce->reg_init_ctrl = 0;
		o_afbce->head_baddr = di_buf->afbc_adr;
		o_afbce->mmu_info_baddr = di_buf->afbct_adr;
		o_afbce->reg_format_mode = 1; /* 422 tmp */
		//o_afbce->reg_compbits_y = 8;
		//o_afbce->reg_compbits_c = 8; /* 8 bit */
		if (pst->cfg_rot == 2 || pst->cfg_rot == 3) {
			h = pwm->src_h;
			v = pwm->src_w;
			//di_cnt_post_buf_rotation(pch, &cvsw, &cvsh);
			//dim_print("rotation:cvsw[%d],cvsh[%d]\n", cvsw, cvsh);
			//di_buf->canvas_width[0] = cvsw;
			//di_buf->canvas_height = cvsh;
			o_afbce->rot_en = 1;
		} else {
			h = pwm->src_w;
			v = pwm->src_h;
			o_afbce->rot_en = 0;
		}
	//		v = v / 2;
		o_afbce->hsize_in = h;
		o_afbce->vsize_in = v;
		o_afbce->enc_win_bgn_h = 0;
		o_afbce->enc_win_end_h = o_afbce->enc_win_bgn_h + h - 1;
		o_afbce->enc_win_bgn_v = 0;
		o_afbce->enc_win_end_v = o_afbce->enc_win_bgn_v + v - 1;

		//o_afbce->rot_en = pst->cfg_rot;
		//sync with afbce
		if (o_afbce->rot_en && cfg_cpy->in_afbcd) {
			/*sync with afbce 422*/
			in_afbcd->rot_ofmt_mode = o_afbce->reg_format_mode;
			if (o_afbce->reg_compbits_y == 8)
				in_afbcd->rot_ocompbit	= 0;
			else if (o_afbce->reg_compbits_y == 9)
				in_afbcd->rot_ocompbit	= 1;
			else/* if (o_afbce->reg_compbits_y == 10) */
				in_afbcd->rot_ocompbit	= 2;
		}

		//di_buf->vframe->compHeight	= v;
		//di_buf->vframe->compWidth	= h;
		//di_buf->vframe->height	= v;
		//di_buf->vframe->width	= h;
		di_buf->vframe->compHeadAddr = di_buf->afbc_adr;
		di_buf->vframe->compBodyAddr = di_buf->nr_adr;

		//dim_print("%s:v[%d]\n", __func__, v);
		dim_print("\tblk[%d]:head[0x%lx], body[0x%lx]\n",
			  di_buf->blk_buf->header.index,
			  di_buf->afbct_adr,
			  di_buf->nr_adr);
	} else {
		o_mif = &ppost->di_diwr_mif;
		cfg_cpy->out_wrmif = o_mif;
		cfg_cpy->out_afbce = NULL;
		cvsi_out->cvs_id[0] = (unsigned char)cvss->post_idx[1][0];
		cvsi_out->plane_nub = 1;
		cvsi_out->cvs_cfg[0].phy_addr = di_buf->nr_adr;
		cvsi_out->cvs_cfg[0].width	= di_buf->canvas_width[0];
		cvsi_out->cvs_cfg[0].height	= di_buf->canvas_height;
		cvsi_out->cvs_cfg[0].block_mode	= 0;

		cvsi_out->cvs_cfg[0].endian	= 0;

		o_mif->canvas_num	= (unsigned short)cvsi_out->cvs_id[0];
		memcpy(&di_buf->vframe->canvas0_config[0],
		       &cvsi_out->cvs_cfg[0],
		       sizeof(di_buf->vframe->canvas0_config[0]));
		di_buf->vframe->plane_num = cvsi_out->plane_nub;
		di_buf->vframe->canvas0Addr = (u32)o_mif->canvas_num;

		/* set cvs */
		cvsi_cfg(cvsi_out);

		opl1()->wr_cfg_mif(o_mif, EDI_MIFSM_WR, di_buf->vframe, NULL);
	}

	di_buf->vframe->early_process_fun = dim_do_post_wr_fun;
	di_buf->vframe->process_fun = NULL;

	/* 2019-04-22 Suggestions from brian.zhu*/
	di_buf->vframe->mem_handle = NULL;
	di_buf->vframe->type |= VIDTYPE_DI_PW;
	di_buf->in_buf	= NULL;
	memcpy(&pst->vf_post, di_buf->vframe, sizeof(pst->vf_post));

	/**********************/

	if (pst->cfg_from == DI_SRC_ID_MIF_IF0 ||
	    pst->cfg_from == DI_SRC_ID_AFBCD_IF0) {
		//in_buf = acfg->buf_mif[0];
		mif_index = DI_MIF0_ID_IF0;
		dec_index = EAFBC_DEC_IF0;
		cfg_cpy->port = 0x81;
	} else if (pst->cfg_from == DI_SRC_ID_MIF_IF1	||
		 pst->cfg_from == DI_SRC_ID_AFBCD_IF1) {
		//in_buf = acfg->buf_mif[1];
		mif_index = DI_MIF0_ID_IF1;
		dec_index = EAFBC_DEC_IF1;
		cfg_cpy->port = 0x92;
	} else {
		//in_buf = acfg->buf_mif[2];
		mif_index = DI_MIF0_ID_IF2;
		dec_index = EAFBC_DEC_IF2;
		cfg_cpy->port = 0xa3;
	}

	cfg_cpy->hold_line = 4;
	cfg_cpy->opin	= &di_pre_regset;

	dbg_copy("is_4k[%d]\n", is_4k);
	if (is_4k)
		dim_sc2_4k_set(2);
	else
		dim_sc2_4k_set(0);
	cfg.d32 = 0;
	dim_sc2_contr_pre(&cfg);
	if (cfg_cpy->in_afbcd)
		dim_print("1:afbcd:0x%px\n", cfg_cpy->in_afbcd);
	if (cfg_cpy->in_rdmif)
		dim_print("2:in_rdmif:0x%px\n", cfg_cpy->in_rdmif);
	if (cfg_cpy->out_afbce)
		dim_print("3:out_afbce:0x%px\n", cfg_cpy->out_afbce);
	if (cfg_cpy->out_wrmif)
		dim_print("4:out_wrmif:0x%px\n", cfg_cpy->out_wrmif);

	/*************************************/
	t_st = cur_to_usecs();
	o_afbce->hsize_bgnd = bgrh;
	o_afbce->vsize_bgnd = bgrv;
	di_buf->vframe->compWidth	= bgrh;
	di_buf->vframe->compHeight	= bgrv;
	dbg_copy("%s:pip_nub[%d]\n", __func__, pip_nub);
	for (i = 0; i < pip_nub; i++) {
		time_cnt = 0;
		if (winmode & DI_BIT1)
			memcpy(&win[i], &win_pip_16[i], sizeof(win[i]));
		else
			memcpy(&win[i], &win_pip[i], sizeof(win[i]));
		win[i].x_size = pwm->src_w;
		win[i].y_size = pwm->src_h;
		if ((winmode & DI_BIT0) && i == 1) {
			if ((pstiong_shift % 30) == 0) {
				shif_cnt = pstiong_shift / 30;

				if ((win[i].x_st - (shif_cnt * 4)) <
				    win_pip[3].x_st)
					win[i].x_st -= shif_cnt * 4;
				else
					pstiong_shift = 0;
				if ((win[i].y_st + (shif_cnt * 4)) <
				    win_pip[3].y_st)
					win[i].y_st += shif_cnt * 4;
				else
					pstiong_shift = 0;
			} else {
				win[i].x_st -= shif_cnt * 4;
				win[i].y_st += shif_cnt * 4;
			}
			pstiong_shift++;
			PR_INF("x_st[%d], y_st[%d], %d\n",
			       win[i].x_st, win[i].y_st, pstiong_shift);
		}
		pwin  = &win[i];
		if (o_afbce) {
			o_afbce->enc_win_bgn_h = pwin->x_st;
			o_afbce->enc_win_end_h = o_afbce->enc_win_bgn_h + h - 1;
			o_afbce->enc_win_bgn_v = pwin->y_st;
			o_afbce->enc_win_end_v = o_afbce->enc_win_bgn_v + v - 1;
			if (i == 0)
				o_afbce->reg_init_ctrl = 1;
			else
				o_afbce->reg_init_ctrl = 0;
			o_afbce->reg_pip_mode = 1;
		}
		dbg_copy("%s:%d <%d:%d:%d:%d>\n", __func__, i,
			 o_afbce->enc_win_bgn_h,
			 o_afbce->enc_win_end_h,
			 o_afbce->enc_win_bgn_v,
			 o_afbce->enc_win_end_v);
		/********************/
		opl2()->memcpy(cfg_cpy);

		pst->pst_tst_use	= 1;
		pst->flg_int_done	= false;
		/* wait finish */
		while (time_cnt < 50) {
			if (pst->flg_int_done)
				break;
			//msleep(3);
			usleep_range(1000, 1001);
			time_cnt++;
		}
		if (!pst->flg_int_done) {
			PR_ERR("%s:copy failed\n", __func__);
			pw_vf_put(vfm_in, ch);
			break;
		}
		t_c = cur_to_usecs();
		us_diff[i] = (unsigned int)(t_c - t_st);
	}
	diffa = 0;
	for (i = 0; i < DIM_PIP_WIN_NUB; i++) {
		diffa = us_diff[i] - diffa;
		dbg_copy("\t:[%d]:%d:%d\n", i, us_diff[i], diffa);
		diffa = us_diff[i];
	}

	return 0;
}

/************************************************
 * mode:
 *	[16]: still or not;;
 *	[3:0]: nub
 *	[7:4]:
 *		[4]:pip mode pic 1 move;
 *		[5]:select pip window;
 *		[7]:select background size (4k or 1080)
 ************************************************/
void dbg_pip_func(struct di_ch_s *pch, unsigned int mode)
{
	struct vframe_s		*vfm_in = NULL;
	struct di_buf_s		*di_buf;
	unsigned int ch;
	struct di_hpst_s  *pst = get_hw_pst();
	struct dim_nins_s *nins;

	ch	= pch->ch_id;
	/**********************/
	pst->cfg_rot = 0;
	pst->cfg_out_bit = 2;
	pst->cfg_out_enc = 1;
	pst->cfg_out_fmt = 1;
	/**********************/
	/* vfm */
	vfm_in = nins_peekvfm(pch);//pw_vf_peek(ch);
	if (!vfm_in) {
		dbg_copy("no input");
		return;
	}
	//dbg_copy("%s:vf:%px\n", __func__, vfm_in);
	/**********************/
	if (di_que_is_empty(ch, QUE_POST_FREE))
		return;
	/**********************/
	//vfm_in	= pw_vf_get(ch);
	nins = nins_get(pch);
	vfm_in = &nins->c.vfm_cp;
	di_buf	= di_que_out_to_di_buf(ch, QUE_POST_FREE);

	/**********************/
	pst->cfg_pip_nub = (mode & 0xf);
	dim_post_copy_pip(pch, vfm_in, di_buf, (mode & 0xf0) >> 4);

	//di_que_in(ch, QUE_POST_READY, di_buf);
	pch->itf.op_fill_ready(pch, di_buf);
	dbg_copy("di:typ2[0x%x]:\n", di_buf->vframe->type);
	//pw_vf_put(vfm_in, ch);
	pw_vf_put(nins->c.ori, ch);
	nins_move(pch, QBF_NINS_Q_USED, QBF_NINS_Q_IDLE);
}
#else

void dbg_cp_4k(struct di_ch_s *pch, unsigned int mode)
{
}

void dbg_pip_func(struct di_ch_s *pch, unsigned int mode)
{
}
#endif

static unsigned int dbg_trig_eos;
module_param_named(dbg_trig_eos, dbg_trig_eos, uint, 0664);

bool dbg_is_trig_eos(unsigned int ch)
{
	bool ret = false;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);

	if ((dbg_trig_eos & DI_BIT0) && ppre->field_count_for_cont >= 1) {
		ret = true;
		dbg_trig_eos &= ~DI_BIT0;
		PR_INF("%s:%d\n", __func__, dbg_trig_eos);
	}

	return ret;
}

void dbg_q_listid(struct seq_file *s, struct buf_que_s *pqbuf)
{
	unsigned int j, i;
	struct qs_cls_s *p;
	unsigned int psize;
	char *splt = "---------------------------";

	seq_printf(s, "%s:\n", pqbuf->name);
	for (j = 0; j < pqbuf->nub_que; j++) {
		p = pqbuf->pque[j];
		p->ops.list(pqbuf, p, &psize);

		seq_printf(s, "%s (crr %d):\n", p->name, psize);
		for (i = 0; i < psize; i++)
			seq_printf(s, "\t%2d,\n", pqbuf->list_id[i]);

		seq_printf(s, "%s\n", splt);
	}
}

void dbg_q_listid_print(struct buf_que_s *pqbuf)
{
	unsigned int j, i;
	struct qs_cls_s *p;
	unsigned int psize;
	char *splt = "---------------------------";

	PR_INF("%s:\n", pqbuf->name);
	for (j = 0; j < pqbuf->nub_que; j++) {
		p = pqbuf->pque[j];
		p->ops.list(pqbuf, p, &psize);

		PR_INF("%s (crr %d):\n", p->name, psize);
		for (i = 0; i < psize; i++)
			PR_INF("\t%2d,\n", pqbuf->list_id[i]);

		PR_INF("%s\n", splt);
	}
}

void dbg_hd(struct seq_file *s, struct qs_buf_s *header)
{
	seq_printf(s, "%s\n", "header");
	if (!header) {
		seq_puts(s, "\tnone\n");
		return;
	}
	seq_printf(s, "\t%s:%d\n", "index", header->index);
	seq_printf(s, "\t%s:0x%x\n", "code", header->code);
	//seq_printf(s, "\t%s:%d:%s\n", "dbg_id", header->dbg_id,
	//	   dbgid_name(header->dbg_id));
}

void dbg_hd_print(struct qs_buf_s *header)
{
	PR_INF("%s\n", "header");
	if (!header) {
		PR_INF("\tnone\n");
		return;
	}
	PR_INF("\t%s:%d\n", "index", header->index);
	PR_INF("\t%s:0x%x\n", "code", header->code);
	//PR_INF("\t%s:%d:%s\n", "dbg_id", header->dbg_id,
	//	   dbgid_name(header->dbg_id));
	PR_INF("%px\n", header);
}

void dbg_blk(struct seq_file *s, struct dim_mm_blk_s *blk_buf)
{
	seq_printf(s, "%s\n", "blk");
	if (!blk_buf) {
		seq_puts(s, "\tnone\n");
		return;
	}
	dbg_hd(s, &blk_buf->header);

	seq_printf(s, "\t%s:0x%lx\n", "mem_start", blk_buf->mem_start);
	seq_printf(s, "\t%s:0x%x\n", "size_page", blk_buf->flg.b.page);
	seq_printf(s, "\t%s:0x%x\n", "size_page", blk_buf->flg.b.typ);
	seq_printf(s, "\t%s:%d\n", "flg_alloc", blk_buf->flg_alloc);
	seq_printf(s, "\t%s:%d\n", "tvp", blk_buf->flg.b.tvp);
	seq_printf(s, "\t%s:%d\n", "p_ref_mem",
		   atomic_read(&blk_buf->p_ref_mem));
}

/* @ary_note: */
void dbg_q_list_qbuf(struct seq_file *s, struct buf_que_s *pqbuf)
{
	unsigned int j;
	union q_buf_u	pbuf;
	//struct qs_buf_s *header;

	char *splt = "---------------------------";

	seq_printf(s, "%s:\n", pqbuf->name);
	for (j = 0; j < pqbuf->nub_buf; j++) {
		pbuf = pqbuf->pbuf[j];
		if (!pbuf.qbc)
			break;
		//header = pbuf->qbc;
		dbg_hd(s, pbuf.qbc);

		seq_printf(s, "%s\n", splt);
	}
}

void dbg_q_list_qbuf_print(struct buf_que_s *pqbuf)
{
	unsigned int j;
	union q_buf_u pbuf;
	//struct qs_buf_s *header;

	char *splt = "---------------------------";

	PR_INF("%s:\n", pqbuf->name);
	for (j = 0; j < pqbuf->nub_buf; j++) {
		pbuf = pqbuf->pbuf[j];
		if (!pbuf.qbc)
			break;
		//header = pbuf->qbc;
		dbg_hd_print(pbuf.qbc);

		PR_INF("%s\n", splt);
	}
}

void dbg_buffer(struct seq_file *s, void *in)
{
	struct di_buffer *buffer;

	buffer = (struct di_buffer *)in;
	seq_printf(s, "%s:%px\n", "dbg_buffer", buffer);
	seq_printf(s, "\t:code:0x%x,ch[%d],indx[%d], type[%d]\n",
		buffer->mng.code, buffer->mng.ch,
		buffer->mng.index, buffer->mng.type);
	if (buffer->vf)
		seq_printf(s, "\t:vf:0x%px, 0x%x\n", buffer->vf, buffer->vf->index);
	else
		seq_printf(s, "\t:%s\n", "no vf");
}

void dbg_buffer_print(void *in)
{
	struct di_buffer *buffer;

	buffer = (struct di_buffer *)in;
	PR_INF("%s:%px\n", "dbg_buffer", buffer);
	PR_INF("\t:code:0x%x,ch[%d],indx[%d], type[%d]\n",
		buffer->mng.code,
		buffer->mng.ch, buffer->mng.index, buffer->mng.type);
	if (buffer->vf)
		PR_INF("\t:vf:0x%px, 0x%x\n", buffer->vf, buffer->vf->index);
	else
		PR_INF("\t:%s\n", "no vf");
}

void dbg_vfm_w(struct vframe_s *vfm, unsigned int dbgid)
{
	dbg_ic("%s:%d:\n", __func__, dbgid);
	if (!vfm) {
		dbg_ic("\t:no vmf\n");
		return;
	}
	dbg_ic("\t0x%px:id[%d];t[0x%x]\n", vfm, vfm->index_disp, vfm->type);
	dbg_ic("\t xy<%d,%d><%d, %d>\n", vfm->width, vfm->height, vfm->compWidth, vfm->compHeight);
	dbg_ic("\t ph:<%d %d>\n", vfm->canvas0_config[0].width, vfm->canvas0_config[0].height);
}

