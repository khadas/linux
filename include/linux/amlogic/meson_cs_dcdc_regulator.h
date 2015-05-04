/*
 * include/linux/amlogic/meson_cs_dcdc_regulator.h
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

#ifndef MESON_CS_DCDC_REGULATOR_H
#define MESON_CS_DCDC_REGULATOR_H
#include <linux/regulator/driver.h>

#define P_VGHL_PWM_REG0			(0x21e1 << 2)
#define P_PWM_PWM_A			(0x2154 << 2)
#define P_PWM_PWM_B			(0x2155 << 2)
#define P_PWM_MISC_REG_AB		(0x2156 << 2)
#define P_PWM_PWM_C			(0x2194 << 2)
#define P_PWM_PWM_D			(0x2195 << 2)
#define P_PWM_MISC_REG_CD		(0x2196 << 2)
#define P_PWM_PWM_E			(0x21b0 << 2)
#define P_PWM_PWM_F			(0x21b1 << 2)
#define P_PWM_MISC_REG_EF		(0x21b2 << 2)

#define MESON_CS_MAX_STEPS 16

struct meson_cs_pdata_t {
	struct regulator_init_data *meson_cs_init_data;
	int voltage_step_table[MESON_CS_MAX_STEPS];
	int default_uV;
	int (*get_voltage)(void);
	int (*set_voltage)(unsigned int);
};


struct meson_cs_regulator_dev {
	struct regulator *regulator;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	int min_uV;
	int max_uV;
	int cur_uV;
	struct mutex io_lock;
	int *voltage_step_table;
};

int meson_cs_dcdc_set_voltage_global(int minuV, int maxuV);
int meson_cs_dcdc_get_voltage_global(void);

#endif
