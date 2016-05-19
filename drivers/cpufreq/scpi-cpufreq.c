/*
 * SCPI CPUFreq Interface driver
 *
 * It provides necessary ops to arm_big_little cpufreq driver.
 *
 * Copyright (C) 2014 ARM Ltd.
 * Sudeep Holla <sudeep.holla@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#define DEBUG
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/opp.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "arm_big_little.h"
#include <linux/amlogic/cpu_version.h>

#include <linux/pm_opp.h>
int dev_pm_opp_add(struct device *dev, unsigned long freq,
		   unsigned long u_volt);
static struct cpufreq_frequency_table *meson_freq_table[3];

static int scpi_init_opp_table(struct device *cpu_dev)
{
	u8 domain = topology_physical_package_id(cpu_dev->id);
	struct scpi_opp *opps;
	int idx, ret = 0, max_opp;
	struct scpi_opp_entry *opp;

	opps = scpi_dvfs_get_opps(domain);
	if (IS_ERR(opps))
		return PTR_ERR(opps);

	opp = opps->opp;
	max_opp = opps->count;
	meson_freq_table[domain] =
		kzalloc(sizeof(*meson_freq_table) * (max_opp + 1), GFP_KERNEL);
	for (idx = 0; idx < max_opp; idx++, opp++) {
		meson_freq_table[domain][idx].driver_data = idx;
		meson_freq_table[domain][idx].frequency = opp->freq_hz/1000;
		ret = dev_pm_opp_add(cpu_dev, opp->freq_hz, opp->volt_mv*1000);
		if (ret) {
			dev_warn(cpu_dev, "failed to add opp %uHz %umV\n",
					opp->freq_hz, opp->volt_mv);
			return ret;
		}
	}
	meson_freq_table[domain][idx].driver_data = idx;
	meson_freq_table[domain][idx].frequency = CPUFREQ_TABLE_END;
	return ret;
}

static int scpi_get_transition_latency(struct device *cpu_dev)
{
	u8 domain = topology_physical_package_id(cpu_dev->id);
	struct scpi_opp *opp;

	opp = scpi_dvfs_get_opps(domain);
	if (IS_ERR(opp))
		return PTR_ERR(opp);

	return opp->latency * 1000; /* SCPI returns in uS */
}

static struct cpufreq_arm_bL_ops scpi_cpufreq_ops = {
	.name	= "scpi",
	.get_transition_latency = scpi_get_transition_latency,
	.init_opp_table = scpi_init_opp_table,
};

static int scpi_cpufreq_probe(struct platform_device *pdev)
{
	int val;
	if (!is_meson_gxm_cpu())
		return -1;
	val =  bL_cpufreq_register(&scpi_cpufreq_ops);
	return val;
}

static int scpi_cpufreq_remove(struct platform_device *pdev)
{
	bL_cpufreq_unregister(&scpi_cpufreq_ops);
	return 0;
}

static struct of_device_id scpi_cpufreq_of_match[] = {
	{ .compatible = "arm, scpi-cpufreq" },
	{},
};
MODULE_DEVICE_TABLE(of, scpi_cpufreq_of_match);

static struct platform_driver scpi_cpufreq_platdrv = {
	.driver = {
		.name	= "scpi-cpufreq",
		.owner	= THIS_MODULE,
		.of_match_table = scpi_cpufreq_of_match,
	},
	.probe		= scpi_cpufreq_probe,
	.remove		= scpi_cpufreq_remove,
};
module_platform_driver(scpi_cpufreq_platdrv);

MODULE_LICENSE("GPL");

int dev_pm_opp_init_cpufreq_table(struct device *dev,
				  struct cpufreq_frequency_table **table)
{
	u8 domain = topology_physical_package_id(dev->id);
	*table = &meson_freq_table[domain][0];
	return 0;
}

void dev_pm_opp_free_cpufreq_table(struct device *dev,
						 struct cpufreq_frequency_table
						 **table)
{
	*table = NULL;
}

