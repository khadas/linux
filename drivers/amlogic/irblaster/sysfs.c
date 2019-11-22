/*
 * drivers/amlogic/irblaster/sysfs.c
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

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/amlogic/irblaster.h>
#include <linux/amlogic/irblaster_consumer.h>
#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
#include <linux/amlogic/irblaster_encoder.h>
#endif
static ssize_t send_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct irblaster_chip *chip = dev_get_drvdata(dev);
	int i = 0, j = 0, m = 0;
	int val, ret;
	char tone[PS_SIZE];
	unsigned int *buffer;

	buffer = kzalloc(sizeof(uint32_t) * MAX_PLUSE, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	mutex_lock(&chip->sys_lock);
	while (buf[i] != '\0') {
		if (buf[i] == 's') {
			tone[j] = '\0';
			ret = kstrtoint(tone, 10, &val);
			if (ret) {
				pr_err("Invalid tone\n");
				mutex_unlock(&chip->sys_lock);
				kfree(buffer);
				return ret;
			}
			buffer[m] = val * 10;
			j = 0;
			i++;
			m++;
			if (m >= MAX_PLUSE)
				break;
			continue;
		}
		tone[j] = buf[i];
		i++;
		j++;
		if (j >= PS_SIZE) {
			pr_err("send timing value is out of range\n");
			mutex_unlock(&chip->sys_lock);
			kfree(buffer);
			return -ENOMEM;
		}
	}

	ret = irblaster_send(chip, buffer, m);
	if (ret)
		pr_err("send raw data fail\n");

	irblaster_chip_data_clear(chip);
	mutex_unlock(&chip->sys_lock);
	kfree(buffer);

	return ret ? : count;
}

#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
static ssize_t send_key_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct irblaster_chip *chip = dev_get_drvdata(dev);
	unsigned int addr, command;
	int ret;

	ret = sscanf(buf, "%x %x", &addr, &command);
	if (ret != 2) {
		pr_err("Can't parse addr and command,usage:[addr command]\n");
		return -EINVAL;
	}
	mutex_lock(&chip->sys_lock);
	ret = irblaster_send_key(chip, addr, command);
	if (ret)
		pr_err("send key fail\n");

	mutex_unlock(&chip->sys_lock);

	return ret ? : count;
}
#endif

static ssize_t carrier_freq_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct irblaster_chip *chip = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", chip->state.freq);
}

static ssize_t carrier_freq_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct irblaster_chip *chip = dev_get_drvdata(dev);
	int ret = 0, val;

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input for carrier_freq\n");
		return ret;
	}

	mutex_lock(&chip->sys_lock);
	ret = irblaster_set_freq(chip, val);
	if (ret)
		pr_err("set freq fail\n");

	mutex_unlock(&chip->sys_lock);

	return ret ? : count;
}

static ssize_t duty_cycle_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct irblaster_chip *chip = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", chip->state.duty);
}

static ssize_t duty_cycle_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct irblaster_chip *chip = dev_get_drvdata(dev);
	int ret = 0, val;

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("Invalid input for duty_cycle\n");
		return ret;
	}

	mutex_lock(&chip->sys_lock);
	ret = irblaster_set_duty(chip, val);
	if (ret)
		pr_err("set duty fail\n");

	mutex_unlock(&chip->sys_lock);

	return ret ? : count;
}

#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
static ssize_t protocol_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct irblaster_chip *chip = dev_get_drvdata(dev);
	unsigned int len;

	len = protocol_show_select(chip, buf);

	return len;
}

static ssize_t protocol_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct irblaster_chip *chip = dev_get_drvdata(dev);
	unsigned int ret, protocol;

	protocol = protocol_store_select(buf);
	if (protocol >= IRBLASTER_PROTOCOL_MAX)
		pr_err("protocol is not found\n");

	ret = irblaster_set_protocol(chip, protocol);
	if (ret)
		pr_err("set protocol fail\n");

	return ret ? : count;
}
#endif

//static DEVICE_ATTR(debug, 0644, show_debug, store_debug);
static DEVICE_ATTR_WO(send);
#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
static DEVICE_ATTR_WO(send_key);
#endif
static DEVICE_ATTR_RW(carrier_freq);
static DEVICE_ATTR_RW(duty_cycle);
#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
static DEVICE_ATTR_RW(protocol);
#endif

static struct attribute *irblaster_chip_attrs[] = {
//	&dev_attr_debug.attr,
	&dev_attr_send.attr,
#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
	&dev_attr_send_key.attr,
#endif
	&dev_attr_carrier_freq.attr,
	&dev_attr_duty_cycle.attr,
#ifdef CONFIG_AMLOGIC_IRBLASTER_PROTOCOL
	&dev_attr_protocol.attr,
#endif
	NULL,
};
ATTRIBUTE_GROUPS(irblaster_chip);

static struct class irblaster_class = {
	.name = "irblaster",
	.owner = THIS_MODULE,
	.dev_groups = irblaster_chip_groups,
};

static int irblasterchip_sysfs_match(struct device *parent, const void *data)
{
	return dev_get_drvdata(parent) == data;
}

void irblasterchip_sysfs_export(struct irblaster_chip *chip)
{
	struct device *parent;

	/*
	 * If device_create() fails the irblaster_chip is still usable by
	 * the kernel its just not exported.
	 */
	parent = device_create(&irblaster_class, chip->dev, MKDEV(0, 0), chip,
			       "irblaster%d", chip->base);
	if (IS_ERR(parent)) {
		dev_warn(chip->dev,
			 "device_create failed for irblaster_chip sysfs export\n");
	}

	mutex_init(&chip->sys_lock);
}

void irblasterchip_sysfs_unexport(struct irblaster_chip *chip)
{
	struct device *parent;

	parent = class_find_device(&irblaster_class, NULL, chip,
				   irblasterchip_sysfs_match);
	if (parent) {
		/* for class_find_device() */
		put_device(parent);
		device_unregister(parent);
	}
}

static int __init irblaster_sysfs_init(void)
{
	return class_register(&irblaster_class);
}
subsys_initcall(irblaster_sysfs_init);
