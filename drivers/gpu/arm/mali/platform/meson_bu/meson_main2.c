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
#include <linux/version.h>
#include <linux/pm.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <asm/io.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>

#include "mali_executor.h"
#include "mali_scaling.h"
#include "mali_clock.h"
#include "meson_main2.h"

int mali_pm_statue = 0;
extern void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data);

u32 mali_gp_reset_fail = 0;
module_param(mali_gp_reset_fail, int, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH); /* rw-rw-r-- */
MODULE_PARM_DESC(mali_gp_reset_fail, "times of failed to reset GP");
u32 mali_core_timeout  = 0;
module_param(mali_core_timeout, int, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH); /* rw-rw-r-- */
MODULE_PARM_DESC(mali_core_timeout, "timeout of failed to reset GP");

static struct mali_gpu_device_data mali_gpu_data = {

#if defined(CONFIG_ARCH_REALVIEW)
	.dedicated_mem_start = 0x80000000, /* Physical start address (use 0xD0000000 for old indirect setup) */
	.dedicated_mem_size = 0x10000000, /* 256MB */
#endif
#if defined(CONFIG_ARM64)
	.fb_start = 0x5f000000,
	.fb_size = 0x91000000,
#else
	.fb_start = 0xe0000000,
	.fb_size = 0x01000000,
#endif
	.control_interval = 200, /* 1000ms */
};

int mali_platform_device_init(struct platform_device *device)
{
	int err = -1;

	err = mali_meson_init_start(device);
	if (0 != err) printk("mali init failed\n");
	err = mali_meson_get_gpu_data(&mali_gpu_data);
	if (0 != err) printk("mali get gpu data failed\n");

	err = platform_device_add_data(device, &mali_gpu_data, sizeof(mali_gpu_data));

	if (0 == err) {
		device->dev.type = &mali_pm_device; /* We should probably use the pm_domain instead of type on newer kernels */
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
		pm_runtime_set_autosuspend_delay(&device->dev, 1000);
		pm_runtime_use_autosuspend(&device->dev);
#endif
		pm_runtime_enable(&device->dev);
#endif
		mali_meson_init_finish(device);
	}

	mali_gp_reset_fail = 0;
	mali_core_timeout  = 0;

	return err;
}

int mali_platform_device_deinit(struct platform_device *device)
{
	MALI_IGNORE(device);

	printk("%s, %d\n", __FILE__, __LINE__);
	MALI_DEBUG_PRINT(4, ("mali_platform_device_deinit() called\n"));


	mali_meson_uninit(device);

	return 0;
}

#if 0
static int param_set_core_scaling(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val, kp);
	printk("%s, %d\n", __FILE__, __LINE__);

	if (1 == mali_core_scaling_enable) {
		mali_core_scaling_sync(mali_executor_get_num_cores_enabled());
	}
	return ret;
}

static struct kernel_param_ops param_ops_core_scaling = {
	.set = param_set_core_scaling,
	.get = param_get_int,
};

module_param_cb(mali_core_scaling_enable, &param_ops_core_scaling, &mali_core_scaling_enable, 0644);
MODULE_PARM_DESC(mali_core_scaling_enable, "1 means to enable core scaling policy, 0 means to disable core scaling policy");
#endif
