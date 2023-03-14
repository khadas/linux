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

static void patgen_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	int i, j;
	u32 addr = 0;
	u32 val = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 12; i++) {
			val = (lut_cfg->pat_bar24rgb[0][j][i * 2] << 0) |
				(lut_cfg->pat_bar24rgb[0][j][i * 2 + 1] << 16);

			addr = ISP_PAT0_BAR24_R01 + j * 12 * 4 + i * 4;
			isp_reg_write(isp_dev, addr, val);
		}
	}

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 12; i++) {
			val = (lut_cfg->pat_bar24rgb[1][j][i * 2] << 0) |
				(lut_cfg->pat_bar24rgb[1][j][i * 2 + 1] << 16);

			addr = ISP_PAT1_BAR24_R01 + j * 12 * 4 + i * 4;
			isp_reg_write(isp_dev, addr, val);
		}
	}
}

static void patgen_cfg_misc(struct isp_dev_t *isp_dev, void *misc)
{
	aisp_misc_cfg_t *m_cfg = misc;
	aisp_pat_cfg_t *p_cfg = &m_cfg->pat_cfg;

	isp_reg_update_bits(isp_dev, ISP_PAT0_FWIN_R, p_cfg->pat_fwin_en_0, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_PAT0_CTRL, p_cfg->pat_xmode_0, 5, 3);
	isp_reg_update_bits(isp_dev, ISP_PAT0_CTRL, p_cfg->pat_ymode_0, 2, 3);
	isp_reg_update_bits(isp_dev, ISP_PAT0_CTRL, p_cfg->pat_xinvt_0, 1, 1);
	isp_reg_update_bits(isp_dev, ISP_PAT0_CTRL, p_cfg->pat_yinvt_0, 0, 1);

	isp_reg_update_bits(isp_dev, ISP_PAT1_FWIN_R, p_cfg->pat_fwin_en_1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_PAT1_CTRL, p_cfg->pat_xmode_1, 5, 3);
	isp_reg_update_bits(isp_dev, ISP_PAT1_CTRL, p_cfg->pat_ymode_1, 2, 3);
	isp_reg_update_bits(isp_dev, ISP_PAT1_CTRL, p_cfg->pat_xinvt_1, 1, 1);
	isp_reg_update_bits(isp_dev, ISP_PAT1_CTRL, p_cfg->pat_yinvt_1, 0, 1);
}

void isp_timgen_enable(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_TOP_FRM_END_MASK, 0x3, 4, 2);
	isp_reg_update_bits(isp_dev, ISP_TOP_MEAS, 1, 15, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_MEAS, 1, 16, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_MEAS, 1, 17, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_CTRL0, 0, 6, 2);
	isp_reg_update_bits(isp_dev, ISP_TOP_MEAS, 1, 18, 1);
}

void isp_timgen_disable(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_TOP_MEAS, 0, 18, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_MEAS, 1, 17, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_MEAS, 1, 15, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_MEAS, 0, 16, 1);
}

void isp_timgen_cfg(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;

	val = (fmt->height << 16) | (fmt->width << 0);
	isp_reg_write(isp_dev, ISP_TOP_TIMGEN, val);

	val = (0 << 16) | (2 << 0);
	isp_reg_write(isp_dev, ISP_TOP_TIMGEN_REF, val);
}

void isp_patgen_enable(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_0, 1, 4, 1);
}

void isp_patgen_disable(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_0, 0, 4, 1);
}

void isp_patgen_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
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

	isp_reg_update_bits(isp_dev, ISP_PAT0_CTRL, xofst, 12, 2);
	isp_reg_update_bits(isp_dev, ISP_PAT0_CTRL, yofst, 10, 2);

	isp_reg_update_bits(isp_dev, ISP_PAT1_CTRL, xofst, 12, 2);
	isp_reg_update_bits(isp_dev, ISP_PAT1_CTRL, yofst, 10, 2);
}

void isp_patgen_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	return;
}

void isp_patgen_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_base)
		patgen_cfg_base(isp_dev, &param->base_cfg);

	if (param->pvalid.aisp_misc)
		patgen_cfg_misc(isp_dev, &param->misc_cfg);
}

void isp_patgen_init(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_PAT0_IDX_SCL, 34, 16, 12);
	isp_reg_update_bits(isp_dev, ISP_PAT0_IDX_SCL, 60, 4, 12);

	isp_reg_update_bits(isp_dev, ISP_PAT1_IDX_SCL, 34, 16, 12);
	isp_reg_update_bits(isp_dev, ISP_PAT1_IDX_SCL, 60, 4, 12);

	return;
}
