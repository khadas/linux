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
#include <linux/reboot.h>


enum breathe_state {
	BREATHE_STATE_OFF = 0,
	BREATHE_STATE_UP,
	BREATHE_STATE_PAUSE,
	BREATHE_STATE_DOWN,
	BREATHE_STATE_ON,
};

struct trigger_breathe_data {
	enum breathe_state state;
	struct timer_list timer;
};


static void led_breathe_function(unsigned long data)
{
	struct led_classdev *ldev = (struct led_classdev *) data;
	struct trigger_breathe_data *breathe_data = ldev->trigger_data;

	int brightness = LED_OFF;
	unsigned long delay = 0;

	/* @todo breathe led setting */

	switch (breathe_data->state) {
	case BREATHE_STATE_OFF:
		delay = msecs_to_jiffies(1000);

		brightness = LED_OFF;
		breathe_data->state = BREATHE_STATE_UP;
		break;

	case BREATHE_STATE_UP:
		delay = msecs_to_jiffies(50);

		brightness = ldev->brightness + 4;
		if (brightness > ldev->max_brightness) {
			brightness =  ldev->max_brightness;
			breathe_data->state = BREATHE_STATE_PAUSE;
		}
		break;

	case BREATHE_STATE_PAUSE:
		delay = msecs_to_jiffies(20);

		brightness = ldev->max_brightness;
		breathe_data->state = BREATHE_STATE_DOWN;
		break;

	case BREATHE_STATE_DOWN:
		delay = msecs_to_jiffies(25);

		brightness = ldev->brightness - 2;
		if (brightness < 0) {
			brightness =  LED_OFF;
			breathe_data->state = BREATHE_STATE_OFF;
		}
		break;

	default:
		delay = 0;
		brightness = LED_OFF;
		break;

	}

	led_set_brightness(ldev, brightness);
	mod_timer(&breathe_data->timer, jiffies + delay);
}

static void trigger_breathe_activate(struct led_classdev *ldev)
{
	struct trigger_breathe_data *breathe_data;

	breathe_data = kzalloc(sizeof(*breathe_data), GFP_KERNEL);
	if (!breathe_data)
		return;

	ldev->trigger_data = breathe_data;
	setup_timer(&breathe_data->timer,
		    led_breathe_function, (unsigned long) ldev);

	/* @todo init parameter */
	breathe_data->state = BREATHE_STATE_OFF;

	led_breathe_function(breathe_data->timer.data);
	ldev->activated = true;
}

static void trigger_breathe_deactivate(struct led_classdev *ldev)
{
	struct trigger_breathe_data *breathe_data = ldev->trigger_data;

	if (ldev->activated) {
		del_timer_sync(&breathe_data->timer);
		kfree(breathe_data);
		ldev->activated = false;
	}
}

static struct led_trigger breathe_led_trigger = {
	.name     = "breathe",
	.activate = trigger_breathe_activate,
	.deactivate = trigger_breathe_deactivate,
};

static int __init trigger_breathe_init(void)
{
	return led_trigger_register(&breathe_led_trigger);
}

static void __exit trigger_breathe_exit(void)
{
	led_trigger_unregister(&breathe_led_trigger);
}

module_init(trigger_breathe_init);
module_exit(trigger_breathe_exit);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Breathe LED trigger");
MODULE_LICENSE("GPL");

