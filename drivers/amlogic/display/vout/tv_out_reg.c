/*
 * drivers/amlogic/display/vout/tv_out_reg.c
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

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/cpu_version.h>
#include "tv_out_reg.h"

struct reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	int flag;
};

#define TV_OUT_MAP_CBUS      0
#define TV_OUT_MAP_VCBUS     1
#define TV_OUT_MAP_HIUBUS    2
static struct reg_map_s tv_out_reg_maps[] = {
	{ /* CBUS */
		.base_addr = 0xc1100000,
		.size = 0x8000,
		.flag = 0,
	},
	{ /* VCBUS */
		.base_addr = 0xd0100000,
		.size = 0x10000,
		.flag = 0,
	},
	{ /* HIU */
		.base_addr = 0xc883c000,
		.size = 0x400,
		.flag = 0,
	},
};

int tv_out_ioremap(void)
{
	int i;
	int ret = 0;

	for (i = 0; i < 3; i++) {
		tv_out_reg_maps[i].p = ioremap(tv_out_reg_maps[i].base_addr,
					tv_out_reg_maps[i].size);
		if (tv_out_reg_maps[i].p == NULL) {
			tv_out_reg_maps[i].flag = 0;
			pr_info("tv_out reg map failed: 0x%x\n",
				tv_out_reg_maps[i].base_addr);
			ret = -1;
		} else {
			tv_out_reg_maps[i].flag = 1;
			/* pr_info("tv_out reg mapped: 0x%x -> %p\n",
			tv_out_reg_maps[i].base_addr, tv_out_reg_maps[i].p); */
		}
	}
	return ret;
}

static int check_tv_out_ioremap(int n)
{
	if (tv_out_reg_maps[n].flag == 0) {
		pr_info("tv_out reg 0x%x mapped error\n",
			tv_out_reg_maps[n].base_addr);
		return -1;
	}
	return 0;
}

/* ********************************
 * register access api
 * ********************************* */
/* normal: vcbus or cbus */
static inline void __iomem *check_tv_out_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_M6)
		reg_bus = TV_OUT_MAP_CBUS;
	else
		reg_bus = TV_OUT_MAP_VCBUS;
	if (check_tv_out_ioremap(reg_bus))
		return NULL;

	if (reg_bus == TV_OUT_MAP_CBUS)
		p = tv_out_reg_maps[reg_bus].p + TV_OUT_REG_OFFSET_CBUS(_reg);
	else
		p = tv_out_reg_maps[reg_bus].p + TV_OUT_REG_OFFSET_VCBUS(_reg);
	return p;
}

unsigned int tv_out_reg_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_tv_out_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void tv_out_reg_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_tv_out_reg(_reg);
	if (p)
		writel(_value, p);
};

void tv_out_reg_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	tv_out_reg_write(reg, ((tv_out_reg_read(reg) &
		(~(((1L << _len)-1) << _start))) |
		((value & ((1L << _len)-1)) << _start)));
}

unsigned int tv_out_reg_getb(unsigned int reg,
		unsigned int _start, unsigned int _len)
{
	return (tv_out_reg_read(reg) >> _start) & ((1L << _len)-1);
}

void tv_out_reg_set_mask(unsigned int reg, unsigned int _mask)
{
	tv_out_reg_write(reg, (tv_out_reg_read(reg) | (_mask)));
}

void tv_out_reg_clr_mask(unsigned int reg, unsigned int _mask)
{
	tv_out_reg_write(reg, (tv_out_reg_read(reg) & (~(_mask))));
}

/* hiu_bus */
static inline void __iomem *check_tv_out_hiu_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		reg_bus = TV_OUT_MAP_HIUBUS;
	else
		reg_bus = TV_OUT_MAP_CBUS;
	if (check_tv_out_ioremap(reg_bus))
		return NULL;

	if (reg_bus == TV_OUT_MAP_HIUBUS) {
		_reg = TV_OUT_HIU_REG_GX(_reg);
		p = tv_out_reg_maps[reg_bus].p + TV_OUT_REG_OFFSET_HIU(_reg);
	} else {
		p = tv_out_reg_maps[reg_bus].p + TV_OUT_REG_OFFSET_CBUS(_reg);
	}
	return p;
}

unsigned int tv_out_hiu_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_tv_out_hiu_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void tv_out_hiu_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_tv_out_hiu_reg(_reg);
	if (p)
		writel(_value, p);
};

void tv_out_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	tv_out_hiu_write(_reg, ((tv_out_hiu_read(_reg) &
		(~(((1L << _len)-1) << _start))) |
		((_value & ((1L << _len)-1)) << _start)));
}

unsigned int tv_out_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (tv_out_hiu_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void tv_out_hiu_set_mask(unsigned int _reg, unsigned int _mask)
{
	tv_out_hiu_write(_reg, (tv_out_hiu_read(_reg) | (_mask)));
}

void tv_out_hiu_clr_mask(unsigned int _reg, unsigned int _mask)
{
	tv_out_hiu_write(_reg, (tv_out_hiu_read(_reg) & (~(_mask))));
}

/* cbus */
static inline void __iomem *check_tv_out_cbus_reg(unsigned int _reg)
{
	void __iomem *p;

	if (check_tv_out_ioremap(TV_OUT_MAP_CBUS))
		return NULL;

	p = tv_out_reg_maps[TV_OUT_MAP_CBUS].p + TV_OUT_REG_OFFSET_CBUS(_reg);
	return p;
}

unsigned int tv_out_cbus_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_tv_out_cbus_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void tv_out_cbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_tv_out_cbus_reg(_reg);
	if (p)
		writel(_value, p);
};

void tv_out_cbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	tv_out_cbus_write(_reg, ((tv_out_cbus_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

unsigned int tv_out_cbus_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (tv_out_cbus_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void tv_out_cbus_set_mask(unsigned int _reg, unsigned int _mask)
{
	tv_out_cbus_write(_reg, (tv_out_cbus_read(_reg) | (_mask)));
}

void tv_out_cbus_clr_mask(unsigned int _reg, unsigned int _mask)
{
	tv_out_cbus_write(_reg, (tv_out_cbus_read(_reg) & (~(_mask))));
}

