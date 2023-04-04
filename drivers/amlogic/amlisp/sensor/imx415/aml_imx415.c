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

#define IMX415_GAIN       0x3090
#define IMX415_EXPOSURE   0x3050
#define IMX415_ID         0x0602

#define AML_SENSOR_NAME  "imx415-%u"

#define HDR_30FPS_1440M
#define SDR_30FPS_1440M

struct imx415_regval {
	u16 reg;
	u8 val;
};

struct imx415_mode {
	u32 width;
	u32 height;
	u32 hmax;
	u32 link_freq_index;

	const struct imx415_regval *data;
	u32 data_size;
};

struct imx415 {
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
	const struct imx415_mode *current_mode;

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

struct imx415_pixfmt {
	u32 code;
	u32 min_width;
	u32 max_width;
	u32 min_height;
	u32 max_height;
	u8 bpp;
};

static const struct imx415_pixfmt imx415_formats[] = {
	{ MEDIA_BUS_FMT_SGBRG10_1X10, 720, 3840, 720, 2160, 10 },
	{ MEDIA_BUS_FMT_SGBRG10_1X10, 1280, 3840, 720, 2160, 10 },
};

static const struct regmap_config imx415_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

#ifdef SDR_30FPS_1440M
static struct imx415_regval linear_4k_30fps_1440Mbps_4lane_10bits[] = {
	{0x3000, 0x01}, /* standby */

	{0x3002, 0x00}, /* XTMSTA */

	{0x3008, 0x54},
	{0x300A, 0x3B},
	{0x3024, 0x97},
	{0x3025, 0x11},
	{0x3028, 0x15},
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x3033, 0x08},
	{0x3050, 0x08},
	{0x30C1, 0x00},
	{0x3116, 0x23},
	{0x3118, 0xB4},
	{0x311A, 0xFC},
	{0x311E, 0x23},
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3701, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4004, 0x00},
	{0x4005, 0x06},
	{0x4018, 0x9F},
	{0x401A, 0x57},
	{0x401C, 0x57},
	{0x401E, 0x87},
	{0x4020, 0x5F},
	{0x4022, 0xA7},
	{0x4024, 0x5F},
	{0x4026, 0x97},
	{0x4028, 0x4F},

    {0x344C, 0x2B}, //fix vertical stripe issue, start
    {0x344D, 0x01},
    {0x344E, 0xED},
    {0x344F, 0x01},
    {0x3450, 0xF6},
    {0x3451, 0x02},
    {0x3720, 0x00},
    {0x39A4, 0x07},
    {0x39A8, 0x32},
    {0x39AA, 0x32},
    {0x39AC, 0x32},
    {0x39AE, 0x32},
    {0x39B0, 0x32},
    {0x39B2, 0x2F},
    {0x39B4, 0x2D},
    {0x39B6, 0x28},
    {0x39B8, 0x30},
    {0x39BA, 0x30},
    {0x39BC, 0x30},
    {0x39BE, 0x30},
    {0x39C0, 0x30},
    {0x39C2, 0x2E},
    {0x39C4, 0x2B},
    {0x39C6, 0x25}, //end

	{0x3002, 0x00},
};

#endif

#ifdef HDR_30FPS_1440M
static struct imx415_regval dol_4k_30fps_1440Mbps_4lane_10bits[] = {
	{0x3000, 0x01}, /* standby */

	{0x3002, 0x00}, /* XTMSTA */

	{0x3008, 0x54},
	{0x300A, 0x3B},
	{0x3024, 0xD0},
	{0x3028, 0x14},
	{0x302C, 0x01},
	{0x302D, 0x01},
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x3033, 0x08},
	{0x3050, 0x5C},
	{0x3051, 0x10},
	{0x3054, 0x09},
	{0x3060, 0x11},
	{0x3061, 0x01}, //
	{0x30C1, 0x00},
	{0x30CF, 0x01},
	{0x3116, 0x23},
	{0x3118, 0xB4},
	{0x311A, 0xFC},
	{0x311E, 0x23},
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3701, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4004, 0x00},
	{0x4005, 0x06},
	{0x4018, 0x9F},
	{0x401A, 0x57},
	{0x401C, 0x57},
	{0x401E, 0x87},
	{0x4020, 0x5F},
	{0x4022, 0xA7},
	{0x4024, 0x5F},
	{0x4026, 0x97},
	{0x4028, 0x4F},

	{0x344C, 0x2B}, //fix vertical stripe issue, start
	{0x344D, 0x01},
	{0x344E, 0xED},
	{0x344F, 0x01},
	{0x3450, 0xF6},
	{0x3451, 0x02},
	{0x3720, 0x00},
	{0x39A4, 0x07},
	{0x39A8, 0x32},
	{0x39AA, 0x32},
	{0x39AC, 0x32},
	{0x39AE, 0x32},
	{0x39B0, 0x32},
	{0x39B2, 0x2F},
	{0x39B4, 0x2D},
	{0x39B6, 0x28},
	{0x39B8, 0x30},
	{0x39BA, 0x30},
	{0x39BC, 0x30},
	{0x39BE, 0x30},
	{0x39C0, 0x30},
	{0x39C2, 0x2E},
	{0x39C4, 0x2B},
	{0x39C6, 0x25}, //end

	{0x3002, 0x00},
};

#else
static struct imx415_regval setting_hdr_3840_2160_4lane_848m_15fps[] = {

};
#endif

static const struct imx415_regval imx415_1080p_settings[] = {

};

static const struct imx415_regval imx415_720p_settings[] = {

};

static const struct imx415_regval imx415_10bit_settings[] = {

};


static const struct imx415_regval imx415_12bit_settings[] = {

};

/* supported link frequencies */
#define FREQ_INDEX_1080P	0
#define FREQ_INDEX_720P		1

/* supported link frequencies */

#if defined(HDR_30FPS_1440M) || defined(SDR_30FPS_1440M)
static const s64 imx415_link_freq_2lanes[] = {
	[FREQ_INDEX_1080P] = 1440000000,
	[FREQ_INDEX_720P] = 1440000000,
};

static const s64 imx415_link_freq_4lanes[] = {
	[FREQ_INDEX_1080P] = 1440000000,
	[FREQ_INDEX_720P] = 1440000000,
};

#else
static const s64 imx415_link_freq_2lanes[] = {
	[FREQ_INDEX_1080P] = 848000000,
	[FREQ_INDEX_720P] = 848000000,
};

static const s64 imx415_link_freq_4lanes[] = {
	[FREQ_INDEX_1080P] = 848000000,
	[FREQ_INDEX_720P] = 848000000,
};
#endif

static inline const s64 *imx415_link_freqs_ptr(const struct imx415 *imx415)
{
	if (imx415->nlanes == 2)
		return imx415_link_freq_2lanes;
	else {
		return imx415_link_freq_4lanes;
	}
}

static inline int imx415_link_freqs_num(const struct imx415 *imx415)
{
	if (imx415->nlanes == 2)
		return ARRAY_SIZE(imx415_link_freq_2lanes);
	else
		return ARRAY_SIZE(imx415_link_freq_4lanes);
}

/* Mode configs */
static const struct imx415_mode imx415_modes_2lanes[] = {
	{
		.width = 1920,
		.height = 1080,
		.hmax  = 0x1130,
		.data = imx415_1080p_settings,
		.data_size = ARRAY_SIZE(imx415_1080p_settings),

		.link_freq_index = FREQ_INDEX_1080P,
	},
	{
		.width = 1280,
		.height = 720,
		.hmax = 0x19c8,
		.data = imx415_720p_settings,
		.data_size = ARRAY_SIZE(imx415_720p_settings),

		.link_freq_index = FREQ_INDEX_720P,
	},
};


static const struct imx415_mode imx415_modes_4lanes[] = {
	{
		.width = 3840,
		.height = 2160,
		.hmax = 0x0898,
		.link_freq_index = FREQ_INDEX_1080P,
		.data = imx415_1080p_settings,
		.data_size = ARRAY_SIZE(imx415_1080p_settings),
	},
	{
		.width = 3840,
		.height = 2160,
		.hmax = 0x0ce4,
		.link_freq_index = FREQ_INDEX_720P,
		.data = imx415_720p_settings,
		.data_size = ARRAY_SIZE(imx415_720p_settings),
	},
};

static inline const struct imx415_mode *imx415_modes_ptr(const struct imx415 *imx415)
{
	if (imx415->nlanes == 2)
		return imx415_modes_2lanes;
	else
		return imx415_modes_4lanes;
}

static inline int imx415_modes_num(const struct imx415 *imx415)
{
	if (imx415->nlanes == 2)
		return ARRAY_SIZE(imx415_modes_2lanes);
	else
		return ARRAY_SIZE(imx415_modes_4lanes);
}

static inline struct imx415 *to_imx415(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct imx415, sd);
}

static inline int imx415_read_reg(struct imx415 *imx415, u16 addr, u8 *value)
{
	unsigned int regval;

	int i, ret;

	for ( i = 0; i < 3; ++i ) {
		ret = regmap_read(imx415->regmap, addr, &regval);
		if ( 0 == ret ) {
			break;
		}
	}

	if (ret)
		dev_err(imx415->dev, "I2C read with i2c transfer failed for addr: %x, ret %d\n", addr, ret);

	*value = regval & 0xff;
	return 0;
}

static int imx415_write_reg(struct imx415 *imx415, u16 addr, u8 value)
{
	int i, ret;

	for (i = 0; i < 3; i++) {
		ret = regmap_write(imx415->regmap, addr, value);
		if (0 == ret) {
			break;
		}
	}

	if (ret)
		dev_err(imx415->dev, "I2C write failed for addr: %x, ret %d\n", addr, ret);

	return ret;
}

static int imx415_get_id(struct imx415 *imx415)
{
	int rtn = -EINVAL;
    uint32_t sensor_id = 0;
    u8 val = 0;

	imx415_read_reg(imx415, 0x30d9, &val);
	sensor_id |= (val << 8);
    imx415_read_reg(imx415, 0x30da, &val);
	sensor_id |= val;

	if (sensor_id != IMX415_ID) {
		dev_err(imx415->dev, "Failed to get imx415 id: 0x%x\n", sensor_id);
		return rtn;
	} else {
		dev_err(imx415->dev, "success get imx415 id 0x%x", sensor_id);
	}

	return 0;
}

static int imx415_set_register_array(struct imx415 *imx415,
				     const struct imx415_regval *settings,
				     unsigned int num_settings)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < num_settings; ++i, ++settings) {
		ret = imx415_write_reg(imx415, settings->reg, settings->val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int imx415_write_buffered_reg(struct imx415 *imx415, u16 address_low,
				     u8 nr_regs, u32 value)
{
	unsigned int i;
	int ret;

	for (i = 0; i < nr_regs; i++) {
		ret = imx415_write_reg(imx415, address_low + i,
				       (u8)(value >> (i * 8)));
		if (ret) {
			dev_err(imx415->dev, "Error writing buffered registers\n");
			return ret;
		}
	}

	return ret;
}

static int imx415_set_gain(struct imx415 *imx415, u32 value)
{
	int ret;

	ret = imx415_write_buffered_reg(imx415, IMX415_GAIN, 1, value);
	if (ret)
		dev_err(imx415->dev, "Unable to write gain\n");

	return ret;
}

static int imx415_set_exposure(struct imx415 *imx415, u32 value)
{
	int ret;

	ret = imx415_write_buffered_reg(imx415, IMX415_EXPOSURE, 2, value);
	if (ret)
		dev_err(imx415->dev, "Unable to write gain\n");

	return ret;
}


/* Stop streaming */
static int imx415_stop_streaming(struct imx415 *imx415)
{
	imx415->enWDRMode = WDR_MODE_NONE;
	return imx415_write_reg(imx415, 0x3000, 0x01);
}

static int imx415_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx415 *imx415 = container_of(ctrl->handler,
					     struct imx415, ctrls);
	int ret = 0;

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(imx415->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		ret = imx415_set_gain(imx415, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret = imx415_set_exposure(imx415, ctrl->val);
		break;
	case V4L2_CID_HBLANK:
		break;
	case V4L2_CID_AML_MODE:
		imx415->enWDRMode = ctrl->val;
		break;
	default:
		dev_err(imx415->dev, "Error ctrl->id %u, flag 0x%lx\n",
			ctrl->id, ctrl->flags);
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(imx415->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx415_ctrl_ops = {
	.s_ctrl = imx415_set_ctrl,
};
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int imx415_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#else
static int imx415_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	if (code->index >= ARRAY_SIZE(imx415_formats))
		return -EINVAL;

	code->code = imx415_formats[code->index].code;

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int imx415_enum_frame_size(struct v4l2_subdev *sd,
			        struct v4l2_subdev_state *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#else
static int imx415_enum_frame_size(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	if (fse->index >= ARRAY_SIZE(imx415_formats))
		return -EINVAL;

	fse->min_width = imx415_formats[fse->index].min_width;
	fse->min_height = imx415_formats[fse->index].min_height;;
	fse->max_width = imx415_formats[fse->index].max_width;
	fse->max_height = imx415_formats[fse->index].max_height;

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int imx415_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *cfg,
			  struct v4l2_subdev_format *fmt)
#else
static int imx415_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
#endif
{
	struct imx415 *imx415 = to_imx415(sd);
	struct v4l2_mbus_framefmt *framefmt;

	mutex_lock(&imx415->lock);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		framefmt = v4l2_subdev_get_try_format(&imx415->sd, cfg,
						      fmt->pad);
	else
		framefmt = &imx415->current_format;

	fmt->format = *framefmt;

	mutex_unlock(&imx415->lock);

	return 0;
}


static inline u8 imx415_get_link_freq_index(struct imx415 *imx415)
{
	return imx415->current_mode->link_freq_index;
}

static s64 imx415_get_link_freq(struct imx415 *imx415)
{
	u8 index = imx415_get_link_freq_index(imx415);

	return *(imx415_link_freqs_ptr(imx415) + index);
}

static u64 imx415_calc_pixel_rate(struct imx415 *imx415)
{
	s64 link_freq = imx415_get_link_freq(imx415);
	u8 nlanes = imx415->nlanes;
	u64 pixel_rate;

	/* pixel rate = link_freq * 2 * nr_of_lanes / bits_per_sample */
	pixel_rate = link_freq * 2 * nlanes;
	do_div(pixel_rate, imx415->bpp);
	return pixel_rate;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int imx415_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *cfg,
			struct v4l2_subdev_format *fmt)
#else
static int imx415_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct imx415 *imx415 = to_imx415(sd);
	const struct imx415_mode *mode;
	struct v4l2_mbus_framefmt *format;
	unsigned int i,ret;

	mutex_lock(&imx415->lock);

	mode = v4l2_find_nearest_size(imx415_modes_ptr(imx415),
				 imx415_modes_num(imx415),
				width, height,
				fmt->format.width, fmt->format.height);

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;

	for (i = 0; i < ARRAY_SIZE(imx415_formats); i++) {
		if (imx415_formats[i].code == fmt->format.code) {
			dev_err(imx415->dev, " zzw find proper format %d \n",i);
			break;
		}
	}

	if (i >= ARRAY_SIZE(imx415_formats)) {
		i = 0;
		dev_err(imx415->dev, " zzw No format. reset i = 0 \n");
	}

	fmt->format.code = imx415_formats[i].code;
	fmt->format.field = V4L2_FIELD_NONE;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		dev_err(imx415->dev, " zzw try format \n");
		format = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		mutex_unlock(&imx415->lock);
		return 0;
	} else {
		dev_err(imx415->dev, " zzw set format, w %d, h %d, code 0x%x \n",
            fmt->format.width, fmt->format.height,
            fmt->format.code);
		format = &imx415->current_format;
		imx415->current_mode = mode;
		imx415->bpp = imx415_formats[i].bpp;

		if (imx415->link_freq)
			__v4l2_ctrl_s_ctrl(imx415->link_freq, imx415_get_link_freq_index(imx415) );
		if (imx415->pixel_rate)
			__v4l2_ctrl_s_ctrl_int64(imx415->pixel_rate, imx415_calc_pixel_rate(imx415) );
	}

	*format = fmt->format;
	imx415->status = 0;

	mutex_unlock(&imx415->lock);

	if (imx415->enWDRMode) {
		/* Set init register settings */
		#ifdef HDR_30FPS_1440M
		ret = imx415_set_register_array(imx415, dol_4k_30fps_1440Mbps_4lane_10bits,
			ARRAY_SIZE(dol_4k_30fps_1440Mbps_4lane_10bits));
		dev_err(imx415->dev, "%s:%d ***************WDR 1440M******\n",__FUNCTION__,__LINE__);
		#endif
		if (ret < 0) {
			dev_err(imx415->dev, "Could not set init registers\n");
			return ret;
		} else
			dev_err(imx415->dev, "imx415 wdr mode init...\n");
	} else {
		/* Set init register settings */

		#ifdef SDR_30FPS_1440M
		ret = imx415_set_register_array(imx415, linear_4k_30fps_1440Mbps_4lane_10bits,
			ARRAY_SIZE(linear_4k_30fps_1440Mbps_4lane_10bits));
		dev_err(imx415->dev, "%s:%d ***************1440M SDR******\n",__FUNCTION__,__LINE__);
		#endif

		if (ret < 0) {
			dev_err(imx415->dev, "Could not set init registers\n");
			return ret;
		} else
			dev_err(imx415->dev, "imx415 linear mode init...\n");
	}

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
int imx415_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *cfg,
			     struct v4l2_subdev_selection *sel)
#else
int imx415_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_subdev_selection *sel)
#endif
{
	int rtn = 0;
	struct imx415 *imx415 = to_imx415(sd);
	const struct imx415_mode *mode = imx415->current_mode;

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
		dev_err(imx415->dev, "Error support target: 0x%x\n", sel->target);
	break;
	}

	return rtn;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int imx415_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_state *cfg)
#else
static int imx415_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg)
#endif
{
	struct v4l2_subdev_format fmt = { 0 };

	fmt.which = cfg ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.format.width = 1920;
	fmt.format.height = 1080;

	imx415_set_fmt(subdev, cfg, &fmt);

	return 0;
}

/* Start streaming */
static int imx415_start_streaming(struct imx415 *imx415)
{

	/* Start streaming */
	return imx415_write_reg(imx415, 0x3000, 0x00);
}

static int imx415_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx415 *imx415 = to_imx415(sd);
	int ret = 0;

	if (imx415->status == enable)
		return ret;
	else
		imx415->status = enable;

	if (enable) {
		ret = imx415_start_streaming(imx415);
		if (ret) {
			dev_err(imx415->dev, "Start stream failed\n");
			goto unlock_and_return;
		}

		dev_info(imx415->dev, "stream on\n");
	} else {
		imx415_stop_streaming(imx415);

		dev_info(imx415->dev, "stream off\n");
	}

unlock_and_return:

	return ret;
}

static int imx415_power_on(struct imx415 *imx415)
{
	int ret;

	reset_am_enable(imx415->dev,"reset", 1);

	ret = mclk_enable(imx415->dev,24000000);
	if (ret < 0 )
		dev_err(imx415->dev, "set mclk fail\n");
	udelay(30);

	// 30ms
	usleep_range(30000, 31000);

	return 0;
}

static int imx415_power_off(struct imx415 *imx415)
{
	mclk_disable(imx415->dev);

	reset_am_enable(imx415->dev,"reset", 0);

	return 0;
}

static int imx415_power_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx415 *imx415 = to_imx415(sd);

	reset_am_enable(imx415->dev,"reset", 0);

	return 0;
}

static int imx415_power_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx415 *imx415 = to_imx415(sd);

	reset_am_enable(imx415->dev,"reset", 1);

	return 0;
}

static int imx415_log_status(struct v4l2_subdev *sd)
{
	struct imx415 *imx415 = to_imx415(sd);

	dev_info(imx415->dev, "log status done\n");

	return 0;
}

int imx415_sbdev_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh) {
	struct imx415 *imx415 = to_imx415(sd);
	imx415_power_on(imx415);
	return 0;
}

int imx415_sbdev_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh) {
	struct imx415 *imx415 = to_imx415(sd);
	imx415_stop_streaming(imx415);
	imx415_power_off(imx415);
	return 0;
}

static const struct dev_pm_ops imx415_pm_ops = {
	SET_RUNTIME_PM_OPS(imx415_power_suspend, imx415_power_resume, NULL)
};

const struct v4l2_subdev_core_ops imx415_core_ops = {
	.log_status = imx415_log_status,
};

static const struct v4l2_subdev_video_ops imx415_video_ops = {
	.s_stream = imx415_set_stream,
};

static const struct v4l2_subdev_pad_ops imx415_pad_ops = {
	.init_cfg = imx415_entity_init_cfg,
	.enum_mbus_code = imx415_enum_mbus_code,
	.enum_frame_size = imx415_enum_frame_size,
	.get_selection = imx415_get_selection,
	.get_fmt = imx415_get_fmt,
	.set_fmt = imx415_set_fmt,
};

static struct v4l2_subdev_internal_ops imx415_internal_ops = {
	.open = imx415_sbdev_open,
	.close = imx415_sbdev_close,
};

static const struct v4l2_subdev_ops imx415_subdev_ops = {
	.core = &imx415_core_ops,
	.video = &imx415_video_ops,
	.pad = &imx415_pad_ops,
};

static const struct media_entity_operations imx415_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static struct v4l2_ctrl_config wdr_cfg = {
	.ops = &imx415_ctrl_ops,
	.id = V4L2_CID_AML_MODE,
	.name = "wdr mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	.min = 0,
	.max = 2,
	.step = 1,
	.def = 0,
};

static int imx415_ctrls_init(struct imx415 *imx415)
{
	int rtn = 0;

	v4l2_ctrl_handler_init(&imx415->ctrls, 4);

	v4l2_ctrl_new_std(&imx415->ctrls, &imx415_ctrl_ops,
				V4L2_CID_GAIN, 0, 0xF0, 1, 0);

	v4l2_ctrl_new_std(&imx415->ctrls, &imx415_ctrl_ops,
				V4L2_CID_EXPOSURE, 0, 0xffff, 1, 0);

	imx415->link_freq = v4l2_ctrl_new_int_menu(&imx415->ctrls,
					       &imx415_ctrl_ops,
					       V4L2_CID_LINK_FREQ,
					       imx415_link_freqs_num(imx415) - 1,
					       0, imx415_link_freqs_ptr(imx415) );

	if (imx415->link_freq)
		imx415->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx415->pixel_rate = v4l2_ctrl_new_std(&imx415->ctrls,
					       &imx415_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       1, INT_MAX, 1,
					       imx415_calc_pixel_rate(imx415));

	imx415->wdr = v4l2_ctrl_new_custom(&imx415->ctrls, &wdr_cfg, NULL);

	imx415->sd.ctrl_handler = &imx415->ctrls;

	if (imx415->ctrls.error) {
		dev_err(imx415->dev, "Control initialization a error  %d\n",
			imx415->ctrls.error);
		rtn = imx415->ctrls.error;
	}

	return rtn;
}

static int imx415_parse_power(struct imx415 *imx415)
{
	int rtn = 0;

	imx415->rst_gpio = devm_gpiod_get_optional(imx415->dev,
						"reset",
						GPIOD_OUT_LOW);
	if (IS_ERR(imx415->rst_gpio)) {
		dev_err(imx415->dev, "Cannot get reset gpio\n");
		rtn = PTR_ERR(imx415->rst_gpio);
		goto err_return;
	}

	/*imx415->pwdn_gpio = devm_gpiod_get_optional(imx415->dev,
						"pwdn",
						GPIOD_OUT_LOW);
	if (IS_ERR(imx415->pwdn_gpio)) {
		dev_err(imx415->dev, "Cannot get pwdn gpio\n");
		rtn = PTR_ERR(imx415->pwdn_gpio);
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
static s64 imx415_check_link_freqs(const struct imx415 *imx415,
				   const struct v4l2_fwnode_endpoint *ep)
{
	int i, j;
	const s64 *freqs = imx415_link_freqs_ptr(imx415);
	int freqs_count = imx415_link_freqs_num(imx415);

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

static int imx415_parse_endpoint(struct imx415 *imx415)
{
	int rtn = 0;
	s64 fq;
	struct fwnode_handle *endpoint = NULL;
	//struct device_node *node = NULL;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(imx415->dev), NULL);
	if (!endpoint) {
		dev_err(imx415->dev, "Endpoint node not found\n");
		return -EINVAL;
	}

	rtn = v4l2_fwnode_endpoint_alloc_parse(endpoint, &imx415->ep);
	fwnode_handle_put(endpoint);
	if (rtn) {
		dev_err(imx415->dev, "Parsing endpoint node failed\n");
		rtn = -EINVAL;
		goto err_return;
	}

	/* Only CSI2 is supported for now */
	if (imx415->ep.bus_type != V4L2_MBUS_CSI2_DPHY) {
		dev_err(imx415->dev, "Unsupported bus type, should be CSI2\n");
		rtn = -EINVAL;
		goto err_free;
	}

	imx415->nlanes = imx415->ep.bus.mipi_csi2.num_data_lanes;
	if (imx415->nlanes != 2 && imx415->nlanes != 4) {
		dev_err(imx415->dev, "Invalid data lanes: %d\n", imx415->nlanes);
		rtn = -EINVAL;
		goto err_free;
	}
	dev_info(imx415->dev, "Using %u data lanes\n", imx415->nlanes);

	if (!imx415->ep.nr_of_link_frequencies) {
		dev_err(imx415->dev, "link-frequency property not found in DT\n");
		rtn = -EINVAL;
		goto err_free;
	}

	/* Check that link frequences for all the modes are in device tree */
	fq = imx415_check_link_freqs(imx415, &imx415->ep);
	if (fq) {
		dev_err(imx415->dev, "Link frequency of %lld is not supported\n", fq);
		rtn = -EINVAL;
		goto err_free;
	}

	return rtn;

err_free:
	v4l2_fwnode_endpoint_free(&imx415->ep);
err_return:
	return rtn;
}


static int imx415_register_subdev(struct imx415 *imx415)
{
	int rtn = 0;

	v4l2_i2c_subdev_init(&imx415->sd, imx415->client, &imx415_subdev_ops);

	imx415->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	imx415->sd.dev = &imx415->client->dev;
	imx415->sd.internal_ops = &imx415_internal_ops;
	imx415->sd.entity.ops = &imx415_subdev_entity_ops;
	imx415->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	snprintf(imx415->sd.name, sizeof(imx415->sd.name), AML_SENSOR_NAME, imx415->index);

	imx415->pad.flags = MEDIA_PAD_FL_SOURCE;
	rtn = media_entity_pads_init(&imx415->sd.entity, 1, &imx415->pad);
	if (rtn < 0) {
		dev_err(imx415->dev, "Could not register media entity\n");
		goto err_return;
	}

	rtn = v4l2_async_register_subdev(&imx415->sd);
	if (rtn < 0) {
		dev_err(imx415->dev, "Could not register v4l2 device\n");
		goto err_return;
	}

err_return:
	return rtn;
}

static int imx415_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct imx415 *imx415;
	int ret = -EINVAL;


	imx415 = devm_kzalloc(dev, sizeof(*imx415), GFP_KERNEL);
	if (!imx415)
		return -ENOMEM;
	dev_err(dev, "i2c dev addr 0x%x, name %s \n", client->addr, client->name);

	imx415->dev = dev;
	imx415->client = client;

	imx415->regmap = devm_regmap_init_i2c(client, &imx415_regmap_config);
	if (IS_ERR(imx415->regmap)) {
		dev_err(dev, "Unable to initialize I2C\n");
		return -ENODEV;
	}

	if (of_property_read_u32(dev->of_node, "index", &imx415->index)) {
		dev_err(dev, "Failed to read sensor index. default to 0\n");
		imx415->index = 0;
	}


	ret = imx415_parse_endpoint(imx415);
	if (ret) {
		dev_err(imx415->dev, "Error parse endpoint\n");
		goto return_err;
	}

	ret = imx415_parse_power(imx415);
	if (ret) {
		dev_err(imx415->dev, "Error parse power ctrls\n");
		goto free_err;
	}

	mutex_init(&imx415->lock);

	/* Power on the device to match runtime PM state below */
	dev_err(dev, "bef get id. pwdn -0, reset - 1\n");

	ret = imx415_power_on(imx415);
	if (ret < 0) {
		dev_err(dev, "Could not power on the device\n");
		goto free_err;
	}


	ret = imx415_get_id(imx415);
	if (ret) {
		dev_err(dev, "Could not get id\n");
		imx415_power_off(imx415);
		goto free_err;
	}

	/*
	 * Initialize the frame format. In particular, imx415->current_mode
	 * and imx415->bpp are set to defaults: imx415_calc_pixel_rate() call
	 * below in imx415_ctrls_init relies on these fields.
	 */
	imx415_entity_init_cfg(&imx415->sd, NULL);

	ret = imx415_ctrls_init(imx415);
	if (ret) {
		dev_err(imx415->dev, "Error ctrls init\n");
		goto free_ctrl;
	}

	ret = imx415_register_subdev(imx415);
	if (ret) {
		dev_err(imx415->dev, "Error register subdev\n");
		goto free_entity;
	}

	v4l2_fwnode_endpoint_free(&imx415->ep);

	dev_info(imx415->dev, "probe done \n");

	return 0;

free_entity:
	media_entity_cleanup(&imx415->sd.entity);
free_ctrl:
	v4l2_ctrl_handler_free(&imx415->ctrls);
	mutex_destroy(&imx415->lock);
free_err:
	v4l2_fwnode_endpoint_free(&imx415->ep);
return_err:
	return ret;
}

static int imx415_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx415 *imx415 = to_imx415(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	mutex_destroy(&imx415->lock);

	imx415_power_off(imx415);

	return 0;
}

static const struct of_device_id imx415_of_match[] = {
	{ .compatible = "sony, imx415" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx415_of_match);

static struct i2c_driver imx415_i2c_driver = {
	.probe_new  = imx415_probe,
	.remove = imx415_remove,
	.driver = {
		.name  = "imx415",
		.pm = &imx415_pm_ops,
		.of_match_table = of_match_ptr(imx415_of_match),
	},
};

module_i2c_driver(imx415_i2c_driver);

MODULE_DESCRIPTION("Sony IMX290 CMOS Image Sensor Driver");
MODULE_AUTHOR("keke.li");
MODULE_AUTHOR("keke.li <keke.li@amlogic.com>");
MODULE_LICENSE("GPL v2");
