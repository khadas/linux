/*
 * Contiguous Memory Allocator
 *
 * Copyright (c) 2010-2011 by Samsung Electronics.
 * Copyright IBM Corporation, 2013
 * Copyright LG Electronics Inc., 2014
 * Written by:
 *	Marek Szyprowski <m.szyprowski@samsung.com>
 *	Michal Nazarewicz <mina86@mina86.com>
 *	Aneesh Kumar K.V <aneesh.kumar@linux.vnet.ibm.com>
 *	Joonsoo Kim <iamjoonsoo.kim@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License or (at your optional) any later version of the license.
 */

#define pr_fmt(fmt) "cma: " fmt

#ifdef CONFIG_CMA_DEBUG
#ifndef DEBUG
#  define DEBUG
#endif
#endif

#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/log2.h>
#include <linux/cma.h>
#include <linux/highmem.h>
#include <linux/page-isolation.h>
#include <linux/list.h>

struct cma {
	unsigned long	base_pfn;
	unsigned long	count;
	unsigned long	*bitmap;
	unsigned int order_per_bit; /* Order of pages represented by one bit */
	struct mutex	lock;
};


/*
 * try to migrate pages with many map count to non cma areas
 */
#define CMA_MR_WORK_THRESHOLD		32

struct cma_migrate_list {
	struct list_head list;
	unsigned long pfn;
};

static atomic_t nr_cma_mr_list;
static atomic_long_t nr_cma_allocated;
static __read_mostly bool use_cma_first = 1;
static LIST_HEAD(mr_list);
static DEFINE_SPINLOCK(mr_lock);
struct work_struct cma_migrate_work;

static struct cma cma_areas[MAX_CMA_AREAS];
static unsigned cma_area_count;
static DEFINE_MUTEX(cma_mutex);

static atomic_t cma_allocate;
__read_mostly unsigned long total_cma_pages;
static int __init setup_cma_first(char *buf)
{
	if (!strcmp(buf, "0"))
		use_cma_first = 0;
	else
		use_cma_first = 1;

	pr_info("%s, use_cma_first:%d\n", __func__, use_cma_first);
	return 0;
}
__setup("use_cma_first=", setup_cma_first);

int cma_alloc_ref(void)
{
	return atomic_read(&cma_allocate);
}
EXPORT_SYMBOL(cma_alloc_ref);

void get_cma_alloc_ref(void)
{
	atomic_inc(&cma_allocate);
}
EXPORT_SYMBOL(get_cma_alloc_ref);

void put_cma_alloc_ref(void)
{
	atomic_dec(&cma_allocate);
}
EXPORT_SYMBOL(put_cma_alloc_ref);

unsigned long get_cma_allocated(void)
{
	return atomic_long_read(&nr_cma_allocated);
}
EXPORT_SYMBOL(get_cma_allocated);

unsigned long get_total_cmapages(void)
{
	return total_cma_pages;
}
EXPORT_SYMBOL(get_total_cmapages);

bool can_use_cma(gfp_t gfp_flags)
{
	unsigned long free_cma;
	unsigned long free_pages;

	/*
	 * __GFP_BDEV flags will cause long allocate time
	 * of cma allocation, for these flags, we can't use cma pages.
	 * it has first priority
	 */
	if (gfp_flags & (__GFP_BDEV | __GFP_WRITE))
		return false;

	/*
	 * Make sure CMA pages can be used if memory is not enough
	 */
	free_cma   = global_page_state(NR_FREE_CMA_PAGES);
	free_pages = global_page_state(NR_FREE_PAGES);
	if (free_pages - free_cma < mem_management_thresh)
		return true;

	/*
	 * boot args can asign 'use_cma_first' to 0 for strict cma check.
	 * This args can be used for high speed cma allocation
	 */
	if (use_cma_first)
		return true;

	/*
	 * __GFP_COLD is a significant flag to identify pages are allocated
	 * for anon mapping or file mapping. Allocation cost of anon mapping
	 * is larger than file mapping, because we need allocate a new page
	 * and copy page data to new page. For file mapping, we just need
	 * unmap page and free it.
	 * So if system free memory is enough, we do not use cma pages for
	 * anon mapping, but if free memory is below threshold, we should
	 * release this limit.
	 */
	if (!(gfp_flags & __GFP_COLD))
		return false;

	/*
	 * do not use cma pages when cma allocate is working. this is the
	 * weakest condition
	 */
	if (cma_alloc_ref())
		return false;
	return true;
}
EXPORT_SYMBOL(can_use_cma);

bool cma_page(struct page *page)
{
	int migrate_type = 0;

	if (!page)
		return false;
	migrate_type = get_pageblock_migratetype(page);
	if (is_migrate_cma(migrate_type) ||
	   is_migrate_isolate(migrate_type)) {
		return true;
	}
	return false;
}
EXPORT_SYMBOL(cma_page);

int mark_cma_migrate_page(struct page *page)
{
	struct cma_migrate_list *cma_list, *next;
	unsigned long pfn;

	/* we don't need to move pages with high mapping
	 * if speed is not concerned
	 */
	if (use_cma_first)
		return 0;

	/* check if this page has marked */
	pfn = page_to_pfn(page);
	spin_lock(&mr_lock);
	list_for_each_entry_safe(cma_list, next, &mr_list, list) {
		/* alread marked */
		if (cma_list->pfn == pfn) {
			spin_unlock(&mr_lock);
			goto work;
		}
	}
	spin_unlock(&mr_lock);

	/* add to list */
	cma_list = kzalloc(sizeof(*cma_list), GFP_ATOMIC);
	if (!cma_list) {
		pr_err("%s, no memory\n", __func__);
		return -ENOMEM;
	}
	cma_list->pfn = pfn;
	spin_lock(&mr_lock);
	list_add_tail(&cma_list->list, &mr_list);
	atomic_inc(&nr_cma_mr_list);
	spin_unlock(&mr_lock);
work:
	if (atomic_read(&nr_cma_mr_list) > CMA_MR_WORK_THRESHOLD)
		schedule_work(&cma_migrate_work);
	return 0;
}
EXPORT_SYMBOL(mark_cma_migrate_page);

static void cma_mr_work_func(struct work_struct *work)
{
	unsigned int nr_scan = 0, nr_migrate = 0, map_cnt;
	struct cma_migrate_list *cma_list, *next;
	struct page *page;
	unsigned long total_mapcnt = 0;

	list_for_each_entry_safe(cma_list, next, &mr_list, list) {
		nr_scan++;
		page = pfn_to_page(cma_list->pfn);
		map_cnt = page_mapcount(page);
		/*
		 * map cnt may reduce during work, if so
		 * just remove it from list
		 */
		if (map_cnt < CMA_MIGRATE_MAP_THRESHOLD ||
		    !migrate_cma_page(cma_list->pfn)) {
			total_mapcnt += map_cnt;
			nr_migrate++;
			atomic_dec(&nr_cma_mr_list);
			spin_lock(&mr_lock);
			list_del(&cma_list->list);
			spin_unlock(&mr_lock);
			kfree(cma_list);
		}
	}
	pr_debug("%s, nr_scaned:%d, nr_migrate:%d, total_map:%ld, list:%d\n",
		__func__, nr_scan, nr_migrate, total_mapcnt,
		atomic_read(&nr_cma_mr_list));
}

phys_addr_t cma_get_base(struct cma *cma)
{
	return PFN_PHYS(cma->base_pfn);
}

unsigned long cma_get_size(struct cma *cma)
{
	return cma->count << PAGE_SHIFT;
}

static unsigned long cma_bitmap_aligned_mask(struct cma *cma, int align_order)
{
	if (align_order <= cma->order_per_bit)
		return 0;
	return (1UL << (align_order - cma->order_per_bit)) - 1;
}

static unsigned long cma_bitmap_maxno(struct cma *cma)
{
	return cma->count >> cma->order_per_bit;
}

static unsigned long cma_bitmap_pages_to_bits(struct cma *cma,
						unsigned long pages)
{
	return ALIGN(pages, 1UL << cma->order_per_bit) >> cma->order_per_bit;
}

static void cma_clear_bitmap(struct cma *cma, unsigned long pfn, int count)
{
	unsigned long bitmap_no, bitmap_count;

	bitmap_no = (pfn - cma->base_pfn) >> cma->order_per_bit;
	bitmap_count = cma_bitmap_pages_to_bits(cma, count);

	mutex_lock(&cma->lock);
	bitmap_clear(cma->bitmap, bitmap_no, bitmap_count);
	mutex_unlock(&cma->lock);
}

static int __init cma_activate_area(struct cma *cma)
{
	int bitmap_size = BITS_TO_LONGS(cma_bitmap_maxno(cma)) * sizeof(long);
	unsigned long base_pfn = cma->base_pfn, pfn = base_pfn;
	unsigned i = cma->count >> pageblock_order;
	struct zone *zone;

	cma->bitmap = kzalloc(bitmap_size, GFP_KERNEL);

	if (!cma->bitmap)
		return -ENOMEM;

	WARN_ON_ONCE(!pfn_valid(pfn));
	zone = page_zone(pfn_to_page(pfn));

	do {
		unsigned j;

		base_pfn = pfn;
		for (j = pageblock_nr_pages; j; --j, pfn++) {
			WARN_ON_ONCE(!pfn_valid(pfn));
			/*
			 * alloc_contig_range requires the pfn range
			 * specified to be in the same zone. Make this
			 * simple by forcing the entire CMA resv range
			 * to be in the same zone.
			 */
			if (page_zone(pfn_to_page(pfn)) != zone)
				goto err;
		}
		init_cma_reserved_pageblock(pfn_to_page(base_pfn));
	} while (--i);

	mutex_init(&cma->lock);
	return 0;

err:
	kfree(cma->bitmap);
	return -EINVAL;
}

static int __init cma_init_reserved_areas(void)
{
	int i;

	for (i = 0; i < cma_area_count; i++) {
		int ret = cma_activate_area(&cma_areas[i]);

		if (ret)
			return ret;
	}
	atomic_set(&cma_allocate, 0);
	atomic_set(&nr_cma_mr_list, 0);
	atomic_long_set(&nr_cma_allocated, 0);
	INIT_LIST_HEAD(&mr_list);
	INIT_WORK(&cma_migrate_work, cma_mr_work_func);
	pr_info("%s, use_cma_first:%d\n", __func__, use_cma_first);

	return 0;
}
core_initcall(cma_init_reserved_areas);

/**
 * cma_init_reserved_mem() - create custom contiguous area from reserved memory
 * @base: Base address of the reserved area
 * @size: Size of the reserved area (in bytes),
 * @order_per_bit: Order of pages represented by one bit on bitmap.
 * @res_cma: Pointer to store the created cma region.
 *
 * This function creates custom contiguous area from already reserved memory.
 */
int __init cma_init_reserved_mem(phys_addr_t base, phys_addr_t size,
				 int order_per_bit, struct cma **res_cma)
{
	struct cma *cma;
	phys_addr_t alignment;

	/* Sanity checks */
	if (cma_area_count == ARRAY_SIZE(cma_areas)) {
		pr_err("Not enough slots for CMA reserved regions!\n");
		return -ENOSPC;
	}

	if (!size || !memblock_is_region_reserved(base, size))
		return -EINVAL;

	/* ensure minimal alignment requied by mm core */
	alignment = PAGE_SIZE << max(MAX_ORDER - 1, pageblock_order);

	/* alignment should be aligned with order_per_bit */
	if (!IS_ALIGNED(alignment >> PAGE_SHIFT, 1 << order_per_bit))
		return -EINVAL;

	if (ALIGN(base, alignment) != base || ALIGN(size, alignment) != size)
		return -EINVAL;

	/*
	 * Each reserved area must be initialised later, when more kernel
	 * subsystems (like slab allocator) are available.
	 */
	cma = &cma_areas[cma_area_count];
	cma->base_pfn = PFN_DOWN(base);
	cma->count = size >> PAGE_SHIFT;
	cma->order_per_bit = order_per_bit;
	*res_cma = cma;
	cma_area_count++;
	total_cma_pages += size / PAGE_SIZE;
	pr_info("Reserved %ld MiB at %08lx, total cma pages:%ld\n",
		(unsigned long)size / SZ_1M, (unsigned long)base,
		total_cma_pages);

	return 0;
}

/**
 * cma_declare_contiguous() - reserve custom contiguous area
 * @base: Base address of the reserved area optional, use 0 for any
 * @size: Size of the reserved area (in bytes),
 * @limit: End address of the reserved memory (optional, 0 for any).
 * @alignment: Alignment for the CMA area, should be power of 2 or zero
 * @order_per_bit: Order of pages represented by one bit on bitmap.
 * @fixed: hint about where to place the reserved area
 * @res_cma: Pointer to store the created cma region.
 *
 * This function reserves memory from early allocator. It should be
 * called by arch specific code once the early allocator (memblock or bootmem)
 * has been activated and all other subsystems have already allocated/reserved
 * memory. This function allows to create custom reserved areas.
 *
 * If @fixed is true, reserve contiguous area at exactly @base.  If false,
 * reserve in range from @base to @limit.
 */
int __init cma_declare_contiguous(phys_addr_t base,
			phys_addr_t size, phys_addr_t limit,
			phys_addr_t alignment, unsigned int order_per_bit,
			bool fixed, struct cma **res_cma)
{
	phys_addr_t memblock_end = memblock_end_of_DRAM();
	phys_addr_t highmem_start = __pa(high_memory);
	int ret = 0;

	pr_debug("%s(size %lx, base %08lx, limit %08lx alignment %08lx)\n",
		__func__, (unsigned long)size, (unsigned long)base,
		(unsigned long)limit, (unsigned long)alignment);

	if (cma_area_count == ARRAY_SIZE(cma_areas)) {
		pr_err("Not enough slots for CMA reserved regions!\n");
		return -ENOSPC;
	}

	if (!size)
		return -EINVAL;

	if (alignment && !is_power_of_2(alignment))
		return -EINVAL;

	/*
	 * Sanitise input arguments.
	 * Pages both ends in CMA area could be merged into adjacent unmovable
	 * migratetype page by page allocator's buddy algorithm. In the case,
	 * you couldn't get a contiguous memory, which is not what we want.
	 */
	alignment = max(alignment,
		(phys_addr_t)PAGE_SIZE << max(MAX_ORDER - 1, pageblock_order));
	base = ALIGN(base, alignment);
	size = ALIGN(size, alignment);
	limit &= ~(alignment - 1);

	/* size should be aligned with order_per_bit */
	if (!IS_ALIGNED(size >> PAGE_SHIFT, 1 << order_per_bit))
		return -EINVAL;

	/*
	 * adjust limit to avoid crossing low/high memory boundary for
	 * automatically allocated regions
	 */
	if (((limit == 0 || limit > memblock_end) &&
	     (memblock_end - size < highmem_start &&
	      memblock_end > highmem_start)) ||
	    (!fixed && limit > highmem_start && limit - size < highmem_start)) {
		limit = highmem_start;
	}

	if (fixed && base < highmem_start && base+size > highmem_start) {
		ret = -EINVAL;
		pr_err("Region at %08lx defined on low/high memory boundary (%08lx)\n",
			(unsigned long)base, (unsigned long)highmem_start);
		goto err;
	}

	/* Reserve memory */
	if (base && fixed) {
		if (memblock_is_region_reserved(base, size) ||
		    memblock_reserve(base, size) < 0) {
			ret = -EBUSY;
			goto err;
		}
	} else {
		phys_addr_t addr = memblock_alloc_range(size, alignment, base,
							limit);
		if (!addr) {
			ret = -ENOMEM;
			goto err;
		} else {
			base = addr;
		}
	}

	ret = cma_init_reserved_mem(base, size, order_per_bit, res_cma);
	if (ret)
		goto err;

	return 0;

err:
	pr_err("Failed to reserve %ld MiB\n", (unsigned long)size / SZ_1M);
	return ret;
}

void update_cma_ip(struct page *page, int count, unsigned long ip)
{
	int i;

	if (!page)
		return;
	for (i = 0; i < count; i++) {
		relpace_page_ip(page, ip);
		page->alloc_mask = __GFP_BDEV;
		page++;
	}
}

/**
 * cma_alloc() - allocate pages from contiguous area
 * @cma:   Contiguous memory region for which the allocation is performed.
 * @count: Requested number of pages.
 * @align: Requested alignment of pages (in PAGE_SIZE order).
 *
 * This function allocates part of contiguous memory on specific
 * contiguous memory area.
 */
struct page *cma_alloc(struct cma *cma, int count, unsigned int align)
{
	unsigned long mask, pfn, start = 0;
	unsigned long bitmap_maxno, bitmap_no, bitmap_count;
	struct page *page = NULL;
	int ret;

	if (!cma || !cma->count)
		return NULL;

	pr_debug("%s(cma %p, count %d, align %d)\n", __func__, (void *)cma,
		 count, align);

	if (!count)
		return NULL;

	get_cma_alloc_ref();
	mask = cma_bitmap_aligned_mask(cma, align);
	bitmap_maxno = cma_bitmap_maxno(cma);
	bitmap_count = cma_bitmap_pages_to_bits(cma, count);

	for (;;) {
		mutex_lock(&cma->lock);
		bitmap_no = bitmap_find_next_zero_area(cma->bitmap,
				bitmap_maxno, start, bitmap_count, mask);
		if (bitmap_no >= bitmap_maxno) {
			mutex_unlock(&cma->lock);
			break;
		}
		bitmap_set(cma->bitmap, bitmap_no, bitmap_count);
		/*
		 * It's safe to drop the lock here. We've marked this region for
		 * our exclusive use. If the migration fails we will take the
		 * lock again and unmark it.
		 */
		mutex_unlock(&cma->lock);

		pfn = cma->base_pfn + (bitmap_no << cma->order_per_bit);
		mutex_lock(&cma_mutex);
		ret = alloc_contig_range(pfn, pfn + count, MIGRATE_CMA);
		mutex_unlock(&cma_mutex);
		if (ret == 0) {
			page = pfn_to_page(pfn);
			break;
		} else if (ret != -EBUSY) {
			cma_clear_bitmap(cma, pfn, count);
			break;
		}
		cma_clear_bitmap(cma, pfn, count);
		pr_debug("%s(): memory range at %p is busy, retrying\n",
			 __func__, pfn_to_page(pfn));
		/* try again with a bit different memory target */
		start = bitmap_no + mask + 1;
	}
	put_cma_alloc_ref();
	atomic_long_add(count, &nr_cma_allocated);
	update_cma_ip(page, count, _RET_IP_);
	pr_debug("%s(): returned %p\n", __func__, page);
	return page;
}

/**
 * cma_release() - release allocated pages
 * @cma:   Contiguous memory region for which the allocation is performed.
 * @pages: Allocated pages.
 * @count: Number of allocated pages.
 *
 * This function releases memory allocated by alloc_cma().
 * It returns false when provided pages do not belong to contiguous area and
 * true otherwise.
 */
bool cma_release(struct cma *cma, struct page *pages, int count)
{
	unsigned long pfn;

	if (!cma || !pages)
		return false;

	pr_debug("%s(page %p)\n", __func__, (void *)pages);

	pfn = page_to_pfn(pages);

	if (pfn < cma->base_pfn || pfn >= cma->base_pfn + cma->count)
		return false;

	VM_BUG_ON(pfn + count > cma->base_pfn + cma->count);

	free_contig_range(pfn, count);
	cma_clear_bitmap(cma, pfn, count);
	atomic_long_sub(count, &nr_cma_allocated);

	return true;
}
