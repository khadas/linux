/*
 * Khadas rk3399 board type detect driver
 *
 * Written by: Terry <terry@szwesion.com>
 *
 * Copyright (C) 2019 Wesion Technologies Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/device.h>


static int board_type = KHADAS_EDGE;

int get_board_type(void)
{
	int type = 0;
	if (strstr(boot_command_line, "board.type=2"))
		type = KHADAS_EDGEV;  //edge_v
	else if (strstr(boot_command_line, "board.type=3"))
		type = KHADAS_CAPTAIN;  //captain
	else
		type = KHADAS_EDGE;  //edge
	return type;
}
EXPORT_SYMBOL(get_board_type);

static ssize_t show_type(struct class *cls,
			 struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", board_type);
}

static struct class_attribute type_class_attrs[] = {
	__ATTR(type, 0644, show_type, NULL),
};

static void create_boardtype_attrs(void)
{
	struct class *board_class;
	int i;
	board_class = class_create(THIS_MODULE, "board");
	if (IS_ERR(board_class)) {
		pr_err("create board_class debug class fail\n");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(type_class_attrs); i++) {
		if (class_create_file(board_class, &type_class_attrs[i]))
			pr_err("create board attribute %s fail\n", type_class_attrs[i].attr.name);
	}
}

static int khadas_boardtype_probe(struct platform_device *pdev)
{
	printk("%s\n", __func__);
	board_type = get_board_type();
	create_boardtype_attrs();
	return 0;

}


static int khadas_boardtype_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id of_khadas_boardtype_match[] = {
	{ .compatible = "khadas-boardtype", },
	{},
};

static struct platform_driver khadas_boardtype_driver = {
	.probe  = khadas_boardtype_probe,
	.remove = khadas_boardtype_remove,
	.driver = {
		.name = "boardtype",
		.of_match_table = of_match_ptr(of_khadas_boardtype_match),

	},
};

module_platform_driver(khadas_boardtype_driver);

MODULE_AUTHOR("Terry <terry@szwesion.com>");
MODULE_DESCRIPTION("Khadas rk3399 board type detect driver");
MODULE_LICENSE("GPL");
