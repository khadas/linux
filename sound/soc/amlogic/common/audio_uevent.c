// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "audio_uevent.h"

static struct audio_uevent audio_events[] = {
	{
		.type = AUDIO_SPDIF_FMT_EVENT,
		.env = "AUDIO_FORMAT=",
	},
	{
		.type = HEADPHONE_DETECTION_EVENT,
		.env = "HEADPHONE_DETECTION=",
	},
	{
		.type = MICROPHONE_DETECTION_EVENT,
		.env = "MICROPHONE_DETECTION=",
	},
	{
		.type = AUDIO_NONE_EVENT,
	}
};

int audio_send_uevent(struct snd_soc_card *card, enum audio_event type, int val)
{
	char env[MAX_UEVENT_LEN];
	struct audio_uevent *event = audio_events;
	char *envp[2];
	int ret = -1;

	for (event = audio_events; event->type != AUDIO_NONE_EVENT; event++) {
		if (type == event->type)
			break;
	}

	if (event->type == AUDIO_NONE_EVENT)
		return ret;

	event->state = val;
	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, MAX_UEVENT_LEN, "%s%d", event->env, val);

	ret = kobject_uevent_env(&card->dev->kobj, KOBJ_CHANGE, envp);
	pr_info("%s[%d] %s %d\n", __func__, __LINE__, env, ret);
	return ret;
}

