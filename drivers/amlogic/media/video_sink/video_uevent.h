/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VIDEO_UEVENT_H__
#define __VIDEO_UEVENT_H__

#include <linux/kobject.h>

enum video_event {
	VIDEO_NONE_EVENT = 0,
	VIDEO_FMT_EVENT,
};

#define MAX_UEVENT_LEN 64
struct video_uevent {
	const enum video_event type;
	int state;
	const char *env;
};

int video_send_uevent(enum video_event type, int val);

#endif

