/*
 * drivers/amlogic/led/led_trigger_scpistop.c
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

#undef pr_fmt
#define pr_fmt(fmt)	"scpistoptrig: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/leds.h>
#include <linux/reboot.h>

#include <linux/amlogic/scpi_protocol.h>

#include "led_pwm.h"



static void trigger_scpistop_activate(struct led_classdev *lcdev)
{
	struct aml_pwmled_dev *ldev;
	struct led_timer_data *ltd;

	ltd = kzalloc(sizeof(struct led_timer_data), GFP_KERNEL);
	if (!ltd) {
		pr_err("unable to allocate for scpistop trigger\n");
		return;
	}
	lcdev->trigger_data = ltd;

	ldev = container_of(lcdev, struct aml_pwmled_dev, cdev);
	ltd->expires = ldev->ltd.expires;
	ltd->expires_count = ldev->ltd.expires_count;
	ltd->led_mode = ldev->ltd.led_mode;

	/* set expires_count to 0, and scpi send to stop led timer */
	ltd->expires_count = 0;
	scpi_send_usr_data(SCPI_CL_LED_TIMER, (u32 *)ltd, sizeof(*ltd));

	lcdev->activated = true;
}

static void trigger_scpistop_deactivate(struct led_classdev *lcdev)
{
	struct led_timer_data *ltd = lcdev->trigger_data;

	if (lcdev->activated) {
		kfree(ltd);
		lcdev->activated = false;
	}
}


static struct led_trigger scpistop_led_trigger = {
	.name     = "scpistop",
	.activate = trigger_scpistop_activate,
	.deactivate = trigger_scpistop_deactivate,
};

static int __init trigger_scpistop_init(void)
{
	return led_trigger_register(&scpistop_led_trigger);
}

static void __exit trigger_scpistop_exit(void)
{
	led_trigger_unregister(&scpistop_led_trigger);
}

module_init(trigger_scpistop_init);
module_exit(trigger_scpistop_exit);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("scpistop trigger");
MODULE_LICENSE("GPL");

