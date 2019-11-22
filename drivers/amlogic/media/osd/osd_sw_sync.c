/*
 * drivers/amlogic/media/osd/osd_sw_sync.c
 *
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
 */
#ifdef CONFIG_SYNC_FILE

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sync_file.h>

#include "osd_sw_sync.h"

static const struct fence_ops timeline_fence_ops;
static inline struct sync_pt *fence_to_sync_pt(struct fence *fence)
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
	obj->context = fence_context_alloc(1);
	strlcpy(obj->name, name, sizeof(obj->name));
	INIT_LIST_HEAD(&obj->active_list_head);
	INIT_LIST_HEAD(&obj->pt_list);
	spin_lock_init(&obj->lock);

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

static const char *timeline_fence_get_driver_name(struct fence *fence)
{
	return "sw_sync";
}

static const char *timeline_fence_get_timeline_name(struct fence *fence)
{
	struct sync_timeline *parent = fence_parent(fence);

	return parent->name;
}

static void timeline_fence_release(struct fence *fence)
{
	struct sync_pt *pt = fence_to_sync_pt(fence);
	struct sync_timeline *parent = fence_parent(fence);
	unsigned long flags;

	spin_lock_irqsave(fence->lock, flags);
	list_del(&pt->link);
	if (!list_empty(&pt->active_list))
		list_del(&pt->active_list);
	spin_unlock_irqrestore(fence->lock, flags);
	sync_timeline_put(parent);
	fence_free(fence);
}

static bool timeline_fence_signaled(struct fence *fence)
{
	struct sync_timeline *parent = fence_parent(fence);

	return !__fence_is_later(fence->seqno, parent->value);
}

static bool timeline_fence_enable_signaling(struct fence *fence)
{
	struct sync_pt *pt = container_of(fence, struct sync_pt, base);
	struct sync_timeline *parent = fence_parent(fence);

	if (timeline_fence_signaled(fence))
		return false;

	list_add_tail(&pt->active_list, &parent->active_list_head);
	return true;
}

static void timeline_fence_disable_signaling(struct fence *fence)
{
	struct sync_pt *pt = container_of(fence, struct sync_pt, base);

	list_del_init(&pt->active_list);
}

static void timeline_fence_value_str(struct fence *fence,
				    char *str, int size)
{
	snprintf(str, size, "%d", fence->seqno);
}

static void timeline_fence_timeline_value_str(struct fence *fence,
					     char *str, int size)
{
	struct sync_timeline *parent = fence_parent(fence);

	snprintf(str, size, "%d", parent->value);
}

static const struct fence_ops timeline_fence_ops = {
	.get_driver_name = timeline_fence_get_driver_name,
	.get_timeline_name = timeline_fence_get_timeline_name,
	.enable_signaling = timeline_fence_enable_signaling,
	.disable_signaling = timeline_fence_disable_signaling,
	.signaled = timeline_fence_signaled,
	.wait = fence_default_wait,
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
	obj->value += inc;
	list_for_each_entry_safe(pt, next, &obj->active_list_head,
				 active_list) {
		if (fence_is_signaled_locked(&pt->base))
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
	fence_init(&pt->base, &timeline_fence_ops, &obj->lock,
		   obj->context, value);
	list_add_tail(&pt->link, &obj->pt_list);
	INIT_LIST_HEAD(&pt->active_list);
	spin_unlock_irqrestore(&obj->lock, flags);

	return pt;
}

static void sync_pt_free(struct sync_timeline *obj,
				      struct sync_pt *pt)
{
	unsigned long flags;

	spin_lock_irqsave(&obj->lock, flags);
	list_del(&pt->link);
	sync_timeline_put(obj);
	spin_unlock_irqrestore(&obj->lock, flags);
	kfree(pt);
	pt = NULL;
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

	if (tl == NULL)
		return -EPERM;

	fd =  get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return -EBADF;

	pt = sync_pt_create(tl, value);
	if (!pt) {
		err = -ENOMEM;
		goto err;
	}

	sync_file = sync_file_create(&pt->base);
	fence_put(&pt->base);
	if (!sync_file) {
		err = -ENOMEM;
		goto err;
	}

	fd_install(fd, sync_file->file);
	return fd;

err:
	put_unused_fd(fd);
	if (pt)
		sync_pt_free(tl, pt);
	return err;
}

void aml_sync_inc_timeline(void *timeline, unsigned int value)
{
	struct sync_timeline *tl = (struct sync_timeline *)timeline;

	if (tl == NULL)
		return;
	while (value > INT_MAX)  {
		sync_timeline_signal(tl, INT_MAX);
		value -= INT_MAX;
	}

	sync_timeline_signal(tl, value);
}

struct fence *aml_sync_get_fence(int syncfile_fd)
{
	return sync_file_get_fence(syncfile_fd);
}

int aml_sync_wait_fence(struct fence *fence, long timeout)
{
	long ret;

	ret = fence_wait_timeout(fence, false, timeout);
	return ret;
}

void aml_sync_put_fence(struct fence *fence)
{
	fence_put(fence);
}
#endif
