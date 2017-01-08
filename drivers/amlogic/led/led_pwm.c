/*
 * drivers/amlogic/led/led_pwm.c
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

#define pr_fmt(fmt)	"pwmled: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/leds.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/slab.h>

#include <linux/amlogic/jtag.h>
#include <linux/amlogic/led.h>
#include <linux/amlogic/scpi_protocol.h>

#include "led_pwm.h"

#define AML_DEV_NAME		"pwmled"
#define AML_LED_NAME		"led-pwm"

#define DEFAULT_PWM_PERIOD	100000;

static unsigned int ledmode;

static int __init ledmode_setup(char *str)
{
	ledmode = lwm_parse_workmode(str);
	pr_info("ledmode = 0x%x\n", ledmode);
	return 0;
}

__setup("ledmode=", ledmode_setup);


static void aml_pwmled_work(struct work_struct *work)
{
	struct aml_pwmled_dev *ldev;
	int new_duty;

	ldev = container_of(work, struct aml_pwmled_dev, work);
	new_duty = ldev->new_duty;

	/*  @todo pwm setup */
	pwm_config(ldev->pwmd, new_duty, ldev->period);

	if (new_duty == 0)
		pwm_disable(ldev->pwmd);
	else
		pwm_enable(ldev->pwmd);
}


static void aml_pwmled_brightness_set(struct led_classdev *cdev,
	enum led_brightness brightness)
{
	struct platform_device *pdev;
	struct aml_pwmled_dev *ldev;
	unsigned int max;
	unsigned long long duty;

	pdev = to_platform_device(cdev->dev->parent);
	ldev = platform_get_drvdata(pdev);

	max = ldev->cdev.max_brightness;
	/* calculate new duty */
	duty = brightness * ldev->period;
	do_div(duty, max);
	/* save new duty */
	ldev->new_duty = duty;
	/* save new brightness */
	ldev->new_brightness = brightness;

	/* schedule work */
	schedule_work(&ldev->work);
}


static int aml_pwmled_dt_parse(struct platform_device *pdev)
{
	struct device_node *node;
	struct aml_pwmled_dev *ldev;
	struct pinctrl *p;
	int ret;

	ldev = platform_get_drvdata(pdev);
	node = pdev->dev.of_node;
	if (!node) {
		pr_err("failed to find node for %s\n", AML_DEV_NAME);
		return -ENODEV;
	}

	if (is_jtag_apao()) {
		pr_err("conflict with jtag apao\n");
		return -1;
	}

	p = devm_pinctrl_get_select(&pdev->dev, "pwm_ao_a_pins");
	if (IS_ERR(p)) {
		pr_err("failed to get select pins\n");
		return -1;
	}

	ldev->pwmd = devm_of_pwm_get(&pdev->dev, node, NULL);
	if (IS_ERR(ldev->pwmd)) {
		pr_info("unable to request pwm device for %s\n",
			node->full_name);
		ret = PTR_ERR(ldev->pwmd);
		return ret;
	}

	/* Get the period from PWM core when n*/
	ldev->period = pwm_get_period(ldev->pwmd);

	ret = of_property_read_u32(node, "polarity", &ldev->polarity);
	if (ret < 0) {
		pr_err("failed to get polarity\n");
		return -1;
	}

	pr_info("get polarity %u\n", ldev->polarity);

	/* set pwm device polarity */
	pwm_set_polarity(ldev->pwmd, ldev->polarity);


	ret = of_property_read_u32(node, "max_brightness",
			&ldev->cdev.max_brightness);
	if (ret < 0) {
		pr_err("failed to get max_brightness\n");
		return -1;
	}
	pr_info("get max_brightness %d\n", ldev->cdev.max_brightness);


	/* parse led timer data */
	ret = of_property_read_u32(node, "expires", &ldev->ltd.expires);
	if (ret < 0) {
		pr_err("failed to get expires\n");
		return -1;
	}
	pr_info("get expires %d\n", ldev->ltd.expires);
	ldev->ltd.expires *= 1000;

	ret = of_property_read_u32(node, "expires_count",
						&ldev->ltd.expires_count);
	if (ret < 0) {
		pr_err("failed to get expires_count\n");
		return -1;
	}
	pr_info("get expires_count %d\n", ldev->ltd.expires_count);

	ldev->ltd.led_mode = ledmode;

	return 0;
}



static const struct of_device_id aml_pwmled_dt_match[] = {
	{
		.compatible = "amlogic, pwmled",
	},
	{},
};


static int aml_pwmled_probe(struct platform_device *pdev)
{
	struct aml_pwmled_dev *ldev;
	int ret;

	ldev = kzalloc(sizeof(struct aml_pwmled_dev), GFP_KERNEL);
	if (!ldev) {
		pr_err("kzalloc error\n");
		return -ENOMEM;
	}

	/* set driver data */
	platform_set_drvdata(pdev, ldev);

	/* parse dt param */
	ret = aml_pwmled_dt_parse(pdev);
	if (ret)
		return ret;

	/* register led class device */
	ldev->cdev.name = AML_LED_NAME;
	ldev->cdev.brightness_set = aml_pwmled_brightness_set;
	INIT_WORK(&ldev->work, aml_pwmled_work);
	ret = led_classdev_register(&pdev->dev, &ldev->cdev);
	if (ret < 0) {
		kfree(ldev);
		return ret;
	}

	pr_info("module probed ok\n");
	return 0;
}


static int __exit aml_pwmled_remove(struct platform_device *pdev)
{
	struct aml_pwmled_dev *ldev = platform_get_drvdata(pdev);

	led_classdev_unregister(&ldev->cdev);
	cancel_work_sync(&ldev->work);
	platform_set_drvdata(pdev, NULL);
	kfree(ldev);
	return 0;
}


static struct platform_driver aml_pwmled_driver = {
	.driver = {
		.name = AML_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = aml_pwmled_dt_match,
	},
	.probe = aml_pwmled_probe,
	.remove = __exit_p(aml_pwmled_remove),
};


static int __init aml_pwmled_init(void)
{
	if (platform_driver_register(&aml_pwmled_driver)) {
		pr_err("failed to register driver\n");
		return -ENODEV;
	}

	pr_info("module init\n");
	return 0;
}


static void __exit aml_pwmled_exit(void)
{
	platform_driver_unregister(&aml_pwmled_driver);
	pr_info("module exit\n");
}


module_init(aml_pwmled_init);
module_exit(aml_pwmled_exit);

MODULE_DESCRIPTION("Amlogic pwm led driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");

