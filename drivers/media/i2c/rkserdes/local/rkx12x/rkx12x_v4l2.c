// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip SerDes RKx12x Deserializer V4L2 driver
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 */
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/of_graph.h>
#include <linux/pm_runtime.h>
#include <linux/compat.h>
#include <linux/rk-camera-module.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>

#include "rkx12x_api.h"
#include "rkx12x_remote.h"

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

#define MIPI_PHY_FREQ_MHZ(x)		((x) * 1000000UL)

/* link freq = index * MIPI_PHY_FREQ_MHZ(50) */
static const s64 link_freq_items[] = {
	MIPI_PHY_FREQ_MHZ(0),   /* 0 */
	MIPI_PHY_FREQ_MHZ(50),  /* 1 */
	MIPI_PHY_FREQ_MHZ(100), /* 2 */
	MIPI_PHY_FREQ_MHZ(150), /* 3 */
	MIPI_PHY_FREQ_MHZ(200), /* 4 */
	MIPI_PHY_FREQ_MHZ(250), /* 5 */
	MIPI_PHY_FREQ_MHZ(300), /* 6 */
	MIPI_PHY_FREQ_MHZ(350), /* 7 */
	MIPI_PHY_FREQ_MHZ(400), /* 8 */
	MIPI_PHY_FREQ_MHZ(450), /* 9 */
	MIPI_PHY_FREQ_MHZ(500), /* 10 */
	MIPI_PHY_FREQ_MHZ(550), /* 11 */
	MIPI_PHY_FREQ_MHZ(600), /* 12 */
	MIPI_PHY_FREQ_MHZ(650), /* 13 */
	MIPI_PHY_FREQ_MHZ(700), /* 14 */
	MIPI_PHY_FREQ_MHZ(750), /* 15 */
};

static const char * const rkx12x_test_pattern_menu[] = {
	"Disabled"
};

static const struct rkx12x_mode rkx12x_def_mode = {
	.width = 1920,
	.height = 1080,
	.max_fps = {
		.numerator = 10000,
		.denominator = 300000,
	},
	.link_freq_idx = 10,
	.bus_fmt = MEDIA_BUS_FMT_SBGGR12_1X12,
	.bpp = 12,
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	.vc[PAD0] = 0,
#else
	.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
#endif /* LINUX_VERSION_CODE */
	/* crop rect */
	.crop_rect = {
		.left = 0,
		.width = 1920,
		.top = 0,
		.height = 1080,
	},
};

static struct rkmodule_csi_dphy_param rk3588_dcphy_param = {
	.vendor = PHY_VENDOR_SAMSUNG,
	.lp_vol_ref = 3,
	.lp_hys_sw = {3, 0, 3, 0},
	.lp_escclk_pol_sel = {1, 0, 1, 0},
	.skew_data_cal_clk = {0, 0, 0, 0},
	.clk_hs_term_sel = 2,
	.data_hs_term_sel = {2, 2, 2, 2},
	.reserved = {0},
};

static int rkx12x_support_mode_init(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	struct device_node *node = NULL;
	struct rkx12x_mode *mode = NULL;
	u32 value = 0, vc_array[PAD_MAX], crop_array[4];
	int ret = 0, i = 0, array_size = 0;

	dev_info(dev, "=== rkx12x support mode init ===\n");

	rkx12x->cfg_modes_num = 1;
	rkx12x->cur_mode = &rkx12x->supported_mode;
	mode = &rkx12x->supported_mode;

	// init using def mode
	memcpy(mode, &rkx12x_def_mode, sizeof(struct rkx12x_mode));

	node = of_get_child_by_name(dev->of_node, "support-mode-config");
	if (IS_ERR_OR_NULL(node)) {
		dev_info(dev, "no mode config node, using default config.\n");

		return 0;
	}

	if (!of_device_is_available(node)) {
		dev_info(dev, "%pOF is disabled, using default config.\n", node);

		of_node_put(node);

		return 0;
	}

	ret = of_property_read_u32(node, "sensor-width", &value);
	if (ret == 0) {
		dev_info(dev, "sensor-width property: %d\n", value);
		mode->width = value;
	}
	dev_info(dev, "support mode: width = %d\n", mode->width);

	ret = of_property_read_u32(node, "sensor-height", &value);
	if (ret == 0) {
		dev_info(dev, "sensor-height property: %d\n", value);
		mode->height = value;
	}
	dev_info(dev, "support mode: height = %d\n", mode->height);

	ret = of_property_read_u32(node, "bus-format", &value);
	if (ret == 0) {
		dev_info(dev, "bus-format property: %d\n", value);
		mode->bus_fmt = value;
	}
	dev_info(dev, "support mode: bus_fmt = 0x%x\n", mode->bus_fmt);

	ret = of_property_read_u32(node, "bpp", &value);
	if (ret == 0) {
		dev_info(dev, "bpp property: %d\n", value);
		mode->bpp = value;
	}
	dev_info(dev, "support mode: bpp = %d\n", mode->bpp);

	ret = of_property_read_u32(node, "max-fps-numerator", &value);
	if (ret == 0) {
		dev_info(dev, "max-fps-numerator property: %d\n", value);
		mode->max_fps.numerator = value;
	}
	dev_info(dev, "support mode: numerator = %d\n", mode->max_fps.numerator);

	ret = of_property_read_u32(node, "max-fps-denominator", &value);
	if (ret == 0) {
		dev_info(dev, "max-fps-denominator property: %d\n", value);
		mode->max_fps.denominator = value;
	}
	dev_info(dev, "support mode: denominator = %d\n", mode->max_fps.denominator);

	ret = of_property_read_u32(node, "link-freq-idx", &value);
	if (ret == 0) {
		dev_info(dev, "link-freq-idx property: %d\n", value);
		mode->link_freq_idx = value;
	}
	dev_info(dev, "support mode: link_freq_idx = %d\n", mode->link_freq_idx);

	ret = of_property_read_u32(node, "hts-def", &value);
	if (ret == 0) {
		dev_info(dev, "hts-def property: %d\n", value);
		mode->hts_def = value;
	}
	dev_info(dev, "support mode: hts_def = %d\n", mode->hts_def);

	ret = of_property_read_u32(node, "vts-def", &value);
	if (ret == 0) {
		dev_info(dev, "vts-def property: %d\n", value);
		mode->vts_def = value;
	}
	dev_info(dev, "support mode: vts_def = %d\n", mode->vts_def);

	ret = of_property_read_u32(node, "exp-def", &value);
	if (ret == 0) {
		dev_info(dev, "exp-def property: %d\n", value);
		mode->exp_def = value;
	}
	dev_info(dev, "support mode: exp_def = %d\n", mode->exp_def);

	array_size = of_property_read_variable_u32_array(node,
				"vc-array", vc_array, 1, PAD_MAX);
	if (array_size > 0) {
		if (array_size > PAD_MAX)
			array_size = PAD_MAX;

		for (i = 0; i < array_size; i++) {
			dev_info(dev, "vc-array[%d] property: 0x%x\n", i, vc_array[i]);
			mode->vc[i] = vc_array[i];
		}
	} else {
		/* default vc config */
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		for (i = 0; i < PAD_MAX; i++)
			mode->vc[i] = i;
#else
		switch (PAD_MAX) {
		case 4:
			mode->vc[3] = V4L2_MBUS_CSI2_CHANNEL_3;
			fallthrough;
		case 3:
			mode->vc[2] = V4L2_MBUS_CSI2_CHANNEL_2;
			fallthrough;
		case 2:
			mode->vc[1] = V4L2_MBUS_CSI2_CHANNEL_1;
			fallthrough;
		case 1:
		default:
			mode->vc[0] = V4L2_MBUS_CSI2_CHANNEL_0;
			break;
		}
#endif
	}

	for (i = 0; i < PAD_MAX; i++)
		dev_info(dev, "support mode: vc[%d] = 0x%x\n", i, mode->vc[i]);

	/* crop rect */
	array_size = of_property_read_variable_u32_array(node,
				"crop-rect", crop_array, 1, 4);
	if (array_size == 4) {
		/* [left, top, width, height] */
		for (i = 0; i < array_size; i++)
			dev_info(dev, "crop-rect[%d] property: %d\n", i, crop_array[i]);

		mode->crop_rect.left = crop_array[0];
		mode->crop_rect.top = crop_array[1];
		mode->crop_rect.width = crop_array[2];
		mode->crop_rect.height = crop_array[3];
	} else {
		mode->crop_rect.left = 0;
		mode->crop_rect.width = mode->width;
		mode->crop_rect.top = 0;
		mode->crop_rect.height = mode->height;
	}
	dev_info(dev, "support mode crop rect: [ left = %d, top = %d, width = %d, height = %d ]\n",
						mode->crop_rect.left, mode->crop_rect.top,
						mode->crop_rect.width, mode->crop_rect.height);

	of_node_put(node);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static int rkx12x_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);
#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->state, 0);
#else
	struct v4l2_mbus_framefmt *try_fmt =
		v4l2_subdev_get_try_format(sd, fh->pad, 0);
#endif
	const struct rkx12x_mode *def_mode = &rkx12x->supported_mode;

	mutex_lock(&rkx12x->mutex);

	/* Initialize try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&rkx12x->mutex);
	/* No crop or compose */

	return 0;
}
#endif

static int rkx12x_s_power(struct v4l2_subdev *sd, int on)
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);
	struct i2c_client *client = rkx12x->client;
	int ret = 0;

	mutex_lock(&rkx12x->mutex);

	/* If the power state is not modified - no work to do. */
	if (rkx12x->power_on == !!on)
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

		rkx12x->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		rkx12x->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&rkx12x->mutex);

	return ret;
}

static void rkx12x_get_module_inf(struct rkx12x *rkx12x,
				struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, rkx12x->sensor_name, sizeof(inf->base.sensor));
	strscpy(inf->base.module, rkx12x->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, rkx12x->len_name, sizeof(inf->base.lens));
}

static void rkx12x_get_vicap_rst_inf(struct rkx12x *rkx12x,
				struct rkmodule_vicap_reset_info *rst_info)
{
	struct i2c_client *client = rkx12x->client;

	rst_info->is_reset = rkx12x->hot_plug;
	rkx12x->hot_plug = false;
	rst_info->src = RKCIF_RESET_SRC_ERR_HOTPLUG;

	dev_info(&client->dev, "%s: rst_info->is_reset:%d.\n",
		__func__, rst_info->is_reset);
}

static void rkx12x_set_vicap_rst_inf(struct rkx12x *rkx12x,
				struct rkmodule_vicap_reset_info rst_info)
{
	rkx12x->is_reset = rst_info.is_reset;
}

static long rkx12x_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);
	struct rkmodule_csi_dphy_param *dphy_param;
	long ret = 0;

	dev_dbg(&rkx12x->client->dev, "ioctl cmd = 0x%08x\n", cmd);

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		rkx12x_get_module_inf(rkx12x, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_VICAP_RST_INFO:
		rkx12x_get_vicap_rst_inf(rkx12x,
			(struct rkmodule_vicap_reset_info *)arg);
		break;
	case RKMODULE_SET_VICAP_RST_INFO:
		rkx12x_set_vicap_rst_inf(rkx12x,
			*(struct rkmodule_vicap_reset_info *)arg);
		break;
	case RKMODULE_SET_CSI_DPHY_PARAM:
		dphy_param = (struct rkmodule_csi_dphy_param *)arg;
		rk3588_dcphy_param = *dphy_param;
		dev_info(&rkx12x->client->dev, "set dcphy parameter\n");
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		dphy_param = (struct rkmodule_csi_dphy_param *)arg;
		*dphy_param = rk3588_dcphy_param;
		dev_info(&rkx12x->client->dev, "get dcphy parameter\n");
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long rkx12x_compat_ioctl32(struct v4l2_subdev *sd, unsigned int cmd,
					unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_vicap_reset_info *vicap_rst_inf;
	struct rkmodule_csi_dphy_param *dphy_param;
	long ret = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = rkx12x_ioctl(sd, cmd, inf);
		if (!ret) {
			ret = copy_to_user(up, inf, sizeof(*inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_GET_VICAP_RST_INFO:
		vicap_rst_inf = kzalloc(sizeof(*vicap_rst_inf), GFP_KERNEL);
		if (!vicap_rst_inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = rkx12x_ioctl(sd, cmd, vicap_rst_inf);
		if (!ret) {
			ret = copy_to_user(up, vicap_rst_inf, sizeof(*vicap_rst_inf));
			if (ret)
				ret = -EFAULT;
		}
		kfree(vicap_rst_inf);
		break;
	case RKMODULE_SET_VICAP_RST_INFO:
		vicap_rst_inf = kzalloc(sizeof(*vicap_rst_inf), GFP_KERNEL);
		if (!vicap_rst_inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(vicap_rst_inf, up, sizeof(*vicap_rst_inf));
		if (!ret)
			ret = rkx12x_ioctl(sd, cmd, vicap_rst_inf);
		else
			ret = -EFAULT;
		kfree(vicap_rst_inf);
		break;
	case RKMODULE_SET_CSI_DPHY_PARAM:
		dphy_param = kzalloc(sizeof(*dphy_param), GFP_KERNEL);
		if (!dphy_param) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(dphy_param, up, sizeof(*dphy_param));
		if (!ret)
			ret = rkx12x_ioctl(sd, cmd, dphy_param);
		else
			ret = -EFAULT;
		kfree(dphy_param);
		break;
	case RKMODULE_GET_CSI_DPHY_PARAM:
		dphy_param = kzalloc(sizeof(*dphy_param), GFP_KERNEL);
		if (!dphy_param) {
			ret = -ENOMEM;
			return ret;
		}

		ret = rkx12x_ioctl(sd, cmd, dphy_param);
		if (!ret) {
			ret = copy_to_user(up, dphy_param, sizeof(*dphy_param));
			if (ret)
				ret = -EFAULT;
		}
		kfree(dphy_param);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif /* CONFIG_COMPAT */

static int __rkx12x_start_stream(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	s64 link_freq_hz = 0, dvptx_pclk_hz = 0;
	int ret = 0;

	// remote devices power on
	ret = rkx12x_remote_devices_s_power(rkx12x, 1);
	if (ret) {
		dev_err(dev, "remote devices power on error\n");
		return ret;
	}

	dev_info(dev, "%s\n", __func__);

	ret = rkx12x_link_preinit(rkx12x);
	if (ret) {
		dev_err(dev, "rkx12x link preinit error\n");
		return ret;
	}

	ret = rkx12x_module_hw_init(rkx12x);
	if (ret) {
		dev_err(dev, "rkx12x module hw init error\n");
		return ret;
	}

	link_freq_hz = link_freq_items[rkx12x->cur_mode->link_freq_idx];
	if (rkx12x->stream_interface == RKX12X_STREAM_INTERFACE_MIPI) {
		// mipi txphy pll setting
		dev_info(dev, "csi: txphy pll set freq = %lld\n", link_freq_hz);
		rkx12x_txphy_csi_enable(&rkx12x->txphy, link_freq_hz);
	}

	if (rkx12x->stream_interface == RKX12X_STREAM_INTERFACE_DVP) {
		// dvptx pclk
		if (rkx12x->dvptx.pclk_freq == 0)
			dvptx_pclk_hz = link_freq_hz;
		else
			dvptx_pclk_hz = rkx12x->dvptx.pclk_freq;

		dev_info(dev, "dvp: dvptx pclk set freq = %lld\n", dvptx_pclk_hz);
		rkx12x_hwclk_set_rate(&rkx12x->hwclk,
				RKX120_CPS_PCLKOUT_DVPTX, dvptx_pclk_hz);
	}

	// remote devices start stream
	ret = rkx12x_remote_devices_s_stream(rkx12x, 1);
	if (ret) {
		dev_err(dev, "remote devices start stream error\n");
		return ret;
	}

	/* In case these controls are set before streaming */
	ret = __v4l2_ctrl_handler_setup(&rkx12x->ctrl_handler);
	if (ret) {
		dev_err(dev, "%s: setup v4l2 control handler error\n", __func__);
		return ret;
	}

	if (rkx12x->state_irq > 0) {
		dev_info(dev, "state irq enable\n");
		enable_irq(rkx12x->state_irq);
	}

	return 0;
}

static int __rkx12x_stop_stream(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	int ret = 0;

	if (rkx12x->state_irq > 0) {
		dev_info(dev, "state irq disable\n");
		disable_irq(rkx12x->state_irq);
	}

	// remote devices stop stream
	ret |= rkx12x_remote_devices_s_stream(rkx12x, 0);

	// remote devices power off
	ret |= rkx12x_remote_devices_s_power(rkx12x, 0);

	if (ret) {
		dev_err(dev, "stop stream error\n");
		return ret;
	}

	return 0;
}

static int rkx12x_s_stream(struct v4l2_subdev *sd, int on)
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);
	struct i2c_client *client = rkx12x->client;
	int ret = 0;

	dev_info(&client->dev, "%s: on: %d, %dx%d@%d\n", __func__, on,
		rkx12x->cur_mode->width, rkx12x->cur_mode->height,
		DIV_ROUND_CLOSEST(rkx12x->cur_mode->max_fps.denominator,
				rkx12x->cur_mode->max_fps.numerator));

	mutex_lock(&rkx12x->mutex);
	on = !!on;
	if (on == rkx12x->streaming)
		goto unlock_and_return;

	if (on) {
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE
		ret = pm_runtime_resume_and_get(&client->dev);
#else
		ret = pm_runtime_get_sync(&client->dev);
#endif
		if (ret < 0)
			goto unlock_and_return;

		ret = __rkx12x_start_stream(rkx12x);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put_sync(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__rkx12x_stop_stream(rkx12x);
		pm_runtime_mark_last_busy(&client->dev);
		pm_runtime_put_autosuspend(&client->dev);
	}

	rkx12x->streaming = on;

unlock_and_return:
	mutex_unlock(&rkx12x->mutex);

	return ret;
}

static int rkx12x_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *fi)
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);
	const struct rkx12x_mode *mode = rkx12x->cur_mode;

	mutex_lock(&rkx12x->mutex);
	fi->interval = mode->max_fps;
	mutex_unlock(&rkx12x->mutex);

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int rkx12x_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_mbus_code_enum *code)
#else
static int rkx12x_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);
	const struct rkx12x_mode *mode = rkx12x->cur_mode;

	if (code->index != 0)
		return -EINVAL;
	code->code = mode->bus_fmt;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int rkx12x_enum_frame_sizes(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_size_enum *fse)
#else
static int rkx12x_enum_frame_sizes(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_frame_size_enum *fse)
#endif
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);

	if (fse->index >= rkx12x->cfg_modes_num)
		return -EINVAL;

	if (fse->code != rkx12x->supported_mode.bus_fmt)
		return -EINVAL;

	fse->min_width  = rkx12x->supported_mode.width;
	fse->max_width  = rkx12x->supported_mode.width;
	fse->max_height = rkx12x->supported_mode.height;
	fse->min_height = rkx12x->supported_mode.height;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int rkx12x_enum_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_frame_interval_enum *fie)
#else
static int rkx12x_enum_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_frame_interval_enum *fie)
#endif
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);

	if (fie->index >= rkx12x->cfg_modes_num)
		return -EINVAL;

	fie->code = rkx12x->supported_mode.bus_fmt;
	fie->width = rkx12x->supported_mode.width;
	fie->height = rkx12x->supported_mode.height;
	fie->interval = rkx12x->supported_mode.max_fps;

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int rkx12x_get_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *fmt)
#else
static int rkx12x_get_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);
	const struct rkx12x_mode *mode = rkx12x->cur_mode;

	mutex_lock(&rkx12x->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		fmt->format = *v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
	#else
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
	#endif
#else
		mutex_unlock(&rkx12x->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
		fmt->reserved[0] = mode->vc[fmt->pad];
	}
	mutex_unlock(&rkx12x->mutex);

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int rkx12x_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_state *sd_state,
			struct v4l2_subdev_format *fmt)
#else
static int rkx12x_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
#endif
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);
	struct device *dev = &rkx12x->client->dev;
	const struct rkx12x_mode *mode = NULL;
	u64 link_freq = 0, pixel_rate = 0;
	u8 data_lanes;

	mutex_lock(&rkx12x->mutex);

	mode = &rkx12x->supported_mode;

	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
		*v4l2_subdev_get_try_format(sd, sd_state, fmt->pad) = fmt->format;
	#else
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
	#endif
#else
		mutex_unlock(&rkx12x->mutex);
		return -ENOTTY;
#endif
	} else {
		if (rkx12x->streaming) {
			mutex_unlock(&rkx12x->mutex);
			return -EBUSY;
		}

		rkx12x->cur_mode = mode;

		__v4l2_ctrl_s_ctrl(rkx12x->link_freq, mode->link_freq_idx);

		/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
		link_freq = link_freq_items[mode->link_freq_idx];
		data_lanes = rkx12x->bus_cfg.bus.mipi_csi2.num_data_lanes;
		pixel_rate = (u32)link_freq / mode->bpp * 2 * data_lanes;
		__v4l2_ctrl_s_ctrl_int64(rkx12x->pixel_rate, pixel_rate);

		dev_info(dev, "mipi_freq_idx = %d, mipi_link_freq = %lld\n",
					mode->link_freq_idx, link_freq);
		dev_info(dev, "pixel_rate = %lld, bpp = %d\n",
					pixel_rate, mode->bpp);
	}

	mutex_unlock(&rkx12x->mutex);

	return 0;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int rkx12x_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
#else
static int rkx12x_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
#endif
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = rkx12x->cur_mode->crop_rect.left;
		sel->r.width = rkx12x->cur_mode->crop_rect.width;
		sel->r.top = rkx12x->cur_mode->crop_rect.top;
		sel->r.height = rkx12x->cur_mode->crop_rect.height;
		return 0;
	}

	return -EINVAL;
}

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
static int rkx12x_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_config *config)
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);

	if (rkx12x->bus_cfg.bus_type == V4L2_MBUS_CSI2_DPHY) {
		config->type = V4L2_MBUS_CSI2_DPHY;
		config->bus.mipi_csi2 = rkx12x->bus_cfg.bus.mipi_csi2;
	} else if (rkx12x->bus_cfg.bus_type == V4L2_MBUS_PARALLEL) {
		config->type = V4L2_MBUS_PARALLEL;
		config->bus.parallel = rkx12x->bus_cfg.bus.parallel;
	} else {
		v4l2_err(sd, "unknown mbus type\n");
	}

	return 0;
}
#elif KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static int rkx12x_g_mbus_config(struct v4l2_subdev *sd, unsigned int pad,
				struct v4l2_mbus_config *config)
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);
	u32 val = 0;
	const struct rkx12x_mode *mode = rkx12x->cur_mode;
	u8 data_lanes = rkx12x->bus_cfg.bus.mipi_csi2.num_data_lanes;
	int i = 0;

	if (rkx12x->bus_cfg.bus_type == V4L2_MBUS_CSI2_DPHY) {
		val |= V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
		val |= (1 << (data_lanes - 1));

		for (i = 0; i < PAD_MAX; i++)
			val |= (mode->vc[i] & V4L2_MBUS_CSI2_CHANNELS);

		config->type = V4L2_MBUS_CSI2_DPHY;
		config->flags = val;
	} else if (rkx12x->bus_cfg.bus_type == V4L2_MBUS_PARALLEL) {
		config->type = V4L2_MBUS_PARALLEL;
		config->flags = V4L2_MBUS_HSYNC_ACTIVE_HIGH |
				V4L2_MBUS_VSYNC_ACTIVE_LOW |
				V4L2_MBUS_PCLK_SAMPLE_RISING;
	} else {
		v4l2_err(sd, "unknown mbus type\n");
	}

	return 0;
}
#else
static int rkx12x_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *config)
{
	struct rkx12x *rkx12x = v4l2_get_subdevdata(sd);
	u32 val = 0;
	const struct rkx12x_mode *mode = rkx12x->cur_mode;
	u8 data_lanes = rkx12x->bus_cfg.bus.mipi_csi2.num_data_lanes;
	int i = 0;

	if (rkx12x->bus_cfg.bus_type == V4L2_MBUS_CSI2_DPHY) {
		val |= V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
		val |= (1 << (data_lanes - 1));

		for (i = 0; i < PAD_MAX; i++)
			val |= (mode->vc[i] & V4L2_MBUS_CSI2_CHANNELS);

		config->type = V4L2_MBUS_CSI2;
		config->flags = val;
	} else if (rkx12x->bus_cfg.bus_type == V4L2_MBUS_PARALLEL) {
		config->type = V4L2_MBUS_PARALLEL;
		config->flags = V4L2_MBUS_HSYNC_ACTIVE_HIGH |
				V4L2_MBUS_VSYNC_ACTIVE_LOW |
				V4L2_MBUS_PCLK_SAMPLE_RISING;
	} else {
		v4l2_err(sd, "unknown mbus type\n");
	}

	return 0;
}
#endif /* LINUX_VERSION_CODE */

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops rkx12x_internal_ops = {
	.open = rkx12x_open,
};
#endif

static const struct v4l2_subdev_core_ops rkx12x_core_ops = {
	.s_power = rkx12x_s_power,
	.ioctl = rkx12x_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = rkx12x_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops rkx12x_video_ops = {
	.s_stream = rkx12x_s_stream,
	.g_frame_interval = rkx12x_g_frame_interval,
#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
	.g_mbus_config = rkx12x_g_mbus_config,
#endif
};

static const struct v4l2_subdev_pad_ops rkx12x_pad_ops = {
	.enum_mbus_code = rkx12x_enum_mbus_code,
	.enum_frame_size = rkx12x_enum_frame_sizes,
	.enum_frame_interval = rkx12x_enum_frame_interval,
	.get_fmt = rkx12x_get_fmt,
	.set_fmt = rkx12x_set_fmt,
	.get_selection = rkx12x_get_selection,
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	.get_mbus_config = rkx12x_g_mbus_config,
#endif
};

static const struct v4l2_subdev_ops rkx12x_subdev_ops = {
	.core = &rkx12x_core_ops,
	.video = &rkx12x_video_ops,
	.pad = &rkx12x_pad_ops,
};

static int rkx12x_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct rkx12x *rkx12x = container_of(ctrl->handler,
					struct rkx12x, ctrl_handler);
	struct i2c_client *client = rkx12x->client;
	int ret = 0;

	if (pm_runtime_get(&client->dev) <= 0)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_info(&client->dev, "%s: set exposure = 0x%x\n",
				__func__, ctrl->val);
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_info(&client->dev, "%s: set analogue gain = 0x%x\n",
				__func__, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		dev_info(&client->dev, "%s: set v_blank = 0x%x\n",
				__func__, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		dev_info(&client->dev, "%s: set test pattern = 0x%x\n",
				__func__, ctrl->val);
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
					__func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops rkx12x_ctrl_ops = {
	.s_ctrl = rkx12x_set_ctrl,
};

static int rkx12x_initialize_controls(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	const struct rkx12x_mode *mode;
	struct v4l2_ctrl_handler *handler;
	u64 link_freq = 0, pixel_rate = 0;
	u8 data_lanes;
	int ret = 0;

	handler = &rkx12x->ctrl_handler;

	ret = v4l2_ctrl_handler_init(handler, 3);
	if (ret)
		return ret;
	handler->lock = &rkx12x->mutex;

	mode = rkx12x->cur_mode;
	rkx12x->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				V4L2_CID_LINK_FREQ,
				ARRAY_SIZE(link_freq_items) - 1, 0,
				link_freq_items);
	__v4l2_ctrl_s_ctrl(rkx12x->link_freq, mode->link_freq_idx);

	link_freq = link_freq_items[mode->link_freq_idx];
	dev_info(dev, "link_freq_idx = %d, link_freq = %lld\n",
				mode->link_freq_idx, link_freq);

	/* pixel rate = link frequency * 2 * lanes / BITS_PER_SAMPLE */
	data_lanes = rkx12x->bus_cfg.bus.mipi_csi2.num_data_lanes;
	pixel_rate = (u32)link_freq / mode->bpp * 2 * data_lanes;
	rkx12x->pixel_rate =
		v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE, 0,
				pixel_rate, 1, pixel_rate);
	dev_info(dev, "pixel_rate = %lld, bpp = %d\n",
				pixel_rate, mode->bpp);

	/* test pattern */
	rkx12x->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
				&rkx12x_ctrl_ops, V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(rkx12x_test_pattern_menu) - 1,
				0, 0, rkx12x_test_pattern_menu);

	if (handler->error) {
		ret = handler->error;
		dev_err(dev, "Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	rkx12x->subdev.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static int rkx12x_bus_config_parse(struct rkx12x *rkx12x)
{
	struct device *dev = &rkx12x->client->dev;
	struct device_node *endpoint;
	u32 mipi_data_lanes, bus_type;
	int ret = 0;

	endpoint = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!endpoint) {
		dev_err(dev, "Failed to get endpoint\n");
		return -EINVAL;
	}

	/* bus_type default init */
	if (rkx12x->stream_interface == RKX12X_STREAM_INTERFACE_MIPI)
		rkx12x->bus_cfg.bus_type = V4L2_MBUS_CSI2_DPHY;
	else if (rkx12x->stream_interface == RKX12X_STREAM_INTERFACE_DVP)
		rkx12x->bus_cfg.bus_type = V4L2_MBUS_PARALLEL;
	else
		rkx12x->bus_cfg.bus_type = V4L2_MBUS_UNKNOWN;

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
		&rkx12x->bus_cfg);
	if (ret) {
		dev_err(dev, "Failed to get bus config\n");
		return -EINVAL;
	}

	bus_type = rkx12x->bus_cfg.bus_type;
	if (bus_type == V4L2_MBUS_CSI2_DPHY) {
		dev_info(dev, "bus type = 0x%x: dphy\n", bus_type);

		mipi_data_lanes = rkx12x->bus_cfg.bus.mipi_csi2.num_data_lanes;
		dev_info(dev, "bus data lanes = %d\n", mipi_data_lanes);
	} else if (bus_type == V4L2_MBUS_PARALLEL) {
		dev_info(dev, "bus type = 0x%x: parallel\n", bus_type);
	} else {
		dev_info(dev, "bus type = 0x%x: unknown\n", bus_type);
	}

	return 0;
}

int rkx12x_v4l2_subdev_init(struct rkx12x *rkx12x)
{
	struct i2c_client *client = rkx12x->client;
	struct device *dev = &client->dev;
	struct v4l2_subdev *sd = NULL;
	char facing[2];
	int ret = 0;

	dev_info(dev, "rkx12x v4l2 subdev init\n");

	rkx12x_bus_config_parse(rkx12x);

	rkx12x_support_mode_init(rkx12x);

	sd = &rkx12x->subdev;
	v4l2_i2c_subdev_init(sd, client, &rkx12x_subdev_ops);
	ret = rkx12x_initialize_controls(rkx12x);
	if (ret)
		goto err_free_handler;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &rkx12x_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif

#if defined(CONFIG_MEDIA_CONTROLLER)
	rkx12x->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &rkx12x->pad);
	if (ret < 0)
		goto err_free_handler;
#endif

	v4l2_set_subdevdata(sd, rkx12x);

	memset(facing, 0, sizeof(facing));
	if (strcmp(rkx12x->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 rkx12x->module_index, facing, rkx12x->sensor_name,
		 dev_name(sd->dev));

#if KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE
	ret = v4l2_async_register_subdev_sensor(sd);
#else
	ret = v4l2_async_register_subdev_sensor_common(sd);
#endif
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif

err_free_handler:
	v4l2_ctrl_handler_free(&rkx12x->ctrl_handler);

	return ret;
}
EXPORT_SYMBOL(rkx12x_v4l2_subdev_init);

void rkx12x_v4l2_subdev_deinit(struct rkx12x *rkx12x)
{
	struct v4l2_subdev *sd = &rkx12x->subdev;

	v4l2_async_unregister_subdev(sd);

#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif

	v4l2_ctrl_handler_free(&rkx12x->ctrl_handler);
}
EXPORT_SYMBOL(rkx12x_v4l2_subdev_deinit);
