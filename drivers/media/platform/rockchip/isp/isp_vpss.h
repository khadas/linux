/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_VPSS_H
#define _RKISP_VPSS_H

#define RKISP_VPSS_CMD_SOF \
	_IOW('V', BASE_VIDIOC_PRIVATE + 0, struct rkisp_vpss_sof)

#define RKISP_VPSS_CMD_EOF \
	_IOW('V', BASE_VIDIOC_PRIVATE + 1, int)

#define RKISP_VPSS_GET_UNITE_MODE \
	_IOR('V', BASE_VIDIOC_PRIVATE + 2, unsigned int)

struct rkisp_vpss_sof {
	u32 irq;
	u32 seq;
	u64 timestamp;
	u32 unite_index;
	u32 reserved;
};

#endif
