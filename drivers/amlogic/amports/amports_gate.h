/*
 * drivers/amlogic/amports/amports_gate.h
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

#ifndef AMPORT_GATE_H
#define AMPORT_GATE_H
#include <linux/device.h>

extern int amports_clock_gate_init(struct device *dev);
extern int amports_switch_gate(const char *name, int enable);

#endif

