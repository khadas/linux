/*
 * drivers/amlogic/tvin/hdmirx_ext/hdmiin_attrs.h
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

#ifndef __HDMIIN_DEVICE_ATTRS_H__
#define __HDMIIN_DEVICE_ATTRS_H__

extern int hdmiin_create_device_attrs(struct device *pdevice);
extern void hdmiin_destroy_device_attrs(struct device *pdevice);

#endif
