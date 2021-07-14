// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "cpuinfo: " fmt

#include <linux/export.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/amlogic/iomap.h>
#ifdef CONFIG_AMLOGIC_SEC
#include <linux/amlogic/secmon.h>
#endif
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/amlogic/cpu_info.h>
#include <linux/arm-smccc.h>
#include <linux/amlogic/cpu_version.h>

static unsigned char cpuinfo_chip_id[CHIPID_LEN];

static noinline int fn_smc(u64 function_id,
			   u64 arg0,
			   u64 arg1,
			   u64 arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id,
		      (unsigned long)arg0,
		      (unsigned long)arg1,
		      (unsigned long)arg2,
		      0, 0, 0, 0, &res);
	return res.a0;
}

static int cpuinfo_probe(struct platform_device *pdev)
{
	void __iomem *shm_out;
	struct device_node *np = pdev->dev.of_node;
	int cmd, ret;

	if (of_property_read_u32(np, "cpuinfo_cmd", &cmd))
		return -EINVAL;

	shm_out = get_meson_sm_output_base();
	if (!shm_out) {
		pr_err("failed to allocate shared memory\n");
		return -ENOMEM;
	}

	meson_sm_mutex_lock();
	ret = fn_smc(cmd, 2, 0, 0);
	if (ret == 0) {
		int version = *((unsigned int *)shm_out);

		if (version == 2) {
			memcpy((void *)&cpuinfo_chip_id[0],
			       (void *)shm_out + 4,
			       CHIPID_LEN);
		} else {
			pr_err("failed to get version\n");
		}
	} else {
		ret = -EPROTO;
	}
	meson_sm_mutex_unlock();

	return ret;
}

void cpuinfo_get_chipid(unsigned char *cid, unsigned int size)
{
	memcpy(&cid[0], cpuinfo_chip_id, size);
}

static const struct of_device_id cpuinfo_dt_match[] = {
	{ .compatible = "amlogic, cpuinfo" },
	{ /* sentinel */ }
};

static  struct platform_driver cpuinfo_platform_driver = {
	.probe		= cpuinfo_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "cpuinfo",
		.of_match_table	= cpuinfo_dt_match,
	},
};

static int __init meson_cpuinfo_init(void)
{
	meson_cpu_version_init();

	return  platform_driver_register(&cpuinfo_platform_driver);
}
subsys_initcall(meson_cpuinfo_init);
