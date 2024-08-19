/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#ifndef __RKX12X_CSI2TX_H__
#define __RKX12X_CSI2TX_H__

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>

enum {
	RKX12X_CSI2TX_ID0 = 0,
	RKX12X_CSI2TX_ID1,
	RKX12X_CSI2TX_ID_MAX
};

enum {
	RKX12X_CSI2TX_DPHY = 0,
	RKX12X_CSI2TX_CPHY
};

enum {
	RKX12X_CSI2TX_CLK_CONTINUOUS = 0,
	RKX12X_CSI2TX_CLK_NON_CONTINUOUS
};

enum {
	RKX12X_CSI2TX_FMT_RAW8 = 0,
	RKX12X_CSI2TX_FMT_RAW10,
	RKX12X_CSI2TX_FMT_PIXEL10,
	RKX12X_CSI2TX_FMT_PIXEL128
};

struct rkx12x_csi2tx {
	u32 phy_id;
	u32 phy_mode;
	u32 clock_mode;
	u32 data_lanes;
	u32 format;

	/* register read/write api */
	struct i2c_client *client;
	int (*i2c_reg_read)(struct i2c_client *client, u32 reg, u32 *val);
	int (*i2c_reg_write)(struct i2c_client *client, u32 reg, u32 val);
	int (*i2c_reg_update)(struct i2c_client *client, u32 reg, u32 mask, u32 val);
};

/* rkx12x csi2tx api */
int rkx12x_csi2tx_init(struct rkx12x_csi2tx *csi2tx);

#endif /* __RKX12X_CSI2TX_H__ */
