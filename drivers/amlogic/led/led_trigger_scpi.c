/*
 * drivers/amlogic/led/led_trigger_scpi.c
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
#define pr_fmt(fmt)	"scpitrig: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/leds.h>
#include <linux/reboot.h>

#include <linux/amlogic/scpi_protocol.h>

#include "led_pwm.h"


static ssize_t scpi_expires_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *lcdev = dev_get_drvdata(dev);
	struct led_timer_data *ltd = lcdev->trigger_data;

	return sprintf(buf, "%u\n", ltd->expires);
}

static ssize_t scpi_expires_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *lcdev = dev_get_drvdata(dev);
	struct led_timer_data *ltd = lcdev->trigger_data;

	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	ltd->expires = val;
	return size;
}

static DEVICE_ATTR(scpi_expires, 0644, scpi_expires_show, scpi_expires_store);


static ssize_t scpi_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *lcdev = dev_get_drvdata(dev);
	struct led_timer_data *ltd = lcdev->trigger_data;

	return sprintf(buf, "%u\n", ltd->expires_count);
}

static ssize_t scpi_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *lcdev = dev_get_drvdata(dev);
	struct led_timer_data *ltd = lcdev->trigger_data;

	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	ltd->expires_count = val;
	return size;
}

static DEVICE_ATTR(scpi_count, 0644, scpi_count_show, scpi_count_store);


static ssize_t scpi_ledmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *lcdev = dev_get_drvdata(dev);
	struct led_timer_data *ltd = lcdev->trigger_data;

	return sprintf(buf, "0x%x\n", ltd->led_mode);
}

static ssize_t scpi_ledmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *lcdev = dev_get_drvdata(dev);
	struct led_timer_data *ltd = lcdev->trigger_data;

	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	ltd->led_mode = val;
	return size;
}

static DEVICE_ATTR(scpi_ledmode, 0644, scpi_ledmode_show, scpi_ledmode_store);

static ssize_t scpi_send_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int len = 0;

	len += sprintf(buf + len, "usage:\n");
	len += sprintf(buf + len, "    echo <n> > scpi_send\n\n");
	len += sprintf(buf + len, "<n>:\n");
	len += sprintf(buf + len, "    %u -- SCPI_CL_LED_TIMER\n",
					SCPI_CL_LED_TIMER);
	return len;
}

static ssize_t scpi_send_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *lcdev = dev_get_drvdata(dev);
	struct led_timer_data *ltd = lcdev->trigger_data;
	struct aml_pwmled_dev *ldev;

	unsigned long val;
	int ret;

	ldev = container_of(lcdev, struct aml_pwmled_dev, cdev);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	switch (val) {
	case SCPI_CL_LED_TIMER:
		/* sync data */
		ldev->ltd.expires = ltd->expires;
		ldev->ltd.expires_count = ltd->expires_count;
		ldev->ltd.led_mode = ltd->led_mode;
		scpi_send_usr_data(SCPI_CL_LED_TIMER,
				(u32 *)ltd, sizeof(*ltd));
		break;
	default:
		pr_info("unsupported value %lu\n", val);
		break;
	}
	return size;
}

static DEVICE_ATTR(scpi_send, 0644, scpi_send_show, scpi_send_store);

static void trigger_scpi_activate(struct led_classdev *lcdev)
{
	struct aml_pwmled_dev *ldev;
	struct led_timer_data *ltd;

	ltd = kzalloc(sizeof(struct led_timer_data), GFP_KERNEL);
	if (!ltd) {
		pr_err("unable to allocate for scpi trigger\n");
		return;
	}
	lcdev->trigger_data = ltd;

	ldev = container_of(lcdev, struct aml_pwmled_dev, cdev);
	ltd->expires = ldev->ltd.expires;
	ltd->expires_count = ldev->ltd.expires_count;
	ltd->led_mode = ldev->ltd.led_mode;

	device_create_file(lcdev->dev, &dev_attr_scpi_expires);
	device_create_file(lcdev->dev, &dev_attr_scpi_count);
	device_create_file(lcdev->dev, &dev_attr_scpi_ledmode);
	device_create_file(lcdev->dev, &dev_attr_scpi_send);

	lcdev->activated = true;
}

static void trigger_scpi_deactivate(struct led_classdev *lcdev)
{
	struct led_timer_data *ltd = lcdev->trigger_data;

	if (lcdev->activated) {
		device_remove_file(lcdev->dev, &dev_attr_scpi_expires);
		device_remove_file(lcdev->dev, &dev_attr_scpi_count);
		device_remove_file(lcdev->dev, &dev_attr_scpi_ledmode);
		device_remove_file(lcdev->dev, &dev_attr_scpi_send);
		kfree(ltd);
		lcdev->activated = false;
	}
}


static struct led_trigger scpi_led_trigger = {
	.name     = "scpi",
	.activate = trigger_scpi_activate,
	.deactivate = trigger_scpi_deactivate,
};

static int __init trigger_scpi_init(void)
{
	return led_trigger_register(&scpi_led_trigger);
}

static void __exit trigger_scpi_exit(void)
{
	led_trigger_unregister(&scpi_led_trigger);
}

module_init(trigger_scpi_init);
module_exit(trigger_scpi_exit);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("scpi trigger");
MODULE_LICENSE("GPL");
