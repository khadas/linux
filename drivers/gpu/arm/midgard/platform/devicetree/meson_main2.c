/*
 * Copyright (C) 2010, 2012-2014 Amlogic Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for:
 * meson8m2 and the newer chip
 */
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/pm.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <asm/io.h>
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>

#include <mali_kbase.h>
#include <mali_kbase_hw.h>
#include <mali_kbase_config.h>
#include <mali_kbase_defs.h>

#include "mali_scaling.h"
#include "mali_clock.h"
#include "meson_main2.h"

extern void mali_post_init(void);
struct kbase_device;
//static int gpu_dvfs_probe(struct platform_device *pdev)
int platform_dt_init_func(struct kbase_device *kbdev)
{
    struct device *dev = kbdev->dev;
	struct platform_device *pdev = to_platform_device(dev);

	int err = -1;

	err = mali_meson_init_start(pdev);
    mali_meson_init_finish(pdev);
    mpgpu_class_init();
    mali_post_init();
    return err;
}

//static int gpu_dvfs_remove(struct platform_device *pdev)
void platform_dt_term_func(struct kbase_device *kbdev)
{
    struct device *dev = kbdev->dev;
	struct platform_device *pdev = to_platform_device(dev);

	printk("%s, %d\n", __FILE__, __LINE__);

    mpgpu_class_exit();
	mali_meson_uninit(pdev);

}

static u32 last_utilisation, last_util_gl_share, last_util_cl_share[2];
inline int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation,
	u32 util_gl_share, u32 util_cl_share[2])
{
    last_utilisation = utilisation;
    last_util_gl_share = util_gl_share;
    last_util_cl_share[0] = util_cl_share[0];
    last_util_cl_share[1] = util_cl_share[1];
    mali_gpu_utilization_callback(utilisation*255/100);
    return 1;
}

u32 mpgpu_get_utilization(void)
{
    return last_utilisation;
}
u32 mpgpu_get_util_gl_share(void)
{
    return last_util_gl_share;
}
u32 mpgpu_get_util_cl_share(u32 *util)
{
    util[0] = last_util_cl_share[0];
    util[1] = last_util_cl_share[1];
    return 0;
}

struct kbase_platform_funcs_conf dt_funcs_conf = {
    .platform_init_func = platform_dt_init_func,
    .platform_term_func = platform_dt_term_func,
};
#if 0
static const struct of_device_id gpu_dvfs_ids[] = {
	{ .compatible = "meson, gpu-dvfs-1.00.a" },
	{ },
};
MODULE_DEVICE_TABLE(of, gpu_dvfs_ids);

static struct platform_driver gpu_dvfs_driver = {
	.driver = {
		.name = "meson-gpu-dvfs",
		.owner = THIS_MODULE,
		.of_match_table = gpu_dvfs_ids,
	},
	.probe = gpu_dvfs_probe,
	.remove = gpu_dvfs_remove,
};
module_platform_driver(gpu_dvfs_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Amlogic SH, MM");
MODULE_DESCRIPTION("Driver for the Meson GPU dvfs");
#endif
