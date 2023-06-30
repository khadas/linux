/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_PRINT_H
#define __MESON_PRINT_H

#include <linux/printk.h>
#include <drm/drm.h>
#include <drm/drm_print.h>

#define MESON_DRM_UT_FENCE		0x200  /* fence message */
#define MESON_DRM_UT_REG		0x400  /* register message */
#define MESON_DRM_UT_FBDEV		0x800  /* fbdev message */
#define MESON_DRM_UT_TRAVERSE	0X1000  /* pipeline traverse message */
#define MESON_DRM_UT_BLOCK		0X2000  /* vpu block message */

#define MESON_DRM_FENCE(fmt, ...)						\
	drm_dbg(MESON_DRM_UT_FENCE, fmt, ## __VA_ARGS__)

#define MESON_DRM_REG(fmt, ...)						\
	drm_dbg(MESON_DRM_UT_REG, fmt, ## __VA_ARGS__)

#define MESON_DRM_FBDEV(fmt, ...)						\
	drm_dbg(MESON_DRM_UT_FBDEV, fmt, ## __VA_ARGS__)

#define MESON_DRM_TRAVERSE(fmt, ...)						\
	drm_dbg(MESON_DRM_UT_TRAVERSE, fmt, ## __VA_ARGS__)

#define MESON_DRM_BLOCK(fmt, ...)						\
	drm_dbg(MESON_DRM_UT_BLOCK, fmt, ## __VA_ARGS__)

#endif /* __AM_MESON_DRV_H */
