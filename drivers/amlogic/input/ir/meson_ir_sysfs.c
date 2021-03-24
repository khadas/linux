// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>

#include "meson_ir_main.h"

static ssize_t protocol_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);

	if (ENABLE_LEGACY_IR(chip->protocol))
		return sprintf(buf, "protocol=%s&%s (0x%x)\n",
			       chip->ir_contr[LEGACY_IR_ID].proto_name,
			       chip->ir_contr[MULTI_IR_ID].proto_name,
			       chip->protocol);

	return sprintf(buf, "protocol=%s (0x%x)\n",
		       chip->ir_contr[MULTI_IR_ID].proto_name,
		       chip->protocol);
}

static ssize_t protocol_store(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t count)
{
	int ret;
	int protocol;
	unsigned long flags;
	struct meson_ir_chip *chip = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &protocol);
	if (ret != 0) {
		dev_err(chip->dev, "input parameter error\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&chip->slock, flags);
	chip->protocol = protocol;
	chip->set_register_config(chip, chip->protocol);
	spin_unlock_irqrestore(&chip->slock, flags);

	if (MULTI_IR_SOFTWARE_DECODE(chip->r_dev->rc_type) &&
	    !MULTI_IR_SOFTWARE_DECODE(chip->protocol)) {
		meson_ir_raw_event_unregister(chip->r_dev); /*raw->no raw*/
		dev_info(chip->dev, "meson_ir_raw_event_unregister\n");
	} else if (!MULTI_IR_SOFTWARE_DECODE(chip->r_dev->rc_type) &&
		  MULTI_IR_SOFTWARE_DECODE(chip->protocol)) {
		meson_ir_raw_event_register(chip->r_dev); /*no raw->raw*/
	}
	chip->r_dev->rc_type = chip->protocol;
	return count;
}

static ssize_t keymap_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_map_tab_list *map_tab;
	unsigned long flags;
	int i, len;

	spin_lock_irqsave(&chip->slock, flags);
	map_tab = meson_ir_seek_map_tab(chip, chip->sys_custom_code);
	if (!map_tab) {
		dev_err(chip->dev, "please set valid keymap name first\n");
		spin_unlock_irqrestore(&chip->slock, flags);
		return 0;
	}
	len = sprintf(buf, "custom_code=0x%x\n", map_tab->tab.custom_code);
	len += sprintf(buf + len, "custom_name=%s\n", map_tab->tab.custom_name);
	len += sprintf(buf + len, "release_delay=%d\n",
			map_tab->tab.release_delay);
	len += sprintf(buf + len, "map_size=%d\n", map_tab->tab.map_size);
	len += sprintf(buf + len, "fn_key_scancode = 0x%x\n",
			map_tab->tab.cursor_code.fn_key_scancode);
	len += sprintf(buf + len, "cursor_left_scancode = 0x%x\n",
			map_tab->tab.cursor_code.cursor_left_scancode);
	len += sprintf(buf + len, "cursor_right_scancode = 0x%x\n",
			map_tab->tab.cursor_code.cursor_right_scancode);
	len += sprintf(buf + len, "cursor_up_scancode = 0x%x\n",
			map_tab->tab.cursor_code.cursor_up_scancode);
	len += sprintf(buf + len, "cursor_down_scancode = 0x%x\n",
			map_tab->tab.cursor_code.cursor_down_scancode);
	len += sprintf(buf + len, "cursor_ok_scancode = 0x%x\n",
			map_tab->tab.cursor_code.cursor_ok_scancode);
	len += sprintf(buf + len, "keycode scancode\n");
	for (i = 0; i <  map_tab->tab.map_size; i++) {
		len += sprintf(buf + len, "%4d %4d\n",
			       map_tab->tab.codemap[i].map.keycode,
			       map_tab->tab.codemap[i].map.scancode);
	}
	spin_unlock_irqrestore(&chip->slock, flags);
	return len;
}

static ssize_t keymap_store(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t count)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	int ret;
	int value;

	ret = kstrtoint(buf, 0, &value);
	if (ret != 0) {
		dev_err(chip->dev, "keymap store input err\n");
		return -EINVAL;
	}
	chip->sys_custom_code = value;
	return count;
}

static ssize_t debug_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;

	return sprintf(buf, "%d\n", r_dev->debug_enable);
}

static ssize_t debug_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;
	int debug_enable;
	int ret;

	ret = kstrtoint(buf, 0, &debug_enable);
	if (ret != 0)
		return -EINVAL;
	r_dev->debug_enable = debug_enable;
	return count;
}

static ssize_t enable_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;

	return sprintf(buf, "%d\n", r_dev->enable);
}

static ssize_t enable_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;
	unsigned int val;
	int enable;
	int ret;
	int id;

	ret = kstrtoint(buf, 0, &enable);
	if (ret != 0 || (enable != 0 && enable != 1))
		return -EINVAL;

	for (id = 0; id < (ENABLE_LEGACY_IR(chip->protocol) ? 2 : 1); id++) {
		if (enable) {
			regmap_read(chip->ir_contr[id].base, REG_FRAME, &val);
			regmap_update_bits(chip->ir_contr[id].base, REG_REG1,
					   BIT(15), BIT(15));
		} else {
			regmap_update_bits(chip->ir_contr[id].base, REG_REG1,
					   BIT(15), 0);
		}
	}
	r_dev->enable = enable;

	return count;
}
static ssize_t map_tables_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_map_tab_list *ir_map;
	unsigned long flags;
	int len = 0;
	int cnt = 0;

	spin_lock_irqsave(&chip->slock, flags);
	list_for_each_entry(ir_map, &chip->map_tab_head, list) {
		len += sprintf(buf + len, "%d. 0x%x,%s\n", cnt,
			      ir_map->tab.custom_code,
			      ir_map->tab.custom_name);
		cnt++;
	}
	spin_unlock_irqrestore(&chip->slock, flags);
	return len;
}

static ssize_t led_blink_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;

	return sprintf(buf, "%u\n", r_dev->led_blink);
}

static ssize_t led_blink_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	int ret = 0;
	int val = 0;

	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;

	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	r_dev->led_blink = val;
	return count;
}

static ssize_t led_frq_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;

	return sprintf(buf, "%ld\n", r_dev->delay_on);
}

static ssize_t led_frq_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;

	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;

	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	r_dev->delay_off = val;
	r_dev->delay_on = val;
	return count;
}

static ssize_t ir_learning_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;

	return sprintf(buf, "%d\n", r_dev->ir_learning_on);
}

static ssize_t ir_learning_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;

	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;

	if (r_dev->ir_learning_on == !!val)
		return count;

	disable_irq(chip->irqno);
	mutex_lock(&chip->file_lock);
	r_dev->ir_learning_on = !!val;
	if (!!val) {
		chip->set_register_config(chip, REMOTE_TYPE_RAW_NEC);
		r_dev->protocol = chip->protocol;
		chip->protocol = REMOTE_TYPE_RAW_NEC;
		if (meson_ir_pulses_malloc(chip) < 0) {
			mutex_unlock(&chip->file_lock);
			enable_irq(chip->irqno);
			return -ENOMEM;
		}
	} else {
		chip->protocol = r_dev->protocol;
		chip->set_register_config(chip, chip->protocol);
		meson_ir_pulses_free(chip);
	}
	mutex_unlock(&chip->file_lock);
	enable_irq(chip->irqno);
	return count;
}

static ssize_t learned_pulse_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int len = 0;
	int i = 0;
	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;

	if (!r_dev->pulses)
		return len;

	disable_irq(chip->irqno);
	mutex_lock(&chip->file_lock);

	for (i = 0; i < r_dev->pulses->len; i++)
		len += sprintf(buf + len, "%lds",
			       r_dev->pulses->pulse[i] & GENMASK(30, 0));

	mutex_unlock(&chip->file_lock);
	enable_irq(chip->irqno);
	len += sprintf(buf + len, "\n");

	return len;
}

static ssize_t learned_pulse_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int len = 0;

	struct meson_ir_chip *chip = dev_get_drvdata(dev);
	struct meson_ir_dev  *r_dev = chip->r_dev;

	if (!r_dev->pulses)
		return len;

	/*clear learned pulse*/
	if (buf[0] == 'c')
		memset(r_dev->pulses, 0, sizeof(struct pulse_group) +
		       r_dev->max_learned_pulse * sizeof(u32));

	return count;
}
DEVICE_ATTR_RW(learned_pulse);
DEVICE_ATTR_RW(ir_learning);
DEVICE_ATTR_RW(led_frq);
DEVICE_ATTR_RW(led_blink);
DEVICE_ATTR_RW(protocol);
DEVICE_ATTR_RW(keymap);
DEVICE_ATTR_RW(debug_enable);
DEVICE_ATTR_RW(enable);
DEVICE_ATTR_RO(map_tables);

static struct attribute *meson_ir_sysfs_attrs[] = {
	&dev_attr_protocol.attr,
	&dev_attr_map_tables.attr,
	&dev_attr_keymap.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_enable.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_frq.attr,
	&dev_attr_ir_learning.attr,
	&dev_attr_learned_pulse.attr,
	NULL,
};

ATTRIBUTE_GROUPS(meson_ir_sysfs);

static struct class meson_ir_class = {
	.name		= "remote",
	.owner		= THIS_MODULE,
	.dev_groups = meson_ir_sysfs_groups,
};

int meson_ir_sysfs_init(struct meson_ir_chip *chip)
{
	struct device *dev;
	int err;

	err = class_register(&meson_ir_class);
	if (unlikely(err))
		return err;

	dev = device_create(&meson_ir_class,  NULL, chip->chr_devno, chip,
			    chip->dev_name);
	if (IS_ERR(dev))
		return PTR_ERR(dev);
	return 0;
}
EXPORT_SYMBOL_GPL(meson_ir_sysfs_init);

void meson_ir_sysfs_exit(struct meson_ir_chip *chip)
{
	device_destroy(&meson_ir_class, chip->chr_devno);
	class_unregister(&meson_ir_class);
}
EXPORT_SYMBOL_GPL(meson_ir_sysfs_exit);
