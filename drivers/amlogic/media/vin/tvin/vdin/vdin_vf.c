// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_vf.c
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

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/seq_file.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vfm/vframe.h>

/* Local Headers */
#include "vdin_vf.h"
#include "vdin_ctl.h"
#include "vdin_mem_scatter.h"

static bool vf_log_enable = true;
static bool vf_log_fe = true;
static bool vf_log_be = true;

module_param(vf_log_enable, bool, 0664);
MODULE_PARM_DESC(vf_log_enable, "enable/disable vframe manager log");

module_param(vf_log_fe, bool, 0664);
MODULE_PARM_DESC(vf_log_fe, "enable/disable vframe manager log frontend");

module_param(vf_log_be, bool, 0664);
MODULE_PARM_DESC(vf_log_be, "enable/disable vframe manager log backend");

unsigned int vf_list_dbg;
module_param(vf_list_dbg, uint, 0664);
MODULE_PARM_DESC(vf_list_dbg, "vf list dbg");

unsigned int vf_move_print_cnt;
module_param(vf_move_print_cnt, uint, 0664);
MODULE_PARM_DESC(vf_move_print_cnt, "vf move print cnt");

#ifdef VF_LOG_EN
void vf_log_init(struct vf_pool *p)
{
	memset(&p->log, 0, sizeof(struct vf_log_s));
}
#else
void vf_log_init(struct vf_pool *p)
{
}
#endif

#ifdef VF_LOG_EN
static void vf_log(struct vf_pool *p, enum vf_operation_e operation,
		   bool operation_done)
{
	unsigned int i = 0;
	struct vf_log_s *log = &p->log;

	if (!vf_log_enable)
		return;

	if (!vf_log_fe)
		if (operation == VF_OPERATION_FPEEK ||
		    operation == VF_OPERATION_FGET ||
		    operation == VF_OPERATION_FPUT)
			return;

	if (!vf_log_be)
		if (operation == VF_OPERATION_BPEEK ||
		    operation == VF_OPERATION_BGET ||
		    operation == VF_OPERATION_BPUT)
			return;

	if (log->log_cur >= VF_LOG_LEN)
		return;

	ktime_get_ts(&log->log_time[log->log_cur]);
	for (i = 0; i < 11; i++)
		log->log_buf[log->log_cur][i] = 0x00;
	for (i = 0; i < p->size; i++) {
		switch (p->master[i].status) {
		case VF_STATUS_WL:
			log->log_buf[log->log_cur][0] |= 1 << i;
			break;
		case VF_STATUS_WM:
			log->log_buf[log->log_cur][1] |= 1 << i;
			break;
		case  VF_STATUS_RL:
			log->log_buf[log->log_cur][2] |= 1 << i;
			break;
		case  VF_STATUS_RM:
			log->log_buf[log->log_cur][3] |= 1 << i;
			break;
		case  VF_STATUS_WT:
			log->log_buf[log->log_cur][4] |= 1 << i;
			break;
		default:
			break;
		}

		switch (p->slave[i].status) {
		case VF_STATUS_WL:
			log->log_buf[log->log_cur][5] |= 1 << i;
			break;
		case VF_STATUS_WM:
			log->log_buf[log->log_cur][6] |= 1 << i;
			break;
		case  VF_STATUS_RL:
			log->log_buf[log->log_cur][7] |= 1 << i;
			break;
		case  VF_STATUS_RM:
			log->log_buf[log->log_cur][8] |= 1 << i;
			break;
		case  VF_STATUS_WT:
			log->log_buf[log->log_cur][9] |= 1 << i;
			break;
		default:
			break;
		}
	}
	log->log_buf[log->log_cur][10] = operation;
	if (!operation_done)
		log->log_buf[log->log_cur][10] |= 0x80;

	log->log_cur++;
}
#else
static void vf_log(struct vf_pool *p, enum vf_operation_e operation,
		   bool failure)
{
}
#endif

#ifdef VF_LOG_EN
void vf_log_print(struct vf_pool *p)
{
	unsigned int i = 0, j = 0, k = 0;
	int len = 0;
	char buf1[VF_LOG_PRINT_MAX_LEN];
	char buf2[VF_LOG_PRINT_MAX_LEN];
	struct vf_log_s *log = &p->log;

	pr_info("%-10s %-10s %-10s %-10s %-10s %-10s %5s\n",
		"WR_LIST", "RW_MODE", "RD_LIST", "RD_MODE", "WT_LIST",
		"OPERATION", "TIME");

	for (i = 0; i < log->log_cur; i++) {
		memset(buf1, 0, sizeof(buf1));
		len = 0;

		for (j = 0; j < 5; j++) {
			for (k = 0; k < 8; k++) {
				if (k == 4)
					len += sprintf(buf1 + len, "-");
				len += sprintf(buf1 + len, "%1d",
					(log->log_buf[i][j] >> (7 - k)) & 1);
			}
			len += sprintf(buf1 + len, "  ");
		}

		if (log->log_buf[i][10] & 0x80)
			len += sprintf(buf1 + len, "(X)");
		else
			len += sprintf(buf1 + len, "	 ");
		switch (log->log_buf[i][10] & 0x7) {
		case VF_OPERATION_INIT:
			len += sprintf(buf1 + len, "%-7s", "INIT");
			break;
		case VF_OPERATION_FPEEK:
			len += sprintf(buf1 + len, "%-7s", "FE_PEEK");
			break;
		case VF_OPERATION_FGET:
			len += sprintf(buf1 + len, "%-7s", "FE_GET");
			break;
		case VF_OPERATION_FPUT:
			len += sprintf(buf1 + len, "%-7s", "FE_PUT");
			break;
		case VF_OPERATION_BPEEK:
			len += sprintf(buf1 + len, "%-7s", "BE_PEEK");
			break;
		case VF_OPERATION_BGET:
			len += sprintf(buf1 + len, "%-7s", "BE_GET");
			break;
		case VF_OPERATION_BPUT:
			len += sprintf(buf1 + len, "%-7s", "BE_PUT");
			break;
		case VF_OPERATION_FREE:
			len += sprintf(buf1 + len, "%-7s", "FREE");
			break;
		default:
			break;
		}
		len += sprintf(buf1 + len, " %ld.%03ld",
				(long)log->log_time[i].tv_sec,
				(long)log->log_time[i].tv_nsec);

		memset(buf2, 0, sizeof(buf2));
		len = 0;
		for (j = 5; j < 10; j++) {
			for (k = 0; k < 8; k++) {
				if (k == 4)
					len += sprintf(buf2 + len, "-");
				len += sprintf(buf2 + len, "%1d",
					(log->log_buf[i][j] >> (7 - k)) & 1);
			}
			len += sprintf(buf2 + len, "  ");
		}
		pr_info("%s\n", buf1);
		pr_info("%s\n\n", buf2);
	}
}
#else
void vf_log_print(struct vf_pool *p)
{
}
#endif

#ifdef ISR_LOG_EN
void isr_log_init(struct vf_pool *p)
{
	memset(&p->isr_log, 0, sizeof(struct isr_log_s));
	p->isr_log.isr_log_en = 1;
}

void isr_log_print(struct vf_pool *p)
{
	unsigned int i = 0;
	struct isr_log_s *log = &p->isr_log;

	log->isr_log_en = 0;
	pr_info("%-7s\t%-7s\t%-7s\n",
			"ID", "I_TIME", "O_TIME");
	for (i = 0; i <= log->log_cur; i += 2) {
		pr_info("%u\t %7ld.%03ld\t%7ld.%03ld\n",
			i >> 1, (long)log->isr_time[i].tv_sec,
			(long)log->isr_time[i].tv_nsec,
			(long)log->isr_time[i + 1].tv_sec,
			(long)log->isr_time[i + 1].tv_nsec);
	}
}

void isr_log(struct vf_pool *p)
{
	struct isr_log_s *log = &p->isr_log;

	if (!log->isr_log_en)
		return;
	if (log->log_cur >= ISR_LOG_LEN)
		return;
	ktime_get_ts(&log->isr_time[log->log_cur]);
	log->log_cur++;
}

#else

void isr_log_init(struct vf_pool *p)
{
}

void isr_log_print(struct vf_pool *p)
{
}

void isr_log(struct vf_pool *p)
{
}

#endif

struct vf_entry *vf_get_master(struct vf_pool *p, int index)
{
	if (index >= p->max_size)
		return NULL;
	return &p->master[index];
}

struct vf_entry *vf_get_slave(struct vf_pool *p, int index)
{
	if (index >= p->max_size)
		return NULL;
	return &p->slave[index];
}

/* size : max canvas quantity */
struct vf_pool *vf_pool_alloc(int size)
{
	struct vf_pool *p;

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;

	p->master = kmalloc((sizeof(struct vf_entry) * size), GFP_KERNEL);
	if (!p->master) {
		kfree(p);
		return NULL;
	}
	p->slave  = kmalloc((sizeof(struct vf_entry) * size), GFP_KERNEL);
	if (!p->slave) {
		kfree(p->master);
		kfree(p);
		return NULL;
	}
	memset(p->master, 0, (sizeof(struct vf_entry) * size));
	memset(p->slave, 0, (sizeof(struct vf_entry) * size));
	p->max_size = size;
	/* initialize list lock */
	spin_lock_init(&p->wr_lock);
	spin_lock_init(&p->rd_lock);
	spin_lock_init(&p->wt_lock);
	spin_lock_init(&p->fz_lock);
	spin_lock_init(&p->tmp_lock);
	spin_lock_init(&p->log_lock);
	spin_lock_init(&p->dv_lock);
	/* initialize list head */
	INIT_LIST_HEAD(&p->wr_list);
	INIT_LIST_HEAD(&p->rd_list);
	INIT_LIST_HEAD(&p->wt_list);
	INIT_LIST_HEAD(&p->fz_list);
	INIT_LIST_HEAD(&p->tmp_list);
	return p;
}

/* size: valid canvas quantity*/
int vf_pool_init(struct vf_pool *p, int size)
{
	bool log_state = true;
	int i = 0;
	unsigned long flags;
	struct vf_entry *master = NULL, *slave = NULL;
	struct vf_entry *pos = NULL, *tmp = NULL;

	if (!p)
		return -1;
	if (size > p->max_size)
		return -1;
	p->size = size;

	/*clear pool flag*/
	p->pool_flag = 0;
	/* clear write list */
	spin_lock_irqsave(&p->wr_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->wr_list, list) {
		list_del(&pos->list);
	}
	spin_unlock_irqrestore(&p->wr_lock, flags);
	if (vf_list_dbg & VDIN_VF_DBG_EN)
		pr_info("--clear write list end--\n");

	/* clear read list */
	spin_lock_irqsave(&p->rd_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->rd_list, list) {
		list_del(&pos->list);
	}
	spin_unlock_irqrestore(&p->rd_lock, flags);

	spin_lock_irqsave(&p->wt_lock, flags);
	/* clear wait list */
	list_for_each_entry_safe(pos, tmp, &p->wt_list, list) {
		list_del(&pos->list);
	}
	spin_unlock_irqrestore(&p->wt_lock, flags);
	/* clear freeze list*/
	spin_lock_irqsave(&p->fz_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->fz_list, list) {
		list_del(&pos->list);
	}
	spin_unlock_irqrestore(&p->fz_lock, flags);
	/* clear tmp list*/
	spin_lock_irqsave(&p->tmp_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->tmp_list, list) {
		list_del(&pos->list);
	}
	spin_unlock_irqrestore(&p->tmp_lock, flags);
	p->wr_list_size = 0;
	p->rd_list_size = 0;
	p->wr_mode_size = 0;
	p->rd_mode_size = 0;
	p->fz_list_size = 0;
	p->tmp_list_size = 0;
	p->last_vfe = NULL;
	p->last_last_vfe = NULL;
	p->vf_move_prt_cnt = vf_move_print_cnt;
	/* initialize provider write list */
	for (i = 0; i < size; i++) {
		p->dv_buf_size[i] = 0;
		master = vf_get_master(p, i);
		if (!master) {
			log_state = false;
			break;
		}
		master->af_num = i;
		master->status = VF_STATUS_WL;
		master->sct_stat = VFRAME_SCT_STATE_INIT;
		master->flag |= VF_FLAG_NORMAL_FRAME;
		master->flag &= (~VF_FLAG_FREEZED_FRAME);
		spin_lock_irqsave(&p->wr_lock, flags);
		list_add(&master->list, &p->wr_list);
		p->wr_list_size++;
		spin_unlock_irqrestore(&p->wr_lock, flags);
	}

	if (vf_list_dbg & VDIN_VF_DBG_EN) {
		pr_info("init wr list:0x%x\n", p->wr_list_size);
		spin_lock_irqsave(&p->wr_lock, flags);
		list_for_each_entry_safe(pos, tmp, &p->wr_list, list) {
			pr_info("entry:0x%p vf:0x%p index:0x%x\n",
				pos, &pos->vf, pos->vf.index);
		}
		spin_unlock_irqrestore(&p->wr_lock, flags);
	}

	for (i = 0; i < size; i++) {
		slave = vf_get_slave(p, i);
		if (!slave) {
			log_state = false;
			break;
		}
		slave->status = VF_STATUS_SL;
	}
	atomic_set(&p->buffer_cnt, 0);
	for (i = 0; i < VFRAME_DISP_MAX_NUM; i++) {
		if (p->skip_vf_num == 0)
			p->disp_mode[i] = VFRAME_DISP_MODE_NULL;
		else
			p->disp_mode[i] = VFRAME_DISP_MODE_UNKNOWN;
		p->disp_index[i] = 0;
	}
#ifdef VF_LOG_EN
	spin_lock_irqsave(&p->log_lock, flags);
	vf_log_init(p);
	vf_log(p, VF_OPERATION_INIT, log_state);
	spin_unlock_irqrestore(&p->log_lock, flags);
#endif
	return 0;
}

/* free the vframe pool of the vfp */
void vf_pool_free(struct vf_pool *p)
{
	unsigned long flags;

	if (p) {
		spin_lock_irqsave(&p->log_lock, flags);
		vf_log(p, VF_OPERATION_FREE, true);
		spin_unlock_irqrestore(&p->log_lock, flags);
		/* if (p->master) */
		kfree(p->master);
		/* if (p->slave) */
		kfree(p->slave);
		kfree(p);
	}
}

/* return the last entry */
static inline struct vf_entry *vf_pool_peek(struct list_head *head)
{
	struct vf_entry *vfe;

	if (list_empty(head))
		return NULL;
	vfe = list_entry(head->prev, struct vf_entry, list);
	return vfe;
}

/* return and del the last entry*/
static inline struct vf_entry *vf_pool_get(struct list_head *head)
{
	struct vf_entry *vfe;

	if (list_empty(head))
		return NULL;
	vfe = list_entry(head->prev, struct vf_entry, list);
	list_del(&vfe->list);
	return vfe;
}

/* add entry in the list head */
static inline void vf_pool_put(struct vf_entry *vfe, struct list_head *head)
{
	list_add(&vfe->list, head);
}

/*
 * move all vf_entry in tmp list to writable list
 */
void recycle_tmp_vfs(struct vf_pool *p)
{
	struct vf_entry *pos = NULL, *tmp = NULL;
	unsigned long flags;

	spin_lock_irqsave(&p->tmp_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->tmp_list, list) {
		list_del(&pos->list);
		receiver_vf_put(&pos->vf, p);
		p->tmp_list_size--;
	}
	spin_unlock_irqrestore(&p->tmp_lock, flags);
}

/*
 * put vf_entry to tmp list
 */
void tmp_vf_put(struct vf_entry *vfe, struct vf_pool *p)
{
	unsigned long flags;

	spin_lock_irqsave(&p->tmp_lock, flags);
	vf_pool_put(vfe, &p->tmp_list);
	p->tmp_list_size++;
	spin_unlock_irqrestore(&p->tmp_lock, flags);
}

/*
 * move all vf_entry in tmp list to readable list
 */
void tmp_to_rd(struct vf_pool *p)
{
	struct vf_entry *pos = NULL, *tmp = NULL;
	unsigned long flags;

	spin_lock_irqsave(&p->tmp_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->tmp_list, list) {
		list_del(&pos->list);
		provider_vf_put(pos, p);
		p->tmp_list_size--;
	}
	spin_unlock_irqrestore(&p->tmp_lock, flags);
}

struct vf_entry *provider_vf_peek(struct vf_pool *p)
{
	struct vf_entry *vfe;
	unsigned long flags;

	spin_lock_irqsave(&p->wr_lock, flags);
	vfe = vf_pool_peek(&p->wr_list);
	spin_unlock_irqrestore(&p->wr_lock, flags);
	spin_lock_irqsave(&p->log_lock, flags);
	if (!vfe)
		vf_log(p, VF_OPERATION_FPEEK, false);
	else
		vf_log(p, VF_OPERATION_FPEEK, true);
	spin_unlock_irqrestore(&p->log_lock, flags);
	return vfe;
}

/* provider get last vframe to write */
struct vf_entry *provider_vf_get(struct vf_pool *p)
{
	struct vf_entry *vfe;
	unsigned long flags;

	spin_lock_irqsave(&p->wr_lock, flags);
	vfe = vf_pool_get(&p->wr_list);
	if (vfe) {
		if (vfe->status != VF_STATUS_WL) {
			if (vf_list_dbg & VDIN_VF_DBG_EN)
				pr_info("not WL entry:0x%p index:%x sta:%x\n",
					vfe, vfe->vf.index, vfe->status);
			spin_unlock_irqrestore(&p->wr_lock, flags);
			return NULL;
		}
		p->wr_list_size--;
		vfe->status = VF_STATUS_WM;
		p->wr_mode_size++;
		if (vf_list_dbg & VDIN_VF_MOVE_EN && p->vf_move_prt_cnt) {
			p->vf_move_prt_cnt--;
			pr_info("del_wr:entry:0x%p wr_size:%x index:%x sta:%x\n",
				vfe, p->wr_list_size, vfe->vf.index, vfe->status);
		}
	}
	spin_unlock_irqrestore(&p->wr_lock, flags);
	if (!vfe) {
		spin_lock_irqsave(&p->log_lock, flags);
		vf_log(p, VF_OPERATION_FGET, false);
		spin_unlock_irqrestore(&p->log_lock, flags);
		return NULL;
	}
	spin_lock_irqsave(&p->log_lock, flags);
	vf_log(p, VF_OPERATION_FGET, true);
	spin_unlock_irqrestore(&p->log_lock, flags);
	return vfe;
}

void provider_vf_put(struct vf_entry *vfe, struct vf_pool *p)
{
	unsigned long flags;

	spin_lock_irqsave(&p->rd_lock, flags);
	if (vfe->status != VF_STATUS_WM) {
		if (vf_list_dbg & VDIN_VF_DBG_EN)
			pr_info("not WM entry:%p index:%x sta:%x\n",
				vfe, vfe->vf.index, vfe->status);
		spin_unlock_irqrestore(&p->rd_lock, flags);
		return;
	}
	vfe->status = VF_STATUS_RL;
	vf_pool_put(vfe, &p->rd_list);
	p->rd_list_size++;
	p->wr_mode_size--;
	spin_unlock_irqrestore(&p->rd_lock, flags);
	spin_lock_irqsave(&p->log_lock, flags);
	vf_log(p, VF_OPERATION_FPUT, true);
	spin_unlock_irqrestore(&p->log_lock, flags);
}

/* receiver peek to read */
struct vf_entry *receiver_vf_peek(struct vf_pool *p)
{
	struct vf_entry *vfe;
	unsigned long flags;

	if (p->pool_flag & VDIN_VF_POOL_FREEZE) {
		spin_lock_irqsave(&p->fz_lock, flags);
		vfe = vf_pool_peek(&p->fz_list);
		spin_unlock_irqrestore(&p->fz_lock, flags);
		return vfe;
	}

	spin_lock_irqsave(&p->rd_lock, flags);
	vfe = vf_pool_peek(&p->rd_list);
	spin_unlock_irqrestore(&p->rd_lock, flags);
	spin_lock_irqsave(&p->log_lock, flags);
	if (!vfe)
		vf_log(p, VF_OPERATION_BPEEK, false);
	else
		vf_log(p, VF_OPERATION_BPEEK, true);
	spin_unlock_irqrestore(&p->log_lock, flags);
	if (!vfe)
		return NULL;
	return vfe;
}

/* receiver get vframe to read */
struct vf_entry *receiver_vf_get(struct vf_pool *p)
{
	struct vf_entry *vfe;
	unsigned long flags;

	/*get the vframe from the frozen list*/
	if (p->pool_flag & VDIN_VF_POOL_FREEZE) {
		spin_lock_irqsave(&p->fz_lock, flags);
		if (list_empty(&p->fz_list)) {
			spin_unlock_irqrestore(&p->fz_lock, flags);
			return NULL;
		}
		vfe = vf_pool_get(&p->fz_list);
		spin_unlock_irqrestore(&p->fz_lock, flags);
		return vfe;
	}

	spin_lock_irqsave(&p->rd_lock, flags);
	if (list_empty(&p->rd_list)) {
		spin_unlock_irqrestore(&p->rd_lock, flags);
		spin_lock_irqsave(&p->log_lock, flags);
		vf_log(p, VF_OPERATION_BGET, false);
		spin_unlock_irqrestore(&p->log_lock, flags);
		return NULL;
	}

	vfe = vf_pool_get(&p->rd_list);
	if (vfe->status != VF_STATUS_RL) {
		if (vf_list_dbg & VDIN_VF_DBG_EN)
			pr_info("not RL entry:0x%p index:%x sta:%x\n",
				vfe, vfe->vf.index, vfe->status);
		spin_unlock_irqrestore(&p->rd_lock, flags);
		return NULL;
	}
	p->rd_list_size--;
	vfe->status = VF_STATUS_RM;
	p->rd_mode_size++;
	spin_unlock_irqrestore(&p->rd_lock, flags);
	spin_lock_irqsave(&p->log_lock, flags);
	vf_log(p, VF_OPERATION_BGET, true);
	spin_unlock_irqrestore(&p->log_lock, flags);
	return vfe;
}

/*check vf point,0:normal;1:bad*/
static unsigned int check_vf_put(struct vframe_s *vf, struct vf_pool *p)
{
	struct vf_entry *master;
	unsigned int i;

	if (!vf || !p)
		return 1;
	for (i = 0; i < p->size; i++) {
		master = vf_get_master(p, i);
		if (&master->vf == vf)
			return 0;
	}
	pr_info("[%s]vf:%p!!!!\n", __func__, vf);
	return 1;
}

void receiver_vf_put(struct vframe_s *vf, struct vf_pool *p)
{
	struct vf_entry *master = NULL, *slave = NULL;
	unsigned long flags;
	struct vf_entry *pos = NULL, *tmp = NULL;
	int found_in_wt_list = 0;

	if (check_vf_put(vf, p))
		return;
	master = vf_get_master(p, vf->index);
	if (!master) {
		spin_lock_irqsave(&p->log_lock, flags);
		vf_log(p, VF_OPERATION_BPUT, false);
		spin_unlock_irqrestore(&p->log_lock, flags);
		return;
	}
	/*keep the frozen frame in rd list&recycle the
	 * frame which not in fz list when unfreeze
	 */
	if (master->flag & VF_FLAG_FREEZED_FRAME) {
		if (p->pool_flag & VDIN_VF_POOL_FREEZE) {
			spin_lock_irqsave(&p->fz_lock, flags);
			vf_pool_put(master, &p->fz_list);
			spin_unlock_irqrestore(&p->fz_lock, flags);
			return;
		}
		master->flag &= (~VF_FLAG_FREEZED_FRAME);
		pr_info("%s: del frame %u from fz list.\n",
			__func__, master->vf.index);
	}

	/* normal vframe */
	if (master->flag & VF_FLAG_NORMAL_FRAME) {
		if (vf_list_dbg & VDIN_VF_MOVE_EN && p->vf_move_prt_cnt) {
			p->vf_move_prt_cnt--;
			pr_info("add_wr:entry:0x%p wr_size:%x index:%x sta:%x\n",
				master, p->wr_list_size, vf->index, master->status);
		}

		spin_lock_irqsave(&p->wr_lock, flags);
		if (master->status == VF_STATUS_WL ||
		    master->status == VF_STATUS_RL) {
			if (vf_list_dbg & VDIN_VF_DBG_EN)
				pr_info("not WL and RL entry:0x%p index:%x sta:%x\n",
					master, vf->index, master->status);
			spin_unlock_irqrestore(&p->wr_lock, flags);
			return;
		}
		if (master->status == VF_STATUS_WM)
			p->wr_mode_size--;
		if (master->status == VF_STATUS_RM)
			p->rd_mode_size--;
		master->status = VF_STATUS_WL;
		vf_pool_put(master, &p->wr_list);
		p->wr_list_size++;
		spin_unlock_irqrestore(&p->wr_lock, flags);
		spin_lock_irqsave(&p->log_lock, flags);
		vf_log(p, VF_OPERATION_BPUT, true);
		spin_unlock_irqrestore(&p->log_lock, flags);
	} else {
		spin_lock_irqsave(&p->wt_lock, flags);
		list_for_each_entry_safe(pos, tmp, &p->wt_list, list) {
			/*
			 * if the index to be putted is same
			 * with one in wt_list, we consider that
			 * they are the same entry. so the same
			 * vframe hold by receiver should not be
			 *  putted more than one times.
			 */
			if (pos->vf.index == vf->index) {
				found_in_wt_list = 1;
				break;
			}
		}
		/*
		 * the entry 'pos' found in wt_list maybe
		 * entry 'master' or 'slave'
		 */
		slave = vf_get_slave(p, vf->index);
		if (!slave) {
			spin_unlock_irqrestore(&p->wt_lock, flags);
			spin_lock_irqsave(&p->log_lock, flags);
			vf_log(p, VF_OPERATION_BPUT, false);
			spin_unlock_irqrestore(&p->log_lock, flags);
			return;
		}
		/* if found associated entry in wait list */
		if (found_in_wt_list) {
			/* remove from wait list,
			 * and put the master entry in wr_list
			 */
			list_del(&pos->list);
			spin_unlock_irqrestore(&p->wt_lock, flags);

			master->status = VF_STATUS_WL;
			spin_lock_irqsave(&p->wr_lock, flags);
			vf_pool_put(master, &p->wr_list);
			p->wr_list_size++;
			spin_unlock_irqrestore(&p->wr_lock, flags);
			slave->status = VF_STATUS_SL;
			spin_lock_irqsave(&p->log_lock, flags);
			vf_log(p, VF_OPERATION_BPUT, true);
			spin_unlock_irqrestore(&p->log_lock, flags);
		} else {
		/* if not found associated entry in wait list */

			/*
			 * put the recycled vframe in wait list
			 *
			 * you should put the vframe that you got,
			 * you should not put the copy one.
			 * because we also compare the address of vframe
			 * structure to determine it is master or slave.
			 */
			if (&slave->vf == vf) {
				slave->status = VF_STATUS_WT;
				list_add(&slave->list, &p->wt_list);
			} else {
				master->status = VF_STATUS_WT;
				list_add(&master->list, &p->wt_list);
			}
			spin_unlock_irqrestore(&p->wt_lock, flags);
			spin_lock_irqsave(&p->log_lock, flags);
			vf_log(p, VF_OPERATION_BPUT, true);
			spin_unlock_irqrestore(&p->log_lock, flags);
		}
	}
	atomic_dec(&p->buffer_cnt);
}

struct vframe_s *vdin_vf_peek(void *op_arg)
{
	struct vf_pool *p;
	struct vf_entry *vfe;

	if (!op_arg)
		return NULL;
	p = (struct vf_pool *)op_arg;
	vfe =  receiver_vf_peek(p);
	if (!vfe)
		return NULL;
	return &vfe->vf;
}

struct vframe_s *vdin_vf_get(void *op_arg)
{
	struct vf_pool *p;
	struct vf_entry *vfe;

	if (!op_arg)
		return NULL;
	p = (struct vf_pool *)op_arg;
	vfe =  receiver_vf_get(p);
	if (!vfe)
		return NULL;
	atomic_inc(&p->buffer_cnt);
	return &vfe->vf;
}

void vdin_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vf_pool *p;

	if (!op_arg || !vf)
		return;
	p = (struct vf_pool *)op_arg;

	//vdin_vf_put can called from ISR,so below function only for debug!
	vdin_sct_free_wr_list_idx(p, vf);

	receiver_vf_put(vf, p);
	/*clean dv-buf-size*/
	if (vf && (dv_dbg_mask & DV_CLEAN_UP_MEM)) {
		p->dv_buf_size[vf->index] = 0;
		/*if (p->dv_buf_ori[vf->index])*/
		/*	memset(p->dv_buf_ori[vf->index], 0, dolby_size_byte);*/
	}
}

int vdin_vf_states(struct vframe_states *vf_ste, void *op_arg)
{
	struct vf_pool *p;

	if (!vf_ste)
		return -1;
	p = (struct vf_pool *)op_arg;
	vf_ste->vf_pool_size = p->size;
	vf_ste->buf_free_num = p->wr_list_size;
	vf_ste->buf_avail_num = p->rd_list_size;
	vf_ste->buf_recycle_num = p->size - p->wr_list_size - p->rd_list_size;
	return 0;
}

/*
 * hold the buffer from rd list,if rd list is not enough,
 * get buffer from wr list
 */
void vdin_vf_freeze(struct vf_pool *p, unsigned int num)
{
	struct vf_entry *vfe, *tmp;
	struct list_head *rd_head, *wr_head, *fz_head;
	unsigned long flags;

	rd_head  = &p->rd_list;
	wr_head = &p->wr_list;
	fz_head  = &p->fz_list;
	p->pool_flag |= VDIN_VF_POOL_FREEZE;
	if (p->fz_list_size < num) {
		/*add the buffer in rd list to fz list*/
		spin_lock_irqsave(&p->rd_lock, flags);
		list_for_each_entry_safe(vfe, tmp, rd_head, list) {
			list_del(&vfe->list);
			spin_lock_irqsave(&p->fz_lock, flags);
			list_add_tail(&vfe->list, fz_head);
			spin_unlock_irqrestore(&p->fz_lock, flags);
			vfe->flag |=  VF_FLAG_FREEZED_FRAME;
			pr_info("%s: add  %u frame from rd list.\n",
				__func__, vfe->vf.index);
			if (++p->fz_list_size >= num)
				break;
		}
		spin_unlock_irqrestore(&p->rd_lock, flags);
		if (p->fz_list_size < num) {
			spin_lock_irqsave(&p->wr_lock, flags);
			spin_lock_irqsave(&p->fz_lock, flags);
			list_for_each_entry_safe_reverse(vfe, tmp, wr_head,
							 list) {
				list_del(&vfe->list);
				list_add(&vfe->list, fz_head);
				vfe->flag |=  VF_FLAG_FREEZED_FRAME;
				pr_info("%s: add frame %u from wr list.\n",
					__func__, vfe->vf.index);
				if (++p->fz_list_size >= num)
					break;
			}
			spin_unlock_irqrestore(&p->fz_lock, flags);
			spin_unlock_irqrestore(&p->wr_lock, flags);
		}
	}
}

void vdin_vf_unfreeze(struct vf_pool *p)
{
	struct list_head *fz_head, *wr_head;
	struct vf_entry *vfe, *tmp;
	unsigned long flags;

	fz_head = &p->fz_list;
	wr_head = &p->wr_list;
	if (p->fz_list_size > 0) {
		p->pool_flag &= (~VDIN_VF_POOL_FREEZE);
		p->fz_list_size = 0;
		spin_lock_irqsave(&p->fz_lock, flags);
		spin_lock_irqsave(&p->wr_lock, flags);
		list_for_each_entry_safe(vfe, tmp, fz_head, list) {
			list_del(&vfe->list);
			list_add_tail(&vfe->list, wr_head);
			vfe->flag &=  (~VF_FLAG_FREEZED_FRAME);
			pr_info("%s: del frame %u from fz list.\n",
				__func__, vfe->vf.index);
		}
		spin_unlock_irqrestore(&p->wr_lock, flags);
		spin_unlock_irqrestore(&p->fz_lock, flags);
	}
}

/*dump vframe state*/
void vdin_dump_vf_state(struct vf_pool *p)
{
	unsigned long flags;
	struct vf_entry *pos = NULL, *tmp = NULL;

	pr_info("buffers in writable list:\n");
	spin_lock_irqsave(&p->wr_lock, flags);
	pr_info("wr list:0x%x\n", p->wr_list_size);
	list_for_each_entry_safe(pos, tmp, &p->wr_list, list) {
		pr_info("index: %2u,status %u, canvas index0: 0x%x,",
			pos->vf.index, pos->status, pos->vf.canvas0Addr);
		pr_info("\t canvas index1: 0x%x, vframe type: 0x%x.\n",
			pos->vf.canvas1Addr, pos->vf.type);
		pr_info("\t ratio_control(0x%x) signal_type(0x%x)\n",
			pos->vf.ratio_control, pos->vf.signal_type);
	}
	spin_unlock_irqrestore(&p->wr_lock, flags);

	pr_info("buffer in readable list:\n");
	spin_lock_irqsave(&p->rd_lock, flags);
	pr_info("rd list:0x%x\n", p->rd_list_size);
	list_for_each_entry_safe(pos, tmp, &p->rd_list, list) {
		pr_info("index: %u,status %u, canvas index0: 0x%x,",
			pos->vf.index, pos->status, pos->vf.canvas0Addr);
		pr_info("\t canvas index1: 0x%x, vframe type: 0x%x.\n",
			pos->vf.canvas1Addr, pos->vf.type);
		pr_info("\t ratio_control(0x%x) signal_type(0x%x)\n",
			pos->vf.ratio_control, pos->vf.signal_type);
	}
	pr_info("wr_mode_size:0x%x, rd_mode_size:0x%x\n",
		p->wr_mode_size, p->rd_mode_size);
	spin_unlock_irqrestore(&p->rd_lock, flags);

	pr_info("buffer in waiting list:\n");
	spin_lock_irqsave(&p->wt_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->wt_list, list) {
		pr_info("index: %u, status %u, canvas index0: 0x%x,",
			pos->vf.index, pos->status, pos->vf.canvas0Addr);
		pr_info("\t canvas index1: 0x%x, vframe type: 0x%x.\n",
			pos->vf.canvas1Addr, pos->vf.type);
		pr_info("\t ratio_control(0x%x).\n", pos->vf.ratio_control);
	}
	spin_unlock_irqrestore(&p->wt_lock, flags);
	pr_info("buffer in temp list:\n");
	spin_lock_irqsave(&p->tmp_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->tmp_list, list) {
		pr_info("index: %u, status %u, canvas index0: 0x%x,",
			pos->vf.index, pos->status, pos->vf.canvas0Addr);
		pr_info("\t canvas index1: 0x%x, vframe type: 0x%x.\n",
			pos->vf.canvas1Addr, pos->vf.type);
		pr_info("\t ratio_control(0x%x).\n", pos->vf.ratio_control);
	}
	spin_unlock_irqrestore(&p->tmp_lock, flags);
	pr_info("buffer get count %d.\n", atomic_read(&p->buffer_cnt));
}

/*2018-07-18 add debugfs*/
/*same as vdin_dump_vf_state*/
void vdin_dump_vf_state_seq(struct vf_pool *p, struct seq_file *seq)
{
	unsigned long flags;
	struct vf_entry *pos = NULL, *tmp = NULL;

	seq_puts(seq, "buffers in writable list:\n");
	spin_lock_irqsave(&p->wr_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->wr_list, list) {
		seq_printf(seq, "index: %2u,status %u, canvas index0: 0x%x,\n",
			   pos->vf.index, pos->status, pos->vf.canvas0Addr);
		seq_printf(seq, "\t canvas index1: 0x%x, vframe type: 0x%x.\n",
			   pos->vf.canvas1Addr, pos->vf.type);
		seq_printf(seq, "\t ratio_control(0x%x).\n",
			   pos->vf.ratio_control);
	}
	spin_unlock_irqrestore(&p->wr_lock, flags);

	seq_puts(seq, "buffer in readable list:\n");
	spin_lock_irqsave(&p->rd_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->rd_list, list) {
		seq_printf(seq, "index: %u,status %u, canvas index0: 0x%x,\n",
			   pos->vf.index, pos->status, pos->vf.canvas0Addr);
		seq_printf(seq, "\t canvas index1: 0x%x, vframe type: 0x%x.\n",
			   pos->vf.canvas1Addr, pos->vf.type);
		seq_printf(seq, "\t ratio_control(0x%x).\n",
			   pos->vf.ratio_control);
	}
	spin_unlock_irqrestore(&p->rd_lock, flags);

	seq_puts(seq, "buffer in waiting list:\n");
	spin_lock_irqsave(&p->wt_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->wt_list, list) {
		seq_printf(seq, "index: %u, status %u, canvas index0: 0x%x,\n",
			   pos->vf.index, pos->status, pos->vf.canvas0Addr);
		seq_printf(seq, "\t canvas index1: 0x%x, vframe type: 0x%x.\n",
			   pos->vf.canvas1Addr, pos->vf.type);
		seq_printf(seq, "\t ratio_control(0x%x).\n",
			   pos->vf.ratio_control);
	}
	spin_unlock_irqrestore(&p->wt_lock, flags);
	seq_puts(seq, "buffer in temp list:\n");
	spin_lock_irqsave(&p->tmp_lock, flags);
	list_for_each_entry_safe(pos, tmp, &p->tmp_list, list) {
		seq_printf(seq, "index: %u, status %u, canvas index0: 0x%x,\n",
			   pos->vf.index, pos->status, pos->vf.canvas0Addr);
		seq_printf(seq, "\t canvas index1: 0x%x, vframe type: 0x%x.\n",
			   pos->vf.canvas1Addr, pos->vf.type);
		seq_printf(seq, "\t ratio_control(0x%x).\n",
			   pos->vf.ratio_control);
	}
	spin_unlock_irqrestore(&p->tmp_lock, flags);
	seq_printf(seq, "buffer get count %d.\n", atomic_read(&p->buffer_cnt));
}

void vdin_vf_skip_all_disp(struct vf_pool *p)
{
	unsigned int i;

	for (i = 0; i < VFRAME_DISP_MAX_NUM; i++)
		p->disp_mode[i] = VFRAME_DISP_MODE_SKIP;
}

/*update the vframe disp_mode
 *	a.VFRAME_DISP_MODE_UNKNOWN
 *	b. VFRAME_DISP_MODE_OK
 */
void vdin_vf_disp_mode_update(struct vf_entry *vfe, struct vf_pool *p)
{
	unsigned int i;

	for (i = p->skip_vf_num; (i > 0) && (i < VFRAME_DISP_MAX_NUM); i--)
		p->disp_index[i] = p->disp_index[i - 1];
	p->disp_index[0]++;
	if (p->disp_index[0] >= VFRAME_DISP_MAX_NUM)
		p->disp_index[0] = 0;
	vfe->vf.index_disp = p->disp_index[0];

	for (i = p->skip_vf_num; i > 0  && (i < VFRAME_DISP_MAX_NUM); i--)
		if (p->disp_mode[p->disp_index[i]] != VFRAME_DISP_MODE_SKIP)
			p->disp_mode[p->disp_index[i]] = VFRAME_DISP_MODE_OK;

	p->disp_mode[p->disp_index[0]] = VFRAME_DISP_MODE_UNKNOWN;
}

/*skip all from current
 *disp_index[i]:
 *2:last last vframe,	1:last vframe
 *0:current vframe
 */
void vdin_vf_disp_mode_skip(struct vf_pool *p)
{
	unsigned int i;

	for (i = 0; i <= p->skip_vf_num; i++)
		p->disp_mode[p->disp_index[i]] = VFRAME_DISP_MODE_SKIP;
}
