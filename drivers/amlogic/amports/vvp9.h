/*
 * drivers/amlogic/amports/vvp9.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef VVP9_H
#define VVP9_H

#ifdef VP9_10B_MMU
void init_mmu_spec(void);
void alloc_mmu(int cur_buf_idx, int pic_width, int pic_height,
		unsigned short bit_depth, unsigned int *mmu_index_adr);
void release_unused_4k(long used_4k_num, int cur_buf_idx);
void release_buffer_4k(int cur_buf_idx);
int  compute_losless_comp_body_size(int width, int height,
		unsigned char bit_depth_10);
#endif
void adapt_coef_probs(int pic_count, int prev_kf, int cur_kf, int pre_fc,
unsigned int *prev_prob, unsigned int *cur_prob, unsigned int *count);
#endif
