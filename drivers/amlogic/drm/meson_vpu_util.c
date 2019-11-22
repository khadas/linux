/*
 * drivers/amlogic/drm/meson_vpu_util.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include "meson_vpu_util.h"

/*****drm reg access by rdma*****/

u32 meson_drm_rdma_read_reg(u32 addr)
{
	return VSYNCOSD_RD_MPEG_REG(addr);
}

int meson_drm_rdma_write_reg(u32 addr, u32 val)
{
	return VSYNCOSD_WR_MPEG_REG(addr, val);
}

int meson_drm_rdma_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
	return VSYNCOSD_WR_MPEG_REG_BITS(addr, val, start, len);
}

int meson_drm_rdma_set_reg_mask(u32 addr, u32 mask)
{
	return VSYNCOSD_SET_MPEG_REG_MASK(addr, mask);
}

int meson_drm_rdma_clr_reg_mask(u32 addr, u32 mask)
{
	return VSYNCOSD_CLR_MPEG_REG_MASK(addr, mask);
}

int meson_drm_rdma_irq_write_reg(u32 addr, u32 val)
{
	return VSYNCOSD_IRQ_WR_MPEG_REG(addr, val);
}

/** reg direct access without rdma **/

u32 meson_drm_read_reg(u32 addr)
{
	u32 val;

	val = aml_read_vcbus(addr);

	return val;
}

void meson_drm_write_reg(u32 addr, u32 val)
{
	aml_write_vcbus(addr, val);
}
#if 0
int meson_drm_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
	int ret;
	u32 raw_val;

	ret = aml_reg_read(IO_VAPB_BUS_BASE, addr << 2, &raw_val);
	if (ret) {
		pr_err("read vcbus reg %x error %d\n", addr, ret);
		return -1;
	}

	raw_val |= val & GENMASK(start, start + len);

	ret = aml_reg_write(IO_VAPB_BUS_BASE, addr << 2, raw_val);
	if (ret) {
		pr_err("write vcbus reg %x error %d\n", addr, ret);
		return -1;
	}
	return 0;
}

int meson_drm_set_reg_mask(u32 addr, u32 mask)
{
	int ret;
	u32 raw_val;

	ret = aml_reg_read(IO_VAPB_BUS_BASE, addr << 2, &raw_val);
	if (ret) {
		pr_err("read vcbus reg %x error %d\n", addr, ret);
		return -1;
	}

	raw_val |= mask;

	ret = aml_reg_write(IO_VAPB_BUS_BASE, addr << 2, raw_val);
	if (ret) {
		pr_err("write vcbus reg %x error %d\n", addr, ret);
		return -1;
	}
	return 0;
}

int meson_drm_clr_reg_mask(u32 addr, u32 mask)
{
	int ret;
	u32 raw_val;

	ret = aml_reg_read(IO_VAPB_BUS_BASE, addr << 2, &raw_val);
	if (ret) {
		pr_err("read vcbus reg %x error %d\n", addr, ret);
		return -1;
	}

	raw_val &= ~mask;

	ret = aml_reg_write(IO_VAPB_BUS_BASE, addr << 2, raw_val);
	if (ret) {
		pr_err("write vcbus reg %x error %d\n", addr, ret);
		return -1;
	}
	return 0;
}
#endif

/** canvas config  **/

void meson_drm_canvas_config(u32 index, unsigned long addr, u32 width,
			     u32 height, u32 wrap, u32 blkmode)
{
	canvas_config(index, addr, width, height, wrap, blkmode);
}

int meson_drm_canvas_pool_alloc_table(const char *owner, u32 *table, int size,
				      enum canvas_map_type_e type)
{
	return canvas_pool_alloc_canvas_table(owner, table, size, type);
}

