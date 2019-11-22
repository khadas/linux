/*
 * drivers/amlogic/input/remote/sysfs.c
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
#include <linux/amlogic/cpu_version.h>
#include <linux/of_address.h>

#include "remote_meson.h"
#include <linux/amlogic/iomap.h>

static ssize_t protocol_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	const struct aml_remote_reg_proto **supported_proto =
							ir_get_proto_reg();
	int len;

	if (ENABLE_LEGACY_IR(chip->protocol))
		len =  sprintf(buf, "current protocol = %s&%s (0x%x)\n",
			chip->ir_contr[LEGACY_IR_ID].proto_name,
			chip->ir_contr[MULTI_IR_ID].proto_name,
			chip->protocol);
	else
		len =  sprintf(buf, "currnet protocol = %s (0x%x)\n",
			       chip->ir_contr[MULTI_IR_ID].proto_name,
			       chip->protocol);

	len += sprintf(buf + len, "supported protocol:\n");

	for ( ; (*supported_proto) != NULL ; ) {
		len += sprintf(buf + len, "%s (0x%x)\n",
		((*supported_proto)->name), ((*supported_proto)->protocol));
		supported_proto++;
	}

	return len;
}

static ssize_t protocol_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	int protocol;
	unsigned long flags;
	struct remote_chip *chip = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &protocol);
	if (ret != 0) {
		dev_err(chip->dev, "input parameter error\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&chip->slock, flags);
	chip->protocol = protocol;
	chip->set_register_config(chip, chip->protocol);
	if (MULTI_IR_SOFTWARE_DECODE(chip->r_dev->rc_type) &&
				!MULTI_IR_SOFTWARE_DECODE(chip->protocol)) {
		remote_raw_event_unregister(chip->r_dev); /*raw->no raw*/
		dev_info(chip->dev, "remote_raw_event_unregister\n");
	} else if (!MULTI_IR_SOFTWARE_DECODE(chip->r_dev->rc_type) &&
				MULTI_IR_SOFTWARE_DECODE(chip->protocol)) {
		remote_raw_init();
		remote_raw_event_register(chip->r_dev); /*no raw->raw*/
	}
	chip->r_dev->rc_type = chip->protocol;
	spin_unlock_irqrestore(&chip->slock, flags);
	return count;
}

static ssize_t keymap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct ir_map_tab_list *map_tab;
	unsigned long flags;
	int i, len;

	spin_lock_irqsave(&chip->slock, flags);
	map_tab = seek_map_tab(chip, chip->sys_custom_code);
	if (!map_tab) {
		dev_err(chip->dev, "please set valid keymap name first\n");
		spin_unlock_irqrestore(&chip->slock, flags);
		return 0;
	}
	len = sprintf(buf, "custom_code=0x%x\n", map_tab->tab.custom_code);
	len += sprintf(buf+len, "custom_name=%s\n",  map_tab->tab.custom_name);
	len += sprintf(buf+len, "release_delay=%d\n",
						map_tab->tab.release_delay);
	len += sprintf(buf+len, "map_size=%d\n",  map_tab->tab.map_size);
	len += sprintf(buf+len, "fn_key_scancode = 0x%x\n",
				map_tab->tab.cursor_code.fn_key_scancode);
	len += sprintf(buf+len, "cursor_left_scancode = 0x%x\n",
				map_tab->tab.cursor_code.cursor_left_scancode);
	len += sprintf(buf+len, "cursor_right_scancode = 0x%x\n",
				map_tab->tab.cursor_code.cursor_right_scancode);
	len += sprintf(buf+len, "cursor_up_scancode = 0x%x\n",
				map_tab->tab.cursor_code.cursor_up_scancode);
	len += sprintf(buf+len, "cursor_down_scancode = 0x%x\n",
				map_tab->tab.cursor_code.cursor_down_scancode);
	len += sprintf(buf+len, "cursor_ok_scancode = 0x%x\n",
				map_tab->tab.cursor_code.cursor_ok_scancode);
	len += sprintf(buf+len, "keycode scancode\n");
	for (i = 0; i <  map_tab->tab.map_size; i++) {
		len += sprintf(buf+len, "%4d %4d\n",
			map_tab->tab.codemap[i].map.keycode,
			map_tab->tab.codemap[i].map.scancode);
	}
	spin_unlock_irqrestore(&chip->slock, flags);
	return len;
}

static ssize_t keymap_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	int ret;
	int value;

	ret = kstrtoint(buf, 0, &value);
	if (ret != 0) {
		dev_err(chip->dev, "keymap_store input err\n");
		return -EINVAL;
	}
	chip->sys_custom_code = value;
	return count;
}

static ssize_t debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int debug_enable;

	debug_enable = remote_debug_get_enable();

	return sprintf(buf, "%d\n", debug_enable);
}

static ssize_t debug_enable_store(struct device *dev,
				   struct device_attribute *attr,
					const char *buf, size_t count)
{
	int debug_enable;
	int ret;

	ret = kstrtoint(buf, 0, &debug_enable);
	if (ret != 0)
		return -EINVAL;
	remote_debug_set_enable(debug_enable);
	return count;
}

int debug_log_printk(struct remote_dev *dev, const char *fmt)
{
	char *p;
	int len;

	len = strlen(fmt) + 1;
	if (len > dev->debug_buffer_size)
		return 0;
	if (dev->debug_current + len > dev->debug_buffer_size)
		dev->debug_current = 0;
	p = (char *)(dev->debug_buffer+dev->debug_current);
	strcpy(p, fmt);
	dev->debug_current += (len-1);
	return 0;
}

static ssize_t debug_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	if (!r_dev->debug_buffer)
		return 0;
	return sprintf(buf, r_dev->debug_buffer);
}

static ssize_t debug_log_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	if (!r_dev->debug_buffer)
		return 0;
	if (buf[0] == 'c') {
		r_dev->debug_buffer[0] = 0;
		r_dev->debug_current = 0;
	}
	return count;
}

static ssize_t repeat_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", chip->repeat_enable);
}

static ssize_t repeat_enable_store(struct device *dev,
				   struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	int val;
	struct remote_chip *chip = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	chip->repeat_enable = val;
	return count;
}

static ssize_t map_tables_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct ir_map_tab_list *ir_map;
	unsigned long flags;
	int len = 0;
	int cnt = 0;

	spin_lock_irqsave(&chip->slock, flags);
	list_for_each_entry(ir_map, &chip->map_tab_head, list) {
		len += sprintf(buf+len, "%d. 0x%x,%s\n",
			cnt, ir_map->tab.custom_code, ir_map->tab.custom_name);
		cnt++;
	}
	spin_unlock_irqrestore(&chip->slock, flags);
	return len;
}

static ssize_t led_blink_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	return sprintf(buf, "%u\n", r_dev->led_blink);
}

static ssize_t led_blink_store(struct device *dev,
				   struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;

	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	r_dev->led_blink = val;
	return count;
}

static ssize_t led_frq_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	return sprintf(buf, "%ld\n", r_dev->delay_on);
}

static ssize_t led_frq_store(struct device *dev,
				   struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;

	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

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

	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	return sprintf(buf, "%d\n", r_dev->ir_learning_on);
}

static ssize_t ir_learning_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;

	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	if (r_dev->ir_learning_on == val)
		return count;

	disable_irq(chip->irqno);
	mutex_lock(&chip->file_lock);
	r_dev->ir_learning_on = !!val;
	if (!!val) {
		if (r_dev->demod_enable)
			demod_reset(chip);

		if (remote_pulses_malloc(chip) < 0) {
			mutex_unlock(&chip->file_lock);
			enable_irq(chip->irqno);
			return -ENOMEM;
		}
		chip->set_register_config(chip, REMOTE_TYPE_RAW_NEC);
		r_dev->protocol = chip->protocol;/*backup protocol*/
		chip->protocol = REMOTE_TYPE_RAW_NEC;
		irq_set_affinity(chip->irqno,
				 cpumask_of(chip->irq_cpumask));
	} else {
		chip->protocol = r_dev->protocol;
		chip->set_register_config(chip, chip->protocol);
		remote_pulses_free(chip);
		chip->r_dev->ir_learning_done = false;
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

	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	if (!r_dev->pulses)
		return len;

	disable_irq(chip->irqno);
	mutex_lock(&chip->file_lock);
	for (i = 0; i < r_dev->pulses->len; i++)
		len += sprintf(buf + len, "%lds",
			       r_dev->pulses->pulse[i] & GENMASK(30, 0));

	len += sprintf(buf + len, "\n");

	remote_reg_update_bits(chip, MULTI_IR_ID, REG_REG1, BIT(15), BIT(15));

	r_dev->ir_learning_done = false;

	mutex_unlock(&chip->file_lock);
	enable_irq(chip->irqno);

	return len;
}

static ssize_t learned_pulse_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct remote_chip *chip = dev_get_drvdata(dev);
	struct remote_dev  *r_dev = chip->r_dev;

	if (!r_dev->pulses)
		return count;

	disable_irq(chip->irqno);
	mutex_lock(&chip->file_lock);
	if (buf[0] == 'c') {
		memset(r_dev->pulses, 0, sizeof(struct pulse_group) +
		       r_dev->max_learned_pulse * sizeof(u32));
		remote_reg_update_bits(chip, MULTI_IR_ID, REG_REG1, BIT(15),
				       BIT(15));

		r_dev->ir_learning_done = false;
	}
	mutex_unlock(&chip->file_lock);
	enable_irq(chip->irqno);
	return count;
}

static ssize_t sum_cnt0_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int val = 0;
	struct remote_chip *chip = dev_get_drvdata(dev);

	remote_reg_read(chip, MULTI_IR_ID, REG_DEMOD_SUM_CNT0, &val);

	return sprintf(buf, "%d\n", val);
}

static ssize_t sum_cnt1_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{

	int val = 0;
	struct remote_chip *chip = dev_get_drvdata(dev);

	remote_reg_read(chip, MULTI_IR_ID, REG_DEMOD_SUM_CNT1, &val);

	return sprintf(buf, "%d\n", val);

}

static ssize_t use_fifo_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct remote_chip *chip = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", chip->r_dev->use_fifo);
}

static ssize_t use_fifo_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int val = 0;
	int len = 0;
	struct remote_chip *chip = dev_get_drvdata(dev);

	len = kstrtoint(buf, 0, &val);

	if (len != 0) {
		dev_err(chip->dev, "input parameter error\n");
		return -EINVAL;
	}

	chip->r_dev->use_fifo = val;

	return count;
}

DEVICE_ATTR_RW(use_fifo);
DEVICE_ATTR_RO(sum_cnt0);
DEVICE_ATTR_RO(sum_cnt1);
DEVICE_ATTR_RW(learned_pulse);
DEVICE_ATTR_RW(ir_learning);
DEVICE_ATTR_RW(led_frq);
DEVICE_ATTR_RW(led_blink);
DEVICE_ATTR_RW(repeat_enable);
DEVICE_ATTR_RW(protocol);
DEVICE_ATTR_RW(keymap);
DEVICE_ATTR_RW(debug_enable);
DEVICE_ATTR_RW(debug_log);
DEVICE_ATTR_RO(map_tables);

static struct attribute *remote_attrs[] = {
	&dev_attr_protocol.attr,
	&dev_attr_map_tables.attr,
	&dev_attr_keymap.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_repeat_enable.attr,
	&dev_attr_debug_log.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_frq.attr,
	&dev_attr_ir_learning.attr,
	&dev_attr_learned_pulse.attr,
	&dev_attr_sum_cnt0.attr,
	&dev_attr_sum_cnt1.attr,
	&dev_attr_use_fifo.attr,
	NULL,
};

ATTRIBUTE_GROUPS(remote);

static struct class remote_class = {
	.name		= "remote",
	.owner		= THIS_MODULE,
	.dev_groups = remote_groups,
};

int ir_sys_device_attribute_init(struct remote_chip *chip)
{
	struct device *dev;
	int err;

	err = class_register(&remote_class);
	if (unlikely(err))
		return err;

	dev = device_create(&remote_class,  NULL,
					chip->chr_devno, chip, chip->dev_name);
	if (IS_ERR_OR_NULL(dev))
		return -1;
	return 0;
}
EXPORT_SYMBOL_GPL(ir_sys_device_attribute_init);

void ir_sys_device_attribute_sys(struct remote_chip *chip)
{
	device_destroy(&remote_class, chip->chr_devno);
}
EXPORT_SYMBOL_GPL(ir_sys_device_attribute_sys);
