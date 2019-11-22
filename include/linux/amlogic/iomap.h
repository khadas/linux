/*
 * include/linux/amlogic/iomap.h
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

#ifndef __SOC_IO_H
#define __SOC_IO_H

enum{
	IO_CBUS_BASE = 0,
	IO_APB_BUS_BASE,
	IO_AOBUS_BASE,
	IO_VAPB_BUS_BASE,
	IO_HIUBUS_BASE,
	IO_BUS_MAX,
};

extern inline int aml_reg_read(u32 bus_type, u32 reg, u32 *val);
extern inline int aml_reg_write(u32 bus_type, u32 reg, u32 val);
extern int aml_regmap_update_bits(u32 bus_type,
			unsigned int reg, unsigned int mask,
			unsigned int val);
/*
 ** CBUS REG Read Write and Update some bits
 */
extern  int aml_read_cbus(unsigned int reg);


extern   void aml_write_cbus(unsigned int reg, unsigned int val);


extern  void aml_cbus_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val);

/*
 ** AO REG Read Write and Update some bits
 */
extern  int aml_read_aobus(unsigned int reg);


extern  void aml_write_aobus(unsigned int reg, unsigned int val);


extern  void aml_aobus_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val);



/*
 ** VCBUS Bus REG Read Write and Update some bits
 */
extern  int aml_read_vcbus(unsigned int reg);

extern  void aml_write_vcbus(unsigned int reg, unsigned int val);

extern  void aml_vcbus_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val);


/*
 ** DOS BUS Bus REG Read Write and Update some bits
 */
extern  int aml_read_dosbus(unsigned int reg);

extern  void aml_write_dosbus(unsigned int reg, unsigned int val);

extern  void aml_dosbus_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val);

extern int  aml_read_sec_reg(unsigned int reg);
extern void  aml_write_sec_reg(unsigned int reg, unsigned int val);

/*
 ** HIUBUS REG Read Write and Update some bits
 */
extern  int aml_read_hiubus(unsigned int reg);


extern  void aml_write_hiubus(unsigned int reg, unsigned int val);


extern  void aml_hiubus_update_bits(unsigned int reg,
		unsigned int mask, unsigned int val);

#include <linux/io.h>
extern void __iomem *vpp_base;
extern uint vpp_max;

static inline int aml_reg_vcbus_invalid(unsigned int reg)
{
	return !(vpp_base && (vpp_max >= reg));
}

static inline int aml_read_vcbus_s(unsigned int reg)
{
	return readl((vpp_base + (reg << 2)));
}

static inline void aml_write_vcbus_s(unsigned int reg, unsigned int val)
{
	writel(val, (vpp_base + (reg << 2)));
}

static inline void aml_vcbus_update_bits_s(unsigned int reg, unsigned int value,
					   unsigned int start, unsigned int len)
{
	unsigned int tmp, orig;
	unsigned int mask = (((1L << len) - 1) << start);
	int r = (reg << 2);

	orig =  readl((vpp_base + r));
	tmp = orig  & ~mask;
	tmp |= (value << start) & mask;
	writel(tmp, (vpp_base + r));
}

#endif
