/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __CPU_HOTPLUG_H_
#define __CPU_HOTPLUG_H_

#ifdef CONFIG_AMLOGIC_CPU_HOTPLUG
void cpufreq_set_max_cpu_num(unsigned int num, unsigned int cluster);
#else
static inline void cpufreq_set_max_cpu_num(unsigned int n, unsigned int c)
{
}

#endif
#endif
