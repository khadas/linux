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
#include <linux/amlogic/vout/vinfo.h>
#include "vout_log.h"

struct vmode_match_s {
	char *name;
	enum vmode_e mode;
};

static struct vmode_match_s vmode_match_table[] = {
	{"480i60hz",      VMODE_480I},
	{"480irpt",       VMODE_480I_RPT},
	{"480cvbs",       VMODE_480CVBS},
	{"480p60hz",      VMODE_480P},
	{"480prtp",       VMODE_480P_RPT},
	{"576i50hz",      VMODE_576I},
	{"576irpt",       VMODE_576I_RPT},
	{"576cvbs",       VMODE_576CVBS},
	{"576p50hz",      VMODE_576P},
	{"576prpt",       VMODE_576P_RPT},
	{"720p60hz",      VMODE_720P},
	{"720p50hz",      VMODE_720P_50HZ},
	{"768p60hz",      VMODE_768P},
	{"768p50hz",      VMODE_768P_50HZ},
	{"1080i60hz",     VMODE_1080I},
	{"1080i50hz",     VMODE_1080I_50HZ},
	{"1080p60hz",     VMODE_1080P},
	{"1080p25hz",     VMODE_1080P_25HZ},
	{"1080p30hz",     VMODE_1080P_30HZ},
	{"1080p50hz",     VMODE_1080P_50HZ},
	{"1080p24hz",     VMODE_1080P_24HZ},
	{"2160p30hz",     VMODE_4K2K_30HZ},
	{"2160p25hz",     VMODE_4K2K_25HZ},
	{"2160p24hz",     VMODE_4K2K_24HZ},
	{"smpte24hz",     VMODE_4K2K_SMPTE},
	{"smpte25hz",     VMODE_4K2K_SMPTE_25HZ},
	{"smpte30hz",     VMODE_4K2K_SMPTE_30HZ},
	{"smpte50hz420",  VMODE_4K2K_SMPTE_50HZ_Y420},
	{"smpte50hz",     VMODE_4K2K_SMPTE_50HZ},
	{"smpte60hz420",  VMODE_4K2K_SMPTE_60HZ_Y420},
	{"smpte60hz",     VMODE_4K2K_SMPTE_60HZ},
	{"4k2k5g",        VMODE_4K2K_FAKE_5G},
	{"2160p60hz420",  VMODE_4K2K_60HZ_Y420},
	{"2160p60hz",     VMODE_4K2K_60HZ},
	{"2160p50hz420",  VMODE_4K2K_50HZ_Y420},
	{"2160p50hz",     VMODE_4K2K_50HZ},
	{"2160p5g",       VMODE_4K2K_5G},
	{"4k1k120hz420",  VMODE_4K1K_120HZ_Y420},
	{"4k1k120hz",     VMODE_4K1K_120HZ},
	{"4k1k100hz420",  VMODE_4K1K_100HZ_Y420},
	{"4k1k100hz",     VMODE_4K1K_100HZ},
	{"4k05k240hz420", VMODE_4K05K_240HZ_Y420},
	{"4k05k240hz",    VMODE_4K05K_240HZ},
	{"4k05k200hz420", VMODE_4K05K_200HZ_Y420},
	{"4k05k200hz",    VMODE_4K05K_200HZ},
	{"panel",         VMODE_LCD},
	{"invalid",       VMODE_INIT_NULL},
};

static struct vinfo_s vinfo_invalid = {
	.name              = "invalid",
	.mode              = VMODE_INIT_NULL,
	.width             = 1920,
	.height            = 1080,
	.field_height      = 1080,
	.aspect_ratio_num  = 16,
	.aspect_ratio_den  = 9,
	.sync_duration_num = 60,
	.sync_duration_den = 1,
	.screen_real_width = 16,
	.screen_real_height = 9,
	.video_clk = 148500000,
	.viu_color_fmt = TVIN_RGB444,
};

enum vmode_e vmode_name_to_mode(const char *str)
{
	int i;
	enum vmode_e vmode = VMODE_MAX;

	for (i = 0; i < ARRAY_SIZE(vmode_match_table); i++) {
		if (strstr(str, vmode_match_table[i].name)) {
			vmode = vmode_match_table[i].mode;
			break;
		}
#if 0
		if (strcmp(vmode_match_table[i].name, str) == 0) {
			vmode = vmode_match_table[i].mode;
			break;
		}
#endif
	}

	return vmode;
}

const char *vmode_mode_to_name(enum vmode_e vmode)
{
	int i;
	char *str = vinfo_invalid.name;

	for (i = 0; i < ARRAY_SIZE(vmode_match_table); i++) {
		if (vmode == vmode_match_table[i].mode) {
			str = vmode_match_table[i].name;
			break;
		}
	}

	return str;
}

struct vinfo_s *get_invalid_vinfo(void)
{
	vout_log_info("error: invalid vinfo. current vmode is not supported\n");
	return &vinfo_invalid;
}

