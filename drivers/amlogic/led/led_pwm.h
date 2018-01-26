/*
 * drivers/amlogic/led/led_pwm.h
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

#ifndef __AML_LED_PWM_H
#define __AML_LED_PWM_H

#include <linux/leds.h>
#include <linux/pwm.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/amlogic/led.h>



struct aml_pwmled_dev {
	struct led_classdev cdev;
	enum led_brightness new_brightness;

	struct pwm_device *pwmd;
	struct pwm_device *pwmd2;
	unsigned int pwm;
	enum pwm_polarity polarity;
	unsigned int period;
	int new_duty;
	struct led_timer_data ltd;

	struct work_struct work;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend es;
#endif
};

#endif

