/*
 * drivers/framerate_adapter/video_framerate_adaper.h
 *
 * Copyright (C) 2020 Amlogic, Inc. All rights reserved.
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

#ifndef VIDEOFRAMERATEADAPTER_H
#define VIDEOFRAMERATEADAPTER_H

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>


struct frame_rate_dev_s  {
	struct cdev cdev;
	struct device *dev;
	dev_t dev_no;
};

#endif

