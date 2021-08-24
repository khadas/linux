/*
 * mali_kbase_config_devicetree.c
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

#ifdef CONFIG_DEVFREQ_THERMAL
#include <linux/version.h>
#include <linux/devfreq_cooling.h>
#include <linux/thermal.h>

#define FALLBACK_STATIC_TEMPERATURE 55000

#ifdef CONFIG_MALI_DEVFREQ
static unsigned long t83x_static_power(unsigned long voltage)
{
	return 0;
}

static unsigned long t83x_dynamic_power(unsigned long freq,
		unsigned long voltage)
{
	/* The inputs: freq (f) is in Hz, and voltage (v) in mV.
	 * The coefficient (c) is in mW/(MHz mV mV).
	 *
	 * This function calculates the dynamic power after this formula:
	 * Pdyn (mW) = c (mW/(MHz*mV*mV)) * v (mV) * v (mV) * f (MHz)
	 */
	const unsigned long v2 = (voltage * voltage) / 1000; /* m*(V*V) */
	const unsigned long f_mhz = freq / 1000000; /* MHz */
	const unsigned long coefficient = 3600; /* mW/(MHz*mV*mV) */

	return (coefficient * v2 * f_mhz) / 1000000; /* mW */
}
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 16))
struct devfreq_cooling_ops t83x_model_ops = {
#else
struct devfreq_cooling_power t83x_model_ops = {
#endif
#ifdef CONFIG_MALI_DEVFREQ
	.get_static_power = t83x_static_power,
	.get_dynamic_power = t83x_dynamic_power,
#endif
};

#endif

#include <mali_kbase_config.h>

int kbase_platform_early_init(void)
{
	/* Nothing needed at this stage */
	return 0;
}

static struct kbase_platform_config dummy_platform_config;

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &dummy_platform_config;
}

#ifndef CONFIG_OF
int kbase_platform_register(void)
{
	return 0;
}

void kbase_platform_unregister(void)
{
}
#endif
