/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>

#include "aml_vcodec_drv.h"
#include "aml_vcodec_dec.h"
//#include "aml_vcodec_intr.h"
#include "aml_vcodec_util.h"
#include "vdec_drv_if.h"
#include "aml_vcodec_dec_pm.h"
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/crc32.h>
#include "aml_vcodec_adapt.h"

#include "aml_vcodec_vfm.h"

#define OUT_FMT_IDX	0
#define CAP_FMT_IDX	1//3

#define AML_VDEC_MIN_W	64U
#define AML_VDEC_MIN_H	64U
#define DFT_CFG_WIDTH	AML_VDEC_MIN_W
#define DFT_CFG_HEIGHT	AML_VDEC_MIN_H

//#define USEC_PER_SEC 1000000

#define call_void_memop(vb, op, args...)				\
	do {								\
		if ((vb)->vb2_queue->mem_ops->op)			\
			(vb)->vb2_queue->mem_ops->op(args);		\
	} while (0)

static struct aml_video_fmt aml_video_formats[] = {
	{
		.fourcc = V4L2_PIX_FMT_H264,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
/*	{
		.fourcc = V4L2_PIX_FMT_VP8,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_VP9,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},*/
	{
		.fourcc = V4L2_PIX_FMT_NV12,
		.type = AML_FMT_FRAME,
		.num_planes = 2,
	},
};

static const struct aml_codec_framesizes aml_vdec_framesizes[] = {
	{
		.fourcc	= V4L2_PIX_FMT_H264,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 16,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 16 },
	},
/*	{
		.fourcc	= V4L2_PIX_FMT_VP8,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 16,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 16 },
	},
	{
		.fourcc = V4L2_PIX_FMT_VP9,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 16,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 16 },
	},*/
};

#define NUM_SUPPORTED_FRAMESIZE ARRAY_SIZE(aml_vdec_framesizes)
#define NUM_FORMATS ARRAY_SIZE(aml_video_formats)

static struct aml_video_fmt *aml_vdec_find_format(struct v4l2_format *f)
{
	struct aml_video_fmt *fmt;
	unsigned int k;

	for (k = 0; k < NUM_FORMATS; k++) {
		fmt = &aml_video_formats[k];
		if (fmt->fourcc == f->fmt.pix_mp.pixelformat)
			return fmt;
	}

	return NULL;
}

static struct aml_q_data *aml_vdec_get_q_data(struct aml_vcodec_ctx *ctx,
					      enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->q_data[AML_Q_DATA_SRC];

	return &ctx->q_data[AML_Q_DATA_DST];
}

static void aml_vdec_queue_res_chg_event(struct aml_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_src_ch = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes =
		V4L2_EVENT_SRC_CH_RESOLUTION,
	};

	aml_v4l2_debug(4, "[%d] %s()", ctx->id, __func__);
	v4l2_event_queue_fh(&ctx->fh, &ev_src_ch);
}

static void aml_vdec_flush_decoder(struct aml_vcodec_ctx *ctx)
{
	aml_v4l2_debug(3, "[%d] %s() [%d]", ctx->id, __func__, __LINE__);

	aml_decoder_flush(ctx->ada_ctx);
}

static void aml_vdec_pic_info_update(struct aml_vcodec_ctx *ctx)
{
	unsigned int dpbsize = 0;
	int ret;

	if (vdec_if_get_param(ctx,
				GET_PARAM_PIC_INFO,
				&ctx->last_decoded_picinfo)) {
		aml_v4l2_err("[%d] Error!! Cannot get param : GET_PARAM_PICTURE_INFO ERR",
				ctx->id);
		return;
	}

	if (ctx->last_decoded_picinfo.visible_width == 0 ||
		ctx->last_decoded_picinfo.visible_height == 0 ||
		ctx->last_decoded_picinfo.coded_width == 0 ||
		ctx->last_decoded_picinfo.coded_height == 0) {
		aml_v4l2_err("Cannot get correct pic info");
		return;
	}

	if ((ctx->last_decoded_picinfo.visible_width == ctx->picinfo.visible_width) ||
	    (ctx->last_decoded_picinfo.visible_height == ctx->picinfo.visible_height))
		return;

	aml_v4l2_debug(4,
			"[%d]-> new(%d,%d), old(%d,%d), real(%d,%d)",
			ctx->id, ctx->last_decoded_picinfo.visible_width,
			ctx->last_decoded_picinfo.visible_height,
			ctx->picinfo.visible_width, ctx->picinfo.visible_height,
			ctx->last_decoded_picinfo.coded_width,
			ctx->last_decoded_picinfo.coded_width);

	ret = vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpbsize);
	if (dpbsize == 0)
		aml_v4l2_err("[%d] Incorrect dpb size, ret=%d", ctx->id, ret);

	ctx->dpb_size = dpbsize;
}

int get_fb_from_queue(struct aml_vcodec_ctx *ctx, struct vdec_fb **out_fb)
{
	struct vb2_buffer *dst_buf = NULL;
	struct vdec_fb *pfb;
	struct aml_video_dec_buf *dst_buf_info, *info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2;
	int try_cnt = 10;

	aml_v4l2_debug(4, "[%d] %s() [%d]", ctx->id, __func__, __LINE__);

	do {
		dst_buf = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
		if (dst_buf)
			break;
		aml_v4l2_debug(3, "[%d] waitting enough dst buffers.", ctx->id);
		msleep(20);
	} while (try_cnt--);

	if (!dst_buf)
		return -1;

	dst_vb2_v4l2 = container_of(dst_buf, struct vb2_v4l2_buffer, vb2_buf);
	dst_buf_info = container_of(dst_vb2_v4l2, struct aml_video_dec_buf, vb);

	pfb = &dst_buf_info->frame_buffer;
	//pfb->base_y.va = vb2_plane_vaddr(dst_buf, 0);
	pfb->base_y.dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 0);
	pfb->base_y.va = phys_to_virt(dma_to_phys(v4l_get_dev_from_codec_mm(), pfb->base_y.dma_addr));
	pfb->base_y.size = ctx->picinfo.y_len_sz;

	//pfb->base_c.va = vb2_plane_vaddr(dst_buf, 1);
	pfb->base_c.dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 1);
	pfb->base_c.va = phys_to_virt(dma_to_phys(v4l_get_dev_from_codec_mm(), pfb->base_c.dma_addr));
	pfb->base_c.size = ctx->picinfo.c_len_sz;
	pfb->status = FB_ST_NORMAL;

	aml_v4l2_debug(4, "[%d] id=%d Framebuf pfb=%p VA=%p Y_DMA=%pad C_DMA=%pad y_size=%zx, c_size: %zx",
			ctx->id, dst_buf->index, pfb, pfb->base_y.va, &pfb->base_y.dma_addr,
			&pfb->base_c.dma_addr, pfb->base_y.size, pfb->base_c.size);

	//mutex_lock(&ctx->lock);
	dst_buf_info->used = true;
	//mutex_unlock(&ctx->lock);

	v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	*out_fb = pfb;

	info = container_of(pfb, struct aml_video_dec_buf, frame_buffer);

	return 0;
}
EXPORT_SYMBOL(get_fb_from_queue);

int put_fb_to_queue(struct aml_vcodec_ctx *ctx, struct vdec_fb *in_fb)
{
	struct aml_video_dec_buf *dstbuf;

	pr_info("[%d] %s() [%d]\n", ctx->id, __func__, __LINE__);

	if (in_fb == NULL) {
		aml_v4l2_debug(4, "[%d] No free frame buffer", ctx->id);
		return -1;
	}

	aml_v4l2_debug(4, "[%d] tmp_frame_addr = 0x%p", ctx->id, in_fb);

	dstbuf = container_of(in_fb, struct aml_video_dec_buf, frame_buffer);

	mutex_lock(&ctx->lock);

	if (!dstbuf->used)
		goto out;

	aml_v4l2_debug(4,
			"[%d] status=%x queue id=%d to rdy_queue",
			ctx->id, in_fb->status,
			dstbuf->vb.vb2_buf.index);

	v4l2_m2m_buf_queue(ctx->m2m_ctx, &dstbuf->vb);

	dstbuf->used = false;
out:
	mutex_unlock(&ctx->lock);

	return 0;

}
EXPORT_SYMBOL(put_fb_to_queue);

void trans_vframe_to_user(struct aml_vcodec_ctx *ctx, struct vdec_fb *fb)
{
	int cnt;
	struct aml_video_dec_buf *dstbuf = NULL;
	struct vframe_s *vf = (struct vframe_s *)fb->vf_handle;

	dstbuf = container_of(fb, struct aml_video_dec_buf, frame_buffer);

	aml_v4l2_debug(4,"%s() [%d], base_y: %zu, idx: %u\n",
		__FUNCTION__, __LINE__, fb->base_y.size, dstbuf->vb.vb2_buf.index);
	aml_v4l2_debug(4,"%s() [%d], base_c: %zu, idx: %u\n",
		__FUNCTION__, __LINE__, fb->base_c.size, dstbuf->vb.vb2_buf.index);

	dstbuf->vb.vb2_buf.timestamp = vf->timestamp;
	aml_v4l2_debug(4, "[%d] %s() [%d], timestamp: %llx",
		ctx->id, __func__, __LINE__, vf->timestamp);

	vb2_set_plane_payload(&dstbuf->vb.vb2_buf, 0, fb->base_y.bytes_used);
	vb2_set_plane_payload(&dstbuf->vb.vb2_buf, 1, fb->base_c.bytes_used);

	dstbuf->ready_to_display = true;

	aml_v4l2_debug(4, "[%d]status=%x queue id=%d to done_list %d",
			ctx->id, fb->status, dstbuf->vb.vb2_buf.index,
			dstbuf->queued_in_vb2);

	cnt = atomic_read(&dstbuf->vb.vb2_buf.vb2_queue->owned_by_drv_count);
	aml_v4l2_debug(4, "[%d] %s() [%d], owned_by_drv_count: %d",
		ctx->id, __func__, __LINE__, cnt);
	aml_v4l2_debug(4, "[%d] %s() [%d], y_va: %p, vf_h: %lx",
		ctx->id, __func__, __LINE__, dstbuf->frame_buffer.base_y.va,
		dstbuf->frame_buffer.vf_handle);

	if (vf->flag == VFRAME_FLAG_EMPTY_FRAME_V4L) {
		dstbuf->lastframe = true;
		dstbuf->vb.flags = V4L2_BUF_FLAG_LAST;
		vb2_set_plane_payload(&dstbuf->vb.vb2_buf, 0, 0);
		vb2_set_plane_payload(&dstbuf->vb.vb2_buf, 1, 0);
		ctx->has_receive_eos = true;
		pr_info("[%d] recevie a empty frame.\n", ctx->id);
	}

	v4l2_m2m_buf_done(&dstbuf->vb, VB2_BUF_STATE_DONE);

	mutex_lock(&ctx->state_lock);
	if (ctx->state == AML_STATE_FLUSHING &&
		ctx->has_receive_eos) {
		ctx->state = AML_STATE_FLUSHED;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_FLUSHED)",
			ctx->id, __func__);
	}
	mutex_unlock(&ctx->state_lock);

	ctx->decoded_frame_cnt++;
}

static int get_display_buffer(struct aml_vcodec_ctx *ctx, struct vdec_fb **out)
{
	int ret = -1;

	aml_v4l2_debug(4, "[%d] %s()", ctx->id, __func__);

	ret = vdec_if_get_param(ctx, GET_PARAM_DISP_FRAME_BUFFER, out);
	if (ret) {
		aml_v4l2_err("[%d] Cannot get param : GET_PARAM_DISP_FRAME_BUFFER", ctx->id);
		return -1;
	}

	if (!*out) {
		aml_v4l2_debug(4, "[%d] No display frame buffer", ctx->id);
		return -1;
	}

	return ret;
}

static int is_vdec_ready(struct aml_vcodec_ctx *ctx)
{
	struct aml_vcodec_dev *dev = ctx->dev;

	if (!is_input_ready(ctx->ada_ctx)) {
		pr_err("[%d] %s() the decoder intput has not ready.\n",
			ctx->id, __func__);
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		return 0;
	}

	if (ctx->state == AML_STATE_PROBE) {
		int buf_ready_num;

		/* is there enough dst bufs for decoding? */
		buf_ready_num = v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx);
		if (buf_ready_num < ctx->dpb_size) {
			aml_v4l2_debug(4, "[%d] Not enough dst bufs, num: %d.\n",
				ctx->id, buf_ready_num);
			v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
			v4l2_m2m_try_schedule(ctx->m2m_ctx);
			msleep(60);
			return 0;
		}

		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_PROBE) {
			ctx->state = AML_STATE_READY;
			aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_READY)",
				ctx->id, __func__);
		}
		mutex_unlock(&ctx->state_lock);
	}

	mutex_lock(&ctx->state_lock);
	if (ctx->state == AML_STATE_READY) {
		ctx->state = AML_STATE_ACTIVE;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_ACTIVE)",
			ctx->id, __func__);
	}
	mutex_unlock(&ctx->state_lock);

	return 1;
}

static void aml_vdec_worker(struct work_struct *work)
{
	struct aml_vcodec_ctx *ctx =
		container_of(work, struct aml_vcodec_ctx, decode_work);
	struct aml_vcodec_dev *dev = ctx->dev;
	struct vb2_buffer *src_buf;
	struct aml_vcodec_mem buf;
	bool res_chg = false;
	int ret;
	struct aml_video_dec_buf *src_buf_info;
	struct vb2_v4l2_buffer *src_vb2_v4l2;

	aml_v4l2_debug(4, "[%d] entry [%d] [%s]", ctx->id, __LINE__, __func__);

	aml_vdec_lock(ctx);

	if (ctx->state < AML_STATE_INIT ||
		ctx->state > AML_STATE_FLUSHED) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		aml_vdec_unlock(ctx);
		return;
	}

	if (!is_vdec_ready(ctx)) {
		pr_err("[%d] %s() the decoder has not ready.\n",
			ctx->id, __func__);
		aml_vdec_unlock(ctx);
		return;
	}

	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (src_buf == NULL) {
		pr_err("[%d] src_buf empty!\n", ctx->id);
		aml_vdec_unlock(ctx);
		return;
	}

	src_vb2_v4l2 = container_of(src_buf, struct vb2_v4l2_buffer, vb2_buf);
	src_buf_info = container_of(src_vb2_v4l2, struct aml_video_dec_buf, vb);

	if (src_buf_info->lastframe) {
		//the empty data use to flushed the decoder.
		aml_v4l2_debug(3, "[%d] Got empty flush input buffer.", ctx->id);

		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);

		/* sets eos data for vdec input. */
		aml_vdec_flush_decoder(ctx);

		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_ACTIVE) {
			ctx->state = AML_STATE_FLUSHING;// prepare flushing
			aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_FLUSHING)",
				ctx->id, __func__);
		}
		mutex_unlock(&ctx->state_lock);

		aml_vdec_unlock(ctx);
		return;
	}

	buf.va = vb2_plane_vaddr(src_buf, 0);
	buf.dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	//buf.va = phys_to_virt(dma_to_phys(v4l_get_dev_from_codec_mm(), buf.dma_addr));

	buf.size = (size_t)src_buf->planes[0].bytesused;
	if (!buf.va) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		aml_v4l2_err("[%d] id=%d src_addr is NULL!!",
				ctx->id, src_buf->index);
		aml_vdec_unlock(ctx);
		return;
	}
	aml_v4l2_debug(4, "[%d] Bitstream VA=%p DMA=%pad Size=%zx vb=%p",
			ctx->id, buf.va, &buf.dma_addr, buf.size, src_buf);

	src_buf_info->used = true;

	//pr_err("%s() [%d], size: 0x%zx, crc: 0x%x\n",
		//__func__, __LINE__, buf.size, crc32(0, buf.va, buf.size));

	/* pts = (time / 10e6) * (90k / fps) */
	aml_v4l2_debug(4, "[%d] %s() timestamp: 0x%llx", ctx->id , __func__,
		src_buf->timestamp);

	ret = vdec_if_decode(ctx, &buf, src_buf->timestamp, &res_chg);
	if (ret > 0) {
		/*
		 * we only return src buffer with VB2_BUF_STATE_DONE
		 * when decode success without resolution change
		 */
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(&src_buf_info->vb, VB2_BUF_STATE_DONE);
	} else if (ret == -EAGAIN) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		aml_vdec_unlock(ctx);
		return;
	} else {
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		if (ret == -EIO) {
			//mutex_lock(&ctx->lock);
			src_buf_info->error = true;
			//mutex_unlock(&ctx->lock);
		}
		v4l2_m2m_buf_done(&src_buf_info->vb, VB2_BUF_STATE_ERROR);
	}

	if (!ret && res_chg) {
		if (0) aml_vdec_pic_info_update(ctx);
		/*
		 * On encountering a resolution change in the stream.
		 * The driver must first process and decode all
		 * remaining buffers from before the resolution change
		 * point, so call flush decode here
		 */
		if (0) aml_vdec_flush_decoder(ctx);
		/*
		 * After all buffers containing decoded frames from
		 * before the resolution change point ready to be
		 * dequeued on the CAPTURE queue, the driver sends a
		 * V4L2_EVENT_SOURCE_CHANGE event for source change
		 * type V4L2_EVENT_SRC_CH_RESOLUTION
		 */
		if (0) aml_vdec_queue_res_chg_event(ctx);
	}

	v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
	aml_vdec_unlock(ctx);
}

static void aml_reset_worker(struct work_struct *work)
{
	struct aml_vcodec_ctx *ctx =
		container_of(work, struct aml_vcodec_ctx, reset_work);
	struct aml_video_dec_buf *buf = NULL;

	aml_vdec_lock(ctx);

	if (ctx->state == AML_STATE_ABORT) {
		pr_err("[%d] %s() the decoder will be exited.\n",
			ctx->id, __func__);
		aml_vdec_unlock(ctx);
		return;
	}

	/* fast enque capture buffers. */
	list_for_each_entry(buf, &ctx->capture_list, node) {
		if (buf->que_in_m2m)
			v4l2_m2m_buf_queue(ctx->m2m_ctx, &buf->vb);
	}

	if (aml_codec_reset(ctx->ada_ctx)) {
		ctx->state = AML_STATE_ABORT;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_ABORT)",
			ctx->id, __func__);
		aml_vdec_unlock(ctx);
		return;
	}

	mutex_lock(&ctx->state_lock);
	if (ctx->state ==  AML_STATE_RESET) {
		ctx->state = AML_STATE_READY;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_READY)",
			ctx->id, __func__);

		v4l2_m2m_try_schedule(ctx->m2m_ctx);
	}
	mutex_unlock(&ctx->state_lock);

	aml_vdec_unlock(ctx);
}

void try_to_capture(struct aml_vcodec_ctx *ctx)
{
	int ret = 0;
	struct vdec_fb *fb = NULL;

	ret = get_display_buffer(ctx, &fb);
	if (ret) {
		aml_v4l2_debug(4, "[%d] %s() [%d], the que have no disp buf,ret: %d",
			ctx->id, __func__, __LINE__, ret);
		return;
	}

	trans_vframe_to_user(ctx, fb);
}
EXPORT_SYMBOL_GPL(try_to_capture);

static int vdec_thread(void *data)
{
	struct aml_vdec_thread *thread =
		(struct aml_vdec_thread *) data;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *) thread->priv;

	for (;;) {
		aml_v4l2_debug(3, "[%d] %s() state: %d", ctx->id,
			__func__, ctx->state);

		if (down_interruptible(&thread->sem))
			break;

		if (thread->stop)
			break;

		thread->func(ctx);
	}

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	return 0;
}

void aml_thread_notify(struct aml_vcodec_ctx *ctx,
	enum aml_thread_type type)
{
	struct aml_vdec_thread *thread = NULL;

	list_for_each_entry(thread, &ctx->vdec_thread_list, node) {
		if (thread->task == NULL)
			continue;

		if (thread->type == type)
			up(&thread->sem);
	}
}
EXPORT_SYMBOL_GPL(aml_thread_notify);

int aml_thread_start(struct aml_vcodec_ctx *ctx, aml_thread_func func,
	enum aml_thread_type type, const char *thread_name)
{
	struct aml_vdec_thread *thread;
	int ret = 0;

	thread = kzalloc(sizeof(*thread), GFP_KERNEL);
	if (thread == NULL)
		return -ENOMEM;

	thread->type = type;
	thread->func = func;
	thread->priv = ctx;
	sema_init(&thread->sem, 0);

	thread->task = kthread_run(vdec_thread, thread, "aml-%s", thread_name);
	if (IS_ERR(thread->task)) {
		ret = PTR_ERR(thread->task);
		thread->task = NULL;
		goto err;
	}

	list_add(&thread->node, &ctx->vdec_thread_list);

	return 0;

err:
	kfree(thread);

	return ret;
}
EXPORT_SYMBOL_GPL(aml_thread_start);

void aml_thread_stop(struct aml_vcodec_ctx *ctx)
{
	struct aml_vdec_thread *thread = NULL;

	while (!list_empty(&ctx->vdec_thread_list)) {
		thread = list_entry(ctx->vdec_thread_list.next,
			struct aml_vdec_thread, node);
		list_del(&thread->node);
		thread->stop = true;
		up(&thread->sem);
		kthread_stop(thread->task);
		thread->task = NULL;
		kfree(thread);
	}
}
EXPORT_SYMBOL_GPL(aml_thread_stop);

static int vidioc_try_decoder_cmd(struct file *file, void *priv,
				struct v4l2_decoder_cmd *cmd)
{
	switch (cmd->cmd) {
	case V4L2_DEC_CMD_STOP:
	case V4L2_DEC_CMD_START:
		if (cmd->flags != 0) {
			aml_v4l2_err("cmd->flags=%u", cmd->flags);
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int vidioc_decoder_cmd(struct file *file, void *priv,
				struct v4l2_decoder_cmd *cmd)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *src_vq, *dst_vq;
	int ret;

	ret = vidioc_try_decoder_cmd(file, priv, cmd);
	if (ret)
		return ret;

	aml_v4l2_debug(3, "[%d] %s() [%d], cmd: %u",
		ctx->id, __func__, __LINE__, cmd->cmd);
	dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	switch (cmd->cmd) {
	case V4L2_DEC_CMD_STOP:

		if (ctx->state != AML_STATE_ACTIVE)
			return 0;

		src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		if (!vb2_is_streaming(src_vq)) {
			pr_err("[%d] Output stream is off. No need to flush.\n", ctx->id);
			return 0;
		}

		if (!vb2_is_streaming(dst_vq)) {
			pr_err("[%d] Capture stream is off. No need to flush.\n", ctx->id);
			return 0;
		}

		/* flush src */
		v4l2_m2m_buf_queue(ctx->m2m_ctx, &ctx->empty_flush_buf->vb);
		v4l2_m2m_try_schedule(ctx->m2m_ctx);//pay attention

		break;

	case V4L2_DEC_CMD_START:
		aml_v4l2_debug(4, "[%d] CMD V4L2_DEC_CMD_START ", ctx->id);
		vb2_clear_last_buffer_dequeued(dst_vq);//pay attention
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int vidioc_decoder_streamon(struct file *file, void *priv,
	enum v4l2_buf_type i)
{
	struct v4l2_fh *fh = file->private_data;
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct aml_vcodec_dev *dev = ctx->dev;
	struct vb2_queue *q;
	int ret = 0;

	ret = v4l2_m2m_ioctl_streamon(file, priv, i);

	q = v4l2_m2m_get_vq(fh->m2m_ctx, i);

	mutex_lock(&ctx->state_lock);
	if (ctx->state == AML_STATE_FLUSHED) {
		if (!V4L2_TYPE_IS_OUTPUT(q->type)) {
			ctx->state = AML_STATE_RESET;
			aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_RESET)",
				ctx->id, __func__);
			queue_work(dev->reset_workqueue, &ctx->reset_work);
		}
	}
	mutex_unlock(&ctx->state_lock);

	return ret;
}

void aml_vdec_unlock(struct aml_vcodec_ctx *ctx)
{
	mutex_unlock(&ctx->dev->dec_mutex);
}

void aml_vdec_lock(struct aml_vcodec_ctx *ctx)
{
	mutex_lock(&ctx->dev->dec_mutex);
}

void aml_vcodec_dec_release(struct aml_vcodec_ctx *ctx)
{
	aml_vdec_lock(ctx);

	ctx->state = AML_STATE_ABORT;
	aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_ABORT)",
		ctx->id, __func__);

	vdec_if_deinit(ctx);

	aml_vdec_unlock(ctx);
}

void aml_vcodec_dec_set_default_params(struct aml_vcodec_ctx *ctx)
{
	struct aml_q_data *q_data;

	ctx->m2m_ctx->q_lock = &ctx->dev->dev_mutex;
	ctx->fh.m2m_ctx = ctx->m2m_ctx;
	ctx->fh.ctrl_handler = &ctx->ctrl_hdl;
	INIT_WORK(&ctx->decode_work, aml_vdec_worker);
	INIT_WORK(&ctx->reset_work, aml_reset_worker);
	ctx->colorspace = V4L2_COLORSPACE_REC709;
	ctx->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	ctx->quantization = V4L2_QUANTIZATION_DEFAULT;
	ctx->xfer_func = V4L2_XFER_FUNC_DEFAULT;
	ctx->dev->dec_capability = VCODEC_CAPABILITY_4K_DISABLED;//disable 4k

	q_data = &ctx->q_data[AML_Q_DATA_SRC];
	memset(q_data, 0, sizeof(struct aml_q_data));
	q_data->visible_width = DFT_CFG_WIDTH;
	q_data->visible_height = DFT_CFG_HEIGHT;
	q_data->fmt = &aml_video_formats[OUT_FMT_IDX];
	q_data->field = V4L2_FIELD_NONE;

	q_data->sizeimage[0] = (1024 * 1024);//DFT_CFG_WIDTH * DFT_CFG_HEIGHT; //1m
	q_data->bytesperline[0] = 0;

	q_data = &ctx->q_data[AML_Q_DATA_DST];
	memset(q_data, 0, sizeof(struct aml_q_data));
	q_data->visible_width = DFT_CFG_WIDTH;
	q_data->visible_height = DFT_CFG_HEIGHT;
	q_data->coded_width = DFT_CFG_WIDTH;
	q_data->coded_height = DFT_CFG_HEIGHT;
	q_data->fmt = &aml_video_formats[CAP_FMT_IDX];
	q_data->field = V4L2_FIELD_NONE;

	v4l_bound_align_image(&q_data->coded_width,
				AML_VDEC_MIN_W,
				AML_VDEC_MAX_W, 4,
				&q_data->coded_height,
				AML_VDEC_MIN_H,
				AML_VDEC_MAX_H, 5, 6);

	q_data->sizeimage[0] = q_data->coded_width * q_data->coded_height;
	q_data->bytesperline[0] = q_data->coded_width;
	q_data->sizeimage[1] = q_data->sizeimage[0] / 2;
	q_data->bytesperline[1] = q_data->coded_width;

	ctx->state = AML_STATE_IDLE;
	aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_IDLE)",
		ctx->id, __func__);
}

static int vidioc_vdec_qbuf(struct file *file, void *priv,
			    struct v4l2_buffer *buf)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (ctx->state == AML_STATE_ABORT) {
		aml_v4l2_err("[%d] Call on QBUF after unrecoverable error",
				ctx->id);
		return -EIO;
	}

	return v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);
}

static int vidioc_vdec_dqbuf(struct file *file, void *priv,
			     struct v4l2_buffer *buf)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (ctx->state == AML_STATE_ABORT) {
		aml_v4l2_err("[%d] Call on DQBUF after unrecoverable error",
				ctx->id);
		return -EIO;
	}

	return v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
}

static int vidioc_vdec_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	strlcpy(cap->driver, AML_VCODEC_DEC_NAME, sizeof(cap->driver));
	strlcpy(cap->bus_info, AML_PLATFORM_STR, sizeof(cap->bus_info));
	strlcpy(cap->card, AML_PLATFORM_STR, sizeof(cap->card));

	return 0;
}

static int vidioc_vdec_subscribe_evt(struct v4l2_fh *fh,
				     const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 2, NULL);
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	default:
		return v4l2_ctrl_subscribe_event(fh, sub);
	}
}

static int vidioc_try_fmt(struct v4l2_format *f, struct aml_video_fmt *fmt)
{
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	int i;

	pix_fmt_mp->field = V4L2_FIELD_NONE;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		pix_fmt_mp->num_planes = 1;
		pix_fmt_mp->plane_fmt[0].bytesperline = 0;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		int tmp_w, tmp_h;

		pix_fmt_mp->height = clamp(pix_fmt_mp->height,
					AML_VDEC_MIN_H,
					AML_VDEC_MAX_H);
		pix_fmt_mp->width = clamp(pix_fmt_mp->width,
					AML_VDEC_MIN_W,
					AML_VDEC_MAX_W);

		/*
		 * Find next closer width align 64, heign align 64, size align
		 * 64 rectangle
		 * Note: This only get default value, the real HW needed value
		 *       only available when ctx in AML_STATE_PROBE state
		 */
		tmp_w = pix_fmt_mp->width;
		tmp_h = pix_fmt_mp->height;
		v4l_bound_align_image(&pix_fmt_mp->width,
					AML_VDEC_MIN_W,
					AML_VDEC_MAX_W, 6,
					&pix_fmt_mp->height,
					AML_VDEC_MIN_H,
					AML_VDEC_MAX_H, 6, 9);

		if (pix_fmt_mp->width < tmp_w &&
			(pix_fmt_mp->width + 64) <= AML_VDEC_MAX_W)
			pix_fmt_mp->width += 64;
		if (pix_fmt_mp->height < tmp_h &&
			(pix_fmt_mp->height + 64) <= AML_VDEC_MAX_H)
			pix_fmt_mp->height += 64;

		aml_v4l2_debug(4,
			"before resize width=%d, height=%d, after resize width=%d, height=%d, sizeimage=%d",
			tmp_w, tmp_h, pix_fmt_mp->width,
			pix_fmt_mp->height,
			pix_fmt_mp->width * pix_fmt_mp->height);

		pix_fmt_mp->num_planes = fmt->num_planes;
		pix_fmt_mp->plane_fmt[0].sizeimage =
				pix_fmt_mp->width * pix_fmt_mp->height;
		pix_fmt_mp->plane_fmt[0].bytesperline = pix_fmt_mp->width;

		if (pix_fmt_mp->num_planes == 2) {
			pix_fmt_mp->plane_fmt[1].sizeimage =
				(pix_fmt_mp->width * pix_fmt_mp->height) / 2;
			pix_fmt_mp->plane_fmt[1].bytesperline =
				pix_fmt_mp->width;
		}
	}

	for (i = 0; i < pix_fmt_mp->num_planes; i++)
		memset(&(pix_fmt_mp->plane_fmt[i].reserved[0]), 0x0,
			   sizeof(pix_fmt_mp->plane_fmt[0].reserved));

	pix_fmt_mp->flags = 0;
	memset(&pix_fmt_mp->reserved, 0x0, sizeof(pix_fmt_mp->reserved));
	return 0;
}

static int vidioc_try_fmt_vid_cap_mplane(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct aml_video_fmt *fmt;

	fmt = aml_vdec_find_format(f);
	if (!fmt) {
		f->fmt.pix.pixelformat = aml_video_formats[CAP_FMT_IDX].fourcc;
		fmt = aml_vdec_find_format(f);
	}

	return vidioc_try_fmt(f, fmt);
}

static int vidioc_try_fmt_vid_out_mplane(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	struct aml_video_fmt *fmt;

	fmt = aml_vdec_find_format(f);
	if (!fmt) {
		f->fmt.pix.pixelformat = aml_video_formats[OUT_FMT_IDX].fourcc;
		fmt = aml_vdec_find_format(f);
	}

	if (pix_fmt_mp->plane_fmt[0].sizeimage == 0) {
		aml_v4l2_err("sizeimage of output format must be given");
		return -EINVAL;
	}

	return vidioc_try_fmt(f, fmt);
}

static int vidioc_vdec_g_selection(struct file *file, void *priv,
			struct v4l2_selection *s)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct aml_q_data *q_data;

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	q_data = &ctx->q_data[AML_Q_DATA_DST];

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.visible_width;
		s->r.height = ctx->picinfo.visible_height;
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.coded_width;
		s->r.height = ctx->picinfo.coded_height;
		break;
	case V4L2_SEL_TGT_COMPOSE:
		if (vdec_if_get_param(ctx, GET_PARAM_CROP_INFO, &(s->r))) {
			/* set to default value if header info not ready yet*/
			s->r.left = 0;
			s->r.top = 0;
			s->r.width = q_data->visible_width;
			s->r.height = q_data->visible_height;
		}
		break;
	default:
		return -EINVAL;
	}

	if (ctx->state < AML_STATE_PROBE) {
		/* set to default value if header info not ready yet*/
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = q_data->visible_width;
		s->r.height = q_data->visible_height;
		return 0;
	}

	return 0;
}

static int vidioc_vdec_s_selection(struct file *file, void *priv,
				struct v4l2_selection *s)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.visible_width;
		s->r.height = ctx->picinfo.visible_height;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vidioc_vdec_s_fmt(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp;
	struct aml_q_data *q_data;
	int ret = 0;
	struct aml_video_fmt *fmt;

	aml_v4l2_debug(4, "[%d] %s()", ctx->id, __func__);

	q_data = aml_vdec_get_q_data(ctx, f->type);
	if (!q_data)
		return -EINVAL;

	pix_mp = &f->fmt.pix_mp;
	if ((f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) &&
	    vb2_is_busy(&ctx->m2m_ctx->out_q_ctx.q)) {
		aml_v4l2_err("[%d] out_q_ctx buffers already requested", ctx->id);
	}

	if ((f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
	    vb2_is_busy(&ctx->m2m_ctx->cap_q_ctx.q)) {
		aml_v4l2_err("[%d] cap_q_ctx buffers already requested", ctx->id);
	}

	fmt = aml_vdec_find_format(f);
	if (fmt == NULL) {
		if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			f->fmt.pix.pixelformat =
				aml_video_formats[OUT_FMT_IDX].fourcc;
			fmt = aml_vdec_find_format(f);
		} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			f->fmt.pix.pixelformat =
				aml_video_formats[CAP_FMT_IDX].fourcc;
			fmt = aml_vdec_find_format(f);
		}
	}

	q_data->fmt = fmt;
	vidioc_try_fmt(f, q_data->fmt);
	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		q_data->sizeimage[0] = pix_mp->plane_fmt[0].sizeimage;
		q_data->coded_width = pix_mp->width;
		q_data->coded_height = pix_mp->height;

		aml_v4l2_debug(4, "[%d] %s() [%d], w: %d, h: %d, size: %d",
			ctx->id, __func__, __LINE__, pix_mp->width, pix_mp->height,
			pix_mp->plane_fmt[0].sizeimage);

		ctx->colorspace = f->fmt.pix_mp.colorspace;
		ctx->ycbcr_enc = f->fmt.pix_mp.ycbcr_enc;
		ctx->quantization = f->fmt.pix_mp.quantization;
		ctx->xfer_func = f->fmt.pix_mp.xfer_func;

		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_IDLE) {
			ret = vdec_if_init(ctx, q_data->fmt->fourcc);
			if (ret) {
				aml_v4l2_err("[%d]: vdec_if_init() fail ret=%d",
					ctx->id, ret);
				mutex_unlock(&ctx->state_lock);
				return -EINVAL;
			}
			ctx->state = AML_STATE_INIT;
			aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_INIT)",
				ctx->id, __func__);
		}
		mutex_unlock(&ctx->state_lock);
	}

	return 0;
}

static int vidioc_enum_framesizes(struct file *file, void *priv,
				struct v4l2_frmsizeenum *fsize)
{
	int i = 0;
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (fsize->index != 0)
		return -EINVAL;

	for (i = 0; i < NUM_SUPPORTED_FRAMESIZE; ++i) {
		if (fsize->pixel_format != aml_vdec_framesizes[i].fourcc)
			continue;

		fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
		fsize->stepwise = aml_vdec_framesizes[i].stepwise;
		if (!(ctx->dev->dec_capability &
				VCODEC_CAPABILITY_4K_DISABLED)) {
			aml_v4l2_debug(4, "[%d] 4K is enabled", ctx->id);
			fsize->stepwise.max_width =
					VCODEC_DEC_4K_CODED_WIDTH;
			fsize->stepwise.max_height =
					VCODEC_DEC_4K_CODED_HEIGHT;
		}
		aml_v4l2_debug(4, "[%d] %x, %d %d %d %d %d %d",
				ctx->id, ctx->dev->dec_capability,
				fsize->stepwise.min_width,
				fsize->stepwise.max_width,
				fsize->stepwise.step_width,
				fsize->stepwise.min_height,
				fsize->stepwise.max_height,
				fsize->stepwise.step_height);
		return 0;
	}

	return -EINVAL;
}

static int vidioc_enum_fmt(struct v4l2_fmtdesc *f, bool output_queue)
{
	struct aml_video_fmt *fmt;
	int i, j = 0;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (output_queue && (aml_video_formats[i].type != AML_FMT_DEC))
			continue;
		if (!output_queue &&
			(aml_video_formats[i].type != AML_FMT_FRAME))
			continue;

		if (j == f->index)
			break;
		++j;
	}

	if (i == NUM_FORMATS)
		return -EINVAL;

	fmt = &aml_video_formats[i];
	f->pixelformat = fmt->fourcc;

	return 0;
}

static int vidioc_vdec_enum_fmt_vid_cap_mplane(struct file *file, void *pirv,
					       struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, false);
}

static int vidioc_vdec_enum_fmt_vid_out_mplane(struct file *file, void *priv,
					       struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, true);
}

static int vidioc_vdec_g_fmt(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct vb2_queue *vq;
	struct aml_q_data *q_data;

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq) {
		aml_v4l2_err("[%d] no vb2 queue for type=%d", ctx->id, f->type);
		return -EINVAL;
	}

	q_data = aml_vdec_get_q_data(ctx, f->type);

	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->colorspace = ctx->colorspace;
	pix_mp->ycbcr_enc = ctx->ycbcr_enc;
	pix_mp->quantization = ctx->quantization;
	pix_mp->xfer_func = ctx->xfer_func;

	if ((f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
	    (ctx->state >= AML_STATE_PROBE)) {
		/* Until STREAMOFF is called on the CAPTURE queue
		 * (acknowledging the event), the driver operates as if
		 * the resolution hasn't changed yet.
		 * So we just return picinfo yet, and update picinfo in
		 * stop_streaming hook function
		 */
		/* it is used for alloc the decode buffer size. */
		q_data->sizeimage[0] = ctx->picinfo.y_len_sz;
		q_data->sizeimage[1] = ctx->picinfo.c_len_sz;

		/* it is used for alloc the EGL image buffer size. */
		q_data->coded_width = ctx->picinfo.coded_width;
		q_data->coded_height = ctx->picinfo.coded_height;

		q_data->bytesperline[0] = ctx->picinfo.coded_width;
		q_data->bytesperline[1] = ctx->picinfo.coded_height;

		/*
		 * Width and height are set to the dimensions
		 * of the movie, the buffer is bigger and
		 * further processing stages should crop to this
		 * rectangle.
		 */
		pix_mp->width = q_data->coded_width;
		pix_mp->height = q_data->coded_height;

		/*
		 * Set pixelformat to the format in which mt vcodec
		 * outputs the decoded frame
		 */
		pix_mp->num_planes = q_data->fmt->num_planes;
		pix_mp->pixelformat = q_data->fmt->fourcc;
		pix_mp->plane_fmt[0].bytesperline = q_data->bytesperline[0];
		pix_mp->plane_fmt[0].sizeimage = q_data->sizeimage[0];
		pix_mp->plane_fmt[1].bytesperline = q_data->bytesperline[1];
		pix_mp->plane_fmt[1].sizeimage = q_data->sizeimage[1];
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		/*
		 * This is run on OUTPUT
		 * The buffer contains compressed image
		 * so width and height have no meaning.
		 * Assign value here to pass v4l2-compliance test
		 */
		pix_mp->width = q_data->visible_width;
		pix_mp->height = q_data->visible_height;
		pix_mp->plane_fmt[0].bytesperline = q_data->bytesperline[0];
		pix_mp->plane_fmt[0].sizeimage = q_data->sizeimage[0];
		pix_mp->pixelformat = q_data->fmt->fourcc;
		pix_mp->num_planes = q_data->fmt->num_planes;
	} else {
		pix_mp->width = q_data->coded_width;
		pix_mp->height = q_data->coded_height;
		pix_mp->num_planes = q_data->fmt->num_planes;
		pix_mp->pixelformat = q_data->fmt->fourcc;
		pix_mp->plane_fmt[0].bytesperline = q_data->bytesperline[0];
		pix_mp->plane_fmt[0].sizeimage = q_data->sizeimage[0];
		pix_mp->plane_fmt[1].bytesperline = q_data->bytesperline[1];
		pix_mp->plane_fmt[1].sizeimage = q_data->sizeimage[1];

		aml_v4l2_debug(4, "[%d] type=%d state=%d Format information could not be read, not ready yet!",
				ctx->id, f->type, ctx->state);
	}

	return 0;
}

/*int vidioc_vdec_g_ctrl(struct file *file, void *fh,
	struct v4l2_control *a)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);

	if (a->id == V4L2_CID_MIN_BUFFERS_FOR_CAPTURE)
		a->value = 20;

	return 0;
}*/

static int vb2ops_vdec_queue_setup(struct vb2_queue *vq,
				unsigned int *nbuffers,
				unsigned int *nplanes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vq);
	struct aml_q_data *q_data;
	unsigned int i;

	q_data = aml_vdec_get_q_data(ctx, vq->type);

	if (q_data == NULL) {
		aml_v4l2_err("[%d] vq->type=%d err", ctx->id, vq->type);
		return -EINVAL;
	}

	if (*nplanes) {
		for (i = 0; i < *nplanes; i++) {
			if (sizes[i] < q_data->sizeimage[i])
				return -EINVAL;
			//alloc_devs[i] = &ctx->dev->plat_dev->dev;
			alloc_devs[i] = v4l_get_dev_from_codec_mm();//alloc mm from the codec mm
		}
	} else {
		if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			*nplanes = 2;
		else
			*nplanes = 1;

		for (i = 0; i < *nplanes; i++) {
			sizes[i] = q_data->sizeimage[i];
			//alloc_devs[i] = &ctx->dev->plat_dev->dev;
			alloc_devs[i] = v4l_get_dev_from_codec_mm();//alloc mm from the codec mm
		}
	}

	pr_info("[%d] type: %d, plane: %d, buf cnt: %d, size: [Y: %u, C: %u]\n",
		ctx->id, vq->type, *nplanes, *nbuffers, sizes[0], sizes[1]);

	return 0;
}

static int vb2ops_vdec_buf_prepare(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct aml_q_data *q_data;
	int i;

	aml_v4l2_debug(4, "[%d] (%d) id=%d",
			ctx->id, vb->vb2_queue->type, vb->index);

	q_data = aml_vdec_get_q_data(ctx, vb->vb2_queue->type);

	for (i = 0; i < q_data->fmt->num_planes; i++) {
		if (vb2_plane_size(vb, i) < q_data->sizeimage[i]) {
			aml_v4l2_err("[%d] data will not fit into plane %d (%lu < %d)",
				ctx->id, i, vb2_plane_size(vb, i),
				q_data->sizeimage[i]);
		}
	}

	return 0;
}

static void vb2ops_vdec_buf_queue(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct aml_video_dec_buf *buf = NULL;
	struct aml_vcodec_mem src_mem;
	unsigned int dpb = 0;

	vb2_v4l2 = to_vb2_v4l2_buffer(vb);
	buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);

	aml_v4l2_debug(3, "[%d] %s(), vb: %p, type: %d, idx: %d, state: %d, used: %d",
		ctx->id, __func__, vb, vb->vb2_queue->type,
		vb->index, vb->state, buf->used);
	/*
	 * check if this buffer is ready to be used after decode
	 */
	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		aml_v4l2_debug(3, "[%d] %s() [%d], y_va: %p, vf_h: %lx, state: %d", ctx->id,
			__func__, __LINE__, buf->frame_buffer.base_y.va,
			buf->frame_buffer.vf_handle, buf->frame_buffer.status);

		if (!buf->que_in_m2m && buf->frame_buffer.status == FB_ST_NORMAL) {
			aml_v4l2_debug(3, "[%d] enque capture buf idx %d, %p\n",
				ctx->id, vb->index, vb);
			v4l2_m2m_buf_queue(ctx->m2m_ctx, vb2_v4l2);
			buf->que_in_m2m = true;
			buf->queued_in_vb2 = true;
			buf->queued_in_v4l2 = true;
			buf->ready_to_display = false;

			/*save capture bufs to be used for resetting config.*/
			list_add(&buf->node, &ctx->capture_list);
		} else if (buf->frame_buffer.status == FB_ST_DISPLAY) {
			buf->queued_in_vb2 = false;
			buf->queued_in_v4l2 = true;
			buf->ready_to_display = false;

			/* recycle vf */
			video_vf_put(ctx->ada_ctx->recv_name,
				&buf->frame_buffer, ctx->id);
		}
		return;
	}

	v4l2_m2m_buf_queue(ctx->m2m_ctx, to_vb2_v4l2_buffer(vb));

	if (ctx->state != AML_STATE_INIT) {
		aml_v4l2_debug(4, "[%d] already init driver %d",
				ctx->id, ctx->state);
		return;
	}

	vb2_v4l2 = to_vb2_v4l2_buffer(vb);
	buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);
	if (buf->lastframe) {
		/* This shouldn't happen. Just in case. */
		aml_v4l2_err("[%d] Invalid flush buffer.", ctx->id);
		v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		return;
	}

	src_mem.va = vb2_plane_vaddr(vb, 0);
	src_mem.dma_addr = vb2_dma_contig_plane_dma_addr(vb, 0);
	src_mem.size = (size_t)vb->planes[0].bytesused;
	if (vdec_if_probe(ctx, &src_mem, NULL))
		return;

	if (vdec_if_get_param(ctx, GET_PARAM_PIC_INFO, &ctx->picinfo)) {
		pr_err("[%d] GET_PARAM_PICTURE_INFO err\n", ctx->id);
		return;
	}

	if (vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpb)) {
		pr_err("[%d] GET_PARAM_DPB_SIZE err\n", ctx->id);
		return;
	}

	ctx->dpb_size = dpb;
	ctx->last_decoded_picinfo = ctx->picinfo;
	aml_vdec_queue_res_chg_event(ctx);

	mutex_lock(&ctx->state_lock);
	if (ctx->state == AML_STATE_INIT) {
		ctx->state = AML_STATE_PROBE;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_PROBE)",
			ctx->id, __func__);
	}
	mutex_unlock(&ctx->state_lock);
}

static void vb2ops_vdec_buf_finish(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct aml_video_dec_buf *buf = NULL;
	bool buf_error;

	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);

	//mutex_lock(&ctx->lock);
	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		buf->queued_in_v4l2 = false;
		buf->queued_in_vb2 = false;
	}
	buf_error = buf->error;
	//mutex_unlock(&ctx->lock);

	if (buf_error) {
		aml_v4l2_err("[%d] Unrecoverable error on buffer.", ctx->id);
		ctx->state = AML_STATE_ABORT;
		aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_ABORT)",
			ctx->id, __func__);
	}
}

static int vb2ops_vdec_buf_init(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = container_of(vb,
					struct vb2_v4l2_buffer, vb2_buf);
	struct aml_video_dec_buf *buf = container_of(vb2_v4l2,
					struct aml_video_dec_buf, vb);
	unsigned int size, phy_addr = 0;
	char *owner = __getname();

	aml_v4l2_debug(4, "[%d] (%d) id=%d",
		ctx->id, vb->vb2_queue->type, vb->index);

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		buf->used = false;
		buf->ready_to_display = false;
		buf->queued_in_v4l2 = false;
		buf->frame_buffer.status = FB_ST_NORMAL;
	} else {
		buf->lastframe = false;
	}

	/* codec_mm buffers count */
	if (V4L2_TYPE_IS_OUTPUT(vb->type)) {
		size = vb->planes[0].length;
		phy_addr = vb2_dma_contig_plane_dma_addr(vb, 0);
		snprintf(owner, PATH_MAX, "%s-%d", "v4l-input", ctx->id);
		strncpy(buf->mem_onwer, owner, sizeof(buf->mem_onwer));
		buf->mem_onwer[sizeof(buf->mem_onwer) - 1] = '\0';

		buf->mem[0] = v4l_reqbufs_from_codec_mm(buf->mem_onwer,
				phy_addr, size, vb->index);
		aml_v4l2_debug(3, "[%d] IN alloc, addr: %x, size: %u, idx: %u",
			ctx->id, phy_addr, size, vb->index);
	} else {
		size = vb->planes[0].length;
		phy_addr = vb2_dma_contig_plane_dma_addr(vb, 0);
		snprintf(owner, PATH_MAX, "%s-%d", "v4l-output", ctx->id);
		strncpy(buf->mem_onwer, owner, sizeof(buf->mem_onwer));
		buf->mem_onwer[sizeof(buf->mem_onwer) - 1] = '\0';

		buf->mem[0] = v4l_reqbufs_from_codec_mm(buf->mem_onwer,
			phy_addr, size, vb->index);
		aml_v4l2_debug(3, "[%d] OUT Y alloc, addr: %x, size: %u, idx: %u",
			ctx->id, phy_addr, size, vb->index);

		size = vb->planes[1].length;
		phy_addr = vb2_dma_contig_plane_dma_addr(vb, 1);
		buf->mem[1] = v4l_reqbufs_from_codec_mm(buf->mem_onwer,
				phy_addr, size, vb->index);
		aml_v4l2_debug(3, "[%d] OUT C alloc, addr: %x, size: %u, idx: %u",
			ctx->id, phy_addr, size, vb->index);
	}

	__putname(owner);

	return 0;
}

static void codec_mm_bufs_cnt_clean(struct vb2_queue *q)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct aml_video_dec_buf *buf = NULL;
	int i;

	for (i = 0; i < q->num_buffers; ++i) {
		vb2_v4l2 = to_vb2_v4l2_buffer(q->bufs[i]);
		buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);
		if (IS_ERR_OR_NULL(buf->mem[0]))
			return;

		if (V4L2_TYPE_IS_OUTPUT(q->bufs[i]->type)) {
			v4l_freebufs_back_to_codec_mm(buf->mem_onwer, buf->mem[0]);

			aml_v4l2_debug(3, "[%d] IN clean, addr: %lx, size: %u, idx: %u",
				ctx->id, buf->mem[0]->phy_addr, buf->mem[0]->buffer_size, i);
			buf->mem[0] = NULL;
			continue;
		}

		v4l_freebufs_back_to_codec_mm(buf->mem_onwer, buf->mem[0]);
		v4l_freebufs_back_to_codec_mm(buf->mem_onwer, buf->mem[1]);

		aml_v4l2_debug(3, "[%d] OUT Y clean, addr: %lx, size: %u, idx: %u",
			ctx->id, buf->mem[0]->phy_addr, buf->mem[0]->buffer_size, i);
		aml_v4l2_debug(3, "[%d] OUT C clean, addr: %lx, size: %u, idx: %u",
			ctx->id, buf->mem[1]->phy_addr, buf->mem[1]->buffer_size, i);
		buf->mem[0] = NULL;
		buf->mem[1] = NULL;
	}
}

static int vb2ops_vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(q);

	ctx->has_receive_eos = false;
	v4l2_m2m_set_dst_buffered(ctx->fh.m2m_ctx, true);

	return 0;
}

static void vb2ops_vdec_stop_streaming(struct vb2_queue *q)
{
	struct aml_video_dec_buf *buf = NULL;
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	int i;

	aml_v4l2_debug(3, "[%d] (%d) state=(%x) frame_cnt=%d",
			ctx->id, q->type, ctx->state, ctx->decoded_frame_cnt);

	codec_mm_bufs_cnt_clean(q);

	if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		while (v4l2_m2m_src_buf_remove(ctx->m2m_ctx));
		return;
	}

	while (v4l2_m2m_dst_buf_remove(ctx->m2m_ctx));

	for (i = 0; i < q->num_buffers; ++i) {
		if (q->bufs[i]->state == VB2_BUF_STATE_ACTIVE) {
			q->bufs[i]->state = VB2_BUF_STATE_DEQUEUED;
			atomic_dec(&q->owned_by_drv_count);
		}
		vb2_v4l2 = to_vb2_v4l2_buffer(q->bufs[i]);
		buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);
		buf->frame_buffer.status = FB_ST_FREE;
	}
}

static void m2mops_vdec_device_run(void *priv)
{
	struct aml_vcodec_ctx *ctx = priv;
	struct aml_vcodec_dev *dev = ctx->dev;

	aml_v4l2_debug(4, "[%d] %s() [%d]", ctx->id, __func__, __LINE__);

	queue_work(dev->decode_workqueue, &ctx->decode_work);
}

void vdec_device_vf_run(struct aml_vcodec_ctx *ctx)
{
	aml_v4l2_debug(3, "[%d] %s() [%d]", ctx->id, __func__, __LINE__);

	if (ctx->state < AML_STATE_ACTIVE ||
		ctx->state > AML_STATE_FLUSHED)
		return;

	aml_thread_notify(ctx, AML_THREAD_CAPTURE);
}

static int m2mops_vdec_job_ready(void *m2m_priv)
{
	struct aml_vcodec_ctx *ctx = m2m_priv;

	aml_v4l2_debug(4, "[%d] %s(), state: %d",  ctx->id,
		__func__, ctx->state);

	if (ctx->state < AML_STATE_PROBE ||
		ctx->state > AML_STATE_FLUSHED)
		return 0;

	return 1;
}

static void m2mops_vdec_job_abort(void *priv)
{
	struct aml_vcodec_ctx *ctx = priv;

	aml_v4l2_debug(3, "[%d] %s() [%d]", ctx->id, __func__, __LINE__);

	ctx->state = AML_STATE_ABORT;
	aml_v4l2_debug(1, "[%d] %s() vcodec state (AML_STATE_ABORT)",
		ctx->id, __func__);

	wake_up(&ctx->m2m_ctx->finished);
}

static int aml_vdec_g_v_ctrl(struct v4l2_ctrl *ctrl)
{
	struct aml_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);
	int ret = 0;

	aml_v4l2_debug(4, "%s() [%d]", __func__, __LINE__);

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (ctx->state >= AML_STATE_PROBE) {
			ctrl->val = ctx->dpb_size;
		} else {
			pr_err("Seqinfo not ready.\n");
			ctrl->val = 0;
		}
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static const struct v4l2_ctrl_ops aml_vcodec_dec_ctrl_ops = {
	.g_volatile_ctrl = aml_vdec_g_v_ctrl,
};

int aml_vcodec_dec_ctrls_setup(struct aml_vcodec_ctx *ctx)
{
	struct v4l2_ctrl *ctrl;

	v4l2_ctrl_handler_init(&ctx->ctrl_hdl, 1);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&aml_vcodec_dec_ctrl_ops,
				V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
				0, 32, 1, 1);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (ctx->ctrl_hdl.error) {
		aml_v4l2_err("[%d] Adding control failed %d",
				ctx->id, ctx->ctrl_hdl.error);
		return ctx->ctrl_hdl.error;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);
	return 0;
}

static void m2mops_vdec_lock(void *m2m_priv)
{
	struct aml_vcodec_ctx *ctx = m2m_priv;

	aml_v4l2_debug(4, "[%d] %s()", ctx->id, __func__);
	mutex_lock(&ctx->dev->dev_mutex);
}

static void m2mops_vdec_unlock(void *m2m_priv)
{
	struct aml_vcodec_ctx *ctx = m2m_priv;

	aml_v4l2_debug(4, "[%d] %s()", ctx->id, __func__);
	mutex_unlock(&ctx->dev->dev_mutex);
}

const struct v4l2_m2m_ops aml_vdec_m2m_ops = {
	.device_run	= m2mops_vdec_device_run,
	.job_ready	= m2mops_vdec_job_ready,
	.job_abort	= m2mops_vdec_job_abort,
	.lock		= m2mops_vdec_lock,
	.unlock		= m2mops_vdec_unlock,
};

static const struct vb2_ops aml_vdec_vb2_ops = {
	.queue_setup	= vb2ops_vdec_queue_setup,
	.buf_prepare	= vb2ops_vdec_buf_prepare,
	.buf_queue	= vb2ops_vdec_buf_queue,
	.wait_prepare	= vb2_ops_wait_prepare,
	.wait_finish	= vb2_ops_wait_finish,
	.buf_init	= vb2ops_vdec_buf_init,
	.buf_finish	= vb2ops_vdec_buf_finish,
	.start_streaming = vb2ops_vdec_start_streaming,
	.stop_streaming	= vb2ops_vdec_stop_streaming,
};

const struct v4l2_ioctl_ops aml_vdec_ioctl_ops = {
	.vidioc_streamon	= vidioc_decoder_streamon,
	.vidioc_streamoff	= v4l2_m2m_ioctl_streamoff,
	.vidioc_reqbufs		= v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf	= v4l2_m2m_ioctl_querybuf,
	.vidioc_expbuf		= v4l2_m2m_ioctl_expbuf,//??
	//.vidioc_g_ctrl		= vidioc_vdec_g_ctrl,

	.vidioc_qbuf		= vidioc_vdec_qbuf,
	.vidioc_dqbuf		= vidioc_vdec_dqbuf,

	.vidioc_try_fmt_vid_cap_mplane	= vidioc_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_out_mplane	= vidioc_try_fmt_vid_out_mplane,

	.vidioc_s_fmt_vid_cap_mplane	= vidioc_vdec_s_fmt,
	.vidioc_s_fmt_vid_out_mplane	= vidioc_vdec_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane	= vidioc_vdec_g_fmt,
	.vidioc_g_fmt_vid_out_mplane	= vidioc_vdec_g_fmt,

	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,

	.vidioc_enum_fmt_vid_cap_mplane	= vidioc_vdec_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out_mplane	= vidioc_vdec_enum_fmt_vid_out_mplane,
	.vidioc_enum_framesizes		= vidioc_enum_framesizes,

	.vidioc_querycap		= vidioc_vdec_querycap,
	.vidioc_subscribe_event		= vidioc_vdec_subscribe_evt,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
	.vidioc_g_selection             = vidioc_vdec_g_selection,
	.vidioc_s_selection             = vidioc_vdec_s_selection,

	.vidioc_decoder_cmd		= vidioc_decoder_cmd,
	.vidioc_try_decoder_cmd		= vidioc_try_decoder_cmd,
};

int aml_vcodec_dec_queue_init(void *priv, struct vb2_queue *src_vq,
			   struct vb2_queue *dst_vq)
{
	struct aml_vcodec_ctx *ctx = priv;
	int ret = 0;

	aml_v4l2_debug(4, "[%d] %s()", ctx->id, __func__);

	src_vq->type		= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes	= VB2_DMABUF | VB2_MMAP;
	src_vq->drv_priv	= ctx;
	src_vq->buf_struct_size = sizeof(struct aml_video_dec_buf);
	src_vq->ops		= &aml_vdec_vb2_ops;
	src_vq->mem_ops		= &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock		= &ctx->dev->dev_mutex;

	ret = vb2_queue_init(src_vq);
	if (ret) {
		aml_v4l2_err("[%d] Failed to initialize videobuf2 queue(output)", ctx->id);
		return ret;
	}
	dst_vq->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes	= VB2_DMABUF | VB2_MMAP | VB2_USERPTR;
	dst_vq->drv_priv	= ctx;
	dst_vq->buf_struct_size = sizeof(struct aml_video_dec_buf);
	dst_vq->ops		= &aml_vdec_vb2_ops;
	dst_vq->mem_ops		= &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock		= &ctx->dev->dev_mutex;

	ret = vb2_queue_init(dst_vq);
	if (ret) {
		vb2_queue_release(src_vq);
		aml_v4l2_err("[%d] Failed to initialize videobuf2 queue(capture)", ctx->id);
	}

	return ret;
}

