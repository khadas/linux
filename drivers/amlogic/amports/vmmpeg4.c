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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/amports/ptsserv.h>

#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/codec_mm/codec_mm.h>

#include "vdec_reg.h"
#include "vmpeg4.h"
#include "arch/register.h"
#include "amports_priv.h"

#include "amvdec.h"

#define DRIVER_NAME "ammvdec_mpeg4"
#define MODULE_NAME "ammvdec_mpeg4"

#define MEM_NAME "codec_mpeg4"

#define DEBUG_PTS

#define NV21
#define I_PICTURE   0
#define P_PICTURE   1
#define B_PICTURE   2

#define ORI_BUFFER_START_ADDR   0x01000000
#define DEFAULT_MEM_SIZE	(32*SZ_1M)

#define INTERLACE_FLAG          0x80
#define TOP_FIELD_FIRST_FLAG    0x40

/* protocol registers */
#define MREG_REF0           AV_SCRATCH_1
#define MREG_REF1           AV_SCRATCH_2
#define MP4_PIC_RATIO       AV_SCRATCH_5
#define MP4_RATE            AV_SCRATCH_3
#define MP4_ERR_COUNT       AV_SCRATCH_6
#define MP4_PIC_WH          AV_SCRATCH_7
#define MREG_INPUT          AV_SCRATCH_8
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

#define VF_POOL_SIZE          16
#define DECODE_BUFFER_NUM_MAX 4
#define PUT_INTERVAL        (HZ/100)

#define CTX_LMEM_SWAP_OFFSET    0
#define CTX_QUANT_MATRIX_OFFSET 0x800
/* dcac buffer must align at 4k boundary */
#define CTX_DCAC_BUF_OFFSET     0x1000
#define CTX_DECBUF_OFFSET       (0x0c0000 + 0x1000)

#define RATE_DETECT_COUNT   5
#define DURATION_UNIT       96000
#define PTS_UNIT            90000

#define DUR2PTS(x) ((x) - ((x) >> 4))

#define DEC_RESULT_NONE     0
#define DEC_RESULT_DONE     1
#define DEC_RESULT_AGAIN    2
#define DEC_RESULT_ERROR    3

static struct vframe_s *vmpeg_vf_peek(void *);
static struct vframe_s *vmpeg_vf_get(void *);
static void vmpeg_vf_put(struct vframe_s *, void *);
static int vmpeg_vf_states(struct vframe_states *states, void *);
static int vmpeg_event_cb(int type, void *data, void *private_data);

struct vdec_mpeg4_hw_s {
	spinlock_t lock;
	struct platform_device *platform_dev;
	struct device *cma_dev;

	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
	struct vframe_s vfpool[VF_POOL_SIZE];

	s32 vfbuf_use[DECODE_BUFFER_NUM_MAX];
	u32 frame_width;
	u32 frame_height;
	u32 frame_dur;
	u32 frame_prog;

	u32 ctx_valid;
	u32 reg_vcop_ctrl_reg;
	u32 reg_pic_head_info;
	u32 reg_mpeg1_2_reg;
	u32 reg_slice_qp;
	u32 reg_mp4_pic_wh;
	u32 reg_mp4_rate;
	u32 reg_mb_info;
	u32 reg_dc_ac_ctrl;
	u32 reg_iqidct_control;
	u32 reg_resync_marker_length;
	u32 reg_rv_ai_mb_count;

	struct vframe_chunk_s *chunk;
	u32 stat;
	u32 buf_start;
	u32 buf_size;
	unsigned long cma_alloc_addr;
	int cma_alloc_count;
	u32 vmpeg4_ratio;
	u64 vmpeg4_ratio64;
	u32 rate_detect;
	u32 vmpeg4_rotation;
	u32 total_frame;
	u32 last_vop_time_inc;
	u32 last_duration;
	u32 last_anch_pts;
	u32 vop_time_inc_since_last_anch;
	u32 frame_num_since_last_anch;
	u64 last_anch_pts_us64;

	u32 pts_hit;
	u32 pts_missed;
	u32 pts_i_hit;
	u32 pts_i_missed;

	u32 buffer_info[DECODE_BUFFER_NUM_MAX];
	u32 pts[DECODE_BUFFER_NUM_MAX];
	u64 pts64[DECODE_BUFFER_NUM_MAX];
	bool pts_valid[DECODE_BUFFER_NUM_MAX];
	u32 canvas_spec[DECODE_BUFFER_NUM_MAX];
#ifdef NV21
	struct canvas_config_s canvas_config[DECODE_BUFFER_NUM_MAX][2];
#else
	struct canvas_config_s canvas_config[DECODE_BUFFER_NUM_MAX][3];
#endif
	struct dec_sysinfo vmpeg4_amstream_dec_info;

	s32 refs[2];
	int dec_result;
	struct work_struct work;

	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;

};
static void vmpeg4_local_init(struct vdec_mpeg4_hw_s *hw);
static int vmpeg4_hw_ctx_restore(struct vdec_mpeg4_hw_s *hw);

#define PROVIDER_NAME   "vdec.mpeg4"

/*
int query_video_status(int type, int *value);
*/
static const struct vframe_operations_s vf_provider_ops = {
	.peek = vmpeg_vf_peek,
	.get = vmpeg_vf_get,
	.put = vmpeg_vf_put,
	.event_cb = vmpeg_event_cb,
	.vf_states = vmpeg_vf_states,
};

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

static int find_buffer(struct vdec_mpeg4_hw_s *hw)
{
	int i;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		if (hw->vfbuf_use[i] == 0)
			return i;
	}

	return -1;
}

static int spec_to_index(struct vdec_mpeg4_hw_s *hw, u32 spec)
{
	int i;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		if (hw->canvas_spec[i] == spec)
			return i;
	}

	return -1;
}

static void set_frame_info(struct vdec_mpeg4_hw_s *hw, struct vframe_s *vf,
			int buffer_index)
{
	int ar = 0;
	unsigned int num = 0;
	unsigned int den = 0;
	unsigned pixel_ratio = READ_VREG(MP4_PIC_RATIO);

	if (hw->vmpeg4_ratio64 != 0) {
		num = hw->vmpeg4_ratio64>>32;
		den = hw->vmpeg4_ratio64 & 0xffffffff;
	} else {
		num = hw->vmpeg4_ratio>>16;
		den = hw->vmpeg4_ratio & 0xffff;

	}
	if ((num == 0) || (den == 0)) {
		num = 1;
		den = 1;
	}

	if (hw->vmpeg4_ratio == 0) {
		vf->ratio_control |= (0x90 << DISP_RATIO_ASPECT_RATIO_BIT);
		/* always stretch to 16:9 */
	} else if (pixel_ratio > 0x0f) {
		num = (pixel_ratio >> 8) *
			hw->vmpeg4_amstream_dec_info.width * num;
		ar = div_u64((pixel_ratio & 0xff) *
			hw->vmpeg4_amstream_dec_info.height * den * 0x100ULL +
			(num >> 1), num);
	} else {
		switch (aspect_ratio_table[pixel_ratio]) {
		case 0:
			num = hw->vmpeg4_amstream_dec_info.width * num;
			ar = (hw->vmpeg4_amstream_dec_info.height * den *
				0x100 + (num >> 1)) / num;
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

	vf->signal_type = 0;
	vf->type_original = vf->type;
	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);
	vf->canvas0Addr = vf->canvas1Addr = -1;
#ifdef NV21
	vf->plane_num = 2;
#else
	vf->plane_num = 3;
#endif
	vf->canvas0_config[0] = hw->canvas_config[buffer_index][0];
	vf->canvas0_config[1] = hw->canvas_config[buffer_index][1];
#ifndef NV21
	vf->canvas0_config[2] = hw->canvas_config[buffer_index][2];
#endif
	vf->canvas1_config[0] = hw->canvas_config[buffer_index][0];
	vf->canvas1_config[1] = hw->canvas_config[buffer_index][1];
#ifndef NV21
	vf->canvas1_config[2] = hw->canvas_config[buffer_index][2];
#endif
}

static inline void vmpeg4_save_hw_context(struct vdec_mpeg4_hw_s *hw)
{
	hw->reg_mpeg1_2_reg = READ_VREG(MPEG1_2_REG);
	hw->reg_vcop_ctrl_reg = READ_VREG(VCOP_CTRL_REG);
	hw->reg_pic_head_info = READ_VREG(PIC_HEAD_INFO);
	hw->reg_slice_qp = READ_VREG(SLICE_QP);
	hw->reg_mp4_pic_wh = READ_VREG(MP4_PIC_WH);
	hw->reg_mp4_rate = READ_VREG(MP4_RATE);
	hw->reg_mb_info = READ_VREG(MB_INFO);
	hw->reg_dc_ac_ctrl = READ_VREG(DC_AC_CTRL);
	hw->reg_iqidct_control = READ_VREG(IQIDCT_CONTROL);
	hw->reg_resync_marker_length = READ_VREG(RESYNC_MARKER_LENGTH);
	hw->reg_rv_ai_mb_count = READ_VREG(RV_AI_MB_COUNT);
}

static irqreturn_t vmpeg4_isr(struct vdec_s *vdec)
{
	u32 reg;
	struct vframe_s *vf = NULL;
	u32 picture_type;
	int index;
	u32 pts, offset = 0;
	bool pts_valid = false;
	u64 pts_us64 = 0;
	u32 time_increment_resolution, fixed_vop_rate, vop_time_inc;
	u32 repeat_cnt, duration = 3200;
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)(vdec->private);

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	reg = READ_VREG(MREG_BUFFEROUT);

	time_increment_resolution = READ_VREG(MP4_RATE);
	fixed_vop_rate = time_increment_resolution >> 16;
	time_increment_resolution &= 0xffff;

	if (hw->vmpeg4_amstream_dec_info.rate == 0) {
		if ((fixed_vop_rate != 0) && (time_increment_resolution != 0)) {
			/* fixed VOP rate */
			hw->vmpeg4_amstream_dec_info.rate = fixed_vop_rate *
					DURATION_UNIT /
					time_increment_resolution;
		}
	}

	if (reg == 2) {
		/* timeout when decoding next frame */

		/* for frame based case, insufficient result may happen
		 * at the begining when only VOL head is available save
		 * HW context also, such as for the QTable from VCOP register
		 */
		if (input_frame_based(vdec))
			vmpeg4_save_hw_context(hw);

		hw->dec_result = DEC_RESULT_AGAIN;

		schedule_work(&hw->work);

		return IRQ_HANDLED;
	} else {
		picture_type = (reg >> 3) & 7;
		repeat_cnt = READ_VREG(MP4_NOT_CODED_CNT);
		vop_time_inc = READ_VREG(MP4_VOP_TIME_INC);

		index = spec_to_index(hw, READ_VREG(REC_CANVAS_ADDR));

		if (index < 0) {
			pr_err("invalid buffer index.");
			hw->dec_result = DEC_RESULT_ERROR;

			schedule_work(&hw->work);

			return IRQ_HANDLED;
		}

		hw->dec_result = DEC_RESULT_DONE;

		pr_debug("amvdec_mpeg4: offset = 0x%x\n",
			READ_VREG(MP4_OFFSET_REG));

		if (hw->vmpeg4_amstream_dec_info.width == 0) {
			hw->vmpeg4_amstream_dec_info.width =
				READ_VREG(MP4_PIC_WH) >> 16;
		}
#if 0
		else {
			pr_info("info width = %d, ucode width = %d\n",
				   hw->vmpeg4_amstream_dec_info.width,
				   READ_VREG(MP4_PIC_WH) >> 16);
		}
#endif

		if (hw->vmpeg4_amstream_dec_info.height == 0) {
			hw->vmpeg4_amstream_dec_info.height =
				READ_VREG(MP4_PIC_WH) & 0xffff;
		}
#if 0
		else {
			pr_info("info height = %d, ucode height = %d\n",
				   hw->vmpeg4_amstream_dec_info.height,
				   READ_VREG(MP4_PIC_WH) & 0xffff);
		}
#endif
		if (hw->vmpeg4_amstream_dec_info.rate == 0) {
			if (vop_time_inc < hw->last_vop_time_inc) {
				duration = vop_time_inc +
					time_increment_resolution -
					hw->last_vop_time_inc;
			} else {
				duration = vop_time_inc -
					hw->last_vop_time_inc;
			}

			if (duration == hw->last_duration) {
				hw->rate_detect++;
				if (hw->rate_detect >= RATE_DETECT_COUNT) {
					hw->vmpeg4_amstream_dec_info.rate =
					duration * DURATION_UNIT /
					time_increment_resolution;
					duration =
					hw->vmpeg4_amstream_dec_info.rate;
				}
			} else {
				hw->rate_detect = 0;
				hw->last_duration = duration;
			}
		} else {
			duration = hw->vmpeg4_amstream_dec_info.rate;
#if 0
			pr_info("info rate = %d, ucode rate = 0x%x:0x%x\n",
				   hw->vmpeg4_amstream_dec_info.rate,
				   READ_VREG(MP4_RATE), vop_time_inc);
#endif
		}

		if ((I_PICTURE == picture_type) ||
			(P_PICTURE == picture_type)) {
			offset = READ_VREG(MP4_OFFSET_REG);
			if (hw->chunk) {
				hw->pts_valid[index] = hw->chunk->pts_valid;
				hw->pts[index] = hw->chunk->pts;
				hw->pts64[index] = hw->chunk->pts64;
			} else {
				if (pts_lookup_offset_us64
					(PTS_TYPE_VIDEO, offset, &pts, 3000,
					&pts_us64) == 0) {
					hw->pts_valid[index] = true;
					hw->pts[index] = pts;
					hw->pts64[index] = pts_us64;
					hw->pts_hit++;
				} else {
					hw->pts_valid[index] = false;
					hw->pts_missed++;
				}
			}
			pr_debug("I/P offset 0x%x, pts_valid %d pts=0x%x\n",
					offset, hw->pts_valid[index],
					hw->pts[index]);
		} else {
			hw->pts_valid[index] = false;
			hw->pts[index] = 0;
			hw->pts64[index] = 0;
		}

		hw->buffer_info[index] = reg;
		hw->vfbuf_use[index] = 0;

		pr_debug("amvdec_mpeg4: decoded buffer %d, frame_type %s\n",
			index,
			(picture_type == I_PICTURE) ? "I" :
			(picture_type == P_PICTURE) ? "P" : "B");

		/* Buffer management
		 * todo: add sequence-end flush
		 */
		if ((picture_type == I_PICTURE) ||
			(picture_type == P_PICTURE)) {
			hw->vfbuf_use[index]++;

			if (hw->refs[1] == -1) {
				hw->refs[1] = index;
				index = -1;
			} else if (hw->refs[0] == -1) {
				hw->refs[0] = hw->refs[1];
				hw->refs[1] = index;
				index = hw->refs[0];
			} else {
				hw->vfbuf_use[hw->refs[0]]--;
				hw->refs[0] = hw->refs[1];
				hw->refs[1] = index;
				index = hw->refs[0];
			}
		} else {
			/* if this is a B frame, then drop (depending on if
			 * there are two reference frames) or display
			 * immediately
			 */
			if (hw->refs[1] == -1)
				index = -1;
		}

		vmpeg4_save_hw_context(hw);

		if (index < 0) {
			schedule_work(&hw->work);
			return IRQ_HANDLED;
		}

		reg = hw->buffer_info[index];
		pts_valid = hw->pts_valid[index];
		pts = hw->pts[index];
		pts_us64 = hw->pts64[index];

		pr_debug("queued buffer %d, pts = 0x%x, pts_valid=%d\n",
				index, pts, pts_valid);

		if (pts_valid) {
			hw->last_anch_pts = pts;
			hw->last_anch_pts_us64 = pts_us64;
			hw->frame_num_since_last_anch = 0;
			hw->vop_time_inc_since_last_anch = 0;
		} else {
			pts = hw->last_anch_pts;
			pts_us64 = hw->last_anch_pts_us64;

			if ((time_increment_resolution != 0) &&
				(fixed_vop_rate == 0) &&
				(hw->vmpeg4_amstream_dec_info.rate == 0)) {
				/* variable PTS rate */
				/*bug on variable pts calc,
				do as dixed vop first if we
				have rate setting before.
				*/
				if (vop_time_inc > hw->last_vop_time_inc) {
					duration = vop_time_inc -
						hw->last_vop_time_inc;
				} else {
					duration = vop_time_inc +
						time_increment_resolution -
						hw->last_vop_time_inc;
				}

				hw->vop_time_inc_since_last_anch += duration;

				pts += hw->vop_time_inc_since_last_anch *
					PTS_UNIT / time_increment_resolution;
				pts_us64 += (hw->vop_time_inc_since_last_anch *
					PTS_UNIT / time_increment_resolution) *
					100 / 9;

				if (hw->vop_time_inc_since_last_anch >
					(1 << 14)) {
					/* avoid overflow */
					hw->last_anch_pts = pts;
					hw->last_anch_pts_us64 = pts_us64;
					hw->vop_time_inc_since_last_anch = 0;
				}
			} else {
				/* fixed VOP rate */
				hw->frame_num_since_last_anch++;
				pts += DUR2PTS(hw->frame_num_since_last_anch *
					hw->vmpeg4_amstream_dec_info.rate);
				pts_us64 += DUR2PTS(
					hw->frame_num_since_last_anch *
					hw->vmpeg4_amstream_dec_info.rate) *
					100 / 9;

				if (hw->frame_num_since_last_anch > (1 << 15)) {
					/* avoid overflow */
					hw->last_anch_pts = pts;
					hw->last_anch_pts_us64 = pts_us64;
					hw->frame_num_since_last_anch = 0;
				}
			}
		}

		if (reg & INTERLACE_FLAG) {	/* interlace */
			if (kfifo_get(&hw->newframe_q, &vf) == 0) {
				pr_err
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}

			vf->index = index;
			vf->width = hw->vmpeg4_amstream_dec_info.width;
			vf->height = hw->vmpeg4_amstream_dec_info.height;
			vf->bufWidth = 1920;
			vf->flag = 0;
			vf->orientation = hw->vmpeg4_rotation;
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
			set_frame_info(hw, vf, index);

			hw->vfbuf_use[index]++;

			kfifo_put(&hw->display_q, (const struct vframe_s *)vf);

			vf_notify_receiver(vdec->vf_provider_name,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);

			if (kfifo_get(&hw->newframe_q, &vf) == 0) {
				pr_err("fatal error, no available buffer slot.");
				hw->dec_result = DEC_RESULT_ERROR;
				schedule_work(&hw->work);
				return IRQ_HANDLED;
			}

			vf->index = index;
			vf->width = hw->vmpeg4_amstream_dec_info.width;
			vf->height = hw->vmpeg4_amstream_dec_info.height;
			vf->bufWidth = 1920;
			vf->flag = 0;
			vf->orientation = hw->vmpeg4_rotation;

			vf->pts = 0;
			vf->pts_us64 = 0;
			vf->duration = duration >> 1;

			vf->duration_pulldown = 0;
			vf->type = (reg & TOP_FIELD_FIRST_FLAG) ?
			VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;
#ifdef NV21
			vf->type |= VIDTYPE_VIU_NV21;
#endif
			set_frame_info(hw, vf, index);

			hw->vfbuf_use[index]++;

			kfifo_put(&hw->display_q, (const struct vframe_s *)vf);

			vf_notify_receiver(vdec->vf_provider_name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);

		} else {	/* progressive */
			if (kfifo_get(&hw->newframe_q, &vf) == 0) {
				pr_err("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}

			vf->index = index;
			vf->width = hw->vmpeg4_amstream_dec_info.width;
			vf->height = hw->vmpeg4_amstream_dec_info.height;
			vf->bufWidth = 1920;
			vf->flag = 0;
			vf->orientation = hw->vmpeg4_rotation;
			vf->pts = pts;
			vf->pts_us64 = pts_us64;
			vf->duration = duration;
			vf->duration_pulldown = repeat_cnt * duration;
#ifdef NV21
			vf->type =
				VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
#else
			vf->type =
				VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
#endif
			set_frame_info(hw, vf, index);


			hw->vfbuf_use[index]++;

			kfifo_put(&hw->display_q, (const struct vframe_s *)vf);

			vf_notify_receiver(vdec->vf_provider_name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);
		}

		hw->total_frame += repeat_cnt + 1;
		hw->last_vop_time_inc = vop_time_inc;

		schedule_work(&hw->work);
	}

	return IRQ_HANDLED;
}

static void vmpeg4_work(struct work_struct *work)
{
	struct vdec_mpeg4_hw_s *hw =
		container_of(work, struct vdec_mpeg4_hw_s, work);

	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */
	amvdec_stop();

	if ((hw->dec_result == DEC_RESULT_DONE) ||
		((hw->chunk) &&
		(input_frame_based(&(hw_to_vdec(hw))->input)))) {
		if (!hw->ctx_valid)
			hw->ctx_valid = 1;

		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
	}

	/* mark itself has all HW resource released and input released */
	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_CONNECTED);

	if (hw->vdec_cb)
		hw->vdec_cb(hw_to_vdec(hw), hw->vdec_cb_arg);
}

static struct vframe_s *vmpeg_vf_peek(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;

	if (!hw)
		return NULL;

	if (kfifo_peek(&hw->display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vmpeg_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;

	if (kfifo_get(&hw->display_q, &vf))
		return vf;

	return NULL;
}

static void vmpeg_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;

	hw->vfbuf_use[vf->index]--;

	kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
}

static int vmpeg_event_cb(int type, void *data, void *private_data)
{
	return 0;
}

static int  vmpeg_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;

	spin_lock_irqsave(&hw->lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hw->newframe_q);
	states->buf_avail_num = kfifo_len(&hw->display_q);
	states->buf_recycle_num = 0;

	spin_unlock_irqrestore(&hw->lock, flags);

	return 0;
}


static int dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;

	vstatus->width = hw->vmpeg4_amstream_dec_info.width;
	vstatus->height = hw->vmpeg4_amstream_dec_info.height;
	if (0 != hw->vmpeg4_amstream_dec_info.rate)
		vstatus->fps = DURATION_UNIT /
				hw->vmpeg4_amstream_dec_info.rate;
	else
		vstatus->fps = DURATION_UNIT;
	vstatus->error_count = READ_VREG(MP4_ERR_COUNT);
	vstatus->status = hw->stat;

	return 0;
}

/****************************************/
static int vmpeg4_canvas_init(struct vdec_mpeg4_hw_s *hw)
{
	int i;
	u32 decbuf_size, decbuf_y_size;
	struct vdec_s *vdec = hw_to_vdec(hw);
	u32 decbuf_start;

	int w = hw->vmpeg4_amstream_dec_info.width;
	int h = hw->vmpeg4_amstream_dec_info.height;

	if (w == 0)
		w = 1920;
	if (h == 0)
		h = 1088;

	w = ALIGN(w, 64);
	h = ALIGN(h, 64);
	decbuf_y_size = ALIGN(w * h, SZ_64K);
	decbuf_size = ALIGN(w * h * 3/2, SZ_64K);

	decbuf_start = hw->buf_start + CTX_DECBUF_OFFSET;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
#ifdef NV21
		unsigned int canvas = vdec->get_canvas(i, 2);
#else
		unsigned int canvas = vdec->get_canvas(i, 3);
#endif

		hw->canvas_spec[i] = canvas;

#ifdef NV21
		hw->canvas_config[i][0].phy_addr = decbuf_start +
					i * decbuf_size;
		hw->canvas_config[i][0].width = w;
		hw->canvas_config[i][0].height = h;
		hw->canvas_config[i][0].block_mode = CANVAS_BLKMODE_32X32;

		canvas_config_config(canvas_y(canvas),
					&hw->canvas_config[i][0]);

		hw->canvas_config[i][1].phy_addr = decbuf_start +
					i * decbuf_size + decbuf_y_size;
		hw->canvas_config[i][1].width = w;
		hw->canvas_config[i][1].height = h / 2;
		hw->canvas_config[i][1].block_mode = CANVAS_BLKMODE_32X32;

		canvas_config_config(canvas_u(canvas),
					&hw->canvas_config[i][1]);
#else
		hw->canvas_config[i][0].phy_addr = decbuf_start +
					i * decbuf_size;
		hw->canvas_config[i][0].width = w;
		hw->canvas_config[i][0].height = h;
		hw->canvas_config[i][0].block_mode = CANVAS_BLKMODE_32X32;

		canvas_config_config(canvas_y(canvas),
					&hw->canvas_config[i][0]);

		hw->canvas_config[i][1].phy_addr = decbuf_start +
					i * decbuf_size + decbuf_y_size;
		hw->canvas_config[i][1].width = w / 2;
		hw->canvas_config[i][1].height = h / 2;
		hw->canvas_config[i][1].block_mode = CANVAS_BLKMODE_32X32;

		canvas_config_config(canvas_u(canvas),
					&hw->canvas_config[i][1]);

		hw->canvas_config[i][2].phy_addr = decbuf_start +
					i * decbuf_size + decbuf_y_size +
					decbuf_uv_size;
		hw->canvas_config[i][2].width = w / 2;
		hw->canvas_config[i][2].height = h / 2;
		hw->canvas_config[i][2].block_mode = CANVAS_BLKMODE_32X32;

		canvas_config_config(canvas_v(canvas),
					&hw->canvas_config[i][2]);
#endif
	}

	return 0;
}

static int vmpeg4_hw_ctx_restore(struct vdec_mpeg4_hw_s *hw)
{
	int index;


	index = find_buffer(hw);
	if (index < 0)
		return -1;


	if (vmpeg4_canvas_init(hw) < 0)
		return -1;

	/* prepare REF0 & REF1
	 * points to the past two IP buffers
	 * prepare REC_CANVAS_ADDR and ANC2_CANVAS_ADDR
	 * points to the output buffer
	 */
	if (hw->refs[0] == -1) {
		WRITE_VREG(MREG_REF0, (hw->refs[1] == -1) ? 0xffffffff :
					hw->canvas_spec[hw->refs[1]]);
	} else {
		WRITE_VREG(MREG_REF0, (hw->refs[0] == -1) ? 0xffffffff :
					hw->canvas_spec[hw->refs[0]]);
	}
	WRITE_VREG(MREG_REF1, (hw->refs[1] == -1) ? 0xffffffff :
				hw->canvas_spec[hw->refs[1]]);

	WRITE_VREG(MREG_REF0, (hw->refs[0] == -1) ? 0xffffffff :
				hw->canvas_spec[hw->refs[0]]);
	WRITE_VREG(MREG_REF1, (hw->refs[1] == -1) ? 0xffffffff :
				hw->canvas_spec[hw->refs[1]]);
	WRITE_VREG(REC_CANVAS_ADDR, hw->canvas_spec[index]);
	WRITE_VREG(ANC2_CANVAS_ADDR, hw->canvas_spec[index]);

	pr_debug("vmpeg4_hw_ctx_restore ref0=0x%x, ref1=0x%x, rec=0x%x, ctx_valid=%d\n",
	   READ_VREG(MREG_REF0),
	   READ_VREG(MREG_REF1),
	   READ_VREG(REC_CANVAS_ADDR),
	   hw->ctx_valid);

	/* notify ucode the buffer start address */
	WRITE_VREG(MEM_OFFSET_REG, hw->buf_start);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);

	WRITE_VREG(MREG_BUFFEROUT, 0);

	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);

	/* clear repeat count */
	WRITE_VREG(MP4_NOT_CODED_CNT, 0);

#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
#endif

#if 1/* /MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	WRITE_VREG(MDEC_PIC_DC_THRESH, 0x404038aa);
#endif

	WRITE_VREG(MP4_PIC_WH, (hw->ctx_valid) ?
		hw->reg_mp4_pic_wh :
			((hw->vmpeg4_amstream_dec_info.width << 16) |
				hw->vmpeg4_amstream_dec_info.height));
	WRITE_VREG(MP4_SYS_RATE, hw->vmpeg4_amstream_dec_info.rate);

	if (hw->ctx_valid) {
		WRITE_VREG(DC_AC_CTRL, hw->reg_dc_ac_ctrl);
		WRITE_VREG(IQIDCT_CONTROL, hw->reg_iqidct_control);
		WRITE_VREG(RESYNC_MARKER_LENGTH, hw->reg_resync_marker_length);
		WRITE_VREG(RV_AI_MB_COUNT, hw->reg_rv_ai_mb_count);
	}
	WRITE_VREG(MPEG1_2_REG, (hw->ctx_valid) ? hw->reg_mpeg1_2_reg : 1);
	WRITE_VREG(VCOP_CTRL_REG, hw->reg_vcop_ctrl_reg);
	WRITE_VREG(PIC_HEAD_INFO, hw->reg_pic_head_info);
	WRITE_VREG(SLICE_QP, hw->reg_slice_qp);
	WRITE_VREG(MB_INFO, hw->reg_mb_info);

	if (hw->chunk) {
		/* frame based input */
		WRITE_VREG(MREG_INPUT, (hw->chunk->offset & 7) | (1<<7) |
							(hw->ctx_valid<<6));
	} else {
		/* stream based input */
		WRITE_VREG(MREG_INPUT, (hw->ctx_valid<<6));
	}

	return 0;
}

static void vmpeg4_local_init(struct vdec_mpeg4_hw_s *hw)
{
	int i;

	hw->vmpeg4_ratio = hw->vmpeg4_amstream_dec_info.ratio;

	hw->vmpeg4_ratio64 = hw->vmpeg4_amstream_dec_info.ratio64;

	hw->vmpeg4_rotation =
		(((unsigned long) hw->vmpeg4_amstream_dec_info.param)
			>> 16) & 0xffff;

	hw->frame_width = hw->frame_height = hw->frame_dur = hw->frame_prog = 0;

	hw->total_frame = 0;

	hw->last_anch_pts = 0;

	hw->last_anch_pts_us64 = 0;

	hw->last_vop_time_inc = hw->last_duration = 0;

	hw->vop_time_inc_since_last_anch = 0;

	hw->frame_num_since_last_anch = 0;

	hw->pts_hit = hw->pts_missed = hw->pts_i_hit = hw->pts_i_missed = 0;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
		hw->vfbuf_use[i] = 0;

	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hw->vfpool[i];
		hw->vfpool[i].index = DECODE_BUFFER_NUM_MAX;
		kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
	}

	INIT_WORK(&hw->work, vmpeg4_work);
}

static s32 vmpeg4_init(struct vdec_mpeg4_hw_s *hw)
{
	int trickmode_fffb = 0;

	query_video_status(0, &trickmode_fffb);

	pr_info("vmpeg4_init\n");

	amvdec_enable();

	vmpeg4_local_init(hw);

	return 0;
}

static bool run_ready(struct vdec_s *vdec)
{
	int index;
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;

	index = find_buffer(hw);

	return index >= 0;
}

static void run(struct vdec_s *vdec, void (*callback)(struct vdec_s *, void *),
		void *arg)
{
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;
	int save_reg = READ_VREG(POWER_CTL_VLD);
	int r;

	/* reset everything except DOS_TOP[1] and APB_CBUS[0] */
	WRITE_VREG(DOS_SW_RESET0, 0xfffffff0);
	WRITE_VREG(DOS_SW_RESET0, 0);
	WRITE_VREG(POWER_CTL_VLD, save_reg);

	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;

	r = vdec_prepare_input(vdec, &hw->chunk);
	if (r < 0) {
		pr_debug("amvdec_mpeg4: Input not ready\n");
		hw->dec_result = DEC_RESULT_AGAIN;
		schedule_work(&hw->work);
		return;
	}

	vdec_enable_input(vdec);

	if (hw->chunk)
		pr_debug("input chunk offset %d, size %d\n",
			hw->chunk->offset, hw->chunk->size);

	hw->dec_result = DEC_RESULT_NONE;

	if (hw->vmpeg4_amstream_dec_info.format ==
		VIDEO_DEC_FORMAT_MPEG4_3) {
		pr_info("load VIDEO_DEC_FORMAT_MPEG4_3\n");
		if (amvdec_vdec_loadmc_ex(vdec,
					"vmmpeg4_mc_311") < 0) {
			pr_err("VIDEO_DEC_FORMAT_MPEG4_3 ucode loading failed\n");
			hw->dec_result = DEC_RESULT_ERROR;
			schedule_work(&hw->work);
			return;
		}
	} else if (hw->vmpeg4_amstream_dec_info.format ==
			VIDEO_DEC_FORMAT_MPEG4_4) {
		pr_info("load VIDEO_DEC_FORMAT_MPEG4_4\n");
		if (amvdec_vdec_loadmc_ex(vdec,
					"vmmpeg4_mc_4") < 0) {
			hw->dec_result = DEC_RESULT_ERROR;
			schedule_work(&hw->work);
			pr_err("VIDEO_DEC_FORMAT_MPEG4_4 ucode loading failed\n");
			return;
		}
	} else if (hw->vmpeg4_amstream_dec_info.format ==
			VIDEO_DEC_FORMAT_MPEG4_5) {
		/* pr_info("load VIDEO_DEC_FORMAT_MPEG4_5\n"); */
		if (amvdec_vdec_loadmc_ex(vdec,
					"vmmpeg4_mc_5") < 0) {
			hw->dec_result = DEC_RESULT_ERROR;
			schedule_work(&hw->work);
			pr_err("VIDEO_DEC_FORMAT_MPEG4_5 ucode loading failed\n");
			return;
		}
	} else if (hw->vmpeg4_amstream_dec_info.format ==
			VIDEO_DEC_FORMAT_H263) {
		pr_info("load VIDEO_DEC_FORMAT_H263\n");
		if (amvdec_vdec_loadmc_ex(vdec,
					"mh263_mc") < 0) {
			hw->dec_result = DEC_RESULT_ERROR;
			schedule_work(&hw->work);
			pr_err("VIDEO_DEC_FORMAT_H263 ucode loading failed\n");
			return;
		}
	}

	if (vmpeg4_hw_ctx_restore(hw) < 0) {
		hw->dec_result = DEC_RESULT_ERROR;
		pr_err("amvdec_mpeg4: error HW context restore\n");
		schedule_work(&hw->work);
		return;
	}

	/* wmb before ISR is handled */
	wmb();

	amvdec_start();
}

static void reset(struct vdec_s *vdec)
{
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;

	pr_info("amvdec_mpeg4: reset.\n");

	vmpeg4_local_init(hw);

	hw->ctx_valid = false;
}

static int amvdec_mpeg4_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_mpeg4_hw_s *hw = NULL;

	pr_info("amvdec_mpeg4[%d] probe start.\n", pdev->id);

	if (pdata == NULL) {
		pr_err("ammvdec_mpeg4 memory resource undefined.\n");
		return -EFAULT;
	}

	hw = (struct vdec_mpeg4_hw_s *)devm_kzalloc(&pdev->dev,
		sizeof(struct vdec_mpeg4_hw_s), GFP_KERNEL);
	if (hw == NULL) {
		pr_info("\namvdec_mpeg4 decoder driver alloc failed\n");
		return -ENOMEM;
	}

	pdata->private = hw;
	pdata->dec_status = dec_status;
	/* pdata->set_trickmode = set_trickmode; */
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vmpeg4_isr;

	pdata->id = pdev->id;

	if (pdata->use_vfm_path)
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			PROVIDER_NAME ".%02x", pdev->id & 0xff);

	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vf_provider_ops, pdata);

	platform_set_drvdata(pdev, pdata);

	hw->platform_dev = pdev;
	hw->cma_dev = pdata->cma_dev;

	hw->cma_alloc_count = PAGE_ALIGN(DEFAULT_MEM_SIZE) / PAGE_SIZE;
	hw->cma_alloc_addr = codec_mm_alloc_for_dma(MEM_NAME,
				hw->cma_alloc_count,
				4, CODEC_MM_FLAGS_FOR_VDECODER);

	if (!hw->cma_alloc_addr) {
		pr_err("codec_mm alloc failed, request buf size 0x%lx\n",
			hw->cma_alloc_count * PAGE_SIZE);
		hw->cma_alloc_count = 0;
		return -ENOMEM;
	}
	hw->buf_start = hw->cma_alloc_addr;
	hw->buf_size = DEFAULT_MEM_SIZE;

	if (pdata->sys_info)
		hw->vmpeg4_amstream_dec_info = *pdata->sys_info;

	if (vmpeg4_init(hw) < 0) {
		pr_err("amvdec_mpeg4 init failed.\n");

		return -ENODEV;
	}

	return 0;
}

static int amvdec_mpeg4_remove(struct platform_device *pdev)
{
	struct vdec_mpeg4_hw_s *hw =
		(struct vdec_mpeg4_hw_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);


	amvdec_disable();

	if (hw->cma_alloc_addr) {
		pr_info("codec_mm release buffer 0x%lx\n", hw->cma_alloc_addr);
		codec_mm_free_for_dma(MEM_NAME, hw->cma_alloc_addr);
		hw->cma_alloc_count = 0;
	}

	pr_info("pts hit %d, pts missed %d, i hit %d, missed %d\n", hw->pts_hit,
		   hw->pts_missed, hw->pts_i_hit, hw->pts_i_missed);
	pr_info("total frame %d, rate %d\n", hw->total_frame,
		   hw->vmpeg4_amstream_dec_info.rate);

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
	.name = "mmpeg4",
	.profile = ""
};

static int __init amvdec_mmpeg4_driver_init_module(void)
{
	pr_info("amvdec_mmpeg4 module init\n");

	if (platform_driver_register(&amvdec_mpeg4_driver)) {
		pr_err("failed to register amvdec_mpeg4 driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&amvdec_mpeg4_profile);
	return 0;
}

static void __exit amvdec_mmpeg4_driver_remove_module(void)
{
	pr_info("amvdec_mmpeg4 module remove.\n");

	platform_driver_unregister(&amvdec_mpeg4_driver);
}

/****************************************/

module_init(amvdec_mmpeg4_driver_init_module);
module_exit(amvdec_mmpeg4_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC MPEG4 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");

