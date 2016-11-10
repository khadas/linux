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

#ifndef __AML_PWM_REG_H__
#define __AML_PWM_REG_H__
#include <linux/amlogic/iomap.h>

#define AO_PWM_PWM_B   (0x554)
#define AO_PWM_MISC_REG_AB                                (0x558)
#define AO_RTI_PIN_MUX_REG                                 (0x05 << 2)
#define AO_RTI_PIN_MUX_REG2                                (0x06 << 2)
#define	OFFSET	24

static inline void bl_write_reg(unsigned int addr, unsigned int val)
{
	int ret = 0;
	unsigned int type = (addr >> OFFSET);
	unsigned int reg = addr & ((1 << OFFSET) - 1);
	ret = aml_reg_write(type, reg, val);
	if (ret < 0)
		pr_info("Wr[0x%x]0x%x Error\n", addr, val);
}

static inline unsigned int bl_read_reg(unsigned int addr)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int type = (addr >> OFFSET);
	unsigned int reg = addr & ((1 << OFFSET) - 1);
	ret = aml_reg_read(type, reg, &val);
	if (ret < 0)
		pr_info("Rd[0x%x] Error\n", addr);
	return val;
}

static inline void bl_write_ao_reg(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	aml_write_aobus(reg, ((aml_read_aobus(reg) &
				(~(((1L << _len)-1) << _start))) |
				((value & ((1L << _len)-1)) << _start)));

}

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

