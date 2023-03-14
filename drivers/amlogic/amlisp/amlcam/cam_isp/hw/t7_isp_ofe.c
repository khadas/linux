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

static void ofe_decmp_init(struct isp_dev_t *isp_dev)
{
	int i = 0;
	u32 val = 0;
	u32 stp[] = {8, 9, 11, 12, 13, 14, 14, 14};
	u32 num[] = {5, 4, 3, 3, 3, 3, 4, 5};
	u32 decmp0[] = {0, 32768, 65536, 98304, 131072, 163840, 196608, 229376,
			262144, 294912, 327680, 360448, 393216, 425984, 458752,
			491520, 524288, 557056, 589824, 622592, 655360, 688128,
			720896, 753664, 786432, 819200, 851968, 884736, 917504,
			950272, 983040, 1015808, 1048575};

	u32 decmp2[] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17,
			17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18,
			18, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 20,
			20, 21, 21, 22, 22, 23, 24, 25, 26, 27, 28, 29, 31, 32, 35, 38, 41,
			45, 49, 54, 59, 64, 76, 91, 108, 128, 152, 181, 215, 256, 304, 362,
			431, 512, 609, 724, 861, 1024, 1218, 1448, 1722, 2048, 2435, 2896,
			3444, 4096, 4871, 5793, 6889, 8192, 9742, 11585, 13777, 16384,
			19484, 23170, 27554, 32768, 38968, 46341, 55109, 65536, 77936,
			92682, 110218, 131072, 155872, 185364, 220436, 262144, 311744,
			370728, 440872, 524288, 623487, 741455, 881744, 1048575};

	isp_reg_write(isp_dev, ISP_DECOMP_CTRL, 0x1);

	isp_reg_write(isp_dev, ISP_OFE_DECMP0_LUT_ADDR, 0);
	for (i = 0; i < 33; i++) {
		isp_reg_write(isp_dev, ISP_OFE_DECMP0_LUT_DATA, decmp0[i]);
	}

	isp_reg_write(isp_dev, ISP_OFE_DECMP1_LUT_ADDR, 0);
	for (i = 0; i < 33; i++) {
		isp_reg_write(isp_dev, ISP_OFE_DECMP1_LUT_DATA, decmp2[i]);
	}

	val = (stp[7] << 28) | (stp[6] << 24) | (stp[5] << 20) | (stp[4] << 16) |
		(stp[3] << 12) | (stp[2] << 8) | (stp[1] << 4) | (stp[0] << 0);
	isp_reg_write(isp_dev, ISP_DECOMP1_STP, val);

	val = (num[7] << 28) | (num[6] << 24) | (num[5] << 20) | (num[4] << 16) |
		(num[3] << 12) | (num[2] << 8) | (num[1] << 4) | (num[0] << 0);
	isp_reg_write(isp_dev, ISP_DECOMP1_NUM, val);
}

static void ofe_inpfmt_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	val = (1408 << 16) | (512 << 0);
	isp_reg_write(isp_dev, ISP_INPFMT_KPTS_01, val);

	val = (2176 << 16) | (9 << 12) | (5 << 8) | (4 << 4) | (2 << 0);
	isp_reg_write(isp_dev, ISP_INPFMT_KPTS2_SLP, val);

	isp_reg_update_bits(isp_dev, ISP_INPFMT_SPLT, 1, 16, 2);
	isp_reg_update_bits(isp_dev, ISP_INPFMT_SPLT, 0, 12, 1);
	isp_reg_update_bits(isp_dev, ISP_INPFMT_SPLT, 4, 8, 4);
	isp_reg_update_bits(isp_dev, ISP_INPFMT_SPLT, 0, 4, 2);
	isp_reg_update_bits(isp_dev, ISP_INPFMT_SPLT, 0, 0, 2);

	val = (0 << 8) | (2 << 4) | (2 << 0);
	isp_reg_write(isp_dev, ISP_INPFMT_MOD_BD_0, val);

	val = (0 << 8) | (2 << 4) | (2 << 0);
	isp_reg_write(isp_dev, ISP_INPFMT_MOD_BD_1, val);
}

static void ofe_bac_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	isp_reg_update_bits(isp_dev, ISP_BIN_BAC_CTRL, 0, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_BIN_BAC_CTRL, 0, 4, 1);
	isp_reg_update_bits(isp_dev, ISP_BIN_BAC_CTRL, 0, 16, 2);

	val = (16 << 24) | (48 << 16) | (0 << 8) | (64 << 0);
	isp_reg_write(isp_dev, ISP_BAC_HCOEF, val);

	val = (16 << 24) | (48 << 16) | (0 << 8) | (64 << 0);
	isp_reg_write(isp_dev, ISP_BAC_VCOEF, val);
}

static void ofe_fpnr_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;

	isp_reg_update_bits(isp_dev, ISP_FPNR_CTRL, 0, 14, 2);
	isp_reg_update_bits(isp_dev, ISP_FPNR_CTRL, 0, 18, 1);
	isp_reg_update_bits(isp_dev, ISP_FPNR_CTRL, 0, 28, 1);
	isp_reg_update_bits(isp_dev, ISP_FPNR_CTRL, 0, 30, 1);

/* FPNR0 */
	isp_reg_update_bits(isp_dev, ISP_FPNR0_CTRL_CORR, 0, 0, 1);

	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_FPNR0_CORR_GAIN_0, val);
	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_FPNR0_CORR_GAIN_1, val);
	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_FPNR0_CORR_GAIN_2, val);
	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_FPNR0_CORR_GAIN_3, val);
	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_FPNR0_CORR_GAIN_4, val);

/* FPNR1 */
	isp_reg_update_bits(isp_dev, ISP_FPNR1_CTRL_CORR, 0, 0, 1);

	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_FPNR1_CORR_GAIN_0, val);
	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_FPNR1_CORR_GAIN_1, val);
	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_FPNR1_CORR_GAIN_2, val);
	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_FPNR1_CORR_GAIN_3, val);
	val = (256 << 16) | (256 << 0);
	isp_reg_write(isp_dev, ISP_FPNR1_CORR_GAIN_4, val);
}

static void ofe_fpnr_cfg_param(struct isp_dev_t *isp_dev, void *param)
{
	u32 val = 0;
	aisp_fpnr_cfg_t *fpnr_cfg = param;

/* FPNR0 */
	val = (fpnr_cfg->fpnr_corr_gain0[0][0] << 16) |
		(fpnr_cfg->fpnr_corr_gain1[0][0] << 0);
	isp_reg_write(isp_dev, ISP_FPNR0_CORR_GAIN_0, val);

	val = (fpnr_cfg->fpnr_corr_gain0[0][1] << 16) |
		(fpnr_cfg->fpnr_corr_gain1[0][1] << 0);
	isp_reg_write(isp_dev, ISP_FPNR0_CORR_GAIN_1, val);

	val = (fpnr_cfg->fpnr_corr_gain0[0][2] << 16) |
		(fpnr_cfg->fpnr_corr_gain1[0][2] << 0);
	isp_reg_write(isp_dev, ISP_FPNR0_CORR_GAIN_2, val);

	val = (fpnr_cfg->fpnr_corr_gain0[0][3] << 16) |
		(fpnr_cfg->fpnr_corr_gain1[0][3] << 0);
	isp_reg_write(isp_dev, ISP_FPNR0_CORR_GAIN_3, val);

	val = (fpnr_cfg->fpnr_corr_gain0[0][4] << 16) |
		(fpnr_cfg->fpnr_corr_gain1[0][4] << 0);
	isp_reg_write(isp_dev, ISP_FPNR0_CORR_GAIN_4, val);

/* FPNR1 */
	val = (fpnr_cfg->fpnr_corr_gain0[1][0] << 16) |
		(fpnr_cfg->fpnr_corr_gain1[1][0] << 0);
	isp_reg_write(isp_dev, ISP_FPNR1_CORR_GAIN_0, val);

	val = (fpnr_cfg->fpnr_corr_gain0[1][1] << 16) |
		(fpnr_cfg->fpnr_corr_gain1[1][1] << 0);
	isp_reg_write(isp_dev, ISP_FPNR1_CORR_GAIN_1, val);

	val = (fpnr_cfg->fpnr_corr_gain0[1][2] << 16) |
		(fpnr_cfg->fpnr_corr_gain1[1][2] << 0);
	isp_reg_write(isp_dev, ISP_FPNR1_CORR_GAIN_2, val);

	val = (fpnr_cfg->fpnr_corr_gain0[1][3] << 16) |
		(fpnr_cfg->fpnr_corr_gain1[1][3] << 0);
	isp_reg_write(isp_dev, ISP_FPNR1_CORR_GAIN_3, val);

	val = (fpnr_cfg->fpnr_corr_gain0[1][4] << 16) |
		(fpnr_cfg->fpnr_corr_gain1[1][4] << 0);
	isp_reg_write(isp_dev, ISP_FPNR1_CORR_GAIN_4, val);
}

static void ofe_ge_init(struct isp_dev_t *isp_dev)
{
	int val = 0;

/* GE0 */
	isp_reg_update_bits(isp_dev, ISP_GE0_CTRL, 1, 8, 1);
	isp_reg_update_bits(isp_dev, ISP_GE0_STAT, 16, 0, 16);

	val = (17 << 8) | (0 << 0);
	isp_reg_write(isp_dev, ISP_GE0_COMM, val);

	val = (64 << 24) | (64 << 16) | (64 << 8) | (64 << 0);
	isp_reg_write(isp_dev, ISP_GE0_HV_WT2, val);

/* GE1 */
	isp_reg_update_bits(isp_dev, ISP_GE1_CTRL, 1, 8, 1);
	isp_reg_update_bits(isp_dev, ISP_GE1_STAT, 16, 0, 16);

	val = (17 << 8) | (0 << 0);
	isp_reg_write(isp_dev, ISP_GE1_COMM, val);

	val = (64 << 24) | (64 << 16) | (64 << 8) | (64 << 0);
	isp_reg_write(isp_dev, ISP_GE1_HV_WT2, val);
}

static void ofe_ge_cfg_param(struct isp_dev_t *isp_dev, void *param)
{
	u32 val = 0;
	aisp_ge_cfg_t *ge_cfg = param;

/* GE0 */
	isp_reg_write(isp_dev, ISP_GE0_STAT, ge_cfg->ge_stat_edge_thd[0]);
	isp_reg_update_bits(isp_dev, ISP_GE0_COMM, ge_cfg->ge_hv_thrd[0], 8, 10);

	val = (ge_cfg->ge_hv_wtlut_0[0] << 24) |
		(ge_cfg->ge_hv_wtlut_0[1] << 16) |
		(ge_cfg->ge_hv_wtlut_0[2] << 8) |
		(ge_cfg->ge_hv_wtlut_0[3] << 0);
	isp_reg_write(isp_dev, ISP_GE0_HV_WT2, val);

/* GE1 */
	isp_reg_write(isp_dev, ISP_GE1_STAT, ge_cfg->ge_stat_edge_thd[1]);
	isp_reg_update_bits(isp_dev, ISP_GE1_COMM, ge_cfg->ge_hv_thrd[1], 8, 10);

	val = (ge_cfg->ge_hv_wtlut_1[0] << 24) |
		(ge_cfg->ge_hv_wtlut_1[1] << 16) |
		(ge_cfg->ge_hv_wtlut_1[2] << 8) |
		(ge_cfg->ge_hv_wtlut_1[3] << 0);
	isp_reg_write(isp_dev, ISP_GE1_HV_WT2, val);
}

static void ofe_og_init(struct isp_dev_t *isp_dev)
{
/* OG0 */
	isp_reg_update_bits(isp_dev, ISP_OG0_OFST0, 0, 16, 14);
	isp_reg_write(isp_dev, ISP_OG0_OFST12, 0x0);
	isp_reg_write(isp_dev, ISP_OG0_OFST34, 0x0);
	isp_reg_update_bits(isp_dev, ISP_OG0_GAIN_PST, 0, 0, 14);

/* OG1 */
	isp_reg_update_bits(isp_dev, ISP_OG1_OFST0, 0, 16, 14);
	isp_reg_write(isp_dev, ISP_OG1_OFST12, 0x0);
	isp_reg_write(isp_dev, ISP_OG1_OFST34, 0x0);
	isp_reg_update_bits(isp_dev, ISP_OG1_GAIN_PST, 0, 0, 14);
}

static void ofe_decmp_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	u32 val = 0;
	u32 i;
	aisp_base_cfg_t *base_cfg = base;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;

	isp_hw_lut_wstart(isp_dev, OFE_DECMPR_LUT_CFG);

	isp_reg_write(isp_dev, ISP_DECOMP_CTRL, 0x1);

	isp_reg_write(isp_dev, ISP_OFE_DECMP0_LUT_ADDR, 0);
	for (i = 0; i < 33; i++) {
		isp_reg_write(isp_dev, ISP_OFE_DECMP0_LUT_DATA, lut_cfg->decmp0_lut[i]);
	}

	isp_reg_write(isp_dev, ISP_OFE_DECMP1_LUT_ADDR, 0);
	for (i = 0; i < 129; i++) {
		isp_reg_write(isp_dev, ISP_OFE_DECMP1_LUT_DATA, lut_cfg->decmp1_lut[i]);
	}

	isp_hw_lut_wend(isp_dev);
}

static void ofe_bac_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	u32 val = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_setting_fixed_cfg_t *fixed_cfg = &base_cfg->fxset_cfg;

	val = (fixed_cfg->bac_hcoef[0] << 0) | (fixed_cfg->bac_hcoef[1] << 8) |
		(fixed_cfg->bac_hcoef[2] << 16) | (fixed_cfg->bac_hcoef[3] << 24);
	isp_reg_write(isp_dev, ISP_BAC_HCOEF, val);

	val = (fixed_cfg->bac_vcoef[0] << 0) | (fixed_cfg->bac_vcoef[1] << 8) |
		(fixed_cfg->bac_vcoef[2] << 16) | (fixed_cfg->bac_vcoef[3] << 24);
	isp_reg_write(isp_dev, ISP_BAC_VCOEF, val);
}

static void ofe_fpnr_cfg_base(struct isp_dev_t *isp_dev, void *base)
{
	int i = 0;
	u32 val = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_lut_fixed_cfg_t *lut_cfg = &base_cfg->fxlut_cfg;
	u32 *lut00, *lut01, *lut02, *lut03, *lut04;
	u32 *lut10, *lut11, *lut12, *lut13, *lut14;

	isp_hw_lut_wstart(isp_dev, OFE_FPNR_LUT_CFG);

	lut00 = &lut_cfg->fpnr_corr_val[0][0];
	lut01 = &lut_cfg->fpnr_corr_val[0][2048];
	lut02 = &lut_cfg->fpnr_corr_val[0][4096];
	lut03 = &lut_cfg->fpnr_corr_val[0][6144];
	lut04 = &lut_cfg->fpnr_corr_val[0][8192];

	lut10 = &lut_cfg->fpnr_corr_val[1][0];
	lut11 = &lut_cfg->fpnr_corr_val[1][2048];
	lut12 = &lut_cfg->fpnr_corr_val[1][4096];
	lut13 = &lut_cfg->fpnr_corr_val[1][6144];
	lut14 = &lut_cfg->fpnr_corr_val[1][8192];
	isp_reg_update_bits(isp_dev, ISP_FPNR_CTRL, 1, 18, 1);//trigger

	isp_reg_write(isp_dev, ISP_FPNR0_LUT_ADDR, 0);
	isp_reg_write(isp_dev, ISP_FPNR1_LUT_ADDR, 0);
	for (i = 0; i < 2048; i++) {
		val = (lut00[i] << 0) | (lut01[i] << 16);
		isp_reg_write(isp_dev, ISP_FPNR0_LUT_DATA, val);

		val = (lut10[i] << 0) | (lut11[i] << 16);
		isp_reg_write(isp_dev, ISP_FPNR1_LUT_DATA, val);
	}

	isp_reg_write(isp_dev, ISP_FPNR0_LUT_ADDR, 0);
	isp_reg_write(isp_dev, ISP_FPNR1_LUT_ADDR, 0);
	for (i = 0; i < 2048; i++) {
		val = (lut02[i] << 0) | (lut03[i] << 16);
		isp_reg_write(isp_dev, ISP_FPNR0_LUT_DATA, val);

		val = (lut12[i] << 0) | (lut13[i] << 16);
		isp_reg_write(isp_dev, ISP_FPNR1_LUT_DATA, val);
	}

	isp_reg_write(isp_dev, ISP_FPNR0_LUT_ADDR, 0);
	isp_reg_write(isp_dev, ISP_FPNR1_LUT_ADDR, 0);
	for (i = 0; i < 2048; i++) {
		val = lut04[i];
		isp_reg_write(isp_dev, ISP_FPNR0_LUT_DATA, val);

		val = lut14[i];
		isp_reg_write(isp_dev, ISP_FPNR1_LUT_DATA, val);
	}

	isp_hw_lut_wend(isp_dev);
}

void isp_ofe_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
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

	isp_reg_update_bits(isp_dev, ISP_OG0_OFST0, xofst, 4, 2);
	isp_reg_update_bits(isp_dev, ISP_OG0_OFST0, yofst, 0, 2);

	isp_reg_update_bits(isp_dev, ISP_GE0_CTRL, xofst, 16, 2);
	isp_reg_update_bits(isp_dev, ISP_GE0_CTRL, yofst, 12, 2);

	isp_reg_update_bits(isp_dev, ISP_BIN_BAC_CTRL, xofst, 12, 2);
	isp_reg_update_bits(isp_dev, ISP_BIN_BAC_CTRL, yofst, 8, 2);

	isp_reg_update_bits(isp_dev, ISP_FPNR_CTRL, xofst, 24, 2);
	isp_reg_update_bits(isp_dev, ISP_FPNR_CTRL, yofst, 20, 2);


	isp_reg_update_bits(isp_dev, ISP_OG1_OFST0, xofst, 4, 2);
	isp_reg_update_bits(isp_dev, ISP_OG1_OFST0, yofst, 0, 2);

	isp_reg_update_bits(isp_dev, ISP_GE1_CTRL, xofst, 16, 2);
	isp_reg_update_bits(isp_dev, ISP_GE1_CTRL, yofst, 12, 2);
}

void isp_ofe_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize = fmt->width;
	u32 ysize = fmt->height;

	val = (fmt->xstart << 16) | (fmt->ystart << 0);
	isp_reg_write(isp_dev, ISP_CROP_START, val);

	val = (xsize << 16) | (ysize & 0xffff);
	isp_reg_write(isp_dev, ISP_CROP_SIZE, val);

	isp_reg_update_bits(isp_dev, ISP_BIN_BAC_CTRL, 0, 16, 2);
	isp_reg_update_bits(isp_dev, ISP_INPFMT_SPLT, 0, 4, 2);
}

void isp_ofe_cfg_slice_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize = fmt->width;
	u32 ysize = fmt->height;

	val = (fmt->xstart << 16) | (fmt->ystart << 0);
	isp_hwreg_write(isp_dev, ISP_CROP_START, val);

	val = (xsize << 16) | (ysize & 0xffff);
	isp_hwreg_write(isp_dev, ISP_CROP_SIZE, val);

	isp_hwreg_update_bits(isp_dev, ISP_BIN_BAC_CTRL, 0, 16, 2);
	isp_hwreg_update_bits(isp_dev, ISP_INPFMT_SPLT, 0, 4, 2);
}

void isp_ofe_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_base) {
		ofe_bac_cfg_base(isp_dev, &param->base_cfg);
		ofe_fpnr_cfg_base(isp_dev, &param->base_cfg);
		ofe_decmp_cfg_base(isp_dev, &param->base_cfg);
	}

	if (param->pvalid.aisp_ge)
		ofe_ge_cfg_param(isp_dev, &param->ge_cfg);

	if (param->pvalid.aisp_fpnr)
		ofe_fpnr_cfg_param(isp_dev, &param->fpnr_cfg);
}

void isp_ofe_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_stats_info_t *stats = buff->vaddr[AML_PLANE_A];
	ofe_req_info_t *o_info = &stats->module_info.ofe_req_info;

	isp_reg_read_bits(isp_dev, ISP_BIN_BAC_CTRL, &o_info->bac_mode, 4, 1);
}

void isp_ofe_init(struct isp_dev_t *isp_dev)
{
	ofe_decmp_init(isp_dev);
	ofe_inpfmt_init(isp_dev);
	ofe_bac_init(isp_dev);
	ofe_fpnr_init(isp_dev);
	ofe_ge_init(isp_dev);
	ofe_og_init(isp_dev);
}
