/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _OSD_SW_SYNC_H
#define _OSD_SW_SYNC_H

#ifdef CONFIG_SYNC_FILE
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/dma-fence.h>

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
	spinlock_t		lock; /*spin lock */
};

static inline struct sync_timeline *dma_fence_parent(struct dma_fence *fence)
{
	return container_of(fence->lock, struct sync_timeline, lock);
}

struct sync_pt {
	struct dma_fence base;
	struct list_head link;
	struct list_head active_list;
};

void *aml_sync_create_timeline(const char *tname);
int aml_sync_create_fence(void *timeline, unsigned int value);
void aml_sync_inc_timeline(void *timeline, unsigned int value);
struct dma_fence *aml_sync_get_fence(int syncfile_fd);
int aml_sync_wait_fence(struct dma_fence *fence, long timeout);
void aml_sync_put_fence(struct dma_fence *fence);
void output_fence_info(void);
int aml_sync_fence_status(struct dma_fence *fence);
#endif

#endif /* _OSD_SW_SYNC_H */
