/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 Rockchip Electronics Co., Ltd. */

#ifndef _RKISP_VPSS_H
#define _RKISP_VPSS_H

#define RKISP_VPSS_CMD_SOF \
	_IOW('V', BASE_VIDIOC_PRIVATE + 0, struct rkisp_vpss_sof)

#define RKISP_VPSS_CMD_EOF \
	_IO('V', BASE_VIDIOC_PRIVATE + 1)

struct rkisp_vpss_sof {
	u32 irq;
	u32 seq;
	u64 timestamp;
};

#endif
