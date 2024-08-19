// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>

#include "rkx11x_reg.h"
#include "rkx11x_vicap.h"

/* vicap base */
#define VICAP_BASE		RKX11X_VICAP_BASE

/* vicap register */
#define DVP_CTRL		0x0010
#define CAP_EN(x)		UPDATE(x, 0, 0)

#define DVP_INTEN		0x0014
#define FRAME_END_INTEN(x)	UPDATE(x, 8, 8)
#define FRAME_START_INTEN(x)	UPDATE(x, 0, 0)

#define DVP_INTSTAT		0x0018
#define FRAME_END_INTST		BIT(8)
#define FRAME_START_INTST	BIT(0)

#define DVP_FORMAT		0x001c
#define RAW_WIDTH(x)		UPDATE(x, 8, 7)
#define YUV_IN_ORDER(x)		UPDATE(x, 6, 5)
#define INPUT_MODE(x)		UPDATE(x, 4, 2)
#define HREF_POL(x)		UPDATE(x, 1, 1)
#define VSYNC_POL(x)		UPDATE(x, 0, 0)

#define DVP_PIX_NUM_ID0		0x0084
#define DVP_LINE_NUM_ID0	0x0088

#define MIPI_ID0_CTRL0		0x0100
#define MIPI_ID1_CTRL0		0x0108
#define MIPI_ID2_CTRL0		0x0110
#define MIPI_ID3_CTRL0		0x0118
#define ID_DT(x)		UPDATE(x, 15, 10)
#define ID_VC(x)		UPDATE(x, 9, 8)
#define ID_PARSE_TYPE(x)	UPDATE(x, 3, 1)
#define ID_CAP_EN(x)		UPDATE(x, 0, 0)

#define MIPI_LVDS_ID0_CTRL1	0x0104
#define MIPI_LVDS_ID1_CTRL1	0x010c
#define MIPI_LVDS_ID2_CTRL1	0x0114
#define MIPI_LVDS_ID3_CTRL1	0x011c
#define ID_HEIGHT(x)		UPDATE(x, 29, 16)
#define ID_WIDTH(x)		UPDATE(x, 13, 0)

#define MIPI_LVDS_CTRL		0x0120
#define LVDS_ALIGN_DIS(x)	UPDATE(x, 31, 31)
#define MIPI_LVDS_SEL(x)	UPDATE(x, 3, 3)
#define ALL_ID_CAP_EN(x)	UPDATE(x, 0, 0)

#define MIPI_LVDS_INTEN		0x0174
#define FIFO_OVERFLOW_EN(x)	UPDATE(x, 19, 19)
#define FRAME_END_ID3_EN(x)	UPDATE(x, 14, 14)
#define FRAME_END_ID2_EN(x)	UPDATE(x, 12, 12)
#define FRAME_END_ID1_EN(x)	UPDATE(x, 10, 10)
#define FRAME_END_ID0_EN(x)	UPDATE(x, 8, 8)
#define FRAME_START_ID3_EN(x)	UPDATE(x, 6, 6)
#define FRAME_START_ID2_EN(x)	UPDATE(x, 4, 4)
#define FRAME_START_ID1_EN(x)	UPDATE(x, 2, 2)
#define FRAME_START_ID0_EN(x)	UPDATE(x, 0, 0)

#define MIPI_LVDS_INTSTAT	0x0178
#define FIFO_OVERFLOW_ST	BIT(19)
#define FRAME_END_ID3_ST	BIT(14)
#define FRAME_END_ID2_ST	BIT(12)
#define FRAME_END_ID1_ST	BIT(10)
#define FRAME_END_ID0_ST	BIT(8)
#define FRAME_START_ID3_ST	BIT(6)
#define FRAME_START_ID2_ST	BIT(4)
#define FRAME_START_ID1_ST	BIT(2)
#define FRAME_START_ID0_ST	BIT(0)

#define MIPI_FRAME_NUM_VC0	0x019C
#define MIPI_FRAME_NUM_VC1	0x01A0
#define MIPI_FRAME_NUM_VC2	0x01A4
#define MIPI_FRAME_NUM_VC3	0x01A8
#define VC_FRAME_NUM_END(x)	UPDATE(x, 31, 16)
#define VC_FRAME_NUM_START(x)	UPDATE(x, 15, 0)

#define MIPI_LVDS_SIZE_NUM_ID0	0x01C0
#define MIPI_LVDS_SIZE_NUM_ID1	0x01C4
#define MIPI_LVDS_SIZE_NUM_ID2	0x01C8
#define MIPI_LVDS_SIZE_NUM_ID3	0x01CC
#define ID_LINE_NUM(x)		UPDATE(x, 29, 16)
#define ID_PIXEL_NUM(x)		UPDATE(x, 13, 0)

#define LVDS_ID0_CTRL		0x01D0
#define LVDS_ID1_CTRL		0x01D4
#define LVDS_ID2_CTRL		0x01D8
#define LVDS_ID3_CTRL		0x01DC
#define ID_LVDS_PARSE_TYPE(x)	UPDATE(x, 21, 19)
#define ID_LVDS_HDR_FRAME(x)	UPDATE(x, 12, 12)
#define ID_LVDS_FID(x)		UPDATE(x, 11, 10)
#define ID_LVDS_MAIN_LANE(x)	UPDATE(x, 9, 8)
#define ID_LVDS_LANE_EN(x)	UPDATE(x, 7, 4)
#define ID_LVDS_LANE_MODE(x)	UPDATE(x, 3, 1)
#define ID_LVDS_CAP_EN(x)	UPDATE(x, 0, 0)

#define LVDS_SAV_EAV_ACT0_ID0	0x01E0
#define LVDS_SAV_EAV_BLK0_ID0	0x01E4
#define LVDS_SAV_EAV_ACT1_ID0	0x01E8
#define LVDS_SAV_EAV_BLK1_ID0	0x01EC
#define LVDS_SAV_EAV_ACT0_ID1	0x01F0
#define LVDS_SAV_EAV_BLK0_ID1	0x01F4
#define LVDS_SAV_EAV_ACT1_ID1	0x01F8
#define LVDS_SAV_EAV_BLK1_ID1	0x01FC
#define LVDS_SAV_EAV_ACT0_ID2	0x0200
#define LVDS_SAV_EAV_BLK0_ID2	0x0204
#define LVDS_SAV_EAV_ACT1_ID2	0x0208
#define LVDS_SAV_EAV_BLK1_ID2	0x020C
#define LVDS_SAV_EAV_ACT0_ID3	0x0210
#define LVDS_SAV_EAV_BLK0_ID3	0x0214
#define LVDS_SAV_EAV_ACT1_ID3	0x0218
#define LVDS_SAV_EAV_BLK1_ID3	0x021C

static int rkx11x_vicap_mipi_id_cfg(struct rkx11x_vicap *vicap, u8 id)
{
	struct i2c_client *client = vicap->client;
	struct device *dev = &client->dev;
	struct vicap_vc_id *csi_vc_id = &vicap->vicap_csi.vc_id[id];
	u32 vicap_base = VICAP_BASE;
	u32 reg, val = 0;
	int ret = 0;

	dev_info(dev, "mipi id cfg: id = %d\n", id);

	/* config data type / virtual channel / parse type, enable vicap */
	reg = vicap_base + (MIPI_ID0_CTRL0 + 8 * id);
	val = ID_CAP_EN(1) | ID_DT(csi_vc_id->data_type)
		| ID_VC(csi_vc_id->vc) | ID_PARSE_TYPE(csi_vc_id->parse_type);
	ret |= vicap->i2c_reg_write(client, reg, val);

	/* width and height */
	reg = vicap_base + (MIPI_LVDS_ID0_CTRL1 + 8 * id);
	val = ID_HEIGHT(csi_vc_id->height) | ID_WIDTH(csi_vc_id->width);
	ret |= vicap->i2c_reg_write(client, reg, val);

	return ret;
}

/*
 * data_type: mipi data type
 * vc_flag: bit0~3 for vc0~3, bit0=1 enable vc0
 * parse_type: vicap parse type
 * width, height: sensor width and height
 */
static int rkx11x_vicap_csi_enable(struct rkx11x_vicap *vicap)
{
	struct i2c_client *client = vicap->client;
	struct device *dev = &client->dev;
	u32 vicap_base = VICAP_BASE;
	u32 reg, val;
	u8 id = 0;
	int ret = 0;

	if (vicap->vicap_csi.vc_flag == 0) {
		dev_err(dev, "VICAP: vc_flag = 0 error\n");
		return -EINVAL;
	}

	/* clear interrupt status */
	reg = vicap_base + MIPI_LVDS_INTSTAT;
	val = 0x000fffff;
	ret |= vicap->i2c_reg_write(client, reg, val);

	/*
	 * 1. Set VC and data type for VICAP by writing VICAP_MIPI0_ID0_CTRL0 register
	 * 2. Set picture width and height by writing VICAP_MIPI0_ID0_CTRL1 register
	 * 3. Enable all id by writing VICAP_MIPI0_CTRL register
	 * 4. Enable VICAP interrupt function if necessary by writing VICAP_MIPI0_INTEN
	 */
	for (id = 0; id < RKX11X_VICAP_VC_ID_MAX; id++) {
		if (vicap->vicap_csi.vc_flag & BIT(id)) {
			ret |= rkx11x_vicap_mipi_id_cfg(vicap, id);
		}
	}

	/* select mipi, vicap enable all id */
	reg = vicap_base + MIPI_LVDS_CTRL;
	val = MIPI_LVDS_SEL(0) | ALL_ID_CAP_EN(1);
	ret |= vicap->i2c_reg_write(client, reg, val);

	return ret;
}

/*
 * input_mode: 0 - DVP YUV422, 1 - DVP RAW, 4 - SONY RAW
 * raw_width: 0 - RAW8, 1 - RAW10, 2 - RAW12
 * yuv_order: 0 - UYVY, 1 - VYUY, 2 - YUYV, 3 - YVYU
 */
static int rkx11x_vicap_dvp_enable(struct rkx11x_vicap *vicap)
{
	struct i2c_client *client = vicap->client;
	struct device *dev = &client->dev;
	struct rkx11x_vicap_dvp *vicap_dvp = &vicap->vicap_dvp;
	u32 vicap_base = VICAP_BASE;
	u32 reg, val;
	int ret = 0;

	/* clock_invert: 0 - bypass, 1 - invert */
	if (vicap_dvp->clock_invert != 0) {
		dev_info(dev, "VICAP: VICAP clock invert\n");
		reg = SER_GRF_SOC_CON3;
		val = (1 << (8 + 16)) | (1 << 8);
		ret |= vicap->i2c_reg_write(client, reg, val);
	}

	/* dvp format */
	reg = vicap_base + DVP_FORMAT;
	if (vicap_dvp->input_mode == RKX11X_VICAP_DVP_INPUT_MODE_YUV422) { // YUV422
		val = INPUT_MODE(0) | YUV_IN_ORDER(vicap_dvp->yuv_order);
	} else if (vicap_dvp->input_mode == RKX11X_VICAP_DVP_INPUT_MODE_RAW) { // RAW
		val = INPUT_MODE(1) | RAW_WIDTH(vicap_dvp->raw_width);
	} else if (vicap_dvp->input_mode == RKX11X_VICAP_DVP_INPUT_MODE_SONNY_RAW) { // Sony RAW
		val = INPUT_MODE(4) | RAW_WIDTH(vicap_dvp->raw_width);
	} else {
		dev_err(dev, "VICAP: dvp input mode error!\n");
		return -EINVAL;
	}
	if (vicap_dvp->href_pol)
		val |= HREF_POL(1);
	else
		val |= HREF_POL(0);
	if (vicap_dvp->vsync_pol)
		val |= VSYNC_POL(1);
	else
		val |= VSYNC_POL(0);
	ret |= vicap->i2c_reg_write(client, reg, val);

	reg = vicap_base + DVP_INTSTAT;
	val = FRAME_END_INTST | FRAME_START_INTST;
	ret |= vicap->i2c_reg_write(client, reg, val);

	reg = vicap_base + DVP_INTEN;
	val = FRAME_END_INTEN(0) | FRAME_START_INTEN(0);
	ret |= vicap->i2c_reg_write(client, reg, val);

	/* dvp capture enable */
	reg = vicap_base + DVP_CTRL;
	val = CAP_EN(1);
	ret |= vicap->i2c_reg_write(client, reg, val);

	return ret;
}

int rkx11x_vicap_init(struct rkx11x_vicap *vicap)
{
	int ret = 0;

	if ((vicap == NULL) || (vicap->client == NULL)
			|| (vicap->i2c_reg_read == NULL)
			|| (vicap->i2c_reg_write == NULL)
			|| (vicap->i2c_reg_update == NULL))
		return -EINVAL;

	switch (vicap->interface) {
	case RKX11X_VICAP_CSI:
		ret = rkx11x_vicap_csi_enable(vicap);
		break;
	case RKX11X_VICAP_DVP:
		ret = rkx11x_vicap_dvp_enable(vicap);
		break;
	default:
		dev_info(&vicap->client->dev, "vicap interface = %d error\n",
				vicap->interface);
		ret = -1;
		break;
	}

	return ret;
}
