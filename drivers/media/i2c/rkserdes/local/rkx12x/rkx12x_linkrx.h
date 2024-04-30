/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 */

#ifndef __RKX12X_LINKRX_H__
#define __RKX12X_LINKRX_H__

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of_device.h>

/* link lane id */
enum {
	RKX12X_LINK_LANE0 = 0,
	RKX12X_LINK_LANE1,
	RKX12X_LINK_LANE_MAX
};

/* Link Lane Bit Mask: bit0 ~ bit1 */
#define RKX12X_LINK_MASK_LANE0		BIT(RKX12X_LINK_LANE0)
#define RKX12X_LINK_MASK_LANE1		BIT(RKX12X_LINK_LANE1)

#define RKX12X_LINK_MASK_LANES		GENMASK(RKX12X_LINK_LANE1, RKX12X_LINK_LANE0)

/* link engine id */
enum {
	RKX12X_LINK_ENGINE0 = 0,
	RKX12X_LINK_ENGINE1,
	RKX12X_LINK_ENGINE_MAX
};

/* Link engine Bit Mask: bit0 ~ bit1 */
#define RKX12X_LINK_MASK_ENGINE0	BIT(RKX12X_LINK_ENGINE0)
#define RKX12X_LINK_MASK_ENGINE1	BIT(RKX12X_LINK_ENGINE1)

#define RKX12X_LINK_MASK_ENGINES	GENMASK(RKX12X_LINK_ENGINE1, RKX12X_LINK_ENGINE0)

/* stream source */
enum {
	RKX12X_LINK_STREAM_CAMERA_CSI = 0,
	RKX12X_LINK_STREAM_CAMERA_DVP
};

/* pma pll mode */
enum {
	RKX12X_PMA_PLL_FDR_RATE_MODE = 0,
	RKX12X_PMA_PLL_HDR_RATE_MODE,
	RKX12X_PMA_PLL_QDR_RATE_MODE,
};

/* link pma pll */
struct rkx12x_pma_pll {
	uint32_t rate_mode;

	uint32_t pll_refclk_div;
	uint32_t pll_div;
	uint32_t clk_div;
	bool pll_div4;
	bool pll_fck_vco_div2;
	bool force_init_en;
};

/* link line config */
enum {
	RKX12X_LINK_LINE_COMMON_CONFIG = 0, /* common line config */
	RKX12X_LINK_LINE_SHORT_CONFIG, /* short line config */
	RKX12X_LINK_LINE_LONG_CONFIG, /* long line config */
};

/* link passthrough id */
#define RKX12X_LINK_PT_GPI_GPO_0	0
#define RKX12X_LINK_PT_GPI_GPO_1	1
#define RKX12X_LINK_PT_GPI_GPO_2	2
#define RKX12X_LINK_PT_GPI_GPO_3	3
#define RKX12X_LINK_PT_GPI_GPO_4	4
#define RKX12X_LINK_PT_GPI_GPO_5	5
#define RKX12X_LINK_PT_GPI_GPO_6	6
#define RKX12X_LINK_PT_IRQ		7
#define RKX12X_LINK_PT_UART_0		8
#define RKX12X_LINK_PT_UART_1		9
#define RKX12X_LINK_PT_SPI		10

struct rkx12x_linkrx {
	u32 lane_id;
	u32 link_lock;
	u32 link_rate;
	u32 link_line;
	u32 engine_id;
	u32 engine_source;
	u32 engine_delay;
	u32 video_packet_size;

	struct device_node *remote_ser_node;
	struct device_node *remote_cam_node;

	/* register read/write api */
	struct i2c_client *client;
	int (*i2c_reg_read)(struct i2c_client *client, u32 reg, u32 *val);
	int (*i2c_reg_write)(struct i2c_client *client, u32 reg, u32 val);
	int (*i2c_reg_update)(struct i2c_client *client, u32 reg, u32 mask, u32 val);
};

/* rkx12x linkrx api */
int rkx12x_linkrx_lane_init(struct rkx12x_linkrx *linkrx);
int rkx12x_linkrx_wait_lane_lock(struct rkx12x_linkrx *linkrx);
int rkx12x_linkrx_cmd_select(struct rkx12x_linkrx *linkrx, u32 lane_id);
int rkx12x_linkrx_video_stop(struct rkx12x_linkrx *linkrx,
			u32 engine_id, bool engine_stop);
int rkx12x_linkrx_audio_stop(struct rkx12x_linkrx *linkrx, bool audio_stop);
int rkx12x_linkrx_passthrough_cfg(struct rkx12x_linkrx *linkrx, u32 pt_id, bool is_pt_rx);

int rkx12x_des_pma_enable(struct rkx12x_linkrx *linkrx, bool enable, u32 pma_id);
int rkx12x_des_pcs_enable(struct rkx12x_linkrx *linkrx, bool enable, u32 pcs_id);
int rkx12x_des_pma_set_rate(struct rkx12x_linkrx *linkrx,
			const struct rkx12x_pma_pll *pma_pll);
int rkx12x_des_pma_set_line(struct rkx12x_linkrx *linkrx, u32 link_line);

int rkx12x_linkrx_irq_enable(struct rkx12x_linkrx *linkrx);
int rkx12x_linkrx_irq_disable(struct rkx12x_linkrx *linkrx);
int rkx12x_linkrx_irq_handler(struct rkx12x_linkrx *linkrx);
int rkx12x_des_pcs_irq_enable(struct rkx12x_linkrx *linkrx, u32 pcs_id);
int rkx12x_des_pcs_irq_disable(struct rkx12x_linkrx *linkrx, u32 pcs_id);
int rkx12x_des_pcs_irq_handler(struct rkx12x_linkrx *linkrx, u32 pcs_id);
int rkx12x_des_pma_irq_enable(struct rkx12x_linkrx *linkrx, u32 pma_id);
int rkx12x_des_pma_irq_disable(struct rkx12x_linkrx *linkrx, u32 pma_id);
int rkx12x_des_pma_irq_handler(struct rkx12x_linkrx *linkrx, u32 pma_id);

#endif /* __RKX12X_LINKRX_H__ */
