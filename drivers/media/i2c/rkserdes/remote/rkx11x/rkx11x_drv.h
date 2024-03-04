/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 */

#ifndef __RKX11X_DRV_H__
#define __RKX11X_DRV_H__

#include <linux/version.h>
#include <linux/i2c.h>

#include "rkx11x_i2c.h"
#include "rkx11x_rxphy.h"
#include "rkx11x_csi2host.h"
#include "rkx11x_vicap.h"
#include "rkx11x_linktx.h"
#include "rkx11x_compact.h"

#include "hal/cru_api.h"
#include "hal/pinctrl_api.h"

#include "../rkser_dev.h"

/* rkx11x chip id */
#define RKX11X_GRF_REG_CHIP_ID		0x00010800

#define RKX11X_SER_CHIP_ID_V0		0x00002201
#define RKX11X_SER_CHIP_ID_V1		0x01100001

/* chip version */
enum {
	RKX11X_SER_CHIP_V0 = 0,
	RKX11X_SER_CHIP_V1,
};

/* stream interface */
enum {
	RKX11X_STREAM_INTERFACE_MIPI = 0,
	RKX11X_STREAM_INTERFACE_DVP,
	RKX11X_STREAM_INTERFACE_LVDS
};

/* rkx11x driver */
struct rkx11x {
	struct i2c_client *client;
	struct rkser_dev *parent;

	u32 chip_id; /* chip id */
	u32 version; /* chip version */

	struct mutex mutex;

	u32 stream_interface; /* stream interface: 0 = mipi / 1 = dvp / 2 = lvds */
	u32 serdes_dclk_hz; /* serdes dclk */
	u32 camera_mclk_hz; /* mclk output for camera */
	u32 camera_mclk_id; /* mclk id: 0 or 1 */

	struct hwclk hwclk; /* cru clock */

	struct hwpin hwpin; /* pin ctrl */

	struct rkx11x_rxphy rxphy;

	struct rkx11x_csi2host csi2host;

	struct rkx11x_vicap vicap;

	struct rkx11x_linktx linktx;

	struct rkx11x_i2c_init_seq ser_init_seq;

	struct dentry *debugfs_root;

	/* register read/write api */
	int (*i2c_reg_read)(struct i2c_client *client, u32 reg, u32 *val);
	int (*i2c_reg_write)(struct i2c_client *client, u32 reg, u32 val);
	int (*i2c_reg_update)(struct i2c_client *client, u32 reg, u32 mask, u32 val);
};

/* rkx11x drv api */
int rkx11x_linktx_pma_enable(struct i2c_client *ser_client, bool enable, u32 pma_id);
int rkx11x_linktx_set_rate(struct i2c_client *ser_client,
				uint32_t rate_mode,
				uint32_t pll_refclk_div,
				uint32_t pll_div,
				uint32_t clk_div,
				bool pll_div4,
				bool pll_fck_vco_div2,
				bool force_init_en);
int rkx11x_linktx_set_line(struct i2c_client *ser_client, u32 link_line);

#endif /* __RKX11X_DRV_H__ */
