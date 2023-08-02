/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __AMDV_UEVENT_H__
#define __AMDV_UEVENT_H__

#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>

enum amdv_event {
	AMDV_NONE_EVENT = 0,
	AMDV_CONT_EVENT,
};

#define AMDV_MAX_UEVENT_LEN 256
struct amdv_uevent {
	const enum amdv_event type;
	const char *env[4];
};

int amdv_send_uevent(enum amdv_event type, struct apo_value_s *apo_value);
struct device *get_amdv_device(void);

#endif
