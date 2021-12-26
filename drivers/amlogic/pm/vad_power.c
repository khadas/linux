// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/amlogic/pm/vad_power.c
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
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/errno.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/pm.h>
#include <linux/kobject.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include "../../gpio/gpiolib.h"
#include "../../gpio/gpiolib-of.h"
#include "vad_power.h"

static int default_pm_init(struct platform_device *pdev)
{
	dev_warn(&pdev->dev,
			"Does not match the item type, use the default [init] function.\n");
	return 0;
}

static int defalut_pm_suspend(struct platform_device *pdev)
{
	dev_warn(&pdev->dev,
			"Does not match the item type, use the default [suspend] function.\n");
	return 0;
}

static int default_pm_resume(struct platform_device *pdev)
{
	dev_warn(&pdev->dev,
			"Does not match the item type, use the default [resume] function.\n");
	return 0;
}

struct pm_ops pm_default_ops = {
	.vad_pm_init = default_pm_init,
	.vad_pm_suspend = defalut_pm_suspend,
	.vad_pm_resume = default_pm_resume,
};

struct pm_item_table {
	const char *name;
	struct pm_ops *ops;
};

struct pm_item_table pm_support_item_list[] = {
	{"T3", &t3_pm_ops},
	{"T7", &pm_default_ops},
};

struct pm_ops *find_item_ops(const char *name)
{
	int i;

	if (!name)
		return &pm_default_ops;

	/* Find matching item form the table */
	for (i = 0; i < ARRAY_SIZE(pm_support_item_list); i++) {
		if (!strcmp(name, pm_support_item_list[i].name))
			return pm_support_item_list[i].ops;
	}

	return &pm_default_ops;
}

int vad_wakeup_power_init(struct platform_device *pdev, struct pm_data *p_data)
{
	struct device_node *np = pdev->dev.of_node;
	int ret;
	u32 paddr = 0;

	/* check vad global flag */
	ret = of_property_read_u32(pdev->dev.of_node,
			"vad_wakeup_disable", &paddr);
	if (!ret) {
		p_data->vad_wakeup_disable = paddr;
		dev_info(&pdev->dev, "vad_wakeup_disable: 0x%x\n", paddr);
	} else {
		dev_warn(&pdev->dev, "Vad enable not detected ~~~\n");
		p_data->vad_wakeup_disable = 1;
	}

	/* Resolve the project name in dts */
	p_data->name = of_get_property(np, "vad-item-name", NULL);
	if (!p_data->name) {
		dev_warn(&pdev->dev,
			"Not found item name in dts.The default item will be used!\n");
	}

	p_data->ops = find_item_ops(p_data->name);

	p_data->ops->vad_pm_init(pdev);

	return 0;
}

int vad_wakeup_power_suspend(struct device *dev)
{
	struct pm_data *p_data = dev_get_drvdata(dev);
	struct pm_ops *ops = p_data->ops;

	dev_info(dev, "Entry %s\n", __func__);

	if (!is_pm_s2idle_mode()) {
		dev_info(dev, "Power mode is not freeze, return directly\n");
		return 0;
	}

	if (p_data->vad_wakeup_disable) {
		dev_info(dev, "vad function don't enable, return directly!!!\n");
		return 0;
	}

	ops->vad_pm_suspend(to_platform_device(dev));

	return 0;
}

int vad_wakeup_power_resume(struct device *dev)
{
	struct pm_data *p_data = dev_get_drvdata(dev);
	struct pm_ops *ops = p_data->ops;

	dev_info(dev, "Entry %s\n", __func__);

	if (!is_pm_s2idle_mode()) {
		dev_info(dev, " Power mode is not freeze!!!\n");
		dev_info(dev, "return directly!!\n");
		return 0;
	}

	if (p_data->vad_wakeup_disable) {
		dev_info(dev, "vad function don't enable !!!\n");
		dev_info(dev, "return directly!!\n");
		return 0;
	}

	ops->vad_pm_resume(to_platform_device(dev));

	return 0;
}
