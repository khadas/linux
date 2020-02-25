/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_SYS_H__
#define __DI_SYS_H__

#define DEVICE_NAME		"di_multi"
#define CLASS_NAME		"deinterlace"

u8 *dim_vmap(ulong addr, u32 size, bool *bflg);
void dim_unmap_phyaddr(u8 *vaddr);
void dim_mcinfo_v_alloc(struct di_buf_s *pbuf, unsigned int bsize);
void dim_mcinfo_v_release(struct di_buf_s *pbuf);
struct dim_mm_s {
	struct page	*ppage;
	unsigned long	addr;
};

bool dim_mm_alloc_api(int cma_mode, size_t count, struct dim_mm_s *o);
bool dim_mm_release_api(int cma_mode,
			struct page *pages,
			int count,
			unsigned long addr);
bool dim_cma_top_alloc(unsigned int ch);
bool dim_cma_top_release(unsigned int ch);
bool dim_rev_mem_check(void);
/*--Different DI versions flag---*/
void dil_set_diffver_flag(unsigned int para);

unsigned int dil_get_diffver_flag(void);
/*-------------------------*/
#endif	/*__DI_SYS_H__*/
