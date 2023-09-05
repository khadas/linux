// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/delay.h>

#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_scatter.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include <linux/amlogic/media/utils/vdec_reg.h>

#include "codec_mm_priv.h"
#include "codec_mm_scatter_priv.h"
#include "codec_mm_keeper_priv.h"
#include "codec_mm_track_priv.h"
#include <linux/highmem.h>
#include <linux/page-flags.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/cpu_version.h>

#define TVP_POOL_NAME "TVP_POOL"
#define CMA_RES_POOL_NAME "CMA_RES"

#define CONFIG_PATH "media.codec_mm"
#define CONFIG_PREFIX "media"

#define MM_ALIGN_DOWN_2N(addr, alg2n)  ((addr) & (~((1 << (alg2n)) - 1)))

#define RES_IS_MAPED
#define DEFAULT_TVP_SIZE_FOR_4K (236 * SZ_1M)
#define DEFAULT_TVP_SIZE_FOR_NO4K (160 * SZ_1M)
#define DEFAULT_TVP_SEGMENT_MIN_SIZE (16 * SZ_1M)

#define ALLOC_MAX_RETRY 1

#define CODEC_MM_FOR_DMA_ONLY(flags) \
	(((flags) & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_DMA)

#define CODEC_MM_FOR_CPU_ONLY(flags) \
	(((flags) & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_CPU)

#define CODEC_MM_FOR_DMACPU(flags) \
	(((flags) & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_DMA_CPU)

#define RESERVE_MM_ALIGNED_2N	17
#define TVP_MM_ALIGNED_2N	16

#define RES_MEM_FLAGS_HAVE_MAPED 0x4

#define MAP_RANGE (SZ_1M)

#ifdef CONFIG_PRINTK_CALLER
#define PREFIX_MAX		48
#else
#define PREFIX_MAX		32
#endif
#define LOG_LINE_MAX		(1536 - PREFIX_MAX)

static void dump_mem_infos(void);
static int dump_free_mem_infos(void *buf, int size);
static int __init secure_vdec_res_setup(struct reserved_mem *rmem);

static inline u32 codec_mm_align_up2n(u32 addr, u32 alg2n)
{
	return ((addr + (1 << alg2n) - 1) & (~((1 << alg2n) - 1)));
}

/*
 *debug_mode:
 *
 *disable reserved:1
 *disable cma:2
 *disable sys memory:4
 *disable half memory:8,
 *	only used half memory,for debug.
 *	return nomem,if alloced > total/2;
 *dump memory info on failed:0x10,
 *trace memory alloc/free info:0x20,
 */
static u32 debug_mode;

static u32 debug_sc_mode;
u32 codec_mm_get_sc_debug_mode(void)
{
	return debug_sc_mode;
}
EXPORT_SYMBOL(codec_mm_get_sc_debug_mode);

static u32 debug_keep_mode;
u32 codec_mm_get_keep_debug_mode(void)
{
	return debug_keep_mode;
}
EXPORT_SYMBOL(codec_mm_get_keep_debug_mode);

static u32 default_tvp_size;
static u32 default_tvp_4k_size;
static u32 default_cma_res_size;
static u32 default_tvp_pool_segment_size[4];
static u32 default_tvp_pool_size_0;
static u32 default_tvp_pool_size_1;
static u32 default_tvp_pool_size_2;
static u32 tvp_dynamic_increase_disable;
static u32 tvp_dynamic_alloc_max_size;
static u32 tvp_dynamic_alloc_force_small_segment;
static u32 tvp_dynamic_alloc_force_small_segment_size;
static bool dbuf_trace;
static u32 tvp_pool_early_release_switch;

#define TVP_POOL_SEGMENT_MAX_USED 4
#define TVP_MAX_SLOT 8
/*
 *tvp_mode == 0 means protect secure memory in secmem ta
 *tvp_mode == 1 means use protect secure memory in codec_mm.
 */
static u32 tvp_mode;

struct extpool_mgt_s {
	struct gen_pool *gen_pool[TVP_MAX_SLOT];
	struct codec_mm_s *mm[TVP_MAX_SLOT];
	int slot_num;
	int alloced_size;
	int total_size;
	/* mutex lock */
	struct mutex pool_lock;
	struct task_struct *tvp_kthread;
};

struct codec_mm_mgt_s {
	struct cma *cma;
	struct device *dev;
	struct list_head mem_list;
	struct gen_pool *res_pool;
	struct extpool_mgt_s tvp_pool;
	struct extpool_mgt_s cma_res_pool;
	struct reserved_mem rmem;
	int total_codec_mem_size;
	int total_alloced_size;
	int total_cma_size;
	int total_reserved_size;
	int max_used_mem_size;

	int alloced_res_size;
	int alloced_cma_size;
	int alloced_sys_size;
	int alloced_for_sc_size;
	int alloced_for_sc_cnt;
	int alloced_from_coherent;
	int phys_vmaped_page_cnt;

	int alloc_from_sys_pages_max;
	int enable_kmalloc_on_nomem;
	int res_mem_flags;
	int global_memid;
	/*1:for 1080p,2:for 4k*/
	int tvp_enable;
	/*1:for 1080p,2:for 4k */
	int fastplay_enable;
	int codec_mm_for_linear;
	int codec_mm_scatter_watermark;
	int codec_mm_scatter_free_size;
	/* spin lock for protect scatter_watermark */
	spinlock_t scatter_watermark_lock;
	/* spin lock */
	spinlock_t lock;
	atomic_t tvp_user_count;
	/* for tvp operator used */
	struct mutex tvp_protect_lock;
	void *trk_h;
};

#define PHY_OFF() offsetof(struct codec_mm_s, phy_addr)
#define HANDLE_OFF() offsetof(struct codec_mm_s, mem_handle)
#define VADDR_OFF() offsetof(struct codec_mm_s, vbuffer)
#define VAL_OFF_VAL(mem, off) (*(unsigned long *)((unsigned long)(mem) + (off)))

static int codec_mm_extpool_pool_release(struct extpool_mgt_s *tvp_pool);
static int codec_mm_tvp_pool_alloc_by_slot(struct extpool_mgt_s *tvp_pool, int memflags, int flags);
static int codec_mm_tvp_pool_unprotect_and_release(struct extpool_mgt_s *tvp_pool);

static struct codec_mm_mgt_s *get_mem_mgt(void)
{
	static struct codec_mm_mgt_s mgt;
	static int inited;
	int ret = 0;
	int handle = 0;

	if (!inited) {
		memset(&mgt, 0, sizeof(struct codec_mm_mgt_s));
		/*If tee_protect_tvp_mem is not implement
		 *will return 0xFFFFFFF we used to init use
		 *which mode
		 */
		ret = tee_protect_tvp_mem(0, 0, &handle);
		if (ret == 0xFFFFFFFF) {
			tvp_mode = 0;
			tvp_dynamic_increase_disable = 1;
		} else
			tvp_mode = 1;
		mutex_init(&mgt.tvp_protect_lock);
		inited++;
	}
	return &mgt;
};

static void *codec_mm_extpool_alloc(struct extpool_mgt_s *tvp_pool,
				    void **from_pool, int size)
{
	int i = 0;
	void *handle = NULL;

	for (i = 0; i < tvp_pool->slot_num; i++) {
		if (!tvp_pool->gen_pool[i])
			return NULL;
		handle = (void *)gen_pool_alloc(tvp_pool->gen_pool[i], size);
		if (handle) {
			*from_pool = tvp_pool->gen_pool[i];
			return handle;
		}
	}
	return NULL;
}

static void *codec_mm_extpool_free(struct gen_pool *from_pool, void *handle,
				   int size)
{
	gen_pool_free(from_pool, (unsigned long)handle, size);
	return 0;
}

static int codec_mm_valid_mm_locked(struct codec_mm_s *mmhandle)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem;
	int have_found = 0;

	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			if (mem == mmhandle) {
				have_found =  1;
				break;
			}
		}
	}
	return have_found;
}

static int codec_mm_alloc_tvp_pre_check_in(struct codec_mm_mgt_s *mgt,
		int need_size, int flags)
{
	if (!mgt)
		return 0;

	struct extpool_mgt_s *tvp_pool = &mgt->tvp_pool;
	int i = 0;

	mutex_lock(&tvp_pool->pool_lock);
	for (i = 0; i < tvp_pool->slot_num; i++) {
		if (gen_pool_avail(tvp_pool->gen_pool[i]) >= need_size) {
			mutex_unlock(&tvp_pool->pool_lock);
			return 1;
		}
	}
	mutex_unlock(&tvp_pool->pool_lock);
	return 0;
}

/*
 *have_space:
 *1:	can alloced from reserved
 *2:	can alloced from cma
 *4:  can alloced from sys.
 *8:  can alloced from tvp.
 *
 *flags:
 *	is tvp = (flags & 1)
 */
static int codec_mm_alloc_pre_check_in(struct codec_mm_mgt_s *mgt,
				       int need_size, int flags)
{
	int have_space = 0;
	int aligned_size = PAGE_ALIGN(need_size);

	if (aligned_size <= mgt->total_reserved_size - mgt->alloced_res_size)
		have_space |= 1;
	if (aligned_size <= mgt->cma_res_pool.total_size -
		mgt->cma_res_pool.alloced_size)
		have_space |= 1;

	if (aligned_size <= mgt->total_cma_size - mgt->alloced_cma_size)
		have_space |= 2;

	if (aligned_size / PAGE_SIZE <= mgt->alloc_from_sys_pages_max)
		have_space |= 4;
	if (tvp_dynamic_increase_disable) {
		if (aligned_size <= mgt->tvp_pool.total_size -
			mgt->tvp_pool.alloced_size)
			have_space |= 8;
	} else {
		if (flags & CODEC_MM_FLAGS_TVP) {
			if (codec_mm_alloc_tvp_pre_check_in(mgt, aligned_size, flags)) {
				have_space |= 8;
			} else {
				do {
					if (codec_mm_tvp_pool_alloc_by_slot(&mgt->tvp_pool,
						0, mgt->tvp_enable) == 0) {
						pr_err("no more memory %d", mgt->tvp_enable);
						break;
					}
					if (codec_mm_alloc_tvp_pre_check_in(mgt, aligned_size,
						flags)) {
						have_space |= 8;
						break;
					}
				} while (mgt->tvp_pool.slot_num <= TVP_POOL_SEGMENT_MAX_USED);
			}
		}
	}

	if (debug_mode & 0xf) {
		have_space = have_space & (~(debug_mode & 1));
		have_space = have_space & (~(debug_mode & 2));
		have_space = have_space & (~(debug_mode & 4));
		if (debug_mode & 8) {
			if (mgt->total_alloced_size >
				mgt->total_codec_mem_size / 2) {
				pr_info("codec mm memory is limited by %d, (bit8 enable)\n",
					debug_mode);
				have_space = 0;
			}
		}
	}

	return have_space;
}

static ulong codec_mm_search_phy_addr(char *vaddr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	ulong phy_addr = 0;
	unsigned long flags;

	spin_lock_irqsave(&mgt->lock, flags);

	list_for_each_entry(mem, &mgt->mem_list, list) {
		/*
		 * If work on the fast play mode that is allocate a lot of
		 * memory from CMA, It will be a reserve memory and add to the
		 * codec_mm list, but this area can't be used for search vaddr
		 * thus the node of codec_mm list must be ignored.
		 */
		if (mem->flags & CODEC_MM_FLAGS_FOR_LOCAL_MGR)
			continue;

		if (vaddr - mem->vbuffer >= 0 &&
		    vaddr - mem->vbuffer < mem->buffer_size) {
			if (mem->phy_addr)
				phy_addr = mem->phy_addr +
					(vaddr - mem->vbuffer);
			break;
		}
	}

	spin_unlock_irqrestore(&mgt->lock, flags);

	return phy_addr;
}

static void *codec_mm_search_vaddr(unsigned long phy_addr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	void *vaddr = NULL;
	unsigned long flags;

	if (!PageHighMem(phys_to_page(phy_addr)))
		return phys_to_virt(phy_addr);

	spin_lock_irqsave(&mgt->lock, flags);

	list_for_each_entry(mem, &mgt->mem_list, list) {
		/*
		 * If work on the fast play mode that is allocate a lot of
		 * memory from CMA, It will be a reserve memory and add to the
		 * codec_mm list, but this area can't be used for search vaddr
		 * thus the node of codec_mm list must be ignored.
		 */
		if (mem->flags & CODEC_MM_FLAGS_FOR_LOCAL_MGR)
			continue;

		if (phy_addr >= mem->phy_addr &&
		    phy_addr < mem->phy_addr + mem->buffer_size) {
			if (mem->vbuffer)
				vaddr = mem->vbuffer +
					(phy_addr - mem->phy_addr);
			break;
		}
	}

	spin_unlock_irqrestore(&mgt->lock, flags);

	return vaddr;
}

u8 *codec_mm_vmap(ulong addr, u32 size)
{
	u8 *vaddr = NULL;
	struct page **pages = NULL;
	u32 i, npages, offset = 0;
	ulong phys, page_start;
	pgprot_t pgprot = PAGE_KERNEL;

	if (!PageHighMem(phys_to_page(addr)))
		return phys_to_virt(addr);

	offset = offset_in_page(addr);
	page_start = addr - offset;
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = kmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++) {
		phys = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(phys >> PAGE_SHIFT);
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("the phy(%lx) vmaped fail, size: %d\n",
		       page_start, npages << PAGE_SHIFT);
		kfree(pages);
		return NULL;
	}

	kfree(pages);

	if (debug_mode & 0x20) {
		pr_info("[HIGH-MEM-MAP] %s, pa(%lx) to va(%lx), size: %d\n",
			__func__, page_start, (ulong)vaddr,
			npages << PAGE_SHIFT);
	}

	return vaddr + offset;
}
EXPORT_SYMBOL(codec_mm_vmap);

void codec_mm_memset(ulong phys, u32 val, u32 size)
{
	void *ptr = NULL;
	struct page *page = phys_to_page(phys);
	int i, len;

	/*any data lurking in the kernel direct-mapped region is invalidated.*/
	if (!PageHighMem(page)) {
		ptr = page_address(page);
		memset(ptr, val, size);
		codec_mm_dma_flush(ptr, size, DMA_TO_DEVICE);
		return;
	}

	/* memset highmem area */
	for (i = 0; i < size; i += MAP_RANGE) {
		len = ((size - i) > MAP_RANGE) ? MAP_RANGE : size - i;
		ptr = codec_mm_vmap(phys + i, len);
		if (!ptr) {
			pr_err("%s,vmap the page failed.\n", __func__);
			return;
		}
		memset(ptr, val, len);
		codec_mm_dma_flush(ptr, len, DMA_TO_DEVICE);
		codec_mm_unmap_phyaddr(ptr);
	}
}
EXPORT_SYMBOL(codec_mm_memset);

void codec_mm_unmap_phyaddr(u8 *vaddr)
{
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	if (is_vmalloc_or_module_addr(vaddr))
		vunmap(addr);
}
EXPORT_SYMBOL(codec_mm_unmap_phyaddr);

static void *codec_mm_map_phyaddr(struct codec_mm_s *mem)
{
	void *vaddr = NULL;
	unsigned int phys = mem->phy_addr;
	unsigned int size = mem->buffer_size;

	if (!PageHighMem(phys_to_page(phys)))
		return phys_to_virt(phys);

	vaddr = codec_mm_vmap(phys, size);
	/*vaddr = ioremap_nocache(phy_addr, size);*/

	if (vaddr)
		mem->flags |= CODEC_MM_FLAGS_FOR_PHYS_VMAPED;

	return vaddr;
}

int codec_mm_add_release_callback(struct codec_mm_s *mem, struct codec_mm_cb_s *cb)
{
	unsigned long flags;

	if (!mem || !cb)
		return -EINVAL;

	spin_lock_irqsave(&mem->lock, flags);

	if (cb->func) {
		list_add_tail(&cb->node, &mem->release_cb_list);
		mem->release_cb_cnt += 1;
	} else {
		INIT_LIST_HEAD(&cb->node);
	}
	spin_unlock_irqrestore(&mem->lock, flags);

	return 0;
}
EXPORT_SYMBOL(codec_mm_add_release_callback);

static int codec_mm_alloc_in(struct codec_mm_mgt_s *mgt, struct codec_mm_s *mem)
{
	int try_alloced_from_sys = 0;
	int try_alloced_from_reserved = 0;
	int align_2n = mem->align2n < PAGE_SHIFT ? PAGE_SHIFT : mem->align2n;
	int try_cma_first = mem->flags & CODEC_MM_FLAGS_CMA_FIRST;
	int max_retry = ALLOC_MAX_RETRY;
	int have_space;
	int alloc_trace_mask = 0;

	int can_from_res = ((mgt->res_pool ||
		(mgt->cma_res_pool.total_size > 0)) &&	/*have res */
		!(mem->flags & CODEC_MM_FLAGS_CMA)) ||	/*must not CMA */
		((mem->flags & CODEC_MM_FLAGS_RESERVED) ||/*need RESERVED */
		CODEC_MM_FOR_DMA_ONLY(mem->flags) ||	/*NO CPU */
		((mem->flags & CODEC_MM_FLAGS_CPU) &&
			(mgt->res_mem_flags & RES_MEM_FLAGS_HAVE_MAPED)));
	 /*CPU*/
	int can_from_cma = ((mgt->total_cma_size > 0) &&/*have cma */
		!(mem->flags & CODEC_MM_FLAGS_RESERVED)) ||
		(mem->flags & CODEC_MM_FLAGS_CMA);	/*can from CMA */
	/*not always reserved. */

	int can_from_sys = (mem->flags & CODEC_MM_FLAGS_DMA_CPU) &&
		(mem->page_count <= mgt->alloc_from_sys_pages_max);

	int can_from_tvp = (mem->flags & CODEC_MM_FLAGS_TVP);

	if (can_from_tvp) {
		can_from_sys = 0;
		can_from_res = 0;
		can_from_cma = 0;
	}

	have_space = codec_mm_alloc_pre_check_in(mgt, mem->buffer_size, mem->flags);
	if (!have_space)
		return -10001;

	can_from_res = can_from_res && (have_space & 1);
	can_from_cma = can_from_cma && (have_space & 2);
	can_from_sys = can_from_sys && (have_space & 4);
	can_from_tvp = can_from_tvp && (have_space & 8);
	if (!can_from_res && !can_from_cma &&
	    !can_from_sys && !can_from_tvp) {
		if (debug_mode & 0x10)
			pr_info("error, codec mm have space:%x\n",
				have_space);
		return -10002;
	}

	do {
		if ((mem->flags & CODEC_MM_FLAGS_DMA_CPU) &&
		    mem->page_count <= mgt->alloc_from_sys_pages_max &&
		    align_2n <= PAGE_SHIFT) {
			alloc_trace_mask |= 1 << 0;
			mem->mem_handle = (void *)__get_free_pages(GFP_KERNEL,
				get_order(mem->buffer_size));
			mem->from_flags =
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES;
			if (mem->mem_handle) {
				mem->vbuffer = mem->mem_handle;
				mem->phy_addr = virt_to_phys(mem->mem_handle);
				break;
			}
			try_alloced_from_sys = 1;
		}
		/*cma first. */
		if (try_cma_first && can_from_cma) {
			/*
			 *normal cma.
			 */
			alloc_trace_mask |= 1 << 1;
			mem->mem_handle =
				dma_alloc_from_contiguous(mgt->dev,
							  mem->page_count,
							  align_2n -
							  PAGE_SHIFT, false);
			mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA;
			if (mem->mem_handle) {
				mem->vbuffer = mem->mem_handle;
				mem->phy_addr =
				 page_to_phys((struct page *)mem->mem_handle);
#ifdef CONFIG_ARM64
				if (mem->flags & CODEC_MM_FLAGS_CMA_CLEAR) {
					/*dma_clear_buffer((struct page *)*/
					/*mem->vbuffer, mem->buffer_size);*/
				}
#endif
				break;
			}
		}
		/*reserved alloc..*/
		if ((can_from_res && mgt->res_pool) &&
			align_2n <= RESERVE_MM_ALIGNED_2N) {
			int aligned_buffer_size = ALIGN(mem->buffer_size,
				(1 << RESERVE_MM_ALIGNED_2N));
			alloc_trace_mask |= 1 << 2;
			mem->mem_handle = (void *)gen_pool_alloc(mgt->res_pool,
							aligned_buffer_size);
			mem->from_flags =
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED;
			if (mem->mem_handle) {
				/*default is no maped */
				mem->vbuffer = NULL;
				mem->phy_addr = (unsigned long)mem->mem_handle;
				mem->buffer_size = aligned_buffer_size;
				break;
			}
			try_alloced_from_reserved = 1;
		}
		/*can_from_res is reserved.. */
		if (can_from_res) {
			if (mgt->cma_res_pool.total_size > 0 &&
			    (mgt->cma_res_pool.alloced_size +
			    mem->buffer_size) < mgt->cma_res_pool.total_size) {
				/*
				 *from cma res first.
				 */
				int aligned_buffer_size =
					ALIGN(mem->buffer_size,
					      (1 << RESERVE_MM_ALIGNED_2N));
				alloc_trace_mask |= 1 << 3;
				mem->mem_handle =
					(void *)
					codec_mm_extpool_alloc(&mgt
							       ->cma_res_pool,
								&mem->from_ext,
							  aligned_buffer_size);
				mem->from_flags =
					AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES;
				if (mem->mem_handle) {
					/*no vaddr for TVP MEMORY */
					mem->phy_addr =
						(unsigned long)mem->mem_handle;
					mem->buffer_size = aligned_buffer_size;
					mem->vbuffer = (mem->flags &
						CODEC_MM_FLAGS_CPU) ?
						codec_mm_map_phyaddr(mem) :
						NULL;
					break;
				}
			}
		}
		if (can_from_cma) {
			/*
			 *normal cma.
			 */
			alloc_trace_mask |= 1 << 4;
			mem->mem_handle =
				dma_alloc_from_contiguous(mgt->dev,
							  mem->page_count,
							  align_2n - PAGE_SHIFT,
							  false);
			mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA;
			if (mem->mem_handle) {
				mem->phy_addr =
					page_to_phys((struct page *)
						mem->mem_handle);

				if (mem->flags & CODEC_MM_FLAGS_CPU)
					mem->vbuffer =
						codec_mm_map_phyaddr(mem);
#ifdef CONFIG_ARM64
				if (mem->flags & CODEC_MM_FLAGS_CMA_CLEAR) {
					/*dma_clear_buffer((struct page *)*/
					/*mem->vbuffer, mem->buffer_size);*/
				}
#endif
				break;
			}
		}
		if (can_from_tvp &&
			align_2n <= TVP_MM_ALIGNED_2N) {
			/* 64k,aligend */
			int aligned_buffer_size = ALIGN(mem->buffer_size,
					(1 << TVP_MM_ALIGNED_2N));
			alloc_trace_mask |= 1 << 5;
			if (tvp_dynamic_increase_disable) {
				mem->mem_handle = (void *)codec_mm_extpool_alloc(&mgt->tvp_pool,
					&mem->from_ext,
					aligned_buffer_size);
				mem->from_flags =
					AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP;
				if (mem->mem_handle) {
					/*no vaddr for TVP MEMORY */
					mem->vbuffer = NULL;
					mem->phy_addr = (unsigned long)mem->mem_handle;
					mem->buffer_size = aligned_buffer_size;
					break;
				}
			} else {
				do {
					mem->mem_handle =
						(void *)codec_mm_extpool_alloc(&mgt->tvp_pool,
							&mem->from_ext,
							aligned_buffer_size);
					if (mem->mem_handle) {
						/*no vaddr for TVP MEMORY */
						mem->from_flags =
							AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP;
						mem->vbuffer = NULL;
						mem->phy_addr = (unsigned long)mem->mem_handle;
						mem->buffer_size = aligned_buffer_size;
						break;
					}
					if (codec_mm_tvp_pool_alloc_by_slot(&mgt->tvp_pool,
						0, mgt->tvp_enable) == 0) {
						pr_err("no more memory can be alloc %d",
							mgt->tvp_enable);
						break;
					}
				} while (mgt->tvp_pool.slot_num <= TVP_POOL_SEGMENT_MAX_USED);
			}
		}

		if ((mem->flags & CODEC_MM_FLAGS_DMA_CPU) &&
		    mgt->enable_kmalloc_on_nomem &&
		    !try_alloced_from_sys) {
			alloc_trace_mask |= 1 << 6;
			mem->mem_handle = (void *)__get_free_pages(GFP_KERNEL,
				get_order(mem->buffer_size));
			mem->from_flags =
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES;
			if (mem->mem_handle) {
				mem->vbuffer = mem->mem_handle;
				mem->phy_addr =
					virt_to_phys((void *)mem->mem_handle);
				break;
			}
		}
	} while (--max_retry > 0);

	if (mem->mem_handle) {
		return 0;
	} else {
		if (debug_mode & 0x10) {
			pr_info("codec mm have space:%x\n",
				have_space);
			pr_info("canfrom: %d,%d,%d,%d\n",
				can_from_tvp,
				can_from_sys,
				can_from_res,
				can_from_cma);
			pr_info("alloc flags:%d,align=%d,%d,pages:%d,s:%d\n",
				mem->flags,
				mem->align2n,
				align_2n,
				mem->page_count,
				mem->buffer_size);
			pr_info("try alloc mask:%x\n",
				alloc_trace_mask);
		}
		return -10003;
	}
}

static void codec_mm_free_in(struct codec_mm_mgt_s *mgt,
			     struct codec_mm_s *mem)
{
	unsigned long flags;

	if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA) {
		if (mem->flags & CODEC_MM_FLAGS_FOR_PHYS_VMAPED)
			codec_mm_unmap_phyaddr(mem->vbuffer);

		dma_release_from_contiguous(mgt->dev, mem->mem_handle,
					    mem->page_count);
	} else if (mem->from_flags ==
		AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED) {
		gen_pool_free(mgt->res_pool,
			      (unsigned long)mem->mem_handle,
			      mem->buffer_size);
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP) {
		codec_mm_extpool_free((struct gen_pool *)mem->from_ext,
				      mem->mem_handle,
				      mem->buffer_size);
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES) {
		free_pages((unsigned long)mem->mem_handle,
			   get_order(mem->buffer_size));
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES) {
		if (mem->flags & CODEC_MM_FLAGS_FOR_PHYS_VMAPED)
			codec_mm_unmap_phyaddr(mem->vbuffer);
		codec_mm_extpool_free((struct gen_pool *)mem->from_ext,
				      mem->mem_handle,
				      mem->buffer_size);
	}
	spin_lock_irqsave(&mgt->lock, flags);
	if (!(mem->flags & CODEC_MM_FLAGS_FOR_LOCAL_MGR))
		mgt->total_alloced_size -= mem->buffer_size;
	if (mem->flags & CODEC_MM_FLAGS_FOR_SCATTER) {
		mgt->alloced_for_sc_size -= mem->buffer_size;
		mgt->alloced_for_sc_cnt--;
	}

	if (mem->flags & CODEC_MM_FLAGS_FOR_PHYS_VMAPED)
		mgt->phys_vmaped_page_cnt -= mem->page_count;

	if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA) {
		mgt->alloced_cma_size -= mem->buffer_size;
	} else if (mem->from_flags ==
		AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED) {
		mgt->alloced_res_size -= mem->buffer_size;
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP) {
		mgt->tvp_pool.alloced_size -= mem->buffer_size;
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES) {
		mgt->alloced_sys_size -= mem->buffer_size;
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES) {
		mgt->cma_res_pool.alloced_size -= mem->buffer_size;
	}

	spin_unlock_irqrestore(&mgt->lock, flags);
	if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP &&
	    tvp_mode >= 1) {
		mutex_lock(&mgt->tvp_protect_lock);
		if (atomic_read(&mgt->tvp_user_count) == 0) {
			if (codec_mm_tvp_pool_unprotect_and_release(&mgt->tvp_pool) == 0) {
				mgt->tvp_enable = 0;
				pr_info("disalbe tvp\n");
			}
		}
		mutex_unlock(&mgt->tvp_protect_lock);
	}

	return;
}

static void dump_tvp_pool_info(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct extpool_mgt_s *tvp_pool = &mgt->tvp_pool;
	int i, ret, size = 0;
	char *buf, *pbuf;

	buf = kzalloc(120, GFP_KERNEL);
	if (!buf)
		return;
	pbuf = buf;

	for (i = 0; i < TVP_POOL_SEGMENT_MAX_USED; i++) {
		struct gen_pool *gpool = tvp_pool->gen_pool[i];

		if (gpool) {
			ret = snprintf(pbuf, 120 - size,
					"TVP_POOL[%d]%lx/%lx\n", i,
					(ulong)(gen_pool_size(gpool) - gen_pool_avail(gpool)),
					(ulong)gen_pool_size(gpool));
			pbuf += ret;
			size += ret;
		}
	}
	pr_info("%s", buf);
	kfree(buf);
}

struct codec_mm_s *codec_mm_alloc(const char *owner, int size,
				  int align2n, int memflags)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = kmalloc(sizeof(*mem), GFP_KERNEL);
	int count;
	int ret;
	unsigned long flags;

	if (!mem) {
		pr_err("not enough mem for struct codec_mm_s\n");
		return NULL;
	}

	if (mgt->tvp_enable & 3) {
		/*if tvp & video decoder used tvp memory.*/
		   /*Audio don't protect for default now.*/
		if (memflags & CODEC_MM_FLAGS_TVP) {
			/*clear other flags, when tvp mode.*/
			memflags = memflags & (~CODEC_MM_FLAGS_FROM_MASK);
			memflags |= CODEC_MM_FLAGS_TVP;
		}
	} else { /*tvp not enabled*/
		if (memflags & CODEC_MM_FLAGS_TVP) {
			pr_err("TVP not enabled, when alloc from tvp %s need %d\n",
			       owner, size);
		}
	}
	if ((memflags & CODEC_MM_FLAGS_FROM_MASK) == 0)
		memflags |= CODEC_MM_FLAGS_DMA;

	memset(mem, 0, sizeof(struct codec_mm_s));
	mem->buffer_size = PAGE_ALIGN(size);
	count = mem->buffer_size / PAGE_SIZE;
	mem->page_count = count;
	mem->align2n = align2n;
	mem->flags = memflags;
	INIT_LIST_HEAD(&mem->release_cb_list);
	mem->release_cb_cnt = 0;
	ret = codec_mm_alloc_in(mgt, mem);
	if (ret < 0) {
		if (mgt->alloced_for_sc_cnt > 0 && /*have used for scatter.*/
			!(memflags & CODEC_MM_FLAGS_FOR_SCATTER)) {
			/*if not scatter, free scatter caches. */
			pr_err(" No mem ret=%d, clear scatter cache!!\n", ret);
			dump_free_mem_infos(NULL, 0);
			if (memflags & CODEC_MM_FLAGS_TVP)
				codec_mm_scatter_free_all_ignorecache(2);
			else
				codec_mm_scatter_free_all_ignorecache(1);
		}
		if (atomic_read(&mgt->tvp_user_count) != 0 &&
			!(memflags & CODEC_MM_FLAGS_TVP) &&
			strncmp((void *)mem->owner, TVP_POOL_NAME, strlen(TVP_POOL_NAME)) &&
			tvp_pool_early_release_switch) {
			codec_mm_tvp_pool_unprotect_and_release(&mgt->tvp_pool);
			dump_tvp_pool_info();
		}
		ret = codec_mm_alloc_in(mgt, mem);
	}
	if (ret < 0) {
		if (mgt->total_codec_mem_size - mgt->total_alloced_size <= size)
			pr_err("not enough mem for %s size %d, ret=%d\n",
				owner, size, ret);
		else
			pr_err("not enough mem for %s size %d, ret=%d due to mem fragmentation\n",
				owner, size, ret);
		pr_err("mem flags %d %d, %d\n",
			memflags, mem->flags, align2n);
		kfree(mem);
		dump_mem_infos();
		if (mgt->tvp_enable)
			dump_tvp_pool_info();
		return NULL;
	}

	atomic_set(&mem->use_cnt, 1);
	mem->owner[0] = owner;
	spin_lock_init(&mem->lock);
	spin_lock_irqsave(&mgt->lock, flags);
	mem->mem_id = mgt->global_memid++;
	list_add_tail(&mem->list, &mgt->mem_list);
	switch (mem->from_flags) {
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES:
		mgt->alloced_sys_size += mem->buffer_size;
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA:
		mgt->alloced_cma_size += mem->buffer_size;
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP:
		mgt->tvp_pool.alloced_size += mem->buffer_size;
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED:
		mgt->alloced_res_size += mem->buffer_size;
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES:
		mgt->cma_res_pool.alloced_size += mem->buffer_size;
		break;
	default:
		pr_err("error alloc flags %d\n", mem->from_flags);
	}
	if (!(mem->flags & CODEC_MM_FLAGS_FOR_LOCAL_MGR)) {
		mgt->total_alloced_size += mem->buffer_size;
		if (mgt->total_alloced_size > mgt->max_used_mem_size)
			mgt->max_used_mem_size = mgt->total_alloced_size;
	}
	if ((mem->flags & CODEC_MM_FLAGS_FOR_SCATTER)) {
		mgt->alloced_for_sc_size += mem->buffer_size;
		mgt->alloced_for_sc_cnt++;
	}

	if (mem->flags & CODEC_MM_FLAGS_FOR_PHYS_VMAPED)
		mgt->phys_vmaped_page_cnt += mem->page_count;

	spin_unlock_irqrestore(&mgt->lock, flags);
	mem->alloced_jiffies = get_jiffies_64();
	if (debug_mode & 0x20)
		pr_err("mem_id [%d] %s alloc size %d at 0x%lx from %d,2n:%d,flags:%d\n",
		       mem->mem_id, owner, size, mem->phy_addr,
		       mem->from_flags,
		       align2n,
		       memflags);
	return mem;
}
EXPORT_SYMBOL(codec_mm_alloc);

void codec_mm_release(struct codec_mm_s *mem, const char *owner)
{
	int index;
	unsigned long flags;
	int i, j;
	const char *max_owner;
	struct codec_mm_cb_s **release_cb;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (!mem)
		return;

	release_cb = (struct codec_mm_cb_s **)
		vzalloc(sizeof(struct codec_mm_cb_s *) * (mem->release_cb_cnt + 1));

	spin_lock_irqsave(&mgt->lock, flags);
	if (!codec_mm_valid_mm_locked(mem)) {
		pr_err("codec mm not invalid!\n");
		spin_unlock_irqrestore(&mgt->lock, flags);
		if (release_cb)
			vfree(release_cb);
		return;
	}
	index = atomic_dec_return(&mem->use_cnt);
	max_owner = mem->owner[index];
	for (i = 0; i < index; i++) {
		if (mem->owner[i] && strcmp(owner, mem->owner[i]) == 0)
			mem->owner[i] = max_owner;
	}
	if (debug_mode & 0x20)
		pr_err("mem_id [%d] %s free mem size %d at %lx from %d,index =%d\n",
		       mem->mem_id, owner, mem->buffer_size, mem->phy_addr,
		       mem->from_flags, index);
	mem->owner[index] = NULL;
	if (index == 0) {
		struct codec_mm_cb_s *cur, *tmp;
		list_del(&mem->list);
		i = 0;
		list_for_each_entry_safe(cur,
			tmp, &mem->release_cb_list, node) {
			list_del_init(&cur->node);
			if (release_cb)
				release_cb[i] = cur;
			i++;
			if (i > mem->release_cb_cnt) {
				pr_err("%s, error, release cnt %d less than loop %d\n",
					__func__, mem->release_cb_cnt, i);
				break;
			}
		}
		spin_unlock_irqrestore(&mgt->lock, flags);

		if (release_cb) {
			for (j = 0; j < i; j++) {
				cur = release_cb[j];
				if (cur)
					cur->func(mem, cur);
				mem->release_cb_cnt--;
			}
		}
		if (mem->release_cb_cnt != 0) {
			pr_err("%s, error, i %d, release %d\n",
					__func__, i, mem->release_cb_cnt);
		}
		codec_mm_free_in(mgt, mem);
		kfree(mem);
		if (release_cb)
			vfree(release_cb);
		return;
	}
	spin_unlock_irqrestore(&mgt->lock, flags);

	if (release_cb)
		vfree(release_cb);
}
EXPORT_SYMBOL(codec_mm_release);

void *codec_mm_dma_alloc_coherent(ulong *handle,
				  ulong *phy_out,
				  int size,
				  const char *owner)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	void *vaddr = NULL;
	int space, s_res, s_cma, s_sys;
	dma_addr_t dma_handle = 0;
	int buf_size = PAGE_ALIGN(size);
	ulong flags;

	vaddr = dma_alloc_coherent(mgt->dev, buf_size, &dma_handle, GFP_KERNEL);
	if (!vaddr)
		goto err;

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		goto err;

	mem->owner[0]	= owner;
	mem->vbuffer	= vaddr;
	mem->phy_addr	= dma_handle;
	mem->buffer_size = buf_size;
	mem->from_flags	= AMPORTS_MEM_FLAGS_FROM_GET_FROM_COHERENT;
	mem->alloced_jiffies = get_jiffies_64();

	space = codec_mm_alloc_pre_check_in(mgt, mem->buffer_size, 0);
	if (!space)
		goto err;

	s_res = (space & 1);
	s_cma = (space & 2);
	s_sys = (space & 4);
	if (!s_res && !s_cma && !s_sys) {
		if (debug_mode & 0x20)
			pr_err("error, codec mm have space: %x\n", space);
		goto err;
	}
	mem->flags = space;

	spin_lock_irqsave(&mgt->lock, flags);

	mem->mem_id = mgt->global_memid++;
	if (s_cma) {
		mgt->alloced_cma_size	+= buf_size;
	} else if (s_res) {
		if (mgt->cma_res_pool.total_size > 0)
			mgt->cma_res_pool.total_size += buf_size;
		else
			mgt->alloced_res_size += buf_size;
	} else {
		mgt->alloced_sys_size	+= buf_size;
	}
	mgt->alloced_from_coherent	+= buf_size;
	mgt->total_alloced_size		+= buf_size;
	if (mgt->total_alloced_size > mgt->max_used_mem_size)
		mgt->max_used_mem_size = mgt->total_alloced_size;
	*handle				= (ulong)mem;
	*phy_out			= mem->phy_addr;
	list_add_tail(&mem->list, &mgt->mem_list);

	spin_unlock_irqrestore(&mgt->lock, flags);

	if (debug_mode & 0x20) {
		pr_info("mem_id [%d] [%s] alloc coherent mem (phy %lx, vddr %px) size (%d).\n",
			mem->mem_id, owner, mem->phy_addr, vaddr, buf_size);
	}
	return vaddr;
err:
	kfree(mem);
	if (vaddr)
		dma_free_coherent(mgt->dev, buf_size, vaddr, dma_handle);

	return NULL;
}
EXPORT_SYMBOL(codec_mm_dma_alloc_coherent);

void codec_mm_dma_free_coherent(ulong handle)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = (struct codec_mm_s *)handle;
	ulong flags;

	if (!handle)
		return;

	dma_free_coherent(mgt->dev, mem->buffer_size,
		mem->vbuffer, mem->phy_addr);

	spin_lock_irqsave(&mgt->lock, flags);

	if (mem->flags & 2)
		mgt->alloced_cma_size	-= mem->buffer_size;
	else if (mem->flags & 1)
		if (mgt->cma_res_pool.total_size > 0)
			mgt->cma_res_pool.total_size += mem->buffer_size;
		else
			mgt->alloced_res_size += mem->buffer_size;
	else
		mgt->alloced_sys_size	-= mem->buffer_size;
	mgt->alloced_from_coherent	-= mem->buffer_size;
	mgt->total_alloced_size		-= mem->buffer_size;
	list_del(&mem->list);

	spin_unlock_irqrestore(&mgt->lock, flags);

	if (debug_mode & 0x20) {
		pr_info("mem_id [%d] [%s] free coherent mem (phy %lx, vddr %px) size (%d)\n",
			mem->mem_id, mem->owner[0], mem->phy_addr, mem->vbuffer,
			mem->buffer_size);
	}

	kfree(mem);
}
EXPORT_SYMBOL(codec_mm_dma_free_coherent);

void codec_mm_release_with_check(struct codec_mm_s *mem, const char *owner)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&mgt->lock, flags);
	ret = codec_mm_valid_mm_locked(mem);
	spin_unlock_irqrestore(&mgt->lock, flags);
	if (ret) {
		/*for check,*/
		return codec_mm_release(mem, owner);
	}
}
EXPORT_SYMBOL(codec_mm_release_with_check);

void codec_mm_dma_flush(void *vaddr,
			int size,
			enum dma_data_direction dir)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	dma_addr_t dma_addr;
	ulong phy_addr;

	if (is_vmalloc_or_module_addr(vaddr)) {
		phy_addr = codec_mm_search_phy_addr(vaddr);
		if (!phy_addr)
			phy_addr = page_to_phys(vmalloc_to_page(vaddr))
				+ offset_in_page(vaddr);
		if (phy_addr && PageHighMem(phys_to_page(phy_addr)))
			dma_sync_single_for_device(mgt->dev,
						   phy_addr, size, dir);
		return;
	}

	/* only apply to the lowmem. */
	dma_addr = dma_map_single(mgt->dev, vaddr, size, dir);
	if (dma_mapping_error(mgt->dev, dma_addr)) {
		pr_err("dma map %d bytes error\n", size);
		return;
	}

	dma_sync_single_for_device(mgt->dev, dma_addr, size, dir);
	dma_unmap_single(mgt->dev, dma_addr, size, dir);
}
EXPORT_SYMBOL(codec_mm_dma_flush);

int codec_mm_has_owner(struct codec_mm_s *mem, const char *owner)
{
	int index;
	int i;
	unsigned long flags;
	int is_owner = 0;

	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (mem) {
		spin_lock_irqsave(&mgt->lock, flags);
		if (!codec_mm_valid_mm_locked(mem)) {
			spin_unlock_irqrestore(&mgt->lock, flags);
			pr_err("codec mm %p not invalid!\n", mem);
			return 0;
		}

		index = atomic_read(&mem->use_cnt);

		for (i = 0; i < index; i++) {
			if (mem->owner[i] &&
			    strcmp(owner, mem->owner[i]) == 0) {
				is_owner = 1;
				break;
			}
		}
		spin_unlock_irqrestore(&mgt->lock, flags);
	}

	return is_owner;
}

int codec_mm_request_shared_mem(struct codec_mm_s *mem, const char *owner)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned long flags;
	int ret = -1;

	spin_lock_irqsave(&mgt->lock, flags);
	if (!codec_mm_valid_mm_locked(mem)) {
		ret = -2;
		goto out;
	}
	if (atomic_read(&mem->use_cnt) > 7) {
		ret = -3;
		goto out;
	}
	ret = 0;
	mem->owner[atomic_inc_return(&mem->use_cnt) - 1] = owner;

out:
	spin_unlock_irqrestore(&mgt->lock, flags);

	return 0;
}
EXPORT_SYMBOL(codec_mm_request_shared_mem);

static struct codec_mm_s *codec_mm_get_by_val_off(unsigned long val, int off)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem, *want_mem;
	unsigned long flags;

	want_mem = NULL;
	spin_lock_irqsave(&mgt->lock, flags);
	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			if (mem && VAL_OFF_VAL(mem, off) == val)
				want_mem = mem;
		}
	}
	spin_unlock_irqrestore(&mgt->lock, flags);
	return want_mem;
}

unsigned long codec_mm_alloc_for_dma(const char *owner, int page_cnt,
				     int align2n, int memflags)
{
	struct codec_mm_s *mem;

	mem = codec_mm_alloc(owner, page_cnt << PAGE_SHIFT, align2n, memflags);
	if (!mem)
		return 0;
	return mem->phy_addr;
}
EXPORT_SYMBOL(codec_mm_alloc_for_dma);

unsigned long codec_mm_alloc_for_dma_ex(const char *owner,
					int page_cnt,
					int align2n,
					int memflags,
					int ins_id,
					int buffer_id)
{
	struct codec_mm_s *mem;

	mem = codec_mm_alloc(owner, page_cnt << PAGE_SHIFT, align2n, memflags);
	if (!mem)
		return 0;
	mem->ins_id = ins_id;
	mem->ins_buffer_id = buffer_id;
	if (debug_mode & 0x20) {
		pr_err("%s, for ins %d, buffer id:%d\n",
		       mem->owner[0] ? mem->owner[0] : "no",
		       mem->ins_id,
		       buffer_id);
	}
	return mem->phy_addr;
}
EXPORT_SYMBOL(codec_mm_alloc_for_dma_ex);

int codec_mm_free_for_dma(const char *owner, unsigned long phy_addr)
{
	struct codec_mm_s *mem;

	mem = codec_mm_get_by_val_off(phy_addr, PHY_OFF());

	if (mem)
		codec_mm_release(mem, owner);
	else
		return -1;
	return 0;
}
EXPORT_SYMBOL(codec_mm_free_for_dma);

static int codec_mm_init_tvp_pool(struct extpool_mgt_s *tvp_pool,
				  struct codec_mm_s *mm)
{
	struct gen_pool *pool;
	int ret;

	pool = gen_pool_create(TVP_MM_ALIGNED_2N, -1);
	if (!pool)
		return -ENOMEM;
	ret = gen_pool_add(pool, mm->phy_addr, mm->buffer_size, -1);
	if (ret < 0) {
		gen_pool_destroy(pool);
		return -1;
	}
	tvp_pool->gen_pool[tvp_pool->slot_num] = pool;
	mm->tvp_handle = -1;
	tvp_pool->mm[tvp_pool->slot_num] = mm;
	return 0;
}

static int codec_mm_tvp_pool_protect(struct extpool_mgt_s *tvp_pool)
{
	int ret = 0;
	int i = 0;

	for (i = 0; i < tvp_pool->slot_num; i++) {
		if (tvp_pool->mm[i]->tvp_handle == -1) {
			ret = tee_protect_tvp_mem
				((uint32_t)tvp_pool->mm[i]->phy_addr,
				(uint32_t)tvp_pool->mm[i]->buffer_size,
				&tvp_pool->mm[i]->tvp_handle);
			pr_info("protect tvp %d %d ret %d %x %x\n",
				i, tvp_pool->mm[i]->tvp_handle, ret,
				(unsigned int)tvp_pool->mm[i]->phy_addr,
				(unsigned int)tvp_pool->mm[i]->buffer_size);
		} else {
			pr_info("protect tvp %d %d ret %d\n",
				i, tvp_pool->mm[i]->tvp_handle, ret);
		}
	}
	return ret;
}

static int codec_mm_extpool_pool_release_inner(int slot_num_start,
					       struct extpool_mgt_s *tvp_pool)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int i;

	for (i = slot_num_start; i < tvp_pool->slot_num; i++) {
		struct gen_pool *gpool = tvp_pool->gen_pool[i];
		int slot_mem_size = 0;

		if (gpool) {
			slot_mem_size = gen_pool_size(gpool);
			gen_pool_destroy(tvp_pool->gen_pool[i]);
			if (tvp_pool->mm[i]) {
				struct page *mm = tvp_pool->mm[i]->mem_handle;

				if (tvp_pool->mm[i]->from_flags ==
					AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES)
					mm = phys_to_page((unsigned long)mm);
				cma_mmu_op(mm,
					   tvp_pool->mm[i]->page_count,
					   1);
				codec_mm_release(tvp_pool->mm[i],
						 TVP_POOL_NAME);
			}
		}
		mgt->tvp_pool.total_size -= slot_mem_size;
		tvp_pool->gen_pool[i] = NULL;
		tvp_pool->mm[i] = NULL;
	}
	tvp_pool->slot_num = slot_num_start;
	return slot_num_start;
}

static int codec_mm_tvp_pool_alloc_by_type(struct extpool_mgt_s *tvp_pool,
	int size, int memflags, int type)
{
	struct codec_mm_s *mem;
	int ret;

	if (type == CODEC_MM_FLAGS_CMA) {
		mem = codec_mm_alloc(TVP_POOL_NAME,
					size,
					RESERVE_MM_ALIGNED_2N,
					CODEC_MM_FLAGS_FOR_LOCAL_MGR |
					CODEC_MM_FLAGS_CMA);
		if (mem) {
			struct page *mm = mem->mem_handle;

			if (mem->from_flags ==
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES)
				mm = phys_to_page((unsigned long)mm);
			cma_mmu_op(mm, mem->page_count, 0);
			ret = codec_mm_init_tvp_pool(tvp_pool, mem);
			if (ret < 0) {
				cma_mmu_op(mm, mem->page_count, 1);
				codec_mm_release(mem, TVP_POOL_NAME);
			} else {
				tvp_pool->total_size += size;
				tvp_pool->slot_num++;
				if (tvp_mode >= 1) {
					if (codec_mm_tvp_pool_protect(tvp_pool)) {
						codec_mm_extpool_pool_release_inner
							(tvp_pool->slot_num - 1, tvp_pool);
						return 1;
					}
				}
				return 0;
			}
		}
	} else if (type == CODEC_MM_FLAGS_RESERVED) {
		mem = codec_mm_alloc(TVP_POOL_NAME,
				size,
				RESERVE_MM_ALIGNED_2N,
				CODEC_MM_FLAGS_FOR_LOCAL_MGR |
				CODEC_MM_FLAGS_RESERVED);

		if (mem) {
			ret = codec_mm_init_tvp_pool(tvp_pool, mem);
			if (ret < 0) {
				codec_mm_release(mem, TVP_POOL_NAME);
			} else {
				tvp_pool->total_size += size;
				tvp_pool->slot_num++;
				if (tvp_mode >= 1) {
					if (codec_mm_tvp_pool_protect(tvp_pool)) {
						codec_mm_extpool_pool_release_inner
							(tvp_pool->slot_num - 1, tvp_pool);
						return 1;
					}
				}
				return 0;
			}
		}
	}
	return 1;
}

static int codec_mm_tvp_pool_alloc_by_slot(struct extpool_mgt_s *tvp_pool,
	int memflags, int flags)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int size = 0;
	int try_alloced_size = 0;
	int max_reserved_free_size = 0;
	int max_cma_free_size = 0;
	int use_cma_pool_first = 0;
	int retry_cnt = 0;

	mutex_lock(&tvp_pool->pool_lock);
	max_reserved_free_size = mgt->total_reserved_size - mgt->alloced_res_size;
	if (mgt->cma_res_pool.total_size - mgt->cma_res_pool.alloced_size > 16 * SZ_1M)
		max_reserved_free_size += mgt->cma_res_pool.total_size -
			mgt->cma_res_pool.alloced_size;
	max_cma_free_size = mgt->total_cma_size - mgt->alloced_cma_size;
	use_cma_pool_first = max_cma_free_size > max_reserved_free_size ? 1 : 0;

	if (tvp_pool->slot_num >= TVP_POOL_SEGMENT_MAX_USED) {
		try_alloced_size = 0;
		pr_info("slot_num  exceed the limit %d\n", tvp_pool->slot_num);
		goto alloced_finished;
	}

	size = default_tvp_pool_segment_size[tvp_pool->slot_num];
	if (size <= 0) {
		try_alloced_size = 0;
		goto alloced_finished;
	}

	if (use_cma_pool_first) {
		try_alloced_size = size > max_cma_free_size ? max_cma_free_size : size;
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
		retry_cnt = try_alloced_size / (4 * SZ_1M);
		if (try_alloced_size > 0) {
			int retry = 0;

			do {
				if (codec_mm_tvp_pool_alloc_by_type(tvp_pool,
					try_alloced_size, memflags, CODEC_MM_FLAGS_CMA)) {
					try_alloced_size = try_alloced_size - 4 * SZ_1M;
					if (try_alloced_size < 16 * SZ_1M)
						break;
					try_alloced_size = codec_mm_align_up2n(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
				} else {
					goto alloced_finished;
				}
			} while (retry++ < retry_cnt);
		}
	} else {
		try_alloced_size = size > max_reserved_free_size ? max_reserved_free_size : size;
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
		retry_cnt = try_alloced_size / (4 * SZ_1M);
		if (try_alloced_size > 0) {
			int retry = 0;

			do {
				if (codec_mm_tvp_pool_alloc_by_type(tvp_pool,
					try_alloced_size, memflags, CODEC_MM_FLAGS_RESERVED)) {
					try_alloced_size = try_alloced_size - 4 * SZ_1M;
					if (try_alloced_size < 16 * SZ_1M)
						break;
					try_alloced_size = codec_mm_align_up2n(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
				} else {
					goto alloced_finished;
				}
			} while (retry++ < retry_cnt);
		}
	}

	if (use_cma_pool_first) {
		try_alloced_size = size > max_reserved_free_size ? max_reserved_free_size : size;
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
		retry_cnt = try_alloced_size / (4 * SZ_1M);
		if (try_alloced_size > 0) {
			int retry = 0;

			do {
				if (codec_mm_tvp_pool_alloc_by_type(tvp_pool,
					try_alloced_size, memflags, CODEC_MM_FLAGS_RESERVED)) {
					try_alloced_size = try_alloced_size - 4 * SZ_1M;
					if (try_alloced_size < 16 * SZ_1M)
						break;
					try_alloced_size = codec_mm_align_up2n(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
				} else {
					goto alloced_finished;
				}
			} while (retry++ < retry_cnt);
		}
	} else {
		try_alloced_size = size > max_cma_free_size ? max_cma_free_size : size;
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
		retry_cnt = try_alloced_size / (4 * SZ_1M);
		if (try_alloced_size > 0) {
			int retry = 0;

			do {
				if (codec_mm_tvp_pool_alloc_by_type(tvp_pool,
					try_alloced_size, memflags, CODEC_MM_FLAGS_CMA)) {
					try_alloced_size = try_alloced_size - 4 * SZ_1M;
					if (try_alloced_size < 16 * SZ_1M)
						break;
					try_alloced_size = codec_mm_align_up2n(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
				} else {
					goto alloced_finished;
				}
			} while (retry++ < retry_cnt);
		}
	}
	try_alloced_size = 0;
alloced_finished:
	if (try_alloced_size && !tvp_dynamic_increase_disable && !flags) {
		pr_info("Force enable tvp, please enable it by resource manager or secmem");
		mgt->tvp_enable = 2;
	}
	mutex_unlock(&tvp_pool->pool_lock);
	return try_alloced_size;
}

int codec_mm_extpool_pool_alloc(struct extpool_mgt_s *tvp_pool,
	int size, int memflags, int for_tvp)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem;
	int alloced_size = tvp_pool->total_size;
	int try_alloced_size = 0;
	int ret;
	int retry_cnt = size / (4 * SZ_1M);
	int slot_num = tvp_pool->slot_num;

	/*alloced from reserved*/
	mutex_lock(&tvp_pool->pool_lock);
	if (alloced_size >= size)
		goto alloced_finished1;
	try_alloced_size = mgt->total_reserved_size - mgt->alloced_res_size;
	if (try_alloced_size > 0 && for_tvp) {
		int retry = 0;
		try_alloced_size = min_t(int,
			size - alloced_size, try_alloced_size);
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
			RESERVE_MM_ALIGNED_2N);
		do {
			mem = codec_mm_alloc(TVP_POOL_NAME,
						try_alloced_size,
						RESERVE_MM_ALIGNED_2N,
						CODEC_MM_FLAGS_FOR_LOCAL_MGR |
						CODEC_MM_FLAGS_RESERVED);

			if (mem) {
				ret = codec_mm_init_tvp_pool(tvp_pool, mem);
				if (ret < 0) {
					codec_mm_release(mem, TVP_POOL_NAME);
				} else {
					alloced_size += try_alloced_size;
					tvp_pool->slot_num++;
				}
				try_alloced_size = size - alloced_size;
			} else {
				try_alloced_size = try_alloced_size - 4 * SZ_1M;
				if (try_alloced_size < 16 * SZ_1M)
					break;
			}
			if (tvp_pool->slot_num < TVP_POOL_SEGMENT_MAX_USED &&
			    alloced_size < size) {
				try_alloced_size = codec_mm_align_up2n
					(try_alloced_size, RESERVE_MM_ALIGNED_2N);
			} else {
				break;
			}
		} while (retry++ < retry_cnt);
	}
	if (alloced_size >= size || tvp_pool->slot_num >= TVP_POOL_SEGMENT_MAX_USED) {
		/*alloc finished. */
		goto alloced_finished;
	}

/*alloced from cma:*/
	try_alloced_size = mgt->total_cma_size - mgt->alloced_cma_size;
	if (try_alloced_size > 0) {
		int retry = 0;
		int memflags = CODEC_MM_FLAGS_FOR_LOCAL_MGR | CODEC_MM_FLAGS_CMA;

		if (for_tvp)
			memflags |= CODEC_MM_FLAGS_RESERVED;

		try_alloced_size = min_t(int,
			size - alloced_size, try_alloced_size);
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
			RESERVE_MM_ALIGNED_2N);
		do {
			mem = codec_mm_alloc(for_tvp ? TVP_POOL_NAME :
						CMA_RES_POOL_NAME,
					try_alloced_size, RESERVE_MM_ALIGNED_2N,
					memflags);
			if (mem) {
				struct page *mm = mem->mem_handle;

				if (mem->from_flags ==
					AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES)
					mm = phys_to_page((unsigned long)mm);
				if (for_tvp) {
					cma_mmu_op(mm,
						mem->page_count,
						0);
				}
				ret = codec_mm_init_tvp_pool(tvp_pool, mem);
				if (ret < 0) {
					if (for_tvp) {
						cma_mmu_op(mm,
							mem->page_count,
							1);
					}
					codec_mm_release(mem, TVP_POOL_NAME);
				} else {
					alloced_size += try_alloced_size;
					tvp_pool->slot_num++;
				}
				try_alloced_size = size - alloced_size;
			} else {
				try_alloced_size = try_alloced_size - 4 * SZ_1M;
				if (try_alloced_size < 16 * SZ_1M)
					break;
			}
			if (tvp_pool->slot_num < TVP_POOL_SEGMENT_MAX_USED &&
			    alloced_size < size) {
				try_alloced_size = codec_mm_align_up2n(try_alloced_size,
					RESERVE_MM_ALIGNED_2N);
			} else {
				break;
			}
		} while (retry++ < retry_cnt);
	}

alloced_finished:
	if (alloced_size > 0)
		tvp_pool->total_size = alloced_size;
	if (for_tvp) {
		if (alloced_size >= size) {
			if (tvp_mode >= 1)
				codec_mm_tvp_pool_protect(tvp_pool);
		} else {
			codec_mm_extpool_pool_release_inner(slot_num, tvp_pool);
			alloced_size = 0;
		}
	}
alloced_finished1:
	mutex_unlock(&tvp_pool->pool_lock);
	return alloced_size;
}
EXPORT_SYMBOL(codec_mm_extpool_pool_alloc);

/*
 *call this before free all
 *alloced TVP memory;
 *it not free,some memory may free ignore.
 *return :
 */
static int codec_mm_extpool_pool_release(struct extpool_mgt_s *tvp_pool)
{
	int i;
	int ignored = 0;

	mutex_lock(&tvp_pool->pool_lock);
	for (i = 0; i < tvp_pool->slot_num; i++) {
		struct gen_pool *gpool = tvp_pool->gen_pool[i];
		int slot_mem_size = 0;

		if (gpool) {
			if (gen_pool_avail(gpool) != gen_pool_size(gpool)) {
				pr_err("ext pool is not free.\n");
				ignored++;
				continue;	/*ignore this free now, */
			}
			slot_mem_size = gen_pool_size(gpool);
			gen_pool_destroy(tvp_pool->gen_pool[i]);
			if (tvp_pool->mm[i]) {
				struct page *mm = tvp_pool->mm[i]->mem_handle;

				if (tvp_pool->mm[i]->from_flags ==
				    AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES)
					mm = phys_to_page((unsigned long)mm);
				cma_mmu_op(mm,
					   tvp_pool->mm[i]->page_count,
					   1);
				codec_mm_release(tvp_pool->mm[i],
						 TVP_POOL_NAME);
			}
		}
		tvp_pool->total_size -= slot_mem_size;
		tvp_pool->gen_pool[i] = NULL;
		tvp_pool->mm[i] = NULL;
	}
	if (ignored > 0) {
		int before_free_slot = tvp_pool->slot_num + 1;

		for (i = 0; i < tvp_pool->slot_num; i++) {
			if (tvp_pool->gen_pool[i] && before_free_slot < i) {
				tvp_pool->gen_pool[before_free_slot] =
					tvp_pool->gen_pool[i];
				tvp_pool->mm[before_free_slot] =
					tvp_pool->mm[i];
				swap(default_tvp_pool_segment_size[i],
					default_tvp_pool_segment_size[before_free_slot]);
				tvp_pool->gen_pool[i] = NULL;
				tvp_pool->mm[i] = NULL;
				before_free_slot++;
			}
			if (!tvp_pool->gen_pool[i] && before_free_slot > i) {
				before_free_slot = i;
				/**/
			}
		}
	}
	tvp_pool->slot_num = ignored;
	mutex_unlock(&tvp_pool->pool_lock);
	return ignored;
}

static int codec_mm_tvp_pool_unprotect_and_release(struct extpool_mgt_s *tvp_pool)
{
	int i;
	int ignored = 0;

	mutex_lock(&tvp_pool->pool_lock);
	for (i = 0; i < tvp_pool->slot_num; i++) {
		struct gen_pool *gpool = tvp_pool->gen_pool[i];
		int slot_mem_size = 0;

		if (gpool) {
			if (gen_pool_avail(gpool) != gen_pool_size(gpool)) {
				if (debug_mode & 0x20)
					pr_err("Warn: TVP pool will release later.\n");
				ignored++;
				continue;	/*ignore this free now, */
			}
			slot_mem_size = gen_pool_size(gpool);
			gen_pool_destroy(tvp_pool->gen_pool[i]);
			if (tvp_pool->mm[i]) {
				struct page *mm = tvp_pool->mm[i]->mem_handle;

				pr_info("unprotect tvp %d handle is %d\n",
					i, tvp_pool->mm[i]->tvp_handle);
				if (tvp_pool->mm[i]->tvp_handle > 0) {
					tee_unprotect_tvp_mem(tvp_pool->mm[i]->tvp_handle);
					tvp_pool->mm[i]->tvp_handle = -1;
				}
				if (tvp_pool->mm[i]->from_flags ==
					AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES)
					mm = phys_to_page((unsigned long)mm);
				cma_mmu_op(mm, tvp_pool->mm[i]->page_count, 1);
				codec_mm_release(tvp_pool->mm[i], TVP_POOL_NAME);
			}
		}
		tvp_pool->total_size -= slot_mem_size;
		tvp_pool->gen_pool[i] = NULL;
		tvp_pool->mm[i] = NULL;
	}
	if (ignored > 0) {
		int before_free_slot = tvp_pool->slot_num + 1;

		for (i = 0; i < tvp_pool->slot_num; i++) {
			if (tvp_pool->gen_pool[i] && before_free_slot < i) {
				tvp_pool->gen_pool[before_free_slot] =
					tvp_pool->gen_pool[i];
				tvp_pool->mm[before_free_slot] =
					tvp_pool->mm[i];
				tvp_pool->mm[before_free_slot]->tvp_handle =
					tvp_pool->mm[i]->tvp_handle;
				swap(default_tvp_pool_segment_size[i],
					default_tvp_pool_segment_size[before_free_slot]);
				tvp_pool->gen_pool[i] = NULL;
				tvp_pool->mm[i] = NULL;
				before_free_slot++;
			}
			if (!tvp_pool->gen_pool[i] && before_free_slot > i) {
				before_free_slot = i;
				/**/
			}
		}
	}
	tvp_pool->slot_num = ignored;
	mutex_unlock(&tvp_pool->pool_lock);
	return ignored;
}

/*
 *victor_size
 *=sizeof(res)/sizeof(ulong)
 *res[0]=addr1_start;
 *res[1]=addr1_end;
 *res[2]=addr2_start;
 *res[3]=addr2_end;
 *..
 *res[2*n]=addrx_start;
 *res[2*n+1]=addrx_end;
 *
 *return n;
 */
static int codec_mm_tvp_get_mem_resource(ulong *res, int victor_size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct extpool_mgt_s *tvp_pool = &mgt->tvp_pool;
	int i;
	mutex_lock(&tvp_pool->pool_lock);
	for (i = 0; i < tvp_pool->slot_num && i < victor_size / 2; i++) {
		if (tvp_pool->mm[i]) {
			res[2 * i] = tvp_pool->mm[i]->phy_addr;
			res[2 * i + 1] = tvp_pool->mm[i]->phy_addr +
				tvp_pool->mm[i]->buffer_size - 1;
		}
	}
	mutex_unlock(&tvp_pool->pool_lock);
	return i;
}

static int codec_mm_is_in_tvp_region(ulong phy_addr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct extpool_mgt_s *tvp_pool = &mgt->tvp_pool;
	int i;
	int in = 0, in2 = 0;

	mutex_lock(&tvp_pool->pool_lock);
	for (i = 0; i < tvp_pool->slot_num; i++) {
		if (tvp_pool->mm[i]) {
			in = tvp_pool->mm[i]->phy_addr <= phy_addr;
			in2 = (tvp_pool->mm[i]->phy_addr +
				tvp_pool->mm[i]->buffer_size - 1) >= phy_addr;
			in = in && in2;
			if (in)
				break;
			in = 0;
		}
	}
	mutex_unlock(&tvp_pool->pool_lock);
	return in;
}

void *codec_mm_phys_to_virt(unsigned long phy_addr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (codec_mm_is_in_tvp_region(phy_addr))
		return NULL;	/* no virt for tvp memory; */

	if (phy_addr >= mgt->rmem.base &&
	    phy_addr < mgt->rmem.base + mgt->rmem.size) {
		if (mgt->res_mem_flags & RES_MEM_FLAGS_HAVE_MAPED)
			return codec_mm_search_vaddr(phy_addr);
		return NULL;	/* no virt for reserved memory; */
	}

	return codec_mm_search_vaddr(phy_addr);
}
EXPORT_SYMBOL(codec_mm_phys_to_virt);

unsigned long codec_mm_virt_to_phys(void *vaddr)
{
	return page_to_phys((struct page *)vaddr);
}
EXPORT_SYMBOL(codec_mm_virt_to_phys);

unsigned long dma_get_cma_size_int_byte(struct device *dev)
{
	unsigned long size = 0;
	struct cma *cma = NULL;

	if (!dev) {
		pr_err("CMA: NULL DEV\n");
		return 0;
	}

	cma = dev_get_cma_area(dev);
	if (!cma) {
		pr_err("CMA:  NO CMA region\n");
		return 0;
	}
	size = cma_get_size(cma);
	return size;
}
EXPORT_SYMBOL(dma_get_cma_size_int_byte);

static int codec_mm_get_cma_size_int_byte(struct device *dev)
{
	static int static_size = -1;
	struct cma *cma = NULL;

	if (static_size >= 0)
		return static_size;
	if (!dev) {
		pr_err("CMA: NULL DEV\n");
		return 0;
	}

	cma = dev_get_cma_area(dev);
	if (!cma) {
		pr_err("CMA:  NO CMA region\n");
		return 0;
	}
	if (cma == dev_get_cma_area(NULL))
		static_size = 0;/*ignore default cma pool*/
	else
		static_size = cma_get_size(cma);

	return static_size;
}

static int codec_mm_cal_dump_buf_size(void)
{
	int size = PAGE_SIZE;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	unsigned long flags;

	spin_lock_irqsave(&mgt->lock, flags);
	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			size += 128 + PREFIX_MAX;
		}
	}
	spin_unlock_irqrestore(&mgt->lock, flags);

	return codec_mm_align_up2n(size, PAGE_SHIFT);
}

int get_string_offset(int *buf_len, int *pos, int len)
{
	int log_line_step = len + PREFIX_MAX;
	int remain = LOG_LINE_MAX -
		 ((*pos + log_line_step) % LOG_LINE_MAX);

	*pos = (remain > 256) ?
		(*pos + log_line_step) :
		((((*pos + log_line_step) / LOG_LINE_MAX) + 1) * LOG_LINE_MAX);
	len = (remain > 256) ?
		(len) : (*pos - *buf_len);
	*buf_len += len;

	return len;
}

int get_string_segment(int size)
{
	return (size % LOG_LINE_MAX == 0) ?
		(size / LOG_LINE_MAX) :
		(size / LOG_LINE_MAX + 1);
}

static void dump_mem_infos(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	unsigned long flags = 0;
	char *alloc_buf = NULL;
	int buf_size = 0;
	char *pbuf = NULL;
	int tsize = 0;
	int s = 0;
	int buf_len = 0;
	int segment_count = 0;
	int i = 0;

	buf_size = codec_mm_cal_dump_buf_size();
	alloc_buf = vzalloc(buf_size);
	pbuf = alloc_buf;

	s = snprintf(pbuf, buf_size - tsize,
			"codec mem info:\n");
	pbuf += get_string_offset(&buf_len, &tsize, s);

	s = snprintf(pbuf, buf_size - tsize,
			"\ttotal codec mem size:%d MB\n",
			mgt->total_codec_mem_size / SZ_1M);
	pbuf += get_string_offset(&buf_len, &tsize, s);

	s = snprintf(pbuf, buf_size - tsize,
			"\talloced size= %d MB\n",
			mgt->total_alloced_size / SZ_1M);
	pbuf += get_string_offset(&buf_len, &tsize, s);

	s = snprintf(pbuf, buf_size - tsize,
			"\tmax alloced: %d MB\n",
			mgt->max_used_mem_size / SZ_1M);
	pbuf += get_string_offset(&buf_len, &tsize, s);

	s = snprintf(pbuf, buf_size - tsize,
		"\tCMA:%d,RES:%d,TVP:%d,SYS:%d,COHER:%d,VMAPED:%d MB\n",
		mgt->alloced_cma_size / SZ_1M,
		mgt->alloced_res_size / SZ_1M,
		mgt->tvp_pool.alloced_size / SZ_1M,
		mgt->alloced_sys_size / SZ_1M,
		mgt->alloced_from_coherent / SZ_1M,
		(mgt->phys_vmaped_page_cnt << PAGE_SHIFT) / SZ_1M);
	pbuf += get_string_offset(&buf_len, &tsize, s);

	if (mgt->res_pool) {
		s = snprintf(pbuf, buf_size - tsize,
				"\t[%d]RES size:%d MB,alloced:%d MB free:%d MB\n",
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED,
				(int)(gen_pool_size(mgt->res_pool) / SZ_1M),
				(int)(mgt->alloced_res_size / SZ_1M),
				(int)(gen_pool_avail(mgt->res_pool) / SZ_1M));
		pbuf += get_string_offset(&buf_len, &tsize, s);
	}

	s = snprintf(pbuf, buf_size - tsize,
			"\t[%d]CMA size:%d MB:alloced: %d MB,free:%d MB\n",
			AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA,
			(int)(mgt->total_cma_size / SZ_1M),
			(int)(mgt->alloced_cma_size / SZ_1M),
			(int)((mgt->total_cma_size -
			mgt->alloced_cma_size) / SZ_1M));
	pbuf += get_string_offset(&buf_len, &tsize, s);

	if (mgt->tvp_pool.slot_num > 0) {
		s = snprintf(pbuf, buf_size - tsize,
				"\t[%d]TVP size:%d MB,alloced:%d MB free:%d MB\n",
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP,
				(int)(mgt->tvp_pool.total_size / SZ_1M),
				(int)(mgt->tvp_pool.alloced_size / SZ_1M),
				(int)((mgt->tvp_pool.total_size -
					mgt->tvp_pool.alloced_size) / SZ_1M));
		pbuf += get_string_offset(&buf_len, &tsize, s);
	}

	if (mgt->cma_res_pool.slot_num > 0) {
		s = snprintf(pbuf, buf_size - tsize,
				"\t[%d]CMA_RES size:%d MB,alloced:%d MB free:%d MB\n",
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES,
				(int)(mgt->cma_res_pool.total_size / SZ_1M),
				(int)(mgt->cma_res_pool.alloced_size / SZ_1M),
				(int)((mgt->cma_res_pool.total_size -
				mgt->cma_res_pool.alloced_size) / SZ_1M));
		pbuf += get_string_offset(&buf_len, &tsize, s);
	}

	spin_lock_irqsave(&mgt->lock, flags);
	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			s = snprintf(pbuf, buf_size - tsize,
					"\t[%d].%d:%s.%d,addr=0x%lx,size=%d,from=%d,cnt=%d,",
				mem->mem_id,
				mem->ins_id,
				mem->owner[0] ? mem->owner[0] : "no",
				mem->ins_buffer_id,
				mem->phy_addr,
				mem->buffer_size,
				mem->from_flags,
				atomic_read(&mem->use_cnt)
				);
			s += snprintf(pbuf + s, buf_size - tsize,
				"flags=%d,used:%u ms\n",
				mem->flags, jiffies_to_msecs(get_jiffies_64() -
				mem->alloced_jiffies));
			pbuf += get_string_offset(&buf_len, &tsize, s);

			if (tsize > buf_size - 256) {
				s = snprintf(pbuf, buf_size - tsize,
					"\n\t\t**NOT END**\n");
				tsize += s;
				break;/*no memory for dump now.*/
			}
		}
	}
	spin_unlock_irqrestore(&mgt->lock, flags);

	segment_count = get_string_segment(tsize);
	for (i = 0; i < segment_count; i++)
		pr_info("%s", alloc_buf + i * LOG_LINE_MAX);

	vfree(alloc_buf);
}

int codec_mm_alloc_cma_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->total_alloced_size / SZ_1M;
}
EXPORT_SYMBOL(codec_mm_alloc_cma_size);

static int dump_free_mem_infos(void *buf, int size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem;
	unsigned long flags;
	int i = 0, j, k;
	unsigned long minphy;
	u32 cma_base_phy, cma_end_phy;
	struct dump_buf_s {
		unsigned long phy_addr;
		int buffer_size;
		int align2n;
	} *usedb, *freeb;
	int total_free_size = 0;

	usedb = kzalloc(sizeof(*usedb) * 256, GFP_KERNEL);
	if (!usedb)
		return 0;
	freeb = usedb + 128;

	spin_lock_irqsave(&mgt->lock, flags);
	list_for_each_entry(mem, &mgt->mem_list, list) {
		usedb[i].phy_addr = mem->phy_addr;
		usedb[i].buffer_size = mem->buffer_size;
		usedb[i].align2n = mem->align2n;
		if (++i >= 128) {
			pr_info("too many memlist in codec_mm, break;\n");
			break;
		}
	};
	cma_base_phy = cma_get_base(dev_get_cma_area(mgt->dev));
	cma_end_phy = cma_base_phy + dma_get_cma_size_int_byte(mgt->dev);
	spin_unlock_irqrestore(&mgt->lock, flags);
	pr_info("free cma idea[%x-%x] from codec_mm items %d\n", cma_base_phy,
		cma_end_phy, i);
	for (j = 0; j < i; j++) { /* free */
		freeb[j].phy_addr = usedb[j].phy_addr +
			codec_mm_align_up2n(usedb[j].buffer_size, usedb[j].align2n);
		minphy = cma_end_phy;
		for (k = 0; k < i; k++) { /* used */
			if (usedb[k].phy_addr >= freeb[j].phy_addr &&
			    usedb[k].phy_addr < minphy)
				minphy = usedb[k].phy_addr;
		}
		freeb[j].buffer_size = minphy - freeb[j].phy_addr;
		total_free_size += freeb[j].buffer_size;
		if (freeb[j].buffer_size > 0)
			pr_info("\t[%d] free buf phyaddr=%p, used [%p,%x], size=%x\n",
				j, (void *)freeb[j].phy_addr,
				(void *)usedb[j].phy_addr,
				usedb[j].buffer_size, freeb[j].buffer_size);
	}
	pr_info("total_free_size %x\n", total_free_size);
	kfree(usedb);
	return 0;
}

static void codec_mm_tvp_segment_init(void)
{
	u32 size = 0;
	u32 segment_size = 0;
	u32 rest_size = 0;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	/*2M for audio not protect.*/
	if (tvp_dynamic_increase_disable) {
		if (default_tvp_4k_size <= 0 ||
			(tvp_dynamic_alloc_max_size > 0 &&
				default_tvp_4k_size >= tvp_dynamic_alloc_max_size)) {
			default_tvp_4k_size = mgt->total_codec_mem_size - SZ_1M * 2;
			if (default_tvp_4k_size > DEFAULT_TVP_SIZE_FOR_4K)
				default_tvp_4k_size = DEFAULT_TVP_SIZE_FOR_4K;
		}
		/*97MB -> 160MB, may not enough for h265*/
		default_tvp_size = (mgt->total_codec_mem_size - (SZ_1M * 2)) >
				DEFAULT_TVP_SIZE_FOR_NO4K ?
					DEFAULT_TVP_SIZE_FOR_NO4K :
					default_tvp_4k_size;
		return;
	}
	default_tvp_4k_size = mgt->total_codec_mem_size - SZ_1M * 2;
	default_tvp_size = default_tvp_4k_size;

	if (default_tvp_pool_size_0 > default_tvp_size) {
		default_tvp_pool_size_0 = default_tvp_size;
		default_tvp_pool_size_1 = 0;
		default_tvp_pool_size_2 = 0;
	}

	size = default_tvp_pool_size_0 +
		default_tvp_pool_size_1 + default_tvp_pool_size_2;
	if (size > default_tvp_size) {
		default_tvp_pool_size_0 = 0;
		default_tvp_pool_size_1 = 0;
		default_tvp_pool_size_2 = 0;
	}

	if (default_tvp_pool_size_0 >= DEFAULT_TVP_SEGMENT_MIN_SIZE) {
		default_tvp_pool_segment_size[0] =
			default_tvp_pool_size_0;
	} else {
		segment_size = default_tvp_size / 3;
		default_tvp_pool_segment_size[0] = segment_size;
	}
	size = default_tvp_pool_segment_size[0];
	if (default_tvp_pool_size_1 >= DEFAULT_TVP_SEGMENT_MIN_SIZE) {
		default_tvp_pool_segment_size[1] =
			default_tvp_pool_size_1;
	} else {
		segment_size = default_tvp_size / 4;
		rest_size = default_tvp_size - size;
		default_tvp_pool_segment_size[1] =
			(rest_size > segment_size) ? segment_size : rest_size;
	}
	size += default_tvp_pool_segment_size[1];
	if (default_tvp_pool_size_2 >= DEFAULT_TVP_SEGMENT_MIN_SIZE) {
		default_tvp_pool_segment_size[2] =
			default_tvp_pool_size_2;
	} else {
		segment_size = default_tvp_size / 4;
		rest_size = default_tvp_size - size;
		default_tvp_pool_segment_size[2] =
			(rest_size > segment_size) ? segment_size : rest_size;
	}
	size += default_tvp_pool_segment_size[2];
	rest_size = default_tvp_size - size;
	if (rest_size >= DEFAULT_TVP_SEGMENT_MIN_SIZE)
		default_tvp_pool_segment_size[3] = rest_size;
	else
		default_tvp_pool_segment_size[3] = 0;
	size += default_tvp_pool_segment_size[3];
	default_tvp_4k_size = size;
	default_tvp_size = size;
	tvp_dynamic_alloc_max_size = size;
}

static int codec_mm_calc_init_pool_count(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct extpool_mgt_s *tvp_pool = &mgt->tvp_pool;
	u32 size = default_tvp_pool_segment_size[tvp_pool->slot_num];
	u32 need_pool_count = 0;
	u32 last_segment_size = 0;
	u32 segment_size[TVP_POOL_SEGMENT_MAX_USED] = { 0 };
	u32 i = 0;

	if (tvp_dynamic_alloc_force_small_segment > 0) {
		if (tvp_dynamic_alloc_force_small_segment_size == 0)
			tvp_dynamic_alloc_force_small_segment_size =
				50 * 1024 * 1024;
		need_pool_count =
			size / tvp_dynamic_alloc_force_small_segment_size;
		last_segment_size =
			size % tvp_dynamic_alloc_force_small_segment_size;
		if (last_segment_size < DEFAULT_TVP_SEGMENT_MIN_SIZE)
			last_segment_size = 0;
		else
			need_pool_count++;
		if (need_pool_count <= TVP_POOL_SEGMENT_MAX_USED) {
			for (i = 0; i < need_pool_count; i++) {
				if (i == need_pool_count - 1 &&
					last_segment_size > 0)
					segment_size[i] =
						last_segment_size;
				else
					segment_size[i] =
				tvp_dynamic_alloc_force_small_segment_size;
			}
			if (need_pool_count > TVP_POOL_SEGMENT_MAX_USED - 1)
				need_pool_count = TVP_POOL_SEGMENT_MAX_USED - 1;
			last_segment_size =
				mgt->total_codec_mem_size - 2 * SZ_1M;
			for (i = 0; i < TVP_POOL_SEGMENT_MAX_USED - 1; i++) {
				if (i < need_pool_count) {
					last_segment_size -= segment_size[i];
					default_tvp_pool_segment_size[i] =
						segment_size[i];
				} else {
					last_segment_size -=
					default_tvp_pool_segment_size[i];
				}
			}
		default_tvp_pool_segment_size[TVP_POOL_SEGMENT_MAX_USED - 1] =
				last_segment_size;
		} else {
			need_pool_count = 1;
		}
	} else {
		need_pool_count = 1;
	}
	return need_pool_count;
}

int codec_mm_enable_tvp(int size, int flags)
{
	int ret;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	u32 count = 0;
	u32 i = 0;

	mutex_lock(&mgt->tvp_protect_lock);
	if (mgt->tvp_pool.slot_num <= 0)
		codec_mm_tvp_segment_init();
	if (size == 0) {
		if (flags == 1)
			size = default_tvp_size;
		else
			size = default_tvp_4k_size;
	}
	if (tvp_dynamic_increase_disable) {
		ret = codec_mm_extpool_pool_alloc(&mgt->tvp_pool,
			size, 0, 1);
		if (ret) {
			ret = 0;
			mgt->tvp_enable = flags;
			if (tvp_mode > 0)
				atomic_add_return(1, &mgt->tvp_user_count);
			pr_info("enable tvp for %d\n", flags);
		} else {
			pr_info("tvp enable failed size %d\n", size);
			mutex_unlock(&mgt->tvp_protect_lock);
			return -1;
		}
	} else {
		if (mgt->tvp_pool.slot_num <= 0) {
			count = codec_mm_calc_init_pool_count();
			if (count > 1) {
				for (i = 0; i < count; i++) {
					ret =
					codec_mm_tvp_pool_alloc_by_slot
					(&mgt->tvp_pool, 0, flags);
					if (ret) {
						ret = 0;
						continue;
					}
					if (i == 0) {
						pr_info("tvp enable failed\n");
						mutex_unlock
						(&mgt->tvp_protect_lock);
						return -1;
					}
					break;
				}
				if (mgt->tvp_enable != 2)
					mgt->tvp_enable = flags;
				if (tvp_mode > 0)
					atomic_add_return(1,
						&mgt->tvp_user_count);
				pr_info("enable tvp %d %d %d %d\n",
					flags, count, i, atomic_read
					(&mgt->tvp_user_count));
			} else {
				ret = codec_mm_tvp_pool_alloc_by_slot
					(&mgt->tvp_pool, 0, flags);
				if (ret) {
					ret = 0;
					if (mgt->tvp_enable != 2)
						mgt->tvp_enable = flags;
					if (tvp_mode > 0)
						atomic_add_return(1,
							&mgt->tvp_user_count);
					pr_info("enable tvp for %d\n", flags);
				} else {
					pr_info("tvp enable failed size %d\n",
						size);
					mutex_unlock(&mgt->tvp_protect_lock);
					return -1;
				}
			}
		} else {
			ret = 0;
			if (mgt->tvp_enable != 2)
				mgt->tvp_enable = flags;
			if (tvp_mode > 0)
				atomic_add_return(1, &mgt->tvp_user_count);
			pr_info("enable tvp for %d\n", flags);
		}
	}
	if (tvp_mode > 0)
		pr_info("tvp_user_count is %d\n",
			atomic_read(&mgt->tvp_user_count));
	mutex_unlock(&mgt->tvp_protect_lock);
	return ret;
}
EXPORT_SYMBOL(codec_mm_enable_tvp);

int codec_mm_disable_tvp(void)
{
	int ret = 0;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	mutex_lock(&mgt->tvp_protect_lock);
	if (tvp_mode == 0) {
		ret = codec_mm_extpool_pool_release(&mgt->tvp_pool);
		mgt->tvp_enable = 0;
		pr_info("disalbe tvp\n");
		mutex_unlock(&mgt->tvp_protect_lock);
		return ret;
	}
	if (atomic_dec_and_test(&mgt->tvp_user_count)) {
		if (codec_mm_tvp_pool_unprotect_and_release(&mgt->tvp_pool) == 0) {
			mgt->tvp_enable = 0;
			pr_info("disalbe tvp\n");
			mutex_unlock(&mgt->tvp_protect_lock);
			return ret;
		}
	}
	if (atomic_read(&mgt->tvp_user_count) < 0)
		atomic_set(&mgt->tvp_user_count, 0);
	pr_info("tvp_user_count is %d\n",
		atomic_read(&mgt->tvp_user_count));
	mutex_unlock(&mgt->tvp_protect_lock);
	return ret;
}
EXPORT_SYMBOL(codec_mm_disable_tvp);

int codec_mm_video_tvp_enabled(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->tvp_enable;
}
EXPORT_SYMBOL(codec_mm_video_tvp_enabled);

int codec_mm_get_total_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int total_size = mgt->total_codec_mem_size;

	if ((debug_mode & 0xf) == 0) {	/*no debug memory mode. */
		return total_size;
	}
	/*
	 *disable reserved:1
	 *disable cma:2
	 *disable sys memory:4
	 *disable half memory:8,
	 */
	if (debug_mode & 0x8)
		total_size -= mgt->total_codec_mem_size / 2;
	if (debug_mode & 0x1) {
		total_size -= mgt->total_reserved_size;
		total_size -= mgt->cma_res_pool.total_size;
	}
	if (debug_mode & 0x2)
		total_size -= mgt->total_cma_size;
	if (total_size < 0)
		total_size = 0;
	return total_size;
}
EXPORT_SYMBOL(codec_mm_get_total_size);

/*count remain size for no tvp*/
int codec_mm_get_free_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	if (debug_mode & 0x20)
		dump_mem_infos();
	return codec_mm_get_total_size() -
		mgt->tvp_pool.total_size  + mgt->tvp_pool.alloced_size -
		mgt->total_alloced_size;
}
EXPORT_SYMBOL(codec_mm_get_free_size);

int codec_mm_get_tvp_free_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	if (debug_mode & 0x20)
		dump_mem_infos();
	return mgt->tvp_pool.total_size -
		mgt->tvp_pool.alloced_size;
}

int codec_mm_get_reserved_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->total_reserved_size;
}
EXPORT_SYMBOL(codec_mm_get_reserved_size);

struct device *v4l_get_dev_from_codec_mm(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->dev;
}
EXPORT_SYMBOL(v4l_get_dev_from_codec_mm);

struct codec_mm_s *v4l_reqbufs_from_codec_mm(const char *owner,
						 unsigned int addr,
						 unsigned int size,
						 unsigned int index)
{
	unsigned long flags;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	int buf_size = PAGE_ALIGN(size);

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (IS_ERR_OR_NULL(mem))
		goto out;

	mem->owner[0]	= owner;
	mem->mem_handle  = NULL;
	mem->buffer_size = buf_size;
	mem->page_count  = buf_size / PAGE_SIZE;
	mem->phy_addr	= addr;
	mem->ins_buffer_id = index;
	mem->alloced_jiffies = get_jiffies_64();
	mem->from_flags =
		AMPORTS_MEM_FLAGS_FROM_GET_FROM_COHERENT;

	spin_lock_irqsave(&mgt->lock, flags);

	mem->mem_id = mgt->global_memid++;
	mgt->alloced_from_coherent += buf_size;
	mgt->total_alloced_size += buf_size;
	mgt->alloced_cma_size += buf_size;
	list_add_tail(&mem->list, &mgt->mem_list);

	spin_unlock_irqrestore(&mgt->lock, flags);

	if (debug_mode & 0x20)
		pr_info("mem_id [%d] %s alloc coherent size %d at %lx from %d.\n",
			mem->mem_id, owner, buf_size, mem->phy_addr, mem->from_flags);
out:
	return mem;
}
EXPORT_SYMBOL(v4l_reqbufs_from_codec_mm);

void v4l_freebufs_back_to_codec_mm(const char *owner, struct codec_mm_s *mem)
{
	unsigned long flags;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (IS_ERR_OR_NULL(mem))
		return;

	if (!mem->owner[0] || strcmp(owner, mem->owner[0]) ||
		!mem->buffer_size)
		goto out;

	spin_lock_irqsave(&mgt->lock, flags);

	mgt->alloced_from_coherent -= mem->buffer_size;
	mgt->total_alloced_size -= mem->buffer_size;
	mgt->alloced_cma_size -= mem->buffer_size;
	list_del(&mem->list);

	spin_unlock_irqrestore(&mgt->lock, flags);

	if (debug_mode & 0x20)
		pr_info("mem_id [%d] %s free mem size %d at %lx from %d\n", mem->mem_id,
			mem->owner[0], mem->buffer_size, mem->phy_addr, mem->from_flags);
out:
	kfree(mem);
}
EXPORT_SYMBOL(v4l_freebufs_back_to_codec_mm);

/*
 *with_wait:
 *1: if no mem, do wait and free some cache.
 *0: do not wait.
 */
int codec_mm_enough_for_size(int size, int with_wait, int mem_flags)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int have_mem = codec_mm_alloc_pre_check_in(mgt, size, mem_flags);

	if (!have_mem && with_wait && mgt->alloced_for_sc_cnt > 0) {
		pr_err(" No mem, clear scatter cache!!\n");
		//codec_mm_scatter_free_all_ignorecache(1);
		have_mem = codec_mm_alloc_pre_check_in(mgt, size, 0);
		if (have_mem)
			return 1;
		if (debug_mode & 0x20)
			dump_mem_infos();
		msleep(50);
		return 0;
	}
	return 1;
}
EXPORT_SYMBOL(codec_mm_enough_for_size);

int codec_mm_mgt_init(struct device *dev)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int ret;

	INIT_LIST_HEAD(&mgt->mem_list);
	mgt->dev = dev;
	mgt->alloc_from_sys_pages_max = 4;
	if (mgt->rmem.size > 0) {
		unsigned long aligned_addr;
		int aligned_size;
		/*mem aligned, */
		mgt->res_pool = gen_pool_create(RESERVE_MM_ALIGNED_2N, -1);
		if (!mgt->res_pool)
			return -ENOMEM;
		aligned_addr = ((unsigned long)mgt->rmem.base +
			((1 << RESERVE_MM_ALIGNED_2N) - 1)) &
			(~((1 << RESERVE_MM_ALIGNED_2N) - 1));
		aligned_size = mgt->rmem.size -
			(int)(aligned_addr - (unsigned long)mgt->rmem.base);
		ret = gen_pool_add(mgt->res_pool,
				 aligned_addr, aligned_size, -1);
		if (ret < 0) {
			gen_pool_destroy(mgt->res_pool);
			return -1;
		}
		pr_debug("add reserve memory %p(aligned %p) size=%x(aligned %x)\n",
			 (void *)mgt->rmem.base, (void *)aligned_addr,
			 (int)mgt->rmem.size, (int)aligned_size);
		mgt->total_reserved_size = aligned_size;
		mgt->total_codec_mem_size = aligned_size;
#ifdef RES_IS_MAPED
		mgt->res_mem_flags |= RES_MEM_FLAGS_HAVE_MAPED;
#endif
	}
	mgt->total_cma_size = codec_mm_get_cma_size_int_byte(mgt->dev);
	mgt->total_codec_mem_size += mgt->total_cma_size;
	if (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) <
		MESON_CPU_MAJOR_ID_G12A)
		tvp_dynamic_increase_disable = 1;
	default_tvp_4k_size = 0;
	codec_mm_tvp_segment_init();
	default_cma_res_size = mgt->total_cma_size;
	mgt->global_memid = 0;
	mutex_init(&mgt->tvp_pool.pool_lock);
	mutex_init(&mgt->cma_res_pool.pool_lock);
	spin_lock_init(&mgt->lock);
	return 0;
}
EXPORT_SYMBOL(codec_mm_mgt_init);

void codec_mm_get_default_tvp_size(int *tvp_fhd, int *tvp_uhd)
{
	if (tvp_dynamic_increase_disable) {
		if (tvp_fhd)
			*tvp_fhd = default_tvp_size;
		if (tvp_uhd)
			*tvp_uhd = default_tvp_4k_size;
	} else {
		if (tvp_fhd) {
			*tvp_fhd = default_tvp_size <
				DEFAULT_TVP_SIZE_FOR_NO4K ?
				default_tvp_size : DEFAULT_TVP_SIZE_FOR_NO4K;
		}
		if (tvp_uhd) {
			*tvp_uhd = default_tvp_4k_size <
				DEFAULT_TVP_SIZE_FOR_4K ?
				default_tvp_4k_size : DEFAULT_TVP_SIZE_FOR_4K;
		}
	}
}
EXPORT_SYMBOL(codec_mm_get_default_tvp_size);

static int __init amstream_test_init(void)
{
	return 0;
}

static ssize_t codec_mm_dump_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	size_t ret = 0;

	dump_mem_infos();
	return ret;
}

static ssize_t codec_mm_scatter_dump_show(struct class *class,
					  struct class_attribute *attr,
					  char *buf)
{
	size_t ret;

	ret = codec_mm_scatter_info_dump(buf, PAGE_SIZE);
	return ret;
}

static ssize_t codec_mm_keeper_dump_show(struct class *class,
					 struct class_attribute *attr,
					 char *buf)
{
	size_t ret;

	ret = codec_mm_keeper_dump_info(buf, PAGE_SIZE);
	return ret;
}

static ssize_t tvp_enable_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	ssize_t size = 0;

	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	mutex_lock(&mgt->tvp_protect_lock);
	size += sprintf(buf, "tvp_flag=%d\n",
			(tvp_mode << 4) + mgt->tvp_enable);
	size += sprintf(buf + size, "tvp ref count=%d\n",
			atomic_read(&mgt->tvp_user_count));
	size += sprintf(buf + size, "tvp enable help:\n");
	size += sprintf(buf + size, "echo n > tvp_enable\n");
	size += sprintf(buf + size, "0: disable tvp(tvp size to 0)\n");
	size += sprintf(buf + size,
		"1: enable tvp for 1080p playing(use default size)\n");
	size += sprintf(buf + size,
		"2: enable tvp for 4k playing(use default 4k size)\n");
	mutex_unlock(&mgt->tvp_protect_lock);
	return size;
}

static ssize_t tvp_enable_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned int val;
	ssize_t ret;

	val = -1;
	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	/*
	 * always free all scatter cache for
	 * tvp changes when tvp mode is 0.
	 */
	if (tvp_mode < 1) {
		mutex_lock(&mgt->tvp_protect_lock);
		codec_mm_keeper_free_all_keep(2);
		codec_mm_scatter_free_all_ignorecache(3);
		mutex_unlock(&mgt->tvp_protect_lock);
	}
	switch (val) {
		case 0:
			codec_mm_disable_tvp();
			break;
		case 1:
			ret = codec_mm_enable_tvp(default_tvp_size, val);
			if (ret) {
				pr_info("tvp enable failed tvp mode %d %d %d\n",
					tvp_mode, val, default_tvp_size);
				return -1;
			}
			break;
		case 2:
			ret = codec_mm_enable_tvp(default_tvp_4k_size, val);
			if (ret) {
				pr_info("tvp enable failed tvp mode %d %d %d\n",
					tvp_mode, val, default_tvp_4k_size);
				return -1;
			}
			break;
		default:
			pr_err("unknown cmd! %d\n", val);
			break;
	}
	return size;
}

static ssize_t fastplay_enable_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	ssize_t size = 0;

	size += sprintf(buf, "fastplay enable help:\n");
	size += sprintf(buf + size, "echo n > tvp_enable\n");
	size += sprintf(buf + size, "0: disable fastplay\n");
	size += sprintf(buf + size,
		"1: enable fastplay for 1080p playing(use default size)\n");
	return size;
}

static ssize_t fastplay_enable_store(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned int val;
	ssize_t ret;

	val = -1;
	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	switch (val) {
	case 0:
		ret = codec_mm_extpool_pool_release(&mgt->cma_res_pool);
		mgt->fastplay_enable = 0;
		pr_err("disalbe fastplay ret 0x%zx\n", ret);
		break;
	case 1:
		codec_mm_extpool_pool_alloc(&mgt->cma_res_pool,
						default_cma_res_size, 0, 0);
		mgt->fastplay_enable = 1;
		pr_err("enable fastplay\n");
		break;
	default:
		pr_err("unknown cmd! %d\n", val);
	}
	return size;
}

static ssize_t config_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	ssize_t ret;

	ret = configs_list_path_nodes(CONFIG_PATH, buf, PAGE_SIZE,
					  LIST_MODE_NODE_CMDVAL_ALL);
	return ret;
}

static ssize_t config_store(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	int ret;

	ret = configs_set_prefix_path_valonpath(CONFIG_PREFIX, buf);
	if (ret < 0)
		pr_err("set config failed %s\n", buf);
	return size;
}

static ssize_t tvp_region_show(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	size_t ret;
	ulong res_victor[16];
	int i;
	int off = 0;

	ret = codec_mm_tvp_get_mem_resource(res_victor, 8);
	for (i = 0; i < ret; i++) {
		off += sprintf(buf + off,
			"segment%d:0x%lx - 0x%lx (size:0x%x)\n",
			i,
			res_victor[2 * i],
			res_victor[2 * i + 1],
			(int)(res_victor[2 * i + 1] - res_victor[2 * i] + 1));
	}
	return off;
}

static ssize_t debug_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	ssize_t size = 0;

	size += sprintf(buf, "mm_scatter help:\n");
	size += sprintf(buf + size, "echo n > mm_scatter_debug\n");
	size += sprintf(buf + size, "n==0: clear all debugs)\n");
	size += sprintf(buf + size,
	"n=1: dump all alloced scatters\n");
	size += sprintf(buf + size,
	"n=2: dump all slots\n");

	size += sprintf(buf + size,
	"n=3: dump all free slots\n");

	size += sprintf(buf + size,
	"n=4: dump all sid hash table\n");

	size += sprintf(buf + size,
	"n=5: free all free slot now!\n");

	size += sprintf(buf + size,
	"n=6: clear all time infos!\n");

	size += sprintf(buf + size,
	"n=10: force free all keeper\n");

	size += sprintf(buf + size,
	"n=20: dump memory,# 20 #addr(hex) #len\n");

	size += sprintf(buf + size,
	"n==100: cmd mode p1 p ##mode:0,dump, 1,alloc 2,more,3,free some,4,free all\n");

	size += sprintf(buf + size,
	"n=200: walk dmabuf infos.\n");

	return size;
}

static int codec_mm_mem_dump(unsigned long addr, int isphy, int len)
{
	void *vaddr;
	int is_map = 0;

	pr_info("start dump addr: %p %d\n", (void *)addr, len);
	if (!isphy) {
		vaddr = (void *)addr;
	} else {
		vaddr = ioremap_nocache(addr, len);
		if (!vaddr) {
			pr_info("map addr: %p len: %d, failed\n",
				(void *)addr, len);
			vaddr = codec_mm_phys_to_virt(addr);
		} else {
			is_map = 1;
		}
	}
	if (vaddr) {
		unsigned int *p, *vint;
		int i;

		vint = (unsigned int *)vaddr;
		for (i = 0; i <= len - 32; i += sizeof(int) * 8) {
			p = (int *)&vint[i];
			pr_info("%p: %08x %08x %08x %08x %08x %08x %08x %08x\n",
				p,
				p[0], p[1], p[2], p[3],
				p[4], p[5], p[6], p[7]);
		}
	}
	if (vaddr && is_map) {
		/*maped free...*/
		iounmap(vaddr);
	}
	return 0;
}

static ssize_t debug_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val;
	ssize_t ret;

	val = -1;
	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	switch (val) {
	case 0:
		pr_info("clear debug!\n");
		break;
	case 1:
		codec_mm_dump_all_scatters();
		break;
	case 2:
		codec_mm_dump_all_slots();
		break;
	case 3:
		codec_mm_dump_free_slots();
		break;
	case 4:
		codec_mm_dump_all_hash_table();
		break;
	case 5:
		codec_mm_free_all_free_slots();
		break;
	case 6:
		codec_mm_clear_alloc_infos();
		break;
	case 10:
		codec_mm_keeper_free_all_keep(1);
		break;
	case 11:
		dump_mem_infos();
		break;
	case 12:
		dump_free_mem_infos(NULL, 0);
		break;
	case 20: {
		int cmd = 0, len = 0;
		unsigned int addr;

		addr = 0;
		ret = sscanf(buf, "%d %x %d", &cmd, &addr, &len);
		if (addr > 0 && len > 0)
			codec_mm_mem_dump(addr, 1, len);
		}
		break;
	case 100:{
			int cmd, mode = 0, p1 = 0, p2 = 0;

			ret = sscanf(buf, "%d %d %d %d", &cmd, &mode, &p1, &p2);
			codec_mm_scatter_test(mode, p1, p2);
		}
		break;
	case 200:
		codec_mm_walk_dbuf();
		break;
	default:
		pr_err("unknown cmd! %d\n", val);
	}
	return size;

}

static ssize_t debug_sc_mode_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "%u\n", debug_sc_mode);

	return size;
}

static ssize_t debug_sc_mode_store(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t size)
{
	unsigned int val;
	ssize_t ret;

	val = -1;
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;

	debug_sc_mode = val;

	return size;
}

static ssize_t debug_keep_mode_show(struct class *class,
					struct class_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "%u\n", debug_keep_mode);

	return size;
}

static ssize_t debug_keep_mode_store(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned int val;
	ssize_t ret;

	val = -1;
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;

	debug_keep_mode = val;

	return size;
}

bool codec_mm_scatter_available_check(int alloc)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	/* if codec_mm_scatter_watermark has been configed, use scatter
	 * free size to check, else use reserved size to check
	 */
	if (mgt->codec_mm_scatter_watermark) {
		if (mgt->codec_mm_scatter_free_size > alloc)
			return true;
	} else {
		if (codec_mm_get_free_size() >
			codec_mm_scatter_get_reserved_size())
			return true;
	}
	if (debug_mode & 0x80)
		pr_info("[%s]%s not enough. (watermark:%d/%d)\n", __func__,
				mgt->codec_mm_for_linear ? "scatter watermark" : "reserved",
				mgt->codec_mm_scatter_free_size,
				mgt->codec_mm_scatter_watermark);
	return false;
}

void codec_mm_scatter_level_decrease(int alloc_size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned long flags;

	if (mgt->codec_mm_for_linear) {
		spin_lock_irqsave(&mgt->scatter_watermark_lock, flags);
		mgt->codec_mm_scatter_free_size -= alloc_size;
		spin_unlock_irqrestore(&mgt->scatter_watermark_lock, flags);
	}
}

void codec_mm_scatter_level_increase(int free_size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned long flags;

	if (mgt->codec_mm_for_linear) {
		spin_lock_irqsave(&mgt->scatter_watermark_lock, flags);
		mgt->codec_mm_scatter_free_size += free_size;
		spin_unlock_irqrestore(&mgt->scatter_watermark_lock, flags);
	}
}

void codec_mm_set_min_linear_size(int min_mem_val)
{
	int val = min_mem_val;
	unsigned long flags;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (val <= 0)
		return;

	if (val > codec_mm_get_total_size()) {
		pr_err("[%s]set value %d is too large, set as %d\n",
				__func__, val, codec_mm_get_total_size());
		val = codec_mm_get_total_size();
	}

	if (!mgt->codec_mm_for_linear) { // first time set
		spin_lock_init(&mgt->scatter_watermark_lock);
		mgt->codec_mm_for_linear = val;
		mgt->codec_mm_scatter_watermark =
			codec_mm_get_total_size() - mgt->codec_mm_for_linear;
		mgt->codec_mm_scatter_free_size =
			mgt->codec_mm_scatter_watermark;
	} else { // has been set, just need to make adjustment
		spin_lock_irqsave(&mgt->scatter_watermark_lock, flags);
		mgt->codec_mm_scatter_watermark += (mgt->codec_mm_for_linear - val);
		mgt->codec_mm_scatter_free_size += (mgt->codec_mm_for_linear - val);
		mgt->codec_mm_for_linear = val;
		spin_unlock_irqrestore(&mgt->scatter_watermark_lock, flags);
		if (mgt->codec_mm_scatter_free_size < 0)
			pr_warn_once("[%s]adjust watermark, scatter need free %d cma\n",
				__func__, mgt->codec_mm_scatter_free_size);
	}

	if (debug_mode & 0x80)
		pr_info("[%s]linear:%d, scatter:%d/%d\n",
					__func__, mgt->codec_mm_for_linear,
					mgt->codec_mm_scatter_free_size,
					mgt->codec_mm_scatter_watermark);
}

int codec_mm_get_min_linear_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->codec_mm_for_linear;
}

int codec_mm_get_scatter_watermark(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->codec_mm_scatter_watermark;
}

static ssize_t dbuf_trace_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	char *pbuf = buf;

	pbuf += sprintf(pbuf, "Dmabuf tracking [%s]\n",
		mgt->trk_h ? "enable" : "disable");

	return pbuf - buf;
}

static ssize_t dbuf_trace_store(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	bool val = false;
	ssize_t ret;

	ret = kstrtobool(buf, &val);
	if (ret != 0)
		return -EINVAL;

	if (val) {
		codec_mm_track_open(&mgt->trk_h);
	} else {
		codec_mm_track_close(mgt->trk_h);
		mgt->trk_h = NULL;
	}

	dbuf_trace = val;

	return size;
}

static CLASS_ATTR_RO(codec_mm_dump);
static CLASS_ATTR_RO(codec_mm_scatter_dump);
static CLASS_ATTR_RO(codec_mm_keeper_dump);
static CLASS_ATTR_RO(tvp_region);
static CLASS_ATTR_RW(tvp_enable);
static CLASS_ATTR_RW(fastplay_enable);
static CLASS_ATTR_RW(config);
static CLASS_ATTR_RW(debug);
//static CLASS_ATTR_RW(debug_mode);
static CLASS_ATTR_RW(debug_sc_mode);
static CLASS_ATTR_RW(debug_keep_mode);
static CLASS_ATTR_RW(dbuf_trace);

static struct attribute *codec_mm_class_attrs[] = {
	&class_attr_codec_mm_dump.attr,
	&class_attr_codec_mm_scatter_dump.attr,
	&class_attr_codec_mm_keeper_dump.attr,
	&class_attr_tvp_region.attr,
	&class_attr_tvp_enable.attr,
	&class_attr_fastplay_enable.attr,
	&class_attr_config.attr,
	&class_attr_debug.attr,
	//&class_attr_debug_mode.attr,
	&class_attr_debug_sc_mode.attr,
	&class_attr_debug_keep_mode.attr,
	&class_attr_dbuf_trace.attr,
	NULL
};

ATTRIBUTE_GROUPS(codec_mm_class);

static struct class codec_mm_class = {
		.name = "codec_mm",
		.class_groups = codec_mm_class_groups,
	};

static struct mconfig codec_mm_configs[] = {
	MC_PI32("default_tvp_size", &default_tvp_size),
	MC_PI32("default_tvp_4k_size", &default_tvp_4k_size),
	MC_PI32("default_cma_res_size", &default_cma_res_size),
	MC_PI32("default_tvp_pool_size_0", &default_tvp_pool_size_0),
	MC_PI32("default_tvp_pool_size_1", &default_tvp_pool_size_1),
	MC_PI32("default_tvp_pool_size_2", &default_tvp_pool_size_2),
};

static struct mconfig_node codec_mm_trigger_node;

int codec_mm_trigger_fun(const char *trigger, int id, const char *buf, int size)
{
	int ret = size;

	switch (trigger[0]) {
	case 't':
		tvp_enable_store(NULL, NULL, buf, size);
		break;
	case 'f':
		fastplay_enable_store(NULL, NULL, buf, size);
		break;
	case 'd':
		debug_store(NULL, NULL, buf, size);
		break;
	default:
		ret = -1;
	}
	return size;
}

int codec_mm_trigger_help_fun(const char *trigger, int id, char *sbuf, int size)
{
	int ret = -1;
	void *buf, *getbuf = NULL;

	if (size < PAGE_SIZE) {
		getbuf = (void *)__get_free_page(GFP_KERNEL);

		if (!getbuf)
			return -ENOMEM;
		buf = getbuf;
	} else {
		buf = sbuf;
	}
	switch (trigger[0]) {
	case 't':
		ret = tvp_enable_show(NULL, NULL, buf);
		break;
	case 'f':
		ret = fastplay_enable_show(NULL, NULL, buf);
		break;
	case 'd':
		ret = debug_show(NULL, NULL, buf);
		break;
	default:
		pr_err("unknown trigger:[%s]\n", trigger);
		ret = -1;
	}
	if (ret > 0 && getbuf) {
		ret = min_t(int, ret, size);

		strncpy(sbuf, buf, ret);
	}
	if (getbuf)
		free_page((unsigned long)getbuf);
	return ret;
}

static struct mconfig codec_mm_trigger[] = {
	MC_FUN("tvp_enable", codec_mm_trigger_help_fun, codec_mm_trigger_fun),
	MC_FUN("fastplay", codec_mm_trigger_help_fun, codec_mm_trigger_fun),
	MC_FUN("debug", codec_mm_trigger_help_fun, codec_mm_trigger_fun),
};

static int codec_mm_probe(struct platform_device *pdev)
{
	int r;
	struct reserved_mem *mem = NULL;
	int secure_region_index = 0;
	struct device_node *search_target = NULL;

	while (1) {
		search_target = of_parse_phandle(pdev->dev.of_node,
						"memory-region",
						secure_region_index);
		if (!search_target)
			break;

		if (!strcmp(search_target->name, "linux,secure_vdec_reserved")) {
			mem = of_reserved_mem_lookup(search_target);
			if (mem) {
				r = secure_vdec_res_setup(mem);
				if (r)
					pr_err("secure_vdec_res_setup res %x\n", r);
				r = of_reserved_mem_device_init_by_idx(&pdev->dev,
					pdev->dev.of_node, secure_region_index);
				if (r)
					pr_err("secure_vdec_res_setup device init failed\n");
			}
			break;
		}
		secure_region_index++;
	}

	pdev->dev.platform_data = get_mem_mgt();
	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		pr_debug("%s mem init done\n", __func__);

	codec_mm_mgt_init(&pdev->dev);
	r = class_register(&codec_mm_class);
	if (r) {
		pr_err("vdec class create fail.\n");
		return r;
	}
	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		pr_debug("codec_mm reserved memory probed done\n");

	pr_info("%s ok\n", __func__);

	codec_mm_scatter_mgt_init(&pdev->dev);
	codec_mm_keeper_mgr_init();
	amstream_test_init();
	codec_mm_scatter_mgt_test();
	REG_PATH_CONFIGS(CONFIG_PATH, codec_mm_configs);
	INIT_REG_NODE_CONFIGS(CONFIG_PATH, &codec_mm_trigger_node,
				  "trigger", codec_mm_trigger,
				  CONFIG_FOR_RW | CONFIG_FOR_T);
	return 0;
}

static const struct of_device_id amlogic_mem_dt_match[] = {
	{
			.compatible = "amlogic, codec, mm",
		},
	{},
};

static struct platform_driver codec_mm_driver = {
	.probe = codec_mm_probe,
	.remove = NULL,
	.driver = {
			.owner = THIS_MODULE,
			.name = "codec_mm",
			.of_match_table = amlogic_mem_dt_match,
		}
};

int __init codec_mm_module_init(void)
{
	pr_err("%s\n", __func__);

	if (platform_driver_register(&codec_mm_driver)) {
		pr_err("failed to register amports mem module\n");
		return -ENODEV;
	}

	return 0;
}

//MODULE_DESCRIPTION("AMLOGIC amports mem  driver");
//MODULE_LICENSE("GPL");

static int codec_mm_reserved_init(struct reserved_mem *rmem, struct device *dev)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	mgt->rmem = *rmem;
	pr_debug("%s %p->%p\n", __func__,
		 (void *)mgt->rmem.base,
		 (void *)mgt->rmem.base + mgt->rmem.size);
	return 0;
}

static const struct reserved_mem_ops codec_mm_rmem_vdec_ops = {
	.device_init = codec_mm_reserved_init,
};

static int __init codec_mm_res_setup(struct reserved_mem *rmem)
{
	rmem->ops = &codec_mm_rmem_vdec_ops;
	pr_debug("vdec: reserved mem setup\n");

	return 0;
}

RESERVEDMEM_OF_DECLARE(codec_mm_reserved, "amlogic, codec-mm-reserved",
			   codec_mm_res_setup);

static int secure_vdec_reserved_init(struct reserved_mem *rmem,
	struct device *dev)
{
	int ret = -1;

	if (!rmem || rmem->size <= 0) {
		pr_err("Invalid reserved secure vdec memory");
		return 0;
	}

	ret = tee_register_mem(TEE_MEM_TYPE_STREAM_INPUT,
				rmem->base,
				rmem->size);
	if (ret) {
		pr_err("protect vdec failed addr %llx %llx ret is %x\n",
			(u64)rmem->base, (u64)rmem->size, ret);
	}

	return ret;
}

static const struct reserved_mem_ops secure_vdec_rmem_ops = {
	.device_init = secure_vdec_reserved_init,
};

static int __init secure_vdec_res_setup(struct reserved_mem *rmem)
{
	rmem->ops = &secure_vdec_rmem_ops;

	return 0;
}

RESERVEDMEM_OF_DECLARE(secure_vdec_reserved, "amlogic, secure-vdec-reserved",
			secure_vdec_res_setup);

module_param(debug_mode, uint, 0664);
MODULE_PARM_DESC(debug_mode, "\n debug module\n");
module_param(debug_sc_mode, uint, 0664);
MODULE_PARM_DESC(debug_sc_mode, "\n debug scatter module\n");
module_param(debug_keep_mode, uint, 0664);
MODULE_PARM_DESC(debug_keep_mode, "\n debug keep module\n");
module_param(tvp_mode, uint, 0664);
MODULE_PARM_DESC(tvp_mode, "\n tvp module\n");
module_param(tvp_dynamic_increase_disable, uint, 0664);
MODULE_PARM_DESC(tvp_dynamic_increase_disable, "\n disable tvp_dynamic_increase\n");
module_param(tvp_dynamic_alloc_force_small_segment, uint, 0664);
MODULE_PARM_DESC(tvp_dynamic_alloc_force_small_segment,
	"\n enable tvp_dynamic_alloc_force_small_segment\n");
module_param(tvp_dynamic_alloc_force_small_segment_size, uint, 0664);
MODULE_PARM_DESC(tvp_dynamic_alloc_force_small_segment_size,
	"\n setting tvp_dynamic_alloc_force_small_segment_size\n");
module_param(tvp_pool_early_release_switch, uint, 0664);
MODULE_PARM_DESC(tvp_pool_early_release_switch,
	"\n forbiden tvp pool release when tvp not disable\n");
