/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/dolby_sys.h
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

#ifndef __DOLBY_SYS_H__
#define __DOLBY_SYS_H__
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/cdev.h>

#define DIDEF_G2L_LUT_SIZE           0x100

struct didm_lut_ipcore_s {
	u32 tmluti[64 * 4];
	u32 tmluts[64 * 4];
	u32 smluti[64 * 4];
	u32 smluts[64 * 4];
	u32 G2L[DIDEF_G2L_LUT_SIZE];
};

struct didm_register_ipcore_1_s {
	u32 srange;
	u32 srange_inverse;
	u32 frame_format_1;
	u32 frame_format_2;
	u32 frame_pixel_def;
	u32 y2rgb_coefficient_1;
	u32 y2rgb_coefficient_2;
	u32 y2rgb_coefficient_3;
	u32 y2rgb_coefficient_4;
	u32 y2rgb_coefficient_5;
	u32 y2rgb_offset_1;
	u32 y2rgb_offset_2;
	u32 y2rgb_offset_3;
	u32 eotf;
/*	u32 Sparam_1;*/
/*	u32 Sparam_2;*/
/*	u32 Sgamma; */
	u32 a2b_coefficient_1;
	u32 a2b_coefficient_2;
	u32 a2b_coefficient_3;
	u32 a2b_coefficient_4;
	u32 a2b_coefficient_5;
	u32 c2d_coefficient_1;
	u32 c2d_coefficient_2;
	u32 c2d_coefficient_3;
	u32 c2d_coefficient_4;
	u32 c2d_coefficient_5;
	u32 c2d_offset;
	u32 active_area_left_top;
	u32 active_area_bottom_right;
};

struct dicomposer_register_ipcore_s {
	/* offset 0xc8 */
	u32 composer_mode;
	u32 vdr_resolution;
	u32 bit_depth;
	u32 coefficient_log2_denominator;
	u32 bl_num_pivots_y;
	u32 bl_pivot[5];
	u32 bl_order;
	u32 bl_coefficient_y[8][3];
	u32 el_nlq_offset_y;
	u32 el_coefficient_y[3];
	u32 mapping_idc_u;
	u32 bl_num_pivots_u;
	u32 bl_pivot_u[3];
	u32 bl_crder_u;
	u32 bl_coefficient_u[4][3];
	u32 mmr_coefficient_u[22][2];
	u32 mmr_order_u;
	u32 el_nlq_offset_u;
	u32 el_coefficient_u[3];
	u32 mapping_idc_v;
	u32 bl_num_pivots_v;
	u32 bl_pivot_v[3];
	u32 bl_order_v;
	u32 bl_coefficient_v[4][3];
	u32 mmr_coefficient_v[22][2];
	u32 mmr_order_V;
	u32 el_nlq_offset_v;
	u32 el_coefficient_v[3];
};

#define DOLBY_CORE1C_REG_START                     0x1800
#define DOLBY_CORE1C_CLKGATE_CTRL                  0x18f2
#define DOLBY_CORE1C_SWAP_CTRL0                    0x18f3
#define DOLBY_CORE1C_SWAP_CTRL1                    0x18f4
#define DOLBY_CORE1C_SWAP_CTRL2                    0x18f5
#define DOLBY_CORE1C_SWAP_CTRL3                    0x18f6
#define DOLBY_CORE1C_SWAP_CTRL4                    0x18f7
#define DOLBY_CORE1C_SWAP_CTRL5                    0x18f8
#define DOLBY_CORE1C_DMA_CTRL                      0x18f9
#define DOLBY_CORE1C_DMA_STATUS                    0x18fa
#define DOLBY_CORE1C_STATUS0                       0x18fb
#define DOLBY_CORE1C_STATUS1                       0x18fc
#define DOLBY_CORE1C_STATUS2                       0x18fd
#define DOLBY_CORE1C_STATUS3                       0x18fe
#define DOLBY_CORE1C_DMA_PORT                      0x18ff

#define DI_SC_TOP_CTRL                             0x374f
#define DI_HDR_IN_HSIZE                            0x376e
#define DI_HDR_IN_VSIZE                            0x376f

void di_dolby_sw_init(void);
int di_dolby_do_setting(void);
void di_dolby_enable(bool enable);
#endif

