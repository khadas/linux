/*
 * include/linux/amlogic/pixel_probe.h
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

#ifndef __PIXEL_PROBE_H__
#define __PIXEL_PROBE_H__

int vpp_probe_enable(void);
int vpp_probe_disable(void);
int vdin_probe_enable(void);
int vdin_probe_disable(void);

#endif
