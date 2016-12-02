/*
 * drivers/amlogic/amports/decoder/decoder_mmu_box.h
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

#ifndef DECODER_BUFFER_BOX
#define DECODER_BUFFER_BOX

void *decoder_mmu_box_alloc_box(const char *name,
	int channel_id,
	int max_num,
	int min_size_M);

int decoder_mmu_box_alloc_idx(
	void *handle, int idx, int num_pages,
	unsigned int *mmu_index_adr);

int decoder_mmu_box_free_idx_tail(void *handle, int idx,
		int start_release_index);

int decoder_mmu_box_free_idx(void *handle, int idx);

int decoder_mmu_box_free(void *handle);

int decoder_mmu_box_move_keep_idx(void *box_handle,
	int keep_idx);
int decoder_mmu_box_free_keep(int keep_id);
int decoder_mmu_box_free_all_keep(void);
void *decoder_mmu_box_get_mem_handle(void *box_handle, int idx);

#endif

