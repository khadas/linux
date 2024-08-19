// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/init.h>

#include "rkx12x_csi2tx.h"
#include "rkx12x_reg.h"

/* csi2tx base */
#define CSI2TX0_BASE			RKX12X_CSI_TX0_BASE
#define CSI2TX1_BASE			RKX12X_CSI_TX1_BASE

#define CSI2TX_DEV(id)			((id) == 0 ? (CSI2TX0_BASE) : (CSI2TX1_BASE))

/* csi tx register */
#define CSITX_CONFIG_DONE		0x0000
#define CONFIG_DONE_MODE(x)		UPDATE(x, 8, 8)
#define CONFIG_DONE_IMD			BIT(4)
#define CONFIG_DONE			BIT(0)

#define CSITX_CSITX_EN			0x0004
#define AUTO_GATING_EN(x)		UPDATE(x, 31, 31)
#define IDI_CLK_DIS(x)			UPDATE(x, 22, 22)
#define VOP_CLK_DIS(x)			UPDATE(x, 21, 21)
#define CAM_CLK_DIS(x)			UPDATE(x, 20, 20)
#define ESC_CLK_DIS(x)			UPDATE(x, 19, 19)
#define HS_CLK_DIS(x)			UPDATE(x, 18, 18)
#define WORD_HS_CLK_DIS(x)		UPDATE(x, 17, 17)
#define BYTE_HS_CLK_DIS(x)		UPDATE(x, 16, 16)
#define DATA_LANE_NUM(x)		UPDATE(x, 5, 4)
#define DPHY_EN(x)			UPDATE(x, 2, 2)
#define CPHY_EN(x)			UPDATE(x, 1, 1)
#define CSITX_EN(x)			UPDATE(x, 0, 0)

#define CSITX_VERSION			0x0008

#define CSITX_SYS_CTRL0_IMD		0x0010
#define SOFT_RST_EN(x)			UPDATE(x, 0, 0)

#define CSITX_SYS_CTRL1			0x0014
#define BYPASS_SELECT(x)		UPDATE(x, 0, 0)

#define CSITX_SYS_CTRL2			0x0018
#define IDI_WHOLE_FRM_EN(x)		UPDATE(x, 4, 4)
#define HSYNC_EN(x)			UPDATE(x, 1, 1)
#define VSYNC_EN(x)			UPDATE(x, 0, 0)

#define CSITX_SYS_CTRL3_IMD		0x001c
#define CONT_MODE_CLK_CLR		BIT(8)
#define CONT_MODE_CLK_SET		BIT(4)
#define NO_CONTINUOUS_MODE		BIT(0)

#define CSITX_BYPASS_CTRL		0x0060
#define BYPASS_WC_USERDEFINE(x)		UPDATE(x, 31, 16)
#define BYPASS_VC_USERDEFINE(x)		UPDATE(x, 15, 14)
#define BYPASS_DT_USERDEFINE(x)		UPDATE(x, 13, 8)
#define BYPASS_CAM_FORAMT(x)		UPDATE(x, 7, 4)
#define BYPASS_WC_USERDEFINE_EN(x)	UPDATE(x, 3, 3)
#define BYPASS_VC_USERDEFINE_EN(x)	UPDATE(x, 2, 2)
#define BYPASS_DT_USERDEFINE_EN(x)	UPDATE(x, 1, 1)
#define BYPASS_PATH_EN(x)		UPDATE(x, 0, 0)

#define CSITX_BYPASS_PKT_CTRL		0x0064
#define BYPASS_WC_ACTTIVE		UPDATE(x, 31, 16)
#define BYPASS_PKT_PADDING_EN(x)	UPDATE(x, 8, 8)
#define BYPASS_LINE_PADDING_NUM(x)	UPDATE(x, 7, 5)
#define BYPASS_LINE_PADDING_EN(x)	UPDATE(x, 4, 4)

#define CSITX_BYPASS_D_GAIN_CTRL	0x0068
#define CSITX_BYPASS_D_GAIN_ID		0x006c

#define CSITX_STATUS0			0x0070
#define LANE_SPLITTER_IDLE		BIT(6)
#define PACKETISER_IDLE			BIT(5)
#define TX_BUFF_EMTPY			BIT(4)
#define PLD_LB_VALID			BIT(3)
#define PLD_LB_EMPTY			BIT(2)
#define HDR_FIFO_EMPTY			BIT(1)
#define CSITX_IDLE			BIT(0)

#define CSITX_STATUS1			0x0074
#define CSITX_STATUS2			0x0078
#define CSITX_LINE_FLAG_NUM		0x007c
#define CSITX_INTR_EN_IMD		0x0080
#define CSITX_INTR_CLR_IMD		0x0084
#define CSITX_INTR_STATUS		0x0088
#define CSITX_INTR_RAW_STATUS		0x008c
#define CSITX_ERR_INTR_EN_IMD		0x0090
#define CSITX_ERR_INTR_CLR_IMD		0x0094
#define CSITX_ERR_INTR_STATUS_IMD	0x0098
#define CSITX_ERR_INTR_RAW_STATUS_IMD	0x009c


/**
 * rkx12x_csi_tx_enable - csi tx enable.
 * @id: cst tx id - 0 or 1
 * @data_lanes: mipi tx dphy lane number
 * @format: 0: RAW8-4PP, 1: RAW10-4PP, 2: PIXEL10-4PP, 3: PIXEL128
 * @non_continuous: 0: continuous mode, 1: non-continuous mode
 *
 */
static int rkx12x_csi2tx_enable(struct rkx12x_csi2tx *csi2tx)
{
	struct i2c_client *client = csi2tx->client;
	struct device *dev = &client->dev;
	u32 csi2tx_base = CSI2TX_DEV(csi2tx->phy_id);
	u32 reg, val;
	int ret = 0;

	/* cst2tx version */
	reg = csi2tx_base + CSITX_VERSION;
	ret = csi2tx->i2c_reg_read(client, reg, &val);
	if (ret)
		return ret;
	dev_info(dev, "CSI2TX: Version: %x\n", val);

	/* csi2tx enable, set phy mode and lane number */
	reg = csi2tx_base + CSITX_CSITX_EN;
	val = CSITX_EN(1) | DATA_LANE_NUM(csi2tx->data_lanes - 1);
	if (csi2tx->phy_mode == RKX12X_CSI2TX_DPHY)
		val |= DPHY_EN(1);
	else
		val |= CPHY_EN(1);
	ret |= csi2tx->i2c_reg_write(client, reg, val);

	/* bypass mode */
	reg = csi2tx_base + CSITX_SYS_CTRL1;
	val = BYPASS_SELECT(1);
	ret |= csi2tx->i2c_reg_write(client, reg, val);

	/* idi_whole_frm enable, hsync disable, vsync enable */
	reg = csi2tx_base + CSITX_SYS_CTRL2;
	val = IDI_WHOLE_FRM_EN(1) | HSYNC_EN(0) | VSYNC_EN(1);
	ret |= csi2tx->i2c_reg_write(client, reg, val);

	if (csi2tx->clock_mode == RKX12X_CSI2TX_CLK_NON_CONTINUOUS) {
		dev_info(dev, "CSI2TX: non-continuous mode\n");

		/*
		 * if csi_tx set non-continuous, dphy also should be set.
		 *   mipi dphy non-continuous setting: dphy register MISC_PARA bit11 set 1
		 */
		reg = csi2tx_base + CSITX_SYS_CTRL3_IMD;
		val = NO_CONTINUOUS_MODE;
		ret |= csi2tx->i2c_reg_write(client, reg, val);
	} else {
		/* clock lane enable in continuous mode */
		reg = csi2tx_base + CSITX_SYS_CTRL3_IMD;
		val = CONT_MODE_CLK_SET;
		ret |= csi2tx->i2c_reg_write(client, reg, val);
	}

	/* line num flag1 and flag0 */
	reg = csi2tx_base + CSITX_LINE_FLAG_NUM;
	val = 0x000e0002;
	ret |= csi2tx->i2c_reg_write(client, reg, val);

	/* clear all interrupt */
	reg = csi2tx_base + CSITX_INTR_CLR_IMD;
	val = 0xffffffff;
	ret |= csi2tx->i2c_reg_write(client, reg, val);

	/* clear all errors interrupt */
	reg = csi2tx_base + CSITX_ERR_INTR_CLR_IMD;
	val = 0x00000fff;
	ret |= csi2tx->i2c_reg_write(client, reg, val);

	/* enable all errors interrupt */
	reg = csi2tx_base + CSITX_ERR_INTR_EN_IMD;
	val = 0x00000fff;
	ret |= csi2tx->i2c_reg_write(client, reg, val);

	/* bypass camera format and bypass path enable */
	reg = csi2tx_base + CSITX_BYPASS_CTRL;
	val = BYPASS_CAM_FORAMT(csi2tx->format) | BYPASS_PATH_EN(1);
	ret |= csi2tx->i2c_reg_write(client, reg, val);

	/* make register valid immediately */
	reg = csi2tx_base + CSITX_CONFIG_DONE;
	val = CONFIG_DONE_IMD;
	ret |= csi2tx->i2c_reg_write(client, reg, val);

	return ret;
}

int rkx12x_csi2tx_init(struct rkx12x_csi2tx *csi2tx)
{
	int ret = 0;

	if ((csi2tx == NULL) || (csi2tx->client == NULL)
			|| (csi2tx->i2c_reg_read == NULL)
			|| (csi2tx->i2c_reg_write == NULL)
			|| (csi2tx->i2c_reg_update == NULL))
		return -EINVAL;

	dev_info(&csi2tx->client->dev, "=== rkx12x csi2tx init ===\n");

	ret = rkx12x_csi2tx_enable(csi2tx);
	if (ret)
		return ret;

	return 0;
}