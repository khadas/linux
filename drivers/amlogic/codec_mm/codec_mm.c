/*
 * drivers/amlogic/codec_mm/codec_mm.c
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

#include <linux/amlogic/codec_mm/codec_mm.h>

#include "codec_mm_priv.h"




#define CODEC_MM_FOR_DMA_ONLY(flags) \
	((flags & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_DMA)

#define CODEC_MM_FOR_CPU_ONLY(flags) \
	((flags & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_CPU)

#define CODEC_MM_FOR_DMACPU(flags) \
	((flags & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_DMA_CPU)

#define RESERVE_MM_ALIGNED_2N	17

struct codec_mm_mgt_s {
	struct cma *cma;
	struct device *dev;
	struct list_head mem_list;
	struct gen_pool *res_pool;
	struct gen_pool *tvp_pool;
	struct reserved_mem rmem;
	struct reserved_mem tvp_rmem;
	int alloc_from_sys_pages_max;
	int enable_kmalloc_on_nomem;
	spinlock_t lock;
};

#define PHY_OFF() offsetof(struct codec_mm_s, phy_addr)
#define HANDLE_OFF() offsetof(struct codec_mm_s, mem_handle)
#define VADDR_OFF() offsetof(struct codec_mm_s, vbuffer)
#define VAL_OFF_VAL(mem, off) (*(unsigned long *)((unsigned long)(mem) + off))

static struct codec_mm_mgt_s *get_mem_mgt(void)
{
	static struct codec_mm_mgt_s mgt;
	static int inited;

	if (!inited) {
		memset(&mgt, 0, sizeof(struct codec_mm_mgt_s));
		inited++;
	}
	return &mgt;
};

static int codec_mm_alloc_in(
	struct codec_mm_mgt_s *mgt, struct codec_mm_s *mem)
{
	int try_alloced_from_sys = 0;
	int align_2n = mem->align2n < PAGE_SHIFT ? PAGE_SHIFT : mem->align2n;

	do {
		if ((mem->flags & CODEC_MM_FLAGS_DMA_CPU) &&
			mem->page_count <= mgt->alloc_from_sys_pages_max &&
			align_2n <= PAGE_SHIFT) {
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

		if ((CODEC_MM_FOR_DMA_ONLY(mem->flags) |
			(mem->flags & CODEC_MM_FLAGS_RESERVED))
			&& mgt->res_pool != NULL &&
			(align_2n <= RESERVE_MM_ALIGNED_2N)) {
			mem->mem_handle = (void *)gen_pool_alloc(mgt->res_pool,
							mem->buffer_size);
			mem->from_flags =
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED;
			if (mem->mem_handle) {
				/*default is no maped */
				mem->vbuffer = NULL;
				mem->phy_addr = (unsigned long)mem->mem_handle;
				break;
			}
		}
		if (mem->flags &
			(CODEC_MM_FLAGS_CMA | CODEC_MM_FLAGS_DMA_CPU)) {
			mem->mem_handle = dma_alloc_from_contiguous(mgt->dev,
					mem->page_count,
					align_2n - PAGE_SHIFT);
			mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA;
			if (mem->mem_handle) {
				mem->vbuffer = mem->mem_handle;
				mem->phy_addr =
				 page_to_phys((struct page *)mem->mem_handle);
#ifdef CONFIG_ARM64
				if (mem->flags & CODEC_MM_FLAGS_CMA_CLEAR) {
					dma_clear_buffer(
						(struct page *)mem->vbuffer,
						mem->buffer_size);
				}
#endif
				break;
			}
		}

		if ((mem->flags & CODEC_MM_FLAGS_TVP) &&
			mgt->tvp_pool != NULL &&
				align_2n < 16) {
			/* 64k,aligend */
			mem->mem_handle = (void *)gen_pool_alloc(mgt->tvp_pool,
					mem->buffer_size);
			mem->from_flags =
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED;
			if (mem->mem_handle) {
				/*no vaddr for TVP MEMORY */
				mem->vbuffer = NULL;
				mem->phy_addr = (unsigned long)mem->mem_handle;
				break;
			}
		}

		if ((mem->flags & CODEC_MM_FLAGS_DMA_CPU) &&
			mgt->enable_kmalloc_on_nomem &&
			!try_alloced_from_sys) {
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
	} while (0);
	if (mem->mem_handle)
		return 0;
	else
		return -ENOMEM;
}

static void codec_mm_free_in(struct codec_mm_mgt_s *mgt,
		struct codec_mm_s *mem)
{

	if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA) {
		dma_release_from_contiguous(mgt->dev,
			mem->mem_handle, mem->page_count);
	} else if (mem->from_flags ==
		AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED) {
		gen_pool_free(mgt->res_pool,
			(unsigned long)mem->mem_handle, mem->buffer_size);
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP) {
		gen_pool_free(mgt->tvp_pool,
			(unsigned long)mem->mem_handle, mem->buffer_size);
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES) {
		__free_pages((struct page *)mem->mem_handle,
			get_order(mem->buffer_size));
	}

	return;
}

static struct codec_mm_s *codec_mm_alloc(const char *owner, int size,
		int align2n, int memflags)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = kmalloc(sizeof(struct codec_mm_s),
					GFP_KERNEL);
	int count;
	int ret;
	unsigned long flags;

	if (!mem) {
		pr_err("not enough mem for struct codec_mm_s\n");
		return NULL;
	}

	if ((memflags & CODEC_MM_FLAGS_FROM_MASK) == 0)
		memflags |= CODEC_MM_FLAGS_DMA;

	memset(mem, 0, sizeof(struct codec_mm_s));
	count = PAGE_ALIGN(size) / PAGE_SIZE;
	mem->buffer_size = size;
	mem->page_count = count;
	mem->align2n = align2n;
	mem->flags = memflags;
	ret = codec_mm_alloc_in(mgt, mem);
	if (ret < 0) {
		pr_err("not enough mem for %s size %d\n", owner, size);
		kfree(mem);
		return NULL;
	}

	atomic_set(&mem->use_cnt, 1);
	mem->owner[0] = owner;
	spin_lock_init(&mem->lock);
	spin_lock_irqsave(&mgt->lock, flags);
	list_add_tail(&mem->list, &mgt->mem_list);
	spin_unlock_irqrestore(&mgt->lock, flags);
	/*
	pr_err("%s alloc mem size %d at %lx from %d\n",
			owner, size, mem->phy_addr,
			mem->from_flags);
	*/
	return mem;
}

static void codec_mm_release(struct codec_mm_s *mem, const char *owner)
{

	int index;
	unsigned long flags;
	int i;
	const char *max_owner;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	if (!mem)
		return;

	spin_lock_irqsave(&mem->lock, flags);
	index = atomic_dec_return(&mem->use_cnt);
	max_owner = mem->owner[index];
	for (i = 0; i < index; i++) {
		if (mem->owner[i] && strcmp(owner, mem->owner[i]) == 0)
			mem->owner[i] = max_owner;
	}
	mem->owner[index] = NULL;
	if (index == 0) {
		spin_unlock_irqrestore(&mem->lock, flags);
		codec_mm_free_in(mgt, mem);
		list_del(&mem->list);
		kfree(mem);
		return;
	}
	spin_unlock_irqrestore(&mem->lock, flags);
	return;
}

void codec_mm_dma_flush(void *cpu_addr, int size, enum dma_data_direction dir)
{

	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	dma_addr_t dma_addr;
	dma_addr = dma_map_single(mgt->dev, cpu_addr, size, dir);
	dma_unmap_single(mgt->dev, dma_addr, size, dir);
	return;
}

int codec_mm_request_shared_mem(struct codec_mm_s *mem, const char *owner)
{
	if (!mem || atomic_read(&mem->use_cnt) > 7)
		return -1;
	mem->owner[atomic_inc_return(&mem->use_cnt) - 1] = owner;
	return 0;
}

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


void *codec_mm_phys_to_virt(unsigned long phy_addr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (phy_addr >= mgt->rmem.base &&
			phy_addr < mgt->rmem.base + mgt->rmem.size)
		return NULL;	/* no virt for reserved memory; */
	if (phy_addr >= mgt->tvp_rmem.base &&
		phy_addr < mgt->tvp_rmem.base + mgt->tvp_rmem.size)
		return NULL;	/* no virt for tvp memory; */
	return phys_to_virt(phy_addr);
}

unsigned long codec_mm_virt_to_phys(void *vaddr)
{
	return page_to_phys((struct page *)vaddr);
}

int dump_mem_infos(void *buf, int size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem;
	unsigned long flags;

	char *pbuf = buf;
	char sbuf[512];
	int tsize = 0;
	int s;

	if (!pbuf)
		pbuf = sbuf;

	s = sprintf(pbuf, "[%d]RES pool size:%d MB\n",
			AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED,
			(int)mgt->rmem.size / SZ_1M);
	tsize += s;
	pbuf += s;

	s = sprintf(pbuf, "[%d]CMA pool size:%d MB\n",
			AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA,
			(int)(dma_get_cma_size_int_byte(mgt->dev) / SZ_1M));
	tsize += s;
	pbuf += s;

	spin_lock_irqsave(&mgt->lock, flags);
	if (list_empty(&mgt->mem_list)) {
		spin_unlock_irqrestore(&mgt->lock, flags);
		return tsize;
	}
	list_for_each_entry(mem, &mgt->mem_list, list) {
		s = sprintf(pbuf,
		"\towner: %s:%s,addr=%p,s=%d,f=%d,c=%d\n",
		mem->owner[0] ? mem->owner[0] : "no",
		mem->owner[1] ? mem->owner[1] : "no",
		(void *)mem->phy_addr,
		mem->buffer_size, mem->from_flags,
		atomic_read(&mem->use_cnt));

		if (buf)
			pbuf += s;
		else
			pr_info("%s", sbuf);
		tsize += s;
	}
	spin_unlock_irqrestore(&mgt->lock, flags);

	return tsize;
}

int codec_mm_mgt_init(struct device *dev)
{

	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	INIT_LIST_HEAD(&mgt->mem_list);
	mgt->dev = dev;
	mgt->alloc_from_sys_pages_max = 4;
	if (mgt->rmem.size > 0) {
		unsigned long aligned_addr;
		int aligned_size;
		/*mem aligned,*/
		mgt->res_pool = gen_pool_create(RESERVE_MM_ALIGNED_2N, -1);
		if (!mgt->res_pool)
			return -ENOMEM;
		aligned_addr = ((unsigned long)mgt->rmem.base +
			((1 << RESERVE_MM_ALIGNED_2N) - 1)) &
				(~((1 << RESERVE_MM_ALIGNED_2N) - 1));
		aligned_size = mgt->rmem.size -
			(int)(aligned_addr - (unsigned long)mgt->rmem.base);
		gen_pool_add(mgt->res_pool,
			aligned_addr, aligned_size, -1);
		pr_info("add reserve memory %p(aligned %p) size=%x(aligned %x)\n",
			(void *)mgt->rmem.base, (void *)aligned_addr,
			(int)mgt->rmem.size, (int)aligned_size);
	}
	spin_lock_init(&mgt->lock);
	return 0;
}

static int __init amstream_test_init(void)
{
#if 0
	unsigned long buf[4];
	struct codec_mm_s *mem[20];
	mem[0] = codec_mm_alloc("test0", 1024 * 1024, 0, 0);
	mem[1] = codec_mm_alloc("test1", 1024 * 1024, 0, 0);
	mem[2] = codec_mm_alloc("test2", 1024 * 1024, 0, 0);
	mem[3] = codec_mm_alloc("test32", 1024 * 1024, 0, 0);
	mem[4] = codec_mm_alloc("test3", 400 * 1024, 0, 0);
	mem[5] = codec_mm_alloc("test3", 400 * 1024, 0, 0);
	mem[6] = codec_mm_alloc("test666", 400 * 1024, 0, 0);
	mem[7] = codec_mm_alloc("test3", 8 * 1024 * 1024, 0, 0);
	mem[8] = codec_mm_alloc("test4", 3 * 1024 * 1024, 0, 0);
	pr_info("TT:%p,%p,%p,%p\n", mem[0], mem[1], mem[2], mem[3]);
	dump_mem_infos(NULL, 0);
	codec_mm_release(mem[1], "test1");
	codec_mm_release(mem[7], "test3");
	codec_mm_request_shared_mem(mem[3], "test55");
	codec_mm_request_shared_mem(mem[6], "test667");
	dump_mem_infos(NULL, 0);
	codec_mm_release(mem[3], "test32");
	codec_mm_release(mem[0], "test1");
	codec_mm_request_shared_mem(mem[3], "test57");
	codec_mm_request_shared_mem(mem[3], "test58");
	codec_mm_release(mem[3], "test55");
	dump_mem_infos(NULL, 0);
	mem[8] = codec_mm_alloc("test4", 8 * 1024 * 1024, 0,
		CODEC_MM_FLAGS_DMA);
	mem[9] = codec_mm_alloc("test5", 4 * 1024 * 1024, 0,
		CODEC_MM_FLAGS_TVP);
	mem[10] = codec_mm_alloc("test6", 10 * 1024 * 1024, 0,
		CODEC_MM_FLAGS_DMA_CPU);
	dump_mem_infos(NULL, 0);



	buf[0] = codec_mm_alloc_for_dma("streambuf1",
		8 * 1024 * 1024 / PAGE_SIZE, 0, 0);
	buf[1] = codec_mm_alloc_for_dma("streambuf2",
		2 * 1024 * 1024 / PAGE_SIZE, 0, 0);
	buf[2] = codec_mm_alloc_for_dma("streambuf2",
		118 * 1024 / PAGE_SIZE, 0, 0);
	buf[3] = codec_mm_alloc_for_dma("streambuf2",
		(jiffies & 0x7f) * 1024 / PAGE_SIZE, 0, 0);
	dump_mem_infos(NULL, 0);
	codec_mm_free_for_dma("streambuf2", buf[3]);
	codec_mm_free_for_dma("streambuf2", buf[1]);
	codec_mm_free_for_dma("streambuf2", buf[2]);
	codec_mm_free_for_dma("streambuf1", buf[0]);
	dump_mem_infos(NULL, 0);
#endif

	return 0;
}

static ssize_t codec_mm_dump_show(struct class *class,
	struct class_attribute *attr, char *buf)
{
	size_t ret;
	ret = dump_mem_infos(buf, PAGE_SIZE);
	return ret;
}

static struct class_attribute codec_mm_class_attrs[] = {
	__ATTR_RO(codec_mm_dump),
	__ATTR_NULL
};

static struct class codec_mm_class = {
		.name = "codec_mm",
		.class_attrs = codec_mm_class_attrs,
	};

static int codec_mm_probe(struct platform_device *pdev)
{

	int r;
	pdev->dev.platform_data = get_mem_mgt();
	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		pr_info("codec_mm_probe mem init done\n");

	codec_mm_mgt_init(&pdev->dev);
	r = class_register(&codec_mm_class);
	if (r) {
		pr_info("vdec class create fail.\n");
		return r;
	}
	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		pr_info("codec_mm reserved memory probed done\n");

	pr_info("codec_mm_probe ok\n");
	amstream_test_init();

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

static int __init codec_mm_module_init(void)
{

	pr_err("codec_mm_module_init\n");

	if (platform_driver_register(&codec_mm_driver)) {
		pr_err("failed to register amports mem module\n");
		return -ENODEV;

	}

	return 0;
}

arch_initcall(codec_mm_module_init);
MODULE_DESCRIPTION("AMLOGIC amports mem  driver");
MODULE_LICENSE("GPL");

#if 0
static int __init codec_mm_cma_setup(struct reserved_mem *rmem)
{
	int ret;
	phys_addr_t base, size, align;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	align = (phys_addr_t) PAGE_SIZE << max(MAX_ORDER - 1, pageblock_order);
	base = ALIGN(rmem->base, align);
	size = round_down(rmem->size - (base - rmem->base), align);
	ret = cma_init_reserved_mem(base, size, 0, &mgt->cma);
	if (ret) {
		pr_info("TT:vdec: cma init reserve area failed.\n");
		mgt->cma = NULL;
		return ret;
	}

#ifndef CONFIG_ARM64
	dma_contiguous_early_fixup(base, size);
#endif
	pr_info("TT:vdec: cma setup\n");
	return 0;
}

RESERVEDMEM_OF_DECLARE(codec_mm_cma, "amlogic, codec-mm-cma",
					   codec_mm_cma_setup);

#endif
static int codec_mm_reserved_init(struct reserved_mem *rmem, struct device *dev)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	mgt->rmem = *rmem;
	pr_info("codec_mm_reserved_init %p->%p\n",
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
	pr_info("vdec: reserved mem setup\n");

	return 0;
}


RESERVEDMEM_OF_DECLARE(codec_mm_reversed, "amlogic, codec-mm-reserve",
					   codec_mm_res_setup);
