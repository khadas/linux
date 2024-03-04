// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>

#include "rkx11x_rxphy.h"
#include "rkx11x_reg.h"
#include "rkx11x_compact.h"

#define PSEC_PER_SEC		1000000000000LL

/* mipi rx phy base */
#define MIPI_RX0_PHY_BASE	RKX11X_MIPI_LVDS_RX_PHY0_BASE
#define MIPI_RX1_PHY_BASE	RKX11X_MIPI_LVDS_RX_PHY1_BASE

#define MIPI_RXPHY_DEV(id)	((id) == 0 ? (MIPI_RX0_PHY_BASE) : (MIPI_RX1_PHY_BASE))

/* mipi rx phy register */
#define RXPHY_SOFT_RST		0x0000
#define HS_CLK_SOFT_RST		BIT(1)
#define CFG_CLK_SOFT_RST	BIT(0)

#define RXPHY_PHY_RCAL		0x0004
#define RCAL_DONE		BIT(17)
#define RCAL_OUT(x)		UPDATE(x, 16, 13)
#define RCAL_CTL(x)		UPDATE(x, 12, 5)
#define RCAL_TRIM(x)		UPDATE(x, 4, 1)
#define RCAL_EN			BIT(0)

#define RXPHY_ULP_RX_EN		0x0008

#define RXPHY_VOFFCAL_OUT	0x000c
#define CSI_CLK_VOFFCAL_DONE	BIT(29)
#define CSI_CLK_VOFFCAL_OUT(x)	UPDATE(x, 28, 24)
#define CSI_0_VOFFCAL_DONE	BIT(23)
#define CSI_0_VOFFCAL_OUT(x)	UPDATE(x, 22, 18)
#define CSI_1_VOFFCAL_DONE	BIT(17)
#define CSI_1_VOFFCAL_OUT(x)	UPDATE(x, 16, 12)
#define CSI_2_VOFFCAL_DONE	BIT(11)
#define CSI_2_VOFFCAL_OUT(x)	UPDATE(x, 10, 6)
#define CSI_3_VOFFCAL_DONE	BIT(5)
#define CSI_3_VOFFCAL_OUT(x)	UPDATE(x, 4, 0)

#define RXPHY_CSI_CTL01		0x0010
#define CSI_CTL1(x)		UPDATE(x, 31, 16)
#define CSI_CTL0(x)		UPDATE(x, 15, 0)

#define RXPHY_CSI_CTL23		0x0014
#define CSI_CTL3(x)		UPDATE(x, 31, 16)
#define CSI_CTL2(x)		UPDATE(x, 15, 0)

#define RXPHY_CSI_VINIT		0x001c
#define CSI_LPRX_VREF_TRIM	UPDATE(x, 23, 20)
#define CSI_CLK_LPRX_VINIT(x)	UPDATE(x, 19, 16)
#define CSI_3_LPRX_VINIT(x)	UPDATE(x, 15, 12)
#define CSI_2_LPRX_VINIT(x)	UPDATE(x, 11, 8)
#define CSI_1_LPRX_VINIT(x)	UPDATE(x, 7, 4)
#define CSI_0_LPRX_VINIT(x)	UPDATE(x, 3, 0)

#define RXPHY_CLANE_PARA	0x0020
#define T_CLK_TERM_EN(x)	UPDATE(x, 15, 8)
#define T_CLK_SETTLE(x)		UPDATE(x, 7, 0)

#define RXPHY_T_HS_TERMEN	0x0024
#define T_D3_TERMEN(x)		UPDATE(x, 31, 24)
#define T_D2_TERMEN(x)		UPDATE(x, 23, 16)
#define T_D1_TERMEN(x)		UPDATE(x, 15, 8)
#define T_D0_TERMEN(x)		UPDATE(x, 7, 0)

#define RXPHY_T_HS_SETTLE	0x0028
#define T_D3_SETTLE(x)		UPDATE(x, 31, 24)
#define T_D2_SETTLE(x)		UPDATE(x, 23, 16)
#define T_D1_SETTLE(x)		UPDATE(x, 15, 8)
#define T_D0_SETTLE(x)		UPDATE(x, 7, 0)

#define RXPHY_T_CLANE_INIT	0x0030
#define T_CLK_INIT(x)		UPDATE(x, 23, 0)

#define RXPHY_T_LANE0_INIT	0x0034
#define T_D0_INIT(x)		UPDATE(x, 23, 0)

#define RXPHY_T_LANE1_INIT	0x0038
#define T_D1_INIT(x)		UPDATE(x, 23, 0)

#define RXPHY_T_LANE2_INIT	0x003c
#define T_D2_INIT(x)		UPDATE(x, 23, 0)

#define RXPHY_T_LANE3_INIT	0x0040
#define T_D3_INIT(x)		UPDATE(x, 23, 0)

#define RXPHY_TLPX_CTRL		0x0044
#define EN_TLPX_CHECK		BIT(8)
#define TLPX(x)			UPDATE(x, 7, 0)

#define RXPHY_NE_SWAP		0x0048
#define DPDN_SWAP_LANE(x)	UPDATE(1 << x, 11, 8)
#define LANE_SWAP_LAN3(x)	UPDATE(x, 7, 6)
#define LANE_SWAP_LANE2(x)	UPDATE(x, 5, 4)
#define LANE_SWAP_LANE1(x)	UPDATE(x, 3, 2)
#define LANE_SWAP_LANE0(x)	UPDATE(x, 1, 0)

#define RXPHY_MISC_INFO		0x004c
#define ULPS_LP10_SEL		BIT(1)
#define LONG_SOTSYNC_EN	BIT(0)

/* mipi rx grf base */
#define MIPI_RX0_GRF_BASE	RKX11X_GRF_MIPI0_BASE
#define MIPI_RX1_GRF_BASE	RKX11X_GRF_MIPI1_BASE

#define GRF_RXPHY_DEV(id)	((id) == 0 ? (MIPI_RX0_GRF_BASE) : (MIPI_RX1_GRF_BASE))

#define GRF_MIPI_RX_CON0	0x0000
#define LVDS_RX_CK_RTRM(x)	HIWORD_UPDATE(x, GENMASK(15, 12), 12)
#define LVDS_RX3_PD		HIWORD_UPDATE(1, BIT(10), 10)
#define LVDS_RX2_PD		HIWORD_UPDATE(1, BIT(9), 9)
#define LVDS_RX1_PD		HIWORD_UPDATE(1, BIT(8), 8)
#define LVDS_RX0_PD		HIWORD_UPDATE(1, BIT(7), 7)
#define LVDS_RX_CK_PD		HIWORD_UPDATE(1, BIT(6), 6)
#define DATA_LANE3_EN		HIWORD_UPDATE(1, BIT(5), 5)
#define DATA_LANE2_EN		HIWORD_UPDATE(1, BIT(4), 4)
#define DATA_LANE1_EN		HIWORD_UPDATE(1, BIT(3), 3)
#define DATA_LANE0_EN		HIWORD_UPDATE(1, BIT(2), 2)
#define PHY_MODE(x)		HIWORD_UPDATE(x, GENMASK(1, 0), 0)

#define GRF_MIPI_RX_CON(x)	(0x0000 + (x) * 4)

#define GRF_SOC_STATUS		0x0580

static int rkx11x_rxphy_dvp_init(struct rkx11x_rxphy *rxphy)
{
	struct i2c_client *client = rxphy->client;
	struct device *dev = &client->dev;
	u32 grf_mipi_base = 0;
	u32 reg = 0, val = 0;
	int ret = 0;

	/* dvp camera port only in rxphy 0 */
	if (rxphy->phy_id != RKX11X_RXPHY_0) {
		dev_err(dev, "RXPHY: phy_id = %d error\n", rxphy->phy_id);
		return -EINVAL;
	}

	grf_mipi_base = GRF_RXPHY_DEV(rxphy->phy_id);

	/* rxphy mode select GPIO */
	reg = grf_mipi_base + GRF_MIPI_RX_CON0;
	val = PHY_MODE(RKX11X_RXPHY_MODE_GPIO);
	ret = rxphy->i2c_reg_write(client, reg, val);
	if (ret) {
		dev_err(dev, "RXPHY: phy_id = %d phy mode set error\n", rxphy->phy_id);
		return ret;
	}

	return 0;
}

static int rkx11x_rxphy_mipi_init(struct rkx11x_rxphy *rxphy)
{
	struct i2c_client *client = rxphy->client;
	struct device *dev = &client->dev;
	u32 grf_mipi_base, rx_phy_base;
	u32 cfg_clk = 100 * 1000 * 1000; /* config clock = 100MHz */
	u64 t_cfg_clk = PSEC_PER_SEC / cfg_clk;
	u32 init_time = 100; /* 100 us */
	u32 reg = 0, val = 0;
	int ret = 0;

	if (rxphy->phy_id >= RKX11X_RXPHY_MAX) {
		dev_err(dev, "RXPHY: phy_id = %d error!\n", rxphy->phy_id);
		return -1;
	}

	grf_mipi_base = GRF_RXPHY_DEV(rxphy->phy_id);
	rx_phy_base = MIPI_RXPHY_DEV(rxphy->phy_id);

	/*
	 * 1. Configure phy mode for DPHY RX mode by writing GRF_MIPI_CON0 register
	 * 2. Enable data lane by writing GRF_MIPI_CON0 register,
	 *    min one lane enable, max four lane enable.
	 * 3. Configure initialization time for clock and data lane if necessary
	 *    by changing t_clane_init and t_laneN_init registers
	 */

	/* RXPHY mode select MIPI */
	val = PHY_MODE(RKX11X_RXPHY_MODE_MIPI);

	/* config data lanes */
	switch (rxphy->data_lanes) {
	case 4:
		val |= DATA_LANE3_EN;
		fallthrough;
	case 3:
		val |= DATA_LANE2_EN;
		fallthrough;
	case 2:
		val |= DATA_LANE1_EN;
		fallthrough;
	case 1:
	default:
		val |= DATA_LANE0_EN;
		break;
	}

	reg = grf_mipi_base + GRF_MIPI_RX_CON0;
	ret = rxphy->i2c_reg_write(client, reg, val);
	if (ret) {
		dev_err(dev, "RXPHY: phy_id = %d rxphy mode set error\n",
				rxphy->phy_id);
		return ret;
	}

	/* INIT: 100us, base on 100MHz */
	val = init_time * (PSEC_PER_SEC / USEC_PER_SEC) / t_cfg_clk;
	dev_info(dev, "RXPHY: init time = %d us, val = %d\n", init_time, val);

	reg = rx_phy_base + RXPHY_T_CLANE_INIT;
	ret |= rxphy->i2c_reg_write(client, reg, val);

	reg = rx_phy_base + RXPHY_T_LANE0_INIT;
	ret |= rxphy->i2c_reg_write(client, reg, val);

	reg = rx_phy_base + RXPHY_T_LANE1_INIT;
	ret |= rxphy->i2c_reg_write(client, reg, val);

	reg = rx_phy_base + RXPHY_T_LANE2_INIT;
	ret |= rxphy->i2c_reg_write(client, reg, val);

	reg = rx_phy_base + RXPHY_T_LANE3_INIT;
	ret |= rxphy->i2c_reg_write(client, reg, val);
	if (ret) {
		dev_err(dev, "RXPHY: phy_id = %d timing set error\n",
				rxphy->phy_id);
		return ret;
	}

	return 0;
}

int rkx11x_rxphy_init(struct rkx11x_rxphy *rxphy)
{
	int ret = 0;

	if ((rxphy == NULL)
			|| (rxphy->client == NULL)
			|| (rxphy->i2c_reg_read == NULL)
			|| (rxphy->i2c_reg_write == NULL)
			|| (rxphy->i2c_reg_update == NULL))
		return -EINVAL;

	switch (rxphy->phy_mode) {
	case RKX11X_RXPHY_MODE_GPIO:
		ret = rkx11x_rxphy_dvp_init(rxphy);
		break;
	case RKX11X_RXPHY_MODE_MIPI:
		ret = rkx11x_rxphy_mipi_init(rxphy);
		break;
	default:
		dev_info(&rxphy->client->dev, "rxphy mode = %d error\n",
				rxphy->phy_mode);
		ret = -1;
		break;
	}

	return ret;
}
