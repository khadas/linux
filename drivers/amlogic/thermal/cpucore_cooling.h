/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __CPUCORE_COOLING_H__
#define __CPUCORE_COOLING_H__

#include <linux/thermal.h>
#include <linux/cpumask.h>
struct cpucore_cooling_device {
	int id;
	struct thermal_cooling_device *cool_dev;
	unsigned int cpucore_state;
	unsigned int cpucore_val;
	struct list_head node;
	int max_cpu_core_num;
	int cluster_id;
	int stop_flag;
};

#define CPU_STOP 0x80000000

/**
 * cpucore_cooling_register - function to create cpucore cooling device.
 * @clip_cpus: cpumask of cpus where the frequency constraints will happen
 */
struct thermal_cooling_device *cpucore_cooling_register(struct device_node *np,
							int cluster_id);

/**
 * cpucore_cooling_unregister - function to remove cpucore cooling device.
 * @cdev: thermal cooling device pointer.
 */
void cpucore_cooling_unregister(struct thermal_cooling_device *cdev);
extern struct platform_driver meson_tsensor_driver;

int cpu_hotplug_init(void);
#endif /* __CPU_COOLING_H__ */
