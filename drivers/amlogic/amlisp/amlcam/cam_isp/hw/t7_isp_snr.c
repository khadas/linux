/* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "aml_isp_reg.h"
#include "aml_isp_hw.h"

static void snr_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	u32 i = 0;
	u32 val = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	isp_hw_lut_wstart(isp_dev, SNR_LUT_CFG);

	isp_reg_write(isp_dev, ISP_SNR_PHS_SEL_ADDR, 0);
	for (i = 0; i < 8; i++) {
		val = (lut_cfg->snr_lpf_phs_sel[i * 12 + 0] << 0) |
			(lut_cfg->snr_lpf_phs_sel[i * 12 + 1] << 2) |
			(lut_cfg->snr_lpf_phs_sel[i * 12 + 2] << 4) |
			(lut_cfg->snr_lpf_phs_sel[i * 12 + 3] << 6) |
			(lut_cfg->snr_lpf_phs_sel[i * 12 + 4] << 8) |
			(lut_cfg->snr_lpf_phs_sel[i * 12 + 5] << 10) |
			(lut_cfg->snr_lpf_phs_sel[i * 12 + 6] << 12) |
			(lut_cfg->snr_lpf_phs_sel[i * 12 + 7] << 14) |
			(lut_cfg->snr_lpf_phs_sel[i * 12 + 8] << 16) |
			(lut_cfg->snr_lpf_phs_sel[i * 12 + 9] << 18) |
			(lut_cfg->snr_lpf_phs_sel[i * 12 + 10] << 20) |
			(lut_cfg->snr_lpf_phs_sel[i * 12 + 11] << 22);

		isp_reg_write(isp_dev, ISP_SNR_PHS_SEL_DATA, val);
	}

	isp_hw_lut_wend(isp_dev);
}

static void snr_cfg_noise_profile(struct isp_dev_t *isp_dev, void *param)
{
	int i = 0;
	u32 val = 0;
	aisp_snr_cfg_t *s_cfg = param;

	for (i = 0; i < 8; i++) {
		val = (s_cfg->snr_np_lut16[i * 2 + 0] << 0) |
			(s_cfg->snr_np_lut16[i * 2 + 1] << 12);
		isp_reg_write(isp_dev, ISP_SNR_NP_LUT0_0 + i * 4, val);
	}
}

static void snr_cfg_param(struct isp_dev_t *isp_dev, void *param)
{
	int i = 0;
	u32 val = 0;
	aisp_snr_cfg_t *snr_cfg = param;

	for (i = 0; i < 4; i++) {
		val = (snr_cfg->snr_wt_lut[i * 4 + 0] << 0) |
			(snr_cfg->snr_wt_lut[i * 4 + 1] << 8) |
			(snr_cfg->snr_wt_lut[i * 4 + 2] << 16) |
			(snr_cfg->snr_wt_lut[i * 4 + 3] << 24);
		isp_reg_write(isp_dev, ISP_SNR_WT_LUT_0 + i * 4, val);
	}

	val = snr_cfg->snr_sad_cor_profile_adj;
	isp_reg_update_bits(isp_dev, ISP_SNR_PROFILE_ADJ, val, 8, 8);

	val = snr_cfg->snr_sad_cor_profile_ofst;
	isp_reg_update_bits(isp_dev, ISP_SNR_PROFILE_ADJ, val, 0, 8);

	val = (snr_cfg->snr_sad_wt_sum_th[0] << 0) |
		(snr_cfg->snr_sad_wt_sum_th[1] << 16);
	isp_reg_write(isp_dev, ISP_SNR_WT_ADJ_SUM_TH, val);

	val = snr_cfg->snr_var_flat_th_x0;
	isp_reg_update_bits(isp_dev, ISP_SNR_VARIANCE_FLAT, val, 8, 12);

	val = snr_cfg->snr_var_flat_th_x1;
	isp_reg_update_bits(isp_dev, ISP_SNR_VARIANCE_FLAT, val, 4, 4);

	val = snr_cfg->snr_var_flat_th_x2;
	isp_reg_update_bits(isp_dev, ISP_SNR_VARIANCE_FLAT, val, 0, 4);

	val = (snr_cfg->snr_var_flat_th_y[0] << 0) |
		(snr_cfg->snr_var_flat_th_y[1] << 8) |
		(snr_cfg->snr_var_flat_th_y[2] << 16);
	isp_reg_write(isp_dev, ISP_SNR_VARIANCE_FLAT_GAIN, val);

	val = (snr_cfg->snr_wt_luma_gain[0] << 0) |
		(snr_cfg->snr_wt_luma_gain[1] << 8) |
		(snr_cfg->snr_wt_luma_gain[2] << 16) |
		(snr_cfg->snr_wt_luma_gain[3] << 24);
	isp_reg_write(isp_dev, ISP_SNR_WT_LUMA_SCAL_0, val);

	val = (snr_cfg->snr_wt_luma_gain[4] << 0) |
		(snr_cfg->snr_wt_luma_gain[5] << 8) |
		(snr_cfg->snr_wt_luma_gain[6] << 16) |
		(snr_cfg->snr_wt_luma_gain[7] << 24);
	isp_reg_write(isp_dev, ISP_SNR_WT_LUMA_SCAL_1, val);

	val = (snr_cfg->snr_sad_meta2alp[0] << 0) |
		(snr_cfg->snr_sad_meta2alp[1] << 8) |
		(snr_cfg->snr_sad_meta2alp[2] << 16) |
		(snr_cfg->snr_sad_meta2alp[3] << 24);
	isp_reg_write(isp_dev, ISP_SNR_CORING_META2ALP_0, val);

	val = (snr_cfg->snr_sad_meta2alp[4] << 0) |
		(snr_cfg->snr_sad_meta2alp[5] << 8) |
		(snr_cfg->snr_sad_meta2alp[6] << 16) |
		(snr_cfg->snr_sad_meta2alp[7] << 24);
	isp_reg_write(isp_dev, ISP_SNR_CORING_META2ALP_1, val);

	val = snr_cfg->snr_luma_adj_en;
	isp_reg_update_bits(isp_dev, ISP_SNR_WT_LUMA_ADJ, val, 8, 1);

	val = snr_cfg->snr_sad_wt_adjust_en;
	isp_reg_update_bits(isp_dev, ISP_SNR_WT_ADJ, val, 0, 1);

	val = snr_cfg->snr_mask_en;
	isp_reg_update_bits(isp_dev, ISP_SNR_CTRL, val, 0, 1);

	val = snr_cfg->snr_meta_en;
	isp_reg_update_bits(isp_dev, ISP_SNR_CTRL, val, 16, 1);

	val = snr_cfg->rad_snr1_en;
	isp_reg_update_bits(isp_dev, ISP_CUBIC_RAD_CRTL, val, 25, 1);

	val = ((snr_cfg->snr_wt_var_adj_en << 27) & 0x8000000) |
		  ((snr_cfg->snr_wt_var_meta_th << 20) & 0xF00000) |
		  ((snr_cfg->snr_wt_var_th_x0 << 8) & 0xF00) |
		  ((snr_cfg->snr_wt_var_th_x1 << 4) & 0xF0) |
		  ((snr_cfg->snr_wt_var_th_x2 << 0) & 0xF);
	isp_reg_write(isp_dev, ISP_SNR_WT_VAR_ADJ_X, val);

	val = snr_cfg->snr_wt_var_th_y[0] |
		  ((snr_cfg->snr_wt_var_th_y[1] << 8) & 0xFF00) |
		  ((snr_cfg->snr_wt_var_th_y[2] << 16) & 0xFF0000);
	isp_reg_write(isp_dev, ISP_SNR_WT_VAR_ADJ_Y, val);

	val = snr_cfg->snr_grad_gain[0] |
		(snr_cfg->snr_grad_gain[1] << 8) |
		(snr_cfg->snr_grad_gain[2] << 16) |
		(snr_cfg->snr_grad_gain[3] << 24);
	isp_reg_write(isp_dev, ISP_SNR_CORING_GRAD_GAIN, val);

	val = snr_cfg->snr_grad_gain[4];
	isp_reg_update_bits(isp_dev, ISP_SNR_CORING_GRAD, val, 0, 6);

	val = snr_cfg->snr_sad_th_mask_gain[3] |
		(snr_cfg->snr_sad_th_mask_gain[2] << 8) |
		(snr_cfg->snr_sad_th_mask_gain[1] << 16) |
		(snr_cfg->snr_sad_th_mask_gain[0] << 24);
	isp_reg_write(isp_dev, ISP_SNR_CORING_MASK, val);

	val = snr_cfg->snr_coring_mv_gain_y[0] |
		(snr_cfg->snr_coring_mv_gain_y[1] << 8) |
		(snr_cfg->snr_coring_mv_gain_xn << 16) |
		(snr_cfg->snr_coring_mv_gain_x << 20);
	isp_reg_write(isp_dev, ISP_SNR_MV_CORING_GAIN, val);

	val = (snr_cfg->snr_sad_meta_ratio[0] << 0) |
		(snr_cfg->snr_sad_meta_ratio[1] << 8) |
		(snr_cfg->snr_sad_meta_ratio[2] << 16) |
		(snr_cfg->snr_sad_meta_ratio[3] << 24);
	isp_reg_write(isp_dev, ISP_SNR_SAD_META_RATIO, val);

	val = (snr_cfg->snr_mask_adj[0] << 16) |
		(snr_cfg->snr_mask_adj[1] << 8) |
		(snr_cfg->snr_mask_adj[2] << 0);
	isp_reg_write(isp_dev, ISP_SNR_MASK_LUT0, val);

	val = (snr_cfg->snr_mask_adj[3] << 16) |
		(snr_cfg->snr_mask_adj[4] << 8) |
		(snr_cfg->snr_mask_adj[5] << 0);
	isp_reg_write(isp_dev, ISP_SNR_MASK_LUT1, val);

	val = (snr_cfg->snr_mask_adj[6] << 16) |
		(snr_cfg->snr_mask_adj[7] << 0);
	isp_reg_write(isp_dev, ISP_SNR_MASK_LUT2, val);

	val = (snr_cfg->snr_meta_adj[0] << 16) |
		(snr_cfg->snr_meta_adj[1] << 8) |
		(snr_cfg->snr_meta_adj[2] << 0);
	isp_reg_write(isp_dev, ISP_SNR_META_LUT0, val);

	val = (snr_cfg->snr_meta_adj[3] << 16) |
		(snr_cfg->snr_meta_adj[4] << 8) |
		(snr_cfg->snr_meta_adj[5] << 0);
	isp_reg_write(isp_dev, ISP_SNR_META_LUT1, val);

	val = (snr_cfg->snr_meta_adj[6] << 16) |
		(snr_cfg->snr_meta_adj[7] << 0);
	isp_reg_write(isp_dev, ISP_SNR_META_LUT2, val);

	val = (snr_cfg->snr_cur_wt[1] << 12) |
		(snr_cfg->snr_cur_wt[0] << 0);
	isp_reg_write(isp_dev, ISP_SNR_CUR_WT_0, val);

	val = (snr_cfg->snr_cur_wt[3] << 12) |
		(snr_cfg->snr_cur_wt[2] << 0);
	isp_reg_write(isp_dev, ISP_SNR_CUR_WT_1, val);

	val = (snr_cfg->snr_cur_wt[5] << 12) |
		(snr_cfg->snr_cur_wt[4] << 0);
	isp_reg_write(isp_dev, ISP_SNR_CUR_WT_2, val);

	val = (snr_cfg->snr_cur_wt[7] << 12) |
		(snr_cfg->snr_cur_wt[6] << 0);
	isp_reg_write(isp_dev, ISP_SNR_CUR_WT_3, val);
}

static void snr_cfg_nr(struct isp_dev_t *isp_dev, void *nr)
{
	aisp_nr_cfg_t *nr_cfg = nr;

	if (nr_cfg->pvalid.aisp_snr)
		snr_cfg_noise_profile(isp_dev, &nr_cfg->snr_cfg);

	if (nr_cfg->pvalid.aisp_snr)
		snr_cfg_param(isp_dev, &nr_cfg->snr_cfg);
}

void isp_snr_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	int isp_fmt = 0;
	int xofst, yofst;

	isp_fmt = isp_hw_convert_fmt(fmt);

	xofst = (isp_fmt == 0) ? 0 : //grbg
		(isp_fmt == 1) ? 1 : //rggb
		(isp_fmt == 2) ? 0 : //bggr
		(isp_fmt == 3) ? 1 : //gbrg
		(isp_fmt == 4) ? 3 : 0; //rgbir4x4

	yofst = (isp_fmt == 0) ? 0 : //grbg
		(isp_fmt == 1) ? 0 : //rggb
		(isp_fmt == 4) ? 0 : //rgbir4x4
		(isp_fmt == 2) ? 1 : //bggr
		(isp_fmt == 3) ? 1 : 0; //gbrg

	isp_reg_update_bits(isp_dev, ISP_SNR_CTRL, xofst, 26, 2);
	isp_reg_update_bits(isp_dev, ISP_SNR_CTRL, yofst, 24, 2);
}

void isp_snr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_base)
		snr_cfg_base(isp_dev, &param->base_cfg);

	if (param->pvalid.aisp_nr)
		snr_cfg_nr(isp_dev, &param->nr_cfg);
}

void isp_snr_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;
	int meta_adj[8] = {63, 48, 40, 32, 28, 24, 20, 16};
	int cur_wt[8] = {255, 280, 320, 360, 400, 450, 500, 540};
	int wt_lut[16] = {255, 222, 209, 192, 171, 145, 128, 115,
			104, 96, 88, 82, 72, 64, 57, 51};

	isp_reg_update_bits(isp_dev, ISP_SNR_WT_LUMA_ADJ, 1, 8, 1);
	isp_reg_update_bits(isp_dev, ISP_SNR_WT_ADJ, 1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_SNR_CTRL, 0, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_SNR_CTRL, 0, 16, 1);
	isp_reg_update_bits(isp_dev, ISP_CUBIC_RAD_CRTL, 0, 25, 1);

	val = (2 <<  0) | (6 << 4) | (7 << 8);
	isp_reg_write(isp_dev, ISP_SNR_SAD_META_BND, val);

	isp_reg_update_bits(isp_dev, ISP_SNR_WT_LUMA_ADJ, 8, 0, 4);

	val = (32 << 0) | (44 << 8) | (48 << 16);
	isp_reg_write(isp_dev, ISP_SNR_VARIANCE_FLAT_GAIN, val);

	isp_reg_update_bits(isp_dev, ISP_SNR_VARIANCE_FLAT, 32, 8, 12);

	val = (4 << 24) | (32 << 16) | (4 << 12) | (5 << 8) | (32 << 0);
	isp_reg_write(isp_dev, ISP_SNR_CORING_2, val);

	val = (80 << 12) | (140 << 0);
	isp_reg_write(isp_dev, ISP_SNR_CORING, val);

	val = (180 << 12) | (512 << 0);
	isp_reg_write(isp_dev, ISP_SNR_CORING_1, val);

	val = (meta_adj[0] << 16) | (meta_adj[1] << 8) | (meta_adj[2] << 0);
	isp_reg_write(isp_dev, ISP_SNR_META_LUT0, val);

	val = (meta_adj[3] << 16) | (meta_adj[4] << 8) | (meta_adj[5] << 0);
	isp_reg_write(isp_dev, ISP_SNR_META_LUT1, val);

	val = (meta_adj[6] << 16) | (meta_adj[7] << 0);
	isp_reg_write(isp_dev, ISP_SNR_META_LUT2, val);

	val = (cur_wt[1] << 12) | (cur_wt[0] << 0);
	isp_reg_write(isp_dev, ISP_SNR_CUR_WT_0, val);

	val = (cur_wt[3] << 12) | (cur_wt[2] << 0);
	isp_reg_write(isp_dev, ISP_SNR_CUR_WT_1, val);

	val = (cur_wt[5] << 12) | (cur_wt[4] << 0);
	isp_reg_write(isp_dev, ISP_SNR_CUR_WT_2, val);

	val = (cur_wt[7] << 12) | (cur_wt[6] << 0);
	isp_reg_write(isp_dev, ISP_SNR_CUR_WT_3, val);

	val = (wt_lut[3] << 24) | (wt_lut[2] << 16) |
		wt_lut[1] << 8 | wt_lut[0] << 0;
	isp_reg_write(isp_dev, ISP_SNR_WT_LUT_0, val);

	val = (wt_lut[7] << 24) | (wt_lut[6] << 16) |
		wt_lut[5] << 8 | wt_lut[4] << 0;
	isp_reg_write(isp_dev, ISP_SNR_WT_LUT_1, val);

	val = (wt_lut[11] << 24) | (wt_lut[10] << 16) |
		wt_lut[9] << 8 | wt_lut[8] << 0;
	isp_reg_write(isp_dev, ISP_SNR_WT_LUT_2, val);

	val = (wt_lut[15] << 24) | (wt_lut[14] << 16) |
		wt_lut[13] << 8 | wt_lut[12] << 0;
	isp_reg_write(isp_dev, ISP_SNR_WT_LUT_3, val);

	isp_reg_update_bits(isp_dev, ISP_SNR_WT_LUMA_ADJ, 6, 4, 4);

	val = (16 << 24) | (8 << 16) | (16 << 8) | (16 << 0);
	isp_reg_write(isp_dev, ISP_SNR_CORING_GRAD, val);

	val = (16 << 24) | (12 << 16) |
		(8 << 8) | (4 << 0);
	isp_reg_write(isp_dev, ISP_SNR_CORING_GRAD_RATIO, val);

	val = (63 << 0) | (48 << 8) | (32 << 16) |(24 << 24);
	isp_reg_write(isp_dev, ISP_SNR_CORING_GRAD_GAIN, val);

	val = (32 << 24) | (32 << 16) |
		(32 << 8) | (32 << 0);
	isp_reg_write(isp_dev, ISP_SNR_CORING_MASK, val);

	isp_reg_update_bits(isp_dev, ISP_APL_BLACK_LEVEL, 400, 8, 12);

	val = (255 << 0) | (0 << 8) | (8 << 16) | (1 << 24);
	isp_reg_write(isp_dev, ISP_SNR_SAD_CURV, val);

	val = (64 << 0) | (76 << 8) | (96 << 16) | (102 << 24);
	isp_reg_write(isp_dev, ISP_SNR_VAR_FLAT_LUMA_SCAL_0, val);

	val = (128 << 0) | (128 << 8) | (128 << 16) | (128 << 24);
	isp_reg_write(isp_dev, ISP_SNR_VAR_FLAT_LUMA_SCAL_1, val);

	isp_reg_update_bits(isp_dev, ISP_SNR_VAR_LUMA_RS, 8, 0, 4);

	val = (20 << 0) | (10 << 12) | (2 << 20) | (2 << 28);
	isp_reg_write(isp_dev, ISP_SNR_STRENGHT, val);

	isp_reg_update_bits(isp_dev, ISP_SNR_LPF_PHS_DIFF, 0, 19, 1);
	isp_reg_update_bits(isp_dev, ISP_SNR_SAD_MAP_FLAT_TH, 1, 8, 10);
	isp_reg_update_bits(isp_dev, ISP_SNR_SAD_MAP_FLAT_TH, 100, 20, 10);

	val = (80 << 0) | (700 << 12);
	isp_reg_write(isp_dev, ISP_SNR_SAD_MAP_EDGE_TH, val);
	isp_reg_write(isp_dev, ISP_SNR_SAD_MAP_EDGE_TH1, 1000);

	val = (160 << 0) | (500 << 12);
	isp_reg_write(isp_dev, ISP_SNR_SAD_MAP_TXT_TH, val);

	val = (2100 << 0) | (1572 << 16);
	isp_reg_write(isp_dev, ISP_SNR_WT_ADJ_SUM_TH, val);

	val = (0 << 20) | (4 << 16) | (52 << 8) | (52 << 0);
	isp_reg_write(isp_dev, ISP_SNR_MV_CORING_GAIN, val);
}
