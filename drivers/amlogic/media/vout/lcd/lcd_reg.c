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
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/vclk_serve.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "lcd_common.h"
#include "lcd_reg.h"

#define LCD_MAP_PERIPHS     0
#define LCD_MAP_DSI_HOST    1
#define LCD_MAP_DSI_PHY     2
#define LCD_MAP_TCON        3
#define LCD_MAP_EDP         4
#define LCD_MAP_COMBO_DPHY  5
#define LCD_MAP_RST         6
#define LCD_MAP_MAX         7

int lcd_reg_g12a[] = {
	LCD_MAP_DSI_HOST,
	LCD_MAP_DSI_PHY,
	LCD_MAP_MAX,
};

int lcd_reg_tl1[] = {
	LCD_MAP_TCON,
	LCD_MAP_PERIPHS,
	LCD_MAP_MAX,
};

int lcd_reg_t5[] = {
	LCD_MAP_TCON,
	LCD_MAP_PERIPHS,
	LCD_MAP_RST,
	LCD_MAP_MAX,
};

int lcd_reg_t7[] = {
	LCD_MAP_COMBO_DPHY,
	LCD_MAP_EDP,
	LCD_MAP_DSI_HOST,
	LCD_MAP_DSI_PHY,
	LCD_MAP_RST,
	LCD_MAP_PERIPHS,
	LCD_MAP_MAX,
};

/* for lcd reg access */
spinlock_t lcd_reg_spinlock;

int lcd_ioremap(struct aml_lcd_drv_s *pdrv, struct platform_device *pdev)
{
	struct lcd_reg_map_s *reg_map = NULL;
	struct resource *res;
	int *table;
	int i = 0;

	if (!pdrv->data || !pdrv->data->reg_map_table) {
		LCDERR("[%d]: %s: reg_map table is null\n",
		       pdrv->index, __func__);
		return -1;
	}
	table = pdrv->data->reg_map_table;

	reg_map = kcalloc(LCD_MAP_MAX,
			  sizeof(struct lcd_reg_map_s), GFP_KERNEL);
	if (!reg_map)
		return -1;
	pdrv->reg_map = reg_map;

	while (i < LCD_MAP_MAX) {
		if (table[i] == LCD_MAP_MAX)
			break;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			LCDERR("[%d]: %s: get resource error\n",
			       pdrv->index, __func__);
			goto lcd_ioremap_err;
		}
		reg_map[table[i]].base_addr = res->start;
		reg_map[table[i]].size = resource_size(res);
		reg_map[table[i]].p = devm_ioremap_nocache(&pdev->dev,
			res->start, reg_map[table[i]].size);
		if (!reg_map[table[i]].p) {
			reg_map[table[i]].flag = 0;
			LCDERR("[%d]: %s: reg %d failed: 0x%x 0x%x\n",
			       pdrv->index, __func__, i,
			       reg_map[table[i]].base_addr,
			       reg_map[table[i]].size);
			goto lcd_ioremap_err;
		}
		reg_map[table[i]].flag = 1;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: %s: reg %d: 0x%x -> %p, size: 0x%x\n",
			      pdrv->index, __func__, i,
			      reg_map[table[i]].base_addr,
			      reg_map[table[i]].p,
			      reg_map[table[i]].size);
		}

		i++;
	}

	return 0;

lcd_ioremap_err:
	kfree(reg_map);
	pdrv->reg_map = NULL;
	return -1;
}

static int check_lcd_ioremap(struct aml_lcd_drv_s *pdrv, unsigned int n)
{
	if (!pdrv->reg_map) {
		LCDERR("[%d]: %s: reg_map is null\n", pdrv->index, __func__);
		return -1;
	}
	if (n >= LCD_MAP_MAX)
		return -1;
	if (pdrv->reg_map[n].flag == 0) {
		LCDERR("[%d]: %s: reg 0x%x mapped error\n",
		       pdrv->index, __func__, pdrv->reg_map[n].base_addr);
		return -1;
	}
	return 0;
}

static inline void __iomem *check_lcd_periphs_reg(struct aml_lcd_drv_s *pdrv,
						  unsigned int reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_PERIPHS;
	if (check_lcd_ioremap(pdrv, reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET(reg);

	if (reg_offset >= pdrv->reg_map[reg_bus].size) {
		LCDERR("[%d]: invalid periphs reg offset: 0x%04x\n",
		       pdrv->index, reg);
		return NULL;
	}
	p = pdrv->reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_dsi_host_reg(struct aml_lcd_drv_s *pdrv,
						   unsigned int reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_DSI_HOST;
	if (check_lcd_ioremap(pdrv, reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET(reg);
	if (reg_offset >= pdrv->reg_map[reg_bus].size) {
		LCDERR("[%d]: invalid dsi_host reg offset: 0x%04x\n",
		       pdrv->index, reg);
		return NULL;
	}
	p = pdrv->reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_dsi_phy_reg(struct aml_lcd_drv_s *pdrv,
						  unsigned int reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_DSI_PHY;
	if (check_lcd_ioremap(pdrv, reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET(reg);
	if (reg_offset >= pdrv->reg_map[reg_bus].size) {
		LCDERR("[%d]: invalid dsi_phy reg offset: 0x%04x\n",
		       pdrv->index, reg);
		return NULL;
	}
	p = pdrv->reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_tcon_reg(struct aml_lcd_drv_s *pdrv,
					       unsigned int reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_TCON;
	if (check_lcd_ioremap(pdrv, reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET(reg);
	if (reg_offset >= pdrv->reg_map[reg_bus].size) {
		LCDERR("[%d]: invalid tcon reg offset: 0x%04x\n",
		       pdrv->index, reg);
		return NULL;
	}
	p = pdrv->reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_tcon_reg_byte(struct aml_lcd_drv_s *pdrv,
						    unsigned int reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_TCON;
	if (check_lcd_ioremap(pdrv, reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET_BYTE(reg);
	if (reg_offset >= pdrv->reg_map[reg_bus].size) {
		LCDERR("[%d]: invalid tcon reg offset: 0x%04x\n",
		       pdrv->index, reg);
		return NULL;
	}
	p = pdrv->reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_dptx_reg(struct aml_lcd_drv_s *pdrv,
					       unsigned int reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_EDP;
	if (check_lcd_ioremap(pdrv, reg_bus))
		return NULL;

	reg_offset = reg; /* don't left shift */

	if (reg_offset >= pdrv->reg_map[reg_bus].size) {
		LCDERR("[%d]: invalid dptx reg offset: 0x%04x\n",
		       pdrv->index, reg);
		return NULL;
	}
	p = pdrv->reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_combo_dphy_reg(struct aml_lcd_drv_s *pdrv,
						     unsigned int reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_COMBO_DPHY;
	if (check_lcd_ioremap(pdrv, reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET(reg);

	if (reg_offset >= pdrv->reg_map[reg_bus].size) {
		LCDERR("[%d]: invalid combo dphy reg offset: 0x%04x\n",
		       pdrv->index, reg);
		return NULL;
	}
	p = pdrv->reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_lcd_reset_reg(struct aml_lcd_drv_s *pdrv,
					      unsigned int reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = LCD_MAP_RST;
	if (check_lcd_ioremap(pdrv, reg_bus))
		return NULL;

	reg_offset = LCD_REG_OFFSET(reg);

	if (reg_offset >= pdrv->reg_map[reg_bus].size) {
		LCDERR("[%d]: invalid reset reg offset: 0x%04x\n",
		       pdrv->index, reg);
		return NULL;
	}
	p = pdrv->reg_map[reg_bus].p + reg_offset;
	return p;
}

/******************************************************/
unsigned int lcd_vcbus_read(unsigned int reg)
{
#ifdef CONFIG_AMLOGIC_VPU
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	temp = vpu_vcbus_read(reg);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
	return temp;
#endif
};

void lcd_vcbus_write(unsigned int reg, unsigned int value)
{
#ifdef CONFIG_AMLOGIC_VPU
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vpu_vcbus_write(reg, value);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
#endif
};

void lcd_vcbus_setb(unsigned int reg, unsigned int value,
		    unsigned int start, unsigned int len)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vpu_vcbus_write(reg, ((vpu_vcbus_read(reg) &
		(~(((1L << len) - 1) << start))) |
		((value & ((1L << len) - 1)) << start)));
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int lcd_vcbus_getb(unsigned int reg,
			    unsigned int start, unsigned int len)
{
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	temp = (vpu_vcbus_read(reg) >> start) & ((1L << len) - 1);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

void lcd_vcbus_set_mask(unsigned int reg, unsigned int mask)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vpu_vcbus_write(reg, (vpu_vcbus_read(reg) | (mask)));
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

void lcd_vcbus_clr_mask(unsigned int reg, unsigned int mask)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vpu_vcbus_write(reg, (vpu_vcbus_read(reg) & (~(mask))));
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int lcd_hiu_read(unsigned int reg)
{
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	temp =  vclk_hiu_reg_read(reg);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
	if (lcd_debug_print_flag & LCD_DBG_PR_REG)
		LCDPR("%s: 0x%02x = 0x%08x\n", __func__, reg, temp);

	return temp;
};

void lcd_hiu_write(unsigned int reg, unsigned int val)
{
	unsigned long flags = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_REG)
		LCDPR("%s: 0x%02x = 0x%08x\n", __func__, reg, val);
	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vclk_hiu_reg_write(reg, val);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
};

void lcd_hiu_setb(unsigned int reg, unsigned int val,
		  unsigned int start, unsigned int len)
{
	unsigned long flags = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_REG) {
		LCDPR("%s: 0x%02x[%d:%d] = 0x%x\n",
			__func__, reg, (start + len - 1), start, val);
	}
	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vclk_hiu_reg_write(reg, ((vclk_hiu_reg_read(reg) &
			   ~(((1L << (len)) - 1) << (start))) |
			   (((val) & ((1L << (len)) - 1)) << (start))));
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int lcd_hiu_getb(unsigned int reg,
			  unsigned int start, unsigned int len)
{
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	temp =  (vclk_hiu_reg_read(reg) >> (start)) & ((1L << (len)) - 1);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
	if (lcd_debug_print_flag & LCD_DBG_PR_REG) {
		LCDPR("%s: 0x%02x[%d:%d] = 0x%x\n",
			__func__, reg, (start + len - 1), start, temp);
	}

	return temp;
}

unsigned int lcd_clk_read(unsigned int reg)
{
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	temp =  vclk_clk_reg_read(reg);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
};

void lcd_clk_write(unsigned int reg, unsigned int val)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vclk_clk_reg_write(reg, val);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
};

void lcd_clk_setb(unsigned int reg, unsigned int val,
		  unsigned int start, unsigned int len)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vclk_clk_reg_write(reg, ((vclk_clk_reg_read(reg) &
			   ~(((1L << (len)) - 1) << (start))) |
			   (((val) & ((1L << (len)) - 1)) << (start))));
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int lcd_clk_getb(unsigned int reg,
			  unsigned int start, unsigned int len)
{
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	temp =  (vclk_clk_reg_read(reg) >> (start)) & ((1L << (len)) - 1);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

void lcd_clk_set_mask(unsigned int reg, unsigned int mask)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vclk_clk_reg_write(reg, (vclk_clk_reg_read(reg) | (mask)));
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

void lcd_clk_clr_mask(unsigned int reg, unsigned int mask)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vclk_clk_reg_write(reg, (vclk_clk_reg_read(reg) & (~(mask))));
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int lcd_ana_read(unsigned int reg)
{
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	temp =  vclk_ana_reg_read(reg);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

void lcd_ana_write(unsigned int reg, unsigned int val)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vclk_ana_reg_write(reg, val);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

void lcd_ana_setb(unsigned int reg, unsigned int val,
		  unsigned int start, unsigned int len)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	vclk_ana_reg_write(reg, ((vclk_ana_reg_read(reg) &
			   (~(((1L << len) - 1) << start))) |
			   ((val & ((1L << len) - 1)) << start)));
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int lcd_ana_getb(unsigned int reg,
			  unsigned int start, unsigned int len)
{
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	temp = (vclk_ana_reg_read(reg) >> (start)) & ((1L << (len)) - 1);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

unsigned int lcd_cbus_read(unsigned int reg)
{
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	temp = aml_read_cbus(reg);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
};

void lcd_cbus_write(unsigned int reg, unsigned int val)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	aml_write_cbus(reg, val);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
};

void lcd_cbus_setb(unsigned int reg, unsigned int val,
		   unsigned int start, unsigned int len)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	aml_write_cbus(reg, ((aml_read_cbus(reg) &
		       ~(((1L << (len)) - 1) << (start))) |
		       (((val) & ((1L << (len)) - 1)) << (start))));
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int lcd_periphs_read(struct aml_lcd_drv_s *pdrv, unsigned int reg)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_periphs_reg(pdrv, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
};

void lcd_periphs_write(struct aml_lcd_drv_s *pdrv,
		       unsigned int reg, unsigned int val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_periphs_reg(pdrv, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
};

unsigned int dsi_host_read(struct aml_lcd_drv_s *pdrv, unsigned int reg)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_host_reg(pdrv, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
};

void dsi_host_write(struct aml_lcd_drv_s *pdrv,
		    unsigned int reg, unsigned int val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_host_reg(pdrv, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
};

void dsi_host_setb(struct aml_lcd_drv_s *pdrv,
		   unsigned int reg, unsigned int value,
		   unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_host_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int dsi_host_getb(struct aml_lcd_drv_s *pdrv, unsigned int reg,
			   unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_host_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

void dsi_host_set_mask(struct aml_lcd_drv_s *pdrv,
		       unsigned int reg, unsigned int mask)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_host_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp |= (mask);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

void dsi_host_clr_mask(struct aml_lcd_drv_s *pdrv,
		       unsigned int reg, unsigned int mask)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_host_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp &= ~(mask);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int dsi_phy_read(struct aml_lcd_drv_s *pdrv, unsigned int reg)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_phy_reg(pdrv, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
};

void dsi_phy_write(struct aml_lcd_drv_s *pdrv,
		   unsigned int reg, unsigned int val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_phy_reg(pdrv, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
};

void dsi_phy_setb(struct aml_lcd_drv_s *pdrv,
		  unsigned int reg, unsigned int value,
		  unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_phy_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int dsi_phy_getb(struct aml_lcd_drv_s *pdrv, unsigned int reg,
			  unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_phy_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

void dsi_phy_set_mask(struct aml_lcd_drv_s *pdrv,
		      unsigned int reg, unsigned int mask)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_phy_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp |= (mask);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

void dsi_phy_clr_mask(struct aml_lcd_drv_s *pdrv,
		      unsigned int reg, unsigned int mask)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dsi_phy_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp &= ~(mask);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int lcd_tcon_read(struct aml_lcd_drv_s *pdrv, unsigned int reg)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_tcon_reg(pdrv, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
};

void lcd_tcon_write(struct aml_lcd_drv_s *pdrv,
		    unsigned int reg, unsigned int val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_tcon_reg(pdrv, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
};

void lcd_tcon_setb(struct aml_lcd_drv_s *pdrv,
		   unsigned int reg, unsigned int value,
		   unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_tcon_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int lcd_tcon_getb(struct aml_lcd_drv_s *pdrv, unsigned int reg,
			   unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);

	p = check_lcd_tcon_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

void lcd_tcon_update_bits(struct aml_lcd_drv_s *pdrv, unsigned int reg,
			  unsigned int mask, unsigned int value)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_tcon_reg(pdrv, reg);
	if (p) {
		if (mask == 0xffffffff) {
			writel(value, p);
		} else {
			temp =  readl(p);
			temp = (temp & (~(mask))) | (value & mask);
			writel(temp, p);
		}
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

int lcd_tcon_check_bits(struct aml_lcd_drv_s *pdrv, unsigned int reg,
			unsigned int mask, unsigned int value)
{
	void __iomem *p;
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_tcon_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		if ((temp & mask) != value)
			temp = -1;
		else
			temp = 0;
	} else {
		temp = -1;
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

unsigned char lcd_tcon_read_byte(struct aml_lcd_drv_s *pdrv, unsigned int reg)
{
	void __iomem *p;
	unsigned char temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_tcon_reg_byte(pdrv, reg);
	if (p)
		temp = readb(p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
};

void lcd_tcon_write_byte(struct aml_lcd_drv_s *pdrv,
			 unsigned int reg, unsigned char val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_tcon_reg_byte(pdrv, reg);
	if (p)
		writeb(val, p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
};

void lcd_tcon_setb_byte(struct aml_lcd_drv_s *pdrv,
			unsigned int reg, unsigned char value,
			unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned char temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_tcon_reg_byte(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned char lcd_tcon_getb_byte(struct aml_lcd_drv_s *pdrv, unsigned int reg,
				 unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned char temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_tcon_reg_byte(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

void lcd_tcon_update_bits_byte(struct aml_lcd_drv_s *pdrv, unsigned int reg,
			       unsigned char mask, unsigned char value)
{
	void __iomem *p;
	unsigned char temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_tcon_reg_byte(pdrv, reg);
	if (p) {
		if (mask == 0xff) {
			writel(value, p);
		} else {
			temp =  readl(p);
			temp = (temp & (~(mask))) | (value & mask);
			writel(temp, p);
		}
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

int lcd_tcon_check_bits_byte(struct aml_lcd_drv_s *pdrv, unsigned int reg,
			     unsigned char mask, unsigned char value)
{
	void __iomem *p;
	unsigned char temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_tcon_reg_byte(pdrv, reg);
	if (p) {
		temp = readl(p);
		if ((temp & mask) != value)
			temp = -1;
		else
			temp = 0;
	} else {
		temp = -1;
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

unsigned int dptx_reg_read(struct aml_lcd_drv_s *pdrv, unsigned int reg)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dptx_reg(pdrv, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
};

void dptx_reg_write(struct aml_lcd_drv_s *pdrv,
		    unsigned int reg, unsigned int val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dptx_reg(pdrv, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
};

void dptx_reg_setb(struct aml_lcd_drv_s *pdrv,
		   unsigned int reg, unsigned int value,
		   unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dptx_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int dptx_reg_getb(struct aml_lcd_drv_s *pdrv, unsigned int reg,
			  unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_dptx_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

unsigned int lcd_combo_dphy_read(struct aml_lcd_drv_s *pdrv, unsigned int reg)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_combo_dphy_reg(pdrv, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
};

void lcd_combo_dphy_write(struct aml_lcd_drv_s *pdrv,
			  unsigned int reg, unsigned int val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_combo_dphy_reg(pdrv, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
};

void lcd_combo_dphy_setb(struct aml_lcd_drv_s *pdrv,
			 unsigned int reg, unsigned int value,
			 unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_combo_dphy_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int lcd_combo_dphy_getb(struct aml_lcd_drv_s *pdrv, unsigned int reg,
				 unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_combo_dphy_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

unsigned int lcd_reset_read(struct aml_lcd_drv_s *pdrv, unsigned int reg)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_reset_reg(pdrv, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
};

void lcd_reset_write(struct aml_lcd_drv_s *pdrv,
		     unsigned int reg, unsigned int val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_reset_reg(pdrv, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
};

void lcd_reset_setb(struct aml_lcd_drv_s *pdrv,
		    unsigned int reg, unsigned int value,
		    unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_reset_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

unsigned int lcd_reset_getb(struct aml_lcd_drv_s *pdrv, unsigned int reg,
			    unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_reset_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);

	return temp;
}

void lcd_reset_set_mask(struct aml_lcd_drv_s *pdrv,
			unsigned int reg, unsigned int mask)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_reset_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp |= (mask);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}

void lcd_reset_clr_mask(struct aml_lcd_drv_s *pdrv,
			unsigned int reg, unsigned int mask)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&lcd_reg_spinlock, flags);
	p = check_lcd_reset_reg(pdrv, reg);
	if (p) {
		temp = readl(p);
		temp &= ~(mask);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&lcd_reg_spinlock, flags);
}
