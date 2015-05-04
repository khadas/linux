/*
 * drivers/amlogic/cpufreq/voltage.h
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


#ifndef __ARCH_ARM_MESON6_VOLTAGE_H
#define __ARCH_ARM_MESON6_VOLTAGE_H

struct meson_opp {
	/* in kHz */
	unsigned int freq;
	unsigned int min_uV;
	unsigned int max_uV;
};

extern struct meson_opp meson_vcck_opp_table[];
extern const int meson_vcck_opp_table_size;

extern unsigned int meson_vcck_cur_max_freq(
						struct regulator *reg,
						struct meson_opp *table,
						int tablesize);
extern int meson_vcck_scale(struct regulator *reg,
				struct meson_opp *table,
				int tablesize, unsigned int frequency);

#endif
