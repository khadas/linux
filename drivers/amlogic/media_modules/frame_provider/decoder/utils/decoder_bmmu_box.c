/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#define DEBUG
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
#include <linux/amlogic/media/codec_mm/codec_mm_scatter.h>
#include <linux/platform_device.h>

#include <linux/amlogic/media/video_sink/video_keeper.h>
#include "decoder_bmmu_box.h"
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_keeper.h>

struct mm_list_expand {
	int index;
	struct list_head mm_list;
	struct codec_mm_s *mm;
};

struct decoder_bmmu_box {
	int max_mm_num;
	int exp_num;
	const char *name;
	int channel_id;
	struct mutex mutex;
	struct list_head list;
	int total_size;
	int box_ref_cnt;
	int change_size_on_need_smaller;
	int align2n;		/*can overwite on idx alloc */
	int mem_flags;		/*can overwite on idx alloc */
	u32 alloc_flags;
	struct mm_list_expand exp_mm_list;
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

bool decoder_bmmu_box_valide_check(void *box)
{
	struct decoder_bmmu_box_mgr *mgr = get_decoder_bmmu_box_mgr();
	struct decoder_bmmu_box *bmmu_box = NULL;
	bool is_valide = false;

	mutex_lock(&mgr->mutex);
	list_for_each_entry(bmmu_box, &mgr->box_list, list) {
		if (bmmu_box && bmmu_box == box) {
			is_valide = true;
			break;
		}
	}
	mutex_unlock(&mgr->mutex);

	return is_valide;
}
EXPORT_SYMBOL(decoder_bmmu_box_valide_check);

void *decoder_bmmu_box_alloc_box(const char *name,
		int channel_id, int max_num,
		int aligned, int mem_flags,
		u32 alloc_flags)
/*min_size_M:wait alloc this size*/
{
	struct decoder_bmmu_box *box;
	int size;
	int tvp_flags;
	tvp_flags = (mem_flags & CODEC_MM_FLAGS_TVP) ?
		CODEC_MM_FLAGS_TVP : 0;

	pr_debug("decoder_bmmu_box_alloc_box, tvp_flags = %x\n", tvp_flags);

	size = sizeof(struct decoder_bmmu_box) + sizeof(struct codec_mm_s *) *
		   max_num;
	box = kzalloc(size, GFP_KERNEL);
	if (!box) {
		pr_err("can't alloc decoder buffers box!!!\n");
		return NULL;
	}
	box->max_mm_num = max_num;
	box->name = name;
	box->channel_id = channel_id;
	box->align2n = aligned;
	box->mem_flags = mem_flags | tvp_flags;
	box->alloc_flags = alloc_flags;
	box->exp_num = 0;
	box->exp_mm_list.mm = NULL;
	box->exp_mm_list.index = -1;
	INIT_LIST_HEAD(&box->exp_mm_list.mm_list);
	mutex_init(&box->mutex);
	INIT_LIST_HEAD(&box->list);
	decoder_bmmu_box_mgr_add_box(box);
	return (void *)box;
}
EXPORT_SYMBOL(decoder_bmmu_box_alloc_box);

struct codec_mm_s *decoder_bmmu_box_get_mm_from_idx(
	struct decoder_bmmu_box *box, int idx)
{
	struct list_head *p;
	struct mm_list_expand *ent;

	if (!box || idx < 0) {
		pr_err("can't get mm from box(%p),idx:%d\n",
			box, idx);
		return NULL;
	}
	if (likely(idx < box->max_mm_num)) {
		return box->mm_list[idx];
	} else {
		list_for_each(p, &box->exp_mm_list.mm_list) {
			ent = list_entry(p, struct mm_list_expand, mm_list);
			if (ent->index == idx) {
				return ent->mm;
			}
		}
		return NULL;
	}
}

void decoder_bmmu_box_set_mm_from_idx(
	struct decoder_bmmu_box *box, int idx, struct codec_mm_s *mm)
{
	struct list_head *p;
	struct mm_list_expand *ent, *exp;

	if (!box || idx < 0) {
		pr_err("can't set mm to box(%s),idx:%d\n",
			box->name, idx);
		return;
	}
	if (likely(idx < box->max_mm_num)) {
		box->mm_list[idx] = mm;
	} else {
		list_for_each(p, &box->exp_mm_list.mm_list) {
			ent = list_entry(p, struct mm_list_expand, mm_list);
			if (ent->index == idx) {
				ent->mm = mm;
				return;
			}
		}

		exp = kzalloc(sizeof(struct mm_list_expand), GFP_KERNEL);
		if (!exp) {
			pr_err("creat exp_list failed! box name%s, idx=%d\n", box->name, idx);
			return;
		}
		exp->index = idx;
		exp->mm = mm;
		box->exp_num++;
		list_add(&exp->mm_list, &box->exp_mm_list.mm_list);
		pr_debug("add new node to expand list, idx = %d, exp_num=%d.\n", idx ,box->exp_num);
		return;
	}
}

int decoder_bmmu_box_alloc_idx(void *handle, int idx, int size, int aligned_2n,
							   int mem_flags)
/*align& flags if -1 user box default.*/
{
	struct decoder_bmmu_box *box = handle;
	struct codec_mm_s *mm;
	int align = aligned_2n;
	int memflags = mem_flags;

	if (!box || idx < 0) {
		pr_err("can't alloc bmmu box(%p),idx:%d\n",
				box, idx);
		return -1;
	}
	if (align == -1)
		align = box->align2n;
	if (memflags == -1)
		memflags = box->mem_flags;

	mutex_lock(&box->mutex);
	mm = decoder_bmmu_box_get_mm_from_idx(box, idx);
	if (mm) {
		int invalid = 0;
		int keeped = 0;

		keeped = is_codec_mm_keeped(mm);
		if (!keeped) {
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
				decoder_bmmu_box_set_mm_from_idx(box, idx, NULL);
				mm = NULL;
			}
		} else {
			box->total_size -= mm->buffer_size;
			codec_mm_release(mm, box->name);
			decoder_bmmu_box_set_mm_from_idx(box, idx, NULL);
			mm = NULL;
		}
	}
	if (!mm) {
		mm = codec_mm_alloc(box->name, size, align, memflags);
		if (mm) {
			decoder_bmmu_box_set_mm_from_idx(box, idx, mm);
			box->total_size += mm->buffer_size;
			mm->ins_id = box->channel_id;
			mm->ins_buffer_id = idx;
			box->box_ref_cnt++;
		}
	}
	mutex_unlock(&box->mutex);
	return mm ? 0 : -ENOMEM;
}

int decoder_bmmu_box_free_idx(void *handle, int idx)
{
	struct decoder_bmmu_box *box = handle;
	struct codec_mm_s *mm;

	if (!box || idx < 0) {
		pr_err("can't free idx of box(%p),idx:%d  in (%d-%d)\n",
				box, idx, 0,
			   box ? (box->max_mm_num - 1) : 0);
		return -1;
	}
	mutex_lock(&box->mutex);
	mm = decoder_bmmu_box_get_mm_from_idx(box, idx);
	if (mm) {
		box->total_size -= mm->buffer_size;
		codec_mm_release(mm, box->name);
		decoder_bmmu_box_set_mm_from_idx(box, idx, NULL);
		mm = NULL;
		box->box_ref_cnt--;
	}
	mutex_unlock(&box->mutex);
	return 0;
}
EXPORT_SYMBOL(decoder_bmmu_box_free_idx);

int decoder_bmmu_box_free(void *handle)
{
	struct decoder_bmmu_box *box = handle;
	struct codec_mm_s *mm;
	int i;
	struct mm_list_expand *p;
	struct mm_list_expand *next;

	if (!box) {
		pr_err("can't free box of NULL box!\n");
		return -1;
	}
	mutex_lock(&box->mutex);
	for (i = 0; i < box->max_mm_num; i++) {
		mm = decoder_bmmu_box_get_mm_from_idx(box, i);
		if (mm) {
			codec_mm_release(mm, box->name);
			decoder_bmmu_box_set_mm_from_idx(box, i, NULL);
		}
	}
	list_for_each_entry_safe(p, next, &box->exp_mm_list.mm_list, mm_list) {
		if (p->mm) {
			codec_mm_release(p->mm, box->name);
			decoder_bmmu_box_set_mm_from_idx(box, p->index, NULL);
		}
		list_del(&p->mm_list);
		kfree(p);
		box->exp_num--;
	}
	mutex_unlock(&box->mutex);
	decoder_bmmu_box_mgr_del_box(box);
	kfree(box);
	return 0;
}
EXPORT_SYMBOL(decoder_bmmu_box_free);

void decoder_bmmu_try_to_release_box(void *handle)
{
	struct decoder_bmmu_box *box = handle;
	bool is_keep = false;
	int i;
	struct list_head *p;
	struct mm_list_expand *ent;

	if (!box || box->box_ref_cnt)
		return;

	mutex_lock(&box->mutex);
	for (i = 0; i <box->max_mm_num; i++) {
		if (decoder_bmmu_box_get_mm_from_idx(box, i)) {
			is_keep = true;
			break;
		}
	}
	list_for_each(p, &box->exp_mm_list.mm_list) {
		ent = list_entry(p, struct mm_list_expand, mm_list);
		if (ent->mm) {
			is_keep = true;
			break;
		}
	}
	mutex_unlock(&box->mutex);

	if (!is_keep) {
		decoder_bmmu_box_mgr_del_box(box);
		kfree(box);
	}
}
EXPORT_SYMBOL(decoder_bmmu_try_to_release_box);

void *decoder_bmmu_box_get_mem_handle(void *box_handle, int idx)
{
	struct decoder_bmmu_box *box = box_handle;

	if (!box || idx < 0)
		return NULL;
	return decoder_bmmu_box_get_mm_from_idx(box, idx);;
}
EXPORT_SYMBOL(decoder_bmmu_box_get_mem_handle);

int decoder_bmmu_box_get_mem_size(void *box_handle, int idx)
{
	struct decoder_bmmu_box *box = box_handle;
	int size = 0;
	struct codec_mm_s *mm;

	if (!box || idx < 0)
		return 0;

	mm = decoder_bmmu_box_get_mm_from_idx(box, idx);
	if (mm != NULL)
		size = mm->buffer_size;
	return size;
}


unsigned long decoder_bmmu_box_get_phy_addr(void *box_handle, int idx)
{
	struct decoder_bmmu_box *box = box_handle;
	struct codec_mm_s *mm;

	if (!box || idx < 0)
		return 0;
	mm = decoder_bmmu_box_get_mm_from_idx(box, idx);
	if (!mm)
		return 0;
	return mm->phy_addr;
}
EXPORT_SYMBOL(decoder_bmmu_box_get_phy_addr);

void *decoder_bmmu_box_get_virt_addr(void *box_handle, int idx)
{
	struct decoder_bmmu_box *box = box_handle;
	struct codec_mm_s *mm;

	if (!box || idx < 0)
		return NULL;
	mm = decoder_bmmu_box_get_mm_from_idx(box, idx);
	if (!mm)
		return 0;
	return codec_mm_phys_to_virt(mm->phy_addr);
}

/*flags: &0x1 for wait,*/
int decoder_bmmu_box_check_and_wait_size(int size, int flags, int mem_flags)
{
	if ((flags & BMMU_ALLOC_FLAGS_CAN_CLEAR_KEEPER) &&
		codec_mm_get_free_size() < size) {
		pr_err("CMA force free keep,for size = %d\n", size);
		/*need free others?
		 */
		try_free_keep_video(1);
	}

	return codec_mm_enough_for_size(size,
			flags & BMMU_ALLOC_FLAGS_WAIT, mem_flags);
}

int decoder_bmmu_box_alloc_idx_wait(
	void *handle, int idx,
	int size, int aligned_2n,
	int mem_flags,
	int wait_flags)
{
	int have_space;
	int ret = -1;
	int keeped = 0;

	if (decoder_bmmu_box_get_mem_size(handle, idx) >= size) {
		struct decoder_bmmu_box *box = handle;
		struct codec_mm_s *mm;
		mutex_lock(&box->mutex);
		mm = decoder_bmmu_box_get_mm_from_idx(box, idx);
		keeped = is_codec_mm_keeped(mm);
		mutex_unlock(&box->mutex);

		if (!keeped)
			return 0;/*have alloced memery before.*/
	}
	have_space = decoder_bmmu_box_check_and_wait_size(
					size,
					wait_flags,
					mem_flags);
	if (have_space) {
		ret = decoder_bmmu_box_alloc_idx(handle,
				idx, size, aligned_2n, mem_flags);
		if (ret == -ENOMEM) {
			if (wait_flags & BMMU_ALLOC_FLAGS_CAN_CLEAR_KEEPER) {
				pr_info("bmmu alloc idx fail, try free keep video.\n");
				try_free_keep_video(1);
			}
		}
	} else {
		if (wait_flags & BMMU_ALLOC_FLAGS_CAN_CLEAR_KEEPER) {
			try_free_keep_video(1);
		}

		ret = -ENOMEM;
	}
	return ret;
}
EXPORT_SYMBOL(decoder_bmmu_box_alloc_idx_wait);

int decoder_bmmu_box_alloc_buf_phy(
	void *handle, int idx,
	int size, unsigned char *driver_name,
	unsigned long *buf_phy_addr)
{
	struct decoder_bmmu_box *bmmu_box = (struct decoder_bmmu_box *)handle;

	if (bmmu_box == NULL)
		return -EINVAL;

	if (!decoder_bmmu_box_check_and_wait_size(
			size,
			1, bmmu_box->mem_flags)) {
		pr_info("%s not enough buf for buf_idx = %d\n",
					driver_name, idx);
		return	-ENOMEM;
	}
	if (!decoder_bmmu_box_alloc_idx_wait(
			handle,
			idx,
			size,
			-1,
			bmmu_box->mem_flags,
			bmmu_box->alloc_flags)) {
		*buf_phy_addr =
			decoder_bmmu_box_get_phy_addr(
			handle,
			idx);
		/*
		 *pr_info("%s malloc buf_idx = %d addr = %ld size = %d\n",
		 *	driver_name, idx, *buf_phy_addr, size);
		 */
		} else {
			pr_info("%s malloc failed  %d\n", driver_name, idx);
			return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(decoder_bmmu_box_alloc_buf_phy);

int decoder_bmmu_box_add_callback_func(
	void *handle, int idx,
	void *cb)
{
	struct decoder_bmmu_box *box = handle;
	struct codec_mm_s *mm;

	if (!box || idx < 0)
		return 0;

	mutex_lock(&box->mutex);
	mm = decoder_bmmu_box_get_mm_from_idx(box, idx);
	codec_mm_add_release_callback(mm, (struct codec_mm_cb_s *)cb);
	mutex_unlock(&box->mutex);

	return 0;
}
EXPORT_SYMBOL(decoder_bmmu_box_add_callback_func);

static int decoder_bmmu_box_dump(struct decoder_bmmu_box *box, void *buf,
								 int size)
{
	char *pbuf = buf;
	char sbuf[512];
	int tsize = 0;
	int s;
	int i;
	struct list_head *p;
	struct mm_list_expand *ent;

	if (!buf) {
		pbuf = sbuf;
		size = 512;
	}
#define BUFPRINT(args...) \
	do {\
		s = snprintf(pbuf, size - tsize, args);\
		tsize += s;\
		pbuf += s; \
	} while (0)

	BUFPRINT("**mm list**\n");
	for (i = 0; i < box->max_mm_num; i++) {
		struct codec_mm_s *mm = decoder_bmmu_box_get_mm_from_idx(box, i);
		if (buf && (size - tsize) < 256) {
			BUFPRINT("\n\t**NOT END**\n");
			break;
		}
		if (mm) {
			BUFPRINT("code mem[%d]:%p, addr=%p, size=%d,from=%d\n",
					 i,
					 (void *)mm,
					 (void *)mm->phy_addr,
					 mm->buffer_size,
					 mm->from_flags);
		}
	}
	/*dump exp_mm_list*/
	if (!list_empty(&box->exp_mm_list.mm_list)) {
		BUFPRINT("**exp mm list**\n");
		list_for_each(p, &box->exp_mm_list.mm_list) {
			if (buf && (size - tsize) < 256) {
				BUFPRINT("\n\t**NOT END**\n");
				break;
			}
			ent = list_entry(p, struct mm_list_expand, mm_list);
			if (ent->mm) {
				BUFPRINT("code mem[%d]:%p, addr=%p, size=%d,from=%d\n",
					 ent->index,
					 (void *)ent->mm,
					 (void *)ent->mm->phy_addr,
					 ent->mm->buffer_size,
					 ent->mm->from_flags);
			}
		}
	}
#undef BUFPRINT
	if (!buf) {
		pr_info("%s", sbuf);
		pbuf = sbuf;
	}

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

	if (!buf) {
		pbuf = sbuf;
		size = 512;
	}
#define BUFPRINT(args...) \
	do {\
		s = snprintf(pbuf, size - tsize, args);\
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
		BUFPRINT("box[%d]: %s, %splayer_id:%d, max_num:%d, size:%d, exp_num:%d\n",
				 i, box->name,
				 (box->mem_flags & CODEC_MM_FLAGS_TVP) ?
				 "TVP mode " : "",
				 box->channel_id,
				 box->max_mm_num,
				 box->total_size,
				 box->exp_num);
		if (buf) {
			s = decoder_bmmu_box_dump(box, pbuf, size - tsize);
			if (s > 0) {
				tsize += s;
				pbuf += s;
			}
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

struct decoder_bmmu_box *decoder_bmmu_box_find_box_by_name(char *name)
{
	struct decoder_bmmu_box_mgr *mgr = get_decoder_bmmu_box_mgr();
	struct list_head *p;
	struct decoder_bmmu_box *box;

	mutex_lock(&mgr->mutex);
	list_for_each(p, &mgr->box_list) {
		box = list_entry(p, struct decoder_bmmu_box, list);
		if (strcmp(box->name, name) == 0) {
			mutex_unlock(&mgr->mutex);
			return box;
		}
	}
	mutex_unlock(&mgr->mutex);
	return NULL;
}

static ssize_t
box_dump_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	char cmd[16];
	static char name[32];
	void* box = NULL;
	u32 idx = 0;

	sscanf(buf, "%s", cmd);
	if (strcmp(cmd, "alloc-box") == 0) {
		sscanf(buf, "%s %s %d", cmd, name, &idx);
		pr_info("alloc box[%s], max_idx = %d\n", name, idx);
		box = decoder_bmmu_box_alloc_box(name, 0, idx,
						CODEC_MM_FLAGS_CMA_CLEAR |CODEC_MM_FLAGS_FOR_VDECODER,
						0, 0);
		if (!box) {
			pr_err("%s - alloc bmmu box failed!!\n", __func__);
			return -1;
		} else {
			pr_info("box[%p] alloc success\n", box);
		}
	} else if (strcmp(cmd, "alloc-idx") == 0) {
		sscanf(buf, "%s %s %d", cmd, name, &idx);
		pr_info("alloc from box[%s], idx = %d\n", name, idx);
		box = decoder_bmmu_box_find_box_by_name(name);
		if (!box) {
			pr_err("box[%s] not found!\n", name);
			return -1;
		}
		decoder_bmmu_box_alloc_idx(box, idx, 64, -1, -1);
		pr_info("alloc index succeed\n");
	} else if (strcmp(cmd, "free-box") == 0) {
		sscanf(buf, "%s %s", cmd, name);
		pr_info("free box[%s]\n", name);
		box = decoder_bmmu_box_find_box_by_name(name);
		if (!box) {
			pr_err("box[%s] not found!\n", name);
			return -1;
		}
		decoder_bmmu_box_free(box);
		pr_info("free box succeed\n");
	} else if (strcmp(cmd, "free-idx") == 0) {
		sscanf(buf, "%s %s %d", cmd, name, &idx);
		pr_info("free box[%s], idx = %d\n", name, idx);
		box = decoder_bmmu_box_find_box_by_name(name);
		if (!box) {
			pr_err("box[%s] not found!\n", name);
			return -1;
		}
		decoder_bmmu_box_free_idx(box, idx);
		pr_info("free index succeed\n");
	} else {
		pr_err("input error! %s\n", cmd);
	}

	return size;
}

static ssize_t debug_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	ssize_t size = 0;
	size += sprintf(buf, "box debug help:\n");
	size += sprintf(buf + size, "echo n > debug\n");
	size += sprintf(buf + size, "n==0: clear all debugs)\n");
	size += sprintf(buf + size,
	"n=1: dump all box\n");

	return size;
}

static ssize_t debug_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned val;
	ssize_t ret;
	val = -1;
	ret = sscanf(buf, "%d", &val);
	if (ret != 1)
		return -EINVAL;
	switch (val) {
	case 1:
		decoder_bmmu_box_dump_all(NULL , 0);
		break;
	default:
		pr_err("unknow cmd! %d\n", val);
	}
	return size;

}

static CLASS_ATTR_RW(box_dump);
static CLASS_ATTR_RW(debug);

static struct attribute *decoder_bmmu_box_class_attrs[] = {
	&class_attr_box_dump.attr,
	&class_attr_debug.attr,
	NULL
};

ATTRIBUTE_GROUPS(decoder_bmmu_box_class);

static struct class decoder_bmmu_box_class = {
	.name = "decoder_bmmu_box",
	.class_groups = decoder_bmmu_box_class_groups,
};

int decoder_bmmu_box_init(void)
{
	int r;

	memset(&global_blk_mgr, 0, sizeof(global_blk_mgr));
	INIT_LIST_HEAD(&global_blk_mgr.box_list);
	mutex_init(&global_blk_mgr.mutex);
	r = class_register(&decoder_bmmu_box_class);
	return r;
}
EXPORT_SYMBOL(decoder_bmmu_box_init);

void decoder_bmmu_box_exit(void)
{
	class_unregister(&decoder_bmmu_box_class);
	pr_info("dec bmmu box exit.\n");
}

#if 0
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
#endif
