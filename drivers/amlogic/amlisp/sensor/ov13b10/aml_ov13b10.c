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
#include <linux/of_gpio.h>

#include "../include/mclk_api.h"

#define OV13B10_GAIN       0x3508
#define OV13B10_GAIN_L       0x3509
#define OV13B10_EXPOSURE   0x3501
#define OV13B10_EXPOSURE_L   0x3502
#define OV13B10_ID         0x560d42
//0x530841

#define AML_SENSOR_NAME  "ov13b10-%u"

//#define HDR_30FPS_1440M
#define SDR_60FPS_1440M

struct ov13b10_regval {
	u16 reg;
	u8 val;
};

struct ov13b10_mode {
	u32 width;
	u32 height;
	u32 hmax;
	u32 link_freq_index;

	const struct ov13b10_regval *data;
	u32 data_size;
};

struct ov13b10 {
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
	const struct ov13b10_mode *current_mode;

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

struct ov13b10_pixfmt {
	u32 code;
	u32 min_width;
	u32 max_width;
	u32 min_height;
	u32 max_height;
	u8 bpp;
};

static const struct ov13b10_pixfmt ov13b10_formats[] = {
	{ MEDIA_BUS_FMT_SBGGR10_1X10, 720, 4208, 720, 3120, 10 },
	{ MEDIA_BUS_FMT_SBGGR12_1X12, 1280, 4208, 720, 3120, 12 },
};

static const struct regmap_config ov13b10_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

#ifdef SDR_60FPS_1440M
static struct ov13b10_regval setting_4208_3120_4lane_1080m_30fps[] = {
	{0x0103, 0x01},
	{0x0303, 0x01},
	{0x0305, 0x46},
	{0x0321, 0x00},
	{0x0323, 0x04},
	{0x0324, 0x01},
	{0x0325, 0x50},
	{0x0326, 0x81},
	{0x0327, 0x04},
	{0x3011, 0x7c},
	{0x3012, 0x07},
	{0x3013, 0x32},
	{0x3107, 0x23},
	{0x3501, 0x0c},
	{0x3502, 0x10},
	{0x3504, 0x08},
	{0x3508, 0x07},
	{0x3509, 0xc0},
	{0x3600, 0x16},
	{0x3601, 0x54},
	{0x3612, 0x4e},
	{0x3620, 0x00},
	{0x3621, 0x68},
	{0x3622, 0x66},
	{0x3623, 0x03},
	{0x3662, 0x92},
	{0x3666, 0xbb},
	{0x3667, 0x44},
	{0x366e, 0xff},
	{0x366f, 0xf3},
	{0x3675, 0x44},
	{0x3676, 0x00},
	{0x367f, 0xe9},
	{0x3681, 0x32},
	{0x3682, 0x1f},
	{0x3683, 0x0b},
	{0x3684, 0x0b},
	{0x3704, 0x0f},
	{0x3706, 0x40},
	{0x3708, 0x3b},
	{0x3709, 0x72},
	{0x370b, 0xa2},
	{0x3714, 0x24},
	{0x371a, 0x3e},
	{0x3725, 0x42},
	{0x3739, 0x12},
	{0x3767, 0x00},
	{0x377a, 0x0d},
	{0x3789, 0x18},
	{0x3790, 0x40},
	{0x3791, 0xa2},
	{0x37c2, 0x04},
	{0x37c3, 0xf1},
	{0x37d9, 0x0c},
	{0x37da, 0x02},
	{0x37dc, 0x02},
	{0x37e1, 0x04},
	{0x37e2, 0x0a},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x08},
	{0x3804, 0x10},
	{0x3805, 0x8f},
	{0x3806, 0x0c},
	{0x3807, 0x47},
	{0x3808, 0x10},
	{0x3809, 0x70},
	{0x380a, 0x0c},
	{0x380b, 0x30},
	{0x380c, 0x04},
	{0x380d, 0x98},
	{0x380e, 0x0c},
	{0x380f, 0xc0},
	{0x3811, 0x0f},
	{0x3813, 0x08},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x381f, 0x08},
	{0x3820, 0x88},
	{0x3821, 0x00},
	{0x3822, 0x14},
	{0x3823, 0x18},
	{0x3827, 0x01},
	{0x382e, 0xe6},
	{0x3c80, 0x00},
	{0x3c87, 0x01},
	{0x3c8c, 0x19},
	{0x3c8d, 0x1c},
	{0x3ca0, 0x00},
	{0x3ca1, 0x00},
	{0x3ca2, 0x00},
	{0x3ca3, 0x00},
	{0x3ca4, 0x50},
	{0x3ca5, 0x11},
	{0x3ca6, 0x01},
	{0x3ca7, 0x00},
	{0x3ca8, 0x00},
	{0x4008, 0x02},
	{0x4009, 0x0f},
	{0x400a, 0x01},
	{0x400b, 0x19},
	{0x4011, 0x21},
	{0x4017, 0x08},
	{0x4019, 0x04},
	{0x401a, 0x58},
	{0x4032, 0x1e},
	{0x4050, 0x02},
	{0x4051, 0x09},
	{0x405e, 0x00},
	{0x4066, 0x02},
	{0x4501, 0x00},
	{0x4502, 0x10},
	{0x4505, 0x00},
	{0x4800, 0x64},
	{0x481b, 0x3e},
	{0x481f, 0x30},
	{0x4825, 0x34},
	{0x4837, 0x0e},
	{0x484b, 0x01},
	{0x4883, 0x02},
	{0x5000, 0xfd},
	{0x5001, 0x0d},
	{0x5045, 0x20},
	{0x5046, 0x20},
	{0x5047, 0xa4},
	{0x5048, 0x20},
	{0x5049, 0xa4},

	{0x0100, 0x00},
	{0x0305, 0x46},
	{0x0325, 0x50},
	{0x0327, 0x05},
	{0x3501, 0x0c},
	{0x3502, 0x10},
	{0x3621, 0x28},
	{0x3622, 0xe6},
	{0x3623, 0x00},
	{0x3662, 0x92},
	{0x3714, 0x24},
	{0x371a, 0x3e},
	{0x3739, 0x12},
	{0x37c2, 0x04},
	{0x37c5, 0x00},
	{0x37d9, 0x0c},
	{0x37e2, 0x0a},
	{0x37e4, 0x04},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x08},
	{0x3804, 0x10},
	{0x3805, 0x8f},
	{0x3806, 0x0c},
	{0x3807, 0x47},
	{0x3808, 0x10},
	{0x3809, 0x70},
	{0x380a, 0x0c},
	{0x380b, 0x30},
	{0x380c, 0x04},
	{0x380d, 0x98},
	{0x380e, 0x0c},
	{0x380f, 0xc0},
	{0x3811, 0x0f},
	{0x3813, 0x08},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x01},
	{0x3817, 0x01},
	{0x3820, 0x88},
	{0x3c8c, 0x19},
	{0x3f02, 0x2a},
	{0x3f03, 0x10},
	{0x4008, 0x02},
	{0x4009, 0x0f},
	{0x4050, 0x02},
	{0x4051, 0x09},
	{0x4500, 0x0a},
	{0x4501, 0x00},
	{0x4505, 0x00},
	{0x4837, 0x0e},
	{0x5000, 0xfd},
	{0x5001, 0x0d},
	{0x0100, 0x01},
};
#endif


static const struct ov13b10_regval ov13b10_1080p_settings[] = {

};

static const struct ov13b10_regval ov13b10_720p_settings[] = {

};

static const struct ov13b10_regval ov13b10_10bit_settings[] = {

};


static const struct ov13b10_regval ov13b10_12bit_settings[] = {

};

/* supported link frequencies */
#define FREQ_INDEX_1080P	0
#define FREQ_INDEX_720P		1

/* supported link frequencies */

#if defined(HDR_30FPS_1440M) || defined(SDR_60FPS_1440M)
static const s64 ov13b10_link_freq_2lanes[] = {
	[FREQ_INDEX_1080P] = 1080000000,
	[FREQ_INDEX_720P] = 1080000000,
};

static const s64 ov13b10_link_freq_4lanes[] = {
	[FREQ_INDEX_1080P] = 1080000000,
	[FREQ_INDEX_720P] = 1080000000,
};

#else
static const s64 ov13b10_link_freq_2lanes[] = {
	[FREQ_INDEX_1080P] = 848000000,
	[FREQ_INDEX_720P] = 848000000,
};

static const s64 ov13b10_link_freq_4lanes[] = {
	[FREQ_INDEX_1080P] = 848000000,
	[FREQ_INDEX_720P] = 848000000,
};
#endif

static inline const s64 *ov13b10_link_freqs_ptr(const struct ov13b10 *ov13b10)
{
	if (ov13b10->nlanes == 2)
		return ov13b10_link_freq_2lanes;
	else {
		return ov13b10_link_freq_4lanes;
	}
}

static inline int ov13b10_link_freqs_num(const struct ov13b10 *ov13b10)
{
	if (ov13b10->nlanes == 2)
		return ARRAY_SIZE(ov13b10_link_freq_2lanes);
	else
		return ARRAY_SIZE(ov13b10_link_freq_4lanes);
}

/* Mode configs */
static const struct ov13b10_mode ov13b10_modes_2lanes[] = {
	{
		.width = 1920,
		.height = 1080,
		.hmax  = 0x1130,
		.data = ov13b10_1080p_settings,
		.data_size = ARRAY_SIZE(ov13b10_1080p_settings),

		.link_freq_index = FREQ_INDEX_1080P,
	},
	{
		.width = 1280,
		.height = 720,
		.hmax = 0x19c8,
		.data = ov13b10_720p_settings,
		.data_size = ARRAY_SIZE(ov13b10_720p_settings),

		.link_freq_index = FREQ_INDEX_720P,
	},
};


static const struct ov13b10_mode ov13b10_modes_4lanes[] = {
	{
		.width = 4208,
		.height = 3120,
		.hmax = 0x0898,
		.link_freq_index = FREQ_INDEX_1080P,
		.data = ov13b10_1080p_settings,
		.data_size = ARRAY_SIZE(ov13b10_1080p_settings),
	},
	{
		.width = 4208,
		.height = 3120,
		.hmax = 0x0ce4,
		.link_freq_index = FREQ_INDEX_720P,
		.data = ov13b10_720p_settings,
		.data_size = ARRAY_SIZE(ov13b10_720p_settings),
	},
};

static inline const struct ov13b10_mode *ov13b10_modes_ptr(const struct ov13b10 *ov13b10)
{
	if (ov13b10->nlanes == 2)
		return ov13b10_modes_2lanes;
	else
		return ov13b10_modes_4lanes;
}

static inline int ov13b10_modes_num(const struct ov13b10 *ov13b10)
{
	if (ov13b10->nlanes == 2)
		return ARRAY_SIZE(ov13b10_modes_2lanes);
	else
		return ARRAY_SIZE(ov13b10_modes_4lanes);
}

static inline struct ov13b10 *to_ov13b10(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct ov13b10, sd);
}

static inline int ov13b10_read_reg(struct ov13b10 *ov13b10, u16 addr, u8 *value)
{
	unsigned int regval;

	int i, ret;

	for (i = 0; i < 3; ++i) {
		ret = regmap_read(ov13b10->regmap, addr, &regval);
		if (0 == ret ) {
			break;
		}
	}

	if (ret)
		dev_err(ov13b10->dev, "I2C read with i2c transfer failed for addr: %x, ret %d\n", addr, ret);

	*value = regval & 0xff;
	return 0;
}

static int ov13b10_write_reg(struct ov13b10 *ov13b10, u16 addr, u8 value)
{
	int i, ret;

	for (i = 0; i < 3; i++) {
		ret = regmap_write(ov13b10->regmap, addr, value);
		if (0 == ret) {
			break;
		}
	}

	if (ret)
		dev_err(ov13b10->dev, "I2C write failed for addr: %x, ret %d\n", addr, ret);

	return ret;
}

static int ov13b10_get_id(struct ov13b10 *ov13b10)
{
	int rtn = -EINVAL;
    uint32_t sensor_id = 0;
    u8 val = 0;

	ov13b10_read_reg(ov13b10, 0x300a, &val);
	sensor_id |= (val << 16);
	ov13b10_read_reg(ov13b10, 0x300b, &val);
	sensor_id |= (val << 8);
    ov13b10_read_reg(ov13b10, 0x300c, &val);
	sensor_id |= val;

	if (sensor_id != OV13B10_ID) {
		dev_err(ov13b10->dev, "Failed to get ov13b10 id: 0x%x\n", sensor_id);
		return rtn;
	} else {
		dev_err(ov13b10->dev, "success get ov13b10 id 0x%x", sensor_id);
	}

	return 0;
}

static int ov13b10_set_register_array(struct ov13b10 *ov13b10,
				     const struct ov13b10_regval *settings,
				     unsigned int num_settings)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < num_settings; ++i, ++settings) {
		ret = ov13b10_write_reg(ov13b10, settings->reg, settings->val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
static int ov13b10_write_buffered_reg(struct ov13b10 *ov13b10, u16 address_low,
				     u8 nr_regs, u32 value)
{
	unsigned int i;
	int ret;

	for (i = 0; i < nr_regs; i++) {
		ret = ov13b10_write_reg(ov13b10, address_low + i,
				       (u8)(value >> ((1-i) * 8)));
		if (ret) {
			dev_err(ov13b10->dev, "Error writing buffered registers\n");
			return ret;
		}
	}

	return ret;
}
*/

static int ov13b10_set_gain(struct ov13b10 *ov13b10, u32 value)
{
	int ret = 0;
	u8 value_H = 0;
	u8 value_L = 0;
	//value = 256;
	dev_err(ov13b10->dev, "ov13b10_set_gain = 0x%x \n", value);
	ret = ov13b10_write_reg(ov13b10, OV13B10_GAIN, (value >> 8) & 0xFF);
	if (ret)
		dev_err(ov13b10->dev, "Unable to write OV13B10_GAIN_H \n");
	ret = ov13b10_write_reg(ov13b10, OV13B10_GAIN_L, value & 0xFF);
	if (ret)
		dev_err(ov13b10->dev, "Unable to write OV13B10_GAIN_L \n");

	ov13b10_read_reg(ov13b10, OV13B10_GAIN, &value_H);
	ov13b10_read_reg(ov13b10, OV13B10_GAIN_L, &value_L);
	dev_err(ov13b10->dev, "ov13b10 read gain = 0x%x \n", (value_H << 8) | value_L);
	return ret;
}

static int ov13b10_set_exposure(struct ov13b10 *ov13b10, u32 value)
{
	int ret;
	u8 value_H = 0;
	u8 value_L = 0;
	dev_err(ov13b10->dev, "ov13b10_set_exposure = 0x%x \n", value);
	ret = ov13b10_write_reg(ov13b10, OV13B10_EXPOSURE, (value >> 8) & 0xFF);
	if (ret)
		dev_err(ov13b10->dev, "Unable to write gain_H\n");
	ret = ov13b10_write_reg(ov13b10, OV13B10_EXPOSURE_L, value & 0xFF);
	if (ret)
		dev_err(ov13b10->dev, "Unable to write gain_L\n");

	ov13b10_read_reg(ov13b10, OV13B10_EXPOSURE, &value_H);
	ov13b10_read_reg(ov13b10, OV13B10_EXPOSURE_L, &value_L);
	dev_err(ov13b10->dev, "ov13b10 read exposure = 0x%x \n", (value_H << 8) | value_L);
	return ret;
}


/* Stop streaming */
static int ov13b10_stop_streaming(struct ov13b10 *ov13b10)
{
	ov13b10->enWDRMode = WDR_MODE_NONE;
	return ov13b10_write_reg(ov13b10, 0x0100, 0x00);
}

static int ov13b10_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov13b10 *ov13b10 = container_of(ctrl->handler,
					     struct ov13b10, ctrls);
	int ret = 0;

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(ov13b10->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		ret = ov13b10_set_gain(ov13b10, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = ov13b10_set_exposure(ov13b10, ctrl->val);
		break;
	case V4L2_CID_HBLANK:
		break;
	case V4L2_CID_AML_MODE:
		ov13b10->enWDRMode = ctrl->val;
		break;
	default:
		dev_err(ov13b10->dev, "Error ctrl->id %u, flag 0x%lx\n",
			ctrl->id, ctrl->flags);
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(ov13b10->dev);

	return ret;
}

static const struct v4l2_ctrl_ops ov13b10_ctrl_ops = {
	.s_ctrl = ov13b10_set_ctrl,
};
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov13b10_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#else
static int ov13b10_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	if (code->index >= ARRAY_SIZE(ov13b10_formats))
		return -EINVAL;

	code->code = ov13b10_formats[code->index].code;

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov13b10_enum_frame_size(struct v4l2_subdev *sd,
			        struct v4l2_subdev_state *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#else
static int ov13b10_enum_frame_size(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	if (fse->index >= ARRAY_SIZE(ov13b10_formats))
		return -EINVAL;

	fse->min_width = ov13b10_formats[fse->index].min_width;
	fse->min_height = ov13b10_formats[fse->index].min_height;;
	fse->max_width = ov13b10_formats[fse->index].max_width;
	fse->max_height = ov13b10_formats[fse->index].max_height;

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov13b10_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *cfg,
			  struct v4l2_subdev_format *fmt)
#else
static int ov13b10_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
#endif
{
	struct ov13b10 *ov13b10 = to_ov13b10(sd);
	struct v4l2_mbus_framefmt *framefmt;

	mutex_lock(&ov13b10->lock);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		framefmt = v4l2_subdev_get_try_format(&ov13b10->sd, cfg,
						      fmt->pad);
	else
		framefmt = &ov13b10->current_format;

	fmt->format = *framefmt;

	mutex_unlock(&ov13b10->lock);

	return 0;
}


static inline u8 ov13b10_get_link_freq_index(struct ov13b10 *ov13b10)
{
	return ov13b10->current_mode->link_freq_index;
}

static s64 ov13b10_get_link_freq(struct ov13b10 *ov13b10)
{
	u8 index = ov13b10_get_link_freq_index(ov13b10);

	return *(ov13b10_link_freqs_ptr(ov13b10) + index);
}

static u64 ov13b10_calc_pixel_rate(struct ov13b10 *ov13b10)
{
	s64 link_freq = ov13b10_get_link_freq(ov13b10);
	u8 nlanes = ov13b10->nlanes;
	u64 pixel_rate;

	/* pixel rate = link_freq * 2 * nr_of_lanes / bits_per_sample */
	pixel_rate = link_freq * 2 * nlanes;
	do_div(pixel_rate, ov13b10->bpp);
	return pixel_rate;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov13b10_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *cfg,
			struct v4l2_subdev_format *fmt)
#else
static int ov13b10_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct ov13b10 *ov13b10 = to_ov13b10(sd);
	const struct ov13b10_mode *mode;
	struct v4l2_mbus_framefmt *format;
	unsigned int i,ret;

	mutex_lock(&ov13b10->lock);

	mode = v4l2_find_nearest_size(ov13b10_modes_ptr(ov13b10),
				 ov13b10_modes_num(ov13b10),
				width, height,
				fmt->format.width, fmt->format.height);

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;

	for (i = 0; i < ARRAY_SIZE(ov13b10_formats); i++) {
		if (ov13b10_formats[i].code == fmt->format.code) {
			dev_err(ov13b10->dev, " zzw find proper format %d \n",i);
			break;
		}
	}

	if (i >= ARRAY_SIZE(ov13b10_formats)) {
		i = 0;
		dev_err(ov13b10->dev, " zzw No format. reset i = 0 \n");
	}

	fmt->format.code = ov13b10_formats[i].code;
	fmt->format.field = V4L2_FIELD_NONE;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		dev_err(ov13b10->dev, " zzw try format \n");
		format = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		mutex_unlock(&ov13b10->lock);
		return 0;
	} else {
		dev_err(ov13b10->dev, " zzw set format, w %d, h %d, code 0x%x \n",
            fmt->format.width, fmt->format.height,
            fmt->format.code);
		format = &ov13b10->current_format;
		ov13b10->current_mode = mode;
		ov13b10->bpp = ov13b10_formats[i].bpp;
		/*__v4l2_ctrl_s_ctrl(struct v4l2_ctrl *ctrl, s32 val)
			ov13b10_get_link_freq_index(ov13b10):0 | 1
		*/
		if (ov13b10->link_freq)
			__v4l2_ctrl_s_ctrl(ov13b10->link_freq, ov13b10_get_link_freq_index(ov13b10) );
		if (ov13b10->pixel_rate)
			__v4l2_ctrl_s_ctrl_int64(ov13b10->pixel_rate, ov13b10_calc_pixel_rate(ov13b10) );
	}

	*format = fmt->format;
	ov13b10->status = 0;

	mutex_unlock(&ov13b10->lock);

	if (ov13b10->enWDRMode) {
		/* Set init register settings */
		ret = ov13b10_set_register_array(ov13b10, setting_4208_3120_4lane_1080m_30fps,
			ARRAY_SIZE(setting_4208_3120_4lane_1080m_30fps));
		dev_err(ov13b10->dev, "%s:%d ***************WDR 1440M******\n",__FUNCTION__,__LINE__);
		if (ret < 0) {
			dev_err(ov13b10->dev, "Could not set init registers\n");
			return ret;
		} else
			dev_err(ov13b10->dev, "ov13b10 wdr mode init...\n");
	} else {
		/* Set init register settings */
		ret = ov13b10_set_register_array(ov13b10, setting_4208_3120_4lane_1080m_30fps,
			ARRAY_SIZE(setting_4208_3120_4lane_1080m_30fps));
		dev_err(ov13b10->dev, "%s:%d ***************1440M SDR******\n",__FUNCTION__,__LINE__);
		if (ret < 0) {
			dev_err(ov13b10->dev, "Could not set init registers\n");
			return ret;
		} else
			dev_err(ov13b10->dev, "ov13b10 linear mode init...\n");
	}

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
int ov13b10_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *cfg,
			     struct v4l2_subdev_selection *sel)
#else
int ov13b10_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_subdev_selection *sel)
#endif
{
	int rtn = 0;
	struct ov13b10 *ov13b10 = to_ov13b10(sd);
	const struct ov13b10_mode *mode = ov13b10->current_mode;

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
		dev_err(ov13b10->dev, "Error support target: 0x%x\n", sel->target);
	break;
	}

	return rtn;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov13b10_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_state *cfg)
#else
static int ov13b10_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg)
#endif
{
	struct v4l2_subdev_format fmt = { 0 };

	fmt.which = cfg ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.format.width = 1920;
	fmt.format.height = 1080;

	ov13b10_set_fmt(subdev, cfg, &fmt);

	return 0;
}

/* Start streaming */
static int ov13b10_start_streaming(struct ov13b10 *ov13b10)
{

	/* Start streaming */
	return ov13b10_write_reg(ov13b10, 0x0100, 0x01);
}

static int ov13b10_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov13b10 *ov13b10 = to_ov13b10(sd);
	int ret = 0;

	if (ov13b10->status == enable)
		return ret;
	else
		ov13b10->status = enable;

	if (enable) {
		ret = ov13b10_start_streaming(ov13b10);
		if (ret) {
			dev_err(ov13b10->dev, "Start stream failed\n");
			goto unlock_and_return;
		}

		dev_info(ov13b10->dev, "stream on\n");
	} else {
		ov13b10_stop_streaming(ov13b10);

		dev_info(ov13b10->dev, "stream off\n");
	}

unlock_and_return:

	return ret;
}

/*
int reset_am_enable(struct device *dev, const char* propname, int val)
{
	int ret = -1;

	int reset = of_get_named_gpio(dev->of_node, propname, 0);
	ret = reset;

	if (ret >= 0) {
		devm_gpio_request(dev, reset, "RESET");
		if (gpio_is_valid(reset)) {
			gpio_direction_output(reset, val);
			pr_info("reset init\n");
		} else {
			pr_err("reset_enable: gpio %s is not valid\n", propname);
			return -1;
		}
	} else {
		pr_err("reset_enable: get_named_gpio %s fail\n", propname);
	}

	return ret;
}
*/

static int ov13b10_power_on(struct ov13b10 *ov13b10)
{
	int ret;
	reset_am_enable(ov13b10->dev,"reset", 1);

	ret = mclk_enable(ov13b10->dev,24000000);
	if (ret < 0 )
		dev_err(ov13b10->dev, "set mclk fail\n");
	udelay(30);

	// 30ms
	usleep_range(30000, 31000);

	return 0;
}

static int ov13b10_power_off(struct ov13b10 *ov13b10)
{
	mclk_disable(ov13b10->dev);

	gpiod_set_value_cansleep(ov13b10->rst_gpio,1);
	//gpiod_set_value_cansleep(ov13b10->pwdn_gpio, 1);

	return 0;
}

static int ov13b10_power_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov13b10 *ov13b10 = to_ov13b10(sd);

	gpiod_set_value_cansleep(ov13b10->rst_gpio, 1);
	//gpiod_set_value_cansleep(ov13b10->pwdn_gpio, 1);

	return 0;
}

static int ov13b10_power_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov13b10 *ov13b10 = to_ov13b10(sd);

	gpiod_set_value_cansleep(ov13b10->rst_gpio, 1);
	//gpiod_set_value_cansleep(ov13b10->pwdn_gpio, 1);

	return 0;
}

static int ov13b10_log_status(struct v4l2_subdev *sd)
{
	struct ov13b10 *ov13b10 = to_ov13b10(sd);

	dev_info(ov13b10->dev, "log status done\n");

	return 0;
}

static const struct dev_pm_ops ov13b10_pm_ops = {
	SET_RUNTIME_PM_OPS(ov13b10_power_suspend, ov13b10_power_resume, NULL)
};

const struct v4l2_subdev_core_ops ov13b10_core_ops = {
	.log_status = ov13b10_log_status,
};

static const struct v4l2_subdev_video_ops ov13b10_video_ops = {
	.s_stream = ov13b10_set_stream,
};

static const struct v4l2_subdev_pad_ops ov13b10_pad_ops = {
	.init_cfg = ov13b10_entity_init_cfg,
	.enum_mbus_code = ov13b10_enum_mbus_code,
	.enum_frame_size = ov13b10_enum_frame_size,
	.get_selection = ov13b10_get_selection,
	.get_fmt = ov13b10_get_fmt,
	.set_fmt = ov13b10_set_fmt,
};

static const struct v4l2_subdev_ops ov13b10_subdev_ops = {
	.core = &ov13b10_core_ops,
	.video = &ov13b10_video_ops,
	.pad = &ov13b10_pad_ops,
};

static const struct media_entity_operations ov13b10_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static struct v4l2_ctrl_config wdr_cfg = {
	.ops = &ov13b10_ctrl_ops,
	.id = V4L2_CID_AML_MODE,
	.name = "wdr mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 2,
	.step = 1,
	.def = 0,
};

static int ov13b10_ctrls_init(struct ov13b10 *ov13b10)
{
	int rtn = 0;

	v4l2_ctrl_handler_init(&ov13b10->ctrls, 4);

	v4l2_ctrl_new_std(&ov13b10->ctrls, &ov13b10_ctrl_ops,
				V4L2_CID_GAIN, 0, 0xFFFF, 1, 0);

	v4l2_ctrl_new_std(&ov13b10->ctrls, &ov13b10_ctrl_ops,
				V4L2_CID_EXPOSURE, 0, 0xffff, 1, 0);

	ov13b10->link_freq = v4l2_ctrl_new_int_menu(&ov13b10->ctrls,
					       &ov13b10_ctrl_ops,
					       V4L2_CID_LINK_FREQ,
					       ov13b10_link_freqs_num(ov13b10) - 1,
					       0, ov13b10_link_freqs_ptr(ov13b10) );

	if (ov13b10->link_freq)
		ov13b10->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ov13b10->pixel_rate = v4l2_ctrl_new_std(&ov13b10->ctrls,
					       &ov13b10_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       1, INT_MAX, 1,
					       ov13b10_calc_pixel_rate(ov13b10));

	ov13b10->wdr = v4l2_ctrl_new_custom(&ov13b10->ctrls, &wdr_cfg, NULL);

	ov13b10->sd.ctrl_handler = &ov13b10->ctrls;

	if (ov13b10->ctrls.error) {
		dev_err(ov13b10->dev, "Control initialization a error  %d\n",
			ov13b10->ctrls.error);
		rtn = ov13b10->ctrls.error;
	}

	return rtn;
}

static int ov13b10_parse_power(struct ov13b10 *ov13b10)
{
	//int rtn = 0;
	int reset_pin = -1;
	int pwdn_pin = -1;

	reset_pin = of_get_named_gpio(ov13b10->dev->of_node, "reset", 0);
	if (reset_pin < 0) {
		pr_err("%s fail to get reset pin from dts!\n", __func__);
	} else {
		pr_info("%s reset_pin = %d!\n", __func__, reset_pin);
		gpio_direction_output(reset_pin, 1);
	}

	pwdn_pin = of_get_named_gpio(ov13b10->dev->of_node, "pwdn", 0);
	if (pwdn_pin < 0) {
		pr_err("%s fail to get pwdn pin from dts!\n", __func__);
	} else {
		pr_info("%s pwdn_pin = %d!\n", __func__, pwdn_pin);
		gpio_direction_output(pwdn_pin, 1);
	}

	return 0;

}

/*
 * Returns 0 if all link frequencies used by the driver for the given number
 * of MIPI data lanes are mentioned in the device tree, or the value of the
 * first missing frequency otherwise.
 */
static s64 ov13b10_check_link_freqs(const struct ov13b10 *ov13b10,
				   const struct v4l2_fwnode_endpoint *ep)
{
	int i, j;
	const s64 *freqs = ov13b10_link_freqs_ptr(ov13b10);
	int freqs_count = ov13b10_link_freqs_num(ov13b10);

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

static int ov13b10_parse_endpoint(struct ov13b10 *ov13b10)
{
	int rtn = 0;
	s64 fq;
	struct fwnode_handle *endpoint = NULL;
	struct device_node *node = NULL;

	//endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(ov13b10->dev), NULL);
	for_each_endpoint_of_node(ov13b10->dev->of_node, node) {
		if (strstr(node->name, "ov13b10")) {
			endpoint = of_fwnode_handle(node);
			break;
		}
	}

	rtn = v4l2_fwnode_endpoint_alloc_parse(endpoint, &ov13b10->ep);
	fwnode_handle_put(endpoint);
	if (rtn) {
		dev_err(ov13b10->dev, "Parsing endpoint node failed\n");
		rtn = -EINVAL;
		goto err_return;
	}

	/* Only CSI2 is supported for now */
	if (ov13b10->ep.bus_type != V4L2_MBUS_CSI2_DPHY) {
		dev_err(ov13b10->dev, "Unsupported bus type, should be CSI2\n");
		rtn = -EINVAL;
		goto err_free;
	}

	ov13b10->nlanes = ov13b10->ep.bus.mipi_csi2.num_data_lanes;
	if (ov13b10->nlanes != 2 && ov13b10->nlanes != 4) {
		dev_err(ov13b10->dev, "Invalid data lanes: %d\n", ov13b10->nlanes);
		rtn = -EINVAL;
		goto err_free;
	}
	dev_info(ov13b10->dev, "Using %u data lanes\n", ov13b10->nlanes);

	if (!ov13b10->ep.nr_of_link_frequencies) {
		dev_err(ov13b10->dev, "link-frequency property not found in DT\n");
		rtn = -EINVAL;
		goto err_free;
	}

	/* Check that link frequences for all the modes are in device tree */
	fq = ov13b10_check_link_freqs(ov13b10, &ov13b10->ep);
	if (fq) {
		dev_err(ov13b10->dev, "Link frequency of %lld is not supported\n", fq);
		rtn = -EINVAL;
		goto err_free;
	}

	return rtn;

err_free:
	v4l2_fwnode_endpoint_free(&ov13b10->ep);
err_return:
	return rtn;
}


static int ov13b10_register_subdev(struct ov13b10 *ov13b10)
{
	int rtn = 0;

	v4l2_i2c_subdev_init(&ov13b10->sd, ov13b10->client, &ov13b10_subdev_ops);

	ov13b10->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ov13b10->sd.dev = &ov13b10->client->dev;
	ov13b10->sd.entity.ops = &ov13b10_subdev_entity_ops;
	ov13b10->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	snprintf(ov13b10->sd.name, sizeof(ov13b10->sd.name), AML_SENSOR_NAME, ov13b10->index);

	ov13b10->pad.flags = MEDIA_PAD_FL_SOURCE;
	rtn = media_entity_pads_init(&ov13b10->sd.entity, 1, &ov13b10->pad);
	if (rtn < 0) {
		dev_err(ov13b10->dev, "Could not register media entity\n");
		goto err_return;
	}

	rtn = v4l2_async_register_subdev(&ov13b10->sd);
	if (rtn < 0) {
		dev_err(ov13b10->dev, "Could not register v4l2 device\n");
		goto err_return;
	}

err_return:
	return rtn;
}

static int ov13b10_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ov13b10 *ov13b10;
	int ret = -EINVAL;


	ov13b10 = devm_kzalloc(dev, sizeof(*ov13b10), GFP_KERNEL);
	if (!ov13b10)
		return -ENOMEM;
	dev_err(dev, "i2c dev addr 0x%x, name %s \n", client->addr, client->name);

	ov13b10->dev = dev;
	ov13b10->client = client;

	ov13b10->regmap = devm_regmap_init_i2c(client, &ov13b10_regmap_config);
	if (IS_ERR(ov13b10->regmap)) {
		dev_err(dev, "Unable to initialize I2C\n");
		return -ENODEV;
	}

	/*index = 1*/
	if (of_property_read_u32(dev->of_node, "index", &ov13b10->index)) {
		dev_err(dev, "Failed to read sensor index. default to 0\n");
		ov13b10->index = 0;
	}

	ret = ov13b10_parse_endpoint(ov13b10);
	if (ret) {
		dev_err(ov13b10->dev, "Error parse endpoint\n");
		goto return_err;
	}

	ret = ov13b10_parse_power(ov13b10);
	if (ret) {
		dev_err(ov13b10->dev, "Error parse power ctrls\n");
		goto free_err;
	}

	mutex_init(&ov13b10->lock);

	/* Power on the device to match runtime PM state below */
	dev_err(dev, "bef get id. pwdn -0, reset - 1\n");

	ret = ov13b10_power_on(ov13b10);
	if (ret < 0) {
		dev_err(dev, "Could not power on the device\n");
		goto free_err;
	}


	ret = ov13b10_get_id(ov13b10);
	if (ret) {
		dev_err(dev, "Could not get id\n");
		ov13b10_power_off(ov13b10);
		goto free_err;
	}

	/*
	 * Initialize the frame format. In particular, ov13b10->current_mode
	 * and ov13b10->bpp are set to defaults: ov13b10_calc_pixel_rate() call
	 * below in ov13b10_ctrls_init relies on these fields.
	 */
	ov13b10_entity_init_cfg(&ov13b10->sd, NULL);

	ret = ov13b10_ctrls_init(ov13b10);
	if (ret) {
		dev_err(ov13b10->dev, "Error ctrls init\n");
		goto free_ctrl;
	}

	ret = ov13b10_register_subdev(ov13b10);
	if (ret) {
		dev_err(ov13b10->dev, "Error register subdev\n");
		goto free_entity;
	}

	v4l2_fwnode_endpoint_free(&ov13b10->ep);

	dev_info(ov13b10->dev, "probe done \n");

	return 0;

free_entity:
	media_entity_cleanup(&ov13b10->sd.entity);
free_ctrl:
	v4l2_ctrl_handler_free(&ov13b10->ctrls);
	mutex_destroy(&ov13b10->lock);
free_err:
	v4l2_fwnode_endpoint_free(&ov13b10->ep);
return_err:
	return ret;
}

static int ov13b10_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov13b10 *ov13b10 = to_ov13b10(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	mutex_destroy(&ov13b10->lock);

	ov13b10_power_off(ov13b10);

	return 0;
}

static const struct of_device_id ov13b10_of_match[] = {
	{ .compatible = "omini, ov13b10" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ov13b10_of_match);

static struct i2c_driver ov13b10_i2c_driver = {
	.probe_new  = ov13b10_probe,
	.remove = ov13b10_remove,
	.driver = {
		.name  = "ov13b10",
		.pm = &ov13b10_pm_ops,
		.of_match_table = of_match_ptr(ov13b10_of_match),
	},
};

module_i2c_driver(ov13b10_i2c_driver);

MODULE_DESCRIPTION("Omini OV13B10 CMOS Image Sensor Driver");
MODULE_AUTHOR("keke.li");
MODULE_AUTHOR("keke.li <keke.li@amlogic.com>");
MODULE_LICENSE("GPL v2");
