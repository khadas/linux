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

static void post_tnr_cfg_lut(struct isp_dev_t *isp_dev, void *param)
{
	u32 val = 0 ;
	aisp_tnr_cfg_t *tnr_cfg = param;

	isp_hw_lut_wstart(isp_dev, PST_TNR_LUT_CFG);

	val = (tnr_cfg->pst_tnr_alp_lut[0] << 0) |
		(tnr_cfg->pst_tnr_alp_lut[1] << 8) |
		(tnr_cfg->pst_tnr_alp_lut[2] << 16) |
		(tnr_cfg->pst_tnr_alp_lut[3] << 24);
	isp_reg_write(isp_dev, ISP_PST_TNR_ALP_LUT_0, val);

	val = (tnr_cfg->pst_tnr_alp_lut[4] << 0) |
		(tnr_cfg->pst_tnr_alp_lut[5] << 8) |
		(tnr_cfg->pst_tnr_alp_lut[6] << 16) |
		(tnr_cfg->pst_tnr_alp_lut[7] << 24);
	isp_reg_write(isp_dev, ISP_PST_TNR_ALP_LUT_1, val);

	isp_hw_lut_wend(isp_dev);
}

static void post_tnr_cfg_nr(struct isp_dev_t *isp_dev, void *nr)
{
	aisp_nr_cfg_t *nr_cfg = nr;

	if (nr_cfg->pvalid.aisp_tnr)
		post_tnr_cfg_lut(isp_dev, &nr_cfg->tnr_cfg);
}

void isp_post_tnr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_nr)
		post_tnr_cfg_nr(isp_dev, &param->nr_cfg);
}

void isp_post_tnr_init(struct isp_dev_t *isp_dev)
{
	return;
}
