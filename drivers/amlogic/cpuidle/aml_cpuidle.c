// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"aml-cpuidle: " fmt

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <asm/system_misc.h>
#include <linux/kdebug.h>
#include <linux/arm-smccc.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>

#define AML_CPUIDLE_NAME "amlcpuidle"

static unsigned int suspend_power_cmd;
static unsigned int resume_power_cmd;

bool is_aml_cpuidle_enabled(void)
{
	if (suspend_power_cmd && resume_power_cmd)
		return true;
	else
		return false;
}

void do_aml_resume_power(void)
{
	struct arm_smccc_res res __attribute__((unused));

	if (!resume_power_cmd)
		return;

	arm_smccc_smc(resume_power_cmd, 0, 0, 0, 0, 0, 0, 0, &res);
	smp_send_reschedule(1);
}

static void do_aml_suspend_power(struct work_struct *work)
{
	int i;
	struct arm_smccc_res res;

	if (!suspend_power_cmd)
		return;

	for (i = 0; i < 10; i++) {
		arm_smccc_smc(suspend_power_cmd, 0, 0, 0, 0, 0, 0, 0, &res);
		if (res.a0 == 1)
			break;
		usleep_range(1500, 2000);
		//pr_info("Retrying again to check for CPU idle\n");
	}
}

static DECLARE_WORK(aml_suspend_power_work, do_aml_suspend_power);

void aml_suspend_power_handler(void)
{
	unsigned int cpu;

	if (!suspend_power_cmd)
		return;

	/* if only cpu0 online, the IPI is rtos interrupt*/
	if (num_online_cpus() <= 1)
		return;

	cpu = smp_processor_id();
	if (cpu == 0)
		schedule_work(&aml_suspend_power_work);
}

static int aml_cpuidle_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret;

	ret = of_property_read_u32(np, "suspend_power", &suspend_power_cmd);
	if (ret) {
		pr_info("suspend power cmd is not defined in dts!\n");
		return ret;
	}

	ret = of_property_read_u32(np, "resume_power", &resume_power_cmd);
	if (ret) {
		pr_info("resume power cmd is not defined in dts!\n");
		return ret;
	}

	pr_info("aml cpuidle probe new!\n");
	return 0;
}

static const struct of_device_id aml_cpuidle_dt_match[] = {
	{
		.compatible = "amlogic,cpuidle",
	},
};

static struct platform_driver aml_cpuidle_driver = {
	.driver = {
		.name = AML_CPUIDLE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = aml_cpuidle_dt_match,
	},
	.probe = aml_cpuidle_probe,
};

static int __init aml_cpuidle_init(void)
{
	return platform_driver_register(&aml_cpuidle_driver);
}
core_initcall(aml_cpuidle_init);
