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

static void pk_cfg_cnr(struct isp_dev_t *isp_dev, void *cnr)
{
	int i = 0;
	u32 val = 0;
	aisp_cnr_cfg_t *c_cfg = cnr;

	isp_hw_lut_wstart(isp_dev, PK_CNR_LUT_CFG);

	isp_reg_write(isp_dev, CNR_SATUR_BLK_LUT_IDX, 0);
	for (i = 0; i < 128; i++) {
		val = (c_cfg->cnr2_satur_blk[i * 8 + 0] << 0) |
			(c_cfg->cnr2_satur_blk[i * 8 + 1] << 4) |
			(c_cfg->cnr2_satur_blk[i * 8 + 2] << 8) |
			(c_cfg->cnr2_satur_blk[i * 8 + 3] << 12) |
			(c_cfg->cnr2_satur_blk[i * 8 + 4] << 16) |
			(c_cfg->cnr2_satur_blk[i * 8 + 5] << 20) |
			(c_cfg->cnr2_satur_blk[i * 8 + 6] << 24) |
			(c_cfg->cnr2_satur_blk[i * 8 + 7] << 28);
		isp_reg_write(isp_dev, CNR_SATUR_BLK_LUT_DATA, val);
	}

	for (i = 0; i < 4; i++) {
		val = (c_cfg->cnr2_tdif_rng_lut[i * 4 + 0] << 0) |
			(c_cfg->cnr2_tdif_rng_lut[i * 4 + 1] << 8) |
			(c_cfg->cnr2_tdif_rng_lut[i * 4 + 2] << 16) |
			(c_cfg->cnr2_tdif_rng_lut[i * 4 + 3] << 24);
		isp_reg_write(isp_dev, CNR_TDIF_RNG_LUT_0 + i * 4, val);
	}

	for (i = 0; i < 4; i++) {
		val = (c_cfg->cnr2_ydif_rng_lut[i * 4 + 0] << 0) |
			(c_cfg->cnr2_ydif_rng_lut[i * 4 + 1] << 8) |
			(c_cfg->cnr2_ydif_rng_lut[i * 4 + 2] << 16) |
			(c_cfg->cnr2_ydif_rng_lut[i * 4 + 3] << 24);
		isp_reg_write(isp_dev, CNR_YDIF_RNG_LUT_0 + i * 4, val);
	}

	val = (c_cfg->cnr2_umargin_up << 16) |
		(c_cfg->cnr2_umargin_dw << 0);
	isp_reg_write(isp_dev, CNR_UMARGIN, val);

	val = (c_cfg->cnr2_vmargin_up << 16) |
		(c_cfg->cnr2_vmargin_dw << 0);
	isp_reg_write(isp_dev, CNR_VMARGIN, val);

	val = c_cfg->cnr2_luma_osat_thd;
	isp_reg_update_bits(isp_dev, CNR_CTRST_FRG_ALP, val, 8, 8);

	val = c_cfg->cnr2_fin_alp_mode;
	isp_reg_update_bits(isp_dev, CNR_HS_DBG_MISC, val, 8, 2);

	val = c_cfg->cnr2_ctrst_xthd;
	isp_reg_update_bits(isp_dev, CNR_CTRST_XTH_K, val, 16, 8);

	val = c_cfg->cnr2_adp_desat_vrt;
	isp_reg_update_bits(isp_dev, CNR_HS_DBG_MISC, val, 20, 2);

	val = c_cfg->cnr2_adp_desat_hrz;
	isp_reg_update_bits(isp_dev, CNR_HS_DBG_MISC, val, 24, 2);

	isp_hw_lut_wend(isp_dev);
}

static void pk_cfg_pst_nr(struct isp_dev_t *isp_dev, void *param)
{
	u32 val = 0;
	aisp_snr_cfg_t *s_cfg = param;

	val = s_cfg->nry_strength;
	isp_reg_update_bits(isp_dev, ISP_POST_NRY_GAU_FILTER, val, 0, 6);

	val = s_cfg->nrc_strength;
	isp_reg_update_bits(isp_dev, ISP_POST_NRC_GAU_FILTER, val, 0, 6);
}

static void pk_cfg_nr(struct isp_dev_t *isp_dev, void *nr)
{
	aisp_nr_cfg_t *nr_cfg = nr;

	if (nr_cfg->pvalid.aisp_cnr)
		pk_cfg_cnr(isp_dev, &nr_cfg->cnr_cfg);

	if (nr_cfg->pvalid.aisp_snr)
		pk_cfg_pst_nr(isp_dev, &nr_cfg->snr_cfg);
}

static void pk_cfg_peaking(struct isp_dev_t *isp_dev, void *peaking)
{
	u32 val = 0;
	aisp_peaking_cfg_t *p_cfg = peaking;

	isp_reg_update_bits(isp_dev, ISP_POST_PK_DEBUG, p_cfg->pk_debug_edge, 20, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_DRT_EN_CTL, p_cfg->drtlpf_theta_min_idx_replace, 16, 1);

	if (isp_dev->mcnr_en)
		isp_reg_update_bits(isp_dev, ISP_PK_MOTION_ADP_CTRL, p_cfg->pk_motion_adp_en, 0, 1);
	else
		isp_reg_update_bits(isp_dev, ISP_PK_MOTION_ADP_CTRL, 0, 0, 1);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_FINAL_GAIN, p_cfg->bp_final_gain, 0, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_FINAL_GAIN, p_cfg->hp_final_gain, 8, 15);

	isp_reg_update_bits(isp_dev, ISP_PST_PRE_STRENGTH, p_cfg->pre_flt_strength, 8, 6);

	val = (p_cfg->hp_motion_adp_gain_lut[0] << 0) |
		(p_cfg->hp_motion_adp_gain_lut[1] << 8) |
		(p_cfg->hp_motion_adp_gain_lut[2] << 16) |
		(p_cfg->hp_motion_adp_gain_lut[3] << 24);
	isp_reg_write(isp_dev, ISP_HP_MOTION_ADP_GAIN_LUT_0, val);

	val = (p_cfg->hp_motion_adp_gain_lut[4] << 0) |
		(p_cfg->hp_motion_adp_gain_lut[5] << 8) |
		(p_cfg->hp_motion_adp_gain_lut[6] << 16) |
		(p_cfg->hp_motion_adp_gain_lut[7] << 24);
	isp_reg_write(isp_dev, ISP_HP_MOTION_ADP_GAIN_LUT_1, val);

	val = (p_cfg->bp_motion_adp_gain_lut[0] << 0) |
		(p_cfg->bp_motion_adp_gain_lut[1] << 8) |
		(p_cfg->bp_motion_adp_gain_lut[2] << 16) |
		(p_cfg->bp_motion_adp_gain_lut[3] << 24);
	isp_reg_write(isp_dev, ISP_BP_MOTION_ADP_GAIN_LUT_0, val);

	val = (p_cfg->bp_motion_adp_gain_lut[4] << 0) |
		(p_cfg->bp_motion_adp_gain_lut[5] << 8) |
		(p_cfg->bp_motion_adp_gain_lut[6] << 16) |
		(p_cfg->bp_motion_adp_gain_lut[7] << 24);
	isp_reg_write(isp_dev, ISP_BP_MOTION_ADP_GAIN_LUT_1, val);

	val = (p_cfg->pk_cir_hp_con2gain[0] << 24) |
		(p_cfg->pk_cir_hp_con2gain[1] << 16) |
		(192 << 8) | (128 << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_HP_GAIN, val);

	val = (p_cfg->pk_cir_hp_con2gain[2] << 16) |
		(p_cfg->pk_cir_hp_con2gain[3] << 8) |
		(p_cfg->pk_cir_hp_con2gain[4] << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_HP_GAIN_1, val);

	val = (p_cfg->pk_cir_bp_con2gain[0] << 24) |
		(p_cfg->pk_cir_bp_con2gain[1] << 16) |
		(192 << 8) | (128 << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_BP_GAIN, val);

	val = (p_cfg->pk_cir_bp_con2gain[2] << 16) |
		(p_cfg->pk_cir_bp_con2gain[3] << 8) |
		(p_cfg->pk_cir_bp_con2gain[4] << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_BP_GAIN_1, val);

	val = (p_cfg->pk_drt_hp_con2gain[0] << 24) |
		(p_cfg->pk_drt_hp_con2gain[1] << 16) |
		(192 << 8) | (128 << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_HP_GAIN, val);

	val = (p_cfg->pk_drt_hp_con2gain[2] << 16) |
		(p_cfg->pk_drt_hp_con2gain[3] << 8) |
		(p_cfg->pk_drt_hp_con2gain[4] << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_HP_GAIN_1, val);

	val = (p_cfg->pk_drt_bp_con2gain[0] << 24) |
		(p_cfg->pk_drt_bp_con2gain[1] << 16) |
		(192 << 8) | (128 << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_BP_GAIN, val);

	val = (p_cfg->pk_drt_bp_con2gain[2] << 16) |
		(p_cfg->pk_drt_bp_con2gain[3] << 8) |
		(p_cfg->pk_drt_bp_con2gain[4] << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_BP_GAIN_1, val);

	val = (p_cfg->pkgain_vsluma_lut[0] << 0) |
		(p_cfg->pkgain_vsluma_lut[1] << 4) |
		(p_cfg->pkgain_vsluma_lut[2] << 8) |
		(p_cfg->pkgain_vsluma_lut[3] << 12) |
		(p_cfg->pkgain_vsluma_lut[4] << 16) |
		(p_cfg->pkgain_vsluma_lut[5] << 20) |
		(p_cfg->pkgain_vsluma_lut[6] << 24) |
		(p_cfg->pkgain_vsluma_lut[7] << 28);
	isp_reg_write(isp_dev, ISP_POST_PK_LUMA_GAIN_LUT, val);

	val = p_cfg->pkgain_vsluma_lut[8];
	isp_reg_write(isp_dev, ISP_POST_PK_LUMA_GAIN_LUT_1, val);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_OSHT, p_cfg->pk_os_up, 20, 10);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_OSHT, p_cfg->pk_os_down, 8, 10);

	isp_reg_update_bits(isp_dev, ISP_PST_PRE_BPC_THRD_MARGIN, p_cfg->pre_bpc_margin, 0, 10);

	val = (p_cfg->pk_bpf_vdtap05[0] & 0xFF)  |
		((p_cfg->pk_bpf_vdtap05[1] << 8) & 0xFF00) |
		((p_cfg->pk_bpf_vdtap05[2] << 16) & 0xFF0000) |
		((p_cfg->pk_bpf_hztap09[4] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_FILT_BP_LUT_1, val);

	val = (p_cfg->pk_hpf_vdtap05[0] & 0xFF)  |
		((p_cfg->pk_hpf_vdtap05[1] << 8) & 0xFF00) |
		((p_cfg->pk_hpf_vdtap05[2] << 16) & 0xFF0000) |
		((p_cfg->pk_hpf_hztap09[4] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_FILT_HP_LUT_1, val);

	val = (p_cfg->pk_bpf_hztap09[0] & 0xFF)  |
		((p_cfg->pk_bpf_hztap09[1] << 8) & 0xFF00) |
		((p_cfg->pk_bpf_hztap09[2] << 16) & 0xFF0000) |
		((p_cfg->pk_bpf_hztap09[3] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_FILT_BP_LUT, val);

	val = (p_cfg->pk_hpf_hztap09[0] & 0xFF)  |
		((p_cfg->pk_hpf_hztap09[1] << 8) & 0xFF00) |
		((p_cfg->pk_hpf_hztap09[2] << 16) & 0xFF0000) |
		((p_cfg->pk_hpf_hztap09[3] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_FILT_HP_LUT, val);

	val = (p_cfg->pk_circ_bpf_2d5x7[0][0] & 0xFF)  |
		((p_cfg->pk_circ_bpf_2d5x7[0][1] << 8) & 0xFF00) |
		((p_cfg->pk_circ_bpf_2d5x7[0][2] << 16) & 0xFF0000) |
		((p_cfg->pk_circ_bpf_2d5x7[0][3] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_0, val);

	val = (p_cfg->pk_circ_bpf_2d5x7[1][0] & 0xFF)  |
		((p_cfg->pk_circ_bpf_2d5x7[1][1] << 8) & 0xFF00) |
		((p_cfg->pk_circ_bpf_2d5x7[1][2] << 16) & 0xFF0000) |
		((p_cfg->pk_circ_bpf_2d5x7[1][3] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_1, val);

	val = (p_cfg->pk_circ_bpf_2d5x7[2][0] & 0xFF)  |
		((p_cfg->pk_circ_bpf_2d5x7[2][1] << 8) & 0xFF00) |
		((p_cfg->pk_circ_bpf_2d5x7[2][2] << 16) & 0xFF0000) |
		((p_cfg->pk_circ_bpf_2d5x7[2][3] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_2, val);

	val = (p_cfg->pk_circ_hpf_2d5x7[0][0] & 0xFF)  |
		((p_cfg->pk_circ_hpf_2d5x7[0][1] << 8) & 0xFF00) |
		((p_cfg->pk_circ_hpf_2d5x7[0][2] << 16) & 0xFF0000) |
		((p_cfg->pk_circ_hpf_2d5x7[0][3] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_0, val);

	val = (p_cfg->pk_circ_hpf_2d5x7[1][0] & 0xFF)  |
		((p_cfg->pk_circ_hpf_2d5x7[1][1] << 8) & 0xFF00) |
		((p_cfg->pk_circ_hpf_2d5x7[1][2] << 16) & 0xFF0000) |
		((p_cfg->pk_circ_hpf_2d5x7[1][3] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_1, val);

	val = (p_cfg->pk_circ_hpf_2d5x7[2][0] & 0xFF)  |
		((p_cfg->pk_circ_hpf_2d5x7[2][1] << 8) & 0xFF00) |
		((p_cfg->pk_circ_hpf_2d5x7[2][2] << 16) & 0xFF0000) |
		((p_cfg->pk_circ_hpf_2d5x7[2][3] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_2, val);

	val = (p_cfg->ltm_shrp_base_alpha << 2) |
		(p_cfg->ltm_shrp_r_u6 << 8) |
		(p_cfg->ltm_shrp_s_u8 << 16) |
		(p_cfg->ltm_shrp_smth_lvlsft << 24);
	val = val >> 2;
	isp_reg_update_bits(isp_dev, ISP_LTM_SHRP_CRTL, val, 2, 30);

	val = p_cfg->pk_osh_winsize;
	isp_reg_update_bits(isp_dev, ISP_POST_PK_OSHT, val, 4, 2);

	val = p_cfg->pk_osv_winsize;
	isp_reg_update_bits(isp_dev, ISP_POST_PK_OSHT, val, 0, 2);

}

void isp_pk_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_nr)
		pk_cfg_nr(isp_dev, &param->nr_cfg);

	if (param->pvalid.aisp_peaking)
		pk_cfg_peaking(isp_dev, &param->peaking_cfg);
}

void isp_pk_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	val = (124 << 0) | (70 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_0, val);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_0, -29, 16, 8);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_1, 70, 0, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_1, -37, 8, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_1, -16, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_1, 0, 24, 8);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_2, -29, 0, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_2, -16, 8, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_2, -3, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILTER_BPLUT_2, 0, 24, 8);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_0, 124, 0, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_0, 70, 8, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_0, -29, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_0, 0, 24, 8);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_1, 70, 0, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_1, -37, 8, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_1, -16, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_1, 0, 24, 8);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_2, -29, 0, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_2, -16, 8, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_2, -3, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_FILT_HPLUT_2, 0, 24, 8);

	val = (4 << 0) | (5 << 4) | (5 << 8) | (4 << 12) |
		(3 << 16) | (2 << 20) | (1 << 24) | (0 << 28);
	isp_reg_write(isp_dev, ISP_POST_PK_LUMA_OSHT_LUT, val);
	isp_reg_write(isp_dev, ISP_POST_PK_LUMA_OSHT_LUT_1, 0);

	val = (3 << 0) | (6 << 4) | (5 << 8) | (4 << 12) |
		(4 << 16) | (4 << 20) | (3 << 24) | (2 << 28);
	isp_reg_write(isp_dev, ISP_POST_PK_LUMA_GAIN_LUT, val);
	isp_reg_write(isp_dev, ISP_POST_PK_LUMA_GAIN_LUT_1, 1);

	val = (120 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_FILT_BP_LUT, val);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_DRT_FILT_BP_LUT, -60, 8, 8);

	val = (120 << 0) | (0 << 8) | (0 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_FILT_HP_LUT, val);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_DRT_FILT_HP_LUT, -60, 8, 8);

	val = (0 << 24) | (0 << 16) | (0 << 8) | (120 << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_FILT_BP_LUT_1, val);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_DRT_FILT_BP_LUT_1, -60, 8, 8);

	val = (0 << 24) | (0 << 16) | (0 << 8) | (120 << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_FILT_HP_LUT_1, val);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_DRT_FILT_HP_LUT_1, -60, 8, 8);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_DRT_HP_GAIN, 20, 24, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_DRT_HP_GAIN, 60, 16, 8);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_DRT_BP_GAIN, 20, 24, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_DRT_BP_GAIN, 60, 16, 8);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_HP_GAIN, 20, 24, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_HP_GAIN, 60, 16, 8);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_BP_GAIN, 20, 24, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_CIR_BP_GAIN, 60, 16, 8);

	val = (16 << 16) | (128 << 8) | (128 << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_HP_GAIN_1, val);

	val = (128 << 16) | (32 << 8) | (8 << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_DRT_BP_GAIN_1, val);

	val = (16 << 16) | (128 << 8) | (128 << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_HP_GAIN_1, val);

	val = (128 << 16) | (32 << 8) | (16 << 0);
	isp_reg_write(isp_dev, ISP_POST_PK_CIR_BP_GAIN_1, val);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_FINAL_GAIN, 100, 0, 8);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_FINAL_GAIN, 80, 8, 8);

	isp_reg_update_bits(isp_dev, ISP_POST_PK_OSHT, 400, 8, 10);
	isp_reg_update_bits(isp_dev, ISP_POST_PK_OSHT, 120, 20, 10);

	isp_reg_update_bits(isp_dev, ISP_POST_DRT_EN_CTL, 1, 19, 1);
	isp_reg_update_bits(isp_dev, ISP_POST_DRT_EN_CTL, 0, 16, 1);
	isp_reg_update_bits(isp_dev, ISP_POST_DRT_EN_CTL, 0, 18, 1);

	isp_reg_update_bits(isp_dev, ISP_PST_PRE_CTRL, 1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_PST_PRE_CTRL, 1, 12, 1);
	isp_reg_update_bits(isp_dev, ISP_PST_PRE_CTRL, 1, 4, 1);
	isp_reg_update_bits(isp_dev, ISP_PK_MOTION_ADP_CTRL, 0, 0, 1); // keke.li

	isp_reg_update_bits(isp_dev, ISP_PST_PRE_BPC_THRD_MARGIN, 32, 0, 10);
	isp_reg_update_bits(isp_dev, ISP_PST_PRE_BPC_CTRL, 1, 4, 2);
	isp_reg_update_bits(isp_dev, ISP_POST_NRY_GAU_FILTER, 63, 0, 6);

	val = (1 << 0) | (1 << 1);
	isp_reg_write(isp_dev, ISP_POST_ADA_GAIN_EN, val);

	val = (63 << 0) | (32 << 8) | (32 << 16) | (0 << 24);
	isp_reg_write(isp_dev, ISP_POST_NR_ADA_GIAN_LUT_0, val);

	val = (32 << 0) | (48 << 8) | (56 << 16) | (63 << 24);
	isp_reg_write(isp_dev, ISP_POST_PK_ADA_GIAN_LUT_0, val);

	isp_reg_update_bits(isp_dev, CNR_ADPT_PRCT, 1, 0, 1);

	isp_reg_update_bits(isp_dev, CNR_CTRST_FRG_ALP, 1, 0, 3);
	isp_reg_update_bits(isp_dev, CNR_CTRST_FRG_ALP, 1, 4, 3);
	isp_reg_update_bits(isp_dev, CNR_CTRST_FRG_ALP, 1, 7, 1);

	isp_reg_update_bits(isp_dev, CNR_CTRST_XTH_K, 128, 0, 16);

	val = 48 | (32 << 8) | (16 << 16) | (16 << 24);
	isp_reg_write(isp_dev, CNR_HUE_PRT_LUT_1, val);

	val = 16 | (16 << 8) | (16 << 16) | (16 << 24);
	isp_reg_write(isp_dev, CNR_HUE_PRT_LUT_2, val);
	isp_reg_write(isp_dev, CNR_HUE_PRT_LUT_3, val);
	isp_reg_write(isp_dev, CNR_HUE_PRT_LUT_4, val);
	isp_reg_write(isp_dev, CNR_HUE_PRT_LUT_5, val);

	val = 32 | (48 << 8);
	isp_reg_update_bits(isp_dev, CNR_HUE_PRT_LUT_6, val, 0, 14);

}
