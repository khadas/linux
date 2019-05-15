/*
 * drivers/amlogic/amports/vmpeg4.c
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

#define DEBUG
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/canvas/canvas.h>

#include <linux/amlogic/media/utils/vdec_reg.h>
#include "vmpeg4.h"
#include <linux/amlogic/media/registers/register.h>
#include "../../../stream_input/amports/amports_priv.h"
#include "../utils/decoder_mmu_box.h"
#include "../utils/decoder_bmmu_box.h"
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/tee.h>

/* #define CONFIG_AM_VDEC_MPEG4_LOG */
#ifdef CONFIG_AM_VDEC_MPEG4_LOG
#define AMLOG
#define LOG_LEVEL_VAR       amlog_level_vmpeg4
#define LOG_MASK_VAR        amlog_mask_vmpeg4
#define LOG_LEVEL_ERROR     0
#define LOG_LEVEL_INFO      1
#define LOG_LEVEL_DESC  "0:ERROR, 1:INFO"
#define LOG_MASK_PTS    0x01
#define LOG_MASK_DESC   "0x01:DEBUG_PTS"
#endif

#include <linux/amlogic/media/utils/amlog.h>

MODULE_AMLOG(LOG_LEVEL_ERROR, 0, LOG_LEVEL_DESC, LOG_DEFAULT_MASK_DESC);

#include "../utils/amvdec.h"
#include "../utils/vdec.h"
#include "../utils/firmware.h"

#define DRIVER_NAME "amvdec_mpeg4"
#define MODULE_NAME "amvdec_mpeg4"

#define DEBUG_PTS

/* /#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#define NV21
/* /#endif */

#define I_PICTURE   0
#define P_PICTURE   1
#define B_PICTURE   2

#define ORI_BUFFER_START_ADDR   0x01000000

#define INTERLACE_FLAG          0x80
#define TOP_FIELD_FIRST_FLAG    0x40

/* protocol registers */
#define MP4_PIC_RATIO       AV_SCRATCH_5
#define MP4_RATE            AV_SCRATCH_3
#define MP4_ERR_COUNT       AV_SCRATCH_6
#define MP4_PIC_WH          AV_SCRATCH_7
#define MREG_BUFFERIN       AV_SCRATCH_8
#define MREG_BUFFEROUT      AV_SCRATCH_9
#define MP4_NOT_CODED_CNT   AV_SCRATCH_A
#define MP4_VOP_TIME_INC    AV_SCRATCH_B
#define MP4_OFFSET_REG      AV_SCRATCH_C
#define MP4_SYS_RATE        AV_SCRATCH_E
#define MEM_OFFSET_REG      AV_SCRATCH_F

#define PARC_FORBIDDEN              0
#define PARC_SQUARE                 1
#define PARC_CIF                    2
#define PARC_10_11                  3
#define PARC_16_11                  4
#define PARC_40_33                  5
#define PARC_RESERVED               6
/* values between 6 and 14 are reserved */
#define PARC_EXTENDED              15

#define VF_POOL_SIZE          32
#define DECODE_BUFFER_NUM_MAX 8
#define PUT_INTERVAL        (HZ/100)
#define WORKSPACE_SIZE		(1 * SZ_1M)
#define MAX_BMMU_BUFFER_NUM	(DECODE_BUFFER_NUM_MAX + 1)
#define DCAC_BUFF_START_IP	0x02b00000


#define RATE_DETECT_COUNT   5
#define DURATION_UNIT       96000
#define PTS_UNIT            90000

#define DUR2PTS(x) ((x) - ((x) >> 4))

static struct vframe_s *vmpeg_vf_peek(void *);
static struct vframe_s *vmpeg_vf_get(void *);
static void vmpeg_vf_put(struct vframe_s *, void *);
static int vmpeg_vf_states(struct vframe_states *states, void *);
static int vmpeg_event_cb(int type, void *data, void *private_data);

static int vmpeg4_prot_init(void);
static void vmpeg4_local_init(void);

static const char vmpeg4_dec_id[] = "vmpeg4-dev";

#define PROVIDER_NAME   "decoder.mpeg4"

/*
 *int query_video_status(int type, int *value);
 */
static const struct vframe_operations_s vmpeg_vf_provider = {
	.peek = vmpeg_vf_peek,
	.get = vmpeg_vf_get,
	.put = vmpeg_vf_put,
	.event_cb = vmpeg_event_cb,
	.vf_states = vmpeg_vf_states,
};
static void *mm_blk_handle;
static struct vframe_provider_s vmpeg_vf_prov;

static DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
static DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
static DECLARE_KFIFO(recycle_q, struct vframe_s *, VF_POOL_SIZE);

static struct vframe_s vfpool[VF_POOL_SIZE];
static s32 vfbuf_use[DECODE_BUFFER_NUM_MAX];
static u32 frame_width, frame_height, frame_dur, frame_prog;
static u32 saved_resolution;
static struct timer_list recycle_timer;
static u32 stat;
static u32 buf_size = 32 * 1024 * 1024;
static u32 buf_offset;
static u32 vmpeg4_ratio;
static u64 vmpeg4_ratio64;
static u32 rate_detect;
static u32 vmpeg4_rotation;
static u32 fr_hint_status;

static u32 total_frame;
static u32 last_vop_time_inc, last_duration;
static u32 last_anch_pts, vop_time_inc_since_last_anch,
	   frame_num_since_last_anch;
static u64 last_anch_pts_us64;
static struct vdec_info *gvs;

#ifdef CONFIG_AM_VDEC_MPEG4_LOG
u32 pts_hit, pts_missed, pts_i_hit, pts_i_missed;
#endif

static struct work_struct reset_work;
static struct work_struct notify_work;
static struct work_struct set_clk_work;
static bool is_reset;

static DEFINE_SPINLOCK(lock);

static struct dec_sysinfo vmpeg4_amstream_dec_info;

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
	const u32 canvas_tab[8] = {
#ifdef NV21
		0x010100, 0x030302, 0x050504, 0x070706,
		0x090908, 0x0b0b0a, 0x0d0d0c, 0x0f0f0e
#else
		0x020100, 0x050403, 0x080706, 0x0b0a09,
		0x0e0d0c, 0x11100f, 0x141312, 0x171615
#endif
	};

	return canvas_tab[index];
}

static void set_aspect_ratio(struct vframe_s *vf, unsigned int pixel_ratio)
{
	int ar = 0;
	unsigned int num = 0;
	unsigned int den = 0;

	if (vmpeg4_ratio64 != 0) {
		num = vmpeg4_ratio64 >> 32;
		den = vmpeg4_ratio64 & 0xffffffff;
	} else {
		num = vmpeg4_ratio >> 16;
		den = vmpeg4_ratio & 0xffff;

	}
	if ((num == 0) || (den == 0)) {
		num = 1;
		den = 1;
	}

	if (vmpeg4_ratio == 0) {
		vf->ratio_control |= (0x90 << DISP_RATIO_ASPECT_RATIO_BIT);
		/* always stretch to 16:9 */
	} else if (pixel_ratio > 0x0f) {
		num = (pixel_ratio >> 8) *
			vmpeg4_amstream_dec_info.width * num;
		ar = div_u64((pixel_ratio & 0xff) *
			vmpeg4_amstream_dec_info.height * den * 0x100ULL +
			(num >> 1), num);
	} else {
		switch (aspect_ratio_table[pixel_ratio]) {
		case 0:
			num = vmpeg4_amstream_dec_info.width * num;
			ar = (vmpeg4_amstream_dec_info.height * den * 0x100 +
				  (num >> 1)) / num;
			break;
		case 1:
			num = vf->width * num;
			ar = (vf->height * den * 0x100 + (num >> 1)) / num;
			break;
		case 2:
			num = (vf->width * 12) * num;
			ar = (vf->height * den * 0x100 * 11 +
				  ((num) >> 1)) / num;
			break;
		case 3:
			num = (vf->width * 10) * num;
			ar = (vf->height * den * 0x100 * 11 + (num >> 1)) /
				num;
			break;
		case 4:
			num = (vf->width * 16) * num;
			ar = (vf->height * den * 0x100 * 11 + (num >> 1)) /
				num;
			break;
		case 5:
			num = (vf->width * 40) * num;
			ar = (vf->height * den * 0x100 * 33 + (num >> 1)) /
				num;
			break;
		default:
			num = vf->width * num;
			ar = (vf->height * den * 0x100 + (num >> 1)) / num;
			break;
		}
	}

	ar = min(ar, DISP_RATIO_ASPECT_RATIO_MAX);

	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);
}

static irqreturn_t vmpeg4_isr(int irq, void *dev_id)
{
	u32 reg;
	struct vframe_s *vf = NULL;
	u32 picture_type;
	u32 buffer_index;
	u32 pts, pts_valid = 0, offset = 0;
	u64 pts_us64 = 0;
	u32 rate, vop_time_inc, repeat_cnt, duration = 3200;

	reg = READ_VREG(MREG_BUFFEROUT);

	if (reg) {
		buffer_index = reg & 0x7;
		picture_type = (reg >> 3) & 7;
		rate = READ_VREG(MP4_RATE);
		repeat_cnt = READ_VREG(MP4_NOT_CODED_CNT);
		vop_time_inc = READ_VREG(MP4_VOP_TIME_INC);

		if (buffer_index >= DECODE_BUFFER_NUM_MAX) {
			pr_err("fatal error, invalid buffer index.");
			return IRQ_HANDLED;
		}

		if (vmpeg4_amstream_dec_info.width == 0) {
			vmpeg4_amstream_dec_info.width =
				READ_VREG(MP4_PIC_WH) >> 16;
		}
#if 0
		else {
			pr_info("info width = %d, ucode width = %d\n",
				   vmpeg4_amstream_dec_info.width,
				   READ_VREG(MP4_PIC_WH) >> 16);
		}
#endif

		if (vmpeg4_amstream_dec_info.height == 0) {
			vmpeg4_amstream_dec_info.height =
				READ_VREG(MP4_PIC_WH) & 0xffff;
		}
#if 0
		else {
			pr_info("info height = %d, ucode height = %d\n",
				   vmpeg4_amstream_dec_info.height,
				   READ_VREG(MP4_PIC_WH) & 0xffff);
		}
#endif
		if (vmpeg4_amstream_dec_info.rate == 0
		|| vmpeg4_amstream_dec_info.rate > 96000) {
			/* if ((rate >> 16) != 0) { */
			if ((rate & 0xffff) != 0 && (rate >> 16) != 0) {
				vmpeg4_amstream_dec_info.rate =
					(rate >> 16) * DURATION_UNIT /
					(rate & 0xffff);
				duration = vmpeg4_amstream_dec_info.rate;
				if (fr_hint_status == VDEC_NEED_HINT) {
					schedule_work(&notify_work);
					fr_hint_status = VDEC_HINTED;
				}
			} else if (rate_detect < RATE_DETECT_COUNT) {
				if (vop_time_inc < last_vop_time_inc) {
					duration =
						vop_time_inc + rate -
						last_vop_time_inc;
				} else {
					duration =
					vop_time_inc - last_vop_time_inc;
				}

				if (duration == last_duration) {
					rate_detect++;
					if (rate_detect >= RATE_DETECT_COUNT) {
						vmpeg4_amstream_dec_info.rate =
						duration * DURATION_UNIT /
						rate;
						duration =
						vmpeg4_amstream_dec_info.rate;
					}
				} else
					rate_detect = 0;

				last_duration = duration;
			}
		} else {
			duration = vmpeg4_amstream_dec_info.rate;
#if 0
			pr_info("info rate = %d, ucode rate = 0x%x:0x%x\n",
				   vmpeg4_amstream_dec_info.rate,
				   READ_VREG(MP4_RATE), vop_time_inc);
#endif
		}

		if ((picture_type == I_PICTURE) ||
				(picture_type == P_PICTURE)) {
			offset = READ_VREG(MP4_OFFSET_REG);
			/*2500-->3000,because some mpeg4
			 *video may checkout failed;
			 *may have av sync problem.can changed small later.
			 *263 may need small?
			 */
			if (pts_lookup_offset_us64
				(PTS_TYPE_VIDEO, offset, &pts, 3000,
				 &pts_us64) == 0) {
				pts_valid = 1;
				last_anch_pts = pts;
				last_anch_pts_us64 = pts_us64;
#ifdef CONFIG_AM_VDEC_MPEG4_LOG
				pts_hit++;
#endif
			} else {
#ifdef CONFIG_AM_VDEC_MPEG4_LOG
				pts_missed++;
#endif
			}
#ifdef CONFIG_AM_VDEC_MPEG4_LOG
			amlog_mask(LOG_MASK_PTS,
				"I offset 0x%x, pts_valid %d pts=0x%x\n",
				offset, pts_valid, pts);
#endif
		}

		if (pts_valid) {
			last_anch_pts = pts;
			last_anch_pts_us64 = pts_us64;
			frame_num_since_last_anch = 0;
			vop_time_inc_since_last_anch = 0;
		} else {
			pts = last_anch_pts;
			pts_us64 = last_anch_pts_us64;

			if ((rate != 0) && ((rate >> 16) == 0)
				&& vmpeg4_amstream_dec_info.rate == 0) {
				/* variable PTS rate */
				/*bug on variable pts calc,
				 *do as dixed vop first if we
				 *have rate setting before.
				 */
				if (vop_time_inc > last_vop_time_inc) {
					vop_time_inc_since_last_anch +=
					vop_time_inc - last_vop_time_inc;
				} else {
					vop_time_inc_since_last_anch +=
						vop_time_inc + rate -
						last_vop_time_inc;
				}

				pts += vop_time_inc_since_last_anch *
					PTS_UNIT / rate;
				pts_us64 += (vop_time_inc_since_last_anch *
					PTS_UNIT / rate) * 100 / 9;

				if (vop_time_inc_since_last_anch > (1 << 14)) {
					/* avoid overflow */
					last_anch_pts = pts;
					last_anch_pts_us64 = pts_us64;
					vop_time_inc_since_last_anch = 0;
				}
			} else {
				/* fixed VOP rate */
				frame_num_since_last_anch++;
				pts += DUR2PTS(frame_num_since_last_anch *
					vmpeg4_amstream_dec_info.rate);
				pts_us64 += DUR2PTS(frame_num_since_last_anch *
					vmpeg4_amstream_dec_info.rate) *
					100 / 9;

				if (frame_num_since_last_anch > (1 << 15)) {
					/* avoid overflow */
					last_anch_pts = pts;
					last_anch_pts_us64 = pts_us64;
					frame_num_since_last_anch = 0;
				}
			}
		}

		if (reg & INTERLACE_FLAG) {	/* interlace */
			if (kfifo_get(&newframe_q, &vf) == 0) {
				printk
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}
			vf->signal_type = 0;
			vf->index = buffer_index;
			vf->width = vmpeg4_amstream_dec_info.width;
			vf->height = vmpeg4_amstream_dec_info.height;
			vf->bufWidth = 1920;
			vf->flag = 0;
			vf->orientation = vmpeg4_rotation;
			vf->pts = pts;
			vf->pts_us64 = pts_us64;
			vf->duration = duration >> 1;
			vf->duration_pulldown = 0;
			vf->type = (reg & TOP_FIELD_FIRST_FLAG) ?
				VIDTYPE_INTERLACE_TOP :
				VIDTYPE_INTERLACE_BOTTOM;
#ifdef NV21
			vf->type |= VIDTYPE_VIU_NV21;
#endif
			vf->canvas0Addr = vf->canvas1Addr =
				index2canvas(buffer_index);
			vf->type_original = vf->type;

			set_aspect_ratio(vf, READ_VREG(MP4_PIC_RATIO));

			vfbuf_use[buffer_index]++;
			vf->mem_handle =
				decoder_bmmu_box_get_mem_handle(
					mm_blk_handle,
					buffer_index);

			kfifo_put(&display_q, (const struct vframe_s *)vf);

			vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);

			if (kfifo_get(&newframe_q, &vf) == 0) {
				printk(
				"fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}
			vf->signal_type = 0;
			vf->index = buffer_index;
			vf->width = vmpeg4_amstream_dec_info.width;
			vf->height = vmpeg4_amstream_dec_info.height;
			vf->bufWidth = 1920;
			vf->flag = 0;
			vf->orientation = vmpeg4_rotation;

			vf->pts = 0;
			vf->pts_us64 = 0;
			vf->duration = duration >> 1;

			vf->duration_pulldown = 0;
			vf->type = (reg & TOP_FIELD_FIRST_FLAG) ?
			VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;
#ifdef NV21
			vf->type |= VIDTYPE_VIU_NV21;
#endif
			vf->canvas0Addr = vf->canvas1Addr =
					index2canvas(buffer_index);
			vf->type_original = vf->type;

			set_aspect_ratio(vf, READ_VREG(MP4_PIC_RATIO));

			vfbuf_use[buffer_index]++;
			vf->mem_handle =
				decoder_bmmu_box_get_mem_handle(
					mm_blk_handle,
					buffer_index);

			amlog_mask(LOG_MASK_PTS,
			"[%s:%d] [inte] dur=0x%x rate=%d picture_type=%d\n",
				__func__, __LINE__, vf->duration,
				vmpeg4_amstream_dec_info.rate, picture_type);

			kfifo_put(&display_q, (const struct vframe_s *)vf);

			vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);

		} else {	/* progressive */
			if (kfifo_get(&newframe_q, &vf) == 0) {
				printk
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}
			vf->signal_type = 0;
			vf->index = buffer_index;
			vf->width = vmpeg4_amstream_dec_info.width;
			vf->height = vmpeg4_amstream_dec_info.height;
			vf->bufWidth = 1920;
			vf->flag = 0;
			vf->orientation = vmpeg4_rotation;
			vf->pts = pts;
			vf->pts_us64 = pts_us64;
			vf->duration = duration;
			vf->duration_pulldown = repeat_cnt * duration;
#ifdef NV21
			vf->type =
				VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
#else
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
#endif
			vf->canvas0Addr = vf->canvas1Addr =
					index2canvas(buffer_index);
			vf->type_original = vf->type;

			set_aspect_ratio(vf, READ_VREG(MP4_PIC_RATIO));

			amlog_mask(LOG_MASK_PTS,
			"[%s:%d] [prog] dur=0x%x rate=%d picture_type=%d\n",
			__func__, __LINE__, vf->duration,
			vmpeg4_amstream_dec_info.rate, picture_type);

			vfbuf_use[buffer_index]++;
			vf->mem_handle =
				decoder_bmmu_box_get_mem_handle(
					mm_blk_handle,
					buffer_index);

			kfifo_put(&display_q, (const struct vframe_s *)vf);

			vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);
		}

		total_frame += repeat_cnt + 1;

		WRITE_VREG(MREG_BUFFEROUT, 0);

		last_vop_time_inc = vop_time_inc;

		/*count info*/
		gvs->frame_dur = duration;
		vdec_count_info(gvs, 0, offset);
	}

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	return IRQ_HANDLED;
}

static struct vframe_s *vmpeg_vf_peek(void *op_arg)
{
	struct vframe_s *vf;

	if (kfifo_peek(&display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vmpeg_vf_get(void *op_arg)
{
	struct vframe_s *vf;

	if (kfifo_get(&display_q, &vf))
		return vf;

	return NULL;
}

static void vmpeg_vf_put(struct vframe_s *vf, void *op_arg)
{
	kfifo_put(&recycle_q, (const struct vframe_s *)vf);
}

static int vmpeg_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_RESET) {
		unsigned long flags;

		amvdec_stop();
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vf_light_unreg_provider(&vmpeg_vf_prov);
#endif
		spin_lock_irqsave(&lock, flags);
		vmpeg4_local_init();
		vmpeg4_prot_init();
		spin_unlock_irqrestore(&lock, flags);
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vf_reg_provider(&vmpeg_vf_prov);
#endif
		amvdec_start();
	}
	return 0;
}

static int vmpeg_vf_states(struct vframe_states *states, void *op_arg)
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

static void vmpeg4_notify_work(struct work_struct *work)
{
	pr_info("frame duration changed %d\n", vmpeg4_amstream_dec_info.rate);
	vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_FR_HINT,
					(void *)
					((unsigned long)
					vmpeg4_amstream_dec_info.rate));
	return;
}

static void reset_do_work(struct work_struct *work)
{
	unsigned long flags;

	amvdec_stop();
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
	vf_light_unreg_provider(&vmpeg_vf_prov);
#endif
	spin_lock_irqsave(&lock, flags);
	vmpeg4_local_init();
	vmpeg4_prot_init();
	spin_unlock_irqrestore(&lock, flags);
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
	vf_reg_provider(&vmpeg_vf_prov);
#endif
	amvdec_start();
}

static void vmpeg4_set_clk(struct work_struct *work)
{
	if (frame_dur > 0 && saved_resolution !=
		frame_width * frame_height * (96000 / frame_dur)) {
		int fps = 96000 / frame_dur;

		saved_resolution = frame_width * frame_height * fps;
		vdec_source_changed(VFORMAT_MPEG4,
			frame_width, frame_height, fps);
	}
}

static void vmpeg_put_timer_func(unsigned long arg)
{
	struct timer_list *timer = (struct timer_list *)arg;

	while (!kfifo_is_empty(&recycle_q) && (READ_VREG(MREG_BUFFERIN) == 0)) {
		struct vframe_s *vf;

		if (kfifo_get(&recycle_q, &vf)) {
			if ((vf->index < DECODE_BUFFER_NUM_MAX)
				&& (--vfbuf_use[vf->index] == 0)) {
				WRITE_VREG(MREG_BUFFERIN, ~(1 << vf->index));
				vf->index = DECODE_BUFFER_NUM_MAX;
		}
			kfifo_put(&newframe_q, (const struct vframe_s *)vf);
		}
	}

	schedule_work(&set_clk_work);

	if (READ_VREG(AV_SCRATCH_L)) {
		pr_info("mpeg4 fatal error happened,need reset    !!\n");
		schedule_work(&reset_work);
	}


	timer->expires = jiffies + PUT_INTERVAL;

	add_timer(timer);
}

int vmpeg4_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	if (!(stat & STAT_VDEC_RUN))
		return -1;

	vstatus->frame_width = vmpeg4_amstream_dec_info.width;
	vstatus->frame_height = vmpeg4_amstream_dec_info.height;
	if (0 != vmpeg4_amstream_dec_info.rate)
		vstatus->frame_rate =
			DURATION_UNIT / vmpeg4_amstream_dec_info.rate;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = READ_VREG(MP4_ERR_COUNT);
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

int vmpeg4_set_isreset(struct vdec_s *vdec, int isreset)
{
	is_reset = isreset;
	return 0;
}

static int vmpeg4_vdec_info_init(void)
{
	gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);
	if (NULL == gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}
	return 0;
}

/****************************************/
static int vmpeg4_canvas_init(void)
{
	int i, ret;
	u32 canvas_width, canvas_height;
	unsigned long buf_start;
	u32 alloc_size, decbuf_size, decbuf_y_size, decbuf_uv_size;

	if (buf_size <= 0x00400000) {
		/* SD only */
		canvas_width = 768;
		canvas_height = 576;
		decbuf_y_size = 0x80000;
		decbuf_uv_size = 0x20000;
		decbuf_size = 0x100000;
	} else {
		int w = vmpeg4_amstream_dec_info.width;
		int h = vmpeg4_amstream_dec_info.height;
		int align_w, align_h;
		int max, min;

		align_w = ALIGN(w, 64);
		align_h = ALIGN(h, 64);
		if (align_w > align_h) {
			max = align_w;
			min = align_h;
		} else {
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
			buf_offset = buf_start - DCAC_BUFF_START_IP;
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

static int vmpeg4_prot_init(void)
{
	int r;
#if 1	/* /MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6));
	WRITE_VREG(DOS_SW_RESET0, 0);
#else
	WRITE_RESET_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC);
#endif

	r = vmpeg4_canvas_init();

	/* index v << 16 | u << 8 | y */
#ifdef NV21
	WRITE_VREG(AV_SCRATCH_0, 0x010100);
	WRITE_VREG(AV_SCRATCH_1, 0x030302);
	WRITE_VREG(AV_SCRATCH_2, 0x050504);
	WRITE_VREG(AV_SCRATCH_3, 0x070706);
	WRITE_VREG(AV_SCRATCH_G, 0x090908);
	WRITE_VREG(AV_SCRATCH_H, 0x0b0b0a);
	WRITE_VREG(AV_SCRATCH_I, 0x0d0d0c);
	WRITE_VREG(AV_SCRATCH_J, 0x0f0f0e);
#else
	WRITE_VREG(AV_SCRATCH_0, 0x020100);
	WRITE_VREG(AV_SCRATCH_1, 0x050403);
	WRITE_VREG(AV_SCRATCH_2, 0x080706);
	WRITE_VREG(AV_SCRATCH_3, 0x0b0a09);
	WRITE_VREG(AV_SCRATCH_G, 0x0e0d0c);
	WRITE_VREG(AV_SCRATCH_H, 0x11100f);
	WRITE_VREG(AV_SCRATCH_I, 0x141312);
	WRITE_VREG(AV_SCRATCH_J, 0x171615);
#endif
	WRITE_VREG(AV_SCRATCH_L, 0);/*clearfatal error flag*/

	/* notify ucode the buffer offset */
	WRITE_VREG(AV_SCRATCH_F, buf_offset);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);

	/* clear repeat count */
	WRITE_VREG(MP4_NOT_CODED_CNT, 0);

	WRITE_VREG(MREG_BUFFERIN, 0);
	WRITE_VREG(MREG_BUFFEROUT, 0);

	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);



#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
#endif

#if 1/* /MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	pr_debug("mpeg4 meson8 prot init\n");
	WRITE_VREG(MDEC_PIC_DC_THRESH, 0x404038aa);
#endif

	WRITE_VREG(MP4_PIC_WH, (vmpeg4_amstream_dec_info.
		width << 16) | vmpeg4_amstream_dec_info.height);
	WRITE_VREG(MP4_SYS_RATE, vmpeg4_amstream_dec_info.rate);
	return r;
}

static void vmpeg4_local_init(void)
{
	int i;

	vmpeg4_ratio = vmpeg4_amstream_dec_info.ratio;

	vmpeg4_ratio64 = vmpeg4_amstream_dec_info.ratio64;

	vmpeg4_rotation =
		(((unsigned long) vmpeg4_amstream_dec_info.param)
			>> 16) & 0xffff;

	frame_width = frame_height = frame_dur = frame_prog = 0;

	total_frame = 0;
	saved_resolution = 0;
	last_anch_pts = 0;

	last_anch_pts_us64 = 0;

	last_vop_time_inc = last_duration = 0;

	vop_time_inc_since_last_anch = 0;

	frame_num_since_last_anch = 0;

#ifdef CONFIG_AM_VDEC_MPEG4_LOG
	pts_hit = pts_missed = pts_i_hit = pts_i_missed = 0;
#endif

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
		vfbuf_use[i] = 0;

	INIT_KFIFO(display_q);
	INIT_KFIFO(recycle_q);
	INIT_KFIFO(newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &vfpool[i];

		vfpool[i].index = DECODE_BUFFER_NUM_MAX;
		kfifo_put(&newframe_q, (const struct vframe_s *)vf);
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
}

static s32 vmpeg4_init(void)
{
	int trickmode_fffb = 0;
	int size = -1, ret = -1;
	char fw_name[32] = {0};
	char *buf = vmalloc(0x1000 * 16);

	if (IS_ERR_OR_NULL(buf))
		return -ENOMEM;
	amlog_level(LOG_LEVEL_INFO, "vmpeg4_init\n");

	if (vmpeg4_amstream_dec_info.format ==
				VIDEO_DEC_FORMAT_MPEG4_5) {
		size = get_firmware_data(VIDEO_DEC_MPEG4_5, buf);
		strncpy(fw_name, "vmpeg4_mc_5", sizeof(fw_name));

		amlog_level(LOG_LEVEL_INFO, "load VIDEO_DEC_FORMAT_MPEG4_5\n");
	} else if (vmpeg4_amstream_dec_info.format == VIDEO_DEC_FORMAT_H263) {
		size = get_firmware_data(VIDEO_DEC_H263, buf);
		strncpy(fw_name, "h263_mc", sizeof(fw_name));

		pr_info("load VIDEO_DEC_FORMAT_H263\n");
	} else
		pr_err("not supported MPEG4 format %d\n",
				vmpeg4_amstream_dec_info.format);

	if (size < 0) {
		pr_err("get firmware fail.");
		vfree(buf);
		return -1;
	}

	ret = amvdec_loadmc_ex(VFORMAT_MPEG4, fw_name, buf);
	if (ret < 0) {
		amvdec_disable();
		vfree(buf);
		pr_err("%s: the %s fw loading failed, err: %x\n",
			fw_name, tee_enabled() ? "TEE" : "local", ret);
		return -EBUSY;
	}

	vfree(buf);
	stat |= STAT_MC_LOAD;
	query_video_status(0, &trickmode_fffb);

	init_timer(&recycle_timer);
	stat |= STAT_TIMER_INIT;

	if (vdec_request_irq(VDEC_IRQ_1, vmpeg4_isr,
			"vmpeg4-irq", (void *)vmpeg4_dec_id)) {
		amlog_level(LOG_LEVEL_ERROR, "vmpeg4 irq register error.\n");
		return -ENOENT;
	}
	stat |= STAT_ISR_REG;
	vmpeg4_local_init();
	/* enable AMRISC side protocol */
	ret = vmpeg4_prot_init();
	if (ret < 0) 	 {
		if (mm_blk_handle) {
			decoder_bmmu_box_free(mm_blk_handle);
			mm_blk_handle = NULL;
		}
		return ret;
	}
	amvdec_enable();
	fr_hint_status = VDEC_NO_NEED_HINT;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
	vf_provider_init(&vmpeg_vf_prov, PROVIDER_NAME, &vmpeg_vf_provider,
					 NULL);
	vf_reg_provider(&vmpeg_vf_prov);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
#else
	vf_provider_init(&vmpeg_vf_prov, PROVIDER_NAME, &vmpeg_vf_provider,
					 NULL);
	vf_reg_provider(&vmpeg_vf_prov);
#endif
	if (vmpeg4_amstream_dec_info.rate != 0) {
		if (!is_reset)
			vf_notify_receiver(PROVIDER_NAME,
						VFRAME_EVENT_PROVIDER_FR_HINT,
						(void *)
						((unsigned long)
						vmpeg4_amstream_dec_info.rate));
		fr_hint_status = VDEC_HINTED;
	} else
		fr_hint_status = VDEC_NEED_HINT;

	stat |= STAT_VF_HOOK;

	recycle_timer.data = (ulong)&recycle_timer;
	recycle_timer.function = vmpeg_put_timer_func;
	recycle_timer.expires = jiffies + PUT_INTERVAL;

	add_timer(&recycle_timer);

	stat |= STAT_TIMER_ARM;

	amvdec_start();

	stat |= STAT_VDEC_RUN;

	return 0;
}

static int amvdec_mpeg4_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;

	if (pdata == NULL) {
		amlog_level(LOG_LEVEL_ERROR,
			"amvdec_mpeg4 memory resource undefined.\n");
		return -EFAULT;
	}

	if (pdata->sys_info)
		vmpeg4_amstream_dec_info = *pdata->sys_info;

	pdata->dec_status = vmpeg4_dec_status;
	pdata->set_isreset = vmpeg4_set_isreset;
	is_reset = 0;

	INIT_WORK(&reset_work, reset_do_work);
	INIT_WORK(&notify_work, vmpeg4_notify_work);
	INIT_WORK(&set_clk_work, vmpeg4_set_clk);

	vmpeg4_vdec_info_init();

	if (vmpeg4_init() < 0) {
		amlog_level(LOG_LEVEL_ERROR, "amvdec_mpeg4 init failed.\n");
		kfree(gvs);
		gvs = NULL;
		pdata->dec_status = NULL;
		return -ENODEV;
	}

	return 0;
}

static int amvdec_mpeg4_remove(struct platform_device *pdev)
{
	if (stat & STAT_VDEC_RUN) {
		amvdec_stop();
		stat &= ~STAT_VDEC_RUN;
	}

	if (stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_1, (void *)vmpeg4_dec_id);
		stat &= ~STAT_ISR_REG;
	}

	if (stat & STAT_TIMER_ARM) {
		del_timer_sync(&recycle_timer);
		stat &= ~STAT_TIMER_ARM;
	}

	if (stat & STAT_VF_HOOK) {
		if (fr_hint_status == VDEC_HINTED && !is_reset)
			vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_FR_END_HINT, NULL);
		fr_hint_status = VDEC_NO_NEED_HINT;

		vf_unreg_provider(&vmpeg_vf_prov);
		stat &= ~STAT_VF_HOOK;
	}

	cancel_work_sync(&reset_work);
	cancel_work_sync(&notify_work);
	cancel_work_sync(&set_clk_work);

	amvdec_disable();

	if (mm_blk_handle) {
		decoder_bmmu_box_free(mm_blk_handle);
		mm_blk_handle = NULL;
	}

	amlog_mask(LOG_MASK_PTS,
		"pts hit %d, pts missed %d, i hit %d, missed %d\n", pts_hit,
			   pts_missed, pts_i_hit, pts_i_missed);
	amlog_mask(LOG_MASK_PTS, "total frame %d, rate %d\n", total_frame,
			   vmpeg4_amstream_dec_info.rate);
	kfree(gvs);
	gvs = NULL;

	return 0;
}

/****************************************/

static struct platform_driver amvdec_mpeg4_driver = {
	.probe = amvdec_mpeg4_probe,
	.remove = amvdec_mpeg4_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

static struct codec_profile_t amvdec_mpeg4_profile = {
	.name = "mpeg4",
	.profile = ""
};
static struct mconfig mpeg4_configs[] = {
	MC_PU32("stat", &stat),
};
static struct mconfig_node mpeg4_node;

static int __init amvdec_mpeg4_driver_init_module(void)
{
	amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg4 module init\n");

	if (platform_driver_register(&amvdec_mpeg4_driver)) {
		amlog_level(LOG_LEVEL_ERROR,
		"failed to register amvdec_mpeg4 driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&amvdec_mpeg4_profile);
	INIT_REG_NODE_CONFIGS("media.decoder", &mpeg4_node,
		"mpeg4", mpeg4_configs, CONFIG_FOR_R);
	return 0;
}

static void __exit amvdec_mpeg4_driver_remove_module(void)
{
	amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg4 module remove.\n");

	platform_driver_unregister(&amvdec_mpeg4_driver);
}

/****************************************/
module_init(amvdec_mpeg4_driver_init_module);
module_exit(amvdec_mpeg4_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC MPEG4 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
