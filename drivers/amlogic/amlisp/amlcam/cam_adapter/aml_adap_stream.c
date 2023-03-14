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

#include "aml_adapter.h"

static char *stream_name[] = {
	"adap-tof",
};

static const struct aml_format adap_cap_support_format[] = {
	{0, 0, 0, 0, V4L2_PIX_FMT_GREY, 0, 1, 8},
	{0, 0, 0, 0, V4L2_PIX_FMT_Y16, 0, 1, 16},
	{0, 0, 0, 0, V4L2_PIX_FMT_Y12, 0, 1, 12},
};

static int adap_cap_irq_handler(void *video, int status)
{
	unsigned long flags;
	struct aml_video *vd = video;
	struct aml_buffer *b_current = vd->b_current;
	struct adapter_dev_t *adap_dev = vd->priv;
	const struct adapter_dev_ops *ops = adap_dev->ops;

	spin_lock_irqsave(&vd->buff_list_lock, flags);

	if (vd->status != AML_ON) {
		spin_unlock_irqrestore(&vd->buff_list_lock, flags);
		return 0;
	}

	if (b_current) {
		vd->frm_cnt++;
		b_current->vb.sequence = vd->frm_cnt;
		b_current->vb.vb2_buf.timestamp = ops->hw_timestamp(adap_dev);
		b_current->vb.field = V4L2_FIELD_NONE;
		vb2_buffer_done(&b_current->vb.vb2_buf, VB2_BUF_STATE_DONE);

		vd->b_current = NULL;
	}

	vd->b_current = list_first_entry_or_null(&vd->head, struct aml_buffer, list);
	if (vd->b_current) {
		//ops->hw_stream_cfg_buf(vd, vd->b_current);
		list_del(&vd->b_current->list);
	}

	spin_unlock_irqrestore(&vd->buff_list_lock, flags);

	return 0;
}

static int adap_cap_set_format(void *video)
{
	u32 i = 0;
	int rtn = 0;
	struct aml_video *vd = video;
	struct aml_format *fmt = &vd->afmt;
	struct adapter_dev_t *adap_dev = vd->priv;
	const struct adapter_dev_ops *ops = adap_dev->ops;

	fmt->width = vd->f_current.fmt.pix.width;
	fmt->height = vd->f_current.fmt.pix.height;
	fmt->fourcc = vd->f_current.fmt.pix.pixelformat;
	for (i = 0; i < vd->fmt_cnt; i++) {
		if (fmt->fourcc == vd->format[i].fourcc)
			fmt->bpp = vd->format[i].bpp;
	}

	//if (ops && ops->hw_stream_set_fmt)
		//rtn = ops->hw_stream_set_fmt(video, fmt);

	return rtn;
}

static int adap_cap_cfg_buffer(void *video, void *buff)
{
	int rtn = 0;
	unsigned long flags;
	struct aml_video *vd = video;
	struct aml_buffer *buffer = buff;
	struct adapter_dev_t *adap_dev = vd->priv;
	const struct adapter_dev_ops *ops = adap_dev->ops;

	spin_lock_irqsave(&vd->buff_list_lock, flags);

	if (vd->b_current) {
		list_add_tail(&buffer->list, &vd->head);
		spin_unlock_irqrestore(&vd->buff_list_lock, flags);
		return rtn;
	}

	//if (ops && ops->hw_stream_cfg_buf) {
	//	rtn = ops->hw_stream_cfg_buf(video, buff);
	//	vd->b_current = buff;
	//}

	spin_unlock_irqrestore(&vd->buff_list_lock, flags);

	return rtn;
}

static void adap_cap_stream_on(void *video)
{
	struct aml_video *vd = video;
	struct adapter_dev_t *adap_dev = vd->priv;
	const struct adapter_dev_ops *ops = adap_dev->ops;

	//if (ops && ops->hw_stream_on)
	//	ops->hw_stream_on(video);

	vd->status = AML_ON;
}

static void adap_cap_stream_off(void *video)
{
	struct aml_video *vd = video;
	struct adapter_dev_t *adap_dev = vd->priv;
	const struct adapter_dev_ops *ops = adap_dev->ops;

	//if (ops && ops->hw_stream_off)
	//	ops->hw_stream_off(video);

	vd->status = AML_OFF;
}

static void adap_vb2_discard_done(struct vb2_queue *q)
{
	struct vb2_buffer *vb;
	unsigned long flags;

	spin_lock_irqsave(&q->done_lock, flags);
	list_for_each_entry(vb, &q->done_list, done_entry)
		vb->state = VB2_BUF_STATE_ERROR;
	spin_unlock_irqrestore(&q->done_lock, flags);
}

static void adap_cap_flush_buffer(void *video)
{
	unsigned long flags;
	struct aml_video *vd = video;
	struct aml_buffer *buff;

	spin_lock_irqsave(&vd->buff_list_lock, flags);

	list_for_each_entry(buff, &vd->head, list) {
		if (buff->vb.vb2_buf.state == VB2_BUF_STATE_ACTIVE)
			vb2_buffer_done(&buff->vb.vb2_buf, VB2_BUF_STATE_ERROR);
	}

	INIT_LIST_HEAD(&vd->head);

	if (vd->b_current && vd->b_current->vb.vb2_buf.state == VB2_BUF_STATE_ACTIVE)
		vb2_buffer_done(&vd->b_current->vb.vb2_buf, VB2_BUF_STATE_ERROR);

	adap_vb2_discard_done(&vd->vb2_q);
	vd->b_current = NULL;

	spin_unlock_irqrestore(&vd->buff_list_lock, flags);
}

static const struct aml_cap_ops adap_cap_ops = {
	.cap_irq_handler = adap_cap_irq_handler,
	.cap_set_format = adap_cap_set_format,
	.cap_cfg_buffer = adap_cap_cfg_buffer,
	.cap_stream_on = adap_cap_stream_on,
	.cap_stream_off = adap_cap_stream_off,
	.cap_flush_buffer = adap_cap_flush_buffer,
};

int aml_adap_video_register(struct adapter_dev_t *adap_dev)
{
	int id;
	int rtn = 0;
	struct aml_video *video;

	for (id = 0; id < AML_ADAP_STREAM_MAX; id++) {
		video = &adap_dev->video[id];
		video->id = id;
		video->name = stream_name[id];
		video->dev = adap_dev->dev;
		video->v4l2_dev = adap_dev->v4l2_dev;
		video->bus_info = adap_dev->bus_info;
		video->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		video->ops = &adap_cap_ops;
		video->format = adap_cap_support_format;
		video->fmt_cnt = ARRAY_SIZE(adap_cap_support_format);
		video->pipe = &adap_dev->pipe;
		video->status = AML_OFF;
		video->priv = adap_dev;

		video->pad.flags = MEDIA_PAD_FL_SINK;
		video->min_buffer_count = 2;

		INIT_LIST_HEAD(&video->head);
		spin_lock_init(&video->buff_list_lock);

		rtn = aml_video_register(video);
		if (rtn) {
			dev_err(video->dev, "Failed to register stream-%d: %d\n", id, rtn);
			break;
		}
	}

	return rtn;
}

void aml_adap_video_unregister(struct adapter_dev_t *adap_dev)
{
	int i;
	struct aml_video *video;

	for (i = 0; i < AML_ADAP_STREAM_MAX; i++) {
		video = &adap_dev->video[i];
		aml_video_unregister(video);
	}
}
