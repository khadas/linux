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
#include "aml_isp_ctrls.h"
#include "aml_isp.h"

#define AML_ISP_VIDEO_NAME	"aml-cap%d"

#define V4L2_META_AML_ISP_CONFIG     v4l2_fourcc('A', 'C', 'F', 'G') /* Aml isp config */
#define V4L2_META_AML_ISP_STATS	     v4l2_fourcc('A', 'S', 'T', 'S') /* Aml isp statistics */

static char *stream_name[] = {
	"isp-ddr-input",
	"isp-param",
	"isp-stats",
	"isp-output0",
	"isp-output1",
	"isp-output2",
	"isp-output3",
	"isp-raw",
};

static const struct aml_format img_support_format[] = {
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_NV12, 2, 12},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_NV21, 2, 12},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_GREY, 1, 8},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_RGB24, 1, 24},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_YUYV, 1, 16},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SBGGR8,  1, 8},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SBGGR10, 1, 16},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SBGGR12, 1, 16},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SGBRG8,  1, 8},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SGBRG10, 1, 16},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SGBRG12, 1, 16},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SRGGB8,  1, 8},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SRGGB10, 1, 16},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SRGGB12, 1, 16},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SGRBG8,  1, 8},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SGRBG10, 1, 16},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_SGRBG12, 1, 16},
	{0, 0, 0, 0, 0, V4L2_PIX_FMT_RGBX32, 2, 32},
};

static const struct aml_format data_support_format[] = {
	{0, 0, 0, 0, 0, V4L2_META_AML_ISP_CONFIG,  1, 8},
	{0, 0, 0, 0, 0, V4L2_META_AML_ISP_STATS, 1, 8},
};

static const struct aml_format raw_support_format[] = {
	{0, 0, 0, 0, V4L2_PIX_FMT_SBGGR8, 0, 1, 8},
	{0, 0, 0, 0, V4L2_PIX_FMT_SBGGR10, 0, 1, 16},
	{0, 0, 0, 0, V4L2_PIX_FMT_SBGGR12, 0, 1, 16},
	{0, 0, 0, 0, V4L2_PIX_FMT_SGBRG8, 0, 1, 8},
	{0, 0, 0, 0, V4L2_PIX_FMT_SGBRG10, 0, 1, 16},
	{0, 0, 0, 0, V4L2_PIX_FMT_SGBRG12, 0, 1, 16},
	{0, 0, 0, 0, V4L2_PIX_FMT_SRGGB8, 0, 1, 8},
	{0, 0, 0, 0, V4L2_PIX_FMT_SRGGB10, 0, 1, 16},
	{0, 0, 0, 0, V4L2_PIX_FMT_SRGGB12, 0, 1, 16},
	{0, 0, 0, 0, V4L2_PIX_FMT_SGRBG8, 0, 1, 8},
	{0, 0, 0, 0, V4L2_PIX_FMT_SGRBG10, 0, 1, 16},
	{0, 0, 0, 0, V4L2_PIX_FMT_SGRBG12, 0, 1, 16},
};

static int isp_cap_stream_bilateral_cfg(struct aml_video *vd, struct aml_buffer *buff)
{
	struct isp_dev_t *isp_dev = vd->priv;
	const struct isp_dev_ops *ops = isp_dev->ops;

	switch (vd->id) {
	case AML_ISP_STREAM_PARAM:
	case AML_ISP_STREAM_STATS:
		if (ops->hw_stream_bilateral_cfg)
			ops->hw_stream_bilateral_cfg(vd, buff);
	break;
	}

	return 0;
}

/*
return 0: next frame no drop, wrimif enable
       > 0: next frame drop, wrmif disable
       < 0: no need to change wrmif status
*/
static int isp_cap_next_drop(struct aml_video *video, int frm_cnt)
{
	u32 cur_tmp = 0;
	u32 next_tmp = 0;
	int ret = 0;
	u32 fps_sensor = video->actrl.fps_sensor;
	u32 fps_output = video->actrl.fps_output;
	if (frm_cnt == 1)
		return -1;

	if (video->id == AML_ISP_STREAM_PARAM ||
			video->id == AML_ISP_STREAM_STATS)
		return -1;

	if (fps_sensor <= fps_output || fps_output == 0 || fps_sensor == 0)
		return -1;

	cur_tmp = frm_cnt * (fps_sensor - fps_output) * 1000L / fps_sensor / 1000L;

	next_tmp = (frm_cnt + 1) *
			(fps_sensor - fps_output) * 1000L / fps_sensor / 1000L;

	ret = next_tmp - cur_tmp;

	return ret;

}

static int isp_cap_cur_drop(struct aml_video *video)
{
	struct isp_dev_t *isp_dev = video->priv;
	const struct isp_dev_ops *ops = isp_dev->ops;

	return (1 - ops->hw_get_wrmif_stat(video));
}

static int isp_cap_irq_handler(void *video, int status)
{
	unsigned long flags;
	struct aml_video *vd = video;

	struct aml_buffer *b_current = vd->b_current;
	struct isp_dev_t *isp_dev = vd->priv;
	const struct isp_dev_ops *ops = isp_dev->ops;
	struct aml_buffer *t_current = NULL;
	struct isp_global_info *g_info = isp_global_get_info();

	spin_lock_irqsave(&vd->buff_list_lock, flags);

	if (vd->status != AML_ON) {
		spin_unlock_irqrestore(&vd->buff_list_lock, flags);
		return 0;
	}
	if (b_current) {
		vd->frm_cnt++;
		if (g_info->mode == AML_ISP_SCAM) {
			isp_cap_stream_bilateral_cfg(vd, b_current);
		} else {
			if (vd->id != AML_ISP_STREAM_PARAM)
				isp_cap_stream_bilateral_cfg(vd, b_current);
		}
		t_current = list_first_entry_or_null(&vd->head, struct aml_buffer, list);
		if ((t_current == NULL) && (vd->id > AML_ISP_STREAM_PARAM)) {
			pr_err("ISP%d video%d no buf %x %x\n", isp_dev->index, vd->id, b_current->addr[0], b_current->bsize);
			ops->hw_stream_cfg_buf(vd, b_current);
			spin_unlock_irqrestore(&vd->buff_list_lock, flags);
			return 0;
		}
		b_current->vb.sequence = vd->frm_cnt;
		b_current->vb.vb2_buf.timestamp = ktime_get_ns();
		b_current->vb.field = V4L2_FIELD_NONE;
		if (vd->id == AML_ISP_STREAM_PARAM ||
				vd->id == AML_ISP_STREAM_STATS ||
				vd->id == AML_ISP_STREAM_DDR ||
				vd->id == AML_ISP_STREAM_RAW) {
			vb2_buffer_done(&b_current->vb.vb2_buf, VB2_BUF_STATE_DONE);
		} else {
			ops->hw_stream_crop(vd);
			if (!isp_cap_cur_drop(vd))
				vb2_buffer_done(&b_current->vb.vb2_buf, VB2_BUF_STATE_DONE);
			if (isp_cap_next_drop(vd, vd->frm_cnt) > 0) {
				ops->hw_enable_wrmif(vd, 0, 1);
			} else if (isp_cap_next_drop(vd, vd->frm_cnt) == 0) {
				ops->hw_enable_wrmif(vd, 1, 1);
			}
		}

		vd->b_current = NULL;
	}

	vd->b_current = list_first_entry_or_null(&vd->head, struct aml_buffer, list);
	if (vd->b_current) {
		ops->hw_stream_cfg_buf(vd, vd->b_current);
		if (isp_cap_next_drop(vd, vd->frm_cnt) <= 0)
			list_del(&vd->b_current->list);
	}

	spin_unlock_irqrestore(&vd->buff_list_lock, flags);

	return 0;
}

void isp_drv_convert_format(struct aml_video *vd, struct aml_format *fmt)
{
	struct isp_dev_t *isp_dev = vd->priv;

	fmt->width = vd->f_current.fmt.pix.width;
	fmt->height = vd->f_current.fmt.pix.height;
	fmt->code = vd->f_current.fmt.pix.pixelformat;

	switch (vd->f_current.fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		fmt->bpp = 8;
		fmt->nplanes = 2;
		fmt->fourcc = AML_FMT_YUV420;
		fmt->size = fmt->width * fmt->height * 3 / 2;
	break;
	case V4L2_PIX_FMT_GREY:
		fmt->bpp = 8;
		fmt->nplanes = 1;
		fmt->fourcc = AML_FMT_YUV400;
		fmt->size = fmt->width * fmt->height;
	break;
	case V4L2_PIX_FMT_RGB24:
		fmt->bpp = 24;
		fmt->nplanes = 1;
		fmt->fourcc = AML_FMT_YUV444;
		fmt->size = fmt->width * fmt->height * 3;
	break;
	case V4L2_PIX_FMT_YUYV:
		fmt->bpp = 16;
		fmt->nplanes = 1;
		fmt->fourcc = AML_FMT_YUV422;
		fmt->size = fmt->width * fmt->height * 2;
	break;
	case V4L2_META_AML_ISP_CONFIG:
	case V4L2_META_AML_ISP_STATS:
		fmt->bpp = 8;
		fmt->nplanes = 1;
	break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SRGGB12:
		fmt->bpp = 16;
		fmt->nplanes = 1;
		fmt->fourcc = AML_FMT_RAW;
		fmt->size = fmt->width * fmt->height * 2;
	break;
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		fmt->bpp = 8;
		fmt->nplanes = 1;
		fmt->fourcc = AML_FMT_RAW;
		fmt->size = fmt->width * fmt->height;
	break;
	case V4L2_PIX_FMT_RGBX32:
		if (isp_dev->enWDRMode == 0)
			pr_err("Error to support RGBX32 without wdr\n");
		fmt->bpp = 16;
		fmt->nplanes = 2;
		fmt->fourcc = AML_FMT_HDR_RAW;
		fmt->size = fmt->width * fmt->height * 4;
	break;
	default:
		pr_err("Error to support this format, %x\n", vd->f_current.fmt.pix.pixelformat);
	break;
	}
}

static int isp_cap_set_format(void *video)
{
	u32 i = 0;
	int rtn = 0;
	struct aml_video *vd = video;
	struct aml_format *fmt = &vd->afmt;
	struct isp_dev_t *isp_dev = vd->priv;
	const struct isp_dev_ops *ops = isp_dev->ops;

	isp_drv_convert_format(vd, fmt);

	if ((fmt->fourcc == AML_FMT_RAW) ||
		(fmt->fourcc == AML_FMT_HDR_RAW) ||
		(isp_dev->fmt.code == MEDIA_BUS_FMT_YUYV8_2X8) ||
		(isp_dev->fmt.code == MEDIA_BUS_FMT_YVYU8_2X8))
		vd->disp_sel = 2;
	else
		vd->disp_sel = 0;

	if (ops && ops->hw_stream_set_fmt)
		rtn = ops->hw_stream_set_fmt(video, fmt);

	return rtn;
}

static int isp_cap_cfg_buffer(void *video, void *buff)
{
	int rtn = 0;
	unsigned long flags;
	struct aml_video *vd = video;
	struct aml_buffer *buffer = buff;
	struct isp_dev_t *isp_dev = vd->priv;
	const struct isp_dev_ops *ops = isp_dev->ops;
	struct isp_global_info *g_info = isp_global_get_info();

	spin_lock_irqsave(&vd->buff_list_lock, flags);

	if (vd->b_current) {
		list_add_tail(&buffer->list, &vd->head);
		spin_unlock_irqrestore(&vd->buff_list_lock, flags);
		return rtn;
	}

	if (ops && ops->hw_stream_cfg_buf) {
		rtn = ops->hw_stream_cfg_buf(video, buff);
		vd->b_current = buff;
	}

	if (ops && ops->hw_stream_bilateral_cfg) {
		if (vd->id == AML_ISP_STREAM_PARAM) {
			if (g_info->mode == AML_ISP_SCAM) {
				if (isp_dev->isp_status == STATUS_STOP) {
					isp_cap_stream_bilateral_cfg(video, buff);
					vd->b_current = buff;
				}
			} else {
				isp_cap_stream_bilateral_cfg(video, buff);
				vd->b_current = buff;
			}
		}
	}

	spin_unlock_irqrestore(&vd->buff_list_lock, flags);

	return rtn;
}

static void isp_cap_stream_on(void *video)
{
	struct aml_video *vd = video;
	struct isp_dev_t *isp_dev = vd->priv;
	const struct isp_dev_ops *ops = isp_dev->ops;

	vd->frm_cnt = 0;
	if (ops && ops->hw_stream_on)
		ops->hw_stream_on(video);

	vd->status = AML_ON;
}

static void isp_cap_stream_off(void *video)
{
	struct aml_video *vd = video;
	struct isp_dev_t *isp_dev = vd->priv;
	const struct isp_dev_ops *ops = isp_dev->ops;

	if (ops && ops->hw_stream_off)
		ops->hw_stream_off(video);

	vd->status = AML_OFF;
	vd->frm_cnt = 0;
}

static void isp_vb2_discard_done(struct vb2_queue *q)
{
	struct vb2_buffer *vb;
	unsigned long flags;

	spin_lock_irqsave(&q->done_lock, flags);
	list_for_each_entry(vb, &q->done_list, done_entry)
		vb->state = VB2_BUF_STATE_ERROR;
	spin_unlock_irqrestore(&q->done_lock, flags);
}

static void isp_cap_flush_buffer(void *video)
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

	isp_vb2_discard_done(&vd->vb2_q);
	vd->b_current = NULL;

	spin_unlock_irqrestore(&vd->buff_list_lock, flags);
	mdelay(33);
}

static const struct aml_cap_ops isp_cap_ops = {
	.cap_irq_handler = isp_cap_irq_handler,
	.cap_set_format = isp_cap_set_format,
	.cap_cfg_buffer = isp_cap_cfg_buffer,
	.cap_stream_on = isp_cap_stream_on,
	.cap_stream_off = isp_cap_stream_off,
	.cap_flush_buffer = isp_cap_flush_buffer,
};

int aml_isp_video_register(struct isp_dev_t *isp_dev)
{
	int id;
	int rtn = 0;
	struct aml_video *video;

	for (id = 0; id < AML_ISP_STREAM_MAX; id++) {
		video = &isp_dev->video[id];
		video->id = id;
		video->name = stream_name[id];
		video->dev = isp_dev->dev;
		video->v4l2_dev = isp_dev->v4l2_dev;
		video->bus_info = isp_dev->bus_info;
		video->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		video->ops = &isp_cap_ops;
		video->pipe = &isp_dev->pipe;
		video->status = AML_OFF;
		video->priv = isp_dev;

		INIT_LIST_HEAD(&video->head);
		spin_lock_init(&video->buff_list_lock);

		switch (video->id) {
		case AML_ISP_STREAM_DDR:
			video->format = raw_support_format;
			video->fmt_cnt = ARRAY_SIZE(raw_support_format);
			video->pad.flags = MEDIA_PAD_FL_SOURCE;
			video->min_buffer_count = 2;
		break;
		case AML_ISP_STREAM_PARAM:
			video->format = data_support_format;
			video->fmt_cnt = ARRAY_SIZE(data_support_format);
			video->pad.flags = MEDIA_PAD_FL_SOURCE;
			video->min_buffer_count = 1;
		break;
		case AML_ISP_STREAM_STATS:
			video->format = data_support_format;
			video->fmt_cnt = ARRAY_SIZE(data_support_format);
			video->pad.flags = MEDIA_PAD_FL_SINK;
			video->min_buffer_count = 2;
		break;
		case AML_ISP_STREAM_0:
		case AML_ISP_STREAM_1:
		case AML_ISP_STREAM_2:
		case AML_ISP_STREAM_3:
			video->format = img_support_format;
			video->fmt_cnt = ARRAY_SIZE(img_support_format);
			video->pad.flags = MEDIA_PAD_FL_SINK;
			video->min_buffer_count = 2;
			isp_v4l2_ctrl_init(video);
		break;
		case AML_ISP_STREAM_RAW:
			video->format = raw_support_format;
			video->fmt_cnt = ARRAY_SIZE(raw_support_format);
			video->pad.flags = MEDIA_PAD_FL_SINK;
			video->min_buffer_count = 2;
		break;
		default:
			dev_err(video->dev, "Error stream type: %d\n", video->id);
			return -EINVAL;
		}

		rtn = aml_video_register(video);
		if (rtn) {
			dev_err(video->dev, "Failed to register stream-%d: %d\n", id, rtn);
			break;
		}
	}

	return rtn;
}

void aml_isp_video_unregister(struct isp_dev_t *isp_dev)
{
	int i;
	struct aml_video *video;

	for (i = 0; i < AML_ISP_STREAM_MAX; i++) {
		video = &isp_dev->video[i];
		aml_video_unregister(video);
		switch (video->id) {
		case AML_ISP_STREAM_0:
		case AML_ISP_STREAM_1:
		case AML_ISP_STREAM_2:
		case AML_ISP_STREAM_3:
			isp_v4l2_ctrl_deinit(video);
		break;
		default:
		break;
		}
	}
}
