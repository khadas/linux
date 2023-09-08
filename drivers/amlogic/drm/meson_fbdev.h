/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_MESON_FBDEV_H
#define __AM_MESON_FBDEV_H

#include <drm/drm_fb_helper.h>

int am_meson_drm_fbdev_init(struct drm_device *dev);
void am_meson_drm_fbdev_fini(struct drm_device *dev);

/*keep sync with amlogic osd/fbdev.*/
#define FBIOGET_OSD_DMABUF               0x46fc
#define FBIO_WAITFORVSYNC          _IOW('F', 0x20, __u32)
#define FBIO_WAITFORVSYNC_64       _IOW('F', 0x21, __u32)

struct fb_dmabuf_export {
	/*idx only used in legacy android implement, not used now.*/
	__u32 buffer_idx;
	__u32 fd;
	__u32 flags;
};

struct meson_drm_rect {
	u32 x;
	u32 y;
	u32 w;
	u32 h;
	u32 isforce;
};

struct meson_drm_fbdev_config {
	u32 ui_w;
	u32 ui_h;
	u32 fb_w;
	u32 fb_h;
	u32 fb_bpp;
	u32 overlay_ui_w;
	u32 overlay_ui_h;
	u32 overlay_fb_w;
	u32 overlay_fb_h;
	u32 overlay_fb_bpp;
	int overlay_flag;
};

struct meson_drm_fbdev {
	struct drm_fb_helper base;
	struct drm_plane *plane;
	//struct am_meson_fb * fb;
	struct drm_gem_object *fb_gem;
	struct drm_mode_set modeset;
	struct meson_drm_rect dst;
	u32 zorder;
	bool blank;
};

extern struct am_meson_logo logo;
#endif /* __AM_MESON_FBDEV_H */
