// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-common.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-dma-sg.h>
#include "dev.h"
#include "regs.h"

/*			ISP39
 *        /M|--->|-->mainpath------------------->ddr
 *output->|U|    |-->selfpath------------------->ddr
 *        \X|------->ldc------------------------>ddr
 */

#define CIF_ISP_REQ_BUFS_MIN 0

static int mi_frame_end(struct rkisp_stream *stream, u32 state);
static int mi_frame_start(struct rkisp_stream *stream, u32 irq);
static int isp_frame_end(struct rkisp_stream *stream, u32 irq);

static const struct capture_fmt mp_fmts[] = {
	/* yuv422 */
	{
		.fourcc = V4L2_PIX_FMT_UYVY,
		.fmt_type = FMT_YUV,
		.bpp = { 16 },
		.cplanes = 1,
		.mplanes = 1,
		.uv_swap = 0,
		.write_format = MI_CTRL_MP_WRITE_YUVINT,
		.output_format = ISP32_MI_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV16,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 0,
		.write_format = MI_CTRL_MP_WRITE_YUV_SPLA,
		.output_format = ISP32_MI_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV61,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 1,
		.write_format = MI_CTRL_MP_WRITE_YUV_SPLA,
		.output_format = ISP32_MI_OUTPUT_YUV422,
	},
	/* yuv420 */
	{
		.fourcc = V4L2_PIX_FMT_NV21,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 1,
		.write_format = MI_CTRL_MP_WRITE_YUV_SPLA,
		.output_format = ISP32_MI_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_NV12,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 0,
		.write_format = MI_CTRL_MP_WRITE_YUV_SPLA,
		.output_format = ISP32_MI_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_NV21M,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 2,
		.uv_swap = 1,
		.write_format = MI_CTRL_MP_WRITE_YUV_SPLA,
		.output_format = ISP32_MI_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_NV12M,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 2,
		.uv_swap = 0,
		.write_format = MI_CTRL_MP_WRITE_YUV_SPLA,
		.output_format = ISP32_MI_OUTPUT_YUV420,
	},
};

static const struct capture_fmt sp_fmts[] = {
	/* yuv422 */
	{
		.fourcc = V4L2_PIX_FMT_UYVY,
		.fmt_type = FMT_YUV,
		.bpp = { 16 },
		.cplanes = 1,
		.mplanes = 1,
		.uv_swap = 0,
		.write_format = MI_CTRL_SP_WRITE_INT,
		.output_format = MI_CTRL_SP_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV16,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 0,
		.write_format = MI_CTRL_SP_WRITE_SPLA,
		.output_format = MI_CTRL_SP_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV61,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 1,
		.write_format = MI_CTRL_SP_WRITE_SPLA,
		.output_format = MI_CTRL_SP_OUTPUT_YUV422,
	},
	/* yuv420 */
	{
		.fourcc = V4L2_PIX_FMT_NV21,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 1,
		.write_format = MI_CTRL_SP_WRITE_SPLA,
		.output_format = MI_CTRL_SP_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_NV12,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 0,
		.write_format = MI_CTRL_SP_WRITE_SPLA,
		.output_format = MI_CTRL_SP_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_NV21M,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 2,
		.uv_swap = 1,
		.write_format = MI_CTRL_SP_WRITE_SPLA,
		.output_format = MI_CTRL_SP_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_NV12M,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 2,
		.uv_swap = 0,
		.write_format = MI_CTRL_SP_WRITE_SPLA,
		.output_format = MI_CTRL_SP_OUTPUT_YUV420,
	},
	/* yuv400 */
	{
		.fourcc = V4L2_PIX_FMT_GREY,
		.fmt_type = FMT_YUV,
		.bpp = { 8 },
		.cplanes = 1,
		.mplanes = 1,
		.uv_swap = 0,
		.write_format = MI_CTRL_SP_WRITE_PLA,
		.output_format = MI_CTRL_SP_OUTPUT_YUV400,
	},
	/* rgb */
	{
		.fourcc = V4L2_PIX_FMT_XBGR32,
		.fmt_type = FMT_RGB,
		.bpp = { 32 },
		.mplanes = 1,
		.write_format = MI_CTRL_SP_WRITE_PLA,
		.output_format = MI_CTRL_SP_OUTPUT_ARGB888,
	}, {
		.fourcc = V4L2_PIX_FMT_RGB565,
		.fmt_type = FMT_RGB,
		.bpp = { 16 },
		.mplanes = 1,
		.write_format = MI_CTRL_SP_WRITE_PLA,
		.output_format = MI_CTRL_SP_OUTPUT_RGB565,
	}, {
		.fourcc = V4L2_PIX_FMT_RGB24,
		.fmt_type = FMT_RGB,
		.bpp = { 24 },
		.mplanes = 1,
		.write_format = MI_CTRL_SP_WRITE_PLA,
		.output_format = MI_CTRL_SP_OUTPUT_RGB888,
	},
};

static const struct capture_fmt ldc_fmts[] = {
	/* yuv422 */
	{
		.fourcc = V4L2_PIX_FMT_UYVY,
		.fmt_type = FMT_YUV,
		.bpp = { 16 },
		.cplanes = 1,
		.mplanes = 1,
		.uv_swap = 0,
		.output_format = ISP39_LDCV_OUTPUT_YUYV,
	}, {
		.fourcc = V4L2_PIX_FMT_NV16,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 0,
		.output_format = ISP39_LDCV_OUTPUT_YUV422,
	}, {
		.fourcc = V4L2_PIX_FMT_NV61,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 1,
		.output_format = ISP39_LDCV_OUTPUT_YUV422,
	},
	/* yuv420 */
	{
		.fourcc = V4L2_PIX_FMT_NV21,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 1,
		.output_format = ISP39_LDCV_OUTPUT_YUV420,
	}, {
		.fourcc = V4L2_PIX_FMT_NV12,
		.fmt_type = FMT_YUV,
		.bpp = { 8, 16 },
		.cplanes = 2,
		.mplanes = 1,
		.uv_swap = 0,
		.output_format = ISP39_LDCV_OUTPUT_YUV420,
	},
	/* yuv400 */
	{
		.fourcc = V4L2_PIX_FMT_GREY,
		.fmt_type = FMT_YUV,
		.bpp = { 8 },
		.cplanes = 1,
		.mplanes = 1,
		.uv_swap = 0,
		.output_format = ISP39_LDCV_OUTPUT_YUV400,
	}
};

static struct stream_config rkisp_mp_stream_cfg = {
	.fmts = mp_fmts,
	.fmt_size = ARRAY_SIZE(mp_fmts),
	.max_rsz_width = CIF_ISP_INPUT_W_MAX_V39,
	.max_rsz_height = CIF_ISP_INPUT_H_MAX_V39,
	.min_rsz_width = STREAM_MIN_RSZ_OUTPUT_WIDTH,
	.min_rsz_height = STREAM_MIN_RSZ_OUTPUT_HEIGHT,
	.frame_end_id = CIF_MI_MP_FRAME,
	/* registers */
	.rsz = {
		.ctrl = ISP39_MAIN_SCALE_CTRL,
		.update = ISP39_MAIN_SCALE_UPDATE,
		.src_size = ISP39_MAIN_SCALE_SRC_SIZE,
		.dst_size = ISP39_MAIN_SCALE_DST_SIZE,
		.scale_hy_offs_mi = ISP39_MAIN_SCALE_HY_OFFS_MI,
		.scale_hc_offs_mi = ISP39_MAIN_SCALE_HC_OFFS_MI,
		.scale_in_crop_offs = ISP39_MAIN_SCALE_IN_CROP_OFFSET,
		.scale_hy_offs = ISP39_MAIN_SCALE_HY_OFFS,
		.scale_hc_offs = ISP39_MAIN_SCALE_HC_OFFS,
		.scale_hy_size = ISP39_MAIN_SCALE_HY_SIZE,
		.scale_hc_size = ISP39_MAIN_SCALE_HC_SIZE,
		.scale_hy = ISP39_MAIN_SCALE_HY_FAC,
		.scale_hcr = ISP39_MAIN_SCALE_HC_FAC,
		.scale_vy = ISP39_MAIN_SCALE_VY_FAC,
		.scale_vc = ISP39_MAIN_SCALE_VC_FAC,
		.scale_hy_shd = ISP39_MAIN_SCALE_HY_FAC_SHD,
		.scale_hcr_shd = ISP39_MAIN_SCALE_HC_FAC_SHD,
		.scale_vy_shd = ISP39_MAIN_SCALE_VY_FAC_SHD,
		.scale_vc_shd = ISP39_MAIN_SCALE_VC_FAC_SHD,
		.phase_hy = ISP39_MAIN_SCALE_PHASE_HY,
		.phase_hc = ISP39_MAIN_SCALE_PHASE_HC,
		.phase_vy = ISP39_MAIN_SCALE_PHASE_VY,
		.phase_vc = ISP39_MAIN_SCALE_PHASE_VC,
		.ctrl_shd = ISP39_MAIN_SCALE_CTRL_SHD,
		.phase_hy_shd = ISP39_MAIN_SCALE_PHASE_HY_SHD,
		.phase_hc_shd = ISP39_MAIN_SCALE_PHASE_HC_SHD,
		.phase_vy_shd = ISP39_MAIN_SCALE_PHASE_VY_SHD,
		.phase_vc_shd = ISP39_MAIN_SCALE_PHASE_VC_SHD,
	},
	.dual_crop = {
		.ctrl = CIF_DUAL_CROP_CTRL,
		.yuvmode_mask = CIF_DUAL_CROP_MP_MODE_YUV,
		.rawmode_mask = CIF_DUAL_CROP_MP_MODE_RAW,
		.h_offset = CIF_DUAL_CROP_M_H_OFFS,
		.v_offset = CIF_DUAL_CROP_M_V_OFFS,
		.h_size = CIF_DUAL_CROP_M_H_SIZE,
		.v_size = CIF_DUAL_CROP_M_V_SIZE,
	},
	.mi = {
		.y_size_init = CIF_MI_MP_Y_SIZE_INIT,
		.cb_size_init = CIF_MI_MP_CB_SIZE_INIT,
		.cr_size_init = CIF_MI_MP_CR_SIZE_INIT,
		.y_base_ad_init = CIF_MI_MP_Y_BASE_AD_INIT,
		.cb_base_ad_init = CIF_MI_MP_CB_BASE_AD_INIT,
		.cr_base_ad_init = CIF_MI_MP_CR_BASE_AD_INIT,
		.y_offs_cnt_init = CIF_MI_MP_Y_OFFS_CNT_INIT,
		.cb_offs_cnt_init = CIF_MI_MP_CB_OFFS_CNT_INIT,
		.cr_offs_cnt_init = CIF_MI_MP_CR_OFFS_CNT_INIT,
		.y_base_ad_shd = CIF_MI_MP_Y_BASE_AD_SHD,
		.y_pic_size = ISP3X_MI_MP_WR_Y_PIC_SIZE,
	},
};

static struct stream_config rkisp_sp_stream_cfg = {
	.fmts = sp_fmts,
	.fmt_size = ARRAY_SIZE(sp_fmts),
	.max_rsz_width = STREAM_MAX_SP_RSZ_OUTPUT_WIDTH,
	.max_rsz_height = STREAM_MAX_SP_RSZ_OUTPUT_HEIGHT,
	.min_rsz_width = STREAM_MIN_RSZ_OUTPUT_WIDTH,
	.min_rsz_height = STREAM_MIN_RSZ_OUTPUT_HEIGHT,
	.frame_end_id = CIF_MI_SP_FRAME,
	/* registers */
	.rsz = {
		.ctrl = ISP32_SELF_SCALE_CTRL,
		.update = ISP32_SELF_SCALE_UPDATE,
		.src_size = ISP32_SELF_SCALE_SRC_SIZE,
		.dst_size = ISP32_SELF_SCALE_DST_SIZE,
		.scale_hy_offs_mi = ISP32_SELF_SCALE_HY_OFFS_MI,
		.scale_hc_offs_mi = ISP32_SELF_SCALE_HC_OFFS_MI,
		.scale_in_crop_offs = ISP32_SELF_SCALE_IN_CROP_OFFSET,
		.scale_hy_offs = ISP32_SELF_SCALE_HY_OFFS,
		.scale_hc_offs = ISP32_SELF_SCALE_HC_OFFS,
		.scale_hy_size = ISP32_SELF_SCALE_HY_SIZE,
		.scale_hc_size = ISP32_SELF_SCALE_HC_SIZE,
		.scale_hy = ISP32_SELF_SCALE_HY_FAC,
		.scale_hcr = ISP32_SELF_SCALE_HC_FAC,
		.scale_vy = ISP32_SELF_SCALE_VY_FAC,
		.scale_vc = ISP32_SELF_SCALE_VC_FAC,
		.scale_hy_shd = ISP32_SELF_SCALE_HY_FAC_SHD,
		.scale_hcr_shd = ISP32_SELF_SCALE_HC_FAC_SHD,
		.scale_vy_shd = ISP32_SELF_SCALE_VY_FAC_SHD,
		.scale_vc_shd = ISP32_SELF_SCALE_VC_FAC_SHD,
		.phase_hy = ISP32_SELF_SCALE_PHASE_HY,
		.phase_hc = ISP32_SELF_SCALE_PHASE_HC,
		.phase_vy = ISP32_SELF_SCALE_PHASE_VY,
		.phase_vc = ISP32_SELF_SCALE_PHASE_VC,
		.ctrl_shd = ISP32_SELF_SCALE_CTRL_SHD,
		.phase_hy_shd = ISP32_SELF_SCALE_PHASE_HY_SHD,
		.phase_hc_shd = ISP32_SELF_SCALE_PHASE_HC_SHD,
		.phase_vy_shd = ISP32_SELF_SCALE_PHASE_VY_SHD,
		.phase_vc_shd = ISP32_SELF_SCALE_PHASE_VC_SHD,
	},
	.dual_crop = {
		.ctrl = CIF_DUAL_CROP_CTRL,
		.yuvmode_mask = CIF_DUAL_CROP_SP_MODE_YUV,
		.rawmode_mask = CIF_DUAL_CROP_SP_MODE_RAW,
		.h_offset = CIF_DUAL_CROP_S_H_OFFS,
		.v_offset = CIF_DUAL_CROP_S_V_OFFS,
		.h_size = CIF_DUAL_CROP_S_H_SIZE,
		.v_size = CIF_DUAL_CROP_S_V_SIZE,
	},
	.mi = {
		.y_size_init = CIF_MI_SP_Y_SIZE_INIT,
		.cb_size_init = CIF_MI_SP_CB_SIZE_INIT,
		.cr_size_init = CIF_MI_SP_CR_SIZE_INIT,
		.y_base_ad_init = CIF_MI_SP_Y_BASE_AD_INIT,
		.cb_base_ad_init = CIF_MI_SP_CB_BASE_AD_INIT,
		.cr_base_ad_init = CIF_MI_SP_CR_BASE_AD_INIT,
		.y_offs_cnt_init = CIF_MI_SP_Y_OFFS_CNT_INIT,
		.cb_offs_cnt_init = CIF_MI_SP_CB_OFFS_CNT_INIT,
		.cr_offs_cnt_init = CIF_MI_SP_CR_OFFS_CNT_INIT,
		.y_base_ad_shd = CIF_MI_SP_Y_BASE_AD_SHD,
		.y_pic_size = ISP3X_MI_SP_WR_Y_PIC_SIZE,
	},
};

static struct stream_config rkisp_ldc_stream_cfg = {
	.fmts = ldc_fmts,
	.fmt_size = ARRAY_SIZE(ldc_fmts),
	.mi = {
		.y_base_ad_init = ISP39_LDCV_WR_ADDR,
		.cb_base_ad_init = ISP39_LDCV_WR_C_ADDR,
		.y_base_ad_shd = ISP39_LDCV_WR_ADDR,
	},
};

static inline bool ldc_is_stream_stopped(struct rkisp_stream *stream)
{
	u32 en = ISP39_LDCV_EN_SHD;
	bool is_direct = true;

	if (!stream->ispdev->hw_dev->is_single) {
		is_direct = false;
		en = ISP39_LDCV_EN;
	}
	return !(rkisp_read(stream->ispdev, ISP39_LDCV_CTRL, is_direct) & en);
}

static void stream_self_update(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	u32 val, mask;

	if (stream->id == RKISP_STREAM_LDC) {
		rkisp_set_bits(dev, ISP39_LDCV_CTRL, 0, ISP39_LDCV_FORCE_UPD, true);
		rkisp_set_bits(dev, ISP3X_LDCH_STS, 0, ISP3X_LDCH_FORCE_UPD, true);
		return;
	}

	mask = ISP3X_MPSELF_UPD | ISP3X_SPSELF_UPD;
	switch (stream->id) {
	case RKISP_STREAM_MP:
		val = ISP3X_MPSELF_UPD;
		break;
	case RKISP_STREAM_SP:
		val = ISP3X_SPSELF_UPD;
		break;
	default:
		return;
	}

	rkisp_set_bits(stream->ispdev, ISP3X_MI_WR_CTRL2, mask, val, true);
}

static int get_stream_irq_mask(struct rkisp_stream *stream)
{
	int ret;

	switch (stream->id) {
	case RKISP_STREAM_SP:
		ret = ISP_FRAME_SP;
		break;
	case RKISP_STREAM_MP:
		ret = ISP_FRAME_MP;
		break;
	case RKISP_STREAM_LDC:
		ret = ISP_FRAME_LDC;
		break;
	default:
		ret = 0;
	}

	return ret;
}

/* configure dual-crop unit */
static int rkisp_stream_config_dcrop(struct rkisp_stream *stream, bool async)
{
	struct rkisp_device *dev = stream->ispdev;
	struct v4l2_rect *dcrop = &stream->dcrop;
	struct v4l2_rect *input_win;

	/* dual-crop unit get data from isp */
	input_win = rkisp_get_isp_sd_win(&dev->isp_sdev);

	if (dcrop->width == input_win->width &&
	    dcrop->height == input_win->height &&
	    dcrop->left == 0 && dcrop->top == 0 &&
	    dev->unite_div < ISP_UNITE_DIV2) {
		rkisp_disable_dcrop(stream, async);
		v4l2_dbg(1, rkisp_debug, &dev->v4l2_dev,
			 "stream %d crop disabled\n", stream->id);
		return 0;
	}

	v4l2_dbg(1, rkisp_debug, &dev->v4l2_dev,
		 "stream %d crop: %dx%d -> %dx%d\n", stream->id,
		 input_win->width, input_win->height,
		 dcrop->width, dcrop->height);

	rkisp_config_dcrop(stream, dcrop, async);

	return 0;
}

/* configure scale unit */
static int rkisp_stream_config_rsz(struct rkisp_stream *stream, bool async)
{
	struct rkisp_device *dev = stream->ispdev;
	struct v4l2_pix_format_mplane output_fmt = stream->out_fmt;
	struct ispsd_out_fmt *input_isp_fmt =
			rkisp_get_ispsd_out_fmt(&dev->isp_sdev);
	struct v4l2_rect in_y, in_c, out_y, out_c;

	if (input_isp_fmt->fmt_type == FMT_BAYER)
		goto disable;

	/* set input and output sizes for scale calculation
	 * input/output yuv422
	 */
	in_y.width = stream->dcrop.width;
	in_y.height = stream->dcrop.height;
	in_c.width = in_y.width / 2;
	in_c.height = in_y.height;

	out_y.width = output_fmt.width;
	out_y.height = output_fmt.height;
	out_c.width = out_y.width / 2;
	out_c.height = out_y.height;
	if (in_c.width == out_c.width && in_c.height == out_c.height)
		goto disable;

	/* set RSZ input and output */
	v4l2_dbg(1, rkisp_debug, &dev->v4l2_dev,
		 "stream %d rsz/scale: %dx%d -> %dx%d\n",
		 stream->id, stream->dcrop.width, stream->dcrop.height,
		 output_fmt.width, output_fmt.height);
	v4l2_dbg(1, rkisp_debug, &dev->v4l2_dev,
		 "chroma scaling %dx%d -> %dx%d\n",
		 in_c.width, in_c.height, out_c.width, out_c.height);

	/* calculate and set scale */
	rkisp_config_rsz(stream, &in_y, &in_c, &out_y, &out_c, async);

	return 0;

disable:
	rkisp_disable_rsz(stream, async);

	return 0;
}

/***************************** stream operations*******************************/

/*
 * memory base addresses should be with respect
 * to the burst alignment restriction for AXI.
 */
static u32 calc_burst_len(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	u32 y_size = stream->out_fmt.plane_fmt[0].bytesperline *
		stream->out_fmt.height;
	u32 cb_size = stream->out_fmt.plane_fmt[1].sizeimage;
	u32 cr_size = stream->out_fmt.plane_fmt[2].sizeimage;
	u32 cb_offs, cr_offs;
	u32 bus = 16, burst;
	int i;

	/* y/c base addr: burstN * bus alignment */
	cb_offs = y_size;
	cr_offs = cr_size ? (cb_size + cb_offs) : 0;

	if (!(cb_offs % (bus * 16)) && !(cr_offs % (bus * 16)))
		burst = CIF_MI_CTRL_BURST_LEN_LUM_16 |
			CIF_MI_CTRL_BURST_LEN_CHROM_16;
	else if (!(cb_offs % (bus * 8)) && !(cr_offs % (bus * 8)))
		burst = CIF_MI_CTRL_BURST_LEN_LUM_8 |
			CIF_MI_CTRL_BURST_LEN_CHROM_8;
	else
		burst = CIF_MI_CTRL_BURST_LEN_LUM_4 |
			CIF_MI_CTRL_BURST_LEN_CHROM_4;

	if (cb_offs % (bus * 4) || cr_offs % (bus * 4))
		v4l2_warn(&dev->v4l2_dev,
			"%dx%d fmt:0x%x not support, should be %d aligned\n",
			stream->out_fmt.width,
			stream->out_fmt.height,
			stream->out_fmt.pixelformat,
			(cr_offs == 0) ? bus * 4 : bus * 16);

	stream->burst = burst;
	for (i = 0; i <= RKISP_STREAM_SP; i++)
		if (burst > dev->cap_dev.stream[i].burst)
			burst = dev->cap_dev.stream[i].burst;

	if (stream->interlaced) {
		if (!stream->out_fmt.width % (bus * 16))
			stream->burst = CIF_MI_CTRL_BURST_LEN_LUM_16 |
				CIF_MI_CTRL_BURST_LEN_CHROM_16;
		else if (!stream->out_fmt.width % (bus * 8))
			stream->burst = CIF_MI_CTRL_BURST_LEN_LUM_8 |
				CIF_MI_CTRL_BURST_LEN_CHROM_8;
		else
			stream->burst = CIF_MI_CTRL_BURST_LEN_LUM_4 |
				CIF_MI_CTRL_BURST_LEN_CHROM_4;
		if (stream->out_fmt.width % (bus * 4))
			v4l2_warn(&dev->v4l2_dev,
				"interlaced: width should be %d aligned\n",
				bus * 4);
		burst = min(stream->burst, burst);
		stream->burst = burst;
	}

	return burst;
}

/*
 * configure memory interface for mainpath
 * This should only be called when stream-on
 */
static int mp_config_mi(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	struct capture_fmt *fmt = &stream->out_isp_fmt;
	struct v4l2_pix_format_mplane *out_fmt = &stream->out_fmt;
	u32 val, mask, height = out_fmt->height;

       /*
	* NOTE: plane_fmt[0].sizeimage is total size of all planes for single
	* memory plane formats, so calculate the size explicitly.
	*/
	val = out_fmt->plane_fmt[0].bytesperline;
	val /= DIV_ROUND_UP(fmt->bpp[0], 8);
	rkisp_unite_write(dev, ISP3X_MI_MP_WR_Y_LLENGTH, val, false);
	val *= height;
	rkisp_unite_write(dev, stream->config->mi.y_pic_size, val, false);
	val = out_fmt->plane_fmt[0].bytesperline * height;
	rkisp_unite_write(dev, stream->config->mi.y_size_init, val, false);

	val = out_fmt->plane_fmt[1].sizeimage;
	rkisp_unite_write(dev, stream->config->mi.cb_size_init, val, false);

	val = out_fmt->plane_fmt[2].sizeimage;
	rkisp_unite_write(dev, stream->config->mi.cr_size_init, val, false);

	val = stream->out_isp_fmt.uv_swap ? ISP3X_MI_XTD_FORMAT_MP_UV_SWAP : 0;
	mask = ISP3X_MI_XTD_FORMAT_MP_UV_SWAP;
	rkisp_unite_set_bits(dev, ISP3X_MI_WR_XTD_FORMAT_CTRL, mask, val, false);

	mask = ISP3X_MPFBC_FORCE_UPD | ISP3X_MP_YUV_MODE;
	val = rkisp_read_reg_cache(dev, ISP3X_MPFBC_CTRL) & ~mask;
	if (stream->out_fmt.pixelformat == V4L2_PIX_FMT_NV21 ||
	    stream->out_fmt.pixelformat == V4L2_PIX_FMT_NV12 ||
	    stream->out_fmt.pixelformat == V4L2_PIX_FMT_NV21M ||
	    stream->out_fmt.pixelformat == V4L2_PIX_FMT_NV12M ||
	    stream->out_fmt.pixelformat == V4L2_PIX_FMT_YUV420)
		val |= ISP3X_SEPERATE_YUV_CFG;
	else
		val |= ISP3X_SEPERATE_YUV_CFG | ISP3X_MP_YUV_MODE;
	rkisp_unite_write(dev, ISP3X_MPFBC_CTRL, val, false);

	val = stream->out_isp_fmt.output_format;
	rkisp_unite_write(dev, ISP32_MI_MP_WR_CTRL, val, false);

	val = calc_burst_len(stream) | CIF_MI_CTRL_INIT_BASE_EN |
		CIF_MI_CTRL_INIT_OFFSET_EN | CIF_MI_MP_AUTOUPDATE_ENABLE |
		stream->out_isp_fmt.write_format;
	mask = GENMASK(19, 16) | MI_CTRL_MP_FMT_MASK;
	rkisp_unite_set_bits(dev, ISP3X_MI_WR_CTRL, mask, val, false);

	mi_frame_end_int_enable(stream);
	/* set up first buffer */
	mi_frame_end(stream, FRAME_INIT);

	rkisp_unite_write(dev, stream->config->mi.y_offs_cnt_init, 0, false);
	rkisp_unite_write(dev, stream->config->mi.cb_offs_cnt_init, 0, false);
	rkisp_unite_write(dev, stream->config->mi.cr_offs_cnt_init, 0, false);
	return 0;
}

static int mbus_code_sp_in_fmt(u32 in_mbus_code, u32 out_fourcc, u32 *format)
{
	switch (in_mbus_code) {
	case MEDIA_BUS_FMT_YUYV8_2X8:
		*format = MI_CTRL_SP_INPUT_YUV422;
		break;
	default:
		return -EINVAL;
	}

	/*
	 * Only SP can support output format of YCbCr4:0:0,
	 * and the input format of SP must be YCbCr4:0:0
	 * when outputting YCbCr4:0:0.
	 * The output format of isp is YCbCr4:2:2,
	 * so the CbCr data is discarded here.
	 */
	if (out_fourcc == V4L2_PIX_FMT_GREY)
		*format = MI_CTRL_SP_INPUT_YUV400;

	return 0;
}

/*
 * configure memory interface for selfpath
 * This should only be called when stream-on
 */
static int sp_config_mi(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	struct v4l2_pix_format_mplane *out_fmt = &stream->out_fmt;
	struct ispsd_out_fmt *input_isp_fmt =
			rkisp_get_ispsd_out_fmt(&dev->isp_sdev);
	u32 sp_in_fmt, val, mask;

	if (mbus_code_sp_in_fmt(input_isp_fmt->mbus_code,
				out_fmt->pixelformat, &sp_in_fmt)) {
		v4l2_err(&dev->v4l2_dev, "Can't find the input format\n");
		return -EINVAL;
	}

       /*
	* NOTE: plane_fmt[0].sizeimage is total size of all planes for single
	* memory plane formats, so calculate the size explicitly.
	*/
	val = stream->u.sp.y_stride;
	rkisp_unite_write(dev, ISP3X_MI_SP_WR_Y_LLENGTH, val, false);
	val *= out_fmt->height;
	rkisp_unite_write(dev, stream->config->mi.y_pic_size, val, false);
	val = out_fmt->plane_fmt[0].bytesperline * out_fmt->height;
	rkisp_unite_write(dev, stream->config->mi.y_size_init, val, false);

	val = out_fmt->plane_fmt[1].sizeimage;
	rkisp_unite_write(dev, stream->config->mi.cb_size_init, val, false);

	val = out_fmt->plane_fmt[2].sizeimage;
	rkisp_unite_write(dev, stream->config->mi.cr_size_init, val, false);

	val = stream->out_isp_fmt.uv_swap ? ISP3X_MI_XTD_FORMAT_SP_UV_SWAP : 0;
	mask = ISP3X_MI_XTD_FORMAT_SP_UV_SWAP;
	rkisp_unite_set_bits(dev, ISP3X_MI_WR_XTD_FORMAT_CTRL, mask, val, false);

	mask = ISP3X_MPFBC_FORCE_UPD | ISP3X_SP_YUV_MODE;
	val = rkisp_read_reg_cache(dev, ISP3X_MPFBC_CTRL) & ~mask;
	if (stream->out_fmt.pixelformat == V4L2_PIX_FMT_NV21 ||
	    stream->out_fmt.pixelformat == V4L2_PIX_FMT_NV12 ||
	    stream->out_fmt.pixelformat == V4L2_PIX_FMT_NV21M ||
	    stream->out_fmt.pixelformat == V4L2_PIX_FMT_NV12M ||
	    stream->out_fmt.pixelformat == V4L2_PIX_FMT_YUV420)
		val |= ISP3X_SEPERATE_YUV_CFG;
	else
		val |= ISP3X_SEPERATE_YUV_CFG | ISP3X_SP_YUV_MODE;
	rkisp_unite_write(dev, ISP3X_MPFBC_CTRL, val, false);

	val = calc_burst_len(stream) | CIF_MI_CTRL_INIT_BASE_EN |
		CIF_MI_CTRL_INIT_OFFSET_EN | stream->out_isp_fmt.write_format |
		sp_in_fmt | stream->out_isp_fmt.output_format |
		CIF_MI_SP_AUTOUPDATE_ENABLE;
	mask = GENMASK(19, 16) | MI_CTRL_SP_FMT_MASK;
	rkisp_unite_set_bits(dev, ISP3X_MI_WR_CTRL, mask, val, false);

	mi_frame_end_int_enable(stream);
	/* set up first buffer */
	mi_frame_end(stream, FRAME_INIT);

	rkisp_unite_write(dev, stream->config->mi.y_offs_cnt_init, 0, false);
	rkisp_unite_write(dev, stream->config->mi.cb_offs_cnt_init, 0, false);
	rkisp_unite_write(dev, stream->config->mi.cr_offs_cnt_init, 0, false);
	return 0;
}

static int ldc_config_mi(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	struct v4l2_pix_format_mplane *out_fmt = &stream->out_fmt;
	u32 val;

	val = out_fmt->width;
	rkisp_unite_write(dev, ISP39_LDCH_OUT_SIZE, val, false);
	val = out_fmt->height;
	rkisp_unite_write(dev, ISP39_LDCV_OUT_SIZE, val, false);
	val = out_fmt->plane_fmt[0].bytesperline;
	rkisp_unite_write(dev, ISP39_LDCV_WR_STRIDE, val, false);
	mi_frame_end(stream, FRAME_INIT);
	rkisp_unite_set_bits(dev, ISP3X_ISP_IMSC, 0, ISP39_LDCV_END, false);
	val = stream->out_isp_fmt.output_format;
	if (stream->out_isp_fmt.uv_swap)
		val |= ISP39_LDCV_UV_SWAP;
	if (val)
		rkisp_unite_set_bits(dev, ISP39_LDCV_CTRL, ISP39_LDCV_FORCE_UPD, val, false);
	return 0;
}

static void mp_enable_mi(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	u32 mask = CIF_MI_CTRL_MP_ENABLE | CIF_MI_CTRL_RAW_ENABLE;
	u32 val = CIF_MI_CTRL_MP_ENABLE;

	rkisp_unite_set_bits(dev, ISP3X_MI_WR_CTRL, mask, val, false);
}

static void sp_enable_mi(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	struct capture_fmt *fmt = &stream->out_isp_fmt;
	u32 val = CIF_MI_CTRL_SP_ENABLE;
	u32 mask = CIF_MI_SP_Y_FULL_YUV2RGB | CIF_MI_SP_CBCR_FULL_YUV2RGB;

	if (fmt->fmt_type == FMT_RGB &&
	    dev->isp_sdev.quantization == V4L2_QUANTIZATION_FULL_RANGE)
		val |= mask;
	rkisp_unite_set_bits(stream->ispdev, ISP3X_MI_WR_CTRL, mask, val, false);
}

static void ldc_enable_mi(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;

	rkisp_unite_set_bits(dev, ISP39_LDCV_CTRL, ISP39_LDCV_FORCE_UPD, 1, false);
	rkisp_unite_set_bits(dev, ISP3X_LDCH_STS, ISP3X_LDCH_FORCE_UPD, 1, false);
}

static void mp_disable_mi(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	u32 mask = CIF_MI_CTRL_MP_ENABLE | CIF_MI_CTRL_RAW_ENABLE;

	rkisp_unite_set_bits(dev, 0x1814, 0, BIT(0), false);
	rkisp_unite_clear_bits(stream->ispdev, ISP3X_MI_WR_CTRL, mask, false);
}

static void sp_disable_mi(struct rkisp_stream *stream)
{
	rkisp_unite_clear_bits(stream->ispdev, ISP3X_MI_WR_CTRL, CIF_MI_CTRL_SP_ENABLE, false);
}

static void ldc_disable_mi(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	u32 val;

	val = ISP3X_LDCH_EN | ISP3X_LDCH_FORCE_UPD;
	rkisp_unite_set_bits(dev, ISP3X_LDCH_STS, val, 0, false);

	val = ISP39_LDCV_EN | ISP39_LDCV_FORCE_UPD;
	rkisp_unite_set_bits(dev, ISP39_LDCV_CTRL, val, 0, false);
}

static void update_mi(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	struct v4l2_pix_format_mplane *out_fmt = &stream->out_fmt;
	u32 div = stream->out_isp_fmt.fourcc == V4L2_PIX_FMT_UYVY ? 1 : 2;
	u32 val, reg;

	if (stream->next_buf) {
		reg = stream->config->mi.y_base_ad_init;
		val = stream->next_buf->buff_addr[RKISP_PLANE_Y];
		rkisp_write(dev, reg, val, false);

		reg = stream->config->mi.cb_base_ad_init;
		val = stream->next_buf->buff_addr[RKISP_PLANE_CB];
		rkisp_write(dev, reg, val, false);

		if (stream->id != RKISP_STREAM_LDC) {
			reg = stream->config->mi.cr_base_ad_init;
			val = stream->next_buf->buff_addr[RKISP_PLANE_CR];
			rkisp_write(dev, reg, val, false);
		}

		if (dev->unite_div > ISP_UNITE_DIV1) {
			/* right of image, or right top of image */
			reg = stream->config->mi.y_base_ad_init;
			val = stream->next_buf->buff_addr[RKISP_PLANE_Y];
			val += ((out_fmt->width / div) & ~0xf);
			rkisp_idx_write(dev, reg, val, ISP_UNITE_RIGHT, false);

			reg = stream->config->mi.cb_base_ad_init;
			val = stream->next_buf->buff_addr[RKISP_PLANE_CB];
			val += ((out_fmt->width / div) & ~0xf);
			rkisp_idx_write(dev, reg, val, ISP_UNITE_RIGHT, false);

			if (stream->id != RKISP_STREAM_LDC) {
				reg = stream->config->mi.cr_base_ad_init;
				val = stream->next_buf->buff_addr[RKISP_PLANE_CR];
				val += ((out_fmt->width / div) & ~0xf);
				rkisp_idx_write(dev, reg, val, ISP_UNITE_RIGHT, false);
			}
		}

		if (stream->is_pause) {
			/* single sensor mode with pingpong buffer:
			 * if mi on, addr will auto update at frame end
			 * else addr need update by SELF_UPD.
			 *
			 * multi sensor mode with single buffer:
			 * mi and buffer will update by readback.
			 */
			stream->ops->enable_mi(stream);
			if (dev->hw_dev->is_single &&
			    stream->ops->is_stream_stopped(stream)) {
				/* isp no start and mi close, force to enable it */
				if (!ISP3X_ISP_OUT_LINE(rkisp_read(dev, ISP3X_ISP_DEBUG2, true)))
					stream_self_update(stream);
				if (!stream->ops->is_stream_stopped(stream)) {
					if (!stream->curr_buf) {
						stream->curr_buf = stream->next_buf;
						stream->next_buf = NULL;
					}
					/* maybe no next buf to preclose mi */
					stream->ops->disable_mi(stream);
				} else {
					/* isp working and mi closed
					 * config buf and enable mi, capture at next frame
					 */
					stream->is_pause = false;
				}
			} else {
				/* isp working and mi no to close
				 * config buf will auto update at frame end
				 */
				stream->is_pause = false;
			}
		}

		/* single buf force updated at readback for multidevice */
		if (!dev->hw_dev->is_single) {
			stream->curr_buf = stream->next_buf;
			stream->next_buf = NULL;
		}
	} else if (!stream->is_pause) {
		stream->is_pause = true;
		/* no next buf to preclose mi */
		stream->ops->disable_mi(stream);
		/* no buf, force to close mi */
		if (!stream->curr_buf && dev->hw_dev->is_single)
			stream_self_update(stream);
	}

	v4l2_dbg(2, rkisp_debug, &dev->v4l2_dev,
		 "%s stream:%d cur:%p next:%p Y:0x%x CB:0x%x | Y_SHD:0x%x pause:%d stop:%d\n",
		 __func__, stream->id, stream->curr_buf, stream->next_buf,
		 rkisp_read(dev, stream->config->mi.y_base_ad_init, false),
		 rkisp_read(dev, stream->config->mi.cb_base_ad_init, false),
		 rkisp_read(dev, stream->config->mi.y_base_ad_shd, true),
		 stream->is_pause, stream->ops->is_stream_stopped(stream));
}

static struct streams_ops rkisp_mp_streams_ops = {
	.config_mi = mp_config_mi,
	.enable_mi = mp_enable_mi,
	.disable_mi = mp_disable_mi,
	.set_data_path = stream_data_path,
	.is_stream_stopped = mp_is_stream_stopped,
	.update_mi = update_mi,
	.frame_end = mi_frame_end,
	.frame_start = mi_frame_start,
	.isp_end = isp_frame_end,
};

static struct streams_ops rkisp_sp_streams_ops = {
	.config_mi = sp_config_mi,
	.enable_mi = sp_enable_mi,
	.disable_mi = sp_disable_mi,
	.set_data_path = stream_data_path,
	.is_stream_stopped = sp_is_stream_stopped,
	.update_mi = update_mi,
	.frame_end = mi_frame_end,
	.frame_start = mi_frame_start,
	.isp_end = isp_frame_end,
};

static struct streams_ops rkisp_ldc_streams_ops = {
	.config_mi = ldc_config_mi,
	.enable_mi = ldc_enable_mi,
	.disable_mi = ldc_disable_mi,
	.is_stream_stopped = ldc_is_stream_stopped,
	.update_mi = update_mi,
	.frame_end = mi_frame_end,
	.frame_start = mi_frame_start,
};

static int mi_frame_start(struct rkisp_stream *stream, u32 irq)
{
	struct rkisp_device *dev = stream->ispdev;
	unsigned long lock_flags = 0;
	u32 val;

	/* readback start to update stream buf if null */
	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	if (stream->streaming) {
		/* only dynamic clipping and scaling at readback */
		if (!irq && stream->is_crop_upd) {
			rkisp_stream_config_dcrop(stream, false);
			rkisp_stream_config_rsz(stream, false);
			stream->is_crop_upd = false;
		}
		if (!list_empty(&stream->buf_queue) &&
		    ((dev->hw_dev->is_single && !stream->next_buf) ||
		     (!dev->hw_dev->is_single && !stream->curr_buf))) {
			stream->next_buf = list_first_entry(&stream->buf_queue,
							struct rkisp_buffer, queue);
			list_del(&stream->next_buf->queue);
			stream->ops->update_mi(stream);
		} else if (dev->hw_dev->is_single &&
			   stream->next_buf && !stream->curr_buf) {
			val = rkisp_read(dev, ISP3X_ISP_DEBUG2, true);
			if (stream->ops->is_stream_stopped(stream) &&
			    !ISP3X_ISP_OUT_LINE(val)) {
				stream->ops->enable_mi(stream);
				stream_self_update(stream);
			}
			if (!stream->ops->is_stream_stopped(stream)) {
				stream->curr_buf = stream->next_buf;
				stream->next_buf = NULL;
				if (!list_empty(&stream->buf_queue)) {
					stream->next_buf = list_first_entry(&stream->buf_queue,
								struct rkisp_buffer, queue);
					list_del(&stream->next_buf->queue);
				}
				stream->ops->update_mi(stream);
			}
		}
		/* check frame loss */
		if (stream->ops->is_stream_stopped(stream))
			stream->dbg.frameloss++;
	}
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);

	return 0;
}

static int isp_frame_end(struct rkisp_stream *stream, u32 irq)
{
	struct rkisp_device *dev = stream->ispdev;
	u32 val = ISP3X_ISP_OUT_LINE(rkisp_read(dev, ISP3X_ISP_DEBUG2, true));

	if (stream->need_scl_upd) {
		if (val)
			v4l2_err(&dev->v4l2_dev,
				 "no to update scl, need to increase sensor vblank\n");
		else {
			val = ISP32_SCALE_FORCE_UPD | ISP32_SCALE_GEN_UPD;
			rkisp_write(dev, stream->config->rsz.update, val, true);
			stream->need_scl_upd = false;
		}
	}
	return 0;
}

/*
 * This function is called when a frame end come. The next frame
 * is processing and we should set up buffer for next-next frame,
 * otherwise it will overflow.
 */
static int mi_frame_end(struct rkisp_stream *stream, u32 state)
{
	struct rkisp_device *dev = stream->ispdev;
	struct capture_fmt *isp_fmt = &stream->out_isp_fmt;
	unsigned long lock_flags = 0;
	struct rkisp_buffer *buf = NULL;
	u32 i;

	if (stream->id == RKISP_STREAM_VIR)
		return 0;

	if (dev->cap_dev.is_done_early &&
	    (state == FRAME_IRQ || state == FRAME_WORK)) {
		/* skip mainpath wrap mode */
		if (stream->id == RKISP_STREAM_MP && dev->cap_dev.wrap_line)
			return 0;
		spin_lock_irqsave(&stream->vbq_lock, lock_flags);
		if (state == FRAME_IRQ && stream->curr_buf)
			stream->frame_early = false;
		else
			stream->frame_early = true;
		buf = stream->curr_buf;
		stream->curr_buf = NULL;
		spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
		if ((!stream->frame_early && state == FRAME_WORK) ||
		    (stream->frame_early && state == FRAME_IRQ))
			goto end;
	} else {
		spin_lock_irqsave(&stream->vbq_lock, lock_flags);
		buf = stream->curr_buf;
		stream->curr_buf = NULL;
		spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
	}

	if (buf) {
		struct vb2_buffer *vb2_buf = &buf->vb.vb2_buf;
		struct rkisp_stream *vir = &dev->cap_dev.stream[RKISP_STREAM_VIR];
		u64 ns = 0;

		if (dev->skip_frame || stream->skip_frame) {
			spin_lock_irqsave(&stream->vbq_lock, lock_flags);
			list_add_tail(&buf->queue, &stream->buf_queue);
			spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
			if (stream->skip_frame)
				stream->skip_frame--;
			goto end;
		}

		for (i = 0; i < isp_fmt->mplanes; i++) {
			u32 payload_size = stream->out_fmt.plane_fmt[i].sizeimage;

			vb2_set_plane_payload(vb2_buf, i, payload_size);
		}

		rkisp_dmarx_get_frame(dev, &i, NULL, &ns, true);
		if (!ns)
			ns = ktime_get_ns();
		buf->vb.sequence = i;
		buf->vb.vb2_buf.timestamp = ns;
		ns = ktime_get_ns();
		stream->dbg.interval = ns - stream->dbg.timestamp;
		stream->dbg.delay = ns - dev->isp_sdev.frm_timestamp;
		stream->dbg.timestamp = ns;
		stream->dbg.id = i;

		if (vir->streaming && vir->conn_id == stream->id) {
			spin_lock_irqsave(&vir->vbq_lock, lock_flags);
			list_add_tail(&buf->queue, &dev->cap_dev.vir_cpy.queue);
			spin_unlock_irqrestore(&vir->vbq_lock, lock_flags);
			if (!completion_done(&dev->cap_dev.vir_cpy.cmpl))
				complete(&dev->cap_dev.vir_cpy.cmpl);
		} else {
			rkisp_stream_buf_done(stream, buf);
		}
	}

end:
	if (state == FRAME_WORK)
		return 0;
	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	if (stream->next_buf) {
		stream->curr_buf = stream->next_buf;
		stream->next_buf = NULL;
	}
	if (!list_empty(&stream->buf_queue)) {
		stream->next_buf = list_first_entry(&stream->buf_queue,
						    struct rkisp_buffer, queue);
		list_del(&stream->next_buf->queue);
	}
	stream->ops->update_mi(stream);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
	return 0;
}

/***************************** vb2 operations*******************************/

/*
 * Set flags and wait, it should stop in interrupt.
 * If it didn't, stop it by force.
 */
static void rkisp_stream_stop(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	unsigned long lock_flags = 0;
	int ret = 0;
	bool is_wait = dev->hw_dev->is_shutdown ? false : true;

	stream->stopping = true;
	stream->is_pause = false;
	if (stream->ops->disable_mi && dev->hw_dev->is_single)
		stream->ops->disable_mi(stream);
	if (IS_HDR_RDBK(dev->rd_mode)) {
		spin_lock_irqsave(&dev->hw_dev->rdbk_lock, lock_flags);
		if (dev->hw_dev->cur_dev_id != dev->dev_id || dev->hw_dev->is_idle) {
			is_wait = false;
			if (stream->ops->disable_mi)
				stream->ops->disable_mi(stream);
			/* force update to close */
			if (dev->hw_dev->is_single)
				stream_self_update(stream);
		}
		if (atomic_read(&dev->cap_dev.refcnt) == 1 && !is_wait)
			dev->isp_state = ISP_STOP;
		spin_unlock_irqrestore(&dev->hw_dev->rdbk_lock, lock_flags);
	}
	if (is_wait && !stream->ops->is_stream_stopped(stream)) {
		ret = wait_event_timeout(stream->done,
					 !stream->streaming,
					 msecs_to_jiffies(500));
		if (!ret)
			v4l2_warn(v4l2_dev, "%s id:%d timeout\n",
				  __func__, stream->id);
	}

	stream->stopping = false;
	stream->streaming = false;
	if (stream->ops->disable_mi)
		stream->ops->disable_mi(stream);
	if (stream->id == RKISP_STREAM_MP || stream->id == RKISP_STREAM_SP) {
		rkisp_disable_dcrop(stream, true);
		rkisp_disable_rsz(stream, true);
	}
	ret = get_stream_irq_mask(stream);
	dev->irq_ends_mask &= ~ret;

	stream->burst =
		CIF_MI_CTRL_BURST_LEN_LUM_16 |
		CIF_MI_CTRL_BURST_LEN_CHROM_16;
	stream->interlaced = false;
}

/*
 * Most of registers inside rockchip isp1 have shadow register since
 * they must be not changed during processing a frame.
 * Usually, each sub-module updates its shadow register after
 * processing the last pixel of a frame.
 */
static int rkisp_start(struct rkisp_stream *stream)
{
	int ret;

	if (stream->ops->set_data_path)
		stream->ops->set_data_path(stream);
	if (stream->ops->config_mi) {
		ret = stream->ops->config_mi(stream);
		if (ret)
			return ret;
	}
	if (stream->ops->enable_mi && !stream->is_pause)
		stream->ops->enable_mi(stream);

	stream->streaming = true;
	stream->skip_frame = 0;
	return 0;
}

static int rkisp_queue_setup(struct vb2_queue *queue,
			      unsigned int *num_buffers,
			      unsigned int *num_planes,
			      unsigned int sizes[],
			      struct device *alloc_ctxs[])
{
	struct rkisp_stream *stream = queue->drv_priv;
	struct rkisp_device *dev = stream->ispdev;
	const struct v4l2_pix_format_mplane *pixm = NULL;
	const struct capture_fmt *isp_fmt = NULL;
	u32 i;

	pixm = &stream->out_fmt;
	isp_fmt = &stream->out_isp_fmt;
	*num_planes = isp_fmt->mplanes;

	for (i = 0; i < isp_fmt->mplanes; i++) {
		const struct v4l2_plane_pix_format *plane_fmt;
		u32 height = pixm->height;

		plane_fmt = &pixm->plane_fmt[i];
		/* height to align with 16 when allocating memory
		 * so that Rockchip encoder can use DMA buffer directly
		 */
		sizes[i] = (isp_fmt->fmt_type == FMT_YUV) ?
			plane_fmt->sizeimage / height *
			ALIGN(height, 16) :
			plane_fmt->sizeimage;
	}

	rkisp_chk_tb_over(dev);
	v4l2_dbg(1, rkisp_debug, &dev->v4l2_dev, "%s %s count %d, size %d\n",
		 stream->vnode.vdev.name, v4l2_type_names[queue->type], *num_buffers, sizes[0]);

	return 0;
}

/*
 * The vb2_buffer are stored in rkisp_buffer, in order to unify
 * mplane buffer and none-mplane buffer.
 */
static void rkisp_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rkisp_buffer *ispbuf = to_rkisp_buffer(vbuf);
	struct vb2_queue *queue = vb->vb2_queue;
	struct rkisp_stream *stream = queue->drv_priv;
	struct rkisp_device *dev = stream->ispdev;
	struct v4l2_pix_format_mplane *pixm = &stream->out_fmt;
	struct capture_fmt *isp_fmt = &stream->out_isp_fmt;
	unsigned long lock_flags = 0;
	struct sg_table *sgt;
	u32 height, offset;
	int i;

	memset(ispbuf->buff_addr, 0, sizeof(ispbuf->buff_addr));
	for (i = 0; i < isp_fmt->mplanes; i++) {
		ispbuf->vaddr[i] = vb2_plane_vaddr(vb, i);

		if (stream->ispdev->hw_dev->is_dma_sg_ops) {
			sgt = vb2_dma_sg_plane_desc(vb, i);
			ispbuf->buff_addr[i] = sg_dma_address(sgt->sgl);
		} else {
			ispbuf->buff_addr[i] = vb2_dma_contig_plane_dma_addr(vb, i);
		}
	}
	/*
	 * NOTE: plane_fmt[0].sizeimage is total size of all planes for single
	 * memory plane formats, so calculate the size explicitly.
	 */
	if (isp_fmt->mplanes == 1) {
		for (i = 0; i < isp_fmt->cplanes - 1; i++) {
			height = pixm->height;
			if (dev->cap_dev.wrap_line && stream->id == RKISP_STREAM_MP)
				height = dev->cap_dev.wrap_line;
			offset = (i == 0) ?
				pixm->plane_fmt[i].bytesperline * height :
				pixm->plane_fmt[i].sizeimage;
			ispbuf->buff_addr[i + 1] =
				ispbuf->buff_addr[i] + offset;
		}
	}

	v4l2_dbg(2, rkisp_debug, &dev->v4l2_dev,
		 "stream:%d queue buf:0x%x\n",
		 stream->id, ispbuf->buff_addr[0]);

	spin_lock_irqsave(&stream->vbq_lock, lock_flags);
	list_add_tail(&ispbuf->queue, &stream->buf_queue);
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
}

static void destroy_buf_queue(struct rkisp_stream *stream,
			      enum vb2_buffer_state state)
{
	unsigned long lock_flags = 0;
	struct rkisp_buffer *buf;

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
		if (buf->vb.vb2_buf.memory)
			vb2_buffer_done(&buf->vb.vb2_buf, state);
	}
	while (!list_empty(&stream->buf_done_list)) {
		buf = list_first_entry(&stream->buf_done_list,
			struct rkisp_buffer, queue);
		list_del(&buf->queue);
		if (buf->vb.vb2_buf.memory)
			vb2_buffer_done(&buf->vb.vb2_buf, state);
	}
	spin_unlock_irqrestore(&stream->vbq_lock, lock_flags);
}

static void rkisp_stop_streaming(struct vb2_queue *queue)
{
	struct rkisp_stream *stream = queue->drv_priv;
	struct rkisp_vdev_node *node = &stream->vnode;
	struct rkisp_device *dev = stream->ispdev;
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	int ret;

	mutex_lock(&dev->hw_dev->dev_lock);

	v4l2_dbg(1, rkisp_debug, v4l2_dev, "%s %s %d\n",
		 __func__, node->vdev.name, stream->id);

	if (!stream->streaming)
		goto end;

	if (stream->id == RKISP_STREAM_VIR) {
		stream->stopping = true;
		if (!dev->hw_dev->is_shutdown)
			wait_event_timeout(stream->done,
					   stream->frame_end,
					   msecs_to_jiffies(500));
		stream->streaming = false;
		stream->stopping = false;
		destroy_buf_queue(stream, VB2_BUF_STATE_ERROR);
		if (!completion_done(&dev->cap_dev.vir_cpy.cmpl)) {
			complete(&dev->cap_dev.vir_cpy.cmpl);
			stream->conn_id = -1;
		}
		goto end;
	}

	rkisp_stream_stop(stream);
	/* call to the other devices */
	video_device_pipeline_stop(&node->vdev);
	ret = dev->pipe.set_stream(&dev->pipe, false);
	if (ret < 0)
		v4l2_err(v4l2_dev, "pipeline stream-off failed:%d\n", ret);

	/* release buffers */
	destroy_buf_queue(stream, VB2_BUF_STATE_ERROR);

	ret = dev->pipe.close(&dev->pipe);
	if (ret < 0)
		v4l2_err(v4l2_dev, "pipeline close failed error:%d\n", ret);
	atomic_dec(&dev->cap_dev.refcnt);
	tasklet_disable(&stream->buf_done_tasklet);
end:
	mutex_unlock(&dev->hw_dev->dev_lock);
}

static int rkisp_stream_start(struct rkisp_stream *stream)
{
	struct rkisp_device *dev = stream->ispdev;
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	bool async = false;
	int ret;

	stream->need_scl_upd = false;
	if (stream->id == RKISP_STREAM_LDC)
		goto skip;

	async = (dev->cap_dev.stream[RKISP_STREAM_MP].streaming ||
		 dev->cap_dev.stream[RKISP_STREAM_SP].streaming);

	/*
	 * can't be async now, otherwise the latter started stream fails to
	 * produce mi interrupt.
	 */
	ret = rkisp_stream_config_dcrop(stream, false);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "config dcrop failed with error %d\n", ret);
		return ret;
	}

	ret = rkisp_stream_config_rsz(stream, async);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "config rsz failed with error %d\n", ret);
		return ret;
	}
	if (async && dev->hw_dev->is_single)
		stream->need_scl_upd = true;
skip:
	return rkisp_start(stream);
}

static int
rkisp_start_streaming(struct vb2_queue *queue, unsigned int count)
{
	struct rkisp_stream *stream = queue->drv_priv;
	struct rkisp_vdev_node *node = &stream->vnode;
	struct rkisp_device *dev = stream->ispdev;
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	int ret = -EINVAL;

	mutex_lock(&dev->hw_dev->dev_lock);

	v4l2_dbg(1, rkisp_debug, v4l2_dev, "%s %s id:%d\n",
		 __func__, node->vdev.name, stream->id);

	if (WARN_ON(stream->streaming)) {
		mutex_unlock(&dev->hw_dev->dev_lock);
		return -EBUSY;
	}

	if (stream->id == RKISP_STREAM_VIR) {
		struct rkisp_stream *t = &dev->cap_dev.stream[stream->conn_id];

		if (t->streaming) {
			INIT_WORK(&dev->cap_dev.vir_cpy.work, rkisp_stream_vir_cpy_image);
			init_completion(&dev->cap_dev.vir_cpy.cmpl);
			INIT_LIST_HEAD(&dev->cap_dev.vir_cpy.queue);
			dev->cap_dev.vir_cpy.stream = stream;
			schedule_work(&dev->cap_dev.vir_cpy.work);
			ret = 0;
		} else {
			v4l2_err(&dev->v4l2_dev, "no stream enable for iqtool\n");
			destroy_buf_queue(stream, VB2_BUF_STATE_QUEUED);
			ret = -EINVAL;
		}
		mutex_unlock(&dev->hw_dev->dev_lock);
		return ret;
	}

	memset(&stream->dbg, 0, sizeof(stream->dbg));

	atomic_inc(&dev->cap_dev.refcnt);
	if (!dev->isp_inp || !stream->linked) {
		v4l2_err(v4l2_dev, "check %s link or isp input\n", node->vdev.name);
		goto buffer_done;
	}

	if (atomic_read(&dev->cap_dev.refcnt) == 1 &&
	    (dev->isp_inp & INP_CIF)) {
		/* update sensor info when first streaming */
		ret = rkisp_update_sensor_info(dev);
		if (ret < 0) {
			v4l2_err(v4l2_dev, "update sensor info failed %d\n", ret);
			goto buffer_done;
		}
	}

	/* enable clocks/power-domains */
	ret = dev->pipe.open(&dev->pipe, &node->vdev.entity, true);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "open isp pipeline failed %d\n", ret);
		goto buffer_done;
	}

	/* configure stream hardware to start */
	ret = rkisp_stream_start(stream);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "start %s failed\n", node->vdev.name);
		goto close_pipe;
	}

	/* start sub-devices */
	ret = dev->pipe.set_stream(&dev->pipe, true);
	if (ret < 0)
		goto stop_stream;

	ret = video_device_pipeline_start(&node->vdev, &dev->pipe.pipe);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "start pipeline failed %d\n", ret);
		goto pipe_stream_off;
	}
	tasklet_enable(&stream->buf_done_tasklet);
	mutex_unlock(&dev->hw_dev->dev_lock);
	return 0;

pipe_stream_off:
	dev->pipe.set_stream(&dev->pipe, false);
stop_stream:
	rkisp_stream_stop(stream);
close_pipe:
	dev->pipe.close(&dev->pipe);
buffer_done:
	destroy_buf_queue(stream, VB2_BUF_STATE_QUEUED);
	atomic_dec(&dev->cap_dev.refcnt);
	stream->streaming = false;
	mutex_unlock(&dev->hw_dev->dev_lock);
	return ret;
}

static const struct vb2_ops rkisp_vb2_ops = {
	.queue_setup = rkisp_queue_setup,
	.buf_queue = rkisp_buf_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.stop_streaming = rkisp_stop_streaming,
	.start_streaming = rkisp_start_streaming,
};

static int rkisp_init_vb2_queue(struct vb2_queue *q,
				struct rkisp_stream *stream,
				enum v4l2_buf_type buf_type)
{
	q->type = buf_type;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->drv_priv = stream;
	q->ops = &rkisp_vb2_ops;
	q->mem_ops = stream->ispdev->hw_dev->mem_ops;
	q->buf_struct_size = sizeof(struct rkisp_buffer);
	q->min_buffers_needed = CIF_ISP_REQ_BUFS_MIN;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->lock = &stream->apilock;
	q->dev = stream->ispdev->hw_dev->dev;
	q->allow_cache_hints = 1;
	q->bidirectional = 1;
	if (stream->ispdev->hw_dev->is_dma_contig)
		q->dma_attrs = DMA_ATTR_FORCE_CONTIGUOUS;
	q->gfp_flags = GFP_DMA32;
	return vb2_queue_init(q);
}

static int rkisp_stream_init(struct rkisp_device *dev, u32 id)
{
	struct rkisp_capture_device *cap_dev = &dev->cap_dev;
	struct rkisp_stream *stream;
	struct video_device *vdev;
	struct rkisp_vdev_node *node;
	int ret = 0;

	stream = &cap_dev->stream[id];
	stream->id = id;
	stream->ispdev = dev;
	vdev = &stream->vnode.vdev;

	INIT_LIST_HEAD(&stream->buf_queue);
	init_waitqueue_head(&stream->done);
	spin_lock_init(&stream->vbq_lock);
	stream->linked = true;

	switch (id) {
	case RKISP_STREAM_SP:
		strscpy(vdev->name, SP_VDEV_NAME, sizeof(vdev->name));
		stream->ops = &rkisp_sp_streams_ops;
		stream->config = &rkisp_sp_stream_cfg;
		if (dev->hw_dev->unite)
			stream->config->max_rsz_width *= 2;
		break;
	case RKISP_STREAM_VIR:
		strscpy(vdev->name, VIR_VDEV_NAME, sizeof(vdev->name));
		stream->ops = NULL;
		stream->config = &rkisp_mp_stream_cfg;
		stream->conn_id = -1;
		break;
	case RKISP_STREAM_LDC:
		stream->linked = false;
		strscpy(vdev->name, LDC_VDEV_NAME, sizeof(vdev->name));
		stream->ops = &rkisp_ldc_streams_ops;
		stream->config = &rkisp_ldc_stream_cfg;
		break;
	default:
		strscpy(vdev->name, MP_VDEV_NAME, sizeof(vdev->name));
		stream->ops = &rkisp_mp_streams_ops;
		stream->config = &rkisp_mp_stream_cfg;
		if (dev->hw_dev->unite) {
			stream->config->max_rsz_width = CIF_ISP_INPUT_W_MAX_V39_UNITE;
			stream->config->max_rsz_height = CIF_ISP_INPUT_H_MAX_V39_UNITE;
		}
	}

	node = vdev_to_node(vdev);
	rkisp_init_vb2_queue(&node->buf_queue, stream,
			     V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	ret = rkisp_register_stream_vdev(stream);
	if (ret < 0)
		return ret;

	stream->streaming = false;
	stream->interlaced = false;
	stream->burst =
		CIF_MI_CTRL_BURST_LEN_LUM_16 |
		CIF_MI_CTRL_BURST_LEN_CHROM_16;
	atomic_set(&stream->sequence, 0);
	return 0;
}

int rkisp_register_stream_v39(struct rkisp_device *dev)
{
	struct rkisp_capture_device *cap_dev = &dev->cap_dev;
	int ret;

	ret = rkisp_stream_init(dev, RKISP_STREAM_MP);
	if (ret < 0)
		goto err;
	ret = rkisp_stream_init(dev, RKISP_STREAM_SP);
	if (ret < 0)
		goto err_free_mp;
	ret = rkisp_stream_init(dev, RKISP_STREAM_LDC);
	if (ret < 0)
		goto err_free_sp;
	ret = rkisp_stream_init(dev, RKISP_STREAM_VIR);
	if (ret < 0)
		goto err_free_ldc;
	return 0;
err_free_ldc:
	rkisp_unregister_stream_vdev(&cap_dev->stream[RKISP_STREAM_LDC]);
err_free_sp:
	rkisp_unregister_stream_vdev(&cap_dev->stream[RKISP_STREAM_SP]);
err_free_mp:
	rkisp_unregister_stream_vdev(&cap_dev->stream[RKISP_STREAM_MP]);
err:
	return ret;
}

void rkisp_unregister_stream_v39(struct rkisp_device *dev)
{
	struct rkisp_capture_device *cap_dev = &dev->cap_dev;
	struct rkisp_stream *stream;

	stream = &cap_dev->stream[RKISP_STREAM_MP];
	rkisp_unregister_stream_vdev(stream);
	stream = &cap_dev->stream[RKISP_STREAM_SP];
	rkisp_unregister_stream_vdev(stream);
	stream = &cap_dev->stream[RKISP_STREAM_LDC];
	rkisp_unregister_stream_vdev(stream);
	stream = &cap_dev->stream[RKISP_STREAM_VIR];
	rkisp_unregister_stream_vdev(stream);
}

/****************  Interrupter Handler ****************/

void rkisp_mi_v39_isr(u32 mis_val, struct rkisp_device *dev)
{
	struct rkisp_stream *stream;
	unsigned int i;

	v4l2_dbg(3, rkisp_debug, &dev->v4l2_dev,
		 "mi isr:0x%x\n", mis_val);

	if ((dev->unite_div == ISP_UNITE_DIV2 && dev->unite_index != ISP_UNITE_RIGHT) ||
	    (dev->unite_div == ISP_UNITE_DIV4 && dev->unite_index != ISP_UNITE_RIGHT_B)) {
		rkisp_write(dev, ISP3X_MI_ICR, mis_val, true);
		goto end;
	}

	for (i = 0; i <= RKISP_STREAM_SP; ++i) {
		stream = &dev->cap_dev.stream[i];

		if (!(mis_val & CIF_MI_FRAME(stream)))
			continue;

		mi_frame_end_int_clear(stream);

		if (stream->stopping) {
			/*
			 * Make sure stream is actually stopped, whose state
			 * can be read from the shadow register, before
			 * wake_up() thread which would immediately free all
			 * frame buffers. disable_mi() takes effect at the next
			 * frame end that sync the configurations to shadow
			 * regs.
			 */
			if (!dev->hw_dev->is_single) {
				stream->stopping = false;
				stream->streaming = false;
				stream->ops->disable_mi(stream);
				wake_up(&stream->done);
			} else if (stream->ops->is_stream_stopped(stream)) {
				stream->stopping = false;
				stream->streaming = false;
				wake_up(&stream->done);
			}
		} else {
			mi_frame_end(stream, FRAME_IRQ);
		}
	}
end:
	if (mis_val & ISP3X_MI_MP_FRAME) {
		stream = &dev->cap_dev.stream[RKISP_STREAM_MP];
		if (!stream->streaming)
			dev->irq_ends_mask &= ~ISP_FRAME_MP;
		rkisp_check_idle(dev, ISP_FRAME_MP);
	}
	if (mis_val & ISP3X_MI_SP_FRAME) {
		stream = &dev->cap_dev.stream[RKISP_STREAM_SP];
		if (!stream->streaming)
			dev->irq_ends_mask &= ~ISP_FRAME_SP;
		rkisp_check_idle(dev, ISP_FRAME_SP);
	}
}

void rkisp_stream_ldc_end_v39(struct rkisp_device *dev)
{
	struct rkisp_stream *stream = &dev->cap_dev.stream[RKISP_STREAM_LDC];
	u32 val = rkisp_read(dev, ISP39_LDCV_CTRL, true);

	/* ldcv_irq: ldcv enable is frame end other frame input */
	if (val & ISP39_LDCV_MAP_ERROR) {
		v4l2_err(&dev->v4l2_dev, "ldcv map data error\n");
		return;
	}
	if (stream->stopping) {
		if (!dev->hw_dev->is_single) {
			stream->stopping = false;
			stream->streaming = false;
			stream->ops->disable_mi(stream);
			wake_up(&stream->done);
		} else if (stream->ops->is_stream_stopped(stream)) {
			stream->stopping = false;
			stream->streaming = false;
			wake_up(&stream->done);
		}
	} else if (stream->streaming) {
		mi_frame_end(stream, FRAME_IRQ);
	}
	rkisp_check_idle(dev, ISP_FRAME_LDC);
}
