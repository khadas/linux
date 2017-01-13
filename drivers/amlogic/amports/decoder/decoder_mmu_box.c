/*
 * drivers/amlogic/amports/decoder/decoder_mmu_box.c
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
#include <linux/amlogic/codec_mm/codec_mm_scatter.h>
#include <linux/platform_device.h>
struct decoder_mmu_box {
	int max_sc_num;
	const char *name;
	int channel_id;
	int tvp_mode;
	struct mutex mutex;
	struct list_head list;
	struct codec_mm_scatter *sc_list[1];
};
#define MAX_KEEP_FRAME 4
#define START_KEEP_ID 0x9
#define MAX_KEEP_ID    (INT_MAX - 1)
struct decoder_mmu_box_mgr {
	int num;
	struct mutex mutex;
	struct codec_mm_scatter *keep_sc[MAX_KEEP_FRAME];
	int	keep_id[MAX_KEEP_FRAME];
	int next_id;/*id for keep & free.*/
	struct list_head box_list;
};
static struct decoder_mmu_box_mgr global_mgr;
static struct decoder_mmu_box_mgr *get_decoder_mmu_box_mgr(void)
{
	return &global_mgr;
}

static int decoder_mmu_box_mgr_add_box(struct decoder_mmu_box *box)
{
	struct decoder_mmu_box_mgr *mgr = get_decoder_mmu_box_mgr();
	mutex_lock(&mgr->mutex);
	list_add_tail(&box->list, &mgr->box_list);
	mutex_unlock(&mgr->mutex);
	return 0;
}

static int decoder_mmu_box_mgr_del_box(struct decoder_mmu_box *box)
{
	struct decoder_mmu_box_mgr *mgr = get_decoder_mmu_box_mgr();
	mutex_lock(&mgr->mutex);
	list_del(&box->list);
	mutex_unlock(&mgr->mutex);
	return 0;
}



void *decoder_mmu_box_alloc_box(const char *name,
	int channel_id,
	int max_num,
	int min_size_M)
/*min_size_M:wait alloc this size*/
{
	struct decoder_mmu_box *box;
	int size;

	size = sizeof(struct decoder_mmu_box) +
			sizeof(struct codec_mm_scatter *) *
			max_num;
	box = kmalloc(size, GFP_KERNEL);
	if (!box) {
		pr_err("can't alloc decoder buffers box!!!\n");
		return NULL;
	}
	memset(box, 0, size);
	box->max_sc_num = max_num;
	box->name = name;
	box->channel_id = channel_id;
	box->tvp_mode = codec_mm_video_tvp_enabled() ?
		CODEC_MM_FLAGS_TVP : 0;
	/*TODO.changed to tvp flags from decoder init*/
	mutex_init(&box->mutex);
	INIT_LIST_HEAD(&box->list);
	decoder_mmu_box_mgr_add_box(box);
	codec_mm_scatter_mgt_delay_free_swith(1, 2000,
		min_size_M, box->tvp_mode);
	return (void *)box;
}

int decoder_mmu_box_alloc_idx(
	void *handle, int idx, int num_pages,
	unsigned int *mmu_index_adr)
{
	struct decoder_mmu_box *box = handle;
	struct codec_mm_scatter *sc;
	int ret;
	int i;

	if (!box || idx < 0 || idx >= box->max_sc_num) {
		pr_err("can't alloc mmu box(%p),idx:%d\n",
			box, idx);
		return -1;
	}
	mutex_lock(&box->mutex);
	sc = box->sc_list[idx];
	if (sc) {
		if (sc->page_max_cnt >= num_pages)
			ret = codec_mm_scatter_alloc_want_pages(sc,
				num_pages);
		else {
			codec_mm_scatter_dec_owner_user(sc, 0);
			box->sc_list[idx] = NULL;
			sc = NULL;
		}

	}
	if (!sc) {
		sc = codec_mm_scatter_alloc(num_pages + 64, num_pages,
			box->tvp_mode);
		if (!sc) {
			mutex_unlock(&box->mutex);
			pr_err("alloc mmu failed, need pages=%d\n",
				num_pages);
			return -1;
		}
		box->sc_list[idx] = sc;
	}

	for (i = 0; i < num_pages; i++)
		mmu_index_adr[i] = PAGE_INDEX(sc->pages_list[i]);
	mmu_index_adr[num_pages] = 0;

	mutex_unlock(&box->mutex);

	return 0;
}

int decoder_mmu_box_free_idx_tail(
		void *handle, int idx,
		int start_release_index)
{
	struct decoder_mmu_box *box = handle;
	struct codec_mm_scatter *sc;
	if (!box || idx < 0 || idx >= box->max_sc_num) {
		pr_err("can't free tail mmu box(%p),idx:%d in (%d-%d)\n",
			box, idx, 0,
			box->max_sc_num - 1);
		return -1;
	}
	mutex_lock(&box->mutex);
	sc = box->sc_list[idx];
	if (sc && start_release_index < sc->page_cnt)
		codec_mm_scatter_free_tail_pages_fast(sc,
				start_release_index);
	mutex_unlock(&box->mutex);
	return 0;
}

int decoder_mmu_box_free_idx(void *handle, int idx)
{
	struct decoder_mmu_box *box = handle;
	struct codec_mm_scatter *sc;
	if (!box || idx < 0 || idx >= box->max_sc_num) {
		pr_err("can't free idx of box(%p),idx:%d  in (%d-%d)\n",
			box, idx, 0,
			box->max_sc_num - 1);
		return -1;
	}
	mutex_lock(&box->mutex);
	sc = box->sc_list[idx];
	if (sc && sc->page_cnt > 0) {
		codec_mm_scatter_dec_owner_user(sc, 0);
		box->sc_list[idx] = NULL;
	} mutex_unlock(&box->mutex);
	return 0;
}


int decoder_mmu_box_free(void *handle)
{
	struct decoder_mmu_box *box = handle;
	struct codec_mm_scatter *sc;
	int i;
	if (!box) {
		pr_err("can't free box of NULL box!\n");
		return -1;
	}
	mutex_lock(&box->mutex);
	for (i = 0; i < box->max_sc_num; i++) {
		sc = box->sc_list[i];
		if (sc) {
			codec_mm_scatter_dec_owner_user(sc, 200);
			box->sc_list[i] = NULL;
		}
	}
	mutex_unlock(&box->mutex);
	decoder_mmu_box_mgr_del_box(box);
	codec_mm_scatter_mgt_delay_free_swith(0, 2000, 0, box->tvp_mode);
	kfree(box);
	return 0;
}

void *decoder_mmu_box_get_mem_handle(void *box_handle, int idx)
{
	struct decoder_mmu_box *box = box_handle;
	if (!box || idx < 0 || idx >= box->max_sc_num)
		return NULL;
	return  box->sc_list[idx];
}


static int decoder_mmu_box_dump(struct decoder_mmu_box *box,
				void *buf, int size)
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

	for (i = 0; i < box->max_sc_num; i++) {
		struct codec_mm_scatter *sc = box->sc_list[i];
		if (sc) {
			BUFPRINT("sc mem[%d]:%p, size=%d\n",
				i, sc,
				sc->page_cnt << PAGE_SHIFT);
		}
	}
#undef BUFPRINT
	if (!buf)
		pr_info("%s", sbuf);

	return tsize;
}

static int decoder_mmu_box_dump_all(void *buf, int size)
{
	struct decoder_mmu_box_mgr *mgr = get_decoder_mmu_box_mgr();
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
		struct decoder_mmu_box *box;
		box = list_entry(list, struct decoder_mmu_box,
							list);
		BUFPRINT("box[%d]: %s, %splayer_id:%d, max_num:%d\n",
			i,
			box->name,
			box->tvp_mode ? "TVP mode " : "",
			box->channel_id,
			box->max_sc_num);
		if (buf) {
			tsize += decoder_mmu_box_dump(box, pbuf, size - tsize);
			if (tsize > 0)
				pbuf += tsize;
		} else {
			pr_info("%s", sbuf);
			pbuf = sbuf;
			tsize += decoder_mmu_box_dump(box, NULL, 0);
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



static ssize_t
box_dump_show(struct class *class,
		       struct class_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	ret = decoder_mmu_box_dump_all(buf, PAGE_SIZE);
	return ret;
}



static struct class_attribute decoder_mmu_box_class_attrs[] = {
	__ATTR_RO(box_dump),
	__ATTR_NULL
};

static struct class decoder_mmu_box_class = {
	.name = "decoder_mmu_box",
	.class_attrs = decoder_mmu_box_class_attrs,
};


static int __init decoder_mmu_box_init(void)
{
	int r;
	memset(&global_mgr, 0, sizeof(global_mgr));
	INIT_LIST_HEAD(&global_mgr.box_list);
	mutex_init(&global_mgr.mutex);
	global_mgr.next_id = START_KEEP_ID;
	r = class_register(&decoder_mmu_box_class);
	return r;
}

module_init(decoder_mmu_box_init);

