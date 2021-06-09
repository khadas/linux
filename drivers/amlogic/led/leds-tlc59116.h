/* SPDX-License-Identifier: LGPL-2.1+
 * driver/amlogic/led/leds_tlc59116.h
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

#ifndef _LEDS_TLC59116_H_
#define _LEDS_TLC59116_H_

#define REGISTER_OFFSET 2

struct meson_tlc59116_io {
	unsigned int r_io;
	unsigned int g_io;
	unsigned int b_io;
};

struct meson_tlc59116_colors {
	unsigned int red;
	unsigned int green;
	unsigned int blue;
};

struct meson_tlc59116 {
	struct device *dev;
	struct i2c_client *i2c;
	struct led_classdev cdev;
	struct meson_tlc59116_io *io;
	struct meson_tlc59116_colors *colors;
	unsigned int *colors_buf;
	int ignore_led_suspend;
	int reset_gpio;
	int led_counts;
};

enum led_num {
	LED_NUM0 = 0,
	LED_NUM1,
	LED_NUM2,
	LED_NUM3,
};
#endif

