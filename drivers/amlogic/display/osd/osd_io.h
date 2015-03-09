/*
 * Amlogic Frame buffer driver
 *
 * Copyright (C) 2015 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author: Amlogic Platform-BJ <platform.bj@amlogic.com>
 *
 */

#ifndef _OSD_IO_H_
#define _OSD_IO_H_

#include <linux/amlogic/iomap.h>

static inline uint32_t osd_reg_read(uint32_t reg)
{
	return (uint32_t)aml_read_vcbus(reg);
};

static inline void osd_reg_write(uint32_t reg,
				 const uint32_t val)
{
	aml_write_vcbus(reg, val);
};

static inline void osd_reg_set_mask(uint32_t reg,
				    const uint32_t mask)
{
	osd_reg_write(reg, (osd_reg_read(reg) | (mask)));
}

static inline void osd_reg_clr_mask(uint32_t reg,
				    const uint32_t mask)
{
	osd_reg_write(reg, (osd_reg_read(reg) & (~(mask))));
}

static inline void osd_reg_set_bits(uint32_t reg,
				    const uint32_t value,
				    const uint32_t start,
				    const uint32_t len)
{
	osd_reg_write(reg, ((osd_reg_read(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}


#ifdef CONFIG_FB_OSD_VSYNC_RDMA
u32 VSYNCOSD_RD_MPEG_REG(u32 reg);
int VSYNCOSD_WR_MPEG_REG(u32 reg, u32 val);
int VSYNCOSD_WR_MPEG_REG_BITS(u32 reg, u32 val, u32 start, u32 len);
int VSYNCOSD_SET_MPEG_REG_MASK(u32 reg, u32 mask);
int VSYNCOSD_CLR_MPEG_REG_MASK(u32 reg, u32 mask);
#else
#define VSYNCOSD_RD_MPEG_REG(reg) osd_reg_read(reg)
#define VSYNCOSD_WR_MPEG_REG(reg, val) osd_reg_write(reg, val)
#define VSYNCOSD_WR_MPEG_REG_BITS(reg, val, start, len) \
	osd_reg_set_bits(reg, val, start, len)
#define VSYNCOSD_SET_MPEG_REG_MASK(reg, mask) osd_reg_set_mask(reg, mask)
#define VSYNCOSD_CLR_MPEG_REG_MASK(reg, mask) osd_reg_clr_mask(reg, mask)
#endif

#endif
