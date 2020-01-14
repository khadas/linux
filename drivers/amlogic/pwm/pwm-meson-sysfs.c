// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#undef pr_fmt
#define pr_fmt(fmt) "pwm: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pwm.h>
#include <linux/amlogic/pwm-meson.h>

/**
 * pwm_constant_enable()
 * - start a constant PWM output toggling
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B
 */
int pwm_constant_enable(struct meson_pwm *meson, int index)
{
	u32 enable;

	switch (index) {
	case 0:
		enable = MISC_A_CONSTANT;
		break;
	case 1:
		enable = MISC_B_CONSTANT;
		break;
	default:
		return -EINVAL;
	}
	regmap_update_bits(meson->regmap_base, REG_MISC_AB, enable, enable);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_constant_enable);

/**
 * pwm_constant_disable() - stop a constant PWM output toggling
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B
 */
int pwm_constant_disable(struct meson_pwm *meson, int index)
{
	u32 enable;

	switch (index) {
	case 0:
		enable = BIT(28);
		break;

	case 1:
		enable = BIT(29);
		break;

	default:
		return -EINVAL;
	}
	regmap_update_bits(meson->regmap_base,
			   REG_MISC_AB, enable, PWM_DISABLE);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_constant_disable);

static ssize_t constant_show(struct device *child,
			     struct device_attribute *attr,
			     char *buf)
{
	struct meson_pwm *meson =
		(struct meson_pwm *)dev_get_drvdata(child);
	return sprintf(buf, "%d\n", meson->variant.constant);
}

static ssize_t constant_store(struct device *child,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct meson_pwm *meson =
		(struct meson_pwm *)dev_get_drvdata(child);
	int val, ret, id, res;

	res = sscanf(buf, "%d %d", &val, &id);
	if (res != 2) {
		dev_err(child, "Can't parse pwm id,usage:[value index]\n");
		return -EINVAL;
	}

	switch (val) {
	case 0:
		ret = pwm_constant_disable(meson, id);
		meson->variant.constant = 0;
		break;
	case 1:
		ret = pwm_constant_enable(meson, id);
		meson->variant.constant = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret ? : size;
}

/**
 * pwm_set_times() - set PWM times output toggling
 *		     set pwm a1 and pwm a2 timer together
 *		     and pwm a1 should be set first
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B,range from 1 to 15
 * @value: blink times to set,range from 1 to 255
 */
int pwm_set_times(struct meson_pwm *meson,
		  int index, int value)
{
	unsigned int clear_val, val;

	if (value < 0 || value > 255) {
		dev_err(meson->chip.dev,
			"index or value is not within the scope!\n");
		return -EINVAL;
	}

	switch (index) {
	case 0:
		clear_val = 0xff << 24;
		val = value << 24;
		break;
	case 1:
		clear_val = 0xff << 8;
		val = value << 8;
		break;
	case 2:
		clear_val = 0xff << 16;
		val = value << 16;
		break;
	case 3:
		clear_val = 0xff;
		val = value;
		break;
	default:
		return -EINVAL;
	}
	regmap_update_bits(meson->regmap_base, REG_TIME_AB, clear_val, val);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_set_times);

static ssize_t times_show(struct device *child,
			  struct device_attribute *attr, char *buf)
{
	struct meson_pwm *meson =
		(struct meson_pwm *)dev_get_drvdata(child);
	return sprintf(buf, "%d\n", meson->variant.times);
}

static ssize_t times_store(struct device *child,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	struct meson_pwm *meson =
		(struct meson_pwm *)dev_get_drvdata(child);
	int val, ret, id, res;

	res = sscanf(buf, "%d %d", &val, &id);
	if (res != 2) {
		dev_err(child,
			"Can't parse pwm id and value,usage:[value index]\n");
		return -EINVAL;
	}
	ret = pwm_set_times(meson, id, val);
	meson->variant.times = val;

	return ret ? : size;
}

/**
 * pwm_blink_enable()
 *	- start a blink PWM output toggling
 *	  txl only support 8 channel blink output
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B
 */
int pwm_blink_enable(struct meson_pwm *meson, int index)
{
	u32 enable;

	switch (index) {
	case 0:
		enable = BLINK_A;
		break;
	case 1:
		enable = BLINK_B;
		break;
	default:
		return -EINVAL;
	}
	regmap_update_bits(meson->regmap_base, REG_BLINK_AB, enable, enable);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_blink_enable);

/**
 * pwm_blink_disable() - stop a constant PWM output toggling
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B
 */
int pwm_blink_disable(struct meson_pwm *meson, int index)
{
	u32 enable;

	switch (index) {
	case 0:
		enable = BLINK_A;
		break;

	case 1:
		enable = BLINK_B;
		break;

	default:
		return -EINVAL;
	}
	regmap_update_bits(meson->regmap_base,
			   REG_BLINK_AB, enable, PWM_DISABLE);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_blink_disable);

static ssize_t blink_enable_show(struct device *child,
				 struct device_attribute *attr, char *buf)
{
	struct meson_pwm *meson =
		(struct meson_pwm *)dev_get_drvdata(child);
	return sprintf(buf, "%d\n", meson->variant.blink_enable);
}

static ssize_t blink_enable_store(struct device *child,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct meson_pwm *meson =
		(struct meson_pwm *)dev_get_drvdata(child);
	int val, ret, id, res;

	res = sscanf(buf, "%d %d", &val, &id);
	if (res != 2) {
		dev_err(child, "blink enable,Can't parse pwm id,usage:[value index]\n");
		return -EINVAL;
	}

	switch (val) {
	case 0:
		ret = pwm_blink_disable(meson, id);
		meson->variant.blink_enable = 0;
		break;
	case 1:
		ret = pwm_blink_enable(meson, id);
		meson->variant.blink_enable = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret ? : size;
}

/**
 * pwm_set_blink_times() - set PWM blink times output toggling
 * @chip: aml_pwm_chip struct
 * @index: pwm channel to choose,like PWM_A or PWM_B
 * @value: blink times to set,range from 1 to 15
 */
int pwm_set_blink_times(struct meson_pwm *meson,
			int index,
			int value)
{
	unsigned int clear_val, val;

	if (value < 0 || value > 15) {
		dev_err(meson->chip.dev,
			"value or index is not within the scope!\n");
		return -EINVAL;
	}
	switch (index) {
	case 0:
		clear_val = 0xf;
		val = value;
		break;

	case 1:
		clear_val = 0xf << 4;
		val = value << 4;
		break;

	default:
		return -EINVAL;
	}
	regmap_update_bits(meson->regmap_base, REG_BLINK_AB, clear_val, val);

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_set_blink_times);

static ssize_t blink_times_show(struct device *child,
				struct device_attribute *attr, char *buf)
{
	struct meson_pwm *meson =
		(struct meson_pwm *)dev_get_drvdata(child);
	return sprintf(buf, "%d\n", meson->variant.blink_times);
}

static ssize_t blink_times_store(struct device *child,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct meson_pwm *meson =
		(struct meson_pwm *)dev_get_drvdata(child);
	int val, ret, id, res;

	res = sscanf(buf, "%d %d", &val, &id);
	if (res != 2) {
		dev_err(child,
			"Can't parse pwm id and value,usage:[value index]\n");
		return -EINVAL;
	}
	ret = pwm_set_blink_times(meson, id, val);
	meson->variant.blink_times = val;

	return ret ? : size;
}

int pwm_register_debug(struct meson_pwm *meson)
{
	unsigned int i, value;

	for (i = 0; i < 8; i++) {
		regmap_read(meson->regmap_base, 4 * i, &value);
		pr_info("[base+%x] = %x\n", 4 * i, value);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pwm_register_debug);

static DEVICE_ATTR_RW(constant);
static DEVICE_ATTR_RW(times);
static DEVICE_ATTR_RW(blink_enable);
static DEVICE_ATTR_RW(blink_times);

static struct attribute *pwm_attrs[] = {
		&dev_attr_constant.attr,
		&dev_attr_times.attr,
		&dev_attr_blink_enable.attr,
		&dev_attr_blink_times.attr,
		NULL,
};

static struct attribute_group pwm_attr_group = {
		.attrs = pwm_attrs,
};

int meson_pwm_sysfs_init(struct device *dev)
{
	int retval;

	retval = sysfs_create_group(&dev->kobj, &pwm_attr_group);
	if (retval) {
		dev_err(dev,
			"pwm sysfs group creation failed: %d\n", retval);
		return retval;
	}

	return 0;
}

void meson_pwm_sysfs_exit(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &pwm_attr_group);
}
