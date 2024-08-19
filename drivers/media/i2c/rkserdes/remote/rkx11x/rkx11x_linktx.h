/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 */

#ifndef __RKX11X_LINKTX_H__
#define __RKX11X_LINKTX_H__

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>

/* linktx version */
enum {
	RKX11X_LINKTX_V0 = 0,
	RKX11X_LINKTX_V1,
};

/* link lane id */
enum {
	RKX11X_LINK_LANE0 = 0,
	RKX11X_LINK_LANE1,
	RKX11X_LINK_LANE_MAX
};

/* lane stream source */
enum {
	RKX11X_LINK_LANE_STREAM_MIPI = 0,
	RKX11X_LINK_LANE_STREAM_DVP,
	RKX11X_LINK_LANE_STREAM_LVDS
};

/* line config */
enum {
	RKX11X_LINK_LINE_COMMON_CONFIG = 0, /* link common line config */
	RKX11X_LINK_LINE_SHORT_CONFIG, /* link short line config */
	RKX11X_LINK_LINE_LONG_CONFIG, /* link long line config */
};

/* link passthrough id */
#define RKX11X_LINK_PT_GPI_GPO_0	0
#define RKX11X_LINK_PT_GPI_GPO_1	1
#define RKX11X_LINK_PT_GPI_GPO_2	2
#define RKX11X_LINK_PT_GPI_GPO_3	3
#define RKX11X_LINK_PT_GPI_GPO_4	4
#define RKX11X_LINK_PT_GPI_GPO_5	5
#define RKX11X_LINK_PT_GPI_GPO_6	6
#define RKX11X_LINK_PT_IRQ		7
#define RKX11X_LINK_PT_UART_0		8
#define RKX11X_LINK_PT_UART_1		9
#define RKX11X_LINK_PT_SPI		10

/* pma pll mode */
enum {
	RKX11X_PMA_PLL_FDR_RATE_MODE = 0,
	RKX11X_PMA_PLL_HDR_RATE_MODE,
	RKX11X_PMA_PLL_QDR_RATE_MODE,
};

/* link pma pll */
struct rkx11x_pma_pll {
	uint32_t rate_mode;

	uint32_t pll_refclk_div;
	uint32_t pll_div;
	uint32_t clk_div;
	bool pll_div4;
	bool pll_fck_vco_div2;
	bool force_init_en;
};

/* lane config */
struct rkx11x_lane_cfg {
	u32 lane_enable;
	u32 dsource_id;
};

struct rkx11x_linktx {
	u32 version;
	u32 stream_source;
	u32 video_packet_size;

	struct rkx11x_lane_cfg lane_cfg[RKX11X_LINK_LANE_MAX];

	/* register read/write api */
	struct i2c_client *client;
	int (*i2c_reg_read)(struct i2c_client *client, u32 reg, u32 *val);
	int (*i2c_reg_write)(struct i2c_client *client, u32 reg, u32 val);
	int (*i2c_reg_update)(struct i2c_client *client, u32 reg, u32 mask, u32 val);
};

/* rkx11x linktx api */
int rkx11x_linktx_lane_init(struct rkx11x_linktx *linktx);
int rkx11x_linktx_video_enable(struct rkx11x_linktx *linktx, bool enable);
int rkx11x_linktx_video_stop(struct rkx11x_linktx *linktx, bool stop, u32 lane_id);
int rkx11x_linktx_passthrough_cfg(struct rkx11x_linktx *linktx, u32 pt_id, bool is_pt_rx);

int rkx11x_ser_pma_set_rate(struct rkx11x_linktx *linktx,
			const struct rkx11x_pma_pll *ser_pma_pll);
int rkx11x_ser_pma_enable(struct rkx11x_linktx *linktx, bool enable, u32 pma_id);
int rkx11x_ser_pma_set_line(struct rkx11x_linktx *linktx, u32 link_line);

#endif /* __RKX11X_LINKTX_H__ */
