/*
 * drivers/amlogic/display/vout/tv_vout.h
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


#ifndef _TV_VOUT_H_
#define _TV_VOUT_H_

/* Amlogic Headers */
#include <linux/amlogic/vout/vout_notify.h>

#define TV_CLASS_NAME	"tv"
#define	MAX_NUMBER_PARA  10

#define SHOW_INFO(name) \
	{return snprintf(buf, 40, "%s\n", name); }

#define  STORE_INFO(name) \
	{mutex_lock(&TV_mutex);\
	snprintf(name, 40, "%s", buf) ;\
	mutex_unlock(&TV_mutex); }

#define SET_TV_CLASS_ATTR(name, op) \
static char name[40]; \
static ssize_t aml_TV_attr_##name##_show(struct class *cla, \
		struct class_attribute *attr, char *buf) \
{ \
	SHOW_INFO(name)	\
} \
static ssize_t  aml_TV_attr_##name##_store(struct class *cla, \
		struct class_attribute *attr, \
			    const char *buf, size_t count) \
{ \
	STORE_INFO(name); \
	op(name); \
	return strnlen(buf, count); \
} \
struct  class_attribute  class_TV_attr_##name =  \
__ATTR(name, S_IRUGO|S_IWUSR, \
		aml_TV_attr_##name##_show, aml_TV_attr_##name##_store);

struct disp_module_info_s {
	unsigned int major;  /* dev major number */
	const struct vinfo_s *vinfo;
	char name[20];
	struct class  *base_class;
};

static  DEFINE_MUTEX(TV_mutex);

struct vmode_tvmode_tab_s {
	enum tvmode_e tvmode;
	enum vmode_e  mode;
};

static struct vmode_tvmode_tab_s mode_tab[] = {
	{TVMODE_480I, VMODE_480I},
	{TVMODE_480I_RPT, VMODE_480I_RPT},
	{TVMODE_480CVBS, VMODE_480CVBS},
	{TVMODE_480P, VMODE_480P},
	{TVMODE_480P_RPT, VMODE_480P_RPT},
	{TVMODE_576I, VMODE_576I},
	{TVMODE_576I_RPT, VMODE_576I_RPT},
	{TVMODE_576CVBS, VMODE_576CVBS},
	{TVMODE_576P, VMODE_576P},
	{TVMODE_576P_RPT, VMODE_576P_RPT},
	{TVMODE_720P, VMODE_720P},
	{TVMODE_1080I, VMODE_1080I},
	{TVMODE_1080P, VMODE_1080P},
	{TVMODE_720P_50HZ, VMODE_720P_50HZ},
	{TVMODE_1080I_50HZ, VMODE_1080I_50HZ},
	{TVMODE_1080P_50HZ, VMODE_1080P_50HZ},
	{TVMODE_1080P_24HZ, VMODE_1080P_24HZ},
	{TVMODE_4K2K_30HZ, VMODE_4K2K_30HZ},
	{TVMODE_4K2K_25HZ, VMODE_4K2K_25HZ},
	{TVMODE_4K2K_24HZ, VMODE_4K2K_24HZ},
	{TVMODE_4K2K_SMPTE, VMODE_4K2K_SMPTE},
	{TVMODE_4K2K_60HZ_Y420, VMODE_4K2K_60HZ_Y420},
	{TVMODE_4K2K_50HZ_Y420, VMODE_4K2K_50HZ_Y420},
	{TVMODE_4K2K_50HZ, VMODE_4K2K_50HZ},
	{TVMODE_VGA, VMODE_VGA},
	{TVMODE_SVGA, VMODE_SVGA},
	{TVMODE_XGA, VMODE_XGA},
	{TVMODE_SXGA, VMODE_SXGA},
	{TVMODE_WSXGA, VMODE_WSXGA},
	{TVMODE_FHDVGA, VMODE_FHDVGA},
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	{TVMODE_480P_59HZ, VMODE_480P_59HZ},
	{TVMODE_720P_59HZ, VMODE_720P_59HZ}, /* for 720p 59.94hz */
	{TVMODE_1080I_59HZ, VMODE_1080I_59HZ},
	{TVMODE_1080P_59HZ, VMODE_1080P_59HZ}, /* for 1080p 59.94hz */
	{TVMODE_1080P_23HZ, VMODE_1080P_23HZ}, /* for 1080p 23.97hz */
	{TVMODE_4K2K_29HZ, VMODE_4K2K_29HZ}, /* for 4k2k 29.97hz */
	{TVMODE_4K2K_23HZ, VMODE_4K2K_23HZ}, /* for 4k2k 23.97hz */
#endif
	{TVMODE_4K1K_100HZ, VMODE_4K1K_100HZ},
	{TVMODE_4K1K_100HZ_Y420, VMODE_4K1K_100HZ_Y420},
	{TVMODE_4K1K_120HZ, VMODE_4K1K_120HZ},
	{TVMODE_4K1K_120HZ_Y420, VMODE_4K1K_120HZ_Y420},
	{TVMODE_4K05K_200HZ, VMODE_4K05K_200HZ},
	{TVMODE_4K05K_200HZ_Y420, VMODE_4K05K_200HZ_Y420},
	{TVMODE_4K05K_240HZ, VMODE_4K05K_240HZ},
	{TVMODE_4K05K_240HZ_Y420, VMODE_4K05K_240HZ_Y420},
};

static const struct vinfo_s tv_info[] = {
	{ /* VMODE_480I */
		.name              = "480i60hz",
		.mode              = VMODE_480I,
		.width             = 720,
		.height            = 480,
		.field_height      = 240,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
	},
	{ /* VMODE_480I_RPT */
		.name              = "480i_rpt",
		.mode              = VMODE_480I_RPT,
		.width             = 720,
		.height            = 480,
		.field_height      = 240,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
	},
	{ /* VMODE_480CVBS*/
		.name              = "480cvbs",
		.mode              = VMODE_480CVBS,
		.width             = 720,
		.height            = 480,
		.field_height      = 240,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
	},
	{ /* VMODE_480P */
		.name              = "480p60hz",
		.mode              = VMODE_480P,
		.width             = 720,
		.height            = 480,
		.field_height      = 480,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
	},
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	{ /* VMODE_480P_59HZ */
		.name              = "480p59hz",
		.mode              = VMODE_480P_59HZ,
		.width             = 720,
		.height            = 480,
		.field_height      = 480,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60000,
		.sync_duration_den = 1001,
		.video_clk         = 27000000,
	},
#endif
	{ /* VMODE_480P_RPT */
		.name              = "480p_rpt",
		.mode              = VMODE_480P_RPT,
		.width             = 720,
		.height            = 480,
		.field_height      = 480,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
	},
	{ /* VMODE_576I */
		.name              = "576i50hz",
		.mode              = VMODE_576I,
		.width             = 720,
		.height            = 576,
		.field_height      = 288,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
	},
	{ /* VMODE_576I_RPT */
		.name              = "576i_rpt",
		.mode              = VMODE_576I_RPT,
		.width             = 720,
		.height            = 576,
		.field_height      = 288,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
	},
	{ /* VMODE_576I */
		.name              = "576cvbs",
		.mode              = VMODE_576CVBS,
		.width             = 720,
		.height            = 576,
		.field_height      = 288,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
	},
	{ /* VMODE_576P */
		.name              = "576p50hz",
		.mode              = VMODE_576P,
		.width             = 720,
		.height            = 576,
		.field_height      = 576,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
	},
	{ /* VMODE_576P_RPT */
		.name              = "576p_rpt",
		.mode              = VMODE_576P_RPT,
		.width             = 720,
		.height            = 576,
		.field_height      = 576,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 27000000,
	},
	{ /* VMODE_720P */
		.name              = "720p60hz",
		.mode              = VMODE_720P,
		.width             = 1280,
		.height            = 720,
		.field_height      = 720,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 74250000,
	},
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	{ /* VMODE_720P_59HZ */
		.name              = "720p59hz",
		.mode              = VMODE_720P_59HZ,
		.width             = 1280,
		.height            = 720,
		.field_height      = 720,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60000,
		.sync_duration_den = 1001,
		.video_clk         = 74250000,
	},
#endif
	{ /* VMODE_1080I */
		.name              = "1080i60hz",
		.mode              = VMODE_1080I,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 540,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 74250000,
	},
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	{ /* VMODE_1080I_59HZ */
		.name              = "1080i59hz",
		.mode              = VMODE_1080I_59HZ,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 540,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60000,
		.sync_duration_den = 1001,
		.video_clk         = 74250000,
	},
#endif
	{ /* VMODE_1080P */
		.name              = "1080p60hz",
		.mode              = VMODE_1080P,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
	},
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	{ /* VMODE_1080P_59HZ */
		.name			   = "1080p59hz",
		.mode			   = VMODE_1080P_59HZ,
		.width			   = 1920,
		.height		   = 1080,
		.field_height	   = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60000,
		.sync_duration_den = 1001,
		.video_clk		   = 148500000,
	},
#endif
	{ /* VMODE_720P_50hz */
		.name              = "720p50hz",
		.mode              = VMODE_720P_50HZ,
		.width             = 1280,
		.height            = 720,
		.field_height      = 720,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 74250000,
	},
	{ /* VMODE_1080I_50HZ */
		.name              = "1080i50hz",
		.mode              = VMODE_1080I_50HZ,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 540,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 74250000,
	},
	{ /* VMODE_1080P_50HZ */
		.name              = "1080p50hz",
		.mode              = VMODE_1080P_50HZ,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
	},
	{ /* VMODE_1080P_24HZ */
		.name              = "1080p24hz",
		.mode              = VMODE_1080P_24HZ,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 24,
		.sync_duration_den = 1,
		.video_clk         = 74250000,
	},
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	{ /* VMODE_1080P_23HZ */
		.name			   = "1080p23hz",
		.mode			   = VMODE_1080P_23HZ,
		.width			   = 1920,
		.height		   = 1080,
		.field_height	   = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 2397,
		.sync_duration_den = 100,
		.video_clk		   = 74250000,
	},
#endif
	{ /* VMODE_4K2K_30HZ */
		.name              = "2160p30hz",
		.mode              = VMODE_4K2K_30HZ,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 30,
		.sync_duration_den = 1,
		.video_clk         = 297000000,
	},
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	{ /* VMODE_4K2K_29HZ */
		.name			   = "4k2k29hz",
		.mode			   = VMODE_4K2K_29HZ,
		.width			   = 3840,
		.height		   = 2160,
		.field_height	   = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 2997,
		.sync_duration_den = 100,
		.video_clk		   = 297000000,
	},
#endif
	{ /* VMODE_4K2K_25HZ */
		.name              = "2160p25hz",
		.mode              = VMODE_4K2K_25HZ,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 25,
		.sync_duration_den = 1,
		.video_clk         = 297000000,
	},
	{ /* VMODE_4K2K_24HZ */
		.name              = "2160p24hz",
		.mode              = VMODE_4K2K_24HZ,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 24,
		.sync_duration_den = 1,
		.video_clk         = 297000000,
	},
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	{ /* VMODE_4K2K_23HZ */
		.name			   = "4k2k23hz",
		.mode			   = VMODE_4K2K_23HZ,
		.width			   = 3840,
		.height		   = 2160,
		.field_height	   = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 2397,
		.sync_duration_den = 100,
		.video_clk		   = 297000000,
	},
#endif
	{ /* VMODE_4K2K_SMPTE */
		.name              = "smpte24hz",
		.mode              = VMODE_4K2K_SMPTE,
		.width             = 4096,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 24,
		.sync_duration_den = 1,
		.video_clk         = 297000000,
	},
	{ /* VMODE_4K2K_FAKE_5G */
		.name              = "4k2k5g",
		.mode              = VMODE_4K2K_FAKE_5G,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 495000000,
	},
	{ /* VMODE_4K2K_60HZ_Y420 */
		.name              = "2160p60hz420",
		.mode              = VMODE_4K2K_60HZ_Y420,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_4K2K_60HZ */
		.name              = "2160p60hz",
		.mode              = VMODE_4K2K_60HZ,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_4K1K_100HZ_Y420 */
		.name              = "4k1k100hz420",
		.mode              = VMODE_4K1K_100HZ_Y420,
		.width             = 3840,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 32,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 100,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_4K1K_100HZ */
		.name              = "4k1k100hz",
		.mode              = VMODE_4K1K_100HZ,
		.width             = 3840,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 32,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 100,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_4K1K_120HZ_Y420 */
		.name              = "4k1k120hz420",
		.mode              = VMODE_4K1K_120HZ_Y420,
		.width             = 3840,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 32,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 120,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_4K1K_120HZ */
		.name              = "4k1k120hz",
		.mode              = VMODE_4K1K_120HZ,
		.width             = 3840,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 32,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 120,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_4K05K_200HZ_Y420 */
		.name              = "4k05k200hz420",
		.mode              = VMODE_4K05K_200HZ_Y420,
		.width             = 3840,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 64,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 200,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_4K05K_200HZ */
		.name              = "4k05k200hz",
		.mode              = VMODE_4K05K_200HZ,
		.width             = 3840,
		.height            = 540,
		.field_height      = 540,
		.aspect_ratio_num  = 64,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 200,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_4K05K_240HZ_Y420 */
		.name              = "4k05k240hz420",
		.mode              = VMODE_4K05K_240HZ_Y420,
		.width             = 3840,
		.height            = 540,
		.field_height      = 540,
		.aspect_ratio_num  = 64,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 240,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_4K05K_240HZ */
		.name              = "4k05k240hz",
		.mode              = VMODE_4K05K_240HZ,
		.width             = 3840,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 64,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 240,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_4K2K_50HZ_Y420 */
		.name              = "2160p50hz420",
		.mode              = TVMODE_4K2K_50HZ_Y420,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_4K2K_50HZ */
		.name              = "2160p50hz",
		.mode              = TVMODE_4K2K_50HZ,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 50,
		.sync_duration_den = 1,
		.video_clk         = 594000000,
	},
	{ /* VMODE_vga */
		.name              = "vga",
		.mode              = VMODE_VGA,
		.width             = 640,
		.height            = 480,
		.field_height      = 240,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 25175000,
	},
	{ /* VMODE_SVGA */
		.name              = "svga",
		.mode              = VMODE_SVGA,
		.width             = 800,
		.height            = 600,
		.field_height      = 600,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 40000000,
	},
	{ /* VMODE_XGA */
		.name              = "xga",
		.mode              = VMODE_XGA,
		.width             = 1024,
		.height            = 768,
		.field_height      = 768,
		.aspect_ratio_num  = 4,
		.aspect_ratio_den  = 3,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 65000000,
	},
	{ /* VMODE_sxga */
		.name              = "sxga",
		.mode              = VMODE_SXGA,
		.width             = 1280,
		.height            = 1024,
		.field_height      = 1024,
		.aspect_ratio_num  = 5,
		.aspect_ratio_den  = 4,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 108000000,
	},
	{ /* VMODE_wsxga */
		.name              = "wsxga",
		.mode              = VMODE_WSXGA,
		.width             = 1440,
		.height            = 900,
		.field_height      = 900,
		.aspect_ratio_num  = 8,
		.aspect_ratio_den  = 5,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 88750000,
	},
	{ /* VMODE_fhdvga */
		.name              = "fhdvga",
		.mode              = VMODE_FHDVGA,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60,
		.sync_duration_den = 1,
		.video_clk         = 148500000,
	},
};

#endif
