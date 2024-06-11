/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 */

#ifndef __RKX12X_DRV_H__
#define __RKX12X_DRV_H__

#include <linux/version.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>

#include "rkx12x_i2c.h"
#include "rkx12x_linkrx.h"
#include "rkx12x_txphy.h"
#include "rkx12x_csi2tx.h"
#include "rkx12x_dvptx.h"

#include "hal/cru_api.h"
#include "hal/pinctrl_api.h"

/* rkx12x chip id */
#define RKX12X_GRF_REG_CHIP_ID		0x01010400

#define RKX12X_DES_CHIP_ID_V0		0x00002201
#define RKX12X_DES_CHIP_ID_V1		0x01200001

/* chip version */
enum {
	RKX12X_DES_CHIP_V0 = 0,
	RKX12X_DES_CHIP_V1,
};

/* stream interface */
enum {
	RKX12X_STREAM_PORT0 = 0,
	RKX12X_STREAM_PORT1,
	RKX12X_STREAM_PORT_MAX
};

/* stream interface */
enum {
	RKX12X_STREAM_INTERFACE_MIPI = 0,
	RKX12X_STREAM_INTERFACE_DVP,
};

/* link rate */
enum {
	RKX12X_LINK_RATE_2GBPS_83M = 0,
	RKX12X_LINK_RATE_4GBPS_83M,
	RKX12X_LINK_RATE_4GBPS_125M,
	RKX12X_LINK_RATE_4GBPS_250M,
	RKX12X_LINK_RATE_4_5GBPS_140M,
	RKX12X_LINK_RATE_4_6GBPS_300M,
	RKX12X_LINK_RATE_4_8GBPS_150M,
};

/* rkx12x i2c mux */
struct rkx12x_i2c_mux {
	struct i2c_mux_core *muxc;
	u32 i2c_mux_mask;
	u32 mux_channel;
	bool mux_disable;
};

struct rkx12x_vc_info {
	u32 enable; // 0: disable, !0: enable

	u32 width;
	u32 height;
	u32 bus_fmt;

	/*
	 * the following are optional parameters, user-defined data types
	 *   default 0: invalid parameter
	 */
	u32 data_type;
	u32 data_bit;
};

/* rkx12x support mode */
struct rkx12x_mode {
	u32 width;
	u32 height;
	struct v4l2_fract max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 exp_def;
	u32 link_freq_idx;
	u32 bus_fmt;
	u32 bpp;
	u32 vc[PAD_MAX];
	struct rkx12x_vc_info vc_info[PAD_MAX];
	struct v4l2_rect crop_rect;
};

/* rkx12x driver */
struct rkx12x {
	struct i2c_client *client;
	struct rkx12x_i2c_mux i2c_mux; /*i2c mux for remote devices */

	struct gpio_desc *gpio_reset; /* local chip reset gpio */
	struct gpio_desc *gpio_enable; /* local chip enable gpio */
	struct gpio_desc *gpio_irq; /* local chip irq gpio */

	u32 chip_id; /* chip id */
	u32 version; /* chip version */

	struct mutex mutex;

	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *anal_gain;
	struct v4l2_ctrl *digi_gain;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *test_pattern;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *link_freq;
	struct v4l2_fwnode_endpoint bus_cfg;

	bool streaming;
	bool power_on;
	bool hot_plug;
	u8 is_reset;

	struct rkx12x_mode supported_mode;
	const struct rkx12x_mode *cur_mode;
	u32 cfg_modes_num;

	u32 module_index;
	const char *module_facing;
	const char *module_name;
	const char *len_name;
	const char *sensor_name;

	int state_irq; /* state irq */

	u32 stream_port; /* stream port: 0 = lane0 / 1 = lane1 */
	u32 stream_interface; /* stream interface: 0 = mipi / 1 = dvp / 2 = lvds */
	u32 stream_clock_mode; /* 0: continuous mode, 1: non-continuous mode */
	u32 serdes_dclk_hz; /* serdes dclk */

	struct hwclk hwclk; /* cru clock */

	struct hwpin hwpin; /* pin ctrl */

	struct rkx12x_txphy txphy; /* txphy */

	struct rkx12x_csi2tx csi2tx; /* csi2tx */

	struct rkx12x_dvptx dvptx; /* dvptx */

	struct rkx12x_linkrx linkrx; /* link rx */

	struct rkx12x_i2c_init_seq extra_init_seq; /* extra i2c init seq */

	struct dentry *debugfs_root;

	/* register read/write api */
	int (*i2c_reg_read)(struct i2c_client *client, u32 reg, u32 *val);
	int (*i2c_reg_write)(struct i2c_client *client, u32 reg, u32 val);
	int (*i2c_reg_update)(struct i2c_client *client, u32 reg, u32 mask, u32 val);
};

/* rkx12x drv api */
int rkx12x_link_preinit(struct rkx12x *rkx12x);
int rkx12x_module_hw_init(struct rkx12x *rkx12x);
int rkx12x_i2c_mux_disable(struct rkx12x *rkx12x);
int rkx12x_i2c_mux_enable(struct rkx12x *rkx12x, u8 def_mask);
int rkx12x_link_set_rate(struct rkx12x_linkrx *linkrx,
			u32 link_rate, bool force_update);
int rkx12x_link_set_line(struct rkx12x_linkrx *linkrx, u32 link_line);

/* rkx12x v4l2 subdev api */
int rkx12x_v4l2_subdev_init(struct rkx12x *rkx12x);
void rkx12x_v4l2_subdev_deinit(struct rkx12x *rkx12x);

#endif /* __RKX12X_DRV_H__ */
