/*
 * include/linux/amlogic/led.h
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

#ifndef __AML_LINUX_LED_H
#define __AML_LINUX_LED_H

#include <linux/string.h>


#define SHUTDOWN_MODE 0
#define SUSPEND_RESUME_MODE 1


enum led_workmode {
	LWM_OFF,
	LWM_ON,
	LWM_FLASH,
	LWM_BREATH,
	LWM_NULL,
};


/* s,b,w type is enum led_workmode */
#define lwm_set_standby(mode, s) (mode |= (s) << 0)
#define lwm_set_booting(mode, b) (mode |= (b) << 4)
#define lwm_set_working(mode, w) (mode |= (w) << 8)
#define lwm_set_suspend(mode, s) (mode |= (s) << 12)

#define lwm_get_standby(mode) (((mode) >> 0) & 0xF)
#define lwm_get_booting(mode) (((mode) >> 4) & 0xF)
#define lwm_get_working(mode) (((mode) >> 8) & 0xF)
#define lwm_get_suspend(mode) (((mode) >> 12) & 0xF)


static inline enum led_workmode lwm_get_workmode(char *str)
{
	if (!str)
		return LWM_NULL;
	else if (!strncmp(str, "off", 3))
		return LWM_OFF;
	else if (!strncmp(str, "on", 2))
		return LWM_ON;
	else if (!strncmp(str, "flash", 5))
		return LWM_FLASH;
	else if (!strncmp(str, "breath", 6))
		return LWM_BREATH;
	else
		return LWM_NULL;
}


/* options example:
 * ledmode=standby:on,booting:flash,working:breath
 */
static inline unsigned int lwm_parse_workmode(char *options)
{
	char *option;
	enum led_workmode mode;
	unsigned int ledmode = 0;

	while ((option = strsep(&options, ",")) != NULL) {
		if (!strncmp(option, "standby:", 8)) {
			option += 8;
			mode = lwm_get_workmode(option);
			lwm_set_standby(ledmode, mode);
		}

		if (!strncmp(option, "booting:", 8)) {
			option += 8;
			mode = lwm_get_workmode(option);
			lwm_set_booting(ledmode, mode);
		}

		if (!strncmp(option, "working:", 8)) {
			option += 8;
			mode = lwm_get_workmode(option);
			lwm_set_working(ledmode, mode);
		}
	}

	return ledmode;
}


struct led_timer_data {
	unsigned int expires;
	unsigned int expires_count;
	unsigned int led_mode;
};

struct led_disturb_data {
	unsigned int mode;
	unsigned int poweron_time;
	unsigned int suspend_time;
	unsigned int resume_time;
	unsigned int count;
};


#endif

