/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _AD82584F_H
#define _AD82584F_H

#define MUTE                             0x02
#define MVOL                             0x03
#define C1VOL                            0x04
#define C2VOL                            0x05
#define CTRL4                            0x0c
#define CH1_CONFIG                       0x0d
#define CH2_CONFIG                       0x0e
#define CH3_CONFIG                       0x0f
#define CH4_CONFIG                       0x10
#define CH5_CONFIG                       0x11
#define CH6_CONFIG                       0x12
#define CH7_CONFIG                       0x13
#define CH8_CONFIG                       0x14
#define CFADDR                           0x1d
#define A1CF1                            0x1e
#define A1CF2                            0x1f
#define A1CF3                            0x20
#define CFUD                             0x2d

#define AD82584F_REGISTER_COUNT          0x86
#define AD82584F_REGISTER_TABLE_SIZE     0x10c
#define AD82584F_RAM_TABLE_COUNT         0x80
#define AD82584F_RAM_TABLE_SIZE          0x200

struct ad82584f_platform_data {
	int reset_pin;
	bool no_mclk;
};

#endif
