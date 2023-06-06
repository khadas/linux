// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/enhancement/amvecm/dnlp_cal.c
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
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/ve.h>
#include "dnlp_cal.h"
#include "amve.h"
#include "arch/vpp_regs.h"
#include <linux/amlogic/media/amvecm/ve.h>
#include <linux/amlogic/media/amvecm/dnlp_alg.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "ai_pq/ai_pq.h"
#include "reg_helper.h"

bool ve_en;
bool ve_en_pre;
unsigned int ve_dnlp_rt;
ulong ve_dnlp_lpf[64], ve_dnlp_reg[16];
ulong ve_dnlp_reg_v2[32];
ulong ve_dnlp_reg_def[16] = {
	0x0b070400,	0x1915120e,	0x2723201c,	0x35312e2a,
	0x47423d38,	0x5b56514c,	0x6f6a6560,	0x837e7974,
	0x97928d88,	0xaba6a19c,	0xbfbab5b0,	0xcfccc9c4,
	0xdad7d5d2,	0xe6e3e0dd,	0xf2efece9,	0xfdfaf7f4
};

static bool hist_sel = 1; /*1->vpp , 0->vdin*/
module_param(hist_sel, bool, 0664);
MODULE_PARM_DESC(hist_sel, "hist_sel");

static unsigned int dnlp_dbg_print; /*1->vpp , 0->vdin*/
module_param(dnlp_dbg_print, uint, 0664);
MODULE_PARM_DESC(dnlp_dbg_print, "dnlp_dbg_print");

bool dnlp_insmod_ok; /*0:fail, 1:ok*/
module_param(dnlp_insmod_ok, bool, 0664);
MODULE_PARM_DESC(dnlp_insmod_ok, "dnlp_insmod_ok");

struct dnlp_alg_s *dnlp_alg_function;
EXPORT_SYMBOL(dnlp_alg_function);

static struct dnlp_alg_input_param_s *dnlp_alg_input;
static struct dnlp_alg_output_param_s *dnlp_alg_output;
static struct dnlp_dbg_ro_param_s *dnlp_dbg_ro_param;
static struct dnlp_dbg_rw_param_s *dnlp_dbg_rw_param;
static struct dnlp_dbg_print_s *dnlp_dbg_printk;
static struct ble_whe_param_s *ble_whe_param;

int *dnlp_scurv_low_copy;
int *dnlp_scurv_mid1_copy;
int *dnlp_scurv_mid2_copy;
int *dnlp_scurv_hgh1_copy;
int *dnlp_scurv_hgh2_copy;
int *gain_var_lut49_copy;
int *wext_gain_copy;
int *adp_thrd_copy;
int *reg_blk_boost_12_copy;
int *reg_adp_ofset_20_copy;
int *reg_mono_protect_copy;
int *reg_trend_wht_expand_lut8_copy;
int *c_hist_gain_copy;
int *s_hist_gain_copy;

int *ro_luma_avg4_copy;
int *ro_var_d8_copy;
int *ro_scurv_gain_copy;
int *ro_blk_wht_ext0_copy;
int *ro_blk_wht_ext1_copy;
int *ro_dnlp_brightness_copy;
int *gmscurve_copy;
int *clash_curve_copy;
int *clsh_scvbld_copy;
int *blkwht_ebld_copy;

int *dnlp_printk_copy;

unsigned char *ve_dnlp_tgt_copy;
unsigned int *ve_dnlp_tgt_10b_copy;

unsigned int *pre_1_gamma_copy;
unsigned int *pre_0_gamma_copy;
unsigned int *ntstcnt_copy;
unsigned int *ve_dnlp_luma_sum_copy;
int *rbase_copy;
bool *menu_chg_en_copy;

/* only for debug */
struct dnlp_alg_param_s dnlp_alg_param;
struct param_for_dnlp_s *dnlp_alg_node_copy;
struct ve_dnlp_curve_param_s dnlp_curve_param_load;
struct ve_ble_whe_param_s ble_whe_param_load;
struct dnlp_parse_cmd_s dnlp_parse_cmd[] = {
	{"alg_enable", &dnlp_alg_param.dnlp_alg_enable},
	{"respond", &dnlp_alg_param.dnlp_respond},
	{"sel", &dnlp_alg_param.dnlp_sel},
	{"respond_flag", &dnlp_alg_param.dnlp_respond_flag},
	{"smhist_ck", &dnlp_alg_param.dnlp_smhist_ck},
	{"mvreflsh", &dnlp_alg_param.dnlp_mvreflsh},
	{"pavg_btsft", &dnlp_alg_param.dnlp_pavg_btsft},
	{"dbg_i2r", &dnlp_alg_param.dnlp_dbg_i2r},
	{"cuvbld_min", &dnlp_alg_param.dnlp_cuvbld_min},
	{"cuvbld_max", &dnlp_alg_param.dnlp_cuvbld_max},
	{"schg_sft", &dnlp_alg_param.dnlp_schg_sft},
	{"bbd_ratio_low", &dnlp_alg_param.dnlp_bbd_ratio_low},
	{"bbd_ratio_hig", &dnlp_alg_param.dnlp_bbd_ratio_hig},
	{"limit_rng", &dnlp_alg_param.dnlp_limit_rng},
	{"range_det", &dnlp_alg_param.dnlp_range_det},
	{"blk_cctr", &dnlp_alg_param.dnlp_blk_cctr},
	{"brgt_ctrl", &dnlp_alg_param.dnlp_brgt_ctrl},
	{"brgt_range", &dnlp_alg_param.dnlp_brgt_range},
	{"brght_add", &dnlp_alg_param.dnlp_brght_add},
	{"brght_max", &dnlp_alg_param.dnlp_brght_max},
	{"dbg_adjavg", &dnlp_alg_param.dnlp_dbg_adjavg},
	{"auto_rng", &dnlp_alg_param.dnlp_auto_rng},
	{"lowrange", &dnlp_alg_param.dnlp_lowrange},
	{"hghrange", &dnlp_alg_param.dnlp_hghrange},
	{"satur_rat", &dnlp_alg_param.dnlp_satur_rat},
	{"satur_max", &dnlp_alg_param.dnlp_satur_max},
	{"set_saturtn", &dnlp_alg_param.dnlp_set_saturtn},
	{"sbgnbnd", &dnlp_alg_param.dnlp_sbgnbnd},
	{"sendbnd", &dnlp_alg_param.dnlp_sendbnd},
	{"clashBgn", &dnlp_alg_param.dnlp_clashbgn},
	{"clashEnd", &dnlp_alg_param.dnlp_clashend},
	{"var_th", &dnlp_alg_param.dnlp_var_th},
	{"clahe_gain_neg", &dnlp_alg_param.dnlp_clahe_gain_neg},
	{"clahe_gain_pos", &dnlp_alg_param.dnlp_clahe_gain_pos},
	{"clahe_gain_delta", &dnlp_alg_param.dnlp_clahe_gain_delta},
	{"mtdbld_rate", &dnlp_alg_param.dnlp_mtdbld_rate},
	{"adpmtd_lbnd", &dnlp_alg_param.dnlp_adpmtd_lbnd},
	{"adpmtd_hbnd", &dnlp_alg_param.dnlp_adpmtd_hbnd},
	{"blkext_ofst", &dnlp_alg_param.dnlp_blkext_ofst},
	{"whtext_ofst", &dnlp_alg_param.dnlp_whtext_ofst},
	{"blkext_rate", &dnlp_alg_param.dnlp_blkext_rate},
	{"whtext_rate", &dnlp_alg_param.dnlp_whtext_rate},
	{"bwext_div4x_min", &dnlp_alg_param.dnlp_bwext_div4x_min},
	{"iRgnBgn", &dnlp_alg_param.dnlp_irgnbgn},
	{"iRgnEnd", &dnlp_alg_param.dnlp_irgnend},
	{"dbg_map", &dnlp_alg_param.dnlp_dbg_map},
	{"final_gain", &dnlp_alg_param.dnlp_final_gain},
	{"cliprate_v3", &dnlp_alg_param.dnlp_cliprate_v3},
	{"cliprate_min", &dnlp_alg_param.dnlp_cliprate_min},
	{"adpcrat_lbnd", &dnlp_alg_param.dnlp_adpcrat_lbnd},
	{"adpcrat_hbnd", &dnlp_alg_param.dnlp_adpcrat_hbnd},
	{"scurv_low_th", &dnlp_alg_param.dnlp_scurv_low_th},
	{"scurv_mid1_th", &dnlp_alg_param.dnlp_scurv_mid1_th},
	{"scurv_mid2_th", &dnlp_alg_param.dnlp_scurv_mid2_th},
	{"scurv_hgh1_th", &dnlp_alg_param.dnlp_scurv_hgh1_th},
	{"scurv_hgh2_th", &dnlp_alg_param.dnlp_scurv_hgh2_th},
	{"mtdrate_adp_en", &dnlp_alg_param.dnlp_mtdrate_adp_en},
	{"clahe_method", &dnlp_alg_param.dnlp_clahe_method},
	{"ble_en", &dnlp_alg_param.dnlp_ble_en},
	{"norm", &dnlp_alg_param.dnlp_norm},
	{"scn_chg_th", &dnlp_alg_param.dnlp_scn_chg_th},
	{"step_th", &dnlp_alg_param.dnlp_step_th},
	{"iir_step_mux", &dnlp_alg_param.dnlp_iir_step_mux},
	{"single_bin_bw", &dnlp_alg_param.dnlp_single_bin_bw},
	{"single_bin_method", &dnlp_alg_param.dnlp_single_bin_method},
	{"reg_max_slop_1st", &dnlp_alg_param.dnlp_reg_max_slop_1st},
	{"reg_max_slop_mid", &dnlp_alg_param.dnlp_reg_max_slop_mid},
	{"reg_max_slop_fin", &dnlp_alg_param.dnlp_reg_max_slop_fin},
	{"reg_min_slop_1st", &dnlp_alg_param.dnlp_reg_min_slop_1st},
	{"reg_min_slop_mid", &dnlp_alg_param.dnlp_reg_min_slop_mid},
	{"reg_min_slop_fin", &dnlp_alg_param.dnlp_reg_min_slop_fin},
	{"reg_trend_wht_expand_mode",
		&dnlp_alg_param.dnlp_reg_trend_wht_expand_mode},
	{"reg_trend_blk_expand_mode",
		&dnlp_alg_param.dnlp_reg_trend_blk_expand_mode},
	{"ve_hist_cur_gain", &dnlp_alg_param.dnlp_ve_hist_cur_gain},
	{"ve_hist_cur_gain_precise",
		&dnlp_alg_param.dnlp_ve_hist_cur_gain_precise},
	{"reg_mono_binrang_st", &dnlp_alg_param.dnlp_reg_mono_binrang_st},
	{"reg_mono_binrang_ed", &dnlp_alg_param.dnlp_reg_mono_binrang_ed},
	{"c_hist_gain_base", &dnlp_alg_param.dnlp_c_hist_gain_base},
	{"s_hist_gain_base", &dnlp_alg_param.dnlp_s_hist_gain_base},
	{"mvreflsh_offset", &dnlp_alg_param.dnlp_mvreflsh_offset},
	{"luma_avg_th", &dnlp_alg_param.dnlp_luma_avg_th},
	{"", NULL}
};

void dnlp_alg_param_copy(void)
{
	dnlp_scurv_low_copy = dnlp_dbg_rw_param->dnlp_scurv_low;
	dnlp_scurv_mid1_copy = dnlp_dbg_rw_param->dnlp_scurv_mid1;
	dnlp_scurv_mid2_copy = dnlp_dbg_rw_param->dnlp_scurv_mid2;
	dnlp_scurv_hgh1_copy = dnlp_dbg_rw_param->dnlp_scurv_hgh1;
	dnlp_scurv_hgh2_copy = dnlp_dbg_rw_param->dnlp_scurv_hgh2;
	gain_var_lut49_copy = dnlp_dbg_rw_param->gain_var_lut49;
	wext_gain_copy = dnlp_dbg_rw_param->wext_gain;
	adp_thrd_copy = dnlp_dbg_rw_param->adp_thrd;
	reg_blk_boost_12_copy = dnlp_dbg_rw_param->reg_blk_boost_12;
	reg_adp_ofset_20_copy = dnlp_dbg_rw_param->reg_adp_ofset_20;
	reg_mono_protect_copy = dnlp_dbg_rw_param->reg_mono_protect;
	reg_trend_wht_expand_lut8_copy =
		dnlp_dbg_rw_param->reg_trend_wht_expand_lut8;
	c_hist_gain_copy = dnlp_dbg_rw_param->c_hist_gain;
	s_hist_gain_copy = dnlp_dbg_rw_param->s_hist_gain;

	ro_luma_avg4_copy = dnlp_dbg_ro_param->ro_luma_avg4;
	ro_var_d8_copy = dnlp_dbg_ro_param->ro_var_d8;
	ro_scurv_gain_copy = dnlp_dbg_ro_param->ro_scurv_gain;
	ro_blk_wht_ext0_copy = dnlp_dbg_ro_param->ro_blk_wht_ext0;
	ro_blk_wht_ext1_copy = dnlp_dbg_ro_param->ro_blk_wht_ext1;
	ro_dnlp_brightness_copy = dnlp_dbg_ro_param->ro_dnlp_brightness;
	gmscurve_copy = dnlp_dbg_ro_param->gmscurve;
	clash_curve_copy = dnlp_dbg_ro_param->clash_curve;
	clsh_scvbld_copy = dnlp_dbg_ro_param->clsh_scvbld;
	blkwht_ebld_copy = dnlp_dbg_ro_param->blkwht_ebld;

	ve_dnlp_tgt_copy = dnlp_alg_output->ve_dnlp_tgt;
	ve_dnlp_tgt_10b_copy = dnlp_alg_output->ve_dnlp_tgt_10b;

	pre_1_gamma_copy = dnlp_alg_input->pre_1_gamma;
	pre_0_gamma_copy = dnlp_alg_input->pre_0_gamma;
	ntstcnt_copy = dnlp_alg_input->ntstcnt;
	ve_dnlp_luma_sum_copy = dnlp_alg_input->ve_dnlp_luma_sum;
	rbase_copy = dnlp_alg_input->rbase;
	menu_chg_en_copy = dnlp_alg_input->menu_chg_en;

	dnlp_printk_copy = dnlp_dbg_printk->dnlp_printk;
}

void dnlp_dbg_node_copy(void)
{
	memcpy(dnlp_alg_node_copy, &dnlp_alg_param,
	       sizeof(struct dnlp_alg_param_s));
}

void ble_whe_param_init(void)
{
	ble_whe_param_load.blk_adj_en = ble_whe_param->blk_adj_en;
	ble_whe_param_load.blk_end = ble_whe_param->blk_end;
	ble_whe_param_load.blk_slp = ble_whe_param->blk_slp;
	ble_whe_param_load.brt_adj_en = ble_whe_param->brt_adj_en;
	ble_whe_param_load.brt_start = ble_whe_param->brt_start;
	ble_whe_param_load.brt_slp = ble_whe_param->brt_slp;
}

void dnlp_alg_param_init(void)
{
	if (!dnlp_alg_function)
		return;
	dnlp_alg_param.dnlp_alg_enable = 0;
	dnlp_alg_param.dnlp_respond = 0;
	dnlp_alg_param.dnlp_sel = 2;
	dnlp_alg_param.dnlp_respond_flag = 0;
	dnlp_alg_param.dnlp_smhist_ck = 0;
	dnlp_alg_param.dnlp_mvreflsh = 6;
	dnlp_alg_param.dnlp_pavg_btsft = 5;
	dnlp_alg_param.dnlp_dbg_i2r = 255;
	dnlp_alg_param.dnlp_cuvbld_min = 3;
	dnlp_alg_param.dnlp_cuvbld_max = 17;
	dnlp_alg_param.dnlp_schg_sft = 1;
	dnlp_alg_param.dnlp_bbd_ratio_low = 16;
	dnlp_alg_param.dnlp_bbd_ratio_hig = 128;
	dnlp_alg_param.dnlp_limit_rng = 1;
	dnlp_alg_param.dnlp_range_det = 0;
	dnlp_alg_param.dnlp_blk_cctr = 8;
	dnlp_alg_param.dnlp_brgt_ctrl = 48;
	dnlp_alg_param.dnlp_brgt_range = 16;
	dnlp_alg_param.dnlp_brght_add = 32;
	dnlp_alg_param.dnlp_brght_max = 0;
	dnlp_alg_param.dnlp_dbg_adjavg = 0;
	dnlp_alg_param.dnlp_auto_rng = 0;
	dnlp_alg_param.dnlp_lowrange = 18;
	dnlp_alg_param.dnlp_hghrange = 18;
	dnlp_alg_param.dnlp_satur_rat = 30;
	dnlp_alg_param.dnlp_satur_max = 0;
	dnlp_alg_param.dnlp_set_saturtn = 0;
	dnlp_alg_param.dnlp_sbgnbnd = 4;
	dnlp_alg_param.dnlp_sendbnd = 4;
	dnlp_alg_param.dnlp_clashbgn = 4;
	dnlp_alg_param.dnlp_clashend = 59;
	dnlp_alg_param.dnlp_var_th = 16;
	dnlp_alg_param.dnlp_clahe_gain_neg = 120;
	dnlp_alg_param.dnlp_clahe_gain_pos = 24;
	dnlp_alg_param.dnlp_clahe_gain_delta = 32;
	dnlp_alg_param.dnlp_mtdbld_rate = 40;
	dnlp_alg_param.dnlp_adpmtd_lbnd = 19;
	dnlp_alg_param.dnlp_adpmtd_hbnd = 20;
	dnlp_alg_param.dnlp_blkext_ofst = 2;
	dnlp_alg_param.dnlp_whtext_ofst = 1;
	dnlp_alg_param.dnlp_blkext_rate = 32;
	dnlp_alg_param.dnlp_whtext_rate = 16;
	dnlp_alg_param.dnlp_bwext_div4x_min = 16;
	dnlp_alg_param.dnlp_irgnbgn = 0;
	dnlp_alg_param.dnlp_irgnend = 64;
	dnlp_alg_param.dnlp_dbg_map = 0;
	dnlp_alg_param.dnlp_final_gain = 8;
	dnlp_alg_param.dnlp_cliprate_v3 = 36;
	dnlp_alg_param.dnlp_cliprate_min = 19;
	dnlp_alg_param.dnlp_adpcrat_lbnd = 10;
	dnlp_alg_param.dnlp_adpcrat_hbnd = 20;
	dnlp_alg_param.dnlp_scurv_low_th = 32;
	dnlp_alg_param.dnlp_scurv_mid1_th = 48;
	dnlp_alg_param.dnlp_scurv_mid2_th = 112;
	dnlp_alg_param.dnlp_scurv_hgh1_th = 176;
	dnlp_alg_param.dnlp_scurv_hgh2_th = 240;
	dnlp_alg_param.dnlp_mtdrate_adp_en = 1;
	dnlp_alg_param.dnlp_clahe_method = 1;
	dnlp_alg_param.dnlp_ble_en = 1;
	dnlp_alg_param.dnlp_norm = 10;
	dnlp_alg_param.dnlp_scn_chg_th = 48;
	dnlp_alg_param.dnlp_step_th = 1;
	dnlp_alg_param.dnlp_iir_step_mux = 1;
	dnlp_alg_param.dnlp_single_bin_bw = 2;
	dnlp_alg_param.dnlp_single_bin_method = 1;
	dnlp_alg_param.dnlp_reg_max_slop_1st = 614;
	dnlp_alg_param.dnlp_reg_max_slop_mid = 400;
	dnlp_alg_param.dnlp_reg_max_slop_fin = 614;
	dnlp_alg_param.dnlp_reg_min_slop_1st = 77;
	dnlp_alg_param.dnlp_reg_min_slop_mid = 144;
	dnlp_alg_param.dnlp_reg_min_slop_fin = 77;
	dnlp_alg_param.dnlp_reg_trend_wht_expand_mode = 2;
	dnlp_alg_param.dnlp_reg_trend_blk_expand_mode = 2;
	dnlp_alg_param.dnlp_ve_hist_cur_gain = 8;
	dnlp_alg_param.dnlp_ve_hist_cur_gain_precise = 8;
	dnlp_alg_param.dnlp_reg_mono_binrang_st = 7;
	dnlp_alg_param.dnlp_reg_mono_binrang_ed = 26;
	dnlp_alg_param.dnlp_c_hist_gain_base = 128;
	dnlp_alg_param.dnlp_s_hist_gain_base = 128;
	dnlp_alg_param.dnlp_mvreflsh_offset = 0;
	dnlp_alg_param.dnlp_luma_avg_th = 50;

	if (dnlp_alg_function) {
		dnlp_alg_function->dnlp_para_set(&dnlp_alg_output,
			&dnlp_alg_input, &dnlp_dbg_rw_param, &dnlp_dbg_ro_param,
			&dnlp_alg_node_copy, &dnlp_dbg_printk, &ble_whe_param);

		dnlp_alg_param_copy();
		dnlp_dbg_node_copy();
		ble_whe_param_init();

		dnlp_insmod_ok = 1;
		pr_info("%s: is ok\n", __func__);
	} else {
		pr_info("%s: dnlp_alg_function is NULL\n",
			__func__);
	}
}

static void ve_dnlp_add_cm(unsigned int value)
{
	unsigned int reg_value;

	VSYNC_WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x207);
	reg_value = VSYNC_READ_VPP_REG(VPP_CHROMA_DATA_PORT);
	reg_value = (reg_value & 0xf000ffff) | (value << 16);
	VSYNC_WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x207);
	VSYNC_WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, reg_value);
}

/*in: vf (hist), h_sel
 *out: hstSum,pre_0_gamma, *osamebin_num
 *return: hstSum
 */
static int load_histogram(int *osamebin_num, struct vframe_s *vf,
			  int h_sel, unsigned int ntstcnt)
{
	struct vframe_prop_s *p = &vf->prop;
	int nT0;   /* counter number of bins same to last frame */
	int hstSum, nT1, i, j; /* sum of pixels in histogram */

	nT0 = 0;
	hstSum = 0;
	for (i = 0; i < 64; i++) {
		/* histogram stored for one frame delay */
		pre_1_gamma_copy[i] = pre_0_gamma_copy[i];
		if (h_sel)
			pre_0_gamma_copy[i] =
				(unsigned int)p->hist.vpp_gamma[i];
		else
			pre_0_gamma_copy[i] = (unsigned int)p->hist.gamma[i];

		/* counter the same histogram */
		if (pre_1_gamma_copy[i] == pre_0_gamma_copy[i])
			nT0++;
		else if (pre_1_gamma_copy[i] > pre_0_gamma_copy[i])
			nT1 = (pre_1_gamma_copy[i] - pre_0_gamma_copy[i]);
		else
			nT1 = (pre_0_gamma_copy[i] - pre_1_gamma_copy[i]);

		hstSum += pre_0_gamma_copy[i];
	}

	if (dnlp_dbg_print & 0x1)
		pr_info("\nRflsh%03d: %02d same bins hstSum(%d)\n",
			ntstcnt, nT0, hstSum);

	/* output, same hist bin nums of this frame */
	*osamebin_num = nT0;

	if (dnlp_dbg_print & 0x1) {
		pr_info("\n ==ntstcnt= %d====\n\n", ntstcnt);
			pr_info("\n#### %s()-1: [\n", __func__);
			for (j = 0; j < 4; j++) {
				i = j * 16;
				pr_info("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\n",
					pre_0_gamma_copy[i],
					pre_0_gamma_copy[i + 1],
					pre_0_gamma_copy[i + 2],
					pre_0_gamma_copy[i + 3],
					pre_0_gamma_copy[i + 4],
					pre_0_gamma_copy[i + 5],
					pre_0_gamma_copy[i + 6],
					pre_0_gamma_copy[i + 7],
					pre_0_gamma_copy[i + 8],
					pre_0_gamma_copy[i + 9],
					pre_0_gamma_copy[i + 10],
					pre_0_gamma_copy[i + 11],
					pre_0_gamma_copy[i + 12],
					pre_0_gamma_copy[i + 13],
					pre_0_gamma_copy[i + 14],
					pre_0_gamma_copy[i + 15]);
			}
			pr_info("  ]\n");
		pr_info("### ===== hstSum = %d ======\n", hstSum);
		pr_info("### ve_dnlp_luma_sum(raw) = %d\n",
			*ve_dnlp_luma_sum_copy);
	}

	return hstSum;
}

/* dnlp saturation compensations */
/* input: ve_dnlp_tgt[]; */
/* output: ve_dnlp_add_cm(nTmp + 512), and delta saturation; */
int dnlp_sat_compensation(void)
{
	int nT0, nT1, nTmp0, nTmp, i;
	unsigned int pre_stur = 0;

	nT0 = 0;	nT1 = 0;
	for (i = 1; i < 64; i++) {
		if (ve_dnlp_tgt_copy[i] > 4 * i) {
			nT0 += (ve_dnlp_tgt_copy[i] - 4 * i) * (65 - i);
			nT1 += (65 - i);
		}
	}
	nTmp0 = nT0 * dnlp_alg_param.dnlp_satur_rat + (nT1 >> 1);
	nTmp0 = nTmp0 / (nT1 + 1);
	nTmp0 = ((nTmp0 + 4) >> 3);

	nTmp =	(dnlp_alg_param.dnlp_satur_max << 3);
	if (nTmp0 < nTmp)
		nTmp = nTmp0;

	if (((dnlp_dbg_print) & 0x2))
		pr_info("#sat_comp: pre(%3d) => %5d / %3d => %3d cur(%3d)\n",
			pre_stur, nT0, nT1, nTmp0, nTmp);

	if (dnlp_alg_param.dnlp_set_saturtn == 0) {
		if (nTmp != pre_stur) {
			ve_dnlp_add_cm(nTmp + 512);
			pre_stur = nTmp;
		}
	} else {
		if (pre_stur != dnlp_alg_param.dnlp_set_saturtn) {
			if (dnlp_alg_param.dnlp_set_saturtn < 512)
				ve_dnlp_add_cm(dnlp_alg_param.dnlp_set_saturtn +
					512);
			else
				ve_dnlp_add_cm(dnlp_alg_param.dnlp_set_saturtn);
			pre_stur = dnlp_alg_param.dnlp_set_saturtn;
		}
	}

	return	nTmp;
}

int ve_dnlp_calculate_tgtx(struct vframe_s *vf)
{
	struct vframe_prop_s *p = &vf->prop;

	/*---- some intermediate variables ----*/
	unsigned int raw_hst_sum;
	int smbin_num;

	if (!dnlp_alg_function ||
	    dnlp_insmod_ok == 0)
		return 0;

	/* calc iir-coef params */
	if (dnlp_alg_param.dnlp_mvreflsh < 1)
		dnlp_alg_param.dnlp_mvreflsh = 1;
	*rbase_copy = (1 << dnlp_alg_param.dnlp_mvreflsh);

	/* parameters refresh */
	dnlp_alg_function->dnlp3_param_refresh();

	if (hist_sel)
		*ve_dnlp_luma_sum_copy = p->hist.vpp_luma_sum;
	else
		*ve_dnlp_luma_sum_copy = p->hist.luma_sum;

	/* counter the calling function  */
	(*ntstcnt_copy)++;
	if (*ntstcnt_copy > 16384)
		*ntstcnt_copy = 0;

	/*--------------------------------------------------------*/

	/*STEP A: load histogram and
	 *do histogram pre-processing and detections
	 */
	/*step 0.00 load the histogram*/
	raw_hst_sum = load_histogram(&smbin_num, vf, hist_sel, *ntstcnt_copy);

	/*step 0.01 all the same histogram as last frame, freeze DNLP */
	if (smbin_num == 64 &&
	    dnlp_alg_param.dnlp_smhist_ck &&
	    !dnlp_alg_param.dnlp_respond_flag) {
		if (dnlp_dbg_print & 0x2)
			pr_info("WARNING: invalid hist @ step 0.10\n");
		return 1;
	}

	dnlp_alg_function->dnlp_algorithm_main(raw_hst_sum);
	dnlp_sat_compensation();

	return 1;
}

 /* lpf[0] is always 0 & no need calculation */
void ve_dnlp_calculate_lpf(void)
{
	ulong i = 0;

	for (i = 0; i < 64; i++)
		ve_dnlp_lpf[i] = ve_dnlp_lpf[i] -
		(ve_dnlp_lpf[i] >> ve_dnlp_rt) + ve_dnlp_tgt_copy[i];
}

void ve_dnlp_calculate_reg(void)
{
	ulong i = 0, j = 0, cur = 0, data = 0,
			offset = ve_dnlp_rt ? (1 << (ve_dnlp_rt - 1)) : 0;
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		for (i = 0; i < 32; i++) {
			ve_dnlp_reg_v2[i] = 0;
			cur = i << 1;
			for (j = 0; j < 2; j++) {
				data = ve_dnlp_lpf[cur + j] << 2;
				if (data > 1023)
					data = 1023;
				ve_dnlp_reg_v2[i] |= data << (j << 4);
			}
		}
	} else {
		for (i = 0; i < 16; i++) {
			ve_dnlp_reg[i] = 0;
			cur = i << 2;
			for (j = 0; j < 4; j++) {
				data = (ve_dnlp_lpf[cur + j] + offset) >>
					ve_dnlp_rt;
				if (data > 255)
					data = 255;
				ve_dnlp_reg[i] |= data << (j << 3);
			}
		}
	}
}

void ai_dnlp_param_update(int value)
{
	dnlp_alg_param.dnlp_final_gain = value;
	if (dnlp_insmod_ok == 0)
		return;
	dnlp_dbg_node_copy();
}

void ve_set_v3_dnlp(struct ve_dnlp_curve_param_s *p)
{
	/* get command parameters */
	/* general settings */
	if (dnlp_insmod_ok == 0)
		return;
	/*load static curve*/
	memcpy(dnlp_scurv_low_copy, p->ve_dnlp_scurv_low,
	       sizeof(int) * DNLP_SCURV_LEN);
	memcpy(dnlp_scurv_mid1_copy, p->ve_dnlp_scurv_mid1,
	       sizeof(int) * DNLP_SCURV_LEN);
	memcpy(dnlp_scurv_mid2_copy, p->ve_dnlp_scurv_mid2,
	       sizeof(int) * DNLP_SCURV_LEN);
	memcpy(dnlp_scurv_hgh1_copy, p->ve_dnlp_scurv_hgh1,
	       sizeof(int) * DNLP_SCURV_LEN);
	memcpy(dnlp_scurv_hgh2_copy, p->ve_dnlp_scurv_hgh2,
	       sizeof(int) * DNLP_SCURV_LEN);
	/*load gain var*/
	memcpy(gain_var_lut49_copy, p->ve_gain_var_lut49,
	       sizeof(int) * GAIN_VAR_LUT_LEN);
	/*load wext gain*/
	memcpy(wext_gain_copy, p->ve_wext_gain,
	       sizeof(int) * WEXT_GAIN_LEN);
	/*load new c curve lut for vlsi-kite.li*/
	memcpy(adp_thrd_copy, p->ve_adp_thrd,
	       sizeof(int) * ADP_THRD_LEN);
	memcpy(reg_blk_boost_12_copy, p->ve_reg_blk_boost_12,
	       sizeof(int) * REG_BLK_BOOST_LEN);
	memcpy(reg_adp_ofset_20_copy, p->ve_reg_adp_ofset_20,
	       sizeof(int) * REG_ADP_OFSET_LEN);
	memcpy(reg_mono_protect_copy, p->ve_reg_mono_protect,
	       sizeof(int) * REG_MONO_PROT_LEN);
	memcpy(reg_trend_wht_expand_lut8_copy, p->ve_reg_trend_wht_expand_lut8,
	       sizeof(int) * TREND_WHT_EXP_LUT_LEN);
	memcpy(c_hist_gain_copy, p->ve_c_hist_gain,
	       sizeof(int) * C_HIST_GAIN_LEN);
	memcpy(s_hist_gain_copy, p->ve_s_hist_gain,
	       sizeof(int) * S_HIST_GAIN_LEN);

	if (ve_en != p->param[ve_dnlp_enable] ||
	    dnlp_sel != p->param[ve_dnlp_sel] ||
		dnlp_alg_param.dnlp_lowrange !=
			p->param[ve_dnlp_lowrange] ||
		dnlp_alg_param.dnlp_hghrange !=
			p->param[ve_dnlp_hghrange] ||
		dnlp_alg_param.dnlp_auto_rng !=
			p->param[ve_dnlp_auto_rng] ||
		dnlp_alg_param.dnlp_cuvbld_min !=
			p->param[ve_dnlp_cuvbld_min] ||
		dnlp_alg_param.dnlp_cuvbld_max !=
			p->param[ve_dnlp_cuvbld_max] ||
		dnlp_alg_param.dnlp_bbd_ratio_low !=
			p->param[ve_dnlp_bbd_ratio_low] ||
		dnlp_alg_param.dnlp_bbd_ratio_hig !=
			p->param[ve_dnlp_bbd_ratio_hig] ||
		dnlp_alg_param.dnlp_mvreflsh !=
			p->param[ve_dnlp_mvreflsh] ||
		dnlp_alg_param.dnlp_smhist_ck !=
			p->param[ve_dnlp_smhist_ck] ||
		dnlp_alg_param.dnlp_final_gain !=
			p->param[ve_dnlp_final_gain] ||
		dnlp_alg_param.dnlp_clahe_gain_neg !=
			p->param[ve_dnlp_clahe_gain_neg] ||
		dnlp_alg_param.dnlp_clahe_gain_pos !=
			p->param[ve_dnlp_clahe_gain_pos] ||
		dnlp_alg_param.dnlp_mtdbld_rate !=
			p->param[ve_dnlp_mtdbld_rate] ||
		dnlp_alg_param.dnlp_adpmtd_lbnd !=
			p->param[ve_dnlp_adpmtd_lbnd] ||
		dnlp_alg_param.dnlp_adpmtd_hbnd !=
			p->param[ve_dnlp_adpmtd_hbnd] ||
		dnlp_alg_param.dnlp_sbgnbnd !=
			p->param[ve_dnlp_sbgnbnd] ||
		dnlp_alg_param.dnlp_sendbnd !=
			p->param[ve_dnlp_sendbnd] ||
		dnlp_alg_param.dnlp_cliprate_v3 !=
			p->param[ve_dnlp_cliprate_v3] ||
		dnlp_alg_param.dnlp_cliprate_min !=
			p->param[ve_dnlp_cliprate_min] ||
		dnlp_alg_param.dnlp_adpcrat_lbnd !=
			p->param[ve_dnlp_adpcrat_lbnd] ||
		dnlp_alg_param.dnlp_adpcrat_hbnd !=
			p->param[ve_dnlp_adpcrat_hbnd] ||
		dnlp_alg_param.dnlp_clashbgn !=
			p->param[ve_dnlp_clashbgn] ||
		dnlp_alg_param.dnlp_clashend !=
			p->param[ve_dnlp_clashend] ||
		dnlp_alg_param.dnlp_blkext_rate !=
			p->param[ve_dnlp_blkext_rate] ||
		dnlp_alg_param.dnlp_whtext_rate !=
			p->param[ve_dnlp_whtext_rate] ||
		dnlp_alg_param.dnlp_blkext_ofst !=
			p->param[ve_dnlp_blkext_ofst] ||
		dnlp_alg_param.dnlp_whtext_ofst !=
			p->param[ve_dnlp_whtext_ofst] ||
		dnlp_alg_param.dnlp_bwext_div4x_min !=
			p->param[ve_dnlp_bwext_div4x_min] ||
		dnlp_alg_param.dnlp_irgnbgn !=
			p->param[ve_dnlp_irgnbgn] ||
		dnlp_alg_param.dnlp_irgnend !=
			p->param[ve_dnlp_irgnend] ||
		dnlp_alg_param.dnlp_blk_cctr !=
			p->param[ve_dnlp_blk_cctr] ||
		dnlp_alg_param.dnlp_brgt_ctrl !=
			p->param[ve_dnlp_brgt_ctrl] ||
		dnlp_alg_param.dnlp_brgt_range !=
			p->param[ve_dnlp_brgt_range] ||
		dnlp_alg_param.dnlp_brght_add !=
			p->param[ve_dnlp_brght_add] ||
		dnlp_alg_param.dnlp_brght_max !=
			p->param[ve_dnlp_brght_max] ||
		dnlp_alg_param.dnlp_satur_rat !=
			p->param[ve_dnlp_satur_rat] ||
		dnlp_alg_param.dnlp_satur_max !=
			p->param[ve_dnlp_satur_max] ||
		dnlp_alg_param.dnlp_scurv_low_th !=
			p->param[ve_dnlp_scurv_low_th] ||
		dnlp_alg_param.dnlp_scurv_mid1_th !=
			p->param[ve_dnlp_scurv_mid1_th] ||
		dnlp_alg_param.dnlp_scurv_mid2_th !=
			p->param[ve_dnlp_scurv_mid2_th] ||
		dnlp_alg_param.dnlp_scurv_hgh1_th !=
			p->param[ve_dnlp_scurv_hgh1_th] ||
		dnlp_alg_param.dnlp_scurv_hgh2_th !=
			p->param[ve_dnlp_scurv_hgh2_th] ||
		dnlp_alg_param.dnlp_mtdrate_adp_en !=
			p->param[ve_dnlp_mtdrate_adp_en] ||
		dnlp_alg_param.dnlp_clahe_method !=
			p->param[ve_dnlp_clahe_method] ||
		dnlp_alg_param.dnlp_ble_en !=
			p->param[ve_dnlp_ble_en] ||
		dnlp_alg_param.dnlp_norm !=
			p->param[ve_dnlp_norm] ||
		dnlp_alg_param.dnlp_scn_chg_th !=
			p->param[ve_dnlp_scn_chg_th] ||
		dnlp_alg_param.dnlp_iir_step_mux !=
			p->param[ve_dnlp_iir_step_mux] ||
		dnlp_alg_param.dnlp_single_bin_bw !=
			p->param[ve_dnlp_single_bin_bw] ||
		dnlp_alg_param.dnlp_single_bin_method !=
			p->param[ve_dnlp_single_bin_method] ||
		dnlp_alg_param.dnlp_reg_max_slop_1st !=
			p->param[ve_dnlp_reg_max_slop_1st] ||
		dnlp_alg_param.dnlp_reg_max_slop_mid !=
			p->param[ve_dnlp_reg_max_slop_mid] ||
		dnlp_alg_param.dnlp_reg_max_slop_fin !=
			p->param[ve_dnlp_reg_max_slop_fin] ||
		dnlp_alg_param.dnlp_reg_min_slop_1st !=
			p->param[ve_dnlp_reg_min_slop_1st] ||
		dnlp_alg_param.dnlp_reg_min_slop_mid !=
			p->param[ve_dnlp_reg_min_slop_mid] ||
		dnlp_alg_param.dnlp_reg_min_slop_fin !=
			p->param[ve_dnlp_reg_min_slop_fin] ||
		dnlp_alg_param.dnlp_reg_trend_wht_expand_mode !=
			p->param[ve_dnlp_reg_trend_wht_expand_mode] ||
		dnlp_alg_param.dnlp_reg_trend_blk_expand_mode !=
			p->param[ve_dnlp_reg_trend_blk_expand_mode] ||
		dnlp_alg_param.dnlp_ve_hist_cur_gain !=
			p->param[ve_dnlp_ve_hist_cur_gain] ||
		dnlp_alg_param.dnlp_ve_hist_cur_gain_precise !=
			p->param[ve_dnlp_ve_hist_cur_gain_precise] ||
		dnlp_alg_param.dnlp_reg_mono_binrang_st !=
			p->param[ve_dnlp_reg_mono_binrang_st] ||
		dnlp_alg_param.dnlp_reg_mono_binrang_ed !=
			p->param[ve_dnlp_reg_mono_binrang_ed] ||
		dnlp_alg_param.dnlp_c_hist_gain_base !=
			p->param[ve_dnlp_c_hist_gain_base] ||
		dnlp_alg_param.dnlp_s_hist_gain_base !=
			p->param[ve_dnlp_s_hist_gain_base] ||
		dnlp_alg_param.dnlp_mvreflsh_offset !=
			p->param[ve_dnlp_mvreflsh_offset] ||
		dnlp_alg_param.dnlp_luma_avg_th !=
			p->param[ve_dnlp_luma_avg_th]) {
		if (dnlp_insmod_ok)
			*menu_chg_en_copy = 1;
	} else {
		return;
	}

	ve_en = p->param[ve_dnlp_enable];
	dnlp_sel = p->param[ve_dnlp_sel];

	/* hist auto range parms */
	dnlp_alg_param.dnlp_alg_enable = p->param[ve_dnlp_enable];
	dnlp_alg_param.dnlp_lowrange = p->param[ve_dnlp_lowrange];
	dnlp_alg_param.dnlp_hghrange = p->param[ve_dnlp_hghrange];
	dnlp_alg_param.dnlp_auto_rng = p->param[ve_dnlp_auto_rng];

	dnlp_alg_param.dnlp_cuvbld_min = p->param[ve_dnlp_cuvbld_min];
	dnlp_alg_param.dnlp_cuvbld_max = p->param[ve_dnlp_cuvbld_max];

	/* histogram refine parms (remove bb affects) */
	dnlp_alg_param.dnlp_bbd_ratio_low = p->param[ve_dnlp_bbd_ratio_low];
	dnlp_alg_param.dnlp_bbd_ratio_hig = p->param[ve_dnlp_bbd_ratio_hig];

	/* calc iir-coef params */
	dnlp_alg_param.dnlp_mvreflsh = p->param[ve_dnlp_mvreflsh];
	dnlp_alg_param.dnlp_smhist_ck = p->param[ve_dnlp_smhist_ck];

	/* gains to delta of curves (for strength of the DNLP) */
	dnlp_alg_param.dnlp_final_gain = p->param[ve_dnlp_final_gain];
	dnlp_alg_param.dnlp_clahe_gain_neg = p->param[ve_dnlp_clahe_gain_neg];
	dnlp_alg_param.dnlp_clahe_gain_pos = p->param[ve_dnlp_clahe_gain_pos];

	/* coef of blending between gma_scurv and clahe curves */
	dnlp_alg_param.dnlp_mtdbld_rate =  p->param[ve_dnlp_mtdbld_rate];
	dnlp_alg_param.dnlp_adpmtd_lbnd = p->param[ve_dnlp_adpmtd_lbnd];
	dnlp_alg_param.dnlp_adpmtd_hbnd = p->param[ve_dnlp_adpmtd_hbnd];

	/* for gma_scurvs processing range */
	dnlp_alg_param.dnlp_sbgnbnd = p->param[ve_dnlp_sbgnbnd];
	dnlp_alg_param.dnlp_sendbnd = p->param[ve_dnlp_sendbnd];

	/* curve- clahe */
	dnlp_alg_param.dnlp_cliprate_v3 = p->param[ve_dnlp_cliprate_v3];
	dnlp_alg_param.dnlp_cliprate_min = p->param[ve_dnlp_cliprate_min];
	dnlp_alg_param.dnlp_adpcrat_lbnd = p->param[ve_dnlp_adpcrat_lbnd];
	dnlp_alg_param.dnlp_adpcrat_hbnd = p->param[ve_dnlp_adpcrat_hbnd];

	/* for clahe_curvs processing range */
	dnlp_alg_param.dnlp_clashbgn = p->param[ve_dnlp_clashbgn];
	dnlp_alg_param.dnlp_clashend = p->param[ve_dnlp_clashend];

	/* black white extension control params */
	dnlp_alg_param.dnlp_blkext_rate = p->param[ve_dnlp_blkext_rate];
	dnlp_alg_param.dnlp_whtext_rate = p->param[ve_dnlp_whtext_rate];
	dnlp_alg_param.dnlp_blkext_ofst = p->param[ve_dnlp_blkext_ofst];
	dnlp_alg_param.dnlp_whtext_ofst = p->param[ve_dnlp_whtext_ofst];
	dnlp_alg_param.dnlp_bwext_div4x_min = p->param[ve_dnlp_bwext_div4x_min];
	dnlp_alg_param.dnlp_irgnbgn = p->param[ve_dnlp_irgnbgn];
	dnlp_alg_param.dnlp_irgnend = p->param[ve_dnlp_irgnend];

	/* brightness_plus */
	dnlp_alg_param.dnlp_blk_cctr = p->param[ve_dnlp_blk_cctr];
	dnlp_alg_param.dnlp_brgt_ctrl = p->param[ve_dnlp_brgt_ctrl];
	dnlp_alg_param.dnlp_brgt_range = p->param[ve_dnlp_brgt_range];
	dnlp_alg_param.dnlp_brght_add = p->param[ve_dnlp_brght_add];
	dnlp_alg_param.dnlp_brght_max = p->param[ve_dnlp_brght_max];

	/* adaptive saturation compensations */
	dnlp_alg_param.dnlp_satur_rat = p->param[ve_dnlp_satur_rat];
	dnlp_alg_param.dnlp_satur_max = p->param[ve_dnlp_satur_max];
	dnlp_alg_param.dnlp_scurv_low_th = p->param[ve_dnlp_scurv_low_th];
	dnlp_alg_param.dnlp_scurv_mid1_th = p->param[ve_dnlp_scurv_mid1_th];
	dnlp_alg_param.dnlp_scurv_mid2_th = p->param[ve_dnlp_scurv_mid2_th];
	dnlp_alg_param.dnlp_scurv_hgh1_th = p->param[ve_dnlp_scurv_hgh1_th];
	dnlp_alg_param.dnlp_scurv_hgh2_th = p->param[ve_dnlp_scurv_hgh2_th];
	dnlp_alg_param.dnlp_mtdrate_adp_en =
		p->param[ve_dnlp_mtdrate_adp_en];
	/* TODO: ve_dnlp_set_saturtn = p->dnlp_set_saturtn; */
	/*new c curve param add for vlsi-kiteli*/
	dnlp_alg_param.dnlp_mtdrate_adp_en =
		p->param[ve_dnlp_mtdrate_adp_en];

	dnlp_alg_param.dnlp_clahe_method =
		p->param[ve_dnlp_clahe_method];
	dnlp_alg_param.dnlp_ble_en =
		p->param[ve_dnlp_ble_en];
	dnlp_alg_param.dnlp_norm =
		p->param[ve_dnlp_norm];
	dnlp_alg_param.dnlp_scn_chg_th =
		p->param[ve_dnlp_scn_chg_th];
	dnlp_alg_param.dnlp_iir_step_mux =
		p->param[ve_dnlp_iir_step_mux];
	dnlp_alg_param.dnlp_single_bin_bw =
		p->param[ve_dnlp_single_bin_bw];
	dnlp_alg_param.dnlp_single_bin_method =
		p->param[ve_dnlp_single_bin_method];
	dnlp_alg_param.dnlp_reg_max_slop_1st =
		p->param[ve_dnlp_reg_max_slop_1st];
	dnlp_alg_param.dnlp_reg_max_slop_mid =
		p->param[ve_dnlp_reg_max_slop_mid];
	dnlp_alg_param.dnlp_reg_max_slop_fin =
		p->param[ve_dnlp_reg_max_slop_fin];
	dnlp_alg_param.dnlp_reg_min_slop_1st =
		p->param[ve_dnlp_reg_min_slop_1st];
	dnlp_alg_param.dnlp_reg_min_slop_mid =
		p->param[ve_dnlp_reg_min_slop_mid];
	dnlp_alg_param.dnlp_reg_min_slop_fin =
		p->param[ve_dnlp_reg_min_slop_fin];
	dnlp_alg_param.dnlp_reg_trend_wht_expand_mode =
		p->param[ve_dnlp_reg_trend_wht_expand_mode];
	dnlp_alg_param.dnlp_reg_trend_blk_expand_mode =
		p->param[ve_dnlp_reg_trend_blk_expand_mode];
	dnlp_alg_param.dnlp_ve_hist_cur_gain =
		p->param[ve_dnlp_ve_hist_cur_gain];
	dnlp_alg_param.dnlp_ve_hist_cur_gain_precise =
		p->param[ve_dnlp_ve_hist_cur_gain_precise];
	dnlp_alg_param.dnlp_reg_mono_binrang_st =
		p->param[ve_dnlp_reg_mono_binrang_st];
	dnlp_alg_param.dnlp_reg_mono_binrang_ed =
		p->param[ve_dnlp_reg_mono_binrang_ed];
	dnlp_alg_param.dnlp_c_hist_gain_base =
		p->param[ve_dnlp_c_hist_gain_base];
	dnlp_alg_param.dnlp_s_hist_gain_base =
		p->param[ve_dnlp_s_hist_gain_base];
	dnlp_alg_param.dnlp_mvreflsh_offset =
		p->param[ve_dnlp_mvreflsh_offset];
	dnlp_alg_param.dnlp_luma_avg_th =
		p->param[ve_dnlp_luma_avg_th];

	if (dnlp_insmod_ok == 0)
		return;

	if (dnlp_dbg_print & 4)
		pr_info("ve_en = %d, dnlp_sel = %d, dnlp_alg_enable = %d\n",
			ve_en, dnlp_sel, dnlp_alg_param.dnlp_alg_enable);

	dnlp_dbg_node_copy();

	if (ve_en != ve_en_pre) {
		ve_en_pre = ve_en;
		ve_dnlp_ctrl_vsync(ve_en);
	}
}

void ble_whe_param_update(struct ve_ble_whe_param_s *p)
{
	ble_whe_param->blk_adj_en = p->blk_adj_en;
	ble_whe_param->blk_end = p->blk_end;
	ble_whe_param->blk_slp = p->blk_slp;
	ble_whe_param->brt_adj_en = p->brt_adj_en;
	ble_whe_param->brt_slp = p->brt_slp;
	ble_whe_param->brt_start = p->brt_start;

	if (dnlp_dbg_print & 4) {
		pr_info("blk_adj_en = %d\n", ble_whe_param->blk_adj_en);
		pr_info("blk_end = %d\n", ble_whe_param->blk_end);
		pr_info("blk_slp = %d\n", ble_whe_param->blk_slp);
		pr_info("brt_adj_en = %d\n", ble_whe_param->brt_adj_en);
		pr_info("brt_slp = %d\n", ble_whe_param->brt_slp);
		pr_info("brt_start = %d\n", ble_whe_param->brt_start);
	}
}

