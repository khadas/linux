/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_CRTC_H
#define __MESON_CRTC_H

#include <linux/kernel.h>
#include <drm/drmP.h>
#include <drm/drm_plane.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <linux/sync_file.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include "meson_vpu.h"
#include "meson_drv.h"
#include "meson_fb.h"

struct video_out_fence_state {
	s32 __user *video_out_fence_ptr;
	struct sync_file *sync_file;
	int fd;
};

struct am_meson_crtc_state {
	struct drm_crtc_state base;
	u32 hdr_policy;
	u32 dv_policy;
	struct video_out_fence_state fence_state;

	int uboot_mode_init;
};

struct am_meson_video_out_fence {
	struct dma_fence *fence;
	atomic_t refcount;
};

struct am_meson_crtc {
	struct drm_crtc base;
	struct device *dev;
	struct drm_device *drm_dev;

	struct meson_drm *priv;

	struct drm_pending_vblank_event *event;
	struct am_meson_video_out_fence video_fence[VIDEO_LATENCY_VSYNC];

	unsigned int irq;

	struct dentry *crtc_debugfs_dir;

	struct meson_vpu_pipeline *pipeline;
	struct drm_property *prop_hdr_policy;
	struct drm_property *prop_dv_policy;

	struct drm_property *prop_video_out_fence_ptr;
	int dump_enable;
	int blank_enable;
	int dump_counts;
	int dump_index;
	char osddump_path[64];

	int vpp_crc_enable;
};

#define to_am_meson_crtc(x) container_of(x, \
		struct am_meson_crtc, base)
#define to_am_meson_crtc_state(x) container_of(x, \
		struct am_meson_crtc_state, base)

int am_meson_crtc_create(struct am_meson_crtc *amcrtc);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
void set_dolby_vision_policy(int policy);
int get_dolby_vision_policy(void);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
void set_hdr_policy(int policy);
int get_hdr_policy(void);
#endif
#endif
