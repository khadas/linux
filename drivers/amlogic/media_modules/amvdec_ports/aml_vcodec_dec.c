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
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
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
#include <media/videobuf2-dma-sg.h>

#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/crc32.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/scatterlist.h>
#include <linux/sched/clock.h>
#include <linux/highmem.h>
#include <uapi/linux/sched/types.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>

#include "aml_vcodec_drv.h"
#include "aml_vcodec_dec.h"
#include "aml_vcodec_util.h"
#include "vdec_drv_if.h"
#include "aml_vcodec_adapt.h"
#include "aml_vcodec_vpp.h"
#include "aml_vcodec_ge2d.h"

#include "../frame_provider/decoder/utils/decoder_bmmu_box.h"
#include "../frame_provider/decoder/utils/decoder_mmu_box.h"
#include "../common/chips/decoder_cpu_ver_info.h"
#include "utils/common.h"
#include "../frame_provider/decoder/utils/vdec_sync.h"


#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_V4L2
#include <trace/events/meson_atrace.h>


#define OUT_FMT_IDX		(0) //default h264
#define CAP_FMT_IDX		(11) //capture nv21m
#define CAP_FMT_I420_IDX	(15) //use for mjpeg

#define AML_VDEC_MIN_W	64U
#define AML_VDEC_MIN_H	64U
#define DFT_CFG_WIDTH	AML_VDEC_MIN_W
#define DFT_CFG_HEIGHT	AML_VDEC_MIN_H

#define V4L2_CID_USER_AMLOGIC_BASE (V4L2_CID_USER_BASE + 0x1100)
#define AML_V4L2_SET_DRMMODE (V4L2_CID_USER_AMLOGIC_BASE + 0)
#define AML_V4L2_GET_INPUT_BUFFER_NUM (V4L2_CID_USER_AMLOGIC_BASE + 1)
#define AML_V4L2_SET_DURATION (V4L2_CID_USER_AMLOGIC_BASE + 2)
#define AML_V4L2_GET_FILMGRAIN_INFO (V4L2_CID_USER_AMLOGIC_BASE + 3)
#define AML_V4L2_GET_DECODER_INFO (V4L2_CID_USER_AMLOGIC_BASE + 5)

#define V4L2_EVENT_PRIVATE_EXT_VSC_BASE (V4L2_EVENT_PRIVATE_START + 0x2000)
#define V4L2_EVENT_PRIVATE_EXT_VSC_EVENT (V4L2_EVENT_PRIVATE_EXT_VSC_BASE + 1)
#define V4L2_EVENT_PRIVATE_EXT_SEND_ERROR (V4L2_EVENT_PRIVATE_EXT_VSC_BASE + 2)

#define WORK_ITEMS_MAX (32)
#define MAX_DI_INSTANCE (2)

//#define USEC_PER_SEC 1000000

#define call_void_memop(vb, op, args...)				\
	do {								\
		if ((vb)->vb2_queue->mem_ops->op)			\
			(vb)->vb2_queue->mem_ops->op(args);		\
	} while (0)

static struct aml_video_fmt aml_video_formats[] = {
	{
		.name = "H.264",
		.fourcc = V4L2_PIX_FMT_H264,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.name = "H.265",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.name = "VP9",
		.fourcc = V4L2_PIX_FMT_VP9,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.name = "MPEG1",
		.fourcc = V4L2_PIX_FMT_MPEG1,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.name = "MPEG2",
		.fourcc = V4L2_PIX_FMT_MPEG2,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.name = "MPEG4",
		.fourcc = V4L2_PIX_FMT_MPEG4,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.name = "MJPEG",
		.fourcc = V4L2_PIX_FMT_MJPEG,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.name = "AVS",
		.fourcc = V4L2_PIX_FMT_AVS,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.name = "AV1",
		.fourcc = V4L2_PIX_FMT_AV1,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.name = "AVS2",
		.fourcc = V4L2_PIX_FMT_AVS2,
		.type = AML_FMT_DEC,
		.num_planes = 1,
	},
	{
		.name = "NV21",
		.fourcc = V4L2_PIX_FMT_NV21,
		.type = AML_FMT_FRAME,
		.num_planes = 1,
	},
	{
		.name = "NV21M",
		.fourcc = V4L2_PIX_FMT_NV21M,
		.type = AML_FMT_FRAME,
		.num_planes = 2,
	},
	{
		.name = "NV12",
		.fourcc = V4L2_PIX_FMT_NV12,
		.type = AML_FMT_FRAME,
		.num_planes = 1,
	},
	{
		.name = "NV12M",
		.fourcc = V4L2_PIX_FMT_NV12M,
		.type = AML_FMT_FRAME,
		.num_planes = 2,
	},
	{
		.name = "YUV420",
		.fourcc = V4L2_PIX_FMT_YUV420,
		.type = AML_FMT_FRAME,
		.num_planes = 1,
	},
	{
		.name = "YUV420M",
		.fourcc = V4L2_PIX_FMT_YUV420M,
		.type = AML_FMT_FRAME,
		.num_planes = 2,
	},
};

static const struct aml_codec_framesizes aml_vdec_framesizes[] = {
	{
		.fourcc	= V4L2_PIX_FMT_H264,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc	= V4L2_PIX_FMT_HEVC,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_VP9,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG1,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG2,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG4,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_MJPEG,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_AVS,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_AV1,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_AVS2,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_NV21,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_NV21M,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_NV12,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_NV12M,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_YUV420,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
	{
		.fourcc = V4L2_PIX_FMT_YUV420M,
		.stepwise = {  AML_VDEC_MIN_W, AML_VDEC_MAX_W, 2,
				AML_VDEC_MIN_H, AML_VDEC_MAX_H, 2},
	},
};

#define NUM_SUPPORTED_FRAMESIZE ARRAY_SIZE(aml_vdec_framesizes)
#define NUM_FORMATS ARRAY_SIZE(aml_video_formats)

extern bool multiplanar;
extern int dump_capture_frame;
extern char dump_path[32];
extern int bypass_vpp;
extern int bypass_ge2d;
extern bool support_format_I420;
extern bool support_mjpeg;
extern int bypass_progressive;
extern int force_enable_nr;
extern int force_enable_di_local_buffer;
extern int max_di_instance;
extern int bypass_nr_flag;

extern int dmabuf_fd_install_data(int fd, void* data, u32 size);
extern bool is_v4l2_buf_file(struct file *file);
extern int get_double_write_ratio(int dw_mode);
static void box_release(struct kref *kref);
static struct internal_comp_buf* vb_to_comp(struct aml_vcodec_ctx *ctx,
					    struct vb2_buffer *vb);
static void update_ctx_dimension(struct aml_vcodec_ctx *ctx, u32 type);
static void copy_v4l2_format_dimention(struct v4l2_pix_format_mplane *pix_mp,
				       struct v4l2_pix_format *pix,
				       struct aml_q_data *q_data,
				       u32 type);

static ulong aml_vcodec_ctx_lock(struct aml_vcodec_ctx *ctx)
{
	ulong flags;

	spin_lock_irqsave(&ctx->slock, flags);

	return flags;
}

static void aml_vcodec_ctx_unlock(struct aml_vcodec_ctx *ctx, ulong flags)
{
	spin_unlock_irqrestore(&ctx->slock, flags);
}

static ulong dmabuf_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	ulong size = 0;
	u32 i;

	for_each_sg(sgt->sgl, s, sgt->nents, i) {
		if (sg_dma_address(s) != expected)
			break;
		expected = sg_dma_address(s) + sg_dma_len(s);
		size += sg_dma_len(s);
	}

	return size;
}

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

void aml_vdec_dispatch_event(struct aml_vcodec_ctx *ctx, u32 changes)
{
	struct v4l2_event event = {0};

	switch (changes) {
	case V4L2_EVENT_SRC_CH_RESOLUTION:
	case V4L2_EVENT_SRC_CH_HDRINFO:
	case V4L2_EVENT_REQUEST_RESET:
	case V4L2_EVENT_REQUEST_EXIT:
		event.type = V4L2_EVENT_SOURCE_CHANGE;
		event.u.src_change.changes = changes;
		break;
	case V4L2_EVENT_SEND_EOS:
		event.type = V4L2_EVENT_EOS;
		break;
	case V4L2_EVENT_SEND_ERROR:
		event.type = V4L2_EVENT_PRIVATE_EXT_SEND_ERROR;
		break;
	default:
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"unsupport dispatch event %x\n", changes);
		return;
	}

	v4l2_event_queue_fh(&ctx->fh, &event);
	if (changes != V4L2_EVENT_SRC_CH_HDRINFO)
		v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO, "changes: %x\n", changes);
	else
		v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO, "changes: %x\n", changes);
}

static void aml_vdec_flush_decoder(struct aml_vcodec_ctx *ctx)
{
	v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO, "%s\n", __func__);

	aml_decoder_flush(ctx->ada_ctx);
}

/* Conditions:
 * Always connect VPP for mpeg2 and h264 when the stream size is under 2K.
 * Always connect VPP for hevc/av1/vp9 when color space is not SDR and
 *     stream size is under 2K.
 * For DV, need application to notify V4L2 driver to enforce the color space
 *     conversion. Plan to do it through a system node.
 * Do not connect VPP in other cases.
 */
static bool vpp_needed(struct aml_vcodec_ctx *ctx, u32* mode)
{
	int width = ctx->picinfo.coded_width;
	int height = ctx->picinfo.coded_height;
	int size = 1920 * 1088;

	if (bypass_vpp)
		return false;

	if (!ctx->vpp_cfg.enable_nr &&
		(ctx->picinfo.field == V4L2_FIELD_NONE) &&
		!(ctx->config.parm.dec.cfg.double_write_mode & 0x20)) {
		return false;
	}

	if (!ctx->vpp_cfg.enable_nr &&
		(ctx->output_pix_fmt == V4L2_PIX_FMT_HEVC)) {
		if (is_over_size(width, height, size)) {
			return false;
		}
	}

	if ((ctx->output_pix_fmt == V4L2_PIX_FMT_H264) &&
		(ctx->picinfo.field != V4L2_FIELD_NONE)) {
		if (is_over_size(width, height, size)) {
			return false;
		}
	}

	if (ctx->vpp_cfg.enable_nr) {
		if (ctx->vpp_cfg.enable_local_buf)
			*mode = VPP_MODE_NOISE_REDUC_LOCAL;
		else
			*mode = VPP_MODE_NOISE_REDUC;
	} else {
		if (ctx->vpp_cfg.enable_local_buf)
			*mode = VPP_MODE_DI_LOCAL;
		else
			*mode = VPP_MODE_DI;
	}

	if (!disable_vpp_dw_mmu &&
		(ctx->config.parm.dec.cfg.double_write_mode & 0x20)) {
		*mode = VPP_MODE_S4_DW_MMU;;
	}
#if 0//enable later
	if (ctx->colorspace != V4L2_COLORSPACE_DEFAULT &&
		!is_over_size(width, height, size)) {
		if (ctx->vpp_cfg.enable_local_buf)
			*mode = VPP_MODE_COLOR_CONV_LOCAL;
		else
			*mode = VPP_MODE_COLOR_CONV;
	}
#endif

	return true;
}

static bool ge2d_needed(struct aml_vcodec_ctx *ctx, u32* mode)
{
	bool enable_fence = (ctx->config.parm.dec.cfg.low_latency_mode & 2) ? 1 : 0;

	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) && enable_fence) {
		return false;
	}

	if (bypass_ge2d)
		return false;

	if (ctx->ge2d_cfg.bypass)
		return false;

	if (is_cpu_t7()) {
		if ((ctx->output_pix_fmt != V4L2_PIX_FMT_H264) &&
			(ctx->output_pix_fmt != V4L2_PIX_FMT_MPEG1) &&
			(ctx->output_pix_fmt != V4L2_PIX_FMT_MPEG2) &&
			(ctx->output_pix_fmt != V4L2_PIX_FMT_MPEG4) &&
			(ctx->output_pix_fmt != V4L2_PIX_FMT_MJPEG) &&
			(ctx->output_pix_fmt != V4L2_PIX_FMT_AVS)) {
			return false;
		}
	} else if (ctx->output_pix_fmt != V4L2_PIX_FMT_MJPEG) {
			return false;
	}

	if (ctx->picinfo.field != V4L2_FIELD_NONE) {
		return false;
	}

	if ((ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12) ||
		(ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12M))
		*mode = GE2D_MODE_CONVERT_NV12;
	else if ((ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21) ||
		(ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21M))
		*mode = GE2D_MODE_CONVERT_NV21;
	else
		*mode = GE2D_MODE_CONVERT_NV21;

	*mode |= GE2D_MODE_CONVERT_LE;

	return true;
}

static u32 v4l_buf_size_decision(struct aml_vcodec_ctx *ctx)
{
	u32 mode, total_size;
	struct vdec_pic_info *picinfo = &ctx->picinfo;
	struct aml_vpp_cfg_infos *vpp = &ctx->vpp_cfg;
	struct aml_ge2d_cfg_infos *ge2d = &ctx->ge2d_cfg;

	if (vpp_needed(ctx, &mode)) {
		vpp->mode        = mode;
		vpp->fmt         = ctx->cap_pix_fmt;
		vpp->is_drm      = ctx->is_drm_mode;
		vpp->buf_size = aml_v4l2_vpp_get_buf_num(vpp->mode)
			+ picinfo->vpp_margin;

		if (picinfo->field == V4L2_FIELD_NONE) {
			vpp->is_prog = true;
			vpp->buf_size = 0;
		} else {
			vpp->is_prog = false;
			/* for between with dec & vpp. */
			picinfo->dpb_margin = 2;
		}

		if (vpp->is_prog &&
			!vpp->enable_local_buf &&
			bypass_progressive) {
			vpp->is_bypass_p = true;
		}
		ctx->vpp_is_need = true;
	} else {
		vpp->buf_size = 0;
		ctx->vpp_is_need = false;
	}

	if (ge2d_needed(ctx, &mode)) {
		ge2d->mode = mode;
		ge2d->buf_size = 4 + picinfo->dpb_margin;
		ctx->ge2d_is_need = true;
		picinfo->dpb_margin = 2;
	} else {
		ge2d->buf_size = 0;
		ctx->ge2d_is_need = false;
	}

	ctx->dpb_size = picinfo->dpb_frames + picinfo->dpb_margin;
	ctx->vpp_size = vpp->buf_size;
	ctx->ge2d_size = ge2d->buf_size;

	total_size = ctx->dpb_size + ctx->vpp_size + ctx->ge2d_size;

	if (total_size > V4L_CAP_BUFF_MAX) {
		if (ctx->ge2d_size) {
			ctx->dpb_size = V4L_CAP_BUFF_MAX - ctx->ge2d_size - ctx->vpp_size;
		} else if (ctx->vpp_size) {
			ctx->dpb_size = V4L_CAP_BUFF_MAX - ctx->vpp_size;
		} else {
			ctx->dpb_size = V4L_CAP_BUFF_MAX;
		}
		picinfo->dpb_margin = ctx->dpb_size - picinfo->dpb_frames;
		total_size = V4L_CAP_BUFF_MAX;
	}
	vdec_if_set_param(ctx, SET_PARAM_PIC_INFO, picinfo);

	return total_size;
}

void aml_vdec_pic_info_update(struct aml_vcodec_ctx *ctx)
{
	if (vdec_if_get_param(ctx, GET_PARAM_PIC_INFO, &ctx->last_decoded_picinfo)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Cannot get param : GET_PARAM_PICTURE_INFO ERR\n");
		return;
	}

	if (ctx->last_decoded_picinfo.visible_width == 0 ||
		ctx->last_decoded_picinfo.visible_height == 0 ||
		ctx->last_decoded_picinfo.coded_width == 0 ||
		ctx->last_decoded_picinfo.coded_height == 0) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Cannot get correct pic info\n");
		return;
	}

	/*if ((ctx->last_decoded_picinfo.visible_width == ctx->picinfo.visible_width) ||
	    (ctx->last_decoded_picinfo.visible_height == ctx->picinfo.visible_height))
		return;*/

	v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
		"new(%d,%d), old(%d,%d), real(%d,%d)\n",
			ctx->last_decoded_picinfo.visible_width,
			ctx->last_decoded_picinfo.visible_height,
			ctx->picinfo.visible_width, ctx->picinfo.visible_height,
			ctx->last_decoded_picinfo.coded_width,
			ctx->last_decoded_picinfo.coded_width);

	ctx->picinfo = ctx->last_decoded_picinfo;

	if (ctx->vpp_is_need)
		ctx->vpp_cfg.is_vpp_reset = true;

	v4l_buf_size_decision(ctx);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
		"Update picture buffer count: dec:%u, vpp:%u, ge2d:%u, margin:%u, total:%u\n",
		ctx->picinfo.dpb_frames, ctx->vpp_size, ctx->ge2d_size,
		ctx->picinfo.dpb_margin,
		CTX_BUF_TOTAL(ctx));
}

void vdec_frame_buffer_release(void *data)
{
	struct file_private_data *priv_data =
		(struct file_private_data *) data;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)
		priv_data->v4l_dec_ctx;
	struct aml_video_dec_buf *vb = (struct aml_video_dec_buf *)
		priv_data->vb_handle;
	struct uvm_hook_mod_info *uvm = NULL;

	if (ctx && ctx->uvm_proxy) {
		uvm = &ctx->uvm_proxy[vb->internal_index];
		uvm->free(uvm->arg);
	}

	memset(data, 0, sizeof(struct file_private_data));
	kfree(data);
}

void aml_clean_proxy_uvm(struct aml_vcodec_ctx *ctx)
{
	struct uvm_hook_mod_info *uvm = NULL;
	int i;

	for (i = 0; i < V4L_CAP_BUFF_MAX; i++) {
		if (ctx && ctx->uvm_proxy) {
			uvm = &ctx->uvm_proxy[i];
			if (uvm->free)
				uvm->free(uvm->arg);
		}
	}
}

static void v4l2_buff_done(struct vb2_v4l2_buffer *buf, enum vb2_buffer_state state)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(buf->vb2_buf.vb2_queue);

	mutex_lock(&ctx->buff_done_lock);
	if (buf->vb2_buf.state != VB2_BUF_STATE_ACTIVE) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "vb is not active state = %d!\n",
			buf->vb2_buf.state);
		mutex_unlock(&ctx->buff_done_lock);
		return;
	}
	v4l2_m2m_buf_done(buf, state);
	mutex_unlock(&ctx->buff_done_lock);
}

static void comp_buf_set_vframe(struct aml_vcodec_ctx *ctx,
			 struct vb2_buffer *vb,
			 struct vframe_s *vf)
{
	dmabuf_set_vframe(vb->planes[0].dbuf, vf, VF_SRC_DECODER);
}

static void fb_map_table_clean(struct aml_vcodec_ctx *ctx)
{
	int i;
	ulong flags;

	flags = aml_vcodec_ctx_lock(ctx);

	for (i = 0; i < ARRAY_SIZE(ctx->fb_map); i++) {
		ctx->fb_map[i].addr	= 0;
		ctx->fb_map[i].vframe	= NULL;
		ctx->fb_map[i].task	= NULL;
		ctx->fb_map[i].icomp	= 0;
	}

	aml_vcodec_ctx_unlock(ctx, flags);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO, "%s done\n", __func__);
}

static void fb_map_table_hold(struct aml_vcodec_ctx *ctx,
				struct vb2_buffer *vb,
				struct vframe_s *vf,
				struct task_chain_s *task,
				u32 icomp)
{
	int i;
	ulong addr, flags;

	flags = aml_vcodec_ctx_lock(ctx);

	addr = vb2_dma_contig_plane_dma_addr(vb, 0);

	for (i = 0; i < ARRAY_SIZE(ctx->fb_map); i++) {
		if (!ctx->fb_map[i].addr ||
			(addr == ctx->fb_map[i].addr)) {
			ctx->fb_map[i].task	= task;
			ctx->fb_map[i].addr	= addr;
			ctx->fb_map[i].vframe	= vf;
			ctx->fb_map[i].icomp	= icomp;

			v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
				"%s, task:%px, vf:%px, addr:%lx, icomp:%u\n",
				__func__, task, vf, addr, icomp);
			break;
		}
	}

	aml_vcodec_ctx_unlock(ctx, flags);

	if (i >= ARRAY_SIZE(ctx->fb_map)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"%s, table is full. addr:%lx, vf:%px\n",
			__func__, addr, vf);
	}
}

static void fb_map_table_fetch(struct aml_vcodec_ctx *ctx,
				struct vb2_buffer *vb,
				struct vframe_s **vf,
				struct task_chain_s **task,
				u32 *icomp)
{
	int i;
	ulong addr, flags;

	flags = aml_vcodec_ctx_lock(ctx);

	addr = vb2_dma_contig_plane_dma_addr(vb, 0);

	for (i = 0; i < ARRAY_SIZE(ctx->fb_map); i++) {
		if (addr == ctx->fb_map[i].addr) {
			*task = ctx->fb_map[i].task;
			*vf = ctx->fb_map[i].vframe;
			*icomp = ctx->fb_map[i].icomp;

			v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
				"%s, task:%px, vf:%px, addr:%lx, icomp:%u\n",
				__func__, task, vf, addr, *icomp);

			ctx->fb_map[i].task	= NULL;
			ctx->fb_map[i].vframe	= NULL;
			ctx->fb_map[i].addr	= 0;
			ctx->fb_map[i].icomp	= 0;
			break;
		}
	}

	aml_vcodec_ctx_unlock(ctx, flags);

	if (i >= ARRAY_SIZE(ctx->fb_map)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
			"%s, there is new addr:%lx.\n",
			__func__, addr);
	}
}

static bool is_fb_mapped(struct aml_vcodec_ctx *ctx, ulong addr)
{
	int i;
	ulong flags;

	flags = aml_vcodec_ctx_lock(ctx);

	for (i = 0; i < ARRAY_SIZE(ctx->fb_map); i++) {
		if (addr == ctx->fb_map[i].addr)
			break;
	}

	aml_vcodec_ctx_unlock(ctx, flags);

	if (i >= ARRAY_SIZE(ctx->fb_map)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
			"%s, addr:%lx isn't output.\n",
			__func__, addr);
		return false;
	}

	return true;
}

 static void post_frame_to_upper(struct aml_vcodec_ctx *ctx,
	struct vdec_v4l2_buffer *fb)
{
	struct aml_video_dec_buf *dstbuf =
		container_of(fb, struct aml_video_dec_buf, frame_buffer);
	struct vb2_buffer *vb2_buf = &dstbuf->vb.vb2_buf;
	struct vframe_s *vf = fb->vframe;
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;

	vf->index_disp = ctx->index_disp;
	ctx->index_disp++;
	ctx->post_to_upper_done = false;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_OUTPUT,
		"OUT_BUFF (%s, st:%d, seq:%d) vb:(%d, %px), vf:(%d, %px), ts:%lld, flag: 0x%x "
		"Y:(%lx, %u) C/U:(%lx, %u) V:(%lx, %u)\n",
		ctx->ada_ctx->frm_name, fb->status, vf->index_disp,
		vb2_buf->index, vb2_buf,
		vf->index & 0xff, vf,
		vf->timestamp,
		vf->flag,
		fb->m.mem[0].addr, fb->m.mem[0].size,
		fb->m.mem[1].addr, fb->m.mem[1].size,
		fb->m.mem[2].addr, fb->m.mem[2].size);

	vb2_v4l2 = container_of(vb2_buf, struct vb2_v4l2_buffer, vb2_buf);

	if (dstbuf->frame_buffer.num_planes == 1) {
		vb2_set_plane_payload(vb2_buf, 0, fb->m.mem[0].bytes_used);
	} else if (dstbuf->frame_buffer.num_planes == 2) {
		vb2_set_plane_payload(vb2_buf, 0, fb->m.mem[0].bytes_used);
		vb2_set_plane_payload(vb2_buf, 1, fb->m.mem[1].bytes_used);
	}
	vb2_buf->timestamp = vf->timestamp;
	dstbuf->vb.flags |= vf->frame_type;

	if ((ctx->picinfo.field == V4L2_FIELD_INTERLACED) && (!ctx->vpp_is_need)) {
		vb2_v4l2->field = V4L2_FIELD_INTERLACED;
	}

	do {
		unsigned int dw_mode = VDEC_DW_NO_AFBC;
		struct file *fp;
		char file_name[64] = {0};

		if (!dump_capture_frame || ctx->is_drm_mode)
			break;
		if (vdec_if_get_param(ctx, GET_PARAM_DW_MODE, &dw_mode))
			break;
		if (dw_mode == VDEC_DW_AFBC_ONLY)
			break;

		snprintf(file_name, 64, "%s/dec_dump_%ux%u.raw", dump_path, vf->width, vf->height);

		fp = filp_open(file_name,
				O_CREAT | O_RDWR | O_LARGEFILE | O_APPEND, 0600);

		if (!IS_ERR(fp)) {
			struct vb2_buffer *vb = vb2_buf;
			// dump y data
			u8 *yuv_data_addr = aml_yuv_dump(fp, (u8 *)vb2_plane_vaddr(vb, 0),
				vf->width, vf->height, 64);
			// dump uv data
			if (dstbuf->frame_buffer.num_planes == 1) {
				aml_yuv_dump(fp, yuv_data_addr, vf->width,
					vf->height / 2, 64);
			} else {
				aml_yuv_dump(fp, (u8 *)vb2_plane_vaddr(vb, 1),
					vf->width, vf->height / 2, 64);
			}

			pr_info("dump idx: %d %dx%d\n", dump_capture_frame, vf->width, vf->height);
			dump_capture_frame--;
			filp_close(fp, NULL);
		}
	} while(0);

	ATRACE_COUNTER("VC_OUT_VSINK-1.submit", vb2_buf->index);
	ATRACE_COUNTER("V_ST_VSINK-input_buffering", vdec_frame_number(ctx->ada_ctx));

	if (vf->flag & VFRAME_FLAG_EMPTY_FRAME_V4L) {
		dstbuf->vb.flags = V4L2_BUF_FLAG_LAST;
		if (dstbuf->frame_buffer.num_planes == 1) {
			vb2_set_plane_payload(vb2_buf, 0, 0);
		} else if (dstbuf->frame_buffer.num_planes == 2) {
			vb2_set_plane_payload(vb2_buf, 0, 0);
			vb2_set_plane_payload(vb2_buf, 1, 0);
		}
		ctx->has_receive_eos = true;
		v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
			"recevie a empty frame. idx: %d, state: %d\n",
			vb2_buf->index, vb2_buf->state);
	}

	v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
		"receive vbuf idx: %d, state: %d\n",
		vb2_buf->index, vb2_buf->state);

	if (vf->flag & VFRAME_FLAG_EMPTY_FRAME_V4L) {
		if (ctx->v4l_resolution_change) {
			/* make the run to stanby until new buffs to enque. */
			ctx->v4l_codec_dpb_ready = false;
			ctx->reset_flag = V4L_RESET_MODE_LIGHT;
			ctx->vpp_cfg.res_chg = true;

			/*
			 * After all buffers containing decoded frames from
			 * before the resolution change point ready to be
			 * dequeued on the CAPTURE queue, the driver sends a
			 * V4L2_EVENT_SOURCE_CHANGE event for source change
			 * type V4L2_EVENT_SRC_CH_RESOLUTION, also the upper
			 * layer will get new information from cts->picinfo.
			 */
			aml_vdec_dispatch_event(ctx, V4L2_EVENT_SRC_CH_RESOLUTION);
		} else
			aml_vdec_dispatch_event(ctx, V4L2_EVENT_SEND_EOS);
	}

	if (dstbuf->vb.vb2_buf.state == VB2_BUF_STATE_ACTIVE) {
		/* binding vframe handle. */
		if (is_cpu_t7()) {
			if (vf->canvas0_config[0].block_mode == CANVAS_BLKMODE_LINEAR) {
				if ((ctx->output_pix_fmt != V4L2_PIX_FMT_H264) &&
					(ctx->output_pix_fmt != V4L2_PIX_FMT_MPEG1) &&
					(ctx->output_pix_fmt != V4L2_PIX_FMT_MPEG2) &&
					(ctx->output_pix_fmt != V4L2_PIX_FMT_MPEG4) &&
					(ctx->output_pix_fmt != V4L2_PIX_FMT_MJPEG)) {
					vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
				}
				else {
					if (fb->status == FB_ST_GE2D)
						vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
				}
			}
		} else {
			if (vf->canvas0_config[0].block_mode == CANVAS_BLKMODE_LINEAR)
				vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
		}

		vf->omx_index = vf->index_disp;
		dstbuf->privdata.vf = *vf;

		if (vb2_buf->memory == VB2_MEMORY_DMABUF) {
			struct dma_buf * dma;

			dma = dstbuf->vb.vb2_buf.planes[0].dbuf;
			if (dmabuf_is_uvm(dma)) {
				/* only Y will contain vframe */
				comp_buf_set_vframe(ctx, vb2_buf, vf);
				v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
					"set vf(%px) into %dth buf\n",
					vf, vb2_buf->index);
			}
		}

		fb_map_table_hold(ctx, vb2_buf, vf, fb->task, dstbuf->internal_index);

		if (vf->frame_type & V4L2_BUF_FLAG_ERROR)
			v4l2_buff_done(&dstbuf->vb, VB2_BUF_STATE_ERROR);
		else
			v4l2_buff_done(&dstbuf->vb, VB2_BUF_STATE_DONE);

		fb->status = FB_ST_DISPLAY;
	}

	mutex_lock(&ctx->state_lock);
	if (ctx->state == AML_STATE_FLUSHING &&
		ctx->has_receive_eos) {
		ctx->state = AML_STATE_FLUSHED;
		ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
		v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
			"vcodec state (AML_STATE_FLUSHED)\n");
	}
	mutex_unlock(&ctx->state_lock);

	if (ctx->post_to_upper_done == false) {
		ctx->post_to_upper_done = true;
		wake_up_interruptible(&ctx->post_done_wq);
	}

	ctx->decoded_frame_cnt++;
}

static void fill_capture_done_cb(void *v4l_ctx, void *fb_ctx)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)v4l_ctx;
	struct vdec_v4l2_buffer *fb =
		(struct vdec_v4l2_buffer *)fb_ctx;
	struct aml_video_dec_buf *aml_buff =
		container_of(fb, struct aml_video_dec_buf, frame_buffer);
	struct vb2_v4l2_buffer *vb = &aml_buff->vb;

	if (ctx->is_stream_off) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_INPUT,
			"ignore buff idx: %d streamoff\n", fb->buf_idx);
		return;
	}

	ATRACE_COUNTER("VC_OUT_VSINK-0.receive", vb->vb2_buf.index);

	mutex_lock(&ctx->capture_buffer_lock);
	kfifo_put(&ctx->capture_buffer, vb);
	mutex_unlock(&ctx->capture_buffer_lock);

	aml_thread_post_task(ctx, AML_THREAD_CAPTURE);
}

static void update_vdec_buf_plane(struct aml_vcodec_ctx *ctx,
				  struct vdec_v4l2_buffer *fb,
				  struct vb2_buffer *vb)
{
	int i;
	char plane_n[3] = {'Y','U','V'};

	fb->num_planes = vb->num_planes;
	fb->buf_idx = vb->index;

	for (i = 0 ; i < vb->num_planes ; i++) {
		fb->m.mem[i].addr	= vb2_dma_contig_plane_dma_addr(vb, i);
		fb->m.mem[i].dbuf	= vb->planes[i].dbuf;
		if (i == 0) {
			//Y
			if (vb->num_planes == 1) {
				fb->m.mem[0].size	= ctx->picinfo.y_len_sz +
					ctx->picinfo.c_len_sz;
				fb->m.mem[0].offset = ctx->picinfo.y_len_sz;
			} else {
				fb->m.mem[0].size	= ctx->picinfo.y_len_sz;
				fb->m.mem[0].offset = 0;
			}
		} else {
			if (vb->num_planes == 2) {
				//UV
				fb->m.mem[1].size	= ctx->picinfo.c_len_sz;
				fb->m.mem[1].offset = ctx->picinfo.c_len_sz >> 1;
			} else {
				fb->m.mem[i].size  = ctx->picinfo.c_len_sz >> 1;
				fb->m.mem[i].offset = 0;
			}
		}

		v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
				"idx: %u, %c:(0x%lx, %d)\n", vb->index,
				plane_n[i], fb->m.mem[i].addr, fb->m.mem[i].size);
	}
}

static bool fb_token_insert(struct aml_vcodec_ctx *ctx,
			    ulong *token)
{
	ulong vb_handle;
	int i;

	for (i = 0; i < ARRAY_SIZE(ctx->token_table); i++) {
		if (ctx->token_table[i] &&
			(ctx->token_table[i] == *token)) {
			return true;
		}
	}

	if (!v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx))
		return false;

	vb_handle = (ulong)v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	for (i = 0; i < ARRAY_SIZE(ctx->token_table); i++) {
		if (!ctx->token_table[i]) {
			ctx->token_table[i] = vb_handle;
			break;
		}
	}

	if (i >= ARRAY_SIZE(ctx->token_table)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"%s, table is full. token:%lx\n",
			__func__, vb_handle);
		return false;
	}

	*token = vb_handle;

	return true;
}

static void fb_token_remove(struct aml_vcodec_ctx *ctx,
			    ulong token)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ctx->token_table); i++) {
		if (token == ctx->token_table[i]) {
			ctx->token_table[i] = 0;
			break;
		}
	}

	if (i >= ARRAY_SIZE(ctx->token_table)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"%s, remove token err, token:%lx.\n",
			__func__, token);
	}
}

static void fb_token_clean(struct aml_vcodec_ctx *ctx)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ctx->token_table); i++) {
		ctx->token_table[i] = 0;
	}

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO, "%s done\n", __func__);
}

static bool fb_buff_query(struct aml_fb_ops *fb, ulong *token)
{
	struct aml_vcodec_ctx *ctx =
		container_of(fb, struct aml_vcodec_ctx, fb_ops);
	struct vb2_queue * que = v4l2_m2m_get_dst_vq(ctx->m2m_ctx);
	bool ret = false;
	ulong flags;

	if (!que->streaming)
		return false;

	flags = aml_vcodec_ctx_lock(ctx);

	ret = fb_token_insert(ctx, token);

	aml_vcodec_ctx_unlock(ctx, flags);

	return ret;
}

static void aml_task_chain_remove(struct aml_vcodec_ctx *ctx)
{
	struct task_chain_s *task, *tmp;

	list_for_each_entry_safe(task, tmp, &ctx->task_chain_pool, node) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
			"remove task chain:%d, %px\n", task->id, task);
		list_del(&task->node);
		task_chain_clean(task);
		task_chain_release(task);
	}
}

static struct task_ops_s *get_v4l_sink_ops(void);

static void aml_creat_pipeline(struct aml_vcodec_ctx *ctx,
			       struct vdec_v4l2_buffer *fb,
			       u32 requester)
{
	struct task_chain_s *task = fb->task;
	/*
	 * line 1: dec <==> vpp <==> v4l-sink, for P / P + DI.NR.
	 * line 2: dec <==> vpp, vpp <==> v4l-sink, for I / I + DI.NR.
	 * line 3: dec <==> v4l-sink, only for P.
	 * line 4: dec <==> ge2d, ge2d <==> v4l-sink, used for fmt convert.
	 * line 5: dec <==> ge2d, ge2d <==>vpp, vpp <==> v4l-sink.
	 * line 6: dec <==> ge2d, ge2d <==> vpp <==> v4l-sink.
	 */

	switch (requester) {
	case AML_FB_REQ_DEC:
		if (ctx->ge2d) {
			/* dec <==> ge2d. */
			task->attach(task, get_ge2d_ops(), ctx->ge2d);
		} else if (ctx->vpp) {
			if (ctx->vpp->is_prog) {
				/* dec <==> vpp <==> v4l-sink. */
				task->attach(task, get_v4l_sink_ops(), ctx);
				task->attach(task, get_vpp_ops(), ctx->vpp);
			} else {
				/* dec <==> vpp. */
				task->attach(task, get_vpp_ops(), ctx->vpp);
			}
		} else {
			/* dec <==> v4l-sink. */
			task->attach(task, get_v4l_sink_ops(), ctx);
		}
		break;

	case AML_FB_REQ_GE2D:
		if (ctx->vpp) {
			if (ctx->vpp->is_prog) {
				/* ge2d <==> vpp <==> v4l-sink. */
				task->attach(task, get_v4l_sink_ops(), ctx);
				task->attach(task, get_vpp_ops(), ctx->vpp);
				task->attach(task, get_ge2d_ops(), ctx->ge2d);
			} else {
				/* ge2d <==> vpp. */
				task->attach(task, get_vpp_ops(), ctx->vpp);
				task->attach(task, get_ge2d_ops(), ctx->ge2d);
			}
		} else {
			/* ge2d <==> v4l-sink. */
			task->attach(task, get_v4l_sink_ops(), ctx);
			task->attach(task, get_ge2d_ops(), ctx->ge2d);
		}
		break;

	case AML_FB_REQ_VPP:
		/* vpp <==> v4l-sink. */
		task->attach(task, get_v4l_sink_ops(), ctx);
		task->attach(task, get_vpp_ops(), ctx->vpp);
		break;

	default:
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"unsupport requester %x\n", requester);
	}
}

static int fb_buff_from_queue(struct aml_fb_ops *fb_ops,
		ulong token, struct vdec_v4l2_buffer **out_fb,
		u32 requester)
{
	struct aml_vcodec_ctx *ctx =
		container_of(fb_ops, struct aml_vcodec_ctx, fb_ops);
	struct aml_video_dec_buf *aml_buf = NULL;
	struct vb2_v4l2_buffer *v4l_buf = NULL;
	struct vdec_v4l2_buffer *fb = NULL;
	u32 buf_status = 0;
	ulong flags;

	flags = aml_vcodec_ctx_lock(ctx);

	if (ctx->is_stream_off) {
		aml_vcodec_ctx_unlock(ctx, flags);
		return -1;
	}

	v4l_buf = (struct vb2_v4l2_buffer *) token;
	if (!v4l_buf) {
		aml_vcodec_ctx_unlock(ctx, flags);
		return -1;
	}

	aml_buf = container_of(v4l_buf, struct aml_video_dec_buf, vb);

	fb		= &aml_buf->frame_buffer;
	fb->buf_idx	= v4l_buf->vb2_buf.index;
	aml_buf->used	= true;
	ctx->buf_used_count++;

	if (requester == AML_FB_REQ_VPP) {
		buf_status = V4L_CAP_BUFF_IN_VPP;
		ctx->cap_pool.vpp++;
	} else if (requester == AML_FB_REQ_DEC) {
		buf_status = V4L_CAP_BUFF_IN_DEC;
		ctx->cap_pool.dec++;
	} else if (requester == AML_FB_REQ_GE2D) {
		buf_status = V4L_CAP_BUFF_IN_GE2D;
		ctx->cap_pool.ge2d++;
	}

	ctx->cap_pool.seq[ctx->cap_pool.out++] =
		(buf_status << 16 | fb->buf_idx);

	update_vdec_buf_plane(ctx, fb, &v4l_buf->vb2_buf);

	aml_creat_pipeline(ctx, fb, requester);

	fb_token_remove(ctx, token);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
		"vid:%d, task:%px, phy:%lx, state:%d, ready:%d, requester:%d\n",
		fb->buf_idx, fb->task, fb->m.mem[0].addr, v4l_buf->vb2_buf.state,
		v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx), requester);

	ATRACE_COUNTER("VC_IN_VSINK-3.require", v4l_buf->vb2_buf.index);

	*out_fb = fb;

	aml_vcodec_ctx_unlock(ctx, flags);

	return 0;
}

static struct task_ops_s v4l_sink_ops = {
	.type		= TASK_TYPE_V4L_SINK,
	.fill_buffer	= fill_capture_done_cb,
};

static struct task_ops_s *get_v4l_sink_ops(void)
{
	return &v4l_sink_ops;
}

void aml_vdec_basic_information(struct aml_vcodec_ctx *ctx)
{
	struct aml_q_data *outq = NULL;
	struct aml_q_data *capq = NULL;
	struct vdec_pic_info pic;

	if (vdec_if_get_param(ctx, GET_PARAM_PIC_INFO, &pic)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"get pic info err\n");
		return;
	}

	outq = aml_vdec_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	capq = aml_vdec_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);

	pr_info("\n==== Show Basic Information ==== \n");

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
		"Format     : %s\n",
		outq->fmt->name);
	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
		"Color space: %s\n",
		capq->fmt->name);
	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
		"Scan type  : %s\n",
		(pic.field == V4L2_FIELD_NONE) ?
		"Progressive" : "Interlaced");
	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
		"Resolution : visible(%dx%d), coded(%dx%d)\n",
		pic.visible_width, pic.visible_height,
		pic.coded_width, pic.coded_height);
	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
		"Buffer num : dec:%d, vpp:%d, ge2d:%d, margin:%d, total:%d\n",
		ctx->picinfo.dpb_frames, ctx->vpp_size, ctx->ge2d_size,
		ctx->picinfo.dpb_margin, CTX_BUF_TOTAL(ctx));
	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
		"Config     : dw:%d, drm:%d, byp:%d, lc:%d, nr:%d, ge2d:%x\n",
		ctx->config.parm.dec.cfg.double_write_mode,
		ctx->is_drm_mode,
		ctx->vpp_cfg.is_bypass_p,
		ctx->vpp_cfg.enable_local_buf,
		ctx->vpp_cfg.enable_nr,
		ctx->ge2d_cfg.mode);
}

void aml_buffer_status(struct aml_vcodec_ctx *ctx)
{
	struct vb2_v4l2_buffer *vb = NULL;
	struct aml_video_dec_buf *aml_buff = NULL;
	struct vdec_v4l2_buffer *fb = NULL;
	struct vb2_queue *q = NULL;
	ulong flags;
	int i;

	flags = aml_vcodec_ctx_lock(ctx);

	q = v4l2_m2m_get_vq(ctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (!q->streaming) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"can't achieve buffers status before start streaming.\n");
	}

	pr_info("\n==== Show Buffer Status ======== \n");
	for (i = 0; i < q->num_buffers; ++i) {
		vb = to_vb2_v4l2_buffer(q->bufs[i]);
		aml_buff = container_of(vb, struct aml_video_dec_buf, vb);
		fb = &aml_buff->frame_buffer;

		/* print out task chain status. */
		task_chain_show(fb->task);
	}

	aml_vcodec_ctx_unlock(ctx, flags);
}

static void aml_check_dpb_ready(struct aml_vcodec_ctx *ctx)
{
	if (!ctx->v4l_codec_dpb_ready) {
		/*
		 * make sure enough dst bufs for decoding.
		 */
		if ((ctx->dpb_size) && (ctx->cap_pool.in >= ctx->dpb_size))
			ctx->v4l_codec_dpb_ready = true;

		v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
			"dpb: %d, vpp: %d, ready: %d, used: %d, dpb is ready: %s\n",
			ctx->dpb_size, ctx->vpp_size,
			v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx),
			ctx->cap_pool.out, ctx->v4l_codec_dpb_ready ? "yes" : "no");
	}
}

static void reconfig_vpp_status(struct aml_vcodec_ctx *ctx)
{
	if (bypass_nr_flag &&
		!ctx->vpp_cfg.is_prog &&
		((ctx->vpp_cfg.mode == VPP_MODE_NOISE_REDUC_LOCAL) ||
		(ctx->vpp_cfg.mode == VPP_MODE_NOISE_REDUC))) {
		ctx->vpp_cfg.enable_nr = 0;
		ctx->vpp_cfg.enable_local_buf = 0;

		ctx->vpp_cfg.mode = VPP_MODE_DI;
	}
}

static int is_vdec_ready(struct aml_vcodec_ctx *ctx)
{
	struct aml_vcodec_dev *dev = ctx->dev;

	if (!is_input_ready(ctx->ada_ctx)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"the decoder input has not ready.\n");
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		return 0;
	}

	if (ctx->state == AML_STATE_PROBE) {
		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_PROBE) {
			ctx->state = AML_STATE_READY;
			ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
			v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
				"vcodec state (AML_STATE_READY)\n");
		}
		mutex_unlock(&ctx->state_lock);
	}

	mutex_lock(&ctx->state_lock);
	if (ctx->state == AML_STATE_READY) {
		if (ctx->m2m_ctx->out_q_ctx.q.streaming &&
			ctx->m2m_ctx->cap_q_ctx.q.streaming) {
			ctx->state = AML_STATE_ACTIVE;
			ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
			v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
				"vcodec state (AML_STATE_ACTIVE)\n");
		}
	}
	mutex_unlock(&ctx->state_lock);

	/* check dpb ready */
	//aml_check_dpb_ready(ctx);

	return 1;
}

static bool is_enough_work_items(struct aml_vcodec_ctx *ctx)
{
	struct aml_vcodec_dev *dev = ctx->dev;

	if (vdec_frame_number(ctx->ada_ctx) >= WORK_ITEMS_MAX) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		return false;
	}

	return true;
}

static void aml_wait_buf_ready(struct aml_vcodec_ctx *ctx)
{
	ulong expires;

	expires = jiffies + msecs_to_jiffies(1000);
	while (!ctx->v4l_codec_dpb_ready) {
		u32 ready_num = 0;

		if (time_after(jiffies, expires)) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
				"the DPB state has not ready.\n");
			break;
		}

		ready_num = v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx);
		if ((ready_num + ctx->buf_used_count) >= CTX_BUF_TOTAL(ctx))
			ctx->v4l_codec_dpb_ready = true;
	}
}

void dmabuff_recycle_worker(struct work_struct *work)
{
	struct aml_vcodec_ctx *ctx =
		container_of(work, struct aml_vcodec_ctx, dmabuff_recycle_work);
	struct vb2_v4l2_buffer *vb = NULL;
	struct aml_video_dec_buf *buf = NULL;
	unsigned long flags;

	for (;;) {
		spin_lock_irqsave(&ctx->dmabuff_recycle_lock, flags);
		if (!kfifo_get(&ctx->dmabuff_recycle, &vb)) {
			spin_unlock_irqrestore(&ctx->dmabuff_recycle_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&ctx->dmabuff_recycle_lock, flags);

		buf = container_of(vb, struct aml_video_dec_buf, vb);

		if (ctx->is_out_stream_off)
			continue;

		if (wait_event_interruptible_timeout
			(ctx->wq, buf->used == false,
			 msecs_to_jiffies(200)) == 0) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
				"wait recycle dma buff timeout.\n");
		}

		v4l_dbg(ctx, V4L_DEBUG_CODEC_INPUT,
			"recycle buff idx: %d, vbuf: %lx\n", vb->vb2_buf.index,
			(ulong)sg_dma_address(buf->out_sgt->sgl));

		ATRACE_COUNTER("VO_OUT_VSINK-2.write_secure_end", vb->vb2_buf.index);

		if (vb->vb2_buf.state != VB2_BUF_STATE_ERROR)
			v4l2_buff_done(vb, buf->error ? VB2_BUF_STATE_ERROR :
				VB2_BUF_STATE_DONE);
	}
}

void aml_recycle_dma_buffers(struct aml_vcodec_ctx *ctx, u32 handle)
{
	struct aml_vcodec_dev *dev = ctx->dev;
	struct vb2_v4l2_buffer *vb = NULL;
	struct vb2_queue *q = NULL;
	int index = handle & 0xf;
	unsigned long flags;

	if (ctx->is_out_stream_off) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_INPUT,
			"ignore buff idx: %d streamoff\n", index);
		return;
	}

	q = v4l2_m2m_get_vq(ctx->m2m_ctx,
		V4L2_BUF_TYPE_VIDEO_OUTPUT);

	vb = to_vb2_v4l2_buffer(q->bufs[index]);

	spin_lock_irqsave(&ctx->dmabuff_recycle_lock, flags);
	kfifo_put(&ctx->dmabuff_recycle, vb);
	spin_unlock_irqrestore(&ctx->dmabuff_recycle_lock, flags);

	queue_work(dev->decode_workqueue, &ctx->dmabuff_recycle_work);
}

static void aml_vdec_worker(struct work_struct *work)
{
	struct aml_vcodec_ctx *ctx =
		container_of(work, struct aml_vcodec_ctx, decode_work);
	struct aml_vcodec_dev *dev = ctx->dev;
	struct aml_video_dec_buf *aml_buf;
	struct vb2_v4l2_buffer *vb2_v4l2;
	struct vb2_buffer *vb;
	struct aml_vcodec_mem buf;
	bool res_chg = false;
	int ret;

	if (ctx->state < AML_STATE_INIT ||
		ctx->state > AML_STATE_FLUSHED) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		goto out;
	}

	if (!is_vdec_ready(ctx)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"the decoder has not ready.\n");
		goto out;
	}

	vb2_v4l2 = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (vb2_v4l2 == NULL) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"src_buf empty.\n");
		goto out;
	}

	vb = (struct vb2_buffer *)vb2_v4l2;

	/*this case for google, but some frames are droped on ffmpeg, so disabled temp.*/
	if (0 && !is_enough_work_items(ctx))
		goto out;

	aml_buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);
	if (aml_buf->lastframe) {
		ulong expires;

		/*the empty data use to flushed the decoder.*/
		v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
			"Got empty flush input buffer.\n");

		/*
		 * when inputs a small amount of src buff, then soon to
		 * switch state FLUSHING, must to wait the DBP to be ready.
		 * (!ctx->v4l_codec_dpb_ready) change to  only need one buf
		 * for run ready in new version.
		 */
		expires = jiffies + msecs_to_jiffies(5000);
		while ((vdec_frame_number(ctx->ada_ctx) > 0) &&
			(ctx->cap_pool.in < 1)) {
			if (time_after(jiffies, expires)) {
				aml_vdec_flush_decoder(ctx);
				v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
				v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
				aml_vdec_dispatch_event(ctx, V4L2_EVENT_REQUEST_EXIT);
				v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
					"capture buffer waiting timeout.\n");
				goto out;
			}
			usleep_range(5000, 5500);
		}

		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_ACTIVE) {
			ctx->state = AML_STATE_FLUSHING;// prepare flushing
			ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
			v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
				"vcodec state (AML_STATE_FLUSHING-LASTFRM)\n");
		}
		mutex_unlock(&ctx->state_lock);

		v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);

		/* sets eos data for vdec input. */
		aml_vdec_flush_decoder(ctx);

		goto out;
	}

	buf.index	= vb->index;
	buf.vaddr	= vb2_plane_vaddr(vb, 0);
	buf.addr	= sg_dma_address(aml_buf->out_sgt->sgl);
	buf.size	= vb->planes[0].bytesused;
	buf.model	= vb->memory;
	buf.timestamp	= vb->timestamp;
	buf.meta_ptr	= (ulong)aml_buf->meta_data;

	if (!buf.vaddr && !buf.addr) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"id=%d src_addr is NULL.\n", vb->index);
		goto out;
	}

	aml_buf->used = true;

	/* v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
		"size: 0x%zx, crc: 0x%x\n",
		buf.size, crc32(0, buf.va, buf.size));*/

	/* pts = (time / 10e6) * (90k / fps) */
	/*v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
		"timestamp: 0x%llx\n", src_buf->timestamp);*/

	if (ctx->is_drm_mode &&
		(buf.model == VB2_MEMORY_DMABUF)) {
		ATRACE_COUNTER("VO_IN_VSINK-2.write_secure", buf.size);
	} else {
		ATRACE_COUNTER("VO_IN_VSINK-2.write", buf.size);
	}

	ATRACE_COUNTER("V_ST_VSINK-input_buffering", vdec_frame_number(ctx->ada_ctx));

	ret = vdec_if_decode(ctx, &buf, &res_chg);
	if (ret > 0) {
		/*
		 * we only return src buffer with VB2_BUF_STATE_DONE
		 * when decode success without resolution change.
		 */
		aml_buf->used = false;
		v4l2_m2m_src_buf_remove(ctx->m2m_ctx);

		if (ctx->is_drm_mode &&
			(buf.model == VB2_MEMORY_DMABUF)) {
			wake_up_interruptible(&ctx->wq);
		} else {
			ATRACE_COUNTER("VO_OUT_VSINK-0.wrtie_end", buf.size);
			v4l2_buff_done(&aml_buf->vb,
				VB2_BUF_STATE_DONE);
		}
	} else if (ret && ret != -EAGAIN) {
		aml_buf->used = false;
		v4l2_m2m_src_buf_remove(ctx->m2m_ctx);

		if (ctx->is_drm_mode &&
			(buf.model == VB2_MEMORY_DMABUF)) {
			wake_up_interruptible(&ctx->wq);
		} else {
			ATRACE_COUNTER("VO_OUT_VSINK-3.write_error", buf.size);
			v4l2_buff_done(&aml_buf->vb,
				VB2_BUF_STATE_ERROR);
		}

		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"error processing src data. %d.\n", ret);
	} else if (res_chg) {
		/* wait the DPB state to be ready. */
		aml_wait_buf_ready(ctx);

		aml_buf->used = false;
		aml_vdec_pic_info_update(ctx);
		/*
		 * On encountering a resolution change in the stream.
		 * The driver must first process and decode all
		 * remaining buffers from before the resolution change
		 * point, so call flush decode here
		 */
		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_ACTIVE) {
			ctx->state = AML_STATE_FLUSHING;// prepare flushing
			ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
			v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
				"vcodec state (AML_STATE_FLUSHING-RESCHG)\n");
		}
		mutex_unlock(&ctx->state_lock);

		ctx->v4l_resolution_change = true;
		while (ctx->m2m_ctx->job_flags & TRANS_RUNNING) {
			v4l2_m2m_job_pause(dev->m2m_dev_dec, ctx->m2m_ctx);
		}

		aml_vdec_flush_decoder(ctx);

		goto out;
	} else {
		ATRACE_COUNTER("VO_OUT_VSINK-1.write_again", buf.size);
		/* decoder is lack of resource, retry after short delay */
		if (vdec_get_instance_num() < 2)
			usleep_range(2000, 4000);
	}

	v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
out:
	return;
}

static void aml_vdec_reset(struct aml_vcodec_ctx *ctx)
{
	if (ctx->state == AML_STATE_ABORT) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"the decoder will be exited.\n");
		goto out;
	}

	if (aml_codec_reset(ctx->ada_ctx, &ctx->reset_flag)) {
		ctx->state = AML_STATE_ABORT;
		ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
		v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
			"vcodec state (AML_STATE_ABORT).\n");
	}
out:
	complete(&ctx->comp);
	return;
}

void wait_vcodec_ending(struct aml_vcodec_ctx *ctx)
{
	/* disable queue output item to worker. */
	ctx->output_thread_ready = false;
	ctx->is_stream_off = true;

	/* flush output buffer worker. */
	cancel_work_sync(&ctx->decode_work);
	cancel_work_sync(&ctx->dmabuff_recycle_work);

	/* clean output cache and decoder status . */
	if (ctx->state > AML_STATE_INIT)
		aml_vdec_reset(ctx);

	/* pause the job and clean trans status. */
	while (ctx->m2m_ctx->job_flags & TRANS_RUNNING) {
		v4l2_m2m_job_pause(ctx->dev->m2m_dev_dec, ctx->m2m_ctx);
	}

	ctx->v4l_codec_dpb_ready = false;
}

void aml_thread_capture_worker(struct aml_vcodec_ctx *ctx)
{
	struct vb2_v4l2_buffer *vb = NULL;
	struct aml_video_dec_buf *aml_buff = NULL;
	struct vdec_v4l2_buffer *fb = NULL;

	for (;;) {
		mutex_lock(&ctx->capture_buffer_lock);
		if (!kfifo_get(&ctx->capture_buffer, &vb)) {
			mutex_unlock(&ctx->capture_buffer_lock);
			break;
		}
		mutex_unlock(&ctx->capture_buffer_lock);

		aml_buff = container_of(vb, struct aml_video_dec_buf, vb);
		fb = &aml_buff->frame_buffer;

		if (ctx->is_stream_off)
			continue;

		post_frame_to_upper(ctx, fb);
	}
}
EXPORT_SYMBOL_GPL(aml_thread_capture_worker);

static int vdec_capture_thread(void *data)
{
	struct aml_vdec_thread *thread =
		(struct aml_vdec_thread *) data;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *) thread->priv;

	for (;;) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
			"%s, state: %d\n", __func__, ctx->state);

		if (down_interruptible(&thread->sem))
			break;

		if (thread->stop)
			break;

		/* handle event. */
		thread->func(ctx);
	}

	while (!kthread_should_stop()) {
		usleep_range(1000, 2000);
	}

	return 0;
}

void aml_thread_post_task(struct aml_vcodec_ctx *ctx,
	enum aml_thread_type type)
{
	struct aml_vdec_thread *thread = NULL;
	ulong flags;

	spin_lock_irqsave(&ctx->tsplock, flags);
	list_for_each_entry(thread, &ctx->vdec_thread_list, node) {
		if (thread->task == NULL)
			continue;

		if (thread->type == type)
			up(&thread->sem);
	}
	spin_unlock_irqrestore(&ctx->tsplock, flags);
}
EXPORT_SYMBOL_GPL(aml_thread_post_task);

int aml_thread_start(struct aml_vcodec_ctx *ctx, aml_thread_func func,
	enum aml_thread_type type, const char *thread_name)
{
	struct aml_vdec_thread *thread;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret = 0;

	thread = kzalloc(sizeof(*thread), GFP_KERNEL);
	if (thread == NULL)
		return -ENOMEM;

	thread->type = type;
	thread->func = func;
	thread->priv = ctx;
	sema_init(&thread->sem, 0);

	thread->task = kthread_run(vdec_capture_thread, thread, "aml-%s-%d", thread_name, ctx->id);
	if (IS_ERR(thread->task)) {
		ret = PTR_ERR(thread->task);
		thread->task = NULL;
		goto err;
	}
	sched_setscheduler_nocheck(thread->task, SCHED_FIFO, &param);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
			"%s, policy is:%d priority is:%d\n",
			__func__, thread->task->policy, thread->task->rt_priority);

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
	ulong flags;

	while (!list_empty(&ctx->vdec_thread_list)) {
		thread = list_entry(ctx->vdec_thread_list.next,
			struct aml_vdec_thread, node);
		spin_lock_irqsave(&ctx->tsplock, flags);
		list_del(&thread->node);
		spin_unlock_irqrestore(&ctx->tsplock, flags);

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
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, cmd: %u\n", __func__, cmd->cmd);

	switch (cmd->cmd) {
	case V4L2_DEC_CMD_STOP:
	case V4L2_DEC_CMD_START:
		if (cmd->cmd == V4L2_DEC_CMD_START) {
			if (cmd->start.speed == ~0)
				cmd->start.speed = 0;
			if (cmd->start.format == ~0)
				cmd->start.format = 0;
		}

		if (cmd->flags == ~0)
			cmd->flags = 0;

		if ((cmd->flags != 0) && (cmd->flags != ~0)) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"cmd->flags=%u\n", cmd->flags);
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

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, cmd: %u\n", __func__, cmd->cmd);

	ret = vidioc_try_decoder_cmd(file, priv, cmd);
	if (ret)
		return ret;

	switch (cmd->cmd) {
	case V4L2_DEC_CMD_STOP:
		src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		if (!vb2_is_streaming(src_vq)) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
				"Output stream is off. No need to flush.\n");
			return 0;
		}

		dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
			multiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
			V4L2_BUF_TYPE_VIDEO_CAPTURE);
		if (!vb2_is_streaming(dst_vq)) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
				"Capture stream is off. No need to flush.\n");
			return 0;
		}

		/* flush pipeline */
		v4l2_m2m_buf_queue(ctx->m2m_ctx, &ctx->empty_flush_buf->vb);
		v4l2_m2m_try_schedule(ctx->m2m_ctx);//pay attention
		ctx->receive_cmd_stop = true;

		v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
			"%s, receive cmd stop and prepare flush pipeline.\n", __func__);
		break;

	case V4L2_DEC_CMD_START:
		dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
			multiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
			V4L2_BUF_TYPE_VIDEO_CAPTURE);
		vb2_clear_last_buffer_dequeued(dst_vq);//pay attention

		v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
			"%s, receive cmd start.\n", __func__);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static void aml_wait_resource(struct aml_vcodec_ctx *ctx)
{
	ulong expires = jiffies + msecs_to_jiffies(1000);

	while (atomic_read(&ctx->dev->vpp_count) >= max_di_instance) {
		if (time_after(jiffies, expires)) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
				"wait resource timeout.\n");
			break;
		}
		usleep_range(2000, 4000);
	}
}

static int vidioc_decoder_streamon(struct file *file, void *priv,
	enum v4l2_buf_type i)
{
	struct v4l2_fh *fh = file->private_data;
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *q;

	q = v4l2_m2m_get_vq(fh->m2m_ctx, i);
	if (!V4L2_TYPE_IS_OUTPUT(q->type) &&
		ctx->is_stream_off) {
		if (ctx->ge2d_is_need) {
			int ret;

			if (ctx->ge2d) {
				aml_v4l2_ge2d_destroy(ctx->ge2d);
				ctx->ge2d = NULL;
			}

			ret = aml_v4l2_ge2d_init(ctx, &ctx->ge2d_cfg, &ctx->ge2d);
			if (ret) {
				v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
					"ge2d_wrapper init err:%d\n", ret);
				return ret;
			}
		}

		if (ctx->vpp_is_need) {
			int ret;

			if (ctx->vpp_cfg.fmt == 0)
				ctx->vpp_cfg.fmt = ctx->cap_pix_fmt;

			if (ctx->vpp == NULL)
				aml_wait_resource(ctx);

			if ((atomic_read(&ctx->dev->vpp_count) < max_di_instance) ||
				(ctx->vpp != NULL)) {
				if (ctx->vpp && ctx->vpp_cfg.is_vpp_reset &&
					(ctx->vpp->is_prog == ctx->vpp_cfg.is_prog) &&
					(ctx->vpp->is_bypass_p == ctx->vpp_cfg.is_bypass_p) &&
					(ctx->vpp->work_mode == ctx->vpp_cfg.mode)) {
					aml_v4l2_vpp_reset(ctx->vpp);
				} else {
					if (ctx->vpp) {
						aml_v4l2_vpp_destroy(ctx->vpp);
						atomic_dec(&ctx->dev->vpp_count);
						ctx->vpp = NULL;
					}
					atomic_inc(&ctx->dev->vpp_count);
					ret = aml_v4l2_vpp_init(ctx, &ctx->vpp_cfg, &ctx->vpp);
					if (ret) {
						atomic_dec(&ctx->dev->vpp_count);
						v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
							"vpp_wrapper init err:%d vpp_cfg.fmt: %d\n",
							ret, ctx->vpp_cfg.fmt);
						return ret;
					}

					v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
						"vpp_wrapper instance count: %d\n",
						atomic_read(&ctx->dev->vpp_count));
				}
			} else {
				ctx->vpp_cfg.enable_local_buf = 0;
				ctx->vpp_cfg.enable_nr = 0;
				ctx->picinfo.dpb_margin += ctx->vpp_size;
				ctx->dpb_size = ctx->picinfo.dpb_margin + ctx->picinfo.dpb_frames;
				ctx->vpp_size = 0;
				vdec_if_set_param(ctx, SET_PARAM_PIC_INFO, &ctx->picinfo);
				ctx->vpp_is_need = false;
			}
			ctx->vpp_cfg.is_vpp_reset = false;
		} else {
			if (ctx->vpp) {
				aml_v4l2_vpp_destroy(ctx->vpp);
				atomic_dec(&ctx->dev->vpp_count);
				ctx->vpp = NULL;
			}
		}

		ctx->is_stream_off = false;
	} else
		ctx->is_out_stream_off = false;

	if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		memset(&ctx->decoder_status_info, 0,
			sizeof(ctx->decoder_status_info));
	}


	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d\n", __func__, q->type);

	return v4l2_m2m_ioctl_streamon(file, priv, i);
}

static int vidioc_decoder_streamoff(struct file *file, void *priv,
	enum v4l2_buf_type i)
{
	struct v4l2_fh *fh = file->private_data;
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *q;
	ulong flags;

	q = v4l2_m2m_get_vq(fh->m2m_ctx, i);

	flags = aml_vcodec_ctx_lock(ctx);

	if (V4L2_TYPE_IS_OUTPUT(q->type))
		ctx->is_out_stream_off = true;
	else
		ctx->is_stream_off = true;

	aml_vcodec_ctx_unlock(ctx, flags);

	if (!V4L2_TYPE_IS_OUTPUT(q->type)) {
		if (ctx->vpp) {
			reconfig_vpp_status(ctx);
		}
	} else {
		ctx->index_disp = 0;
	}

	if (ctx->ge2d && ctx->is_stream_off) {
		aml_v4l2_ge2d_destroy(ctx->ge2d);
		ctx->ge2d = NULL;
		v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
			"ge2d destory in streamoff.\n");
	}

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d\n", __func__, q->type);

	return v4l2_m2m_ioctl_streamoff(file, priv, i);
}

static int vidioc_decoder_reqbufs(struct file *file, void *priv,
	struct v4l2_requestbuffers *rb)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_fh *fh = file->private_data;
	struct vb2_queue *q;

	q = v4l2_m2m_get_vq(fh->m2m_ctx, rb->type);

	if (!rb->count) {
		if (!V4L2_TYPE_IS_OUTPUT(rb->type)) {
			if (wait_event_interruptible_timeout
				(ctx->post_done_wq, ctx->post_to_upper_done == true,
				 msecs_to_jiffies(200)) == 0) {
				v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
					"wait post frame to upper finish timeout.\n");
			}
		}
		vb2_queue_release(q);
	}

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d, count: %d\n",
		__func__, q->type, rb->count);

	if (!V4L2_TYPE_IS_OUTPUT(rb->type)) {
		/* driver needs match v4l buffer number with total size*/
		if (rb->count > CTX_BUF_TOTAL(ctx)) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
					"reqbufs (st:%d) %d -> %d\n",
					ctx->state, rb->count, CTX_BUF_TOTAL(ctx));
			ctx->picinfo.dpb_margin += (rb->count - CTX_BUF_TOTAL(ctx));
			ctx->dpb_size = ctx->picinfo.dpb_frames + ctx->picinfo.dpb_margin;
			vdec_if_set_param(ctx, SET_PARAM_PIC_INFO, &ctx->picinfo);
			v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
					"%s buf updated, dec: %d (%d + %d), vpp %d\n",
					__func__,
					ctx->dpb_size,
					ctx->picinfo.dpb_frames,
					ctx->picinfo.dpb_margin,
					ctx->vpp_size);
			//rb->count = ctx->dpb_size;
		}
		ctx->capture_memory_mode = rb->memory;
		v4l_dbg(ctx, V4L_DEBUG_CODEC_OUTPUT,
			"capture buffer memory mode is %d\n", rb->memory);
	} else {
		ctx->output_dma_mode =
			(rb->memory == VB2_MEMORY_DMABUF) ? 1 : 0;

		v4l_dbg(ctx, V4L_DEBUG_CODEC_INPUT,
			"output buffer memory mode is %d\n", rb->memory);
	}

	return v4l2_m2m_ioctl_reqbufs(file, priv, rb);
}

static int vidioc_vdec_querybuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d\n", __func__, buf->type);

	return v4l2_m2m_ioctl_querybuf(file, priv, buf);
}

static int vidioc_vdec_expbuf(struct file *file, void *priv,
	struct v4l2_exportbuffer *eb)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d\n", __func__, eb->type);

	return v4l2_m2m_ioctl_expbuf(file, priv, eb);
}

void aml_vcodec_dec_release(struct aml_vcodec_ctx *ctx)
{
	ulong flags;
	if (ctx->capture_memory_mode == VB2_MEMORY_MMAP) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,"clean proxy uvm\n");
		aml_clean_proxy_uvm(ctx);
	}

	if (kref_read(&ctx->box_ref))
		kref_put(&ctx->box_ref, box_release);

	flags = aml_vcodec_ctx_lock(ctx);
	ctx->state = AML_STATE_ABORT;
	ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
	v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
		"vcodec state (AML_STATE_ABORT)\n");
	aml_vcodec_ctx_unlock(ctx, flags);

	vdec_if_deinit(ctx);
}

void aml_vcodec_dec_set_default_params(struct aml_vcodec_ctx *ctx)
{
	struct aml_q_data *q_data;

	ctx->m2m_ctx->q_lock = &ctx->dev->dev_mutex;
	ctx->fh.m2m_ctx = ctx->m2m_ctx;
	ctx->fh.ctrl_handler = &ctx->ctrl_hdl;
	INIT_WORK(&ctx->decode_work, aml_vdec_worker);
	ctx->colorspace = V4L2_COLORSPACE_REC709;
	ctx->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	ctx->quantization = V4L2_QUANTIZATION_DEFAULT;
	ctx->xfer_func = V4L2_XFER_FUNC_DEFAULT;
	ctx->dev->dec_capability = 0;//VCODEC_CAPABILITY_4K_DISABLED;//disable 4k

	q_data = &ctx->q_data[AML_Q_DATA_SRC];
	memset(q_data, 0, sizeof(struct aml_q_data));
	q_data->visible_width = DFT_CFG_WIDTH;
	q_data->visible_height = DFT_CFG_HEIGHT;
	q_data->coded_width = DFT_CFG_WIDTH;
	q_data->coded_height = DFT_CFG_HEIGHT;
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
	if (support_format_I420)
		q_data->fmt = &aml_video_formats[CAP_FMT_I420_IDX];

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
	ctx->reset_flag = V4L_RESET_MODE_NORMAL;

	ctx->fb_ops.query	= fb_buff_query;
	ctx->fb_ops.alloc	= fb_buff_from_queue;

	ctx->state = AML_STATE_IDLE;
	ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
	v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
		"vcodec state (AML_STATE_IDLE)\n");
}

static int vidioc_vdec_qbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ret;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d\n", __func__, buf->type);

	if (ctx->state == AML_STATE_ABORT) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Call on QBUF after unrecoverable error, type = %s\n",
			V4L2_TYPE_IS_OUTPUT(buf->type) ? "OUT" : "IN");
		return -EIO;
	}

	ret = v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);

	if (V4L2_TYPE_IS_OUTPUT(buf->type)) {
		if (V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
			if (ret == -EAGAIN)
				ATRACE_COUNTER("VO_IN_VSINK-1.que_again", buf->m.planes[0].bytesused);
			else
				ATRACE_COUNTER("VO_IN_VSINK-0.que", buf->m.planes[0].bytesused);
		} else {
			if (ret == -EAGAIN)
				ATRACE_COUNTER("VO_IN_VSINK-1.que_again", buf->length);
			else
				ATRACE_COUNTER("VO_IN_VSINK-0.que", buf->length);
		}
	} else {
		if (ret == -EAGAIN)
			ATRACE_COUNTER("VC_IN_VSINK-1.que_again", buf->index);
		else
			ATRACE_COUNTER("VC_IN_VSINK-0.que", buf->index);
	}

	return ret;
}

static int vidioc_vdec_dqbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ret;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d\n", __func__, buf->type);

	if (ctx->state == AML_STATE_ABORT) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Call on DQBUF after unrecoverable error, type = %s\n",
			V4L2_TYPE_IS_OUTPUT(buf->type) ? "OUT" : "IN");
		if (!V4L2_TYPE_IS_OUTPUT(buf->type))
			return -EIO;
	}

	ret = v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
	if (!ret && !V4L2_TYPE_IS_OUTPUT(buf->type)) {
		struct vb2_queue *vq;
		struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
		struct aml_video_dec_buf *aml_buf = NULL;
		struct file *file = NULL;

		vq = v4l2_m2m_get_vq(ctx->m2m_ctx, buf->type);
		vb2_v4l2 = to_vb2_v4l2_buffer(vq->bufs[buf->index]);
		aml_buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);
		aml_buf->privdata.vb_handle	= (ulong) aml_buf;
		aml_buf->privdata.v4l_dec_ctx	= (ulong) ctx;

		file = fget(vb2_v4l2->private);
		if (file && is_v4l2_buf_file(file)) {
			dmabuf_fd_install_data(vb2_v4l2->private,
				(void*)&aml_buf->privdata,
				sizeof(struct file_private_data));
			v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO, "disp: %d, vf: %lx\n",
				aml_buf->privdata.vf.index_disp,
				(ulong) v4l_get_vf_handle(vb2_v4l2->private));
			fput(file);
		}
	}

	if (V4L2_TYPE_IS_OUTPUT(buf->type)) {
		if (V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
			if (ret == -EAGAIN)
				ATRACE_COUNTER("VO_OUT_VSINK-5.deque_again", buf->m.planes[0].bytesused);
			else
				ATRACE_COUNTER("VO_OUT_VSINK-4.deque", buf->m.planes[0].bytesused);
		} else {
			if (ret == -EAGAIN)
				ATRACE_COUNTER("VO_OUT_VSINK-5.deque_again", buf->length);
			else
				ATRACE_COUNTER("VO_OUT_VSINK-4.deque", buf->length);
		}
	} else {
		if (ret == -EAGAIN)
			ATRACE_COUNTER("VC_OUT_VSINK-3.deque_again", buf->index);
		else
			ATRACE_COUNTER("VC_OUT_VSINK-2.deque", buf->index);
	}

	return ret;
}

static int vidioc_vdec_querycap(struct file *file, void *priv,
	struct v4l2_capability *cap)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct video_device *vfd_dec = video_devdata(file);

	strlcpy(cap->driver, AML_VCODEC_DEC_NAME, sizeof(cap->driver));
	strlcpy(cap->bus_info, AML_PLATFORM_STR, sizeof(cap->bus_info));
	strlcpy(cap->card, AML_PLATFORM_STR, sizeof(cap->card));
	cap->device_caps = vfd_dec->device_caps;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s, %s\n", __func__, cap->card);

	return 0;
}

static int vidioc_vdec_subscribe_evt(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d\n", __func__, sub->type);

	switch (sub->type) {
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 2, NULL);
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	case V4L2_EVENT_PRIVATE_EXT_SEND_ERROR:
		return v4l2_event_subscribe(fh, sub, 5, NULL);
	default:
		return v4l2_ctrl_subscribe_event(fh, sub);
	}
}

static int vidioc_vdec_event_unsubscribe(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s, type: %d\n",
		__func__, sub->type);

	return v4l2_event_unsubscribe(fh, sub);
}

static int vidioc_try_fmt(struct v4l2_format *f, struct aml_video_fmt *fmt)
{
	int i;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct v4l2_pix_format *pix = &f->fmt.pix;

	if (V4L2_TYPE_IS_MULTIPLANAR(f->type)) {
		if (V4L2_TYPE_IS_OUTPUT(f->type)) {
			pix_mp->num_planes = 1;
			pix_mp->plane_fmt[0].bytesperline = 0;

			if ((pix_mp->pixelformat != V4L2_PIX_FMT_MPEG2) &&
			    (pix_mp->pixelformat != V4L2_PIX_FMT_H264) &&
			    (pix_mp->pixelformat != V4L2_PIX_FMT_MPEG1)) {
				pix_mp->field = V4L2_FIELD_NONE;
			} else if (pix_mp->field != V4L2_FIELD_NONE) {
				if (pix_mp->field == V4L2_FIELD_ANY)
					pix_mp->field = V4L2_FIELD_NONE;

				pr_info("%s, field: %u, fmt: %x\n",
					__func__, pix_mp->field,
					pix_mp->pixelformat);
			}
		} else {
			if (pix_mp->field != V4L2_FIELD_INTERLACED)
				pix_mp->field = V4L2_FIELD_NONE;
			pix_mp->height = clamp(pix_mp->height,
						AML_VDEC_MIN_H,
						AML_VDEC_MAX_H);
			pix_mp->width = clamp(pix_mp->width,
						AML_VDEC_MIN_W,
						AML_VDEC_MAX_W);

			pix_mp->num_planes = fmt->num_planes;

			pix_mp->plane_fmt[0].bytesperline = pix_mp->width;
			pix_mp->plane_fmt[0].sizeimage =
				pix_mp->width * pix_mp->height;

			pix_mp->plane_fmt[1].bytesperline = pix_mp->width;
			pix_mp->plane_fmt[1].sizeimage =
				pix_mp->width * pix_mp->height / 2;
		}

		for (i = 0; i < pix_mp->num_planes; i++) {
			memset(&(pix_mp->plane_fmt[i].reserved[0]), 0x0,
				   sizeof(pix_mp->plane_fmt[0].reserved));
		}
		memset(&pix_mp->reserved, 0x0, sizeof(pix_mp->reserved));

		pix_mp->flags = 0;
	} else {
		if (V4L2_TYPE_IS_OUTPUT(f->type)) {
			pix->bytesperline = 0;
			if ((pix->pixelformat != V4L2_PIX_FMT_MPEG2) &&
			    (pix->pixelformat != V4L2_PIX_FMT_H264) &&
			    (pix->pixelformat != V4L2_PIX_FMT_MPEG1)) {
				pix->field = V4L2_FIELD_NONE;
			} else if (pix->field != V4L2_FIELD_NONE) {
				if (pix->field == V4L2_FIELD_ANY)
					pix->field = V4L2_FIELD_NONE;

				pr_info("%s, field: %u, fmt: %x\n",
					__func__, pix->field,
					pix->pixelformat);
			}
		} else {
			if (pix->field != V4L2_FIELD_INTERLACED)
				pix->field = V4L2_FIELD_NONE;

			pix->height = clamp(pix->height,
						AML_VDEC_MIN_H,
						AML_VDEC_MAX_H);
			pix->width = clamp(pix->width,
						AML_VDEC_MIN_W,
						AML_VDEC_MAX_W);

			pix->bytesperline = pix->width;
			pix->sizeimage = pix->width * pix->height;
		}
		pix->flags = 0;
	}

	return 0;
}

static int vidioc_try_fmt_vid_cap_out(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct aml_q_data *q_data = NULL;
	struct aml_video_fmt *fmt = NULL;
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *dst_vq;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %u, planes: %u, fmt: %x\n",
		__func__, f->type,
		V4L2_TYPE_IS_MULTIPLANAR(f->type) ?
		f->fmt.pix_mp.num_planes : 1,
		f->fmt.pix_mp.pixelformat);

	dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (!dst_vq) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"no vb2 queue for type=%d\n", V4L2_BUF_TYPE_VIDEO_CAPTURE);
		return -EINVAL;
	}

	if (!V4L2_TYPE_IS_MULTIPLANAR(f->type) && dst_vq->is_multiplanar)
		return -EINVAL;

	fmt = aml_vdec_find_format(f);
	if (!fmt) {
		if (V4L2_TYPE_IS_OUTPUT(f->type))
			f->fmt.pix.pixelformat = aml_video_formats[OUT_FMT_IDX].fourcc;
		else
			f->fmt.pix.pixelformat = aml_video_formats[CAP_FMT_IDX].fourcc;
		fmt = aml_vdec_find_format(f);
	}

	vidioc_try_fmt(f, fmt);

	q_data = aml_vdec_get_q_data(ctx, f->type);
	if (!q_data)
		return -EINVAL;

	if (ctx->state >= AML_STATE_PROBE)
		update_ctx_dimension(ctx, f->type);
	copy_v4l2_format_dimention(pix_mp, pix, q_data, f->type);

	if (!V4L2_TYPE_IS_OUTPUT(f->type))
		return 0;

	if (V4L2_TYPE_IS_MULTIPLANAR(f->type)) {
		if (pix_mp->plane_fmt[0].sizeimage == 0) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"sizeimage of output format must be given\n");
			return -EINVAL;
		}
	} else {
		if (pix->sizeimage == 0) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"sizeimage of output format must be given\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int vidioc_vdec_g_selection(struct file *file, void *priv,
	struct v4l2_selection *s)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct aml_q_data *q_data;
	int ratio = 1;

	if ((s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) &&
		(s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE))
		return -EINVAL;

	if (ctx->internal_dw_scale) {
		if (ctx->state >= AML_STATE_PROBE) {
			unsigned int dw_mode = VDEC_DW_NO_AFBC;
			if (vdec_if_get_param(ctx, GET_PARAM_DW_MODE, &dw_mode))
				return -EBUSY;
			ratio = get_double_write_ratio(dw_mode);
		}
	}

	q_data = &ctx->q_data[AML_Q_DATA_DST];

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.visible_width / ratio;
		s->r.height = ctx->picinfo.visible_height / ratio;
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.coded_width / ratio;
		s->r.height = ctx->picinfo.coded_height / ratio;
		break;
	case V4L2_SEL_TGT_CROP_DEFAULT:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.visible_width;
		s->r.height = ctx->picinfo.visible_height;
		break;
	case V4L2_SEL_TGT_CROP_BOUNDS:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.coded_width;
		s->r.height = ctx->picinfo.coded_height;
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
	}

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s, type: %d\n",
		__func__, s->type);

	return 0;
}

static int vidioc_vdec_s_selection(struct file *file, void *priv,
	struct v4l2_selection *s)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ratio = 1;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s, type: %d\n",
		__func__, s->type);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if (ctx->internal_dw_scale) {
		if (ctx->state >= AML_STATE_PROBE) {
			unsigned int dw_mode = VDEC_DW_NO_AFBC;
			if (vdec_if_get_param(ctx, GET_PARAM_DW_MODE, &dw_mode))
				return -EBUSY;
			ratio = get_double_write_ratio(dw_mode);
		}
	}

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.visible_width / ratio;
		s->r.height = ctx->picinfo.visible_height / ratio;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* called when it is beyong AML_STATE_PROBE */
static void update_ctx_dimension(struct aml_vcodec_ctx *ctx, u32 type)
{
	struct aml_q_data *q_data;
	unsigned int dw_mode = VDEC_DW_NO_AFBC;
	int ratio = 1;

	q_data = aml_vdec_get_q_data(ctx, type);

	if (ctx->internal_dw_scale) {
		if (vdec_if_get_param(ctx, GET_PARAM_DW_MODE, &dw_mode))
			return;
		ratio = get_double_write_ratio(dw_mode);
	}

	if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
		q_data->sizeimage[0] = ctx->picinfo.y_len_sz;
		q_data->sizeimage[1] = ctx->picinfo.c_len_sz;

		q_data->coded_width = ALIGN(ctx->picinfo.coded_width / ratio, 64);
		q_data->coded_height = ALIGN(ctx->picinfo.coded_height / ratio, 64);

		q_data->bytesperline[0] = ALIGN(ctx->picinfo.coded_width / ratio, 64);
		q_data->bytesperline[1] = ALIGN(ctx->picinfo.coded_width / ratio, 64);
	} else {
		q_data->coded_width = ALIGN(ctx->picinfo.coded_width / ratio, 64);
		q_data->coded_height = ALIGN(ctx->picinfo.coded_height / ratio, 64);
		q_data->sizeimage[0] = ctx->picinfo.y_len_sz;
		q_data->sizeimage[0] += ctx->picinfo.c_len_sz;
		q_data->bytesperline[0] = ALIGN(ctx->picinfo.coded_width / ratio, 64);
	}
}

static void copy_v4l2_format_dimention(struct v4l2_pix_format_mplane *pix_mp,
				       struct v4l2_pix_format *pix,
				       struct aml_q_data *q_data,
				       u32 type)
{
	int i;

	if (!pix || !pix_mp || !q_data)
		return;

	if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
		pix_mp->width		= q_data->coded_width;
		pix_mp->height		= q_data->coded_height;
		pix_mp->num_planes	= q_data->fmt->num_planes;
		pix_mp->pixelformat	= q_data->fmt->fourcc;

		for (i = 0; i < q_data->fmt->num_planes; i++) {
			pix_mp->plane_fmt[i].bytesperline = q_data->bytesperline[i];
			pix_mp->plane_fmt[i].sizeimage = q_data->sizeimage[i];
		}
	} else {
		pix->width		= q_data->coded_width;
		pix->height		= q_data->coded_height;
		pix->pixelformat	= q_data->fmt->fourcc;
		pix->bytesperline	= q_data->bytesperline[0];
		pix->sizeimage		= q_data->sizeimage[0];
	}
}

static int vidioc_vdec_s_fmt(struct file *file, void *priv,
	struct v4l2_format *f)
{
	int ret = 0;
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct aml_q_data *q_data = NULL;
	struct aml_video_fmt *fmt;
	struct vb2_queue *dst_vq;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %u, planes: %u, fmt: %x\n",
		__func__, f->type,
		V4L2_TYPE_IS_MULTIPLANAR(f->type) ?
		f->fmt.pix_mp.num_planes : 1,
		f->fmt.pix_mp.pixelformat);

	dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (!dst_vq) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"no vb2 queue for type=%d\n", V4L2_BUF_TYPE_VIDEO_CAPTURE);
		return -EINVAL;
	}

	if (!V4L2_TYPE_IS_MULTIPLANAR(f->type) && dst_vq->is_multiplanar)
		return -EINVAL;

	q_data = aml_vdec_get_q_data(ctx, f->type);
	if (!q_data)
		return -EINVAL;

	if ((f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) &&
	    vb2_is_busy(&ctx->m2m_ctx->out_q_ctx.q)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"out_q_ctx buffers already requested\n");
		ret = -EBUSY;
	}

	if ((!V4L2_TYPE_IS_OUTPUT(f->type)) &&
	    vb2_is_busy(&ctx->m2m_ctx->cap_q_ctx.q)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"cap_q_ctx buffers already requested\n");
		ret = -EBUSY;
	}

	fmt = aml_vdec_find_format(f);
	if (fmt == NULL) {
		if (V4L2_TYPE_IS_OUTPUT(f->type))
			fmt = &aml_video_formats[OUT_FMT_IDX];
		else
			fmt = &aml_video_formats[CAP_FMT_IDX];
		f->fmt.pix.pixelformat = fmt->fourcc;
	}

	q_data->fmt = fmt;
	vidioc_try_fmt(f, q_data->fmt);

	if (V4L2_TYPE_IS_OUTPUT(f->type) && ctx->drv_handle && ctx->receive_cmd_stop) {
		ctx->state = AML_STATE_IDLE;
		ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
		v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
			"vcodec state (AML_STATE_IDLE)\n");
		vdec_if_deinit(ctx);
		ctx->receive_cmd_stop = 0;
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		q_data->sizeimage[0] = pix_mp->plane_fmt[0].sizeimage;
		q_data->coded_width = pix_mp->width;
		q_data->coded_height = pix_mp->height;

		v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
			"w: %d, h: %d, size: %d\n",
			pix_mp->width, pix_mp->height,
			pix_mp->plane_fmt[0].sizeimage);

		ctx->output_pix_fmt = pix_mp->pixelformat;
		ctx->colorspace = f->fmt.pix_mp.colorspace;
		ctx->ycbcr_enc = f->fmt.pix_mp.ycbcr_enc;
		ctx->quantization = f->fmt.pix_mp.quantization;
		ctx->xfer_func = f->fmt.pix_mp.xfer_func;

		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_IDLE) {
			ret = vdec_if_init(ctx, q_data->fmt->fourcc);
			if (ret) {
				v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
					"vdec_if_init() fail ret=%d\n", ret);
				mutex_unlock(&ctx->state_lock);
				return -EINVAL;
			}
			ctx->state = AML_STATE_INIT;
			ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
			v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
				"vcodec state (AML_STATE_INIT)\n");
		}
		mutex_unlock(&ctx->state_lock);
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		q_data->sizeimage[0] = pix->sizeimage;
		q_data->coded_width = pix->width;
		q_data->coded_height = pix->height;

		v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
			"w: %d, h: %d, size: %d\n",
			pix->width, pix->height,
			pix->sizeimage);

		ctx->output_pix_fmt = pix->pixelformat;
		ctx->colorspace = f->fmt.pix.colorspace;
		ctx->ycbcr_enc = f->fmt.pix.ycbcr_enc;
		ctx->quantization = f->fmt.pix.quantization;
		ctx->xfer_func = f->fmt.pix.xfer_func;

		mutex_lock(&ctx->state_lock);
		if (ctx->state == AML_STATE_IDLE) {
			ret = vdec_if_init(ctx, q_data->fmt->fourcc);
			if (ret) {
				v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
					"vdec_if_init() fail ret=%d\n", ret);
				mutex_unlock(&ctx->state_lock);
				return -EINVAL;
			}
			ctx->state = AML_STATE_INIT;
			ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
			v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
				"vcodec state (AML_STATE_INIT)\n");
		}
		mutex_unlock(&ctx->state_lock);
	}

	if (!V4L2_TYPE_IS_OUTPUT(f->type)) {
		ctx->cap_pix_fmt = V4L2_TYPE_IS_MULTIPLANAR(f->type) ?
			pix_mp->pixelformat : pix->pixelformat;
		if (ctx->state >= AML_STATE_PROBE) {
			update_ctx_dimension(ctx, f->type);
			copy_v4l2_format_dimention(pix_mp, pix, q_data, f->type);
		}
	}

	return 0;
}

static int vidioc_enum_framesizes(struct file *file, void *priv,
				struct v4l2_frmsizeenum *fsize)
{
	int i = 0;
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s, idx: %d, pix fmt: %x\n",
		__func__, fsize->index, fsize->pixel_format);

	if (fsize->index != 0)
		return -EINVAL;

	for (i = 0; i < NUM_SUPPORTED_FRAMESIZE; ++i) {
		if (fsize->pixel_format != aml_vdec_framesizes[i].fourcc)
			continue;

		fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
		fsize->stepwise = aml_vdec_framesizes[i].stepwise;
		if (!(ctx->dev->dec_capability &
				VCODEC_CAPABILITY_4K_DISABLED)) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO, "4K is enabled\n");
			fsize->stepwise.max_width =
					VCODEC_DEC_4K_CODED_WIDTH;
			fsize->stepwise.max_height =
					VCODEC_DEC_4K_CODED_HEIGHT;
		}
		v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
			"%x, %d %d %d %d %d %d\n",
			ctx->dev->dec_capability,
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
	int i = 0, j = 0;

	/* I420 only used for mjpeg. */
	if (!output_queue && support_mjpeg && support_format_I420) {
		for (i = 0; i < NUM_FORMATS; i++) {
			fmt = &aml_video_formats[i];
			if ((fmt->fourcc == V4L2_PIX_FMT_YUV420) ||
				(fmt->fourcc == V4L2_PIX_FMT_YUV420M)) {
				break;
			}
		}
	}

	for (; i < NUM_FORMATS; i++) {
		fmt = &aml_video_formats[i];
		if (output_queue && (fmt->type != AML_FMT_DEC))
			continue;
		if (!output_queue && (fmt->type != AML_FMT_FRAME))
			continue;
		if (support_mjpeg && !support_format_I420 &&
			((fmt->fourcc == V4L2_PIX_FMT_YUV420) ||
			(fmt->fourcc == V4L2_PIX_FMT_YUV420M)))
			continue;

		if (j == f->index) {
			f->pixelformat = fmt->fourcc;
			return 0;
		}
		++j;
	}

	return -EINVAL;
}

static int vidioc_vdec_enum_fmt_vid_cap_mplane(struct file *file,
	void *priv, struct v4l2_fmtdesc *f)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s\n", __func__);

	return vidioc_enum_fmt(f, false);
}

static int vidioc_vdec_enum_fmt_vid_out_mplane(struct file *file,
	void *priv, struct v4l2_fmtdesc *f)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s\n", __func__);

	return vidioc_enum_fmt(f, true);
}

static int vidioc_vdec_g_fmt(struct file *file, void *priv,
	struct v4l2_format *f)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct vb2_queue *vq;
	struct vb2_queue *dst_vq;
	struct aml_q_data *q_data;
	int ret = 0;

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"no vb2 queue for type=%d\n", f->type);
		return -EINVAL;
	}

	dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (!dst_vq) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"no vb2 queue for type=%d\n", V4L2_BUF_TYPE_VIDEO_CAPTURE);
		return -EINVAL;
	}

	if (!V4L2_TYPE_IS_MULTIPLANAR(f->type) && dst_vq->is_multiplanar)
		return -EINVAL;

	q_data = aml_vdec_get_q_data(ctx, f->type);

	ret = vdec_if_get_param(ctx, GET_PARAM_PIC_INFO, &ctx->picinfo);
	if (ret) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"GET_PARAM_PICTURE_INFO err\n");
	} else {
		if ((ctx->picinfo.visible_height < 16 && ctx->picinfo.visible_height > 0) ||
			(ctx->picinfo.visible_width < 16 && ctx->picinfo.visible_width > 0)) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
				"The width or height of the stream is less than 16\n");
			return -EPERM;
		}
	}

	if (V4L2_TYPE_IS_MULTIPLANAR(f->type)) {
		pix_mp->field = ret ? V4L2_FIELD_NONE : ctx->picinfo.field;
		pix_mp->colorspace = ctx->colorspace;
		pix_mp->ycbcr_enc = ctx->ycbcr_enc;
		pix_mp->quantization = ctx->quantization;
		pix_mp->xfer_func = ctx->xfer_func;
	} else {
		pix->field = ret ? V4L2_FIELD_NONE : ctx->picinfo.field;
		pix->colorspace = ctx->colorspace;
		pix->ycbcr_enc = ctx->ycbcr_enc;
		pix->quantization = ctx->quantization;
		pix->xfer_func = ctx->xfer_func;
	}

	if ((!V4L2_TYPE_IS_OUTPUT(f->type)) &&
	    (ctx->state >= AML_STATE_PROBE)) {
		update_ctx_dimension(ctx, f->type);
		copy_v4l2_format_dimention(pix_mp, pix, q_data, f->type);
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		/*
		 * This is run on OUTPUT
		 * The buffer contains compressed image
		 * so width and height have no meaning.
		 * Assign value here to pass v4l2-compliance test
		 */
		copy_v4l2_format_dimention(pix_mp, pix, q_data, f->type);
	} else {
		copy_v4l2_format_dimention(pix_mp, pix, q_data, f->type);

		v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
			"type=%d state=%d Format information could not be read, not ready yet!\n",
			f->type, ctx->state);
	}

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %u, planes: %u, fmt: %x\n",
		__func__, f->type,
		V4L2_TYPE_IS_MULTIPLANAR(f->type) ?
		q_data->fmt->num_planes : 1,
		q_data->fmt->fourcc);

	return 0;
}

static int vidioc_vdec_create_bufs(struct file *file, void *priv,
	struct v4l2_create_buffers *create)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(priv);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %u, count: %u\n",
		__func__, create->format.type, create->count);

	return v4l2_m2m_ioctl_create_bufs(file, priv, create);
}

/*int vidioc_vdec_g_ctrl(struct file *file, void *fh,
	struct v4l2_control *a)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, id: %d\n", __func__, a->id);

	if (a->id == V4L2_CID_MIN_BUFFERS_FOR_CAPTURE)
		a->value = 4;
	else if (a->id == V4L2_CID_MIN_BUFFERS_FOR_OUTPUT)
		a->value = 8;

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

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s, type: %d\n",
		__func__, vq->type);

	q_data = aml_vdec_get_q_data(ctx, vq->type);
	if (q_data == NULL) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"vq->type=%d err\n", vq->type);
		return -EINVAL;
	}

	if (*nplanes) {
		for (i = 0; i < *nplanes; i++) {
			if (sizes[i] < q_data->sizeimage[i])
				return -EINVAL;
			alloc_devs[i] = &ctx->dev->plat_dev->dev;

			if (!V4L2_TYPE_IS_OUTPUT(vq->type))
				alloc_devs[i] = v4l_get_dev_from_codec_mm();
		}
	} else {
		int dw_mode = VDEC_DW_NO_AFBC;

		if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			*nplanes = 2;
		else
			*nplanes = 1;

		if (!vdec_if_get_param(ctx, GET_PARAM_DW_MODE, &dw_mode)) {
			if (dw_mode == VDEC_DW_AFBC_ONLY)
				*nplanes = 1;
		}

		for (i = 0; i < *nplanes; i++) {
			sizes[i] = q_data->sizeimage[i];
			if (V4L2_TYPE_IS_OUTPUT(vq->type) && ctx->output_dma_mode)
				sizes[i] = 1;
			alloc_devs[i] = &ctx->dev->plat_dev->dev;

			if (!V4L2_TYPE_IS_OUTPUT(vq->type))
				alloc_devs[i] = v4l_get_dev_from_codec_mm();
		}
	}

	v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
		"type: %d, plane: %d, buf cnt: %d, size: [Y: %u, C: %u]\n",
		vq->type, *nplanes, *nbuffers, sizes[0], sizes[1]);

	return 0;
}

static int vb2ops_vdec_buf_prepare(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct aml_q_data *q_data;
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct aml_video_dec_buf *buf = NULL;
	int i;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d, idx: %d\n",
		__func__, vb->vb2_queue->type, vb->index);

	if (vb->memory == VB2_MEMORY_DMABUF
		&& V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type))
		return 0;

	q_data = aml_vdec_get_q_data(ctx, vb->vb2_queue->type);

	for (i = 0; i < q_data->fmt->num_planes; i++) {
		if (vb2_plane_size(vb, i) < q_data->sizeimage[i]) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
				"data will not fit into plane %d (%lu < %d)\n",
				i, vb2_plane_size(vb, i),
				q_data->sizeimage[i]);
		}
	}

	vb2_v4l2 = to_vb2_v4l2_buffer(vb);
	buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);

	if (vb2_v4l2->meta_ptr && (copy_from_user(buf->meta_data,
		(void *)vb2_v4l2->meta_ptr, META_DATA_SIZE + 4))) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"%s:copy meta data error. ptr: %lx\n", __func__, vb2_v4l2->meta_ptr);
	}

	return 0;
}

static int init_mmu_bmmu_box(struct aml_vcodec_ctx *ctx)
{
	int i;
	int mmu_flag = ctx->is_drm_mode? CODEC_MM_FLAGS_TVP:0;
	int bmmu_flag = mmu_flag;
	u32 dw_mode = VDEC_DW_NO_AFBC;

	ctx->comp_bufs = vzalloc(sizeof(*ctx->comp_bufs) * V4L_CAP_BUFF_MAX);
	if (!ctx->comp_bufs)
		return -ENOMEM;

	if (vdec_if_get_param(ctx, GET_PARAM_DW_MODE, &dw_mode)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "invalid dw_mode\n");
		goto free_comp_bufs;
	}

	/* init bmmu box */
	ctx->mmu_box = decoder_mmu_box_alloc_box("v4l2_dec",
			ctx->id, V4L_CAP_BUFF_MAX,
			ctx->comp_info.max_size * SZ_1M, mmu_flag);
	if (!ctx->mmu_box) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "fail to create mmu box\n");
		goto free_comp_bufs;
	}

	/* init mmu box */
	bmmu_flag |= CODEC_MM_FLAGS_CMA_CLEAR | CODEC_MM_FLAGS_FOR_VDECODER;
	ctx->bmmu_box  = decoder_bmmu_box_alloc_box("v4l2_dec",
			ctx->id, V4L_CAP_BUFF_MAX,
			4 + PAGE_SHIFT, bmmu_flag,
			BMMU_ALLOC_FLAGS_WAIT);
	if (!ctx->bmmu_box) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "fail to create bmmu box\n");
		goto free_mmubox;
	}

	if (dw_mode & 0x20) {
		/* init mmu box dw*/
		ctx->mmu_box_dw = decoder_mmu_box_alloc_box("v4l2_dec_dw",
				ctx->id, V4L_CAP_BUFF_MAX,
				ctx->comp_info.max_size * SZ_1M, mmu_flag);
		if (!ctx->mmu_box_dw) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "fail to create mmu box dw\n");
			goto free_bmmubox;
		}

		/* init bmmu box dw*/
		bmmu_flag |= CODEC_MM_FLAGS_CMA_CLEAR | CODEC_MM_FLAGS_FOR_VDECODER;
		ctx->bmmu_box_dw  = decoder_bmmu_box_alloc_box("v4l2_dec_dw",
				ctx->id, V4L_CAP_BUFF_MAX,
				4 + PAGE_SHIFT, bmmu_flag,
				BMMU_ALLOC_FLAGS_WAIT);
		if (!ctx->bmmu_box_dw) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "fail to create nmmu box dw\n");
			goto free_mmubox_dw;
		}
	}

	kref_init(&ctx->box_ref);
	for (i = 0; i < V4L_CAP_BUFF_MAX; i++) {
		struct internal_comp_buf *buf;
		buf = &ctx->comp_bufs[i];
		buf->index = i;
		buf->ref = 0;
		buf->box_ref = &ctx->box_ref;
		buf->mmu_box = ctx->mmu_box;
		buf->bmmu_box = ctx->bmmu_box;
		buf->mmu_box_dw = ctx->mmu_box_dw;
		buf->bmmu_box_dw = ctx->bmmu_box_dw;
		buf->used = 0;
	}
	kref_get(&ctx->ctx_ref);

	ctx->uvm_proxy = vzalloc(sizeof(*ctx->uvm_proxy) * V4L_CAP_BUFF_MAX);
	if (!ctx->uvm_proxy)
		goto free_mmubox;
	memset(ctx->uvm_proxy, 0, sizeof(*ctx->uvm_proxy) * V4L_CAP_BUFF_MAX);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
		"box init, bmmu: %px, mmu: %px, mmu_dw: %px bmmu_dw: %px\n",
		ctx->bmmu_box, ctx->mmu_box, ctx->mmu_box_dw, ctx->bmmu_box_dw);

	return 0;
free_mmubox_dw:
	decoder_mmu_box_free(ctx->mmu_box_dw);
	ctx->mmu_box_dw = NULL;

free_bmmubox:
	decoder_bmmu_box_free(ctx->bmmu_box);
	ctx->bmmu_box = NULL;

free_mmubox:
	decoder_mmu_box_free(ctx->mmu_box);
	ctx->mmu_box = NULL;

free_comp_bufs:
	vfree(ctx->comp_bufs);
	ctx->comp_bufs = NULL;

	return -1;
}

void aml_alloc_buffer(struct aml_vcodec_ctx *ctx, int flag)
{
	int i = 0;

	if (flag & DV_TYPE) {
		for (i = 0; i < V4L_CAP_BUFF_MAX; i++) {
			ctx->aux_infos.bufs[i].md_buf = vzalloc(MD_BUF_SIZE);
			if (ctx->aux_infos.bufs[i].md_buf == NULL) {
				v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
					"v4l2 alloc %dth dv md buffer fail\n", i);
			}

			ctx->aux_infos.bufs[i].comp_buf = vzalloc(COMP_BUF_SIZE);
			if (ctx->aux_infos.bufs[i].comp_buf == NULL) {
				v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
					"v4l2 alloc %dth dv comp buffer fail\n", i);
			}
		}
	}

	if (flag & SEI_TYPE) {
		for (i = 0; i < V4L_CAP_BUFF_MAX; i++) {
			ctx->aux_infos.bufs[i].sei_buf = vzalloc(SEI_BUF_SIZE);
			if (ctx->aux_infos.bufs[i].sei_buf) {
				ctx->aux_infos.bufs[i].sei_size  = 0;
				ctx->aux_infos.bufs[i].sei_state = 1;
				ctx->aux_infos.sei_need_free = false;
				v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
					"v4l2 alloc %dth aux buffer:%px\n",
					i, ctx->aux_infos.bufs[i].sei_buf);
			} else {
				ctx->aux_infos.bufs[i].sei_buf = NULL;
				ctx->aux_infos.bufs[i].sei_state = 0;
				ctx->aux_infos.bufs[i].sei_size  = 0;
				v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
					"v4l2 alloc %dth aux buffer fail\n", i);
			}
		}
	}
}

void aml_free_buffer(struct aml_vcodec_ctx *ctx, int flag)
{
	int i = 0;

	if (flag & DV_TYPE) {
		for (i = 0; i < V4L_CAP_BUFF_MAX; i++) {
			if (ctx->aux_infos.bufs[i].md_buf != NULL) {
				vfree(ctx->aux_infos.bufs[i].md_buf);
				ctx->aux_infos.bufs[i].md_buf = NULL;
			}

			if (ctx->aux_infos.bufs[i].comp_buf != NULL) {
				vfree(ctx->aux_infos.bufs[i].comp_buf);
				ctx->aux_infos.bufs[i].comp_buf = NULL;
			}
		}
	}

	if (flag & SEI_TYPE) {
		for (i = 0; i < V4L_CAP_BUFF_MAX; i++) {
			if (ctx->aux_infos.bufs[i].sei_buf != NULL) {
				v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
					"v4l2 free %dth aux buffer:%px\n",
					i, ctx->aux_infos.bufs[i].sei_buf);
				vfree(ctx->aux_infos.bufs[i].sei_buf);
				ctx->aux_infos.bufs[i].sei_state = 0;
				ctx->aux_infos.bufs[i].sei_size = 0;
				ctx->aux_infos.bufs[i].sei_buf = NULL;
			}
		}
	}
}

void aml_free_one_sei_buffer(struct aml_vcodec_ctx *ctx, char **addr, int *size, int idx)
{
	if (ctx->aux_infos.bufs[idx].sei_buf != NULL) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
			"v4l2 free %dth aux buffer:%px\n",
			idx, ctx->aux_infos.bufs[idx].sei_buf);

		vfree(ctx->aux_infos.bufs[idx].sei_buf);
		ctx->aux_infos.bufs[idx].sei_state = 0;
		ctx->aux_infos.bufs[idx].sei_size = 0;
		ctx->aux_infos.bufs[idx].sei_buf = NULL;
		*addr = NULL;
		*size = 0;
		ctx->aux_infos.sei_need_free = true;
	}
}

void aml_bind_sei_buffer(struct aml_vcodec_ctx *ctx, char **addr, int *size, int *idx)
{
	int index = ctx->aux_infos.sei_index;
	int count = 0;

	if (ctx->aux_infos.sei_need_free) {
		for (count = 0; count < V4L_CAP_BUFF_MAX; count++) {
			if ((ctx->aux_infos.bufs[index].sei_buf != NULL) &&
				(ctx->aux_infos.bufs[index].sei_state == 1)) {
				break;
			}
			index = (index + 1) % V4L_CAP_BUFF_MAX;
		}
	} else {
		for (count = 0; count < V4L_CAP_BUFF_MAX; count++) {
			if ((ctx->aux_infos.bufs[index].sei_buf != NULL) &&
				((ctx->aux_infos.bufs[index].sei_state == 1) ||
				(ctx->aux_infos.bufs[index].sei_state == 2))) {
				memset(ctx->aux_infos.bufs[index].sei_buf, 0, SEI_BUF_SIZE);
				ctx->aux_infos.bufs[index].sei_size = 0;
				break;
			}
			index = (index + 1) % V4L_CAP_BUFF_MAX;
		}
	}

	if (count == V4L_CAP_BUFF_MAX) {
		*addr = NULL;
		*size = 0;
	} else {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
			"v4l2 bind %dth aux buffer:%px, count = %d\n",
			index, ctx->aux_infos.bufs[index].sei_buf, count);
		*addr = ctx->aux_infos.bufs[index].sei_buf;
		*size = ctx->aux_infos.bufs[index].sei_size;
		*idx  = index;
		ctx->aux_infos.bufs[index].sei_state = 2;
		ctx->aux_infos.sei_index = (index + 1) % V4L_CAP_BUFF_MAX;
	}
}

void aml_bind_dv_buffer(struct aml_vcodec_ctx *ctx, char **comp_buf, char **md_buf)
{
	int index = ctx->aux_infos.dv_index;

	if ((ctx->aux_infos.bufs[index].comp_buf != NULL) &&
		(ctx->aux_infos.bufs[index].md_buf != NULL)) {
		*comp_buf = ctx->aux_infos.bufs[index].comp_buf;
		*md_buf = ctx->aux_infos.bufs[index].md_buf;
		ctx->aux_infos.dv_index = (index + 1) % V4L_CAP_BUFF_MAX;
	}
}

static void aml_canvas_cache_free(struct canvas_cache *canche)
{
	int i = -1;

	for (i = 0; i < ARRAY_SIZE(canche->res); i++) {
		if (canche->res[i].cid > 0) {
			v4l_dbg(0, V4L_DEBUG_CODEC_BUFMGR,
				"canvas-free, name:%s, canvas id:%d\n",
				canche->res[i].name,
				canche->res[i].cid);

			canvas_pool_map_free_canvas(canche->res[i].cid);

			canche->res[i].cid = 0;
		}
	}
}

void aml_canvas_cache_put(struct aml_vcodec_dev *dev)
{
	struct canvas_cache *canche = &dev->canche;

	mutex_lock(&canche->lock);

	v4l_dbg(0, V4L_DEBUG_CODEC_BUFMGR,
		"canvas-put, ref:%d\n", canche->ref);

	canche->ref--;

	if (canche->ref == 0) {
		aml_canvas_cache_free(canche);
	}

	mutex_unlock(&canche->lock);
}

int aml_canvas_cache_get(struct aml_vcodec_dev *dev, char *usr)
{
	struct canvas_cache *canche = &dev->canche;
	int i;

	mutex_lock(&canche->lock);

	canche->ref++;

	for (i = 0; i < ARRAY_SIZE(canche->res); i++) {
		if (canche->res[i].cid <= 0) {
			snprintf(canche->res[i].name, 32, "%s-%d", usr, i);
			canche->res[i].cid =
				canvas_pool_map_alloc_canvas(canche->res[i].name);
		}

		v4l_dbg(0, V4L_DEBUG_CODEC_BUFMGR,
			"canvas-alloc, name:%s, canvas id:%d\n",
			canche->res[i].name,
			canche->res[i].cid);

		if (canche->res[i].cid <= 0) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"canvas-fail, name:%s, canvas id:%d.\n",
				canche->res[i].name,
				canche->res[i].cid);

			mutex_unlock(&canche->lock);
			goto err;
		}
	}

	v4l_dbg(0, V4L_DEBUG_CODEC_BUFMGR,
		"canvas-get, ref:%d\n", canche->ref);

	mutex_unlock(&canche->lock);
	return 0;
err:
	aml_canvas_cache_put(dev);
	return -1;
}

int aml_canvas_cache_init(struct aml_vcodec_dev *dev)
{
	dev->canche.ref = 0;
	mutex_init(&dev->canche.lock);

	v4l_dbg(0, V4L_DEBUG_CODEC_BUFMGR,
		"canvas-init, ref:%d\n", dev->canche.ref);

	return 0;
}

void aml_v4l_ctx_release(struct kref *kref)
{
	struct aml_vcodec_ctx * ctx;

	ctx = container_of(kref, struct aml_vcodec_ctx, ctx_ref);

	if (ctx->vpp) {
		aml_v4l2_vpp_destroy(ctx->vpp);
		atomic_dec(&ctx->dev->vpp_count);

		v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
			"vpp destory inst count:%d.\n",
			atomic_read(&ctx->dev->vpp_count));
	}

	if (ctx->ge2d) {
		aml_v4l2_ge2d_destroy(ctx->ge2d);

		v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
			"ge2d destory.\n");
	}

	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	aml_task_chain_remove(ctx);

	vfree(ctx->meta_infos.meta_bufs);
	ctx->aux_infos.free_buffer(ctx, SEI_TYPE | DV_TYPE);
	ctx->aux_infos.free_buffer(ctx, 1);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
		"v4ldec has been destroyed.\n");

	if (ctx->sync) {
		vdec_clean_all_fence(ctx->sync);
	}

	kfree(ctx);
}

static void box_release(struct kref *kref)
{
	struct aml_vcodec_ctx * ctx
		= container_of(kref, struct aml_vcodec_ctx, box_ref);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
		"%s, bmmu: %px, mmu: %px mmu_dw: %pu\n",
		__func__, ctx->bmmu_box, ctx->mmu_box,ctx->mmu_box_dw);

	decoder_bmmu_box_free(ctx->bmmu_box);
	decoder_mmu_box_free(ctx->mmu_box);

	if (ctx->config.parm.dec.cfg.double_write_mode & 0x20) {
		decoder_mmu_box_free(ctx->mmu_box_dw);
		decoder_bmmu_box_free(ctx->bmmu_box_dw);
	}
	vfree(ctx->comp_bufs);
	vfree(ctx->uvm_proxy);
	kref_put(&ctx->ctx_ref, aml_v4l_ctx_release);
}

static void internal_buf_free(void *arg)
{
	struct internal_comp_buf* ibuf =
		(struct internal_comp_buf*)arg;
	struct aml_vcodec_ctx * ctx
		= container_of(ibuf->box_ref,struct aml_vcodec_ctx, box_ref);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
		"%s, idx:%d\n", __func__, ibuf->index);

	mutex_lock(&ctx->comp_lock);

	if (!(ibuf->ref & 0xff00)) {
		decoder_mmu_box_free_idx(ibuf->mmu_box, ibuf->index);
		decoder_bmmu_box_free_idx(ibuf->bmmu_box, ibuf->index);

		if (ctx->config.parm.dec.cfg.double_write_mode & 0x20) {
			decoder_mmu_box_free_idx(ibuf->mmu_box_dw, ibuf->index);
			decoder_bmmu_box_free_idx(ibuf->bmmu_box_dw, ibuf->index);
		}
	}
	ibuf->ref = 0;

	mutex_unlock(&ctx->comp_lock);

	kref_put(ibuf->box_ref, box_release);
}

static void internal_buf_free2(void *arg)
{
	struct internal_comp_buf *ibuf =
		container_of(arg, struct internal_comp_buf, priv_data);
	struct aml_vcodec_ctx * ctx
		= container_of(ibuf->box_ref, struct aml_vcodec_ctx, box_ref);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
		"%s, idx: %d\n", __func__, ibuf->index);

	mutex_lock(&ctx->comp_lock);

	if (!(ibuf->ref & 0xff00)) {
		decoder_mmu_box_free_idx(ibuf->mmu_box, ibuf->index);
		decoder_bmmu_box_free_idx(ibuf->bmmu_box, ibuf->index);

		if (ctx->config.parm.dec.cfg.double_write_mode & 0x20) {
			decoder_mmu_box_free_idx(ibuf->mmu_box_dw, ibuf->index);
			decoder_bmmu_box_free_idx(ibuf->bmmu_box_dw, ibuf->index);
		}
	}
	ibuf->ref = 0;

	mutex_unlock(&ctx->comp_lock);

	kref_put(ibuf->box_ref, box_release);
}

static void aml_uvm_buf_free(void *arg)
{
	struct aml_uvm_buff_ref * ubuf =
		(struct aml_uvm_buff_ref*)arg;
	struct aml_vcodec_ctx * ctx
		= container_of(ubuf->ref, struct aml_vcodec_ctx, ctx_ref);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
		"%s, vb:%d, dbuf:%px, ino:%lu\n",
		__func__, ubuf->index, ubuf->dbuf,
		file_inode(ubuf->dbuf->file)->i_ino);

	kref_put(ubuf->ref, aml_v4l_ctx_release);
	vfree(ubuf);
}

static int uvm_attach_hook_mod_local(struct aml_vcodec_ctx *ctx,
				     struct uvm_hook_mod_info *uvm)
{
	struct internal_comp_buf* ibuf = uvm->arg;

	ctx->uvm_proxy[ibuf->index] = *uvm;

	return 0;
}

static int update_comp_buffer_to_reuse(struct aml_vcodec_ctx *ctx,
				       struct aml_video_dec_buf *buf)
{
	struct internal_comp_buf* ibuf = NULL;
	u32 dw_mode = VDEC_DW_NO_AFBC;

	if (vdec_if_get_param(ctx, GET_PARAM_DW_MODE, &dw_mode)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "invalid dw_mode\n");
		return -EINVAL;
	}

	mutex_lock(&ctx->comp_lock);

	ibuf = vb_to_comp(ctx, &buf->vb.vb2_buf);
	if (!ibuf) {
		mutex_unlock(&ctx->comp_lock);
		return 0;
	}

	if (ibuf->ref & 0xff) {
		buf->internal_index = ibuf->index;
		ibuf->frame_buffer_size = ctx->comp_info.frame_buffer_size;

		if (ctx->comp_info.header_size != ibuf->header_size) {
			decoder_bmmu_box_free_idx(ctx->bmmu_box, ibuf->index);
			if (decoder_bmmu_box_alloc_buf_phy(ctx->bmmu_box,
				ibuf->index, ctx->comp_info.header_size,
				"v4l2_dec", &ibuf->header_addr) < 0) {
				v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
					"fail to alloc %dth bmmu\n",
					ibuf->index);
				mutex_unlock(&ctx->comp_lock);
				return -ENOMEM;
			}

			if (dw_mode & 0x20) {
				decoder_bmmu_box_free_idx(ctx->bmmu_box_dw, ibuf->index);
				if (decoder_bmmu_box_alloc_buf_phy(ctx->bmmu_box_dw,
					ibuf->index, ctx->comp_info.header_size,
					"v4l2_dec_dw", &ibuf->header_dw_addr) < 0) {
					v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
						"fail to alloc %dth bmmu_dw\n",
						ibuf->index);
					mutex_unlock(&ctx->comp_lock);
					return -ENOMEM;
				}
			}
			ibuf->header_size = ctx->comp_info.header_size;
		}

		ibuf->ref |= (1 << 8);

		v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
			"%s, reuse comp buffer vb2:%d <--> internal: %d, header_addr 0x%lx, size: %u\n",
			__func__, buf->vb.vb2_buf.index,
			buf->internal_index,
			ibuf->header_addr,
			ibuf->header_size);
	}

	mutex_unlock(&ctx->comp_lock);

	return (ibuf->ref & 0xff00) ? 1 : 0;
}

static int bind_comp_buffer_to_uvm(struct aml_vcodec_ctx *ctx,
		struct aml_video_dec_buf *buf)
{
	int ret, i;
	struct dma_buf * dma = buf->vb.vb2_buf.planes[0].dbuf;
	struct aml_dec_params *parms = &ctx->config.parm.dec;
	struct uvm_hook_mod_info u_info;
	struct internal_comp_buf* ibuf;
	u32 dw_mode = VDEC_DW_NO_AFBC;

	/* get header and page size */
	if (vdec_if_get_param(ctx, GET_PARAM_COMP_BUF_INFO, &ctx->comp_info)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "fail to get comp info\n");
		return -EINVAL;
	}

	if (!ctx->bmmu_box || !ctx->mmu_box)
		if (init_mmu_bmmu_box(ctx))
			return -EINVAL;

	ret = update_comp_buffer_to_reuse(ctx, buf);
	if (ret < 0)
		return ret;

	if (ret == 1 /*reused*/)
		return 0;

	for (i = 0; i < V4L_CAP_BUFF_MAX; i++) {
		if (!ctx->comp_bufs[i].ref)
			break;
	}

	if (i == V4L_CAP_BUFF_MAX) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "out of internal buf\n");
		return -EINVAL;
	}

	if (vdec_if_get_param(ctx, GET_PARAM_DW_MODE, &dw_mode)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "invalid dw_mode\n");
		return -EINVAL;
	}

	buf->internal_index	= i;
	ibuf			= &ctx->comp_bufs[i];
	ibuf->frame_buffer_size	= ctx->comp_info.frame_buffer_size;
	ibuf->header_size	= ctx->comp_info.header_size;

	/* allocate header */
	ret = decoder_bmmu_box_alloc_buf_phy(ctx->bmmu_box,
			ibuf->index, ctx->comp_info.header_size,
			"v4l2_dec", &ibuf->header_addr);
	if (ret < 0) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "fail to alloc %dth bmmu\n", i);
		return -ENOMEM;
	}

	if (dw_mode & 0x20) {
		ret = decoder_bmmu_box_alloc_buf_phy(ctx->bmmu_box_dw,
			ibuf->index, ctx->comp_info.header_size,
			"v4l2_dec_dw", &ibuf->header_dw_addr);
		if (ret < 0) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "fail to alloc %dth bmmu dw\n", i);
			return -ENOMEM;
		}
	}

	kref_get(&ctx->box_ref);
	ibuf->ref = 1;

	/* frame SG buffer need to be realloc inside decoder,
	 * just before slice decoding to save memory
	 */
	u_info.type = VF_SRC_DECODER;
	u_info.arg = ibuf;
	u_info.free = internal_buf_free;

	if (parms->cfg.uvm_hook_type == VF_PROCESS_V4LVIDEO) {
		/* adapted video composer to use for hwc. */
		ibuf->priv_data.v4l_inst_id = ctx->id;
		u_info.type = VF_PROCESS_V4LVIDEO;
		u_info.arg = &ibuf->priv_data;
		u_info.free = internal_buf_free2;
	}

	ret = ((dma != NULL) && dmabuf_is_uvm(dma)) ?
		uvm_attach_hook_mod(dma, &u_info) :
		uvm_attach_hook_mod_local(ctx, &u_info);
	if (ret < 0) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "fail to set dmabuf priv buf\n");
		goto bmmu_box_free;
	}

	v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
		"%s, bind vb2:(%d, %px) <--> internal: (%d, %px) header_addr 0x%lx, size: %u\n",
		__func__, buf->vb.vb2_buf.index,
		dma, i, ibuf, ibuf->header_addr,
		ctx->comp_info.header_size);

	return 0;

bmmu_box_free:
	if (dw_mode & 0x20) {
		decoder_bmmu_box_free_idx(ibuf->bmmu_box_dw, ibuf->index);
	}
	decoder_bmmu_box_free_idx(ibuf->bmmu_box, ibuf->index);
	kref_put(&ctx->box_ref, box_release);
	ibuf->ref = 0;
	return -EINVAL;
}

static int aml_uvm_buff_attach(struct vb2_buffer * vb)
{
	int ret = 0;
	struct dma_buf *dbuf = vb->planes[0].dbuf;
	struct uvm_hook_mod_info u_info;
	struct aml_vcodec_ctx *ctx =
		vb2_get_drv_priv(vb->vb2_queue);
	struct aml_uvm_buff_ref *ubuf = NULL;

	if (vb->memory != VB2_MEMORY_DMABUF || !dmabuf_is_uvm(dbuf))
		return 0;

	ubuf = vzalloc(sizeof(struct aml_uvm_buff_ref));
	if (ubuf == NULL)
		return -ENOMEM;

	ubuf->index	= vb->index;
	ubuf->addr	= vb2_dma_contig_plane_dma_addr(vb, 0);
	ubuf->dbuf	= dbuf;
	ubuf->ref	= &ctx->ctx_ref;

	u_info.type	= VF_PROCESS_DECODER;
	u_info.arg	= (void *)ubuf;
	u_info.free	= aml_uvm_buf_free;
	ret = uvm_attach_hook_mod(dbuf, &u_info);
	if (ret < 0) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"aml uvm buffer %d attach fail.\n",
			ubuf->index);
		return ret;
	}

	kref_get(ubuf->ref);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
		"%s, vb:%d, dbuf:%px, ino:%lu\n",
		__func__, ubuf->index, ubuf->dbuf,
		file_inode(ubuf->dbuf->file)->i_ino);

	return ret;
}

static struct internal_comp_buf* vb_to_comp(struct aml_vcodec_ctx *ctx,
					    struct vb2_buffer *vb)
{
	struct aml_dec_params *parms = &ctx->config.parm.dec;
	bool is_v4lvideo = (parms->cfg.uvm_hook_type == VF_PROCESS_V4LVIDEO);
	enum uvm_hook_mod_type u_type =
		is_v4lvideo ? VF_PROCESS_V4LVIDEO : VF_SRC_DECODER;
	struct dma_buf *dbuf = vb->planes[0].dbuf;
	struct internal_comp_buf *ibuf = NULL;
	struct uvm_hook_mod *uhmod = NULL;

	uhmod = uvm_get_hook_mod(dbuf, u_type);
	if (IS_ERR_OR_NULL(uhmod))
		return NULL;

	ibuf = !is_v4lvideo ? (struct internal_comp_buf *) uhmod->arg :
		container_of(uhmod->arg, struct internal_comp_buf, priv_data);

	uvm_put_hook_mod(dbuf, u_type);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
		"%s, vb2: (%d, %px) --> comp: (%d, %px)\n",
		__func__, vb->index, dbuf, ibuf->index, ibuf);

	return ibuf;
}

static void vb2ops_vdec_buf_queue(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = to_vb2_v4l2_buffer(vb);
	struct aml_video_dec_buf *buf =
		container_of(vb2_v4l2, struct aml_video_dec_buf, vb);
	struct vdec_v4l2_buffer *fb = &buf->frame_buffer;
	struct aml_vcodec_mem src_mem;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, vb: %lx, type: %d, idx: %d, state: %d, used: %d, ts: %llu\n",
		__func__, (ulong) vb, vb->vb2_queue->type,
		vb->index, vb->state, buf->used, vb->timestamp);

	/*
	 * check if this buffer is ready to be used after decode
	 */
	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		u32 dw_mode = VDEC_DW_NO_AFBC;

		if (vdec_if_get_param(ctx, GET_PARAM_DW_MODE, &dw_mode)) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "invalid dw_mode\n");
			return;

		}

		if (!buf->que_in_m2m) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
				"enque capture buf idx %d, vf: %lx\n",
				vb->index, (ulong) v4l_get_vf_handle(vb2_v4l2->private));

			/* bind compressed buffer to uvm */
			if ((dw_mode != VDEC_DW_NO_AFBC) &&
				bind_comp_buffer_to_uvm(ctx, buf)) {
				v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "fail to bind comp buffer\n");
				return;
			}

			/* DI hook must be detached if the dmabuff be reused. */
			if (ctx->vpp_cfg.enable_local_buf) {
				struct dma_buf *dma = vb->planes[0].dbuf;

				if (dmabuf_is_uvm(dma) &&
					uvm_detach_hook_mod(dma, VF_PROCESS_DI) < 0) {
					v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
						"dmabuf without attach DI hook.\n");
				}
			}

			task_chain_clean(fb->task);

			ctx->cap_pool.seq[ctx->cap_pool.in++] =
				(V4L_CAP_BUFF_IN_M2M << 16 | vb->index);
			v4l2_m2m_buf_queue(ctx->m2m_ctx, vb2_v4l2);
			buf->que_in_m2m = true;

			fb->status = FB_ST_INIT;
			ATRACE_COUNTER("VC_IN_VSINK-2.storage", vb->index);

			/* check dpb ready */
			aml_check_dpb_ready(ctx);
		} else {
			struct vframe_s *vf = fb->vframe;
			struct task_chain_s *task = fb->task;

			v4l_dbg(ctx, V4L_DEBUG_CODEC_OUTPUT,
				"IN__BUFF (%s, st:%d, seq:%d) vb:(%d, %px), vf:(%d, %px), ts:%lld, "
				"Y:(%lx, %u) C/U:(%lx, %u) V:(%lx, %u)\n",
				ctx->ada_ctx->frm_name, fb->status, vf ? vf->index_disp : -1,
				vb->index, vb,
				vf ? vf->index & 0xff : -1, vf,
				vf ? vf->timestamp : 0,
				fb->m.mem[0].addr, fb->m.mem[0].size,
				fb->m.mem[1].addr, fb->m.mem[1].size,
				fb->m.mem[2].addr, fb->m.mem[2].size);

			ATRACE_COUNTER("VC_IN_VSINK-4.recycle", vb->index);

			task->recycle(task, TASK_TYPE_V4L_SINK);
		}

		wake_up_interruptible(&ctx->cap_wq);
		return;
	}

	v4l2_m2m_buf_queue(ctx->m2m_ctx, to_vb2_v4l2_buffer(vb));

	if (ctx->state != AML_STATE_INIT) {
		return;
	}

	buf->used = true;
	vb2_v4l2 = to_vb2_v4l2_buffer(vb);
	buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);
	if (buf->lastframe) {
		/* This shouldn't happen. Just in case. */
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Invalid flush buffer.\n");
		buf->used = false;
		v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		if (ctx->is_drm_mode && (vb->memory == VB2_MEMORY_DMABUF))
			wake_up_interruptible(&ctx->wq);

		return;
	}

	src_mem.index	= vb->index;
	src_mem.vaddr	= vb2_plane_vaddr(vb, 0);
	src_mem.addr	= sg_dma_address(buf->out_sgt->sgl);
	src_mem.size	= vb->planes[0].bytesused;
	src_mem.model	= vb->memory;
	src_mem.timestamp = vb->timestamp;
	src_mem.meta_ptr = (ulong)buf->meta_data;

	if (vdec_if_probe(ctx, &src_mem, NULL)) {
		buf->used = false;
		v4l2_m2m_src_buf_remove(ctx->m2m_ctx);

		if (ctx->is_drm_mode &&
			(src_mem.model == VB2_MEMORY_DMABUF)) {
			wake_up_interruptible(&ctx->wq);
		} else {
			v4l2_buff_done(to_vb2_v4l2_buffer(vb),
				VB2_BUF_STATE_DONE);
		}

		return;
	}

	/*
	 * If on model dmabuf must remove the buffer
	 * because this data has been consumed by hw.
	 */
	buf->used = false;
	v4l2_m2m_src_buf_remove(ctx->m2m_ctx);

	if (ctx->is_drm_mode &&
		(src_mem.model == VB2_MEMORY_DMABUF)) {
		wake_up_interruptible(&ctx->wq);
	} else if (ctx->param_sets_from_ucode) {
		v4l2_buff_done(to_vb2_v4l2_buffer(vb),
			VB2_BUF_STATE_DONE);
	}

	if (vdec_if_get_param(ctx, GET_PARAM_PIC_INFO, &ctx->picinfo)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"GET_PARAM_PICTURE_INFO err\n");
		return;
	}

	if (!ctx->picinfo.dpb_frames)
		return;

	v4l_buf_size_decision(ctx);
	ctx->last_decoded_picinfo = ctx->picinfo;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
		"Picture buffer count: dec:%u, vpp:%u, ge2d:%u, margin:%u, total:%u\n",
		ctx->picinfo.dpb_frames, ctx->vpp_size, ctx->ge2d_size,
		ctx->picinfo.dpb_margin,
		CTX_BUF_TOTAL(ctx));

	aml_vdec_dispatch_event(ctx, V4L2_EVENT_SRC_CH_RESOLUTION);

	mutex_lock(&ctx->state_lock);
	if (ctx->state == AML_STATE_INIT) {
		ctx->state = AML_STATE_PROBE;
		ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
		v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
			"vcodec state (AML_STATE_PROBE)\n");
	}
	mutex_unlock(&ctx->state_lock);
}

static void vb2ops_vdec_buf_finish(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct aml_video_dec_buf *buf = NULL;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d, idx: %d\n",
		__func__, vb->vb2_queue->type, vb->index);

	vb2_v4l2 = to_vb2_v4l2_buffer(vb);
	buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);

	if (buf->error) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Unrecoverable error on buffer.\n");
		ctx->state = AML_STATE_ABORT;
		ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
		v4l_dbg(ctx, V4L_DEBUG_CODEC_STATE,
			"vcodec state (AML_STATE_ABORT)\n");
	}
}

static int vb2ops_vdec_buf_init(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = container_of(vb,
					struct vb2_v4l2_buffer, vb2_buf);
	struct aml_video_dec_buf *buf = container_of(vb2_v4l2,
					struct aml_video_dec_buf, vb);
	struct vdec_v4l2_buffer *fb = &buf->frame_buffer;
	u32 size, phy_addr = 0;
	int i;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s, type: %d, idx: %d\n",
		__func__, vb->vb2_queue->type, vb->index);

	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		buf->lastframe = false;
	}

	/* codec_mm buffers count */
	if (!V4L2_TYPE_IS_OUTPUT(vb->type)) {
		if (vb->memory == VB2_MEMORY_MMAP) {
			char *owner = __getname();

			snprintf(owner, PATH_MAX, "%s-%d", "v4l-output", ctx->id);
			strncpy(buf->mem_onwer, owner, sizeof(buf->mem_onwer));
			buf->mem_onwer[sizeof(buf->mem_onwer) - 1] = '\0';
			__putname(owner);

			for (i = 0; i < vb->num_planes; i++) {
				size = vb->planes[i].length;
				phy_addr = vb2_dma_contig_plane_dma_addr(vb, i);
				buf->mem[i] = v4l_reqbufs_from_codec_mm(buf->mem_onwer,
						phy_addr, size, vb->index);
				v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
						"OUT %c alloc, addr: %x, size: %u, idx: %u\n",
						(i == 0? 'Y':'C'), phy_addr, size, vb->index);
			}
		} else if (vb->memory == VB2_MEMORY_DMABUF) {
			unsigned int dw_mode = VDEC_DW_NO_AFBC;

			for (i = 0; i < vb->num_planes; i++) {
				struct dma_buf * dma;

				if (vdec_if_get_param(ctx, GET_PARAM_DW_MODE, &dw_mode)) {
					v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR, "invalid dw_mode\n");
					return -EINVAL;
				}
				/* None-DW mode means single layer */
				if (dw_mode == VDEC_DW_AFBC_ONLY && i > 0) {
					v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
							"only support single plane in dw mode 0\n");
					return -EINVAL;
				}
				size = vb->planes[i].length;
				dma = vb->planes[i].dbuf;

				if (!dmabuf_is_uvm(dma))
					v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO, "non-uvm dmabuf\n");
			}
		}
	}

	if (!V4L2_TYPE_IS_OUTPUT(vb->type)) {
		struct vframe_s *vf = NULL;
		struct task_chain_s *task = NULL;
		struct task_chain_s *task_pre = fb->task;
		u32 icomp = -1;

		fb_map_table_fetch(ctx, vb, &vf, &task, &icomp);
		if (vf) {
			fb->task		= task;
			fb->vframe		= vf;
			vf->v4l_mem_handle	= (ulong)fb;
			buf->internal_index	= icomp;
			task_chain_update_object(task, fb);
		} else {
			buf->que_in_m2m = false;

			if (aml_uvm_buff_attach(vb))
				return -EFAULT;

			if (task_chain_init(&fb->task, ctx, fb, vb->index))
				return -EFAULT;

			list_add(&fb->task->node, &ctx->task_chain_pool);
 		}

		v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
			"init buffer(%s), vb idx:%d, task:(%px -> %px), addr:(%lx -> %lx), icomp:%d\n",
			vf ? "update" : "idel",
			vb->index, task_pre, fb->task,
			fb->m.mem[0].addr,
			(ulong) vb2_dma_contig_plane_dma_addr(vb, 0),
			(int)icomp);

		update_vdec_buf_plane(ctx, fb, vb);
	}

	if (V4L2_TYPE_IS_OUTPUT(vb->type)) {
		ulong contig_size;

		buf->out_sgt = vb2_dma_sg_plane_desc(vb, 0);

		contig_size = dmabuf_contiguous_size(buf->out_sgt);
		if (contig_size < vb->planes[0].bytesused) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
				"contiguous mapping is too small %lu/%u\n",
				contig_size, size);
			return -EFAULT;
		}
	}

	return 0;
}

static void vb2ops_vdec_buf_cleanup(struct vb2_buffer *vb)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = container_of(vb,
					struct vb2_v4l2_buffer, vb2_buf);
	struct aml_video_dec_buf *buf = container_of(vb2_v4l2,
					struct aml_video_dec_buf, vb);
	struct vdec_v4l2_buffer *fb = &buf->frame_buffer;;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s, type: %d, idx: %d\n",
		__func__, vb->vb2_queue->type, vb->index);

	if (!V4L2_TYPE_IS_OUTPUT(vb->type)) {
		if (vb->memory == VB2_MEMORY_MMAP) {
			int i;

			for (i = 0; i < vb->num_planes ; i++) {
				v4l_dbg(ctx, V4L_DEBUG_CODEC_BUFMGR,
					"OUT %c clean, addr: %lx, size: %u, idx: %u\n",
					(i == 0)? 'Y':'C',
					buf->mem[i]->phy_addr, buf->mem[i]->buffer_size, vb->index);
				v4l_freebufs_back_to_codec_mm(buf->mem_onwer, buf->mem[i]);
				buf->mem[i] = NULL;
			}
		}
		if (ctx->output_thread_ready && ctx->is_stream_off) {
			if (!is_fb_mapped(ctx, fb->m.mem[0].addr)) {
				struct task_chain_s *task;
				bool find_node = false;
				list_for_each_entry(task, &ctx->task_chain_pool, node) {
					if (task == fb->task) {
						find_node = true;
						break;
					}
				}
				if (find_node == true) {
					list_del(&fb->task->node);
					task_chain_clean(fb->task);
					task_chain_release(fb->task);
				}
			}
		}
	}
}

static int vb2ops_vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(q);

	ctx->has_receive_eos = false;
	ctx->v4l_resolution_change = false;

	/* vdec has ready to decode subsequence data of new resolution. */
	v4l2_m2m_job_resume(ctx->dev->m2m_dev_dec, ctx->m2m_ctx);

	v4l2_m2m_set_dst_buffered(ctx->fh.m2m_ctx, true);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d\n", __func__, q->type);

	return 0;
}

static void vb2ops_vdec_stop_streaming(struct vb2_queue *q)
{
	struct aml_video_dec_buf *buf = NULL;
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct aml_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	int i;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, type: %d, state: %x, frame_cnt: %d\n",
		__func__, q->type, ctx->state, ctx->decoded_frame_cnt);

	if (V4L2_TYPE_IS_OUTPUT(q->type))
		ctx->is_out_stream_off = true;
	else
		ctx->is_stream_off = true;

	if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		struct vb2_queue * que = v4l2_m2m_get_dst_vq(ctx->m2m_ctx);
		unsigned long flags;

		cancel_work_sync(&ctx->dmabuff_recycle_work);
		spin_lock_irqsave(&ctx->dmabuff_recycle_lock, flags);
		INIT_KFIFO(ctx->dmabuff_recycle);
		spin_unlock_irqrestore(&ctx->dmabuff_recycle_lock, flags);

		while ((vb2_v4l2 = v4l2_m2m_src_buf_remove(ctx->m2m_ctx)))
			v4l2_buff_done(vb2_v4l2, VB2_BUF_STATE_ERROR);

		for (i = 0; i < q->num_buffers; ++i) {
			vb2_v4l2 = to_vb2_v4l2_buffer(q->bufs[i]);
			if (vb2_v4l2->vb2_buf.state == VB2_BUF_STATE_ACTIVE)
				v4l2_buff_done(vb2_v4l2, VB2_BUF_STATE_ERROR);
		}

		/*
		 * drop es frame was stored in the vdec_input
		 * if the capture queue have not start streaming.
		 */
		if (!que->streaming &&
			(vdec_frame_number(ctx->ada_ctx) > 0) &&
			(ctx->state < AML_STATE_ACTIVE)) {
			ctx->state = AML_STATE_INIT;
			ATRACE_COUNTER("V_ST_VSINK-state", ctx->state);
			ctx->v4l_resolution_change = false;
			ctx->reset_flag = V4L_RESET_MODE_NORMAL;
			v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
				"force reset to drop es frames.\n");
			wake_up_interruptible(&ctx->cap_wq);
			aml_vdec_reset(ctx);
		}
	} else {
		/* clean output cache and decoder status . */
		if (ctx->state > AML_STATE_INIT) {
			wake_up_interruptible(&ctx->cap_wq);
			aml_vdec_reset(ctx);
		}

		cancel_work_sync(&ctx->decode_work);
		mutex_lock(&ctx->capture_buffer_lock);
		INIT_KFIFO(ctx->capture_buffer);
		mutex_unlock(&ctx->capture_buffer_lock);

		while ((vb2_v4l2 = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx)))
			v4l2_buff_done(vb2_v4l2, VB2_BUF_STATE_ERROR);

		for (i = 0; i < q->num_buffers; ++i) {
			vb2_v4l2 = to_vb2_v4l2_buffer(q->bufs[i]);
			buf = container_of(vb2_v4l2, struct aml_video_dec_buf, vb);
			buf->frame_buffer.status	= FB_ST_FREE;
			buf->frame_buffer.vframe	= NULL;
			buf->que_in_m2m			= false;
			buf->used			= false;
			buf->vb.flags			= 0;
			ctx->cap_pool.seq[i]		= 0;

			if (vb2_v4l2->vb2_buf.state == VB2_BUF_STATE_ACTIVE)
				v4l2_buff_done(vb2_v4l2, VB2_BUF_STATE_ERROR);

			/*v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO, "idx: %d, state: %d\n",
				q->bufs[i]->index, q->bufs[i]->state);*/
		}

		fb_map_table_clean(ctx);

		fb_token_clean(ctx);

		ctx->buf_used_count = 0;
		ctx->cap_pool.in = 0;
		ctx->cap_pool.out = 0;
		ctx->cap_pool.dec = 0;
		ctx->cap_pool.vpp = 0;
		ctx->cap_pool.ge2d = 0;
	}
}

static void m2mops_vdec_device_run(void *priv)
{
	struct aml_vcodec_ctx *ctx = priv;
	struct aml_vcodec_dev *dev = ctx->dev;

	if (ctx->output_thread_ready)
		queue_work(dev->decode_workqueue, &ctx->decode_work);
}

static int m2mops_vdec_job_ready(void *m2m_priv)
{
	struct aml_vcodec_ctx *ctx = m2m_priv;

	if (ctx->state < AML_STATE_PROBE ||
		ctx->state > AML_STATE_FLUSHED)
		return 0;

	return 1;
}

static void m2mops_vdec_job_abort(void *priv)
{
	struct aml_vcodec_ctx *ctx = priv;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO, "%s\n", __func__);
}

static int aml_vdec_g_v_ctrl(struct v4l2_ctrl *ctrl)
{
	struct aml_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);
	int ret = 0;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT,
		"%s, id: %d\n", __func__, ctrl->id);

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (ctx->state >= AML_STATE_PROBE) {
			ctrl->val = CTX_BUF_TOTAL(ctx);
		} else {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
				"Seqinfo not ready.\n");
			ctrl->val = 0;
		}
		break;
	case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:
		ctrl->val = 4;
		break;
	case AML_V4L2_GET_INPUT_BUFFER_NUM:
		if (ctx->ada_ctx != NULL)
			ctrl->val = vdec_frame_number(ctx->ada_ctx);
		break;
	case AML_V4L2_GET_FILMGRAIN_INFO:
		ctrl->val = ctx->film_grain_present;
		break;
	case AML_V4L2_GET_DECODER_INFO:
		memcpy(ctrl->p_new.p, &ctx->decoder_status_info,
			sizeof(struct aml_decoder_status_info));
		ctx->decoder_status_info.error_type = 0;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int aml_vdec_try_s_v_ctrl(struct v4l2_ctrl *ctrl)
{
	struct aml_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s\n", __func__);

	if (ctrl->id == AML_V4L2_SET_DRMMODE) {
		ctx->is_drm_mode = ctrl->val;
		ctx->param_sets_from_ucode = true;
		v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
			"set stream mode: %x\n", ctrl->val);
	} else if (ctrl->id == AML_V4L2_SET_DURATION) {
		vdec_set_duration(ctrl->val);
		v4l_dbg(ctx, V4L_DEBUG_CODEC_PRINFO,
			"set duration: %x\n", ctrl->val);
	}

	return 0;
}

static const struct v4l2_ctrl_ops aml_vcodec_dec_ctrl_ops = {
	.g_volatile_ctrl = aml_vdec_g_v_ctrl,
	.try_ctrl = aml_vdec_try_s_v_ctrl,
};

static const struct v4l2_ctrl_config ctrl_st_mode = {
	.name	= "drm mode",
	.id	= AML_V4L2_SET_DRMMODE,
	.ops	= &aml_vcodec_dec_ctrl_ops,
	.type	= V4L2_CTRL_TYPE_BOOLEAN,
	.flags	= V4L2_CTRL_FLAG_WRITE_ONLY,
	.min	= 0,
	.max	= 1,
	.step	= 1,
	.def	= 0,
};

static const struct v4l2_ctrl_config ctrl_gt_input_buffer_number = {
	.name	= "input buffer number",
	.id	= AML_V4L2_GET_INPUT_BUFFER_NUM,
	.ops	= &aml_vcodec_dec_ctrl_ops,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.flags	= V4L2_CTRL_FLAG_VOLATILE,
	.min	= 0,
	.max	= 128,
	.step	= 1,
	.def	= 0,
};

static const struct v4l2_ctrl_config ctrl_st_duration = {
	.name	= "duration",
	.id	= AML_V4L2_SET_DURATION,
	.ops	= &aml_vcodec_dec_ctrl_ops,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.flags	= V4L2_CTRL_FLAG_WRITE_ONLY,
	.min	= 0,
	.max	= 96000,
	.step	= 1,
	.def	= 0,
};

static const struct v4l2_ctrl_config ctrl_gt_filmgrain_info = {
	.name	= "filmgrain info",
	.id	= AML_V4L2_GET_FILMGRAIN_INFO,
	.ops	= &aml_vcodec_dec_ctrl_ops,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.flags	= V4L2_CTRL_FLAG_VOLATILE,
	.min	= 0,
	.max	= 1,
	.step	= 1,
	.def	= 0,
};

static const struct v4l2_ctrl_config ctrl_gt_decoder_info = {
	.name		= "decoder information",
	.id		= AML_V4L2_GET_DECODER_INFO,
	.ops		= &aml_vcodec_dec_ctrl_ops,
	.type		= V4L2_CTRL_COMPOUND_TYPES,
	.flags		= V4L2_CTRL_FLAG_VOLATILE,
	.min		= 0,
	.max		= sizeof(struct aml_decoder_status_info),
	.step		= 1,
	.elem_size	= 1,
	.dims		= { sizeof(struct aml_decoder_status_info) },
};

int aml_vcodec_dec_ctrls_setup(struct aml_vcodec_ctx *ctx)
{
	int ret;
	struct v4l2_ctrl *ctrl;

	v4l2_ctrl_handler_init(&ctx->ctrl_hdl, 3);
	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&aml_vcodec_dec_ctrl_ops,
				V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
				0, 32, 1, 2);
	if ((ctrl == NULL) || (ctx->ctrl_hdl.error)) {
		ret = ctx->ctrl_hdl.error;
		goto err;
	}
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&aml_vcodec_dec_ctrl_ops,
				V4L2_CID_MIN_BUFFERS_FOR_OUTPUT,
				0, 32, 1, 8);
	if ((ctrl == NULL) || (ctx->ctrl_hdl.error)) {
		ret = ctx->ctrl_hdl.error;
		goto err;
	}
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl, &ctrl_st_mode, NULL);
	if ((ctrl == NULL) || (ctx->ctrl_hdl.error)) {
		ret = ctx->ctrl_hdl.error;
		goto err;
	}

	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl, &ctrl_gt_input_buffer_number, NULL);
	if ((ctrl == NULL) || (ctx->ctrl_hdl.error)) {
		ret = ctx->ctrl_hdl.error;
		goto err;
	}

	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl, &ctrl_st_duration, NULL);
	if ((ctrl == NULL) || (ctx->ctrl_hdl.error)) {
		ret = ctx->ctrl_hdl.error;
		goto err;
	}

	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl, &ctrl_gt_filmgrain_info, NULL);
	if ((ctrl == NULL) || (ctx->ctrl_hdl.error)) {
		ret = ctx->ctrl_hdl.error;
		goto err;
	}

	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl, &ctrl_gt_decoder_info, NULL);
	if ((ctrl == NULL) || (ctx->ctrl_hdl.error)) {
		ret = ctx->ctrl_hdl.error;
		goto err;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);

	return 0;
err:
	v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
		"Adding control failed %d\n",
		ctx->ctrl_hdl.error);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
	return ret;
}

static int vidioc_vdec_g_parm(struct file *file, void *fh,
	struct v4l2_streamparm *a)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *dst_vq;

	dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (!dst_vq) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"no vb2 queue for type=%d\n", V4L2_BUF_TYPE_VIDEO_CAPTURE);
		return -EINVAL;
	}

	if (!V4L2_TYPE_IS_MULTIPLANAR(a->type) && dst_vq->is_multiplanar)
		return -EINVAL;

	if ((a->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) || (a->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) {
		if (vdec_if_get_param(ctx, GET_PARAM_CONFIG_INFO, &ctx->config.parm.dec))
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
				"GET_PARAM_CONFIG_INFO err\n");
		else
			memcpy(a->parm.raw_data, ctx->config.parm.data,
				sizeof(a->parm.raw_data));
	}

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s\n", __func__);

	return 0;
}

static int vidioc_vdec_g_pixelaspect(struct file *file, void *fh,
	int buf_type, struct v4l2_fract *aspect)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);
	u32 height_aspect_ratio, width_aspect_ratio;

	if ((aspect == NULL) || (ctx == NULL)) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"GET_PARAM_PIXEL_ASPECT_INFO err\n");
		return -EFAULT;
	}

	height_aspect_ratio = ctx->height_aspect_ratio;
	width_aspect_ratio = ctx->width_aspect_ratio;

	if ((height_aspect_ratio != 0) && (width_aspect_ratio != 0)) {
		aspect->numerator = height_aspect_ratio;
		aspect->denominator = width_aspect_ratio;
	}

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s: numerator is %d, denominator is %d\n",
		__func__, aspect->numerator, aspect->denominator);

	return 0;
}

static int check_dec_cfginfo(struct aml_vdec_cfg_infos *cfg)
{
	if (cfg->double_write_mode != 0 &&
		cfg->double_write_mode != 1 &&
		cfg->double_write_mode != 2 &&
		cfg->double_write_mode != 3 &&
		cfg->double_write_mode != 4 &&
		cfg->double_write_mode != 16 &&
		cfg->double_write_mode != 0x21 &&
		cfg->double_write_mode != 0x100 &&
		cfg->double_write_mode != 0x200) {
		pr_err("invalid double write mode %d\n", cfg->double_write_mode);
		return -1;
	}
	if (cfg->ref_buf_margin > 20) {
		pr_err("invalid margin %d\n", cfg->ref_buf_margin);
		return -1;
	}

	pr_info("double write mode %d margin %d\n",
		cfg->double_write_mode, cfg->ref_buf_margin);
	return 0;
}

static int vidioc_vdec_s_parm(struct file *file, void *fh,
	struct v4l2_streamparm *a)
{
	struct aml_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *dst_vq;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s\n", __func__);

	dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (!dst_vq) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"no vb2 queue for type=%d\n", V4L2_BUF_TYPE_VIDEO_CAPTURE);
		return -EINVAL;
	}

	if (!V4L2_TYPE_IS_MULTIPLANAR(a->type) && dst_vq->is_multiplanar)
		return -EINVAL;

	if (a->type == V4L2_BUF_TYPE_VIDEO_OUTPUT ||
		a->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		struct aml_dec_params *in =
			(struct aml_dec_params *) a->parm.raw_data;
		struct aml_dec_params *dec = &ctx->config.parm.dec;

		ctx->config.type = V4L2_CONFIG_PARM_DECODE;

		if (in->parms_status & V4L2_CONFIG_PARM_DECODE_CFGINFO) {
			if (check_dec_cfginfo(&in->cfg))
				return -EINVAL;
			dec->cfg = in->cfg;
			if (!vdec_if_set_param(ctx, SET_PARAM_CFG_INFO, &dec->cfg) &&
				!vdec_if_get_param(ctx, GET_PARAM_PIC_INFO, &ctx->picinfo)) {
				update_ctx_dimension(ctx, dst_vq->type);
			}
		}
		if (in->parms_status & V4L2_CONFIG_PARM_DECODE_PSINFO)
			dec->ps = in->ps;
		if (in->parms_status & V4L2_CONFIG_PARM_DECODE_HDRINFO)
			dec->hdr = in->hdr;
		if (in->parms_status & V4L2_CONFIG_PARM_DECODE_CNTINFO)
			dec->cnt = in->cnt;

		dec->parms_status |= in->parms_status;

		/* aml v4l driver parms config. */
		ctx->vpp_cfg.enable_nr =
			(dec->cfg.metadata_config_flag & (1 << 15));
		if (force_enable_nr)
			ctx->vpp_cfg.enable_nr = true;

		ctx->vpp_cfg.enable_local_buf =
			(dec->cfg.metadata_config_flag & (1 << 14));
		if (force_enable_di_local_buffer)
			ctx->vpp_cfg.enable_local_buf = true;
		ctx->vpp_cfg.dynamic_bypass_vpp =
			dec->cfg.metadata_config_flag & (1 << 10);

		ctx->ge2d_cfg.bypass =
			(dec->cfg.metadata_config_flag & (1 << 9));

		ctx->internal_dw_scale = dec->cfg.metadata_config_flag & (1 << 13);
		ctx->second_field_pts_mode = dec->cfg.metadata_config_flag & (1 << 12);

		v4l_dbg(ctx, V4L_DEBUG_CODEC_PROT, "%s parms:%x metadata_config_flag: 0x%x\n",
				__func__, in->parms_status, dec->cfg.metadata_config_flag);

		memset(a->parm.output.reserved, 0, sizeof(a->parm.output.reserved));
	} else {
		memset(a->parm.capture.reserved, 0, sizeof(a->parm.capture.reserved));
	}

	return 0;
}


const struct v4l2_m2m_ops aml_vdec_m2m_ops = {
	.device_run	= m2mops_vdec_device_run,
	.job_ready	= m2mops_vdec_job_ready,
	.job_abort	= m2mops_vdec_job_abort,
};

static const struct vb2_ops aml_vdec_vb2_ops = {
	.queue_setup	= vb2ops_vdec_queue_setup,
	.buf_prepare	= vb2ops_vdec_buf_prepare,
	.buf_queue	= vb2ops_vdec_buf_queue,
	.wait_prepare	= vb2_ops_wait_prepare,
	.wait_finish	= vb2_ops_wait_finish,
	.buf_init	= vb2ops_vdec_buf_init,
	.buf_cleanup	= vb2ops_vdec_buf_cleanup,
	.buf_finish	= vb2ops_vdec_buf_finish,
	.start_streaming = vb2ops_vdec_start_streaming,
	.stop_streaming	= vb2ops_vdec_stop_streaming,
};

const struct v4l2_ioctl_ops aml_vdec_ioctl_ops = {
	.vidioc_streamon		= vidioc_decoder_streamon,
	.vidioc_streamoff		= vidioc_decoder_streamoff,
	.vidioc_reqbufs			= vidioc_decoder_reqbufs,
	.vidioc_querybuf		= vidioc_vdec_querybuf,
	.vidioc_expbuf			= vidioc_vdec_expbuf,
	//.vidioc_g_ctrl		= vidioc_vdec_g_ctrl,

	.vidioc_qbuf			= vidioc_vdec_qbuf,
	.vidioc_dqbuf			= vidioc_vdec_dqbuf,

	.vidioc_try_fmt_vid_cap_mplane	= vidioc_try_fmt_vid_cap_out,
	.vidioc_try_fmt_vid_cap		= vidioc_try_fmt_vid_cap_out,
	.vidioc_try_fmt_vid_out_mplane	= vidioc_try_fmt_vid_cap_out,
	.vidioc_try_fmt_vid_out		= vidioc_try_fmt_vid_cap_out,

	.vidioc_s_fmt_vid_cap_mplane	= vidioc_vdec_s_fmt,
	.vidioc_s_fmt_vid_cap		= vidioc_vdec_s_fmt,
	.vidioc_s_fmt_vid_out_mplane	= vidioc_vdec_s_fmt,
	.vidioc_s_fmt_vid_out		= vidioc_vdec_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane	= vidioc_vdec_g_fmt,
	.vidioc_g_fmt_vid_cap		= vidioc_vdec_g_fmt,
	.vidioc_g_fmt_vid_out_mplane	= vidioc_vdec_g_fmt,
	.vidioc_g_fmt_vid_out		= vidioc_vdec_g_fmt,

	.vidioc_create_bufs		= vidioc_vdec_create_bufs,

	//fixme
	//.vidioc_enum_fmt_vid_cap_mplane	= vidioc_vdec_enum_fmt_vid_cap_mplane,
	//.vidioc_enum_fmt_vid_out_mplane = vidioc_vdec_enum_fmt_vid_out_mplane,
	.vidioc_enum_fmt_vid_cap	= vidioc_vdec_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out	= vidioc_vdec_enum_fmt_vid_out_mplane,
	.vidioc_enum_framesizes		= vidioc_enum_framesizes,

	.vidioc_querycap		= vidioc_vdec_querycap,
	.vidioc_subscribe_event		= vidioc_vdec_subscribe_evt,
	.vidioc_unsubscribe_event	= vidioc_vdec_event_unsubscribe,
	.vidioc_g_selection		= vidioc_vdec_g_selection,
	.vidioc_s_selection		= vidioc_vdec_s_selection,

	.vidioc_decoder_cmd		= vidioc_decoder_cmd,
	.vidioc_try_decoder_cmd		= vidioc_try_decoder_cmd,

	.vidioc_g_parm			= vidioc_vdec_g_parm,
	.vidioc_s_parm			= vidioc_vdec_s_parm,

	.vidioc_g_pixelaspect		= vidioc_vdec_g_pixelaspect,
};

int aml_vcodec_dec_queue_init(void *priv, struct vb2_queue *src_vq,
			   struct vb2_queue *dst_vq)
{
	struct aml_vcodec_ctx *ctx = priv;
	int ret = 0;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO, "%s\n", __func__);

	src_vq->type		= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes	= VB2_DMABUF | VB2_MMAP | VB2_USERPTR;
	src_vq->drv_priv	= ctx;
	src_vq->buf_struct_size = sizeof(struct aml_video_dec_buf);
	src_vq->ops		= &aml_vdec_vb2_ops;
	src_vq->mem_ops		= &vb2_dma_sg_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock		= &ctx->dev->dev_mutex;
	ret = vb2_queue_init(src_vq);
	if (ret) {
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Failed to initialize videobuf2 queue(output)\n");
		return ret;
	}

	dst_vq->type		= multiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE :
					V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dst_vq->io_modes	= VB2_DMABUF | VB2_MMAP | VB2_USERPTR;
	dst_vq->drv_priv	= ctx;
	dst_vq->buf_struct_size = sizeof(struct aml_video_dec_buf);
	dst_vq->ops		= &aml_vdec_vb2_ops;
	dst_vq->mem_ops		= &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock		= &ctx->dev->dev_mutex;
	dst_vq->min_buffers_needed = 1;
	ret = vb2_queue_init(dst_vq);
	if (ret) {
		vb2_queue_release(src_vq);
		v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
			"Failed to initialize videobuf2 queue(capture)\n");
	}

	return ret;
}

