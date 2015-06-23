/*
 * drivers/amlogic/display/vout2/tv_vout2.h
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


#ifndef _TV_VOUT2_H_
#define _TV_VOUT2_H_

/* Amlogic Headers */
#include <linux/amlogic/vout/vout_notify.h>

/*****************************************************************
**
**	macro define part
**
******************************************************************/
#define TV_CLASS_NAME	"tv2"
#define	MAX_NUMBER_PARA  10

#define DEFAULT_VDAC_SEQUENCE	0x120120

#define SHOW_INFO(name)      \
	{return snprintf(buf, 40, "%s\n", name); }

#define  STORE_INFO(name)\
	{mutex_lock(&TV_mutex);\
	snprintf(name, 40, "%s", buf) ;\
	mutex_unlock(&TV_mutex); }

#define SET_TV2_CLASS_ATTR(name, op) \
static char name[40]; \
static ssize_t aml_TV_attr_##name##_show(struct class *cla, \
		struct class_attribute *attr, char *buf) \
{ \
	SHOW_INFO(name)	\
} \
static ssize_t aml_TV_attr_##name##_store(struct class *cla, \
		struct class_attribute *attr, const char *buf, size_t count) \
{ \
	STORE_INFO(name); \
	op(name); \
	return strnlen(buf, count); \
} \
struct class_attribute  class_TV2_attr_##name = \
__ATTR(name, S_IRUGO|S_IWUSR, aml_TV_attr_##name##_show, \
		aml_TV_attr_##name##_store);

struct disp_module_info_s {
	unsigned int  major;  /* dev major number */
	const struct vinfo_s *vinfo;
	char	     name[20];
	struct class  *base_class;
};

static  DEFINE_MUTEX(TV_mutex);

/*****************************
*	default settings :
*	Y    -----  DAC1
*	PB  -----  DAC2
*	PR  -----  DAC0
*
*	CVBS	---- DAC1
*	S-LUMA    ---- DAC2
*	S-CHRO	----  DAC0
******************************/

struct vmode_tvmode_tab_s {
	enum tvmode_e tvmode;
	enum vmode_e  mode;
};

enum video_signal_set_e {
	INTERALCE_COMPONENT = 0,
	CVBS_SVIDEO,
	PROGRESSIVE,
	SIGNAL_SET_MAX
};

enum video_signal_type_e {
	VIDEO_SIGNAL_TYPE_INTERLACE_Y = 0, /**< Interlace Y signal */
	VIDEO_SIGNAL_TYPE_CVBS,            /**< CVBS signal */
	VIDEO_SIGNAL_TYPE_SVIDEO_LUMA,     /**< S-Video luma signal */
	VIDEO_SIGNAL_TYPE_SVIDEO_CHROMA,   /**< S-Video chroma signal */
	VIDEO_SIGNAL_TYPE_INTERLACE_PB,    /**< Interlace Pb signal */
	VIDEO_SIGNAL_TYPE_INTERLACE_PR,    /**< Interlace Pr signal */
	VIDEO_SIGNAL_TYPE_INTERLACE_R,     /**< Interlace R signal */
	VIDEO_SIGNAL_TYPE_INTERLACE_G,     /**< Interlace G signal */
	VIDEO_SIGNAL_TYPE_INTERLACE_B,     /**< Interlace B signal */
	VIDEO_SIGNAL_TYPE_PROGRESSIVE_Y,   /**< Progressive Y signal */
	VIDEO_SIGNAL_TYPE_PROGRESSIVE_PB,  /**< Progressive Pb signal */
	VIDEO_SIGNAL_TYPE_PROGRESSIVE_PR,  /**< Progressive Pr signal */
	VIDEO_SIGNAL_TYPE_PROGEESSIVE_R,   /**< Progressive R signal */
	VIDEO_SIGNAL_TYPE_PROGEESSIVE_G,   /**< Progressive G signal */
	VIDEO_SIGNAL_TYPE_PROGEESSIVE_B,   /**< Progressive B signal */
	VIDEO_SIGNAL_TYPE_MAX
};

/*****************************
*	default settings :
*	Y    -----  DAC1
*	PB  -----  DAC2
*	PR  -----  DAC0
*
*	CVBS	---- DAC1
*	S-LUMA    ---- DAC2
*	S-CHRO	----  DAC0
******************************/

static const enum tvmode_e vmode_tvmode_tab[] = {
	TVMODE_480I, TVMODE_480I_RPT, TVMODE_480CVBS, TVMODE_480P,
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	TVMODE_480P_59HZ,
#endif
	TVMODE_480P_RPT, TVMODE_576I, TVMODE_576I_RPT, TVMODE_576CVBS,
	TVMODE_576P, TVMODE_576P_RPT, TVMODE_720P,
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	TVMODE_720P_59HZ , /* for 720p 59.94hz */
#endif
	TVMODE_1080I,
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	TVMODE_1080I_59HZ,
#endif
	TVMODE_1080P,
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	TVMODE_1080P_59HZ , /* for 1080p 59.94hz */
#endif
	TVMODE_720P_50HZ, TVMODE_1080I_50HZ,
	TVMODE_1080P_50HZ, TVMODE_1080P_24HZ,
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	TVMODE_1080P_23HZ , /* for 1080p 23.97hz */
#endif
	TVMODE_4K2K_30HZ,
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	TVMODE_4K2K_29HZ , /* for 4k2k 29.97hz */
#endif
	TVMODE_4K2K_25HZ, TVMODE_4K2K_24HZ,
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	TVMODE_4K2K_23HZ , /* for 4k2k 23.97hz */
#endif
	TVMODE_4K2K_SMPTE,
	TVMODE_VGA, TVMODE_SVGA, TVMODE_XGA, TVMODE_SXGA
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
		.name              = "1080p59hz",
		.mode              = VMODE_1080P_59HZ,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 60000,
		.sync_duration_den = 1001,
		.video_clk         = 148500000,
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
		.name              = "1080p23hz",
		.mode              = VMODE_1080P_23HZ,
		.width             = 1920,
		.height            = 1080,
		.field_height      = 1080,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 2397,
		.sync_duration_den = 100,
		.video_clk         = 74250000,
	},
#endif
	{ /* VMODE_4K2K_30HZ */
		.name              = "2160p30hz",
		.mode              = TVMODE_4K2K_30HZ,
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
		.name              = "4k2k29hz",
		.mode              = TVMODE_4K2K_29HZ,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 2997,
		.sync_duration_den = 100,
		.video_clk         = 297000000,
	},
#endif
	{ /* VMODE_4K2K_25HZ */
		.name              = "2160p25hz",
		.mode              = TVMODE_4K2K_25HZ,
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
		.mode              = TVMODE_4K2K_24HZ,
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
		.name              = "4k2k23hz",
		.mode              = TVMODE_4K2K_23HZ,
		.width             = 3840,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 2397,
		.sync_duration_den = 100,
		.video_clk         = 297000000,
	},
#endif
	{ /* VMODE_4K2K_SMPTE */
		.name              = "smpte24hz",
		.mode              = TVMODE_4K2K_SMPTE,
		.width             = 4096,
		.height            = 2160,
		.field_height      = 2160,
		.aspect_ratio_num  = 16,
		.aspect_ratio_den  = 9,
		.sync_duration_num = 24,
		.sync_duration_den = 1,
		.video_clk         = 297000000,
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



static const unsigned int  signal_set[SIGNAL_SET_MAX][3] = {
	{
		VIDEO_SIGNAL_TYPE_INTERLACE_Y,     /* component interlace */
		VIDEO_SIGNAL_TYPE_INTERLACE_PB,
		VIDEO_SIGNAL_TYPE_INTERLACE_PR,
	},
	{
		VIDEO_SIGNAL_TYPE_CVBS,		/* cvbs&svideo */
		VIDEO_SIGNAL_TYPE_SVIDEO_LUMA,
		VIDEO_SIGNAL_TYPE_SVIDEO_CHROMA,
	},
	{
		VIDEO_SIGNAL_TYPE_PROGRESSIVE_Y,     /* progressive. */
		VIDEO_SIGNAL_TYPE_PROGRESSIVE_PB,
		VIDEO_SIGNAL_TYPE_PROGRESSIVE_PR,
	},
};

static const char * const signal_table[] = {
	"INTERLACE_Y ", /**< Interlace Y signal */
	"CVBS",            /**< CVBS signal */
	"SVIDEO_LUMA",     /**< S-Video luma signal */
	"SVIDEO_CHROMA",   /**< S-Video chroma signal */
	"INTERLACE_PB",    /**< Interlace Pb signal */
	"INTERLACE_PR",    /**< Interlace Pr signal */
	"INTERLACE_R",     /**< Interlace R signal */
	"INTERLACE_G",     /**< Interlace G signal */
	"INTERLACE_B",     /**< Interlace B signal */
	"PROGRESSIVE_Y",   /**< Progressive Y signal */
	"PROGRESSIVE_PB",  /**< Progressive Pb signal */
	"PROGRESSIVE_PR",  /**< Progressive Pr signal */
	"PROGEESSIVE_R",   /**< Progressive R signal */
	"PROGEESSIVE_G",   /**< Progressive G signal */
	"PROGEESSIVE_B",   /**< Progressive B signal */
};

#endif
