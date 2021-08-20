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
 * struct avs2_fb - avs2 decode frame buffer information
 * @vdec_fb_va  : virtual address of struct vdec_fb
 * @y_fb_dma    : dma address of Y frame buffer (luma)
 * @c_fb_dma    : dma address of C frame buffer (chroma)
 * @poc         : picture order count of frame buffer
 * @reserved    : for 8 bytes alignment
 */
struct avs2_fb {
	uint64_t vdec_fb_va;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;
	int32_t poc;
	uint32_t reserved;
};

/**
 * struct vdec_avs2_dec_info - decode information
 * @dpb_sz		: decoding picture buffer size
 * @resolution_changed  : resoltion change happen
 * @reserved		: for 8 bytes alignment
 * @bs_dma		: Input bit-stream buffer dma address
 * @y_fb_dma		: Y frame buffer dma address
 * @c_fb_dma		: C frame buffer dma address
 * @vdec_fb_va		: VDEC frame buffer struct virtual address
 */
struct vdec_avs2_dec_info {
	uint32_t dpb_sz;
	uint32_t resolution_changed;
	uint32_t reserved;
	uint64_t bs_dma;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;
	uint64_t vdec_fb_va;
};

/**
 * struct vdec_avs2_vsi - shared memory for decode information exchange
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
struct vdec_avs2_vsi {
	char *header_buf;
	int sps_size;
	int pps_size;
	int sei_size;
	int head_offset;
	struct vdec_avs2_dec_info dec;
	struct vdec_pic_info pic;
	struct vdec_pic_info cur_pic;
	struct v4l2_rect crop;
	bool is_combine;
	int nalu_pos;
};

/**
 * struct vdec_avs2_inst - avs2 decoder instance
 * @num_nalu : how many nalus be decoded
 * @ctx      : point to aml_vcodec_ctx
 * @vsi      : VPU shared information
 */
struct vdec_avs2_inst {
	unsigned int num_nalu;
	struct aml_vcodec_ctx *ctx;
	struct aml_vdec_adapt vdec;
	struct vdec_avs2_vsi *vsi;
	struct aml_dec_params parms;
	struct completion comp;
	struct vdec_comp_buf_info comp_info;
};


static int vdec_write_nalu(struct vdec_avs2_inst *inst,
	u8 *buf, u32 size, u64 ts);

static int vdec_get_dw_mode(struct vdec_avs2_inst *inst, int dw_mode);

static void get_pic_info(struct vdec_avs2_inst *inst,
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

static void get_crop_info(struct vdec_avs2_inst *inst, struct v4l2_rect *cr)
{
	cr->left = inst->vsi->crop.left;
	cr->top = inst->vsi->crop.top;
	cr->width = inst->vsi->crop.width;
	cr->height = inst->vsi->crop.height;

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO,
		"l=%d, t=%d, w=%d, h=%d\n",
		 cr->left, cr->top, cr->width, cr->height);
}

static void get_dpb_size(struct vdec_avs2_inst *inst, unsigned int *dpb_sz)
{
	*dpb_sz = inst->vsi->dec.dpb_sz;
	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO, "sz=%d\n", *dpb_sz);
}

static u32 vdec_config_default_parms(u8 *parm)
{
	u8 *pbuf = parm;

	pbuf += sprintf(pbuf, "parm_v4l_codec_enable:1;");
	pbuf += sprintf(pbuf, "parm_v4l_buffer_margin:7;");
	pbuf += sprintf(pbuf, "avs2_double_write_mode:3;");
	pbuf += sprintf(pbuf, "avs2_buf_width:1920;");
	pbuf += sprintf(pbuf, "avs2_buf_height:1088;");
	pbuf += sprintf(pbuf, "avs2_max_pic_w:8192;");
	pbuf += sprintf(pbuf, "avs2_max_pic_h:4608;");
	pbuf += sprintf(pbuf, "save_buffer_mode:0;");
	pbuf += sprintf(pbuf, "no_head:0;");
	pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_mode:0;");
	pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_endian:0;");

	return parm - pbuf;
}

static void vdec_parser_parms(struct vdec_avs2_inst *inst)
{
	struct aml_vcodec_ctx *ctx = inst->ctx;

	v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
		"%s:parms_status = 0x%x, present_flag = %d\n",
		__func__, ctx->config.parm.dec.parms_status,
		ctx->config.parm.dec.hdr.color_parms.present_flag);
	if (ctx->config.parm.dec.parms_status &
		V4L2_CONFIG_PARM_DECODE_CFGINFO) {
		u8 *pbuf = ctx->config.buf;

		pbuf += sprintf(pbuf, "parm_v4l_codec_enable:1;");
		pbuf += sprintf(pbuf, "parm_v4l_buffer_margin:%d;",
			ctx->config.parm.dec.cfg.ref_buf_margin);
		pbuf += sprintf(pbuf, "avs2_double_write_mode:%d;",
			ctx->config.parm.dec.cfg.double_write_mode);
		pbuf += sprintf(pbuf, "avs2_buf_width:1920;");
		pbuf += sprintf(pbuf, "avs2_buf_height:1088;");
		pbuf += sprintf(pbuf, "save_buffer_mode:0;");
		pbuf += sprintf(pbuf, "no_head:0;");
		pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_mode:%d;",
			ctx->config.parm.dec.cfg.canvas_mem_mode);
		pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_endian:%d;",
			ctx->config.parm.dec.cfg.canvas_mem_endian);
		pbuf += sprintf(pbuf, "parm_v4l_low_latency_mode:%d;",
			ctx->config.parm.dec.cfg.low_latency_mode);
		ctx->config.length = pbuf - ctx->config.buf;
	} else {
		ctx->config.parm.dec.cfg.double_write_mode = 3;
		ctx->config.parm.dec.cfg.ref_buf_margin = 7;
		ctx->config.length = vdec_config_default_parms(ctx->config.buf);
	}

	if ((ctx->config.parm.dec.parms_status &
		V4L2_CONFIG_PARM_DECODE_HDRINFO) &&
		ctx->config.parm.dec.hdr.color_parms.present_flag) {
		u8 *pbuf = ctx->config.buf + ctx->config.length;

		pbuf += sprintf(pbuf, "HDRStaticInfo:%d;", 1);
		pbuf += sprintf(pbuf, "signal_type:%d;",
			ctx->config.parm.dec.hdr.signal_type);
		pbuf += sprintf(pbuf, "mG.x:%d;",
			ctx->config.parm.dec.hdr.color_parms.primaries[0][0]);
		pbuf += sprintf(pbuf, "mG.y:%d;",
			ctx->config.parm.dec.hdr.color_parms.primaries[0][1]);
		pbuf += sprintf(pbuf, "mB.x:%d;",
			ctx->config.parm.dec.hdr.color_parms.primaries[1][0]);
		pbuf += sprintf(pbuf, "mB.y:%d;",
			ctx->config.parm.dec.hdr.color_parms.primaries[1][1]);
		pbuf += sprintf(pbuf, "mR.x:%d;",
			ctx->config.parm.dec.hdr.color_parms.primaries[2][0]);
		pbuf += sprintf(pbuf, "mR.y:%d;",
			ctx->config.parm.dec.hdr.color_parms.primaries[2][1]);
		pbuf += sprintf(pbuf, "mW.x:%d;",
			ctx->config.parm.dec.hdr.color_parms.white_point[0]);
		pbuf += sprintf(pbuf, "mW.y:%d;",
			ctx->config.parm.dec.hdr.color_parms.white_point[1]);
		pbuf += sprintf(pbuf, "mMaxDL:%d;",
			ctx->config.parm.dec.hdr.color_parms.luminance[0] * 1000);
		pbuf += sprintf(pbuf, "mMinDL:%d;",
			ctx->config.parm.dec.hdr.color_parms.luminance[1]);
		pbuf += sprintf(pbuf, "mMaxCLL:%d;",
			ctx->config.parm.dec.hdr.color_parms.content_light_level.max_content);
		pbuf += sprintf(pbuf, "mMaxFALL:%d;",
			ctx->config.parm.dec.hdr.color_parms.content_light_level.max_pic_average);
		ctx->config.length	= pbuf - ctx->config.buf;
		inst->parms.hdr		= ctx->config.parm.dec.hdr;
		inst->parms.parms_status |= V4L2_CONFIG_PARM_DECODE_HDRINFO;
	}
	v4l_dbg(ctx, V4L_DEBUG_CODEC_EXINFO,
		"config.buf = %s\n", ctx->config.buf);

	inst->vdec.config	= ctx->config;
	inst->parms.cfg		= ctx->config.parm.dec.cfg;
	inst->parms.parms_status |= V4L2_CONFIG_PARM_DECODE_CFGINFO;
}

static int vdec_avs2_init(struct aml_vcodec_ctx *ctx, unsigned long *h_vdec)
{
	struct vdec_avs2_inst *inst = NULL;
	int ret = -1;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;

	inst->vdec.frm_name	= "AVS2";
	inst->vdec.video_type	= VFORMAT_AVS2;
	inst->vdec.filp		= ctx->dev->filp;
	inst->vdec.ctx		= ctx;
	inst->ctx		= ctx;

	vdec_parser_parms(inst);

	/* set play mode.*/
	if (ctx->is_drm_mode)
		inst->vdec.port.flag |= PORT_FLAG_DRM;

	/* to eable avs2 hw.*/
	inst->vdec.port.type	= PORT_TYPE_HEVC;

	/* probe info from the stream */
	inst->vsi = kzalloc(sizeof(struct vdec_avs2_vsi), GFP_KERNEL);
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
			"vdec_avs2 init err=%d\n", ret);
		goto err;
	}

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO,
		"avs2 Instance >> %lx\n", (ulong) inst);

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

static int parse_stream_ucode(struct vdec_avs2_inst *inst,
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

static int parse_stream_ucode_dma(struct vdec_avs2_inst *inst,
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

static int parse_stream_cpu(struct vdec_avs2_inst *inst, u8 *buf, u32 size)
{
	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
		"can not suppport parse stream by cpu.\n");

	return -1;
}

static int vdec_avs2_probe(unsigned long h_vdec,
	struct aml_vcodec_mem *bs, void *out)
{
	struct vdec_avs2_inst *inst =
		(struct vdec_avs2_inst *)h_vdec;
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

static void vdec_avs2_deinit(unsigned long h_vdec)
{
	struct vdec_avs2_inst *inst = (struct vdec_avs2_inst *)h_vdec;
	struct aml_vcodec_ctx *ctx = inst->ctx;

	video_decoder_release(&inst->vdec);

	if (inst->vsi && inst->vsi->header_buf)
		vfree(inst->vsi->header_buf);

	if (inst->vsi)
		kfree(inst->vsi);

	kfree(inst);

	ctx->drv_handle = 0;
}

static int vdec_write_nalu(struct vdec_avs2_inst *inst,
	u8 *buf, u32 size, u64 ts)
{
	int ret = 0;
	struct aml_vdec_adapt *vdec = &inst->vdec;

	ret = vdec_vframe_write(vdec, buf, size, ts, 0);

	return ret;
}

static bool monitor_res_change(struct vdec_avs2_inst *inst, u8 *buf, u32 size)
{
	int ret = -1;
	u8 *p = buf;
	int len = size;
	u32 synccode =
		((p[17] << 16) | (p[18] << 8) | p[19]);

	if (synccode == SYNC_CODE) {
		ret = parse_stream_cpu(inst, p, len);
		if (!ret && (inst->vsi->cur_pic.coded_width !=
			inst->vsi->pic.coded_width ||
			inst->vsi->cur_pic.coded_height !=
			inst->vsi->pic.coded_height)) {
			inst->vsi->cur_pic = inst->vsi->pic;
			return true;
		}
	}

	return false;
}

static int vdec_avs2_decode(unsigned long h_vdec,
			   struct aml_vcodec_mem *bs, bool *res_chg)
{
	struct vdec_avs2_inst *inst = (struct vdec_avs2_inst *)h_vdec;
	struct aml_vdec_adapt *vdec = &inst->vdec;
	u8 *buf = (u8 *) bs->vaddr;
	u32 size = bs->size;
	int ret = -1;

	if (bs == NULL)
		return -1;

	if (vdec_input_full(vdec)) {
		return -EAGAIN;
	}

	if (inst->ctx->is_drm_mode) {
		if (bs->model == VB2_MEMORY_MMAP) {
			struct aml_video_stream *s =
				(struct aml_video_stream *) buf;

			if (s->magic != AML_VIDEO_MAGIC)
				return -1;

			if (!inst->ctx->param_sets_from_ucode &&
				(s->type == V4L_STREAM_TYPE_MATEDATA)) {
				if ((*res_chg = monitor_res_change(inst,
					s->data, s->len)))
				return 0;
			}

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
		/*checked whether the resolution changes.*/
		if ((!inst->ctx->param_sets_from_ucode) &&
			(*res_chg = monitor_res_change(inst, buf, size)))
			return 0;

		ret = vdec_write_nalu(inst, buf, size, bs->timestamp);
	}

	return ret;
}

 static void get_param_config_info(struct vdec_avs2_inst *inst,
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

static void get_param_comp_buf_info(struct vdec_avs2_inst *inst,
		struct vdec_comp_buf_info *params)
{
	memcpy(params, &inst->comp_info, sizeof(*params));
}

static int vdec_avs2_get_param(unsigned long h_vdec,
			       enum vdec_get_param_type type, void *out)
{
	int ret = 0;
	struct vdec_avs2_inst *inst = (struct vdec_avs2_inst *)h_vdec;

	if (!inst) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"the avs2 inst of dec is invalid.\n");
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
		u32 m = inst->ctx->config.parm.dec.cfg.double_write_mode;
		if (m <= 16)
			*mode = inst->ctx->config.parm.dec.cfg.double_write_mode;
		else
			*mode = vdec_get_dw_mode(inst, 0);
		break;
	}
	case GET_PARAM_COMP_BUF_INFO:
		get_param_comp_buf_info(inst, out);
		break;

	default:
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"invalid get parameter type=%d\n", type);
		ret = -EINVAL;
	}

	return ret;
}

static void set_param_write_sync(struct vdec_avs2_inst *inst)
{
	complete(&inst->comp);
}

static int vdec_get_dw_mode(struct vdec_avs2_inst *inst, int dw_mode)
{
	u32 valid_dw_mode = inst->parms.cfg.double_write_mode;
	int w = inst->vsi->pic.coded_width;
	int h = inst->vsi->pic.coded_height;
	u32 dw = 0x1; /*1:1*/

	switch (valid_dw_mode) {
	case 0x100:
		if (is_over_size(w, h, 1920 * 1088))
			dw = 0x4; /*1:2*/
		break;
	case 0x200:
		if (is_over_size(w, h, 1920 * 1088))
			dw = 0x2; /*1:4*/
		break;
	case 0x300:
		if (is_over_size(w, h, 1280 * 768))
			dw = 0x4; /*1:2*/
		break;
	default:
		dw = valid_dw_mode;
		break;
	}

	return dw;
}

static int vdec_pic_scale(struct vdec_avs2_inst *inst, int length, int dw_mode)
{
	int ret = 64;

	switch (vdec_get_dw_mode(inst, dw_mode)) {
	case 0x0: /* only afbc, output afbc */
		ret = 64;
		break;
	case 0x1: /* afbc and (w x h), output YUV420 */
		ret = length;
		break;
	case 0x2: /* afbc and (w/4 x h/4), output YUV420 */
	case 0x3: /* afbc and (w/4 x h/4), output afbc and YUV420 */
		ret = length >> 2;
		break;
	case 0x4: /* afbc and (w/2 x h/2), output YUV420 */
		ret = length >> 1;
		break;
	case 0x10: /* (w x h), output YUV420-8bit) */
	default:
		ret = length;
		break;
	}

	return ret;
}

static void set_param_ps_info(struct vdec_avs2_inst *inst,
	struct aml_vdec_ps_infos *ps)
{
	struct vdec_pic_info *pic = &inst->vsi->pic;
	struct vdec_avs2_dec_info *dec = &inst->vsi->dec;
	struct v4l2_rect *rect = &inst->vsi->crop;
	int dw = inst->parms.cfg.double_write_mode;

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

	pic->y_len_sz		= ALIGN(vdec_pic_scale(inst, pic->coded_width, dw), 64) *
				  ALIGN(vdec_pic_scale(inst, pic->coded_height, dw), 64);
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
		"Parse from ucode, visible(%d x %d), coded(%d x %d)\n",
		ps->visible_width, ps->visible_height,
		ps->coded_width, ps->coded_height);
}

static void set_param_comp_buf_info(struct vdec_avs2_inst *inst,
		struct vdec_comp_buf_info *info)
{
	memcpy(&inst->comp_info, info, sizeof(*info));
}

static void set_param_hdr_info(struct vdec_avs2_inst *inst,
	struct aml_vdec_hdr_infos *hdr)
{
	if ((inst->parms.parms_status &
		V4L2_CONFIG_PARM_DECODE_HDRINFO)) {
		inst->parms.hdr = *hdr;
		inst->parms.parms_status |=
			V4L2_CONFIG_PARM_DECODE_HDRINFO;
		aml_vdec_dispatch_event(inst->ctx,
			V4L2_EVENT_SRC_CH_HDRINFO);
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO,
			"avs2 set HDR infos\n");
	}
}

static void set_param_post_event(struct vdec_avs2_inst *inst, u32 *event)
{
		aml_vdec_dispatch_event(inst->ctx, *event);
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO,
			"avs2 post event: %d\n", *event);
}

static void set_pic_info(struct vdec_avs2_inst *inst,
	struct vdec_pic_info *pic)
{
	inst->vsi->pic = *pic;
}

static int vdec_avs2_set_param(unsigned long h_vdec,
	enum vdec_set_param_type type, void *in)
{
	int ret = 0;
	struct vdec_avs2_inst *inst = (struct vdec_avs2_inst *)h_vdec;

	if (!inst) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"the avs2 inst of dec is invalid.\n");
		return -1;
	}

	switch (type) {
	case SET_PARAM_WRITE_FRAME_SYNC:
		set_param_write_sync(inst);
		break;

	case SET_PARAM_PS_INFO:
		set_param_ps_info(inst, in);
		break;

	case SET_PARAM_COMP_BUF_INFO:
		set_param_comp_buf_info(inst, in);
		break;

	case SET_PARAM_HDR_INFO:
		set_param_hdr_info(inst, in);
		break;

	case SET_PARAM_POST_EVENT:
		set_param_post_event(inst, in);
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

static struct vdec_common_if vdec_avs2_if = {
	.init		= vdec_avs2_init,
	.probe		= vdec_avs2_probe,
	.decode		= vdec_avs2_decode,
	.get_param	= vdec_avs2_get_param,
	.set_param	= vdec_avs2_set_param,
	.deinit		= vdec_avs2_deinit,
};

struct vdec_common_if *get_avs2_dec_comm_if(void)
{
	return &vdec_avs2_if;
}

