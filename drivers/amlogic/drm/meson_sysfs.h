/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_MESON_SYSFS_H
#define __AM_MESON_SYSFS_H

#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <drm/drmP.h>

int meson_drm_sysfs_register(struct drm_device *drm_dev);
void meson_drm_sysfs_unregister(struct drm_device *drm_dev);

#endif /* __AM_MESON_SYSFS_H */
