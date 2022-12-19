// SPDX-License-Identifier: GPL-2.0
/*
 * Omni OV5640 CMOS Image Sensor Driver
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

#define OV5640_GAIN       0x3508
#define OV5640_EXPOSURE   0x3501
#define OV5640_ID         0x5640

#define AML_SENSOR_NAME  "ov5640-%u"

struct ov5640_regval {
	u16 reg;
	u8 val;
};

struct ov5640_mode {
	u32 width;
	u32 height;
	u32 hmax;
	u32 link_freq_index;

	const struct ov5640_regval *data;
	u32 data_size;
};

struct ov5640 {
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
	const struct ov5640_mode *current_mode;

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

struct ov5640_pixfmt {
	u32 code;
	u32 min_width;
	u32 max_width;
	u32 min_height;
	u32 max_height;
	u8 bpp;
};
static int ov5640_power_on(struct ov5640 *ov5640);

static const struct ov5640_pixfmt ov5640_formats[] = {
	{ MEDIA_BUS_FMT_YUYV8_2X8, 1920, 2592, 1080, 1944, 8 },
};

static const struct regmap_config ov5640_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static struct ov5640_regval setting_1920_1080_2lane_672m_30fps[] = {
	{0x3103, 0x11},
	{0x3008, 0x82},
	{0x3008, 0x42},
	{0x3103, 0x03},
	{0x3017, 0x00},
	{0x3018, 0x00},
	{0x3034, 0x18},
	{0x3035, 0x11},
	{0x3036, 0x54},
	{0x3037, 0x13},
	{0x3108, 0x01},
	{0x3630, 0x36},
	{0x3631, 0x0e},
	{0x3632, 0xe2},
	{0x3633, 0x12},
	{0x3621, 0xe0},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3905, 0x02},
	{0x3906, 0x10},
	{0x3901, 0x0a},
	{0x3731, 0x12},
	{0x3600, 0x08},
	{0x3601, 0x33},
	{0x302d, 0x60},
	{0x3620, 0x52},
	{0x371b, 0x20},
	{0x471c, 0x50},
	{0x3a13, 0x43},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3635, 0x13},
	{0x3636, 0x03},
	{0x3634, 0x40},
	{0x3622, 0x01},
	{0x3c01, 0x34},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c06, 0x00},
	{0x3c07, 0x07},
	{0x3c08, 0x00},
	{0x3c09, 0x1c},
	{0x3c0a, 0x9c},
	{0x3c0b, 0x40},
	{0x3820, 0x40},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3800, 0x01},
	{0x3801, 0x50},
	{0x3802, 0x01},
	{0x3803, 0xb2},
	{0x3804, 0x08},
	{0x3805, 0xef},
	{0x3806, 0x05},
	{0x3807, 0xf1},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380c, 0x09},
	{0x380d, 0xc4},
	{0x380e, 0x04},
	{0x380f, 0x60},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3618, 0x04},
	{0x3612, 0x2b},
	{0x3708, 0x63},
	{0x3709, 0x12},
	{0x370c, 0x00},
	{0x3a02, 0x04},
	{0x3a03, 0x60},
	{0x3a08, 0x01},
	{0x3a09, 0x50},
	{0x3a0a, 0x01},
	{0x3a0b, 0x18},
	{0x3a0e, 0x03},
	{0x3a0d, 0x04},
	{0x3a14, 0x04},
	{0x3a15, 0x60},
	{0x4001, 0x02},
	{0x4004, 0x06},
	{0x4050, 0x6e},
	{0x4051, 0x8f},
	{0x3000, 0x00},
	{0x3002, 0x1c},
	{0x3004, 0xff},
	{0x3006, 0xc3},
	{0x300e, 0x41},
	{0x302e, 0x08},
	{0x4300, 0x33},
	{0x501f, 0x00},
	{0x5684, 0x07},
	{0x5685, 0xa0},
	{0x5686, 0x04},
	{0x5687, 0x40},
	{0x4713, 0x02},
	{0x4407, 0x04},
	{0x440e, 0x00},
	{0x460b, 0x37},
	{0x460c, 0x20},
	{0x4837, 0x0a},
	{0x3824, 0x04},
	{0x5000, 0xa7},
	{0x5001, 0x83},
	{0x5180, 0xff},
	{0x5181, 0xf2},
	{0x5182, 0x00},
	{0x5183, 0x14},
	{0x5184, 0x25},
	{0x5185, 0x24},
	{0x5186, 0x09},
	{0x5187, 0x09},
	{0x5188, 0x09},
	{0x5189, 0x75},
	{0x518a, 0x54},
	{0x518b, 0xe0},
	{0x518c, 0xb2},
	{0x518d, 0x42},
	{0x518e, 0x3d},
	{0x518f, 0x56},
	{0x5190, 0x46},
	{0x5191, 0xf8},
	{0x5192, 0x04},
	{0x5193, 0x70},
	{0x5194, 0xf0},
	{0x5195, 0xf0},
	{0x5196, 0x03},
	{0x5197, 0x01},
	{0x5198, 0x04},
	{0x5199, 0x12},
	{0x519a, 0x04},
	{0x519b, 0x00},
	{0x519c, 0x06},
	{0x519d, 0x82},
	{0x519e, 0x38},
	{0x5381, 0x1e},
	{0x5382, 0x5b},
	{0x5383, 0x08},
	{0x5384, 0x0a},
	{0x5385, 0x7e},
	{0x5386, 0x88},
	{0x5387, 0x7c},
	{0x5388, 0x6c},
	{0x5389, 0x10},
	{0x538a, 0x01},
	{0x538b, 0x98},
	{0x5300, 0x08},
	{0x5301, 0x30},
	{0x5302, 0x10},
	{0x5303, 0x00},
	{0x5304, 0x08},
	{0x5305, 0x30},
	{0x5306, 0x08},
	{0x5307, 0x16},
	{0x5309, 0x08},
	{0x530a, 0x30},
	{0x530b, 0x04},
	{0x530c, 0x06},
	{0x5480, 0x01},
	{0x5481, 0x08},
	{0x5482, 0x14},
	{0x5483, 0x28},
	{0x5484, 0x51},
	{0x5485, 0x65},
	{0x5486, 0x71},
	{0x5487, 0x7d},
	{0x5488, 0x87},
	{0x5489, 0x91},
	{0x548a, 0x9a},
	{0x548b, 0xaa},
	{0x548c, 0xb8},
	{0x548d, 0xcd},
	{0x548e, 0xdd},
	{0x548f, 0xea},
	{0x5490, 0x1d},
	{0x5580, 0x02},
	{0x5583, 0x40},
	{0x5584, 0x10},
	{0x5589, 0x10},
	{0x558a, 0x00},
	{0x558b, 0xf8},
	{0x5800, 0x23},
	{0x5801, 0x14},
	{0x5802, 0x0f},
	{0x5803, 0x0f},
	{0x5804, 0x12},
	{0x5805, 0x26},
	{0x5806, 0x0c},
	{0x5807, 0x08},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x08},
	{0x580b, 0x0d},
	{0x580c, 0x08},
	{0x580d, 0x03},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x09},
	{0x5812, 0x07},
	{0x5813, 0x03},
	{0x5814, 0x00},
	{0x5815, 0x01},
	{0x5816, 0x03},
	{0x5817, 0x08},
	{0x5818, 0x0d},
	{0x5819, 0x08},
	{0x581a, 0x05},
	{0x581b, 0x06},
	{0x581c, 0x08},
	{0x581d, 0x0e},
	{0x581e, 0x29},
	{0x581f, 0x17},
	{0x5820, 0x11},
	{0x5821, 0x11},
	{0x5822, 0x15},
	{0x5823, 0x28},
	{0x5824, 0x46},
	{0x5825, 0x26},
	{0x5826, 0x08},
	{0x5827, 0x26},
	{0x5828, 0x64},
	{0x5829, 0x26},
	{0x582a, 0x24},
	{0x582b, 0x22},
	{0x582c, 0x24},
	{0x582d, 0x24},
	{0x582e, 0x06},
	{0x582f, 0x22},
	{0x5830, 0x40},
	{0x5831, 0x42},
	{0x5832, 0x24},
	{0x5833, 0x26},
	{0x5834, 0x24},
	{0x5835, 0x22},
	{0x5836, 0x22},
	{0x5837, 0x26},
	{0x5838, 0x44},
	{0x5839, 0x24},
	{0x583a, 0x26},
	{0x583b, 0x28},
	{0x583c, 0x42},
	{0x583d, 0xce},
	{0x5025, 0x00},
	{0x3a0f, 0x30},
	{0x3a10, 0x28},
	{0x3a1b, 0x30},
	{0x3a1e, 0x26},
	{0x3a11, 0x60},
	{0x3a1f, 0x14},
	{0x3008, 0x02},
};

static struct ov5640_regval setting_2592_1944_2lane_672m_30fps[] = {
	{0x3103, 0x11},
	{0x3008, 0x82},
	{0x3008, 0x42},
	{0x3103, 0x03},
	{0x3017, 0x00},
	{0x3018, 0x00},
	{0x3034, 0x18},
	{0x3035, 0x11},
	{0x3036, 0x54},
	{0x3037, 0x13},
	{0x3108, 0x01},
	{0x3630, 0x36},
	{0x3631, 0x0e},
	{0x3632, 0xe2},
	{0x3633, 0x12},
	{0x3621, 0xe0},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3905, 0x02},
	{0x3906, 0x10},
	{0x3901, 0x0a},
	{0x3731, 0x12},
	{0x3600, 0x08},
	{0x3601, 0x33},
	{0x302d, 0x60},
	{0x3620, 0x52},
	{0x371b, 0x20},
	{0x471c, 0x50},
	{0x3a13, 0x43},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3635, 0x13},
	{0x3636, 0x03},
	{0x3634, 0x40},
	{0x3622, 0x01},
	{0x3c01, 0x34},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c06, 0x00},
	{0x3c07, 0x07},
	{0x3c08, 0x00},
	{0x3c09, 0x1c},
	{0x3c0a, 0x9c},
	{0x3c0b, 0x40},
	{0x3820, 0x40},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0x9f},
	{0x3808, 0x0a},
	{0x3809, 0x20},
	{0x380a, 0x07},
	{0x380b, 0x98},
	{0x380c, 0x0b},
	{0x380d, 0x1c},
	{0x380e, 0x07},
	{0x380f, 0xb0},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3618, 0x04},
	{0x3612, 0x2b},
	{0x3708, 0x63},
	{0x3709, 0x12},
	{0x370c, 0x00},
	{0x3a02, 0x07},
	{0x3a03, 0xb0},
	{0x3a08, 0x01},
	{0x3a09, 0x27},
	{0x3a0a, 0x00},
	{0x3a0b, 0xf6},
	{0x3a0e, 0x06},
	{0x3a0d, 0x08},
	{0x3a14, 0x07},
	{0x3a15, 0xb0},
	{0x4001, 0x02},
	{0x4004, 0x06},
	{0x4050, 0x6e},
	{0x4051, 0x8f},
	{0x3000, 0x00},
	{0x3002, 0x1c},
	{0x3004, 0xff},
	{0x3006, 0xc3},
	{0x300e, 0x41},
	{0x302e, 0x08},
	{0x4300, 0x33},
	{0x4837, 0x0a},
	{0x501f, 0x00},
	{0x5684, 0x0a},
	{0x5685, 0x20},
	{0x5686, 0x07},
	{0x5687, 0x98},
	{0x440e, 0x00},
	{0x5000, 0xa7},
	{0x5001, 0x83},
	{0x5180, 0xff},
	{0x5181, 0xf2},
	{0x5182, 0x00},
	{0x5183, 0x14},
	{0x5184, 0x25},
	{0x5185, 0x24},
	{0x5186, 0x09},
	{0x5187, 0x09},
	{0x5188, 0x09},
	{0x5189, 0x75},
	{0x518a, 0x54},
	{0x518b, 0xe0},
	{0x518c, 0xb2},
	{0x518d, 0x42},
	{0x518e, 0x3d},
	{0x518f, 0x56},
	{0x5190, 0x46},
	{0x5191, 0xf8},
	{0x5192, 0x04},
	{0x5193, 0x70},
	{0x5194, 0xf0},
	{0x5195, 0xf0},
	{0x5196, 0x03},
	{0x5197, 0x01},
	{0x5198, 0x04},
	{0x5199, 0x12},
	{0x519a, 0x04},
	{0x519b, 0x00},
	{0x519c, 0x06},
	{0x519d, 0x82},
	{0x519e, 0x38},
	{0x5381, 0x1e},
	{0x5382, 0x5b},
	{0x5383, 0x08},
	{0x5384, 0x0a},
	{0x5385, 0x7e},
	{0x5386, 0x88},
	{0x5387, 0x7c},
	{0x5388, 0x6c},
	{0x5389, 0x10},
	{0x538a, 0x01},
	{0x538b, 0x98},
	{0x5300, 0x08},
	{0x5301, 0x30},
	{0x5302, 0x10},
	{0x5303, 0x00},
	{0x5304, 0x08},
	{0x5305, 0x30},
	{0x5306, 0x08},
	{0x5307, 0x16},
	{0x5309, 0x08},
	{0x530a, 0x30},
	{0x530b, 0x04},
	{0x530c, 0x06},
	{0x5480, 0x01},
	{0x5481, 0x08},
	{0x5482, 0x14},
	{0x5483, 0x28},
	{0x5484, 0x51},
	{0x5485, 0x65},
	{0x5486, 0x71},
	{0x5487, 0x7d},
	{0x5488, 0x87},
	{0x5489, 0x91},
	{0x548a, 0x9a},
	{0x548b, 0xaa},
	{0x548c, 0xb8},
	{0x548d, 0xcd},
	{0x548e, 0xdd},
	{0x548f, 0xea},
	{0x5490, 0x1d},
	{0x5580, 0x02},
	{0x5583, 0x40},
	{0x5584, 0x10},
	{0x5589, 0x10},
	{0x558a, 0x00},
	{0x558b, 0xf8},
	{0x5800, 0x23},
	{0x5801, 0x14},
	{0x5802, 0x0f},
	{0x5803, 0x0f},
	{0x5804, 0x12},
	{0x5805, 0x26},
	{0x5806, 0x0c},
	{0x5807, 0x08},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x08},
	{0x580b, 0x0d},
	{0x580c, 0x08},
	{0x580d, 0x03},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x09},
	{0x5812, 0x07},
	{0x5813, 0x03},
	{0x5814, 0x00},
	{0x5815, 0x01},
	{0x5816, 0x03},
	{0x5817, 0x08},
	{0x5818, 0x0d},
	{0x5819, 0x08},
	{0x581a, 0x05},
	{0x581b, 0x06},
	{0x581c, 0x08},
	{0x581d, 0x0e},
	{0x581e, 0x29},
	{0x581f, 0x17},
	{0x5820, 0x11},
	{0x5821, 0x11},
	{0x5822, 0x15},
	{0x5823, 0x28},
	{0x5824, 0x46},
	{0x5825, 0x26},
	{0x5826, 0x08},
	{0x5827, 0x26},
	{0x5828, 0x64},
	{0x5829, 0x26},
	{0x582a, 0x24},
	{0x582b, 0x22},
	{0x582c, 0x24},
	{0x582d, 0x24},
	{0x582e, 0x06},
	{0x582f, 0x22},
	{0x5830, 0x40},
	{0x5831, 0x42},
	{0x5832, 0x24},
	{0x5833, 0x26},
	{0x5834, 0x24},
	{0x5835, 0x22},
	{0x5836, 0x22},
	{0x5837, 0x26},
	{0x5838, 0x44},
	{0x5839, 0x24},
	{0x583a, 0x26},
	{0x583b, 0x28},
	{0x583c, 0x42},
	{0x583d, 0xce},
	{0x5025, 0x00},
	{0x3a0f, 0x30},
	{0x3a10, 0x28},
	{0x3a1b, 0x30},
	{0x3a1e, 0x26},
	{0x3a11, 0x60},
	{0x3a1f, 0x14},
	{0x3008, 0x02},
};

/* supported link frequencies */
#define FREQ_INDEX_1080P	0
#define FREQ_INDEX_5MP		1

/* supported link frequencies */
static const s64 ov5640_link_freq_2lanes[] = {
	[FREQ_INDEX_1080P] = 672000000,
	[FREQ_INDEX_5MP] = 672000000,
};

static inline const s64 *ov5640_link_freqs_ptr(const struct ov5640 *ov5640)
{
	return ov5640_link_freq_2lanes;
}

static inline int ov5640_link_freqs_num(const struct ov5640 *ov5640)
{
	return ARRAY_SIZE(ov5640_link_freq_2lanes);
}

/* Mode configs */
static const struct ov5640_mode ov5640_modes_2lanes[] = {
	{
		.width = 1920,
		.height = 1080,
		.hmax  = 0x1130,
		.data = setting_1920_1080_2lane_672m_30fps,
		.data_size = ARRAY_SIZE(setting_1920_1080_2lane_672m_30fps),

		.link_freq_index = FREQ_INDEX_1080P,
	},
	{
		.width = 2592,
		.height = 1944,
		.hmax = 0x19c8,
		.data = setting_2592_1944_2lane_672m_30fps,
		.data_size = ARRAY_SIZE(setting_2592_1944_2lane_672m_30fps),

		.link_freq_index = FREQ_INDEX_5MP,
	},
};

static inline const struct ov5640_mode *ov5640_modes_ptr(const struct ov5640 *ov5640)
{
	return ov5640_modes_2lanes;
}

static inline int ov5640_modes_num(const struct ov5640 *ov5640)
{
	return ARRAY_SIZE(ov5640_modes_2lanes);
}

static inline struct ov5640 *to_ov5640(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct ov5640, sd);
}

static inline int ov5640_read_reg(struct ov5640 *ov5640, u16 addr, u8 *value)
{
	unsigned int regval;

	int i, ret;

	for (i = 0; i < 3; ++i) {
		ret = regmap_read(ov5640->regmap, addr, &regval);
		if (0 == ret ) {
			break;
		}
	}

	if (ret)
		dev_err(ov5640->dev, "I2C read with i2c transfer failed for addr: %x, ret %d\n", addr, ret);

	*value = regval & 0xff;
	return 0;
}

static int ov5640_write_reg(struct ov5640 *ov5640, u16 addr, u8 value)
{
	int i, ret;

	for (i = 0; i < 3; i++) {
		ret = regmap_write(ov5640->regmap, addr, value);
		if (0 == ret) {
			break;
		}
	}

	if (ret)
		dev_err(ov5640->dev, "I2C write failed for addr: %x, ret %d\n", addr, ret);

	return ret;
}

static int ov5640_get_id(struct ov5640 *ov5640)
{
	int rtn = -EINVAL;
    uint32_t sensor_id = 0;
    u8 val = 0;

	ov5640_read_reg(ov5640, 0x300a, &val);
	sensor_id |= (val << 8);
	ov5640_read_reg(ov5640, 0x300b, &val);
	sensor_id |= val;

	if (sensor_id != OV5640_ID) {
		dev_err(ov5640->dev, "Failed to get ov5640 id: 0x%x\n", sensor_id);
		return rtn;
	} else {
		dev_err(ov5640->dev, "success get ov5640 id 0x%x", sensor_id);
	}

	return 0;
}

static int ov5640_set_register_array(struct ov5640 *ov5640,
				     const struct ov5640_regval *settings,
				     unsigned int num_settings)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < num_settings; ++i, ++settings) {
		ret = ov5640_write_reg(ov5640, settings->reg, settings->val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ov5640_write_buffered_reg(struct ov5640 *ov5640, u16 address_low,
				     u8 nr_regs, u32 value)
{
	unsigned int i;
	int ret;

	for (i = 0; i < nr_regs; i++) {
		ret = ov5640_write_reg(ov5640, address_low + i,
				       (u8)(value >> (i * 8)));
		if (ret) {
			dev_err(ov5640->dev, "Error writing buffered registers\n");
			return ret;
		}
	}

	return ret;
}

static int ov5640_set_gain(struct ov5640 *ov5640, u32 value)
{
	int ret;

	ret = ov5640_write_buffered_reg(ov5640, OV5640_GAIN, 1, value);
	if (ret)
		dev_err(ov5640->dev, "Unable to write gain\n");

	return ret;
}

static int ov5640_set_exposure(struct ov5640 *ov5640, u32 value)
{
	int ret;

	ret = ov5640_write_buffered_reg(ov5640, OV5640_EXPOSURE, 2, value);
	if (ret)
		dev_err(ov5640->dev, "Unable to write gain\n");

	return ret;
}


/* Stop streaming */
static int ov5640_stop_streaming(struct ov5640 *ov5640)
{
	ov5640->enWDRMode = WDR_MODE_NONE;
	return ov5640_write_reg(ov5640, 0x300e, 0x41);
}

static int ov5640_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov5640 *ov5640 = container_of(ctrl->handler,
					     struct ov5640, ctrls);
	int ret = 0;

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(ov5640->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		ret = ov5640_set_gain(ov5640, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = ov5640_set_exposure(ov5640, ctrl->val);
		break;
	case V4L2_CID_HBLANK:
		break;
	case V4L2_CID_AML_MODE:
		ov5640->enWDRMode = ctrl->val;
		break;
	default:
		dev_err(ov5640->dev, "Error ctrl->id %u, flag 0x%lx\n",
			ctrl->id, ctrl->flags);
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(ov5640->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ov5640_ctrl_ops = {
	.s_ctrl = ov5640_set_ctrl,
};
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov5640_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#else
static int ov5640_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	if (code->index >= ARRAY_SIZE(ov5640_formats))
		return -EINVAL;

	code->code = ov5640_formats[code->index].code;

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov5640_enum_frame_size(struct v4l2_subdev *sd,
			        struct v4l2_subdev_state *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#else
static int ov5640_enum_frame_size(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	if (fse->index >= ARRAY_SIZE(ov5640_formats))
		return -EINVAL;

	fse->min_width = ov5640_formats[fse->index].min_width;
	fse->min_height = ov5640_formats[fse->index].min_height;;
	fse->max_width = ov5640_formats[fse->index].max_width;
	fse->max_height = ov5640_formats[fse->index].max_height;

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov5640_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *cfg,
			  struct v4l2_subdev_format *fmt)
#else
static int ov5640_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
#endif
{
	struct ov5640 *ov5640 = to_ov5640(sd);
	struct v4l2_mbus_framefmt *framefmt;

	mutex_lock(&ov5640->lock);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		framefmt = v4l2_subdev_get_try_format(&ov5640->sd, cfg,
						      fmt->pad);
	else
		framefmt = &ov5640->current_format;

	fmt->format = *framefmt;

	mutex_unlock(&ov5640->lock);

	return 0;
}


static inline u8 ov5640_get_link_freq_index(struct ov5640 *ov5640)
{
	return ov5640->current_mode->link_freq_index;
}

static s64 ov5640_get_link_freq(struct ov5640 *ov5640)
{
	u8 index = ov5640_get_link_freq_index(ov5640);

	return *(ov5640_link_freqs_ptr(ov5640) + index);
}

static u64 ov5640_calc_pixel_rate(struct ov5640 *ov5640)
{
	s64 link_freq = ov5640_get_link_freq(ov5640);
	u8 nlanes = ov5640->nlanes;
	u64 pixel_rate;

	/* pixel rate = link_freq * 2 * nr_of_lanes / bits_per_sample */
	pixel_rate = link_freq * 2 * nlanes;
	do_div(pixel_rate, ov5640->bpp);
	return pixel_rate;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov5640_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *cfg,
			struct v4l2_subdev_format *fmt)
#else
static int ov5640_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct ov5640 *ov5640 = to_ov5640(sd);
	const struct ov5640_mode *mode;
	struct v4l2_mbus_framefmt *format;
	unsigned int i,ret;

	mutex_lock(&ov5640->lock);

	mode = v4l2_find_nearest_size(ov5640_modes_ptr(ov5640),
				 ov5640_modes_num(ov5640),
				width, height,
				fmt->format.width, fmt->format.height);

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;

	for (i = 0; i < ARRAY_SIZE(ov5640_formats); i++) {
		if (ov5640_formats[i].code == fmt->format.code) {
			dev_err(ov5640->dev, " zzw find proper format %d \n",i);
			break;
		}
	}

	if (i >= ARRAY_SIZE(ov5640_formats)) {
		i = 0;
		dev_err(ov5640->dev, " zzw No format. reset i = 0 \n");
	}

	fmt->format.code = ov5640_formats[i].code;
	fmt->format.field = V4L2_FIELD_NONE;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		dev_err(ov5640->dev, " zzw try format \n");
		format = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		mutex_unlock(&ov5640->lock);
		return 0;
	} else {
		dev_err(ov5640->dev, " zzw set format, w %d, h %d, code 0x%x \n",
            fmt->format.width, fmt->format.height,
            fmt->format.code);
		format = &ov5640->current_format;
		ov5640->current_mode = mode;
		ov5640->bpp = ov5640_formats[i].bpp;

		if (ov5640->link_freq)
			__v4l2_ctrl_s_ctrl(ov5640->link_freq, ov5640_get_link_freq_index(ov5640) );
		if (ov5640->pixel_rate)
			__v4l2_ctrl_s_ctrl_int64(ov5640->pixel_rate, ov5640_calc_pixel_rate(ov5640) );
	}

	*format = fmt->format;
	ov5640->status = 0;

	mutex_unlock(&ov5640->lock);

	/* Set init register settings */
	ret = ov5640_set_register_array(ov5640, setting_2592_1944_2lane_672m_30fps,
		ARRAY_SIZE(setting_2592_1944_2lane_672m_30fps));
	if (ret < 0) {
		dev_err(ov5640->dev, "Could not set init registers\n");
		return ret;
	} else
		dev_err(ov5640->dev, "ov5640 linear mode init...\n");

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
int ov5640_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *cfg,
			     struct v4l2_subdev_selection *sel)
#else
int ov5640_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_subdev_selection *sel)
#endif
{
	int rtn = 0;
	struct ov5640 *ov5640 = to_ov5640(sd);
	const struct ov5640_mode *mode = ov5640->current_mode;

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
		dev_err(ov5640->dev, "Error support target: 0x%x\n", sel->target);
	break;
	}

	return rtn;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov5640_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_state *cfg)
#else
static int ov5640_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg)
#endif
{
	struct v4l2_subdev_format fmt = { 0 };

	fmt.which = cfg ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.format.width = 1920;
	fmt.format.height = 1080;

	ov5640_set_fmt(subdev, cfg, &fmt);

	return 0;
}

/* Start streaming */
static int ov5640_start_streaming(struct ov5640 *ov5640)
{
	/* Start streaming */
	return ov5640_write_reg(ov5640, 0x300e, 0x45);
}

static int ov5640_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov5640 *ov5640 = to_ov5640(sd);
	int ret = 0;

	if (ov5640->status == enable)
		return ret;
	else
		ov5640->status = enable;

	if (enable) {
		ret = ov5640_start_streaming(ov5640);
		if (ret) {
			dev_err(ov5640->dev, "Start stream failed\n");
			goto unlock_and_return;
		}

		dev_info(ov5640->dev, "stream on\n");
	} else {
		ov5640_stop_streaming(ov5640);

		dev_info(ov5640->dev, "stream off\n");
	}

unlock_and_return:

	return ret;
}

static int ov5640_power_on(struct ov5640 *ov5640)
{
	int ret;

	reset_am_enable(ov5640->dev,"reset", 1);

	ret = mclk_enable(ov5640->dev,24000000);
	if (ret < 0 )
		dev_err(ov5640->dev, "set mclk fail\n");
	udelay(30);

	// 30ms
	usleep_range(30000, 31000);

	return 0;
}

static int ov5640_power_off(struct ov5640 *ov5640)
{
	mclk_disable(ov5640->dev);

	reset_am_enable(ov5640->dev,"reset", 0);

	return 0;
}

static int ov5640_power_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5640 *ov5640 = to_ov5640(sd);

	reset_am_enable(ov5640->dev,"reset", 0);

	return 0;
}

static int ov5640_power_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5640 *ov5640 = to_ov5640(sd);

	reset_am_enable(ov5640->dev,"reset", 1);

	return 0;
}

static int ov5640_log_status(struct v4l2_subdev *sd)
{
	struct ov5640 *ov5640 = to_ov5640(sd);

	dev_info(ov5640->dev, "log status done\n");

	return 0;
}

int ov5640_sbdev_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh) {
	struct ov5640 *ov5640 = to_ov5640(sd);
	ov5640_power_on(ov5640);
	return 0;
}

int ov5640_sbdev_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh) {
	struct ov5640 *ov5640 = to_ov5640(sd);
	ov5640_stop_streaming(ov5640);
	ov5640_power_off(ov5640);
	return 0;
}


static const struct dev_pm_ops ov5640_pm_ops = {
	SET_RUNTIME_PM_OPS(ov5640_power_suspend, ov5640_power_resume, NULL)
};

const struct v4l2_subdev_core_ops ov5640_core_ops = {
	.log_status = ov5640_log_status,
};

static const struct v4l2_subdev_video_ops ov5640_video_ops = {
	.s_stream = ov5640_set_stream,
};

static const struct v4l2_subdev_pad_ops ov5640_pad_ops = {
	.init_cfg = ov5640_entity_init_cfg,
	.enum_mbus_code = ov5640_enum_mbus_code,
	.enum_frame_size = ov5640_enum_frame_size,
	.get_selection = ov5640_get_selection,
	.get_fmt = ov5640_get_fmt,
	.set_fmt = ov5640_set_fmt,
};
static struct v4l2_subdev_internal_ops ov5640_internal_ops = {
	.open = ov5640_sbdev_open,
	.close = ov5640_sbdev_close,
};

static const struct v4l2_subdev_ops ov5640_subdev_ops = {
	.core = &ov5640_core_ops,
	.video = &ov5640_video_ops,
	.pad = &ov5640_pad_ops,
};

static const struct media_entity_operations ov5640_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static struct v4l2_ctrl_config wdr_cfg = {
	.ops = &ov5640_ctrl_ops,
	.id = V4L2_CID_AML_MODE,
	.name = "wdr mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 2,
	.step = 1,
	.def = 0,
};

static int ov5640_ctrls_init(struct ov5640 *ov5640)
{
	int rtn = 0;

	v4l2_ctrl_handler_init(&ov5640->ctrls, 4);

	v4l2_ctrl_new_std(&ov5640->ctrls, &ov5640_ctrl_ops,
				V4L2_CID_GAIN, 0, 0xF0, 1, 0);

	v4l2_ctrl_new_std(&ov5640->ctrls, &ov5640_ctrl_ops,
				V4L2_CID_EXPOSURE, 0, 0xffff, 1, 0);

	ov5640->link_freq = v4l2_ctrl_new_int_menu(&ov5640->ctrls,
					       &ov5640_ctrl_ops,
					       V4L2_CID_LINK_FREQ,
					       ov5640_link_freqs_num(ov5640) - 1,
					       0, ov5640_link_freqs_ptr(ov5640) );

	if (ov5640->link_freq)
		ov5640->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ov5640->pixel_rate = v4l2_ctrl_new_std(&ov5640->ctrls,
					       &ov5640_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       1, INT_MAX, 1,
					       ov5640_calc_pixel_rate(ov5640));

	ov5640->wdr = v4l2_ctrl_new_custom(&ov5640->ctrls, &wdr_cfg, NULL);

	ov5640->sd.ctrl_handler = &ov5640->ctrls;

	if (ov5640->ctrls.error) {
		dev_err(ov5640->dev, "Control initialization a error  %d\n",
			ov5640->ctrls.error);
		rtn = ov5640->ctrls.error;
	}

	return rtn;
}

static int ov5640_parse_power(struct ov5640 *ov5640)
{
	int rtn = 0;

	ov5640->rst_gpio = devm_gpiod_get_optional(ov5640->dev,
						"reset",
						GPIOD_OUT_LOW);
	if (IS_ERR(ov5640->rst_gpio)) {
		dev_err(ov5640->dev, "Cannot get reset gpio\n");
		rtn = PTR_ERR(ov5640->rst_gpio);
		goto err_return;
	}

	/*ov5640->pwdn_gpio = devm_gpiod_get_optional(ov5640->dev,
						"pwdn",
						GPIOD_OUT_LOW);
	if (IS_ERR(ov5640->pwdn_gpio)) {
		dev_err(ov5640->dev, "Cannot get pwdn gpio\n");
		rtn = PTR_ERR(ov5640->pwdn_gpio);
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
static s64 ov5640_check_link_freqs(const struct ov5640 *ov5640,
				   const struct v4l2_fwnode_endpoint *ep)
{
	int i, j;
	const s64 *freqs = ov5640_link_freqs_ptr(ov5640);
	int freqs_count = ov5640_link_freqs_num(ov5640);

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

static int ov5640_parse_endpoint(struct ov5640 *ov5640)
{
	int rtn = 0;
	s64 fq;
	struct fwnode_handle *endpoint = NULL;
	struct device_node *node = NULL;

	//endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(ov5640->dev), NULL);
	for_each_endpoint_of_node(ov5640->dev->of_node, node) {
		if (strstr(node->name, "ov5640")) {
			endpoint = of_fwnode_handle(node);
			break;
		}
	}

	rtn = v4l2_fwnode_endpoint_alloc_parse(endpoint, &ov5640->ep);
	fwnode_handle_put(endpoint);
	if (rtn) {
		dev_err(ov5640->dev, "Parsing endpoint node failed\n");
		rtn = -EINVAL;
		goto err_return;
	}

	/* Only CSI2 is supported for now */
	if (ov5640->ep.bus_type != V4L2_MBUS_CSI2_DPHY) {
		dev_err(ov5640->dev, "Unsupported bus type, should be CSI2\n");
		rtn = -EINVAL;
		goto err_free;
	}

	ov5640->nlanes = ov5640->ep.bus.mipi_csi2.num_data_lanes;
	if (ov5640->nlanes != 2 && ov5640->nlanes != 4) {
		dev_err(ov5640->dev, "Invalid data lanes: %d\n", ov5640->nlanes);
		rtn = -EINVAL;
		goto err_free;
	}
	dev_info(ov5640->dev, "Using %u data lanes\n", ov5640->nlanes);

	if (!ov5640->ep.nr_of_link_frequencies) {
		dev_err(ov5640->dev, "link-frequency property not found in DT\n");
		rtn = -EINVAL;
		goto err_free;
	}

	/* Check that link frequences for all the modes are in device tree */
	fq = ov5640_check_link_freqs(ov5640, &ov5640->ep);
	if (fq) {
		dev_err(ov5640->dev, "Link frequency of %lld is not supported\n", fq);
		rtn = -EINVAL;
		goto err_free;
	}

	return rtn;

err_free:
	v4l2_fwnode_endpoint_free(&ov5640->ep);
err_return:
	return rtn;
}


static int ov5640_register_subdev(struct ov5640 *ov5640)
{
	int rtn = 0;

	v4l2_i2c_subdev_init(&ov5640->sd, ov5640->client, &ov5640_subdev_ops);

	ov5640->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ov5640->sd.dev = &ov5640->client->dev;
	ov5640->sd.internal_ops = &ov5640_internal_ops;
	ov5640->sd.entity.ops = &ov5640_subdev_entity_ops;
	ov5640->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	snprintf(ov5640->sd.name, sizeof(ov5640->sd.name), AML_SENSOR_NAME, ov5640->index);

	ov5640->pad.flags = MEDIA_PAD_FL_SOURCE;
	rtn = media_entity_pads_init(&ov5640->sd.entity, 1, &ov5640->pad);
	if (rtn < 0) {
		dev_err(ov5640->dev, "Could not register media entity\n");
		goto err_return;
	}

	rtn = v4l2_async_register_subdev(&ov5640->sd);
	if (rtn < 0) {
		dev_err(ov5640->dev, "Could not register v4l2 device\n");
		goto err_return;
	}

err_return:
	return rtn;
}

static int ov5640_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ov5640 *ov5640;
	int ret = -EINVAL;


	ov5640 = devm_kzalloc(dev, sizeof(*ov5640), GFP_KERNEL);
	if (!ov5640)
		return -ENOMEM;
	dev_err(dev, "i2c dev addr 0x%x, name %s \n", client->addr, client->name);

	ov5640->dev = dev;
	ov5640->client = client;

	ov5640->regmap = devm_regmap_init_i2c(client, &ov5640_regmap_config);
	if (IS_ERR(ov5640->regmap)) {
		dev_err(dev, "Unable to initialize I2C\n");
		return -ENODEV;
	}

	if (of_property_read_u32(dev->of_node, "index", &ov5640->index)) {
		dev_err(dev, "Failed to read sensor index. default to 0\n");
		ov5640->index = 0;
	}


	ret = ov5640_parse_endpoint(ov5640);
	if (ret) {
		dev_err(ov5640->dev, "Error parse endpoint\n");
		goto return_err;
	}

	ret = ov5640_parse_power(ov5640);
	if (ret) {
		dev_err(ov5640->dev, "Error parse power ctrls\n");
		goto free_err;
	}

	mutex_init(&ov5640->lock);

	/* Power on the device to match runtime PM state below */
	dev_err(dev, "bef get id. pwdn -0, reset - 1\n");

	ret = ov5640_power_on(ov5640);
	if (ret < 0) {
		dev_err(dev, "Could not power on the device\n");
		goto free_err;
	}


	ret = ov5640_get_id(ov5640);
	if (ret) {
		dev_err(dev, "Could not get id\n");
		ov5640_power_off(ov5640);
		goto free_err;
	}

	/*
	 * Initialize the frame format. In particular, ov5640->current_mode
	 * and ov5640->bpp are set to defaults: ov5640_calc_pixel_rate() call
	 * below in ov5640_ctrls_init relies on these fields.
	 */
	ov5640_entity_init_cfg(&ov5640->sd, NULL);

	ret = ov5640_ctrls_init(ov5640);
	if (ret) {
		dev_err(ov5640->dev, "Error ctrls init\n");
		goto free_ctrl;
	}

	ret = ov5640_register_subdev(ov5640);
	if (ret) {
		dev_err(ov5640->dev, "Error register subdev\n");
		goto free_entity;
	}

	v4l2_fwnode_endpoint_free(&ov5640->ep);

	dev_info(ov5640->dev, "probe done \n");

	return 0;

free_entity:
	media_entity_cleanup(&ov5640->sd.entity);
free_ctrl:
	v4l2_ctrl_handler_free(&ov5640->ctrls);
	mutex_destroy(&ov5640->lock);
free_err:
	v4l2_fwnode_endpoint_free(&ov5640->ep);
return_err:
	return ret;
}

static int ov5640_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5640 *ov5640 = to_ov5640(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	mutex_destroy(&ov5640->lock);

	ov5640_power_off(ov5640);

	return 0;
}

static const struct of_device_id ov5640_of_match[] = {
	{ .compatible = "omini, ov5640" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ov5640_of_match);

static struct i2c_driver ov5640_i2c_driver = {
	.probe_new  = ov5640_probe,
	.remove = ov5640_remove,
	.driver = {
		.name  = "ov5640",
		.pm = &ov5640_pm_ops,
		.of_match_table = of_match_ptr(ov5640_of_match),
	},
};

module_i2c_driver(ov5640_i2c_driver);

MODULE_DESCRIPTION("OV5640 Image Sensor Driver");
MODULE_AUTHOR("open source");
MODULE_AUTHOR("@amlogic.com");
MODULE_LICENSE("GPL v2");
