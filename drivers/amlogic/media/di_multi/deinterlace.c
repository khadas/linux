// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/amlogic/cpu_version.h>
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
#include "register.h"
#include "deinterlace.h"
#include "deinterlace_dbg.h"
#include "nr_downscale.h"

#include "di_data_l.h"
#include "di_dbg.h"
#include "di_pps.h"
#include "di_pre.h"
#include "di_prc.h"
#include "di_task.h"
#include "di_vframe.h"
#include "di_que.h"
#include "di_api.h"
#include "di_sys.h"

/*2018-07-18 add debugfs*/
#include <linux/seq_file.h>
#include <linux/debugfs.h>
/*2018-07-18 -----------*/

#ifdef DET3D
#include "detect3d.h"
#endif
#define ENABLE_SPIN_LOCK_ALWAYS

static DEFINE_SPINLOCK(di_lock2);

#define di_lock_irqfiq_save(irq_flag) \
	spin_lock_irqsave(&di_lock2, irq_flag)

#define di_unlock_irqfiq_restore(irq_flag) \
	spin_unlock_irqrestore(&di_lock2, irq_flag)

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

/**************************************
 *
 *
 *************************************/
unsigned int di_dbg = DBG_M_EVENT;
module_param(di_dbg, uint, 0664);
MODULE_PARM_DESC(di_dbg, "debug print");

/* destroy unnecessary frames before display */
static unsigned int hold_video;

DEFINE_SPINLOCK(plist_lock);

static const char version_s[] = "2019-04-25ma";

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

static unsigned int unreg_cnt;/*cnt for vframe unreg*/
static unsigned int reg_cnt;/*cnt for vframe reg*/

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
static void dump_state(unsigned int channel);
static void recycle_keep_buffer(unsigned int channel);

#define DI_PRE_INTERVAL         (HZ / 100)

/*
 * progressive frame process type config:
 * 0, process by field;
 * 1, process by frame (only valid for vdin source whose
 * width/height does not change)
 */

static struct di_buf_s *cur_post_ready_di_buf;

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

void DIM_VSC_WR_MPG_BT(unsigned int addr, unsigned int val,
		       unsigned int start, unsigned int len)
{
	if (is_need_stop_reg(addr))
		return;
	if (dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support))
		DIM_DI_WR_REG_BITS(addr, val, start, len);
	else
		VSYNC_WR_MPEG_REG_BITS(addr, val, start, len);
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
static int dump_state_flag;

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
		dump_state(channel);
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
		recycle_keep_buffer(channel);
	} else if (strncmp(buf, "recycle_post", 12) == 0) {
		if (di_vf_l_peek(channel))
			di_vf_l_put(di_vf_l_get(channel), channel);
	} else if (strncmp(buf, "mem_map", 7) == 0) {
		dim_dump_buf_addr(pbuf_local, MAX_LOCAL_BUF_NUM * 2);
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

static unsigned int buf_state_log_start;
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
	struct di_dev_s  *de_devp = get_dim_de_devp();
	/*ary add 2019-07-2 being*/
	unsigned int indx;
	struct di_buf_s *pbuf_post;
	struct di_buf_s *pbuf_local;
	struct di_post_stru_s *ppost;
	struct di_mm_s *mm;
	/*************************/

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
		/*mm-0705	if (indx >= ppost->di_post_num) {*/
		if (indx >= mm->sts.num_post) {
			PR_ERR("c_post:index is overflow:%d[%d]\n", indx,
			       mm->sts.num_post);
			kfree(buf_orig);
			return 0;
		}
		pbuf_post = get_buf_post(channel);
		di_buf = &pbuf_post[indx];
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

		if (!di_que_is_empty(channel, QUE_POST_READY)) {
			di_buf = di_que_peek(channel, QUE_POST_READY);
			pr_info("get post ready di_buf:%d:0x%p\n",
				di_buf->index, di_buf);
		} else {
			pr_info("war:no post ready buf\n");
		}
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
		nr_size = canvas_w * canvas_h * 2;
		dump_adr = di_buf->nr_adr;

		pr_info("w=%d,h=%d,size=%ld,addr=%lu\n",
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
	if (de_devp->flags & DI_MAP_FLAG) {
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

	if (!(de_devp->flags & DI_MAP_FLAG))
		iounmap(buff);
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
	ulong flags = 0, irq_flag2 = 0;
	unsigned int channel = get_current_channel();/* debug only*/
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	set_init_flag(channel, false);/*init_flag = 0;*/
	di_lock_irqfiq_save(irq_flag2);
/* vf_unreg_provider(&di_vf_prov); */
	pw_vf_light_unreg_provider(channel);
	di_unlock_irqfiq_restore(irq_flag2);
	set_reg_flag(channel, false);
	spin_lock_irqsave(&plist_lock, flags);
	di_lock_irqfiq_save(irq_flag2);
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

	di_unlock_irqfiq_restore(irq_flag2);
	spin_unlock_irqrestore(&plist_lock, flags);
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

#define VFRAME_FORMAT_MASK      \
	(VIDTYPE_VIU_422 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_444 | \
	 VIDTYPE_MVC)
	if (ppre->cur_width != vframe->width		||
	    ppre->cur_height != vframe->height		||
	    (((ppre->cur_inp_type & VFRAME_FORMAT_MASK)		!=
	    (vframe->type & VFRAME_FORMAT_MASK))		&&
	    (!is_handle_prog_frame_as_interlace(vframe)))	||
	    ppre->cur_source_type != vframe->source_type) {
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

unsigned char dim_is_bypass(vframe_t *vf_in, unsigned int channel)
{
	unsigned int vtype = 0;
	int ret = 0;
	static vframe_t vf_tmp;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (dimp_get(edi_mp_di_debug_flag) & 0x10000) /* for debugging */
		return (dimp_get(edi_mp_di_debug_flag) >> 17) & 0x1;

	if (di_cfgx_get(channel, EDI_CFGX_BYPASS_ALL))
		return 1;
	if (ppre->cur_prog_flag		&&
	    (ppre->cur_width > 1920	||
	    ppre->cur_height > 1080	||
	    (ppre->cur_inp_type & VIDTYPE_VIU_444))
	    )
		return 1;

	if (ppre->cur_width < 16 || ppre->cur_height < 16)
		return 1;

	if (ppre->cur_inp_type & VIDTYPE_MVC)
		return 1;

	if (ppre->cur_source_type == VFRAME_SOURCE_TYPE_PPMGR)
		return 1;

	if (dimp_get(edi_mp_bypass_trick_mode)) {
		int trick_mode_fffb = 0;
		int trick_mode_i = 0;

		if (dimp_get(edi_mp_bypass_trick_mode) & 0x1)
			query_video_status(0, &trick_mode_fffb);
		if (dimp_get(edi_mp_bypass_trick_mode) & 0x2)
			query_video_status(1, &trick_mode_i);
		trick_mode = trick_mode_fffb | (trick_mode_i << 1);
		if (trick_mode)
			return 1;
	}

	if (dimp_get(edi_mp_bypass_3d)	&&
	    ppre->source_trans_fmt != 0)
		return 1;

/*prot is conflict with di post*/
	if (vf_in && vf_in->video_angle)
		return 1;
	if (vf_in && (vf_in->type & VIDTYPE_PIC))
		return 1;
#ifdef MARK_HIS
	if (vf_in && (vf_in->type & VIDTYPE_COMPRESS))
		return 1;
#endif
	if ((dimp_get(edi_mp_di_vscale_skip_enable) & 0x4) &&
	    vf_in && !dimp_get(edi_mp_post_wr_en)) {
		/*--------------------------*/
		if (vf_in->type & VIDTYPE_COMPRESS) {
			vf_tmp.width = vf_in->compWidth;
			vf_tmp.height = vf_in->compHeight;
			if (vf_tmp.width > 1920 || vf_tmp.height > 1088)
				return 1;
		}
		/*--------------------------*/
		/*backup vtype,set type as progressive*/
		vtype = vf_in->type;
		vf_in->type &= (~VIDTYPE_TYPEMASK);
		vf_in->type &= (~VIDTYPE_VIU_NV21);
		vf_in->type |= VIDTYPE_VIU_SINGLE_PLANE;
		vf_in->type |= VIDTYPE_VIU_FIELD;
		vf_in->type |= VIDTYPE_PRE_INTERLACE;
		vf_in->type |= VIDTYPE_VIU_422;
		ret = ext_ops.get_current_vscale_skip_count(vf_in);
		/*di_vscale_skip_count = (ret&0xff);*/
		dimp_set(edi_mp_di_vscale_skip_count, ret & 0xff);
		/*vpp_3d_mode = ((ret>>8)&0xff);*/
		dimp_set(edi_mp_vpp_3d_mode, ((ret >> 8) & 0xff));

		vf_in->type = vtype;

		if (dimp_get(edi_mp_di_vscale_skip_count) > 0)
			return 1;

		if (dimp_get(edi_mp_vpp_3d_mode)) {
			#ifdef DET3D
			if (!dimp_get(edi_mp_det3d_en))
			#endif
				return 1;
		}
	}

	return 0;
}

static bool need_bypass(struct vframe_s *vf);

/**********************************
 *diff with  dim_is_bypass
 *	delet di_vscale_skip_enable
 *	use vf_in replace ppre
 **********************************/
bool is_bypass2(struct vframe_s *vf_in, unsigned int ch)
{
	/*check debug info*/
	if (dimp_get(edi_mp_di_debug_flag) & 0x10000) /* for debugging */
		return true;

	if (di_cfgx_get(ch, EDI_CFGX_BYPASS_ALL))	/*bypass_all*/
		return true;

	if (dimp_get(edi_mp_bypass_trick_mode)) {
		int trick_mode_fffb = 0;
		int trick_mode_i = 0;

		if (dimp_get(edi_mp_bypass_trick_mode) & 0x1)
			query_video_status(0, &trick_mode_fffb);
		if (dimp_get(edi_mp_bypass_trick_mode) & 0x2)
			query_video_status(1, &trick_mode_i);
		trick_mode = trick_mode_fffb | (trick_mode_i << 1);
		if (trick_mode)
			return true;
	}
	/* check vframe */
	if (!vf_in)
		return false;

	if (need_bypass(vf_in))
		return true;

	if (vf_in->width < 16 || vf_in->height < 16)
		return true;

	if (dimp_get(edi_mp_bypass_3d)	&&
	    vf_in->trans_fmt != 0)
		return true;

/*prot is conflict with di post*/
	if (vf_in->video_angle)
		return true;

	return false;
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

#ifdef RUN_DI_PROCESS_IN_IRQ
static unsigned char is_input2pre(void, unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (input2pre		&&
	    ppre->cur_prog_flag	&&
	    ppre->vdin_source	&&
	    (di_bypass_state_get(channel) == 0))
		return 1;

	return 0;
}
#endif

#ifdef DI_USE_FIXED_CANVAS_IDX
static int di_post_idx[2][6];
static int di_pre_idx[2][10];
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static unsigned int di_inp_idx[3];
#else
static int di_wr_idx;
#endif

int dim_get_canvas(void)
{
	unsigned int pre_num = 7, post_num = 6;
	struct di_dev_s  *de_devp = get_dim_de_devp();

	if (dimp_get(edi_mp_mcpre_en)) {
		/* mem/chan2/nr/mtn/contrd/contrd2/
		 * contw/mcinfrd/mcinfow/mcvecw
		 */
		pre_num = 10;
		/* buf0/buf1/buf2/mtnp/mcvec */
		post_num = 6;
	}
	if (ext_ops.canvas_pool_alloc_canvas_table("di_pre",
						   &di_pre_idx[0][0],
						   pre_num,
						   CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s allocate di pre canvas error.\n", __func__);
		return 1;
	}
	if (di_pre_rdma_enable) {
		if (ext_ops.canvas_pool_alloc_canvas_table("di_pre",
							   &di_pre_idx[1][0],
							   pre_num,
						   CANVAS_MAP_TYPE_1)) {
			PR_ERR("%s allocate di pre canvas error.\n", __func__);
			return 1;
		}
	} else {
		#ifdef MARK_HIS
		for (i = 0; i < pre_num; i++)
			di_pre_idx[1][i] = di_pre_idx[0][i];
		#else
		memcpy(&di_pre_idx[1][0],
		       &di_pre_idx[0][0], sizeof(int) * pre_num);
		#endif
	}
	if (ext_ops.canvas_pool_alloc_canvas_table("di_post",
						   &di_post_idx[0][0],
						   post_num,
						   CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s allocate di post canvas error.\n", __func__);
		return 1;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (ext_ops.canvas_pool_alloc_canvas_table("di_post",
						   &di_post_idx[1][0],
						   post_num,
						   CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s allocate di post canvas error.\n", __func__);
		return 1;
	}
#else
	for (i = 0; i < post_num; i++)
		di_post_idx[1][i] = di_post_idx[0][i];
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (ext_ops.canvas_pool_alloc_canvas_table("di_inp", &di_inp_idx[0], 3,
						   CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s allocat di inp canvas error.\n", __func__);
		return 1;
	}
	pr_info("DI: support multi decoding %u~%u~%u.\n",
		di_inp_idx[0], di_inp_idx[1], di_inp_idx[2]);
#endif
	if (de_devp->post_wr_support == 0)
		return 0;

#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (ext_ops.canvas_pool_alloc_canvas_table("di_wr",
						   &di_wr_idx, 1,
						   CANVAS_MAP_TYPE_1)) {
		PR_ERR("%s allocat di write back canvas error.\n",
		       __func__);
		return 1;
	}
	pr_info("DI: support post write back %u.\n", di_wr_idx);
#endif
	return 0;
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

#else

static void config_canvas(struct di_buf_s *di_buf)
{
	unsigned int height = 0;

	if (!di_buf)
		return;

	if (di_buf->canvas_config_flag == 1) {
		/* linked two interlace buffer should double height*/
		if (di_buf->di_wr_linked_buf)
			height = (di_buf->canvas_height << 1);
		else
			height =  di_buf->canvas_height;
		canvas_config(di_buf->nr_canvas_idx, di_buf->nr_adr,
			      di_buf->canvas_width[NR_CANVAS], height, 0, 0);
		di_buf->canvas_config_flag = 0;
	} else if (di_buf->canvas_config_flag == 2) {
		canvas_config(di_buf->nr_canvas_idx, di_buf->nr_adr,
			      di_buf->canvas_width[MV_CANVAS],
			      di_buf->canvas_height, 0, 0);
		canvas_config(di_buf->mtn_canvas_idx, di_buf->mtn_adr,
			      di_buf->canvas_width[MTN_CANVAS],
			      di_buf->canvas_height, 0, 0);
		di_buf->canvas_config_flag = 0;
	}
}

#endif
//----begin
#ifdef DIM_HIS	/*no use*/
/*******************************************
 *
 *
 ******************************************/
#define DI_KEEP_BUF_SIZE 3
static struct di_buf_s *di_keep_buf[DI_KEEP_BUF_SIZE];
static int di_keep_point;

void keep_buf_clear(void)
{
	int i;

	for (i = 0; i < DI_KEEP_BUF_SIZE; i++)
		di_keep_buf[i] = NULL;

	di_keep_point = -1;
}

void keep_buf_in(struct di_buf_s *ready_buf)
{
	di_keep_point++;
	if (di_keep_point >= DI_KEEP_BUF_SIZE)
		di_keep_point = 0;
	di_keep_buf[di_keep_point] = ready_buf;
}

void keep_buf_in_full(struct di_buf_s *ready_buf)
{
	int i;

	keep_buf_in(ready_buf);
	for (i = 0; i < DI_KEEP_BUF_SIZE; i++) {
		if (!di_keep_buf[i])
			di_keep_buf[i] = ready_buf;
	}
}
#endif

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

static int di_init_buf(int width, int height, unsigned char prog_flag,
		       unsigned int channel)
{
	int i;
	int canvas_height = height + 8;
	struct page *tmp_page = NULL;
	unsigned int di_buf_size = 0, di_post_buf_size = 0, mtn_size = 0;
	unsigned int nr_size = 0, count_size = 0, mv_size = 0, mc_size = 0;
	unsigned int nr_width = width, mtn_width = width, mv_width = width;
	unsigned int nr_canvas_width = width, mtn_canvas_width = width;
	unsigned int mv_canvas_width = width, canvas_align_width = 32;
	unsigned long di_post_mem = 0, nrds_mem = 0;
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	struct vframe_s *pvframe_in_dup = get_vframe_in_dup(channel);
	struct vframe_s *pvframe_local = get_vframe_local(channel);
	struct vframe_s *pvframe_post = get_vframe_post(channel);
	struct di_buf_s *pbuf_local = get_buf_local(channel);
	struct di_buf_s *pbuf_in = get_buf_in(channel);
	struct di_buf_s *pbuf_post = get_buf_post(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_buf_s *keep_buf = ppost->keep_buf;
	struct di_dev_s *de_devp = get_dim_de_devp();
/*	struct di_buf_s *keep_buf_post = ppost->keep_buf_post;*/
	struct di_mm_s *mm = dim_mm_get(channel); /*mm-0705*/

	unsigned int mem_st_local;

	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	dbg_reg("%s:begin\n", __func__);
	frame_count = 0;
	disp_frame_count = 0;
	cur_post_ready_di_buf = NULL;
	/* decoder'buffer had been releae no need put */
	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		pvframe_in[i] = NULL;
	/*pre init*/
	memset(ppre, 0, sizeof(struct di_pre_stru_s));

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
		ppre->prog_proc_type = 1;
		mm->cfg.buf_alloc_mode = 1;
		di_buf_size = nr_width * canvas_height * 2;
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);
	} else {
		/*pr_info("canvas_height=%d\n", canvas_height);*/
		ppre->prog_proc_type = 0;
		mm->cfg.buf_alloc_mode = 0;
		/*nr_size(bits) = w * active_h * 8 * 2(yuv422)
		 * mtn(bits) = w * active_h * 4
		 * cont(bits) = w * active_h * 4 mv(bits) = w * active_h / 5*16
		 * mcinfo(bits) = active_h * 16
		 */
		nr_size = (nr_width * canvas_height) * 8 * 2 / 16;
		mtn_size = (mtn_width * canvas_height) * 4 / 16;
		count_size = (mtn_width * canvas_height) * 4 / 16;
		mv_size = (mv_width * canvas_height) / 5;
		/*mc_size = canvas_height;*/
		mc_size = roundup(canvas_height >> 1, canvas_align_width) << 1;
		if (mc_mem_alloc) {
			di_buf_size = nr_size + mtn_size + count_size +
				mv_size + mc_size;
		} else {
			di_buf_size = nr_size + mtn_size + count_size;
		}
		di_buf_size = roundup(di_buf_size, PAGE_SIZE);
	}
	/*de_devp->buffer_size = di_buf_size;*/
	mm->cfg.size_local = di_buf_size;

	mm->cfg.nr_size = nr_size;
	mm->cfg.count_size = count_size;
	mm->cfg.mtn_size = mtn_size;
	mm->cfg.mv_size = mv_size;
	mm->cfg.mcinfo_size = mc_size;

	dimp_set(edi_mp_same_field_top_count, 0);
	same_field_bot_count = 0;
	dbg_init("size:\n");
	dbg_init("\t%-15s:0x%x\n", "nr_size", mm->cfg.nr_size);
	dbg_init("\t%-15s:0x%x\n", "count", mm->cfg.count_size);
	dbg_init("\t%-15s:0x%x\n", "mtn", mm->cfg.mtn_size);
	dbg_init("\t%-15s:0x%x\n", "mv", mm->cfg.mv_size);
	dbg_init("\t%-15s:0x%x\n", "mcinfo", mm->cfg.mcinfo_size);

	/**********************************************/
	/* que init */
	/**********************************************/

	queue_init(channel, mm->cfg.num_local);
	di_que_init(channel); /*new que*/

	mem_st_local = di_get_mem_start(channel);

	/**********************************************/
	/* local buf init */
	/**********************************************/

	for (i = 0; i < mm->cfg.num_local; i++) {
		struct di_buf_s *di_buf = &pbuf_local[i];
		int ii = USED_LOCAL_BUF_MAX;

		if (!IS_ERR_OR_NULL(keep_buf)) {
			for (ii = 0; ii < USED_LOCAL_BUF_MAX; ii++) {
				if (di_buf == keep_buf->di_buf_dup_p[ii]) {
					dim_print("%s skip %d\n", __func__, i);
					break;
				}
			}
		}

		if (ii >= USED_LOCAL_BUF_MAX) {
			/* backup cma pages */
			tmp_page = di_buf->pages;
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->pages = tmp_page;
			di_buf->type = VFRAME_TYPE_LOCAL;
			di_buf->pre_ref_count = 0;
			di_buf->post_ref_count = 0;
			di_buf->canvas_width[NR_CANVAS] = nr_canvas_width;
			di_buf->canvas_width[MTN_CANVAS] = mtn_canvas_width;
			di_buf->canvas_width[MV_CANVAS] = mv_canvas_width;
			if (prog_flag) {
				di_buf->canvas_height = canvas_height;
				di_buf->canvas_height_mc = canvas_height;
				di_buf->nr_adr = mem_st_local +
					di_buf_size * i;
				di_buf->canvas_config_flag = 1;
			} else {
				di_buf->canvas_height = (canvas_height >> 1);
				di_buf->canvas_height_mc =
					roundup(di_buf->canvas_height,
						canvas_align_width);
				di_buf->nr_adr = mem_st_local +
					di_buf_size * i;
				di_buf->mtn_adr = mem_st_local +
					di_buf_size * i +
					nr_size;
				di_buf->cnt_adr = mem_st_local +
					di_buf_size * i +
					nr_size + mtn_size;

				if (mc_mem_alloc) {
					di_buf->mcvec_adr = mem_st_local +
						di_buf_size * i +
						nr_size + mtn_size
						+ count_size;
					di_buf->mcinfo_adr =
						mem_st_local +
						di_buf_size * i + nr_size +
						mtn_size + count_size
						+ mv_size;
				if (cfgeq(MEM_FLAG, EDI_MEM_M_REV) ||
				    cfgeq(MEM_FLAG, EDI_MEM_M_CMA_ALL))
					dim_mcinfo_v_alloc(di_buf,
							   mm->cfg.mcinfo_size);
				}
				di_buf->canvas_config_flag = 2;
			}
			di_buf->index = i;
			di_buf->vframe = &pvframe_local[i];
			di_buf->vframe->private_data = di_buf;
			di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
			di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			di_buf->channel = channel;
			queue_in(channel, di_buf, QUEUE_LOCAL_FREE);
			dbg_init("buf[%d], addr=0x%lx\n", di_buf->index,
				 di_buf->nr_adr);
		}
	}

	if (cfgeq(MEM_FLAG, EDI_MEM_M_CMA)	||
	    cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_A)	||
	    cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_B)) {	/*trig cma alloc*/
		dip_wq_cma_run(channel, true);
	}

	dbg_init("one local buf size:0x%x\n", di_buf_size);
	/*mm-0705	di_post_mem = mem_st_local +*/
	/*mm-0705		di_buf_size*de_devp->buf_num_avail;*/
	di_post_mem = mem_st_local + di_buf_size * mm->cfg.num_local;
	if (dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support)) {
		di_post_buf_size = nr_width * canvas_height * 2;
		#ifdef MARK_HIS	/*ary test for vout 25Hz*/
		/* pre buffer must 2 more than post buffer */
		if ((de_devp->buf_num_avail - 2) > MAX_POST_BUF_NUM)
			ppost->di_post_num = MAX_POST_BUF_NUM;
		else
			ppost->di_post_num = (de_devp->buf_num_avail - 2);
		#else
			/*mm-0705 ppost->di_post_num = MAX_POST_BUF_NUM;*/
		#endif
		dbg_init("DI: di post buffer size 0x%x byte.\n",
			 di_post_buf_size);
	} else {
		/*mm-0705 ppost->di_post_num = MAX_POST_BUF_NUM;*/
		di_post_buf_size = 0;
	}
	/*de_devp->post_buffer_size = di_post_buf_size;*/
	mm->cfg.size_post = di_post_buf_size;

	/**********************************************/
	/* input buf init */
	/**********************************************/

	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		struct di_buf_s *di_buf = &pbuf_in[i];

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
			di_buf->channel = channel;
			di_que_in(channel, QUE_IN_FREE, di_buf);
		}
	}
	/**********************************************/
	/* post buf init */
	/**********************************************/
	/*mm-0705 for (i = 0; i < ppost->di_post_num; i++) {*/
	for (i = 0; i < mm->cfg.num_post; i++) {
		struct di_buf_s *di_buf = &pbuf_post[i];

		if (di_buf) {
			if (dimp_get(edi_mp_post_wr_en) &&
			    dimp_get(edi_mp_post_wr_support)) {
				if (di_que_is_in_que(channel, QUE_POST_KEEP,
						     di_buf)) {
					dbg_reg("%s:post keep buf %d\n",
						__func__,
						di_buf->index);
					continue;
				}
			}
			tmp_page = di_buf->pages;
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->pages		= tmp_page;
			di_buf->type = VFRAME_TYPE_POST;
			di_buf->index = i;
			di_buf->vframe = &pvframe_post[i];
			di_buf->vframe->private_data = di_buf;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			di_buf->channel = channel;
			if (dimp_get(edi_mp_post_wr_en) &&
			    dimp_get(edi_mp_post_wr_support)) {
				di_buf->canvas_width[NR_CANVAS] =
					(nr_width << 1);
				di_buf->canvas_height = canvas_height;
				di_buf->canvas_config_flag = 1;
				di_buf->nr_adr = di_post_mem
					+ di_post_buf_size * i;
				dbg_init("[%d]post buf:%d: addr=0x%lx\n", i,
					 di_buf->index, di_buf->nr_adr);
			}

			di_que_in(channel, QUE_POST_FREE, di_buf);

		} else {
			PR_ERR("%s:%d:post buf is null\n", __func__, i);
		}
	}
	if (cfgeq(MEM_FLAG, EDI_MEM_M_REV) && de_devp->nrds_enable) {
		nrds_mem = di_post_mem + mm->cfg.num_post * di_post_buf_size;
		/*mm-0705 ppost->di_post_num * di_post_buf_size;*/
		dim_nr_ds_buf_init(cfgg(MEM_FLAG), nrds_mem,
				   &de_devp->pdev->dev);
	}
	return 0;
}

#ifdef DIM_HIS	/*no use*/
void dim_keep_mirror_buffer(unsigned int channel)	/*not use*/
{
	struct di_buf_s *p = NULL;
	int i = 0, ii = 0, itmp;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	queue_for_each_entry(p, channel, QUEUE_DISPLAY, list) {
		if (p->di_buf[0]->type != VFRAME_TYPE_IN	&&
		    p->process_fun_index != PROCESS_FUN_NULL	&&
		    ii < USED_LOCAL_BUF_MAX			&&
		    p->index == ppost->cur_disp_index) {
			ppost->keep_buf = p;
			for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
				if (IS_ERR_OR_NULL(p->di_buf_dup_p[i]))
					continue;
				/* prepare for recycle
				 * the keep buffer
				 */
				p->di_buf_dup_p[i]->pre_ref_count = 0;
				p->di_buf_dup_p[i]->post_ref_count = 0;
				if (p->di_buf_dup_p[i]->queue_index >= 0 &&
				    p->di_buf_dup_p[i]->queue_index
				    < QUEUE_NUM) {
					if (is_in_queue
						(channel,
						 p->di_buf_dup_p[i],
						 p->di_buf_dup_p[i]->queue_index
						 ))
						queue_out(channel,
							  p->di_buf_dup_p[i]);
						/*which que?*/
				}
				ii++;
				if (p->di_buf_dup_p[i]->di_wr_linked_buf)
					p->di_buf_dup_p[i + 1] =
						p->di_buf_dup_p[i]->
						di_wr_linked_buf;
			}
			queue_out(channel, p);	/*which que?*/
			break;
		}
	}
}
#endif

void dim_post_keep_mirror_buffer(unsigned int channel)
{
	struct di_buf_s *p = NULL;
	int itmp;
	bool flg = false;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	queue_for_each_entry(p, channel, QUEUE_DISPLAY, list) {
		if (p->type != VFRAME_TYPE_POST	||
		    !p->process_fun_index) {
			dbg_reg("%s:not post buf:%d\n",
				__func__, p->type);
			continue;
		}

		ppost->keep_buf_post = p;	/*only keep one*/
		flg = true;
		dbg_reg("%s %d\n", __func__, p->index);
	}

	if (flg && ppost->keep_buf_post) {
		ppost->keep_buf_post->queue_index = -1;
		ppost->keep_buf_post->invert_top_bot_flag = 0;
	}
}

void dim_post_keep_mirror_buffer2(unsigned int ch)
{
	struct di_buf_s *p = NULL;
	int itmp;

	queue_for_each_entry(p, ch, QUEUE_DISPLAY, list) {
		if (p->type != VFRAME_TYPE_POST	||
		    !p->process_fun_index) {
			dbg_keep("%s:not post buf:%d\n", __func__, p->type);
			continue;
		}
		if (di_que_is_in_que(ch, QUE_POST_BACK, p)) {
			dbg_keep("%s:is in back[%d]\n", __func__, p->index);
			continue;
		}

		p->queue_index = -1;
		di_que_in(ch, QUE_POST_KEEP, p);
		p->invert_top_bot_flag = 0;

		dbg_keep("%s %d\n", __func__, p->index);
	}
}

bool dim_post_keep_is_in(unsigned int ch, struct di_buf_s *di_buf)
{
	if (di_que_is_in_que(ch, QUE_POST_KEEP, di_buf))
		return true;
	return false;
}

bool dim_post_keep_release_one(unsigned int ch, unsigned int di_buf_index)
{
	struct di_buf_s *pbuf_post;
	struct di_buf_s *di_buf;

	/*must post or err*/
	pbuf_post = get_buf_post(ch);
	di_buf = &pbuf_post[di_buf_index];

	if (!di_que_is_in_que(ch, QUE_POST_KEEP, di_buf)) {
		if (is_in_queue(ch, di_buf, QUEUE_DISPLAY)) {
			di_buf->queue_index = -1;
			di_que_in(ch, QUE_POST_BACK, di_buf);
			dbg_keep("%s:to back[%d]\n", __func__, di_buf_index);
		} else {
			PR_ERR("%s:buf[%d] is not in keep or display\n",
			       __func__, di_buf_index);
		}
		return false;
	}
	di_que_out_not_fifo(ch, QUE_POST_KEEP, di_buf);
	di_que_in(ch, QUE_POST_FREE, di_buf);
	dbg_keep("%s:buf[%d]\n", __func__, di_buf_index);
	return true;
}

bool dim_post_keep_release_all_2free(unsigned int ch)
{
	struct di_buf_s *di_buf;
	struct di_buf_s *pbuf_post;
	unsigned int i = 0;

	if (di_que_is_empty(ch, QUE_POST_KEEP))
		return true;

	pbuf_post = get_buf_post(ch);
	dbg_keep("%s:ch[%d]\n", __func__, ch);

	while (i <= MAX_POST_BUF_NUM) {
		i++;
		di_buf = di_que_peek(ch, QUE_POST_KEEP);
		if (!di_buf)
			break;

		if (!di_que_out(ch, QUE_POST_KEEP, di_buf)) {
			PR_ERR("%s:out err\n", __func__);
			break;
		}

		di_que_in(ch, QUE_POST_FREE, di_buf);

		dbg_keep("\ttype[%d],index[%d]\n", di_buf->type, di_buf->index);
	}

	return true;
}

void dim_post_keep_cmd_release(unsigned int ch, struct vframe_s *vframe)
{
	struct di_buf_s *di_buf;

	if (!vframe)
		return;

	di_buf = (struct di_buf_s *)vframe->private_data;

	if (!di_buf) {
		PR_WARN("%s:ch[%d]:no di_buf\n", __func__, ch);
		return;
	}

	if (di_buf->type != VFRAME_TYPE_POST) {
		PR_WARN("%s:ch[%d]:not post\n", __func__, ch);
		return;
	}
	dbg_keep("release keep ch[%d],index[%d]\n",
		 di_buf->channel,
		 di_buf->index);
	task_send_cmd(LCMD2(ECMD_RL_KEEP, di_buf->channel, di_buf->index));
}

void dim_post_keep_cmd_release2(struct vframe_s *vframe)
{
	struct di_buf_s *di_buf;

	if (!dil_get_diffver_flag())
		return;

	if (!vframe)
		return;

	di_buf = (struct di_buf_s *)vframe->private_data;

	if (!di_buf) {
		PR_WARN("%s:no di_buf\n", __func__);
		return;
	}

	if (di_buf->type != VFRAME_TYPE_POST) {
		PR_WARN("%s:ch[%d]:not post\n", __func__, di_buf->channel);
		return;
	}

	dbg_keep("release keep ch[%d],index[%d]\n",
		 di_buf->channel,
		 di_buf->index);
	task_send_cmd(LCMD2(ECMD_RL_KEEP, di_buf->channel,
			    di_buf->index));
}
EXPORT_SYMBOL(dim_post_keep_cmd_release2);

void dim_dbg_release_keep_all(unsigned int ch)
{
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize, itmp;
	struct di_buf_s *p;

	di_que_list(ch, QUE_POST_KEEP, &tmpa[0], &psize);
	dbg_keep("post_keep: curr(%d)\n", psize);

	for (itmp = 0; itmp < psize; itmp++) {
		p = pw_qindex_2_buf(ch, tmpa[itmp]);
		dbg_keep("\ttype[%d],index[%d]\n", p->type, p->index);
		dim_post_keep_cmd_release(ch, p->vframe);
	}
	dbg_keep("%s:end\n", __func__);
}

void dim_post_keep_back_recycle(unsigned int ch)
{
	unsigned int tmpa[MAX_FIFO_SIZE];
	unsigned int psize, itmp;
	struct di_buf_s *p;

	if (di_que_is_empty(ch, QUE_POST_KEEP_BACK))
		return;
	di_que_list(ch, QUE_POST_KEEP_BACK, &tmpa[0], &psize);
	dbg_keep("post_keep_back: curr(%d)\n", psize);
	for (itmp = 0; itmp < psize; itmp++) {
		p = pw_qindex_2_buf(ch, tmpa[itmp]);
		dbg_keep("keep back recycle %d\n", p->index);
		dim_post_keep_release_one(ch, p->index);
	}
	pw_queue_clear(ch, QUE_POST_KEEP_BACK);
}

void dim_post_keep_cmd_proc(unsigned int ch, unsigned int index)
{
	struct di_dev_s *di_dev;
	enum EDI_TOP_STATE chst;
	unsigned int len_keep, len_back;
	struct di_buf_s *pbuf_post;
	struct di_buf_s *di_buf;

	/*must post or err*/
	di_dev = get_dim_de_devp();

	if (!di_dev || !di_dev->data_l) {
		PR_WARN("%s: no di_dev\n", __func__);
		return;
	}

	chst = dip_chst_get(ch);
	switch (chst) {
	case EDI_TOP_STATE_READY:
	case EDI_TOP_STATE_UNREG_STEP2:
		dim_post_keep_release_one(ch, index);
		break;
	case EDI_TOP_STATE_IDLE:
	case EDI_TOP_STATE_BYPASS:
		pbuf_post = get_buf_post(ch);
		if (!pbuf_post) {
			PR_ERR("%s:no pbuf_post\n", __func__);
			break;
		}

		di_buf = &pbuf_post[index];
		di_buf->queue_index = -1;
		di_que_in(ch, QUE_POST_KEEP_BACK, di_buf);
		len_keep = di_que_list_count(ch, QUE_POST_KEEP);
		len_back = di_que_list_count(ch, QUE_POST_KEEP_BACK);
		if (len_back >= len_keep) {
			/*release all*/
			pw_queue_clear(ch, QUE_POST_KEEP);
			pw_queue_clear(ch, QUE_POST_KEEP_BACK);
			dip_wq_cma_run(ch, false);
		}
		break;
	case EDI_TOP_STATE_REG_STEP1:
	case EDI_TOP_STATE_REG_STEP1_P1:
	case EDI_TOP_STATE_REG_STEP2:
		pbuf_post = get_buf_post(ch);
		if (!pbuf_post) {
			PR_ERR("%s:no pbuf_post\n", __func__);
			break;
		}

		di_buf = &pbuf_post[index];
		di_buf->queue_index = -1;
		di_que_in(ch, QUE_POST_KEEP_BACK, di_buf);

		break;
	case EDI_TOP_STATE_UNREG_STEP1:
		pbuf_post = get_buf_post(ch);
		if (!pbuf_post) {
			PR_ERR("%s:no pbuf_post\n", __func__);
			break;
		}

		di_buf = &pbuf_post[index];
		if (is_in_queue(ch, di_buf, QUEUE_DISPLAY)) {
			di_buf->queue_index = -1;
			di_que_in(ch, QUE_POST_BACK, di_buf);
			dbg_keep("%s:to back[%d]\n", __func__, index);
		} else {
			PR_ERR("%s:buf[%d] is not in display\n",
			       __func__, index);
		}
		break;
	default:
		PR_ERR("%s:do nothing? %s:%d\n",
		       __func__,
		       dip_chst_get_name(chst),
		       index);
		break;
	}
}

void dim_uninit_buf(unsigned int disable_mirror, unsigned int channel)
{
	/*int i = 0;*/
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (!queue_empty(channel, QUEUE_DISPLAY) || disable_mirror)
		ppost->keep_buf = NULL;
#ifdef MARK_HIS
	if (disable_mirror != 1)
		dim_keep_mirror_buffer();

	if (!IS_ERR_OR_NULL(di_post_stru.keep_buf)) {
		keep_buf = di_post_stru.keep_buf;
		pr_dbg("%s keep cur di_buf %d (",
		       __func__, keep_buf->index);
		for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
			if (!IS_ERR_OR_NULL(keep_buf->di_buf_dup_p[i]))
				pr_dbg("%d\t",
				       keep_buf->di_buf_dup_p[i]->index);
		}
		pr_dbg(")\n");
	}
#else
	if (!disable_mirror)
		dim_post_keep_mirror_buffer2(channel);
#endif
	queue_init(channel, 0);
	di_que_init(channel); /*new que*/

	/* decoder'buffer had been releae no need put */
	#ifdef MARK_HIS
	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		pvframe_in[i] = NULL;
	#else
	memset(pvframe_in, 0, sizeof(*pvframe_in) * MAX_IN_BUF_NUM);
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

void dim_log_buffer_state(unsigned char *tag, unsigned int channel)
{
	struct di_pre_stru_s *ppre;
	unsigned int tmpa[MAX_FIFO_SIZE]; /*new que*/
	unsigned int psize; /*new que*/

	if (dimp_get(edi_mp_di_log_flag) & DI_LOG_BUFFER_STATE) {
		struct di_buf_s *p = NULL;/* , *ptmp; */
		int itmp;
		int in_free = 0;
		int local_free = 0;
		int pre_ready = 0;
		int post_free = 0;
		int post_ready = 0;
		int post_ready_ext = 0;
		int display = 0;
		int display_ext = 0;
		int recycle = 0;
		int di_inp = 0;
		int di_wr = 0;
		ulong irq_flag2 = 0;

		ppre = get_pre_stru(channel);

		di_lock_irqfiq_save(irq_flag2);
		in_free = di_que_list_count(channel, QUE_IN_FREE);
		local_free = list_count(channel, QUEUE_LOCAL_FREE);
		pre_ready = di_que_list_count(channel, QUE_PRE_READY);
		post_free = di_que_list_count(channel, QUE_POST_FREE);
		post_ready = di_que_list_count(channel, QUE_POST_READY);

		di_que_list(channel, QUE_POST_READY, &tmpa[0], &psize);
		/*di_que_for_each(channel, p, psize, &tmpa[0]) {*/
		for (itmp = 0; itmp < psize; itmp++) {
			p = pw_qindex_2_buf(channel, tmpa[itmp]);
			if (p->di_buf[0])
				post_ready_ext++;

			if (p->di_buf[1])
				post_ready_ext++;
		}
		queue_for_each_entry(p, channel, QUEUE_DISPLAY, list) {
			display++;
			if (p->di_buf[0])
				display_ext++;

			if (p->di_buf[1])
				display_ext++;
		}
		recycle = list_count(channel, QUEUE_RECYCLE);

		if (ppre->di_inp_buf)
			di_inp++;
		if (ppre->di_wr_buf)
			di_wr++;

		if (dimp_get(edi_mp_buf_state_log_threshold) == 0)
			buf_state_log_start = 0;
		else if (post_ready < dimp_get(edi_mp_buf_state_log_threshold))
			buf_state_log_start = 1;

		if (buf_state_log_start) {
			dim_print("[%s]i i_f %d/%d, l_f %d",
				  tag,
				  in_free, MAX_IN_BUF_NUM,
				  local_free);
			dim_print("pre_r %d, post_f %d/%d",
				  pre_ready,
				  post_free, MAX_POST_BUF_NUM);
			dim_print("post_r (%d:%d), disp (%d:%d),rec %d\n",
				  post_ready, post_ready_ext,
				  display, display_ext, recycle);
			dim_print("di_i %d, di_w %d\n", di_inp, di_wr);
		}
		di_unlock_irqfiq_restore(irq_flag2);
	}
}

static void dump_state(unsigned int channel)
{
	struct di_buf_s *p = NULL, *keep_buf;
	int itmp, i;
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
	/*struct di_dev_s *de_devp = get_dim_de_devp();*/
	unsigned int tmpa[MAX_FIFO_SIZE];	/*new que*/
	unsigned int psize;		/*new que*/
	struct di_mm_s *mm = dim_mm_get(channel); /*mm-0705*/

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
	keep_buf = ppost->keep_buf;
	pr_info("used_post_buf_index %d(0x%p),",
		IS_ERR_OR_NULL(keep_buf) ?
		-1 : keep_buf->index, keep_buf);
#ifdef MARK_HIS
	if (!IS_ERR_OR_NULL(keep_buf)) {
		pr_info("used_local_buf_index:\n");
		for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
			p = keep_buf->di_buf_dup_p[i];
			pr_info("%d(0x%p) ",
				IS_ERR_OR_NULL(p) ? -1 : p->index, p);
		}
	}
#endif
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
		pre_size_h = (di_buf->vframe->height + 1) / 2;
		di_mcinfo_mif->size_x = (pre_size_h + 1) / 2 - 1;
		di_mcinfo_mif->size_y = 1;
		di_mcinfo_mif->canvas_num = di_buf->mcinfo_canvas_idx;

		di_mcvec_mif->size_x = (pre_size_w + 4) / 5 - 1;
		di_mcvec_mif->size_y = pre_size_h - 1;
		di_mcvec_mif->canvas_num = di_buf->mcvec_canvas_idx;
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
	if (ppre->prog_proc_type == 0) {
		di_mtnwr_mif->start_x = 0;
		di_mtnwr_mif->end_x = vf->width - 1;
		di_mtnwr_mif->start_y = 0;
		di_mtnwr_mif->end_y = vf->height / 2 - 1;
		di_mtnwr_mif->canvas_num = di_buf->mtn_canvas_idx;
	}
}

static void config_di_mif(struct DI_MIF_s *di_mif, struct di_buf_s *di_buf,
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

	di_mif->nocompress = (di_buf->vframe->type & VIDTYPE_COMPRESS) ? 0 : 1;

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
	} else {
		if (di_buf->vframe->type & VIDTYPE_VIU_444)
			di_mif->video_mode = 1;
		else
			di_mif->video_mode = 0;
		if (di_buf->vframe->type & VIDTYPE_VIU_NV21)
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
				di_buf->vframe->height / 2 - 1;
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
					di_buf->vframe->height - 2;
				di_mif->chroma_x_start0 = 0;
				di_mif->chroma_x_end0 =
					di_buf->vframe->width / 2 - 1;
				di_mif->chroma_y_start0 = 0;
				di_mif->chroma_y_end0 =
					di_buf->vframe->height / 2
						- (di_mif->src_prog ? 1 : 2);
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
					di_buf->vframe->height / 2 - 1;
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
void dim_pre_de_process(unsigned int channel)
{
	ulong irq_flag2 = 0;
	unsigned short pre_width = 0, pre_height = 0;
	unsigned char chan2_field_num = 1;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	int canvases_idex = ppre->field_count_for_cont % 2;
	unsigned short cur_inp_field_type = VIDTYPE_TYPEMASK;
	unsigned short int_mask = 0x7f;
	struct di_dev_s *de_devp = get_dim_de_devp();

	ppre->pre_de_process_flag = 1;
	dim_ddbg_mod_save(EDI_DBG_MOD_PRE_SETB, channel, ppre->in_seq);/*dbg*/

	#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	pre_inp_canvas_config(ppre->di_inp_buf->vframe);
	#endif

	config_di_mif(&ppre->di_inp_mif, ppre->di_inp_buf, channel);
	/* pr_dbg("set_separate_en=%d vframe->type %d\n",
	 * di_pre_stru.di_inp_mif.set_separate_en,
	 * di_pre_stru.di_inp_buf->vframe->type);
	 */
#ifdef DI_USE_FIXED_CANVAS_IDX
	if (ppre->di_mem_buf_dup_p	&&
	    ppre->di_mem_buf_dup_p != ppre->di_inp_buf) {
		config_canvas_idx(ppre->di_mem_buf_dup_p,
				  di_pre_idx[canvases_idex][0], -1);
		config_cnt_canvas_idx(ppre->di_mem_buf_dup_p,
				      di_pre_idx[canvases_idex][1]);
	} else {
		config_cnt_canvas_idx(ppre->di_wr_buf,
				      di_pre_idx[canvases_idex][1]);
		config_di_cnt_mif(&ppre->di_contp2rd_mif,
				  ppre->di_wr_buf);
	}
	if (ppre->di_chan2_buf_dup_p) {
		config_canvas_idx(ppre->di_chan2_buf_dup_p,
				  di_pre_idx[canvases_idex][2], -1);
		config_cnt_canvas_idx(ppre->di_chan2_buf_dup_p,
				      di_pre_idx[canvases_idex][3]);
	} else {
		config_cnt_canvas_idx(ppre->di_wr_buf,
				      di_pre_idx[canvases_idex][3]);
	}
	config_canvas_idx(ppre->di_wr_buf,
			  di_pre_idx[canvases_idex][4],
			  di_pre_idx[canvases_idex][5]);
	config_cnt_canvas_idx(ppre->di_wr_buf,
			      di_pre_idx[canvases_idex][6]);
	if (dimp_get(edi_mp_mcpre_en)) {
		if (ppre->di_chan2_buf_dup_p)
			config_mcinfo_canvas_idx(ppre->di_chan2_buf_dup_p,
						 di_pre_idx[canvases_idex][7]);
		else
			config_mcinfo_canvas_idx(ppre->di_wr_buf,
						 di_pre_idx[canvases_idex][7]);

		config_mcinfo_canvas_idx(ppre->di_wr_buf,
					 di_pre_idx[canvases_idex][8]);
		config_mcvec_canvas_idx(ppre->di_wr_buf,
					di_pre_idx[canvases_idex][9]);
	}
#endif
	config_di_mif(&ppre->di_mem_mif, ppre->di_mem_buf_dup_p, channel);
	if (!ppre->di_chan2_buf_dup_p) {
		config_di_mif(&ppre->di_chan2_mif,
			      ppre->di_inp_buf, channel);
	} else {
		config_di_mif(&ppre->di_chan2_mif,
			      ppre->di_chan2_buf_dup_p, channel);
	}
	config_di_wr_mif(&ppre->di_nrwr_mif, &ppre->di_mtnwr_mif,
			 ppre->di_wr_buf, channel);

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

	pre_width = ppre->di_nrwr_mif.end_x + 1;
	pre_height = ppre->di_nrwr_mif.end_y + 1;
	if (ppre->input_size_change_flag) {
		cur_inp_field_type =
		(ppre->di_inp_buf->vframe->type & VIDTYPE_TYPEMASK);
		cur_inp_field_type =
	ppre->cur_prog_flag ? VIDTYPE_PROGRESSIVE : cur_inp_field_type;
		/*di_async_reset2();*/
		di_pre_size_change(pre_width, pre_height,
				   cur_inp_field_type, channel);
		ppre->input_size_change_flag = false;
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

	/* set interrupt mask for pre module.
	 * we need to only leave one mask open
	 * to prevent multiple entry for dim_irq
	 */

	/*dim_dbg_pre_cnt(channel, "s2");*/

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
			       ppre->vdin2nr);

	dimh_enable_afbc_input(ppre->di_inp_buf->vframe);

	if (dimp_get(edi_mp_mcpre_en)) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
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

	ppre->field_count_for_cont++;
	dimh_txl_patch_prog(ppre->cur_prog_flag,
			    ppre->field_count_for_cont,
			    dimp_get(edi_mp_mcpre_en));

#ifdef SUPPORT_MPEG_TO_VDIN
	if (mpeg2vdin_flag) {
		struct vdin_arg_s vdin_arg;
		struct vdin_v4l2_ops_s *vdin_ops = get_vdin_v4l2_ops();

		vdin_arg.cmd = VDIN_CMD_FORCE_GO_FIELD;
		if (vdin_ops->tvin_vdin_func)
			vdin_ops->tvin_vdin_func(0, &vdin_arg);
	}
#endif
	/* must make sure follow part issue without interrupts,
	 * otherwise may cause watch dog reboot
	 */
	di_lock_irqfiq_save(irq_flag2);
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
	di_pre_wait_irq_set(true);
	di_unlock_irqfiq_restore(irq_flag2);
	/*reinit pre busy flag*/
	ppre->pre_de_busy = 1;

	#ifdef SUPPORT_MPEG_TO_VDIN
	if (mpeg2vdin_flag)
		DIM_RDMA_WR_BITS(DI_PRE_CTRL, 1, 13, 1);
	#endif
	dim_dbg_pre_cnt(channel, "s3");
	ppre->irq_time[0] = cur_to_msecs();
	ppre->irq_time[1] = cur_to_msecs();
	dim_ddbg_mod_save(EDI_DBG_MOD_PRE_SETE, channel, ppre->in_seq);/*dbg*/
	dim_tr_ops.pre_set(ppre->di_wr_buf->vframe->omx_index);
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

void dim_pre_de_done_buf_config(unsigned int channel, bool flg_timeout)
{
	ulong irq_flag2 = 0;
	int tmp_cur_lev;
	struct di_buf_s *post_wr_buf = NULL;
	unsigned int frame_motnum = 0;
	unsigned int field_motnum = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	dim_dbg_pre_cnt(channel, "d1");
	dim_ddbg_mod_save(EDI_DBG_MOD_PRE_DONEB, channel, ppre->in_seq);/*dbg*/
	if (ppre->di_wr_buf) {
		dim_tr_ops.pre_ready(ppre->di_wr_buf->vframe->omx_index);
		if (ppre->pre_throw_flag > 0) {
			ppre->di_wr_buf->throw_flag = 1;
			ppre->pre_throw_flag--;
		} else {
			ppre->di_wr_buf->throw_flag = 0;
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
		if (!di_pre_rdma_enable)
			ppre->di_post_wr_buf = ppre->di_wr_buf;
		post_wr_buf = ppre->di_post_wr_buf;

		if (post_wr_buf && !ppre->cur_prog_flag &&
		    !flg_timeout) {
			dim_read_pulldown_info(&frame_motnum,
					       &field_motnum);
			if (dimp_get(edi_mp_pulldown_enable))
				/*pulldown_detection*/
				get_ops_pd()->detection
					(&post_wr_buf->pd_config,
					 ppre->mtn_status,
					 overturn,
					 ppre->di_inp_buf->vframe);
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
			dim_pulldown_info_clear_g12a();
		}

		if (ppre->cur_prog_flag) {
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
		di_lock_irqfiq_save(irq_flag2);
		queue_in(channel, ppre->di_post_inp_buf, QUEUE_RECYCLE);
		ppre->di_post_inp_buf = NULL;
		di_unlock_irqfiq_restore(irq_flag2);
	}
	if (ppre->di_inp_buf) {
		if (!di_pre_rdma_enable) {
#ifdef DI_BUFFER_DEBUG
			dim_print("%s: %s[%d] => recycle_list\n", __func__,
				  vframe_type_name[ppre->di_inp_buf->type],
				  ppre->di_inp_buf->index);
#endif
			di_lock_irqfiq_save(irq_flag2);
			queue_in(channel, ppre->di_inp_buf, QUEUE_RECYCLE);
			ppre->di_inp_buf = NULL;
			di_unlock_irqfiq_restore(irq_flag2);
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
	ulong irq_flag2 = 0;

	di_lock_irqfiq_save(irq_flag2);

	queue_in(channel, di_buf, QUEUE_RECYCLE);

	di_unlock_irqfiq_restore(irq_flag2);
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
	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(di_inp_idx[0],
				     &vf->canvas0_config[0]);
		canvas_config_config(di_inp_idx[1],
				     &vf->canvas0_config[1]);
		vf->canvas0Addr = (di_inp_idx[1] << 8) | (di_inp_idx[0]);
		if (vf->plane_num == 2) {
			vf->canvas0Addr |= (di_inp_idx[1] << 16);
		} else if (vf->plane_num == 3) {
			canvas_config_config(di_inp_idx[2],
					     &vf->canvas0_config[2]);
			vf->canvas0Addr |= (di_inp_idx[2] << 16);
		}
		vf->canvas1Addr = vf->canvas0Addr;
	}
}
#endif

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

#ifdef MARK_HIS
/*must been called when dim_pre_de_buf_config return true*/
void pre_p_asi_set_next(unsigned int ch)
{
	struct di_pre_stru_s *ppre = get_pre_stru(ch);

	ppre->p_asi_next = ppre->di_inp_buf;
}
#endif

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

/*for first frame no need to ready buf*/
bool dim_bypass_first_frame(unsigned int ch)
{
	struct di_buf_s *di_buf = NULL;
	struct di_buf_s *di_buf_post = NULL;
	struct vframe_s *vframe;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	struct vframe_s **pvframe_in = get_vframe_in(ch);
	ulong irq_flag2 = 0;

	vframe = pw_vf_peek(ch);

	if (!vframe)
		return false;
	if (di_que_is_empty(ch, QUE_POST_FREE))
		return false;

	vframe = pw_vf_get(ch);

	di_buf = di_que_out_to_di_buf(ch, QUE_IN_FREE);

	if (dim_check_di_buf(di_buf, 10, ch))
		return 0;

	memcpy(di_buf->vframe, vframe, sizeof(struct vframe_s));
	di_buf->vframe->private_data = di_buf;
	pvframe_in[di_buf->index] = vframe;
	di_buf->seq = ppre->in_seq;
	ppre->in_seq++;

	#ifdef MARK_HIS

	if (vframe->type & VIDTYPE_COMPRESS) {	/*?*/
		vframe->width = vframe->compWidth;
		vframe->height = vframe->compHeight;
	}

	di_que_in(ch, QUE_PRE_READY, di_buf);
	#endif

	di_buf_post = di_que_out_to_di_buf(ch, QUE_POST_FREE);
	memcpy(di_buf_post->vframe, vframe, sizeof(struct vframe_s));
	di_buf_post->vframe->private_data = di_buf_post;
	di_lock_irqfiq_save(irq_flag2);

	di_que_in(ch, QUE_POST_READY, di_buf_post);

	di_unlock_irqfiq_restore(irq_flag2);
	pw_vf_notify_receiver(ch,
			      VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);

	PR_INF("%s:ok\n", __func__);
	return true;
}

unsigned char dim_pre_de_buf_config(unsigned int channel)
{
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *vframe;
	int i, di_linked_buf_idx = -1;
	unsigned char change_type = 0;
	unsigned char change_type2 = 0;
	bool bit10_pack_patch = false;
	unsigned int width_roundup = 2;
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();
	int cfg_prog_proc = dimp_get(edi_mp_prog_proc_config);
	bool flg_1080i = false;

	if (di_blocking || !dip_cma_st_is_ready(channel))
		return 0;

	if (di_que_list_count(channel, QUE_IN_FREE) < 1)
		return 0;

	if ((di_que_list_count(channel, QUE_IN_FREE) < 2	&&
	     !ppre->di_inp_buf_next)				||
	    (queue_empty(channel, QUEUE_LOCAL_FREE)))
		return 0;

	if (di_que_list_count(channel, QUE_PRE_READY) >= DI_PRE_READY_LIMIT)
		return 0;

	if (dim_is_bypass(NULL, channel)) {
		/* some provider has problem if receiver
		 * get all buffers of provider
		 */
		int in_buf_num = 0;
		/*cur_lev = 0;*/
		dimp_set(edi_mp_cur_lev, 0);
		for (i = 0; i < MAX_IN_BUF_NUM; i++)
			if (pvframe_in[i])
				in_buf_num++;
		if (in_buf_num > BYPASS_GET_MAX_BUF_NUM) {
#ifdef DET3D
			if (ppre->vframe_interleave_flag == 0)
#endif
				return 0;
		}

		dimh_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
	} else if (ppre->prog_proc_type == 2) {
		di_linked_buf_idx = peek_free_linked_buf(channel);
		if (di_linked_buf_idx == -1 &&
		    !IS_ERR_OR_NULL(ppost->keep_buf)) {
			recycle_keep_buffer(channel);
		pr_info("%s: recycle keep buffer for peek null linked buf\n",
			__func__);
			return 0;
		}
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
		/* check if source change */
		vframe = pw_vf_peek(channel);

		if (vframe && is_from_vdin(vframe)) {
#ifdef RUN_DI_PROCESS_IN_IRQ
			ppre->vdin2nr = is_input2pre(channel);
#endif
		}

		vframe = pw_vf_get(channel);

		if (!vframe)
			return 0;

		dim_tr_ops.pre_get(vframe->omx_index);
		didbg_vframe_in_copy(channel, vframe);

		if (vframe->type & VIDTYPE_COMPRESS) {
			vframe->width = vframe->compWidth;
			vframe->height = vframe->compHeight;
		}
		dim_print("DI:ch[%d] get %dth vf[0x%p] from frontend %u ms.\n",
			  channel,
			  ppre->in_seq, vframe,
			  jiffies_to_msecs(jiffies_64 -
			  vframe->ready_jiffies64));
		vframe->prog_proc_config = (cfg_prog_proc & 0x20) >> 5;

		if (vframe->width > 10000 || vframe->height > 10000 ||
		    hold_video || ppre->bad_frame_throw_count > 0) {
			if (vframe->width > 10000 || vframe->height > 10000)
				ppre->bad_frame_throw_count = 10;
			ppre->bad_frame_throw_count--;
			pw_vf_put(vframe, channel);
			pw_vf_notify_provider(channel,
					      VFRAME_EVENT_RECEIVER_PUT, NULL);
			return 0;
		}
		bit10_pack_patch =  (is_meson_gxtvbb_cpu() ||
							is_meson_gxl_cpu() ||
							is_meson_gxm_cpu());
		width_roundup = bit10_pack_patch ? 16 : width_roundup;
		if (dimp_get(edi_mp_di_force_bit_mode) == 10)
			dimp_set(edi_mp_force_width,
				 roundup(vframe->width, width_roundup));
		else
			dimp_set(edi_mp_force_width, 0);
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

		/* backup frame motion info */
		vframe->combing_cur_lev = dimp_get(edi_mp_cur_lev);/*cur_lev;*/

		dim_print("%s: vf_get => 0x%p\n", __func__, vframe);

		di_buf = di_que_out_to_di_buf(channel, QUE_IN_FREE);

		if (dim_check_di_buf(di_buf, 10, channel))
			return 0;

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
		di_buf->vframe->private_data = di_buf;
		pvframe_in[di_buf->index] = vframe;
		di_buf->seq = ppre->in_seq;
		ppre->in_seq++;

		pre_vinfo_set(channel, vframe);
		change_type = is_source_change(vframe, channel);
		#ifdef MARK_HIS
		if (!change_type)
			change_type = is_vinfo_change(channel);
		#endif
		/* source change, when i mix p,force p as i*/
		if (change_type == 1 || (change_type == 2 &&
					 ppre->cur_prog_flag == 1)) {
			if (ppre->di_mem_buf_dup_p) {
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
			#ifdef MARK_HIS
			/* channel change will occur between atv and dtv,
			 * that need mirror
			 */
			if (!IS_ERR_OR_NULL(di_post_stru.keep_buf)) {
				if (di_post_stru.keep_buf->vframe
					->source_type !=
					di_buf->vframe->source_type) {
					recycle_keep_buffer();
					pr_info("%s:source_tp chg recycle bf\n",
						__func__);
				}
			}
			#endif
			pr_info("%s:ch[%d]:%ums %dth source change:\n",
				__func__,
				channel,
				jiffies_to_msecs(jiffies_64),
				ppre->in_seq);
			pr_info("source change:0x%x/%d/%d/%d=>0x%x/%d/%d/%d\n",
				ppre->cur_inp_type,
				ppre->cur_width,
				ppre->cur_height,
				ppre->cur_source_type,
				di_buf->vframe->type,
				di_buf->vframe->width,
				di_buf->vframe->height,
				di_buf->vframe->source_type);
			if (di_buf->type & VIDTYPE_COMPRESS) {
				ppre->cur_width =
					di_buf->vframe->compWidth;
				ppre->cur_height =
					di_buf->vframe->compHeight;
			} else {
				ppre->cur_width = di_buf->vframe->width;
				ppre->cur_height = di_buf->vframe->height;
			}
			ppre->cur_prog_flag =
				is_progressive(di_buf->vframe);
			if (ppre->cur_prog_flag) {
				if ((dimp_get(edi_mp_use_2_interlace_buff)) &&
				    !(cfg_prog_proc & 0x10))
					ppre->prog_proc_type = 2;
				else
					ppre->prog_proc_type =
						cfg_prog_proc & 0x10;
			} else {
				ppre->prog_proc_type = 0;
			}
			ppre->cur_inp_type = di_buf->vframe->type;
			ppre->cur_source_type =
				di_buf->vframe->source_type;
			ppre->cur_sig_fmt = di_buf->vframe->sig_fmt;
			ppre->orientation = di_buf->vframe->video_angle;
			ppre->source_change_flag = 1;
			ppre->input_size_change_flag = true;
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
		} else if (ppre->cur_prog_flag == 0) {
			/* check if top/bot interleaved */
			if (change_type == 2)
				/* source is i interleaves p fields */
				ppre->force_interlace = true;
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
		if (dim_is_bypass(di_buf->vframe, channel)
			/*|| is_bypass_i_p()*/
			/*|| ((ppre->pre_ready_seq % 5)== 0)*/
			/*|| (ppre->pre_ready_seq == 10)*/
			) {
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

		if (di_bypass_state_get(channel) == 0) {
			if (ppre->di_mem_buf_dup_p) {
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

			dim_print("%s:bypass_state = 1, is_bypass() %d\n",
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
			return 0;
		} else if (is_progressive(di_buf->vframe)) {
			if (is_handle_prog_frame_as_interlace(vframe) &&
			    is_progressive(vframe)) {
				struct di_buf_s *di_buf_tmp = NULL;

				pvframe_in[di_buf->index] = NULL;
				di_buf->vframe->type &=
					(~VIDTYPE_TYPEMASK);
				di_buf->vframe->type |=
					VIDTYPE_INTERLACE_TOP;
				di_buf->post_proc_flag = 0;

		di_buf_tmp = di_que_out_to_di_buf(channel, QUE_IN_FREE);
		if (dim_check_di_buf(di_buf_tmp, 10, channel)) {
			recycle_vframe_type_pre(di_buf, channel);
			PR_ERR("DI:no free in_buffer for progressive skip.\n");
			return 0;
		}

				di_buf_tmp->vframe->private_data = di_buf_tmp;
				di_buf_tmp->seq = ppre->in_seq;
				ppre->in_seq++;
				pvframe_in[di_buf_tmp->index] = vframe;
				memcpy(di_buf_tmp->vframe, vframe,
				       sizeof(vframe_t));
				ppre->di_inp_buf_next = di_buf_tmp;
				di_buf_tmp->vframe->type &=
					(~VIDTYPE_TYPEMASK);
				di_buf_tmp->vframe->type |=
					VIDTYPE_INTERLACE_BOTTOM;
				di_buf_tmp->post_proc_flag = 0;

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
			/*********************************/
			if (di_buf->vframe->width >= 1920 &&
			    di_buf->vframe->height >= 1080)
				flg_1080i = true;
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
	}
	/*dim_dbg_pre_cnt(channel, "cfg");*/
	/* di_wr_buf */
	if (ppre->prog_proc_type == 2) {
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
			recycle_keep_buffer(channel);
			ppre->di_inp_buf_next = ppre->di_inp_buf;
			return 0;
		}
		di_buf->post_proc_flag = 0;
		di_buf->di_wr_linked_buf->pre_ref_count = 0;
		di_buf->di_wr_linked_buf->post_ref_count = 0;
		di_buf->canvas_config_flag = 1;
	} else {
		di_buf = get_di_buf_head(channel, QUEUE_LOCAL_FREE);
		if (dim_check_di_buf(di_buf, 11, channel)) {
			/* recycle_keep_buffer();
			 *  pr_dbg("%s:recycle keep buffer\n", __func__);
			 */
			recycle_vframe_type_pre(ppre->di_inp_buf, channel);
			return 0;
		}
		queue_out(channel, di_buf);/*QUEUE_LOCAL_FREE*/
		if (ppre->prog_proc_type & 0x10)
			di_buf->canvas_config_flag = 1;
		else
			di_buf->canvas_config_flag = 2;
		di_buf->di_wr_linked_buf = NULL;
	}

	ppre->di_wr_buf = di_buf;
	ppre->di_wr_buf->pre_ref_count = 1;

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
	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
	di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
	/* set vframe bit info */
	di_buf->vframe->bitdepth &= ~(BITDEPTH_YMASK);
	di_buf->vframe->bitdepth &= ~(FULL_PACK_422_MODE);
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
			di_buf->width_bk =
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
	if (ppre->prog_proc_type) {
		di_buf->vframe->type = VIDTYPE_PROGRESSIVE |
				       VIDTYPE_VIU_422 |
				       VIDTYPE_VIU_SINGLE_PLANE |
				       VIDTYPE_VIU_FIELD;
		if (ppre->cur_inp_type & VIDTYPE_PRE_INTERLACE)
			di_buf->vframe->type |= VIDTYPE_PRE_INTERLACE;
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
		return 0;
	}
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
		else
			get_ops_mtn()->com_patch_pre_sw_set(0);
	}
	return 1;
}

int dim_check_recycle_buf(unsigned int channel)
{
	struct di_buf_s *di_buf = NULL;/* , *ptmp; */
	int itmp;
	int ret = 0;
#ifdef DI_BUFFER_DEBUG
	int type;
	int index;
#endif
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (di_blocking)
		return ret;
	queue_for_each_entry(di_buf, channel, QUEUE_RECYCLE, list) {
		if (di_buf->pre_ref_count == 0 &&
		    di_buf->post_ref_count <= 0) {	/*ary maybe <=*/
			if (di_buf->type == VFRAME_TYPE_IN) {
				queue_out(channel, di_buf);
				if (pvframe_in[di_buf->index]) {
					pw_vf_put(pvframe_in[di_buf->index],
						  channel);
					pw_vf_notify_provider
						(channel,
						 VFRAME_EVENT_RECEIVER_PUT,
						 NULL);
					dim_print("%sch[%d]vfpt(%d)0x%p,%ums\n",
						  __func__, channel,
						  ppre->recycle_seq,
						  pvframe_in[di_buf->index],
						  jiffies_to_msecs(jiffies_64 -
						  pvframe_in[di_buf->index]->
						  ready_jiffies64));
					pvframe_in[di_buf->index] = NULL;
				}
				di_buf->invert_top_bot_flag = 0;

				di_que_in(channel, QUE_IN_FREE, di_buf);
				ppre->recycle_seq++;
				ret |= 1;
			} else {
				queue_out(channel, di_buf);
				di_buf->invert_top_bot_flag = 0;
				queue_in(channel, di_buf, QUEUE_LOCAL_FREE);
				di_buf->post_ref_count = 0;/*ary maybe*/
				if (di_buf->di_wr_linked_buf) {
					queue_in(channel,
						 di_buf->di_wr_linked_buf,
						 QUEUE_LOCAL_FREE);
#ifdef DI_BUFFER_DEBUG
					type = di_buf->di_wr_linked_buf->type;
					index = di_buf->di_wr_linked_buf->index;
					dim_print("%s:lk %s[%d]>recycle_list\n",
						  __func__,
						  vframe_type_name[type],
						  index);
#endif
					di_buf->di_wr_linked_buf = NULL;
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

/*get info for current field process by post*/
	ppre->di_wr_buf->curr_field_mcinfo.highvertfrqflg =
		(RD(MCDI_RO_HIGH_VERT_FRQ_FLG) & 0x1);
/* post:MCDI_MC_REL_GAIN_OFFST_0 */
	ppre->di_wr_buf->curr_field_mcinfo.motionparadoxflg =
		(RD(MCDI_RO_MOTION_PARADOX_FLG) & 0x1);
/* post:MCDI_MC_REL_GAIN_OFFST_0 */
	reg = ppre->di_wr_buf->curr_field_mcinfo.regs;
	for (i = 0; i < 26; i++) {
		ro_mcdi_col_cfd[i] = RD(0x2fb0 + i);
		ppre->di_wr_buf->curr_field_mcinfo.regs[i] = 0;
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

irqreturn_t dim_irq(int irq, void *dev_instance)
{
	unsigned int channel;
	struct di_pre_stru_s *ppre;
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct di_hpre_s  *pre = get_hw_pre();

#ifndef CHECK_DI_DONE
	unsigned int data32 = RD(DI_INTR_CTRL);
	unsigned int mask32 = (data32 >> 16) & 0x3ff;
	unsigned int flag = 0;

	channel = pre->curr_ch;
	ppre = pre->pres;

	data32 &= 0x3fffffff;
	if ((data32 & 1) == 0 && dimp_get(edi_mp_di_dbg_mask) & 8)
		pr_info("irq[%d]pre|post=0 write done.\n", irq);
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
	}

#else
	channel = pre->curr_ch;
	ppre = pre->pres;
#endif

#ifdef DET3D
	if (dimp_get(edi_mp_det3d_en)) {
		if ((data32 & 0x100) && !(mask32 & 0x100) && flag) {
			DIM_DI_WR(DI_INTR_CTRL, data32);
			det3d_irq(channel);
		} else {
			goto end;
		}
	} else {
		DIM_DI_WR(DI_INTR_CTRL, data32);
	}
#else
	if (flag) {
		hpre_gl_sw(false);
		DIM_DI_WR(DI_INTR_CTRL,
			  (data32 & 0xfffffffb) | (intr_mode << 30));
	}
#endif

	/*if (ppre->pre_de_busy == 0) {*/
	if (!di_pre_wait_irq_get()) {
		PR_ERR("%s:ch[%d]:enter:reg[0x%x]= 0x%x,dtab[%d]\n", __func__,
		       channel,
		       DI_INTR_CTRL,
		       RD(DI_INTR_CTRL),
		       pre->sdt_mode.op_crr);
		return IRQ_HANDLED;
	}

	if (flag) {
		ppre->irq_time[0] =
			(cur_to_msecs() - ppre->irq_time[0]);

		dim_tr_ops.pre(ppre->field_count_for_cont, ppre->irq_time[0]);

		/*add from valsi wang.feng*/
		dim_arb_sw(false);
		dim_arb_sw(true);
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

		ppre->pre_de_busy = 0;

		if (get_init_flag(channel))
			/* pr_dbg("%s:up di sema\n", __func__); */
			task_send_ready();

		pre->flg_int_done = 1;
	}

	return IRQ_HANDLED;
}

irqreturn_t dim_post_irq(int irq, void *dev_instance)
{
	unsigned int data32 = RD(DI_INTR_CTRL);
	unsigned int channel;
	struct di_post_stru_s *ppost;
	struct di_hpst_s  *pst = get_hw_pst();

	channel = pst->curr_ch;
	ppost = pst->psts;

	data32 &= 0x3fffffff;
	if ((data32 & 4) == 0) {
		if (dimp_get(edi_mp_di_dbg_mask) & 8)
			pr_info("irq[%d]post write undone.\n", irq);
			return IRQ_HANDLED;
	}

	if (pst->state != EDI_PST_ST_WAIT_INT) {
		PR_ERR("%s:ch[%d]:s[%d]\n", __func__, channel, pst->state);
		ddbg_sw(EDI_LOG_TYPE_MOD, false);
		return IRQ_HANDLED;
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
		DIM_DI_WR_REG_BITS(DI_POST_CTRL, 0, 7, 1);
		di_post_set_flow(1, EDI_POST_FLOW_STEP1_STOP);	/*dbg a*/
		dim_print("irq p ch[%d]done\n", channel);
		pst->flg_int_done = true;
	}
	dim_ddbg_mod_save(EDI_DBG_MOD_POST_IRQE, pst->curr_ch,
			  ppost->frame_cnt);

	if (get_init_flag(channel))
		task_send_ready();

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

#ifdef MARK_HIS	/*no use*/
static void vscale_skip_disable_post(struct di_buf_s *di_buf,
				     vframe_t *disp_vf, unsigned int channel)
{
	struct di_buf_s *di_buf_i = NULL;
	int canvas_height = di_buf->di_buf[0]->canvas_height;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	if (di_vscale_skip_enable & 0x2) {/* drop the bottom field */
		if (di_buf->di_buf_dup_p[0] && di_buf->di_buf_dup_p[1])
			di_buf_i =
				(di_buf->di_buf_dup_p[1]->vframe->type &
				 VIDTYPE_TYPEMASK) ==
				VIDTYPE_INTERLACE_TOP ? di_buf->di_buf_dup_p[1]
				: di_buf->di_buf_dup_p[0];
		else
			di_buf_i = di_buf->di_buf[0];
	} else {
		if (di_buf->di_buf[0]->post_proc_flag > 0 &&
		    di_buf->di_buf_dup_p[1])
			di_buf_i = di_buf->di_buf_dup_p[1];
		else
			di_buf_i = di_buf->di_buf[0];
	}
	disp_vf->type = di_buf_i->vframe->type;
	/* pr_dbg("%s (%x %x) (%x %x)\n", __func__,
	 * disp_vf, disp_vf->type, di_buf_i->vframe,
	 * di_buf_i->vframe->type);
	 */
	disp_vf->width = di_buf_i->vframe->width;
	disp_vf->height = di_buf_i->vframe->height;
	disp_vf->duration = di_buf_i->vframe->duration;
	disp_vf->pts = di_buf_i->vframe->pts;
	disp_vf->flag = di_buf_i->vframe->flag;
	disp_vf->canvas0Addr = di_post_idx[ppost->canvas_id][0];
	disp_vf->canvas1Addr = di_post_idx[ppost->canvas_id][0];
	canvas_config(di_post_idx[ppost->canvas_id][0],
		      di_buf_i->nr_adr, di_buf_i->canvas_width[NR_CANVAS],
		      canvas_height, 0, 0);
	dimh_disable_post_deinterlace_2();
	ppost->vscale_skip_flag = true;
}
#endif

/*early_process_fun*/
static int early_NONE(void)
{
	return 0;
}

int dim_do_post_wr_fun(void *arg, vframe_t *disp_vf)
{
	#ifdef MARK_HIS
	struct di_post_stru_s *ppost;
	int i;

	for (i = 0; i < DI_CHANNEL_NUB; i++) {
		ppost = get_post_stru(i);

		ppost->toggle_flag = true;
	}
	return 1;
	#else
	return early_NONE();
	#endif
}

static int de_post_disable_fun(void *arg, vframe_t *disp_vf)
{
	#ifdef MARK_HIS
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;
	unsigned int channel = di_buf->channel;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	ppost->vscale_skip_flag = false;
	ppost->toggle_flag = true;

	PR_ERR("%s------------------------------\n", __func__);

	process_vscale_skip(di_buf, disp_vf, channel);
/* for atv static image flickering */
	if (di_buf->process_fun_index == PROCESS_FUN_NULL)
		dimh_disable_post_deinterlace_2();

	return 1;
/* called for new_format_flag, make
 * video set video_property_changed
 */
	#else
	return early_NONE();
	#endif
}

static int do_nothing_fun(void *arg, vframe_t *disp_vf)
{
	#ifdef MARK_HIS
	struct di_buf_s *di_buf = (struct di_buf_s *)arg;
	unsigned int channel = di_buf->channel;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	ppost->vscale_skip_flag = false;
	ppost->toggle_flag = true;

	PR_ERR("%s------------------------------\n", __func__);

	process_vscale_skip(di_buf, disp_vf, channel);

	if (di_buf->process_fun_index == PROCESS_FUN_NULL) {
		if (RD(DI_IF1_GEN_REG) & 0x1 || RD(DI_POST_CTRL) & 0xf)
			dimh_disable_post_deinterlace_2();
	/*if(di_buf->pulldown_mode == PULL_DOWN_EI && RD(DI_IF1_GEN_REG)&0x1)
	 * DIM_VSYNC_WR_MPEG_REG(DI_IF1_GEN_REG, 0x3 << 30);
	 */
	}
	return 0;
	#else
	return early_NONE();
	#endif
}

static int do_pre_only_fun(void *arg, vframe_t *disp_vf)
{
	#ifdef MARK_HIS
	unsigned int channel;
	unsigned int tmptest;
	struct di_post_stru_s *ppost;

	PR_ERR("%s------------------------------\n", __func__);
#ifdef DI_USE_FIXED_CANVAS_IDX
	if (arg) {
		struct di_buf_s *di_buf = (struct di_buf_s *)arg;
		vframe_t *vf = di_buf->vframe;
		int width, canvas_height;

		channel = di_buf->channel;
		ppost = get_post_stru(channel);

		ppost->vscale_skip_flag = false;
		ppost->toggle_flag = true;

		if (!vf || !di_buf->di_buf[0]) {
			dim_print("error:%s,NULL point!!\n", __func__);
			return 0;
		}
		width = di_buf->di_buf[0]->canvas_width[NR_CANVAS];
		/* linked two interlace buffer should double height*/
		if (di_buf->di_buf[0]->di_wr_linked_buf)
			canvas_height =
			(di_buf->di_buf[0]->canvas_height << 1);
		else
			canvas_height =
			di_buf->di_buf[0]->canvas_height;
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		if (is_vsync_rdma_enable()) {
			ppost->canvas_id = ppost->next_canvas_id;
		} else {
			ppost->canvas_id = 0;
			ppost->next_canvas_id = 1;
			if (post_wr_en && post_wr_support)
				ppost->canvas_id = ppost->next_canvas_id;
		}
#endif

		canvas_config(di_post_idx[ppost->canvas_id][0],
			      di_buf->di_buf[0]->nr_adr,
			      di_buf->di_buf[0]->canvas_width[NR_CANVAS],
			      canvas_height, 0, 0);

		vf->canvas0Addr =
			di_post_idx[ppost->canvas_id][0];
		vf->canvas1Addr =
			di_post_idx[ppost->canvas_id][0];
#ifdef DET3D
		if (ppre->vframe_interleave_flag && di_buf->di_buf[1]) {
			tmptest = di_buf->di_buf[1]->canvas_width[NR_CANVAS];
			canvas_config(di_post_idx[ppost->canvas_id][1],
				      di_buf->di_buf[1]->nr_adr,
				      tmptest,
				      canvas_height, 0, 0);
			vf->canvas1Addr =
				di_post_idx[ppost->canvas_id][1];
			vf->duration <<= 1;
		}
#endif
		ppost->next_canvas_id = ppost->canvas_id ? 0 : 1;

		if (di_buf->process_fun_index == PROCESS_FUN_NULL) {
			if (RD(DI_IF1_GEN_REG) & 0x1 ||
			    RD(DI_POST_CTRL) & 0x10f)
				dimh_disable_post_deinterlace_2();
		}
	}
#endif

	return 0;
#else
	return early_NONE();
#endif
}

static void get_vscale_skip_count(unsigned int par)
{
	/*di_vscale_skip_count_real = (par >> 24) & 0xff;*/
	dimp_set(edi_mp_di_vscale_skip_real,
		 (par >> 24) & 0xff);
}

#define get_vpp_reg_update_flag(par) (((par) >> 16) & 0x1)

static unsigned int pldn_dly = 1;

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

	dimp_inc(edi_mp_post_cnt);
	if (ppost->vscale_skip_flag)
		return 0;

	get_vscale_skip_count(zoom_start_x_lines);

	if (IS_ERR_OR_NULL(di_buf))
		return 0;
	else if (IS_ERR_OR_NULL(di_buf->di_buf_dup_p[0]))
		return 0;

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
		dim_tr_ops.post_set(di_buf->vframe->omx_index);
	else
		return 0;
	/*dbg*/
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
	#ifdef MARK_HIS
	dimh_post_ctrl(DI_HW_POST_CTRL_INIT,
		       (post_wr_en && post_wr_support));
	#endif

	if (RD(DI_POST_SIZE) != ((di_width - 1) | ((di_height - 1) << 16)) ||
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
		/* bit mode config */
		if (di_buf->vframe->bitdepth & BITDEPTH_Y10) {
			if (di_buf->vframe->type & VIDTYPE_VIU_444) {
				ppost->di_buf0_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 2;
				ppost->di_buf1_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 2;
				ppost->di_buf2_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 2;
				ppost->di_diwr_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 2;

			} else {
				ppost->di_buf0_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 1;
				ppost->di_buf1_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 1;
				ppost->di_buf2_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 1;
				ppost->di_diwr_mif.bit_mode =
		(di_buf->vframe->bitdepth & FULL_PACK_422_MODE) ? 3 : 1;
			}
		} else {
			ppost->di_buf0_mif.bit_mode = 0;
			ppost->di_buf1_mif.bit_mode = 0;
			ppost->di_buf2_mif.bit_mode = 0;
			ppost->di_diwr_mif.bit_mode = 0;
		}
		if (di_buf->vframe->type & VIDTYPE_VIU_444) {
			ppost->di_buf0_mif.video_mode = 1;
			ppost->di_buf1_mif.video_mode = 1;
			ppost->di_buf2_mif.video_mode = 1;
		} else {
			ppost->di_buf0_mif.video_mode = 0;
			ppost->di_buf1_mif.video_mode = 0;
			ppost->di_buf2_mif.video_mode = 0;
		}
		if (ppost->buf_type == VFRAME_TYPE_IN &&
		    !(di_buf->di_buf_dup_p[0]->vframe->type &
		      VIDTYPE_VIU_FIELD)) {
			if (di_buf->vframe->type & VIDTYPE_VIU_NV21) {
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

		if (dimp_get(edi_mp_post_wr_en) &&
		    dimp_get(edi_mp_post_wr_support)) {
			if (de_devp->pps_enable &&
			    dimp_get(edi_mp_pps_position) == 0) {
				dim_pps_config(0, di_width, di_height,
					       dimp_get(edi_mp_pps_dstw),
					       dimp_get(edi_mp_pps_dsth));
				ppost->di_diwr_mif.start_x = 0;
				ppost->di_diwr_mif.end_x =
					dimp_get(edi_mp_pps_dstw) - 1;
				ppost->di_diwr_mif.start_y = 0;
				ppost->di_diwr_mif.end_y =
					dimp_get(edi_mp_pps_dsth) - 1;
			} else {
				ppost->di_diwr_mif.start_x = di_start_x;
				ppost->di_diwr_mif.end_x   = di_end_x;
				ppost->di_diwr_mif.start_y = di_start_y;
				ppost->di_diwr_mif.end_y   = di_end_y;
			}
		}

		ppost->di_mtnprd_mif.start_x = di_start_x;
		ppost->di_mtnprd_mif.end_x = di_end_x;
		ppost->di_mtnprd_mif.start_y = di_start_y >> 1;
		ppost->di_mtnprd_mif.end_y = di_end_y >> 1;
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
		}
		ppost->update_post_reg_flag = 1;
		/* if height decrease, mtn will not enough */
		if (di_buf->pd_config.global_mode != PULL_DOWN_BUF1 &&
		    !dimp_get(edi_mp_post_wr_en))
			di_buf->pd_config.global_mode = PULL_DOWN_EI;
	}

#ifdef DI_USE_FIXED_CANVAS_IDX
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (is_vsync_rdma_enable()) {
		ppost->canvas_id = ppost->next_canvas_id;
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
				  di_post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2], -1,
				  di_post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[0],
				  di_post_idx[ppost->canvas_id][1], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2],
				  di_post_idx[ppost->canvas_id][3], -1);
		if (dimp_get(edi_mp_mcpre_en)) {
			tmptest = di_post_idx[ppost->canvas_id][4];
			config_mcvec_canvas_idx(di_buf->di_buf_dup_p[2],
						tmptest);
		}
		break;
	case PULL_DOWN_BLEND_2:
	case PULL_DOWN_NORMAL_2:
		config_canvas_idx(di_buf->di_buf_dup_p[0],
				  di_post_idx[ppost->canvas_id][3], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[1],
				  di_post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2], -1,
				  di_post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[2],
				  di_post_idx[ppost->canvas_id][1], -1);
		if (dimp_get(edi_mp_mcpre_en)) {
			tmptest = di_post_idx[ppost->canvas_id][4];
			config_mcvec_canvas_idx(di_buf->di_buf_dup_p[2],
						tmptest);
		}
		break;
	case PULL_DOWN_MTN:
		config_canvas_idx(di_buf->di_buf_dup_p[1],
				  di_post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[2], -1,
				  di_post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[0],
				  di_post_idx[ppost->canvas_id][1], -1);
		break;
	case PULL_DOWN_BUF1:/* wave with buf1 */
		config_canvas_idx(di_buf->di_buf_dup_p[1],
				  di_post_idx[ppost->canvas_id][0], -1);
		config_canvas_idx(di_buf->di_buf_dup_p[1], -1,
				  di_post_idx[ppost->canvas_id][2]);
		config_canvas_idx(di_buf->di_buf_dup_p[0],
				  di_post_idx[ppost->canvas_id][1], -1);
		break;
	case PULL_DOWN_EI:
		if (di_buf->di_buf_dup_p[1])
			config_canvas_idx(di_buf->di_buf_dup_p[1],
					  di_post_idx[ppost->canvas_id][0],
					  -1);
		break;
	default:
		break;
	}
	ppost->next_canvas_id = ppost->canvas_id ? 0 : 1;
#endif
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
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		/*mc_pre_flag = is_meson_txl_cpu()?2:(overturn?0:1);*/
		if (is_meson_txl_cpu() && overturn) {
			/* swap if1&if2 mean negation of mv for normal di*/
			tmp_idx = ppost->di_buf1_mif.canvas0_addr0;
			ppost->di_buf1_mif.canvas0_addr0 =
				ppost->di_buf2_mif.canvas0_addr0;
			ppost->di_buf2_mif.canvas0_addr0 = tmp_idx;
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
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
		if (is_meson_txl_cpu() && overturn) {
			ppost->di_buf1_mif.canvas0_addr0 =
			ppost->di_buf2_mif.canvas0_addr0;
		}
		if (dimp_get(edi_mp_mcpre_en)) {
			ppost->di_mcvecrd_mif.canvas_num =
				di_buf->di_buf_dup_p[2]->mcvec_canvas_idx;
			mc_pre_flag = is_meson_txl_cpu() ? 0 :
				(overturn ? 1 : 0);
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
				invert_mv = true;
			else if (!overturn)
				ppost->di_buf2_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[2]->nr_canvas_idx;
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
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[2]->mtn_canvas_idx;
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
		ppost->di_mtnprd_mif.canvas_num =
			di_buf->di_buf_dup_p[1]->mtn_canvas_idx;
		ppost->di_buf1_mif.canvas0_addr0 =
			di_buf->di_buf_dup_p[0]->nr_canvas_idx;
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

	if (dimp_get(edi_mp_post_wr_en) && dimp_get(edi_mp_post_wr_support)) {
		config_canvas_idx(di_buf,
				  di_post_idx[ppost->canvas_id][5], -1);
		ppost->di_diwr_mif.canvas_num = di_buf->nr_canvas_idx;
		di_vpp_en = 0;
		di_ddr_en = 1;
	} else {
		di_vpp_en = 1;
		di_ddr_en = 0;
	}

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
	if (ppost->update_post_reg_flag) {
		dimh_enable_di_post_2(&ppost->di_buf0_mif,
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
				      dimp_get(edi_mp_di_vscale_skip_real)
				      );
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
		dimh_post_switch_buffer(&ppost->di_buf0_mif,
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
					dimp_get(edi_mp_di_vscale_skip_real)
					);
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
		    is_meson_sm1_cpu()) {
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
	post_mif_sw(true);	/*position?*/
	/*dimh_post_ctrl(DI_HW_POST_CTRL_RESET, di_ddr_en);*/
	/*add by wangfeng 2018-11-15*/
	DIM_VSC_WR_MPG_BT(DI_DIWR_CTRL, 1, 31, 1);
	DIM_VSC_WR_MPG_BT(DI_DIWR_CTRL, 0, 31, 1);
	/*ary add for post crash*/
	di_post_set_flow((dimp_get(edi_mp_post_wr_en)	&&
			  dimp_get(edi_mp_post_wr_support)),
			 EDI_POST_FLOW_STEP2_START);

	if (ppost->update_post_reg_flag > 0)
		ppost->update_post_reg_flag--;

	/*dbg*/
	dim_ddbg_mod_save(EDI_DBG_MOD_POST_SETE, channel, ppost->frame_cnt);
	dbg_post_cnt(channel, "ps2");
	ppost->frame_cnt++;

	return 0;
}

#ifndef DI_DEBUG_POST_BUF_FLOW
static void post_ready_buf_set(unsigned int ch, struct di_buf_s *di_buf)
{
	vframe_t *vframe_ret = NULL;
	struct di_buf_s *nr_buf = NULL;

	vframe_ret = di_buf->vframe;
	nr_buf = di_buf->di_buf_dup_p[1];
	if ((dimp_get(edi_mp_post_wr_en)	&&
	     dimp_get(edi_mp_post_wr_support))	&&
	    di_buf->process_fun_index != PROCESS_FUN_NULL) {
	#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
		vframe_ret->canvas0_config[0].phy_addr =
			di_buf->nr_adr;
		vframe_ret->canvas0_config[0].width =
			di_buf->canvas_width[NR_CANVAS],
		vframe_ret->canvas0_config[0].height =
			di_buf->canvas_height;
		vframe_ret->canvas0_config[0].block_mode = 0;
		vframe_ret->plane_num = 1;
		vframe_ret->canvas0Addr = -1;
		vframe_ret->canvas1Addr = -1;
		if (di_mp_uit_get(edi_mp_show_nrwr)) {
			vframe_ret->canvas0_config[0].phy_addr =
				nr_buf->nr_adr;
			vframe_ret->canvas0_config[0].width =
				nr_buf->canvas_width[NR_CANVAS];
			vframe_ret->canvas0_config[0].height =
				nr_buf->canvas_height;
		}
	#else
		config_canvas_idx(di_buf, di_wr_idx, -1);
		vframe_ret->canvas0Addr = di_buf->nr_canvas_idx;
		vframe_ret->canvas1Addr = di_buf->nr_canvas_idx;
		if (di_mp_uit_get(edi_mp_show_nrwr)) {
			config_canvas_idx(nr_buf,
					  di_wr_idx, -1);
			vframe_ret->canvas0Addr = di_wr_idx;
			vframe_ret->canvas1Addr = di_wr_idx;
		}
	#endif
		vframe_ret->early_process_fun = dim_do_post_wr_fun;
		vframe_ret->process_fun = NULL;

		/* 2019-04-22 Suggestions from brian.zhu*/
		vframe_ret->mem_handle = NULL;
		vframe_ret->type |= VIDTYPE_DI_PW;
		/* 2019-04-22 */
	}
}

#endif
void dim_post_de_done_buf_config(unsigned int channel)
{
	ulong irq_flag2 = 0;
	struct di_buf_s *di_buf = NULL;
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (!ppost->cur_post_buf) {
		PR_ERR("%s:no cur\n", __func__);
		return;
	}
	dbg_post_cnt(channel, "pd1");
	/*dbg*/
	dim_ddbg_mod_save(EDI_DBG_MOD_POST_DB, channel, ppost->frame_cnt);
	di_buf = ppost->cur_post_buf;

	di_lock_irqfiq_save(irq_flag2);
	queue_out(channel, ppost->cur_post_buf);/*? which que?post free*/

	if (de_devp->pps_enable && dimp_get(edi_mp_pps_position) == 0) {
		di_buf->vframe->width = dimp_get(edi_mp_pps_dstw);
		di_buf->vframe->height = dimp_get(edi_mp_pps_dsth);
	}

	#ifdef DI_DEBUG_POST_BUF_FLOW
	#else
	post_ready_buf_set(channel, di_buf);
	#endif
	di_que_in(channel, QUE_POST_READY, ppost->cur_post_buf);

	#ifdef DI_DEBUG_POST_BUF_FLOW
	#else
	/*add by ary:*/
	recycle_post_ready_local(ppost->cur_post_buf, channel);
	#endif
	di_unlock_irqfiq_restore(irq_flag2);
	dim_tr_ops.post_ready(di_buf->vframe->omx_index);
	pw_vf_notify_receiver(channel,
			      VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	ppost->cur_post_buf = NULL;
	/*dbg*/
	dim_ddbg_mod_save(EDI_DBG_MOD_POST_DE, channel, ppost->frame_cnt);
	dbg_post_cnt(channel, "pd2");
}

#ifdef MARK_HIS
static void di_post_process(unsigned int channel)
{
	struct di_buf_s *di_buf = NULL;
	vframe_t *vf_p = NULL;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	if (ppost->post_de_busy)
		return;
	if (queue_empty(channel, QUEUE_POST_DOING)) {
		ppost->post_peek_underflow++;
		return;
	}

	di_buf = get_di_buf_head(channel, QUEUE_POST_DOING);
	if (dim_check_di_buf(di_buf, 20, channel))
		return;
	vf_p = di_buf->vframe;
	if (ppost->run_early_proc_fun_flag) {
		if (vf_p->early_process_fun)
			vf_p->early_process_fun = dim_do_post_wr_fun;
	}
	if (di_buf->process_fun_index) {
		ppost->post_wr_cnt++;
		dim_post_process(di_buf, 0, vf_p->width - 1,
				 0, vf_p->height - 1, vf_p);
		ppost->post_de_busy = 1;
		ppost->irq_time = cur_to_msecs();
	} else {
		ppost->de_post_process_done = 1;
	}
	ppost->cur_post_buf = di_buf;
}
#endif

static void recycle_vframe_type_post(struct di_buf_s *di_buf,
				     unsigned int channel)
{
	int i;

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
		}
	}
	queue_out(channel, di_buf); /* remove it from display_list_head */
	if (di_buf->queue_index != -1) {
		PR_ERR("qout err:index[%d],typ[%d],qindex[%d]\n",
		       di_buf->index, di_buf->type, di_buf->queue_index);
	/* queue_out_dbg(channel, di_buf);*/
	}
	di_buf->invert_top_bot_flag = 0;
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
	ulong irq_flag2 = 0;
	int i = 0, drop_flag = 0;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	di_lock_irqfiq_save(irq_flag2);
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
		    dimp_get(edi_mp_post_wr_support))
			//queue_in(channel, di_buf, QUEUE_POST_DOING);
			di_que_in(channel, QUE_POST_DOING, di_buf);
		else
			di_que_in(channel, QUE_POST_READY, di_buf);

		dim_tr_ops.post_do(di_buf->vframe->omx_index);
		dim_print("di:ch[%d]:%dth %s[%d] => post ready %u ms.\n",
			  channel,
			  frame_count,
			  vframe_type_name[di_buf->type], di_buf->index,
			  jiffies_to_msecs(jiffies_64 -
			  di_buf->vframe->ready_jiffies64));
	}
	di_unlock_irqfiq_restore(irq_flag2);
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
	ulong irq_flag2 = 0;
	int i = 0;
	int tmp = 0;
	int tmp0 = 0;
	int tmp1 = 0;
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

#ifdef MARK_HIS
	/*for post write mode ,need reserved a post free buf;*/
	if (di_que_list_count(channel, QUE_POST_FREE) < 2)
		return 0;
#else
	if (di_que_is_empty(channel, QUE_POST_FREE))
		return 0;
	/*add : for now post buf only 3.*/
	//if (list_count(channel, QUEUE_POST_DOING) > 2)
	if (di_que_list_count(channel, QUE_POST_DOING) > 2)
		return 0;
#endif
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
	dim_print("%s:1 ready_count[%d]:post_proc_flag[%d]\n", __func__,
		  ready_count, ready_di_buf->post_proc_flag);
	if (ready_di_buf->post_proc_flag &&
	    ready_count >= buffer_keep_count) {
		i = 0;

		di_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);
		for (itmp = 0; itmp < psize; itmp++) {
			p = pw_qindex_2_buf(channel, tmpa[itmp]);
			/* if(p->post_proc_flag == 0){ */
			if (p->type == VFRAME_TYPE_IN) {
				ready_di_buf->post_proc_flag = -1;
				ready_di_buf->new_format_flag = 1;
			}
			i++;
			if (i > 2)
				break;
		}
	}
	if (ready_di_buf->post_proc_flag > 0) {
		if (ready_count >= buffer_keep_count) {
			di_lock_irqfiq_save(irq_flag2);

			di_buf = di_que_out_to_di_buf(channel, QUE_POST_FREE);
			if (dim_check_di_buf(di_buf, 17, channel)) {
				di_unlock_irqfiq_restore(irq_flag2);
				return 0;
			}

			di_unlock_irqfiq_restore(irq_flag2);

			i = 0;

			di_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

			for (itmp = 0; itmp < psize; itmp++) {
				p = pw_qindex_2_buf(channel, tmpa[itmp]);
				dim_print("di:keep[%d]:t[%d]:idx[%d]\n",
					  i, tmpa[itmp], p->index);
				di_buf->di_buf_dup_p[i++] = p;

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

			memcpy(di_buf->vframe,
			       di_buf->di_buf_dup_p[1]->vframe,
			       sizeof(vframe_t));
			di_buf->vframe->private_data = di_buf;
			if (di_buf->di_buf_dup_p[1]->post_proc_flag == 3) {
				/* dummy, not for display */
				inc_post_ref_count(di_buf);
				di_buf->di_buf[0] = di_buf->di_buf_dup_p[0];
				di_buf->di_buf[1] = NULL;
				queue_out(channel, di_buf->di_buf[0]);
				di_lock_irqfiq_save(irq_flag2);
				queue_in(channel, di_buf, QUEUE_TMP);
				recycle_vframe_type_post(di_buf, channel);

				di_unlock_irqfiq_restore(irq_flag2);
				dim_print("%s <dummy>: ", __func__);
#ifdef DI_BUFFER_DEBUG
				dim_print("%s <dummy>: ", __func__);
#endif
			} else {
				if (di_buf->di_buf_dup_p[1]->post_proc_flag
						== 2) {
					di_buf->pd_config.global_mode =
						PULL_DOWN_BLEND_2;
					/* blend with di_buf->di_buf_dup_p[2] */
				} else {
					set_pulldown_mode(di_buf, channel);
				}
				di_buf->vframe->type =
					VIDTYPE_PROGRESSIVE |
					VIDTYPE_VIU_422 |
					VIDTYPE_VIU_SINGLE_PLANE |
					VIDTYPE_VIU_FIELD |
					VIDTYPE_PRE_INTERLACE;

			di_buf->vframe->width =
				di_buf->di_buf_dup_p[1]->width_bk;

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
#ifdef DI_BUFFER_DEBUG
				dim_print("%s <interlace>: ", __func__);
#endif
				if (!(dimp_get(edi_mp_post_wr_en) &&
				      dimp_get(edi_mp_post_wr_support)))
					pw_vf_notify_receiver(channel,
VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
			}
			ret = 1;
		}
	} else {
		if (is_progressive(ready_di_buf->vframe) ||
		    ready_di_buf->type == VFRAME_TYPE_IN ||
		    ready_di_buf->post_proc_flag < 0 ||
		    dimp_get(edi_mp_bypass_post_state)
		    ){
			int vframe_process_count = 1;
#ifdef DET3D
			int dual_vframe_flag = 0;

			if ((ppre->vframe_interleave_flag &&
			     ready_di_buf->left_right) ||
			    (dimp_get(edi_mp_bypass_post) & 0x100)) {
				dual_vframe_flag = 1;
				vframe_process_count = 2;
			}
#endif
			if (dimp_get(edi_mp_skip_top_bot) &&
			    (!is_progressive(ready_di_buf->vframe)))
				vframe_process_count = 2;

			if (ready_count >= vframe_process_count) {
				struct di_buf_s *di_buf_i;

				di_lock_irqfiq_save(irq_flag2);

		di_buf = di_que_out_to_di_buf(channel, QUE_POST_FREE);
				if (dim_check_di_buf(di_buf, 19, channel)) {
					di_unlock_irqfiq_restore(irq_flag2);
					return 0;
				}

				di_unlock_irqfiq_restore(irq_flag2);

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

				di_buf->vframe->width = di_buf_i->width_bk;
				di_buf->vframe->private_data = di_buf;

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
				if (ready_di_buf->post_proc_flag == -2) {
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

#ifdef DET3D
				if (dual_vframe_flag) {
					di_buf->di_buf[1] =
						di_buf->di_buf_dup_p[1];
					queue_out(channel, di_buf->di_buf[1]);
				}
#endif
				tmp = di_buf->di_buf[0]->throw_flag;
				drop_frame(check_drop, tmp, di_buf, channel);

				frame_count++;
#ifdef DI_BUFFER_DEBUG
				dim_print("%s <prog by frame>: ", __func__);
#endif
				ret = 1;
				pw_vf_notify_receiver
					(channel,
					 VFRAME_EVENT_PROVIDER_VFRAME_READY,
					 NULL);
			}
		} else if (ready_count >= 2) {
			/*for progressive input,type
			 * 1:separate tow fields,type
			 * 2:bypass post as frame
			 */
			unsigned char prog_tb_field_proc_type =
				(dimp_get(edi_mp_prog_proc_config) >> 1) & 0x3;
			di_lock_irqfiq_save(irq_flag2);

			di_buf = di_que_out_to_di_buf(channel, QUE_POST_FREE);
			if (dim_check_di_buf(di_buf, 20, channel)) {
				di_unlock_irqfiq_restore(irq_flag2);
				return 0;
			}

			di_unlock_irqfiq_restore(irq_flag2);

			i = 0;

			di_que_list(channel, QUE_PRE_READY, &tmpa[0], &psize);

			for (itmp = 0; itmp < psize; itmp++) {
				p = pw_qindex_2_buf(channel, tmpa[itmp]);
				di_buf->di_buf_dup_p[i++] = p;
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

			memcpy(di_buf->vframe,
			       di_buf->di_buf_dup_p[0]->vframe,
			       sizeof(vframe_t));
			di_buf->vframe->private_data = di_buf;

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
			pw_vf_notify_receiver
				(channel,
				 VFRAME_EVENT_PROVIDER_VFRAME_READY,
				 NULL);
		}
	}

#ifdef DI_BUFFER_DEBUG
	if (di_buf) {
		dim_print("%s[%d](",
			  vframe_type_name[di_buf->type], di_buf->index);
		for (i = 0; i < 2; i++) {
			if (di_buf->di_buf[i]) {
				tmp = vframe_type_name[di_buf->di_buf[i]->type],
				dim_print("%s[%d]", tmp,
					  di_buf->di_buf[i]->index);
		}
		dim_print(")(vframe type %x dur %d)",
			  di_buf->vframe->type, di_buf->vframe->duration);
		if (di_buf->di_buf_dup_p[1] &&
		    di_buf->di_buf_dup_p[1]->post_proc_flag == 3)
			dim_print("=> recycle_list\n");
		else
			dim_print("=> post_ready_list\n");
	}
#endif
	return ret;
}

/*
 * di task
 */
void dim_unreg_process(unsigned int channel)
{
	unsigned long start_jiffes = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	pr_info("%s unreg start %d.\n", __func__, get_reg_flag(channel));
	if (get_reg_flag(channel)) {
		start_jiffes = jiffies_64;
		di_vframe_unreg(channel);
		pr_dbg("%s vf unreg cost %u ms.\n", __func__,
		       jiffies_to_msecs(jiffies_64 - start_jiffes));
		unreg_cnt++;
		if (unreg_cnt > 0x3fffffff)
			unreg_cnt = 0;
		pr_dbg("%s unreg stop %d.\n", __func__, get_reg_flag(channel));

	} else {
		ppre->force_unreg_req_flag = 0;
		ppre->disable_req_flag = 0;
		recovery_flag = 0;
	}
}

void di_unreg_setting(void)
{
	/*unsigned int mirror_disable = get_blackout_policy();*/
	unsigned int mirror_disable = 0;

	if (!get_hw_reg_flg()) {
		PR_ERR("%s:have setting?do nothing\n", __func__);
		return;
	}

	pr_info("%s:\n", __func__);
	/*set flg*/
	set_hw_reg_flg(false);

	dimh_enable_di_pre_mif(false, dimp_get(edi_mp_mcpre_en));
	post_close_new();	/*2018-11-29*/
	dimh_afbc_reg_sw(false);
	dimh_hw_uninit();
	if (is_meson_txlx_cpu()	||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()	||
	    is_meson_sm1_cpu()) {
		dim_pre_gate_control(false, dimp_get(edi_mp_mcpre_en));
		get_ops_nr()->nr_gate_control(false);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		DIM_DI_WR(DI_CLKG_CTRL, 0x80f60000);
		DIM_DI_WR(DI_PRE_CTRL, 0);
	} else {
		DIM_DI_WR(DI_CLKG_CTRL, 0xf60000);
	}
	/*ary add for switch to post wr, can't display*/
	pr_info("di: patch dimh_disable_post_deinterlace_2\n");
	dimh_disable_post_deinterlace_2();
	/* nr/blend0/ei0/mtn0 clock gate */

	dim_hw_disable(dimp_get(edi_mp_mcpre_en));

	if (is_meson_txlx_cpu() ||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_tm2_cpu()	||
	    is_meson_sm1_cpu()) {
		dimh_enable_di_post_mif(GATE_OFF);
		dim_post_gate_control(false);
		dim_top_gate_control(false, false);
	} else {
		DIM_DI_WR(DI_CLKG_CTRL, 0x80000000);
	}
	if (!is_meson_gxl_cpu()	&&
	    !is_meson_gxm_cpu()	&&
	    !is_meson_gxbb_cpu() &&
	    !is_meson_txlx_cpu())
		diext_clk_b_sw(false);
	pr_info("%s disable di mirror image.\n", __func__);

#ifdef MARK_HIS
	if (mirror_disable) {
		/*no mirror:*/
		if (dimp_get(edi_mp_post_wr_en) &&
		    dimp_get(edi_mp_post_wr_support))
			dim_set_power_control(0);

	} else {
		/*have mirror:*/
	}
#else	/*0624?*/
	if ((dimp_get(edi_mp_post_wr_en)	&&
	     dimp_get(edi_mp_post_wr_support))	||
	     mirror_disable) {
		/*diwr_set_power_control(0);*/
		hpst_mem_pd_sw(0);
	}
	if (mirror_disable)
		hpst_vd1_sw(0);
#endif

	#ifdef MARK_HIS	/*tmp*/
	if (post_wr_en && post_wr_support)
		dim_set_power_control(0);
	#endif

	disp_frame_count = 0;/* debug only*/
}

void di_unreg_variable(unsigned int channel)
{
	ulong irq_flag2 = 0;
	unsigned int mirror_disable = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();

#if (defined ENABLE_SPIN_LOCK_ALWAYS)
	ulong flags = 0;

	spin_lock_irqsave(&plist_lock, flags);
#endif
	pr_info("%s:\n", __func__);
	set_init_flag(channel, false);	/*init_flag = 0;*/
	dim_sumx_clear(channel);
	/*mirror_disable = get_blackout_policy();*/
	mirror_disable = 0;
	di_lock_irqfiq_save(irq_flag2);
	dim_print("%s: dim_uninit_buf\n", __func__);
	dim_uninit_buf(mirror_disable, channel);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (di_pre_rdma_enable)
		rdma_clear(de_devp->rdma_handle);
#endif
	get_ops_mtn()->adpative_combing_exit();

	di_unlock_irqfiq_restore(irq_flag2);

#if (defined ENABLE_SPIN_LOCK_ALWAYS)
	spin_unlock_irqrestore(&plist_lock, flags);
#endif
	dimh_patch_post_update_mc_sw(DI_MC_SW_REG, false);

	ppre->force_unreg_req_flag = 0;
	ppre->disable_req_flag = 0;
	recovery_flag = 0;
	ppre->cur_prog_flag = 0;

	if (cfgeq(MEM_FLAG, EDI_MEM_M_CMA)	||
	    cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_A)	||
	    cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_B))
		dip_wq_cma_run(channel, false);

	sum_g_clear(channel);
	sum_p_clear(channel);
	dbg_reg("%s:end\n", __func__);
}

#ifdef HIS_CODE
void dim_unreg_process_irq(unsigned int channel)
{
	ulong irq_flag2 = 0;
	unsigned int mirror_disable = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();

#if (defined ENABLE_SPIN_LOCK_ALWAYS)
	ulong flags = 0;

	spin_lock_irqsave(&plist_lock, flags);
#endif
	pr_info("%s:warn:not use\n", __func__);
	set_init_flag(channel, false);	/*init_flag = 0;*/
	mirror_disable = 1;	/*get_blackout_policy()*/;
	di_lock_irqfiq_save(irq_flag2);
	dim_print("%s: dim_uninit_buf\n", __func__);
	dim_uninit_buf(mirror_disable, channel);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (di_pre_rdma_enable)
		rdma_clear(de_devp->rdma_handle);
#endif
	get_ops_mtn()->adpative_combing_exit();
	dimh_enable_di_pre_mif(false, dimp_get(edi_mp_mcpre_en));
	dimh_afbc_reg_sw(false);
	dimh_hw_uninit();
	if (is_meson_txlx_cpu() ||
	    is_meson_txhd_cpu()	||
	    is_meson_g12a_cpu() ||
	    is_meson_g12b_cpu()	||
	    is_meson_tl1_cpu()	||
	    is_meson_sm1_cpu()	||
	    is_meson_tm2_cpu()) {
		dim_pre_gate_control(false, dimp_get(edi_mp_mcpre_en));
		get_ops_nr()->nr_gate_control(false);
	} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXTVBB)) {
		DIM_DI_WR(DI_CLKG_CTRL, 0x80f60000);
		DIM_DI_WR(DI_PRE_CTRL, 0);
	} else {
		DIM_DI_WR(DI_CLKG_CTRL, 0xf60000);
	}
	/*ary add for switch to post wr, can't display*/
	pr_info("di: patch dimh_disable_post_deinterlace_2\n");
	dimh_disable_post_deinterlace_2();

/* nr/blend0/ei0/mtn0 clock gate */
	if (mirror_disable) {
		dim_hw_disable(dimp_get(edi_mp_mcpre_en));
		if (is_meson_txlx_cpu()	||
		    is_meson_txhd_cpu()	||
		    is_meson_g12a_cpu()	||
		    is_meson_g12b_cpu()	||
		    is_meson_tl1_cpu()	||
		    is_meson_sm1_cpu()	||
		    is_meson_tm2_cpu()) {
			dimh_enable_di_post_mif(GATE_OFF);
			dim_post_gate_control(false);
			dim_top_gate_control(false, false);
		} else {
			DIM_DI_WR(DI_CLKG_CTRL, 0x80000000);
		}
		if (!is_meson_gxl_cpu()		&&
		    !is_meson_gxm_cpu()		&&
		    !is_meson_gxbb_cpu()	&&
		    !is_meson_txlx_cpu())
			diext_clk_b_sw(false);

		pr_info("%s disable di mirror image.\n", __func__);
	}
	if (dimp_get(edi_mp_post_wr_en)	&& dimp_get(edi_mp_post_wr_support))
		dim_set_power_control(0);
	di_unlock_irqfiq_restore(irq_flag2);

#if (defined ENABLE_SPIN_LOCK_ALWAYS)
	spin_unlock_irqrestore(&plist_lock, flags);
#endif
	dimh_patch_post_update_mc_sw(DI_MC_SW_REG, false);
	ppre->force_unreg_req_flag = 0;
	ppre->disable_req_flag = 0;
	recovery_flag = 0;
	ppre->cur_prog_flag = 0;

#ifdef MARK_HIS
#ifdef CONFIG_CMA
	if (de_devp->flag_cma == 1) {
		pr_dbg("%s:cma release req time: %d ms\n",
		       __func__, jiffies_to_msecs(jiffies));
		ppre->cma_release_req = 1;
		up(get_sema());
	}
#endif
#else
	if (de_devp->flag_cma == 1)
		dip_wq_cma_run(channel, false);

#endif
}
#endif

void dim_reg_process(unsigned int channel)
{
	/*get vout information first time*/
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (get_reg_flag(channel))
		return;
	di_vframe_reg(channel);

	ppre->bypass_flag = false;
	reg_cnt++;
	if (reg_cnt > 0x3fffffff)
		reg_cnt = 0;
	dim_print("########%s\n", __func__);
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

	if (atomic_read(&de_devp->pq_flag) == 0 &&
	    (de_devp->flags & DI_LOAD_REG_FLAG)) {
		atomic_set(&de_devp->pq_flag, 1);
		list_for_each_entry_safe(pos, tmp,
					 &de_devp->pq_table_list, list) {
			dimh_load_regs(pos);
			list_del(&pos->list);
			di_pq_parm_destroy(pos);
		}
		de_devp->flags &= ~DI_LOAD_REG_FLAG;
		atomic_set(&de_devp->pq_flag, 0);
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
		    is_meson_sm1_cpu())
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
	if (channel == 0 ||
	    (channel == 1 && !get_reg_flag(0)))
		di_load_pq_table();
	#ifdef OLD_PRE_GL
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		DIM_RDMA_WR(DI_PRE_GL_CTRL, 0x80000005);
	#endif
	if (de_devp->nrds_enable)
		dim_nr_ds_init(width, height);
	if (de_devp->pps_enable	&& dimp_get(edi_mp_pps_position)) {
		pps_w = ppre->cur_width;
		pps_h = ppre->cur_height >> 1;
		dim_pps_config(1, pps_w, pps_h,
			       dimp_get(edi_mp_pps_dstw),
			       (dimp_get(edi_mp_pps_dsth) >> 1));
	}
	if (is_meson_sm1_cpu() || is_meson_tm2_cpu()) {
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
}

static bool need_bypass(struct vframe_s *vf)
{
	if (vf->type & VIDTYPE_MVC)
		return true;

	if (vf->source_type == VFRAME_SOURCE_TYPE_PPMGR)
		return true;

	if (vf->type & VIDTYPE_VIU_444)
		return true;

	if (vf->type & VIDTYPE_PIC)
		return true;
#ifdef MARK_HIS
	if (vf->type & VIDTYPE_COMPRESS)
		return true;
#else
	/*support G12A and TXLX platform*/
	if (vf->type & VIDTYPE_COMPRESS) {
		if (!dimh_afbc_is_supported())
			return true;
		if ((vf->compHeight > (default_height + 8))	||
		    vf->compWidth > default_width)
			return true;
	}
#endif
	if (vf->width > default_width ||
	    (vf->height > (default_height + 8)))
		return true;

	if (vf_type_is_prog(vf->type)) {/*temp bypass p*/
		return true;
	}

	/*true bypass for 720p above*/
	if ((vf->flag & VFRAME_FLAG_GAME_MODE) &&
	    vf->width > 720)
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

	pr_info("%s:ch[%d]:for first ch reg:\n", __func__, channel);

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
		    is_meson_sm1_cpu()
		    ) {
			#ifdef CLK_TREE_SUPPORT
			clk_set_rate(de_devp->vpu_clkb,
				     de_devp->clkb_max_rate);
			#endif
		}

		dimh_enable_di_pre_mif(false, dimp_get(edi_mp_mcpre_en));
		dim_pre_gate_control(true, dimp_get(edi_mp_mcpre_en));
		dim_rst_protect(true);/*2019-01-22 by VLSI feng.wang*/
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
	post_mif_sw(false);
	post_dbg_contr();
	/*--------------------------*/

	nr_height = (vframe->height >> 1);/*temp*/
	/*--------------------------*/
	dimh_calc_lmv_init();
	first_field_type = (vframe->type & VIDTYPE_TYPEMASK);
	di_pre_size_change(vframe->width, nr_height,
			   first_field_type, channel);
	get_ops_nr()->cue_int(vframe);
	dim_ddbg_mod_save(EDI_DBG_MOD_REGE, channel, 0);

	/*--------------------------*/
	/*test*/
	dimh_int_ctr(0, 0, 0, 0, 0, 0);
}

/*********************************
 *
 * setting variable
 * from di_reg_process_irq
 *
 *********************************/
void di_reg_variable(unsigned int channel, struct vframe_s *vframe)
{
	ulong irq_flag2 = 0;
	#ifndef	RUN_DI_PROCESS_IN_IRQ
	ulong flags = 0;
	#endif
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (pre_run_flag != DI_RUN_FLAG_RUN &&
	    pre_run_flag != DI_RUN_FLAG_STEP)
		return;
	if (pre_run_flag == DI_RUN_FLAG_STEP)
		pre_run_flag = DI_RUN_FLAG_STEP_DONE;

	dbg_reg("%s:\n", __func__);

	dim_print("%s:0x%p\n", __func__, vframe);
	if (vframe) {
		dip_init_value_reg(channel);/*add 0404 for post*/
		dim_ddbg_mod_save(EDI_DBG_MOD_RVB, channel, 0);
		if (need_bypass(vframe)	||
		    ((dimp_get(edi_mp_di_debug_flag) >> 20) & 0x1)) {
			if (!ppre->bypass_flag) {
				pr_info("DI  %ux%u-0x%x.\n",
					vframe->width,
					vframe->height,
					vframe->type);
			}
			ppre->bypass_flag = true;
			dimh_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);
			return;
		}
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

		dim_print("%s: vframe come => di_init_buf\n", __func__);

		if (cfgeq(MEM_FLAG, EDI_MEM_M_REV) && !de_devp->mem_flg)
			dim_rev_mem_check();

		/*(is_progressive(vframe) && (prog_proc_config & 0x10)) {*/
		if (0) {
#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_lock_irqsave(&plist_lock, flags);
#endif
			di_lock_irqfiq_save(irq_flag2);
			/*
			 * 10 bit mode need 1.5 times buffer size of
			 * 8 bit mode, init the buffer size as 10 bit
			 * mode size, to make sure can switch bit mode
			 * smoothly.
			 */
			di_init_buf(default_width, default_height, 1, channel);

			di_unlock_irqfiq_restore(irq_flag2);

#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_unlock_irqrestore(&plist_lock, flags);
#endif
		} else {
#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_lock_irqsave(&plist_lock, flags);
#endif
			di_lock_irqfiq_save(irq_flag2);
			/*
			 * 10 bit mode need 1.5 times buffer size of
			 * 8 bit mode, init the buffer size as 10 bit
			 * mode size, to make sure can switch bit mode
			 * smoothly.
			 */
			di_init_buf(default_width, default_height, 0, channel);

			di_unlock_irqfiq_restore(irq_flag2);

#if (!(defined RUN_DI_PROCESS_IN_IRQ)) || (defined ENABLE_SPIN_LOCK_ALWAYS)
			spin_unlock_irqrestore(&plist_lock, flags);
#endif
		}

		ppre->mtn_status =
			get_ops_mtn()->adpative_combing_config
				(vframe->width, (vframe->height >> 1),
				 (vframe->source_type),
				 is_progressive(vframe),
				 vframe->sig_fmt);

		dimh_patch_post_update_mc_sw(DI_MC_SW_REG, true);
		di_sum_reg_init(channel);

		set_init_flag(channel, true);/*init_flag = 1;*/

		dim_ddbg_mod_save(EDI_DBG_MOD_RVE, channel, 0);
	}
}

/*para 1 not use*/

/*
 * provider/receiver interface
 */

/*************************/
int di_ori_event_unreg(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	pr_dbg("%s , is_bypass() %d trick_mode %d bypass_all %d\n",
	       __func__,
	       dim_is_bypass(NULL, channel),
	       trick_mode,
	       di_cfgx_get(channel, EDI_CFGX_BYPASS_ALL));
	/*dbg_ev("%s:unreg\n", __func__);*/
	dip_even_unreg_val(channel);	/*new*/

	ppre->vdin_source = false;

	dip_event_unreg_chst(channel);

#ifdef SUPPORT_MPEG_TO_VDIN
	if (mpeg2vdin_flag) {
		struct vdin_arg_s vdin_arg;
		struct vdin_v4l2_ops_s *vdin_ops = get_vdin_v4l2_ops();

		vdin_arg.cmd = VDIN_CMD_MPEGIN_STOP;
		if (vdin_ops->tvin_vdin_func)
			vdin_ops->tvin_vdin_func(0, &vdin_arg);

		mpeg2vdin_flag = 0;
	}
#endif
	di_bypass_state_set(channel, true);
#ifdef RUN_DI_PROCESS_IN_IRQ
	if (ppre->vdin_source)
		DIM_DI_WR_REG_BITS(VDIN_WR_CTRL, 0x3, 24, 3);
#endif

	dbg_ev("ch[%d]unreg end\n", channel);
	return 0;
}

int di_ori_event_reg(void *data, unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct vframe_receiver_s *preceiver;

	char *provider_name = (char *)data;
	const char *receiver_name = NULL;

	dip_even_reg_init_val(channel);
	if (de_devp->flags & DI_SUSPEND_FLAG) {
		PR_ERR("reg event device hasn't resumed\n");
		return -1;
	}
	if (get_reg_flag(channel)) {
		PR_ERR("no muti instance.\n");
		return -1;
	}
	dbg_ev("reg:%s\n", provider_name);

	di_bypass_state_set(channel, false);

	dip_event_reg_chst(channel);

	if (strncmp(provider_name, "vdin0", 4) == 0)
		ppre->vdin_source = true;
	else
		ppre->vdin_source = false;

	/*ary: need use interface api*/
	/*receiver_name = vf_get_receiver_name(VFM_NAME);*/
	preceiver = vf_get_receiver(di_rev_name[channel]);
	if (preceiver)
		receiver_name = preceiver->name;

	if (receiver_name) {
		if (!strcmp(receiver_name, "amvideo")) {
			ppost->run_early_proc_fun_flag = 0;
			if (dimp_get(edi_mp_post_wr_en)	&&
			    dimp_get(edi_mp_post_wr_support))
				ppost->run_early_proc_fun_flag = 1;
		} else {
			ppost->run_early_proc_fun_flag = 1;
			pr_info("set run_early_proc_fun_flag to 1\n");
		}
	} else {
		pr_info("%s error receiver is null.\n", __func__);
	}

	dbg_ev("ch[%d]:reg end\n", channel);
	return 0;
}

int di_ori_event_qurey_vdin2nr(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	return ppre->vdin2nr;
}

int di_ori_event_reset(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	int i;
	ulong flags;

	/*block*/
	di_blocking = 1;

	/*dbg_ev("%s: VFRAME_EVENT_PROVIDER_RESET\n", __func__);*/
	if (dim_is_bypass(NULL, channel)	||
	    di_bypass_state_get(channel)	||
	    ppre->bypass_flag) {
		pw_vf_notify_receiver(channel,
				      VFRAME_EVENT_PROVIDER_RESET,
			NULL);
	}

	spin_lock_irqsave(&plist_lock, flags);
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		if (pvframe_in[i])
			pr_dbg("DI:clear vframe_in[%d]\n", i);

		pvframe_in[i] = NULL;
	}
	spin_unlock_irqrestore(&plist_lock, flags);
	di_blocking = 0;

	return 0;
}

int di_ori_event_light_unreg(unsigned int channel)
{
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	int i;
	ulong flags;

	di_blocking = 1;

	pr_dbg("%s: vf_notify_receiver ligth unreg\n", __func__);

	spin_lock_irqsave(&plist_lock, flags);
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		if (pvframe_in[i])
			pr_dbg("DI:clear vframe_in[%d]\n", i);

		pvframe_in[i] = NULL;
	}
	spin_unlock_irqrestore(&plist_lock, flags);
	di_blocking = 0;

	return 0;
}

int di_ori_event_light_unreg_revframe(unsigned int channel)
{
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	int i;
	ulong flags;

	unsigned char vf_put_flag = 0;

	pr_info("%s:VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME\n",
		__func__);
/*
 * do not display garbage when 2d->3d or 3d->2d
 */
	spin_lock_irqsave(&plist_lock, flags);
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		if (pvframe_in[i]) {
			pw_vf_put(pvframe_in[i], channel);
			pr_dbg("DI:clear vframe_in[%d]\n", i);
			vf_put_flag = 1;
		}
		pvframe_in[i] = NULL;
	}
	if (vf_put_flag)
		pw_vf_notify_provider(channel,
				      VFRAME_EVENT_RECEIVER_PUT, NULL);

	spin_unlock_irqrestore(&plist_lock, flags);

	return 0;
}

int di_irq_ori_event_ready(unsigned int channel)
{
	return 0;
}

int di_ori_event_ready(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (ppre->bypass_flag)
		pw_vf_notify_receiver(channel,
				      VFRAME_EVENT_PROVIDER_VFRAME_READY,
				      NULL);

	if (dip_chst_get(channel) == EDI_TOP_STATE_REG_STEP1)
		task_send_cmd(LCMD1(ECMD_READY, channel));
	else
		task_send_ready();

	di_irq_ori_event_ready(channel);
	return 0;
}

int di_ori_event_qurey_state(unsigned int channel)
{
	/*int in_buf_num = 0;*/
	struct vframe_states states;

	if (recovery_flag)
		return RECEIVER_INACTIVE;

	/*fix for ucode reset method be break by di.20151230*/
	di_vf_l_states(&states, channel);
	if (states.buf_avail_num > 0)
		return RECEIVER_ACTIVE;

	if (pw_vf_notify_receiver(channel,
				  VFRAME_EVENT_PROVIDER_QUREY_STATE,
				  NULL) == RECEIVER_ACTIVE)
		return RECEIVER_ACTIVE;

	return RECEIVER_INACTIVE;
}

void  di_ori_event_set_3D(int type, void *data, unsigned int channel)
{
#ifdef DET3D

	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (type == VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE) {
		int flag = (long)data;

		ppre->vframe_interleave_flag = flag;
	}

#endif
}

/*************************/

/*recycle the buffer for keeping buffer*/
static void recycle_keep_buffer(unsigned int channel)
{
	ulong irq_flag2 = 0;
	int i = 0;
	int tmp = 0;
	struct di_buf_s *keep_buf;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	keep_buf = ppost->keep_buf;
	if (!IS_ERR_OR_NULL(keep_buf)) {
		PR_ERR("%s:\n", __func__);
		if (keep_buf->type == VFRAME_TYPE_POST) {
			pr_dbg("%s recycle keep cur di_buf %d (",
			       __func__, keep_buf->index);
			di_lock_irqfiq_save(irq_flag2);
			for (i = 0; i < USED_LOCAL_BUF_MAX; i++) {
				if (keep_buf->di_buf_dup_p[i]) {
					queue_in(channel,
						 keep_buf->di_buf_dup_p[i],
						 QUEUE_RECYCLE);
					tmp = keep_buf->di_buf_dup_p[i]->index;
					pr_dbg(" %d ", tmp);
				}
			}
			di_que_in(channel, QUE_POST_FREE, keep_buf);
			di_unlock_irqfiq_restore(irq_flag2);
			pr_dbg(")\n");
		}
		ppost->keep_buf = NULL;
	}
}

/************************************/
/************************************/
struct vframe_s *di_vf_l_get(unsigned int channel)
{
	vframe_t *vframe_ret = NULL;
	struct di_buf_s *di_buf = NULL;
#ifdef DI_DEBUG_POST_BUF_FLOW
	struct di_buf_s *nr_buf = NULL;
#endif

	ulong irq_flag2 = 0;
	struct di_post_stru_s *ppost = get_post_stru(channel);

	dim_print("%s:ch[%d]\n", __func__, channel);

	if (!get_init_flag(channel)	||
	    recovery_flag		||
	    di_blocking			||
	    !get_reg_flag(channel)	||
	    dump_state_flag) {
		dim_tr_ops.post_get2(1);
		return NULL;
	}

	/**************************/
	if (list_count(channel, QUEUE_DISPLAY) > DI_POST_GET_LIMIT) {
		dim_tr_ops.post_get2(2);
		return NULL;
	}
	/**************************/

	if (!di_que_is_empty(channel, QUE_POST_READY)) {
		dim_log_buffer_state("ge_", channel);
		di_lock_irqfiq_save(irq_flag2);

		di_buf = di_que_out_to_di_buf(channel, QUE_POST_READY);
		if (dim_check_di_buf(di_buf, 21, channel)) {
			di_unlock_irqfiq_restore(irq_flag2);
			return NULL;
		}
		/* add it into display_list */
		queue_in(channel, di_buf, QUEUE_DISPLAY);

		di_unlock_irqfiq_restore(irq_flag2);

		if (di_buf) {
			vframe_ret = di_buf->vframe;
#ifdef DI_DEBUG_POST_BUF_FLOW
	nr_buf = di_buf->di_buf_dup_p[1];
	if (dimp_get(edi_mp_post_wr_en)		&&
	    dimp_get(edi_mp_post_wr_support)	&&
	    di_buf->process_fun_index != PROCESS_FUN_NULL) {
		#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
		vframe_ret->canvas0_config[0].phy_addr =
			di_buf->nr_adr;
		vframe_ret->canvas0_config[0].width =
			di_buf->canvas_width[NR_CANVAS],
		vframe_ret->canvas0_config[0].height =
			di_buf->canvas_height;
		vframe_ret->canvas0_config[0].block_mode = 0;
		vframe_ret->plane_num = 1;
		vframe_ret->canvas0Addr = -1;
		vframe_ret->canvas1Addr = -1;
		if (di_mp_uit_get(edi_mp_show_nrwr)) {
			vframe_ret->canvas0_config[0].phy_addr =
				nr_buf->nr_adr;
			vframe_ret->canvas0_config[0].width =
				nr_buf->canvas_width[NR_CANVAS];
			vframe_ret->canvas0_config[0].height =
				nr_buf->canvas_height;
		}
		#else
		config_canvas_idx(di_buf, di_wr_idx, -1);
		vframe_ret->canvas0Addr = di_buf->nr_canvas_idx;
		vframe_ret->canvas1Addr = di_buf->nr_canvas_idx;
		if (di_mp_uit_get(edi_mp_show_nrwr)) {
			config_canvas_idx(nr_buf,
					  di_wr_idx, -1);
			vframe_ret->canvas0Addr = di_wr_idx;
			vframe_ret->canvas1Addr = di_wr_idx;
		}
		#endif
		vframe_ret->early_process_fun = dim_do_post_wr_fun;
		vframe_ret->process_fun = NULL;
	}
#endif
			di_buf->seq = disp_frame_count;
			atomic_set(&di_buf->di_cnt, 1);
		}
		disp_frame_count++;

		dim_log_buffer_state("get", channel);
	}
	if (vframe_ret) {
		dim_print("%s: %s[%d]:0x%p %u ms\n", __func__,
			  vframe_type_name[di_buf->type],
			  di_buf->index,
			  vframe_ret,
			  jiffies_to_msecs(jiffies_64 -
			  vframe_ret->ready_jiffies64));
		didbg_vframe_out_save(vframe_ret);

		dim_tr_ops.post_get(vframe_ret->omx_index);
	} else {
		dim_tr_ops.post_get2(3);
	}

	if (!dimp_get(edi_mp_post_wr_en)	&&
	    ppost->run_early_proc_fun_flag	&&
	    vframe_ret) {
		if (vframe_ret->early_process_fun == do_pre_only_fun)
			vframe_ret->early_process_fun(vframe_ret->private_data,
						      vframe_ret);
	}
	return vframe_ret;
}

void di_vf_l_put(struct vframe_s *vf, unsigned char channel)
{
	struct di_buf_s *di_buf = NULL;
	ulong irq_flag2 = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_post_stru_s *ppost = get_post_stru(channel);

	dim_print("%s:ch[%d]\n", __func__, channel);

	if (ppre->bypass_flag) {
		pw_vf_put(vf, channel);
		pw_vf_notify_provider(channel,
				      VFRAME_EVENT_RECEIVER_PUT, NULL);
		if (!IS_ERR_OR_NULL(ppost->keep_buf))
			recycle_keep_buffer(channel);
		return;
	}
/* struct di_buf_s *p = NULL; */
/* int itmp = 0; */
	if (!get_init_flag(channel)	||
	    recovery_flag		||
	    IS_ERR_OR_NULL(vf)) {
		PR_ERR("%s: 0x%p\n", __func__, vf);
		return;
	}
	if (di_blocking)
		return;
	dim_log_buffer_state("pu_", channel);
	di_buf = (struct di_buf_s *)vf->private_data;
	if (IS_ERR_OR_NULL(di_buf)) {
		pw_vf_put(vf, channel);
		pw_vf_notify_provider(channel,
				      VFRAME_EVENT_RECEIVER_PUT, NULL);
		PR_ERR("%s: get vframe %p without di buf\n",
		       __func__, vf);
		return;
	}

	if (di_buf->type == VFRAME_TYPE_POST) {
		di_lock_irqfiq_save(irq_flag2);

		if (is_in_queue(channel, di_buf, QUEUE_DISPLAY)) {
			di_buf->queue_index = -1;
			di_que_in(channel, QUE_POST_BACK, di_buf);
			di_unlock_irqfiq_restore(irq_flag2);
		} else {
			di_unlock_irqfiq_restore(irq_flag2);
			PR_ERR("%s:not in display %d\n",
			       __func__,
			       di_buf->index);
		}
	} else {
		di_lock_irqfiq_save(irq_flag2);
		queue_in(channel, di_buf, QUEUE_RECYCLE);
		di_unlock_irqfiq_restore(irq_flag2);

		dim_print("%s: %s[%d] =>recycle_list\n", __func__,
			  vframe_type_name[di_buf->type], di_buf->index);
	}

	task_send_ready();
}

void dim_recycle_post_back(unsigned int channel)
{
	struct di_buf_s *di_buf = NULL;
	ulong irq_flag2 = 0;
	unsigned int i = 0;

	if (di_que_is_empty(channel, QUE_POST_BACK))
		return;

	#ifdef DIM_HIS /*history*/
	while (pw_queue_out(channel, QUE_POST_BACK, &post_buf_index)) {
		/*pr_info("dp2:%d\n", post_buf_index);*/
		if (post_buf_index >= MAX_POST_BUF_NUM) {
			PR_ERR("%s:index is overlfow[%d]\n",
			       __func__, post_buf_index);
			break;
		}
		dim_print("di_back:%d\n", post_buf_index);
		di_lock_irqfiq_save(irq_flag2);	/**/

		di_buf = &pbuf_post[post_buf_index];
		if (!atomic_dec_and_test(&di_buf->di_cnt))
			PR_ERR("%s,di_cnt > 0\n", __func__);
		recycle_vframe_type_post(di_buf, channel);
		di_unlock_irqfiq_restore(irq_flag2);
	}
	#else
	while (i < MAX_FIFO_SIZE) {
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

		dim_print("di_back:%d\n", di_buf->index);
		di_lock_irqfiq_save(irq_flag2); /**/
		di_que_out(channel, QUE_POST_BACK, di_buf);
		di_buf->queue_index = QUEUE_DISPLAY;
		/*queue_out(channel, di_buf);*/

		if (!atomic_dec_and_test(&di_buf->di_cnt))
			PR_ERR("%s,di_cnt > 0\n", __func__);

		recycle_vframe_type_post(di_buf, channel);
		//di_buf->invert_top_bot_flag = 0;
		//di_que_in(channel, QUE_POST_FREE, di_buf);

		di_unlock_irqfiq_restore(irq_flag2);
	}

	#endif
	#ifdef DIM_HIS /*history*/
	/*back keep_buf:*/
	if (ppost->keep_buf_post) {
		PR_INF("ch[%d]:%s:%d\n",
		       channel,
		       __func__,
		       ppost->keep_buf_post->index);
		di_lock_irqfiq_save(irq_flag2);	/**/
		di_que_in(channel, QUE_POST_FREE, ppost->keep_buf_post);
		di_unlock_irqfiq_restore(irq_flag2);
		ppost->keep_buf_post = NULL;
	}
	#else
	if (di_cfg_top_get(EDI_CFG_KEEP_CLEAR_AUTO))
		dim_post_keep_release_all_2free(channel);
	#endif
}

struct vframe_s *di_vf_l_peek(unsigned int channel)
{
	struct vframe_s *vframe_ret = NULL;
	struct di_buf_s *di_buf = NULL;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	/*dim_print("%s:ch[%d]\n",__func__, channel);*/

	di_sum_inc(channel, EDI_SUM_O_PEEK_CNT);
	if (ppre->bypass_flag) {
		dim_tr_ops.post_peek(0);
		return pw_vf_peek(channel);
	}
	if (!get_init_flag(channel)	||
	    recovery_flag		||
	    di_blocking			||
	    !get_reg_flag(channel)	||
	    dump_state_flag) {
		dim_tr_ops.post_peek(1);
		return NULL;
	}

	/**************************/
	if (list_count(channel, QUEUE_DISPLAY) > DI_POST_GET_LIMIT) {
		dim_tr_ops.post_peek(2);
		return NULL;
	}
	/**************************/
	dim_log_buffer_state("pek", channel);

	if (!di_que_is_empty(channel, QUE_POST_READY)) {
		di_buf = di_que_peek(channel, QUE_POST_READY);
		if (di_buf)
			vframe_ret = di_buf->vframe;
	}
#ifdef DI_BUFFER_DEBUG
	if (vframe_ret)
		dim_print("%s: %s[%d]:%x\n", __func__,
			  vframe_type_name[di_buf->type],
			  di_buf->index, vframe_ret);
#endif
	if (vframe_ret) {
		dim_tr_ops.post_peek(9);
	} else {
		task_send_ready();
		dim_tr_ops.post_peek(4);
	}
	return vframe_ret;
}

int di_vf_l_states(struct vframe_states *states, unsigned int channel)
{
	struct di_mm_s *mm = dim_mm_get(channel);
	struct dim_sum_s *psumx = get_sumx(channel);

	/*pr_info("%s: ch[%d]\n", __func__, channel);*/
	if (!states)
		return -1;
	states->vf_pool_size = mm->sts.num_local;
	states->buf_free_num = psumx->b_pre_free;

	states->buf_avail_num = psumx->b_pst_ready;
	states->buf_recycle_num = psumx->b_recyc;
	if (dimp_get(edi_mp_di_dbg_mask) & 0x1) {
		di_pr_info("di-pre-ready-num:%d\n", psumx->b_pre_ready);
		di_pr_info("di-display-num:%d\n", psumx->b_display);
	}
	return 0;
}

/**********************************************/

/*****************************
 *	 di driver file_operations
 *
 ******************************/
#ifdef MARK_HIS /*move to di_sys.c*/
static int di_open(struct inode *node, struct file *file)
{
	di_dev_t *di_in_devp;

/* Get the per-device structure that contains this cdev */
	di_in_devp = container_of(node->i_cdev, di_dev_t, cdev);
	file->private_data = di_in_devp;

	return 0;
}

static int di_release(struct inode *node, struct file *file)
{
/* di_dev_t *di_in_devp = file->private_data; */

/* Reset file pointer */

/* Release some other fields */
	file->private_data = NULL;
	return 0;
}

#endif	/*move to di_sys.c*/

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

	mm_size = sizeof(struct am_pq_parm_s);
	if (copy_from_user(&tmp_pq_s, argp, mm_size)) {
		PR_ERR("set pq parm errors\n");
		return -EFAULT;
	}
	if (tmp_pq_s.table_len >= TABLE_LEN_MAX) {
		PR_ERR("load 0x%x wrong pq table_len.\n",
		       tmp_pq_s.table_len);
		return -EFAULT;
	}
	tab_flag = TABLE_NAME_DI | TABLE_NAME_NR | TABLE_NAME_MCDI |
		TABLE_NAME_DEBLOCK | TABLE_NAME_DEMOSQUITO;
	if (tmp_pq_s.table_name & tab_flag) {
		PR_INF("load 0x%x pq table len %u %s.\n",
		       tmp_pq_s.table_name, tmp_pq_s.table_len,
		       get_reg_flag_all() ? "directly" : "later");
	} else {
		PR_ERR("load 0x%x wrong pq table.\n",
		       tmp_pq_s.table_name);
		return -EFAULT;
	}
	di_pq_ptr = di_pq_parm_create(&tmp_pq_s);
	if (!di_pq_ptr) {
		PR_ERR("allocat pq parm struct error.\n");
		return -EFAULT;
	}
	argp = (void __user *)tmp_pq_s.table_ptr;
	mm_size = tmp_pq_s.table_len * sizeof(struct am_reg_s);
	if (copy_from_user(di_pq_ptr->regs, argp, mm_size)) {
		PR_ERR("user copy pq table errors\n");
		return -EFAULT;
	}
	if (get_reg_flag_all()) {
		dimh_load_regs(di_pq_ptr);
		di_pq_parm_destroy(di_pq_ptr);

		return ret;
	}
	if (atomic_read(&de_devp->pq_flag) == 0) {
		atomic_set(&de_devp->pq_flag, 1);
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
		atomic_set(&de_devp->pq_flag, 0);
	} else {
		PR_ERR("please retry table name 0x%x.\n",
		       di_pq_ptr->pq_parm.table_name);
		di_pq_parm_destroy(di_pq_ptr);
		ret = -EFAULT;
	}

	return ret;
}

unsigned int rd_reg_bits(unsigned int adr, unsigned int start,
			 unsigned int len)
{
	return ((aml_read_vcbus(adr) &
		(((1UL << (len)) - 1UL) << (start))) >> (start));
}

#ifdef MARK_HIS /*#ifdef CONFIG_AMLOGIC_MEDIA_RDMA*/
unsigned int DIM_RDMA_RD_BITS(unsigned int adr, unsigned int start,
			      unsigned int len)
{
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (de_devp->rdma_handle && di_pre_rdma_enable)
		return rdma_read_reg(de_devp->rdma_handle, adr) &
		       (((1 << len) - 1) << start);
	else
		return rd_reg_bits(adr, start, len);
}

unsigned int DIM_RDMA_WR(unsigned int adr, unsigned int val)
{
	unsigned int channel = get_current_channel();
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (is_need_stop_reg(adr))
		return 0;

	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable) {
		if (ppre->field_count_for_cont < 1)
			DIM_DI_WR(adr, val);
		else
			rdma_write_reg(de_devp->rdma_handle, adr, val);
		return 0;
	}

	DIM_DI_WR(adr, val);
	return 1;
}

unsigned int DIM_RDMA_RD(unsigned int adr)
{
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable)
		return rdma_read_reg(de_devp->rdma_handle, adr);
	else
		return RD(adr);
}

unsigned int DIM_RDMA_WR_BITS(unsigned int adr, unsigned int val,
			      unsigned int start, unsigned int len)
{
	unsigned int channel = get_current_channel();
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (is_need_stop_reg(adr))
		return 0;

	if (de_devp->rdma_handle > 0 && di_pre_rdma_enable) {
		if (ppre->field_count_for_cont < 1)
			DIM_DI_WR_REG_BITS(adr, val, start, len);
		else
			rdma_write_reg_bits(de_devp->rdma_handle,
					    adr, val, start, len);
		return 0;
	}
	DIM_DI_WR_REG_BITS(adr, val, start, len);
	return 1;
}
#else
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
#endif

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
	    is_meson_sm1_cpu()) {
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
		    is_meson_sm1_cpu()) {
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
			 is_meson_sm1_cpu()) ? 10 : 17);
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
	pr_info("DI: vpu clkb <%lu, %lu>\n", pdev->clkb_min_rate,
		pdev->clkb_max_rate);
	#ifdef CLK_TREE_SUPPORT
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
MODULE_DESCRIPTION("AMLOGIC DEINTERLACE driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("4.0.0");
#endif
