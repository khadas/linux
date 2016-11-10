/*
 * drivers/amlogic/led/led_trigger_breathe.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>
#include "pwm_reg.h"

#define LS_POLLING_DELAY 60
#define PWM_PRE_DIV_DEFAULT (0)
#define PWM_DUTY_TOTAL_DEFAULT 0xffff
#define PWM_DUTY_HIGH_DEFAULT  1


static int breathe_led_enable = 1;
char inc_flag = 0;
static short pwm_h = 0x7e00;
static short pwm_l = 0x0;
static short dep = 0x2a0;
int waitcnt = 0;


enum breathe_state {
	BREATHE_STATE_OFF = 0,
	BREATHE_STATE_UP,
	BREATHE_STATE_PAUSE,
	BREATHE_STATE_DOWN,
	BREATHE_STATE_ON,
};

struct trigger_breathe_data {
	enum breathe_state state;
	struct class *gpio_pwm_class;
	struct device *gpio_pwm_dev;
	atomic_t delay;
	int enable;
	struct workqueue_struct *gpio_pwm_wq;
	struct delayed_work work;
	int polling_delay;
	int pwm_duty_total;
	int pwm_duty_high;
	int pwm_pre_div;
};

static struct trigger_breathe_data *breathe_data;



static int pwm_on(void)
{
	bl_write_ao_reg(AO_PWM_MISC_REG_AB, 0, 16, 7);
	bl_write_ao_reg(AO_PWM_MISC_REG_AB, 1, 23, 1);
	bl_write_ao_reg(AO_PWM_MISC_REG_AB, 1, 1, 1);
	bl_write_ao_reg(AO_RTI_PIN_MUX_REG, 0, 31, 1);
	bl_write_ao_reg(AO_RTI_PIN_MUX_REG, 0, 4, 1);
	bl_write_ao_reg(AO_RTI_PIN_MUX_REG, 1, 3, 1);
	bl_write_ao_reg(AO_RTI_PIN_MUX_REG2, 0, 1, 1);
	return 0;
}

static int pwm_off(void)
{
	bl_write_ao_reg(AO_PWM_MISC_REG_AB, 0, 1, 1);
	bl_write_ao_reg(AO_RTI_PIN_MUX_REG, 0, 31, 1);
	bl_write_ao_reg(AO_RTI_PIN_MUX_REG, 0, 4, 1);
	bl_write_ao_reg(AO_RTI_PIN_MUX_REG, 0, 3, 1);
	bl_write_ao_reg(AO_RTI_PIN_MUX_REG2, 0, 1, 1);
	gpio_direction_output(228, 0);
	return 0;
}
static int pwm_duty_set(int duty_high, int duty_low)
{
	int val = 0;
	bl_write_ao_reg(AO_PWM_MISC_REG_AB, breathe_data->pwm_pre_div, 16, 7);
	bl_write_ao_reg(AO_PWM_PWM_B, duty_high , 16, 16);
	bl_write_ao_reg(AO_PWM_PWM_B, duty_low, 0 , 16);
	val = aml_read_aobus(AO_PWM_PWM_B);
	return 0;
}

void breathe_led(void)
{
		if (inc_flag == 0) {
			pwm_h = pwm_h - dep;
			pwm_l = pwm_l + dep;
			if (pwm_h == 0) {
				inc_flag = 1;
				waitcnt = 1;
			}
		} else {
			pwm_h = pwm_h + dep;
			pwm_l = pwm_l - dep;
			if (pwm_l == 0)
				inc_flag = 0;
		}
		pwm_duty_set(pwm_h, pwm_l);
}

void breathe_enable(int enable)
{
	if (enable == 1) {
		pwm_on();
		schedule_delayed_work(&breathe_data->work, 1 * HZ);
	} else {
		pwm_off();
		cancel_delayed_work(&breathe_data->work);
	}
	breathe_led_enable = enable;
}

static void gpio_pwm_work(struct work_struct *work)
{
	unsigned long delay = msecs_to_jiffies(
			atomic_read(&breathe_data->delay));

	if (breathe_led_enable == 1)
		breathe_led();

	if (waitcnt == 1) {
		msleep(500);
		waitcnt = 0;
	}
	schedule_delayed_work(&breathe_data->work, delay);
}



static void trigger_breathe_activate(struct led_classdev *ldev)
{

	ldev->trigger_data = breathe_data;

	/* @todo init parameter */
	breathe_data->state = BREATHE_STATE_OFF;

	ldev->activated = true;
	breathe_enable(1);
}

static void trigger_breathe_deactivate(struct led_classdev *ldev)
{

	if (ldev->activated) {
		ldev->activated = false;
		breathe_enable(0);
	}
}

static struct led_trigger breathe_led_trigger = {
	.name     = "breathe",
	.activate = trigger_breathe_activate,
	.deactivate = trigger_breathe_deactivate,
};

static int __init trigger_breathe_init(void)
{
	int ret = 0;
	breathe_data = kzalloc(sizeof(*breathe_data), GFP_KERNEL);
	if (!breathe_data)
		return -ENOMEM;
	breathe_data->polling_delay = msecs_to_jiffies(LS_POLLING_DELAY);
	INIT_DELAYED_WORK(&breathe_data->work, gpio_pwm_work);
	atomic_set(&breathe_data->delay, LS_POLLING_DELAY);
	breathe_data->gpio_pwm_wq =
		create_singlethread_workqueue("gpio_pwm_wq");
	if (!breathe_data->gpio_pwm_wq) {
		pr_err("%s:  can't create workqueue\n", __func__);
		ret = -ENOMEM;
		goto err_create_singlethread_workqueue;
	}
	breathe_data->gpio_pwm_class = class_create(THIS_MODULE, "gpio_pwm");
	if (IS_ERR(breathe_data->gpio_pwm_class)) {
		ret = PTR_ERR(breathe_data->gpio_pwm_class);
		breathe_data->gpio_pwm_class = NULL;
		goto err_create_class;
	}

	breathe_data->gpio_pwm_dev = device_create(
			breathe_data->gpio_pwm_class,
			NULL, 0, "%s", "gpio_pwm");
	if (unlikely(IS_ERR(breathe_data->gpio_pwm_dev))) {
		ret = PTR_ERR(breathe_data->gpio_pwm_dev);
		breathe_data->gpio_pwm_dev = NULL;
		goto err_create_gpio_pwm_device;
	}
	breathe_data->pwm_duty_total = PWM_DUTY_TOTAL_DEFAULT;
	breathe_data->pwm_duty_high = PWM_DUTY_HIGH_DEFAULT;
	breathe_data->pwm_pre_div = PWM_PRE_DIV_DEFAULT;

	led_trigger_register(&breathe_led_trigger);

	return ret;
err_create_gpio_pwm_device:
	class_destroy(breathe_data->gpio_pwm_class);
err_create_class:
	destroy_workqueue(breathe_data->gpio_pwm_wq);
err_create_singlethread_workqueue:
	kfree(breathe_data);
	return ret;
}

static void __exit trigger_breathe_exit(void)
{

	device_unregister(breathe_data->gpio_pwm_dev);
	class_destroy(breathe_data->gpio_pwm_class);
	destroy_workqueue(breathe_data->gpio_pwm_wq);
	kfree(breathe_data);
	led_trigger_unregister(&breathe_led_trigger);
}

module_init(trigger_breathe_init);
module_exit(trigger_breathe_exit);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Breathe LED trigger");
MODULE_LICENSE("GPL");

