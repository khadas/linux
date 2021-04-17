/* SPDX-License-Identifier: (GPL-2.0+ OR MIT)*/
/*
 * drivers/amlogic/media/video_processor/ppmgr/decontour.h
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

#include "register.h"

void ini_dcntr_pre(int hsize, int vsize, int grd_num_mode, u32 ratio);
void ini_dcntr_post(int hsize, int vsize, int grd_num_mode);

void set_dcntr_grid_mif(unsigned int x_start,
unsigned int x_end,
unsigned int y_start,
unsigned int y_end,
unsigned int mode,
unsigned int canvas_addr0,
unsigned int canvas_addr1,
unsigned int canvas_addr2,
unsigned int pic_struct,
unsigned int h_avg);

void set_dcntr_grid_fmt(unsigned int hfmt_en,
	unsigned int hz_yc_ratio,
	unsigned int hz_ini_phase,
	unsigned int vfmt_en,
	unsigned int vt_yc_ratio,
	unsigned int vt_ini_phase,
	unsigned int y_length);

void dcntr_grid_rdmif(int canvas_id0,
	int canvas_id1,
	int canvas_id2,
	int canvas_baddr0,
	int canvas_baddr1,
	int canvas_baddr2,
	int src_hsize,
	int src_vsize,
	int src_fmt,
	int mif_x_start,
	int mif_x_end,
	int mif_y_start,
	int mif_y_end,
	int mif_reverse,
	int pic_struct,
	int h_avg);

void dcntr_grid_wrmif(int mif_index,
	int mem_mode,
	int src_fmt,
	int canvas_id,
	int mif_x_start,
	int mif_x_end,
	int mif_y_start,
	int mif_y_end,
	int swap_64bit,
	int mif_reverse,
	int linear_baddr,
	int linear_length);

