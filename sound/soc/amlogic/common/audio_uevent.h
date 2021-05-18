/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AUDIO_UEVENT_H__
#define __AUDIO_UEVENT_H__

#include <linux/kobject.h>
#include <sound/soc.h>

enum audio_event {
	AUDIO_NONE_EVENT = 0,
	AUDIO_SPDIF_FMT_EVENT,
	HEADPHONE_DETECTION_EVENT,
	MICROPHONE_DETECTION_EVENT,
	EARCTX_ATNDTYP_EVENT,
};

#define MAX_UEVENT_LEN 64
struct audio_uevent {
	const enum audio_event type;
	int state;
	const char *env;
};

int audio_send_uevent(struct device *dev, enum audio_event type, int val);

#endif
