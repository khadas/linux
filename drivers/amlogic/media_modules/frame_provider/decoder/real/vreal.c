/*
 * drivers/amlogic/amports/vreal.c
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
#include <linux/fs.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>

#include <linux/amlogic/media/canvas/canvas.h>

#include <linux/dma-mapping.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "../../../stream_input/amports/amports_priv.h"
#include "../utils/vdec.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include "../utils/amvdec.h"

#include "../../../stream_input/parser/streambuf.h"
#include "../../../stream_input/parser/streambuf_reg.h"
#include "../../../stream_input/parser/rmparser.h"

#include "vreal.h"
#include <linux/amlogic/media/registers/register.h>
#include "../utils/decoder_mmu_box.h"
#include "../utils/decoder_bmmu_box.h"
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include "../utils/firmware.h"
#include <linux/amlogic/tee.h>

#define DRIVER_NAME "amvdec_real"
#define MODULE_NAME "amvdec_real"

#if 1	/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#define NV21
#endif

#define RM_DEF_BUFFER_ADDR        0x01000000
/* protocol registers */
#define STATUS_AMRISC   AV_SCRATCH_4

#define RV_PIC_INFO     AV_SCRATCH_5
#define VPTS_TR         AV_SCRATCH_6
#define VDTS            AV_SCRATCH_7
#define FROM_AMRISC     AV_SCRATCH_8
#define TO_AMRISC       AV_SCRATCH_9
#define SKIP_B_AMRISC   AV_SCRATCH_A
#define INT_REASON      AV_SCRATCH_B
#define WAIT_BUFFER     AV_SCRATCH_E

#if 1	/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#define MDEC_WIDTH      AV_SCRATCH_I
#define MDEC_HEIGHT     AV_SCRATCH_J
#else
#define MDEC_WIDTH      HARM_ASB_MB2
#define MDEC_HEIGHT     HASB_ARM_MB0
#endif

#define PARC_FORBIDDEN              0
#define PARC_SQUARE                 1
#define PARC_CIF                    2
#define PARC_10_11                  3
#define PARC_16_11                  4
#define PARC_40_33                  5
#define PARC_RESERVED               6
/* values between 6 and 14 are reserved */
#define PARC_EXTENDED              15

#define VF_POOL_SIZE        16
#define VF_BUF_NUM          4
#define PUT_INTERVAL        (HZ/100)
#define WORKSPACE_SIZE		(1 * SZ_1M)
#define MAX_BMMU_BUFFER_NUM	(VF_BUF_NUM + 1)
#define RV_AI_BUFF_START_IP	 0x01f00000

static struct vframe_s *vreal_vf_peek(void *);
static struct vframe_s *vreal_vf_get(void *);
static void vreal_vf_put(struct vframe_s *, void *);
static int vreal_vf_states(struct vframe_states *states, void *);
static int vreal_event_cb(int type, void *data, void *private_data);

static int vreal_prot_init(void);
static void vreal_local_init(void);

static const char vreal_dec_id[] = "vreal-dev";

#define PROVIDER_NAME   "decoder.real"

/*
 *int query_video_status(int type, int *value);
 */
static const struct vframe_operations_s vreal_vf_provider = {
	.peek = vreal_vf_peek,
	.get = vreal_vf_get,
	.put = vreal_vf_put,
	.event_cb = vreal_event_cb,
	.vf_states = vreal_vf_states,
};

static struct vframe_provider_s vreal_vf_prov;
static void *mm_blk_handle;

static DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
static DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
static DECLARE_KFIFO(recycle_q, struct vframe_s *, VF_POOL_SIZE);

static struct vframe_s vfpool[VF_POOL_SIZE];
static s32 vfbuf_use[VF_BUF_NUM];

static u32 frame_width, frame_height, frame_dur, frame_prog;
static u32 saved_resolution;
static struct timer_list recycle_timer;
static u32 stat;
static u32 buf_size = 32 * 1024 * 1024;
static u32 buf_offset;
static u32 vreal_ratio;
u32 vreal_format;
static u32 wait_key_frame;
static u32 last_tr;
static u32 frame_count;
static u32 current_vdts;
static u32 hold;
static u32 decoder_state;
static u32 real_err_count;

static u32 fatal_flag;
static s32 wait_buffer_counter;
static struct work_struct set_clk_work;
static bool is_reset;

static DEFINE_SPINLOCK(lock);

static unsigned short pic_sz_tbl[12] ____cacheline_aligned;
static dma_addr_t pic_sz_tbl_map;
static const unsigned char RPR_size[9] = { 0, 1, 1, 2, 2, 3, 3, 3, 3 };

static struct dec_sysinfo vreal_amstream_dec_info;

static unsigned char aspect_ratio_table[16] = {
	PARC_FORBIDDEN,
	PARC_SQUARE,
	PARC_CIF,
	PARC_10_11,
	PARC_16_11,
	PARC_40_33,
	PARC_RESERVED, PARC_RESERVED, PARC_RESERVED, PARC_RESERVED,
	PARC_RESERVED, PARC_RESERVED, PARC_RESERVED, PARC_RESERVED,
	PARC_RESERVED, PARC_EXTENDED
};

static inline u32 index2canvas(u32 index)
{
	const u32 canvas_tab[4] = {
#ifdef NV21
		0x010100, 0x030302, 0x050504, 0x070706
#else
		0x020100, 0x050403, 0x080706, 0x0b0a09
#endif
	};

	return canvas_tab[index];
}

static void set_aspect_ratio(struct vframe_s *vf, unsigned int pixel_ratio)
{
	int ar = 0;

	if (vreal_ratio == 0) {
		vf->ratio_control |= (0x90 <<
			DISP_RATIO_ASPECT_RATIO_BIT);
		/* always stretch to 16:9 */
	} else {
		switch (aspect_ratio_table[pixel_ratio]) {
		case 0:
			ar = vreal_amstream_dec_info.height * vreal_ratio /
				 vreal_amstream_dec_info.width;
			break;
		case 1:
		case 0xff:
			ar = vreal_ratio * vf->height / vf->width;
			break;
		case 2:
			ar = (vreal_ratio * vf->height * 12) / (vf->width * 11);
			break;
		case 3:
			ar = (vreal_ratio * vf->height * 11) / (vf->width * 10);
			break;
		case 4:
			ar = (vreal_ratio * vf->height * 11) / (vf->width * 16);
			break;
		case 5:
			ar = (vreal_ratio * vf->height * 33) / (vf->width * 40);
			break;
		default:
			ar = vreal_ratio * vf->height / vf->width;
			break;
		}
	}

	ar = min(ar, DISP_RATIO_ASPECT_RATIO_MAX);

	vf->ratio_control |= (ar << DISP_RATIO_ASPECT_RATIO_BIT);
}

static irqreturn_t vreal_isr(int irq, void *dev_id)
{
	u32 from;
	struct vframe_s *vf = NULL;
	u32 buffer_index;
	unsigned int status;
	unsigned int vdts;
	unsigned int info;
	unsigned int tr;
	unsigned int pictype;
	u32 r = READ_VREG(INT_REASON);

	if (decoder_state == 0)
		return IRQ_HANDLED;

	status = READ_VREG(STATUS_AMRISC);
	if (status & (PARSER_ERROR_WRONG_PACKAGE_SIZE |
		PARSER_ERROR_WRONG_HEAD_VER |
		DECODER_ERROR_VLC_DECODE_TBL)) {
		/* decoder or parser error */
		real_err_count++;
		/* pr_info("real decoder or parser
		 *error, status 0x%x\n", status);
		 */
	}

	if (r == 2) {
		pr_info("first vpts = 0x%x\n", READ_VREG(VDTS));
		pts_checkin_offset(PTS_TYPE_AUDIO, 0, READ_VREG(VDTS) * 90);
		WRITE_VREG(AV_SCRATCH_B, 0);
		WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);
		return IRQ_HANDLED;
	} else if (r == 3) {
		pr_info("first apts = 0x%x\n", READ_VREG(VDTS));
		pts_checkin_offset(PTS_TYPE_VIDEO, 0, READ_VREG(VDTS) * 90);
		WRITE_VREG(AV_SCRATCH_B, 0);
		WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);
		return IRQ_HANDLED;
	}

	from = READ_VREG(FROM_AMRISC);
	if ((hold == 0) && from) {
		tr = READ_VREG(VPTS_TR);
		pictype = (tr >> 13) & 3;
		tr = (tr & 0x1fff) * 96;

		if (kfifo_get(&newframe_q, &vf) == 0) {
			pr_info("fatal error, no available buffer slot.");
			return IRQ_HANDLED;
		}

		vdts = READ_VREG(VDTS);
		if (last_tr == -1)	/* ignore tr for first time */
			vf->duration = frame_dur;
		else {
			if (tr > last_tr)
				vf->duration = tr - last_tr;
			else
				vf->duration = (96 << 13) + tr - last_tr;

			if (vf->duration > 10 * frame_dur) {
				/* not a reasonable duration,
				 *should not happen
				 */
				vf->duration = frame_dur;
			}
#if 0
			else {
				if (check_frame_duration == 0) {
					frame_dur = vf->duration;
					check_frame_duration = 1;
				}
			}
#endif
		}

		last_tr = tr;
		buffer_index = from & 0x03;

		if (pictype == 0) {	/* I */
			current_vdts = vdts * 90 + 1;
			vf->pts = current_vdts;
			if (wait_key_frame)
				wait_key_frame = 0;
		} else {
			if (wait_key_frame) {
				while (READ_VREG(TO_AMRISC))
					;
				WRITE_VREG(TO_AMRISC, ~(1 << buffer_index));
				WRITE_VREG(FROM_AMRISC, 0);
				return IRQ_HANDLED;
			} else {
				current_vdts +=
					vf->duration - (vf->duration >> 4);
				vf->pts = current_vdts;
			}
		}

		/* pr_info("pts %d, picture type %d\n", vf->pts, pictype); */

		info = READ_VREG(RV_PIC_INFO);
		vf->signal_type = 0;
		vf->index = buffer_index;
		vf->width = info >> 16;
		vf->height = (info >> 4) & 0xfff;
		vf->bufWidth = 1920;
		vf->flag = 0;
		vf->ratio_control = 0;
		set_aspect_ratio(vf, info & 0x0f);
		vf->duration_pulldown = 0;
#ifdef NV21
		vf->type = VIDTYPE_PROGRESSIVE |
				VIDTYPE_VIU_FIELD | VIDTYPE_VIU_NV21;
#else
		vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
#endif
		vf->canvas0Addr = vf->canvas1Addr = index2canvas(buffer_index);
		vf->orientation = 0;
		vf->type_original = vf->type;

		vfbuf_use[buffer_index] = 1;
		vf->mem_handle =
			decoder_bmmu_box_get_mem_handle(
				mm_blk_handle,
				buffer_index);

		kfifo_put(&display_q, (const struct vframe_s *)vf);

		frame_count++;
		vf_notify_receiver(PROVIDER_NAME,
			VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
		WRITE_VREG(FROM_AMRISC, 0);
	}

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	return IRQ_HANDLED;
}

static struct vframe_s *vreal_vf_peek(void *op_arg)
{
	struct vframe_s *vf;

	if (kfifo_peek(&display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vreal_vf_get(void *op_arg)
{
	struct vframe_s *vf;

	if (kfifo_get(&display_q, &vf))
		return vf;

	return NULL;
}

static void vreal_vf_put(struct vframe_s *vf, void *op_arg)
{
	kfifo_put(&recycle_q, (const struct vframe_s *)vf);
}

static int vreal_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_RESET) {
		unsigned long flags;

		amvdec_stop();
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vf_light_unreg_provider(&vreal_vf_prov);
#endif
		spin_lock_irqsave(&lock, flags);
		vreal_local_init();
		vreal_prot_init();
		spin_unlock_irqrestore(&lock, flags);
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vf_reg_provider(&vreal_vf_prov);
#endif
		amvdec_start();
	}
	return 0;
}

static int vreal_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&newframe_q);
	states->buf_avail_num = kfifo_len(&display_q);
	states->buf_recycle_num = kfifo_len(&recycle_q);

	spin_unlock_irqrestore(&lock, flags);

	return 0;
}
#if 0
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
static void vreal_ppmgr_reset(void)
{
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_RESET, NULL);

	vreal_local_init();

	pr_info("vrealdec: vf_ppmgr_reset\n");
}
#endif
#endif

static void vreal_set_clk(struct work_struct *work)
{
	if (frame_dur > 0 &&
		saved_resolution !=
		frame_width * frame_height * (96000 / frame_dur)) {
		int fps = 96000 / frame_dur;

		saved_resolution = frame_width * frame_height * fps;
		vdec_source_changed(VFORMAT_REAL,
			frame_width, frame_height, fps);
	}
}

static void vreal_put_timer_func(unsigned long arg)
{
	struct timer_list *timer = (struct timer_list *)arg;
	/* unsigned int status; */

#if 0
	enum receviver_start_e state = RECEIVER_INACTIVE;

	if (vf_get_receiver(PROVIDER_NAME)) {
		state =
			vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_QUREY_STATE, NULL);
		if ((state == RECEIVER_STATE_NULL)
			|| (state == RECEIVER_STATE_NONE)) {
			/* receiver has no event_cb
			 *or receiver's event_cb does not process this event
			 */
			state = RECEIVER_INACTIVE;
		}
	} else
		state = RECEIVER_INACTIVE;

	if ((READ_VREG(WAIT_BUFFER) != 0) &&
		kfifo_is_empty(&display_q) &&
		kfifo_is_empty(&recycle_q) && (state == RECEIVER_INACTIVE)) {
		pr_info("$$$$$$decoder is waiting for buffer\n");
		if (++wait_buffer_counter > 2) {
			amvdec_stop();

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
			vreal_ppmgr_reset();
#else
			vf_light_unreg_provider(&vreal_vf_prov);
			vreal_local_init();
			vf_reg_provider(&vreal_vf_prov);
#endif
			vreal_prot_init();
			amvdec_start();
		}
	}
#endif

	while (!kfifo_is_empty(&recycle_q) && (READ_VREG(TO_AMRISC) == 0)) {
		struct vframe_s *vf;

		if (kfifo_get(&recycle_q, &vf)) {
			if ((vf->index < VF_BUF_NUM)
				&& (--vfbuf_use[vf->index] == 0)) {
				WRITE_VREG(TO_AMRISC, ~(1 << vf->index));
				vf->index = VF_BUF_NUM;
			}

			kfifo_put(&newframe_q, (const struct vframe_s *)vf);
		}
	}

	schedule_work(&set_clk_work);

	timer->expires = jiffies + PUT_INTERVAL;

	add_timer(timer);
}

int vreal_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	if (!(stat & STAT_VDEC_RUN))
		return -1;

	vstatus->frame_width = vreal_amstream_dec_info.width;
	vstatus->frame_height = vreal_amstream_dec_info.height;
	if (0 != vreal_amstream_dec_info.rate)
		vstatus->frame_rate = 96000 / vreal_amstream_dec_info.rate;
	else
		vstatus->frame_rate = 96000;
	vstatus->error_count = real_err_count;
	vstatus->status =
		((READ_VREG(STATUS_AMRISC) << 16) | fatal_flag) | stat;
	/* pr_info("vreal_dec_status 0x%x\n", vstatus->status); */
	return 0;
}

int vreal_set_isreset(struct vdec_s *vdec, int isreset)
{
	is_reset = isreset;
	return 0;
}

/****************************************/
static int  vreal_canvas_init(void)
{
	int i, ret;
	unsigned long buf_start;
	u32 canvas_width, canvas_height;
	u32 alloc_size, decbuf_size, decbuf_y_size, decbuf_uv_size;

	if (buf_size <= 0x00400000) {
		/* SD only */
		canvas_width = 768;
		canvas_height = 576;
		decbuf_y_size = 0x80000;
		decbuf_uv_size = 0x20000;
		decbuf_size = 0x100000;
	} else {
		/* HD & SD */
	#if 1
		int w = vreal_amstream_dec_info.width;
		int h = vreal_amstream_dec_info.height;
		int align_w, align_h;
		int max, min;

		align_w = ALIGN(w, 64);
		align_h = ALIGN(h, 64);
		if (align_w > align_h) {
			max = align_w;
			min = align_h;
		} else {
			canvas_width = 1920;
			canvas_height = 1088;
			max = align_h;
			min = align_w;
		}
		/* HD & SD */
		if ((max > 1920 || min > 1088) &&
			ALIGN(align_w * align_h * 3/2, SZ_64K) * 9 <=
			buf_size) {
			canvas_width = align_w;
			canvas_height = align_h;
			decbuf_y_size = ALIGN(align_w * align_h, SZ_64K);
			decbuf_uv_size = ALIGN(align_w * align_h/4, SZ_64K);
			decbuf_size = ALIGN(align_w * align_h * 3/2, SZ_64K);
		} else { /*1080p*/
			if (h > w) {
				canvas_width = 1088;
				canvas_height = 1920;
			} else {
				canvas_width = 1920;
				canvas_height = 1088;
			}
			decbuf_y_size = 0x200000;
			decbuf_uv_size = 0x80000;
			decbuf_size = 0x300000;
		}
		#endif
	}

	for (i = 0; i < MAX_BMMU_BUFFER_NUM; i++) {
		/* workspace mem */
		if (i == (MAX_BMMU_BUFFER_NUM - 1))
			alloc_size =  WORKSPACE_SIZE;
		else
			alloc_size = decbuf_size;

		ret = decoder_bmmu_box_alloc_buf_phy(mm_blk_handle, i,
				alloc_size, DRIVER_NAME, &buf_start);
		if (ret < 0)
			return ret;

		if (i == (MAX_BMMU_BUFFER_NUM - 1)) {
			buf_offset = buf_start - RV_AI_BUFF_START_IP;
			continue;
		}

#ifdef NV21
		canvas_config(2 * i + 0,
			buf_start,
			canvas_width, canvas_height,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
		canvas_config(2 * i + 1,
			buf_start +
			decbuf_y_size, canvas_width,
			canvas_height / 2, CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_32X32);
#else
		canvas_config(3 * i + 0,
			buf_start,
			canvas_width, canvas_height,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
		canvas_config(3 * i + 1,
			buf_start +
			decbuf_y_size, canvas_width / 2,
			canvas_height / 2, CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_32X32);
		canvas_config(3 * i + 2,
			buf_start +
			decbuf_y_size + decbuf_uv_size,
			canvas_width / 2, canvas_height / 2,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
#endif
	}

	return 0;
}

static int vreal_prot_init(void)
{
	int r;
#if 1	/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6));
	WRITE_VREG(DOS_SW_RESET0, 0);
#else
	WRITE_RESET_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC);
#endif



	r = vreal_canvas_init();

	/* index v << 16 | u << 8 | y */
#ifdef NV21
	WRITE_VREG(AV_SCRATCH_0, 0x010100);
	WRITE_VREG(AV_SCRATCH_1, 0x030302);
	WRITE_VREG(AV_SCRATCH_2, 0x050504);
	WRITE_VREG(AV_SCRATCH_3, 0x070706);
#else
	WRITE_VREG(AV_SCRATCH_0, 0x020100);
	WRITE_VREG(AV_SCRATCH_1, 0x050403);
	WRITE_VREG(AV_SCRATCH_2, 0x080706);
	WRITE_VREG(AV_SCRATCH_3, 0x0b0a09);
#endif

	/* notify ucode the buffer offset */
	WRITE_VREG(AV_SCRATCH_F, buf_offset);

#if 1	/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	WRITE_VREG(DOS_SW_RESET0, (1 << 9) | (1 << 8));
	WRITE_VREG(DOS_SW_RESET0, 0);
#else
	WRITE_RESET_REG(RESET2_REGISTER, RESET_PIC_DC | RESET_DBLK);
#endif

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);

	WRITE_VREG(FROM_AMRISC, 0);
	WRITE_VREG(TO_AMRISC, 0);
	WRITE_VREG(STATUS_AMRISC, 0);

	WRITE_VREG(RV_PIC_INFO, 0);
	WRITE_VREG(VPTS_TR, 0);
	WRITE_VREG(VDTS, 0);
	WRITE_VREG(SKIP_B_AMRISC, 0);

	WRITE_VREG(MDEC_WIDTH, (frame_width + 15) & 0xfff0);
	WRITE_VREG(MDEC_HEIGHT, (frame_height + 15) & 0xfff0);

	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);

	/* clear wait buffer status */
	WRITE_VREG(WAIT_BUFFER, 0);

#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
#endif
	return r;
}

static void vreal_local_init(void)
{
	int i;

	/* vreal_ratio = vreal_amstream_dec_info.ratio; */
	vreal_ratio = 0x100;

	frame_prog = 0;

	frame_width = vreal_amstream_dec_info.width;
	frame_height = vreal_amstream_dec_info.height;
	frame_dur = vreal_amstream_dec_info.rate;

	for (i = 0; i < VF_BUF_NUM; i++)
		vfbuf_use[i] = 0;

	INIT_KFIFO(display_q);
	INIT_KFIFO(recycle_q);
	INIT_KFIFO(newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &vfpool[i];
		vfpool[i].index = VF_BUF_NUM;
		kfifo_put(&newframe_q, vf);
	}

	if (mm_blk_handle) {
		decoder_bmmu_box_free(mm_blk_handle);
		mm_blk_handle = NULL;
	}

	 mm_blk_handle = decoder_bmmu_box_alloc_box(
			DRIVER_NAME,
			0,
			MAX_BMMU_BUFFER_NUM,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER);

	decoder_state = 1;
	hold = 0;
	last_tr = -1;
	wait_key_frame = 1;
	frame_count = 0;
	current_vdts = 0;
	real_err_count = 0;

	pic_sz_tbl_map = 0;
	saved_resolution = 0;
	fatal_flag = 0;
	wait_buffer_counter = 0;
}

static void load_block_data(void *dest, unsigned int count)
{
	unsigned short *pdest = (unsigned short *)dest;
	unsigned short src_tbl[12];
	unsigned int i;

	src_tbl[0] = RPR_size[vreal_amstream_dec_info.extra + 1];
	memcpy((void *)&src_tbl[1], vreal_amstream_dec_info.param,
		   2 << src_tbl[0]);

#if 0
	for (i = 0; i < 12; i++)
		pr_info("src_tbl[%d]: 0x%x\n", i, src_tbl[i]);
#endif

	for (i = 0; i < count / 4; i++) {
		pdest[i * 4] = src_tbl[i * 4 + 3];
		pdest[i * 4 + 1] = src_tbl[i * 4 + 2];
		pdest[i * 4 + 2] = src_tbl[i * 4 + 1];
		pdest[i * 4 + 3] = src_tbl[i * 4];
	}

	pic_sz_tbl_map = dma_map_single(amports_get_dma_device(), &pic_sz_tbl,
				sizeof(pic_sz_tbl), DMA_TO_DEVICE);

}

s32 vreal_init(struct vdec_s *vdec)
{
	int ret = -1, size = -1;
	char fw_name[32] = {0};
	char *buf = vmalloc(0x1000 * 16);

	if (IS_ERR_OR_NULL(buf))
		return -ENOMEM;

	pr_info("vreal_init\n");

	init_timer(&recycle_timer);

	stat |= STAT_TIMER_INIT;

	amvdec_enable();

	vreal_local_init();

	ret = rmparser_init(vdec);
	if (ret) {
		amvdec_disable();
		vfree(buf);
		pr_info("rm parser init failed\n");
		return ret;
	}

	if (vreal_amstream_dec_info.format == VIDEO_DEC_FORMAT_REAL_8) {
		if (vreal_amstream_dec_info.param == NULL) {
			rmparser_release();
			amvdec_disable();
			vfree(buf);
			return -1;
		}
		load_block_data((void *)pic_sz_tbl, 12);

		/* TODO: need to load the table into lmem */
		WRITE_VREG(LMEM_DMA_ADR, (unsigned int)pic_sz_tbl_map);
		WRITE_VREG(LMEM_DMA_COUNT, 10);
		WRITE_VREG(LMEM_DMA_CTRL, 0xc178 | (3 << 11));
		while (READ_VREG(LMEM_DMA_CTRL) & 0x8000)
			;
		size = get_firmware_data(VIDEO_DEC_REAL_V8, buf);
		strncpy(fw_name, "vreal_mc_8", sizeof(fw_name));

		pr_info("load VIDEO_DEC_FORMAT_REAL_8\n");
	} else if (vreal_amstream_dec_info.format == VIDEO_DEC_FORMAT_REAL_9) {
		size = get_firmware_data(VIDEO_DEC_REAL_V9, buf);
		strncpy(fw_name, "vreal_mc_9", sizeof(fw_name));

		pr_info("load VIDEO_DEC_FORMAT_REAL_9\n");
	} else
		pr_info("unsurpported real format\n");

	if (size < 0) {
		rmparser_release();
		amvdec_disable();
		pr_err("get firmware fail.");
		vfree(buf);
		return -1;
	}

	ret = amvdec_loadmc_ex(VFORMAT_REAL, fw_name, buf);
	if (ret < 0) {
		rmparser_release();
		amvdec_disable();
		vfree(buf);
		pr_err("%s: the %s fw loading failed, err: %x\n",
			fw_name, tee_enabled() ? "TEE" : "local", ret);
		return -EBUSY;
	}

	vfree(buf);

	stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	ret = vreal_prot_init();
	if (ret < 0) {
		rmparser_release();
		amvdec_disable();
		return ret;
	}
	if (vdec_request_irq(VDEC_IRQ_1,  vreal_isr,
		    "vreal-irq", (void *)vreal_dec_id)) {
		rmparser_release();
		amvdec_disable();

		pr_info("vreal irq register error.\n");
		return -ENOENT;
	}

	stat |= STAT_ISR_REG;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
	vf_provider_init(&vreal_vf_prov, PROVIDER_NAME, &vreal_vf_provider,
					 NULL);
	vf_reg_provider(&vreal_vf_prov);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
#else
	vf_provider_init(&vreal_vf_prov, PROVIDER_NAME, &vreal_vf_provider,
					 NULL);
	vf_reg_provider(&vreal_vf_prov);
#endif

	if (!is_reset)
		vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_FR_HINT,
			(void *)((unsigned long)vreal_amstream_dec_info.rate));

	stat |= STAT_VF_HOOK;

	recycle_timer.data = (ulong)&recycle_timer;
	recycle_timer.function = vreal_put_timer_func;
	recycle_timer.expires = jiffies + PUT_INTERVAL;

	add_timer(&recycle_timer);

	stat |= STAT_TIMER_ARM;

	amvdec_start();

	stat |= STAT_VDEC_RUN;

	pr_info("vreal init finished\n");

	return 0;
}

void vreal_set_fatal_flag(int flag)
{
	if (flag)
		fatal_flag = PARSER_FATAL_ERROR;
}

static int amvdec_real_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;

	if (pdata == NULL) {
		pr_info("amvdec_real memory resource undefined.\n");
		return -EFAULT;
	}
	if (pdata->sys_info)
		vreal_amstream_dec_info = *pdata->sys_info;

	pdata->dec_status = vreal_dec_status;
	pdata->set_isreset = vreal_set_isreset;
	is_reset = 0;

	if (vreal_init(pdata) < 0) {
		pr_info("amvdec_real init failed.\n");
		pdata->dec_status = NULL;
		return -ENODEV;
	}
	INIT_WORK(&set_clk_work, vreal_set_clk);
	return 0;
}

static int amvdec_real_remove(struct platform_device *pdev)
{
	cancel_work_sync(&set_clk_work);
	if (stat & STAT_VDEC_RUN) {
		amvdec_stop();
		stat &= ~STAT_VDEC_RUN;
	}

	if (stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_1, (void *)vreal_dec_id);
		stat &= ~STAT_ISR_REG;
	}

	if (stat & STAT_TIMER_ARM) {
		del_timer_sync(&recycle_timer);
		stat &= ~STAT_TIMER_ARM;
	}

	if (stat & STAT_VF_HOOK) {
		if (!is_reset)
			vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_FR_END_HINT, NULL);

		vf_unreg_provider(&vreal_vf_prov);
		stat &= ~STAT_VF_HOOK;
	}

	if (pic_sz_tbl_map != 0) {
		dma_unmap_single(NULL, pic_sz_tbl_map, sizeof(pic_sz_tbl),
						 DMA_TO_DEVICE);
	}

	rmparser_release();

	vdec_source_changed(VFORMAT_REAL, 0, 0, 0);

	amvdec_disable();

	if (mm_blk_handle) {
		decoder_bmmu_box_free(mm_blk_handle);
		mm_blk_handle = NULL;
	}
	pr_info("frame duration %d, frames %d\n", frame_dur, frame_count);
	return 0;
}

/****************************************/

static struct platform_driver amvdec_real_driver = {
	.probe = amvdec_real_probe,
	.remove = amvdec_real_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

static struct codec_profile_t amvdec_real_profile = {
	.name = "real",
	.profile = "rmvb,1080p+"
};
static struct mconfig real_configs[] = {
	MC_PU32("stat", &stat),
};
static struct mconfig_node real_node;

static int __init amvdec_real_driver_init_module(void)
{
	pr_debug("amvdec_real module init\n");

	if (platform_driver_register(&amvdec_real_driver)) {
		pr_err("failed to register amvdec_real driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&amvdec_real_profile);
	INIT_REG_NODE_CONFIGS("media.decoder", &real_node,
		"real", real_configs, CONFIG_FOR_R);
	return 0;
}

static void __exit amvdec_real_driver_remove_module(void)
{
	pr_debug("amvdec_real module remove.\n");

	platform_driver_unregister(&amvdec_real_driver);
}

/****************************************/

module_init(amvdec_real_driver_init_module);
module_exit(amvdec_real_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC REAL Video Decoder Driver");
MODULE_LICENSE("GPL");
