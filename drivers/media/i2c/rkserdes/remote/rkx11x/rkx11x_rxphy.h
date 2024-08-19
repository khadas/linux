/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co. Ltd.
 *
 */

#ifndef __RKX11X_RXPHY_H__
#define __RKX11X_RXPHY_H__

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>

/* rkx11x rxphy id */
enum rkx11x_rxphy_id {
	RKX11X_RXPHY_0 = 0,
	RKX11X_RXPHY_1,
	RKX11X_RXPHY_MAX
};

/* rkx11x rxphy mode */
enum rkx11x_rxphy_mode {
	RKX11X_RXPHY_MODE_GPIO = 0,
	RKX11X_RXPHY_MODE_LVDS_DISPLAY = 1,
	RKX11X_RXPHY_MODE_MIPI = 2,
	RKX11X_RXPHY_MODE_LVDS_CAMERA = 3
};

struct rkx11x_rxphy {
	u32 phy_id;
	u32 phy_mode; // 0: GPIO, 1: LVDS Display, 2: MIPI, 3: LVDS Camera
	u32 data_lanes; // data lanes: 1 ~ 4

	/* register read/write api */
	struct i2c_client *client;
	int (*i2c_reg_read)(struct i2c_client *client, u32 reg, u32 *val);
	int (*i2c_reg_write)(struct i2c_client *client, u32 reg, u32 val);
	int (*i2c_reg_update)(struct i2c_client *client, u32 reg, u32 mask, u32 val);
};

int rkx11x_rxphy_init(struct rkx11x_rxphy *rxphy);

#endif /* __RKX11X_RXPHY_H__ */
