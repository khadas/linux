/*
 * drivers/misc/khadas-red-led.c
 *
 * Copyright (C) 2019 Khadas, Inc. All rights reserved.
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
#include <linux/khadas-hwver.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/amlogic/pm.h>
#include <linux/sysfs.h>

#define HW_VERSION_VIM1_V12_STR    "VIM1.V12"
#define HW_VERSION_VIM1_V13_STR    "VIM1.V13"
#define HW_VERSION_VIM2_V12_STR    "VIM2.V12"
#define HW_VERSION_VIM2_V14_STR    "VIM2.V14"
#define HW_VERSION_VIM3_V11_STR    "VIM3.V11"
#define HW_VERSION_VIM3_V12_STR    "VIM3.V12"
#define HW_VERSION_UNKNOW_STR      "Unknow"

static char khadas_hwver[64];
static ssize_t show_hwver(struct class *cls,
			struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", khadas_hwver);

}

static struct class_attribute hwver_class_attrs[] = {
	__ATTR(hwver, 0644, show_hwver, NULL),
};

static void create_hwver_attrs(void)
{
	int i;
	struct class *k_class;
	k_class = class_create(THIS_MODULE, "khadas");
	if (IS_ERR(k_class)) {
		printk("create k_class debug class fail\n");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(hwver_class_attrs); i++) {
		if (class_create_file(k_class, &hwver_class_attrs[i]))
			printk("create khadas hwver attribute %s fail\n", hwver_class_attrs[i].attr.name);

	}

}


int get_hwver(void)
{
	if (khadas_hwver == NULL)
		return HW_VERSION_UNKNOW;
	if (strstr(khadas_hwver, HW_VERSION_VIM1_V12_STR)) {
		return HW_VERSION_VIM1_V12;
	} else if (strstr(khadas_hwver, HW_VERSION_VIM1_V13_STR)) {
		return HW_VERSION_VIM1_V13;
	} else if (strstr(khadas_hwver, HW_VERSION_VIM2_V12_STR)) {
		return HW_VERSION_VIM2_V12;
	} else if (strstr(khadas_hwver, HW_VERSION_VIM2_V14_STR)) {
		return HW_VERSION_VIM2_V14;
	} else if (strstr(khadas_hwver, HW_VERSION_VIM3_V11_STR)) {
		return HW_VERSION_VIM3_V11;
	} else if (strstr(khadas_hwver, HW_VERSION_VIM3_V12_STR)) {
		return HW_VERSION_VIM3_V12;
	} else {
		return HW_VERSION_UNKNOW;
	}
}

static int __init hwver_setup(char *str)
{
	if (str == NULL)
		snprintf(khadas_hwver, sizeof(khadas_hwver), "%s", HW_VERSION_UNKNOW_STR);
	else
		snprintf(khadas_hwver, sizeof(khadas_hwver), "%s", str);
	return 1;
}
__setup("hwver=", hwver_setup);


static int khadas_hwver_probe(struct platform_device *pdev)
{
	printk("%s\n", __func__);
	create_hwver_attrs();
	return 0;
}


static const struct of_device_id hwver_dt_match[] = {
	{	.compatible = "khadas-hwver", },
	{},
};

static struct platform_driver khadas_hwver_driver = {
	.probe = khadas_hwver_probe,
	.driver = {
		.name = "hwver",
		.of_match_table = hwver_dt_match,
	},
};

static int __init khadas_hwver_driver_init(void)
{
	return platform_driver_register(&khadas_hwver_driver);
}

static void __exit khadas_hwver_driver_exit(void)
{
	return platform_driver_unregister(&khadas_hwver_driver);
}

subsys_initcall(khadas_hwver_driver_init);
module_exit(khadas_hwver_driver_exit);
MODULE_AUTHOR("Terry terry@khadas.com");
MODULE_DESCRIPTION("Hardware version Driver for khadas VIMs");
MODULE_LICENSE("GPL");
