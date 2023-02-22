/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef VDIN_BUFFER_BOX
#define VDIN_BUFFER_BOX

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

struct sc_list_expand {
	int index;
	struct list_head sc_list;
	struct codec_mm_scatter *sc;
};

struct vdin_mmu_box {
	int max_sc_num;
	int exp_num;
	const char *name;
	int channel_id;
	int tvp_mode;
	struct mutex mutex; /* box mutex */
	struct list_head list;
	struct sc_list_expand exp_sc_list;
	struct codec_mm_scatter *sc_list[1];
};

#define MAX_KEEP_FRAME 4
#define START_KEEP_ID 0x9
#define MAX_KEEP_ID    (INT_MAX - 1)

struct vdin_mmu_box_mgr {
	int num;
	struct mutex mutex; /* box mgr mutex */
	struct codec_mm_scatter *keep_sc[MAX_KEEP_FRAME];
	int	keep_id[MAX_KEEP_FRAME];
	int next_id;/*id for keep & free.*/
	struct list_head box_list;
};

void *vdin_mmu_box_alloc_box(const char *name,
	int channel_id,
	int max_num,
	int min_size_M,
	int mem_flags);

int vdin_mmu_box_sc_check(void *handle, int is_tvp);

int vdin_mmu_box_alloc_idx(void *handle, int idx, int num_pages,
	unsigned int *mmu_index_adr);

int vdin_mmu_box_free_idx_tail(void *handle, int idx,
	int start_release_index);

int vdin_mmu_box_free_idx(void *handle, int idx);

int vdin_mmu_box_free(void *handle);

int vdin_mmu_box_move_keep_idx(void *box_handle,
	int keep_idx);
int vdin_mmu_box_free_keep(int keep_id);

int vdin_mmu_box_free_all_keep(void);

void *vdin_mmu_box_get_mem_handle(void *box_handle, int idx);

struct codec_mm_scatter *vdin_mmu_box_get_sc_from_idx(struct vdin_mmu_box *box,
	int idx);

int vdin_mmu_box_init(void);

void vdin_mmu_box_exit(void);

#endif
