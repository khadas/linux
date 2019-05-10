/*
 *
 * (C) COPYRIGHT 2015, 2017 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */


#ifdef CONFIG_DEVFREQ_THERMAL
#include <linux/version.h>
#include <linux/devfreq_cooling.h>
#include <linux/thermal.h>

#define FALLBACK_STATIC_TEMPERATURE 55000

static unsigned long t83x_static_power(unsigned long voltage)
{
#if 0
	struct thermal_zone_device *tz;
	unsigned long temperature, temp;
	unsigned long temp_squared, temp_cubed, temp_scaling_factor;
	const unsigned long coefficient = (410UL << 20) / (729000000UL >> 10);
	const unsigned long voltage_cubed = (voltage * voltage * voltage) >> 10;

	tz = thermal_zone_get_zone_by_name("gpu");
	if (IS_ERR(tz)) {
		pr_warn_ratelimited("Error getting gpu thermal zone (%ld), not yet ready?\n",
				PTR_ERR(tz));
		temperature = FALLBACK_STATIC_TEMPERATURE;
	} else {
		int ret;

		ret = tz->ops->get_temp(tz, &temperature);
		if (ret) {
			pr_warn_ratelimited("Error reading temperature for gpu thermal zone: %d\n",
					ret);
			temperature = FALLBACK_STATIC_TEMPERATURE;
		}
	}

	/* Calculate the temperature scaling factor. To be applied to the
	 * voltage scaled power.
	 */
	temp = temperature / 1000;
	temp_squared = temp * temp;
	temp_cubed = temp_squared * temp;
	temp_scaling_factor =
			(2 * temp_cubed)
			- (80 * temp_squared)
			+ (4700 * temp)
			+ 32000;

	return (((coefficient * voltage_cubed) >> 20)
			* temp_scaling_factor)
				/ 1000000;
#else
	return 0;
#endif
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
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 16))
struct devfreq_cooling_ops t83x_model_ops = {
#else
struct devfreq_cooling_power t83x_model_ops = {
#endif
	.get_static_power = t83x_static_power,
	.get_dynamic_power = t83x_dynamic_power,
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
