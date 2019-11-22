/*
 * drivers/amlogic/ircut/ircut.c
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
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

#define OWNER_NAME "ircut"
#undef pr_fmt
#define pr_fmt(fmt) "ircut: " fmt

static unsigned int mode_s;

struct ircut_plat_s {
	struct gpio_desc *light_in;
	struct gpio_desc *filter1;
	struct gpio_desc *filter2;
	struct gpio_desc *alarm;
	struct class ircut_class;
	struct timer_list timer;
};

enum {
	IRCUT_STANDBY = 0,/* filter1:L filter2:L */
	IRCUT_FOREWARD, /* filter1:H filter2:L */
	IRCUT_REVERSAL, /* filter1:L filter2:H */
	IRCUT_LOCK,     /* filter1:H filter2:H */
	IRCUT_AUTO,     /* auto detect by light sensor */
};

static const struct of_device_id ircut_match[] = {
	{
		.compatible = "amlogic, ircut",
	},
	{},
};

static int ircut_control(unsigned int value, struct ircut_plat_s *plat)
{
	if (mode_s != value)
		mode_s = value;
	else
		return 0;

	pr_debug("%s(%d)\n", __func__, value);
	switch (value) {
	case IRCUT_STANDBY:
		gpiod_set_value(plat->filter1, 0);
		gpiod_set_value(plat->filter2, 0);
		break;
	case IRCUT_FOREWARD:
		gpiod_set_value(plat->filter1, 1);
		gpiod_set_value(plat->filter2, 0);
		break;
	case IRCUT_REVERSAL:
		gpiod_set_value(plat->filter1, 0);
		gpiod_set_value(plat->filter2, 1);
		break;
	case IRCUT_LOCK:
		gpiod_set_value(plat->filter1, 1);
		gpiod_set_value(plat->filter2, 1);
		break;
	default:
		gpiod_set_value(plat->filter1, 0);
		gpiod_set_value(plat->filter2, 1);
		break;
	}
	return 0;
}

void ircut_in_detect(struct ircut_plat_s *plat)
{
	int value;

	gpiod_direction_input(plat->light_in);
	value = gpiod_get_value(plat->light_in);
	pr_debug("value=%d\n", value);
	if (value == 0)
		ircut_control(IRCUT_REVERSAL, plat);
	else
		ircut_control(IRCUT_FOREWARD, plat);
}

static void ircut_timer_handle(unsigned long data)
{
	struct ircut_plat_s *plat = (struct ircut_plat_s *)data;

	ircut_in_detect(plat);
	mod_timer(&plat->timer, jiffies + msecs_to_jiffies(2000));
}

/*ircut class*/
static ssize_t set_ircut(struct class *cls,
				  struct class_attribute *attr, const char *buf,
				  size_t count)
{
	unsigned int mode = 0;
	ssize_t ret = 0;

	struct ircut_plat_s *ircutdev = container_of(cls,
					struct ircut_plat_s, ircut_class);

	if (!strcmp(attr->attr.name, "mode")) {
		ret = kstrtoint(buf, 0, &mode);
		if (ret)
			return -EINVAL;
	}
	if (mode < IRCUT_AUTO) {
		del_timer_sync(&ircutdev->timer);
		ircut_control(mode, ircutdev);
	} else
		mod_timer(&ircutdev->timer, jiffies + msecs_to_jiffies(2000));

	return count;
}

static ssize_t get_ircut(struct class *cls,
				  struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", mode_s);
}

static ssize_t set_alarm(struct class *cls,
				  struct class_attribute *attr, const char *buf,
				  size_t count)
{
	unsigned int alarm = 0;
	ssize_t ret = 0;

	struct ircut_plat_s *ircutdev = container_of(cls,
					struct ircut_plat_s, ircut_class);

	if (!strcmp(attr->attr.name, "alarm")) {
		ret = kstrtoint(buf, 0, &alarm);
		if (ret)
			return -EINVAL;
	}
	if (IS_ERR_OR_NULL(ircutdev->alarm)) {
		pr_err("alarm gpio not config.\n");
		return -EINVAL;
	}
	if (alarm == 1)
		gpiod_set_value(ircutdev->alarm, 1);
	else
		gpiod_set_value(ircutdev->alarm, 0);

	return count;
}

static struct class_attribute ircut_class_attrs[] = {
	__ATTR(mode, 0644, get_ircut, set_ircut),
	__ATTR(alarm, 0644, NULL, set_alarm),
	__ATTR_NULL
};

static int ircut_setup_dt(struct platform_device *pdev)
{
	struct ircut_plat_s *ircutdev = platform_get_drvdata(pdev);

	ircut_control(IRCUT_STANDBY, ircutdev);
	return 0;
}

static int ircut_dev_probe(struct platform_device *pdev)
{
	int ret;
	struct ircut_plat_s *plat = NULL;

	plat = devm_kzalloc(&pdev->dev,
			sizeof(struct ircut_plat_s), GFP_KERNEL);
	if (!plat)
		return -ENOMEM;

	platform_set_drvdata(pdev, plat);

	if (pdev->dev.of_node) {
		/* get all gpio desc. */
		plat->filter1 = devm_gpiod_get(&pdev->dev,
			"filter1", GPIOD_OUT_LOW);
		if (IS_ERR_OR_NULL(plat->filter1))
			return PTR_ERR(plat->filter1);
		plat->filter2 = devm_gpiod_get(&pdev->dev, "filter2",
			GPIOD_OUT_HIGH);
		if (IS_ERR_OR_NULL(plat->filter2))
			return PTR_ERR(plat->filter2);
		plat->light_in = devm_gpiod_get(&pdev->dev, "light_in",
			GPIOD_IN);
		if (IS_ERR_OR_NULL(plat->light_in))
			return PTR_ERR(plat->light_in);
		plat->alarm = devm_gpiod_get(&pdev->dev, "alarm",
			GPIOD_OUT_LOW);
		if (IS_ERR_OR_NULL(plat->alarm))
			pr_err("ignore alarm gpio not config.\n");
	}

	/*init class*/
	plat->ircut_class.name = OWNER_NAME;
	plat->ircut_class.owner = THIS_MODULE;
	plat->ircut_class.class_attrs = ircut_class_attrs;

	ret = class_register(&plat->ircut_class);
	if (ret < 0) {
		return ret;
		pr_err("failed to register ircut class.\n");
	}

	ircut_setup_dt(pdev);

	/* timer init */
	setup_timer(&plat->timer, ircut_timer_handle, (unsigned long)plat);
	/* start timer */
	/* HZ / 1000 * timer_expires_ms); */
	mod_timer(&plat->timer, jiffies + msecs_to_jiffies(2000));
	return 0;
}

static int ircut_dev_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct ircut_plat_s *ircutdev = platform_get_drvdata(pdev);

	del_timer_sync(&ircutdev->timer);
	pr_info("ircut_dev_suspend ok.\n");
	return 0;
}

static int ircut_dev_resume(struct platform_device *pdev)
{
	struct ircut_plat_s *ircutdev = platform_get_drvdata(pdev);

	mod_timer(&ircutdev->timer, jiffies + msecs_to_jiffies(2000));
	pr_info("ircut_dev_resume ok.\n");
	return 0;
}

static int ircut_dev_remove(struct platform_device *pdev)
{
	struct ircut_plat_s *ircutdev = platform_get_drvdata(pdev);

	class_unregister(&ircutdev->ircut_class);
	del_timer_sync(&ircutdev->timer);
	return 0;
}

static struct platform_driver ircut_plat_driver = {
	.probe = ircut_dev_probe,
	.remove = ircut_dev_remove,
	.suspend    = ircut_dev_suspend,
	.resume     = ircut_dev_resume,
	.driver = {
		.name = "meson_ircut",
		.owner = THIS_MODULE,
		.of_match_table = ircut_match
	},
};

module_platform_driver(ircut_plat_driver);

MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("IPC ircut driver");
MODULE_AUTHOR("Dianzhong Huo <dianzhong.huo@amlogic.com>");
MODULE_LICENSE("GPL");
