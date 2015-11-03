/*
 * drivers/amlogic/display/vout/tv_vout.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/amlogic/vout/vout_notify.h>
#include "vout_log.h"

static char *vmode_name[] = {
	"480i",
	"480irpt",
	"480cvbs",
	"480p",
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	"480p59hz",
#endif
	"480prtp",
	"576i",
	"576irpt",
	"576cvbs",
	"576p",
	"576prpt",
	"720p60hz",
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	"720p59hz",
#endif
	"720p50hz",
	"768p60hz",
	"768p50hz",
	"1080i60hz",
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	"1080i59hz",
#endif
	"1080i50hz",
	"1080p60hz",
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	"1080p59hz",
#endif
	"1080p50hz",
	"1080p24hz",
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	"1080p23hz",
#endif
	"2160p30hz",
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	"2160p29hz",
#endif
	"2160p25hz",
	"2160p24hz",
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	"2160p23hz",
#endif
	"smpte24hz",
	"4k2k5g",
	"2160p60hz",
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	"2160p59hz",
#endif
	"2160p60hz420",
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	"2160p59hz420",
#endif
	"2160p50hz",
	"2160p50hz420",
	"2160p5g",
	"4k1k120hz",
	"4k1k120hz420",
	"4k1k100hz",
	"4k1k100hz420",
	"4k05k240hz",
	"4k05k240hz420",
	"4k05k200hz",
	"4k05k200hz420",
	"panel",
	"invalid",
};

static const struct vinfo_s vinfo_invalid = {
	.name              = "invalid",
	.mode              = VMODE_INIT_NULL,
	.width             = 1920,
	.height            = 1080,
	.field_height      = 1080,
	.aspect_ratio_num  = 16,
	.aspect_ratio_den  = 9,
	.sync_duration_num = 60,
	.sync_duration_den = 1,
	.viu_color_fmt = TVIN_RGB444,
};

enum vmode_e vmode_name_to_mode(const char *str)
{
	int i;
	enum vmode_e vmode;

	for (i = 0; i < VMODE_MAX; i++) {
		if (strcmp(vmode_name[i], str) == 0)
			break;
	}
	vmode = VMODE_480I + i;

	return vmode;
}

const struct vinfo_s *get_invalid_vinfo(void)
{
	vout_log_info("error: invalid vinfo. current vmode is not supported\n");
	return &vinfo_invalid;
}

