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

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/property.h>

#include <media/v4l2-common.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mc.h>

#include "aml_cam.h"

static int cam_subdevs_register(struct cam_device *cam_dev)
{
	int rtn;

	rtn = aml_csiphy_subdev_register(&cam_dev->csiphy_dev);
	if (rtn)
		return rtn;

	rtn = aml_adap_subdev_register(&cam_dev->adap_dev);
	if (rtn)
		return rtn;

	switch (cam_dev->index) {
	case AML_CAM_0:
	case AML_CAM_1:
	case AML_CAM_2:
	case AML_CAM_3:
		aml_pattern_subdev_register(&cam_dev->pattern);
		rtn = aml_isp_subdev_register(&cam_dev->isp_dev);
	break;
	case AML_CAM_4:
		rtn = 0;
	break;
	default:
		rtn = -EINVAL;
		dev_err(cam_dev->dev, "Error camera index: %u\n", cam_dev->index);
	break;
	}

	return rtn;
}

static int cam_videos_register(struct cam_device *cam_dev)
{
	int rtn = 0;

	switch (cam_dev->index) {
	case AML_CAM_0:
	case AML_CAM_1:
	case AML_CAM_2:
	case AML_CAM_3:
		rtn = aml_isp_video_register(&cam_dev->isp_dev);
	break;
	case AML_CAM_4:
		rtn = aml_adap_video_register(&cam_dev->adap_dev);
	break;
	default:
		rtn = -EINVAL;
		dev_err(cam_dev->dev, "Error camera index: %u\n", cam_dev->index);
	break;
	}

	return rtn;
}

static void cam_subdevs_unregister(struct cam_device *cam_dev)
{
	aml_csiphy_subdev_unregister(&cam_dev->csiphy_dev);
	aml_adap_subdev_unregister(&cam_dev->adap_dev);

	switch (cam_dev->index) {
	case AML_CAM_0:
	case AML_CAM_1:
	case AML_CAM_2:
	case AML_CAM_3:
		aml_isp_subdev_unregister(&cam_dev->isp_dev);
		aml_pattern_subdev_unregister(&cam_dev->pattern);
	break;
	case AML_CAM_4:
	break;
	default:
		dev_err(cam_dev->dev, "Error camera index: %u\n", cam_dev->index);
	break;
	}
}

static void cam_videos_unregister(struct cam_device *cam_dev)
{
	switch (cam_dev->index) {
	case AML_CAM_0:
	case AML_CAM_1:
	case AML_CAM_2:
	case AML_CAM_3:
		aml_isp_video_unregister(&cam_dev->isp_dev);
	break;
	case AML_CAM_4:
		aml_adap_video_unregister(&cam_dev->adap_dev);
	break;
	default:
		dev_err(cam_dev->dev, "Error camera index: %u\n", cam_dev->index);
	break;
	}
}

static int cam_create_csiphy_adap_links(struct cam_device *cam_dev)
{
	int rtn = 0;
	u32 flags = 0;
	struct media_entity *csiphy = &cam_dev->csiphy_dev.sd.entity;
	struct media_entity *adap = &cam_dev->adap_dev.sd.entity;

	flags = MEDIA_LNK_FL_ENABLED;
	rtn = media_create_pad_link(csiphy, AML_CSIPHY_PAD_SRC,
				adap, AML_ADAP_PAD_SINK, flags);
	if (rtn) {
		dev_err(cam_dev->dev, "Failed to link %s->%s entity\n",
			csiphy->name, adap->name);
		return rtn;
	}

	return rtn;
}

static int cam_create_adap_isp_links(struct cam_device *cam_dev)
{
	int rtn = 0;
	u32 flags = 0;
	struct media_entity *adap = &cam_dev->adap_dev.sd.entity;
	struct media_entity *isp = &cam_dev->isp_dev.sd.entity;

	flags = MEDIA_LNK_FL_ENABLED;
	rtn = media_create_pad_link(adap, AML_ADAP_PAD_SRC,
				isp, AML_ISP_PAD_SINK_VIDEO, flags);
	if (rtn) {
		dev_err(cam_dev->dev, "Failed to link %s->%s entity\n",
			adap->name, isp->name);
		return rtn;
	}

	return rtn;
}

static int cam_create_pattern_isp_links(struct cam_device *cam_dev)
{
	int rtn = 0;
	u32 flags = 0;
	struct media_entity *pattern = &cam_dev->pattern.sd.entity;
	struct media_entity *isp = &cam_dev->isp_dev.sd.entity;

	flags = 0;//MEDIA_LNK_FL_ENABLED;
	rtn = media_create_pad_link(pattern, AML_PATTERN_PAD_SRC,
				isp, AML_ISP_PAD_SINK_PATTERN, flags);
	if (rtn) {
		dev_err(cam_dev->dev, "Failed to link %s->%s entity\n",
			pattern->name, isp->name);
		return rtn;
	}

	return rtn;
}

static int cam_create_isp_video_links(struct cam_device *cam_dev)
{
	int rtn = 0;
	u32 flags = 0, i = 0;
	struct media_entity *video;
	struct media_entity *isp = &cam_dev->isp_dev.sd.entity;

	flags = MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED;
	for (i = 2; i < AML_ISP_PAD_MAX; i++) {
		video = &cam_dev->isp_dev.video[i - 2].vdev.entity;
		if (i <= AML_ISP_PAD_SINK_PARAM)
			rtn = media_create_pad_link(video, 0, isp, i, flags);
		else
			rtn = media_create_pad_link(isp, i, video, 0, flags);

		if (rtn) {
			dev_err(cam_dev->dev, "Failed to link %s->%s entity\n",
				video->name, isp->name);
			break;
		}
	}

	return rtn;
}

static int cam_create_adap_video_links(struct cam_device *cam_dev)
{
	int rtn = 0;
	u32 flags = 0, i = 0;
	struct media_entity *video;
	struct media_entity *adap = &cam_dev->adap_dev.sd.entity;

	flags = MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED;
	video = &cam_dev->adap_dev.video[AML_ADAP_STREAM_TOF].vdev.entity;

	rtn = media_create_pad_link(adap, AML_ADAP_PAD_SRC, video, 0, flags);
	if (rtn) {
		dev_err(cam_dev->dev, "Failed to link %s->%s entity\n",
				video->name, adap->name);
		return rtn;
	}

	return rtn;
}

static int cam_create_links(struct cam_device *cam_dev)
{
	int rtn = 0;
	u32 flags = 0, i = 0;
	struct media_entity *video;
	struct media_entity *csiphy = &cam_dev->csiphy_dev.sd.entity;
	struct media_entity *adap = &cam_dev->adap_dev.sd.entity;
	struct media_entity *isp = &cam_dev->isp_dev.sd.entity;

	rtn = cam_create_csiphy_adap_links(cam_dev);
	if (rtn)
		return rtn;

	switch (cam_dev->index) {
	case AML_CAM_0:
	case AML_CAM_1:
	case AML_CAM_2:
	case AML_CAM_3:
		rtn = cam_create_adap_isp_links(cam_dev);
		if (rtn)
			break;

		rtn = cam_create_pattern_isp_links(cam_dev);
		if (rtn)
			break;

		rtn = cam_create_isp_video_links(cam_dev);
	break;
	case AML_CAM_4:
		rtn = cam_create_adap_video_links(cam_dev);
	break;
	default:
		rtn = -EINVAL;
		dev_err(cam_dev->dev, "Error camera index: %u\n", cam_dev->index);
	break;
	}

	return rtn;
}

static int cam_devnode_register(struct cam_device *cam_dev)
{
	int rtn = 0;

	rtn = cam_videos_register(cam_dev);
	if (rtn < 0) {
		dev_err(cam_dev->dev, "Failed to register video node: %d\n", rtn);
		goto error_return;
	}

	rtn = cam_create_links(cam_dev);
	if (rtn) {
		dev_err(cam_dev->dev, "Failed to create links: %d\n", rtn);
		goto error_video;
	}

	rtn = v4l2_device_register_subdev_nodes(&cam_dev->v4l2_dev);
	if (rtn < 0) {
		dev_err(cam_dev->dev, "Failed to register sd node: %d\n", rtn);
		goto error_video;
	}

	rtn = media_device_register(&cam_dev->media_dev);
	if (rtn) {
		dev_err(cam_dev->dev, "Failed to register media: %d\n", rtn);
		goto error_video;
	}

	return rtn;

error_video:
	cam_videos_unregister(cam_dev);
error_return:
	return rtn;
}

static void cam_devnode_unregister(struct cam_device *cam_dev)
{
	media_device_unregister(&cam_dev->media_dev);
	cam_videos_unregister(cam_dev);
}

static int cam_async_notifier_bound(struct v4l2_async_notifier *async,
				       struct v4l2_subdev *subdev,
				       struct v4l2_async_subdev *asd)
{
	struct csiphy_async_subdev *c_asd =
			container_of(asd, struct csiphy_async_subdev, asd);

	subdev->host_priv = c_asd;

	return 0;
}

void cam_async_notifier_unbind(struct v4l2_async_notifier *async,
				struct v4l2_subdev *subdev,
				struct v4l2_async_subdev *asd)
{
	return;
}

static int cam_async_notifier_complete(struct v4l2_async_notifier *async)
{
	int rtn = -1;
	unsigned int i;
	struct v4l2_subdev *sd;
	struct media_entity *sensor;
	struct media_entity *csiphy;
	struct csiphy_dev_t *csiphy_dev;
	struct cam_device *cam_dev = container_of(async, struct cam_device, notifier);
	struct v4l2_device *v4l2_dev = &cam_dev->v4l2_dev;

	list_for_each_entry(sd, &v4l2_dev->subdevs, list) {
		if (!sd->host_priv)
			continue;

		sensor = &sd->entity;
		csiphy_dev = &cam_dev->csiphy_dev;
		csiphy = &csiphy_dev->sd.entity;

		for (i = 0; i < sensor->num_pads; i++) {
			if (sensor->pads[i].flags & MEDIA_PAD_FL_SOURCE)
					break;
		}
		if (i == sensor->num_pads) {
			rtn = -EINVAL;
			dev_err(cam_dev->dev, "No source pad in sensor\n");
			goto error_return;
		}

		rtn = media_create_pad_link(sensor, i, csiphy,
				AML_CSIPHY_PAD_SINK, MEDIA_LNK_FL_ENABLED);
		if (rtn < 0) {
			dev_err(cam_dev->dev,
					"Failed to link %s->%s entities: %d\n",
					sensor->name, csiphy->name, rtn);
				goto error_return;
		}
	}
	rtn = cam_devnode_register(cam_dev);
	if (rtn) {
		dev_info(cam_dev->dev, "cam_devnode_register fail. ret %d\n", rtn);
	}

	dev_info(cam_dev->dev, "Success async notifier complete\n");

error_return:

	return rtn;

}

static const struct v4l2_async_notifier_operations cam_async_notifier_ops = {
	.bound = cam_async_notifier_bound,
	.unbind = cam_async_notifier_unbind,
	.complete = cam_async_notifier_complete,
};

static int cam_async_notifier_register(struct cam_device *cam_dev)
{
	int rtn = -1;
	struct v4l2_device *v4l2_dev = &cam_dev->v4l2_dev;
	struct v4l2_async_notifier *notifier = &cam_dev->notifier;

	if (list_empty(&notifier->asd_list)) {
		dev_err(cam_dev->dev, "Error input param\n");
		return -EINVAL;
	}

	notifier->ops = &cam_async_notifier_ops;

	rtn = v4l2_async_notifier_register(v4l2_dev, notifier);
	if (rtn)
		dev_err(cam_dev->dev, "Failed to notifier register: %d\n", rtn);

	return rtn;
}

static int cam_init_subdevices(struct cam_device *cam_dev)
{
	int rtn = -1;

	rtn = aml_csiphy_subdev_init(cam_dev);
	if (rtn) {
		dev_err(cam_dev->dev, "Failed to init csiphy subdev: %d\n", rtn);
		return rtn;
	}

	rtn = aml_adap_subdev_init(cam_dev);
	if (rtn) {
		dev_err(cam_dev->dev, "Failed to init adap subdev: %d\n", rtn);
		return rtn;
	}

	switch (cam_dev->index) {
	case AML_CAM_0:
	case AML_CAM_1:
	case AML_CAM_2:
	case AML_CAM_3:
		rtn = aml_isp_subdev_init(cam_dev);
		if (rtn) {
			dev_err(cam_dev->dev, "Failed to init isp subdev: %d\n", rtn);
			return rtn;
		}

		aml_pattern_subdev_init(cam_dev);
	break;
	case AML_CAM_4:
		rtn = 0;
	break;
	default:
		rtn = -EINVAL;
		dev_err(cam_dev->dev, "Error camera index: %u\n", cam_dev->index);
	break;
	}

	return rtn;
}

static void cam_deinit_subdevices(struct cam_device *cam_dev)
{
	switch (cam_dev->index) {
		case AML_CAM_0:
		case AML_CAM_1:
		case AML_CAM_2:
		case AML_CAM_3:
			aml_isp_subdev_deinit(cam_dev);
			aml_pattern_subdev_deinit(cam_dev);
		break;
		case AML_CAM_4:
		break;
		default:
			dev_err(cam_dev->dev, "Error camera index: %u\n", cam_dev->index);
		break;
	}

	aml_adap_subdev_deinit(cam_dev);
	aml_csiphy_subdev_deinit(cam_dev);
}

static int cam_link_notify(struct media_link *link, u32 flags,
			   unsigned int notification)
{
	pr_debug("amlcam: %s --> %s, flag %u\n",
		link->source->entity->name,
		link->sink->entity->name,
		flags);

	return 0;
}

const struct media_device_ops mdev_ops = {
	.link_notify = cam_link_notify,
};

static void cam_media_dev_init(struct cam_device *cam_dev)
{
	snprintf(cam_dev->media_dev.driver_name,
		sizeof(cam_dev->media_dev.driver_name),
		AML_CAM_DRIVER_NAME);
	snprintf(cam_dev->media_dev.model,
		sizeof(cam_dev->media_dev.model),
		AML_CAM_DRIVER_NAME"%u", cam_dev->index);
	snprintf(cam_dev->media_dev.bus_info,
		sizeof(cam_dev->media_dev.bus_info),
		AML_CAM_BUS_INFO);

	cam_dev->bus_info = cam_dev->media_dev.bus_info;
	cam_dev->media_dev.dev = cam_dev->dev;
	cam_dev->media_dev.ops = &mdev_ops;

	media_device_init(&cam_dev->media_dev);
}

static int cam_v4l2_dev_register(struct cam_device *cam_dev)
{
	struct v4l2_device *v4l2_dev;

	v4l2_dev = &cam_dev->v4l2_dev;
	v4l2_dev->mdev = &cam_dev->media_dev;

	snprintf(v4l2_dev->name, sizeof(v4l2_dev->name), AML_CAM_DRIVER_NAME);

	return v4l2_device_register(cam_dev->dev, v4l2_dev);;
}

static int cam_probe(struct platform_device *pdev)
{
	int rtn = 0;
	struct cam_device *cam_dev;
	struct device *dev = &pdev->dev;

	cam_dev = devm_kzalloc(dev, sizeof(*cam_dev), GFP_KERNEL);
	if (!cam_dev)
		return -ENOMEM;

	if (of_property_read_u32(dev->of_node, "index", &cam_dev->index)) {
		dev_err(dev, "Failed to read camera index\n");
		return -EINVAL;
	}

	dev_set_drvdata(dev, cam_dev);
	cam_dev->dev = dev;

	cam_media_dev_init(cam_dev);

	rtn = cam_init_subdevices(cam_dev);
	if (rtn)
		goto error_return;

	rtn = cam_v4l2_dev_register(cam_dev);
	if (rtn) {
		dev_err(dev, "Failed to v4l2 register: %d\n", rtn);
		goto error_init_subdevs;
	}

	rtn = cam_subdevs_register(cam_dev);
	if (rtn) {
		dev_err(dev, "Failed to register entity\n");
		goto error_v4l2_dev;
	}

	rtn = cam_async_notifier_register(cam_dev);
	if (rtn) {
		dev_err(cam_dev->dev,
			"Failed to register subdev notifier(%d)\n", rtn);
		goto error_subdevs_register;
	}

	return rtn;

error_subdevs_register:
	cam_subdevs_unregister(cam_dev);

error_v4l2_dev:
	v4l2_device_unregister(&cam_dev->v4l2_dev);

error_init_subdevs:
	cam_deinit_subdevices(cam_dev);

error_return:
	return rtn;
}

static int cam_remove(struct platform_device *pdev)
{
	struct cam_device *cam_dev = platform_get_drvdata(pdev);

	cam_devnode_unregister(cam_dev);

	v4l2_async_notifier_unregister(&cam_dev->notifier);

	cam_subdevs_unregister(cam_dev);

	v4l2_device_unregister(&cam_dev->v4l2_dev);

	cam_deinit_subdevices(cam_dev);

	dev_info(cam_dev->dev, "cam-%u remove finished\n", cam_dev->index);

	return 0;
}

static int cam_power_suspend(struct device *dev)
{
	struct cam_device *cam_dev;

	cam_dev = dev_get_drvdata(dev);

	switch (cam_dev->index) {
		case AML_CAM_0:
		case AML_CAM_1:
		case AML_CAM_2:
		case AML_CAM_3:
			csiphy_subdev_suspend(&cam_dev->csiphy_dev);
			isp_subdev_suspend(&cam_dev->isp_dev);
		break;
		case AML_CAM_4:
			csiphy_subdev_suspend(&cam_dev->csiphy_dev);
		break;
		default:
			dev_err(cam_dev->dev, "Error camera index: %u\n", cam_dev->index);
		break;
	}

	dev_info(dev, "suspend\n");

	return 0;
}

static int cam_power_resume(struct device *dev)
{
	struct cam_device *cam_dev;

	cam_dev = dev_get_drvdata(dev);

	switch (cam_dev->index) {
		case AML_CAM_0:
		case AML_CAM_1:
		case AML_CAM_2:
		case AML_CAM_3:
			csiphy_subdev_resume(&cam_dev->csiphy_dev);
			isp_subdev_resume(&cam_dev->isp_dev);
		break;
		case AML_CAM_4:
			csiphy_subdev_resume(&cam_dev->csiphy_dev);
		break;
		default:
			dev_err(cam_dev->dev, "Error camera index: %u\n", cam_dev->index);
		break;
	}

	dev_info(dev, "resume\n");

	return 0;
}

static const struct dev_pm_ops cam_pm_ops = {
	.suspend = cam_power_suspend,
	.resume = cam_power_resume,
	.runtime_suspend = cam_power_suspend,
	.runtime_resume = cam_power_resume,
};

static const struct of_device_id cam_of_table[] = {
	{.compatible = "amlogic, camera"},
	{ },
};

MODULE_DEVICE_TABLE(of, cam_of_table);

static struct platform_driver cam_driver = {
	.probe = cam_probe,
	.remove = cam_remove,
	.driver = {
		.name = "aml_camera",
		.pm = &cam_pm_ops,
		.of_match_table = cam_of_table,
	},
};

module_platform_driver(cam_driver);

MODULE_AUTHOR("Keke Li");
MODULE_DESCRIPTION("Amlogic Camera Driver");
MODULE_LICENSE("Dual BSD/GPL");
