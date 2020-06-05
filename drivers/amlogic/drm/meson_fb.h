/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_MESON_FB_H
#define __AM_MESON_FB_H
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_modeset_helper.h>

#include "meson_gem.h"

#define to_am_meson_fb(x) container_of(x, struct am_meson_fb, base)

#define VMODE_NAME_LEN_MAX    64
/*gem object num for pre framebuffer,
 *need seem with struct drm_mode_fb_cmd2->handles
 */
#define AM_MESON_GEM_OBJECT_NUM 4

struct am_meson_logo {
	struct page *logo_page;
	phys_addr_t start;
	u32 size;
	u32 width;
	u32 height;
	u32 bpp;
	u32 alloc_flag;
	u32 info_loaded_mask;
	u32 osd_reverse;
	char *outputmode_t;
	char outputmode[VMODE_NAME_LEN_MAX];
};

struct am_meson_fb {
	struct drm_framebuffer base;
	struct am_meson_gem_object *bufp[AM_MESON_GEM_OBJECT_NUM];
	struct am_meson_logo *logo;
};

struct drm_framebuffer *
am_meson_fb_create(struct drm_device *dev,
		   struct drm_file *file_priv,
		   const struct drm_mode_fb_cmd2 *mode_cmd);
struct drm_framebuffer *
am_meson_drm_framebuffer_init(struct drm_device *dev,
			      struct drm_mode_fb_cmd2 *mode_cmd,
			      struct drm_gem_object *obj);
struct drm_framebuffer *
am_meson_fb_alloc(struct drm_device *dev,
		  struct drm_mode_fb_cmd2 *mode_cmd,
		  struct drm_gem_object *obj);

#endif
