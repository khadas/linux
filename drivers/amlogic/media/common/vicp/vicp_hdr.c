// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include <linux/amlogic/media/amvecm/hdr2_ext.h>
#include "vicp_hdr.h"
#include "vicp_hardware.h"

static void dump_hdr_param(struct vicp_hdr_s *vicp_hdr)
{
	int i = 0;

	if (IS_ERR_OR_NULL(vicp_hdr)) {
		vicp_print(VICP_ERROR, "%s: invalid param.\n", __func__);
		return;
	}

	pr_info("###############eotf param###############");
	for (i = 0; i < 140; i += 10)
		pr_info("0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx\n",
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[i],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[i + 1],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[i + 2],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[i + 3],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[i + 4],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[i + 5],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[i + 6],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[i + 7],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[i + 8],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[i + 9]);
	pr_info("0x%llx 0x%llx 0x%llx.\n",
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[140],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[141],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.eotf_lut[142]);

	pr_info("###############oetf param###############");
	for (i = 0; i < 140; i += 10)
		pr_info("0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx\n",
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[i],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[i + 1],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[i + 2],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[i + 3],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[i + 4],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[i + 5],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[i + 6],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[i + 7],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[i + 8],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[i + 9]);
	pr_info("0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx\n",
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[140],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[141],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[142],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[143],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[144],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[145],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[146],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[147],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.oetf_lut[148]);

	pr_info("###############cgain param###############");
	for (i = 0; i < 60; i += 10)
		pr_info("0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx 0x%llx\n",
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[i],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[i + 1],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[i + 2],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[i + 3],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[i + 4],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[i + 5],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[i + 6],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[i + 7],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[i + 8],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[i + 9]);
	pr_info("0x%llx 0x%llx 0x%llx 0x%llx 0x%llx\n",
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[60],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[61],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[62],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[63],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.cgain_lut[64]);

	pr_info("###############ogain param###############");
	for (i = 0; i < 140; i += 10)
		pr_info("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[i],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[i + 1],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[i + 2],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[i + 3],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[i + 4],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[i + 5],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[i + 6],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[i + 7],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[i + 8],
			vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[i + 9]);
	pr_info("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[140],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[141],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[142],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[143],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[144],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[145],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[146],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[147],
		vicp_hdr->hdr_data.hdr_para.hdr_lut_param.ogain_lut[148]);
}

bool vicp_hdr_get_setting(struct vicp_hdr_s *vicp_hdr, struct vframe_s *vf)
{
	if (IS_ERR_OR_NULL(vicp_hdr)) {
		vicp_print(VICP_ERROR, "%s: invalid param.\n", __func__);
		return false;
	}
	vicp_print(VICP_HDR2, "%s:\n", __func__);
	get_hdr_setting(vicp_hdr->hdr_data.module_sel,
			vicp_hdr->hdr_data.hdr_process_select,
			&vicp_hdr->hdr_data.hdr_para,
			HDR_FULL_SETTING);

	return true;
}

unsigned char vicp_hdr_get_pre_post(struct vicp_hdr_s *vicp_hdr)
{
	if (IS_ERR_OR_NULL(vicp_hdr)) {
		vicp_print(VICP_ERROR, "%s: invalid param.\n", __func__);
		return false;
	}

	return (unsigned char)vicp_hdr->hdr_data.pre_post;
}

/* from set_hdr_matrix */
void vicp_hdr_set_matrix(struct vicp_hdr_s *vicp_hdr, enum hdr_matrix_sel mtx_sel)
{
	enum hdr_module_sel module_sel;
	struct hdr_proc_mtx_param_s *pmtx;
	struct hdr_proc_adpscl_param_s *padpscl;
	int c_gain_lim_coef[3];
	int gmut_coef[3][3];
	int gmut_shift;
	int i = 0;

	module_sel = vicp_hdr->hdr_data.module_sel;
	pmtx = &vicp_hdr->hdr_data.hdr_para.hdr_mtx_param;
	padpscl = &vicp_hdr->hdr_data.hdr_para.hdr_adpscl_param;

	if (IS_ERR_OR_NULL(pmtx) || IS_ERR_OR_NULL(padpscl)) {
		vicp_print(VICP_ERROR, "%s: NULL param.\n", __func__);
		return;
	}

	vicp_print(VICP_HDR2, "%s: mtx_sel=%d, module_sel = %d, mtx_only = 0x%x, pscl: %d.\n",
		__func__,
		mtx_sel,
		module_sel,
		pmtx->mtx_only,
		padpscl->adpscl_alpha[0]);

	write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, pmtx->mtx_on, 13, 1);

	if (mtx_sel == HDR_IN_MTX) {
		if (pmtx->mtx_only == MTX_ONLY && !pmtx->mtx_on)
			write_vicp_reg(VID_CMPR_HDR2_MATRIXI_EN_CTRL, 1);
		else
			write_vicp_reg(VID_CMPR_HDR2_MATRIXI_EN_CTRL, pmtx->mtx_on);
		write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, pmtx->mtx_on, 4, 1);
		write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, pmtx->mtx_only, 16, 1);
		write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, 1, 14, 1);

		write_vicp_reg(VID_CMPR_HDR2_MATRIXI_COEF00_01,
			(pmtx->mtx_in[0 * 3 + 0] << 16) | (pmtx->mtx_in[0 * 3 + 1] & 0x1FFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXI_COEF02_10,
			(pmtx->mtx_in[0 * 3 + 2] << 16) | (pmtx->mtx_in[1 * 3 + 0] & 0x1FFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXI_COEF11_12,
			(pmtx->mtx_in[1 * 3 + 1] << 16) | (pmtx->mtx_in[1 * 3 + 2] & 0x1FFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXI_COEF20_21,
			(pmtx->mtx_in[2 * 3 + 0] << 16) | (pmtx->mtx_in[2 * 3 + 1] & 0x1FFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXI_COEF22, pmtx->mtx_in[2 * 3 + 2]);
		write_vicp_reg(VID_CMPR_HDR2_MATRIXI_OFFSET0_1,
			(pmtx->mtxi_pos_offset[0] << 16) | (pmtx->mtxi_pos_offset[1] & 0xFFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXI_OFFSET2, pmtx->mtxi_pos_offset[2]);
		write_vicp_reg(VID_CMPR_HDR2_MATRIXI_PRE_OFFSET0_1,
			(pmtx->mtxi_pre_offset[0] << 16) | (pmtx->mtxi_pre_offset[1] & 0xFFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXI_PRE_OFFSET2, pmtx->mtxi_pre_offset[2]);
	} else if (mtx_sel == HDR_GAMUT_MTX) {
		for (i = 0; i < 9; i++)
			gmut_coef[i / 3][i % 3] = pmtx->mtx_gamut[i];
		if ((pmtx->p_sel & HDR_SDR) ||
			(pmtx->p_sel & HDR10P_SDR) ||
			(pmtx->p_sel & HLG_SDR)) {
			if (pmtx->gmt_bit_mode) {
				gmut_shift = 8;
				gmut_shift |= 1 << 4;
			} else {
				gmut_shift = 0;
			}
		} else {
			gmut_shift = 11;
		}

		for (i = 0; i < 3; i++)
			c_gain_lim_coef[i] = pmtx->mtx_cgain[i] << 2;

		write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, pmtx->mtx_gamut_mode, 6, 2);
		write_vicp_reg(VID_CMPR_HDR2_GMUT_CTRL, gmut_shift);
		write_vicp_reg(VID_CMPR_HDR2_GMUT_COEF0,
			(gmut_coef[0][1] & 0xffff) << 16 | (gmut_coef[0][0] & 0xffff));
		write_vicp_reg(VID_CMPR_HDR2_GMUT_COEF1,
			(gmut_coef[1][0] & 0xffff) << 16 | (gmut_coef[0][2] & 0xffff));
		write_vicp_reg(VID_CMPR_HDR2_GMUT_COEF2,
			(gmut_coef[1][2] & 0xffff) << 16 | (gmut_coef[1][1] & 0xffff));
		write_vicp_reg(VID_CMPR_HDR2_GMUT_COEF3,
			(gmut_coef[2][1] & 0xffff) << 16 | (gmut_coef[2][0] & 0xffff));
		write_vicp_reg(VID_CMPR_HDR2_GMUT_COEF4, gmut_coef[2][2] & 0xffff);

		write_vicp_reg(VID_CMPR_HDR2_CGAIN_COEF0,
			c_gain_lim_coef[1] << 16 | c_gain_lim_coef[0]);
		write_vicp_reg_bits(VID_CMPR_HDR2_CGAIN_COEF1, c_gain_lim_coef[2], 0, 12);

		write_vicp_reg(VID_CMPR_HDR2_ADPS_CTRL,
			padpscl->adpscl_bypass[2] << 6 |
			padpscl->adpscl_bypass[1] << 5 |
			padpscl->adpscl_bypass[0] << 4 |
			padpscl->adpscl_mode);
		write_vicp_reg(VID_CMPR_HDR2_ADPS_ALPHA0,
			padpscl->adpscl_alpha[1] << 16 | padpscl->adpscl_alpha[0]);
		write_vicp_reg(VID_CMPR_HDR2_ADPS_ALPHA1,
			padpscl->adpscl_shift[0] << 24 |
			padpscl->adpscl_shift[1] << 20 |
			padpscl->adpscl_shift[2] << 16 |
			padpscl->adpscl_alpha[2]);
		write_vicp_reg(VID_CMPR_HDR2_ADPS_BETA0,
			padpscl->adpscl_beta_s[0] << 20 | padpscl->adpscl_beta[0]);
		write_vicp_reg(VID_CMPR_HDR2_ADPS_BETA1,
			padpscl->adpscl_beta_s[1] << 20 | padpscl->adpscl_beta[1]);
		write_vicp_reg(VID_CMPR_HDR2_ADPS_BETA2,
			padpscl->adpscl_beta_s[2] << 20 | padpscl->adpscl_beta[2]);
		write_vicp_reg(VID_CMPR_HDR2_ADPS_COEF0,
			padpscl->adpscl_ys_coef[1] << 16 | padpscl->adpscl_ys_coef[0]);
		write_vicp_reg(VID_CMPR_HDR2_ADPS_COEF1, padpscl->adpscl_ys_coef[2]);
	} else if (mtx_sel == HDR_OUT_MTX) {
		write_vicp_reg(VID_CMPR_HDR2_CGAIN_OFFT,
		       (pmtx->mtx_cgain_offset[2] << 16) |
		       pmtx->mtx_cgain_offset[1]);
		write_vicp_reg(VID_CMPR_HDR2_MATRIXO_EN_CTRL, pmtx->mtx_on);
		write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, 0, 17, 1);
		write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, 1, 15, 1);

		write_vicp_reg(VID_CMPR_HDR2_MATRIXO_COEF00_01,
			(pmtx->mtx_out[0 * 3 + 0] << 16) | (pmtx->mtx_out[0 * 3 + 1] & 0x1FFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXO_COEF02_10,
			(pmtx->mtx_out[0 * 3 + 2] << 16) | (pmtx->mtx_out[1 * 3 + 0] & 0x1FFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXO_COEF11_12,
			(pmtx->mtx_out[1 * 3 + 1] << 16) | (pmtx->mtx_out[1 * 3 + 2] & 0x1FFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXO_COEF20_21,
			(pmtx->mtx_out[2 * 3 + 0] << 16) | (pmtx->mtx_out[2 * 3 + 1] & 0x1FFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXO_COEF22, pmtx->mtx_out[2 * 3 + 2]);

		write_vicp_reg(VID_CMPR_HDR2_MATRIXO_OFFSET0_1,
			(pmtx->mtxo_pos_offset[0] << 16) | (pmtx->mtxo_pos_offset[1] & 0xFFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXO_OFFSET2, pmtx->mtxo_pos_offset[2]);
		write_vicp_reg(VID_CMPR_HDR2_MATRIXO_PRE_OFFSET0_1,
			(pmtx->mtxo_pre_offset[0] << 16) | (pmtx->mtxo_pre_offset[1] & 0xFFF));
		write_vicp_reg(VID_CMPR_HDR2_MATRIXO_PRE_OFFSET2, pmtx->mtxo_pre_offset[2]);
	}

	if (mtx_sel == HDR_IN_MTX) {
		vicp_print(VICP_HDR2, "hdr:in_mtx %d;%d; = 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,%x\n",
			pmtx->mtx_on,
			pmtx->mtx_only,
			(pmtx->mtxi_pre_offset[0] << 16) | (pmtx->mtxi_pre_offset[1] & 0xFFF),
			(pmtx->mtx_in[0 * 3 + 0] << 16) | (pmtx->mtx_in[0 * 3 + 1] & 0x1FFF),
			(pmtx->mtx_in[0 * 3 + 2] << 16) | (pmtx->mtx_in[1 * 3 + 0] & 0x1FFF),
			(pmtx->mtx_in[1 * 3 + 1] << 16) | (pmtx->mtx_in[1 * 3 + 2] & 0x1FFF),
			(pmtx->mtx_in[2 * 3 + 0] << 16) | (pmtx->mtx_in[2 * 3 + 1] & 0x1FFF),
			pmtx->mtx_in[2 * 3 + 2],
			(pmtx->mtxi_pos_offset[0] << 16) | (pmtx->mtxi_pos_offset[1] & 0xFFF));
	} else if (mtx_sel == HDR_GAMUT_MTX) {
		vicp_print(VICP_HDR2, "hdr: gamut_mtx %d mode %d shift %d = %x %x %x %x %x\n",
			pmtx->mtx_on,
			pmtx->mtx_gamut_mode,
			gmut_shift,
			(gmut_coef[0][1] & 0xffff) << 16 | (gmut_coef[0][0] & 0xffff),
			(gmut_coef[1][0] & 0xffff) << 16 | (gmut_coef[0][2] & 0xffff),
			(gmut_coef[1][2] & 0xffff) << 16 | (gmut_coef[1][1] & 0xffff),
			(gmut_coef[2][1] & 0xffff) << 16 | (gmut_coef[2][0] & 0xffff),
			gmut_coef[2][2] & 0xffff);
		vicp_print(VICP_HDR2, "hdr: adpscl bypass %d, x_shift %d, y_shift %d\n",
			padpscl->adpscl_bypass[0],
			padpscl->adpscl_shift[0],
			padpscl->adpscl_shift[1]);

	}  else if (mtx_sel == HDR_OUT_MTX) {
		vicp_print(VICP_HDR2, "hdr: out_mtx %d %d = %x,%x %x %x %x %x,%x\n",
			pmtx->mtx_on,
			pmtx->mtx_only,
			(pmtx->mtxo_pre_offset[0] << 16) | (pmtx->mtxo_pre_offset[1] & 0xFFF),
			(pmtx->mtx_out[0 * 3 + 0] << 16) | (pmtx->mtx_out[0 * 3 + 1] & 0x1FFF),
			(pmtx->mtx_out[0 * 3 + 2] << 16) |
			(pmtx->mtx_out[1 * 3 + 0] & 0x1FFF),
			(pmtx->mtx_out[1 * 3 + 1] << 16) | (pmtx->mtx_out[1 * 3 + 2] & 0x1FFF),
			(pmtx->mtx_out[2 * 3 + 0] << 16) | (pmtx->mtx_out[2 * 3 + 1] & 0x1FFF),
			pmtx->mtx_out[2 * 3 + 2],
			(pmtx->mtxo_pos_offset[0] << 16) | (pmtx->mtxo_pos_offset[1] & 0xFFF));
	}
}

/************************************************
 * from set_eotf_lut
 * plut -> hdr_lut_param
 ************************************************/
void vicp_hdr_set_eotf_lut(enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *plut)
{
	unsigned int i = 0;

	vicp_print(VICP_HDR2, "%s: module_sel %d, lut_on %d\n", __func__, module_sel, plut->lut_on);

	if (module_sel != VICP_HDR) {
		vicp_print(VICP_ERROR, "%s:not vicp hdr.\n", __func__);
		return;
	}

	write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, plut->lut_on, 3, 1);

	if (!plut->lut_on)
		return;

	write_vicp_reg(VID_CMPR_HDR2_EOTF_LUT_ADDR_PORT, 0x0);
	for (i = 0; i < HDR2_EOTF_LUT_SIZE; i++)
		write_vicp_reg(VID_CMPR_HDR2_EOTF_LUT_DATA_PORT, (unsigned int)plut->eotf_lut[i]);
}

/************************************************
 * from set_ootf_lut
 * plut -> hdr_lut_param
 ************************************************/
void vicp_hdr_set_ootf_lut(enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *plut)
{
	unsigned int i = 0;

	if (module_sel != VICP_HDR) {
		vicp_print(VICP_ERROR, "%s: not vicp hdr.\n", __func__);
		return;
	}

	vicp_print(VICP_HDR2, "%s: module_sel %d, lut_on %d\n", __func__, module_sel, plut->lut_on);
	write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, plut->lut_on, 1, 1);

	if (!plut->lut_on)
		return;

	write_vicp_reg(VID_CMPR_HDR2_OGAIN_LUT_ADDR_PORT, 0x0);
	for (i = 0; i < HDR2_OOTF_LUT_SIZE / 2; i++)
		write_vicp_reg(VID_CMPR_HDR2_OGAIN_LUT_DATA_PORT,
			((unsigned int)plut->ogain_lut[i * 2 + 1] << 16) +
			(unsigned int)plut->ogain_lut[i * 2]);
	write_vicp_reg(VID_CMPR_HDR2_OGAIN_LUT_DATA_PORT, (unsigned int)plut->ogain_lut[148]);
}

/************************************************
 * from set_oetf_lut
 * plut-> hdr_lut_param
 ***********************************************/
void vicp_hdr_set_oetf_lut(enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *plut)
{
	unsigned int i = 0;
	unsigned int tmp1, tmp2;

	if (module_sel != VICP_HDR) {
		vicp_print(VICP_ERROR, "%s: not vicp hdr.\n", __func__);
		return;
	}

	vicp_print(VICP_HDR2, "%s: module_sel %d, lut_on %d\n", __func__, module_sel, plut->lut_on);
	write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, plut->lut_on, 2, 1);

	if (!plut->lut_on)
		return;

	write_vicp_reg(VID_CMPR_HDR2_OETF_LUT_ADDR_PORT, 0x0);
	for (i = 0; i < HDR2_OETF_LUT_SIZE / 2; i++) {
		tmp1 = (unsigned int)plut->oetf_lut[i * 2 + 1];
		tmp2 = (unsigned int)plut->oetf_lut[i * 2];
		if (plut->bitdepth == 10) {
			tmp1 = tmp1 >> 2;
			tmp2 = tmp2 >> 2;
		}
		write_vicp_reg(VID_CMPR_HDR2_OETF_LUT_DATA_PORT, (tmp1 << 16) + tmp2);
	}
	tmp1 = (unsigned int)plut->oetf_lut[148];
	if (plut->bitdepth == 10)
		write_vicp_reg(VID_CMPR_HDR2_OETF_LUT_DATA_PORT, tmp1 >> 2);
	else
		write_vicp_reg(VID_CMPR_HDR2_OETF_LUT_DATA_PORT, tmp1);
}

/************************************************
 * from set_c_gain
 * plut->hdr_lut_param
 ************************************************/
void vicp_hdr_set_c_gain(enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *plut)
{
	unsigned int i = 0;

	if (module_sel != VICP_HDR) {
		vicp_print(VICP_ERROR, "%s: not vicp hdr.\n", __func__);
		return;
	}

	vicp_print(VICP_HDR2, "%s: module_sel %d, lut_on %d\n", __func__, module_sel, plut->lut_on);
	/*cgain mode: 0->y domin*/
	/*cgain mode: 1->rgb domin, use r/g/b max*/
	write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, 0, 12, 1);
	write_vicp_reg_bits(VID_CMPR_HDR2_CTRL, plut->cgain_en, 0, 1);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_SM1)) {
		if (plut->bitdepth == 10)
			write_vicp_reg_bits(VID_CMPR_HDR2_CGAIN_COEF1, 0x400, 16, 13);
		else if (plut->bitdepth == 12)
			write_vicp_reg_bits(VID_CMPR_HDR2_CGAIN_COEF1, 0x1000, 16, 13);
	}

	if (!plut->cgain_en)
		return;

	write_vicp_reg(VID_CMPR_HDR2_CGAIN_LUT_ADDR_PORT, 0x0);
	for (i = 0; i < HDR2_CGAIN_LUT_SIZE / 2; i++)
		write_vicp_reg(VID_CMPR_HDR2_CGAIN_LUT_DATA_PORT,
			((unsigned int)plut->cgain_lut[i * 2 + 1] << 16) +
			(unsigned int)plut->cgain_lut[i * 2]);
	write_vicp_reg(VID_CMPR_HDR2_CGAIN_LUT_DATA_PORT, (unsigned int)plut->cgain_lut[64]);
}

/************************************************
 * hdr_hist_config
 * plut -> hdr_lut_param
 ************************************************/
void vicp_hdr_hist_config(enum hdr_module_sel module_sel,
	struct hdr_proc_lut_param_s *plut)
{
	unsigned int hist_ctrl;
	unsigned int hist_hs_he;
	unsigned int hist_vs_ve;

	if (module_sel == VICP_HDR) {
		hist_ctrl = VID_CMPR_HDR2_HIST_CTRL;
		hist_hs_he = VID_CMPR_HDR2_HIST_H_START_END;
		hist_vs_ve = VID_CMPR_HDR2_HIST_V_START_END;
	} else {
		vicp_print(VICP_ERROR, "%s: not vicp hdr.\n", __func__);
		return;
	}

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_TM2)
		return;

	if (plut->hist_en) {
		write_vicp_reg(hist_ctrl, 0);
		write_vicp_reg(hist_hs_he, 0xeff);
		write_vicp_reg(hist_vs_ve, 0x86f);
	} else {
		write_vicp_reg(hist_ctrl, 0x5510);
		write_vicp_reg(hist_hs_he, 0x10000);
		write_vicp_reg(hist_vs_ve, 0x0);
	}
}

void vicp_hdr_set(struct vicp_hdr_s *vicp_hdr, int pre_post)
{
	enum hdr_module_sel module_sel;

	if (IS_ERR_OR_NULL(vicp_hdr)) {
		vicp_print(VICP_ERROR, "%s: invalid param.\n", __func__);
		return;
	}

	if (print_flag & VICP_HDR2)
		dump_hdr_param(vicp_hdr);

	module_sel = vicp_hdr->hdr_data.module_sel;

	vicp_print(VICP_HDR2, "%s: pre_post %d, module_sel %d.\n", __func__, pre_post, module_sel);

	vicp_hdr_set_matrix(vicp_hdr, HDR_IN_MTX);

	vicp_hdr_set_eotf_lut(module_sel, &vicp_hdr->hdr_data.hdr_para.hdr_lut_param);

	vicp_hdr_set_matrix(vicp_hdr, HDR_GAMUT_MTX);

	vicp_hdr_set_ootf_lut(module_sel, &vicp_hdr->hdr_data.hdr_para.hdr_lut_param);

	vicp_hdr_set_oetf_lut(module_sel, &vicp_hdr->hdr_data.hdr_para.hdr_lut_param);

	vicp_hdr_set_matrix(vicp_hdr, HDR_OUT_MTX);

	vicp_hdr_set_c_gain(module_sel, &vicp_hdr->hdr_data.hdr_para.hdr_lut_param);

	vicp_hdr_hist_config(module_sel, &vicp_hdr->hdr_data.hdr_para.hdr_lut_param);
	vicp_hdr->hdr_data.n_update = false;
}

bool vicp_hdr_init(struct vicp_hdr_s *vicp_hdr)
{
	if (IS_ERR_OR_NULL(vicp_hdr)) {
		vicp_print(VICP_ERROR, "%s: invalid param.\n", __func__);
		return false;
	}

	vicp_hdr->hdr_data.module_sel = VICP_HDR;
	vicp_hdr->hdr_data.para_done = false;
	vicp_hdr->hdr_data.enable = 1;
	vicp_hdr->hdr_data.hdr_process_select = HDR_SDR;

	return true;
}

bool vicp_hdr_uninit(struct vicp_hdr_s *vicp_hdr)
{
	if (IS_ERR_OR_NULL(vicp_hdr)) {
		vicp_print(VICP_ERROR, "%s: invalid param.\n", __func__);
		return false;
	}
	memset(&vicp_hdr->hdr_data, 0, sizeof(struct vicp_hdr_data_s));
	return true;
}

struct vicp_hdr_s *vicp_hdr_prob(void)
{
	struct vicp_hdr_s *hdr;
	size_t size = 0;

	size = sizeof(struct vicp_hdr_s);
	hdr = vmalloc(size);
	if (IS_ERR_OR_NULL(hdr)) {
		vicp_print(VICP_ERROR, "%s: vmalloc.\n", __func__);
		vfree(hdr);
		return NULL;
	}

	memset(hdr, 0, size);
	vicp_hdr_init(hdr);
	vicp_hdr_get_setting(hdr, NULL);

	return hdr;
}

void vicp_hdr_remove(struct vicp_hdr_s *vicp_hdr)
{
	if (IS_ERR_OR_NULL(vicp_hdr))
		return;

	vicp_hdr_uninit(vicp_hdr);

	vfree(vicp_hdr);
	vicp_hdr = NULL;

	vicp_print(VICP_HDR2, "%s.\n", __func__);
}
