/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/media/video_processor/video_composer_ext.h
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

#ifndef VIDEO_COMPOSER_EXT_H
#define VIDEO_COMPOSER_EXT_H

#include <linux/amlogic/media/vfm/vframe.h>

struct composer_dst {
	dma_addr_t phy_addr;
	u32 buffer_width;
	u32 buffer_height;
	u32 content_left;
	u32 content_top;
	u32 content_w;
	u32 content_h;
	u32 format;
};

#endif /* VIDEO_COMPOSER_EXT_H */

