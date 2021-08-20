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
#include "aml_h264_parser.h"
#include "../utils/common.h"

/* h264 NALU type */
#define NAL_NON_IDR_SLICE			0x01
#define NAL_IDR_SLICE				0x05
#define NAL_H264_SEI				0x06
#define NAL_H264_SPS				0x07
#define NAL_H264_PPS				0x08
#define NAL_H264_AUD				0x09

#define AVC_NAL_TYPE(value)				((value) & 0x1F)

#define BUF_PREDICTION_SZ			(64 * 1024)//(32 * 1024)

#define MB_UNIT_LEN				16

/* motion vector size (bytes) for every macro block */
#define HW_MB_STORE_SZ				64

#define H264_MAX_FB_NUM				17
#define HDR_PARSING_BUF_SZ			1024

#define HEADER_BUFFER_SIZE			(128 * 1024)

/**
 * struct h264_fb - h264 decode frame buffer information
 * @vdec_fb_va  : virtual address of struct vdec_fb
 * @y_fb_dma    : dma address of Y frame buffer (luma)
 * @c_fb_dma    : dma address of C frame buffer (chroma)
 * @poc         : picture order count of frame buffer
 * @reserved    : for 8 bytes alignment
 */
struct h264_fb {
	uint64_t vdec_fb_va;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;
	int32_t poc;
	uint32_t reserved;
};

/**
 * struct h264_ring_fb_list - ring frame buffer list
 * @fb_list   : frame buffer arrary
 * @read_idx  : read index
 * @write_idx : write index
 * @count     : buffer count in list
 */
struct h264_ring_fb_list {
	struct h264_fb fb_list[H264_MAX_FB_NUM];
	unsigned int read_idx;
	unsigned int write_idx;
	unsigned int count;
	unsigned int reserved;
};

/**
 * struct vdec_h264_dec_info - decode information
 * @dpb_sz		: decoding picture buffer size
 * @realloc_mv_buf	: flag to notify driver to re-allocate mv buffer
 * @reserved		: for 8 bytes alignment
 * @bs_dma		: Input bit-stream buffer dma address
 * @y_fb_dma		: Y frame buffer dma address
 * @c_fb_dma		: C frame buffer dma address
 * @vdec_fb_va		: VDEC frame buffer struct virtual address
 */
struct vdec_h264_dec_info {
	uint32_t dpb_sz;
	uint32_t realloc_mv_buf;
	uint32_t reserved;
	uint64_t bs_dma;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;
	uint64_t vdec_fb_va;
};

/**
 * struct vdec_h264_vsi - shared memory for decode information exchange
 *                        between VPU and Host.
 *                        The memory is allocated by VPU then mapping to Host
 *                        in vpu_dec_init() and freed in vpu_dec_deinit()
 *                        by VPU.
 *                        AP-W/R : AP is writer/reader on this item
 *                        VPU-W/R: VPU is write/reader on this item
 * @dec          : decode information (AP-R, VPU-W)
 * @pic          : picture information (AP-R, VPU-W)
 * @crop         : crop information (AP-R, VPU-W)
 */
struct vdec_h264_vsi {
	unsigned char hdr_buf[HDR_PARSING_BUF_SZ];
	char *header_buf;
	int sps_size;
	int pps_size;
	int sei_size;
	int head_offset;
	struct vdec_h264_dec_info dec;
	struct vdec_pic_info pic;
	struct vdec_pic_info cur_pic;
	struct v4l2_rect crop;
	bool is_combine;
	int nalu_pos;
};

/**
 * struct vdec_h264_inst - h264 decoder instance
 * @num_nalu : how many nalus be decoded
 * @ctx      : point to aml_vcodec_ctx
 * @pred_buf : HW working predication buffer
 * @mv_buf   : HW working motion vector buffer
 * @vpu      : VPU instance
 * @vsi      : VPU shared information
 */
struct vdec_h264_inst {
	unsigned int num_nalu;
	struct aml_vcodec_ctx *ctx;
	struct aml_vcodec_mem pred_buf;
	struct aml_vcodec_mem mv_buf[H264_MAX_FB_NUM];
	struct aml_vdec_adapt vdec;
	struct vdec_h264_vsi *vsi;
	struct aml_dec_params parms;
	struct completion comp;
	struct vdec_comp_buf_info comp_info;
};

#if 0
#define DUMP_FILE_NAME "/data/dump/dump.tmp"
static struct file *filp;
static loff_t file_pos;

void dump_write(const char __user *buf, size_t count)
{
	mm_segment_t old_fs;

	if (!filp)
		return;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (count != vfs_write(filp, buf, count, &file_pos))
		pr_err("Failed to write file\n");

	set_fs(old_fs);
}

void dump_init(void)
{
	filp = filp_open(DUMP_FILE_NAME, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(filp)) {
		pr_err("open dump file failed\n");
		filp = NULL;
	}
}

void dump_deinit(void)
{
	if (filp) {
		filp_close(filp, current->files);
		filp = NULL;
		file_pos = 0;
	}
}

void swap_uv(void *uv, int size)
{
	int i;
	__u16 *p = uv;

	size /= 2;

	for (i = 0; i < size; i++, p++)
		*p = __swab16(*p);
}
#endif

static void get_pic_info(struct vdec_h264_inst *inst,
			 struct vdec_pic_info *pic)
{
	*pic = inst->vsi->pic;

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO,
		"pic(%d, %d), buf(%d, %d)\n",
		 pic->visible_width, pic->visible_height,
		 pic->coded_width, pic->coded_height);
	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO,
		"Y(%d, %d), C(%d, %d)\n", pic->y_bs_sz,
		 pic->y_len_sz, pic->c_bs_sz, pic->c_len_sz);
}

static void get_crop_info(struct vdec_h264_inst *inst, struct v4l2_rect *cr)
{
	cr->left = inst->vsi->crop.left;
	cr->top = inst->vsi->crop.top;
	cr->width = inst->vsi->crop.width;
	cr->height = inst->vsi->crop.height;

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO,
		"l=%d, t=%d, w=%d, h=%d\n",
		 cr->left, cr->top, cr->width, cr->height);
}

static void get_dpb_size(struct vdec_h264_inst *inst, unsigned int *dpb_sz)
{
	*dpb_sz = inst->vsi->dec.dpb_sz;
	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO, "sz=%d\n", *dpb_sz);
}

static void skip_aud_data(u8 **data, u32 *size)
{
	int i;

	i = find_start_code(*data, *size);
	if (i > 0 && (*data)[i++] == 0x9 && (*data)[i++] == 0xf0) {
		*size -= i;
		*data += i;
	}
}

static u32 vdec_config_default_parms(u8 *parm)
{
	u8 *pbuf = parm;

	pbuf += sprintf(pbuf, "parm_v4l_codec_enable:1;");
	pbuf += sprintf(pbuf, "mh264_double_write_mode:16;");
	pbuf += sprintf(pbuf, "parm_v4l_buffer_margin:7;");
	pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_mode:0;");
	pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_endian:0;");

	return parm - pbuf;
}

static void vdec_parser_parms(struct vdec_h264_inst *inst)
{
	struct aml_vcodec_ctx *ctx = inst->ctx;

	if (ctx->config.parm.dec.parms_status &
		V4L2_CONFIG_PARM_DECODE_CFGINFO) {
		u8 *pbuf = ctx->config.buf;

		pbuf += sprintf(pbuf, "parm_v4l_codec_enable:1;");
		pbuf += sprintf(pbuf, "mh264_double_write_mode:%d;",
			ctx->config.parm.dec.cfg.double_write_mode);
		pbuf += sprintf(pbuf, "parm_v4l_buffer_margin:%d;",
			ctx->config.parm.dec.cfg.ref_buf_margin);
		pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_mode:%d;",
			ctx->config.parm.dec.cfg.canvas_mem_mode);
		pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_endian:%d;",
			ctx->config.parm.dec.cfg.canvas_mem_endian);
		pbuf += sprintf(pbuf, "parm_v4l_low_latency_mode:%d;",
			ctx->config.parm.dec.cfg.low_latency_mode);
		pbuf += sprintf(pbuf, "parm_v4l_metadata_config_flag:%d;",
			ctx->config.parm.dec.cfg.metadata_config_flag);
		pbuf += sprintf(pbuf, "parm_v4l_duration:%d;",
			ctx->config.parm.dec.cfg.duration);
		ctx->config.length = pbuf - ctx->config.buf;
	} else {
		ctx->config.parm.dec.cfg.double_write_mode = 16;
		ctx->config.parm.dec.cfg.ref_buf_margin = 7;
		ctx->config.length = vdec_config_default_parms(ctx->config.buf);
	}

	inst->vdec.config	= ctx->config;
	inst->parms.cfg		= ctx->config.parm.dec.cfg;
	inst->parms.parms_status |= V4L2_CONFIG_PARM_DECODE_CFGINFO;
}

static int vdec_h264_init(struct aml_vcodec_ctx *ctx, unsigned long *h_vdec)
{
	struct vdec_h264_inst *inst = NULL;
	int ret = -1;

	inst = vzalloc(sizeof(*inst));
	if (!inst)
		return -ENOMEM;

	inst->vdec.frm_name	= "H.264";
	inst->vdec.video_type	= VFORMAT_H264;
	inst->vdec.filp		= ctx->dev->filp;
	inst->vdec.ctx		= ctx;
	inst->ctx		= ctx;

	vdec_parser_parms(inst);

	/* set play mode.*/
	if (ctx->is_drm_mode)
		inst->vdec.port.flag |= PORT_FLAG_DRM;

	/* probe info from the stream */
	inst->vsi = vzalloc(sizeof(struct vdec_h264_vsi));
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

	ret = video_decoder_init(&inst->vdec);
	if (ret) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"vdec_h264 init err=%d\n", ret);
		goto err;
	}

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO,
		"H264 Instance >> %lx", (ulong) inst);

	return 0;
err:
	if (inst && inst->vsi && inst->vsi->header_buf)
		vfree(inst->vsi->header_buf);
	if (inst && inst->vsi)
		vfree(inst->vsi);
	if (inst)
		vfree(inst);
	*h_vdec = 0;

	return ret;
}

#if 0
static int refer_buffer_num(int level_idc, int max_poc_cnt,
	int mb_width, int mb_height)
{
	int size;
	int pic_size = mb_width * mb_height * 384;

	switch (level_idc) {
	case 9:
		size = 152064;
		break;
	case 10:
		size = 152064;
		break;
	case 11:
		size = 345600;
		break;
	case 12:
		size = 912384;
		break;
	case 13:
		size = 912384;
		break;
	case 20:
		size = 912384;
		break;
	case 21:
		size = 1824768;
		break;
	case 22:
		size = 3110400;
		break;
	case 30:
		size = 3110400;
		break;
	case 31:
		size = 6912000;
		break;
	case 32:
		size = 7864320;
		break;
	case 40:
		size = 12582912;
		break;
	case 41:
		size = 12582912;
		break;
	case 42:
		size = 13369344;
		break;
	case 50:
		size = 42393600;
		break;
	case 51:
	case 52:
	default:
		size = 70778880;
		break;
	}

	size /= pic_size;
	size = size + 1; /* need more buffers */

	if (size > max_poc_cnt)
		size = max_poc_cnt;

	return size;
}
#endif

static void vdec_config_dw_mode(struct vdec_pic_info *pic, int dw_mode)
{
	switch (dw_mode) {
	case 0x1: /* (w x h) + (w/2 x h) */
		pic->coded_width	+= pic->coded_width >> 1;
		pic->y_len_sz		= pic->coded_width * pic->coded_height;
		pic->c_len_sz		= pic->y_len_sz >> 1;
		break;
	case 0x2: /* (w x h) + (w/2 x h/2) */
		pic->coded_width	+= pic->coded_width >> 1;
		pic->coded_height	+= pic->coded_height >> 1;
		pic->y_len_sz		= pic->coded_width * pic->coded_height;
		pic->c_len_sz		= pic->y_len_sz >> 1;
		break;
	default: /* nothing to do */
		break;
	}
}
static int vdec_h264_pic_scale(int length, int dw_mode)
{
	int ret = 64;

	switch (dw_mode) {
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
	case 0x10: /* (w x h), output YUV420-8bit)*/
	default:
		ret = length;
		break;
	}

	return ret;
}

static void fill_vdec_params(struct vdec_h264_inst *inst, struct h264_SPS_t *sps)
{
	struct vdec_pic_info *pic = &inst->vsi->pic;
	struct vdec_h264_dec_info *dec = &inst->vsi->dec;
	struct v4l2_rect *rect = &inst->vsi->crop;
	int dw = inst->parms.cfg.double_write_mode;
	int margin = inst->parms.cfg.ref_buf_margin;
	u32 mb_w, mb_h, width, height;

	mb_w = sps->mb_width;
	mb_h = sps->mb_height;

	width  = mb_w << 4;
	height = mb_h << 4;

	width  -= (sps->crop_left + sps->crop_right);
	height -= (sps->crop_top + sps->crop_bottom);

	/* fill visible area size that be used for EGL. */
	pic->visible_width	= width;
	pic->visible_height	= height;

	/* calc visible ares. */
	rect->left		= 0;
	rect->top		= 0;
	rect->width		= pic->visible_width;
	rect->height		= pic->visible_height;

	/* config canvas size that be used for decoder. */
	pic->coded_width	= ALIGN(mb_w, 4) << 4;
	pic->coded_height	= ALIGN(mb_h, 4) << 4;
	pic->y_len_sz		= pic->coded_width * pic->coded_height;
	pic->c_len_sz		= pic->y_len_sz >> 1;
	pic->profile_idc	= sps->profile_idc;
	/* calc DPB size */
	dec->dpb_sz		= sps->num_reorder_frames + margin;

	inst->parms.ps.visible_width	= pic->visible_width;
	inst->parms.ps.visible_height	= pic->visible_height;
	inst->parms.ps.coded_width	= pic->coded_width;
	inst->parms.ps.coded_height	= pic->coded_height;
	inst->parms.ps.profile		= sps->profile_idc;
	inst->parms.ps.mb_width		= sps->mb_width;
	inst->parms.ps.mb_height	= sps->mb_height;
	inst->parms.ps.ref_frames	= sps->ref_frame_count;
	inst->parms.ps.dpb_frames	= sps->num_reorder_frames;
	inst->parms.ps.dpb_size		= dec->dpb_sz;
	inst->parms.parms_status	|= V4L2_CONFIG_PARM_DECODE_PSINFO;

	vdec_config_dw_mode(pic, dw);

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_BUFMGR,
		"The stream infos, dw: %d, coded:(%d x %d), visible:(%d x %d), DPB: %d, margin: %d\n",
		dw, pic->coded_width, pic->coded_height,
		pic->visible_width, pic->visible_height,
		dec->dpb_sz - margin, margin);
}

static bool check_frame_combine(u8 *buf, u32 size, int *pos)
{
	bool combine = false;
	int i = 0, j = 0, cnt = 0;
	u8 *p = buf;

	for (i = 4; i < size; i++) {
		j = find_start_code(p, 7);
		if (j > 0) {
			if (++cnt > 1) {
				combine = true;
				break;
			}

			*pos = p - buf + j;
			p += j;
			i += j;
		}
		p++;
	}

	//pr_info("nal pos: %d, is_combine: %d\n",*pos, *is_combine);
	return combine;
}

static bool cloudgame_check_frame_combine(u8 *buf, u32 size)
{
	bool combine = false;
	int i = 0, j = 0, cnt = 0;
	u32 nal_type = 0;
	u8 *p = buf;

	for (i = 4; i < size; i++) {
		j = find_start_code(p, 7);
		if (j > 0) {
			if (++cnt > 1) {
				combine = true;
				break;
			}

			nal_type = AVC_NAL_TYPE(p[j]);
			if (nal_type != NAL_H264_AUD &&
				(nal_type > NAL_H264_PPS || nal_type < NAL_H264_SEI))
				break;
			p += j;
			i += j;
		}
		p++;
	}
	return combine;
}

static int vdec_search_startcode(u8 *buf, u32 range)
{
	int pos = -1;
	int i = 0, j = 0;
	u8 *p = buf;

	for (i = 4; i < range; i++) {
		j = find_start_code(p, 7);
		if (j > 0) {
			pos = p - buf + j;
			break;
		}
		p++;
	}

	return pos;
}

static int parse_stream_ucode(struct vdec_h264_inst *inst,
			      u8 *buf, u32 size, u64 timestamp)
{
	int ret = 0;
	struct aml_vdec_adapt *vdec = &inst->vdec;

	ret = vdec_vframe_write(vdec, buf, size, timestamp, 0);
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

static int parse_stream_ucode_dma(struct vdec_h264_inst *inst,
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

static int parse_stream_cpu(struct vdec_h264_inst *inst, u8 *buf, u32 size)
{
	int ret = 0;
	struct h264_param_sets *ps;
	int nal_idx = 0;
	bool is_combine = false;

	is_combine = check_frame_combine(buf, size, &nal_idx);
	if (nal_idx < 0)
		return -1;

	/* if the st compose from csd + slice that is the combine data. */
	inst->vsi->is_combine = is_combine;
	inst->vsi->nalu_pos = nal_idx;

	ps = vzalloc(sizeof(struct h264_param_sets));
	if (ps == NULL)
		return -ENOMEM;

	ret = h264_decode_extradata_ps(buf, size, ps);
	if (ret) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"parse extra data failed. err: %d\n", ret);
		goto out;
	}

	if (ps->sps_parsed)
		fill_vdec_params(inst, &ps->sps);

	ret = ps->sps_parsed ? 0 : -1;
out:
	vfree(ps);

	return ret;
}

static int vdec_h264_probe(unsigned long h_vdec,
	struct aml_vcodec_mem *bs, void *out)
{
	struct vdec_h264_inst *inst =
		(struct vdec_h264_inst *)h_vdec;
	u8 *buf = (u8 *) bs->vaddr;
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
				skip_aud_data((u8 **)&s->data, &s->len);
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
			skip_aud_data(&buf, &size);
			ret = parse_stream_cpu(inst, buf, size);
		}
	}

	inst->vsi->cur_pic = inst->vsi->pic;

	return ret;
}

static void vdec_h264_deinit(unsigned long h_vdec)
{
	struct vdec_h264_inst *inst = (struct vdec_h264_inst *)h_vdec;
	struct aml_vcodec_ctx *ctx = inst->ctx;

	video_decoder_release(&inst->vdec);

	if (inst->vsi && inst->vsi->header_buf)
		vfree(inst->vsi->header_buf);

	if (inst->vsi)
		vfree(inst->vsi);

	vfree(inst);

	ctx->drv_handle = 0;
}

static int vdec_write_nalu(struct vdec_h264_inst *inst,
	u8 *buf, u32 size, u64 ts)
{
	int ret = -1;
	struct aml_vdec_adapt *vdec = &inst->vdec;
	bool is_combine = inst->vsi->is_combine;
	int nalu_pos;
	u32 nal_type;

	/*print_hex_debug(buf, size, 32);*/

	nalu_pos = vdec_search_startcode(buf, 16);
	if (nalu_pos < 0)
		goto err;

	nal_type = AVC_NAL_TYPE(buf[nalu_pos]);
	//v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO, "NALU type: %d, size: %u\n", nal_type, size);

	if (nal_type == NAL_H264_SPS && !is_combine) {
		if (inst->vsi->head_offset + size > HEADER_BUFFER_SIZE) {
			ret = -EILSEQ;
			goto err;
		}
		inst->vsi->sps_size = size;
		memcpy(inst->vsi->header_buf + inst->vsi->head_offset, buf, size);
		inst->vsi->head_offset += inst->vsi->sps_size;
		ret = size;
	} else if (nal_type == NAL_H264_PPS && !is_combine) {
			//buf_sz -= nal_start_idx;
		if (inst->vsi->head_offset + size > HEADER_BUFFER_SIZE) {
			ret = -EILSEQ;
			goto err;
		}
		inst->vsi->pps_size = size;
		memcpy(inst->vsi->header_buf + inst->vsi->head_offset, buf, size);
		inst->vsi->head_offset += inst->vsi->pps_size;
		ret = size;
	} else if (nal_type == NAL_H264_SEI && !is_combine) {
		if (inst->vsi->head_offset + size > HEADER_BUFFER_SIZE) {
			ret = -EILSEQ;
			goto err;
		}
		inst->vsi->sei_size = size;
		memcpy(inst->vsi->header_buf + inst->vsi->head_offset, buf, size);
		inst->vsi->head_offset += inst->vsi->sei_size;
		ret = size;
	} else if (inst->vsi->head_offset == 0) {
		ret = vdec_vframe_write(vdec, buf, size, ts, 0);
	} else {
		char *write_buf = vmalloc(inst->vsi->head_offset + size);
		if (!write_buf) {
			ret = -ENOMEM;
			goto err;
		}

		memcpy(write_buf, inst->vsi->header_buf, inst->vsi->head_offset);
		memcpy(write_buf + inst->vsi->head_offset, buf, size);

		ret = vdec_vframe_write(vdec, write_buf,
			inst->vsi->head_offset + size, ts, 0);

		memset(inst->vsi->header_buf, 0, HEADER_BUFFER_SIZE);
		inst->vsi->head_offset = 0;
		inst->vsi->sps_size = 0;
		inst->vsi->pps_size = 0;
		inst->vsi->sei_size = 0;

		vfree(write_buf);
	}

	return ret;
err:
	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR, "err(%d)", ret);
	return ret;
}

static bool monitor_res_change(struct vdec_h264_inst *inst, u8 *buf, u32 size)
{
	int ret = 0, i = 0, j = 0;
	u8 *p = buf;
	int len = size;
	u32 type;

	for (i = 4; i < size; i++) {
		j = find_start_code(p, len);
		if (j > 0) {
			len = size - (p - buf);
			type = AVC_NAL_TYPE(p[j]);
			if (type != NAL_H264_AUD &&
				(type > NAL_H264_PPS || type < NAL_H264_SEI))
				break;

			if (type == NAL_H264_SPS) {
				ret = parse_stream_cpu(inst, p, len);
				if (ret)
					break;
			}
			p += j;
		}
		p++;
	}

	if (!ret && ((inst->vsi->cur_pic.coded_width !=
		inst->vsi->pic.coded_width ||
		inst->vsi->cur_pic.coded_height !=
		inst->vsi->pic.coded_height) ||
		(inst->vsi->pic.profile_idc !=
		inst->vsi->cur_pic.profile_idc))) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO, "res change\n");
		inst->vsi->cur_pic = inst->vsi->pic;
		return true;
	}

	return false;
}

static int vdec_h264_decode(unsigned long h_vdec,
			    struct aml_vcodec_mem *bs, bool *res_chg)
{
	struct vdec_h264_inst *inst = (struct vdec_h264_inst *)h_vdec;
	struct aml_vdec_adapt *vdec = &inst->vdec;
	u8 *buf = (u8 *) bs->vaddr;
	u32 size = bs->size;
	int ret = -1;

	if (bs == NULL)
		return -1;

	if (vdec_input_full(vdec))
		return -EAGAIN;

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
		if (inst->ctx->param_sets_from_ucode) {
			int nal_idx = 0;
			bool enable_fence = (inst->parms.cfg.low_latency_mode & 2) ? 1 : 0;
			/* if the st compose from csd + slice that is the combine data. */
			if (enable_fence)
				inst->vsi->is_combine = cloudgame_check_frame_combine(buf, size);
			else
				inst->vsi->is_combine = check_frame_combine(buf, size, &nal_idx);
			/*if (nal_idx < 0)
				return -1;*/
		} else {
			/*checked whether the resolution changes.*/
			if ((*res_chg = monitor_res_change(inst, buf, size))) {
				return 0;
			}
		}
		ret = vdec_write_nalu(inst, buf, size, bs->timestamp);
	}

	return ret;
}

static void get_param_config_info(struct vdec_h264_inst *inst,
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

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO,
		"parms status: %u\n", parms->parms_status);
}

static void get_param_comp_buf_info(struct vdec_h264_inst *inst,
	struct vdec_comp_buf_info *params)
{
	memcpy(params, &inst->comp_info, sizeof(*params));
}

static int vdec_h264_get_param(unsigned long h_vdec,
			       enum vdec_get_param_type type, void *out)
{
	int ret = 0;
	struct vdec_h264_inst *inst = (struct vdec_h264_inst *)h_vdec;

	if (!inst) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"the h264 inst of dec is invalid.\n");
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
		unsigned int* mode = out;
		*mode = inst->ctx->config.parm.dec.cfg.double_write_mode;
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

static void set_param_write_sync(struct vdec_h264_inst *inst)
{
	complete(&inst->comp);
}

static void set_param_ps_info(struct vdec_h264_inst *inst,
	struct aml_vdec_ps_infos *ps)
{
	struct vdec_pic_info *pic = &inst->vsi->pic;
	struct vdec_h264_dec_info *dec = &inst->vsi->dec;
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
	pic->coded_width 	= ps->coded_width;
	pic->coded_height 	= ps->coded_height;

	pic->y_len_sz		= vdec_h264_pic_scale(ALIGN(ps->coded_width, 64), dw) *
		vdec_h264_pic_scale(ALIGN(ps->coded_height, 64), dw);
	pic->c_len_sz		= pic->y_len_sz >> 1;
	pic->profile_idc	= ps->profile;
	pic->field		= ps->field;
	pic->dpb_frames		= ps->dpb_frames;
	pic->dpb_margin		= ps->dpb_margin;
	pic->vpp_margin		= ps->dpb_margin;
	dec->dpb_sz		= ps->dpb_size;

	inst->parms.ps 	= *ps;
	inst->parms.parms_status |=
		V4L2_CONFIG_PARM_DECODE_PSINFO;

	/*wake up*/
	complete(&inst->comp);

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO,
		"Parse from ucode, visible(%d x %d), coded(%d x %d), scan:%s\n",
		ps->visible_width, ps->visible_height,
		ps->coded_width, ps->coded_height,
		ps->field == V4L2_FIELD_NONE ? "P" : "I");
}

static void set_param_hdr_info(struct vdec_h264_inst *inst,
	struct aml_vdec_hdr_infos *hdr)
{
	inst->parms.hdr = *hdr;
	if (!(inst->parms.parms_status &
		V4L2_CONFIG_PARM_DECODE_HDRINFO)) {
		inst->parms.hdr = *hdr;
		inst->parms.parms_status |=
			V4L2_CONFIG_PARM_DECODE_HDRINFO;
		aml_vdec_dispatch_event(inst->ctx,
			V4L2_EVENT_SRC_CH_HDRINFO);
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO,
			"H264 set HDR infos\n");
	}
}

static void set_pic_info(struct vdec_h264_inst *inst,
	struct vdec_pic_info *pic)
{
	inst->vsi->pic = *pic;
}

static void set_param_post_event(struct vdec_h264_inst *inst, u32 *event)
{
	aml_vdec_dispatch_event(inst->ctx, *event);
	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO,
		"H264 post event: %d\n", *event);
}
static void set_param_comp_buf_info(struct vdec_h264_inst *inst,
		struct vdec_comp_buf_info *info)
{
	memcpy(&inst->comp_info, info, sizeof(*info));
}

static int vdec_h264_set_param(unsigned long h_vdec,
	enum vdec_set_param_type type, void *in)
{
	int ret = 0;
	struct vdec_h264_inst *inst = (struct vdec_h264_inst *)h_vdec;

	if (!inst) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"the h264 inst of dec is invalid.\n");
		return -1;
	}

	switch (type) {
	case SET_PARAM_WRITE_FRAME_SYNC:
		set_param_write_sync(inst);
		break;

	case SET_PARAM_PS_INFO:
		set_param_ps_info(inst, in);
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
	case SET_PARAM_COMP_BUF_INFO:
		set_param_comp_buf_info(inst, in);
		break;
	default:
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"invalid set parameter type=%d\n", type);
		ret = -EINVAL;
	}

	return ret;
}

static struct vdec_common_if vdec_h264_if = {
	.init		= vdec_h264_init,
	.probe		= vdec_h264_probe,
	.decode		= vdec_h264_decode,
	.get_param	= vdec_h264_get_param,
	.set_param	= vdec_h264_set_param,
	.deinit		= vdec_h264_deinit,
};

struct vdec_common_if *get_h264_dec_comm_if(void);

struct vdec_common_if *get_h264_dec_comm_if(void)
{
	return &vdec_h264_if;
}
