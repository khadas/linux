// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <media/v4l2-common.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-dma-sg.h>
#include <uapi/linux/rk-video-format.h>

#include "dev.h"
#include "regs.h"

#define STREAM_OUT_REQ_BUFS_MIN 0

static void rkvpss_frame_end(struct rkvpss_stream *stream);
static int rkvpss_stream_crop(struct rkvpss_stream *stream, bool on, bool sync);
static int rkvpss_stream_scale(struct rkvpss_stream *stream, bool on, bool sync);
static void rkvpss_stream_mf(struct rkvpss_stream *stream);

static const struct capture_fmt scl_fmts[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV16,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV12,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_GREY,
		.fmt_type = FMT_YUV,
		.bpp = { 8 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV400,
	}, {
		.fourcc = V4L2_PIX_FMT_UYVY,
		.fmt_type = FMT_YUV,
		.bpp = { 16 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_422P,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}
};

static const struct capture_fmt scl1_fmts[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV16,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV12,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_GREY,
		.fmt_type = FMT_YUV,
		.bpp = { 8 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_42XSP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV400,
	}, {
		.fourcc = V4L2_PIX_FMT_UYVY,
		.fmt_type = FMT_YUV,
		.bpp = { 16 },
		.cplanes = 1,
		.mplanes = 1,
		.swap = 0,
		.wr_fmt = RKVPSS_MI_CHN_WR_422P,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_RGB565,
		.fmt_type = FMT_RGB,
		.bpp = { 16 },
		.mplanes = 1,
		.cplanes = 1,
		.wr_fmt = 0,
		.swap = RKVPSS_MI_CHN_WR_RB_SWAP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_RGB565,
	}, {
		.fourcc = V4L2_PIX_FMT_RGB24,
		.fmt_type = FMT_RGB,
		.bpp = { 24 },
		.mplanes = 1,
		.cplanes = 1,
		.wr_fmt = 0,
		.swap = RKVPSS_MI_CHN_WR_RB_SWAP,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_RGB888,
	}, {
		.fourcc = V4L2_PIX_FMT_XBGR32,
		.fmt_type = FMT_RGB,
		.bpp = { 32 },
		.mplanes = 1,
		.cplanes = 1,
		.wr_fmt = 0,
		.swap = 0,
		.output_fmt = RKVPSS_MI_CHN_WR_OUTPUT_ARGB888,
	}
};

static struct stream_config scl0_config = {
	.fmts = scl_fmts,
	.fmt_size = ARRAY_SIZE(scl_fmts),
	.frame_end_id = RKVPSS_MI_CHN0_FRM_END,
	.crop = {
		.ctrl = RKVPSS_CROP1_CTRL,
		.update = RKVPSS_CROP1_UPDATE,
		.h_offs = RKVPSS_CROP1_0_H_OFFS,
		.v_offs = RKVPSS_CROP1_0_V_OFFS,
		.h_size = RKVPSS_CROP1_0_H_SIZE,
		.v_size = RKVPSS_CROP1_0_V_SIZE,
		.ctrl_shd = RKVPSS_CROP1_CTRL_SHD,
		.h_offs_shd = RKVPSS_CROP1_0_H_OFFS_SHD,
		.v_offs_shd = RKVPSS_CROP1_0_V_OFFS_SHD,
		.h_size_shd = RKVPSS_CROP1_0_H_SIZE_SHD,
		.v_size_shd = RKVPSS_CROP1_0_V_SIZE_SHD,
	},
	.mi = {
		.ctrl = RKVPSS_MI_CHN0_WR_CTRL,
		.stride = RKVPSS_MI_CHN0_WR_Y_STRIDE,
		.y_base = RKVPSS_MI_CHN0_WR_Y_BASE,
		.uv_base = RKVPSS_MI_CHN0_WR_CB_BASE,
		.y_size = RKVPSS_MI_CHN0_WR_Y_SIZE,
		.uv_size = RKVPSS_MI_CHN0_WR_CB_SIZE,
		.y_offs_cnt = RKVPSS_MI_CHN0_WR_Y_OFFS_CNT,
		.uv_offs_cnt = RKVPSS_MI_CHN0_WR_CB_OFFS_CNT,
		.y_pic_width = RKVPSS_MI_CHN0_WR_Y_PIC_WIDTH,
		.y_pic_size = RKVPSS_MI_CHN0_WR_Y_PIC_SIZE,

		.ctrl_shd = RKVPSS_MI_CHN0_WR_CTRL_SHD,
		.y_shd = RKVPSS_MI_CHN0_WR_Y_BASE_SHD,
		.uv_shd = RKVPSS_MI_CHN0_WR_CB_BASE_SHD,
	},
};

static struct stream_config scl1_config = {
	.fmts = scl1_fmts,
	.fmt_size = ARRAY_SIZE(scl1_fmts),
	.frame_end_id = RKVPSS_MI_CHN1_FRM_END,
	.crop = {
		.ctrl = RKVPSS_CROP1_CTRL,
		.update = RKVPSS_CROP1_UPDATE,
		.h_offs = RKVPSS_CROP1_1_H_OFFS,
		.v_offs = RKVPSS_CROP1_1_V_OFFS,
		.h_size = RKVPSS_CROP1_1_H_SIZE,
		.v_size = RKVPSS_CROP1_1_V_SIZE,
		.ctrl_shd = RKVPSS_CROP1_CTRL_SHD,
		.h_offs_shd = RKVPSS_CROP1_1_H_OFFS_SHD,
		.v_offs_shd = RKVPSS_CROP1_1_V_OFFS_SHD,
		.h_size_shd = RKVPSS_CROP1_1_H_SIZE_SHD,
		.v_size_shd = RKVPSS_CROP1_1_V_SIZE_SHD,
	},
	.scale = {
		.ctrl = RKVPSS_SCALE1_CTRL,
		.update = RKVPSS_SCALE1_UPDATE,
		.src_size = RKVPSS_SCALE1_SRC_SIZE,
		.dst_size = RKVPSS_SCALE1_DST_SIZE,
		.hy_fac = RKVPSS_SCALE1_HY_FAC,
		.hc_fac = RKVPSS_SCALE1_HC_FAC,
		.vy_fac = RKVPSS_SCALE1_VY_FAC,
		.vc_fac = RKVPSS_SCALE1_VC_FAC,
		.hy_offs = RKVPSS_SCALE1_HY_OFFS,
		.hc_offs = RKVPSS_SCALE1_HC_OFFS,
		.vy_offs = RKVPSS_SCALE1_VY_OFFS,
		.vc_offs = RKVPSS_SCALE1_VC_OFFS,
		.hy_size = RKVPSS_SCALE1_HY_SIZE,
		.hc_size = RKVPSS_SCALE1_HC_SIZE,
		.hy_offs_mi = RKVPSS_SCALE1_HY_OFFS_MI,
		.hc_offs_mi = RKVPSS_SCALE1_HC_OFFS_MI,
		.in_crop_offs = RKVPSS_SCALE1_IN_CROP_OFFSET,
		.ctrl_shd = RKVPSS_SCALE1_CTRL_SHD,
		.src_size_shd = RKVPSS_SCALE1_SRC_SIZE_SHD,
		.dst_size_shd = RKVPSS_SCALE1_DST_SIZE_SHD,
		.hy_fac_shd = RKVPSS_SCALE1_HY_FAC_SHD,
		.hc_fac_shd = RKVPSS_SCALE1_HC_FAC_SHD,
		.vy_fac_shd = RKVPSS_SCALE1_VY_FAC_SHD,
		.vc_fac_shd = RKVPSS_SCALE1_VC_FAC_SHD,
		.hy_offs_shd = RKVPSS_SCALE1_HY_OFFS_SHD,
		.hc_offs_shd = RKVPSS_SCALE1_HC_OFFS_SHD,
		.vy_offs_shd = RKVPSS_SCALE1_VY_OFFS_SHD,
		.vc_offs_shd = RKVPSS_SCALE1_VC_OFFS_SHD,
		.hy_size_shd = RKVPSS_SCALE1_HY_SIZE_SHD,
		.hc_size_shd = RKVPSS_SCALE1_HC_SIZE_SHD,
		.hy_offs_mi_shd = RKVPSS_SCALE1_HY_OFFS_MI_SHD,
		.hc_offs_mi_shd = RKVPSS_SCALE1_HC_OFFS_MI_SHD,
		.in_crop_offs_shd = RKVPSS_SCALE1_IN_CROP_OFFSET_SHD,
	},
	.mi = {
		.ctrl = RKVPSS_MI_CHN1_WR_CTRL,
		.stride = RKVPSS_MI_CHN1_WR_Y_STRIDE,
		.y_base = RKVPSS_MI_CHN1_WR_Y_BASE,
		.uv_base = RKVPSS_MI_CHN1_WR_CB_BASE,
		.y_size = RKVPSS_MI_CHN1_WR_Y_SIZE,
		.uv_size = RKVPSS_MI_CHN1_WR_CB_SIZE,
		.y_offs_cnt = RKVPSS_MI_CHN1_WR_Y_OFFS_CNT,
		.uv_offs_cnt = RKVPSS_MI_CHN1_WR_CB_OFFS_CNT,
		.y_pic_width = RKVPSS_MI_CHN1_WR_Y_PIC_WIDTH,
		.y_pic_size = RKVPSS_MI_CHN1_WR_Y_PIC_SIZE,

		.ctrl_shd = RKVPSS_MI_CHN1_WR_CTRL_SHD,
		.y_shd = RKVPSS_MI_CHN1_WR_Y_BASE_SHD,
		.uv_shd = RKVPSS_MI_CHN1_WR_CB_BASE_SHD,
	},
};

static struct stream_config scl2_config = {
	.fmts = scl_fmts,
	.fmt_size = ARRAY_SIZE(scl_fmts),
	.frame_end_id = RKVPSS_MI_CHN2_FRM_END,
	.crop = {
		.ctrl = RKVPSS_CROP1_CTRL,
		.update = RKVPSS_CROP1_UPDATE,
		.h_offs = RKVPSS_CROP1_2_H_OFFS,
		.v_offs = RKVPSS_CROP1_2_V_OFFS,
		.h_size = RKVPSS_CROP1_2_H_SIZE,
		.v_size = RKVPSS_CROP1_2_V_SIZE,
		.ctrl_shd = RKVPSS_CROP1_CTRL_SHD,
		.h_offs_shd = RKVPSS_CROP1_2_H_OFFS_SHD,
		.v_offs_shd = RKVPSS_CROP1_2_V_OFFS_SHD,
		.h_size_shd = RKVPSS_CROP1_2_H_SIZE_SHD,
		.v_size_shd = RKVPSS_CROP1_2_V_SIZE_SHD,
	},
	.scale = {
		.ctrl = RKVPSS_SCALE1_CTRL,
		.update = RKVPSS_SCALE1_UPDATE,
		.src_size = RKVPSS_SCALE1_SRC_SIZE,
		.dst_size = RKVPSS_SCALE1_DST_SIZE,
		.hy_fac = RKVPSS_SCALE1_HY_FAC,
		.hc_fac = RKVPSS_SCALE1_HC_FAC,
		.vy_fac = RKVPSS_SCALE1_VY_FAC,
		.vc_fac = RKVPSS_SCALE1_VC_FAC,
		.hy_offs = RKVPSS_SCALE1_HY_OFFS,
		.hc_offs = RKVPSS_SCALE1_HC_OFFS,
		.vy_offs = RKVPSS_SCALE1_VY_OFFS,
		.vc_offs = RKVPSS_SCALE1_VC_OFFS,
		.hy_size = RKVPSS_SCALE1_HY_SIZE,
		.hc_size = RKVPSS_SCALE1_HC_SIZE,
		.hy_offs_mi = RKVPSS_SCALE1_HY_OFFS_MI,
		.hc_offs_mi = RKVPSS_SCALE1_HC_OFFS_MI,
		.in_crop_offs = RKVPSS_SCALE1_IN_CROP_OFFSET,
		.ctrl_shd = RKVPSS_SCALE1_CTRL_SHD,
		.src_size_shd = RKVPSS_SCALE1_SRC_SIZE_SHD,
		.dst_size_shd = RKVPSS_SCALE1_DST_SIZE_SHD,
		.hy_fac_shd = RKVPSS_SCALE1_HY_FAC_SHD,
		.hc_fac_shd = RKVPSS_SCALE1_HC_FAC_SHD,
		.vy_fac_shd = RKVPSS_SCALE1_VY_FAC_SHD,
		.vc_fac_shd = RKVPSS_SCALE1_VC_FAC_SHD,
		.hy_offs_shd = RKVPSS_SCALE1_HY_OFFS_SHD,
		.hc_offs_shd = RKVPSS_SCALE1_HC_OFFS_SHD,
		.vy_offs_shd = RKVPSS_SCALE1_VY_OFFS_SHD,
		.vc_offs_shd = RKVPSS_SCALE1_VC_OFFS_SHD,
		.hy_size_shd = RKVPSS_SCALE1_HY_SIZE_SHD,
		.hc_size_shd = RKVPSS_SCALE1_HC_SIZE_SHD,
		.hy_offs_mi_shd = RKVPSS_SCALE1_HY_OFFS_MI_SHD,
		.hc_offs_mi_shd = RKVPSS_SCALE1_HC_OFFS_MI_SHD,
		.in_crop_offs_shd = RKVPSS_SCALE1_IN_CROP_OFFSET_SHD,
	},
	.mi = {
		.ctrl = RKVPSS_MI_CHN2_WR_CTRL,
		.stride = RKVPSS_MI_CHN2_WR_Y_STRIDE,
		.y_base = RKVPSS_MI_CHN2_WR_Y_BASE,
		.uv_base = RKVPSS_MI_CHN2_WR_CB_BASE,
		.y_size = RKVPSS_MI_CHN2_WR_Y_SIZE,
		.uv_size = RKVPSS_MI_CHN2_WR_CB_SIZE,
		.y_offs_cnt = RKVPSS_MI_CHN2_WR_Y_OFFS_CNT,
		.uv_offs_cnt = RKVPSS_MI_CHN2_WR_CB_OFFS_CNT,
		.y_pic_width = RKVPSS_MI_CHN2_WR_Y_PIC_WIDTH,
		.y_pic_size = RKVPSS_MI_CHN2_WR_Y_PIC_SIZE,

		.ctrl_shd = RKVPSS_MI_CHN2_WR_CTRL_SHD,
		.y_shd = RKVPSS_MI_CHN2_WR_Y_BASE_SHD,
		.uv_shd = RKVPSS_MI_CHN2_WR_CB_BASE_SHD,
	},
};

static struct stream_config scl3_config = {
	.fmts = scl_fmts,
	.fmt_size = ARRAY_SIZE(scl_fmts),
	.frame_end_id = RKVPSS_MI_CHN3_FRM_END,
	.crop = {
		.ctrl = RKVPSS_CROP1_CTRL,
		.update = RKVPSS_CROP1_UPDATE,
		.h_offs = RKVPSS_CROP1_3_H_OFFS,
		.v_offs = RKVPSS_CROP1_3_V_OFFS,
		.h_size = RKVPSS_CROP1_3_H_SIZE,
		.v_size = RKVPSS_CROP1_3_V_SIZE,
		.ctrl_shd = RKVPSS_CROP1_CTRL_SHD,
		.h_offs_shd = RKVPSS_CROP1_3_H_OFFS_SHD,
		.v_offs_shd = RKVPSS_CROP1_3_V_OFFS_SHD,
		.h_size_shd = RKVPSS_CROP1_3_H_SIZE_SHD,
		.v_size_shd = RKVPSS_CROP1_3_V_SIZE_SHD,
	},
	.scale = {
		.ctrl = RKVPSS_SCALE3_CTRL,
		.update = RKVPSS_SCALE3_UPDATE,
		.src_size = RKVPSS_SCALE3_SRC_SIZE,
		.dst_size = RKVPSS_SCALE3_DST_SIZE,
		.hy_fac = RKVPSS_SCALE3_HY_FAC,
		.hc_fac = RKVPSS_SCALE3_HC_FAC,
		.vy_fac = RKVPSS_SCALE3_VY_FAC,
		.vc_fac = RKVPSS_SCALE3_VC_FAC,
		.hy_offs = RKVPSS_SCALE3_HY_OFFS,
		.hc_offs = RKVPSS_SCALE3_HC_OFFS,
		.vy_offs = RKVPSS_SCALE3_VY_OFFS,
		.vc_offs = RKVPSS_SCALE3_VC_OFFS,
		.hy_size = RKVPSS_SCALE3_HY_SIZE,
		.hc_size = RKVPSS_SCALE3_HC_SIZE,
		.hy_offs_mi = RKVPSS_SCALE3_HY_OFFS_MI,
		.hc_offs_mi = RKVPSS_SCALE3_HC_OFFS_MI,
		.in_crop_offs = RKVPSS_SCALE3_IN_CROP_OFFSET,
		.ctrl_shd = RKVPSS_SCALE3_CTRL_SHD,
		.src_size_shd = RKVPSS_SCALE3_SRC_SIZE_SHD,
		.dst_size_shd = RKVPSS_SCALE3_DST_SIZE_SHD,
		.hy_fac_shd = RKVPSS_SCALE3_HY_FAC_SHD,
		.hc_fac_shd = RKVPSS_SCALE3_HC_FAC_SHD,
		.vy_fac_shd = RKVPSS_SCALE3_VY_FAC_SHD,
		.vc_fac_shd = RKVPSS_SCALE3_VC_FAC_SHD,
		.hy_offs_shd = RKVPSS_SCALE3_HY_OFFS_SHD,
		.hc_offs_shd = RKVPSS_SCALE3_HC_OFFS_SHD,
		.vy_offs_shd = RKVPSS_SCALE3_VY_OFFS_SHD,
		.vc_offs_shd = RKVPSS_SCALE3_VC_OFFS_SHD,
		.hy_size_shd = RKVPSS_SCALE3_HY_SIZE_SHD,
		.hc_size_shd = RKVPSS_SCALE3_HC_SIZE_SHD,
		.hy_offs_mi_shd = RKVPSS_SCALE3_HY_OFFS_MI_SHD,
		.hc_offs_mi_shd = RKVPSS_SCALE3_HC_OFFS_MI_SHD,
		.in_crop_offs_shd = RKVPSS_SCALE3_IN_CROP_OFFSET_SHD,
	},
	.mi = {
		.ctrl = RKVPSS_MI_CHN3_WR_CTRL,
		.stride = RKVPSS_MI_CHN3_WR_Y_STRIDE,
		.y_base = RKVPSS_MI_CHN3_WR_Y_BASE,
		.uv_base = RKVPSS_MI_CHN3_WR_CB_BASE,
		.y_size = RKVPSS_MI_CHN3_WR_Y_SIZE,
		.uv_size = RKVPSS_MI_CHN3_WR_CB_SIZE,
		.y_offs_cnt = RKVPSS_MI_CHN3_WR_Y_OFFS_CNT,
		.uv_offs_cnt = RKVPSS_MI_CHN3_WR_CB_OFFS_CNT,
		.y_pic_width = RKVPSS_MI_CHN3_WR_Y_PIC_WIDTH,
		.y_pic_size = RKVPSS_MI_CHN3_WR_Y_PIC_SIZE,

		.ctrl_shd = RKVPSS_MI_CHN3_WR_CTRL_SHD,
		.y_shd = RKVPSS_MI_CHN3_WR_Y_BASE_SHD,
		.uv_shd = RKVPSS_MI_CHN3_WR_CB_BASE_SHD,
	},
};

static void stream_frame_start(struct rkvpss_stream *stream, u32 irq)
{
	if (stream->is_crop_upd) {
		rkvpss_stream_scale(stream, true, !irq);
		rkvpss_stream_crop(stream, true, !irq);
	}
	if (!irq && !stream->curr_buf &&
	    !stream->dev->hw_dev->is_single)
		stream->ops->update_mi(stream);
}

static void scl_force_update(struct rkvpss_stream *stream)
{
	u32 val;

	switch (stream->id) {
	case RKVPSS_OUTPUT_CH0:
		val = RKVPSS_MI_CHN0_FORCE_UPD;
		break;
	case RKVPSS_OUTPUT_CH1:
		val = RKVPSS_MI_CHN1_FORCE_UPD;
		break;
	case RKVPSS_OUTPUT_CH2:
		val = RKVPSS_MI_CHN2_FORCE_UPD;
		break;
	case RKVPSS_OUTPUT_CH3:
		val = RKVPSS_MI_CHN3_FORCE_UPD;
		break;
	default:
		return;
	}
	/* need update two for online2 mode */
	rkvpss_write(stream->dev, RKVPSS_MI_WR_INIT, val);
	rkvpss_write(stream->dev, RKVPSS_MI_WR_INIT, val);
}

static void scl_update_mi(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	struct rkvpss_buffer *buf = NULL;
	unsigned long lock_flags = 0;
	u32 val;

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	if (!list_empty(&stream->buf_queue) && !stream->curr_buf) {
		buf = list_first_entry(&stream->buf_queue, struct rkvpss_buffer, queue);
		list_del(&buf->queue);
		stream->curr_buf = buf;
	}
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);

	if (buf) {
		val = buf->dma[0];
		rkvpss_write(dev, stream->config->mi.y_base, val);
		val = buf->dma[1];
		rkvpss_write(dev, stream->config->mi.uv_base, val);
		scl_force_update(stream);
		if (stream->is_pause) {
			stream->is_pause = false;
			stream->ops->enable_mi(stream);
		}
	} else if (!stream->is_pause) {
		stream->is_pause = true;
		stream->ops->disable_mi(stream);
	}
	v4l2_dbg(2, rkvpss_debug, &dev->v4l2_dev,
		 "%s id:%d Y:0x%x UV:0x%x | Y_SHD:0x%x\n",
		__func__, stream->id,
		rkvpss_read(dev, stream->config->mi.y_base),
		rkvpss_read(dev, stream->config->mi.uv_base),
		rkvpss_hw_read(dev->hw_dev, stream->config->mi.y_shd));
}

static void scl_config_mi(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	struct capture_fmt *fmt = &stream->out_cap_fmt;
	struct v4l2_pix_format_mplane *out_fmt = &stream->out_fmt;
	u32 reg, val;

	val = out_fmt->plane_fmt[0].bytesperline;
	reg = stream->config->mi.stride;
	rkvpss_write(dev, reg, val);

	val *= out_fmt->height;
	reg = stream->config->mi.y_pic_size;
	rkvpss_write(dev, reg, val);

	val = out_fmt->plane_fmt[0].bytesperline * out_fmt->height;
	reg = stream->config->mi.y_size;
	rkvpss_write(dev, reg, val);

	val = out_fmt->plane_fmt[1].sizeimage;
	reg = stream->config->mi.uv_size;
	rkvpss_write(dev, reg, val);

	reg = stream->config->mi.y_offs_cnt;
	rkvpss_write(dev, reg, 0);
	reg = stream->config->mi.uv_offs_cnt;
	rkvpss_write(dev, reg, 0);

	val = fmt->wr_fmt | fmt->output_fmt | fmt->swap |
	      RKVPSS_MI_CHN_WR_EN | RKVPSS_MI_CHN_WR_AUTO_UPD;
	reg = stream->config->mi.ctrl;
	rkvpss_write(dev, reg, val);

	stream->is_mf_upd = true;
	rkvpss_frame_end(stream);
}

static void scl_enable_mi(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	u32 val, mask;

	switch (stream->id) {
	case RKVPSS_OUTPUT_CH0:
		val = RKVPSS_ISP2VPSS_CHN0_SEL(2);
		mask = RKVPSS_ISP2VPSS_CHN0_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH1:
		val = RKVPSS_ISP2VPSS_CHN1_SEL(2);
		mask = RKVPSS_ISP2VPSS_CHN1_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH2:
		val = RKVPSS_ISP2VPSS_CHN2_SEL(2);
		mask = RKVPSS_ISP2VPSS_CHN2_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH3:
		val = RKVPSS_ISP2VPSS_CHN3_SEL(2);
		mask = RKVPSS_ISP2VPSS_CHN3_SEL(3);
		break;
	default:
		return;
	}
	val |= RKVPSS_ISP2VPSS_ONLINE2;
	if (!dev->hw_dev->is_ofl_cmsc)
		val |= RKVPSS_ISP2VPSS_ONLINE2_CMSC_EN;
	rkvpss_set_bits(dev, RKVPSS_VPSS_ONLINE, mask, val);
	rkvpss_write(dev, RKVPSS_VPSS_UPDATE, RKVPSS_ONLINE2_CHN_FORCE_UPD);
}

static void scl_disable_mi(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	u32 val;

	switch (stream->id) {
	case RKVPSS_OUTPUT_CH0:
		val = RKVPSS_ISP2VPSS_CHN0_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH1:
		val = RKVPSS_ISP2VPSS_CHN1_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH2:
		val = RKVPSS_ISP2VPSS_CHN2_SEL(3);
		break;
	case RKVPSS_OUTPUT_CH3:
		val = RKVPSS_ISP2VPSS_CHN3_SEL(3);
		break;
	default:
		return;
	}

	rkvpss_clear_bits(dev, RKVPSS_VPSS_ONLINE, val);
	rkvpss_write(dev, RKVPSS_VPSS_UPDATE, RKVPSS_ONLINE2_CHN_FORCE_UPD);
}

static struct streams_ops scl_stream_ops = {
	.config_mi = scl_config_mi,
	.enable_mi = scl_enable_mi,
	.disable_mi = scl_disable_mi,
	.update_mi = scl_update_mi,
	.frame_start = stream_frame_start,
};

static void rkvpss_buf_done_task(unsigned long arg)
{
	struct rkvpss_stream *stream = (struct rkvpss_stream *)arg;
	struct rkvpss_vdev_node *node = &stream->vnode;
	struct rkvpss_buffer *buf = NULL;
	unsigned long lock_flags = 0;
	LIST_HEAD(local_list);

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	list_replace_init(&stream->buf_done_list, &local_list);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);

	while (!list_empty(&local_list)) {
		buf = list_first_entry(&local_list, struct rkvpss_buffer, queue);
		list_del(&buf->queue);
		v4l2_dbg(2, rkvpss_debug, &stream->dev->v4l2_dev,
			 "%s id:%d seq:%d buf:0x%x done\n",
			 node->vdev.name, stream->id, buf->vb.sequence, buf->dma[0]);
		vb2_buffer_done(&buf->vb.vb2_buf,
				stream->streaming ? VB2_BUF_STATE_DONE : VB2_BUF_STATE_ERROR);
	}
}

static void rkvpss_stream_buf_done(struct rkvpss_stream *stream,
				   struct rkvpss_buffer *buf)
{
	unsigned long lock_flags = 0;

	if (!stream || !buf)
		return;

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	list_add_tail(&buf->queue, &stream->buf_done_list);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
	tasklet_schedule(&stream->buf_done_tasklet);
}

static void rkvpss_frame_end(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	struct rkvpss_subdev *sdev = &dev->vpss_sdev;
	struct rkvpss_buffer *buf = NULL;
	unsigned long lock_flags = 0;

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	if (stream->curr_buf) {
		buf = stream->curr_buf;
		stream->curr_buf = NULL;
	}
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);

	if (buf) {
		struct vb2_buffer *vb2_buf = &buf->vb.vb2_buf;
		struct capture_fmt *fmt = &stream->out_cap_fmt;
		u64 ns = sdev->frame_timestamp;
		int i;

		for (i = 0; i < fmt->mplanes; i++)
			vb2_set_plane_payload(vb2_buf, i, stream->out_fmt.plane_fmt[i].sizeimage);
		if (!ns)
			ns = ktime_get_ns();
		buf->vb.vb2_buf.timestamp = ns;
		buf->vb.sequence = sdev->frame_seq;
		rkvpss_stream_buf_done(stream, buf);
	}

	rkvpss_stream_mf(stream);
	stream->ops->update_mi(stream);
}

static int rkvpss_queue_setup(struct vb2_queue *queue,
			      unsigned int *num_buffers,
			      unsigned int *num_planes,
			      unsigned int sizes[],
			      struct device *alloc_ctxs[])
{
	struct rkvpss_stream *stream = queue->drv_priv;
	struct rkvpss_device *dev = stream->dev;
	const struct v4l2_pix_format_mplane *pixm = NULL;
	const struct capture_fmt *cap_fmt = NULL;
	u32 i;

	pixm = &stream->out_fmt;
	if (!pixm->width || !pixm->height)
		return -EINVAL;
	cap_fmt = &stream->out_cap_fmt;
	*num_planes = cap_fmt->mplanes;

	for (i = 0; i < cap_fmt->mplanes; i++) {
		const struct v4l2_plane_pix_format *plane_fmt;

		plane_fmt = &pixm->plane_fmt[i];
		sizes[i] = plane_fmt->sizeimage / pixm->height * ALIGN(pixm->height, 16);
	}

	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s stream:%d count %d, size %d\n",
		 v4l2_type_names[queue->type],
		 stream->id, *num_buffers, sizes[0]);

	return 0;
}

static void rkvpss_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkvpss_buffer *vpssbuf = to_rkvpss_buffer(vbuf);
	struct vb2_queue *queue = vb->vb2_queue;
	struct rkvpss_stream *stream = queue->drv_priv;
	struct rkvpss_device *dev = stream->dev;
	struct v4l2_pix_format_mplane *pixm = &stream->out_fmt;
	struct capture_fmt *cap_fmt = &stream->out_cap_fmt;
	unsigned long lock_flags = 0;
	struct sg_table *sgt;
	u32 height, size;
	int i;

	for (i = 0; i < cap_fmt->mplanes; i++) {
		sgt = vb2_dma_sg_plane_desc(vb, i);
		vpssbuf->dma[i] = sg_dma_address(sgt->sgl);
	}
	/*
	 * NOTE: plane_fmt[0].sizeimage is total size of all planes for single
	 * memory plane formats, so calculate the size explicitly.
	 */
	if (cap_fmt->mplanes == 1) {
		for (i = 0; i < cap_fmt->cplanes - 1; i++) {
			height = pixm->height;
			size = (i == 0) ?
				pixm->plane_fmt[i].bytesperline * height :
				pixm->plane_fmt[i].sizeimage;
			vpssbuf->dma[i + 1] = vpssbuf->dma[i] + size;
		}
	}

	v4l2_dbg(2, rkvpss_debug, &dev->v4l2_dev,
		 "%s stream:%d buf:0x%x\n", __func__,
		 stream->id, vpssbuf->dma[0]);

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	list_add_tail(&vpssbuf->queue, &stream->buf_queue);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
	if (dev->hw_dev->is_single &&
	    stream->streaming && !stream->curr_buf)
		stream->ops->update_mi(stream);
}

static void destroy_buf_queue(struct rkvpss_stream *stream,
			      enum vb2_buffer_state state)
{
	unsigned long lock_flags = 0;
	struct rkvpss_buffer *buf;

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	if (stream->curr_buf) {
		list_add_tail(&stream->curr_buf->queue, &stream->buf_queue);
		stream->curr_buf = NULL;
	}
	while (!list_empty(&stream->buf_queue)) {
		buf = list_first_entry(&stream->buf_queue, struct rkvpss_buffer, queue);
		list_del(&buf->queue);
		buf->vb.vb2_buf.synced = false;
		vb2_buffer_done(&buf->vb.vb2_buf, state);
	}
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
}

static int rkvpss_stream_crop(struct rkvpss_stream *stream, bool on, bool sync)
{
	struct rkvpss_device *dev = stream->dev;
	struct v4l2_rect *crop = &stream->crop;
	u32 reg_ctrl = stream->config->crop.ctrl;
	u32 reg_upd = stream->config->crop.update;
	u32 reg_h_offs = stream->config->crop.h_offs;
	u32 reg_v_offs = stream->config->crop.v_offs;
	u32 reg_h_size = stream->config->crop.h_size;
	u32 reg_v_size = stream->config->crop.v_size;
	u32 val;

	val = RKVPSS_CROP_CHN_EN(stream->id);
	if (on) {
		rkvpss_set_bits(dev, reg_ctrl, 0, val);
		rkvpss_write(dev, reg_h_offs, crop->left);
		rkvpss_write(dev, reg_v_offs, crop->top);
		rkvpss_write(dev, reg_h_size, crop->width);
		rkvpss_write(dev, reg_v_size, crop->height);
	} else {
		rkvpss_clear_bits(dev, reg_ctrl, val);
	}
	if (sync) {
		val = RKVPSS_CROP_CHN_FORCE_UPD(stream->id);
		val |= RKVPSS_CROP_GEN_UPD;
		rkvpss_write(dev, reg_upd, val);
	}
	stream->is_crop_upd = false;
	return 0;
}

static void poly_phase_scale(struct rkvpss_stream *stream, bool on, bool sync)
{
	struct rkvpss_device *dev = stream->dev;
	u32 out_w = stream->out_fmt.width;
	u32 out_h = stream->out_fmt.height;
	u32 in_w = stream->crop.width;
	u32 in_h = stream->crop.height;
	u32 ctrl, y_xscl_fac, y_yscl_fac, uv_xscl_fac, uv_yscl_fac;
	u32 i, j, idx, ratio, val, in_div, out_div, factor;
	bool dering_en = false, yuv420_in = false, yuv422_to_420 = false;

	if (!on) {
		rkvpss_write(dev, RKVPSS_ZME_Y_SCL_CTRL, 0);
		rkvpss_write(dev, RKVPSS_ZME_UV_SCL_CTRL, 0);
		return;
	}

	/* TODO diff for input and output format */
	if (yuv420_in) {
		in_div = 2;
		out_div = 2;
	} else if (yuv422_to_420) {
		in_div = 1;
		out_div = 2;
	} else {
		in_div = 1;
		out_div = 1;
	}

	val = (in_w - 1) | ((in_h - 1) << 16);
	rkvpss_write(dev, RKVPSS_ZME_Y_SRC_SIZE, val);
	rkvpss_write(dev, RKVPSS_ZME_UV_SRC_SIZE, val);
	val = (out_w - 1) | ((out_h - 1) << 16);
	rkvpss_write(dev, RKVPSS_ZME_Y_DST_SIZE, val);
	rkvpss_write(dev, RKVPSS_ZME_UV_DST_SIZE, val);

	ctrl = RKVPSS_ZME_XSCL_MODE | RKVPSS_ZME_YSCL_MODE;
	if (dering_en) {
		ctrl |= RKVPSS_ZME_DERING_EN;
		rkvpss_write(dev, RKVPSS_ZME_Y_DERING_PARA, 0xd10410);
		rkvpss_write(dev, RKVPSS_ZME_UV_DERING_PARA, 0xd10410);
	}
	if (in_w != out_w) {
		if (in_w > out_w) {
			factor = 4096;
			ctrl |= RKVPSS_ZME_XSD_EN;
		} else {
			factor = 65536;
			ctrl |= RKVPSS_ZME_XSU_EN;
		}
		y_xscl_fac = (in_w - 1) * factor / (out_w - 1);
		uv_xscl_fac = (in_w / 2 - 1) * factor / (out_w / 2 - 1);

		ratio = y_xscl_fac * 10000 / factor;
		idx = rkvpss_get_zme_tap_coe_index(ratio);
		for (i = 0; i < 17; i++) {
			for (j = 0; j < 8; j += 2) {
				val = RKVPSS_ZME_TAP_COE(rkvpss_zme_tap8_coe[idx][i][j],
							 rkvpss_zme_tap8_coe[idx][i][j + 1]);
				rkvpss_write(dev, RKVPSS_ZME_Y_HOR_COE0_10 + i * 16 + j * 2, val);
				rkvpss_write(dev, RKVPSS_ZME_UV_HOR_COE0_10 + i * 16 + j * 2, val);
			}
		}
	} else {
		y_xscl_fac = 0;
		uv_xscl_fac = 0;
	}
	rkvpss_write(dev, RKVPSS_ZME_Y_XSCL_FACTOR, y_xscl_fac);
	rkvpss_write(dev, RKVPSS_ZME_UV_XSCL_FACTOR, uv_xscl_fac);
	if (in_h != out_h) {
		if (in_h > out_h) {
			factor = 4096;
			ctrl |= RKVPSS_ZME_YSD_EN;
		} else {
			factor = 65536;
			ctrl |= RKVPSS_ZME_YSU_EN;
		}
		y_yscl_fac = (in_h - 1) * factor / (out_h - 1);
		uv_yscl_fac = (in_h / in_div - 1) * factor / (out_h / out_div - 1);

		ratio = y_yscl_fac * 10000 / factor;
		idx = rkvpss_get_zme_tap_coe_index(ratio);
		for (i = 0; i < 17; i++) {
			for (j = 0; j < 8; j += 2) {
				val = RKVPSS_ZME_TAP_COE(rkvpss_zme_tap6_coe[idx][i][j],
							 rkvpss_zme_tap6_coe[idx][i][j + 1]);
				rkvpss_write(dev, RKVPSS_ZME_Y_VER_COE0_10 + i * 16 + j * 2, val);
				rkvpss_write(dev, RKVPSS_ZME_UV_VER_COE0_10 + i * 16 + j * 2, val);
			}
		}
	} else {
		y_yscl_fac = 0;
		uv_yscl_fac = 0;
	}
	rkvpss_write(dev, RKVPSS_ZME_Y_YSCL_FACTOR, y_yscl_fac);
	rkvpss_write(dev, RKVPSS_ZME_UV_YSCL_FACTOR, uv_yscl_fac);

	rkvpss_write(dev, RKVPSS_ZME_Y_SCL_CTRL, ctrl);
	rkvpss_write(dev, RKVPSS_ZME_UV_SCL_CTRL, ctrl);

	ctrl = RKVPSS_ZME_GATING_EN;
	if (yuv420_in)
		ctrl |= RKVPSS_ZME_SCL_YUV420_REAL_EN;
	if (yuv422_to_420)
		ctrl |= RKVPSS_ZME_422TO420_EN;
	rkvpss_write(dev, RKVPSS_ZME_CTRL, ctrl);

	val = RKVPSS_ZME_GEN_UPD;
	if (sync)
		val |= RKVPSS_ZME_FORCE_UPD;
	rkvpss_write(dev, RKVPSS_ZME_UPDATE, val);
}

static void bilinear_scale(struct rkvpss_stream *stream, bool on, bool sync)
{
	struct rkvpss_device *dev = stream->dev;
	u32 out_w = stream->out_fmt.width;
	u32 out_h = stream->out_fmt.height;
	u32 in_w = stream->crop.width;
	u32 in_h = stream->crop.height;
	u32 in_div, out_div;
	u32 reg, val, ctrl = 0;
	bool yuv420_in = false, yuv422_to_420 = false;

	if (!on) {
		reg = stream->config->scale.ctrl;
		rkvpss_write(dev, reg, 0);
		return;
	}

	/* TODO diff for input and output format */
	if (yuv420_in) {
		in_div = 2;
		out_div = 2;
	} else if (yuv422_to_420) {
		in_div = 1;
		out_div = 2;
	} else {
		in_div = 1;
		out_div = 1;
	}

	reg = stream->config->scale.hy_offs;
	rkvpss_write(dev, reg, 0);
	reg = stream->config->scale.hc_offs;
	rkvpss_write(dev, reg, 0);
	reg = stream->config->scale.vy_offs;
	rkvpss_write(dev, reg, 0);
	reg = stream->config->scale.vc_offs;
	rkvpss_write(dev, reg, 0);

	val = in_w | (in_h << 16);
	reg = stream->config->scale.src_size;
	rkvpss_write(dev, reg, val);
	val = out_w | (out_h << 16);
	reg = stream->config->scale.dst_size;
	rkvpss_write(dev, reg, val);

	if (in_w != out_w) {
		val = (in_w - 1) * 4096 / (out_w - 1);
		reg = stream->config->scale.hy_fac;
		rkvpss_write(dev, reg, val);

		val = (in_w / 2 - 1) * 4096 / (out_w / 2 - 1);
		reg = stream->config->scale.hc_fac;
		rkvpss_write(dev, reg, val);

		ctrl |= RKVPSS_SCL_HY_EN | RKVPSS_SCL_HC_EN;
	}
	if (in_h != out_h) {
		val = (in_h - 1) * 4096 / (out_h - 1);
		reg = stream->config->scale.vy_fac;
		rkvpss_write(dev, reg, val);

		val = (in_h / in_div - 1) * 4096 / (out_h / out_div - 1);
		reg = stream->config->scale.vc_fac;
		rkvpss_write(dev, reg, val);

		ctrl |= RKVPSS_SCL_VY_EN | RKVPSS_SCL_VC_EN;
	}
	reg = stream->config->scale.ctrl;
	rkvpss_write(dev, reg, ctrl);

	val = RKVPSS_SCL_GEN_UPD;
	if (sync)
		val |= RKVPSS_SCL_FORCE_UPD;
	reg = stream->config->scale.update;
	rkvpss_write(dev, reg, val);
}

static int rkvpss_stream_scale(struct rkvpss_stream *stream, bool on, bool sync)
{
	if (stream->id == RKVPSS_OUTPUT_CH0)
		poly_phase_scale(stream, on, sync);
	else
		bilinear_scale(stream, on, sync);
	return 0;
}

static void rkvpss_stream_stop(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	int ret;

	stream->stopping = true;
	ret = wait_event_timeout(stream->done, !stream->streaming,
				 msecs_to_jiffies(300));
	if (!ret)
		v4l2_warn(&dev->v4l2_dev, "%s id:%d timeout\n", __func__, stream->id);
	stream->stopping = false;
	stream->streaming = false;
	if (stream->ops->disable_mi)
		stream->ops->disable_mi(stream);
	rkvpss_stream_crop(stream, false, true);
	rkvpss_stream_scale(stream, false, true);
}

static void rkvpss_stop_streaming(struct vb2_queue *queue)
{
	struct rkvpss_stream *stream = queue->drv_priv;
	struct rkvpss_vdev_node *node = &stream->vnode;
	struct rkvpss_device *dev = stream->dev;
	struct rkvpss_hw_dev *hw = dev->hw_dev;

	if (!stream->streaming)
		return;

	mutex_lock(&hw->dev_lock);
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s %s id:%d enter\n", __func__,
		 node->vdev.name, stream->id);
	rkvpss_stream_stop(stream);
	rkvpss_pipeline_stream(dev, false);
	destroy_buf_queue(stream, VB2_BUF_STATE_ERROR);
	rkvpss_pipeline_close(dev);
	tasklet_disable(&stream->buf_done_tasklet);
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s %s id:%d exit\n", __func__,
		 node->vdev.name, stream->id);
	mutex_unlock(&hw->dev_lock);
}

static int rkvpss_stream_start(struct rkvpss_stream *stream)
{
	int ret = 0;

	stream->is_crop_upd = true;
	ret = rkvpss_stream_scale(stream, true, true);
	if (ret < 0)
		return ret;
	ret = rkvpss_stream_crop(stream, true, true);
	if (ret < 0)
		return ret;

	if (stream->ops->config_mi)
		stream->ops->config_mi(stream);

	stream->streaming = true;
	return ret;
}

static int rkvpss_start_streaming(struct vb2_queue *queue, unsigned int count)
{
	struct rkvpss_stream *stream = queue->drv_priv;
	struct rkvpss_vdev_node *node = &stream->vnode;
	struct rkvpss_device *dev = stream->dev;
	struct rkvpss_hw_dev *hw = dev->hw_dev;
	int ret = -EINVAL;

	if (stream->streaming)
		return -EBUSY;

	mutex_lock(&hw->dev_lock);
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s %s id:%d enter\n", __func__,
		 node->vdev.name, stream->id);

	stream->is_pause = true;
	if (!dev->inp || !stream->linked) {
		v4l2_err(&dev->v4l2_dev, "no link or invalid input source\n");
		goto free_buf_queue;
	}

	rkvpss_pipeline_open(dev);

	ret = rkvpss_stream_start(stream);
	if (ret < 0) {
		v4l2_err(&dev->v4l2_dev, "start %s failed\n", node->vdev.name);
		goto pipe_close;
	}

	ret = rkvpss_pipeline_stream(dev, true);
	if (ret < 0)
		goto stop_stream;

	tasklet_enable(&stream->buf_done_tasklet);
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "%s %s id:%d exit\n", __func__,
		 node->vdev.name, stream->id);
	mutex_unlock(&hw->dev_lock);
	return 0;
stop_stream:
	stream->streaming = false;
	rkvpss_stream_stop(stream);
pipe_close:
	rkvpss_pipeline_close(dev);
free_buf_queue:
	destroy_buf_queue(stream, VB2_BUF_STATE_QUEUED);
	mutex_unlock(&hw->dev_lock);
	return ret;
}

static const struct vb2_ops stream_vb2_ops = {
	.queue_setup = rkvpss_queue_setup,
	.buf_queue = rkvpss_buf_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.stop_streaming = rkvpss_stop_streaming,
	.start_streaming = rkvpss_start_streaming,
};

static int rkvpss_init_vb2_queue(struct vb2_queue *q,
				 struct rkvpss_stream *stream,
				 enum v4l2_buf_type buf_type)
{
	q->type = buf_type;
	q->io_modes = VB2_MMAP | VB2_DMABUF | VB2_USERPTR;
	q->drv_priv = stream;
	q->ops = &stream_vb2_ops;
	q->mem_ops = stream->dev->hw_dev->mem_ops;
	q->buf_struct_size = sizeof(struct rkvpss_buffer);
	q->min_buffers_needed = STREAM_OUT_REQ_BUFS_MIN;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	q->lock = &stream->dev->apilock;
	q->dev = stream->dev->hw_dev->dev;
	q->allow_cache_hints = 1;
	q->bidirectional = 1;
	if (stream->dev->hw_dev->is_dma_contig)
		q->dma_attrs = DMA_ATTR_FORCE_CONTIGUOUS;
	q->gfp_flags = GFP_DMA32;
	return vb2_queue_init(q);
}

static const
struct capture_fmt *find_fmt(struct rkvpss_stream *stream, u32 pixelfmt)
{
	const struct capture_fmt *fmt;
	u32 i;

	for (i = 0; i < stream->config->fmt_size; i++) {
		fmt = &stream->config->fmts[i];
		if (fmt->fourcc == pixelfmt)
			return fmt;
	}
	return NULL;
}

static int fcc_xysubs(u32 fcc, u32 *xsubs, u32 *ysubs)
{
	switch (fcc) {
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_YUV444M:
		*xsubs = 1;
		*ysubs = 1;
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_FBC2:
		*xsubs = 2;
		*ysubs = 1;
		break;
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_FBC0:
		*xsubs = 2;
		*ysubs = 2;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int rkvpss_set_fmt(struct rkvpss_stream *stream,
			  struct v4l2_pix_format_mplane *pixm,
			  bool try)
{
	struct rkvpss_device *dev = stream->dev;
	u32 i, planes, xsubs = 1, ysubs = 1, imagsize = 0;
	const struct capture_fmt *fmt;

	fmt = find_fmt(stream, pixm->pixelformat);
	if (!fmt) {
		v4l2_err(&dev->v4l2_dev,
			 "nonsupport pixelformat:%c%c%c%c\n",
			 pixm->pixelformat,
			 pixm->pixelformat >> 8,
			 pixm->pixelformat >> 16,
			 pixm->pixelformat >> 24);
		return -EINVAL;
	}

	pixm->num_planes = fmt->mplanes;
	pixm->field = V4L2_FIELD_NONE;
	if (!pixm->quantization)
		pixm->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fcc_xysubs(fmt->fourcc, &xsubs, &ysubs);
	planes = fmt->cplanes ? fmt->cplanes : fmt->mplanes;
	for (i = 0; i < planes; i++) {
		struct v4l2_plane_pix_format *plane_fmt;
		u32 width, height, bytesperline, w, h;

		plane_fmt = pixm->plane_fmt + i;
		w = pixm->width;
		h = pixm->height;
		width = i ? w / xsubs : w;
		height = i ? h / ysubs : h;
		bytesperline = width * DIV_ROUND_UP(fmt->bpp[i], 8);
		if (i != 0 || plane_fmt->bytesperline < bytesperline)
			plane_fmt->bytesperline = bytesperline;
		plane_fmt->sizeimage = plane_fmt->bytesperline * height;
		imagsize += plane_fmt->sizeimage;
	}
	if (fmt->mplanes == 1)
		pixm->plane_fmt[0].sizeimage = imagsize;
	if (!try) {
		stream->out_cap_fmt = *fmt;
		stream->out_fmt = *pixm;
		v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
			 "%s: stream:%d req(%d, %d) out(%d, %d)\n",
			 __func__, stream->id, pixm->width, pixm->height,
			 stream->out_fmt.width, stream->out_fmt.height);
	}
	return 0;
}

/************************* v4l2_file_operations***************************/

static int rkvpss_fh_open(struct file *file)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	struct rkvpss_device *dev = stream->dev;
	int ret;

	if (!dev->is_probe_end)
		return -EINVAL;

	ret = v4l2_fh_open(file);
	if (!ret) {
		ret = v4l2_pipeline_pm_get(&stream->vnode.vdev.entity);
		if (ret < 0) {
			v4l2_err(&dev->v4l2_dev,
				 "pipeline power on failed %d\n", ret);
			vb2_fop_release(file);
		}
	}
	return ret;
}

static int rkvpss_fh_release(struct file *file)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	int ret;

	ret = vb2_fop_release(file);
	if (!ret)
		v4l2_pipeline_pm_put(&stream->vnode.vdev.entity);
	return ret;
}

static const struct v4l2_file_operations rkvpss_fops = {
	.open = rkvpss_fh_open,
	.release = rkvpss_fh_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

static int rkvpss_enum_input(struct file *file, void *priv,
			     struct v4l2_input *input)
{
	if (input->index > 0)
		return -EINVAL;

	input->type = V4L2_INPUT_TYPE_CAMERA;
	strscpy(input->name, "Camera", sizeof(input->name));

	return 0;
}

static int rkvpss_try_fmt_vid_mplane(struct file *file, void *fh,
				     struct v4l2_format *f)
{
	struct rkvpss_stream *stream = video_drvdata(file);

	return rkvpss_set_fmt(stream, &f->fmt.pix_mp, true);
}

static int rkvpss_enum_fmt_vid_mplane(struct file *file, void *priv,
				      struct v4l2_fmtdesc *f)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	const struct capture_fmt *fmt = NULL;

	if (f->index >= stream->config->fmt_size)
		return -EINVAL;

	fmt = &stream->config->fmts[f->index];
	f->pixelformat = fmt->fourcc;

	return 0;
}

static int rkvpss_s_fmt_vid_mplane(struct file *file,
				   void *priv, struct v4l2_format *f)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	struct video_device *vdev = &stream->vnode.vdev;
	struct rkvpss_vdev_node *node = vdev_to_node(vdev);
	struct rkvpss_device *dev = stream->dev;

	/* Change not allowed if queue is streaming. */
	if (vb2_is_streaming(&node->buf_queue)) {
		v4l2_err(&dev->v4l2_dev, "%s queue busy\n", __func__);
		return -EBUSY;
	}

	return rkvpss_set_fmt(stream, &f->fmt.pix_mp, false);
}

static int rkvpss_g_fmt_vid_mplane(struct file *file, void *fh,
				       struct v4l2_format *f)
{
	struct rkvpss_stream *stream = video_drvdata(file);

	f->fmt.pix_mp = stream->out_fmt;

	return 0;
}

static int rkvpss_g_selection(struct file *file, void *prv,
			      struct v4l2_selection *sel)
{
	struct rkvpss_stream *stream = video_drvdata(file);

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = stream->crop;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int rkvpss_s_selection(struct file *file, void *prv,
			      struct v4l2_selection *sel)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	struct rkvpss_device *dev = stream->dev;
	struct v4l2_rect *crop = &stream->crop;
	u32 max_w = dev->vpss_sdev.in_fmt.width;
	u32 max_h = dev->vpss_sdev.in_fmt.height;

	if (sel->target != V4L2_SEL_TGT_CROP)
		return -EINVAL;
	if (sel->flags != 0)
		return -EINVAL;

	sel->r.left = ALIGN(sel->r.left, 2);
	sel->r.top = ALIGN(sel->r.top, 2);
	sel->r.width = ALIGN(sel->r.width, 2);
	sel->r.height = ALIGN(sel->r.height, 2);
	if (!sel->r.width || !sel->r.height ||
	    sel->r.width + sel->r.left > max_w ||
	    sel->r.height + sel->r.top > max_h) {
		v4l2_err(&dev->v4l2_dev,
			 "invalid crop left:%d top:%d w:%d h:%d for %dx%d\n",
			 sel->r.left, sel->r.top, sel->r.width, sel->r.height, max_w, max_h);
		return -EINVAL;
	}
	*crop = sel->r;
	stream->is_crop_upd = true;
	v4l2_dbg(1, rkvpss_debug, &dev->v4l2_dev,
		 "stream %d crop(%d,%d)/%dx%d\n", stream->id,
		 crop->left, crop->top, crop->width, crop->height);
	return 0;
}

static int rkvpss_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	struct device *dev = stream->dev->dev;
	struct video_device *vdev = video_devdata(file);

	strscpy(cap->card, vdev->name, sizeof(cap->card));
	snprintf(cap->driver, sizeof(cap->driver),
		 "%s_v%d", dev->driver->name,
		 stream->dev->vpss_ver >> 4);
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", dev_name(dev));

	return 0;
}

static void rkvpss_stream_mf(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	u32 val, mask;

	if (!stream->is_mf_upd)
		return;
	stream->is_mf_upd = false;

	if (!dev->hw_dev->is_ofl_cmsc) {
		mask = RKVPSS_MIR_EN;
		val = dev->mir_en ? RKVPSS_MIR_EN : 0;
		rkvpss_set_bits(dev, RKVPSS_VPSS_CTRL, mask, val);
	}
	mask = RKVPSS_MI_CHN_V_FLIP(stream->id);
	val = stream->flip_en ? mask : 0;
	rkvpss_set_bits(dev, RKVPSS_MI_WR_VFLIP_CTRL, mask, val);
}

static int rkvpss_get_mirror_flip(struct rkvpss_stream *stream,
				  struct rkvpss_mirror_flip *cfg)
{
	cfg->mirror = stream->dev->mir_en;
	cfg->flip = stream->flip_en;
	return 0;
}

static int rkvpss_set_mirror_flip(struct rkvpss_stream *stream,
				  struct rkvpss_mirror_flip *cfg)
{
	struct rkvpss_device *dev = stream->dev;

	if (dev->hw_dev->is_ofl_cmsc && cfg->mirror) {
		cfg->mirror = 0;
		v4l2_warn(&stream->dev->v4l2_dev,
			  "mirror mux to vpss offline mode\n");
	}
	if (dev->mir_en != !!cfg->mirror ||
	    stream->flip_en != !!cfg->flip)
		stream->is_mf_upd = true;
	dev->mir_en = !!cfg->mirror;
	stream->flip_en = !!cfg->flip;
	return 0;
}

void rkvpss_cmsc_config(struct rkvpss_device *dev, bool sync)
{
	unsigned long lock_flags = 0;
	struct rkvpss_cmsc_cfg cfg;
	int i, j, k, slope, hor;
	u32 win_en, mode, val, ctrl = 0;

	spin_lock_irqsave(&dev->cmsc_lock, lock_flags);
	if (dev->hw_dev->is_ofl_cmsc || !dev->cmsc_upd) {
		spin_unlock_irqrestore(&dev->cmsc_lock, lock_flags);
		return;
	}
	dev->cmsc_upd = false;
	cfg = dev->cmsc_cfg;
	spin_unlock_irqrestore(&dev->cmsc_lock, lock_flags);

	for (i = 0; i < RKVPSS_OUTPUT_MAX; i++) {
		win_en = cfg.win[i].win_en;
		if (win_en)
			ctrl |= RKVPSS_CMSC_CHN_EN(i);
		rkvpss_write(dev, RKVPSS_CMSC_CHN0_WIN + i * 4, win_en);

		mode = cfg.win[i].mode;
		rkvpss_write(dev, RKVPSS_CMSC_CHN0_MODE + i * 4, mode);

		for (j = 0; j < RKVPSS_CMSC_WIN_MAX && win_en; j++) {
			if (!(win_en & BIT(j)))
				continue;
			for (k = 0; k < RKVPSS_CMSC_POINT_MAX; k++) {
				val = RKVPSS_CMSC_WIN_VTX(cfg.win[j].point[k].x,
							  cfg.win[j].point[k].y);
				rkvpss_write(dev, RKVPSS_CMSC_WIN0_L0_VTX + k * 8 + j * 32, val);

				rkvpss_cmsc_slop(&cfg.win[j].point[k],
						 (k + 1 == RKVPSS_CMSC_POINT_MAX) ?
						 &cfg.win[j].point[0] : &cfg.win[j].point[k + 1],
						 &slope, &hor);
				val = RKVPSS_CMSC_WIN_SLP(slope, hor);
				rkvpss_write(dev, RKVPSS_CMSC_WIN0_L0_SLP + k * 8 + j * 32, val);
			}

			if ((mode & BIT(j)))
				continue;
			val = RKVPSS_CMSK_WIN_YUV(cfg.win[j].cover_color_y,
						  cfg.win[j].cover_color_u,
						  cfg.win[j].cover_color_v);
			val |= RKVPSS_CMSC_WIN_ALPHA(cfg.win[j].cover_color_a);
			rkvpss_write(dev, RKVPSS_CMSC_WIN0_PARA + j * 4, val);
		}
	}

	if (ctrl) {
		ctrl |= RKVPSS_CMSC_EN;
		ctrl |= RKVPSS_CMSC_BLK_SZIE(cfg.mosaic_block);
	}
	rkvpss_write(dev, RKVPSS_CMSC_CTRL, ctrl);
	val = RKVPSS_CMSC_GEN_UPD;
	if (sync)
		val |= RKVPSS_CMSC_FORCE_UPD;
	rkvpss_write(dev, RKVPSS_CMSC_UPDATE, val);
}

static int rkvpss_get_cmsc(struct rkvpss_stream *stream, struct rkvpss_cmsc_cfg *cfg)
{
	struct rkvpss_device *dev = stream->dev;
	unsigned long lock_flags = 0;
	u32 i, win_en, mode;

	spin_lock_irqsave(&dev->cmsc_lock, lock_flags);
	*cfg = dev->cmsc_cfg;
	spin_unlock_irqrestore(&dev->cmsc_lock, lock_flags);

	win_en = cfg->win[stream->id].win_en;
	mode = cfg->win[stream->id].mode;
	for (i = 0; i < RKVPSS_CMSC_WIN_MAX; i++) {
		cfg->win[i].win_en = !!(win_en & BIT(i));
		cfg->win[i].mode = !!(mode & BIT(i));
	}
	cfg->width_ro = dev->vpss_sdev.in_fmt.width;
	cfg->height_ro = dev->vpss_sdev.in_fmt.height;
	return 0;
}

static int rkvpss_set_cmsc(struct rkvpss_stream *stream, struct rkvpss_cmsc_cfg *cfg)
{
	struct rkvpss_device *dev = stream->dev;
	unsigned long lock_flags = 0;
	u16 i, j, k, win_en = 0, mode = 0;
	int ret = 0;

	if (dev->hw_dev->is_ofl_cmsc)
		return -EINVAL;

	spin_lock_irqsave(&dev->cmsc_lock, lock_flags);
	for (i = 0; i < RKVPSS_CMSC_WIN_MAX; i++) {
		if (!cfg->win[i].win_en)
			continue;

		win_en |= BIT(i);
		mode |= cfg->win[i].mode ? BIT(i) : 0;

		for (j = 0; j < RKVPSS_CMSC_POINT_MAX; j++) {
			for (k = j + 1; k < RKVPSS_CMSC_POINT_MAX; k++) {
				if (cfg->win[i].point[j].x == cfg->win[i].point[k].x &&
				    cfg->win[i].point[j].y == cfg->win[i].point[k].y) {
					v4l2_warn(&dev->v4l2_dev,
						  "points should different, P%d(%d,%d) P%d(%d,%d)\n",
						  j, cfg->win[i].point[j].x, cfg->win[i].point[j].y,
						  k, cfg->win[i].point[k].x, cfg->win[i].point[k].y);
					ret = -EINVAL;
					goto unlock;
				}
			}
		}
		if (!cfg->win[i].mode) {
			dev->cmsc_cfg.win[i].cover_color_y = cfg->win[i].cover_color_y;
			dev->cmsc_cfg.win[i].cover_color_u = cfg->win[i].cover_color_u;
			dev->cmsc_cfg.win[i].cover_color_v = cfg->win[i].cover_color_v;
			dev->cmsc_cfg.win[i].cover_color_a = cfg->win[i].cover_color_a;
			if (dev->cmsc_cfg.win[i].cover_color_a > 15)
				dev->cmsc_cfg.win[i].cover_color_a = 15;
		}
		for (j = 0; j < RKVPSS_CMSC_POINT_MAX; j++)
			dev->cmsc_cfg.win[i].point[j] = cfg->win[i].point[j];
	}
	dev->cmsc_cfg.win[stream->id].win_en = win_en;
	dev->cmsc_cfg.win[stream->id].mode = mode;
	dev->cmsc_cfg.mosaic_block = cfg->mosaic_block;
	dev->cmsc_upd = true;
unlock:
	spin_unlock_irqrestore(&dev->cmsc_lock, lock_flags);
	return ret;
}

static long rkvpss_ioctl_default(struct file *file, void *fh,
				 bool valid_prio, unsigned int cmd, void *arg)
{
	struct rkvpss_stream *stream = video_drvdata(file);
	long ret = 0;

	if (!arg)
		return -EINVAL;

	switch (cmd) {
	case RKVPSS_CMD_GET_MIRROR_FLIP:
		ret = rkvpss_get_mirror_flip(stream, arg);
		break;
	case RKVPSS_CMD_SET_MIRROR_FLIP:
		ret = rkvpss_set_mirror_flip(stream, arg);
		break;
	case RKVPSS_CMD_GET_CMSC:
		ret = rkvpss_get_cmsc(stream, arg);
		break;
	case RKVPSS_CMD_SET_CMSC:
		ret = rkvpss_set_cmsc(stream, arg);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct v4l2_ioctl_ops rkvpss_v4l2_ioctl_ops = {
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_enum_input = rkvpss_enum_input,
	.vidioc_try_fmt_vid_cap_mplane = rkvpss_try_fmt_vid_mplane,
	.vidioc_enum_fmt_vid_cap = rkvpss_enum_fmt_vid_mplane,
	.vidioc_s_fmt_vid_cap_mplane = rkvpss_s_fmt_vid_mplane,
	.vidioc_g_fmt_vid_cap_mplane = rkvpss_g_fmt_vid_mplane,
	.vidioc_s_selection = rkvpss_s_selection,
	.vidioc_g_selection = rkvpss_g_selection,
	.vidioc_querycap = rkvpss_querycap,
	.vidioc_default = rkvpss_ioctl_default,
};

static void rkvpss_unregister_stream_video(struct rkvpss_stream *stream)
{
	tasklet_kill(&stream->buf_done_tasklet);
	media_entity_cleanup(&stream->vnode.vdev.entity);
	video_unregister_device(&stream->vnode.vdev);
}

static int rkvpss_register_stream_video(struct rkvpss_stream *stream)
{
	struct rkvpss_device *dev = stream->dev;
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	struct video_device *vdev = &stream->vnode.vdev;
	struct rkvpss_vdev_node *node;
	enum v4l2_buf_type buf_type;
	int ret = 0;

	node = vdev_to_node(vdev);
	vdev->release = video_device_release_empty;
	vdev->fops = &rkvpss_fops;
	vdev->minor = -1;
	vdev->v4l2_dev = v4l2_dev;
	vdev->lock = &dev->apilock;
	video_set_drvdata(vdev, stream);

	vdev->ioctl_ops = &rkvpss_v4l2_ioctl_ops;
	vdev->device_caps = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE_MPLANE;
	vdev->vfl_dir = VFL_DIR_RX;
	node->pad.flags = MEDIA_PAD_FL_SINK;
	buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	rkvpss_init_vb2_queue(&node->buf_queue, stream, buf_type);
	vdev->queue = &node->buf_queue;

	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		v4l2_err(v4l2_dev,
			 "video register failed with error %d\n", ret);
		return ret;
	}

	ret = media_entity_pads_init(&vdev->entity, 1, &node->pad);
	if (ret < 0)
		goto unreg;

	INIT_LIST_HEAD(&stream->buf_done_list);
	tasklet_init(&stream->buf_done_tasklet,
		     rkvpss_buf_done_task, (unsigned long)stream);
	tasklet_disable(&stream->buf_done_tasklet);
	return 0;
unreg:
	video_unregister_device(vdev);
	return ret;
}

void rkvpss_stream_default_fmt(struct rkvpss_device *dev, u32 id,
			       u32 width, u32 height, u32 pixelformat)
{
	struct rkvpss_stream *stream = &dev->stream_vdev.stream[id];
	struct v4l2_pix_format_mplane pixm = { 0 };

	if (pixelformat)
		pixm.pixelformat = pixelformat;
	else
		pixm.pixelformat = stream->out_cap_fmt.fourcc;
	if (!pixm.pixelformat)
		return;

	stream->crop.left = 0;
	stream->crop.top = 0;
	stream->crop.width = width;
	stream->crop.height = height;

	pixm.width = width;
	pixm.height = height;
	rkvpss_set_fmt(stream, &pixm, false);
}

int rkvpss_register_stream_vdevs(struct rkvpss_device *dev)
{
	struct rkvpss_stream_vdev *stream_vdev;
	struct rkvpss_stream *stream;
	struct video_device *vdev;
	char *vdev_name;
	int i, j, ret = 0;

	stream_vdev = &dev->stream_vdev;
	memset(stream_vdev, 0, sizeof(*stream_vdev));

	for (i = 0; i < RKVPSS_OUTPUT_MAX; i++) {
		stream = &stream_vdev->stream[i];
		stream->id = i;
		stream->dev = dev;
		vdev = &stream->vnode.vdev;
		INIT_LIST_HEAD(&stream->buf_queue);
		init_waitqueue_head(&stream->done);
		spin_lock_init(&stream->vbq_lock);
		switch (i) {
		case RKVPSS_OUTPUT_CH0:
			vdev_name = S0_VDEV_NAME;
			stream->ops = &scl_stream_ops;
			stream->config = &scl0_config;
			break;
		case RKVPSS_OUTPUT_CH1:
			vdev_name = S1_VDEV_NAME;
			stream->ops = &scl_stream_ops;
			stream->config = &scl1_config;
			break;
		case RKVPSS_OUTPUT_CH2:
			vdev_name = S2_VDEV_NAME;
			stream->ops = &scl_stream_ops;
			stream->config = &scl2_config;
			break;
		case RKVPSS_OUTPUT_CH3:
			vdev_name = S3_VDEV_NAME;
			stream->ops = &scl_stream_ops;
			stream->config = &scl3_config;
			break;
		default:
			v4l2_err(&dev->v4l2_dev, "Invalid stream:%d\n", i);
			return -EINVAL;
		}
		strscpy(vdev->name, vdev_name, sizeof(vdev->name));
		ret = rkvpss_register_stream_video(stream);
		if (ret < 0)
			goto err;
	}
	return 0;
err:
	for (j = 0; j < i; j++) {
		stream = &stream_vdev->stream[j];
		rkvpss_unregister_stream_video(stream);
	}
	return ret;
}

void rkvpss_unregister_stream_vdevs(struct rkvpss_device *dev)
{
	struct rkvpss_stream_vdev *stream_vdev;
	struct rkvpss_stream *stream;
	int i;

	stream_vdev = &dev->stream_vdev;
	for (i = 0; i < RKVPSS_OUTPUT_MAX; i++) {
		stream = &stream_vdev->stream[i];
		rkvpss_unregister_stream_video(stream);
	}
}

void rkvpss_isr(struct rkvpss_device *dev, u32 mis_val)
{
	v4l2_dbg(3, rkvpss_debug, &dev->v4l2_dev,
		 "isr:0x%x\n", mis_val);
	if (mis_val & RKVPSS_ISP_ALL_FRM_END && dev->remote_sd)
		v4l2_subdev_call(dev->remote_sd, core, ioctl, RKISP_VPSS_CMD_EOF, NULL);
}

void rkvpss_mi_isr(struct rkvpss_device *dev, u32 mis_val)
{
	struct rkvpss_stream *stream;
	int i, ris = readl(dev->hw_dev->base_addr + RKVPSS_MI_RIS);

	v4l2_dbg(3, rkvpss_debug, &dev->v4l2_dev,
		 "mi isr:0x%x, ris:0x%x\n", mis_val, ris);
	if (mis_val & RKVPSS_MI_BUS_ERR)
		v4l2_warn(&dev->v4l2_dev, "axi bus error\n");

	for (i = 0; i < RKVPSS_OUTPUT_MAX; i++) {
		stream = &dev->stream_vdev.stream[i];

		if (!stream->streaming ||
		    !(ris & stream->config->frame_end_id))
			continue;
		writel(stream->config->frame_end_id, dev->hw_dev->base_addr + RKVPSS_MI_ICR);
		if (stream->stopping) {
			stream->stopping = false;
			stream->streaming = false;
			stream->ops->disable_mi(stream);
			wake_up(&stream->done);
		} else {
			rkvpss_frame_end(stream);
		}
	}
}
