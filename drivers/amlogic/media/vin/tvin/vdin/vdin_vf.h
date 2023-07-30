/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_vf.h
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

#ifndef __VDIN_VF_H
#define __VDIN_VF_H

/* Standard Linux Headers */
#include <linux/kernel.h>
#include <linux/spinlock.h>

/* Amlogic Linux Headers */
#include <linux/amlogic/media/vfm/vframe_provider.h>
#define VF_LOG_EN

#ifdef VF_LOG_EN

#define VF_LOG_LEN		 200
/* only log frontend operations */
#define VF_LOG_FE
/* only log backend operations */
#define VF_LOG_BE

#define VDIN_DV_MAX_NUM		        9

#define VF_FLAG_NORMAL_FRAME		 0x00000001
#define VF_FLAG_FREEZED_FRAME		 0x00000002
#define VF_FLAG_ONE_BUFFER_MODE		 0x00000004

#define VFRAME_DISP_MAX_NUM	25
#define VDIN_CANVAS_MIN_CNT	3
#define VDIN_CANVAS_MAX_CNT	12
#define VDIN_CANVAS_INTERLACED_MIN_CNT	8
/* total canvas num in chips */
#define VDIN_TOTAL_CANVAS_NUM	20
/* real canvas num for each vdin */
#define VDIN0_CANVAS_MAX_CNT	12
#define VDIN1_CANVAS_MAX_CNT	8
#define VDIN_VF_POOL_FREEZE              0x00000001
#define ISR_LOG_EN
#define VDIN_VF_MAX_NUM_PLANES	2
#define VF_LOG_PRINT_MAX_LEN 100
#define VDIN_VF_DBG_EN		BIT(0)
#define VDIN_VF_MOVE_EN		BIT(1)

/* vdin v4l2 defination */
#define VDIN_PLANES_IDX_Y	0
#define VDIN_PLANES_IDX_C	1
#define VDIN_MAX_PLANES		2
/* v4l2 defination end*/

enum vf_operation_e {
	VF_OPERATION_INIT = 0,
	VF_OPERATION_FPEEK,
	VF_OPERATION_FGET,
	VF_OPERATION_FPUT,
	VF_OPERATION_BPEEK,
	VF_OPERATION_BGET,
	VF_OPERATION_BPUT,
	VF_OPERATION_FREE,
};

enum vf_status_e {
	VF_STATUS_WL = 0, /* In write list */
	VF_STATUS_WM,	  /* In write mode */
	VF_STATUS_RL,	  /* In read  list */
	VF_STATUS_RM,	  /* In read  mode */
	VF_STATUS_WT,	  /* In wait  list */
	VF_STATUS_SL,
};

/* For scatter memory status */
enum vframe_sct_status_e {
	VFRAME_SCT_STATE_INIT = 0,
	VFRAME_SCT_STATE_FULL = 1,
	VFRAME_SCT_STATE_FREE_TAIL = 2,
};

struct vf_log_s {
	/*
	 * master
	 */
	/* [ 0][  n] 1: buf[n] is in write list */
	/* [ 1][  n] 1: buf[n] is in write mode */
	/* [ 2][  n] 1: buf[n] is in read  list */
	/* [ 3][  n] 1: buf[n] is in read  mode */
	/* [ 4][  n] 1: buf[n] is in wait  list */
	/*
	 * slave
	 */
	/* [ 5][  n] 1: buf[n] is in write list */
	/* [ 6][  n] 1: buf[n] is in write mode */
	/* [ 7][  n] 1: buf[n] is in read  list */
	/* [ 8][  n] 1: buf[n] is in read  mode */
	/* [ 9][  n] 1: buf[n] is in wait  list */
	/* [10][  7] 1: operation failure */
	/* [6:3]	reserved */
	/* [2:0]	operation ID */
	unsigned char  log_buf[VF_LOG_LEN][11];
	unsigned int   log_cur;
	struct timespec/*timeval*/ log_time[VF_LOG_LEN];
};
#endif

#ifdef ISR_LOG_EN
#define ISR_LOG_LEN 2000
struct isr_log_s {
	struct timespec /*timeval*/ isr_time[ISR_LOG_LEN];
	unsigned int log_cur;
	unsigned char isr_log_en;
};
#endif

struct vf_entry {
	struct vframe_s vf;
	enum vf_status_e status;
	struct list_head list;
	unsigned int flag;
	unsigned char af_num;/*afbce num*/
	enum vframe_sct_status_e sct_stat;/* scatter */
	/* for one buffer mode,backup the initial mif phy address */
	unsigned long phy_y_addr_bak;
	unsigned long phy_c_addr_bak;
};

struct vf_pool {
	unsigned int pool_flag;
	unsigned int max_size, size;
	struct vf_entry *master;
	struct vf_entry *slave;
	struct vf_entry *last_vfe;
	struct vf_entry *last_last_vfe;
	struct list_head wr_list; /* vf_entry */
	spinlock_t wr_lock;
	unsigned int wr_list_size;
	unsigned int wr_mode_size;
	struct list_head *wr_next;
	struct list_head rd_list; /* vf_entry */
	spinlock_t rd_lock;
	unsigned int rd_list_size;
	unsigned int rd_mode_size;
	struct list_head wt_list; /* vframe_s */
	spinlock_t wt_lock;
	unsigned int fz_list_size;
	struct list_head fz_list;
	spinlock_t fz_lock;/*fz lock*/
	unsigned int tmp_list_size;
	struct list_head tmp_list;
	struct tvin_dv_vsif_s dv_vsif;/*dolby vsi info*/
	spinlock_t tmp_lock;/*tmp lock*/
	spinlock_t log_lock;/*log lock*/
	spinlock_t dv_lock;/*dolby vision lock*/
#ifdef VF_LOG_EN
	struct vf_log_s log;
#endif
#ifdef ISR_LOG_EN
	struct isr_log_s isr_log;
#endif
	atomic_t buffer_cnt;
	unsigned int low_latency;
	unsigned int dv_buf_mem[VDIN_CANVAS_MAX_CNT];/*phy address*/
	void *dv_buf_vmem[VDIN_CANVAS_MAX_CNT];/*virtual address*/
	unsigned int dv_buf_size[VDIN_CANVAS_MAX_CNT];
	char *dv_buf[VDIN_CANVAS_MAX_CNT];/*temp use*/
	/*char *dv_buf_ori[VDIN_CANVAS_MAX_CNT];*/
	unsigned int disp_index[VFRAME_DISP_MAX_NUM];
	unsigned int skip_vf_num;/*skip pre vframe num*/
	unsigned int vf_move_prt_cnt;
	enum vframe_disp_mode_e	disp_mode[VFRAME_DISP_MAX_NUM];
};

extern unsigned int dolby_size_byte;
extern unsigned int dv_dbg_mask;

void vf_log_init(struct vf_pool *p);
void vf_log_print(struct vf_pool *p);

void isr_log_init(struct vf_pool *p);
void isr_log_print(struct vf_pool *p);
void isr_log(struct vf_pool *p);

struct vf_entry *vf_get_master(struct vf_pool *p, int index);
struct vf_entry *vf_get_slave(struct vf_pool *p, int index);

struct vf_pool *vf_pool_alloc(int size);
int vf_pool_init(struct vf_pool *p, int size);
void vf_pool_free(struct vf_pool *p);

void recycle_tmp_vfs(struct vf_pool *p);
void tmp_vf_put(struct vf_entry *vfe, struct vf_pool *p);
void tmp_to_rd(struct vf_pool *p);

struct vf_entry *provider_vf_peek(struct vf_pool *p);
struct vf_entry *provider_vf_get(struct vf_pool *p);
void provider_vf_put(struct vf_entry *vf, struct vf_pool *p);

struct vf_entry *receiver_vf_peek(struct vf_pool *p);
struct vf_entry *receiver_vf_get(struct vf_pool *p);
void receiver_vf_put(struct vframe_s *vf, struct vf_pool *p);

struct vframe_s *vdin_vf_peek(void *op_arg);
struct vframe_s *vdin_vf_get(void *op_arg);
void vdin_vf_put(struct vframe_s *vf, void *op_arg);
int vdin_vf_states(struct vframe_states *vf_ste, void *op_arg);

void vdin_vf_freeze(struct vf_pool *p, unsigned int hold_num);
void vdin_vf_unfreeze(struct vf_pool *p);
void vdin_dump_vf_state(struct vf_pool *p);
void vdin_dump_vf_state_seq(struct vf_pool *p, struct seq_file *seq);

void vdin_vf_skip_all_disp(struct vf_pool *p);
void vdin_vf_disp_mode_update(struct vf_entry *vfe, struct vf_pool *p);
void vdin_vf_disp_mode_skip(struct vf_pool *p);
#endif /* __VDIN_VF_H */

