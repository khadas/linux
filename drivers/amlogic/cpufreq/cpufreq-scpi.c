/*
 * drivers/amlogic/cpufreq/meson-cpufreq.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <asm/smp_plat.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/machine.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/pm_opp.h>
#include <linux/cpu.h>
#include <linux/amlogic/cpu_version.h>

struct meson_cpufreq {
	struct device *dev;
	struct clk *armclk;
};

static struct meson_cpufreq cpufreq;

static DEFINE_MUTEX(meson_cpufreq_mutex);
static struct cpufreq_frequency_table *meson_freq_table;


static int meson_cpufreq_verify(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *freq_table =
		cpufreq_frequency_get_table(policy->cpu);

	if (freq_table)
		return cpufreq_frequency_table_verify(policy, freq_table);

	if (policy->cpu)
		return -EINVAL;

	cpufreq_verify_within_limits(
				 policy, policy->cpuinfo.min_freq,
				 policy->cpuinfo.max_freq);

	policy->min = clk_round_rate(cpufreq.armclk, policy->min * 1000) / 1000;
	policy->max = clk_round_rate(cpufreq.armclk, policy->max * 1000) / 1000;
	cpufreq_verify_within_limits(
				 policy, policy->cpuinfo.min_freq,
				 policy->cpuinfo.max_freq);
	return 0;
}

#if (defined CONFIG_SMP) && (defined CONFIG_HAS_EARLYSUSPEND)
static int early_suspend_flag;
#include <linux/earlysuspend.h>
static void meson_system_early_suspend(struct early_suspend *h)
{
	early_suspend_flag = 1;
}

static void meson_system_late_resume(struct early_suspend *h)
{
	early_suspend_flag = 0;
}
static struct early_suspend early_suspend = {
		.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
		.suspend = meson_system_early_suspend,
		.resume = meson_system_late_resume,

};

#endif
static int meson_cpufreq_target_locked(
						struct cpufreq_policy *policy,
						unsigned int target_freq,
						unsigned int relation)
{
	struct cpufreq_freqs freqs;
	uint cpu = policy ? policy->cpu : 0;
	int ret = -EINVAL;

	if (cpu > (num_possible_cpus() - 1)) {
		pr_err("cpu %d set target freq error\n", cpu);
		return ret;
	}

	/* Ensure desired rate is within allowed range.  Some govenors
	 * (ondemand) will just pass target_freq=0 to get the minimum. */
	if (policy) {
		if (target_freq < policy->min)
			target_freq = policy->min;
		if (target_freq > policy->max)
			target_freq = policy->max;
	}


	freqs.old = clk_get_rate(cpufreq.armclk) / 1000;
	freqs.new = clk_round_rate(cpufreq.armclk, target_freq * 1000) / 1000;
	freqs.cpu = cpu;

	if (freqs.old == freqs.new)
		return ret;

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);

#ifndef CONFIG_CPU_FREQ_DEBUG
	pr_debug("cpufreq-meson: CPU%d transition: %u --> %u\n",
		   freqs.cpu, freqs.old, freqs.new);
#endif





	ret = clk_set_rate(cpufreq.armclk, freqs.new * 1000);
	if (ret)
		goto out;


	freqs.new = clk_get_rate(cpufreq.armclk) / 1000;



out:
	freqs.new = clk_get_rate(cpufreq.armclk) / 1000;
	cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);

	return ret;
}

static int meson_cpufreq_target(
						struct cpufreq_policy *policy,
						unsigned int target_freq,
						unsigned int relation)
{
	int ret;

	mutex_lock(&meson_cpufreq_mutex);
	ret = meson_cpufreq_target_locked(policy, target_freq, relation);
	mutex_unlock(&meson_cpufreq_mutex);

	return ret;
}

static unsigned int meson_cpufreq_get(unsigned int cpu)
{
	unsigned long rate;
	if (cpu > (num_possible_cpus()-1)) {
		pr_err("cpu %d on current thread error\n", cpu);
		return 0;
	}
	rate = clk_get_rate(cpufreq.armclk) / 1000;
	return rate;
}

static ssize_t opp_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int size = 0;
	struct dev_pm_opp *opp;
	unsigned long freq = 0, voltage;

	while (1) {
		opp = dev_pm_opp_find_freq_ceil(dev, &freq);
		if (IS_ERR_OR_NULL(opp)) {
			pr_debug("can't found opp for freq:%ld\n", freq);
			break;
		}
		freq = dev_pm_opp_get_freq(opp);
		voltage = dev_pm_opp_get_voltage(opp);
		size += sprintf(buf + size, "%10ld  %7ld\n", freq, voltage);
		freq += 1;	/* increase for next freq */
	}
	return size;
}
static DEVICE_ATTR(opp, 0444, opp_show, NULL);

static int meson_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *freq_table = NULL;
	int index = 0;
	u8 domain = 0;
	struct scpi_opp *opps;
	int idx, max_opp;
	struct scpi_opp_entry *opp;
	struct device *cpu_dev;
	int ret;
	if (policy->cpu != 0)
		return -EINVAL;

	if (policy->cpu > (num_possible_cpus() - 1)) {
		pr_err("cpu %d on current thread error\n", policy->cpu);
		return -1;
	}

	opps = scpi_dvfs_get_opps(domain);
	if (IS_ERR(opps))
		return PTR_ERR(opps);

	opp = opps->opp;
	max_opp = opps->count;
	cpu_dev  = get_cpu_device(policy->cpu);
	meson_freq_table = kzalloc(sizeof(*meson_freq_table) * (max_opp + 1),
								GFP_KERNEL);
	for (idx = 0; idx < max_opp; idx++, opp++) {
		meson_freq_table[idx].driver_data = idx;
		meson_freq_table[idx].frequency = opp->freq_hz/1000;
		ret = dev_pm_opp_add(cpu_dev, opp->freq_hz,
				     opp->volt_mv*1000);
		if (ret) {
			pr_warn("failed to add opp %uHz %umV\n",
				opp->freq_hz, opp->volt_mv);
			return ret;
		}
	}

	ret = device_create_file(cpu_dev, &dev_attr_opp);
	if (ret < 0)
		dev_err(cpu_dev, "create opp sysfs failed\n");

	meson_freq_table[idx].driver_data = idx;
	meson_freq_table[idx].frequency = CPUFREQ_TABLE_END;
	cpufreq_frequency_table_get_attr(meson_freq_table,
							 policy->cpu);
	freq_table = cpufreq_frequency_get_table(policy->cpu);
	while (freq_table[index].frequency != CPUFREQ_TABLE_END)
		index++;
	index -= 1;

	policy->min = freq_table[0].frequency;
	policy->max = freq_table[index].frequency;
	policy->cpuinfo.min_freq = clk_round_rate(cpufreq.armclk, 0) / 1000;
	policy->cpuinfo.max_freq =
		clk_round_rate(cpufreq.armclk, 0xffffffff) / 1000;

	if (policy->min < policy->cpuinfo.min_freq)
		policy->min = policy->cpuinfo.min_freq;
	if (policy->max > policy->cpuinfo.max_freq)
		policy->max = policy->cpuinfo.max_freq;
	policy->cur =  clk_round_rate(cpufreq.armclk,
					  clk_get_rate(cpufreq.armclk)) / 1000;

	/* FIXME: what's the actual transition time? */
	policy->cpuinfo.transition_latency = 200 * 1000;

		/* Both cores must be set to same frequency.
		* Set affected_cpus to all. */
	cpumask_setall(policy->cpus);

	return 0;
}

static struct freq_attr *meson_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static int meson_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return 0;
}

static int meson_cpufreq_resume(struct cpufreq_policy *policy)
{
	return 0;
}

static struct cpufreq_driver meson_cpufreq_driver = {
	.flags      = CPUFREQ_STICKY,
	.verify     = meson_cpufreq_verify,
	.target     = meson_cpufreq_target,
	.get        = meson_cpufreq_get,
	.init       = meson_cpufreq_init,
	.name       = "meson_cpufreq",
	.attr       = meson_cpufreq_attr,
	.suspend    = meson_cpufreq_suspend,
	.resume     = meson_cpufreq_resume
};

static int __init meson_cpufreq_probe(struct platform_device *pdev)
{
	if (is_meson_gxm_cpu())
		return -1;
	dev_info(&pdev->dev, "enter  cpufreq\n");
	cpufreq.dev = &pdev->dev;
	cpufreq.armclk = clk_get(&pdev->dev, "cpu_clk");
	if (IS_ERR(cpufreq.armclk)) {
		dev_err(cpufreq.dev, "Unable to get ARM clock\n");
		return PTR_ERR(cpufreq.armclk);
	}
	dev_info(&pdev->dev, "probe  cpufreq okay\n");
	return cpufreq_register_driver(&meson_cpufreq_driver);
}


static int  meson_cpufreq_remove(struct platform_device *pdev)
{
	return cpufreq_unregister_driver(&meson_cpufreq_driver);
}

#ifdef CONFIG_OF
static const struct of_device_id amlogic_cpufreq_meson_dt_match[] = {
	{	.compatible = "amlogic, cpufreq-scpi",
	},
	{},
};
#else
#define amlogic_cpufreq_meson_dt_match NULL
#endif


static struct platform_driver meson_cpufreq_parent_driver = {
	.driver = {
		.name   = "cpufreq-scpi",
		.owner  = THIS_MODULE,
		.of_match_table = amlogic_cpufreq_meson_dt_match,
	},
	.remove = meson_cpufreq_remove,
};

static int __init meson_cpufreq_parent_init(void)
{
#if (defined CONFIG_SMP) && (defined CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&early_suspend);
#endif
	return platform_driver_probe(&meson_cpufreq_parent_driver,
							meson_cpufreq_probe);
}
late_initcall(meson_cpufreq_parent_init);
