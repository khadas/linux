/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AD82587D_H
#define _AD82587D_H

#define MUTE                             0x02
#define MVOL                             0x03
#define C1VOL                            0x04
#define C2VOL                            0x05

#define AD82587D_REGISTER_COUNT          0x25
#define AD82587D_REGISTER_TABLE_SIZE     (AD82587D_REGISTER_COUNT * 2)

struct ad82587d_platform_data {
	int reset_pin;
};

#endif
