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

static int sky_prot_lut_default1[64] = {1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
		1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024,
		1024, 1024, 1024, 969, 916, 869, 826, 788, 754, 724, 698, 676, 657,
		642, 631, 622, 617, 615, 624, 640, 656, 672, 688, 704, 720, 736, 752,
		768, 784, 800, 816, 832, 848, 864, 880, 896, 912, 928, 944, 960, 976,
		992, 1008, 1024
};

static int lcmap_nodes[96 * 2];
static int dhzmap_nodes[96 * 2];

static void post_pg2_ctrst_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	int i = 0;
	u32 val = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_setting_fixed_cfg_t *fixed_cfg = &base_cfg->fxset_cfg;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	isp_hw_lut_wstart(isp_dev, PST_PG2_CTRST_LUT_CFG);

	isp_reg_update_bits(isp_dev, ISP_LC_CURVE_CTRL, fixed_cfg->curve_lc_en, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_LC_CURVE_CTRL, fixed_cfg->curve_dhz_en, 1, 1);

/* lc_satur_lut */
	isp_reg_write(isp_dev, ISP_LC_SAT_LUT_IDX, 0);
	for (i = 0; i < 31; i++) {
		val = (lut_cfg->lc_satur_lut[i * 2 + 0] << 0) |
			(lut_cfg->lc_satur_lut[i * 2 + 1] << 16);
		isp_reg_write(isp_dev, ISP_LC_SAT_LUT_DATA, val);
	}
	val = lut_cfg->lc_satur_lut[62];
	isp_reg_write(isp_dev, ISP_LC_SAT_LUT_DATA, val);

/* dhz_sky_prot_lut */
	isp_reg_write(isp_dev, ISP_LC_DHZ_SKY_PROT_LUT_IDX, 0);
	for (i = 0; i < 32; i++) {
		val = (lut_cfg->dhz_sky_prot_lut[i * 2 + 0] << 0) |
			(lut_cfg->dhz_sky_prot_lut[i * 2 + 1] << 16);
		isp_reg_write(isp_dev, ISP_LC_DHZ_SKY_PROT_LUT_DATA, val);
	}

/* ram_lcmap_nodes */
	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 1);
	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_ADDR, 0);
	for (i = 0; i < 96; i++) {
		val = (lut_cfg->ram_lcmap_nodes[i * 6 + 0] << 0) |
			(lut_cfg->ram_lcmap_nodes[i * 6 + 1] << 10) |
			(lut_cfg->ram_lcmap_nodes[i * 6 + 2] << 20);
		isp_reg_write(isp_dev, ISP_LC_MAP_RAM_DATA, val);

		val = (lut_cfg->ram_lcmap_nodes[i * 6 + 3] << 0) |
			(lut_cfg->ram_lcmap_nodes[i * 6 + 4] << 10) |
			(lut_cfg->ram_lcmap_nodes[i * 6 + 5] << 20);
		isp_reg_write(isp_dev, ISP_LC_MAP_RAM_DATA, val);
	}
	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 0);

/* ram_dhzmap_nodes */
	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 1);
	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_ADDR, 1 << 7);
	for (i = 0; i < 96; i++) {
		val = (lut_cfg->ram_dhzmap_nodes[i * 5 + 0] << 0) |
			(lut_cfg->ram_dhzmap_nodes[i * 5 + 1] << 10) |
			(lut_cfg->ram_dhzmap_nodes[i * 5 + 2] << 20);
		isp_reg_write(isp_dev, ISP_LC_MAP_RAM_DATA, val);

		val = (lut_cfg->ram_dhzmap_nodes[i * 5 + 3] << 0) |
			(lut_cfg->ram_dhzmap_nodes[i * 5 + 4] << 10);
		isp_reg_write(isp_dev, ISP_LC_MAP_RAM_DATA, val);
	}
	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 0);

	isp_hw_lut_wend(isp_dev);
}

static void post_pg2_ctrst_cfg_lc(struct isp_dev_t *isp_dev, void *lc)
{
	int i = 0;
	u32 val = 0;
	aisp_lc_cfg_t *l_cfg = lc;

	isp_hw_lut_wstart(isp_dev, PST_PG2_CTRST_LC_LUT_CFG);

	val = l_cfg->lc_histvld_thrd;
	isp_reg_write(isp_dev, ISP_LC_CURVE_HISTVLD_THRD, val);

	val = l_cfg->lc_blackbar_mute_thrd;
	isp_reg_write(isp_dev, ISP_LC_CURVE_BB_MUTE_THRD, val);

	val = l_cfg->lc_pk_vld;
	isp_reg_write(isp_dev, ISP_LC_DB_VLD, val);

	val = l_cfg->lc_pk_no_trd_mrgn;
	isp_reg_write(isp_dev, ISP_LC_DB_TRD_MRGN, val);

	val = l_cfg->lc_pk_1stb_th;
	isp_reg_write(isp_dev, ISP_LC_PK_1STB_TH, val);

	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 1);
	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_ADDR, 0);
	for (i = 0; i < 96; i++) {
		val = (l_cfg->ram_lcmap_nodes[i * 6 + 0] << 0) |
			(l_cfg->ram_lcmap_nodes[i * 6 + 1] << 10) |
			(l_cfg->ram_lcmap_nodes[i * 6 + 2] << 20);
		isp_reg_write(isp_dev, ISP_LC_MAP_RAM_DATA, val);

		val = (l_cfg->ram_lcmap_nodes[i * 6 + 3] << 0) |
			(l_cfg->ram_lcmap_nodes[i * 6 + 4] << 10) |
			(l_cfg->ram_lcmap_nodes[i * 6 + 5] << 20);
		isp_reg_write(isp_dev, ISP_LC_MAP_RAM_DATA, val);
	}
	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 0);

	isp_hw_lut_wend(isp_dev);
}

static void post_pg2_ctrst_cfg_lc_enhc(struct isp_dev_t *isp_dev, void *lc_enhc)
{
	int i = 0;
	u32 val = 0;
	aisp_lc_enhc_cfg_t *enhc = lc_enhc;

	isp_hw_lut_wstart(isp_dev, PST_PG2_CTRST_LC_ENHC_LUT_CFG);

	//isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, enhc->lc_en & 0x1, 8, 1);
	isp_reg_update_bits(isp_dev, ISP_LC_TOP_CTRL, enhc->lc_cc_en & 0x1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_LC_TOP_CTRL, enhc->lc_blkblend_mode & 0x1, 4, 1);
	isp_reg_update_bits(isp_dev, ISP_LC_CURVE_LMT_RAT, enhc->lc_lmtrat_minmax & 0xff, 8, 8);

	val = ((enhc->lc_contrast_low & 0x3ff) << 16) | (enhc->lc_contrast_hig & 0x3ff);
	isp_reg_write(isp_dev, ISP_LC_CURVE_CONTRAST_LH, val);

	val = ((enhc->lc_cntstscl_low & 0xff) << 8) | (enhc->lc_cntstscl_hig & 0xff);
	isp_reg_write(isp_dev, ISP_LC_CURVE_CONTRAST_SCL_LH, val);

	val = ((enhc->lc_cntstbvn_low & 0xff) << 8) | (enhc->lc_cntstbvn_hig & 0xff);
	isp_reg_write(isp_dev, ISP_LC_CURVE_CONTRAST_BVN_LH, val);

	val = ((enhc->lc_ypkbv_slope_lmt_0 & 0xff) << 8) | (enhc->lc_ypkbv_slope_lmt_1 & 0xff);
	isp_reg_write(isp_dev, ISP_LC_CURVE_YPKBV_SLP_LMT, val);

	isp_reg_update_bits(isp_dev, ISP_LC_CURVE_YPKBV_RAT, enhc->lc_ypkbv_ratio_1 & 0xff, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_LC_CURVE_YPKBV_RAT, enhc->lc_ypkbv_ratio_2 & 0xff, 8, 8);

	isp_reg_write(isp_dev, ISP_LC_SAT_LUT_IDX, 0);
	for (i = 0; i < 31; i++) {
		val = (enhc->lc_satur_lut[i * 2 + 0] << 0) |
			(enhc->lc_satur_lut[i * 2 + 1] << 16);
		isp_reg_write(isp_dev, ISP_LC_SAT_LUT_DATA, val);
	}
	val = enhc->lc_satur_lut[62];
	isp_reg_write(isp_dev, ISP_LC_SAT_LUT_DATA, val);

	isp_hw_lut_wend(isp_dev);
}

static void post_pg2_ctrst_lc_stats(struct isp_dev_t *isp_dev, void *lc)
{
	int i = 0;
	u32 val = 0;
	lc_stats_info_t *lc_info = lc;

	val = isp_reg_read(isp_dev, ISP_LC_HV_NUM);
	lc_info->reg_lc_blk_hnum_both = (val >> 8) & 0x1f;
	lc_info->reg_lc_blk_vnum_both = val & 0x1f;

	val = isp_reg_read(isp_dev, ISP_LC_STA_HV_NUM);
	lc_info->reg_lc_sta_hnum_both = val & 0x1f;
	lc_info->reg_lc_sta_vnum_both = (val >> 8) & 0x1f;

	val = isp_hwreg_read(isp_dev, ISP_RO_PST_STA);
	lc_info->ro_min_val_glb = (val >> 16) & 0x3ff;
	lc_info->ro_max_val_glb = val & 0x3ff;

	isp_hwreg_write(isp_dev, ISP_CH0_RO_WDR_STAT_FLT_ADDR, 0);
	for (i = 0; i < 1024; i++) {
		val = isp_hwreg_read(isp_dev, ISP_CH0_RO_WDR_STAT_FLT_DATA);
		lc_info->ro_wdr_stat_yblk_flt[i] = val & 0x3fff;
	}

	isp_hwreg_write(isp_dev, ISP_LC_CURVE_RAM_ADDR, 1 << 31);
	val = isp_hwreg_read(isp_dev, ISP_LC_CURVE_RAM_ADDR);
	for (i = 0; i < 96; i++) {
		val = isp_hwreg_read(isp_dev, ISP_LC_CURVE_RO_RAM_DATA);
		lc_info->fw_ram_curve_nodes_in[i * 6 + 0] = (val >> 0) & 0x3ff;
		lc_info->fw_ram_curve_nodes_in[i * 6 + 1] = (val >> 10) & 0x3ff;
		lc_info->fw_ram_curve_nodes_in[i * 6 + 2] = (val >> 20) & 0x3ff;

		val = isp_hwreg_read(isp_dev, ISP_LC_CURVE_RO_RAM_DATA);
		lc_info->fw_ram_curve_nodes_in[i * 6 + 3] = (val >> 0) & 0x3ff;
		lc_info->fw_ram_curve_nodes_in[i * 6 + 4] = (val >> 10) & 0x3ff;
		lc_info->fw_ram_curve_nodes_in[i * 6 + 5] = (val >> 20) & 0x3ff;
	}

	val = 0x200 | (1 << 31);
	isp_hwreg_write(isp_dev, ISP_LC_CURVE_RAM_ADDR, val);
	for (i = 0; i < 96; i++) {
		val = isp_hwreg_read(isp_dev, ISP_LC_CURVE_RO_RAM_DATA);
		lc_info->fw_ram_pk_vals_in[i * 4 + 0] = (val >> 0) & 0xff;
		lc_info->fw_ram_pk_vals_in[i * 4 + 1] = (val >> 16) & 0xff;
		lc_info->fw_ram_pk_idxs_in[i * 4 + 0] = (val >> 8) & 0xf;
		lc_info->fw_ram_pk_idxs_in[i * 4 + 1] = (val >> 24) & 0xf;

		val = isp_hwreg_read(isp_dev, ISP_LC_CURVE_RO_RAM_DATA);
		lc_info->fw_ram_pk_vals_in[i * 4 + 2] = (val >> 0) & 0xff;
		lc_info->fw_ram_pk_vals_in[i * 4 + 3] = (val >> 16) & 0xff;
		lc_info->fw_ram_pk_idxs_in[i * 4 + 2] = (val >> 8) & 0xf;
		lc_info->fw_ram_pk_idxs_in[i * 4 + 3] = (val >> 24) & 0xf;
	}
}

static void post_pg2_ctrst_lc_info(struct isp_dev_t *isp_dev, void *info)
{
	int i = 0;
	u32 val = 0;
	lc_req_info_t *lc_info = info;

	val = isp_reg_read(isp_dev, ISP_LC_CURVE_LMT_RAT);
	lc_info->lc_lmtrat_minmax = (val >> 8) & 0xff;
	lc_info->lc_lmtrat_valid = (val >> 0) & 0xff;

	isp_reg_read_bits(isp_dev, ISP_LC_CURVE_LPF_MOD_TAPS, &lc_info->lc_blackbar_mute_en, 0, 1);

	lc_info->lc_blackbar_mute_thrd = isp_reg_read(isp_dev, ISP_LC_CURVE_BB_MUTE_THRD);
	lc_info->lc_histvld_thrd = isp_reg_read(isp_dev, ISP_LC_CURVE_HISTVLD_THRD);

	val = isp_reg_read(isp_dev, ISP_LC_CURVE_CONTRAST_LH);
	lc_info->lc_contrast_low = (val >> 16) & 0x3ff;
	lc_info->lc_contrast_hig = (val >> 0) & 0x3ff;

	val = isp_reg_read(isp_dev, ISP_LC_CURVE_CONTRAST_LMT_LH);
	lc_info->lc_cntstlmt_low[0] = (val >> 24) & 0xff;
	lc_info->lc_cntstlmt_low[1] = (val >> 8) & 0xff;
	lc_info->lc_cntstlmt_hig[0] = (val >> 16) & 0xff;
	lc_info->lc_cntstlmt_hig[1] = (val >> 0) & 0xff;

	val = isp_reg_read(isp_dev, ISP_LC_CURVE_CONTRAST_SCL_LH);
	lc_info->lc_cntstscl_low = (val >> 8) & 0xff;
	lc_info->lc_cntstscl_hig = (val >> 0) & 0xff;

	val = isp_reg_read(isp_dev, ISP_LC_CURVE_CONTRAST_BVN_LH);
	lc_info->lc_cntstbvn_low = (val >> 8) & 0xff;
	lc_info->lc_cntstbvn_hig = (val >> 0) & 0xff;

	val = isp_reg_read(isp_dev, ISP_LC_CURVE_MISC0);
	lc_info->lc_num_m_coring = (val >> 16) & 0xf;
	lc_info->lc_vbin_min = (val >> 8) & 0xff;
	lc_info->lc_slope_max_face = (val >> 0) & 0xff;

	val = isp_reg_read(isp_dev, ISP_LC_CURVE_YPKBV_RAT);
	lc_info->lc_ypkbv_ratio[0] = (val >> 24) & 0xff;
	lc_info->lc_ypkbv_ratio[1] = (val >> 16) & 0xff;
	lc_info->lc_ypkbv_ratio[2] = (val >> 8) & 0xff;
	lc_info->lc_ypkbv_ratio[3] = (val >> 0) & 0xff;

	val = isp_reg_read(isp_dev, ISP_LC_CURVE_YPKBV_SLP_LMT);
	lc_info->lc_ypkbv_slope_lmt[0] = (val >> 8) & 0xff;
	lc_info->lc_ypkbv_slope_lmt[1] = (val >> 0) & 0xff;

	for (i = 0; i < 8; i++) {
		val = isp_reg_read(isp_dev, ISP_LC_CURVE_YMINVAL_LMT_0 + i * 4);
		lc_info->lc_yminval_lmt[i * 2 + 0] = (val >> 0) & 0x3ff;
		lc_info->lc_yminval_lmt[i * 2 + 1] = (val >> 16) & 0x3ff;

		val = isp_reg_read(isp_dev, ISP_LC_CURVE_YPKBV_LMT_0 + i * 4);
		lc_info->lc_ypkbv_lmt[i * 2 + 0] = (val >> 0) & 0x3ff;
		lc_info->lc_ypkbv_lmt[i * 2 + 1] = (val >> 16) & 0x3ff;

		val = isp_reg_read(isp_dev, ISP_LC_CURVE_YMAXVAL_LMT_0 + i * 4);
		lc_info->lc_ymaxval_lmt[i * 2 + 0] = (val >> 0) & 0x3ff;
		lc_info->lc_ymaxval_lmt[i * 2 + 1] = (val >> 16) & 0x3ff;
	}

	lc_info->lc_pk_vld = isp_reg_read(isp_dev, ISP_LC_DB_VLD);

	isp_reg_read_bits(isp_dev, ISP_LC_HIST_BLK_MISC,  &lc_info->lc_sta_blk_enable, 6, 1);
	isp_reg_read_bits(isp_dev, ISP_POST_STA_GLB_MISC, &lc_info->pst_sta_enable, 0, 1);
	isp_reg_read_bits(isp_dev, VIU_DMAWR_SIZE2,  &lc_info->lc_stat_pack_num, 0, 16);
}

static void post_pg2_ctrst_cfg_dnlp(struct isp_dev_t *isp_dev, void *dnlp)
{
	int i = 0;
	u32 val = 0;
	aisp_dnlp_cfg_t *d_cfg = dnlp;

	for (i = 0; i < 32; i++) {
		val = (d_cfg->dnlp_ygrid[i * 2 + 0] << 0) |
			(d_cfg->dnlp_ygrid[i * 2 + 1] << 16);
		isp_reg_write(isp_dev, ISP_CNTRST_DNLP_YGRID_0 + i * 4, val);
	}
}

static void post_pg2_ctrst_dnlp_stats(struct isp_dev_t *isp_dev, void *dnlp)
{
	int i = 0;
	u64 sum_l, sum_h;
	dnlp_stats_info_t *d_info = dnlp;

	isp_hwreg_update_bits(isp_dev, ISP_POST_TOP_MISC, 1, 4, 28);
	isp_hwreg_write(isp_dev, ISP_POST_STATS_RAM_ADDR, 1 << 31);
	for (i = 0; i < DNLP_STA_BIN_NUM; i++) {
		d_info->ro_dnlp_sta_hst[i] =
			isp_hwreg_read(isp_dev, ISP_POST_RO_STATS_RAM_DATA);
	}

	sum_l = isp_hwreg_read(isp_dev, ISP_POST_RO_STATS_RAM_DATA);
	sum_h = isp_hwreg_read(isp_dev, ISP_POST_RO_STATS_RAM_DATA);

	d_info->ro_dnlp_pix_amount = isp_hwreg_read(isp_dev, ISP_POST_RO_STATS_RAM_DATA);

	d_info->ve_dnlp_luma_sum = (sum_h << 32) | (sum_l << 0);
}


static void post_pg2_ctrst_cfg_dhz(struct isp_dev_t *isp_dev, void *dhz)
{
	int i = 0;
	u32 val = 0;
	aisp_dhz_cfg_t *d_cfg = dhz;

	isp_hw_lut_wstart(isp_dev, PST_PG2_CTRST_DHZ_LUT_CFG);

	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 1);
	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_ADDR, 1 << 7);
	for (i = 0; i < 96; i++) {
		val = (d_cfg->ram_dhz_nodes[i * 5 + 0] << 0) |
			(d_cfg->ram_dhz_nodes[i * 5 + 1] << 10) |
			(d_cfg->ram_dhz_nodes[i * 5 + 2] << 20);
		isp_reg_write(isp_dev, ISP_LC_MAP_RAM_DATA, val);

		val = (d_cfg->ram_dhz_nodes[i * 5 + 3] << 0) |
			(d_cfg->ram_dhz_nodes[i * 5 + 4] << 10);
		isp_reg_write(isp_dev, ISP_LC_MAP_RAM_DATA, val);
	}
	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 0);

	val = d_cfg->dhz_atmos_light;
	isp_reg_update_bits(isp_dev, ISP_LC_DHZ_SKY_PROT0, val, 0, 10);

	val = d_cfg->dhz_atmos_light_inver;
	isp_reg_update_bits(isp_dev, ISP_LC_DHZ_SKY_PROT1, val, 8, 15);

	val = d_cfg->dhz_sky_prot_stre;
	isp_reg_update_bits(isp_dev, ISP_LC_DHZ_SKY_PROT0, val, 12, 10);

	val = d_cfg->dhz_sky_prot_offset;
	isp_reg_update_bits(isp_dev, ISP_LC_DHZ_SKY_TRANS_COEF, val, 0, 10);

	val = d_cfg->dhz_satura_ratio_sky;
	isp_reg_update_bits(isp_dev, ISP_LC_DHZ_SKY_TRANS_COEF, val, 24, 8);

	isp_hw_lut_wend(isp_dev);
}

static void post_pg2_ctrst_dhz_stats(struct isp_dev_t *isp_dev, void *dhz)
{
	int i = 0;
	u32 val = 0;
	dehaze_stats_info_t *d_info = dhz;

	val = isp_hwreg_read(isp_dev, ISP_LC_HV_NUM);
	d_info->reg_dhz_blk_hnum_both = (val >> 8) & 0x1f;
	d_info->reg_dhz_blk_vnum_both = val & 0x1f;

	val = 0x100 | (1 << 31);
	isp_hwreg_write(isp_dev, ISP_LC_CURVE_RAM_ADDR, val);
	for (i = 0; i < 96; i++) {
		val = isp_hwreg_read(isp_dev, ISP_LC_CURVE_RO_RAM_DATA);
		d_info->fw_ram_dhz_nodes_in[i * 5 + 3] = (val >> 0) & 0x3ff;
		d_info->fw_ram_dhz_nodes_in[i * 5 + 4] = (val >> 10) & 0x3fff;

		val = isp_hwreg_read(isp_dev, ISP_LC_CURVE_RO_RAM_DATA);
		d_info->fw_ram_dhz_nodes_in[i * 5 + 0] = (val >> 0) & 0x3ff;
		d_info->fw_ram_dhz_nodes_in[i * 5 + 1] = (val >> 10) & 0x3ff;
		d_info->fw_ram_dhz_nodes_in[i * 5 + 2] = (val >> 20) & 0x3ff;
	}

	val = isp_hwreg_read(isp_dev, ISP_RO_PST_STA);
	d_info->ro_min_val_glb = (val >> 16) & 0x3ff;
	d_info->ro_max_val_glb = val & 0x3ff;
}

static void post_pg2_ctrst_cfg_dhz_enhc(struct isp_dev_t *isp_dev, void *enhc)
{
	int i = 0;
	u32 val = 0;
	aisp_dhz_enhc_cfg_t *e_cfg = enhc;

	isp_hw_lut_wstart(isp_dev, PST_PG2_CTRST_DHZ_ENHC_LUT_CFG);

	//isp_reg_update_bits(isp_dev, ISP_TOP_BED_CTRL, e_cfg->dhz_en & 0x1, 7, 1);
	isp_reg_update_bits(isp_dev, ISP_DHZ_CURVE_RAT, e_cfg->dhz_dlt_rat & 0x7fff, 0, 15);

	val = ((e_cfg->dhz_hig_dlt_rat & 0x3ff) << 16) | (e_cfg->dhz_low_dlt_rat & 0x3ff);
	isp_reg_write(isp_dev, ISP_DHZ_CURVE_DLT_RAT, val);

	val = ((e_cfg->dhz_lmtrat_lowc & 0xfff) << 16) | (e_cfg->dhz_lmtrat_higc & 0xfff);
	isp_reg_write(isp_dev, ISP_DHZ_CURVE_CUT, val);

	isp_reg_update_bits(isp_dev, ISP_LC_TOP_CTRL, e_cfg->dhz_cc_en & 0x1, 8, 1);
	isp_reg_update_bits(isp_dev, ISP_LC_DHZ_SKY_PROT0, e_cfg->dhz_sky_prot_en & 0x1, 25, 1);
	isp_reg_update_bits(isp_dev, ISP_LC_DHZ_SKY_PROT0, e_cfg->dhz_sky_prot_stre & 0x3ff, 12, 10);

	isp_reg_write(isp_dev, ISP_LC_DHZ_SKY_PROT_LUT_IDX, 0);
	for (i = 0; i < 32; i++) {
		val = (e_cfg->dhz_sky_prot_lut[i * 2 + 0] << 0) |
			(e_cfg->dhz_sky_prot_lut[i * 2 + 1] << 16);
		isp_reg_write(isp_dev, ISP_LC_DHZ_SKY_PROT_LUT_DATA, val);
	}

	isp_hw_lut_wend(isp_dev);
}

static void post_pg2_ctrst_dhz_info(struct isp_dev_t *isp_dev, void *info)
{
	int i = 0;
	u32 val = 0;
	dehaze_req_info_t *d_info = info;

	for (i = 0; i < 8; i++) {
		val = isp_reg_read(isp_dev, ISP_DHZ_CURVE_MIN_LMT_0 + i * 4);
		d_info->dhz_minvval_lmt[i * 2 + 0] = (val >> 0) & 0x3ff;
		d_info->dhz_minvval_lmt[i * 2 + 1] = (val >> 16) & 0x3ff;

		val = isp_reg_read(isp_dev, ISP_DHZ_CURVE_MAX_LMT_0 + i * 4);
		d_info->dhz_maxvval_lmt[i * 2 + 0] = (val >> 0) & 0x3ff;
		d_info->dhz_maxvval_lmt[i * 2 + 1] = (val >> 16) & 0x3ff;
	}

	val = isp_reg_read(isp_dev, ISP_DHZ_CURVE_CUT);
	d_info->dhz_lmtrat_lowc = (val >> 16) & 0xfff;
	d_info->dhz_lmtrat_higc = (val >> 0) & 0xfff;

	d_info->dhz_dlt_rat = isp_reg_read(isp_dev, ISP_DHZ_CURVE_RAT);
	isp_reg_read_bits(isp_dev, ISP_LC_TOP_CTRL, &d_info->dhz_cc_en, 8, 1);

	val = isp_reg_read(isp_dev, ISP_DHZ_CURVE_DLT_RAT);
	d_info->dhz_hig_dlt_rat = (val >> 16) & 0x3ff;
	d_info->dhz_low_dlt_rat = (val >> 0 ) & 0x3ff;
}

void isp_post_pg2_ctrst_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	int phidx, thidx, pvidx, tvidx;
	u32 xsize = 0;
	u32 ysize = 0;

	xsize = fmt->width;
	ysize = fmt->height;

	isp_reg_update_bits(isp_dev, ISP_LC_HV_NUM, LTM_STA_LEN_H - 1, 8, 5);
	isp_reg_update_bits(isp_dev, ISP_LC_HV_NUM, LTM_STA_LEN_V - 1, 0, 5);

	phidx = (0 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (1 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_HIDX_0, val);

	phidx = (2 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (3 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_HIDX_1, val);

	phidx = (4 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (5 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_HIDX_2, val);

	phidx = (6 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (7 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_HIDX_3, val);

	phidx = (8 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (9 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_HIDX_4, val);

	phidx = (10 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize);
	thidx = (11 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_HIDX_5, val);

	phidx = xsize;
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_HIDX_12, phidx);


	pvidx = 0 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = 1 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_VIDX_0, val);

	pvidx = 2 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = 3 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_VIDX_1, val);

	pvidx = 4 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = 5 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_VIDX_2, val);

	pvidx = 6 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = 7 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_VIDX_3, val);

	pvidx = ysize;
	isp_reg_write(isp_dev, ISP_LC_CURVE_BLK_VIDX_8, pvidx);
}

void isp_post_pg2_ctrst_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param)
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
		for (i = 0; i < LTM_STA_LEN_H; i++) {
			hidx = (i == (LTM_STA_LEN_H - 1)) ? hsize : (i * hsize / MAX(1, LTM_STA_LEN_H - 1));
			slice_size = (param->pleft_hsize) +
				hsize / (LTM_STA_LEN_H - 1) -
				(param->pleft_ovlp) +
				(i - (LTM_STA_LEN_H - 1) / 2 - 1) * 4;
			hidx = MIN(MAX(hidx, 0), slice_size);

			addr = ISP_LC_CURVE_BLK_HIDX_0 + (i /2 * 4);
			isp_hwreg_update_bits(isp_dev, addr, hidx, (i % 2) * 16, 16);
		}
	} else if (param->pos == 1) {
		slice_size = param->pright_hsize;
		ovlp = param->pright_ovlp;

		for (i = 0; i < LTM_STA_LEN_H; i++) {
			hidx = (i == (LTM_STA_LEN_H - 1)) ? hsize : (i * hsize / MAX(1, LTM_STA_LEN_H - 1));
			hidx = MIN(MAX(hidx + ovlp * 2 - slice_size,
					(ovlp -hsize /(LTM_STA_LEN_H - 1) +
					(i - (LTM_STA_LEN_H - 1) / 2 + 1) * 4)),
					slice_size);

			addr = ISP_LC_CURVE_BLK_HIDX_0 + (i /2 * 4);
			isp_hwreg_update_bits(isp_dev, addr, hidx, (i % 2) * 16, 16);
		}
	}
}

void isp_post_pg2_ctrst_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_base)
		post_pg2_ctrst_cfg_base(isp_dev, &param->base_cfg);

	if (param->pvalid.aisp_lc)
		post_pg2_ctrst_cfg_lc(isp_dev, &param->lc_cfg);

	if (param->pvalid.aisp_lc_enhc)
		post_pg2_ctrst_cfg_lc_enhc(isp_dev, &param->lc_enhc_cfg);

	if (param->pvalid.aisp_dnlp)
		post_pg2_ctrst_cfg_dnlp(isp_dev, &param->dnlp_cfg);

	if (param->pvalid.aisp_dhz)
		post_pg2_ctrst_cfg_dhz(isp_dev, &param->dhz_cfg);

	if (param->pvalid.aisp_dhz_enhc)
		post_pg2_ctrst_cfg_dhz_enhc(isp_dev, &param->dhz_enhc_cfg);
}

void isp_post_pg2_ctrst_stats(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_stats_info_t *stats = buff->vaddr[AML_PLANE_A];

	post_pg2_ctrst_lc_stats(isp_dev, &stats->lc_stats);
	post_pg2_ctrst_dnlp_stats(isp_dev, &stats->dnlp_stats);
	post_pg2_ctrst_dhz_stats(isp_dev, &stats->dehaze_stats);
}

void isp_post_pg2_ctrst_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_stats_info_t *stats = buff->vaddr[AML_PLANE_A];

	post_pg2_ctrst_lc_info(isp_dev, &stats->module_info.lc_req_info);
	post_pg2_ctrst_dhz_info(isp_dev, &stats->module_info.dehaze_req_info);
}

void isp_post_pg2_ctrst_init(struct isp_dev_t *isp_dev)
{
	int i = 0;
	u32 val = 0;
	u32 satur_lut_default[63];
	int *ram_lcmap_nodes = lcmap_nodes;
	int *ram_dhzmap_nodes = dhzmap_nodes;
	int *sky_lut = sky_prot_lut_default1;

	for (i = 0; i < 63; i++)
		satur_lut_default[i] = MIN(i * 64 + 64, 4095);

	for (i = 0; i < 96; i++) {
		ram_lcmap_nodes[2 * i] = (0 << 0) | (0 << 10) | (512 << 20);
		ram_lcmap_nodes[2 * i + 1] = (1023 << 0) | (1023 << 10) | (512 << 20);
		ram_dhzmap_nodes[2 * i] = (0 << 0) | (1023 << 10) | (0<<20);
		ram_dhzmap_nodes[2 * i + 1] = (1023 << 0) | (1024 << 10);
	}

	isp_reg_write(isp_dev, ISP_LC_SAT_LUT_IDX, 0);
	for (i = 0; i < (63 >> 1); i++) {
		val = satur_lut_default[i * 2] | (satur_lut_default[i * 2 + 1] << 16);
		isp_reg_write(isp_dev, ISP_LC_SAT_LUT_DATA, val);
	}
	val = satur_lut_default[62];
	isp_reg_write(isp_dev, ISP_LC_SAT_LUT_DATA, val);

	isp_reg_write(isp_dev, ISP_LC_DHZ_SKY_PROT_LUT_IDX, 0);
	for (i = 0; i < 64; i++)
		isp_reg_write(isp_dev, ISP_LC_DHZ_SKY_PROT_LUT_DATA, sky_lut[i]);

	//isp_reg_update_bits(isp_dev, ISP_LC_HIST_BLK_MISC, 1, 6, 1);

	isp_reg_update_bits(isp_dev, ISP_POST_TOP_MISC, 1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_POST_STA_GLB_MISC, 1, 1, 1);
	isp_reg_update_bits(isp_dev, ISP_LC_CURVE_YPKBV_SLP_LMT, 64, 8, 8);

	isp_reg_update_bits(isp_dev, ISP_CNTRST_DNLP_CTRL, 5, 0, 3);

	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 0x1);
	isp_reg_update_bits(isp_dev, ISP_LC_MAP_RAM_ADDR, 0, 31, 1);
	isp_reg_update_bits(isp_dev, ISP_LC_MAP_RAM_ADDR, 0, 0, 8);
	for (i = 0; i < 192; i++)
		isp_reg_write(isp_dev, ISP_LC_MAP_RAM_DATA, ram_lcmap_nodes[i]);

	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 0x0);

	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 0x1);
	isp_reg_update_bits(isp_dev, ISP_LC_MAP_RAM_ADDR, 0, 31, 1);
	isp_reg_update_bits(isp_dev, ISP_LC_MAP_RAM_ADDR, (1 << 7), 0, 8);
	for (i = 0; i < 192; i++)
		isp_reg_write(isp_dev, ISP_LC_MAP_RAM_DATA, ram_dhzmap_nodes[i]);

	isp_reg_write(isp_dev, ISP_LC_MAP_RAM_CTRL, 0x0);

	isp_reg_write(isp_dev, ISP_CNTRST_PK_CLR_PRCT_LUT_14, 0);
	isp_reg_write(isp_dev, ISP_CNTRST_PK_CLR_PRCT_LUT_15, 0);
	isp_reg_write(isp_dev, ISP_CNTRST_PK_CLR_PRCT_LUT_16, 0);
	val = (0 << 0) | (3 << 12) | (6 << 16) | (9 << 20) | (12 << 24) | (15 << 28);
	isp_reg_write(isp_dev, ISP_CNTRST_PK_CLR_PRCT_LUT_17, val);
	val = (0 << 16) | (3 << 12) | (6 << 8) | (9 << 4) | (12 << 0);
	isp_reg_write(isp_dev, ISP_CNTRST_PK_CLR_PRCT_LUT_13, val);
	isp_reg_write(isp_dev, ISP_CNTRST_PK_CLR_PRCT_GAIN, 128);

	isp_reg_update_bits(isp_dev, ISP_CNTRST_SAT_PRT_CRTL, 0, 0, 1);
	isp_reg_write(isp_dev, ISP_CNTRST_SAT_PRT_LMT_0_1, (4095 << 16) | 4095);
	isp_reg_write(isp_dev, ISP_CNTRST_SAT_PRT_LMT_2, 4095);

}
