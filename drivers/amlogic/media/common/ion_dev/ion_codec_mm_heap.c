// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/dmabuf_manage.h>
#include <linux/ion.h>

#include "dev_ion.h"

#define ION_CODEC_MM_ALLOCATE_FAIL -1

#define CODEC_MM_ION "ION"

static phys_addr_t ion_codec_mm_allocate(struct ion_heap *heap,
					 unsigned long size, unsigned long flags)
{
	struct ion_cma_heap *codec_heap =
		container_of(heap, struct ion_cma_heap, heap);
	unsigned long offset;
	unsigned long allocflags = CODEC_MM_FLAGS_DMA;

	if (codec_heap->alloced_size + size > codec_heap->max_can_alloc_size)
		pr_debug("%s failed out size %lu,alloced %lu\n",
			 __func__,
			 size,
			 codec_heap->alloced_size);

	if (flags & ION_FLAG_EXTEND_PROTECTED)
		allocflags |= CODEC_MM_FLAGS_TVP;

	offset = codec_mm_alloc_for_dma(CODEC_MM_ION,
					size / PAGE_SIZE,
					0,
					allocflags);

	if (!offset) {
		pr_err("%s failed out size %d\n", __func__, (int)size);
		return ION_CODEC_MM_ALLOCATE_FAIL;
	}
	mutex_lock(&codec_heap->mutex);
	codec_heap->alloced_size += size;
	mutex_unlock(&codec_heap->mutex);
	return offset;
}

static void ion_codec_mm_free(struct ion_heap *heap,
			      phys_addr_t addr,
			      unsigned long size)
{
	struct ion_cma_heap *codec_heap =
		container_of(heap, struct ion_cma_heap, heap);

	if (addr == ION_CODEC_MM_ALLOCATE_FAIL)
		return;
	mutex_lock(&codec_heap->mutex);
	if (!codec_mm_free_for_dma(CODEC_MM_ION, addr))
		codec_heap->alloced_size -= size;
	mutex_unlock(&codec_heap->mutex);
}

static phys_addr_t ion_secure_allocate(struct ion_heap *heap,
				unsigned long size, unsigned long flags)
{
	struct ion_cma_heap *codec_heap =
		container_of(heap, struct ion_cma_heap, heap);
	phys_addr_t paddr;

	paddr = secure_block_alloc(size, size, 0);
	if (!paddr) {
		pr_err("%s failed out size %d\n", __func__, (int)size);
		return ION_CODEC_MM_ALLOCATE_FAIL;
	}
	codec_heap->alloced_size += size;
	return paddr;
}

static void ion_secure_free(struct ion_heap *heap,
				phys_addr_t addr,
				unsigned long size)
{
	struct ion_cma_heap *codec_heap =
		container_of(heap, struct ion_cma_heap, heap);
	if (addr == ION_CODEC_MM_ALLOCATE_FAIL)
		return;
	if (secure_block_free(addr, size))
		pr_err("block free error, please fix it");
	codec_heap->alloced_size -= size;
}

static int ion_codec_mm_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size,
				      unsigned long flags)
{
	struct sg_table *table;
	phys_addr_t paddr;
	int ret;

	if (!(flags & ION_FLAG_EXTEND_MESON_HEAP))
		return -ENOMEM;
	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	if (flags & ION_FLAG_EXTEND_MESON_SECURE_VDEC_HEAP)
		paddr = ion_secure_allocate(heap, size, flags);
	else
		paddr = ion_codec_mm_allocate(heap, size, flags);
	if (paddr == ION_CODEC_MM_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->priv_virt = table;
	buffer->sg_table = table;
	if (!(flags & ION_FLAG_EXTEND_PROTECTED))
		ion_buffer_prep_noncached(buffer);
	return 0;

err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_codec_mm_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));
	struct device *ion_dev = meson_ion_get_dev();

	if ((buffer->flags & ION_FLAG_EXTEND_PROTECTED) !=
		ION_FLAG_EXTEND_PROTECTED)
		ion_buffer_zero(buffer);

	if (buffer->flags & ION_FLAG_EXTEND_MESON_SECURE_VDEC_HEAP) {
		ion_secure_free(heap, paddr, buffer->size);
	} else {
		if (!!(buffer->flags & ION_FLAG_CACHED))
			dma_sync_sg_for_device(ion_dev,
							table->sgl,
							table->nents,
							DMA_BIDIRECTIONAL);
		ion_codec_mm_free(heap, paddr, buffer->size);
	}
	sg_free_table(table);
	kfree(table);
}

struct ion_heap_ops codec_mm_heap_ops = {
	.allocate = ion_codec_mm_heap_allocate,
	.free = ion_codec_mm_heap_free,
};
