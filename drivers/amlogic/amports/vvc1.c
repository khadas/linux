/*
 * drivers/amlogic/amports/vvc1.c
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
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/amports/ptsserv.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/canvas/canvas_mgr.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include <linux/slab.h>

#include "vdec_reg.h"
#include "amvdec.h"
#include "arch/register.h"
#include "amports_priv.h"


#define DRIVER_NAME "amvdec_vc1"
#define MODULE_NAME "amvdec_vc1"

#define DEBUG_PTS
#if 1	/* //MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#define NV21
#endif

#define I_PICTURE   0
#define P_PICTURE   1
#define B_PICTURE   2

#define ORI_BUFFER_START_ADDR   0x01000000

#define INTERLACE_FLAG          0x80
#define BOTTOM_FIELD_FIRST_FLAG 0x40

/* protocol registers */
#define VC1_PIC_RATIO       AV_SCRATCH_0
#define VC1_ERROR_COUNT    AV_SCRATCH_6
#define VC1_SOS_COUNT     AV_SCRATCH_7
#define VC1_BUFFERIN       AV_SCRATCH_8
#define VC1_BUFFEROUT      AV_SCRATCH_9
#define VC1_REPEAT_COUNT    AV_SCRATCH_A
#define VC1_TIME_STAMP      AV_SCRATCH_B
#define VC1_OFFSET_REG      AV_SCRATCH_C
#define MEM_OFFSET_REG      AV_SCRATCH_F

#define VF_POOL_SIZE          16
#define DECODE_BUFFER_NUM_MAX 4
#define PUT_INTERVAL        (HZ/100)

#if 1	/* /MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
/* TODO: move to register headers */
#define VPP_VD1_POSTBLEND       (1 << 10)
#define MEM_FIFO_CNT_BIT        16
#define MEM_LEVEL_CNT_BIT       18
#endif
static struct vdec_info *gvs;

static struct vframe_s *vvc1_vf_peek(void *);
static struct vframe_s *vvc1_vf_get(void *);
static void vvc1_vf_put(struct vframe_s *, void *);
static int vvc1_vf_states(struct vframe_states *states, void *);
static int vvc1_event_cb(int type, void *data, void *private_data);

static void vvc1_prot_init(void);
static void vvc1_local_init(void);

static const char vvc1_dec_id[] = "vvc1-dev";

#define PROVIDER_NAME   "decoder.vc1"
static const struct vframe_operations_s vvc1_vf_provider = {
	.peek = vvc1_vf_peek,
	.get = vvc1_vf_get,
	.put = vvc1_vf_put,
	.event_cb = vvc1_event_cb,
	.vf_states = vvc1_vf_states,
};

static struct vframe_provider_s vvc1_vf_prov;

static DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
static DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
static DECLARE_KFIFO(recycle_q, struct vframe_s *, VF_POOL_SIZE);

static struct vframe_s vfpool[VF_POOL_SIZE];
static struct vframe_s vfpool2[VF_POOL_SIZE];
static int cur_pool_idx;

static s32 vfbuf_use[DECODE_BUFFER_NUM_MAX];
static struct timer_list recycle_timer;
static u32 stat;
static unsigned long buf_start;
static u32 buf_size, buf_offset;
static u32 avi_flag;
static u32 keyframe_pts_only;
static u32 vvc1_ratio;
static u32 vvc1_format;

static u32 intra_output;
static u32 frame_width, frame_height, frame_dur;
static u32 saved_resolution;
static u32 pts_by_offset = 1;
static u32 total_frame;
static u32 next_pts;
static u64 next_pts_us64;
static u32 next_IP_pts;
static u64 next_IP_pts_us64;

#ifdef DEBUG_PTS
static u32 pts_hit, pts_missed, pts_i_hit, pts_i_missed;
#endif
static DEFINE_SPINLOCK(lock);

static struct dec_sysinfo vvc1_amstream_dec_info;

struct frm_s {
	int state;
	u32 start_pts;
	int num;
	u32 end_pts;
	u32 rate;
	u32 trymax;
};

static struct frm_s frm;

enum {
	RATE_MEASURE_START_PTS = 0,
	RATE_MEASURE_END_PTS,
	RATE_MEASURE_DONE
};
#define RATE_MEASURE_NUM 8
#define RATE_CORRECTION_THRESHOLD 5
#define RATE_24_FPS  3755	/* 23.97 */
#define RATE_30_FPS  3003	/* 29.97 */
#define DUR2PTS(x) ((x)*90/96)
#define PTS2DUR(x) ((x)*96/90)

static inline int pool_index(struct vframe_s *vf)
{
	if ((vf >= &vfpool[0]) && (vf <= &vfpool[VF_POOL_SIZE - 1]))
		return 0;
	else if ((vf >= &vfpool2[0]) && (vf <= &vfpool2[VF_POOL_SIZE - 1]))
		return 1;
	else
		return -1;
}

static inline bool close_to(int a, int b, int m)
{
	return abs(a - b) < m;
}

static inline u32 index2canvas(u32 index)
{
	const u32 canvas_tab[4] = {
#if 1	/* ALWASY.MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		0x010100, 0x030302, 0x050504, 0x070706
#else
		0x020100, 0x050403, 0x080706, 0x0b0a09
#endif
	};

	return canvas_tab[index];
}

static void set_aspect_ratio(struct vframe_s *vf, unsigned pixel_ratio)
{
	int ar = 0;

	if (vvc1_ratio == 0) {
		/* always stretch to 16:9 */
		vf->ratio_control |= (0x90 << DISP_RATIO_ASPECT_RATIO_BIT);
	} else if (pixel_ratio > 0x0f) {
		ar = (vvc1_amstream_dec_info.height * (pixel_ratio & 0xff) *
			  vvc1_ratio) / (vvc1_amstream_dec_info.width *
							 (pixel_ratio >> 8));
	} else {
		switch (pixel_ratio) {
		case 0:
			ar = (vvc1_amstream_dec_info.height * vvc1_ratio) /
				 vvc1_amstream_dec_info.width;
			break;
		case 1:
			ar = (vf->height * vvc1_ratio) / vf->width;
			break;
		case 2:
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 12);
			break;
		case 3:
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 10);
			break;
		case 4:
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 16);
			break;
		case 5:
			ar = (vf->height * 33 * vvc1_ratio) / (vf->width * 40);
			break;
		case 6:
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 24);
			break;
		case 7:
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 20);
			break;
		case 8:
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 32);
			break;
		case 9:
			ar = (vf->height * 33 * vvc1_ratio) / (vf->width * 80);
			break;
		case 10:
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 18);
			break;
		case 11:
			ar = (vf->height * 11 * vvc1_ratio) / (vf->width * 15);
			break;
		case 12:
			ar = (vf->height * 33 * vvc1_ratio) / (vf->width * 64);
			break;
		case 13:
			ar = (vf->height * 99 * vvc1_ratio) /
				(vf->width * 160);
			break;
		default:
			ar = (vf->height * vvc1_ratio) / vf->width;
			break;
		}
	}

	ar = min(ar, DISP_RATIO_ASPECT_RATIO_MAX);

	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);
	/*vf->ratio_control |= DISP_RATIO_FORCECONFIG | DISP_RATIO_KEEPRATIO;*/
}

static irqreturn_t vvc1_isr(int irq, void *dev_id)
{
	u32 reg;
	struct vframe_s *vf = NULL;
	u32 repeat_count;
	u32 picture_type;
	u32 buffer_index;
	unsigned int pts, pts_valid = 0, offset;
	u32 v_width, v_height, dur;
	u64 pts_us64 = 0;

	reg = READ_VREG(VC1_BUFFEROUT);

	if (reg) {
		v_width = READ_VREG(AV_SCRATCH_J);
		v_height = READ_VREG(AV_SCRATCH_K);

		if (v_width && v_width <= 4096
			&& (v_width != vvc1_amstream_dec_info.width)) {
			pr_info("frame width changed %d to %d\n",
				   vvc1_amstream_dec_info.width, v_width);
			vvc1_amstream_dec_info.width = v_width;
			frame_width = v_width;
		}
		if (v_height && v_height <= 4096
			&& (v_height != vvc1_amstream_dec_info.height)) {
			pr_info("frame height changed %d to %d\n",
				   vvc1_amstream_dec_info.height, v_height);
			vvc1_amstream_dec_info.height = v_height;
			frame_height = v_height;
		}


		repeat_count = READ_VREG(VC1_REPEAT_COUNT);
		buffer_index = ((reg & 0x7) - 1) & 3;
		picture_type = (reg >> 3) & 7;

		if (pts_by_offset) {
			offset = READ_VREG(VC1_OFFSET_REG);
			if (pts_lookup_offset_us64(PTS_TYPE_VIDEO, offset, &pts, 0, &pts_us64) == 0) {
				pts_valid = 1;
				if (keyframe_pts_only)
				{
					//pr_info("PT:%d rpc:%d pts64:%lld\n", picture_type , repeat_count, pts_us64);
					dur = DUR2PTS(vvc1_amstream_dec_info.rate);
					if (picture_type == B_PICTURE)
					{
						next_IP_pts = pts;
						next_IP_pts_us64 = pts_us64;
						pts -= dur;
						pts_us64 -= (dur * 100) / 9;
					}
					else if (next_IP_pts)
					{
						pts = next_IP_pts;
						next_IP_pts = 0;
						pts_us64 = next_IP_pts_us64;
						next_IP_pts_us64 = 0;
					}
				}
#ifdef DEBUG_PTS
				pts_hit++;
#endif
			} else {
#ifdef DEBUG_PTS
				pts_missed++;
#endif
			}
		}

		if (buffer_index >= DECODE_BUFFER_NUM_MAX) {
			pr_info("fatal error, invalid buffer index.");
			return IRQ_HANDLED;
		}

		if ((intra_output == 0) && (picture_type != 0)) {
			WRITE_VREG(VC1_BUFFERIN, ~(1 << buffer_index));
			WRITE_VREG(VC1_BUFFEROUT, 0);
			WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

			return IRQ_HANDLED;
		}

		intra_output = 1;

#ifdef DEBUG_PTS
		if (picture_type == I_PICTURE) {
			/* pr_info("I offset 0x%x,
			pts_valid %d\n", offset, pts_valid); */
			if (!pts_valid)
				pts_i_missed++;
			else
				pts_i_hit++;
		}
#endif

		if ((pts_valid) && (frm.state != RATE_MEASURE_DONE)) {
			if (frm.state == RATE_MEASURE_START_PTS) {
				frm.start_pts = pts;
				frm.state = RATE_MEASURE_END_PTS;
				frm.trymax = RATE_MEASURE_NUM;
			} else if (frm.state == RATE_MEASURE_END_PTS) {
				if (frm.num >= frm.trymax) {
					frm.end_pts = pts;
					frm.rate = (frm.end_pts -
						frm.start_pts) / frm.num;
					pr_info("frate before=%d,%d,num=%d\n",
					frm.rate,
					DUR2PTS(vvc1_amstream_dec_info.rate),
					frm.num);
					/* check if measured rate is same as
					 * settings from upper layer
					 * and correct it if necessary */
					if ((close_to(frm.rate, RATE_30_FPS,
						RATE_CORRECTION_THRESHOLD) &&
						close_to(
						DUR2PTS(
						vvc1_amstream_dec_info.rate),
						RATE_24_FPS,
						RATE_CORRECTION_THRESHOLD))
						||
						(close_to(
						frm.rate, RATE_24_FPS,
						  RATE_CORRECTION_THRESHOLD)
						 &&
						 close_to(DUR2PTS(
						vvc1_amstream_dec_info.rate),
						RATE_30_FPS,
						RATE_CORRECTION_THRESHOLD))) {
							pr_info(
						"vvc1: frate from %d to %d\n",
						vvc1_amstream_dec_info.rate,
						PTS2DUR(frm.rate));

						vvc1_amstream_dec_info.rate =
							PTS2DUR(frm.rate);
						frm.state = RATE_MEASURE_DONE;
					} else if (close_to(frm.rate,
						DUR2PTS(
						vvc1_amstream_dec_info.rate),
						RATE_CORRECTION_THRESHOLD))
						frm.state = RATE_MEASURE_DONE;
					else {	/*maybe still have problem,
						try next double frames.... */
						frm.state = RATE_MEASURE_DONE;
						frm.start_pts = pts;
						frm.state =
						RATE_MEASURE_END_PTS;
						/*60 fps*60 S */
						frm.num = 0;
					}
				}
			}
		}

		if (frm.state != RATE_MEASURE_DONE)
			frm.num += (repeat_count > 1) ? repeat_count : 1;
		if (0 == vvc1_amstream_dec_info.rate)
			vvc1_amstream_dec_info.rate = PTS2DUR(frm.rate);

		if (reg & INTERLACE_FLAG) {	/* interlace */
			if (kfifo_get(&newframe_q, &vf) == 0) {
				pr_info
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}
			vf->signal_type = 0;
			vf->index = buffer_index;
			vf->width = vvc1_amstream_dec_info.width;
			vf->height = vvc1_amstream_dec_info.height;
			vf->bufWidth = 1920;
			vf->flag = 0;

			if (pts_valid) {
				vf->pts = pts;
				vf->pts_us64 = pts_us64;
				if ((repeat_count > 1) && avi_flag) {
					vf->duration =
						vvc1_amstream_dec_info.rate *
						repeat_count >> 1;
					next_pts = pts +
						(vvc1_amstream_dec_info.rate *
						 repeat_count >> 1) * 15 / 16;
					next_pts_us64 = pts_us64 +
						((vvc1_amstream_dec_info.rate *
						repeat_count >> 1) * 15 / 16) *
						100 / 9;
				} else {
					vf->duration =
					vvc1_amstream_dec_info.rate >> 1;
					next_pts = 0;
					next_pts_us64 = 0;
				}
			} else {
				vf->pts = next_pts;
				vf->pts_us64 = next_pts_us64;
				if ((repeat_count > 1) && avi_flag) {
					vf->duration =
						vvc1_amstream_dec_info.rate *
						repeat_count >> 1;
					if (next_pts != 0) {
						next_pts += ((vf->duration) -
						((vf->duration) >> 4));
					}
					if (next_pts_us64 != 0) {
						next_pts_us64 +=
						((vf->duration) -
						((vf->duration) >> 4)) *
						100 / 9;
					}
				} else {
					vf->duration =
					vvc1_amstream_dec_info.rate >> 1;
					next_pts = 0;
					next_pts_us64 = 0;
				}
			}

			vf->duration_pulldown = 0;
			vf->type = (reg & BOTTOM_FIELD_FIRST_FLAG) ?
			VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;
#ifdef NV21
			vf->type |= VIDTYPE_VIU_NV21;
#endif
			vf->canvas0Addr = vf->canvas1Addr =
						index2canvas(buffer_index);
			vf->orientation = 0;
			vf->type_original = vf->type;
			set_aspect_ratio(vf, READ_VREG(VC1_PIC_RATIO));

			vfbuf_use[buffer_index]++;

			kfifo_put(&display_q, (const struct vframe_s *)vf);

			vf_notify_receiver(
				PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);

			if (kfifo_get(&newframe_q, &vf) == 0) {
				pr_info
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}
			vf->signal_type = 0;
			vf->index = buffer_index;
			vf->width = vvc1_amstream_dec_info.width;
			vf->height = vvc1_amstream_dec_info.height;
			vf->bufWidth = 1920;
			vf->flag = 0;

			vf->pts = next_pts;
			vf->pts_us64 = next_pts_us64;
			if ((repeat_count > 1) && avi_flag) {
				vf->duration =
					vvc1_amstream_dec_info.rate *
					repeat_count >> 1;
				if (next_pts != 0) {
					next_pts +=
						((vf->duration) -
						 ((vf->duration) >> 4));
				}
				if (next_pts_us64 != 0) {
					next_pts_us64 += ((vf->duration) -
					((vf->duration) >> 4)) * 100 / 9;
				}
			} else {
				vf->duration =
					vvc1_amstream_dec_info.rate >> 1;
				next_pts = 0;
				next_pts_us64 = 0;
			}

			vf->duration_pulldown = 0;
			vf->type = (reg & BOTTOM_FIELD_FIRST_FLAG) ?
			VIDTYPE_INTERLACE_TOP : VIDTYPE_INTERLACE_BOTTOM;
#ifdef NV21
			vf->type |= VIDTYPE_VIU_NV21;
#endif
			vf->canvas0Addr = vf->canvas1Addr =
					index2canvas(buffer_index);
			vf->orientation = 0;
			vf->type_original = vf->type;
			set_aspect_ratio(vf, READ_VREG(VC1_PIC_RATIO));

			vfbuf_use[buffer_index]++;

			kfifo_put(&display_q, (const struct vframe_s *)vf);

			vf_notify_receiver(
					PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
		} else {	/* progressive */
			if (kfifo_get(&newframe_q, &vf) == 0) {
				pr_info
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}
			vf->signal_type = 0;
			vf->index = buffer_index;
			vf->width = vvc1_amstream_dec_info.width;
			vf->height = vvc1_amstream_dec_info.height;
			vf->bufWidth = 1920;
			vf->flag = 0;

			if (pts_valid) {
				vf->pts = pts;
				vf->pts_us64 = pts_us64;
				if ((repeat_count > 1) && avi_flag) {
					vf->duration =
						vvc1_amstream_dec_info.rate *
						repeat_count;
					next_pts =
						pts +
						(vvc1_amstream_dec_info.rate *
						 repeat_count) * 15 / 16;
					next_pts_us64 = pts_us64 +
						((vvc1_amstream_dec_info.rate *
						repeat_count) * 15 / 16) *
						100 / 9;
				} else {
					vf->duration =
						vvc1_amstream_dec_info.rate;
					next_pts = 0;
					next_pts_us64 = 0;
				}
			} else {
				vf->pts = next_pts;
				vf->pts_us64 = next_pts_us64;
				if ((repeat_count > 1) && avi_flag) {
					vf->duration =
						vvc1_amstream_dec_info.rate *
						repeat_count;
					if (next_pts != 0) {
						next_pts += ((vf->duration) -
							((vf->duration) >> 4));
					}
					if (next_pts_us64 != 0) {
						next_pts_us64 +=
						((vf->duration) -
						((vf->duration) >> 4)) *
						100 / 9;
					}
				} else {
					vf->duration =
						vvc1_amstream_dec_info.rate;
					next_pts = 0;
					next_pts_us64 = 0;
				}
			}

			vf->duration_pulldown = 0;
#ifdef NV21
			vf->type =
				VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
#else
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
#endif
			vf->canvas0Addr = vf->canvas1Addr =
						index2canvas(buffer_index);
			vf->orientation = 0;
			vf->type_original = vf->type;
			set_aspect_ratio(vf, READ_VREG(VC1_PIC_RATIO));

			vfbuf_use[buffer_index]++;

			kfifo_put(&display_q, (const struct vframe_s *)vf);

			vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
		}
		frame_dur = vvc1_amstream_dec_info.rate;
		total_frame++;

		/*count info*/
		gvs->frame_dur = frame_dur;
		vdec_count_info(gvs, 0, offset);

		/* pr_info("PicType = %d, PTS = 0x%x, repeat
		count %d\n", picture_type, vf->pts, repeat_count); */
		WRITE_VREG(VC1_BUFFEROUT, 0);
	}

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	return IRQ_HANDLED;
}

static struct vframe_s *vvc1_vf_peek(void *op_arg)
{
	struct vframe_s *vf;

	if (kfifo_peek(&display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vvc1_vf_get(void *op_arg)
{
	struct vframe_s *vf;

	if (kfifo_get(&display_q, &vf))
		return vf;

	return NULL;
}

static void vvc1_vf_put(struct vframe_s *vf, void *op_arg)
{
	if (pool_index(vf) == cur_pool_idx)
		kfifo_put(&recycle_q, (const struct vframe_s *)vf);
}

static int vvc1_vf_states(struct vframe_states *states, void *op_arg)
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

static int vvc1_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_RESET) {
		unsigned long flags;
		amvdec_stop();
#ifndef CONFIG_POST_PROCESS_MANAGER
		vf_light_unreg_provider(&vvc1_vf_prov);
#endif
		spin_lock_irqsave(&lock, flags);
		vvc1_local_init();
		vvc1_prot_init();
		spin_unlock_irqrestore(&lock, flags);
#ifndef CONFIG_POST_PROCESS_MANAGER
		vf_reg_provider(&vvc1_vf_prov);
#endif
		amvdec_start();
	}
	return 0;
}

int vvc1_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	vstatus->frame_width = vvc1_amstream_dec_info.width;
	vstatus->frame_height = vvc1_amstream_dec_info.height;
	if (vvc1_amstream_dec_info.rate != 0)
		vstatus->frame_rate = 96000 / vvc1_amstream_dec_info.rate;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = READ_VREG(AV_SCRATCH_C);
	vstatus->status = stat;
	vstatus->bit_rate = gvs->bit_rate;
	vstatus->frame_dur = vvc1_amstream_dec_info.rate;
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

static int vvc1_vdec_info_init(void)
{
	gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);
	if (NULL == gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}
	return 0;
}

/****************************************/
static void vvc1_canvas_init(void)
{
	int i;
	u32 canvas_width, canvas_height;
	u32 decbuf_size, decbuf_y_size, decbuf_uv_size;
	u32 disp_addr = 0xffffffff;

	if (buf_size <= 0x00400000) {
		/* SD only */
		canvas_width = 768;
		canvas_height = 576;
		decbuf_y_size = 0x80000;
		decbuf_uv_size = 0x20000;
		decbuf_size = 0x100000;
	} else {
		/* HD & SD */
		canvas_width = 1920;
		canvas_height = 1088;
		decbuf_y_size = 0x200000;
		decbuf_uv_size = 0x80000;
		decbuf_size = 0x300000;
	}

	if (is_vpp_postblend()) {
		struct canvas_s cur_canvas;

		canvas_read((READ_VCBUS_REG(VD1_IF0_CANVAS0) & 0xff),
					&cur_canvas);
		disp_addr = (cur_canvas.addr + 7) >> 3;
	}

	for (i = 0; i < 4; i++) {
		if (((buf_start + i * decbuf_size + 7) >> 3) == disp_addr) {
#ifdef NV21
			canvas_config(2 * i + 0,
				buf_start + 4 * decbuf_size,
				canvas_width, canvas_height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
			canvas_config(2 * i + 1,
				buf_start + 4 * decbuf_size +
				decbuf_y_size, canvas_width,
				canvas_height / 2, CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_32X32);
#else
			canvas_config(3 * i + 0,
				buf_start + 4 * decbuf_size,
				canvas_width, canvas_height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
			canvas_config(3 * i + 1,
				buf_start + 4 * decbuf_size +
				decbuf_y_size, canvas_width / 2,
				canvas_height / 2, CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_32X32);
			canvas_config(3 * i + 2,
				buf_start + 4 * decbuf_size +
				decbuf_y_size + decbuf_uv_size,
				canvas_width / 2, canvas_height / 2,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
#endif
		} else {
#ifdef NV21
			canvas_config(2 * i + 0,
				buf_start + i * decbuf_size,
				canvas_width, canvas_height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
			canvas_config(2 * i + 1,
				buf_start + i * decbuf_size +
				decbuf_y_size, canvas_width,
				canvas_height / 2, CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_32X32);
#else
			canvas_config(3 * i + 0,
				buf_start + i * decbuf_size,
				canvas_width, canvas_height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
			canvas_config(3 * i + 1,
				buf_start + i * decbuf_size +
				decbuf_y_size, canvas_width / 2,
				canvas_height / 2, CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_32X32);
			canvas_config(3 * i + 2,
				buf_start + i * decbuf_size +
				decbuf_y_size + decbuf_uv_size,
				canvas_width / 2, canvas_height / 2,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32);
#endif
		}
	}
}

static void vvc1_prot_init(void)
{
#if 1	/* /MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) | (1 << 4));
	WRITE_VREG(DOS_SW_RESET0, 0);

	READ_VREG(DOS_SW_RESET0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) | (1 << 4));
	WRITE_VREG(DOS_SW_RESET0, 0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 9) | (1 << 8));
	WRITE_VREG(DOS_SW_RESET0, 0);

#else
	WRITE_MPEG_REG(RESET0_REGISTER,
				   RESET_IQIDCT | RESET_MC | RESET_VLD_PART);
	READ_MPEG_REG(RESET0_REGISTER);
	WRITE_MPEG_REG(RESET0_REGISTER,
				   RESET_IQIDCT | RESET_MC | RESET_VLD_PART);

	WRITE_MPEG_REG(RESET2_REGISTER, RESET_PIC_DC | RESET_DBLK);
#endif

	WRITE_VREG(POWER_CTL_VLD, 0x10);
	WRITE_VREG_BITS(VLD_MEM_VIFIFO_CONTROL, 2, MEM_FIFO_CNT_BIT, 2);
	WRITE_VREG_BITS(VLD_MEM_VIFIFO_CONTROL, 8, MEM_LEVEL_CNT_BIT, 6);

	vvc1_canvas_init();

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

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);

	WRITE_VREG(VC1_SOS_COUNT, 0);
	WRITE_VREG(VC1_BUFFERIN, 0);
	WRITE_VREG(VC1_BUFFEROUT, 0);

	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);

#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
#endif
}

static void vvc1_local_init(void)
{
	int i;

	/* vvc1_ratio = vvc1_amstream_dec_info.ratio; */
	vvc1_ratio = 0x100;

	avi_flag = (unsigned long) vvc1_amstream_dec_info.param;
	keyframe_pts_only = (u32)vvc1_amstream_dec_info.param & 0x100;
	total_frame = 0;

	next_pts = 0;
	next_pts_us64 = 0;
	next_IP_pts = 0;
	next_IP_pts_us64 = 0;

	saved_resolution = 0;
	frame_width = frame_height = frame_dur = 0;
#ifdef DEBUG_PTS
	pts_hit = pts_missed = pts_i_hit = pts_i_missed = 0;
#endif

	memset(&frm, 0, sizeof(frm));

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
		vfbuf_use[i] = 0;

	INIT_KFIFO(display_q);
	INIT_KFIFO(recycle_q);
	INIT_KFIFO(newframe_q);
	cur_pool_idx ^= 1;
	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf;
		if (cur_pool_idx == 0) {
			vf = &vfpool[i];
			vfpool[i].index = DECODE_BUFFER_NUM_MAX;
			} else {
			vf = &vfpool2[i];
			vfpool2[i].index = DECODE_BUFFER_NUM_MAX;
			}
		kfifo_put(&newframe_q, (const struct vframe_s *)vf);
	}
}

#ifdef CONFIG_POST_PROCESS_MANAGER
static void vvc1_ppmgr_reset(void)
{
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_RESET, NULL);

	vvc1_local_init();

	/* vf_notify_receiver(PROVIDER_NAME,
	* VFRAME_EVENT_PROVIDER_START,NULL); */

	pr_info("vvc1dec: vf_ppmgr_reset\n");
}
#endif

static void vvc1_put_timer_func(unsigned long arg)
{
	struct timer_list *timer = (struct timer_list *)arg;

#if 1
	if (READ_VREG(VC1_SOS_COUNT) > 10) {
		amvdec_stop();
#ifdef CONFIG_POST_PROCESS_MANAGER
		vvc1_ppmgr_reset();
#else
		vf_light_unreg_provider(&vvc1_vf_prov);
		vvc1_local_init();
		vf_reg_provider(&vvc1_vf_prov);
#endif
		vvc1_prot_init();
		amvdec_start();
	}
#endif

	while (!kfifo_is_empty(&recycle_q) && (READ_VREG(VC1_BUFFERIN) == 0)) {
		struct vframe_s *vf;
		if (kfifo_get(&recycle_q, &vf)) {
			if ((vf->index < DECODE_BUFFER_NUM_MAX) &&
				(--vfbuf_use[vf->index] == 0)) {
				WRITE_VREG(VC1_BUFFERIN, ~(1 << vf->index));
				vf->index = DECODE_BUFFER_NUM_MAX;
			}
		if (pool_index(vf) == cur_pool_idx)
			kfifo_put(&newframe_q, (const struct vframe_s *)vf);
		}
	}
	if (frame_dur > 0 && saved_resolution !=
		frame_width * frame_height * (96000 / frame_dur)) {
		int fps = 96000 / frame_dur;
		saved_resolution = frame_width * frame_height * fps;
		vdec_source_changed(VFORMAT_VC1,
			frame_width, frame_height, fps);
	}
	timer->expires = jiffies + PUT_INTERVAL;

	add_timer(timer);
}

static s32 vvc1_init(void)
{
	pr_info("vvc1_init, format %d\n", vvc1_amstream_dec_info.format);
	init_timer(&recycle_timer);

	stat |= STAT_TIMER_INIT;

	intra_output = 0;
	amvdec_enable();

	vvc1_local_init();

	if (vvc1_amstream_dec_info.format == VIDEO_DEC_FORMAT_WMV3) {
		pr_info("WMV3 dec format\n");
		vvc1_format = VIDEO_DEC_FORMAT_WMV3;
		WRITE_VREG(AV_SCRATCH_4, 0);
	} else if (vvc1_amstream_dec_info.format == VIDEO_DEC_FORMAT_WVC1) {
		pr_info("WVC1 dec format\n");
		vvc1_format = VIDEO_DEC_FORMAT_WVC1;
		WRITE_VREG(AV_SCRATCH_4, 1);
	} else
		pr_info("not supported VC1 format\n");

	if (amvdec_loadmc_ex(VFORMAT_VC1, "vc1_mc", NULL) < 0) {
		amvdec_disable();

		pr_info("failed\n");
		return -EBUSY;
	}

	stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	vvc1_prot_init();

	if (vdec_request_irq(VDEC_IRQ_1, vvc1_isr,
		    "vvc1-irq", (void *)vvc1_dec_id)) {
		amvdec_disable();

		pr_info("vvc1 irq register error.\n");
		return -ENOENT;
	}

	stat |= STAT_ISR_REG;
#ifdef CONFIG_POST_PROCESS_MANAGER
	vf_provider_init(&vvc1_vf_prov,
		PROVIDER_NAME, &vvc1_vf_provider, NULL);
	vf_reg_provider(&vvc1_vf_prov);
	vf_notify_receiver(PROVIDER_NAME,
		VFRAME_EVENT_PROVIDER_START, NULL);
#else
	vf_provider_init(&vvc1_vf_prov,
		PROVIDER_NAME, &vvc1_vf_provider, NULL);
	vf_reg_provider(&vvc1_vf_prov);
#endif

	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_FR_HINT,
			(void *)((unsigned long)vvc1_amstream_dec_info.rate));

	stat |= STAT_VF_HOOK;

	recycle_timer.data = (ulong)&recycle_timer;
	recycle_timer.function = vvc1_put_timer_func;
	recycle_timer.expires = jiffies + PUT_INTERVAL;

	add_timer(&recycle_timer);

	stat |= STAT_TIMER_ARM;

	amvdec_start();

	stat |= STAT_VDEC_RUN;

	return 0;
}

static int amvdec_vc1_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;

	if (pdata == NULL) {
		pr_info("amvdec_vc1 memory resource undefined.\n");
		return -EFAULT;
	}

	buf_start = pdata->mem_start;
	buf_size = pdata->mem_end - pdata->mem_start + 1;
	buf_offset = buf_start - ORI_BUFFER_START_ADDR;

	if (pdata->sys_info)
		vvc1_amstream_dec_info = *pdata->sys_info;

	pdata->dec_status = vvc1_dec_status;

	vvc1_vdec_info_init();

	if (vvc1_init() < 0) {
		pr_info("amvdec_vc1 init failed.\n");
		kfree(gvs);
		gvs = NULL;
		return -ENODEV;
	}

	return 0;
}

static int amvdec_vc1_remove(struct platform_device *pdev)
{
	if (stat & STAT_VDEC_RUN) {
		amvdec_stop();
		stat &= ~STAT_VDEC_RUN;
	}

	if (stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_1, (void *)vvc1_dec_id);
		stat &= ~STAT_ISR_REG;
	}

	if (stat & STAT_TIMER_ARM) {
		del_timer_sync(&recycle_timer);
		stat &= ~STAT_TIMER_ARM;
	}

	if (stat & STAT_VF_HOOK) {
		vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_FR_END_HINT, NULL);

		vf_unreg_provider(&vvc1_vf_prov);
		stat &= ~STAT_VF_HOOK;
	}

	amvdec_disable();

#ifdef DEBUG_PTS
	pr_info("pts hit %d, pts missed %d, i hit %d, missed %d\n", pts_hit,
		pts_missed, pts_i_hit, pts_i_missed);
	pr_info("total frame %d, avi_flag %d, rate %d\n",
		total_frame, avi_flag,
		vvc1_amstream_dec_info.rate);
#endif
	kfree(gvs);
	gvs = NULL;

	return 0;
}

/****************************************/

static struct platform_driver amvdec_vc1_driver = {
	.probe = amvdec_vc1_probe,
	.remove = amvdec_vc1_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

#if defined(CONFIG_ARCH_MESON)	/*meson1 only support progressive */
static struct codec_profile_t amvdec_vc1_profile = {
	.name = "vc1",
	.profile = "progressive, wmv3"
};
#else
static struct codec_profile_t amvdec_vc1_profile = {
	.name = "vc1",
	.profile = "progressive, interlace, wmv3"
};
#endif

static int __init amvdec_vc1_driver_init_module(void)
{
	pr_debug("amvdec_vc1 module init\n");

	if (platform_driver_register(&amvdec_vc1_driver)) {
		pr_err("failed to register amvdec_vc1 driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&amvdec_vc1_profile);
	return 0;
}

static void __exit amvdec_vc1_driver_remove_module(void)
{
	pr_debug("amvdec_vc1 module remove.\n");

	platform_driver_unregister(&amvdec_vc1_driver);
}

/****************************************/

module_param(stat, uint, 0664);
MODULE_PARM_DESC(stat, "\n amvdec_vc1 stat\n");

module_init(amvdec_vc1_driver_init_module);
module_exit(amvdec_vc1_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC VC1 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qi Wang <qi.wang@amlogic.com>");
