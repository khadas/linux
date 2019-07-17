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
#include <linux/khadas-tca6408.h>

extern int tca6408_output_set_value(u8 value, u8 mask);

struct khadas_redled_data {
	int mode;
	int flag;
	struct class *led_class;
	struct delayed_work work;
};

enum khadas_redled_mode {
	KHADAS_RED_LED_HEARTBEAT = 0,
	KHADAS_RED_LED_ON,
	KHADAS_RED_LED_OFF,
};

struct khadas_redled_data *g_khadas_redled_data;

static void led_work_func(struct work_struct *_work)
{
	if (g_khadas_redled_data->flag == 1) {
		tca6408_output_set_value(1<<5, TCA_RED_LED_MASK);
		g_khadas_redled_data->flag = 0;
		schedule_delayed_work(&g_khadas_redled_data->work, msecs_to_jiffies(200));
	} else {
		tca6408_output_set_value(0<<5, TCA_RED_LED_MASK);
		g_khadas_redled_data->flag = 1;
		schedule_delayed_work(&g_khadas_redled_data->work, msecs_to_jiffies(200));
	}

}

static void set_mode(int mode)
{

	cancel_delayed_work(&g_khadas_redled_data->work);
	switch (mode) {
		case KHADAS_RED_LED_HEARTBEAT:
			g_khadas_redled_data->mode = KHADAS_RED_LED_HEARTBEAT;
			g_khadas_redled_data->flag = 1;
			schedule_delayed_work(&g_khadas_redled_data->work, 0);
			break;
		case KHADAS_RED_LED_ON:
			g_khadas_redled_data->mode = KHADAS_RED_LED_ON;
			tca6408_output_set_value(1<<5, TCA_RED_LED_MASK);
			break;
		case KHADAS_RED_LED_OFF:
			g_khadas_redled_data->mode = KHADAS_RED_LED_OFF;
			tca6408_output_set_value(0<<5, TCA_RED_LED_MASK);
			break;
		default:
			g_khadas_redled_data->mode = KHADAS_RED_LED_OFF;
			tca6408_output_set_value(0<<5, TCA_RED_LED_MASK);
			break;
	}

}

static ssize_t store_mode(struct class *cls, struct class_attribute *attr,
			const char *buf, size_t count)
{
	int mode;
	if (kstrtoint(buf, 0, &mode))
		return -EINVAL;
	set_mode(mode);
	return count;

}

static ssize_t show_mode(struct class *cls,
			struct class_attribute *attr, char *buf)
{
	int mode;
	mode = g_khadas_redled_data->mode;
	return sprintf(buf, "%d\n", mode);

}


static struct class_attribute led_class_attrs[] = {
	__ATTR(mode, 0644, show_mode, store_mode),
};

static void create_led_attrs(void)
{
	int i;
	g_khadas_redled_data->led_class = class_create(THIS_MODULE, "redled");
	if (IS_ERR(g_khadas_redled_data->led_class)) {
		printk("create led_class debug class fail\n");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(led_class_attrs); i++) {
		if (class_create_file(g_khadas_redled_data->led_class, &led_class_attrs[i]))
			printk("create wol attribute %s fail\n", led_class_attrs[i].attr.name);

	}

}

static int khadas_redled_probe(struct platform_device *pdev)
{

	if (!(pdev->dev.of_node)) {
		printk("pdev->dev.of_node == NULL!\n");
		return -EINVAL;
	}
	g_khadas_redled_data = devm_kzalloc(&pdev->dev,
		sizeof(struct khadas_redled_data), GFP_KERNEL);
	if (!g_khadas_redled_data)
		return -EINVAL;

	create_led_attrs();
	INIT_DELAYED_WORK(&g_khadas_redled_data->work, led_work_func);
	set_mode(KHADAS_RED_LED_OFF);
	return 0;
}

static int khadas_redled_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id led_dt_match[] = {
	{	.compatible = "khadas-redled", },
	{},
};

static int khadas_redled_suspend(struct platform_device *dev,
	pm_message_t state)
{
	return 0;
}

static int khadas_redled_resume(struct platform_device *dev)
{
	return 0;
}

static struct platform_driver khadas_redled_driver = {
	.probe = khadas_redled_probe,
	.remove = khadas_redled_remove,
	.suspend = khadas_redled_suspend,
	.resume = khadas_redled_resume,
	.driver = {
		.name = "redled",
		.of_match_table = led_dt_match,
	},
};

module_platform_driver(khadas_redled_driver);
MODULE_AUTHOR("Terry");
MODULE_DESCRIPTION("Led Driver for khadas VIM3");
MODULE_LICENSE("GPL");
