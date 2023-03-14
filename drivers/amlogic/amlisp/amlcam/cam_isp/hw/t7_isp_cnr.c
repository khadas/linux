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

static void isp_rcnr_init(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_RAWCNR_CTRL, 1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_RAWCNR_CTRL, 0, 4, 2);
	isp_reg_update_bits(isp_dev, ISP_RAWCNR_CTRL, 0, 6, 2);
	isp_reg_update_bits(isp_dev, ISP_RAWCNR_CTRL, 1, 9, 1);
	isp_reg_update_bits(isp_dev, ISP_RAWCNR_MISC_CTRL0, 4, 8, 5);
}

static void rcnr_cfg_param(struct isp_dev_t *isp_dev, void *param)
{
	u32 val = 0;
	aisp_rawcnr_cfg_t *r_cfg = param;

	val = r_cfg->rawcnr_totblk_higfrq_en;
	isp_reg_update_bits(isp_dev, ISP_RAWCNR_CTRL, val & 0x1, 1, 1);

	val = r_cfg->rawcnr_curblk_higfrq_en;
	isp_reg_update_bits(isp_dev, ISP_RAWCNR_CTRL, val & 0x1, 2, 1);

	val = r_cfg->rawcnr_ishigfreq_mode;
	isp_reg_update_bits(isp_dev, ISP_RAWCNR_CTRL, val & 0x1, 0, 1);

	val = r_cfg->rawcnr_sad_cor_np_gain;
	isp_reg_update_bits(isp_dev, ISP_RAWCNR_NP_CTRL, val, 16, 6);

	val = (r_cfg->rawcnr_meta_gain_lut[0] << 0) |
		(r_cfg->rawcnr_meta_gain_lut[1] << 8) |
		(r_cfg->rawcnr_meta_gain_lut[2] << 16) |
		(r_cfg->rawcnr_meta_gain_lut[3] << 24);
	isp_reg_write(isp_dev, ISP_RAWCNR_META_ADP_LUT_0, val);

	val = (r_cfg->rawcnr_meta_gain_lut[4] << 0) |
		(r_cfg->rawcnr_meta_gain_lut[5] << 8) |
		(r_cfg->rawcnr_meta_gain_lut[6] << 16) |
		(r_cfg->rawcnr_meta_gain_lut[7] << 24);
	isp_reg_write(isp_dev, ISP_RAWCNR_META_ADP_LUT_1, val);

	val = (r_cfg->rawcnr_higfrq_sublk_sum_dif_thd[0] << 16) |
		(r_cfg->rawcnr_higfrq_sublk_sum_dif_thd[1] << 0);
	isp_reg_write(isp_dev, ISP_RAWCNR_UCOR_SUM_DIF_THD, val);

	val = (r_cfg->rawcnr_higfrq_curblk_sum_difnxn_thd[0] << 16) |
		(r_cfg->rawcnr_higfrq_curblk_sum_difnxn_thd[1] << 0);
	isp_reg_write(isp_dev, ISP_RAWCNR_CURBLK_SUM_THD, val);

	val = (r_cfg->rawcnr_thrd_ya_min << 0) |
		(r_cfg->rawcnr_thrd_ya_max << 16);
	isp_reg_write(isp_dev, ISP_RAWCNR_LUMA_MAX_MIN_THD, val);

	val = (r_cfg->rawcnr_thrd_ca_min << 0) |
		(r_cfg->rawcnr_thrd_ca_max << 16);
	isp_reg_write(isp_dev, ISP_RAWCNR_CHROMA_MAX_MIN_THD, val);

	val = (r_cfg->rawcnr_sps_csig_weight5x5[0] << 0) |
		(r_cfg->rawcnr_sps_csig_weight5x5[1] << 3) |
		(r_cfg->rawcnr_sps_csig_weight5x5[2] << 6) |
		(r_cfg->rawcnr_sps_csig_weight5x5[3] << 9) |
		(r_cfg->rawcnr_sps_csig_weight5x5[4] << 12) |
		(r_cfg->rawcnr_sps_csig_weight5x5[5] << 16) |
		(r_cfg->rawcnr_sps_csig_weight5x5[6] << 19) |
		(r_cfg->rawcnr_sps_csig_weight5x5[7] << 22) |
		(r_cfg->rawcnr_sps_csig_weight5x5[8] << 25) |
		(r_cfg->rawcnr_sps_csig_weight5x5[9] << 28);
	isp_reg_write(isp_dev, ISP_RAWCNR_CSIG_WEIGHT5X5_LUT_0, val);

	val = (r_cfg->rawcnr_sps_csig_weight5x5[10] << 0) |
		(r_cfg->rawcnr_sps_csig_weight5x5[11] << 3) |
		(r_cfg->rawcnr_sps_csig_weight5x5[12] << 6) |
		(r_cfg->rawcnr_sps_csig_weight5x5[13] << 9) |
		(r_cfg->rawcnr_sps_csig_weight5x5[14] << 12) |
		(r_cfg->rawcnr_sps_csig_weight5x5[15] << 16) |
		(r_cfg->rawcnr_sps_csig_weight5x5[16] << 19) |
		(r_cfg->rawcnr_sps_csig_weight5x5[17] << 22) |
		(r_cfg->rawcnr_sps_csig_weight5x5[18] << 25) |
		(r_cfg->rawcnr_sps_csig_weight5x5[19] << 28);
	isp_reg_write(isp_dev, ISP_RAWCNR_CSIG_WEIGHT5X5_LUT_1, val);

	val = (r_cfg->rawcnr_sps_csig_weight5x5[20] << 0) |
		(r_cfg->rawcnr_sps_csig_weight5x5[21] << 3) |
		(r_cfg->rawcnr_sps_csig_weight5x5[22] << 6) |
		(r_cfg->rawcnr_sps_csig_weight5x5[23] << 9) |
		(r_cfg->rawcnr_sps_csig_weight5x5[24] << 12);
	isp_reg_write(isp_dev, ISP_RAWCNR_CSIG_WEIGHT5X5_LUT_2, val);
}

static void isp_mcnr_init(struct isp_dev_t *isp_dev)
{
	int val = 0;

	isp_reg_update_bits(isp_dev, ISP_MCNR_PURE_DETAIL, 12, 2, 8);
	isp_reg_update_bits(isp_dev, ISP_MCNR_ZMV_PATCH_EN, 0, 3, 10);
	isp_reg_update_bits(isp_dev, ISP_MCNR_ZMV_PATCH_EN, 15, 13, 10);
	isp_reg_update_bits(isp_dev, ISP_MCNR_ZMV_PATCH_EN, 0, 23, 8);

	isp_reg_update_bits(isp_dev, ISP_MCNR_SSAD_DETAIL, 4, 21, 8);

	val = (10 << 24) | (20 << 16) | (30 << 8) | (110 << 0);
	isp_reg_write(isp_dev, ISP_MCNR_META_SAD_TH0, val);

	val = (1 << 24) | (2 << 16) | (5 << 8) | (120 << 0);
	isp_reg_write(isp_dev, ISP_MCNR_META_SAD_TH1, val);
}

static void mcnr_cfg_noise_profile(struct isp_dev_t *isp_dev, void *param)
{
	int i = 0;
	aisp_snr_cfg_t *s_cfg = param;

	for (i = 0; i < 16; i++)
		isp_reg_write(isp_dev,
			ISP_MCNR_PURE_SAD_NP_LUT_0 + i * 4,
			s_cfg->me_np_lut16[i]);
}

static void mcnr_cfg_param(struct isp_dev_t *isp_dev, void *param)
{
	int i, j;
	u32 val = 0;
	aisp_tnr_cfg_t *tnr_cfg = param;

	val = ((tnr_cfg->me_sad_cor_np_gain & 0xff)  << 16) | ((tnr_cfg->me_sad_cor_np_ofst & 0xff)  << 8);
	isp_reg_write(isp_dev, ISP_MCNR_PURE_SAD_NP_GAIN, val);

	for (j = 0; j < 8; j++) {
		for (i = 0; i < 8; i++) {
			val = (tnr_cfg->mc_meta2alpha[j][i] & 0x3f) << 26;
			isp_reg_write(isp_dev, ISP_MCNR_ALPHA_0_0 + j * 8 * 4 + i * 4, val);
		}
	}
}

static void rcnr_cfg_noise_profile(struct isp_dev_t *isp_dev, void *param)
{
	int i = 0;
	u32 val = 0;
	aisp_snr_cfg_t *s_cfg = param;

	for (i = 0; i < 8; i++) {
		val = (s_cfg->rawcnr_np_lut16[i * 2 + 0] << 0) |
			(s_cfg->rawcnr_np_lut16[i * 2 + 1] << 16);
		isp_reg_write(isp_dev, ISP_RAWCNR_NP_0 + i * 4, val);
	}
}

static void cnr_cfg_nr(struct isp_dev_t *isp_dev, void *nr)
{
	aisp_nr_cfg_t *nr_cfg = nr;

	if (nr_cfg->pvalid.aisp_snr) {
		mcnr_cfg_noise_profile(isp_dev, &nr_cfg->snr_cfg);
		rcnr_cfg_noise_profile(isp_dev, &nr_cfg->snr_cfg);
	}

	if (nr_cfg->pvalid.aisp_tnr)
		mcnr_cfg_param(isp_dev, &nr_cfg->tnr_cfg);

	if (nr_cfg->pvalid.aisp_rawcnr)
		rcnr_cfg_param(isp_dev, &nr_cfg->rawcnr_cfg);
}

void isp_cnr_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
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

	isp_reg_update_bits(isp_dev, ISP_CUBICT_CTRL, xofst, 16, 2);
	isp_reg_update_bits(isp_dev, ISP_CUBICT_CTRL, yofst, 8, 2);

	isp_reg_update_bits(isp_dev, ISP_RAWCNR_CTRL, xofst, 18, 2);
	isp_reg_update_bits(isp_dev, ISP_RAWCNR_CTRL, yofst, 16, 2);
}

void isp_cnr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_nr)
		cnr_cfg_nr(isp_dev, &param->nr_cfg);
}

void isp_cnr_init(struct isp_dev_t *isp_dev)
{
	isp_rcnr_init(isp_dev);
	isp_mcnr_init(isp_dev);
}
