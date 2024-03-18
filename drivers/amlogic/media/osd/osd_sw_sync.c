// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sync_file.h>

#include "osd_sw_sync.h"

#define MAX_NUM 64
static int timeline_num;
struct timeline_debug_info {
	char name[32];
	u32 create_num;
	u32 release_num;
};

static struct timeline_debug_info timeline_debug_s[MAX_NUM];
static const struct dma_fence_ops timeline_fence_ops;

void output_fence_info(void)
{
	int i;

	for (i = 0; (i < timeline_num) && (i < MAX_NUM); i++) {
		pr_info("fence info: name=%s, create_num=%d, release_num=%d\n",
			timeline_debug_s[i].name,
			timeline_debug_s[i].create_num,
			timeline_debug_s[i].release_num);
	}
}

static inline struct sync_pt *fence_to_sync_pt(struct dma_fence *fence)
{
	if (fence->ops != &timeline_fence_ops)
		return NULL;
	return container_of(fence, struct sync_pt, base);
}

/**
 * sync_timeline_create() - creates a sync object
 * @name:	sync_timeline name
 *
 * Creates a new sync_timeline. Returns the sync_timeline object or NULL in
 * case of error.
 */
static struct sync_timeline *sync_timeline_create(const char *name)
{
	struct sync_timeline *obj;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return NULL;

	kref_init(&obj->kref);
	obj->context = dma_fence_context_alloc(1);
	strlcpy(obj->name, name, sizeof(obj->name));
	INIT_LIST_HEAD(&obj->active_list_head);
	INIT_LIST_HEAD(&obj->pt_list);
	spin_lock_init(&obj->lock);
	if (timeline_num < MAX_NUM)
		strlcpy(timeline_debug_s[timeline_num].name,
			name, sizeof(obj->name));
	timeline_num++;
	return obj;
}

static void sync_timeline_free(struct kref *kref)
{
	struct sync_timeline *obj =
		container_of(kref, struct sync_timeline, kref);

	kfree(obj);
}

static void sync_timeline_get(struct sync_timeline *obj)
{
	kref_get(&obj->kref);
}

static void sync_timeline_put(struct sync_timeline *obj)
{
	kref_put(&obj->kref, sync_timeline_free);
}

static const char *timeline_fence_get_driver_name(struct dma_fence *fence)
{
	return "sw_sync";
}

static const char *timeline_fence_get_timeline_name(struct dma_fence *fence)
{
	struct sync_timeline *parent = dma_fence_parent(fence);

	return parent->name;
}

static void timeline_fence_release(struct dma_fence *fence)
{
	struct sync_pt *pt = fence_to_sync_pt(fence);
	struct sync_timeline *parent = dma_fence_parent(fence);
	unsigned long flags;
	int i;

	spin_lock_irqsave(fence->lock, flags);
	list_del(&pt->link);
	if (!list_empty(&pt->active_list))
		list_del(&pt->active_list);
	spin_unlock_irqrestore(fence->lock, flags);
	sync_timeline_put(parent);
	dma_fence_free(fence);
	for (i = 0; (i < timeline_num) && (i < MAX_NUM); i++) {
		if (!strcmp(timeline_debug_s[i].name, parent->name))
			timeline_debug_s[i].release_num++;
	}
}

static bool timeline_fence_signaled(struct dma_fence *fence)
{
	struct sync_timeline *parent = dma_fence_parent(fence);

	return !__dma_fence_is_later(fence->seqno, parent->value, fence->ops);
}

static bool timeline_fence_enable_signaling(struct dma_fence *fence)
{
	return true;
}

static void timeline_fence_value_str(struct dma_fence *fence,
				     char *str, int size)
{
	snprintf(str, size, "%lld", fence->seqno);
}

static void timeline_fence_timeline_value_str(struct dma_fence *fence,
					      char *str, int size)
{
	struct sync_timeline *parent = dma_fence_parent(fence);

	snprintf(str, size, "%d", parent->value);
}

static const struct dma_fence_ops timeline_fence_ops = {
	.get_driver_name = timeline_fence_get_driver_name,
	.get_timeline_name = timeline_fence_get_timeline_name,
	.enable_signaling = timeline_fence_enable_signaling,
	.signaled = timeline_fence_signaled,
	.release = timeline_fence_release,
	.fence_value_str = timeline_fence_value_str,
	.timeline_value_str = timeline_fence_timeline_value_str,
};

/**
 * sync_timeline_signal() - signal a status change on a sync_timeline
 * @obj:	sync_timeline to signal
 * @inc:	num to increment on timeline->value
 *
 * A sync implementation should call this any time one of it's fences
 * has signaled or has an error condition.
 */
static void sync_timeline_signal(struct sync_timeline *obj, unsigned int inc)
{
	struct sync_pt *pt, *next;
	unsigned long flags;

	spin_lock_irqsave(&obj->lock, flags);
	/*coverity[missing_lock] It's locked here*/
	obj->value += inc;
	list_for_each_entry_safe(pt, next, &obj->active_list_head,
				 active_list) {
		if (dma_fence_is_signaled_locked(&pt->base))
			list_del_init(&pt->active_list);
	}
	spin_unlock_irqrestore(&obj->lock, flags);
}

/**
 * sync_pt_create() - creates a sync pt
 * @parent:	fence's parent sync_timeline
 * @inc:	value of the fence
 *
 * Creates a new sync_pt as a child of @parent.  @size bytes will be
 * allocated allowing for implementation specific data to be kept after
 * the generic sync_timeline struct. Returns the sync_pt object or
 * NULL in case of error.
 */
static struct sync_pt *sync_pt_create(struct sync_timeline *obj,
				      unsigned int value)
{
	struct sync_pt *pt;
	unsigned long flags;

	pt = kzalloc(sizeof(*pt), GFP_KERNEL);
	if (!pt)
		return NULL;
	spin_lock_irqsave(&obj->lock, flags);
	sync_timeline_get(obj);
	dma_fence_init(&pt->base, &timeline_fence_ops, &obj->lock,
		       obj->context, value);
	list_add_tail(&pt->link, &obj->pt_list);
	INIT_LIST_HEAD(&pt->active_list);
	list_add_tail(&pt->active_list, &obj->active_list_head);
	spin_unlock_irqrestore(&obj->lock, flags);

	return pt;
}

void *aml_sync_create_timeline(const char *tname)
{
	struct sync_timeline *timeline;

	timeline = sync_timeline_create(tname);
	return (void *)timeline;
}

int aml_sync_create_fence(void *timeline, unsigned int value)
{
	struct sync_timeline *tl = (struct sync_timeline *)timeline;
	int fd;
	int err;
	struct sync_pt *pt;
	struct sync_file *sync_file;
	int i;

	if (!tl)
		return -EPERM;

	fd =  get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return -EBADF;

	pt = sync_pt_create(tl, value);
	if (!pt) {
		pr_info("Err: sync_pt_create failed\n");
		err = -ENOMEM;
		goto err;
	}

	sync_file = sync_file_create(&pt->base);
	dma_fence_put(&pt->base);
	if (!sync_file) {
		pr_info("Err: sync_file_create failed\n");
		err = -ENOMEM;
		goto err;
	}

	fd_install(fd, sync_file->file);
	for (i = 0; (i < timeline_num) && (i < MAX_NUM); i++) {
		if (!strcmp(timeline_debug_s[i].name, tl->name))
			timeline_debug_s[i].create_num++;
	}
	return fd;

err:
	put_unused_fd(fd);
	return err;
}

void aml_sync_inc_timeline(void *timeline, unsigned int value)
{
	struct sync_timeline *tl = (struct sync_timeline *)timeline;

	if (!tl)
		return;
	while (value > INT_MAX)  {
		sync_timeline_signal(tl, INT_MAX);
		value -= INT_MAX;
	}

	sync_timeline_signal(tl, value);
}

struct dma_fence *aml_sync_get_fence(int syncfile_fd)
{
	return sync_file_get_fence(syncfile_fd);
}

int aml_sync_wait_fence(struct dma_fence *fence, long timeout)
{
	long ret;

	ret = dma_fence_wait_timeout(fence, false, timeout);
	return ret;
}

void aml_sync_put_fence(struct dma_fence *fence)
{
	dma_fence_put(fence);
}

/**
 * dma_fence_get_status_locked - returns the status upon completion
 * @fence: the dma_fence to query
 *
 * Returns 0 if the fence has not yet been signaled, 1 if the fence has
 * been signaled without an error condition, or a negative error code
 * if the fence has been completed in err.
 */
int aml_sync_fence_status(struct dma_fence *fence)
{
	return dma_fence_get_status(fence);
}
