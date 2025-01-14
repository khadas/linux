// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2022 Rockchip Electronics Co., Ltd. */

#define pr_fmt(fmt) "isp_rockit: %s:%d " fmt, __func__, __LINE__

#include <linux/of.h>
#include <linux/of_platform.h>
#include <soc/rockchip/rockchip_rockit.h>

#include "dev.h"
#include "capture.h"
#include "regs.h"

static struct rockit_cfg *rockit_cfg;

struct rkisp_rockit_buffer {
	struct rkisp_buffer isp_buf;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *dba;
	struct sg_table *sgt;
	void *mpi_buf;
	struct list_head queue;
	int buf_id;
	u32 buff_addr;
	void *vaddr;
};

static struct rkisp_stream *rkisp_rockit_get_stream(struct rockit_cfg *input_rockit_cfg)
{
	struct rkisp_device *ispdev = NULL;
	struct rkisp_stream *stream = NULL;
	u8 i;

	if (!rockit_cfg) {
		pr_err("rockit_cfg is null get stream failed\n");
		return NULL;
	}
	if (!input_rockit_cfg) {
		pr_err("input is null get stream failed\n");
		return NULL;
	}

	for (i = 0; i < rockit_cfg->isp_num; i++) {
		if (!strcmp(rockit_cfg->rkisp_dev_cfg[i].isp_name,
			    input_rockit_cfg->current_name)) {
			ispdev = rockit_cfg->rkisp_dev_cfg[i].isp_dev;
			break;
		}
	}

	if (ispdev == NULL) {
		pr_err("Can not find ispdev!");
		return NULL;
	}

	if (ispdev->isp_ver == ISP_V33 &&
	    (input_rockit_cfg->nick_id == 3 || input_rockit_cfg->nick_id == 4))
		return NULL;

	switch (input_rockit_cfg->nick_id) {
	case 0:
		stream = &ispdev->cap_dev.stream[RKISP_STREAM_MP];
		break;
	case 1:
		stream = &ispdev->cap_dev.stream[RKISP_STREAM_SP];
		break;
	case 2:
		stream = &ispdev->cap_dev.stream[RKISP_STREAM_BP];
		break;
	case 3:
		stream = &ispdev->cap_dev.stream[RKISP_STREAM_MPDS];
		break;
	case 4:
		stream = &ispdev->cap_dev.stream[RKISP_STREAM_BPDS];
		break;
	case 5:
		stream = &ispdev->cap_dev.stream[RKISP_STREAM_LUMA];
		break;
	default:
		stream = NULL;
		break;
	}

	return stream;
}

int rkisp_rockit_buf_queue(struct rockit_cfg *input_rockit_cfg)
{
	struct rkisp_stream *stream = NULL;
	struct rkisp_rockit_buffer *isprk_buf = NULL;
	struct rkisp_device *ispdev = NULL;
	int i, height, offset, dev_id;
	struct rkisp_stream_cfg *stream_cfg = NULL;
	unsigned long lock_flags = 0;
	u32 reg, val, bytesperline;

	if (!input_rockit_cfg)
		return -EINVAL;

	stream = rkisp_rockit_get_stream(input_rockit_cfg);
	if (!stream || stream->id >= ROCKIT_STREAM_NUM_MAX) {
		pr_err("inval stream");
		return -EINVAL;
	}

	dev_id = stream->ispdev->dev_id;
	ispdev = stream->ispdev;

	stream_cfg = &rockit_cfg->rkisp_dev_cfg[dev_id].rkisp_stream_cfg[stream->id];
	stream_cfg->node = input_rockit_cfg->node;
	/* invalid dmabuf for wrap mode */
	if (!input_rockit_cfg->buf)
		return -EINVAL;

	for (i = 0; i < ROCKIT_BUF_NUM_MAX; i++) {
		if (stream_cfg->buff_id[i] == input_rockit_cfg->mpi_id) {
			input_rockit_cfg->is_alloc = 0;
			break;
		}
	}

	if (input_rockit_cfg->is_alloc) {
		struct dma_buf_attachment *dba;
		struct sg_table *sgt;
		struct iosys_map map;

		for (i = 0; i < ROCKIT_BUF_NUM_MAX; i++) {
			if (!stream_cfg->buff_id[i] && !stream_cfg->rkisp_buff[i]) {
				stream_cfg->buff_id[i] = input_rockit_cfg->mpi_id;
				isprk_buf = kzalloc(sizeof(struct rkisp_rockit_buffer), GFP_KERNEL);
				if (!isprk_buf) {
					stream_cfg->buff_id[i] = 0;
					pr_err("rkisp_buff alloc failed!\n");
					return -ENOMEM;
				}
				break;
			}
		}
		if (i == ROCKIT_BUF_NUM_MAX)
			return -EINVAL;

		dba = dma_buf_attach(input_rockit_cfg->buf, ispdev->hw_dev->dev);
		if (IS_ERR(dba)) {
			kfree(isprk_buf);
			stream_cfg->buff_id[i] = 0;
			return PTR_ERR(dba);
		}

		sgt = dma_buf_map_attachment(dba, DMA_BIDIRECTIONAL);
		if (IS_ERR(sgt)) {
			dma_buf_detach(input_rockit_cfg->buf, dba);
			kfree(isprk_buf);
			stream_cfg->buff_id[i] = 0;
			return PTR_ERR(sgt);
		}
		isprk_buf->vaddr = NULL;
		if (dma_buf_vmap(input_rockit_cfg->buf, &map) == 0)
			isprk_buf->vaddr = map.vaddr;
		if (rkisp_buf_dbg) {
			u64 *data = isprk_buf->vaddr;

			if (data)
				*data = RKISP_DATA_CHECK;
		}
		isprk_buf->buff_addr = sg_dma_address(sgt->sgl);
		get_dma_buf(input_rockit_cfg->buf);
		isprk_buf->mpi_buf = input_rockit_cfg->mpibuf;
		isprk_buf->dmabuf = input_rockit_cfg->buf;
		isprk_buf->dba = dba;
		isprk_buf->sgt = sgt;
		stream_cfg->rkisp_buff[i] = isprk_buf;
	}

	if (ispdev->cap_dev.wrap_line && stream->id == RKISP_STREAM_MP) {
		if (isprk_buf) {
			val = isprk_buf->buff_addr;
			reg = stream->config->mi.y_base_ad_init;
			rkisp_write(ispdev, reg, val, false);

			bytesperline = stream->out_fmt.plane_fmt[0].bytesperline;
			val += bytesperline * ispdev->cap_dev.wrap_line;
			reg = stream->config->mi.cb_base_ad_init;
			rkisp_write(ispdev, reg, val, false);
			stream->dummy_buf.dma_addr = isprk_buf->buff_addr;
			v4l2_info(&ispdev->v4l2_dev, "rockit wrap buf:0x%x\n", isprk_buf->buff_addr);
		}
		return -EINVAL;
	}

	if (stream_cfg->is_discard && stream->streaming)
		return -EINVAL;

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	isprk_buf = NULL;
	for (i = 0; i < ROCKIT_BUF_NUM_MAX; i++) {
		if (stream_cfg->buff_id[i] == input_rockit_cfg->mpi_id) {
			isprk_buf = stream_cfg->rkisp_buff[i];
			break;
		}
	}

	if (!isprk_buf) {
		spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
		return -EINVAL;
	}

	if (stream->out_isp_fmt.mplanes == 1) {
		u32 y_offs = input_rockit_cfg->y_offset;
		u32 u_offs = input_rockit_cfg->u_offset;
		u32 vir_w = input_rockit_cfg->vir_width;
		u32 dma_addr = isprk_buf->buff_addr;

		if (vir_w)
			stream->out_fmt.plane_fmt[0].bytesperline = vir_w;
		else
			vir_w = stream->out_fmt.plane_fmt[0].bytesperline;
		height = stream->out_fmt.height;
		if (u_offs) {
			offset = u_offs;
			if (stream->out_isp_fmt.output_format == ISP32_MI_OUTPUT_YUV420)
				stream->out_fmt.plane_fmt[1].sizeimage = vir_w * height / 2;
			else
				stream->out_fmt.plane_fmt[1].sizeimage = vir_w * height;
			stream->out_fmt.plane_fmt[0].sizeimage = vir_w * height +
				stream->out_fmt.plane_fmt[1].sizeimage;
		} else {
			offset = vir_w * height;
		}
		isprk_buf->isp_buf.buff_addr[0] = dma_addr + y_offs;
		isprk_buf->isp_buf.buff_addr[1] = dma_addr + offset;
		isprk_buf->isp_buf.vaddr[0] = NULL;
		isprk_buf->isp_buf.vaddr[1] = NULL;
		isprk_buf->isp_buf.vb.vb2_buf.planes[0].mem_priv = NULL;
		if (isprk_buf->vaddr) {
			isprk_buf->isp_buf.vaddr[0] = isprk_buf->vaddr + y_offs;
			isprk_buf->isp_buf.vaddr[1] = isprk_buf->vaddr + offset;
			isprk_buf->isp_buf.vb.vb2_buf.planes[0].mem_priv = isprk_buf->sgt;
		}
	}

	v4l2_dbg(2, rkisp_debug, &ispdev->v4l2_dev,
		 "stream:%d rockit_queue buf:%p y:0x%x uv:0x%x\n",
		 stream->id, isprk_buf,
		 isprk_buf->isp_buf.buff_addr[0], isprk_buf->isp_buf.buff_addr[1]);

	list_add_tail(&isprk_buf->isp_buf.queue, &stream->buf_queue);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);

	return 0;
}

int rkisp_rockit_buf_done(struct rkisp_stream *stream, int cmd, struct rkisp_buffer *curr_buf)
{
	struct rkisp_device *dev = stream->ispdev;
	struct rkisp_rockit_buffer *isprk_buf = NULL;
	struct rkisp_stream_cfg *stream_cfg = NULL;
	u32 seq, dev_id = stream->ispdev->dev_id;
	u64 ns = 0;

	if (!rockit_cfg ||
	    !rockit_cfg->rkisp_rockit_mpibuf_done ||
	    stream->id >= ROCKIT_STREAM_NUM_MAX)
		return -EINVAL;

	stream_cfg = &rockit_cfg->rkisp_dev_cfg[dev_id].rkisp_stream_cfg[stream->id];
	if (cmd == ROCKIT_DVBM_END) {
		isprk_buf =
			container_of(curr_buf, struct rkisp_rockit_buffer, isp_buf);

		rockit_cfg->mpibuf = isprk_buf->mpi_buf;

		rockit_cfg->frame.u64PTS = curr_buf->vb.vb2_buf.timestamp;

		rockit_cfg->frame.u32TimeRef = curr_buf->vb.sequence;
		v4l2_dbg(2, rkisp_debug, &dev->v4l2_dev,
			 "stream:%d seq:%d rockit buf done:0x%x\n",
			 stream->id,
			 curr_buf->vb.sequence,
			 curr_buf->buff_addr[0]);
		if (rkisp_buf_dbg) {
			u64 *data = isprk_buf->vaddr;

			if (data && *data == RKISP_DATA_CHECK)
				v4l2_info(&dev->v4l2_dev,
					  "rockit seq:%d data no update:%llx %llx\n",
					  curr_buf->vb.sequence,
					  *data, *(data + 1));
		}
	} else {
		if (stream->ispdev->cap_dev.wrap_line &&
		    stream->id == RKISP_STREAM_MP) {
			if (dev->skip_frame || stream_cfg->is_discard ||
			    stream->skip_frame || stream->ops->is_stream_stopped(stream)) {
				if (stream->skip_frame)
					stream->skip_frame--;
				return 0;
			}
		} else if (stream_cfg->dst_fps) {
			if (!stream_cfg->is_discard && !stream->curr_buf) {
				rockit_cfg->is_qbuf = true;
			} else {
				rockit_cfg->is_qbuf = false;
				return 0;
			}
		} else {
			return 0;
		}

		rkisp_dmarx_get_frame(stream->ispdev, &seq, NULL, &ns, true);

		if (!ns)
			ns = rkisp_time_get_ns(dev);

		rockit_cfg->frame.u64PTS = ns;

		rockit_cfg->frame.u32TimeRef = seq;
		if (dev->isp_ver == ISP_V33)
			rockit_cfg->frame.ispEncCnt =
				ISP33_ISP2ENC_FRM_CNT(rkisp_read(dev, ISP3X_ISP_DEBUG1, true));
	}

	rockit_cfg->is_color = !rkisp_read(dev, ISP3X_IMG_EFF_CTRL, true);

	rockit_cfg->frame.u32Height = stream->out_fmt.height;

	rockit_cfg->frame.u32Width = stream->out_fmt.width;

	rockit_cfg->frame.enPixelFormat = stream->out_fmt.pixelformat;

	rockit_cfg->frame.u32VirWidth = stream->out_fmt.width;

	rockit_cfg->frame.u32VirHeight = stream->out_fmt.height;

	rockit_cfg->current_name = dev->name;

	rockit_cfg->node = stream_cfg->node;

	rockit_cfg->event = cmd;

	if (list_empty(&stream->buf_queue))
		rockit_cfg->is_empty = true;
	else
		rockit_cfg->is_empty = false;

	if (rockit_cfg->rkisp_rockit_mpibuf_done)
		rockit_cfg->rkisp_rockit_mpibuf_done(rockit_cfg);

	return 0;
}

int rkisp_rockit_pause_stream(struct rockit_cfg *input_rockit_cfg)
{
	struct rkisp_stream *stream = NULL;

	stream = rkisp_rockit_get_stream(input_rockit_cfg);

	if (stream == NULL) {
		pr_err("the stream is NULL");
		return -EINVAL;
	}

	v4l2_dbg(1, rkisp_debug, &stream->ispdev->v4l2_dev,
		 "%s stream:%d\n", __func__, stream->id);

	rockit_isp_ops.rkisp_stream_stop(stream);
	if (stream->ispdev->cap_dev.wrap_line && stream->id == RKISP_STREAM_MP)
		rkisp_dvbm_deinit(stream->ispdev);
	return 0;
}
EXPORT_SYMBOL(rkisp_rockit_pause_stream);

int rkisp_rockit_config_stream(struct rockit_cfg *input_rockit_cfg,
				int width, int height, int wrap_line)
{
	struct rkisp_stream *stream = NULL;
	struct rkisp_buffer *isp_buf, *buf_temp;
	int offset, ret;
	unsigned long lock_flags = 0;
	u32 reg, val, bytesperline;

	stream = rkisp_rockit_get_stream(input_rockit_cfg);

	if (stream == NULL) {
		pr_err("the stream is NULL");
		return -EINVAL;
	}

	v4l2_dbg(1, rkisp_debug, &stream->ispdev->v4l2_dev,
		 "%s stream:%d %dx%d wrap:%d\n",
		 __func__, stream->id, width, height, wrap_line);

	stream->ispdev->cap_dev.wrap_line = wrap_line;
	stream->out_fmt.width = width;
	stream->out_fmt.height = height;
	stream->out_fmt.plane_fmt[0].bytesperline = 0;
	ret = rockit_isp_ops.rkisp_set_fmt(stream, &stream->out_fmt, false);
	if (ret < 0) {
		pr_err("stream id %d config failed\n", stream->id);
		return -EINVAL;
	}
	if (stream->ispdev->cap_dev.wrap_line && stream->id == RKISP_STREAM_MP) {
		rkisp_dvbm_init(stream);
		if (!stream->dummy_buf.mem_priv && stream->dummy_buf.dma_addr) {
			bytesperline = stream->out_fmt.plane_fmt[0].bytesperline;
			val = stream->dummy_buf.dma_addr;
			reg = stream->config->mi.y_base_ad_init;
			rkisp_write(stream->ispdev, reg, val, false);
			val += bytesperline * stream->ispdev->cap_dev.wrap_line;
			reg = stream->config->mi.cb_base_ad_init;
			rkisp_write(stream->ispdev, reg, val, false);
		}
	}

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	if (stream->curr_buf) {
		list_add_tail(&stream->curr_buf->queue, &stream->buf_queue);
		if (stream->curr_buf == stream->next_buf)
			stream->next_buf = NULL;
		stream->curr_buf = NULL;
	}
	if (stream->next_buf) {
		list_add_tail(&stream->next_buf->queue, &stream->buf_queue);
		stream->next_buf = NULL;
	}
	list_for_each_entry_safe(isp_buf, buf_temp, &stream->buf_queue, queue) {
		struct rkisp_rockit_buffer *isprk_buf =
			container_of(isp_buf, struct rkisp_rockit_buffer, isp_buf);

		if (stream->out_isp_fmt.mplanes == 1) {
			u32 y_offs = input_rockit_cfg->y_offset;
			u32 u_offs = input_rockit_cfg->u_offset;
			u32 vir_w = input_rockit_cfg->vir_width;
			u32 dma_addr = isprk_buf->buff_addr;

			if (vir_w)
				stream->out_fmt.plane_fmt[0].bytesperline = vir_w;
			else
				vir_w = stream->out_fmt.plane_fmt[0].bytesperline;
			height = stream->out_fmt.height;
			if (u_offs) {
				offset = u_offs;
				if (stream->out_isp_fmt.output_format == ISP32_MI_OUTPUT_YUV420)
					stream->out_fmt.plane_fmt[1].sizeimage = vir_w * height / 2;
				else
					stream->out_fmt.plane_fmt[1].sizeimage = vir_w * height;
				stream->out_fmt.plane_fmt[0].sizeimage = vir_w * height +
					stream->out_fmt.plane_fmt[1].sizeimage;
			} else {
				offset = vir_w * height;
			}
			isp_buf->buff_addr[0] = dma_addr + y_offs;
			isp_buf->buff_addr[1] = dma_addr + offset;
		}
	}
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);

	return 0;
}
EXPORT_SYMBOL(rkisp_rockit_config_stream);

int rkisp_rockit_resume_stream(struct rockit_cfg *input_rockit_cfg)
{
	struct rkisp_stream *stream = NULL;
	int ret = 0;

	stream = rkisp_rockit_get_stream(input_rockit_cfg);

	if (stream == NULL) {
		pr_err("the stream is NULL");
		return -EINVAL;
	}

	v4l2_dbg(1, rkisp_debug, &stream->ispdev->v4l2_dev,
		 "%s stream:%d\n", __func__, stream->id);

	ret = rockit_isp_ops.rkisp_stream_start(stream);
	if (ret < 0) {
		pr_err("stream id %d start failed\n", stream->id);
		return -EINVAL;
	}
	stream->skip_frame = 2;
	if (stream->ispdev->isp_state == ISP_STOP) {
		stream->ispdev->isp_state = ISP_START;
		rkisp_rdbk_trigger_event(stream->ispdev, T_CMD_QUEUE, NULL);
	}

	return 0;
}
EXPORT_SYMBOL(rkisp_rockit_resume_stream);

int rkisp_rockit_get_tb_stream_info(struct rockit_cfg *input_rockit_cfg,
				    struct rkisp_tb_stream_info *info)
{
	struct rkisp_stream *stream;

	if (!input_rockit_cfg || !info)
		return -EINVAL;

	stream = rkisp_rockit_get_stream(input_rockit_cfg);
	if (!stream)
		return -EINVAL;
	if (info->buf[0].timestamp)
		stream->is_using_resmem = false;
	return rkisp_get_tb_stream_info(stream, info);
}
EXPORT_SYMBOL(rkisp_rockit_get_tb_stream_info);

int rkisp_rockit_free_tb_stream_buf(struct rockit_cfg *input_rockit_cfg)
{
	struct rkisp_stream *stream;

	if (!input_rockit_cfg)
		return -EINVAL;
	stream = rkisp_rockit_get_stream(input_rockit_cfg);
	if (!stream)
		return -EINVAL;

	return rkisp_free_tb_stream_buf(stream);
}
EXPORT_SYMBOL(rkisp_rockit_free_tb_stream_buf);

int rkisp_rockit_free_stream_buf(struct rockit_cfg *input_rockit_cfg)
{
	struct rkisp_stream *stream;
	struct rkisp_buffer *buf;
	unsigned long lock_flags = 0;

	if (!input_rockit_cfg)
		return -EINVAL;
	stream = rkisp_rockit_get_stream(input_rockit_cfg);
	if (!stream)
		return -EINVAL;

	if (stream->streaming)
		return 0;

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	if (stream->curr_buf) {
		list_add_tail(&stream->curr_buf->queue, &stream->buf_queue);
		if (stream->curr_buf == stream->next_buf)
			stream->next_buf = NULL;
		stream->curr_buf = NULL;
	}
	if (stream->next_buf) {
		list_add_tail(&stream->next_buf->queue, &stream->buf_queue);
		stream->next_buf = NULL;
	}

	while (!list_empty(&stream->buf_queue)) {
		buf = list_first_entry(&stream->buf_queue,
			struct rkisp_buffer, queue);
		list_del(&buf->queue);
	}
	rkisp_rockit_buf_state_clear(stream);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
	rkisp_rockit_buf_free(stream);

	return 0;
}
EXPORT_SYMBOL(rkisp_rockit_free_stream_buf);

void rkisp_rockit_buf_state_clear(struct rkisp_stream *stream)
{
	struct rkisp_stream_cfg *stream_cfg;
	u32 i = 0, dev_id = stream->ispdev->dev_id;

	if (!rockit_cfg || stream->id >= ROCKIT_STREAM_NUM_MAX)
		return;

	stream_cfg = &rockit_cfg->rkisp_dev_cfg[dev_id].rkisp_stream_cfg[stream->id];
	stream_cfg->is_discard = false;
	for (i = 0; i < ROCKIT_BUF_NUM_MAX; i++)
		stream_cfg->buff_id[i] = 0;
}

int rkisp_rockit_buf_free(struct rkisp_stream *stream)
{
	struct rkisp_rockit_buffer *isprk_buf;
	struct rkisp_stream_cfg *stream_cfg;
	u32 i = 0, dev_id = stream->ispdev->dev_id;

	if (!rockit_cfg || stream->id >= ROCKIT_STREAM_NUM_MAX)
		return -EINVAL;

	stream_cfg = &rockit_cfg->rkisp_dev_cfg[dev_id].rkisp_stream_cfg[stream->id];
	mutex_lock(&stream_cfg->freebuf_lock);
	for (i = 0; i < ROCKIT_BUF_NUM_MAX; i++) {
		if (stream_cfg->rkisp_buff[i]) {
			isprk_buf = (struct rkisp_rockit_buffer *)stream_cfg->rkisp_buff[i];
			if (isprk_buf->dba) {
				if (isprk_buf->vaddr) {
					struct iosys_map map = IOSYS_MAP_INIT_VADDR(isprk_buf->vaddr);

					dma_buf_vunmap(isprk_buf->dmabuf, &map);
					isprk_buf->vaddr = NULL;
				}
				if (isprk_buf->sgt) {
					dma_buf_unmap_attachment(isprk_buf->dba,
								 isprk_buf->sgt,
								 DMA_BIDIRECTIONAL);
					isprk_buf->sgt = NULL;
				}
				dma_buf_detach(isprk_buf->dmabuf, isprk_buf->dba);
				dma_buf_put(isprk_buf->dmabuf);
				isprk_buf->dba = NULL;
			}
			kfree(stream_cfg->rkisp_buff[i]);
			stream_cfg->rkisp_buff[i] = NULL;
		}
	}
	mutex_unlock(&stream_cfg->freebuf_lock);
	return 0;
}

void rkisp_rockit_dev_init(struct rkisp_device *dev)
{
	struct rkisp_stream_cfg *stream_cfg;
	int i, j;

	if (rockit_cfg == NULL) {
		rockit_cfg = kzalloc(sizeof(struct rockit_cfg), GFP_KERNEL);
		if (rockit_cfg == NULL)
			return;
	}
	rockit_cfg->isp_num = dev->hw_dev->dev_num;
	for (i = 0; i < rockit_cfg->isp_num; i++) {
		if (dev->hw_dev->isp[i]) {
			rockit_cfg->rkisp_dev_cfg[i].isp_name =
				dev->hw_dev->isp[i]->name;
			rockit_cfg->rkisp_dev_cfg[i].isp_dev =
				dev->hw_dev->isp[i];
			for (j = 0; j < RKISP_MAX_STREAM; j++) {
				stream_cfg = &rockit_cfg->rkisp_dev_cfg[i].rkisp_stream_cfg[j];
				mutex_init(&stream_cfg->freebuf_lock);
			}
		}
	}
}

void rkisp_rockit_dev_deinit(void)
{
	if (rockit_cfg) {
		kfree(rockit_cfg);
		rockit_cfg = NULL;
	}
}

int rkisp_rockit_fps_set(int *dst_fps, struct rkisp_stream *stream)
{
	int dev_id = stream->ispdev->dev_id;
	int id = stream->id;

	if (stream->id >= ROCKIT_STREAM_NUM_MAX) {
		pr_err("fps_set stream id %u exceeds maximum\n", id);
		return -EINVAL;
	}

	if (dst_fps == NULL) {
		pr_err("fps_set dst_fps is null\n");
		return -EINVAL;
	}

	if (!rockit_cfg)
		return -EINVAL;

	rockit_cfg->rkisp_dev_cfg[dev_id].rkisp_stream_cfg[id].dst_fps = *dst_fps;
	rockit_cfg->rkisp_dev_cfg[dev_id].rkisp_stream_cfg[id].fps_cnt = *dst_fps;
	return 0;
}

int rkisp_rockit_fps_get(int *dst_fps, struct rkisp_stream *stream)
{
	int dev_id = stream->ispdev->dev_id;
	int id = stream->id;

	if (id >= ROCKIT_STREAM_NUM_MAX) {
		pr_err("fps_get stream id %u exceeds maximum\n", id);
		return -EINVAL;
	}

	if (dst_fps == NULL) {
		pr_err("fps_get dst_fps is null\n");
		return -EINVAL;
	}

	if (!rockit_cfg)
		return -EINVAL;

	*dst_fps = rockit_cfg->rkisp_dev_cfg[dev_id].rkisp_stream_cfg[id].cur_fps;
	return 0;
}

static bool rkisp_rockit_ctrl_fps(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	struct rkisp_sensor_info *sensor = NULL;
	int dev_id = stream->ispdev->dev_id, id = stream->id;
	struct rkisp_stream_cfg *stream_cfg;
	int ret, dst_fps, *fps_cnt;
	static int fps_in, cur_fps[ROCKIT_STREAM_NUM_MAX];
	u32 denominator = 0, numerator = 0;
	bool *is_discard;
	u64 cur_time, *old_time;

	if (!rockit_cfg || stream->id >= ROCKIT_STREAM_NUM_MAX)
		return false;
	stream_cfg = &rockit_cfg->rkisp_dev_cfg[dev_id].rkisp_stream_cfg[id];
	fps_cnt = &stream_cfg->fps_cnt;
	is_discard = &stream_cfg->is_discard;
	old_time = &stream_cfg->old_time;
	dst_fps = stream_cfg->dst_fps;
	if (dst_fps == 0 || !stream->streaming) {
		*is_discard  = false;
		return false;
	}

	if (dev->active_sensor == NULL) {
		*is_discard  = false;
		pr_err("the sensor is not found\n");
		return false;
	}

	sensor = dev->active_sensor;

	ret = v4l2_subdev_call(sensor->sd, video, g_frame_interval, &sensor->fi);
	if (!ret) {
		denominator = sensor->fi.interval.denominator;
		numerator = sensor->fi.interval.numerator;
		if (numerator)
			fps_in = denominator / numerator;
		else {
			*is_discard  = false;
			pr_err("the numerator is 0\n");
			return false;
		}
	}

	if (dst_fps >= fps_in) {
		/* avoid from small frame rate to big frame rate lead to all buf is discard issue */
		*is_discard = false;
		stream_cfg->dst_fps = fps_in;
		return false;
	}

	if ((fps_in > 0) && (dst_fps > 0)) {
		if (*fps_cnt < 0)
			*fps_cnt = fps_in - dst_fps;
		*fps_cnt += dst_fps;

		if (*fps_cnt < fps_in) {
			*is_discard = true;
			if (stream->next_buf || !list_empty(&stream->buf_queue))
				stream->skip_frame = 1;
		} else {
			*fps_cnt -= fps_in;
			*is_discard = false;
			++cur_fps[stream->id];
			cur_time = ktime_get_ns();
			if (cur_time - *old_time >= 1000000000) {
				*old_time = cur_time;
				stream_cfg->cur_fps = cur_fps[stream->id];
				cur_fps[stream->id] = 0;
			}
		}
	} else {
		*is_discard  = false;
	}
	return true;
}

void rkisp_rockit_frame_start(struct rkisp_device *dev)
{
	struct rkisp_stream *stream;
	int i;

	if (rockit_cfg == NULL)
		return;

	for (i = 0; i < RKISP_MAX_STREAM; i++) {
		if (i == RKISP_STREAM_VIR || i == RKISP_STREAM_LUMA)
			continue;
		stream = &dev->cap_dev.stream[i];
		if (!stream->streaming)
			continue;
		rkisp_rockit_buf_done(stream, ROCKIT_DVBM_START, stream->curr_buf);
		rkisp_rockit_ctrl_fps(stream);
	}
}

void *rkisp_rockit_function_register(void *function, int cmd)
{
	if (rockit_cfg == NULL) {
		pr_err("rockit_cfg is null function register failed");
		return NULL;
	}

	switch (cmd) {
	case ROCKIT_BUF_QUE:
		function = rkisp_rockit_buf_queue;
		break;
	case ROCKIT_MPIBUF_DONE:
		rockit_cfg->rkisp_rockit_mpibuf_done = function;
		if (!rockit_cfg->rkisp_rockit_mpibuf_done)
			pr_err("get rkisp_rockit_buf_queue failed!");
		break;
	default:
		break;
	}
	return function;
}
EXPORT_SYMBOL(rkisp_rockit_function_register);

int rkisp_rockit_get_ispdev(char **name)
{
	int i = 0;

	if (rockit_cfg == NULL) {
		pr_err("rockit_cfg is null");
		return -EINVAL;
	}

	if (name == NULL) {
		pr_err("the name is null");
		return -EINVAL;
	}

	for (i = 0; i < rockit_cfg->isp_num; i++)
		name[i] = rockit_cfg->rkisp_dev_cfg[i].isp_name;
	if (name[0] == NULL)
		return -EINVAL;
	else
		return 0;
}
EXPORT_SYMBOL(rkisp_rockit_get_ispdev);

int rkisp_rockit_get_isp_mode(const char *name)
{
	struct rkisp_device *ispdev = NULL;
	int i, ret = -EINVAL;

	if (rockit_cfg == NULL)
		goto end;

	for (i = 0; i < rockit_cfg->isp_num; i++) {
		if (!strcmp(rockit_cfg->rkisp_dev_cfg[i].isp_name, name)) {
			ispdev = rockit_cfg->rkisp_dev_cfg[i].isp_dev;
			break;
		}
	}
	if (!ispdev)
		goto end;

	if (ispdev->is_pre_on) {
		if (IS_HDR_RDBK(ispdev->rd_mode))
			ret = RKISP_FAST_OFFLINE;
		else
			ret = RKISP_FAST_ONLINE;
	} else {
		if (IS_HDR_RDBK(ispdev->rd_mode))
			ret = RKISP_NORMAL_OFFLINE;
		else
			ret = RKISP_NORMAL_ONLINE;
	}

end:
	return ret;
}
EXPORT_SYMBOL(rkisp_rockit_get_isp_mode);
