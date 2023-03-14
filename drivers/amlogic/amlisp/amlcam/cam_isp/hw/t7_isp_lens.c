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

#define LNS_MESH_XNUM       (32)
#define LNS_MESH_YNUM       (32)

static const u32 l_eotf0[] = {
	0, 32768, 65536, 98304, 131072, 163840, 196608, 229376,262144,
	294912, 327680, 360448, 393216, 425984, 458752, 491520, 524288,
	557056, 589824, 622592, 655360, 688128,720896,753664, 786432, 819200,
	851968, 884736, 917504, 950272, 983040, 1015808, 1048575
};

static const u32 l_eotf4[] = {
	0, 512, 1027, 1542, 2060, 2578, 3099, 3621,
	4144, 4669, 5196, 5724, 6253, 6785, 7318, 7852,
	8388, 8926, 9465, 10006, 10549, 11093, 11639, 12186,
	12735, 13286, 13839, 14393, 14949, 15506, 16066, 16627,
	17189, 18320, 19458, 20602, 21754, 22913, 24080, 25253,
	26434, 27623, 28819, 30022, 31234,  32453, 33680, 34914,
	36157, 38667, 41210, 43786, 46397,  49042, 51723, 54440,
	57195, 59987, 62817, 65688, 68598,  71549, 74543, 77579,
	80659, 83784, 86955, 90172, 93437,  96751, 100115, 103530,
	106997, 110518, 114093, 117725, 121414, 125161, 128969, 132838,
	136770, 140767, 144830, 148962, 153162, 157434, 161780, 166200,
	170698, 175275, 179933, 184674, 189501, 194416, 199422, 204521,
	209715, 215007, 220401, 225899, 231503, 237218, 243047, 248992,
	255059, 261249, 267567, 274018, 280604, 287332, 294204, 301227,
	308404, 323245, 338770, 355029, 372075, 389966, 408766, 428548,
	449389, 494611, 545259, 602373, 667275, 741675, 827823, 928738,1048575
};

static void lswb_idg_cfg_gain(struct isp_dev_t *isp_dev, void *gain)
{
	u32 val = 0;
	aisp_dgain_cfg_t *dgain = gain;

	val = (dgain->idg_gain[1] << 0) | (dgain->idg_gain[0] << 16);
	isp_reg_write(isp_dev, ISP_LSWB_IDG_GAIN0, val);

	val = (dgain->idg_gain[3] << 0) | (dgain->idg_gain[2] << 16);
	isp_reg_write(isp_dev, ISP_LSWB_IDG_GAIN1, val);

	val = (dgain->idg_gain[4] << 0);
	isp_reg_write(isp_dev, ISP_LSWB_IDG_GAIN2, val);
}

static void lswb_wb_cfg_gain(struct isp_dev_t *isp_dev, void *change)
{
	u32 val = 0;
	aisp_wb_change_cfg_t *wb_chg = change;

	val = (wb_chg->wb_gain[1] << 0) | (wb_chg->wb_gain[0] << 16);
	isp_reg_write(isp_dev, ISP_LSWB_WB_GAIN0, val);

	val = (wb_chg->wb_gain[3] << 0) | (wb_chg->wb_gain[2] << 16);
	isp_reg_write(isp_dev, ISP_LSWB_WB_GAIN1, val);

	val = wb_chg->wb_gain[4];
	isp_reg_update_bits(isp_dev, ISP_LSWB_WB_GAIN2, val, 0, 12);
}

static void lswb_wb_cfg_limit(struct isp_dev_t *isp_dev, void *change)
{
	u32 val = 0;
	aisp_wb_change_cfg_t *wb_chg = change;

	val = (wb_chg->wb_limit[1] << 0) | (wb_chg->wb_limit[0] << 16);
	isp_reg_write(isp_dev, ISP_LSWB_WB_LIMIT0, val);

	val = (wb_chg->wb_limit[3] << 0) | (wb_chg->wb_limit[2] << 16);
	isp_reg_write(isp_dev, ISP_LSWB_WB_LIMIT1, val);

	val = wb_chg->wb_limit[4];
	isp_reg_write(isp_dev, ISP_LSWB_WB_LIMIT2, val);
}

static void lswb_wb_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	aisp_base_cfg_t *base_cfg = base;
	aisp_setting_fixed_cfg_t *fixed_cfg = &base_cfg->fxset_cfg;

	isp_reg_update_bits(isp_dev, ISP_LSWB_WB_GAIN2, fixed_cfg->wb_rate_rs, 16, 2);
}

static void lswb_eotf_init(struct isp_dev_t *isp_dev)
{
	int i = 0;
	u32 val = 0;
	u32 stp[] = {11,12,13,13,13,13,14,15};
	u32 num[] = {5, 4, 4, 4, 4, 4, 3, 3};
	const u32 *eotf0 = l_eotf0;
	const u32 *eotf4 = l_eotf4;

	val = (1 << 2) | (1 << 1) | (1 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_EOTF_PARAM, val);

	isp_reg_write(isp_dev, ISP_LNS_EOTF0_LUT_ADDR, 0x0);
	for (i = 0; i < 33; i++)
		isp_reg_write(isp_dev, ISP_LNS_EOTF0_LUT_DATA, eotf0[i]);

	isp_reg_write(isp_dev, ISP_LNS_EOTF1_LUT_ADDR, 0x0);
	for (i = 0; i < 129; i++)
		isp_reg_write(isp_dev, ISP_LNS_EOTF1_LUT_DATA, eotf4[i]);

	val = (num[0] << 27) | (num[1] << 24) |
		(num[2] << 19) | (num[3] << 16) |
		(num[4] << 11) | (num[5] << 8) |
		(num[6] << 3) | (num[7] << 0);
	isp_reg_write(isp_dev, ISP_LSWB_EOTF_NUM, val);

	val = (stp[0] << 28) | (stp[1] << 24) |
		(stp[2] << 20) | (stp[3] << 16) |
		(stp[4] << 12) | (stp[5] << 8) |
		(stp[6] << 4) | (stp[7] << 0);
	isp_reg_write(isp_dev, ISP_LSWB_EOTF_STP, val);

	isp_reg_write(isp_dev, ISP_LSWB_EOTF_OFST0, 0);
	isp_reg_write(isp_dev, ISP_LSWB_EOTF_OFST1, 0);
}

static void lswb_eotf_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	int i = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_setting_fixed_cfg_t *fixed_cfg = &base_cfg->fxset_cfg;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	isp_hw_lut_wstart(isp_dev, LSWB_EOTF_LUT_CFG);

	isp_reg_update_bits(isp_dev, ISP_LSWB_EOTF_PARAM, fixed_cfg->eotf1_mode, 0, 1);

	isp_reg_write(isp_dev, ISP_LNS_EOTF0_LUT_ADDR, 0);
	for (i = 0; i < ISP_ARRAY_SIZE(lut_cfg->eotf0_lut); i++) {
		isp_reg_write(isp_dev, ISP_LNS_EOTF0_LUT_DATA, lut_cfg->eotf0_lut[i]);
	}

	isp_reg_write(isp_dev, ISP_LNS_EOTF1_LUT_ADDR, 0);
	for (i = 0; i < ISP_ARRAY_SIZE(lut_cfg->eotf1_lut); i++) {
		isp_reg_write(isp_dev, ISP_LNS_EOTF1_LUT_DATA, lut_cfg->eotf1_lut[i]);
	}

	isp_hw_lut_wend(isp_dev);
}

static void lswb_eotf_cfg_ofst(struct isp_dev_t *isp_dev, void *blc)
{
	aisp_blc_cfg_t *blc_cfg = blc;

	isp_reg_write(isp_dev, ISP_LSWB_EOTF_OFST0, blc_cfg->eotf_pre_ofst);
	isp_reg_write(isp_dev, ISP_LSWB_EOTF_OFST1, blc_cfg->eotf_pst_ofst);
}

static void lswb_idg_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	val = (4096 << 16) | (4096 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_IDG_GAIN0, val);
	val = (4096 << 16) | (4096 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_IDG_GAIN1, val);
	val = (4096 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_IDG_GAIN2, val);

	isp_reg_update_bits(isp_dev, ISP_LSWB_IDG_OFST, 0, 4, 16);
}

static void lswb_idg_cfg_ofst(struct isp_dev_t *isp_dev, void *blc)
{
	aisp_blc_cfg_t *blc_cfg = blc;

	isp_reg_update_bits(isp_dev, ISP_LSWB_IDG_OFST, blc_cfg->idg_ofst, 4, 16);
}

static void lswb_blc_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	val = (0 << 4) | (0 << 3) | (0 << 2) | (0 << 1) | (0 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_BLC_MODE, val);

	val = (0 << 16) | (0 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_BLC_OFST0, val);
	val = (0 << 16) | (0 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_BLC_OFST1, val);
	val = (0 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_BLC_OFST2, val);
}

static void lswb_blc_cfg_ofst(struct isp_dev_t *isp_dev, void *blc)
{
	u32 val = 0;
	aisp_blc_cfg_t *blc_cfg = blc;

	val = (blc_cfg->blc_ofst[0] << 16) | (blc_cfg->blc_ofst[1] << 0);
	isp_reg_write(isp_dev, ISP_LSWB_BLC_OFST0, val);

	val = (blc_cfg->blc_ofst[2] << 16) | (blc_cfg->blc_ofst[3] << 0);
	isp_reg_write(isp_dev, ISP_LSWB_BLC_OFST1, val);

	val = blc_cfg->blc_ofst[4];
	isp_reg_write(isp_dev, ISP_LSWB_BLC_OFST2, val);
}

static void lswb_wb_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_WB_GAIN0, val);
	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_WB_GAIN1, val);
	val = (0 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_WB_GAIN2, val);

	val = (0x9fff << 16) | (0x9fff << 0);
	isp_reg_write(isp_dev, ISP_LSWB_WB_LIMIT0, val);
	val = (0x9fff << 16) | (0x9fff << 0);
	isp_reg_write(isp_dev, ISP_LSWB_WB_LIMIT1, val);
	val = (0x9fff << 0);
	isp_reg_write(isp_dev, ISP_LSWB_WB_LIMIT2, val);
}

static void lswb_rad_init(struct isp_dev_t *isp_dev)
{
	int i = 0;
	u32 val = 0;

	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_PARAM, 1, 11, 1);
	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_PARAM, 0, 10, 1);
	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_PARAM, 0, 9, 1);

	isp_reg_write(isp_dev, ISP_LSWB_RS_XSCALE, 8738);
	isp_reg_write(isp_dev, ISP_LSWB_RS_YSCALE, 15534);
	isp_reg_write(isp_dev, ISP_LSWB_RS_CENTER, 0);

	val = (1448 << 0) | (0 << 12) | (0 << 16);
	isp_reg_write(isp_dev, ISP_LSWB_RS_CENTEROFST_0, val);
	val = (1448 << 0) | (0 << 12) | (0 << 16);
	isp_reg_write(isp_dev, ISP_LSWB_RS_CENTEROFST_1, val);
	val = (1448 << 0) | (0 << 12) | (0 << 16);
	isp_reg_write(isp_dev, ISP_LSWB_RS_CENTEROFST_2, val);
	val = (1448 << 0) | (0 << 12) | (0 << 16);
	isp_reg_write(isp_dev, ISP_LSWB_RS_CENTEROFST_3, val);

	val = (4096 << 0) | (4096 << 16);
	isp_reg_write(isp_dev, ISP_LNS_RAD_LUT_ADDR, 0x0);
	for (i = 0; i < 258; i++)
		isp_reg_write(isp_dev, ISP_LNS_RAD_LUT_DATA, val);
}

static void lswb_rad_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	int i = 0, j = 0;
	u32 val = 0;
	u32 idx0, idx1;
	aisp_base_cfg_t *base_cfg = base;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	isp_hw_lut_wstart(isp_dev, LSWB_RAD_LUT_CFG);

	isp_reg_write(isp_dev, ISP_LNS_RAD_LUT_ADDR, 0);
	for (j = 0; j < 4; j++) {
		for (i = 0; i < 64; i++) {
			idx0 = (i * 2 + 0) * 4 + j;
			idx1 = (i * 2 + 1) * 4 + j;
			val = (lut_cfg->lns_rad_lut129[idx0] << 0) |
				(lut_cfg->lns_rad_lut129[idx1] << 16);
			isp_reg_write(isp_dev, ISP_LNS_RAD_LUT_DATA, val);
		}

		idx0 = 128 * 4 + j;
		val = lut_cfg->lns_rad_lut129[idx0];
		isp_reg_write(isp_dev, ISP_LNS_RAD_LUT_DATA, val);
	}

	isp_hw_lut_wend(isp_dev);
}

static void lswb_rad_cfg_strength(struct isp_dev_t *isp_dev, void *lns)
{
	u32 val = 0;
	aisp_lns_cfg_t *lns_cfg = lns;

	val = lns_cfg->lns_rad_strength;
	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_PARAM, val, 0, 9);
}

static void lswb_mesh_init(struct isp_dev_t *isp_dev)
{
	int i = 0;
	u32 val = 0;

	val = (1 << 19) | (0 << 18) |
		(2 << 16) | (0 << 15) |
		(512 << 1) | (0 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_MS_PARAM, val);

	val = (64 << 24) | (64 << 16) | (64 << 8) | (64 << 0);
	isp_reg_write(isp_dev, ISP_LNS_MESH_LUT_ADDR, 0x0);
	for (i = 0; i < 129; i++)
		isp_reg_write(isp_dev, ISP_LNS_MESH_LUT_DATA, val);

	val = (255 << 24) | (0 << 16) |
		(0 << 8) | (0 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_MS_ALP, val);

	isp_reg_write(isp_dev, ISP_LSWB_MS_XSHIFT, 0);
	isp_reg_write(isp_dev, ISP_LSWB_MS_YSHIFT, 0);
	val = (32 << 16) | (32 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_MS_NUM, val);
}

static void lswb_mesh_cfg_param(struct isp_dev_t *isp_dev, void *mesh)
{
	u32 val = 0;
	aisp_mesh_cfg_t *mesh_cfg = mesh;

	val = (mesh_cfg->lns_mesh_xnum << 16) | (mesh_cfg->lns_mesh_ynum << 0);
	isp_reg_write(isp_dev, ISP_LSWB_MS_NUM, val);

	val = mesh_cfg->lns_mesh_alpmode;
	isp_reg_update_bits(isp_dev, ISP_LSWB_MS_PARAM, val , 16, 2);

	val = (mesh_cfg->lns_mesh_xlimit << 8) | (mesh_cfg->lns_mesh_ylimit << 0);
	isp_reg_write(isp_dev, ISP_LSWB_MS_LIMIT, val);

	//val = mesh_cfg->lns_mesh_xscale;
	//isp_reg_write(isp_dev, ISP_LSWB_MS_XSCALE, val);

	//val = mesh_cfg->lns_mesh_yscale;
	//isp_reg_write(isp_dev, ISP_LSWB_MS_YSCALE, val);

	val = mesh_cfg->lns_mesh_prtmode;
	isp_reg_update_bits(isp_dev, ISP_LSWB_MS_PARAM, val , 15, 1);

	isp_reg_write(isp_dev, ISP_LSWB_MS_LUTNORM_0, mesh_cfg->lns_mesh_lutnorm_grbg[0]);
	isp_reg_write(isp_dev, ISP_LSWB_MS_LUTNORM_1, mesh_cfg->lns_mesh_lutnorm_grbg[1]);
	isp_reg_write(isp_dev, ISP_LSWB_MS_LUTNORM_2, mesh_cfg->lns_mesh_lutnorm_grbg[2]);
	isp_reg_write(isp_dev, ISP_LSWB_MS_LUTNORM_3, mesh_cfg->lns_mesh_lutnorm_grbg[3]);

	val = (mesh_cfg->lns_mesh_alp[0] << 24) | (mesh_cfg->lns_mesh_alp[1] << 16);
	val |= (mesh_cfg->lns_mesh_alp[2] << 8) | (mesh_cfg->lns_mesh_alp[3] << 0);
	isp_reg_write(isp_dev, ISP_LSWB_MS_ALP, val);
}

static void lswb_mesh_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	int i = 0;
	u32 val = 0;
	u32 cnt = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	isp_hw_lut_wstart(isp_dev, LSWB_MESH_LUT_CFG);

	cnt = ISP_ARRAY_SIZE(lut_cfg->lns_mesh_lut) / 4;

	isp_reg_write(isp_dev, ISP_LNS_MESH_LUT_ADDR, 0);
	for (i = 0; i < cnt; i++) {
		val = (lut_cfg->lns_mesh_lut[i * 4 + 0] << 0) |
			(lut_cfg->lns_mesh_lut[i * 4 + 1] << 8) |
			(lut_cfg->lns_mesh_lut[i * 4 + 2] << 16) |
			(lut_cfg->lns_mesh_lut[i * 4 + 3] << 24);

		isp_reg_write(isp_dev, ISP_LNS_MESH_LUT_DATA, val);
	}

	isp_hw_lut_wend(isp_dev);
}

static void lswb_mesh_cfg_strength(struct isp_dev_t *isp_dev, void *lns)
{
	u32 val = 0;
	aisp_lns_cfg_t *lns_cfg = lns;

	val = lns_cfg->lns_mesh_strength;
	isp_reg_update_bits(isp_dev, ISP_LSWB_MS_PARAM, val, 1, 12);
}

static void lswb_mesh_cfg_crt(struct isp_dev_t *isp_dev, void *crt)
{
	u32 val = 0, i = 0;
	aisp_mesh_crt_cfg_t *crt_cfg = crt;

	isp_hw_lut_wstart(isp_dev, LSWB_MESH_CRT_LUT_CFG);

	val = (crt_cfg->lns_mesh_alp[0] << 24) |
		(crt_cfg->lns_mesh_alp[1] << 16) |
		(crt_cfg->lns_mesh_alp[2] << 8) |
		(crt_cfg->lns_mesh_alp[3] << 0);
	isp_reg_write(isp_dev, ISP_LSWB_MS_ALP, val);

	isp_reg_write(isp_dev, ISP_CUBIC_LUT65_ADDR, 0);
	for (i = 0; i < 16; i++) {
		val = (crt_cfg->rad_lut65[i * 4 + 0] << 0) |
			(crt_cfg->rad_lut65[i * 4 + 1] << 8) |
			(crt_cfg->rad_lut65[i * 4 + 2] << 16) |
			(crt_cfg->rad_lut65[i * 4 + 3] << 24);
		isp_reg_write(isp_dev, ISP_CUBIC_LUT65_DATA, val);
	}

	val = crt_cfg->rad_lut65[64];
	isp_reg_write(isp_dev, ISP_CUBIC_LUT65_DATA, val);

	isp_hw_lut_wend(isp_dev);
}

void isp_lens_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
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

	isp_reg_update_bits(isp_dev, ISP_LSWB_IDG_OFST, xofst, 2, 2);
	isp_reg_update_bits(isp_dev, ISP_LSWB_IDG_OFST, yofst, 0, 2);

	isp_reg_update_bits(isp_dev, ISP_LSWB_BLC_PHSOFST, xofst, 2, 2);
	isp_reg_update_bits(isp_dev, ISP_LSWB_BLC_PHSOFST, yofst, 0, 2);

	isp_reg_update_bits(isp_dev, ISP_LSWB_WB_PHSOFST, xofst, 2, 2);
	isp_reg_update_bits(isp_dev, ISP_LSWB_WB_PHSOFST, yofst, 0, 2);

	isp_reg_update_bits(isp_dev, ISP_LSWB_LNS_PHSOFST, xofst, 2, 2);
	isp_reg_update_bits(isp_dev, ISP_LSWB_LNS_PHSOFST, yofst, 0, 2);
}

u16 sqrt32( u32 arg )
{
	u32 mask = (u32)1 << 15;
	u16 res = 0;
	int i = 0;

	for ( i = 0; i < 16; i++ ) {
	if ( ( res + ( mask >> i ) ) * ( res + ( mask >> i ) ) <= arg )
		res = res + ( mask >> i );
	}
	return res;
}

void isp_lens_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u64 ptmp = 0;
	u32 xsize = fmt->width;
	u32 ysize = fmt->height;

	val = (1 << 24) / (xsize >> 1);
	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_XSCALE, val, 0, 16);
	val = (1 << 24) / (ysize >> 1);
	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_YSCALE, val, 0, 16);

	val = (1 << 11) * xsize / sqrt32(xsize * xsize + ysize * ysize);
	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_CENTEROFST_0, val, 0, 12);
	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_CENTEROFST_1, val, 0, 12);
	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_CENTEROFST_2, val, 0, 12);
	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_CENTEROFST_3, val, 0, 12);

	val = (xsize >> 1);
	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_CENTER, val, 16, 16);
	val = (ysize >> 1);
	isp_reg_update_bits(isp_dev, ISP_LSWB_RS_CENTER, val, 0, 16);

	val = isp_reg_read(isp_dev, ISP_LSWB_MS_NUM);
	val = (((val >> 16) & 0x3f) - 1) * 4096 * 1024 / MAX(xsize, 256);
	isp_reg_update_bits(isp_dev, ISP_LSWB_MS_XSCALE, val, 0, 20);

	val = isp_reg_read(isp_dev, ISP_LSWB_MS_NUM);
	val = ((val & 0x3f) - 1) * 4096 * 1024 / MAX(ysize, 256);
	isp_reg_update_bits(isp_dev, ISP_LSWB_MS_YSCALE, val, 0, 20);

	val = (LNS_MESH_XNUM - 1) * 4;
	isp_reg_update_bits(isp_dev, ISP_LSWB_MS_LIMIT, val, 8, 8);
	val = (LNS_MESH_YNUM - 1) * 4;
	isp_reg_update_bits(isp_dev, ISP_LSWB_MS_LIMIT, val, 0, 8);
}

void isp_lens_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param)
{
	u32 val = 0;
	u32 hsize, vsize;
	u32 hnum, vnum;

	hsize = param->whole_frame_hcenter * 2;

	val = isp_hwreg_read(isp_dev, ISP_CROP_SIZE);
	vsize = val & 0xffff;

	val = isp_reg_read(isp_dev, ISP_LSWB_MS_NUM);
	hnum = (val >> 16) & 0x7f;
	vnum = val & 0x7f;

	val = (hnum - 1) * 4096 * 1024 / MAX(hsize, 256);
	isp_hwreg_update_bits(isp_dev, ISP_LSWB_MS_XSCALE, val, 0, 20);

	val = (vnum - 1) * 4096 * 1024 / MAX(vsize, 256);
	isp_hwreg_update_bits(isp_dev, ISP_LSWB_MS_YSCALE, val, 0, 20);

	if (param->pos == 0) {
		val = param->pleft_hsize - param->pleft_ovlp;
		isp_hwreg_update_bits(isp_dev, ISP_LSWB_RS_CENTER, val, 16, 16);
		isp_hwreg_update_bits(isp_dev, ISP_CUBIC_RAD_CENTER, val, 16, 16);
		isp_hwreg_update_bits(isp_dev, ISP_LSWB_MS_XSHIFT, 0, 0, 18);
	} else if (param->pos == 1) {
		val = param->pright_ovlp;
		isp_hwreg_update_bits(isp_dev, ISP_LSWB_RS_CENTER, val, 16, 16);
		isp_hwreg_update_bits(isp_dev, ISP_CUBIC_RAD_CENTER, val, 16, 16);

		val = isp_reg_read(isp_dev, ISP_LSWB_MS_NUM);
		val = (val >> 16) & 0x7f;
		val = ((val - 1) << 12) / 2;
		isp_hwreg_update_bits(isp_dev, ISP_LSWB_MS_XSHIFT, val, 0, 18);
	}
}

void isp_lens_cfg_ofst(struct isp_dev_t *isp_dev)
{
	u32 val = 0;
	u32 tp, pp;
	u32 bl[4] = {240, 240, 240, 240};

	tp = isp_hw_float_convert(((1 << 20) - 1) - (bl[0] << (20 - 12)));
	pp = isp_hw_float_convert(((1 << 20) - 1) - (bl[1] << (20 -12)));
	val = (tp << 16) | (pp << 0);
	isp_reg_write(isp_dev, ISP_LSWB_WB_LIMIT0, val);

	tp = isp_hw_float_convert(((1 << 20) - 1) - (bl[2] << (20 - 12)));
	pp = isp_hw_float_convert(((1 << 20) - 1) - (bl[3] << (20 -12)));
	val = (tp << 16) | (pp << 0);
	isp_reg_write(isp_dev, ISP_LSWB_WB_LIMIT1, val);

	val = isp_hw_float_convert(((1 << 20) - 1) - (bl[0] << (20 - 12)));
	isp_reg_write(isp_dev, ISP_LSWB_WB_LIMIT2, val);

	val = (256 << 16) | (450 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_WB_GAIN0, val);

	val = (432 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_WB_GAIN1, val);

	val = (256 << 0) | (0 << 16);
	isp_reg_write(isp_dev, ISP_LSWB_WB_GAIN2, val);

	val = (4096 << 16) | (4096 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_IDG_GAIN0, val);

	val = (4096 << 16) | (4096 << 0);
	isp_reg_write(isp_dev, ISP_LSWB_IDG_GAIN1, val);

	isp_reg_write(isp_dev, ISP_LSWB_IDG_GAIN2, 4096);
}

void isp_lens_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_dgain)
		lswb_idg_cfg_gain(isp_dev, &param->dgain);

	if (param->pvalid.aisp_wb_change) {
		lswb_wb_cfg_gain(isp_dev, &param->wb_change);
		lswb_wb_cfg_limit(isp_dev, &param->wb_change);
	}

	if (param->pvalid.aisp_mesh)
		lswb_mesh_cfg_param(isp_dev, &param->mesh_cfg);

	if (param->pvalid.aisp_base) {
		lswb_eotf_cfg_base(isp_dev, &param->base_cfg);
		lswb_wb_cfg_base(isp_dev, &param->base_cfg);
		lswb_rad_cfg_base(isp_dev, &param->base_cfg);
		lswb_mesh_cfg_base(isp_dev, &param->base_cfg);
	}

	if (param->pvalid.aisp_lns) {
		lswb_rad_cfg_strength(isp_dev, &param->lns_cfg);
		lswb_mesh_cfg_strength(isp_dev, &param->lns_cfg);
	}

	if (param->pvalid.aisp_blc) {
		lswb_blc_cfg_ofst(isp_dev, &param->blc_cfg);
		lswb_eotf_cfg_ofst(isp_dev, &param->blc_cfg);
		lswb_idg_cfg_ofst(isp_dev, &param->blc_cfg);
	}

	if (param->pvalid.aisp_mesh_crt)
		lswb_mesh_cfg_crt(isp_dev, &param->mesh_crt_cfg);
}

void isp_lens_init(struct isp_dev_t *isp_dev)
{
	lswb_eotf_init(isp_dev);
	lswb_idg_init(isp_dev);
	lswb_blc_init(isp_dev);
	lswb_wb_init(isp_dev);
	lswb_rad_init(isp_dev);
	lswb_mesh_init(isp_dev);
}
