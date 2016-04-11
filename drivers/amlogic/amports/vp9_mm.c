 /*
 * drivers/amlogic/amports/vp9_mm.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/amports/vformat.h>
#include <linux/amlogic/amports/ptsserv.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/slab.h>
#include "amports_priv.h"
#include <linux/amlogic/codec_mm/codec_mm.h>

/*#define VP9_10B_MMU*/
#include "vvp9.h"



#ifdef VP9_10B_MMU
  /* to change ... */
#define MC_BUFFER_START_4K      (0)

#if 0
enum vpx_bit_depth {
	VPX_BITS_8  =  8,  /**<  8 bits */
	VPX_BITS_10 = 10,  /**< 10 bits */
	VPX_BITS_12 = 12,  /**< 12 bits */
};
#endif
#define REF_FRAMES_LOG2 3
#define REF_FRAMES (1 << REF_FRAMES_LOG2)
#define FRAME_BUFFERS (REF_FRAMES + 7)

#define TOTAL_4K_NUM 0x8000
/* compute_losless_comp_body_size(4096, 2304, 1) = 18874368(0x1200000)*/
#define MAX_FRAME_4K_NUM 0x1200
int mmu_offset_seed;
int mmu_4k_status[TOTAL_4K_NUM];
struct MMU_BUFF_CONFIG {
	long mmu_4k_number;
	unsigned int  mmu_4k_index[MAX_FRAME_4K_NUM];
};

struct MMU_BUFF_CONFIG mmu_buf[FRAME_BUFFERS];
int mmu_pic_count;
unsigned long cur_mem_usage;
unsigned long max_mem_usage;

void init_mmu_spec(void)
{
	int i, j;
	for (i = 0; i < TOTAL_4K_NUM; i++)
		mmu_4k_status[i] = 0;
	for (i = 0; i < FRAME_BUFFERS; i++) {
		mmu_buf[i].mmu_4k_number = 0;
		for (j = 0; j < FRAME_BUFFERS; j++)
			mmu_buf[i].mmu_4k_index[j] = 0;
	};
	mmu_offset_seed = 0;
	cur_mem_usage = 0;
	max_mem_usage = 0;
	mmu_pic_count = -1;
}

void alloc_mmu(int cur_buf_idx, int pic_width, int pic_height,
		unsigned short bit_depth, unsigned int *mmu_index_adr) {
	/*int cur_buf_idx = cm->new_fb_idx;*/
	int bit_depth_10 = (bit_depth == VPX_BITS_10);
	int picture_size;
	int cur_mmu_4k_number;
	int prefer_4k_position;
	int i;
	pr_info
	("MMU alloc new_fb_idx : %d (width: %d, height: %d, bit_depth: %d)\r\n",
	cur_buf_idx, pic_width, pic_height, bit_depth);

	picture_size =
	compute_losless_comp_body_size(pic_width, pic_height, bit_depth_10);
	cur_mmu_4k_number = ((picture_size+(1<<12)-1) >> 12);
	pr_info("MMU allo picture_size : %d(0x%x) mmu_4k_number: %d(0x%x) \r\n",
	picture_size, picture_size, cur_mmu_4k_number, cur_mmu_4k_number);

	mmu_buf[cur_buf_idx].mmu_4k_number = cur_mmu_4k_number;
	for (i = 0; i < cur_mmu_4k_number; i++) {
		prefer_4k_position = (mmu_offset_seed & 0xff) +
			(
			((i & 0xf) << 11) |  /* bit[14:11]*/
			(((i >> 4) & 0xf) << 7) |  /*bit[10:7]*/
			(((i >> 8) & 0xf) << 3) |  /* bit[6:3]*/
			(((i >> 12) & 0x7) << 0)    /* bit[2:0]*/
			);
		if ((prefer_4k_position >= TOTAL_4K_NUM)
			| (mmu_4k_status[prefer_4k_position] != 0)) {
			for (prefer_4k_position = 0;
				prefer_4k_position < TOTAL_4K_NUM;
				prefer_4k_position++) {
				if (mmu_4k_status[prefer_4k_position] == 0)
					break;
				}
			if (prefer_4k_position == TOTAL_4K_NUM)
				pr_err
				("ERROR : NOT Enough 4K MMU Memory !!!!\r\n");
		}
		mmu_4k_status[prefer_4k_position] = 1;
		mmu_buf[cur_buf_idx].mmu_4k_index[i] =
				MC_BUFFER_START_4K + prefer_4k_position;
		pr_info("MMU alloc  mmu_4k_index[%d] : 0x%x\r\n", i,
				mmu_buf[cur_buf_idx].mmu_4k_index[i]);
	}

	mmu_offset_seed++;
	/*copyToDDR_32bits(FRAME_MMU_MAP_ADDR,
		mmu_buf[cur_buf_idx].mmu_4k_index, cur_mmu_4k_number*4, 0);*/
	for (i = 0; i < cur_mmu_4k_number; i++)
		mmu_index_adr[i] = mmu_buf[cur_buf_idx].mmu_4k_index[i];

	mmu_pic_count++;
	cur_mem_usage += cur_mmu_4k_number*4096;
	if (max_mem_usage < cur_mem_usage)
		max_mem_usage = cur_mem_usage;
	pr_info("MMU USAGE current : %ld, max : %ld (P%0d)\r\n",
			cur_mem_usage, max_mem_usage, mmu_pic_count);

}

void release_unused_4k(long used_4k_num, int cur_buf_idx)
{
	long release_4k_position;
	int i;
	if (mmu_pic_count < 0)
		return;

	mmu_offset_seed = used_4k_num & 0xff;
	if (used_4k_num > mmu_buf[cur_buf_idx].mmu_4k_number) {
		pr_err
		("MMU ERROR:Use more 4K Page than allocated %ld>([%d]=%ld)\r\n",
		used_4k_num, cur_buf_idx, mmu_buf[cur_buf_idx].mmu_4k_number);
	} else
		pr_info
		("MMU RELEASE: P%d(buffer%d)used %ld of %ld 4k buff(%ld%c)\r\n",
		mmu_pic_count, cur_buf_idx, used_4k_num,
		mmu_buf[cur_buf_idx].mmu_4k_number,
		used_4k_num*100/mmu_buf[cur_buf_idx].mmu_4k_number, '%');

	for (i = used_4k_num; i < mmu_buf[cur_buf_idx].mmu_4k_number; i++) {
		release_4k_position =
		mmu_buf[cur_buf_idx].mmu_4k_index[i] - MC_BUFFER_START_4K;
		mmu_4k_status[release_4k_position] = 0;
	}
	cur_mem_usage -=
		(mmu_buf[cur_buf_idx].mmu_4k_number - used_4k_num)*4096;
	mmu_buf[cur_buf_idx].mmu_4k_number = used_4k_num;

}

void release_buffer_4k(int cur_buf_idx)
{
	/*long used_4k_num;*/
	long release_4k_position;
	int i;

	if (mmu_buf[cur_buf_idx].mmu_4k_number == 0)
		return;
	pr_info("[MMU MEM RECYCLE] : buffer %d (%ld 4K Buffer)\r\n",
		cur_buf_idx, mmu_buf[cur_buf_idx].mmu_4k_number);

	for (i = 0; i < mmu_buf[cur_buf_idx].mmu_4k_number; i++) {
		release_4k_position = mmu_buf[cur_buf_idx].mmu_4k_index[i]
			- MC_BUFFER_START_4K;
		mmu_4k_status[release_4k_position] = 0;
	}
	cur_mem_usage -= mmu_buf[cur_buf_idx].mmu_4k_number * 4096;
	mmu_buf[cur_buf_idx].mmu_4k_number = 0;

}

#endif
