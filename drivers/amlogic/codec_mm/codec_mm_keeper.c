/*
 * drivers/amlogic/codec_mm/codec_mm_keeper.c
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
#include <linux/list.h>
#include <linux/genalloc.h>
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/amlogic/codec_mm/codec_mm_scatter.h>
#include <linux/amlogic/codec_mm/codec_mm_keeper.h>

#include <linux/workqueue.h>
#include "codec_mm_priv.h"
#include "codec_mm_scatter_priv.h"

#define MAX_KEEP_FRAME 4
#define START_KEEP_ID 0x9
#define MAX_KEEP_ID    (INT_MAX - 1)

struct keep_mem_info {
	void *handle;
	int keep_id;
	int type;
	int user;
};

struct codec_mm_keeper_mgr {
	int num;
	spinlock_t lock;
	struct delayed_work dealy_work;
	int work_runs;
	struct keep_mem_info keep_list[MAX_KEEP_FRAME];
	int next_id;		/*id for keep & free. */
};

static struct codec_mm_keeper_mgr codec_keeper_mgr_private;
static struct codec_mm_keeper_mgr *get_codec_mm_keeper_mgr(void)
{
	return &codec_keeper_mgr_private;
}

/*
not call in interrupt;
*/
int codec_mm_keeper_mask_keep_mem(void *mem_handle, int type)
{
	struct codec_mm_keeper_mgr *mgr = get_codec_mm_keeper_mgr();
	int keep_id = -1;
	int i;
	unsigned long flags;
	int ret;
	int slot_i = -1;
	int have_samed = 0;
	if (!mem_handle) {
		pr_err("NULL mem_handle for keeper!!\n");
		return -2;
	}
	if (type != MEM_TYPE_CODEC_MM_SCATTER) {
		pr_err("not valid type for keeper!!,%d\n", type);
		return -3;
	}
	ret = codec_mm_scatter_inc_for_keeper(mem_handle);
	if (ret < 0) {
		pr_err("keeper scatter failed,%d\n", ret);
		return -4;
	}
	spin_lock_irqsave(&mgr->lock, flags);
	keep_id = mgr->next_id++;
	if (mgr->next_id >= MAX_KEEP_ID)
		mgr->next_id = START_KEEP_ID;
	for (i = 0; i < MAX_KEEP_FRAME; i++) {
		if (!mgr->keep_list[i].handle && slot_i < 0)
			slot_i = i;
		else if (mgr->keep_list[i].handle == mem_handle) {
			have_samed = 1;
			keep_id = mgr->keep_list[i].keep_id;
			break;
		}
	}
	if (slot_i >= 0 && !have_samed) {
		mgr->keep_list[slot_i].handle = mem_handle;
		mgr->keep_list[slot_i].keep_id = keep_id;
		mgr->keep_list[slot_i].type = type;
		mgr->keep_list[slot_i].user = 1;
	} else {
		if (!have_samed)
			keep_id = -1;
	}
	if (keep_id < 0 || have_samed) {
		ret = codec_mm_scatter_dec_keeper_user(mem_handle, 0);
		if (keep_id < 0)
			pr_err("keep mem failed because keep buffer fulled!!!\n");
	}
	spin_unlock_irqrestore(&mgr->lock, flags);
	return keep_id;
}

/*
can call in irq
*/
int codec_mm_keeper_unmask_keeper(int keep_id)
{
	struct codec_mm_keeper_mgr *mgr = get_codec_mm_keeper_mgr();
	int i;
	unsigned long flags;
	if (keep_id < START_KEEP_ID || keep_id >= MAX_KEEP_ID) {
		pr_err("invalid keepid %d\n", keep_id);
		return -1;
	}
	spin_lock_irqsave(&mgr->lock, flags);
	for (i = 0; i < MAX_KEEP_FRAME; i++) {
		if (keep_id == mgr->keep_list[i].keep_id) {
			mgr->keep_list[i].user--;	/*mask can free. */
			break;
		}
	}
	spin_unlock_irqrestore(&mgr->lock, flags);
	schedule_delayed_work(&mgr->dealy_work, 100);	/*do free later, */
	return 0;
}

static int codec_mm_keeper_free_keep(int keep_id)
{
	struct codec_mm_keeper_mgr *mgr = get_codec_mm_keeper_mgr();
	void *mem_handle = NULL;
	int type;
	int i;
	unsigned long flags;
	if (keep_id < START_KEEP_ID || keep_id > MAX_KEEP_ID) {
		pr_err("invalid keepid %d\n", keep_id);
		return -1;
	}
	spin_lock_irqsave(&mgr->lock, flags);
	for (i = 0; i < MAX_KEEP_FRAME; i++) {
		if (keep_id == mgr->keep_list[i].keep_id) {
			mem_handle = mgr->keep_list[i].handle;
			type = mgr->keep_list[i].type;
			mgr->keep_list[i].handle = NULL;
			break;
		}
	}
	spin_unlock_irqrestore(&mgr->lock, flags);
	if (!mem_handle)
		return -1;
	if (type == MEM_TYPE_CODEC_MM)
		codec_mm_free_for_dma("mem_keeper", (unsigned long)mem_handle);
	else if (type == MEM_TYPE_CODEC_MM_SCATTER) {
		struct codec_mm_scatter *sc = mem_handle;
		codec_mm_scatter_dec_keeper_user(sc, 0);
	}

	return 0;
}

/*
can't call it
in irq/timer.tasklet
*/
int codec_mm_keeper_free_all_keep(int force)
{
	struct codec_mm_keeper_mgr *mgr = get_codec_mm_keeper_mgr();
	int i;
	for (i = 0; i < MAX_KEEP_FRAME; i++) {
		struct keep_mem_info *info = &mgr->keep_list[i];
		if (info->handle &&
			info->keep_id > 0 &&
			(force || info->user <= 0))
			codec_mm_keeper_free_keep(info->keep_id);
	}

	return 0;
}

int codec_mm_keeper_dump_info(void *buf, int size)
{
	struct codec_mm_keeper_mgr *mgr = get_codec_mm_keeper_mgr();
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
			pbuf += s;\
		} while (0)\

	BUFPRINT("dump keep list:next_id=%d,work_run:%d\n",
			mgr->next_id,
			mgr->work_runs);
	for (i = 0; i < MAX_KEEP_FRAME; i++) {
		BUFPRINT("keeper:[%d]:\t\tid:%d\thandle:%p,type:%d,user:%d\n",
			i,
			mgr->keep_list[i].keep_id,
			mgr->keep_list[i].handle,
			mgr->keep_list[i].type,
			mgr->keep_list[i].user);
	}
#undef BUFPRINT
	if (!buf)
		pr_info("%s", sbuf);
	return tsize;
}

static void codec_mm_keeper_monitor(struct work_struct *work)
{
	struct codec_mm_keeper_mgr *mgr = container_of(work,
					struct codec_mm_keeper_mgr,
					dealy_work.work);
	mgr->work_runs++;
	codec_mm_keeper_free_all_keep(0);
}

int codec_mm_keeper_mgr_init(void)
{
	struct codec_mm_keeper_mgr *mgr = get_codec_mm_keeper_mgr();
	memset(mgr, 0, sizeof(struct codec_mm_keeper_mgr));
	mgr->next_id = START_KEEP_ID;
	spin_lock_init(&mgr->lock);
	INIT_DELAYED_WORK(&mgr->dealy_work, codec_mm_keeper_monitor);
	return 0;
}
