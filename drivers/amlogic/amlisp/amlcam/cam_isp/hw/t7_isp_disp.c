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

#define DISP_CNT    (4)
#define DISP_INTER  (0x100)

#define SELECT_H_0123(data)    (((data)&(~((1U<<13)-1)))|(2<<10)|(1<<9))
#define SELECT_H_4567(data)    (((data)&(~((1U<<13)-1)))|(3<<10)|(1<<9))
#define SELECT_V_0123(data)    (((data)&(~((1U<<13)-1)))|(0<<10)|(1<<9))
#define SELECT_V_4567(data)    (((data)&(~((1U<<13)-1)))|(1<<10)|(1<<9))

static const int pps_lut_tap4_s11[33][4] =  {
	{0, 511, 0  , 0  },
	{-5 , 511, 5  , 0  },
	{-10, 511, 11 , 0  },
	{-14, 510, 17 , -1 },
	{-18, 508, 23 , -1 },
	{-22, 506, 29 , -1 },
	{-25, 503, 36 , -2 },
	{-28, 500, 43 , -3 },
	{-32, 496, 51 , -3 },
	{-34, 491, 59 , -4 },
	{-37, 487, 67 , -5 },
	{-39, 482, 75 , -6 },
	{-41, 476, 84 , -7 },
	{-42, 470, 92 , -8 },
	{-44, 463, 102, -9 },
	{-45, 456, 111, -10},
	{-45, 449, 120, -12},
	{-47, 442, 130, -13},
	{-47, 434, 140, -15},
	{-47, 425, 151, -17},
	{-47, 416, 161, -18},
	{-47, 407, 172, -20},
	{-47, 398, 182, -21},
	{-47, 389, 193, -23},
	{-46, 379, 204, -25},
	{-45, 369, 215, -27},
	{-44, 358, 226, -28},
	{-43, 348, 237, -30},
	{-43, 337, 249, -31},
	{-41, 326, 260, -33},
	{-40, 316, 271, -35},
	{-39, 305, 282, -36},
	{-37, 293, 293, -37}
};

static const int pps_lut_tap2_s11[33][2] =  {
	{128, 0},
	{127, 1},
	{126, 2},
	{124, 4},
	{123, 5},
	{122, 6},
	{120, 8},
	{118, 10},
	{116, 12},
	{114, 14},
	{112, 16},
	{110, 18},
	{108, 20},
	{106, 22},
	{104, 24},
	{101, 27},
	{99,  29},
	{97,  31},
	{94,  34},
	{91,  37},
	{89,  39},
	{86,  42},
	{84,  44},
	{81,  47},
	{78,  50},
	{75,  53},
	{73,  55},
	{70,  58},
	{68,  60},
	{67,  61},
	{66,  62},
	{65,  63},
	{64,  64}
};

static const int pps_lut_tap8_s11[33][8] = {
	{0,    0,   0, 128,  0,   0,  0,   0},
	{-1,   1,   0, 127,  2,  -1,  1,  -1},
	{-1,   2,  -2, 127,  4,  -2,  1,  -1},
	{-2,   3,  -4, 127,  6,  -3,  2,  -1},
	{-3,   4,  -7, 127, 10,  -4,  3,  -2},
	{-3,   5,  -9, 127, 12,  -5,  3,  -2},
	{-4,   6, -11, 127, 15,  -6,  4,  -3},
	{-4,   7, -13, 127, 16,  -7,  5,  -3},
	{-5,   7, -14, 127, 20,  -8,  5,  -4},
	{-5,   8, -16, 127, 21,  -9,  6,  -4},
	{-6,   9, -17, 127, 24, -11,  7,  -5},
	{-6,  10, -18, 126, 26, -12,  7,  -5},
	{-7,  10, -20, 127, 29, -13,  8,  -6},
	{-7,  11, -21, 124, 32, -14,  9,  -6},
	{-8,  12, -22, 124, 35, -15,  9,  -7},
	{-8,  12, -23, 123, 37, -16, 10,  -7},
	{-9,  13, -24, 121, 40, -17, 11,  -7},
	{-9,  14, -25, 120, 43, -18, 11,  -8},
	{-10, 14, -26, 119, 46, -19, 12,  -8},
	{-10, 15, -27, 118, 49, -20, 12,  -9},
	{-10, 15, -27, 115, 52, -21, 13,  -9},
	{-10, 15, -28, 114, 55, -22, 13,  -9},
	{-11, 16, -28, 112, 58, -23, 14, -10},
	{-11, 16, -29, 111, 61, -24, 14, -10},
	{-11, 16, -29, 107, 64, -24, 15, -10},
	{-11, 17, -29, 104, 67, -25, 15, -10},
	{-11, 17, -29, 103, 70, -26, 15, -11},
	{-12, 17, -29, 100, 73, -26, 16, -11},
	{-12, 17, -29,  98, 76, -27, 16, -11},
	{-12, 17, -29,  96, 79, -28, 16, -11},
	{-12, 17, -29,  92, 82, -28, 17, -11},
	{-12, 17, -29,  90, 85, -29, 17, -11},
	{-12, 17, -29,  88, 88, -29, 17, -12}
	};

static const u32 calibration_gamma[] = {
	0, 347, 539, 679, 794, 894, 982, 1062,
	1136, 1204, 1268, 1329, 1386, 1441, 1493,
	1543, 1591, 1638, 1683, 1726, 1768, 1809,
	1849, 1888, 1926, 1963, 1999, 2034, 2068,
	2102, 2135, 2168, 2200, 2231, 2262, 2292,
	2322, 2351, 2380, 2408, 2436, 2463, 2491,
	2517, 2544, 2570, 2596, 2621, 2646, 2671,
	2695, 2719, 2743, 2767, 2790, 2814, 2836,
	2859, 2882, 2904, 2926, 2948, 2969, 2990,
	3012, 3033, 3053, 3074, 3094, 3115, 3135,
	3155, 3174, 3194, 3213, 3233, 3252, 3271,
	3290, 3308, 3327, 3345, 3364, 3382, 3400,
	3418, 3436, 3453, 3471, 3488, 3506, 3523,
	3540, 3557, 3574, 3591, 3607, 3624, 3640,
	3657, 3673, 3689, 3705, 3721, 3737, 3753,
	3769, 3785, 3800, 3816, 3831, 3846, 3862,
	3877, 3892, 3907, 3922, 3937, 3951, 3966,
	3981, 3995, 4010, 4024, 4039, 4053, 4067,
	4081, 4095, 0
};

static int disp_ceil(int size, int rate)
{
	int rst = 0;

	if (size == 0) {
		rst = 0;
	} else if ((size > 0) && (rate > 0)) {
		rst = size / rate;
		if (rst * rate != size) {
			rst += 1;
		}
	} else if ((size < 0) && (rate < 0)) {
		rst = (-size) / (-rate);
		if (rst * rate != size) {
			rst += 1;
		}
	} else {
		rst = size / rate;
	}

	return rst;
}

static void disp_cfg_base_fxset(struct isp_dev_t *isp_dev, u32 idx, void *base)
{
	u32 addr = 0;
	aisp_base_cfg_t *base_cfg = base;
	aisp_setting_fixed_cfg_t *fixed_cfg = &base_cfg->fxset_cfg;

	addr = DISP0_PPS_SCALE_EN + ((idx * 0x100) << 2);
	isp_reg_update_bits(isp_dev, addr, fixed_cfg->vsc_tap_num_0, 0, 4);
	isp_reg_update_bits(isp_dev, addr, fixed_cfg->hsc_tap_num_0, 4, 4);
	isp_reg_update_bits(isp_dev, addr, fixed_cfg->vsc_nor_rs_bits_0, 24, 4);
	isp_reg_update_bits(isp_dev, addr, fixed_cfg->hsc_nor_rs_bits_0, 28, 4);
}

static void disp_cfg_base_h_luma(struct isp_dev_t *isp_dev, u32 idx, void *lut)
{
	int i = 0;
	u32 val = 0;
	u32 addr = 0;
	u32 coef[4];
	u32 h_tap;
	aisp_lut_fixed_cfg_t *lut_cfg = lut;

	isp_hw_lut_wstart(isp_dev, DISP_HLUMA_LUT_CFG);

	addr = DISP0_PPS_SCALE_EN + ((idx * 0x100) << 2);
	isp_reg_read_bits(isp_dev, addr, &h_tap, 4, 4);

	addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	val = SELECT_H_0123(val);
	isp_reg_write(isp_dev, addr, val);

	for (i = 0; i < 33; i++) {
		if (0 < h_tap)
			coef[0] = lut_cfg->pps_h_luma_coef[i * h_tap + 0];
		else
			coef[0] = 0;

		if (1 < h_tap)
			coef[1] = lut_cfg->pps_h_luma_coef[i * h_tap + 1];
		else
			coef[1] = 0;

		if (2 < h_tap)
			coef[2] = lut_cfg->pps_h_luma_coef[i * h_tap + 2];
		else
			coef[2] = 0;

		if (3 < h_tap)
			coef[3] = lut_cfg->pps_h_luma_coef[i * h_tap + 3];
		else
			coef[3] = 0;

		addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
		val = ((coef[0]&0x7ff)<< 16) | ((coef[1]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);

		val = ((coef[2]&0x7ff)<< 16) | ((coef[3]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);
	}

	addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	val = SELECT_H_4567(val);
	isp_reg_write(isp_dev, addr, val);
	for (i = 0; i < 33; i++) {
		if (4 < h_tap)
			coef[0] = lut_cfg->pps_h_luma_coef[i * h_tap + 4];
		else
			coef[0] = 0;

		if (5 < h_tap)
			coef[1] = lut_cfg->pps_h_luma_coef[i * h_tap + 5];
		else
			coef[1] = 0;

		if (6 < h_tap)
			coef[2] = lut_cfg->pps_h_luma_coef[i * h_tap + 6];
		else
			coef[2] = 0;

		if (7 < h_tap)
			coef[3] = lut_cfg->pps_h_luma_coef[i * h_tap + 7];
		else
			coef[3] = 0;

		addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
		val = ((coef[0]&0x7ff) << 16) | ((coef[1]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);

		val = ((coef[2]&0x7ff) << 16) | ((coef[3]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);
	}

	isp_hw_lut_wend(isp_dev);
}

static void disp_cfg_base_v_luma(struct isp_dev_t *isp_dev, u32 idx, void *lut)
{
	int i = 0;
	u32 val = 0;
	u32 addr = 0;
	u32 coef[4];
	u32 v_tap;
	aisp_lut_fixed_cfg_t *lut_cfg = lut;

	isp_hw_lut_wstart(isp_dev, DISP_VLUMA_LUT_CFG);

	addr = DISP0_PPS_SCALE_EN + ((idx * 0x100) << 2);
	isp_reg_read_bits(isp_dev, addr, &v_tap, 0, 4);

	addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	val = SELECT_V_0123(val);
	isp_reg_write(isp_dev, addr, val);
	for (i = 0; i < 33; i++) {
		if (0 < v_tap)
			coef[0] = lut_cfg->pps_v_luma_coef[i * v_tap + 0];
		else
			coef[0] = 0;

		if (1 < v_tap)
			coef[1] = lut_cfg->pps_v_luma_coef[i * v_tap + 1];
		else
			coef[1] = 0;

		if (2 < v_tap)
			coef[2] = lut_cfg->pps_v_luma_coef[i * v_tap + 2];
		else
			coef[2] = 0;

		if (3 < v_tap)
			coef[3] = lut_cfg->pps_v_luma_coef[i * v_tap + 3];
		else
			coef[3] = 0;

		addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
		val = ((coef[0]&0x7ff) << 16) | ((coef[1]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);

		val = ((coef[2]&0x7ff) << 16) | ((coef[3]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);
	}

	addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	val = SELECT_V_4567(val);
	isp_reg_write(isp_dev, addr, val);
	for (i = 0; i < 33; i++) {
		if (4 < v_tap)
			coef[0] = lut_cfg->pps_v_luma_coef[i * v_tap + 4];
		else
			coef[0] = 0;

		if (5 < v_tap)
			coef[1] = lut_cfg->pps_v_luma_coef[i * v_tap + 5];
		else
			coef[1] = 0;

		if (6 < v_tap)
			coef[2] = lut_cfg->pps_v_luma_coef[i * v_tap + 6];
		else
			coef[2] = 0;

		if (7 < v_tap)
			coef[3] = lut_cfg->pps_v_luma_coef[i * v_tap + 7];
		else
			coef[3] = 0;

		addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
		val = ((coef[0]&0x7ff) << 16) | ((coef[1]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);

		val = ((coef[2]&0x7ff) << 16) | ((coef[3]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);
	}

	isp_hw_lut_wend(isp_dev);
}

static void disp_cfg_base_h_chroma(struct isp_dev_t *isp_dev, u32 idx, void *lut)
{
	int i = 0;
	u32 val = 0;
	u32 addr = 0;
	u32 coef[4];
	u32 h_tap;
	aisp_lut_fixed_cfg_t *lut_cfg = lut;

	isp_hw_lut_wstart(isp_dev, DISP_HCHROMA_LUT_CFG);

	addr = DISP0_PPS_SCALE_EN + ((idx * 0x100) << 2);
	isp_reg_read_bits(isp_dev, addr, &h_tap, 4, 4);

	addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	val = SELECT_H_0123(val);
	isp_reg_write(isp_dev, addr, val);
	for (i = 0; i < 33; i++) {
		if (0 < h_tap)
			coef[0] = lut_cfg->pps_h_chroma_coef[i * h_tap + 0];
		else
			coef[0] = 0;

		if (1 < h_tap)
			coef[1] = lut_cfg->pps_h_chroma_coef[i * h_tap + 1];
		else
			coef[1] = 0;

		if (2 < h_tap)
			coef[2] = lut_cfg->pps_h_chroma_coef[i * h_tap + 2];
		else
			coef[2] = 0;

		if (3 < h_tap)
			coef[3] = lut_cfg->pps_h_chroma_coef[i * h_tap + 3];
		else
			coef[3] = 0;

		addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
		val = ((coef[0]&0x7ff) << 16) | ((coef[1]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);

		val = ((coef[2]&0x7ff) << 16) | ((coef[3]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);
	}

	addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	val = SELECT_H_4567(val);
	isp_reg_write(isp_dev, addr, val);
	for (i = 0; i < 33; i++) {
		if (4 < h_tap)
			coef[0] = lut_cfg->pps_h_chroma_coef[i * h_tap + 4];
		else
			coef[0] = 0;

		if (5 < h_tap)
			coef[1] = lut_cfg->pps_h_chroma_coef[i * h_tap + 5];
		else
			coef[1] = 0;

		if (6 < h_tap)
			coef[2] = lut_cfg->pps_h_chroma_coef[i * h_tap + 6];
		else
			coef[2] = 0;

		if (7 < h_tap)
			coef[3] = lut_cfg->pps_h_chroma_coef[i * h_tap + 7];
		else
			coef[3] = 0;

		addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
		val = ((coef[0]&0x7ff) << 16) | ((coef[1]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);

		val = ((coef[2]&0x7ff) << 16) | ((coef[3]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);
	}

	isp_hw_lut_wend(isp_dev);
}

static void disp_cfg_base_v_chroma(struct isp_dev_t *isp_dev, u32 idx, void *lut)
{
	int i = 0;
	u32 val = 0;
	u32 addr = 0;
	u32 coef[4];
	u32 v_tap;
	aisp_lut_fixed_cfg_t *lut_cfg = lut;

	isp_hw_lut_wstart(isp_dev, DISP_VCHROMA_LUT_CFG);

	addr = DISP0_PPS_SCALE_EN + ((idx * 0x100) << 2);
	isp_reg_read_bits(isp_dev, addr, &v_tap, 0, 4);

	addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	val = SELECT_V_0123(val);
	isp_reg_write(isp_dev, addr, val);
	for (i = 0; i < 33; i++) {
		if (0 < v_tap)
			coef[0] = lut_cfg->pps_v_chroma_coef[i * v_tap + 0];
		else
			coef[0] = 0;

		if (1 < v_tap)
			coef[1] = lut_cfg->pps_v_chroma_coef[i * v_tap + 1];
		else
			coef[1] = 0;

		if (2 < v_tap)
			coef[2] = lut_cfg->pps_v_chroma_coef[i * v_tap + 2];
		else
			coef[2] = 0;

		if (3 < v_tap)
			coef[3] = lut_cfg->pps_v_chroma_coef[i * v_tap + 3];
		else
			coef[3] = 0;

		addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
		val = ((coef[0]&0x7ff) << 16) | ((coef[1]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);

		val = ((coef[2]&0x7ff) << 16) | ((coef[3]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);
	}

	addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	val = SELECT_V_4567(val);
	isp_reg_write(isp_dev, addr, val);
	for (i = 0; i < 33; i++) {
		if (4 < v_tap)
			coef[0] = lut_cfg->pps_v_chroma_coef[i * v_tap + 4];
		else
			coef[0] = 0;

		if (5 < v_tap)
			coef[1] = lut_cfg->pps_v_chroma_coef[i * v_tap + 5];
		else
			coef[1] = 0;

		if (6 < v_tap)
			coef[2] = lut_cfg->pps_v_chroma_coef[i * v_tap + 6];
		else
			coef[2] = 0;

		if (7 < v_tap)
			coef[3] = lut_cfg->pps_v_chroma_coef[i * v_tap + 7];
		else
			coef[3] = 0;

		addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
		val = ((coef[0]&0x7ff) << 16) | ((coef[1]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);

		val = ((coef[2]&0x7ff) << 16) | ((coef[3]&0x7ff) << 0);
		isp_reg_write(isp_dev, addr, val);
	}

	isp_hw_lut_wend(isp_dev);
}

static void disp_cfg_base_fxlut(struct isp_dev_t *isp_dev, u32 idx, void *base)
{
	aisp_base_cfg_t *base_cfg = base;
	aisp_lut_fixed_cfg_t *lut = &base_cfg->fxlut_cfg;

	disp_cfg_base_h_luma(isp_dev, idx, lut);
	disp_cfg_base_v_luma(isp_dev, idx, lut);
	disp_cfg_base_h_chroma(isp_dev, idx, lut);
	disp_cfg_base_v_chroma(isp_dev, idx, lut);
}

void isp_disp_set_crop_size(struct isp_dev_t *isp_dev, u32 idx, struct aml_crop *crop)
{
	u32 val = 0;
	u32 addr = 0;

	if (idx >= DISP_CNT) {
		pr_err("Error input disp index\n");
		return;
	}

	addr = DISP0_TOP_CRP2_START + ((idx * 0x100) << 2);

	val = ((crop->hstart & 0xffff) << 16) |(crop->vstart &0xffff);
	isp_reg_write(isp_dev, addr, val);

	addr = DISP0_TOP_CRP2_SIZE + ((idx * 0x100) << 2);
	val = ((crop->hsize & 0xffff) << 16) |(crop->vsize &0xffff);
	isp_reg_write(isp_dev, addr, val);
}

void isp_disp_get_crop_size(struct isp_dev_t *isp_dev, u32 idx, struct aml_crop *crop)
{
	u32 val = 0;
	u32 addr = 0;

	addr = DISP0_TOP_CRP2_START + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);

	crop->hstart = (val >> 16) & 0xffff;
	crop->vstart = val & 0xffff;

	addr = DISP0_TOP_CRP2_SIZE + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);

	crop->hsize = (val >> 16) & 0xffff;
	crop->vsize = val & 0xffff;
}

void isp_disp_set_scaler_out_size(struct isp_dev_t *isp_dev, u32 idx, struct aml_format *fmt)
{
	u32 val = 0;
	u32 addr = 0;

	if (idx >= DISP_CNT) {
		pr_err("Error input disp index\n");
		return;
	}

	addr = DISP0_TOP_OUT_SIZE + ((idx * 0x100) << 2);

	val = ((fmt->width & 0xffff) << 16) |(fmt->height &0xffff);
	isp_reg_write(isp_dev, addr, val);
}

void isp_disp_set_csc2_fmt(struct isp_dev_t *isp_dev, u32 idx, struct aml_format *fmt)
{
	u32 val = 0;
	u32 addr = 0;
#ifndef T7C_CHIP
	if (idx != 0)
		return;
#endif

	isp_reg_update_bits(isp_dev, DISP0_TOP_TOP_CTRL + (idx * 0x100), 0, 1, 1);

	if (fmt->code != V4L2_PIX_FMT_RGB24) {
		pr_debug("Error to support disp%u csc2\n", idx);
		return;
	}

	val = (0 << 16) | (0x1e00 << 0);
	isp_reg_write(isp_dev, DISP0_CSC2_OFFSET_INP01 + (idx * 0x100), val);

	val = (0x1e00 << 0);
	isp_reg_write(isp_dev, DISP0_CSC2_OFFSET_INP2 + (idx * 0x100), val);

	val = (256 << 16) | (454 << 0);
	isp_reg_write(isp_dev, DISP0_CSC2_COEF_00_01 + (idx * 0x100), val);

	val = (0 << 16) | (256 << 0);
	isp_reg_write(isp_dev, DISP0_CSC2_COEF_02_10 + (idx * 0x100), val);

	val = (0x1fa8 << 16) | (0x1f49 << 0);
	isp_reg_write(isp_dev, DISP0_CSC2_COEF_11_12 + (idx * 0x100), val);

	val = (256 << 16) | (0 << 0);
	isp_reg_write(isp_dev, DISP0_CSC2_COEF_20_21 + (idx * 0x100), val);

	val = (359 << 0);
	isp_reg_write(isp_dev, DISP0_CSC2_COEF_22 + (idx * 0x100), val);

	val = (0 << 16) | (0 << 0);
	isp_reg_write(isp_dev, DISP0_CSC2_OFFSET_OUP01 + (idx * 0x100), val);

	val = (0 << 16) | (0 << 0);
	isp_reg_write(isp_dev, DISP0_CSC2_OFFSET_OUP2 + (idx * 0x100), val);

	isp_reg_write(isp_dev, ISP_DISP0_TOP_HW_RE + (idx * 0x100), 0x24);
	isp_reg_update_bits(isp_dev, ISP_DISP0_TOP_TOP_REG + (idx * 0x100), 0, 6, 3);
	isp_reg_update_bits(isp_dev, DISP0_TOP_TOP_CTRL + (idx * 0x100), 1, 1, 1);

	pr_info("rgb fmt need to set csc2\n");
}

void isp_disp_get_scaler_out_size(struct isp_dev_t *isp_dev, u32 idx, struct aml_format *fmt)
{
	u32 val = 0;
	u32 addr = 0;

	addr = DISP0_TOP_OUT_SIZE + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);

	fmt->width = (val >> 16) & 0xffff;
	fmt->height = val & 0xffff;
}

void isp_disp_cfg_sel(struct isp_dev_t *isp_dev, u32 idx, u32 sel)
{
	isp_reg_update_bits(isp_dev, ISP_TOP_DISPIN_SEL, sel, idx * 4, 4);
}

void isp_disp_enable(struct isp_dev_t *isp_dev, u32 idx)
{
	u32 addr;

	addr = DISP0_TOP_TOP_CTRL + ((idx * 0x100) << 2);
	isp_reg_update_bits(isp_dev, addr, 0x1, 5, 1);
	isp_reg_update_bits(isp_dev, addr, 0x0, 3, 1);
	isp_reg_update_bits(isp_dev, addr, 0x1, 2, 1);

	addr = ISP_TOP_PATH_EN;
	isp_reg_update_bits(isp_dev, addr, 0x1, idx, 1);
}

void isp_disp_disable(struct isp_dev_t *isp_dev, u32 idx)
{
	u32 addr;
	struct isp_global_info *g_info = isp_global_get_info();

	addr = DISP0_TOP_TOP_CTRL + ((idx * 0x100) << 2);
	isp_reg_update_bits(isp_dev, addr, 0x0, 5, 1);
	isp_reg_update_bits(isp_dev, addr, 0x0, 3, 1);
	isp_reg_update_bits(isp_dev, addr, 0x0, 2, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_DISPIN_SEL, 0xf, idx * 4, 4);

	addr = DISP0_PPS_SCALE_EN + ((idx * 0x100) << 2);
	isp_reg_update_bits(isp_dev, addr, 0, 20, 4);

	addr = ISP_TOP_PATH_EN;
	isp_reg_update_bits(isp_dev, addr, 0x0, idx, 1);

	if (g_info->mode == AML_ISP_SCAM)
		isp_hwreg_update_bits(isp_dev, addr, 0x0, idx, 1);
}

void isp_disp_set_input_size(struct isp_dev_t *isp_dev, u32 idx, struct aml_format *fmt)
{
	u32 val = 0;
	u32 addr = 0;

	if (idx >= DISP_CNT) {
		pr_err("Error input disp index\n");
		return;
	}

	addr = ISP_DISP0_TOP_IN_SIZE + ((idx * 0x100) << 2);

	val = ((fmt->width & 0xffff) << 16) |(fmt->height &0xffff);
	isp_reg_write(isp_dev, addr, val);
}

void isp_disp_pps_config(struct isp_dev_t *isp_dev, u32 idx,
			struct aml_crop *input, struct aml_format *output)
{
	u32 addr, val;
	int i, pps_en;
	int max_hsize;
	int hsc_en, vsc_en;
	int preh_en, prev_en;
	int thsize, tvsize;
	u32 reg_prehsc_rate, reg_prevsc_rate, reg_prehsc_flt_num;
	u32 reg_prevsc_flt_num, reg_hsc_tap_num, reg_vsc_tap_num;
	int preh_force_open, prev_flt_4_invld, prev_flt_8_invld, pre_vscale_max_hsize;
	u32 pre_hsc_mult, ihsize_after_pre_hsc, ihsize_pre_vsc_vld;
	u32 reg_prehsc_rate_alt, pre_hsc_mult_alt, ihsize_after_pre_hsc_alt;
	int ohsize_range;
	u32 reg_vsc_tap_num_alt;
	u32 step_h_frac_div;
	u32 step_v_frac_div;
	int step_h_integer, step_v_integer;
	int step_h_fraction;
	int step_v_fraction;
	int yuv444to422_en;
	u32 ihsize = input->hsize;
	u32 ivsize = input->vsize;
	u32 ohsize = output->width;
	u32 ovsize = output->height;
	const int (*pps_lut_tap4)[4] = pps_lut_tap4_s11;
	const int (*pps_lut_tap2)[2] = pps_lut_tap2_s11;
	const int (*pps_lut_tap8)[8] = pps_lut_tap8_s11;

	hsc_en = (ihsize == ohsize) ? 0 : 1;
	vsc_en = (ivsize == ovsize) ? 0 : 1;
#ifdef T7C_CHIP
	max_hsize = 4608;
#else
	max_hsize = (idx == 2) ? 2888 : 1920;
#endif
	preh_en = (ihsize > 4 * ohsize) ? 1 : 0;
	prev_en = (ivsize > 4 * ovsize) ? 1 : 0;

	pps_en = hsc_en | vsc_en | preh_en | prev_en;
	if (!pps_en) {
		pr_info("ISP%u: No need to enable idx %u pps\n", isp_dev->index, idx);
		return;
	}

	pr_info("ISP%u: pps%u: ih-iv: %u-%u, oh-ov: %u-%u\n",
			isp_dev->index, idx, ihsize, ivsize, ohsize, ovsize);

	addr = DISP0_PPS_SCALE_EN + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);

	reg_prehsc_flt_num = (val >>12) & 0xf;
	reg_prevsc_flt_num = (val >> 8) & 0xf;
	reg_hsc_tap_num    = (val >> 4) & 0xf;
	reg_vsc_tap_num    = (val >> 0) & 0xf;
#ifdef T7C_CHIP
	pre_vscale_max_hsize = reg_prevsc_flt_num == 2 ? max_hsize : reg_prevsc_flt_num == 4 ? max_hsize/2 : max_hsize/4;
	preh_force_open  = (ihsize > pre_vscale_max_hsize) & prev_en;
	//prev_flt_4_invld = (ihsize > (max_hsize/2)) & (ihsize <= max_hsize)   & prev_en;
	//prev_flt_8_invld = (ihsize > (max_hsize/4)) & (ihsize <= max_hsize/2) & prev_en;
	reg_prehsc_rate    = (preh_en | preh_force_open) ? 1 : 0;

	reg_prevsc_rate    = prev_en ? 1 : 0;

	pre_hsc_mult             = 1 << reg_prehsc_rate;
	ihsize_after_pre_hsc     = preh_en | preh_force_open ? (ihsize + pre_hsc_mult - 1) / pre_hsc_mult : ihsize;

	prev_flt_4_invld         = (reg_prevsc_flt_num == 4) & (ihsize_after_pre_hsc > max_hsize/2);
	prev_flt_8_invld         = (reg_prevsc_flt_num == 8) & (ihsize_after_pre_hsc > max_hsize/4);

	reg_prevsc_flt_num       = ( prev_flt_4_invld |  prev_flt_8_invld)  & prev_en ?
			        (reg_prevsc_flt_num >> 1) : reg_prevsc_flt_num; //reg_prevsc_flt_num = 2/4/8
#else
	preh_force_open  = ((idx==0) | (idx==1)) & (ihsize > max_hsize); //& prev_en;
	prev_flt_4_invld = ((idx==0) | (idx==1)) & (ihsize > (max_hsize/2)) & (ihsize <= max_hsize)   & prev_en;
	prev_flt_8_invld = ((idx==0) | (idx==1)) & (ihsize > (max_hsize/4)) & (ihsize <= max_hsize/2) & prev_en;
	reg_prehsc_rate    = (preh_en | preh_force_open) ? 1 : 0;

	reg_prevsc_rate    = prev_en ? 1 : 0;

	reg_prevsc_flt_num		= ((reg_prevsc_flt_num == 4) & prev_flt_4_invld) | ((reg_prevsc_flt_num == 8) & prev_flt_8_invld) ?
							(reg_prevsc_flt_num >> 1) : reg_prevsc_flt_num; //reg_prevsc_flt_num = 2/4/8

	pre_hsc_mult			 = 1 << reg_prehsc_rate;
	ihsize_after_pre_hsc	 = preh_en | preh_force_open ? (ihsize + pre_hsc_mult - 1) / pre_hsc_mult : ihsize;
#endif
	pre_vscale_max_hsize = reg_prevsc_flt_num == 2 ? max_hsize : reg_prevsc_flt_num == 4 ? max_hsize/2 : max_hsize/4;// LBUF_MAX_HSIZE/2

	ihsize_pre_vsc_vld       = prev_en ? ihsize_after_pre_hsc < pre_vscale_max_hsize : 1;

	reg_prehsc_rate_alt      = ihsize_pre_vsc_vld ? reg_prehsc_rate : reg_prehsc_rate + 1; //reg_prehsc_rate = 0/1/2/3
	pre_hsc_mult_alt         = 1 << reg_prehsc_rate_alt;
	ihsize_after_pre_hsc_alt = preh_en | preh_force_open ? (ihsize + pre_hsc_mult_alt - 1) / pre_hsc_mult_alt : ihsize;

	ohsize_range             = ihsize_after_pre_hsc_alt <= (max_hsize/4) ? 0
							: ihsize_after_pre_hsc_alt <= (max_hsize/2) ? 1
							: ihsize_after_pre_hsc_alt <= max_hsize ? 2
							: 3;

	reg_vsc_tap_num_alt      = prev_en ? ( ohsize_range == 2 ? 2
							: ohsize_range == 1 ? (reg_vsc_tap_num > 4 ? 4 : reg_vsc_tap_num)
							: reg_vsc_tap_num )
							: (ohsize_range == 3 ? 2
							: ohsize_range == 2 ? (reg_vsc_tap_num > 4 ? 4 : reg_vsc_tap_num) : reg_vsc_tap_num );

	thsize = ihsize_after_pre_hsc_alt;
	tvsize = prev_en ? (ivsize+1)/2 : ivsize;

	step_h_integer  = thsize/ohsize;
	step_v_integer  = tvsize/ovsize;

	if(thsize > 2048) {
		step_h_frac_div = (((thsize << 18 ) / ohsize) << 2) - (step_h_integer << 20);
	} else {
		step_h_frac_div = ( thsize << 20) / ohsize - (step_h_integer << 20);
	}

	if(tvsize > 2048) {
		step_v_frac_div = (((tvsize << 18 ) / ovsize) << 2) - (step_v_integer << 20);
	} else {
		step_v_frac_div = ( tvsize << 20) / ovsize - (step_v_integer << 20);
	}

	yuv444to422_en = ihsize > (max_hsize/2) ? 1 : 0;

	step_h_fraction = step_h_frac_div << 4;//24bit
	step_v_fraction = step_v_frac_div << 4;//24bit

	addr = DISP0_PPS_SCALE_EN + ((idx * 0x100) << 2);
	val = (9 << 28) | (9 << 24) |
		((preh_en | preh_force_open) << 23) | (prev_en << 22) |
		(vsc_en << 21) | (hsc_en << 20) |
		((reg_prehsc_rate_alt & 3) << 18) |
		((reg_prevsc_rate & 0x03) << 16) |
		((reg_prehsc_flt_num & 0x0f) << 12) |
		((reg_prevsc_flt_num & 0x0f) << 8) |
		((reg_hsc_tap_num & 0x0f) << 4) |
		((reg_vsc_tap_num_alt & 0x0f) << 0);
	isp_reg_write(isp_dev, addr, val);

	addr = DISP0_PPS_444TO422 + ((idx * 0x100) << 2);
	val = yuv444to422_en & 0x1;
	isp_reg_write(isp_dev, addr, val);

	addr = DISP0_PPS_VSC_START_PHASE_STEP + ((idx * 0x100) << 2);
	val = (0 << 28) | (step_v_integer << 24) | (step_v_fraction & 0xffffff);
	isp_reg_write(isp_dev, addr, val);

	addr = DISP0_PPS_HSC_START_PHASE_STEP + ((idx * 0x100) << 2);
	val = (0 << 28) | (step_h_integer << 24) | (step_h_fraction & 0xffffff);
	isp_reg_write(isp_dev, addr, val);

	if (reg_vsc_tap_num_alt == 2) {
			addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
			val = (0 << 10) | (1 << 9) | (1 << 8) | (0 << 7) | (0 << 0);
			isp_reg_write(isp_dev, addr, val);

			addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
			for (i = 0; i < 33; i++) {
				val = ((pps_lut_tap2[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap2[i][1] & 0x7ff) << 0);
				isp_reg_write(isp_dev, addr, val);
				isp_reg_write(isp_dev, addr, 0);
			}

			addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
			val = (0 << 10) | (1 << 9) | (1 << 8) | (0 << 7) | (0 << 0);
			isp_reg_write(isp_dev, addr, val);

			addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
			for (i = 0; i < 33; i++) {
				val = ((pps_lut_tap2[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap2[i][1] & 0x7ff) << 0);
				isp_reg_write(isp_dev, addr, val);
				isp_reg_write(isp_dev, addr, 0);
			}

	} else if ( reg_vsc_tap_num_alt == 4) {

		addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
		val = (0 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap4[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap4[i][1] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap4[i][2] << 16 ) & 0x7ff0000) | ((pps_lut_tap4[i][3] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
		}

		addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
		val = (0 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap4[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap4[i][1] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap4[i][2] << 16 ) & 0x7ff0000) | ((pps_lut_tap4[i][3] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

		}
	} else {

		addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
		val = (0 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap8[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][1] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap8[i][2] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][3] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
		}

		addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
		val = (1 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap8[i][4] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][5] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap8[i][6] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][7] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
		}

		addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
		val = (0 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap8[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][1] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap8[i][2] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][3] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
		}

		addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
		val = (1 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap8[i][4] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][5] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap8[i][6] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][7] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
		}
	}

	if (reg_hsc_tap_num == 2) {

		addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
		val = (2 << 10) | (1 << 9) | (1 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap2[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap2[i][1] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
			isp_reg_write(isp_dev, addr, 0);
		}

		addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
		val = (2 << 10) | (1 << 9) | (1 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap2[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap2[i][1] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
			isp_reg_write(isp_dev, addr, 0);
		}
	} else if (reg_hsc_tap_num == 4) {

		addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
		val = (2 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap4[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap4[i][1] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap4[i][2] << 16 ) & 0x7ff0000) | ((pps_lut_tap4[i][3] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
		}

		addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
		val = (2 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap4[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap4[i][1] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap4[i][2] << 16 ) & 0x7ff0000) | ((pps_lut_tap4[i][3] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
		}

	} else {

		addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
		val = (2 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap8[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][1] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap8[i][2] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][3] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
		}

		addr = ISP_SCALE0_COEF_IDX_LUMA + ((idx * 0x100) << 2);
		val = (3 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_LUMA + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap8[i][4] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][5] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap8[i][6] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][7] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
		}

		addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
		val = (2 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap8[i][0] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][1] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap8[i][2] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][3] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
		}

		addr = ISP_SCALE0_COEF_IDX_CHRO + ((idx * 0x100) << 2);
		val = (3 << 10) | (1 << 9) | (0 << 8) | (0 << 7) | (0 << 0);
		isp_reg_write(isp_dev, addr, val);

		addr = ISP_SCALE0_COEF_CHRO + ((idx * 0x100) << 2);
		for (i = 0; i < 33; i++) {
			val = ((pps_lut_tap8[i][4] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][5] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);

			val = ((pps_lut_tap8[i][6] << 16 ) & 0x7ff0000) | ((pps_lut_tap8[i][7] & 0x7ff) << 0);
			isp_reg_write(isp_dev, addr, val);
		}
	}

	addr = DISP0_PPS_PRE_HSCALE_COEF_1 + ((idx * 0x100) << 2);
	val = (32 << 16) | 32;
	isp_reg_write(isp_dev, addr, val);

	addr = DISP0_PPS_PRE_HSCALE_COEF_0 + ((idx * 0x100) << 2);
	val = (128 << 16) | 128;
	isp_reg_write(isp_dev, addr, val);

}

void isp_disp_set_overlap(struct isp_dev_t *isp_dev, int ovlp)
{
	u32 addr = 0;
	u32 val = 0;

	addr = ISP_TOP_STITCH_CTRL;
	val = ovlp;

	isp_reg_update_bits(isp_dev, addr, val, 1, 16);
}

void isp_disp_get_overlap(struct isp_dev_t *isp_dev, int *ovlp)
{
	u32 addr = 0;
	u32 val = 0;

	addr = ISP_TOP_STITCH_CTRL;
	isp_reg_read_bits(isp_dev, addr, &val, 1, 16);

	*ovlp = val;
}

void isp_disp_calc_slice(struct isp_dev_t *isp_dev, u32 idx, struct aml_slice *param)
{
	int i = 0;
	u32 addr, val;
	int xsize;
	int crop_hend;
	int hscale_rate;
	int phase_step[2];
	int post_h_posi[2];
	int phase_sum[2];
	int phase_cnt[2];
	int phase_acc[2];
	int hinte[2];
	int right_hsc_ini_integer[2] = {0};
	int right_hsc_ini_phs0[2] = {0};
	s64 left_hsize, left_overlap;
	s64 right_hsize, right_overlap;
	int tmp_left_hsize, tmp_left_overlap;
	int tmp_right_hsize, tmp_right_overlap;
	int right_frame_hstart, whole_frame_hcenter;

	u32 reg_prehsc_rate;
	u32 reg_hsc_fraction_part;
	u32 reg_hsc_integer_part;
	u32 reg_hsc_yuv444to422_en;
	u32 reg_hsc_en;
	u32 reg_stch_ovlp_num;
	u32 reg_disp_out_hsize;
	u32 reg_crop_hstart;
	u32 reg_crop_hsize;
	u32 reg_crop_en;
	u32 reg_hsc_ini_integer[2];
	u32 reg_hsc_ini_phs0[2];
	u32 reg_prehsc_en;

	addr = DISP0_PPS_SCALE_EN + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	reg_prehsc_rate = (val >> 18) & 0x3;
	reg_hsc_en = (val >> 20) & 0x1;
	reg_prehsc_en = (val >> 23) & 0x1;

	hscale_rate = (reg_prehsc_rate == 0) ? 1 :
			(reg_prehsc_rate == 1) ? 2 :
			(reg_prehsc_rate == 2) ? 4 : 8;

	addr = DISP0_PPS_HSC_START_PHASE_STEP + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	reg_hsc_fraction_part = val & 0xffffff;
	reg_hsc_integer_part = (val >> 24) & 0xf;

	phase_step[0] = reg_hsc_fraction_part + (reg_hsc_integer_part << 24);
	phase_step[1] = phase_step[0];

	addr = DISP0_PPS_444TO422 + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	reg_hsc_yuv444to422_en = val & 0x1;

	if (reg_hsc_yuv444to422_en == 1)
		phase_step[1] = phase_step[0] >> 1;

	if ((reg_hsc_en == 0) && (reg_hsc_yuv444to422_en == 1))
		phase_step[1] = (1 << 23);


	addr = ISP_TOP_STITCH_CTRL;
	val = isp_reg_read(isp_dev, addr);
	reg_stch_ovlp_num = (val >> 1) & 0x1ffff;

	addr = DISP0_TOP_CRP2_START + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	reg_crop_hstart = (val >> 16) & 0xffff;

	addr = DISP0_TOP_CRP2_SIZE + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	reg_crop_hsize = (val >> 16) & 0xffff;

	addr = DISP0_PPS_HSC_LUMA_PHASE_CTRL + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	reg_hsc_ini_phs0[0] = val & 0xffffff;
	reg_hsc_ini_integer[0] = (val >> 24) & 0x1f;

	addr = DISP0_PPS_HSC_CHROMA_PHASE_CTRL + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	reg_hsc_ini_phs0[1] = val & 0xffffff;
	reg_hsc_ini_integer[1] = (val >> 24) & 0x1f;

	addr = DISP0_TOP_OUT_SIZE + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	reg_disp_out_hsize = (val >> 16) & 0x1fff;

	addr = DISP0_TOP_TOP_CTRL + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	reg_crop_en = (val >> 5) & 0x1;

	addr = ISP_DISP0_TOP_IN_SIZE + ((idx * 0x100) << 2);
	val = isp_reg_read(isp_dev, addr);
	xsize = (val >> 16) & 0xffff;

	left_hsize = xsize;
	left_overlap = reg_stch_ovlp_num;
	right_hsize = xsize;
	right_overlap = reg_stch_ovlp_num;

	whole_frame_hcenter = xsize -reg_stch_ovlp_num;
	right_frame_hstart = xsize - reg_stch_ovlp_num * 2;

	param->pleft_hsize = left_hsize;
	param->pleft_ovlp = left_overlap;
	param->pright_hsize = right_hsize;
	param->pright_ovlp = right_overlap;

	if (reg_crop_en == 1) {
		crop_hend = (reg_crop_hstart + reg_crop_hsize) > whole_frame_hcenter ?
				xsize : (reg_crop_hstart + reg_crop_hsize);
		left_hsize = reg_crop_hstart > whole_frame_hcenter ?
				0 : (crop_hend - reg_crop_hstart);
		left_overlap = ((reg_crop_hstart > whole_frame_hcenter) ||(crop_hend < whole_frame_hcenter)) ? 0 : reg_stch_ovlp_num;

		crop_hend = reg_crop_hstart + reg_crop_hsize;
		right_hsize = crop_hend < whole_frame_hcenter ?
				0 : (reg_crop_hstart > whole_frame_hcenter) ?
				(crop_hend - reg_crop_hstart) : crop_hend;
		right_overlap = ((reg_crop_hstart > whole_frame_hcenter) || (crop_hend < whole_frame_hcenter)) ? 0 : reg_stch_ovlp_num;
	}

	if (reg_prehsc_en == 1) {
		left_hsize = disp_ceil(left_hsize, hscale_rate);
		left_overlap = disp_ceil(left_overlap, hscale_rate);
		right_hsize = disp_ceil(right_hsize, hscale_rate);
		right_overlap = disp_ceil(right_overlap, hscale_rate);
	}

	if (reg_hsc_en) {
		tmp_left_hsize = left_hsize;
		tmp_left_overlap = left_overlap;
		tmp_right_hsize = right_hsize;
		tmp_right_overlap = right_overlap;

		post_h_posi[0] = tmp_left_hsize - tmp_left_overlap - tmp_right_overlap;
		post_h_posi[1] = (reg_hsc_yuv444to422_en == 1) ?
				(tmp_left_hsize / 2 - tmp_left_overlap / 2  - tmp_right_overlap / 2) : post_h_posi[0];

		left_hsize = div_s64(left_hsize << 24, phase_step[0]);
		left_overlap = div_s64(left_overlap << 24, phase_step[0]);

		hinte[0] = reg_hsc_ini_integer[0];
		hinte[1] = reg_hsc_ini_integer[1];
		phase_acc[0] = reg_hsc_ini_phs0[0];
		phase_acc[1] = reg_hsc_ini_phs0[1];
		phase_sum[0] = 0;
		phase_sum[1] = 0;

		for (i = 0; i < left_hsize; i++) {
			if (hinte[0] >= post_h_posi[0])
				break;
			phase_sum[0] = phase_acc[0] + phase_step[0];
			phase_cnt[0] = phase_sum[0] >> 24;
			phase_acc[0] = phase_sum[0] & 0xffffff;
			hinte[0] = hinte[0] + phase_cnt[0];
		}

		right_hsc_ini_integer[0] = hinte[0] - post_h_posi[0];
		right_hsc_ini_phs0[0] = phase_acc[0];
		right_overlap = left_hsize - i - left_overlap;
		right_hsize = left_hsize - left_overlap + right_overlap;

		for (i = 0; i < left_hsize; i++) {
			if (hinte[1] >= post_h_posi[1])
				break;
			phase_sum[1] = phase_acc[1] + phase_step[1];
			phase_cnt[1] = phase_sum[1] >> 24;
			phase_acc[1] = phase_sum[1] & 0xffffff;
			hinte[1] = hinte[1] + phase_cnt[1];
		}

		right_hsc_ini_integer[1] = hinte[1] - post_h_posi[1];
		right_hsc_ini_phs0[1] = phase_acc[1];
	}

	param->xsize = xsize;
	param->left_hsize = left_hsize;
	param->right_hsize = right_hsize;
	param->left_ovlp = left_overlap;
	param->right_ovlp = right_overlap;
	param->right_frame_hstart = right_frame_hstart;
	param->whole_frame_hcenter = whole_frame_hcenter;
	param->reg_crop_hstart = reg_crop_hstart;
	param->reg_crop_hsize = reg_crop_hsize;
	param->reg_hsc_ini_integer[0] = reg_hsc_ini_integer[0];
	param->reg_hsc_ini_integer[1] = reg_hsc_ini_integer[1];
	param->reg_hsc_ini_phs0[0] = reg_hsc_ini_phs0[0];
	param->reg_hsc_ini_phs0[1] = reg_hsc_ini_phs0[1];
	param->right_hsc_ini_integer[0] = right_hsc_ini_integer[0];
	param->right_hsc_ini_integer[1] = right_hsc_ini_integer[1];
	param->right_hsc_ini_phs0[0] = right_hsc_ini_phs0[0];
	param->right_hsc_ini_phs0[1] = right_hsc_ini_phs0[1];

	pr_info("ISP%u: left_hsize-left_ovlp: %lld-%lld, right_hsize-right_ovlp: %lld-%lld\n",
				idx, left_hsize, left_overlap, right_hsize, right_overlap);
}

void isp_disp_cfg_slice(struct isp_dev_t *isp_dev, u32 idx, struct aml_slice *param)
{
	u32 addr, val;
	int crop_hend;
	int xsize = param->xsize;
	int right_frame_hstart = param->right_frame_hstart;
	int whole_frame_hcenter = param->whole_frame_hcenter;
	int reg_crop_hstart = param->reg_crop_hstart;
	int reg_crop_hsize = param->reg_crop_hsize;
	int *reg_hsc_ini_integer = param->reg_hsc_ini_integer;
	int *reg_hsc_ini_phs0 = param->reg_hsc_ini_phs0;
	int *right_hsc_ini_integer = param->right_hsc_ini_integer;
	int *right_hsc_ini_phs0 = param->right_hsc_ini_phs0;

	if (param->pos == 0) { // left half
		addr = DISP0_PPS_HSC_LUMA_PHASE_CTRL + ((idx * 0x100) << 2);
		val = (reg_hsc_ini_integer[0] << 24) | (reg_hsc_ini_phs0[0] << 0);
		isp_hwreg_write(isp_dev, addr, val);

		addr = DISP0_PPS_HSC_CHROMA_PHASE_CTRL + ((idx * 0x100) << 2);
		val = (reg_hsc_ini_integer[1] << 24) | (reg_hsc_ini_phs0[1] << 0);
		isp_hwreg_write(isp_dev, addr, val);

		addr = DISP0_TOP_OUT_SIZE + ((idx * 0x100) << 2);
		val = param->left_hsize;
		isp_hwreg_update_bits(isp_dev, addr, val, 16, 16);
	} else if (param->pos == 1) { // right half
		addr = DISP0_PPS_HSC_LUMA_PHASE_CTRL + ((idx * 0x100) << 2);
		val = (right_hsc_ini_integer[0] << 24) | (right_hsc_ini_phs0[0] << 0);
		isp_hwreg_write(isp_dev, addr, val);

		addr = DISP0_PPS_HSC_CHROMA_PHASE_CTRL + ((idx * 0x100) << 2);
		val = (right_hsc_ini_integer[1] << 24) | (right_hsc_ini_phs0[1] << 0);
		isp_hwreg_write(isp_dev, addr, val);

		addr = DISP0_TOP_OUT_SIZE + ((idx * 0x100) << 2);
		val = param->right_hsize;
		isp_hwreg_update_bits(isp_dev, addr, val, 16, 16);
	}
}

void isp_disp_cfg_param(struct isp_dev_t *isp_dev, u32 idx, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];
	return;

	if (param->pvalid.aisp_base) {
		disp_cfg_base_fxset(isp_dev, idx, &param->base_cfg);
		disp_cfg_base_fxlut(isp_dev, idx, &param->base_cfg);
	}
}

void isp_disp_init(struct isp_dev_t *isp_dev)
{
	u32 i = 0;
#ifdef T7C_CHIP
	isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, 0x7, 4, 4);
#endif
	for (i = 0; i < 3; i++) {
		isp_reg_update_bits(isp_dev, DISP0_TOP_TOP_CTRL + ((i * DISP_INTER) << 2), 0, 0, 7);

		isp_reg_update_bits(isp_dev, DISP0_PPS_SCALE_EN + ((i * DISP_INTER) << 2), 4, 0, 4);
		isp_reg_update_bits(isp_dev, DISP0_PPS_SCALE_EN + ((i * DISP_INTER) << 2), 4, 4, 4);
	}
}
