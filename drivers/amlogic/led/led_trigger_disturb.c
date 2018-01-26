/*
 * drivers/amlogic/led/led_trigger_disturb_led_timer.c
 *
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#undef pr_fmt
#define pr_fmt(fmt)	"ledtrig: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/leds.h>
#include <linux/reboot.h>

#include <linux/amlogic/led.h>
#include <linux/amlogic/scpi_protocol.h>


unsigned int led_no_disturb = 1; /* 0: disturb, 1: no disturb */
unsigned int led_poweron_time;
unsigned int led_suspend_time;
unsigned int led_resume_time;
unsigned int led_count = 1;


static int __init led_disturb_trigger_setup_led_no_disturb(char *p)
{
	if (!strncmp(p, "enable", 6))
		led_no_disturb = 1;
	else
		led_no_disturb = 0;

	pr_info("setup led_no_disturb = %d\n", led_no_disturb);
	return 0;
}
__setup("led_no_disturb=", led_disturb_trigger_setup_led_no_disturb);


static int __init led_disturb_trigger_setup_led_poweron_time(char *p)
{
	get_option(&p, &led_poweron_time);
	pr_info("setup led_poweron_time = %u\n", led_poweron_time);
	return 0;
}
__setup("led_poweron_time=", led_disturb_trigger_setup_led_poweron_time);


static int __init led_disturb_trigger_setup_led_suspend_time(char *p)
{
	get_option(&p, &led_suspend_time);
	pr_info("setup led_suspend_time = %u\n", led_suspend_time);
	return 0;
}
__setup("led_suspend_time=", led_disturb_trigger_setup_led_suspend_time);


static int __init led_disturb_trigger_setup_led_resume_time(char *p)
{
	get_option(&p, &led_resume_time);
	pr_info("setup led_resume_time = %u\n", led_resume_time);
	return 0;
}
__setup("led_resume_time=", led_disturb_trigger_setup_led_resume_time);


static int __init led_disturb_trigger_setup_led_count(char *p)
{
	get_option(&p, &led_count);
	pr_info("setup led_count = %u\n", led_count);
	return 0;
}
__setup("led_count=", led_disturb_trigger_setup_led_count);


static ssize_t led_disturb_trigger_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct led_disturb_data *data = led->trigger_data;

	return sprintf(buf, "%u\n", data->mode);
}

static ssize_t led_disturb_trigger_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct led_disturb_data *data = led->trigger_data;
	unsigned long mode;
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &mode);
	if (ret)
		return ret;
	data->mode = mode;

	dev_dbg(dev, "mode = %u\n", data->mode);

	/* send data */
	scpi_send_usr_data(SCPI_CL_LED_TIMER, (u32 *)data, sizeof(*data));

	return size;
}

static DEVICE_ATTR(mode, 0644,
	led_disturb_trigger_mode_show,
	led_disturb_trigger_mode_store);

static ssize_t led_disturb_trigger_poweron_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct led_disturb_data *data = led->trigger_data;

	return sprintf(buf, "%u\n", data->poweron_time);
}

static ssize_t led_disturb_trigger_poweron_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct led_disturb_data *data = led->trigger_data;
	unsigned long poweron_time;
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &poweron_time);
	if (ret)
		return ret;
	data->poweron_time = poweron_time;

	dev_dbg(dev, "poweron_time = %u\n", data->poweron_time);

	/* send data */
	scpi_send_usr_data(SCPI_CL_LED_TIMER, (u32 *)data, sizeof(*data));

	return size;
}

static DEVICE_ATTR(poweron_time, 0644,
	led_disturb_trigger_poweron_time_show,
	led_disturb_trigger_poweron_time_store);

static ssize_t led_disturb_trigger_suspend_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct led_disturb_data *data = led->trigger_data;

	return sprintf(buf, "%u\n", data->suspend_time);
}

static ssize_t led_disturb_trigger_suspend_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct led_disturb_data *data = led->trigger_data;
	unsigned long suspend_time;
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &suspend_time);
	if (ret)
		return ret;
	data->suspend_time = suspend_time;

	dev_dbg(dev, "suspend_time = %u\n", data->suspend_time);

	/* send data */
	scpi_send_usr_data(SCPI_CL_LED_TIMER, (u32 *)data, sizeof(*data));

	return size;
}

static DEVICE_ATTR(suspend_time, 0644,
	led_disturb_trigger_suspend_time_show,
	led_disturb_trigger_suspend_time_store);


static ssize_t led_disturb_trigger_resume_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct led_disturb_data *data = led->trigger_data;

	return sprintf(buf, "%u\n", data->resume_time);
}

static ssize_t led_disturb_trigger_resume_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct led_disturb_data *data = led->trigger_data;
	unsigned long resume_time;
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &resume_time);
	if (ret)
		return ret;
	data->resume_time = resume_time;

	dev_dbg(dev, "resume_time = %u\n", data->resume_time);

	/* send data */
	scpi_send_usr_data(SCPI_CL_LED_TIMER, (u32 *)data, sizeof(*data));

	return size;
}

static DEVICE_ATTR(resume_time, 0644,
	led_disturb_trigger_resume_time_show,
	led_disturb_trigger_resume_time_store);


static ssize_t led_disturb_trigger_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct led_disturb_data *data = led->trigger_data;

	return sprintf(buf, "%u\n", data->count);
}

static ssize_t led_disturb_trigger_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct led_disturb_data *data = led->trigger_data;
	unsigned long count;
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &count);
	if (ret)
		return ret;
	data->count = count;

	dev_dbg(dev, "count = %u\n", data->count);

	/* send data */
	scpi_send_usr_data(SCPI_CL_LED_TIMER, (u32 *)data, sizeof(*data));

	return size;
}

static DEVICE_ATTR(count, 0644,
	led_disturb_trigger_count_show,
	led_disturb_trigger_count_store);


static void led_trigger_disturb_activate(struct led_classdev *led)
{
	struct led_disturb_data *data;

	data = kzalloc(sizeof(struct led_disturb_data), GFP_KERNEL);
	if (!data) {
		pr_err("failed to allocater for led disturb trigger\n");
		return;
	}

	data->mode = led_no_disturb;
	data->poweron_time = led_poweron_time * 1000;
	data->suspend_time = led_suspend_time * 1000;
	data->resume_time = led_resume_time * 1000;
	data->count = led_count;
	led->trigger_data = data;

	device_create_file(led->dev, &dev_attr_mode);
	device_create_file(led->dev, &dev_attr_poweron_time);
	device_create_file(led->dev, &dev_attr_suspend_time);
	device_create_file(led->dev, &dev_attr_resume_time);
	device_create_file(led->dev, &dev_attr_count);

	led->activated = true;
}

static void led_trigger_disturb_deactivate(struct led_classdev *led)
{
	struct led_disturb_data *data = led->trigger_data;

	if (led->activated) {
		device_remove_file(led->dev, &dev_attr_mode);
		device_remove_file(led->dev, &dev_attr_poweron_time);
		device_remove_file(led->dev, &dev_attr_suspend_time);
		device_remove_file(led->dev, &dev_attr_resume_time);
		device_remove_file(led->dev, &dev_attr_count);
		kfree(data);
		led->activated = false;
	}
}


static struct led_trigger led_trigger_disturb = {
	.name     = "disturb",
	.activate = led_trigger_disturb_activate,
	.deactivate = led_trigger_disturb_deactivate,
};

static int __init led_trigger_disturb_init(void)
{
	return led_trigger_register(&led_trigger_disturb);
}

static void __exit led_trigger_disturb_exit(void)
{
	led_trigger_unregister(&led_trigger_disturb);
}

module_init(led_trigger_disturb_init);
module_exit(led_trigger_disturb_exit);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("led disturb trigger");
MODULE_LICENSE("GPL");
