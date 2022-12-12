/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef VC_UTIL_H
#define VC_UTIL_H

#include <linux/amlogic/media/vfm/vframe.h>

enum videocom_source_type {
	DECODER_8BIT_NORMAL = 0,
	DECODER_8BIT_BOTTOM,
	DECODER_8BIT_TOP,
	DECODER_10BIT_NORMAL,
	DECODER_10BIT_BOTTOM,
	DECODER_10BIT_TOP,
	VDIN_8BIT_NORMAL,
	VDIN_10BIT_NORMAL,
};

struct dst_buf_t {
	int index;
	struct vframe_s frame;
	struct composer_info_t composer_info;
	bool dirty;
	u32 phy_addr;
	u32 buf_w;
	u32 buf_h;
	u32 buf_size;
	bool is_tvp;
	u32 dw_size;
	ulong afbc_head_addr;
	u32 afbc_head_size;
	ulong afbc_body_addr;
	u32 afbc_body_size;
	ulong afbc_table_addr;
	u32 afbc_table_size;
};

struct composer_vf_para {
	int src_vf_format;
	int src_vf_width;
	int src_vf_height;
	int src_vf_plane_count;
	int src_vf_angle;
	u32 src_buf_addr0;
	int src_buf_stride0;
	u32 src_buf_addr1;
	int src_buf_stride1;
	int dst_vf_format;
	int dst_vf_width;
	int dst_vf_height;
	int dst_vf_plane_count;
	u32 dst_buf_addr;
	int dst_buf_stride;
};

#endif
