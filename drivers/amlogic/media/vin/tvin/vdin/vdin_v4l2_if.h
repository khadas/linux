/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VDIN_V4L2_IF_H
#define __VDIN_V4L2_IF_H

#include <linux/amlogic/meson_uvm_core.h>

#define VDIN_DEV_VER		0x20201003
#define VDIN_DEV_VER2		"change video dev number"
#define VDIN_V4L_DV_NAME	"videovdin"
#define VDIN_VD_NUMBER		(70)

#define NUM_PLANES_YUYV		1
#define NUM_PLANES_NV21		2

#define VDIN_NUM_PLANES		NUM_PLANES_YUYV

struct vdin_vb_buff {
	struct vb2_v4l2_buffer vb;
	struct list_head list;

	struct dma_buf *dmabuf[VB2_MAX_PLANES];

	unsigned int tag;
};

#define to_vdin_vb_buf(buf)	container_of(buf, struct vdin_vb_buff, vb)

char *vb2_memory_sts_to_str(uint32_t memory);

#endif

