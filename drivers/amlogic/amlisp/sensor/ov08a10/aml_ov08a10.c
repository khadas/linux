// SPDX-License-Identifier: GPL-2.0
/*
 * Sony IMX290 CMOS Image Sensor Driver
 *
 * Copyright (C) 2019 FRAMOS GmbH.
 *
 * Copyright (C) 2019 Linaro Ltd.
 * Author: Manivannan Sadhasivam <manivannan.sadhasivam@linaro.org>
 */
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>

#include "../include/mclk_api.h"

#define OV08A10_GAIN       0x3508
#define OV08A10_GAIN_L     0x3509
#define OV08A10_EXPOSURE   0x3501
#define OV08A10_EXPOSURE_L 0x3502
#define OV08A10_ID         0x530841

#define AML_SENSOR_NAME  "ov08a10-%u"

#define HDR_30FPS_1440M
//#define SDR_60FPS_1440M

struct ov08a10_regval {
	u16 reg;
	u8 val;
};

struct ov08a10_mode {
	u32 width;
	u32 height;
	u32 hmax;
	u32 link_freq_index;

	const struct ov08a10_regval *data;
	u32 data_size;
};

struct ov08a10 {
	int index;
	struct device *dev;
	struct clk *xclk;
	struct regmap *regmap;
	u8 nlanes;
	u8 bpp;
	u32 enWDRMode;

	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct v4l2_fwnode_endpoint ep;
	struct media_pad pad;
	struct v4l2_mbus_framefmt current_format;
	const struct ov08a10_mode *current_mode;

	struct gpio_desc *rst_gpio;
	struct gpio_desc *pwdn_gpio;
	struct gpio_desc *power_gpio;

	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *wdr;

	int status;
	struct mutex lock;
};

struct ov08a10_pixfmt {
	u32 code;
	u32 min_width;
	u32 max_width;
	u32 min_height;
	u32 max_height;
	u8 bpp;
};
static int ov08a10_power_on(struct ov08a10 *ov08a10);

static const struct ov08a10_pixfmt ov08a10_formats[] = {
	{ MEDIA_BUS_FMT_SBGGR10_1X10, 720, 3840, 720, 2160, 10 },
	{ MEDIA_BUS_FMT_SBGGR12_1X12, 1280, 3840, 720, 2160, 12 },
};

static const struct regmap_config ov08a10_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

#ifdef SDR_60FPS_1440M
static struct ov08a10_regval setting_3840_2160_4lane_1440m_60fps[] = {
	{0x0103, 0x01 },
	{0x0303, 0x01 },
	{0x0305, 0x5c },
	{0x0306, 0x00 },
	{0x0308, 0x03 },
	{0x0309, 0x04 },
	{0x032a, 0x00 },
	{0x300f, 0x11 },
	{0x3010, 0x01 },
	{0x3012, 0x41 },
	{0x3016, 0xf0 },
	{0x301e, 0x98 },
	{0x3031, 0xa9 },
	{0x3103, 0x92 },
	{0x3104, 0x01 },
	{0x3106, 0x10 },
	{0x3400, 0x04 },
	{0x3025, 0x03 },
	{0x3425, 0x51 },
	{0x3428, 0x01 },
	{0x3406, 0x08 },
	{0x3408, 0x03 },
	{0x340c, 0xff },
	{0x340d, 0xff },
	{0x031e, 0x09 },
	{0x3501, 0x08 },
	{0x3502, 0xe5 },
	{0x3505, 0x83 },
	{0x3508, 0x00 },
	{0x3509, 0x80 },
	{0x350a, 0x04 },
	{0x350b, 0x00 },
	{0x350c, 0x00 },
	{0x350d, 0x80 },
	{0x350e, 0x04 },
	{0x350f, 0x00 },
	{0x3600, 0x00 },
	{0x3605, 0x50 },
	{0x3609, 0xb5 },
	{0x3610, 0x69 },
	{0x360c, 0x01 },
	{0x3628, 0xa4 },
	{0x362d, 0x10 },
	{0x3660, 0x43 },
	{0x3661, 0x06 },
	{0x3662, 0x00 },
	{0x3663, 0x28 },
	{0x3664, 0x0d },
	{0x366a, 0x38 },
	{0x366b, 0xa0 },
	{0x366d, 0x00 },
	{0x366e, 0x00 },
	{0x3680, 0x00 },
	{0x3701, 0x02 },
	{0x373b, 0x02 },
	{0x373c, 0x02 },
	{0x3736, 0x02 },
	{0x3737, 0x02 },
	{0x3705, 0x00 },
	{0x3706, 0x35 },
	{0x370a, 0x00 },
	{0x370b, 0x98 },
	{0x3709, 0x49 },
	{0x3714, 0x21 },
	{0x371c, 0x00 },
	{0x371d, 0x08 },
	{0x375e, 0x0b },
	{0x3776, 0x10 },
	{0x3781, 0x02 },
	{0x3782, 0x04 },
	{0x3783, 0x02 },
	{0x3784, 0x08 },
	{0x3785, 0x08 },
	{0x3788, 0x01 },
	{0x3789, 0x01 },
	{0x3797, 0x04 },
	{0x3800, 0x00 },
	{0x3801, 0x00 },
	{0x3802, 0x00 },
	{0x3803, 0x0c },
	{0x3804, 0x0e },
	{0x3805, 0xff },
	{0x3806, 0x08 },
	{0x3807, 0x6f },
	{0x3808, 0x0f },
	{0x3809, 0x00 },
	{0x380a, 0x08 },
	{0x380b, 0x70 },
	{0x380c, 0x04 },
	{0x380d, 0x0c },
	{0x380e, 0x09 },
	{0x380f, 0x0a },
	{0x3813, 0x10 },
	{0x3814, 0x01 },
	{0x3815, 0x01 },
	{0x3816, 0x01 },
	{0x3817, 0x01 },
	{0x381c, 0x00 },
	{0x3820, 0x00 },
	{0x3821, 0x04 },
	{0x3823, 0x08 },
	{0x3826, 0x00 },
	{0x3827, 0x08 },
	{0x382d, 0x08 },
	{0x3832, 0x02 },
	{0x383c, 0x48 },
	{0x383d, 0xff },
	{0x3d85, 0x0b },
	{0x3d84, 0x40 },
	{0x3d8c, 0x63 },
	{0x3d8d, 0xd7 },
	{0x4000, 0xf8 },
	{0x4001, 0x2f },
	{0x400a, 0x01 },
	{0x400f, 0xa1 },
	{0x4010, 0x12 },
	{0x4018, 0x04 },
	{0x4008, 0x02 },
	{0x4009, 0x0d },
	{0x401a, 0x58 },
	{0x4050, 0x00 },
	{0x4051, 0x01 },
	{0x4028, 0x0f },
	{0x4052, 0x00 },
	{0x4053, 0x80 },
	{0x4054, 0x00 },
	{0x4055, 0x80 },
	{0x4056, 0x00 },
	{0x4057, 0x80 },
	{0x4058, 0x00 },
	{0x4059, 0x80 },
	{0x430b, 0xff },
	{0x430c, 0xff },
	{0x430d, 0x00 },
	{0x430e, 0x00 },
	{0x4501, 0x18 },
	{0x4502, 0x00 },
	{0x4643, 0x00 },
	{0x4640, 0x01 },
	{0x4641, 0x04 },
	{0x4800, 0x04 },
	{0x4809, 0x2b },
	{0x4813, 0x90 },
	{0x4817, 0x04 },
	{0x4833, 0x18 },
	{0x4837, 0x0a },
	{0x483b, 0x00 },
	{0x484b, 0x03 },
	{0x4850, 0x7c },
	{0x4852, 0x06 },
	{0x4856, 0x58 },
	{0x4857, 0xaa },
	{0x4862, 0x0a },
	{0x4869, 0x18 },
	{0x486a, 0xaa },
	{0x486e, 0x03 },
	{0x486f, 0x55 },
	{0x4875, 0xf0 },
	{0x5000, 0x89 },
	{0x5001, 0x40 },
	{0x5004, 0x40 },
	{0x5005, 0x00 },
	{0x5180, 0x00 },
	{0x5181, 0x10 },
	{0x580b, 0x03 },

	{0x4700, 0x2b },
	{0x4e00, 0x2b },

};

#else
static struct ov08a10_regval setting_3840_2160_4lane_800m_30fps[] = {
	{0x0103, 0x01},
	{0x0303, 0x01},
	{0x0305, 0x32},
	{0x0306, 0x00},
	{0x0308, 0x03},
	{0x0309, 0x04},
	{0x032a, 0x00},
	{0x300f, 0x11},
	{0x3010, 0x01},
	{0x3012, 0x41},
	{0x3016, 0xf0},
	{0x301e, 0x98},
	{0x3031, 0xa9},
	{0x3103, 0x92},
	{0x3104, 0x01},
	{0x3106, 0x10},
	{0x3400, 0x04},
	{0x3025, 0x03},
	{0x3425, 0x51},
	{0x3428, 0x01},
	{0x3406, 0x08},
	{0x3408, 0x03},
	{0x340c, 0xff},
	{0x340d, 0xff},
	{0x031e, 0x09},
	{0x3501, 0x08},
	{0x3502, 0xe5},
	{0x3505, 0x83},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3600, 0x00},
	{0x3605, 0x50},
	{0x3609, 0xb5},
	{0x3610, 0x69},
	{0x360c, 0x01},
	{0x3628, 0xa4},
	{0x362d, 0x10},
	{0x3660, 0x43},
	{0x3661, 0x06},
	{0x3662, 0x00},
	{0x3663, 0x28},
	{0x3664, 0x0d},
	{0x366a, 0x38},
	{0x366b, 0xa0},
	{0x366d, 0x00},
	{0x366e, 0x00},
	{0x3680, 0x00},
	{0x3701, 0x02},
	{0x373b, 0x02},
	{0x373c, 0x02},
	{0x3736, 0x02},
	{0x3737, 0x02},
	{0x3705, 0x00},
	{0x3706, 0x35},
	{0x370a, 0x00},
	{0x370b, 0x98},
	{0x3709, 0x49},
	{0x3714, 0x21},
	{0x371c, 0x00},
	{0x371d, 0x08},
	{0x375e, 0x0b},
	{0x3776, 0x10},
	{0x3781, 0x02},
	{0x3782, 0x04},
	{0x3783, 0x02},
	{0x3784, 0x08},
	{0x3785, 0x08},
	{0x3788, 0x01},
	{0x3789, 0x01},
	{0x3797, 0x04},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0e},
	{0x3805, 0xff},
	{0x3806, 0x08},
	{0x3807, 0x6f},
	{0x3808, 0x0f},
	{0x3809, 0x00},
	{0x380a, 0x08},
	{0x380b, 0x70},
	{0x380c, 0x08},
	{0x380d, 0x18},
	{0x380e, 0x09},
	{0x380f, 0x0a},
	{0x3813, 0x10},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x381c, 0x00},
	{0x3820, 0x00},
	{0x3821, 0x00},
	{0x3823, 0x08},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x382d, 0x08},
	{0x3832, 0x02},
	{0x383c, 0x48},
	{0x383d, 0xff},
	{0x3d85, 0x0b},
	{0x3d84, 0x40},
	{0x3d8c, 0x63},
	{0x3d8d, 0xd7},
	{0x4000, 0xf8},
	{0x4001, 0x2f},
	{0x400a, 0x01},
	{0x400f, 0xa1},
	{0x4010, 0x12},
	{0x4018, 0x04},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x401a, 0x58},
	{0x4050, 0x00},
	{0x4051, 0x01},
	{0x4028, 0x0f},
	{0x4052, 0x00},
	{0x4053, 0x80},
	{0x4054, 0x00},
	{0x4055, 0x80},
	{0x4056, 0x00},
	{0x4057, 0x80},
	{0x4058, 0x00},
	{0x4059, 0x80},
	{0x430b, 0xff},
	{0x430c, 0xff},
	{0x430d, 0x00},
	{0x430e, 0x00},
	{0x4501, 0x18},
	{0x4502, 0x00},
	{0x4643, 0x00},
	{0x4640, 0x01},
	{0x4641, 0x04},
	{0x4800, 0x04},
	{0x4809, 0x2b},
	{0x4813, 0x90},
	{0x4817, 0x04},
	{0x4833, 0x18},
	{0x4837, 0x14},
	{0x483b, 0x00},
	{0x484b, 0x03},
	{0x4850, 0x7c},
	{0x4852, 0x06},
	{0x4856, 0x58},
	{0x4857, 0xaa},
	{0x4862, 0x0a},
	{0x4869, 0x18},
	{0x486a, 0xaa},
	{0x486e, 0x03},
	{0x486f, 0x55},
	{0x4875, 0xf0},
	{0x5000, 0x89},
	{0x5001, 0x40},
	{0x5004, 0x40},
	{0x5005, 0x00},
	{0x5180, 0x00},
	{0x5181, 0x10},
	{0x580b, 0x03},
	{0x4700, 0x2b},
	{0x4e00, 0x2b},
};
#endif

#ifdef HDR_30FPS_1440M
static struct ov08a10_regval setting_hdr_3840_2160_4lane_1440m_30fps[] = {
	{ 0x0100, 0x00},
	{ 0x0103, 0x01},
	{ 0x0303, 0x01},
	{ 0x0305, 0x5a},
	{ 0x0306, 0x00},
	{ 0x0308, 0x03},
	{ 0x0309, 0x04},
	{ 0x032a, 0x00},
	{ 0x300f, 0x11},
	{ 0x3010, 0x01},
	{ 0x3011, 0x04},
	{ 0x3012, 0x41},
	{ 0x3016, 0xf0},
	{ 0x301e, 0x98},
	{ 0x3031, 0xa9},
	{ 0x3103, 0x92},
	{ 0x3104, 0x01},
	{ 0x3106, 0x10},
	{ 0x340c, 0xff},
	{ 0x340d, 0xff},
	{ 0x031e, 0x09},
	{ 0x3501, 0x08},
	{ 0x3502, 0xe5},
	{ 0x3505, 0x83},
	{ 0x3508, 0x00},
	{ 0x3509, 0x80},
	{ 0x350a, 0x04},
	{ 0x350b, 0x00},
	{ 0x350c, 0x00},
	{ 0x350d, 0x80},
	{ 0x350e, 0x04},
	{ 0x350f, 0x00},
	{ 0x3600, 0x00},
	{ 0x3603, 0x2c},
	{ 0x3605, 0x50},
	{ 0x3609, 0xb5},
	{ 0x3610, 0x39},
	{ 0x360c, 0x01},
	{ 0x3628, 0xa4},
	{ 0x362d, 0x10},
	{ 0x3660, 0x42},
	{ 0x3661, 0x07},
	{ 0x3662, 0x00},
	{ 0x3663, 0x28},
	{ 0x3664, 0x0d},
	{ 0x366a, 0x38},
	{ 0x366b, 0xa0},
	{ 0x366d, 0x00},
	{ 0x366e, 0x00},
	{ 0x3680, 0x00},
	{ 0x36c0, 0x00},
	{ 0x3701, 0x02},
	{ 0x373b, 0x02},
	{ 0x373c, 0x02},
	{ 0x3736, 0x02},
	{ 0x3737, 0x02},
	{ 0x3705, 0x00},
	{ 0x3706, 0x39},
	{ 0x370a, 0x00},
	{ 0x370b, 0x98},
	{ 0x3709, 0x49},
	{ 0x3714, 0x21},
	{ 0x371c, 0x00},
	{ 0x371d, 0x08},
	{ 0x3740, 0x1b},
	{ 0x3741, 0x04},
	{ 0x375e, 0x0b},
	{ 0x3760, 0x10},
	{ 0x3776, 0x10},
	{ 0x3781, 0x02},
	{ 0x3782, 0x04},
	{ 0x3783, 0x02},
	{ 0x3784, 0x08},
	{ 0x3785, 0x08},
	{ 0x3788, 0x01},
	{ 0x3789, 0x01},
	{ 0x3797, 0x04},
	{ 0x3762, 0x11},
	{ 0x3800, 0x00},
	{ 0x3801, 0x00},
	{ 0x3802, 0x00},
	{ 0x3803, 0x0c},
	{ 0x3804, 0x0e},
	{ 0x3805, 0xff},
	{ 0x3806, 0x08},
	{ 0x3807, 0x6f},
	{ 0x3808, 0x0f},
	{ 0x3809, 0x00},
	{ 0x380a, 0x08},
	{ 0x380b, 0x70},
	{ 0x380c, 0x04},
	{ 0x380d, 0x0c},
	{ 0x380e, 0x09},
	{ 0x380f, 0x0a},
	{ 0x3813, 0x10},
	{ 0x3814, 0x01},
	{ 0x3815, 0x01},
	{ 0x3816, 0x01},
	{ 0x3817, 0x01},
	{ 0x381c, 0x08},
	{ 0x3820, 0x00},
	{ 0x3821, 0x24},
	{ 0x3823, 0x08},
	{ 0x3826, 0x00},
	{ 0x3827, 0x08},
	{ 0x382d, 0x08},
	{ 0x3832, 0x02},
	{ 0x3833, 0x01},
	{ 0x383c, 0x48},
	{ 0x383d, 0xff},
	{ 0x3d85, 0x0b},
	{ 0x3d84, 0x40},
	{ 0x3d8c, 0x63},
	{ 0x3d8d, 0xd7},
	{ 0x4000, 0xf8},
	{ 0x4001, 0x2b},
	{ 0x4004, 0x00},
	{ 0x4005, 0x40},
	{ 0x400a, 0x01},
	{ 0x400f, 0xa0},
	{ 0x4010, 0x12},
	{ 0x4018, 0x00},
	{ 0x4008, 0x02},
	{ 0x4009, 0x0d},
	{ 0x401a, 0x58},
	{ 0x4050, 0x00},
	{ 0x4051, 0x01},
	{ 0x4028, 0x2f},
	{ 0x4052, 0x00},
	{ 0x4053, 0x80},
	{ 0x4054, 0x00},
	{ 0x4055, 0x80},
	{ 0x4056, 0x00},
	{ 0x4057, 0x80},
	{ 0x4058, 0x00},
	{ 0x4059, 0x80},
	{ 0x430b, 0xff},
	{ 0x430c, 0xff},
	{ 0x430d, 0x00},
	{ 0x430e, 0x00},
	{ 0x4501, 0x18},
	{ 0x4502, 0x00},
	{ 0x4643, 0x00},
	{ 0x4640, 0x01},
	{ 0x4641, 0x04},
	{ 0x4800, 0x04},
	{ 0x4809, 0x2b},
	{ 0x4813, 0x98},
	{ 0x4817, 0x04},
	{ 0x4833, 0x18},
	{ 0x4837, 0x0b},
	{ 0x483b, 0x00},
	{ 0x484b, 0x03},
	{ 0x4850, 0x7c},
	{ 0x4852, 0x06},
	{ 0x4856, 0x58},
	{ 0x4857, 0xaa},
	{ 0x4862, 0x0a},
	{ 0x4869, 0x18},
	{ 0x486a, 0xaa},
	{ 0x486e, 0x07},
	{ 0x486f, 0x55},
	{ 0x4875, 0xf0},
	{ 0x5000, 0x89},
	{ 0x5001, 0x42},
	{ 0x5004, 0x40},
	{ 0x5005, 0x00},
	{ 0x5180, 0x00},
	{ 0x5181, 0x10},
	{ 0x580b, 0x03},
	{ 0x4d00, 0x03},
	{ 0x4d01, 0xc9},
	{ 0x4d02, 0xbc},
	{ 0x4d03, 0xc6},
	{ 0x4d04, 0x4a},
	{ 0x4d05, 0x25},
	{ 0x4700, 0x2b},
	{ 0x4e00, 0x2b},
	{ 0x3501, 0x08},
	{ 0x3502, 0xe1},
	{ 0x3511, 0x00},
	{ 0x3512, 0x20},
	{ 0x3833, 0x01},
};
#else
static struct ov08a10_regval setting_hdr_3840_2160_4lane_848m_15fps[] = {
	{0x0100, 0x00},
	{0x0103, 0x01},
	{0x0303, 0x01},
	{0x0305, 0x35},
	{0x0306, 0x00},
	{0x0308, 0x03},
	{0x0309, 0x04},
	{0x032a, 0x00},
	{0x300f, 0x11},
	{0x3010, 0x01},
	{0x3011, 0x04},
	{0x3012, 0x41},
	{0x3016, 0xf0},
	{0x301e, 0x98},
	{0x3031, 0xa9},
	{0x3103, 0x92},
	{0x3104, 0x01},
	{0x3106, 0x10},
	{0x340c, 0xff},
	{0x340d, 0xff},
	{0x031e, 0x09},
	{0x3501, 0x06},
	{0x3502, 0xe5},
	{0x3505, 0x83},
	{0x3508, 0x00},
	{0x3509, 0x80},
	{0x350a, 0x04},
	{0x350b, 0x00},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3600, 0x00},
	{0x3603, 0x2c},
	{0x3605, 0x50},
	{0x3609, 0xb5},
	{0x3610, 0x39},
	{0x360c, 0x01},
	{0x3628, 0xa4},
	{0x362d, 0x10},
	{0x3660, 0x42},
	{0x3661, 0x07},
	{0x3662, 0x00},
	{0x3663, 0x28},
	{0x3664, 0x0d},
	{0x366a, 0x38},
	{0x366b, 0xa0},
	{0x366d, 0x00},
	{0x366e, 0x00},
	{0x3680, 0x00},
	//{0x36c0, 0x 1},
	{0x36c0, 0x00},
	{0x3701, 0x02},
	{0x373b, 0x02},
	{0x373c, 0x02},
	{0x3736, 0x02},
	{0x3737, 0x02},
	{0x3705, 0x00},
	{0x3706, 0x39},
	{0x370a, 0x00},
	{0x370b, 0x98},
	{0x3709, 0x49},
	{0x3714, 0x21},
	{0x371c, 0x00},
	{0x371d, 0x08},
	{0x3740, 0x1b},
	{0x3741, 0x04},
	{0x375e, 0x0b},
	{0x3760, 0x10},
	{0x3776, 0x10},
	{0x3781, 0x02},
	{0x3782, 0x04},
	{0x3783, 0x02},
	{0x3784, 0x08},
	{0x3785, 0x08},
	{0x3788, 0x01},
	{0x3789, 0x01},
	{0x3797, 0x04},
	{0x3798, 0x00},
	{0x3799, 0x00},
	{0x3762, 0x11},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0e},
	{0x3805, 0xff},
	{0x3806, 0x08},
	{0x3807, 0x6f},
	{0x3808, 0x0f},
	{0x3809, 0x00},
	{0x380a, 0x08},
	{0x380b, 0x70},
	{0x3813, 0x10},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x381c, 0x08},
	{0x3820, 0x00},
	{0x3821, 0x24},
	{0x3822, 0x54},
	{0x3823, 0x08},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x382d, 0x08},
	{0x3832, 0x02},
	{0x3833, 0x01},
	{0x383c, 0x48},
	{0x383d, 0xff},
	{0x3d85, 0x0b},
	{0x3d84, 0x40},
	{0x3d8c, 0x63},
	{0x3d8d, 0xd7},
	{0x4000, 0xf8},
	{0x4001, 0x2b},
	{0x4004, 0x00},
	{0x4005, 0x40},
	{0x400a, 0x01},
	{0x400f, 0xa0},
	{0x4010, 0x12},
	{0x4018, 0x00},
	{0x4008, 0x02},
	{0x4009, 0x0d},
	{0x401a, 0x58},
	{0x4050, 0x00},
	{0x4051, 0x01},
	{0x4028, 0x2f},
	{0x4052, 0x00},
	{0x4053, 0x80},
	{0x4054, 0x00},
	{0x4055, 0x80},
	{0x4056, 0x00},
	{0x4057, 0x80},
	{0x4058, 0x00},
	{0x4059, 0x80},
	{0x430b, 0xff},
	{0x430c, 0xff},
	{0x430d, 0x00},
	{0x430e, 0x00},
	{0x4501, 0x18},
	{0x4502, 0x00},
	{0x4643, 0x00},
	{0x4640, 0x01},
	{0x4641, 0x04},
	{0x4800, 0x04},
	{0x4809, 0x2b},
	{0x4813, 0x98},
	{0x4817, 0x04},
	{0x4833, 0x18},
	{0x4837, 0x12},
	{0x483b, 0x00},
	{0x484b, 0x03},
	{0x4850, 0x7c},
	{0x4852, 0x06},
	{0x4856, 0x58},
	{0x4857, 0xaa},
	{0x4862, 0x0a},
	{0x4869, 0x18},
	{0x486a, 0xaa},
	{0x486e, 0x07},
	{0x486f, 0x55},
	{0x4875, 0xf0},
	{0x5000, 0x89},
	{0x5001, 0x42},
	{0x5004, 0x40},
	{0x5005, 0x00},
	{0x5180, 0x00},
	{0x5181, 0x10},
	{0x580b, 0x03},
	{0x4d00, 0x03},
	{0x4d01, 0xc9},
	{0x4d02, 0xbc},
	{0x4d03, 0xc6},
	{0x4d04, 0x4a},
	{0x4d05, 0x25},
	{0x4700, 0x2b},
	{0x4e00, 0x2b},
	{0x0323, 0x02},
	{0x0325, 0x45},
	{0x0328, 0x05},
	{0x0329, 0x01},
	{0x032a, 0x00},
	{0x3106, 0x10},
	{0x380c, 0x07},
	{0x380d, 0xd0},
	{0x380e, 0x08},
	{0x380f, 0xca},
	{0x3810, 0x00},
	{0x3811, 0x00},
	{0x3812, 0x00},
	{0x3813, 0x10},
};
#endif

static const struct ov08a10_regval ov08a10_1080p_settings[] = {

};

static const struct ov08a10_regval ov08a10_720p_settings[] = {

};

static const struct ov08a10_regval ov08a10_10bit_settings[] = {

};


static const struct ov08a10_regval ov08a10_12bit_settings[] = {

};

/* supported link frequencies */
#define FREQ_INDEX_1080P	0
#define FREQ_INDEX_720P		1

/* supported link frequencies */

#if defined(HDR_30FPS_1440M) || defined(SDR_60FPS_1440M)
static const s64 ov08a10_link_freq_2lanes[] = {
	[FREQ_INDEX_1080P] = 1440000000,
	[FREQ_INDEX_720P] = 1440000000,
};

static const s64 ov08a10_link_freq_4lanes[] = {
	[FREQ_INDEX_1080P] = 1440000000,
	[FREQ_INDEX_720P] = 1440000000,
};

#else
static const s64 ov08a10_link_freq_2lanes[] = {
	[FREQ_INDEX_1080P] = 848000000,
	[FREQ_INDEX_720P] = 848000000,
};

static const s64 ov08a10_link_freq_4lanes[] = {
	[FREQ_INDEX_1080P] = 848000000,
	[FREQ_INDEX_720P] = 848000000,
};
#endif

static inline const s64 *ov08a10_link_freqs_ptr(const struct ov08a10 *ov08a10)
{
	if (ov08a10->nlanes == 2)
		return ov08a10_link_freq_2lanes;
	else {
		return ov08a10_link_freq_4lanes;
	}
}

static inline int ov08a10_link_freqs_num(const struct ov08a10 *ov08a10)
{
	if (ov08a10->nlanes == 2)
		return ARRAY_SIZE(ov08a10_link_freq_2lanes);
	else
		return ARRAY_SIZE(ov08a10_link_freq_4lanes);
}

/* Mode configs */
static const struct ov08a10_mode ov08a10_modes_2lanes[] = {
	{
		.width = 1920,
		.height = 1080,
		.hmax  = 0x1130,
		.data = ov08a10_1080p_settings,
		.data_size = ARRAY_SIZE(ov08a10_1080p_settings),

		.link_freq_index = FREQ_INDEX_1080P,
	},
	{
		.width = 1280,
		.height = 720,
		.hmax = 0x19c8,
		.data = ov08a10_720p_settings,
		.data_size = ARRAY_SIZE(ov08a10_720p_settings),

		.link_freq_index = FREQ_INDEX_720P,
	},
};


static const struct ov08a10_mode ov08a10_modes_4lanes[] = {
	{
		.width = 3840,
		.height = 2160,
		.hmax = 0x0898,
		.link_freq_index = FREQ_INDEX_1080P,
		.data = ov08a10_1080p_settings,
		.data_size = ARRAY_SIZE(ov08a10_1080p_settings),
	},
	{
		.width = 3840,
		.height = 2160,
		.hmax = 0x0ce4,
		.link_freq_index = FREQ_INDEX_720P,
		.data = ov08a10_720p_settings,
		.data_size = ARRAY_SIZE(ov08a10_720p_settings),
	},
};

static inline const struct ov08a10_mode *ov08a10_modes_ptr(const struct ov08a10 *ov08a10)
{
	if (ov08a10->nlanes == 2)
		return ov08a10_modes_2lanes;
	else
		return ov08a10_modes_4lanes;
}

static inline int ov08a10_modes_num(const struct ov08a10 *ov08a10)
{
	if (ov08a10->nlanes == 2)
		return ARRAY_SIZE(ov08a10_modes_2lanes);
	else
		return ARRAY_SIZE(ov08a10_modes_4lanes);
}

static inline struct ov08a10 *to_ov08a10(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct ov08a10, sd);
}

static inline int ov08a10_read_reg(struct ov08a10 *ov08a10, u16 addr, u8 *value)
{
	unsigned int regval;

	int i, ret;

	for (i = 0; i < 3; ++i) {
		ret = regmap_read(ov08a10->regmap, addr, &regval);
		if (0 == ret ) {
			break;
		}
	}

	if (ret)
		dev_err(ov08a10->dev, "I2C read with i2c transfer failed for addr: %x, ret %d\n", addr, ret);

	*value = regval & 0xff;
	return 0;
}

static int ov08a10_write_reg(struct ov08a10 *ov08a10, u16 addr, u8 value)
{
	int i, ret;

	for (i = 0; i < 3; i++) {
		ret = regmap_write(ov08a10->regmap, addr, value);
		if (0 == ret) {
			break;
		}
	}

	if (ret)
		dev_err(ov08a10->dev, "I2C write failed for addr: %x, ret %d\n", addr, ret);

	return ret;
}

static int ov08a10_get_id(struct ov08a10 *ov08a10)
{
	int rtn = -EINVAL;
	uint32_t sensor_id = 0;
	u8 val = 0;

	ov08a10_read_reg(ov08a10, 0x300a, &val);
	sensor_id |= (val << 16);
	ov08a10_read_reg(ov08a10, 0x300b, &val);
	sensor_id |= (val << 8);
	ov08a10_read_reg(ov08a10, 0x300c, &val);
	sensor_id |= val;

	if (sensor_id != OV08A10_ID) {
		dev_err(ov08a10->dev, "Failed to get ov08a10 id: 0x%x\n", sensor_id);
		return rtn;
	} else {
		dev_err(ov08a10->dev, "success get ov08a10 id 0x%x", sensor_id);
	}

	return 0;
}

static int ov08a10_set_register_array(struct ov08a10 *ov08a10,
				     const struct ov08a10_regval *settings,
				     unsigned int num_settings)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < num_settings; ++i, ++settings) {
		ret = ov08a10_write_reg(ov08a10, settings->reg, settings->val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ov08a10_set_gain(struct ov08a10 *ov08a10, u32 value)
{
	int ret;
	ret = ov08a10_write_reg(ov08a10, OV08A10_GAIN, (value >> 8) & 0xFF);
	if (ret)
		dev_err(ov08a10->dev, "Unable to write gain_H\n");
	ret = ov08a10_write_reg(ov08a10, OV08A10_GAIN_L, value & 0xFF);
	if (ret)
		dev_err(ov08a10->dev, "Unable to write gain_L\n");

	return ret;
}

static int ov08a10_set_exposure(struct ov08a10 *ov08a10, u32 value)
{
	int ret;
	ret = ov08a10_write_reg(ov08a10, OV08A10_EXPOSURE, (value >> 8) & 0xFF);
	if (ret)
		dev_err(ov08a10->dev, "Unable to write gain_H\n");
	ret = ov08a10_write_reg(ov08a10, OV08A10_EXPOSURE_L, value & 0xFF);
	if (ret)
		dev_err(ov08a10->dev, "Unable to write gain_L\n");
	return ret;
}


/* Stop streaming */
static int ov08a10_stop_streaming(struct ov08a10 *ov08a10)
{
	ov08a10->enWDRMode = WDR_MODE_NONE;
	return ov08a10_write_reg(ov08a10, 0x0100, 0x00);
}

static int ov08a10_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov08a10 *ov08a10 = container_of(ctrl->handler,
					     struct ov08a10, ctrls);
	int ret = 0;

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(ov08a10->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		ret = ov08a10_set_gain(ov08a10, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = ov08a10_set_exposure(ov08a10, ctrl->val);
		break;
	case V4L2_CID_HBLANK:
		break;
	case V4L2_CID_AML_MODE:
		ov08a10->enWDRMode = ctrl->val;
		break;
	default:
		dev_err(ov08a10->dev, "Error ctrl->id %u, flag 0x%lx\n",
			ctrl->id, ctrl->flags);
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(ov08a10->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ov08a10_ctrl_ops = {
	.s_ctrl = ov08a10_set_ctrl,
};
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov08a10_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#else
static int ov08a10_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	if (code->index >= ARRAY_SIZE(ov08a10_formats))
		return -EINVAL;

	code->code = ov08a10_formats[code->index].code;

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov08a10_enum_frame_size(struct v4l2_subdev *sd,
			        struct v4l2_subdev_state *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#else
static int ov08a10_enum_frame_size(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	if (fse->index >= ARRAY_SIZE(ov08a10_formats))
		return -EINVAL;

	fse->min_width = ov08a10_formats[fse->index].min_width;
	fse->min_height = ov08a10_formats[fse->index].min_height;;
	fse->max_width = ov08a10_formats[fse->index].max_width;
	fse->max_height = ov08a10_formats[fse->index].max_height;

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov08a10_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *cfg,
			  struct v4l2_subdev_format *fmt)
#else
static int ov08a10_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
#endif
{
	struct ov08a10 *ov08a10 = to_ov08a10(sd);
	struct v4l2_mbus_framefmt *framefmt;

	mutex_lock(&ov08a10->lock);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		framefmt = v4l2_subdev_get_try_format(&ov08a10->sd, cfg,
						      fmt->pad);
	else
		framefmt = &ov08a10->current_format;

	fmt->format = *framefmt;

	mutex_unlock(&ov08a10->lock);

	return 0;
}


static inline u8 ov08a10_get_link_freq_index(struct ov08a10 *ov08a10)
{
	return ov08a10->current_mode->link_freq_index;
}

static s64 ov08a10_get_link_freq(struct ov08a10 *ov08a10)
{
	u8 index = ov08a10_get_link_freq_index(ov08a10);

	return *(ov08a10_link_freqs_ptr(ov08a10) + index);
}

static u64 ov08a10_calc_pixel_rate(struct ov08a10 *ov08a10)
{
	s64 link_freq = ov08a10_get_link_freq(ov08a10);
	u8 nlanes = ov08a10->nlanes;
	u64 pixel_rate;

	/* pixel rate = link_freq * 2 * nr_of_lanes / bits_per_sample */
	pixel_rate = link_freq * 2 * nlanes;
	do_div(pixel_rate, ov08a10->bpp);
	return pixel_rate;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov08a10_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *cfg,
			struct v4l2_subdev_format *fmt)
#else
static int ov08a10_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct ov08a10 *ov08a10 = to_ov08a10(sd);
	const struct ov08a10_mode *mode;
	struct v4l2_mbus_framefmt *format;
	unsigned int i,ret;

	mutex_lock(&ov08a10->lock);

	mode = v4l2_find_nearest_size(ov08a10_modes_ptr(ov08a10),
				 ov08a10_modes_num(ov08a10),
				width, height,
				fmt->format.width, fmt->format.height);

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;

	for (i = 0; i < ARRAY_SIZE(ov08a10_formats); i++) {
		if (ov08a10_formats[i].code == fmt->format.code) {
			dev_err(ov08a10->dev, " zzw find proper format %d \n",i);
			break;
		}
	}

	if (i >= ARRAY_SIZE(ov08a10_formats)) {
		i = 0;
		dev_err(ov08a10->dev, " zzw No format. reset i = 0 \n");
	}

	fmt->format.code = ov08a10_formats[i].code;
	fmt->format.field = V4L2_FIELD_NONE;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		dev_err(ov08a10->dev, " zzw try format \n");
		format = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		mutex_unlock(&ov08a10->lock);
		return 0;
	} else {
		dev_err(ov08a10->dev, " zzw set format, w %d, h %d, code 0x%x \n",
			fmt->format.width, fmt->format.height,
			fmt->format.code);
		format = &ov08a10->current_format;
		ov08a10->current_mode = mode;
		ov08a10->bpp = ov08a10_formats[i].bpp;

		if (ov08a10->link_freq)
			__v4l2_ctrl_s_ctrl(ov08a10->link_freq, ov08a10_get_link_freq_index(ov08a10) );
		if (ov08a10->pixel_rate)
			__v4l2_ctrl_s_ctrl_int64(ov08a10->pixel_rate, ov08a10_calc_pixel_rate(ov08a10) );
	}

	*format = fmt->format;
	ov08a10->status = 0;

	mutex_unlock(&ov08a10->lock);

	if (ov08a10->enWDRMode) {
		/* Set init register settings */
		#ifdef HDR_30FPS_1440M
		ret = ov08a10_set_register_array(ov08a10, setting_hdr_3840_2160_4lane_1440m_30fps,
			ARRAY_SIZE(setting_hdr_3840_2160_4lane_1440m_30fps));
		dev_err(ov08a10->dev, "%s:%d ***************WDR 1440M******\n",__FUNCTION__,__LINE__);
		#else
		ret = ov08a10_set_register_array(ov08a10, setting_hdr_3840_2160_4lane_848m_15fps,
				ARRAY_SIZE(setting_hdr_3840_2160_4lane_848m_15fps));
		dev_err(ov08a10->dev, "%s:%d ***************WDR 848M******\n",__FUNCTION__,__LINE__);
		#endif
		if (ret < 0) {
			dev_err(ov08a10->dev, "Could not set init registers\n");
			return ret;
		} else
			dev_err(ov08a10->dev, "ov08a10 wdr mode init...\n");
	} else {
		/* Set init register settings */

		#ifdef SDR_60FPS_1440M
		ret = ov08a10_set_register_array(ov08a10, setting_3840_2160_4lane_1440m_60fps,
			ARRAY_SIZE(setting_3840_2160_4lane_1440m_60fps));
		dev_err(ov08a10->dev, "%s:%d ***************1440M SDR******\n",__FUNCTION__,__LINE__);
		#else
		ret = ov08a10_set_register_array(ov08a10, setting_3840_2160_4lane_800m_30fps,
			ARRAY_SIZE(setting_3840_2160_4lane_800m_30fps));
		#endif

		if (ret < 0) {
			dev_err(ov08a10->dev, "Could not set init registers\n");
			return ret;
		} else
			dev_err(ov08a10->dev, "ov08a10 linear mode init...\n");
	}

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
int ov08a10_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *cfg,
			     struct v4l2_subdev_selection *sel)
#else
int ov08a10_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_subdev_selection *sel)
#endif
{
	int rtn = 0;
	struct ov08a10 *ov08a10 = to_ov08a10(sd);
	const struct ov08a10_mode *mode = ov08a10->current_mode;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_DEFAULT:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = mode->width;
		sel->r.height = mode->height;
	break;
	case V4L2_SEL_TGT_CROP:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = mode->width;
		sel->r.height = mode->height;
	break;
	default:
		rtn = -EINVAL;
		dev_err(ov08a10->dev, "Error support target: 0x%x\n", sel->target);
	break;
	}

	return rtn;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov08a10_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_state *cfg)
#else
static int ov08a10_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg)
#endif
{
	struct v4l2_subdev_format fmt = { 0 };

	fmt.which = cfg ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.format.width = 1920;
	fmt.format.height = 1080;

	ov08a10_set_fmt(subdev, cfg, &fmt);

	return 0;
}

/* Start streaming */
static int ov08a10_start_streaming(struct ov08a10 *ov08a10)
{

	/* Start streaming */
	return ov08a10_write_reg(ov08a10, 0x0100, 0x01);
}

static int ov08a10_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov08a10 *ov08a10 = to_ov08a10(sd);
	int ret = 0;

	if (ov08a10->status == enable)
		return ret;
	else
		ov08a10->status = enable;

	if (enable) {
		ret = ov08a10_start_streaming(ov08a10);
		if (ret) {
			dev_err(ov08a10->dev, "Start stream failed\n");
			goto unlock_and_return;
		}

		dev_info(ov08a10->dev, "stream on\n");
	} else {
		ov08a10_stop_streaming(ov08a10);

		dev_info(ov08a10->dev, "stream off\n");
	}

unlock_and_return:

	return ret;
}

static int ov08a10_power_on(struct ov08a10 *ov08a10)
{
	int ret;

	reset_am_enable(ov08a10->dev,"reset", 1);

	ret = mclk_enable(ov08a10->dev,24000000);
	if (ret < 0 )
		dev_err(ov08a10->dev, "set mclk fail\n");
	udelay(30);

	// 30ms
	usleep_range(30000, 31000);

	return 0;
}

static int ov08a10_power_off(struct ov08a10 *ov08a10)
{
	mclk_disable(ov08a10->dev);

	reset_am_enable(ov08a10->dev,"reset", 0);

	return 0;
}

static int ov08a10_power_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov08a10 *ov08a10 = to_ov08a10(sd);

	reset_am_enable(ov08a10->dev,"reset", 0);

	return 0;
}

static int ov08a10_power_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov08a10 *ov08a10 = to_ov08a10(sd);

	reset_am_enable(ov08a10->dev,"reset", 1);

	return 0;
}

static int ov08a10_log_status(struct v4l2_subdev *sd)
{
	struct ov08a10 *ov08a10 = to_ov08a10(sd);

	dev_info(ov08a10->dev, "log status done\n");

	return 0;
}

int ov08a10_sbdev_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh) {
	struct ov08a10 *ov08a10 = to_ov08a10(sd);
	ov08a10_power_on(ov08a10);
	return 0;
}

int ov08a10_sbdev_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh) {
	struct ov08a10 *ov08a10 = to_ov08a10(sd);
	ov08a10_set_stream(sd, 0);
	ov08a10_power_off(ov08a10);
	return 0;
}


static const struct dev_pm_ops ov08a10_pm_ops = {
	SET_RUNTIME_PM_OPS(ov08a10_power_suspend, ov08a10_power_resume, NULL)
};

const struct v4l2_subdev_core_ops ov08a10_core_ops = {
	.log_status = ov08a10_log_status,
};

static const struct v4l2_subdev_video_ops ov08a10_video_ops = {
	.s_stream = ov08a10_set_stream,
};

static const struct v4l2_subdev_pad_ops ov08a10_pad_ops = {
	.init_cfg = ov08a10_entity_init_cfg,
	.enum_mbus_code = ov08a10_enum_mbus_code,
	.enum_frame_size = ov08a10_enum_frame_size,
	.get_selection = ov08a10_get_selection,
	.get_fmt = ov08a10_get_fmt,
	.set_fmt = ov08a10_set_fmt,
};
static struct v4l2_subdev_internal_ops ov08a10_internal_ops = {
	.open = ov08a10_sbdev_open,
	.close = ov08a10_sbdev_close,
};

static const struct v4l2_subdev_ops ov08a10_subdev_ops = {
	.core = &ov08a10_core_ops,
	.video = &ov08a10_video_ops,
	.pad = &ov08a10_pad_ops,
};

static const struct media_entity_operations ov08a10_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static struct v4l2_ctrl_config wdr_cfg = {
	.ops = &ov08a10_ctrl_ops,
	.id = V4L2_CID_AML_MODE,
	.name = "wdr mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 2,
	.step = 1,
	.def = 0,
};

static int ov08a10_ctrls_init(struct ov08a10 *ov08a10)
{
	int rtn = 0;

	v4l2_ctrl_handler_init(&ov08a10->ctrls, 4);

	v4l2_ctrl_new_std(&ov08a10->ctrls, &ov08a10_ctrl_ops,
				V4L2_CID_GAIN, 0, 0xF0, 1, 0);

	v4l2_ctrl_new_std(&ov08a10->ctrls, &ov08a10_ctrl_ops,
				V4L2_CID_EXPOSURE, 0, 0xffff, 1, 0);

	ov08a10->link_freq = v4l2_ctrl_new_int_menu(&ov08a10->ctrls,
					       &ov08a10_ctrl_ops,
					       V4L2_CID_LINK_FREQ,
					       ov08a10_link_freqs_num(ov08a10) - 1,
					       0, ov08a10_link_freqs_ptr(ov08a10) );

	if (ov08a10->link_freq)
		ov08a10->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ov08a10->pixel_rate = v4l2_ctrl_new_std(&ov08a10->ctrls,
					       &ov08a10_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       1, INT_MAX, 1,
					       ov08a10_calc_pixel_rate(ov08a10));

	ov08a10->wdr = v4l2_ctrl_new_custom(&ov08a10->ctrls, &wdr_cfg, NULL);

	ov08a10->sd.ctrl_handler = &ov08a10->ctrls;

	if (ov08a10->ctrls.error) {
		dev_err(ov08a10->dev, "Control initialization a error  %d\n",
			ov08a10->ctrls.error);
		rtn = ov08a10->ctrls.error;
	}

	return rtn;
}

static int ov08a10_parse_power(struct ov08a10 *ov08a10)
{
	int rtn = 0;

	ov08a10->rst_gpio = devm_gpiod_get_optional(ov08a10->dev,
						"reset",
						GPIOD_OUT_LOW);
	if (IS_ERR(ov08a10->rst_gpio)) {
		dev_err(ov08a10->dev, "Cannot get reset gpio\n");
		rtn = PTR_ERR(ov08a10->rst_gpio);
		goto err_return;
	}

	/*ov08a10->pwdn_gpio = devm_gpiod_get_optional(ov08a10->dev,
						"pwdn",
						GPIOD_OUT_LOW);
	if (IS_ERR(ov08a10->pwdn_gpio)) {
		dev_err(ov08a10->dev, "Cannot get pwdn gpio\n");
		rtn = PTR_ERR(ov08a10->pwdn_gpio);
		goto err_return;
	}*/

err_return:

	return rtn;
}

/*
 * Returns 0 if all link frequencies used by the driver for the given number
 * of MIPI data lanes are mentioned in the device tree, or the value of the
 * first missing frequency otherwise.
 */
static s64 ov08a10_check_link_freqs(const struct ov08a10 *ov08a10,
				   const struct v4l2_fwnode_endpoint *ep)
{
	int i, j;
	const s64 *freqs = ov08a10_link_freqs_ptr(ov08a10);
	int freqs_count = ov08a10_link_freqs_num(ov08a10);

	for (i = 0; i < freqs_count; i++) {
		for (j = 0; j < ep->nr_of_link_frequencies; j++) {
			if (freqs[i] == ep->link_frequencies[j]) {
				return 0;
			}
		}
		if (j == ep->nr_of_link_frequencies)
			return freqs[i];
	}
	return 0;
}

static int ov08a10_parse_endpoint(struct ov08a10 *ov08a10)
{
	int rtn = 0;
	s64 fq;
	struct fwnode_handle *endpoint = NULL;
	struct device_node *node = NULL;

	//endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(ov08a10->dev), NULL);
	for_each_endpoint_of_node(ov08a10->dev->of_node, node) {
		if (strstr(node->name, "ov08a10")) {
			endpoint = of_fwnode_handle(node);
			break;
		}
	}

	rtn = v4l2_fwnode_endpoint_alloc_parse(endpoint, &ov08a10->ep);
	fwnode_handle_put(endpoint);
	if (rtn) {
		dev_err(ov08a10->dev, "Parsing endpoint node failed\n");
		rtn = -EINVAL;
		goto err_return;
	}

	/* Only CSI2 is supported for now */
	if (ov08a10->ep.bus_type != V4L2_MBUS_CSI2_DPHY) {
		dev_err(ov08a10->dev, "Unsupported bus type, should be CSI2\n");
		rtn = -EINVAL;
		goto err_free;
	}

	ov08a10->nlanes = ov08a10->ep.bus.mipi_csi2.num_data_lanes;
	if (ov08a10->nlanes != 2 && ov08a10->nlanes != 4) {
		dev_err(ov08a10->dev, "Invalid data lanes: %d\n", ov08a10->nlanes);
		rtn = -EINVAL;
		goto err_free;
	}
	dev_info(ov08a10->dev, "Using %u data lanes\n", ov08a10->nlanes);

	if (!ov08a10->ep.nr_of_link_frequencies) {
		dev_err(ov08a10->dev, "link-frequency property not found in DT\n");
		rtn = -EINVAL;
		goto err_free;
	}

	/* Check that link frequences for all the modes are in device tree */
	fq = ov08a10_check_link_freqs(ov08a10, &ov08a10->ep);
	if (fq) {
		dev_err(ov08a10->dev, "Link frequency of %lld is not supported\n", fq);
		rtn = -EINVAL;
		goto err_free;
	}

	return rtn;

err_free:
	v4l2_fwnode_endpoint_free(&ov08a10->ep);
err_return:
	return rtn;
}


static int ov08a10_register_subdev(struct ov08a10 *ov08a10)
{
	int rtn = 0;

	v4l2_i2c_subdev_init(&ov08a10->sd, ov08a10->client, &ov08a10_subdev_ops);

	ov08a10->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ov08a10->sd.dev = &ov08a10->client->dev;
	ov08a10->sd.internal_ops = &ov08a10_internal_ops;
	ov08a10->sd.entity.ops = &ov08a10_subdev_entity_ops;
	ov08a10->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	snprintf(ov08a10->sd.name, sizeof(ov08a10->sd.name), AML_SENSOR_NAME, ov08a10->index);

	ov08a10->pad.flags = MEDIA_PAD_FL_SOURCE;
	rtn = media_entity_pads_init(&ov08a10->sd.entity, 1, &ov08a10->pad);
	if (rtn < 0) {
		dev_err(ov08a10->dev, "Could not register media entity\n");
		goto err_return;
	}

	rtn = v4l2_async_register_subdev(&ov08a10->sd);
	if (rtn < 0) {
		dev_err(ov08a10->dev, "Could not register v4l2 device\n");
		goto err_return;
	}

err_return:
	return rtn;
}

static int ov08a10_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ov08a10 *ov08a10;
	int ret = -EINVAL;


	ov08a10 = devm_kzalloc(dev, sizeof(*ov08a10), GFP_KERNEL);
	if (!ov08a10)
		return -ENOMEM;
	dev_err(dev, "i2c dev addr 0x%x, name %s \n", client->addr, client->name);

	ov08a10->dev = dev;
	ov08a10->client = client;

	ov08a10->regmap = devm_regmap_init_i2c(client, &ov08a10_regmap_config);
	if (IS_ERR(ov08a10->regmap)) {
		dev_err(dev, "Unable to initialize I2C\n");
		return -ENODEV;
	}

	if (of_property_read_u32(dev->of_node, "index", &ov08a10->index)) {
		dev_err(dev, "Failed to read sensor index. default to 0\n");
		ov08a10->index = 0;
	}


	ret = ov08a10_parse_endpoint(ov08a10);
	if (ret) {
		dev_err(ov08a10->dev, "Error parse endpoint\n");
		goto return_err;
	}

	ret = ov08a10_parse_power(ov08a10);
	if (ret) {
		dev_err(ov08a10->dev, "Error parse power ctrls\n");
		goto free_err;
	}

	mutex_init(&ov08a10->lock);

	/* Power on the device to match runtime PM state below */
	dev_err(dev, "bef get id. pwdn -0, reset - 1\n");

	ret = ov08a10_power_on(ov08a10);
	if (ret < 0) {
		dev_err(dev, "Could not power on the device\n");
		goto free_err;
	}


	ret = ov08a10_get_id(ov08a10);
	if (ret) {
		dev_err(dev, "Could not get id\n");
		ov08a10_power_off(ov08a10);
		goto free_err;
	}

	/*
	 * Initialize the frame format. In particular, ov08a10->current_mode
	 * and ov08a10->bpp are set to defaults: ov08a10_calc_pixel_rate() call
	 * below in ov08a10_ctrls_init relies on these fields.
	 */
	ov08a10_entity_init_cfg(&ov08a10->sd, NULL);

	ret = ov08a10_ctrls_init(ov08a10);
	if (ret) {
		dev_err(ov08a10->dev, "Error ctrls init\n");
		goto free_ctrl;
	}

	ret = ov08a10_register_subdev(ov08a10);
	if (ret) {
		dev_err(ov08a10->dev, "Error register subdev\n");
		goto free_entity;
	}

	v4l2_fwnode_endpoint_free(&ov08a10->ep);

	dev_info(ov08a10->dev, "probe done \n");

	return 0;

free_entity:
	media_entity_cleanup(&ov08a10->sd.entity);
free_ctrl:
	v4l2_ctrl_handler_free(&ov08a10->ctrls);
	mutex_destroy(&ov08a10->lock);
free_err:
	v4l2_fwnode_endpoint_free(&ov08a10->ep);
return_err:
	return ret;
}

static int ov08a10_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov08a10 *ov08a10 = to_ov08a10(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	mutex_destroy(&ov08a10->lock);

	ov08a10_power_off(ov08a10);

	return 0;
}

static const struct of_device_id ov08a10_of_match[] = {
	{ .compatible = "omini, ov08a10" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ov08a10_of_match);

static struct i2c_driver ov08a10_i2c_driver = {
	.probe_new  = ov08a10_probe,
	.remove = ov08a10_remove,
	.driver = {
		.name  = "ov08a10",
		.pm = &ov08a10_pm_ops,
		.of_match_table = of_match_ptr(ov08a10_of_match),
	},
};

module_i2c_driver(ov08a10_i2c_driver);

MODULE_DESCRIPTION("Sony IMX290 CMOS Image Sensor Driver");
MODULE_AUTHOR("keke.li");
MODULE_AUTHOR("keke.li <keke.li@amlogic.com>");
MODULE_LICENSE("GPL v2");
