// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/deinterlace/nr_drv.c
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
#include <linux/amlogic/iomap.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include "register.h"
#include "register_nr4.h"
#include "nr_drv.h"
#include "deinterlace.h"
#include "deinterlace_dbg.h"
#include "di_pqa.h"
#include <linux/amlogic/media/di/di.h>

static DNR_PRM_t dnr_param;
static struct NR_PARM_s nr_param;
static bool dnr_pr;
module_param(dnr_pr, bool, 0644);
MODULE_PARM_DESC(dnr_pr, "/n print dnr debug information /n");

static bool dnr_dm_en;
module_param(dnr_dm_en, bool, 0644);
MODULE_PARM_DESC(dnr_dm_en, "/n dnr dm enable debug /n");

static bool dnr_en = true;
module_param_named(dnr_en, dnr_en, bool, 0644);

static unsigned int nr2_en = 0x1;
module_param_named(nr2_en, nr2_en, uint, 0644);

static bool dynamic_dm_chk = true;
module_param_named(dynamic_dm_chk, dynamic_dm_chk, bool, 0644);

static unsigned int autonr_en = 0x1;
module_param_named(autonr_en, autonr_en, uint, 0644);

static bool nr4ne_en;
module_param_named(nr4ne_en, nr4ne_en, bool, 0644);

static bool nr_ctrl_reg;
module_param_named(nr_ctrl_reg, nr_ctrl_reg, bool, 0644);

bool nr_demo_flag;

static void nr_gate_control(bool gate);
static void nr_gate_control_op(bool gate, const struct reg_acc *op);

int global_bs_calc_sw(int *pGbsVldCnt,
			  int *pGbsVldFlg,
			  int *pGbs,
			  int nGbsStatLR,
			  int nGbsStatLL,
			  int nGbsStatRR,
			  int nGbsStatDif,
			  int nGbsStatCnt,
			  int prm_gbs_vldcntthd, /* prm below */
			  int prm_gbs_cnt_min,
			  int prm_gbs_ratcalcmod,
			  int prm_gbs_ratthd[3],
			  int prm_gbs_difthd[3],
			  int prm_gbs_bsdifthd,
			  int prm_gbs_calcmod)
{
	int nMax, nMin;
	int nDif0, nDif1, nDif2;
	int nDif, nRat;
	int nCurGbs;

	nMax = max(max(nGbsStatLR, nGbsStatLL), nGbsStatRR);
	nMin = min(min(nGbsStatLR, nGbsStatLL), nGbsStatRR);

	nDif0 = nMax == 0 ? 0 : ((nMax - nMin) << 9)/nMax;
	nDif0 = min(511, nDif0);

	nDif1 = nGbsStatLR == 0 ? 0 :
	(abs(nGbsStatLR - (nGbsStatLL + nGbsStatRR)/2) << 9)/nGbsStatLR;
	nDif1 = min(511, nDif1);

	nDif2 = nGbsStatLR == 0 ? 0 :
	(abs(nGbsStatLR - max(nGbsStatLL, nGbsStatRR)) << 9)/nGbsStatLR;
	nDif2 = min(511, nDif2);

	if (prm_gbs_ratcalcmod == 0)
		nRat = (nGbsStatLR << 4) / max(prm_gbs_cnt_min, nGbsStatCnt);
	else
		nRat = (nGbsStatDif << 4) / max(prm_gbs_cnt_min, nGbsStatCnt);

	nDif = (prm_gbs_calcmod == 0) ? nDif0 :
(prm_gbs_calcmod == 1 ? nDif1 : nDif2);

	if (nGbsStatLR < max(nGbsStatLL, nGbsStatRR)) {
		if (nGbsStatCnt <= prm_gbs_cnt_min || nRat <= prm_gbs_ratthd[0])
			nCurGbs = 0;
		else if (nRat <= prm_gbs_ratthd[1])
			nCurGbs = 1;
		else if (nRat <= prm_gbs_ratthd[2])
			nCurGbs = 2;
		else
			nCurGbs = 3;
	} else {
		if (nGbsStatCnt <= prm_gbs_cnt_min || nDif <= prm_gbs_difthd[0])
			nCurGbs = 0;
		else if (nDif <= prm_gbs_difthd[1])
			nCurGbs = 1;
		else if (nDif <= prm_gbs_difthd[2])
			nCurGbs = 2;
		else
			nCurGbs = 3;
	}

	/*	*/
	if ((nCurGbs != 0 && 0 == *pGbs) ||
(nCurGbs != 0 && abs(nCurGbs - *pGbs) <= prm_gbs_bsdifthd))
		(*pGbsVldCnt)++;
	else
		*pGbsVldCnt = 0;

	if (*pGbsVldCnt >= prm_gbs_vldcntthd)
		*pGbsVldFlg = 1;
	else
		*pGbsVldFlg = 0;
	if (dnr_pr)
		pr_info("reg:nCurGbs=%d,pGbs=%d, LR/LL/RR=[%d,%d,%d],dif=%d\n",
				nCurGbs, *pGbs, nGbsStatLR, nGbsStatLL, nGbsStatRR, nDif);


	*pGbs = nCurGbs;

	/* print debug info. */
	/* printk("GBS info at Field: LR = %6d, LL = %6d, RR = %6d.\n",
	 * nGbsStatLR, nGbsStatLL, nGbsStatRR;
	 */

	return 0;
}

#ifdef DNR_HV_SHIFT
int hor_blk_ofst_calc_sw(int *pHbOfVldCnt,
			 int *pHbOfVldFlg,
			 int *pHbOfst,
			 int nHbOfStatCnt[32],
			 int nXst,
			 int nXed,
			 int prm_hbof_minthd,
			 int prm_hbof_ratthd0,
			 int prm_hbof_ratthd1,
			 int prm_hbof_vldcntthd,
			 int nRow,
			 int nCol)
{
	int i = 0;

	int nCurHbOfst = 0;
	int nRat0 = 0, nRat1 = 0;

	int nMax1 = 0;
	int nMax2 = 0;
	int nMaxIdx = 0;

	/* get 2 maximum, move to RTL part */
	nMax1 = nMax2 = 0;
	for (i = 0; i < 8; i++) {
		if (nHbOfStatCnt[i] > nMax1) {
			nMax2 = nMax1;
			nMax1 = nHbOfStatCnt[i];
			nMaxIdx = i;
		} else if (nHbOfStatCnt[i] > nMax2) {
			nMax2 = nHbOfStatCnt[i];
		}
	if (dnr_pr)
		pr_info("nHbOfStatCnt[i]= %d\n", nHbOfStatCnt[i]);

	} /* i */

	/* decide if offset valid */
	nCurHbOfst = -1;
	nRat0 = 256*nMax1/((nXed - nXst)/8)/nRow;
	nRat1 = 128*nMax1/max(nMax2, prm_hbof_minthd);
	if (nRat0 >= prm_hbof_ratthd0 && nRat1 >= prm_hbof_ratthd1)
		nCurHbOfst = (nMaxIdx+1)%8;

	if (nCurHbOfst == *pHbOfst)
		(*pHbOfVldCnt)++;
	else
		*pHbOfVldCnt = 0;

	if (*pHbOfVldCnt >= prm_hbof_vldcntthd)
		*pHbOfVldFlg = 1;
	else
		*pHbOfVldFlg = 0;

	*pHbOfst = (nCurHbOfst == -1) ? 0 : nCurHbOfst;

	/* print for debug
	 * printk("Hoff info at Field: ");
	 * for ( i = 0; i < 32; i++ ) {
	 * printk("%5d, ",  nHbOfStatCnt[i]);
	 * }
	 */
	if (dnr_pr) {
		pr_info("Max1 = %5d, Max2 = %5d, MaxIdx = %5d, Rat0 = %5d,Rat1 = %5d.\n",
				nMax1, nMax2, nMaxIdx, nRat0, nRat1);
		pr_info("CurHbOfst = %5d, HbOfVldFlg = %d, HbOfVldCnt = %d.\n",
				nCurHbOfst, *pHbOfVldFlg, *pHbOfVldCnt);
	}

	return 0;
}
int ver_blk_ofst_calc_sw(int *pVbOfVldCnt,
			 int *pVbOfVldFlg,
			 int *pVbOfst,
			 int nVbOfStatCnt[32],
			 int nYst,
			 int nYed,
			 int prm_vbof_minthd,
			 int prm_vbof_ratthd0,
			 int prm_vbof_ratthd1,
			 int prm_vbof_vldcntthd,
			 int nRow,
			 int nCol)
{
	int i = 0;

	int nCurVbOfst = 0;
	int nRat0 = 0, nRat1 = 0;

	int nMax1 = 0;
	int nMax2 = 0;
	int nMaxIdx = 0;

	/* get 2 maximum, move to RTL part */
	nMax1 = nMax2 = 0;
	for (i = 0; i < 8; i++) {
		if (nVbOfStatCnt[i] > nMax1) {
			nMax2 = nMax1;
			nMax1 = nVbOfStatCnt[i];
			nMaxIdx = i;
		}
	else if (nVbOfStatCnt[i] > nMax2) {
		nMax2 = nVbOfStatCnt[i];
		}
	}

	/* decide if offset valid */
	nCurVbOfst = -1;
	nRat0 = 256*nMax1/((nYed - nYst)/8)/nCol;
	nRat1 = 128*nMax1/max(nMax2, prm_vbof_minthd);
	if (nRat0 >= prm_vbof_ratthd0 && nRat1 >= prm_vbof_ratthd1)
		nCurVbOfst = (nMaxIdx+1)%8;

	if (nCurVbOfst == *pVbOfst)
		(*pVbOfVldCnt)++;
	else
		*pVbOfVldCnt = 0;

	if (*pVbOfVldCnt >= prm_vbof_vldcntthd)
		*pVbOfVldFlg = 1;
	else
		*pVbOfVldFlg = 0;

	*pVbOfst = (nCurVbOfst == -1) ? 0 : nCurVbOfst;

	/* print for debug
	 * printk("Voff info at Field: ");
	 * for ( i = 0; i < 32; i++ ) {
	 * printk("%5d, ",  nVbOfStatCnt[i]);
	 * }//i
	 * //printk("Max1 = %5d, Max2 = %5d, MaxIdx = %5d, Rat0 = %5d,
	 * Rat1 = %5d, CurVbOfst = %5d, VbOfVldFlg = %d, VbOfVldCnt = %d.\n"
	 * nMax1, nMax2, nMaxIdx, nRat0, nRat1, nCurVbOfst, *pVbOfVldFlg,
	 * *pVbOfVldCnt);
	 */
	return 0;
}
#endif

static u32 check_dnr_dm_ctrl(u32 org_val, unsigned short width,
			     unsigned short height)
{
	if (!dynamic_dm_chk)
		return org_val;

	if (is_meson_tl1_cpu() || is_meson_tm2_cpu() ||
	    IS_IC(dil_get_cpuver_flag(), T5)	||
	    IS_IC(dil_get_cpuver_flag(), T5D)	||
	    IS_IC(dil_get_cpuver_flag(), T5DB)  ||
	    (cpu_after_eq(MESON_CPU_MAJOR_ID_SC2))) {
		/* disable dm chroma when > 720p */
		if (width > 1280)
			org_val &= ~(1 << 8);
		/* disable dm when > 1080p */
		if (width > 1920 || !dnr_dm_en)
			org_val &= ~(1 << 9);
	} else if (is_meson_gxlx_cpu() || is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu() || is_meson_sm1_cpu()) {
		/* disable chroma dm according to baozheng */
		org_val &= ~(1 << 8);
		/* disable dm when > 1080p */
		if (width > 1920 || !dnr_dm_en)
			org_val &= ~(1 << 9);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX) || is_meson_gxl_cpu()) {
		/* disable dm when >720p */
		if (width > 1280 || !dnr_dm_en) {
			org_val &= ~(1 << 8);
			/* disable dm for 1080 which will cause pre timeout*/
			org_val &= ~(1 << 9);
		}
	} else {
		/* disable dm when >= 1080p for other chips */
		if (width >= 1920 || !dnr_dm_en)
			org_val &= ~(1 << 9);
	}
	return org_val;
}

static void dnr_config_op(struct DNR_PARM_s *dnr_parm_p,
		const struct reg_acc *op, struct nr_cfg_s *cfg)
{
	unsigned short border_offset = dnr_parm_p->dnr_stat_coef;

	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	op->wr(DNR_HVSIZE, (cfg->width << 16) | cfg->height);
	op->wr(DNR_STAT_X_START_END,
	       (((border_offset << 3) & 0x3fff) << 16) |
	       ((cfg->width - ((border_offset << 3) + 1)) & 0x3fff));
	op->wr(DNR_STAT_Y_START_END,
	       (((border_offset << 3) & 0x3fff) << 16) |
	       ((cfg->height - ((border_offset << 3) + 1)) & 0x3fff));
	op->wr(DNR_DM_CTRL, op->rd(DNR_DM_CTRL) | (1 << 11));
	op->bwr(DNR_CTRL, cfg->linkflag ? 1 : 0, 17, 1);
	op->bwr(DNR_CTRL, dnr_en ? 1 : 0, 16, 1);
	/* dm for sd, hd will slower */
	if (is_meson_tl1_cpu() || is_meson_tm2_cpu() ||
	    IS_IC(dil_get_cpuver_flag(), T5)	||
	    IS_IC(dil_get_cpuver_flag(), T5D)	||
	    IS_IC(dil_get_cpuver_flag(), T5DB)	||
	    (cpu_after_eq(MESON_CPU_MAJOR_ID_SC2)))//from vlsi feijun
		op->wr(DNR_CTRL, 0x1df00 | (0x03 << 18)); //5 line
	else
		op->wr(DNR_CTRL, 0x1df00);

	if (is_meson_tl1_cpu() || is_meson_tm2_cpu() ||
	    IS_IC(dil_get_cpuver_flag(), T5)	||
	    IS_IC(dil_get_cpuver_flag(), T5D)	||
	    IS_IC(dil_get_cpuver_flag(), T5DB)  ||
	    (cpu_after_eq(MESON_CPU_MAJOR_ID_SC2))) {
		if (cfg->width > 1280)
			op->bwr(DNR_DM_CTRL, 0, 8, 1);
		else
			op->bwr(DNR_DM_CTRL, 1, 8, 1);
		if (cfg->width > 1920 || !dnr_dm_en)
			op->bwr(DNR_DM_CTRL, 0, 9, 1);
		else
			op->bwr(DNR_DM_CTRL, 1, 9, 1);
	} else if (is_meson_gxlx_cpu() || is_meson_g12a_cpu() ||
		is_meson_g12b_cpu() || is_meson_sm1_cpu()) {
		/* disable chroma dm according to baozheng */
		op->bwr(DNR_DM_CTRL, 0, 8, 1);
		op->wr(DNR_CTRL, 0x1dd00);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX) || is_meson_gxl_cpu()) {
		/*disable */
		if (cfg->width > 1280) {
			op->bwr(DNR_DM_CTRL, 0, 8, 1);
			/* disable dm for 1080 which will cause pre timeout*/
			op->bwr(DNR_DM_CTRL, 0, 9, 1);
		} else {
			op->bwr(DNR_DM_CTRL, 1, 8, 1);
			op->bwr(DNR_DM_CTRL, dnr_dm_en, 9, 1);
		}
	} else {
		if (cfg->width >= 1920)
			op->bwr(DNR_DM_CTRL, 0, 9, 1);
		else
			op->bwr(DNR_DM_CTRL, dnr_dm_en, 9, 1);
	}
}

static void nr4_config_op(struct NR4_PARM_s *nr4_parm_p,
			unsigned short width,
			unsigned short height,
		const struct reg_acc *op)
{
	unsigned short val = 0;
	int pq_temp = Rd(NR4_TOP_CTRL);
	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	val = nr4_parm_p->border_offset;
	/* luma enhancement*/
	op->wr(NR4_MCNR_LUMA_STAT_LIMTX, (val << 16) | (width - val - 1));
	op->wr(NR4_MCNR_LUMA_STAT_LIMTY, (val << 16) | (height - val - 1));
	if (dim_config_crc_ic()) {
		op->wr(NR4_MCNR_LUMAPRE_CAL_PRAM, 0);
		op->wr(NR4_MCNR_LUMACUR_CAL_PRAM, 0);
		Wr(NR4_MCNR_MV_CTRL_REG, 0x2408);
	} //add for crc @2k22-0102
	/* noise meter */
	op->wr(NR4_NM_X_CFG, (val << 16) | (width - val - 1));
	op->wr(NR4_NM_Y_CFG, (val << 16) | (height - val - 1));
	if (IS_IC(dil_get_cpuver_flag(), T3) && nr4ne_en) {
		op->wr(NR4_NE_X, width - 1);
		op->wr(NR4_NE_Y, height - 1);
	}
	/* enable nr4 */
	pq_temp |= (1 << 16);
	pq_temp |= (1 << 18);
	pq_temp |= (1 << 3);
	Wr(NR4_TOP_CTRL, pq_temp);
	if (IS_IC(dil_get_cpuver_flag(), S4) && dim_ic_sub() == 1) {
		pq_temp = Rd(NR4_TOP_CTRL);
		pq_temp &= ~(1 << 3);
		Wr(NR4_TOP_CTRL, pq_temp);
	}
	pq_temp = Rd(NR4_TOP_CTRL);
	pq_temp |= (1 << 5);
	Wr(NR4_TOP_CTRL, pq_temp);
	//add for crc @2k22-0102
	if (dim_config_crc_ic()) {
		pq_temp = Rd(NR4_TOP_CTRL);
		pq_temp &= ~(1 << 0);
		Wr(NR4_TOP_CTRL, pq_temp);
	}
	nr4_parm_p->width = width - val - val - 1;
	nr4_parm_p->height = height - val - val - 1;
}

/*
 * line buffer ctrl such as 5 lines or 3 lines
 * yuv444 or yuv422
 */
static void linebuffer_config_op(unsigned short width,
		const struct reg_acc *op)
{
	unsigned short val = 0;
	unsigned short line5_444 = 1368, line5_422 = 2052;
	unsigned short line3_444 = 2736;

	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	if (is_meson_txhd_cpu() || IS_IC(dil_get_cpuver_flag(), S4)) {
		line5_444 = 640;
		line5_422 = 960;
		line3_444 = 1280;
	}
	if (width <= line5_444)
		val = 3;
	else if (width <= line5_422)
		val = 1;
	else if (width <= line3_444)
		val = 2;
	else
		val = 0;
	/* line buffer no gate clock */
	op->bwr(LBUF_TOP_CTRL, 0, 20, 6);
	op->bwr(LBUF_TOP_CTRL, val, 16, 2);
}

static void nr2_config_op(unsigned short width, unsigned short height,
		const struct reg_acc *op)
{
	int pq_temp = 0;
	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	if (is_meson_txlx_cpu() || is_meson_g12a_cpu() ||
		is_meson_g12b_cpu() || is_meson_tl1_cpu() ||
		is_meson_sm1_cpu() || is_meson_tm2_cpu() ||
		IS_IC(dil_get_cpuver_flag(), T5)	||
		IS_IC(dil_get_cpuver_flag(), T5D)	||
		IS_IC(dil_get_cpuver_flag(), T5DB)	||
		cpu_after_eq(MESON_CPU_MAJOR_ID_SC2)) {
		if (IS_IC(dil_get_cpuver_flag(), S4) && dim_ic_sub() == 1) {
			op->wr(NR2_FRM_SIZE, (height << 16) |  width);
			op->wr(NR2_SW_EN, 0x70);
			Wr(NR4_TOP_CTRL, 0xf8ff4);
			}
		pq_temp = Rd(NR4_TOP_CTRL);
		if (nr2_en) {
			pq_temp |= (1 << 2);
			pq_temp |= (1 << 15);
			pq_temp |= (1 << 17);
		} else {
			pq_temp &= ~(1 << 2);
			pq_temp &= ~(1 << 15);
			pq_temp &= ~(1 << 17);
		}
		Wr(NR4_TOP_CTRL, pq_temp);
	} else {
		/*set max height to disable nfram cnt in cue*/
		if (is_meson_gxlx_cpu())
			op->wr(NR2_FRM_SIZE, (0xfff << 16) | width);
		else
			op->wr(NR2_FRM_SIZE, (height << 16) |  width);
		op->bwr(NR2_SW_EN, nr2_en, 4, 1);
	}
}

static bool cue_en = true;
module_param_named(cue_en, cue_en, bool, 0664);
/********************************************
 * debug cue_en
 ********************************************/
static bool cue_en_last;
static bool cue_en_force_disable;
module_param_named(cue_en_force_disable, cue_en_force_disable, bool, 0664);

/*
 * workaround for nframe count
 * indicating error field type in cue
 * from sc2 cure en from 0x1717[b26]->2dff[b26]
 */
static void cue_config_op(struct CUE_PARM_s *pcue_parm, unsigned short field_type,
		const struct reg_acc *op)
{
	int pq_temp = Rd(NR4_TOP_CTRL);
	pcue_parm->field_count = 8;
	pcue_parm->frame_count = 8;
	pcue_parm->field_count1 = 8;

	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_SC2) && (!IS_IC(dil_get_cpuver_flag(), T5D)) &&
	    (!IS_IC(dil_get_cpuver_flag(), T5)) && (!IS_IC(dil_get_cpuver_flag(), T5DB))) {
		pq_temp &= ~(1 << 1);
		if (field_type != VIDTYPE_PROGRESSIVE) {
			op->bwr(NR2_CUE_PRG_DIF, 0, 20, 1);
			Wr(NR4_TOP_CTRL, pq_temp);
			/* cur row mode avoid seek error */
			op->bwr(NR2_CUE_MODE, 5, 0, 4);
		} else {
			op->bwr(NR2_CUE_PRG_DIF, 1, 20, 1);
			/* disable cue for progressive issue */
			Wr(NR4_TOP_CTRL, pq_temp);
		}
	} else {
		if (field_type != VIDTYPE_PROGRESSIVE) {
			op->bwr(NR2_CUE_PRG_DIF, 0, 20, 1);
			op->bwr(DI_NR_CTRL0, 0, 26, 1);
			/* cur row mode avoid seek error */
			op->bwr(NR2_CUE_MODE, 5, 0, 4);
		} else {
			op->bwr(NR2_CUE_PRG_DIF, 1, 20, 1);
			/* disable cue for progressive issue */
			op->bwr(DI_NR_CTRL0, 0, 26, 1);
		}
	}
}

static void nr_all_config_op(unsigned short field_type,
		const struct reg_acc *op, struct nr_cfg_s *cfg)
{
	nr_param.width = cfg->width;
	nr_param.height = cfg->height;
	nr_param.frame_count = 0;
	nr_param.prog_flag = field_type?false:true;
	nr2_config_op(cfg->width, cfg->height, op);
	dnr_config_op(nr_param.pdnr_parm, op, cfg);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
		cue_config_op(nr_param.pcue_parm, field_type, op);
	if (is_meson_txlx_cpu() || is_meson_g12a_cpu() ||
		is_meson_g12b_cpu() || is_meson_tl1_cpu() ||
		is_meson_sm1_cpu() || is_meson_tm2_cpu() ||
		IS_IC(dil_get_cpuver_flag(), T5)	||
		IS_IC(dil_get_cpuver_flag(), T5D)	||
		IS_IC(dil_get_cpuver_flag(), T5DB)	||
		cpu_after_eq(MESON_CPU_MAJOR_ID_SC2)) {
		linebuffer_config_op(cfg->width, op);
		nr4_config_op(nr_param.pnr4_parm, cfg->width, cfg->height, op);
	}
	if (is_meson_txhd_cpu())
		linebuffer_config_op(cfg->width, op);
}

bool secam_cfr_en = true;
unsigned int cfr_phase1 = 1;/*0x179c[6]*/
unsigned int cfr_phase2 = 1;/*0x179c[7]*/
unsigned int gb_flg = 1;/*1:top, 0:bot*/

static ssize_t
secam_show(struct device *dev,
	   struct device_attribute *attr, char *buf)
{
	int len = 0;

	len += sprintf(buf + len,
		      "secam_cfr_en %u, gb_flg %u, cfr_phase2 %u.\n",
		      secam_cfr_en, gb_flg, cfr_phase2);
	return len;
}

static ssize_t
secam_store(struct device *dev, struct device_attribute *attr, const char *buf,
	    size_t count)
{
	char *parm[3] = { NULL }, *buf_orig;
	long val;
	ssize_t ret_ext = count;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));

	if (!parm[2]) {
		ret_ext = -EINVAL;
		pr_info("miss param!!\n");
	} else {
		if (kstrtol(parm[0], 10, &val) == 0)
			secam_cfr_en = val;
		if (kstrtol(parm[1], 10, &val) == 0)
			gb_flg = val;
		if (kstrtol(parm[2], 10, &val) == 0)
			cfr_phase2 = val;
	}

	kfree(buf_orig);

	return ret_ext;
}
static DEVICE_ATTR(secam, 0664, secam_show, secam_store);

static void secam_cfr_fun_op(int top,
		const struct reg_acc *op)
{
	int pq_temp = Rd(NR4_TOP_CTRL);
	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		pq_temp |= (1 << 12);
		Wr(NR4_TOP_CTRL, pq_temp);/*set cfr_en:1*/
	} else {
		op->bwr(NR2_SW_EN, 1, 7, 1);/*set cfr_en:1*/
	}
	op->bwr(NR2_CFR_PARA_CFG0, 1, 2, 2);
	op->bwr(NR2_CFR_PARA_CFG1, 0x208020, 0, 24);
	if ((gb_flg == 0 && top) || (gb_flg == 1 && !top)) {
		cfr_phase1 = ~cfr_phase1;
		op->bwr(NR2_CFR_PARA_CFG0, cfr_phase1, 6, 1);
	}
	op->bwr(NR2_CFR_PARA_CFG0, cfr_phase2, 7, 1);
}

void secam_cfr_adjust_op(unsigned int sig_fmt, unsigned int frame_type,
		const struct reg_acc *op)
{
	int pq_temp = Rd(NR4_TOP_CTRL);
	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}
	if (sig_fmt == TVIN_SIG_FMT_CVBS_SECAM && secam_cfr_en) {
		secam_cfr_fun_op((frame_type & VIDTYPE_TYPEMASK) ==
			      VIDTYPE_INTERLACE_TOP, op);
	} else {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
			pq_temp &= ~(1 << 12);
			Wr(NR4_TOP_CTRL, pq_temp);/*set cfr_en:0*/
		} else {
			op->bwr(NR2_SW_EN, 0, 7, 1);/*set cfr_en:0*/
		}
		op->bwr(NR2_CFR_PARA_CFG0, 2, 2, 2);
	}
}

static int find_lut16(unsigned int val, int *pLut)
{
	int idx_L, shft, dist;
	int left, right, norm;
	int res;

	if (val < 2) {
		idx_L = 0;
		dist  = val - 0;
		shft = 1;
	} else if (val < 4) {
		idx_L = 1;
		dist  = val - 2;
		shft = 1;
	} else if (val < 8) {
		idx_L = 2;
		dist  = val - 4;
		shft = 2;
	} else if (val < 16) {
		idx_L = 3;
		dist  = val - 8;
		shft = 3;
	} else if (val < 32) {
		idx_L = 4;
		dist  = val - 16;
		shft = 4;
	} else if (val < 48) {
		idx_L = 5;
		dist  = val - 32;
		shft = 4;
	} else if (val < 64) {
		idx_L = 6;
		dist  = val - 48;
		shft = 4;
	} else if (val < 80) {
		idx_L = 7;
		dist  = val - 64;
		shft = 4;
	} else if (val < 96) {
		idx_L = 8;
		dist  = val - 80;
		shft = 4;
	} else if (val < 112) {
		idx_L = 9;
		dist = val - 96;
		shft = 4;
	} else if (val < 128) {
		idx_L = 10;
		dist = val - 112;
		shft = 4;
	} else if (val < 160) {
		idx_L = 11;
		dist  = val - 128;
		shft = 5;
	} else if (val < 192) {
		idx_L = 12;
		dist = val - 160;
		shft = 5;
	} else if (val < 224) {
		idx_L = 13;
		dist = val - 192;
		shft = 5;
	} else {
		idx_L = 14;
		dist = val - 224;
		shft = 5;
	}
	left = pLut[idx_L];
	right = pLut[idx_L+1];
	norm = (1<<shft);
	res = ((left*(norm-dist) + right*dist + (norm>>1))>>shft);
	return res;
}

static void noise_meter_process_op(struct NR4_PARM_s *nr4_param_p,
		unsigned int field_cnt,
		const struct reg_acc *op)
{
	unsigned int val1 = 0, val2 = 0, field_sad = 0, field_var = 0;
	int val = 0;
	int nr4_temp = Rd(NR4_MCNR_MV_CTRL_REG);
	int pq_temp = Rd(NR4_TOP_CTRL);
	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	val1 = op->rd(NR4_RO_NM_SAD_CNT);
	val2 = op->rd(NR4_RO_NM_SAD_SUM);

	field_sad = (val1 == 0 ? 0 : (val2 + (val1>>1)) / val1);

	val1 = op->rd(NR4_RO_NM_VAR_SCNT);
	val2 = op->rd(NR4_RO_NM_VAR_SUM);
	field_var = (val1 == 0 ? 0 : (val2 + (val1>>1)) / val1);
	/*
	 *  field sad based global noise level to gain,
	 *  maybe improved further
	 */
	if (nr4_param_p->sw_nr4_sad2gain_en == 1) {
		val2 = (field_sad<<2) < 255 ? (field_sad<<2) : 255;
		val1 = find_lut16(val2, &nr4_param_p->sw_nr4_sad2gain_lut[0]);
	} else
		val1 = 64;
	nr4_temp &= ~(0xff0);
	nr4_temp |= (val1 << 4);
	Wr(NR4_MCNR_MV_CTRL_REG, nr4_temp);
	/*add for TL1------*/
	if (nr4_param_p->sw_nr4_noise_ctrl_dm_en == 1) {
		if (nr4_param_p->sw_nr4_noise_sel == 0) {
			val2 = val1 >= nr4_param_p->sw_nr4_noise_thd ? 1 : 0;
		} else {
			val2 =
			field_sad >= nr4_param_p->sw_nr4_noise_thd ? 1 : 0;
		}

		op->bwr(DNR_DM_NR_BLND, val2, 24, 1);
	}
	/*------------------*/
	/* scene change processing */
	nr4_param_p->sw_nr4_scene_change_flg[0] =
		nr4_param_p->sw_nr4_scene_change_flg[1];
	nr4_param_p->sw_nr4_scene_change_flg[1] =
		nr4_param_p->sw_nr4_scene_change_flg[2];
	val = field_sad - nr4_param_p->sw_nr4_field_sad[1];
	if (field_cnt > 2
		&& (val > nr4_param_p->sw_nr4_scene_change_thd)) {
		nr4_param_p->sw_nr4_scene_change_flg[2] = 1;
		if (nr4_param_p->nr4_debug) {
			pr_info("NR4 current field_sad=%d, sad[1]=%d, val=%d",
			field_sad, nr4_param_p->sw_nr4_field_sad[1], val);
		}
	} else
		nr4_param_p->sw_nr4_scene_change_flg[2] = 0;
	if (nr4_param_p->sw_nr4_scene_change_flg[1] ||
		nr4_param_p->sw_nr4_scene_change_flg[2]){
		pq_temp |= (1 << 0);
		Wr(NR4_TOP_CTRL, pq_temp);
	} else {
		pq_temp &= ~(1 << 0);
		Wr(NR4_TOP_CTRL, pq_temp);
	}

	/*fot TL1 **************/
	if (nr4_param_p->sw_dm_scene_change_en == 1) {
		val = field_sad >= nr4_param_p->sw_nr4_scene_change_thd2
			&& nr4_param_p->sw_nr4_field_sad[1]
				>= nr4_param_p->sw_nr4_scene_change_thd2;

		op->bwr(DNR_DM_CTRL, val, 12, 1);
	}
	/***********************/
	nr4_param_p->sw_nr4_field_sad[0] = nr4_param_p->sw_nr4_field_sad[1];
	nr4_param_p->sw_nr4_field_sad[1] = field_sad;
}

//auto global tnr
//function : update reg_MATNR_mtn_lp/hp_Ygain,
//           reg_MATNR_mtn_lp/hp_Cgain by motion_info and apl
//input : diff_sum, apl
//output : mtn_lp/hp_Ygain, mtn_lp/hp_Cgain
static void autonr_process_op(struct AUTONR_PARM_S *autonr_param_p,
		unsigned short input_width,
		unsigned short input_height,
		const struct reg_acc *op)
{
	unsigned int idx;
	u64 motion_sum;
	int apl_idx; //3bit
	u64 diff_sum;

	diff_sum = op->rd(DIPD_RO_COMB_0);

	//diff_sum norm
	/*coverity[SIGN_EXTENSION] misjudgment*/
	motion_sum = div64_u64(diff_sum * (1920 * 1080),
			       (input_height * input_width));
	if (autonr_en & 0x10)
		pr_info("%s start#### diff_sum=0x%llx,motion_sum=0x%llx\n", __func__,
			diff_sum, motion_sum);

	motion_sum = motion_sum >> 12;
	if (autonr_en & 0x10)
		pr_info("%s motion_sum=0x%llx\n", __func__, motion_sum);

	//get mtn gain
	apl_idx = get_lum_ave();
	if (autonr_en & 0x10)
		pr_info("%s lum_ave=%d\n", __func__, apl_idx);

	if (apl_idx <= 0) {
		apl_idx = 0;
	} else {
		apl_idx = apl_idx >> 5;
		if (apl_idx >= 7)
			apl_idx = 7;
	}
	if (autonr_en & 0x10)
		pr_info("%s apl_idx=%d\n", __func__, apl_idx);
	//apl control
	motion_sum = (motion_sum * autonr_param_p->apl_gain[apl_idx] + 16) >> 5;
	if (autonr_en & 0x10)
		pr_info("%s end#### motion_sum=0x%llx\n", __func__, motion_sum);

	if (motion_sum <= autonr_param_p->motion_th[0])
		idx = 0;
	else if (motion_sum <= autonr_param_p->motion_th[1])
		idx = 1;
	else if (motion_sum <= autonr_param_p->motion_th[2])
		idx = 2;
	else if (motion_sum <= autonr_param_p->motion_th[3])
		idx = 3;
	else if (motion_sum <= autonr_param_p->motion_th[4])
		idx = 4;
	else if (motion_sum <= autonr_param_p->motion_th[5])
		idx = 5;
	else if (motion_sum <= autonr_param_p->motion_th[6])
		idx = 6;
	else if (motion_sum <= autonr_param_p->motion_th[7])
		idx = 7;
	else if (motion_sum <= autonr_param_p->motion_th[8])
		idx = 8;
	else if (motion_sum <= autonr_param_p->motion_th[9])
		idx = 9;
	else if (motion_sum <= autonr_param_p->motion_th[10])
		idx = 10;
	else if (motion_sum <= autonr_param_p->motion_th[11])
		idx = 11;
	else if (motion_sum <= autonr_param_p->motion_th[12])
		idx = 12;
	else if (motion_sum <= autonr_param_p->motion_th[13])
		idx = 13;
	else if (motion_sum <= autonr_param_p->motion_th[14])
		idx = 14;
	else
		idx = 15;

	op->bwr(NR2_MATNR_MTN_GAIN,
		autonr_param_p->motion_lp_ygain[idx], 0, 8);
	op->bwr(NR2_MATNR_MTN_GAIN,
		autonr_param_p->motion_lp_cgain[idx], 8, 8);
	op->bwr(NR2_MATNR_MTN_GAIN,
		autonr_param_p->motion_hp_ygain[idx], 16, 8);
	op->bwr(NR2_MATNR_MTN_GAIN,
		autonr_param_p->motion_hp_cgain[idx], 24, 8);
	if (autonr_en & 0x10)
		pr_info("%s idx=%d.h=%d,GAIN=%d\n", __func__,
			idx, input_height, op->rd(NR2_MATNR_MTN_GAIN));
}

//sort from largest to smallest
static void sort(int *data, int datanum)
{
	int  i,  j, k;

	for (i = 0; i < datanum - 1; i++) {
		for (j = i + 1; j < datanum; j++) {
			if (data[i] < data[j]) {
				k = data[i];
				data[i] = data[j];
				data[j] = k;
			}
		}
	}
}

// calculate spatial by soft,
//Input is the array which hold the spatial sigma results
static int soft_cal_spatial_sigma(int nrow, int lysizespatial,
				  int totalnum,
				  struct NR4_NEPARM_S *nr4_neparm_p)
{
	int i;
	int a1, a2, a3, th;
	int num = 0;
	int sumvar = 0, sigma = 0;
	int data[16];

	// put the variable back into the register
	for (i = 0; i < totalnum; i++)
		data[i] = nr4_neparm_p->ro_nr4_ne_spatial_blockvar[i];

	sort(data, totalnum);
	a1 = data[0];    // max
	a2 = data[1];    // second max
	a3 = data[totalnum - 1]; // min
	if (a2 * 2 < a1 && a1 > 2048)
		th = (a2 - a3);
	else
		th = (a1 - a3);

	for (i = 0; i < totalnum; i++) {
		if ((data[i] * 2) < ((a3) * 2 + th)) {
			num = num + 1;
			sumvar = sumvar + data[i];
		}
	}
	if (num == 0)
		sigma = 0;
	else
		sigma = sumvar / num;
	sigma = sigma > 2047 ? 2047 : sigma;
	return sigma;
}

static int Soft_cal_final_sigma(int *vardata, int *meandata, int spatial_outsigma,
			 int temporal_outsigma, int varnum, int th)
{
	int i, sum_dif, num_sum;
	int Final_sigma;

	if (spatial_outsigma == 0) {
		Final_sigma = 0;
	} else if (spatial_outsigma < 307 && temporal_outsigma > 1638) {
		Final_sigma = 0;
	} else if (spatial_outsigma > 40 &&
		  spatial_outsigma < 307 && temporal_outsigma < 819) {
		sum_dif = 0;
		num_sum = 0;

		// Traversing the array of block var data
		for (i = 0; i < varnum; i++) {
			if (vardata[i] > 0) {
				sum_dif = sum_dif + meandata[i];
				num_sum = num_sum + 1;
			}
		}
		if (sum_dif > th && num_sum)
			Final_sigma = (spatial_outsigma + temporal_outsigma) *
				(1 / (sum_dif / num_sum));
		else
			Final_sigma = spatial_outsigma + temporal_outsigma;
	} else if (spatial_outsigma > temporal_outsigma ||
		  temporal_outsigma > 4096) {
		Final_sigma = spatial_outsigma;
	} else {
		Final_sigma = spatial_outsigma + temporal_outsigma;
	}
	return Final_sigma;
}

static void noise_meter2_process(struct NR4_NEPARM_S *nr4_neparm_p,
				 unsigned short nexsizein,
				 unsigned short neysizein)
{
	int blocknum = 16;
	int sumvar = 0;
	int spatial_blocknum = 16;
	int vardatalen = 16;
	int i, k, temporal_sigma;
	int th1;
	int final_sigma, spatial_sigma;
	int vardata[16], temporalblockmean[16];

	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[0] =
		Rd(RO_NR4_NE_SBVAR_0) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[1] =
		(Rd(RO_NR4_NE_SBVAR_0) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[2] =
		Rd(RO_NR4_NE_SBVAR_1) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[3] =
		(Rd(RO_NR4_NE_SBVAR_1) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[4] =
		Rd(RO_NR4_NE_SBVAR_2) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[5] =
		(Rd(RO_NR4_NE_SBVAR_2) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[6] =
		Rd(RO_NR4_NE_SBVAR_3) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[7] =
		(Rd(RO_NR4_NE_SBVAR_3) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[8] =
		Rd(RO_NR4_NE_SBVAR_4) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[9] =
		(Rd(RO_NR4_NE_SBVAR_4) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[10] =
		Rd(RO_NR4_NE_SBVAR_5) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[11] =
		(Rd(RO_NR4_NE_SBVAR_5) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[12] =
		Rd(RO_NR4_NE_SBVAR_6) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[13] =
		(Rd(RO_NR4_NE_SBVAR_6) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[14] =
		Rd(RO_NR4_NE_SBVAR_7) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_spatial_blockvar[15] =
		(Rd(RO_NR4_NE_SBVAR_7) >> 16) & 0x7ff;

	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[0] =
		Rd(RO_NR4_NE_TBVAR_0) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[1] =
		(Rd(RO_NR4_NE_TBVAR_0) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[2] =
		Rd(RO_NR4_NE_TBVAR_1) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[3] =
		(Rd(RO_NR4_NE_TBVAR_1) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[4] =
		Rd(RO_NR4_NE_TBVAR_2) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[5] =
		(Rd(RO_NR4_NE_TBVAR_2) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[6] =
		Rd(RO_NR4_NE_TBVAR_3) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[7] =
		(Rd(RO_NR4_NE_TBVAR_3) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[8] =
		Rd(RO_NR4_NE_TBVAR_4) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[9] =
		(Rd(RO_NR4_NE_TBVAR_4) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[10] =
		Rd(RO_NR4_NE_TBVAR_5) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[11] =
		(Rd(RO_NR4_NE_TBVAR_5) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[12] =
		Rd(RO_NR4_NE_TBVAR_6) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[13] =
		(Rd(RO_NR4_NE_TBVAR_6) >> 16) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[14] =
		Rd(RO_NR4_NE_TBVAR_7) & 0x7ff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockvar[15] =
		(Rd(RO_NR4_NE_TBVAR_7) >> 16) & 0x7ff;

	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[0] =
		Rd(RO_NR4_NE_TBSUM_0) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[1] =
		(Rd(RO_NR4_NE_TBSUM_0) >> 16) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[2] =
		Rd(RO_NR4_NE_TBSUM_0) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[3] =
		(Rd(RO_NR4_NE_TBSUM_0) >> 16) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[4] =
		Rd(RO_NR4_NE_TBSUM_0) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[5] =
		(Rd(RO_NR4_NE_TBSUM_0) >> 16) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[6] =
		Rd(RO_NR4_NE_TBSUM_0) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[7] =
		(Rd(RO_NR4_NE_TBSUM_0) >> 16) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[8] =
		Rd(RO_NR4_NE_TBSUM_0) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[9] =
		(Rd(RO_NR4_NE_TBSUM_0) >> 16) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[10] =
		Rd(RO_NR4_NE_TBSUM_0) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[11] =
		(Rd(RO_NR4_NE_TBSUM_0) >> 16) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[12] =
		Rd(RO_NR4_NE_TBSUM_0) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[13] =
		(Rd(RO_NR4_NE_TBSUM_0) >> 16) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[14] =
		Rd(RO_NR4_NE_TBSUM_0) & 0x1fff;
	nr4_neparm_p->ro_nr4_ne_temporal_blockmean[15] =
		(Rd(RO_NR4_NE_TBSUM_0) >> 16) & 0x1fff;

	//Function implementation
	// calculate temporal_sigma
	for (i = 0; i < blocknum; i++)
		sumvar += nr4_neparm_p->ro_nr4_ne_temporal_blockvar[i];

	temporal_sigma = ((sumvar + 8) >> 4);
	temporal_sigma = temporal_sigma > 2047 ? 2047 : temporal_sigma;
	nr4_neparm_p->ro_nr4_ne_temporalsigma = temporal_sigma;

	//if (NERow == NEYSizeIn - 1)
	//{
	th1 = (nr4_neparm_p->nr4_ne_spatial_th) << 4;
	for (k = 0; k < spatial_blocknum; k++) {
		vardata[k] = nr4_neparm_p->ro_nr4_ne_spatial_blockvar[k];
		temporalblockmean[k] =
			nr4_neparm_p->ro_nr4_ne_temporal_blockmean[k];
	}
	spatial_sigma = soft_cal_spatial_sigma(neysizein - 1,
					       nexsizein, spatial_blocknum,
					       nr4_neparm_p);
	final_sigma  =  Soft_cal_final_sigma(vardata, temporalblockmean,
					     spatial_sigma, temporal_sigma,
					     vardatalen, th1);
	nr4_neparm_p->ro_nr4_ne_spatialsigma = spatial_sigma;
	nr4_neparm_p->ro_nr4_ne_finalsigma = final_sigma;
	//pr_info("%s temporal=%x,spatial=%x,final=%x\n", __func__,
		//temporal_sigma, spatial_sigma, final_sigma);
	//}
}

static void luma_enhancement_process_op(struct NR4_PARM_s *nr4_param_p,
	unsigned int field_cnt,
		const struct reg_acc *op)
{
	unsigned int reg_val = 0, tmp1 = 0;

	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	tmp1 = nr4_param_p->width * nr4_param_p->height;
	if (field_cnt <= 2) {
		reg_val = (op->rd(NR4_MCNR_RO_U_SUM) + (tmp1 >> 1)) / tmp1;
		op->bwr(NR4_MCNR_LUMAPRE_CAL_PRAM, reg_val, 8, 8);
		reg_val = (op->rd(NR4_MCNR_RO_V_SUM) + (tmp1 >> 1)) / tmp1;
		op->bwr(NR4_MCNR_LUMAPRE_CAL_PRAM, reg_val, 0, 8);
	} else {
		reg_val = op->brd(NR4_MCNR_LUMACUR_CAL_PRAM, 8, 8);
		op->bwr(NR4_MCNR_LUMAPRE_CAL_PRAM, reg_val, 8, 8);
		reg_val = op->brd(NR4_MCNR_LUMACUR_CAL_PRAM, 0, 8);
		op->bwr(NR4_MCNR_LUMACUR_CAL_PRAM, reg_val, 0, 8);
	}
	if (dim_config_crc_ic())//add for crc @2k22-0102
		reg_val = field_cnt <= 1 ? 0 : op->brd
				(NR4_MCNR_LUMACUR_CAL_PRAM, 24, 2);
	else
		reg_val = op->brd(NR4_MCNR_LUMACUR_CAL_PRAM, 24, 2);
	op->bwr(NR4_MCNR_LUMAPRE_CAL_PRAM, reg_val, 24, 2);
	if (dim_config_crc_ic())//add for crc @2k22-0102
		reg_val = field_cnt <= 1 ? 0 : op->brd
			(NR4_MCNR_LUMACUR_CAL_PRAM, 16, 2);
	else
		reg_val = op->brd(NR4_MCNR_LUMACUR_CAL_PRAM, 16, 2);
	op->bwr(NR4_MCNR_LUMAPRE_CAL_PRAM, reg_val, 16, 2);
	reg_val = (op->rd(NR4_MCNR_RO_U_SUM) + (tmp1 >> 1)) / tmp1;
	op->bwr(NR4_MCNR_LUMACUR_CAL_PRAM, reg_val, 8, 8);
	reg_val = (op->rd(NR4_MCNR_RO_V_SUM) + (tmp1 >> 1)) / tmp1;
	op->bwr(NR4_MCNR_LUMACUR_CAL_PRAM, reg_val, 0, 8);
	reg_val = SGN2((int)(op->rd(NR4_MCNR_RO_GRDU_SUM)));
	op->bwr(NR4_MCNR_LUMACUR_CAL_PRAM, reg_val, 24, 2);
	reg_val = SGN2((int)(op->rd(NR4_MCNR_RO_GRDV_SUM)));
	op->bwr(NR4_MCNR_LUMACUR_CAL_PRAM, reg_val, 16, 2);
}

static void dnr_process_op(struct DNR_PARM_s *pdnrprm,
			   const struct reg_acc *op,
			unsigned short nexsizein,
			unsigned short neysizein)
{
	static int ro_gbs_stat_lr = 0, ro_gbs_stat_ll = 0, ro_gbs_stat_rr = 0,
	ro_gbs_stat_dif = 0, ro_gbs_stat_cnt = 0;
	/* int reg_dnr_stat_xst=0,reg_dnr_stat_xed=0,
	 * reg_dnr_stat_yst=0,reg_dnr_stat_yed=0;
	 */
#ifdef DNR_HV_SHIFT
	int ro_hbof_stat_cnt[32], ro_vbof_stat_cnt[32], i = 0;
	int nCol, nRow;
#endif
	int ll, lr;

	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	if (is_meson_tl1_cpu()) {
		ll = op->rd(DNR_RO_GBS_STAT_LR);
		lr = op->rd(DNR_RO_GBS_STAT_LL);
	} else {
		ll = op->rd(DNR_RO_GBS_STAT_LL);
		lr = op->rd(DNR_RO_GBS_STAT_LR);

	}
	if (ro_gbs_stat_lr != lr ||
		ro_gbs_stat_ll != ll ||
		ro_gbs_stat_rr != op->rd(DNR_RO_GBS_STAT_RR) ||
		ro_gbs_stat_dif != op->rd(DNR_RO_GBS_STAT_DIF) ||
		ro_gbs_stat_cnt != op->rd(DNR_RO_GBS_STAT_CNT)) {

		ro_gbs_stat_lr = lr;
		ro_gbs_stat_ll = ll;
		ro_gbs_stat_rr = op->rd(DNR_RO_GBS_STAT_RR);
		ro_gbs_stat_dif = op->rd(DNR_RO_GBS_STAT_DIF);
		ro_gbs_stat_cnt = op->rd(DNR_RO_GBS_STAT_CNT);
	} else {
		return;
	}

	global_bs_calc_sw(&pdnrprm->sw_gbs_vld_cnt,
			  &pdnrprm->sw_gbs_vld_flg,
			  &pdnrprm->sw_gbs,
			  ro_gbs_stat_lr,
			  ro_gbs_stat_ll,
			  ro_gbs_stat_rr,
			  ro_gbs_stat_dif,
			  ro_gbs_stat_cnt,
			  pdnrprm->prm_gbs_vldcntthd, /* prm below */
			  pdnrprm->prm_gbs_cnt_min,
			  pdnrprm->prm_gbs_ratcalcmod,
			  pdnrprm->prm_gbs_ratthd,
			  pdnrprm->prm_gbs_difthd,
			  pdnrprm->prm_gbs_bsdifthd,
			  pdnrprm->prm_gbs_calcmod);
#ifdef DNR_HV_SHIFT
	nCol = nexsizein;
	nRow = neysizein;

	for (i = 0; i < 32; i++)
		ro_hbof_stat_cnt[i] = op->rd(DNR_RO_HBOF_STAT_CNT_0 + i);
	for (i = 0; i < 32; i++)
		ro_vbof_stat_cnt[i] = op->rd(DNR_RO_VBOF_STAT_CNT_0 + i);
	hor_blk_ofst_calc_sw(&pdnrprm->sw_hbof_vld_cnt,
		 &pdnrprm->sw_hbof_vld_flg,
		 &pdnrprm->sw_hbof,
		 ro_hbof_stat_cnt,
		 0,
		 nCol - 1,
		 pdnrprm->prm_hbof_minthd,
		 pdnrprm->prm_hbof_ratthd0,
		 pdnrprm->prm_hbof_ratthd1,
		 pdnrprm->prm_hbof_vldcntthd,
		 nRow,
		 nCol);

	ver_blk_ofst_calc_sw(&pdnrprm->sw_vbof_vld_cnt,
			 &pdnrprm->sw_vbof_vld_flg,
			 &pdnrprm->sw_vbof,
			 ro_vbof_stat_cnt,
			 0,
			 nRow-1,
			 pdnrprm->prm_vbof_minthd,
			 pdnrprm->prm_vbof_ratthd0,
			 pdnrprm->prm_vbof_ratthd1,
			 pdnrprm->prm_vbof_vldcntthd,
			 nRow,
			 nCol);
#endif
	/* update hardware registers */
	if (pdnrprm->prm_sw_gbs_ctrl == 0) {
		op->wr(DNR_GBS,
		       (pdnrprm->sw_gbs_vld_flg == 1) ? pdnrprm->sw_gbs : 0);
	} else if (pdnrprm->prm_sw_gbs_ctrl == 1) {
		op->bwr(DNR_BLK_OFFST,
			(pdnrprm->sw_hbof_vld_flg == 1) ?
			pdnrprm->sw_hbof : 0, 4, 3);
		op->wr(DNR_GBS,
		       (pdnrprm->sw_hbof_vld_flg == 1 &&
		       pdnrprm->sw_gbs_vld_flg == 1) ? pdnrprm->sw_gbs : 0);
	} else if (pdnrprm->prm_sw_gbs_ctrl == 2) {
		op->bwr(DNR_BLK_OFFST,
			(pdnrprm->sw_vbof_vld_flg == 1) ?
			pdnrprm->sw_vbof : 0, 0, 3);
		op->wr(DNR_GBS,
		       (pdnrprm->sw_vbof_vld_flg == 1 &&
			pdnrprm->sw_gbs_vld_flg == 1) ? pdnrprm->sw_gbs : 0);
	} else if (pdnrprm->prm_sw_gbs_ctrl == 3) {
		op->bwr(DNR_BLK_OFFST,
			pdnrprm->sw_hbof_vld_flg == 1 ?
			pdnrprm->sw_hbof : 0, 4, 3);
		op->bwr(DNR_BLK_OFFST,
			pdnrprm->sw_vbof_vld_flg == 1 ?
			pdnrprm->sw_vbof : 0, 0, 3);
		op->wr(DNR_GBS,
		       (pdnrprm->sw_hbof_vld_flg == 1	&&
		       pdnrprm->sw_vbof_vld_flg == 1	&&
		       pdnrprm->sw_gbs_vld_flg == 1) ? pdnrprm->sw_gbs : 0);
	}
	if (dnr_pr)
		pr_info("reg:nCurGbs=%d,hbof/vld=[%d,%d],sw_gbs_ctrl=%d\n",
			pdnrprm->sw_gbs, pdnrprm->sw_hbof_vld_flg,
			pdnrprm->sw_gbs_vld_flg, pdnrprm->prm_sw_gbs_ctrl);
}

static bool invert_cue_phase;
module_param_named(invert_cue_phase, invert_cue_phase, bool, 0644);

static unsigned int cue_pr_cnt;
module_param_named(cue_pr_cnt, cue_pr_cnt, uint, 0644);

static bool cue_glb_mot_check_en = true;
module_param_named(cue_glb_mot_check_en, cue_glb_mot_check_en, bool, 0644);

/* confirm with vlsi-liuyanling, cue_process_irq is no use */
/* when CUE disable					*/

static void cue_process_irq_op(const struct reg_acc *op)
{
	int pre_field_num = 0, cue_invert = 0;
	int pq_temp = Rd(NR4_TOP_CTRL);
	int pq1_temp = Rd(DI_NR_CTRL0);
	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}
	if (is_meson_gxlx_cpu()) {
		pre_field_num = op->brd(DI_PRE_CTRL, 29, 1);
		if (invert_cue_phase)
			cue_invert = (pre_field_num?3:0);
		else
			cue_invert = (pre_field_num?0:3);
		if (cue_pr_cnt > 0) {
			pr_info("[DI]: chan2 field num %d, cue_invert %d.\n",
				pre_field_num, cue_invert);
			cue_pr_cnt--;
		}
		op->bwr(NR2_CUE_MODE, cue_invert, 10, 2);
	}
	if (!nr_param.prog_flag) {
		if (nr_param.frame_count > 1 && cue_glb_mot_check_en) {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_SC2) &&
			    (!IS_IC(dil_get_cpuver_flag(), T5D)) &&
			    (!IS_IC(dil_get_cpuver_flag(), T5)) &&
			    (!IS_IC(dil_get_cpuver_flag(), T5DB)))
				Wr(NR4_TOP_CTRL, cue_en ? (pq_temp | 0x2) : (pq_temp & 0xfffffffd));
			else
				Wr(DI_NR_CTRL0, cue_en ?
				(pq1_temp | 0x4000000) : (pq1_temp & 0xfbffffff));
			/*confirm with vlsi,fix jira SWPL-31571*/
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_SC2) &&
			    (!IS_IC(dil_get_cpuver_flag(), T5D)) &&
			    (!IS_IC(dil_get_cpuver_flag(), T5)) &&
			    (!IS_IC(dil_get_cpuver_flag(), T5DB)))
				op->bwr(MCDI_CTRL_MODE,
					       (!cue_en) ? 1 : 0, 16, 1);
		}
	}
	if (nr_param.frame_count == 5)
		op->bwr(NR2_CUE_MODE, 7, 0, 4);
}

void cue_int_op(struct vframe_s *vf, const struct reg_acc *op)
{
	int pq_temp = Rd(NR4_TOP_CTRL);
	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	/*confirm with vlsi-liuyanling, G12a cue must be disabled*/
	if (is_meson_g12a_cpu()) {
		cue_en = false;
		cue_glb_mot_check_en = false;
	} else if (vf && IS_VDIN_SRC(vf->source_type)) {
	/*VLSI-yanling suggest close cue(422/444) except local play(420)*/
		cue_en = false;
		cue_glb_mot_check_en = false;
	} else {
		if (cue_en_force_disable)
			cue_en = false;
		else
			cue_en = true;
		cue_glb_mot_check_en = true;
		if (cue_en_force_disable)
			cue_en = false;
	}
	if (cue_en_last != cue_en) {
		di_print("cue:chg1:%d->%d\n", cue_en_last, cue_en);
		cue_en_last = cue_en;
	}
	/*close cue when cue disable*/
	if (!cue_en) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_SC2) &&
		    (!IS_IC(dil_get_cpuver_flag(), T5D)) &&
		    (!IS_IC(dil_get_cpuver_flag(), T5)) &&
		    (!IS_IC(dil_get_cpuver_flag(), T5DB))) {
			pq_temp &= ~(1 << 0);
			Wr(NR4_TOP_CTRL, pq_temp);
		}
		else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
			op->bwr(DI_NR_CTRL0, 0, 26, 1);
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B)) {
		if (cue_en)
			op->bwr(NR2_CUE_MODE, 3, 10, 2);
	}
}

static bool glb_fieldck_en = true;
module_param_named(glb_fieldck_en, glb_fieldck_en, bool, 0644);

/* confirm with vlsi-liuyanling, cue_process_irq is no use */
/* when CUE disable					*/

void adaptive_cue_adjust_op(unsigned int frame_diff, unsigned int field_diff,
				const struct reg_acc *op)
{
	struct CUE_PARM_s *pcue_parm = nr_param.pcue_parm;
	unsigned int mask1, mask2;

	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	if (!cue_glb_mot_check_en)
		return;

	//if (is_meson_tl1_cpu() || is_meson_tm2_cpu()) {
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_SC2) &&
	    (!IS_IC(dil_get_cpuver_flag(), T5D)) &&
	    (!IS_IC(dil_get_cpuver_flag(), T5)) &&
	    (!IS_IC(dil_get_cpuver_flag(), T5DB))) {
		/*value from VLSI(yanling.liu)*/
		/*after SC2 need new setting 2020-08-04: */
		mask1 = 0x50362;
		mask2 = 0x00054357;
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12B)) {
		/*value from VLSI(yanling.liu) 2018-12-07: */
		/*after G12B need new setting 2019-06-24: */
		mask1 = 0x50332;
		mask2 = 0x00054357;
	} else { /*ori value*/
		mask1 = 0x50323;
		mask2 = 0x00054375;
	}

	if (frame_diff > pcue_parm->glb_mot_framethr) {
		pcue_parm->frame_count = pcue_parm->frame_count > 0 ?
			(pcue_parm->frame_count - 1) : 0;
	} else if (pcue_parm->frame_count < pcue_parm->glb_mot_fieldnum) {
		pcue_parm->frame_count = pcue_parm->frame_count + 1;
	}
	if (glb_fieldck_en) {
		if (field_diff < pcue_parm->glb_mot_fieldthr)
			pcue_parm->field_count =
			pcue_parm->field_count < pcue_parm->glb_mot_fieldnum ?
			(pcue_parm->field_count + 1) :
			pcue_parm->glb_mot_fieldnum;
		else if (pcue_parm->field_count > 0)
			pcue_parm->field_count = (pcue_parm->field_count - 1);
		/*--------------------------*/
		/*patch from vlsi-yanling to fix tv-7314 cue cause sawtooth*/
		if (field_diff < pcue_parm->glb_mot_fieldthr ||
			field_diff > pcue_parm->glb_mot_fieldthr1)
			pcue_parm->field_count1 =
			pcue_parm->field_count1 < pcue_parm->glb_mot_fieldnum ?
			(pcue_parm->field_count1 + 1) :
			pcue_parm->glb_mot_fieldnum;
		else if (pcue_parm->field_count1 > 0)
			pcue_parm->field_count1 = pcue_parm->field_count1 - 1;
		/*--------------------------*/
	}
	if (cue_glb_mot_check_en) {
		if (pcue_parm->frame_count >
			(pcue_parm->glb_mot_fieldnum - 6) &&
			pcue_parm->field_count1 >
			(pcue_parm->glb_mot_fieldnum - 6) &&
			!cue_en_force_disable)
			cue_en = true;
		else
			cue_en = false;

		if (cue_en != cue_en_last) {
			di_print("cue_en:chg2:%d->%d\n", cue_en_last, cue_en);
			di_print("\t%d,%d,%d\n",
					pcue_parm->frame_count,
					pcue_parm->field_count1,
					pcue_parm->glb_mot_fieldnum);
			cue_en_last = cue_en;
		}
		di_print("cue:%d,%d,%d\n",
					pcue_parm->frame_count,
					pcue_parm->field_count1,
					pcue_parm->glb_mot_fieldnum);
		/* for clockfuliness clip */
		if (pcue_parm->field_count >
				(pcue_parm->glb_mot_fieldnum - 6)) {
			op->wr(NR2_CUE_MODE,
			       mask1 | (op->rd(NR2_CUE_MODE) & 0xc00));
			op->wr(NR2_CUE_CON_MOT_TH, 0x03010e01);
		} else {
			op->wr(NR2_CUE_MODE,
			       mask2 | (op->rd(NR2_CUE_MODE) & 0xc00));
			op->wr(NR2_CUE_CON_MOT_TH, 0xa03c8c3c);
		}
	}
}

/*
 * insert nr ctrl regs into ctrl table
 */
bool set_nr_ctrl_reg_table(unsigned int addr, unsigned int value)
{
	unsigned int i = 0;
	struct NR_CTRL_REGS_s *pnr_regs = NULL;

	pnr_regs = nr_param.pnr_regs;
	for (i = 0; i < NR_CTRL_REG_NUM; i++) {
		if (pnr_regs->regs[i].addr == addr) {
			pnr_regs->regs[i].addr = addr;
			pnr_regs->regs[i].value = value;
			atomic_set(&pnr_regs->regs[i].load_flag, 1);
			if (nr_ctrl_reg)
				pr_info("NR_CTRL_REG[0x%x]=[0x%x].\n",
					addr, value);
			return true;
		}
	}
	return false;
}

/* load nr related ctrl regs */

static void nr_ctrl_reg_load_op(struct NR_CTRL_REGS_s *pnr_regs,
				const struct reg_acc *op)
{
	unsigned int i = 0;

	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	for (i = 0; i < pnr_regs->reg_num; i++) {
		if (atomic_read(&pnr_regs->regs[i].load_flag)) {
			if (pnr_regs->regs[i].addr == DNR_DM_CTRL)
				pnr_regs->regs[i].value = check_dnr_dm_ctrl
					(pnr_regs->regs[i].value,
					 nr_param.width,
					 nr_param.height);

			op->wr(pnr_regs->regs[i].addr,
				pnr_regs->regs[i].value);
			atomic_set(&pnr_regs->regs[i].load_flag, 0);
			if (nr_ctrl_reg) {
				pr_info("LOAD NR[0x%x]=[0x%x]\n",
					pnr_regs->regs[i].addr,
					pnr_regs->regs[i].value);
			}
		}
	}
}

static void nr_process_in_irq_op(const struct reg_acc *op)
{
	nr_param.frame_count++;
	nr_ctrl_reg_load_op(nr_param.pnr_regs, op);

	/* confirm with vlsi-liuyanling, cue_process_irq is no use */
	/* when CUE disable					*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX) &&
	    cue_glb_mot_check_en)
		cue_process_irq_op(op);
	if (dnr_en)
		dnr_process_op(&dnr_param, op, nr_param.width,
				     nr_param.height);
	if (is_meson_txlx_cpu() || is_meson_g12a_cpu()
		|| is_meson_g12a_cpu() || is_meson_tl1_cpu() ||
		is_meson_sm1_cpu() || is_meson_tm2_cpu() ||
		IS_IC(dil_get_cpuver_flag(), T5)	||
		IS_IC(dil_get_cpuver_flag(), T5D)	||
		IS_IC(dil_get_cpuver_flag(), T5DB)	||
		cpu_after_eq(MESON_CPU_MAJOR_ID_SC2)) {
		noise_meter_process_op(nr_param.pnr4_parm, nr_param.frame_count, op);
		luma_enhancement_process_op(nr_param.pnr4_parm,
				nr_param.frame_count, op);
	}
	if (IS_IC(dil_get_cpuver_flag(), T3) && nr4ne_en)
		noise_meter2_process(nr_param.pnr4_neparm, nr_param.width,
				     nr_param.height);
	if (((IS_IC_EF(dil_get_cpuver_flag(), T3)) ||
	     IS_IC(dil_get_cpuver_flag(), T5DB)) && autonr_en)
		autonr_process_op(nr_param.pautonr_parm,
				  nr_param.width, nr_param.height, op);
}

static dnr_param_t dnr_params[] = {
	{"prm_sw_gbs_ctrl", &(dnr_param.prm_sw_gbs_ctrl)},
	{"prm_gbs_vldcntthd", &(dnr_param.prm_gbs_vldcntthd)},
	{"prm_gbs_cnt_min", &(dnr_param.prm_gbs_cnt_min)},
	{"prm_gbs_ratcalcmod", &(dnr_param.prm_gbs_ratcalcmod)},
	{"prm_gbs_ratthd[0]", &(dnr_param.prm_gbs_ratthd[0])},
	{"prm_gbs_ratthd[1]", &(dnr_param.prm_gbs_ratthd[1])},
	{"prm_gbs_ratthd[2]", &(dnr_param.prm_gbs_ratthd[2])},
	{"prm_gbs_difthd[0]", &(dnr_param.prm_gbs_difthd[0])},
	{"prm_gbs_difthd[1]", &(dnr_param.prm_gbs_difthd[1])},
	{"prm_gbs_difthd[2]", &(dnr_param.prm_gbs_difthd[2])},
	{"prm_gbs_bsdifthd", &(dnr_param.prm_gbs_bsdifthd)},
	{"prm_gbs_calcmod", &(dnr_param.prm_gbs_calcmod)},
	{"sw_gbs", &(dnr_param.sw_gbs)},
	{"sw_gbs_vld_flg", &(dnr_param.sw_gbs_vld_flg)},
	{"sw_gbs_vld_cnt", &(dnr_param.sw_gbs_vld_cnt)},
	{"prm_hbof_minthd", &(dnr_param.prm_hbof_minthd)},
	{"prm_hbof_ratthd0", &(dnr_param.prm_hbof_ratthd0)},
	{"prm_hbof_ratthd1", &(dnr_param.prm_hbof_ratthd1)},
	{"prm_hbof_vldcntthd", &(dnr_param.prm_hbof_vldcntthd)},
	{"sw_hbof", &(dnr_param.sw_hbof)},
	{"sw_hbof_vld_flg", &(dnr_param.sw_hbof_vld_flg)},
	{"sw_hbof_vld_cnt", &(dnr_param.sw_hbof_vld_cnt)},
	{"prm_vbof_minthd", &(dnr_param.prm_vbof_minthd)},
	{"prm_vbof_ratthd0", &(dnr_param.prm_vbof_ratthd0)},
	{"prm_vbof_ratthd1", &(dnr_param.prm_vbof_ratthd1)},
	{"prm_vbof_vldcntthd", &(dnr_param.prm_vbof_vldcntthd)},
	{"sw_vbof", &(dnr_param.sw_vbof)},
	{"sw_vbof_vld_flg", &(dnr_param.sw_vbof_vld_flg)},
	{"sw_vbof_vld_cnt", &(dnr_param.sw_vbof_vld_cnt)},
	{"dnr_stat_coef", &(dnr_param.dnr_stat_coef)},
	{"", NULL}
};
static ssize_t dnr_param_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t count)
{
	int i = 0, value = 0;
	char *parm[2] = {NULL}, *buf_orig;

	buf_orig = kstrdup(buff, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));
	for (i = 0; dnr_params[i].addr; i++) {
		if (!strcmp(parm[0], dnr_params[i].name)) {
			value = kstrtol(parm[1], 10, NULL);
			*(dnr_params[i].addr) = value;
			pr_dbg("%s=%d.\n", dnr_params[i].name,
			*(dnr_params[i].addr));
		}
	}

	kfree(buf_orig);
	return count;
}

static ssize_t dnr_param_show(struct device *dev,
				struct device_attribute *attr,
				char *buff)
{
	ssize_t len = 0;
	int i = 0;

	for (i = 0; dnr_params[i].addr; i++)
		len += sprintf(buff+len, "%s=%d.\n",
		dnr_params[i].name, *(dnr_params[i].addr));
	return len;
}

static nr4_param_t nr4_params[NR4_PARAMS_NUM];
static void nr4_params_init(struct NR4_PARM_s *nr4_parm_p)
{
	nr4_params[0].name = "prm_nr4_srch_stp";
	nr4_params[0].addr = &(nr4_parm_p->prm_nr4_srch_stp);
	nr4_params[1].name = "sw_nr4_field_sad[0]";
	nr4_params[1].addr = &(nr4_parm_p->sw_nr4_field_sad[0]);
	nr4_params[2].name = "sw_nr4_field_sad[1]";
	nr4_params[2].addr = &(nr4_parm_p->sw_nr4_field_sad[1]);
	nr4_params[3].name = "sw_nr4_scene_change_thd";
	nr4_params[3].addr = &(nr4_parm_p->sw_nr4_scene_change_thd);
	nr4_params[4].name = "sw_nr4_scene_change_flg[0]";
	nr4_params[4].addr = &(nr4_parm_p->sw_nr4_scene_change_flg[0]);
	nr4_params[5].name = "sw_nr4_scene_change_flg[1]";
	nr4_params[5].addr = &(nr4_parm_p->sw_nr4_scene_change_flg[1]);
	nr4_params[6].name = "sw_nr4_scene_change_flg[2]";
	nr4_params[6].addr = &(nr4_parm_p->sw_nr4_scene_change_flg[2]);
	nr4_params[7].name = "sw_nr4_sad2gain_en";
	nr4_params[7].addr = &(nr4_parm_p->sw_nr4_sad2gain_en);
	nr4_params[8].name = "sw_nr4_sad2gain_lut[0]";
	nr4_params[8].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[0]);
	nr4_params[9].name = "sw_nr4_sad2gain_lut[1]";
	nr4_params[9].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[1]);
	nr4_params[10].name = "sw_nr4_sad2gain_lut[2]";
	nr4_params[10].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[2]);
	nr4_params[11].name = "sw_nr4_sad2gain_lut[3]";
	nr4_params[11].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[3]);
	nr4_params[12].name = "sw_nr4_sad2gain_lut[4]";
	nr4_params[12].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[4]);
	nr4_params[13].name = "sw_nr4_sad2gain_lut[5]";
	nr4_params[13].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[5]);
	nr4_params[14].name = "sw_nr4_sad2gain_lut[6]";
	nr4_params[14].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[6]);
	nr4_params[15].name = "sw_nr4_sad2gain_lut[7]";
	nr4_params[15].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[7]);
	nr4_params[16].name = "sw_nr4_sad2gain_lut[8]";
	nr4_params[16].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[8]);
	nr4_params[17].name = "sw_nr4_sad2gain_lut[9]";
	nr4_params[17].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[9]);
	nr4_params[18].name = "sw_nr4_sad2gain_lut[10]";
	nr4_params[18].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[10]);
	nr4_params[19].name = "sw_nr4_sad2gain_lut11]";
	nr4_params[19].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[11]);
	nr4_params[20].name = "sw_nr4_sad2gain_lut[12]";
	nr4_params[20].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[12]);
	nr4_params[21].name = "sw_nr4_sad2gain_lut13]";
	nr4_params[21].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[13]);
	nr4_params[22].name = "sw_nr4_sad2gain_lut[14]";
	nr4_params[22].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[14]);
	nr4_params[23].name = "sw_nr4_sad2gain_lut[15]";
	nr4_params[23].addr = &(nr4_parm_p->sw_nr4_sad2gain_lut[15]);
	nr4_params[24].name = "nr4_debug";
	nr4_params[24].addr = &(nr4_parm_p->nr4_debug);

	nr4_params[25].name = "sw_nr4_noise_thd";
	nr4_params[25].addr = &(nr4_parm_p->sw_nr4_noise_thd);
	nr4_params[26].name = "sw_nr4_noise_sel";
	nr4_params[26].addr = &(nr4_parm_p->sw_nr4_noise_sel);
	nr4_params[27].name = "sw_nr4_noise_ctrl_dm_en";
	nr4_params[27].addr = &(nr4_parm_p->sw_nr4_noise_ctrl_dm_en);
	nr4_params[28].name = "sw_nr4_scene_change_thd2";
	nr4_params[28].addr = &(nr4_parm_p->sw_nr4_scene_change_thd2);
	nr4_params[29].name = "sw_dm_scene_change_en";
	nr4_params[29].addr = &(nr4_parm_p->sw_dm_scene_change_en);

};

static ssize_t nr4_param_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t count)
{
	long i = 0, value = 0;
	char *parm[2] = {NULL}, *buf_orig;

	buf_orig = kstrdup(buff, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));
	for (i = 0; i < NR4_PARAMS_NUM; i++) {
		if (IS_ERR_OR_NULL(nr4_params[i].name) ||
		    IS_ERR_OR_NULL(nr4_params[i].addr))
			continue;
		if (!strcmp(parm[0], nr4_params[i].name)) {
			if (parm[1]) {
				if (kstrtol(parm[1], 10, &value) < 0)
					pr_err("DI: input value error.\n");
				*(nr4_params[i].addr) = value;
			}
			pr_info(" %d\n",    *(nr4_params[i].addr));
		}
	}

	kfree(buf_orig);
	return count;
}

static ssize_t nr4_param_show(struct device *dev,
				struct device_attribute *attr,
				char *buff)
{
	ssize_t len = 0;
	int i = 0;

	for (i = 0; i < NR4_PARAMS_NUM; i++) {
		if (IS_ERR_OR_NULL(nr4_params[i].name) ||
		    IS_ERR_OR_NULL(nr4_params[i].addr))
			continue;
		len += sprintf(buff+len, "%s=%d.\n",
		nr4_params[i].name, *(nr4_params[i].addr));
	}

	return len;
}

static DEVICE_ATTR(nr4_param, 0664, nr4_param_show, nr4_param_store);

static void nr4_param_init(struct NR4_PARM_s *nr4_parm_p)
{
	int k = 0;

	nr4_parm_p->border_offset = 8;
	nr4_parm_p->nr4_debug = 0;
	nr4_parm_p->prm_nr4_srch_stp = 1;
	nr4_parm_p->sw_nr4_field_sad[0] = 0;
	nr4_parm_p->sw_nr4_field_sad[1] = 0;
	nr4_parm_p->sw_nr4_scene_change_thd = 20;
	for (k = 0; k < 3; k++)
		nr4_parm_p->sw_nr4_scene_change_flg[k] = 0;
	nr4_parm_p->sw_nr4_sad2gain_en   =   1;
	nr4_parm_p->sw_nr4_sad2gain_lut[0] =   1;
	nr4_parm_p->sw_nr4_sad2gain_lut[1] =   2;
	nr4_parm_p->sw_nr4_sad2gain_lut[2] =   2;
	nr4_parm_p->sw_nr4_sad2gain_lut[3] =   4;
	nr4_parm_p->sw_nr4_sad2gain_lut[4] =   8;
	nr4_parm_p->sw_nr4_sad2gain_lut[5] =  16;
	nr4_parm_p->sw_nr4_sad2gain_lut[6] =  32;
	nr4_parm_p->sw_nr4_sad2gain_lut[7] =  63;
	nr4_parm_p->sw_nr4_sad2gain_lut[8] =  67;
	nr4_parm_p->sw_nr4_sad2gain_lut[9] = 104;
	nr4_parm_p->sw_nr4_sad2gain_lut[10] = 48;
	nr4_parm_p->sw_nr4_sad2gain_lut[11] = 32;
	nr4_parm_p->sw_nr4_sad2gain_lut[12] = 20;
	nr4_parm_p->sw_nr4_sad2gain_lut[13] = 16;
	nr4_parm_p->sw_nr4_sad2gain_lut[14] = 14;
	nr4_parm_p->sw_nr4_sad2gain_lut[15] = 9;

	nr4_parm_p->sw_nr4_noise_thd = 32;
	nr4_parm_p->sw_nr4_noise_sel = 0;
	nr4_parm_p->sw_nr4_noise_ctrl_dm_en = 0;
	nr4_parm_p->sw_nr4_scene_change_thd2 = 80;
	nr4_parm_p->sw_dm_scene_change_en = 0;
}

static autonr_param_t autonr_params[AUTONR_PARAMS_NUM];
static void autonr_params_init_th(struct AUTONR_PARM_S *autonr_parm_p)
{
	autonr_params[0].name = "motion_th[0]";
	autonr_params[0].addr = &autonr_parm_p->motion_th[0];
	autonr_params[1].name = "motion_th[1]";
	autonr_params[1].addr = &autonr_parm_p->motion_th[1];
	autonr_params[2].name = "motion_th[2]";
	autonr_params[2].addr = &autonr_parm_p->motion_th[2];
	autonr_params[3].name = "motion_th[3]";
	autonr_params[3].addr = &autonr_parm_p->motion_th[3];
	autonr_params[4].name = "motion_th[4]";
	autonr_params[4].addr = &autonr_parm_p->motion_th[4];
	autonr_params[5].name = "motion_th[5]";
	autonr_params[5].addr = &autonr_parm_p->motion_th[5];
	autonr_params[6].name = "motion_th[6]";
	autonr_params[6].addr = &autonr_parm_p->motion_th[6];
	autonr_params[7].name = "motion_th[7]";
	autonr_params[7].addr = &autonr_parm_p->motion_th[7];
	autonr_params[8].name = "motion_th[8]";
	autonr_params[8].addr = &autonr_parm_p->motion_th[8];
	autonr_params[9].name = "motion_th[9]";
	autonr_params[9].addr = &autonr_parm_p->motion_th[9];
	autonr_params[10].name = "motion_th[10]";
	autonr_params[10].addr = &autonr_parm_p->motion_th[10];
	autonr_params[11].name = "motion_th[11]";
	autonr_params[11].addr = &autonr_parm_p->motion_th[11];
	autonr_params[12].name = "motion_th[12]";
	autonr_params[12].addr = &autonr_parm_p->motion_th[12];
	autonr_params[13].name = "motion_th[13]";
	autonr_params[13].addr = &autonr_parm_p->motion_th[13];
	autonr_params[14].name = "motion_th[14]";
	autonr_params[14].addr = &autonr_parm_p->motion_th[14];
};

static void autonr_params_init_lp_ygain(struct AUTONR_PARM_S *autonr_parm_p)
{
	autonr_params[15].name = "motion_lp_ygain[0]";
	autonr_params[15].addr = &autonr_parm_p->motion_lp_ygain[0];
	autonr_params[16].name = "motion_lp_ygain[1]";
	autonr_params[16].addr = &autonr_parm_p->motion_lp_ygain[1];
	autonr_params[17].name = "motion_lp_ygain[2]";
	autonr_params[17].addr = &autonr_parm_p->motion_lp_ygain[2];
	autonr_params[18].name = "motion_lp_ygain[3]";
	autonr_params[18].addr = &autonr_parm_p->motion_lp_ygain[3];
	autonr_params[19].name = "motion_lp_ygain[4]";
	autonr_params[19].addr = &autonr_parm_p->motion_lp_ygain[4];
	autonr_params[20].name = "motion_lp_ygain[5]";
	autonr_params[20].addr = &autonr_parm_p->motion_lp_ygain[5];
	autonr_params[21].name = "motion_lp_ygain[6]";
	autonr_params[21].addr = &autonr_parm_p->motion_lp_ygain[6];
	autonr_params[22].name = "motion_lp_ygain[7]";
	autonr_params[22].addr = &autonr_parm_p->motion_lp_ygain[7];
	autonr_params[23].name = "motion_lp_ygain[8]";
	autonr_params[23].addr = &autonr_parm_p->motion_lp_ygain[8];
	autonr_params[24].name = "motion_lp_ygain[9]";
	autonr_params[24].addr = &autonr_parm_p->motion_lp_ygain[9];
	autonr_params[25].name = "motion_lp_ygain[10]";
	autonr_params[25].addr = &autonr_parm_p->motion_lp_ygain[10];
	autonr_params[26].name = "motion_lp_ygain[11]";
	autonr_params[26].addr = &autonr_parm_p->motion_lp_ygain[11];
	autonr_params[27].name = "motion_lp_ygain[12]";
	autonr_params[27].addr = &autonr_parm_p->motion_lp_ygain[12];
	autonr_params[28].name = "motion_lp_ygain[13]";
	autonr_params[28].addr = &autonr_parm_p->motion_lp_ygain[13];
	autonr_params[29].name = "motion_lp_ygain[14]";
	autonr_params[29].addr = &autonr_parm_p->motion_lp_ygain[14];
	autonr_params[30].name = "motion_lp_ygain[15]";
	autonr_params[30].addr = &autonr_parm_p->motion_lp_ygain[15];
};

static void autonr_params_init_hp_ygain(struct AUTONR_PARM_S *autonr_parm_p)
{
	autonr_params[31].name = "motion_hp_ygain[0]";
	autonr_params[31].addr = &autonr_parm_p->motion_hp_ygain[0];
	autonr_params[32].name = "motion_hp_ygain[1]";
	autonr_params[32].addr = &autonr_parm_p->motion_hp_ygain[1];
	autonr_params[33].name = "motion_hp_ygain[2]";
	autonr_params[33].addr = &autonr_parm_p->motion_hp_ygain[2];
	autonr_params[34].name = "motion_hp_ygain[3]";
	autonr_params[34].addr = &autonr_parm_p->motion_hp_ygain[3];
	autonr_params[35].name = "motion_hp_ygain[4]";
	autonr_params[35].addr = &autonr_parm_p->motion_hp_ygain[4];
	autonr_params[36].name = "motion_hp_ygain[5]";
	autonr_params[36].addr = &autonr_parm_p->motion_hp_ygain[5];
	autonr_params[37].name = "motion_hp_ygain[6]";
	autonr_params[37].addr = &autonr_parm_p->motion_hp_ygain[6];
	autonr_params[38].name = "motion_hp_ygain[7]";
	autonr_params[38].addr = &autonr_parm_p->motion_hp_ygain[7];
	autonr_params[39].name = "motion_hp_ygain[8]";
	autonr_params[39].addr = &autonr_parm_p->motion_hp_ygain[8];
	autonr_params[40].name = "motion_hp_ygain[9]";
	autonr_params[40].addr = &autonr_parm_p->motion_hp_ygain[9];
	autonr_params[41].name = "motion_hp_ygain[10]";
	autonr_params[41].addr = &autonr_parm_p->motion_hp_ygain[10];
	autonr_params[42].name = "motion_hp_ygain[11]";
	autonr_params[42].addr = &autonr_parm_p->motion_hp_ygain[11];
	autonr_params[43].name = "motion_hp_ygain[12]";
	autonr_params[43].addr = &autonr_parm_p->motion_hp_ygain[12];
	autonr_params[44].name = "motion_hp_ygain[13]";
	autonr_params[44].addr = &autonr_parm_p->motion_hp_ygain[13];
	autonr_params[45].name = "motion_hp_ygain[14]";
	autonr_params[45].addr = &autonr_parm_p->motion_hp_ygain[14];
	autonr_params[46].name = "motion_hp_ygain[15]";
	autonr_params[46].addr = &autonr_parm_p->motion_hp_ygain[15];
};

static void autonr_params_init_lp_cgain(struct AUTONR_PARM_S *autonr_parm_p)
{
	autonr_params[47].name = "motion_lp_cgain[0]";
	autonr_params[47].addr = &autonr_parm_p->motion_lp_cgain[0];
	autonr_params[48].name = "motion_lp_cgain[1]";
	autonr_params[48].addr = &autonr_parm_p->motion_lp_cgain[1];
	autonr_params[49].name = "motion_lp_cgain[2]";
	autonr_params[49].addr = &autonr_parm_p->motion_lp_cgain[2];
	autonr_params[50].name = "motion_lp_cgain[3]";
	autonr_params[50].addr = &autonr_parm_p->motion_lp_cgain[3];
	autonr_params[51].name = "motion_lp_cgain[4]";
	autonr_params[51].addr = &autonr_parm_p->motion_lp_cgain[4];
	autonr_params[52].name = "motion_lp_cgain[5]";
	autonr_params[52].addr = &autonr_parm_p->motion_lp_cgain[5];
	autonr_params[53].name = "motion_lp_cgain[6]";
	autonr_params[53].addr = &autonr_parm_p->motion_lp_cgain[6];
	autonr_params[54].name = "motion_lp_cgain[7]";
	autonr_params[54].addr = &autonr_parm_p->motion_lp_cgain[7];
	autonr_params[55].name = "motion_lp_cgain[8]";
	autonr_params[55].addr = &autonr_parm_p->motion_lp_cgain[8];
	autonr_params[56].name = "motion_lp_cgain[9]";
	autonr_params[56].addr = &autonr_parm_p->motion_lp_cgain[9];
	autonr_params[57].name = "motion_lp_cgain[10]";
	autonr_params[57].addr = &autonr_parm_p->motion_lp_cgain[10];
	autonr_params[58].name = "motion_lp_cgain[11]";
	autonr_params[58].addr = &autonr_parm_p->motion_lp_cgain[11];
	autonr_params[59].name = "motion_lp_cgain[12]";
	autonr_params[59].addr = &autonr_parm_p->motion_lp_cgain[12];
	autonr_params[60].name = "motion_lp_cgain[13]";
	autonr_params[60].addr = &autonr_parm_p->motion_lp_cgain[13];
	autonr_params[61].name = "motion_lp_cgain[14]";
	autonr_params[61].addr = &autonr_parm_p->motion_lp_cgain[14];
	autonr_params[62].name = "motion_lp_cgain[15]";
	autonr_params[62].addr = &autonr_parm_p->motion_lp_cgain[15];
};

static void autonr_params_init_hp_cgain(struct AUTONR_PARM_S *autonr_parm_p)
{
	autonr_params[63].name = "motion_hp_cgain[0]";
	autonr_params[63].addr = &autonr_parm_p->motion_hp_cgain[0];
	autonr_params[64].name = "motion_hp_cgain[1]";
	autonr_params[64].addr = &autonr_parm_p->motion_hp_cgain[1];
	autonr_params[65].name = "motion_hp_cgain[2]";
	autonr_params[65].addr = &autonr_parm_p->motion_hp_cgain[2];
	autonr_params[66].name = "motion_hp_cgain[3]";
	autonr_params[66].addr = &autonr_parm_p->motion_hp_cgain[3];
	autonr_params[67].name = "motion_hp_cgain[4]";
	autonr_params[67].addr = &autonr_parm_p->motion_hp_cgain[4];
	autonr_params[68].name = "motion_hp_cgain[5]";
	autonr_params[68].addr = &autonr_parm_p->motion_hp_cgain[5];
	autonr_params[69].name = "motion_hp_cgain[6]";
	autonr_params[69].addr = &autonr_parm_p->motion_hp_cgain[6];
	autonr_params[70].name = "motion_hp_cgain[7]";
	autonr_params[70].addr = &autonr_parm_p->motion_hp_cgain[7];
	autonr_params[71].name = "motion_hp_cgain[8]";
	autonr_params[71].addr = &autonr_parm_p->motion_hp_cgain[8];
	autonr_params[72].name = "motion_hp_cgain[9]";
	autonr_params[72].addr = &autonr_parm_p->motion_hp_cgain[9];
	autonr_params[73].name = "motion_hp_cgain[10]";
	autonr_params[73].addr = &autonr_parm_p->motion_hp_cgain[10];
	autonr_params[74].name = "motion_hp_cgain[11]";
	autonr_params[74].addr = &autonr_parm_p->motion_hp_cgain[11];
	autonr_params[75].name = "motion_hp_cgain[12]";
	autonr_params[75].addr = &autonr_parm_p->motion_hp_cgain[12];
	autonr_params[76].name = "motion_hp_cgain[13]";
	autonr_params[76].addr = &autonr_parm_p->motion_hp_cgain[13];
	autonr_params[77].name = "motion_hp_cgain[14]";
	autonr_params[77].addr = &autonr_parm_p->motion_hp_cgain[14];
	autonr_params[78].name = "motion_hp_cgain[15]";
	autonr_params[78].addr = &autonr_parm_p->motion_hp_cgain[15];
};

static void autonr_params_init_apl(struct AUTONR_PARM_S *autonr_parm_p)
{
	autonr_params[79].name = "apl_gain[0]";
	autonr_params[79].addr = &autonr_parm_p->apl_gain[0];
	autonr_params[80].name = "apl_gain[1]";
	autonr_params[80].addr = &autonr_parm_p->apl_gain[1];
	autonr_params[81].name = "apl_gain[2]";
	autonr_params[81].addr = &autonr_parm_p->apl_gain[2];
	autonr_params[82].name = "apl_gain[3]";
	autonr_params[82].addr = &autonr_parm_p->apl_gain[3];
	autonr_params[83].name = "apl_gain[4]";
	autonr_params[83].addr = &autonr_parm_p->apl_gain[4];
	autonr_params[84].name = "apl_gain[5]";
	autonr_params[84].addr = &autonr_parm_p->apl_gain[5];
	autonr_params[85].name = "apl_gain[6]";
	autonr_params[85].addr = &autonr_parm_p->apl_gain[6];
	autonr_params[86].name = "apl_gain[7]";
	autonr_params[86].addr = &autonr_parm_p->apl_gain[7];
};

static ssize_t autonr_param_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t count)
{
	long i = 0, value = 0;
	char *parm[17] = {NULL}, *buf_orig;

	buf_orig = kstrdup(buff, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));

	if (!strcmp(parm[0], "motion_th")) {
		for (i = 0; i < 15; i++) {
			if (parm[i + 1]) {
				if (kstrtol(parm[i + 1], 10, &value) < 0)
					pr_err("DI: input value error.\n");
				*autonr_params[i].addr = value;
			}
			if (autonr_en & 0x20)
				pr_info("%lx ,%x\n", i, *autonr_params[i].addr);
		}
	} else if (!strcmp(parm[0], "motion_lp_ygain")) {
		for (i = 0; i < 16; i++) {
			if (parm[i + 1]) {
				if (kstrtol(parm[i + 1], 10, &value) < 0)
					pr_err("DI: input value error.\n");
				*autonr_params[i + 15].addr = value;
			}
			if (autonr_en & 0x20)
				pr_info("%lx,%x\n", i + 15,
					*autonr_params[i + 15].addr);
		}
	} else if (!strcmp(parm[0], "motion_hp_ygain")) {
		for (i = 0; i < 16; i++) {
			if (parm[i + 1]) {
				if (kstrtol(parm[i + 1], 10, &value) < 0)
					pr_err("DI: input value error.\n");
				*autonr_params[i + 31].addr = value;
			}
			if (autonr_en & 0x20)
				pr_info("%lx,%x\n", i + 31,
					*autonr_params[i + 31].addr);
		}
	} else if (!strcmp(parm[0], "motion_lp_cgain")) {
		for (i = 0; i < 16; i++) {
			if (parm[i + 1]) {
				if (kstrtol(parm[i + 1], 10, &value) < 0)
					pr_err("DI: input value error.\n");
				*autonr_params[i + 47].addr = value;
			}
			if (autonr_en & 0x20)
				pr_info("%lx,%x\n", i + 47,
					*autonr_params[i + 47].addr);
		}
	} else if (!strcmp(parm[0], "motion_hp_cgain")) {
		for (i = 0; i < 16; i++) {
			if (parm[i + 1]) {
				if (kstrtol(parm[i + 1], 10, &value) < 0)
					pr_err("DI: input value error.\n");
				*autonr_params[i + 63].addr = value;
			}
			if (autonr_en & 0x20)
				pr_info("%lx,%x\n", i + 63,
					*autonr_params[i + 63].addr);
		}
	} else if (!strcmp(parm[0], "apl_gain")) {
		for (i = 0; i < 8; i++) {
			if (parm[i + 1]) {
				if (kstrtol(parm[i + 1], 10, &value) < 0)
					pr_err("DI: input value error.\n");
				*autonr_params[i + 79].addr = value;
			}
			if (autonr_en & 0x20)
				pr_info("%lx,%x\n", i + 79,
					*autonr_params[i + 79].addr);
		}
	} else {
		pr_info(" null\n");
	}

	kfree(buf_orig);
	return count;
}

static ssize_t autonr_param_show(struct device *dev,
				struct device_attribute *attr,
				char *buff)
{
	ssize_t len = 0;
	int i = 0;

	len += sprintf(buff + len, "%s=%d", "motion_th",
			*autonr_params[0].addr);
	for (i = 1; i < 15; i++) {
		if (IS_ERR_OR_NULL(autonr_params[i].name) ||
		    IS_ERR_OR_NULL(autonr_params[i].addr))
			continue;
		len += sprintf(buff + len, ",%d", *autonr_params[i].addr);
	}

	len += sprintf(buff + len, "\n%s=%d", "motion_lp_ygain",
			*autonr_params[15].addr);
	for (i = 16; i < 31; i++) {
		if (IS_ERR_OR_NULL(autonr_params[i].name) ||
		    IS_ERR_OR_NULL(autonr_params[i].addr))
			continue;
		len += sprintf(buff + len, ",%d", *autonr_params[i].addr);
	}

	len += sprintf(buff + len, "\n%s=%d", "motion_hp_ygain",
			*autonr_params[31].addr);
	for (i = 32; i < 47; i++) {
		if (IS_ERR_OR_NULL(autonr_params[i].name) ||
		    IS_ERR_OR_NULL(autonr_params[i].addr))
			continue;
		len += sprintf(buff + len, ",%d", *autonr_params[i].addr);
	}

	len += sprintf(buff + len, "\n%s=%d", "motion_lp_cgain",
			*autonr_params[47].addr);
	for (i = 48; i < 63; i++) {
		if (IS_ERR_OR_NULL(autonr_params[i].name) ||
		    IS_ERR_OR_NULL(autonr_params[i].addr))
			continue;
		len += sprintf(buff + len, ",%d", *autonr_params[i].addr);
	}

	len += sprintf(buff + len, "\n%s=%d", "motion_hp_cgain",
			*autonr_params[63].addr);
	for (i = 64; i < 79; i++) {
		if (IS_ERR_OR_NULL(autonr_params[i].name) ||
		    IS_ERR_OR_NULL(autonr_params[i].addr))
			continue;
		len += sprintf(buff + len, ",%d", *autonr_params[i].addr);
	}

	len += sprintf(buff + len, "\n%s=%d", "apl_gain",
			*autonr_params[79].addr);
	for (i = 80; i < 87; i++) {
		if (IS_ERR_OR_NULL(autonr_params[i].name) ||
		    IS_ERR_OR_NULL(autonr_params[i].addr))
			continue;
		len += sprintf(buff + len, ",%d", *autonr_params[i].addr);
	}
	len += sprintf(buff + len, "\n");
	return len;
}

static DEVICE_ATTR(autonr_param, 0664, autonr_param_show, autonr_param_store);

static void autonr_param_init(struct AUTONR_PARM_S *autonr_parm_p)
{
	int k = 0;

	for (k = 0; k < 16; k++)
		autonr_parm_p->motion_lp_ygain[k] = 64;
	for (k = 0; k < 16; k++)
		autonr_parm_p->motion_hp_ygain[k] = 64;
	for (k = 0; k < 16; k++)
		autonr_parm_p->motion_lp_cgain[k] = 64;
	for (k = 0; k < 16; k++)
		autonr_parm_p->motion_hp_cgain[k] = 64;
	for (k = 0; k < 8; k++)
		autonr_parm_p->apl_gain[k] = 32;

	autonr_parm_p->motion_th[0] = 0;
	autonr_parm_p->motion_th[1] = 1;
	autonr_parm_p->motion_th[2] = 2;
	autonr_parm_p->motion_th[3] = 4;
	autonr_parm_p->motion_th[4] = 6;
	autonr_parm_p->motion_th[5] = 8;
	autonr_parm_p->motion_th[6] = 9;
	autonr_parm_p->motion_th[7] = 10;
	autonr_parm_p->motion_th[8] = 12;
	autonr_parm_p->motion_th[9] = 14;
	autonr_parm_p->motion_th[10] = 16;
	autonr_parm_p->motion_th[11] = 18;
	autonr_parm_p->motion_th[12] = 20;
	autonr_parm_p->motion_th[13] = 34;
	autonr_parm_p->motion_th[14] = 30;
}

static void nr4_neparam_init(struct NR4_NEPARM_S *nr4_neparm_p)
{
	int k = 0;

	nr4_neparm_p->nr4_ne_spatial_th = 375;
	nr4_neparm_p->nr4_ne_spatial_pixel_step = 2;
	nr4_neparm_p->nr4_ne_temporal_pixel_step = 2;
	nr4_neparm_p->nr4_ne_xst = 14;
	nr4_neparm_p->nr4_ne_xed = 1904;
	nr4_neparm_p->nr4_ne_yst = 14;
	nr4_neparm_p->nr4_ne_yed = 1064;
	nr4_neparm_p->nr4_ne_luma_lowlimit = 16;
	nr4_neparm_p->nr4_ne_luma_higlimit = 235;

	nr4_neparm_p->ro_nr4_ne_finalsigma = 0;
	nr4_neparm_p->ro_nr4_ne_spatialsigma = 0;
	nr4_neparm_p->ro_nr4_ne_temporalsigma = 0;

	for (k = 0; k < 16; k++) {
		nr4_neparm_p->ro_nr4_ne_spatial_blockvar[k] = 0;
		nr4_neparm_p->ro_nr4_ne_temporal_blockmean[k] = 0;
		nr4_neparm_p->ro_nr4_ne_temporal_blockvar[k] = 0;
	}
	pr_info("%s\n", __func__);
}

static void cue_param_init(struct CUE_PARM_s *cue_parm_p)
{
	cue_parm_p->glb_mot_framethr = 1000;
	cue_parm_p->glb_mot_fieldnum = 20;
	cue_parm_p->glb_mot_fieldthr = 10;
	cue_parm_p->glb_mot_fieldthr1 = 1000;/*fix tv-7314 cue cause sawtooth*/
	cue_parm_p->field_count = 8;
	cue_parm_p->frame_count = 8;
	cue_parm_p->field_count1 = 8;/*fix tv-7314 cue cause sawtooth*/
}
static int dnr_prm_init(DNR_PRM_t *pPrm)
{
	pPrm->prm_sw_gbs_ctrl = 1;
	/*
	 * 0: update gbs, 1: update hoffst & gbs,
	 * 2: update voffst & gbs, 3: update all (hoffst & voffst & gbs).
	 */

	pPrm->prm_gbs_vldcntthd  =	4;
	pPrm->prm_gbs_cnt_min	 = 32;
	pPrm->prm_gbs_ratcalcmod =	1;/* 0: use LR, 1: use Dif */
	pPrm->prm_gbs_ratthd[0]  = 40;
	pPrm->prm_gbs_ratthd[1]  = 80;
	pPrm->prm_gbs_ratthd[2]  = 120;
	pPrm->prm_gbs_difthd[0]  = 25;
	pPrm->prm_gbs_difthd[1]  = 75;
	pPrm->prm_gbs_difthd[2]  = 125;
	pPrm->prm_gbs_bsdifthd	 = 2;
	pPrm->prm_gbs_calcmod	 = 1; /* 0:dif0, 1:dif1, 2: dif2 */

	pPrm->sw_gbs = 0;
	pPrm->sw_gbs_vld_flg = 0;
	pPrm->sw_gbs_vld_cnt = 0;

	pPrm->prm_hbof_minthd	 = 32;
	pPrm->prm_hbof_ratthd0	 = 150;
	pPrm->prm_hbof_ratthd1	 = 150;
	pPrm->prm_hbof_vldcntthd = 4;
	pPrm->sw_hbof			 = 0;
	pPrm->sw_hbof_vld_flg	 = 0;
	pPrm->sw_hbof_vld_cnt	 = 0;

	pPrm->prm_vbof_minthd	 = 32;
	pPrm->prm_vbof_ratthd0	 = 150;
	pPrm->prm_vbof_ratthd1	 = 120;
	pPrm->prm_vbof_vldcntthd = 4;
	pPrm->sw_vbof		  = 0;
	pPrm->sw_vbof_vld_flg = 0;
	pPrm->sw_vbof_vld_cnt = 0;
	pPrm->dnr_stat_coef = 3;
	return 0;
}

static DEVICE_ATTR(dnr_param, 0664, dnr_param_show, dnr_param_store);

static void nr_all_ctrl(bool enable, const struct reg_acc *op)
{
	unsigned char value = 0;
	unsigned int data;

	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	value = enable ? 1 : 0;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		/* nr4 nr2*/
		if (value) {
			data = Rd(NR4_TOP_CTRL);
			Wr(NR4_TOP_CTRL, data | 0x00040004);
			data = Rd(DNR_CTRL);
			Wr(DNR_CTRL, data | 0x00010000);
		} else {
			data = Rd(NR4_TOP_CTRL);
			Wr(NR4_TOP_CTRL, data & 0xfffbfffb);
			data = Rd(DNR_CTRL);
			Wr(DNR_CTRL, data & 0xfffeffff);
		}
	} else {
		op->bwr(NR2_SW_EN, value, 4, 1);
		op->bwr(DNR_CTRL, value, 16, 1);
	}

	if (IS_IC(dil_get_cpuver_flag(), S4) && dim_ic_sub() == 1) {
		Wr_reg_bits(NR4_TOP_CTRL, value, 1, 1);
		Wr_reg_bits(NR2_SW_EN, value, 4, 2);
		Wr_reg_bits(NR2_SW_EN, value, 3, 1);
		Wr_reg_bits(NR2_SW_EN, value, 7, 1);
	}
}

static void nr_demo_mode(bool enable, const struct reg_acc *op)
{
	int pq_temp = Rd(NR4_TOP_CTRL);
	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	if (enable) {
		pq_temp &= ~(0x1c0);
		Wr(NR4_TOP_CTRL, pq_temp);
		nr_demo_flag = 1;
	} else {
		pq_temp &= ~(0x1c0);
		pq_temp |= (0x7 << 6);
		Wr(NR4_TOP_CTRL, pq_temp);
		nr_demo_flag = 0;
	}
}

static ssize_t nr_dbg_store(struct device *dev,
				struct device_attribute *attr,
				const char *buff, size_t count)
{
	char *parm[2] = {NULL}, *buf_orig;

	buf_orig = kstrdup(buff, GFP_KERNEL);
	parse_cmd_params(buf_orig, (char **)(&parm));
	if (!strcmp(parm[0], "disable"))
		nr_all_ctrl(false, &dio_pre_regset);
	else if (!strcmp(parm[0], "enable"))
		nr_all_ctrl(true, &dio_pre_regset);
	else if (!strcmp(parm[0], "demo")) {
		if (!strcmp(parm[1], "enable")) {
			nr_demo_mode(true, &dio_pre_regset);
			pr_info("nr demo enable\n");
		} else if (!strcmp(parm[1], "disable")) {
			nr_demo_mode(false, &dio_pre_regset);
			pr_info("nr demo disable\n");
		}
	}

	kfree(buf_orig);
	return count;
}

static ssize_t nr_dbg_show(struct device *dev,
				struct device_attribute *attr,
				char *buff)
{
	ssize_t len = 0;

	len += sprintf(buff+len,
		"echo disable/enable to disable/enable nr(nr2/nr4/dnr).\n");
	len += sprintf(buff+len,
		"NR4_TOP_CTRL=0x%x DNR_CTRL=0x%x DI_NR_CTRL0=0x%x\n",
		Rd(NR4_TOP_CTRL), Rd(DNR_CTRL), Rd(DI_NR_CTRL0));

	return len;
}

static DEVICE_ATTR(nr_debug, 0664, nr_dbg_show, nr_dbg_store);

void nr_hw_init_op(const struct reg_acc *op)
{
	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}

	nr_gate_control_op(true, op);
	if (is_meson_tl1_cpu() || is_meson_tm2_cpu() ||
	    IS_IC(dil_get_cpuver_flag(), T5)	||
	    IS_IC(dil_get_cpuver_flag(), T5D)	||
	    IS_IC(dil_get_cpuver_flag(), T5DB)	||
	    (cpu_after_eq(MESON_CPU_MAJOR_ID_SC2)))
		op->wr(DNR_CTRL, 0x1df00 | (0x03 << 18));//5 line
	else
		op->wr(DNR_CTRL, 0x1df00);
	op->wr(NR3_MODE, 0x3);
	op->wr(NR3_COOP_PARA, 0x28ff00);
	op->wr(NR3_CNOOP_GAIN, 0x881900);
	op->wr(NR3_YMOT_PARA, 0x0c0a1e);
	op->wr(NR3_CMOT_PARA, 0x08140f);
	op->wr(NR3_SUREMOT_YGAIN, 0x100c4014);
	op->wr(NR3_SUREMOT_CGAIN, 0x22264014);
	nr_gate_control_op(false, op);
}

static void nr_gate_control_op(bool gate, const struct reg_acc *op)
{
	if (!op) {
		pr_error("%s:no op\n", __func__);
		return;
	}
	if (!is_meson_txlx_cpu() && !is_meson_g12a_cpu() &&
		!is_meson_g12b_cpu() && !is_meson_sm1_cpu() &&
		!is_meson_tl1_cpu() && !is_meson_tm2_cpu() &&
		!IS_IC(dil_get_cpuver_flag(), T5)	&&
		!IS_IC(dil_get_cpuver_flag(), T5D)	&&
		!IS_IC(dil_get_cpuver_flag(), T5DB)
		)
		return;
	if (gate) {
		/* enable nr auto gate */
		op->bwr(VIUB_GCLK_CTRL2, 0, 0, 2);
		/* enable dnr auto gate */
		op->bwr(VIUB_GCLK_CTRL2, 0, 8, 2);
		/* enable nr/dm blend auto gate */
		op->bwr(VIUB_GCLK_CTRL2, 0, 10, 2);
		/* enable nr internal snr gate */
		op->bwr(NR4_TOP_CTRL, 0, 30, 2);
		/* enable nr internal tnr gate */
		op->bwr(NR4_TOP_CTRL, 0, 28, 2);
		/* enable nr internal mcnr gate */
		op->bwr(NR4_TOP_CTRL, 0, 26, 2);
		/* enable nr internal cfr gate */
		op->bwr(NR4_TOP_CTRL, 0, 24, 2);
		/* enable nr internal det_polar gate */
		op->bwr(NR4_TOP_CTRL, 0, 22, 2);
		/* enable nr internal 3ddet gate */
		op->bwr(NR4_TOP_CTRL, 0, 20, 2);
	} else {
		/* enable nr auto gate */
		op->bwr(VIUB_GCLK_CTRL2, 1, 0, 2);
		/* enable dnr auto gate */
		op->bwr(VIUB_GCLK_CTRL2, 1, 8, 2);
		/* disable nr/dm blend auto gate */
		op->bwr(VIUB_GCLK_CTRL2, 1, 10, 2);
		/* disable nr internal snr gate */
		op->bwr(NR4_TOP_CTRL, 1, 30, 2);
		/* disable nr internal tnr gate */
		op->bwr(NR4_TOP_CTRL, 1, 28, 2);
		/* disable nr internal mcnr gate */
		op->bwr(NR4_TOP_CTRL, 1, 26, 2);
		/* disable nr internal cfr gate */
		op->bwr(NR4_TOP_CTRL, 1, 24, 2);
		/* disable nr internal det_polar gate */
		op->bwr(NR4_TOP_CTRL, 1, 22, 2);
		/* disable nr internal 3ddet gate */
		op->bwr(NR4_TOP_CTRL, 1, 20, 2);
	}
}
/*
 * set ctrl reg address need load in irq
 */
static void nr_ctrl_regs_init(struct NR_CTRL_REGS_s *pnr_regs)
{
	unsigned int i = 0;

	pnr_regs->regs[0].addr = NR4_TOP_CTRL;
	pnr_regs->regs[1].addr = NR_DB_FLT_CTRL;
	pnr_regs->regs[2].addr = DNR_DM_CTRL;
	pnr_regs->regs[3].addr = DI_NR_CTRL0;
	pnr_regs->regs[4].addr = DNR_CTRL;
	pnr_regs->regs[5].addr = NR2_CUE_PRG_DIF;
	pnr_regs->reg_num = NR_CTRL_REG_NUM;
	for (i = 0; i < pnr_regs->reg_num; i++) {
		pnr_regs->regs[i].value = 0;
		atomic_set(&pnr_regs->regs[i].load_flag, 0);
	}
}

void nr_drv_uninit(struct device *dev)
{
	if (nr_param.pnr4_parm) {
		vfree(nr_param.pnr4_parm);
		nr_param.pnr4_parm = NULL;
	}
	if (nr_param.pnr_regs) {
		vfree(nr_param.pnr_regs);
		nr_param.pnr_regs = NULL;
	}
	if (nr_param.pcue_parm) {
		vfree(nr_param.pcue_parm);
		nr_param.pcue_parm = NULL;
	}
	if (nr_param.pnr4_neparm) {
		vfree(nr_param.pnr4_neparm);
		nr_param.pnr4_neparm = NULL;
	}
	if (nr_param.pautonr_parm) {
		vfree(nr_param.pautonr_parm);
		nr_param.pautonr_parm = NULL;
	}
	device_remove_file(dev, &dev_attr_nr4_param);
	device_remove_file(dev, &dev_attr_dnr_param);
	device_remove_file(dev, &dev_attr_nr_debug);
	device_remove_file(dev, &dev_attr_secam);
	device_remove_file(dev, &dev_attr_autonr_param);
}
void nr_drv_init(struct device *dev)
{
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		nr_param.pnr4_parm = vmalloc(sizeof(struct NR4_PARM_s));
		if (IS_ERR(nr_param.pnr4_parm))
			pr_err("%s allocate nr4 parm error.\n", __func__);
		else {
			nr4_params_init(nr_param.pnr4_parm);
			nr4_param_init(nr_param.pnr4_parm);
			device_create_file(dev, &dev_attr_nr4_param);
		}
	}
	nr_param.pnr_regs = vmalloc(sizeof(struct NR_CTRL_REGS_s));
	if (IS_ERR(nr_param.pnr_regs))
		pr_err("%s allocate ctrl regs error.\n", __func__);
	else
		nr_ctrl_regs_init(nr_param.pnr_regs);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX)) {
		nr_param.pcue_parm = vmalloc(sizeof(struct CUE_PARM_s));
		if (IS_ERR(nr_param.pcue_parm))
			pr_err("%s allocate cue parm error.\n", __func__);
		else
			cue_param_init(nr_param.pcue_parm);
	}

	if (IS_IC(dil_get_cpuver_flag(), T3) && nr4ne_en) {
		nr_param.pnr4_neparm = vmalloc(sizeof(struct NR4_NEPARM_S));
		if (IS_ERR(nr_param.pnr4_neparm))
			pr_err("%s allocate ne parm error.\n", __func__);
		else
			nr4_neparam_init(nr_param.pnr4_neparm);
	}
	if (((IS_IC_EF(dil_get_cpuver_flag(), T3)) ||
	     IS_IC(dil_get_cpuver_flag(), T5DB)) && autonr_en) {
		nr_param.pautonr_parm = vmalloc(sizeof(*nr_param.pautonr_parm));
		if (IS_ERR(nr_param.pautonr_parm)) {
			pr_err("%s allocate autonr parm error.\n", __func__);
		} else {
			autonr_params_init_th(nr_param.pautonr_parm);
			autonr_params_init_lp_ygain(nr_param.pautonr_parm);
			autonr_params_init_hp_ygain(nr_param.pautonr_parm);
			autonr_params_init_lp_cgain(nr_param.pautonr_parm);
			autonr_params_init_hp_cgain(nr_param.pautonr_parm);
			autonr_params_init_apl(nr_param.pautonr_parm);
			autonr_param_init(nr_param.pautonr_parm);
			device_create_file(dev, &dev_attr_autonr_param);
		}
	}
	dnr_prm_init(&dnr_param);
	nr_param.pdnr_parm = &dnr_param;
	device_create_file(dev, &dev_attr_dnr_param);
	device_create_file(dev, &dev_attr_nr_debug);
	device_create_file(dev, &dev_attr_secam);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXBB))
		dnr_dm_en = true;
	else
		dnr_dm_en = false;
	if (IS_IC(dil_get_cpuver_flag(), S4) && dim_ic_sub() == 1) {
		dnr_en = false;
		dynamic_dm_chk = false;
		autonr_en = 0x0;
	}
}

static void nr_hw_init(void)
{
	nr_hw_init_op(&dio_pre_regset);
}

static void nr_gate_control(bool gate)
{
	nr_gate_control_op(gate, &dio_pre_regset);
}

static void nr_process_in_irq(void)
{
	nr_process_in_irq_op(&dio_pre_regset);
}

static void nr_all_config(unsigned short type, struct nr_cfg_s *cfg)
{
	nr_all_config_op(type, &dio_pre_regset, cfg);
}

static void cue_int(struct vframe_s *vf)
{
	cue_int_op(vf, &dio_pre_regset);
}

static void adaptive_cue_adjust(unsigned int frame_diff,
			    unsigned int field_diff)
{
	adaptive_cue_adjust_op(frame_diff, field_diff, &dio_pre_regset);
}

static void secam_cfr_adjust(unsigned int sig_fmt,
			 unsigned int frame_type)
{
	secam_cfr_adjust_op(sig_fmt, frame_type, &dio_pre_regset);
}

static const struct nr_op_s di_ops_nr = {
	.nr_hw_init		= nr_hw_init,
	.nr_gate_control	= nr_gate_control,
	.nr_drv_init		= nr_drv_init,
	.nr_drv_uninit		= nr_drv_uninit,
	.nr_process_in_irq	= nr_process_in_irq,
	.nr_all_config		= nr_all_config,
	.set_nr_ctrl_reg_table	= set_nr_ctrl_reg_table,
	.cue_int		= cue_int,
	.adaptive_cue_adjust	= adaptive_cue_adjust,
	.secam_cfr_adjust	= secam_cfr_adjust,
	/*.module_para = dim_seq_file_module_para_nr,*/
};

bool di_attach_ops_nr(const struct nr_op_s **ops)
{
	#if 0
	if (!ops)
		return false;

	memcpy(ops, &di_pd_ops, sizeof(struct pulldown_op_s));
	#else
	*ops = &di_ops_nr;
	#endif

	return true;
}
EXPORT_SYMBOL(di_attach_ops_nr);

const struct nr_op_s *di_nr_opl(void)
{
	return &di_ops_nr;
}

static const struct nr_opr_s di_ops_nr_op = {
	.nr_hw_init		= nr_hw_init_op,
	.nr_gate_control	= nr_gate_control_op,
	.nr_drv_init		= nr_drv_init,
	.nr_drv_uninit		= nr_drv_uninit,
	.nr_process_in_irq	= nr_process_in_irq_op,
	.nr_all_config		= nr_all_config_op,
	.set_nr_ctrl_reg_table	= set_nr_ctrl_reg_table,//same
	.cue_int		= cue_int_op,
	.adaptive_cue_adjust	= adaptive_cue_adjust_op,
	.secam_cfr_adjust	= secam_cfr_adjust_op,
	/*.module_para = dim_seq_file_module_para_nr,*/
};

//for local: have op
const struct nr_opr_s *di_nr_op(void)
{
	return &di_ops_nr_op;
}

bool di_attach_opr_nr(const struct nr_opr_s **ops)
{
	#ifdef HIS_CODE
	if (!ops)
		return false;

	memcpy(ops, &di_pd_ops, sizeof(struct pulldown_op_s));
	#else
	*ops = &di_ops_nr_op;
	#endif

	return true;
}
EXPORT_SYMBOL(di_attach_opr_nr);
