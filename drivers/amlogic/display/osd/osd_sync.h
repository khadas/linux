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

#ifndef _OSD_SYNC_H_
#define _OSD_SYNC_H_

#include <sw_sync.h>
#include <sync.h>

struct fb_sync_request_s {
	unsigned int xoffset;
	unsigned int yoffset;
	int in_fen_fd;
	int out_fen_fd;
};

#endif
