/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#ifndef __RKX12X_DVPTX_H__
#define __RKX12X_DVPTX_H__

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>

struct rkx12x_dvptx {
	u32 width;
	u32 height;
	u32 pclk_freq; // pclk
	u32 clock_invert;

	/* register read/write api */
	struct i2c_client *client;
	int (*i2c_reg_read)(struct i2c_client *client, u32 reg, u32 *val);
	int (*i2c_reg_write)(struct i2c_client *client, u32 reg, u32 val);
	int (*i2c_reg_update)(struct i2c_client *client, u32 reg, u32 mask, u32 val);
};

/* rkx12x dvptx api */
int rkx12x_dvptx_init(struct rkx12x_dvptx *dvptx);

#endif /* __RKX12X_DVPTX_H__ */
