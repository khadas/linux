/*
 * drivers/amlogic/media/di_multi/di_reg_tab.c
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

#include <linux/amlogic/iomap.h>

#include "deinterlace.h"
#include "di_data_l.h"
#include "register.h"

static const struct reg_t rtab_contr[] = {
	/*--------------------------*/
	{VD1_AFBCD0_MISC_CTRL, 20, 2, 0, "VD1_AFBCD0_MISC_CTRL",
		"vd1_go_field_sel",
		"0: gofile;1: post;2: pre"},
	{VD1_AFBCD0_MISC_CTRL, 9, 1, 0, "",
		"afbc0_mux_vpp_mad",
		"afbc0 to 0:vpp; 1:di"},
	{VD1_AFBCD0_MISC_CTRL, 8, 1, 0, "",
		"di_mif0_en",
		":mif to 0-vpp;1-di"},
	/*--------------------------*/
	{DI_POST_CTRL, 12, 1, 0, "DI_POST_CTRL",
		"di_post_viu_link",
		""},
	{DI_POST_CTRL, 8, 1, 0, "",
		"di_vpp_out_en",
		""},
	/*--------------------------*/
	{VIU_MISC_CTRL0, 20, 1, 0, "VIU_MISC_CTRL0",
		"?",
		"?"},
	{VIU_MISC_CTRL0, 18, 1, 0, "",
		"Vdin0_wr_out_ctrl",
		"0: nr_inp to vdin;  1: vdin wr dout"},
	{VIU_MISC_CTRL0, 17, 1, 0, "",
		"Afbc_inp_sel",
		"0: mif to INP; 1: afbc to INP"},
	{VIU_MISC_CTRL0, 16, 1, 0, "",
		"di_mif0_en",
		" vd1(afbc) to di post(if0) enable"},
	/*--------------------------*/
	{DI_IF1_GEN_REG, 0, 1, 0, "DI_IF1_GEN_REG",
		"enable",
		""},

	/*--------------------------*/
	{DI_IF1_GEN_REG3, 8, 2, 0, "DI_IF1_GEN_REG3",
		"cntl_bits_mode",
		"0:8bit;1:10bit 422;2:10bit 444"},

	/*--------------------------*/
	{DI_IF2_GEN_REG3, 8, 2, 0, "DI_IF2_GEN_REG3",
		"cntl_bits_mode",
		"0:8bit;1:10bit 422;2:10bit 444"},

	/*--------------------------*/
	{DI_IF0_GEN_REG3, 8, 2, 0, "DI_IF0_GEN_REG3",
		"cntl_bits_mode",
		"0:8bit;1:10bit 422;2:10bit 444"},

	/*--------------------------*/
	{DI_POST_GL_CTRL, 31, 1, 0, "DI_POST_GL_CTRL",
		"post count enable",
		""},
	{DI_POST_GL_CTRL, 30, 1, 0, "",
			"post count reset",
			""},
	{DI_POST_GL_CTRL, 16, 14, 0, "",
			"total line number for post count",
			""},
	{DI_POST_GL_CTRL, 0, 14, 0, "",
			"the line number of post frame reset",
			""},

	{TABLE_FLG_END, 0, 0, 0, "end", "end", ""},

};

/**********************/
/* debug register     */
/**********************/
static unsigned int get_reg_bits(unsigned int val, unsigned int bstart,
				 unsigned int bw)
{
	return((val &
	       (((1L << bw) - 1) << bstart)) >> (bstart));
}

static void dbg_reg_tab(struct seq_file *s, const struct reg_t *pRegTab)
{
	struct reg_t creg;
	int i;
	unsigned int l_add;
	unsigned int val32 = 1, val;
	char *bname;
	char *info;

	i = 0;
	l_add = 0;
	creg = pRegTab[i];

	do {
		if (creg.add != l_add) {
			val32 = Rd(creg.add);		/*RD*/
			seq_printf(s, "add:0x%x = 0x%08x, %s\n",
				   creg.add, val32, creg.name);
			l_add = creg.add;
		}
		val = get_reg_bits(val32, creg.bit, creg.wid);	/*RD_B*/

		if (creg.bname)
			bname = creg.bname;
		else
			bname = "";
		if (creg.info)
			info = creg.info;
		else
			info = "";

		seq_printf(s, "\tbit[%d,%d]:\t0x%x[%d]:\t%s:\t%s\n",
			   creg.bit, creg.wid, val, val, bname, info);

		i++;
		creg = pRegTab[i];
		if (i > TABLE_LEN_MAX) {
			pr_info("warn: too long, stop\n");
			break;
		}
	} while (creg.add != TABLE_FLG_END);
}

int reg_con_show(struct seq_file *seq, void *v)
{
	dbg_reg_tab(seq, &rtab_contr[0]);
	return 0;
}

static const struct reg_t rtab_cue_int[] = {
	/*--------------------------*/
	{NR2_CUE_CON_DIF0, 0, 32, 0x1400, "NR2_CUE_CON_DIF0",
			NULL,
			NULL},
	{NR2_CUE_CON_DIF1, 0, 32, 0x80064, "NR2_CUE_CON_DIF1",
			NULL,
			NULL},
	{NR2_CUE_CON_DIF2, 0, 32, 0x80064, "NR2_CUE_CON_DIF2",
			NULL,
			NULL},
	{NR2_CUE_CON_DIF3, 0, 32, 0x80a0a, "NR2_CUE_CON_DIF3",
			NULL,
			NULL},
	{NR2_CUE_PRG_DIF, 0, 32, 0x80a0a, "NR2_CUE_PRG_DIF",
			NULL,
			NULL},
	{TABLE_FLG_END, 0, 0, 0, "end", "end", ""},
	/*--------------------------*/
};

/************************************************
 * register table
 ************************************************/
static bool di_g_rtab_cue(const struct reg_t **tab, unsigned int *tabsize)
{
	*tab = &rtab_cue_int[0];
	*tabsize = ARRAY_SIZE(rtab_cue_int);

	return true;
}

static unsigned int dim_reg_read(unsigned int addr)
{
	return aml_read_vcbus(addr);
}

static const struct reg_acc di_pre_regset = {
	.wr = dim_DI_Wr,
	.rd = dim_reg_read,
	.bwr = dim_RDMA_WR_BITS,
	.brd = dim_RDMA_RD_BITS,
};

static bool di_wr_tab(const struct reg_acc *ops,
		      const struct reg_t *ptab, unsigned int tabsize)
{
	int i;
	const struct reg_t *pl;

	pl = ptab;

	if (!ops	||
	    !tabsize	||
	    !ptab)
		return false;

	for (i = 0; i < tabsize; i++) {
		if (pl->add == TABLE_FLG_END ||
		    i > TABLE_LEN_MAX) {
			break;
		}

		if (pl->wid == 32)
			ops->wr(pl->add, pl->df_val);
		else
			ops->bwr(pl->add, pl->df_val, pl->bit, pl->wid);

		pl++;
	}

	return true;
}

bool dim_wr_cue_int(void)
{
	const struct reg_t *ptab;
	unsigned int tabsize;

	di_g_rtab_cue(&ptab, &tabsize);
	di_wr_tab(&di_pre_regset,
		  ptab,
		  tabsize);
	PR_INF("%s:finish\n", __func__);

	return true;
}

int dim_reg_cue_int_show(struct seq_file *seq, void *v)
{
	dbg_reg_tab(seq, &rtab_cue_int[0]);
	return 0;
}

