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

static const u32 ltm_histxptsbuf_blk65[79] = { 0,         90,  256,     470,     724,    1011,    1330,    1676,
				2048,    2443,    2862,    3302,    3762,    4242,    4741,    5258,
				5792,    10641,   16384,   22897,   30099,   37929,
				46340,   55296,   64763,   74716,   85133,   95994,  107280,  118978,
				131072,  143550,  156400,  169613,  183178,  197087,  211331,  225902,
				240794,  256000,  271512,  287326,  303435,  319835,  336520,  353486,
				370727,  388240,  406020,  424064,  442368,  460927,  479739,  498800,
				518107,  537657,  557447,  577474,  597735,  618228,  638949,  659897,
				681070,  702464,  724077,  745907,  767953,  790212,  812681,  835360,
				858246,  881337,  904631,  928128,  951824,  975718,  999810,  1024096, 1048575,
};

static void ltm_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	u32 i;
	u32 val = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	isp_hw_lut_wstart(isp_dev, LTM_LUT_CFG);

	isp_reg_write(isp_dev, ISP_LTM_STA_THD_ADDR, 0);
	for (i = 0; i < 79; i++) {
		val = lut_cfg->ltm_histxptsbuf_blk[i];
		isp_reg_write(isp_dev, ISP_LTM_STA_THD_DATA, val);
	}

	isp_reg_write(isp_dev, ISP_LTM_BLK_STA_ADDR, 0);
	for (i = 0; i < 96; i++) {
		val = lut_cfg->ltm_lmin_blk[i];
		isp_reg_write(isp_dev, ISP_LTM_BLK_STA_DATA, val);
	}

	for (i = 0; i < 96; i++) {
		val = lut_cfg->ltm_lmax_blk[i];
		isp_reg_write(isp_dev, ISP_LTM_BLK_STA_DATA, val);
	}

	isp_reg_write(isp_dev, ISP_LTM_CCALP_ADDR, 0);
	for (i = 0; i < 32; i++) {
		val = (lut_cfg->ltm_ccalp_lut[i * 8 + 0] << 0) |
			(lut_cfg->ltm_ccalp_lut[i * 8 + 1] << 4) |
			(lut_cfg->ltm_ccalp_lut[i * 8 + 2] << 8) |
			(lut_cfg->ltm_ccalp_lut[i * 8 + 3] << 12) |
			(lut_cfg->ltm_ccalp_lut[i * 8 + 4] << 16) |
			(lut_cfg->ltm_ccalp_lut[i * 8 + 5] << 20) |
			(lut_cfg->ltm_ccalp_lut[i * 8 + 6] << 24) |
			(lut_cfg->ltm_ccalp_lut[i * 8 + 7] << 28);
		isp_reg_write(isp_dev, ISP_LTM_CCALP_DATA, val);
	}

	isp_hw_lut_wend(isp_dev);
}

static void ltm_cfg_param(struct isp_dev_t *isp_dev, void *ltm)
{
	u32 val = 0;
	aisp_ltm_cfg_t *ltm_cfg = ltm;

	isp_reg_write(isp_dev, ISP_LTM_LOG_KEY, ltm_cfg->ltm_lr_u28);

	val = (ltm_cfg->ltm_expblend_thd0 << 0) |(ltm_cfg->ltm_expblend_thd1 << 16);
	isp_reg_write(isp_dev, ISP_LTM_SHRP_EXP_THD, val);

	isp_reg_write(isp_dev, ISP_LTM_STA_GMIN_TOTAL, ltm_cfg->ltm_gmin_total);
	isp_reg_write(isp_dev, ISP_LTM_STA_GMAX_TOTAL, ltm_cfg->ltm_gmax_total);

	val = (ltm_cfg->ltm_glbwin_hstart << 0) | (ltm_cfg->ltm_glbwin_hend << 16);
	isp_reg_write(isp_dev, ISP_LTM_GLBWIN_H, val);

	val = (ltm_cfg->ltm_glbwin_vstart << 0) | (ltm_cfg->ltm_glbwin_vend << 16);
	isp_reg_write(isp_dev, ISP_LTM_GLBWIN_V, val);

	isp_reg_update_bits(isp_dev, ISP_LTM_POW_CRTL, ltm_cfg->ltm_lo_gm_u6, 0, 6);
	isp_reg_update_bits(isp_dev, ISP_LTM_POW_CRTL, ltm_cfg->ltm_hi_gm_u7, 8, 7);

	isp_reg_write(isp_dev, ISP_LTM_HIST_POW_Y, ltm_cfg->ltm_pow_y_u20);
	isp_reg_write(isp_dev, ISP_LTM_HIST_POW_DIVISOR, ltm_cfg->ltm_pow_divisor_u23);
}

static void ltm_cfg_enhc(struct isp_dev_t *isp_dev, void *enhc)
{
	u32 i;
	u32 val = 0;
	aisp_ltm_enhc_cfg_t *e_cfg = enhc;

	isp_hw_lut_wstart(isp_dev, LTM_ENHC_LUT_CFG);

	//isp_reg_update_bits(isp_dev, ISP_TOP_BEO_CTRL, e_cfg->ltm_en & 0x1, 1, 1);
	isp_reg_update_bits(isp_dev, ISP_LTM_FLT_CTRL, e_cfg->ltm_cc_en & 0x1, 28, 1);
	isp_reg_update_bits(isp_dev, ISP_LTM_SHRP_CRTL, e_cfg->ltm_dtl_ehn_en & 0x1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_LTM_FINAL_CTRL, e_cfg->ltm_vs_gtm_alpha & 0x3f, 24, 6);
	isp_reg_update_bits(isp_dev, ISP_LTM_STA_LPF_CTRL, e_cfg->ltm_lmin_med_en & 0x1, 12, 1);
	isp_reg_update_bits(isp_dev, ISP_LTM_STA_LPF_CTRL, e_cfg->ltm_lmax_med_en & 0x1, 8, 1);
	isp_reg_update_bits(isp_dev, ISP_LTM_FINAL_CTRL, e_cfg->ltm_b2luma_alpha & 0x3f, 16, 6);

	isp_reg_write(isp_dev, ISP_LTM_CCRAT_ADDR, 0);
	for (i = 0; i < 31; i++) {
		val = (e_cfg->ltm_satur_lut[i * 2 + 0] << 0) |
			(e_cfg->ltm_satur_lut[i * 2 + 1] << 12);
		isp_reg_write(isp_dev, ISP_LTM_CCRAT_DATA, val);
	}
	val = e_cfg->ltm_satur_lut[62];
	isp_reg_write(isp_dev, ISP_LTM_CCRAT_DATA, val);

	isp_hw_lut_wend(isp_dev);
}

void isp_ltm_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	int isp_fmt = 0;
	int xofst, yofst;
	u32 val = 0;

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

	isp_reg_update_bits(isp_dev, ISP_LTM_FLT_CTRL, xofst, 26, 2);
	isp_reg_update_bits(isp_dev, ISP_LTM_FLT_CTRL, yofst, 24, 2);

	val = (0 << 0) | (fmt->width << 16);
	isp_reg_write(isp_dev, ISP_LTM_GLBWIN_H, val);

	val = (0 << 0) | (fmt->height << 16);
	isp_reg_write(isp_dev, ISP_LTM_GLBWIN_V, val);
}

void isp_ltm_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	int phidx, thidx, pvidx, tvidx;
	u32 xsize = 0;
	u32 ysize = 0;

	xsize = fmt->width;
	ysize = fmt->height;

	isp_reg_update_bits(isp_dev, ISP_LTM_FLT_CTRL, LTM_STA_LEN_H - 1, 4, 4);
	isp_reg_update_bits(isp_dev, ISP_LTM_FLT_CTRL, LTM_STA_LEN_V - 1, 0, 4);

	phidx = (0 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (1 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LTM_STA_HIDX_0_1, val);

	phidx = (2 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (3 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LTM_STA_HIDX_2_3, val);

	phidx = (4 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (5 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LTM_STA_HIDX_4_5, val);

	phidx = (6 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (7 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LTM_STA_HIDX_6_7, val);

	phidx = (8 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (9 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LTM_STA_HIDX_8_9, val);

	phidx = (10 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (11 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LTM_STA_HIDX_10_11, val);

	phidx = xsize;
	isp_reg_write(isp_dev, ISP_LTM_STA_HIDX_12, phidx);


	pvidx = 0 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = 1 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LTM_STA_VIDX_0_1, val);

	pvidx = 2 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = 3 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LTM_STA_VIDX_2_3, val);

	pvidx = 4 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = 5 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LTM_STA_VIDX_4_5, val);

	pvidx = 6 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = 7 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LTM_STA_VIDX_6_7, val);

	pvidx = ysize;
	isp_reg_write(isp_dev, ISP_LTM_STA_VIDX_8, pvidx);
}

void isp_ltm_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param)
{
	int i = 0;
	u32 addr = 0;
	u32 val = 0;
	s32 hsize = 0;
	s32 hidx = 0;
	s32 ovlp = 0;
	s32 slice_size = 0;

	hsize = param->whole_frame_hcenter * 2;

	if (param->pos == 0) {
		isp_hwreg_update_bits(isp_dev, ISP_LTM_GLBWIN_H, 0, 0, 16);

		val = param->left_hsize - param->left_ovlp;
		isp_hwreg_update_bits(isp_dev, ISP_LTM_GLBWIN_H, val, 16, 16);

		for (i = 0; i < LTM_STA_LEN_H; i++) {
			hidx = (i == (LTM_STA_LEN_H - 1)) ? hsize : (i * hsize / MAX(1, LTM_STA_LEN_H - 1));
			slice_size = (param->pleft_hsize) +
				hsize / (LTM_STA_LEN_H-1) -
				(param->pleft_ovlp) +
				(i - (LTM_STA_LEN_H - 1) / 2 - 1) * 4;
			hidx = MIN(MAX(hidx, 0), slice_size);

			addr = ISP_LTM_STA_HIDX_0_1 + (i /2 * 4);
			isp_hwreg_update_bits(isp_dev, addr, hidx, (i % 2) * 16, 16);
		}
	} else if (param->pos == 1) {
		slice_size = param->pright_hsize;
		ovlp = param->pright_ovlp;

		isp_hwreg_update_bits(isp_dev, ISP_LTM_GLBWIN_H, ovlp, 0, 16);
		isp_hwreg_update_bits(isp_dev, ISP_LTM_GLBWIN_H, slice_size, 16, 16);

		for (i = 0; i < LTM_STA_LEN_H; i++) {
			hidx = (i == (LTM_STA_LEN_H - 1)) ? hsize : (i * hsize / MAX(1, LTM_STA_LEN_H - 1));
			hidx = MIN(MAX(hidx + ovlp * 2 - slice_size,
				(ovlp -hsize /(LTM_STA_LEN_H - 1) +
				(i - (LTM_STA_LEN_H - 1) / 2 + 1) * 4)), slice_size);

			addr = ISP_LTM_STA_HIDX_0_1 + (i /2 * 4);
			isp_hwreg_update_bits(isp_dev, addr, hidx, (i % 2) * 16, 16);
		}
	}
}

void isp_ltm_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_base)
		ltm_cfg_base(isp_dev, &param->base_cfg);

	if (param->pvalid.aisp_ltm)
		ltm_cfg_param(isp_dev, &param->ltm_cfg);

	if (param->pvalid.aisp_ltm_enhc)
		ltm_cfg_enhc(isp_dev, &param->ltm_enhc_cfg);
}

void isp_ltm_stats(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	int i = 0;
	u32 val = 0;
	int idx_blk, idx_blk2;
	aisp_stats_info_t *stats = buff->vaddr[AML_PLANE_A];
	ltm_stats_info_t *ltm = &stats->ltm_stats;

	isp_reg_read_bits(isp_dev, ISP_LTM_FLT_CTRL,
		&ltm->reg_ltm_stat_hblk_num, 4, 4);
	isp_reg_read_bits(isp_dev, ISP_LTM_FLT_CTRL,
		&ltm->reg_ltm_stat_vblk_num, 0, 4);

	val = isp_hwreg_read(isp_dev, ISP_LTM_STA_GLB);
	ltm->ro_ltm_gmax_idx = (val >> 16) & 0xffff;
	ltm->ro_ltm_gmin_idx = val & 0xffff;

	ltm->ro_sum_log_4096_l = isp_hwreg_read(isp_dev, ISP_LTM_STA_HST_SUM_GLBL);
	ltm->ro_sum_log_4096_h = isp_hwreg_read(isp_dev, ISP_LTM_STA_HST_SUM_GLBH);

	isp_hwreg_write(isp_dev, ISP_LTM_RO_GLB_ADDR, 0);
	for (i = 0; i < LTM_STA_BIN_NUM; i++) {
		ltm->ro_ltm_histbuf[i] = isp_hwreg_read(isp_dev, ISP_LTM_RO_GLB_DATA);
	}

	isp_hwreg_write(isp_dev, ISP_LTM_RO_LMIN_ADDR, 0);
	for (i = 0; i < LTM_STA_BLK_REG_NUM; i++) {
		ltm->ro_ltm_lmin_blk_in[i] = isp_hwreg_read(isp_dev, ISP_LTM_RO_LMIN_DATA);
	}

	isp_hwreg_write(isp_dev, ISP_LTM_RO_LMAX_ADDR, 0);
	for (i = 0; i < LTM_STA_BLK_REG_NUM; i++) {
		ltm->ro_ltm_lmax_blk_in[i] = isp_hwreg_read(isp_dev, ISP_LTM_RO_LMAX_DATA);
	}

	isp_hwreg_write(isp_dev, ISP_LTM_RO_BLK_SUM_ADDR, 0);
	idx_blk = isp_hwreg_read(isp_dev, ISP_LTM_RO_BLK_SUM_ADDR);
	if (idx_blk & 1)
	{
		//throw
		val = isp_hwreg_read(isp_dev, ISP_LTM_RO_BLK_SUM_DATA);
		idx_blk=idx_blk + 1;
	}
	idx_blk = idx_blk / 2;
	for (i = idx_blk; i < LTM_STA_BLK_REG_NUM; i++) {
		val = isp_hwreg_read(isp_dev, ISP_LTM_RO_BLK_SUM_DATA);
		ltm->ro_ltm_sta_hst_blk_sum_l_init[i] = val;

		val = isp_hwreg_read(isp_dev, ISP_LTM_RO_BLK_SUM_DATA);
		ltm->ro_ltm_sta_hst_blk_sum_h_init[i] = val & 0xffff;
	}
	idx_blk2 = isp_hwreg_read(isp_dev, ISP_LTM_RO_BLK_SUM_ADDR);
	for (i = 0; i < idx_blk; i++) {
		val = isp_hwreg_read(isp_dev, ISP_LTM_RO_BLK_SUM_DATA);
		ltm->ro_ltm_sta_hst_blk_sum_l_init[i] = val;

		val = isp_hwreg_read(isp_dev, ISP_LTM_RO_BLK_SUM_DATA);
		ltm->ro_ltm_sta_hst_blk_sum_h_init[i] = val & 0xffff;
	}
}

void isp_ltm_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	u32 val = 0;
	aisp_stats_info_t *stats = buff->vaddr[AML_PLANE_A];
	ltm_req_info_t *ltm_info = &stats->module_info.ltm_req_info;

	val = isp_reg_read(isp_dev, ISP_LTM_STA_FLOOR);
	ltm_info->ltm_dark_floor_u8 = (val >> 16) & 0xff;
	ltm_info->ltm_bright_floor_u14 = (val >> 0) & 0x3fff;

	val = isp_reg_read(isp_dev, ISP_LTM_SHRP_EXP_THD);
	ltm_info->ltm_expblend_thd0 = (val >> 0) & 0x3fff;
	ltm_info->ltm_expblend_thd1 = (val >> 16) & 0x3fff;

	val = isp_reg_read(isp_dev, ISP_LTM_POW_CRTL);
	ltm_info->ltm_min_factor_u8 = (val >> 24) & 0xff;
	ltm_info->ltm_max_factor_u8 = (val >> 16) & 0xff;
}

void isp_ltm_init(struct isp_dev_t *isp_dev)
{
	int i = 0;
	int ccrat_lut[63];
	const u32 *ltm_blk65 = ltm_histxptsbuf_blk65;

	for (i = 0; i < 63; i++)
		ccrat_lut[i] = MIN(i * 64 + 64, 4095);

	isp_reg_write(isp_dev, ISP_LTM_STA_THD_ADDR, 0);
	for (i = 0; i < 79; i++)
		isp_reg_write(isp_dev, ISP_LTM_STA_THD_DATA, ltm_blk65[i]);

	isp_reg_write(isp_dev, ISP_LTM_BLK_STA_ADDR, 0);
	for (i = 0; i < (12 * 8); i++)
		isp_reg_write(isp_dev, ISP_LTM_BLK_STA_DATA, 4323);

	isp_reg_write(isp_dev, ISP_LTM_BLK_STA_ADDR, 12 * 8);
	for (i = 0; i < (12 * 8); i++)
		isp_reg_write(isp_dev, ISP_LTM_BLK_STA_DATA, 1048575);

	isp_reg_write(isp_dev, ISP_LTM_CCRAT_ADDR, 0);
	for (i = 0; i < 63; i++)
		isp_reg_write(isp_dev, ISP_LTM_CCRAT_DATA, ccrat_lut[i]);

	isp_reg_write(isp_dev, ISP_LTM_CCALP_ADDR, 0);
	for (i = 0; i < 256; i++)
		isp_reg_write(isp_dev, ISP_LTM_CCALP_DATA, 0);

	isp_reg_update_bits(isp_dev, ISP_LTM_SHRP_CRTL, 32, 8, 6);
	isp_reg_update_bits(isp_dev, ISP_LTM_SHRP_CRTL, 16, 16, 8);

	isp_reg_update_bits(isp_dev, ISP_LTM_STAT_BIN, 0, 0, 7);
	isp_reg_update_bits(isp_dev, ISP_LTM_STAT_BIN, 16, 8, 7);
	isp_reg_update_bits(isp_dev, ISP_LTM_FINAL_CTRL, 50, 24, 6);
	isp_reg_update_bits(isp_dev, ISP_LTM_POW_CRTL, 32, 0, 6);
	isp_reg_update_bits(isp_dev, ISP_LTM_POW_CRTL, 40, 8, 7);
}
