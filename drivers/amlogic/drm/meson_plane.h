/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_PLANE_H
#define __MESON_PLANE_H

#include <linux/kernel.h>
#include <drm/drmP.h>
#include <drm/drm_plane.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#include "meson_fb.h"

#define MESON_PLANE_BEGIN_ZORDER	1
#define MESON_PLANE_END_ZORDER		65

struct am_meson_plane_state {
	struct drm_plane_state base;
	u32 premult_en;
};

struct am_osd_plane {
	struct drm_plane base; //must be first element.
	struct meson_drm *drv; //point to struct parent.
	struct dentry *plane_debugfs_dir;
	int plane_index;
	u32 osd_reverse;
	u32 osd_blend_bypass;
	struct drm_property *prop_premult_en;
};

struct am_video_plane {
	struct drm_plane base; //must be first element.
	struct meson_drm *drv; //point to struct parent.
	struct dentry *plane_debugfs_dir;
	int plane_index;
};

#define to_am_osd_plane(x) container_of(x, \
	struct am_osd_plane, base)
#define to_am_meson_plane_state(x) container_of(x, \
	struct am_meson_plane_state, base)
#define to_am_video_plane(x) container_of(x, \
	struct am_video_plane, base)

int am_meson_plane_create(struct meson_drm *priv);

#endif
