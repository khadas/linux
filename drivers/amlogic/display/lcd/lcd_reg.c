/*
 * drivers/amlogic/display/lcd/lcd_reg.c
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
#include <linux/amlogic/vout/lcd_vout.h>
#include "lcd_common.h"
#include "lcd_reg.h"

/* ********************************************* */
#define LCD_REG_GLOBAL_API     0
#define LCD_REG_IOREMAP        1
#define LCD_REG_IF             LCD_REG_IOREMAP
/* ********************************************* */

#if (LCD_REG_IF == LCD_REG_IOREMAP)
struct reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	int flag;
};

#define LCD_MAP_CBUS      0
#define LCD_MAP_VCBUS     1
#define LCD_MAP_HIUBUS    2
#define LCD_MAP_PERIPHS   3
static struct reg_map_s lcd_reg_maps[] = {
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
	{ /* PERIPHS */
		.base_addr = 0xc8834400,
		.size = 0x100,
		.flag = 0,
	},
};

int lcd_ioremap(void)
{
	int i;
	int ret = 0;

	for (i = 0; i < 4; i++) {
		lcd_reg_maps[i].p = ioremap(lcd_reg_maps[i].base_addr,
					lcd_reg_maps[i].size);
		if (lcd_reg_maps[i].p == NULL) {
			lcd_reg_maps[i].flag = 0;
			LCDPR("reg map failed: 0x%x\n",
				lcd_reg_maps[i].base_addr);
			ret = -1;
		} else {
			lcd_reg_maps[i].flag = 1;
			if (lcd_debug_print_flag) {
				LCDPR("reg mapped: 0x%x -> %p\n",
				lcd_reg_maps[i].base_addr, lcd_reg_maps[i].p);
			}
		}
	}
	return ret;
}

static int check_lcd_ioremap(int n)
{
	if (lcd_reg_maps[n].flag == 0) {
		LCDPR("reg 0x%x mapped error\n",
			lcd_reg_maps[n].base_addr);
		return -1;
	}
	return 0;
}
#else
int lcd_ioremap(void)
{
	LCDPR("reg interface is global api\n");
	return 0;
}
#endif

/* ********************************
 * register mapping check
 * ********************************* */
#if (LCD_REG_IF == LCD_REG_IOREMAP)
static inline void __iomem *check_lcd_vcbus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;

	reg_bus = LCD_MAP_VCBUS;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	p = lcd_reg_maps[reg_bus].p + LCD_REG_OFFSET_VCBUS(_reg);
	return p;
}

static inline void __iomem *check_lcd_hiu_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		reg_bus = LCD_MAP_HIUBUS;
	else
		reg_bus = LCD_MAP_CBUS;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	if (reg_bus == LCD_MAP_HIUBUS) {
		_reg = LCD_HIU_REG_GX(_reg);
		p = lcd_reg_maps[reg_bus].p + LCD_REG_OFFSET_HIU(_reg);
	} else {
		p = lcd_reg_maps[reg_bus].p + LCD_REG_OFFSET_CBUS(_reg);
	}
	return p;
}

static inline void __iomem *check_lcd_cbus_reg(unsigned int _reg)
{
	void __iomem *p;

	if (check_lcd_ioremap(LCD_MAP_CBUS))
		return NULL;

	p = lcd_reg_maps[LCD_MAP_CBUS].p + LCD_REG_OFFSET_CBUS(_reg);
	return p;
}

static inline void __iomem *check_lcd_periphs_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB)
		reg_bus = LCD_MAP_PERIPHS;
	else
		reg_bus = LCD_MAP_CBUS;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	if (reg_bus == LCD_MAP_PERIPHS) {
		_reg = LCD_PERIPHS_REG_GX(_reg);
		p = lcd_reg_maps[reg_bus].p + LCD_REG_OFFSET_PERIPHS(_reg);
	} else {
		p = lcd_reg_maps[reg_bus].p + LCD_REG_OFFSET_CBUS(_reg);
	}
	return p;
}
#endif

/* ********************************
 * register access api
 * ********************************* */
#if (LCD_REG_IF == LCD_REG_IOREMAP)
unsigned int lcd_vcbus_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_vcbus_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void lcd_vcbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_vcbus_reg(_reg);
	if (p)
		writel(_value, p);
};
#else
unsigned int lcd_vcbus_read(unsigned int reg)
{
	return aml_read_vcbus(reg);
};

void lcd_vcbus_write(unsigned int reg, unsigned int value)
{
	aml_write_vcbus(reg, value);
};
#endif

void lcd_vcbus_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	lcd_vcbus_write(reg, ((lcd_vcbus_read(reg) &
		(~(((1L << _len)-1) << _start))) |
		((value & ((1L << _len)-1)) << _start)));
}

unsigned int lcd_vcbus_getb(unsigned int reg,
		unsigned int _start, unsigned int _len)
{
	return (lcd_vcbus_read(reg) >> _start) & ((1L << _len)-1);
}

void lcd_vcbus_set_mask(unsigned int reg, unsigned int _mask)
{
	lcd_vcbus_write(reg, (lcd_vcbus_read(reg) | (_mask)));
}

void lcd_vcbus_clr_mask(unsigned int reg, unsigned int _mask)
{
	lcd_vcbus_write(reg, (lcd_vcbus_read(reg) & (~(_mask))));
}

#if (LCD_REG_IF == LCD_REG_IOREMAP)
unsigned int lcd_hiu_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_hiu_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void lcd_hiu_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_hiu_reg(_reg);
	if (p)
		writel(_value, p);
};
#else
unsigned int lcd_hiu_read(unsigned int _reg)
{
	return aml_read_cbus(_reg);
};

void lcd_hiu_write(unsigned int _reg, unsigned int _value)
{
	aml_write_cbus(_reg, _value);
};
#endif

void lcd_hiu_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	lcd_hiu_write(_reg, ((lcd_hiu_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

unsigned int lcd_hiu_getb(unsigned int _reg,
		unsigned int _start, unsigned int _len)
{
	return (lcd_hiu_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void lcd_hiu_set_mask(unsigned int _reg, unsigned int _mask)
{
	lcd_hiu_write(_reg, (lcd_hiu_read(_reg) | (_mask)));
}

void lcd_hiu_clr_mask(unsigned int _reg, unsigned int _mask)
{
	lcd_hiu_write(_reg, (lcd_hiu_read(_reg) & (~(_mask))));
}

#if (LCD_REG_IF == LCD_REG_IOREMAP)
unsigned int lcd_cbus_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_cbus_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void lcd_cbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_cbus_reg(_reg);
	if (p)
		writel(_value, p);
};
#else
unsigned int lcd_cbus_read(unsigned int _reg)
{
	return aml_read_cbus(_reg);
};

void lcd_cbus_write(unsigned int _reg, unsigned int _value)
{
	aml_write_cbus(_reg, _value);
};
#endif

void lcd_cbus_setb(unsigned int _reg, unsigned int _value,
		unsigned int _start, unsigned int _len)
{
	lcd_cbus_write(_reg, ((lcd_cbus_read(_reg) &
			~(((1L << (_len))-1) << (_start))) |
			(((_value)&((1L<<(_len))-1)) << (_start))));
}

#if (LCD_REG_IF == LCD_REG_IOREMAP)
unsigned int lcd_periphs_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_periphs_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void lcd_periphs_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_periphs_reg(_reg);
	if (p)
		writel(_value, p);
};
#else
unsigned int lcd_periphs_read(unsigned int _reg)
{
	return aml_read_cbus(_reg);
};

void lcd_periphs_write(unsigned int _reg, unsigned int _value)
{
	aml_write_cbus(_reg, _value);
};
#endif

void lcd_pinmux_set_mask(unsigned int n, unsigned int _mask)
{
	unsigned int _reg = PERIPHS_PIN_MUX_0;

	_reg += n;
	lcd_periphs_write(_reg, (lcd_periphs_read(_reg) | (_mask)));
}

void lcd_pinmux_clr_mask(unsigned int n, unsigned int _mask)
{
	unsigned int _reg = PERIPHS_PIN_MUX_0;

	_reg += n;
	lcd_periphs_write(_reg, (lcd_periphs_read(_reg) & (~(_mask))));
}
