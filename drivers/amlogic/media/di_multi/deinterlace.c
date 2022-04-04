// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/deinterlace.c
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/of_fdt.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vpu/vpu.h>
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#endif
#include <linux/amlogic/media/video_sink/video.h>

#include "../common/vfm/vfm.h"

#include "register.h"
#include "deinterlace.h"
#include "deinterlace_dbg.h"
#include "nr_downscale.h"
#include "di_reg_v2.h"

#include "di_data_l.h"
#include "deinterlace_hw.h"
#include "di_hw_v3.h"
#include "di_afbc_v3.h"

#include "di_dbg.h"
#include "di_pps.h"
#include "di_pre.h"
#include "di_prc.h"
#include "di_task.h"
#include "di_vframe.h"
#include "di_que.h"
#include "di_api.h"
#include "di_sys.h"
#include "di_reg_v3.h"
#include "dolby_sys.h"
/*2018-07-18 add debugfs*/
#include <linux/seq_file.h>
#include <linux/debugfs.h>
/*2018-07-18 -----------*/

#ifdef DET3D
#include "detect3d.h"
#endif
//#define ENABLE_SPIN_LOCK_ALWAYS

#undef module_param
#define module_param(x...)
#undef module_param_named
#define module_param_named(x...)
#undef module_param_array
#define module_param_array(x...)

static DEFINE_SPINLOCK(di_lock2);

#define di_lock_irqfiq_save(irq_flag) \
	spin_lock_irqsave(&di_lock2, irq_flag)

#define di_unlock_irqfiq_restore(irq_flag) \
	spin_unlock_irqrestore(&di_lock2, irq_flag)
void di_lock_irq(void)
{
	spin_lock(&di_lock2);
}

void di_unlock_irq(void)
{
	spin_unlock(&di_lock2);
}

#ifdef SUPPORT_MPEG_TO_VDIN
static int mpeg2vdin_flag;
static int mpeg2vdin_en;
#endif

static int di_reg_unreg_cnt = 40;
static bool overturn;

int dim_get_reg_unreg_cnt(void)
{
	return di_reg_unreg_cnt;
}

static bool mc_mem_alloc;

bool dim_get_mcmem_alloc(void)
{
	return mc_mem_alloc;
}

static unsigned int di_pre_rdma_enable;

static int kpi_frame_num;// default print first coming n frames

int di_get_kpi_frame_num(void)
{
	return kpi_frame_num;
}
/**************************************
 *
 *
 *************************************/


/* destroy unnecessary frames before display */
static unsigned int hold_video;

DEFINE_SPINLOCK(plist_lock);

static const char version_s[] = "2021-09-01 add dw";

/*1:enable bypass pre,ei only;
 * 2:debug force bypass pre,ei for post
 */
static int bypass_pre;

static int invert_top_bot;

/* add avoid vframe put/get error */
static int di_blocking;
/*
 * bit[2]: enable bypass all when skip
 * bit[1:0]: enable bypass post when skip
 */
/*static int di_vscale_skip_enable;*/

/* 0: not support nr10bit, 1: support nr10bit */
/*static unsigned int nr10bit_support;*/

#ifdef RUN_DI_PROCESS_IN_IRQ
/*
 * di_process() run in irq,
 * dim_reg_process(), dim_unreg_process() run in kernel thread
 * dim_reg_process_irq(), di_unreg_process_irq() run in irq
 * di_vf_put(), di_vf_peek(), di_vf_get() run in irq
 * di_receiver_event_fun() run in task or irq
 */
/*
 * important:
 * to set input2pre, VFRAME_EVENT_PROVIDER_VFRAME_READY of
 * vdin should be sent in irq
 */

static int input2pre;
/*false:process progress by field;
 * true: process progress by frame with 2 interlace buffer
 */
static int input2pre_buf_miss_count;
static int input2pre_proc_miss_count;
static int input2pre_throw_count;
static int input2pre_miss_policy;
/* 0, do not force pre_de_busy to 0, use di_wr_buf after dim_irq happen;
 * 1, force pre_de_busy to 0 and call
 *	dim_pre_de_done_buf_clear to clear di_wr_buf
 */
#endif
/*false:process progress by field;
 * bit0: process progress by frame with 2 interlace buffer
 * bit1: temp add debug for 3d process FA,1:bit0 force to 1;
 */
/*static int use_2_interlace_buff;*/
/* prog_proc_config,
 * bit[2:1]: when two field buffers are used,
 * 0 use vpp for blending ,
 * 1 use post_di module for blending
 * 2 debug mode, bob with top field
 * 3 debug mode, bot with bot field
 * bit[0]:
 * 0 "prog vdin" use two field buffers,
 * 1 "prog vdin" use single frame buffer
 * bit[4]:
 * 0 "prog frame from decoder/vdin" use two field buffers,
 * 1 use single frame buffer
 * bit[5]:
 * when two field buffers are used for decoder (bit[4] is 0):
 * 1,handle prog frame as two interlace frames
 * bit[6]:(bit[4] is 0,bit[5] is 0,use_2_interlace_buff is 0): 0,
 * process progress frame as field,blend by post;
 * 1, process progress frame as field,process by normal di
 */
/*static int prog_proc_config = (1 << 5) | (1 << 1) | 1;*/
/*
 * for source include both progressive and interlace pictures,
 * always use post_di module for blending
 */
#define is_handle_prog_frame_as_interlace(vframe)			\
	(((dimp_get(edi_mp_prog_proc_config) & 0x30) == 0x20) &&	\
	 (((vframe)->type & VIDTYPE_VIU_422) == 0))

static int frame_count;
static int disp_frame_count;
int di_get_disp_cnt(void)
{
	return disp_frame_count;
}

static unsigned long reg_unreg_timeout_cnt;
#ifdef DET3D
static unsigned int det3d_mode;
static void set3d_view(enum tvin_trans_fmt trans_fmt, struct vframe_s *vf);
#endif

static void di_pq_parm_destroy(struct di_pq_parm_s *pq_ptr);
static struct di_pq_parm_s *di_pq_parm_create(struct am_pq_parm_s *);

//static unsigned int unreg_cnt;/*cnt for vframe unreg*/
//static unsigned int reg_cnt;/*cnt for vframe reg*/

static unsigned char recovery_flag;

static unsigned int recovery_log_reason;
static unsigned int recovery_log_queue_idx;
static struct di_buf_s *recovery_log_di_buf;

unsigned char dim_vcry_get_flg(void)
{
	return recovery_flag;
}

void dim_vcry_flg_inc(void)
{
	recovery_flag++;
}

void dim_vcry_set_flg(unsigned char val)
{
	recovery_flag = val;
}

void dim_reg_timeout_inc(void)
{
	reg_unreg_timeout_cnt++;
}

/********************************/
unsigned int dim_vcry_get_log_reason(void)
{
	return recovery_log_reason;
}

void dim_vcry_set_log_reason(unsigned int val)
{
	recovery_log_reason = val;
}

/********************************/
unsigned char dim_vcry_get_log_q_idx(void)
{
	return recovery_log_queue_idx;
}

void dim_vcry_set_log_q_idx(unsigned int val)
{
	recovery_log_queue_idx = val;
}

/********************************/
struct di_buf_s **dim_vcry_get_log_di_buf(void)
{
	return &recovery_log_di_buf;
}

void dim_vcry_set_log_di_buf(struct di_buf_s *di_bufp)
{
	recovery_log_di_buf = di_bufp;
}

void dim_vcry_set(unsigned int reason, unsigned int idx,
		  struct di_buf_s *di_bufp)
{
	recovery_log_reason = reason;
	recovery_log_queue_idx = idx;
	recovery_log_di_buf = di_bufp;
}

static long same_field_top_count;
static long same_field_bot_count;
/* bit 0:
 * 0, keep 3 buffers in pre_ready_list for checking;
 * 1, keep 4 buffers in pre_ready_list for checking;
 */

static struct queue_s *get_queue_by_idx(unsigned int channel, int idx);
//static void dump_state(unsigned int channel);
//static void recycle_keep_buffer(unsigned int channel);

#define DI_PRE_INTERVAL         (HZ / 100)

/*
 * progressive frame process type config:
 * 0, process by field;
 * 1, process by frame (only valid for vdin source whose
 * width/height does not change)
 */

//static struct di_buf_s *cur_post_ready_di_buf;

/************For Write register**********************/

static unsigned int num_di_stop_reg_addr = 4;
static unsigned int di_stop_reg_addr[4] = {0};

static unsigned int is_need_stop_reg(unsigned int addr)
{
	int idx = 0;

	if (dimp_get(edi_mp_di_stop_reg_flag)) {
		for (idx = 0; idx < num_di_stop_reg_addr; idx++) {
			if (addr == di_stop_reg_addr[idx]) {
				pr_dbg("stop write addr: %x\n", addr);
				return 1;
			}
		}
	}

	return 0;
}

void DIM_DI_WR(unsigned int addr, unsigned int val)
{
	if (is_need_stop_reg(addr))
		return;
	ddbg_reg_save(addr, val, 0, 32);
	WR(addr, val);
}

void wr_reg_bits(unsigned int adr, unsigned int val,
		 unsigned int start, unsigned int len)
{
	if (len >= 32) {
		DIM_DI_WR(adr, val);
		return;
	}
	aml_vcbus_update_bits(adr, ((1 << (len)) - 1) << (start),
			      (val) << (start));
}

void DIM_DI_WR_REG_BITS(unsigned int adr, unsigned int val,
			unsigned int start, unsigned int len)
{
	if (is_need_stop_reg(adr))
		return;
	ddbg_reg_save(adr, val, start, len); /*ary add for debug*/
	wr_reg_bits(adr, val, start, len);
}

void DIM_VSYNC_WR_MPEG_REG(unsigned int addr, unsigned int val)
{
	if (is_need_stop_reg(addr))
		return;
	if (dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support))
		DIM_DI_WR(addr, val);
	else
		VSYNC_WR_MPEG_REG(addr, val);
}

/* dim_VSYNC_WR_MPEG_REG_BITS */

unsigned int DIM_VSC_WR_MPG_BT(unsigned int addr, unsigned int val,
			       unsigned int start, unsigned int len)
{
	if (is_need_stop_reg(addr))
		return 0;
	if (dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support))
		DIM_DI_WR_REG_BITS(addr, val, start, len);
	else
		VSYNC_WR_MPEG_REG_BITS(addr, val, start, len);

	return 0;
}

#ifdef DI_V2
unsigned int DI_POST_REG_RD(unsigned int addr)
{
	struct di_dev_s  *de_devp = get_dim_de_devp();

	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		PR_ERR("REG 0x%x access prohibited.\n", addr);
		return 0;
	}
	return VSYNC_RD_MPEG_REG(addr);
}
EXPORT_SYMBOL(DI_POST_REG_RD);

int DI_POST_WR_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	struct di_dev_s  *de_devp = get_dim_de_devp();

	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		PR_ERR("REG 0x%x access prohibited.\n", adr);
		return -1;
	}
	return VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
}
EXPORT_SYMBOL(DI_POST_WR_REG_BITS);
#else
unsigned int l_DI_POST_REG_RD(unsigned int addr)
{
	struct di_dev_s  *de_devp = get_dim_de_devp();

	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		PR_ERR("REG 0x%x access prohibited.\n", addr);
		return 0;
	}
	return VSYNC_RD_MPEG_REG(addr);
}

int l_DI_POST_WR_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	struct di_dev_s  *de_devp = get_dim_de_devp();

	if (IS_ERR_OR_NULL(de_devp))
		return 0;
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		PR_ERR("REG 0x%x access prohibited.\n", adr);
		return -1;
	}
	return VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
}

#endif
/**********************************/

/*****************************
 *	 di attr management :
 *	 enable
 *	 mode
 *	 reg
 ******************************/
/*config attr*/

int pre_run_flag = DI_RUN_FLAG_RUN;

bool pre_dbg_is_run(void)
{
	bool ret = false;

	if (pre_run_flag == DI_RUN_FLAG_RUN	||
	    pre_run_flag == DI_RUN_FLAG_STEP) {
		if (pre_run_flag == DI_RUN_FLAG_STEP)
			pre_run_flag = DI_RUN_FLAG_STEP_DONE;
		ret = true;
	}

	return ret;
}
static int dump_state_flag;

int dump_state_flag_get(void)
{
	return dump_state_flag;
}
const struct afd_ops_s *dim_afds(void)
{
	return get_datal()->afds;
}

struct afbcd_ctr_s *di_get_afd_ctr(void)
{
	return &get_datal()->di_afd.ctr;
}

const char *dim_get_version_s(void)
{
	return version_s;
}

int dim_get_blocking(void)
{
	return di_blocking;
}

unsigned long dim_get_reg_unreg_timeout_cnt(void)
{
	return reg_unreg_timeout_cnt;
}

struct di_buf_s *dim_get_recovery_log_di_buf(void)
{
	return recovery_log_di_buf;
}

struct vframe_s **dim_get_vframe_in(unsigned int ch)
{
	return get_vframe_in(ch);
}

int dim_get_dump_state_flag(void)
{
	return dump_state_flag;
}

/*--------------------------*/

ssize_t
store_dbg(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf, size_t count)
{
	unsigned int channel = get_current_channel();	/* debug only*/
	struct di_buf_s *pbuf_local = get_buf_local(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);

	if (strncmp(buf, "buf", 3) == 0) {
		struct di_buf_s *di_buf_tmp = 0;

		if (kstrtoul(buf + 3, 16, (unsigned long *)&di_buf_tmp))
			return count;
		dim_dump_di_buf(di_buf_tmp);
	} else if (strncmp(buf, "vframe", 6) == 0) {
		vframe_t *vf = 0;

		if (kstrtoul(buf + 6, 16, (unsigned long *)&vf))
			return count;
		dim_dump_vframe(vf);
	} else if (strncmp(buf, "pool", 4) == 0) {
		unsigned long idx = 0;

		if (kstrtoul(buf + 4, 10, &idx))
			return count;
		dim_dump_pool(get_queue_by_idx(channel, idx));
	} else if (strncmp(buf, "state", 4) == 0) {
		//dump_state(channel);
		pr_info("add new debugfs: cat /sys/kernel/debug/di/state\n");
	} else if (strncmp(buf, "prog_proc_config", 16) == 0) {
		if (buf[16] == '1')
			dimp_set(edi_mp_prog_proc_config, 1);
		else
			dimp_set(edi_mp_prog_proc_config, 0);
	} else if (strncmp(buf, "init_flag", 9) == 0) {
		if (buf[9] == '1')
			set_init_flag(0, true);/*init_flag = 1;*/
		else
			set_init_flag(0, false);/*init_flag = 0;*/
	} else if (strncmp(buf, "prun", 4) == 0) {
		pre_run_flag = DI_RUN_FLAG_RUN;
	} else if (strncmp(buf, "ppause", 6) == 0) {
		pre_run_flag = DI_RUN_FLAG_PAUSE;
	} else if (strncmp(buf, "pstep", 5) == 0) {
		pre_run_flag = DI_RUN_FLAG_STEP;
	} else if (strncmp(buf, "dumpreg", 7) == 0) {
		pr_info("add new debugfs: cat /sys/kernel/debug/di/dumpreg\n");
	} else if (strncmp(buf, "dumpmif", 7) == 0) {
		dim_dump_mif_size_state(ppre, ppost);
	} else if (strncmp(buf, "recycle_buf", 11) == 0) {
//		recycle_keep_buffer(channel);
	} else if (strncmp(buf, "recycle_post", 12) == 0) {
		if (di_vf_l_peek(channel))
			di_vf_l_put(di_vf_l_get(channel), channel);
	} else if (strncmp(buf, "mem_map", 7) == 0) {
		dim_dump_buf_addr(pbuf_local, MAX_LOCAL_BUF_NUM * 2);
	} else if (strncmp(buf, "setdv", 5) == 0) {
		pr_info("setdv\n");
		di_dolby_do_setting();
	} else if (strncmp(buf, "endv", 4) == 0) {
		pr_info("endv\n");
		di_dolby_enable(1);
	} else if (strncmp(buf, "initdv", 6) == 0) {
		pr_info("initdv\n");
		di_dolby_sw_init();
	} else if (strncmp(buf, "crcdump", 7) == 0) {
		dim_dump_crc_state();
	} else if (strncmp(buf, "pulldown", 8) == 0) {
		dim_dump_pulldown_state();
	} else {
		pr_info("DI no support cmd %s!!!\n", buf);
	}

	return count;
}

#ifdef ARY_TEMP
static int __init di_read_canvas_reverse(char *str)
{
	unsigned char *ptr = str;

	pr_dbg("%s: bootargs is %s.\n", __func__, str);
	if (strstr(ptr, "1")) {
		invert_top_bot |= 0x1;
		overturn = true;
	} else {
		invert_top_bot &= (~0x1);
		overturn = false;
	}

	return 0;
}

__setup("video_reverse=", di_read_canvas_reverse);
#endif

static unsigned char *di_log_buf;
static unsigned int di_log_wr_pos;
static unsigned int di_log_rd_pos;
static unsigned int di_log_buf_size;

//static unsigned int buf_state_log_start;
/*  set to 1 by condition of "post_ready count < buf_state_log_threshold",
 * reset to 0 by set buf_state_log_threshold as 0
 */

static DEFINE_SPINLOCK(di_print_lock);

#define PRINT_TEMP_BUF_SIZE 128

static int di_print_buf(char *buf, int len)
{
	unsigned long flags;
	int pos;
	int di_log_rd_pos_;

	if (di_log_buf_size == 0)
		return 0;

	spin_lock_irqsave(&di_print_lock, flags);
	di_log_rd_pos_ = di_log_rd_pos;
	if (di_log_wr_pos >= di_log_rd_pos)
		di_log_rd_pos_ += di_log_buf_size;

	for (pos = 0; pos < len && di_log_wr_pos < (di_log_rd_pos_ - 1);
	     pos++, di_log_wr_pos++) {
		if (di_log_wr_pos >= di_log_buf_size)
			di_log_buf[di_log_wr_pos - di_log_buf_size] = buf[pos];
		else
			di_log_buf[di_log_wr_pos] = buf[pos];
	}
	if (di_log_wr_pos >= di_log_buf_size)
		di_log_wr_pos -= di_log_buf_size;
	spin_unlock_irqrestore(&di_print_lock, flags);
	return pos;
}

/* static int log_seq = 0; */
int dim_print(const char *fmt, ...)
{
	va_list args;
	int avail = PRINT_TEMP_BUF_SIZE;
	char buf[PRINT_TEMP_BUF_SIZE];
	int pos, len = 0;

	if (dimp_get(edi_mp_di_printk_flag) & 1) {
		if (dimp_get(edi_mp_di_log_flag) & DI_LOG_PRECISE_TIMESTAMP)
			pr_dbg("%llums:", cur_to_msecs());
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
		return 0;
	}

	if (di_log_buf_size == 0)
		return 0;

/* len += snprintf(buf+len, avail-len, "%d:",log_seq++); */
	if (dimp_get(edi_mp_di_log_flag) & DI_LOG_TIMESTAMP)
		len += snprintf(buf + len, avail - len, "%u:",
			jiffies_to_msecs(jiffies_64));

	va_start(args, fmt);
	len += vsnprintf(buf + len, avail - len, fmt, args);
	va_end(args);

	if ((avail - len) <= 0)
		buf[PRINT_TEMP_BUF_SIZE - 1] = '\0';

	pos = di_print_buf(buf, len);
/* pr_dbg("dim_print:%d %d\n", di_log_wr_pos, di_log_rd_pos); */
	return pos;
}

ssize_t dim_read_log(char *buf)
{
	unsigned long flags;
	ssize_t read_size = 0;

	if (di_log_buf_size == 0)
		return 0;
/* pr_dbg("show_log:%d %d\n", di_log_wr_pos, di_log_rd_pos); */
	spin_lock_irqsave(&di_print_lock, flags);
	if (di_log_rd_pos < di_log_wr_pos)
		read_size = di_log_wr_pos - di_log_rd_pos;

	else if (di_log_rd_pos > di_log_wr_pos)
		read_size = di_log_buf_size - di_log_rd_pos;

	if (read_size > PAGE_SIZE)
		read_size = PAGE_SIZE;
	if (read_size > 0)
		memcpy(buf, di_log_buf + di_log_rd_pos, read_size);

	di_log_rd_pos += read_size;
	if (di_log_rd_pos >= di_log_buf_size)
		di_log_rd_pos = 0;
	spin_unlock_irqrestore(&di_print_lock, flags);
	return read_size;
}

ssize_t
store_log(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf, size_t count)
{
	unsigned long flags, tmp;

	if (strncmp(buf, "bufsize", 7) == 0) {
		if (kstrtoul(buf + 7, 10, &tmp))
			return count;
		spin_lock_irqsave(&di_print_lock, flags);
		kfree(di_log_buf);
		di_log_buf = NULL;
		di_log_buf_size = 0;
		di_log_rd_pos = 0;
		di_log_wr_pos = 0;
		if (tmp >= 1024) {
			di_log_buf_size = 0;
			di_log_rd_pos = 0;
			di_log_wr_pos = 0;
			di_log_buf = kmalloc(tmp, GFP_KERNEL);
			if (di_log_buf)
				di_log_buf_size = tmp;
		}
		spin_unlock_irqrestore(&di_print_lock, flags);
		pr_dbg("di_store:set bufsize tmp %lu %u\n",
		       tmp, di_log_buf_size);
	} else if (strncmp(buf, "printk", 6) == 0) {
		if (kstrtoul(buf + 6, 10, &tmp))
			return count;

		dimp_set(edi_mp_di_printk_flag, tmp);
	} else {
		dim_print("%s", buf);
	}
	return 16;
}

ssize_t
show_vframe_status(struct device *dev,
		   struct device_attribute *attr,
		   char *buf)
{
	int ret = 0;
	int get_ret = 0;

	struct vframe_states states;
	int ch;
	struct di_mng_s *pbm = get_bufmng();

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		if (!pbm->sub_act_flg[ch])
			continue;
		else
			ret += sprintf(buf + ret, "ch[%d]\n", ch);

		get_ret = vf_get_states_by_name(di_rev_name[ch], &states);

		if (get_ret == 0) {
			ret += sprintf(buf + ret, "vframe_pool_size=%d\n",
				states.vf_pool_size);
			ret += sprintf(buf + ret, "vframe buf_free_num=%d\n",
				states.buf_free_num);
			ret += sprintf(buf + ret, "vframe buf_recycle_num=%d\n",
				states.buf_recycle_num);
			ret += sprintf(buf + ret, "vframe buf_avail_num=%d\n",
				states.buf_avail_num);
		} else {
			ret += sprintf(buf + ret, "vframe no states\n");
		}
	}
	return ret;
}

ssize_t
show_kpi_frame_num(struct device *dev,
		   struct device_attribute *attr,
		   char *buf)
{
	return sprintf(buf, "kpi_frame_num:%d\n", kpi_frame_num);
}

ssize_t
store_kpi_frame_num(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t len)
{
	unsigned long num;
	int ret = kstrtoul(buf, 0, (unsigned long *)&num);

	if (ret < 0)
		return -EINVAL;
	kpi_frame_num = (int)num;
	return len;
}

/***************************
 * di buffer management
 ***************************/

static const char * const vframe_type_name[] = {
	"", "di_buf_in", "di_buf_loc", "di_buf_post"
};

const char *dim_get_vfm_type_name(unsigned int nub)
{
	if (nub < 4)
		return vframe_type_name[nub];

	return "";
}

static unsigned int default_width = 1920;
static unsigned int default_height = 1080;

void dim_get_default(unsigned int *h, unsigned int *w)
{
	*h = default_height;
	*w = default_width;
}

void di_set_default(unsigned int ch)
{
	struct div2_mm_s *mm;
//	enum EDI_SGN sgn;
//	struct di_pre_stru_s *ppre = get_pre_stru(ch);

	mm = dim_mm_get(ch);
	if (dim_afds()		&&
	    dip_is_support_4k(ch)) {
		default_width = 3840;
		default_height = 2160;
	} else {
		default_width = 1920;
		default_height = 1080;
	}

	if (dip_cfg_is_pps_4k(ch)) {
		default_width = 3840;
		default_height = 2160;
	}
}

void dbg_h_w(unsigned int ch, unsigned int nub)
{
	struct div2_mm_s *mm;

	mm = dim_mm_get(ch);
	dbg_reg("%s:ch[%d][%d]:h[%d],w[%d]\n", __func__,
		ch, nub, mm->cfg.di_h, mm->cfg.di_w);
}
/*
 * all buffers are in
 * 1) list of local_free_list,in_free_list,pre_ready_list,recycle_list
 * 2) di_pre_stru.di_inp_buf
 * 3) di_pre_stru.di_wr_buf
 * 4) cur_post_ready_di_buf
 * 5) (struct di_buf_s*)(vframe->private_data)->di_buf[]
 *
 * 6) post_free_list_head
 * 8) (struct di_buf_s*)(vframe->private_data)
 */

/*move to deinterlace .h #define queue_t struct queue_s*/

static struct queue_s *get_queue_by_idx(unsigned int channel, int idx)
{
	struct queue_s *pqueue = get_queue(channel);

	if (idx < QUEUE_NUM)
		return &pqueue[idx];
	else
		return NULL;
}

struct di_buf_s *dim_get_buf(unsigned int channel, int queue_idx,
			     int *start_pos)
{
	struct queue_s *pqueue = get_queue(channel);
	queue_t *q = &pqueue[queue_idx];
	int idx = 0;
	unsigned int pool_idx, num;
	struct di_buf_s *di_buf = NULL;
	int start_pos_init = *start_pos;
	struct di_buf_pool_s *pbuf_pool = get_buf_pool(channel);

	if (dimp_get(edi_mp_di_log_flag) & DI_LOG_QUEUE)
		dim_print("%s:<%d:%d,%d,%d> %d\n", __func__, queue_idx,
			  q->num, q->in_idx, q->out_idx, *start_pos);

	if (q->type == 0) {
		if ((*start_pos) < q->num) {
			idx = q->out_idx + (*start_pos);
			if (idx >= MAX_QUEUE_POOL_SIZE)
				idx -= MAX_QUEUE_POOL_SIZE;

			(*start_pos)++;
		} else {
			idx = MAX_QUEUE_POOL_SIZE;
		}
	} else if ((q->type == 1) || (q->type == 2)) {
		for (idx = (*start_pos); idx < MAX_QUEUE_POOL_SIZE; idx++) {
			if (q->pool[idx] != 0) {
				*start_pos = idx + 1;
				break;
			}
		}
	}
	if (idx < MAX_QUEUE_POOL_SIZE) {
		pool_idx = ((q->pool[idx] >> 8) & 0xff) - 1;
		num = q->pool[idx] & 0xff;
		if (pool_idx < VFRAME_TYPE_NUM) {
			if (num < pbuf_pool[pool_idx].size)
				di_buf =
					&pbuf_pool[pool_idx].di_buf_ptr[num];
		}
	}

	if ((di_buf) && ((((pool_idx + 1) << 8) | num)
			 != ((di_buf->type << 8) | di_buf->index))) {
		PR_ERR("%s:(%x,%x)\n", __func__,
		       (((pool_idx + 1) << 8) | num),
		       ((di_buf->type << 8) | (di_buf->index)));
		if (recovery_flag == 0) {
			recovery_log_reason = 1;
			recovery_log_queue_idx =
				(start_pos_init << 8) | queue_idx;
			recovery_log_di_buf = di_buf;
		}
		recovery_flag++;
		di_buf = NULL;
	}

	if (dimp_get(edi_mp_di_log_flag) & DI_LOG_QUEUE) {
		if (di_buf)
			dim_print("%s: 0x%p(%d,%d)\n", __func__, di_buf,
				  pool_idx, num);
		else
			dim_print("%s: 0x%p\n", __func__, di_buf);
	}

	return di_buf;
}

/*--------------------------*/
/*--------------------------*/
ssize_t
store_dump_mem(struct device *dev, struct device_attribute *attr,
	       const char *buf, size_t len)
{
	unsigned int n = 0, canvas_w = 0, canvas_h = 0;
	unsigned long nr_size = 0, dump_adr = 0;
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *post_vf = NULL;
	char *buf_orig, *ps, *token;
	char *parm[5] = { NULL };
	char delim1[3] = " ";
	char delim2[2] = "\n";
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buff = NULL;
	mm_segment_t old_fs;
	bool bflg_vmap = false;
	unsigned int channel = get_current_channel();/* debug only*/
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
//	struct di_dev_s  *de_devp = get_dim_de_devp();
	/*ary add 2019-07-2 being*/
	unsigned int indx;
	struct di_buf_s *pbuf_post;
	struct di_buf_s *pbuf_local;
	struct di_post_stru_s *ppost;
	struct div2_mm_s *mm;
	struct di_ch_s *pch;
	unsigned int sh, sv;
	/*************************/
	pch = get_chdata(channel);

	buf_orig = kstrdup(buf, GFP_KERNEL);
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (strcmp(parm[0], "capture") == 0) {
		di_buf = ppre->di_mem_buf_dup_p;
	} else if (strcmp(parm[0], "c_post") == 0) {
		/*ary add 2019-07-2*/
		if (kstrtouint(parm[2], 0, &channel)) {
			PR_ERR("c_post:ch is not number\n");
			kfree(buf_orig);
			return 0;
		}
		if (kstrtouint(parm[3], 0, &indx)) {
			PR_ERR("c_post:ch is not number\n");
			kfree(buf_orig);
			return 0;
		}
		di_pr_info("c_post:ch[%d],index[%d]\n", channel, indx);
		mm = dim_mm_get(channel);

		ppre = get_pre_stru(channel);
		ppost = get_post_stru(channel);
		#ifdef MARK_SC2
		/*mm-0705	if (indx >= ppost->di_post_num) {*/
		if (indx >= mm->sts.num_post) {
			PR_ERR("c_post:index is overflow:%d[%d]\n", indx,
			       mm->sts.num_post);
			kfree(buf_orig);
			return 0;
		}
		#endif
		pbuf_post = get_buf_post(channel);
		di_buf = &pbuf_post[indx];
	} else if (strcmp(parm[0], "c_phf") == 0) {
		/*ary add 2021-04-15 for post hf */
		if (kstrtouint(parm[2], 0, &channel)) {
			PR_ERR("c_phf:ch is not number\n");
			kfree(buf_orig);
			return 0;
		}
		if (kstrtouint(parm[3], 0, &indx)) {
			PR_ERR("c_phf:ch is not number\n");
			kfree(buf_orig);
			return 0;
		}
		di_pr_info("c_phf:ch[%d],index[%d]\n", channel, indx);
		mm = dim_mm_get(channel);

		ppre = get_pre_stru(channel);
		ppost = get_post_stru(channel);
		pbuf_post = get_buf_post(channel);
		di_buf = &pbuf_post[indx];
		if (!di_buf->hf_done) {
			PR_ERR("c_phf: no hf\n");
			kfree(buf_orig);
			return 0;
		}
		dump_adr = di_buf->hf.phy_addr;
		nr_size = (unsigned long)di_buf->hf.buffer_w *
			(unsigned long)di_buf->hf.buffer_h;
		PR_INF("\tadd:0x%lx;size:0x%lx <%d,%d>\n",
			dump_adr,
			nr_size,
			di_buf->hf.buffer_w,
			di_buf->hf.buffer_h);
	} else if (strcmp(parm[0], "c_local") == 0) {
		/*ary add 2019-07-2*/
		if (kstrtouint(parm[2], 0, &channel)) {
			PR_ERR("c_local:ch is not number\n");
			kfree(buf_orig);
			return 0;
		}
		if (kstrtouint(parm[3], 0, &indx)) {
			PR_ERR("c_local:ch is not number\n");
			kfree(buf_orig);
			return 0;
		}
		di_pr_info("c_local:ch[%d],index[%d]\n", channel, indx);

		ppre = get_pre_stru(channel);
		ppost = get_post_stru(channel);
		#ifdef MARK_HIS
		if (indx >= ppost->di_post_num) {
			PR_ERR("c_local:index is overflow:%d[%d]\n",
			       indx, ppost->di_post_num);
			kfree(buf_orig);
			return 0;
		}
		#endif
		pbuf_local = get_buf_local(channel);
		di_buf = &pbuf_local[indx];
	} else if (strcmp(parm[0], "capture_pready") == 0) {	/*ary add*/
		#ifdef MARK_HIS
		if (!di_que_is_empty(channel, QUE_POST_READY)) {
			di_buf = di_que_peek(channel, QUE_POST_READY);
			pr_info("get post ready di_buf:%d:0x%p\n",
				di_buf->index, di_buf);
		} else {
			pr_info("war:no post ready buf\n");
		}
		#else
		di_buf = ndrd_qpeekbuf(pch);
		if (di_buf)
			pr_info("get post ready di_buf:%d:0x%p\n",
				di_buf->index, di_buf);
		else
			pr_info("war:no post ready buf\n");

		#endif
	} else if (strcmp(parm[0], "capture_post") == 0) {
		if (di_vf_l_peek(channel)) {
			post_vf = di_vf_l_get(channel);
			if (!IS_ERR_OR_NULL(post_vf)) {
				di_buf = post_vf->private_data;
				di_vf_l_put(post_vf, channel);
				pr_info("get post di_buf:%d:0x%p\n",
					di_buf->index, di_buf);
			} else {
				pr_info("war:peek no post buf, vfm[0x%p]\n",
					post_vf);
			}

			post_vf = NULL;
		} else {
			pr_info("war:can't peek post buf\n");
		}
	} else if (strcmp(parm[0], "capture_nrds") == 0) {
		dim_get_nr_ds_buf(&dump_adr, &nr_size);
	} else {
		PR_ERR("wrong dump cmd\n");
		kfree(buf_orig);
		return len;
	}
	if (nr_size == 0) {
		if (unlikely(!di_buf)) {
			pr_info("war:di_buf is null\n");
			kfree(buf_orig);
			return len;
		}
		canvas_w = di_buf->canvas_width[NR_CANVAS];
		canvas_h = di_buf->canvas_height;
		//nr_size = canvas_w * canvas_h * 2;
		dump_adr = di_buf->nr_adr;
		sh = di_buf->vframe->canvas0_config[0].width;
		sv = di_buf->vframe->canvas0_config[0].height;
		nr_size = sh * sv;

		if (di_buf->vframe->plane_num == 2)
			nr_size = nr_size * 2;

		pr_info("w=%d,h=%d,size=%ld,addr=%lx\n",
			canvas_w, canvas_h, nr_size, dump_adr);
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	/* pr_dbg("dump path =%s\n",dump_path); */
	filp = filp_open(parm[1], O_RDWR | O_CREAT, 0666);
	if (IS_ERR(filp)) {
		PR_ERR("create %s error.\n", parm[1]);
		kfree(buf_orig);
		return len;
	}
	dump_state_flag = 1;
	if (/*de_devp->flags & DI_MAP_FLAG*/ 1) {
		/*buff = (void *)phys_to_virt(dump_adr);*/
		buff = dim_vmap(dump_adr, nr_size, &bflg_vmap);
		if (!buff) {
			if (nr_size <= 5222400) {
				pr_info("di_vap err\n");
				filp_close(filp, NULL);
				kfree(buf_orig);
				return len;
			}
			/*try again:*/
			PR_INF("vap err,size to 5222400, try again\n");
			nr_size = 5222400;
			buff = dim_vmap(dump_adr, nr_size, &bflg_vmap);
			if (!buff) {
				filp_close(filp, NULL);
				kfree(buf_orig);
				return len;
			}
		}
		pr_info("wr buffer 0x%lx  to %s.\n", dump_adr, parm[1]);
		pr_info("size:0x%lx, bflg_vmap[%d]\n", nr_size, bflg_vmap);
	} else {
		buff = ioremap(dump_adr, nr_size);
	}
	if (IS_ERR_OR_NULL(buff))
		PR_ERR("%s: ioremap error.\n", __func__);
	vfs_write(filp, buff, nr_size, &pos);
/*	pr_dbg("di_chan2_buf_dup_p:\n	nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_chan2_buf_dup_p->nr_adr,
 * di_pre_stru.di_chan2_buf_dup_p->mtn_adr,
 * di_pre_stru.di_chan2_buf_dup_p->cnt_adr);
 * pr_dbg("di_inp_buf:\n	nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_inp_buf->nr_adr,
 * di_pre_stru.di_inp_buf->mtn_adr,
 * di_pre_stru.di_inp_buf->cnt_adr);
 * pr_dbg("di_wr_buf:\n	nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_wr_buf->nr_adr,
 * di_pre_stru.di_wr_buf->mtn_adr,
 * di_pre_stru.di_wr_buf->cnt_adr);
 * pr_dbg("di_mem_buf_dup_p:\n  nr:%u,mtn:%u,cnt:%u\n",
 * di_pre_stru.di_mem_buf_dup_p->nr_adr,
 * di_pre_stru.di_mem_buf_dup_p->mtn_adr,
 * di_pre_stru.di_mem_buf_dup_p->cnt_adr);
 * pr_dbg("di_mem_start=%u\n",di_mem_start);
 */
	vfs_fsync(filp, 0);
	pr_info("write buffer 0x%lx  to %s.\n", dump_adr, parm[1]);
	if (bflg_vmap)
		dim_unmap_phyaddr(buff);
#ifdef MARK_HIS
	if (!(de_devp->flags & DI_MAP_FLAG))
		iounmap(buff);
#endif
	dump_state_flag = 0;
	filp_close(filp, NULL);
	set_fs(old_fs);
	kfree(buf_orig);
	return len;
}

static void recycle_vframe_type_pre(struct di_buf_s *di_buf,
				    unsigned int channel);
static void recycle_vframe_type_post(struct di_buf_s *di_buf,
				     unsigned int channel);
static void add_dummy_vframe_type_pre(struct di_buf_s *src_buf,
				      unsigned int channel);
#ifdef DI_BUFFER_DEBUG
static void
recycle_vframe_type_post_print(struct di_buf_s *di_buf,
			       const char *func,
			       const int line);
#endif

static void dis2_di(void)
{
//ary 2020-12-09	ulong flags = 0, irq_flag2 = 0;
	unsigned int channel = get_current_channel();/* debug only*/
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	set_init_flag(channel, false);/*init_flag = 0;*/
//ary 2020-12-09	di_lock_irqfiq_save(irq_flag2);
/* vf_unreg_provider(&di_vf_prov); */
	pw_vf_light_unreg_provider(channel);
//ary 2020-12-09	di_unlock_irqfiq_restore(irq_flag2);
	set_reg_flag(channel, false);
//ary 2020-12-09	spin_lock_irqsave(&plist_lock, flags);
//ary 2020-12-09	di_lock_irqfiq_save(irq_flag2);
	if (ppre->di_inp_buf) {
		if (pvframe_in[ppre->di_inp_buf->index]) {
			pw_vf_put(pvframe_in[ppre->di_inp_buf->index],
				  channel);
			pvframe_in[ppre->di_inp_buf->index] = NULL;
			pw_vf_notify_provider(channel,
					      VFRAME_EVENT_RECEIVER_PUT, NULL);
		}
		ppre->di_inp_buf->invert_top_bot_flag = 0;

		di_que_in(channel, QUE_IN_FREE, ppre->di_inp_buf);
		ppre->di_inp_buf = NULL;
	}
	dim_uninit_buf(0, channel);
	if (0/*get_blackout_policy()*/) {
		DIM_DI_WR(DI_CLKG_CTRL, 0x2);
		if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
			dimh_enable_di_post_mif(GATE_OFF);
			dim_post_gate_control(false);
			dim_top_gate_control(false, false);
		}
	}

	if (dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support))
		dim_set_power_control(0);

//ary 2020-12-09	di_unlock_irqfiq_restore(irq_flag2);
//ary 2020-12-09	spin_unlock_irqrestore(&plist_lock, flags);
}

ssize_t
store_config(struct device *dev,
	     struct device_attribute *attr,
	     const char *buf, size_t count)
{
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	unsigned int channel = get_current_channel();/* debug only*/
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	buf_orig = kstrdup(buf, GFP_KERNEL);
	dim_parse_cmd_params(buf_orig, (char **)(&parm));

	if (strncmp(buf, "disable", 7) == 0) {
		dim_print("%s: disable\n", __func__);

		if (get_init_flag(channel)) {/*if (init_flag) {*/
			ppre->disable_req_flag = 1;

			while (ppre->disable_req_flag)
				usleep_range(1000, 1001);
		}
	} else if (strncmp(buf, "dis2", 4) == 0) {
		dis2_di();
	} else if (strcmp(parm[0], "hold_video") == 0) {
		pr_info("%s(%s %s)\n", __func__, parm[0], parm[1]);
		rc = kstrtouint(parm[1], 10, &hold_video);
	}
	kfree(buf_orig);
	return count;
}

static unsigned char is_progressive(vframe_t *vframe)
{
	unsigned char ret = 0;

	ret = ((vframe->type & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE);
	return ret;
}

static unsigned char is_source_change(vframe_t *vframe, unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	unsigned int x, y;

	dim_vf_x_y(vframe, &x, &y);
#define VFRAME_FORMAT_MASK      \
	(VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_444 | \
	 VIDTYPE_MVC)
	if (ppre->cur_width != x		||
	    ppre->cur_height != y		||
	    (((ppre->cur_inp_type & VFRAME_FORMAT_MASK)		!=
	    (vframe->type & VFRAME_FORMAT_MASK))		&&
	    (!is_handle_prog_frame_as_interlace(vframe)))	||
	    (ppre->cur_source_type != vframe->source_type)	||
	    ((ppre->cur_inp_type & VIDTYPE_INTERLACE_TOP)	!=
	     (vframe->type & VIDTYPE_INTERLACE_TOP))) {
		/* video format changed */
		return 1;
	} else if (((ppre->cur_prog_flag != is_progressive(vframe)) &&
		   (!is_handle_prog_frame_as_interlace(vframe))) ||
		   ((ppre->cur_inp_type & VIDTYPE_VIU_FIELD) !=
		   (vframe->type & VIDTYPE_VIU_FIELD))
	) {
		/* just scan mode changed */
		if (!ppre->force_interlace)
			pr_dbg("DI I<->P.\n");
		return 2;
	}
	return 0;
}

/*
 * static unsigned char is_vframe_type_change(vframe_t* vframe)
 * {
 * if(
 * (di_pre_stru.cur_prog_flag!=is_progressive(vframe))||
 * ((di_pre_stru.cur_inp_type&VFRAME_FORMAT_MASK)!=
 * (vframe->type&VFRAME_FORMAT_MASK))
 * )
 * return 1;
 *
 * return 0;
 * }
 */
static int trick_mode;

static unsigned int dim_bypass_check(struct vframe_s *vf);
//---------------------
//dbg only for fg:
static bool dim_trig_fg;
module_param_named(dim_trig_fg, dim_trig_fg, bool, 0664);
static bool fg_bypass;

unsigned char dim_is_bypass(vframe_t *vf_in, unsigned int ch)
{
//	unsigned int vtype = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	unsigned int reason = 0;
	struct di_ch_s *pch;

	/*need bypass*/
	reason = dim_bypass_check(vf_in);

	if (reason)
		return reason;

	if (di_cfgx_get(ch, EDI_CFGX_BYPASS_ALL)) {
		reason = 0x81;
	} else if (ppre->cur_prog_flag		&&
		   ((ppre->cur_width > default_width)	||
		    (ppre->cur_height > (default_height + 8))	||
		    (ppre->cur_inp_type & VIDTYPE_VIU_444))) {
		reason = 0x82;
	} else if ((ppre->cur_width < 128) || (ppre->cur_height < 16)) {
		reason = 0x83;
	} else if (ppre->cur_inp_type & VIDTYPE_MVC) {
		reason = 0x84;
	} else if (ppre->cur_source_type == VFRAME_SOURCE_TYPE_PPMGR) {
		reason = 0x85;
	} else if (dimp_get(edi_mp_bypass_3d)	&&
		  (ppre->source_trans_fmt != 0)) {
		reason = 0x86;
/*prot is conflict with di post*/
	} else if (vf_in && vf_in->video_angle) {
		reason = 0x87;
	}

	if (!reason	&&
	    vf_in	&&
	    (vf_in->fgs_valid	||
	     ppre->is_bypass_fg	||
	     dim_trig_fg)) {
		reason = 0x8b;
		ppre->is_bypass_fg = 1;
	}
	//dbg:
	if (fg_bypass != ppre->is_bypass_fg) {
		PR_INF("fg_bypass:%d->%d\n", fg_bypass, ppre->is_bypass_fg);
		fg_bypass = ppre->is_bypass_fg;
	}
	if (reason)
		return reason;

	pch = get_chdata(ch);
	if (dimp_get(edi_mp_bypass_trick_mode)) {
		int trick_mode_fffb = 0;
		int trick_mode_i = 0;

		if (dimp_get(edi_mp_bypass_trick_mode) & 0x1)
			query_video_status(0, &trick_mode_fffb);
		if (dimp_get(edi_mp_bypass_trick_mode) & 0x2)
			query_video_status(1, &trick_mode_i);
		trick_mode = trick_mode_fffb | (trick_mode_i << 1);
		if (trick_mode)
			reason = 0x88;
	}

	if (!reason && pch->rsc_bypass.d32)
		reason = 0x89;

	if (!reason	&&
	    vf_in	&&
	    pch->ponly	&&
	    IS_I_SRC(vf_in->type)) {
		reason = 0x8a;
		//PR_INF("%s:bypass for p only\n", __func__);
	}
	return reason;
}

//bool dim_need_bypass(struct vframe_s *vf);

/**********************************
 *diff with  dim_is_bypass
 *	delet di_vscale_skip_enable
 *	use vf_in replace ppre
 **********************************/
unsigned int is_bypass2(struct vframe_s *vf_in, unsigned int ch)
{
	unsigned int reason, x, y;
	struct di_ch_s *pch;
	static unsigned int last_bypass; //debug;

	reason = dim_bypass_check(vf_in);
	if (reason)
		goto tag_bypass;

	if (di_cfgx_get(ch, EDI_CFGX_BYPASS_ALL)) {	/*bypass_all*/
		reason = 0x81;
		goto tag_bypass;
	}
	/* check vframe */
	if (!vf_in)
		return 0;
	dim_vf_x_y(vf_in, &x, &y);

	if (x < 128 || y < 16) {
		reason = 0x82;
		goto tag_bypass;
	}
	if (dimp_get(edi_mp_bypass_3d)	&&
	    vf_in->trans_fmt != 0) {
		reason = 0x86;
		goto tag_bypass;
	}

/*prot is conflict with di post*/
	if (vf_in->video_angle) {
		reason = 0x87;
		goto tag_bypass;
	}
	if (vf_in->fgs_valid) {
		reason = 0x8b;
		goto tag_bypass;
	}
	pch = get_chdata(ch);
	reason = dim_polic_is_bypass(pch, vf_in);
	if (reason)
		goto tag_bypass;

	if (last_bypass != reason) {
		dbg_dbg("%s:1:0x%x->0x%x\n", __func__, last_bypass, reason);
		last_bypass = reason;
	}
	return 0;
tag_bypass:
	if (last_bypass != reason) {
		dbg_dbg("%s:2:0x%x->0x%x\n", __func__, last_bypass, reason);
		last_bypass = reason;
	}
	return reason;
}

static unsigned char is_bypass_post(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (dimp_get(edi_mp_di_debug_flag) & 0x40000) /* for debugging */
		return (dimp_get(edi_mp_di_debug_flag) >> 19) & 0x1;

	/*prot is conflict with di post*/
	if (ppre->orientation)
		return 1;
	if (dimp_get(edi_mp_bypass_post))
		return 1;

#ifdef DET3D
	if (ppre->vframe_interleave_flag != 0)
		return 1;

#endif
	return 0;
}

int dim_get_canvas(void)
{
	unsigned int pre_num = 7, post_num = 6;
//	struct di_dev_s  *de_devp = get_dim_de_devp();
	struct di_cvs_s *cvss = &get_datal()->cvs;

	if (dimp_get(edi_mp_mcpre_en)) {
		/* mem/chan2/nr/mtn/contrd/contrd2/
		 * contw/mcinfrd/mcinfow/mcvecw
		 */
		pre_num = 10;
		/* buf0/buf1/buf2/mtnp/mcvec */
		post_num = 6;
	}
	cvss->pre_num	= pre_num;
	cvss->post_num	= post_num;

	if (ops_ext()->cvs_alloc_table("di_pre",
				       &cvss->pre_idx[0][0],
				       pre_num,
				       CANVAS_MAP_TYPE_1)) {
		cvss->err_cnt++;
		PR_ERR("%s:pre\n", __func__);
		return 1;
	}
	cvss->en |= DI_CVS_EN_PRE;

	if (di_pre_rdma_enable) {
		if (ops_ext()->cvs_alloc_table("di_pre",
					       &cvss->pre_idx[1][0],
					       pre_num,
					       CANVAS_MAP_TYPE_1)) {
			PR_ERR("%s di pre 2.\n", __func__);
			cvss->err_cnt++;
			return 1;
		}
		cvss->en |= DI_CVS_EN_PRE2;
	} else {
		memcpy(&cvss->pre_idx[1][0],
		       &cvss->pre_idx[0][0], sizeof(int) * pre_num);
	}
	if (ops_ext()->cvs_alloc_table("di_post",
				       &cvss->post_idx[0][0],
				       post_num,
				       CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s:post\n", __func__);
		return 1;
	}
	cvss->en |= DI_CVS_EN_PST;
	if (ops_ext()->cvs_alloc_table("di_post",
				       &cvss->post_idx[1][0],
				       post_num,
				       CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s post 2\n", __func__);
		return 1;
	}
	cvss->en |= DI_CVS_EN_PST2;

	if (ops_ext()->cvs_alloc_table("di_inp",
				       &cvss->inp_idx[0],
				       3,
				       CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s inp\n", __func__);
		return 1;
	}
	cvss->en |= DI_CVS_EN_INP;
	PR_INF("cvs inp:0x%x~0x%x~0x%x.\n",
	       cvss->inp_idx[0], cvss->inp_idx[1], cvss->inp_idx[2]);

	return 0;
}

void dim_release_canvas(void)
{
	struct di_cvs_s *cvss = &get_datal()->cvs;

	if (cvss->en & DI_CVS_EN_PRE)
		ops_ext()->cvs_free_table(&cvss->pre_idx[0][0], cvss->pre_num);

	if (cvss->en & DI_CVS_EN_PRE2)
		ops_ext()->cvs_free_table(&cvss->pre_idx[1][0], cvss->pre_num);

	if (cvss->en & DI_CVS_EN_PST)
		ops_ext()->cvs_free_table(&cvss->post_idx[0][0],
					  cvss->post_num);

	if (cvss->en & DI_CVS_EN_PST2)
		ops_ext()->cvs_free_table(&cvss->post_idx[1][0],
					  cvss->post_num);

	if (cvss->en & DI_CVS_EN_INP)
		ops_ext()->cvs_free_table(&cvss->inp_idx[0], 3);

	if (cvss->en & DI_CVS_EN_DS)
		ops_ext()->cvs_free_table(&cvss->nr_ds_idx, 1);
	cvss->en = 0;
	PR_INF("%s:finish\n", __func__);
}

static void config_canvas_idx(struct di_buf_s *di_buf, int nr_canvas_idx,
			      int mtn_canvas_idx)
{
	unsigned int height = 0;

	if (!di_buf)
		return;
	if (di_buf->canvas_config_flag == 1) {
		if (nr_canvas_idx >= 0) {
			/* linked two interlace buffer should double height*/
			if (di_buf->di_wr_linked_buf)
				height = (di_buf->canvas_height << 1);
			else
				height =  di_buf->canvas_height;
			di_buf->nr_canvas_idx = nr_canvas_idx;
			canvas_config(nr_canvas_idx, di_buf->nr_adr,
				      di_buf->canvas_width[NR_CANVAS],
				      height, 0, 0);
		}
	} else if (di_buf->canvas_config_flag == 2) {
		if (nr_canvas_idx >= 0) {
			di_buf->nr_canvas_idx = nr_canvas_idx;
			canvas_config(nr_canvas_idx, di_buf->nr_adr,
				      di_buf->canvas_width[NR_CANVAS],
				      di_buf->canvas_height, 0, 0);
		}
		if (mtn_canvas_idx >= 0) {
			di_buf->mtn_canvas_idx = mtn_canvas_idx;
			canvas_config(mtn_canvas_idx, di_buf->mtn_adr,
				      di_buf->canvas_width[MTN_CANVAS],
				      di_buf->canvas_height, 0, 0);
		}
	}
	if (nr_canvas_idx >= 0) {
		di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
		di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
	}
}

static void config_canvas_idx_mtn(struct di_buf_s *di_buf,
				  int mtn_canvas_idx)
{
	if (!di_buf)
		return;

	if (di_buf->canvas_config_flag == 2) {
		if (mtn_canvas_idx >= 0) {
			di_buf->mtn_canvas_idx = mtn_canvas_idx;
			canvas_config(mtn_canvas_idx, di_buf->mtn_adr,
				      di_buf->canvas_width[MTN_CANVAS],
				      di_buf->canvas_height, 0, 0);
		}
	}
}

static void config_cnt_canvas_idx(struct di_buf_s *di_buf,
				  unsigned int cnt_canvas_idx)
{
	if (!di_buf)
		return;

	di_buf->cnt_canvas_idx = cnt_canvas_idx;
	canvas_config(cnt_canvas_idx, di_buf->cnt_adr,
		      di_buf->canvas_width[MTN_CANVAS],
		      di_buf->canvas_height, 0, 0);
}

static void config_mcinfo_canvas_idx(struct di_buf_s *di_buf,
				     int mcinfo_canvas_idx)
{
	if (!di_buf)
		return;

	di_buf->mcinfo_canvas_idx = mcinfo_canvas_idx;
	canvas_config(mcinfo_canvas_idx,
		      di_buf->mcinfo_adr,
		      di_buf->canvas_height_mc, 2, 0, 0);
}

static void config_mcvec_canvas_idx(struct di_buf_s *di_buf,
				    int mcvec_canvas_idx)
{
	if (!di_buf)
		return;

	di_buf->mcvec_canvas_idx = mcvec_canvas_idx;
	canvas_config(mcvec_canvas_idx,
		      di_buf->mcvec_adr,
		      di_buf->canvas_width[MV_CANVAS],
		      di_buf->canvas_height, 0, 0);
}

//----begin
int di_cnt_buf(int width, int height, int prog_flag, int mc_mm,
	       int bit10_support, int pack422)
{
	int canvas_height = height + 8;
	unsigned int di_buf_size = 0, di_post_buf_size = 0, mtn_size = 0;
	unsigned int nr_size = 0, count_size = 0, mv_size = 0, mc_size = 0;
	unsigned int nr_width = width, mtn_width = width, mv_width = width;
	unsigned int nr_canvas_width = width, mtn_canvas_width = width;
	unsigned int mv_canvas_width = width, canvas_align_width = 32;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	if (dimp_get(edi_mp_nr10bit_support)) {
		if (dimp_get(edi_mp_full_422_pack))
			nr_width = (width * 5) / 4;
		else
			nr_width = (width * 3) / 2;
	} else {
		nr_width = width;
	}
	/* make sure canvas width must be divided by 256bit|32byte align */
	nr_canvas_width = nr_width << 1;
	mtn_canvas_width = mtn_width >> 1;
	mv_canvas_width = (mv_width << 1) / 5;
	nr_canvas_width = roundup(nr_canvas_width, canvas_align_width);
	mtn_canvas_width = roundup(mtn_canvas_width, canvas_align_width);
	mv_canvas_width = roundup(mv_canvas_width, canvas_align_width);
	nr_width = nr_canvas_width >> 1;
	mtn_width = mtn_canvas_width << 1;
	mv_width = (mv_canvas_width * 5) >> 1;

	if (prog_flag) {
		di_buf_size = nr_width * canvas_height * 2;
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);

	} else {
		/*pr_info("canvas_height=%d\n", canvas_height);*/

		/*nr_size(bits)=w*active_h*8*2(yuv422)
		 * mtn(bits)=w*active_h*4
		 * cont(bits)=w*active_h*4 mv(bits)=w*active_h/5*16
		 * mcinfo(bits)=active_h*16
		 */
		nr_size = (nr_width * canvas_height) * 8 * 2 / 16;
		mtn_size = (mtn_width * canvas_height) * 4 / 16;
		count_size = (mtn_width * canvas_height) * 4 / 16;
		mv_size = (mv_width * canvas_height) / 5;
		/*mc_size = canvas_height;*/
		mc_size = roundup(canvas_height >> 1, canvas_align_width) << 1;
		if (mc_mm) {
			di_buf_size = nr_size + mtn_size + count_size +
				mv_size + mc_size;
		} else {
			di_buf_size = nr_size + mtn_size + count_size;
		}
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);
	}

	PR_INF("size:0x%x\n", di_buf_size);
	PR_INF("\t%-15s:0x%x\n", "nr_size", nr_size);
	PR_INF("\t%-15s:0x%x\n", "count", count_size);
	PR_INF("\t%-15s:0x%x\n", "mtn", mtn_size);
	PR_INF("\t%-15s:0x%x\n", "mv", mv_size);
	PR_INF("\t%-15s:0x%x\n", "mcinfo", mc_size);

	di_post_buf_size = nr_width * canvas_height * 2;
	/*PR_INF("size:post:0x%x\n", di_post_buf_size);*/
	di_post_buf_size = roundup(di_post_buf_size, PAGE_SIZE);
	PR_INF("size:post:0x%x\n", di_post_buf_size);

	return 0;
}

unsigned int insert_line; //= 0x100;
module_param(insert_line, uint, 0664);
MODULE_PARM_DESC(insert_line, "debug insert_line");

static unsigned int di_cnt_pre_afbct(struct di_ch_s *pch)
{
	unsigned int afbc_info_size = 0, afbc_tab_size = 0;
	unsigned int afbc_buffer_size = 0, blk_total = 0;
	unsigned int width, height;

	width = 1920;
	height = 1088;

	if (dim_afds() && dim_afds()->cnt_info_size &&
	    (cfggch(pch, DAT) & DI_BIT1)) {
		afbc_info_size = dim_afds()->cnt_info_size(width,
							   height / 2,
							   &blk_total);
		afbc_buffer_size = dim_afds()->cnt_buf_size(0x21,
							    blk_total);

		afbc_tab_size = dim_afds()->cnt_tab_size(afbc_buffer_size);
	} else {
		afbc_tab_size = 0;
	}

	return afbc_tab_size;
}

static int di_cnt_i_buf(struct di_ch_s *pch, int width, int height)
{
	unsigned int canvas_height = height;
	enum EDPST_MODE mode;

	unsigned int di_buf_size = 0, mtn_size = 0;
	unsigned int nr_size = 0, count_size = 0, mv_size = 0, mc_size = 0;
	unsigned int nr_width = width, mtn_width = width, mv_width = width;
	unsigned int nr_canvas_width = width, mtn_canvas_width = width;
	unsigned int mv_canvas_width = width, canvas_align_width = 32;
	unsigned int afbc_buffer_size = 0, blk_total = 0;
	struct div2_mm_s *mm;
	unsigned int ch;
	unsigned int afbc_info_size = 0, afbc_tab_size = 0, old_size;
	//unsigned int insert_line = 544;
	unsigned int insert_size;

	unsigned int one_idat_size = 0;
	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	ch = pch->ch_id;
	mode = pch->mode;
	mm = dim_mm_get(ch);

	if (mode == EDPST_MODE_422_10BIT_PACK)
		nr_width = (width * 5) / 4;
	else if (mode == EDPST_MODE_422_10BIT)
		nr_width = (width * 3) / 2;
	else
		nr_width = width;
	//dbg_reg("%s:begin\n", __func__);

	/* make sure canvas width must be divided by 256bit|32byte align */
	nr_canvas_width = nr_width << 1;
	mtn_canvas_width = mtn_width >> 1;
	mv_canvas_width = (mv_width << 1) / 5;
	nr_canvas_width = roundup(nr_canvas_width, canvas_align_width);
	mtn_canvas_width = roundup(mtn_canvas_width, canvas_align_width);
	mv_canvas_width = roundup(mv_canvas_width, canvas_align_width);
	nr_width = nr_canvas_width >> 1;
	mtn_width = mtn_canvas_width << 1;
	mv_width = (mv_canvas_width * 5) >> 1;
//	rot_width = roundup((rot_width << 1), canvas_align_width);

	/* tvp flg */
	if (get_flag_tvp(ch) == 2) {
		mm->sts.flg_tvp = 1;
		mm->cfg.ibuf_flg.b.tvp = 1;
		mm->cfg.pbuf_flg.b.tvp = 1;
	} else {
		mm->sts.flg_tvp = 0;
		mm->cfg.ibuf_flg.b.tvp = 0;
		mm->cfg.pbuf_flg.b.tvp = 0;
	}

	dbg_ev("%s1:tvp:%d\n", __func__, mm->cfg.pbuf_flg.b.tvp);
	if (dim_afds() && dim_afds()->cnt_info_size &&
	    (dim_afds()->is_sts(EAFBC_MEMI_NEED) || cfgg(FIX_BUF))) {
		afbc_info_size = dim_afds()->cnt_info_size(width,
							   height / 2,
							   &blk_total);
		afbc_buffer_size = dim_afds()->cnt_buf_size(0x21,
							    blk_total);
	}

	mm->cfg.buf_alloc_mode = 0;
	/*nr_size(bits) = w * active_h * 8 * 2(yuv422)
	 * mtn(bits) = w * active_h * 4
	 * cont(bits) = w * active_h * 4 mv(bits) = w * active_h / 5*16
	 * mcinfo(bits) = active_h * 16
	 */
	nr_size = (nr_width * canvas_height) * 8 * 2 / 16;
	if (afbc_buffer_size > nr_size) {
		PR_INF("%s:0x%x, 0x%x\n", "bufi large:",
		       nr_size, afbc_buffer_size);
		nr_size = afbc_buffer_size;
	}

	if (dim_afds() && dim_afds()->cnt_tab_size &&
	    (dim_afds()->is_sts(EAFBC_MEM_NEED) || cfgg(FIX_BUF)))
		afbc_tab_size = dim_afds()->cnt_tab_size(nr_size);

	mtn_size = (mtn_width * canvas_height) * 4 / 16;
	count_size = (mtn_width * canvas_height) * 4 / 16;
	mv_size = (mv_width * canvas_height) / 5;
	/*mc_size = canvas_height;*/
	mc_size = roundup(canvas_height >> 1, canvas_align_width) << 1;

#ifdef CFG_BUF_ALLOC_SP
	insert_size = insert_line * nr_canvas_width;

	di_buf_size	= nr_size;
	if (mc_mem_alloc) {
		di_buf_size += mtn_size +
				count_size +
				mv_size +
				mc_size;
		//one_idat_size = mc_size;
	} else {
		di_buf_size += mtn_size +
				count_size;
		//one_idat_size = 0;
	}
	mm->cfg.afbct_local_max_size = di_cnt_pre_afbct(pch);
	mm->cfg.ibuf_hsize	= width;
	one_idat_size += mm->cfg.afbct_local_max_size;

	old_size	= nr_size;
	di_buf_size = roundup(di_buf_size, PAGE_SIZE);
	di_buf_size += afbc_info_size;

	di_buf_size = roundup(di_buf_size, PAGE_SIZE);
#else
	if (mc_mem_alloc) {
		di_buf_size = nr_size + mtn_size + count_size +
			mv_size + mc_size;
	} else {
		di_buf_size = nr_size + mtn_size + count_size;
	}

	insert_size = insert_line * nr_canvas_width;
	di_buf_size += insert_size;
	di_buf_size = roundup(di_buf_size, PAGE_SIZE);
	old_size = di_buf_size;
	di_buf_size += afbc_info_size;
	di_buf_size += afbc_tab_size;
#endif
	dbg_init("nr_width=%d,cvs_h=%d,w[%d],h[%d],al_w[%d]\n",
		 nr_width, canvas_height,
		 width, height,
		 canvas_align_width);
	/*de_devp->buffer_size = di_buf_size;*/
	mm->cfg.size_local = di_buf_size;
	mm->cfg.size_local_page = mm->cfg.size_local >> PAGE_SHIFT;
	mm->cfg.nr_size = nr_size;
	mm->cfg.count_size = count_size;
	mm->cfg.mtn_size = mtn_size;
	mm->cfg.mv_size = mv_size;
	mm->cfg.mcinfo_size = mc_size;
	mm->cfg.di_size = old_size;
	mm->cfg.afbci_size = afbc_info_size;
	mm->cfg.afbct_size = afbc_tab_size;
	mm->cfg.pre_inser_size = insert_size;

#ifdef CFG_BUF_ALLOC_SP
	mm->cfg.size_idat_one	= one_idat_size;
	mm->cfg.nub_idat	= DIM_IAT_NUB;
	mm->cfg.size_idat_all	= mm->cfg.size_idat_one * mm->cfg.nub_idat;
	mm->cfg.size_idat_all	+= mm->cfg.pre_inser_size;
	mm->cfg.size_idat_all = roundup(mm->cfg.size_idat_all, PAGE_SIZE);
	/* cfg i dat buf flg */
	mm->cfg.dat_idat_flg.b.page = mm->cfg.size_idat_all >> PAGE_SHIFT;
	mm->cfg.dat_idat_flg.b.is_i = 1;
	mm->cfg.dat_idat_flg.b.afbc = 1;
	mm->cfg.dat_idat_flg.b.dw = 0;
	mm->cfg.dat_idat_flg.b.tvp	= 0;
	mm->cfg.dat_idat_flg.b.typ	= EDIM_BLK_TYP_DATI;
#endif
	dimp_set(edi_mp_same_field_top_count, 0);
	same_field_bot_count = 0;
	#ifdef PRINT_BASIC
	dbg_mem2("%s:size:\n", __func__);
	dbg_mem2("\t%-15s:0x%x\n", "nr_size", mm->cfg.nr_size);
	dbg_mem2("\t%-15s:0x%x\n", "count", mm->cfg.count_size);
	dbg_mem2("\t%-15s:0x%x\n", "mtn", mm->cfg.mtn_size);
	dbg_mem2("\t%-15s:0x%x\n", "mv", mm->cfg.mv_size);
	dbg_mem2("\t%-15s:0x%x\n", "mcinfo", mm->cfg.mcinfo_size);
	dbg_mem2("\t%-15s:0x%x\n", "insert_size", mm->cfg.pre_inser_size);
	dbg_mem2("\t%-15s:0x%x\n", "afbci_size", mm->cfg.afbci_size);
	dbg_mem2("\t%-15s:0x%x\n", "afbct_size", mm->cfg.afbct_size);
	dbg_mem2("\t%-15s:0x%x\n", "dw_size", mm->cfg.dw_size);
	dbg_mem2("\t%-15s:0x%x\n", "size_local", mm->cfg.size_local);
	dbg_mem2("\t%-15s:0x%x\n", "size_page", mm->cfg.size_local_page);
	dbg_mem2("\t%-15s:0x%x\n", "afbct_local_max_size",
		 mm->cfg.afbct_local_max_size);
	dbg_mem2("\t%-15s:0x%x\n", "size_idat_one", mm->cfg.size_idat_one);
	dbg_mem2("\t%-15s:0x%x\n", "nub_idat", mm->cfg.nub_idat);
	dbg_mem2("\t%-15s:0x%x\n", "size_idat_all", mm->cfg.size_idat_all);
	dbg_mem2("\t%-15s:0x%x\n", "idat_pate", mm->cfg.dat_idat_flg.b.page);
	#endif
	mm->cfg.canvas_width[NR_CANVAS]		= nr_canvas_width;
	mm->cfg.canvas_width[MTN_CANVAS]	= mtn_canvas_width;
	mm->cfg.canvas_width[MV_CANVAS]		= mv_canvas_width;
	mm->cfg.canvas_height			= (canvas_height >> 1);
	mm->cfg.canvas_height_mc = roundup(mm->cfg.canvas_height,
					   canvas_align_width);
	dbg_mem2("\t%-15s:0x%x\n", "canvas_height_mc",
		 mm->cfg.canvas_height_mc);

	/* cfg i/p buf flg */
	mm->cfg.ibuf_flg.b.page = mm->cfg.size_local_page;
	mm->cfg.ibuf_flg.b.is_i = 1;
	if (mm->cfg.afbci_size)
		mm->cfg.ibuf_flg.b.afbc = 1;
	else
		mm->cfg.ibuf_flg.b.afbc = 0;
	if (mm->cfg.dw_size)
		mm->cfg.ibuf_flg.b.dw = 1;
	else
		mm->cfg.ibuf_flg.b.dw = 0;

	mm->cfg.ibuf_flg.b.typ	= EDIM_BLK_TYP_OLDI;
	return 0;
}

static void di_cnt_pst_afbct(struct di_ch_s *pch)
{
#ifdef CFG_BUF_ALLOC_SP

	/* cnt the largest size to avoid rebuild */
	unsigned int height, width, ch;
	bool is_4k = false;
	const struct di_mm_cfg_s *pcfg;
	unsigned int afbc_buffer_size = 0, afbc_tab_size = 0;
	unsigned int afbc_info_size = 0, blk_total = 0, tsize;
	bool flg_afbc = false;
	struct div2_mm_s *mm;

	if (!pch)
		return;
	ch = pch->ch_id;
	mm = dim_mm_get(ch);

	if (dim_afds()		&&
	    dim_afds()->cnt_info_size &&
	    (cfgg(DAT) & DI_BIT0))
		flg_afbc = true;

	if (!flg_afbc) {
		mm->cfg.size_pafbct_all	= 0;
		mm->cfg.size_pafbct_one	= 0;
		mm->cfg.nub_pafbct	= 0;
		mm->cfg.dat_pafbct_flg.d32 = 0;
		return;
	}

	//if (cfgg(4K))
	is_4k = true;

	pcfg = di_get_mm_tab(is_4k, pch);
	width	= pcfg->di_w;
	height	= pcfg->di_h;

	afbc_info_size = dim_afds()->cnt_info_size(width,
						   height,
						   &blk_total);
	afbc_buffer_size =
		dim_afds()->cnt_buf_size(0x21, blk_total);
	afbc_buffer_size = roundup(afbc_buffer_size, PAGE_SIZE);
	tsize = afbc_buffer_size + afbc_info_size;
	afbc_tab_size =
		dim_afds()->cnt_tab_size(tsize);

	mm->cfg.nub_pafbct	= DIM_PAT_NUB - 1;/*pcfg->num_post*/
	mm->cfg.size_pafbct_all = afbc_tab_size * mm->cfg.nub_pafbct;
	mm->cfg.size_pafbct_one = afbc_tab_size;

	mm->cfg.dat_pafbct_flg.d32	= 0;
	//mm->cfg.dat_pafbct_flg.b.afbc	= 1;
	mm->cfg.dat_pafbct_flg.b.typ	= EDIM_BLK_TYP_PAFBCT;
	mm->cfg.dat_pafbct_flg.b.page	= mm->cfg.size_pafbct_all >> PAGE_SHIFT;
	mm->cfg.dat_pafbci_flg.b.tvp	= 0;
	dbg_mem2("%s:size_pafbct_all:0x%x, 0x%x, nub[%d]\n",
		 __func__,
		 mm->cfg.size_pafbct_all,
		 mm->cfg.size_pafbct_one,
		 mm->cfg.nub_pafbct);
#endif
}

/* width, height from mm*/
static int di_cnt_post_buf(struct di_ch_s *pch /*,enum EDPST_OUT_MODE mode*/)
{
	unsigned int ch;
	struct div2_mm_s *mm;
	unsigned int nr_width, nr_canvas_width, canvas_align_width = 32;
	unsigned int height, width;
	unsigned int tmpa, tmpb;
	unsigned int canvas_height;
	unsigned int afbc_info_size = 0, afbc_tab_size = 0;
	unsigned int afbc_buffer_size = 0, blk_total = 0;
	enum EDPST_MODE mode;
	bool is_4k = false;
	bool is_yuv420_10 = false;
	unsigned int canvas_align_width_hf = 64;
	ch = pch->ch_id;
	mm = dim_mm_get(ch);

	height	= mm->cfg.di_h;
	canvas_height = roundup(height, 32);
	width	= mm->cfg.di_w;
	if (mm->cfg.di_w > 1920)
		is_4k = true;
	mm->cfg.pbuf_hsize = width;
	nr_width = width;
	dbg_mem2("%s w[%d]h[%d]\n", __func__, width, height);
	nr_canvas_width = width;
	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	/***********************/
	mode = pch->mode;
	if (is_4k &&
	    ((cfggch(pch, POUT_FMT) == 1)	||
	     (cfggch(pch, POUT_FMT) == 2)	||
	     (cfggch(pch, POUT_FMT) == 5)))
		mode = EDPST_MODE_NV21_8BIT;
	dbg_mem2("%s:mode:%d\n", __func__, mode);

	/***********************/
	if (mode == EDPST_MODE_422_10BIT_PACK)
		nr_width = (width * 5) / 4;
	else if (mode == EDPST_MODE_422_10BIT)
		nr_width = (width * 3) / 2;
	else
		nr_width = width;

	/* p */
	//tmp nr_width = roundup(nr_width, canvas_align_width);
	mm->cfg.pst_mode = mode;
	if (mode == EDPST_MODE_NV21_8BIT) {
		nr_width = roundup(nr_width, canvas_align_width);
		tmpa = (nr_width * canvas_height) >> 1;/*uv*/
		tmpb = tmpa;
		tmpa = nr_width * canvas_height;

		mm->cfg.pst_buf_uv_size = roundup(tmpb, PAGE_SIZE);
		mm->cfg.pst_buf_y_size	= roundup(tmpa, PAGE_SIZE);
		mm->cfg.pst_buf_size	= mm->cfg.pst_buf_y_size +
				mm->cfg.pst_buf_uv_size;//tmpa + tmpb;
		mm->cfg.size_post	= mm->cfg.pst_buf_size;
		mm->cfg.pst_cvs_w	= nr_width;
		mm->cfg.pst_cvs_h	= canvas_height;
		mm->cfg.pst_afbci_size	= 0;
		mm->cfg.pst_afbct_size	= 0;
	} else {
		/* 422 */
		tmpa = roundup(nr_width * canvas_height * 2, PAGE_SIZE);
		mm->cfg.pst_buf_size	= tmpa;
		mm->cfg.pst_buf_uv_size	= tmpa >> 1;
		mm->cfg.pst_buf_y_size	= mm->cfg.pst_buf_uv_size;

		if (dim_afds() && dim_afds()->cnt_info_size &&
		    ((dim_afds()->is_sts(EAFBC_MEM_NEED) &&
		     (!mm->cfg.dis_afbce)) ||
		     cfgg(FIX_BUF))) {
			afbc_info_size =
				dim_afds()->cnt_info_size(width,
							  height,
							  &blk_total);
			if (is_4k && dip_is_support_nv2110(ch))
				is_yuv420_10 = true;

			if (is_yuv420_10)
				afbc_buffer_size =
				dim_afds()->cnt_buf_size(0x20, blk_total);
			else
				afbc_buffer_size =
					dim_afds()->cnt_buf_size(0x21, blk_total);
			afbc_buffer_size = roundup(afbc_buffer_size, PAGE_SIZE);
			if (afbc_buffer_size > mm->cfg.pst_buf_size) {
				PR_INF("%s:0x%x, 0x%x\n", "buf large:",
				       mm->cfg.pst_buf_size,
				       afbc_buffer_size);
				mm->cfg.pst_buf_size = afbc_buffer_size;
			}

			if (is_yuv420_10)
				mm->cfg.pst_buf_size = afbc_buffer_size;

			afbc_tab_size =
				dim_afds()->cnt_tab_size(mm->cfg.pst_buf_size);
		}
		mm->cfg.pst_afbci_size	= afbc_info_size;
		mm->cfg.pst_afbct_size	= afbc_tab_size;

		#ifdef CFG_BUF_ALLOC_SP
		if (is_4k && dip_is_4k_sct_mem(pch)) {
			mm->cfg.size_post	= mm->cfg.pst_afbci_size;
		} else if (pch->ponly && dip_is_ponly_sct_mem(pch)) {
			mm->cfg.size_post	= mm->cfg.pst_afbci_size;
		} else if (dip_itf_is_ins_exbuf(pch)) {
			mm->cfg.size_post	= mm->cfg.pst_afbci_size;
		} else {
			mm->cfg.size_post	= mm->cfg.pst_buf_size +
					mm->cfg.pst_afbci_size;
		}
		#else
		mm->cfg.size_post	= mm->cfg.pst_buf_size +
			mm->cfg.pst_afbci_size + mm->cfg.pst_afbct_size;

		#endif
		mm->cfg.pst_cvs_w	= nr_width << 1;
		mm->cfg.pst_cvs_w	= roundup(mm->cfg.pst_cvs_w,
						  canvas_align_width);
		mm->cfg.pst_cvs_h	= canvas_height;
	}
	if (pch->en_hf_buf) {
		width = roundup(1920, canvas_align_width_hf);
		mm->cfg.size_buf_hf = width * 1080;
		mm->cfg.size_buf_hf	= PAGE_ALIGN(mm->cfg.size_buf_hf);
		mm->cfg.hf_hsize = width;
		mm->cfg.hf_vsize = 1080;
		//mm->cfg.size_hf = di_hf_cnt_size(width, height, is_4k);
	} else {
		mm->cfg.size_buf_hf = 0;
	}

	if (pch->en_dw &&
	    ((cfggch(pch, DW_EN) == 1 && is_4k) ||
	    (cfggch(pch, DW_EN) == 2))) {
		mm->cfg.dw_size = dim_getdw()->size_info.p.size_total;
		mm->cfg.size_post += mm->cfg.dw_size;
		pch->en_dw_mem = true;
	} else {
		mm->cfg.dw_size = 0;
	}

	//mm->cfg.size_post += mm->cfg.size_buf_hf;

	mm->cfg.size_post	= roundup(mm->cfg.size_post, PAGE_SIZE);
	mm->cfg.size_post_page	= mm->cfg.size_post >> PAGE_SHIFT;

	/* p */
	mm->cfg.pbuf_flg.b.page = mm->cfg.size_post_page;
	mm->cfg.pbuf_flg.b.is_i = 0;
	if (mm->cfg.pst_afbct_size)
		mm->cfg.pbuf_flg.b.afbc = 1;
	else
		mm->cfg.pbuf_flg.b.afbc = 0;
	if (mm->cfg.dw_size)
		mm->cfg.pbuf_flg.b.dw = 1;
	else
		mm->cfg.pbuf_flg.b.dw = 0;

	if (is_4k && dip_is_4k_sct_mem(pch))
		mm->cfg.pbuf_flg.b.typ = EDIM_BLK_TYP_PSCT;
	else if (pch->ponly && dip_is_ponly_sct_mem(pch))
		mm->cfg.pbuf_flg.b.typ = EDIM_BLK_TYP_PSCT;
	else
		mm->cfg.pbuf_flg.b.typ = EDIM_BLK_TYP_OLDP;

	if (dip_itf_is_ins_exbuf(pch)) {
		mm->cfg.pbuf_flg.b.typ |= EDIM_BLK_TYP_POUT;
		mm->cfg.size_buf_hf = 0;
	}
	dbg_mem2("hf:size:%d\n", mm->cfg.size_buf_hf);

	dbg_mem2("%s:3 pst_cvs_w[%d]\n", __func__, mm->cfg.pst_cvs_w);
	#ifdef PRINT_BASIC
	dbg_mem2("%s:size:\n", __func__);
	dbg_mem2("\t%-15s:0x%x\n", "total_size", mm->cfg.size_post);
	dbg_mem2("\t%-15s:0x%x\n", "total_page", mm->cfg.size_post_page);
	dbg_mem2("\t%-15s:0x%x\n", "buf_size", mm->cfg.pst_buf_size);
	dbg_mem2("\t%-15s:0x%x\n", "y_size", mm->cfg.pst_buf_y_size);
	dbg_mem2("\t%-15s:0x%x\n", "uv_size", mm->cfg.pst_buf_uv_size);
	dbg_mem2("\t%-15s:0x%x\n", "afbci_size", mm->cfg.pst_afbci_size);
	dbg_mem2("\t%-15s:0x%x\n", "afbct_size", mm->cfg.pst_afbct_size);
	dbg_mem2("\t%-15s:0x%x\n", "flg", mm->cfg.pbuf_flg.d32);
	dbg_mem2("\t%-15s:0x%x\n", "dw_size", mm->cfg.dw_size);
	dbg_mem2("\t%-15s:0x%x\n", "size_hf", mm->cfg.size_buf_hf);
	#endif
	return 0;
}

static int di_init_buf_simple(struct di_ch_s *pch)
{
	int i, j;

	struct vframe_s **pvframe_in;
	struct vframe_s *pvframe_in_dup;
	struct vframe_s *pvframe_local;
	struct vframe_s *pvframe_post;
	struct di_buf_s *pbuf_local;
	struct di_buf_s *pbuf_in;
	struct di_buf_s *pbuf_post;
	struct di_buf_s *di_buf;

	struct div2_mm_s *mm; /*mm-0705*/
	unsigned int cnt;

	unsigned int ch;
	unsigned int length;
	u8 *tmp_meta;
	u8 *channel_meta_addr;
	u8 *tmp_ud;
	u8 *channel_ud_addr;
	struct di_dev_s *de_devp = get_dim_de_devp();

	/**********************************************/
	/* count buf info */
	/**********************************************/
	//if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
	//	canvas_align_width = 64;

	ch = pch->ch_id;
	pvframe_in	= get_vframe_in(ch);
	pvframe_in_dup	= get_vframe_in_dup(ch);
	pvframe_local	= get_vframe_local(ch);
	pvframe_post	= get_vframe_post(ch);
	pbuf_local	= get_buf_local(ch);
	pbuf_in		= get_buf_in(ch);
	pbuf_post	= get_buf_post(ch);
	mm		= dim_mm_get(ch);

	//dbg_reg("%s:begin\n", __func__);

	/* decoder'buffer had been releae no need put */
	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		pvframe_in[i] = NULL;

	/**********************************************/
	/* que init */
	/**********************************************/

	queue_init(ch, MAX_LOCAL_BUF_NUM * 2);
	di_que_init(ch); /*new que*/

	//mem_st_local = di_get_mem_start(ch);

	/**********************************************/
	/* local buf init */
	/**********************************************/
	tmp_meta = de_devp->local_meta_addr +
		((de_devp->local_meta_size / DI_CHANNEL_NUB) * ch);
	channel_meta_addr = tmp_meta;

	tmp_ud = de_devp->local_ud_addr +
		((de_devp->local_ud_size / DI_CHANNEL_NUB) * ch);
	channel_ud_addr = tmp_ud;

	//for (i = 0; i < mm->cfg.num_local; i++) {
	for (i = 0; i < (MAX_LOCAL_BUF_NUM * 2); i++) {
		di_buf = &pbuf_local[i];

		/* backup cma pages */
		//tmp_page = di_buf->pages;
		memset(di_buf, 0, sizeof(struct di_buf_s));
		//di_buf->pages = tmp_page;
		di_buf->type = VFRAME_TYPE_LOCAL;
		di_buf->pre_ref_count = 0;
		di_buf->post_ref_count = 0;
		for (j = 0; j < 3; j++)
			di_buf->canvas_width[j] = mm->cfg.canvas_width[j];

		di_buf->index = i;
		di_buf->flg_null = 1;
		di_buf->vframe = &pvframe_local[i];
		di_buf->vframe->private_data = di_buf;
		di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
		di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
		di_buf->queue_index = -1;
		di_buf->invert_top_bot_flag = 0;
		di_buf->channel = ch;
		di_buf->canvas_config_flag = 2;
		//queue_in(channel, di_buf, QUEUE_LOCAL_FREE);
		if (de_devp->local_meta_addr) {
			di_buf->local_meta = tmp_meta;
			di_buf->local_meta_total_size =
				LOCAL_META_BUFF_SIZE;
		} else {
			di_buf->local_meta = NULL;
			di_buf->local_meta_total_size = 0;
		}
		di_buf->local_meta_used_size = 0;

		if (de_devp->local_ud_addr) {
			di_buf->local_ud = tmp_ud;
			di_buf->local_ud_total_size =
				LOCAL_UD_BUFF_SIZE;
		} else {
			di_buf->local_ud = NULL;
			di_buf->local_ud_total_size = 0;
		}
		di_buf->local_ud_used_size = 0;
		di_que_in(ch, QUE_PRE_NO_BUF, di_buf);
		dbg_init("buf[%d], addr=0x%lx\n", di_buf->index,
			 di_buf->nr_adr);
		if (tmp_meta)
			tmp_meta += LOCAL_META_BUFF_SIZE;
		if (tmp_ud)
			tmp_ud += LOCAL_UD_BUFF_SIZE;
	}

	/**********************************************/
	/* input buf init */
	/**********************************************/
	tmp_meta = channel_meta_addr +
			(MAX_LOCAL_BUF_NUM * LOCAL_META_BUFF_SIZE * 2);
	tmp_ud = channel_ud_addr +
			(MAX_LOCAL_BUF_NUM * LOCAL_UD_BUFF_SIZE * 2);

	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		di_buf = &pbuf_in[i];

		if (di_buf) {
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->type = VFRAME_TYPE_IN;
			di_buf->pre_ref_count = 0;
			di_buf->post_ref_count = 0;
			di_buf->vframe = &pvframe_in_dup[i];
			di_buf->vframe->private_data = di_buf;
			di_buf->index = i;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			di_buf->channel = ch;
			if (de_devp->local_meta_addr) {
				di_buf->local_meta = tmp_meta;
				di_buf->local_meta_total_size =
					LOCAL_META_BUFF_SIZE;
			} else {
				di_buf->local_meta = NULL;
				di_buf->local_meta_total_size = 0;
			}
			di_buf->local_meta_used_size = 0;

			if (de_devp->local_ud_addr) {
				di_buf->local_ud = tmp_ud;
				di_buf->local_ud_total_size =
					LOCAL_UD_BUFF_SIZE;
			} else {
				di_buf->local_ud = NULL;
				di_buf->local_ud_total_size = 0;
			}
			di_buf->local_ud_used_size = 0;
			di_que_in(ch, QUE_IN_FREE, di_buf);
		}
		if (tmp_meta)
			tmp_meta += LOCAL_META_BUFF_SIZE;
		if (tmp_ud)
			tmp_ud += LOCAL_UD_BUFF_SIZE;
	}
	/**********************************************/
	/* post buf init */
	/**********************************************/
	tmp_meta = channel_meta_addr +
		((MAX_IN_BUF_NUM + (MAX_LOCAL_BUF_NUM * 2)) *
		 LOCAL_META_BUFF_SIZE);
	tmp_ud = channel_ud_addr +
		((MAX_IN_BUF_NUM + (MAX_LOCAL_BUF_NUM * 2)) *
		 LOCAL_UD_BUFF_SIZE);
	cnt = 0;
	for (i = 0; i < MAX_POST_BUF_NUM; i++) {
		di_buf = &pbuf_post[i];

		if (di_buf) {
#ifdef MARK_HIS
			if (dimp_get(edi_mp_post_wr_en) &&
			    dimp_get(edi_mp_post_wr_support)) {
				if (di_que_is_in_que(ch, QUE_POST_KEEP,
						     di_buf)) {
					dbg_reg("%s:post keep buf %d\n",
						__func__,
						di_buf->index);
					dbg_wq("k:b[%d]\n", di_buf->index);
					continue;
				}
			}
#endif
			memset(di_buf, 0, sizeof(struct di_buf_s));

			di_buf->type = VFRAME_TYPE_POST;
			di_buf->index = i;
			di_buf->vframe = &pvframe_post[i];
			di_buf->vframe->private_data = di_buf;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			di_buf->channel = ch;
			di_buf->flg_null = 1;
			if (de_devp->local_meta_addr) {
				di_buf->local_meta = tmp_meta;
				di_buf->local_meta_total_size =
					LOCAL_META_BUFF_SIZE;
			} else {
				di_buf->local_meta = NULL;
				di_buf->local_meta_total_size = 0;
			}
			di_buf->local_meta_used_size = 0;

			if (de_devp->local_ud_addr) {
				di_buf->local_ud = tmp_ud;
				di_buf->local_ud_total_size =
					LOCAL_UD_BUFF_SIZE;
			} else {
				di_buf->local_ud = NULL;
				di_buf->local_ud_total_size = 0;
			}
			di_buf->local_ud_used_size = 0;
			if (dimp_get(edi_mp_post_wr_en) &&
			    dimp_get(edi_mp_post_wr_support)) {
				di_buf->canvas_width[NR_CANVAS] =
					mm->cfg.pst_cvs_w;
				//di_buf->canvas_rot_w = rot_width;
				di_buf->canvas_height = mm->cfg.pst_cvs_h;
				di_buf->canvas_config_flag = 1;

				dbg_init("[%d]post buf:%d: addr=0x%lx\n", i,
					 di_buf->index, di_buf->nr_adr);
			}
			cnt++;
			//di_que_in(channel, QUE_POST_FREE, di_buf);
			di_que_in(ch, QUE_PST_NO_BUF, di_buf);

		} else {
			PR_ERR("%s:%d:post buf is null\n", __func__, i);
		}
		if (tmp_meta)
			tmp_meta += LOCAL_META_BUFF_SIZE;
		if (tmp_ud)
			tmp_ud += LOCAL_UD_BUFF_SIZE;
	}
	/* check */
	length = di_que_list_count(ch, QUE_PST_NO_BUF);

	di_buf = di_que_out_to_di_buf(ch, QUE_PST_NO_BUF);
	dbg_mem2("%s:ch[%d]:pst_no_buf:%d, indx[%d]\n", __func__,
		 ch, length, di_buf->index);
	return 0;
}

static void check_tvp_state(struct di_ch_s *pch)
{
#ifdef TMP_TEST /* for ko only */
	set_flag_tvp(pch->ch_id, 1);
#else
	unsigned int ch;
	struct provider_state_req_s req;
	char *provider_name;// = vf_get_provider_name(pch->vfm.name);

	ch = pch->ch_id;
	if (!dip_itf_is_vfm(pch)) {
		set_flag_secure_pre(0);
		set_flag_secure_pst(0);
		if (pch->itf.u.dinst.parm.output_format & DI_OUTPUT_TVP)
			set_flag_tvp(pch->ch_id, 2);
		else
			set_flag_tvp(pch->ch_id, 1);
		return;
	}

	set_flag_tvp(ch, 0);
	set_flag_secure_pre(0);
	set_flag_secure_pst(0);
	provider_name = vf_get_provider_name(pch->itf.dvfm.name);

	while (provider_name) {
		if (!vf_get_provider_name(provider_name))
			break;
		provider_name =
			vf_get_provider_name(provider_name);
	}
	if (provider_name) {
		req.vf = NULL;
		req.req_type = REQ_STATE_SECURE;
		req.req_result[0] = 0xffffffff;
		vf_notify_provider_by_name
			(provider_name,
			 VFRAME_EVENT_RECEIVER_REQ_STATE,
			 (void *)&req);
		if (req.req_result[0] == 0)
			set_flag_tvp(ch, 1);
		else if (req.req_result[0] != 0xffffffff)
			set_flag_tvp(ch, 2);
	}
	dbg_mem2("%s:tvp1:%d\n", __func__, get_flag_tvp(ch));
	if (get_flag_tvp(ch) == 0) {
		if (codec_mm_video_tvp_enabled())
			set_flag_tvp(ch, 2);
		else
			set_flag_tvp(ch, 1);
	}
	dbg_mem2("%s:tvp2:%d\n", __func__, get_flag_tvp(ch));
#endif
}

static int di_init_buf_new(struct di_ch_s *pch, struct vframe_s *vframe)
{
	struct mtsk_cmd_s blk_cmd;
	//struct di_dev_s *de_devp = get_dim_de_devp();
	struct div2_mm_s *mm;
	unsigned int ch;
	unsigned int length_keep;
//	struct di_dat_s *pdat;
	unsigned int tmp_nub;

	ch = pch->ch_id;
	mm = dim_mm_get(ch);

	check_tvp_state(pch);
	di_cnt_i_buf(pch, 1920, 1088);
	//di_cnt_i_buf(pch, 1920, 2160);
	di_cnt_pst_afbct(pch);
	di_cnt_post_buf(pch);
	di_init_buf_simple(pch);

	length_keep = ndis_cnt(pch, QBF_NDIS_Q_KEEP);
	//di_que_list_count(ch, QUE_POST_KEEP);
	mm->cfg.num_rebuild_alloc = 0;

	if (cfgeq(MEM_FLAG, EDI_MEM_M_CMA)	||
	    cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_A)	||
	    cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_B)) {	/*trig cma alloc*/
		dbg_timer(ch, EDBG_TIMER_ALLOC);

		if (is_bypass2(vframe, ch) && !dip_itf_is_ins_exbuf(pch)) {
			mm->sts.flg_alloced = false;
			di_bypass_state_set(ch, true);
			//pch->sumx.vfm_bypass = true;
			mm->cfg.num_rebuild_keep = length_keep;
			return 0;
		}
		//pch->sumx.vfm_bypass = false;
		blk_cmd.cmd = ECMD_BLK_ALLOC;
		blk_cmd.hf_need = 0;
		if (mm->cfg.num_local) {
			/* pre */
			//blk_cmd.cmd = ECMD_BLK_ALLOC;
			blk_cmd.nub = mm->cfg.num_local;
			blk_cmd.flg.d32 = mm->cfg.ibuf_flg.d32;

			//mtask_send_cmd(ch, &blk_cmd);
			mtsk_alloc_block2(ch, &blk_cmd);
		}

		/* post */
		if (length_keep > 8)
			PR_ERR("%s:keep nub:%d\n", __func__, length_keep);

		blk_cmd.nub = mm->cfg.num_post - length_keep;
		blk_cmd.flg.d32 = mm->cfg.pbuf_flg.d32;

		if (mm->cfg.pbuf_flg.b.page) {//@ary_note: ??
			if (mm->cfg.size_buf_hf)
				blk_cmd.hf_need = 1;
			else
				blk_cmd.hf_need = 0;

			if (blk_cmd.nub > 4) {
				tmp_nub = blk_cmd.nub - 3;
				blk_cmd.nub = 3;
				//mtask_send_cmd(ch, &blk_cmd);
				mtsk_alloc_block2(ch, &blk_cmd);
				blk_cmd.nub = tmp_nub;
				mtask_send_cmd(ch, &blk_cmd);
			} else {
				mtask_send_cmd(ch, &blk_cmd);
			}
		} else if ((mm->cfg.pbuf_flg.b.typ & 0x8) ==
			   EDIM_BLK_TYP_POUT) {
			//move all to wait:
			di_buf_no2wait(pch);
		}

		mm->sts.flg_alloced = true;
		mm->cfg.num_rebuild_keep = 0;
	} else {
		mm->sts.flg_alloced = true;
		mm->cfg.num_rebuild_keep = 0;
	}
	return 0;
}

/* @ary_note: only for ready */
/* @ary_note: when ready, process non-keep buffer and keep buffer */

bool dim_post_keep_release_one_check(unsigned int ch, unsigned int di_buf_index)
{
//	struct di_buf_s *pbuf_post;
	struct di_buf_s *di_buf;
	struct div2_mm_s *mm = dim_mm_get(ch);
//	bool flg_alloc = false;
	struct di_ch_s *pch;
	struct dim_ndis_s *ndis;

	pch = get_chdata(ch);

	ndis = ndis_get_fromid(pch, di_buf_index);
	if (!ndis) {
		PR_ERR("%s:no ndis\n", __func__);
		return false;
	}

	if (!ndis_is_in_keep(pch, ndis)) {
		/* @ary_note: from put */
		if (ndis_is_in_display(pch, ndis)) {
			di_buf = ndis->c.di_buf;
			if (!di_buf) {
				PR_ERR("%s:no di_buf\n", __func__);
				return false;
			}
			di_buf->queue_index = -1;
			di_que_in(ch, QUE_POST_BACK, di_buf);
			ndis_move_display2idle(pch, ndis);
			if (dimp_get(edi_mp_bypass_post_state))
				PR_INF("%s:buf[%d,%d]\n", __func__, di_buf->type, di_buf->index);

			dbg_keep("%s:to back[%d]\n", __func__, di_buf_index);
		} else {
			PR_ERR("%s:ndis[%d] is not in keep or display\n",
			       __func__, ndis->header.index);
		}
		return true;
	}
	if (mm->sts.flg_alloced)
		mm->sts.flg_realloc++;
	else
		mm->cfg.num_rebuild_alloc++;

	dbg_mem2("%s:stsflg_realloc[%d][%d]\n",
		 __func__, mm->sts.flg_realloc,
		 mm->cfg.num_rebuild_alloc);
	mem_release_one_inused(pch, ndis->c.blk);
	ndis_move_keep2idle(pch, ndis);
	return true;
}

/* after dim_post_keep_release_one_check */
void dim_post_re_alloc(unsigned int ch)
{
	struct div2_mm_s *mm = dim_mm_get(ch);
//	struct di_mng_s *pbm = get_bufmng();
	struct di_ch_s *pch;
	unsigned int length;
	struct mtsk_cmd_s cmd;

	pch = get_chdata(ch);
	if (mm->sts.flg_release) {
		/* KEEP_BACK -> release */
		mem_release_keep_back(pch);
		//mtsk_release(ch, ECMD_BLK_RELEASE);
		mm->sts.flg_release = 0;
	}

	/* trig recycle */
	length =	qbufp_count(&pch->mem_qb, QBF_MEM_Q_RECYCLE);
	if (length) {
		mem_2_blk(pch);
		cmd.cmd = ECMD_BLK_RELEASE;
		cmd.hf_need = 0;
		mtask_send_cmd(ch, &cmd);
		//mtsk_release(ch, ECMD_BLK_RELEASE);
	}

	if (mm->sts.flg_realloc) {
		cmd.cmd = ECMD_BLK_ALLOC;
		cmd.nub = mm->sts.flg_realloc;
		cmd.flg.d32 = mm->cfg.pbuf_flg.d32;
		if (mm->cfg.size_buf_hf)
			cmd.hf_need = 1;
		else
			cmd.hf_need = 0;
		mtask_send_cmd(ch, &cmd);
		//mtsk_alloc(ch, mm->sts.flg_realloc, mm->cfg.size_post_page);
		mm->sts.flg_realloc = 0;
	}
}

//EXPORT_SYMBOL(dim_post_keep_cmd_release2);

void dim_post_keep_cmd_proc(unsigned int ch, unsigned int index)
{
	struct di_dev_s *di_dev;
	enum EDI_TOP_STATE chst;
//	unsigned int len_keep, len_back;
//	struct di_buf_s *pbuf_post;
//	struct di_buf_s *di_buf;
	//ulong flags = 0;
	struct div2_mm_s *mm = dim_mm_get(ch);
	struct di_ch_s *pch;

	/*must post or err*/
	di_dev = get_dim_de_devp();

	if (!di_dev || !di_dev->data_l) {
		PR_WARN("%s: no di_dev\n", __func__);
		return;
	}
	pch = get_chdata(ch);

	//ary 2020-12-07 spin_lock_irqsave(&plist_lock, flags);

	chst = dip_chst_get(ch);
	dbg_wq("k:p[%d]%d\n", chst, index);
	switch (chst) {
	case EDI_TOP_STATE_READY:	/* need check tvp*/
	case EDI_TOP_STATE_UNREG_STEP1:
	case EDI_TOP_STATE_UNREG_STEP2:
		/*dim_post_keep_release_one(ch, index);*/
		dim_post_keep_release_one_check(ch, index);
		break;
	case EDI_TOP_STATE_IDLE:
	case EDI_TOP_STATE_BYPASS:
		ndkb_qin_byidx(pch, index);
		mm->sts.flg_release++;
		break;
	case EDI_TOP_STATE_REG_STEP1:
	case EDI_TOP_STATE_REG_STEP1_P1:
	case EDI_TOP_STATE_REG_STEP2:
		ndkb_qin_byidx(pch, index);
		break;
	default:
		PR_ERR("%s:do nothing? %s:%d\n",
		       __func__,
		       dip_chst_get_name(chst),
		       index);
		break;
	}
	//ary 2020-12-07 spin_unlock_irqrestore(&plist_lock, flags);
}

void dim_uninit_buf(unsigned int disable_mirror, unsigned int channel)
{
	/*int i = 0;*/
	#ifdef VFM_ORI
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	#endif
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct di_ch_s *pch;
	struct dim_mm_blk_s *blks[POST_BUF_NUM];
	unsigned int blk_nub = 0;
	int i;

	pch = get_chdata(channel);

	for (i = 0; i < POST_BUF_NUM; i++)
		blks[i] = NULL;

	//if (!disable_mirror)
	blk_nub = ndis_2keep(pch, &blks[0], POST_BUF_NUM, disable_mirror);
	//dim_post_keep_mirror_buffer2(channel);

	mem_release(pch, &blks[0], blk_nub);
	PR_INF("unreg:2keep[%d], msk:[0x%x,0x%x]\n",
	       pch->sts_unreg_dis2keep,
	       pch->sts_unreg_blk_msk,
	       pch->sts_unreg_pat_mst);
	mem_2_blk(pch);

	queue_init(channel, 0);
	di_que_init(channel); /*new que*/
	//bufq_iat_rest(pch);
	qiat_all_back2_ready(pch);
	bufq_ndis_unreg(pch);

	/* decoder'buffer had been releae no need put */
	#ifdef VFM_ORI
	memset(pvframe_in, 0, sizeof(*pvframe_in) * MAX_IN_BUF_NUM);
	#else
	/* clear all ?*/
	#endif
	ppre->pre_de_process_flag = 0;
	if (dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support)) {
		ppost->cur_post_buf = NULL;
		ppost->post_de_busy = 0;
		ppost->de_post_process_done = 0;
		ppost->post_wr_cnt = 0;
	}
	if (cfgeq(MEM_FLAG, EDI_MEM_M_REV) && de_devp->nrds_enable) {
		dim_nr_ds_buf_uninit(cfgg(MEM_FLAG),
				     &de_devp->pdev->dev);
	}
}

#ifdef MARK_HIS
static void dump_state(unsigned int channel)
{
	struct di_buf_s *p = NULL/*, *keep_buf*/;
	int itmp, i;
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
	/*struct di_dev_s *de_devp = get_dim_de_devp();*/
	unsigned int tmpa[MAX_FIFO_SIZE];	/*new que*/
	unsigned int psize;		/*new que*/
	struct div2_mm_s *mm = dim_mm_get(channel); /*mm-0705*/

	dump_state_flag = 1;
	pr_info("version %s, init_flag %d, is_bypass %d\n",
		version_s, get_init_flag(channel),
		dim_is_bypass(NULL, channel));
	pr_info("recovery_flag = %d, recovery_log_reason=%d, di_blocking=%d",
		recovery_flag, recovery_log_reason, di_blocking);
	pr_info("recovery_log_queue_idx=%d, recovery_log_di_buf=0x%p\n",
		recovery_log_queue_idx, recovery_log_di_buf);
	pr_info("buffer_size=%d, mem_flag=%s, cma_flag=%d\n",
		/*atomic_read(&de_devp->mem_flag)*/
		mm->cfg.size_local, di_cma_dbg_get_st_name(channel),
		cfgg(MEM_FLAG));

	pr_info("\nin_free_list (max %d):\n", MAX_IN_BUF_NUM);

	di_que_list(channel, QUE_IN_FREE, &tmpa[0], &psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pw_qindex_2_buf(channel, tmpa[itmp]);
		pr_info("index %2d, 0x%p, type %d\n",
			p->index, p, p->type);
	}
	pr_info("local_free_list (max %d):\n", mm->cfg.num_local);
	queue_for_each_entry(p, channel, QUEUE_LOCAL_FREE, list) {
		pr_info("index %2d, 0x%p, type %d\n", p->index, p, p->type);
	}

	pr_info("post_doing_list:\n");
	//queue_for_each_entry(p, channel, QUEUE_POST_DOING, list) {
	di_que_list(channel, QUE_POST_DOING, &tmpa[0], &psize);
	for (itmp = 0; itmp < psize; itmp++) {
		p = pw_qindex_2_buf(channel, tmpa[itmp]);
		dim_print_di_buf(p, 2);
	}
	pr_info("pre_ready_list:\n");

	di_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pw_qindex_2_buf(channel, tmpa[itmp]);
		dim_print_di_buf(p, 2);
	}
	pr_info("post_free_list (max %d):\n", mm->cfg.num_post);

	di_que_list(channel, QUE_POST_FREE, &tmpa[0], &psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pw_qindex_2_buf(channel, tmpa[itmp]);

		pr_info("index %2d, 0x%p, type %d, vframetype 0x%x\n",
			p->index, p, p->type, p->vframe->type);
	}
	pr_info("post_ready_list:\n");

	di_que_list(channel, QUE_POST_READY, &tmpa[0], &psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pw_qindex_2_buf(channel, tmpa[itmp]);

		dim_print_di_buf(p, 2);
		dim_print_di_buf(p->di_buf[0], 1);
		dim_print_di_buf(p->di_buf[1], 1);
	}
	pr_info("display_list:\n");
	queue_for_each_entry(p, channel, QUEUE_DISPLAY, list) {
		dim_print_di_buf(p, 2);
		dim_print_di_buf(p->di_buf[0], 1);
		dim_print_di_buf(p->di_buf[1], 1);
	}
	pr_info("recycle_list:\n");
	queue_for_each_entry(p, channel, QUEUE_RECYCLE, list) {
		pr_info("index %d, 0x%p, type %d, vframetype 0x%x\n",
			p->index, p, p->type,
			p->vframe->type);
		pr_info("pre_ref_count %d post_ref_count %d\n",
			p->pre_ref_count,
			p->post_ref_count);
		if (p->di_wr_linked_buf) {
			pr_info("linked index %2d, 0x%p, type %d\n",
				p->di_wr_linked_buf->index,
				p->di_wr_linked_buf,
				p->di_wr_linked_buf->type);
			pr_info("linked pre_ref_count %d post_ref_count %d\n",
				p->di_wr_linked_buf->pre_ref_count,
				p->di_wr_linked_buf->post_ref_count);
		}
	}
	if (ppre->di_inp_buf) {
		pr_info("di_inp_buf:index %d, 0x%p, type %d\n",
			ppre->di_inp_buf->index,
			ppre->di_inp_buf,
			ppre->di_inp_buf->type);
	} else {
		pr_info("di_inp_buf: NULL\n");
	}
	if (ppre->di_wr_buf) {
		pr_info("di_wr_buf:index %d, 0x%p, type %d\n",
			ppre->di_wr_buf->index,
			ppre->di_wr_buf,
			ppre->di_wr_buf->type);
	} else {
		pr_info("di_wr_buf: NULL\n");
	}
	dim_dump_pre_stru(ppre);
	dim_dump_post_stru(ppost);
	pr_info("vframe_in[]:");

	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		pr_info("0x%p ", pvframe_in[i]);

	pr_info("\n");
	pr_info("vf_peek()=>0x%p, video_peek_cnt = %d\n",
		pw_vf_peek(channel), di_sum_get(channel, EDI_SUM_O_PEEK_CNT));
	pr_info("reg_unreg_timerout = %lu\n", reg_unreg_timeout_cnt);
	dump_state_flag = 0;
}
#endif

unsigned char dim_check_di_buf(struct di_buf_s *di_buf, int reason,
			       unsigned int channel)
{
	int error = 0;
	struct vframe_s *pvframe_in_dup = get_vframe_in_dup(channel);
	struct vframe_s *pvframe_local = get_vframe_local(channel);
	struct vframe_s *pvframe_post = get_vframe_post(channel);

	if (!di_buf) {
		PR_ERR("%s: %d, di_buf is NULL\n", __func__, reason);
		return 1;
	}

	if (di_buf->type == VFRAME_TYPE_IN) {
		if (di_buf->vframe != &pvframe_in_dup[di_buf->index])
			error = 1;
	} else if (di_buf->type == VFRAME_TYPE_LOCAL) {
		if (di_buf->vframe != &pvframe_local[di_buf->index])
			error = 1;
	} else if (di_buf->type == VFRAME_TYPE_POST) {
		if (di_buf->vframe != &pvframe_post[di_buf->index])
			error = 1;
	} else {
		error = 1;
	}

	if (error) {
		PR_ERR("%s: %d, di_buf wrong\n", __func__, reason);
		if (recovery_flag == 0)
			recovery_log_reason = reason;
		recovery_flag++;
		dim_dump_di_buf(di_buf);
		return 1;
	}

	return 0;
}

/*
 *  di pre process
 */
static void
config_di_mcinford_mif(struct DI_MC_MIF_s *di_mcinford_mif,
		       struct di_buf_s *di_buf)
{
	if (di_buf) {
		di_mcinford_mif->size_x = (di_buf->vframe->height + 2) / 4 - 1;
		di_mcinford_mif->size_y = 1;
		di_mcinford_mif->canvas_num = di_buf->mcinfo_canvas_idx;
		di_mcinford_mif->addr = di_buf->mcinfo_adr;
	}
}

static void
config_di_pre_mc_mif(struct DI_MC_MIF_s *di_mcinfo_mif,
		     struct DI_MC_MIF_s *di_mcvec_mif,
		     struct di_buf_s *di_buf)
{
	unsigned int pre_size_w = 0, pre_size_h = 0;

	if (di_buf) {
		pre_size_w = di_buf->vframe->width;
		pre_size_h = di_buf->vframe->height / 2;
		di_mcinfo_mif->size_x = pre_size_h / 2 - 1;
		di_mcinfo_mif->size_y = 1;
		di_mcinfo_mif->canvas_num = di_buf->mcinfo_canvas_idx;
		di_mcinfo_mif->addr = di_buf->mcinfo_adr;

		di_mcvec_mif->size_x = (pre_size_w + 4) / 5 - 1;
		di_mcvec_mif->size_y = pre_size_h - 1;
		di_mcvec_mif->canvas_num = di_buf->mcvec_canvas_idx;
		di_mcvec_mif->addr = di_buf->mcvec_adr;
	}
}

static void config_di_cnt_mif(struct DI_SIM_MIF_s *di_cnt_mif,
			      struct di_buf_s *di_buf)
{
	if (di_buf) {
		di_cnt_mif->start_x = 0;
		di_cnt_mif->end_x = di_buf->vframe->width - 1;
		di_cnt_mif->start_y = 0;
		di_cnt_mif->end_y = di_buf->vframe->height / 2 - 1;
		di_cnt_mif->canvas_num = di_buf->cnt_canvas_idx;
		di_cnt_mif->addr = di_buf->cnt_adr;
	}
}

static void
config_di_wr_mif(struct DI_SIM_MIF_s *di_nrwr_mif,
		 struct DI_SIM_MIF_s *di_mtnwr_mif,
		 struct di_buf_s *di_buf, unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	vframe_t *vf = di_buf->vframe;

	di_nrwr_mif->canvas_num = di_buf->nr_canvas_idx;
	di_nrwr_mif->start_x = 0;
	di_nrwr_mif->end_x = vf->width - 1;
	di_nrwr_mif->start_y = 0;
	if (di_buf->vframe->bitdepth & BITDEPTH_Y10)
		di_nrwr_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ?
			3 : 1;
	else
		di_nrwr_mif->bit_mode = 0;
	if (ppre->prog_proc_type == 0)
		di_nrwr_mif->end_y = vf->height / 2 - 1;
	else
		di_nrwr_mif->end_y = vf->height - 1;
	/* separate */
	if (vf->type & VIDTYPE_VIU_422)
		di_nrwr_mif->set_separate_en = 0;
	else
		di_nrwr_mif->set_separate_en = 2; /*nv12 ? nv 21?*/
	if (ppre->prog_proc_type == 0) {
		di_mtnwr_mif->start_x = 0;
		di_mtnwr_mif->end_x = vf->width - 1;
		di_mtnwr_mif->start_y = 0;
		di_mtnwr_mif->end_y = vf->height / 2 - 1;
		di_mtnwr_mif->canvas_num = di_buf->mtn_canvas_idx;
	}
}

/* move di_hw_v2.c */
static void config_di_mtnwr_mif(struct DI_SIM_MIF_s *di_mtnwr_mif,
				struct di_buf_s *di_buf)
{
//	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	vframe_t *vf = di_buf->vframe;

	if (di_mtnwr_mif->src_i) {
		di_mtnwr_mif->start_x = 0;
		di_mtnwr_mif->end_x = vf->width - 1;
		di_mtnwr_mif->start_y = 0;
		di_mtnwr_mif->end_y = vf->height / 2 - 1;
		di_mtnwr_mif->canvas_num = di_buf->mtn_canvas_idx;
		di_mtnwr_mif->addr	= di_buf->mtn_adr;
	}
}

/*ary move  to di_hw_v2.c */
static void config_di_mif(struct DI_MIF_S *di_mif, struct di_buf_s *di_buf,
			  unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (!di_buf)
		return;
	di_mif->canvas0_addr0 =
		di_buf->vframe->canvas0Addr & 0xff;
	di_mif->canvas0_addr1 =
		(di_buf->vframe->canvas0Addr >> 8) & 0xff;
	di_mif->canvas0_addr2 =
		(di_buf->vframe->canvas0Addr >> 16) & 0xff;

//	di_mif->nocompress = (di_buf->vframe->type & VIDTYPE_COMPRESS) ? 0 : 1;

	if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
		if (di_buf->vframe->type & VIDTYPE_VIU_444)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ?
			3 : 2;
		else if (di_buf->vframe->type & VIDTYPE_VIU_422)
			di_mif->bit_mode =
			(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ?
			3 : 1;
	} else {
		di_mif->bit_mode = 0;
	}
	if (di_buf->vframe->type & VIDTYPE_VIU_422) {
		/* from vdin or local vframe */
		if ((!is_progressive(di_buf->vframe))	||
		    ppre->prog_proc_type) {
			di_mif->video_mode = 0;
			di_mif->set_separate_en = 0;
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0;
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 =
				di_buf->vframe->width - 1;
			di_mif->luma_y_start0 = 0;
			if (ppre->prog_proc_type)
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 1;
			else
				di_mif->luma_y_end0 =
					di_buf->vframe->height / 2 - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 = 0;
			di_mif->chroma_y_start0 = 0;
			di_mif->chroma_y_end0 = 0;
			di_mif->canvas0_addr0 =
				di_buf->vframe->canvas0Addr & 0xff;
			di_mif->canvas0_addr1 =
				(di_buf->vframe->canvas0Addr >> 8) & 0xff;
			di_mif->canvas0_addr2 =
				(di_buf->vframe->canvas0Addr >> 16) & 0xff;
		}
		di_mif->reg_swap = 1;
		di_mif->l_endian = 0;
		di_mif->cbcr_swap = 0;
	} else {
		if (di_buf->vframe->type & VIDTYPE_VIU_444)
			di_mif->video_mode = 1;
		else
			di_mif->video_mode = 0;
		if ((di_buf->vframe->type & VIDTYPE_VIU_NV21) ||
			(di_buf->vframe->type & VIDTYPE_VIU_NV12))
			di_mif->set_separate_en = 2;
		else
			di_mif->set_separate_en = 1;

		if (is_progressive(di_buf->vframe) && ppre->prog_proc_type) {
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0; /* top */
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 =
				di_buf->vframe->width - 1;
			di_mif->luma_y_start0 = 0;
			di_mif->luma_y_end0 =
				di_buf->vframe->height - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 =
				di_buf->vframe->width / 2 - 1;
			di_mif->chroma_y_start0 = 0;
			di_mif->chroma_y_end0 =
				(di_buf->vframe->height + 1) / 2 - 1;
		} else if ((ppre->cur_inp_type & VIDTYPE_INTERLACE) &&
				(ppre->cur_inp_type & VIDTYPE_VIU_FIELD)) {
			di_mif->src_prog = 0;
			di_mif->src_field_mode = 0;
			di_mif->output_field_num = 0; /* top */
			di_mif->luma_x_start0 = 0;
			di_mif->luma_x_end0 =
				di_buf->vframe->width - 1;
			di_mif->luma_y_start0 = 0;
			di_mif->luma_y_end0 =
				di_buf->vframe->height / 2 - 1;
			di_mif->chroma_x_start0 = 0;
			di_mif->chroma_x_end0 =
				di_buf->vframe->width / 2 - 1;
			di_mif->chroma_y_start0 = 0;
			di_mif->chroma_y_end0 =
				di_buf->vframe->height / 4 - 1;
		} else {
			/*move to mp	di_mif->src_prog = force_prog?1:0;*/
			if (ppre->cur_inp_type  & VIDTYPE_INTERLACE)
				di_mif->src_prog = 0;
			else
				di_mif->src_prog =
				dimp_get(edi_mp_force_prog) ? 1 : 0;
			di_mif->src_field_mode = 1;
			if ((di_buf->vframe->type & VIDTYPE_TYPEMASK) ==
			    VIDTYPE_INTERLACE_TOP) {
				di_mif->output_field_num = 0; /* top */
				di_mif->luma_x_start0 = 0;
				di_mif->luma_x_end0 =
					di_buf->vframe->width - 1;
				di_mif->luma_y_start0 = 0;
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 1;
				di_mif->chroma_x_start0 = 0;
				di_mif->chroma_x_end0 =
					di_buf->vframe->width / 2 - 1;
				di_mif->chroma_y_start0 = 0;
				di_mif->chroma_y_end0 =
					(di_buf->vframe->height + 1) / 2 - 1;
			} else {
				di_mif->output_field_num = 1;
				/* bottom */
				di_mif->luma_x_start0 = 0;
				di_mif->luma_x_end0 =
					di_buf->vframe->width - 1;
				di_mif->luma_y_start0 = 1;
				di_mif->luma_y_end0 =
					di_buf->vframe->height - 1;
				di_mif->chroma_x_start0 = 0;
				di_mif->chroma_x_end0 =
					di_buf->vframe->width / 2 - 1;
				di_mif->chroma_y_start0 =
					(di_mif->src_prog ? 0 : 1);
				di_mif->chroma_y_end0 =
					(di_buf->vframe->height + 1) / 2 - 1;
			}
		}
	}
}

static void di_pre_size_change(unsigned short width,
			       unsigned short height,
			       unsigned short vf_type,
			       unsigned int channel);

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static void pre_inp_canvas_config(struct vframe_s *vf);
#endif

//static void pre_inp_mif_w(struct DI_MIF_S *di_mif, struct vframe_s *vf);
static void dim_canvas_set2(struct vframe_s *vf, u32 *index);

void dim_pre_de_process(unsigned int channel)
{
	ulong irq_flag2 = 0;
	unsigned short pre_width = 0, pre_height = 0, t5d_cnt;
	unsigned char chan2_field_num = 1;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	int canvases_idex = ppre->field_count_for_cont % 2;
	unsigned short cur_inp_field_type = VIDTYPE_TYPEMASK;
	unsigned short int_mask = 0x7f;
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct di_cvs_s *cvss;
	struct vframe_s *vf_i, *vf_mem, *vf_chan2;
	union hw_sc2_ctr_pre_s *sc2_pre_cfg;
	struct di_ch_s *pch;
	u32	cvs_nv21[2];

	vf_i	= NULL;
	vf_mem	= NULL;
	vf_chan2 = NULL;

	pch = get_chdata(channel);
	ppre->pre_de_process_flag = 1;
	dim_ddbg_mod_save(EDI_DBG_MOD_PRE_SETB, channel, ppre->in_seq);/*dbg*/
	cvss = &get_datal()->cvs;

	if (IS_ERR_OR_NULL(ppre->di_inp_buf))
		return;

	#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	pre_inp_canvas_config(ppre->di_inp_buf->vframe);
	#endif

	pre_inp_mif_w(&ppre->di_inp_mif, ppre->di_inp_buf->vframe);
	if (DIM_IS_IC_EF(SC2))
		opl1()->pre_cfg_mif(&ppre->di_inp_mif,
				    DI_MIF0_ID_INP,
				    ppre->di_inp_buf,
				    channel);
	else
		config_di_mif(&ppre->di_inp_mif, ppre->di_inp_buf, channel);
	/* pr_dbg("set_separate_en=%d vframe->type %d\n",
	 * di_pre_stru.di_inp_mif.set_separate_en,
	 * di_pre_stru.di_inp_buf->vframe->type);
	 */

	if (IS_ERR_OR_NULL(ppre->di_mem_buf_dup_p))
		return;

	dim_secure_sw_pre(channel);

	if (ppre->di_mem_buf_dup_p	&&
	    ppre->di_mem_buf_dup_p != ppre->di_inp_buf) {
		if (ppre->di_mem_buf_dup_p->flg_nv21) {
			cvs_nv21[0] = cvss->post_idx[1][3];
			cvs_nv21[1] = cvss->post_idx[1][4];
			ppre->di_mem_buf_dup_p->vframe->canvas0Addr = ((u32)-1);
			dim_canvas_set2(ppre->di_mem_buf_dup_p->vframe,
					&cvs_nv21[0]);
			dim_print("mem:vfm:pnub[%d]\n",
				  ppre->di_mem_buf_dup_p->vframe->plane_num);
			//dim_print("mem:vfm:w[%d]\n",
				    //ppre->di_mem_buf_dup_p->vframe->
				    //canvas0_config[0].width);
			//ppre->di_mem_mif.canvas_num =
				//re->di_mem_buf_dup_p->vframe->canvas0Addr;
		} else {
			config_canvas_idx(ppre->di_mem_buf_dup_p,
					  cvss->pre_idx[canvases_idex][0], -1);
		}
		dim_print("mem:flg_nv21[%d]:flg_[%d] %px\n",
			  ppre->di_mem_buf_dup_p->flg_nv21,
			  ppre->di_mem_buf_dup_p->flg_nr,
			  ppre->di_mem_buf_dup_p);
		config_cnt_canvas_idx(ppre->di_mem_buf_dup_p,
				      cvss->pre_idx[canvases_idex][1]);
		if (DIM_IS_IC_EF(T7)) {
			ppre->di_contp2rd_mif.addr =
				ppre->di_mem_buf_dup_p->cnt_adr;
			dbg_ic("contp2rd:0x%lx\n", ppre->di_contp2rd_mif.addr);
		}
	} else {
		config_cnt_canvas_idx(ppre->di_wr_buf,
				      cvss->pre_idx[canvases_idex][1]);
		config_di_cnt_mif(&ppre->di_contp2rd_mif,
			  ppre->di_wr_buf);
	}
	if (ppre->di_chan2_buf_dup_p) {
		config_canvas_idx(ppre->di_chan2_buf_dup_p,
				  cvss->pre_idx[canvases_idex][2], -1);
		config_cnt_canvas_idx(ppre->di_chan2_buf_dup_p,
				      cvss->pre_idx[canvases_idex][3]);
	} else {
		config_cnt_canvas_idx(ppre->di_wr_buf,
				      cvss->pre_idx[canvases_idex][3]);
	}
	ppre->di_nrwr_mif.is_dw = 0;
	if (ppre->di_wr_buf->flg_nv21) {
		//cvss = &get_datal()->cvs;
		//0925	cvs_nv21[0] = cvss->post_idx[1][1];
		cvs_nv21[0] = cvss->pre_idx[canvases_idex][4];//0925
		cvs_nv21[1] = cvss->post_idx[1][2];
		dim_canvas_set2(ppre->di_wr_buf->vframe, &cvs_nv21[0]);
		config_canvas_idx_mtn(ppre->di_wr_buf,
				      cvss->pre_idx[canvases_idex][5]);
		//ppost->di_diwr_mif.canvas_num = pst->vf_post.canvas0Addr;
		ppre->di_nrwr_mif.canvas_num =
			ppre->di_wr_buf->vframe->canvas0Addr;
		ppre->di_wr_buf->nr_canvas_idx =
			ppre->di_wr_buf->vframe->canvas0Addr;
		if (ppre->di_wr_buf->flg_nv21 == 1)
			ppre->di_nrwr_mif.cbcr_swap = 0;
		else
			ppre->di_nrwr_mif.cbcr_swap = 1;
		if (dip_itf_is_o_linear(pch)) {
			ppre->di_nrwr_mif.reg_swap = 0;
			ppre->di_nrwr_mif.l_endian = 1;

		} else {
			ppre->di_nrwr_mif.reg_swap = 1;
			ppre->di_nrwr_mif.l_endian = 0;
		}
		if (cfgg(LINEAR)) {
			ppre->di_nrwr_mif.linear = 1;
			ppre->di_nrwr_mif.addr =
				ppre->di_wr_buf->vframe->canvas0_config[0].phy_addr;
			ppre->di_nrwr_mif.addr1 =
				ppre->di_wr_buf->vframe->canvas0_config[1].phy_addr;
		}
	} else  if (ppre->shrk_cfg.shrk_en) {
		ppre->di_nrwr_mif.reg_swap = 1;
		ppre->di_nrwr_mif.cbcr_swap = 0;
		ppre->di_nrwr_mif.l_endian = 0;
		ppre->di_nrwr_mif.is_dw = 1;
		if (cfgg(LINEAR)) {
			ppre->di_nrwr_mif.linear = 1;
			ppre->di_nrwr_mif.addr =
				ppre->dw_wr_dvfm.canvas0_config[0].phy_addr;
			ppre->di_nrwr_mif.addr1 =
				ppre->dw_wr_dvfm.canvas0_config[1].phy_addr;
		}

		dim_dvf_config_canvas(&ppre->dw_wr_dvfm);
		//ppre->di_wr_buf->vframe->canvas0Addr = cvs_nv21[0];
		//ppre->di_wr_buf->vframe->canvas1Addr = cvs_nv21[0]; /*?*/
		//dim_print("wr:%px\n", ppre->di_wr_buf);
	} else {
		ppre->di_nrwr_mif.reg_swap = 1;
		ppre->di_nrwr_mif.cbcr_swap = 0;
		ppre->di_nrwr_mif.l_endian = 0;
		if (cfgg(LINEAR)) {
			ppre->di_nrwr_mif.linear = 1;
			ppre->di_nrwr_mif.addr = ppre->di_wr_buf->nr_adr;
		}
		config_canvas_idx(ppre->di_wr_buf,
				  cvss->pre_idx[canvases_idex][4],
				  cvss->pre_idx[canvases_idex][5]);
	}
	config_cnt_canvas_idx(ppre->di_wr_buf,
			      cvss->pre_idx[canvases_idex][6]);
	if (dimp_get(edi_mp_mcpre_en)) {
		if (ppre->di_chan2_buf_dup_p)
			config_mcinfo_canvas_idx
				(ppre->di_chan2_buf_dup_p,
				 cvss->pre_idx[canvases_idex][7]);
		else
			config_mcinfo_canvas_idx
				(ppre->di_wr_buf,
				 cvss->pre_idx[canvases_idex][7]);

		config_mcinfo_canvas_idx(ppre->di_wr_buf,
					 cvss->pre_idx[canvases_idex][8]);
		config_mcvec_canvas_idx(ppre->di_wr_buf,
					cvss->pre_idx[canvases_idex][9]);
	}

	if (DIM_IS_IC_EF(SC2))
		opl1()->pre_cfg_mif(&ppre->di_mem_mif,
				    DI_MIF0_ID_MEM,
				    ppre->di_mem_buf_dup_p,
				    channel);
	else
		config_di_mif(&ppre->di_mem_mif,
			      ppre->di_mem_buf_dup_p, channel);
	/* patch */
	if (ppre->di_wr_buf->flg_nv21	&&
	    ppre->di_mem_buf_dup_p	&&
	    ppre->di_mem_buf_dup_p != ppre->di_inp_buf) {
		ppre->di_mem_mif.l_endian = ppre->di_nrwr_mif.l_endian;
		ppre->di_mem_mif.cbcr_swap = ppre->di_nrwr_mif.cbcr_swap;
		ppre->di_mem_mif.reg_swap = ppre->di_nrwr_mif.reg_swap;
	}
	if (!ppre->di_chan2_buf_dup_p) {
		if (DIM_IS_IC_EF(SC2))
			opl1()->pre_cfg_mif(&ppre->di_chan2_mif,
					    DI_MIF0_ID_CHAN2,
					    ppre->di_inp_buf,
					    channel);
		else
			config_di_mif(&ppre->di_chan2_mif,
				      ppre->di_inp_buf, channel);

	} else {
		if (DIM_IS_IC_EF(SC2))
			opl1()->pre_cfg_mif(&ppre->di_chan2_mif,
					    DI_MIF0_ID_CHAN2,
					    ppre->di_chan2_buf_dup_p,
					    channel);
		else
			config_di_mif(&ppre->di_chan2_mif,
				      ppre->di_chan2_buf_dup_p, channel);
	}
	if (ppre->prog_proc_type == 0) {
		ppre->di_nrwr_mif.src_i = 1;
		ppre->di_mtnwr_mif.src_i = 1;
	} else {
		ppre->di_nrwr_mif.src_i = 0;
		ppre->di_mtnwr_mif.src_i = 0;
	}
	if (DIM_IS_IC_EF(SC2)) {
		if (ppre->shrk_cfg.shrk_en) {
			//dw_fill_outvf_pre(&ppre->vf_copy, ppre->di_wr_buf);
			//ppre->vfm_cpy.private_data = ppre->di_wr_buf;
			opl1()->wr_cfg_mif_dvfm(&ppre->di_nrwr_mif,
					   EDI_MIFSM_NR,
					   &ppre->dw_wr_dvfm, NULL);
		} else {
			opl1()->wr_cfg_mif(&ppre->di_nrwr_mif,
					   EDI_MIFSM_NR,
					   ppre->di_wr_buf, NULL);
		}
	} else {
		config_di_wr_mif(&ppre->di_nrwr_mif, &ppre->di_mtnwr_mif,
				 ppre->di_wr_buf, channel);
	}

	if (DIM_IS_IC_EF(SC2))
		config_di_mtnwr_mif(&ppre->di_mtnwr_mif, ppre->di_wr_buf);

	if (ppre->di_chan2_buf_dup_p)
		config_di_cnt_mif(&ppre->di_contprd_mif,
				  ppre->di_chan2_buf_dup_p);
	else
		config_di_cnt_mif(&ppre->di_contprd_mif,
				  ppre->di_wr_buf);

	config_di_cnt_mif(&ppre->di_contwr_mif, ppre->di_wr_buf);
	if (dimp_get(edi_mp_mcpre_en)) {
		if (ppre->di_chan2_buf_dup_p)
			config_di_mcinford_mif(&ppre->di_mcinford_mif,
					       ppre->di_chan2_buf_dup_p);
		else
			config_di_mcinford_mif(&ppre->di_mcinford_mif,
					       ppre->di_wr_buf);

		config_di_pre_mc_mif(&ppre->di_mcinfowr_mif,
				     &ppre->di_mcvecwr_mif, ppre->di_wr_buf);
	}

	if (ppre->di_chan2_buf_dup_p &&
	    ((ppre->di_chan2_buf_dup_p->vframe->type & VIDTYPE_TYPEMASK)
	     == VIDTYPE_INTERLACE_TOP))
		chan2_field_num = 0;

	if (ppre->shrk_cfg.shrk_en) {
		pre_width = ppre->di_wr_buf->vframe->width;
		pre_height = ppre->di_wr_buf->vframe->height;
	} else {
		pre_width = ppre->di_nrwr_mif.end_x + 1;
		pre_height = ppre->di_nrwr_mif.end_y + 1;
	}
	if (IS_ERR_OR_NULL(ppre->di_inp_buf))
		return;

	if (/*ppre->di_inp_buf && */ppre->di_inp_buf->vframe)
		vf_i = ppre->di_inp_buf->vframe;

	if (dim_afds() && dim_afds()->pre_check)
		dim_afds()->pre_check(ppre->di_wr_buf->vframe, pch);
	if (ppre->di_wr_buf->is_4k) {
		ppre->pre_top_cfg.b.is_inp_4k = 1;
		ppre->pre_top_cfg.b.is_mem_4k = 1;
	}
	if (ppre->input_size_change_flag) {
		if (dim_afds() && dim_afds()->rest_val)
			dim_afds()->rest_val();
		cur_inp_field_type =
		(ppre->di_inp_buf->vframe->type & VIDTYPE_TYPEMASK);
		cur_inp_field_type =
	ppre->cur_prog_flag ? VIDTYPE_PROGRESSIVE : cur_inp_field_type;
		/*di_async_reset2();*/
		di_pre_size_change(pre_width, pre_height,
				   cur_inp_field_type, channel);
		ppre->input_size_change_flag = false;
	}

	if (DIM_IS_IC_EF(SC2)) {
		sc2_pre_cfg = &get_hw_pre()->pre_top_cfg;
		if (sc2_pre_cfg->d32 != ppre->pre_top_cfg.d32) {
			sc2_pre_cfg->d32 = ppre->pre_top_cfg.d32;
			dim_sc2_contr_pre(sc2_pre_cfg);
			dim_sc2_4k_set(sc2_pre_cfg->b.mode_4k);
		}
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		if (de_devp->nrds_enable) {
			dim_nr_ds_mif_config();
			dim_nr_ds_hw_ctrl(true);
			int_mask = 0x3f;
		} else {
			dim_nr_ds_hw_ctrl(false);
		}
	}
	#ifndef TMP_MASK_FOR_T7
	/*patch for SECAM signal format from vlsi-feijun for all IC*/
	get_ops_nr()->secam_cfr_adjust(ppre->di_inp_buf->vframe->sig_fmt,
				       ppre->di_inp_buf->vframe->type);
	#endif
	/* set interrupt mask for pre module.
	 * we need to only leave one mask open
	 * to prevent multiple entry for dim_irq
	 */

	/*dim_dbg_pre_cnt(channel, "s2");*/
	/* for t5d vb netflix change afbc input timeout */
	if (DIM_IS_IC(T5DB)) {
		t5d_cnt = cfgg(T5DB_P_NOTNR_THD);
		if (t5d_cnt < 2)
			t5d_cnt = 2;
		if (ppre->field_count_for_cont < t5d_cnt &&
		    ppre->cur_prog_flag)
			ppre->is_bypass_mem |= 0x02;
		else
			ppre->is_bypass_mem &= ~0x02;

		if (ppre->is_bypass_mem)
			ppre->di_wr_buf->is_bypass_mem = 1;
		else
			ppre->di_wr_buf->is_bypass_mem = 0;
		dim_print("%s:is_bypass_mem[%d]\n", __func__,
			  ppre->di_wr_buf->is_bypass_mem);
		if (ppre->field_count_for_cont < 1 &&
		    IS_FIELD_I_SRC(ppre->cur_inp_type))
			ppre->is_disable_chan2 = 1;
		else
			ppre->is_disable_chan2 = 0;
		//when p mode/frist frame ,set 0 to reset the mc vec wr,
		//second frame write back to 1,from vlsi feijun.fan for DMC bug

		if (ppre->field_count_for_cont < 1 &&
		    IS_PROG(ppre->cur_inp_type)) {
			dimh_set_slv_mcvec(0);
		} else {
			if (!dimh_get_slv_mcvec())
				dimh_set_slv_mcvec(1);
		}
	}
	if (dim_config_crc_ic()) //add for crc @2k22-0102
		dimh_set_crc_init(ppre->field_count_for_cont);

	if (IS_ERR_OR_NULL(ppre->di_wr_buf))
		return;
	if (dim_hdr_ops() && ppre->di_wr_buf->c.en_hdr)
		dim_hdr_ops()->set(1);

	di_lock_irqfiq_save(irq_flag2);
	if (IS_IC_SUPPORT(DW)) { /* dw */
		opl1()->shrk_set(&ppre->shrk_cfg, &di_pre_regset);
	}
	dimh_enable_di_pre_aml(&ppre->di_inp_mif,
			       &ppre->di_mem_mif,
			       &ppre->di_chan2_mif,
			       &ppre->di_nrwr_mif,
			       &ppre->di_mtnwr_mif,
			       &ppre->di_contp2rd_mif,
			       &ppre->di_contprd_mif,
			       &ppre->di_contwr_mif,
			       ppre->madi_enable,
			       chan2_field_num,
			       ppre->vdin2nr |
			       (ppre->is_bypass_mem << 4),
			       ppre);

	//dimh_enable_afbc_input(ppre->di_inp_buf->vframe);

	dcntr_set();

	if (dim_afds()) {
		if (ppre->di_mem_buf_dup_p && ppre->di_mem_buf_dup_p->vframe)
			vf_mem = ppre->di_mem_buf_dup_p->vframe;
		if (ppre->di_chan2_buf_dup_p &&
		    ppre->di_chan2_buf_dup_p->vframe)
			vf_chan2 = ppre->di_chan2_buf_dup_p->vframe;

		if (/*ppre->di_wr_buf && */ppre->di_wr_buf->vframe)
			dim_afds()->en_pre_set(vf_i,
					       vf_mem,
					       vf_chan2,
					       ppre->di_wr_buf->vframe);
	}

	if (dimp_get(edi_mp_mcpre_en)) {
		if (DIM_IS_IC_EF(T7))
			opl1()->pre_enable_mc(&ppre->di_mcinford_mif,
						  &ppre->di_mcinfowr_mif,
						  &ppre->di_mcvecwr_mif,
						  ppre->mcdi_enable);
		else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			dimh_enable_mc_di_pre_g12(&ppre->di_mcinford_mif,
						  &ppre->di_mcinfowr_mif,
						  &ppre->di_mcvecwr_mif,
						  ppre->mcdi_enable);
		else
			dimh_enable_mc_di_pre(&ppre->di_mcinford_mif,
					      &ppre->di_mcinfowr_mif,
					      &ppre->di_mcvecwr_mif,
					      ppre->mcdi_enable);
	}
	if (dim_is_slt_mode()) {
		if (DIM_IS_IC(T5) || DIM_IS_IC(T5DB) ||
		    DIM_IS_IC(T5D)) {
			DIM_DI_WR(0x2dff, 0x3e03c); //crc test for nr//0xbeffc
		} else if (DIM_IS_IC_EF(SC2)) {
			if (DIM_IS_ICS(T5W))
				DIM_DI_WR(0x2dff, 0xbe03c); //disable the 3dnr
			else
				DIM_DI_WR(0x2dff, 0xbeffc);
			DIM_DI_WR(0x2d00, 0x0);
		}
	}
	ppre->field_count_for_cont++;

	if (ppre->field_count_for_cont >= 5)
		DIM_DI_WR_REG_BITS(DI_MTN_CTRL, 0, 30, 1);

	dimh_txl_patch_prog(ppre->cur_prog_flag,
			    ppre->field_count_for_cont,
			    dimp_get(edi_mp_mcpre_en));

	if (ppre->di_wr_buf->en_hf	&&
	    ppre->di_wr_buf->hf_adr	&&
	    di_hf_size_check(&ppre->di_nrwr_mif) &&
	    di_hf_hw_try_alloc(1)	&&
	    opl1()->aisr_pre) {
		ppre->hf_mif.addr = ppre->di_wr_buf->hf_adr;
		ppre->hf_mif.start_x = 0;
		ppre->hf_mif.start_y = 0;
		ppre->hf_mif.end_x = ppre->di_nrwr_mif.end_x;
		ppre->hf_mif.end_y = ppre->di_nrwr_mif.end_y;
		di_hf_buf_size_set(&ppre->hf_mif);
		ppre->di_wr_buf->hf.height = ppre->di_nrwr_mif.end_y + 1;
		ppre->di_wr_buf->hf.width =
			ppre->di_nrwr_mif.end_x + 1;
		/* use addr2 for vsize */
		ppre->di_wr_buf->hf.buffer_w = ppre->hf_mif.buf_hsize;
		ppre->di_wr_buf->hf.buffer_h =
			(unsigned int)ppre->hf_mif.addr2;
		//di_hf_lock_blend_buffer_pre(ppre->di_wr_buf);
		if (is_di_hf_y_reverse())
			ppre->di_wr_buf->hf.revert_mode = true;
		else
			ppre->di_wr_buf->hf.revert_mode = false;
		opl1()->aisr_pre(&ppre->hf_mif, 0,
				 ppre->di_wr_buf->hf.revert_mode);

		ppre->di_wr_buf->hf_done = 1;
	}
	dbg_ic("hf:en:pre:%d;addr:0x%lx;done:%d\n",
		ppre->di_wr_buf->en_hf,
		ppre->di_wr_buf->hf_adr,
		ppre->di_wr_buf->hf_done);
	/* must make sure follow part issue without interrupts,
	 * otherwise may cause watch dog reboot
	 */
	//di_lock_irqfiq_save(irq_flag2);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/* enable mc pre mif*/
		dimh_enable_di_pre_mif(true, dimp_get(edi_mp_mcpre_en));
		dim_pre_frame_reset_g12(ppre->madi_enable,
					ppre->mcdi_enable);
	} else {
		dim_pre_frame_reset();
		/* enable mc pre mif*/
		dimh_enable_di_pre_mif(true, dimp_get(edi_mp_mcpre_en));
	}
	/*dbg_set_DI_PRE_CTRL();*/
	atomic_set(&get_hw_pre()->flg_wait_int, 1);
	ppre->pre_de_busy = 1;
	ppre->irq_time[0] = cur_to_usecs();
	ppre->irq_time[1] = ppre->irq_time[0];
	di_unlock_irqfiq_restore(irq_flag2);
	/*reinit pre busy flag*/
	pch->sum_pre++;
	dim_dbg_pre_cnt(channel, "s3");
//	ppre->irq_time[0] = cur_to_msecs();
//	ppre->irq_time[1] = cur_to_msecs();
	dim_ddbg_mod_save(EDI_DBG_MOD_PRE_SETE, channel, ppre->in_seq);/*dbg*/
	dim_tr_ops.pre_set(ppre->di_wr_buf->vframe->index_disp);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (di_pre_rdma_enable & 0x2)
		rdma_config(de_devp->rdma_handle, RDMA_TRIGGER_MANUAL);
	else if (di_pre_rdma_enable & 1)
		rdma_config(de_devp->rdma_handle, RDMA_DEINT_IRQ);
#endif
	ppre->pre_de_process_flag = 0;
}

void dim_pre_de_done_buf_clear(unsigned int channel)
{
	struct di_buf_s *wr_buf = NULL;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (ppre->di_wr_buf) {
		wr_buf = ppre->di_wr_buf;
		if (ppre->prog_proc_type == 2 &&
		    wr_buf->di_wr_linked_buf) {
			wr_buf->di_wr_linked_buf->pre_ref_count = 0;
			wr_buf->di_wr_linked_buf->post_ref_count = 0;
			queue_in(channel, wr_buf->di_wr_linked_buf,
				 QUEUE_RECYCLE);
			wr_buf->di_wr_linked_buf = NULL;
		}
		wr_buf->pre_ref_count = 0;
		wr_buf->post_ref_count = 0;
		queue_in(channel, wr_buf, QUEUE_RECYCLE);
		ppre->di_wr_buf = NULL;
	}
	if (ppre->di_inp_buf) {
		if (ppre->di_mem_buf_dup_p == ppre->di_inp_buf)
			ppre->di_mem_buf_dup_p = NULL;

		queue_in(channel, ppre->di_inp_buf, QUEUE_RECYCLE);
		ppre->di_inp_buf = NULL;
	}
}

static void top_bot_config(struct di_buf_s *di_buf)
{
	vframe_t *vframe = di_buf->vframe;

	if (((invert_top_bot & 0x1) != 0) && (!is_progressive(vframe))) {
		if (di_buf->invert_top_bot_flag == 0) {
			if ((vframe->type & VIDTYPE_TYPEMASK) ==
			    VIDTYPE_INTERLACE_TOP) {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_BOTTOM;
			} else {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_TOP;
			}
			di_buf->invert_top_bot_flag = 1;
		}
	}
}

static void pp_buf_clear(struct di_buf_s *buff);

void dim_pre_de_done_buf_config(unsigned int channel, bool flg_timeout)
{
//ary 2020-12-09	ulong irq_flag2 = 0;
	int tmp_cur_lev;
	struct di_buf_s *post_wr_buf = NULL, *wpst = NULL;
	unsigned int frame_motnum = 0;
	unsigned int field_motnum = 0;
	unsigned int pd_info = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_ch_s *pch;
	//struct di_buf_s *bufn;
	//bool crc_right;
	unsigned int afbce_used;

	dim_dbg_pre_cnt(channel, "d1");
	dim_ddbg_mod_save(EDI_DBG_MOD_PRE_DONEB, channel, ppre->in_seq);/*dbg*/
	pch = get_chdata(channel);
	if (ppre->di_wr_buf) {
		if (flg_timeout) {
			hpre_timeout_read();
			if (DIM_IS_IC_EF(SC2))
				opl1()->pre_gl_sw(false);
			else
				hpre_gl_sw(false);
			if (ppre->di_wr_buf->hf_done) {
				opl1()->aisr_disable();
				di_hf_hw_release(1);
				ppre->di_wr_buf->hf_done = false;
			}
			ppre->timeout_check = true;
		} else {
			if (ppre->di_wr_buf->hf_done) {
				di_hf_hw_release(1);
				//opl1()->aisr_disable();
			}
		}
		if (ppre->di_wr_buf->field_count < 3) {
			if (!ppre->di_wr_buf->field_count)
				dbg_timer(channel, EDBG_TIMER_1_PREADY);
			else if (ppre->di_wr_buf->field_count == 1)
				dbg_timer(channel, EDBG_TIMER_2_PREADY);
			else if (ppre->di_wr_buf->field_count == 2)
				dbg_timer(channel, EDBG_TIMER_3_PREADY);
		}
		dim_pqrpt_init(&ppre->di_wr_buf->pq_rpt);
		if (!flg_timeout)
			dcntr_pq_tune(&ppre->di_wr_buf->pq_rpt);
		dim_tr_ops.pre_ready(ppre->di_wr_buf->vframe->index_disp);
		ppre->di_wr_buf->flg_nr = 1;
		if (ppre->pre_throw_flag > 0) {
			ppre->di_wr_buf->throw_flag = 1;
			ppre->pre_throw_flag--;
		} else {
			ppre->di_wr_buf->throw_flag = 0;
		}
		if (/*ppre->di_wr_buf->flg_afbce_set*/ppre->prog_proc_type !=
			0x10) { /*afbce check cec*/
			//ppre->di_wr_buf->flg_afbce_set = 0;
			//afbce_sw(EAFBC_ENC0, 0);
			#ifdef DBG_CRC
			bufn = next_buf(ppre->di_wr_buf);
			if (bufn) {
				crc_right = dbg_checkcrc(bufn);
				if (!crc_right)
					PR_ERR("pre crc next err:b[%d]nb[%d]\n",
					       ppre->di_wr_buf->index,
					       bufn->index);
			}
			#endif
		} else {
			//dbg_checkcrc(ppre->di_wr_buf);
		}
		if (ppre->di_wr_buf->flg_afbce_set) {
			ppre->di_wr_buf->flg_afbce_set = 0;
			//tmp
			//if (ppre->di_wr_buf->blk_buf->flg.b.typ == EDIM_BLK_TYP_PSCT) {
			if (ppre->di_wr_buf->blk_buf &&
			    dim_blk_tvp_is_sct(ppre->di_wr_buf->blk_buf)) {
				afbce_used = afbce_read_used(EAFBC_ENC0);
				//pch = get_chdata(channel);
				sct_free_tail_l(pch,
					afbce_used,
					(struct dim_sct_s *)ppre->di_wr_buf->blk_buf->sct);
//			tst_resize(pch, afbce_used);
			}
			/*************************/
			//afbce_sw(EAFBC_ENC0, 0);
		}
#ifdef DET3D
		if (ppre->di_wr_buf->vframe->trans_fmt == 0	&&
		    ppre->det3d_trans_fmt != 0			&&
		    dimp_get(edi_mp_det3d_en)) {
			ppre->di_wr_buf->vframe->trans_fmt =
			ppre->det3d_trans_fmt;
			set3d_view(ppre->det3d_trans_fmt,
				   ppre->di_wr_buf->vframe);
		}
#endif
		/*dec vf keep*/
		if (ppre->di_inp_buf &&
		    ppre->di_inp_buf->dec_vf_state & DI_BIT0) {
			ppre->di_wr_buf->in_buf = ppre->di_inp_buf;
			dim_print("dim:dec vf:l[%d]\n",
				  ppre->di_wr_buf->in_buf->vframe->index_disp);
		}

		if (!di_pre_rdma_enable)
			ppre->di_post_wr_buf = ppre->di_wr_buf;
		post_wr_buf = ppre->di_post_wr_buf;

		if (post_wr_buf) {
			post_wr_buf->vframe->di_pulldown = 0;
			post_wr_buf->vframe->di_gmv = 0;
			post_wr_buf->vframe->di_cm_cnt = 0;
		}

		if (post_wr_buf && !ppre->cur_prog_flag &&
		    !flg_timeout && ppre->di_inp_buf) {
			dim_read_pulldown_info(&frame_motnum,
					       &field_motnum);
			if (dimp_get(edi_mp_pulldown_enable)) {
				/*pulldown_detection*/
				pd_info = get_ops_pd()->detection
					(&post_wr_buf->pd_config,
					 ppre->mtn_status,
					 overturn,
					 ppre->di_inp_buf->vframe);
				post_wr_buf->vframe->di_pulldown = pd_info;
			}
			post_wr_buf->vframe->di_pulldown |= 0x08;

			post_wr_buf->vframe->di_gmv = frame_motnum;
			post_wr_buf->vframe->di_cm_cnt = dim_rd_mcdi_fldcnt();

			/*if (combing_fix_en)*/
			/*if (dimp_get(eDI_MP_combing_fix_en)) {*/
			if (ppre->combing_fix_en) {
				tmp_cur_lev = /*cur_lev*/
				get_ops_mtn()->adaptive_combing_fixing
					(ppre->mtn_status,
					 field_motnum,
					 frame_motnum,
					 dimp_get(edi_mp_di_force_bit_mode));
				dimp_set(edi_mp_cur_lev, tmp_cur_lev);
			}

			if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX))
				get_ops_nr()->adaptive_cue_adjust(frame_motnum,
								  field_motnum);
			if (!(di_dbg & DBG_M_RESET_PRE))
				dim_pulldown_info_clear_g12a();
		}
		if (ppre->prog_proc_type == 0x10) {
			ppre->di_mem_buf_dup_p->pre_ref_count = 0;
			/*recycle the progress throw buffer*/

			if (ppre->di_wr_buf->throw_flag) {
				ppre->di_wr_buf->pre_ref_count = 0;
				ppre->di_mem_buf_dup_p = NULL;

			} else if (ppre->di_wr_buf->is_bypass_mem) {
				di_que_in(channel, QUE_PRE_READY,
					  ppre->di_wr_buf);
				ppre->di_mem_buf_dup_p = NULL;
				dim_print("mem bypass\n");
			} else {
				#ifdef HOLD_ONE_FRAME
				if (ppre->di_mem_buf_dup_p->flg_nr) {
					di_que_in(channel, QUE_PRE_READY,
						  ppre->di_mem_buf_dup_p);
				}
				ppre->di_mem_buf_dup_p = ppre->di_wr_buf;
				#else
				/* clear mem ref */
				if (ppre->di_mem_buf_dup_p->flg_nr) {
					p_ref_set_buf(ppre->di_mem_buf_dup_p,
						      0, 0, 0);
					pp_buf_clear(ppre->di_mem_buf_dup_p);
					di_que_in(channel, QUE_PRE_NO_BUF,
						  ppre->di_mem_buf_dup_p);
				}

				ppre->di_mem_buf_dup_p = ppre->di_wr_buf;
				p_ref_set_buf(ppre->di_mem_buf_dup_p, 1, 0, 1);
				di_que_in(channel, QUE_PRE_READY,
						  ppre->di_wr_buf);
				if (dimp_get(edi_mp_pstcrc_ctrl) == 1) {
					if ((DIM_IS_IC(T5) || DIM_IS_IC(T5DB) ||
					    DIM_IS_IC(T5D)) &&
					    ppre->di_wr_buf) {
						ppre->di_wr_buf->nrcrc =
							RD(DI_T5_RO_CRC_NRWR);
					} else if (DIM_IS_IC_EF(SC2)) {
						ppre->di_wr_buf->nrcrc =
							RD(DI_RO_CRC_NRWR);
						DIM_DI_WR_REG_BITS(DI_CRC_CHK0,
								   0x1, 31, 1);
					}
				}

				#endif
			}

			ppre->di_wr_buf->seq = ppre->pre_ready_seq++;
			ppre->di_wr_buf->post_ref_count = 0;
			ppre->di_wr_buf->left_right = ppre->left_right;
			if (ppre->di_wr_buf->dw_have) {
				wpst = ppre->di_wr_buf->di_buf_post;
				memcpy(&wpst->c.dvframe,
				       &ppre->dw_wr_dvfm,
				       sizeof(wpst->c.dvframe));
			}
			/******************************************/
			/* need check */
			if (ppre->source_change_flag) {
				//ppre->di_wr_buf->new_format_flag = 1;
				ppre->source_change_flag = 0;
			} else {
				//ppre->di_wr_buf->new_format_flag = 0;
			}
			if (di_bypass_state_get(channel) == 1) {
				//ppre->di_wr_buf->new_format_flag = 1;
				/*bypass_state = 0;*/
				di_bypass_state_set(channel, false);
				pch->sumx.flg_rebuild = 0;
			}
			if (ppre->di_wr_buf) {
				if (di_pre_rdma_enable)
					ppre->di_post_wr_buf =
						ppre->di_wr_buf;
				else
					ppre->di_post_wr_buf = NULL;
						ppre->di_wr_buf = NULL;
			}

		} else if (ppre->cur_prog_flag) {
			if (ppre->prog_proc_type == 0) {
				/* di_mem_buf_dup->vfrme
				 * is either local vframe,
				 * or bot field of vframe from in_list
				 */
				ppre->di_mem_buf_dup_p->pre_ref_count = 0;
				ppre->di_mem_buf_dup_p =
					ppre->di_chan2_buf_dup_p;
				ppre->di_chan2_buf_dup_p = ppre->di_wr_buf;
#ifdef DI_BUFFER_DEBUG
			dim_print("%s:set di_mem to di_chan2,", __func__);
			dim_print("%s:set di_chan2 to di_wr_buf\n", __func__);
#endif
			} else {
				ppre->di_mem_buf_dup_p->pre_ref_count = 0;
				/*recycle the progress throw buffer*/
				if (ppre->di_wr_buf->throw_flag) {
					ppre->di_wr_buf->pre_ref_count = 0;
					ppre->di_mem_buf_dup_p = NULL;
#ifdef DI_BUFFER_DEBUG
				dim_print("%s st throw %s[%d] pre_ref_cont 0\n",
					  __func__,
				vframe_type_name[ppre->di_wr_buf->type],
				ppre->di_wr_buf->index);
#endif
				} else {
					ppre->di_mem_buf_dup_p =
						ppre->di_wr_buf;
				}
#ifdef DI_BUFFER_DEBUG
			dim_print("%s: set di_mem_buf_dup_p to di_wr_buf\n",
				  __func__);
#endif
			}

			ppre->di_wr_buf->seq = ppre->pre_ready_seq++;
			ppre->di_wr_buf->post_ref_count = 0;
			ppre->di_wr_buf->left_right = ppre->left_right;
			if (ppre->source_change_flag) {
				ppre->di_wr_buf->new_format_flag = 1;
				ppre->source_change_flag = 0;
			} else {
				ppre->di_wr_buf->new_format_flag = 0;
			}
			if (di_bypass_state_get(channel) == 1) {
				ppre->di_wr_buf->new_format_flag = 1;
				/*bypass_state = 0;*/
				di_bypass_state_set(channel, false);
				pch->sumx.flg_rebuild = 0;
#ifdef DI_BUFFER_DEBUG
		dim_print("%s:bypass_state->0, is_bypass() %d\n",
			  __func__, dim_is_bypass(NULL, channel));
		dim_print("trick_mode %d bypass_all %d\n",
			  trick_mode,
			  di_cfgx_get(channel, EDI_CFGX_BYPASS_ALL));
#endif
			}
			if (ppre->di_post_wr_buf)
				di_que_in(channel, QUE_PRE_READY,
					  ppre->di_post_wr_buf);
			if (dimp_get(edi_mp_pstcrc_ctrl) == 1) {
				if (DIM_IS_IC(T5) || DIM_IS_IC(T5DB) ||
				    DIM_IS_IC(T5D)) {
					if (ppre->di_wr_buf)
						ppre->di_wr_buf->nrcrc =
							RD(DI_T5_RO_CRC_NRWR);
				} else if (DIM_IS_IC_EF(SC2)) {
					if (ppre->di_wr_buf) {
						ppre->di_wr_buf->nrcrc =
							RD(DI_RO_CRC_NRWR);
					}
					DIM_DI_WR_REG_BITS(DI_CRC_CHK0,
							   0x1, 31, 1);
				}
			}

#ifdef DI_BUFFER_DEBUG
			dim_print("%s: %s[%d] => pre_ready_list\n", __func__,
				  vframe_type_name[ppre->di_wr_buf->type],
				  ppre->di_wr_buf->index);
#endif
			if (ppre->di_wr_buf) {
				if (di_pre_rdma_enable)
					ppre->di_post_wr_buf =
				ppre->di_wr_buf;
				else
					ppre->di_post_wr_buf = NULL;
				ppre->di_wr_buf = NULL;
			}
		} else {
			ppre->di_mem_buf_dup_p->pre_ref_count = 0;
			ppre->di_mem_buf_dup_p = NULL;
			if (ppre->di_chan2_buf_dup_p) {
				ppre->di_mem_buf_dup_p =
					ppre->di_chan2_buf_dup_p;
#ifdef DI_BUFFER_DEBUG
				dim_print("%s:mem_buf=chan2_buf_dup_p\n",
					  __func__);
#endif
			}
			ppre->di_chan2_buf_dup_p = ppre->di_wr_buf;

			if (ppre->source_change_flag) {
				/* add dummy buf, will not be displayed */
				add_dummy_vframe_type_pre(post_wr_buf,
							  channel);
			}
			ppre->di_wr_buf->seq = ppre->pre_ready_seq++;
			ppre->di_wr_buf->left_right = ppre->left_right;
			ppre->di_wr_buf->post_ref_count = 0;

			if (ppre->source_change_flag) {
				ppre->di_wr_buf->new_format_flag = 1;
				ppre->source_change_flag = 0;
			} else {
				ppre->di_wr_buf->new_format_flag = 0;
			}
			if (di_bypass_state_get(channel) == 1) {
				ppre->di_wr_buf->new_format_flag = 1;
				/*bypass_state = 0;*/
				di_bypass_state_set(channel, false);
				pch->sumx.flg_rebuild = 0;

#ifdef DI_BUFFER_DEBUG
		dim_print("%s:bypass_state->0, is_bypass() %d\n",
			  __func__, dim_is_bypass(NULL, channel));
		dim_print("trick_mode %d bypass_all %d\n",
			  trick_mode,
			  di_cfgx_get(channel, EDI_CFGX_BYPASS_ALL));
#endif
			}

			if (ppre->di_post_wr_buf)
				di_que_in(channel, QUE_PRE_READY,
					  ppre->di_post_wr_buf);
			if (dimp_get(edi_mp_pstcrc_ctrl) == 1) {
				if (DIM_IS_IC(T5) || DIM_IS_IC(T5DB) ||
				    DIM_IS_IC(T5D)) {
					if (ppre->di_wr_buf->di_buf_post)
						ppre->di_wr_buf->di_buf_post->nrcrc =
							RD(DI_T5_RO_CRC_NRWR);
				} else if (DIM_IS_IC_EF(SC2)) {
					if (ppre->di_wr_buf->di_buf_post) {
						ppre->di_wr_buf->di_buf_post->nrcrc =
							RD(DI_RO_CRC_NRWR);
					}
					DIM_DI_WR_REG_BITS(DI_CRC_CHK0,
							   0x1, 31, 1);
				}
			}
			dim_print("%s: %s[%d] => pre_ready_list\n", __func__,
				  vframe_type_name[ppre->di_wr_buf->type],
				  ppre->di_wr_buf->index);

			if (ppre->di_wr_buf) {
				if (di_pre_rdma_enable)
					ppre->di_post_wr_buf = ppre->di_wr_buf;
				else
					ppre->di_post_wr_buf = NULL;

				ppre->di_wr_buf = NULL;
			}
		}
	}
	if (ppre->di_post_inp_buf && di_pre_rdma_enable) {
#ifdef DI_BUFFER_DEBUG
		dim_print("%s: %s[%d] => recycle_list\n", __func__,
			  vframe_type_name[ppre->di_post_inp_buf->type],
			  ppre->di_post_inp_buf->index);
#endif
//ary 2020-12-09		di_lock_irqfiq_save(irq_flag2);
		queue_in(channel, ppre->di_post_inp_buf, QUEUE_RECYCLE);
		ppre->di_post_inp_buf = NULL;
//ary 2020-12-09		di_unlock_irqfiq_restore(irq_flag2);
	}
	if (ppre->di_inp_buf) {
		if (!di_pre_rdma_enable) {
#ifdef DI_BUFFER_DEBUG
			dim_print("%s: %s[%d] => recycle_list\n", __func__,
				  vframe_type_name[ppre->di_inp_buf->type],
				  ppre->di_inp_buf->index);
#endif
//ary 2020-12-09			di_lock_irqfiq_save(irq_flag2);
			if (!(ppre->di_inp_buf->dec_vf_state & DI_BIT0)) {
				/*dec vf keep*/
				queue_in(channel, ppre->di_inp_buf,
					 QUEUE_RECYCLE);
				ppre->di_inp_buf = NULL;
			}
//ary 2020-12-09			di_unlock_irqfiq_restore(irq_flag2);
		} else {
			ppre->di_post_inp_buf = ppre->di_inp_buf;
			ppre->di_inp_buf = NULL;
		}
	}
	dim_ddbg_mod_save(EDI_DBG_MOD_PRE_DONEE, channel, ppre->in_seq);/*dbg*/
	dim_dbg_pre_cnt(channel, "d2");
}

static void recycle_vframe_type_pre(struct di_buf_s *di_buf,
				    unsigned int channel)
{
//ary 2020-12-09	ulong irq_flag2 = 0;

//ary 2020-12-09	di_lock_irqfiq_save(irq_flag2);

	queue_in(channel, di_buf, QUEUE_RECYCLE);

//ary 2020-12-09	di_unlock_irqfiq_restore(irq_flag2);
}

/*
 * add dummy buffer to pre ready queue
 */
static void add_dummy_vframe_type_pre(struct di_buf_s *src_buf,
				      unsigned int channel)
{
	struct di_buf_s *di_buf_tmp = NULL;

	if (!queue_empty(channel, QUEUE_LOCAL_FREE)) {
		di_buf_tmp = get_di_buf_head(channel, QUEUE_LOCAL_FREE);
		if (di_buf_tmp) {
			queue_out(channel, di_buf_tmp);
			di_buf_tmp->pre_ref_count = 0;
			di_buf_tmp->post_ref_count = 0;
			di_buf_tmp->post_proc_flag = 3;
			di_buf_tmp->new_format_flag = 0;
			if (!IS_ERR_OR_NULL(src_buf))
				memcpy(di_buf_tmp->vframe, src_buf->vframe,
				       sizeof(vframe_t));

			di_que_in(channel, QUE_PRE_READY, di_buf_tmp);
			#ifdef DI_BUFFER_DEBUG
			dim_print("%s: dummy %s[%d] => pre_ready_list\n",
				  __func__,
				  vframe_type_name[di_buf_tmp->type],
				  di_buf_tmp->index);
			#endif
		}
	}
}

/* 2021-01-26 for eos out*/
static void add_eos_in(unsigned int ch, struct dim_nins_s *nin)
{
	struct di_buf_s *di_buf;

	if (di_que_is_empty(ch, QUE_IN_FREE)) {
		PR_ERR("%s:no in buf\n", __func__);
		return;
	}
	//PR_INF("%s:ch[%d]\n", __func__, ch);
	di_buf = di_que_out_to_di_buf(ch, QUE_IN_FREE);
	di_buf->is_eos = 1;
	if (nin) {
		di_buf->c.in = nin;
		di_buf->is_nbypass = 1;
	} else {
		di_buf->c.in = NULL;
	}

	di_que_in(ch, QUE_PRE_READY, di_buf);
	dbg_bypass("%s:\n", __func__);
}

/*
 * it depend on local buffer queue type is 2
 */
static int peek_free_linked_buf(unsigned int channel)
{
	struct di_buf_s *p = NULL;
	int itmp, p_index = -2;

	if (list_count(channel, QUEUE_LOCAL_FREE) < 2)
		return -1;

	queue_for_each_entry(p, channel, QUEUE_LOCAL_FREE, list) {
		if (abs(p->index - p_index) == 1)
			return min(p->index, p_index);
		p_index = p->index;
	}
	return -1;
}

/*
 * it depend on local buffer queue type is 2
 */
static struct di_buf_s *get_free_linked_buf(int idx, unsigned int channel)
{
	struct di_buf_s *di_buf = NULL, *linkp = NULL;
	int pool_idx = 0, di_buf_idx = 0;
	struct queue_s *pqueue = get_queue(channel);
	struct di_buf_pool_s *ppo = get_buf_pool(channel);

	queue_t *q = &pqueue[QUEUE_LOCAL_FREE];

	if (list_count(channel, QUEUE_LOCAL_FREE) < 2)
		return NULL;
	if (q->pool[idx] != 0 && q->pool[idx + 1] != 0) {
		pool_idx = ((q->pool[idx] >> 8) & 0xff) - 1;
		di_buf_idx = q->pool[idx] & 0xff;
		if (pool_idx < VFRAME_TYPE_NUM) {
			if (di_buf_idx < ppo[pool_idx].size) {
				di_buf = &ppo[pool_idx].di_buf_ptr[di_buf_idx];
				queue_out(channel, di_buf);
			}
		}
		pool_idx = ((q->pool[idx + 1] >> 8) & 0xff) - 1;
		di_buf_idx = q->pool[idx + 1] & 0xff;
		if (pool_idx < VFRAME_TYPE_NUM) {
			if (di_buf_idx < ppo[pool_idx].size) {
				linkp =	&ppo[pool_idx].di_buf_ptr[di_buf_idx];
				queue_out(channel, linkp);
			}
		}
		if (IS_ERR_OR_NULL(di_buf))
			return NULL;
		di_buf->di_wr_linked_buf = linkp;
	}
	return di_buf;
}

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static void pre_inp_canvas_config(struct vframe_s *vf)
{
	struct di_cvs_s *cvss;

	cvss = &get_datal()->cvs;

	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(cvss->inp_idx[0],
				     &vf->canvas0_config[0]);
		canvas_config_config(cvss->inp_idx[1],
				     &vf->canvas0_config[1]);
		vf->canvas0Addr = (cvss->inp_idx[1] << 8) | (cvss->inp_idx[0]);
		if (vf->plane_num == 2) {
			vf->canvas0Addr |= (cvss->inp_idx[1] << 16);
		} else if (vf->plane_num == 3) {
			canvas_config_config(cvss->inp_idx[2],
					     &vf->canvas0_config[2]);
			vf->canvas0Addr |= (cvss->inp_idx[2] << 16);
		}
		vf->canvas1Addr = vf->canvas0Addr;
		dim_print("%s:w:%d\n", __func__, vf->canvas0_config[0].width);
	}
}
#endif

void pre_cfg_cvs(struct vframe_s *vf)
{
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	pre_inp_canvas_config(vf);
#endif
}

//static
void pre_inp_mif_w(struct DI_MIF_S *di_mif, struct vframe_s *vf)
{
	unsigned int dbga = dim_get_dbg_dec21();
	int i;
	unsigned long addr[3];

	if (vf->canvas0Addr != (u32)-1)
		di_mif->canvas_w =
			canvas_get_width(vf->canvas0Addr & 0xff);
	else
		di_mif->canvas_w = vf->canvas0_config[0].width;

	if (cfgg(LINEAR)) {
		for (i = 0; i < vf->plane_num; i++) {
			addr[i] = vf->canvas0_config[i].phy_addr;
			dbg_ic("%s:[%d]addr[0x%lx]\n", __func__, i, addr[i]);
		}
		di_mif->addr0 = addr[0];
		di_mif->cvs0_w = vf->canvas0_config[0].width;
		di_mif->cvs1_w = 0;
		di_mif->cvs2_w = 0;
		if (vf->plane_num >= 2) {
			di_mif->addr1 = addr[1];
			di_mif->cvs1_w = vf->canvas0_config[1].width;
		}
		if (vf->plane_num >= 3) {
			di_mif->addr2 = addr[2];
			di_mif->cvs2_w = vf->canvas0_config[2].width;
		}
		di_mif->linear = 1;
		if (vf->plane_num == 2) {
			di_mif->buf_crop_en = 1;
			di_mif->buf_hsize = vf->canvas0_config[0].width;
			dbg_ic("\t:buf_h[%d]\n", di_mif->buf_hsize);
		} else {
			di_mif->buf_crop_en = 0;
			dbg_ic("\t:not nv21?\n");
		}

		if (vf->canvas0_config[0].block_mode)
			di_mif->block_mode = vf->canvas0_config[0].block_mode;
		else
			di_mif->block_mode = 0;
	}

	if (vf->type & VIDTYPE_VIU_NV12)
		di_mif->cbcr_swap = 1;
	else
		di_mif->cbcr_swap = 0;
	if ((vf->flag & VFRAME_FLAG_VIDEO_LINEAR) ||
	    dim_in_linear()) {
		if (vf->canvas0_config[0].endian && (DIM_IS_IC_EF(T7))) {
			di_mif->reg_swap = 1;
			di_mif->l_endian = 0;
		} else {
			di_mif->reg_swap = 0;
			di_mif->l_endian = 1;
		}

	} else {
		di_mif->reg_swap = 1;
		di_mif->l_endian = 0;
		//di_mif->cbcr_swap = 0;
	}
	if (dbga & 0xf) {
		di_mif->reg_swap = bget(&dbga, 0);
		di_mif->l_endian = bget(&dbga, 1);
		di_mif->cbcr_swap = bget(&dbga, 2);
	}
}

#ifdef MARK_HIS
bool di_get_pre_hsc_down_en(void)
{
	return pre_hsc_down_en;
}
#endif
bool dbg_first_frame;	/*debug */
unsigned int  dbg_first_cnt_pre;
unsigned int  dbg_first_cnt_post;
#define DI_DBG_CNT	(2)
void dim_dbg_pre_cnt(unsigned int channel, char *item)
{
	bool flgs = false;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (ppre->field_count_for_cont < DI_DBG_CNT) {
		dbg_first_frame("%d:%s:%d\n", channel, item,
				ppre->field_count_for_cont);
		dbg_first_frame = true;
		flgs = true;
		dbg_first_cnt_pre = DI_DBG_CNT * 5;
		dbg_first_cnt_post = DI_DBG_CNT * 4 + 1;
	} else if (ppre->field_count_for_cont == DI_DBG_CNT) {/*don't use >=*/
		dbg_first_frame = false;
		dbg_first_cnt_pre = 0;
		dbg_first_cnt_post = 0;
	}

	if ((dbg_first_frame) && !flgs && dbg_first_cnt_pre) {
		dbg_first_frame("%d:n%s:%d\n", channel, item,
				ppre->field_count_for_cont);
		dbg_first_cnt_pre--;
	}
}

static void dbg_post_cnt(unsigned int ch, char *item)
{
	struct di_post_stru_s *ppost = get_post_stru(ch);

	if (dbg_first_cnt_post) {
		dbg_first_frame("%d:%s:%d\n", ch, item,
				ppost->frame_cnt);
		dbg_first_cnt_post--;
	}
}

unsigned char pre_p_asi_de_buf_config(unsigned int ch)
{
	struct di_pre_stru_s *ppre = get_pre_stru(ch);

#ifdef MARK_HIS
	if (di_blocking || !dip_cma_st_is_ready(ch))
		return 0;

	if (di_que_list_count(ch, QUE_IN_FREE) < 1)
		return 0;

	if (queue_empty(ch, QUEUE_LOCAL_FREE))
		return 0;
#endif
	ppre->di_inp_buf = ppre->di_inp_buf_next;
	ppre->di_inp_buf_next = NULL;

	if (!ppre->di_mem_buf_dup_p) {/* use n */
		ppre->di_mem_buf_dup_p = ppre->di_inp_buf;
	}

	return 1;
}

static bool pp_check_buf_cfg(struct di_ch_s *pch)
{
	unsigned int ch;

	ch = pch->ch_id;

	if (di_que_is_empty(ch, QUE_PRE_NO_BUF))
		return false;
	if (di_que_is_empty(ch, QUE_POST_FREE))
		return false;

	return true;
}

static bool pp_check_buf_post(struct di_ch_s *pch)
{
	unsigned int ch;

	ch = pch->ch_id;

	if (di_que_is_empty(ch, QUE_PST_NO_BUF))
		return false;

	return true;
}

static void pp_buf_cp(struct di_buf_s *buft, struct di_buf_s *buff)
{
	buft->blk_buf	= buff->blk_buf;
	buft->nr_adr	= buff->nr_adr;
	buft->afbc_adr	= buff->afbc_adr;
	buft->afbct_adr	= buff->afbct_adr;
	buft->dw_adr	= buff->dw_adr;
	buft->afbc_crc	= buff->afbc_crc;
	buft->adr_start	= buff->adr_start;
	buft->hf_adr	= buff->hf_adr;
	buft->en_hf	= buff->en_hf;
	buft->hf_done	= buff->hf_done;
//	buft->pat_buf	= buff->pat_buf;
	buft->nr_size	= buff->nr_size;
	buft->tab_size	= buff->tab_size;
	buft->canvas_height	= buff->canvas_height;
	buft->canvas_width[NR_CANVAS]	= buff->canvas_width[NR_CANVAS];
	buft->buf_is_i	= buff->buf_is_i;
	buft->flg_null	= buff->flg_null;
	buft->in_buf	= buff->in_buf;
	buft->field_count = buff->field_count;
	buft->afbce_out_yuv420_10 = buff->afbce_out_yuv420_10;
	buft->c.buffer	= buff->c.buffer;
	buft->c.src_is_i	= buff->c.src_is_i;
	buft->buf_hsize	= buff->buf_hsize;
}

static void pp_buf_clear(struct di_buf_s *buff)
{
	if (!buff) {
		PR_ERR("%s:no buffer\n", __func__);
		return;
	}
	/* clear */
	buff->blk_buf	= NULL;
	buff->flg_null	= 1;
	buff->buf_is_i	= 0;
	buff->afbc_crc	= 0;
//	buff->pat_buf	= 0;
	buff->adr_start = 0;
	buff->nr_adr	= 0;
	buff->afbc_adr	= 0;
	buff->hf_adr	= 0;
	buff->hf_done	= 0;
	buff->en_hf	= 0;
	buff->afbct_adr	= 0;
	buff->dw_adr	= 0;
	buff->canvas_height	= 0;
	buff->canvas_width[NR_CANVAS] = 0;
	buff->nr_size	= 0;
	buff->tab_size	= 0;
	buff->in_buf	= NULL;
	buff->field_count = 0;
	buff->afbce_out_yuv420_10 = 0;
	buff->c.buffer	= NULL;
	buff->c.src_is_i	= false;
	buff->buf_hsize	= 0;
}

static struct di_buf_s *pp_pst_2_local(struct di_ch_s *pch)
{
	struct di_buf_s *di_buf = NULL;
	struct di_buf_s *buf_pst = NULL;
	unsigned int ch;
//ary 2020-12-09	ulong irq_flag2 = 0;

	if (!pp_check_buf_cfg(pch))
		return di_buf;

	ch = pch->ch_id;

	di_buf = di_que_out_to_di_buf(ch, QUE_PRE_NO_BUF);
//ary 2020-12-09	di_lock_irqfiq_save(irq_flag2);
	buf_pst = di_que_out_to_di_buf(ch, QUE_POST_FREE);

	pp_buf_cp(di_buf, buf_pst);
	pp_buf_clear(buf_pst);
	memcpy(&di_buf->hf, &buf_pst->hf, sizeof(di_buf->hf));
	#ifdef HOLD_ONE_FRAME
	di_que_in(ch, QUE_PST_NO_BUF, buf_pst);
	#else
	di_buf->di_buf_post = buf_pst;
	#endif
	/* debug */
	//dbg_buf_log_save(pch, di_buf, 1);
	//dbg_buf_log_save(pch, buf_pst, 2);

//ary 2020-12-09	di_unlock_irqfiq_restore(irq_flag2);

	return di_buf;
}

static struct di_buf_s *pp_local_2_post(struct di_ch_s *pch,
					struct di_buf_s *di_buf)
{
	struct di_buf_s *buf_pst = NULL;
	unsigned int ch;
	//ulong irq_flag2 = 0;

	if (!pp_check_buf_post(pch))
		return NULL;

	ch = pch->ch_id;
	//di_lock_irqfiq_save(irq_flag2);
	#ifdef HOLD_ONE_FRAME
	buf_pst = di_que_out_to_di_buf(ch, QUE_PST_NO_BUF);
	//di_unlock_irqfiq_restore(irq_flag2);
	if (!buf_pst)
		return NULL;
	#else
	buf_pst = di_buf->di_buf_post;
	di_buf->di_buf_post = NULL;
	#endif
	pp_buf_cp(buf_pst, di_buf);

	/* hf */
	memcpy(&buf_pst->hf, &di_buf->hf, sizeof(buf_pst->hf));
	if (di_dbg & DBG_M_IC) {
		dim_print_hf(&buf_pst->hf);
		dbg_ic("hf add2 =0x%lx\n", buf_pst->hf_adr);
	}

	//di_que_in(ch, QUE_PRE_NO_BUF, di_buf);

	return buf_pst;
}

static void pp_drop_frame(struct di_buf_s *di_buf,
			  unsigned int channel)
{
//ary 2020-12-09	ulong irq_flag2 = 0;

//ary 2020-12-09	di_lock_irqfiq_save(irq_flag2);

	if (dimp_get(edi_mp_post_wr_en) &&
	    dimp_get(edi_mp_post_wr_support)) {
		//queue_in(channel, di_buf, QUEUE_POST_DOING);
		di_que_in(channel, QUE_POST_DOING, di_buf);
	} else {
		//no use di_que_in(channel, QUE_POST_READY, di_buf);
	}
	dim_tr_ops.post_do(di_buf->vframe->index_disp);
	dim_print("di:ch[%d]:%dth %s[%d] => post ready %u ms.\n",
		  channel,
		  frame_count,
		  vframe_type_name[di_buf->type], di_buf->index,
		  jiffies_to_msecs(jiffies_64 -
		  di_buf->vframe->ready_jiffies64));

//ary 2020-12-09	di_unlock_irqfiq_restore(irq_flag2);
}

static void dimpst_fill_outvf(struct vframe_s *vfm,
			      struct di_buf_s *di_buf,
			      enum EDPST_OUT_MODE mode);
//static void dimpst_fill_outvf_ext(struct vframe_s *vfm,
//			      struct di_buf_s *di_buf,
//			      enum EDPST_OUT_MODE mode);

static void re_build_buf(struct di_ch_s *pch, enum EDI_SGN sgn)
{
	bool is_4k = false;
	struct div2_mm_s *mm;
	const struct di_mm_cfg_s *ptab;
	struct mtsk_cmd_s blk_cmd;
	unsigned int ch;
	unsigned int release_post = 0, length_keep = 0;
	unsigned int post_nub;

	if (sgn == EDI_SGN_4K)
		is_4k  = true;

	ch = pch->ch_id;
	mm = dim_mm_get(ch);
	/* free buf -> release */

	if (mm->sts.flg_alloced)
		release_post = mem_release_free(pch);
	else
		length_keep = mm->cfg.num_rebuild_keep;
	//ndis_cnt(pch, QBF_NDIS_Q_KEEP);
	mem_2_blk(pch);
	mtsk_release(ch, ECMD_BLK_RELEASE);

	/* mm change*/
	ptab = di_get_mm_tab(is_4k, pch);

	mm->cfg.di_h = ptab->di_h;
	mm->cfg.di_w = ptab->di_w;
	mm->cfg.num_local = ptab->num_local;
	mm->cfg.num_post = ptab->num_post;
	mm->cfg.num_step1_post = ptab->num_step1_post;

	post_nub = cfgg(POST_NUB);
	if (post_nub && post_nub < POST_BUF_NUM)
		mm->cfg.num_post = post_nub;

	if (pch->ponly && dip_is_ponly_sct_mem(pch))
		mm->cfg.dis_afbce = 0;
	else if ((!is_4k &&
	     ((cfggch(pch, POUT_FMT) == 4) || (cfggch(pch, POUT_FMT) == 6))))
		mm->cfg.dis_afbce = 1;
	else
		mm->cfg.dis_afbce = 0;

	di_cnt_post_buf(pch);
	if (cfgeq(MEM_FLAG, EDI_MEM_M_CMA)	||
	    cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_A)	||
	    cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_B)) {	/*trig cma alloc*/

		blk_cmd.cmd = ECMD_BLK_ALLOC;
		blk_cmd.hf_need = 0;
		if (mm->cfg.num_local) {
			/*pre idat*/
			pre_sec_alloc(pch, mm->cfg.dat_idat_flg.d32);

			/* pre */
			blk_cmd.nub = mm->cfg.num_local;
			blk_cmd.flg.d32 = mm->cfg.ibuf_flg.d32;

			mtask_send_cmd(ch, &blk_cmd);
		}

		/* post */
		if (mm->cfg.pbuf_flg.b.typ == EDIM_BLK_TYP_PSCT) {
			if (mm->cfg.num_post)
				sct_sw_on(pch,
					mm->cfg.num_post,
					mm->cfg.pbuf_flg.b.tvp,
					mm->cfg.pst_buf_size);
			else
				PR_WARN("post is 0, no alloc\n");
		} else {
			sct_sw_off_rebuild(pch);
		}
		if (mm->sts.flg_alloced)
			release_post += mem_release_sct_wait(pch);

		if (mm->sts.flg_alloced)
			post_nub = release_post;
		else
			post_nub = mm->cfg.num_post - length_keep +
				   mm->cfg.num_rebuild_alloc;

		PR_INF("%s:allock nub[%d], flg[%d]\n", __func__, post_nub, mm->sts.flg_alloced);
		blk_cmd.nub = post_nub;//mm->cfg.num_post;

		blk_cmd.flg.d32 = mm->cfg.pbuf_flg.d32;
		if (mm->cfg.size_buf_hf)
			blk_cmd.hf_need = 1;
		else
			blk_cmd.hf_need = 0;
		mtask_send_cmd(ch, &blk_cmd);
		mm->sts.flg_alloced = true;
	}
}

/* 0: bypass ok */
unsigned char dim_pre_bypass(struct di_ch_s *pch)
{
	struct dim_nins_s *nins = NULL;
	struct vframe_s *vframe;
	unsigned int ch;
	//bool is_bypass;
	struct di_pre_stru_s *ppre;
	unsigned char bypassr;
	struct di_buf_s *di_buf = NULL;
	struct div2_mm_s *mm;
	enum EDI_SGN sgn;

	ch = pch->ch_id;
	/* pre check */
	if (!di_bypass_state_get(ch))
		return 71;
	if (di_que_list_count(ch, QUE_IN_FREE) < 1)
		return 72;
//	if (dim_is_pre_link_l() && dim_is_pre_link_cnt() < 1)
//		return 0x51;

	nins = nins_peek_pre(pch);
	if (!nins)
		return 73;
	vframe = &nins->c.vfm_cp;

	bypassr = is_bypass2(vframe, ch);
	if (!bypassr && !(vframe->type & VIDTYPE_V4L_EOS)) {
		mm = dim_mm_get(ch);
		if (!mm->sts.flg_alloced) {
			sgn = di_vframe_2_sgn(vframe);
			//if (IS_I_SRC(vframe->type))
			if (!pch->ponly && sgn < EDI_SGN_4K)
				pch->sumx.need_local = true;
			else
				pch->sumx.need_local = false;
			re_build_buf(pch, sgn);
			pch->sumx.flg_rebuild = 1;
			PR_INF("%s:ch[%d]:rebuild\n", __func__, ch);
		}
		//pch->sumx.vfm_bypass = false;
		return 74;
	}
	//pch->sumx.vfm_bypass = true;
	nins = nins_get(pch);
	if (!nins)
		return 75;

	if (nins->c.cnt < 3) {
		if (nins->c.cnt == 0)
			dbg_timer(ch, EDBG_TIMER_1_PRE_CFG);
		else if (nins->c.cnt == 1)
			dbg_timer(ch, EDBG_TIMER_2_PRE_CFG);
		else if (nins->c.cnt == 2)
			dbg_timer(ch, EDBG_TIMER_3_PRE_CFG);
	}

	dim_bypass_set(pch, 1, bypassr);
	ppre = get_pre_stru(ch);

	/*mem check*/
	memcpy(&ppre->vfm_cpy, vframe, sizeof(ppre->vfm_cpy));
	vframe = &nins->c.vfm_cp;
	//dbg_save_vf_data(vframe);
	dim_tr_ops.pre_get(vframe->index_disp);
	if (dip_itf_is_vfm(pch))
		didbg_vframe_in_copy(ch, nins->c.ori);
	else
		didbg_vframe_in_copy(ch, vframe);

	di_buf = di_que_out_to_di_buf(ch, QUE_IN_FREE);
	di_buf->dec_vf_state = 0;	/*dec vf keep*/
	if (dim_check_di_buf(di_buf, 10, ch))
		return 16;

	memcpy(di_buf->vframe, vframe, sizeof(vframe_t));
	di_buf->vframe->private_data = di_buf;
	di_buf->c.in = nins;
	di_buf->seq = ppre->in_seq;
	ppre->in_seq++;

	di_buf->post_proc_flag = 0;
	di_buf->is_nbypass = 1; /* 2020-12-07*/
	di_que_in(ch, QUE_PRE_READY, di_buf);

	if (pch->rsc_bypass.b.ponly_fst_cnt) {
		pch->rsc_bypass.b.ponly_fst_cnt--;
		//PR_INF("%s:%d\n", __func__, pch->rsc_bypass.b.ponly_fst_cnt);
	}
	dpre_vdoing(ch);

	return 0;
}

/* @ary_note: change return val for check block reason */
/* 0: ok
 * other: reason
 **/
static void di_load_pq_table(void);

unsigned char dim_pre_de_buf_config(unsigned int channel)
{
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *vframe;
	int /*i,*/ di_linked_buf_idx = -1;
	unsigned char change_type = 0;
	unsigned char change_type2 = 0;
	bool bit10_pack_patch = false;
	unsigned int width_roundup = 2;
	#ifdef VFM_ORI
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	#endif
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
//	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();
	int cfg_prog_proc = dimp_get(edi_mp_prog_proc_config);
	bool flg_1080i = false;
	bool flg_480i = false;
	unsigned int bypassr;
	struct di_ch_s *pch;
	unsigned int nv21_flg = 0;
	enum EDI_SGN sgn;
	struct div2_mm_s *mm;
	u32 cur_dw_width = 0xffff;
	u32 cur_dw_height = 0xffff;
	struct dim_nins_s *nins = NULL;
	enum EDPST_OUT_MODE tmpmode;
	unsigned int pps_w, pps_h;
	u32 typetmp;
	unsigned char chg_hdr = 0;
	bool is_hdr10p = false;

	pch = get_chdata(channel);

	if (di_blocking /*|| !dip_cma_st_is_ready(channel)*/)
		return 1;

	if (di_que_list_count(channel, QUE_IN_FREE) < 1)
		return 2;

	if (di_que_list_count(channel, QUE_IN_FREE) < 2	&&
	    !ppre->di_inp_buf_next)
		return 3;
#ifdef HIS_TEMP
	if (ppre->sgn_lv != EDI_SGN_4K &&
	    !pch->ponly		&&
	    queue_empty(channel, QUEUE_LOCAL_FREE))
		return 35;
#endif
	if (di_que_list_count(channel, QUE_PRE_READY) >= DI_PRE_READY_LIMIT)
		return 4;

	if (di_que_is_empty(channel, QUE_POST_FREE))
		return 5;
	di_buf = di_que_peek(channel, QUE_POST_FREE);
	mm = dim_mm_get(channel);
	if (!dip_itf_is_ins_exbuf(pch) &&
	    (!di_buf->blk_buf ||
	    di_buf->blk_buf->flg.d32 != mm->cfg.pbuf_flg.d32)) {
		if (!di_buf->blk_buf)
			PR_ERR("%s:pst no blk:idx[%d]\n",
		       __func__,
		       di_buf->index);
		else
			PR_ERR("%s:pst flgis err:buf:idx[%d] 0x%x->0x%x\n",
		       __func__,
		       di_buf->index,
		       mm->cfg.pbuf_flg.d32,
		       di_buf->blk_buf->flg.d32);

		return 6;
	}

	if (di_que_is_empty(channel, QUE_PRE_NO_BUF))
		return 7;

	if (dim_is_bypass(NULL, channel)) {
		/* some provider has problem if receiver
		 * get all buffers of provider
		 */
		int in_buf_num = 0;
		/*cur_lev = 0;*/
		dimp_set(edi_mp_cur_lev, 0);
		#ifdef VFM_ORI
		for (i = 0; i < MAX_IN_BUF_NUM; i++)
			if (pvframe_in[i])
				in_buf_num++;
		#else
		in_buf_num = nins_cnt_used_all(pch);
		#endif
		if (dip_itf_is_vfm(pch) &&
		    in_buf_num > BYPASS_GET_MAX_BUF_NUM) {
#ifdef DET3D
			if (ppre->vframe_interleave_flag == 0)
#endif
				return 30;
		}

		dimh_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else if (ppre->prog_proc_type == 2) {
		di_linked_buf_idx = peek_free_linked_buf(channel);
		if (di_linked_buf_idx == -1)
			return 8;
	} else if (ppre->prog_proc_type == 0x10) { //ary add for pp
		if (!pp_check_buf_cfg(pch))
			return 9;
	}
	if (ppre->di_inp_buf_next) {
		ppre->di_inp_buf = ppre->di_inp_buf_next;
		ppre->di_inp_buf_next = NULL;
#ifdef DI_BUFFER_DEBUG
		dim_print("%s: di_inp_buf_next %s[%d] => di_inp_buf\n",
			  __func__,
			  vframe_type_name[ppre->di_inp_buf->type],
			  ppre->di_inp_buf->index);
#endif
		if (!ppre->di_mem_buf_dup_p) {/* use n */
			ppre->di_mem_buf_dup_p = ppre->di_inp_buf;
#ifdef DI_BUFFER_DEBUG
			dim_print("%s: set di_mem_buf_dup_p to di_inp_buf\n",
				  __func__);
#endif
		}
	} else {

		nins = nins_peek_pre(pch);
		if (!nins)
			return 10;
		vframe = &nins->c.vfm_cp;

		/*eos check*/
		if ((vframe->type & VIDTYPE_V4L_EOS) ||
		    dbg_is_trig_eos(channel)) {
			nins = nins_get(pch);
			if (!nins)
				return 11;
			//vframe = &nins->c.vfm_cp;

			dbg_reg("cfg:eos\n");
			if (ppre->cur_prog_flag == 0		&&
			    ppre->field_count_for_cont > 0) {
				if (dip_itf_is_ins(pch) && (vframe->type & VIDTYPE_V4L_EOS))
					add_eos_in(channel, nins);
				else
					add_eos_in(channel, NULL);
			} else if (ppre->prog_proc_type == 0x10	&&
				   ppre->di_mem_buf_dup_p		&&
				   ppre->di_mem_buf_dup_p->flg_nr) {
				/*clear last p buf eos */
				ppre->di_mem_buf_dup_p->pre_ref_count = 0;
				ppre->di_mem_buf_dup_p->is_lastp = 1;
				#ifdef HOLD_ONE_FRAME
				di_que_in(channel, QUE_PRE_READY,
					  ppre->di_mem_buf_dup_p);
				#else
				p_ref_set_buf(ppre->di_mem_buf_dup_p, 0, 0, 2);
				pp_buf_clear(ppre->di_mem_buf_dup_p);
				di_que_in(channel, QUE_PRE_NO_BUF,
					  ppre->di_mem_buf_dup_p);
				#endif
				ppre->di_mem_buf_dup_p = NULL;
				if (dip_itf_is_ins(pch) && (vframe->type & VIDTYPE_V4L_EOS))
					add_eos_in(channel, nins);
				else
					add_eos_in(channel, NULL);
			} else {
				if (dip_itf_is_ins(pch) && (vframe->type & VIDTYPE_V4L_EOS))
					add_eos_in(channel, nins);
				else
					add_eos_in(channel, NULL);
			}
			ppre->cur_width = 0;
			ppre->di_mem_buf_dup_p	= NULL;
			ppre->di_chan2_buf_dup_p =  NULL;
			ppre->field_count_for_cont = 0;

			if (dip_itf_is_vfm(pch))
				nins_used_some_to_recycle(pch, nins);

			/*debug only*/
			//pre_run_flag = DI_RUN_FLAG_PAUSE;
			return 12;
		}
		/**************************************************/
		/*mem check*/
		memcpy(&ppre->vfm_cpy, vframe, sizeof(ppre->vfm_cpy));

		bypassr = is_bypass2(vframe, channel);//dim_is_bypass(vframe, channel);
		/*2020-12-02: here use di_buf->vframe is err*/
		change_type = is_source_change(vframe, channel);
		if (change_type)
			ppre->is_bypass_fg = 0;
		if (!bypassr && change_type) {
			sgn = di_vframe_2_sgn(vframe);
			if (pch->ponly && dip_is_ponly_sct_mem(pch)) {
				if (sgn != ppre->sgn_lv)
					PR_INF("%s:p:%d->%d\n", __func__,
				       ppre->sgn_lv, sgn);
				       ppre->sgn_lv = sgn;
			} else if ((sgn != ppre->sgn_lv)	&&
			    dim_afds()			&&
			    dip_is_support_4k(channel) &&
			    ((sgn == EDI_SGN_4K &&
			      ppre->sgn_lv <= EDI_SGN_HD) ||
			     (sgn <= EDI_SGN_HD &&
			      ppre->sgn_lv == EDI_SGN_4K))) {
				ppre->sgn_lv = sgn;
				if (!mm->cfg.fix_buf) {
					re_build_buf(pch, sgn);
					/*debug*/
					//pre_run_flag = DI_RUN_FLAG_PAUSE;
					//dimp_set(edi_mp_di_printk_flag, 1);
					//cnt_rebuild = 0;
					PR_INF("%s:rebuild:%d,sgn[%d]\n",
					       __func__,
					       mm->cfg.fix_buf, ppre->sgn_lv);
					return 13;
				}
			} else if (sgn != ppre->sgn_lv) {
				PR_INF("%s:%d->%d\n", __func__,
				       ppre->sgn_lv, sgn);
				       ppre->sgn_lv = sgn;
			}
		}

		sgn = di_vframe_2_sgn(vframe);
		if (sgn < EDI_SGN_4K &&
		    !pch->ponly &&
		    queue_empty(channel, QUEUE_LOCAL_FREE))
			return 35;

		/**************************************************/
		nins = nins_get(pch);
		if (!nins)
			return 14;
		if (nins->c.cnt < 3) {
			if (nins->c.cnt == 0)
				dbg_timer(channel, EDBG_TIMER_1_PRE_CFG);
			else if (nins->c.cnt == 1)
				dbg_timer(channel, EDBG_TIMER_2_PRE_CFG);
			else if (nins->c.cnt == 2)
				dbg_timer(channel, EDBG_TIMER_3_PRE_CFG);
		}

		vframe = &nins->c.vfm_cp;

		if (vframe->type & VIDTYPE_COMPRESS) {
			/* backup the original vf->width/height for bypass case */
			cur_dw_width = vframe->width;
			cur_dw_height = vframe->height;
			vframe->width = vframe->compWidth;
			vframe->height = vframe->compHeight;
		}

		/*dbg_vfm(vframe, 1);*/
		if (ppre->in_seq < kpi_frame_num) {
			PR_INF("[di_kpi] DI:ch[%d] get %dth vf[0x%p] from frontend %u ms.\n",
			       channel,
			       ppre->in_seq, vframe,
			       jiffies_to_msecs(jiffies_64 -
			       vframe->ready_jiffies64));
		}
		dim_tr_ops.pre_get(vframe->index_disp);
		if (dip_itf_is_vfm(pch))
			didbg_vframe_in_copy(channel, nins->c.ori);
		else
			didbg_vframe_in_copy(channel, vframe);
#ifdef MARK_SC2
		if (vframe->type & VIDTYPE_COMPRESS) {
			vframe->width = vframe->compWidth;
			vframe->height = vframe->compHeight;
		}
#endif
		dim_print("DI:ch[%d]get %dth vf[0x%p][%d] from front %u ms\n",
			  channel,
			  ppre->in_seq, vframe,
			  vframe->index_disp,
			  jiffies_to_msecs(jiffies_64 -
			  vframe->ready_jiffies64));
		vframe->prog_proc_config = (cfg_prog_proc & 0x20) >> 5;

		bit10_pack_patch =  (is_meson_gxtvbb_cpu() ||
							is_meson_gxl_cpu() ||
							is_meson_gxm_cpu());
		width_roundup = bit10_pack_patch ? 16 : width_roundup;
		#ifdef MARK_SC2 //tmp
		if (dimp_get(edi_mp_di_force_bit_mode) == 10)
			dimp_set(edi_mp_force_width,
				 roundup(vframe->width, width_roundup));
		else
			dimp_set(edi_mp_force_width, 0);
		#endif
		ppre->source_trans_fmt = vframe->trans_fmt;
		ppre->left_right = ppre->left_right ? 0 : 1;
		ppre->invert_flag =
			(vframe->type &  TB_DETECT_MASK) ? true : false;
		vframe->type &= ~TB_DETECT_MASK;

		if ((((invert_top_bot & 0x2) != 0)	||
		     ppre->invert_flag)			&&
		    (!is_progressive(vframe))) {
			if ((vframe->type & VIDTYPE_TYPEMASK) ==
			    VIDTYPE_INTERLACE_TOP) {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_BOTTOM;
			} else {
				vframe->type &= (~VIDTYPE_TYPEMASK);
				vframe->type |= VIDTYPE_INTERLACE_TOP;
			}
		}
		ppre->width_bk = vframe->width;
		if (dimp_get(edi_mp_force_width))
			vframe->width = dimp_get(edi_mp_force_width);
		if (dimp_get(edi_mp_force_height))
			vframe->height = dimp_get(edi_mp_force_height);
		vframe->width = roundup(vframe->width, width_roundup);

		/* backup frame motion info */
		vframe->combing_cur_lev = dimp_get(edi_mp_cur_lev);/*cur_lev;*/

		dim_print("%s: vf_get => 0x%p\n", __func__, nins->c.ori);

		di_buf = di_que_out_to_di_buf(channel, QUE_IN_FREE);
		di_buf->dec_vf_state = 0;	/*dec vf keep*/
		di_buf->is_eos = 0;
		if (dim_check_di_buf(di_buf, 10, channel))
			return 16;

		if (dimp_get(edi_mp_di_log_flag) & DI_LOG_VFRAME)
			dim_dump_vframe(vframe);

#ifdef SUPPORT_MPEG_TO_VDIN
		if ((!is_from_vdin(vframe)) &&
		    vframe->sig_fmt == TVIN_SIG_FMT_NULL &&
		    mpeg2vdin_flag) {
			struct vdin_arg_s vdin_arg;
			struct vdin_v4l2_ops_s *vdin_ops = get_vdin_v4l2_ops();

			vdin_arg.cmd = VDIN_CMD_GET_HISTGRAM;
			vdin_arg.private = (unsigned int)vframe;
			if (vdin_ops->tvin_vdin_func)
				vdin_ops->tvin_vdin_func(0, &vdin_arg);
		}
#endif
		memcpy(di_buf->vframe, vframe, sizeof(vframe_t));
		dim_dbg_pre_cnt(channel, "cf1");
		di_buf->width_bk = ppre->width_bk;	/*ary.sui 2019-04-23*/
		di_buf->dw_width_bk = cur_dw_width;
		di_buf->dw_height_bk = cur_dw_height;
		di_buf->vframe->private_data = di_buf;
		//10-09	di_buf->vframe->vf_ext = NULL; /*09-25*/

		di_buf->c.in = nins;
		//dbg_nins_log_buf(di_buf, 1);

		di_buf->seq = ppre->in_seq;
		ppre->in_seq++;
		di_buf->local_meta_used_size = 0;
		di_buf->local_ud_used_size = 0;

		if (is_ud_param_valid(vframe->vf_ud_param)) {
			struct userdata_param_t *ud_p = &vframe->vf_ud_param.ud_param;

			if (ud_p->pbuf_addr &&
			    ud_p->buf_len > 0 &&
			    ud_p->buf_len <= di_buf->local_ud_total_size) {
				memcpy(di_buf->local_ud, ud_p->pbuf_addr,
				       ud_p->buf_len);
				di_buf->local_ud_used_size = ud_p->buf_len;
			}
		}

		if ((((vframe->signal_type >> 8) & 0xff) == 0x30) &&
		    ((((vframe->signal_type >> 16) & 0xff) == 9) ||
		     (((vframe->signal_type >> 16) & 0xff) == 2)))
			is_hdr10p = true;
		if (vframe->source_type != VFRAME_SOURCE_TYPE_HDMI &&
		    (is_hdr10p ||
		     get_vframe_src_fmt(vframe) ==
		     VFRAME_SIGNAL_FMT_HDR10PRIME)) {
			struct provider_aux_req_s req;
			char *provider_name = NULL, *tmp_name = NULL;
			u32 sei_size = 0;

			memset(&req, 0, sizeof(struct provider_aux_req_s));
			if (dip_itf_is_vfm(pch)) {
				/* For vfm path, request aux buffer by notify interface */
				provider_name = vf_get_provider_name(pch->itf.dvfm.name);
				while (provider_name) {
					tmp_name =
						vf_get_provider_name(provider_name);
					if (!tmp_name)
						break;
					provider_name = tmp_name;
				}

				if (provider_name) {
					req.vf = vframe;
					vf_notify_provider_by_name(provider_name,
						VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
						(void *)&req);
				}
			} else if (get_vframe_src_fmt(vframe) != VFRAME_SIGNAL_FMT_INVALID) {
				/* for DI interface, v4l dec will update src_fmt */
				req.aux_buf = (char *)get_sei_from_src_fmt(vframe, &sei_size);
				req.aux_size = sei_size;
			}
			if (req.aux_buf && req.aux_size &&
			    di_buf->local_meta &&
			    di_buf->local_meta_total_size >= req.aux_size) {
				memcpy(di_buf->local_meta, req.aux_buf,
				       req.aux_size);
				di_buf->local_meta_used_size = req.aux_size;
			} else if (di_buf->local_meta && provider_name) {
				pr_info("DI:get meta data error aux_buf:%p\n",
					req.aux_buf);
				pr_info("DI:get meta data error size:%d (%d)\n",
					req.aux_size,
					di_buf->local_meta_total_size);
			}
		}

		pre_vinfo_set(channel, vframe);
		change_type = is_source_change(vframe, channel);
		if ((di_bypass_state_get(channel) == 0)	&&
		    change_type				&&
		    (ppre->cur_prog_flag == 0)		&&
		    /*(ppre->in_seq > 1)*/(ppre->field_count_for_cont > 0)) {
			/* last is i */
			add_eos_in(channel, NULL);
		}
		/* source change, when i mix p,force p as i*/
		//if (change_type == 1 || (change_type == 2 &&
		//			 ppre->cur_prog_flag == 1)) {
		if (change_type) {
			if (ppre->prog_proc_type == 0x10	&&
			    ppre->di_mem_buf_dup_p		&&
			    ppre->di_mem_buf_dup_p->flg_nr) {
			//#if 1
			/*clear last p buf eos */
				ppre->di_mem_buf_dup_p->pre_ref_count = 0;
				ppre->di_mem_buf_dup_p->is_lastp = 1;
				#ifdef HOLD_ONE_FRAME
				di_que_in(channel, QUE_PRE_READY,
					  ppre->di_mem_buf_dup_p);
				#else
				p_ref_set_buf(ppre->di_mem_buf_dup_p, 0, 0, 3);
				pp_buf_clear(ppre->di_mem_buf_dup_p);
				di_que_in(channel, QUE_PRE_NO_BUF,
					  ppre->di_mem_buf_dup_p);
				#endif
				ppre->di_mem_buf_dup_p = NULL;
			//#else

			//#endif
			} else if (ppre->di_mem_buf_dup_p) {
				/*avoid only 2 i field then p field*/
				if (ppre->cur_prog_flag == 0 &&
				    dimp_get(edi_mp_use_2_interlace_buff))
					ppre->di_mem_buf_dup_p->post_proc_flag =
						-1;
				ppre->di_mem_buf_dup_p->pre_ref_count = 0;
				ppre->di_mem_buf_dup_p = NULL;
			}
			if (ppre->di_chan2_buf_dup_p) {
				/*avoid only 1 i field then p field*/
				if (ppre->cur_prog_flag == 0 &&
				    dimp_get(edi_mp_use_2_interlace_buff))
					ppre->di_chan2_buf_dup_p->
					post_proc_flag = -1;
				ppre->di_chan2_buf_dup_p->pre_ref_count =
					0;
				ppre->di_chan2_buf_dup_p = NULL;
			}

			PR_INF("%s:ch[%d]:%ums %dth source change:\n",
			       "pre cfg",
			       channel,
			       jiffies_to_msecs(jiffies_64),
			       ppre->in_seq);
			PR_INF("source change:0x%x/%d/%d/%d=>0x%x/%d/%d/%d\n",
			       ppre->cur_inp_type,
			       ppre->cur_width,
			       ppre->cur_height,
			       ppre->cur_source_type,
			       di_buf->vframe->type,
			       di_buf->vframe->width,
			       di_buf->vframe->height,
			       di_buf->vframe->source_type);
			if (di_buf->vframe->type & VIDTYPE_COMPRESS) {
				ppre->cur_width =
					di_buf->vframe->compWidth;
				ppre->cur_height =
					di_buf->vframe->compHeight;
			} else {
				ppre->cur_width = di_buf->vframe->width;
				ppre->cur_height = di_buf->vframe->height;
			}
			/* pps count */
			if (dip_cfg_is_pps_4k(channel)) {
				if (VFMT_IS_I(ppre->cur_inp_type)) {
					de_devp->pps_enable = false;
				} else {
					de_devp->pps_enable = true;
					if (ppre->sgn_lv < EDI_SGN_4K) {
						dimp_set(edi_mp_pps_position, 1);
						dimp_set(edi_mp_pps_dstw, ppre->cur_width);
						dimp_set(edi_mp_pps_dsth, ppre->cur_height);
					} else if (ppre->sgn_lv == EDI_SGN_4K) {
						pps_w = ppre->cur_width;
						pps_h = ppre->cur_height;

						dip_pps_cnt_hv(&pps_w, &pps_h);
						dimp_set(edi_mp_pps_position, 1);
						dimp_set(edi_mp_pps_dstw, pps_w);
						dimp_set(edi_mp_pps_dsth, pps_h);
					}
				}
			}
			/* dw */
			if (pch->en_dw_mem && ppre->sgn_lv == EDI_SGN_4K)
				dim_dw_pre_para_init(pch, nins);
			else
				memset(&ppre->shrk_cfg,  0,
				       sizeof(ppre->shrk_cfg));

			ppre->cur_prog_flag =
				is_progressive(di_buf->vframe);
			if (ppre->cur_prog_flag) {
				ppre->prog_proc_type = 0x10;
				pch->sumx.need_local = 0;
			} else {
				ppre->prog_proc_type = 0;
				pch->sumx.need_local = true;
			}
			ppre->cur_inp_type = di_buf->vframe->type;
			ppre->cur_source_type =
				di_buf->vframe->source_type;
			ppre->cur_sig_fmt = di_buf->vframe->sig_fmt;
			ppre->orientation = di_buf->vframe->video_angle;
			ppre->source_change_flag = 1;
			ppre->input_size_change_flag = true;
			ppre->is_bypass_mem = 0;
#ifdef SUPPORT_MPEG_TO_VDIN
			if ((!is_from_vdin(vframe)) &&
			    vframe->sig_fmt == TVIN_SIG_FMT_NULL &&
			    (mpeg2vdin_en)) {
				struct vdin_arg_s vdin_arg;
				struct vdin_v4l2_ops_s *vdin_ops =
					get_vdin_v4l2_ops();
				vdin_arg.cmd = VDIN_CMD_MPEGIN_START;
				vdin_arg.h_active = ppre->cur_width;
				vdin_arg.v_active = ppre->cur_height;
				if (vdin_ops->tvin_vdin_func)
					vdin_ops->tvin_vdin_func(0, &vdin_arg);
				mpeg2vdin_flag = 1;
			}
#endif
			ppre->field_count_for_cont = 0;
			chg_hdr++;
		} else if (ppre->cur_prog_flag == 0) {
			#ifdef MARK_SC2
			/* check if top/bot interleaved */
			if (change_type == 2)
				/* source is i interleaves p fields */
				ppre->force_interlace = true;
			#endif
			if ((ppre->cur_inp_type &
			VIDTYPE_TYPEMASK) == (di_buf->vframe->type &
			VIDTYPE_TYPEMASK)) {
				if ((di_buf->vframe->type &
				VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP)
					same_field_top_count++;
				else
					same_field_bot_count++;
			}
			ppre->cur_inp_type = di_buf->vframe->type;
		} else {
			ppre->cur_inp_type = di_buf->vframe->type;
		}
		/*--------------------------*/
		if (!change_type) {
			change_type2 = is_vinfo_change(channel);
			if (change_type2) {
				/*ppre->source_change_flag = 1;*/
				ppre->input_size_change_flag = true;
			}
		}

		/*--------------------------*/
		bypassr = dim_is_bypass(di_buf->vframe, channel);
		if (bypassr) {
			dim_bypass_set(pch, 1, bypassr);
		} else {
			bypassr = dim_polic_is_bypass(pch, di_buf->vframe);
			dim_bypass_set(pch, 1, bypassr);
		}
		if (bypassr
			/*|| is_bypass_i_p()*/
			/*|| ((ppre->pre_ready_seq % 5)== 0)*/
			/*|| (ppre->pre_ready_seq == 10)*/
			) {
			if ((di_buf->vframe->type & VIDTYPE_COMPRESS) &&
			    (cur_dw_width != 0xffff) &&
			    (cur_dw_height != 0xffff)) {
				di_buf->vframe->width = cur_dw_width;
				di_buf->vframe->height = cur_dw_height;
			} else {
				di_buf->vframe->width = di_buf->width_bk;
			}
			/* bypass progressive */
			di_buf->seq = ppre->pre_ready_seq++;
			di_buf->post_ref_count = 0;
			/*cur_lev = 0;*/
			dimp_set(edi_mp_cur_lev, 0);
			if (ppre->source_change_flag) {
				di_buf->new_format_flag = 1;
				ppre->source_change_flag = 0;
			} else {
				di_buf->new_format_flag = 0;
			}
			dcntr_check_bypass(vframe);

		if (di_bypass_state_get(channel) == 0) {
			//cnt_rebuild = 0; /*from no bypass to bypass*/
			ppre->is_bypass_all = true;
			bset(&pch->self_trig_mask, 29);
			/*********************************/
			if (ppre->cur_prog_flag == 0		&&
			    ppre->in_seq > 1) {
				/* last is i */
				add_eos_in(channel, NULL);
				PR_INF("i to bypass, inset eos\n");
			}

			if (ppre->prog_proc_type == 0x10	&&
			    ppre->di_mem_buf_dup_p		&&
			    ppre->di_mem_buf_dup_p->flg_nr) {
				/*clear last p buf eos */
				ppre->di_mem_buf_dup_p->pre_ref_count = 0;
				ppre->di_mem_buf_dup_p->is_lastp = 1;
				#ifdef HOLD_ONE_FRAME
				di_que_in(channel, QUE_PRE_READY,
					  ppre->di_mem_buf_dup_p);
				#else
				p_ref_set_buf(ppre->di_mem_buf_dup_p, 0, 0, 4);
				pp_buf_clear(ppre->di_mem_buf_dup_p);
				di_que_in(channel, QUE_PRE_NO_BUF,
					  ppre->di_mem_buf_dup_p);
				#endif
				ppre->di_mem_buf_dup_p = NULL;
				PR_INF("p to bypass, push mem\n");
			/*********************************/
			} else if (ppre->di_mem_buf_dup_p) {
				ppre->di_mem_buf_dup_p->pre_ref_count = 0;
				ppre->di_mem_buf_dup_p = NULL;
			}
			if (ppre->di_chan2_buf_dup_p) {
				ppre->di_chan2_buf_dup_p->pre_ref_count = 0;
				ppre->di_chan2_buf_dup_p = NULL;
			}

				if (ppre->di_wr_buf) {
					ppre->di_wr_buf->pre_ref_count = 0;
					ppre->di_wr_buf->post_ref_count = 0;
					recycle_vframe_type_pre(ppre->di_wr_buf,
								channel);
#ifdef DI_BUFFER_DEBUG
			dim_print("%s: %s[%d] => recycle_list\n",
				  __func__,
				  vframe_type_name[ppre->di_wr_buf->type],
				  ppre->di_wr_buf->index);
#endif
					ppre->di_wr_buf = NULL;
				}

			di_buf->new_format_flag = 1;
			di_bypass_state_set(channel, true);/*bypass_state:1;*/
			pch->sumx.need_local = false;
			dim_print("%s:bypass_state = 1, is_bypass(0x%x)\n",
				  __func__, dim_is_bypass(NULL, channel));
#ifdef DI_BUFFER_DEBUG

			dim_print("trick_mode %d bypass_all %d\n",
				  trick_mode,
				  di_cfgx_get(channel, EDI_CFGX_BYPASS_ALL));
#endif
			}

			top_bot_config(di_buf);

			di_que_in(channel, QUE_PRE_READY, di_buf);
			/*if previous isn't bypass post_wr_buf not recycled */
			if (ppre->di_post_wr_buf && di_pre_rdma_enable) {
				queue_in(channel, ppre->di_post_inp_buf,
					 QUEUE_RECYCLE);
				ppre->di_post_inp_buf = NULL;
			}

			if ((bypass_pre & 0x2) && !ppre->cur_prog_flag)
				di_buf->post_proc_flag = -2;
			else
				di_buf->post_proc_flag = 0;

			dim_print("di:cfg:post_proc_flag=%d\n",
				  di_buf->post_proc_flag);
#ifdef DI_BUFFER_DEBUG
			dim_print("%s: %s[%d] => pre_ready_list\n",
				  __func__,
				  vframe_type_name[di_buf->type],
				  di_buf->index);
#endif
			di_buf->is_nbypass = 1; /* 2020-12-07*/
			if (pch->rsc_bypass.b.ponly_fst_cnt) {
				pch->rsc_bypass.b.ponly_fst_cnt--;
				//PR_INF("%s:%d\n", __func__, pch->rsc_bypass.b.ponly_fst_cnt);
			}
			return 17;
		} else if (is_progressive(di_buf->vframe)) {
			if (ppre->is_bypass_all) {
				ppre->input_size_change_flag = true;
				ppre->source_change_flag = 1;
				bclr(&pch->self_trig_mask, 29);
				//ppre->field_count_for_cont = 0;
			}
			ppre->is_bypass_all = false;
			if (ppre->prog_proc_type != 0x10 &&
			    is_handle_prog_frame_as_interlace(vframe) &&
			    is_progressive(vframe)) {
				struct di_buf_s *di_buf_tmp = NULL;
				#ifdef VFM_ORI
				pvframe_in[di_buf->index] = NULL;
				#else
				di_buf->c.in = NULL;
				#endif
				di_buf->vframe->type &=
					(~VIDTYPE_TYPEMASK);
				di_buf->vframe->type |=
					VIDTYPE_INTERLACE_TOP;
				di_buf->post_proc_flag = 0;

				di_buf_tmp = di_que_out_to_di_buf(channel, QUE_IN_FREE);
				if (dim_check_di_buf(di_buf_tmp, 10, channel)) {
					recycle_vframe_type_pre(di_buf, channel);
					PR_ERR("DI:no free in_buffer for progressive skip.\n");
					return 18;
				}

				di_buf_tmp->is_eos = 0;
				di_buf_tmp->vframe->private_data = di_buf_tmp;
				di_buf_tmp->seq = ppre->in_seq;
				ppre->in_seq++;
				#ifdef VFM_ORI
				pvframe_in[di_buf_tmp->index] = vframe;
				#else
				di_buf_tmp->c.in = nins;
				#endif
				memcpy(di_buf_tmp->vframe, vframe,
				       sizeof(vframe_t));
				ppre->di_inp_buf_next = di_buf_tmp;
				di_buf_tmp->vframe->type &=
					(~VIDTYPE_TYPEMASK);
				di_buf_tmp->vframe->type |=
					VIDTYPE_INTERLACE_BOTTOM;
				di_buf_tmp->post_proc_flag = 0;
				/*keep dec vf*/
				if (pch->ponly && dip_is_ponly_sct_mem(pch))
					di_buf_tmp->dec_vf_state = DI_BIT0;
				else if (cfggch(pch, KEEP_DEC_VF) == 1)
					di_buf_tmp->dec_vf_state = DI_BIT0;
				else if ((cfggch(pch, KEEP_DEC_VF) == 2) &&
					 (ppre->sgn_lv == EDI_SGN_4K))
					di_buf_tmp->dec_vf_state = DI_BIT0;
				ppre->di_inp_buf = di_buf;
#ifdef DI_BUFFER_DEBUG
				dim_print("%s:%s[%d]>in_bf;%s[%d]>in_bf_next\n",
					  __func__,
					  vframe_type_name[di_buf->type],
					  di_buf->index,
					  vframe_type_name[di_buf_tmp->type],
					  di_buf_tmp->index);
#endif
				if (!ppre->di_mem_buf_dup_p) {
					ppre->di_mem_buf_dup_p = di_buf;
#ifdef DI_BUFFER_DEBUG
					dim_print("%s:set mem_buf to inp_buf\n",
						  __func__);
#endif
				}
			} else {
				di_buf->post_proc_flag = 0;
				if ((cfg_prog_proc & 0x40) ||
				    ppre->force_interlace)
					di_buf->post_proc_flag = 1;

				ppre->di_inp_buf = di_buf;
#ifdef DI_BUFFER_DEBUG
				dim_print("%s: %s[%d] => di_inp_buf\n",
					  __func__,
					  vframe_type_name[di_buf->type],
					  di_buf->index);
#endif
				if (!ppre->di_mem_buf_dup_p) {
					/* use n */
					ppre->di_mem_buf_dup_p = di_buf;
#ifdef DI_BUFFER_DEBUG
					dim_print("%s:set mem_buf to inp_buf\n",
						  __func__);
#endif
				}
			}
		} else {
			if (ppre->is_bypass_all) {
				ppre->input_size_change_flag = true;
				ppre->source_change_flag = 1;
				//ppre->field_count_for_cont = 0;
			}
			ppre->is_bypass_all = false;
			/*********************************/
			if (di_buf->vframe->width >= 1920 &&
			    di_buf->vframe->height >= 1080)
				flg_1080i = true;
			else if ((di_buf->vframe->width == 720) &&
				 (di_buf->vframe->height == 480))
				flg_480i = true;
			/*********************************/

			if (!ppre->di_chan2_buf_dup_p) {
				ppre->field_count_for_cont = 0;
				/* ignore contp2rd and contprd */
			}
			di_buf->post_proc_flag = 1;
			ppre->di_inp_buf = di_buf;
			dim_print("%s: %s[%d] => di_inp_buf\n", __func__,
				  vframe_type_name[di_buf->type],
				  di_buf->index);

			if (!ppre->di_mem_buf_dup_p) {/* use n */
				ppre->di_mem_buf_dup_p = di_buf;
#ifdef DI_BUFFER_DEBUG
				dim_print("%s: set di_mem_buf to di_inp_buf\n",
					  __func__);
#endif
			}
		}
		dcntr_check(vframe);
		//dcntr_check(&ppre->vfm_cpy);
		if (dim_hdr_ops() &&
		    dim_hdr_ops()->get_change(vframe, chg_hdr))
			dim_hdr_ops()->get_setting(vframe);
	}
	/*dim_dbg_pre_cnt(channel, "cfg");*/
	/* di_wr_buf */
	if (ppre->prog_proc_type == 0 && (VFMT_IS_I(ppre->cur_inp_type))) {
		/* i */
		di_buf = get_di_buf_head(channel, QUEUE_LOCAL_FREE);
		if (dim_check_di_buf(di_buf, 11, channel)) {
			/* recycle_keep_buffer();
			 *  pr_dbg("%s:recycle keep buffer\n", __func__);
			 */
			recycle_vframe_type_pre(ppre->di_inp_buf, channel);
			return 19;
		}
		queue_out(channel, di_buf);/*QUEUE_LOCAL_FREE*/
		if (ppre->prog_proc_type & 0x10)
			di_buf->canvas_config_flag = 1;
		else
			di_buf->canvas_config_flag = 2;
		di_buf->di_wr_linked_buf = NULL;

		if (dimp_get(edi_mp_bypass_post_state)) {
			PR_INF("%s:no post buffer\n", __func__);
		} else {
			di_buf->di_buf_post =
				di_que_out_to_di_buf(channel, QUE_POST_FREE);
			if (!di_buf->di_buf_post) {
				PR_ERR("%s:no post buf\n", __func__);
				return 20;
			}
			if (ppre->input_size_change_flag)
				di_buf->di_buf_post->trig_post_update = 1;
			else
				di_buf->di_buf_post->trig_post_update = 0;
			di_buf->di_buf_post->c.src_is_i = true;
			mem_resize_buf(pch, di_buf->di_buf_post);
			/*hf*/
			if (di_buf->di_buf_post->hf_adr && pch->en_hf)
				di_buf->di_buf_post->en_hf = 1;
			else
				di_buf->di_buf_post->en_hf = 0;

			di_buf->di_buf_post->hf_done = 0;
			dbg_ic("hf:i cfg:%px:%d\n", &di_buf->di_buf_post->hf,
				di_buf->di_buf_post->en_hf);

			/*hdr*/
			di_buf->di_buf_post->c.en_hdr = false;
			di_buf->c.en_hdr = false;
			if (dim_hdr_ops()) {
				if (dim_hdr_ops()->get_pre_post == 0)
					di_buf->di_buf_post->c.en_hdr = true;
			}

			dim_pqrpt_init(&di_buf->di_buf_post->pq_rpt);
			if (dip_itf_is_ins(pch) && dim_dbg_new_int(2))
				dim_dbg_buffer2(di_buf->di_buf_post->c.buffer, 3);
			//dim_pqrpt_init(&di_buf->di_buf_post->pq_rpt);
		}

	} else if (ppre->prog_proc_type == 2) {
		/* p use 2 i buf */
		di_linked_buf_idx = peek_free_linked_buf(channel);
		if (di_linked_buf_idx != -1)
			di_buf = get_free_linked_buf(di_linked_buf_idx,
						     channel);
		else
			di_buf = NULL;
		if (!di_buf) {
			/* recycle_vframe_type_pre(di_pre_stru.di_inp_buf);
			 *save for next process
			 */
			//recycle_keep_buffer(channel);
			ppre->di_inp_buf_next = ppre->di_inp_buf;
			return 21;
		}
		di_buf->post_proc_flag = 0;
		di_buf->di_wr_linked_buf->pre_ref_count = 0;
		di_buf->di_wr_linked_buf->post_ref_count = 0;
		di_buf->canvas_config_flag = 1;
		di_buf->c.src_is_i = false;
#ifdef TEST_4K_NR
	} else if (ppre->prog_proc_type == 0x10) {
/********************************************************/
/* di_buf is local buf */
		//di_que_out_to_di_buf(channel, QUE_POST_FREE);
		di_buf = pp_pst_2_local(pch);
		if (dim_check_di_buf(di_buf, 17, channel)) {
			//di_unlock_irqfiq_restore(irq_flag2);
			return 22;
		}
		mem_resize_buf(pch, di_buf);
		/* hdr */
		di_buf->c.en_hdr = false;
		di_buf->di_buf_post->c.en_hdr = false;
		if (dim_hdr_ops() &&
		    dim_hdr_ops()->get_pre_post() == 1)
			di_buf->c.en_hdr = true;

		di_buf->post_proc_flag = 0;
		di_buf->canvas_config_flag = 1;
		di_buf->di_wr_linked_buf = NULL;
		di_buf->c.src_is_i = false;
		if (di_buf->hf_adr && pch->en_hf)
			di_buf->en_hf = 1;
		else
			di_buf->en_hf = 0;

		di_buf->hf_done = 0;
		dbg_ic("hf:p cfg:%px:%d\n", &di_buf->hf, di_buf->en_hf);
		//if (dim_cfg_pre_nv21(0)) {
		if (pch->ponly && dip_is_ponly_sct_mem(pch)) {
			nv21_flg = 0;
			di_buf->flg_nv21 = 0;
		} else if ((cfggch(pch, POUT_FMT) == 1) || (cfggch(pch, POUT_FMT) == 2)) {
			nv21_flg = 1; /*nv21*/
			di_buf->flg_nv21 = cfggch(pch, POUT_FMT);
		} else if ((cfggch(pch, POUT_FMT) == 5) &&
			   (ppre->sgn_lv == EDI_SGN_4K)) {
			nv21_flg = 1; /*nv21*/
			di_buf->flg_nv21 = 1;
		}
		/*keep dec vf*/
		//di_buf->dec_vf_state = DI_BIT0;
		if (pch->ponly && dip_is_ponly_sct_mem(pch))
			ppre->di_inp_buf->dec_vf_state = DI_BIT0;
		else if (cfggch(pch, KEEP_DEC_VF) == 1)
			ppre->di_inp_buf->dec_vf_state = DI_BIT0;
		else if ((cfggch(pch, KEEP_DEC_VF) == 2) &&
			 (ppre->sgn_lv == EDI_SGN_4K))
			ppre->di_inp_buf->dec_vf_state = DI_BIT0;

		if ((dip_is_support_nv2110(channel)) &&
		    ppre->sgn_lv == EDI_SGN_4K)
			di_buf->afbce_out_yuv420_10 = 1;
		else
			di_buf->afbce_out_yuv420_10 = 0;

		if (ppre->shrk_cfg.shrk_en) {
			dw_pre_sync_addr(&ppre->dw_wr_dvfm, di_buf);
			di_buf->dw_have = true;
			ppre->dw_wr_dvfm.is_dw = true;
		} else {
			di_buf->dw_have = false;
			ppre->dw_wr_dvfm.is_dw = false;
		}

		if (ppre->input_size_change_flag)
			di_buf->trig_post_update = 1;
		else
			di_buf->trig_post_update = 0;
#endif
	} else {
		di_buf = get_di_buf_head(channel, QUEUE_LOCAL_FREE);
		if (dim_check_di_buf(di_buf, 11, channel)) {
			/* recycle_keep_buffer();
			 *  pr_dbg("%s:recycle keep buffer\n", __func__);
			 */
			recycle_vframe_type_pre(ppre->di_inp_buf, channel);
			return 23;
		}
		queue_out(channel, di_buf);/*QUEUE_LOCAL_FREE*/
		if (ppre->prog_proc_type & 0x10)
			di_buf->canvas_config_flag = 1;
		else
			di_buf->canvas_config_flag = 2;
		di_buf->di_wr_linked_buf = NULL;
	}

	di_buf->afbc_sgn_cfg	= 0;
	di_buf->sgn_lv		= ppre->sgn_lv;
	ppre->di_wr_buf		= di_buf;
	ppre->di_wr_buf->pre_ref_count = 1;
#ifdef DBG_TEST_CRC_P
	//if (ppre->di_wr_buf->blk_buf->flg.b.typ != EDIM_BLK_TYP_PSCT)
	if (ppre->di_wr_buf->blk_buf &&
	    (ppre->di_wr_buf->blk_buf->flg.b.typ == EDIM_BLK_TYP_OLDI ||
	    ppre->di_wr_buf->blk_buf->flg.b.typ == EDIM_BLK_TYP_OLDP))
		dbg_checkcrc(ppre->di_wr_buf, 2); //debug
#endif
#ifdef DI_BUFFER_DEBUG
	dim_print("%s: %s[%d] => di_wr_buf\n", __func__,
		  vframe_type_name[di_buf->type], di_buf->index);
	if (di_buf->di_wr_linked_buf)
		dim_print("%s: linked %s[%d] => di_wr_buf\n", __func__,
			  vframe_type_name[di_buf->di_wr_linked_buf->type],
			  di_buf->di_wr_linked_buf->index);
#endif
	if (ppre->cur_inp_type & VIDTYPE_COMPRESS) {
		ppre->di_inp_buf->vframe->width =
			ppre->di_inp_buf->vframe->compWidth;
		ppre->di_inp_buf->vframe->height =
			ppre->di_inp_buf->vframe->compHeight;
	}

	memcpy(di_buf->vframe,
	       ppre->di_inp_buf->vframe, sizeof(vframe_t));
	di_buf->dw_width_bk = cur_dw_width;
	di_buf->dw_height_bk = cur_dw_height;
	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
	di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
	//if (di_buf->vframe->width == 3840 && di_buf->vframe->height == 2160)
	if (ppre->sgn_lv == EDI_SGN_4K) {
		di_buf->is_4k = 1;
		if (cfgg(BYPASS_MEM) == 2) {
			ppre->is_bypass_mem = 1;
		} else if (cfgg(BYPASS_MEM) == 3) {
			if (IS_VDIN_SRC(pch->src_type))
				ppre->is_bypass_mem = 0;
			else
				ppre->is_bypass_mem = 1;
		} else {
			ppre->is_bypass_mem = 0;
		}
	} else {
		di_buf->is_4k = 0;
	}

	/* set vframe bit info */
	di_buf->vframe->bitdepth &= ~(BITDEPTH_YMASK);
	di_buf->vframe->bitdepth &= ~(FULL_PACK_422_MODE);
	/* pps auto */
	if (de_devp->pps_enable & DI_BIT1) {
		if (VFMT_IS_I(ppre->cur_inp_type))
			dimp_set(edi_mp_pps_position, 0);
		else
			dimp_set(edi_mp_pps_position, 1);
	}
	if (di_buf->local_meta &&
	    ppre->di_inp_buf->local_meta &&
	    ppre->di_inp_buf->local_meta_used_size) {
		memcpy(di_buf->local_meta,
			ppre->di_inp_buf->local_meta,
			ppre->di_inp_buf->local_meta_used_size * sizeof(u8));
		di_buf->local_meta_used_size =
			ppre->di_inp_buf->local_meta_used_size;
	} else {
		di_buf->local_meta_used_size = 0;
	}

	if (di_buf->local_ud &&
	    ppre->di_inp_buf->local_ud &&
	    ppre->di_inp_buf->local_ud_used_size) {
		memcpy(di_buf->local_ud,
			ppre->di_inp_buf->local_ud,
			ppre->di_inp_buf->local_ud_used_size * sizeof(u8));
		di_buf->local_ud_used_size =
			ppre->di_inp_buf->local_ud_used_size;
	} else {
		di_buf->local_ud_used_size = 0;
	}

	if (de_devp->pps_enable && dimp_get(edi_mp_pps_position)) {
		if (dimp_get(edi_mp_pps_dstw) != di_buf->vframe->width) {
			di_buf->vframe->width = dimp_get(edi_mp_pps_dstw);
			ppre->width_bk = dimp_get(edi_mp_pps_dstw);
		}
		if (dimp_get(edi_mp_pps_dsth) != di_buf->vframe->height)
			di_buf->vframe->height = dimp_get(edi_mp_pps_dsth);
	} else if (de_devp->h_sc_down_en) {
		if (di_mp_uit_get(edi_mp_pre_hsc_down_width)
			!= di_buf->vframe->width) {
			pr_info("di: hscd %d to %d\n", di_buf->vframe->width,
				di_mp_uit_get(edi_mp_pre_hsc_down_width));
			di_buf->vframe->width =
				di_mp_uit_get(edi_mp_pre_hsc_down_width);
			/*di_pre_stru.width_bk = pre_hsc_down_width;*/
			ppre->width_bk =
				di_mp_uit_get(edi_mp_pre_hsc_down_width);
		}
	}
	if (dimp_get(edi_mp_di_force_bit_mode) == 10) {
		di_buf->vframe->bitdepth |= (BITDEPTH_Y10);
		if (dimp_get(edi_mp_full_422_pack))
			di_buf->vframe->bitdepth |= (FULL_PACK_422_MODE);
	} else {
		di_buf->vframe->bitdepth |= (BITDEPTH_Y8);
	}
	di_buf->width_bk = ppre->width_bk;	/*ary:2019-04-23*/
	di_buf->field_count = ppre->field_count_for_cont;
	if (ppre->prog_proc_type) {
		if (ppre->prog_proc_type != 0x10 ||
		    (ppre->prog_proc_type == 0x10 && !nv21_flg))
			di_buf->vframe->type = VIDTYPE_PROGRESSIVE |
				       VIDTYPE_VIU_422 |
				       VIDTYPE_VIU_SINGLE_PLANE |
				       VIDTYPE_VIU_FIELD;
		if (ppre->cur_inp_type & VIDTYPE_PRE_INTERLACE)
			di_buf->vframe->type |= VIDTYPE_PRE_INTERLACE;

		if (pch->ponly && dip_is_ponly_sct_mem(pch)) {
			if (dim_afds() && dim_afds()->cnt_sgn_mode) {
				if (IS_COMP_MODE
				   (ppre->di_inp_buf->vframe->type)) {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode
						(AFBC_SGN_P_H265);
				} else {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode(AFBC_SGN_P);
				}
			}
		} else if (ppre->prog_proc_type == 0x10 &&
		    (nv21_flg				||
		     (cfggch(pch, POUT_FMT) == 0)	||
		     (((cfggch(pch, POUT_FMT) == 4) ||
		      (cfggch(pch, POUT_FMT) == 5) ||
		      (cfggch(pch, POUT_FMT) == 6)) &&
		     (ppre->sgn_lv <= EDI_SGN_HD)))) {
			if (dim_afds() && dim_afds()->cnt_sgn_mode) {
				typetmp = ppre->di_inp_buf->vframe->type;
				if (IS_COMP_MODE(typetmp)) {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode
						(AFBC_SGN_P_H265_AS_P);
					if (ppre->di_inp_buf ==
					    ppre->di_mem_buf_dup_p) {
						di_buf->afbc_sgn_cfg =
							dim_afds()->cnt_sgn_mode
							(AFBC_SGN_H265_SINP);
					}
				} else {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode
						(AFBC_SGN_BYPSS);
				}
			}
		} else {
			if (dim_afds() && dim_afds()->cnt_sgn_mode) {
				typetmp = ppre->di_inp_buf->vframe->type;
				if (IS_COMP_MODE(typetmp)) {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode
						(AFBC_SGN_P_H265);
				} else {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode(AFBC_SGN_P);
				}
			}
		}
		if (nv21_flg && ppre->prog_proc_type == 0x10) {
			dim_print("cfg wr buf as nv21\n");
			//if (dip_itf_is_ins_exbuf(pch)) {
				if (cfggch(pch, POUT_FMT) == 1)
					tmpmode = EDPST_OUT_MODE_NV21;
				else
					tmpmode = EDPST_OUT_MODE_NV12;
				dimpst_fill_outvf(di_buf->vframe,
					  di_buf, tmpmode);
			//} else {
			//	dimpst_fill_outvf(di_buf->vframe,
			//		  di_buf, EDPST_OUT_MODE_NV21);
			//}
		}
	} else {
		if (((ppre->di_inp_buf->vframe->type &
		      VIDTYPE_TYPEMASK) ==
		     VIDTYPE_INTERLACE_TOP))
			di_buf->vframe->type = VIDTYPE_INTERLACE_TOP |
					       VIDTYPE_VIU_422 |
					       VIDTYPE_VIU_SINGLE_PLANE |
					       VIDTYPE_VIU_FIELD;
		else
			di_buf->vframe->type = VIDTYPE_INTERLACE_BOTTOM |
					       VIDTYPE_VIU_422 |
					       VIDTYPE_VIU_SINGLE_PLANE |
					       VIDTYPE_VIU_FIELD;
		/*add for vpp skip line ref*/
		if (di_bypass_state_get(channel) == 0)
			di_buf->vframe->type |= VIDTYPE_PRE_INTERLACE;

		if (VFMT_IS_I(ppre->cur_inp_type))
			di_buf->afbc_info |= DI_BIT1;/*flg is real i */
		#ifdef HIS_CODE
		if (DIM_IS_IC_EF(SC2) && dim_afds() &&
		    dim_afds()->cnt_sgn_mode) {
			if (IS_COMP_MODE(ppre->di_inp_buf->vframe->type)) {
				di_buf->afbc_sgn_cfg =
				dim_afds()->cnt_sgn_mode(AFBC_SGN_I_H265);
			} else {
				di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode(AFBC_SGN_I);
			}
		} else if (dim_afds() && dim_afds()->cnt_sgn_mode) {
			di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode(AFBC_SGN_BYPSS);
		}
		#else
		if (dim_afds()) {
			if (dim_afds()->is_sts(EAFBC_EN_6CH)) {
				if (IS_COMP_MODE(ppre->di_inp_buf->vframe->type)) {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode(AFBC_SGN_I_H265);
				} else {
					di_buf->afbc_sgn_cfg =
						dim_afds()->cnt_sgn_mode(AFBC_SGN_I);
				}
				if (cfggch(pch, IOUT_FMT) != 3)	//afbce
					dim_afds()->sgn_mode_set(&di_buf->afbc_sgn_cfg,
								 EAFBC_SNG_CLR_WR);
			} else {
				di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode(AFBC_SGN_BYPSS);
				if (cfggch(pch, IOUT_FMT) == 3)
					dim_afds()->sgn_mode_set(&di_buf->afbc_sgn_cfg,
								 EAFBC_SNG_SET_WR);
			}
		}
		#endif
	}

	if (is_bypass_post(channel)) {
		if (dimp_get(edi_mp_bypass_post_state) == 0)
			ppre->source_change_flag = 1;

		dimp_set(edi_mp_bypass_post_state, 1);
	} else {
		if (dimp_get(edi_mp_bypass_post_state))
			ppre->source_change_flag = 1;

		dimp_set(edi_mp_bypass_post_state, 0);
	}

	if (ppre->di_inp_buf->post_proc_flag == 0) {
		ppre->madi_enable = 0;
		ppre->mcdi_enable = 0;
		di_buf->post_proc_flag = 0;
		dimh_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else if (dimp_get(edi_mp_bypass_post_state)) {
		ppre->madi_enable = 0;
		ppre->mcdi_enable = 0;
		di_buf->post_proc_flag = 0;
		dimh_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else {
		ppre->madi_enable = (dimp_get(edi_mp_pre_enable_mask) & 1);
		ppre->mcdi_enable =
			((dimp_get(edi_mp_pre_enable_mask) >> 1) & 1);
		di_buf->post_proc_flag = 1;
		dimh_patch_post_update_mc_sw(DI_MC_SW_OTHER,
					     dimp_get(edi_mp_mcpre_en));/*en*/
	}
	if (ppre->di_mem_buf_dup_p == ppre->di_wr_buf ||
	    ppre->di_chan2_buf_dup_p == ppre->di_wr_buf) {
		pr_dbg("+++++++++++++++++++++++\n");
		if (recovery_flag == 0)
			recovery_log_reason = 12;

		recovery_flag++;

		return 24;
	}

	di_load_pq_table();

	if (is_meson_tl1_cpu()			&&
	    ppre->comb_mode			&&
	    flg_1080i) {
		ppre->combing_fix_en = false;
		get_ops_mtn()->fix_tl1_1080i_patch_sel(ppre->comb_mode);
	} else {
		ppre->combing_fix_en = di_mpr(combing_fix_en);
	}

	if (ppre->combing_fix_en) {
		if (flg_1080i)
			get_ops_mtn()->com_patch_pre_sw_set(1);
		else if (flg_480i)
			get_ops_mtn()->com_patch_pre_sw_set(2);
		else
			get_ops_mtn()->com_patch_pre_sw_set(0);
	}
	di_hf_polling_active(pch);

	return 0; /*ok*/
}

int dim_check_recycle_buf(unsigned int channel)
{
	struct di_buf_s *di_buf = NULL;/* , *ptmp; */
	struct di_buf_s *tmp;
	int itmp;
	int ret = 0;
	struct div2_mm_s *mm;
	struct di_ch_s *pch = get_chdata(channel);
#ifdef DI_BUFFER_DEBUG
	int type;
	int index;
#endif
	#ifdef VFM_ORI
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	#else
	struct dim_nins_s *ins = NULL;
	#endif
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	mm = dim_mm_get(channel);
	if (di_blocking)
		return ret;
	queue_for_each_entry(di_buf, channel, QUEUE_RECYCLE, list) {
		if (di_buf->pre_ref_count == 0 &&
		    di_buf->post_ref_count <= 0) {	/*ary maybe <=*/
			if (di_buf->type == VFRAME_TYPE_IN) {
				queue_out(channel, di_buf);
				if (di_buf->c.in) {
					ins = (struct dim_nins_s *)di_buf->c.in;
					nins_used_some_to_recycle(pch, ins);
					di_buf->c.in = NULL;
				}

				di_buf->invert_top_bot_flag = 0;
				di_buf->dec_vf_state = 0;
				di_que_in(channel, QUE_IN_FREE, di_buf);
				ppre->recycle_seq++;
				ret |= 1;
			} else {
				queue_out(channel, di_buf);
				di_buf->invert_top_bot_flag = 0;
				di_buf->afbc_sgn_cfg = 0;
				di_buf->flg_nr = 0;
				di_buf->flg_nv21 = 0;
				di_buf->afbce_out_yuv420_10 = 0;
				di_buf->post_ref_count = 0;/*ary maybe*/
				if (mm->cfg.num_local == 0) {
					if (di_buf->iat_buf) {
						qiat_in_ready(pch,
					(struct dim_iat_s *)di_buf->iat_buf);
						di_buf->iat_buf = NULL;
					}
					mem_release_one_inused(pch,
							       di_buf->blk_buf);
					di_buf->blk_buf = NULL;
					di_que_in(channel,
						  QUE_PRE_NO_BUF, di_buf);
				} else {
					if (di_buf->di_wr_linked_buf) {
						tmp = di_buf->di_wr_linked_buf;
						queue_in(channel,
							 tmp,
							 QUEUE_LOCAL_FREE);

						di_buf->di_wr_linked_buf = NULL;
					}
					queue_in(channel,
						 di_buf, QUEUE_LOCAL_FREE);
				}
				ret |= 2;
			}
#ifdef DI_BUFFER_DEBUG
			dim_print("%s: recycle %s[%d]\n", __func__,
				  vframe_type_name[di_buf->type],
				  di_buf->index);
#endif
		}
	}
	return ret;
}

#ifdef DET3D
static void set3d_view(enum tvin_trans_fmt trans_fmt, struct vframe_s *vf)
{
	struct vframe_view_s *left_eye, *right_eye;

	left_eye = &vf->left_eye;
	right_eye = &vf->right_eye;

	switch (trans_fmt) {
	case TVIN_TFMT_3D_DET_LR:
	case TVIN_TFMT_3D_LRH_OLOR:
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = vf->width >> 1;
		left_eye->height = vf->height;
		right_eye->start_x = vf->width >> 1;
		right_eye->start_y = 0;
		right_eye->width = vf->width >> 1;
		right_eye->height = vf->height;
		break;
	case TVIN_TFMT_3D_DET_TB:
	case TVIN_TFMT_3D_TB:
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = vf->width;
		left_eye->height = vf->height >> 1;
		right_eye->start_x = 0;
		right_eye->start_y = vf->height >> 1;
		right_eye->width = vf->width;
		right_eye->height = vf->height >> 1;
		break;
	case TVIN_TFMT_3D_DET_INTERLACE:
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = vf->width;
		left_eye->height = vf->height >> 1;
		right_eye->start_x = 0;
		right_eye->start_y = 0;
		right_eye->width = vf->width;
		right_eye->height = vf->height >> 1;
		break;
	case TVIN_TFMT_3D_DET_CHESSBOARD:
/***
 * LRLRLR	  LRLRLR
 * LRLRLR  or RLRLRL
 * LRLRLR	  LRLRLR
 * LRLRLR	  RLRLRL
 */
		break;
	default: /* 2D */
		left_eye->start_x = 0;
		left_eye->start_y = 0;
		left_eye->width = 0;
		left_eye->height = 0;
		right_eye->start_x = 0;
		right_eye->start_y = 0;
		right_eye->width = 0;
		right_eye->height = 0;
		break;
	}
}

/*
 * static int get_3d_info(struct vframe_s *vf)
 * {
 * int ret = 0;
 *
 * vf->trans_fmt = det3d_fmt_detect();
 * pr_dbg("[det3d..]new 3d fmt: %d\n", vf->trans_fmt);
 *
 * vdin_set_view(vf->trans_fmt, vf);
 *
 * return ret;
 * }
 */
static unsigned int det3d_frame_cnt = 50;
module_param_named(det3d_frame_cnt, det3d_frame_cnt, uint, 0644);
static void det3d_irq(unsigned int channel)
{
	unsigned int data32 = 0, likely_val = 0;
	unsigned long frame_sum = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (!dimp_get(edi_mp_det3d_en))
		return;

	data32 = get_ops_3d()->det3d_fmt_detect();/*det3d_fmt_detect();*/
	switch (data32) {
	case TVIN_TFMT_3D_DET_LR:
	case TVIN_TFMT_3D_LRH_OLOR:
		ppre->det_lr++;
		break;
	case TVIN_TFMT_3D_DET_TB:
	case TVIN_TFMT_3D_TB:
		ppre->det_tp++;
		break;
	case TVIN_TFMT_3D_DET_INTERLACE:
		ppre->det_la++;
		break;
	default:
		ppre->det_null++;
		break;
	}

	if (det3d_mode != data32) {
		det3d_mode = data32;
		dim_print("[det3d..]new 3d fmt: %d\n", det3d_mode);
	}
	if (frame_count > 20) {
		frame_sum = ppre->det_lr + ppre->det_tp
					+ ppre->det_la
					+ ppre->det_null;
		if ((frame_count % det3d_frame_cnt) || frame_sum > UINT_MAX)
			return;
		likely_val = max3(ppre->det_lr,
				  ppre->det_tp,
				  ppre->det_la);
		if (ppre->det_null >= likely_val)
			det3d_mode = 0;
		else if (likely_val == ppre->det_lr)
			det3d_mode = TVIN_TFMT_3D_LRH_OLOR;
		else if (likely_val == ppre->det_tp)
			det3d_mode = TVIN_TFMT_3D_TB;
		else
			det3d_mode = TVIN_TFMT_3D_DET_INTERLACE;
		ppre->det3d_trans_fmt = det3d_mode;
	} else {
		ppre->det3d_trans_fmt = 0;
	}
}
#endif

static unsigned int ro_mcdi_col_cfd[26];
static void get_mcinfo_from_reg_in_irq(unsigned int channel)
{
	unsigned int i = 0, ncolcrefsum = 0, blkcount = 0, *reg = NULL;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_buf_s *wr_buf;

	wr_buf = ppre->di_wr_buf;

	if (!wr_buf) {
		PR_ERR("%s:no di_wr_buf\n", __func__);
		return;
	}

/*get info for current field process by post*/
	wr_buf->curr_field_mcinfo.highvertfrqflg =
		(RD(MCDI_RO_HIGH_VERT_FRQ_FLG) & 0x1);
/* post:MCDI_MC_REL_GAIN_OFFST_0 */
	wr_buf->curr_field_mcinfo.motionparadoxflg =
		(RD(MCDI_RO_MOTION_PARADOX_FLG) & 0x1);
/* post:MCDI_MC_REL_GAIN_OFFST_0 */
	reg = wr_buf->curr_field_mcinfo.regs;
	for (i = 0; i < 26; i++) {
		ro_mcdi_col_cfd[i] = RD(0x2fb0 + i);
		wr_buf->curr_field_mcinfo.regs[i] = 0;
		if (!dimp_get(edi_mp_calc_mcinfo_en))
			*(reg + i) = ro_mcdi_col_cfd[i];
	}
	if (dimp_get(edi_mp_calc_mcinfo_en)) {
		blkcount = (ppre->cur_width + 4) / 5;
		for (i = 0; i < blkcount; i++) {
			ncolcrefsum +=
				((ro_mcdi_col_cfd[i / 32] >> (i % 32)) & 0x1);
			if (((ncolcrefsum + (blkcount >> 1)) << 8) /
			    blkcount > dimp_get(edi_mp_colcfd_thr))
				for (i = 0; i < blkcount; i++)
					*(reg + i / 32) += (1 << (i % 32));
		}
	}
}

static unsigned int bit_reverse(unsigned int val)
{
	unsigned int i = 0, res = 0;

	for (i = 0; i < 16; i++) {
		res |= (((val & (1 << i)) >> i) << (31 - i));
		res |= (((val & (1 << (31 - i))) << i) >> (31 - i));
	}
	return res;
}

static void set_post_mcinfo(struct mcinfo_pre_s *curr_field_mcinfo)
{
	unsigned int i = 0, value = 0;

	DIM_VSC_WR_MPG_BT(MCDI_MC_REL_GAIN_OFFST_0,
			  curr_field_mcinfo->highvertfrqflg, 24, 1);
	DIM_VSC_WR_MPG_BT(MCDI_MC_REL_GAIN_OFFST_0,
			  curr_field_mcinfo->motionparadoxflg, 25, 1);
	for (i = 0; i < 26; i++) {
		if (overturn)
			value = bit_reverse(curr_field_mcinfo->regs[i]);
		else
			value = curr_field_mcinfo->regs[i];
		DIM_VSYNC_WR_MPEG_REG(0x2f78 + i, value);
	}
}

static unsigned char intr_mode;

void dim_irq_pre(void)
{
	unsigned int channel;
	struct di_pre_stru_s *ppre;
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct di_hpre_s  *pre = get_hw_pre();
	u64 ctime;
	unsigned int data32 = RD(DI_INTR_CTRL);
	unsigned int mask32 = (data32 >> 16) & 0x3ff;
	unsigned int flag = 0;
	unsigned long irq_flg = 0;

	if (!sc2_dbg_is_en_pre_irq()) {
		sc2_dbg_pre_info(data32);
		return;
	}
	di_lock_irqfiq_save(irq_flg);	//2020-12-10
	channel = pre->curr_ch;
	ppre = pre->pres;
	data32 = RD(DI_INTR_CTRL);

	data32 &= 0x3fffffff;
	//if ((data32 & 1) == 0 && dimp_get(edi_mp_di_dbg_mask) & 8)
	//	pr_info("irq[%d]pre|post=0 write done.\n", irq);
	if (ppre->pre_de_busy) {
		/* only one inetrrupr mask should be enable */
		if ((data32 & 2) && !(mask32 & 2)) {
			dim_print("irq pre MTNWR ==ch[%d]\n", channel);
			flag = 1;
		} else if ((data32 & 1) && !(mask32 & 1)) {
			dim_print("irq pre NRWR ==ch[%d]\n", channel);
			flag = 1;
		} else {
			dim_print("irq pre DI IRQ 0x%x ==\n", data32);
			flag = 0;
		}
	} else {
		PR_WARN("irq:flag 0\n");
	}

	/* check timeout*/
	ctime = cur_to_usecs() - ppre->irq_time[0];
	if (ppre->timeout_check) {
		if (ctime < 300 && flag) {
			//DIM_DI_WR(DI_INTR_CTRL,
			//	  (data32 & 0xfffffffb) | (intr_mode << 30));
			ppre->timeout_check = false;
			di_unlock_irqfiq_restore(irq_flg);
			PR_WARN("irq delet %d\n", (unsigned int)ctime);
			return;
		}
		ppre->timeout_check = false;
	}
	if (flag) {
		if (DIM_IS_IC_EF(SC2))
			opl1()->pre_gl_sw(false);
		else
			hpre_gl_sw(false);
		DIM_DI_WR(DI_INTR_CTRL,
			  (data32 & 0xfffffffb) | (intr_mode << 30));
	}

	/*if (ppre->pre_de_busy == 0) {*/
	/*if (!di_pre_wait_irq_get()) {*/
	if (flag && (!atomic_dec_and_test(&get_hw_pre()->flg_wait_int))) {
		PR_ERR("%s:ch[%d]:enter:reg[0x%x]= 0x%x,dtab[%d]\n", __func__,
		       channel,
		       DI_INTR_CTRL,
		       RD(DI_INTR_CTRL),
		       pre->sdt_mode.op_crr);
		di_unlock_irqfiq_restore(irq_flg);	//2020-12-10
		return;
	}

	if (flag) {
		//ppre->irq_time[0] =
		//	(cur_to_msecs() - ppre->irq_time[0]);
		dim_tr_ops.pre(ppre->field_count_for_cont,
			       (unsigned long)ctime);

		/*add from valsi wang.feng*/
		if (!dim_dbg_cfg_disable_arb()) {
			dim_arb_sw(false);
			dim_arb_sw(true);
			dbg_ic("arb reset\n");
		} else {
			dbg_ic("arb not reset\n");
		}
		if (dimp_get(edi_mp_mcpre_en)) {
			get_mcinfo_from_reg_in_irq(channel);
			if ((is_meson_gxlx_cpu()		&&
			     ppre->field_count_for_cont >= 4)	||
			    is_meson_txhd_cpu())
				dimh_mc_pre_mv_irq();
			dimh_calc_lmv_base_mcinfo((ppre->cur_height >> 1),
						  ppre->di_wr_buf->mcinfo_adr_v,
						  /*ppre->mcinfo_size*/0);
		}
		get_ops_nr()->nr_process_in_irq();
		if ((data32 & 0x200) && de_devp->nrds_enable)
			dim_nr_ds_irq();
		/* disable mif */
		dimh_enable_di_pre_mif(false, dimp_get(edi_mp_mcpre_en));
		dcntr_dis();

		ppre->pre_de_busy = 0;

		if (get_init_flag(channel))
			/* pr_dbg("%s:up di sema\n", __func__); */
			task_send_ready(1);

		pre->flg_int_done = 1;
	}
	di_unlock_irqfiq_restore(irq_flg);	//2020-12-10
}

void dbg_irq(unsigned char *name)
{
	u64 timerus;

	timerus = cur_to_usecs();
	dim_print("irq:%s:%lu\n", name, timerus);
}

irqreturn_t dim_irq(int irq, void *dev_instance)
{
	struct di_hpre_s  *pre = get_hw_pre();
	unsigned long irq_flg;

	di_lock_irqfiq_save(irq_flg);
	dim_tr_ops.irq_aisr(1);
	if (pre->hf_busy && pre->hf_owner == 1) {
		pre->irq_nr = true; //debug hf timeout
		dbg_irq("nr");
		di_unlock_irqfiq_restore(irq_flg);
		return IRQ_HANDLED;
	}
	di_unlock_irqfiq_restore(irq_flg);
	dim_irq_pre();
	return IRQ_HANDLED;
}

void dim_post_irq_sub(int irq)
{
	unsigned int data32 = RD(DI_INTR_CTRL);
	unsigned int channel;
	struct di_post_stru_s *ppost;
	struct di_hpst_s  *pst = get_hw_pst();
	bool flg_right = false;

	if (!sc2_dbg_is_en_pst_irq()) {
		sc2_dbg_pst_info(data32);
		return;
	}

	channel = pst->curr_ch;
	ppost = pst->psts;

	data32 &= 0x3fffffff;
	if ((data32 & 4) == 0) {
		if (dimp_get(edi_mp_di_dbg_mask) & 8)
			pr_info("irq[%d]post write undone.\n", irq);
			return;
	}
	if (pst->state == EDI_PST_ST_SET && pst->flg_have_set)
		flg_right = true;

	if (pst->state != EDI_PST_ST_WAIT_INT &&
	    !pst->pst_tst_use			&&
	    !flg_right) {
		PR_ERR("%s:ch[%d]:s[%d]\n", __func__, channel, pst->state);
		ddbg_sw(EDI_LOG_TYPE_MOD, false);
		return;
	}
	dim_ddbg_mod_save(EDI_DBG_MOD_POST_IRQB, pst->curr_ch,
			  ppost->frame_cnt);
	dim_tr_ops.post_ir(0);

	if ((dimp_get(edi_mp_post_wr_en)	&&
	     dimp_get(edi_mp_post_wr_support))	&&
	    (data32 & 0x4)) {
		ppost->de_post_process_done = 1;
		ppost->post_de_busy = 0;
		ppost->irq_time =
			(cur_to_msecs() - ppost->irq_time);

		dim_tr_ops.post(ppost->post_wr_cnt, ppost->irq_time);

		DIM_DI_WR(DI_INTR_CTRL,
			  (data32 & 0xffff0004) | (intr_mode << 30));

		/* disable wr back avoid pps sreay in g12a */
		/* dim_DI_Wr_reg_bits(DI_POST_CTRL, 0, 7, 1); */
		if (DIM_IS_IC_EF(SC2))
			opl1()->pst_set_flow(1, EDI_POST_FLOW_STEP3_IRQ);
		else
			di_post_set_flow(1, EDI_POST_FLOW_STEP3_IRQ);
		dim_print("irq p ch[%d]done\n", channel);
		pst->flg_int_done = true;
		pst->flg_have_set = false;
	}
	dim_ddbg_mod_save(EDI_DBG_MOD_POST_IRQE, pst->curr_ch,
			  ppost->frame_cnt);

	if (get_init_flag(channel))
		task_send_ready(2);
}

irqreturn_t dim_post_irq(int irq, void *dev_instance)
{
	struct di_hpre_s  *pre = get_hw_pre();

	dim_tr_ops.irq_aisr(2);
	if (pre->hf_owner == 2 && pre->hf_busy) {
		dbg_irq("wr");
		return IRQ_HANDLED;
	}

	dim_post_irq_sub(irq);

	return IRQ_HANDLED;
}

irqreturn_t dim_aisr_irq(int irq, void *dev_instance)
{
	struct di_hpre_s  *pre = get_hw_pre();

	dim_tr_ops.irq_aisr(3);
	dbg_irq("aisr");

	if (pre->hf_busy) {
		//
		opl1()->aisr_disable();
		if (pre->hf_owner == 1) {
			dim_irq_pre();
			return IRQ_HANDLED;
		}
		if (pre->hf_owner == 2) {
			dim_post_irq_sub(irq);
			return IRQ_HANDLED;
		}
		PR_ERR("%s:\n", __func__);
	}

	return IRQ_HANDLED;
}

/*
 * di post process
 */
static void inc_post_ref_count(struct di_buf_s *di_buf)
{
	int i;	/*debug only:*/

	/* int post_blend_mode; */
	if (IS_ERR_OR_NULL(di_buf)) {
		PR_ERR("%s:\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 13;

		recovery_flag++;
		return;
	}

	if (di_buf->di_buf_dup_p[1])
		di_buf->di_buf_dup_p[1]->post_ref_count++;

	if (di_buf->di_buf_dup_p[0])
		di_buf->di_buf_dup_p[0]->post_ref_count++;

	if (di_buf->di_buf_dup_p[2])
		di_buf->di_buf_dup_p[2]->post_ref_count++;

	/*debug only:*/
	for (i = 0; i < 3; i++) {
		if (di_buf->di_buf_dup_p[i])
			dbg_post_ref("%s:pst_buf[%d],dup_p[%d],pref[%d]\n",
				     __func__,
				     di_buf->index,
				     i,
				     di_buf->di_buf_dup_p[i]->post_ref_count);
	}
}

static void dec_post_ref_count(struct di_buf_s *di_buf)
{
	int i;	/*debug only:*/

	if (IS_ERR_OR_NULL(di_buf)) {
		PR_ERR("%s:\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 14;

		recovery_flag++;
		return;
	}
	if (di_buf->pd_config.global_mode == PULL_DOWN_BUF1)
		return;
	if (di_buf->di_buf_dup_p[1])
		di_buf->di_buf_dup_p[1]->post_ref_count--;

	if (di_buf->di_buf_dup_p[0] &&
	    di_buf->di_buf_dup_p[0]->post_proc_flag != -2)
		di_buf->di_buf_dup_p[0]->post_ref_count--;

	if (di_buf->di_buf_dup_p[2])
		di_buf->di_buf_dup_p[2]->post_ref_count--;

	/*debug only:*/
	for (i = 0; i < 3; i++) {
		if (di_buf->di_buf_dup_p[i])
			dbg_post_ref("%s:pst_buf[%d],dup_p[%d],pref[%d]\n",
				     __func__,
				     di_buf->index,
				     i,
				     di_buf->di_buf_dup_p[i]->post_ref_count);
	}
}

/*early_process_fun*/
static int early_NONE(void)
{
	return 0;
}

int dim_do_post_wr_fun(void *arg, vframe_t *disp_vf)
{

	return early_NONE();
}

static int de_post_disable_fun(void *arg, vframe_t *disp_vf)
{

	return early_NONE();
}

static int do_nothing_fun(void *arg, vframe_t *disp_vf)
{
	return early_NONE();
}

static int do_pre_only_fun(void *arg, vframe_t *disp_vf)
{

	return early_NONE();
}

static void get_vscale_skip_count(unsigned int par)
{
	/*di_vscale_skip_count_real = (par >> 24) & 0xff;*/
	dimp_set(edi_mp_di_vscale_skip_real,
		 (par >> 24) & 0xff);
}

#define get_vpp_reg_update_flag(par) (((par) >> 16) & 0x1)

static unsigned int pldn_dly = 1;

/******************************************
 *
 ******************************************/

#ifdef DIM_OUT_NV21

static unsigned int cfg_nv21/* = DI_BIT0*/;
module_param_named(cfg_nv21, cfg_nv21, uint, 0664);

#ifdef NV21_DBG
static unsigned int cfg_vf;
module_param_named(cfg_vf, cfg_vf, uint, 0664);
#endif

unsigned int dim_cfg_nv21(void)
{
	if (cfg_nv21 & DI_BIT0)
		return 1; /* nv21 */
	else if (cfg_nv21 & DI_BIT5)
		return 2; /* nv12 */
	else
		return 0;

	return false;
}

bool dim_cfg_pre_nv21(unsigned int cmd)
{
	if (!cmd && (cfg_nv21 & DI_BIT8)) { /*nv21 */
		return true;
	} else if (cmd && (cfg_nv21 & DI_BIT9)) {
		return true;
	}

	return false;
}

/**********************************************************
 * canvans
 *	set vfm canvas by config | planes | index
 *	set vf->canvas0Addr
 *
 **********************************************************/
static void dim_canvas_set2(struct vframe_s *vf, u32 *index)
{
	int i;
	u32 *canvas_index = index;
	unsigned int shift;
	struct canvas_config_s *cfg = &vf->canvas0_config[0];
	u32 planes = vf->plane_num;

	if (vf->canvas0Addr != ((u32)-1))
		return;
	if (planes > 3) {
		PR_ERR("%s:planes overflow[%d]\n", __func__, planes);
		return;
	}
	dim_print("%s:p[%d]\n", __func__, planes);

	vf->canvas0Addr = 0;
	for (i = 0; i < planes; i++, canvas_index++, cfg++) {
		canvas_config_config(*canvas_index, cfg);
		#ifdef CVS_UINT
		dim_print("\tw[%d],h[%d],cid[%d],0x%x\n",
			  cfg->width, cfg->height, *canvas_index, cfg->phy_addr);
		#else
		dim_print("\tw[%d],h[%d],cid[%d],0x%lx\n",
			  cfg->width, cfg->height, *canvas_index, cfg->phy_addr);
		#endif
		shift = 8 * i;
		vf->canvas0Addr |= (*canvas_index << shift);
		//vf->plane_num = planes;
	}
}

static void di_cnt_cvs_nv21(unsigned int mode,
			    unsigned int *h,
			    unsigned int *v,
			    unsigned int ch)
{
	struct div2_mm_s *mm = dim_mm_get(ch); /*mm-0705*/
	int width = mm->cfg.di_w;
	int height = mm->cfg.di_h;
	int canvas_height = height + 8;
	unsigned int nr_width = width;
	unsigned int nr_canvas_width = width;
	unsigned int canvas_align_width = 32;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	nr_width = width;
	nr_canvas_width = nr_width;//nr_width << 1;
	nr_canvas_width = roundup(nr_canvas_width, canvas_align_width);
	*h = nr_canvas_width;
	*v = canvas_height;
}

/* @ary_note: use di_buf to fill vfm */
static void dimpst_fill_outvf(struct vframe_s *vfm,
			      struct di_buf_s *di_buf,
			      enum EDPST_OUT_MODE mode)
{
	struct canvas_config_s *cvsp, *cvsf;
	unsigned int cvsh, cvsv, csize;
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct di_buffer *buffer; //for ext buff
	unsigned int ch;
	struct di_ch_s *pch;
	bool ext_buf = false;

	//check ext buffer:
	ch = di_buf->channel;
	pch = get_chdata(ch);

	if (dip_itf_is_ins_exbuf(pch)) {
		ext_buf = true;
		if (!di_buf->c.buffer) {
			PR_ERR("%s:ext_buf,no buffer\n", __func__);
			return;
		}
		buffer = (struct di_buffer *)di_buf->c.buffer;
		if (!buffer->vf) {
			PR_ERR("%s:ext_buf,no vf\n", __func__);
			return;
		}
		dim_print("%s:buffer %px\n", __func__, buffer);
		cvsf = &buffer->vf->canvas0_config[0];
	}
//	unsigned int tmp;
	if (vfm != di_buf->vframe) //@ary_note
		memcpy(vfm, di_buf->vframe, sizeof(*vfm));

	/* canvas */
	vfm->canvas0Addr = (u32)-1;

	if (mode == EDPST_OUT_MODE_DEF) {
		vfm->plane_num = 1;
		cvsp = &vfm->canvas0_config[0];
		cvsp->phy_addr = di_buf->nr_adr;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
		cvsp->width = di_buf->canvas_width[NR_CANVAS];
		cvsp->height = di_buf->canvas_height;
	} else {
		vfm->plane_num = 2;
		/* count canvs size */
		di_cnt_cvs_nv21(0, &cvsh, &cvsv, di_buf->channel);
		/* 0 */
		cvsp = &vfm->canvas0_config[0];
		if (ext_buf) {
			cvsp->phy_addr	= cvsf->phy_addr;
			cvsp->width	= cvsf->width;
			cvsp->height	= cvsf->height;
		} else {
			cvsp->phy_addr = di_buf->nr_adr;
			cvsp->width = cvsh;
			cvsp->height = cvsv;
		}
		cvsp->block_mode = 0;
		cvsp->endian = 0;

		csize = roundup((cvsh * cvsv), PAGE_SIZE);
		/* 1 */
		cvsp = &vfm->canvas0_config[1];

		if (ext_buf) {
			cvsf = &buffer->vf->canvas0_config[1];
			cvsp->phy_addr = cvsf->phy_addr;
			cvsp->width	= cvsf->width;
			cvsp->height	= cvsf->height;
		} else {
			cvsp->phy_addr = di_buf->nr_adr + csize;
			cvsp->width = cvsh;
			cvsp->height = cvsv;
		}
		cvsp->block_mode = 0;
		cvsp->endian = 0;
	}
	#ifdef CVS_UINT
	dim_print("%s:%d:addr0[0x%x], 1[0x%x],w[%d,%d]\n",
		__func__,
	ext_buf, vfm->canvas0_config[0].phy_addr,
		vfm->canvas0_config[1].phy_addr,
		vfm->canvas0_config[0].width,
		vfm->canvas0_config[0].height);
	#else
	dim_print("%s:%d:addr0[0x%lx], 1[0x%lx],w[%d,%d]\n",
		__func__,
		ext_buf,
		vfm->canvas0_config[0].phy_addr,
		vfm->canvas0_config[1].phy_addr,
		vfm->canvas0_config[0].width,
		vfm->canvas0_config[0].height);
	#endif
	/* type */
	if (mode == EDPST_OUT_MODE_NV21 ||
	    mode == EDPST_OUT_MODE_NV12) {
		/*clear*/
		vfm->type &= ~(VIDTYPE_VIU_NV12	|
			       VIDTYPE_VIU_444	|
			       VIDTYPE_VIU_NV21	|
			       VIDTYPE_VIU_422	|
			       VIDTYPE_VIU_SINGLE_PLANE	|
			       VIDTYPE_COMPRESS	|
			       VIDTYPE_PRE_INTERLACE);
		vfm->type |= VIDTYPE_VIU_FIELD;
		vfm->type |= VIDTYPE_DI_PW;
		if (mode == EDPST_OUT_MODE_NV21)
			vfm->type |= VIDTYPE_VIU_NV21;
		else
			vfm->type |= VIDTYPE_VIU_NV12;

		/* bit */
		vfm->bitdepth &= ~(BITDEPTH_MASK);
		vfm->bitdepth &= ~(FULL_PACK_422_MODE);
		vfm->bitdepth |= (BITDEPTH_Y8	|
				  BITDEPTH_U8	|
				  BITDEPTH_V8);
	}

	if (de_devp->pps_enable &&
	    dimp_get(edi_mp_pps_position) == 0) {
		if (dimp_get(edi_mp_pps_dstw))
			vfm->width = dimp_get(edi_mp_pps_dstw);

		if (dimp_get(edi_mp_pps_dsth))
			vfm->height = dimp_get(edi_mp_pps_dsth);
	}

	if (di_buf->afbc_info & DI_BIT0)
		vfm->height	= vfm->height / 2;

	//dim_print("%s:h[%d]\n", __func__, vfm->height);
#ifdef NV21_DBG
	if (cfg_vf)
		vfm->type = cfg_vf;
#endif
}

#ifdef MARK_HIS
/* for exit buffer */
/* only change addr */
static void dimpst_fill_outvf_ext(struct vframe_s *vfm,
			      struct di_buf_s *di_buf,
			      enum EDPST_OUT_MODE mode)
{
	struct canvas_config_s *cvsp, *cvsf;
	unsigned int cvsh, cvsv, csize;
	struct di_dev_s *de_devp = get_dim_de_devp();
//	unsigned int tmp;
	struct di_buffer *buffer; //for ext buff

	if (!di_buf->c.buffer) {
		PR_ERR("%s:no buffer\n", __func__);
		return;
	}
	buffer = (struct di_buffer *)di_buf->c.buffer;
	if (!buffer->vf) {
		PR_ERR("%s:no buffer vf\n", __func__);
		return;
	}
	if (vfm != di_buf->vframe) //@ary_note
		memcpy(vfm, di_buf->vframe, sizeof(*vfm));

	/* canvas */
	vfm->canvas0Addr = (u32)-1;

	if (mode == EDPST_OUT_MODE_DEF) {
		vfm->plane_num = 1;
		cvsp = &vfm->canvas0_config[0];
		cvsf = &buffer->vf->canvas0_config[0];
		cvsp->phy_addr = cvsf->phy_addr;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
		cvsp->width = di_buf->canvas_width[NR_CANVAS];
		cvsp->height = di_buf->canvas_height;
	} else {
		vfm->plane_num = 2;
		/* count canvs size */
		di_cnt_cvs_nv21(0, &cvsh, &cvsv, di_buf->channel);
		/* 0 */
		cvsp = &vfm->canvas0_config[0];
		cvsf = &buffer->vf->canvas0_config[0];
		cvsp->phy_addr = cvsf->phy_addr;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
		cvsp->width = cvsh;
		cvsp->height = cvsv;
		csize = roundup((cvsh * cvsv), PAGE_SIZE);
		/* 1 */
		cvsp = &vfm->canvas0_config[1];
		cvsf = &buffer->vf->canvas0_config[1];
		cvsp->width = cvsh;
		cvsp->height = cvsv;
		cvsp->phy_addr = cvsf->phy_addr;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
	}

	/* type */
	if (mode == EDPST_OUT_MODE_NV21 ||
	    mode == EDPST_OUT_MODE_NV12) {
		/*clear*/
		vfm->type &= ~(VIDTYPE_VIU_NV12	|
			       VIDTYPE_VIU_444	|
			       VIDTYPE_VIU_NV21	|
			       VIDTYPE_VIU_422	|
			       VIDTYPE_VIU_SINGLE_PLANE	|
			       VIDTYPE_COMPRESS	|
			       VIDTYPE_PRE_INTERLACE);
		vfm->type |= VIDTYPE_VIU_FIELD;
		vfm->type |= VIDTYPE_DI_PW;
		if (mode == EDPST_OUT_MODE_NV21)
			vfm->type |= VIDTYPE_VIU_NV21;
		else
			vfm->type |= VIDTYPE_VIU_NV12;

		/* bit */
		vfm->bitdepth &= ~(BITDEPTH_MASK);
		vfm->bitdepth &= ~(FULL_PACK_422_MODE);
		vfm->bitdepth |= (BITDEPTH_Y8	|
				  BITDEPTH_U8	|
				  BITDEPTH_V8);
	}

	if (de_devp->pps_enable &&
	    dimp_get(edi_mp_pps_position) == 0) {
		if (dimp_get(edi_mp_pps_dstw))
			vfm->width = dimp_get(edi_mp_pps_dstw);

		if (dimp_get(edi_mp_pps_dsth))
			vfm->height = dimp_get(edi_mp_pps_dsth);
	}

	if (di_buf->afbc_info & DI_BIT0)
		vfm->height	= vfm->height / 2;

	dim_print("%s:h[%d]\n", __func__, vfm->height);
#ifdef NV21_DBG
	if (cfg_vf)
		vfm->type = cfg_vf;
#endif
}
#endif

//#if 1/* move to di_hw_v2.c */
static void dim_cfg_s_mif(struct DI_SIM_MIF_s *smif,
			  struct vframe_s *vf,
			  struct di_win_s *win)
{
	struct di_dev_s *de_devp = get_dim_de_devp();

	//vframe_t *vf = di_buf->vframe;

	//smif->canvas_num = di_buf->nr_canvas_idx;
	/* bit mode config */
	if (vf->bitdepth & BITDEPTH_Y10) {
		if (vf->type & VIDTYPE_VIU_444) {
			smif->bit_mode = (vf->bitdepth & FULL_PACK_422_MODE) ?
						3 : 2;
		} else {
			smif->bit_mode = (vf->bitdepth & FULL_PACK_422_MODE) ?
						3 : 1;
		}
	} else {
		smif->bit_mode = 0;
	}

	/* video mode */
	if (vf->type & VIDTYPE_VIU_444)
		smif->video_mode = 1;
	else
		smif->video_mode = 0;

	/* separate */
	if (vf->type & VIDTYPE_VIU_422)
		smif->set_separate_en = 0;
	else
		smif->set_separate_en = 2; /*nv12 ? nv 21?*/

	/*x,y,*/

	if (de_devp->pps_enable &&
	    dimp_get(edi_mp_pps_position) == 0) {
		//dim_pps_config(0, di_width, di_height,
		//	       dimp_get(edi_mp_pps_dstw),
		//	       dimp_get(edi_mp_pps_dsth));
		if (win) {
			smif->start_x = win->x_st;
			smif->end_x = win->x_st +
				dimp_get(edi_mp_pps_dstw) - 1;
			smif->start_y = win->y_st;
			smif->end_y = win->y_st +
				dimp_get(edi_mp_pps_dsth) - 1;
		} else {
			smif->start_x = 0;
			smif->end_x =
				dimp_get(edi_mp_pps_dstw) - 1;
			smif->start_y = 0;
			smif->end_y =
				dimp_get(edi_mp_pps_dsth) - 1;
		}
	} else {
		if (win) {
			smif->start_x = win->x_st;
			smif->end_x   = win->x_st + win->x_size - 1;
			smif->start_y = win->y_st;
			smif->end_y   = win->y_st + win->y_size - 1;

		} else {
			smif->start_x = 0;
			smif->end_x   = vf->width - 1;
			smif->start_y = 0;
			smif->end_y   = vf->height - 1;
		}
	}
}

//#endif

void dbg_vfm(struct vframe_s *vf, unsigned int dbgpos)
{
	int i;
	struct canvas_config_s *cvsp;

	if (!(cfg_nv21 & DI_BIT1))
		return;
	PR_INF("%d:type=0x%x\n", dbgpos, vf->type);
	PR_INF("plane_num=0x%x\n", vf->plane_num);
	PR_INF("0Addr=0x%x\n", vf->canvas0Addr);
	PR_INF("1Addr=0x%x\n", vf->canvas1Addr);
	PR_INF("plane_num=0x%x\n", vf->plane_num);
	for (i = 0; i < vf->plane_num; i++) {
		PR_INF("%d:\n", i);
		cvsp = &vf->canvas0_config[i];
	#ifdef CVS_UINT
		PR_INF("\tph=0x%x\n", cvsp->phy_addr);
	#else
		PR_INF("\tph=0x%lx\n", cvsp->phy_addr);
	#endif
		PR_INF("\tw=%d\n", cvsp->width);
		PR_INF("\th=%d\n", cvsp->height);
		PR_INF("\tb=%d\n", cvsp->block_mode);
		PR_INF("\tendian=%d\n", cvsp->endian);
	}
}
#endif /* DIM_OUT_NV21 */

int dim_post_process(void *arg, unsigned int zoom_start_x_lines,
		     unsigned int zoom_end_x_lines,
		     unsigned int zoom_start_y_lines,
		     unsigned int zoom_end_y_lines,
		     vframe_t *disp_vf)
{
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;
	struct di_buf_s *di_pldn_buf = NULL;
	unsigned int di_width, di_height, di_start_x, di_end_x, mv_offset;
	unsigned int di_start_y, di_end_y, hold_line;
	unsigned int post_blend_en = 0, post_blend_mode = 0,
		     blend_mtn_en = 0, ei_en = 0, post_field_num = 0;
	int di_vpp_en, di_ddr_en;
	int tmptest;
	unsigned char mc_pre_flag = 0;
	bool invert_mv = false;
	static int post_index = -1;
	unsigned char tmp_idx = 0;
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct di_hpst_s  *pst = get_hw_pst();

	unsigned char channel = pst->curr_ch;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_cvs_s *cvss;
	struct pst_cfg_afbc_s *acfg;
	union afbc_blk_s	*en_set_pst;
#ifdef DIM_OUT_NV21
	u32	cvs_nv21[2];
#endif
	union hw_sc2_ctr_pst_s *sc2_post_cfg;
	union hw_sc2_ctr_pst_s *sc2_post_cfg_set;
	unsigned int tmp;
	unsigned int dbg_r;

	struct di_ch_s *pch;

	dimp_inc(edi_mp_post_cnt);
	if (ppost->vscale_skip_flag)
		return 0;

	dim_mp_update_post();
	get_vscale_skip_count(zoom_start_x_lines);

	if (IS_ERR_OR_NULL(di_buf))
		return 0;
	else if (IS_ERR_OR_NULL(di_buf->di_buf_dup_p[0]))
		return 0;

	pch = get_chdata(channel);
	cvss = &get_datal()->cvs;
	acfg = &ppost->afbc_cfg;
	hold_line = dimp_get(edi_mp_post_hold_line);
	di_pldn_buf = di_buf->di_buf_dup_p[pldn_dly];

	if (di_que_is_in_que(channel, QUE_POST_FREE, di_buf) &&
	    post_index != di_buf->index) {
		post_index = di_buf->index;
		PR_ERR("%s:post_buf[%d] is in post free list.\n",
		       __func__, di_buf->index);
		return 0;
	}
	hpst_dbg_mem_pd_trig(0);

	if (ppost->toggle_flag && di_buf->di_buf_dup_p[1])
		top_bot_config(di_buf->di_buf_dup_p[1]);

	ppost->toggle_flag = false;

	ppost->cur_disp_index = di_buf->index;

	if (get_vpp_reg_update_flag(zoom_start_x_lines)	||
	    dimp_get(edi_mp_post_refresh))
		ppost->update_post_reg_flag = 1;

	zoom_start_x_lines = zoom_start_x_lines & 0xffff;
	zoom_end_x_lines = zoom_end_x_lines & 0xffff;
	zoom_start_y_lines = zoom_start_y_lines & 0xffff;
	zoom_end_y_lines = zoom_end_y_lines & 0xffff;

	if (!get_init_flag(channel) && IS_ERR_OR_NULL(ppost->keep_buf)) {
		PR_ERR("%s 2:\n", __func__);
		return 0;
	}
	if (di_buf->vframe)
		dim_tr_ops.post_set(di_buf->vframe->index_disp);
	else
		return 0;
	/*dbg*/
	if (ppost->frame_cnt == 1 && kpi_frame_num > 0) {
		pr_dbg("[di_kpi] di:ch[%d]:queue 1st frame to post ready.\n",
		       channel);
	}

	dim_secure_sw_post(channel);
	if (dim_hdr_ops() && di_buf->c.en_hdr)
		dim_hdr_ops()->set(0);

	dim_ddbg_mod_save(EDI_DBG_MOD_POST_SETB, channel, ppost->frame_cnt);
	dbg_post_cnt(channel, "ps1");
	di_start_x = zoom_start_x_lines;
	di_end_x = zoom_end_x_lines;
	di_width = di_end_x - di_start_x + 1;
	di_start_y = zoom_start_y_lines;
	di_end_y = zoom_end_y_lines;
	di_height = di_end_y - di_start_y + 1;
	di_height = di_height / (dimp_get(edi_mp_di_vscale_skip_real) + 1);
	/* make sure the height is even number */
	if (di_height % 2) {
		/*for skip mode,post only half line-1*/
		if (!dimp_get(edi_mp_post_wr_en)	&&
		    (di_height > 150			&&
		    di_height < 1080)			&&
		    dimp_get(edi_mp_di_vscale_skip_real))
			di_height = di_height - 3;
		else
			di_height++;
	}
	if (dip_itf_is_ins(pch) && dim_dbg_new_int(2))
		dim_dbg_buffer2(di_buf->c.buffer, 7);

#ifdef DIM_OUT_NV21
	/* nv 21*/
	if (is_mask(SC2_DW_EN))
		dw_fill_outvf(&pst->vf_post, di_buf);
	//else if (cfg_nv21 & DI_BIT0)
	else if (cfggch(pch, IOUT_FMT) == 1)
		dimpst_fill_outvf(&pst->vf_post, di_buf, EDPST_OUT_MODE_NV21);
	else if (cfggch(pch, IOUT_FMT) == 2)
		dimpst_fill_outvf(&pst->vf_post, di_buf, EDPST_OUT_MODE_NV12);
	else
		dimpst_fill_outvf(&pst->vf_post, di_buf, EDPST_OUT_MODE_DEF);
	/*************************************************/
#endif
	if (is_mask(SC2_POST_TRIG)) {
		pst->last_pst_size  = 0;
		//PR_INF("%s:trig\n", __func__);
	}

	if (di_buf->trig_post_update) {
		pst->last_pst_size = 0;
		//PR_INF("post trig\n");
		ppost->seq = 0;
	}
	di_buf->seq = ppost->seq;
	if (/*RD(DI_POST_SIZE)*/pst->last_pst_size != ((di_width - 1) |
	    ((di_height - 1) << 16)) ||
	    ppost->buf_type != di_buf->di_buf_dup_p[0]->type ||
	    ppost->di_buf0_mif.luma_x_start0 != di_start_x ||
	    (ppost->di_buf0_mif.luma_y_start0 != di_start_y / 2)) {
		dim_ddbg_mod_save(EDI_DBG_MOD_POST_RESIZE, channel,
				  ppost->frame_cnt);/*dbg*/
		ppost->buf_type = di_buf->di_buf_dup_p[0]->type;

		dimh_initial_di_post_2(di_width, di_height,
				       hold_line,
			(dimp_get(edi_mp_post_wr_en) &&
			dimp_get(edi_mp_post_wr_support)));

		if (!di_buf->di_buf_dup_p[0]->vframe) {
			PR_ERR("%s 3:\n", __func__);
			return 0;
		}
		sc2_post_cfg = &ppost->pst_top_cfg;//&pst->pst_top_cfg;
		if (DIM_IS_IC_EF(SC2) && dim_afds() && dim_afds()->pst_check)
			dim_afds()->pst_check(&pst->vf_post, pch);
		if (DIM_IS_IC_EF(SC2)) {
			if (sc2_post_cfg->b.afbc_wr)
				sc2_post_cfg->b.mif_en	= 0;
			else
				sc2_post_cfg->b.mif_en	= 1;

			if (dimp_get(edi_mp_post_wr_en) &&
			    dimp_get(edi_mp_post_wr_support))
				sc2_post_cfg->b.post_frm_sel = 1;
			else
				sc2_post_cfg->b.post_frm_sel = 0;
			//dim_sc2_contr_pst(sc2_post_cfg);
			if (cfgg(LINEAR)) {
				ppost->di_diwr_mif.linear = 1;
				ppost->di_diwr_mif.buf_crop_en = 1;
				ppost->di_diwr_mif.buf_hsize = di_buf->buf_hsize;//1920; tmp;
				ppost->di_buf0_mif.linear = 1;
				ppost->di_buf0_mif.buf_crop_en = 1;
				ppost->di_buf0_mif.buf_hsize =
					di_buf->di_buf_dup_p[0]->buf_hsize;
				ppost->di_buf1_mif.linear = 1;
				ppost->di_buf1_mif.buf_crop_en = 1;
				ppost->di_buf1_mif.buf_hsize =
					di_buf->di_buf_dup_p[0]->buf_hsize;
				ppost->di_buf2_mif.linear = 1;
				ppost->di_buf2_mif.buf_crop_en = 1;
				ppost->di_buf2_mif.buf_hsize =
					di_buf->di_buf_dup_p[0]->buf_hsize;

				ppost->di_mtnprd_mif.linear = 1;
				ppost->di_mcvecrd_mif.linear = 1;
			}
		}
		pst->last_pst_size = ((di_width - 1) | ((di_height - 1) << 16));
#ifdef DIM_OUT_NV21
		/* nv 21*/
		if (di_buf->flg_nv21 == 2 || cfggch(pch, IOUT_FMT) == 2)
			ppost->di_diwr_mif.cbcr_swap = 1;
		else
			ppost->di_diwr_mif.cbcr_swap = 0;

		if (dip_itf_is_o_linear(pch) &&
		    (cfggch(pch, IOUT_FMT) == 2 ||
		     cfggch(pch, IOUT_FMT) == 1)) {
			ppost->di_diwr_mif.reg_swap = 0;
			ppost->di_diwr_mif.l_endian = 1;
		} else {
			ppost->di_diwr_mif.reg_swap = 1;
			ppost->di_diwr_mif.l_endian = 0;
		}
		dbg_r = dim_get_dbg_dec21();
		if (dbg_r & 0xf0) {
			ppost->di_diwr_mif.reg_swap = bget(&dbg_r, 4);
			ppost->di_diwr_mif.l_endian = bget(&dbg_r, 5);
			ppost->di_diwr_mif.cbcr_swap = bget(&dbg_r, 6);
		}
		dim_print("%s1:reg_swap[%d],%d,%d\n", __func__,
			ppost->di_diwr_mif.reg_swap,
			dip_itf_is_o_linear(pch),
			di_buf->flg_nv21);
		if (DIM_IS_IC_EF(SC2))
			opl1()->wr_cfg_mif(&ppost->di_diwr_mif,
					   EDI_MIFSM_WR,
					   &pst->vf_post, NULL);
		else
			dim_cfg_s_mif(&ppost->di_diwr_mif, &pst->vf_post, NULL);
#endif
		/******************************************/
		/* bit mode config */
		if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
			if (di_buf->vframe->type & VIDTYPE_VIU_444) {
				ppost->di_buf0_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 2;
				ppost->di_buf1_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 2;
				ppost->di_buf2_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 2;

			} else {
				ppost->di_buf0_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 1;
				ppost->di_buf1_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 1;
				ppost->di_buf2_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 1;

			}
		} else {
			ppost->di_buf0_mif.bit_mode = 0;
			ppost->di_buf1_mif.bit_mode = 0;
			ppost->di_buf2_mif.bit_mode = 0;
		}
		if (di_buf->vframe->type & VIDTYPE_VIU_444) {
			if (DIM_IS_IC_EF(SC2)) {
				ppost->di_buf0_mif.video_mode = 2;
				ppost->di_buf1_mif.video_mode = 2;
				ppost->di_buf2_mif.video_mode = 2;
			} else {
				ppost->di_buf0_mif.video_mode = 1;
				ppost->di_buf1_mif.video_mode = 1;
				ppost->di_buf2_mif.video_mode = 1;
			}
		} else if (di_buf->vframe->type & VIDTYPE_VIU_422) {
			if (DIM_IS_IC_EF(SC2)) {
				if (opl1()->info.main_version >= 3) {
					ppost->di_buf0_mif.video_mode = 1;
					ppost->di_buf1_mif.video_mode = 1;
					ppost->di_buf2_mif.video_mode = 1;
				}
			} else {
				ppost->di_buf0_mif.video_mode = 0;
				ppost->di_buf1_mif.video_mode = 0;
				ppost->di_buf2_mif.video_mode = 0;
			}

		} else {
			ppost->di_buf0_mif.video_mode = 0;
			ppost->di_buf1_mif.video_mode = 0;
			ppost->di_buf2_mif.video_mode = 0;
		}
		if (ppost->buf_type == VFRAME_TYPE_IN &&
		    !(di_buf->di_buf_dup_p[0]->vframe->type &
		      VIDTYPE_VIU_FIELD)) {
			if ((di_buf->vframe->type & VIDTYPE_VIU_NV21) ||
			    (di_buf->vframe->type & VIDTYPE_VIU_NV12)) {
				ppost->di_buf0_mif.set_separate_en = 1;
				ppost->di_buf1_mif.set_separate_en = 1;
				ppost->di_buf2_mif.set_separate_en = 1;
			} else {
				ppost->di_buf0_mif.set_separate_en = 0;
				ppost->di_buf1_mif.set_separate_en = 0;
				ppost->di_buf2_mif.set_separate_en = 0;
			}
			ppost->di_buf0_mif.luma_y_start0 = di_start_y;
			ppost->di_buf0_mif.luma_y_end0 = di_end_y;
		} else { /* from vdin or local vframe process by di pre */
			ppost->di_buf0_mif.set_separate_en = 0;
			ppost->di_buf0_mif.luma_y_start0 =
				di_start_y >> 1;
			ppost->di_buf0_mif.luma_y_end0 = di_end_y >> 1;
			ppost->di_buf1_mif.set_separate_en = 0;
			ppost->di_buf1_mif.luma_y_start0 =
				di_start_y >> 1;
			ppost->di_buf1_mif.luma_y_end0 = di_end_y >> 1;
			ppost->di_buf2_mif.set_separate_en = 0;
			ppost->di_buf2_mif.luma_y_end0 = di_end_y >> 1;
			ppost->di_buf2_mif.luma_y_start0 =
				di_start_y >> 1;
		}
		ppost->di_buf0_mif.luma_x_start0 = di_start_x;
		ppost->di_buf0_mif.luma_x_end0 = di_end_x;
		ppost->di_buf1_mif.luma_x_start0 = di_start_x;
		ppost->di_buf1_mif.luma_x_end0 = di_end_x;
		ppost->di_buf2_mif.luma_x_start0 = di_start_x;
		ppost->di_buf2_mif.luma_x_end0 = di_end_x;

		ppost->di_buf0_mif.reg_swap	= 1;
		ppost->di_buf0_mif.l_endian	= 0;
		ppost->di_buf0_mif.cbcr_swap	= 0;

		ppost->di_buf1_mif.reg_swap	= 1;
		ppost->di_buf1_mif.l_endian	= 0;
		ppost->di_buf1_mif.cbcr_swap	= 0;
		ppost->di_buf2_mif.reg_swap	= 1;
		ppost->di_buf2_mif.l_endian	= 0;
		ppost->di_buf2_mif.cbcr_swap	= 0;
		if (dimp_get(edi_mp_post_wr_en) &&
		    dimp_get(edi_mp_post_wr_support)) {
			if (de_devp->pps_enable &&
			    dimp_get(edi_mp_pps_position) == 0) {
				dim_pps_config(0, di_width, di_height,
					       dimp_get(edi_mp_pps_dstw),
					       dimp_get(edi_mp_pps_dsth));
			}
		}

		ppost->di_mtnprd_mif.start_x = di_start_x;
		ppost->di_mtnprd_mif.end_x = di_end_x;
		ppost->di_mtnprd_mif.start_y = di_start_y >> 1;
		ppost->di_mtnprd_mif.end_y = di_end_y >> 1;
		ppost->di_mtnprd_mif.per_bits	= 4;
		if (dimp_get(edi_mp_mcpre_en)) {
			ppost->di_mcvecrd_mif.start_x = di_start_x / 5;
			mv_offset = (di_start_x % 5) ? (5 - di_start_x % 5) : 0;
			ppost->di_mcvecrd_mif.vecrd_offset =
				overturn ? (di_end_x + 1) % 5 : mv_offset;
			ppost->di_mcvecrd_mif.start_y =
				(di_start_y >> 1);
			ppost->di_mcvecrd_mif.size_x =
				(di_end_x + 1 + 4) / 5 - 1 - di_start_x / 5;
			ppost->di_mcvecrd_mif.end_y =
				(di_end_y >> 1);
			ppost->di_mcvecrd_mif.per_bits = 16;
		}
		ppost->update_post_reg_flag = 1;
		/* if height decrease, mtn will not enough */
		if (di_buf->pd_config.global_mode != PULL_DOWN_BUF1 &&
		    !dimp_get(edi_mp_post_wr_en))
			di_buf->pd_config.global_mode = PULL_DOWN_EI;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (is_vsync_rdma_enable()) {
#ifdef DIM_OUT_NV21
		if (dimp_get(edi_mp_post_wr_en) &&
		    dimp_get(edi_mp_post_wr_support))
			ppost->canvas_id = 0;
		else
			ppost->canvas_id = ppost->next_canvas_id;

#endif
	} else {
		ppost->canvas_id = 0;
		ppost->next_canvas_id = 1;
		if (dimp_get(edi_mp_post_wr_en) &&
		    dimp_get(edi_mp_post_wr_support))
			ppost->canvas_id =
				ppost->next_canvas_id;
	}
#endif
	/*post_blend = di_buf->pd_config.global_mode;*/
	dimp_set(edi_mp_post_blend, di_buf->pd_config.global_mode);
	dim_print("%s:ch[%d]\n", __func__, channel);
	switch (dimp_get(edi_mp_post_blend)) {
	case PULL_DOWN_BLEND_0:
	case PULL_DOWN_NORMAL:
		config_canvas_idx(di_buf->di_buf_dup_p[1],
				  cvss->post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2], -1,
				  cvss->post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[0],
				  cvss->post_idx[ppost->canvas_id][1], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2],
				  cvss->post_idx[ppost->canvas_id][3], -1);
		if (dimp_get(edi_mp_mcpre_en)) {
			tmptest = cvss->post_idx[ppost->canvas_id][4];
			config_mcvec_canvas_idx(di_buf->di_buf_dup_p[2],
						tmptest);
		}
		break;
	case PULL_DOWN_BLEND_2:
	case PULL_DOWN_NORMAL_2:
		config_canvas_idx(di_buf->di_buf_dup_p[0],
				  cvss->post_idx[ppost->canvas_id][3], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[1],
				  cvss->post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2], -1,
				  cvss->post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[2],
				  cvss->post_idx[ppost->canvas_id][1], -1);
		if (dimp_get(edi_mp_mcpre_en)) {
			tmptest = cvss->post_idx[ppost->canvas_id][4];
			config_mcvec_canvas_idx(di_buf->di_buf_dup_p[2],
						tmptest);
		}
		break;
	case PULL_DOWN_MTN:
		config_canvas_idx(di_buf->di_buf_dup_p[1],
				  cvss->post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2], -1,
				  cvss->post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[0],
				  cvss->post_idx[ppost->canvas_id][1], -1);
		break;
	case PULL_DOWN_BUF1:/* wave with buf1 */
		config_canvas_idx(di_buf->di_buf_dup_p[1],
				  cvss->post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[1], -1,
				  cvss->post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[0],
				  cvss->post_idx[ppost->canvas_id][1], -1);
		break;
	case PULL_DOWN_EI:
		if (di_buf->di_buf_dup_p[1])
			config_canvas_idx(di_buf->di_buf_dup_p[1],
					  cvss->post_idx[ppost->canvas_id][0],
					  -1);
		break;
	default:
		break;
	}
	ppost->next_canvas_id = ppost->canvas_id ? 0 : 1;

	if (!di_buf->di_buf_dup_p[1]) {
		PR_ERR("%s 4:\n", __func__);
		return 0;
	}
	if (!di_buf->di_buf_dup_p[1]->vframe ||
	    !di_buf->di_buf_dup_p[0]->vframe) {
		PR_ERR("%s 5:\n", __func__);
		return 0;
	}

	if (is_meson_txl_cpu() && overturn && di_buf->di_buf_dup_p[2]) {
		/*sync from kernel 3.14 txl*/
		if (dimp_get(edi_mp_post_blend) == PULL_DOWN_BLEND_2)
			dimp_set(edi_mp_post_blend, PULL_DOWN_BLEND_0);
		else if (dimp_get(edi_mp_post_blend) == PULL_DOWN_BLEND_0)
			dimp_set(edi_mp_post_blend, PULL_DOWN_BLEND_2);
	}

	switch (dimp_get(edi_mp_post_blend)) {
	case PULL_DOWN_BLEND_0:
	case PULL_DOWN_NORMAL:
		post_field_num =
		(di_buf->di_buf_dup_p[1]->vframe->type &
		 VIDTYPE_TYPEMASK)
		== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		ppost->di_buf2_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		//t7
		ppost->di_buf0_mif.addr0 = di_buf->di_buf_dup_p[1]->nr_adr;
		ppost->di_buf1_mif.addr0 =
			di_buf->di_buf_dup_p[0]->nr_adr;
		ppost->di_buf2_mif.addr0 =
			di_buf->di_buf_dup_p[2]->nr_adr;

		/* afbc dec */
		acfg->buf_mif[0] = di_buf->di_buf_dup_p[1];
		acfg->buf_mif[1] = di_buf->di_buf_dup_p[0];
		acfg->buf_mif[2] = di_buf->di_buf_dup_p[2];
		/************/
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		ppost->di_mtnprd_mif.addr = di_buf->di_buf_dup_p[2]->mtn_adr;
		/*mc_pre_flag = is_meson_txl_cpu()?2:(overturn?0:1);*/
		if (is_meson_txl_cpu() && overturn) {
			/* swap if1&if2 mean negation of mv for normal di*/
			tmp_idx = ppost->di_buf1_mif.canvas0_addr0;
			ppost->di_buf1_mif.canvas0_addr0 =
				ppost->di_buf2_mif.canvas0_addr0;
			ppost->di_buf2_mif.canvas0_addr0 = tmp_idx;
			/* afbc dec */
			acfg->buf_mif[1] = di_buf->di_buf_dup_p[2];
			acfg->buf_mif[2] = di_buf->di_buf_dup_p[0];
		}
		mc_pre_flag = overturn ? 0 : 1;
		if (di_buf->pd_config.global_mode == PULL_DOWN_NORMAL) {
			post_blend_mode = 3;
			/*if pulldown, mcdi_mcpreflag is 1,*/
			/*it means use previous field for MC*/
			/*else not pulldown,mcdi_mcpreflag is 2*/
			/*it means use forward & previous field for MC*/
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 2;
		} else {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 1;
			post_blend_mode = 1;
		}
		if (is_meson_txl_cpu() && overturn)
			mc_pre_flag = 1;

		if (dimp_get(edi_mp_mcpre_en)) {
			ppost->di_mcvecrd_mif.canvas_num =
				di_buf->di_buf_dup_p[2]->mcvec_canvas_idx;
			ppost->di_mcvecrd_mif.addr =
				di_buf->di_buf_dup_p[2]->mcvec_adr;
		}
		blend_mtn_en = 1;
		ei_en = 1;
		dimp_set(edi_mp_post_ei, 1);
		post_blend_en = 1;
		break;
	case PULL_DOWN_BLEND_2:
	case PULL_DOWN_NORMAL_2:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[2]->nr_canvas_idx;
		ppost->di_buf2_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;

		ppost->di_buf0_mif.addr0 =
			di_buf->di_buf_dup_p[1]->nr_adr;
		ppost->di_buf1_mif.addr0 =
			di_buf->di_buf_dup_p[2]->nr_adr;
		ppost->di_buf2_mif.addr0 =
			di_buf->di_buf_dup_p[0]->nr_adr;

		/* afbc dec */
		acfg->buf_mif[0] = di_buf->di_buf_dup_p[1];
		acfg->buf_mif[1] = di_buf->di_buf_dup_p[2];
		acfg->buf_mif[2] = di_buf->di_buf_dup_p[0];
		/************/
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		ppost->di_mtnprd_mif.addr =
			di_buf->di_buf_dup_p[2]->mtn_adr;
		if (is_meson_txl_cpu() && overturn) {
			ppost->di_buf1_mif.canvas0_addr0 =
			ppost->di_buf2_mif.canvas0_addr0;
			/* afbc dec */
			acfg->buf_mif[1] = di_buf->di_buf_dup_p[0];
		}
		if (dimp_get(edi_mp_mcpre_en)) {
			ppost->di_mcvecrd_mif.canvas_num =
				di_buf->di_buf_dup_p[2]->mcvec_canvas_idx;
			ppost->di_mcvecrd_mif.addr =
				di_buf->di_buf_dup_p[2]->mcvec_adr;
			mc_pre_flag = is_meson_txl_cpu() ? 0 :
				(overturn ? 1 : 0);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
				invert_mv = true;
			} else if (!overturn) {
				ppost->di_buf2_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[2]->nr_canvas_idx;
				/* afbc dec */
				acfg->buf_mif[2] = di_buf->di_buf_dup_p[2];
			}
		}
		if (di_buf->pd_config.global_mode == PULL_DOWN_NORMAL_2) {
			post_blend_mode = 3;
			/*if pulldown, mcdi_mcpreflag is 1,*/
			/*it means use previous field for MC*/
			/*else not pulldown,mcdi_mcpreflag is 2*/
			/*it means use forward & previous field for MC*/
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 2;
		} else {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXHD))
				mc_pre_flag = 1;
			post_blend_mode = 1;
		}
		blend_mtn_en = 1;
		ei_en = 1;
		dimp_set(edi_mp_post_ei, 1);
		post_blend_en = 1;
		break;
	case PULL_DOWN_MTN:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		//t7
		ppost->di_buf0_mif.addr0 =
			di_buf->di_buf_dup_p[1]->nr_adr;
		ppost->di_buf1_mif.addr0 =
			di_buf->di_buf_dup_p[0]->nr_adr;
		/* afbc dec */
		acfg->buf_mif[0] = di_buf->di_buf_dup_p[1];
		acfg->buf_mif[1] = di_buf->di_buf_dup_p[0];
		acfg->buf_mif[2] = NULL;
		/************/
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		ppost->di_mtnprd_mif.addr =
			di_buf->di_buf_dup_p[2]->mtn_adr;
		post_blend_mode = 0;
		blend_mtn_en = 1;
		ei_en = 1;
		dimp_set(edi_mp_post_ei, 1);
		post_blend_en = 1;
		break;
	case PULL_DOWN_BUF1:
		post_field_num =
			(di_buf->di_buf_dup_p[1]->vframe->type &
			 VIDTYPE_TYPEMASK)
			== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		ppost->di_buf0_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[1]->nr_canvas_idx;
		//t7:
		ppost->di_buf0_mif.addr0 =
			di_buf->di_buf_dup_p[1]->nr_adr;
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[1]->mtn_canvas_idx;
		ppost->di_mtnprd_mif.addr =
			di_buf->di_buf_dup_p[1]->mtn_adr;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
		ppost->di_buf1_mif.addr0 =
			di_buf->di_buf_dup_p[0]->nr_adr;
		/* afbc dec */
		acfg->buf_mif[0] = di_buf->di_buf_dup_p[1];
		acfg->buf_mif[1] = di_buf->di_buf_dup_p[0];
		acfg->buf_mif[2] = NULL;
		/************/
		post_blend_mode = 1;
		blend_mtn_en = 0;
		ei_en = 0;
		dimp_set(edi_mp_post_ei, 0);
		post_blend_en = 0;
		break;
	case PULL_DOWN_EI:
		if (di_buf->di_buf_dup_p[1]) {
			ppost->di_buf0_mif.canvas0_addr0 =
				di_buf->di_buf_dup_p[1]->nr_canvas_idx;
			ppost->di_buf0_mif.addr0 =
				di_buf->di_buf_dup_p[1]->nr_adr;
			/* afbc dec */
			acfg->buf_mif[0] = di_buf->di_buf_dup_p[1];
			acfg->buf_mif[1] = NULL;
			acfg->buf_mif[2] = NULL;
		/************/
			post_field_num =
				(di_buf->di_buf_dup_p[1]->vframe->type &
				 VIDTYPE_TYPEMASK)
				== VIDTYPE_INTERLACE_TOP ? 0 : 1;
		} else {
			post_field_num =
				(di_buf->di_buf_dup_p[0]->vframe->type &
				 VIDTYPE_TYPEMASK)
				== VIDTYPE_INTERLACE_TOP ? 0 : 1;
			ppost->di_buf0_mif.src_field_mode =
				post_field_num;
		}
		post_blend_mode = 2;
		blend_mtn_en = 0;
		ei_en = 1;
		dimp_set(edi_mp_post_ei, 1);
		post_blend_en = 0;
		break;
	default:
		break;
	}
	ppost->di_diwr_mif.is_dw = 0;

	if (dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support)) {
		#ifdef DIM_OUT_NV21
		cvs_nv21[0] = cvss->post_idx[0][5];
		cvs_nv21[1] = cvss->post_idx[1][0];
		dim_canvas_set2(&pst->vf_post, &cvs_nv21[0]);
		ppost->di_diwr_mif.canvas_num = pst->vf_post.canvas0Addr;
		ppost->di_diwr_mif.addr	= pst->vf_post.canvas0_config[0].phy_addr;
		ppost->di_diwr_mif.addr1 = pst->vf_post.canvas0_config[1].phy_addr;

		ppost->di_diwr_mif.ddr_en = 1;

		#endif

		di_vpp_en = 0;
		di_ddr_en = 1;
	} else {
		di_vpp_en = 1;
		di_ddr_en = 0;
		#ifdef DIM_OUT_NV21
		ppost->di_diwr_mif.ddr_en = 0;
		#endif
	}
	/* hf */

	if (di_buf->en_hf	&&
	    di_buf->hf_adr	&&
	    di_hf_size_check(&ppost->di_diwr_mif) &&
	    di_hf_hw_try_alloc(2)	&&
	    opl1()->aisr_pre) {
		//di_buf->hf.height = ppost->di_diwr_mif.end_y + 1;
		//di_buf->hf.width = ppost->di_diwr_mif.end_x + 1;
		ppost->hf_mif.addr = di_buf->hf_adr;
		ppost->hf_mif.start_x = 0;
		ppost->hf_mif.start_y = 0;
		ppost->hf_mif.end_x = ppost->di_diwr_mif.end_x;
		ppost->hf_mif.end_y = ppost->di_diwr_mif.end_y;
		di_hf_buf_size_set(&ppost->hf_mif);
		di_buf->hf.height = ppost->di_diwr_mif.end_y + 1;
		di_buf->hf.width = ppost->di_diwr_mif.end_x + 1;
		di_buf->hf.buffer_w = ppost->hf_mif.buf_hsize;
		di_buf->hf.buffer_h = (unsigned int)ppost->hf_mif.addr2;
		if (is_di_hf_y_reverse())
			di_buf->hf.revert_mode = true;
		else
			di_buf->hf.revert_mode = false;
		//di_hf_lock_blend_buffer_pst(di_buf);
		opl1()->aisr_pre(&ppost->hf_mif, 1, di_buf->hf.revert_mode);
		di_buf->hf_done = 1;
	}
	dbg_ic("hf:en:pst:%d;addr:0x%lx;done:%d\n",
	di_buf->en_hf,
	di_buf->hf_adr,
	di_buf->hf_done);
	/**************************************************/
	/* if post size < MIN_POST_WIDTH, force ei */
	if (di_width < MIN_BLEND_WIDTH &&
	    (di_buf->pd_config.global_mode == PULL_DOWN_BLEND_0	||
	    di_buf->pd_config.global_mode == PULL_DOWN_BLEND_2	||
	    di_buf->pd_config.global_mode == PULL_DOWN_NORMAL
	    )) {
		post_blend_mode = 1;
		blend_mtn_en = 0;
		ei_en = 0;
		dimp_set(edi_mp_post_ei, 0);
		post_blend_en = 0;
	}

	if (dimp_get(edi_mp_mcpre_en))
		ppost->di_mcvecrd_mif.blend_en = post_blend_en;
	invert_mv = overturn ? (!invert_mv) : invert_mv;
	if (DIM_IS_IC_EF(SC2)) {
		sc2_post_cfg_set = &pst->pst_top_cfg;
		sc2_post_cfg = &ppost->pst_top_cfg;

		en_set_pst = &ppost->en_set;
		if (en_set_pst->b.if0 && acfg->buf_mif[0])
			en_set_pst->b.if0 = 1;
		else
			en_set_pst->b.if0 = 0;

		if (en_set_pst->b.if1 && acfg->buf_mif[1])
			en_set_pst->b.if1 = 1;
		else
			en_set_pst->b.if1 = 0;

		if (en_set_pst->b.if2 && acfg->buf_mif[2])
			en_set_pst->b.if2 = 1;
		else
			en_set_pst->b.if2 = 0;

		if (en_set_pst->b.if0)
			sc2_post_cfg->b.afbc_if0 = 1;
		else
			sc2_post_cfg->b.afbc_if0 = 0;

		if (en_set_pst->b.if1)
			sc2_post_cfg->b.afbc_if1 = 1;
		else
			sc2_post_cfg->b.afbc_if1 = 0;

		if (en_set_pst->b.if2)
			sc2_post_cfg->b.afbc_if2 = 1;
		else
			sc2_post_cfg->b.afbc_if2 = 0;

		if (sc2_post_cfg_set->d32 != sc2_post_cfg->d32) {
			sc2_post_cfg_set->d32 = sc2_post_cfg->d32;
			dim_sc2_contr_pst(sc2_post_cfg_set);
		}
	}
	if (IS_COMP_MODE(acfg->buf_mif[0]->vframe->type)) {
		dim_print("%s:afbc1\n", __func__);
		if (is_mask(SC2_ROT_WR)) {
			tmp = di_buf->vframe->width;
			di_buf->vframe->width	= di_buf->vframe->height;
			di_buf->vframe->height	= tmp;
		}
		acfg->buf_o	= di_buf;
		acfg->ei_en             = ei_en;            /* ei en*/
		acfg->blend_en          = post_blend_en;    /* blend en */
		acfg->blend_mtn_en      = blend_mtn_en;     /* blend mtn en */
		acfg->blend_mode        = post_blend_mode;  /* blend mode. */
		acfg->di_vpp_en         = di_vpp_en;	/* di_vpp_en. */
		acfg->di_ddr_en         = di_ddr_en;	/* di_ddr_en. */
		acfg->post_field_num    = post_field_num;/* 1bottom gen top */
		acfg->hold_line         = hold_line;
		acfg->urgent            = dimp_get(edi_mp_post_urgent);
		acfg->invert_mv         = (invert_mv ? 1 : 0);
		acfg->vskip_cnt         = dimp_get(edi_mp_di_vscale_skip_real);
		acfg->di_mtnprd_mif	= &ppost->di_mtnprd_mif;
		acfg->di_diwr_mif	= &ppost->di_diwr_mif;
	}

	if (ppost->update_post_reg_flag) {
		if (DIM_IS_IC_EF(SC2) &&
		    IS_COMP_MODE(acfg->buf_mif[0]->vframe->type)) {
			dimh_enable_di_post_afbc(acfg);
			/**/
			if ((!is_mask(SC2_DW_SHOW)) &&
			    pst->pst_top_cfg.b.afbc_wr) {
				pst->vf_post.type = acfg->buf_o->vframe->type;
#ifdef VFRAME_FLAG_DI_422_DW
				pst->vf_post.flag |= VFRAME_FLAG_DI_422_DW;
#endif
				pst->vf_post.canvas0Addr = -1;
				pst->vf_post.canvas1Addr = -1;

				pst->vf_post.compHeadAddr =
					di_buf->vframe->compHeadAddr;
				pst->vf_post.compBodyAddr =
					di_buf->vframe->compBodyAddr;
				pst->vf_post.compHeight   =
					di_buf->vframe->compHeight;
				pst->vf_post.compWidth    =
					di_buf->vframe->compWidth;
			}
			/* */
		} else {
			dimh_enable_di_post_2
				(&ppost->di_buf0_mif,
				 &ppost->di_buf1_mif,
				 &ppost->di_buf2_mif,
				 &ppost->di_diwr_mif,
				 &ppost->di_mtnprd_mif,
				 ei_en,            /* ei en*/
				 post_blend_en,    /* blend en */
				 blend_mtn_en,     /* blend mtn en */
				 post_blend_mode,  /* blend mode. */
				 di_vpp_en,	/* di_vpp_en. */
				 di_ddr_en,	/* di_ddr_en. */
				 post_field_num,/* 1bottom gen top */
				 hold_line,
				 dimp_get(edi_mp_post_urgent),
				 (invert_mv ? 1 : 0),
				 dimp_get(edi_mp_di_vscale_skip_real));
		}
		if (dimp_get(edi_mp_mcpre_en)) {
			tmptest = dimp_get(edi_mp_post_urgent);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
				dimh_en_mc_di_post_g12(&ppost->di_mcvecrd_mif,
						       tmptest,
						       overturn,
						       (invert_mv ? 1 : 0));
			else
				dimh_enable_mc_di_post(&ppost->di_mcvecrd_mif,
						       tmptest,
						       overturn,
						       (invert_mv ? 1 : 0));
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXLX)) {
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 0, 2);
		}
	} else {
		if (DIM_IS_IC_EF(SC2) &&
		    IS_COMP_MODE(acfg->buf_mif[0]->vframe->type)) {
			dimh_enable_di_post_afbc(acfg);
			dim_print("%s:di_buf:[0x%px],tp[%d],ind[%d],vf:0x%px\n",
				  __func__, di_buf, di_buf->type,
				  di_buf->index, di_buf->vframe);
			if (is_mask(SC2_DW_SHOW)) {
				dim_print("%s:dw show\n", __func__);
			} else if (pst->pst_top_cfg.b.afbc_wr) {
				pst->vf_post.type = acfg->buf_o->vframe->type;

#ifdef VFRAME_FLAG_DI_422_DW
				pst->vf_post.flag |= VFRAME_FLAG_DI_422_DW;
#endif
				pst->vf_post.canvas0Addr = -1;
				pst->vf_post.canvas1Addr = -1;
				pst->vf_post.compHeadAddr =
					di_buf->vframe->compHeadAddr;
				pst->vf_post.compBodyAddr =
					di_buf->vframe->compBodyAddr;
				pst->vf_post.compHeight   =
					di_buf->vframe->compHeight;
				pst->vf_post.compWidth    =
					di_buf->vframe->compWidth;
				dim_print("%s:after afbce 0x%px :vtype[0x%x]\n",
					  __func__, di_buf->vframe,
					  di_buf->vframe->type);
				dim_print("0x%px:\n", acfg->buf_o->vframe);
			}
		} else {
			dimh_post_switch_buffer
				(&ppost->di_buf0_mif,
				 &ppost->di_buf1_mif,
				 &ppost->di_buf2_mif,
				 &ppost->di_diwr_mif,
				 &ppost->di_mtnprd_mif,
				 &ppost->di_mcvecrd_mif,
				 ei_en, /* ei enable */
				 post_blend_en, /* blend enable */
				 blend_mtn_en,  /* blend mtn enable */
				 post_blend_mode,  /* blend mode. */
				 di_vpp_en,  /* di_vpp_en. */
				 di_ddr_en,	/* di_ddr_en. */
				 post_field_num,/*1bottom gen top */
				 hold_line,
				 dimp_get(edi_mp_post_urgent),
				 (invert_mv ? 1 : 0),
				 dimp_get(edi_mp_pulldown_enable),
				 dimp_get(edi_mp_mcpre_en),
				 dimp_get(edi_mp_di_vscale_skip_real));
		}
	}

		if (is_meson_gxtvbb_cpu()	||
		    is_meson_txl_cpu()		||
		    is_meson_txlx_cpu()		||
		    is_meson_gxlx_cpu()		||
		    is_meson_txhd_cpu()		||
		    is_meson_g12a_cpu()		||
		    is_meson_g12b_cpu()		||
		    is_meson_tl1_cpu()		||
		    is_meson_tm2_cpu()		||
		    is_meson_sm1_cpu()		||
		    DIM_IS_IC(T5)		||
		    DIM_IS_IC(T5D)		||
		    DIM_IS_IC(T5DB)		||
		    DIM_IS_IC_EF(SC2)) {
			if (di_cfg_top_get(EDI_CFG_REF_2)	&&
			    mc_pre_flag				&&
			    dimp_get(edi_mp_post_wr_en)) { /*OTT-3210*/
				dbg_once("mc_old=%d\n", mc_pre_flag);
				mc_pre_flag = 1;
			}
		dim_post_read_reverse_irq(overturn, mc_pre_flag,
					  post_blend_en ?
					  dimp_get(edi_mp_mcpre_en) : false);
		/* disable mc for first 2 fieldes mv unreliable */
		if (di_buf->seq < 2)
			DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 0, 2);
	}
	if (dimp_get(edi_mp_mcpre_en)) {
		if (di_buf->di_buf_dup_p[2])
			set_post_mcinfo(&di_buf->di_buf_dup_p[2]
				->curr_field_mcinfo);
	} else if (is_meson_gxlx_cpu()	||
		   is_meson_txl_cpu()	||
		   is_meson_txlx_cpu()) {
		DIM_VSC_WR_MPG_BT(MCDI_MC_CRTL, 0, 0, 2);
	}

/* set pull down region (f(t-1) */

	if (di_pldn_buf				&&
	    dimp_get(edi_mp_pulldown_enable)	&&
	    !ppre->cur_prog_flag) {
		unsigned short offset = (di_start_y >> 1);

		if (overturn)
			offset = ((di_buf->vframe->height - di_end_y) >> 1);
		else
			offset = 0;
		/*pulldown_vof_win_vshift*/
		get_ops_pd()->vof_win_vshift(&di_pldn_buf->pd_config, offset);
		dimh_pulldown_vof_win_config(&di_pldn_buf->pd_config);
	}
	//post_mif_sw(true);	/*position?*/
	if (DIM_IS_IC_EF(SC2))
		opl1()->pst_mif_sw(true, DI_MIF0_SEL_PST_ALL);
	else
		post_mif_sw(true);

	if (DIM_IS_IC_EF(SC2)) {
		if (opl1()->wrmif_trig)
			opl1()->wrmif_trig(EDI_MIFSM_WR);
	} else {
		/*add by wangfeng 2018-11-15 bit[31]: Pending_ddr_wrrsp_Diwr*/
		DIM_VSC_WR_MPG_BT(DI_DIWR_CTRL, 1, 31, 1);
		DIM_VSC_WR_MPG_BT(DI_DIWR_CTRL, 0, 31, 1);
	}

	/*ary add for post crash*/
	if (DIM_IS_IC_EF(SC2))
		opl1()->pst_set_flow((dimp_get(edi_mp_post_wr_en) &&
				      dimp_get(edi_mp_post_wr_support)),
				     EDI_POST_FLOW_STEP2_START);
	else
		di_post_set_flow((dimp_get(edi_mp_post_wr_en)	&&
				  dimp_get(edi_mp_post_wr_support)),
				 EDI_POST_FLOW_STEP2_START);
	if (ppost->update_post_reg_flag > 0)
		ppost->update_post_reg_flag--;

	di_buf->seq_post = ppost->post_wr_cnt;
	pst->flg_have_set = true;
	/*dbg*/
	dim_ddbg_mod_save(EDI_DBG_MOD_POST_SETE, channel, ppost->frame_cnt);
	dbg_post_cnt(channel, "ps2");
	ppost->frame_cnt++;
	ppost->seq++;
	pch->sum_pst++;

	return 0;
}

#ifndef DI_DEBUG_POST_BUF_FLOW
static void post_ready_buf_set(unsigned int ch, struct di_buf_s *di_buf)
{
	struct vframe_s *vframe_ret = NULL;
	struct di_buf_s *nr_buf = NULL;
	#ifdef VFM_ORI
	struct vframe_s **pvframe_in = get_vframe_in(ch);
	#else
	struct dim_nins_s *ins;
	#endif
	struct vframe_s *vf;

	struct di_hpst_s  *pst = get_hw_pst();
	struct di_ch_s *pch;
	struct di_buffer *buffer;
	struct di_buf_s *buf_in;

	pch = get_chdata(ch);

	vframe_ret = di_buf->vframe;
	nr_buf = di_buf->di_buf_dup_p[1];

	clear_vframe_src_fmt(vframe_ret);
	if (di_buf->local_meta &&
		di_buf->local_meta_used_size)
		update_vframe_src_fmt(vframe_ret,
			di_buf->local_meta,
			di_buf->local_meta_used_size,
			false, NULL, NULL);

	/* reset ud_param ptr */
	if (di_buf->local_ud &&
		di_buf->local_ud_used_size &&
		is_ud_param_valid(vframe_ret->vf_ud_param)) {
		vframe_ret->vf_ud_param.ud_param.pbuf_addr = (void *)di_buf->local_ud;
		vframe_ret->vf_ud_param.ud_param.buf_len = di_buf->local_ud_used_size;
	} else {
		vframe_ret->vf_ud_param.ud_param.pbuf_addr = NULL;
		vframe_ret->vf_ud_param.ud_param.buf_len = 0;
	}
	if ((dimp_get(edi_mp_post_wr_en)	&&
	     dimp_get(edi_mp_post_wr_support))	&&
	    di_buf->process_fun_index != PROCESS_FUN_NULL) {
	#ifdef DIM_OUT_NV21
		vframe_ret->type = pst->vf_post.type;
		vframe_ret->bitdepth = pst->vf_post.bitdepth;
		vframe_ret->plane_num = pst->vf_post.plane_num;
		vframe_ret->canvas0Addr = -1;
		vframe_ret->canvas1Addr = -1;
		vframe_ret->flag	= pst->vf_post.flag;
		vframe_ret->height	= pst->vf_post.height;
		vframe_ret->width	= pst->vf_post.width;
		memcpy(&vframe_ret->canvas0_config[0],
		       &pst->vf_post.canvas0_config[0],
		       sizeof(vframe_ret->canvas0_config[0]));
		memcpy(&vframe_ret->canvas0_config[1],
		       &pst->vf_post.canvas0_config[1],
		       sizeof(vframe_ret->canvas0_config[1]));
	#endif
		if (di_mp_uit_get(edi_mp_show_nrwr)) {
			vframe_ret->canvas0_config[0].phy_addr =
				nr_buf->nr_adr;
			vframe_ret->canvas0_config[0].width =
				nr_buf->canvas_width[NR_CANVAS];
			vframe_ret->canvas0_config[0].height =
				nr_buf->canvas_height;
		}

		vframe_ret->early_process_fun = dim_do_post_wr_fun;
		vframe_ret->process_fun = NULL;

		/* 2019-04-22 Suggestions from brian.zhu*/
		vframe_ret->mem_handle = NULL;
		vframe_ret->type |= VIDTYPE_DI_PW;
		#ifdef VFRAME_FLAG_DI_PW_VFM
		vframe_ret->flag &=
			~(VFRAME_FLAG_DI_PW_VFM |
			  VFRAME_FLAG_DI_PW_N_LOCAL |
			  VFRAME_FLAG_DI_PW_N_EXT);
		if (dip_itf_is_vfm(pch))
			vframe_ret->flag |= VFRAME_FLAG_DI_PW_VFM;
		else if (dip_itf_is_ins_lbuf(pch))
			vframe_ret->flag |= VFRAME_FLAG_DI_PW_N_LOCAL;
		else
			vframe_ret->flag |= VFRAME_FLAG_DI_PW_N_EXT;
		#endif
		if (!dip_itf_is_o_linear(pch))
			vframe_ret->flag &= (~VFRAME_FLAG_VIDEO_LINEAR);

		/* 2019-04-22 */

		/* dec vf keep */
		if (di_buf->in_buf) {
			vframe_ret->flag |= VFRAME_FLAG_DOUBLE_FRAM;
			//vframe_ret->vf_ext = pvframe_in[di_buf->in_buf->index];
			if (dip_itf_is_vfm(pch)) {
				ins = (struct dim_nins_s *)di_buf->in_buf->c.in;
				vframe_ret->vf_ext = ins->c.ori;
			} else {
				ins = (struct dim_nins_s *)di_buf->in_buf->c.in;
				buffer = (struct di_buffer *)ins->c.ori;
				vframe_ret->vf_ext = buffer->vf;
			}

			if (vframe_ret->vf_ext) {
				//vf = pvframe_in[di_buf->in_buf->index];
				vf = &ins->c.vfm_cp; //@ary_note: need change
				if (vf->type & VIDTYPE_COMPRESS) {
					vf->width = di_buf->dw_width_bk;
					vf->height = di_buf->dw_height_bk;
				}
				dim_print("dim:dec vf:post:p[%d],i[%d],v[%p]\n",
					  di_buf->index, di_buf->in_buf->index,
					  vframe_ret->vf_ext);
				dim_print("dim:dec vf:omx[%d][%d]\n",
					  di_buf->in_buf->vframe->index_disp,
					  vf->index_disp);
			} else {
				PR_ERR("%s:dec vf:buf:t[%d],i[%d],%d null\n",
				       __func__, di_buf->type, di_buf->index,
				       di_buf->in_buf->index);
				dim_print("dec vf:post:p[%d],i[%d],v[NULL]\n",
					  di_buf->index, di_buf->in_buf->index);
			}
		}
		if (dimp_get(edi_mp_force_width)) {
			if (IS_COMP_MODE(di_buf->vframe->type)) {
				vframe_ret->compWidth = dimp_get(edi_mp_force_width);
			} else {
				vframe_ret->width = dimp_get(edi_mp_force_width);
			}
		}
		/* dbg_vfm(vframe_ret, 2);*/
		if (di_buf->en_hf && di_buf->hf_done) {
			vframe_ret->flag |= VFRAME_FLAG_HF;
			//vframe_ret->
			vframe_ret->hf_info = &di_buf->hf;
			if (di_dbg & DBG_M_IC)
				dim_print_hf(vframe_ret->hf_info);
		} else {
			vframe_ret->flag &= ~VFRAME_FLAG_HF;
			vframe_ret->hf_info = NULL;
		}
	} else if (di_buf->flg_nr) {
		vframe_ret->mem_handle = NULL;
		vframe_ret->type |= VIDTYPE_DI_PW;
		#ifdef VFRAME_FLAG_DI_PW_VFM
		vframe_ret->flag &=
			~(VFRAME_FLAG_DI_PW_VFM |
			  VFRAME_FLAG_DI_PW_N_LOCAL |
			  VFRAME_FLAG_DI_PW_N_EXT);
		if (dip_itf_is_vfm(pch))
			vframe_ret->flag |= VFRAME_FLAG_DI_PW_VFM;
		else if (dip_itf_is_ins_lbuf(pch))
			vframe_ret->flag |= VFRAME_FLAG_DI_PW_N_LOCAL;
		else
			vframe_ret->flag |= VFRAME_FLAG_DI_PW_N_EXT;
		#endif
		if (!dip_itf_is_o_linear(pch))
			vframe_ret->flag &= (~VFRAME_FLAG_VIDEO_LINEAR);
		if (di_buf->flg_nv21) {
			//vframe_ret->plane_num = di_buf->vframe->plane_num;
			vframe_ret->canvas0Addr = -1;
			vframe_ret->canvas1Addr = -1;
			#ifdef ERR_CODE
			memcpy(&vframe_ret->canvas0_config[0],
			       &di_buf->vframe->canvas0_config[0],
			       sizeof(vframe_ret->canvas0_config[0]));
			memcpy(&vframe_ret->canvas0_config[1],
			       &di_buf->vframe->canvas0_config[1],
			       sizeof(vframe_ret->canvas0_config[1]));
			#endif

		} else {
			vframe_ret->plane_num = 1;
			vframe_ret->canvas0Addr = -1;
			vframe_ret->canvas1Addr = -1;
			vframe_ret->canvas0_config[0].phy_addr = di_buf->nr_adr;
			vframe_ret->canvas0_config[0].endian = 0;
			vframe_ret->canvas0_config[0].height =
				di_buf->canvas_height;
			vframe_ret->canvas0_config[0].width =
				di_buf->canvas_width[NR_CANVAS];
			vframe_ret->canvas0_config[0].block_mode = 0;
		}
		dim_print("%s:flg_nv21[%d] cvs_h[%d]\n", __func__,
			  di_buf->flg_nv21,
			  di_buf->vframe->canvas0_config[0].height);

		/* dec vf keep */
		if (di_buf->in_buf) {
			vframe_ret->flag |= VFRAME_FLAG_DOUBLE_FRAM;
			if (dip_itf_is_vfm(pch)) {
				ins = (struct dim_nins_s *)di_buf->in_buf->c.in;
				vframe_ret->vf_ext = ins->c.ori;
			} else {
				ins = (struct dim_nins_s *)di_buf->in_buf->c.in;
				buffer = (struct di_buffer *)ins->c.ori;
				vframe_ret->vf_ext = buffer->vf;
			}

			if (vframe_ret->vf_ext) {
				//vf = pvframe_in[di_buf->in_buf->index];
				vf = &ins->c.vfm_cp; //@ary_note:need change
				if (vf->type & VIDTYPE_COMPRESS) {
					vf->width = di_buf->dw_width_bk;
					vf->height = di_buf->dw_height_bk;
				}
				dim_print("dim:2decvf:post:p[%d],i[%d],v[%p]\n",
					  di_buf->index, di_buf->in_buf->index,
					  vframe_ret->vf_ext);
				dim_print("dim:2dec vf:omx[%d][%d]\n",
					  di_buf->in_buf->vframe->index_disp,
					  vf->index_disp);
			} else {
				PR_ERR("%s:2dec vf:buf:t[%d],i[%d],%d null\n",
				       __func__, di_buf->type, di_buf->index,
				       di_buf->in_buf->index);
				dim_print("2dec vf:post:p[%d],i[%d],v[NULL]\n",
					  di_buf->index, di_buf->in_buf->index);
			}
		}
		if (di_buf->en_hf && di_buf->hf_done) {
			vframe_ret->flag |= VFRAME_FLAG_HF;
			//vframe_ret->
			vframe_ret->hf_info = &di_buf->hf;
			if (di_dbg & DBG_M_IC)
				dim_print_hf(vframe_ret->hf_info);
		} else {
			vframe_ret->flag &= ~VFRAME_FLAG_HF;
			vframe_ret->hf_info = NULL;
		}
	} else if (di_buf->is_bypass_pst) {
		if (di_buf->di_buf_dup_p[0] && di_buf->di_buf_dup_p[0]->type == 2) {
			buf_in = di_buf->di_buf_dup_p[0];
			PR_INF("post bypass:\n");
			vframe_ret->mem_handle = NULL;
			#ifdef VFRAME_FLAG_DI_PW_VFM
			if (dip_itf_is_vfm(pch))
				vframe_ret->flag |= VFRAME_FLAG_DI_PW_VFM;
			else if (dip_itf_is_ins_lbuf(pch))
				vframe_ret->flag |= VFRAME_FLAG_DI_PW_N_LOCAL;
			else
				vframe_ret->flag |= VFRAME_FLAG_DI_PW_N_EXT;
			#endif
			if (!dip_itf_is_o_linear(pch))
				vframe_ret->flag &= (~VFRAME_FLAG_VIDEO_LINEAR);
			vframe_ret->type |= VIDTYPE_DI_PW;
			vframe_ret->type &= (~0xf);
			vframe_ret->plane_num = 1;
			di_buf->buf_hsize =  buf_in->buf_hsize;
			if (IS_COMP_MODE(di_buf->vframe->type)) {
				vframe_ret->compHeight = buf_in->vframe->compHeight / 2;
			} else {
				vframe_ret->height = buf_in->vframe->height / 2;
				vframe_ret->canvas0_config[0].phy_addr = buf_in->nr_adr;
				vframe_ret->canvas0_config[0].width = buf_in->canvas_width[0];
				vframe_ret->canvas0_config[0].block_mode = 0;
			}
			if (dimp_get(edi_mp_force_width)) {
				if (IS_COMP_MODE(di_buf->vframe->type))
					vframe_ret->compWidth = dimp_get(edi_mp_force_width);
				else
					vframe_ret->width = dimp_get(edi_mp_force_width);
			}
			PR_INF("\t:buf[%d,%d],vfm[%d,%d,0x%x]\n",
				di_buf->type, di_buf->index,
				di_buf->vframe->width, di_buf->vframe->height,
				di_buf->vframe->type);
			#ifdef CVS_UINT
			PR_INF("\t:0x%x,0x%x\n",
				di_buf->vframe->canvas0_config[0].phy_addr,
				di_buf->vframe->canvas0_config[1].phy_addr);
			#else
			PR_INF("\t:0x%lx,0x%lx\n",
				di_buf->vframe->canvas0_config[0].phy_addr,
				di_buf->vframe->canvas0_config[1].phy_addr);
			#endif
			PR_INF("\t:cmp[%d,%d]\n", di_buf->vframe->compWidth,
				di_buf->vframe->compHeight);
			PR_INF("\t:0x%lx,0x%lx\n",
				di_buf->vframe->compBodyAddr,
				di_buf->vframe->compHeadAddr);
		}

	} else {
		dim_print("%s:c\n", __func__);
	}

	vframe_ret->flag &= ~VFRAME_FLAG_DI_DW;
	if (vframe_ret->type & VIDTYPE_DI_PW	&&
	    di_buf->c.dvframe.is_dw) {
		vframe_ret->flag |= VFRAME_FLAG_DI_DW;
		vframe_ret->plane_num = di_buf->c.dvframe.plane_num;
		memcpy(&vframe_ret->canvas0_config[0],
		       &di_buf->c.dvframe.canvas0_config[0],
		       sizeof(vframe_ret->canvas0_config[0]));
		if (vframe_ret->plane_num == 2)
			memcpy(&vframe_ret->canvas0_config[1],
			       &di_buf->c.dvframe.canvas0_config[1],
			       sizeof(vframe_ret->canvas0_config[1]));
		vframe_ret->width = di_buf->c.dvframe.width;
		vframe_ret->height = di_buf->c.dvframe.height;

		if (dim_getdw()->dbg_show_dw) { /*dbg only*/
			//vframe_ret->width = dim_getdw()->shrk_cfg.
			dbg_ic("%s:dw:<%d><%d><%d><%d>\n",
				__func__,
				vframe_ret->canvas0_config[0].width,
				vframe_ret->canvas0_config[0].height,
				vframe_ret->width,
				vframe_ret->height);
#ifdef CVS_UINT
			dbg_ic("\t:0x%x,0x%x\n",
				di_buf->vframe->canvas0_config[0].phy_addr,
				di_buf->vframe->canvas0_config[1].phy_addr);
#else
			dbg_ic("\t:0x%lx,0x%lx\n",
				di_buf->vframe->canvas0_config[0].phy_addr,
				di_buf->vframe->canvas0_config[1].phy_addr);
#endif
			dim_print("%s:show dw:old type=0x%x\n", __func__,
				  vframe_ret->type);
			vframe_ret->type &= (~DI_VFM_T_MASK_DI_CLEAR);
			vframe_ret->type |= di_buf->c.dvframe.type;
			dim_print("%s:show dw:new type=0x%x\n", __func__,
				  vframe_ret->type);
			vframe_ret->bitdepth = di_buf->c.dvframe.bitdepth;
			vframe_ret->flag &= ~VFRAME_FLAG_HF;
			vframe_ret->hf_info = NULL;
		}
	}

	if (dip_itf_is_ins_exbuf(pch)) {
		vframe_ret->type &= (~VIDTYPE_DI_PW);
	} else {
	#ifdef MARK_HIS
		if ((vframe_ret->type & VIDTYPE_DI_PW)	&&
		    cfgg(LINEAR)			&&
		    !IS_COMP_MODE(vframe_ret->type)) {
			vframe_ret->canvas0_config[0].width = di_buf->buf_hsize;
			vframe_ret->canvas0_config[1].width = di_buf->buf_hsize;
			dbg_ic("set w buf_size[%d]:\n", di_buf->buf_hsize);
		}
	#endif
	}
}

#endif
void dim_post_de_done_buf_config(unsigned int channel)
{
	//2020-12-07	ulong irq_flag2 = 0;
	struct di_buf_s *di_buf = NULL;
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct di_ch_s *pch;
	unsigned int datacrc = 0xffffffff;
	unsigned int mtncrc = 0xffffffff;

	if (!ppost->cur_post_buf) {
		PR_ERR("%s:no cur\n", __func__);
		return;
	}
	pch = get_chdata(channel);
	dbg_post_cnt(channel, "pd1");
	/*dbg*/
	dim_ddbg_mod_save(EDI_DBG_MOD_POST_DB, channel, ppost->frame_cnt);
	di_buf = ppost->cur_post_buf;

	//2020-12-07	di_lock_irqfiq_save(irq_flag2);
	queue_out(channel, ppost->cur_post_buf);/*? which que?post free*/

	if (de_devp->pps_enable && dimp_get(edi_mp_pps_position) == 0) {
		di_buf->vframe->width = dimp_get(edi_mp_pps_dstw);
		di_buf->vframe->height = dimp_get(edi_mp_pps_dsth);
	}
	if (di_buf->flg_afbce_set) { /*afbce check cec*/
		di_buf->flg_afbce_set = 0;
	}
	if (di_buf->process_fun_index != PROCESS_FUN_NULL &&
	    di_buf->hf_done) {
		di_hf_hw_release(2);
		//opl1()->aisr_disable();
	}

	#ifdef DI_DEBUG_POST_BUF_FLOW
	#else
	post_ready_buf_set(channel, di_buf);
	#endif
	if (di_buf->is_lastp) {
		dbg_mem2("pst:lastp\n");
		di_buf->is_lastp = 0;
	}

	//ary 2020-12-07 di_que_in(channel, QUE_POST_READY, ppost->cur_post_buf);
	if (ppost->cur_post_buf->field_count == 0)
		dbg_timer(channel, EDBG_TIMER_1_PSTREADY);
	else if (ppost->cur_post_buf->field_count == 1)
		dbg_timer(channel, EDBG_TIMER_2_PSTREADY);
	#ifdef DI_DEBUG_POST_BUF_FLOW
	#else
	/*add by ary:*/
	if (!di_buf->is_bypass_pst)
		recycle_post_ready_local(ppost->cur_post_buf, channel);
	#endif
	//2020-12-07	di_unlock_irqfiq_restore(irq_flag2);
	dim_tr_ops.post_ready(di_buf->vframe->index_disp);
	if (dimp_get(edi_mp_pstcrc_ctrl) == 1) {
		if (DIM_IS_IC(T5) || DIM_IS_IC(T5DB) ||
		    DIM_IS_IC(T5D)) {
			datacrc = RD(DI_T5_RO_CRC_DEINT);
			//test crc DIM_DI_WR_REG_BITS(DI_T5_CRC_CHK0,
			//test crc		      0x1, 30, 1);
		} else if (DIM_IS_IC_EF(SC2)) {
			datacrc = RD(DI_RO_CRC_DEINT);
			DIM_DI_WR_REG_BITS(DI_CRC_CHK0,
					   0x1, 30, 1);
			mtncrc = RD(DI_RO_CRC_MTNWR);
			DIM_DI_WR_REG_BITS(DI_CRC_CHK0,
					   0x1, 29, 1);
		}
		dbg_post_ref("DEINT ==ch[=0x%x]\n", datacrc);
		//dbg_post_ref("irq p= 0x%p\n",ppost->cur_post_buf);
		ppost->cur_post_buf->datacrc = datacrc;
		ppost->cur_post_buf->nrcrc = di_buf->nrcrc;
		ppost->cur_post_buf->mtncrc = mtncrc;
		dbg_post_ref("DEINT[=0x%x],NRWR[=0x%x],MTN[=0x%x]\n",
		     datacrc, ppost->cur_post_buf->nrcrc, mtncrc);
		//dbg_slt_crc_count(pch, datacrc, di_buf->nrcrc, mtncrc);
		//dbg_slt_crc(di_buf);
	}
	pch->itf.op_fill_ready(pch, ppost->cur_post_buf);
	//mtask_wake_m();
	#ifdef MARK_HIS //2020-12-07 move to ndis_fill_ready
	pw_vf_notify_receiver(channel,
			      VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	#endif
	ppost->cur_post_buf = NULL;
	/*dbg*/
	dim_ddbg_mod_save(EDI_DBG_MOD_POST_DE, channel, ppost->frame_cnt);
	dbg_post_cnt(channel, "pd2");
}

static void recycle_vframe_type_post(struct di_buf_s *di_buf,
				     unsigned int channel)
{
	int i;
	struct div2_mm_s *mm;
	struct di_ch_s *pch = get_chdata(channel);
	bool release_flg = false;
	bool sct_buf = false;//for sct

	if (!di_buf) {
		PR_ERR("%s:\n", __func__);
		if (recovery_flag == 0)
			recovery_log_reason = 15;

		recovery_flag++;
		return;
	}
	if (di_buf->process_fun_index == PROCESS_FUN_DI)
		dec_post_ref_count(di_buf);

	for (i = 0; i < 2; i++) {
		if (di_buf->di_buf[i]) {
			queue_in(channel, di_buf->di_buf[i], QUEUE_RECYCLE);
			dim_print("%s: ch[%d]:di_buf[%d],type=%d\n", __func__,
				  channel, di_buf->di_buf[i]->index,
				  di_buf->di_buf[i]->type);
			if (dimp_get(edi_mp_bypass_post_state)) {
				PR_INF("%s: ch[%d]:di_buf[%d],type=%d\n", __func__,
				  channel, di_buf->di_buf[i]->index,
				  di_buf->di_buf[i]->type);
			}
		}
	}
	queue_out(channel, di_buf); /* remove it from display_list_head */
	if (di_buf->queue_index != -1) {
		PR_ERR("qout err:index[%d],typ[%d],qindex[%d]\n",
		       di_buf->index, di_buf->type, di_buf->queue_index);
	/* queue_out_dbg(channel, di_buf);*/
	}
	di_buf->invert_top_bot_flag = 0;
	di_buf->flg_nr = 0;
	di_buf->flg_nv21 = 0;
	mm = dim_mm_get(channel);
	if (di_buf->blk_buf) {
		if (di_buf->blk_buf->flg.d32 != mm->cfg.pbuf_flg.d32) {
			mem_release_one_inused(pch, di_buf->blk_buf);
			dbg_mem2("keep_buf:3:flg trig realloc,0x%x->0x%x\n",
				 di_buf->blk_buf->flg.d32,
				 mm->cfg.pbuf_flg.d32);
			di_buf->blk_buf = NULL;
			di_que_in(channel, QUE_PST_NO_BUF, di_buf);
			release_flg = true;
			mm->sts.flg_realloc++;
			dbg_mem2("%s:stsflg_realloc[%d]\n", __func__,
				 mm->sts.flg_realloc);
		} else if (di_buf->blk_buf->flg.b.typ == EDIM_BLK_TYP_PSCT) {
			if (di_buf->blk_buf->sct) {
				qsct_used_some_to_recycle(pch,
					(struct dim_sct_s *)di_buf->blk_buf->sct);
				di_buf->blk_buf->sct = NULL;
				di_buf->blk_buf->pat_buf = NULL;
				di_que_in(channel, QUE_PST_NO_BUF_WAIT, di_buf);
				sct_buf = true;
			}
		}
	} else {
		release_flg = true;
		di_que_in(channel, QUE_PST_NO_BUF, di_buf);
	}
	if (!release_flg && !sct_buf)
		di_que_in(channel, QUE_POST_FREE, di_buf);
}

void recycle_post_ready_local(struct di_buf_s *di_buf,
			      unsigned int channel)
{
	int i;

	if (di_buf->type != VFRAME_TYPE_POST)
		return;

	if (di_buf->process_fun_index == PROCESS_FUN_NULL)	/*bypass?*/
		return;

	if (di_buf->process_fun_index == PROCESS_FUN_DI)
		dec_post_ref_count(di_buf);

	for (i = 0; i < 2; i++) {
		if (di_buf->di_buf[i]) {
			queue_in(channel, di_buf->di_buf[i], QUEUE_RECYCLE);
			dim_print("%s: ch[%d]:di_buf[%d],type=%d\n",
				  __func__,
				  channel,
				  di_buf->di_buf[i]->index,
				  di_buf->di_buf[i]->type);
			di_buf->di_buf[i] = NULL;
		}
	}
}

#ifdef DI_BUFFER_DEBUG
static void
recycle_vframe_type_post_print(struct di_buf_s *di_buf,
			       const char *func,
			       const int	line)
{
	int i;

	dim_print("%s:%d ", func, line);
	for (i = 0; i < 2; i++) {
		if (di_buf->di_buf[i])
			dim_print("%s[%d]<%d>=>recycle_list; ",
				  vframe_type_name[di_buf->di_buf[i]->type],
				  di_buf->di_buf[i]->index, i);
	}
	dim_print("%s[%d] =>post_free_list\n",
		  vframe_type_name[di_buf->type], di_buf->index);
}
#endif

static unsigned int pldn_dly1 = 1;
static void set_pulldown_mode(struct di_buf_s *di_buf, unsigned int channel)
{
	struct di_buf_s *pre_buf_p = di_buf->di_buf_dup_p[pldn_dly1];
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXBB)) {
		if (dimp_get(edi_mp_pulldown_enable) &&
		    !ppre->cur_prog_flag) {
			if (pre_buf_p) {
				di_buf->pd_config.global_mode =
					pre_buf_p->pd_config.global_mode;
			} else {
				/* ary add 2019-06-19*/
				di_buf->pd_config.global_mode =
					PULL_DOWN_EI;
				PR_ERR("[%s]: index out of range.\n",
				       __func__);
			}
		} else {
			di_buf->pd_config.global_mode = PULL_DOWN_NORMAL;
		}
	}
}

static void drop_frame(int check_drop, int throw_flag, struct di_buf_s *di_buf,
		       unsigned int channel)
{
//ary 2020-12-09	ulong irq_flag2 = 0;
	int i = 0, drop_flag = 0;
	struct di_post_stru_s *ppost = get_post_stru(channel);

//ary 2020-12-09	di_lock_irqfiq_save(irq_flag2);
	if (frame_count == 0 && check_drop) {
		ppost->start_pts = di_buf->vframe->pts;
		ppost->start_pts64 = di_buf->vframe->pts_us64;
	}
	if ((check_drop &&
	     frame_count < dimp_get(edi_mp_start_frame_drop_count)) ||
	     throw_flag) {
		drop_flag = 1;
	} else {
		if (check_drop && frame_count
			== dimp_get(edi_mp_start_frame_drop_count)) {
			if (ppost->start_pts &&
			    di_buf->vframe->pts == 0) {
				di_buf->vframe->pts = ppost->start_pts;
				di_buf->vframe->pts_us64 = ppost->start_pts64;
			}
			ppost->start_pts = 0;
		}
		for (i = 0; i < 3; i++) {
			if (di_buf->di_buf_dup_p[i]) {
				if (di_buf->di_buf_dup_p[i]->vframe->bitdepth !=
					di_buf->vframe->bitdepth) {
					pr_info("%s buf[%d] not match bit mode\n",
						__func__, i);
					drop_flag = 1;
					break;
				}
			}
		}
	}
	if (drop_flag) {
		dim_print("%s:drop:t[%d],indx[%d]\n", __func__,
			  di_buf->type, di_buf->index);

		/*dec vf keep*/
		if (di_buf->in_buf) {
			queue_in(channel, di_buf->in_buf, QUEUE_RECYCLE);
			di_buf->in_buf = NULL;
		}
		queue_in(channel, di_buf, QUEUE_TMP);
		recycle_vframe_type_post(di_buf, channel);

#ifdef DI_BUFFER_DEBUG
		recycle_vframe_type_post_print(di_buf, __func__, __LINE__);
#endif
	} else {
		dim_print("%s:wr_en[%d],support[%d]\n", __func__,
			  dimp_get(edi_mp_post_wr_en),
			  dimp_get(edi_mp_post_wr_support));

		if (dimp_get(edi_mp_post_wr_en) &&
		    dimp_get(edi_mp_post_wr_support)) {
			//queue_in(channel, di_buf, QUEUE_POST_DOING);
			di_que_in(channel, QUE_POST_DOING, di_buf);
		} else {
			//no use di_que_in(channel, QUE_POST_READY, di_buf);
		}
		dim_tr_ops.post_do(di_buf->vframe->index_disp);
		dim_print("di:ch[%d]:%dth %s[%d] => post ready %u ms.\n",
			  channel,
			  frame_count,
			  vframe_type_name[di_buf->type], di_buf->index,
			  jiffies_to_msecs(jiffies_64 -
			  di_buf->vframe->ready_jiffies64));
	}
//ary 2020-12-09	di_unlock_irqfiq_restore(irq_flag2);
}

static bool dim_pst_vfm_bypass(struct di_ch_s *pch, struct di_buf_s *ready_buf)
{
	unsigned int ch;
	struct di_buf_s *di_buf, *p;

	ch = pch->ch_id;
	if (ready_buf && ready_buf->is_eos && !ready_buf->c.in) {
	/* int eos */
		dbg_bypass("%s:only int eos\n", __func__);
		p = di_que_out_to_di_buf(ch, QUE_PRE_READY);
		p->is_eos = 0;
		queue_in(ch, p, QUEUE_RECYCLE);
		return true;
	}

	//dbg_bypass("%s:1\n", __func__);
	di_buf = di_que_out_to_di_buf(ch, QUE_PST_NO_BUF);
	if (dim_check_di_buf(di_buf, 19, ch)) {
		PR_ERR("%s:no pst_no_buf", __func__);
		return false;
	}
	//dbg_bypass("%s:2\n", __func__);
	p = di_que_out_to_di_buf(ch, QUE_PRE_READY);
	//dbg_bypass("%s:3\n", __func__);
	di_buf->di_buf_dup_p[0] = p;
	di_buf->di_buf_dup_p[1] = NULL;
	di_buf->di_buf_dup_p[2] = NULL;
	di_buf->process_fun_index =	PROCESS_FUN_NULL;
	di_buf->is_nbypass = p->is_nbypass;
	di_buf->is_eos = p->is_eos;
	//dbg_bypass("%s:4\n", __func__);
	di_que_in(ch, QUE_POST_DOING, di_buf);
	//dbg_bypass("%s:5\n", __func__);
	//dbg_bypass("%s:0x%px:%d,%d\n", __func__, p, p->type, p->index);
	//dbg_bypass("%s:\n", __func__);
	return true;
}

int dim_process_post_vframe(unsigned int channel)
{
/*
 * 1) get buf from post_free_list, config it according to buf
 * in pre_ready_list, send it to post_ready_list
 * (it will be send to post_free_list in di_vf_put())
 * 2) get buf from pre_ready_list, attach it to buf from post_free_list
 * (it will be send to recycle_list in di_vf_put() )
 */
//ary 2020-12-09	ulong irq_flag2 = 0;
	int i = 0;
	int tmp = 0;
	int tmp0 = 0;
	int tmp1 = 0;
	unsigned int tmp2;
	int ret = 0;
	int buffer_keep_count = 3;
	struct di_buf_s *di_buf = NULL;
	struct di_buf_s *ready_di_buf;
	struct di_buf_s *p = NULL;/* , *ptmp; */
	int itmp;
	/* new que int ready_count = list_count(channel, QUEUE_PRE_READY);*/
	int ready_count = di_que_list_count(channel, QUE_PRE_READY);
	bool check_drop = false;
	unsigned int tmpa[MAX_FIFO_SIZE]; /*new que*/
	unsigned int psize; /*new que*/
	struct di_ch_s *pch = get_chdata(channel);
	struct di_buf_s *tmp_buf[3];
	bool flg_eos = false;
	//struct dim_nins_s *nins; //add for eos

#ifdef MARK_SC2 /* */
	if (di_que_is_empty(channel, QUE_POST_FREE))
		return 0;
#endif
	/*add : for now post buf only 3.*/
	//if (list_count(channel, QUEUE_POST_DOING) > 2)
	if (di_que_list_count(channel, QUE_POST_DOING) > 2)
		return 0;

	if (ready_count == 0)
		return 0;

	ready_di_buf = di_que_peek(channel, QUE_PRE_READY);
	if (!ready_di_buf || !ready_di_buf->vframe) {
		pr_dbg("%s:Error1\n", __func__);

		if (recovery_flag == 0)
			recovery_log_reason = 16;

		recovery_flag++;
		return 0;
	}
	if (!ready_di_buf->flg_null && !ready_di_buf->buf_is_i) {
		if (!pp_check_buf_post(pch))
			return 0;
	}
	/*dim_print("%s:1 ready_count[%d]:post_proc_flag[%d]\n", __func__,*/
	/*	  ready_count, ready_di_buf->post_proc_flag);	*/
	if (ready_di_buf->post_proc_flag &&
	    ready_count >= buffer_keep_count) {
		i = 0;

		di_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);
		for (itmp = 0; itmp < psize; itmp++) {
			p = pw_qindex_2_buf(channel, tmpa[itmp]);
			/* if(p->post_proc_flag == 0){ */
			#ifdef HIS_CODE
			if (p->type == VFRAME_TYPE_IN) {
				ready_di_buf->post_proc_flag = -1;
				ready_di_buf->new_format_flag = 1;
			}
			#endif
			if (p->is_eos)
				flg_eos = true;
			i++;
			if (i > 2)
				break;
		}
	}
	if (ready_di_buf->is_eos) {
		dbg_reg("%s:ch[%d] only eos\n", __func__, channel);
		dim_pst_vfm_bypass(pch, ready_di_buf);
		if (ready_count > 1)
			return 1;
		return 0;
	}
	if (ready_di_buf->is_nbypass) {
		dbg_bypass("%s:bypass?\n", __func__);
		dim_pst_vfm_bypass(pch, NULL);
		return 1;
	}
	if (ready_di_buf->post_proc_flag > 0) {
		if (ready_count >= buffer_keep_count && flg_eos)  {
			for (i = 0; i < 3; i++)
				tmp_buf[i] = NULL;
			i = 0;
			di_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

			for (itmp = 0; itmp < psize; itmp++) {
				p = pw_qindex_2_buf(channel, tmpa[itmp]);
				dim_print("di:keep[%d]:t[%d]:idx[%d]\n",
					  i, tmpa[itmp], p->index);
				//di_buf->di_buf_dup_p[i++] = p;
				tmp_buf[i++] = p;

				if (i >= buffer_keep_count)
					break;
			}

			if (!tmp_buf[1]->is_eos && tmp_buf[1]->di_buf_post) {
				di_buf = tmp_buf[1]->di_buf_post;
				tmp_buf[1]->di_buf_post = NULL;
				if (!di_buf) {
					PR_ERR("%s: eos pst null\n", __func__);
					return 0;
				}
				dbg_bypass("%s:eos:post_buf:t[%d]idx[%d]\n",
					  __func__, di_buf->type,
					  di_buf->index);
				memcpy(di_buf->vframe,
				       tmp_buf[1]->vframe,
				       sizeof(vframe_t));
				di_buf->vframe->private_data = di_buf;
				di_buf->pd_config.global_mode =	PULL_DOWN_EI;
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;

				di_buf->vframe->width =
					tmp_buf[1]->width_bk;
				di_buf->dw_width_bk = ready_di_buf->dw_width_bk;
				di_buf->dw_height_bk = ready_di_buf->dw_height_bk;

				di_buf->vframe->early_process_fun =
							do_nothing_fun;

				di_buf->vframe->process_fun = NULL;
				di_buf->process_fun_index = PROCESS_FUN_DI;
				di_buf->di_buf_dup_p[0] = tmp_buf[0];
				di_buf->di_buf_dup_p[1] = tmp_buf[1];
				di_buf->di_buf_dup_p[2] = NULL;
				inc_post_ref_count(di_buf);

				di_buf->di_buf[0] = tmp_buf[0];
				di_buf->di_buf[1] = tmp_buf[1];
				di_buf->di_buf[0]->pre_ref_count = 0;
				di_buf->di_buf[1]->pre_ref_count = 0;
				queue_out(channel, tmp_buf[0]);
				queue_out(channel, tmp_buf[1]);
				dbg_reg("eos:que out 0:t[%d]idx[%d]\n",
					tmp_buf[0]->type,
					tmp_buf[0]->index);
				dbg_reg("eos:que out 1:t[%d]idx[%d]\n",
					tmp_buf[1]->type,
					tmp_buf[1]->index);
//				tmp = di_buf->di_buf_dup_p[0]->throw_flag;
//				tmp0 = di_buf->di_buf_dup_p[1]->throw_flag;
//				tmp1 = di_buf->di_buf_dup_p[2]->throw_flag;
				drop_frame(true, 0, di_buf, channel);

				frame_count++;

				ret = true;
			}

			if (tmp_buf[2] && tmp_buf[2]->is_eos) {
				#ifdef HIS_CODE
				tmp_buf[2]->is_eos = 0;
				queue_out(channel, tmp_buf[2]);
				di_que_in(channel, QUE_PRE_NO_BUF, tmp_buf[2]);
				#endif
				dim_pst_vfm_bypass(pch, tmp_buf[2]);
				dbg_reg("eos:que out 2:t[%d]idx[%d]\n",
					tmp_buf[2]->type, tmp_buf[2]->index);
			} else {
				PR_ERR("tmp_buf[2] is not eos\n");
			}
		} else if (ready_count >= buffer_keep_count) {/* i ?*/
			i = 0;

			di_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

			for (itmp = 0; itmp < psize; itmp++) {
				p = pw_qindex_2_buf(channel, tmpa[itmp]);
				dim_print("di:keep[%d]:t[%d]:idx[%d]\n",
					  i, tmpa[itmp], p->index);
				//di_buf->di_buf_dup_p[i++] = p;
				tmp_buf[i++] = p;

				if (i >= buffer_keep_count)
					break;
			}
			if (i < buffer_keep_count) {
				PR_ERR("%s:3\n", __func__);

				if (recovery_flag == 0)
					recovery_log_reason = 18;
				recovery_flag++;
				return 0;
			}
			di_buf = tmp_buf[1]->di_buf_post;
			tmp_buf[1]->di_buf_post = NULL;
			if (!di_buf) {
				PR_ERR("%s:di_buf_post is null\n", __func__);
				return 0;
			}
			for (i = 0; i < 3; i++)
				di_buf->di_buf_dup_p[i] = tmp_buf[i];

			memcpy(di_buf->vframe,
			       di_buf->di_buf_dup_p[1]->vframe,
			       sizeof(vframe_t));
			if (di_buf->local_meta &&
			    di_buf->di_buf_dup_p[1]->local_meta &&
			    di_buf->di_buf_dup_p[1]->local_meta_used_size) {
				memset(di_buf->local_meta, 0,
					di_buf->local_meta_total_size);
				memcpy(di_buf->local_meta,
					di_buf->di_buf_dup_p[1]->local_meta,
					di_buf->di_buf_dup_p[1]->local_meta_used_size * sizeof(u8));
				di_buf->local_meta_used_size =
					di_buf->di_buf_dup_p[1]->local_meta_used_size;
			} else {
				di_buf->local_meta_used_size = 0;
			}
			if (di_buf->local_ud &&
			    di_buf->di_buf_dup_p[1]->local_ud &&
			    di_buf->di_buf_dup_p[1]->local_ud_used_size) {
				memset(di_buf->local_ud, 0,
					di_buf->local_ud_total_size);
				memcpy(di_buf->local_ud,
					di_buf->di_buf_dup_p[1]->local_ud,
					di_buf->di_buf_dup_p[1]->local_ud_used_size * sizeof(u8));
				di_buf->local_ud_used_size =
					di_buf->di_buf_dup_p[1]->local_ud_used_size;
			} else {
				di_buf->local_ud_used_size = 0;
			}
			di_buf->vframe->private_data = di_buf;
			di_buf->afbc_sgn_cfg =
				di_buf->di_buf_dup_p[1]->afbc_sgn_cfg;
			memcpy(&di_buf->pq_rpt,
			       &di_buf->di_buf_dup_p[1]->pq_rpt,
			       sizeof(di_buf->pq_rpt));
			di_buf->field_count = di_buf->di_buf_dup_p[1]->field_count;
			if (di_buf->di_buf_dup_p[1]->post_proc_flag == 3) {
				/* dummy, not for display */
				inc_post_ref_count(di_buf);
				di_buf->di_buf[0] = di_buf->di_buf_dup_p[0];
				di_buf->di_buf[1] = NULL;
				queue_out(channel, di_buf->di_buf[0]);
//ary 2020-12-09				di_lock_irqfiq_save(irq_flag2);
				queue_in(channel, di_buf, QUEUE_TMP);
				recycle_vframe_type_post(di_buf, channel);

//ary 2020-12-09				di_unlock_irqfiq_restore(irq_flag2);
				dim_print("%s <dummy>: ", __func__);

			} else {
				tmp2 = di_buf->di_buf_dup_p[1]->field_count;
				if (di_buf->di_buf_dup_p[1]->post_proc_flag
						== 2) {
					di_buf->pd_config.global_mode =
						PULL_DOWN_BLEND_2;
					/* blend with di_buf->di_buf_dup_p[2] */
				} else if (tmp2 < 2) {
					di_buf->pd_config.global_mode =
						PULL_DOWN_EI;
				} else {
					set_pulldown_mode(di_buf, channel);
					if (tmp2 == 2)
						di_buf->trig_post_update = 1;
				}
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;

			di_buf->vframe->width =
				di_buf->di_buf_dup_p[1]->width_bk;
			di_buf->dw_width_bk = ready_di_buf->dw_width_bk;
			di_buf->dw_height_bk = ready_di_buf->dw_height_bk;

			if (di_buf->di_buf_dup_p[1]->new_format_flag) {
				/* if (di_buf->di_buf_dup_p[1]
				 * ->post_proc_flag == 2) {
				 */
				di_buf->vframe->early_process_fun =
						de_post_disable_fun;
			} else {
				di_buf->vframe->early_process_fun =
							do_nothing_fun;
			}

			if (di_buf->di_buf_dup_p[1]->type == VFRAME_TYPE_IN) {
				/* next will be bypass */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;
				di_buf->vframe->height >>= 1;
				di_buf->vframe->canvas0Addr =
					di_buf->di_buf_dup_p[0]
					->nr_canvas_idx; /* top */
				di_buf->vframe->canvas1Addr =
					di_buf->di_buf_dup_p[0]
					->nr_canvas_idx;
				di_buf->vframe->process_fun =
					NULL;
				di_buf->process_fun_index = PROCESS_FUN_NULL;
			} else {
				/*for debug*/
				if (dimp_get(edi_mp_debug_blend_mode) != -1)
					di_buf->pd_config.global_mode =
					dimp_get(edi_mp_debug_blend_mode);

				di_buf->vframe->process_fun =
((dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support)) ?
				NULL : dim_post_process);
				di_buf->process_fun_index = PROCESS_FUN_DI;
					inc_post_ref_count(di_buf);
				}
				di_buf->di_buf[0] = /*ary:di_buf_di_buf*/
					di_buf->di_buf_dup_p[0];
				di_buf->di_buf[1] = NULL;
				queue_out(channel, di_buf->di_buf[0]);

				tmp = di_buf->di_buf_dup_p[0]->throw_flag;
				tmp0 = di_buf->di_buf_dup_p[1]->throw_flag;
				tmp1 = di_buf->di_buf_dup_p[2]->throw_flag;
				drop_frame(true, tmp || tmp0 || tmp1,
					   di_buf, channel);

				frame_count++;

				if (!(dimp_get(edi_mp_post_wr_en) &&
				      dimp_get(edi_mp_post_wr_support)))
					pw_vf_notify_receiver(channel,
VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
			}
			if (dip_itf_is_ins(pch) && dim_dbg_new_int(2))
				dim_dbg_buffer2(di_buf->c.buffer, 5);
			ret = 1;
		}
	} else {
		if (is_progressive(ready_di_buf->vframe)	&&
		    !ready_di_buf->flg_null		&&
		    !ready_di_buf->buf_is_i		&&
		    ready_di_buf->type != VFRAME_TYPE_IN) {
/******************************************************************************/
/* p as p */
			struct di_buf_s *di_buf_i;

			//dim_print("%s:p as p\n", __func__);

//ary 2020-12-09			di_lock_irqfiq_save(irq_flag2);

			queue_out(channel, ready_di_buf);

			di_buf = pp_local_2_post(pch, ready_di_buf);
			//di_que_out_to_di_buf(channel, QUE_POST_FREE);
			if (dim_check_di_buf(di_buf, 19, channel)) {
//ary 2020-12-09				di_unlock_irqfiq_restore(irq_flag2);
				return 0;
			}

//ary 2020-12-09			di_unlock_irqfiq_restore(irq_flag2);
			memcpy(&di_buf->pq_rpt, &ready_di_buf->pq_rpt,
			       sizeof(di_buf->pq_rpt));
			di_buf->di_buf_dup_p[0] = di_buf;//ready_di_buf;
			di_buf->di_buf_dup_p[1] = NULL;
			di_buf->di_buf_dup_p[2] = NULL;

			di_buf_i = ready_di_buf;
			dim_print
				("p as p ready_buf:pnub[%d],cvs_w[%d]\n",
				 ready_di_buf->vframe->plane_num,
				 ready_di_buf->vframe->canvas0_config[0].width);
			memcpy(di_buf->vframe, di_buf_i->vframe,
			       sizeof(vframe_t));
			if (di_buf->local_meta &&
			    di_buf_i->local_meta &&
			    di_buf_i->local_meta_used_size) {
				memset(di_buf->local_meta, 0,
					di_buf->local_meta_total_size);
				memcpy(di_buf->local_meta,
					di_buf_i->local_meta,
					di_buf_i->local_meta_used_size *
					sizeof(u8));
				di_buf->local_meta_used_size =
					di_buf_i->local_meta_used_size;
			} else {
				di_buf->local_meta_used_size = 0;
			}
			if (di_buf->local_ud &&
			    di_buf_i->local_ud &&
			    di_buf_i->local_ud_used_size) {
				memset(di_buf->local_ud, 0,
					di_buf->local_ud_total_size);
				memcpy(di_buf->local_ud,
					di_buf_i->local_ud,
					di_buf_i->local_ud_used_size *
					sizeof(u8));
				di_buf->local_ud_used_size =
					di_buf_i->local_ud_used_size;
			} else {
				di_buf->local_ud_used_size = 0;
			}

			di_buf->vframe->width = di_buf_i->width_bk;
			di_buf->dw_width_bk = ready_di_buf->dw_width_bk;
			di_buf->dw_height_bk = ready_di_buf->dw_height_bk;
			di_buf->vframe->private_data = di_buf;

			di_buf->vframe->early_process_fun =
							do_pre_only_fun;

			di_buf->vframe->process_fun =	NULL;
			di_buf->process_fun_index = PROCESS_FUN_NULL;
			di_buf->pd_config.global_mode =	PULL_DOWN_NORMAL;

			di_buf->di_buf[0] = NULL;//ready_di_buf;
			di_buf->di_buf[1] = NULL;
			di_buf->flg_nr = 1;
			if (ready_di_buf->is_lastp) {
				ready_di_buf->is_lastp = 0;
				di_buf->is_lastp = 1;
			}
			di_buf->flg_nv21 = di_buf_i->flg_nv21;
			pp_drop_frame(di_buf, channel);
			#ifdef HOLD_ONE_FRAME
			di_que_in(channel, QUE_PRE_NO_BUF, ready_di_buf); //mark
			#else
			if (ready_di_buf->is_bypass_mem) {
				pp_buf_clear(ready_di_buf);
				di_que_in(channel, QUE_PRE_NO_BUF,
					  ready_di_buf);
			}
			#endif
			frame_count++;

			ret = 1;
#ifdef MARK_HIS
			pw_vf_notify_receiver
				(channel,
				 VFRAME_EVENT_PROVIDER_VFRAME_READY,
				 NULL);
#endif
/******************************************************************************/

		} else if (is_progressive(ready_di_buf->vframe) ||
		    ready_di_buf->type == VFRAME_TYPE_IN ||
		    ready_di_buf->post_proc_flag < 0 ||
		    dimp_get(edi_mp_bypass_post_state)
		    ){
			int vframe_process_count = 1;

			if (dimp_get(edi_mp_skip_top_bot) &&
			    (!is_progressive(ready_di_buf->vframe)))
				vframe_process_count = 2;

			if (ready_count >= vframe_process_count) {
				struct di_buf_s *di_buf_i;

//ary 2020-12-09				di_lock_irqfiq_save(irq_flag2);
#ifdef MARK_SC2
		di_buf = di_que_out_to_di_buf(channel, QUE_POST_FREE);
#else
		di_buf = di_que_out_to_di_buf(channel, QUE_PST_NO_BUF);
#endif
				if (dim_check_di_buf(di_buf, 19, channel)) {
//ary 2020-12-09					di_unlock_irqfiq_restore(irq_flag2);
					return 0;
				}

//ary 2020-12-09				di_unlock_irqfiq_restore(irq_flag2);

				i = 0;

		di_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

				for (itmp = 0; itmp < psize; itmp++) {
					p = pw_qindex_2_buf(channel,
							    tmpa[itmp]);
					di_buf->di_buf_dup_p[i++] = p;
					if (i >= vframe_process_count) {
						di_buf->di_buf_dup_p[i] =
							NULL;
						di_buf->di_buf_dup_p[i + 1] =
							NULL;
						break;
					}
				}
				if (i < vframe_process_count) {
					PR_ERR("%s:6\n", __func__);
					if (recovery_flag == 0)
						recovery_log_reason = 22;

					recovery_flag++;
					return 0;
				}

				di_buf_i = di_buf->di_buf_dup_p[0];
				if (!is_progressive(ready_di_buf->vframe) &&
				    ((dimp_get(edi_mp_skip_top_bot) == 1) ||
				    (dimp_get(edi_mp_skip_top_bot) == 2))) {
					unsigned int frame_type =
						di_buf->di_buf_dup_p[1]->
						vframe->type &
						VIDTYPE_TYPEMASK;
					if (dimp_get(edi_mp_skip_top_bot)
						== 1) {
						di_buf_i = (frame_type ==
						VIDTYPE_INTERLACE_TOP)
						? di_buf->di_buf_dup_p[1]
						: di_buf->di_buf_dup_p[0];
					} else if (dimp_get(edi_mp_skip_top_bot)
						   == 2) {
						di_buf_i = (frame_type ==
						VIDTYPE_INTERLACE_BOTTOM)
						? di_buf->di_buf_dup_p[1]
						: di_buf->di_buf_dup_p[0];
					}
				}

				memcpy(di_buf->vframe, di_buf_i->vframe,
				       sizeof(vframe_t));
				if (di_buf->local_meta &&
				    di_buf_i->local_meta &&
				    di_buf_i->local_meta_used_size) {
					memset(di_buf->local_meta, 0,
						di_buf->local_meta_total_size);
					memcpy(di_buf->local_meta,
						di_buf_i->local_meta,
						di_buf_i->local_meta_used_size * sizeof(u8));
					di_buf->local_meta_used_size =
						di_buf_i->local_meta_used_size;
				} else {
					di_buf->local_meta_used_size = 0;
				}
				if (di_buf->local_ud &&
				    di_buf_i->local_ud &&
				    di_buf_i->local_ud_used_size) {
					memset(di_buf->local_ud, 0,
						di_buf->local_ud_total_size);
					memcpy(di_buf->local_ud,
						di_buf_i->local_ud,
						di_buf_i->local_ud_used_size * sizeof(u8));
					di_buf->local_ud_used_size =
						di_buf_i->local_ud_used_size;
				} else {
					di_buf->local_ud_used_size = 0;
				}
				//ary 0607 di_buf->vframe->width = di_buf_i->width_bk;
				di_buf->dw_width_bk = ready_di_buf->dw_width_bk;
				di_buf->dw_height_bk =
					ready_di_buf->dw_height_bk;
				di_buf->vframe->private_data = di_buf;
				di_buf->is_nbypass = di_buf_i->is_nbypass;
				if (dimp_get(edi_mp_bypass_post_state)) {
					di_buf->is_bypass_pst = 1;
					di_buf_i->pre_ref_count = 0;
					di_buf->process_fun_index = PROCESS_FUN_NULL;
					PR_INF("%s:pst bypass buf[%d:%d]\n", __func__,
						di_buf->type, di_buf->index);
				}
				if (ready_di_buf->new_format_flag &&
				    ready_di_buf->type == VFRAME_TYPE_IN) {
					pr_info("DI:ch[%d],%d disable post.\n",
						channel,
						__LINE__);
					di_buf->vframe->early_process_fun =
						de_post_disable_fun;
				} else {
					if (ready_di_buf->type ==
						VFRAME_TYPE_IN)
						di_buf->vframe->
						early_process_fun =
						do_nothing_fun;

					else
						di_buf->vframe->
						early_process_fun =
						do_pre_only_fun;
				}
				dim_print("%s:2\n", __func__);
				if (di_buf->is_bypass_pst) {
					di_buf->vframe->process_fun =
						NULL;
				} else if (ready_di_buf->post_proc_flag == -2) {
					di_buf->vframe->type |=
						VIDTYPE_VIU_FIELD;
					di_buf->vframe->type &=
						~(VIDTYPE_TYPEMASK);
					di_buf->vframe->process_fun
= (dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support)) ? NULL :
					dim_post_process;
					di_buf->process_fun_index =
						PROCESS_FUN_DI;
					di_buf->pd_config.global_mode =
						PULL_DOWN_EI;
				} else {
					di_buf->vframe->process_fun =
						NULL;
					di_buf->process_fun_index =
							PROCESS_FUN_NULL;
					di_buf->pd_config.global_mode =
						PULL_DOWN_NORMAL;
				}
				di_buf->di_buf[0] = ready_di_buf;
				di_buf->di_buf[1] = NULL;
				queue_out(channel, ready_di_buf);

				tmp = di_buf->di_buf[0]->throw_flag;
				drop_frame(check_drop, tmp, di_buf, channel);

				frame_count++;
#ifdef DI_BUFFER_DEBUG
				dim_print("%s <prog by frame>: ", __func__);
#endif
				ret = 1;
#ifdef MARK_HIS
				pw_vf_notify_receiver
					(channel,
					 VFRAME_EVENT_PROVIDER_VFRAME_READY,
					 NULL);
#endif
			}
		} else if (ready_count >= 2) {
			/*for progressive input,type
			 * 1:separate tow fields,type
			 * 2:bypass post as frame
			 */
			unsigned char prog_tb_field_proc_type =
				(dimp_get(edi_mp_prog_proc_config) >> 1) & 0x3;

			i = 0;

			di_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

			for (itmp = 0; itmp < psize; itmp++) {
				p = pw_qindex_2_buf(channel, tmpa[itmp]);
				//di_buf->di_buf_dup_p[i++] = p;
				tmp_buf[i++] = p;
				if (i >= 2) {
					di_buf->di_buf_dup_p[i] = NULL;
					break;
				}
			}
			if (i < 2) {
				PR_ERR("%s:Error6\n", __func__);

				if (recovery_flag == 0)
					recovery_log_reason = 21;

				recovery_flag++;
				return 0;
			}

			di_buf = tmp_buf[0]->di_buf_post;
			tmp_buf[0]->di_buf_post = NULL;
			for (i = 0; i < 2; i++)
				di_buf->di_buf_dup_p[i] = tmp_buf[i];

			memcpy(di_buf->vframe,
			       di_buf->di_buf_dup_p[0]->vframe,
			       sizeof(vframe_t));
			if (di_buf->local_meta &&
			    di_buf->di_buf_dup_p[0]->local_meta &&
			    di_buf->di_buf_dup_p[0]->local_meta_used_size) {
				memset(di_buf->local_meta, 0,
					di_buf->local_meta_total_size);
				memcpy(di_buf->local_meta,
					di_buf->di_buf_dup_p[0]->local_meta,
					di_buf->di_buf_dup_p[0]->local_meta_used_size * sizeof(u8));
				di_buf->local_meta_used_size =
					di_buf->di_buf_dup_p[0]->local_meta_used_size;
			} else {
				di_buf->local_meta_used_size = 0;
			}
			if (di_buf->local_ud &&
			    di_buf->di_buf_dup_p[0]->local_ud &&
			    di_buf->di_buf_dup_p[0]->local_ud_used_size) {
				memset(di_buf->local_ud, 0,
					di_buf->local_ud_total_size);
				memcpy(di_buf->local_ud,
					di_buf->di_buf_dup_p[0]->local_ud,
					di_buf->di_buf_dup_p[0]->local_ud_used_size * sizeof(u8));
				di_buf->local_ud_used_size =
					di_buf->di_buf_dup_p[0]->local_ud_used_size;
			} else {
				di_buf->local_ud_used_size = 0;
			}

			di_buf->dw_width_bk = ready_di_buf->dw_width_bk;
			di_buf->dw_height_bk = ready_di_buf->dw_height_bk;

			di_buf->vframe->private_data = di_buf;
			di_buf->afbc_sgn_cfg =
				di_buf->di_buf_dup_p[0]->afbc_sgn_cfg;
			/*separate one progressive frame
			 * as two interlace fields
			 */
			if (prog_tb_field_proc_type == 1) {
				/* do weave by di post */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;
				if (di_buf->di_buf_dup_p[0]->new_format_flag)
					di_buf->vframe->early_process_fun =
						de_post_disable_fun;
				else
					di_buf->vframe->early_process_fun =
						do_nothing_fun;

				di_buf->pd_config.global_mode =
					PULL_DOWN_BUF1;
				di_buf->vframe->process_fun =
(dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support)) ? NULL :
				dim_post_process;
				di_buf->process_fun_index = PROCESS_FUN_DI;
			} else if (prog_tb_field_proc_type == 0) {
				/* to do: need change for
				 * DI_USE_FIXED_CANVAS_IDX
				 */
				/* do weave by vpp */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE;
				if (di_buf->di_buf_dup_p[0]->new_format_flag ||
				    (RD(DI_IF1_GEN_REG) & 1))
					di_buf->vframe->early_process_fun =
						de_post_disable_fun;
				else
					di_buf->vframe->early_process_fun =
						do_nothing_fun;
				di_buf->vframe->process_fun = NULL;
				di_buf->process_fun_index = PROCESS_FUN_NULL;
				di_buf->vframe->canvas0Addr =
					di_buf->di_buf_dup_p[0]->nr_canvas_idx;
				di_buf->vframe->canvas1Addr =
					di_buf->di_buf_dup_p[1]->nr_canvas_idx;
			} else {
				/* to do: need change for
				 * DI_USE_FIXED_CANVAS_IDX
				 */
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;
				di_buf->vframe->height >>= 1;

				di_buf->vframe->width =
					di_buf->di_buf_dup_p[0]->width_bk;
				if (di_buf->di_buf_dup_p[0]->new_format_flag ||
				    (RD(DI_IF1_GEN_REG) & 1))
					di_buf->vframe->early_process_fun =
						de_post_disable_fun;
				else
					di_buf->vframe->early_process_fun =
						do_nothing_fun;
				if (prog_tb_field_proc_type == 2) {
					di_buf->vframe->canvas0Addr =
						di_buf->di_buf_dup_p[0]
						->nr_canvas_idx;
/* top */
					di_buf->vframe->canvas1Addr =
						di_buf->di_buf_dup_p[0]
						->nr_canvas_idx;
				} else {
					di_buf->vframe->canvas0Addr =
						di_buf->di_buf_dup_p[1]
						->nr_canvas_idx; /* top */
					di_buf->vframe->canvas1Addr =
						di_buf->di_buf_dup_p[1]
						->nr_canvas_idx;
				}
			}

			di_buf->di_buf[0] = di_buf->di_buf_dup_p[0];
			queue_out(channel, di_buf->di_buf[0]);
			/*check if the field is error,then drop*/
			if ((di_buf->di_buf_dup_p[0]->vframe->type &
			     VIDTYPE_TYPEMASK) ==
			    VIDTYPE_INTERLACE_BOTTOM) {
				di_buf->di_buf[1] =
					di_buf->di_buf_dup_p[1] = NULL;
				queue_in(channel, di_buf, QUEUE_TMP);
				recycle_vframe_type_post(di_buf, channel);
				pr_dbg("%s drop field %d.\n", __func__,
				       di_buf->di_buf_dup_p[0]->seq);
			} else {
				di_buf->di_buf[1] =
					di_buf->di_buf_dup_p[1];
				queue_out(channel, di_buf->di_buf[1]);
				/* dec vf keep */
				if (di_buf->di_buf[1]->in_buf) {
					di_buf->in_buf =
						di_buf->di_buf[1]->in_buf;
					di_buf->di_buf[1]->in_buf = NULL;
					dim_print("dim:dec:p[%d]\n",
						  di_buf->in_buf->index);
				}
				dim_print("%s:bottom ctrl\n", __func__);
				tmp = di_buf->di_buf_dup_p[0]->throw_flag;
				tmp0 = di_buf->di_buf_dup_p[1]->throw_flag;
				drop_frame(dimp_get(edi_mp_check_start_drop),
					   tmp || tmp0, di_buf, channel);
			}
			frame_count++;
#ifdef DI_BUFFER_DEBUG
			dim_print("%s <prog by field>: ", __func__);
#endif
			ret = 1;
#ifdef MARK_HIS
			pw_vf_notify_receiver
				(channel,
				 VFRAME_EVENT_PROVIDER_VFRAME_READY,
				 NULL);
#endif
		}
	}

	return ret;
}

/*
 * di task
 */

void di_unreg_setting(void)
{
	/*unsigned int mirror_disable = get_blackout_policy();*/
	unsigned int mirror_disable = 0;
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (!get_hw_reg_flg()) {
		PR_ERR("%s:have unsetting?do nothing\n", __func__);
		return;
	}

	dbg_pl("%s:\n", __func__);
	sc2_dbg_set(0);
	/*set flg*/
	set_hw_reg_flg(false);
	if (get_datal()->dct_op)
		get_datal()->dct_op->unreg_all();
	if (dim_hdr_ops())
		dim_hdr_ops()->unreg_setting();
	dim_dw_unreg_setting();

	dimh_enable_di_pre_mif(false, dimp_get(edi_mp_mcpre_en));
	post_close_new();	/*2018-11-29*/
	//dimh_afbc_reg_sw(false);
	if (dim_afds())
		dim_afds()->reg_sw(false);
	dimh_hw_uninit();
	if (is_meson_txlx_cpu()	||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()	||
	    DIM_IS_IC(T5)	||
	    DIM_IS_IC(T5DB)	||
	    DIM_IS_IC(T5D)	||
	    is_meson_sm1_cpu()) {
		dim_pre_gate_control(false, dimp_get(edi_mp_mcpre_en));
		get_ops_nr()->nr_gate_control(false);
	} else if (DIM_IS_IC_EF(SC2)) {
		dim_pre_gate_control_sc2(false, dimp_get(edi_mp_mcpre_en));
		get_ops_nr()->nr_gate_control(false);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		DIM_DI_WR(DI_CLKG_CTRL, 0x80f60000);
		DIM_DI_WR(DI_PRE_CTRL, 0);
	} else {
		DIM_DI_WR(DI_CLKG_CTRL, 0xf60000);
	}
	/*ary add for switch to post wr, can't display*/
	dbg_pl("dimh_disable_post_deinterlace_2\n");
	dimh_disable_post_deinterlace_2();
	/* nr/blend0/ei0/mtn0 clock gate */

	dim_hw_disable(dimp_get(edi_mp_mcpre_en));

	if (is_meson_txlx_cpu() ||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()	||
	    DIM_IS_IC(T5)	||
	    DIM_IS_IC(T5DB)	||
	    DIM_IS_IC(T5D)	||
	    is_meson_sm1_cpu()) {
		dimh_enable_di_post_mif(GATE_OFF);
		dim_post_gate_control(false);
		dim_top_gate_control(false, false);
	} else if (DIM_IS_IC_EF(SC2)) {
		dim_post_gate_control_sc2(false);
		dim_top_gate_control_sc2(false, false);
	} else {
		DIM_DI_WR(DI_CLKG_CTRL, 0x80000000);
	}
	if (!is_meson_gxl_cpu()	&&
	    !is_meson_gxm_cpu()	&&
	    !is_meson_gxbb_cpu() &&
	    !is_meson_txlx_cpu())
		diext_clk_b_sw(false);
	dbg_pl("%s disable di mirror image.\n", __func__);

	if ((dimp_get(edi_mp_post_wr_en)	&&
	     dimp_get(edi_mp_post_wr_support))	||
	     mirror_disable) {
		/*diwr_set_power_control(0);*/
		hpst_mem_pd_sw(0);
	}
	if (mirror_disable)
		hpst_vd1_sw(0);
	if (cfgg(HF))
		di_hf_hw_release(0xff);

	get_hw_pre()->pre_top_cfg.d32 = 0;
	get_hw_pst()->last_pst_size = 0;
	disp_frame_count = 0;/* debug only*/

	/*set clkb to low ratio*/
	if (DIM_IS_IC(T5)	||
	    DIM_IS_IC(T5DB)	||
	    DIM_IS_IC(T5D)	||
	    DIM_IS_IC(T3)) {
		#ifdef CLK_TREE_SUPPORT
		if (dimp_get(edi_mp_clock_low_ratio))
			clk_set_rate(de_devp->vpu_clkb,
				     dimp_get(edi_mp_clock_low_ratio));
		#endif
	}
	dbg_pl("%s:end\n", __func__);
}

void di_unreg_variable(unsigned int channel)
{
//ary 2020-12-09	ulong irq_flag2 = 0;
	unsigned int mirror_disable = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct di_ch_s *pch = get_chdata(channel);
	enum EDI_CMA_ST cma_st;
	struct mtsk_cmd_s blk_cmd;
	struct div2_mm_s *mm = dim_mm_get(channel);
	struct di_mtask *tsk = get_mtask();

#if (defined ENABLE_SPIN_LOCK_ALWAYS)
//	ulong flags = 0;
#endif

#if (defined ENABLE_SPIN_LOCK_ALWAYS)
//ary 2020-12-09	spin_lock_irqsave(&plist_lock, flags);
#endif
	dbg_reg("%s:\n", __func__);
	if (get_datal()->dct_op)
		get_datal()->dct_op->unreg(pch);

	set_init_flag(channel, false);	/*init_flag = 0;*/
	pch->itf.op_m_unreg(pch);
	dim_sumx_clear(channel);
	dim_polic_unreg(pch);
	mm->sts.flg_realloc = 0;
	if (ppre->prog_proc_type == 0x10 &&
	    ppre->di_mem_buf_dup_p) {/* set last ref */
		p_ref_set_buf(ppre->di_mem_buf_dup_p, 0, 0, 5);
		pp_buf_clear(ppre->di_mem_buf_dup_p);
		di_que_in(channel, QUE_PRE_NO_BUF, ppre->di_mem_buf_dup_p);
	}

	dim_recycle_post_back(channel);// ?
	/*mirror_disable = get_blackout_policy();*/
	if ((cfgg(KEEP_CLEAR_AUTO) == 2) || dip_itf_is_ins_exbuf(pch))
		mirror_disable = 1;
	else
		mirror_disable = 0;
//ary 2020-12-09	di_lock_irqfiq_save(irq_flag2);
	//dim_print("%s: dim_uninit_buf\n", __func__);
	pch->src_type = 0;
	pch->ponly	= 0;
	dim_uninit_buf(mirror_disable, channel);
	ndrd_reset(pch);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (di_pre_rdma_enable)
		rdma_clear(de_devp->rdma_handle);
#endif
	get_ops_mtn()->adpative_combing_exit();

//ary 2020-12-09	di_unlock_irqfiq_restore(irq_flag2);

#if (defined ENABLE_SPIN_LOCK_ALWAYS)
//	spin_unlock_irqrestore(&plist_lock, flags);
#endif
	dimh_patch_post_update_mc_sw(DI_MC_SW_REG, false);
	sct_sw_off(pch);

	ppre->force_unreg_req_flag = 0;
	ppre->disable_req_flag = 0;
	recovery_flag = 0;
	ppre->cur_prog_flag = 0;
	ppre->is_bypass_fg	= 0;
	cma_st = dip_cma_get_st(channel);
	if ((cfgeq(MEM_FLAG, EDI_MEM_M_CMA)	||
	     cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_A)	||
	     cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_B))) {
		//dip_wq_cma_run(channel, ECMA_CMD_RELEASE);
		//dip_wq_check_unreg(channel);
		//mtsk_release(channel, ECMD_BLK_RELEASE_ALL);
		if (cfgg(MEM_RELEASE_BLOCK_MODE)) {
			mtsk_release_block(channel, ECMD_BLK_RELEASE_ALL);
		} else {
			blk_cmd.cmd = ECMD_BLK_RELEASE_ALL;
			mtask_send_cmd(channel, &blk_cmd);
		}
//		mtsk_release_block(channel, ECMD_BLK_RELEASE_ALL);
		mm->sts.flg_alloced = false;
	}
	pch->sumx.flg_rebuild = false;
	di_hf_t_release(pch);
	sum_g_clear(channel);
	sum_p_clear(channel);
	sum_pst_g_clear(channel);
	sum_pst_p_clear(channel);
	pch->sum_pre = 0;
	pch->sum_pst = 0;
	pch->sum_ext_buf_in = 0;
	pch->sum_ext_buf_in2 = 0;
	pch->in_cnt = 0;
	pch->crc_cnt = 0;
	pch->sumx.need_local = 0;
	set_bypass2_complete(channel, false);
	init_completion(&tsk->fcmd[channel].alloc_done);
	dbg_timer_clear(channel);
	dbg_reg("ndis_used[%d], nout[%d],flg_realloc[%d]\n",
		ndis_cnt(pch, QBF_NDIS_Q_USED),
		ndrd_cnt(pch), mm->sts.flg_realloc);
	dbg_reg("%s:end\n", __func__);
}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
/* di pre rdma operation */
static void di_rdma_irq(void *arg)
{
	struct di_dev_s *di_devp = (struct di_dev_s *)arg;
	unsigned int channel = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (IS_ERR_OR_NULL(di_devp))
		return;
	if (di_devp->rdma_handle <= 0) {
		PR_ERR("%s rdma handle %d error.\n", __func__,
		       di_devp->rdma_handle);
		return;
	}
	if (dimp_get(edi_mp_di_printk_flag))
		pr_dbg("%s...%d.\n", __func__,
		       ppre->field_count_for_cont);
}

static struct rdma_op_s di_rdma_op = {
	di_rdma_irq,
	NULL
};
#endif

void dim_rdma_init(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	struct di_dev_s *de_devp = get_dim_de_devp();
/* rdma handle */
	if (di_pre_rdma_enable) {
		di_rdma_op.arg = de_devp;
		de_devp->rdma_handle = rdma_register(&di_rdma_op,
						     de_devp, RDMA_TABLE_SIZE);
	}

#endif
}

void dim_rdma_exit(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	struct di_dev_s *de_devp = get_dim_de_devp();

	/* rdma handle */
	if (de_devp->rdma_handle > 0)
		rdma_unregister(de_devp->rdma_handle);
#endif
}

static void di_load_pq_table(void)
{
	struct di_pq_parm_s *pos = NULL, *tmp = NULL;
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (de_devp->flags & DI_LOAD_REG_FLAG) {
		if (atomic_read(&de_devp->pq_flag) &&
		    atomic_dec_and_test(&de_devp->pq_flag)) {
			list_for_each_entry_safe(pos, tmp,
						 &de_devp->pq_table_list, list) {
				dimh_load_regs(pos);
				list_del(&pos->list);
				di_pq_parm_destroy(pos);
			}
			de_devp->flags &= ~DI_LOAD_REG_FLAG;
			atomic_set(&de_devp->pq_flag, 1); /* to idle*/
		}
	}
}

static void di_pre_size_change(unsigned short width,
			       unsigned short height,
			       unsigned short vf_type,
			       unsigned int channel)
{
	unsigned int blkhsize = 0;
	int pps_w = 0, pps_h = 0;
	int tmp = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();
	union hw_sc2_ctr_pre_s *sc2_pre_cfg;

	/*pr_info("%s:\n", __func__);*/
	/*debug only:*/
	/*di_pause(channel, true);*/
	get_ops_nr()->nr_all_config(width, height, vf_type);
	#ifdef DET3D
	/*det3d_config*/
	get_ops_3d()->det3d_config(dimp_get(edi_mp_det3d_en) ? 1 : 0);
	#endif
	if (dimp_get(edi_mp_pulldown_enable)) {
		/*pulldown_init(width, height);*/
		get_ops_pd()->init(width, height);
		dimh_init_field_mode(height);

		if (is_meson_txl_cpu()	||
		    is_meson_txlx_cpu() ||
		    is_meson_gxlx_cpu() ||
		    is_meson_txhd_cpu() ||
		    is_meson_g12a_cpu() ||
		    is_meson_g12b_cpu() ||
		    is_meson_tl1_cpu()	||
		    is_meson_tm2_cpu()	||
		    DIM_IS_IC(T5)	||
		    DIM_IS_IC(T5DB)	||
		    DIM_IS_IC(T5D)	||
		    is_meson_sm1_cpu()	||
		    DIM_IS_IC_EF(SC2))
			dim_film_mode_win_config(width, height);
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL))
		dimh_combing_pd22_window_config(width, height);
	DIM_RDMA_WR(DI_PRE_SIZE, (width - 1) |
		((height - 1) << 16));

	if (dimp_get(edi_mp_mcpre_en)) {
		blkhsize = (width + 4) / 5;
		DIM_RDMA_WR(MCDI_HV_SIZEIN, height
			| (width << 16));
		DIM_RDMA_WR(MCDI_HV_BLKSIZEIN, (overturn ? 3 : 0) << 30
			| blkhsize << 16 | height);
		DIM_RDMA_WR(MCDI_BLKTOTAL, blkhsize * height);
		if (is_meson_gxlx_cpu()) {
			DIM_RDMA_WR(MCDI_PD_22_CHK_FLG_CNT, 0);
			DIM_RDMA_WR(MCDI_FIELD_MV, 0);
		}
	}
	if (get_reg_flag_all())
		di_load_pq_table();

	if (de_devp->nrds_enable)
		dim_nr_ds_init(width, height);
	if (de_devp->pps_enable	&& dimp_get(edi_mp_pps_position)) {
		pps_w = ppre->cur_width;
		if (vf_type & VIDTYPE_TYPEMASK) {
			pps_h = ppre->cur_height >> 1;
			dim_pps_config(1, pps_w, pps_h,
				       dimp_get(edi_mp_pps_dstw),
				       (dimp_get(edi_mp_pps_dsth) >> 1));
		} else {
			pps_h = ppre->cur_height;
			dim_pps_config(1, pps_w, pps_h,
				       dimp_get(edi_mp_pps_dstw),
				       (dimp_get(edi_mp_pps_dsth)));
		}
	}
	if (is_meson_sm1_cpu() || is_meson_tm2_cpu()	||
	    DIM_IS_IC(T5)	||
	    DIM_IS_IC(T5DB)	||
	    DIM_IS_IC(T5D)	||
	    DIM_IS_IC_EF(SC2)) {
		if (de_devp->h_sc_down_en) {
			pps_w = ppre->cur_width;
			tmp = di_mp_uit_get(edi_mp_pre_hsc_down_width);
			dim_inp_hsc_setting(pps_w, tmp);
		} else {
			dim_inp_hsc_setting(ppre->cur_width,
					    ppre->cur_width);
		}
	}
	#ifdef MARK_HIS
	dimh_interrupt_ctrl(ppre->madi_enable,
			    det3d_en ? 1 : 0,
			    de_devp->nrds_enable,
			    post_wr_en,
			    ppre->mcdi_enable);
	#else
	/*dimh_int_ctr(0, 0, 0, 0, 0, 0);*/
	dimh_int_ctr(1, ppre->madi_enable,
		     dimp_get(edi_mp_det3d_en) ? 1 : 0,
		     de_devp->nrds_enable,
		     dimp_get(edi_mp_post_wr_en),
		     ppre->mcdi_enable);
	#endif
	if (DIM_IS_IC_EF(SC2) &&
	    ppre->input_size_change_flag &&
	    ppre->di_inp_buf &&
	    ppre->di_inp_buf->vframe) {
		//for crash test if (!ppre->cur_prog_flag)
		dim_pulldown_info_clear_g12a();
		dim_sc2_afbce_rst(0);
		dbg_mem2("pre reset-----\n");

		sc2_pre_cfg = &ppre->pre_top_cfg;//&get_hw_pre()->pre_top_cfg;
		dim_print("%s:cfg[%px]:inp[%d]\n", __func__,
			  sc2_pre_cfg, sc2_pre_cfg->b.afbc_inp);
#ifdef MARK_SC2
		if (sc2_pre_cfg->b.afbc_inp || sc2_pre_cfg->b.afbc_mem)
			sc2_pre_cfg->b.mif_en	= 0; /*ary temp*/
		else
			sc2_pre_cfg->b.mif_en	= 1;
		dim_print("%s:mem_en[%d]\n", __func__,
			  sc2_pre_cfg->b.mif_en);
#endif
		sc2_pre_cfg->b.is_4k	= 0; /*ary temp*/
		sc2_pre_cfg->b.nr_ch0_en	= 1;
		if (dbg_di_prelink_v3())
			sc2_pre_cfg->b.pre_frm_sel = 2;
		else
			sc2_pre_cfg->b.pre_frm_sel = 0;
		//dim_sc2_contr_pre(sc2_pre_cfg);
	}
}

#define DIM_BYPASS_VF_TYPE	(VIDTYPE_MVC | VIDTYPE_VIU_444 | \
				 VIDTYPE_PIC | VIDTYPE_RGB_444)

void dim_vf_x_y(struct vframe_s *vf, unsigned int *x, unsigned int *y)
{
	*x = 0;
	*y = 0;

	if (!vf)
		return;
	*x = vf->width;
	*y = vf->height;

	if (IS_COMP_MODE(vf->type)) {
		*x = vf->compWidth;
		*y = vf->compHeight;
	}
}

static unsigned int dim_bypass_check(struct vframe_s *vf)
{
	unsigned int reason = 0;
	unsigned int x, y;

	if (dimp_get(edi_mp_di_debug_flag) & 0x100000)
		reason = 1;

	if (reason || !vf)
		return reason;

	dim_vf_x_y(vf, &x, &y);
	/* check vf */
	/* for v4l decode vframe */
	if (get_vframe_src_fmt(vf) == VFRAME_SIGNAL_FMT_DOVI ||
	    get_vframe_src_fmt(vf) == VFRAME_SIGNAL_FMT_DOVI_LL) {
		reason = 0xb;
	} else if (vf->type & DIM_BYPASS_VF_TYPE) {
		reason = 2;
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_PPMGR) {
		reason = 6;
	/*support G12A and TXLX platform*/
	} else if (VFMT_IS_I(vf->type)		&&
		   ((vf->width > 1920)	||
		   (vf->height > 1088))) {
		reason = 9;
	} else if (VFMT_IS_P(vf->type) &&
		   (x > default_width	||
		    y > (default_height + 8))) {
		reason = 4;
#ifdef P_NOT_SUPPORT
	} else if (VFMT_IS_P(vf->type)) {
		reason = 8;
#endif//temp bypass p
	/*true bypass for 720p above*/
	} else if ((vf->flag & VFRAME_FLAG_GAME_MODE) &&
		   (vf->width > 720)) {
		reason = 7;
	} else if (vf->flag & VFRAME_FLAG_HIGH_BANDWIDTH) {
		reason = 0xa;
	} else if (vf->type & VIDTYPE_COMPRESS) {
		if (dim_afds() && !dim_afds()->is_supported()) {
			reason = 3;
		} else {
			if ((y > (default_height + 8))	||
			   x > default_width) {
				reason = 5;
			}
		}
	}

	return reason;
}

bool dim_need_bypass(unsigned int ch, struct vframe_s *vf)
{
	unsigned int reason = 0;
	struct di_ch_s *pch = get_chdata(ch);

	reason = dim_bypass_check(vf);

	if (reason) {
		dim_bypass_set(pch, 0, reason);
		return true;
	}

	reason = dim_polic_is_bypass(pch, vf);
	dim_bypass_set(pch, 0, reason);
	if (reason)
		return true;

	return false;
}

/*********************************
 *
 * setting register only call when
 * from di_reg_process_irq
 *********************************/
void di_reg_setting(unsigned int channel, struct vframe_s *vframe)
{
	unsigned short nr_height = 0, first_field_type;
	struct di_dev_s *de_devp = get_dim_de_devp();
	unsigned int x, y;

	dbg_pl("%s:ch[%d]:for first ch reg:\n", __func__, channel);

	if (get_hw_reg_flg()) {
		PR_ERR("%s:have setting?do nothing\n", __func__);
		return;
	}
	/*set flg*/
	set_hw_reg_flg(true);

	diext_clk_b_sw(true);

	dim_ddbg_mod_save(EDI_DBG_MOD_REGB, channel, 0);

	if (dimp_get(edi_mp_post_wr_en)	&&
	    dimp_get(edi_mp_post_wr_support))
		dim_set_power_control(1);

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		/*if (!use_2_interlace_buff) {*/
		if (1) {
			if (DIM_IS_IC_EF(SC2))
				dim_top_gate_control_sc2(true, true);
			else
				dim_top_gate_control(true, true);
			/*dim_post_gate_control(true);*/
			/* freerun for reg configuration */
			/*dimh_enable_di_post_mif(GATE_AUTO);*/
			hpst_power_ctr(true);
		} else {
			dim_top_gate_control(true, false);
		}
		de_devp->flags |= DI_VPU_CLKB_SET;
		/*set clkb to max */
		if (is_meson_g12a_cpu()	||
		    is_meson_g12b_cpu()	||
		    is_meson_tl1_cpu()	||
		    is_meson_tm2_cpu()	||
		    DIM_IS_IC(T5)	||
		    DIM_IS_IC(T5DB)	||
		    DIM_IS_IC(T5D)	||
		    is_meson_sm1_cpu()	||
		    DIM_IS_IC_EF(SC2)) {
			#ifdef CLK_TREE_SUPPORT
			clk_set_rate(de_devp->vpu_clkb,
				     de_devp->clkb_max_rate);
			#endif
		}

		dimh_enable_di_pre_mif(false, dimp_get(edi_mp_mcpre_en));
		if (DIM_IS_IC_EF(SC2))
			dim_pre_gate_control_sc2(true,
						 dimp_get(edi_mp_mcpre_en));
		else
			dim_pre_gate_control(true, dimp_get(edi_mp_mcpre_en));
		dim_pre_gate_control(true, dimp_get(edi_mp_mcpre_en));

		/*2019-01-22 by VLSI feng.wang*/
		//
		if (DIM_IS_IC_EF(SC2)) {
			if (opl1()->wr_rst_protect)
				opl1()->wr_rst_protect(true);
		} else {
			dim_rst_protect(true);
		}

		dim_pre_nr_wr_done_sel(true);
		get_ops_nr()->nr_gate_control(true);
	} else {
		/* if mcdi enable DI_CLKG_CTRL should be 0xfef60000 */
		DIM_DI_WR(DI_CLKG_CTRL, 0xfef60001);
		/* nr/blend0/ei0/mtn0 clock gate */
	}
	/*--------------------------*/
	dim_init_setting_once();
	/*--------------------------*/
	/*di_post_reset();*/ /*add by feijun 2018-11-19 */
	if (DIM_IS_IC_EF(SC2)) {
		opl1()->pst_mif_sw(false, DI_MIF0_SEL_PST_ALL);
		opl1()->pst_dbg_contr();
	} else {
		post_mif_sw(false);
		post_dbg_contr();
	}

	/*--------------------------*/
	dim_vf_x_y(vframe, &x, &y);
	nr_height = (unsigned short)y;
	if (IS_I_SRC(vframe->type))
		nr_height = (nr_height >> 1);/*temp*/
	dbg_reg("%s:0x%x:%d,%d,%d\n", __func__, vframe->type, x, y, nr_height);
	/*--------------------------*/
	dimh_calc_lmv_init();
	first_field_type = (vframe->type & VIDTYPE_TYPEMASK);
	di_pre_size_change(vframe->width, nr_height,
			   first_field_type, channel);
	get_ops_nr()->cue_int(vframe);
	dim_ddbg_mod_save(EDI_DBG_MOD_REGE, channel, 0);
	if (dim_hdr_ops())
		dim_hdr_ops()->init();
	/*--------------------------*/
	/*test*/
	/*if (dim_config_crc_ic()) {//add for crc @2k22-0102
		ma_di_init();
		if (DIM_IS_REV(SC2, MAJOR))
			mc_blend_sc2_init();
	}
	*/
	dimh_int_ctr(0, 0, 0, 0, 0, 0);
	sc2_dbg_set(DI_BIT0 | DI_BIT1);
}

void di_reg_setting_working(struct di_ch_s *pch,
			    struct vframe_s *vfm)
{
	/****************************/
	if (DIM_IS_IC(T5DB))
		dim_afds()->reg_sw(true);
}
/*********************************
 *
 * setting variable
 * from di_reg_process_irq
 *
 *********************************/
void di_reg_variable(unsigned int channel, struct vframe_s *vframe)
{
//ary 2020-12-09	ulong irq_flag2 = 0;
	#ifndef	RUN_DI_PROCESS_IN_IRQ
//ary 2020-12-09	ulong flags = 0;
	#endif
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct di_ch_s *pch;
	struct div2_mm_s *mm;

#ifdef HIS_CODE
	if (pre_run_flag != DI_RUN_FLAG_RUN &&
	    pre_run_flag != DI_RUN_FLAG_STEP)
		return;
	if (pre_run_flag == DI_RUN_FLAG_STEP)
		pre_run_flag = DI_RUN_FLAG_STEP_DONE;
#endif
	dbg_reg("%s:=%x\n", __func__, channel);
	dim_slt_init();

	dim_print("%s:0x%p\n", __func__, vframe);
	pch = get_chdata(channel);
	mm = dim_mm_get(channel);
	if (vframe) {
//		if (DIM_IS_IC_EF(SC2) && dim_afds())
//			di_set_default();
		dip_init_value_reg(channel, vframe);/*add 0404 for post*/
		dim_ddbg_mod_save(EDI_DBG_MOD_RVB, channel, 0);
		//if (!dip_itf_is_ins_exbuf(pch)) {
		if (dip_itf_is_vfm(pch)) {
			if (dim_need_bypass(channel, vframe)) {
				if (!ppre->bypass_flag) {
					PR_INF("%ux%u-0x%x.\n",
						vframe->width,
						vframe->height,
						vframe->type);
				}
				ppre->bypass_flag = true;
				ppre->sgn_lv	= EDI_SGN_OTHER;
				dimh_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
				return;
			}
		}
		dim_mp_update_reg();
		ppre->bypass_flag = false;
		/* patch for vdin progressive input */
		if ((is_from_vdin(vframe) && is_progressive(vframe)) ||
		    #ifdef DET3D
		    dimp_get(edi_mp_det3d_en) ||
		    #endif
		    (dimp_get(edi_mp_use_2_interlace_buff) & 0x2)) {
			dimp_set(edi_mp_use_2_interlace_buff, 1);
		} else {
			dimp_set(edi_mp_use_2_interlace_buff, 0);
		}
		de_devp->nrds_enable = dimp_get(edi_mp_nrds_en);
		de_devp->pps_enable = dimp_get(edi_mp_pps_en);
		/*di pre h scaling down: sm1 tm2*/
		de_devp->h_sc_down_en = di_mp_uit_get(edi_mp_pre_hsc_down_en);

		if (dimp_get(edi_mp_di_printk_flag) & 2)
			dimp_set(edi_mp_di_printk_flag, 1);

//		dim_print("%s: vframe come => di_init_buf\n", __func__);

		if (cfgeq(MEM_FLAG, EDI_MEM_M_REV) && !de_devp->mem_flg)
			dim_rev_mem_check();

		/*need before set buffer*/
		if (dim_afds())
			dim_afds()->reg_val(pch);
		if (get_datal()->dct_op)
			get_datal()->dct_op->reg(pch);

		/*
		 * 10 bit mode need 1.5 times buffer size of
		 * 8 bit mode, init the buffer size as 10 bit
		 * mode size, to make sure can switch bit mode
		 * smoothly.
		 */

		//di_init_buf(mm->cfg.di_w, mm->cfg.di_h,
		//	      is_progressive(vframe), channel);
		di_init_buf_new(pch, vframe);

		if (mm->cfg.pbuf_flg.b.typ == EDIM_BLK_TYP_PSCT) {
			if (mm->cfg.num_post)
				sct_sw_on(pch,
					mm->cfg.num_post,
					mm->cfg.pbuf_flg.b.tvp,
					mm->cfg.pst_buf_size);
			else
				PR_WARN("post is 0, reg no alloc\n");
		}
		pre_sec_alloc(pch, mm->cfg.dat_idat_flg.d32);
		pst_sec_alloc(pch, mm->cfg.dat_pafbct_flg.d32);
		ppre->mtn_status =
			get_ops_mtn()->adpative_combing_config
				(vframe->width,
				 (vframe->height >> 1),
				 (vframe->source_type),
				 is_progressive(vframe),
				 vframe->sig_fmt);

		dimh_patch_post_update_mc_sw(DI_MC_SW_REG, true);
		di_sum_reg_init(channel);
		#ifdef MARK_HIS //must before init buffer
		if (dim_afds())
			dim_afds()->reg_val(pch);
		#endif
#ifdef HIS_CODE	//move to front
		if (get_datal()->dct_op)
			get_datal()->dct_op->reg(pch);
#endif
		dcntr_reg(1);

		set_init_flag(channel, true);/*init_flag = 1;*/

		dim_ddbg_mod_save(EDI_DBG_MOD_RVE, channel, 0);
	}
}

/*para 1 not use*/

/*
 * provider/receiver interface
 */

/*************************/
void di_block_set(int val)
{
	di_blocking = val;
}

int di_block_get(void)
{
	return di_blocking;
}

void dim_recycle_post_back(unsigned int channel)
{
	struct di_buf_s *di_buf = NULL;
//ary 2020-12-09	ulong irq_flag2 = 0;
	unsigned int i = 0, len;

	static unsigned long add_last;
	unsigned long addr;

	if (di_que_is_empty(channel, QUE_POST_BACK))
		return;
	len = di_que_list_count(channel, QUE_POST_BACK);
	while (i < len) {
		i++;

		di_buf = di_que_peek(channel, QUE_POST_BACK);
		/*pr_info("dp2:%d\n", post_buf_index);*/
		if (!di_buf)
			break;

		if (di_buf->type != VFRAME_TYPE_POST) {
			queue_out(channel, di_buf);
			PR_ERR("%s:type is not post\n", __func__);
			continue;
		}
		if (di_buf->blk_buf &&
		    atomic_read(&di_buf->blk_buf->p_ref_mem)) {
			addr = (unsigned long)di_buf->blk_buf;
			if (add_last != addr) {
				dbg_mem("is ref:0x%lx %d\n",
					addr, di_buf->blk_buf->header.index);
				add_last = addr;
			}
			queue_out(channel, di_buf);
			di_que_in(channel, QUE_POST_BACK, di_buf);
			continue;
		}

		dim_print("di_back:%d\n", di_buf->index);
		if (di_buf->blk_buf)
			dim_print("di_back:blk:%d\n",
				  di_buf->blk_buf->header.index);

		/*dec vf keep*/
		if (di_buf->in_buf) {
			dim_print("dim:dec vf:b:p[%d],i[%d]\n",
				  di_buf->index, di_buf->in_buf->index);
			queue_in(channel, di_buf->in_buf, QUEUE_RECYCLE);
			di_buf->in_buf = NULL;
		}
		/*queue_out(channel, di_buf);*/

		if (!atomic_dec_and_test(&di_buf->di_cnt))
			PR_ERR("%s,di_cnt > 0\n", __func__);

		recycle_vframe_type_post(di_buf, channel);
		//di_buf->invert_top_bot_flag = 0;
		//di_que_in(channel, QUE_POST_FREE, di_buf);

//ary 2020-12-09		di_unlock_irqfiq_restore(irq_flag2);
	}

	if (di_cfg_top_get(EDI_CFG_KEEP_CLEAR_AUTO))
		;//dim_post_keep_release_all_2free(channel);
}

/**********************************************/

/*****************************
 *	 di driver file_operations
 *
 ******************************/

static struct di_pq_parm_s *di_pq_parm_create(struct am_pq_parm_s *pq_parm_p)
{
	struct di_pq_parm_s *pq_ptr = NULL;
	struct am_reg_s *am_reg_p = NULL;
	size_t mem_size = 0;

	pq_ptr = vzalloc(sizeof(*pq_ptr));
	mem_size = sizeof(struct am_pq_parm_s);
	memcpy(&pq_ptr->pq_parm, pq_parm_p, mem_size);
	mem_size = sizeof(struct am_reg_s) * pq_parm_p->table_len;
	am_reg_p = vzalloc(mem_size);
	if (!am_reg_p) {
		vfree(pq_ptr);
		PR_ERR("alloc pq table memory errors\n");
		return NULL;
	}
	pq_ptr->regs = am_reg_p;

	return pq_ptr;
}

static void di_pq_parm_destroy(struct di_pq_parm_s *pq_ptr)
{
	if (!pq_ptr) {
		PR_ERR("%s pq parm pointer null.\n", __func__);
		return;
	}
	vfree(pq_ptr->regs);
	vfree(pq_ptr);
}

/*move from ioctrl*/
long dim_pq_load_io(unsigned long arg)
{
	long ret = 0, tab_flag = 0;
	di_dev_t *di_devp;
	void __user *argp = (void __user *)arg;
	size_t mm_size = 0;
	struct am_pq_parm_s tmp_pq_s = {0};
	struct di_pq_parm_s *di_pq_ptr = NULL;
	struct di_dev_s *de_devp = get_dim_de_devp();
	/*unsigned int channel = 0;*/	/*fix to channel 0*/

	di_devp = de_devp;
	/* check io busy or not*/
	if (atomic_read(&de_devp->pq_io) < 1 ||
	    !atomic_dec_and_test(&de_devp->pq_io)) {
		PR_ERR("%s:busy, do nothing\n", __func__);
		return -EFAULT;
	}
	mm_size = sizeof(struct am_pq_parm_s);
	if (copy_from_user(&tmp_pq_s, argp, mm_size)) {
		PR_ERR("set pq parm errors\n");
		atomic_set(&de_devp->pq_io, 1); /* idle */
		return -EFAULT;
	}
	if (tmp_pq_s.table_len >= DIMTABLE_LEN_MAX) {
		PR_ERR("load 0x%x wrong pq table_len.\n",
		       tmp_pq_s.table_len);
		atomic_set(&de_devp->pq_io, 1); /* idle */
		return -EFAULT;
	}
	tab_flag = TABLE_NAME_DI | TABLE_NAME_NR | TABLE_NAME_MCDI |
		TABLE_NAME_DEBLOCK | TABLE_NAME_DEMOSQUITO;

	tab_flag |= TABLE_NAME_SMOOTHPLUS;
	if (tmp_pq_s.table_name & tab_flag) {
		PR_INF("load 0x%x pq len %u %s.\n",
		       tmp_pq_s.table_name, tmp_pq_s.table_len,
		       (!dim_dbg_is_force_later() && get_reg_flag_all()) ? "directly" : "later");
	} else {
		PR_ERR("load 0x%x wrong pq table.\n",
		       tmp_pq_s.table_name);
		atomic_set(&de_devp->pq_io, 1); /* idle */
		return -EFAULT;
	}
	di_pq_ptr = di_pq_parm_create(&tmp_pq_s);
	if (!di_pq_ptr) {
		PR_ERR("allocat pq parm struct error.\n");
		atomic_set(&de_devp->pq_io, 1); /* idle */
		return -EFAULT;
	}
	argp = (void __user *)tmp_pq_s.table_ptr;
	mm_size = tmp_pq_s.table_len * sizeof(struct am_reg_s);
	if (copy_from_user(di_pq_ptr->regs, argp, mm_size)) {
		PR_ERR("user copy pq table errors\n");
		atomic_set(&de_devp->pq_io, 1); /* idle */
		return -EFAULT;
	}
	if (!dim_dbg_is_force_later() &&  get_reg_flag_all()) {
		dimh_load_regs(di_pq_ptr);
		di_pq_parm_destroy(di_pq_ptr);
		atomic_set(&de_devp->pq_io, 1); /* idle */
		return ret;
	}
	if (atomic_read(&de_devp->pq_flag) &&
	    atomic_dec_and_test(&de_devp->pq_flag)) {
		if (di_devp->flags & DI_LOAD_REG_FLAG) {
			struct di_pq_parm_s *pos = NULL, *tmp = NULL;

			list_for_each_entry_safe(pos, tmp,
						 &di_devp->pq_table_list,
						 list) {
				if (di_pq_ptr->pq_parm.table_name ==
				    pos->pq_parm.table_name) {
					PR_INF("remove 0x%x table.\n",
					       pos->pq_parm.table_name);
					list_del(&pos->list);
					di_pq_parm_destroy(pos);
				}
			}
		}
		list_add_tail(&di_pq_ptr->list,
			      &di_devp->pq_table_list);
		di_devp->flags |= DI_LOAD_REG_FLAG;
		atomic_set(&de_devp->pq_flag, 1);/* to idle */
	} else {
		PR_ERR("please retry table name 0x%x.\n",
		       di_pq_ptr->pq_parm.table_name);
		di_pq_parm_destroy(di_pq_ptr);
		ret = -EFAULT;
	}
	atomic_set(&de_devp->pq_io, 1); /* idle */
	return ret;
}

unsigned int rd_reg_bits(unsigned int adr, unsigned int start,
			 unsigned int len)
{
	return ((aml_read_vcbus(adr) &
		(((1UL << (len)) - 1UL) << (start))) >> (start));
}

unsigned int DIM_RDMA_RD_BITS(unsigned int adr, unsigned int start,
			      unsigned int len)
{
	return rd_reg_bits(adr, start, len);
}

unsigned int DIM_RDMA_WR(unsigned int adr, unsigned int val)
{
	DIM_DI_WR(adr, val);
	return 1;
}

unsigned int DIM_RDMA_RD(unsigned int adr)
{
	return RD(adr);
}

unsigned int DIM_RDMA_WR_BITS(unsigned int adr, unsigned int val,
			      unsigned int start, unsigned int len)
{
	DIM_DI_WR_REG_BITS(adr, val, start, len);
	return 1;
}

void dim_set_di_flag(void)
{
	if (is_meson_txl_cpu()	||
	    is_meson_txlx_cpu()	||
	    is_meson_gxlx_cpu() ||
	    is_meson_txhd_cpu() ||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu() ||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()	||
	    DIM_IS_IC(T5)	||
	    DIM_IS_IC(T5DB)	||
	    DIM_IS_IC(T5D)	||
	    is_meson_sm1_cpu()	||
	    DIM_IS_IC_EF(SC2)) {
		dimp_set(edi_mp_mcpre_en, 1);
		mc_mem_alloc = true;
		dimp_set(edi_mp_pulldown_enable, 0);
		di_pre_rdma_enable = false;
		/*
		 * txlx atsc 1080i ei only will cause flicker
		 * when full to small win in home screen
		 */

		dimp_set(edi_mp_di_vscale_skip_enable,
			 (is_meson_txlx_cpu()	||
			 is_meson_txhd_cpu()) ? 12 : 4);
		/*use_2_interlace_buff = is_meson_gxlx_cpu()?0:1;*/
		dimp_set(edi_mp_use_2_interlace_buff,
			 is_meson_gxlx_cpu() ? 0 : 1);
		if (is_meson_txl_cpu()	||
		    is_meson_txlx_cpu()	||
		    is_meson_gxlx_cpu() ||
		    is_meson_txhd_cpu() ||
		    is_meson_g12a_cpu() ||
		    is_meson_g12b_cpu() ||
		    is_meson_tl1_cpu()	||
		    is_meson_tm2_cpu()	||
		    DIM_IS_IC(T5)	||
		    DIM_IS_IC(T5DB)	||
		    DIM_IS_IC(T5D)	||
		    is_meson_sm1_cpu()	||
		    DIM_IS_IC_EF(SC2)) {
			dimp_set(edi_mp_full_422_pack, 1);
		}

		if (dimp_get(edi_mp_nr10bit_support)) {
			dimp_set(edi_mp_di_force_bit_mode, 10);
		} else {
			dimp_set(edi_mp_di_force_bit_mode, 8);
			dimp_set(edi_mp_full_422_pack, 0);
		}

		dimp_set(edi_mp_post_hold_line,
			 (is_meson_g12a_cpu()	||
			 is_meson_g12b_cpu()	||
			 is_meson_tl1_cpu()	||
			 is_meson_tm2_cpu()	||
			 DIM_IS_IC(T5)		||
			 DIM_IS_IC(T5DB)	||
			 DIM_IS_IC(T5D)		||
			 is_meson_sm1_cpu()	||
			 DIM_IS_IC_EF(SC2)) ? 10 : 17);
	} else {
		/*post_hold_line = 8;*/	/*2019-01-10: from VLSI feijun*/
		dimp_set(edi_mp_post_hold_line, 8);
		dimp_set(edi_mp_mcpre_en, 0);
		dimp_set(edi_mp_pulldown_enable, 0);
		di_pre_rdma_enable = false;
		dimp_set(edi_mp_di_vscale_skip_enable, 4);
		dimp_set(edi_mp_use_2_interlace_buff, 0);
		dimp_set(edi_mp_di_force_bit_mode, 8);
	}
	/*if (is_meson_tl1_cpu() || is_meson_tm2_cpu())*/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		dimp_set(edi_mp_pulldown_enable, 1);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		intr_mode = 3;
	if (di_pre_rdma_enable) {
		pldn_dly = 1;
		pldn_dly1 = 1;
	} else {
		pldn_dly = 2;
		pldn_dly1 = 2;
	}

	if (DIM_IS_IC(T5)	||
	    DIM_IS_IC(TM2B)	||
	    DIM_IS_IC(T5DB)	||
	    DIM_IS_IC(T5D)	||
	    DIM_IS_IC(T7) ||
	    DIM_IS_IC(T3))//s4/sc2 box bypass nr from brian
		di_cfg_set(ECFG_DIM_BYPASS_P, 0);//for t5 enable p

	get_ops_mtn()->mtn_int_combing_glbmot();
}

#ifdef MARK_HIS	/*move to di_sys.c*/
static const struct reserved_mem_ops rmem_di_ops = {
	.device_init	= rmem_di_device_init,
	.device_release = rmem_di_device_release,
};

static int __init rmem_di_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_di_ops;
/* rmem->priv = cma; */

	di_pr_info("DI reserved mem:creat CMA mem pool at %pa, size %ld MiB\n",
		   &rmem->base, (unsigned long)rmem->size / SZ_1M);

	return 0;
}

RESERVEDMEM_OF_DECLARE(di, "amlogic, di-mem", rmem_di_setup);
#endif

void dim_get_vpu_clkb(struct device *dev, struct di_dev_s *pdev)
{
	int ret = 0;
	unsigned int tmp_clk[2] = {0, 0};
	struct clk *vpu_clk = NULL;

	if (DIM_IS_IC_EF(SC2))
		vpu_clk = clk_get(dev, "vpu_mux");
	else if (DIM_IS_IC(T5)		||
		 DIM_IS_IC(T5DB)	||
		 DIM_IS_IC(T5D))
		vpu_clk = clk_get(dev, "t5_vpu_clkb_tmp_gate");
	else
		vpu_clk = clk_get(dev, "vpu_mux");
	if (IS_ERR(vpu_clk))
		PR_ERR("%s: get clk vpu error.\n", __func__);
	else
		clk_prepare_enable(vpu_clk);

	ret = of_property_read_u32_array(dev->of_node, "clock-range",
					 tmp_clk, 2);
	if (ret) {
		pdev->clkb_min_rate = 250000000;
		pdev->clkb_max_rate = 500000000;
	} else {
		pdev->clkb_min_rate = tmp_clk[0] * 1000000;
		pdev->clkb_max_rate = tmp_clk[1] * 1000000;
	}
	PR_INF("vpu clkb <%lu, %lu>\n", pdev->clkb_min_rate,
	       pdev->clkb_max_rate);
	#ifdef CLK_TREE_SUPPORT
	if (DIM_IS_IC_EF(SC2))
		pdev->vpu_clkb = clk_get(dev, "vpu_clkb");
	else if (DIM_IS_IC(T5)		||
		 DIM_IS_IC(T5DB)	||
		 DIM_IS_IC(T5D))
		pdev->vpu_clkb = clk_get(dev, "t5_vpu_clkb_gate");
	else
		pdev->vpu_clkb = clk_get(dev, "vpu_clkb");

	if (IS_ERR(pdev->vpu_clkb))
		PR_ERR("%s: get vpu clkb gate error.\n", __func__);
	#endif
}

module_param_named(invert_top_bot, invert_top_bot, int, 0664);

#ifdef DET3D

MODULE_PARM_DESC(det3d_mode, "\n det3d_mode\n");
module_param(det3d_mode, uint, 0664);
#endif

module_param_array(di_stop_reg_addr, uint, &num_di_stop_reg_addr,
		   0664);

module_param_named(overturn, overturn, bool, 0664);

#ifdef DEBUG_SUPPORT
#ifdef RUN_DI_PROCESS_IN_IRQ
module_param_named(input2pre, input2pre, uint, 0664);
module_param_named(input2pre_buf_miss_count, input2pre_buf_miss_count,
		   uint, 0664);
module_param_named(input2pre_proc_miss_count, input2pre_proc_miss_count,
		   uint, 0664);
module_param_named(input2pre_miss_policy, input2pre_miss_policy, uint, 0664);
module_param_named(input2pre_throw_count, input2pre_throw_count, uint, 0664);
#endif
#ifdef SUPPORT_MPEG_TO_VDIN
module_param_named(mpeg2vdin_en, mpeg2vdin_en, int, 0664);
module_param_named(mpeg2vdin_flag, mpeg2vdin_flag, int, 0664);
#endif
module_param_named(di_pre_rdma_enable, di_pre_rdma_enable, uint, 0664);
module_param_named(pldn_dly, pldn_dly, uint, 0644);
module_param_named(pldn_dly1, pldn_dly1, uint, 0644);
module_param_named(di_reg_unreg_cnt, di_reg_unreg_cnt, int, 0664);
module_param_named(bypass_pre, bypass_pre, int, 0664);
module_param_named(frame_count, frame_count, int, 0664);
#endif

int dim_seq_file_module_para_di(struct seq_file *seq)
{
	seq_puts(seq, "di---------------\n");

#ifdef DET3D
	seq_printf(seq, "%-15s:%d\n", "det3d_frame_cnt", det3d_frame_cnt);
#endif
	seq_printf(seq, "%-15s:%ld\n", "same_field_top_count",
		   same_field_top_count);
	seq_printf(seq, "%-15s:%ld\n", "same_field_bot_count",
		   same_field_bot_count);

	seq_printf(seq, "%-15s:%d\n", "overturn", overturn);

#ifdef DEBUG_SUPPORT
#ifdef RUN_DI_PROCESS_IN_IRQ
	seq_printf(seq, "%-15s:%d\n", "input2pre", input2pre);
	seq_printf(seq, "%-15s:%d\n", "input2pre_buf_miss_count",
		   input2pre_buf_miss_count);
	seq_printf(seq, "%-15s:%d\n", "input2pre_proc_miss_count",
		   input2pre_proc_miss_count);
	seq_printf(seq, "%-15s:%d\n", "input2pre_miss_policy",
		   input2pre_miss_policy);
	seq_printf(seq, "%-15s:%d\n", "input2pre_throw_count",
		   input2pre_throw_count);
#endif
#ifdef SUPPORT_MPEG_TO_VDIN

	seq_printf(seq, "%-15s:%d\n", "mpeg2vdin_en", mpeg2vdin_en);
	seq_printf(seq, "%-15s:%d\n", "mpeg2vdin_flag", mpeg2vdin_flag);
#endif
	seq_printf(seq, "%-15s:%d\n", "di_pre_rdma_enable",
		   di_pre_rdma_enable);
	seq_printf(seq, "%-15s:%d\n", "pldn_dly", pldn_dly);
	seq_printf(seq, "%-15s:%d\n", "pldn_dly1", pldn_dly1);
	seq_printf(seq, "%-15s:%d\n", "di_reg_unreg_cnt", di_reg_unreg_cnt);
	seq_printf(seq, "%-15s:%d\n", "bypass_pre", bypass_pre);
	seq_printf(seq, "%-15s:%d\n", "frame_count", frame_count);
#endif
/******************************/

#ifdef DET3D
	seq_printf(seq, "%-15s:%d\n", "det3d_mode", det3d_mode);
#endif
	return 0;
}

#ifdef MARK_HIS /*move to di_sys.c*/
//MODULE_DESCRIPTION("AMLOGIC DEINTERLACE driver");
//MODULE_LICENSE("GPL");
//MODULE_VERSION("4.0.0");
#endif
