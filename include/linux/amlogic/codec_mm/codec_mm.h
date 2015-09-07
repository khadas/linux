
/*
 * include/linux/amlogic/codec_mm/codec_mm.h
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

#ifndef CODEC_MM_API_HEADER
#define CODEC_MM_API_HEADER
#include <linux/dma-direction.h>

/*
memflags
*/
#define CODEC_MM_FLAGS_DMA 1
#define CODEC_MM_FLAGS_CPU 2
#define CODEC_MM_FLAGS_TVP 4

#define CODEC_MM_FLAGS_RESERVED 0x100
#define CODEC_MM_FLAGS_CMA		0x200


/*if cma,
clear thie buffer cache.
*/
#define CODEC_MM_FLAGS_CMA_CLEAR 0x10000


#define CODEC_MM_FLAGS_FROM_MASK \
	(CODEC_MM_FLAGS_DMA |\
	CODEC_MM_FLAGS_CPU |\
	CODEC_MM_FLAGS_TVP)

#define CODEC_MM_FLAGS_DMA_CPU  (CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CPU)
#define CODEC_MM_FLAGS_ANY	CODEC_MM_FLAGS_DMA_CPU





unsigned long codec_mm_alloc_for_dma(const char *owner, int page_cnt,
	int align2n, int memflags);
int codec_mm_free_for_dma(const char *owner, unsigned long phy_addr);

void *codec_mm_phys_to_virt(unsigned long phy_addr);
unsigned long codec_mm_virt_to_phys(void *vaddr);

void codec_mm_dma_flush(void *vaddr,
	int size,
	enum dma_data_direction dir);




int codec_mm_get_total_size(void);
int codec_mm_get_free_size(void);



#endif	/*
 */

