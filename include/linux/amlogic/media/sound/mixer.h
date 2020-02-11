/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __MIXER_H__
#define __MIXER_H__

#include <sound/control.h>

static int snd_int_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffffffff;
	uinfo->count = 1;

	return 0;
}

#define SND_INT(xname, xhandler_get, xhandler_put)     \
{                                      \
	.iface = SNDRV_CTL_ELEM_IFACE_PCM, \
	.name  = xname,               \
	.info  = snd_int_info,        \
	.get   = xhandler_get,        \
	.put   = xhandler_put,        \
}

#endif
