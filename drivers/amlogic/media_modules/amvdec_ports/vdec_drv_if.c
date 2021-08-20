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
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "vdec_drv_if.h"
#include "aml_vcodec_dec.h"
#include "vdec_drv_base.h"

const struct vdec_common_if *get_h264_dec_comm_if(void);
const struct vdec_common_if *get_hevc_dec_comm_if(void);
const struct vdec_common_if *get_vp9_dec_comm_if(void);
const struct vdec_common_if *get_mpeg12_dec_comm_if(void);
const struct vdec_common_if *get_mpeg4_dec_comm_if(void);
const struct vdec_common_if *get_mjpeg_dec_comm_if(void);
const struct vdec_common_if *get_av1_dec_comm_if(void);
const struct vdec_common_if *get_avs_dec_comm_if(void);
const struct vdec_common_if *get_avs2_dec_comm_if(void);

int vdec_if_init(struct aml_vcodec_ctx *ctx, unsigned int fourcc)
{
	int ret = 0;

	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
		ctx->dec_if = get_h264_dec_comm_if();
		break;
	case V4L2_PIX_FMT_HEVC:
		ctx->dec_if = get_hevc_dec_comm_if();
		break;
	case V4L2_PIX_FMT_VP9:
		ctx->dec_if = get_vp9_dec_comm_if();
		break;
	case V4L2_PIX_FMT_MPEG:
	case V4L2_PIX_FMT_MPEG1:
	case V4L2_PIX_FMT_MPEG2:
		ctx->dec_if = get_mpeg12_dec_comm_if();
		break;
	case V4L2_PIX_FMT_MPEG4:
		ctx->dec_if = get_mpeg4_dec_comm_if();
		break;
	case V4L2_PIX_FMT_MJPEG:
		ctx->dec_if = get_mjpeg_dec_comm_if();
		break;
	case V4L2_PIX_FMT_AV1:
		ctx->dec_if = get_av1_dec_comm_if();
		break;
	case V4L2_PIX_FMT_AVS:
		ctx->dec_if = get_avs_dec_comm_if();
		break;
	case V4L2_PIX_FMT_AVS2:
		ctx->dec_if = get_avs2_dec_comm_if();
		break;
	default:
		return -EINVAL;
	}

	ret = ctx->dec_if->init(ctx, &ctx->drv_handle);

	return ret;
}

int vdec_if_probe(struct aml_vcodec_ctx *ctx,
	struct aml_vcodec_mem *bs, void *out)
{
	int ret = 0;

	ret = ctx->dec_if->probe(ctx->drv_handle, bs, out);

	return ret;
}

int vdec_if_decode(struct aml_vcodec_ctx *ctx,
		   struct aml_vcodec_mem *bs, bool *res_chg)
{
	int ret = 0;

	if (bs) {
		if ((bs->addr & 63) != 0) {
			v4l_dbg(ctx, V4L_DEBUG_CODEC_ERROR,
				"bs dma_addr should 64 byte align\n");
			return -EINVAL;
		}
	}

	if (ctx->drv_handle == 0)
		return -EIO;

	aml_vcodec_set_curr_ctx(ctx->dev, ctx);
	ret = ctx->dec_if->decode(ctx->drv_handle, bs, res_chg);
	aml_vcodec_set_curr_ctx(ctx->dev, NULL);

	return ret;
}

int vdec_if_get_param(struct aml_vcodec_ctx *ctx,
	enum vdec_get_param_type type, void *out)
{
	int ret = 0;

	if (ctx->drv_handle == 0)
		return -EIO;

	ret = ctx->dec_if->get_param(ctx->drv_handle, type, out);

	return ret;
}

int vdec_if_set_param(struct aml_vcodec_ctx *ctx,
	enum vdec_set_param_type type, void *in)
{
	int ret = 0;

	if (ctx->drv_handle == 0)
		return -EIO;

	ret = ctx->dec_if->set_param(ctx->drv_handle, type, in);

	return ret;
}

void vdec_if_deinit(struct aml_vcodec_ctx *ctx)
{
	if (ctx->drv_handle == 0)
		return;

	ctx->dec_if->deinit(ctx->drv_handle);
	ctx->drv_handle = 0;
}
