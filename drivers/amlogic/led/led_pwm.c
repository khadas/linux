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

#include <linux/pwm.h>
#include <linux/amlogic/pwm_meson.h>

#include "led_pwm.h"

#define AML_DEV_NAME		"pwmled"
#define AML_LED_NAME		"led-pwm"

#define DEFAULT_PWM_PERIOD	100000;
#define LED_ON  1
#define LED_OFF 0

static unsigned int ledmode;
static struct timer_list timer;
static void pwmled_timer_sr(unsigned long data);
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
	duty = ldev->period;

	max = ldev->cdev.max_brightness;
	/* calculate new duty */
	duty *= brightness;
	do_div(duty, max);
	/* save new duty */
	ldev->new_duty = duty;
	/* save new brightness */
	ldev->new_brightness = brightness;

	/* schedule work */
	schedule_work(&ldev->work);
}

/**clear_pwm_ao_a:clear pwm ao a config
*if not, the config in uboot ,bl301 will
*affect kernel.
*/
static int clear_pwm_ao_a(struct platform_device *pdev)
{

	struct pwm_device *pwm_ch1 = NULL;
	struct aml_pwm_chip *aml_chip = NULL;
	struct aml_pwmled_dev *ldev = platform_get_drvdata(pdev);

	/*get pwm device*/
	pwm_ch1 = ldev->pwmd;
	aml_chip = to_aml_pwm_chip(pwm_ch1->chip);
	/*clear duty A1 A2*/
	pwm_write_reg(aml_chip->ao_base + REG_PWM_AO_A, 0);
	pwm_write_reg(aml_chip->ao_base + REG_PWM_AO_A2, 0);
	/*clear clock ,blink, times,output enable and so on*/
	pwm_clear_reg_bits(aml_chip->ao_base + REG_MISC_AO_AB,
	(0x1<<0)|(0x3<<4)|(0xff<<8)|(1<<25)|(1<<26));
	pwm_clear_reg_bits(aml_chip->ao_base + REG_TIME_AO_AB, (0xffff<<16));
	pwm_clear_reg_bits(aml_chip->ao_blink_base + REG_BLINK_AO_AB,
	(0xff)|(1<<8));

	return 0;
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
		pr_err("unable to request pwm device for %s\n",
			node->full_name);
		ret = PTR_ERR(ldev->pwmd);
		return ret;
	}

	/*request ao a2*/
	ldev->pwmd2 = pwm_request(ldev->pwmd->hwpwm + 8, NULL);
	if (IS_ERR(ldev->pwmd2)) {
		pr_err("request pwm A2 failed\n");
		ret = PTR_ERR(ldev->pwmd);
		return ret;
	}
	ret = clear_pwm_ao_a(pdev);
	if (ret)
		pr_err("clear ao a failed\n");
	/* Get the period from PWM core when n*/
	ldev->period = pwm_get_period(ldev->pwmd);

	ldev->pwmd2->hwpwm = ldev->pwmd->hwpwm + 8;
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

	platform_set_drvdata(pdev, ldev);
	return 0;
}



static const struct of_device_id aml_pwmled_dt_match[] = {
	{
		.compatible = "amlogic, pwmled",
	},
	{},
};

/**
*pwmled_output_init,turn off the led Initialized.
*for txl p320 low level off.(shuld set polarity as 0)   0% output
*for tcl p332,high level off.(shuld set polarity as 1)  100% output
*polarity set 0 : 0%   off
*                 100%  on
*polarity set 1:  0%   on
*                 100% off
*/
static int pwmled_output_init(struct platform_device *pdev)
{
	int duty_off;
	int period;
	struct aml_pwmled_dev *ldev = platform_get_drvdata(pdev);

	period = ldev->period;
	duty_off = period-period;/*0% pwm output*/
	pwm_set_period(ldev->pwmd, period);
	pwm_config(ldev->pwmd, duty_off, period);
	pwm_enable(ldev->pwmd);
	return 0;
}

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

	setup_timer(&timer, pwmled_timer_sr, (unsigned long)pdev);
	/* register led class device */
	ldev->cdev.name = AML_LED_NAME;
	pwmled_output_init(pdev);
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

int pwmled_on_off(struct platform_device *pdev, int led_switch)
{
	struct pwm_device *pwm_ch1 = NULL;
	struct pwm_device *pwm_ch2 = NULL;
	struct aml_pwm_chip *aml_chip1 = NULL;
	unsigned int ch1_num;
	unsigned int period;
	unsigned int duty_on;
	unsigned int duty_off;
	struct aml_pwmled_dev *ldev = platform_get_drvdata(pdev);

	/*get pwm device*/
	pwm_ch1 = ldev->pwmd;
	pwm_ch2 = ldev->pwmd2;
	/*get aml_pwm device*/
	aml_chip1 = to_aml_pwm_chip(pwm_ch1->chip);

	/*get 2 channel num*/
	ch1_num = ldev->pwmd->hwpwm;
	/*get period and duty*/
	/*duty setting is related to polarity and active level,
	for example :high level lights led*/
	period = ldev->period;
	/*if polarity is 0,will output 0%
	*if polarity is 1,wil output 100%
	*/
	duty_on = period;
	duty_off = period - period;
	/*if not this function,blink 3 times works only one time,why?*/

	pwm_disable(pwm_ch1);
	pwm_disable(pwm_ch2);
	pwm_blink_disable(aml_chip1, ch1_num);
	pwm_set_period(pwm_ch1, period);
	if (LED_ON == led_switch)
		pwm_config(pwm_ch1, duty_on, period);
	else
		pwm_config(pwm_ch1, duty_off, period);
	pwm_enable(pwm_ch1);
	return 0;
}
static int pwmled_blink_times(struct platform_device *pdev, int times)
{
	struct pwm_device *pwm_ch1 = NULL;
	struct pwm_device *pwm_ch2 = NULL;
	struct aml_pwm_chip *aml_chip1 = NULL;
	struct aml_pwm_chip *aml_chip2 = NULL;
	unsigned int ch1_num;
	unsigned int ch2_num;
	unsigned int period;
	unsigned int duty1;
	unsigned int duty2;

	struct aml_pwmled_dev *ldev = platform_get_drvdata(pdev);

	/*get pwm device*/
	pwm_ch1 = ldev->pwmd;
	pwm_ch2 = ldev->pwmd2;

	/*get aml_pwm device*/
	aml_chip1 = to_aml_pwm_chip(pwm_ch1->chip);
	aml_chip2 = to_aml_pwm_chip(pwm_ch2->chip);

	/*get 2 channel num*/
	ch1_num = ldev->pwmd->hwpwm;
	ch2_num = ch1_num + 8;
	ldev->pwmd2->hwpwm = ch1_num + 8;
	/*get period and duty*/
	period = ldev->period;
	/*duty setting is related to polarity and active level,
	for example :high level lights led*/
	if (ldev->polarity)
		duty2 = period - period;
	else
		duty2 = period;/*should be duty2 =period - period;*/
	/*may change here to set duty cycle,
	*when blink,duty set the brightness
	*/
	duty1 = period/2;

	/*if not this function,blink 3 times works only one time,why?*/
	pwm_disable(pwm_ch1);
	pwm_disable(pwm_ch2);
	pwm_blink_disable(aml_chip1, ch1_num);

	pwm_set_period(pwm_ch1, period);
	pwm_set_period(pwm_ch2, period);
	pwm_config(pwm_ch1, duty1, period);
	pwm_config(pwm_ch2, duty2, period);
	pwm_set_times(aml_chip1, ch1_num, 99);
	pwm_set_times(aml_chip2, ch2_num, 99);
	pwm_set_blink_times(aml_chip1, ch1_num, times-1);

	pwm_blink_enable(aml_chip1, ch1_num);
	pwm_enable(pwm_ch1);
	pwm_enable(pwm_ch2);
	return 0;
}
static int aml_pwmled_suspend(struct platform_device *pdev, pm_message_t state)
{
	pr_info("enter aml_pwmled_suspend\n");
	del_timer_sync(&timer);
	return 0;
}
static void pwmled_timer_sr(unsigned long data)
{
	int mode;
	struct platform_device *pdev = (struct platform_device *)data;
	return;
	mode = lwm_get_standby(ledmode);
	if (mode == LWM_BREATH)
		pwmled_on_off(pdev, LED_ON);
	else
		pwmled_on_off(pdev, LED_OFF);
}
static int aml_pwmled_resume(struct platform_device *pdev)
{
	pr_info("aml_pwmled_resume\n");
	pwmled_blink_times(pdev, 3);
	setup_timer(&timer, pwmled_timer_sr, (unsigned long)pdev);
	mod_timer(&timer, jiffies+msecs_to_jiffies(2500));
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
	.suspend = aml_pwmled_suspend,
	.resume = aml_pwmled_resume,
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

