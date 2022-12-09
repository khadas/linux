// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/io.h>

/* Local Headers */
#include "osd_log.h"
#include "osd_backup.h"

#define OSDBUS_REG_ADDR(reg) ((reg) << 2)

struct reg_map_s {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *vir_addr;
	int flag;
};

static struct reg_map_s osd_reg_map = {
	.phy_addr = 0xff900000,
	.size = 0x10000,
};

int osd_io_remap(int iomap)
{
	int ret = 0;

	if (iomap) {
		if (osd_reg_map.flag)
			return 1;
		osd_reg_map.vir_addr =
			ioremap(osd_reg_map.phy_addr, osd_reg_map.size);
		if (!osd_reg_map.vir_addr) {
			pr_info("failed map phy: 0x%x\n", osd_reg_map.phy_addr);
			ret = 0;
		} else {
			osd_reg_map.flag = 1;
			pr_info("mapped phy: 0x%x\n", osd_reg_map.phy_addr);
			ret = 1;
		}
	} else {
		ret = 1;
	}
	return ret;
}

u32 osd_cbus_read(u32 reg)
{
	u32 ret = 0;
	unsigned int addr = 0;

	if (osd_reg_map.flag) {
		addr = OSDBUS_REG_ADDR(reg);
		ret = readl(osd_reg_map.vir_addr + addr);
	} else {
		ret = (u32)aml_read_cbus(reg);
	}
	osd_log_dbg3(MODULE_BASE, "%s(0x%x)=0x%x\n", __func__, reg, ret);

	return ret;
}

void osd_cbus_write(u32 reg, const u32 val)
{
	unsigned int addr = 0;

	if (osd_reg_map.flag) {
		addr = OSDBUS_REG_ADDR(reg);
		writel(val, osd_reg_map.vir_addr + addr);
	} else {
		aml_write_cbus(reg, val);
	}
	osd_log_dbg3(MODULE_BASE, "%s(0x%x, 0x%x)\n", __func__, reg, val);
}

u32 osd_reg_read(u32 reg)
{
	u32 ret = 0;
	unsigned int addr = 0;

	/* if (get_backup_reg(reg, &ret) != 0) */
	/* not read from backup */
	if (osd_reg_map.flag) {
		addr = OSDBUS_REG_ADDR(reg);
		ret = readl(osd_reg_map.vir_addr + addr);
	} else {
		ret = (u32)aml_read_vcbus(reg);
	}
	osd_log_dbg3(MODULE_BASE, "%s(0x%x)=0x%x\n", __func__, reg, ret);
	return ret;
}

void osd_reg_write(u32 reg, const u32 val)
{
	unsigned int addr = 0;

	if (osd_reg_map.flag) {
		addr = OSDBUS_REG_ADDR(reg);
		writel(val, osd_reg_map.vir_addr + addr);
	} else {
		aml_write_vcbus(reg, val);
	}
	update_backup_reg(reg, val);
	osd_log_dbg3(MODULE_BASE, "%s(0x%x, 0x%x)\n", __func__, reg, val);
}

void osd_reg_set_mask(u32 reg, const u32 mask)
{
	osd_reg_write(reg, (osd_reg_read(reg) | (mask)));
}

void osd_reg_clr_mask(u32 reg, const u32 mask)
{
	osd_reg_write(reg, (osd_reg_read(reg) & (~(mask))));
}

void osd_reg_set_bits(u32 reg,
		      const u32 value,
		      const u32 start,
		      const u32 len)
{
	osd_reg_write(reg, ((osd_reg_read(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

