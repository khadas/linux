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
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-dma-contig.h>

#include "aml_common.h"
#include "aml_misc.h"
#include <linux/delay.h>

static int video_verify_fmt(struct aml_video *video, struct v4l2_format *fmt)
{
	u32 i = 0;
	int rtn = -EINVAL;

	for (i = 0; i < video->fmt_cnt; i++) {
		if (fmt->fmt.pix.pixelformat == video->format[i].fourcc) {
			fmt->fmt.pix.sizeimage = DATA_ALIGN(fmt->fmt.pix.width, 64) *
					fmt->fmt.pix.height *
					video->format[i].bpp / 8;
			rtn = 0;
			break;
		}
	}

	return rtn;
}

static void video_init_fmt(struct aml_video *video)
{
	struct v4l2_format fmt;

	memset(&fmt, 0, sizeof(fmt));

	fmt.type = video->type;
	fmt.fmt.pix.width = 1920;
	fmt.fmt.pix.height = 1080;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	fmt.fmt.pix.pixelformat = video->format[0].fourcc;
	fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

	video_verify_fmt(video, &fmt);

	video->f_current = fmt;
}

static long video_get_dma_buff(struct vb2_buffer *vb)
{
	int fd = 0;
	struct sg_table *sgt;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct aml_buffer *buff = container_of(vbuf, struct aml_buffer, vb);
	struct aml_video *video = vb2_get_drv_priv(vb->vb2_queue);

	fd = vb->planes[AML_PLANE_A].m.fd;
	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		pr_err("Error to get dbuf: fd %d\n", fd);
		return PTR_ERR(dbuf);
	}

	attach = dma_buf_attach(dbuf, video->dev);
	if (IS_ERR(attach)) {
		dma_buf_put(dbuf);
		pr_err("Error to attach dbuf\n");
		return PTR_ERR(attach);
	}

	sgt = dma_buf_map_attachment(attach, DMA_NONE);
	if (IS_ERR(sgt)) {
		dma_buf_detach(dbuf, attach);
		dma_buf_put(dbuf);
		pr_err("Error to map attach dbuf\n");
		return PTR_ERR(sgt);
	}

	buff->dbuffer.fd = fd;
	buff->dbuffer.sgt = sgt;
	buff->dbuffer.dbuf = dbuf;
	buff->dbuffer.attach = attach;
	buff->addr[AML_PLANE_A] = sg_dma_address(sgt->sgl);

	return 0;
}

static void video_put_dma_buff(struct vb2_buffer *vb)
{
	struct sg_table *sgt;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct aml_buffer *buff = container_of(vbuf, struct aml_buffer, vb);
	struct aml_video *video = vb2_get_drv_priv(vb->vb2_queue);

	sgt = buff->dbuffer.sgt;
	dbuf = buff->dbuffer.dbuf;
	attach = buff->dbuffer.attach;

	dma_buf_unmap_attachment(attach, sgt, DMA_NONE);
	dma_buf_detach(dbuf, attach);
	dma_buf_put(dbuf);

	buff->addr[AML_PLANE_A] = 0x00000000;
	buff->dbuffer.sgt = NULL;
	buff->dbuffer.dbuf = NULL;
	buff->dbuffer.attach = NULL;
}

static int video_querycap(struct file *file, void *fh,
			  struct v4l2_capability *cap)
{
	struct aml_video *video = video_drvdata(file);

	strncpy(cap->driver, "aml-camera", sizeof(cap->driver));
	strncpy(cap->card, "Amlogic Camera Card", sizeof(cap->card));

	if (video->bus_info)
		strncpy(cap->bus_info, video->bus_info, sizeof(cap->bus_info));

	return 0;
}

static int video_enum_fmt(struct file *file, void *fh, struct v4l2_fmtdesc *fmt)
{
	struct aml_video *video = video_drvdata(file);

	if (fmt->type != video->type || fmt->index >= video->fmt_cnt)
		return -EINVAL;

	fmt->pixelformat = video->format[fmt->index].fourcc;

	return 0;
}

static int video_get_fmt(struct file *file, void *fh, struct v4l2_format *fmt)
{
	struct aml_video *video = video_drvdata(file);

	*fmt = video->f_current;

	return 0;
}

static int video_set_fmt(struct file *file, void *fh, struct v4l2_format *fmt)
{
	int rtn = 0;
	struct aml_video *video = video_drvdata(file);

	if (vb2_is_busy(&video->vb2_q))
		return -EBUSY;

	rtn = video_verify_fmt(video, fmt);
	if (rtn) {
		fmt->type = video->type;
		fmt->fmt.pix.field = V4L2_FIELD_NONE;
		fmt->fmt.pix.pixelformat = video->format[0].fourcc;
		fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	}

	video->f_current = *fmt;

	if (video->ops->cap_set_format)
		video->ops->cap_set_format(video);

	return 0;
}

static int video_try_fmt(struct file *file, void *fh, struct v4l2_format *fmt)
{
	int rtn = 0;
	struct aml_video *video = video_drvdata(file);

	rtn = video_verify_fmt(video, fmt);
	if (rtn) {
		fmt->type = video->type;
		fmt->fmt.pix.field = V4L2_FIELD_NONE;
		fmt->fmt.pix.pixelformat = video->format[0].fourcc;
		fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
	}

	return 0;
}

static int video_g_selection(struct file *file, void *fh, struct v4l2_selection *slt)
{
	struct aml_video *video = video_drvdata(file);
	struct aml_crop *crop = &video->acrop;

	slt->r.left = crop->hstart;
	slt->r.top = crop->vstart;
	slt->r.width = crop->hsize;
	slt->r.height = crop->vsize;

	return 0;
}

static int video_s_selection(struct file *file, void *fh, struct v4l2_selection *slt)
{
	struct aml_video *video = video_drvdata(file);
	struct aml_crop *crop = &video->acrop;

	crop->hstart = slt->r.left;
	crop->vstart = slt->r.top;
	crop->hsize = slt->r.width;
	crop->vsize = slt->r.height;

	return 0;
}

static int video_ioctl_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	int rtn = -EINVAL;
	struct vb2_buffer *vb;
	struct video_device *vdev;

	vdev = video_devdata(file);
	vb = vdev->queue->bufs[p->index];

	switch (p->memory) {
	case V4L2_MEMORY_MMAP:
	break;
	case V4L2_MEMORY_DMABUF:
		rtn = video_get_dma_buff(vb);
		if (rtn) {
			pr_err("Failed to get dma buf: %d\n", rtn);
			return rtn;
		}
	break;
	default:
		pr_err("Error  memory type: %d\n", vb->memory);
		return rtn;
	}

	rtn = vb2_ioctl_qbuf(file, priv, p);
	if (rtn)
		pr_err("Failed to dqubuf: %d\n", rtn);

	return rtn;
}

static int video_ioctl_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	int rtn = 0;
	struct vb2_buffer *vb;
	struct video_device *vdev;

	rtn = vb2_ioctl_dqbuf(file, priv, p);
	if (rtn) {
		pr_err("Failed to dqubuf: %d\n", rtn);
		return rtn;
	}

	vdev = video_devdata(file);
	vb = vdev->queue->bufs[p->index];

	switch (vb->memory) {
	case V4L2_MEMORY_MMAP:
	break;
	case V4L2_MEMORY_DMABUF:
		video_put_dma_buff(vb);
	break;
	default:
		rtn = -EINVAL;
		pr_err("Error  memory type: %d\n", vb->memory);
	break;
	}

	return rtn;
}

static const struct v4l2_ioctl_ops aml_v4l2_ioctl_ops = {
	.vidioc_querycap		= video_querycap,
	.vidioc_enum_fmt_vid_cap	= video_enum_fmt,
	.vidioc_g_fmt_vid_cap		= video_get_fmt,
	.vidioc_s_fmt_vid_cap		= video_set_fmt,
	.vidioc_try_fmt_vid_cap		= video_try_fmt,
	.vidioc_reqbufs			= vb2_ioctl_reqbufs,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_qbuf			= video_ioctl_qbuf,
	.vidioc_expbuf			= vb2_ioctl_expbuf,
	.vidioc_dqbuf			= video_ioctl_dqbuf,
	.vidioc_prepare_buf		= vb2_ioctl_prepare_buf,
	.vidioc_create_bufs		= vb2_ioctl_create_bufs,
	.vidioc_g_selection             = video_g_selection,
	.vidioc_s_selection             = video_s_selection,
	.vidioc_streamon		= vb2_ioctl_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,
	.vidioc_log_status		= v4l2_ctrl_log_status,
	.vidioc_subscribe_event		= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static int video_buff_queue_setup(struct vb2_queue *queue,
			unsigned int *num_buffers,
			unsigned int *num_planes,
			unsigned int sizes[],
			struct device *alloc_devs[])
{
	struct aml_video *video = queue->drv_priv;
	const struct v4l2_pix_format *pix = &video->f_current.fmt.pix;

	if (*num_planes) {
		if (sizes[0] < pix->sizeimage)
			return -EINVAL;
	}

	*num_planes = 1;
	sizes[0] = pix->sizeimage;

	return 0;
}

static void video_buff_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct aml_buffer *buff = container_of(vbuf, struct aml_buffer, vb);
	struct aml_video *video = vb2_get_drv_priv(vb->vb2_queue);

	if (video->ops->cap_cfg_buffer)
		video->ops->cap_cfg_buffer(video, buff);
}

static int video_buff_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct aml_video *video = vb2_get_drv_priv(vb->vb2_queue);
	u32 size = video->f_current.fmt.pix.sizeimage;

	if (vb2_plane_size(vb, 0) < size) {
		dev_err(video->dev,
			"Error user buffer too small (%ld < %u)\n",
			vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(vb, 0, size);

	vbuf->field = V4L2_FIELD_NONE;

	return 0;
}

static int video_buff_init(struct vb2_buffer *vb)
{
	u32 p_size = 0;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct aml_video *video = vb2_get_drv_priv(vb->vb2_queue);
	struct aml_buffer *buff = container_of(vbuf, struct aml_buffer, vb);
	const struct v4l2_pix_format *pix = &video->f_current.fmt.pix;

	buff->addr[AML_PLANE_A] = *((u32 *)vb2_plane_cookie(vb, 0));
	buff->vaddr[AML_PLANE_A] = vb2_plane_vaddr(vb, 0);
	buff->nplanes = video->afmt.nplanes;
	buff->bsize = pix->width * pix->height * video->afmt.bpp / 8;

	if (buff->nplanes > 1) {
		if (pix->pixelformat == V4L2_PIX_FMT_NV12 ||
			pix->pixelformat == V4L2_PIX_FMT_NV21) {
			p_size = pix->width * pix->height;
		}

		if (video->afmt.fourcc == AML_FMT_HDR_RAW)
			p_size = pix->width * pix->height * video->afmt.bpp / 8;

		buff->addr[AML_PLANE_B] = buff->addr[AML_PLANE_A] + p_size;
		buff->bsize = pix->width * pix->height * 12 / 8;
	}

	return 0;
}

static int video_get_sensor_fps(struct media_entity *entity, struct aml_video *video)
{
	int rtn = 0;
	struct v4l2_subdev *subdev;
	struct v4l2_ctrl *ctrl;

	if (entity->function != MEDIA_ENT_F_CAM_SENSOR)
		return 0;

	subdev = media_entity_to_v4l2_subdev(entity);

	ctrl = v4l2_ctrl_find(subdev->ctrl_handler, V4L2_CID_AML_USER_FPS);
	if (!ctrl) {
		pr_debug("Failed to get fps ctrl,set default 30\n");
		video->actrl.fps_sensor = 30;
		video->actrl.fps_output = 30;
		return -EINVAL;
	}

	video->actrl.fps_sensor = ctrl->val;

	return 0;
}

static int video_start_streaming(struct vb2_queue *queue, unsigned int count)
{
	int rtn = 0;
	struct media_pad *pad = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct aml_video *video = vb2_get_drv_priv(queue);
	struct media_entity *entity = &video->vdev.entity;

	if (strstr(entity->name, "isp-stats") || strstr(entity->name, "isp-param")) {
		if (video->ops->cap_stream_on)
			video->ops->cap_stream_on(video);
		return 0;
	}

	rtn = media_pipeline_start(entity, video->pipe);
	if (rtn) {
		dev_err(video->dev, "Failed to start pipeline: %d\n", rtn);
		goto error_return;
	}

	if (video->ops->cap_stream_on)
		video->ops->cap_stream_on(video);

	while (1) {
		pad = &entity->pads[0];
		pad = media_entity_remote_pad(pad);
		if (!pad || !is_media_entity_v4l2_subdev(pad->entity))
			break;

		if (pad->flags & MEDIA_PAD_FL_SINK)
			break;

		entity = pad->entity;

		video_get_sensor_fps(entity, video);

		if (entity->stream_count > 1)
			continue;

		subdev = media_entity_to_v4l2_subdev(entity);

		rtn = v4l2_subdev_call(subdev, video, s_stream, 1);
		if (rtn < 0 && rtn != -ENOIOCTLCMD) {
			entity = &video->vdev.entity;
			media_pipeline_stop(entity);
			goto error_return;
		}
	}

error_return:
	return rtn;
}

static void video_stop_streaming(struct vb2_queue *queue)
{
	struct media_pad *pad = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct aml_video *video = vb2_get_drv_priv(queue);
	struct media_entity *entity = &video->vdev.entity;

	if (strstr(entity->name, "isp-stats") || strstr(entity->name, "isp-param")) {
		if (video->ops->cap_stream_off)
			video->ops->cap_stream_off(video);
		if (video->ops->cap_flush_buffer)
			video->ops->cap_flush_buffer(video);
		return;
	}

	media_pipeline_stop(entity);

	while (1) {
		pad = &entity->pads[0];
		pad = media_entity_remote_pad(pad);
		if (!pad || !is_media_entity_v4l2_subdev(pad->entity))
			break;

		if (pad->flags & MEDIA_PAD_FL_SINK)
			break;

		entity = pad->entity;
		if (entity->stream_count)
			continue;

		subdev = media_entity_to_v4l2_subdev(entity);
	}

	if (subdev && video)
		v4l2_subdev_call(subdev, video, s_stream, 0);

	msleep(100);
	entity = &video->vdev.entity;
	while (1) {
		pad = &entity->pads[0];
		pad = media_entity_remote_pad(pad);
		if (!pad || !is_media_entity_v4l2_subdev(pad->entity))
			break;

		if (pad->flags & MEDIA_PAD_FL_SINK)
			break;

		entity = pad->entity;
		if (entity->stream_count)
			continue;

		subdev = media_entity_to_v4l2_subdev(entity);
		v4l2_subdev_call(subdev, video, s_stream, 0);
	}

	video->actrl.fps_sensor = 0;
	video->actrl.fps_output = 0;

	if (video->ops->cap_stream_off)
		video->ops->cap_stream_off(video);

	if (video->ops->cap_flush_buffer)
		video->ops->cap_flush_buffer(video);
}

static struct vb2_ops aml_vb2_ops = {
	.queue_setup = video_buff_queue_setup,
	.buf_queue = video_buff_queue,
	.buf_prepare = video_buff_prepare,
	.buf_init = video_buff_init,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.start_streaming = video_start_streaming,
	.stop_streaming = video_stop_streaming,
};

static const struct v4l2_file_operations aml_v4l2_fops = {
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
	.read = vb2_fop_read,
};

static void aml_video_release(struct video_device *vdev)
{
	struct aml_video *video = video_get_drvdata(vdev);

	media_entity_cleanup(&vdev->entity);

	mutex_destroy(&video->q_lock);
	mutex_destroy(&video->lock);
}

int aml_video_register(struct aml_video *video)
{
	int rtn = 0;
	struct video_device *vdev;
	struct vb2_queue *vb2_q;
	struct media_pad *pad;

	vdev = &video->vdev;
	pad = &video->pad;

	mutex_init(&video->q_lock);

	vb2_q = &video->vb2_q;
	vb2_q->drv_priv = video;
	vb2_q->mem_ops = &vb2_dma_contig_memops;
	vb2_q->ops = &aml_vb2_ops;
	vb2_q->type = video->type;
	vb2_q->io_modes = VB2_DMABUF | VB2_MMAP | VB2_READ;
	vb2_q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	vb2_q->buf_struct_size = sizeof(struct aml_buffer);
	vb2_q->dev = video->dev;
	vb2_q->lock = &video->q_lock;
	vb2_q->min_buffers_needed = video->min_buffer_count;
	rtn = vb2_queue_init(vb2_q);
	if (rtn < 0) {
		dev_err(video->dev, "Failed to init vb2 queue: %d\n", rtn);
		goto error_vb2_init;
	}

	rtn = media_entity_pads_init(&vdev->entity, 1, pad);
	if (rtn < 0) {
		dev_err(video->dev, "Failed to init entity pads: %d\n", rtn);
		goto error_media_init;
	}

	mutex_init(&video->lock);
	video_init_fmt(video);

	vdev->fops = &aml_v4l2_fops;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE |
			V4L2_CAP_STREAMING |
			V4L2_CAP_READWRITE;

	vdev->ioctl_ops = &aml_v4l2_ioctl_ops;
	vdev->release = aml_video_release;
	vdev->v4l2_dev = video->v4l2_dev;
	vdev->vfl_dir = VFL_DIR_RX;
	vdev->queue = &video->vb2_q;
	vdev->lock = &video->lock;
	strncpy(vdev->name, video->name, sizeof(vdev->name));
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
	rtn = video_register_device(vdev, VFL_TYPE_VIDEO, VIDEO_NODE);
#else
	rtn = video_register_device(vdev, VFL_TYPE_GRABBER, VIDEO_NODE);
#endif
	if (rtn < 0) {
		dev_err(video->dev, "Failed to register video device: %d\n", rtn);
		goto error_video_register;
	}

	video_set_drvdata(vdev, video);

	return 0;

error_video_register:
	media_entity_cleanup(&vdev->entity);
	mutex_destroy(&video->lock);
error_media_init:
	vb2_queue_release(&video->vb2_q);
error_vb2_init:
	mutex_destroy(&video->q_lock);

	return rtn;
}

void aml_video_unregister(struct aml_video *video)
{
	vb2_queue_release(&video->vb2_q);
	video_unregister_device(&video->vdev);
}
