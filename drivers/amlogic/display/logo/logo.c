/*
 * drivers/amlogic/display/logo/logo.c
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


/* Linux Headers */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ctype.h>

/* Amlogic Headers */
#include <linux/amlogic/vout/vout_notify.h>

/* Local Headers */
#include <osd/osd_hw.h>


#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define LOGO_DEV_OSD0 0x0
#define LOGO_DEV_OSD1 0x1
#define LOGO_DEBUG    0x1001
#define LOGO_LOADED   0x1002

static enum vmode_e hdmimode = VMODE_1080P;
static enum vmode_e cvbsmode = VMODE_480CVBS;

struct para_pair_s {
	char *name;
	int value;
};

static struct para_pair_s logo_args[] = {
	{"osd0", LOGO_DEV_OSD0},
	{"osd1", LOGO_DEV_OSD1},
	{"debug", LOGO_DEBUG},
	{"loaded", LOGO_LOADED},
};

static struct para_pair_s mode_infos[] = {
	{"480cvbs", VMODE_480CVBS},
	{"576cvbs", VMODE_576CVBS},
	{"480i60hz", VMODE_480I},
	{"480p60hz", VMODE_480P},
	{"576i50hz", VMODE_576I},
	{"576p50hz", VMODE_576P},
	{"720p60hz", VMODE_720P},
	{"1080i60hz", VMODE_1080I},
	{"1080p60hz", VMODE_1080P},
	{"720p50hz", VMODE_720P_50HZ},
	{"1080i50hz", VMODE_1080I_50HZ},
	{"1080p50hz", VMODE_1080P_50HZ},
	{"1080p24hz", VMODE_1080P_24HZ},
	{"2160p24hz", VMODE_4K2K_24HZ},
	{"2160p25hz", VMODE_4K2K_25HZ},
	{"2160p30hz", VMODE_4K2K_30HZ},
	{"smpte24hz", VMODE_4K2K_SMPTE},
	{"2160p50hz420", VMODE_4K2K_50HZ_Y420},
	{"2160p60hz420", VMODE_4K2K_60HZ_Y420},
};

struct logo_info_s {
	u32 index;
	u32 vmode;
	u32 debug;
	u32 loaded;
} logo_info = {
	.index = -1,
	.vmode = VMODE_MAX,
	.debug = 0,
	.loaded = 0,
};


static int get_value_by_name(char *name, struct para_pair_s *pair, u32 cnt)
{
	u32 i = 0;
	int found = -1;

	for (i = 0; i < cnt; i++, pair++) {
		if (strcmp(pair->name, name) == 0) {
			found = pair->value;
			break;
		}
	}

	return found;
}

static char *get_name_by_value(u32 value, struct para_pair_s *pair, u32 cnt)
{
	u32 i = 0;
	char *found = NULL;

	for (i = 0; i < cnt; i++, pair++) {
		if (value == pair->value) {
			found = pair->name;
			break;
		}
	}

	return found;
}

static int logo_info_init(char *para)
{
	u32 count = 0;
	int value = -1;

	count = sizeof(logo_args) / sizeof(logo_args[0]);
	value = get_value_by_name(para, logo_args, count);
	if (value >= 0) {
		switch (value) {
		case LOGO_DEV_OSD0:
			logo_info.index = LOGO_DEV_OSD0;
			break;
		case LOGO_DEV_OSD1:
			logo_info.index = LOGO_DEV_OSD1;
			break;
		case LOGO_DEBUG:
			logo_info.debug = 1;
			break;
		case LOGO_LOADED:
			logo_info.loaded = 1;
			break;
		default:
			break;
		}
		return 0;
	}

	count = sizeof(mode_infos) / sizeof(mode_infos[0]);
	value = get_value_by_name(para, mode_infos, count);
	if (value >= 0)
		logo_info.vmode = value;

	return 0;
}

static int str2lower(char *str)
{
	while (*str != '\0') {
		*str = tolower(*str);
		str++;
	}
	return 0;
}

static int __init logo_setup(char *str)
{
	char *ptr = str;
	char sep[2];
	char *option;
	int count = 5;
	char find = 0;

	if (NULL == str)
		return -EINVAL;

	do {
		if (!isalpha(*ptr) && !isdigit(*ptr)) {
			find = 1;
			break;
		}
	} while (*++ptr != '\0');

	if (!find)
		return -EINVAL;

	logo_info.index = -1;
	logo_info.debug = 0;
	logo_info.loaded = 0;
	logo_info.vmode = VMODE_MAX;

	sep[0] = *ptr;
	sep[1] = '\0';
	while ((count--) && (option = strsep(&str, sep))) {
		pr_info("%s\n", option);
		str2lower(option);
		logo_info_init(option);
	}
	return 0;
}
__setup("logo=", logo_setup);

static int __init get_hdmi_mode(char *str)
{
	u32 i;
	u32 cnt;

	cnt = sizeof(mode_infos) / sizeof(mode_infos[0]);
	for (i = 0; i < cnt; i++) {
		if (strcmp(mode_infos[i].name, str) == 0) {
			hdmimode = mode_infos[i].value;
			break;
		}
	}

	pr_info("get hdmimode: %s\n", str);
	return 1;
}
__setup("hdmimode=", get_hdmi_mode);

static int __init get_cvbs_mode(char *str)
{
	if (strncmp("480", str, 3) == 0)
		cvbsmode = VMODE_480CVBS;
	else if (strncmp("576", str, 3) == 0)
		cvbsmode = VMODE_576CVBS;
	else if (strncmp("nocvbs", str, 6) == 0)
		cvbsmode = hdmimode;
	pr_info("get cvbsmode: %s\n", str);
	return 1;
}
__setup("cvbsmode=", get_cvbs_mode);


static int __init logo_init(void)
{
	int ret = 0;
	u32 cnt = 0;

	pr_info("%s\n", __func__);

	if (logo_info.loaded == 0)
		return 0;

	cnt = sizeof(mode_infos) / sizeof(mode_infos[0]);
	if (logo_info.vmode < VMODE_MAX) {
		set_logo_vmode(logo_info.vmode);
		pr_info("set vmode: %s\n",
			get_name_by_value(logo_info.vmode, mode_infos, cnt));
	}

	if ((logo_info.index >= 0)) {
		osd_set_logo_index(logo_info.index);
		osd_init_hw(logo_info.loaded);
	}

	return ret;
}
subsys_initcall(logo_init);
