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

extern int av1_need_prefix;

/**
 * struct av1_fb - av1 decode frame buffer information
 * @vdec_fb_va  : virtual address of struct vdec_fb
 * @y_fb_dma    : dma address of Y frame buffer (luma)
 * @c_fb_dma    : dma address of C frame buffer (chroma)
 * @poc         : picture order count of frame buffer
 * @reserved    : for 8 bytes alignment
 */
struct av1_fb {
	uint64_t vdec_fb_va;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;
	int32_t poc;
	uint32_t reserved;
};

/**
 * struct vdec_av1_dec_info - decode information
 * @dpb_sz		: decoding picture buffer size
 * @resolution_changed  : resoltion change happen
 * @reserved		: for 8 bytes alignment
 * @bs_dma		: Input bit-stream buffer dma address
 * @y_fb_dma		: Y frame buffer dma address
 * @c_fb_dma		: C frame buffer dma address
 * @vdec_fb_va		: VDEC frame buffer struct virtual address
 */
struct vdec_av1_dec_info {
	uint32_t dpb_sz;
	uint32_t resolution_changed;
	uint32_t reserved;
	uint64_t bs_dma;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;
	uint64_t vdec_fb_va;
};

/**
 * struct vdec_av1_vsi - shared memory for decode information exchange
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
struct vdec_av1_vsi {
	char *header_buf;
	int sps_size;
	int pps_size;
	int sei_size;
	int head_offset;
	struct vdec_av1_dec_info dec;
	struct vdec_pic_info pic;
	struct vdec_pic_info cur_pic;
	struct v4l2_rect crop;
	bool is_combine;
	int nalu_pos;
};

/**
 * struct vdec_av1_inst - av1 decoder instance
 * @num_nalu : how many nalus be decoded
 * @ctx      : point to aml_vcodec_ctx
 * @vsi      : VPU shared information
 */
struct vdec_av1_inst {
	unsigned int num_nalu;
	struct aml_vcodec_ctx *ctx;
	struct aml_vdec_adapt vdec;
	struct vdec_av1_vsi *vsi;
	struct aml_dec_params parms;
	struct completion comp;
	struct vdec_comp_buf_info comp_info;
};

/*!\brief OBU types. */
enum OBU_TYPE {
	OBU_SEQUENCE_HEADER = 1,
	OBU_TEMPORAL_DELIMITER = 2,
	OBU_FRAME_HEADER = 3,
	OBU_TILE_GROUP = 4,
	OBU_METADATA = 5,
	OBU_FRAME = 6,
	OBU_REDUNDANT_FRAME_HEADER = 7,
	OBU_TILE_LIST = 8,
	OBU_PADDING = 15,
};

/*!\brief OBU metadata types. */
enum OBU_METADATA_TYPE {
	OBU_METADATA_TYPE_RESERVED_0 = 0,
	OBU_METADATA_TYPE_HDR_CLL = 1,
	OBU_METADATA_TYPE_HDR_MDCV = 2,
	OBU_METADATA_TYPE_SCALABILITY = 3,
	OBU_METADATA_TYPE_ITUT_T35 = 4,
	OBU_METADATA_TYPE_TIMECODE = 5,
};

struct ObuHeader {
	size_t size;  // Size (1 or 2 bytes) of the OBU header (including the
			// optional OBU extension header) in the bitstream.
	enum OBU_TYPE type;
	int has_size_field;
	int has_extension;
	// The following fields come from the OBU extension header and therefore are
	// only used if has_extension is true.
	int temporal_layer_id;
	int spatial_layer_id;
};

static const size_t kMaximumLeb128Size = 8;
static const u8 kLeb128ByteMask = 0x7f;  // Binary: 01111111

// Disallow values larger than 32-bits to ensure consistent behavior on 32 and
// 64 bit targets: value is typically used to determine buffer allocation size
// when decoded.
static const u64 kMaximumLeb128Value = ULONG_MAX;

char obu_type_name[16][32] = {
	"UNKNOWN",
	"OBU_SEQUENCE_HEADER",
	"OBU_TEMPORAL_DELIMITER",
	"OBU_FRAME_HEADER",
	"OBU_TILE_GROUP",
	"OBU_METADATA",
	"OBU_FRAME",
	"OBU_REDUNDANT_FRAME_HEADER",
	"OBU_TILE_LIST",
	"UNKNOWN",
	"UNKNOWN",
	"UNKNOWN",
	"UNKNOWN",
	"UNKNOWN",
	"UNKNOWN",
	"OBU_PADDING"
};

char meta_type_name[6][32] = {
	"OBU_METADATA_TYPE_RESERVED_0",
	"OBU_METADATA_TYPE_HDR_CLL",
	"OBU_METADATA_TYPE_HDR_MDCV",
	"OBU_METADATA_TYPE_SCALABILITY",
	"OBU_METADATA_TYPE_ITUT_T35",
	"OBU_METADATA_TYPE_TIMECODE"
};

struct read_bit_buffer {
	const u8 *bit_buffer;
	const u8 *bit_buffer_end;
	u32 bit_offset;
};

struct DataBuffer {
	const u8 *data;
	size_t size;
};

static int vdec_write_nalu(struct vdec_av1_inst *inst,
	u8 *buf, u32 size, u64 ts);
static int vdec_get_dw_mode(struct vdec_av1_inst *inst, int dw_mode);

static void get_pic_info(struct vdec_av1_inst *inst,
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

static void get_crop_info(struct vdec_av1_inst *inst, struct v4l2_rect *cr)
{
	cr->left = inst->vsi->crop.left;
	cr->top = inst->vsi->crop.top;
	cr->width = inst->vsi->crop.width;
	cr->height = inst->vsi->crop.height;

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO,
		"l=%d, t=%d, w=%d, h=%d\n",
		 cr->left, cr->top, cr->width, cr->height);
}

static void get_dpb_size(struct vdec_av1_inst *inst, unsigned int *dpb_sz)
{
	*dpb_sz = inst->vsi->dec.dpb_sz;
	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO, "sz=%d\n", *dpb_sz);
}

static u32 vdec_config_default_parms(u8 *parm)
{
	u8 *pbuf = parm;

	pbuf += sprintf(pbuf, "parm_v4l_codec_enable:1;");
	pbuf += sprintf(pbuf, "parm_v4l_buffer_margin:7;");
	pbuf += sprintf(pbuf, "av1_double_write_mode:1;");
	pbuf += sprintf(pbuf, "av1_buf_width:1920;");
	pbuf += sprintf(pbuf, "av1_buf_height:1088;");
	pbuf += sprintf(pbuf, "av1_max_pic_w:4096;");
	pbuf += sprintf(pbuf, "av1_max_pic_h:2304;");
	pbuf += sprintf(pbuf, "save_buffer_mode:0;");
	pbuf += sprintf(pbuf, "no_head:0;");
	pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_mode:0;");
	pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_endian:0;");

	return parm - pbuf;
}

static void vdec_parser_parms(struct vdec_av1_inst *inst)
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
		pbuf += sprintf(pbuf, "av1_double_write_mode:%d;",
			ctx->config.parm.dec.cfg.double_write_mode);
		pbuf += sprintf(pbuf, "av1_buf_width:1920;");
		pbuf += sprintf(pbuf, "av1_buf_height:1088;");
		pbuf += sprintf(pbuf, "save_buffer_mode:0;");
		pbuf += sprintf(pbuf, "no_head:0;");
		pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_mode:%d;",
			ctx->config.parm.dec.cfg.canvas_mem_mode);
		pbuf += sprintf(pbuf, "parm_v4l_canvas_mem_endian:%d;",
			ctx->config.parm.dec.cfg.canvas_mem_endian);
		pbuf += sprintf(pbuf, "parm_v4l_low_latency_mode:%d;",
			ctx->config.parm.dec.cfg.low_latency_mode);
		pbuf += sprintf(pbuf, "parm_v4l_duration:%d;",
			ctx->config.parm.dec.cfg.duration);
		ctx->config.length = pbuf - ctx->config.buf;
	} else {
		ctx->config.parm.dec.cfg.double_write_mode = 1;
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

static int vdec_av1_init(struct aml_vcodec_ctx *ctx, unsigned long *h_vdec)
{
	struct vdec_av1_inst *inst = NULL;
	int ret = -1;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;

	inst->vdec.frm_name	= "AV1";
	inst->vdec.video_type	= VFORMAT_AV1;
	inst->vdec.filp		= ctx->dev->filp;
	inst->vdec.ctx		= ctx;
	inst->ctx		= ctx;

	vdec_parser_parms(inst);

	/* set play mode.*/
	if (ctx->is_drm_mode)
		inst->vdec.port.flag |= PORT_FLAG_DRM;

	/* to eable av1 hw.*/
	inst->vdec.port.type	= PORT_TYPE_HEVC;

	/* probe info from the stream */
	inst->vsi = kzalloc(sizeof(struct vdec_av1_vsi), GFP_KERNEL);
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
			"vdec_av1 init err=%d\n", ret);
		goto err;
	}

	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO,
		"av1 Instance >> %lx\n", (ulong) inst);

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

static int parse_stream_ucode(struct vdec_av1_inst *inst,
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

static int parse_stream_ucode_dma(struct vdec_av1_inst *inst,
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

static int parse_stream_cpu(struct vdec_av1_inst *inst, u8 *buf, u32 size)
{
	v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
		"can not suppport parse stream by cpu.\n");

	return -1;
}

static int vdec_av1_probe(unsigned long h_vdec,
	struct aml_vcodec_mem *bs, void *out)
{
	struct vdec_av1_inst *inst =
		(struct vdec_av1_inst *)h_vdec;
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

static void vdec_av1_deinit(unsigned long h_vdec)
{
	struct vdec_av1_inst *inst = (struct vdec_av1_inst *)h_vdec;
	struct aml_vcodec_ctx *ctx = inst->ctx;

	video_decoder_release(&inst->vdec);

	if (inst->vsi && inst->vsi->header_buf)
		vfree(inst->vsi->header_buf);

	if (inst->vsi)
		kfree(inst->vsi);

	kfree(inst);

	ctx->drv_handle = 0;
}

// Returns 1 when OBU type is valid, and 0 otherwise.
static int valid_obu_type(int obu_type)
{
	int valid_type = 0;

	switch (obu_type) {
	case OBU_SEQUENCE_HEADER:
	case OBU_TEMPORAL_DELIMITER:
	case OBU_FRAME_HEADER:
	case OBU_TILE_GROUP:
	case OBU_METADATA:
	case OBU_FRAME:
	case OBU_REDUNDANT_FRAME_HEADER:
	case OBU_TILE_LIST:
	case OBU_PADDING:
		valid_type = 1;
		break;
	default:
		break;
	}

	return valid_type;
}

size_t uleb_size_in_bytes(u64 value)
{
	size_t size = 0;

	do {
		++size;
	} while ((value >>= 7) != 0);

	return size;
}

int uleb_decode(const u8 *buffer, size_t available,
	u64 *value, size_t *length)
{
	int i;

	if (buffer && value) {
		*value = 0;

		for (i = 0; i < kMaximumLeb128Size && i < available; ++i) {
			const u8 decoded_byte = *(buffer + i) & kLeb128ByteMask;

			*value |= ((u64)decoded_byte) << (i * 7);
			if ((*(buffer + i) >> 7) == 0) {
				if (length) {
					*length = i + 1;
				}

				// Fail on values larger than 32-bits to ensure consistent behavior on
				// 32 and 64 bit targets: value is typically used to determine buffer
				// allocation size.
				if (*value > ULONG_MAX)
					return -1;

				return 0;
			}
		}
	}

	// If we get here, either the buffer/value pointers were invalid,
	// or we ran over the available space
	return -1;
}

int uleb_encode(u64 value, size_t available,
	u8 *coded_value, size_t *coded_size)
{
	int i;
	const size_t leb_size = uleb_size_in_bytes(value);

	if (value > kMaximumLeb128Value || leb_size > kMaximumLeb128Size ||
		leb_size > available || !coded_value || !coded_size) {
		return -1;
	}

	for (i = 0; i < leb_size; ++i) {
		u8 byte = value & 0x7f;

		value >>= 7;
		if (value != 0) byte |= 0x80;  // Signal that more bytes follow.

		*(coded_value + i) = byte;
	}

	*coded_size = leb_size;

	return 0;
}

static int rb_read_bit(struct read_bit_buffer *rb)
{
	const u32 off = rb->bit_offset;
	const u32 p = off >> 3;
	const int q = 7 - (int)(off & 0x7);

	if (rb->bit_buffer + p < rb->bit_buffer_end) {
		const int bit = (rb->bit_buffer[p] >> q) & 1;

		rb->bit_offset = off + 1;
		return bit;
	} else {
		return 0;
	}
}

static int rb_read_literal(struct read_bit_buffer *rb, int bits)
{
	int value = 0, bit;

	for (bit = bits - 1; bit >= 0; bit--)
		value |= rb_read_bit(rb) << bit;

	return value;
}

static int read_obu_size(const u8 *data,
	size_t bytes_available,
	size_t *const obu_size,
	size_t *const length_field_size)
{
	u64 u_obu_size = 0;

	if (uleb_decode(data, bytes_available, &u_obu_size, length_field_size) != 0) {
		return -1;
	}

	if (u_obu_size > ULONG_MAX)
		return -1;

	*obu_size = (size_t) u_obu_size;

	return 0;
}

// Parses OBU header and stores values in 'header'.
static int read_obu_header(struct read_bit_buffer *rb,
	int is_annexb, struct ObuHeader *header)
{
	const int bit_buffer_byte_length =
		rb->bit_buffer_end - rb->bit_buffer;

	if (!rb || !header)
		return -1;

	if (bit_buffer_byte_length < 1)
		return -1;

	header->size = 1;

	if (rb_read_bit(rb) != 0) {
		// Forbidden bit. Must not be set.
		return -1;
	}

	header->type = (enum OBU_TYPE) rb_read_literal(rb, 4);
	if (!valid_obu_type(header->type))
		return -1;

	header->has_extension = rb_read_bit(rb);
	header->has_size_field = rb_read_bit(rb);

	if (!header->has_size_field && !is_annexb) {
		// section 5 obu streams must have obu_size field set.
		return -1;
	}

	if (rb_read_bit(rb) != 0) {
		// obu_reserved_1bit must be set to 0.
		return -1;
	}

	if (header->has_extension) {
		if (bit_buffer_byte_length == 1)
			return -1;

		header->size += 1;
		header->temporal_layer_id = rb_read_literal(rb, 3);
		header->spatial_layer_id = rb_read_literal(rb, 2);
		if (rb_read_literal(rb, 3) != 0) {
			// extension_header_reserved_3bits must be set to 0.
			return -1;
		}
	}

	return 0;
}

int read_obu_header_and_size(const u8 *data,
	size_t bytes_available,
	int is_annexb,
	struct ObuHeader *obu_header,
	size_t *const payload_size,
	size_t *const bytes_read)
{
	size_t length_field_size_obu = 0;
	size_t length_field_size_payload = 0;
	size_t obu_size = 0;
	int status = 0;
	struct read_bit_buffer rb = { data + length_field_size_obu,
		data + bytes_available, 0};

	if (is_annexb) {
		// Size field comes before the OBU header, and includes the OBU header
		status = read_obu_size(data, bytes_available, &obu_size, &length_field_size_obu);
		if (status != 0)
			return status;
	}

	status = read_obu_header(&rb, is_annexb, obu_header);
	if (status != 0)
		return status;

	if (!obu_header->has_size_field) {
		// Derive the payload size from the data we've already read
		if (obu_size < obu_header->size)
			return -1;

		*payload_size = obu_size - obu_header->size;
	} else {
		// Size field comes after the OBU header, and is just the payload size
		status = read_obu_size(data + length_field_size_obu + obu_header->size,
			bytes_available - length_field_size_obu - obu_header->size,
			payload_size, &length_field_size_payload);
		if (status != 0)
			return status;
	}

	*bytes_read = length_field_size_obu + obu_header->size + length_field_size_payload;

	return 0;
}

int parser_frame(int is_annexb, u8 *data, const u8 *data_end,
	u8 *dst_data, u32 *frame_len, u8 *meta_buf, u32 *meta_len)
{
	int frame_decoding_finished = 0;
	u32 obu_size = 0;
	int seen_frame_header = 0;
	int next_start_tile = 0;
	struct DataBuffer obu_size_hdr;
	u8 header[20] = {0};
	u8 *p = NULL;
	u32 rpu_size = 0;
	struct ObuHeader obu_header;

	memset(&obu_header, 0, sizeof(obu_header));

	// decode frame as a series of OBUs
	while (!frame_decoding_finished) {
		//	struct read_bit_buffer rb;
		size_t payload_size = 0;
		size_t header_size = 0;
		size_t bytes_read = 0;
		const size_t bytes_available = data_end - data;
		enum OBU_METADATA_TYPE meta_type;
		int status;
		u64 type;
		u32 i;

		if (bytes_available == 0 && !seen_frame_header) {
			break;
		}

		status = read_obu_header_and_size(data, bytes_available, is_annexb,
			&obu_header, &payload_size, &bytes_read);
		if (status != 0) {
			return -1;
		}

		// Record obu size header information.
		obu_size_hdr.data = data + obu_header.size;
		obu_size_hdr.size = bytes_read - obu_header.size;

		// Note: read_obu_header_and_size() takes care of checking that this
		// doesn't cause 'data' to advance past 'data_end'.

		if ((size_t)(data_end - data - bytes_read) < payload_size) {
			return -1;
		}

		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "obu %s len %zu+%zu\n",
			obu_type_name[obu_header.type],
			bytes_read, payload_size);

		if (!is_annexb) {
			obu_size = bytes_read + payload_size + 4;
			header_size = 20;
		} else {
			obu_size = bytes_read + payload_size;
			header_size = 16;
		}

		header[0] = ((obu_size + 4) >> 24) & 0xff;
		header[1] = ((obu_size + 4) >> 16) & 0xff;
		header[2] = ((obu_size + 4) >> 8) & 0xff;
		header[3] = ((obu_size + 4) >> 0) & 0xff;
		header[4] = header[0] ^ 0xff;
		header[5] = header[1] ^ 0xff;
		header[6] = header[2] ^ 0xff;
		header[7] = header[3] ^ 0xff;
		header[8] = 0;
		header[9] = 0;
		header[10] = 0;
		header[11] = 1;
		header[12] = 'A';
		header[13] = 'M';
		header[14] = 'L';
		header[15] = 'V';

		// put new size to here as annexb
		header[16] = (obu_size & 0xff) | 0x80;
		header[17] = ((obu_size >> 7) & 0xff) | 0x80;
		header[18] = ((obu_size >> 14) & 0xff) | 0x80;
		header[19] = ((obu_size >> 21) & 0xff) | 0x00;

		memcpy(dst_data, header, header_size);
		dst_data += header_size;
		memcpy(dst_data, data, bytes_read + payload_size);
		dst_data += (bytes_read + payload_size);

		data += bytes_read;
		*frame_len += (header_size + bytes_read + payload_size);

		switch (obu_header.type) {
		case OBU_TEMPORAL_DELIMITER:
			seen_frame_header = 0;
			next_start_tile = 0;
			break;
		case OBU_SEQUENCE_HEADER:
			// The sequence header should not change in the middle of a frame.
			if (seen_frame_header) {
				return -1;
			}
			break;
		case OBU_FRAME_HEADER:
			if (data_end == data + payload_size) {
				frame_decoding_finished = 1;
			} else {
				seen_frame_header = 1;
			}
			break;
		case OBU_REDUNDANT_FRAME_HEADER:
		case OBU_FRAME:
			if (obu_header.type == OBU_REDUNDANT_FRAME_HEADER) {
				if (!seen_frame_header) {
					return -1;
				}
			} else {
				// OBU_FRAME_HEADER or OBU_FRAME.
				if (seen_frame_header) {
					return -1;
				}
			}
			if (obu_header.type == OBU_FRAME) {
				if (data_end == data + payload_size) {
					frame_decoding_finished = 1;
					seen_frame_header = 0;
				}
			}
			break;
		case OBU_TILE_GROUP:
			if (!seen_frame_header) {
				return -1;
			}
			if (data + payload_size == data_end)
				frame_decoding_finished = 1;
			if (frame_decoding_finished)
				seen_frame_header = 0;
			break;
		case OBU_METADATA:
			uleb_decode(data, 8, &type, &bytes_read);
			if (type < 6)
				meta_type = type;
			else
				meta_type = 0;
			p = data + bytes_read;
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER,
				"meta type %s %zu+%zu\n",
				meta_type_name[type],
				bytes_read,
				payload_size - bytes_read);

			if (meta_type == OBU_METADATA_TYPE_ITUT_T35) {
#if 0 /* for dumping original obu payload */
				for (i = 0; i < payload_size - bytes_read; i++) {
					pr_info("%02x ", p[i]);
					if (i % 16 == 15)
						pr_info("\n");
				}
				if (i % 16 != 0)
					pr_info("\n");
#endif
				if ((p[0] == 0xb5) /* country code */
					&& ((p[1] == 0x00) && (p[2] == 0x3b)) /* terminal_provider_code */
					&& ((p[3] == 0x00) && (p[4] == 0x00) && (p[5] == 0x08) && (p[6] == 0x00))) { /* terminal_provider_oriented_code */
					v4l_dbg(0, V4L_DEBUG_CODEC_PARSER,
						"dolbyvison rpu\n");
					meta_buf[0] = meta_buf[1] = meta_buf[2] = 0;
					meta_buf[3] = 0x01;
					meta_buf[4] = 0x19;

					if (p[11] & 0x10) {
						rpu_size = 0x100;
						rpu_size |= (p[11] & 0x0f) << 4;
						rpu_size |= (p[12] >> 4) & 0x0f;
						if (p[12] & 0x08) {
							v4l_dbg(0, V4L_DEBUG_CODEC_PARSER,
								"meta rpu in obu exceed 512 bytes\n");
							break;
						}
						for (i = 0; i < rpu_size; i++) {
							meta_buf[5 + i] = (p[12 + i] & 0x07) << 5;
							meta_buf[5 + i] |= (p[13 + i] >> 3) & 0x1f;
						}
						rpu_size += 5;
					} else {
						rpu_size = (p[10] & 0x1f) << 3;
						rpu_size |= (p[11] >> 5) & 0x07;
						for (i = 0; i < rpu_size; i++) {
							meta_buf[5 + i] = (p[11 + i] & 0x0f) << 4;
							meta_buf[5 + i] |= (p[12 + i] >> 4) & 0x0f;
						}
						rpu_size += 5;
					}
					*meta_len = rpu_size;
				}
			} else if (meta_type == OBU_METADATA_TYPE_HDR_CLL) {
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "hdr10 cll:\n");
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "max_cll = %x\n", (p[0] << 8) | p[1]);
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "max_fall = %x\n", (p[2] << 8) | p[3]);
			} else if (meta_type == OBU_METADATA_TYPE_HDR_MDCV) {
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "hdr10 primaries[r,g,b] = \n");
				for (i = 0; i < 3; i++) {
					v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, " %x, %x\n",
						(p[i * 4] << 8) | p[i * 4 + 1],
						(p[i * 4 + 2] << 8) | p[i * 4 + 3]);
				}
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER,
					"white point = %x, %x\n", (p[12] << 8) | p[13], (p[14] << 8) | p[15]);
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER,
					"maxl = %x\n", (p[16] << 24) | (p[17] << 16) | (p[18] << 8) | p[19]);
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER,
					"minl = %x\n", (p[20] << 24) | (p[21] << 16) | (p[22] << 8) | p[23]);
			}
			break;
		case OBU_TILE_LIST:
			break;
		case OBU_PADDING:
			break;
		default:
			// Skip unrecognized OBUs
			break;
		}

		data += payload_size;
	}

	return 0;
}

static int vdec_write_nalu(struct vdec_av1_inst *inst,
	u8 *buf, u32 size, u64 ts)
{
	int ret = 0;
	struct aml_vdec_adapt *vdec = &inst->vdec;
	u8 *data = NULL;
	u32 length = 0;
	bool need_prefix = av1_need_prefix;

	if (need_prefix) {
		u8 meta_buffer[1024] = {0};
		u32 meta_size = 0;
		u8 *src = buf;

		data = vzalloc(size + 0x1000);
		if (!data)
			return -ENOMEM;

		parser_frame(0, src, src + size, data, &length, meta_buffer, &meta_size);

		if (length)
			ret = vdec_vframe_write(vdec, data, length, ts, 0);
		else
			ret = -1;

		vfree(data);
	} else {
		ret = vdec_vframe_write(vdec, buf, size, ts, 0);
	}

	return ret;
}

static bool monitor_res_change(struct vdec_av1_inst *inst, u8 *buf, u32 size)
{
	int ret = -1;
	u8 *p = buf;
	int len = size;
	u32 synccode = av1_need_prefix ?
		((p[1] << 16) | (p[2] << 8) | p[3]) :
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

static int vdec_av1_decode(unsigned long h_vdec,
			   struct aml_vcodec_mem *bs, bool *res_chg)
{
	struct vdec_av1_inst *inst = (struct vdec_av1_inst *)h_vdec;
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

 static void get_param_config_info(struct vdec_av1_inst *inst,
	struct aml_dec_params *parms)
 {
	 if (inst->parms.parms_status & V4L2_CONFIG_PARM_DECODE_CFGINFO) {
		/* dw use v4l cfg */
		inst->parms.cfg.double_write_mode =
			inst->ctx->config.parm.dec.cfg.double_write_mode;
		parms->cfg = inst->parms.cfg;
	 }
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

static void get_param_comp_buf_info(struct vdec_av1_inst *inst,
		struct vdec_comp_buf_info *params)
{
	memcpy(params, &inst->comp_info, sizeof(*params));
}

static int vdec_av1_get_param(unsigned long h_vdec,
			       enum vdec_get_param_type type, void *out)
{
	int ret = 0;
	struct vdec_av1_inst *inst = (struct vdec_av1_inst *)h_vdec;

	if (!inst) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"the av1 inst of dec is invalid.\n");
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

static void set_param_write_sync(struct vdec_av1_inst *inst)
{
	complete(&inst->comp);
}

static int vdec_get_dw_mode(struct vdec_av1_inst *inst, int dw_mode)
{
	u32 valid_dw_mode = inst->ctx->config.parm.dec.cfg.double_write_mode;
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

static int vdec_pic_scale(struct vdec_av1_inst *inst, int length, int dw_mode)
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
	case 0x21: /* only afbc, But di will output YUV420 (w/4 x h/4) */
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

static void set_param_ps_info(struct vdec_av1_inst *inst,
	struct aml_vdec_ps_infos *ps)
{
	struct vdec_pic_info *pic = &inst->vsi->pic;
	struct vdec_av1_dec_info *dec = &inst->vsi->dec;
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

static void set_param_comp_buf_info(struct vdec_av1_inst *inst,
		struct vdec_comp_buf_info *info)
{
	memcpy(&inst->comp_info, info, sizeof(*info));
}

static void set_param_hdr_info(struct vdec_av1_inst *inst,
	struct aml_vdec_hdr_infos *hdr)
{
	if (hdr->signal_type != 0) {
		inst->parms.hdr = *hdr;
		inst->parms.parms_status |=
			V4L2_CONFIG_PARM_DECODE_HDRINFO;
		aml_vdec_dispatch_event(inst->ctx,
			V4L2_EVENT_SRC_CH_HDRINFO);
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_EXINFO,
			"av1 set HDR infos\n");
	}
}

static void set_param_post_event(struct vdec_av1_inst *inst, u32 *event)
{
		aml_vdec_dispatch_event(inst->ctx, *event);
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_PRINFO,
			"av1 post event: %d\n", *event);
}

static void set_pic_info(struct vdec_av1_inst *inst,
	struct vdec_pic_info *pic)
{
	inst->vsi->pic = *pic;
}

static int vdec_av1_set_param(unsigned long h_vdec,
	enum vdec_set_param_type type, void *in)
{
	int ret = 0;
	struct vdec_av1_inst *inst = (struct vdec_av1_inst *)h_vdec;

	if (!inst) {
		v4l_dbg(inst->ctx, V4L_DEBUG_CODEC_ERROR,
			"the av1 inst of dec is invalid.\n");
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

static struct vdec_common_if vdec_av1_if = {
	.init		= vdec_av1_init,
	.probe		= vdec_av1_probe,
	.decode		= vdec_av1_decode,
	.get_param	= vdec_av1_get_param,
	.set_param	= vdec_av1_set_param,
	.deinit		= vdec_av1_deinit,
};

struct vdec_common_if *get_av1_dec_comm_if(void);

struct vdec_common_if *get_av1_dec_comm_if(void)
{
	return &vdec_av1_if;
}

