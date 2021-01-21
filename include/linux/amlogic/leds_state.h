/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __LEDS_STATE_H
#define __LEDS_STATE_H

#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/of.h>

#define DRIVER_NAME			"[Led State]"
#define MESON_LEDS_SCPI_CNT		50
#define MESON_LEDS_MAX_BLINK_CNT	15
#define MESON_LEDS_BREATH_MAX_COUNT	4
/* Limit on stick mem */
//#define LED_MAX_TIMES_CNT		255
#define MESON_LEDS_MAX_TIMES_CNT	0x3F
#define MESON_LEDS_MAX_HIGH_MS		(50 * MESON_LEDS_MAX_TIMES_CNT)
#define MESON_LEDS_MAX_LOW_MS		(50 * MESON_LEDS_MAX_TIMES_CNT)

enum led_state_t {
	LED_STATE_DEFAULT = 0,
	LED_STATE_BRIGHTNESS,
	LED_STATE_BREATH,
	LED_STATE_BLINK_ON,
	LED_STATE_BLINK_OFF,
	LED_STATE_BLINK_BREATH,
	LED_STATE_BLINK_ON_HANDLE,
	LED_STATE_BLINK_OFF_HANDLE,
	LED_STATE_BLINK_BREATH_HANDLE,
	LED_STATE_INVALID,
};

enum LedBrightness {
	LED_STATE_OFF = 0,
	LED_STATE_HALF = 127,
	LED_STATE_FULL = 255,
};

struct led_state_data {
	struct led_classdev	cdev;
};

/* leds state APIs */
int meson_led_state_set_brightness(u32 led_id, u32 brightness);
int meson_led_state_set_breath(u32 led_id, u32 breath_id);
int meson_led_state_set_blink_on(u32 led_id, u32 blink_times,
				 u32 blink_high, u32 blink_low,
				 u32 brightness_high,
				 u32 brightness_low);
int meson_led_state_set_blink_off(u32 led_id, u32 blink_times,
				  u32 blink_high, u32 blink_low,
				  u32 brightness_high,
				  u32 brightness_low);

#endif /* __LEDS_STATE_H */
