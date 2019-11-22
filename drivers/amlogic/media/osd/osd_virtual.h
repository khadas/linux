/*
 * drivers/amlogic/media/osd/osd_virtual.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifndef _OSD_VIRTUAL_H_
#define _OSD_VIRTUAL_H_

/* Linux Headers */
#include <linux/list.h>
#include <linux/fb.h>
#include <linux/types.h>

/* Local Headers */
#include "osd.h"

struct fb_virtual_dev_s {
	struct mutex lock;
	struct fb_info *fb_info;
	struct platform_device *dev;
	phys_addr_t fb_mem_paddr;
	void __iomem *fb_mem_vaddr;
	u32 fb_len;
	struct timer_list timer;
	struct completion fb_com;
	struct completion timer_com;
	struct completion post_com;
	struct completion pan_display_com;
	struct task_struct *fb_thread;
	int fb_monitor_run;
	const struct color_bit_define_s *color;
	u32 fb_index;
	u32 open_count;
};

struct virt_fb_para_s {
	struct pandata_s pandata;
	ulong screen_base_paddr;
	void *screen_base_vaddr;
	ulong screen_size;
	u32 stride;
	u32 offset;
	u32 size;
	u32 osd_fps;
	u32 osd_fps_start;
};

int amlfb_virtual_probe(struct platform_device *pdev);
void amlfb_virtual_remove(struct platform_device *pdev);
#endif
