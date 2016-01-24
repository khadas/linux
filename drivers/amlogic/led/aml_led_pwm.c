/*
 * drivers/amlogic/led/aml_led_pwm.c
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

#include <linux/amlogic/led.h>
#include <linux/amlogic/scpi_protocol.h>

#include "aml_led_pwm.h"

#define AML_DEV_NAME		"pwmled"
#define AML_LED_NAME		"led-pwm"

#define DEFAULT_PWM_PERIOD	100000;

struct led_timer_data ledtd;

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
	duty = brightness * 1024; /* 1024: max pwm duty cycle */
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

	/* @todo pinctrl setup */
	p = devm_pinctrl_get_select(&pdev->dev, "pwm_ao_a_pins");
	if (IS_ERR(p)) {
		pr_err("failed to get select pins\n");
		return -1;
	}

	ret = of_property_read_u32(node, "pwm", &ldev->pwm);
	if (ret < 0) {
		pr_err("failed to get pwm index\n");
		return -1;
	}
	pr_info("get pwm index %u\n", ldev->pwm);

	ret = of_property_read_u32(node, "polarity", &ldev->polarity);
	if (ret < 0) {
		pr_err("failed to get polarity\n");
		return -1;
	}
	pr_info("get polarity %u\n", ldev->polarity);

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

	ret = of_property_read_u32(node, "expires_count",
						&ldev->ltd.expires_count);
	if (ret < 0) {
		pr_err("failed to get expires_count\n");
		return -1;
	}
	pr_info("get expires_count %d\n", ldev->ltd.expires_count);

	ret = of_property_read_u32(node, "led_mode", &ldev->ltd.led_mode);
	if (ret < 0) {
		pr_err("failed to get led_mode\n");
		return -1;
	}
	pr_info("get led_mode %d\n", ldev->ltd.led_mode);

	ledtd.expires = ldev->ltd.expires * 1000;
	ledtd.expires_count = ldev->ltd.expires_count;
	ledtd.led_mode = ldev->ltd.led_mode;

	return 0;
}



static const struct of_device_id aml_pwmled_dt_match[] = {
	{
		.compatible = "amlogic, pwmled",
	},
	{},
};

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>

static int led_early_suspend_flag;

static void pwmled_early_suspend(struct early_suspend *h)
{

	struct led_timer_data ltd;

	pr_info("%s\n", __func__);
	if (led_early_suspend_flag)
		return;

	ltd.expires = ledtd.expires;
	ltd.expires_count = ledtd.expires_count;
	ltd.led_mode = ledtd.led_mode;
	lwm_set_suspend(ltd.led_mode, SUSPEND_RESUME_MODE);

	scpi_send_usr_data(SCPI_CL_LED_TIMER,
				(u32 *)&ltd, sizeof(ltd));
	pr_info("%s scpi send SCPI_CL_LED_TIMER\n", __func__);

	led_early_suspend_flag = 1;
}

static void pwmled_late_suspend(struct early_suspend *h)
{
	pr_info("%s\n", __func__);
	if (!led_early_suspend_flag)
		return;

	led_early_suspend_flag = 0;
}

static struct early_suspend led_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = pwmled_early_suspend,
	.resume = pwmled_late_suspend,
};
#endif


static ssize_t led_debug_show(struct class *cls,
			struct class_attribute *attr, char *buf)
{
	unsigned int len = 0;
	len += sprintf(buf + len, "SCPI_CL_LED_TIMER:\n");
	len += sprintf(buf + len, "    expires  = %u\n", ledtd.expires);
	len += sprintf(buf + len, "    count    = %u\n", ledtd.expires_count);
	len += sprintf(buf + len, "    led_mode = %u\n", ledtd.led_mode);
	return len;
}

static ssize_t led_debug_store(struct class *cls,
			 struct class_attribute *attr,
			 const char *buffer, size_t count)
{
	if (!strncmp(buffer, "send", 4)) {
		scpi_send_usr_data(SCPI_CL_LED_TIMER,
					(u32 *)&ledtd, sizeof(ledtd));
		pr_info("scpi send SCPI_CL_LED_TIMER\n");
	}
	return count;
}

static struct class_attribute led_attrs[] = {
	__ATTR(debug,  0644, led_debug_show, led_debug_store),
	__ATTR_NULL,
};

static struct class ledcls;


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

	/* @todo get pwm configuration from pwm driver */
	/* get pwm device */
	ldev->pwmd = pwm_request(6, AML_DEV_NAME);
	if (IS_ERR(ldev->pwmd)) {
		pr_err("failed to request pwm device\n");
		return PTR_ERR(ldev->pwmd);
	}
	/* set pwm device polarity */
	pwm_set_polarity(ldev->pwmd, ldev->polarity);
	/* first get period from pwm device, if failed set default */
	ldev->period = pwm_get_period(ldev->pwmd);
	pr_info("period = %u\n", ldev->period);
	if (!ldev->period) {
		ldev->period = DEFAULT_PWM_PERIOD;
		pr_err("set default period = %u\n", ldev->period);
	}


#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&led_early_suspend);
#endif


	/* create class attributes */
	ledcls.name = AML_LED_NAME;
	ledcls.owner = THIS_MODULE;
	ledcls.class_attrs = led_attrs;
	class_register(&ledcls);

	pr_info("module probed ok\n");
	return 0;
}


static int __exit aml_pwmled_remove(struct platform_device *pdev)
{
	struct aml_pwmled_dev *ldev = platform_get_drvdata(pdev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&led_early_suspend);
#endif
	class_unregister(&ledcls);
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
	pr_info("module init\n");
	if (platform_driver_register(&aml_pwmled_driver)) {
		pr_err("failed to register driver\n");
		return -ENODEV;
	}

	return 0;
}


static void __exit aml_pwmled_exit(void)
{
	pr_info("module exit\n");
	platform_driver_unregister(&aml_pwmled_driver);
}


module_init(aml_pwmled_init);
module_exit(aml_pwmled_exit);

MODULE_DESCRIPTION("Amlogic pwm led driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");

