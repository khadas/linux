// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/dmabuf_manage.h>

#define DMA_BUF_CODEC_MM "DMA_BUF"

static struct dma_heap *secure_heap;

static int dma_buf_debug = 1;
module_param(dma_buf_debug, int, 0644);

#define dprintk(level, fmt, arg...) \
	do { \
		if (dma_buf_debug >= (level)) \
			pr_info("DMA-BUF:%s: " fmt,  __func__, ## arg);	\
	} while (0)

#define pr_dbg(fmt, args ...)		dprintk(6, fmt, ## args)
#define pr_error(fmt, args ...)		dprintk(1, fmt, ## args)
#define pr_inf(fmt, args ...)		dprintk(8, fmt, ## args)
#define pr_enter()			pr_inf("enter")

#define SECURE_DMA_BLOCK_PADDING_SIZE		(16 * 1024)
#define SECURE_DMA_BLOCK_MIN_SIZE			(4 * 1024)
#define SECURE_DMA_BLOCK_ALIGN_2N			12

struct secure_block_info {
	__u32 version;
	__u32 size;
	__u32 phyaddr;
	__u32 allocsize;
	__u32 state;
	__u32 id;
	__u32 bind;
	__u32 allocnum;
	__u32 freenum;
};

struct secure_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	//lock for buffer access
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;
	//struct deferred_freelist_item deferred_free;
	bool uncached;
	//used for mmap to get phyaddr
	struct secure_block_info *block;
	struct page *block_page;
	struct dma_buf *buf;
};

struct dma_heap_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
	bool mapped;
	bool uncached;
};

static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static int secure_heap_attach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *table;

	pr_enter();
	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(&buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;
	a->uncached = buffer->uncached;
	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void secure_heap_detach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	pr_enter();
	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}

static struct sg_table *secure_heap_map_dma_buf
					(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	int attr = 0;
	int ret;

	pr_enter();
	if (a->uncached)
		attr = DMA_ATTR_SKIP_CPU_SYNC;

	ret = dma_map_sgtable(attachment->dev, table, direction, attr);
	if (ret)
		return ERR_PTR(ret);

	a->mapped = true;
	return table;
}

static void secure_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
					struct sg_table *table,
					enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	int attr = 0;

	pr_enter();
	if (a->uncached)
		attr = DMA_ATTR_SKIP_CPU_SYNC;
	a->mapped = false;
	dma_unmap_sgtable(attachment->dev, table, direction, attr);
}

static int secure_heap_dma_buf_begin_cpu_access
						(struct dma_buf *dmabuf,
					enum dma_data_direction direction)
{
	pr_enter();
	return 0;
}

static int secure_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
						enum dma_data_direction direction)
{
	pr_enter();
	return 0;
}

static inline u32 secure_align_up2n(u32 size, u32 alg2n)
{
	return ((size + (1 << alg2n) - 1) & (~((1 << alg2n) - 1)));
}

static int secure_heap_mmap(struct dma_buf *dmabuf,
						struct vm_area_struct *vma)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	unsigned long len = 0;
	unsigned long paddr = 0;
	struct secure_block_info *block = NULL;
	int ret = -1;

	pr_enter();
	if (!buffer)
		goto out;
	mutex_lock(&buffer->lock);
	if (buffer->block &&
		buffer->block->version >= SECURE_HEAP_SUPPORT_DELAY_ALLOC_VERSION) {
		block = buffer->block;
		switch (block->state) {
		case 1:
			if (block->allocsize < SECURE_DMA_BLOCK_MIN_SIZE)
				len = SECURE_DMA_BLOCK_MIN_SIZE;
			else
				len = secure_align_up2n(block->allocsize,
					SECURE_DMA_BLOCK_ALIGN_2N);
			len += SECURE_DMA_BLOCK_PADDING_SIZE;
			if (block->size < len ||
				(block->size - len >= SECURE_DMA_BLOCK_MIN_SIZE)) {
				if (block->size && block->phyaddr) {
					if (secure_block_free(block->phyaddr, block->size))
						pr_err("Secure buffer free err please fix it");
					block->phyaddr = 0;
					block->size = 0;
				}
				paddr = secure_block_alloc(len, buffer->len, block->id);
				if (!paddr) {
					pr_err("No more secure memory can be alloc %ld", len);
					goto out_unlock;
				}
				sg_set_page(buffer->sg_table.sgl,
					pfn_to_page(PFN_DOWN(paddr)), len, 0);
				block->phyaddr = paddr;
				block->size = len;
				buffer->buf->size = len;
				if (block->id > 0)
					block->bind = 1;
			}
			block->allocsize = 0;
			block->state = 0;
			break;
		case 2:
			if (block->version >= SECURE_HEAP_SUPPORT_MULTI_POOL_VERSION) {
				paddr = block->phyaddr;
				if (block->size && block->phyaddr) {
					if (secure_block_free(block->phyaddr, block->size))
						pr_err("Secure buffer free err please fix it");
					block->phyaddr = 0;
					block->size = 0;
				}
				if (block->id != 0 && block->allocsize > 0) {
					block->freenum =
						dmabuf_manage_get_can_alloc_blocknum(block->id,
						buffer->len,
						block->allocsize + SECURE_DMA_BLOCK_ALIGN_2N,
						paddr);
					block->allocnum =
						dmabuf_manage_get_allocated_blocknum(block->id);
					block->allocsize = 0;
				} else {
					block->allocnum = 0;
					block->freenum = 0;
					block->allocsize = 0;
				}
			}
			block->state = 0;
			break;
		default:
			break;
		}
	}
	ret = remap_pfn_range(vma, vma->vm_start,
						page_to_pfn(buffer->block_page),
						PAGE_SIZE,
						vma->vm_page_prot);
out_unlock:
	mutex_unlock(&buffer->lock);
out:
	return ret;
}

static void *secure_heap_vmap(struct dma_buf *dmabuf)
{
	pr_enter();
	return NULL;
}

static void secure_heap_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	pr_enter();
}

static void secure_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct secure_heap_buffer *buffer = dmabuf->priv;
	struct secure_block_info *block = NULL;

	pr_enter();
	if (!buffer)
		return;
	block = buffer->block;
	if (block && block->phyaddr && block->size) {
		if (secure_block_free(block->phyaddr, block->size))
			pr_err("Secure buffer release error, please fix it");
	}
	sg_free_table(&buffer->sg_table);
	if (buffer->block_page)
		free_reserved_page(buffer->block_page);
	else if (buffer->block)
		free_pages((unsigned long)buffer->block, 0);
	kfree(buffer);
}

static const struct dma_buf_ops secure_heap_buf_ops = {
	.attach = secure_heap_attach,
	.detach = secure_heap_detach,
	.map_dma_buf = secure_heap_map_dma_buf,
	.unmap_dma_buf = secure_heap_unmap_dma_buf,
	.begin_cpu_access = secure_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = secure_heap_dma_buf_end_cpu_access,
	.mmap = secure_heap_mmap,
	.vmap = secure_heap_vmap,
	.vunmap = secure_heap_vunmap,
	.release = secure_heap_dma_buf_release,
};

static struct dma_buf *secure_heap_do_allocate(struct dma_heap *heap,
						unsigned long len,
						unsigned long fd_flags,
						unsigned long heap_flags,
						bool uncached)
{
	struct secure_heap_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	struct sg_table *table;
	unsigned long paddr = 0;
	int ret = -ENOMEM;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = heap;
	buffer->len = len;
	buffer->uncached = uncached;

	buffer->block = (struct secure_block_info *)
		get_zeroed_page(GFP_KERNEL);
	if (!buffer->block)
		goto free_buffer;
	buffer->block_page = pfn_to_page(PFN_DOWN(virt_to_phys(buffer->block)));
	if (buffer->block_page)
		mark_page_reserved(buffer->block_page);

	table = &buffer->sg_table;
	if (sg_alloc_table(table, 1, GFP_KERNEL))
		goto free_priv_buffer;

	buffer->block->version = dmabuf_manage_get_secure_heap_version();
	if (buffer->block->version == SECURE_HEAP_DEFAULT_VERSION) {
		paddr = secure_block_alloc(len, len, 0);
		if (!paddr)
			goto free_tables;
		buffer->block->size = len;
		buffer->block->phyaddr = paddr;
		sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), len, 0);
	}

	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = &secure_heap_buf_ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_tables;
	}
	buffer->buf = dmabuf;

	return dmabuf;

free_tables:
	sg_free_table(table);
free_priv_buffer:
	free_page((unsigned long)buffer->block);
free_buffer:
	kfree(buffer);
	return ERR_PTR(ret);
}

static struct dma_buf *secure_uncached_heap_allocate
						(struct dma_heap *heap,
						unsigned long len,
						unsigned long fd_flags,
						unsigned long heap_flags)
{
	pr_enter();
	return secure_heap_do_allocate(heap, len, fd_flags, heap_flags, true);
}

static struct dma_buf *secure_uncached_heap_not_initialized
						(struct dma_heap *heap,
						unsigned long len,
						unsigned long fd_flags,
						unsigned long heap_flags)
{
	pr_enter();
	return ERR_PTR(-EBUSY);
}

static struct dma_heap_ops secure_heap_ops = {
	.allocate = secure_uncached_heap_not_initialized,
};

int __init amlogic_system_secure_dma_buf_init(void)
{
	struct dma_heap_export_info exp_info;

	pr_enter();
	exp_info.name = "system-secure-uncached";
	exp_info.ops = &secure_heap_ops;
	exp_info.priv = NULL;

	secure_heap = dma_heap_add(&exp_info);
	if (IS_ERR(secure_heap))
		return PTR_ERR(secure_heap);

	dma_coerce_mask_and_coherent(dma_heap_get_dev(secure_heap),
		DMA_BIT_MASK(64));
	mb(); /* make sure we only set allocate after dma_mask is set */
	secure_heap_ops.allocate = secure_uncached_heap_allocate;

	return 0;
}

MODULE_LICENSE("GPL v2");
