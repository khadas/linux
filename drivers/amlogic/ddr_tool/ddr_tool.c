// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include "dmc_monitor.h"
#include "ddr_bandwidth.h"

#include <linux/amlogic/gki_module.h>

static int __init ddr_tool_init(void)
{
	int ret;

	ret = ddr_bandwidth_init();
	if (ret)
		return ret;

	ret = dmc_monitor_init();
	if (ret)
		ddr_bandwidth_exit();

	return ret;
}

static void __exit ddr_tool_exit(void)
{
	ddr_bandwidth_exit();
	dmc_monitor_exit();
}

subsys_initcall(ddr_tool_init);
module_exit(ddr_tool_exit);
MODULE_LICENSE("GPL v2");
