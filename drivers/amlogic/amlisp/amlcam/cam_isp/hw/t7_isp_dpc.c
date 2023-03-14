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

static void dpc_cfg_param(struct isp_dev_t *isp_dev, void *param)
{
	u32 val = 0;
	aisp_dpc_cfg_t *dpc_cfg = param;

	val = (dpc_cfg->dpc_avg_gain_h0[0] << 16) | (dpc_cfg->dpc_avg_gain_l0[0] << 0);
	isp_reg_write(isp_dev, ISP_DPC0_AVG_GAIN0, val);

	val = (dpc_cfg->dpc_avg_gain_h1[0] << 16) | (dpc_cfg->dpc_avg_gain_l1[0] << 0);
	isp_reg_write(isp_dev, ISP_DPC0_AVG_GAIN1, val);

	val = (dpc_cfg->dpc_avg_gain_h2[0] << 16) | (dpc_cfg->dpc_avg_gain_l2[0] << 0);
	isp_reg_write(isp_dev, ISP_DPC0_AVG_GAIN2, val);

	val = dpc_cfg->dpc_cond_en[0];
	isp_reg_update_bits(isp_dev, ISP_DPC0_VAR_THD, val, 24, 8);

	val = (dpc_cfg->dpc_max_min_bias_thd[0]) |
		(dpc_cfg->dpc_std_diff_gain[0] << 8) |
		(dpc_cfg->dpc_std_gain[0] << 20);
	isp_reg_write(isp_dev, ISP_DPC0_STD_GAIN, val);

	val = dpc_cfg->dpc_avg_dev_offset[0];
	isp_reg_update_bits(isp_dev, ISP_DPC0_AVG_DEV, val, 16, 8);

	val = dpc_cfg->dpc_cor_en[0];
	isp_reg_update_bits(isp_dev, ISP_DPC0_CNTL, val, 0, 1);

	val = dpc_cfg->dpc_avg_dev_mode[0];
	isp_reg_update_bits(isp_dev, ISP_DPC0_AVG_DEV, val, 24, 1);

	val = dpc_cfg->dpc_avg_mode[0];
	isp_reg_update_bits(isp_dev, ISP_DPC0_AVG_MOD, val, 8, 2);

	val = dpc_cfg->dpc_avg_thd2_en[0];
	isp_reg_update_bits(isp_dev, ISP_DPC0_VAR_GAIN, val, 20, 1);

	val = dpc_cfg->dpc_highlight_en[0];
	isp_reg_update_bits(isp_dev, ISP_DPC0_COR_CTRL, val, 4, 1);

	val = dpc_cfg->dpc_correct_mode[0];
	isp_reg_update_bits(isp_dev, ISP_DPC0_COR_CTRL, val, 0, 2);

	val = dpc_cfg->dpc_write_to_lut[0];
	isp_reg_update_bits(isp_dev, ISP_DPC0_LUT_CTRL, val, 16, 1);

	val = (dpc_cfg->dpc_avg_gain_h0[1] << 16) | (dpc_cfg->dpc_avg_gain_l0[1] << 0);
	isp_reg_write(isp_dev, ISP_DPC1_AVG_GAIN0, val);

	val = (dpc_cfg->dpc_avg_gain_h1[1] << 16) | (dpc_cfg->dpc_avg_gain_l1[1] << 0);
	isp_reg_write(isp_dev, ISP_DPC1_AVG_GAIN1, val);

	val = (dpc_cfg->dpc_avg_gain_h2[1] << 16) | (dpc_cfg->dpc_avg_gain_l2[1] << 0);
	isp_reg_write(isp_dev, ISP_DPC1_AVG_GAIN2, val);

	val = dpc_cfg->dpc_cond_en[1];
	isp_reg_update_bits(isp_dev, ISP_DPC1_VAR_THD, val, 24, 8);

	val = (dpc_cfg->dpc_max_min_bias_thd[1]) |
		(dpc_cfg->dpc_std_diff_gain[1] << 8) |
		(dpc_cfg->dpc_std_gain[1] << 20);
	isp_reg_write(isp_dev, ISP_DPC1_STD_GAIN, val);

	val = dpc_cfg->dpc_avg_dev_offset[1];
	isp_reg_update_bits(isp_dev, ISP_DPC1_AVG_DEV, val, 16, 8);

	val = dpc_cfg->dpc_cor_en[1];
	isp_reg_update_bits(isp_dev, ISP_DPC1_CNTL, val, 0, 1);

	val = dpc_cfg->dpc_avg_dev_mode[1];
	isp_reg_update_bits(isp_dev, ISP_DPC1_AVG_DEV, val, 24, 1);

	val = dpc_cfg->dpc_avg_mode[1];
	isp_reg_update_bits(isp_dev, ISP_DPC1_AVG_MOD, val, 8, 2);

	val = dpc_cfg->dpc_avg_thd2_en[1];
	isp_reg_update_bits(isp_dev, ISP_DPC1_VAR_GAIN, val, 20, 1);

	val = dpc_cfg->dpc_highlight_en[1];
	isp_reg_update_bits(isp_dev, ISP_DPC1_COR_CTRL, val, 4, 1);

	val = dpc_cfg->dpc_correct_mode[1];
	isp_reg_update_bits(isp_dev, ISP_DPC1_COR_CTRL, val, 0, 2);

	val = dpc_cfg->dpc_write_to_lut[1];
	isp_reg_update_bits(isp_dev, ISP_DPC1_LUT_CTRL, val, 16, 1);
}

void isp_dpc_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
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

	isp_reg_update_bits(isp_dev, ISP_DPC0_CNTL, xofst, 8, 2);
	isp_reg_update_bits(isp_dev, ISP_DPC0_CNTL, yofst, 12, 2);

	isp_reg_update_bits(isp_dev, ISP_DPC1_CNTL, xofst, 8, 2);
	isp_reg_update_bits(isp_dev, ISP_DPC1_CNTL, yofst, 12, 2);
}

void isp_dpc_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_dpc)
		dpc_cfg_param(isp_dev, &param->dpc_cfg);
}

void isp_dpc_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

/* DPC0 */
	isp_reg_update_bits(isp_dev, ISP_DPC0_CNTL, 1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_DPC0_CNTL, 1, 4, 1);

	val = (512 << 16) | (128 << 0);
	isp_reg_write(isp_dev, ISP_DPC0_AVG_GAIN0, val);

	val = (435 << 16) | (150 << 0);
	isp_reg_write(isp_dev, ISP_DPC0_AVG_GAIN1, val);

	val = (332 << 16) | (196 << 0);
	isp_reg_write(isp_dev, ISP_DPC0_AVG_GAIN2, val);

	//isp_reg_update_bits(isp_dev, ISP_DPC0_STD_GAIN, 55, 0, 8);
	//isp_reg_update_bits(isp_dev, ISP_DPC0_STD_GAIN, 22, 8, 5);

	isp_reg_update_bits(isp_dev, ISP_DPC0_AVG_MOD, 40, 0, 8);
	isp_reg_update_bits(isp_dev, ISP_DPC0_AVG_MOD, 0, 8, 2);
	isp_reg_update_bits(isp_dev, ISP_DPC0_AVG_MOD, 61, 0, 8);

	isp_reg_update_bits(isp_dev, ISP_DPC0_AVG_DEV, 2, 28, 2);
	isp_reg_update_bits(isp_dev, ISP_DPC0_AVG_DEV, 1, 24, 1);
	//isp_reg_update_bits(isp_dev, ISP_DPC0_AVG_DEV, 0, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_DPC0_AVG_DEV, 4095, 0, 12);

	val = (33 << 16) | (2129 << 0);
	isp_reg_write(isp_dev, ISP_DPC0_DEV_DP, val);

	val = (0 << 24) | (1 << 16) | (3 << 12) | (7 << 8) | (0 << 4) | (3 << 0);
	isp_reg_write(isp_dev, ISP_DPC0_COR_CTRL, val);

	val = (0 << 16) | (0xfff << 0);
	isp_reg_write(isp_dev, ISP_DPC0_BLEND, val);

	isp_reg_update_bits(isp_dev, ISP_DPC0_LUT_CTRL, 0, 16, 1);
	isp_reg_update_bits(isp_dev, ISP_DPC0_LUT_CTRL, 0, 0, 12);

	isp_reg_update_bits(isp_dev, ISP_DPC0_VAR_THD, 32, 24, 8);
	isp_reg_update_bits(isp_dev, ISP_DPC0_VAR_GAIN, 2, 16, 3);
	isp_reg_update_bits(isp_dev, ISP_DPC0_VAR_GAIN, 1, 24, 1);

/* DPC1 */
	isp_reg_update_bits(isp_dev, ISP_DPC1_CNTL, 1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_DPC1_CNTL, 1, 4, 1);

	val = (512 << 16) | (128 << 0);
	isp_reg_write(isp_dev, ISP_DPC1_AVG_GAIN0, val);

	val = (435 << 16) | (150 << 0);
	isp_reg_write(isp_dev, ISP_DPC1_AVG_GAIN1, val);

	val = (332 << 16) | (196 << 0);
	isp_reg_write(isp_dev, ISP_DPC1_AVG_GAIN2, val);

	//isp_reg_update_bits(isp_dev, ISP_DPC1_STD_GAIN, 55, 0, 8);
	//isp_reg_update_bits(isp_dev, ISP_DPC1_STD_GAIN, 22, 8, 5);

	isp_reg_update_bits(isp_dev, ISP_DPC1_AVG_MOD, 40, 0, 8);
	isp_reg_update_bits(isp_dev, ISP_DPC1_AVG_MOD, 0, 8, 2);
	isp_reg_update_bits(isp_dev, ISP_DPC1_AVG_MOD, 61, 0, 8);

	isp_reg_update_bits(isp_dev, ISP_DPC1_AVG_DEV, 2, 28, 2);
	isp_reg_update_bits(isp_dev, ISP_DPC1_AVG_DEV, 1, 24, 1);
	//isp_reg_update_bits(isp_dev, ISP_DPC1_AVG_DEV, 0, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_DPC1_AVG_DEV, 4095, 0, 12);

	val = (33 << 16) | (2129 << 0);
	isp_reg_write(isp_dev, ISP_DPC1_DEV_DP, val);

	val = (0 << 24) | (1 << 16) | (3 << 12) | (7 << 8) | (0 << 4) | (3 << 0);
	isp_reg_write(isp_dev, ISP_DPC1_COR_CTRL, val);

	val = (0 << 16) | (0xfff << 0);
	isp_reg_write(isp_dev, ISP_DPC1_BLEND, val);

	isp_reg_update_bits(isp_dev, ISP_DPC1_LUT_CTRL, 0, 16, 1);
	isp_reg_update_bits(isp_dev, ISP_DPC1_LUT_CTRL, 0, 0, 12);

	isp_reg_update_bits(isp_dev, ISP_DPC1_VAR_THD, 32, 24, 8);
	isp_reg_update_bits(isp_dev, ISP_DPC1_VAR_GAIN, 2, 16, 3);
	isp_reg_update_bits(isp_dev, ISP_DPC1_VAR_GAIN, 1, 24, 1);
}
