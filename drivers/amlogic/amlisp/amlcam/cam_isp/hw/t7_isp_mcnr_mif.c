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

static void mcnr_mv_wrmif_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize, ysize;

	xsize = fmt->width;
	ysize = fmt->height;

	val = (0 << 0) | ((((xsize + 7) >> 3) - 1) << 16);
	isp_reg_write(isp_dev, ISP_WMIF_NR2_SCOPE_X, val);

	val = (0 << 0) | ((((ysize + 7) >> 3) - 1) << 16);
	isp_reg_write(isp_dev, ISP_WMIF_NR2_SCOPE_Y, val);

	val = (((xsize + 7) >> 3) * 16 + 127) >> 7;
	isp_reg_update_bits(isp_dev, ISP_WMIF_NR2_CTRL3, val, 0, 13);
}

static void mcnr_mv_wrmif_init(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_WMIF_NR2_CTRL1, 0, 0, 3);

	isp_reg_update_bits(isp_dev, ISP_WMIF_NR2_CTRL5, 0, 24, 1);
	isp_reg_update_bits(isp_dev, ISP_WMIF_NR2_CTRL5, 2, 3, 4);
	isp_reg_update_bits(isp_dev, ISP_WMIF_NR2_CTRL5, 0, 0, 3);
}

static void mcnr_mv_rdmif_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize, ysize;

	xsize = fmt->width;
	ysize = fmt->height;

	val = (0 << 0) | ((((xsize + 7) >> 3) - 1) << 16);
	isp_reg_write(isp_dev, ISP_RMIF_NR2_SCOPE_X, val);

	val = (0 << 0) | ((((ysize + 7) >> 3) - 1) << 16);
	isp_reg_write(isp_dev, ISP_RMIF_NR2_SCOPE_Y, val);

	val = (((xsize + 7) >> 3) * 16 + 127) >> 7;
	isp_reg_update_bits(isp_dev, ISP_RMIF_NR2_CTRL3, val, 0, 13);
}

static void mcnr_mv_rdmif_init(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_RMIF_NR2_CTRL1, 0, 0, 3);

	isp_reg_update_bits(isp_dev, ISP_RMIF_NR2_CTRL5, 2, 3, 4);
	isp_reg_update_bits(isp_dev, ISP_RMIF_NR2_CTRL5, 0, 0, 3);
}

static void mcnr_meta_wrmif_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize, ysize;

	xsize = fmt->width;
	ysize = fmt->height;

	val = (0 << 0) | ((((xsize + 1) >> 1) - 1) << 16);
	isp_reg_write(isp_dev, ISP_WMIF_NR3_SCOPE_X, val);

	val = (0 << 0) | ((((ysize + 1) >> 1) - 1) << 16);
	isp_reg_write(isp_dev, ISP_WMIF_NR3_SCOPE_Y, val);

	val = (((xsize + 1) >> 1) * 4 + 127) >> 7;
	isp_reg_update_bits(isp_dev, ISP_WMIF_NR3_CTRL3, val, 0, 13);
}

static void mcnr_meta_wrmif_init(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_WMIF_NR3_CTRL5, 0, 24, 1);

	isp_reg_update_bits(isp_dev, ISP_WMIF_NR3_CTRL5, 0, 3, 4);
	isp_reg_update_bits(isp_dev, ISP_WMIF_NR3_CTRL5, 0, 0, 3);
}

static void mcnr_meta_rdmif_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize, ysize;

	xsize = fmt->width;
	ysize = fmt->height;

	val = (0 << 0) | ((((xsize + 1) >> 1) - 1) << 16);
	isp_reg_write(isp_dev, ISP_RMIF_NR3_SCOPE_X, val);

	val = (0 << 0) | ((((ysize + 1) >> 1) - 1) << 16);
	isp_reg_write(isp_dev, ISP_RMIF_NR3_SCOPE_Y, val);

	val = (((xsize + 1) >> 1) * 4 + 127) >> 7;
	isp_reg_update_bits(isp_dev, ISP_RMIF_NR3_CTRL3, val, 0, 13);
}

static void mcnr_meta_rdmif_init(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_RMIF_NR3_CTRL5, 0, 3, 4);
	isp_reg_update_bits(isp_dev, ISP_RMIF_NR3_CTRL5, 0, 0, 3);
}

static void mcnr_mix_wrmif_loss_core(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize, ysize;

	xsize = fmt->width;
	ysize = fmt->height;

	val = ((xsize >> 1) << 0) | ((ysize >> 1) << 16);
	isp_reg_write(isp_dev, ISP_LOSSE_MIX_PIC_SIZE, val);

	val = ysize >> 1;
	isp_reg_update_bits(isp_dev, ISP_LOSSE_MIX_SLICE_Y, val, 0, 16);

	val = ysize >> 2;
	isp_reg_update_bits(isp_dev, ISP_LOSSE_MIX_RC_GROUP_2, val, 16, 16);

	val = (1 << 14) / ((xsize + 15) >> 4);
	isp_reg_update_bits(isp_dev, ISP_LOSSE_MIX_RC_GROUP_2, val, 0, 14);

	isp_reg_update_bits(isp_dev, ISP_LOSSE_MIX_CTRL, 15, 4, 4);

	isp_reg_update_bits(isp_dev, ISP_LOSSE_MIX_BASIS, 0, 31, 1);
}

static void mcnr_mix_rdmif_loss_core(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize, ysize;

	xsize = fmt->width;
	ysize = fmt->height;

	val = ((ysize >> 1) << 16) | ((xsize >> 1) << 0);
	isp_reg_write(isp_dev, ISP_LOSSD_MIX_PIC_SIZE, val);

	val = ysize >> 1;
	isp_reg_update_bits(isp_dev, ISP_LOSSD_MIX_SLICE_Y, val, 0, 16);

	val = ysize >> 2;
	isp_reg_update_bits(isp_dev, ISP_LOSSD_MIX_RC_GROUP_2, val, 16, 16);

	val = (1 << 14) / ((xsize + 15) >> 4);
	isp_reg_update_bits(isp_dev, ISP_LOSSD_MIX_RC_GROUP_2, val, 0, 14);

	isp_reg_update_bits(isp_dev, ISP_LOSSD_MIX_CTRL, 15, 4, 4);

	isp_reg_update_bits(isp_dev, ISP_LOSSD_MIX_BASIS, 0, 31, 1);
}

static void mcnr_pix_wrmif_loss_core(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize, ysize;

	xsize = fmt->width;
	ysize = fmt->height;

	val = (ysize << 16) | (xsize << 0);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_PIC_SIZE, val);

	val = ysize;
	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_RC_GROUP_2, val, 16, 16);

	val = (1 << 14) / ((xsize + 7) >> 3);
	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_RC_GROUP_2, val, 0, 14);

	val = (1 << 16) / ((ysize + 1) >> 1);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_ACCUM_OFSET_3, val);

	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_CTRL, 15, 4, 4);

	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_BASIS, 0, 18, 1);

	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_OFST_BIT_DEPTH, 16, 0, 5);

	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_BASIS, 0, 31, 1);
}

static void mcnr_pix_rdmif_loss_core(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	u32 xsize, ysize;

	xsize = fmt->width;
	ysize = fmt->height;

	val = (ysize << 16) | (xsize << 0);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_PIC_SIZE, val);

	val = ysize;
	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_RC_GROUP_2, val, 16, 16);

	val = (1 << 14) / ((xsize + 7) >> 3);
	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_RC_GROUP_2, val, 0, 14);

	val = (1 << 16) / ((ysize + 1) >> 1);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_ACCUM_OFSET_3, val);

	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_CTRL, 15, 4, 4);

	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_BASIS, 0, 18, 1);

	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_OFST_BIT_DEPTH, 16, 0, 5);

	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_BASIS, 0, 31, 1);
}

void isp_mcnr_mif_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	mcnr_mv_wrmif_cfg_size(isp_dev, fmt);
	mcnr_meta_wrmif_cfg_size(isp_dev, fmt);

	mcnr_mv_rdmif_cfg_size(isp_dev, fmt);
	mcnr_meta_rdmif_cfg_size(isp_dev, fmt);

	mcnr_mix_wrmif_loss_core(isp_dev, fmt);
	mcnr_mix_rdmif_loss_core(isp_dev, fmt);

	mcnr_pix_wrmif_loss_core(isp_dev, fmt);
	mcnr_pix_rdmif_loss_core(isp_dev, fmt);
}

void isp_mcnr_mif_cfg_buf(struct isp_dev_t *isp_dev, struct aml_format *fmt, struct aml_buffer *buff)
{
	u32 mv_addr = 0;
	u32 meta_size = 0;
	u32 meta_addr = 0;

	meta_size = (((fmt->width + 15) / 16) * 16) * fmt->height / 4 * 4 / 8;
	meta_size = ISP_ALIGN(meta_size, 1 << 12);
	meta_addr = buff->addr[AML_PLANE_A];

	mv_addr = meta_addr + meta_size;

	isp_reg_write(isp_dev, ISP_WMIF_NR2_CTRL4, mv_addr >> 4);
	isp_reg_write(isp_dev, ISP_WMIF_NR3_CTRL4, meta_addr >> 4);

	isp_reg_write(isp_dev, ISP_RMIF_NR2_CTRL4, mv_addr >> 4);
	isp_reg_write(isp_dev, ISP_RMIF_NR3_CTRL4, meta_addr >> 4);
}

void isp_mcnr_mif_enable(struct isp_dev_t *isp_dev, u32 enable)
{
	if (enable) {
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, 0xff, 20, 8);
		isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, 1, 10, 1);
		isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, 1, 11, 1);
		isp_dev->mcnr_en = 1;
	} else {
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, 0x0, 20, 8);
		isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, 0, 10, 1);
		isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, 0, 11, 1);
		isp_reg_update_bits(isp_dev, ISP_PK_MOTION_ADP_CTRL, 0, 0, 1);
		isp_dev->mcnr_en = 0;
	}
}

void isp_mcnr_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	int isp_fmt = 0;
	u32 raw_mode = 1;
	int *raw_phslut = NULL;
	int xofst, yofst;
	int mono_phs[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int grbg_phs[] = {1,0,1,0,3,2,3,2,1,0,1,0,3,2,3,2};
	int rggb_phs[] = {0,1,0,1,2,3,2,3,0,1,0,1,2,3,2,3};
	int bggr_phs[] = {2,3,2,3,0,1,0,1,2,3,2,3,0,1,0,1};
	int gbrg_phs[] = {3,2,3,2,1,0,1,0,3,2,3,2,1,0,1,0};
	int irbg_phs[] = {4,1,4,1,2,3,2,3,4,1,4,1,2,3,2,3};
	int grbi_phs[] = {0,1,0,1,2,4,2,4,0,1,0,1,2,4,2,4};
	int grig_4x4_phs[] = {0,1,0,2,4,3,4,3,0,2,0,1,4,3,4,3};
	int *bayer_phs[ISP_FMT_MAX] = {grbg_phs, rggb_phs,
			bggr_phs, gbrg_phs, grig_4x4_phs};

	raw_phslut = (raw_mode == 0) ? mono_phs:
			(raw_mode == 1) ? grbg_phs:
			(raw_mode == 2) ? irbg_phs:
			(raw_mode == 3) ? grbi_phs: grig_4x4_phs;

	isp_fmt = isp_hw_convert_fmt(fmt);
	raw_phslut = bayer_phs[isp_fmt];

	val = raw_phslut[0] | (raw_phslut[1] << 4) |
		(raw_phslut[2] << 8) | (raw_phslut[3] << 12) |
		(raw_phslut[4] << 16) | (raw_phslut[5] << 20) |
		(raw_phslut[6] << 24) | (raw_phslut[7] << 28);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_GLOBAL_PHASE_LUT_1, val);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_GLOBAL_PHASE_LUT_1, val);

	val = raw_phslut[8] | (raw_phslut[9] << 4) |
		(raw_phslut[10] << 8) | (raw_phslut[11] << 12) |
		(raw_phslut[12] << 16) | (raw_phslut[13] << 20) |
		(raw_phslut[14] << 24) | (raw_phslut[15] << 28);
	isp_reg_write(isp_dev, ISP_LOSSE_PIX_GLOBAL_PHASE_LUT, val);
	isp_reg_write(isp_dev, ISP_LOSSD_PIX_GLOBAL_PHASE_LUT, val);

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

	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_OFST_BIT_DEPTH, xofst, 10, 2);
	isp_reg_update_bits(isp_dev, ISP_LOSSE_PIX_OFST_BIT_DEPTH, yofst, 8, 2);

	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_OFST_BIT_DEPTH, xofst, 10, 2);
	isp_reg_update_bits(isp_dev, ISP_LOSSD_PIX_OFST_BIT_DEPTH, yofst, 8, 2);
}

void isp_mcnr_mif_init(struct isp_dev_t *isp_dev)
{
	mcnr_mv_wrmif_init(isp_dev);
	mcnr_meta_wrmif_init(isp_dev);
	mcnr_mv_rdmif_init(isp_dev);
	mcnr_meta_rdmif_init(isp_dev);
}
