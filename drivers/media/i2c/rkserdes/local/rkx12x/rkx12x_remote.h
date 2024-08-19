/* SPDX-License-Identifier: GPL-2.0 */
/*
 * RKx12x deserializer remote devices API function
 *
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#ifndef __RKX12X_REMOTE_H__
#define __RKX12X_REMOTE_H__

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>

#include "rkx12x_drv.h"

/* rkx12x remote api */
int rkx12x_remote_devices_s_power(struct rkx12x *rkx12x, int on);
int rkx12x_remote_devices_s_stream(struct rkx12x *rkx12x, int enable);

extern int rkx11x_linktx_pma_enable(struct i2c_client *ser_client, bool enable, u32 pma_id);
extern int rkx11x_linktx_set_rate(struct i2c_client *ser_client,
				uint32_t rate_mode,
				uint32_t pll_refclk_div,
				uint32_t pll_div,
				uint32_t clk_div,
				bool pll_div4,
				bool pll_fck_vco_div2,
				bool force_init_en);
extern int rkx11x_linktx_set_line(struct i2c_client *ser_client, u32 link_line);

#endif /* __RKX12X_REMOTE_H__ */
