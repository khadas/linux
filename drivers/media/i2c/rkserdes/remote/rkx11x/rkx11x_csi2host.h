/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#ifndef __RKX11X_CSI2HOST_H__
#define __RKX11X_CSI2HOST_H__

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>

/* rkx11x csi2host id */
enum {
	RKX11X_CSI2HOST_0 = 0,
	RKX11X_CSI2HOST_1,
	RKX11X_CSI2HOST_MAX
};

enum {
	RKX11X_CSI2HOST_CSI = 0,
	RKX11X_CSI2HOST_DSI
};

enum {
	RKX11X_CSI2HOST_DPHY = 0,
	RKX11X_CSI2HOST_CPHY
};

struct rkx11x_csi2host {
	u32 id;
	u32 data_lanes; // data lanes: 1 ~ 4
	u32 interface; // 0: CSI2, 1: DSI
	u32 phy_mode; // 0: DPHY, 1: CPHY

	/* register read/write api */
	struct i2c_client *client;
	int (*i2c_reg_read)(struct i2c_client *client, u32 reg, u32 *val);
	int (*i2c_reg_write)(struct i2c_client *client, u32 reg, u32 val);
	int (*i2c_reg_update)(struct i2c_client *client, u32 reg, u32 mask, u32 val);
};

int rkx11x_csi2host_init(struct rkx11x_csi2host *csi2host);

#endif /* __RKX11X_CSI2HOST_H__ */
