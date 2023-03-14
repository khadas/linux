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

static const u32 f_sqrt0[] = {
	0, 32768, 65536, 98304, 131072, 163840, 196608,
	229376, 262144, 294912, 327680, 360448, 393216,
	425984, 458752, 491520, 524288, 557056, 589824, 622592,
	655360, 688128, 720896, 753664, 786432, 819200, 851968,
	884736, 917504, 950272, 983040, 1015808, 1048575
};

static const u32 f_sqrt4[] = {
	0, 8144, 16194, 24151, 32017, 39794, 47482, 55084,
	62601, 70034, 77385, 84655, 91846, 98958, 105993, 112952,
	119837, 126648, 133387, 140055, 146653, 153183, 159644, 166039,
	172368, 178633, 184833, 190971, 197047, 203063, 209018, 214914,
	220752, 232257, 243540, 254606, 265462, 276114, 286567, 296827,
	306900, 316790, 326502, 336042, 345413, 354620, 363667, 372559,
	381300, 398341, 414821, 430766, 446202, 461153, 475642, 489690,
	503316, 516539, 529378, 541847, 553964, 565743, 577197, 588341,
	599186, 609744, 620027, 630045, 639809, 649327, 658609, 667664,
	676500, 685125, 693546, 701770, 709805, 717656, 725330, 732833,
	740171, 747348, 754371, 761243, 767971, 774557, 781008, 787326,
	793516, 799583, 805528, 811357, 817072, 822676, 828174, 833568,
	838860, 844054, 849153, 854159, 859074, 863901, 868642, 873300,
	877877, 882375, 886795, 891141, 895413, 899613, 903745, 907808,
	911805, 919606, 927161, 934482, 941578, 948460, 955138, 961620,
	967916, 979977, 991380, 1002178, 1012418, 1022141, 1031386,
	1040187, 1048575
};

static void fed_cfg_dgain(struct isp_dev_t *isp_dev, void *gain)
{
	u32 val = 0;
	aisp_dgain_cfg_t *dgain = gain;

	val = (dgain->dg_gain[0] << 0) | (dgain->dg_gain[1] << 16);
	isp_reg_write(isp_dev, ISP_FED_DG_GAIN01, val);

	val = (dgain->dg_gain[2] << 0) | (dgain->dg_gain[3] << 16);
	isp_reg_write(isp_dev, ISP_FED_DG_GAIN23, val);

	val = (dgain->dg_gain[4] << 0);
	isp_reg_write(isp_dev, ISP_FED_DG_GAIN4, val);
}

static void fed_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	u32 i = 0;
	u32 val = 0;
	u32 *stp, *num;

	aisp_base_cfg_t *base_cfg = base;
	aisp_setting_fixed_cfg_t *fixed_cfg = &base_cfg->fxset_cfg;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	isp_hw_lut_wstart(isp_dev, FED_LUT_CFG);

	isp_reg_update_bits(isp_dev, ISP_FED_SQRT_EN_MODE, fixed_cfg->sqrt1_mode, 8, 1);

	isp_reg_write(isp_dev, ISP_FED_SQRT0_ADDR, 0);
	for (i = 0; i < ISP_ARRAY_SIZE(lut_cfg->sqrt0_lut); i++) {
		isp_reg_write(isp_dev, ISP_FED_SQRT0_DATA, lut_cfg->sqrt0_lut[i]);
	}

	isp_reg_write(isp_dev, ISP_FED_SQRT1_ADDR, 0);
	for (i = 0; i < ISP_ARRAY_SIZE(lut_cfg->sqrt1_lut); i++) {
		isp_reg_write(isp_dev, ISP_FED_SQRT1_DATA, lut_cfg->sqrt1_lut[i]);
	}

	stp = fixed_cfg->sqrt1_stp;
	val = (stp[0] << 0) | (stp[1] << 4) |
		(stp[2] << 8) | (stp[3] << 12) |
		(stp[4] << 16) | (stp[5] << 20) |
		(stp[6] << 24) | (stp[7] << 28);
	isp_reg_write(isp_dev, ISP_FED_SQRT_STP, val);

	num = fixed_cfg->sqrt1_num;
	val = (num[0] << 0) | (num[1] << 4) |
		(num[2] << 8) | (num[3] << 12) |
		(num[4] << 16) | (num[5] << 20) |
		(num[6] << 24) | (num[7] << 28);
	isp_reg_write(isp_dev, ISP_FED_SQRT_NUM, val);

	stp = fixed_cfg->eotf1_stp;
	val = (stp[0] << 28) | (stp[1] << 24) |
		(stp[2] << 20) | (stp[3] << 16) |
		(stp[4] << 12) | (stp[5] << 8) |
		(stp[6] << 4) | (stp[7] << 0);
	isp_reg_write(isp_dev, ISP_LSWB_EOTF_STP, val);

	num = fixed_cfg->eotf1_num;
	val = (num[0] << 27) | (num[1] << 24) |
		(num[2] << 19) | (num[3] << 16) |
		(num[4] << 11) | (num[5] << 8) |
		(num[6] << 3) | (num[7] << 0);
	isp_reg_write(isp_dev, ISP_LSWB_EOTF_NUM, val);

	isp_hw_lut_wend(isp_dev);
}

static void fed_cfg_blc(struct isp_dev_t *isp_dev, void *blc)
{
	aisp_blc_cfg_t *blc_cfg = blc;

	isp_reg_write(isp_dev, ISP_FED_BL_OFST_GR, blc_cfg->fe_bl_ofst[0]);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_R, blc_cfg->fe_bl_ofst[1]);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_B, blc_cfg->fe_bl_ofst[2]);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_GB, blc_cfg->fe_bl_ofst[3]);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_IR, blc_cfg->fe_bl_ofst[4]);

	isp_reg_write(isp_dev, ISP_FED_DG_OFST, blc_cfg->dg_ofst);
	isp_reg_write(isp_dev, ISP_FED_SQRT_PRE_OFST, blc_cfg->sqrt_pre_ofst);
	isp_reg_write(isp_dev, ISP_FED_SQRT_PST_OFST, blc_cfg->sqrt_pst_ofst);
}

void isp_fed_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
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

	isp_reg_update_bits(isp_dev, ISP_FED_DG_PHS_OFST, xofst, 4, 2);
	isp_reg_update_bits(isp_dev, ISP_FED_DG_PHS_OFST, yofst, 0, 2);

	isp_reg_update_bits(isp_dev, ISP_FED_BL_PHS, xofst, 4, 2);
	isp_reg_update_bits(isp_dev, ISP_FED_BL_PHS, yofst, 0, 2);
}

void isp_fed_cfg_ofst(struct isp_dev_t *isp_dev)
{
	u32 val = 0;
	u32 bl[4] = {240, 240, 240, 240};

	isp_reg_write(isp_dev, ISP_FED_BL_OFST_GR, bl[0] << 8);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_R, bl[1] << 8);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_B, bl[2] << 8);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_GB, bl[3] << 8);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_IR, bl[0] << 8);

	val = (4096 << 16) | (4096 << 0);
	isp_reg_write(isp_dev, ISP_FED_DG_GAIN01, val);

	val = (4096 << 16) | (4096 << 0);
	isp_reg_write(isp_dev, ISP_FED_DG_GAIN23, val);

	isp_reg_write(isp_dev, ISP_FED_DG_GAIN4, 4096);

	isp_reg_read_bits(isp_dev, ISP_LSWB_BLC_OFST0, &val, 16, 16);
	//val = (val & ((1 << 12) - 1)) << (val >> 12);
	//isp_reg_write(isp_dev, ISP_FED_DG_OFST, val);
}

void isp_fed_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_dgain)
		fed_cfg_dgain(isp_dev, &param->dgain);

	if (param->pvalid.aisp_base)
		fed_cfg_base(isp_dev, &param->base_cfg);

	if (param->pvalid.aisp_blc)
		fed_cfg_blc(isp_dev, &param->blc_cfg);
}

void isp_fed_init(struct isp_dev_t *isp_dev)
{
	u32 i, val = 0;
	int stp[] = {11, 12, 13, 13, 13, 13, 14, 15};
	int num[] = {5, 4, 4, 4, 4, 4, 3, 3};
	const u32 *sqrt0 = f_sqrt0;
	const u32 *sqrt4 = f_sqrt4;


	val = (4096 << 16) | (4096 << 0);
	isp_reg_write(isp_dev, ISP_FED_DG_GAIN01, val);

	val = (4096 << 16) | (4096 << 0);
	isp_reg_write(isp_dev, ISP_FED_DG_GAIN23, val);
	isp_reg_write(isp_dev, ISP_FED_DG_GAIN4, 4096);

	isp_reg_write(isp_dev, ISP_FED_DG_OFST, 0);

	isp_reg_write(isp_dev, ISP_FED_BL_OFST_GR, 0);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_R, 0);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_B, 0);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_GB, 0);
	isp_reg_write(isp_dev, ISP_FED_BL_OFST_IR, 0);

	isp_reg_write(isp_dev, ISP_FED_SQRT_PRE_OFST, 0);
	isp_reg_write(isp_dev, ISP_FED_SQRT_PST_OFST, 0);

	val = (1 << 8) | (1 << 4) | (1 << 0);
	isp_reg_write(isp_dev, ISP_FED_SQRT_EN_MODE, val);

	isp_reg_write(isp_dev, ISP_FED_SQRT0_ADDR, 0);
	for (i = 0; i < 33; i++)
		isp_reg_write(isp_dev, ISP_FED_SQRT0_DATA, sqrt0[i]);

	isp_reg_write(isp_dev, ISP_FED_SQRT1_ADDR, 0);
	for (i = 0; i < 129; i++)
		isp_reg_write(isp_dev, ISP_FED_SQRT1_DATA, sqrt4[i]);

	val = (stp[0] << 0) | (stp[1] << 4) |
		(stp[2] << 8) | (stp[3] << 12) |
		(stp[4] << 16) | (stp[5] << 20) |
		(stp[6] << 24) | (stp[7] << 28);
	isp_reg_write(isp_dev, ISP_FED_SQRT_STP, val);

	val = (num[0] << 0) | (num[1] << 4) |
		(num[2] << 8) | (num[3] << 12) |
		(num[4] << 16) | (num[5] << 20) |
		(num[6] << 24) | (num[7] << 28);
	isp_reg_write(isp_dev, ISP_FED_SQRT_NUM, val);
}
