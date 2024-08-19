// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip SerDes RKx12x Deserializer driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 * V1.00.00 rockchip serdes rkx12x driver framework.
 *     1. support for rkx12x chip version v0 and v1
 *     2. support csi and dvp camera stream base functions
 *
 * V1.01.00
 *     1. support pinctrl and passthrough
 *     2. support state irq
 *
 * V1.02.00
 *     1. support mipi txphy dlane timing setting
 *     2. support mipi txphy dlane timing property parsing
 *     3. support rk3588 dcphy parameter setting
 *
 * V1.03.00
 *     1. unified use __v4l2_ctrl_handler_setup in the xxx_start_stream
 *     2. g_mbus_config fix config->flags channels setting error
 *     3. fix kernel-6.1 compile error
 *
 * V1.04.00
 *     1. support multi-channel information configuration
 *     2. mode vc initialization when vc-array isn't configured
 *
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>

#include "rkx12x_i2c.h"
#include "rkx12x_drv.h"
#include "rkx12x_reg.h"
#include "rkx12x_api.h"
#include "rkx12x_remote.h"

#define DRIVER_VERSION		KERNEL_VERSION(1, 0x04, 0x00)

#define RKX12X_NAME		"rkx12x"

#define I2CMUX_CH_SEL_EN	0 /* 1: i2c-mux channel select */

#define IOMUX_PINSET_API_EN	1 /* 1: PINCTRL API enable */

/* link rate pma pll */
struct rkx12x_link_rate_pll {
	u32 link_rate;

	struct rkx12x_pma_pll ser_pma_pll;
	struct rkx12x_pma_pll des_pma_pll;
};

static const struct rkx12x_link_rate_pll link_rate_plls[] = {
	{
		.link_rate = RKX12X_LINK_RATE_2GBPS_83M,

		.ser_pma_pll = {
			.rate_mode = RKX12X_PMA_PLL_HDR_RATE_MODE,
			.pll_div4 = true,
			.pll_div = 21330,
			.clk_div = 2,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},

		.des_pma_pll = {
			.pll_div4 = true,
			.pll_div = 21330,
			.clk_div = 23,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},
	},
	{
		.link_rate = RKX12X_LINK_RATE_4GBPS_83M,

		.ser_pma_pll = {
			.rate_mode = RKX12X_PMA_PLL_FDR_RATE_MODE,
			.pll_div4 = true,
			.pll_div = 21330,
			.clk_div = 2,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},

		.des_pma_pll = {
			.pll_div4 = true,
			.pll_div = 21330,
			.clk_div = 23,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},
	},
	{
		.link_rate = RKX12X_LINK_RATE_4GBPS_125M,

		.ser_pma_pll = {
			.rate_mode = RKX12X_PMA_PLL_FDR_RATE_MODE,
			.pll_div4 = true,
			.pll_div = 21330,
			.clk_div = 1,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},

		.des_pma_pll = {
			.pll_div4 = true,
			.pll_div = 21330,
			.clk_div = 15,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},
	},
	{
		.link_rate = RKX12X_LINK_RATE_4GBPS_250M,

		.ser_pma_pll = {
			.rate_mode = RKX12X_PMA_PLL_FDR_RATE_MODE,
			.pll_div4 = true,
			.pll_div = 21330,
			.clk_div = 0,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},

		.des_pma_pll = {
			.pll_div4 = true,
			.pll_div = 21330,
			.clk_div = 7,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},
	},
	{
		.link_rate = RKX12X_LINK_RATE_4_5GBPS_140M,

		.ser_pma_pll = {
			.rate_mode = RKX12X_PMA_PLL_FDR_RATE_MODE,
			.pll_div4 = true,
			.pll_div = 24000,
			.clk_div = 1,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},

		.des_pma_pll = {
			.pll_div4 = true,
			.pll_div = 12000,
			.clk_div = 7,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},
	},
	{
		.link_rate = RKX12X_LINK_RATE_4_6GBPS_300M,

		.ser_pma_pll = {
			.rate_mode = RKX12X_PMA_PLL_FDR_RATE_MODE,
			.pll_div4 = true,
			.pll_div = 25000,
			.clk_div = 0,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},

		.des_pma_pll = {
			.pll_div4 = true,
			.pll_div = 25000,
			.clk_div = 7,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},
	},
	{
		.link_rate = RKX12X_LINK_RATE_4_8GBPS_150M,

		.ser_pma_pll = {
			.rate_mode = RKX12X_PMA_PLL_FDR_RATE_MODE,
			.pll_div4 = true,
			.pll_div = 26000,
			.clk_div = 1,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},

		.des_pma_pll = {
			.pll_div4 = true,
			.pll_div = 13000,
			.clk_div = 7,
			.pll_refclk_div = 0,
			.pll_fck_vco_div2 = true,
		},
	},
};

static void rkx12x_callback_init(struct rkx12x *rkx12x)
{
	rkx12x->i2c_reg_read = rkx12x_i2c_reg_read;
	rkx12x->i2c_reg_write = rkx12x_i2c_reg_write;
	rkx12x->i2c_reg_update = rkx12x_i2c_reg_update;
}

static int rkx12x_check_chip_id(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	u32 chip_id;
	int ret = 0, loop = 0;

	for (loop = 0; loop < 5; loop++) {
		if (loop != 0) {
			dev_info(dev, "check rkx12x chipid retry (%d)", loop);
			msleep(10);
		}
		ret = rkx12x->i2c_reg_read(client, RKX12X_GRF_REG_CHIP_ID, &chip_id);
		if (ret == 0) {
			if (chip_id == rkx12x->chip_id) {
				if (chip_id == RKX12X_DES_CHIP_ID_V0) {
					dev_info(dev, "rkx12x v0, chip_id: 0x%08x\n", chip_id);
					rkx12x->version = RKX12X_DES_CHIP_V0;
					return 0;
				}

				if (chip_id == RKX12X_DES_CHIP_ID_V1) {
					dev_info(dev, "rkx12x v1, chip_id: 0x%08x\n", chip_id);
					rkx12x->version = RKX12X_DES_CHIP_V1;
					return 0;
				}
			} else {
				// if chip id is unexpected, retry
				dev_err(dev, "Unexpected rkx12x chipid = 0x%08x\n", chip_id);
			}
		}
	}

	dev_err(dev, "rkx12x check chipid error, ret(%d)\n", ret);

	return -ENODEV;
}

/* rkx12x cru module */
static int rkx12x_add_hwclk(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	struct hwclk *hwclk = NULL;
	struct xferclk xfer;
	int ret = 0;

	dev_info(dev, "rkx12x add hwclk\n");

	memset(&xfer, 0, sizeof(struct xferclk));

	snprintf(xfer.name, sizeof(xfer.name), "0x%x", client->addr);
	xfer.type = CLK_RKX120;
	xfer.client = client;
	if (rkx12x->version == RKX12X_DES_CHIP_V0) {
		xfer.version = RKX120_DES_CLK;
	} else if (rkx12x->version == RKX12X_DES_CHIP_V1) {
		xfer.version = RKX121_DES_CLK;
	} else {
		xfer.version = -1;
		dev_err(dev, "%s chip version error\n", __func__);
		return -1;
	}
	xfer.read = rkx12x->i2c_reg_read;
	xfer.write = rkx12x->i2c_reg_write;

	hwclk = &rkx12x->hwclk;
	ret = rkx12x_hwclk_register(hwclk, &xfer);
	if (ret)
		dev_err(dev, "%s error, ret(%d)\n", __func__, ret);

	return ret;
}

static int rkx12x_clk_hw_init(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	struct hwclk *hwclk = &rkx12x->hwclk;
	u32 clk_rklink_hz;
	int ret = 0;

	dev_info(dev, "rkx12x clock init\n");

	ret = rkx12x_hwclk_init(hwclk);
	if (ret) {
		dev_err(dev, "hwclk init error, ret(%d)\n", ret);
		return ret;
	}

	clk_rklink_hz = rkx12x->serdes_dclk_hz;
	if (rkx12x->linkrx.lane_id == RKX12X_LINK_LANE0) {
		dev_info(dev, "rkx12x e0_clk_rklink_rx clock set rate = %d", clk_rklink_hz);
		ret = rkx12x_hwclk_set_rate(hwclk, RKX120_CPS_E0_CLK_RKLINK_RX_PRE, clk_rklink_hz);

		return ret;
	}

	if (rkx12x->linkrx.lane_id == RKX12X_LINK_LANE1) {
		dev_info(dev, "rkx12x e1_clk_rklink_rx clock set rate = %d", clk_rklink_hz);
		ret = rkx12x_hwclk_set_rate(hwclk, RKX120_CPS_E1_CLK_RKLINK_RX_PRE, clk_rklink_hz);

		return ret;
	}

	return 0;
}

/* rkx12x pin module */
static int rkx12x_add_hwpin(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	struct hwpin *hwpin = NULL;
	struct xferpin xfer;
	int ret = 0;

	dev_info(dev, "rkx12x add hwpin\n");

	memset(&xfer, 0, sizeof(struct xferpin));

	snprintf(xfer.name, sizeof(xfer.name), "0x%x", client->addr);
	xfer.type = PIN_RKX120;
	xfer.client = client;
	xfer.read = rkx12x->i2c_reg_read;
	xfer.write = rkx12x->i2c_reg_write;

	hwpin = &rkx12x->hwpin;
	ret = rkx12x_hwpin_register(hwpin, &xfer);
	if (ret)
		dev_err(dev, "%s error, ret(%d)\n", __func__, ret);

	return ret;
}

/* rkx12x txphy module */
static int rkx12x_add_txphy(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	struct rkx12x_txphy *txphy = &rkx12x->txphy;

	dev_info(dev, "rkx12x add txphy\n");

	switch (rkx12x->stream_interface) {
	case RKX12X_STREAM_INTERFACE_MIPI:
		// phy id
		if (rkx12x->stream_port == RKX12X_STREAM_PORT0)
			txphy->phy_id = RKX12X_TXPHY_ID0;
		else
			txphy->phy_id = RKX12X_TXPHY_ID1;

		// phy mode
		txphy->phy_mode = RKX12X_TXPHY_MODE_MIPI;

		// data lanes
		txphy->data_lanes = rkx12x->bus_cfg.bus.mipi_csi2.num_data_lanes;
		break;
	case RKX12X_STREAM_INTERFACE_DVP:
		// phy id
		txphy->phy_id = RKX12X_TXPHY_ID0; // fixed id0

		// phy mode
		txphy->phy_mode = RKX12X_TXPHY_MODE_GPIO;
		break;
	default:
		dev_info(dev, "stream interface = %d error for txphy\n",
			rkx12x->stream_interface);
		return -1;
	}

	txphy->client = rkx12x->client;
	txphy->i2c_reg_read = rkx12x->i2c_reg_read;
	txphy->i2c_reg_write = rkx12x->i2c_reg_write;
	txphy->i2c_reg_update = rkx12x->i2c_reg_update;

	return 0;
}

static int rkx12x_txphy_parse_dt(struct device *dev,
		struct device_node *local_node, struct rkx12x_txphy *txphy)
{
	const char *dlane_hstrail_name = "txphy-timing-dlane-hstrail";
	u32 *dlane_t_hstrail;
	u32 value = 0;
	int ret = 0, length = 0;

	if (dev == NULL || local_node == NULL || txphy == NULL)
		return -EINVAL;

	ret = of_property_read_u32(local_node, "txphy-clock-mode", &value);
	if (ret == 0) {
		dev_info(dev, "txphy-clock-mode property: %d", value);
		txphy->clock_mode = value;
	}

	/* txphy timing */
	length = of_property_count_u32_elems(local_node, dlane_hstrail_name);
	if (length < 0)
		return 0;

	dlane_t_hstrail = kmalloc_array(length, sizeof(u32), GFP_KERNEL);
	if (!dlane_t_hstrail)
		return 0;

	ret = of_property_read_u32_array(local_node,
				dlane_hstrail_name, dlane_t_hstrail, length);
	if (ret == 0) {
		switch (length) {
		case 4:
			dev_info(dev, "%s[3] property: %d\n", dlane_hstrail_name, dlane_t_hstrail[3]);
			txphy->mipi_timing.t_hstrail_dlane3 = dlane_t_hstrail[3];
			txphy->mipi_timing.t_hstrail_dlane3 |= RKX12X_MIPI_TIMING_EN;
			fallthrough;
		case 3:
			dev_info(dev, "%s[2] property: %d\n", dlane_hstrail_name, dlane_t_hstrail[2]);
			txphy->mipi_timing.t_hstrail_dlane2 = dlane_t_hstrail[2];
			txphy->mipi_timing.t_hstrail_dlane2 |= RKX12X_MIPI_TIMING_EN;
			fallthrough;
		case 2:
			dev_info(dev, "%s[1] property: %d\n", dlane_hstrail_name, dlane_t_hstrail[1]);
			txphy->mipi_timing.t_hstrail_dlane1 = dlane_t_hstrail[1];
			txphy->mipi_timing.t_hstrail_dlane1 |= RKX12X_MIPI_TIMING_EN;
			fallthrough;
		case 1:
			dev_info(dev, "%s[0] property: %d\n", dlane_hstrail_name, dlane_t_hstrail[0]);
			txphy->mipi_timing.t_hstrail_dlane0 = dlane_t_hstrail[0];
			txphy->mipi_timing.t_hstrail_dlane0 |= RKX12X_MIPI_TIMING_EN;
			fallthrough;
		default:
			break;
		}

		kfree(dlane_t_hstrail);
	}

	return 0;
}

static int rkx12x_txphy_hw_init(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;

	dev_info(dev, "=== rkx12x txphy hw init ===\n");

	return rkx12x_txphy_init(&rkx12x->txphy);
}

/* rkx12x csi2tx module */
static int rkx12x_add_csi2tx(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	struct rkx12x_csi2tx *csi2tx = &rkx12x->csi2tx;

	dev_info(dev, "rkx12x add csi2tx\n");

	// phy id
	if (rkx12x->stream_port == RKX12X_STREAM_PORT0)
		csi2tx->phy_id = RKX12X_CSI2TX_ID0;
	else
		csi2tx->phy_id = RKX12X_CSI2TX_ID1;

	// phy mode
	csi2tx->phy_mode = RKX12X_CSI2TX_DPHY;

	// clock mode
	if (rkx12x->txphy.clock_mode == RKX12X_TXPHY_CLK_NON_CONTINUOUS)
		csi2tx->clock_mode = RKX12X_CSI2TX_CLK_NON_CONTINUOUS;
	else
		csi2tx->clock_mode = RKX12X_CSI2TX_CLK_CONTINUOUS;

	// data lanes
	csi2tx->data_lanes = rkx12x->bus_cfg.bus.mipi_csi2.num_data_lanes;

	csi2tx->client = rkx12x->client;
	csi2tx->i2c_reg_read = rkx12x->i2c_reg_read;
	csi2tx->i2c_reg_write = rkx12x->i2c_reg_write;
	csi2tx->i2c_reg_update = rkx12x->i2c_reg_update;

	return 0;
}

static int rkx12x_csi2tx_parse_dt(struct device *dev,
		struct device_node *local_node, struct rkx12x_csi2tx *csi2tx)
{
	u32 value = 0;
	int ret = 0;

	if (dev == NULL || local_node == NULL || csi2tx == NULL)
		return -EINVAL;

	ret = of_property_read_u32(local_node, "csi2tx-format", &value);
	if (ret == 0) {
		dev_info(dev, "csi2tx-format property: %d", value);
		csi2tx->format = value;
	}

	return 0;
}

static int rkx12x_csi2tx_hw_init(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;

	dev_info(dev, "=== rkx12x csi2tx hw init ===\n");

	return rkx12x_csi2tx_init(&rkx12x->csi2tx);
}

/* rkx12x dvptx module */
static int rkx12x_add_dvptx(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	struct rkx12x_dvptx *dvptx = &rkx12x->dvptx;

	dev_info(dev, "rkx12x add dvptx\n");

	dvptx->client = rkx12x->client;
	dvptx->i2c_reg_read = rkx12x->i2c_reg_read;
	dvptx->i2c_reg_write = rkx12x->i2c_reg_write;
	dvptx->i2c_reg_update = rkx12x->i2c_reg_update;

	return 0;
}

static int rkx12x_dvptx_parse_dt(struct device *dev,
		struct device_node *local_node, struct rkx12x_dvptx *dvptx)
{
	u32 value = 0;
	int ret = 0;

	if (dev == NULL || local_node == NULL || dvptx == NULL)
		return -EINVAL;

	ret = of_property_read_u32(local_node, "dvptx-pclk", &value);
	if (ret == 0) {
		dev_info(dev, "dvptx-pclk property: %d", value);
		dvptx->pclk_freq = value;
	}

	ret = of_property_read_u32(local_node, "dvptx-clock-invert", &value);
	if (ret == 0) {
		dev_info(dev, "dvptx-clock-invert property: %d", value);
		dvptx->clock_invert = value;
	}

	return 0;
}

static int rkx12x_dvptx_hw_init(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;

	dev_info(dev, "=== rkx12x dvptx hw init ===\n");

	rkx12x->dvptx.width = rkx12x->cur_mode->width;
	rkx12x->dvptx.height = rkx12x->cur_mode->height;

	return rkx12x_dvptx_init(&rkx12x->dvptx);
}

/* rkx12x dvp iomux */
static int rkx12x_dvp_iomux_cfg(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
#if (IOMUX_PINSET_API_EN == 0)
	u32 reg = 0, val = 0;
	int ret = 0;

	dev_info(dev, "rkx12x dvp iomux\n");

	reg = DES_GRF_GPIO1B_IOMUX;
	val = 0;
	val |= HIWORD_UPDATE(GPIO1B3_CIF_D15, GPIO1B3_MASK, GPIO1B3_SHIFT);
	val |= HIWORD_UPDATE(GPIO1B2_CIF_D14, GPIO1B2_MASK, GPIO1B2_SHIFT);
	val |= HIWORD_UPDATE(GPIO1B1_CIF_D13, GPIO1B1_MASK, GPIO1B1_SHIFT);
	val |= HIWORD_UPDATE(GPIO1B0_CIF_D12, GPIO1B0_MASK, GPIO1B0_SHIFT);
	ret |= rkx12x->i2c_reg_write(client, reg, val);

	reg = DES_GRF_GPIO1A_IOMUX;
	val = 0;
	val |= HIWORD_UPDATE(GPIO1A7_CIF_D11, GPIO1A7_MASK, GPIO1A7_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A6_CIF_D10, GPIO1A6_MASK, GPIO1A6_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A5_CIF_D9, GPIO1A5_MASK, GPIO1A5_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A4_CIF_D8, GPIO1A4_MASK, GPIO1A4_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A3_CIF_D7, GPIO1A3_MASK, GPIO1A3_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A2_CIF_D6, GPIO1A2_MASK, GPIO1A2_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A1_CIF_D5, GPIO1A1_MASK, GPIO1A1_SHIFT);
	val |= HIWORD_UPDATE(GPIO1A0_CIF_D4, GPIO1A0_MASK, GPIO1A0_SHIFT);
	ret |= rkx12x->i2c_reg_write(client, reg, val);

	reg = DES_GRF_GPIO0C_IOMUX_H;
	val = 0;
	val |= HIWORD_UPDATE(GPIO0C7_CIF_D3, GPIO0C7_MASK, GPIO0C7_SHIFT);
	val |= HIWORD_UPDATE(GPIO0C6_CIF_D2, GPIO0C6_MASK, GPIO0C6_SHIFT);
	val |= HIWORD_UPDATE(GPIO0C5_CIF_D1, GPIO0C5_MASK, GPIO0C5_SHIFT);
	ret |= rkx12x->i2c_reg_write(client, reg, val);

	reg = DES_GRF_GPIO0C_IOMUX_L;
	val = 0;
	val |= HIWORD_UPDATE(GPIO0C4_CIF_D0, GPIO0C4_MASK, GPIO0C4_SHIFT);
	val |= HIWORD_UPDATE(GPIO0C2_CIF_HSYNC, GPIO0C2_MASK, GPIO0C2_SHIFT);
	val |= HIWORD_UPDATE(GPIO0C1_CIF_VSYNC, GPIO0C1_MASK, GPIO0C1_SHIFT);
	val |= HIWORD_UPDATE(GPIO0C0_CIF_CLK, GPIO0C0_MASK, GPIO0C0_SHIFT);
	ret |= rkx12x->i2c_reg_write(client, reg, val);

	if (ret)
		dev_err(dev, "rkx12x dvp iomux error\n");

	return 0;
#else /* IOMUX_PINSET_API_EN */
	uint32_t pins;
	int ret = 0;

	dev_info(dev, "rkx120 dvp pin iomux\n");

	pins = RKX12X_GPIO_PIN_C0 | RKX12X_GPIO_PIN_C1 | RKX12X_GPIO_PIN_C2 |
	       RKX12X_GPIO_PIN_C4 | RKX12X_GPIO_PIN_C5 |
	       RKX12X_GPIO_PIN_C6 | RKX12X_GPIO_PIN_C7;
	ret |= rkx12x_hwpin_set(&rkx12x->hwpin, RKX12X_DES_GPIO_BANK0,
					pins, RKX12X_PIN_CONFIG_MUX_FUNC2);

	pins = RKX12X_GPIO_PIN_A0 | RKX12X_GPIO_PIN_A1 | RKX12X_GPIO_PIN_A2 |
	       RKX12X_GPIO_PIN_A3 | RKX12X_GPIO_PIN_A4 | RKX12X_GPIO_PIN_A5 |
	       RKX12X_GPIO_PIN_A6 | RKX12X_GPIO_PIN_A7 | RKX12X_GPIO_PIN_B0 |
	       RKX12X_GPIO_PIN_B1 | RKX12X_GPIO_PIN_B2 | RKX12X_GPIO_PIN_B3;
	ret |= rkx12x_hwpin_set(&rkx12x->hwpin, RKX12X_DES_GPIO_BANK1,
					pins, RKX12X_PIN_CONFIG_MUX_FUNC2);
	if (ret)
		dev_err(dev, "rkx12x dvp iomux error\n");

	return 0;
#endif /* IOMUX_PINSET_API_EN */
}

/* rkx12x linkrx module */
static int rkx12x_add_linkrx(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	struct rkx12x_linkrx *linkrx = &rkx12x->linkrx;

	dev_info(dev, "rkx12x add linkrx\n");

	// lane id
	if (rkx12x->stream_port == RKX12X_STREAM_PORT0)
		linkrx->lane_id = RKX12X_LINK_LANE0;
	else
		linkrx->lane_id = RKX12X_LINK_LANE1;

	// engine source
	if (rkx12x->stream_interface == RKX12X_STREAM_INTERFACE_MIPI)
		linkrx->engine_source = RKX12X_LINK_STREAM_CAMERA_CSI;
	else
		linkrx->engine_source = RKX12X_LINK_STREAM_CAMERA_DVP;

	linkrx->client = rkx12x->client;
	linkrx->i2c_reg_read = rkx12x->i2c_reg_read;
	linkrx->i2c_reg_write = rkx12x->i2c_reg_write;
	linkrx->i2c_reg_update = rkx12x->i2c_reg_update;

	return 0;
}

static int rkx12x_linkrx_parse_dt(struct device *dev,
		struct device_node *local_node, struct rkx12x_linkrx *linkrx)
{
	struct device_node *remote_node = NULL;
	u32 value = 0;
	int ret = 0;

	if (dev == NULL || local_node == NULL || linkrx == NULL)
		return -EINVAL;

	// link rate
	ret = of_property_read_u32(local_node, "link-rate", &value);
	if (ret == 0) {
		dev_info(dev, "link-rate property: %d", value);
		linkrx->link_rate = value;
	}

	// link line
	ret = of_property_read_u32(local_node, "link-line", &value);
	if (ret == 0) {
		dev_info(dev, "link-line property: %d", value);
		linkrx->link_line = value;
	}

	// lane engine id
	ret = of_property_read_u32(local_node, "lane-engine-id", &value);
	if (ret == 0) {
		dev_info(dev, "lane-engine-id property: %d", value);
		linkrx->engine_id = value;
	}

	// lane engine delay
	ret = of_property_read_u32(local_node, "lane-engine-delay", &value);
	if (ret == 0) {
		dev_info(dev, "lane-engine-delay property: %d", value);
		linkrx->engine_delay = value;
	}

	// lane video packet size
	ret = of_property_read_u32(local_node, "lane-video-packet-size", &value);
	if (ret == 0) {
		dev_info(dev, "lane-video-packet-size property: %d", value);
		linkrx->video_packet_size = value;
	}

	/* link get remote camera node */
	remote_node = of_parse_phandle(local_node, "lane-remote-cam", 0);
	if (!IS_ERR_OR_NULL(remote_node)) {
		dev_info(dev, "remote camera node: %pOF\n", remote_node);
		linkrx->remote_cam_node = remote_node;
	} else {
		dev_warn(dev, "lane-remote-cam node isn't exist\n");
	}

	/* link get remote serializer node */
	remote_node = of_parse_phandle(local_node, "lane-remote-ser", 0);
	if (!IS_ERR_OR_NULL(remote_node)) {
		dev_info(dev, "remote serializer node: %pOF\n", remote_node);
		linkrx->remote_ser_node = remote_node;
	} else {
		dev_warn(dev, "lane-remote-ser node isn't exist\n");
	}

	return ret;
}

static int rkx12x_linkrx_hw_init(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;

	dev_info(dev, "=== rkx12x linkrx hw init ===\n");

	return rkx12x_linkrx_lane_init(&rkx12x->linkrx);
}

/* rkx12x pinctrl init */
static int rkx12x_pinctrl_init(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	const char *pinctrl_name = "rkx12x-pins";
	struct device_node *local_node = NULL, *np = NULL;
	u32 *pinctrl_configs = NULL;
	u32 bank, pins, pin_configs;
	int length, i, ret = 0;

	dev_info(dev, "rkx12x pinctrl init\n");

	// local device
	local_node = of_get_child_by_name(dev->of_node, "serdes-local-device");
	if (IS_ERR_OR_NULL(local_node)) {
		dev_err(dev, "%pOF has no child node: serdes-local-device\n",
				dev->of_node);

		return -ENODEV;
	}

	if (!of_device_is_available(local_node)) {
		dev_info(dev, "%pOF is disabled\n", local_node);

		of_node_put(local_node);
		return -ENODEV;
	}

	/* rkx12x-pins = <bank pins pin_configs>; */
	for_each_child_of_node(local_node, np) {
		if (!of_device_is_available(np))
			continue;

		length = of_property_count_u32_elems(np, pinctrl_name);
		if (length < 0)
			continue;
		if (length % 3) {
			dev_err(dev, "%s: invalid count for pinctrl\n", np->name);
			continue;
		}

		pinctrl_configs = kmalloc_array(length, sizeof(u32), GFP_KERNEL);
		if (!pinctrl_configs)
			continue;

		ret = of_property_read_u32_array(np, pinctrl_name, pinctrl_configs, length);
		if (ret) {
			dev_err(dev, "%s: pinctrl configs data error\n", np->name);
			kfree(pinctrl_configs);
			continue;
		}

		for (i = 0; i < length; i += 3) {
			bank = pinctrl_configs[i + 0];
			pins = pinctrl_configs[i + 1];
			pin_configs = pinctrl_configs[i + 2];

			dev_info(dev, "%s: bank = %d, pins = 0x%08x, pin_configs = 0x%08x\n",
					np->name, bank, pins, pin_configs);

			ret = rkx12x_hwpin_set(&rkx12x->hwpin, bank, pins, pin_configs);
			if (ret)
				dev_err(dev, "%s: pinctrl configs error\n", np->name);
		}

		kfree(pinctrl_configs);
	}

	of_node_put(local_node);

	return 0;
}

/* rkx12x passthrough init */
static int rkx12x_passthrough_init(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	const char *pt_name = "rkx12x-pt";
	struct device_node *local_node = NULL, *np = NULL;
	u32 *pt_configs = NULL;
	u32 func_id = 0, pt_rx = 0;
	int length, i, ret = 0;

	dev_info(dev, "rkx12x passthrough init\n");

	// local device
	local_node = of_get_child_by_name(dev->of_node, "serdes-local-device");
	if (IS_ERR_OR_NULL(local_node)) {
		dev_err(dev, "%pOF has no child node: serdes-local-device\n",
				dev->of_node);

		return -ENODEV;
	}

	if (!of_device_is_available(local_node)) {
		dev_info(dev, "%pOF is disabled\n", local_node);

		of_node_put(local_node);
		return -ENODEV;
	}

	/* rkx12x-pt = <passthrough_func_id passthrough_rx>; */
	for_each_child_of_node(local_node, np) {
		if (!of_device_is_available(np))
			continue;

		length = of_property_count_u32_elems(np, pt_name);
		if (length < 0)
			continue;
		if (length % 2) {
			dev_err(dev, "%s: invalid count for passthrough\n", np->name);
			continue;
		}

		pt_configs = kmalloc_array(length, sizeof(u32), GFP_KERNEL);
		if (!pt_configs)
			continue;

		ret = of_property_read_u32_array(np, pt_name, pt_configs, length);
		if (ret) {
			dev_err(dev, "%s: passthrough configs data error\n", np->name);
			kfree(pt_configs);
			continue;
		}

		for (i = 0; i < length; i += 2) {
			func_id = pt_configs[i + 0];
			pt_rx = pt_configs[i + 1];

			dev_info(dev, "%s: passthrough func_id = %d, pt_rx = %d\n",
					np->name, func_id, pt_rx);

			ret = rkx12x_linkrx_passthrough_cfg(&rkx12x->linkrx, func_id, pt_rx);
			if (ret)
				dev_err(dev, "%s: passthrough func_id = %d config error\n",
						np->name, func_id);
		}

		kfree(pt_configs);
	}

	of_node_put(local_node);

	return 0;
}

/* rkx12x extra init sequence */
static int rkx12x_extra_init_seq_parse(struct device *dev,
		struct device_node *local_node, struct rkx12x_i2c_init_seq *init_seq)
{
	struct device_node *init_seq_node = NULL;

	if (dev == NULL || local_node == NULL || init_seq == NULL)
		return -EINVAL;

	init_seq_node = of_get_child_by_name(local_node, "extra-init-sequence");
	if (IS_ERR_OR_NULL(init_seq_node)) {
		dev_dbg(dev, "%pOF no child node extra-init-sequence\n", local_node);
		return 0;
	}

	if (!of_device_is_available(init_seq_node)) {
		dev_dbg(dev, "%pOF is disabled\n", init_seq_node);

		of_node_put(init_seq_node);
		return 0;
	}

	dev_info(dev, "load extra-init-sequence\n");

	rkx12x_i2c_load_init_seq(dev, init_seq_node, init_seq);

	of_node_put(init_seq_node);

	return 0;
}

static int rkx12x_extra_init_seq_run(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	int ret = 0;

	ret = rkx12x_i2c_run_init_seq(client, &rkx12x->extra_init_seq);
	if (ret) {
		dev_err(dev, "extra init sequence error\n");
		return ret;
	}

	return 0;
}

/* rkx12x i2c mux */
static int rkx12x_i2c_mux_select(struct i2c_mux_core *muxc, u32 chan)
{
	struct rkx12x *rkx12x = i2c_mux_priv(muxc);
	struct device *dev = &rkx12x->client->dev;

	dev_dbg(dev, "rkx12x i2c mux select chan = %d\n", chan);

	/* Channel select is disabled when configured in the disabled state. */
	if (rkx12x->i2c_mux.mux_disable)
		return 0;

	if (rkx12x->i2c_mux.mux_channel == chan)
		return 0;

#if I2CMUX_CH_SEL_EN
	if (rkx12x_linkrx_cmd_select(&rkx12x->linkrx, chan))
		return 0;
#endif

	rkx12x->i2c_mux.mux_channel = chan;

	return 0;
}

static int rkx12x_i2c_mux_deselect(struct i2c_mux_core *muxc, u32 chan)
{
	struct rkx12x *rkx12x = i2c_mux_priv(muxc);
	struct device *dev = &rkx12x->client->dev;

	dev_dbg(dev, "rkx12x i2c mux deselect chan = %d\n", chan);

	/* Channel deselect is disabled when configured in the disabled state. */
	if (rkx12x->i2c_mux.mux_disable)
		return 0;

	return 0;
}

int rkx12x_i2c_mux_disable(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;

	dev_info(dev, "rkx12x i2c mux disable\n");

	rkx12x->i2c_mux.mux_disable = true;

	return 0;
}
EXPORT_SYMBOL(rkx12x_i2c_mux_disable);

int rkx12x_i2c_mux_enable(struct rkx12x *rkx12x, u8 def_mask)
{
	struct device *dev = &rkx12x->client->dev;

	dev_info(dev, "rkx12x i2c mux enable, mask = 0x%02x\n", def_mask);

	rkx12x->i2c_mux.mux_disable = false;
	rkx12x->i2c_mux.mux_channel = -1;

	return 0;
}
EXPORT_SYMBOL(rkx12x_i2c_mux_enable);

static u32 rkx12x_i2c_mux_mask(struct device *dev)
{
	struct device_node *i2c_mux;
	struct device_node *node = NULL;
	u32 i2c_mux_mask = 0;

	/* Balance the of_node_put() performed by of_find_node_by_name(). */
	of_node_get(dev->of_node);
	i2c_mux = of_find_node_by_name(dev->of_node, "i2c-mux");
	if (!i2c_mux) {
		dev_err(dev, "Failed to find i2c-mux node\n");
		return -EINVAL;
	}

	/* Identify which i2c-mux channels are enabled */
	for_each_child_of_node(i2c_mux, node) {
		u32 id = 0;

		of_property_read_u32(node, "reg", &id);
		if (id >= RKX12X_LINK_LANE_MAX)
			continue;

		if (!of_device_is_available(node)) {
			dev_dbg(dev, "Skipping disabled I2C bus port %u\n", id);
			continue;
		}

		i2c_mux_mask |= BIT(id);
	}
	of_node_put(node);
	of_node_put(i2c_mux);

	return i2c_mux_mask;
}

static int rkx12x_i2c_mux_init(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	u32 i2c_mux_mask = 0;
	int i = 0;
	int ret = 0;

	dev_info(dev, "rkx12x i2c mux init\n");

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
		return -ENODEV;

	rkx12x->i2c_mux.muxc = i2c_mux_alloc(client->adapter, dev,
				RKX12X_LINK_LANE_MAX, 0, I2C_MUX_LOCKED,
				rkx12x_i2c_mux_select, rkx12x_i2c_mux_deselect);
	if (!rkx12x->i2c_mux.muxc)
		return -ENOMEM;
	rkx12x->i2c_mux.muxc->priv = rkx12x;

	for (i = 0; i < RKX12X_LINK_LANE_MAX; i++) {
		ret = i2c_mux_add_adapter(rkx12x->i2c_mux.muxc, 0, i, 0);
		if (ret) {
			i2c_mux_del_adapters(rkx12x->i2c_mux.muxc);
			return ret;
		}
	}

	i2c_mux_mask = rkx12x_i2c_mux_mask(dev);
	rkx12x->i2c_mux.i2c_mux_mask = i2c_mux_mask;
	dev_info(dev, "rkx12x i2c mux mask = 0x%x\n", i2c_mux_mask);

	return 0;
}

static int rkx12x_i2c_mux_deinit(struct rkx12x *rkx12x)
{
	if (rkx12x->i2c_mux.muxc)
		i2c_mux_del_adapters(rkx12x->i2c_mux.muxc);

	rkx12x->i2c_mux.i2c_mux_mask = 0;
	rkx12x->i2c_mux.mux_disable = false;
	rkx12x->i2c_mux.mux_channel = -1;

	return 0;
}

/* rkx12x power management */
static int rkx12x_device_power_on(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;

	/*
	 * enable gpio: Simultaneously controlling the power supply of local and remote.
	 *   local and remote simultaneously power on
	 *     1. local chip vcc3v5 power supply
	 *     2. remote chip vcc12V power supply
	 */
	gpiod_set_value(rkx12x->gpio_enable, 1);
	usleep_range(5000, 5100);

	/* local chip reset by reset gpio, remote chip using hardware rc auto reset */
	gpiod_set_value(rkx12x->gpio_reset, 1);
	usleep_range(1000, 1100);
	gpiod_set_value(rkx12x->gpio_reset, 0);

	dev_info(dev, "wait 20ms after reset for power stable\n");
	msleep(20); /* local and remote chip poweron will wait 20ms for power stable */

	return 0;
}

static void rkx12x_device_power_off(struct rkx12x *rkx12x)
{
	/* local and remote simultaneously power off */
	gpiod_set_value(rkx12x->gpio_enable, 0);
	usleep_range(1000, 1100);
}

static int rkx12x_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);
	int ret = 0;

	ret = rkx12x_device_power_on(rkx12x);

	return ret;
}

static int rkx12x_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);

	rkx12x_device_power_off(rkx12x);

	return 0;
}

static const struct dev_pm_ops rkx12x_pm_ops = {
	SET_RUNTIME_PM_OPS(
		rkx12x_runtime_suspend, rkx12x_runtime_resume, NULL)
};

/* rkx12x link preinit */
static struct i2c_client *rkx12x_link_get_ser_client(struct rkx12x_linkrx *linkrx)
{
	struct device *dev = &linkrx->client->dev;
	struct device_node *remote_ser_node = NULL;
	struct i2c_client *remote_ser_client = NULL;

	remote_ser_node = linkrx->remote_ser_node;
	if (IS_ERR_OR_NULL(remote_ser_node)) {
		dev_err(dev, "remote serializer node error\n");
		return NULL;
	}
	remote_ser_client = of_find_i2c_device_by_node(remote_ser_node);
	if (IS_ERR_OR_NULL(remote_ser_client)) {
		dev_err(dev, "remote serializer client error\n");
		return NULL;
	}

	return remote_ser_client;
}

static int rkx12x_ser_pma_set_rate(struct i2c_client *ser_client,
			const struct rkx12x_pma_pll *ser_pma_pll)
{
	return rkx11x_linktx_set_rate(ser_client,
				ser_pma_pll->rate_mode,
				ser_pma_pll->pll_div4,
				ser_pma_pll->pll_div,
				ser_pma_pll->clk_div,
				ser_pma_pll->pll_refclk_div,
				ser_pma_pll->pll_fck_vco_div2,
				ser_pma_pll->force_init_en);
}

static int rkx12x_ser_pma_set_line(struct i2c_client *ser_client, u32 link_line)
{
	return rkx11x_linktx_set_line(ser_client, link_line);
}

int rkx12x_link_set_rate(struct rkx12x_linkrx *linkrx,
			u32 link_rate, bool force_update)
{
	struct i2c_client *des_client = linkrx->client;
	struct device *dev = &des_client->dev;
	struct i2c_client *ser_client = NULL;
	const struct rkx12x_link_rate_pll *pma_pll = NULL;
	int ret = 0, i = 0;

	if (linkrx->link_lock == 0) {
		dev_err(dev, "link isn't locked\n");
		return -EFAULT;
	}

	if (force_update == true) {
		dev_info(dev, "serdes force update link rate = %d\n", link_rate);
	} else {
		if (linkrx->link_rate == link_rate) {
			dev_info(dev, "serdes set the same link rate = %d\n", link_rate);
			return 0;
		}
		dev_info(dev, "serdes set new link rate = %d\n", link_rate);
	}

	/* get serializer client */
	ser_client = rkx12x_link_get_ser_client(linkrx);
	if (ser_client == NULL)
		return -EFAULT;

	/* find pma pll setting */
	pma_pll = NULL;
	for (i = 0; i < ARRAY_SIZE(link_rate_plls); i++) {
		if (link_rate_plls[i].link_rate == link_rate) {
			pma_pll = &link_rate_plls[i];
			break;
		}
	}
	if (pma_pll == NULL) {
		dev_err(dev, "find link pma pll setting error\n");
		return -EINVAL;
	}

	/* serializer pma set rate */
	ret = rkx12x_ser_pma_set_rate(ser_client, &pma_pll->ser_pma_pll);
	if (ret) {
		dev_err(dev, "serializer set link pma pll error\n");
		return -EFAULT;
	}

	/* deserializer pma set rate */
	ret = rkx12x_des_pma_set_rate(linkrx, &pma_pll->des_pma_pll);
	if (ret) {
		dev_err(dev, "deserializer set link pll rate error\n");
		return -EFAULT;
	}

	/* local chip pcs: restart for load pma config */
	ret = rkx12x_des_pcs_enable(linkrx, false, linkrx->lane_id);
	if (ret) {
		dev_err(dev, "deserializer link pcs disable error\n");
		return -EFAULT;
	}

	usleep_range(1000, 2000);

	ret = rkx12x_des_pcs_enable(linkrx, true, linkrx->lane_id);
	if (ret) {
		dev_err(dev, "deserializer link pcs enable error\n");
		return -EFAULT;
	}

	/* local wait lane lock */
	ret = rkx12x_linkrx_wait_lane_lock(linkrx);
	if (ret)
		return -EFAULT;

	linkrx->link_rate = link_rate;

	return 0;
}
EXPORT_SYMBOL(rkx12x_link_set_rate);

int rkx12x_link_set_line(struct rkx12x_linkrx *linkrx, u32 link_line)
{
	struct i2c_client *des_client = linkrx->client;
	struct device *dev = &des_client->dev;
	struct i2c_client *ser_client = NULL;
	int ret = 0;

	if (linkrx->link_lock == 0) {
		dev_err(dev, "link isn't locked\n");
		return -EFAULT;
	}

	/* get serializer client */
	ser_client = rkx12x_link_get_ser_client(linkrx);
	if (ser_client == NULL)
		return -EFAULT;

	/* serializer pma set line config */
	ret = rkx12x_ser_pma_set_line(ser_client, link_line);
	if (ret) {
		dev_err(dev, "serializer link line config error\n");
		return -EFAULT;
	}

	/* deserializer pma set line config */
	ret = rkx12x_des_pma_set_line(linkrx, link_line);
	if (ret) {
		dev_err(dev, "deserializer link line config error\n");
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(rkx12x_link_set_line);

/* rkx12x link preinit */
int rkx12x_link_preinit(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	struct rkx12x_linkrx *linkrx = &rkx12x->linkrx;
	int ret = 0;

	ret = rkx12x_linkrx_wait_lane_lock(linkrx);
	if (ret) {
		dev_err(dev, "rkx12x linkrx wait lane lock error\n");
		return ret;
	}

#if (I2CMUX_CH_SEL_EN == 0)
	ret = rkx12x_linkrx_cmd_select(linkrx, linkrx->lane_id);
	if (ret) {
		dev_err(dev, "rkx12x linkrx cmd select error\n");
		return ret;
	}
#endif /* I2CMUX_CH_SEL_EN */

	ret = rkx12x_link_set_line(linkrx, linkrx->link_line);
	if (ret) {
		dev_err(dev, "rkx12x link set line error\n");
		return ret;
	}

	// note: force_update = false, if link_rate is default, set rate is invalid
	ret = rkx12x_link_set_rate(linkrx, linkrx->link_rate, true);
	if (ret) {
		dev_err(dev, "rkx12x link set rate error\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(rkx12x_link_preinit);

/* rkx12x irq */
static int rkx12x_state_int_cfg(struct rkx12x *rkx12x)
{
#if (IOMUX_PINSET_API_EN == 0)
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	u32 reg, val = 0;
	int ret = 0;

	dev_info(&client->dev, "rkx12x state int config\n");

	/* rkx12x gpio0a4_int_tx iomux */
	reg = DES_GRF_GPIO0A_IOMUX_L;
	val = HIWORD_UPDATE(GPIO0A4_INT_TX, GPIO0A4_MASK, GPIO0A4_SHIFT);
	ret = rkx12x->i2c_reg_write(client, reg, val);
	if (ret) {
		dev_err(dev, "rkx12x GPIO0A4_INT_TX iomux error\n");
		return ret;
	}
#else
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	int ret = 0;

	dev_info(&client->dev, "rkx12x state int config\n");

	ret = rkx12x_hwpin_set(&rkx12x->hwpin, RKX12X_DES_GPIO_BANK0,
				RKX12X_GPIO_PIN_A4, RKX12X_PIN_CONFIG_MUX_FUNC2);
	if (ret) {
		dev_err(dev, "rkx12x state int tx iomux error\n");
		return ret;
	}
#endif /* IOMUX_PINSET_API_EN */

	return 0;
}

static int rkx12x_module_irq_enable(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	struct rkx12x_linkrx *linkrx = &rkx12x->linkrx;
	int ret = 0;

	if (rkx12x->state_irq < 0)
		return -1;

	dev_info(dev, "=== rkx12x module irq enable ===\n");

	ret = rkx12x_state_int_cfg(rkx12x);
	if (ret)
		return ret;

	/* enable pcs irq */
	ret |= rkx12x_des_pcs_irq_enable(linkrx, linkrx->lane_id);

	/* enable pma irq */
	ret |= rkx12x_des_pma_irq_enable(linkrx, linkrx->lane_id);

	/* enable link irq */
	ret |= rkx12x_linkrx_irq_enable(linkrx);

	return ret;
}

static __maybe_unused int rkx12x_module_irq_disable(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	struct rkx12x_linkrx *linkrx = &rkx12x->linkrx;
	int ret = 0;

	if (rkx12x->state_irq < 0)
		return -1;

	dev_info(dev, "=== rkx12x module irq disable ===\n");

	/* disable pcs irq */
	ret |= rkx12x_des_pcs_irq_disable(linkrx, linkrx->lane_id);

	/* disable pma adapt irq */
	ret |= rkx12x_des_pma_irq_disable(linkrx, linkrx->lane_id);

	/* disable link irq */
	ret |= rkx12x_linkrx_irq_disable(linkrx);

	return ret;
}

static int rkx12x_module_irq_handler(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	struct rkx12x_linkrx *linkrx = &rkx12x->linkrx;
	u32 mask = 0, status = 0;
	int ret = 0, i = 0;

	ret = rkx12x->i2c_reg_read(client, DES_GRF_IRQ_EN, &mask);
	if (ret)
		return ret;
	ret = rkx12x->i2c_reg_read(client, DES_GRF_IRQ_STATUS, &status);
	if (ret)
		return ret;

	dev_info(dev, "des module irq status: 0x%08x\n", status);

	i = 0;
	status &= mask;
	while (status) {
		switch (status & BIT(i)) {
		case DES_IRQ_PCS0:
			ret |= rkx12x_des_pcs_irq_handler(linkrx, 0);
			break;
		case DES_IRQ_PCS1:
			ret |= rkx12x_des_pcs_irq_handler(linkrx, 1);
			break;
		case DES_IRQ_EFUSE:
			/* TBD */
			break;
		case DES_IRQ_GPIO0:
			/* TBD */
			break;
		case DES_IRQ_GPIO1:
			/* TBD */
			break;
		case DES_IRQ_CSITX0:
			/* TBD */
			break;
		case DES_IRQ_CSITX1:
			/* TBD */
			break;
		case DES_IRQ_MIPI_DSI_HOST:
			/* TBD */
			break;
		case DES_IRQ_PMA_ADAPT0:
			ret |= rkx12x_des_pma_irq_handler(linkrx, 0);
			break;
		case DES_IRQ_PMA_ADAPT1:
			ret |= rkx12x_des_pma_irq_handler(linkrx, 1);
			break;
		case DES_IRQ_REMOTE:
			/* TBD */
			break;
		case DES_IRQ_PWM:
			/* TBD */
			break;
		case DES_IRQ_DVP_TX:
			/* TBD */
			break;
		case DES_IRQ_LINK:
			ret |= rkx12x_linkrx_irq_handler(linkrx);
			break;
		case DES_IRQ_EXT:
			/* TBD */
			break;
		default:
			break;
		}

		status &= ~BIT(i);
		i++;
	}

	return ret;
}

static irqreturn_t rkx12x_state_irq_handler(int irq, void *arg)
{
	struct rkx12x *rkx12x = arg;
	struct device *dev = &rkx12x->client->dev;
	int state_gpio_level = 0;
	int ret = 0;

	mutex_lock(&rkx12x->mutex);

	if (rkx12x->streaming) {
		state_gpio_level = gpiod_get_value_cansleep(rkx12x->gpio_irq);
		dev_info(dev, "rkx12x state int gpio level: %d\n", state_gpio_level);

		ret = rkx12x_module_irq_handler(rkx12x);
		if (ret)
			dev_warn(dev, "rkx12x module irq handler error\n");
	}

	mutex_unlock(&rkx12x->mutex);

	return IRQ_HANDLED;
}

static int rkx12x_state_irq_init(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	int ret = 0;

	dev_info(dev, "rkx12x state irq init\n");

	if (!IS_ERR(rkx12x->gpio_irq)) {
		rkx12x->state_irq = gpiod_to_irq(rkx12x->gpio_irq);
		if (rkx12x->state_irq < 0)
			return dev_err_probe(dev, rkx12x->state_irq, "failed to get irq\n");

		irq_set_status_flags(rkx12x->state_irq, IRQ_NOAUTOEN);
		ret = devm_request_threaded_irq(dev, rkx12x->state_irq, NULL,
						rkx12x_state_irq_handler,
						IRQF_TRIGGER_LOW | IRQF_ONESHOT,
						"rkx12x-irq", rkx12x);
		if (ret) {
			dev_err(dev, "failed to request rkx12x interrupt\n");
			rkx12x->state_irq = -1;
			return ret;
		}
	} else {
		dev_warn(dev, "no support rkx12x irq function\n");
		rkx12x->state_irq = -1;
		return -1;
	}

	return 0;
}

/* rkx12x module hw init */
int rkx12x_module_hw_init(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	int ret = 0;

	dev_info(dev, "=== rkx12x module hw init ===\n");

	/* cru clock init */
	ret = rkx12x_clk_hw_init(rkx12x);
	if (ret != 0) {
		dev_err(dev, "%s: clk hw init error\n", __func__);
		return ret;
	}

	/* txphy init */
	ret = rkx12x_txphy_hw_init(rkx12x);
	if (ret) {
		dev_err(dev, "%s: txphy hw init error\n", __func__);
		return ret;
	}

	/* csi2tx init */
	if (rkx12x->stream_interface == RKX12X_STREAM_INTERFACE_MIPI) {
		ret = rkx12x_csi2tx_hw_init(rkx12x);
		if (ret) {
			dev_err(dev, "%s: csi2tx hw init error\n", __func__);
			return ret;
		}
	}

	/* dvp iomux config */
	if (rkx12x->stream_interface == RKX12X_STREAM_INTERFACE_DVP) {
		ret = rkx12x_dvp_iomux_cfg(rkx12x);
		if (ret) {
			dev_err(dev, "%s: dvp iomux error\n", __func__);
			return ret;
		}

		ret = rkx12x_dvptx_hw_init(rkx12x);
		if (ret) {
			dev_err(dev, "%s: dvptx hw init error\n", __func__);
			return ret;
		}
	}

	/* linkrx init */
	ret = rkx12x_linkrx_hw_init(rkx12x);
	if (ret) {
		dev_err(dev, "%s: linkrx hw init error\n", __func__);
		return ret;
	}

	/* pinctrl init */
	ret = rkx12x_pinctrl_init(rkx12x);
	if (ret) {
		dev_err(dev, "%s: pinctrl init error\n", __func__);
		return ret;
	}

	/* passthrough init */
	ret = rkx12x_passthrough_init(rkx12x);
	if (ret) {
		dev_err(dev, "%s: passthrough init error\n", __func__);
		return ret;
	}

	/* extra init seq run */
	ret = rkx12x_extra_init_seq_run(rkx12x);
	if (ret) {
		dev_err(dev, "%s: run extra init seq error\n", __func__);
		return ret;
	}

	/* rkx12x module irq */
	ret = rkx12x_module_irq_enable(rkx12x);
	if (ret) {
		dev_err(dev, "%s: module irq enable error\n", __func__);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(rkx12x_module_hw_init);

static int rkx12x_parse_dt(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	struct device_node *local_node = NULL;
	u32 value = 0;
	int ret = 0;

	dev_info(dev, "=== rkx12x parse dt ===\n");

	// local device
	local_node = of_get_child_by_name(dev->of_node, "serdes-local-device");
	if (IS_ERR_OR_NULL(local_node)) {
		dev_err(dev, "%pOF has no child node: serdes-local-device\n",
				dev->of_node);

		return -ENODEV;
	}

	if (!of_device_is_available(local_node)) {
		dev_info(dev, "%pOF is disabled\n", local_node);

		of_node_put(local_node);
		return -ENODEV;
	}

	// stream port
	ret = of_property_read_u32(local_node, "stream-port", &value);
	if (ret == 0) {
		dev_info(dev, "stream-port property: %d", value);
		rkx12x->stream_port = value;
	}

	// stream interface
	ret = of_property_read_u32(local_node, "stream-interface", &value);
	if (ret == 0) {
		dev_info(dev, "stream-interface property: %d", value);
		rkx12x->stream_interface = value;
	}

	// serdes dclk
	ret = of_property_read_u32(local_node, "serdes-dclk-hz", &value);
	if (ret == 0) {
		dev_info(dev, "serdes-dclk-hz property: %d", value);
		rkx12x->serdes_dclk_hz = value;
	}

	// txphy data parse
	rkx12x_txphy_parse_dt(dev, local_node, &rkx12x->txphy);

	// csi2tx data parse
	if (rkx12x->stream_interface == RKX12X_STREAM_INTERFACE_MIPI)
		rkx12x_csi2tx_parse_dt(dev, local_node, &rkx12x->csi2tx);

	// dvptx data parse
	if (rkx12x->stream_interface == RKX12X_STREAM_INTERFACE_DVP)
		rkx12x_dvptx_parse_dt(dev, local_node, &rkx12x->dvptx);

	/* linkrx data parse */
	rkx12x_linkrx_parse_dt(dev, local_node, &rkx12x->linkrx);

	/* extra init seq parse dt */
	rkx12x_extra_init_seq_parse(dev, local_node, &rkx12x->extra_init_seq);

	of_node_put(local_node);

	return 0;
}

static int rkx12x_module_data_init(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	int ret = 0;

	dev_info(dev, "=== rkx12x data init ===\n");

	// default init
	rkx12x->stream_port = RKX12X_STREAM_PORT0;
	rkx12x->stream_interface = RKX12X_STREAM_INTERFACE_MIPI;
	rkx12x->serdes_dclk_hz = (200 * 1000 * 1000);

	rkx12x->txphy.clock_mode = RKX12X_TXPHY_CLK_CONTINUOUS;

	rkx12x->csi2tx.format = RKX12X_CSI2TX_FMT_PIXEL128;

	rkx12x->dvptx.clock_invert = 0;

	rkx12x->linkrx.link_lock = 0;
	rkx12x->linkrx.link_rate = RKX12X_LINK_RATE_2GBPS_83M;
	rkx12x->linkrx.link_line = RKX12X_LINK_LINE_COMMON_CONFIG;
	rkx12x->linkrx.engine_id = 0;
	rkx12x->linkrx.engine_delay = 0;
	rkx12x->linkrx.video_packet_size = 0;
	rkx12x->linkrx.remote_cam_node = NULL;
	rkx12x->linkrx.remote_ser_node = NULL;

	ret = rkx12x_parse_dt(rkx12x);
	if (ret) {
		dev_err(dev, "rkx12x parse dt error\n");
		return ret;
	}

	return 0;
}

static int rkx12x_module_sw_init(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	int ret = 0;

	dev_info(dev, "=== rkx12x module sw init ===\n");

	/* add cru clock */
	ret = rkx12x_add_hwclk(rkx12x);
	if (ret != 0) {
		dev_err(dev, "%s: hw clk add error\n", __func__);
		return ret;
	}

	/* add hwpin */
	ret = rkx12x_add_hwpin(rkx12x);
	if (ret != 0) {
		dev_err(dev, "%s: hw pin add error\n", __func__);
		return ret;
	}

	/* add txphy */
	ret = rkx12x_add_txphy(rkx12x);
	if (ret != 0) {
		dev_err(dev, "%s: txphy add error\n", __func__);
		return ret;
	}

	if (rkx12x->stream_interface == RKX12X_STREAM_INTERFACE_MIPI) {
		/* add csi2tx */
		ret = rkx12x_add_csi2tx(rkx12x);
		if (ret != 0) {
			dev_err(dev, "%s: csi2tx add error\n", __func__);
			return ret;
		}
	}

	if (rkx12x->stream_interface == RKX12X_STREAM_INTERFACE_DVP) {
		/* add dvptx */
		ret = rkx12x_add_dvptx(rkx12x);
		if (ret != 0) {
			dev_err(dev, "%s: dvptx add error\n", __func__);
			return ret;
		}
	}

	/* add linkrx */
	ret = rkx12x_add_linkrx(rkx12x);
	if (ret != 0) {
		dev_err(dev, "%s: linkrx add error\n", __func__);
		return ret;
	}

	return 0;
}

static int rkx12x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct rkx12x *rkx12x = NULL;
	u32 chip_id;
	int ret = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x", DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8, DRIVER_VERSION & 0x00ff);

	chip_id = (uintptr_t)of_device_get_match_data(dev);
	if (chip_id == RKX12X_DES_CHIP_ID_V0) {
		dev_info(dev, "rockchip deserializer driver for rkx120\n");
	} else if (chip_id == RKX12X_DES_CHIP_ID_V1) {
		dev_info(dev, "rockchip deserializer driver for rkx121\n");
	} else {
		dev_err(dev, "rockchip deserializer driver unknown chip\n");
		return -EINVAL;
	}

	rkx12x = devm_kzalloc(dev, sizeof(*rkx12x), GFP_KERNEL);
	if (!rkx12x)
		return -ENOMEM;

	rkx12x->client = client;
	rkx12x->chip_id = chip_id;

	/* sensor name */
	ret = of_property_read_string(node, "rockchip,camera-module-sensor-name",
					&rkx12x->sensor_name);
	if (ret)
		rkx12x->sensor_name = RKX12X_NAME;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
					&rkx12x->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
					&rkx12x->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
					&rkx12x->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
					&rkx12x->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	/* request local chip power gpio */
	rkx12x->gpio_enable = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(rkx12x->gpio_enable)) {
		ret = PTR_ERR(rkx12x->gpio_enable);
		dev_err(dev, "failed to request enable GPIO: %d\n", ret);
		return ret;
	}

	/* request local chip reset gpio */
	rkx12x->gpio_reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(rkx12x->gpio_reset)) {
		ret = PTR_ERR(rkx12x->gpio_reset);
		dev_err(dev, "failed to request reset GPIO: %d\n", ret);
		return ret;
	}

	rkx12x->gpio_irq = devm_gpiod_get_optional(dev, "irq", GPIOD_IN);
	if (IS_ERR(rkx12x->gpio_irq)) {
		ret = PTR_ERR(rkx12x->gpio_irq);
		dev_err(dev, "failed to request irq GPIO: %d\n", ret);
		return ret;
	}

	rkx12x_callback_init(rkx12x);

	mutex_init(&rkx12x->mutex);

	ret = rkx12x_device_power_on(rkx12x);
	if (ret)
		goto err_destroy_mutex;

	pm_runtime_set_active(dev);
	pm_runtime_get_noresume(dev);
	pm_runtime_enable(dev);

	/* check local chip id */
	ret = rkx12x_check_chip_id(rkx12x);
	if (ret != 0)
		goto err_power_off;

	ret = rkx12x_module_data_init(rkx12x);
	if (ret)
		goto err_power_off;

	ret = rkx12x_debugfs_init(rkx12x);
	if (ret)
		goto err_power_off;

	/*
	 * client->dev->driver_data = subdev
	 * subdev->dev->driver_data = rkx12x
	 */
	ret = rkx12x_v4l2_subdev_init(rkx12x);
	if (ret) {
		dev_err(dev, "rkx12x probe v4l2 subdev init error\n");
		goto err_dbgfs_deinit;
	}

	ret = rkx12x_module_sw_init(rkx12x);
	if (ret)
		goto err_subdev_deinit;

	/* i2c mux init, add remote devices */
	ret = rkx12x_i2c_mux_init(rkx12x);
	if (ret)
		goto err_subdev_deinit;

#if I2CMUX_CH_SEL_EN
	/* i2c mux enable: default disable all channel */
	rkx12x_i2c_mux_enable(rkx12x, 0x00);
#else
	/* i2c mux disable select and deselect */
	rkx12x_i2c_mux_disable(rkx12x);
#endif /* I2CMUX_CH_SEL_EN */

	rkx12x_state_irq_init(rkx12x);

	pm_runtime_set_autosuspend_delay(dev, 1000);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;

err_subdev_deinit:
	rkx12x_v4l2_subdev_deinit(rkx12x);

err_dbgfs_deinit:
	rkx12x_debugfs_deinit(rkx12x);

err_power_off:
	pm_runtime_disable(dev);
	pm_runtime_put_noidle(dev);
	rkx12x_device_power_off(rkx12x);

err_destroy_mutex:
	mutex_destroy(&rkx12x->mutex);

	return ret;
}

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
static int rkx12x_i2c_remove(struct i2c_client *client)
#else
static void rkx12x_i2c_remove(struct i2c_client *client)
#endif
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);

	rkx12x_debugfs_deinit(rkx12x);

	rkx12x_v4l2_subdev_deinit(rkx12x);

	rkx12x_i2c_mux_deinit(rkx12x);

	mutex_destroy(&rkx12x->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		rkx12x_device_power_off(rkx12x);
	pm_runtime_set_suspended(&client->dev);

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static const struct of_device_id rkx12x_of_match[] = {
	{
		.compatible = "rockchip,des,rkx120",
		.data = (const void *)RKX12X_DES_CHIP_ID_V0
	}, {
		.compatible = "rockchip,des,rkx121",
		.data = (const void *)RKX12X_DES_CHIP_ID_V1
	}, {
		.compatible = "rockchip,des,rk682",
		.data = (const void *)RKX12X_DES_CHIP_ID_V1
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rkx12x_of_match);

static struct i2c_driver rkx12x_i2c_driver = {
	.driver = {
		.name = "rkx12x_des",
		.pm = &rkx12x_pm_ops,
		.of_match_table = of_match_ptr(rkx12x_of_match),
	},
	.probe = rkx12x_i2c_probe,
	.remove = rkx12x_i2c_remove,
};

module_i2c_driver(rkx12x_i2c_driver);

MODULE_AUTHOR("Cai Wenzhong <cwz@rock-chips.com>");
MODULE_DESCRIPTION("Rockchip rkx12x deserializer driver");
MODULE_LICENSE("GPL");
