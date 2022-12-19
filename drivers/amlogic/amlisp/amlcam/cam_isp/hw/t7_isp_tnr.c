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

static void tnr_cfg_noise_profile(struct isp_dev_t *isp_dev, void *tnr)
{
	int i = 0;
	u32 val = 0;
	aisp_tnr_cfg_t *t_cfg = tnr;

	for (i = 0; i < 8; i++) {
		val = (t_cfg->ma_np_lut16[i * 2 + 0] << 0) |
			(t_cfg->ma_np_lut16[i * 2 + 1] << 12);

		isp_reg_write(isp_dev, ISP_TNR_NP_LUT_0 + i * 4, val);
	}
}

static void tnr_cfg_param(struct isp_dev_t *isp_dev, void *param)
{
	int i = 0;
	u32 val = 0;
	u32 addr;
	aisp_tnr_cfg_t *tnr_cfg = param;

	isp_hw_lut_wstart(isp_dev, TNR_LUT_CFG);

	val = tnr_cfg->rad_tnr0_en;
	isp_reg_update_bits(isp_dev, ISP_CUBIC_RAD_CRTL, val, 26, 1);

	val = tnr_cfg->ma_mix_ratio;
	isp_reg_update_bits(isp_dev, ISP_CUBICT_ERR_MIX_RATIO, val, 0, 8);

	val = (tnr_cfg->ma_mix_th_x0 << 16) |
		(tnr_cfg->ma_mix_th_x1 << 8) |
		(tnr_cfg->ma_mix_th_x2 << 0);
	isp_reg_write(isp_dev, ISP_CUBICT_MIX_THRD_X, val);

	val = (tnr_cfg->ma_mix_h_th_y0 << 16) |
		(tnr_cfg->ma_mix_h_th_y1 << 8) |
		(tnr_cfg->ma_mix_h_th_y2 << 0);
	isp_reg_write(isp_dev, ISP_CUBICT_HIGH_MIX_THRD_Y, val);

	val = (tnr_cfg->ma_mix_l_th_y0 << 16) |
		(tnr_cfg->ma_mix_l_th_y1 << 8) |
		(tnr_cfg->ma_mix_l_th_y2 << 0);
	isp_reg_write(isp_dev, ISP_CUBICT_LOW_MIX_THRD_Y, val);

	val = (tnr_cfg->ma_sad_var_th_x0 << 16) |
		(tnr_cfg->ma_sad_var_th_x1 << 12) |
		(tnr_cfg->ma_sad_var_th_x2 << 8);
	isp_reg_write(isp_dev, ISP_CUBICT_SAD_VAR_GAIN_X, val);
	val = (tnr_cfg->ma_sad_var_th_y_0 << 20) |
		(tnr_cfg->ma_sad_var_th_y_1 << 14) |
		(tnr_cfg->ma_sad_var_th_y_2 << 8);
	isp_reg_write(isp_dev, ISP_CUBICT_SAD_VAR_GAIN_Y, val);

	val = (tnr_cfg->me_meta_sad_th0[0] << 24) |
		(tnr_cfg->me_meta_sad_th0[1] << 16) |
		(tnr_cfg->me_meta_sad_th0[2] << 8);
	isp_reg_update_bits(isp_dev, ISP_MCNR_META_SAD_TH0, val, 8, 24);

	val = (tnr_cfg->me_meta_sad_th1[0] << 24) |
		(tnr_cfg->me_meta_sad_th1[1] << 16) |
		(tnr_cfg->me_meta_sad_th1[2] << 8);
	isp_reg_update_bits(isp_dev, ISP_MCNR_META_SAD_TH1, val, 8, 24);

	val = (tnr_cfg->ma_sad_luma_adj_x[0] << 16) |
		(tnr_cfg->ma_sad_luma_adj_x[1] << 4);
	isp_reg_write(isp_dev, ISP_CUBICT_SAD_LUMA_GAIN_X, val);

	val = (tnr_cfg->ma_sad_luma_adj_x[2] << 12) |
		(tnr_cfg->ma_sad_luma_adj_x[3] << 0);
	isp_reg_update_bits(isp_dev, ISP_CUBICT_ERR_MIX_RATIO, val, 8, 24);

	val = (tnr_cfg->ma_sad_luma_adj_y[0] << 24) |
		(tnr_cfg->ma_sad_luma_adj_y[1] << 18) |
		(tnr_cfg->ma_sad_luma_adj_y[2] << 12) |
		(tnr_cfg->ma_sad_luma_adj_y[3] << 6) |
		(tnr_cfg->ma_sad_luma_adj_y[4] << 0);
	isp_reg_update_bits(isp_dev, ISP_CUBICT_SAD_LUMA_GAIN_Y, val, 0, 30);

	val = (tnr_cfg->ma_sad_pdtl4_x0 << 8) |
		(tnr_cfg->ma_sad_pdtl4_x1 << 4) |
		(tnr_cfg->ma_sad_pdtl4_x2 << 0);
	isp_reg_write(isp_dev, ISP_CUBICT_DETAIL_CURV_X, val);

	val = (tnr_cfg->ma_sad_pdtl4_y0 << 24) |
		(tnr_cfg->ma_sad_pdtl4_y1 << 15) |
		(tnr_cfg->ma_sad_pdtl4_y2 << 8);
	isp_reg_write(isp_dev, ISP_CUBICT_DETAIL_CURV_Y, val);

	val = tnr_cfg->ma_adp_dtl_mix_th_nfl;
	isp_reg_update_bits(isp_dev, ISP_CUBICT_ADP_DTL_NFL, val, 0, 8);

	for (i = 0; i < 4; i++) {
		addr = ISP_CUBICT_MASK_GAIN_0 + 4 * i;
		isp_reg_write(isp_dev, addr, tnr_cfg->ma_sad_th_mask_gain[i]);

		addr = ISP_CUBICT_MIX_THD_MASK_GAIN_0 + 4 * i;
		isp_reg_write(isp_dev, addr, tnr_cfg->ma_mix_th_mask_gain[i]);
	}

	val = tnr_cfg->ma_tnr_sad_cor_np_gain;
	isp_reg_update_bits(isp_dev, ISP_CUBICT_SAD_CORING_NP_CRTL, val, 8, 8);

	val = tnr_cfg->ma_tnr_sad_cor_np_ofst;
	isp_reg_update_bits(isp_dev, ISP_CUBICT_SAD_CORING_NP_CRTL, val, 0, 8);

	isp_reg_write(isp_dev, ISP_CUBICT_ALPHA0_LUT_ADDR, 0);
	for (i = 0; i < 16; i++) {
		val = (tnr_cfg->lut_meta_sad_2alpha[i * 4 + 0] << 0) |
			(tnr_cfg->lut_meta_sad_2alpha[i * 4 + 1] << 8) |
			(tnr_cfg->lut_meta_sad_2alpha[i * 4 + 2] << 16) |
			(tnr_cfg->lut_meta_sad_2alpha[i * 4 + 3] << 24);
		isp_reg_write(isp_dev, ISP_CUBICT_ALPHA0_LUT_DATA, val);
	}

	val = (tnr_cfg->ma_mix_h_th_gain[0] << 18) |
		(tnr_cfg->ma_mix_h_th_gain[1] << 12) |
		(tnr_cfg->ma_mix_h_th_gain[2] << 6) |
		(tnr_cfg->ma_mix_h_th_gain[3] << 0);
	isp_reg_write(isp_dev, ISP_CUBICT_MIX_H_GAIN, val);

	isp_hw_lut_wend(isp_dev);
}

static void tnr_cfg_nr(struct isp_dev_t *isp_dev, void *nr)
{
	aisp_nr_cfg_t *nr_cfg = nr;

	if (nr_cfg->pvalid.aisp_tnr)
		tnr_cfg_noise_profile(isp_dev, &nr_cfg->tnr_cfg);

	if (nr_cfg->pvalid.aisp_tnr)
		tnr_cfg_param(isp_dev, &nr_cfg->tnr_cfg);
}

void isp_tnr_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param)
{
	u32 val = 0;
	u32 start, end;

	if (param->pos == 0) {
		start = 0;
		end = param->pleft_hsize - param->pleft_ovlp;

		val = (start << 16) | (end << 0);
		isp_hwreg_write(isp_dev, ISP_CUBICT_GLOBAL_STAT_WINDOW, val);
	} else if (param->pos == 1) {
		start = param->pright_ovlp;
		end = param->pright_hsize;

		val = (start << 16) | (end << 0);
		isp_hwreg_write(isp_dev, ISP_CUBICT_GLOBAL_STAT_WINDOW, val);
	}
}

void isp_tnr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_nr)
		tnr_cfg_nr(isp_dev, &param->nr_cfg);
}

void isp_tnr_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	isp_reg_update_bits(isp_dev, ISP_CUBIC_RAD_CRTL, 1, 26, 1);

	val = (30 << 16) | (40 << 8) | (50 << 0);
	isp_reg_write(isp_dev, ISP_CUBICT_HIGH_MIX_THRD_Y, val);

	val = (36 << 16) | (5 << 8) | (5 << 0);
	isp_reg_write(isp_dev, ISP_CUBICT_MIX_THRD_X, val);

	val = (30 << 16) | (50 << 8) | (60 << 0);
	isp_reg_write(isp_dev, ISP_CUBICT_LOW_MIX_THRD_Y, val);

	val = (80 << 20) | (80 << 8) | (64 << 0);
	isp_reg_write(isp_dev, ISP_CUBICT_ERR_MIX_RATIO, val);

	val = (70 << 16) | (75 << 4);
	isp_reg_write(isp_dev, ISP_CUBICT_SAD_LUMA_GAIN_X, val);

	val = (32 << 24) | (24 << 18) | (18 << 12) | (16 << 6) | (16 << 0);
	isp_reg_write(isp_dev, ISP_CUBICT_SAD_LUMA_GAIN_Y, val);

	isp_reg_update_bits(isp_dev, ISP_CUBICT_SAD_CORING_NP_CRTL, 10, 8, 8);

	val = (240 << 0) | (120 << 8) | (64 << 16) | (48 << 24);
	isp_reg_write(isp_dev, ISP_LOSS_CORING_META2ALP_0, val);

	val = (32 << 0) | (24 << 8) | (16 << 16) | (16 << 24);
	isp_reg_write(isp_dev, ISP_LOSS_CORING_META2ALP_1, val);

	isp_reg_write(isp_dev, ISP_CUBICT_DETAIL_CORING, 32);

	isp_reg_update_bits(isp_dev, ISP_CUBICT_ALPHA_LUMA_GAIN_BLC, 240, 0, 12);

	val = (1 << 6) | (0 << 8) | (1 << 12) | (12 << 16) | (8 << 20);
	isp_reg_write(isp_dev, ISP_CUBICT_META_PP2, val);

	isp_reg_write(isp_dev, ISP_CUBICT_ALPHA0_LUT_DATA, 0);

	val = (1 << 21);
	isp_reg_write(isp_dev, ISP_CUBICT_MIX_CTRL, val);

	isp_reg_write(isp_dev, ISP_CUBICT_ADP_DTL_NFL, 80);

	val = (2 << 24) | (8 << 16) | (12 << 8);
	isp_reg_write(isp_dev, ISP_CUBICT_DETAIL_CURV_Y, val);

	isp_reg_update_bits(isp_dev, ISP_PST_TNR_ALP_LUT_1, 36, 16, 6);
	isp_reg_update_bits(isp_dev, ISP_PST_TNR_ALP_LUT_1, 40, 24, 6);
}
