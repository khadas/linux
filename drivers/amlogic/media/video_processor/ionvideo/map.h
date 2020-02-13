/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __ASM_MACH_MAP_H
#define __ASM_MACH_MAP_H

#include <linux/io.h>

struct map_desc {
	unsigned long virtual;
	unsigned long pfn;
	unsigned long length;
	unsigned int type;
};

/* types 0-3 are defined in asm/io.h */
#define MT_UNCACHED		4
#define MT_CACHECLEAN		5
#define MT_MINICLEAN		6
#define MT_LOW_VECTORS		7
#define MT_HIGH_VECTORS		8
#define MT_MEMORY		9
#define MT_ROM			10
#define MT_MEMORY_NONCACHED	11
#define MT_MEMORY_DTCM		12
#define MT_MEMORY_ITCM		13
#define MT_MEMORY_SO		14
#define MT_MEMORY_DMA_READY	15

#ifdef CONFIG_MMU
void iotable_init(struct map_desc *io_desc, int nr);
void vm_reserve_area_early(unsigned long addr, unsigned long size,
			   void *caller);

#ifdef CONFIG_DEBUG_LL
void debug_ll_addr(unsigned long *paddr, unsigned long *vaddr);
void debug_ll_io_init(void);
#else
static inline void debug_ll_io_init(void) {}
#endif

struct mem_type;
const struct mem_type *get_mem_type(unsigned int type);
/*
 * external interface to remap single page with appropriate type
 */
int ioremap_page(unsigned long virt, unsigned long phys,
		 const struct mem_type *mtype);
#else
#define iotable_init(map, num)	do { } while (0)
#define vm_reserve_area_early(a, s, c)	do { } while (0)
#endif

#endif
