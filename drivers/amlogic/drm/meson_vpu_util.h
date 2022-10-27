/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_VPU_UTIL_H
#define __MESON_VPU_UTIL_H

#include <drm/drmP.h>
#include <linux/types.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/iomap.h>

#define LINE_THRESHOLD 90
#define WAIT_CNT_MAX 100
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

u32 meson_vpu_read_reg(u32 addr);
int meson_vpu_write_reg(u32 addr, u32 val);
int meson_vpu_write_reg_bits(u32 addr, u32 val, u32 start, u32 len);
u32 meson_drm_read_reg(u32 addr);
void meson_drm_write_reg(u32 addr, u32 val);
void meson_drm_write_reg_bits(u32 addr, u32 val, u32 start, u32 len);

void meson_drm_canvas_config(u32 index, unsigned long addr, u32 width,
			     u32 height, u32 wrap, u32 blkmode);
int meson_drm_canvas_pool_alloc_table(const char *owner, u32 *table, int size,
				      enum canvas_map_type_e type);
void set_video_enabled(u32 value, u32 index);
void meson_vpu_reg_handle_register(u32 vpp_index);
int meson_vpu_reg_vsync_config(u32 vpp_index);
void meson_vpu_power_config(enum vpu_mod_e mode, bool en);
void osd_vpu_power_on(void);
#endif
