// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/vclk_serve.h>
#include <linux/amlogic/gki_module.h>

#define VCLKPR(fmt, args...)     pr_info("vclk: " fmt "", ## args)
#define VCLKERR(fmt, args...)    pr_err("vclk: error: " fmt "", ## args)

#define VCLK_REG_OFFSET(addr)    ((addr) << 2)

struct vclk_data_s {
	unsigned int ioremap_flag;
};

static spinlock_t vclk_ana_lock;
static spinlock_t vclk_clk_lock;

/* ********************************
 * mem map
 * *********************************
 */
#define VCLK_MAP_HIU       0
#define VCLK_MAP_ANA       1
#define VCLK_MAP_CLK       2
#define VCLK_MAP_MAX       3

static int vclk_reg_table_0[] = {
	VCLK_MAP_HIU,
	VCLK_MAP_MAX
};

static int vclk_reg_table_1[] = {
	VCLK_MAP_ANA,
	VCLK_MAP_CLK,
	VCLK_MAP_MAX
};

struct vclk_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

static struct vclk_reg_map_s *vclk_reg_map;
static int vclk_ioremap_flag;
static int vclk_ioremap_mode = 1;

static int vclk_ioremap(struct platform_device *pdev)
{
	int i;
	int *table;
	struct resource *res;

	vclk_reg_map = kcalloc(VCLK_MAP_MAX,
			       sizeof(struct vclk_reg_map_s), GFP_KERNEL);
	if (!vclk_reg_map)
		return -1;

	if (vclk_ioremap_mode)
		table = vclk_reg_table_1;
	else
		table = vclk_reg_table_0;
	for (i = 0; i < VCLK_MAP_MAX; i++) {
		if (table[i] == VCLK_MAP_MAX)
			break;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			VCLKERR("%s: resource get error\n", __func__);
			kfree(vclk_reg_map);
			vclk_reg_map = NULL;
			return -1;
		}
		vclk_reg_map[table[i]].base_addr = res->start;
		vclk_reg_map[table[i]].size = resource_size(res);
		vclk_reg_map[table[i]].p = devm_ioremap_nocache(&pdev->dev,
			res->start, vclk_reg_map[table[i]].size);
		if (!vclk_reg_map[table[i]].p) {
			vclk_reg_map[table[i]].flag = 0;
			VCLKERR("%s: reg bus[%d] map failed: 0x%x\n",
				__func__, table[i],
				vclk_reg_map[table[i]].base_addr);
			kfree(vclk_reg_map);
			vclk_reg_map = NULL;
			return -1;
		}
		vclk_reg_map[table[i]].flag = 1;
		/*
		 *VCLKPR("%s: reg bus[%d] mapped: 0x%x -> 0x%px\n",
		 *     __func__, table[i],
		 *     vclk_reg_map[table[i]].base_addr,
		 *     vclk_reg_map[table[i]].p);
		 */
	}

	return 0;
}

static void vclk_iounmap(struct platform_device *pdev)
{
	int *table;
	int i;

	if (!vclk_reg_map)
		return;

	if (vclk_ioremap_mode)
		table = vclk_reg_table_1;
	else
		table = vclk_reg_table_0;
	for (i = 0; i < VCLK_MAP_MAX; i++) {
		if (table[i] == VCLK_MAP_MAX)
			break;
		devm_iounmap(&pdev->dev, vclk_reg_map[table[i]].p);
		vclk_reg_map[table[i]].flag = 0;
	}

	kfree(vclk_reg_map);
	vclk_reg_map = NULL;
}

static int check_vclk_ioremap(int n)
{
	if (!vclk_reg_map)
		return -1;
	if (n >= VCLK_MAP_MAX)
		return -1;
	if (vclk_reg_map[n].flag == 0) {
		VCLKERR("reg bus[%d] addr: 0x%x mapped error\n",
			n, vclk_reg_map[n].base_addr);
		return -1;
	}
	return 0;
}

static inline void __iomem *check_vclk_hiu_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VCLK_MAP_HIU;
	if (check_vclk_ioremap(reg_bus))
		return NULL;

	reg_offset = VCLK_REG_OFFSET(_reg);

	if (reg_offset >= vclk_reg_map[reg_bus].size) {
		VCLKERR("invalid hiu reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = vclk_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vclk_ana_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VCLK_MAP_ANA;
	if (check_vclk_ioremap(reg_bus))
		return NULL;

	reg_offset = VCLK_REG_OFFSET(_reg);

	if (reg_offset >= vclk_reg_map[reg_bus].size) {
		VCLKERR("invalid ana reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = vclk_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vclk_clk_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VCLK_MAP_CLK;
	if (check_vclk_ioremap(reg_bus))
		return NULL;

	reg_offset = VCLK_REG_OFFSET(_reg);

	if (reg_offset >= vclk_reg_map[reg_bus].size) {
		VCLKERR("invalid clk reg offset: 0x%04x\n", _reg);
		return NULL;
	}
	p = vclk_reg_map[reg_bus].p + reg_offset;
	return p;
}

/* ********************************
 * register access api
 * **********************************/
unsigned int vclk_hiu_reg_read(unsigned int _reg)
{
	unsigned long flags = 0;
	void __iomem *p;
	unsigned int ret = 0;

	spin_lock_irqsave(&vclk_ana_lock, flags);
	if (vclk_ioremap_flag) {
		p = check_vclk_hiu_reg(_reg);
		if (p)
			ret = readl(p);
		else
			ret = 0;
	} else {
		ret = aml_read_hiubus(_reg);
	}
	spin_unlock_irqrestore(&vclk_ana_lock, flags);

	return ret;
};

void vclk_hiu_reg_write(unsigned int _reg, unsigned int _value)
{
	unsigned long flags = 0;
	void __iomem *p;

	spin_lock_irqsave(&vclk_ana_lock, flags);
	if (vclk_ioremap_flag) {
		p = check_vclk_hiu_reg(_reg);
		if (p)
			writel(_value, p);
	} else {
		aml_write_hiubus(_reg, _value);
	}
	spin_unlock_irqrestore(&vclk_ana_lock, flags);
};

void vclk_hiu_reg_setb(unsigned int _reg, unsigned int _value,
		       unsigned int _start, unsigned int _len)
{
	void __iomem *p;
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&vclk_ana_lock, flags);

	if (vclk_ioremap_flag) {
		p = check_vclk_hiu_reg(_reg);
		if (p) {
			temp = readl(p);
			temp = (temp & (~(((1L << _len) - 1) << _start))) |
				((_value & ((1L << _len) - 1)) << _start);
			writel(temp, p);
		}
	} else {
		temp = aml_read_hiubus(_reg);
		temp = (temp & (~(((1L << _len) - 1) << _start))) |
			((_value & ((1L << _len) - 1)) << _start);
		aml_write_hiubus(_reg, temp);
	}

	spin_unlock_irqrestore(&vclk_ana_lock, flags);
}

unsigned int vclk_hiu_reg_getb(unsigned int _reg,
			       unsigned int _start, unsigned int _len)
{
	void __iomem *p;
	unsigned int val;
	unsigned long flags = 0;

	spin_lock_irqsave(&vclk_ana_lock, flags);

	if (vclk_ioremap_flag) {
		p = check_vclk_hiu_reg(_reg);
		if (p)
			val = readl(p);
		else
			val = 0;
	} else {
		val = aml_read_hiubus(_reg);
	}
	val = (val >> _start) & ((1L << _len) - 1);

	spin_unlock_irqrestore(&vclk_ana_lock, flags);
	return val;
}

unsigned int vclk_ana_reg_read(unsigned int _reg)
{
	unsigned long flags = 0;
	void __iomem *p;
	unsigned int ret = 0;

	spin_lock_irqsave(&vclk_ana_lock, flags);
	if (vclk_ioremap_flag) {
		p = check_vclk_ana_reg(_reg);
		if (p)
			ret = readl(p);
		else
			ret = 0;
	} else {
		ret = aml_read_hiubus(_reg);
	}
	spin_unlock_irqrestore(&vclk_ana_lock, flags);

	return ret;
};

void vclk_ana_reg_write(unsigned int _reg, unsigned int _value)
{
	unsigned long flags = 0;
	void __iomem *p;

	spin_lock_irqsave(&vclk_ana_lock, flags);
	if (vclk_ioremap_flag) {
		p = check_vclk_ana_reg(_reg);
		if (p)
			writel(_value, p);
	} else {
		aml_write_hiubus(_reg, _value);
	}
	spin_unlock_irqrestore(&vclk_ana_lock, flags);
};

void vclk_ana_reg_setb(unsigned int _reg, unsigned int _value,
		       unsigned int _start, unsigned int _len)
{
	void __iomem *p;
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&vclk_ana_lock, flags);

	if (vclk_ioremap_flag) {
		p = check_vclk_ana_reg(_reg);
		if (p) {
			temp = readl(p);
			temp = (temp & (~(((1L << _len) - 1) << _start))) |
				((_value & ((1L << _len) - 1)) << _start);
			writel(temp, p);
		}
	} else {
		temp = aml_read_hiubus(_reg);
		temp = (temp & (~(((1L << _len) - 1) << _start))) |
			((_value & ((1L << _len) - 1)) << _start);
		aml_write_hiubus(_reg, temp);
	}

	spin_unlock_irqrestore(&vclk_ana_lock, flags);
}

unsigned int vclk_ana_reg_getb(unsigned int _reg,
			       unsigned int _start, unsigned int _len)
{
	void __iomem *p;
	unsigned int val;
	unsigned long flags = 0;

	spin_lock_irqsave(&vclk_ana_lock, flags);

	if (vclk_ioremap_flag) {
		p = check_vclk_ana_reg(_reg);
		if (p)
			val = readl(p);
		else
			val = 0;
	} else {
		val = aml_read_hiubus(_reg);
	}
	val = (val >> _start) & ((1L << _len) - 1);

	spin_unlock_irqrestore(&vclk_ana_lock, flags);
	return val;
}

unsigned int vclk_clk_reg_read(unsigned int _reg)
{
	unsigned long flags = 0;
	void __iomem *p;
	unsigned int ret = 0;

	spin_lock_irqsave(&vclk_clk_lock, flags);
	if (vclk_ioremap_flag) {
		p = check_vclk_clk_reg(_reg);
		if (p)
			ret = readl(p);
		else
			ret = 0;
	} else {
		ret = aml_read_hiubus(_reg);
	}
	spin_unlock_irqrestore(&vclk_clk_lock, flags);

	return ret;
};

void vclk_clk_reg_write(unsigned int _reg, unsigned int _value)
{
	unsigned long flags = 0;
	void __iomem *p;

	spin_lock_irqsave(&vclk_clk_lock, flags);
	if (vclk_ioremap_flag) {
		p = check_vclk_clk_reg(_reg);
		if (p)
			writel(_value, p);
	} else {
		aml_write_hiubus(_reg, _value);
	}
	spin_unlock_irqrestore(&vclk_clk_lock, flags);
};

void vclk_clk_reg_setb(unsigned int _reg, unsigned int _value,
		       unsigned int _start, unsigned int _len)
{
	void __iomem *p;
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&vclk_clk_lock, flags);

	if (vclk_ioremap_flag) {
		p = check_vclk_clk_reg(_reg);
		if (p) {
			temp = readl(p);
			temp = (temp & (~(((1L << _len) - 1) << _start))) |
				((_value & ((1L << _len) - 1)) << _start);
			writel(temp, p);
		}
	} else {
		temp = aml_read_hiubus(_reg);
		temp = (temp & (~(((1L << _len) - 1) << _start))) |
			((_value & ((1L << _len) - 1)) << _start);
		aml_write_hiubus(_reg, temp);
	}

	spin_unlock_irqrestore(&vclk_clk_lock, flags);
}

unsigned int vclk_clk_reg_getb(unsigned int _reg,
			       unsigned int _start, unsigned int _len)
{
	void __iomem *p;
	unsigned int val;
	unsigned long flags = 0;

	spin_lock_irqsave(&vclk_clk_lock, flags);

	if (vclk_ioremap_flag) {
		p = check_vclk_clk_reg(_reg);
		if (p)
			val = readl(p);
		else
			val = 0;
	} else {
		val = aml_read_hiubus(_reg);
	}
	val = (val >> _start) & ((1L << _len) - 1);

	spin_unlock_irqrestore(&vclk_clk_lock, flags);
	return val;
}

/*****************************************************************
 **
 **	vout driver interface
 **
 ******************************************************************/
static struct vclk_data_s vclk_match_data = {
	.ioremap_flag = 1,
};

static const struct of_device_id vclk_match_table[] = {
	{
		.compatible = "amlogic, vclk_serve",
		.data = &vclk_match_data,
	},
	{ }
};

static int aml_vclk_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct vclk_data_s *vclk_data;
	unsigned int temp;
	int ret;

	match = of_match_device(vclk_match_table, &pdev->dev);
	if (!match) {
		VCLKPR("%s: no match table\n", __func__);
		return -1;
	}
	vclk_data = (struct vclk_data_s *)match->data;

	ret = of_property_read_u32(pdev->dev.of_node, "mode", &temp);
	if (ret == 0) {
		vclk_ioremap_mode = temp;
		VCLKPR("%s: find mode: %d\n", __func__, vclk_ioremap_mode);
	}

	if (vclk_data->ioremap_flag) {
		vclk_ioremap_flag = 1;
		vclk_ioremap(pdev);
	}

	VCLKPR("%s OK\n", __func__);
	return 0;
}

static int aml_vclk_remove(struct platform_device *pdev)
{
	vclk_iounmap(pdev);
	return 0;
}

static struct platform_driver aml_vclk_driver = {
	.probe     = aml_vclk_probe,
	.remove    = aml_vclk_remove,
	.driver = {
		.name = "vclk",
		.of_match_table = vclk_match_table,
	},
};

int __init aml_vclk_init_module(void)
{
	int ret = 0;

	vclk_ioremap_flag = 0;
	spin_lock_init(&vclk_ana_lock);
	spin_lock_init(&vclk_clk_lock);
	if (platform_driver_register(&aml_vclk_driver)) {
		VCLKERR("failed to register vclk driver\n");
		ret = -ENODEV;
	}

	return ret;
}

__exit void aml_vclk_exit_module(void)
{
	platform_driver_unregister(&aml_vclk_driver);
}

//MODULE_AUTHOR("Evoke Zhang <evoke.zhang@amlogic.com>");
//MODULE_DESCRIPTION("VCLK Server Module");
//MODULE_LICENSE("GPL");
