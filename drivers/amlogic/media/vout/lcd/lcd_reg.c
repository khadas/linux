// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/vclk_serve.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "lcd_common.h"
#include "lcd_reg.h"

#define LCD_MAP_HIU	  0
#define LCD_MAP_VCBUS	  1
#define LCD_MAP_CBUS	  2
#define LCD_MAP_PERIPHS   3
#define LCD_MAP_DSI_HOST  4
#define LCD_MAP_DSI_PHY   5
#define LCD_MAP_TCON      6
#define LCD_MAP_MAX       7

int lcd_reg_g12a[] = {
	LCD_MAP_HIU,
	LCD_MAP_VCBUS,
	LCD_MAP_CBUS,
	LCD_MAP_DSI_HOST,
	LCD_MAP_DSI_PHY,
	LCD_MAP_MAX,
};

int lcd_reg_tl1[] = {
	LCD_MAP_HIU,
	LCD_MAP_VCBUS,
	LCD_MAP_CBUS,
	LCD_MAP_TCON,
	LCD_MAP_PERIPHS,
	LCD_MAP_MAX,
};

struct lcd_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

static struct lcd_reg_map_s *lcd_reg_map;

int lcd_ioremap(struct platform_device *pdev)
{
	int i = 0;
	int *table;
	struct resource *res;
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	lcd_reg_map = kcalloc(LCD_MAP_MAX,
			      sizeof(struct lcd_reg_map_s), GFP_KERNEL);
	if (!lcd_reg_map) {
		LCDERR("%s: lcd_reg_map buf malloc error\n", __func__);
		return -1;
	}
	table = lcd_drv->data->reg_map_table;
	while (i < LCD_MAP_MAX) {
		if (table[i] == LCD_MAP_MAX)
			break;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			LCDERR("%s: lcd_reg resource get error\n", __func__);
			kfree(lcd_reg_map);
			lcd_reg_map = NULL;
			return -1;
		}
		lcd_reg_map[table[i]].base_addr = res->start;
		lcd_reg_map[table[i]].size = resource_size(res);
		lcd_reg_map[table[i]].p = devm_ioremap_nocache(&pdev->dev,
			res->start, lcd_reg_map[table[i]].size);
		if (!lcd_reg_map[table[i]].p) {
			lcd_reg_map[table[i]].flag = 0;
			LCDERR("%s: reg map failed: 0x%x\n",
			       __func__,
			       lcd_reg_map[table[i]].base_addr);
			kfree(lcd_reg_map);
			lcd_reg_map = NULL;
			return -1;
		} else {
			lcd_reg_map[table[i]].flag = 1;
			if (lcd_debug_print_flag) {
				LCDPR("%s: reg mapped: 0x%x -> %p\n",
				      __func__,
				      lcd_reg_map[table[i]].base_addr,
				      lcd_reg_map[table[i]].p);
			}
		}
		i++;
	}

	return 0;
}

static int check_lcd_ioremap(int n)
{
	if (!lcd_reg_map)
		return -1;
	if (n >= LCD_MAP_MAX)
		return -1;
	if (lcd_reg_map[n].flag == 0) {
		LCDERR("reg 0x%x mapped error\n", lcd_reg_map[n].base_addr);
		return -1;
	}
	return 0;
}

static inline void __iomem *check_lcd_hiu_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_base, reg_offset;

	reg_bus = LCD_MAP_HIU;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_base = lcd_reg_map[reg_bus].base_addr - LCD_HIU_BASE;
	reg_offset = (_reg << 2) - reg_base;

	if (reg_offset >= lcd_reg_map[reg_bus].size) {
		LCDERR("invalid hiu reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_vcbus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_base, reg_offset;

	reg_bus = LCD_MAP_VCBUS;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_base = lcd_reg_map[reg_bus].base_addr - LCD_VCBUS_BASE;
	reg_offset = (_reg << 2) - reg_base;

	if (reg_offset >= lcd_reg_map[reg_bus].size) {
		LCDERR("invalid vcbus reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_cbus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_base, reg_offset;

	reg_bus = LCD_MAP_CBUS;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_base = lcd_reg_map[reg_bus].base_addr - LCD_CBUS_BASE;
	reg_offset = (_reg << 2) - reg_base;

	if (reg_offset >= lcd_reg_map[reg_bus].size) {
		LCDERR("invalid cbus reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_periphs_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_PERIPHS;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET(_reg);

	if (reg_offset >= lcd_reg_map[reg_bus].size) {
		LCDERR("invalid periphs reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_dsi_host_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_DSI_HOST;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET_MIPI_HOST(_reg);
	if (reg_offset >= lcd_reg_map[reg_bus].size) {
		LCDERR("invalid dsi_host reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_dsi_phy_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_DSI_PHY;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET(_reg);
	if (reg_offset >= lcd_reg_map[reg_bus].size) {
		LCDERR("invalid dsi_phy reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_tcon_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_TCON;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET(_reg);
	if (reg_offset >= lcd_reg_map[reg_bus].size) {
		LCDERR("invalid tcon reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_tcon_reg_byte(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_TCON;
	if (check_lcd_ioremap(reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET_BYTE(_reg);
	if (reg_offset >= lcd_reg_map[reg_bus].size) {
		LCDERR("invalid tcon reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = lcd_reg_map[reg_bus].p + reg_offset;
	return p;
}

unsigned int lcd_vcbus_read(unsigned int reg)
{
#ifdef CONFIG_AMLOGIC_VPU
	return vpu_vcbus_read(reg);
#else
	return aml_read_vcbus(reg);
#endif
};

void lcd_vcbus_write(unsigned int reg, unsigned int value)
{
#ifdef CONFIG_AMLOGIC_VPU
	vpu_vcbus_write(reg, value);
#else
	aml_write_vcbus(reg, value);
#endif
};

int lcd_regmap_update_bits(unsigned int bus_type,
			   unsigned int reg,
			   unsigned int mask,
			   unsigned int val)
{
	unsigned int orig;
	unsigned int tmp;

	if (check_lcd_ioremap(bus_type))
		return -1;

	orig = readl(lcd_reg_map[bus_type].p + reg);

	tmp = orig & ~mask;
	tmp |= val & mask;
	writel(tmp, (lcd_reg_map[bus_type].p + reg));

	return 0;
}

void lcd_vcbus_update_bits(unsigned int reg,
			   unsigned int mask,
			   unsigned int val)
{
	int ret;

	ret = lcd_regmap_update_bits(LCD_VCBUS_BASE,
				     reg << 2, mask, val);
	if (ret)
		LCDERR("write vcbus reg %x error %d\n", reg, ret);
}

void lcd_vcbus_setb(unsigned int reg, unsigned int value,
		    unsigned int _start, unsigned int _len)
{
	lcd_vcbus_write(reg, ((lcd_vcbus_read(reg) &
		(~(((1L << _len) - 1) << _start))) |
		((value & ((1L << _len) - 1)) << _start)));
}

unsigned int lcd_vcbus_getb(unsigned int reg,
			    unsigned int _start, unsigned int _len)
{
	return (lcd_vcbus_read(reg) >> _start) & ((1L << _len) - 1);
}

void lcd_vcbus_set_mask(unsigned int reg, unsigned int _mask)
{
	lcd_vcbus_write(reg, (lcd_vcbus_read(reg) | (_mask)));
}

void lcd_vcbus_clr_mask(unsigned int reg, unsigned int _mask)
{
	lcd_vcbus_write(reg, (lcd_vcbus_read(reg) & (~(_mask))));
}

unsigned int lcd_hiu_read(unsigned int _reg)
{
	return vclk_clk_reg_read(_reg);
};

void lcd_hiu_write(unsigned int _reg, unsigned int _value)
{
	vclk_clk_reg_write(_reg, _value);
};

void lcd_hiu_setb(unsigned int _reg, unsigned int _value,
		  unsigned int _start, unsigned int _len)
{
	lcd_hiu_write(_reg, ((lcd_hiu_read(_reg) &
		      ~(((1L << (_len)) - 1) << (_start))) |
		      (((_value) & ((1L << (_len)) - 1)) << (_start))));
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

unsigned int lcd_ana_read(unsigned int _reg)
{
	return vclk_ana_reg_read(_reg);
}

void lcd_ana_write(unsigned int _reg, unsigned int _value)
{
	vclk_ana_reg_write(_reg, _value);
}

void lcd_ana_setb(unsigned int _reg, unsigned int _value,
		  unsigned int _start, unsigned int _len)
{
	lcd_ana_write(_reg, ((lcd_ana_read(_reg) &
		      (~(((1L << _len) - 1) << _start))) |
		      ((_value & ((1L << _len) - 1)) << _start)));
}

unsigned int lcd_ana_getb(unsigned int _reg,
			  unsigned int _start, unsigned int _len)
{
	return (lcd_ana_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

unsigned int lcd_cbus_read(unsigned int _reg)
{
	return aml_read_cbus(_reg);
};

void lcd_cbus_write(unsigned int _reg, unsigned int _value)
{
	aml_write_cbus(_reg, _value);
};

void lcd_cbus_setb(unsigned int _reg, unsigned int _value,
		   unsigned int _start, unsigned int _len)
{
	lcd_cbus_write(_reg, ((lcd_cbus_read(_reg) &
		       ~(((1L << (_len)) - 1) << (_start))) |
		       (((_value) & ((1L << (_len)) - 1)) << (_start))));
}

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

unsigned int dsi_host_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_dsi_host_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void dsi_host_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_dsi_host_reg(_reg);
	if (p)
		writel(_value, p);
};

void dsi_host_setb(unsigned int reg, unsigned int value,
		   unsigned int _start, unsigned int _len)
{
	dsi_host_write(reg, ((dsi_host_read(reg) &
		       (~(((1L << _len) -  1) << _start))) |
		       ((value & ((1L << _len) - 1)) << _start)));
}

unsigned int dsi_host_getb(unsigned int reg,
			   unsigned int _start, unsigned int _len)
{
	return (dsi_host_read(reg) >> _start) & ((1L << _len) - 1);
}

void dsi_host_set_mask(unsigned int reg, unsigned int _mask)
{
	dsi_host_write(reg, (dsi_host_read(reg) | (_mask)));
}

void dsi_host_clr_mask(unsigned int reg, unsigned int _mask)
{
	dsi_host_write(reg, (dsi_host_read(reg) & (~(_mask))));
}

unsigned int dsi_phy_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_dsi_phy_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void dsi_phy_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_dsi_phy_reg(_reg);
	if (p)
		writel(_value, p);
};

void dsi_phy_setb(unsigned int reg, unsigned int value,
		  unsigned int _start, unsigned int _len)
{
	dsi_phy_write(reg, ((dsi_phy_read(reg) &
		      (~(((1L << _len) - 1) << _start))) |
		      ((value & ((1L << _len) - 1)) << _start)));
}

unsigned int dsi_phy_getb(unsigned int reg,
			  unsigned int _start, unsigned int _len)
{
	return (dsi_phy_read(reg) >> _start) & ((1L << _len) - 1);
}

void dsi_phy_set_mask(unsigned int reg, unsigned int _mask)
{
	dsi_phy_write(reg, (dsi_phy_read(reg) | (_mask)));
}

void dsi_phy_clr_mask(unsigned int reg, unsigned int _mask)
{
	dsi_phy_write(reg, (dsi_phy_read(reg) & (~(_mask))));
}

unsigned int lcd_tcon_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_tcon_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void lcd_tcon_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_lcd_tcon_reg(_reg);
	if (p)
		writel(_value, p);
};

void lcd_tcon_setb(unsigned int reg, unsigned int value,
		   unsigned int _start, unsigned int _len)
{
	lcd_tcon_write(reg, ((lcd_tcon_read(reg) &
		       (~(((1L << _len) - 1) << _start))) |
		       ((value & ((1L << _len) - 1)) << _start)));
}

unsigned int lcd_tcon_getb(unsigned int reg,
			   unsigned int _start, unsigned int _len)
{
	return (lcd_tcon_read(reg) >> _start) & ((1L << _len) - 1);
}

void lcd_tcon_set_mask(unsigned int reg, unsigned int _mask)
{
	lcd_tcon_write(reg, (lcd_tcon_read(reg) | (_mask)));
}

void lcd_tcon_clr_mask(unsigned int reg, unsigned int _mask)
{
	lcd_tcon_write(reg, (lcd_tcon_read(reg) & (~(_mask))));
}

void lcd_tcon_update_bits(unsigned int reg,
			  unsigned int mask, unsigned int value)
{
	if (mask == 0xffffffff) {
		lcd_tcon_write(reg, value);
	} else {
		lcd_tcon_write(reg, (lcd_tcon_read(reg) & (~(mask))) |
			       (value & mask));
	}
}

int lcd_tcon_check_bits(unsigned int reg,
			unsigned int mask, unsigned int value)
{
	unsigned int temp;

	temp = lcd_tcon_read(reg) & mask;
	if (value != temp)
		return -1;

	return 0;
}

unsigned char lcd_tcon_read_byte(unsigned int _reg)
{
	void __iomem *p;

	p = check_lcd_tcon_reg_byte(_reg);
	if (p)
		return readb(p);
	else
		return -1;
};

void lcd_tcon_write_byte(unsigned int _reg, unsigned char _value)
{
	void __iomem *p;

	p = check_lcd_tcon_reg_byte(_reg);
	if (p)
		writeb(_value, p);
};

void lcd_tcon_setb_byte(unsigned int reg, unsigned char value,
			unsigned int _start, unsigned int _len)
{
	lcd_tcon_write_byte(reg, ((lcd_tcon_read_byte(reg) &
		(~(((1L << _len) - 1) << _start))) |
		((value & ((1L << _len) - 1)) << _start)));
}

unsigned char lcd_tcon_getb_byte(unsigned int reg,
				 unsigned int _start, unsigned int _len)
{
	return (lcd_tcon_read_byte(reg) >> _start) & ((1L << _len) - 1);
}

void lcd_tcon_update_bits_byte(unsigned int reg,
			       unsigned char mask, unsigned char value)
{
	if (mask == 0xff) {
		lcd_tcon_write_byte(reg, value);
	} else {
		lcd_tcon_write_byte(reg, (lcd_tcon_read_byte(reg) & (~(mask))) |
				    (value & mask));
	}
}

int lcd_tcon_check_bits_byte(unsigned int reg,
			     unsigned char mask, unsigned char value)
{
	unsigned char temp;

	temp = lcd_tcon_read_byte(reg) & mask;
	if (value != temp)
		return -1;

	return 0;
}
