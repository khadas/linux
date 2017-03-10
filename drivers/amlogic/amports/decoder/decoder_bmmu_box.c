/*
 * drivers/amlogic/amports/decoder/decoder_bmmu_box.c
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
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/platform_device.h>

#include "../video_keeper.h"
#include "decoder_bmmu_box.h"

struct decoder_bmmu_box {
	int max_mm_num;
	const char *name;
	int channel_id;
	struct mutex mutex;
	struct list_head list;
	int total_size;
	int change_size_on_need_smaller;
	int align2n;		/*can overwite on idx alloc */
	int mem_flags;		/*can overwite on idx alloc */
	struct codec_mm_s *mm_list[1];
};

struct decoder_bmmu_box_mgr {
	int num;
	struct mutex mutex;
	struct list_head box_list;
};
static struct decoder_bmmu_box_mgr global_blk_mgr;
static struct decoder_bmmu_box_mgr *get_decoder_bmmu_box_mgr(void)
{
	return &global_blk_mgr;
}

static int decoder_bmmu_box_mgr_add_box(struct decoder_bmmu_box *box)
{
	struct decoder_bmmu_box_mgr *mgr = get_decoder_bmmu_box_mgr();
	mutex_lock(&mgr->mutex);
	list_add_tail(&box->list, &mgr->box_list);
	mutex_unlock(&mgr->mutex);
	return 0;
}

static int decoder_bmmu_box_mgr_del_box(struct decoder_bmmu_box *box)
{
	struct decoder_bmmu_box_mgr *mgr = get_decoder_bmmu_box_mgr();
	mutex_lock(&mgr->mutex);
	list_del(&box->list);
	mutex_unlock(&mgr->mutex);
	return 0;
}

void *decoder_bmmu_box_alloc_box(const char *name,
		int channel_id, int max_num,
		int aligned, int mem_flags)
/*min_size_M:wait alloc this size*/
{
	struct decoder_bmmu_box *box;
	int size;
	int tvp_flags;
	tvp_flags = (mem_flags & CODEC_MM_FLAGS_TVP) ?
		CODEC_MM_FLAGS_TVP : 0;

	pr_info("decoder_bmmu_box_alloc_box, tvp_flags = %x\n", tvp_flags);

	size = sizeof(struct decoder_bmmu_box) + sizeof(struct codec_mm_s *) *
		   max_num;
	box = kmalloc(size, GFP_KERNEL);
	if (!box) {
		pr_err("can't alloc decoder buffers box!!!\n");
		return NULL;
	}
	memset(box, 0, size);
	box->max_mm_num = max_num;
	box->name = name;
	box->channel_id = channel_id;
	box->align2n = aligned;
	box->mem_flags = mem_flags | tvp_flags;
	mutex_init(&box->mutex);
	INIT_LIST_HEAD(&box->list);
	decoder_bmmu_box_mgr_add_box(box);
	return (void *)box;
}

int decoder_bmmu_box_alloc_idx(void *handle, int idx, int size, int aligned_2n,
							   int mem_flags)
/*align& flags if -1 user box default.*/
{
	struct decoder_bmmu_box *box = handle;
	struct codec_mm_s *mm;
	int align = aligned_2n;
	int memflags = mem_flags;

	if (!box || idx < 0 || idx >= box->max_mm_num) {
		pr_err("can't alloc mmu box(%p),idx:%d\n",
				box, idx);
		return -1;
	}
	if (align == -1)
		align = box->align2n;
	if (memflags == -1)
		memflags = box->mem_flags;

	mutex_lock(&box->mutex);
	mm = box->mm_list[idx];
	if (mm) {
		int invalid = 0;
		if (mm->page_count * PAGE_SIZE < size) {
			/*size is small. */
			invalid = 1;
		} else if (box->change_size_on_need_smaller &&
				   (mm->buffer_size > (size << 1))) {
			/*size is too large. */
			invalid = 2;
		} else if (mm->phy_addr & ((1 << align) - 1)) {
			/*addr is not align */
			invalid = 4;
		}
		if (invalid) {
			box->total_size -= mm->buffer_size;
			codec_mm_release(mm, box->name);
			box->mm_list[idx] = NULL;
			mm = NULL;
		}
	}
	if (!mm) {
		mm = codec_mm_alloc(box->name, size, align, memflags);
		if (mm) {
			box->mm_list[idx] = mm;
			box->total_size += mm->buffer_size;
		}
	}
	mutex_unlock(&box->mutex);
	return mm ? 0 : -ENOMEM;
}

int decoder_bmmu_box_free_idx(void *handle, int idx)
{
	struct decoder_bmmu_box *box = handle;
	struct codec_mm_s *mm;
	if (!box || idx < 0 || idx >= box->max_mm_num) {
		pr_err("can't free idx of box(%p),idx:%d  in (%d-%d)\n",
				box, idx, 0,
			   box->max_mm_num - 1);
		return -1;
	}
	mutex_lock(&box->mutex);
	mm = box->mm_list[idx];
	if (mm) {
		box->total_size -= mm->buffer_size;
		codec_mm_release(mm, box->name);
		box->mm_list[idx] = NULL;
		mm = NULL;
	}
	mutex_unlock(&box->mutex);
	return 0;
}

int decoder_bmmu_box_free(void *handle)
{
	struct decoder_bmmu_box *box = handle;
	struct codec_mm_s *mm;
	int i;
	if (!box) {
		pr_err("can't free box of NULL box!\n");
		return -1;
	}
	mutex_lock(&box->mutex);
	for (i = 0; i < box->max_mm_num; i++) {
		mm = box->mm_list[i];
		if (mm) {
			codec_mm_release(mm, box->name);
			box->mm_list[i] = NULL;
		}
	}
	mutex_unlock(&box->mutex);
	decoder_bmmu_box_mgr_del_box(box);
	kfree(box);
	return 0;
}

void *decoder_bmmu_box_get_mem_handle(void *box_handle, int idx)
{
	struct decoder_bmmu_box *box = box_handle;
	if (!box || idx < 0 || idx >= box->max_mm_num)
		return NULL;
	return box->mm_list[idx];
}

int decoder_bmmu_box_get_mem_size(void *box_handle, int idx)
{
	struct decoder_bmmu_box *box = box_handle;
	int size = 0;
	if (!box || idx < 0 || idx >= box->max_mm_num)
		return 0;
	mutex_lock(&box->mutex);
	if (box->mm_list[idx] != NULL)
		size = box->mm_list[idx]->buffer_size;
	mutex_unlock(&box->mutex);
	return size;
}


unsigned long decoder_bmmu_box_get_phy_addr(void *box_handle, int idx)
{
	struct decoder_bmmu_box *box = box_handle;
	struct codec_mm_s *mm;
	if (!box || idx < 0 || idx >= box->max_mm_num)
		return 0;
	mm = box->mm_list[idx];
	if (!mm)
		return 0;
	return mm->phy_addr;
}

void *decoder_bmmu_box_get_virt_addr(void *box_handle, int idx)
{
	struct decoder_bmmu_box *box = box_handle;
	struct codec_mm_s *mm;
	if (!box || idx < 0 || idx >= box->max_mm_num)
		return NULL;
	mm = box->mm_list[idx];
	if (!mm)
		return 0;
	return codec_mm_phys_to_virt(mm->phy_addr);
}

/*flags: &0x1 for wait,*/
int decoder_bmmu_box_check_and_wait_size(int size, int flags)
{
	if ((flags & BMMU_ALLOC_FLAGS_CAN_CLEAR_KEEPER) &&
		codec_mm_get_free_size() < size) {
		pr_err("CMA force free keep,for size = %d\n", size);
		/*need free others?
		*/
		try_free_keep_video(1);
	}

	return codec_mm_enough_for_size(size,
			flags & BMMU_ALLOC_FLAGS_WAIT);
}

int decoder_bmmu_box_alloc_idx_wait(
	void *handle, int idx,
	int size, int aligned_2n,
	int mem_flags,
	int wait_flags)
{
	int have_space;
	int ret = -1;
	if (decoder_bmmu_box_get_mem_size(handle, idx) >= size)
		return 0;/*have alloced memery before.*/
	have_space = decoder_bmmu_box_check_and_wait_size(
					size,
					wait_flags);
	if (have_space) {
		ret = decoder_bmmu_box_alloc_idx(handle,
				idx, size, aligned_2n, mem_flags);
	} else {
		ret = -ENOMEM;
	}
	return ret;
}

int decoder_bmmu_box_alloc_buf_phy(
	void *handle, int idx,
	int size, unsigned char *driver_name,
	unsigned long *buf_phy_addr)
{
	if (!decoder_bmmu_box_check_and_wait_size(
			size,
			1)) {
		pr_info("%s not enough buf for buf_idx = %d\n",
					driver_name, idx);
		return	-ENOMEM;
	}
	if (!decoder_bmmu_box_alloc_idx_wait(
			handle,
			idx,
			size,
			-1,
			-1,
			BMMU_ALLOC_FLAGS_WAITCLEAR
			)) {
		*buf_phy_addr =
			decoder_bmmu_box_get_phy_addr(
			handle,
			idx);
		pr_info("%s malloc buf_idx = %d addr = %ld size = %d\n",
			driver_name, idx, *buf_phy_addr, size);
		} else {
		pr_info("%s malloc failed  %d\n", driver_name, idx);
			return -ENOMEM;
	}

	return 0;
}

static int decoder_bmmu_box_dump(struct decoder_bmmu_box *box, void *buf,
								 int size)
{
	char *pbuf = buf;
	char sbuf[512];
	int tsize = 0;
	int s;
	int i;
	if (!pbuf)
		pbuf = sbuf;

#define BUFPRINT(args...) \
	do {\
		s = sprintf(pbuf, args);\
		tsize += s;\
		pbuf += s; \
	} while (0)

	for (i = 0; i < box->max_mm_num; i++) {
		struct codec_mm_s *mm = box->mm_list[i];
		if (buf && (size - tsize) < 128)
			break;
		if (mm) {
			BUFPRINT("code mem[%d]:%p, addr=%p, size=%d,from=%d\n",
					 i,
					 (void *)mm,
					 (void *)mm->phy_addr,
					 mm->buffer_size,
					 mm->from_flags);
		}
	}
#undef BUFPRINT
	if (!buf)
		pr_info("%s", sbuf);

	return tsize;
}

static int decoder_bmmu_box_dump_all(void *buf, int size)
{
	struct decoder_bmmu_box_mgr *mgr = get_decoder_bmmu_box_mgr();
	char *pbuf = buf;
	char sbuf[512];
	int tsize = 0;
	int s;
	int i;
	struct list_head *head, *list;
	if (!pbuf)
		pbuf = sbuf;

#define BUFPRINT(args...) \
	do {\
		s = sprintf(pbuf, args);\
		tsize += s;\
		pbuf += s; \
	} while (0)

	mutex_lock(&mgr->mutex);
	head = &mgr->box_list;
	list = head->next;
	i = 0;
	while (list != head) {
		struct decoder_bmmu_box *box;
		box = list_entry(list, struct decoder_bmmu_box, list);
		BUFPRINT("box[%d]: %s, %splayer_id:%d, max_num:%d, size:%d\n",
				 i, box->name,
				 (box->mem_flags & CODEC_MM_FLAGS_TVP) ?
				 "TVP mode " : "",
				 box->channel_id,
				 box->max_mm_num,
				 box->total_size);
		if (buf) {
			tsize += decoder_bmmu_box_dump(box, pbuf, size - tsize);
			if (tsize > 0)
				pbuf += tsize;
		} else {
			pr_info("%s", sbuf);
			pbuf = sbuf;
			tsize += decoder_bmmu_box_dump(box, NULL, 0);
		}
		list = list->next;
		i++;
	}
	mutex_unlock(&mgr->mutex);

#undef BUFPRINT
	if (!buf)
		pr_info("%s", sbuf);
	return tsize;
}

static ssize_t box_dump_show(struct class *class, struct class_attribute *attr,
							 char *buf)
{
	ssize_t ret = 0;
	ret = decoder_bmmu_box_dump_all(buf, PAGE_SIZE);
	return ret;
}

static struct class_attribute decoder_bmmu_box_class_attrs[] = {
	__ATTR_RO(box_dump),
	__ATTR_NULL
};

static struct class decoder_bmmu_box_class = {
		.name = "decoder_bmmu_box",
		.class_attrs = decoder_bmmu_box_class_attrs,
	};

static int __init decoder_bmmu_box_init(void)
{
	int r;
	memset(&global_blk_mgr, 0, sizeof(global_blk_mgr));
	INIT_LIST_HEAD(&global_blk_mgr.box_list);
	mutex_init(&global_blk_mgr.mutex);
	r = class_register(&decoder_bmmu_box_class);
	return r;
}

module_init(decoder_bmmu_box_init);
