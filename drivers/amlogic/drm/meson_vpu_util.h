/*
 * drivers/amlogic/drm/meson_vpu_util.h
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

#ifndef __MESON_VPU_UTIL_H
#define __MESON_VPU_UTIL_H

#include <linux/types.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/iomap.h>
#include "osd_rdma.h"

/*osd internal channel*/
enum din_channel_e {
	DIN0 = 0,
	DIN1,
	DIN2,
	DIN3
};

struct osd_scope_s {
	u32 h_start;
	u32 h_end;
	u32 v_start;
	u32 v_end;
};

u32 meson_util_rdma_read_reg(u32 addr);
int meson_util_rdma_write_reg(u32 addr, u32 val);
int meson_util_rdma_write_reg_bits(u32 addr, u32 val, u32 start, u32 len);
int meson_util_rdma_set_reg_mask(u32 addr, u32 mask);
int meson_util_rdma_clr_reg_mask(u32 addr, u32 mask);
int meson_util_rdma_irq_write_reg(u32 addr, u32 val);
u32 meson_drm_read_reg(u32 addr);

void meson_util_canvas_config(u32 index, unsigned long addr, u32 width,
					u32 height, u32 wrap, u32 blkmode);
int meson_util_canvas_pool_alloc_table(const char *owner, u32 *table, int size,
					enum canvas_map_type_e type);

#endif
