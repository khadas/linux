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

static void post_gamma_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	int i, j;
	u32 idx0, idx1;
	u32 val = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	isp_hw_lut_wstart(isp_dev, PST_GAMMA_LUT_CFG);

	for (j = 0; j < 4; j++) {
		isp_reg_write(isp_dev, ISP_PST_GAMMA_LUT_ADDR, j << 7);
		for (i = 0; i < 64; i++) {
			idx0 = j * 129 + 2 * i + 0;
			idx1 = j * 129 + 2 * i + 1;

			val = (lut_cfg->pst_gamma_lut[idx0] << 0) |
				(lut_cfg->pst_gamma_lut[idx1] << 16);
			isp_reg_write(isp_dev, ISP_PST_GAMMA_LUT_DATA, val);
		}

		idx0 = j * 129 + 128;
		val = lut_cfg->pst_gamma_lut[idx0];
		isp_reg_write(isp_dev, ISP_PST_GAMMA_LUT_DATA, val);
	}

	isp_hw_lut_wend(isp_dev);
}

static void post_ccm_cfg_param(struct isp_dev_t *isp_dev, void *param)
{

	u32 val = 0;
	aisp_ccm_cfg_t *c_cfg = param;

	val = ((c_cfg->ccm_4x3matrix[0][0] << 0) & 0x1fff) |
		((c_cfg->ccm_4x3matrix[0][1] << 16) & 0x1fff0000);
	isp_reg_write(isp_dev, ISP_CCM_MTX_00_01, val);

	val = ((c_cfg->ccm_4x3matrix[0][2] << 0) & 0x1fff) |
		((c_cfg->ccm_4x3matrix[0][3] << 16) & 0x1fff0000);
	isp_reg_write(isp_dev, ISP_CCM_MTX_02_03, val);

	val = ((c_cfg->ccm_4x3matrix[1][0] << 0) & 0x1fff) |
		((c_cfg->ccm_4x3matrix[1][1] << 16) & 0x1fff0000);
	isp_reg_write(isp_dev, ISP_CCM_MTX_10_11, val);

	val = ((c_cfg->ccm_4x3matrix[1][2] << 0) & 0x1fff) |
		((c_cfg->ccm_4x3matrix[1][3] << 16) & 0x1fff0000);
	isp_reg_write(isp_dev, ISP_CCM_MTX_12_13, val);

	val = ((c_cfg->ccm_4x3matrix[2][0] << 0) & 0x1fff) |
		((c_cfg->ccm_4x3matrix[2][1] << 16) & 0x1fff0000);
	isp_reg_write(isp_dev, ISP_CCM_MTX_20_21, val);

	val = (c_cfg->ccm_4x3matrix[2][2] << 0) & 0x1fff;
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_22_23_RS, val, 0, 13);

	val = (c_cfg->ccm_4x3matrix[2][3] << 0) & 0x1fff;
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_22_23_RS, val, 16, 13);
#ifdef NO_VERIFIED
	val = c_cfg->csc3_en;
	isp_reg_update_bits(isp_dev, DISP1_TOP_TOP_CTRL, val, 1, 1);

	val = (c_cfg->csc1_offset_inp[1] & 0x1fff) |
		((c_cfg->csc1_offset_inp[0] << 16) & 0x1fff0000);
	isp_reg_write(isp_dev, DISP0_CSC2_OFFSET_INP01, val);

	val = (c_cfg->csc1_offset_inp[2] & 0x1fff);
	isp_reg_write(isp_dev, DISP0_CSC2_OFFSET_INP2, val);

	val = (c_cfg->csc1_offset_oup[1] & 0x1fff) |
		((c_cfg->csc1_offset_oup[0] << 16) & 0x1fff0000);
	isp_reg_write(isp_dev, DISP0_CSC2_OFFSET_OUP01, val);

	val = ((c_cfg->csc1_offset_oup[2] << 16) & 0x1fff) |
			(c_cfg->csc1_3x3mtrx_rs & 0x7);
	isp_reg_write(isp_dev, DISP0_CSC2_OFFSET_OUP2, val);

	val = ((c_cfg->csc1_3x3matrix[0][1] << 0) & 0x1fff) |
		((c_cfg->csc1_3x3matrix[0][0] << 16) & 0x1fff0000);
	isp_reg_write(isp_dev, DISP0_CSC2_COEF_00_01, val);

	val = ((c_cfg->csc1_3x3matrix[1][0] << 0) & 0x1fff) |
		((c_cfg->csc1_3x3matrix[0][2] << 16) & 0x1fff0000);
	isp_reg_write(isp_dev, DISP0_CSC2_COEF_02_10, val);

	val = ((c_cfg->csc1_3x3matrix[1][2] << 0) & 0x1fff) |
		((c_cfg->csc1_3x3matrix[1][1] << 16) & 0x1fff0000);
	isp_reg_write(isp_dev, DISP0_CSC2_COEF_11_12, val);

	val = ((c_cfg->csc1_3x3matrix[2][1] << 0) & 0x1fff) |
		((c_cfg->csc1_3x3matrix[2][0] << 16) & 0x1fff0000);
	isp_reg_write(isp_dev, DISP0_CSC2_COEF_20_21, val);

	val = ((c_cfg->csc1_3x3matrix[2][2] << 0) & 0x1fff);
	isp_reg_write(isp_dev, DISP0_CSC2_COEF_22, val);
#endif
}

static void post_hlc_cfg_param(struct isp_dev_t *isp_dev, void *param)
{

	u32 val = 0;
	aisp_hlc_cfg_t *h_cfg = param;

	val = h_cfg->hlc_en & 0x1;
	isp_reg_update_bits(isp_dev, ISP_CVR_RECT_EN, val, 2, 1);

	val = h_cfg->hlc_luma_thd & 0x3ff;
	isp_reg_update_bits(isp_dev, ISP_CVR_RECT_EN, val, 4, 10);

	val = h_cfg->hlc_luma_trgt & 0x3ff;
	isp_reg_update_bits(isp_dev, ISP_CVR_RECT_EN, val, 16, 10);
}

static void post_cvr_cfg_param(struct isp_dev_t *isp_dev, void *param)
{

	u32 val = 0;
	u32 i;
	aisp_cvr_cfg_t *c_cfg = param;

	val = c_cfg->cvr_rect_en & 0x1;
	isp_reg_update_bits(isp_dev, ISP_CVR_RECT_EN, val, 0, 1);

	for (i = 0; i < 8; i++) {
		val =  (c_cfg->cvr_rect_vstart[i] << 0) |
			(c_cfg->cvr_rect_hstart[i] << 16);
		isp_reg_write(isp_dev, ISP_CVR_RECT_HVSTART_0 + 0xc * i, val);

		val =  (c_cfg->cvr_rect_vend[i] << 0) |
			(c_cfg->cvr_rect_hend[i] << 16);
		isp_reg_write(isp_dev, ISP_CVR_RECT_HVSIZE_0 + 0xc * i, val);

		val =  (c_cfg->cvr_rect_val_v[i] << 0) |
			(c_cfg->cvr_rect_val_u[i] << 8) |
			(c_cfg->cvr_rect_val_y[i] << 16);
		isp_reg_write(isp_dev, ISP_CVR_RECT_YUV_0 + 0xc * i, val);
	}
}

static void post_csc_cfg_param(struct isp_dev_t *isp_dev, void *param)
{
	u32 val = 0;

	aisp_csc_cfg_t *c_cfg = param;

	val = (c_cfg->cm0_offset_inp[0]) |
		(c_cfg->cm0_offset_inp[1] << 16);
	isp_reg_write(isp_dev, ISP_CM0_INP_OFST01, val);

	val = (c_cfg->cm0_offset_inp[2]);
	isp_reg_write(isp_dev, ISP_CM0_INP_OFST2, val);

	val = (c_cfg->cm0_3x3matrix[2][2] & 0xFFFF) |
		((c_cfg->cm0_offset_oup[0] << 16) & 0xFFFF0000);
	isp_reg_write(isp_dev, ISP_CM0_COEF22_OUP_OFST0, val);

	val = (c_cfg->cm0_offset_oup[1] & 0xFFFF) |
		((c_cfg->cm0_offset_oup[2] << 16) & 0xFFFF0000);
	isp_reg_update_bits(isp_dev, ISP_CM0_OUP_OFST12_RS, val, 0, 29);

	val = c_cfg->cm0_3x3mtrx_rs & 0x3;
	isp_reg_update_bits(isp_dev, ISP_CM0_OUP_OFST12_RS, val, 30, 2);

	val = (c_cfg->cm0_3x3matrix[0][0] & 0xFFFF) |
		((c_cfg->cm0_3x3matrix[0][1] << 16) & 0xFFFF0000);
	isp_reg_write(isp_dev, ISP_CM0_COEF00_01, val);

	val = (c_cfg->cm0_3x3matrix[0][2] & 0xFFFF) |
		((c_cfg->cm0_3x3matrix[1][0] << 16) & 0xFFFF0000);
	isp_reg_write(isp_dev, ISP_CM0_COEF02_10, val);

	val = (c_cfg->cm0_3x3matrix[1][1] & 0xFFFF) |
		((c_cfg->cm0_3x3matrix[1][2] << 16) & 0xFFFF0000);
	isp_reg_write(isp_dev, ISP_CM0_COEF11_12, val);

	val = (c_cfg->cm0_3x3matrix[2][0] & 0xFFFF) |
		((c_cfg->cm0_3x3matrix[2][1] << 16) & 0xFFFF0000);
	isp_reg_write(isp_dev, ISP_CM0_COEF20_21, val);
}

void isp_post_pg0_top_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_base)
		post_gamma_cfg_base(isp_dev, &param->base_cfg);

	if (param->pvalid.aisp_ccm)
		post_ccm_cfg_param(isp_dev, &param->ccm_cfg);

	if (param->pvalid.aisp_hlc)
		post_hlc_cfg_param(isp_dev, &param->hlc_cfg);

	if (param->pvalid.aisp_cvr)
		post_cvr_cfg_param(isp_dev, &param->cvr_cfg);

	if (param->pvalid.aisp_csc)
		post_csc_cfg_param(isp_dev, &param->csc_cfg);
}

void isp_post_pg0_top_init(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_00_01, 393, 0, 13);
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_00_01, -88, 16, 13);
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_02_03, -49, 0, 13);
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_02_03, 0, 16, 13);
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_10_11, -49, 0, 13);
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_10_11, 387, 16, 13);
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_12_13, -82, 0, 13);
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_12_13, 0, 16, 13);
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_20_21, 9, 0, 13);
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_20_21, -119, 16, 13);
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_22_23_RS, 366, 0, 13);
	isp_reg_update_bits(isp_dev, ISP_CCM_MTX_22_23_RS, 0, 16, 13);
	isp_reg_update_bits(isp_dev, ISP_CM0_OUP_OFST12_RS, 2, 30, 2);

	return;
}
