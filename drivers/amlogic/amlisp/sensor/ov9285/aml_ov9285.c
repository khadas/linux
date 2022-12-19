/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
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

#include "aml_misc.h"
#include "aml_ov9285.h"

static const struct ov9285_pixfmt ov9285_formats[] = {
	//{MEDIA_BUS_FMT_SRGGB8_1X8, 640, 1328, 400, 1120},
	{MEDIA_BUS_FMT_SRGGB10_1X10, 640, 1328, 400, 1120},
};

static const struct regmap_config ov9285_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static const s64 ov9285_link_freq[] = {
	OV9285_DEFAULT_LINK_FREQ,
};

static const struct v4l2_ctrl_ops ov9285_ctrl_ops = {
	.s_ctrl = ov9285_set_ctrl,
};

static const struct v4l2_ctrl_config fps_cfg = {
	.ops = &ov9285_ctrl_ops,
	.id = V4L2_CID_AML_USER_FPS,
	.name = "sensor ov9285 fps",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 60,
	.step = 1,
	.def = 30,
};

static const struct v4l2_ctrl_config strobe_cfg = {
	.ops = &ov9285_ctrl_ops,
	.id = V4L2_CID_AML_STROBE,
	.name = "sensor ov9285 strobe",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
};

static const struct v4l2_ctrl_config role_cfg = {
	.ops = &ov9285_ctrl_ops,
	.id = V4L2_CID_AML_ROLE,
	.name = "sensor ov9285 role",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
};

static const struct ov9285_mode ov9285_modes[] = {
	{
		.width = 640,
		.height = 400,
		.data = ov9285_640_400_10bits_800mbps_2lane_30fps_settings,
		.data_size = ARRAY_SIZE(ov9285_640_400_10bits_800mbps_2lane_30fps_settings),
		.pixel_rate = 1955555,
		.hblank = 280,
		.link_freq_index = 0,
	},

	{
		.width = 1328,
		.height = 1120,
		.data = ov9285_1328_1120_10bits_800mbps_2lane_30fps_settings,
		.data_size = ARRAY_SIZE(ov9285_1328_1120_10bits_800mbps_2lane_30fps_settings),
		.pixel_rate = 1955555,
		.hblank = 280,
		.link_freq_index = 0,
	},
};

static inline struct ov9285 *to_ov9285(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct ov9285, sd);
}

static inline int ov9285_read_reg(struct ov9285 *ov9285, u16 addr, u8 *value)
{
	unsigned int regval;
	int i, ret;

	for (i = 0; i < 3; i++) {
		ret = regmap_read(ov9285->regmap, addr, &regval);
		if (!ret)
			break;
	}

	if (ret)
		dev_err(ov9285->dev, "I2C read failed for addr: %x, ret %d\n", addr, ret);

	*value = regval & 0xff;

	return 0;
}

static int ov9285_write_reg(struct ov9285 *ov9285, u16 addr, u8 value)
{
	int i, ret;

	for (i = 0; i < 3; i++) {
		ret = regmap_write(ov9285->regmap, addr, value);
		if (!ret)
			break;
	}

	if (ret)
		dev_err(ov9285->dev, "I2C write failed for addr: %x, ret %d\n", addr, ret);

	return ret;
}

static int ov9285_get_id(struct ov9285 *ov9285)
{
	u32 id = 0;
	u8 val = 0;

	ov9285_read_reg(ov9285, 0x300a, &val);
	id |= (val << 8);

	ov9285_read_reg(ov9285, 0x300b, &val);
	id |= val;

	if (id != OV9285_ID) {
		dev_err(ov9285->dev, "Failed to get ov9285 id: 0x%x\n", id);
		return -EINVAL;
	}

	dev_info(ov9285->dev, "success get id\n");

	return 0;
}

static int ov9285_set_register_array(struct ov9285 *ov9285,
				     const struct ov9285_regval *settings,
				     unsigned int num_settings)
{
	unsigned int i;
	int ret;

	for (i = 0; i < num_settings; ++i, ++settings) {
		ret = ov9285_write_reg(ov9285, settings->reg, settings->val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ov9285_set_fps(struct ov9285 *ov9285, u32 value)
{
	switch (value) {
	case 30:
		ov9285_write_reg(ov9285, 0x380e, 0x0d);
		ov9285_write_reg(ov9285, 0x380f, 0xec);
	break;
	case 60:
		ov9285_write_reg(ov9285, 0x380e, 0x06);
		ov9285_write_reg(ov9285, 0x380f, 0xf6);
	break;
	default:
		dev_err(ov9285->dev, "Error input fps\n");
		return -EINVAL;
	}

	return 0;
}

static int ov9285_set_gain(struct ov9285 *ov9285, u32 value)
{
	ov9285_write_reg(ov9285, 0x3509, value);

	return 0;
}

static int ov9285_set_exposure(struct ov9285 *ov9285, u32 value)
{
	u8 hval, mval, lval;

	hval = (value >> 16) & 0xf;
	mval = (value >> 8) & 0xff;
	lval = value & 0xff;

	ov9285_write_reg(ov9285, 0x3500, hval);
	ov9285_write_reg(ov9285, 0x3501, mval);
	ov9285_write_reg(ov9285, 0x3502, lval);

	return 0;
}

/* Stop streaming */
static int ov9285_stop_streaming(struct ov9285 *ov9285)
{
	int ret;

	ret = ov9285_write_reg(ov9285, OV9285_STANDBY, 0x00);

	return ret;
}

static int ov9285_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov9285 *ov9285 = container_of(ctrl->handler,
					     struct ov9285, ctrls);
	int ret = 0;

	/* V4L2 controls values will be applied only when power is already up */
	if (!pm_runtime_get_if_in_use(ov9285->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		ret = ov9285_set_gain(ov9285, ctrl->val);
	break;
	case V4L2_CID_EXPOSURE:
		ret = ov9285_set_exposure(ov9285, ctrl->val);
	break;
	case V4L2_CID_HBLANK:
	break;
	case V4L2_CID_AML_USER_FPS:
		ret = ov9285_set_fps(ov9285, ctrl->val);
	break;
	case V4L2_CID_AML_ROLE:
	break;
	case V4L2_CID_AML_STROBE:
	break;
	default:
		dev_err(ov9285->dev, "Error ctrl->id %u, flag 0x%lx\n",
			ctrl->id, ctrl->flags);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov9285_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#else
static int ov9285_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	if (code->index >= ARRAY_SIZE(ov9285_formats))
		return -EINVAL;

	code->code = ov9285_formats[code->index].code;

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov9285_enum_frame_size(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#else
static int ov9285_enum_frame_size(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	if (fse->index >= ARRAY_SIZE(ov9285_formats))
		return -EINVAL;

	fse->min_width = ov9285_formats[fse->index].min_width;
	fse->min_height = ov9285_formats[fse->index].min_height;
	fse->max_width = ov9285_formats[fse->index].max_width;
	fse->max_height = ov9285_formats[fse->index].max_height;

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov9285_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *cfg,
			  struct v4l2_subdev_format *fmt)
#else
static int ov9285_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
#endif
{
	struct ov9285 *ov9285 = to_ov9285(sd);
	struct v4l2_mbus_framefmt *framefmt;

	mutex_lock(&ov9285->lock);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		framefmt = v4l2_subdev_get_try_format(&ov9285->sd, cfg,
						      fmt->pad);
	else
		framefmt = &ov9285->current_format;

	fmt->format = *framefmt;

	mutex_unlock(&ov9285->lock);

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov9285_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *cfg,
			struct v4l2_subdev_format *fmt)
#else
static int ov9285_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct ov9285 *ov9285 = to_ov9285(sd);
	const struct ov9285_mode *mode;
	struct v4l2_mbus_framefmt *format;
	unsigned int i;

	mutex_lock(&ov9285->lock);

	mode = v4l2_find_nearest_size(ov9285_modes,
				ARRAY_SIZE(ov9285_modes),
				width, height,
				fmt->format.width, fmt->format.height);

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;

	for (i = 0; i < ARRAY_SIZE(ov9285_formats); i++)
		if (ov9285_formats[i].code == fmt->format.code)
			break;

	if (i >= ARRAY_SIZE(ov9285_formats))
		i = 0;

	fmt->format.code = ov9285_formats[i].code;
	fmt->format.field = V4L2_FIELD_NONE;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		format = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
	} else {
		format = &ov9285->current_format;
		__v4l2_ctrl_s_ctrl(ov9285->link_freq, mode->link_freq_index);
		__v4l2_ctrl_s_ctrl_int64(ov9285->pixel_rate, mode->pixel_rate);
		__v4l2_ctrl_s_ctrl(ov9285->hblank, mode->hblank);

		ov9285->current_mode = mode;
	}

	*format = fmt->format;
	ov9285->status = 0;

	mutex_unlock(&ov9285->lock);

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
int ov9285_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *cfg,
			     struct v4l2_subdev_selection *sel)
#else
int ov9285_get_selection(struct v4l2_subdev *sd,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_subdev_selection *sel)
#endif
{
	int rtn = 0;
	struct ov9285 *ov9285 = to_ov9285(sd);
	const struct ov9285_mode *mode = ov9285->current_mode;

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
		dev_err(ov9285->dev, "Error support target: 0x%x\n", sel->target);
	break;
	}

	return rtn;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int ov9285_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_state *cfg)
#else
static int ov9285_entity_init_cfg(struct v4l2_subdev *subdev,
				  struct v4l2_subdev_pad_config *cfg)
#endif
{
	struct v4l2_subdev_format fmt = { 0 };

	fmt.which = cfg ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.format.width = 640;
	fmt.format.height = 480;

	ov9285_set_fmt(subdev, cfg, &fmt);

	return 0;
}

static int ov9285_write_current_format(struct ov9285 *ov9285,
				       struct v4l2_mbus_framefmt *format)
{
	switch (format->code) {
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		ov9285_write_reg(ov9285, 0x4601, 0x9f);
		ov9285_write_reg(ov9285, 0x3662, 0x17);
	break;
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		ov9285_write_reg(ov9285, 0x4601, 0x04);
		ov9285_write_reg(ov9285, 0x3662, 0x15);
	break;
	default:
		dev_err(ov9285->dev, "Unknown pixel format\n");
		return -EINVAL;
	}

	return 0;
}

static int ov9285_write_embedded_line(struct ov9285 *ov9285,
				       struct v4l2_mbus_framefmt *format)
{
	ov9285_write_reg(ov9285, 0x4600, 0x00);
	ov9285_write_reg(ov9285, 0x4601, 0x01);
	ov9285_write_reg(ov9285, 0x4603, 0x01);
	ov9285_write_reg(ov9285, 0x4307, 0x81);
	ov9285_write_reg(ov9285, 0x3673, 0xe4);

	return 0;
}

/* Start streaming */
static int ov9285_start_streaming(struct ov9285 *ov9285)
{
	int ret;

	/* Apply default values of current mode */
	ret = ov9285_set_register_array(ov9285, ov9285->current_mode->data,
				ov9285->current_mode->data_size);
	if (ret < 0) {
		dev_err(ov9285->dev, "Could not set current mode\n");
		return ret;
	}

	/* Set current frame format */
	ret = ov9285_write_current_format(ov9285, &ov9285->current_format);
	if (ret < 0) {
		dev_err(ov9285->dev, "Could not set frame format\n");
		return ret;
	}

	ret = ov9285_write_embedded_line(ov9285, &ov9285->current_format);
	if (ret < 0) {
		dev_err(ov9285->dev, "Could not set embeded line\n");
		return ret;
	}

	/* Apply customized values from user */
	ret = v4l2_ctrl_handler_setup(ov9285->sd.ctrl_handler);
	if (ret) {
		dev_err(ov9285->dev, "Could not sync v4l2 controls\n");
		return ret;
	}

	msleep(2);

	if(ov9285->master) {
		ret = ov9285_set_register_array(ov9285, ov9285_master_settings,
				ARRAY_SIZE(ov9285_master_settings));
	} else {
		ret = ov9285_set_register_array(ov9285, ov9285_slave_settings,
				ARRAY_SIZE(ov9285_slave_settings));
	}

	if(ret < 0) {
		dev_err(ov9285->dev, "Could not set %s settings ret=%d", ov9285->master?"master":"slave", ret);
		return ret;
	}

	return ov9285_write_reg(ov9285, OV9285_STANDBY, 0x01);
}

static int ov9285_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov9285 *ov9285 = to_ov9285(sd);
	int ret = 0;

	if (ov9285->status == enable)
		return ret;
	else
		ov9285->status = enable;

	if (enable) {
		ret = ov9285_start_streaming(ov9285);
		if (ret) {
			dev_err(ov9285->dev, "Start stream failed\n");
			goto unlock_and_return;
		}

		dev_info(ov9285->dev, "stream on\n");
	} else {
		ov9285_stop_streaming(ov9285);

		dev_info(ov9285->dev, "stream off\n");
	}

unlock_and_return:

	return ret;
}

static int ov9285_power_on(struct ov9285 *ov9285)
{
	int ret;

	if (ov9285->vddcam) {
		regulator_set_voltage(ov9285->vddcam, 1200000, 1200000);

		ret = regulator_enable(ov9285->vddcam);
		if (ret) {
			dev_err(ov9285->dev, "Failed to open vddcam: ret %d\n", ret);
			return ret;
		}

		udelay(1000);
	}

	if (ov9285->viocam) {
		regulator_set_voltage(ov9285->viocam, 1800000, 1800000);

		ret = regulator_enable(ov9285->viocam);
		if (ret) {
			dev_err(ov9285->dev, "Failed to open viocam: ret %d\n", ret);
			return ret;
		}

		udelay(1000);
	}

	if (ov9285->pwr_gpio) {
		ret = gpiod_direction_output(ov9285->pwr_gpio, 1);
		if (ret) {
			dev_err(ov9285->dev, "Failed to power on: ret %d\n", ret);
			return ret;
		}

		udelay(1000);
	}

	ret = clk_prepare_enable(ov9285->xclk);
	if (ret) {
		dev_err(ov9285->dev, "Failed to enable clock\n");
		return ret;
	}

	udelay(500);

	if (ov9285->reset_gpio) {
		ret = gpiod_direction_output(ov9285->reset_gpio, 1);
		if (ret) {
			dev_err(ov9285->dev, "Failed to set reset: ret %d\n", ret);
			return ret;
		}

		udelay(1000);
	}

	if (ov9285->pwdn_gpio) {
		ret = gpiod_direction_output(ov9285->pwdn_gpio, 1);
		if (ret) {
			dev_err(ov9285->dev, "Failed to set pwdn: ret %d\n", ret);
			return ret;
		}

		udelay(1000);
	}

	return 0;
}

static int ov9285_power_off(struct ov9285 *ov9285)
{

	int ret = 0;

	if (ov9285->vddcam) {
		ret = regulator_disable(ov9285->vddcam);
		if (ret) {
			dev_err(ov9285->dev, "Failed to disable vddcam: ret %d\n", ret);
			return ret;
		}

		udelay(1000);
	}

	if (ov9285->viocam) {
		ret = regulator_disable(ov9285->viocam);
		if (ret) {
			dev_err(ov9285->dev, "Failed to disable viocam: ret %d\n", ret);
			return ret;
		}

		udelay(1000);
	}

	if (ov9285->reset_gpio) {
		ret = gpiod_direction_output(ov9285->reset_gpio, 0);
		if (ret) {
			dev_err(ov9285->dev, "Failed to set reset: ret %d\n", ret);
			return ret;
		}

		udelay(1000);
	}

	if (ov9285->pwdn_gpio) {
		ret = gpiod_direction_output(ov9285->pwdn_gpio, 1);
		if (ret) {
			dev_err(ov9285->dev, "Failed to set pwdn: ret %d\n", ret);
			return ret;
		}
	}

	clk_disable_unprepare(ov9285->xclk);

	if (ov9285->pwr_gpio) {
		ret = gpiod_direction_output(ov9285->pwr_gpio, 1);
		if (ret) {
			dev_err(ov9285->dev, "Failed to power on: ret %d\n", ret);
			return ret;
		}

		udelay(1000);
	}

	return 0;
}

static int ov9285_power_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov9285 *ov9285 = to_ov9285(sd);

	if (ov9285->pwdn_gpio)
		gpiod_direction_output(ov9285->pwdn_gpio, 0);

	clk_disable_unprepare(ov9285->xclk);

	dev_info(dev, "suspend.\n");

	return 0;
}

static int ov9285_power_resume(struct device *dev)
{
	int ret;

	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov9285 *ov9285 = to_ov9285(sd);

	ret = clk_prepare_enable(ov9285->xclk);
	if (ret) {
		dev_err(ov9285->dev, "Failed to enable clock\n");
	}

	if (ov9285->pwdn_gpio)
		gpiod_direction_output(ov9285->pwdn_gpio, 1);

	dev_info(dev, "resume.\n");

	return ret;
}

static int ov9285_log_status(struct v4l2_subdev *sd)
{
	struct ov9285 *ov9285 = to_ov9285(sd);

	dev_info(ov9285->dev, "log status done\n");

	return 0;
}

static const struct dev_pm_ops ov9285_pm_ops = {
	.suspend = ov9285_power_suspend,
	.resume = ov9285_power_resume,
	.runtime_suspend = ov9285_power_suspend,
	.runtime_resume = ov9285_power_resume,
};

const struct v4l2_subdev_core_ops ov9285_core_ops = {
	.log_status = ov9285_log_status,
};

static const struct v4l2_subdev_video_ops ov9285_video_ops = {
	.s_stream = ov9285_set_stream,
};

static const struct v4l2_subdev_pad_ops ov9285_pad_ops = {
	.init_cfg = ov9285_entity_init_cfg,
	.enum_mbus_code = ov9285_enum_mbus_code,
	.enum_frame_size = ov9285_enum_frame_size,
	.get_selection = ov9285_get_selection,
	.get_fmt = ov9285_get_fmt,
	.set_fmt = ov9285_set_fmt,
};

static const struct v4l2_subdev_ops ov9285_subdev_ops = {
	.core = &ov9285_core_ops,
	.video = &ov9285_video_ops,
	.pad = &ov9285_pad_ops,
};

static const struct media_entity_operations ov9285_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static int ov9285_ctrls_init(struct ov9285 *ov9285)
{
	int rtn = 0;

	v4l2_ctrl_handler_init(&ov9285->ctrls, 8);

	v4l2_ctrl_new_std(&ov9285->ctrls, &ov9285_ctrl_ops,
				V4L2_CID_GAIN, 0, 0xff, 1, 0);
	v4l2_ctrl_new_std(&ov9285->ctrls, &ov9285_ctrl_ops,
				V4L2_CID_EXPOSURE, 0, 0xffff0, 1, 0);

	ov9285->link_freq = v4l2_ctrl_new_int_menu(&ov9285->ctrls,
					       &ov9285_ctrl_ops,
					       V4L2_CID_LINK_FREQ,
					       ARRAY_SIZE(ov9285_link_freq) - 1,
					       0, ov9285_link_freq);
	if (ov9285->link_freq)
		ov9285->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ov9285->pixel_rate = v4l2_ctrl_new_std(&ov9285->ctrls, &ov9285_ctrl_ops,
						V4L2_CID_PIXEL_RATE, 1,
						INT_MAX, 1,
						ov9285_modes[0].pixel_rate);

	ov9285->hblank = v4l2_ctrl_new_std(&ov9285->ctrls, &ov9285_ctrl_ops,
						V4L2_CID_HBLANK, 1,
						INT_MAX, 1,
						ov9285_modes[0].hblank);

	ov9285->strobe = v4l2_ctrl_new_custom(&ov9285->ctrls, &strobe_cfg, NULL);
	ov9285->fps = v4l2_ctrl_new_custom(&ov9285->ctrls, &fps_cfg, NULL);
	ov9285->role = v4l2_ctrl_new_custom(&ov9285->ctrls, &role_cfg, NULL);

	ov9285->sd.ctrl_handler = &ov9285->ctrls;

	if (ov9285->ctrls.error) {
		dev_err(ov9285->dev, "Control initialization error %d\n",
			ov9285->ctrls.error);
		rtn = ov9285->ctrls.error;
	}

	return rtn;
}

static int ov9285_parse_mclk(struct ov9285 *ov9285)
{
	int rtn = 0;
	u32 xclk_freq;

	ov9285->xclk = devm_clk_get(ov9285->dev, "mclk");
	if (IS_ERR(ov9285->xclk)) {
		dev_err(ov9285->dev, "Could not get xclk");
		rtn = PTR_ERR(ov9285->xclk);
		goto err_return;
	}

	rtn = fwnode_property_read_u32(dev_fwnode(ov9285->dev), "clock-frequency",
				       &xclk_freq);
	if (rtn) {
		dev_err(ov9285->dev, "Could not get xclk frequency\n");
		goto err_return;
	}

	if (xclk_freq != OV9285_MCLK) {
		dev_err(ov9285->dev, "External clock frequency %u is not supported\n",
			xclk_freq);
		rtn = -EINVAL;
		goto err_return;
	}

	rtn = clk_set_rate(ov9285->xclk, xclk_freq);
	if (rtn) {
		dev_err(ov9285->dev, "Could not set xclk frequency\n");
		goto err_return;
	}

err_return:
	return rtn;
}

static int ov9285_parse_power(struct ov9285 *ov9285)
{
	ov9285->pwr_gpio = devm_gpiod_get_optional(ov9285->dev, "pwr", GPIOD_OUT_LOW);
	ov9285->pwdn_gpio = devm_gpiod_get_optional(ov9285->dev, "pwdn", GPIOD_OUT_LOW);
	ov9285->reset_gpio = devm_gpiod_get_optional(ov9285->dev, "reset", GPIOD_OUT_LOW);
	ov9285->vddcam = devm_regulator_get(ov9285->dev,"vddcam");
	ov9285->viocam = devm_regulator_get(ov9285->dev,"viocam");

	return 0;
}

static int ov9285_parse_endpoint(struct ov9285 *ov9285)
{
	int rtn = 0;
	struct fwnode_handle *endpoint;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(ov9285->dev), NULL);
	if (!endpoint) {
		dev_err(ov9285->dev, "Endpoint node not found\n");
		return -EINVAL;
	}

	rtn = v4l2_fwnode_endpoint_alloc_parse(endpoint, &ov9285->ep);
	fwnode_handle_put(endpoint);
	if (rtn) {
		dev_err(ov9285->dev, "Parsing endpoint node failed\n");
		rtn = -EINVAL;
		goto err_return;
	}

	if (!ov9285->ep.nr_of_link_frequencies) {
		dev_err(ov9285->dev, "link-frequency property not found in DT\n");
		rtn = -EINVAL;
		goto err_free;
	}

	if (ov9285->ep.link_frequencies[0] != OV9285_DEFAULT_LINK_FREQ) {
		dev_err(ov9285->dev, "Unsupported link frequency\n");
		rtn = -EINVAL;
		goto err_free;
	}

	/* Only CSI2 is supported for now */
	if (ov9285->ep.bus_type != V4L2_MBUS_CSI2_DPHY) {
		dev_err(ov9285->dev, "Unsupported bus type, should be CSI2\n");
		rtn = -EINVAL;
		goto err_free;
	}

	return rtn;

err_free:
	v4l2_fwnode_endpoint_free(&ov9285->ep);
err_return:
	return rtn;
}

static int ov9285_register_subdev(struct ov9285 *ov9285)
{
	int rtn = 0;

	v4l2_i2c_subdev_init(&ov9285->sd, ov9285->client, &ov9285_subdev_ops);

	ov9285->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ov9285->sd.dev = &ov9285->client->dev;
	ov9285->sd.entity.ops = &ov9285_subdev_entity_ops;
	ov9285->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	ov9285->pad.flags = MEDIA_PAD_FL_SOURCE;
	rtn = media_entity_pads_init(&ov9285->sd.entity, 1, &ov9285->pad);
	if (rtn < 0) {
		dev_err(ov9285->dev, "Could not register media entity\n");
		goto err_return;
	}

	rtn = v4l2_async_register_subdev(&ov9285->sd);
	if (rtn < 0) {
		dev_err(ov9285->dev, "Could not register v4l2 device\n");
		goto err_return;
	}

err_return:
	return rtn;
}

static int ov9285_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ov9285 *ov9285;
	int ret = -EINVAL;

	ov9285 = devm_kzalloc(dev, sizeof(*ov9285), GFP_KERNEL);
	if (!ov9285)
		return -ENOMEM;

	ov9285->dev = dev;
	ov9285->client = client;
	ov9285->regmap = devm_regmap_init_i2c(client, &ov9285_regmap_config);
	if (IS_ERR(ov9285->regmap)) {
		dev_err(dev, "Unable to initialize I2C\n");
		return -ENODEV;
	}

	ov9285->master = of_property_read_bool(client->dev.of_node, "master");

	ret = ov9285_parse_endpoint(ov9285);
	if (ret) {
		dev_err(ov9285->dev, "Error parse endpoint\n");
		goto return_err;
	}

	/* Set default mode to max resolution */
	ov9285->current_mode = &ov9285_modes[0];

	/* get system clock (xclk) */
	ret = ov9285_parse_mclk(ov9285);
	if (ret) {
		dev_err(ov9285->dev, "Error parse mclk\n");
		goto free_err;
	}

	ret = ov9285_parse_power(ov9285);
	if (ret) {
		dev_err(ov9285->dev, "Error parse power ctrls\n");
		goto free_err;
	}

	mutex_init(&ov9285->lock);

	/* Power on the device to match runtime PM state below */
	ret = ov9285_power_on(ov9285);
	if (ret < 0) {
		dev_err(dev, "Could not power on the device\n");
		goto free_err;
	}

	ret = ov9285_get_id(ov9285);
	if (ret) {
		dev_err(dev, "Could not get id\n");
		ov9285_power_off(ov9285);
		goto free_err;
	}

	ret = ov9285_ctrls_init(ov9285);
	if (ret) {
		dev_err(ov9285->dev, "Error ctrls init\n");
		goto free_ctrl;
	}

	ret = ov9285_register_subdev(ov9285);
	if (ret) {
		dev_err(ov9285->dev, "Error register subdev\n");
		goto free_entity;
	}

	v4l2_fwnode_endpoint_free(&ov9285->ep);

	return 0;

free_entity:
	media_entity_cleanup(&ov9285->sd.entity);
free_ctrl:
	v4l2_ctrl_handler_free(&ov9285->ctrls);
	mutex_destroy(&ov9285->lock);
free_err:
	v4l2_fwnode_endpoint_free(&ov9285->ep);
return_err:
	return ret;
}

static int ov9285_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov9285 *ov9285 = to_ov9285(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	mutex_destroy(&ov9285->lock);

	ov9285_power_off(ov9285);
	devm_clk_put(ov9285->dev, ov9285->xclk);

	if(ov9285->vddcam)
		devm_regulator_put(ov9285->vddcam);

	if(ov9285->viocam)
		devm_regulator_put(ov9285->viocam);

	if (ov9285->pwr_gpio)
		devm_gpiod_put(ov9285->dev, ov9285->pwr_gpio);

	if (ov9285->pwdn_gpio)
		devm_gpiod_put(ov9285->dev, ov9285->pwdn_gpio);

	if (ov9285->reset_gpio)
		devm_gpiod_put(ov9285->dev, ov9285->reset_gpio);

	return 0;
}

static const struct of_device_id ov9285_of_match[] = {
	{ .compatible = "omnivision, ov9285" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ov9285_of_match);

static struct i2c_driver ov9285_i2c_driver = {
	.probe_new  = ov9285_probe,
	.remove = ov9285_remove,
	.driver = {
		.name  = "ov9285",
		.pm = &ov9285_pm_ops,
		.of_match_table = of_match_ptr(ov9285_of_match),
	},
};

module_i2c_driver(ov9285_i2c_driver);

MODULE_DESCRIPTION("Omnivision OV9285 CMOS Image Sensor Driver");
MODULE_AUTHOR("keke.li");
MODULE_AUTHOR("keke.li <keke.li@amlogic.com>");
MODULE_LICENSE("GPL v2");
