/*
 * drivers/amlogic/amports/vh264.c
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

#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/amports/ptsserv.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include <linux/amlogic/amports/vformat.h>
#include <linux/amlogic/amports/tsync.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include "amports_priv.h"
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/amlogic/canvas/canvas.h>

#include "vdec_input.h"

#include "vdec.h"
#include "vdec_reg.h"
#include "amvdec.h"
#include "vh264.h"
#include "streambuf.h"
#include <linux/delay.h>

#undef pr_info
#define pr_info printk

#define DEBUG_UCODE
#define USE_CMA
#define MEM_NAME "codec_m264"
#define MULTI_INSTANCE_FRAMEWORK
/* #define ONE_COLOCATE_BUF_PER_DECODE_BUF */
#include "h264_dpb.h"
/* #define SEND_PARAM_WITH_REG */

#define DRIVER_NAME "ammvdec_h264"
#define MODULE_NAME "ammvdec_h264"

#define CHECK_INTERVAL        (HZ/100)

#define RATE_MEASURE_NUM 8
#define RATE_CORRECTION_THRESHOLD 5
#define RATE_2397_FPS  4004   /* 23.97 */
#define RATE_25_FPS  3840   /* 25 */
#define RATE_2997_FPS  3203   /* 29.97 */
#define DUR2PTS(x) ((x)*90/96)
#define PTS2DUR(x) ((x)*96/90)
#define DUR2PTS_REM(x) (x*90 - DUR2PTS(x)*96)
#define FIX_FRAME_RATE_CHECK_IFRAME_NUM 2

#define FIX_FRAME_RATE_OFF                0
#define FIX_FRAME_RATE_ON                 1
#define FIX_FRAME_RATE_SMOOTH_CHECKING    2

#define DEC_CONTROL_FLAG_FORCE_2997_1080P_INTERLACE 0x0001
#define DEC_CONTROL_FLAG_FORCE_2500_576P_INTERLACE  0x0002
#define DEC_CONTROL_FLAG_FORCE_RATE_2397_FPS_FIX_FRAME_RATE  0x0010
#define DEC_CONTROL_FLAG_FORCE_RATE_2997_FPS_FIX_FRAME_RATE  0x0020


#define RATE_MEASURE_NUM 8
#define RATE_CORRECTION_THRESHOLD 5
#define RATE_24_FPS  4004	/* 23.97 */
#define RATE_25_FPS  3840	/* 25 */
#define DUR2PTS(x) ((x)*90/96)
#define PTS2DUR(x) ((x)*96/90)
#define DUR2PTS_REM(x) (x*90 - DUR2PTS(x)*96)
#define FIX_FRAME_RATE_CHECK_IDRFRAME_NUM 2

#define H264_DEV_NUM        5

unsigned int h264_debug_flag; /* 0xa0000000; */
unsigned int h264_debug_mask = 0xff;
	/*
	h264_debug_cmd:
		0x1xx, force decoder id of xx to be disconnected
	*/
unsigned int h264_debug_cmd;
static unsigned int dec_control;
static unsigned int force_rate_streambase;
static unsigned int force_rate_framebase;
static unsigned int fixed_frame_rate_mode;
static unsigned int error_recovery_mode_in;
static unsigned int start_decode_buf_level = 0x8000;

static unsigned int reorder_dpb_size_margin = 6;
static unsigned int reference_buf_margin = 4;

static unsigned int decode_timeout_val = 50;
static unsigned int radr;
static unsigned int rval;
static unsigned int max_decode_instance_num = H264_DEV_NUM;
static unsigned int decode_frame_count[H264_DEV_NUM];
static unsigned int max_process_time[H264_DEV_NUM];
static unsigned int max_get_frame_interval[H264_DEV_NUM];
		/* bit[3:0]:
		0, run ; 1, pause; 3, step
		bit[4]:
		1, schedule run
		*/
static unsigned int step[H264_DEV_NUM];

#define is_in_parsing_state(status) \
		((status == H264_ACTION_SEARCH_HEAD) || \
			((status & 0xf0) == 0x80))

static inline bool close_to(int a, int b, int m)
{
	return (abs(a - b) < m) ? true : false;
}

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#define NV21
/* #endif */

/* 12M for L41 */
#define MAX_DPB_BUFF_SIZE       (12*1024*1024)
#define DEFAULT_MEM_SIZE        (32*1024*1024)
#define AVIL_DPB_BUFF_SIZE      0x01ec2000

#define DEF_BUF_START_ADDR            0x1000000
#define V_BUF_ADDR_OFFSET             (0x13e000)

#define PIC_SINGLE_FRAME        0
#define PIC_TOP_BOT_TOP         1
#define PIC_BOT_TOP_BOT         2
#define PIC_DOUBLE_FRAME        3
#define PIC_TRIPLE_FRAME        4
#define PIC_TOP_BOT              5
#define PIC_BOT_TOP              6
#define PIC_INVALID              7

#define EXTEND_SAR                      0xff

#define VF_POOL_SIZE        64
#define MAX_VF_BUF_NUM          28
#define PUT_INTERVAL        (HZ/100)
#define NO_DISP_WD_COUNT    (3 * HZ / PUT_INTERVAL)

#define SWITCHING_STATE_OFF       0
#define SWITCHING_STATE_ON_CMD3   1
#define SWITCHING_STATE_ON_CMD1   2

#define DEC_CONTROL_FLAG_FORCE_2997_1080P_INTERLACE 0x0001
#define DEC_CONTROL_FLAG_FORCE_2500_576P_INTERLACE  0x0002

#define INCPTR(p) ptr_atomic_wrap_inc(&p)

#define SLICE_TYPE_I 2
#define SLICE_TYPE_P 5
#define SLICE_TYPE_B 6

struct buffer_spec_s {
	/*
	used:
	0, free; 1, used by dpb; 2,
	used for display;
	3 isolated (do not use for dpb when vf_put)
	*/
	unsigned int used;
	unsigned int info0;
	unsigned int info1;
	unsigned int info2;
	unsigned int y_addr;
	unsigned int u_addr;
	unsigned int v_addr;

	int y_canvas_index;
	int u_canvas_index;
	int v_canvas_index;

#ifdef NV21
	struct canvas_config_s canvas_config[2];
#else
	struct canvas_config_s canvas_config[3];
#endif
#ifdef USE_CMA
	/* struct page *cma_alloc_pages; */
	unsigned long cma_alloc_addr;
	int cma_alloc_count;
#endif
	unsigned int buf_adr;
};

#define spec2canvas(x)  \
	(((x)->v_canvas_index << 16) | \
	 ((x)->u_canvas_index << 8)  | \
	 ((x)->y_canvas_index << 0))

static struct vframe_s *vh264_vf_peek(void *);
static struct vframe_s *vh264_vf_get(void *);
static void vh264_vf_put(struct vframe_s *, void *);
static int vh264_vf_states(struct vframe_states *states, void *);
static int vh264_event_cb(int type, void *data, void *private_data);
static void vh264_work(struct work_struct *work);

static const char vh264_dec_id[] = "vh264-dev";

#define PROVIDER_NAME "vdec.h264"

static const struct vframe_operations_s vf_provider_ops = {
	.peek = vh264_vf_peek,
	.get = vh264_vf_get,
	.put = vh264_vf_put,
	.event_cb = vh264_event_cb,
	.vf_states = vh264_vf_states,
};

#define DEC_RESULT_NONE             0
#define DEC_RESULT_DONE             1
#define DEC_RESULT_AGAIN            2
#define DEC_RESULT_CONFIG_PARAM     3
#define DEC_RESULT_GET_DATA         4
#define DEC_RESULT_GET_DATA_RETRY   5
#define DEC_RESULT_ERROR            6

/*
static const char *dec_result_str[] = {
    "DEC_RESULT_NONE        ",
    "DEC_RESULT_DONE        ",
    "DEC_RESULT_AGAIN       ",
    "DEC_RESULT_CONFIG_PARAM",
    "DEC_RESULT_GET_DATA    ",
    "DEC_RESULT_GET_DA_RETRY",
    "DEC_RESULT_ERROR       ",
};
*/

#define UCODE_IP_ONLY 2
#define UCODE_IP_ONLY_PARAM 1

#define MC_OFFSET_HEADER    0x0000
#define MC_OFFSET_DATA      0x1000
#define MC_OFFSET_MMCO      0x2000
#define MC_OFFSET_LIST      0x3000
#define MC_OFFSET_SLICE     0x4000
#define MC_OFFSET_MAIN      0x5000

#define MC_TOTAL_SIZE       ((20+16)*SZ_1K)
#define MC_SWAP_SIZE        (4*SZ_1K)
#define MODE_ERROR 0
#define MODE_FULL  1

#define DFS_HIGH_THEASHOLD 3

#define INIT_FLAG_REG       AV_SCRATCH_2
#define HEAD_PADING_REG     AV_SCRATCH_3
#define UCODE_WATCHDOG_REG   AV_SCRATCH_7
#define LMEM_DUMP_ADR       AV_SCRATCH_L
#define DEBUG_REG1          AV_SCRATCH_M
#define DEBUG_REG2          AV_SCRATCH_N
#define FRAME_COUNTER_REG       AV_SCRATCH_I
#define RPM_CMD_REG          AV_SCRATCH_A
#define H264_DECODE_SIZE	AV_SCRATCH_E
#define H264_DECODE_INFO          M4_CONTROL_REG /* 0xc29 */
#define DPB_STATUS_REG       AV_SCRATCH_J
#define MBY_MBX                 MB_MOTION_MODE /*0xc07*/

struct vdec_h264_hw_s {
	spinlock_t lock;

	struct platform_device *platform_dev;
	struct device *cma_dev;
	/* struct page *cma_alloc_pages; */
	unsigned long cma_alloc_addr;
	int cma_alloc_count;
	/* struct page *collocate_cma_alloc_pages; */
	unsigned long collocate_cma_alloc_addr;
	int collocate_cma_alloc_count;

	ulong lmem_addr;
	dma_addr_t lmem_addr_remap;

	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);

	struct vframe_s vfpool[VF_POOL_SIZE];
	struct buffer_spec_s buffer_spec[MAX_VF_BUF_NUM];
	int buffer_spec_num;
	struct vframe_s switching_fense_vf;
	struct h264_dpb_stru dpb;
	u8 init_flag;
	u8 set_params_done;
	u32 max_reference_size;
	u32 decode_pic_count;
	int start_search_pos;

	unsigned char buffer_empty_flag;

	u32 frame_width;
	u32 frame_height;
	u32 frame_dur;
	u32 frame_prog;
	u32 frame_packing_type;

	struct vframe_chunk_s *chunk;

	u32 stat;
	unsigned long buf_start;
	u32 buf_offset;
	u32 buf_size;
	/* u32 ucode_map_start; */
	u32 pts_outside;
	u32 sync_outside;
	u32 vh264_ratio;
	u32 vh264_rotation;
	u32 use_idr_framerate;

	u32 seq_info;
	u32 timing_info_present_flag;
	u32 fixed_frame_rate_flag;
	u32 fixed_frame_rate_check_count;
	u32 iframe_count;
	u32 aspect_ratio_info;
	u32 num_units_in_tick;
	u32 time_scale;
	u32 h264_ar;
	bool h264_first_valid_pts_ready;
	u32 h264pts1;
	u32 h264pts2;
	u32 pts_duration;
	u32 h264_pts_count;
	u32 duration_from_pts_done;
	u32 pts_unstable;	u32 duration_on_correcting;
	u32 last_checkout_pts;
	u32 fatal_error_flag;
	u32 fatal_error_reset;
	u32 max_refer_buf;

	s32 vh264_stream_switching_state;
	struct vframe_s *p_last_vf;
	u32 last_pts;
	u32 last_pts_remainder;
	u32 last_duration;
	u32 saved_resolution;
	u32 last_mb_width, last_mb_height;
	bool check_pts_discontinue;
	bool pts_discontinue;
	u32 wait_buffer_counter;
	u32 first_offset;
	u32 first_pts;
	u64 first_pts64;
	bool first_pts_cached;

	void *sei_data_buffer;
	dma_addr_t sei_data_buffer_phys;

	uint error_recovery_mode;
	uint mb_total;
	uint mb_width;
	uint mb_height;

	uint ucode_type;
	dma_addr_t mc_dma_handle;
	void *mc_cpu_addr;
	int vh264_reset;

	atomic_t vh264_active;

	struct dec_sysinfo vh264_amstream_dec_info;

	struct work_struct error_wd_work;

	int dec_result;
	struct work_struct work;

	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;

	struct timer_list check_timer;

	/**/
	unsigned last_frame_time;

	/* timeout handle */
	unsigned long int start_process_time;
	unsigned last_mby_mbx;
	unsigned last_ucode_watchdog_reg_val;
	unsigned decode_timeout_count;
	unsigned timeout_num;
	unsigned search_dataempty_num;
	unsigned decode_timeout_num;
	unsigned decode_dataempty_num;
	unsigned buffer_empty_recover_num;

	/**/

	/*log*/
	unsigned int packet_write_success_count;
	unsigned int packet_write_EAGAIN_count;
	unsigned int packet_write_ENOMEM_count;
	unsigned int packet_write_EFAULT_count;
	unsigned int total_read_size_pre;
	unsigned int total_read_size;
	unsigned int frame_count_pre;
};


static void vh264_local_init(struct vdec_h264_hw_s *hw);
static int vh264_hw_ctx_restore(struct vdec_h264_hw_s *hw);
static int vh264_stop(struct vdec_h264_hw_s *hw, int mode);
static s32 vh264_init(struct vdec_h264_hw_s *hw);
static void set_frame_info(struct vdec_h264_hw_s *hw, struct vframe_s *vf,
			u32 index);

unsigned char have_free_buf_spec(struct vdec_s *vdec)
{
	int i;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	if ((h264_debug_flag&OUTPUT_CURRENT_BUF) == 0) {
		for (i = 0; i < hw->buffer_spec_num; i++) {
			if (hw->buffer_spec[i].used == 0)
				return 1;
		}
		return 0;
	} else
		return 1;
}

#if 0
static void buf_spec_recover(struct vdec_h264_hw_s *hw)
{ /* do not clear buf_spec used by display */
	int i;
	dpb_print(hw->dpb.decoder_index,
	PRINT_FLAG_DPB, "%s\n", __func__);
	for (i = 0; i < hw->buffer_spec_num; i++) {
		if (hw->buffer_spec[i].used == 2)
			hw->buffer_spec[i].used = 3;
		else
			hw->buffer_spec[i].used = 0;
	}
}
#endif

int get_free_buf_idx(struct vdec_s *vdec)
{
	int i;
	int index = -1;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;

	if ((h264_debug_flag&OUTPUT_CURRENT_BUF) == 0) {
		for (i = hw->start_search_pos; i < hw->buffer_spec_num; i++) {
			if (hw->buffer_spec[i].used == 0) {
				hw->buffer_spec[i].used = 1;
				hw->start_search_pos = i+1;
				index = i;
				break;
			}
		}
		if (index < 0) {
			for (i = 0; i < hw->start_search_pos; i++) {
				if (hw->buffer_spec[i].used == 0) {
					hw->buffer_spec[i].used = 1;
					hw->start_search_pos = i+1;
					index = i;
					break;
				}
			}
		}
	} else {
		index = hw->start_search_pos;
		hw->start_search_pos++;
	}

	if (hw->start_search_pos >= hw->buffer_spec_num)
		hw->start_search_pos = 0;

	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_DPB_DETAIL,
			"%s, buf_spec_num %d\n", __func__, index);

	return index;
}

int release_buf_spec_num(struct vdec_s *vdec, int buf_spec_num)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s buf_spec_num %d\n",
		__func__, buf_spec_num);
	if (buf_spec_num >= 0 && buf_spec_num < hw->buffer_spec_num)
		hw->buffer_spec[buf_spec_num].used = 0;
	return 0;
}

static int get_buf_spec_idx_by_canvas_config(struct vdec_h264_hw_s *hw,
	struct canvas_config_s *cfg)
{
	int i;
	for (i = 0; i < hw->buffer_spec_num; i++) {
		if (hw->buffer_spec[i].canvas_config[0].phy_addr
			== cfg->phy_addr)
			return i;
	}
	return -1;
}

int prepare_display_buf(struct vdec_s *vdec, struct FrameStore *frame)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	struct vframe_s *vf = NULL;

	if (kfifo_get(&hw->newframe_q, &vf) == 0) {
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_STATUS,
			"%s fatal error, no available buffer slot.\n",
			__func__);
		return -1;
	}

	if (vf) {
		int buffer_index = frame->buf_spec_num;
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_DPB_DETAIL,
			"%s, fs[%d] poc %d, buf_spec_num %d\n",
			__func__, frame->index, frame->poc,
			frame->buf_spec_num);
		vf->index = frame->index;
		vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
		vf->duration_pulldown = 0;
		vf->pts = frame->pts;
		vf->pts_us64 = frame->pts64;
		vf->canvas0Addr = vf->canvas1Addr =
			spec2canvas(&hw->buffer_spec[buffer_index]);
		set_frame_info(hw, vf, buffer_index);
		kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
		hw->buffer_spec[buffer_index].used = 2;

		vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
		decode_frame_count[hw->dpb.decoder_index]++;
	}

	return 0;
}

/******************
 * Hardware config
 */
char *slice_type_name[] = {
	"P_SLICE ",
	"B_SLICE ",
	"I_SLICE ",
	"SP_SLICE",
	"SI_SLICE",
};

char *picture_structure_name[] = {
	"FRAME",
	"TOP_FIELD",
	"BOTTOM_FIELD"
};

void print_pic_info(int decindex, const char *info,
			struct StorablePicture *pic,
			int slice_type)
{
	dpb_print(decindex, PRINT_FLAG_UCODE_EVT,
		"%s: %s (original %s), %s, mb_aff_frame_flag %d  poc %d, pic_num %d, buf_spec_num %d\n",
		info,
		picture_structure_name[pic->structure],
		pic->coded_frame ? "Frame" : "Field",
		(slice_type < 0) ? "" : slice_type_name[slice_type],
		pic->mb_aff_frame_flag,
		pic->poc,
		pic->pic_num,
		pic->buf_spec_num);
}

static void reset_process_time(struct vdec_h264_hw_s *hw)
{
	if (hw->start_process_time) {
		unsigned process_time =
			1000 * (jiffies - hw->start_process_time) / HZ;
		hw->start_process_time = 0;
		if (process_time > max_process_time[hw->dpb.decoder_index])
			max_process_time[hw->dpb.decoder_index] = process_time;
	}
}

void config_decode_buf(struct vdec_h264_hw_s *hw, struct StorablePicture *pic)
{
	/* static int count = 0; */
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	struct Slice *pSlice = &(p_H264_Dpb->mSlice);
	unsigned int colocate_adr_offset;
	unsigned int val;

#define H264_BUFFER_INFO_INDEX    PMV3_X /* 0xc24 */
#define H264_BUFFER_INFO_DATA   PMV2_X  /* 0xc22 */
#define H264_CURRENT_POC_IDX_RESET LAST_SLICE_MV_ADDR /* 0xc30 */
#define H264_CURRENT_POC          LAST_MVY /* 0xc32 shared with conceal MV */

#define H264_CO_MB_WR_ADDR        VLD_C38 /* 0xc38 */
/* bit 31:30 -- L1[0] picture coding structure,
	00 - top field,	01 - bottom field,
	10 - frame, 11 - mbaff frame
   bit 29 - L1[0] top/bot for B field pciture , 0 - top, 1 - bot
   bit 28:0 h264_co_mb_mem_rd_addr[31:3]
	-- only used for B Picture Direct mode [2:0] will set to 3'b000
*/
#define H264_CO_MB_RD_ADDR        VLD_C39 /* 0xc39 */

/* bit 15 -- flush co_mb_data to DDR -- W-Only
   bit 14 -- h264_co_mb_mem_wr_addr write Enable -- W-Only
   bit 13 -- h264_co_mb_info_wr_ptr write Enable -- W-Only
   bit 9 -- soft_reset -- W-Only
   bit 8 -- upgent
   bit 7:2 -- h264_co_mb_mem_wr_addr
   bit 1:0 -- h264_co_mb_info_wr_ptr
*/
#define H264_CO_MB_RW_CTL         VLD_C3D /* 0xc3d */

	unsigned long canvas_adr;
	unsigned ref_reg_val;
	unsigned one_ref_cfg = 0;
	int h264_buffer_info_data_write_count;
	int i, j;
	unsigned colocate_wr_adr;
	unsigned colocate_rd_adr;
	unsigned char use_direct_8x8;

	WRITE_VREG(H264_CURRENT_POC_IDX_RESET, 0);
	WRITE_VREG(H264_CURRENT_POC, pic->frame_poc);
	WRITE_VREG(H264_CURRENT_POC, pic->top_poc);
	WRITE_VREG(H264_CURRENT_POC, pic->bottom_poc);

	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
		"%s: pic_num is %d, poc is %d (%d, %d, %d), buf_spec_num %d\n",
		__func__, pic->pic_num, pic->poc, pic->frame_poc,
		pic->top_poc, pic->bottom_poc, pic->buf_spec_num);
	print_pic_info(hw->dpb.decoder_index, "cur", pic, pSlice->slice_type);

	WRITE_VREG(CURR_CANVAS_CTRL, pic->buf_spec_num << 24);

	canvas_adr = READ_VREG(CURR_CANVAS_CTRL) & 0xffffff;

	WRITE_VREG(REC_CANVAS_ADDR, canvas_adr);
	WRITE_VREG(DBKR_CANVAS_ADDR, canvas_adr);
	WRITE_VREG(DBKW_CANVAS_ADDR, canvas_adr);

	if (pic->mb_aff_frame_flag)
		hw->buffer_spec[pic->buf_spec_num].info0 = 0xf4c0;
	else if (pic->structure == TOP_FIELD)
		hw->buffer_spec[pic->buf_spec_num].info0 = 0xf400;
	else if (pic->structure == BOTTOM_FIELD)
		hw->buffer_spec[pic->buf_spec_num].info0 = 0xf440;
	else
		hw->buffer_spec[pic->buf_spec_num].info0 = 0xf480;

	if (pic->bottom_poc < pic->top_poc)
		hw->buffer_spec[pic->buf_spec_num].info0 |= 0x100;

	hw->buffer_spec[pic->buf_spec_num].info1 = pic->top_poc;
	hw->buffer_spec[pic->buf_spec_num].info2 = pic->bottom_poc;
	WRITE_VREG(H264_BUFFER_INFO_INDEX, 16);

	for (i = 0; i < hw->buffer_spec_num; i++) {
		int long_term_flag =
			get_long_term_flag_by_buf_spec_num(p_H264_Dpb, i);
		if (long_term_flag > 0) {
			if (long_term_flag & 0x1)
				hw->buffer_spec[i].info0 |= (1 << 4);
			else
				hw->buffer_spec[i].info0 &= ~(1 << 4);

			if (long_term_flag & 0x2)
				hw->buffer_spec[i].info0 |= (1 << 5);
			else
				hw->buffer_spec[i].info0 &= ~(1 << 5);
		}

		if (i == pic->buf_spec_num)
			WRITE_VREG(H264_BUFFER_INFO_DATA,
				hw->buffer_spec[i].info0 | 0xf);
		else
			WRITE_VREG(H264_BUFFER_INFO_DATA,
				hw->buffer_spec[i].info0);
		WRITE_VREG(H264_BUFFER_INFO_DATA, hw->buffer_spec[i].info1);
		WRITE_VREG(H264_BUFFER_INFO_DATA, hw->buffer_spec[i].info2);
	}

	/* config reference buffer */
	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
		"list0 size %d\n", pSlice->listXsize[0]);
	WRITE_VREG(H264_BUFFER_INFO_INDEX, 0);
	ref_reg_val = 0;
	j = 0;
	h264_buffer_info_data_write_count = 0;

	for (i = 0; i < (unsigned int)(pSlice->listXsize[0]); i++) {
		/*ref list 0 */
		struct StorablePicture *ref = pSlice->listX[0][i];
		unsigned cfg;
		/* bit[6:5] - frame/field info,
		 * 01 - top, 10 - bottom, 11 - frame
		 */
#ifdef ERROR_CHECK
		if (ref == NULL)
			return;
#endif
		if (ref->structure == TOP_FIELD)
			cfg = 0x1;
		else if (ref->structure == BOTTOM_FIELD)
			cfg = 0x2;
		else /* FRAME */
			cfg = 0x3;
		one_ref_cfg = (ref->buf_spec_num & 0x1f) | (cfg << 5);
		ref_reg_val <<= 8;
		ref_reg_val |= one_ref_cfg;
		j++;

		if (j == 4) {
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
				"H264_BUFFER_INFO_DATA: %x\n", ref_reg_val);
			WRITE_VREG(H264_BUFFER_INFO_DATA, ref_reg_val);
			h264_buffer_info_data_write_count++;
			j = 0;
		}
		print_pic_info(hw->dpb.decoder_index, "list0",
			pSlice->listX[0][i], -1);
	}
	if (j != 0) {
		while (j != 4) {
			ref_reg_val <<= 8;
			ref_reg_val |= one_ref_cfg;
			j++;
		}
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
			"H264_BUFFER_INFO_DATA: %x\n",
					ref_reg_val);
		WRITE_VREG(H264_BUFFER_INFO_DATA, ref_reg_val);
		h264_buffer_info_data_write_count++;
	}
	ref_reg_val = (one_ref_cfg << 24) | (one_ref_cfg<<16) |
				(one_ref_cfg << 8) | one_ref_cfg;
	for (i = h264_buffer_info_data_write_count; i < 8; i++)
		WRITE_VREG(H264_BUFFER_INFO_DATA, ref_reg_val);

	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
		"list1 size %d\n", pSlice->listXsize[1]);
	WRITE_VREG(H264_BUFFER_INFO_INDEX, 8);
	ref_reg_val = 0;
	j = 0;

	for (i = 0; i < (unsigned int)(pSlice->listXsize[1]); i++) {
		/* ref list 0 */
		struct StorablePicture *ref = pSlice->listX[1][i];
		unsigned cfg;
		/* bit[6:5] - frame/field info,
		 * 01 - top, 10 - bottom, 11 - frame
		 */
#ifdef ERROR_CHECK
		if (ref == NULL)
			return;
#endif
		if (ref->structure == TOP_FIELD)
			cfg = 0x1;
		else if (ref->structure == BOTTOM_FIELD)
			cfg = 0x2;
		else /* FRAME */
			cfg = 0x3;
		one_ref_cfg = (ref->buf_spec_num & 0x1f) | (cfg << 5);
		ref_reg_val <<= 8;
		ref_reg_val |= one_ref_cfg;
		j++;

		if (j == 4) {
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
				"H264_BUFFER_INFO_DATA: %x\n",
				ref_reg_val);
			WRITE_VREG(H264_BUFFER_INFO_DATA, ref_reg_val);
			j = 0;
		}
		print_pic_info(hw->dpb.decoder_index, "list1",
			pSlice->listX[1][i], -1);
	}
	if (j != 0) {
		while (j != 4) {
			ref_reg_val <<= 8;
			ref_reg_val |= one_ref_cfg;
			j++;
		}
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
		"H264_BUFFER_INFO_DATA: %x\n", ref_reg_val);
		WRITE_VREG(H264_BUFFER_INFO_DATA, ref_reg_val);
	}

	/* configure co-locate buffer */
	while ((READ_VREG(H264_CO_MB_RW_CTL) >> 11) & 0x1)
		;
	if ((pSlice->mode_8x8_flags & 0x4) &&
		(pSlice->mode_8x8_flags & 0x2))
		use_direct_8x8 = 1;
	else
		use_direct_8x8 = 0;

#ifndef ONE_COLOCATE_BUF_PER_DECODE_BUF
	colocate_adr_offset =
		((pic->structure == FRAME && pic->mb_aff_frame_flag == 0)
		 ? 1 : 2) * 96;
	if (use_direct_8x8)
		colocate_adr_offset >>= 2;

	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
		"colocate buf size of each mb 0x%x first_mb_in_slice 0x%x colocate_adr_offset 0x%x\r\n",
		colocate_adr_offset, pSlice->first_mb_in_slice,
		colocate_adr_offset * pSlice->first_mb_in_slice);

	colocate_adr_offset *= pSlice->first_mb_in_slice;

	if ((pic->colocated_buf_index >= 0) &&
		(pic->colocated_buf_index < p_H264_Dpb->colocated_buf_count)) {
		colocate_wr_adr = p_H264_Dpb->colocated_mv_addr_start +
			((p_H264_Dpb->colocated_buf_size *
			pic->colocated_buf_index)
			>> (use_direct_8x8 ? 2 : 0));
		if ((colocate_wr_adr + p_H264_Dpb->colocated_buf_size) >
			p_H264_Dpb->colocated_mv_addr_end)
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_ERROR,
				"Error, colocate buf is not enough, index is %d\n",
			pic->colocated_buf_index);
		val = colocate_wr_adr + colocate_adr_offset;
		WRITE_VREG(H264_CO_MB_WR_ADDR, val);
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
			"WRITE_VREG(H264_CO_MB_WR_ADDR) = %x, first_mb_in_slice %x pic_structure %x  colocate_adr_offset %x mode_8x8_flags %x colocated_buf_size %x\n",
			val, pSlice->first_mb_in_slice, pic->structure,
			colocate_adr_offset, pSlice->mode_8x8_flags,
			p_H264_Dpb->colocated_buf_size);
	} else {
		WRITE_VREG(H264_CO_MB_WR_ADDR, 0xffffffff);
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
			"WRITE_VREG(H264_CO_MB_WR_ADDR) = 0xffffffff\n");
	}
#else
	colocate_adr_offset =
	((pic->structure == FRAME && pic->mb_aff_frame_flag == 0) ? 1 : 2) * 96;
	if (use_direct_8x8)
		colocate_adr_offset >>= 2;

	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
		"colocate buf size of each mb 0x%x first_mb_in_slice 0x%x colocate_adr_offset 0x%x\r\n",
		colocate_adr_offset, pSlice->first_mb_in_slice,
		colocate_adr_offset * pSlice->first_mb_in_slice);

	colocate_adr_offset *= pSlice->first_mb_in_slice;

	colocate_wr_adr = p_H264_Dpb->colocated_mv_addr_start +
		((p_H264_Dpb->colocated_buf_size * pic->buf_spec_num) >>
			(use_direct_8x8 ? 2 : 0));

	if ((colocate_wr_adr + p_H264_Dpb->colocated_buf_size) >
		p_H264_Dpb->colocated_mv_addr_end)
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_ERROR,
		"Error, colocate buf is not enough, pic index is %d\n",
				pic->buf_spec_num);
	val = colocate_wr_adr + colocate_adr_offset;
	WRITE_VREG(H264_CO_MB_WR_ADDR, val);
	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
		"WRITE_VREG(H264_CO_MB_WR_ADDR) = %x, first_mb_in_slice %x pic_structure %x colocate_adr_offset %x mode_8x8_flags %x colocated_buf_size %x\n",
		val, pSlice->first_mb_in_slice, pic->structure,
		colocate_adr_offset, pSlice->mode_8x8_flags,
		p_H264_Dpb->colocated_buf_size);
#endif
	if (pSlice->listXsize[1] > 0) {
		struct StorablePicture *colocate_pic = pSlice->listX[1][0];
		/* H264_CO_MB_RD_ADDR[bit 31:30],
		 * original picture structure of L1[0],
		 * 00 - top field, 01 - bottom field,
		 * 10 - frame, 11 - mbaff frame
		 */
		int l10_structure;
		int cur_colocate_ref_type;
		/* H264_CO_MB_RD_ADDR[bit 29], top/bot for B field pciture,
		 * 0 - top, 1 - bot
		 */
		unsigned int val;
#ifdef ERROR_CHECK
		if (colocate_pic == NULL)
			return;
#endif

		if (colocate_pic->mb_aff_frame_flag)
			l10_structure = 3;
		else {
			if (colocate_pic->coded_frame)
				l10_structure = 2;
			else
				l10_structure =	(colocate_pic->structure ==
					BOTTOM_FIELD) ?	1 : 0;
		}
#if 0
		/*case0016, p16,
		cur_colocate_ref_type should be configured base on current pic*/
		if (pic->structure == FRAME &&
			pic->mb_aff_frame_flag)
			cur_colocate_ref_type = 0;
		else if (pic->structure == BOTTOM_FIELD)
			cur_colocate_ref_type = 1;
		else
			cur_colocate_ref_type = 0;
#else
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
			" CUR TMP DEBUG : mb_aff_frame_flag : %d, structure : %d coded_frame %d\n",
			pic->mb_aff_frame_flag,
			pic->structure,
			pic->coded_frame);
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
			" COL TMP DEBUG : mb_aff_frame_flag : %d, structure : %d coded_frame %d\n",
			colocate_pic->mb_aff_frame_flag,
				colocate_pic->structure,
				colocate_pic->coded_frame);
		if (pic->structure == FRAME  || pic->mb_aff_frame_flag) {
			cur_colocate_ref_type =
				(abs(pic->poc - colocate_pic->top_poc)
				< abs(pic->poc -
				colocate_pic->bottom_poc)) ? 0 : 1;
		} else
			cur_colocate_ref_type =
				(colocate_pic->structure
					== BOTTOM_FIELD) ? 1 : 0;
#endif

#ifndef ONE_COLOCATE_BUF_PER_DECODE_BUF
		if ((colocate_pic->colocated_buf_index >= 0) &&
			(colocate_pic->colocated_buf_index <
				p_H264_Dpb->colocated_buf_count)) {
			colocate_rd_adr = p_H264_Dpb->colocated_mv_addr_start +
				((p_H264_Dpb->colocated_buf_size *
				colocate_pic->colocated_buf_index)
				>> (use_direct_8x8 ? 2 : 0));
			if ((colocate_rd_adr + p_H264_Dpb->colocated_buf_size) >
				p_H264_Dpb->colocated_mv_addr_end)
				dpb_print(hw->dpb.decoder_index,
					PRINT_FLAG_ERROR,
				"Error, colocate buf is not enough, index is %d\n",
					colocate_pic->colocated_buf_index);
			/* bit 31:30 -- L1[0] picture coding structure,
			 * 00 - top field, 01 - bottom field,
			 * 10 - frame, 11 - mbaff frame
			 * bit 29 - L1[0] top/bot for B field pciture,
			 * 0 - top, 1 - bot
			 * bit 28:0 h264_co_mb_mem_rd_addr[31:3]
			 * -- only used for B Picture Direct mode
			 * [2:0] will set to 3'b000
			 */
			/* #define H264_CO_MB_RD_ADDR        VLD_C39 0xc39 */
			val = ((colocate_rd_adr+colocate_adr_offset) >> 3) |
				(l10_structure << 30) |
				(cur_colocate_ref_type << 29);
			WRITE_VREG(H264_CO_MB_RD_ADDR, val);
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
				"co idx %d, WRITE_VREG(H264_CO_MB_RD_ADDR) = %x, addr %x L1(0) pic_structure %d mbaff %d\n",
				colocate_pic->colocated_buf_index,
				val, colocate_rd_adr + colocate_adr_offset,
				colocate_pic->structure,
				colocate_pic->mb_aff_frame_flag);
		} else
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_ERROR,
			"Error, reference pic has no colocated buf\n");
#else
		colocate_rd_adr = p_H264_Dpb->colocated_mv_addr_start +
			((p_H264_Dpb->colocated_buf_size *
				colocate_pic->buf_spec_num)
				>> (use_direct_8x8 ? 2 : 0));
		if ((colocate_rd_adr + p_H264_Dpb->colocated_buf_size) >
			p_H264_Dpb->colocated_mv_addr_end)
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_ERROR,
				"Error, colocate buf is not enough, pic index is %d\n",
				colocate_pic->buf_spec_num);
		/* bit 31:30 -- L1[0] picture coding structure,
		 * 00 - top field, 01 - bottom field,
		 * 10 - frame, 11 - mbaff frame
		 * bit 29 - L1[0] top/bot for B field pciture,
		 * 0 - top, 1 - bot
		 * bit 28:0 h264_co_mb_mem_rd_addr[31:3]
		 * -- only used for B Picture Direct mode
		 * [2:0] will set to 3'b000
		 */
		/* #define H264_CO_MB_RD_ADDR        VLD_C39 0xc39 */
		val = ((colocate_rd_adr+colocate_adr_offset)>>3) |
			(l10_structure << 30) | (cur_colocate_ref_type << 29);
		WRITE_VREG(H264_CO_MB_RD_ADDR, val);
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
			"WRITE_VREG(H264_CO_MB_RD_ADDR) = %x, L1(0) pic_structure %d mbaff %d\n",
			val, colocate_pic->structure,
			colocate_pic->mb_aff_frame_flag);
#endif
	}
}

static int vh264_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;
	struct vdec_s *vdec = op_arg;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;

	spin_lock_irqsave(&hw->lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hw->newframe_q);
	states->buf_avail_num = kfifo_len(&hw->display_q);

	spin_unlock_irqrestore(&hw->lock, flags);

	return 0;
}

static struct vframe_s *vh264_vf_peek(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;

	if (!hw)
		return NULL;

	if (kfifo_peek(&hw->display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vh264_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;

	if (!hw)
		return NULL;

	if (kfifo_get(&hw->display_q, &vf)) {
		int time = jiffies;
		unsigned int frame_interval =
			1000*(time - hw->last_frame_time)/HZ;
		if ((h264_debug_flag & OUTPUT_CURRENT_BUF) == 0) {
			struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
				"%s from fs[%d], poc %d buf_spec_num %d vf %p\n",
				__func__, vf->index,
				p_H264_Dpb->mFrameStore[vf->index].poc,
				p_H264_Dpb->mFrameStore[vf->index]
						.buf_spec_num, vf);
		}
		if (hw->last_frame_time > 0) {
			dpb_print(hw->dpb.decoder_index,
			PRINT_FLAG_TIME_STAMP,
			"%s duration %d pts %d interval %dms\r\n",
			__func__, vf->duration, vf->pts, frame_interval);
			if (frame_interval >
				max_get_frame_interval[hw->dpb.decoder_index])
				max_get_frame_interval[hw->dpb.decoder_index]
				= frame_interval;
		}
		hw->last_frame_time = time;
		return vf;
	}

	return NULL;
}

static void vh264_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int buf_spec_num;

	if ((h264_debug_flag & OUTPUT_CURRENT_BUF) == 0) {
		buf_spec_num =
			get_buf_spec_idx_by_canvas_config(hw,
				&vf->canvas0_config[0]);
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
			"%s to fs[%d], poc %d buf_spec_num %d used %d\n",
			__func__, vf->index,
			p_H264_Dpb->mFrameStore[vf->index].poc,
			buf_spec_num,
			hw->buffer_spec[buf_spec_num].used);

		if (hw->buffer_spec[buf_spec_num].used != 3)
			set_frame_output_flag(&hw->dpb, vf->index);
	}

	kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);

#define ASSIST_MBOX1_IRQ_REG    VDEC_ASSIST_MBOX1_IRQ_REG
	if (hw->buffer_empty_flag)
		WRITE_VREG(ASSIST_MBOX1_IRQ_REG, 0x1);
}

static int vh264_event_cb(int type, void *data,	void *private_data)
{
	return 0;
}

static void set_frame_info(struct vdec_h264_hw_s *hw, struct vframe_s *vf,
				u32 index)
{
	int force_rate = input_frame_based(hw_to_vdec(hw)) ?
		force_rate_framebase : force_rate_streambase;
	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
		"%s (%d,%d) dur %d, vf %p, index %d\n", __func__,
		hw->frame_width, hw->frame_height, hw->frame_dur, vf, index);

	vf->width = hw->frame_width;
	vf->height = hw->frame_height;
	if (force_rate) {
		if (force_rate == -1)
			vf->duration = 0;
		else
			vf->duration = 96000/force_rate;
	} else
		vf->duration = hw->frame_dur;
	vf->ratio_control =
		(min(hw->h264_ar, (u32) DISP_RATIO_ASPECT_RATIO_MAX)) <<
		DISP_RATIO_ASPECT_RATIO_BIT;
	vf->orientation = hw->vh264_rotation;
	vf->flag = 0;

	vf->canvas0Addr = vf->canvas1Addr = -1;
#ifdef NV21
	vf->plane_num = 2;
#else
	vf->plane_num = 3;
#endif
	vf->canvas0_config[0] = hw->buffer_spec[index].canvas_config[0];
	vf->canvas0_config[1] = hw->buffer_spec[index].canvas_config[1];
#ifndef NV21
	vf->canvas0_config[2] = hw->buffer_spec[index].canvas_config[2];
#endif
	vf->canvas1_config[0] = hw->buffer_spec[index].canvas_config[0];
	vf->canvas1_config[1] = hw->buffer_spec[index].canvas_config[1];
#ifndef NV21
	vf->canvas1_config[2] = hw->buffer_spec[index].canvas_config[2];
#endif

	return;
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
	case 52:
	default:
		size = 70778880;
		break;
	}

	size /= pic_size;
	size = size + 1;	/* need one more buffer */

	if (max_reference_frame_num > size)
		size = max_reference_frame_num;

	return size;
}

static int vh264_set_params(struct vdec_h264_hw_s *hw)
{
	int mb_width, mb_total;
	int max_reference_size, level_idc;
	int i, mb_height, addr;
	int mb_mv_byte;
	struct vdec_s *vdec = hw_to_vdec(hw);
	int reg_val;
	int ret = 0;
#ifdef USE_CMA
	unsigned int buf_size;
#endif

	if (hw->set_params_done) {
		WRITE_VREG(AV_SCRATCH_0,
			(hw->max_reference_size << 24) |
			(hw->buffer_spec_num << 16) |
			(hw->buffer_spec_num << 8));
		return 0;
	}
	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT, "%s\n",
		__func__);

#ifndef USE_CMA
	addr = hw->buf_start;
#endif

	/* Read AV_SCRATCH_1 */
	reg_val = READ_VREG(AV_SCRATCH_1);
	hw->seq_info = READ_VREG(AV_SCRATCH_2);
	hw->num_units_in_tick = READ_VREG(AV_SCRATCH_4);
	hw->time_scale = READ_VREG(AV_SCRATCH_5);
	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT, "get %x\n",
		reg_val);
	mb_mv_byte = (reg_val & 0x80000000) ? 24 : 96;

	mb_width = reg_val & 0xff;
	mb_total = (reg_val >> 8) & 0xffff;
	if (!mb_width && mb_total) /*for 4k2k*/
		mb_width = 256;

	mb_height = mb_total/mb_width;
#if 1
	/* if (hw->frame_width == 0 || hw->frame_height == 0) { */
	hw->frame_width = mb_width * 16;
	hw->frame_height = mb_height * 16;
	/* } */

	if (hw->frame_dur == 0)
		hw->frame_dur = 96000 / 30;
#endif

	mb_width = (mb_width+3) & 0xfffffffc;
	mb_height = (mb_height+3) & 0xfffffffc;
	mb_total = mb_width * mb_height;

	reg_val = READ_VREG(AV_SCRATCH_B);
	level_idc = reg_val & 0xff;
	max_reference_size = (reg_val >> 8) & 0xff;
	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
		"mb height/widht/total: %x/%x/%x level_idc %x max_ref_num %x\n",
		mb_height, mb_width, mb_total, level_idc, max_reference_size);


	hw->mb_total = mb_total;
	hw->mb_width = mb_width;
	hw->mb_height = mb_height;

	hw->dpb.reorder_pic_num =
		get_max_dec_frame_buf_size(level_idc,
		max_reference_size, mb_width, mb_height);
	hw->buffer_spec_num =
		hw->dpb.reorder_pic_num
		+ reorder_dpb_size_margin;
	hw->max_reference_size = max_reference_size + reference_buf_margin;

	if (hw->buffer_spec_num > MAX_VF_BUF_NUM) {
		hw->buffer_spec_num = MAX_VF_BUF_NUM;
		hw->dpb.reorder_pic_num = hw->buffer_spec_num
			- reorder_dpb_size_margin;
	}
	hw->dpb.mDPB.size = hw->buffer_spec_num;
	hw->dpb.max_reference_size = hw->max_reference_size;

	pr_info("%s buf_spec_num %d reorder_pic_num %d collocate_buf_num %d\r\n",
		__func__, hw->buffer_spec_num,
		hw->dpb.reorder_pic_num,
		hw->max_reference_size);

#ifdef USE_CMA
	buf_size = (hw->mb_total << 8) + (hw->mb_total << 7);
#endif
	for (i = 0; i < hw->buffer_spec_num; i++) {
		int canvas = vdec->get_canvas(i, 2);

#ifdef USE_CMA
		if (hw->buffer_spec[i].cma_alloc_count == 0) {
			hw->buffer_spec[i].cma_alloc_count =
				PAGE_ALIGN(buf_size) / PAGE_SIZE;
			hw->buffer_spec[i].cma_alloc_addr =
				codec_mm_alloc_for_dma(MEM_NAME,
					hw->buffer_spec[i].cma_alloc_count,
					16, CODEC_MM_FLAGS_FOR_VDECODER);
		}

		if (!hw->buffer_spec[i].cma_alloc_addr) {
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_ERROR,
			"CMA alloc failed, request buf size 0x%lx\n",
				hw->buffer_spec[i].cma_alloc_count * PAGE_SIZE);
			hw->buffer_spec[i].cma_alloc_count = 0;
			ret = -1;
			break;
		}
		hw->buffer_spec[i].buf_adr =
		hw->buffer_spec[i].cma_alloc_addr;
		addr = hw->buffer_spec[i].buf_adr;
#else
		hw->buffer_spec[i].buf_adr = addr;
#endif

		hw->buffer_spec[i].used = 0;
		hw->buffer_spec[i].y_addr = addr;
		addr += hw->mb_total << 8;

		hw->buffer_spec[i].u_addr = addr;
		hw->buffer_spec[i].v_addr = addr;
		addr += hw->mb_total << 7;

		hw->buffer_spec[i].y_canvas_index = canvas_y(canvas);
		hw->buffer_spec[i].u_canvas_index = canvas_u(canvas);
		hw->buffer_spec[i].v_canvas_index = canvas_v(canvas);

		canvas_config(hw->buffer_spec[i].y_canvas_index,
			hw->buffer_spec[i].y_addr,
			hw->mb_width << 4,
			hw->mb_height << 4,
			CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_32X32);

		hw->buffer_spec[i].canvas_config[0].phy_addr =
				hw->buffer_spec[i].y_addr;
		hw->buffer_spec[i].canvas_config[0].width =
				hw->mb_width << 4;
		hw->buffer_spec[i].canvas_config[0].height =
				hw->mb_height << 4;
		hw->buffer_spec[i].canvas_config[0].block_mode =
				CANVAS_BLKMODE_32X32;

		canvas_config(hw->buffer_spec[i].u_canvas_index,
			hw->buffer_spec[i].u_addr,
			hw->mb_width << 4,
			hw->mb_height << 3,
			CANVAS_ADDR_NOWRAP,
			CANVAS_BLKMODE_32X32);

		hw->buffer_spec[i].canvas_config[1].phy_addr =
				hw->buffer_spec[i].u_addr;
		hw->buffer_spec[i].canvas_config[1].width =
				hw->mb_width << 4;
		hw->buffer_spec[i].canvas_config[1].height =
				hw->mb_height << 3;
		hw->buffer_spec[i].canvas_config[1].block_mode =
				CANVAS_BLKMODE_32X32;

		WRITE_VREG(ANC0_CANVAS_ADDR + i,
				spec2canvas(&hw->buffer_spec[i]));

		pr_info("config canvas (%d)\r\n", i);
	}


#ifdef USE_CMA
	if (hw->collocate_cma_alloc_count == 0) {
		hw->collocate_cma_alloc_count =
			PAGE_ALIGN(hw->mb_total * mb_mv_byte *
				hw->max_reference_size) / PAGE_SIZE;
		hw->collocate_cma_alloc_addr =
			codec_mm_alloc_for_dma(MEM_NAME,
				hw->collocate_cma_alloc_count,
				16, CODEC_MM_FLAGS_FOR_VDECODER);
	}

	if (!hw->collocate_cma_alloc_addr) {
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_ERROR,
		"codec_mm alloc failed, request buf size 0x%lx\n",
			hw->collocate_cma_alloc_count * PAGE_SIZE);
		hw->collocate_cma_alloc_count = 0;
		ret = -1;
	} else {
		hw->dpb.colocated_mv_addr_start =
			hw->collocate_cma_alloc_addr;
		hw->dpb.colocated_mv_addr_end  =
			hw->dpb.colocated_mv_addr_start +
			(hw->mb_total * mb_mv_byte * hw->max_reference_size);
		pr_info("callocate cma %d, %lx, %x\n",
			hw->collocate_cma_alloc_count,
			hw->collocate_cma_alloc_addr,
			hw->dpb.colocated_mv_addr_start);
	}
#else
	hw->dpb.colocated_mv_addr_start  = addr;
	hw->dpb.colocated_mv_addr_end  = addr + (hw->mb_total * mb_mv_byte
			* hw->max_reference_size);
#endif
	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
		"colocated_mv_addr_start %x colocated_mv_addr_end %x\n",
		hw->dpb.colocated_mv_addr_start,
		hw->dpb.colocated_mv_addr_end);

	hw->timing_info_present_flag = hw->seq_info & 0x2;
	hw->fixed_frame_rate_flag = 0;
	if (hw->timing_info_present_flag) {
		hw->fixed_frame_rate_flag = hw->seq_info & 0x40;

		if (((hw->num_units_in_tick * 120) >= hw->time_scale &&
			((!hw->sync_outside) ||
				(!hw->frame_dur)))
			&& hw->num_units_in_tick && hw->time_scale) {
			if (hw->use_idr_framerate ||
				hw->fixed_frame_rate_flag ||
				!hw->frame_dur ||
				!hw->duration_from_pts_done
				/*|| vh264_running*/) {
				u32 frame_dur_es =
				div_u64(96000ULL * 2 * hw->num_units_in_tick,
					hw->time_scale);
				if (hw->frame_dur != frame_dur_es) {
					hw->h264_first_valid_pts_ready = false;
					hw->h264pts1 = 0;
					hw->h264pts2 = 0;
					hw->h264_pts_count = 0;
					hw->duration_from_pts_done = 0;
					fixed_frame_rate_mode =
						FIX_FRAME_RATE_OFF;
					hw->pts_duration = 0;
					hw->frame_dur = frame_dur_es;
					pr_info("frame_dur %d from timing_info\n",
						hw->frame_dur);
				}

				/*hack to avoid use ES frame duration when
				it's half of the rate from system info
				 sometimes the encoder is given a wrong
				 frame rate but the system side infomation
				 is more reliable
				if ((frame_dur * 2) != frame_dur_es) {
				    frame_dur = frame_dur_es;
				}*/
			}
		}
	} else {
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
			"H.264: timing_info not present\n");
	}

	if (hw->pts_unstable && (hw->fixed_frame_rate_flag == 0)) {
		if (((RATE_2397_FPS == hw->frame_dur)
		&& (dec_control
		& DEC_CONTROL_FLAG_FORCE_RATE_2397_FPS_FIX_FRAME_RATE))
			|| ((RATE_2997_FPS ==
			hw->frame_dur) &&
		(dec_control &
			DEC_CONTROL_FLAG_FORCE_RATE_2997_FPS_FIX_FRAME_RATE))) {
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
				"force fix frame rate\n");
			hw->fixed_frame_rate_flag = 0x40;
		}
	}

	hw->set_params_done = 1;

	WRITE_VREG(AV_SCRATCH_0, (hw->max_reference_size<<24) |
		(hw->buffer_spec_num<<16) |
		(hw->buffer_spec_num<<8));

	return ret;
}

static bool is_buffer_available(struct vdec_s *vdec)
{
	bool buffer_available = 1;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;

	if ((kfifo_len(&hw->newframe_q) <= 0) ||
	    ((hw->set_params_done) && (!have_free_buf_spec(vdec))) ||
	    ((p_H264_Dpb->mDPB.init_done) &&
	     (p_H264_Dpb->mDPB.used_size == p_H264_Dpb->mDPB.size) &&
	     (is_there_unused_frame_from_dpb(&p_H264_Dpb->mDPB) == 0))) {
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
		"%s, empty, newq(%d), free_spec(%d), initdon(%d), used_size(%d/%d), unused_fr_dpb(%d)\n",
		__func__,
		kfifo_len(&hw->newframe_q),
		have_free_buf_spec(vdec),
		p_H264_Dpb->mDPB.init_done,
		p_H264_Dpb->mDPB.used_size, p_H264_Dpb->mDPB.size,
		is_there_unused_frame_from_dpb(&p_H264_Dpb->mDPB)
		);
		buffer_available = 0;

		bufmgr_h264_remove_unused_frame(p_H264_Dpb);
	}

	return buffer_available;
}

static irqreturn_t vh264_isr(struct vdec_s *vdec)
{
	unsigned int dec_dpb_status;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int i;

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	if (!hw) {
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_STATUS,
			"decoder is not running\n");
		return IRQ_HANDLED;
	}

	p_H264_Dpb->vdec = vdec;
	dec_dpb_status = READ_VREG(DPB_STATUS_REG);

	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
			"%s DPB_STATUS_REG: %x, sb (%x %x %x) bitcnt %x\n",
			__func__,
			dec_dpb_status,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			READ_VREG(VIFF_BIT_CNT));

	if (dec_dpb_status == H264_CONFIG_REQUEST) {
		WRITE_VREG(DPB_STATUS_REG, H264_ACTION_CONFIG_DONE);
#ifdef USE_CMA
		if (hw->set_params_done) {
			WRITE_VREG(AV_SCRATCH_0,
				(hw->max_reference_size<<24) |
				(hw->buffer_spec_num<<16) |
				(hw->buffer_spec_num<<8));
		} else {
			hw->dec_result = DEC_RESULT_CONFIG_PARAM;
			schedule_work(&hw->work);
		}
#else
		if (vh264_set_params(hw) < 0) {
			hw->fatal_error_flag = DECODER_FATAL_ERROR_UNKNOW;
			if (!hw->fatal_error_reset)
				schedule_work(&hw->error_wd_work);
		}
#endif
	} else if (dec_dpb_status == H264_SLICE_HEAD_DONE) {
		int slice_header_process_status = 0;
		unsigned short *p = (unsigned short *)hw->lmem_addr;

		dma_sync_single_for_cpu(
			amports_get_dma_device(),
			hw->lmem_addr_remap,
			PAGE_SIZE,
			DMA_FROM_DEVICE);
#if 0
		if (p_H264_Dpb->mVideo.dec_picture == NULL) {
			if (!is_buffer_available(vdec)) {
				hw->buffer_empty_flag = 1;
				dpb_print(hw->dpb.decoder_index,
				PRINT_FLAG_UCODE_EVT,
				"%s, buffer_empty, newframe_q(%d), have_free_buf_spec(%d), init_done(%d), used_size(%d/%d), is_there_unused_frame_from_dpb(%d)\n",
					__func__,
					kfifo_len(&hw->newframe_q),
					have_free_buf_spec(vdec),
					p_H264_Dpb->mDPB.init_done,
					p_H264_Dpb->mDPB.used_size,
					p_H264_Dpb->mDPB.size,
					is_there_unused_frame_from_dpb(
						&p_H264_Dpb->mDPB));
				return IRQ_HANDLED;
			}
		}

		hw->buffer_empty_flag = 0;
#endif
#ifdef SEND_PARAM_WITH_REG
		for (i = 0; i < (RPM_END-RPM_BEGIN); i++) {
			unsigned int data32;
			do {
				data32 = READ_VREG(RPM_CMD_REG);
				/* printk("%x\n", data32); */
			} while ((data32&0x10000) == 0);
			p_H264_Dpb->dpb_param.l.data[i] = data32 & 0xffff;
			WRITE_VREG(RPM_CMD_REG, 0);
			/* printk("%x:%x\n", i,data32); */
		}
#else
		for (i = 0; i < (RPM_END-RPM_BEGIN); i += 4) {
			int ii;
			for (ii = 0; ii < 4; ii++) {
				p_H264_Dpb->dpb_param.l.data[i+ii] =
					p[i+3-ii];
			}
		}
#endif
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
			"current dpb index %d, poc %d, top/bot poc (%d,%d)\n",
			p_H264_Dpb->dpb_param.dpb.current_dpb_index,
			val(p_H264_Dpb->dpb_param.dpb.frame_pic_order_cnt),
			val(p_H264_Dpb->dpb_param.dpb.top_field_pic_order_cnt),
			val(p_H264_Dpb->dpb_param.dpb.top_field_pic_order_cnt));

		slice_header_process_status =
			h264_slice_header_process(p_H264_Dpb);

		if (p_H264_Dpb->mVideo.dec_picture) {
			if (slice_header_process_status == 1) {
				dpb_print(hw->dpb.decoder_index,
				PRINT_FLAG_UCODE_EVT,
				"==================> frame count %d\n",
				hw->decode_pic_count+1);
			}
			config_decode_buf(hw, p_H264_Dpb->mVideo.dec_picture);
		}
		if (slice_header_process_status == 1)
			WRITE_VREG(DPB_STATUS_REG, H264_ACTION_DECODE_NEWPIC);
		else
			WRITE_VREG(DPB_STATUS_REG, H264_ACTION_DECODE_SLICE);
		hw->last_mby_mbx = 0;
		hw->last_ucode_watchdog_reg_val = 0;
		hw->decode_timeout_count = 2;
	} else if (dec_dpb_status == H264_PIC_DATA_DONE) {
		if (p_H264_Dpb->mVideo.dec_picture) {
			if (hw->chunk) {
				p_H264_Dpb->mVideo.dec_picture->pts =
					hw->chunk->pts;
				p_H264_Dpb->mVideo.dec_picture->pts64 =
					hw->chunk->pts64;
			} else {
				struct StorablePicture *pic =
					p_H264_Dpb->mVideo.dec_picture;
				u32 offset = pic->offset_delimiter_lo |
					(pic->offset_delimiter_hi << 16);
				if (pts_lookup_offset_us64(PTS_TYPE_VIDEO,
					offset, &pic->pts, 0, &pic->pts64)) {
					pic->pts = 0;
					pic->pts64 = 0;
				}
			}
			store_picture_in_dpb(p_H264_Dpb,
				p_H264_Dpb->mVideo.dec_picture);

			bufmgr_post(p_H264_Dpb);

			p_H264_Dpb->mVideo.dec_picture = NULL;
			/* dump_dpb(&p_H264_Dpb->mDPB); */
		}

		if ((h264_debug_flag&ONLY_RESET_AT_START) == 0)
			amvdec_stop();
		hw->decode_pic_count++,
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_STATUS,
			"%s H264_PIC_DATA_DONE decode slice count %d\n",
			__func__,
			hw->decode_pic_count);
		/* WRITE_VREG(DPB_STATUS_REG, H264_ACTION_SEARCH_HEAD); */
		hw->dec_result = DEC_RESULT_DONE;
		reset_process_time(hw);
		schedule_work(&hw->work);
	} else if (/*(dec_dpb_status == H264_DATA_REQUEST) ||*/
			(dec_dpb_status == H264_SEARCH_BUFEMPTY) ||
			(dec_dpb_status == H264_DECODE_BUFEMPTY) ||
			(dec_dpb_status == H264_DECODE_TIMEOUT)) {
empty_proc:
		if (p_H264_Dpb->mVideo.dec_picture) {
			remove_picture(p_H264_Dpb,
				p_H264_Dpb->mVideo.dec_picture);
			p_H264_Dpb->mVideo.dec_picture = NULL;
		}

		if (input_frame_based(vdec) ||
			(READ_VREG(VLD_MEM_VIFIFO_LEVEL) > 0x200)) {
			if (h264_debug_flag &
				DISABLE_ERROR_HANDLE) {
				dpb_print(hw->dpb.decoder_index,
				PRINT_FLAG_ERROR,
					"%s decoding error, level 0x%x\n",
					__func__,
					READ_VREG(VLD_MEM_VIFIFO_LEVEL));
				goto send_again;
			}
			if ((h264_debug_flag & ONLY_RESET_AT_START) == 0)
				amvdec_stop();
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_UCODE_EVT,
				"%s %s\n", __func__,
				(dec_dpb_status == H264_SEARCH_BUFEMPTY) ?
				"H264_SEARCH_BUFEMPTY" :
				(dec_dpb_status == H264_DECODE_BUFEMPTY) ?
				"H264_DECODE_BUFEMPTY" : "H264_DECODE_TIMEOUT");
			hw->dec_result = DEC_RESULT_DONE;

			if (dec_dpb_status == H264_SEARCH_BUFEMPTY)
				hw->search_dataempty_num++;
			else if (dec_dpb_status == H264_DECODE_TIMEOUT)
				hw->decode_timeout_num++;
			else if (dec_dpb_status == H264_DECODE_BUFEMPTY)
				hw->decode_dataempty_num++;


			reset_process_time(hw);
			schedule_work(&hw->work);
		} else {
			/* WRITE_VREG(DPB_STATUS_REG, H264_ACTION_INIT); */
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_AGAIN\n", __func__);
send_again:
			hw->dec_result = DEC_RESULT_AGAIN;
			schedule_work(&hw->work);
		}
	} else if (dec_dpb_status == H264_DATA_REQUEST) {
		if (input_frame_based(vdec)) {
			dpb_print(hw->dpb.decoder_index,
			PRINT_FLAG_VDEC_STATUS,
			"%s H264_DATA_REQUEST\n", __func__);
			hw->dec_result = DEC_RESULT_GET_DATA;
			schedule_work(&hw->work);
		} else
			goto empty_proc;
	}

	/* ucode debug */
	if (READ_VREG(DEBUG_REG1) & 0x10000) {
		int i;
		unsigned short *p = (unsigned short *)hw->lmem_addr;

		dma_sync_single_for_cpu(
			amports_get_dma_device(),
			hw->lmem_addr_remap,
			PAGE_SIZE,
			DMA_FROM_DEVICE);

		pr_info("LMEM<tag %x>:\n", READ_VREG(DEBUG_REG1));
		for (i = 0; i < 0x400; i += 4) {
			int ii;
			if ((i & 0xf) == 0)
				pr_info("%03x: ", i);
			for (ii = 0; ii < 4; ii++)
				pr_info("%04x ", p[i+3-ii]);
			if (((i+ii) & 0xf) == 0)
				pr_info("\n");
		}
		WRITE_VREG(DEBUG_REG1, 0);
	} else if (READ_VREG(DEBUG_REG1) != 0) {
		pr_info("dbg%x: %x\n", READ_VREG(DEBUG_REG1),
			READ_VREG(DEBUG_REG2));
		WRITE_VREG(DEBUG_REG1, 0);
	}
	/**/

	return IRQ_HANDLED;
}

static void timeout_process(struct vdec_h264_hw_s *hw)
{
	hw->timeout_num++;
	if ((h264_debug_flag & ONLY_RESET_AT_START) == 0)
		amvdec_stop();
	dpb_print(hw->dpb.decoder_index,
		PRINT_FLAG_ERROR, "%s decoder timeout\n", __func__);
	hw->dec_result = DEC_RESULT_DONE;
	reset_process_time(hw);
	schedule_work(&hw->work);
}

static void check_timer_func(unsigned long arg)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)arg;

	if ((h264_debug_cmd & 0x100) != 0 &&
		hw_to_vdec(hw)->id == (h264_debug_cmd & 0xff)) {
		hw->dec_result = DEC_RESULT_DONE;
		schedule_work(&hw->work);
		pr_info("vdec %d is forced to be disconnected\n",
			h264_debug_cmd & 0xff);
		h264_debug_cmd = 0;
		return;
	}

	if (hw_to_vdec(hw)->next_status == VDEC_STATUS_DISCONNECTED) {
		hw->dec_result = DEC_RESULT_DONE;
		schedule_work(&hw->work);
		pr_info("vdec requested to be disconnected\n");
		return;
	}

	if (radr != 0) {
		if (rval != 0) {
			WRITE_VREG(radr, rval);
			pr_info("WRITE_VREG(%x,%x)\n", radr, rval);
		} else
			pr_info("READ_VREG(%x)=%x\n", radr, READ_VREG(radr));
		rval = 0;
		radr = 0;
	}

	if ((input_frame_based(hw_to_vdec(hw)) ||
		(READ_VREG(VLD_MEM_VIFIFO_LEVEL) > 0x200)) &&
		((h264_debug_flag & DISABLE_ERROR_HANDLE) == 0) &&
		(decode_timeout_val > 0) &&
		(hw->start_process_time > 0) &&
		((1000 * (jiffies - hw->start_process_time) / HZ)
			> decode_timeout_val)
	) {
		u32 dpb_status = READ_VREG(DPB_STATUS_REG);
		u32 mby_mbx = READ_VREG(MBY_MBX);
		if ((dpb_status == H264_ACTION_DECODE_NEWPIC) ||
			(dpb_status == H264_ACTION_DECODE_SLICE)) {
			if (hw->last_mby_mbx == mby_mbx) {
				if (hw->decode_timeout_count > 0)
					hw->decode_timeout_count--;
				if (hw->decode_timeout_count == 0)
					timeout_process(hw);
			}
		} else if (is_in_parsing_state(dpb_status)) {
			if (hw->last_ucode_watchdog_reg_val ==
				READ_VREG(UCODE_WATCHDOG_REG)) {
				if (hw->decode_timeout_count > 0)
					hw->decode_timeout_count--;
				if (hw->decode_timeout_count == 0)
					timeout_process(hw);
			}
		}
		hw->last_ucode_watchdog_reg_val =
			READ_VREG(UCODE_WATCHDOG_REG);
		hw->last_mby_mbx = mby_mbx;
	}

	hw->check_timer.expires = jiffies + CHECK_INTERVAL;

	add_timer(&hw->check_timer);
}

static int dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	vstatus->width = hw->frame_width;
	vstatus->height = hw->frame_height;
	if (hw->frame_dur != 0)
		vstatus->fps = 96000 / hw->frame_dur;
	else
		vstatus->fps = -1;
	vstatus->error_count = READ_VREG(AV_SCRATCH_D);
	vstatus->status = hw->stat;
	/* if (fatal_error_reset)
		vstatus->status |= hw->fatal_error_flag; */
	return 0;
}

static int vh264_hw_ctx_restore(struct vdec_h264_hw_s *hw)
{
	int i;

	/* if (hw->init_flag == 0) { */
	if (h264_debug_flag & 0x40000000) {
		/* if (1) */
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_STATUS,
		"%s, reset register\n", __func__);

		while (READ_VREG(DCAC_DMA_CTRL) & 0x8000)
			;
		while (READ_VREG(LMEM_DMA_CTRL) & 0x8000)
			;    /* reg address is 0x350 */

#if 1 /* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		WRITE_VREG(DOS_SW_RESET0, (1<<7) | (1<<6) | (1<<4));
		WRITE_VREG(DOS_SW_RESET0, 0);

		READ_VREG(DOS_SW_RESET0);
		READ_VREG(DOS_SW_RESET0);
		READ_VREG(DOS_SW_RESET0);

		WRITE_VREG(DOS_SW_RESET0, (1<<7) | (1<<6) | (1<<4));
		WRITE_VREG(DOS_SW_RESET0, 0);

		WRITE_VREG(DOS_SW_RESET0, (1<<9) | (1<<8));
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
		WRITE_VREG(POWER_CTL_VLD,
			READ_VREG(POWER_CTL_VLD) | (0 << 10) |
				(1 << 9) | (1 << 6));
	} else {
		/* WRITE_VREG(POWER_CTL_VLD,
			READ_VREG(POWER_CTL_VLD) | (0 << 10) | (1 << 9) ); */
		WRITE_VREG(POWER_CTL_VLD,
			READ_VREG(POWER_CTL_VLD) |
				(0 << 10) | (1 << 9) | (1 << 6));
	}
	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);

	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);

#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1<<17);
#endif

#if 1 /* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	/* pr_info("vh264 meson8 prot init\n"); */
	WRITE_VREG(MDEC_PIC_DC_THRESH, 0x404038aa);
#endif
	if (hw->decode_pic_count > 0) {
		WRITE_VREG(AV_SCRATCH_7, (hw->max_reference_size << 24) |
			(hw->buffer_spec_num << 16) |
			(hw->buffer_spec_num << 8));
		for (i = 0; i < hw->buffer_spec_num; i++) {
			canvas_config(hw->buffer_spec[i].y_canvas_index,
				hw->buffer_spec[i].y_addr,
				hw->mb_width << 4,
				hw->mb_height << 4,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_32X32);

			canvas_config(hw->buffer_spec[i].u_canvas_index,
				hw->buffer_spec[i].u_addr,
				hw->mb_width << 4,
				hw->mb_height << 3,
				CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_32X32);

			WRITE_VREG(ANC0_CANVAS_ADDR + i,
				spec2canvas(&hw->buffer_spec[i]));
		}
	} else {
		WRITE_VREG(AV_SCRATCH_0, 0);
		WRITE_VREG(AV_SCRATCH_9, 0);
	}

	if (hw->init_flag == 0)
		WRITE_VREG(DPB_STATUS_REG, 0);
	else
		WRITE_VREG(DPB_STATUS_REG, H264_ACTION_DECODE_START);

	WRITE_VREG(FRAME_COUNTER_REG, hw->decode_pic_count);
	WRITE_VREG(AV_SCRATCH_8, hw->buf_offset);
	WRITE_VREG(AV_SCRATCH_G, hw->mc_dma_handle);

	/* hw->error_recovery_mode = (error_recovery_mode != 0) ?
		error_recovery_mode : error_recovery_mode_in; */
	/* WRITE_VREG(AV_SCRATCH_F,
		(READ_VREG(AV_SCRATCH_F) & 0xffffffc3) ); */
	WRITE_VREG(AV_SCRATCH_F, (READ_VREG(AV_SCRATCH_F) & 0xffffffc3) |
		((error_recovery_mode_in & 0x1) << 4));
	if (hw->ucode_type == UCODE_IP_ONLY_PARAM)
		SET_VREG_MASK(AV_SCRATCH_F, 1 << 6);
	else
		CLEAR_VREG_MASK(AV_SCRATCH_F, 1 << 6);

	WRITE_VREG(LMEM_DUMP_ADR, (u32)hw->lmem_addr_remap);
#if 1 /* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	WRITE_VREG(MDEC_PIC_DC_THRESH, 0x404038aa);
#endif

	WRITE_VREG(DEBUG_REG1, 0);
	WRITE_VREG(DEBUG_REG2, 0);
	return 0;
}

static unsigned char amvdec_enable_flag;
static void vh264_local_init(struct vdec_h264_hw_s *hw)
{
	int i;
	hw->decode_timeout_count = 0;

	hw->vh264_ratio = hw->vh264_amstream_dec_info.ratio;
	/* vh264_ratio = 0x100; */

	hw->vh264_rotation = (((unsigned long)
			hw->vh264_amstream_dec_info.param) >> 16) & 0xffff;

	hw->frame_prog = 0;
	hw->frame_width = hw->vh264_amstream_dec_info.width;
	hw->frame_height = hw->vh264_amstream_dec_info.height;
	hw->frame_dur = hw->vh264_amstream_dec_info.rate;
	hw->pts_outside = ((unsigned long)
			hw->vh264_amstream_dec_info.param) & 0x01;
	hw->sync_outside = ((unsigned long)
			hw->vh264_amstream_dec_info.param & 0x02) >> 1;
	hw->use_idr_framerate = ((unsigned long)
			hw->vh264_amstream_dec_info.param & 0x04) >> 2;
	hw->max_refer_buf = !(((unsigned long)
			hw->vh264_amstream_dec_info.param & 0x10) >> 4);
	if (hw->frame_dur < 96000/960) {
		/*more than 960fps,it should not be a correct value,
		give default 30fps*/
		hw->frame_dur = 96000/30;
	}

	pr_info
	("H264 sysinfo: %dx%d duration=%d, pts_outside=%d, ",
	 hw->frame_width, hw->frame_height, hw->frame_dur, hw->pts_outside);
	pr_info("sync_outside=%d, use_idr_framerate=%d\n",
	 hw->sync_outside, hw->use_idr_framerate);

	if ((unsigned long) hw->vh264_amstream_dec_info.param & 0x08)
		hw->ucode_type = UCODE_IP_ONLY_PARAM;
	else
		hw->ucode_type = 0;

	if ((unsigned long) hw->vh264_amstream_dec_info.param & 0x20)
		error_recovery_mode_in = 1;
	else
		error_recovery_mode_in = 3;

	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hw->vfpool[i];
		hw->vfpool[i].index = -1; /* VF_BUF_NUM; */
		hw->vfpool[i].bufWidth = 1920;
		kfifo_put(&hw->newframe_q, vf);
	}

	hw->duration_from_pts_done = 0;

	hw->p_last_vf = NULL;
	hw->fatal_error_flag = 0;
	hw->vh264_stream_switching_state = SWITCHING_STATE_OFF;

	INIT_WORK(&hw->work, vh264_work);

	return;
}

static s32 vh264_init(struct vdec_h264_hw_s *hw)
{
	/* int trickmode_fffb = 0; */
	int firmwareloaded = 0;

	hw->init_flag = 0;
	hw->set_params_done = 0;
	hw->start_process_time = 0;

	/* pr_info("\nvh264_init\n"); */
	/* init_timer(&hw->recycle_timer); */

	/* timer init */
	init_timer(&hw->check_timer);

	hw->check_timer.data = (unsigned long)hw;
	hw->check_timer.function = check_timer_func;
	hw->check_timer.expires = jiffies + CHECK_INTERVAL;

	/* add_timer(&hw->check_timer); */
	hw->stat |= STAT_TIMER_ARM;

	hw->duration_on_correcting = 0;
	hw->fixed_frame_rate_check_count = 0;
	hw->saved_resolution = 0;

	vh264_local_init(hw);

	if (!amvdec_enable_flag) {
		amvdec_enable_flag = true;
		amvdec_enable();
	}

	/* -- ucode loading (amrisc and swap code) */
	hw->mc_cpu_addr =
		dma_alloc_coherent(amports_get_dma_device(), MC_TOTAL_SIZE,
				&hw->mc_dma_handle, GFP_KERNEL);
	if (!hw->mc_cpu_addr) {
		amvdec_enable_flag = false;
		amvdec_disable();

		pr_info("vh264_init: Can not allocate mc memory.\n");
		return -ENOMEM;
	}

	pr_info("264 ucode swap area: phyaddr %p, cpu vaddr %p\n",
		(void *)hw->mc_dma_handle, hw->mc_cpu_addr);
	if (!firmwareloaded) {
		int r0 , r1 , r2 , r3 , r4 , r5, r6, r7, r8;
		pr_info("start load orignal firmware ...\n");
		/* r0 = amvdec_loadmc_ex(VFORMAT_H264, "vmh264_mc", NULL); */
		r0 = 0;

		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_HEADER, vh264_header_mc,
			   MC_SWAP_SIZE);*/
		r1 = get_decoder_firmware_data(VFORMAT_H264, "vmh264_header_mc",
			(u8 *) hw->mc_cpu_addr + MC_OFFSET_HEADER,
			MC_SWAP_SIZE);
		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_DATA, vh264_data_mc,
			   MC_SWAP_SIZE);
		*/
		r2 = get_decoder_firmware_data(VFORMAT_H264, "vmh264_data_mc",
			(u8 *) hw->mc_cpu_addr + MC_OFFSET_DATA,
			MC_SWAP_SIZE);
		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_MMCO, vh264_mmco_mc,
			   MC_SWAP_SIZE);
		*/
		r3 = get_decoder_firmware_data(VFORMAT_H264, "vmh264_mmco_mc",
			(u8 *) hw->mc_cpu_addr + MC_OFFSET_MMCO,
			MC_SWAP_SIZE);
		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_LIST, vmh264_list_mc,
			   MC_SWAP_SIZE);
		*/
		r4 = get_decoder_firmware_data(VFORMAT_H264, "vmh264_list_mc",
			(u8 *) hw->mc_cpu_addr + MC_OFFSET_LIST,
			MC_SWAP_SIZE);
		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_SLICE, vh264_slice_mc,
			   MC_SWAP_SIZE);
		*/
		r5 = get_decoder_firmware_data(VFORMAT_H264, "vmh264_slice_mc",
			(u8 *) hw->mc_cpu_addr + MC_OFFSET_SLICE,
			MC_SWAP_SIZE);
		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_MAIN, mbuf,
		0x800*4)
		*/
		r6 = get_decoder_firmware_data(VFORMAT_H264, "vmh264_mc",
			(u8 *) hw->mc_cpu_addr + MC_OFFSET_MAIN,
			0x800 * 4);
		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_MAIN + 0x800*4,
		pvh264_data_mc, 0x400*4);
		*/
		r7 = get_decoder_firmware_data(VFORMAT_H264, "vmh264_data_mc",
			(u8 *) hw->mc_cpu_addr + MC_OFFSET_MAIN + 0x800 * 4,
			0x400 * 4);
		/*memcpy((u8 *) mc_cpu_addr + MC_OFFSET_MAIN + 0x800*4 +
		0x400*4, pvh264_list_mc, 0x400*4);
		*/
		r8 = get_decoder_firmware_data(VFORMAT_H264, "vmh264_list_mc",
			(u8 *) hw->mc_cpu_addr + MC_OFFSET_MAIN + 0x800 * 4
			+ 0x400 * 4, 0x400 * 4);

		if (r0 < 0 || r1 < 0 || r2 < 0 || r3 < 0 || r4 < 0 ||
			r5 < 0 || r6 < 0 || r7 < 0 || r8 < 0) {
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_ERROR,
			"264 load orignal firmware error %d,%d,%d,%d,%d,%d,%d,%d,%d\n",
				r0 , r1 , r2 , r3 , r4 , r5, r6, r7, r8);
			amvdec_disable();
			if (hw->mc_cpu_addr) {
				dma_free_coherent(amports_get_dma_device(),
					MC_TOTAL_SIZE, hw->mc_cpu_addr,
					hw->mc_dma_handle);
				hw->mc_cpu_addr = NULL;
			}
			return -EBUSY;
		}
	}

#if 1 /* #ifdef  BUFFER_MGR_IN_C */
	dpb_init_global(&hw->dpb,
		hw_to_vdec(hw)->id, 0, 0);
	hw->lmem_addr = __get_free_page(GFP_KERNEL);
	if (!hw->lmem_addr) {
		pr_info("%s: failed to alloc lmem_addr\n", __func__);
		return -ENOMEM;
	} else {
		hw->lmem_addr_remap = dma_map_single(
				amports_get_dma_device(),
				(void *)hw->lmem_addr,
				PAGE_SIZE, DMA_FROM_DEVICE);
		if (dma_mapping_error(amports_get_dma_device(),
			hw->lmem_addr_remap)) {
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_ERROR,
			"%s: failed to map lmem_addr\n", __func__);
			free_page(hw->lmem_addr);
			hw->lmem_addr = 0;
			hw->lmem_addr_remap = 0;
			return -ENOMEM;
		}

		pr_info("%s, vaddr=%lx phy_addr=%p\n",
			__func__, hw->lmem_addr, (void *)hw->lmem_addr_remap);
	}
	/* BUFFER_MGR_IN_C */
#endif
	hw->stat |= STAT_MC_LOAD;

	/* add memory barrier */
	wmb();

	return 0;
}

static int vh264_stop(struct vdec_h264_hw_s *hw, int mode)
{
	int i;

	if (hw->stat & STAT_MC_LOAD) {
		if (hw->mc_cpu_addr != NULL) {
			dma_free_coherent(amports_get_dma_device(),
					MC_TOTAL_SIZE, hw->mc_cpu_addr,
					hw->mc_dma_handle);
			hw->mc_cpu_addr = NULL;
		}
	}
#ifdef USE_CMA
	if (hw->cma_alloc_addr) {
		pr_info("codec_mm release buffer 0x%lx\n", hw->cma_alloc_addr);
		codec_mm_free_for_dma(MEM_NAME, hw->cma_alloc_addr);
		 hw->cma_alloc_count = 0;
	}

	if (hw->collocate_cma_alloc_addr) {
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_ERROR,
			"codec_mm release collocate buffer 0x%lx\n",
			hw->collocate_cma_alloc_addr);
		codec_mm_free_for_dma(MEM_NAME, hw->collocate_cma_alloc_addr);
		hw->collocate_cma_alloc_count = 0;
	}

	for (i = 0; i < hw->buffer_spec_num; i++) {
		if (hw->buffer_spec[i].cma_alloc_addr) {
			pr_info("codec_mm release buffer_spec[%d], 0x%lx\n", i,
				hw->buffer_spec[i].cma_alloc_addr);
			codec_mm_free_for_dma(MEM_NAME,
				hw->buffer_spec[i].cma_alloc_addr);
			hw->buffer_spec[i].cma_alloc_count = 0;
		}
	}

#endif

	if (hw->lmem_addr_remap) {
		dma_unmap_single(amports_get_dma_device(),
			hw->lmem_addr_remap,
			PAGE_SIZE, DMA_FROM_DEVICE);
		hw->lmem_addr_remap = 0;
	}
	if (hw->lmem_addr) {
		free_page(hw->lmem_addr);
		hw->lmem_addr = 0;
	}
	cancel_work_sync(&hw->work);

	/* amvdec_disable(); */

	return 0;
}

static void vh264_work(struct work_struct *work)
{
	struct vdec_h264_hw_s *hw = container_of(work,
		struct vdec_h264_hw_s, work);
	struct vdec_s *vdec = hw_to_vdec(hw);

	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */
	dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_DETAIL,
		"%s  %x %x %x\n", __func__,
		READ_VREG(0xc47), READ_VREG(0xc45), READ_VREG(0xc46));

#ifdef USE_CMA
	if (hw->dec_result == DEC_RESULT_CONFIG_PARAM) {
		if (vh264_set_params(hw) < 0) {
			hw->fatal_error_flag = DECODER_FATAL_ERROR_UNKNOW;
			if (!hw->fatal_error_reset)
				schedule_work(&hw->error_wd_work);
		}
		return;
	} else
#endif
	if ((hw->dec_result == DEC_RESULT_GET_DATA) ||
		(hw->dec_result == DEC_RESULT_GET_DATA_RETRY)) {
		if (hw->dec_result == DEC_RESULT_GET_DATA) {
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_GET_DATA %x %x %x\n",
				__func__,
				READ_VREG(VLD_MEM_VIFIFO_LEVEL),
				READ_VREG(VLD_MEM_VIFIFO_WP),
				READ_VREG(VLD_MEM_VIFIFO_RP));
			vdec_vframe_dirty(vdec, hw->chunk);
			vdec_clean_input(vdec);
		}

		if (is_buffer_available(vdec)) {
			int r;
			r = vdec_prepare_input(vdec, &hw->chunk);
			if (r < 0) {
				hw->dec_result = DEC_RESULT_GET_DATA_RETRY;

				dpb_print(hw->dpb.decoder_index,
					PRINT_FLAG_VDEC_DETAIL,
					"ammvdec_vh264: Insufficient data\n");

				schedule_work(&hw->work);
				return;
			}
			hw->dec_result = DEC_RESULT_NONE;
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_STATUS,
				"%s: chunk size 0x%x\n",
				__func__, hw->chunk->size);
			WRITE_VREG(POWER_CTL_VLD,
				READ_VREG(POWER_CTL_VLD) |
					(0 << 10) | (1 << 9) | (1 << 6));
			WRITE_VREG(H264_DECODE_INFO, (1<<13));
			WRITE_VREG(H264_DECODE_SIZE, hw->chunk->size);
			WRITE_VREG(VIFF_BIT_CNT, (hw->chunk->size * 8));
			vdec_enable_input(vdec);

			WRITE_VREG(DPB_STATUS_REG, H264_ACTION_SEARCH_HEAD);
		} else{
			hw->dec_result = DEC_RESULT_GET_DATA_RETRY;

			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_DETAIL,
				"ammvdec_vh264: Insufficient data\n");

			schedule_work(&hw->work);
		}
		return;
	} else if (hw->dec_result == DEC_RESULT_DONE) {
		/* if (!hw->ctx_valid)
			hw->ctx_valid = 1; */
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_STATUS,
			"%s dec_result %d %x %x %x\n",
			__func__,
			hw->dec_result,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP));
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
	} else {
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_DETAIL,
			"%s dec_result %d %x %x %x\n",
			__func__,
			hw->dec_result,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP));
	}

	del_timer_sync(&hw->check_timer);
	hw->stat &= ~STAT_TIMER_ARM;

	/* mark itself has all HW resource released and input released */
	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_CONNECTED);

	if (hw->vdec_cb)
		hw->vdec_cb(hw_to_vdec(hw), hw->vdec_cb_arg);
}

static bool run_ready(struct vdec_s *vdec)
{
	if (vdec->master)
		return false;

	if ((!input_frame_based(vdec)) && (start_decode_buf_level > 0)) {
		u32 rp, wp;
		u32 level;

		rp = READ_MPEG_REG(PARSER_VIDEO_RP);
		wp = READ_MPEG_REG(PARSER_VIDEO_WP);

		if (wp < rp)
			level = vdec->input.size + wp - rp;
		else
			level = wp - rp;

		if (level < start_decode_buf_level) {
			struct vdec_h264_hw_s *hw =
				(struct vdec_h264_hw_s *)vdec->private;
			dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_DETAIL,
				"%s vififo level low %x(<%x) (lev%x wp%x rp%x)\n",
				__func__, level,
				start_decode_buf_level,
				READ_VREG(VLD_MEM_VIFIFO_LEVEL),
				READ_VREG(VLD_MEM_VIFIFO_WP),
				READ_VREG(VLD_MEM_VIFIFO_RP));
			return 0;
		}
	}

	if (h264_debug_flag & 0x20000000) {
		/* pr_info("%s, a\n", __func__); */
		return 1;
	} else {
		return is_buffer_available(vdec);
	}
}

static void run(struct vdec_s *vdec,
	void (*callback)(struct vdec_s *, void *), void *arg)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)vdec->private;
	int size;

	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;

	/* hw->chunk = vdec_prepare_input(vdec); */
	size = vdec_prepare_input(vdec, &hw->chunk);
	if ((size < 0) ||
		(input_frame_based(vdec) && hw->chunk == NULL)) {
		hw->dec_result = DEC_RESULT_AGAIN;

		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_DETAIL,
			"ammvdec_vh264: Insufficient data\n");

		schedule_work(&hw->work);
		return;
	}

	hw->dec_result = DEC_RESULT_NONE;
#if 0
	if ((!input_frame_based(vdec)) && (start_decode_buf_level > 0)) {
		if (READ_VREG(VLD_MEM_VIFIFO_LEVEL) <
			start_decode_buf_level) {
			dpb_print(hw->dpb.decoder_index,
				PRINT_FLAG_VDEC_DETAIL,
				"%s: VIFIFO_LEVEL %x is low (<%x)\n",
				__func__,
				READ_VREG(VLD_MEM_VIFIFO_LEVEL),
				start_decode_buf_level);

			hw->dec_result = DEC_RESULT_AGAIN;
			schedule_work(&hw->work);
			return;
		}
	}
#endif

	if (input_frame_based(vdec)) {
		u8 *data = ((u8 *)hw->chunk->block->start_virt) +
			hw->chunk->offset;

		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_STATUS,
			"%s: size 0x%x %02x %02x %02x %02x %02x %02x .. %02x %02x %02x %02x\n",
			__func__, size,
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[size - 4],
			data[size - 3],	data[size - 2],
			data[size - 1]);
	} else
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_VDEC_STATUS,
			"%s: %x %x %x size 0x%x\n",
			__func__,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP), size);

	hw->start_process_time = jiffies;

	if (((h264_debug_flag & ONLY_RESET_AT_START) == 0) ||
		(hw->init_flag == 0)) {
		if (amvdec_vdec_loadmc_ex(vdec, "vmh264_mc") < 0) {
			amvdec_enable_flag = false;
			amvdec_disable();
			pr_info("%s: Error amvdec_vdec_loadmc fail\n",
				__func__);
			return;
		}

		if (vh264_hw_ctx_restore(hw) < 0) {
			schedule_work(&hw->work);
			return;
		}
		if (input_frame_based(vdec)) {
			WRITE_VREG(H264_DECODE_INFO, (1<<13));
			WRITE_VREG(H264_DECODE_SIZE, hw->chunk->size);
			WRITE_VREG(VIFF_BIT_CNT, (hw->chunk->size * 8));
		} else {
			if (size <= 0)
				size = 0x7fffffff; /*error happen*/
			WRITE_VREG(H264_DECODE_INFO, (1<<13));
			WRITE_VREG(H264_DECODE_SIZE, size);
			WRITE_VREG(VIFF_BIT_CNT, size * 8);
		}

		vdec_enable_input(vdec);

		add_timer(&hw->check_timer);

		amvdec_start();

		/* if (hw->init_flag) { */
			WRITE_VREG(DPB_STATUS_REG, H264_ACTION_SEARCH_HEAD);
		/* } */

		hw->init_flag = 1;
	} else {
		WRITE_VREG(H264_DECODE_INFO, (1 << 13));
		vdec_enable_input(vdec);

		WRITE_VREG(DPB_STATUS_REG, H264_ACTION_SEARCH_HEAD);
	}
}

static void reset(struct vdec_s *vdec)
{
	pr_info("ammvdec_h264: reset.\n");

#if 0
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;

	hw->init_flag = 0;
	hw->set_params_done = 0;

	vh264_local_init(hw);

	dpb_init_global(&hw->dpb);
#endif
}

static int ammvdec_h264_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_h264_hw_s *hw = NULL;

	if (pdata == NULL) {
		pr_info("\nammvdec_h264 memory resource undefined.\n");
		return -EFAULT;
	}

	hw = (struct vdec_h264_hw_s *)devm_kzalloc(&pdev->dev,
		sizeof(struct vdec_h264_hw_s), GFP_KERNEL);
	if (hw == NULL) {
		pr_info("\nammvdec_h264 device data allocation failed\n");
		return -ENOMEM;
	}

	pdata->private = hw;
	pdata->dec_status = dec_status;
	/* pdata->set_trickmode = set_trickmode; */
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vh264_isr;

	pdata->id = pdev->id;

	if (pdata->use_vfm_path)
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
	else if (vdec_dual(pdata)) {
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			(pdata->master) ? VFM_DEC_DVEL_PROVIDER_NAME :
			VFM_DEC_DVBL_PROVIDER_NAME);
	} else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			PROVIDER_NAME ".%02x", pdev->id & 0xff);

	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vf_provider_ops, pdata);

	platform_set_drvdata(pdev, pdata);

	hw->platform_dev = pdev;
#ifndef USE_CMA
	hw->buf_start = pdata->mem_start;
	hw->buf_size = pdata->mem_end - pdata->mem_start + 1;
	/* hw->ucode_map_start = pdata->mem_start; */
	if (hw->buf_size < DEFAULT_MEM_SIZE) {
		pr_info("\nammvdec_h264 memory size not enough.\n");
		return -ENOMEM;
	}
#endif

#ifdef USE_CMA
	hw->cma_dev = pdata->cma_dev;
	if (hw->cma_alloc_count == 0) {
		hw->cma_alloc_count = PAGE_ALIGN(V_BUF_ADDR_OFFSET) / PAGE_SIZE;
		hw->cma_alloc_addr = codec_mm_alloc_for_dma(MEM_NAME,
					hw->cma_alloc_count,
					4, CODEC_MM_FLAGS_FOR_VDECODER);
	}
	if (!hw->cma_alloc_addr) {
		dpb_print(hw->dpb.decoder_index, PRINT_FLAG_ERROR,
			"codec_mm alloc failed, request buf size 0x%lx\n",
				hw->cma_alloc_count * PAGE_SIZE);
		hw->cma_alloc_count = 0;
		return -ENOMEM;
	}
	hw->buf_offset = hw->cma_alloc_addr - DEF_BUF_START_ADDR;
#else
	hw->buf_offset = pdata->mem_start - DEF_BUF_START_ADDR;
	hw->buf_start = V_BUF_ADDR_OFFSET + pdata->mem_start;
#endif

	if (pdata->sys_info)
		hw->vh264_amstream_dec_info = *pdata->sys_info;
	if (NULL == hw->sei_data_buffer) {
		hw->sei_data_buffer =
			dma_alloc_coherent(amports_get_dma_device(),
				USER_DATA_SIZE,
				&hw->sei_data_buffer_phys, GFP_KERNEL);
		if (!hw->sei_data_buffer) {
			pr_info("%s: Can not allocate sei_data_buffer\n",
				   __func__);
			return -ENOMEM;
		}
		/* pr_info("buffer 0x%x, phys 0x%x, remap 0x%x\n",
		   sei_data_buffer, sei_data_buffer_phys,
		   (u32)sei_data_buffer_remap); */
	}
#ifdef USE_CMA
	pr_info("ammvdec_h264 mem-addr=%lx,buff_offset=%x,buf_start=%lx\n",
		pdata->mem_start, hw->buf_offset, hw->cma_alloc_addr);
#else
	pr_info("ammvdec_h264 mem-addr=%lx,buff_offset=%x,buf_start=%lx\n",
		pdata->mem_start, hw->buf_offset, hw->buf_start);
#endif

	vdec_source_changed(VFORMAT_H264, 3840, 2160, 60);

	if (vh264_init(hw) < 0) {
		pr_info("\nammvdec_h264 init failed.\n");
		return -ENODEV;
	}
	atomic_set(&hw->vh264_active, 1);

	return 0;
}

static int ammvdec_h264_remove(struct platform_device *pdev)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);

	atomic_set(&hw->vh264_active, 0);

	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->check_timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}

	vh264_stop(hw, MODE_FULL);

	/* vdec_source_changed(VFORMAT_H264, 0, 0, 0); */

	atomic_set(&hw->vh264_active, 0);

	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_DISCONNECTED);

	return 0;
}

/****************************************/

static struct platform_driver ammvdec_h264_driver = {
	.probe = ammvdec_h264_probe,
	.remove = ammvdec_h264_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

static struct codec_profile_t ammvdec_h264_profile = {
	.name = "mh264",
	.profile = ""
};

static int __init ammvdec_h264_driver_init_module(void)
{
	pr_info("ammvdec_h264 module init\n");
	if (platform_driver_register(&ammvdec_h264_driver)) {
		pr_info("failed to register ammvdec_h264 driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&ammvdec_h264_profile);
	return 0;
}

static void __exit ammvdec_h264_driver_remove_module(void)
{
	pr_info("ammvdec_h264 module remove.\n");

	platform_driver_unregister(&ammvdec_h264_driver);
}

/****************************************/

module_param(h264_debug_flag, uint, 0664);
MODULE_PARM_DESC(h264_debug_flag, "\n ammvdec_h264 h264_debug_flag\n");

module_param(start_decode_buf_level, uint, 0664);
MODULE_PARM_DESC(start_decode_buf_level,
		"\n ammvdec_h264 start_decode_buf_level\n");

module_param(fixed_frame_rate_mode, uint, 0664);
MODULE_PARM_DESC(fixed_frame_rate_mode, "\namvdec_h264 fixed_frame_rate_mode\n");

module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val, "\n amvdec_h264 decode_timeout_val\n");

module_param(reorder_dpb_size_margin, uint, 0664);
MODULE_PARM_DESC(reorder_dpb_size_margin, "\n ammvdec_h264 reorder_dpb_size_margin\n");

module_param(reference_buf_margin, uint, 0664);
MODULE_PARM_DESC(reference_buf_margin, "\n ammvdec_h264 reference_buf_margin\n");

module_param(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\nradr\n");

module_param(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\nrval\n");

module_param(h264_debug_mask, uint, 0664);
MODULE_PARM_DESC(h264_debug_mask, "\n amvdec_h264 h264_debug_mask\n");

module_param(h264_debug_cmd, uint, 0664);
MODULE_PARM_DESC(h264_debug_cmd, "\n amvdec_h264 h264_debug_cmd\n");

module_param(force_rate_streambase, int, 0664);
MODULE_PARM_DESC(force_rate_streambase, "\n amvdec_h264 force_rate_streambase\n");

module_param(dec_control, int, 0664);
MODULE_PARM_DESC(dec_control, "\n amvdec_h264 dec_control\n");

module_param(force_rate_framebase, int, 0664);
MODULE_PARM_DESC(force_rate_framebase, "\n amvdec_h264 force_rate_framebase\n");

/*
module_param(trigger_task, uint, 0664);
MODULE_PARM_DESC(trigger_task, "\n amvdec_h264 trigger_task\n");
*/
module_param_array(decode_frame_count, uint, &max_decode_instance_num, 0664);

module_param_array(max_process_time, uint, &max_decode_instance_num, 0664);

module_param_array(max_get_frame_interval, uint,
	&max_decode_instance_num, 0664);

module_param_array(step, uint, &max_decode_instance_num, 0664);

module_init(ammvdec_h264_driver_init_module);
module_exit(ammvdec_h264_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC H264 Video Decoder Driver");
MODULE_LICENSE("GPL");
