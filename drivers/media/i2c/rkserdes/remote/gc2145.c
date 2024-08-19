// SPDX-License-Identifier: GPL-2.0
/*
 * GC2145 CMOS Image Sensor driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * V1.0X00.0X00 support for rockchip serdes
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/videodev2.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>

#include "rkser_dev.h"

#define DRIVER_VERSION			KERNEL_VERSION(1, 0x00, 0x00)

#define DRIVER_NAME			"gc2145"

#define GC2145_PIXEL_RATE		(120 * 1000 * 1000)

#undef GC2145_EXPOSURE_CONTROL

/*
 * GC2145 register definitions
 */
#define REG_SC_CHIP_ID_H		0xf0
#define REG_SC_CHIP_ID_L		0xf1

/* I2C Array token */
#define REG_NULL			0xFFFF	/* Array end token */

#define SENSOR_ID(_msb, _lsb)		((_msb) << 8 | (_lsb))
#define GC2145_ID			0x2145

struct regval {
	u16 addr;
	u8 value;
};

struct gc2145_framesize {
	u16 width;
	u16 height;
	struct v4l2_fract max_fps;
	u16 max_exp_lines;
	const struct regval *regs;
};

struct gc2145_pll_ctrl {
	u8 ctrl1;
	u8 ctrl2;
	u8 ctrl3;
};

struct gc2145_pixfmt {
	u32 code;
	/* Output format Register Value (REG_FORMAT_CTRL00) */
	struct regval *format_ctrl_regs;
};

struct pll_ctrl_reg {
	unsigned int div;
	unsigned char reg;
};

struct gc2145 {
	struct v4l2_subdev		sd;
	struct media_pad		pad;
	struct v4l2_mbus_framefmt	format;
	unsigned int			fps;
	struct regulator		*poc_regulator;
	struct mutex			lock;
	struct i2c_client		*client;
	struct v4l2_ctrl_handler	ctrls;
	struct v4l2_ctrl		*link_frequency;
	struct v4l2_fwnode_endpoint	bus_cfg;
	const struct gc2145_framesize	*frame_size;
	const struct gc2145_framesize	*framesize_cfg;
	unsigned int			cfg_num;
	int				streaming;
	bool				power_on;
	u32				module_index;
	const char			*module_facing;
	const char			*module_name;
	const char			*len_name;

	serializer_t			*serializer;
};

static const struct regval gc2145_dvp_init_regs[] = {
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfc, 0x06},
	{0xf6, 0x00},
	{0xf7, 0x1d},
	{0xf8, 0x84},
	{0xfa, 0x00},
	{0xf9, 0xfe},
	{0xf2, 0x00},
	/*ISP reg*/
	{0xfe, 0x00},
	{0x03, 0x04},
	{0x04, 0xe2},
	{0x09, 0x00},
	{0x0a, 0x00},
	{0x0b, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x04},
	{0x0e, 0xc0},
	{0x0f, 0x06},
	{0x10, 0x52},
	{0x12, 0x2e},
	{0x17, 0x14},
	{0x18, 0x22},
	{0x19, 0x0e},
	{0x1a, 0x01},
	{0x1b, 0x4b},
	{0x1c, 0x07},
	{0x1d, 0x10},
	{0x1e, 0x88},
	{0x1f, 0x78},
	{0x20, 0x03},
	{0x21, 0x40},
	{0x22, 0xa0},
	{0x24, 0x3f},
	{0x25, 0x01},
	{0x26, 0x10},
	{0x2d, 0x60},
	{0x30, 0x01},
	{0x31, 0x90},
	{0x33, 0x06},
	{0x34, 0x01},
	{0xfe, 0x00},
	{0x80, 0x7f},
	{0x81, 0x26},
	{0x82, 0xfa},
	{0x83, 0x00},
	{0x84, 0x00},
	{0x86, 0x02},
	{0x88, 0x03},
	{0x89, 0x03},
	{0x85, 0x08},
	{0x8a, 0x00},
	{0x8b, 0x00},
	{0xb0, 0x55},
	{0xc3, 0x00},
	{0xc4, 0x80},
	{0xc5, 0x90},
	{0xc6, 0x3b},
	{0xc7, 0x46},
	{0xec, 0x06},
	{0xed, 0x04},
	{0xee, 0x60},
	{0xef, 0x90},
	{0xb6, 0x01},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x40},
	/*BLK*/
	{0xfe, 0x00},
	{0x40, 0x42},
	{0x41, 0x00},
	{0x43, 0x5b},
	{0x5e, 0x00},
	{0x5f, 0x00},
	{0x60, 0x00},
	{0x61, 0x00},
	{0x62, 0x00},
	{0x63, 0x00},
	{0x64, 0x00},
	{0x65, 0x00},
	{0x66, 0x20},
	{0x67, 0x20},
	{0x68, 0x20},
	{0x69, 0x20},
	{0x76, 0x00},
	{0x6a, 0x08},
	{0x6b, 0x08},
	{0x6c, 0x08},
	{0x6d, 0x08},
	{0x6e, 0x08},
	{0x6f, 0x08},
	{0x70, 0x08},
	{0x71, 0x08},
	{0x76, 0x00},
	{0x72, 0xf0},
	{0x7e, 0x3c},
	{0x7f, 0x00},
	{0xfe, 0x02},
	{0x48, 0x15},
	{0x49, 0x00},
	{0x4b, 0x0b},
	{0xfe, 0x00},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0xc0},
	{0x03, 0x04},
	{0x04, 0x90},
	{0x05, 0x30},
	{0x06, 0x90},
	{0x07, 0x30},
	{0x08, 0x80},
	{0x09, 0x00},
	{0x0a, 0x82},
	{0x0b, 0x11},
	{0x0c, 0x10},
	{0x11, 0x10},
	{0x13, 0x7b},
	{0x17, 0x00},
	{0x1c, 0x11},
	{0x1e, 0x61},
	{0x1f, 0x35},
	{0x20, 0x40},
	{0x22, 0x40},
	{0x23, 0x20},
	{0xfe, 0x02},
	{0x0f, 0x04},
	{0xfe, 0x01},
	{0x12, 0x35},
	{0x15, 0xb0},
	{0x10, 0x31},
	{0x3e, 0x28},
	{0x3f, 0xb0},
	{0x40, 0x90},
	{0x41, 0x0f},

	/*INTPEE*/
	{0xfe, 0x02},
	{0x90, 0x6c},
	{0x91, 0x03},
	{0x92, 0xcb},
	{0x94, 0x33},
	{0x95, 0x84},
	{0x97, 0x45},
	{0xa2, 0x11},
	{0xfe, 0x00},
	/*DNDD*/
	{0xfe, 0x02},
	{0x80, 0xc1},
	{0x81, 0x08},
	{0x82, 0x1f},
	{0x83, 0x10},
	{0x84, 0x0a},
	{0x86, 0xf0},
	{0x87, 0x50},
	{0x88, 0x15},
	{0x89, 0xb0},
	{0x8a, 0x30},
	{0x8b, 0x10},
	/*ASDE*/
	{0xfe, 0x01},
	{0x21, 0x04},
	{0xfe, 0x02},
	{0xa3, 0x50},
	{0xa4, 0x20},
	{0xa5, 0x40},
	{0xa6, 0x80},
	{0xab, 0x40},
	{0xae, 0x0c},
	{0xb3, 0x46},
	{0xb4, 0x64},
	{0xb6, 0x38},
	{0xb7, 0x01},
	{0xb9, 0x2b},
	{0x3c, 0x04},
	{0x3d, 0x15},
	{0x4b, 0x06},
	{0x4c, 0x20},
	{0xfe, 0x00},
	/*GAMMA*/
	/*gamma1*/
#if 1
	{0xfe, 0x02},
	{0x10, 0x09},
	{0x11, 0x0d},
	{0x12, 0x13},
	{0x13, 0x19},
	{0x14, 0x27},
	{0x15, 0x37},
	{0x16, 0x45},
	{0x17, 0x53},
	{0x18, 0x69},
	{0x19, 0x7d},
	{0x1a, 0x8f},
	{0x1b, 0x9d},
	{0x1c, 0xa9},
	{0x1d, 0xbd},
	{0x1e, 0xcd},
	{0x1f, 0xd9},
	{0x20, 0xe3},
	{0x21, 0xea},
	{0x22, 0xef},
	{0x23, 0xf5},
	{0x24, 0xf9},
	{0x25, 0xff},
#else
	{0xfe, 0x02},
	{0x10, 0x0a},
	{0x11, 0x12},
	{0x12, 0x19},
	{0x13, 0x1f},
	{0x14, 0x2c},
	{0x15, 0x38},
	{0x16, 0x42},
	{0x17, 0x4e},
	{0x18, 0x63},
	{0x19, 0x76},
	{0x1a, 0x87},
	{0x1b, 0x96},
	{0x1c, 0xa2},
	{0x1d, 0xb8},
	{0x1e, 0xcb},
	{0x1f, 0xd8},
	{0x20, 0xe2},
	{0x21, 0xe9},
	{0x22, 0xf0},
	{0x23, 0xf8},
	{0x24, 0xfd},
	{0x25, 0xff},
	{0xfe, 0x00},
#endif
	{0xfe, 0x00},
	{0xc6, 0x20},
	{0xc7, 0x2b},
	/*gamma2*/
#if 1
	{0xfe, 0x02},
	{0x26, 0x0f},
	{0x27, 0x14},
	{0x28, 0x19},
	{0x29, 0x1e},
	{0x2a, 0x27},
	{0x2b, 0x33},
	{0x2c, 0x3b},
	{0x2d, 0x45},
	{0x2e, 0x59},
	{0x2f, 0x69},
	{0x30, 0x7c},
	{0x31, 0x89},
	{0x32, 0x98},
	{0x33, 0xae},
	{0x34, 0xc0},
	{0x35, 0xcf},
	{0x36, 0xda},
	{0x37, 0xe2},
	{0x38, 0xe9},
	{0x39, 0xf3},
	{0x3a, 0xf9},
	{0x3b, 0xff},
#else
	/*Gamma outdoor*/
	{0xfe, 0x02},
	{0x26, 0x17},
	{0x27, 0x18},
	{0x28, 0x1c},
	{0x29, 0x20},
	{0x2a, 0x28},
	{0x2b, 0x34},
	{0x2c, 0x40},
	{0x2d, 0x49},
	{0x2e, 0x5b},
	{0x2f, 0x6d},
	{0x30, 0x7d},
	{0x31, 0x89},
	{0x32, 0x97},
	{0x33, 0xac},
	{0x34, 0xc0},
	{0x35, 0xcf},
	{0x36, 0xda},
	{0x37, 0xe5},
	{0x38, 0xec},
	{0x39, 0xf8},
	{0x3a, 0xfd},
	{0x3b, 0xff},
#endif
	/*YCP*/
	{0xfe, 0x02},
	{0xd1, 0x40},
	{0xd2, 0x40},
	{0xd3, 0x48},
	{0xd6, 0xf0},
	{0xd7, 0x10},
	{0xd8, 0xda},
	{0xdd, 0x14},
	{0xde, 0x86},
	{0xed, 0x80},
	{0xee, 0x00},
	{0xef, 0x3f},
	{0xd8, 0xd8},
	/*abs*/
	{0xfe, 0x01},
	{0x9f, 0x40},
	/*LSC*/
	{0xfe, 0x01},
	{0xc2, 0x14},
	{0xc3, 0x0d},
	{0xc4, 0x0c},
	{0xc8, 0x15},
	{0xc9, 0x0d},
	{0xca, 0x0a},
	{0xbc, 0x24},
	{0xbd, 0x10},
	{0xbe, 0x0b},
	{0xb6, 0x25},
	{0xb7, 0x16},
	{0xb8, 0x15},
	{0xc5, 0x00},
	{0xc6, 0x00},
	{0xc7, 0x00},
	{0xcb, 0x00},
	{0xcc, 0x00},
	{0xcd, 0x00},
	{0xbf, 0x07},
	{0xc0, 0x00},
	{0xc1, 0x00},
	{0xb9, 0x00},
	{0xba, 0x00},
	{0xbb, 0x00},
	{0xaa, 0x01},
	{0xab, 0x01},
	{0xac, 0x00},
	{0xad, 0x05},
	{0xae, 0x06},
	{0xaf, 0x0e},
	{0xb0, 0x0b},
	{0xb1, 0x07},
	{0xb2, 0x06},
	{0xb3, 0x17},
	{0xb4, 0x0e},
	{0xb5, 0x0e},
	{0xd0, 0x09},
	{0xd1, 0x00},
	{0xd2, 0x00},
	{0xd6, 0x08},
	{0xd7, 0x00},
	{0xd8, 0x00},
	{0xd9, 0x00},
	{0xda, 0x00},
	{0xdb, 0x00},
	{0xd3, 0x0a},
	{0xd4, 0x00},
	{0xd5, 0x00},
	{0xa4, 0x00},
	{0xa5, 0x00},
	{0xa6, 0x77},
	{0xa7, 0x77},
	{0xa8, 0x77},
	{0xa9, 0x77},
	{0xa1, 0x80},
	{0xa2, 0x80},

	{0xfe, 0x01},
	{0xdf, 0x0d},
	{0xdc, 0x25},
	{0xdd, 0x30},
	{0xe0, 0x77},
	{0xe1, 0x80},
	{0xe2, 0x77},
	{0xe3, 0x90},
	{0xe6, 0x90},
	{0xe7, 0xa0},
	{0xe8, 0x90},
	{0xe9, 0xa0},
	{0xfe, 0x00},
	/*AWB*/
	{0xfe, 0x01},
	{0x4f, 0x00},
	{0x4f, 0x00},
	{0x4b, 0x01},
	{0x4f, 0x00},

	{0x4c, 0x01},
	{0x4d, 0x71},
	{0x4e, 0x01},
	{0x4c, 0x01},
	{0x4d, 0x91},
	{0x4e, 0x01},
	{0x4c, 0x01},
	{0x4d, 0x70},
	{0x4e, 0x01},
	{0x4c, 0x01},
	{0x4d, 0x90},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xb0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0x8f},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0x6f},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xaf},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xd0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xf0},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xcf},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0xef},
	{0x4e, 0x02},
	{0x4c, 0x01},
	{0x4d, 0x6e},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8e},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xae},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xce},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x4d},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6d},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8d},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xad},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xcd},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x4c},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6c},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8c},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xac},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xcc},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xcb},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x4b},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x6b},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8b},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0xab},
	{0x4e, 0x03},
	{0x4c, 0x01},
	{0x4d, 0x8a},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xaa},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xca},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xca},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xc9},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0x8a},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0x89},
	{0x4e, 0x04},
	{0x4c, 0x01},
	{0x4d, 0xa9},
	{0x4e, 0x04},
	{0x4c, 0x02},
	{0x4d, 0x0b},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x0a},
	{0x4e, 0x05},
	{0x4c, 0x01},
	{0x4d, 0xeb},
	{0x4e, 0x05},
	{0x4c, 0x01},
	{0x4d, 0xea},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x09},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x29},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x2a},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x4a},
	{0x4e, 0x05},
	{0x4c, 0x02},
	{0x4d, 0x8a},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x49},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x69},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x89},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0xa9},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x48},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x68},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0x69},
	{0x4e, 0x06},
	{0x4c, 0x02},
	{0x4d, 0xca},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xc9},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xe9},
	{0x4e, 0x07},
	{0x4c, 0x03},
	{0x4d, 0x09},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xc8},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xe8},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xa7},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xc7},
	{0x4e, 0x07},
	{0x4c, 0x02},
	{0x4d, 0xe7},
	{0x4e, 0x07},
	{0x4c, 0x03},
	{0x4d, 0x07},
	{0x4e, 0x07},

	{0x4f, 0x01},
	{0x50, 0x80},
	{0x51, 0xa8},
	{0x52, 0x47},
	{0x53, 0x38},
	{0x54, 0xc7},
	{0x56, 0x0e},
	{0x58, 0x08},
	{0x5b, 0x00},
	{0x5c, 0x74},
	{0x5d, 0x8b},
	{0x61, 0xdb},
	{0x62, 0xb8},
	{0x63, 0x86},
	{0x64, 0xc0},
	{0x65, 0x04},
	{0x67, 0xa8},
	{0x68, 0xb0},
	{0x69, 0x00},
	{0x6a, 0xa8},
	{0x6b, 0xb0},
	{0x6c, 0xaf},
	{0x6d, 0x8b},
	{0x6e, 0x50},
	{0x6f, 0x18},
	{0x73, 0xf0},
	{0x70, 0x0d},
	{0x71, 0x60},
	{0x72, 0x80},
	{0x74, 0x01},
	{0x75, 0x01},
	{0x7f, 0x0c},
	{0x76, 0x70},
	{0x77, 0x58},
	{0x78, 0xa0},
	{0x79, 0x5e},
	{0x7a, 0x54},
	{0x7b, 0x58},
	{0xfe, 0x00},
	/*CC*/
	{0xfe, 0x02},
	{0xc0, 0x01},
	{0xc1, 0x44},
	{0xc2, 0xfd},
	{0xc3, 0x04},
	{0xc4, 0xF0},
	{0xc5, 0x48},
	{0xc6, 0xfd},
	{0xc7, 0x46},
	{0xc8, 0xfd},
	{0xc9, 0x02},
	{0xca, 0xe0},
	{0xcb, 0x45},
	{0xcc, 0xec},
	{0xcd, 0x48},
	{0xce, 0xf0},
	{0xcf, 0xf0},
	{0xe3, 0x0c},
	{0xe4, 0x4b},
	{0xe5, 0xe0},
	/*ABS*/
	{0xfe, 0x01},
	{0x9f, 0x40},
	{0xfe, 0x00},
	/*OUTPUT*/
	{0xfe, 0x00},
	{0xf2, 0x0f},
	/*dark sun*/
	{0xfe, 0x02},
	{0x40, 0xbf},
	{0x46, 0xcf},
	{0xfe, 0x00},

	/*frame rate 50Hz*/
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x20},
	{0x07, 0x00},
	{0x08, 0x32},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0xfa},

	{0x27, 0x04},
	{0x28, 0xe2},
	{0x29, 0x04},
	{0x2a, 0xe2},
	{0x2b, 0x04},
	{0x2c, 0xe2},
	{0x2d, 0x04},
	{0x2e, 0xe2},
	{0xfe, 0x00},

	{0xfe, 0x00},
	{0xfd, 0x01},
	{0xfa, 0x00},
	/*crop window*/
	{0xfe, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0x58},
	{0x97, 0x03},
	{0x98, 0x20},
	{0x99, 0x11},
	{0x9a, 0x06},
	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x02},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x02},
	{0x9d, 0x08},
	{0xfe, 0x01},
	{0x74, 0x00},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0x60},
	{0x03, 0x02},
	{0x04, 0x48},
	{0x05, 0x18},
	{0x06, 0x50},
	{0x07, 0x10},
	{0x08, 0x38},
	{0x0a, 0x80},
	{0x21, 0x04},
	{0xfe, 0x00},
	{0x20, 0x03},
	{0xfe, 0x00},
	{REG_NULL, 0x00},
};

/* Senor full resolution setting */
static const struct regval gc2145_dvp_full[] = {
	//SENSORDB("GC2145_Sensor_2M"},
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x20},
	{0x07, 0x00},
	{0x08, 0x50},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0xfa},

	{0x27, 0x04},
	{0x28, 0xe2},
	{0x29, 0x04},
	{0x2a, 0xe2},
	{0x2b, 0x04},
	{0x2c, 0xe2},
	{0x2d, 0x04},
	{0x2e, 0xe2},
	{0xfe, 0x00},
	{0xfd, 0x00},
	{0xfa, 0x11},
	{0x18, 0x22},
	/*crop window*/
	{0xfe, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x04},
	{0x96, 0xb0},
	{0x97, 0x06},
	{0x98, 0x40},
	{0x99, 0x11},
	{0x9a, 0x06},
	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x06},
	{0xed, 0x04},
	{0xee, 0x60},
	{0xef, 0x90},
	{0xfe, 0x01},
	{0x74, 0x01},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0xc0},
	{0x03, 0x04},
	{0x04, 0x90},
	{0x05, 0x30},
	{0x06, 0x90},
	{0x07, 0x30},
	{0x08, 0x80},
	{0x0a, 0x82},
	{0xfe, 0x01},
	{0x21, 0x15}, //if 0xfa=11,then 0x21=15;
	{0xfe, 0x00}, //else if 0xfa=00,then 0x21=04
	{0x20, 0x15},
	{0xfe, 0x00},

	{REG_NULL, 0x00},
};

/* Preview resolution setting*/
static const struct regval gc2145_dvp_svga_30fps[] = {
	/*frame rate 50Hz*/
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x20},
	{0x07, 0x00},
	{0x08, 0xb8},
	{0xfe, 0x01},
	{0x13, 0x7b},
	{0x18, 0x95},
	{0x25, 0x01},
	{0x26, 0xac},

	{0x27, 0x05},
	{0x28, 0x04},
	{0x29, 0x05},
	{0x2a, 0x04},
	{0x2b, 0x05},
	{0x2c, 0x04},
	{0x2d, 0x05},
	{0x2e, 0x04},

	{0x3c, 0x00},
	{0x3d, 0x15},
	{0xfe, 0x00},
	{0xfe, 0x00},
	{0xfa, 0x00},
	{0x18, 0x42},
	{0xfd, 0x03},
	{0xb6, 0x01},
	/* crop window */
	{0xfe, 0x00},
	{0x99, 0x11},
	{0x9a, 0x06},
	{0x9b, 0x00},
	{0x9c, 0x00},
	{0x9d, 0x00},
	{0x9e, 0x00},
	{0x9f, 0x00},
	{0xa0, 0x00},
	{0xa1, 0x00},
	{0xa2, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0x58},
	{0x97, 0x03},
	{0x98, 0x20},
	/* AWB */
	{0xfe, 0x00},
	{0xec, 0x01},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x01},
	{0x74, 0x00},
	/* AEC */
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0x60},
	{0x03, 0x02},
	{0x04, 0x48},
	{0x05, 0x18},
	{0x06, 0x4c},
	{0x07, 0x14},
	{0x08, 0x36},
	{0x0a, 0xc0},
	{0x21, 0x14},
	{0xfe, 0x00},
	{REG_NULL, 0x00},
};

/* Preview resolution setting*/
static const struct regval gc2145_dvp_svga_20fps[] = {
	//SENSORDB("GC2145_Sensor_SVGA"},
	{0xfe, 0x00},
	{0x05, 0x02},
	{0x06, 0x20},
	{0x07, 0x00},
	{0x08, 0x50},
	{0xfe, 0x01},
	{0x25, 0x00},
	{0x26, 0xfa},

	{0x27, 0x04},
	{0x28, 0xe2},
	{0x29, 0x04},
	{0x2a, 0xe2},
	{0x2b, 0x04},
	{0x2c, 0xe2},
	{0x2d, 0x04},
	{0x2e, 0xe2},

	{0xfe, 0x00},
	{0xb6, 0x01},
	{0xfd, 0x01},
	{0xfa, 0x00},
	{0x18, 0x22},
	/*crop window*/
	{0xfe, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0x58},
	{0x97, 0x03},
	{0x98, 0x20},
	{0x99, 0x11},
	{0x9a, 0x06},
	/*AWB*/
	{0xfe, 0x00},
	{0xec, 0x02},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x02},
	{0x9d, 0x08},
	{0xfe, 0x01},
	{0x74, 0x00},
	/*AEC*/
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0x60},
	{0x03, 0x02},
	{0x04, 0x48},
	{0x05, 0x18},
	{0x06, 0x50},
	{0x07, 0x10},
	{0x08, 0x38},
	{0x0a, 0x80},
	{0x21, 0x04},
	{0xfe, 0x00},
	{0x20, 0x03},
	{0xfe, 0x00},
	{REG_NULL, 0x00},
};

static const struct gc2145_framesize gc2145_dvp_framesizes[] = {
	{ /* SVGA */
		.width		= 800,
		.height		= 600,
		.max_fps = {
			.numerator = 10000,
			.denominator = 160000,
		},
		.regs		= gc2145_dvp_svga_20fps,
	}, { /* SVGA */
		.width		= 800,
		.height		= 600,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.regs		= gc2145_dvp_svga_30fps,
	}, { /* FULL */
		.width		= 1600,
		.height		= 1200,
		.max_fps = {
			.numerator = 10000,
			.denominator = 160000,
		},
		.regs		= gc2145_dvp_full,
	}
};

static const s64 link_freq_menu_items[] = {
	240000000
};

static const struct gc2145_pixfmt gc2145_formats[] = {
	{
		.code = MEDIA_BUS_FMT_UYVY8_2X8,
	}
};

static inline struct gc2145 *to_gc2145(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc2145, sd);
}

/* sensor register write */
static int gc2145_write(struct i2c_client *client, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	dev_dbg(&client->dev, "write reg(0x%x val:0x%x)!\n", reg, val);

	buf[0] = reg & 0xFF;
	buf[1] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		return 0;

	dev_err(&client->dev,
		"gc2145 write reg(0x%x val:0x%x) failed !\n", reg, val);

	return ret;
}

/* sensor register read */
static int gc2145_read(struct i2c_client *client, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[1];
	int ret;

	buf[0] = reg & 0xFF;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret >= 0) {
		*val = buf[0];
		return 0;
	}

	dev_err(&client->dev,
		"gc2145 read reg:0x%x failed !\n", reg);

	return ret;
}

static int gc2145_write_array(struct i2c_client *client,
			      const struct regval *regs)
{
	int i, ret = 0;

	i = 0;
	while (regs[i].addr != REG_NULL) {
		ret = gc2145_write(client, regs[i].addr, regs[i].value);
		if (ret) {
			dev_err(&client->dev, "%s failed !\n", __func__);
			break;
		}

		i++;
	}

	return ret;
}

static int gc2145_detect(struct gc2145 *gc2145)
{
	struct i2c_client *client = gc2145->client;
	u8 pid, ver;
	int ret;

	dev_dbg(&client->dev, "%s:\n", __func__);

	/* Check sensor revision */
	ret = gc2145_read(client, REG_SC_CHIP_ID_H, &pid);
	if (!ret)
		ret = gc2145_read(client, REG_SC_CHIP_ID_L, &ver);

	if (!ret) {
		unsigned short id;

		id = SENSOR_ID(pid, ver);
		if (id != GC2145_ID) {
			ret = -1;
			dev_err(&client->dev,
				"Sensor detection failed (%04X, %d)\n",
				id, ret);
		} else {
			dev_info(&client->dev, "Found GC%04X sensor\n", id);
		}
	}

	return ret;
}

static void gc2145_get_default_format(struct gc2145 *gc2145,
				      struct v4l2_mbus_framefmt *format)
{
	format->width = gc2145->framesize_cfg[0].width;
	format->height = gc2145->framesize_cfg[0].height;
	format->colorspace = V4L2_COLORSPACE_SRGB;
	format->code = gc2145_formats[0].code;
	format->field = V4L2_FIELD_NONE;
}

static void gc2145_set_streaming(struct gc2145 *gc2145, int on)
{
	struct i2c_client *client = gc2145->client;
	int ret = 0;
	u8 val;

	dev_dbg(&client->dev, "%s: on: %d\n", __func__, on);

	val = on ? 0x0f : 0;
	ret = gc2145_write(client, 0xf2, val);

	if (ret)
		dev_err(&client->dev, "gc2145 soft standby failed\n");
}

/*
 * V4L2 subdev video and pad level operations
 */
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int gc2145_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_mbus_code_enum *code)
#else
static int gc2145_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	if (code->index >= ARRAY_SIZE(gc2145_formats))
		return -EINVAL;

	code->code = gc2145_formats[code->index].code;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int gc2145_enum_frame_sizes(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_size_enum *fse)
#else
static int gc2145_enum_frame_sizes(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	struct gc2145 *gc2145 = to_gc2145(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i = ARRAY_SIZE(gc2145_formats);

	dev_dbg(&client->dev, "%s:\n", __func__);

	if (fse->index >= gc2145->cfg_num)
		return -EINVAL;

	while (--i)
		if (fse->code == gc2145_formats[i].code)
			break;

	fse->code = gc2145_formats[i].code;

	fse->min_width  = gc2145->framesize_cfg[fse->index].width;
	fse->max_width  = fse->min_width;
	fse->max_height = gc2145->framesize_cfg[fse->index].height;
	fse->min_height = fse->max_height;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int gc2145_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
#else
static int gc2145_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
#endif
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc2145 *gc2145 = to_gc2145(sd);

	dev_dbg(&client->dev, "%s enter\n", __func__);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		struct v4l2_mbus_framefmt *mf;
	#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		mf = v4l2_subdev_get_try_format(sd, sd_state, 0);
	#else
		mf = v4l2_subdev_get_try_format(sd, cfg, 0);
	#endif
		mutex_lock(&gc2145->lock);
		fmt->format = *mf;
		mutex_unlock(&gc2145->lock);
		return 0;
#else
	return -ENOTTY;
#endif
	}

	mutex_lock(&gc2145->lock);
	fmt->format = gc2145->format;
	mutex_unlock(&gc2145->lock);

	dev_dbg(&client->dev, "%s: %x %dx%d\n", __func__,
		gc2145->format.code, gc2145->format.width,
		gc2145->format.height);

	return 0;
}

static void __gc2145_try_frame_size_fps(struct gc2145 *gc2145,
					struct v4l2_mbus_framefmt *mf,
					const struct gc2145_framesize **size,
					unsigned int fps)
{
	const struct gc2145_framesize *fsize = &gc2145->framesize_cfg[0];
	const struct gc2145_framesize *match = NULL;
	unsigned int i = gc2145->cfg_num;
	unsigned int min_err = UINT_MAX;

	while (i--) {
		unsigned int err = abs(fsize->width - mf->width)
				+ abs(fsize->height - mf->height);
		if (err < min_err && fsize->regs[0].addr) {
			min_err = err;
			match = fsize;
		}
		fsize++;
	}

	if (!match) {
		match = &gc2145->framesize_cfg[0];
	} else {
		fsize = &gc2145->framesize_cfg[0];
		for (i = 0; i < gc2145->cfg_num; i++) {
			if (fsize->width == match->width &&
			    fsize->height == match->height &&
			    fps >= DIV_ROUND_CLOSEST(fsize->max_fps.denominator,
				fsize->max_fps.numerator))
				match = fsize;

			fsize++;
		}
	}

	mf->width  = match->width;
	mf->height = match->height;

	if (size)
		*size = match;
}

#ifdef GC2145_EXPOSURE_CONTROL
/*
 * the function is called before sensor register setting in VIDIOC_S_FMT
 */
/* Row times = Hb + Sh_delay + win_width + 4*/

static int gc2145_aec_ctrl(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	u8 value;
	static unsigned int capture_fps = 10, capture_lines = 1266;
	static unsigned int preview_fps = 20, preview_lines = 1266;
	static unsigned int lines_10ms = 1;
	static unsigned int shutter_h = 0x04, shutter_l = 0xe2;
	static unsigned int cap = 0, shutter = 0x04e2;

	dev_info(&client->dev, "%s enter\n", __func__);
	if ((mf->width == 800 && mf->height == 600) && cap == 1) {
		cap = 0;
		ret = gc2145_write(client, 0xfe, 0x00);
		ret |= gc2145_write(client, 0xb6, 0x00);
		ret |= gc2145_write(client, 0x03, shutter_h);
		ret |= gc2145_write(client, 0x04, shutter_l);
		ret |= gc2145_write(client, 0x82, 0xfa);
		ret |= gc2145_write(client, 0xb6, 0x01);
		if (ret)
			dev_err(&client->dev, "gc2145 reconfig failed!\n");
	}
	if (mf->width == 1600 && mf->height == 1200) {
		cap = 1;
		ret = gc2145_write(client, 0xfe, 0x00);
		ret |= gc2145_write(client, 0xb6, 0x00);
		ret |= gc2145_write(client, 0x82, 0xf8);

		/*shutter calculate*/
		ret |= gc2145_read(client, 0x03, &value);
		shutter_h = value;
		shutter = (value << 8);
		ret |= gc2145_read(client, 0x04, &value);
		shutter_l = value;
		shutter |= (value & 0xff);
		dev_info(&client->dev, "%s(%d) 800x600 shutter read(0x%04x)!\n",
					__func__, __LINE__, shutter);
		shutter = shutter * capture_lines / preview_lines;
		shutter = shutter * capture_fps / preview_fps;
		lines_10ms = capture_fps * capture_lines / 100;
		if (shutter > lines_10ms) {
			shutter = shutter + lines_10ms / 2;
			shutter /= lines_10ms;
			shutter *= lines_10ms;
		}
		if (shutter < 1)
			shutter = 0x276;
		dev_info(&client->dev, "%s(%d)lines_10ms(%d),cal(0x%08x)!\n",
			  __func__, __LINE__, lines_10ms, shutter);

		ret |= gc2145_write(client, 0x03, ((shutter >> 8) & 0x1f));
		ret |= gc2145_write(client, 0x04, (shutter & 0xff));
		if (ret)
			dev_err(&client->dev, "full exp reconfig failed!\n");
	}
	return ret;
}
#endif

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int gc2145_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
#else
static int gc2145_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
#endif
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int index = ARRAY_SIZE(gc2145_formats);
	struct v4l2_mbus_framefmt *mf = &fmt->format;
	const struct gc2145_framesize *size = NULL;
	struct gc2145 *gc2145 = to_gc2145(sd);

	dev_info(&client->dev, "%s enter\n", __func__);

	__gc2145_try_frame_size_fps(gc2145, mf, &size, gc2145->fps);

	while (--index >= 0)
		if (gc2145_formats[index].code == mf->code)
			break;

	if (index < 0)
		return -EINVAL;

	mf->colorspace = V4L2_COLORSPACE_SRGB;
	mf->code = gc2145_formats[index].code;
	mf->field = V4L2_FIELD_NONE;

	mutex_lock(&gc2145->lock);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		mf = v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
	#else
		mf = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
	#endif
		*mf = fmt->format;
#else
		return -ENOTTY;
#endif
	} else {
		if (gc2145->streaming) {
			mutex_unlock(&gc2145->lock);
			return -EBUSY;
		}

		gc2145->frame_size = size;
		gc2145->format = fmt->format;

	}

#ifdef GC2145_EXPOSURE_CONTROL
	if (gc2145->power_on)
		gc2145_aec_ctrl(sd, mf);
#endif
	mutex_unlock(&gc2145->lock);
	return 0;
}

static int __gc2145_power_on(struct gc2145 *gc2145)
{
	int ret;
	struct device *dev = &gc2145->client->dev;

	dev_info(dev, "%s(%d)\n", __func__, __LINE__);

	ret = regulator_enable(gc2145->poc_regulator);
	if (ret < 0) {
		dev_err(dev, "Unable to turn PoC regulator on\n");
		return ret;
	}

	usleep_range(7000, 10000);
	gc2145->power_on = true;
	return 0;
}

static void __gc2145_power_off(struct gc2145 *gc2145)
{
	int ret = 0;

	dev_info(&gc2145->client->dev, "%s(%d)\n", __func__, __LINE__);

	ret = regulator_disable(gc2145->poc_regulator);
	if (ret < 0)
		dev_warn(&gc2145->client->dev, "Unable to turn PoC regulator off\n");

	gc2145->power_on = false;
}

static int gc2145_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2145 *gc2145 = to_gc2145(sd);

	return __gc2145_power_on(gc2145);
}

static int gc2145_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2145 *gc2145 = to_gc2145(sd);

	__gc2145_power_off(gc2145);

	return 0;
}

static int gc2145_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct gc2145 *gc2145 = to_gc2145(sd);
	struct i2c_client *client = gc2145->client;

	dev_info(&client->dev, "%s(%d)\n", __func__, __LINE__);

	ret = gc2145_write_array(client, gc2145_dvp_init_regs);

	return ret;
}

static int __gc2145_start_stream(struct gc2145 *gc2145)
{
	serializer_t *serializer = gc2145->serializer;
	struct i2c_client *client = gc2145->client;
	struct device *dev = &client->dev;
	int ret = 0;

	dev_info(dev, "gc2145 device start stream\n");

	if (serializer == NULL) {
		dev_err(dev, "%s: serializer error\n", __func__);
		return -EINVAL;
	}

	if (serializer->ser_ops == NULL) {
		dev_err(dev, "%s: serializer ser_ops error\n", __func__);
		return -EINVAL;
	}

	ret = serializer->ser_ops->ser_module_init(serializer);
	if (ret) {
		dev_err(dev, "%s: serializer module_init error\n", __func__);
		return ret;
	}

	ret = gc2145_detect(gc2145);
	if (ret < 0) {
		dev_info(&client->dev, "Check id  failed:\n"
			  "check following information:\n"
			  "Power/PowerDown/Reset/Mclk/I2cBus !!\n");
		return ret;
	}

	ret = gc2145_init(&gc2145->sd, 0);
	usleep_range(10000, 20000);
	if (ret) {
		dev_err(&client->dev, "init error\n");
		return ret;
	}

	ret = gc2145_write_array(client, gc2145->frame_size->regs);
	if (ret) {
		dev_err(&client->dev, "write array error\n");
		return ret;
	}

	gc2145_set_streaming(gc2145, 1);

	return 0;
}

static int __gc2145_stop_stream(struct gc2145 *gc2145)
{
	serializer_t *serializer = gc2145->serializer;
	struct i2c_client *client = gc2145->client;
	struct device *dev = &client->dev;
	u32 delay_us, fps;
	int ret = 0;

	dev_info(dev, "gc2145 device stop stream\n");

	fps = DIV_ROUND_CLOSEST(gc2145->frame_size->max_fps.denominator,
					gc2145->frame_size->max_fps.numerator);

	/* Stop Streaming Sequence */
	gc2145_set_streaming(gc2145, 0);
	/* delay to enable oneframe complete */
	delay_us = 1000 * 1000 / fps;
	usleep_range(delay_us, delay_us+10);
	dev_info(&client->dev, "%s: sleep(%dus)\n", __func__, delay_us);

	if (ret) {
		dev_err(dev, "%s: ar0330 stop stream error\n", __func__);
		return ret;
	}

	if (serializer == NULL) {
		dev_err(dev, "%s: serializer error\n", __func__);
		return -EINVAL;
	}

	if (serializer->ser_ops == NULL) {
		dev_err(dev, "%s: serializer ser_ops error\n", __func__);
		return -EINVAL;
	}

	ret = serializer->ser_ops->ser_module_deinit(serializer);
	if (ret) {
		dev_err(dev, "%s: serializer module_deinit error\n", __func__);
		return ret;
	}

	return 0;
}

static int gc2145_s_stream(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc2145 *gc2145 = to_gc2145(sd);
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
				gc2145->frame_size->width,
				gc2145->frame_size->height,
		DIV_ROUND_CLOSEST(gc2145->frame_size->max_fps.denominator,
				  gc2145->frame_size->max_fps.numerator));

	mutex_lock(&gc2145->lock);

	on = !!on;

	if (gc2145->streaming == on)
		goto unlock_and_return;

	if (on) {
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
		ret = pm_runtime_resume_and_get(&client->dev);
#else
		ret = pm_runtime_get_sync(&client->dev);
#endif
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		ret = __gc2145_start_stream(gc2145);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__gc2145_stop_stream(gc2145);
		pm_runtime_put(&client->dev);
	}

	gc2145->streaming = on;

unlock_and_return:
	mutex_unlock(&gc2145->lock);
	return ret;
}

static int gc2145_set_test_pattern(struct gc2145 *gc2145, int value)
{
	return 0;
}

static int gc2145_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gc2145 *gc2145 =
			container_of(ctrl->handler, struct gc2145, ctrls);

	switch (ctrl->id) {
	case V4L2_CID_TEST_PATTERN:
		return gc2145_set_test_pattern(gc2145, ctrl->val);
	}

	return 0;
}

static const struct v4l2_ctrl_ops gc2145_ctrl_ops = {
	.s_ctrl = gc2145_s_ctrl,
};

static const char * const gc2145_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bars",
};

/* -----------------------------------------------------------------------------
 * V4L2 subdev internal operations
 */

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int gc2145_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct gc2145 *gc2145 = to_gc2145(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	struct v4l2_mbus_framefmt *format =
				v4l2_subdev_get_try_format(sd, fh->state, 0);
#else
	struct v4l2_mbus_framefmt *format =
				v4l2_subdev_get_try_format(sd, fh->pad, 0);
#endif

	dev_dbg(&client->dev, "%s:\n", __func__);

	gc2145_get_default_format(gc2145, format);

	return 0;
}
#endif

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int gc2145_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct gc2145 *gc2145 = to_gc2145(sd);

	config->type = V4L2_MBUS_PARALLEL;
	config->bus.parallel = gc2145->bus_cfg.bus.parallel;

	return 0;
}
#elif KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static int gc2145_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_PARALLEL;
	config->flags = V4L2_MBUS_HSYNC_ACTIVE_HIGH |
			V4L2_MBUS_VSYNC_ACTIVE_LOW |
			V4L2_MBUS_PCLK_SAMPLE_RISING;

	return 0;
}
#else
static int gc2145_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *config)
{
	config->type = V4L2_MBUS_PARALLEL;
	config->flags = V4L2_MBUS_HSYNC_ACTIVE_HIGH |
			V4L2_MBUS_VSYNC_ACTIVE_LOW |
			V4L2_MBUS_PCLK_SAMPLE_RISING;

	return 0;
}
#endif

static int gc2145_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct gc2145 *gc2145 = to_gc2145(sd);

	fi->interval = gc2145->frame_size->max_fps;

	return 0;
}

static int gc2145_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc2145 *gc2145 = to_gc2145(sd);
	const struct gc2145_framesize *size = NULL;
	struct v4l2_mbus_framefmt mf;
	unsigned int fps;
	int ret = 0;

	dev_dbg(&client->dev, "Setting %d/%d frame interval\n",
		fi->interval.numerator, fi->interval.denominator);

	mutex_lock(&gc2145->lock);

	if (gc2145->format.width == 1600)
		goto unlock;

	fps = DIV_ROUND_CLOSEST(fi->interval.denominator,
				fi->interval.numerator);
	mf = gc2145->format;
	__gc2145_try_frame_size_fps(gc2145, &mf, &size, fps);

	if (gc2145->frame_size != size) {
		dev_info(&client->dev, "%s match wxh@FPS is %dx%d@%d\n",
			__func__, size->width, size->height,
			DIV_ROUND_CLOSEST(size->max_fps.denominator,
				size->max_fps.numerator));
		ret |= gc2145_write_array(client, size->regs);
		if (ret)
			goto unlock;
		gc2145->frame_size = size;
		gc2145->fps = fps;
	}
unlock:
	mutex_unlock(&gc2145->lock);

	return ret;
}

static void gc2145_get_module_inf(struct gc2145 *gc2145,
				  struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, DRIVER_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, gc2145->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, gc2145->len_name, sizeof(inf->base.lens));
}

static long gc2145_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct gc2145 *gc2145 = to_gc2145(sd);
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		gc2145_get_module_inf(gc2145, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		gc2145_set_streaming(gc2145, !!stream);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long gc2145_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_awb_cfg *cfg;
	long ret;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = gc2145_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_AWB_CFG:
		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(cfg, up, sizeof(*cfg));
		if (!ret)
			ret = gc2145_ioctl(sd, cmd, cfg);
		else
			ret = -EFAULT;
		kfree(cfg);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = gc2145_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int gc2145_power(struct v4l2_subdev *sd, int on)
{
	struct gc2145 *gc2145 = to_gc2145(sd);
	struct i2c_client *client = gc2145->client;
	int ret = 0;

	dev_info(&client->dev, "%s(%d) on(%d)\n", __func__, __LINE__, on);

	mutex_lock(&gc2145->lock);

	/* If the power state is not modified - no work to do. */
	if (gc2145->power_on == !!on)
		goto unlock_and_return;

	if (on) {
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
		ret = pm_runtime_resume_and_get(&client->dev);
#else
		ret = pm_runtime_get_sync(&client->dev);
#endif
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		gc2145->power_on = true;
	} else {
		pm_runtime_put(&client->dev);

		gc2145->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&gc2145->lock);

	return ret;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int gc2145_enum_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_interval_enum *fie)
#else
static int gc2145_enum_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_frame_interval_enum *fie)
#endif
{
	struct gc2145 *gc2145 = to_gc2145(sd);

	if (fie->index >= gc2145->cfg_num)
		return -EINVAL;

	if (fie->code != MEDIA_BUS_FMT_UYVY8_2X8)
		return -EINVAL;

	fie->width = gc2145->framesize_cfg[fie->index].width;
	fie->height = gc2145->framesize_cfg[fie->index].height;
	fie->interval = gc2145->framesize_cfg[fie->index].max_fps;
	return 0;
}

static const struct dev_pm_ops gc2145_pm_ops = {
	SET_RUNTIME_PM_OPS(gc2145_runtime_suspend,
			   gc2145_runtime_resume, NULL)
};

static const struct v4l2_subdev_core_ops gc2145_subdev_core_ops = {
	.log_status = v4l2_ctrl_subdev_log_status,
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
	.ioctl = gc2145_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = gc2145_compat_ioctl32,
#endif
	.s_power = gc2145_power,
};

static const struct v4l2_subdev_video_ops gc2145_subdev_video_ops = {
	.s_stream = gc2145_s_stream,
	.g_frame_interval = gc2145_g_frame_interval,
	.s_frame_interval = gc2145_s_frame_interval,
#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
	.g_mbus_config = gc2145_g_mbus_config,
#endif
};

static const struct v4l2_subdev_pad_ops gc2145_subdev_pad_ops = {
	.enum_mbus_code = gc2145_enum_mbus_code,
	.enum_frame_size = gc2145_enum_frame_sizes,
	.enum_frame_interval = gc2145_enum_frame_interval,
	.get_fmt = gc2145_get_fmt,
	.set_fmt = gc2145_set_fmt,
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	.get_mbus_config = gc2145_g_mbus_config,
#endif
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_ops gc2145_subdev_ops = {
	.core  = &gc2145_subdev_core_ops,
	.video = &gc2145_subdev_video_ops,
	.pad   = &gc2145_subdev_pad_ops,
};

static const struct v4l2_subdev_internal_ops gc2145_subdev_internal_ops = {
	.open = gc2145_open,
};
#endif


static int gc2145_parse_of(struct gc2145 *gc2145)
{
	struct device *dev = &gc2145->client->dev;
	struct device_node *endpoint;
	int ret;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
		&gc2145->bus_cfg);
	if (ret) {
		dev_err(dev, "Failed to parse endpoint\n");
		of_node_put(endpoint);
		return ret;
	}

	gc2145->framesize_cfg = gc2145_dvp_framesizes;
	gc2145->cfg_num = ARRAY_SIZE(gc2145_dvp_framesizes);

	return ret;
}

static int gc2145_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct v4l2_subdev *sd;
	struct gc2145 *gc2145;
	serializer_t *serializer = NULL;
	char facing[2];
	int ret;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		DRIVER_VERSION >> 16,
		(DRIVER_VERSION & 0xff00) >> 8,
		DRIVER_VERSION & 0x00ff);

	gc2145 = devm_kzalloc(&client->dev, sizeof(*gc2145), GFP_KERNEL);
	if (!gc2145)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &gc2145->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &gc2145->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &gc2145->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &gc2145->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	gc2145->client = client;

	ret = gc2145_parse_of(gc2145);
	if (ret != 0)
		return -EINVAL;

	// poc regulator
	gc2145->poc_regulator = devm_regulator_get(dev, "poc");
	if (IS_ERR(gc2145->poc_regulator)) {
		if (PTR_ERR(gc2145->poc_regulator) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get PoC regulator (%ld)\n",
				PTR_ERR(gc2145->poc_regulator));
		else
			dev_err(dev, "Get PoC regulator deferred\n");

		ret = PTR_ERR(gc2145->poc_regulator);

		return ret;
	}

	v4l2_ctrl_handler_init(&gc2145->ctrls, 3);
	gc2145->link_frequency =
			v4l2_ctrl_new_std(&gc2145->ctrls, &gc2145_ctrl_ops,
					  V4L2_CID_PIXEL_RATE, 0,
					  GC2145_PIXEL_RATE, 1,
					  GC2145_PIXEL_RATE);

	v4l2_ctrl_new_int_menu(&gc2145->ctrls, NULL, V4L2_CID_LINK_FREQ,
			       0, 0, link_freq_menu_items);
	v4l2_ctrl_new_std_menu_items(&gc2145->ctrls, &gc2145_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(gc2145_test_pattern_menu) - 1,
				     0, 0, gc2145_test_pattern_menu);
	gc2145->sd.ctrl_handler = &gc2145->ctrls;

	if (gc2145->ctrls.error) {
		dev_err(&client->dev, "%s: control initialization error %d\n",
			__func__, gc2145->ctrls.error);
		return  gc2145->ctrls.error;
	}

	sd = &gc2145->sd;
	client->flags |= I2C_CLIENT_SCCB;
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	v4l2_i2c_subdev_init(sd, client, &gc2145_subdev_ops);

	sd->internal_ops = &gc2145_subdev_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;
#endif

#if defined(CONFIG_MEDIA_CONTROLLER)
	gc2145->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &gc2145->pad);
	if (ret < 0) {
		v4l2_ctrl_handler_free(&gc2145->ctrls);
		return ret;
	}
#endif

	mutex_init(&gc2145->lock);

	gc2145_get_default_format(gc2145, &gc2145->format);
	gc2145->frame_size = &gc2145->framesize_cfg[0];
	gc2145->format.width = gc2145->framesize_cfg[0].width;
	gc2145->format.height = gc2145->framesize_cfg[0].height;
	gc2145->fps = DIV_ROUND_CLOSEST(gc2145->framesize_cfg[0].max_fps.denominator,
			gc2145->framesize_cfg[0].max_fps.numerator);

	__gc2145_power_on(gc2145);

	memset(facing, 0, sizeof(facing));
	if (strcmp(gc2145->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 gc2145->module_index, facing,
		 DRIVER_NAME, dev_name(sd->dev));
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	ret = v4l2_async_register_subdev_sensor(sd);
#else
	ret = v4l2_async_register_subdev_sensor_common(sd);
#endif
	if (ret)
		goto error;

	/* serializer bind */
	serializer = rkcam_get_ser_by_phandle(dev);
	if (serializer != NULL) {
		dev_info(dev, "serializer bind success\n");

		gc2145->serializer = serializer;
	} else {
		dev_err(dev, "serializer bind fail\n");

		gc2145->serializer = NULL;
	}

	dev_info(&client->dev, "%s sensor driver registered !!\n", sd->name);

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;

error:
	v4l2_ctrl_handler_free(&gc2145->ctrls);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	mutex_destroy(&gc2145->lock);
	__gc2145_power_off(gc2145);
	return ret;
}

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
static int gc2145_remove(struct i2c_client *client)
#else
static void gc2145_remove(struct i2c_client *client)
#endif
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2145 *gc2145 = to_gc2145(sd);

	v4l2_ctrl_handler_free(&gc2145->ctrls);
	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	mutex_destroy(&gc2145->lock);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__gc2145_power_off(gc2145);
	pm_runtime_set_suspended(&client->dev);

#if KERNEL_VERSION(6, 1, 0) > LINUX_VERSION_CODE
	return 0;
#endif
}

static const struct of_device_id gc2145_of_match[] = {
	{ .compatible = "rockchip,galaxycore,gc2145", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, gc2145_of_match);

static struct i2c_driver gc2145_i2c_driver = {
	.driver = {
		.name	= "rkcam-gc2145",
		.pm = &gc2145_pm_ops,
		.of_match_table = of_match_ptr(gc2145_of_match),
	},
	.probe		= gc2145_probe,
	.remove		= gc2145_remove,
};

module_i2c_driver(gc2145_i2c_driver);

MODULE_AUTHOR("Benoit Parrot <bparrot@ti.com>");
MODULE_DESCRIPTION("GC2145 CMOS Image Sensor driver");
MODULE_LICENSE("GPL");
