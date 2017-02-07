/*
 * drivers/amlogic/display/osd/osd_sync.h
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


#ifndef _OSD_SYNC_H_
#define _OSD_SYNC_H_

struct fb_sync_request_s {
	unsigned int xoffset;
	unsigned int yoffset;
	int in_fen_fd;
	int out_fen_fd;
};

struct fb_sync_request_render_s {
	unsigned int    xoffset;
	unsigned int    yoffset;
	int             in_fen_fd;
	int             out_fen_fd;
	int             width;
	int             height;
	int             format;
	int             shared_fd;
	u32             op;
	u32             reserve;
};
#endif
