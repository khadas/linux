// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#include "frc_drv.h"

void __iomem *frc_base;

//#define FRC_DISABLE_REG_RD_WR

void WRITE_FRC_REG(unsigned int reg, unsigned int val)
{
#ifndef FRC_DISABLE_REG_RD_WR
	if (get_frc_devp()->power_on_flag == 0)
		return;
	writel(val, (frc_base + (reg << 2)));
#endif
}
EXPORT_SYMBOL(WRITE_FRC_REG);


void WRITE_FRC_BITS(unsigned int reg, unsigned int value,
					   unsigned int start, unsigned int len)
{
#ifndef FRC_DISABLE_REG_RD_WR
	unsigned int tmp, orig;
	unsigned int mask = (((1L << len) - 1) << start);
	int r = (reg << 2);
	if (get_frc_devp()->power_on_flag == 0)
		return;
	orig =  readl((frc_base + r));
	tmp = orig  & ~mask;
	tmp |= (value << start) & mask;
	writel(tmp, (frc_base + r));
#endif
}
EXPORT_SYMBOL(WRITE_FRC_BITS);


void UPDATE_FRC_REG_BITS(unsigned int reg,
	unsigned int value,
	unsigned int mask)
{
#ifndef FRC_DISABLE_REG_RD_WR
	unsigned int val;

	if (get_frc_devp()->power_on_flag == 0)
		return;
	value &= mask;
	val = readl(frc_base + (reg << 2));
	val &= ~mask;
	val |= value;
	writel(val, (frc_base + (reg << 2)));
#endif
}
EXPORT_SYMBOL(UPDATE_FRC_REG_BITS);


int READ_FRC_REG(unsigned int reg)
{
#ifndef FRC_DISABLE_REG_RD_WR
	if (get_frc_devp()->power_on_flag == 0)
		return 0;
	return readl(frc_base + (reg << 2));
#else
	return 0;
#endif
}
EXPORT_SYMBOL(READ_FRC_REG);

u32 READ_FRC_BITS(u32 reg, const u32 start, const u32 len)
{
	u32 val = 0;

#ifndef FRC_DISABLE_REG_RD_WR
	if (get_frc_devp()->power_on_flag == 0)
		return 0;
	val = ((READ_FRC_REG(reg) >> (start)) & ((1L << (len)) - 1));
#endif
	return val;
}
EXPORT_SYMBOL(READ_FRC_BITS);

u32 floor_rs(u32 ix, u32 rs)
{
	u32 rst = 0;

	rst = (ix) >> rs;

	return rst;
}
EXPORT_SYMBOL(floor_rs);

u32 ceil_rx(u32 ix, u32 rs)
{
	u32 rst = 0;
	u32 tmp = 0;

	tmp = 1 << rs;
	rst = (ix + tmp - 1) >> rs;

	return rst;
}
EXPORT_SYMBOL(ceil_rx);

s32 negative_convert(s32 data, u32 fbits)
{
	s32 rst = 0;
	s64 sign_base = (s64)1 << (fbits - 1);

	if ((data & sign_base) == sign_base)
		rst = -((sign_base << 1) - data);
	else
		rst = data;

	return rst;
}
EXPORT_SYMBOL(negative_convert);

