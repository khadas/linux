// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include "video_uevent.h"
#include "video_priv.h"

static struct video_uevent video_events[] = {
	{
		.type = VIDEO_FMT_EVENT,
		.env  = "VIDEO_FORMAT=",
	},

	{
		.type = VIDEO_NONE_EVENT,
	}
};

int video_send_uevent(enum video_event type, int val)
{
	char env[MAX_UEVENT_LEN];
	struct video_uevent *event = video_events;
	char *envp[2];
	int ret = -1;
	struct device *dev = NULL;

	if (unlikely(in_interrupt()))
		return ret;

	for (event = video_events; event->type != VIDEO_NONE_EVENT; event++) {
		if (type == event->type)
			break;
	}

	if (event->type == VIDEO_NONE_EVENT)
		return ret;

	dev = get_video_device();

	if (!dev)
		return ret;

	event->state = val;
	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, MAX_UEVENT_LEN, "%s%d", event->env, val);

	ret = kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
	pr_info("%s: %s, ret=%d\n", __func__, env, ret);
	return ret;
}

