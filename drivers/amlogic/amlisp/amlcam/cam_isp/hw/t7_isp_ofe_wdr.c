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

static void ofe_wdr_cfg_gen(struct isp_dev_t *isp_dev, void *wdr)
{
	u32 val = 0;
	aisp_wdr_cfg_t *w_cfg = wdr;

	val = (w_cfg->wdr_lexpratio_int64[3] << 16) |
		(w_cfg->wdr_lexpratio_int64[2] << 0);
	isp_reg_write(isp_dev, ISP_WDR_GEN_EXPRATIO1, val);

	val = (w_cfg->wdr_lexpratio_int64[1] << 16) |
		(w_cfg->wdr_lexpratio_int64[0] << 0);
	isp_reg_write(isp_dev, ISP_WDR_GEN_EXPRARIO0, val);

	val = (w_cfg->wdr_lmapratio_int64[2][1] << 16) |
		(w_cfg->wdr_lmapratio_int64[2][0] << 0);
	isp_reg_write(isp_dev, ISP_WDR_GEN_MAPRATIO2, val);

	val = (w_cfg->wdr_lmapratio_int64[1][1] << 16) |
		(w_cfg->wdr_lmapratio_int64[1][0] << 0);
	isp_reg_write(isp_dev, ISP_WDR_GEN_MAPRATIO1, val);

	val = (w_cfg->wdr_lmapratio_int64[0][1] << 16) |
		(w_cfg->wdr_lmapratio_int64[0][0] << 0);
	isp_reg_write(isp_dev, ISP_WDR_GEN_MAPRATIO0, val);

	isp_reg_write(isp_dev, ISP_WDR_GEN_LEXPCOMP0, w_cfg->wdr_lexpcomp_gr_int64[0]);
	isp_reg_write(isp_dev, ISP_WDR_GEN_MEXPCOMP0, w_cfg->wdr_lexpcomp_gr_int64[1]);
	isp_reg_write(isp_dev, ISP_WDR_GEN_SEXPCOMP0, w_cfg->wdr_lexpcomp_gr_int64[2]);

	isp_reg_write(isp_dev, ISP_WDR_GEN_LEXPCOMP1, w_cfg->wdr_lexpcomp_gb_int64[0]);
	isp_reg_write(isp_dev, ISP_WDR_GEN_MEXPCOMP1, w_cfg->wdr_lexpcomp_gb_int64[1]);
	isp_reg_write(isp_dev, ISP_WDR_GEN_SEXPCOMP1, w_cfg->wdr_lexpcomp_gb_int64[2]);

	isp_reg_write(isp_dev, ISP_WDR_GEN_LEXPCOMP2, w_cfg->wdr_lexpcomp_rg_int64[0]);
	isp_reg_write(isp_dev, ISP_WDR_GEN_MEXPCOMP2, w_cfg->wdr_lexpcomp_rg_int64[1]);
	isp_reg_write(isp_dev, ISP_WDR_GEN_SEXPCOMP2, w_cfg->wdr_lexpcomp_rg_int64[2]);

	isp_reg_write(isp_dev, ISP_WDR_GEN_LEXPCOMP3, w_cfg->wdr_lexpcomp_bg_int64[0]);
	isp_reg_write(isp_dev, ISP_WDR_GEN_MEXPCOMP3, w_cfg->wdr_lexpcomp_bg_int64[1]);
	isp_reg_write(isp_dev, ISP_WDR_GEN_SEXPCOMP3, w_cfg->wdr_lexpcomp_bg_int64[2]);

	isp_reg_write(isp_dev, ISP_WDR_GEN_LEXPCOMP4, w_cfg->wdr_lexpcomp_ir_int64[0]);
	isp_reg_write(isp_dev, ISP_WDR_GEN_MEXPCOMP4, w_cfg->wdr_lexpcomp_ir_int64[1]);
	isp_reg_write(isp_dev, ISP_WDR_GEN_SEXPCOMP4, w_cfg->wdr_lexpcomp_ir_int64[2]);
}

static void ofe_wdr_cfg_mdect(struct isp_dev_t *isp_dev, void *wdr)
{
	u32 val = 0;
	aisp_wdr_cfg_t *w_cfg = wdr;

	val = w_cfg->wdr_motiondect_en;
	isp_reg_update_bits(isp_dev, ISP_WDR_COM_PARAM0, val, 8, 1);

	val = w_cfg->wdr_mdetc_withblc_mode;
	isp_reg_update_bits(isp_dev, ISP_WDR_MDETC_MODE, val, 3, 1);

	val = w_cfg->wdr_mdetc_chksat_mode;
	isp_reg_update_bits(isp_dev, ISP_WDR_MDETC_MODE, val, 1, 2);

	val = w_cfg->wdr_mdetc_motionmap_mode;
	isp_reg_update_bits(isp_dev, ISP_WDR_MDETC_MODE, val, 0, 1);

	val = w_cfg->wdr_mdeci_chkstill_mode;
	isp_reg_update_bits(isp_dev, ISP_WDR_MDECI_PARAM, val, 25, 1);

	val = w_cfg->wdr_mdeci_addlong;
	isp_reg_update_bits(isp_dev, ISP_WDR_MDECI_PARAM, val, 16, 8);

	val = w_cfg->wdr_mdeci_still_thd;
	isp_reg_update_bits(isp_dev, ISP_WDR_MDECI_PARAM, val, 8, 8);

	val = w_cfg->wdr_mdeci_fullmot_thd;
	isp_reg_update_bits(isp_dev, ISP_WDR_MDECI_PARAM, val, 0, 8);

	val = (w_cfg->wdr_mdetc_sat_gr_thd << 16) |
		(w_cfg->wdr_mdetc_sat_gb_thd << 0);
	isp_reg_write(isp_dev, ISP_WDR_MDETC_SATTHD0, val);

	val = (w_cfg->wdr_mdetc_sat_rg_thd << 16) |
		(w_cfg->wdr_mdetc_sat_bg_thd << 0);
	isp_reg_write(isp_dev, ISP_WDR_MDETC_SATTHD1, val);

	val = (w_cfg->wdr_mdetc_sqrt_again_ir << 24) |
		(w_cfg->wdr_mdetc_sqrt_again_bg << 16) |
		(w_cfg->wdr_mdetc_sqrt_again_rg << 8) |
		(w_cfg->wdr_mdetc_sqrt_again_g << 0);
	isp_reg_write(isp_dev, ISP_WDR_MDETC_SQRT_AGAIN, val);

	val = (w_cfg->wdr_mdetc_sqrt_dgain_ir << 24) |
		(w_cfg->wdr_mdetc_sqrt_dgain_bg << 16) |
		(w_cfg->wdr_mdetc_sqrt_dgain_rg << 8) |
		(w_cfg->wdr_mdetc_sqrt_dgain_g << 0);
	isp_reg_write(isp_dev, ISP_WDR_MDETC_SQRT_DGAIN, val);

	val = (w_cfg->wdr_mdetc_lo_weight[0] << 8) |
		(w_cfg->wdr_mdetc_lo_weight[1] << 24) |
		(w_cfg->wdr_mdetc_hi_weight[0] << 0) |
		(w_cfg->wdr_mdetc_hi_weight[1] << 16);
	isp_reg_write(isp_dev, ISP_WDR_MDETC_WEIGHT0, val);

	val = (w_cfg->wdr_mdetc_lo_weight[2] << 8) |
		(w_cfg->wdr_mdetc_hi_weight[2] << 0);
	isp_reg_write(isp_dev, ISP_WDR_MDETC_WEIGHT1, val);

	val = (w_cfg->wdr_mdetc_noisefloor_g[0] << 0) |
		(w_cfg->wdr_mdetc_noisefloor_g[1] << 8);
	isp_reg_write(isp_dev, ISP_WDR_MDETC_GNOISEFLOOR, val);

	val = (w_cfg->wdr_mdetc_noisefloor_rg[0] << 0) |
		(w_cfg->wdr_mdetc_noisefloor_rg[1] << 8);
	isp_reg_write(isp_dev, ISP_WDR_MDETC_RNOISEFLOOR, val);

	val = (w_cfg->wdr_mdetc_noisefloor_bg[0] << 0) |
		(w_cfg->wdr_mdetc_noisefloor_bg[1] << 8);
	isp_reg_write(isp_dev, ISP_WDR_MDETC_BNOISEFLOOR, val);

	val = (w_cfg->wdr_mdeci_sexpstill_gr_lsthd[0] << 0) |
		(w_cfg->wdr_mdeci_sexpstill_gr_lsthd[1] << 16);
	isp_reg_write(isp_dev, ISP_WDR_MDECI_LSTHD0, val);

	val = (w_cfg->wdr_mdeci_sexpstill_gb_lsthd[0] << 0);
	isp_reg_write(isp_dev, ISP_WDR_MDECI_LSTHD1, val);

	val = (w_cfg->wdr_mdeci_sexpstill_gb_lsthd[1] << 0);
	isp_reg_write(isp_dev, ISP_WDR_MDECI_LSTHD2, val);

	val = (w_cfg->wdr_mdeci_sexpstill_rg_lsthd[0] << 0) |
		(w_cfg->wdr_mdeci_sexpstill_rg_lsthd[1] << 16);
	isp_reg_write(isp_dev, ISP_WDR_MDECI_LSTHD3, val);

	val = (w_cfg->wdr_mdeci_sexpstill_bg_lsthd[0] << 0);
	isp_reg_write(isp_dev, ISP_WDR_MDECI_LSTHD4, val);

	val = (w_cfg->wdr_mdeci_sexpstill_bg_lsthd[1] << 0);
	isp_reg_write(isp_dev, ISP_WDR_MDECI_LSTHD5, val);
}

static void ofe_wdr_cfg_flong(struct isp_dev_t *isp_dev, void *wdr)
{
	u32 val = 0;
	aisp_wdr_cfg_t *w_cfg = wdr;

	val = ((w_cfg->wdr_forcelong_en & 0x3) << 8) |
		((w_cfg->wdr_flong2_colorcorrect_en & 0x1) << 4) |
		(w_cfg->wdr_forcelong_thdmode & 0x1);
	isp_reg_write(isp_dev, ISP_WDR_FLONG_PARAM, val);

	val = (w_cfg->wdr_flong2_thd0[0] << 16) |
		(w_cfg->wdr_flong2_thd1[0] << 0);
	isp_reg_write(isp_dev, ISP_WDR_FLONG_THD0, val);

	val = (w_cfg->wdr_flong2_thd0[1] << 16) |
		(w_cfg->wdr_flong2_thd1[1] << 0);
	isp_reg_write(isp_dev, ISP_WDR_FLONG_THD1, val);

	val = (w_cfg->wdr_flong1_thd0 << 16) |
		(w_cfg->wdr_flong1_thd1 << 0);
	isp_reg_write(isp_dev, ISP_WDR_FLONG1_THD, val);
}

static void ofe_wdr_cfg_expcomb(struct isp_dev_t *isp_dev, void *wdr)
{
	u32 val = 0;
	aisp_wdr_cfg_t *w_cfg = wdr;

	val = w_cfg->wdr_force_exp_en;
	isp_reg_update_bits(isp_dev, ISP_WDR_COM_PARAM0, val, 24, 1);

	val = w_cfg->wdr_force_exp_mode;
	isp_reg_update_bits(isp_dev, ISP_WDR_COM_PARAM0, val, 26, 3);

	val = w_cfg->wdr_expcomb_maxratio;
	isp_reg_update_bits(isp_dev, ISP_WDR_EXPCOMB_MAXRATIO, val, 1, 22);

	val = (w_cfg->wdr_expcomb_blend_thd0 << 16) |
		(w_cfg->wdr_expcomb_blend_thd1 << 0);
	isp_reg_write(isp_dev, ISP_WDR_EXPCOMB_BLDTHD, val);

	val = ((w_cfg->wdr_expcomb_ir_slope_weight & 0xf) << 10) |
		(w_cfg->wdr_expcomb_ir_blend_slope & 0x3ff);
	isp_reg_write(isp_dev, ISP_WDR_EXPCOMB_IRPARAM, val);

	val = (w_cfg->wdr_expcomb_ir_blend_thd0 << 16) |
		(w_cfg->wdr_expcomb_ir_blend_thd1 << 0);
	isp_reg_write(isp_dev, ISP_WDR_EXPCOMB_IRBLDTHD, val);

	val = (w_cfg->wdr_expcomb_maxsat_gr_thd << 16) |
		(w_cfg->wdr_expcomb_maxsat_gb_thd << 0);
	isp_reg_write(isp_dev, ISP_WDR_EXPCOMB_SATTHD0, val);

	val = (w_cfg->wdr_expcomb_maxsat_rg_thd << 16) |
		(w_cfg->wdr_expcomb_maxsat_bg_thd << 0);
	isp_reg_write(isp_dev, ISP_WDR_EXPCOMB_SATTHD1, val);

	val = w_cfg->wdr_expcomb_maxsat_ir_thd;
	isp_reg_write(isp_dev, ISP_WDR_EXPCOMB_SATTHD2, val);

	val = (w_cfg->wdr_expcomb_maxavg_winsize << 21) |
		((w_cfg->wdr_expcomb_maxavg_mode & 0x7) << 18) |
		((w_cfg->wdr_expcomb_maxavg_ratio & 0xf) << 14) |
		((w_cfg->wdr_expcomb_slope_weight & 0xf) << 10) |
		(w_cfg->wdr_expcomb_blend_slope & 0x3ff);
	isp_reg_write(isp_dev, ISP_WDR_EXPCOMB_PARAM, val);
}

static void ofe_wdr_cfg_comb(struct isp_dev_t *isp_dev, void *wdr)
{
	u32 val = 0;
	aisp_wdr_cfg_t *w_cfg = wdr;

	val = w_cfg->wdr_stat_flt_en;
	isp_reg_update_bits(isp_dev, ISP_WDR_STAT_PARAM, val, 0, 1);

	val = (w_cfg->comb_expratio_int64[0] << 0) |
		(w_cfg->comb_expratio_int64[1] << 16);
	isp_reg_write(isp_dev, ISP_COMB_EXPRAT_01, val);

	val = w_cfg->comb_expratio_int64[2];
	isp_reg_write(isp_dev, ISP_COMB_EXPRAT_2, val);

	val = w_cfg->comb_exprratio_int1024[0];
	isp_reg_write(isp_dev, ISP_COMB_RECIPROCAL_EXPRAT_RLTV_0, val);

	val = w_cfg->comb_exprratio_int1024[1];
	isp_reg_write(isp_dev, ISP_COMB_RECIPROCAL_EXPRAT_RLTV_1, val);

	val = (w_cfg->comb_g_lsbarrier[0] << 16) |
		(w_cfg->comb_rg_lsbarrier[0] << 0);
	isp_reg_write(isp_dev, ISP_COMB_LSBARRIER0_0, val);

	val = (w_cfg->comb_bg_lsbarrier[0] << 16) |
		(w_cfg->comb_ir_lsbarrier[0] << 0);
	isp_reg_write(isp_dev, ISP_COMB_LSBARRIER1_0, val);

	val = (w_cfg->comb_g_lsbarrier[1] << 16) |
		(w_cfg->comb_rg_lsbarrier[1] << 0);
	isp_reg_write(isp_dev, ISP_COMB_LSBARRIER0_1, val);

	val = (w_cfg->comb_bg_lsbarrier[1] << 16) |
		(w_cfg->comb_ir_lsbarrier[1] << 0);
	isp_reg_write(isp_dev, ISP_COMB_LSBARRIER1_1, val);

	val = (w_cfg->comb_g_lsbarrier[2] << 16) |
		(w_cfg->comb_rg_lsbarrier[2] << 0);
	isp_reg_write(isp_dev, ISP_COMB_LSBARRIER0_2, val);

	val = (w_cfg->comb_bg_lsbarrier[2] << 16) |
		(w_cfg->comb_ir_lsbarrier[2] << 0);
	isp_reg_write(isp_dev, ISP_COMB_LSBARRIER1_2, val);

	val = (w_cfg->comb_g_lsbarrier[3] << 16) |
		(w_cfg->comb_rg_lsbarrier[3] << 0);
	isp_reg_write(isp_dev, ISP_COMB_LSBARRIER0_3, val);

	val = (w_cfg->comb_bg_lsbarrier[3] << 16) |
		(w_cfg->comb_ir_lsbarrier[3] << 0);
	isp_reg_write(isp_dev, ISP_COMB_LSBARRIER1_3, val);

	val = (w_cfg->comb_maxratio << 0) | (w_cfg->comb_shortexp_mode << 16);
	isp_reg_write(isp_dev, ISP_COMB_PARAM, val);
}

static void ofe_wdr_cfg_param(struct isp_dev_t *isp_dev, void *wdr)
{
	ofe_wdr_cfg_gen(isp_dev, wdr);
	ofe_wdr_cfg_mdect(isp_dev, wdr);
	ofe_wdr_cfg_flong(isp_dev, wdr);
	ofe_wdr_cfg_expcomb(isp_dev, wdr);
	ofe_wdr_cfg_comb(isp_dev, wdr);
}

static void ofe_wdr_cfg_blc(struct isp_dev_t *isp_dev, void *blc)
{
	u32 val = 0;
	aisp_wdr_blc_cfg_t *b_cfg = blc;

	val = (b_cfg->wdr_blacklevel_gr << 16) |
		(b_cfg->wdr_blacklevel_gb << 0);
	isp_reg_write(isp_dev, ISP_WDR_COM_BLC0, val);

	val = (b_cfg->wdr_blacklevel_rg << 16) |
		(b_cfg->wdr_blacklevel_bg << 0);
	isp_reg_write(isp_dev, ISP_WDR_COM_BLC1, val);

	val = (b_cfg->wdr_blacklevel_ir << 16) |
		(b_cfg->wdr_blacklevel_wdr << 0);
	isp_reg_write(isp_dev, ISP_WDR_COM_BLC2, val);
}

static void ofe_wdr_cfg_fmt(struct isp_dev_t *isp_dev, void *fmt)
{
	u32 val = 0;
	aisp_wdr_fmt_cfg_t *f_cfg = fmt;

	val = f_cfg->sensor_bitdepth;
	isp_reg_update_bits(isp_dev, ISP_WDR_COM_PARAM0, val, 16, 5);
}

static void ofe_wdr_cfg_wb_enhc(struct isp_dev_t *isp_dev, void *enhc)
{
	u32 val = 0;
	aisp_wb_enhc_cfg_t *e_cfg = enhc;

	val = e_cfg->awb_gain_256[4];
	isp_reg_write(isp_dev, ISP_WDR_COM_AWBGAIN2, val);

	val = (e_cfg->awb_gain_256[3] << 16) |
		(e_cfg->awb_gain_256[2] << 0);
	isp_reg_write(isp_dev, ISP_WDR_COM_AWBGAIN1, val);

	val = (e_cfg->awb_gain_256[1] << 16) |
		(e_cfg->awb_gain_256[0] << 0);
	isp_reg_write(isp_dev, ISP_WDR_COM_AWBGAIN0, val);
}

void isp_ofe_wdr_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
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

	isp_reg_update_bits(isp_dev, ISP_WDR_COM_PARAM0, xofst, 2, 2);
	isp_reg_update_bits(isp_dev, ISP_WDR_COM_PARAM0, yofst, 0, 2);
}

void isp_ofe_wdr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_wdr)
		ofe_wdr_cfg_param(isp_dev, &param->wdr_cfg);

	if (param->pvalid.aisp_wdr_blc)
		ofe_wdr_cfg_blc(isp_dev, &param->wdr_blc);

	if (param->pvalid.aisp_wdr_fmt)
		ofe_wdr_cfg_fmt(isp_dev, &param->wdr_fmt);

	if (param->pvalid.aisp_wb_enhc)
		ofe_wdr_cfg_wb_enhc(isp_dev, &param->wb_enhc);
}

void isp_ofe_wdr_stats(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_stats_info_t *stats = buff->vaddr[AML_PLANE_A];
	wdr_stats_info_t *wdr = &stats->wdr_stats;

	isp_reg_read_bits(isp_dev, ISP_WDR_EXPCOMB_PARAM,
		&wdr->wdr_expcomb_slope_weight, 10, 4);
	isp_reg_read_bits(isp_dev, ISP_WDR_EXPCOMB_IRPARAM,
		&wdr->wdr_expcomb_ir_slope_weight, 10, 4);

	wdr->fw_wdr_stat_sum[0] = isp_hwreg_read(isp_dev, ISP_WDR_STAT_SUM_0);
	wdr->fw_wdr_stat_sum[1] = isp_hwreg_read(isp_dev, ISP_WDR_STAT_SUM_1);
	wdr->fw_wdr_stat_cnt[0] = isp_hwreg_read(isp_dev, ISP_WDR_STAT_CNT_0);
	wdr->fw_wdr_stat_cnt[1] = isp_hwreg_read(isp_dev, ISP_WDR_STAT_CNT_1);
}

void isp_ofe_wdr_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	u32 val = 0;
	aisp_stats_info_t *stats = buff->vaddr[AML_PLANE_A];
	wdr_req_info_t *wdr_info = &stats->module_info.wdr_req_info;

	isp_reg_read_bits(isp_dev, ISP_TOP_FEO_CTRL0, &wdr_info->wdr_en, 4, 2);

	val = isp_reg_read(isp_dev, ISP_WDR_GEN_EXPRATIO1);
	wdr_info->wdr_lexpratio_int64_0 = (val >> 0) & 0x7fff;
	wdr_info->wdr_lexpratio_int64_1 = (val >> 16) & 0x7fff;

	val = isp_reg_read(isp_dev, ISP_WDR_EXPCOMB_BLDTHD);
	wdr_info->wdr_expcomb_blend_thd0 = (val >> 16) & 0x3fff;
	wdr_info->wdr_expcomb_blend_thd1 = (val >> 0) & 0x3fff;
}

void isp_ofe_wdr_init(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_WDR_EXPCOMB_MAXRATIO, (1 << 20) - 1, 1, 22);

	return;
}
