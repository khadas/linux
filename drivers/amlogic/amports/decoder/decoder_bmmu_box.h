/*
 * drivers/amlogic/amports/decoder/decoder_bmmu_box.h
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

#ifndef DECODER_BLOCK_BUFFER_BOX
#define DECODER_BLOCK_BUFFER_BOX

void *decoder_bmmu_box_alloc_box(const char *name,
								 int channel_id,
								 int max_num,
								 int aligned,
								 int mem_flags);

int decoder_bmmu_box_alloc_idx(
	void *handle, int idx, int size,
	int aligned_2n, int mem_flags);

int decoder_bmmu_box_free_idx(void *handle, int idx);
int decoder_bmmu_box_free(void *handle);
void *decoder_bmmu_box_get_mem_handle(
	void *box_handle, int idx);

unsigned long decoder_bmmu_box_get_phy_addr(
	void *box_handle, int idx);

void *decoder_bmmu_box_get_virt_addr(
	void *box_handle, int idx);

/*flags: &0x1 for wait,*/
int decoder_bmmu_box_check_and_wait_size(
	int size, int flags);

int decoder_bmmu_box_alloc_buf_phy(
	void *handle, int idx,
	int size, unsigned char *driver_name,
	unsigned long *buf_phy_addr);

#define BMMU_ALLOC_FLAGS_WAIT (1 << 0)
#define BMMU_ALLOC_FLAGS_CAN_CLEAR_KEEPER (1 << 1)
#define BMMU_ALLOC_FLAGS_WAITCLEAR \
		(BMMU_ALLOC_FLAGS_WAIT |\
		BMMU_ALLOC_FLAGS_CAN_CLEAR_KEEPER)

int decoder_bmmu_box_alloc_idx_wait(
	void *handle, int idx,
	int size, int aligned_2n,
	int mem_flags,
	int wait_flags);

#endif	/*
 */

