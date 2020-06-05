/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_MESON_FBDEV_H
#define __AM_MESON_FBDEV_H

#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
int am_meson_drm_fbdev_init(struct drm_device *dev);
void am_meson_drm_fbdev_fini(struct drm_device *dev);
#endif
extern struct am_meson_logo logo;
#endif /* __AM_MESON_FBDEV_H */
