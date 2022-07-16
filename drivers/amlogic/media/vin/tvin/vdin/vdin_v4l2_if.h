/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VDIN_V4L2_IF_H
#define __VDIN_V4L2_IF_H

#include "vdin_drv.h"
#include <linux/amlogic/meson_uvm_core.h>
#define VDIN_V4L2_GET_LOOPBACK_FMT_BY_VOUT_SERVE

#define VDIN_DEV_VER		0x20220325
#define VDIN_DEV_VER2		"v4l2 base func ok"
#define VDIN_V4L_DV_NAME	"vdin_video"
#define VDIN1_V4L_DV_NAME	"vdin1_video"

#define VDIN_V4L_DRV_NAME	"vdin_video"
#define VDIN_V4L_CARD_NAME	"meson_t3"
#define VDIN0_V4L_BUS_INFO	"vdin0 v4l2"
#define VDIN1_V4L_BUS_INFO	"vdin1 v4l2"

#define VDIN_VD_NUMBER		(70)
#define VDIN_DEVICE_CAPS \
	(V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE)

#define NUM_PLANES_YUYV		1
#define NUM_PLANES_NV21		2

#define VDIN_NUM_PLANES		NUM_PLANES_YUYV

//sky
#define V4L2_CID_USER_AMLOGIC_BASE	(V4L2_CID_USER_BASE + 0x1100)
#define AML_V4L2_SET_DRM_MODE		(V4L2_CID_USER_AMLOGIC_BASE + 0)
//sky project end

struct vdin_v4l2_pix_fmt {
	u32 fourcc; /* v4l2 format id */
	int depth;
};

struct vdin_vb_buff {
	struct vb2_v4l2_buffer vb;
	struct list_head list;

	struct dma_buf *dmabuf[VB2_MAX_PLANES];

	unsigned int tag;
	struct vframe_s *v4l2_vframe_s;
};

#define to_vdin_vb_buf(buf)	container_of(buf, struct vdin_vb_buff, vb)

char *vb2_memory_sts_to_str(uint32_t memory);
int vdin_v4l2_start_tvin(struct vdin_dev_s *devp);
int vdin_v4l2_stop_tvin(struct vdin_dev_s *devp);
#endif
