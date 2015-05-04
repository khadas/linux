/*
 * drivers/amlogic/pinctrl/sample_code.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/amlogic/pinctrl_amlogic.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_address.h>
#include <linux/amlogic/gpio-amlogic.h>


static struct of_device_id m8_pinctrl_of_table[] = {
	{
		.compatible = "amlogic, pinmux-sample",
	},
	{},
};

static int  m8_pmx_probe(struct platform_device *pdev)
{
#if 0
	struct gpio_desc *desc;
	desc = gpiod_get_index(&pdev->dev, NULL, 0);
	if (IS_ERR(desc))
		pr_info("get desc error\n");

	gpiod_direction_output(desc, 1);

	gpiod_direction_input(desc);


	desc = gpiod_get_index(&pdev->dev, NULL, 1);
	if (IS_ERR(desc))
		pr_info("get desc error\n");

	gpiod_direction_output(desc, 1);

	gpiod_direction_input(desc);
#endif
	return 0;
}

static int  m8_pmx_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver m8_pmx_driver = {
	.driver = {
		.name = "pinmux-sample",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(m8_pinctrl_of_table),
	},
	.probe = m8_pmx_probe,
	.remove = m8_pmx_remove,
};

static int __init m8_pmx_init(void)
{
	return platform_driver_register(&m8_pmx_driver);
}
arch_initcall_sync(m8_pmx_init);


