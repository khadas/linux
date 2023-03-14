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

static void top_cfg_normal_path(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_TOP_PATH_SEL, 0, 19, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_DISPIN_SEL, 6, 20, 10);
	isp_reg_update_bits(isp_dev, ISP_TOP_DISPIN_SEL, 0, 0, 20);
}

static void top_cfg_param(struct isp_dev_t *isp_dev, void *param)
{
	aisp_top_cfg_t *cfg = param;

	//isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, cfg->wdr_inp_chn, 4, 2);
	//isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, cfg->src_inp_chn, 8, 2);

	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL0, cfg->decmp_en, 7, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL0, cfg->inp_fmt_en, 8, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL0, cfg->bac_en, 9, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL0, cfg->fpnr_en,10, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_0, cfg->ge_en_0, 2, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_1, cfg->ge_en_1, 2, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_0, cfg->dpc_en_0, 3, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_1, cfg->dpc_en_1, 3, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_0, cfg->pat_en_0, 4, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_1, cfg->pat_en_1, 4, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_0, cfg->og_en_0, 5, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_1, cfg->og_en_1, 5, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->lcge_enable, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->pdpc_enable, 1, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->cac_en, 2, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->rawcnr_enable, 5, 2);
	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->snr1_en, 9, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->mc_tnr_en, 10, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->tnr0_en, 11, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->cubic_cs_en, 12, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->sqrt_en, 14, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_BEO_CTRL, cfg->eotf_en, 9, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BEO_CTRL, cfg->ltm_en, 1, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BEO_CTRL, cfg->gtm_en, 2, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BEO_CTRL, cfg->lns_mesh_en, 3, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BEO_CTRL, cfg->lns_rad_en, 4, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BEO_CTRL, cfg->wb_en, 6, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BEO_CTRL, cfg->blc_en, 7, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->nr_en, 4, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->pk_en, 5, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->dnlp_en, 6, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->dhz_en, 7, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->lc_en, 8, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->bsc_en, 10, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->cnr2_en, 11, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->pst_gamma_en, 16, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->ccm_en, 18, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->dmsc_en, 19, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->cm0_en, 14, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->pst_tnr_lite_en, 21, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, cfg->amcm_en, 25, 1);

	//isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, cfg->ae_stat_en, 0, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, cfg->awb_stat_en, 1, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, cfg->af_stat_en, 2, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL0, cfg->wdr_stat_en, 6, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->flkr_stat_en, 3, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->flkr_sta_pos, 17, 2);
	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_CNTL, cfg->flkr_sta_input_format, 12, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, cfg->awb_stat_switch, 4, 2);
	isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, cfg->ae_stat_switch, 8, 2);
	isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, cfg->af_stat_switch, 12, 2);
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, cfg->ae_input_2ln, 7, 1);

	//isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL0, cfg->expstitch_mode, 4, 2);
	//isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL0, cfg->pst_mux_mode, 0, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, cfg->cubic_en, 13, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, cfg->pnrmif_en, 28, 3);
	//isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, cfg->nrmif_en, 20, 8);

	if ((isp_dev->fmt.code == MEDIA_BUS_FMT_YVYU8_2X8) || (isp_dev->fmt.code == MEDIA_BUS_FMT_YUYV8_2X8))
		return;

	isp_mcnr_mif_enable(isp_dev, cfg->tnr0_en && cfg->mc_tnr_en);
	isp_ptnr_mif_enable(isp_dev, cfg->pst_tnr_lite_en);
}

static void top_cfg_base(struct isp_dev_t *isp_dev, void *param)
{
	u32 val = 0;
	aisp_base_cfg_t *base = param;
	aisp_phase_setting_cfg_t *phase = &base->phase_cfg;
	return;
	isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, phase->raw_mode, 0, 3);
	isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, phase->src_bit_depth, 12, 5);
}

static void top_cfg_blc(struct isp_dev_t *isp_dev, void *blc)
{
	aisp_blc_cfg_t *blc_cfg = blc;

	//isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, blc_cfg->fe_bl_en, 15, 1);
	//isp_reg_update_bits(isp_dev, ISP_TOP_BEO_CTRL, blc_cfg->blc_en, 7, 1);
}

void isp_top_decmpr_disable(void *isp_dev)
{
	isp_hwreg_update_bits(isp_dev, MIPI_ADAPT_DE_CTRL0, 1, 3, 1);
	isp_hwreg_update_bits(isp_dev, MIPI_ADAPT_DE_CTRL0, 1, 7, 1);
}

void isp_top_reset(void *isp_dev)
{
#ifdef T7C_CHIP
	isp_hwreg_update_bits(isp_dev, MIPI_TOP_CTRL0, 1, 8, 1);
	isp_hwreg_update_bits(isp_dev, MIPI_TOP_CTRL0, 0, 8, 1);
#else
	isp_hwreg_update_bits(isp_dev, MIPI_TOP_CTRL0, 1, 2, 1);
	isp_hwreg_update_bits(isp_dev, MIPI_TOP_CTRL0, 0, 2, 1);
#endif
}

u32 isp_top_irq_stat(struct isp_dev_t *isp_dev)
{
	u32 irq_stat = 0;

	irq_stat = isp_hwreg_read(isp_dev, ISP_TOP_RO_IRQ_STAT);

	isp_hwreg_write(isp_dev, ISP_TOP_IRQ_CLR, irq_stat);

	return irq_stat;
}

void isp_top_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;

	u32 raw_mode = 1;
	int *raw_phslut = NULL;
	int mono_phs[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int grbg_phs[] = {0,1,0,1,2,3,2,3,0,1,0,1,2,3,2,3};
	int irbg_phs[] = {4,1,4,1,2,3,2,3,4,1,4,1,2,3,2,3};
	int grbi_phs[] = {0,1,0,1,2,4,2,4,0,1,0,1,2,4,2,4};
	int grig_4x4_phs[] = {0,1,0,2,4,3,4,3,0,2,0,1,4,3,4,3};

	val = ((fmt->width & 0xffff) << 16) | (fmt->height & 0xffff);
	isp_reg_write(isp_dev, ISP_TOP_INPUT_SIZE, val);
	isp_reg_write(isp_dev, ISP_TOP_FRM_SIZE, val);

	val = ((fmt->width & 0xffff) << 16) | (5 << 0);
	isp_reg_write(isp_dev, ISP_TOP_HOLD_SIZE, val);

	if ((fmt->code == MEDIA_BUS_FMT_YVYU8_2X8) || (fmt->code == MEDIA_BUS_FMT_YUYV8_2X8)) {
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_SEL, 2, 0, 2);
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_SEL, 0, 16, 3);
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_SEL, 1, 19, 1);
		return;
	} else {
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_SEL, 3, 0, 2);
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_SEL, 1, 16, 3);
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_SEL, 0, 19, 1);
	}

	isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, raw_mode, 0, 3);
	isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, 14, 12, 5);
	isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, 0, 8, 2);
	isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, 0, 4, 2);

	raw_phslut = (raw_mode == 0) ? mono_phs:
			(raw_mode == 1) ? grbg_phs:
			(raw_mode == 2) ? irbg_phs:
			(raw_mode == 3) ? grbi_phs: grig_4x4_phs;

	val = raw_phslut[0] | (raw_phslut[1] << 4) |
		(raw_phslut[2] << 8) | (raw_phslut[3] << 12) |
		(raw_phslut[4] << 16) | (raw_phslut[5] << 20) |
		(raw_phslut[6] << 24) | (raw_phslut[7] << 28);
	isp_reg_write(isp_dev, ISP_TOP_RAW_PHS_0, val);

	val = raw_phslut[8] | (raw_phslut[9] << 4) |
		(raw_phslut[10] << 8) | (raw_phslut[11] << 12) |
		(raw_phslut[12] << 16) | (raw_phslut[13] << 20) |
		(raw_phslut[14] << 24) | (raw_phslut[15] << 28);
	isp_reg_write(isp_dev, ISP_TOP_RAW_PHS_1, val);
}

void isp_top_enable(struct isp_dev_t *isp_dev)
{

}

void isp_top_disable(struct isp_dev_t *isp_dev)
{

}

void isp_top_cfg_wdr(struct isp_dev_t *isp_dev, int wdr_en)
{
	if (wdr_en) {
		isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL0, 0x1, 4, 2);
		isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL0, 0x1, 0, 1);
		isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, 0x1, 8, 2);
		isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, 0x1, 4, 2);
	} else {
		isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL0, 0, 4, 2);
		isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL0, 0, 0, 1);
		isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, 0, 8, 2);
		isp_reg_update_bits(isp_dev, ISP_TOP_MODE_CTRL, 0, 4, 2);
	}

}

void isp_top_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	int i;
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];
	isp_hwreg_t *reg = param->custom_cfg.settings;

	if (param->pvalid.aisp_top)
		top_cfg_param(isp_dev, &param->top_cfg);

	if (param->pvalid.aisp_base)
		top_cfg_base(isp_dev, &param->base_cfg);

	if (param->pvalid.aisp_blc)
		top_cfg_blc(isp_dev, &param->blc_cfg);

	if (param->pvalid.aisp_custom) {
		for ( i = 0; i < 200; i++ ) {
			if ( reg->len ) {
				//AML_ISP_ERR("DBG: %u, %u, %u, %u\n", reg->addr, reg->val, reg->mask, reg->len);
				isp_reg_write(isp_dev, reg->addr << 2, reg->val & reg->mask);
			}
			else
				break;
			reg ++;
		}
	}
}

void isp_top_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_stats_info_t *stats = buff->vaddr[AML_PLANE_A];
	top_req_info_t *top = &stats->module_info.top_req_info;

	isp_reg_read_bits(isp_dev, ISP_TOP_MODE_CTRL, &top->raw_mode, 0, 3);

	isp_reg_read_bits(isp_dev, ISP_TOP_FEO_CTRL0, &top->wdr_stat_en, 6, 1);
}

void isp_top_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	isp_reg_update_bits(isp_dev, ISP_PROB_CTRL, 1, 2, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, 0, 0, 3);
	isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, 2, 4, 3);
	isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, 1, 8, 2);
	isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, 0, 12, 2);
	isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, 0, 16, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, 0, 20, 1);

	val = isp_reg_read(isp_dev, ISP_TOP_FEO_CTRL0);
	val |= (1 << 6) | (1 << 9) | (1 << 12);
	isp_reg_write(isp_dev, ISP_TOP_FEO_CTRL0, val);

	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_0, 1, 2, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_0, 1, 3, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_0, 0, 4, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_0, 1, 5, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_0, 0, 0, 2);

	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_1, 1, 2, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_1, 1, 3, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_1, 0, 4, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_1, 1, 5, 1);
	isp_reg_update_bits(isp_dev, ISP_TOP_FEO_CTRL1_1, 1, 0, 2);

	val = (1 << 0) | (1 << 1) | (1 << 2) | (0 << 3) | (1 << 4) |
		(1 << 5) | (2 << 7) | (1 << 9) | (0 << 10) | (0 << 11) |
		(1 << 12) | (1 << 13) | (1 << 14) | (0 << 15) | (1 << 16) | (1 << 17);
	isp_reg_write(isp_dev, ISP_TOP_FED_CTRL, val);

	val = (1 << 1) | (0 << 2) | (1 << 3) | (0 << 4) | (0 << 5) |
		(1 << 6) | (1 << 7) |(1 << 8) | (1 << 9);
	isp_reg_write(isp_dev, ISP_TOP_BEO_CTRL, val);

	val = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) |(0 << 7) |
		(1 << 8) | (0 << 9) | (1 << 10) | (1 << 11) | (0 << 13) | (1 << 14) |
		(0 << 15) | (1 << 16) | (0 << 17) | (1 << 18) | (1 << 19) | (0 << 20) |
		(0 << 21) | (1 << 25);

	isp_reg_write(isp_dev, ISP_TOP_BED_CTRL, val);

	isp_reg_update_bits(isp_dev, ISP_TOP_BYPASS, 0, 0, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_IRQ_LINE_THRD, 1, 16, 2);
	isp_reg_update_bits(isp_dev, ISP_TOP_IRQ_LINE_THRD, 1079, 0, 16);

	isp_reg_update_bits(isp_dev, ISP_FRM_CNT_CTRL, 1, 26, 1);

	isp_reg_write(isp_dev, ISP_TOP_IRQ_EN, 0x1);

	top_cfg_normal_path(isp_dev);
}
