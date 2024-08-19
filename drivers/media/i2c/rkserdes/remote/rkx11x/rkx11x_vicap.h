/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Rockchip Electronics Co., Ltd.
 *
 */

#ifndef __RKX11X_VICAP_H__
#define __RKX11X_VICAP_H__

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>

enum {
	RKX11X_VICAP_CSI = 0,
	RKX11X_VICAP_DVP
};

enum {
	RKX11X_VICAP_VC_ID0 = 0,
	RKX11X_VICAP_VC_ID1,
	RKX11X_VICAP_VC_ID2,
	RKX11X_VICAP_VC_ID3,
	RKX11X_VICAP_VC_ID_MAX
};

/* MIPI: parse type of id */
enum {
	RKX11X_VICAP_MIPI_PARSE_TYPE_RAW8 = 0,
	RKX11X_VICAP_MIPI_PARSE_TYPE_RAW10 = 1,
	RKX11X_VICAP_MIPI_PARSE_TYPE_RAW12 = 2,
	RKX11X_VICAP_MIPI_PARSE_TYPE_YUV422_8BIT = 4
};

/* DVP: raw data width */
enum {
	RKX11X_VICAP_DVP_RAW_WIDTH_8BIT = 0,
	RKX11X_VICAP_DVP_RAW_WIDTH_10BIT,
	RKX11X_VICAP_DVP_RAW_WIDTH_12BIT
};

/* DVP: YUV in order */
enum {
	RKX11X_VICAP_DVP_YUV_ORDER_UYVY = 0,
	RKX11X_VICAP_DVP_YUV_ORDER_VYUY,
	RKX11X_VICAP_DVP_YUV_ORDER_YUYV,
	RKX11X_VICAP_DVP_YUV_ORDER_YVYU
};

/* DVP: input mode */
enum {
	RKX11X_VICAP_DVP_INPUT_MODE_YUV422 = 0,
	RKX11X_VICAP_DVP_INPUT_MODE_RAW,
	RKX11X_VICAP_DVP_INPUT_MODE_SONNY_RAW
};

struct vicap_vc_id {
	u32 vc;
	u32 data_type;
	u32 parse_type;
	u16 width;
	u16 height;
};

struct rkx11x_vicap_csi {
	u32 data_lanes; /* mipi data lanes */
	u32 vc_flag; /* bit0 ~ 3 for vc_id0 ~ 3 */
	struct vicap_vc_id vc_id[RKX11X_VICAP_VC_ID_MAX];
};

struct rkx11x_vicap_dvp {
	u32 input_mode; // input mode
	u32 raw_width; // raw width
	u32 yuv_order; // yuv order
	u32 clock_invert; // clock invert
	u32 href_pol; // href polarity
	u32 vsync_pol; // vsync polarity
};

struct rkx11x_vicap {
	u32 id;
	u32 interface;
	struct rkx11x_vicap_csi vicap_csi;
	struct rkx11x_vicap_dvp vicap_dvp;

	/* register read/write api */
	struct i2c_client *client;
	int (*i2c_reg_read)(struct i2c_client *client, u32 reg, u32 *val);
	int (*i2c_reg_write)(struct i2c_client *client, u32 reg, u32 val);
	int (*i2c_reg_update)(struct i2c_client *client, u32 reg, u32 mask, u32 val);
};

/* rkx11x vicap api */
int rkx11x_vicap_init(struct rkx11x_vicap *vicap);

#endif /* __RKX11X_VICAP_H__ */
