/*
 * meson_main2.h
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

#ifndef MESON_MAIN_H_
#define MESON_MAIN_H_
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 29))
#include <mach/cpu.h>
#endif

#include "mali_scaling.h"
#include "mali_clock.h"

u32 set_max_mali_freq(u32 idx);
u32 get_max_mali_freq(void);

int mali_meson_init_start(struct platform_device* ptr_plt_dev);
int mali_meson_init_finish(struct platform_device* ptr_plt_dev);
int mali_meson_uninit(struct platform_device* ptr_plt_dev);
int mpgpu_class_init(void);
void mpgpu_class_exit(void);
void mali_gpu_utilization_callback(int utilization_pp);

u32 mpgpu_get_utilization(void);
u32 mpgpu_get_util_gl_share(void);
u32 mpgpu_get_util_cl_share(u32 *util);
u32 mpgpu_get_gpu_err_count(void);

#endif /* MESON_MAIN_H_ */
