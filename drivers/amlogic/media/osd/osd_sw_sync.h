/*
 * drivers/amlogic/media/osd/osd_sw_sync.h
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
#ifndef _OSD_SW_SYNC_H
#define _OSD_SW_SYNC_H

#ifdef CONFIG_SYNC_FILE

#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/fence.h>

#include <linux/sync_file.h>
#include <uapi/linux/sync_file.h>


struct sync_timeline {
	struct kref		kref;
	char			name[32];

	/* protected by lock */
	u64			context;
	int			value;

	struct list_head	active_list_head;
	struct list_head	pt_list;
	spinlock_t		lock;
};

static inline struct sync_timeline *fence_parent(struct fence *fence)
{
	return container_of(fence->lock, struct sync_timeline, lock);
}

struct sync_pt {
	struct fence base;
	struct list_head link;
	struct list_head active_list;
};

void *aml_sync_create_timeline(const char *tname);
int aml_sync_create_fence(void *timeline, unsigned int value);
void aml_sync_inc_timeline(void *timeline, unsigned int value);
struct fence *aml_sync_get_fence(int syncfile_fd);
int aml_sync_wait_fence(struct fence *fence, long timeout);
void aml_sync_put_fence(struct fence *fence);
#endif

#endif /* _OSD_SW_SYNC_H */
