// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include "lut_dma_io.h"

u32 lut_dma_reg_read(u32 reg)
{
	u32 ret = 0;

	ret = (u32)aml_read_vcbus(reg);
	return ret;
};

void lut_dma_reg_write(u32 reg, const u32 val)
{
	aml_write_vcbus(reg, val);
};

void lut_dma_reg_set_mask(u32 reg,
			  const u32 mask)
{
	lut_dma_reg_write(reg, (lut_dma_reg_read(reg) | (mask)));
}

void lut_dma_reg_clr_mask(u32 reg,
			  const u32 mask)
{
	lut_dma_reg_write(reg, (lut_dma_reg_read(reg) & (~(mask))));
}

void lut_dma_reg_set_bits(u32 reg,
			  const u32 value,
			  const u32 start,
			  const u32 len)
{
	lut_dma_reg_write(reg, ((lut_dma_reg_read(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

