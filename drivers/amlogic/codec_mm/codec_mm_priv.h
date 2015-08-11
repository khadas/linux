/*
 * drivers/amlogic/codec_mm/codec_mm.h
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

#ifndef AML_MM_HEADER
#define AML_MM_HEADER
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/list.h>

struct codec_mm_s {

	/*can be shared by many user */
	const char *owner[8];

	/*virtual buffer of this memory */
	char *vbuffer;

	void *mem_handle;	/*used for top level.alloc/free */

	ulong phy_addr;		/*if phy continue or one page only */

	int buffer_size;

	int page_count;

	int align2n;

	/*if vbuffer is no cache set
	   AMPORTS_MEM_FLAGS_NOCACHED  to flags */
#define AMPORTS_MEM_FLAGS_NOCACHED (1<<0)
	/*phy continue,need dma
	 */
#define AMPORTS_MEM_FLAGS_DMA (1<<1)
	int flags;

#define AMPORTS_MEM_FLAGS_FROM_SYS 1
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES 2
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED 3
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA 4
#define AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP 5

	int from_flags;

	/*may can be shared on many user..
	   decoder/di/ppmgr,
	 */
	atomic_t use_cnt;

	spinlock_t lock;

	struct list_head list;

};
extern void dma_clear_buffer(struct page *page, size_t size);
#endif				/*
				 */
