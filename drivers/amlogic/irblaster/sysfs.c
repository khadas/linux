// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/amlogic/irblaster.h>
#include <linux/amlogic/irblaster_consumer.h>
int irblaster_debug;

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

static ssize_t debug_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int ret, debug;

	ret = kstrtoint(buf, 10, &debug);
	if (ret) {
		pr_err("Invalid input for debug\n");
		return ret;
	}

	irblaster_debug = debug;

	return count;
}

static DEVICE_ATTR_WO(send);
static DEVICE_ATTR_RW(carrier_freq);
static DEVICE_ATTR_RW(duty_cycle);
static DEVICE_ATTR_WO(debug);

static struct attribute *irblaster_chip_attrs[] = {
	&dev_attr_send.attr,
	&dev_attr_carrier_freq.attr,
	&dev_attr_duty_cycle.attr,
	&dev_attr_debug.attr,
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
	parent = device_create(&irblaster_class, NULL, MKDEV(0, 0), chip,
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

int irblaster_sysfs_init(void)
{
	return class_register(&irblaster_class);
}

