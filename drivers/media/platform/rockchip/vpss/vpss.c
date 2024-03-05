// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/pm_runtime.h>
#include <linux/videodev2.h>
#include <media/media-entity.h>
#include <media/v4l2-event.h>

#include "dev.h"
#include "regs.h"

static const struct vpsssd_fmt rkvpss_formats[] = {
	{
		.mbus_code = MEDIA_BUS_FMT_YUYV8_2X8,
		.fourcc = V4L2_PIX_FMT_NV16,
	},
};

static int rkvpss_subdev_link_setup(struct media_entity *entity,
				    const struct media_pad *local,
				    const struct media_pad *remote,
				    u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct rkvpss_subdev *vpss_sdev;
	struct rkvpss_device *dev;
	struct rkvpss_stream_vdev *vdev;
	struct rkvpss_stream *stream = NULL;

	if (local->index != RKVPSS_PAD_SINK &&
	    local->index != RKVPSS_PAD_SOURCE)
		return 0;

	if (!sd)
		return -ENODEV;
	vpss_sdev = v4l2_get_subdevdata(sd);
	if (!vpss_sdev)
		return -ENODEV;
	dev = vpss_sdev->dev;
	if (!dev)
		return -ENODEV;
	vdev = &dev->stream_vdev;
	if (vpss_sdev->state & VPSS_START)
		return -EBUSY;

	if (!strcmp(remote->entity->name, S0_VDEV_NAME)) {
		stream = &vdev->stream[RKVPSS_OUTPUT_CH0];
	} else if (!strcmp(remote->entity->name, S1_VDEV_NAME)) {
		stream = &vdev->stream[RKVPSS_OUTPUT_CH1];
	} else if (!strcmp(remote->entity->name, S2_VDEV_NAME)) {
		stream = &vdev->stream[RKVPSS_OUTPUT_CH2];
	} else if (!strcmp(remote->entity->name, S3_VDEV_NAME)) {
		stream = &vdev->stream[RKVPSS_OUTPUT_CH3];
	} else if (strstr(remote->entity->name, "rkisp")) {
		if (flags & MEDIA_LNK_FL_ENABLED)
			dev->inp = INP_ISP;
		else
			dev->inp = INP_INVAL;
	}
	if (stream)
		stream->linked = flags & MEDIA_LNK_FL_ENABLED;
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev, "input:%d\n", dev->inp);
	return 0;
}

static int rkvpss_sd_get_fmt(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *sd_state,
			     struct v4l2_subdev_format *fmt)
{
	struct rkvpss_subdev *sdev = v4l2_get_subdevdata(sd);
	struct rkvpss_device *dev = sdev->dev;
	struct v4l2_mbus_framefmt *mf;
	int ret = 0;

	if (!fmt)
		return -EINVAL;

	if (fmt->pad != RKVPSS_PAD_SINK &&
	    fmt->pad != RKVPSS_PAD_SOURCE)
		return -EINVAL;

	mf = &fmt->format;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		if (!sd_state)
			return -EINVAL;
		mf = v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
	}

	*mf = sdev->in_fmt;
	if (fmt->pad == RKVPSS_PAD_SINK && sdev->dev->inp == INP_ISP) {
		ret = v4l2_subdev_call(dev->remote_sd, pad, get_fmt, sd_state, fmt);
		if (!ret) {
			if (sdev->in_fmt.width != mf->width ||
			    sdev->in_fmt.height != mf->height) {
				sdev->out_fmt.width = mf->width;
				sdev->out_fmt.height = mf->height;
				rkvpss_pipeline_default_fmt(dev);
			}
			sdev->in_fmt = *mf;
		}
	} else {
		*mf = sdev->in_fmt;
		mf->width = sdev->out_fmt.width;
		mf->height = sdev->out_fmt.height;
	}
	return ret;
}

static int rkvpss_sd_set_fmt(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *sd_state,
			     struct v4l2_subdev_format *fmt)
{
	struct rkvpss_subdev *sdev = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mf;

	if (!fmt)
		return -EINVAL;
	/* format from isp output */
	if (fmt->pad == RKVPSS_PAD_SINK && sdev->dev->inp == INP_ISP)
		return rkvpss_sd_get_fmt(sd, sd_state, fmt);

	mf = &fmt->format;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		if (!sd_state)
			return -EINVAL;
		mf = v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
	}

	if (fmt->pad == RKVPSS_PAD_SINK) {
		sdev->in_fmt = *mf;
	} else {
		sdev->out_fmt.width = mf->width;
		sdev->out_fmt.height = mf->height;
	}
	return 0;
}

static int rkvpss_sd_s_stream(struct v4l2_subdev *sd, int on)
{
	struct rkvpss_subdev *sdev = v4l2_get_subdevdata(sd);
	struct rkvpss_device *dev = sdev->dev;
	u32 val, w = sdev->in_fmt.width, h = sdev->in_fmt.height;

	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "s_stream on:%d %dx%d\n", on, w, h);

	if (!on) {
		rkvpss_clear_bits(dev, RKVPSS_VPSS_ONLINE, RKVPSS_ONLINE_MODE_MASK);
		rkvpss_clear_bits(dev, RKVPSS_VPSS_IMSC, RKVPSS_ISP_ALL_FRM_END);
		sdev->state = VPSS_STOP;
		atomic_dec(&dev->hw_dev->refcnt);
		return 0;
	}

	sdev->frame_seq = -1;
	sdev->frame_timestamp = 0;
	atomic_inc(&dev->hw_dev->refcnt);
	dev->cmsc_upd = true;
	rkvpss_cmsc_config(dev, true);
	rkvpss_write(dev, RKVPSS_VPSS_ONLINE2_SIZE, h << 16 | w);

	val = RKVPSS_CFG_FORCE_UPD | RKVPSS_CFG_GEN_UPD | RKVPSS_MIR_GEN_UPD;
	if (!dev->hw_dev->is_ofl_cmsc)
		val |= RKVPSS_MIR_FORCE_UPD;
	rkvpss_write(dev, RKVPSS_VPSS_UPDATE, val);

	rkvpss_set_bits(dev, RKVPSS_VPSS_IMSC, 0, RKVPSS_ISP_ALL_FRM_END);
	sdev->state |= VPSS_START;
	return 0;
}

static int rkvpss_sd_s_rx_buffer(struct v4l2_subdev *sd,
				 void *buf, unsigned int *size)
{
	return 0;
}

static int rkvpss_sd_s_power(struct v4l2_subdev *sd, int on)
{
	struct rkvpss_subdev *sdev = v4l2_get_subdevdata(sd);
	struct rkvpss_device *dev = sdev->dev;
	int ret;

	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "power on:%d\n", on);

	if (on) {
		if (dev->inp == INP_ISP) {
			struct v4l2_subdev_format fmt = {
				.pad = RKVPSS_PAD_SINK,
				.which = V4L2_SUBDEV_FORMAT_ACTIVE,
			};

			ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
			if (ret) {
				v4l2_err(&dev->v4l2_dev,
					 "%s get isp format fail:%d\n", __func__, ret);
				return ret;
			}
			ret = v4l2_subdev_call(dev->remote_sd, core, s_power, 1);
			if (ret < 0) {
				v4l2_err(&dev->v4l2_dev,
					 "%s set isp power on fail:%d\n", __func__, ret);
				return ret;
			}
		}
		ret = pm_runtime_get_sync(dev->dev);
		if (ret < 0) {
			v4l2_err(&dev->v4l2_dev,
				 "%s runtime get fail:%d\n", __func__, ret);
			if (dev->inp == INP_ISP)
				v4l2_subdev_call(dev->remote_sd, core, s_power, 0);
		}
	} else {
		if (dev->inp == INP_ISP)
			v4l2_subdev_call(dev->remote_sd, core, s_power, 0);
		ret = pm_runtime_put_sync(dev->dev);
	}
	if (ret < 0)
		v4l2_err(sd, "%s on:%d failed:%d\n", __func__, on, ret);
	return ret;
}

static int rkvpss_sof(struct rkvpss_subdev *sdev, struct rkisp_vpss_sof *info)
{
	struct rkvpss_device *dev = sdev->dev;
	struct rkvpss_hw_dev *hw = dev->hw_dev;
	struct rkvpss_stream *stream;
	int i;

	if (!info)
		return -EINVAL;
	sdev->frame_seq = info->seq;
	sdev->frame_timestamp = info->timestamp;

	rkvpss_cmsc_config(dev, !info->irq);
	for (i = 0; i < RKVPSS_OUTPUT_MAX; i++) {
		stream = &dev->stream_vdev.stream[i];
		if (!stream->streaming)
			continue;
		if (stream->ops->frame_start)
			stream->ops->frame_start(stream, info->irq);
	}

	if (!info->irq && !hw->is_single) {
		hw->cur_dev_id = dev->dev_id;
		rkvpss_update_regs(dev, RKVPSS_VPSS_CTRL, RKVPSS_VPSS_ONLINE2_SIZE);
		rkvpss_update_regs(dev, RKVPSS_VPSS_IRQ_CFG, RKVPSS_VPSS_IMSC);
		rkvpss_update_regs(dev, RKVPSS_VPSS_Y2R_COE00, RKVPSS_VPSS_Y2R_OFF2);
		rkvpss_update_regs(dev, RKVPSS_CMSC_INTSCT_CORR, RKVPSS_CMSC_WIN7_L3_SLP);
		rkvpss_update_regs(dev, RKVPSS_CROP1_0_H_OFFS, RKVPSS_CROP1_3_V_SIZE);
		rkvpss_update_regs(dev, RKVPSS_ZME_Y_HOR_COE0_10, RKVPSS_ZME_UV_VER_COE16_76);
		rkvpss_update_regs(dev, RKVPSS_ZME_H_SIZE, RKVPSS_ZME_UV_YSCL_FACTOR);
		rkvpss_update_regs(dev, RKVPSS_SCALE1_SRC_SIZE, RKVPSS_SCALE1_IN_CROP_OFFSET);
		rkvpss_update_regs(dev, RKVPSS_SCALE2_SRC_SIZE, RKVPSS_SCALE2_IN_CROP_OFFSET);
		rkvpss_update_regs(dev, RKVPSS_SCALE3_SRC_SIZE, RKVPSS_SCALE3_IN_CROP_OFFSET);
		rkvpss_update_regs(dev, RKVPSS_MI_WR_WRAP_CTRL, RKVPSS_MI_IMSC);
		rkvpss_update_regs(dev, RKVPSS_MI_CHN0_WR_CTRL, RKVPSS_MI_CHN3_WR_LINE_CNT);

		rkvpss_update_regs(dev, RKVPSS_MI_WR_CTRL, RKVPSS_MI_WR_INIT);
		rkvpss_update_regs(dev, RKVPSS_SCALE3_CTRL, RKVPSS_SCALE3_UPDATE);
		rkvpss_update_regs(dev, RKVPSS_SCALE2_CTRL, RKVPSS_SCALE2_UPDATE);
		rkvpss_update_regs(dev, RKVPSS_SCALE1_CTRL, RKVPSS_SCALE1_UPDATE);
		rkvpss_update_regs(dev, RKVPSS_ZME_CTRL, RKVPSS_ZME_UPDATE);
		rkvpss_update_regs(dev, RKVPSS_CROP1_CTRL, RKVPSS_CROP1_UPDATE);
		rkvpss_update_regs(dev, RKVPSS_CMSC_CTRL, RKVPSS_CMSC_UPDATE);
		rkvpss_update_regs(dev, RKVPSS_VPSS_UPDATE, RKVPSS_VPSS_UPDATE);
	}
	return 0;
}

static long rkvpss_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct rkvpss_subdev *sdev = v4l2_get_subdevdata(sd);
	long ret = 0;

	switch (cmd) {
	case RKISP_VPSS_CMD_SOF:
		ret = rkvpss_sof(sdev, arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long rkvpss_compat_ioctl32(struct v4l2_subdev *sd,
				  unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	return ret;
}
#endif

static const struct media_entity_operations rkvpss_sd_media_ops = {
	.link_setup = rkvpss_subdev_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

static const struct v4l2_subdev_pad_ops rkvpss_sd_pad_ops = {
	.get_fmt = rkvpss_sd_get_fmt,
	.set_fmt = rkvpss_sd_set_fmt,
};

static const struct v4l2_subdev_video_ops rkvpss_sd_video_ops = {
	.s_stream = rkvpss_sd_s_stream,
	.s_rx_buffer = rkvpss_sd_s_rx_buffer,
};

static const struct v4l2_subdev_core_ops rkvpss_sd_core_ops = {
	.s_power = rkvpss_sd_s_power,
	.ioctl = rkvpss_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = rkvpss_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_ops rkvpss_sd_ops = {
	.core = &rkvpss_sd_core_ops,
	.video = &rkvpss_sd_video_ops,
	.pad = &rkvpss_sd_pad_ops,
};

static void rkvpss_subdev_default_fmt(struct rkvpss_subdev *sdev)
{
	struct v4l2_mbus_framefmt *in_fmt = &sdev->in_fmt;
	struct vpsssd_fmt *out_fmt = &sdev->out_fmt;

	in_fmt->width = RKVPSS_DEFAULT_WIDTH;
	in_fmt->height = RKVPSS_DEFAULT_HEIGHT;
	in_fmt->code = MEDIA_BUS_FMT_YUYV8_2X8;

	*out_fmt = rkvpss_formats[0];
	out_fmt->width = RKVPSS_DEFAULT_WIDTH;
	out_fmt->height = RKVPSS_DEFAULT_HEIGHT;
}

int rkvpss_register_subdev(struct rkvpss_device *dev,
			   struct v4l2_device *v4l2_dev)
{
	struct rkvpss_subdev *vpss_sdev = &dev->vpss_sdev;
	struct v4l2_subdev *sd;
	int ret;

	spin_lock_init(&dev->cmsc_lock);
	memset(vpss_sdev, 0, sizeof(*vpss_sdev));
	vpss_sdev->dev = dev;
	sd = &vpss_sdev->sd;
	vpss_sdev->state = VPSS_STOP;
	v4l2_subdev_init(sd, &rkvpss_sd_ops);
	//sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	sd->entity.ops = &rkvpss_sd_media_ops;
	snprintf(sd->name, sizeof(sd->name), "rkvpss-subdev");
	sd->entity.function = MEDIA_ENT_F_PROC_VIDEO_COMPOSER;
	vpss_sdev->pads[RKVPSS_PAD_SINK].flags =
		MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_MUST_CONNECT;
	vpss_sdev->pads[RKVPSS_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&sd->entity, RKVPSS_PAD_MAX, vpss_sdev->pads);
	if (ret < 0)
		return ret;
	sd->owner = THIS_MODULE;
	v4l2_set_subdevdata(sd, vpss_sdev);
	sd->grp_id = GRP_ID_VPSS;
	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret < 0)
		goto free_media;
	rkvpss_subdev_default_fmt(vpss_sdev);
	return ret;
free_media:
	media_entity_cleanup(&sd->entity);
	v4l2_err(sd, "Failed to register subdev, ret:%d\n", ret);
	return ret;
}

void rkvpss_unregister_subdev(struct rkvpss_device *dev)
{
	struct v4l2_subdev *sd = &dev->vpss_sdev.sd;

	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
}
