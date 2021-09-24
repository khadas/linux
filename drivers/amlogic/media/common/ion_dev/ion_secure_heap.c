// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/ion.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/cma.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>
#include <linux/genalloc.h>
#include "dev_ion.h"

#define ION_SECURE_ALLOCATE_FAIL	-1

struct ion_secure_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	phys_addr_t base;
};

static phys_addr_t ion_secure_allocate(struct ion_heap *heap,
					     unsigned long size)
{
	struct ion_secure_heap *secure_heap =
		container_of(heap, struct ion_secure_heap, heap);
	unsigned long offset = gen_pool_alloc(secure_heap->pool, size);

	if (!offset)
		return ION_SECURE_ALLOCATE_FAIL;

	return offset;
}

static void ion_secure_free(struct ion_heap *heap, phys_addr_t addr,
			    unsigned long size)
{
	struct ion_secure_heap *secure_heap =
		container_of(heap, struct ion_secure_heap, heap);

	if (addr == ION_SECURE_ALLOCATE_FAIL)
		return;
	gen_pool_free(secure_heap->pool, addr, size);
}

static int ion_secure_heap_allocate(struct ion_heap *heap,
					    struct ion_buffer *buffer,
					    unsigned long len,
					    unsigned long flags)
{
	struct sg_table *table;
	phys_addr_t paddr;
	int ret;

	if (!(flags & ION_FLAG_EXTEND_MESON_HEAP) ||
	    !(flags & ION_FLAG_EXTEND_MESON_HEAP_SECURE))
		return -ENOMEM;
	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	paddr = ion_secure_allocate(heap, len);
	if (paddr == ION_SECURE_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), len, 0);
	buffer->sg_table = table;

	return 0;

err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_secure_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	ion_secure_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops secure_heap_ops = {
	.allocate = ion_secure_heap_allocate,
	.free = ion_secure_heap_free,
};

struct ion_heap *ion_secure_heap_create(phys_addr_t base,
						 unsigned long size)
{
	struct ion_secure_heap *secure_heap;

	secure_heap = kzalloc(sizeof(*secure_heap), GFP_KERNEL);
	if (!secure_heap)
		return ERR_PTR(-ENOMEM);

	secure_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!secure_heap->pool) {
		kfree(secure_heap);
		return ERR_PTR(-ENOMEM);
	}
	secure_heap->base = base;
	gen_pool_add(secure_heap->pool, secure_heap->base, size, -1);
	secure_heap->heap.ops = &secure_heap_ops;
	secure_heap->heap.name = "secure_ion";
	secure_heap->heap.type = ION_HEAP_TYPE_CUSTOM;
	secure_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE |
		ION_FLAG_EXTEND_MESON_HEAP |
		ION_FLAG_EXTEND_MESON_HEAP_SECURE;

	return &secure_heap->heap;
}

void ion_secure_heap_destroy(struct ion_heap *heap)
{
	struct ion_secure_heap *secure_heap =
		container_of(heap, struct  ion_secure_heap, heap);

	gen_pool_destroy(secure_heap->pool);
	kfree(secure_heap);
	secure_heap = NULL;
}
