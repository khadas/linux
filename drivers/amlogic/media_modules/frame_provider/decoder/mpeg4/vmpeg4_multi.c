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
#include <linux/module.h>
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
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/registers/register.h>
#include "../../../stream_input/amports/amports_priv.h"

#include "../utils/amvdec.h"
#include "../utils/vdec_input.h"
#include "../utils/vdec.h"
#include "../utils/firmware.h"
#include "../utils/decoder_mmu_box.h"
#include "../utils/decoder_bmmu_box.h"
#include <linux/amlogic/media/codec_mm/configs.h>
#include "../utils/firmware.h"

#define DRIVER_NAME "ammvdec_mpeg4"
#define MODULE_NAME "ammvdec_mpeg4"

#define MEM_NAME "codec_mmpeg4"

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
#define MAX_BMMU_BUFFER_NUM (DECODE_BUFFER_NUM_MAX + 1)
#define WORKSPACE_SIZE		(12*SZ_64K)
static u32 buf_size = 32 * 1024 * 1024;

#define CTX_LMEM_SWAP_OFFSET    0
#define CTX_QUANT_MATRIX_OFFSET 0x800
/* dcac buffer must align at 4k boundary */
#define CTX_DCAC_BUF_OFFSET     0x1000
#define CTX_DECBUF_OFFSET       (0x0c0000 + 0x1000)

#define RATE_DETECT_COUNT   5
#define DURATION_UNIT       96000
#define PTS_UNIT            90000
#define CHECK_INTERVAL        (HZ/100)

#define DUR2PTS(x) ((x) - ((x) >> 4))

#define DEC_RESULT_NONE     0
#define DEC_RESULT_DONE     1
#define DEC_RESULT_AGAIN    2
#define DEC_RESULT_ERROR    3
#define DEC_RESULT_FORCE_EXIT 4
#define DEC_RESULT_EOS 5
#define DEC_DECODE_TIMEOUT         0x21
#define DECODE_ID(hw) (hw_to_vdec(hw)->id)
#define DECODE_STOP_POS         AV_SCRATCH_K
static u32 udebug_flag;

static struct vframe_s *vmpeg_vf_peek(void *);
static struct vframe_s *vmpeg_vf_get(void *);
static void vmpeg_vf_put(struct vframe_s *, void *);
static int vmpeg_vf_states(struct vframe_states *states, void *);
static int vmpeg_event_cb(int type, void *data, void *private_data);
static int pre_decode_buf_level = 0x800;
static int debug_enable;
static unsigned int radr;
static unsigned int rval;

#define VMPEG4_DEV_NUM        9
static unsigned int max_decode_instance_num = VMPEG4_DEV_NUM;
static unsigned int max_process_time[VMPEG4_DEV_NUM];
static unsigned int decode_timeout_val = 100;


#undef pr_info
#define pr_info printk
unsigned int mpeg4_debug_mask = 0xff;

#define PRINT_FLAG_ERROR              0x0
#define PRINT_FLAG_RUN_FLOW           0X0001
#define PRINT_FLAG_TIMEINFO           0x0002
#define PRINT_FLAG_UCODE_DETAIL	      0x0004
#define PRINT_FLAG_VLD_DETAIL         0x0008
#define PRINT_FLAG_DEC_DETAIL         0x0010
#define PRINT_FLAG_BUFFER_DETAIL      0x0020
#define PRINT_FLAG_RESTORE            0x0040
#define PRINT_FRAME_NUM               0x0080
#define PRINT_FLAG_FORCE_DONE         0x0100
#define PRINT_FLAG_COUNTER            0X0200
#define PRINT_FRAMEBASE_DATA          0x0400
#define PRINT_FLAG_VDEC_STATUS        0x0800
#define PRINT_FLAG_TIMEOUT_STATUS     0x1000

int mmpeg4_debug_print(int index, int debug_flag, const char *fmt, ...)
{
	if (((debug_enable & debug_flag) &&
		((1 << index) & mpeg4_debug_mask))
		|| (debug_flag == PRINT_FLAG_ERROR)) {
		unsigned char buf[512];
		int len = 0;
		va_list args;
		va_start(args, fmt);
		len = sprintf(buf, "%d: ", index);
		vsnprintf(buf + len, 512-len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

struct vdec_mpeg4_hw_s {
	spinlock_t lock;
	struct platform_device *platform_dev;
	/* struct device *cma_dev; */

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
	struct timer_list check_timer;
	u32 decode_timeout_count;
	u32 start_process_time;
	u32 last_vld_level;
	u8 init_flag;
	u32 eos;
	void *mm_blk_handle;

	struct vframe_chunk_s *chunk;
	u32 stat;
	unsigned long buf_start;
	u32 buf_size;
	/*
	unsigned long cma_alloc_addr;
	int cma_alloc_count;
	*/
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
	u32 decoded_type[DECODE_BUFFER_NUM_MAX];
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
	u32 frame_num;
	u32 put_num;
	u32 sys_mp4_rate;
	u32 run_count;
	u32	not_run_ready;
	u32 buffer_not_ready;
	u32	input_empty;
	u32 peek_num;
	u32 get_num;
	u32 first_i_frame_ready;
	u32 drop_frame_count;
	u32 timeout_flag;

	struct firmware_s *fw;
};
static void vmpeg4_local_init(struct vdec_mpeg4_hw_s *hw);
static int vmpeg4_hw_ctx_restore(struct vdec_mpeg4_hw_s *hw);

#define PROVIDER_NAME   "vdec.mpeg4"

/*
 *int query_video_status(int type, int *value);
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

static void reset_process_time(struct vdec_mpeg4_hw_s *hw);
static int find_buffer(struct vdec_mpeg4_hw_s *hw)
{
	int i;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		if (hw->vfbuf_use[i] == 0)
			return i;
	}

	return DECODE_BUFFER_NUM_MAX;
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
	unsigned int pixel_ratio = READ_VREG(MP4_PIC_RATIO);

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

static irqreturn_t vmpeg4_isr_thread_fn(struct vdec_s *vdec, int irq)
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
	if (hw->eos)
		return IRQ_HANDLED;

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);
	if (READ_VREG(AV_SCRATCH_M) != 0 &&
		(debug_enable & PRINT_FLAG_UCODE_DETAIL)) {
		mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_UCODE_DETAIL,
		"dbg %x: %x, level %x, wp %x, rp %x, cnt %x\n",
			READ_VREG(AV_SCRATCH_M), READ_VREG(AV_SCRATCH_N),
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			READ_VREG(VIFF_BIT_CNT));
		WRITE_VREG(AV_SCRATCH_M, 0);
		return IRQ_HANDLED;
	}
	reg = READ_VREG(MREG_BUFFEROUT);

	time_increment_resolution = READ_VREG(MP4_RATE);
	fixed_vop_rate = time_increment_resolution >> 16;
	time_increment_resolution &= 0xffff;
	if (time_increment_resolution > 0 && fixed_vop_rate == 0)
		hw->sys_mp4_rate = time_increment_resolution;
	if (hw->vmpeg4_amstream_dec_info.rate == 0) {
		if ((fixed_vop_rate != 0) && (time_increment_resolution != 0)) {
			/* fixed VOP rate */
			hw->vmpeg4_amstream_dec_info.rate = fixed_vop_rate *
					DURATION_UNIT / time_increment_resolution;
		} else if (time_increment_resolution == 0
		&& hw->sys_mp4_rate > 0)
			time_increment_resolution = hw->sys_mp4_rate;
	}
	mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
	"resolution=%d,fvop=%d,rate=%d\n",
	time_increment_resolution, fixed_vop_rate,
	hw->vmpeg4_amstream_dec_info.rate);

	if (reg == 2) {
		/* timeout when decoding next frame */

		/* for frame based case, insufficient result may happen
		 * at the beginning when only VOL head is available save
		 * HW context also, such as for the QTable from VCOP register
		 */
		mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_VLD_DETAIL,
		"ammvdec_mpeg4: timeout\n");
		mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_VLD_DETAIL,
		"level=%x, vtl=%x,bcnt=%d\n",
		READ_VREG(VLD_MEM_VIFIFO_LEVEL),
		READ_VREG(VLD_MEM_VIFIFO_CONTROL),
		READ_VREG(VIFF_BIT_CNT));
		if (input_frame_based(vdec)) {
			vmpeg4_save_hw_context(hw);
			hw->timeout_flag++;
		} else {
		hw->dec_result = DEC_RESULT_AGAIN;

		schedule_work(&hw->work);
		}
		return IRQ_HANDLED;
	} else {
		reset_process_time(hw);
		picture_type = (reg >> 3) & 7;
		repeat_cnt = READ_VREG(MP4_NOT_CODED_CNT);
		vop_time_inc = READ_VREG(MP4_VOP_TIME_INC);

		index = spec_to_index(hw, READ_VREG(REC_CANVAS_ADDR));

		if (index < 0) {
			mmpeg4_debug_print(DECODE_ID(hw), 0,
			"invalid buffer index.");
			hw->dec_result = DEC_RESULT_ERROR;

			schedule_work(&hw->work);

			return IRQ_HANDLED;
		}
		mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
		"index=%d, used=%d cnt=%d, vopinc=%d\n",
		index, hw->vfbuf_use[index], repeat_cnt, vop_time_inc);

		hw->dec_result = DEC_RESULT_DONE;

		if (hw->vmpeg4_amstream_dec_info.width == 0) {
			hw->vmpeg4_amstream_dec_info.width =
				READ_VREG(MP4_PIC_WH) >> 16;
		}

		if (hw->vmpeg4_amstream_dec_info.height == 0) {
			hw->vmpeg4_amstream_dec_info.height =
				READ_VREG(MP4_PIC_WH) & 0xffff;
		}

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
				if ((hw->rate_detect >= RATE_DETECT_COUNT) &&
					(time_increment_resolution != 0)) {
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
			mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
			"I/P offset 0x%x, pts_valid %d pts=0x%x,index=%d,used=%d\n",
					offset, hw->pts_valid[index],
			hw->pts[index], index, hw->vfbuf_use[index]);
		} else {
			hw->pts_valid[index] = false;
			hw->pts[index] = 0;
			hw->pts64[index] = 0;
		}

		hw->buffer_info[index] = reg;
		hw->vfbuf_use[index] = 0;
		hw->decoded_type[index] = picture_type;
		mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
		"amvdec_mpeg4: decoded buffer %d, frame_type %s,pts=%x\n",
			index,
			(picture_type == I_PICTURE) ? "I" :
		(picture_type == P_PICTURE) ? "P" : "B",
		hw->pts[index]);

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

		if ((hw->first_i_frame_ready == 0) &&
			(I_PICTURE == hw->decoded_type[index]))
			hw->first_i_frame_ready = 1;
		mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
		"queued %d, pts = 0x%x, pts_valid=%d,index=%d,used=%d,type=%d,decode_type=%d\n",
		index, pts, pts_valid, index, hw->vfbuf_use[index],
		picture_type, hw->decoded_type[index]);

		if (pts_valid) {
			hw->last_anch_pts = pts;
			hw->last_anch_pts_us64 = pts_us64;
			hw->frame_num_since_last_anch = 0;
			hw->vop_time_inc_since_last_anch = 0;
		} else if (input_stream_based(vdec)) {
			pts = hw->last_anch_pts;
			pts_us64 = hw->last_anch_pts_us64;

			if ((time_increment_resolution != 0) &&
				(fixed_vop_rate == 0) &&
				(hw->vmpeg4_amstream_dec_info.rate == 0)) {
				/* variable PTS rate */
				/*bug on variable pts calc,
				 *do as dixed vop first if we
				 *have rate setting before.
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
				mmpeg4_debug_print(DECODE_ID(hw), 0,
				"fatal error, no available buffer slot.");
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
			mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
			"pts0=%d,pts64=%lld,w%d,h%d,dur:%d\n",
				vf->pts, vf->pts_us64,
				vf->width, vf->height,
				vf->duration);
			if ((hw->first_i_frame_ready == 0)
				 && (picture_type != I_PICTURE)) {
				hw->drop_frame_count++;
				hw->vfbuf_use[index]--;

				kfifo_put(&hw->newframe_q,
				(const struct vframe_s *)vf);
			} else {
				kfifo_put(&hw->display_q,
				(const struct vframe_s *)vf);
				hw->frame_num++;
			vdec->vdec_fps_detec(vdec->id);
			vf_notify_receiver(vdec->vf_provider_name,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
			}
			if (kfifo_get(&hw->newframe_q, &vf) == 0) {
				mmpeg4_debug_print(DECODE_ID(hw), 0,
				"fatal error, no available buffer slot.");
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
			mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
			"pts1=%d,pts64=%lld,w%d,h%d,dur:%d\n",
				vf->pts, vf->pts_us64,
				vf->width, vf->height,
				vf->duration);
			if ((hw->first_i_frame_ready == 0)
				 && (picture_type != I_PICTURE)) {
				hw->drop_frame_count++;
				hw->vfbuf_use[index]--;

				kfifo_put(&hw->newframe_q,
				(const struct vframe_s *)vf);
			} else {
				kfifo_put(&hw->display_q,
				(const struct vframe_s *)vf);
				hw->frame_num++;
			vdec->vdec_fps_detec(vdec->id);
			vf_notify_receiver(vdec->vf_provider_name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);
			}
		} else {	/* progressive */
			if (kfifo_get(&hw->newframe_q, &vf) == 0) {
				mmpeg4_debug_print(DECODE_ID(hw), 0,
				"fatal error, no available buffer slot.");
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
			mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
			"pts=%d,pts64=%lld,w%d,h%d,index=%d,used=%d,dur:%d, flag=%d, type=%d\n",
				vf->pts, vf->pts_us64,
				vf->width, vf->height,
				index, hw->vfbuf_use[index],
				vf->duration,
				hw->timeout_flag, picture_type);
			if ((hw->first_i_frame_ready == 0)
				 && (picture_type != I_PICTURE)) {
				hw->drop_frame_count++;
				hw->vfbuf_use[index]--;
				hw->timeout_flag++;
				kfifo_put(&hw->newframe_q,
					(const struct vframe_s *)vf);
			} else {
				if (hw->timeout_flag > 2)
					hw->timeout_flag = 2;
				if (hw->timeout_flag && input_frame_based(vdec))
					vf->duration = duration * (hw->timeout_flag + 1);
				kfifo_put(&hw->display_q,
					(const struct vframe_s *)vf);
				hw->frame_num++;
				hw->timeout_flag = 0;
				vdec->vdec_fps_detec(vdec->id);
				vf_notify_receiver(vdec->vf_provider_name,
					VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
			}
		}

		hw->total_frame += repeat_cnt + 1;
		hw->last_vop_time_inc = vop_time_inc;

		schedule_work(&hw->work);
	}
	mmpeg4_debug_print(DECODE_ID(hw), PRINT_FRAME_NUM,
		"%s:frame num:%d\n", __func__, hw->frame_num);

	return IRQ_HANDLED;
}

static irqreturn_t vmpeg4_isr(struct vdec_s *vdec, int irq)
{
	u32 time_increment_resolution, fixed_vop_rate;
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)(vdec->private);

	if (hw->eos)
		return IRQ_HANDLED;
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);
	time_increment_resolution = READ_VREG(MP4_RATE);
	fixed_vop_rate = time_increment_resolution >> 16;
	time_increment_resolution &= 0xffff;
	if (time_increment_resolution > 0 && fixed_vop_rate == 0)
		hw->sys_mp4_rate = time_increment_resolution;
	if (hw->vmpeg4_amstream_dec_info.rate == 0) {
		if ((fixed_vop_rate != 0) && (time_increment_resolution != 0)) {
			hw->vmpeg4_amstream_dec_info.rate = fixed_vop_rate *
					DURATION_UNIT /
					time_increment_resolution;
		} else if (time_increment_resolution == 0
		&& hw->sys_mp4_rate > 0)
			time_increment_resolution = hw->sys_mp4_rate;
	}
	mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
	"resolution=%d,fvop=%d,rate=%d\n",
	time_increment_resolution, fixed_vop_rate,
	hw->vmpeg4_amstream_dec_info.rate);
	return IRQ_WAKE_THREAD;
}
static void vmpeg4_work(struct work_struct *work)
{
	struct vdec_mpeg4_hw_s *hw =
		container_of(work, struct vdec_mpeg4_hw_s, work);
	struct vdec_s *vdec = hw_to_vdec(hw);

	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */
	if (hw->dec_result != DEC_RESULT_DONE)
			mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
		"mmpeg4: vmpeg_work,result=%d,status=%d\n",
		hw->dec_result, hw_to_vdec(hw)->next_status);

	if ((hw->dec_result == DEC_RESULT_DONE) ||
		((hw->chunk) &&
		(input_frame_based(&(hw_to_vdec(hw))->input)))) {
		if (!hw->ctx_valid)
			hw->ctx_valid = 1;

		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
	} else if (hw->dec_result == DEC_RESULT_AGAIN
	&& (hw_to_vdec(hw)->next_status !=
		VDEC_STATUS_DISCONNECTED)) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		if (!vdec_has_more_input(hw_to_vdec(hw))) {
			hw->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hw->work);
			/*pr_info("%s: return\n",
			__func__);*/
			return;
		}
	} else if (hw->dec_result == DEC_RESULT_FORCE_EXIT) {
		mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"%s: force exit\n", __func__);
		if (hw->stat & STAT_ISR_REG) {
			amvdec_stop();
			/*disable mbox interrupt */
			WRITE_VREG(ASSIST_MBOX1_MASK, 0);
			vdec_free_irq(VDEC_IRQ_1, (void *)hw);
			hw->stat &= ~STAT_ISR_REG;
		}
	} else if (hw->dec_result == DEC_RESULT_EOS) {
		/*pr_info("%s: end of stream\n",
			__func__);*/
		hw->eos = 1;
		if (hw->stat & STAT_VDEC_RUN) {
			amvdec_stop();
			hw->stat &= ~STAT_VDEC_RUN;
		}
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
		vdec_clean_input(hw_to_vdec(hw));
	}
	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}
	del_timer_sync(&hw->check_timer);
	hw->stat &= ~STAT_TIMER_ARM;

	/* mark itself has all HW resource released and input released */
	if (vdec->parallel_dec == 1)
		vdec_core_finish_run(hw_to_vdec(hw), CORE_MASK_VDEC_1);
	else
		vdec_core_finish_run(hw_to_vdec(hw), CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

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
	hw->peek_num++;
	if (kfifo_peek(&hw->display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vmpeg_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;
	hw->get_num++;
	if (kfifo_get(&hw->display_q, &vf))
		return vf;

	return NULL;
}

static void vmpeg_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;

	hw->vfbuf_use[vf->index]--;
	hw->put_num++;
	mmpeg4_debug_print(DECODE_ID(hw), PRINT_FRAME_NUM,
	"%s:put num:%d\n",
	__func__, hw->put_num);
	mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
	"index=%d, used=%d\n",
	vf->index, hw->vfbuf_use[vf->index]);
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

	if (!hw)
		return -1;

	vstatus->frame_width = hw->vmpeg4_amstream_dec_info.width;
	vstatus->frame_height = hw->vmpeg4_amstream_dec_info.height;
	if (0 != hw->vmpeg4_amstream_dec_info.rate)
		vstatus->frame_rate = DURATION_UNIT /
				hw->vmpeg4_amstream_dec_info.rate;
	else
		vstatus->frame_rate = DURATION_UNIT;
	vstatus->error_count = READ_VREG(MP4_ERR_COUNT);
	vstatus->status = hw->stat;

	return 0;
}

/****************************************/
static int vmpeg4_canvas_init(struct vdec_mpeg4_hw_s *hw)
{
	int i, ret;
	u32 canvas_width, canvas_height;
	u32 decbuf_size, decbuf_y_size;
	struct vdec_s *vdec = hw_to_vdec(hw);
	unsigned long decbuf_start;

	if (buf_size <= 0x00400000) {
			/* SD only */
			canvas_width = 768;
			canvas_height = 576;
			decbuf_y_size = 0x80000;
			decbuf_size = 0x100000;
		} else {
			int w = hw->vmpeg4_amstream_dec_info.width;
			int h = hw->vmpeg4_amstream_dec_info.height;
			int align_w, align_h;
			int max, min;
			if (w == 0)
				w = 1920;
			if (h == 0)
				h = 1088;
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
				decbuf_y_size =
				ALIGN(align_w * align_h, SZ_64K);
				decbuf_size =
				ALIGN(align_w * align_h * 3/2, SZ_64K);
			} else { /*1080p*/
				if (h > w) {
					canvas_width = 1088;
					canvas_height = 1920;
				} else {
					canvas_width = 1920;
					canvas_height = 1088;
				}
				decbuf_y_size = 0x200000;
				decbuf_size = 0x300000;
			}
		}

	for (i = 0; i < MAX_BMMU_BUFFER_NUM; i++) {

		unsigned canvas;
		if (i == (MAX_BMMU_BUFFER_NUM - 1))
			decbuf_size = WORKSPACE_SIZE;
		ret = decoder_bmmu_box_alloc_buf_phy(hw->mm_blk_handle, i,
				decbuf_size, DRIVER_NAME, &decbuf_start);
		if (ret < 0) {
			pr_err("mmu alloc failed! size 0x%d  idx %d\n",
				decbuf_size, i);
			return ret;
		}

		if (i == (MAX_BMMU_BUFFER_NUM - 1)) {
			hw->buf_start = decbuf_start;
		} else {
			if (vdec->parallel_dec == 1) {
				unsigned tmp;
				if (canvas_u(hw->canvas_spec[i]) == 0xff) {
					tmp =
						vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
					hw->canvas_spec[i] &= ~(0xffff << 8);
					hw->canvas_spec[i] |= tmp << 8;
					hw->canvas_spec[i] |= tmp << 16;
				}
				if (canvas_y(hw->canvas_spec[i]) == 0xff) {
					tmp =
						vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
					hw->canvas_spec[i] &= ~0xff;
					hw->canvas_spec[i] |= tmp;
				}
				canvas = hw->canvas_spec[i];
			} else {
				canvas = vdec->get_canvas(i, 2);
				hw->canvas_spec[i] = canvas;
			}

			hw->canvas_config[i][0].phy_addr =
			decbuf_start;
			hw->canvas_config[i][0].width =
			canvas_width;
			hw->canvas_config[i][0].height =
			canvas_height;
			hw->canvas_config[i][0].block_mode =
			CANVAS_BLKMODE_32X32;

		canvas_config_config(canvas_y(canvas),
					&hw->canvas_config[i][0]);

			hw->canvas_config[i][1].phy_addr =
			decbuf_start + decbuf_y_size;
			hw->canvas_config[i][1].width =
			canvas_width;
			hw->canvas_config[i][1].height =
			canvas_height / 2;
			hw->canvas_config[i][1].block_mode =
			CANVAS_BLKMODE_32X32;

		canvas_config_config(canvas_u(canvas),
					&hw->canvas_config[i][1]);
		}
	}

	return 0;
}
static void vmpeg4_dump_state(struct vdec_s *vdec)
{
	struct vdec_mpeg4_hw_s *hw =
		(struct vdec_mpeg4_hw_s *)(vdec->private);
	u32 i;
	mmpeg4_debug_print(DECODE_ID(hw), 0,
		"====== %s\n", __func__);
	mmpeg4_debug_print(DECODE_ID(hw), 0,
		"width/height (%d/%d), i_fram:%d, buffer_not_ready %d\n",
		hw->vmpeg4_amstream_dec_info.width,
		hw->vmpeg4_amstream_dec_info.height,
		hw->first_i_frame_ready,
		hw->buffer_not_ready
		);
	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		mmpeg4_debug_print(DECODE_ID(hw), 0,
			"index %d, used %d\n", i, hw->vfbuf_use[i]);
	}

	mmpeg4_debug_print(DECODE_ID(hw), 0,
		"is_framebase(%d), eos %d, state 0x%x, dec_result 0x%x dec_frm %d\n",
		input_frame_based(vdec),
		hw->eos,
		hw->stat,
		hw->dec_result,
		hw->frame_num
		);
	mmpeg4_debug_print(DECODE_ID(hw), 0,
		"is_framebase(%d),  put_frm %d run %d not_run_ready %d input_empty %d,drop %d\n",
		input_frame_based(vdec),
		hw->put_num,
		hw->run_count,
		hw->not_run_ready,
		hw->input_empty,
		hw->drop_frame_count
		);

	if (vf_get_receiver(vdec->vf_provider_name)) {
		enum receviver_start_e state =
		vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_QUREY_STATE,
			NULL);
		mmpeg4_debug_print(DECODE_ID(hw), 0,
			"\nreceiver(%s) state %d\n",
			vdec->vf_provider_name,
			state);
	}
	mmpeg4_debug_print(DECODE_ID(hw), 0,
	"%s, newq(%d/%d), dispq(%d/%d) vf peek/get/put (%d/%d/%d)\n",
	__func__,
	kfifo_len(&hw->newframe_q),
	VF_POOL_SIZE,
	kfifo_len(&hw->display_q),
	VF_POOL_SIZE,
	hw->peek_num,
	hw->get_num,
	hw->put_num
	);
	mmpeg4_debug_print(DECODE_ID(hw), 0,
		"VIFF_BIT_CNT=0x%x\n",
		READ_VREG(VIFF_BIT_CNT));
	mmpeg4_debug_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_LEVEL=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_LEVEL));
	mmpeg4_debug_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_WP=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_WP));
	mmpeg4_debug_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_RP=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_RP));
	mmpeg4_debug_print(DECODE_ID(hw), 0,
		"PARSER_VIDEO_RP=0x%x\n",
		READ_PARSER_REG(PARSER_VIDEO_RP));
	mmpeg4_debug_print(DECODE_ID(hw), 0,
		"PARSER_VIDEO_WP=0x%x\n",
		READ_PARSER_REG(PARSER_VIDEO_WP));
	if (input_frame_based(vdec) &&
		debug_enable & PRINT_FRAMEBASE_DATA
		) {
		int jj;
		if (hw->chunk && hw->chunk->block &&
			hw->chunk->size > 0) {
			u8 *data = NULL;

			if (!hw->chunk->block->is_mapped)
				data = codec_mm_vmap(hw->chunk->block->start +
					hw->chunk->offset, hw->chunk->size);
			else
				data = ((u8 *)hw->chunk->block->start_virt) +
					hw->chunk->offset;

			mmpeg4_debug_print(DECODE_ID(hw), 0,
				"frame data size 0x%x\n",
				hw->chunk->size);
			for (jj = 0; jj < hw->chunk->size; jj++) {
				if ((jj & 0xf) == 0)
					mmpeg4_debug_print(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA,
						"%06x:", jj);
				mmpeg4_debug_print(DECODE_ID(hw),
				PRINT_FRAMEBASE_DATA,
					"%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					mmpeg4_debug_print(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA,
						"\n");
			}

			if (!hw->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		}
	}
}

static void reset_process_time(struct vdec_mpeg4_hw_s *hw)
{
	if (hw->start_process_time) {
		unsigned process_time =
			1000 * (jiffies - hw->start_process_time) / HZ;
		hw->start_process_time = 0;
		if (process_time > max_process_time[DECODE_ID(hw)])
			max_process_time[DECODE_ID(hw)] = process_time;
	}
}
static void start_process_time(struct vdec_mpeg4_hw_s *hw)
{
	hw->decode_timeout_count = 2;
	hw->start_process_time = jiffies;
}

static void timeout_process(struct vdec_mpeg4_hw_s *hw)
{
	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}
	mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEOUT_STATUS,
		"%s decoder timeout\n", __func__);
	reset_process_time(hw);
	hw->first_i_frame_ready = 0;
	hw->dec_result = DEC_RESULT_DONE;
	vdec_schedule_work(&hw->work);
}


static void check_timer_func(unsigned long arg)
{
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)arg;
	struct vdec_s *vdec = hw_to_vdec(hw);
	unsigned int timeout_val = decode_timeout_val;

	if (radr != 0) {
		if (rval != 0) {
			WRITE_VREG(radr, rval);
			pr_info("WRITE_VREG(%x,%x)\n", radr, rval);
		} else
			pr_info("READ_VREG(%x)=%x\n", radr, READ_VREG(radr));
		rval = 0;
		radr = 0;
	}

	if (debug_enable == 0 &&
		(input_frame_based(vdec) ||
		(READ_VREG(VLD_MEM_VIFIFO_LEVEL) > 0x100)) &&
		(timeout_val > 0) &&
		(hw->start_process_time > 0) &&
		((1000 * (jiffies - hw->start_process_time) / HZ)
			> timeout_val)) {
		if (hw->last_vld_level == READ_VREG(VLD_MEM_VIFIFO_LEVEL)) {
			if (hw->decode_timeout_count > 0)
				hw->decode_timeout_count--;
			if (hw->decode_timeout_count == 0)
				timeout_process(hw);
		}
		hw->last_vld_level = READ_VREG(VLD_MEM_VIFIFO_LEVEL);
	}

	if (vdec->next_status == VDEC_STATUS_DISCONNECTED) {
		mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"vdec requested to be disconnected\n");
		hw->dec_result = DEC_RESULT_FORCE_EXIT;
		vdec_schedule_work(&hw->work);
		return;
	}

	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
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

	mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_RESTORE,
	"restore ref0=0x%x, ref1=0x%x, rec=0x%x, ctx_valid=%d,index=%d\n",
	   READ_VREG(MREG_REF0),
	   READ_VREG(MREG_REF1),
	   READ_VREG(REC_CANVAS_ADDR),
	   hw->ctx_valid, index);

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
	hw->sys_mp4_rate = hw->vmpeg4_amstream_dec_info.rate;
	hw->frame_width = hw->frame_height = hw->frame_dur = hw->frame_prog = 0;

	hw->total_frame = 0;

	hw->last_anch_pts = 0;

	hw->last_anch_pts_us64 = 0;

	hw->last_vop_time_inc = hw->last_duration = 0;

	hw->vop_time_inc_since_last_anch = 0;

	hw->frame_num_since_last_anch = 0;
	hw->frame_num = 0;
	hw->put_num = 0;
	hw->run_count = 0;
	hw->not_run_ready = 0;
	hw->input_empty = 0;
	hw->peek_num = 0;
	hw->get_num = 0;

	hw->pts_hit = hw->pts_missed = hw->pts_i_hit = hw->pts_i_missed = 0;
	hw->refs[0] = -1;
	hw->refs[1] = -1;
	hw->first_i_frame_ready = 0;
	hw->drop_frame_count = 0;
	hw->buffer_not_ready = 0;
	hw->timeout_flag = 0;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
		hw->vfbuf_use[i] = 0;

	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hw->vfpool[i];

		hw->vfpool[i].index = DECODE_BUFFER_NUM_MAX;
		kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
	}
	if (hw->mm_blk_handle) {
		decoder_bmmu_box_free(hw->mm_blk_handle);
		hw->mm_blk_handle = NULL;
	}
	hw->mm_blk_handle = decoder_bmmu_box_alloc_box(
			DRIVER_NAME,
			0,
			MAX_BMMU_BUFFER_NUM,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER);
	INIT_WORK(&hw->work, vmpeg4_work);
}

static s32 vmmpeg4_init(struct vdec_mpeg4_hw_s *hw)
{
	int trickmode_fffb = 0;
	int size = -1, fw_size = 0x1000 * 16;
	struct firmware_s *fw = NULL;

	fw = vmalloc(sizeof(struct firmware_s) + fw_size);
	if (IS_ERR_OR_NULL(fw))
		return -ENOMEM;

	if (hw->vmpeg4_amstream_dec_info.format ==
			VIDEO_DEC_FORMAT_MPEG4_5) {
		size = get_firmware_data(VIDEO_DEC_MPEG4_5_MULTI, fw->data);
		strncpy(fw->name, "mmpeg4_mc_5", sizeof(fw->name));
	} else if (hw->vmpeg4_amstream_dec_info.format ==
			VIDEO_DEC_FORMAT_H263) {
		size = get_firmware_data(VIDEO_DEC_H263_MULTI, fw->data);
		strncpy(fw->name, "mh263_mc", sizeof(fw->name));
	}
	pr_info("mmpeg4 get fw %s, size %x\n", fw->name, size);
	if (size < 0) {
		pr_err("get firmware failed.");
		vfree(fw);
		return -1;
	}

	fw->len = size;
	hw->fw = fw;

	query_video_status(0, &trickmode_fffb);

	pr_info("%s\n", __func__);

	amvdec_enable();

	init_timer(&hw->check_timer);
	hw->check_timer.data = (unsigned long)hw;
	hw->check_timer.function = check_timer_func;
	hw->check_timer.expires = jiffies + CHECK_INTERVAL;
	hw->stat |= STAT_TIMER_ARM;
	hw->eos = 0;
	WRITE_VREG(DECODE_STOP_POS, udebug_flag);

	vmpeg4_local_init(hw);
	wmb();

	return 0;
}

static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	int index;
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;
	if (hw->eos)
		return 0;
	if (vdec_stream_based(vdec) && (hw->init_flag == 0)
		&& pre_decode_buf_level != 0) {
		u32 rp, wp, level;

		rp = READ_PARSER_REG(PARSER_VIDEO_RP);
		wp = READ_PARSER_REG(PARSER_VIDEO_WP);
		if (wp < rp)
			level = vdec->input.size + wp - rp;
		else
			level = wp - rp;
		if (level < pre_decode_buf_level) {
			hw->not_run_ready++;
			return 0;
		}
	}

	index = find_buffer(hw);
	if (index >= DECODE_BUFFER_NUM_MAX) {
		hw->buffer_not_ready++;
		return 0;
	}
	hw->not_run_ready = 0;
	hw->buffer_not_ready = 0;
	if (vdec->parallel_dec == 1)
		return (unsigned long)(CORE_MASK_VDEC_1);
	else
		return (unsigned long)(CORE_MASK_VDEC_1 | CORE_MASK_HEVC);
}

static unsigned char get_data_check_sum
	(struct vdec_mpeg4_hw_s *hw, int size)
{
	int jj;
	int sum = 0;
	u8 *data = NULL;

	if (!hw->chunk->block->is_mapped)
		data = codec_mm_vmap(hw->chunk->block->start +
			hw->chunk->offset, size);
	else
		data = ((u8 *)hw->chunk->block->start_virt) +
			hw->chunk->offset;

	for (jj = 0; jj < size; jj++)
		sum += data[jj];

	if (!hw->chunk->block->is_mapped)
		codec_mm_unmap_phyaddr(data);
	return sum;
}

static void run(struct vdec_s *vdec, unsigned long mask,
		void (*callback)(struct vdec_s *, void *), void *arg)
{
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;
	int save_reg = READ_VREG(POWER_CTL_VLD);
	int size, ret = 0;

	hw->run_count++;
	/* reset everything except DOS_TOP[1] and APB_CBUS[0] */
	WRITE_VREG(DOS_SW_RESET0, 0xfffffff0);
	WRITE_VREG(DOS_SW_RESET0, 0);
	WRITE_VREG(POWER_CTL_VLD, save_reg);

	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;
	vdec_reset_core(vdec);
	size = vdec_prepare_input(vdec, &hw->chunk);
	if (size < 0) {
		hw->input_empty++;
		hw->dec_result = DEC_RESULT_AGAIN;
		schedule_work(&hw->work);
		return;
	}

	if (input_frame_based(vdec)) {
		u8 *data = NULL;

		if (!hw->chunk->block->is_mapped)
			data = codec_mm_vmap(hw->chunk->block->start +
				hw->chunk->offset, size);
		else
			data = ((u8 *)hw->chunk->block->start_virt) +
				hw->chunk->offset;

		if (debug_enable & PRINT_FLAG_VDEC_STATUS
			) {
			mmpeg4_debug_print(DECODE_ID(hw), 0,
			"%s: size 0x%x sum 0x%x %02x %02x %02x %02x %02x %02x .. %02x %02x %02x %02x\n",
			__func__, size, get_data_check_sum(hw, size),
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[size - 4],
			data[size - 3],	data[size - 2],
			data[size - 1]);
		}
		if (debug_enable & PRINT_FRAMEBASE_DATA
			) {
			int jj;

			for (jj = 0; jj < size; jj++) {
				if ((jj & 0xf) == 0)
					mmpeg4_debug_print(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA,
						"%06x:", jj);
				mmpeg4_debug_print(DECODE_ID(hw),
				PRINT_FRAMEBASE_DATA,
					"%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					mmpeg4_debug_print(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA,
						"\n");
			}
		}

		if (!hw->chunk->block->is_mapped)
			codec_mm_unmap_phyaddr(data);
	} else
		mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s: %x %x %x %x %x size 0x%x\n",
			__func__,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			READ_PARSER_REG(PARSER_VIDEO_RP),
			READ_PARSER_REG(PARSER_VIDEO_WP),
			size);

	mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
	"%s,%d, size=%d, %x %x %x %x %x\n",
	__func__, __LINE__, size,
	READ_VREG(VLD_MEM_VIFIFO_LEVEL),
	READ_VREG(VLD_MEM_VIFIFO_WP),
	READ_VREG(VLD_MEM_VIFIFO_RP),
	READ_PARSER_REG(PARSER_VIDEO_RP),
	READ_PARSER_REG(PARSER_VIDEO_WP));

	hw->dec_result = DEC_RESULT_NONE;
	if (vdec->mc_loaded) {
	/*firmware have load before,
	  and not changes to another.
	  ignore reload.
	*/
	} else {
		ret = amvdec_vdec_loadmc_buf_ex(VFORMAT_MPEG4,hw->fw->name, vdec,
			hw->fw->data, hw->fw->len);
		if (ret < 0) {
			pr_err("[%d] %s: the %s fw loading failed, err: %x\n", vdec->id,
				hw->fw->name, tee_enabled() ? "TEE" : "local", ret);
			hw->dec_result = DEC_RESULT_FORCE_EXIT;
			schedule_work(&hw->work);
			return;
		}
		vdec->mc_loaded = 1;
		vdec->mc_type = VFORMAT_MPEG4;
	}
	if (vmpeg4_hw_ctx_restore(hw) < 0) {
		hw->dec_result = DEC_RESULT_ERROR;
		mmpeg4_debug_print(DECODE_ID(hw), 0,
			"amvdec_mpeg4: error HW context restore\n");
		schedule_work(&hw->work);
		return;
	}
	hw->input_empty = 0;
	hw->last_vld_level = 0;
	start_process_time(hw);
	vdec_enable_input(vdec);
	/* wmb before ISR is handled */
	wmb();

	amvdec_start();
	hw->stat |= STAT_VDEC_RUN;
	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
}

static int vmpeg4_stop(struct vdec_mpeg4_hw_s *hw)
{
	cancel_work_sync(&hw->work);

	if (hw->mm_blk_handle) {
			decoder_bmmu_box_free(hw->mm_blk_handle);
			hw->mm_blk_handle = NULL;
	}

	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->check_timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}

	if (hw->fw) {
		vfree(hw->fw);
		hw->fw = NULL;
	}
	return 0;
}
static void reset(struct vdec_s *vdec)
{
	struct vdec_mpeg4_hw_s *hw = (struct vdec_mpeg4_hw_s *)vdec->private;

	pr_info("amvdec_mmpeg4: reset.\n");

	vmpeg4_local_init(hw);

	hw->ctx_valid = false;
}

static int ammvdec_mpeg4_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_mpeg4_hw_s *hw = NULL;

	pr_info("%s [%d] probe start.\n", __func__, pdev->id);

	if (pdata == NULL) {
		pr_err("%s memory resource undefined.\n", __func__);
		return -EFAULT;
	}

	hw = vmalloc(sizeof(struct vdec_mpeg4_hw_s));
	if (hw == NULL) {
		pr_err("\namvdec_mpeg4 decoder driver alloc failed\n");
		return -ENOMEM;
	}
	memset(hw, 0, sizeof(struct vdec_mpeg4_hw_s));

	pdata->private = hw;
	pdata->dec_status = dec_status;
	/* pdata->set_trickmode = set_trickmode; */
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vmpeg4_isr;
	pdata->threaded_irq_handler = vmpeg4_isr_thread_fn;
	pdata->dump_state = vmpeg4_dump_state;

	if (pdata->use_vfm_path)
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			PROVIDER_NAME ".%02x", pdev->id & 0xff);

	if (pdata->parallel_dec == 1) {
		int i;
		for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
			hw->canvas_spec[i] = 0xffffff;
	}

	vf_provider_init(&pdata->vframe_provider,
		pdata->vf_provider_name, &vf_provider_ops, pdata);

	platform_set_drvdata(pdev, pdata);
	hw->platform_dev = pdev;

/*
	hw->cma_dev = pdata->cma_dev;
	hw->cma_alloc_count = PAGE_ALIGN(DEFAULT_MEM_SIZE) / PAGE_SIZE;
	hw->cma_alloc_addr = codec_mm_alloc_for_dma(MEM_NAME,
				hw->cma_alloc_count,
				4, CODEC_MM_FLAGS_FOR_VDECODER);

	if (!hw->cma_alloc_addr) {
		pr_err("codec_mm alloc failed, request buf size 0x%lx\n",
			hw->cma_alloc_count * PAGE_SIZE);
		hw->cma_alloc_count = 0;
		if (hw) {
			vfree((void *)hw);
			hw = NULL;
		}
		return -ENOMEM;
	}
	hw->buf_start = hw->cma_alloc_addr;
	hw->buf_size = DEFAULT_MEM_SIZE;
*/
	if (pdata->sys_info)
		hw->vmpeg4_amstream_dec_info = *pdata->sys_info;

	mmpeg4_debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
		"W:%d,H:%d,rate=%d\n",
	hw->vmpeg4_amstream_dec_info.width,
	hw->vmpeg4_amstream_dec_info.height,
	hw->vmpeg4_amstream_dec_info.rate);
	hw->vmpeg4_amstream_dec_info.height = 0;
	hw->vmpeg4_amstream_dec_info.width = 0;

	if (vmmpeg4_init(hw) < 0) {
		pr_err("%s init failed.\n", __func__);
/*
		if (hw->cma_alloc_addr) {
			codec_mm_free_for_dma(MEM_NAME, hw->cma_alloc_addr);
			hw->cma_alloc_count = 0;
		}
*/
		if (hw) {
			vfree((void *)hw);
			hw = NULL;
		}
		pdata->dec_status = NULL;
		return -ENODEV;
	}
	if (pdata->parallel_dec == 1)
		vdec_core_request(pdata, CORE_MASK_VDEC_1);
	else {
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
				| CORE_MASK_COMBINE);
	}

	return 0;
}

static int ammvdec_mpeg4_remove(struct platform_device *pdev)
{
	struct vdec_mpeg4_hw_s *hw =
		(struct vdec_mpeg4_hw_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	struct vdec_s *vdec = hw_to_vdec(hw);
	int i;

	vmpeg4_stop(hw);
	/*
	if (hw->cma_alloc_addr) {
		pr_info("codec_mm release buffer 0x%lx\n", hw->cma_alloc_addr);
		codec_mm_free_for_dma(MEM_NAME, hw->cma_alloc_addr);
		hw->cma_alloc_count = 0;
	}
	*/
	if (vdec->parallel_dec == 1)
		vdec_core_release(hw_to_vdec(hw), CORE_MASK_VDEC_1);
	else
		vdec_core_release(hw_to_vdec(hw), CORE_MASK_VDEC_1 | CORE_MASK_HEVC);
	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_DISCONNECTED);

	if (vdec->parallel_dec == 1) {
		for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
			vdec->free_canvas_ex(canvas_y(hw->canvas_spec[i]), vdec->id);
			vdec->free_canvas_ex(canvas_u(hw->canvas_spec[i]), vdec->id);
		}
	}

	pr_info("pts hit %d, pts missed %d, i hit %d, missed %d\n", hw->pts_hit,
		   hw->pts_missed, hw->pts_i_hit, hw->pts_i_missed);
	pr_info("total frame %d, rate %d\n", hw->total_frame,
		   hw->vmpeg4_amstream_dec_info.rate);
	vfree((void *)hw);
	hw = NULL;

	return 0;
}

/****************************************/

static struct platform_driver ammvdec_mpeg4_driver = {
	.probe = ammvdec_mpeg4_probe,
	.remove = ammvdec_mpeg4_remove,
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

static int __init ammvdec_mpeg4_driver_init_module(void)
{
	pr_info("%s \n", __func__);

	if (platform_driver_register(&ammvdec_mpeg4_driver)) {
		pr_err("failed to register ammvdec_mpeg4 driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&amvdec_mpeg4_profile);
	return 0;
}

static void __exit ammvdec_mpeg4_driver_remove_module(void)
{
	pr_info("ammvdec_mpeg4 module remove.\n");

	platform_driver_unregister(&ammvdec_mpeg4_driver);
}

/****************************************/
module_param(debug_enable, uint, 0664);
MODULE_PARM_DESC(debug_enable,
					 "\n ammvdec_mpeg4 debug enable\n");

module_param(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\nradr\n");

module_param(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\nrval\n");

module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val, "\n ammvdec_mpeg4 decode_timeout_val\n");

module_param_array(max_process_time, uint, &max_decode_instance_num, 0664);

module_param(pre_decode_buf_level, int, 0664);
MODULE_PARM_DESC(pre_decode_buf_level,
		"\n ammvdec_ mpeg4 pre_decode_buf_level\n");

module_param(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n ammvdec_mpeg4 udebug_flag\n");
module_init(ammvdec_mpeg4_driver_init_module);
module_exit(ammvdec_mpeg4_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC MPEG4 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");

