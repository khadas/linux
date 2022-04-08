// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifdef MODULE

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/ctype.h>
#include <drm/drmP.h>
#include "meson_drm_main.h"

static int __init meson_drm_main_init(void)
{
	int ret;

	DRM_INFO("%s() start\n", __func__);
	ret = am_meson_vpu_init();
	if (ret) {
		DRM_ERROR("am_meson_vpu_init fail\n");
		return ret;
	}
	ret = am_meson_drm_init();
	if (ret) {
		DRM_ERROR("am_meson_drm_init fail\n");
		return ret;
	}
	DRM_INFO("%s() end\n", __func__);
	return 0;
}

static void __exit meson_drm_main_exit(void)
{
	DRM_INFO("%s() start\n", __func__);
	am_meson_vpu_exit();
	am_meson_drm_exit();
	DRM_INFO("%s() end\n", __func__);
}

module_init(meson_drm_main_init);
module_exit(meson_drm_main_exit);

MODULE_LICENSE("GPL v2");
#endif
