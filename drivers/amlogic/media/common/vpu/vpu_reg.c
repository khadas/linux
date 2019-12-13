// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include "vpu_reg.h"
#include "vpu.h"

enum vpu_map_e {
	VPU_MAP_HIU = 0,
	VPU_MAP_VCBUS,
	VPU_MAP_CBUS,
	VPU_MAP_AOBUS,
	VPU_MAP_MAX,
};

struct vpu_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

static struct vpu_reg_map_s *vpu_reg_map;

int vpu_ioremap(struct platform_device *pdev)
{
	int i = 0;
	struct resource *res;

	vpu_reg_map = devm_kcalloc(&pdev->dev, VPU_MAP_MAX,
				   sizeof(struct vpu_reg_map_s),
				   GFP_KERNEL);
	if (!vpu_reg_map) {
		VPUERR("%s: vpu_reg_map buf malloc error\n", __func__);
		return -ENOMEM;
	}
	while (i < VPU_MAP_MAX) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			VPUERR("%s: vpu_reg resource get error\n", __func__);
			vpu_reg_map = NULL;
			return -ENOMEM;
		}
		vpu_reg_map[i].base_addr = res->start;
		vpu_reg_map[i].size = resource_size(res);
		vpu_reg_map[i].p = devm_ioremap_nocache(&pdev->dev,
							res->start,
							vpu_reg_map[i].size);
		if (!vpu_reg_map[i].p) {
			vpu_reg_map[i].flag = 0;
			VPUERR("%s: reg map failed: 0x%x\n",
			       __func__, vpu_reg_map[i].base_addr);
			vpu_reg_map = NULL;
			return -ENOMEM;
		}
		vpu_reg_map[i].flag = 1;
		if (vpu_debug_print_flag) {
			VPUPR("%s: reg mapped: 0x%x -> %p\n",
			      __func__, vpu_reg_map[i].base_addr,
			      vpu_reg_map[i].p);
		}

		i++;
	}

	return 0;
}

static int check_vpu_ioremap(int n)
{
	int ret = 0;

	if (WARN_ON(!vpu_reg_map || n >= VPU_MAP_MAX ||
		    !vpu_reg_map[n].flag))
		ret = -1;
	return ret;
}

static inline void __iomem *check_vpu_hiu_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_base, reg_offset;

	reg_bus = VPU_MAP_HIU;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_base = vpu_reg_map[reg_bus].base_addr - VPU_HIU_BASE;
	reg_offset = (_reg << 2) - reg_base;

	if (WARN_ON(reg_offset >= vpu_reg_map[reg_bus].size))
		return NULL;

	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vpu_vcbus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_base, reg_offset;

	reg_bus = VPU_MAP_VCBUS;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_base = vpu_reg_map[reg_bus].base_addr - VPU_VCBUS_BASE;
	reg_offset = (_reg << 2) - reg_base;

	if (WARN_ON(reg_offset >= vpu_reg_map[reg_bus].size))
		return NULL;

	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vpu_cbus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_base, reg_offset;

	reg_bus = VPU_MAP_CBUS;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_base = vpu_reg_map[reg_bus].base_addr - VPU_CBUS_BASE;
	reg_offset = (_reg << 2) - reg_base;

	if (WARN_ON(reg_offset >= vpu_reg_map[reg_bus].size))
		return NULL;

	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vpu_aobus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_base, reg_offset;

	reg_bus = VPU_MAP_AOBUS;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_base = vpu_reg_map[reg_bus].base_addr - VPU_AOBUS_BASE;
	reg_offset = (_reg << 2) - reg_base;

	if (WARN_ON(reg_offset >= vpu_reg_map[reg_bus].size))
		return NULL;

	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

/* ********************************
 * register access api
 * *********************************
 */

unsigned int vpu_hiu_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_vpu_hiu_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void vpu_hiu_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_vpu_hiu_reg(_reg);
	if (p)
		writel(_value, p);
};

void vpu_hiu_setb(unsigned int _reg, unsigned int _value,
		  unsigned int _start, unsigned int _len)
{
	vpu_hiu_write(_reg, ((vpu_hiu_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

unsigned int vpu_hiu_getb(unsigned int _reg, unsigned int _start,
			  unsigned int _len)
{
	return (vpu_hiu_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void vpu_hiu_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_hiu_write(_reg, (vpu_hiu_read(_reg) | (_mask)));
}

void vpu_hiu_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_hiu_write(_reg, (vpu_hiu_read(_reg) & (~(_mask))));
}

unsigned int vpu_vcbus_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_vpu_vcbus_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void vpu_vcbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_vpu_vcbus_reg(_reg);
	if (p)
		writel(_value, p);
};

void vpu_vcbus_setb(unsigned int _reg, unsigned int _value,
		    unsigned int _start, unsigned int _len)
{
	vpu_vcbus_write(_reg, ((vpu_vcbus_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

unsigned int vpu_vcbus_getb(unsigned int _reg, unsigned int _start,
			    unsigned int _len)
{
	return (vpu_vcbus_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void vpu_vcbus_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_vcbus_write(_reg, (vpu_vcbus_read(_reg) | (_mask)));
}

void vpu_vcbus_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_vcbus_write(_reg, (vpu_vcbus_read(_reg) & (~(_mask))));
}

unsigned int vpu_cbus_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_vpu_cbus_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void vpu_cbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_vpu_cbus_reg(_reg);
	if (p)
		writel(_value, p);
};

void vpu_cbus_setb(unsigned int _reg, unsigned int _value,
		   unsigned int _start, unsigned int _len)
{
	vpu_cbus_write(_reg, ((vpu_cbus_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

void vpu_cbus_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_cbus_write(_reg, (vpu_cbus_read(_reg) | (_mask)));
}

void vpu_cbus_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_cbus_write(_reg, (vpu_cbus_read(_reg) & (~(_mask))));
}

unsigned int vpu_ao_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_vpu_aobus_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void vpu_ao_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_vpu_aobus_reg(_reg);
	if (p)
		writel(_value, p);
};

void vpu_ao_setb(unsigned int _reg, unsigned int _value,
		 unsigned int _start, unsigned int _len)
{
	vpu_ao_write(_reg, ((vpu_ao_read(_reg) &
		     ~(((1L << (_len)) - 1) << (_start))) |
		     (((_value) & ((1L << (_len)) - 1)) << (_start))));
}

