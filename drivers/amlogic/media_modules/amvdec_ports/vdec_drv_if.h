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
#ifndef _VDEC_DRV_IF_H_
#define _VDEC_DRV_IF_H_

#include "aml_vcodec_drv.h"
#include "aml_vcodec_dec.h"
#include "aml_vcodec_util.h"
#include "../stream_input/amports/streambuf.h"

#define AML_VIDEO_MAGIC CODEC_MODE('A', 'M', 'L', 'V')

#define V4L_STREAM_TYPE_MATEDATA	(0)
#define V4L_STREAM_TYPE_FRAME		(1)

struct stream_info {
	u32 stream_width;
	u32 stream_height;
	u32 stream_field;
	u32 stream_dpb;
};

struct aml_video_stream {
	u32 magic;
	u32 type;
	union {
		struct stream_info s;
		u8 buf[64];
	} m;
	u32 len;
	u8 data[0];
};

/**
 * struct vdec_fb_status  - decoder frame buffer status
 * @FB_ST_INIT		: initial state
 * @FB_ST_DECODER	: frame buffer be allocted by decoder.
 * @FB_ST_VPP		: frame buffer be allocate by vpp.
 * @FB_ST_DISPLAY	: frame buffer is ready to be displayed.
 * @FB_ST_FREE		: frame buffer is not used by decoder any more
 */
enum vdec_fb_status {
	FB_ST_INIT,
	FB_ST_DECODER,
	FB_ST_VPP,
	FB_ST_GE2D,
	FB_ST_DISPLAY,
	FB_ST_FREE
};

enum vdec_dw_mode {
	VDEC_DW_AFBC_ONLY = 0,
	VDEC_DW_AFBC_1_1_DW = 1,
	VDEC_DW_AFBC_1_4_DW = 2,
	VDEC_DW_AFBC_x2_1_4_DW = 3,
	VDEC_DW_AFBC_1_2_DW = 4,
	VDEC_DW_NO_AFBC = 16,
	VDEC_DW_AFBC_AUTO_1_2 = 0x100,
	VDEC_DW_AFBC_AUTO_1_4 = 0x200,
};

/*
 * the caller does not own the returned buffer. The buffer will not be
 *				released before vdec_if_deinit.
 * GET_PARAM_PIC_INFO		: get picture info, struct vdec_pic_info*
 * GET_PARAM_CROP_INFO		: get crop info, struct v4l2_crop*
 * GET_PARAM_DPB_SIZE		: get dpb size, unsigned int*
 * GET_PARAM_DW_MODE:		: get double write mode, unsigned int*
 * GET_PARAM_COMP_BUF_INFO	: get compressed buf info,  struct vdec_comp_buf_info*
 */
enum vdec_get_param_type {
	GET_PARAM_PIC_INFO,
	GET_PARAM_CROP_INFO,
	GET_PARAM_DPB_SIZE,
	GET_PARAM_CONFIG_INFO,
	GET_PARAM_DW_MODE,
	GET_PARAM_COMP_BUF_INFO
};

/*
 * SET_PARAM_PS_INFO	       : set picture parms, data parsed from ucode.
 */
enum vdec_set_param_type {
	SET_PARAM_WRITE_FRAME_SYNC,
	SET_PARAM_PS_INFO,
	SET_PARAM_COMP_BUF_INFO,
	SET_PARAM_HDR_INFO,
	SET_PARAM_POST_EVENT,
	SET_PARAM_PIC_INFO,
	SET_PARAM_CFG_INFO
};

/**
 * struct vdec_fb_node  - decoder frame buffer node
 * @list	: list to hold this node
 * @fb	: point to frame buffer (vdec_fb), fb could point to frame buffer and
 *	working buffer this is for maintain buffers in different state
 */
struct vdec_fb_node {
	struct list_head list;
	struct vdec_fb *fb;
};

/**
 * vdec_if_init() - initialize decode driver
 * @ctx	: [in] v4l2 context
 * @fourcc	: [in] video format fourcc, V4L2_PIX_FMT_H264/VP8/VP9..
 */
int vdec_if_init(struct aml_vcodec_ctx *ctx, unsigned int fourcc);

int vdec_if_probe(struct aml_vcodec_ctx *ctx,
	struct aml_vcodec_mem *bs, void *out);

/**
 * vdec_if_deinit() - deinitialize decode driver
 * @ctx	: [in] v4l2 context
 *
 */
void vdec_if_deinit(struct aml_vcodec_ctx *ctx);

/**
 * vdec_if_decode() - trigger decode
 * @ctx	: [in] v4l2 context
 * @bs	: [in] input bitstream
 * @fb	: [in] frame buffer to store decoded frame, when null menas parse
 *	header only
 * @res_chg	: [out] resolution change happens if current bs have different
 *	picture width/height
 * Note: To flush the decoder when reaching EOF, set input bitstream as NULL.
 *
 * Return: 0 on success. -EIO on unrecoverable error.
 */
int vdec_if_decode(struct aml_vcodec_ctx *ctx,
		   struct aml_vcodec_mem *bs, bool *res_chg);

/**
 * vdec_if_get_param() - get driver's parameter
 * @ctx	: [in] v4l2 context
 * @type	: [in] input parameter type
 * @out	: [out] buffer to store query result
 */
int vdec_if_get_param(struct aml_vcodec_ctx *ctx,
	enum vdec_get_param_type type, void *out);

int vdec_if_set_param(struct aml_vcodec_ctx *ctx,
	enum vdec_set_param_type type, void *in);

#endif
