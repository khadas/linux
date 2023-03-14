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

static void dms_cfg_dmsc(struct isp_dev_t *isp_dev, void *dmsc)
{
	u32 val = 0;
	aisp_dmsc_cfg_t *d_cfg = dmsc;

	val = d_cfg->plp_alp;
	isp_reg_update_bits(isp_dev, ISP_DMS_PP_EN, val, 0, 4);

	val = d_cfg->drt_ambg_mxerr_lmt_0;
	isp_reg_update_bits(isp_dev, ISP_DMS_DRT_AMBG1, val, 0, 10);

	val = d_cfg->drt_ambg_mxerr_lmt_1;
	isp_reg_update_bits(isp_dev, ISP_DMS_DRT_AMBG1, val, 16, 10);
}

static void dms_cfg_nr(struct isp_dev_t *isp_dev, void *nr)
{
	aisp_nr_cfg_t *nr_cfg = nr;

	if (nr_cfg->pvalid.aisp_dmsc)
		dms_cfg_dmsc(isp_dev, &nr_cfg->dmsc_cfg);
}

void isp_dms_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
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

	isp_reg_update_bits(isp_dev, ISP_DMS_COMMON_PARAM0, xofst, 2, 2);
	isp_reg_update_bits(isp_dev, ISP_DMS_COMMON_PARAM0, yofst, 0, 2);
}

void isp_dms_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	return;
}

void isp_dms_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_nr)
		dms_cfg_nr(isp_dev, &param->nr_cfg);
}

void isp_dms_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;
	u32 sft = 0;
	int ambg_lut[8] = {4, 8, 10, 12, 14, 15, 15, 15};
	int ambg_ref_en[5] = {1, 1, 1, 1, 1};
	int ambg_sqrt_lut[16]= {0, 23, 32, 45, 64, 91, 111, 128, 143,
			157, 169, 181, 202, 222, 239, 255};

	isp_reg_update_bits(isp_dev, ISP_DMS_COMMON_PARAM0, 1, 4, 1);

	isp_reg_update_bits(isp_dev, ISP_DMS_COMMON_PARAM1, 0, 24, 1);
	isp_reg_update_bits(isp_dev, ISP_DMS_COMMON_PARAM1, 0, 23, 1);
	isp_reg_update_bits(isp_dev, ISP_DMS_COMMON_PARAM1, 0, 19, 4);
	isp_reg_update_bits(isp_dev, ISP_DMS_COMMON_PARAM1, 24, 9, 6);
	isp_reg_update_bits(isp_dev, ISP_DMS_COMMON_PARAM1, 24, 3, 6);
	isp_reg_update_bits(isp_dev, ISP_DMS_COMMON_PARAM1, 0, 0, 3);

	val = (2 << 17) | (0 << 16) | (8 << 8) | (255 << 0);
	isp_reg_write(isp_dev, ISP_DMS_CDM_GRN, val);

	val = (0 << 8) | (1 << 6) |
		(0 << 5) | (0 << 4) |
		(1 << 3) | (1 << 2) |
		(1 << 1) | (0 << 0);
	isp_reg_write(isp_dev, ISP_DMS_GRN_REF0, val);

	val = (94 << 8) | (255 << 0);
	isp_reg_write(isp_dev, ISP_DMS_GRN_REF1, val);

	val = (20 << 16) | (18 << 8) | (0 << 1) | (0 << 0);
	isp_reg_write(isp_dev, ISP_DMS_PRE_PARAM, val);

	val = (30 << 8) | (2 << 6) |
		(2 << 4) | (0 << 3) |
		(1 << 2) | (1 << 1) | (1 << 0);
	isp_reg_write(isp_dev, ISP_DMS_CT_PARAM0, val);

	val = (40 << 16) | (32 << 8) | (128 << 0);
	isp_reg_write(isp_dev, ISP_DMS_CT_PARAM1, val);

	val = (128 << 24) | (128 << 16) | (255 << 8) | (0 << 0);
	isp_reg_write(isp_dev, ISP_DMS_CT_PARAM2, val);

	val = (255 << 8) | (1 << 0);
	isp_reg_write(isp_dev, ISP_DMS_CT_PARAM3, val);

	val = (8 << 12) | (8 << 8) |
		(8 << 4) | (0 << 3) |
		(1 << 2) | (1 << 1) |
		(1 << 0);
	isp_reg_write(isp_dev, ISP_DMS_DRT_GRAD, val);

	val = (60 << 16) | (8 << 12) |
		(8 << 8) | (1 << 4) |
		(1 << 0);
	isp_reg_write(isp_dev, ISP_DMS_DRT_MAXERR, val);

	isp_reg_update_bits(isp_dev, ISP_DMS_DRT_PARAM, 40, 24, 8);
	isp_reg_update_bits(isp_dev, ISP_DMS_DRT_PARAM, 16, 4, 12);
	isp_reg_update_bits(isp_dev, ISP_DMS_DRT_PARAM, 0, 0, 1);

	val = (256 << 16) | (64 << 0);
	isp_reg_write(isp_dev, ISP_DMS_DRT_HFRQ0, val);

	val = (128 << 16) | (384 << 0);
	isp_reg_write(isp_dev, ISP_DMS_DRT_HFRQ1, val);

	val = (16 << 8) | (8 << 0);
	isp_reg_write(isp_dev, ISP_DMS_DRT_HFRQ2, val);

	val = (ambg_lut[0] << 0) | (ambg_lut[1] << 4) |
		(ambg_lut[2] << 8) | (ambg_lut[3] << 12) |
		(ambg_lut[4] << 16) | (ambg_lut[5] << 20) |
		(ambg_lut[6] << 24) | (ambg_lut[7] << 28) ;
	isp_reg_write(isp_dev, ISP_DMS_DRT_AMBG0, val);

	val = (256 << 16) | (5 << 0);
	isp_reg_write(isp_dev, ISP_DMS_DRT_AMBG1, val);

	val = (128 << 16) | (512 << 0);
	isp_reg_write(isp_dev, ISP_DMS_DRT_AMBG2, val);

	sft = (u32)(-2);
	val = (sft << 12) | (0 << 8) |
		(ambg_ref_en[4] << 4) | (ambg_ref_en[3] << 3) |
		(ambg_ref_en[2] << 2) | (ambg_ref_en[1] << 1) |
		(ambg_ref_en[0] << 0);
	isp_reg_write(isp_dev, ISP_DMS_DRT_AMBG3, val);

	val = (255 << 24) | (0 << 16) |
		(128 << 8) | (1 << 4) |
		(0 << 0);
	isp_reg_write(isp_dev, ISP_DMS_DRT_AMBG4, val);

	val = (ambg_sqrt_lut[3] << 24) | (ambg_sqrt_lut[2] << 16) |
		(ambg_sqrt_lut[1] << 8) | (ambg_sqrt_lut[0] << 0);
	isp_reg_write(isp_dev, ISP_DMS_AMBG_SQR_LUT_0, val);

	val = (ambg_sqrt_lut[7] << 24) | (ambg_sqrt_lut[6] << 16) |
		(ambg_sqrt_lut[5] << 8) | (ambg_sqrt_lut[4] << 0);
	isp_reg_write(isp_dev, ISP_DMS_AMBG_SQR_LUT_1, val);

	val = (ambg_sqrt_lut[11] << 24) | (ambg_sqrt_lut[10] << 16) |
		(ambg_sqrt_lut[9] << 8) | (ambg_sqrt_lut[8] << 0);
	isp_reg_write(isp_dev, ISP_DMS_AMBG_SQR_LUT_2, val);

	val = (ambg_sqrt_lut[15] << 24) | (ambg_sqrt_lut[14] << 16) |
		(ambg_sqrt_lut[13] << 8) | (ambg_sqrt_lut[12] << 0);
	isp_reg_write(isp_dev, ISP_DMS_AMBG_SQR_LUT_3, val);

	isp_reg_update_bits(isp_dev, ISP_DMS_PP_EN, 1, 5, 1);

	isp_reg_update_bits(isp_dev, ISP_DMS_DRT_AMBG1, 1023, 16, 10);
}
