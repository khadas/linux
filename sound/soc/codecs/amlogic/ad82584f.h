/*
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

#ifndef _AD82584F_H
#define _AD82584F_H

#define MUTE                             0x02
#define MVOL                             0x03
#define C1VOL                            0x04
#define C2VOL                            0x05
#define CFADDR                           0x1d
#define A1CF1                            0x1e
#define A1CF2                            0x1f
#define A1CF3                            0x20
#define CFUD                             0x2d

#define AD82584F_REGISTER_COUNT		 0x86
#define AD82584F_RAM_TABLE_COUNT         0x80

struct ad82584f_platform_data {
	int reset_pin;
	bool no_mclk;
};

#endif
