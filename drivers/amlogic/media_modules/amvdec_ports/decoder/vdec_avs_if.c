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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <uapi/linux/swab.h>
#include "../vdec_drv_if.h"
#include "../aml_vcodec_util.h"
#include "../aml_vcodec_dec.h"
#include "../aml_vcodec_drv.h"
#include "../aml_vcodec_adapt.h"
#include "../vdec_drv_base.h"
#include "../utils/common.h"

#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_V4L2
#include <trace/events/meson_atrace.h>

#define PREFIX_SIZE	(16)

#define HEADER_BUFFER_SIZE			(32 * 1024)
#define SYNC_CODE				(0x498342)

/**
 * struct vdec_avs_dec_info - decode information
 * @dpb_sz		: decoding picture buffer size
 * @resolution_changed  : resoltion change happen
 * @reserved		: for 8 bytes alignment
 * @bs_dma		: Input bit-stream buffer dma address
 * @y_fb_dma		: Y frame buffer dma address
 * @c_fb_dma		: C frame buffer dma address
 * @vdec_fb_va		: VDEC frame buffer struct virtual address
 */
struct vdec_avs_dec_info {
	uint32_t dpb_sz;
	uint32_t resolution_changed;
	uint32_t reserved;
	uint64_t bs_dma;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;
	uint64_t vdec_fb_va;
};

/**
 * struct vdec_avs_vsi - shared memory for decode information exchange
 *                        between VPU and Host.
 *                        The memory is allocated by VPU then mapping to Host
 *                        in vpu_dec_init() and freed in vpu_dec_deinit()
 *                        by VPU.
 *                        AP-W/R : AP is writer/reader on this item
 *                        VPU-W/R: VPU is write/reader on this item
 * @hdr_buf      : Header parsing buffer (AP-W, VPU-R)
 * @list_free    : free frame buffer ring list (AP-W/R, VPU-W)
 * @list_disp    : display frame buffer ring list (AP-R, VPU-W)
 * @dec          : decode information (AP-R, VPU-W)
 * @pic          : picture information (AP-R, VPU-W)
 * @crop         : crop information (AP-R, VPU-W)
 */
struct vdec_avs_vsi {
	char *header_buf;
	int sps_size;
	int pps_size;
	int sei_size;
	int head_offset;
	struct vdec_avs_dec_info dec;
	struct vdec_pic_info pic;
	struct vdec_pic_info cur_pic;
	struct v4l2_rect crop;
	bool is_combine;
	int nalu_pos;
};

/**
 * struct vdec_avs_inst - avs decoder instance
 * @num_nalu : how many nalus be decoded
 * @ctx      : point to aml_vcodec_ctx
 * @vsi      : VPU shared information
 */
struct vdec_avs_inst {
	unsigned int num_nalu;
	struct aml_vcodec_ctx *ctx;
	struct aml_vdec_adapt vdec;
	struct vdec_avs_vsi *vsi;
	struct aml_dec_params parms;
	struct completion comp;
	struct vdec_comp_buf_info comp_info;
};

static u32 vdec_config_default_parms(u8 *parm)
{
	u8 *pbuf = parm;

	pbuf += sprintf(pbuf, "parm_v4l_codec_enable:1;");
	pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_mode:0;");
	pbuf += sprintf(pbuf, "parm_v4l_buffer_margin:0;");

	return parm - pbuf;
}

static void vdec_parser_parms(struct vdec_avs_inst *inst)
{
	struct aml_vcodec_ctx *ctx = inst->ctx;

	if (ctx->config.parm.dec.parms_status &
		V4L2_CONFIG_PARM_DECODE_CFGINFO) {
		u8 *pbuf = ctx->config.buf;

		pbuf += sprintf(pbuf, "parm_v4l_codec_enable:1;");
		pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_mode:%d;",
			ctx->config.parm.dec.cfg.canvas_mem_mode);
		pbuf += sprintf(pbuf, "parm_v4l_buffer_margin:%d;",
			ctx->config.parm.dec.cfg.ref_buf_margin);
		ctx->config.length = pbuf - ctx->config.buf;
	} else {
		ctx->config.length = vdec_config_default_parms(ctx->config.buf);
	}

	inst->vdec.config	= ctx->config;
	inst->parms.cfg		= ctx->config.parm.dec.cfg;
	inst->parms.parms_status |= V4L2_CONFIG_PARM_DECODE_CFGINFO;
}

static int vdec_avs_init(struct aml_vcodec_ctx *ctx, unsigned long *h_vdec)
{
	struct vdec_avs_inst *inst = NULL;
	int ret = -1;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;

	inst->vdec.frm_name	= "AVS";
	inst->vdec.video_type	= VFORMAT_AVS;
	inst->vdec.filp		= ctx->dev->filp;
	inst->vdec.ctx		= ctx;
	inst->ctx		= ctx;

	vdec_parser_parms(inst);

	/* set play mode.*/
	if (ctx->is_drm_mode)
		inst->vdec.port.flag |= PORT_FLAG_DRM;

	/* to eable avs hw.*/
	inst->vdec.port.type	= PORT_TYPE_VIDEO;

	/* probe info from the stream */
	inst->vsi = kzalloc(sizeof(struct vdec_avs_vsi), GFP_KERNEL);
	if (!inst->vsi) {
		ret = -ENOMEM;
		goto err;
	}

	/* alloc the header buffer to be used cache sps or spp etc.*/
	inst->vsi->header_buf = vzalloc(HEADER_BUFFER_SIZE);
	if (!inst->vsi->header_buf) {
		ret = -ENOMEM;
		goto err;
	}

	init_completion(&inst->comp);

	ctx->ada_ctx	= &inst->vdec;
	*h_vdec		= (unsigned long)inst;

	/* init decoder. */
	ret = video_decoder_init(&inst->vdec);
	if (ret) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"vdec_avs init err=%d\n", ret);
		goto err;
	}

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO,
		"avs Instance >> %lx\n", (ulong) inst);

	return 0;
err:
	if (inst && inst->vsi && inst->vsi->header_buf)
		vfree(inst->vsi->header_buf);
	if (inst && inst->vsi)
		kfree(inst->vsi);
	if (inst)
		kfree(inst);
	*h_vdec = 0;

	return ret;
}

static int vdec_write_nalu(struct vdec_avs_inst *inst,
	u8 *buf, u32 size, u64 ts)
{
	int ret = 0;
	struct aml_vdec_adapt *vdec = &inst->vdec;

	ret = vdec_vframe_write(vdec, buf, size, ts, 0);

	return ret;
}

static int parse_stream_ucode(struct vdec_avs_inst *inst,
			      u8 *buf, u32 size, u64 timestamp)
{
	int ret = 0;

	ret = vdec_write_nalu(inst, buf, size, timestamp);
	if (ret < 0) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"write data failed. size: %d, err: %d\n", size, ret);
		return ret;
	}

	/* wait ucode parse ending. */
	wait_for_completion_timeout(&inst->comp,
		msecs_to_jiffies(1000));

	return inst->vsi->pic.dpb_frames ? 0 : -1;
}
static int parse_stream_ucode_dma(struct vdec_avs_inst *inst,
	ulong buf, u32 size, u64 timestamp, u32 handle)
{
	int ret = 0;
	struct aml_vdec_adapt *vdec = &inst->vdec;

	ret = vdec_vframe_write_with_dma(vdec, buf, size, timestamp, handle,
		vdec_vframe_input_free, inst->ctx);
	if (ret < 0) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"write frame data failed. err: %d\n", ret);
		return ret;
	}

	/* wait ucode parse ending. */
	wait_for_completion_timeout(&inst->comp,
		msecs_to_jiffies(1000));

	return inst->vsi->pic.dpb_frames ? 0 : -1;
}

static int parse_stream_cpu(struct vdec_avs_inst *inst, u8 *buf, u32 size)
{
	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
		"can not suppport parse stream by cpu.\n");

	return -1;
}

static int vdec_avs_probe(unsigned long h_vdec,
	struct aml_vcodec_mem *bs, void *out)
{
	struct vdec_avs_inst *inst =
		(struct vdec_avs_inst *)h_vdec;
	u8 *buf = (u8 *)bs->vaddr;
	u32 size = bs->size;
	int ret = 0;

	if (inst->ctx->is_drm_mode) {
		if (bs->model == VB2_MEMORY_MMAP) {
			struct aml_video_stream *s =
				(struct aml_video_stream *) buf;

			if ((s->magic != AML_VIDEO_MAGIC) &&
				(s->type != V4L_STREAM_TYPE_MATEDATA))
				return -1;

			if (inst->ctx->param_sets_from_ucode) {
				ret = parse_stream_ucode(inst, s->data,
					s->len, bs->timestamp);
			} else {
				ret = parse_stream_cpu(inst, s->data, s->len);
			}
		} else if (bs->model == VB2_MEMORY_DMABUF ||
			bs->model == VB2_MEMORY_USERPTR) {
			ret = parse_stream_ucode_dma(inst, bs->addr, size,
				bs->timestamp, BUFF_IDX(bs, bs->index));
		}
	} else {
		if (inst->ctx->param_sets_from_ucode) {
			ret = parse_stream_ucode(inst, buf, size, bs->timestamp);
		} else {
			ret = parse_stream_cpu(inst, buf, size);
		}
	}

	inst->vsi->cur_pic = inst->vsi->pic;

	return ret;
}

static int vdec_avs_decode(unsigned long h_vdec,
			      struct aml_vcodec_mem *bs, bool *res_chg)
{
	struct vdec_avs_inst *inst = (struct vdec_avs_inst *)h_vdec;
	struct aml_vdec_adapt *vdec = &inst->vdec;
	u8 *buf = (u8 *) bs->vaddr;
	u32 size = bs->size;
	int ret = -1;

	if (vdec_input_full(vdec))
		return -EAGAIN;

	if (inst->ctx->is_drm_mode) {
		if (bs->model == VB2_MEMORY_MMAP) {
			struct aml_video_stream *s =
				(struct aml_video_stream *) buf;

			if (s->magic != AML_VIDEO_MAGIC)
				return -1;

			ret = vdec_vframe_write(vdec,
				s->data,
				s->len,
				bs->timestamp,
				0);
		} else if (bs->model == VB2_MEMORY_DMABUF ||
			bs->model == VB2_MEMORY_USERPTR) {
			ret = vdec_vframe_write_with_dma(vdec,
				bs->addr, size, bs->timestamp,
				BUFF_IDX(bs, bs->index),
				vdec_vframe_input_free, inst->ctx);
		}
	} else {
		ret = vdec_write_nalu(inst, buf, size, bs->timestamp);
	}

	return ret;
}

static void get_pic_info(struct vdec_avs_inst *inst,
			 struct vdec_pic_info *pic)
{
	*pic = inst->vsi->pic;

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO,
		"pic(%d, %d), buf(%d, %d)\n",
		 pic->visible_width, pic->visible_height,
		 pic->coded_width, pic->coded_height);
	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO,
		"Y(%d, %d), C(%d, %d)\n",
		pic->y_bs_sz, pic->y_len_sz,
		pic->c_bs_sz, pic->c_len_sz);
}

static void get_dpb_size(struct vdec_avs_inst *inst, unsigned int *dpb_sz)
{
	*dpb_sz = inst->vsi->dec.dpb_sz;
	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO, "sz=%d\n", *dpb_sz);
}

static void get_crop_info(struct vdec_avs_inst *inst, struct v4l2_rect *cr)
{
	cr->left = inst->vsi->crop.left;
	cr->top = inst->vsi->crop.top;
	cr->width = inst->vsi->crop.width;
	cr->height = inst->vsi->crop.height;

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO,
		"l=%d, t=%d, w=%d, h=%d\n",
		 cr->left, cr->top, cr->width, cr->height);
}

static void get_param_config_info(struct vdec_avs_inst *inst,
   struct aml_dec_params *parms)
{
	if (inst->parms.parms_status & V4L2_CONFIG_PARM_DECODE_CFGINFO)
		parms->cfg = inst->parms.cfg;
	if (inst->parms.parms_status & V4L2_CONFIG_PARM_DECODE_PSINFO)
		parms->ps = inst->parms.ps;
	if (inst->parms.parms_status & V4L2_CONFIG_PARM_DECODE_HDRINFO)
		parms->hdr = inst->parms.hdr;
	if (inst->parms.parms_status & V4L2_CONFIG_PARM_DECODE_CNTINFO)
		parms->cnt = inst->parms.cnt;

	parms->parms_status |= inst->parms.parms_status;

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO,
		"parms status: %u\n", parms->parms_status);
}

static int vdec_avs_get_param(unsigned long h_vdec,
			       enum vdec_get_param_type type, void *out)
{
	int ret = 0;
	struct vdec_avs_inst *inst = (struct vdec_avs_inst *)h_vdec;

	if (!inst) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"the avs inst of dec is invalid.\n");
		return -1;
	}

	switch (type) {
	case GET_PARAM_PIC_INFO:
		get_pic_info(inst, out);
		break;

	case GET_PARAM_DPB_SIZE:
		get_dpb_size(inst, out);
		break;

	case GET_PARAM_CROP_INFO:
		get_crop_info(inst, out);
		break;

	case GET_PARAM_CONFIG_INFO:
		get_param_config_info(inst, out);
		break;

	case GET_PARAM_DW_MODE:
	{
		u32 *mode = out;
		*mode = VDEC_DW_NO_AFBC;
		break;
	}

	default:
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"invalid get parameter type=%d\n", type);
		ret = -EINVAL;
	}

	return ret;
}

static void set_param_write_sync(struct vdec_avs_inst *inst)
{
	complete(&inst->comp);
}

static void set_param_ps_info(struct vdec_avs_inst *inst,
	struct aml_vdec_ps_infos *ps)
{
	struct vdec_pic_info *pic = &inst->vsi->pic;
	struct vdec_avs_dec_info *dec = &inst->vsi->dec;
	struct v4l2_rect *rect = &inst->vsi->crop;

	/* fill visible area size that be used for EGL. */
	pic->visible_width	= ps->visible_width;
	pic->visible_height	= ps->visible_height;

	/* calc visible ares. */
	rect->left		= 0;
	rect->top		= 0;
	rect->width		= pic->visible_width;
	rect->height		= pic->visible_height;

	/* config canvas size that be used for decoder. */
	pic->coded_width	= ps->coded_width;
	pic->coded_height	= ps->coded_height;

	pic->y_len_sz		= pic->coded_width * pic->coded_height;
	pic->c_len_sz		= pic->y_len_sz >> 1;

	/* calc DPB size */
	pic->dpb_frames		= ps->dpb_frames;
	pic->dpb_margin		= ps->dpb_margin;
	pic->vpp_margin		= ps->dpb_margin;
	dec->dpb_sz		= ps->dpb_size;
	pic->field		= ps->field;

	inst->parms.ps 	= *ps;
	inst->parms.parms_status |=
		V4L2_CONFIG_PARM_DECODE_PSINFO;

	/*wake up*/
	complete(&inst->comp);

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO,
		"Parse from ucode, visible(%d x %d), coded(%d x %d), scan:%s\n",
		ps->visible_width, ps->visible_height,
		ps->coded_width, ps->coded_height,
		pic->field == V4L2_FIELD_NONE ? "P" : "I");
}

static void set_pic_info(struct vdec_avs_inst *inst,
	struct vdec_pic_info *pic)
{
	inst->vsi->pic = *pic;
}

static int vdec_avs_set_param(unsigned long h_vdec,
	enum vdec_set_param_type type, void *in)
{
	int ret = 0;
	struct vdec_avs_inst *inst = (struct vdec_avs_inst *)h_vdec;

	if (!inst) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"the avs inst of dec is invalid.\n");
		return -1;
	}

	switch (type) {
	case SET_PARAM_WRITE_FRAME_SYNC:
		set_param_write_sync(inst);
		break;

	case SET_PARAM_PS_INFO:
		set_param_ps_info(inst, in);
		break;

	case SET_PARAM_PIC_INFO:
		set_pic_info(inst, in);
		break;

	default:
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"invalid set parameter type=%d\n", type);
		ret = -EINVAL;
	}

	return ret;
}

static void vdec_avs_deinit(unsigned long h_vdec)
{
	struct vdec_avs_inst *inst = (struct vdec_avs_inst *)h_vdec;
	struct aml_vcodec_ctx *ctx = inst->ctx;

	video_decoder_release(&inst->vdec);

	if (inst->vsi && inst->vsi->header_buf)
		vfree(inst->vsi->header_buf);

	if (inst->vsi)
		kfree(inst->vsi);

	kfree(inst);

	ctx->drv_handle = 0;
}

static struct vdec_common_if vdec_avs_if = {
	.init		= vdec_avs_init,
	.probe		= vdec_avs_probe,
	.decode		= vdec_avs_decode,
	.get_param	= vdec_avs_get_param,
	.set_param	= vdec_avs_set_param,
	.deinit		= vdec_avs_deinit,
};

struct vdec_common_if *get_avs_dec_comm_if(void)
{
	return &vdec_avs_if;
}

