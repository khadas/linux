/*
 * drivers/amlogic/display/backlight/aml_bl_reg.h
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

#ifndef __AML_BL_REG_H__
#define __AML_BL_REG_H__
#include <linux/amlogic/iomap.h>

/* normal pwm reg: cbus */
#define PWM_PWM_A                       0x2154
#define PWM_PWM_B                       0x2155
#define PWM_MISC_REG_AB                 0x2156
#define PWM_PWM_C                       0x2190
#define PWM_PWM_D                       0x2191
#define PWM_MISC_REG_CD                 0x2192
#define PWM_PWM_E                       0x21b0
#define PWM_PWM_F                       0x21b1
#define PWM_MISC_REG_EF                 0x21b2

/* pwm_vs reg: vcbus */
#define VPU_VPU_PWM_V0                  0x2730
#define VPU_VPU_PWM_V1                  0x2731
#define VPU_VPU_PWM_V2                  0x2732
#define VPU_VPU_PWM_V3                  0x2733

#define ENCL_VIDEO_MAX_LNCNT            0x1cbb


static inline unsigned int bl_cbus_read(unsigned int reg)
{
	return aml_read_cbus(reg);
};

static inline void bl_cbus_write(unsigned int reg, unsigned int value)
{
	aml_write_cbus(reg, value);
};

static inline void bl_cbus_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	bl_cbus_write(reg, ((bl_cbus_read(reg) &
				(~(((1L << _len)-1) << _start))) |
				((value & ((1L << _len)-1)) << _start)));

}

static inline unsigned int bl_vcbus_read(unsigned int reg)
{
	return aml_read_vcbus(reg);
};

static inline void bl_vcbus_write(unsigned int reg, unsigned int value)
{
	aml_write_vcbus(reg, value);
};

static inline void bl_vcbus_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	bl_vcbus_write(reg, ((bl_vcbus_read(reg) &
				(~(((1L << _len)-1) << _start))) |
				((value & ((1L << _len)-1)) << _start)));

}

#endif

