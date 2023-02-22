// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vdin_mmu_box.h"

static struct vdin_mmu_box_mgr global_mgr;

static struct vdin_mmu_box_mgr *get_vdin_mmu_box_mgr(void)
{
	return &global_mgr;
}

static int vdin_mmu_box_mgr_add_box(struct vdin_mmu_box *box)
{
	struct vdin_mmu_box_mgr *mgr = get_vdin_mmu_box_mgr();

	mutex_lock(&mgr->mutex);
	list_add_tail(&box->list, &mgr->box_list);
	mutex_unlock(&mgr->mutex);
	return 0;
}

static int vdin_mmu_box_mgr_del_box(struct vdin_mmu_box *box)
{
	struct vdin_mmu_box_mgr *mgr = get_vdin_mmu_box_mgr();

	mutex_lock(&mgr->mutex);
	list_del(&box->list);
	mutex_unlock(&mgr->mutex);
	return 0;
}

int vdin_mmu_box_sc_check(void *handle, int is_tvp)
{
	struct vdin_mmu_box *box = handle;

	if (!box) {
		pr_err("mmu box NULL !!!\n");
		return 0;
	}
	return codec_mm_scatter_size(is_tvp);
}
EXPORT_SYMBOL(vdin_mmu_box_sc_check);

void *vdin_mmu_box_alloc_box(const char *name,
	int channel_id,
	int max_num,
	int min_size_M,
	int mem_flags)
/*min_size_M:wait alloc this size*/
{
	struct vdin_mmu_box *box;
	int size;

	size = sizeof(struct vdin_mmu_box) +
			sizeof(struct codec_mm_scatter *) *
			max_num;
	box = kzalloc(size, GFP_KERNEL);
	if (!box) {
		pr_err("can't alloc decoder buffers box!!!\n");
		return NULL;
	}
	box->max_sc_num = max_num;
	box->name = name;
	box->channel_id = channel_id;
	box->tvp_mode = mem_flags;
	box->exp_num = 0;
	box->exp_sc_list.sc = NULL;
	box->exp_sc_list.index = -1;
	INIT_LIST_HEAD(&box->exp_sc_list.sc_list);

	mutex_init(&box->mutex);
	INIT_LIST_HEAD(&box->list);
	vdin_mmu_box_mgr_add_box(box);
	codec_mm_scatter_mgt_delay_free_switch(1, 2000,
		min_size_M, box->tvp_mode);

	return (void *)box;
}
EXPORT_SYMBOL(vdin_mmu_box_alloc_box);

struct codec_mm_scatter *vdin_mmu_box_get_sc_from_idx(struct vdin_mmu_box *box,
	int idx)
{
	struct list_head *p;
	struct sc_list_expand *ent;

	if (!box || idx < 0) {
		pr_err("can't get scatter from (%p),idx:%d\n",
			box, idx);
		return NULL;
	}
	if (likely(idx < box->max_sc_num))
		return box->sc_list[idx];

	list_for_each(p, &box->exp_sc_list.sc_list) {
		ent = list_entry(p, struct sc_list_expand, sc_list);
		if (ent->index == idx)
			return ent->sc;
	}
	return NULL;
}

void vdin_mmu_box_set_sc_from_idx(struct vdin_mmu_box *box,
	int idx, struct codec_mm_scatter *sc)
{
	struct list_head *p;
	struct sc_list_expand *ent, *exp;

	if (!box || idx < 0) {
		pr_err("can't get scatter from (%p),idx:%d\n",
			box, idx);
		return;
	}
	if (likely(idx < box->max_sc_num)) {
		box->sc_list[idx] = sc;
	} else {
		/*if idx can be found, set sc and return*/
		list_for_each(p, &box->exp_sc_list.sc_list) {
			ent = list_entry(p, struct sc_list_expand, sc_list);
			if (ent->index == idx) {
				ent->sc = sc;
				return;
			}
		}
		/*idx is not exist, creat and add it into sc list*/
		exp = kzalloc(sizeof(*exp), GFP_KERNEL);
		if (!exp) {
			//pr_err("creat exp_list failed! box name:%s, idx=%d\n", box->name, idx);
			return;
		}
		box->exp_num++;
		exp->index = idx;
		exp->sc = sc;
		list_add(&exp->sc_list, &box->exp_sc_list.sc_list);
		pr_debug("add new node to expand list, idx = %d, exp_num=%d.\n",
			idx, box->exp_num);
		return;
	}
}

int vdin_mmu_box_alloc_idx(void *handle, int idx, int num_pages,
	unsigned int *mmu_index_adr)
{
	struct vdin_mmu_box *box = handle;
	struct codec_mm_scatter *sc;
	int ret;
	int i;

	if (!box || idx < 0) {
		pr_err("can't alloc mmu box(%p),idx:%d\n",
			box, idx);
		return -1;
	}
	mutex_lock(&box->mutex);
	sc = vdin_mmu_box_get_sc_from_idx(box, idx);
	if (sc) {
		if (sc->page_max_cnt >= num_pages) {
			ret = codec_mm_scatter_alloc_want_pages(sc,
				num_pages);
		} else {
			codec_mm_scatter_dec_owner_user(sc, 0);
			vdin_mmu_box_set_sc_from_idx(box, idx, NULL);
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
		vdin_mmu_box_set_sc_from_idx(box, idx, sc);
	}

	for (i = 0; i < num_pages; i++)
		mmu_index_adr[i] = PAGE_INDEX(sc->pages_list[i]);

	mutex_unlock(&box->mutex);

	return 0;
}
EXPORT_SYMBOL(vdin_mmu_box_alloc_idx);

int vdin_mmu_box_free_idx_tail(void *handle, int idx,
	int start_release_index)
{
	struct vdin_mmu_box *box = handle;
	struct codec_mm_scatter *sc;

	if (!box || idx < 0) {
		pr_err("can't free tail mmu box(%p),idx:%d in (%d-%d)\n",
			box, idx, 0,
			box ? (box->max_sc_num - 1) : 0);
		return -1;
	}
	mutex_lock(&box->mutex);
	sc = vdin_mmu_box_get_sc_from_idx(box, idx);
	if (sc && start_release_index < sc->page_cnt)
		codec_mm_scatter_free_tail_pages_fast(sc,
				start_release_index);
	mutex_unlock(&box->mutex);
	return 0;
}
EXPORT_SYMBOL(vdin_mmu_box_free_idx_tail);

int vdin_mmu_box_free_idx(void *handle, int idx)
{
	struct vdin_mmu_box *box = handle;
	struct codec_mm_scatter *sc;

	if (!box || idx < 0) {
		pr_err("can't free idx of box(%p),idx:%d  in (%d-%d)\n",
			box, idx, 0,
			box ? (box->max_sc_num - 1) : 0);
		return -1;
	}

	mutex_lock(&box->mutex);
	sc = vdin_mmu_box_get_sc_from_idx(box, idx);
	if (sc && sc->page_cnt > 0) {
		codec_mm_scatter_dec_owner_user(sc, 0);
		vdin_mmu_box_set_sc_from_idx(box, idx, NULL);
	}

	mutex_unlock(&box->mutex);

	return 0;
}
EXPORT_SYMBOL(vdin_mmu_box_free_idx);

int vdin_mmu_box_free(void *handle)
{
	struct vdin_mmu_box *box = handle;
	struct codec_mm_scatter *sc;
	int i;
	struct sc_list_expand *p;
	struct sc_list_expand *next;

	if (!box) {
		pr_err("can't free box of NULL box!\n");
		return -1;
	}

	mutex_lock(&box->mutex);
	for (i = 0; i < box->max_sc_num; i++) {
		sc = vdin_mmu_box_get_sc_from_idx(box, i);
		if (sc) {
			codec_mm_scatter_dec_owner_user(sc, 0);
			vdin_mmu_box_set_sc_from_idx(box, i, NULL);
		}
	}
	list_for_each_entry_safe(p, next, &box->exp_sc_list.sc_list, sc_list) {
		if (p->sc) {
			codec_mm_scatter_dec_owner_user(p->sc, 0);
			vdin_mmu_box_set_sc_from_idx(box, p->index, NULL);
		}
		list_del(&p->sc_list);
		kfree(p);
		box->exp_num--;
	}

	codec_mm_scatter_mgt_delay_free_switch(0, 0, 0, box->tvp_mode);

	mutex_unlock(&box->mutex);

	vdin_mmu_box_mgr_del_box(box);
	kfree(box);

	return 0;
}
EXPORT_SYMBOL(vdin_mmu_box_free);

void *vdin_mmu_box_get_mem_handle(void *box_handle, int idx)
{
	struct vdin_mmu_box *box = box_handle;

	if (!box || idx < 0)
		return NULL;
	return  vdin_mmu_box_get_sc_from_idx(box, idx);
}
EXPORT_SYMBOL(vdin_mmu_box_get_mem_handle);

static int vdin_mmu_box_dump(struct vdin_mmu_box *box,
				void *buf, int size)
{
	char *pbuf = buf;
	char sbuf[512];
	int tsize = 0;
	int s;
	int i;
	struct list_head *p;
	struct sc_list_expand *ent;

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

	BUFPRINT("**sc list**\n");
	for (i = 0; i < box->max_sc_num; i++) {
		struct codec_mm_scatter *sc = vdin_mmu_box_get_sc_from_idx(box, i);

		if (sc) {
			BUFPRINT("sc mem[%d]:%p, size=%d\n",
				i, sc,
				sc->page_cnt << PAGE_SHIFT);
		}
	}
	/*dump exp_mm_list*/
	if (!list_empty(&box->exp_sc_list.sc_list)) {
		BUFPRINT("**exp sc list**\n");
		list_for_each(p, &box->exp_sc_list.sc_list) {
			if (buf && (size - tsize) < 256) {
				BUFPRINT("\n\t**NOT END**\n");
				break;
			}
			ent = list_entry(p, struct sc_list_expand, sc_list);
			if (ent->sc) {
				BUFPRINT("sc mem[%d]:%p, size=%d\n",
					ent->index, ent->sc,
					ent->sc->page_cnt << PAGE_SHIFT);
			}
		}
	}
#undef BUFPRINT
	if (!buf)
		pr_info("%s", sbuf);

	return tsize;
}

static int vdin_mmu_box_dump_all(void *buf, int size)
{
	struct vdin_mmu_box_mgr *mgr = get_vdin_mmu_box_mgr();
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
		struct vdin_mmu_box *box;

		box = list_entry(list, struct vdin_mmu_box, list);
		BUFPRINT("box[%d]: %s, %splayer_id:%d, max_num:%d, exp_num:%d\n",
			i,
			box->name,
			box->tvp_mode ? "TVP mode " : "",
			box->channel_id,
			box->max_sc_num,
			box->exp_num);
		if (buf) {
			s += vdin_mmu_box_dump(box, pbuf, size - tsize);
			if (s > 0) {
				tsize += s;
				pbuf += s;
			}
		} else {
			pr_info("%s", sbuf);
			pbuf = sbuf;
			tsize += vdin_mmu_box_dump(box, NULL, 0);
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

	ret = vdin_mmu_box_dump_all(buf, PAGE_SIZE);
	return ret;
}

struct vdin_mmu_box *vdin_mmu_box_find_box_by_name(char *name)
{
	struct vdin_mmu_box_mgr *mgr = get_vdin_mmu_box_mgr();
	struct list_head *p;
	struct vdin_mmu_box *box;

	mutex_lock(&mgr->mutex);
	list_for_each(p, &mgr->box_list) {
		box = list_entry(p, struct vdin_mmu_box, list);
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
//	char cmd[16];
//	static char name[32];
//	void* box = NULL;
//	unsigned int* mmu_idx_adr;
//	u32 idx = 0;
//
//	sscanf(buf, "%s", cmd);
//	if (strcmp(cmd, "alloc-box") == 0) {
//		sscanf(buf, "%s %s %d", cmd, name, &idx);
//		pr_info("alloc box[%s], max_idx = %d\n", name, idx);
//		box = vdin_mmu_box_alloc_box(name, 0, idx, 0, 0);
//		if (!box) {
//			pr_err("%s - alloc mmu box failed!!\n", __func__);
//			return -1;
//		} else {
//			pr_info("box[%p] alloc success\n", box);
//		}
//	} else if (strcmp(cmd, "alloc-idx") == 0) {
//		sscanf(buf, "%s %s %d", cmd, name, &idx);
//		pr_info("alloc from box[%s], idx = %d\n", name, idx);
//		box = vdin_mmu_box_find_box_by_name(name);
//		if (!box) {
//			pr_err("box[%s] not found!\n", name);
//			return -1;
//		}
//		mmu_idx_adr = kmalloc(64 * sizeof(unsigned int*), GFP_KERNEL);
//		vdin_mmu_box_alloc_idx(box, idx, 64, mmu_idx_adr);
//		pr_info("alloc index succeed\n");
//	} else if (strcmp(cmd, "free-box") == 0) {
//		sscanf(buf, "%s %s", cmd, name);
//		pr_info("free box[%s]\n", name);
//		box = vdin_mmu_box_find_box_by_name(name);
//		if (!box) {
//			pr_err("box[%s] not found!\n", name);
//			return -1;
//		}
//		vdin_mmu_box_free(box);
//		pr_info("free box succeed\n");
//	} else if (strcmp(cmd, "free-idx") == 0) {
//		sscanf(buf, "%s %s %d", cmd, name, &idx);
//		pr_info("free box[%s], idx = %d\n", name, idx);
//		box = vdin_mmu_box_find_box_by_name(name);
//		if (!box) {
//			pr_err("box[%s] not found!\n", name);
//			return -1;
//		}
//		vdin_mmu_box_free_idx(box, idx);
//		pr_info("free index succeed\n");
//	} else {
//		pr_err("input error! %s\n", cmd);
//	}

	return size;
}

static CLASS_ATTR_RW(box_dump);

static struct attribute *vdin_mmu_box_class_attrs[] = {
	&class_attr_box_dump.attr,
	NULL
};

ATTRIBUTE_GROUPS(vdin_mmu_box_class);

static struct class vdin_mmu_box_class = {
	.name = "vdin_mmu_box",
	.class_groups = vdin_mmu_box_class_groups,
};

int vdin_mmu_box_init(void)
{
	int r = 0;

	memset(&global_mgr, 0, sizeof(global_mgr));
	INIT_LIST_HEAD(&global_mgr.box_list);
	mutex_init(&global_mgr.mutex);
	global_mgr.next_id = START_KEEP_ID;
	//r = class_register(&vdin_mmu_box_class);
	return r;
}
EXPORT_SYMBOL(vdin_mmu_box_init);

void vdin_mmu_box_exit(void)
{
	class_unregister(&vdin_mmu_box_class);
	pr_info("vdin mmu box exit.\n");
}
