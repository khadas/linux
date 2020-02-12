/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __GPUCORE_COOLING_H__
#define __GPUCORE_COOLING_H__

#include <linux/thermal.h>
#include <linux/cpumask.h>
struct gpucore_cooling_device {
	int id;
	struct thermal_cooling_device *cool_dev;
	unsigned int gpucore_state;
	unsigned int gpucore_val;
	int max_gpu_core_num;
	unsigned int (*set_max_pp_num)(unsigned int pp);
	struct device_node *np;
	int stop_flag;
};

#define GPU_STOP 0x80000000

#ifdef CONFIG_AMLOGIC_GPUCORE_THERMAL

/**
 * gpucore_cooling_register - function to create gpucore cooling device.
 * @clip_cpus: cpumask of cpus where the frequency constraints will happen
 */
int gpucore_cooling_register(struct gpucore_cooling_device *gcd);

/**
 * gpucore_cooling_unregister - function to remove gpucore cooling device.
 * @cdev: thermal cooling device pointer.
 */
void gpucore_cooling_unregister(struct thermal_cooling_device *cdev);
struct gpucore_cooling_device *gpucore_cooling_alloc(void);
void save_gpucore_thermal_para(struct device_node *node);
#else
inline struct gpucore_cooling_device *gpucore_cooling_alloc(void)
{
	return NULL;
}

inline int gpucore_cooling_register(struct gpucore_cooling_device *gcd)
{
	return 0;
}

inline void gpucore_cooling_unregister(struct thermal_cooling_device *cdev)
{
}

inline void save_gpucore_thermal_para(struct device_node *n)
{
}
#endif

#endif /* __CPU_COOLING_H__ */
