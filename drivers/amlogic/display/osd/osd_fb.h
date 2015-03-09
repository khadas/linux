/*
 * Amlogic Frame buffer driver
 *
 * Copyright (C) 2015 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author: Amlogic Platform-BJ <platform.bj@amlogic.com>
 *
 */

#ifndef _OSD_FB_H_
#define _OSD_FB_H_

/* Linux Headers */
#include <linux/list.h>
#include <linux/fb.h>

/* Amlogic Headers */
#include <linux/amlogic/vout/vinfo.h>

/* Local Headers */
#include "osd.h"

#ifdef CONFIG_FB_OSD2_ENABLE
#define OSD_COUNT 2 /* enable two OSD layers */
#else
#define OSD_COUNT 1 /* only enable one OSD layer */
#endif

#define INVALID_BPP_ITEM {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
static const struct color_bit_define_s default_color_format_array[] = {
	INVALID_BPP_ITEM,
	INVALID_BPP_ITEM,
	{
		COLOR_INDEX_02_PAL4, 0, 0,
		0, 2, 0, 0, 2, 0, 0, 2, 0, 0, 0, 0,
		FB_VISUAL_PSEUDOCOLOR, 2,
	},
	INVALID_BPP_ITEM,
	{
		COLOR_INDEX_04_PAL16, 0, 1,
		0, 4, 0, 0, 4, 0, 0, 4, 0, 0, 0, 0,
		FB_VISUAL_PSEUDOCOLOR, 4,
	},
	INVALID_BPP_ITEM,
	INVALID_BPP_ITEM,
	INVALID_BPP_ITEM,
	{
		COLOR_INDEX_08_PAL256, 0, 2,
		0, 8, 0, 0, 8, 0, 0, 8, 0, 0, 0, 0,
		FB_VISUAL_PSEUDOCOLOR, 8,
	},
	/*16 bit color*/
	{
		COLOR_INDEX_16_655, 0, 4,
		10, 6, 0, 5, 5, 0, 0, 5, 0, 0, 0, 0,
		FB_VISUAL_TRUECOLOR, 16
	},
	{
		COLOR_INDEX_16_844, 1, 4,
		8, 8, 0, 4, 4, 0, 0, 4, 0, 0, 0, 0,
		FB_VISUAL_TRUECOLOR, 16
	},
	{
		COLOR_INDEX_16_6442, 2, 4,
		10, 6, 0, 6, 4, 0, 2, 4, 0, 0, 2, 0,
		FB_VISUAL_TRUECOLOR, 16
	},
	{
		COLOR_INDEX_16_4444_R, 3, 4,
		12, 4, 0, 8, 4, 0, 4, 4, 0, 0, 4, 0,
		FB_VISUAL_TRUECOLOR, 16
	},
	{
		COLOR_INDEX_16_4642_R, 7, 4,
		12, 4, 0, 6, 6, 0, 2, 4, 0, 0, 2, 0,
		FB_VISUAL_TRUECOLOR, 16
	},
	{
		COLOR_INDEX_16_1555_A, 6, 4,
		10, 5, 0, 5, 5, 0, 0, 5, 0, 15, 1, 0,
		FB_VISUAL_TRUECOLOR, 16
	},
	{
		COLOR_INDEX_16_4444_A, 5, 4,
		8, 4, 0, 4, 4, 0, 0, 4, 0, 12, 4, 0,
		FB_VISUAL_TRUECOLOR, 16
	},
	{
		COLOR_INDEX_16_565, 4, 4,
		11, 5, 0, 5, 6, 0, 0, 5, 0, 0, 0, 0,
		FB_VISUAL_TRUECOLOR, 16
	},
	/*24 bit color*/
	INVALID_BPP_ITEM,
	INVALID_BPP_ITEM,
	{
		COLOR_INDEX_24_6666_A, 4, 7,
		12, 6, 0, 6, 6, 0, 0, 6, 0, 18, 6, 0,
		FB_VISUAL_TRUECOLOR, 24
	},
	{
		COLOR_INDEX_24_6666_R, 3, 7,
		18, 6, 0, 12, 6, 0, 6, 6, 0, 0, 6, 0,
		FB_VISUAL_TRUECOLOR, 24
	},
	{
		COLOR_INDEX_24_8565, 2, 7,
		11, 5, 0, 5, 6, 0, 0, 5, 0, 16, 8, 0,
		FB_VISUAL_TRUECOLOR, 24
	},
	{
		COLOR_INDEX_24_5658, 1, 7,
		19, 5, 0, 13, 6, 0, 8, 5, 0, 0, 8, 0,
		FB_VISUAL_TRUECOLOR, 24
	},
	{
		COLOR_INDEX_24_888_B, 5, 7,
		0, 8, 0, 8, 8, 0, 16, 8, 0, 0, 0, 0,
		FB_VISUAL_TRUECOLOR, 24
	},
	{
		COLOR_INDEX_24_RGB, 0, 7,
		16, 8, 0, 8, 8, 0, 0, 8, 0, 0, 0, 0,
		FB_VISUAL_TRUECOLOR, 24
	},
	/*32 bit color*/
	INVALID_BPP_ITEM,
	INVALID_BPP_ITEM,
	INVALID_BPP_ITEM,
	INVALID_BPP_ITEM,
	{
		COLOR_INDEX_32_BGRA, 3, 5,
		8, 8, 0, 16, 8, 0, 24, 8, 0, 0, 8, 0,
		FB_VISUAL_TRUECOLOR, 32
	},
	{
		COLOR_INDEX_32_ABGR, 2, 5,
		0, 8, 0, 8, 8, 0, 16, 8, 0, 24, 8, 0,
		FB_VISUAL_TRUECOLOR, 32
	},
	{
		COLOR_INDEX_32_RGBA, 0, 5,
		24, 8, 0, 16, 8, 0, 8, 8, 0, 0, 8, 0,
		FB_VISUAL_TRUECOLOR, 32
	},
	{
		COLOR_INDEX_32_ARGB, 1, 5,
		16, 8, 0, 8, 8, 0, 0, 8, 0, 24, 8, 0,
		FB_VISUAL_TRUECOLOR, 32
	},
	/*YUV color*/
	{COLOR_INDEX_YUV_422, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16},
};

static __u32 var_screeninfo[5];

static struct fb_var_screeninfo fb_def_var[] = {
	{
		.xres            = 1200,
		.yres            = 690,
		.xres_virtual    = 1200,
		.yres_virtual    = 1380,
		.xoffset         = 0,
		.yoffset         = 0,
		.bits_per_pixel = 16,
		.grayscale       = 0,
		.red             = {0, 0, 0},
		.green           = {0, 0, 0},
		.blue            = {0, 0, 0},
		.transp          = {0, 0, 0},
		.nonstd          = 0,
		.activate        = FB_ACTIVATE_NOW,
		.height          = -1,
		.width           = -1,
		.accel_flags     = 0,
		.pixclock        = 0,
		.left_margin     = 0,
		.right_margin    = 0,
		.upper_margin    = 0,
		.lower_margin    = 0,
		.hsync_len       = 0,
		.vsync_len       = 0,
		.sync            = 0,
		.vmode           = FB_VMODE_NONINTERLACED,
		.rotate          = 0,
	}
#ifdef CONFIG_FB_OSD2_ENABLE
	,
	{
		.xres            = 32,
		.yres            = 32,
		.xres_virtual    = 32,
		.yres_virtual    = 32,
		.xoffset         = 0,
		.yoffset         = 0,
		.bits_per_pixel = 32,
		.grayscale       = 0,
		/* leave as it is ,set by system. */
		.red             = {0, 0, 0},
		.green           = {0, 0, 0},
		.blue            = {0, 0, 0},
		.transp          = {0, 0, 0},
		.nonstd          = 0,
		.activate        = FB_ACTIVATE_NOW,
		.height          = -1,
		.width           = -1,
		.accel_flags     = 0,
		.pixclock        = 0,
		.left_margin     = 0,
		.right_margin    = 0,
		.upper_margin    = 0,
		.lower_margin    = 0,
		.hsync_len       = 0,
		.vsync_len       = 0,
		.sync            = 0,
		.vmode           = FB_VMODE_NONINTERLACED,
		.rotate          = 0,
	}
#endif
};

static struct fb_fix_screeninfo fb_def_fix = {
	.id         = "OSD FB",
	.xpanstep   = 1,
	.ypanstep   = 1,
	.type       = FB_TYPE_PACKED_PIXELS,
	.visual     = FB_VISUAL_TRUECOLOR,
	.accel      = FB_ACCEL_NONE,
};

struct osd_fb_dev_s {
	struct mutex lock;
	struct fb_info *fb_info;
	struct platform_device *dev;
	u32 fb_mem_paddr;
	void __iomem *fb_mem_vaddr;
	u32 fb_len;
	const struct color_bit_define_s *color;
	enum vmode_e vmode;
	struct osd_ctl_s osd_ctl;
	u32 order;
	u32 scale;
	u32 enable_3d;
	u32 preblend_enable;
	u32 enable_key_flag;
	u32 color_key;
};

#define OSD_INVALID_INFO        0xffffffff
#define OSD_FIRST_GROUP_START   1
#define OSD_SECOND_GROUP_START  4
#define OSD_END                 5

#endif
