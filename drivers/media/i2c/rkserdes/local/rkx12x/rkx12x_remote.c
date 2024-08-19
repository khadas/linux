// SPDX-License-Identifier: GPL-2.0
/*
 * Rockchip RKx12x Deserializer Remode Device Manage
 *
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 */
#include <linux/module.h>
#include <linux/of_graph.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>

#include "rkx12x_api.h"
#include "rkx12x_remote.h"

int rkx12x_remote_devices_s_power(struct rkx12x *rkx12x, int on)
{
	struct device *dev = &rkx12x->client->dev;
	struct rkx12x_linkrx *linkrx = &rkx12x->linkrx;
	struct device_node *remote_cam_node = NULL;
	struct i2c_client *remote_cam_client = NULL;
	struct v4l2_subdev *remote_cam_sd = NULL;
	int error = 0;

	dev_info(dev, "%s: lane id = %d, on = %d\n", __func__, linkrx->lane_id, on);

	remote_cam_node = linkrx->remote_cam_node;
	if (IS_ERR_OR_NULL(remote_cam_node)) {
		dev_info(dev, "remote camera node error\n");
		return -1;
	}
	remote_cam_client = of_find_i2c_device_by_node(remote_cam_node);
	if (IS_ERR_OR_NULL(remote_cam_client)) {
		dev_info(dev, "remote camera client error\n");
		return -1;
	}
	remote_cam_sd = i2c_get_clientdata(remote_cam_client);
	if (IS_ERR_OR_NULL(remote_cam_sd)) {
		dev_info(dev, "remote camera v4l2_subdev error\n");
		return -1;
	}

	dev_info(dev, "remote camera power = %d\n", on);
	error = v4l2_subdev_call(remote_cam_sd, core, s_power, on);
	if (error < 0) {
		dev_err(dev, "remote camera power error = %d\n", error);
		return -1;
	}

	return 0;
}

int rkx12x_remote_devices_s_stream(struct rkx12x *rkx12x, int enable)
{
	struct device *dev = &rkx12x->client->dev;
	struct rkx12x_linkrx *linkrx = &rkx12x->linkrx;
	struct device_node *remote_cam_node = NULL;
	struct i2c_client *remote_cam_client = NULL;
	struct v4l2_subdev *remote_cam_sd = NULL;
	int error = 0;

	dev_info(dev, "%s: lane id = %d, enable = %d\n", __func__, linkrx->lane_id, enable);

	if (linkrx->link_lock == 0) {
		dev_info(dev, "lane id = %d link lock error\n", linkrx->lane_id);
		return -1;
	}

	remote_cam_node = linkrx->remote_cam_node;
	if (IS_ERR_OR_NULL(remote_cam_node)) {
		dev_info(dev, "remote camera node error\n");
		return -1;
	}
	remote_cam_client = of_find_i2c_device_by_node(remote_cam_node);
	if (IS_ERR_OR_NULL(remote_cam_client)) {
		dev_info(dev, "remote camera client error\n");
		return -1;
	}
	remote_cam_sd = i2c_get_clientdata(remote_cam_client);
	if (IS_ERR_OR_NULL(remote_cam_sd)) {
		dev_info(dev, "remote camera v4l2_subdev error\n");
		return -1;
	}

	dev_info(dev, "remote camera s_stream = %d\n", enable);
	error = v4l2_subdev_call(remote_cam_sd, video, s_stream, enable);
	if (error < 0) {
		dev_err(dev, "remote camera s_stream error = %d\n", error);
		return -1;
	}

	return 0;
}
