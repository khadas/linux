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
#ifndef _AML_VDEC_V4L2_BUFFER_H_
#define _AML_VDEC_V4L2_BUFFER_H_

#include "../../../amvdec_ports/vdec_drv_base.h"
#include "../../../amvdec_ports/aml_vcodec_adapt.h"

int vdec_v4l_get_pic_info(
	struct aml_vcodec_ctx *ctx,
	struct vdec_pic_info *pic);

int vdec_v4l_set_cfg_infos(
	struct aml_vcodec_ctx *ctx,
	struct aml_vdec_cfg_infos *cfg);

int vdec_v4l_set_ps_infos(
	struct aml_vcodec_ctx *ctx,
	struct aml_vdec_ps_infos *ps);

int vdec_v4l_set_comp_buf_info(
	struct aml_vcodec_ctx *ctx,
	struct vdec_comp_buf_info *info);

int vdec_v4l_set_hdr_infos(
	struct aml_vcodec_ctx *ctx,
	struct aml_vdec_hdr_infos *hdr);

int vdec_v4l_write_frame_sync(
	struct aml_vcodec_ctx *ctx);

int vdec_v4l_post_error_event(
	struct aml_vcodec_ctx *ctx, u32 type);

int vdec_v4l_post_evet(
	struct aml_vcodec_ctx *ctx,
	u32 event);

int vdec_v4l_res_ch_event(
	struct aml_vcodec_ctx *ctx);

int vdec_v4l_get_dw_mode(
	struct aml_vcodec_ctx *ctx,
	unsigned int *dw_mode);

#endif
