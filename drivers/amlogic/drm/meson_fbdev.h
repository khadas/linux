/*
 * drivers/amlogic/drm/meson_fbdev.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __AM_MESON_FBDEV_H
#define __AM_MESON_FBDEV_H

#ifdef CONFIG_DRM_MESON_EMULATE_FBDEV
int am_meson_drm_fbdev_init(struct drm_device *dev);
void am_meson_drm_fbdev_fini(struct drm_device *dev);
#endif

#endif /* __AM_MESON_FBDEV_H */
