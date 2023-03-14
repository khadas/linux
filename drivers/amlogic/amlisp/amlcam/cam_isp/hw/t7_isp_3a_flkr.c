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

static void post_cfg_dmawr_size(struct isp_dev_t *isp_dev)
{
	u32 dma_size = 0;

	dma_size = (LTM_STA_LEN_H - 1) * (LTM_STA_LEN_V - 1) * 4 + 64 ;

	isp_reg_update_bits(isp_dev, VIU_DMAWR_SIZE2, dma_size, 0, 16);
}

static void flkr_cfg_dmawr_size(struct isp_dev_t *isp_dev)
{
	u32 raw_mode;
	u32 input_fmt;
	u32 dma_size, cond;
	u32 flkr_binning_rs;
	u32 flkr_stat_yst, flkr_stat_yed;
	u32 flkr_stat_xst, flkr_stat_xed;
	u32 flkr_avg0, flkr_avg1, flkr_avg2, flkr_avg3, flkr_avg4;

	isp_reg_read_bits(isp_dev, ISP_TOP_MODE_CTRL, &raw_mode, 0, 3);

	isp_reg_read_bits(isp_dev, ISP_DEFLICKER_CNTL, &flkr_binning_rs, 2, 3);
	isp_reg_read_bits(isp_dev, ISP_DEFLICKER_CNTL, &input_fmt, 12, 1);

	isp_reg_read_bits(isp_dev, ISP_DEFLICKER_CNTL, &flkr_avg0, 20, 1);
	isp_reg_read_bits(isp_dev, ISP_DEFLICKER_CNTL, &flkr_avg1, 19, 1);
	isp_reg_read_bits(isp_dev, ISP_DEFLICKER_CNTL, &flkr_avg2, 18, 1);
	isp_reg_read_bits(isp_dev, ISP_DEFLICKER_CNTL, &flkr_avg3, 17, 1);
	isp_reg_read_bits(isp_dev, ISP_DEFLICKER_CNTL, &flkr_avg4, 16, 1);

	isp_reg_read_bits(isp_dev, ISP_DEFLICKER_STAT_YPOSITION, &flkr_stat_yed, 0, 14);
	isp_reg_read_bits(isp_dev, ISP_DEFLICKER_STAT_YPOSITION, &flkr_stat_yst, 16, 14);
	isp_reg_read_bits(isp_dev, ISP_DEFLICKER_STAT_XPOSITION, &flkr_stat_xed, 0, 14);
	isp_reg_read_bits(isp_dev, ISP_DEFLICKER_STAT_XPOSITION, &flkr_stat_xst, 16, 14);

	cond = (input_fmt == 0 &&
		(raw_mode == 2 ||raw_mode==3) &&
		(flkr_avg0 == 1 && flkr_avg1 == 0 &&
			flkr_avg2 == 0 && flkr_avg3 == 1 &&
			flkr_avg4 == 0)
		) ? 1 : 0;

	dma_size = (((flkr_stat_yed - flkr_stat_yst) >> (flkr_binning_rs + cond)) + 9) /10;

	isp_reg_update_bits(isp_dev, VIU_DMAWR_SIZE1, dma_size, 16, 16);
}

static void flkr_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;

	val = (0 << 16) | (fmt->width & 0xffff);
	isp_reg_write(isp_dev, ISP_DEFLICKER_STAT_XPOSITION, val);

	if (fmt->height >= 1280)
		val = (((fmt->height - 1280) / 2) << 16) |
			(((fmt->height - 1280) / 2 + 1280 - 1) & 0xffff);
	else
		val = (0 << 16) | ((fmt->height - 1) & 0xffff);

	isp_reg_write(isp_dev, ISP_DEFLICKER_STAT_YPOSITION, val);

	val = MIN(((1 << 16) - 1), ((1 << 22) / fmt->width));
	isp_reg_write(isp_dev, ISP_DEFLICKER_DIV_COEF, val);
}

static void flkr_req_info(struct isp_dev_t *isp_dev, void *info)
{
	u32 val = 0;
	flkr_req_info_t *f_info = info;

	val = isp_reg_read(isp_dev, ISP_DEFLICKER_CNTL);
	f_info->flkr_binning_rs = (val >> 2) & 0x7;
	f_info->flkr_ro_mode = (val >> 0) & 0x1;

	val = isp_reg_read(isp_dev, ISP_TOP_FED_CTRL);
	f_info->flkr_det_en = (val >> 4) & 0x1;
	f_info->flkr_sta_pos = (val >> 17) & 0x3;
}

static void flkr_init(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_WDR_RATIO, 1, 0, 15);

	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_CNTL, 1, 20, 1);
	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_CNTL, 0, 19, 1);
	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_CNTL, 0, 18, 1);
	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_CNTL, 1, 17, 1);
	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_CNTL, 0, 16, 1);

	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_CNTL, 1, 12, 1);
	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_CNTL, 0, 8, 2);
	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_CNTL, 0, 2, 3);
	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_CNTL, 1, 0, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, 1, 4, 1);
}

static void af_req_info(struct isp_dev_t *isp_dev, void *info)
{
	u32 val = 0;
	af_req_info_t *af_info = info;

	val = isp_hwreg_read(isp_dev, ISP_AF_HV_BLKNUM);
	af_info->af_stat_hblk_num = (val >> 16) & 0x3f;
	af_info->af_stat_vblk_num = val & 0x3f;

	val = isp_hwreg_read(isp_dev, ISP_AF_EN_CTRL);
	af_info->af_stat_sel = (val >> 21) & 0x1;

	af_info->af_glb_stat_pack0 = isp_hwreg_read(isp_dev, ISP_RO_AF_GLB_STAT_PCK0);
	af_info->af_glb_stat_pack1 = isp_hwreg_read(isp_dev, ISP_RO_AF_GLB_STAT_PCK1);
	af_info->af_glb_stat_pack2 = isp_hwreg_read(isp_dev, ISP_RO_AF_GLB_STAT_PCK2);
	af_info->af_glb_stat_pack3 = isp_hwreg_read(isp_dev, ISP_RO_AF_GLB_STAT_PCK3);
}

static void af_cfg_dmawr_size(struct isp_dev_t *isp_dev)
{
	u32 dma_size = 0;
	u32 val, af_stat_sel;

	val = isp_reg_read(isp_dev, ISP_AF_EN_CTRL);
	af_stat_sel = (val >> 21) & 0x1;

	dma_size = AF_STAT_BLKH_NUM * AF_STAT_BLKV_NUM;

	if (af_stat_sel != 0)
	{
	    dma_size = (dma_size + 1) / 2;
	}

	isp_reg_update_bits(isp_dev, VIU_DMAWR_SIZE0, dma_size, 0, 16);
}

static void af_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 k, val = 0;
	u32 stat_hidx[AF_STAT_BLKH_NUM + 1];
	u32 stat_vidx[AF_STAT_BLKH_NUM + 1]; // keke.li
	u32 xsize = fmt->width;
	u32 ysize = fmt->height;

	isp_reg_update_bits(isp_dev, ISP_AF_HV_BLKNUM, AF_STAT_BLKH_NUM, 16, 6);
	isp_reg_update_bits(isp_dev, ISP_AF_HV_BLKNUM, AF_STAT_BLKV_NUM, 0, 6);
	val = (0x0 << 16) | (0x0 << 0);
	isp_reg_write(isp_dev, ISP_AF_HV_START, val);
	val = (xsize << 16) |(ysize << 0);
	isp_reg_write(isp_dev, ISP_AF_HV_SIZE, val);

	for (k = 0; k <= AF_STAT_BLKH_NUM; k++) {
		val = k * xsize / AF_STAT_BLKH_NUM;
		stat_hidx[k] = MIN(MAX(val, 0), xsize);
	}

	for (k = 0; k <= AF_STAT_BLKV_NUM; k++) {
		val = k * ysize / AF_STAT_BLKV_NUM;
		stat_vidx[k] = MIN(MAX(val, 0), ysize);
	}

	isp_reg_write(isp_dev, ISP_AF_IDX_ADDR, 0);
	for (k = 0; k < AF_STAT_BLKH_NUM + 1; k++) {
		val = ((stat_hidx[k] & 0xffff) << 16) | (stat_vidx[k] & 0xffff);
		isp_reg_write(isp_dev, ISP_AF_IDX_DATA, val);
	}
}

static void af_init(struct isp_dev_t *isp_dev)
{
	u32 val = 0;
	u32 af_chn_valid[5] = {1, 1, 1, 1, 1}; //CH_GR, CH_RED, CH_BLU, CH_GB, CH_Ir
	u32 af_roi_xywin[2][4] = {{50, 50, 80,40}, {100, 50, 80, 40}};
	u32 yuvlimit[6] = {0, 4095, 0, 4095, 0, 4095};

	val = ((yuvlimit[1] & 0xfff) << 16) |(yuvlimit[0] & 0xfff);
	isp_reg_write(isp_dev, ISP_AF_ROI01, val);

	val = ((yuvlimit[3] & 0xfff) << 16) |(yuvlimit[2] & 0xfff);
	isp_reg_write(isp_dev, ISP_AF_ROI23, val);

	val = ((yuvlimit[5] & 0xfff) << 16) |(yuvlimit[4] & 0xfff);
	isp_reg_write(isp_dev, ISP_AF_ROI45, val);

	val = ((af_roi_xywin[0][1] & 0xffff) << 16) |(af_roi_xywin[0][0] & 0xffff);
	isp_reg_write(isp_dev, ISP_AF_ROI0_WIN01, val);

	val = ((af_roi_xywin[0][3] & 0xffff) << 16) |(af_roi_xywin[0][2] & 0xffff);
	isp_reg_write(isp_dev, ISP_AF_ROI0_WIN23, val);

	val = ((af_roi_xywin[1][1] & 0xffff) << 16) |(af_roi_xywin[1][0] & 0xffff);
	isp_reg_write(isp_dev, ISP_AF_ROI1_WIN01, val);

	val = ((af_roi_xywin[1][3] & 0xffff) << 16) |(af_roi_xywin[1][2] & 0xffff);
	isp_reg_write(isp_dev, ISP_AF_ROI1_WIN23, val);

	isp_reg_update_bits(isp_dev, ISP_AF_ROI_EN_0, 0x1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_AF_ROI_EN_1, 0x1, 0, 1);

	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, af_chn_valid[0] & 0x1, 13, 1);
	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, af_chn_valid[1] & 0x1, 12, 1);
	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, af_chn_valid[2] & 0x1, 11, 1);
	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, af_chn_valid[3] & 0x1, 10, 1);
	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, af_chn_valid[4] & 0x1, 9, 1);

	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, 0x0, 8, 1);
	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, 0x0, 6, 2);
	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, 0x0, 4, 2);
	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, 0x0, 2, 2);
	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, 0x0, 0, 2);
	isp_reg_update_bits(isp_dev, ISP_AF_EN_CTRL, 1, 21, 1);
}

static void ae_cfg_dmawr_size(struct isp_dev_t *isp_dev)
{
	u32 local_mode = 0;
	u32 dma_size = 0;

	isp_reg_read_bits(isp_dev, ISP_AE_CTRL, &local_mode, 2, 2);

	dma_size = local_mode & 0x2 ?
		(AE_STAT_BLKH_NUM * AE_STAT_BLKV_NUM * 3 + 1) /2 + (1024 * 32) /128 :
		(AE_STAT_BLKH_NUM * AE_STAT_BLKV_NUM + 1) /2 + (1024 * 32) /128;
	isp_reg_update_bits(isp_dev, VIU_DMAWR_SIZE1, dma_size, 0, 16);
}

static void ae_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 k, val = 0;
	u32 stat_hidx[AE_STAT_BLKH_NUM + 1];
	u32 stat_vidx[AE_STAT_BLKH_NUM + 1]; // keke.li
	u32 xsize = fmt->width;
	u32 ysize = fmt->height;

	val = (AE_STAT_BLKH_NUM << 16) |(AE_STAT_BLKV_NUM & 0xffff);
	isp_reg_write(isp_dev, ISP_AE_HV_BLKNUM, val);

	val = (0x0 << 16) |(0x0 & 0xffff);
	isp_reg_write(isp_dev, ISP_AE_HV_START, val);

	val = (xsize << 16) |(ysize & 0xffff);
	isp_reg_write(isp_dev, ISP_AE_HV_SIZE, val);

	for (k = 0; k <= AE_STAT_BLKH_NUM; k++) {
		val = k * xsize / (2 * AE_STAT_BLKH_NUM) * 2;
		stat_hidx[k] = MIN(val, xsize);
	}

	for (k = 0; k <= AE_STAT_BLKV_NUM; k++) {
		val = k * ysize / (2 * AE_STAT_BLKV_NUM) * 2;
		stat_vidx[k] = MIN(val, ysize);
	}

	isp_reg_write(isp_dev, ISP_AE_IDX_ADDR, 0);
	for (k = 0; k < AE_STAT_BLKH_NUM + 1; k++) {
		val = ((stat_hidx[k] & 0xffff) << 16) | (stat_vidx[k] & 0xffff);
		isp_reg_write(isp_dev, ISP_AE_IDX_DATA, val);
	}
}

static void ae_init(struct isp_dev_t *isp_dev)
{
	u32 i, val = 0;
	u32 ae_stat_thrd[4] = {13107, 13107*2, 13107*3, 13107*4}; // under u16 preciison
	u32 yuvlimit[6] = {0, 4095, 0, 4095, 0, 4095};
	u32 ae_roi_xywin[4][4] = {{50, 50, 80, 40}, {100, 50, 80, 40},
			{32, 62, 32, 42}, {32, 82, 64, 52}};

	val = ((yuvlimit[1] & 0xfff) << 16) |(yuvlimit[0] & 0xfff);
	isp_reg_write(isp_dev, ISP_AE_ROI01, val);

	val = ((yuvlimit[3] & 0xfff) << 16) |(yuvlimit[2] & 0xfff);
	isp_reg_write(isp_dev, ISP_AE_ROI23, val);

	val = ((yuvlimit[5] & 0xfff) << 16) |(yuvlimit[4] & 0xfff);
	isp_reg_write(isp_dev, ISP_AE_ROI45, val);

	val = ((ae_roi_xywin[0][1] & 0xffff) << 16) |(ae_roi_xywin[0][0] & 0xffff);
	isp_reg_write(isp_dev, ISP_AE_ROI0_WIN01, val);

	val = ((ae_roi_xywin[0][3] & 0xffff) << 16) |(ae_roi_xywin[0][2] & 0xffff);
	isp_reg_write(isp_dev, ISP_AE_ROI0_WIN23, val);

	val = ((ae_roi_xywin[1][1] & 0xffff) << 16) |(ae_roi_xywin[1][0] & 0xffff);
	isp_reg_write(isp_dev, ISP_AE_ROI1_WIN01, val);

	val = ((ae_roi_xywin[1][3] & 0xffff) << 16) |(ae_roi_xywin[1][2] & 0xffff);
	isp_reg_write(isp_dev, ISP_AE_ROI1_WIN23, val);

	isp_reg_update_bits(isp_dev, ISP_AE_ROI_EN_0, 0x1, 1, 1);
	isp_reg_update_bits(isp_dev, ISP_AE_ROI_EN_1, 0x1, 1, 1);
	isp_reg_update_bits(isp_dev, ISP_AE_ROI_EN_0, 0x0, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_AE_ROI_EN_1, 0x0, 0, 1);

	val = 61440 & 0xffff;
	isp_reg_update_bits(isp_dev, ISP_AE_SATUR_THRD, val, 0, 16);

	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x0, 20, 3);
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x1, 16, 3);
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x0, 12, 4);
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x0, 10, 2);
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x2, 8, 2);
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x1, 7, 1);
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x1, 5, 1);
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x1, 4, 1);
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x0, 2, 2); //local_mode
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x0, 1, 1);
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x0, 0, 1);

	isp_reg_read_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, &val, 8, 2); //keke.li
	if (val < 1)
		isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x1, 7, 1);
	else
		isp_reg_update_bits(isp_dev, ISP_AE_CTRL, 0x0, 7, 1);

	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_0, 76, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_1, 76, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_2, 76, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_3, 28, 16, 8);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_4, 64, 16, 8);

	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_0, 1, 24, 1);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_1, 1, 24, 1);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_2, 1, 24, 1);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_3, 1, 24, 1);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_4, 1, 24, 1);

	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_0, 0, 0, 12);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_1, 0, 0, 12);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_2, 0, 0, 12);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_3, 0, 0, 12);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_4, 0, 0, 12);

	val = ((ae_stat_thrd[1] & 0xffff) << 16) |(ae_stat_thrd[0] & 0xffff);
	isp_reg_write(isp_dev, ISP_AE_STAT_THD01, val);

	val = ((ae_stat_thrd[3] & 0xffff) << 16) |(ae_stat_thrd[2] & 0xffff);
	isp_reg_write(isp_dev, ISP_AE_STAT_THD23, val);

	val = 0x11111111;
	isp_reg_write(isp_dev, ISP_AE_BLK_WT_ADDR, 0x0);
	for (i = 0; i < 512; i++)
		isp_reg_write(isp_dev, ISP_AE_BLK_WT_DATA, val);
}

static void ae_cfg_grbgi(struct isp_dev_t *isp_dev, void *change)
{
	u32 val = 0;
	aisp_wb_change_cfg_t *wb_chg = change;

	val = (wb_chg->ae_gain_grbgi[0] << 16) | (wb_chg->ae_bl12_grbgi[0] << 0);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_0, val, 0, 24);

	val = (wb_chg->ae_gain_grbgi[1] << 16) | (wb_chg->ae_bl12_grbgi[1] << 0);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_1, val, 0, 24);

	val = (wb_chg->ae_gain_grbgi[2] << 16) | (wb_chg->ae_bl12_grbgi[2] << 0);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_2, val, 0, 24);

	val = (wb_chg->ae_gain_grbgi[3] << 16) | (wb_chg->ae_bl12_grbgi[3] << 0);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_3, val, 0, 24);

	val = (wb_chg->ae_gain_grbgi[4] << 16) | (wb_chg->ae_bl12_grbgi[4] << 0);
	isp_reg_update_bits(isp_dev, ISP_AE_CRTL2_4, val, 0, 24);
}

static void ae_cfg_stat_blk_weight(struct isp_dev_t *isp_dev, void *mode)
{
	int i = 0;
	int cnt = 0;
	u32 val = 0;
	aisp_expo_mode_cfg_t *expo = mode;

	cnt = sizeof(expo->ae_stat_blk_weight) / (8 * sizeof(u8));

	isp_hw_lut_wstart(isp_dev, AE_WEIGHT_LUT_CFG);

	isp_reg_write(isp_dev, ISP_AE_BLK_WT_ADDR, 0);
	for (i = 0; i < cnt; i++) {
		val = ((expo->ae_stat_blk_weight[i * 8 + 0]&0xf) << 0);
		val |= ((expo->ae_stat_blk_weight[i * 8 + 1]&0xf) << 4);
		val |= ((expo->ae_stat_blk_weight[i * 8 + 2]&0xf) << 8);
		val |= ((expo->ae_stat_blk_weight[i * 8 + 3]&0xf) << 12);
		val |= ((expo->ae_stat_blk_weight[i * 8 + 4]&0xf) << 16);
		val |= ((expo->ae_stat_blk_weight[i * 8 + 5]&0xf) << 20);
		val |= ((expo->ae_stat_blk_weight[i * 8 + 6]&0xf) << 24);
		val |= ((expo->ae_stat_blk_weight[i * 8 + 7]&0xf) << 28);

		isp_reg_write(isp_dev, ISP_AE_BLK_WT_DATA, val);
	}

	val = 0;
	for (i = 0; i < (sizeof(expo->ae_stat_blk_weight) / sizeof(u8))% 8; i++) {
		val |= (expo->ae_stat_blk_weight[cnt * 8 + i]&0xf) << (i * 4);
	}

	isp_reg_write(isp_dev, ISP_AE_BLK_WT_DATA, val);

	isp_hw_lut_wend(isp_dev);
}

static void ae_req_info(struct isp_dev_t *isp_dev, void *info)
{
	u32 val = 0;
	ae_req_info_t *ae_info = info;

	isp_reg_read_bits(isp_dev, ISP_TOP_BEO_CTRL, &ae_info->gtm_en, 2, 1);
	isp_reg_read_bits(isp_dev, ISP_GTM_GAIN, &ae_info->gtm_lut_mode, 0, 1);
	isp_reg_read_bits(isp_dev, ISP_PST_GAMMA_MODE, &ae_info->pst_gamma_mode, 0, 1);

	val = isp_reg_read(isp_dev, ISP_GTM_LUT_STP);
	ae_info->gtm_lut_stp[0] = (val >> 0) & 0xf;
	ae_info->gtm_lut_stp[1] = (val >> 4) & 0xf;
	ae_info->gtm_lut_stp[2] = (val >> 8) & 0xf;
	ae_info->gtm_lut_stp[3] = (val >> 12) & 0xf;
	ae_info->gtm_lut_stp[4] = (val >> 16) & 0xf;
	ae_info->gtm_lut_stp[5] = (val >> 20) & 0xf;
	ae_info->gtm_lut_stp[6] = (val >> 24) & 0xf;
	ae_info->gtm_lut_stp[7] = (val >> 28) & 0xf;

	val = isp_reg_read(isp_dev, ISP_GTM_LUT_NUM);
	ae_info->gtm_lut_num[0] = (val >> 0) & 0x7;
	ae_info->gtm_lut_num[1] = (val >> 4) & 0x7;
	ae_info->gtm_lut_num[2] = (val >> 8) & 0x7;
	ae_info->gtm_lut_num[3] = (val >> 12) & 0x7;
	ae_info->gtm_lut_num[4] = (val >> 16) & 0x7;
	ae_info->gtm_lut_num[5] = (val >> 20) & 0x7;
	ae_info->gtm_lut_num[6] = (val >> 24) & 0x7;
	ae_info->gtm_lut_num[7] = (val >> 28) & 0x7;

	isp_reg_read_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, &ae_info->ae_stat_switch, 8, 2);
	isp_reg_read_bits(isp_dev, ISP_AE_CTRL, &ae_info->ae_stat_local_mode, 2, 2);
	isp_reg_read_bits(isp_dev, ISP_AE_HV_BLKNUM, &ae_info->ae_stat_hblk_num, 16, 7);
	isp_reg_read_bits(isp_dev, ISP_AE_HV_BLKNUM, &ae_info->ae_stat_vblk_num, 0, 16);

	ae_info->ae_stat_glbpixnum = isp_hwreg_read(isp_dev, ISP_RO_AE_STAT_GPNUM);

	ae_info->roi0_pack0 = isp_hwreg_read(isp_dev, ISP_RO_AE_ROI_STAT_PCK0_0);
	ae_info->roi0_pack1 = isp_hwreg_read(isp_dev, ISP_RO_AE_ROI_STAT_PCK0_1);
	ae_info->roi1_pack0 = isp_hwreg_read(isp_dev, ISP_RO_AE_ROI_STAT_PCK1_0);
	ae_info->roi0_pack1 = isp_hwreg_read(isp_dev, ISP_RO_AE_ROI_STAT_PCK1_1);
}

static void awb_cfg_dmawr_size(struct isp_dev_t *isp_dev)
{
	u32 div_mode = 0;
	u32 dma_size = 0;

	isp_reg_read_bits(isp_dev, ISP_AWB_STAT_CTRL2, &div_mode, 3, 2);

	dma_size = (AWB_STAT_BLKH_NUM * AWB_STAT_BLKV_NUM * (div_mode + 1) + 1) /2;
	isp_reg_update_bits(isp_dev, VIU_DMAWR_SIZE0, dma_size, 16, 16);
}

static void awb_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 k, val = 0;
	u32 stat_hidx[AWB_STAT_BLKH_NUM + 1];
	u32 stat_vidx[AWB_STAT_BLKH_NUM + 1]; // keke.li
	u32 xsize = fmt->width;
	u32 ysize = fmt->height;

	val = (AWB_STAT_BLKH_NUM << 16) |(AWB_STAT_BLKV_NUM & 0xffff);
	isp_reg_write(isp_dev, ISP_AWB_HV_BLKNUM, val);

	val = (0x0 << 16) |(0x0 & 0xffff);
	isp_reg_write(isp_dev, ISP_AWB_HV_START, val);

	val = (xsize << 16) |(ysize & 0xffff);
	isp_reg_write(isp_dev, ISP_AWB_HV_SIZE, val);


	for (k = 0; k <= AWB_STAT_BLKH_NUM; k++) {
		val = k * xsize / (2 * AWB_STAT_BLKH_NUM) * 2;
		stat_hidx[k] = MIN(val, xsize);
	}
	for (k = 0; k <= AWB_STAT_BLKV_NUM; k++) {
		val = k * ysize / (2 * AWB_STAT_BLKV_NUM) * 2;
		stat_vidx[k] = MIN(val, ysize);
	}

	isp_reg_write(isp_dev, ISP_AWB_IDX_ADDR, 0);
	for (k = 0; k < AWB_STAT_BLKH_NUM + 1; k++) {
		val = ((stat_hidx[k] & 0xffff) << 16) |(stat_vidx[k] & 0xffff);
		isp_reg_write(isp_dev, ISP_AWB_IDX_DATA, val);
	}
}

static void awb_init(struct isp_dev_t *isp_dev)
{
	u32 i, val = 0;
	u32 luma_divth[3] = {1000, 2000, 3000};
	u32 yuvlimit[6] = {0, 4095, 0, 4095, 0, 4095};
	u32 awb_roi_xywin[2][4] = {{32, 62, 32, 42}, {32, 82, 64, 52}}; // [h_start, h_size, v_start, v_size]

	val = ((yuvlimit[1] & 0xfff) << 16) |(yuvlimit[0] & 0xfff);
	isp_reg_write(isp_dev, ISP_AWB_STAT_ROI01, val);

	val = ((yuvlimit[3] & 0xfff) << 16) |(yuvlimit[2] & 0xfff);
	isp_reg_write(isp_dev, ISP_AWB_STAT_ROI23, val);

	val = ((yuvlimit[5] & 0xfff) << 16) |(yuvlimit[4] & 0xfff);
	isp_reg_write(isp_dev, ISP_AWB_STAT_ROI45, val);


	val = ((awb_roi_xywin[0][1] & 0xffff) << 16) |(awb_roi_xywin[0][0] & 0xffff);
	isp_reg_write(isp_dev, ISP_AWB_STAT_ROI0_WIN01, val);

	val = ((awb_roi_xywin[0][3] & 0xffff) << 16) |(awb_roi_xywin[0][2] & 0xffff);
	isp_reg_write(isp_dev, ISP_AWB_STAT_ROI0_WIN23, val);

	val = ((awb_roi_xywin[1][1] & 0xffff) << 16) |(awb_roi_xywin[1][0] & 0xffff);
	isp_reg_write(isp_dev, ISP_AWB_STAT_ROI1_WIN01, val);

	val = ((awb_roi_xywin[1][3] & 0xffff) << 16) |(awb_roi_xywin[1][2] & 0xffff);
	isp_reg_write(isp_dev, ISP_AWB_STAT_ROI1_WIN23, val);

	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_ROI_EN_0, 0x1, 1, 1);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_ROI_EN_1, 0x1, 1, 1);

	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_ROI_EN_0, 0x0, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_ROI_EN_1, 0x0, 0, 1);

	isp_reg_update_bits(isp_dev, ISP_AWB_CTRL, 0x1, 4, 1);

	val = 0xffffffff;
	isp_reg_write(isp_dev, ISP_AWB_BLK_WT_ADDR, 0x0);
	for (i = 0; i < 128; i++)
		isp_reg_write(isp_dev, ISP_AWB_BLK_WT_DATA, val);

	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_THD, 4095, 16, 12);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_THD, 0, 0, 12);

	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_RG, 2048, 16, 12);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_RG, 64, 0, 12);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_BG, 2048, 16, 12);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_BG, 64, 0, 12);

	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_RG_HL, 4095, 16, 12);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_RG_HL, 0, 0, 12);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_BG_HL, 4095, 16, 12);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_BG_HL, 0, 0, 12);

	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_CTRL2, 2, 11, 2);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_CTRL2, 0, 8, 1);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_CTRL2, 0, 6, 1);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_CTRL2, 1, 5, 1);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_CTRL2, 0, 3, 2); //div_mode
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_CTRL2, 0, 2, 1);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_CTRL2, 0, 1, 1);

	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_BLC20_0, 0, 0, 20);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_BLC20_1, 0, 0, 20);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_BLC20_2, 0, 0, 20);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_BLC20_3, 0, 0, 20);

	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_GAIN10_0, 256, 0, 10);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_GAIN10_1, 256, 0, 10);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_GAIN10_2, 256, 0, 10);
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_GAIN10_3, 256, 0, 10);

	val = ((luma_divth[1] & 0xfff) << 16) |(luma_divth[0] & 0xfff);
	isp_reg_write(isp_dev, ISP_AWB_STAT_LDIV_01, val);

	val = luma_divth[2] & 0xfff;
	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_LDIV_2, val, 0, 12);
}

static void awb_cfg_stat_blc20(struct isp_dev_t *isp_dev, void *luma)
{
	aisp_wb_luma_cfg_t *wb_luma = luma;

	isp_reg_write(isp_dev, ISP_AWB_STAT_BLC20_0, wb_luma->awb_stat_blc20[0]);
	isp_reg_write(isp_dev, ISP_AWB_STAT_BLC20_1, wb_luma->awb_stat_blc20[1]);
	isp_reg_write(isp_dev, ISP_AWB_STAT_BLC20_2, wb_luma->awb_stat_blc20[2]);
	isp_reg_write(isp_dev, ISP_AWB_STAT_BLC20_3, wb_luma->awb_stat_blc20[3]);
}

static void awb_cfg_stat_gain10(struct isp_dev_t *isp_dev, void *luma)
{
	aisp_wb_luma_cfg_t *wb_luma = luma;

	isp_reg_write(isp_dev, ISP_AWB_STAT_GAIN10_0, wb_luma->awb_stat_gain10[0]);
	isp_reg_write(isp_dev, ISP_AWB_STAT_GAIN10_1, wb_luma->awb_stat_gain10[1]);
	isp_reg_write(isp_dev, ISP_AWB_STAT_GAIN10_2, wb_luma->awb_stat_gain10[2]);
	isp_reg_write(isp_dev, ISP_AWB_STAT_GAIN10_3, wb_luma->awb_stat_gain10[3]);
}

static void awb_cfg_stat_satur(struct isp_dev_t *isp_dev, void *luma)
{
	u32 val = 0;
	aisp_wb_luma_cfg_t *wb_luma = luma;

	val = (wb_luma->awb_stat_satur_low << 0) | (wb_luma->awb_stat_satur_high << 16);
	isp_reg_write(isp_dev, ISP_AWB_STAT_SATUR_CTRL, val);
}

static void awb_cfg_triangle(struct isp_dev_t *isp_dev, void *triangle)
{
	u32 val = 0;
	aisp_wb_triangle_cfg_t *wb_triangle = triangle;

	isp_reg_update_bits(isp_dev, ISP_AWB_STAT_CTRL2, wb_triangle->awb_stat_satur_vald, 0, 1);

	val = (wb_triangle->awb_stat_rg_min << 0) |  (wb_triangle->awb_stat_rg_max << 16);
	isp_reg_write(isp_dev, ISP_AWB_STAT_RG, val);

	val = (wb_triangle->awb_stat_bg_min << 0) |  (wb_triangle->awb_stat_bg_max << 16);
	isp_reg_write(isp_dev, ISP_AWB_STAT_BG, val);

	val = (wb_triangle->awb_stat_rg_low << 0) |  (wb_triangle->awb_stat_rg_high << 16);
	isp_reg_write(isp_dev, ISP_AWB_STAT_RG_HL, val);

	val = (wb_triangle->awb_stat_bg_low << 0) |  (wb_triangle->awb_stat_bg_high << 16);
	isp_reg_write(isp_dev, ISP_AWB_STAT_BG_HL, val);
}

static void awb_cfg_stat_blk_weight(struct isp_dev_t *isp_dev, void *mode)
{
	int i = 0;
	int cnt = 0;
	u32 val = 0;
	aisp_awb_weight_cfg_t *awb = mode;

	cnt = sizeof(awb->awb_stat_blk_weight) / (8 * sizeof(u32));

	isp_hw_lut_wstart(isp_dev, AWB_WEIGHT_LUT_CFG);

	isp_reg_write(isp_dev, ISP_AWB_BLK_WT_ADDR, 0);
	for (i = 0; i < cnt; i++) {
		val = ((awb->awb_stat_blk_weight[i * 8 + 0]&0xf) << 0);
		val |= ((awb->awb_stat_blk_weight[i * 8 + 1]&0xf) << 4);
		val |= ((awb->awb_stat_blk_weight[i * 8 + 2]&0xf) << 8);
		val |= ((awb->awb_stat_blk_weight[i * 8 + 3]&0xf) << 12);
		val |= ((awb->awb_stat_blk_weight[i * 8 + 4]&0xf) << 16);
		val |= ((awb->awb_stat_blk_weight[i * 8 + 5]&0xf) << 20);
		val |= ((awb->awb_stat_blk_weight[i * 8 + 6]&0xf) << 24);
		val |= ((awb->awb_stat_blk_weight[i * 8 + 7]&0xf) << 28);

		isp_reg_write(isp_dev, ISP_AWB_BLK_WT_DATA, val);
	}

	isp_hw_lut_wend(isp_dev);
}

static void awb_req_info(struct isp_dev_t *isp_dev, void *info)
{
	u32 val = 0;
	awb_req_info_t *awb_info = info;

	val = isp_reg_read(isp_dev, ISP_AWB_HV_BLKNUM);
	awb_info->awb_stat_hblk_num = (val >> 16) & 0x3f;
	awb_info->awb_stat_vblk_num = val & 0x3f;

	val = isp_reg_read(isp_dev, ISP_AWB_STAT_CTRL2);
	awb_info->awb_stat_luma_div_mode = (val >> 3) & 0x3;
	awb_info->awb_stat_local_mode = (val >> 2) & 0x1;

	isp_reg_read_bits(isp_dev, ISP_AWB_CTRL, &awb_info->awb_stat_ratio_mode, 4, 1);

	val = isp_reg_read(isp_dev, ISP_LSWB_BLC_OFST0);
	awb_info->blc_ofst[0] = (val >> 16) & 0xffff;
	awb_info->blc_ofst[1] = val & 0xffff;

	val = isp_reg_read(isp_dev, ISP_LSWB_BLC_OFST1);
	awb_info->blc_ofst[2] = (val >> 16) & 0xffff;
	awb_info->blc_ofst[3] = val & 0xffff;

	val = isp_reg_read(isp_dev, ISP_LSWB_BLC_OFST2);
	awb_info->blc_ofst[4] = val & 0xffff;

	isp_reg_read_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, &awb_info->awb_stat_switch, 4, 3);
}

void isp_3a_flkr_enable(struct isp_dev_t *isp_dev)
{
	if ((isp_dev->fmt.code == MEDIA_BUS_FMT_YVYU8_2X8) || (isp_dev->fmt.code == MEDIA_BUS_FMT_YUYV8_2X8))
		return;

	isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, 0x7, 0, 3);
	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, 0x1, 3, 1);
	isp_reg_update_bits(isp_dev, ISP_POST_STA_GLB_MISC, 1, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_LC_HIST_BLK_MISC, 1, 6, 1);
}

void isp_3a_flkr_disable(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_TOP_3A_STAT_CRTL, 0x0, 0, 3);
	isp_reg_update_bits(isp_dev, ISP_TOP_FED_CTRL, 0x0, 3, 1);
	isp_reg_update_bits(isp_dev, ISP_POST_STA_GLB_MISC, 0, 0, 1);
	isp_reg_update_bits(isp_dev, ISP_LC_HIST_BLK_MISC, 0, 6, 1);
}

void isp_3a_flkr_cfg_dmawr_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	af_cfg_dmawr_size(isp_dev);
	awb_cfg_dmawr_size(isp_dev);
	ae_cfg_dmawr_size(isp_dev);
	flkr_cfg_dmawr_size(isp_dev);
	post_cfg_dmawr_size(isp_dev);
}

void isp_3a_flkr_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_stats_info_t *stats = buff->vaddr[AML_PLANE_A];
	frame_info_t *frm_info = &stats->frame_info;

	frm_info->frm_cnt = isp_dev->frm_cnt;

	if (isp_dev->slice) {
		frm_info->slice_num = 2;
		isp_disp_get_overlap(isp_dev, &frm_info->slice_ovlp);
	} else {
		frm_info->slice_num = 1;
		frm_info->slice_ovlp = 0;
	}

	awb_req_info(isp_dev, &stats->module_info.awb_req_info);
	ae_req_info(isp_dev, &stats->module_info.ae_req_info);
	af_req_info(isp_dev, &stats->module_info.af_req_info);
	flkr_req_info(isp_dev, &stats->module_info.flkr_req_info);
}

void isp_3a_flkr_cfg_stat_buff(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	u32 base_size;
	u32 af_dma_size, awb_dma_size;
	u32 ae_dma_size, flkr_dma_size;
	u32 base_addr, af_dma_addr, awb_dma_addr;
	u32 ae_dma_addr, flkr_dma_addr, post_dma_addr;
	aisp_stats_info_t *stats = buff->vaddr[AML_PLANE_A];
	struct isp_global_info *g_info = isp_global_get_info();

	base_size = sizeof(stats->frame_info);
	awb_dma_size = sizeof(stats->wb_stats);
	ae_dma_size = sizeof(stats->exps_stats);
	af_dma_size = sizeof(stats->af_stats);
	flkr_dma_size = sizeof(stats->deflkr_stats);

	base_addr = buff->addr[AML_PLANE_A];
	awb_dma_addr = base_addr + base_size;
	ae_dma_addr = awb_dma_addr + awb_dma_size;
	af_dma_addr = ae_dma_addr + ae_dma_size;
	flkr_dma_addr = af_dma_addr + af_dma_size;
	post_dma_addr = flkr_dma_addr + flkr_dma_size;

	if (g_info->mode == AML_ISP_SCAM) {
		isp_hwreg_write(isp_dev, VIU_DMAWR_BADDR0, af_dma_addr >> 4);
		isp_hwreg_write(isp_dev, VIU_DMAWR_BADDR1, awb_dma_addr >> 4);
		isp_hwreg_write(isp_dev, VIU_DMAWR_BADDR2, ae_dma_addr >> 4);
		isp_hwreg_write(isp_dev, VIU_DMAWR_BADDR3, flkr_dma_addr >> 4);
		isp_hwreg_write(isp_dev, VIU_DMAWR_BADDR4, post_dma_addr >> 4);
	} else {
		isp_reg_write(isp_dev, VIU_DMAWR_BADDR0, af_dma_addr >> 4);
		isp_reg_write(isp_dev, VIU_DMAWR_BADDR1, awb_dma_addr >> 4);
		isp_reg_write(isp_dev, VIU_DMAWR_BADDR2, ae_dma_addr >> 4);
		isp_reg_write(isp_dev, VIU_DMAWR_BADDR3, flkr_dma_addr >> 4);
		isp_reg_write(isp_dev, VIU_DMAWR_BADDR4, post_dma_addr >> 4);
	}
}

void isp_3a_flkr_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
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

	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_OFST, xofst, 4, 2);
	isp_reg_update_bits(isp_dev, ISP_DEFLICKER_OFST, yofst, 0, 2);

	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, xofst, 16, 2);
	isp_reg_update_bits(isp_dev, ISP_AF_CTRL, yofst, 14, 2);

	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, xofst, 26, 2);
	isp_reg_update_bits(isp_dev, ISP_AE_CTRL, yofst, 24, 2);

	isp_reg_update_bits(isp_dev, ISP_AWB_CTRL, xofst, 2, 2);
	isp_reg_update_bits(isp_dev, ISP_AWB_CTRL, yofst, 0, 2);
}

void isp_3a_flkr_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	af_cfg_size(isp_dev, fmt);
	ae_cfg_size(isp_dev, fmt);
	awb_cfg_size(isp_dev, fmt);
	flkr_cfg_size(isp_dev, fmt);
}

void isp_3a_flkr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_wb_change)
		ae_cfg_grbgi(isp_dev, &param->wb_change);

	if (param->pvalid.aisp_wb_luma) {
		awb_cfg_stat_blc20(isp_dev, &param->wb_luma);
		awb_cfg_stat_gain10(isp_dev, &param->wb_luma);
		awb_cfg_stat_satur(isp_dev, &param->wb_luma);
	}

	if (param->pvalid.aisp_wb_triangle)
		awb_cfg_triangle(isp_dev, &param->wb_triangle);

	if (param->pvalid.aisp_expo_mode)
		ae_cfg_stat_blk_weight(isp_dev, &param->expo_mode);

	//if (param->pvalid.aisp_base)
		//ae_cfg_stat_blk_weight(isp_dev, param->base_cfg.fxlut_cfg.ae_stat_blk_weight);

	if (param->pvalid.aisp_base)
		awb_cfg_stat_blk_weight(isp_dev, param->base_cfg.fxlut_cfg.awb_stat_blk_weight);
}

void isp_3a_flkr_init(struct isp_dev_t *isp_dev)
{
	af_init(isp_dev);
	ae_init(isp_dev);
	awb_init(isp_dev);
	flkr_init(isp_dev);

	isp_3a_flkr_disable(isp_dev);
}
