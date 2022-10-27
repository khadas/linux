/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef VC_UTIL_H
#define VC_UTIL_H

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
	struct componser_info_t componser_info;
	bool dirty;
	u32 phy_addr;
	u32 buf_w;
	u32 buf_h;
	u32 buf_size;
	bool is_tvp;
};

#endif
