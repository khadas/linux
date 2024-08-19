// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Rockchip Electronics Co. Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>

#include "rkx12x_reg.h"
#include "rkx12x_dvptx.h"

/* dvp tx base */
#define DVP_TX_BASE			RKX12X_DVP_TX_BASE

/* dvp tx register */
#define DVP_TX_CTRL0			0x0100
#define VSYNC_WIDTH(x)			UPDATE(x, 31, 8)
#define MODULE_EN(x)			UPDATE(x, 0, 0)

#define DVP_TX_CTRL1			0x0104
#define HEIGHT(x)			UPDATE(x, 29, 16)
#define WIDTH(x)			UPDATE(x, 13, 0)

#define DVP_TX_INTEN			0x0174
#define SIZE_ERROR_INTEN		BIT(24)
#define FIFO_EMPTY_INTEN		BIT(20)
#define FIFO_OVERFLOW_INTEN		BIT(19)
#define FRAME_END_INTEN			BIT(8)
#define FRAME_START_INTEN		BIT(0)

#define DVP_TX_INTSTAT			0x0178
#define SIZE_ERROR_INTST		BIT(24)
#define FIFO_EMPTY_INTST		BIT(20)
#define FIFO_OVERFLOW_INTST		BIT(19)
#define FRAME_END_INTST			BIT(8)
#define FRAME_START_INTST		BIT(0)

#define DVP_TX_SIZE_NUM			0x01c0
#define LINE_NUM			UPDATE(x, 29, 16)
#define PIXEL_NUM			UPDATE(x, 13, 0)

static int rkx12x_dvptx_enable(struct rkx12x_dvptx *dvptx)
{
	struct i2c_client *client = dvptx->client;
	struct device *dev = &client->dev;
	u32 dvp_tx_base = DVP_TX_BASE;
	u32 reg, val;
	int ret = 0;

	/* clock_invert: 0 - bypass, 1 - invert */
	if (dvptx->clock_invert != 0) {
		dev_info(dev, "DVP TX: CIF dclk invert.");
		reg = DES_GRF_SOC_CON3;
		val = (1 << 16) | (1 << 0);
		ret |= dvptx->i2c_reg_write(client, reg, val);
	}

	reg = dvp_tx_base + DVP_TX_CTRL1;
	val = WIDTH(dvptx->width) | HEIGHT(dvptx->height);
	ret |= dvptx->i2c_reg_write(client, reg, val);

	reg = dvp_tx_base + DVP_TX_CTRL0;
	val = VSYNC_WIDTH(128) | MODULE_EN(1);
	ret |= dvptx->i2c_reg_write(client, reg, val);
	if (ret)
		return ret;

	/* if clock and link type config error, dvp tx register will set fail */
	ret = dvptx->i2c_reg_read(client, reg, &val);
	if (ret)
		return ret;
	dev_info(dev, "DVPTX: Get DVP_TX_CTRL0(0x%x) = 0x%x\n", reg, val);

	return 0;
}

int rkx12x_dvptx_init(struct rkx12x_dvptx *dvptx)
{
	int ret = 0;

	if ((dvptx == NULL) || (dvptx->client == NULL)
			|| (dvptx->i2c_reg_read == NULL)
			|| (dvptx->i2c_reg_write == NULL)
			|| (dvptx->i2c_reg_update == NULL))
		return -EINVAL;

	dev_info(&dvptx->client->dev, "=== rkx12x dvptx init ===\n");

	ret = rkx12x_dvptx_enable(dvptx);
	if (ret)
		return ret;

	return 0;
}
