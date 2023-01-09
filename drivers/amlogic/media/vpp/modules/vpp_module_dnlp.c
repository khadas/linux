// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "../vpp_common.h"
#include "../../enhancement/amvecm/dnlp_algorithm/dnlp_alg.h"
#include "vpp_module_dnlp.h"

#define DNLP_LPF_SIZE 64
#define DNLP_REG_SIZE 32

enum _dnlp_mode_e {
	EN_MODE_DNLP_VPP = 0,
	EN_MODE_DNLP_SR0,
	EN_MODE_DNLP_SR1,
	EN_MODE_DNLP_MAX,
};

struct _dnlp_bit_cfg_s {
	struct _bit_s bit_dnlp_ctrl_en;
};

struct _dnlp_reg_cfg_s {
	unsigned char page;
	unsigned char reg_dnlp_ctrl;
	unsigned char reg_dnlp_p00;
};

struct _dnlp_reg_cal_s {
	int len;
	int rate;
	int offset;
	int shift_rt;
	int shift_lf;
	int range_up;
};

/*Default table from T3*/
static struct _dnlp_reg_cfg_s dnlp_reg_cfg[EN_MODE_DNLP_MAX] = {
	{0x1d, 0xa1, 0x81},
	{0x50, 0x45, 0x90},
	{0x52, 0x45, 0x90}
};

static struct _dnlp_bit_cfg_s dnlp_bit_cfg = {
	{0, 1}
};

static unsigned int dnlp_ctrl_pnt_max[EN_MODE_DNLP_MAX] = {
	16,
	32,
	32
};

static enum _dnlp_mode_e cur_mode;
static struct _dnlp_reg_cal_s reg_cal;

static bool dnlp_bypass;
static int dnlp_rt;
static int dnlp_lpf[DNLP_LPF_SIZE] = {0};
static int dnlp_reg[DNLP_REG_SIZE] = {0};
static int dnlp_reg_def_16p[16] = {
	0x0b070400, 0x1915120e, 0x2723201c, 0x35312e2a,
	0x47423d38, 0x5b56514c, 0x6f6a6560, 0x837e7974,
	0x97928d88, 0xaba6a19c, 0xbfbab5b0, 0xcfccc9c4,
	0xdad7d5d2, 0xe6e3e0dd, 0xf2efece9, 0xfdfaf7f4
};

/*Algorithm related*/
static int pre_sat;
static bool dnlp_alg_insmod_ok;
static struct dnlp_alg_s *pdnlp_alg_function;
static struct dnlp_alg_input_param_s *pdnlp_alg_input;
static struct dnlp_alg_output_param_s *pdnlp_alg_output;
static struct dnlp_dbg_ro_param_s *pdnlp_dbg_ro_param;
static struct dnlp_dbg_rw_param_s *pdnlp_dbg_rw_param;
static struct param_for_dnlp_s *pdnlp_alg_node_param;
static struct dnlp_dbg_print_s *pdnlp_dbg_printk;

/*For ai pq*/
static struct dnlp_ai_pq_param_s dnlp_ai_pq_offset;
static struct dnlp_ai_pq_param_s dnlp_ai_pq_base;

/*For debug*/
struct dnlp_dbg_parse_cmd_s dnlp_dbg_parse_cmd[] = {
	{"alg_enable", NULL, 1},
	{"respond", NULL, 1},
	{"sel", NULL, 1},
	{"respond_flag", NULL, 1},
	{"smhist_ck", NULL, 1},
	{"mvreflsh", NULL, 1},
	{"pavg_btsft", NULL, 1},
	{"dbg_i2r", NULL, 1},
	{"cuvbld_min", NULL, 1},
	{"cuvbld_max", NULL, 1},
	{"schg_sft", NULL, 1},
	{"bbd_ratio_low", NULL, 1},
	{"bbd_ratio_hig", NULL, 1},
	{"limit_rng", NULL, 1},
	{"range_det", NULL, 1},
	{"blk_cctr", NULL, 1},
	{"brgt_ctrl", NULL, 1},
	{"brgt_range", NULL, 1},
	{"brght_add", NULL, 1},
	{"brght_max", NULL, 1},
	{"dbg_adjavg", NULL, 1},
	{"auto_rng", NULL, 1},
	{"lowrange", NULL, 1},
	{"hghrange", NULL, 1},
	{"satur_rat", NULL, 1},
	{"satur_max", NULL, 1},
	{"set_saturtn", NULL, 1},
	{"sbgnbnd", NULL, 1},
	{"sendbnd", NULL, 1},
	/*{"clashBgn", NULL, 1},*/
	/*{"clashEnd", NULL, 1},*/
	{"var_th", NULL, 1},
	{"clahe_gain_neg", NULL, 1},
	{"clahe_gain_pos", NULL, 1},
	{"clahe_gain_delta", NULL, 1},
	{"mtdbld_rate", NULL, 1},
	{"adpmtd_lbnd", NULL, 1},
	{"adpmtd_hbnd", NULL, 1},
	{"blkext_ofst", NULL, 1},
	{"whtext_ofst", NULL, 1},
	{"blkext_rate", NULL, 1},
	{"whtext_rate", NULL, 1},
	{"bwext_div4x_min", NULL, 1},
	/*{"iRgnBgn", NULL, 1},*/
	/*{"iRgnEnd", NULL, 1},*/
	{"dbg_map", NULL, 1},
	{"final_gain", NULL, 1},
	{"cliprate_v3", NULL, 1},
	{"cliprate_min", NULL, 1},
	{"adpcrat_lbnd", NULL, 1},
	{"adpcrat_hbnd", NULL, 1},
	{"scurv_low_th", NULL, 1},
	{"scurv_mid1_th", NULL, 1},
	{"scurv_mid2_th", NULL, 1},
	{"scurv_hgh1_th", NULL, 1},
	{"scurv_hgh2_th", NULL, 1},
	{"mtdrate_adp_en", NULL, 1},
	{"clahe_method", NULL, 1},
	{"ble_en", NULL, 1},
	{"norm", NULL, 1},
	{"scn_chg_th", NULL, 1},
	{"step_th", NULL, 1},
	{"iir_step_mux", NULL, 1},
	{"single_bin_bw", NULL, 1},
	{"single_bin_method", NULL, 1},
	{"reg_max_slop_1st", NULL, 1},
	{"reg_max_slop_mid", NULL, 1},
	{"reg_max_slop_fin", NULL, 1},
	{"reg_min_slop_1st", NULL, 1},
	{"reg_min_slop_mid", NULL, 1},
	{"reg_min_slop_fin", NULL, 1},
	{"reg_trend_wht_expand_mode", NULL, 1},
	{"reg_trend_blk_expand_mode", NULL, 1},
	{"ve_hist_cur_gain", NULL, 1},
	{"ve_hist_cur_gain_precise", NULL, 1},
	{"reg_mono_binrang_st", NULL, 1},
	{"reg_mono_binrang_ed", NULL, 1},
	{"c_hist_gain_base", NULL, 1},
	{"s_hist_gain_base", NULL, 1},
	{"mvreflsh_offset", NULL, 1},
	{"luma_avg_th", NULL, 1},
	{"", NULL, 0}
};

struct dnlp_dbg_parse_cmd_s dnlp_dbg_parse_ro_cmd[] = {
	{"luma_avg4", NULL, 1},
	{"var_d8", NULL, 1},
	{"scurv_gain", NULL, 1},
	{"blk_wht_ext0", NULL, 1},
	{"blk_wht_ext1", NULL, 1},
	{"dnlp_brightness", NULL, 1},
	{"", NULL, 0}
};

struct dnlp_dbg_parse_cmd_s dnlp_dbg_parse_cv_cmd[] = {
	{"scurv_low", NULL, VPP_DNLP_SCURV_LEN},
	{"scurv_mid1", NULL, VPP_DNLP_SCURV_LEN},
	{"scurv_mid2", NULL, VPP_DNLP_SCURV_LEN},
	{"scurv_hgh1", NULL, VPP_DNLP_SCURV_LEN},
	{"scurv_hgh2", NULL, VPP_DNLP_SCURV_LEN},
	{"gain_var_lut49", NULL, VPP_DNLP_GAIN_VAR_LUT_LEN},
	{"c_hist_gain", NULL, VPP_DNLP_HIST_GAIN_LEN},
	{"s_hist_gain", NULL, VPP_DNLP_HIST_GAIN_LEN},
	{"wext_gain", NULL, VPP_DNLP_WEXT_GAIN_LEN},
	{"adp_thrd", NULL, VPP_DNLP_ADP_THRD_LEN},
	{"reg_blk_boost_12", NULL, VPP_DNLP_REG_BLK_BOOST_LEN},
	{"reg_adp_ofset_20", NULL, VPP_DNLP_REG_ADP_OFSET_LEN},
	{"reg_mono_protect", NULL, VPP_DNLP_REG_MONO_PROT_LEN},
	{"reg_trend_wht_expand_lut8", NULL, VPP_DNLP_TREND_WHT_EXP_LUT_LEN},
	{"ve_dnlp_tgt", NULL, VPP_DNLP_SCURV_LEN},
	{"ve_dnlp_tgt_10b", NULL, VPP_DNLP_SCURV_LEN},
	/*{"GmScurve", NULL, VPP_DNLP_SCURV_LEN},*/
	{"clash_curve", NULL, VPP_DNLP_SCURV_LEN},
	{"clsh_scvbld", NULL, VPP_DNLP_SCURV_LEN},
	{"blkwht_ebld", NULL, VPP_DNLP_SCURV_LEN},
	{"", NULL, 0}
};

/*Internal functions*/
static void _set_dnlp_ctrl(enum _dnlp_mode_e mode, int val,
	unsigned char start, unsigned char len)
{
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (mode == EN_MODE_DNLP_MAX)
		return;

	addr = ADDR_PARAM(dnlp_reg_cfg[mode].page,
		dnlp_reg_cfg[mode].reg_dnlp_ctrl);

	WRITE_VPP_REG_BITS_BY_MODE(io_mode, addr, val, start, len);
}

static void _set_dnlp_data(enum _dnlp_mode_e mode, int *pdata)
{
	int i = 0;
	unsigned int val = 0;
	unsigned int addr = 0;
	enum io_mode_e io_mode = EN_MODE_DIR;

	if (mode == EN_MODE_DNLP_MAX)
		return;

	addr = ADDR_PARAM(dnlp_reg_cfg[mode].page,
		dnlp_reg_cfg[mode].reg_dnlp_p00);

	for (i = 0; i < dnlp_ctrl_pnt_max[mode]; i++) {
		val = pdata[i];
		WRITE_VPP_REG_BY_MODE(io_mode, addr + i, val);
	}
}

static void _dump_dnlp_reg_info(void)
{
	int i = 0;

	PR_DRV("dnlp_reg_cfg info:\n");
	for (i = 0; i < EN_MODE_DNLP_MAX; i++) {
		PR_DRV("dnlp type = %d\n", i);
		PR_DRV("page = %x\n", dnlp_reg_cfg[i].page);
		PR_DRV("reg_dnlp_ctrl = %x\n", dnlp_reg_cfg[i].reg_dnlp_ctrl);
		PR_DRV("reg_dnlp_p00 = %x\n", dnlp_reg_cfg[i].reg_dnlp_p00);
	}
}

static void _dump_dnlp_data_info(void)
{
	int i = 0;

	PR_DRV("cur_mode = %d\n", cur_mode);
	PR_DRV("dnlp_bypass = %d\n", dnlp_bypass);
	PR_DRV("dnlp_rt = %d\n", dnlp_rt);

	PR_DRV("dnlp_lpf data info:\n");
	for (i = 0; i < DNLP_LPF_SIZE; i++) {
		PR_DRV("%d\t", dnlp_lpf[i]);
		if (i % 8 == 0)
			PR_DRV("\n");
	}

	PR_DRV("dnlp_reg data info:\n");
	for (i = 0; i < DNLP_REG_SIZE; i++) {
		PR_DRV("%d\t", dnlp_reg[i]);
		if (i % 8 == 0)
			PR_DRV("\n");
	}
}

static void _calculate_dnlp_tgtx(int hist_luma_sum,
	unsigned short *phist_data)
{
	int i = 0;
	int same_bin_num = 0;
	unsigned int raw_hist_sum = 0;
	unsigned int *ptmp0;
	unsigned int *ptmp1;

	if (!dnlp_alg_insmod_ok || !pdnlp_alg_function)
		return;

	/*calculate iir-coef params*/
	if (pdnlp_alg_node_param->dnlp_mvreflsh < 1)
		pdnlp_alg_node_param->dnlp_mvreflsh = 1;

	*pdnlp_alg_input->RBASE = 1 << pdnlp_alg_node_param->dnlp_mvreflsh;

	/*parameters refresh*/
	pdnlp_alg_function->dnlp3_param_refresh();

	/*load histogram*/
	*pdnlp_alg_input->ve_dnlp_luma_sum = hist_luma_sum;

	ptmp0 = pdnlp_alg_input->pre_0_gamma;
	ptmp1 = pdnlp_alg_input->pre_1_gamma;

	for (i = 0; i < VPP_HIST_BIN_COUNT; i++) {
		/*histogram stored for one frame delay*/
		ptmp1[i] = ptmp0[i];

		ptmp0[i] = (unsigned int)phist_data[i];
		raw_hist_sum += (unsigned int)phist_data[i];

		/*counter same histogram*/
		if (ptmp1[i] == ptmp0[i])
			same_bin_num++;
	}

	/*all same histogram as last frame, freeze DNLP*/
	if (same_bin_num == VPP_HIST_BIN_COUNT &&
		pdnlp_alg_node_param->dnlp_smhist_ck &&
		!pdnlp_alg_node_param->dnlp_respond_flag)
		return;

	pdnlp_alg_function->dnlp_algorithm_main(raw_hist_sum);
}

static void _calculate_dnlp_lpf(char *pdata_in, int *pdata_out)
{
	int i = 0;
	int tmp = 0;

	for (i = 0; i < DNLP_LPF_SIZE; i++) {
		tmp = pdata_out[i];
		pdata_out[i] = tmp - (tmp >> dnlp_rt) + pdata_in[i];
	}
}

static void _calculate_dnlp_reg(int *pdata_in, int *pdata_out)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int val = 0;

	for (i = 0; i < reg_cal.len; i++) {
		val = 0;
		k = i * reg_cal.rate;
		for (j = 0; j < reg_cal.rate; j++) {
			val = pdata_in[k + j] + reg_cal.offset;
			val = val << reg_cal.shift_lf;
			val = val >> reg_cal.shift_rt;
			val = vpp_check_range(val, 0, reg_cal.range_up);
			val |= val << (j * (sizeof(int) / reg_cal.rate));
		}

		pdata_out[i] = val;
	}
}

void _calculate_dnlp_sat_compensation(bool *psat_comp, int *psat_val)
{
	bool do_comp = false;
	int val = 0;
	int t0 = 0;
	int t1 = 0;
	int tmp0 = 0;
	int tmp1 = 0;
	int i = 0;
	int tmp = 0;
	int satur_rat = 0;
	int satur_max = 0;
	int set_saturtn = 0;

	if (!dnlp_alg_insmod_ok || !pdnlp_alg_function)
		return;

	for (i = 1; i < 64; i++) {
		tmp = pdnlp_alg_output->ve_dnlp_tgt[i];
		if (tmp > 4 * i) {
			t0 += (tmp - 4 * i) * (65 - i);
			t1 += (65 - i);
		}
	}

	satur_rat = pdnlp_alg_node_param->dnlp_satur_rat;
	satur_max = pdnlp_alg_node_param->dnlp_satur_max;
	set_saturtn = pdnlp_alg_node_param->dnlp_set_saturtn;

	/*pr_vpp(PR_DEBUG_DNLP, "[%s] t0 = %d, t1 = %d\n", __func__, t0, t1);*/
	/*pr_vpp(PR_DEBUG_DNLP,*/
	/*	"[%s] satur_rat = %d, satur_max = %d, set_saturtn = %d\n",*/
	/*	__func__, satur_rat, satur_max, set_saturtn);*/

	tmp = (t0 * satur_rat + (t1 >> 1)) / (t1 + 1);
	tmp0 = (tmp + 4) >> 3;

	tmp = satur_max << 3;
	tmp1 = MIN(tmp0, tmp);

	if (set_saturtn == 0) {
		if (tmp1 != pre_sat) {
			do_comp = true;
			val = tmp1 + 512;
			pre_sat = tmp1;
		} else {
			do_comp = false;
		}
	} else {
		if (pre_sat != set_saturtn) {
			do_comp = true;
			pre_sat = set_saturtn;
			if (set_saturtn < 512)
				val = set_saturtn + 512;
			else
				val = set_saturtn;
		} else {
			do_comp = false;
		}
	}

	*psat_comp = do_comp;
	*psat_val = val;
}

static void _init_dnlp_data(int *pluma_sum,
	char *ptgt_data, int *ptgt_data_10b, int *plpf_data)
{
	int i = 0;

	/*clear historic luma sum*/
	*pluma_sum = 0;

	/*init tgt & lpf*/
	for (i = 0; i < DNLP_LPF_SIZE; i++) {
		ptgt_data[i] = i << 2;
		ptgt_data_10b[i] = i << 4;
		plpf_data[i] = ptgt_data[i] << dnlp_rt;
	}
}

static void _dnlp_debug_init(void)
{
	int i = 0;

	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_alg_enable;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_respond;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_sel;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_respond_flag;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_smhist_ck;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_mvreflsh;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_pavg_btsft;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_dbg_i2r;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_cuvbld_min;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_cuvbld_max;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_schg_sft;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_bbd_ratio_low;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_bbd_ratio_hig;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_limit_rng;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_range_det;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_blk_cctr;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_brgt_ctrl;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_brgt_range;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_brght_add;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_brght_max;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_dbg_adjavg;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_auto_rng;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_lowrange;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_hghrange;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_hghrange;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_satur_rat;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_satur_max;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_set_saturtn;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_sbgnbnd;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_sendbnd;
	/*dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_clashBgn;*/
	/*dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_clashEnd;*/
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_var_th;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_clahe_gain_neg;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_clahe_gain_pos;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_clahe_gain_delta;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_mtdbld_rate;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_adpmtd_lbnd;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_adpmtd_hbnd;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_blkext_ofst;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_whtext_ofst;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_blkext_rate;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_whtext_rate;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_bwext_div4x_min;
	/*dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_iRgnBgn;*/
	/*dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_iRgnEnd;*/
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_dbg_map;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_final_gain;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_cliprate_v3;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_cliprate_min;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_adpcrat_lbnd;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_adpcrat_hbnd;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_scurv_low_th;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_scurv_mid1_th;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_scurv_mid2_th;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_scurv_hgh1_th;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_scurv_hgh2_th;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_mtdrate_adp_en;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_clahe_method;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_ble_en;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_norm;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_scn_chg_th;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_step_th;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_iir_step_mux;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_single_bin_bw;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_single_bin_method;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_reg_max_slop_1st;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_reg_max_slop_mid;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_reg_max_slop_fin;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_reg_min_slop_1st;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_reg_min_slop_mid;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_reg_min_slop_fin;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_reg_trend_wht_expand_mode;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_reg_trend_blk_expand_mode;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_ve_hist_cur_gain;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_ve_hist_cur_gain_precise;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_reg_mono_binrang_st;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_reg_mono_binrang_ed;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_c_hist_gain_base;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_s_hist_gain_base;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_mvreflsh_offset;
	dnlp_dbg_parse_cmd[i++].value = &pdnlp_alg_node_param->dnlp_luma_avg_th;

	i = 0;
	dnlp_dbg_parse_ro_cmd[i++].value = pdnlp_dbg_ro_param->ro_luma_avg4;
	dnlp_dbg_parse_ro_cmd[i++].value = pdnlp_dbg_ro_param->ro_var_d8;
	dnlp_dbg_parse_ro_cmd[i++].value = pdnlp_dbg_ro_param->ro_scurv_gain;
	dnlp_dbg_parse_ro_cmd[i++].value = pdnlp_dbg_ro_param->ro_blk_wht_ext0;
	dnlp_dbg_parse_ro_cmd[i++].value = pdnlp_dbg_ro_param->ro_blk_wht_ext1;
	dnlp_dbg_parse_ro_cmd[i++].value = pdnlp_dbg_ro_param->ro_dnlp_brightness;

	i = 0;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->dnlp_scurv_low;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->dnlp_scurv_mid1;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->dnlp_scurv_mid2;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->dnlp_scurv_hgh1;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->dnlp_scurv_hgh2;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->gain_var_lut49;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->c_hist_gain;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->s_hist_gain;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->wext_gain;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->adp_thrd;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->reg_blk_boost_12;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->reg_adp_ofset_20;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->reg_mono_protect;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_rw_param->reg_trend_wht_expand_lut8;
	dnlp_dbg_parse_cv_cmd[i++].value = NULL;/*pdnlp_alg_output->ve_dnlp_tgt;*/
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_alg_output->ve_dnlp_tgt_10b;
	/*dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_ro_param->GmScurve;*/
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_ro_param->clash_curve;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_ro_param->clsh_scvbld;
	dnlp_dbg_parse_cv_cmd[i++].value = pdnlp_dbg_ro_param->blkwht_ebld;
}

static void _dnlp_algorithm_init(void)
{
	/*pdnlp_alg_function = dnlp_alg_init(&pdnlp_alg_function);*/
	pdnlp_alg_function = dnlp_alg_function;
	if (pdnlp_alg_function) {
		pdnlp_alg_function->dnlp_para_set(&pdnlp_alg_output, &pdnlp_alg_input,
			&pdnlp_dbg_rw_param, &pdnlp_dbg_ro_param,
			&pdnlp_alg_node_param, &pdnlp_dbg_printk);

		pdnlp_alg_node_param->dnlp_alg_enable = 0;
		pdnlp_alg_node_param->dnlp_respond = 0;
		pdnlp_alg_node_param->dnlp_sel = 2;
		pdnlp_alg_node_param->dnlp_respond_flag = 0;
		pdnlp_alg_node_param->dnlp_smhist_ck = 0;
		pdnlp_alg_node_param->dnlp_mvreflsh = 6;
		pdnlp_alg_node_param->dnlp_pavg_btsft = 5;
		pdnlp_alg_node_param->dnlp_dbg_i2r = 255;
		pdnlp_alg_node_param->dnlp_cuvbld_min = 3;
		pdnlp_alg_node_param->dnlp_cuvbld_max = 17;
		pdnlp_alg_node_param->dnlp_schg_sft = 1;
		pdnlp_alg_node_param->dnlp_bbd_ratio_low = 16;
		pdnlp_alg_node_param->dnlp_bbd_ratio_hig = 128;
		pdnlp_alg_node_param->dnlp_limit_rng = 1;
		pdnlp_alg_node_param->dnlp_range_det = 0;
		pdnlp_alg_node_param->dnlp_blk_cctr = 8;
		pdnlp_alg_node_param->dnlp_brgt_ctrl = 48;
		pdnlp_alg_node_param->dnlp_brgt_range = 16;
		pdnlp_alg_node_param->dnlp_brght_add = 32;
		pdnlp_alg_node_param->dnlp_brght_max = 0;
		pdnlp_alg_node_param->dnlp_dbg_adjavg = 0;
		pdnlp_alg_node_param->dnlp_auto_rng = 0;
		pdnlp_alg_node_param->dnlp_lowrange = 18;
		pdnlp_alg_node_param->dnlp_hghrange = 18;
		pdnlp_alg_node_param->dnlp_satur_rat = 30;
		pdnlp_alg_node_param->dnlp_satur_max = 0;
		pdnlp_alg_node_param->dnlp_set_saturtn = 0;
		pdnlp_alg_node_param->dnlp_sbgnbnd = 4;
		pdnlp_alg_node_param->dnlp_sendbnd = 4;
		/*pdnlp_alg_node_param->dnlp_clashBgn = 4;*/
		/*pdnlp_alg_node_param->dnlp_clashEnd = 59;*/
		pdnlp_alg_node_param->dnlp_var_th = 16;
		pdnlp_alg_node_param->dnlp_clahe_gain_neg = 120;
		pdnlp_alg_node_param->dnlp_clahe_gain_pos = 24;
		pdnlp_alg_node_param->dnlp_clahe_gain_delta = 32;
		pdnlp_alg_node_param->dnlp_mtdbld_rate = 40;
		pdnlp_alg_node_param->dnlp_adpmtd_lbnd = 19;
		pdnlp_alg_node_param->dnlp_adpmtd_hbnd = 20;
		pdnlp_alg_node_param->dnlp_blkext_ofst = 2;
		pdnlp_alg_node_param->dnlp_whtext_ofst = 1;
		pdnlp_alg_node_param->dnlp_blkext_rate = 32;
		pdnlp_alg_node_param->dnlp_whtext_rate = 16;
		pdnlp_alg_node_param->dnlp_bwext_div4x_min = 16;
		/*pdnlp_alg_node_param->dnlp_iRgnBgn = 0;*/
		/*pdnlp_alg_node_param->dnlp_iRgnEnd = 64;*/
		pdnlp_alg_node_param->dnlp_dbg_map = 0;
		pdnlp_alg_node_param->dnlp_final_gain = 8;
		pdnlp_alg_node_param->dnlp_cliprate_v3 = 36;
		pdnlp_alg_node_param->dnlp_cliprate_min = 19;
		pdnlp_alg_node_param->dnlp_adpcrat_lbnd = 10;
		pdnlp_alg_node_param->dnlp_adpcrat_hbnd = 20;
		pdnlp_alg_node_param->dnlp_scurv_low_th = 32;
		pdnlp_alg_node_param->dnlp_scurv_mid1_th = 48;
		pdnlp_alg_node_param->dnlp_scurv_mid2_th = 112;
		pdnlp_alg_node_param->dnlp_scurv_hgh1_th = 176;
		pdnlp_alg_node_param->dnlp_scurv_hgh2_th = 240;
		pdnlp_alg_node_param->dnlp_mtdrate_adp_en = 1;
		pdnlp_alg_node_param->dnlp_clahe_method = 1;
		pdnlp_alg_node_param->dnlp_ble_en = 1;
		pdnlp_alg_node_param->dnlp_norm = 10;
		pdnlp_alg_node_param->dnlp_scn_chg_th = 48;
		pdnlp_alg_node_param->dnlp_step_th = 1;
		pdnlp_alg_node_param->dnlp_iir_step_mux = 1;
		pdnlp_alg_node_param->dnlp_single_bin_bw = 2;
		pdnlp_alg_node_param->dnlp_single_bin_method = 1;
		pdnlp_alg_node_param->dnlp_reg_max_slop_1st = 614;
		pdnlp_alg_node_param->dnlp_reg_max_slop_mid = 400;
		pdnlp_alg_node_param->dnlp_reg_max_slop_fin = 614;
		pdnlp_alg_node_param->dnlp_reg_min_slop_1st = 77;
		pdnlp_alg_node_param->dnlp_reg_min_slop_mid = 144;
		pdnlp_alg_node_param->dnlp_reg_min_slop_fin = 77;
		pdnlp_alg_node_param->dnlp_reg_trend_wht_expand_mode = 2;
		pdnlp_alg_node_param->dnlp_reg_trend_blk_expand_mode = 2;
		pdnlp_alg_node_param->dnlp_ve_hist_cur_gain = 8;
		pdnlp_alg_node_param->dnlp_ve_hist_cur_gain_precise = 8;
		pdnlp_alg_node_param->dnlp_reg_mono_binrang_st = 7;
		pdnlp_alg_node_param->dnlp_reg_mono_binrang_ed = 26;
		pdnlp_alg_node_param->dnlp_c_hist_gain_base = 128;
		pdnlp_alg_node_param->dnlp_s_hist_gain_base = 128;
		pdnlp_alg_node_param->dnlp_mvreflsh_offset = 0;
		pdnlp_alg_node_param->dnlp_luma_avg_th = 50;

		dnlp_alg_insmod_ok = true;
		PR_DRV("%s: is OK\n", __func__);

		_dnlp_debug_init();
	} else {
		PR_DRV("%s: pdnlp_alg_function is NULL\n", __func__);
	}
}

/*External functions*/
int vpp_module_dnlp_init(struct vpp_dev_s *pdev)
{
	enum vpp_chip_type_e chip_id;
	enum dnlp_hw_e dnlp_hw;

	dnlp_bypass = false;
	dnlp_alg_insmod_ok = false;

	dnlp_rt = 0;
	reg_cal.len = 16;
	reg_cal.rate = 4;
	reg_cal.offset = dnlp_rt ? (1 << (dnlp_rt - 1)) : 0;
	reg_cal.shift_rt = dnlp_rt;
	reg_cal.shift_lf = 0;
	reg_cal.range_up = 255;

	chip_id = pdev->pm_data->chip_id;
	dnlp_hw = pdev->vpp_cfg_data.dnlp_hw;

	if (dnlp_hw != VPP_DNLP) {
		if (chip_id < CHIP_TL1) {
			cur_mode = EN_MODE_DNLP_SR0;
			dnlp_ctrl_pnt_max[cur_mode] = 16;
		} else {
			if (chip_id == CHIP_T7) {
				cur_mode = EN_MODE_DNLP_SR0;
				dnlp_reg_cfg[cur_mode].page = 0x3e;
			} else {
				cur_mode = EN_MODE_DNLP_SR1;
				dnlp_reg_cfg[cur_mode].page = 0x3f;
			}

			dnlp_ctrl_pnt_max[cur_mode] = 32;

			reg_cal.len = 32;
			reg_cal.rate = 2;
			reg_cal.offset = 0;
			reg_cal.shift_rt = 0;
			reg_cal.shift_lf = 2;
			reg_cal.range_up = 1023;
		}
	} else {
		cur_mode = EN_MODE_DNLP_VPP;
	}

	pre_sat = 0;

	memset(&dnlp_ai_pq_offset, 0, sizeof(dnlp_ai_pq_offset));
	dnlp_ai_pq_base.final_gain = 8;

	return 0;
}

void vpp_module_dnlp_en(bool enable)
{
	if (!enable)
		dnlp_bypass = true;
	else
		dnlp_bypass = false;

	pr_vpp(PR_DEBUG_DNLP, "[%s] enable = %d\n", __func__, enable);

	_set_dnlp_ctrl(cur_mode, enable,
		dnlp_bit_cfg.bit_dnlp_ctrl_en.start,
		dnlp_bit_cfg.bit_dnlp_ctrl_en.len);
}

void vpp_module_dnlp_on_vs(int hist_luma_sum,
	unsigned short *phist_data)
{
	if (!dnlp_alg_insmod_ok)
		_dnlp_algorithm_init();

	if (!dnlp_alg_insmod_ok || dnlp_bypass || !phist_data)
		return;

	_calculate_dnlp_tgtx(hist_luma_sum, phist_data);
	_calculate_dnlp_lpf(pdnlp_alg_output->ve_dnlp_tgt, &dnlp_lpf[0]);
	_calculate_dnlp_reg(&dnlp_lpf[0], &dnlp_reg[0]);
	_set_dnlp_data(cur_mode, &dnlp_reg[0]);
}

void vpp_module_dnlp_get_sat_compensation(bool *psat_comp,
	int *psat_val)
{
	if (!psat_comp || !psat_val)
		return;

	_calculate_dnlp_sat_compensation(psat_comp, psat_val);
}

void vpp_module_dnlp_set_default(void)
{
	if (!dnlp_alg_insmod_ok || dnlp_bypass)
		return;

	pr_vpp(PR_DEBUG_DNLP, "[%s] set dnlp_reg_def_16p data\n", __func__);

	_set_dnlp_data(EN_MODE_DNLP_VPP, &dnlp_reg_def_16p[0]);
}

void vpp_module_dnlp_set_param(struct vpp_dnlp_curve_param_s *pdata)
{
	if (!dnlp_alg_insmod_ok || dnlp_bypass || !pdata)
		return;

	pr_vpp(PR_DEBUG_DNLP, "[%s] set data\n", __func__);

	/*load static curve*/
	memcpy(pdnlp_dbg_rw_param->dnlp_scurv_low, pdata->dnlp_scurv_low,
		sizeof(int) * VPP_DNLP_SCURV_LEN);
	memcpy(pdnlp_dbg_rw_param->dnlp_scurv_mid1, pdata->dnlp_scurv_mid1,
		sizeof(int) * VPP_DNLP_SCURV_LEN);
	memcpy(pdnlp_dbg_rw_param->dnlp_scurv_mid2, pdata->dnlp_scurv_mid2,
		sizeof(int) * VPP_DNLP_SCURV_LEN);
	memcpy(pdnlp_dbg_rw_param->dnlp_scurv_hgh1, pdata->dnlp_scurv_hgh1,
		sizeof(int) * VPP_DNLP_SCURV_LEN);
	memcpy(pdnlp_dbg_rw_param->dnlp_scurv_hgh2, pdata->dnlp_scurv_hgh2,
		sizeof(int) * VPP_DNLP_SCURV_LEN);
	/*load gain var*/
	memcpy(pdnlp_dbg_rw_param->gain_var_lut49, pdata->gain_var_lut49,
		sizeof(int) * VPP_DNLP_GAIN_VAR_LUT_LEN);
	/*load wext gain*/
	memcpy(pdnlp_dbg_rw_param->wext_gain, pdata->wext_gain,
		sizeof(int) * VPP_DNLP_WEXT_GAIN_LEN);
	/*load new c curve lut for vlsi-kite.li*/
	memcpy(pdnlp_dbg_rw_param->adp_thrd, pdata->adp_thrd,
		sizeof(int) * VPP_DNLP_ADP_THRD_LEN);
	memcpy(pdnlp_dbg_rw_param->reg_blk_boost_12, pdata->reg_blk_boost_12,
		sizeof(int) * VPP_DNLP_REG_BLK_BOOST_LEN);
	memcpy(pdnlp_dbg_rw_param->reg_adp_ofset_20, pdata->reg_adp_ofset_20,
		sizeof(int) * VPP_DNLP_REG_ADP_OFSET_LEN);
	memcpy(pdnlp_dbg_rw_param->reg_mono_protect, pdata->reg_mono_protect,
		sizeof(int) * VPP_DNLP_REG_MONO_PROT_LEN);
	memcpy(pdnlp_dbg_rw_param->reg_trend_wht_expand_lut8,
		pdata->reg_trend_wht_expand_lut8,
		sizeof(int) * VPP_DNLP_TREND_WHT_EXP_LUT_LEN);
	memcpy(pdnlp_dbg_rw_param->c_hist_gain, pdata->c_hist_gain,
		sizeof(int) * VPP_DNLP_HIST_GAIN_LEN);
	memcpy(pdnlp_dbg_rw_param->s_hist_gain, pdata->s_hist_gain,
		sizeof(int) * VPP_DNLP_HIST_GAIN_LEN);

	*pdnlp_alg_input->menu_chg_en = 1;

	/*calc iir-coef params*/
	pdnlp_alg_node_param->dnlp_smhist_ck =
		pdata->param[EN_DNLP_SMHIST_CK];
	pdnlp_alg_node_param->dnlp_mvreflsh =
		pdata->param[EN_DNLP_MVREFLSH];

	pdnlp_alg_node_param->dnlp_cuvbld_min =
		pdata->param[EN_DNLP_CUVBLD_MIN];
	pdnlp_alg_node_param->dnlp_cuvbld_max =
		pdata->param[EN_DNLP_CUVBLD_MAX];

	/*histogram refine parms (remove bb affects)*/
	pdnlp_alg_node_param->dnlp_bbd_ratio_low =
		pdata->param[EN_DNLP_BBD_RATIO_LOW];
	pdnlp_alg_node_param->dnlp_bbd_ratio_hig =
		pdata->param[EN_DNLP_BBD_RATIO_HIG];

	/*brightness_plus*/
	pdnlp_alg_node_param->dnlp_blk_cctr =
		pdata->param[EN_DNLP_BLK_CCTR];
	pdnlp_alg_node_param->dnlp_brgt_ctrl =
		pdata->param[EN_DNLP_BRGT_CTRL];
	pdnlp_alg_node_param->dnlp_brgt_range =
		pdata->param[EN_DNLP_BRGT_RANGE];
	pdnlp_alg_node_param->dnlp_brght_add =
		pdata->param[EN_DNLP_BRGHT_ADD];
	pdnlp_alg_node_param->dnlp_brght_max =
		pdata->param[EN_DNLP_BRGHT_MAX];

	/*hist auto range parms*/
	pdnlp_alg_node_param->dnlp_auto_rng =
		pdata->param[EN_DNLP_AUTO_RNG];
	pdnlp_alg_node_param->dnlp_lowrange =
		pdata->param[EN_DNLP_LOWRANGE];
	pdnlp_alg_node_param->dnlp_hghrange =
		pdata->param[EN_DNLP_HGHRANGE];

	/*for gma_scurvs processing range*/
	pdnlp_alg_node_param->dnlp_satur_rat =
		pdata->param[EN_DNLP_SATUR_RAT];
	pdnlp_alg_node_param->dnlp_satur_max =
		pdata->param[EN_DNLP_SATUR_MAX];
	pdnlp_alg_node_param->dnlp_sbgnbnd =
		pdata->param[EN_DNLP_SBGNBND];
	pdnlp_alg_node_param->dnlp_sendbnd =
		pdata->param[EN_DNLP_SENDBND];

	/*for clahe_curvs processing range*/
	/*pdnlp_alg_node_param->dnlp_clashBgn = pdata->param[EN_DNLP_CLASHBGN];*/
	/*pdnlp_alg_node_param->dnlp_clashEnd = pdata->param[EN_DNLP_CLASHEND];*/

	/*gains to delta of curves (for strength of the DNLP)*/
	pdnlp_alg_node_param->dnlp_clahe_gain_neg =
		pdata->param[EN_DNLP_CLAHE_GAIN_NEG];
	pdnlp_alg_node_param->dnlp_clahe_gain_pos =
		pdata->param[EN_DNLP_CLAHE_GAIN_POS];
	pdnlp_alg_node_param->dnlp_final_gain =
		pdata->param[EN_DNLP_FINAL_GAIN] + dnlp_ai_pq_offset.final_gain;

	/*coef of blending between gma_scurv and clahe curves*/
	pdnlp_alg_node_param->dnlp_mtdbld_rate =
		pdata->param[EN_DNLP_MTDBLD_RATE];
	pdnlp_alg_node_param->dnlp_adpmtd_lbnd =
		pdata->param[EN_DNLP_ADPMTD_LBND];
	pdnlp_alg_node_param->dnlp_adpmtd_hbnd =
		pdata->param[EN_DNLP_ADPMTD_HBND];

	/*black white extension control params*/
	pdnlp_alg_node_param->dnlp_blkext_ofst =
		pdata->param[EN_DNLP_BLKEXT_OFST];
	pdnlp_alg_node_param->dnlp_whtext_ofst =
		pdata->param[EN_DNLP_WHTEXT_OFST];
	pdnlp_alg_node_param->dnlp_blkext_rate =
		pdata->param[EN_DNLP_BLKEXT_RATE];
	pdnlp_alg_node_param->dnlp_whtext_rate =
		pdata->param[EN_DNLP_WHTEXT_RATE];
	pdnlp_alg_node_param->dnlp_bwext_div4x_min =
		pdata->param[EN_DNLP_BWEXT_DIV4X_MIN];
	/*pdnlp_alg_node_param->dnlp_iRgnBgn = pdata->param[EN_DNLP_IRGNBGN];*/
	/*pdnlp_alg_node_param->dnlp_iRgnEnd = pdata->param[EN_DNLP_IRGNEND];*/

	/*curve- clahe*/
	pdnlp_alg_node_param->dnlp_cliprate_v3 =
		pdata->param[EN_DNLP_CLIPRATE_V3];
	pdnlp_alg_node_param->dnlp_cliprate_min =
		pdata->param[EN_DNLP_CLIPRATE_MIN];
	pdnlp_alg_node_param->dnlp_adpcrat_lbnd =
		pdata->param[EN_DNLP_ADPCRAT_LBND];
	pdnlp_alg_node_param->dnlp_adpcrat_hbnd =
		pdata->param[EN_DNLP_ADPCRAT_HBND];

	/*adaptive saturation compensations*/
	pdnlp_alg_node_param->dnlp_scurv_low_th =
		pdata->param[EN_DNLP_SCURV_LOW_TH];
	pdnlp_alg_node_param->dnlp_scurv_mid1_th =
		pdata->param[EN_DNLP_SCURV_MID1_TH];
	pdnlp_alg_node_param->dnlp_scurv_mid2_th =
		pdata->param[EN_DNLP_SCURV_MID2_TH];
	pdnlp_alg_node_param->dnlp_scurv_hgh1_th =
		pdata->param[EN_DNLP_SCURV_HGH1_TH];
	pdnlp_alg_node_param->dnlp_scurv_hgh2_th =
		pdata->param[EN_DNLP_SCURV_HGH2_TH];

	/*new c curve param add for vlsi-kiteli*/
	pdnlp_alg_node_param->dnlp_mtdrate_adp_en =
		pdata->param[EN_DNLP_MTDRATE_ADP_EN];
	pdnlp_alg_node_param->dnlp_clahe_method =
		pdata->param[EN_DNLP_CLAHE_METHOD];
	pdnlp_alg_node_param->dnlp_ble_en =
		pdata->param[EN_DNLP_BLE_EN];
	pdnlp_alg_node_param->dnlp_norm =
		pdata->param[EN_DNLP_NORM];
	pdnlp_alg_node_param->dnlp_scn_chg_th =
		pdata->param[EN_DNLP_SCN_CHG_TH];
	pdnlp_alg_node_param->dnlp_iir_step_mux =
		pdata->param[EN_DNLP_IIR_STEP_MUX];
	pdnlp_alg_node_param->dnlp_single_bin_bw =
		pdata->param[EN_DNLP_SINGLE_BIN_BW];
	pdnlp_alg_node_param->dnlp_single_bin_method =
		pdata->param[EN_DNLP_SINGLE_BIN_METHOD];
	pdnlp_alg_node_param->dnlp_reg_max_slop_1st =
		pdata->param[EN_DNLP_REG_MAX_SLOP_1ST];
	pdnlp_alg_node_param->dnlp_reg_max_slop_mid =
		pdata->param[EN_DNLP_REG_MAX_SLOP_MID];
	pdnlp_alg_node_param->dnlp_reg_max_slop_fin =
		pdata->param[EN_DNLP_REG_MAX_SLOP_FIN];
	pdnlp_alg_node_param->dnlp_reg_min_slop_1st =
		pdata->param[EN_DNLP_REG_MIN_SLOP_1ST];
	pdnlp_alg_node_param->dnlp_reg_min_slop_mid =
		pdata->param[EN_DNLP_REG_MIN_SLOP_MID];
	pdnlp_alg_node_param->dnlp_reg_min_slop_fin =
		pdata->param[EN_DNLP_REG_MIN_SLOP_FIN];
	pdnlp_alg_node_param->dnlp_reg_trend_wht_expand_mode =
		pdata->param[EN_DNLP_REG_TREND_WHT_EXPAND_MODE];
	pdnlp_alg_node_param->dnlp_reg_trend_blk_expand_mode =
		pdata->param[EN_DNLP_REG_TREND_BLK_EXPAND_MODE];
	pdnlp_alg_node_param->dnlp_ve_hist_cur_gain =
		pdata->param[EN_DNLP_HIST_CUR_GAIN];
	pdnlp_alg_node_param->dnlp_ve_hist_cur_gain_precise =
		pdata->param[EN_DNLP_HIST_CUR_GAIN_PRECISE];
	pdnlp_alg_node_param->dnlp_reg_mono_binrang_st =
		pdata->param[EN_DNLP_REG_MONO_BINRANG_ST];
	pdnlp_alg_node_param->dnlp_reg_mono_binrang_ed =
		pdata->param[EN_DNLP_REG_MONO_BINRANG_ED];
	pdnlp_alg_node_param->dnlp_c_hist_gain_base =
		pdata->param[EN_DNLP_C_HIST_GAIN_BASE];
	pdnlp_alg_node_param->dnlp_s_hist_gain_base =
		pdata->param[EN_DNLP_S_HIST_GAIN_BASE];
	pdnlp_alg_node_param->dnlp_mvreflsh_offset =
		pdata->param[EN_DNLP_MVREFLSH_OFFSET];
	pdnlp_alg_node_param->dnlp_luma_avg_th =
		pdata->param[EN_DNLP_LUMA_AVG_TH];

	dnlp_ai_pq_base.final_gain = pdata->param[EN_DNLP_FINAL_GAIN];

	/*update reg data*/
	_init_dnlp_data(pdnlp_alg_input->ve_dnlp_luma_sum,
		pdnlp_alg_output->ve_dnlp_tgt,
		pdnlp_alg_output->ve_dnlp_tgt_10b,
		&dnlp_lpf[0]);
	_calculate_dnlp_reg(&dnlp_lpf[0], &dnlp_reg[0]);
	_set_dnlp_data(cur_mode, &dnlp_reg[0]);
}

bool vpp_module_dnlp_get_insmod_status(void)
{
	return dnlp_alg_insmod_ok;
}

struct dnlp_dbg_parse_cmd_s *vpp_module_dnlp_get_dbg_info(void)
{
	return &dnlp_dbg_parse_cmd[0];
}

struct dnlp_dbg_parse_cmd_s *vpp_module_dnlp_get_dbg_ro_info(void)
{
	return &dnlp_dbg_parse_ro_cmd[0];
}

struct dnlp_dbg_parse_cmd_s *vpp_module_dnlp_get_dbg_cv_info(void)
{
	return &dnlp_dbg_parse_cv_cmd[0];
}

/*For ai pq*/
void vpp_module_dnlp_get_ai_pq_base(struct dnlp_ai_pq_param_s *pparam)
{
	if (!pparam)
		return;

	pparam->final_gain = dnlp_ai_pq_base.final_gain;
}

void vpp_module_dnlp_set_ai_pq_offset(struct dnlp_ai_pq_param_s *pparam)
{
	int offset = 0;

	if (!pparam)
		return;

	pr_vpp(PR_DEBUG_DNLP, "[%s] set data\n", __func__);

	offset = pparam->final_gain;
	if (dnlp_ai_pq_offset.final_gain != offset) {
		dnlp_ai_pq_offset.final_gain = offset;
		pdnlp_alg_node_param->dnlp_final_gain += offset;
	}
}

void vpp_module_dnlp_dump_info(enum vpp_dump_module_info_e info_type)
{
	if (info_type == EN_DUMP_INFO_REG)
		_dump_dnlp_reg_info();
	else
		_dump_dnlp_data_info();
}

