// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vfm/vframe.h>
#include "amdv_uevent.h"

static struct amdv_uevent amdv_events[] = {
	{
		.type = AMDV_CONT_EVENT,
		.env = {
			"AMDV_CONTENT_TYPE=",
			"AMDV_WHITE_POINT=",
			"AMDV_L11_BYTE2=",
			"AMDV_L11_BYTE3=",
			}
	},

	{
		.type = AMDV_NONE_EVENT,
	}
};

int amdv_send_uevent(enum amdv_event type, struct apo_value_s *apo_value)
{
	char env[AMDV_MAX_UEVENT_LEN];
	struct amdv_uevent *event = amdv_events;
	char *envp[2];
	int ret = -1;
	struct device *dev = NULL;

	if (unlikely(in_interrupt()))
		return ret;

	for (event = amdv_events; event->type != AMDV_NONE_EVENT; event++) {
		if (type == event->type)
			break;
	}

	if (event->type == AMDV_NONE_EVENT)
		return ret;

	dev = get_amdv_device();

	if (!dev)
		return ret;

	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, AMDV_MAX_UEVENT_LEN, "%s%d %s%d %s%d %s%d",
		event->env[0], apo_value->content_type,
		event->env[1], apo_value->white_point,
		event->env[2], apo_value->L11_byte2,
		event->env[3], apo_value->L11_byte3);

	ret = kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);
	pr_info("%s: %s, ret=%d\n", __func__, env, ret);
	return ret;
}

