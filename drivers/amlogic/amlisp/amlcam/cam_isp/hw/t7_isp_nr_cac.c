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

#define CAC_TABLE_XSIZE     32
#define CAC_TABLE_YSIZE     32

static void lcge_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	isp_reg_update_bits(isp_dev, ISP_LCGE_EN, 15, 0, 8);

	val = (0 << 4) | (0 << 0);
	isp_reg_write(isp_dev, ISP_LCGE_FLAT_RATIO_0, val);

	val = (5 << 4) | (1 << 0);
	isp_reg_write(isp_dev, ISP_LCGE_FLAT_RATIO_1, val);

	val = (6 << 4) | (2 << 0);
	isp_reg_write(isp_dev, ISP_LCGE_FLAT_RATIO_2, val);

	val = (8 << 4) | (3 << 0);
	isp_reg_write(isp_dev, ISP_LCGE_FLAT_RATIO_3, val);

	val = (10 << 4) | (4 << 0);
	isp_reg_write(isp_dev, ISP_LCGE_FLAT_RATIO_4, val);

	val = (11 << 4) | (5 << 0);
	isp_reg_write(isp_dev, ISP_LCGE_FLAT_RATIO_5, val);

	val = (12 << 4) | (6 << 0);
	isp_reg_write(isp_dev, ISP_LCGE_FLAT_RATIO_6, val);

	val = (13 << 4) | (7 << 0);
	isp_reg_write(isp_dev, ISP_LCGE_FLAT_RATIO_7, val);

	isp_reg_update_bits(isp_dev, ISP_LCGE_ALPHA, 1, 8, 1);
}

static void cubic_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	int i = 0;
	u32 val = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	isp_hw_lut_wstart(isp_dev, CAC_TAB_LUT_CFG);

	isp_reg_write(isp_dev, ISP_CAC_TAB_CTRL, 1);
	isp_reg_write(isp_dev, ISP_CAC_TAB_ADDR, 0);
	for (i = 0; i < 512; i++) {
		val = (lut_cfg->CAC_table_Rx[2 * i] & 0xff) |
			(((lut_cfg->CAC_table_Ry[2 * i]) << 8) & 0xFF00) |
			(((lut_cfg->CAC_table_Rx[2 * i + 1]) << 16) & 0xFF0000) |
			(((lut_cfg->CAC_table_Ry[2 * i + 1]) << 24) & 0xFF000000);
		isp_reg_write(isp_dev, ISP_CAC_TAB_DATA, val);
	}

	for (i = 0; i < 512; i++) {
		val = (lut_cfg->CAC_table_Bx[2 * i] & 0xff) |
			(((lut_cfg->CAC_table_By[2 * i]) << 8) & 0xFF00) |
			(((lut_cfg->CAC_table_Bx[2 * i + 1]) << 16) & 0xFF0000) |
			(((lut_cfg->CAC_table_By[2 * i + 1]) << 24) & 0xFF000000);
		isp_reg_write(isp_dev, ISP_CAC_TAB_DATA, val);
	}
	isp_reg_write(isp_dev, ISP_CAC_TAB_CTRL, 0);

	isp_hw_lut_wend(isp_dev);
}

void isp_nr_cac_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
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

	isp_reg_update_bits(isp_dev, ISP_CAC_CNTL, xofst, 24, 2);
	isp_reg_update_bits(isp_dev, ISP_CAC_CNTL, yofst, 28, 2);

	isp_reg_update_bits(isp_dev, ISP_CUBIC_CS_COMMON, xofst, 8, 2);
	isp_reg_update_bits(isp_dev, ISP_CUBIC_CS_COMMON, yofst, 4, 2);
}

void isp_nr_cac_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize = fmt->width;
	u32 ysize = fmt->height;

	val = xsize / (CAC_TABLE_XSIZE - 1) + 1;
	isp_reg_update_bits(isp_dev, ISP_CAC_CNTL, val, 12, 12);

	val = ysize / (CAC_TABLE_YSIZE - 1) + 1;
	isp_reg_update_bits(isp_dev, ISP_CAC_CNTL, val, 0, 12);

	val = (1 << 24) / (xsize /(CAC_TABLE_XSIZE - 1) + 1);
	isp_reg_update_bits(isp_dev, ISP_CAC_CNTL0, val, 0, 23);

	val = (1 << 24) / (ysize /(CAC_TABLE_YSIZE - 1) + 1);
	isp_reg_update_bits(isp_dev, ISP_CAC_CNTL1, val, 0, 23);

	val = ((1 << 24) / (xsize >> 1)) >> 4;
	isp_reg_update_bits(isp_dev, ISP_CUBIC_RAD_SCL0, val, 16, 16);

	val = ((1 << 24) / (ysize >> 1)) >> 4;
	isp_reg_update_bits(isp_dev, ISP_CUBIC_RAD_SCL0, val, 0, 16);

	isp_reg_update_bits(isp_dev, ISP_CUBIC_RAD_SCL1, 181, 0, 8);

	val = (xsize >> 1);
	isp_reg_update_bits(isp_dev, ISP_CUBIC_RAD_CENTER, val, 16, 16);

	val = (ysize >> 1);
	isp_reg_update_bits(isp_dev, ISP_CUBIC_RAD_CENTER, val, 0, 16);
}

void isp_nr_cac_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_base)
		cubic_cfg_base(isp_dev, &param->base_cfg);
}

void isp_nr_cac_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param)
{
	u32 val = 0;

	if (param->pos == 0) {
		isp_hwreg_update_bits(isp_dev, ISP_CAC_COL_OFFSET, 0, 0, 5);
		isp_hwreg_update_bits(isp_dev, ISP_CAC_COL_OFFSET, 0, 8, 12);
	} else if (param->pos == 1) {
		val = (CAC_TABLE_XSIZE - 1) / 2;
		isp_hwreg_update_bits(isp_dev, ISP_CAC_COL_OFFSET, val, 0, 5);

		if ((CAC_TABLE_XSIZE - 1) & 0x1) {
			val = isp_reg_read(isp_dev, ISP_CAC_CNTL);
			val = (val >> 12) & 0xfff;
			isp_hwreg_update_bits(isp_dev, ISP_CAC_COL_OFFSET, val / 2, 8, 12);
		} else {
			isp_hwreg_update_bits(isp_dev, ISP_CAC_COL_OFFSET, 0, 8, 12);
		}
	}
}

void isp_nr_cac_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;
	u32 lcge_ratio[8] = {0, 4, 8, 12, 16, 20, 24, 28};

	isp_reg_update_bits(isp_dev, ISP_CAC_CNTL, 0x1f, 12, 12);
	isp_reg_update_bits(isp_dev, ISP_CAC_CNTL, 0x12, 0, 12);

	isp_reg_write(isp_dev, ISP_CAC_CNTL0, 0x84210);
	isp_reg_write(isp_dev, ISP_CAC_CNTL1, 0xe38e3);

	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF0, 0x8000);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF1, 0x280fe);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF2, 0x57ffc);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF3, 0xff087efb);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF4, 0xff0c7bfa);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF5, 0xff1078f9);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF6, 0xfe1476f8);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF7, 0xfe1873f7);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF8, 0xfd1d6ff7);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF9, 0xfc226bf7);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF10, 0xfc2766f7);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF11, 0xfb2c62f7);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF12, 0xfa325df7);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF13, 0xfa3758f7);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF14, 0xf93d53f7);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF15, 0xf9424df8);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF16, 0xf84848f8);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF17, 0xf84d42f9);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF18, 0xf7533df9);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF19, 0xf75837fa);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF20, 0xf75d32fa);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF21, 0xf7622cfb);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF22, 0xf76627fc);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF23, 0xf76b22fc);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF24, 0xf76f1dfd);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF25, 0xf77318fe);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF26, 0xf87614fe);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF27, 0xf97810ff);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF28, 0xfa7b0cff);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF29, 0xfb7e08ff);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF30, 0xfc7f0500);
	isp_reg_write(isp_dev, ISP_CAC_FILTER_COEF31, 0xfe800200);

	val = (1 << 16) | (1 << 12) | (1 << 8) | (2 << 4) | (0 << 0);
	isp_reg_write(isp_dev, ISP_PDPC_CNTL, val);

	val = (1 << 24) | (2 << 16) | (2 << 8) | (3 << 0);
	isp_reg_write(isp_dev, ISP_PDPC_THRD0, val);

	val = (4 << 24) | (16 << 16) | (16 << 8) | (4 << 0);
	isp_reg_write(isp_dev, ISP_PDPC_RATIO_CTRL, val);

	val = (512 << 16) | (65535 << 0);
	isp_reg_write(isp_dev, ISP_PDPC_THR_CTRL, val);

	isp_reg_update_bits(isp_dev, ISP_CUBIC_CS_POWER, 2, 16, 2);
	isp_reg_update_bits(isp_dev, ISP_CUBIC_CS_POWER, 2, 24, 2);
	isp_reg_update_bits(isp_dev, ISP_CUBIC_RAD_CRTL, 0, 26, 1);

	val = (1092 << 16) | (1941 << 0);
	isp_reg_write(isp_dev, ISP_CUBIC_RAD_SCL0, val);
	isp_reg_write(isp_dev, ISP_CUBIC_LUT65_ADDR, 0);
	isp_reg_write(isp_dev, ISP_CUBIC_LUT65_DATA, 639902752);

	isp_reg_update_bits(isp_dev, ISP_LCGE_FLAT_RATIO_0, lcge_ratio[0], 4, 8);
	isp_reg_update_bits(isp_dev, ISP_LCGE_FLAT_RATIO_1, lcge_ratio[1], 4, 8);
	isp_reg_update_bits(isp_dev, ISP_LCGE_FLAT_RATIO_2, lcge_ratio[2], 4, 8);
	isp_reg_update_bits(isp_dev, ISP_LCGE_FLAT_RATIO_3, lcge_ratio[3], 4, 8);

	lcge_init(isp_dev);
}
