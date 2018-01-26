/*
 * drivers/amlogic/amports/vh264mvc.c
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
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/amports/ptsserv.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/canvas/canvas_mgr.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include <linux/amlogic/amports/vformat.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/atomic.h>

#include <linux/module.h>
#include <linux/slab.h>
#include "amports_priv.h"


#include "vdec.h"
#include "vdec_reg.h"

#include "amvdec.h"

#define TIME_TASK_PRINT_ENABLE  0x100
#define PUT_PRINT_ENABLE    0x200

#define DRIVER_NAME "amvdec_h264mvc"
#define MODULE_NAME "amvdec_h264mvc"

#define HANDLE_h264mvc_IRQ

#define DEBUG_PTS
#define DEBUG_SKIP

#define PUT_INTERVAL        (HZ/100)

#define STAT_TIMER_INIT     0x01
#define STAT_MC_LOAD        0x02
#define STAT_ISR_REG        0x04
#define STAT_VF_HOOK        0x08
#define STAT_TIMER_ARM      0x10
#define STAT_VDEC_RUN       0x20

#define DROPPING_THREAD_HOLD    4
#define DROPPING_FIRST_WAIT     16
#define DISPLAY_INVALID_POS    -65536

#define INIT_DROP_FRAME_CNT    8

static int vh264mvc_vf_states(struct vframe_states *states, void *);
static struct vframe_s *vh264mvc_vf_peek(void *);
static struct vframe_s *vh264mvc_vf_get(void *);
static void vh264mvc_vf_put(struct vframe_s *, void *);
static int vh264mvc_event_cb(int type, void *data, void *private_data);

static void vh264mvc_prot_init(void);
static void vh264mvc_local_init(void);
static void vh264mvc_put_timer_func(unsigned long arg);

static const char vh264mvc_dec_id[] = "vh264mvc-dev";

#define PROVIDER_NAME   "decoder.h264mvc"
static struct vdec_info *gvs;

static const struct vframe_operations_s vh264mvc_vf_provider = {
	.peek = vh264mvc_vf_peek,
	.get = vh264mvc_vf_get,
	.put = vh264mvc_vf_put,
	.event_cb = vh264mvc_event_cb,
	.vf_states = vh264mvc_vf_states,
};

static struct vframe_provider_s vh264mvc_vf_prov;

static u32 frame_width, frame_height, frame_dur;
static u32 saved_resolution;
static struct timer_list recycle_timer;
static u32 stat;
static u32 pts_outside;
static u32 sync_outside;
static u32 vh264mvc_ratio;
static u32 h264mvc_ar;
static u32 no_dropping_cnt;
static s32 init_drop_cnt;

#ifdef DEBUG_SKIP
static unsigned long view_total, view_dropped;
#endif

#ifdef DEBUG_PTS
static unsigned long pts_missed, pts_hit;
#endif

static atomic_t vh264mvc_active = ATOMIC_INIT(0);
static struct work_struct error_wd_work;

static struct dec_sysinfo vh264mvc_amstream_dec_info;
static dma_addr_t mc_dma_handle;
static void *mc_cpu_addr;

static DEFINE_SPINLOCK(lock);

static int vh264mvc_stop(void);
static s32 vh264mvc_init(void);

/***************************
*   new
***************************/

/* bit[3:0] command : */
/* 0 - command finished */
/* (DATA0 - {level_idc_mmco, max_reference_frame_num, width, height} */
/* 1 - alloc view_0 display_buffer and reference_data_area */
/* 2 - alloc view_1 display_buffer and reference_data_area */
#define MAILBOX_COMMAND         AV_SCRATCH_0
#define MAILBOX_DATA_0          AV_SCRATCH_1
#define MAILBOX_DATA_1          AV_SCRATCH_2
#define MAILBOX_DATA_2          AV_SCRATCH_3
#define CANVAS_START            AV_SCRATCH_6
#define BUFFER_RECYCLE          AV_SCRATCH_7
#define DROP_CONTROL            AV_SCRATCH_8
#define PICTURE_COUNT           AV_SCRATCH_9
#define DECODE_STATUS           AV_SCRATCH_A
#define SPS_STATUS              AV_SCRATCH_B
#define PPS_STATUS              AV_SCRATCH_C
#define SIM_RESERV_D            AV_SCRATCH_D
#define WORKSPACE_START         AV_SCRATCH_E
#define SIM_RESERV_F            AV_SCRATCH_F
#define DECODE_ERROR_CNT        AV_SCRATCH_G
#define CURRENT_UCODE           AV_SCRATCH_H
#define CURRENT_SPS_PPS         AV_SCRATCH_I/* bit[15:9]-SPS, bit[8:0]-PPS */
#define DECODE_SKIP_PICTURE     AV_SCRATCH_J
#define UCODE_START_ADDR        AV_SCRATCH_K
#define SIM_RESERV_L            AV_SCRATCH_L
#define REF_START_VIEW_0        AV_SCRATCH_M
#define REF_START_VIEW_1        AV_SCRATCH_N

/********************************************
 *  Mailbox command
 ********************************************/
#define CMD_FINISHED               0
#define CMD_ALLOC_VIEW_0           1
#define CMD_ALLOC_VIEW_1           2
#define CMD_FRAME_DISPLAY          3
#define CMD_FATAL_ERROR            4

#define CANVAS_INDEX_START       0x78
/* /AMVDEC_H264MVC_CANVAS_INDEX */

#define MC_TOTAL_SIZE        (28*SZ_1K)
#define MC_SWAP_SIZE         (4*SZ_1K)

unsigned DECODE_BUFFER_START = 0x00200000;
unsigned DECODE_BUFFER_END = 0x05000000;

#define DECODE_BUFFER_NUM_MAX    16
#define DISPLAY_BUFFER_NUM         4

static unsigned int ANC_CANVAS_ADDR;
static unsigned int index;
static unsigned int dpb_start_addr[3];
static unsigned int ref_start_addr[2];
static unsigned int max_dec_frame_buffering[2];
static unsigned int total_dec_frame_buffering[2];
static unsigned int level_idc, max_reference_frame_num, mb_width, mb_height;
static unsigned int dpb_size, ref_size;

static int display_buff_id;
static int display_view_id;
static int display_POC;
static int stream_offset;

#define video_domain_addr(adr) (adr&0x7fffffff)
static unsigned work_space_adr;
static unsigned work_space_size = 0xa0000;

struct buffer_spec_s {
	unsigned int y_addr;
	unsigned int u_addr;
	unsigned int v_addr;

	int y_canvas_index;
	int u_canvas_index;
	int v_canvas_index;
};
static struct buffer_spec_s buffer_spec0[DECODE_BUFFER_NUM_MAX +
			DISPLAY_BUFFER_NUM];
static struct buffer_spec_s buffer_spec1[DECODE_BUFFER_NUM_MAX +
			DISPLAY_BUFFER_NUM];

/*
    dbg_mode:
    bit 0: 1, print debug information
    bit 4: 1, recycle buffer without displaying;
    bit 5: 1, buffer single frame step , set dbg_cmd to 1 to step

*/
static int dbg_mode;
static int dbg_cmd;
static int view_mode =
	3;	/* 0, left; 1 ,right ; 2, left<->right 3, right<->left */
static int drop_rate = 2;
static int drop_thread_hold;
/**/
#define MVC_BUF_NUM     (DECODE_BUFFER_NUM_MAX+DISPLAY_BUFFER_NUM)
struct mvc_buf_s {
	struct list_head list;
	struct vframe_s vframe;
	int display_POC;
	int view0_buff_id;
	int view1_buff_id;
	int view0_drop;
	int view1_drop;
	int stream_offset;
	unsigned pts;
} /*mvc_buf_t */;

#define spec2canvas(x)  \
	(((x)->v_canvas_index << 16) | \
	 ((x)->u_canvas_index << 8)  | \
	 ((x)->y_canvas_index << 0))

#define to_mvcbuf(vf)   \
	container_of(vf, struct mvc_buf_s, vframe)

static int vf_buf_init_flag;

static void init_vf_buf(void)
{

	vf_buf_init_flag = 1;
}

static void uninit_vf_buf(void)
{

}

/* #define QUEUE_SUPPORT */

struct mvc_info_s {
	int view0_buf_id;
	int view1_buf_id;
	int view0_drop;
	int view1_drop;
	int display_pos;
	int used;
	int slot;
	unsigned stream_offset;
};

#define VF_POOL_SIZE        20
static struct vframe_s vfpool[VF_POOL_SIZE];
static struct mvc_info_s vfpool_idx[VF_POOL_SIZE];
static s32 view0_vfbuf_use[DECODE_BUFFER_NUM_MAX];
static s32 view1_vfbuf_use[DECODE_BUFFER_NUM_MAX];

static s32 fill_ptr, get_ptr, putting_ptr, put_ptr;
static s32 dirty_frame_num;
static s32 enable_recycle;

static s32 init_drop_frame_id[INIT_DROP_FRAME_CNT];
#define INCPTR(p) ptr_atomic_wrap_inc(&p)
static inline void ptr_atomic_wrap_inc(u32 *ptr)
{
	u32 i = *ptr;

	i++;

	if (i >= VF_POOL_SIZE)
		i = 0;

	*ptr = i;
}

static void set_frame_info(struct vframe_s *vf)
{
	unsigned int ar = 0;

	vf->width = frame_width;
	vf->height = frame_height;
	vf->duration = frame_dur;
	vf->duration_pulldown = 0;

	if (vh264mvc_ratio == 0) {
		/* always stretch to 16:9 */
		vf->ratio_control |= (0x90 <<
				DISP_RATIO_ASPECT_RATIO_BIT);
	} else {
		/* h264mvc_ar = ((float)frame_height/frame_width)
		 *customer_ratio; */
		ar =  min_t(u32, h264mvc_ar, DISP_RATIO_ASPECT_RATIO_MAX);

		vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);
	}

	return;
}

static int vh264mvc_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;
	int i;
	spin_lock_irqsave(&lock, flags);
	states->vf_pool_size = VF_POOL_SIZE;

	i = put_ptr - fill_ptr;
	if (i < 0)
		i += VF_POOL_SIZE;
	states->buf_free_num = i;

	i = putting_ptr - put_ptr;
	if (i < 0)
		i += VF_POOL_SIZE;
	states->buf_recycle_num = i;

	i = fill_ptr - get_ptr;
	if (i < 0)
		i += VF_POOL_SIZE;
	states->buf_avail_num = i;

	spin_unlock_irqrestore(&lock, flags);
	return 0;
}

void send_drop_cmd(void)
{
	int ready_cnt = 0;
	int temp_get_ptr = get_ptr;
	int temp_fill_ptr = fill_ptr;
	while (temp_get_ptr != temp_fill_ptr) {
		if ((vfpool_idx[temp_get_ptr].view0_buf_id >= 0)
			&& (vfpool_idx[temp_get_ptr].view1_buf_id >= 0)
			&& (vfpool_idx[temp_get_ptr].view0_drop == 0)
			&& (vfpool_idx[temp_get_ptr].view1_drop == 0))
			ready_cnt++;
		INCPTR(temp_get_ptr);
	}
	if (dbg_mode & 0x40) {
		pr_info("ready_cnt is %d ; no_dropping_cnt is %d\n", ready_cnt,
			   no_dropping_cnt);
	}
	if ((no_dropping_cnt >= DROPPING_FIRST_WAIT)
		&& (ready_cnt < drop_thread_hold))
		WRITE_VREG(DROP_CONTROL, (1 << 31) | (drop_rate));
	else
		WRITE_VREG(DROP_CONTROL, 0);
}

#if 0
int get_valid_frame(void)
{
	int ready_cnt = 0;
	int temp_get_ptr = get_ptr;
	int temp_fill_ptr = fill_ptr;
	while (temp_get_ptr != temp_fill_ptr) {
		if ((vfpool_idx[temp_get_ptr].view0_buf_id >= 0)
			&& (vfpool_idx[temp_get_ptr].view1_buf_id >= 0)
			&& (vfpool_idx[temp_get_ptr].view0_drop == 0)
			&& (vfpool_idx[temp_get_ptr].view1_drop == 0))
			ready_cnt++;
		INCPTR(temp_get_ptr);
	}
	return ready_cnt;
}
#endif
static struct vframe_s *vh264mvc_vf_peek(void *op_arg)
{

	if (get_ptr == fill_ptr)
		return NULL;
	send_drop_cmd();
	return &vfpool[get_ptr];

}

static struct vframe_s *vh264mvc_vf_get(void *op_arg)
{

	struct vframe_s *vf;
	int view0_buf_id;
	int view1_buf_id;
	if (get_ptr == fill_ptr)
		return NULL;

	view0_buf_id = vfpool_idx[get_ptr].view0_buf_id;
	view1_buf_id = vfpool_idx[get_ptr].view1_buf_id;
	vf = &vfpool[get_ptr];

	if ((view0_buf_id >= 0) && (view1_buf_id >= 0)) {
		if (view_mode == 0 || view_mode == 1) {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
			vf->canvas0Addr = vf->canvas1Addr =
				(view_mode ==
				 0) ? spec2canvas(&buffer_spec0[view0_buf_id]) :
				spec2canvas(&buffer_spec1[view1_buf_id]);
		} else {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_MVC;

			vf->left_eye.start_x = 0;
			vf->left_eye.start_y = 0;
			vf->left_eye.width = vf->width;
			vf->left_eye.height = vf->height;
			vf->right_eye.start_x = 0;
			vf->right_eye.start_y = 0;
			vf->right_eye.width = vf->width;
			vf->right_eye.height = vf->height;
			vf->trans_fmt = TVIN_TFMT_3D_TB;

			if (view_mode == 2) {
				vf->canvas0Addr =
					spec2canvas(&buffer_spec1[
							view1_buf_id]);
				vf->canvas1Addr =
					spec2canvas(&buffer_spec0[
							view0_buf_id]);
			} else {
				vf->canvas0Addr =
					spec2canvas(&buffer_spec0[
							view0_buf_id]);
				vf->canvas1Addr =
					spec2canvas(&buffer_spec1[
							view1_buf_id]);
			}
		}
	}
	vf->type_original = vf->type;
	if (((vfpool_idx[get_ptr].view0_drop != 0)
		 || (vfpool_idx[get_ptr].view1_drop != 0))
		&& ((no_dropping_cnt >= DROPPING_FIRST_WAIT)))
		vf->frame_dirty = 1;
	else
		vf->frame_dirty = 0;

	INCPTR(get_ptr);

	if (vf) {
		if (frame_width == 0)
			frame_width = vh264mvc_amstream_dec_info.width;
		if (frame_height == 0)
			frame_height = vh264mvc_amstream_dec_info.height;

		vf->width = frame_width;
		vf->height = frame_height;
	}
	if ((no_dropping_cnt < DROPPING_FIRST_WAIT) && (vf->frame_dirty == 0))
		no_dropping_cnt++;
	return vf;

}

static void vh264mvc_vf_put(struct vframe_s *vf, void *op_arg)
{

	if (vf_buf_init_flag == 0)
		return;
	if (vf->frame_dirty) {

		vf->frame_dirty = 0;
		dirty_frame_num++;
		enable_recycle = 0;
		if (dbg_mode & PUT_PRINT_ENABLE) {
			pr_info("invalid: dirty_frame_num is !!! %d\n",
				   dirty_frame_num);
		}
	} else {
		INCPTR(putting_ptr);
		while (dirty_frame_num > 0) {
			INCPTR(putting_ptr);
			dirty_frame_num--;
		}
		enable_recycle = 1;
		if (dbg_mode & PUT_PRINT_ENABLE) {
			pr_info("valid: dirty_frame_num is @@@ %d\n",
				   dirty_frame_num);
		}
		/* send_drop_cmd(); */
	}

}

static int vh264mvc_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_RESET) {
		unsigned long flags;
		amvdec_stop();
#ifndef CONFIG_POST_PROCESS_MANAGER
		vf_light_unreg_provider(&vh264mvc_vf_prov);
#endif
		spin_lock_irqsave(&lock, flags);
		vh264mvc_local_init();
		vh264mvc_prot_init();
		spin_unlock_irqrestore(&lock, flags);
#ifndef CONFIG_POST_PROCESS_MANAGER
		vf_reg_provider(&vh264mvc_vf_prov);
#endif
		amvdec_start();
	}
	return 0;
}

/**/
static long init_canvas(int start_addr, long dpb_size, int dpb_number,
		int mb_width, int mb_height,
		struct buffer_spec_s *buffer_spec)
{

	int dpb_addr, addr;
	int i;
	int mb_total;

	/* cav_con canvas; */

	dpb_addr = start_addr;

	mb_total = mb_width * mb_height;

	for (i = 0; i < dpb_number; i++) {
		WRITE_VREG(ANC_CANVAS_ADDR,
				index | ((index + 1) << 8) |
				((index + 2) << 16));
		ANC_CANVAS_ADDR++;

		addr = dpb_addr;
		buffer_spec[i].y_addr = addr;
		buffer_spec[i].y_canvas_index = index;
		canvas_config(index,
				addr,
				mb_width << 4,
				mb_height << 4,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);

		addr += mb_total << 8;
		index++;
		buffer_spec[i].u_addr = addr;
		buffer_spec[i].u_canvas_index = index;
		canvas_config(index,
				addr,
				mb_width << 3,
				mb_height << 3,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);

		addr += mb_total << 6;
		index++;
		buffer_spec[i].v_addr = addr;
		buffer_spec[i].v_canvas_index = index;
		canvas_config(index,
				addr,
				mb_width << 3,
				mb_height << 3,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);

		addr += mb_total << 6;
		index++;

		dpb_addr = dpb_addr + dpb_size;
		if (dpb_addr >= DECODE_BUFFER_END)
			return -1;
	}

	return dpb_addr;
}

static int get_max_dec_frame_buf_size(int level_idc,
		int max_reference_frame_num, int mb_width,
		int mb_height)
{
	int pic_size = mb_width * mb_height * 384;

	int size = 0;

	switch (level_idc) {
	case 9:
		size = 152064;
		break;
	case 10:
		size = 152064;
		break;
	case 11:
		size = 345600;
		break;
	case 12:
		size = 912384;
		break;
	case 13:
		size = 912384;
		break;
	case 20:
		size = 912384;
		break;
	case 21:
		size = 1824768;
		break;
	case 22:
		size = 3110400;
		break;
	case 30:
		size = 3110400;
		break;
	case 31:
		size = 6912000;
		break;
	case 32:
		size = 7864320;
		break;
	case 40:
		size = 12582912;
		break;
	case 41:
		size = 12582912;
		break;
	case 42:
		size = 13369344;
		break;
	case 50:
		size = 42393600;
		break;
	case 51:
		size = 70778880;
		break;
	default:
		break;
	}

	size /= pic_size;
	size = size + 1;	/* For MVC need onr more buffer */
	if (max_reference_frame_num > size)
		size = max_reference_frame_num;
	if (size > DECODE_BUFFER_NUM_MAX)
		size = DECODE_BUFFER_NUM_MAX;

	return size;
}

int check_in_list(int pos, int *slot)
{
	int i;
	int ret = 0;
	for (i = 0; i < VF_POOL_SIZE; i++) {
		if ((vfpool_idx[i].display_pos == pos)
			&& (vfpool_idx[i].used == 0)) {
			ret = 1;
			*slot = vfpool_idx[i].slot;
			break;
		}
	}
	return ret;
}

#ifdef HANDLE_h264mvc_IRQ
static irqreturn_t vh264mvc_isr(int irq, void *dev_id)
#else
static void vh264mvc_isr(void)
#endif
{
	int drop_status;
	struct vframe_s *vf;
	unsigned int pts, pts_valid = 0;
	u64 pts_us64;
	int ret = READ_VREG(MAILBOX_COMMAND);
	/* pr_info("vh264mvc_isr, cmd =%x\n", ret); */
	switch (ret & 0xff) {
	case CMD_ALLOC_VIEW_0:
		if (dbg_mode & 0x1) {
			pr_info
			("Start H264 display buffer allocation for view 0\n");
		}
		if ((dpb_start_addr[0] != -1) | (dpb_start_addr[1] != -1)) {
			dpb_start_addr[0] = -1;
			dpb_start_addr[1] = -1;
		}
		dpb_start_addr[0] = DECODE_BUFFER_START;
		ret = READ_VREG(MAILBOX_DATA_0);
		level_idc = (ret >> 24) & 0xff;
		max_reference_frame_num = (ret >> 16) & 0xff;
		mb_width = (ret >> 8) & 0xff;
		mb_height = (ret >> 0) & 0xff;
		max_dec_frame_buffering[0] =
			get_max_dec_frame_buf_size(level_idc,
					max_reference_frame_num,
					mb_width, mb_height);

		total_dec_frame_buffering[0] =
			max_dec_frame_buffering[0] + DISPLAY_BUFFER_NUM;

		mb_width = (mb_width + 3) & 0xfffffffc;
		mb_height = (mb_height + 3) & 0xfffffffc;

		dpb_size = mb_width * mb_height * 384;
		ref_size = mb_width * mb_height * 96;

		if (dbg_mode & 0x1) {
			pr_info("dpb_size: 0x%x\n", dpb_size);
			pr_info("ref_size: 0x%x\n", ref_size);
			pr_info("total_dec_frame_buffering[0] : 0x%x\n",
				   total_dec_frame_buffering[0]);
			pr_info("max_reference_frame_num: 0x%x\n",
				   max_reference_frame_num);
		}
		ref_start_addr[0] = dpb_start_addr[0] +
			(dpb_size * total_dec_frame_buffering[0]);
		dpb_start_addr[1] = ref_start_addr[0] +
			(ref_size * (max_reference_frame_num + 1));

		if (dbg_mode & 0x1) {
			pr_info("dpb_start_addr[0]: 0x%x\n", dpb_start_addr[0]);
			pr_info("ref_start_addr[0]: 0x%x\n", ref_start_addr[0]);
			pr_info("dpb_start_addr[1]: 0x%x\n", dpb_start_addr[1]);
		}
		if (dpb_start_addr[1] >= DECODE_BUFFER_END) {
			pr_info(" No enough memory for alloc view 0\n");
			goto exit;
		}

		index = CANVAS_INDEX_START;
		ANC_CANVAS_ADDR = ANC0_CANVAS_ADDR;

		ret =
			init_canvas(dpb_start_addr[0], dpb_size,
					total_dec_frame_buffering[0], mb_width,
					mb_height, buffer_spec0);

		if (ret == -1) {
			pr_info(" Un-expected memory alloc problem\n");
			goto exit;
		}

		WRITE_VREG(REF_START_VIEW_0,
				   video_domain_addr(ref_start_addr[0]));
		WRITE_VREG(MAILBOX_DATA_0,
				   (max_dec_frame_buffering[0] << 8) |
				   (total_dec_frame_buffering[0] << 0)
				  );
		WRITE_VREG(MAILBOX_DATA_1, ref_size);
		WRITE_VREG(MAILBOX_COMMAND, CMD_FINISHED);

		if (dbg_mode & 0x1) {
			pr_info
			("End H264 display buffer allocation for view 0\n");
		}
		if (frame_width == 0) {
			if (vh264mvc_amstream_dec_info.width)
				frame_width = vh264mvc_amstream_dec_info.width;
			else
				frame_width = mb_width << 4;
		}
		if (frame_height == 0) {
			frame_height = mb_height << 4;
			if (frame_height == 1088)
				frame_height = 1080;
		}
		break;
	case CMD_ALLOC_VIEW_1:
		if (dbg_mode & 0x1) {
			pr_info
			("Start H264 display buffer allocation for view 1\n");
		}
		if ((dpb_start_addr[0] == -1) | (dpb_start_addr[1] == -1)) {
			pr_info("Error: allocation view 1 before view 0 !!!\n");
			break;
		}
		ret = READ_VREG(MAILBOX_DATA_0);
		level_idc = (ret >> 24) & 0xff;
		max_reference_frame_num = (ret >> 16) & 0xff;
		mb_width = (ret >> 8) & 0xff;
		mb_height = (ret >> 0) & 0xff;
		max_dec_frame_buffering[1] =
			get_max_dec_frame_buf_size(level_idc,
					max_reference_frame_num,
					mb_width, mb_height);
		if (max_dec_frame_buffering[1] != max_dec_frame_buffering[0]) {
			pr_info
			(" Warning: view0/1 max_dec_frame_buffering ");
			pr_info("different : 0x%x/0x%x, Use View0\n",
			 max_dec_frame_buffering[0],
			 max_dec_frame_buffering[1]);
			max_dec_frame_buffering[1] = max_dec_frame_buffering[0];
		}

		total_dec_frame_buffering[1] =
			max_dec_frame_buffering[1] + DISPLAY_BUFFER_NUM;

		mb_width = (mb_width + 3) & 0xfffffffc;
		mb_height = (mb_height + 3) & 0xfffffffc;

		dpb_size = mb_width * mb_height * 384;
		ref_size = mb_width * mb_height * 96;

		if (dbg_mode & 0x1) {
			pr_info("dpb_size: 0x%x\n", dpb_size);
			pr_info("ref_size: 0x%x\n", ref_size);
			pr_info("total_dec_frame_buffering[1] : 0x%x\n",
				   total_dec_frame_buffering[1]);
			pr_info("max_reference_frame_num: 0x%x\n",
				   max_reference_frame_num);
		}
		ref_start_addr[1] = dpb_start_addr[1] +
			(dpb_size * total_dec_frame_buffering[1]);
		dpb_start_addr[2] = ref_start_addr[1] +
			(ref_size * (max_reference_frame_num + 1));

		if (dbg_mode & 0x1) {
			pr_info("dpb_start_addr[1]: 0x%x\n", dpb_start_addr[1]);
			pr_info("ref_start_addr[1]: 0x%x\n", ref_start_addr[1]);
			pr_info("dpb_start_addr[2]: 0x%x\n", dpb_start_addr[2]);
		}
		if (dpb_start_addr[2] >= DECODE_BUFFER_END) {
			pr_info(" No enough memory for alloc view 1\n");
			goto exit;
		}

		index = CANVAS_INDEX_START + total_dec_frame_buffering[0] * 3;
		ANC_CANVAS_ADDR =
			ANC0_CANVAS_ADDR + total_dec_frame_buffering[0];

		ret =
			init_canvas(dpb_start_addr[1], dpb_size,
					total_dec_frame_buffering[1], mb_width,
					mb_height, buffer_spec1);

		if (ret == -1) {
			pr_info(" Un-expected memory alloc problem\n");
			goto exit;
		}

		WRITE_VREG(REF_START_VIEW_1,
				   video_domain_addr(ref_start_addr[1]));
		WRITE_VREG(MAILBOX_DATA_0,
				   (max_dec_frame_buffering[1] << 8) |
				   (total_dec_frame_buffering[1] << 0)
				  );
		WRITE_VREG(MAILBOX_DATA_1, ref_size);
		WRITE_VREG(MAILBOX_COMMAND, CMD_FINISHED);

		if (dbg_mode & 0x1) {
			pr_info
			("End H264 display buffer allocation for view 1\n");
		}
		if (frame_width == 0) {
			if (vh264mvc_amstream_dec_info.width)
				frame_width = vh264mvc_amstream_dec_info.width;
			else
				frame_width = mb_width << 4;
		}
		if (frame_height == 0) {
			frame_height = mb_height << 4;
			if (frame_height == 1088)
				frame_height = 1080;
		}
		break;
	case CMD_FRAME_DISPLAY:
		ret = READ_VREG(MAILBOX_DATA_0);
		display_buff_id = (ret >> 0) & 0x3f;
		display_view_id = (ret >> 6) & 0x3;
		drop_status = (ret >> 8) & 0x1;
		display_POC = READ_VREG(MAILBOX_DATA_1);
		stream_offset = READ_VREG(MAILBOX_DATA_2);
		/* if (display_view_id == 0) */
		WRITE_VREG(MAILBOX_COMMAND, CMD_FINISHED);

#ifdef DEBUG_SKIP
		view_total++;
		if (drop_status)
			view_dropped++;
#endif
		if (dbg_mode & 0x1) {
			pr_info
			(" H264 display frame ready - View : %x, Buffer : %x\n",
			 display_view_id, display_buff_id);
			pr_info
			(" H264 display frame POC -- Buffer : %x, POC : %x\n",
			 display_buff_id, display_POC);
			pr_info("H264 display frame ready\n");
		}
		if (dbg_mode & 0x10) {
			if ((dbg_mode & 0x20) == 0) {
				while (READ_VREG(BUFFER_RECYCLE) != 0)
					;
				WRITE_VREG(BUFFER_RECYCLE,
						   (display_view_id << 8) |
						   (display_buff_id + 1));
				display_buff_id = -1;
				display_view_id = -1;
				display_POC = -1;
			}
		} else {
			unsigned char in_list_flag = 0;

			int slot = 0;
			in_list_flag = check_in_list(display_POC, &slot);

			if ((dbg_mode & 0x40) && (drop_status)) {
				pr_info
				("drop_status:%dview_id=%d,buff_id=%d,",
				 drop_status, display_view_id, display_buff_id);
				 pr_info
				("offset=%d, display_POC = %d,fill_ptr=0x%x\n",
				 stream_offset, display_POC, fill_ptr);
			}

			if ((in_list_flag) && (stream_offset != 0)) {
				pr_info
				("error case ,display_POC is %d, slot is %d\n",
				 display_POC, slot);
				in_list_flag = 0;
			}
			if (!in_list_flag) {
				if (display_view_id == 0) {
					vfpool_idx[fill_ptr].view0_buf_id =
						display_buff_id;
					view0_vfbuf_use[display_buff_id]++;
					vfpool_idx[fill_ptr].stream_offset =
						stream_offset;
					vfpool_idx[fill_ptr].view0_drop =
						drop_status;
				}
				if (display_view_id == 1) {
					vfpool_idx[fill_ptr].view1_buf_id =
						display_buff_id;
					vfpool_idx[fill_ptr].view1_drop =
						drop_status;
					view1_vfbuf_use[display_buff_id]++;
				}
				vfpool_idx[fill_ptr].slot = fill_ptr;
				vfpool_idx[fill_ptr].display_pos = display_POC;

			} else {
				if (display_view_id == 0) {
					vfpool_idx[slot].view0_buf_id =
						display_buff_id;
					view0_vfbuf_use[display_buff_id]++;
					vfpool_idx[slot].stream_offset =
						stream_offset;
					vfpool_idx[slot].view0_drop =
						drop_status;

				}
				if (display_view_id == 1) {
					vfpool_idx[slot].view1_buf_id =
						display_buff_id;
					view1_vfbuf_use[display_buff_id]++;
					vfpool_idx[slot].view1_drop =
						drop_status;
				}
				vf = &vfpool[slot];
#if 0
				if (pts_lookup_offset
					(PTS_TYPE_VIDEO,
					 vfpool_idx[slot].stream_offset,
					 &vf->pts,
					 0) != 0)
					vf->pts = 0;
#endif
				if (vfpool_idx[slot].stream_offset == 0) {
					pr_info
					("error case, invalid stream offset\n");
				}
				if (pts_lookup_offset_us64
					(PTS_TYPE_VIDEO,
					 vfpool_idx[slot].stream_offset, &pts,
					 0x10000, &pts_us64) == 0)
					pts_valid = 1;
				else
					pts_valid = 0;
				vf->pts = (pts_valid) ? pts : 0;
				vf->pts_us64 = (pts_valid) ? pts_us64 : 0;
				/* vf->pts =  vf->pts_us64 ? vf->pts_us64
				   : vf->pts ; */
				/* vf->pts =  vf->pts_us64; */
				if (dbg_mode & 0x80)
					pr_info("vf->pts:%d\n", vf->pts);
				vfpool_idx[slot].used = 1;
				INCPTR(fill_ptr);
				set_frame_info(vf);

				gvs->frame_dur = frame_dur;
				vdec_count_info(gvs, 0,
						vfpool_idx[slot].stream_offset);

				vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);

			}
		}
		break;
	case CMD_FATAL_ERROR:
		pr_info("fatal error !!!\n");
		schedule_work(&error_wd_work);
		break;
	default:
		break;
	}
exit:
#ifdef HANDLE_h264mvc_IRQ
	return IRQ_HANDLED;
#else
	return;
#endif
}

static void vh264mvc_put_timer_func(unsigned long arg)
{
	struct timer_list *timer = (struct timer_list *)arg;

	int valid_frame = 0;
	if (enable_recycle == 0) {
		if (dbg_mode & TIME_TASK_PRINT_ENABLE) {
			/* valid_frame = get_valid_frame(); */
			pr_info("dirty_frame_num is %d , valid frame is %d\n",
				   dirty_frame_num, valid_frame);

		}
		/* goto RESTART; */
	}

	while ((putting_ptr != put_ptr) && (READ_VREG(BUFFER_RECYCLE) == 0)) {
		int view0_buf_id = vfpool_idx[put_ptr].view0_buf_id;
		int view1_buf_id = vfpool_idx[put_ptr].view1_buf_id;
		if ((view0_buf_id >= 0) &&
				(view0_vfbuf_use[view0_buf_id] == 1)) {
			if (dbg_mode & 0x100) {
				pr_info
				("round 0: put_ptr is %d ;view0_buf_id is %d\n",
				 put_ptr, view0_buf_id);
			}
			WRITE_VREG(BUFFER_RECYCLE,
					   (0 << 8) | (view0_buf_id + 1));
			view0_vfbuf_use[view0_buf_id] = 0;
			vfpool_idx[put_ptr].view0_buf_id = -1;
			vfpool_idx[put_ptr].view0_drop = 0;
		} else if ((view1_buf_id >= 0)
				   && (view1_vfbuf_use[view1_buf_id] == 1)) {
			if (dbg_mode & 0x100) {
				pr_info
				("round 1: put_ptr is %d ;view1_buf_id %d==\n",
				 put_ptr, view1_buf_id);
			}
			WRITE_VREG(BUFFER_RECYCLE,
					   (1 << 8) | (view1_buf_id + 1));
			view1_vfbuf_use[view1_buf_id] = 0;
			vfpool_idx[put_ptr].display_pos = DISPLAY_INVALID_POS;
			vfpool_idx[put_ptr].view1_buf_id = -1;
			vfpool_idx[put_ptr].view1_drop = 0;
			vfpool_idx[put_ptr].used = 0;
			INCPTR(put_ptr);
		}
	}


	if (frame_dur > 0 && saved_resolution !=
		frame_width * frame_height * (96000 / frame_dur)) {
		int fps = 96000 / frame_dur;
		saved_resolution = frame_width * frame_height * fps;
		vdec_source_changed(VFORMAT_H264MVC,
			frame_width, frame_height, fps * 2);
	}

	/* RESTART: */
	timer->expires = jiffies + PUT_INTERVAL;

	add_timer(timer);
}

int vh264mvc_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	vstatus->frame_width = frame_width;
	vstatus->frame_height = frame_height;
	if (frame_dur != 0)
		vstatus->frame_rate = 96000 / frame_dur;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = READ_VREG(AV_SCRATCH_D);
	vstatus->status = stat;
	vstatus->bit_rate = gvs->bit_rate;
	vstatus->frame_dur = frame_dur;
	vstatus->frame_data = gvs->frame_data;
	vstatus->total_data = gvs->total_data;
	vstatus->frame_count = gvs->frame_count;
	vstatus->error_frame_count = gvs->error_frame_count;
	vstatus->drop_frame_count = gvs->drop_frame_count;
	vstatus->total_data = gvs->total_data;
	vstatus->samp_cnt = gvs->samp_cnt;
	vstatus->offset = gvs->offset;
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
		"%s", DRIVER_NAME);

	return 0;
}

static int vh264mvc_vdec_info_init(void)
{
	gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);
	if (NULL == gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}
	return 0;
}

int vh264mvc_set_trickmode(struct vdec_s *vdec, unsigned long trickmode)
{
	if (trickmode == TRICKMODE_I) {
		WRITE_VREG(AV_SCRATCH_F,
				   (READ_VREG(AV_SCRATCH_F) & 0xfffffffc) | 2);
		trickmode_i = 1;
	} else if (trickmode == TRICKMODE_NONE) {
		WRITE_VREG(AV_SCRATCH_F, READ_VREG(AV_SCRATCH_F) & 0xfffffffc);
		trickmode_i = 0;
	}

	return 0;
}

static void H264_DECODE_INIT(void)
{
	int i;
	i = READ_VREG(DECODE_SKIP_PICTURE);

#if 1				/* /MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) | (1 << 4));
	WRITE_VREG(DOS_SW_RESET0, 0);

	READ_VREG(DOS_SW_RESET0);
	READ_VREG(DOS_SW_RESET0);
	READ_VREG(DOS_SW_RESET0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) | (1 << 4));
	WRITE_VREG(DOS_SW_RESET0, 0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 9) | (1 << 8));
	WRITE_VREG(DOS_SW_RESET0, 0);

	READ_VREG(DOS_SW_RESET0);
	READ_VREG(DOS_SW_RESET0);
	READ_VREG(DOS_SW_RESET0);

#else
	WRITE_MPEG_REG(RESET0_REGISTER,
				   RESET_IQIDCT | RESET_MC | RESET_VLD_PART);
	READ_MPEG_REG(RESET0_REGISTER);
	WRITE_MPEG_REG(RESET0_REGISTER,
				   RESET_IQIDCT | RESET_MC | RESET_VLD_PART);
	WRITE_MPEG_REG(RESET2_REGISTER, RESET_PIC_DC | RESET_DBLK);
#endif

	/* Wait for some time for RESET */
	READ_VREG(DECODE_SKIP_PICTURE);
	READ_VREG(DECODE_SKIP_PICTURE);

	WRITE_VREG(DECODE_SKIP_PICTURE, i);

	/* fill_weight_pred */
	WRITE_VREG(MC_MPORT_CTRL, 0x0300);
	for (i = 0; i < 192; i++)
		WRITE_VREG(MC_MPORT_DAT, 0x100);
	WRITE_VREG(MC_MPORT_CTRL, 0);

	WRITE_VREG(MB_WIDTH, 0xff);	/* invalid mb_width */

	/* set slice start to 0x000000 or 0x000001 for check more_rbsp_data */
	WRITE_VREG(SLICE_START_BYTE_01, 0x00000000);
	WRITE_VREG(SLICE_START_BYTE_23, 0x01010000);
	/* set to mpeg2 to enable mismatch logic */
	WRITE_VREG(MPEG1_2_REG, 1);
	/* disable COEF_GT_64 , error_m4_table and voff_rw_err */
	WRITE_VREG(VLD_ERROR_MASK, 0x1011);

	/* Config MCPU Amrisc interrupt */
	WRITE_VREG(ASSIST_AMR1_INT0, 0x1);	/* viu_vsync_int */
	WRITE_VREG(ASSIST_AMR1_INT1, 0x5);	/* mbox_isr */
	WRITE_VREG(ASSIST_AMR1_INT2, 0x8);	/* vld_isr */
	WRITE_VREG(ASSIST_AMR1_INT3, 0x15);	/* vififo_empty */
	WRITE_VREG(ASSIST_AMR1_INT4, 0xd);	/* rv_ai_mb_finished_int */
	WRITE_VREG(ASSIST_AMR1_INT7, 0x14);	/* dcac_dma_done */

	/* Config MCPU Amrisc interrupt */
	WRITE_VREG(ASSIST_AMR1_INT5, 0x9);	/* MCPU interrupt */
	WRITE_VREG(ASSIST_AMR1_INT6, 0x17);	/* CCPU interrupt */

	WRITE_VREG(CPC_P, 0xc00);	/* CCPU Code will start from 0xc00 */
	WRITE_VREG(CINT_VEC_BASE, (0xc20 >> 5));
#if 0
	WRITE_VREG(POWER_CTL_VLD,
			READ_VREG(POWER_CTL_VLD) | (0 << 10) |
			(1 << 9) | (1 << 6));
#else
	WRITE_VREG(POWER_CTL_VLD, ((1 << 10) |	/* disable cabac_step_2 */
				(1 << 9) |	/* viff_drop_flag_en */
				(1 << 6)	/* h264_000003_en */
							  )
			  );
#endif
	WRITE_VREG(M4_CONTROL_REG, (1 << 13));	/* H264_DECODE_INFO - h264_en */

	WRITE_VREG(CANVAS_START, CANVAS_INDEX_START);
#if 1
	/* Start Address of Workspace (UCODE, temp_data...) */
	WRITE_VREG(WORKSPACE_START,
			   video_domain_addr(work_space_adr));
#else
	/* Start Address of Workspace (UCODE, temp_data...) */
	WRITE_VREG(WORKSPACE_START,
			0x05000000);
#endif
	/* Clear all sequence parameter set available */
	WRITE_VREG(SPS_STATUS, 0);
	/* Clear all picture parameter set available */
	WRITE_VREG(PPS_STATUS, 0);
	/* Set current microcode to NULL */
	WRITE_VREG(CURRENT_UCODE, 0xff);
	/* Set current SPS/PPS to NULL */
	WRITE_VREG(CURRENT_SPS_PPS, 0xffff);
	/* Set decode status to DECODE_START_HEADER */
	WRITE_VREG(DECODE_STATUS, 1);
}

static void vh264mvc_prot_init(void)
{
	while (READ_VREG(DCAC_DMA_CTRL) & 0x8000)
		;
	while (READ_VREG(LMEM_DMA_CTRL) & 0x8000)
		;		/* reg address is 0x350 */
	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);

	H264_DECODE_INIT();

#if 1				/* /MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	WRITE_VREG(DOS_SW_RESET0, (1 << 11));
	WRITE_VREG(DOS_SW_RESET0, 0);

	READ_VREG(DOS_SW_RESET0);
	READ_VREG(DOS_SW_RESET0);
	READ_VREG(DOS_SW_RESET0);

#else
	WRITE_MPEG_REG(RESET0_REGISTER, 0x80);	/* RESET MCPU */
#endif

	WRITE_VREG(MAILBOX_COMMAND, 0);
	WRITE_VREG(BUFFER_RECYCLE, 0);
	WRITE_VREG(DROP_CONTROL, 0);
	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
#if 1				/* /MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	WRITE_VREG(MDEC_PIC_DC_THRESH, 0x404038aa);
#endif
}

static void vh264mvc_local_init(void)
{
	int i;
	display_buff_id = -1;
	display_view_id = -1;
	display_POC = -1;
	no_dropping_cnt = 0;
	init_drop_cnt = INIT_DROP_FRAME_CNT;

	for (i = 0; i < INIT_DROP_FRAME_CNT; i++)
		init_drop_frame_id[i] = 0;

#ifdef DEBUG_PTS
	pts_missed = 0;
	pts_hit = 0;
#endif

#ifdef DEBUG_SKIP
	view_total = 0;
	view_dropped = 0;
#endif

	/* vh264mvc_ratio = vh264mvc_amstream_dec_info.ratio; */
	vh264mvc_ratio = 0x100;

	/* frame_width = vh264mvc_amstream_dec_info.width; */
	/* frame_height = vh264mvc_amstream_dec_info.height; */
	frame_dur = vh264mvc_amstream_dec_info.rate;
	if (frame_dur == 0)
		frame_dur = 96000 / 24;

	pts_outside = ((unsigned long) vh264mvc_amstream_dec_info.param) & 0x01;
	sync_outside = ((unsigned long) vh264mvc_amstream_dec_info.param & 0x02)
			>> 1;

	/**/ dpb_start_addr[0] = -1;
	dpb_start_addr[1] = -1;
	max_dec_frame_buffering[0] = -1;
	max_dec_frame_buffering[1] = -1;
	fill_ptr = get_ptr = put_ptr = putting_ptr = 0;
	dirty_frame_num = 0;
	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		view0_vfbuf_use[i] = 0;
		view1_vfbuf_use[i] = 0;
	}

	for (i = 0; i < VF_POOL_SIZE; i++) {
		vfpool_idx[i].display_pos = -1;
		vfpool_idx[i].view0_buf_id = DISPLAY_INVALID_POS;
		vfpool_idx[i].view1_buf_id = -1;
		vfpool_idx[i].view0_drop = 0;
		vfpool_idx[i].view1_drop = 0;
		vfpool_idx[i].used = 0;
	}
	for (i = 0; i < VF_POOL_SIZE; i++)
		memset(&vfpool[i], 0, sizeof(struct vframe_s));
	init_vf_buf();
	return;
}

static s32 vh264mvc_init(void)
{
	int ret = 0;
	int r1, r2, r3, r4;
	unsigned int cpu_type = get_cpu_type();
	pr_info("\nvh264mvc_init\n");
	init_timer(&recycle_timer);

	stat |= STAT_TIMER_INIT;

	ret = vh264mvc_vdec_info_init();
	if (0 != ret)
		return -ret;

	vh264mvc_local_init();

	amvdec_enable();

	/* -- ucode loading (amrisc and swap code) */
	mc_cpu_addr = dma_alloc_coherent(amports_get_dma_device(),
		MC_TOTAL_SIZE, &mc_dma_handle, GFP_KERNEL);
	if (!mc_cpu_addr) {
		amvdec_disable();
		pr_err("vh264_mvc init: Can not allocate mc memory.\n");
		return -ENOMEM;
	}

	WRITE_VREG(UCODE_START_ADDR, mc_dma_handle);
	if (cpu_type >= MESON_CPU_MAJOR_ID_GXM) {
		r1 = amvdec_loadmc_ex(VFORMAT_H264MVC, "gxm_vh264mvc_mc", NULL);

		/*memcpy(p, vh264mvc_header_mc, sizeof(vh264mvc_header_mc));*/
		r2 = get_decoder_firmware_data(VFORMAT_H264MVC,
			"gxm_vh264mvc_header_mc", mc_cpu_addr, 0x1000);

		/*memcpy((void *)((ulong) p + 0x1000),
			   vh264mvc_mmco_mc, sizeof(vh264mvc_mmco_mc));
		*/
		r3 = get_decoder_firmware_data(VFORMAT_H264MVC,
			"gxm_vh264mvc_mmco_mc",
			(void *)((u8 *) mc_cpu_addr + 0x1000), 0x2000);

		/*memcpy((void *)((ulong) p + 0x3000),
			   vh264mvc_slice_mc, sizeof(vh264mvc_slice_mc));
		*/
		r4 = get_decoder_firmware_data(VFORMAT_H264MVC,
			"gxm_vh264mvc_slice_mc",
			(void *)((u8 *) mc_cpu_addr + 0x3000), 0x4000);

		} else {
		r1 = amvdec_loadmc_ex(VFORMAT_H264MVC, "vh264mvc_mc", NULL);

		/*memcpy(p, vh264mvc_header_mc, sizeof(vh264mvc_header_mc));*/
		r2 = get_decoder_firmware_data(VFORMAT_H264MVC,
			"vh264mvc_header_mc", mc_cpu_addr, 0x1000);

		/*memcpy((void *)((ulong) p + 0x1000),
			   vh264mvc_mmco_mc, sizeof(vh264mvc_mmco_mc));
		*/
		r3 = get_decoder_firmware_data(VFORMAT_H264MVC,
			"vh264mvc_mmco_mc",
			(void *)((u8 *) mc_cpu_addr + 0x1000), 0x2000);

		/*memcpy((void *)((ulong) p + 0x3000),
			   vh264mvc_slice_mc, sizeof(vh264mvc_slice_mc));
		*/
		r4 = get_decoder_firmware_data(VFORMAT_H264MVC,
			"vh264mvc_slice_mc",
			(void *)((u8 *) mc_cpu_addr + 0x3000), 0x4000);
		}
	if (r1 < 0 || r2 < 0 || r3 < 0 || r4 < 0) {
		amvdec_disable();

		dma_free_coherent(amports_get_dma_device(),
				MC_TOTAL_SIZE, mc_cpu_addr, mc_dma_handle);
		mc_cpu_addr = NULL;
		return -EBUSY;
	}

	stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	vh264mvc_prot_init();

#ifdef HANDLE_h264mvc_IRQ
	if (vdec_request_irq(VDEC_IRQ_1, vh264mvc_isr,
			"vh264mvc-irq", (void *)vh264mvc_dec_id)) {
		pr_info("vh264mvc irq register error.\n");
		amvdec_disable();
		return -ENOENT;
	}
#endif

	stat |= STAT_ISR_REG;

	vf_provider_init(&vh264mvc_vf_prov, PROVIDER_NAME,
					 &vh264mvc_vf_provider, NULL);
	vf_reg_provider(&vh264mvc_vf_prov);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);

	stat |= STAT_VF_HOOK;

	recycle_timer.data = (ulong) (&recycle_timer);
	recycle_timer.function = vh264mvc_put_timer_func;
	recycle_timer.expires = jiffies + PUT_INTERVAL;

	add_timer(&recycle_timer);

	stat |= STAT_TIMER_ARM;

	amvdec_start();

	stat |= STAT_VDEC_RUN;

	return 0;
}

static int vh264mvc_stop(void)
{
	if (stat & STAT_VDEC_RUN) {
		amvdec_stop();
		stat &= ~STAT_VDEC_RUN;
	}

	if (stat & STAT_ISR_REG) {
		WRITE_VREG(ASSIST_MBOX1_MASK, 0);
#ifdef HANDLE_h264mvc_IRQ
		vdec_free_irq(VDEC_IRQ_1, (void *)vh264mvc_dec_id);
#endif
		stat &= ~STAT_ISR_REG;
	}

	if (stat & STAT_TIMER_ARM) {
		del_timer_sync(&recycle_timer);
		stat &= ~STAT_TIMER_ARM;
	}

	if (stat & STAT_VF_HOOK) {
		ulong flags;
		spin_lock_irqsave(&lock, flags);
		spin_unlock_irqrestore(&lock, flags);
		vf_unreg_provider(&vh264mvc_vf_prov);
		stat &= ~STAT_VF_HOOK;
	}

	if (stat & STAT_MC_LOAD) {
		if (mc_cpu_addr != NULL) {
			dma_free_coherent(amports_get_dma_device(),
				MC_TOTAL_SIZE, mc_cpu_addr, mc_dma_handle);
			mc_cpu_addr = NULL;
		}

		stat &= ~STAT_MC_LOAD;
	}

	amvdec_disable();

	uninit_vf_buf();
	return 0;
}

static void error_do_work(struct work_struct *work)
{
	if (atomic_read(&vh264mvc_active)) {
		vh264mvc_stop();
		vh264mvc_init();
	}
}

static int amvdec_h264mvc_probe(struct platform_device *pdev)
{
	struct resource mem;
	int buf_size;

	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;

	pr_info("amvdec_h264mvc probe start.\n");

#if 0
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		pr_info("\namvdec_h264mvc memory resource undefined.\n");
		return -EFAULT;
	}
#endif

	if (pdata == NULL) {
		pr_info("\namvdec_h264mvc memory resource undefined.\n");
		return -EFAULT;
	}
	mem.start = pdata->mem_start;
	mem.end = pdata->mem_end;

	buf_size = mem.end - mem.start + 1;
	/* buf_offset = mem->start - DEF_BUF_START_ADDR; */
	work_space_adr = mem.start;
	DECODE_BUFFER_START = work_space_adr + work_space_size;
	DECODE_BUFFER_END = mem.start + buf_size;

	pr_info
	("work_space_adr %x, DECODE_BUFFER_START %x, DECODE_BUFFER_END %x\n",
	 work_space_adr, DECODE_BUFFER_START, DECODE_BUFFER_END);
	if (pdata->sys_info)
		vh264mvc_amstream_dec_info = *pdata->sys_info;

	pdata->dec_status = vh264mvc_dec_status;
	/* pdata->set_trickmode = vh264mvc_set_trickmode; */

	if (vh264mvc_init() < 0) {
		pr_info("\namvdec_h264mvc init failed.\n");
		kfree(gvs);
		gvs = NULL;

		return -ENODEV;
	}

	INIT_WORK(&error_wd_work, error_do_work);

	atomic_set(&vh264mvc_active, 1);

	pr_info("amvdec_h264mvc probe end.\n");

	return 0;
}

static int amvdec_h264mvc_remove(struct platform_device *pdev)
{
	pr_info("amvdec_h264mvc_remove\n");
	cancel_work_sync(&error_wd_work);
	vh264mvc_stop();
	frame_width = 0;
	frame_height = 0;
	vdec_source_changed(VFORMAT_H264MVC, 0, 0, 0);
	atomic_set(&vh264mvc_active, 0);

#ifdef DEBUG_PTS
	pr_info
	("pts missed %ld, pts hit %ld, pts_outside %d, ",
	 pts_missed, pts_hit, pts_outside);
	 pr_info("duration %d, sync_outside %d\n",
	 frame_dur, sync_outside);
#endif

#ifdef DEBUG_SKIP
	pr_info("view_total = %ld, dropped %ld\n", view_total, view_dropped);
#endif
	kfree(gvs);
	gvs = NULL;

	return 0;
}

/****************************************/

static struct platform_driver amvdec_h264mvc_driver = {
	.probe = amvdec_h264mvc_probe,
	.remove = amvdec_h264mvc_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

static struct codec_profile_t amvdec_hmvc_profile = {
	.name = "hmvc",
	.profile = ""
};

static int __init amvdec_h264mvc_driver_init_module(void)
{
	pr_debug("amvdec_h264mvc module init\n");

	if (platform_driver_register(&amvdec_h264mvc_driver)) {
		pr_err("failed to register amvdec_h264mvc driver\n");
		return -ENODEV;
	}

	vcodec_profile_register(&amvdec_hmvc_profile);

	return 0;
}

static void __exit amvdec_h264mvc_driver_remove_module(void)
{
	pr_debug("amvdec_h264mvc module remove.\n");

	platform_driver_unregister(&amvdec_h264mvc_driver);
}

/****************************************/

module_param(stat, uint, 0664);
MODULE_PARM_DESC(stat, "\n amvdec_h264mvc stat\n");

module_param(dbg_mode, uint, 0664);
MODULE_PARM_DESC(dbg_mode, "\n amvdec_h264mvc dbg mode\n");

module_param(view_mode, uint, 0664);
MODULE_PARM_DESC(view_mode, "\n amvdec_h264mvc view mode\n");

module_param(dbg_cmd, uint, 0664);
MODULE_PARM_DESC(dbg_mode, "\n amvdec_h264mvc cmd mode\n");

module_param(drop_rate, uint, 0664);
MODULE_PARM_DESC(dbg_mode, "\n amvdec_h264mvc drop rate\n");

module_param(drop_thread_hold, uint, 0664);
MODULE_PARM_DESC(dbg_mode, "\n amvdec_h264mvc drop thread hold\n");
module_init(amvdec_h264mvc_driver_init_module);
module_exit(amvdec_h264mvc_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC h264mvc Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chen Zhang <chen.zhang@amlogic.com>");
