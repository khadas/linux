// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/amlogic/media/codec_mm/codec_mm_scatter.h>
#include <linux/platform_device.h>

#define PR_ERR(fmt, args ...)		pr_err("dim:err:" fmt, ##args)

struct di_mmu_box {//decoder_mmu_box
	int max_sc_num;
	const char *name;
	int channel_id;
	int tvp_mode;
	int box_ref_cnt;
	struct mutex mutex;	/* for box */
	struct list_head list;
	struct codec_mm_scatter *sc_list[1];
};

#define MAX_KEEP_FRAME 4
#define START_KEEP_ID 0x9
#define MAX_KEEP_ID    (INT_MAX - 1)
struct di_mmu_box_mgr {//decoder_mmu_box_mgr
	int num;
	struct mutex mutex; /* for mgr */
	struct codec_mm_scatter *keep_sc[MAX_KEEP_FRAME];
	int	keep_id[MAX_KEEP_FRAME];
	int next_id;/*id for keep & free.*/
	struct list_head box_list;
};

static struct di_mmu_box_mgr global_mgr;
static struct di_mmu_box_mgr *get_decoder_mmu_box_mgr(void)
{
	return &global_mgr;
}

//decoder_mmu_box_mgr_add_box
static int di_mmu_box_mgr_add_box(struct di_mmu_box *box)
{
	struct di_mmu_box_mgr *mgr = get_decoder_mmu_box_mgr();

	mutex_lock(&mgr->mutex);
	list_add_tail(&box->list, &mgr->box_list);
	mutex_unlock(&mgr->mutex);
	return 0;
}

//decoder_mmu_box_mgr_del_box
static int di_mmu_box_mgr_del_box(struct di_mmu_box *box)
{
	struct di_mmu_box_mgr *mgr = get_decoder_mmu_box_mgr();

	mutex_lock(&mgr->mutex);
	list_del(&box->list);
	mutex_unlock(&mgr->mutex);
	return 0;
}

//ary no use now: decoder_xxx, and add 'e' after valid
bool di_mmu_box_valid_check(void *box)
{
	struct di_mmu_box_mgr *mgr = get_decoder_mmu_box_mgr();
	struct di_mmu_box *mmu_box = NULL;
	bool is_valid = false;

	mutex_lock(&mgr->mutex);
	list_for_each_entry(mmu_box, &mgr->box_list, list) {
		if (mmu_box && mmu_box == box) {
			is_valid = true;
			break;
		}
	}
	mutex_unlock(&mgr->mutex);

	return is_valid;
}

//ary EXPORT_SYMBOL(decoder_mmu_box_valid_check);

//decoder_mmu_try_to_release_box
//ary no use now
void di_mmu_try_to_release_box(void *handle)
{
	struct di_mmu_box *box = handle;
	bool is_keep = false;
	int i;

	if (!box || box->box_ref_cnt)
		return;

	mutex_lock(&box->mutex);
	for (i = 0; i < box->max_sc_num; i++) {
		if (box->sc_list[i]) {
			is_keep = true;
			break;
		}
	}
	mutex_unlock(&box->mutex);

	if (!is_keep) {
		di_mmu_box_mgr_del_box(box);
			codec_mm_scatter_mgt_delay_free_switch
				(0, 0, 0, box->tvp_mode);
		kfree(box);
	}
}

//ary EXPORT_SYMBOL(decoder_mmu_try_to_release_box);
//decoder_mmu_box_sc_check
int di_mmu_box_sc_check(void *handle, int is_tvp)
{
	struct di_mmu_box *box = handle;

	if (!box) {
		PR_ERR("mmu box NULL !!!\n");
		return 0;
	}
	return codec_mm_scatter_size(is_tvp);
}

//EXPORT_SYMBOL(decoder_mmu_box_sc_check);

//decoder_mmu_box_alloc_box
void *di_mmu_box_alloc_box(const char *name,
	int channel_id,
	int max_num,
	int min_size_M,
	int mem_flags)
/*min_size_M:wait alloc this size*/
{
	struct di_mmu_box *box;
	int size;

	//PR_INF("di_mmu_box_alloc_box, mem_flags = 0x%x\n", mem_flags);

	size = sizeof(struct di_mmu_box) +
			sizeof(struct codec_mm_scatter *) *
			max_num;
	box = kmalloc(size, GFP_KERNEL);
	if (!box) {
		PR_ERR("can't alloc decoder buffers box!!!\n");
		return NULL;
	}
	memset(box, 0, size);
	box->max_sc_num = max_num;
	box->name = name;
	box->channel_id = channel_id;
	box->tvp_mode = mem_flags;

	mutex_init(&box->mutex);
	INIT_LIST_HEAD(&box->list);
	di_mmu_box_mgr_add_box(box);
	codec_mm_scatter_mgt_delay_free_switch(1, 2000,
					       min_size_M, box->tvp_mode);
	return (void *)box;
}

//EXPORT_SYMBOL(decoder_mmu_box_alloc_box);

//decoder_mmu_box_alloc_idx
int di_mmu_box_alloc_idx(void *handle, int idx, int num_pages,
			 unsigned int *mmu_index_adr)
{
	struct di_mmu_box *box = handle;
	struct codec_mm_scatter *sc;
	int ret;
	int i;

	if (!box || idx < 0 || idx >= box->max_sc_num) {
		PR_ERR("can't alloc mmu box(%p),idx:%d\n",
			box, idx);
		return -1;
	}
	mutex_lock(&box->mutex);
	sc = box->sc_list[idx];
	if (sc) {
		if (sc->page_max_cnt >= num_pages) {
			ret = codec_mm_scatter_alloc_want_pages(sc,
				num_pages);
		} else {
			box->box_ref_cnt--;
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
			PR_ERR("alloc mmu failed, need pages=%d\n",
				num_pages);
			return -1;
		}
		box->sc_list[idx] = sc;
		box->box_ref_cnt++;
	}

	for (i = 0; i < num_pages; i++)
		mmu_index_adr[i] = PAGE_INDEX(sc->pages_list[i]);

	mutex_unlock(&box->mutex);

	return 0;
}

//EXPORT_SYMBOL(decoder_mmu_box_alloc_idx);

//decoder_mmu_box_free_idx_tail
int di_mmu_box_free_idx_tail(void *handle, int idx, int start_release_index)
{
	struct di_mmu_box *box = handle;
	struct codec_mm_scatter *sc;

	if (!box || idx < 0 || idx >= box->max_sc_num) {
		PR_ERR("can't free tail mmu box(%p),idx:%d in (%d-%d)\n",
			box, idx, 0,
			box ? (box->max_sc_num - 1) : 0);
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

//EXPORT_SYMBOL(decoder_mmu_box_free_idx_tail);

//decoder_mmu_box_free_idx
int di_mmu_box_free_idx(void *handle, int idx)
{
	struct di_mmu_box *box = handle;
	struct codec_mm_scatter *sc;

	if (!box || idx < 0 || idx >= box->max_sc_num) {
		PR_ERR("can't free idx of box(%p),idx:%d  in (%d-%d)\n",
			box, idx, 0,
			box ? (box->max_sc_num - 1) : 0);
		return -1;
	}
	mutex_lock(&box->mutex);
	sc = box->sc_list[idx];
	if (sc && sc->page_cnt > 0) {
		codec_mm_scatter_dec_owner_user(sc, 0);
		box->sc_list[idx] = NULL;
		box->box_ref_cnt--;
	}
	mutex_unlock(&box->mutex);

	if (sc && box->box_ref_cnt == 0)
		codec_mm_scatter_mgt_delay_free_switch(0, 0, 0, box->tvp_mode);

	return 0;
}

//EXPORT_SYMBOL(decoder_mmu_box_free_idx);
//decoder_mmu_box_free
int di_mmu_box_free(void *handle)
{
	struct di_mmu_box *box = handle;
	struct codec_mm_scatter *sc;
	int i;

	if (!box) {
		PR_ERR("can't free box of NULL box!\n");
		return -1;
	}
	mutex_lock(&box->mutex);
	for (i = 0; i < box->max_sc_num; i++) {
		sc = box->sc_list[i];
		if (sc) {
			codec_mm_scatter_dec_owner_user(sc, 0);
			box->sc_list[i] = NULL;
		}
	}
	mutex_unlock(&box->mutex);
	di_mmu_box_mgr_del_box(box);
		codec_mm_scatter_mgt_delay_free_switch(0, 0, 0, box->tvp_mode);
	kfree(box);
	return 0;
}

//EXPORT_SYMBOL(decoder_mmu_box_free);

//decoder_mmu_box_get_mem_handle
void *di_mmu_box_get_mem_handle(void *box_handle, int idx)
{
	struct di_mmu_box *box = box_handle;

	if (!box || idx < 0 || idx >= box->max_sc_num)
		return NULL;
	return  box->sc_list[idx];
}

//EXPORT_SYMBOL(decoder_mmu_box_get_mem_handle);

//decoder_mmu_box_dump
static int di_mmu_box_dump(struct di_mmu_box *box,
				void *buf, int size)
{
	char *pbuf = buf;
	char sbuf[512];
	int tsize = 0;
	int s;
	int i;

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

//decoder_mmu_box_dump_all
int di_mmu_box_dump_all(void *buf, int size)
{
	struct di_mmu_box_mgr *mgr = get_decoder_mmu_box_mgr();
	char *pbuf = buf;
	char sbuf[512];
	int tsize = 0;
	int s;
	int i;
	struct list_head *head, *list;

	if (!pbuf) {
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
		struct di_mmu_box *box;

		box = list_entry(list, struct di_mmu_box,
							list);
		BUFPRINT("box[%d]: %s, %splayer_id:%d, max_num:%d\n",
			i,
			box->name,
			box->tvp_mode ? "TVP mode " : "",
			box->channel_id,
			box->max_sc_num);
		if (buf) {
			s += di_mmu_box_dump(box, pbuf, size - tsize);
			if (s > 0) {
				tsize += s;
				pbuf += s;
			}
		} else {
			pr_info("%s", sbuf);
			pbuf = sbuf;
			tsize += di_mmu_box_dump(box, NULL, 0);
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

#ifdef TMP_NO_USED //ary

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
	.class_attrs = decoder_mmu_box_class_attrs
};
#endif

//decoder_mmu_box_init

int di_mmu_box_init(void)
{
	int r = 0; //ary set 0

	memset(&global_mgr, 0, sizeof(global_mgr));
	INIT_LIST_HEAD(&global_mgr.box_list);
	mutex_init(&global_mgr.mutex);
	global_mgr.next_id = START_KEEP_ID;
	//ary r = class_register(&decoder_mmu_box_class);
	return r;
}

//EXPORT_SYMBOL(decoder_mmu_box_init);

//decoder_mmu_box_exit
void di_mmu_box_exit(void)
{
	// class_unregister(&decoder_mmu_box_class);
	pr_info("dec mmu box exit.\n");
}

#ifdef TMP_NO_USED
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
#endif
