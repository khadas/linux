/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef __CANVAS_UTILS_H__
#define __CANVAS_UTILS_H__

#include "vdec.h"

/* external canvas */
struct canvas_status_s {
	int type;
	int canvas_used_flag;
	int id;
};

#define CANVAS_MAX_SIZE (AMVDEC_CANVAS_MAX1 - AMVDEC_CANVAS_START_INDEX + 1 + AMVDEC_CANVAS_MAX2 + 1)


/* internal canvas */
#define CANVAS_ADDR_LMASK       0x1fffffff
#define CANVAS_WIDTH_LMASK      0x7
#define CANVAS_WIDTH_LWID       3
#define CANVAS_HEIGHT_MASK      0x1fff
#define CANVAS_BLKMODE_MASK     3

#define CAV_WADDR_LBIT       0
#define CAV_WIDTH_LBIT       29
#define CAV_WIDTH_HBIT       0
#define CAV_HEIGHT_HBIT      (41 - 32)
#define CAV_WRAPX_HBIT       (54 - 32)
#define CAV_WRAPY_HBIT       (55 - 32)
#define CAV_BLKMODE_HBIT     (56 - 32)
#define CAV_ENDIAN_HBIT      (58 - 32)

#define MDEC_CAV_LUT_MAX 128
#define MDEC_CAV_INDEX_MASK 0x7f

enum vdec_type_e;

void config_cav_lut(int index, struct canvas_config_s *cfg, enum vdec_type_e core);

void config_cav_lut_ex(u32 index, ulong addr, u32 width,
	u32 height, u32 wrap, u32 blkmode,
	u32 endian, enum vdec_type_e core);

unsigned int vdec_cav_get_width(int index);

unsigned int vdec_cav_get_height(int index);

unsigned long vdec_cav_get_addr(int index);

bool is_support_vdec_canvas(void);

void vdec_canvas_port_register(struct vdec_s *vdec);

#endif

