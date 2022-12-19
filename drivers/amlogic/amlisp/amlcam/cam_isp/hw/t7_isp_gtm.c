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

static void gtm_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	u32 i = 0;
	u32 val = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	isp_hw_lut_wstart(isp_dev, GTM_LUT_CFG);

	isp_reg_write(isp_dev, ISP_GTM_LUT129_ADDR, 0);
	for (i = 0; i < 64; i++) {
		val = (lut_cfg->gtm_lut[i * 2 + 0] << 0) | (lut_cfg->gtm_lut[i * 2 + 1] << 12);
		isp_reg_write(isp_dev, ISP_GTM_LUT129_DATA, val);
	}

	val = lut_cfg->gtm_lut[128];
	isp_reg_write(isp_dev, ISP_GTM_LUT129_DATA, val);

	isp_hw_lut_wend(isp_dev);
}

void isp_gtm_req_info(struct isp_dev_t *isp_dev, u32 idx, struct aml_buffer *buff)
{
	return;
}

void isp_gtm_cfg_param(struct isp_dev_t *isp_dev, u32 idx, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_base)
		gtm_cfg_base(isp_dev, &param->base_cfg);
}

void isp_gtm_init(struct isp_dev_t *isp_dev)
{
	u32 i, val = 0;
	u32 stp[] = {8, 9, 11, 12, 13, 14, 14, 14};
	u32 num[] = {5, 4, 3, 3, 3, 3, 4, 5};
	u16 glut3[129] = {0, 97, 132, 159, 181, 200, 217, 233, 247, 261, 273, 285, 297,
			308, 318, 328, 338, 347, 356, 365, 373, 382, 390, 398, 405, 413,
			420, 427, 434, 441, 448, 455, 461, 474, 486, 498, 510, 521, 532,
			543, 554, 564, 574, 584, 593, 603, 612, 621, 630, 664, 697, 727,
			756, 784, 811, 836, 861, 908, 952, 994, 1033, 1071, 1107, 1142,
			1176, 1240, 1300, 1357, 1411, 1463, 1513, 1560, 1606, 1694,
			1776, 1854, 1928, 1999, 2066, 2132, 2194, 2255, 2314, 2371,
			2426, 2480, 2533, 2584, 2634, 2683, 2730, 2777, 2823, 2868,
			2912, 2955, 2998, 3040, 3081, 3121, 3161, 3200, 3239, 3277,
			3314, 3351, 3388, 3424, 3460, 3495, 3530, 3564, 3598, 3631,
			3664, 3697, 3730, 3762, 3794, 3825,3856,3887,3918, 3948,
			3978, 4007, 4037, 4066, 4095};

	val = (256 << 16) | (0 << 0);
	isp_reg_write(isp_dev, ISP_GTM_GAIN, val);

	isp_reg_write(isp_dev, ISP_GTM_OFST, 0);

	isp_reg_write(isp_dev, ISP_GTM_LUT129_ADDR, 0x0);
	for (i = 0; i < (129 >> 1); i++) {
		val = (glut3[i * 2] & 0xfff) | ((glut3[i * 2 + 1] & 0xfff) << 12);
		isp_reg_write(isp_dev, ISP_GTM_LUT129_DATA, val);
	}
	val = (glut3[128] & 0xfff);
	isp_reg_write(isp_dev, ISP_GTM_LUT129_DATA, val);

	val = (num[0] << 0) | (num[1] << 4) |
		(num[2] << 8) | (num[3] << 12) |
		(num[4] << 16) | (num[5] << 20) |
		(num[6] << 24) | (num[7] << 28);
	isp_reg_write(isp_dev, ISP_GTM_LUT_NUM, val);

	val = (stp[0] << 0) | (stp[1] << 4) |
		(stp[2] << 8) | (stp[3] << 12) |
		(stp[4] << 16) | (stp[5] << 20) |
		(stp[6] << 24) | (stp[7] << 28);
	isp_reg_write(isp_dev, ISP_GTM_LUT_STP, val);
}
