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
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "vdec_drv_if.h"
#include "aml_vcodec_dec.h"
#include "vdec_drv_base.h"
#include "aml_vcodec_dec_pm.h"
//#include "aml_vpu.h"

const struct vdec_common_if *get_h264_dec_comm_if(void);
const struct vdec_common_if *get_vp8_dec_comm_if(void);
const struct vdec_common_if *get_vp9_dec_comm_if(void);

int vdec_if_init(struct aml_vcodec_ctx *ctx, unsigned int fourcc)
{
	int ret = 0;

	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
		ctx->dec_if = get_h264_dec_comm_if();
		break;
	/*case V4L2_PIX_FMT_VP8:
		ctx->dec_if = get_vp8_dec_comm_if();
		break;
	case V4L2_PIX_FMT_VP9:
		ctx->dec_if = get_vp9_dec_comm_if();
		break;*/
	default:
		return -EINVAL;
	}

	aml_vdec_lock(ctx);
	//aml_vcodec_dec_clock_on(&ctx->dev->pm);//debug_tmp
	ret = ctx->dec_if->init(ctx, &ctx->drv_handle);
	//aml_vcodec_dec_clock_off(&ctx->dev->pm);
	aml_vdec_unlock(ctx);

	return ret;
}

int vdec_if_probe(struct aml_vcodec_ctx *ctx,
	struct aml_vcodec_mem *bs, void *out)
{
	int ret = 0;

	aml_vdec_lock(ctx);
	ret = ctx->dec_if->probe(ctx->drv_handle, bs, out);
	aml_vdec_unlock(ctx);

	return ret;
}

int vdec_if_decode(struct aml_vcodec_ctx *ctx, struct aml_vcodec_mem *bs,
	unsigned long int timestamp, bool *res_chg)
{
	int ret = 0;

	if (bs) {
		if ((bs->dma_addr & 63) != 0) {
			aml_v4l2_err("bs dma_addr should 64 byte align");
			return -EINVAL;
		}
	}

	/*if (fb) {
		if (((fb->base_y.dma_addr & 511) != 0) ||
		    ((fb->base_c.dma_addr & 511) != 0)) {
			aml_v4l2_err("frame buffer dma_addr should 512 byte align");
			return -EINVAL;
		}
	}*/

	if (ctx->drv_handle == 0)
		return -EIO;

	//aml_vdec_lock(ctx);

	aml_vcodec_set_curr_ctx(ctx->dev, ctx);
	//aml_vcodec_dec_clock_on(&ctx->dev->pm);//debug_tmp
	//enable_irq(ctx->dev->dec_irq);
	ret = ctx->dec_if->decode(ctx->drv_handle, bs, timestamp, res_chg);
	//disable_irq(ctx->dev->dec_irq);
	//aml_vcodec_dec_clock_off(&ctx->dev->pm);
	aml_vcodec_set_curr_ctx(ctx->dev, NULL);

	//aml_vdec_unlock(ctx);

	return ret;
}

int vdec_if_get_param(struct aml_vcodec_ctx *ctx,
	enum vdec_get_param_type type, void *out)
{
	int ret = 0;

	if (ctx->drv_handle == 0)
		return -EIO;

	//aml_vdec_lock(ctx);
	ret = ctx->dec_if->get_param(ctx->drv_handle, type, out);
	//aml_vdec_unlock(ctx);

	return ret;
}

void vdec_if_deinit(struct aml_vcodec_ctx *ctx)
{
	if (ctx->drv_handle == 0)
		return;

	//aml_vdec_lock(ctx);
	//aml_vcodec_dec_clock_on(&ctx->dev->pm);
	ctx->dec_if->deinit(ctx->drv_handle);
	//aml_vcodec_dec_clock_off(&ctx->dev->pm);
	//aml_vdec_unlock(ctx);

	ctx->drv_handle = 0;
}
