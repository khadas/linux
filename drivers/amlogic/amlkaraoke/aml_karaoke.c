/*
 * drivers/amlogic/amlkaraoke/aml_karaoke.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/amlogic/major.h>

#include "aml_karaoke.h"

int builtin_mixer = 1;
EXPORT_SYMBOL(builtin_mixer);

static ssize_t show_builtin_mixer(struct class *class,
				  struct class_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "%d\n", builtin_mixer);
}

static ssize_t store_builtin_mixer(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;

	builtin_mixer = val;
	pr_info("builtin_mixer set to %d\n", builtin_mixer);
	return count;
}

int reverb_time;
EXPORT_SYMBOL(reverb_time);

static ssize_t show_reverb_time(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", reverb_time);
}

static ssize_t store_reverb_time(struct class *class,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;
	if (val > 6)
		val = 6;

	reverb_time = val;
	pr_info("reverb_time set to %d\n", reverb_time);
	return count;
}

int usb_mic_digital_gain = 256;
EXPORT_SYMBOL(usb_mic_digital_gain);

static ssize_t show_usb_mic_digital_gain(struct class *class,
					 struct class_attribute *attr,
					 char *buf)
{
	return sprintf(buf, "%d\n", usb_mic_digital_gain);
}

static ssize_t store_usb_mic_digital_gain(struct class *class,
					  struct class_attribute *attr,
					  const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;

	if (val > 256)
		val = 256;

	usb_mic_digital_gain = val;
	pr_info("usb_mic_digital_gain set to %d\n", usb_mic_digital_gain);
	return count;
}

int reverb_enable;
EXPORT_SYMBOL(reverb_enable);
static ssize_t show_reverb_enable(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", reverb_enable);
}

static ssize_t store_reverb_enable(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	reverb_enable = val;
	pr_info("reverb_enable set to %d\n", reverb_enable);
	return count;
}

int reverb_highpass;
EXPORT_SYMBOL(reverb_highpass);

static ssize_t show_reverb_highpass(struct class *class,
				    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", reverb_highpass);
}

static ssize_t store_reverb_highpass(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	reverb_highpass = val;
	pr_info("reverb_highpass set to %d\n", reverb_highpass);
	return count;
}

/* reverb in gain [0%, 100%]*/
int reverb_in_gain = 100;
EXPORT_SYMBOL(reverb_in_gain);
static ssize_t show_reverb_in_gain(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", reverb_in_gain);
}

static ssize_t store_reverb_in_gain(struct class *class,
				    struct class_attribute *attr,
				    const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;
	if (val > 100)
		val = 100;

	reverb_in_gain = val;
	pr_info("reverb_in_gain set to %d\n", reverb_in_gain);
	return count;
}

/* reverb out gain [0%, 100%]*/
int reverb_out_gain = 100;
EXPORT_SYMBOL(reverb_out_gain);
static ssize_t show_reverb_out_gain(struct class *class,
				    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", reverb_out_gain);
}

static ssize_t store_reverb_out_gain(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int val = 0;

	if (buf[0] && kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		val = 0;
	if (val > 100)
		val = 100;

	reverb_out_gain = val;
	pr_info("reverb_out_gain set to %d\n", reverb_out_gain);
	return count;
}

static struct class_attribute amlkaraoke_attrs[] = {
	__ATTR(builtin_mixer, 0664,
	       show_builtin_mixer, store_builtin_mixer),
	__ATTR(reverb_time, 0644,
	       show_reverb_time, store_reverb_time),
	__ATTR(usb_mic_digital_gain, 0644,
	       show_usb_mic_digital_gain, store_usb_mic_digital_gain),
	__ATTR(reverb_enable, 0644,
	       show_reverb_enable, store_reverb_enable),
	__ATTR(reverb_highpass, 0644,
	       show_reverb_highpass, store_reverb_highpass),
	__ATTR(reverb_in_gain, 0644,
	       show_reverb_in_gain, store_reverb_in_gain),
	__ATTR(reverb_out_gain, 0644,
	       show_reverb_out_gain, store_reverb_out_gain),
	__ATTR_NULL,
};

static struct class amlkaraoke_class = {
	.name = AMAUDIO_CLASS_NAME,
	.class_attrs = amlkaraoke_attrs,
};

int irq_karaoke;
static int amlkaraoke_probe(struct platform_device *pdev)
{
	int int_karaoke = platform_get_irq_byname(pdev, "aml_karaoke");

	pr_info("%s irq %d num", __func__, int_karaoke);
	irq_karaoke = int_karaoke;

	return 0;
}

static int amlkaraoke_init(void)
{
	int ret = class_register(&amlkaraoke_class);

	if (ret) {
		pr_err("amlkaraoke class create fail.\n");
		goto err;
	}

	pr_info("amlkaraoke init success!\n");

err:
	return ret;
}

static int amlkaraoke_exit(void)
{
	class_unregister(&amlkaraoke_class);

	pr_info("amlkaraoke_exit!\n");
	return 0;
}

static const struct of_device_id amlogic_match[] = {
	{.compatible = "amlogic, aml_karaoke",},
	{}
};

static struct platform_driver aml_karaoke_driver = {
	.driver = {
		   .name = "aml_karaoke_driver",
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_match,
		   },

	.probe = amlkaraoke_probe,
	.remove = NULL,
};

static int __init aml_karaoke_modinit(void)
{
	amlkaraoke_init();

	return platform_driver_register(&aml_karaoke_driver);
}

static void __exit aml_karaoke_modexit(void)
{
	amlkaraoke_exit();
	platform_driver_unregister(&aml_karaoke_driver);
}

module_init(aml_karaoke_modinit);
module_exit(aml_karaoke_modexit);

MODULE_DESCRIPTION("AMLOGIC Karaoke Interface driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic Inc.");
MODULE_VERSION("1.0.0");
