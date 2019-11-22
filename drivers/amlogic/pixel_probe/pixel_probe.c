/*
 * drivers/amlogic/pixel_probe/pixel_probe.c
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

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/dma-contiguous.h>
#include <asm/compiler.h>
#include <linux/cma.h>
#include <linux/arm-smccc.h>
#undef pr_fmt
#define pr_fmt(fmt) "pixel_probe: " fmt


static DEFINE_MUTEX(pixel_probe_mutex);
static unsigned int vpp_probe_func;
static unsigned int vdin_probe_func;

#define PROBE_ENABLE 1
#define PROBE_DISABLE 0

static int pixel_probe_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	if (of_property_read_u32(np, "vpp_probe_func", &vpp_probe_func)) {
		pr_err("can't get vpp probe command!\n");
		return -1;
	}
	if (of_property_read_u32(np, "vdin_probe_func", &vdin_probe_func)) {
		pr_err("can't get vdin probe command!\n");
		return -1;
	}

	return 0;
}

static const struct of_device_id pixel_probe_dt_match[] = {
	{ .compatible = "amlogic, pixel_probe" },
	{ /* sentinel */ },
};

static  struct platform_driver pixel_probe_platform_driver = {
	.probe		= pixel_probe_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "pixel",
		.of_match_table	= pixel_probe_dt_match,
	},
};

int __init meson_pixel_probe_init(void)
{
	int ret;

	ret = platform_driver_register(&pixel_probe_platform_driver);
	return ret;
}

subsys_initcall(meson_pixel_probe_init);


int vpp_probe_enable(void)
{
	struct arm_smccc_res res;

	if (!vpp_probe_func) {
		pr_err("vpp probe func error!\n");
		return -1;
	}

	mutex_lock(&pixel_probe_mutex);
	arm_smccc_smc(vpp_probe_func, PROBE_ENABLE, 0, 0, 0, 0, 0, 0, &res);
	mutex_unlock(&pixel_probe_mutex);

	return res.a0;
}

int vpp_probe_disable(void)
{
	struct arm_smccc_res res;

	if (!vpp_probe_func) {
		pr_err("vpp probe func error!\n");
		return -1;
	}

	mutex_lock(&pixel_probe_mutex);
	arm_smccc_smc(vpp_probe_func, PROBE_DISABLE, 0, 0, 0, 0, 0, 0, &res);
	mutex_unlock(&pixel_probe_mutex);

	return res.a0;
}

int vdin_probe_enable(void)
{
	struct arm_smccc_res res;

	if (!vdin_probe_func) {
		pr_err("vdin probe func error!\n");
		return -1;
	}

	mutex_lock(&pixel_probe_mutex);
	arm_smccc_smc(vdin_probe_func, PROBE_ENABLE, 0, 0, 0, 0, 0, 0, &res);
	mutex_unlock(&pixel_probe_mutex);

	return res.a0;
}

int vdin_probe_disable(void)
{
	struct arm_smccc_res res;

	if (!vdin_probe_func) {
		pr_err("vdin probe func error!\n");
		return -1;
	}

	mutex_lock(&pixel_probe_mutex);
	arm_smccc_smc(vdin_probe_func, PROBE_DISABLE, 0, 0, 0, 0, 0, 0, &res);
	mutex_unlock(&pixel_probe_mutex);

	return res.a0;
}
