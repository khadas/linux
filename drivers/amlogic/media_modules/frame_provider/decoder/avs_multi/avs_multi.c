/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/tee.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "../../../stream_input/amports/amports_priv.h"
#include "../../../stream_input/amports/streambuf_reg.h"
#include "../utils/decoder_mmu_box.h"
#include "../utils/decoder_bmmu_box.h"
#include "../utils/firmware.h"
#include "../utils/vdec_feature.h"
#include "../utils/amvdec.h"
#include "avs_multi.h"

#define DEBUG_MULTI_FLAG  0

#define USE_DYNAMIC_BUF_NUM

#ifdef DEBUG_WITH_SINGLE_MODE
#define DRIVER_NAME "amvdec_avs"
#else
#define DRIVER_NAME "ammvdec_avs"
#endif

#define MULTI_DRIVER_NAME "ammvdec_avs"

#define ENABLE_USER_DATA

#define NV21

#define USE_AVS_SEQ_INFO
#define HANDLE_AVS_IRQ
#define DEBUG_PTS

#define CHECK_INTERVAL        (HZ/100)

#define I_PICTURE   0
#define P_PICTURE   1
#define B_PICTURE   2

#define LMEM_BUF_SIZE (0x500 * 2)

#define ORI_BUFFER_START_ADDR   0x80000000

#define INTERLACE_FLAG          0x80
#define TOP_FIELD_FIRST_FLAG 0x40

/* protocol registers */
#define AVS_PIC_RATIO       AV_SCRATCH_0
#define AVS_PIC_INFO      AV_SCRATCH_1
#define AVS_FRAME_RATE     AV_SCRATCH_3

/*#define AVS_ERROR_COUNT    AV_SCRATCH_6*/
#define AVS_SOS_COUNT     AV_SCRATCH_7
#define AVS_BUFFERIN       AV_SCRATCH_8
#define AVS_BUFFEROUT      AV_SCRATCH_9
#define AVS_REPEAT_COUNT    AV_SCRATCH_A
#define AVS_TIME_STAMP      AV_SCRATCH_B
#define AVS_OFFSET_REG      AV_SCRATCH_C
#define MEM_OFFSET_REG      AV_SCRATCH_F
#define AVS_ERROR_RECOVERY_MODE   AV_SCRATCH_G
#define DECODE_PIC_COUNT     AV_SCRATCH_G

#define DECODE_MODE		AV_SCRATCH_6
#define DECODE_MODE_SINGLE					0x0
#define DECODE_MODE_MULTI_FRAMEBASE			0x1
#define DECODE_MODE_MULTI_STREAMBASE		0x2
#define DECODE_MODE_MULTI_STREAMBASE_CONT   0x3

#define DECODE_STATUS	AV_SCRATCH_H
#define DECODE_STATUS_PIC_DONE    0x1
#define DECODE_STATUS_DECODE_BUF_EMPTY	0x2
#define DECODE_STATUS_SEARCH_BUF_EMPTY	0x3
#define DECODE_STATUS_SKIP_PIC_DONE     0x4
#define DECODE_STATUS_INFO     0x5
#define DECODE_SEARCH_HEAD	0xff

#define DECODE_STOP_POS		AV_SCRATCH_J

#define DECODE_LMEM_BUF_ADR   AV_SCRATCH_I

#define DECODE_CFG            AV_SCRATCH_K

#define VF_POOL_SIZE        64
#define PUT_INTERVAL        (HZ/100)

#define INT_AMVENCODER INT_DOS_MAILBOX_1

#ifdef USE_DYNAMIC_BUF_NUM
static unsigned int buf_spec_reg[] = {
	AV_SCRATCH_0,
	AV_SCRATCH_1,
	AV_SCRATCH_2,
	AV_SCRATCH_3,
	AV_SCRATCH_7, /*AVS_SOS_COUNT*/
	AV_SCRATCH_D, /*DEBUG_REG2*/
	AV_SCRATCH_E, /*DEBUG_REG1*/
	AV_SCRATCH_M  /*user_data_poc_number*/
};
#endif

#define DEBUG_REG1	AV_SCRATCH_E
#define DEBUG_REG2	AV_SCRATCH_D

static void check_timer_func(struct timer_list *timer);
static void vavs_work(struct work_struct *work);

#define DEC_CONTROL_FLAG_FORCE_2500_1080P_INTERLACE 0x0001
static u32 dec_control = DEC_CONTROL_FLAG_FORCE_2500_1080P_INTERLACE;

#define VPP_VD1_POSTBLEND       (1 << 10)

static int debug;
static unsigned int debug_mask = 0xff;

/*for debug*/
/*
	udebug_flag:
	bit 0, enable ucode print
	bit 1, enable ucode more print
	bit 3, enable ucdode detail print
	bit [31:16] not 0, pos to dump lmem
		bit 2, pop bits to lmem
		bit [11:8], pre-pop bits for alignment (when bit 2 is 1)

	avs only:
	bit [8], disable empty muitl-instance handling
	bit [9], enable writting of VC1_CONTROL_REG in ucode
*/
static u32 udebug_flag;
/*
	when udebug_flag[1:0] is not 0
	udebug_pause_pos not 0,
		pause position
*/
static u32 udebug_pause_pos;
/*
	when udebug_flag[1:0] is not 0
	and udebug_pause_pos is not 0,
		pause only when DEBUG_REG2 is equal to this val
*/
static u32 udebug_pause_val;

static u32 udebug_pause_decode_idx;

static u32 udebug_pause_ins_id;

static u32 force_fps;

#ifdef DEBUG_MULTI_FRAME_INS
static u32 delay;
#endif

static u32 step;

static u32 start_decoding_delay;

#define AVS_DEV_NUM        9
static unsigned int max_decode_instance_num = AVS_DEV_NUM;
static unsigned int max_process_time[AVS_DEV_NUM];
static unsigned int max_get_frame_interval[AVS_DEV_NUM];
static unsigned int run_count[AVS_DEV_NUM];
static unsigned int ins_udebug_flag[AVS_DEV_NUM];
#ifdef DEBUG_MULTI_FRAME_INS
static unsigned int max_run_count[AVS_DEV_NUM];
#endif
/*
error_handle_policy:
*/
static unsigned int error_handle_policy = 3;

static u32 again_threshold = 0; /*0x40;*/

static unsigned int decode_timeout_val = 200;
static unsigned int start_decode_buf_level = 0x8000;

/********************************
firmware_sel
    0: use avsp_trans long cabac ucode;
    1: not use avsp_trans long cabac ucode
		in ucode:
		#define USE_EXT_BUFFER_ASSIGNMENT
		#undef USE_DYNAMIC_BUF_NUM
********************************/
static int firmware_sel;
static int disable_longcabac_trans = 1;
static int pre_decode_buf_level = 0x800;

static struct vframe_s *vavs_vf_peek(void *);
static struct vframe_s *vavs_vf_get(void *);
static void vavs_vf_put(struct vframe_s *, void *);
static int vavs_vf_states(struct vframe_states *states, void *);
static int vavs_event_cb(int type, void *data, void *private_data);

static const char vavs_dec_id[] = "vavs-dev";

#define PROVIDER_NAME   "decoder.avs"
static DEFINE_SPINLOCK(lock);
static DEFINE_MUTEX(vavs_mutex);

static const struct vframe_operations_s vavs_vf_provider = {
	.peek = vavs_vf_peek,
	.get = vavs_vf_get,
	.put = vavs_vf_put,
	.event_cb = vavs_event_cb,
	.vf_states = vavs_vf_states,
};

static struct vframe_provider_s vavs_vf_prov;

#define VF_BUF_NUM_MAX 16
#ifdef DEBUG_MULTI_FRAME_INS
#define WORKSPACE_SIZE		(16 * SZ_1M)
#else
#define WORKSPACE_SIZE		(4 * SZ_1M)
#endif
#ifdef AVSP_LONG_CABAC
#define MAX_BMMU_BUFFER_NUM	(VF_BUF_NUM_MAX + 2)
#define WORKSPACE_SIZE_A		(MAX_CODED_FRAME_SIZE + LOCAL_HEAP_SIZE)
#else
#define MAX_BMMU_BUFFER_NUM	(VF_BUF_NUM_MAX + 1)
#endif

#define RV_AI_BUFF_START_ADDR	 0x01a00000
#define LONG_CABAC_RV_AI_BUFF_START_ADDR	 0x00000000

/* 4 buffers not enough for multi inc*/
static u32 vf_buf_num = 8;
/*static u32 vf_buf_num_used;*/
static u32 canvas_base = 128;
#ifdef NV21
static int	canvas_num = 2; /*NV21*/
#else
static int	canvas_num = 3;
#endif

static u32 buf_size = 32 * 1024 * 1024;

static u32 pts_by_offset = 1;
static u32 radr, rval;
static u32 dbg_cmd;

#ifdef AVSP_LONG_CABAC
static struct work_struct long_cabac_wd_work;
void *es_write_addr_virt;
dma_addr_t es_write_addr_phy;

void *bitstream_read_tmp;
dma_addr_t bitstream_read_tmp_phy;
void *avsp_heap_adr;
static uint long_cabac_busy;
#endif

static inline u32 index2canvas(u32 index)
{
	const u32 canvas_tab[VF_BUF_NUM_MAX] = {
		0x010100, 0x030302, 0x050504, 0x070706,
		0x090908, 0x0b0b0a, 0x0d0d0c, 0x0f0f0e,
		0x111110, 0x131312, 0x151514, 0x171716,
		0x191918, 0x1b1b1a, 0x1d1d1c, 0x1f1f1e,
	};
	const u32 canvas_tab_3[4] = {
		0x010100, 0x040403, 0x070706, 0x0a0a09
	};

	if (canvas_num == 2)
		return canvas_tab[index] + (canvas_base << 16)
		+ (canvas_base << 8) + canvas_base;

	return canvas_tab_3[index] + (canvas_base << 16)
		+ (canvas_base << 8) + canvas_base;
}

static const u32 frame_rate_tab[16] = {
	96000 / 30,		/* forbidden */
	96000000 / 23976,	/* 24000/1001 (23.967) */
	96000 / 24,
	96000 / 25,
	9600000 / 2997,		/* 30000/1001 (29.97) */
	96000 / 30,
	96000 / 50,
	9600000 / 5994,		/* 60000/1001 (59.94) */
	96000 / 60,
	/* > 8 reserved, use 24 */
	96000 / 24, 96000 / 24, 96000 / 24, 96000 / 24,
	96000 / 24, 96000 / 24, 96000 / 24
};

#define DECODE_BUFFER_NUM_MAX VF_BUF_NUM_MAX
#define PIC_PTS_NUM 64
struct buf_pool_s {
	unsigned detached;
	struct vframe_s vf;
};

#define buf_of_vf(vf) container_of(vf, struct buf_pool_s, vf)

struct pic_pts_s {
	u32 pts;
	u64 pts64;
	u64 timestamp;
	unsigned short decode_pic_count;
};

struct pic_info_t {
	u32 buffer_info;
	u32 index;
	u32 offset;
	u32 width;
	u32 height;
	u32 pts;
	u64 pts64;
	bool pts_valid;
	ulong v4l_ref_buf_addr;
	u32 hw_decode_time;
	u32 frame_size; // For frame base mode
	u64 timestamp;
	u32 picture_type;
	unsigned short decode_pic_count;
	u32 repeat_cnt;
};

struct vdec_avs_hw_s {
	spinlock_t lock;
	unsigned char m_ins_flag;
	struct platform_device *platform_dev;
	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(recycle_q, struct vframe_s *, VF_POOL_SIZE);
	struct buf_pool_s vfpool[VF_POOL_SIZE];
	s32 vfbuf_use[VF_BUF_NUM_MAX];
	unsigned char again_flag;
	unsigned char recover_flag;
	u32 frame_width;
	u32 frame_height;
	u32 frame_dur;
	u32 frame_prog;
	u32 saved_resolution;
	u32 avi_flag;
	u32 vavs_ratio;
	u32 pic_type;

	u32 vf_buf_num_used;
	u32 total_frame;
	u32 next_pts;
	unsigned char throw_pb_flag;
	struct pic_pts_s pic_pts[PIC_PTS_NUM];
	int pic_pts_wr_pos;

#ifdef DEBUG_PTS
	u32 pts_hit;
	u32 pts_missed;
	u32 pts_i_hit;
	u32 pts_i_missed;
#endif
#ifdef ENABLE_USER_DATA
	struct work_struct userdata_push_work;
	void *user_data_buffer;
	dma_addr_t user_data_buffer_phys;
#endif
	dma_addr_t lmem_addr;
	ulong lmem_phy_addr;

	u32 buf_offset;

	struct dec_sysinfo vavs_amstream_dec_info;
	struct vdec_info *gvs;
	u32 fr_hint_status;
	struct work_struct set_clk_work;
	bool is_reset;

	/*debug*/
	u32 ucode_pause_pos;
	u32 decode_pic_count;
	u8 reset_decode_flag;
	u32 display_frame_count;
	u32 buf_status;
	u32 pre_parser_wr_ptr;
		/*
		buffer_status &= ~buf_recycle_status
		*/
	u32 buf_recycle_status;
	u32 seqinfo;
	u32 ctx_valid;
	u32 dec_control;
	void *mm_blk_handle;
	struct vframe_chunk_s *chunk;
	u32 stat;
	u8 init_flag;
	unsigned long buf_start;
	u32 buf_size;

	u32 reg_scratch_0;
	u32 reg_scratch_1;
	u32 reg_scratch_2;
	u32 reg_scratch_3;
	u32 reg_scratch_4;
	u32 reg_scratch_5;
	u32 reg_scratch_6;
	u32 reg_scratch_7;
	u32 reg_scratch_8;
	u32 reg_scratch_9;
	u32 reg_scratch_A;
	u32 reg_scratch_B;
	u32 reg_scratch_C;
	u32 reg_scratch_D;
	u32 reg_scratch_E;
	u32 reg_scratch_F;
	u32 reg_scratch_G;
	u32 reg_scratch_H;
	u32 reg_scratch_I;
	u32 reg_mb_width;
	u32 reg_viff_bit_cnt;
	u32 reg_canvas_addr;
	u32 reg_dbkr_canvas_addr;
	u32 reg_dbkw_canvas_addr;
	u32 reg_anc2_canvas_addr;
	u32 reg_anc0_canvas_addr;
	u32 reg_anc1_canvas_addr;
	u32 reg_anc3_canvas_addr;
	u32 reg_anc4_canvas_addr;
	u32 reg_anc5_canvas_addr;
	u32 slice_ver_pos_pic_type;
	u32 vc1_control_reg;
	u32 avs_co_mb_wr_addr;
	u32 slice_start_byte_01;
	u32 slice_start_byte_23;
	u32 vcop_ctrl_reg;
	u32 iqidct_control;
	u32 rv_ai_mb_count;
	u32 slice_qp;
	u32 dc_scaler;
	u32 avsp_iq_wq_param_01;
	u32 avsp_iq_wq_param_23;
	u32 avsp_iq_wq_param_45;
	u32 avs_co_mb_rd_addr;
	u32 dblk_mb_wid_height;
	u32 mc_pic_w_h;
	u32 avs_co_mb_rw_ctl;
	u32 vld_decode_control;

	struct timer_list check_timer;
	u32 decode_timeout_count;
	unsigned long int start_process_time;
	u32 last_vld_level;
	u32 eos;
	u32 canvas_spec[DECODE_BUFFER_NUM_MAX];
	struct canvas_config_s canvas_config[DECODE_BUFFER_NUM_MAX][2];

	s32 refs[2];
	int dec_result;
	struct timer_list recycle_timer;
	struct work_struct work;
	struct work_struct notify_work;
	atomic_t error_handler_run;
	struct work_struct fatal_error_wd_work;
	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;
/* for error handling */
	u32 run_count;
	u32	not_run_ready;
	u32	input_empty;
	u32 prepare_num;
	u32 put_num;
	u32 peek_num;
	u32 get_num;
	u32 drop_frame_count;
	u32 buffer_not_ready;
	int frameinfo_enable;
	struct firmware_s *fw;
	u32 old_udebug_flag;
	u32 decode_status_skip_pic_done_flag;
	u32 decode_decode_cont_start_code;
	int vdec_pg_enable_flag;
	char vdec_name[32];
	char pts_name[32];
	char new_q_name[32];
	char disp_q_name[32];
	s32 ref_use[DECODE_BUFFER_NUM_MAX];
	s32 buf_use[DECODE_BUFFER_NUM_MAX];
	u32 decoding_index;
	struct pic_info_t pics[DECODE_BUFFER_NUM_MAX];
};

static void reset_process_time(struct vdec_avs_hw_s *hw);
static void start_process_time(struct vdec_avs_hw_s *hw);
static void vavs_save_regs(struct vdec_avs_hw_s *hw);

struct vdec_avs_hw_s *ghw;

#define MULTI_INSTANCE_PROVIDER_NAME    "vdec.avs"

#define DEC_RESULT_NONE     0
#define DEC_RESULT_DONE     1
#define DEC_RESULT_AGAIN    2
#define DEC_RESULT_ERROR    3
#define DEC_RESULT_FORCE_EXIT 4
#define DEC_RESULT_EOS 5
#define DEC_RESULT_GET_DATA         6
#define DEC_RESULT_GET_DATA_RETRY   7
#define DEC_RESULT_USERDATA         8

#define DECODE_ID(hw) (hw->m_ins_flag? hw_to_vdec(hw)->id : 0)

#define PRINT_FLAG_ERROR              0x0
#define PRINT_FLAG_RUN_FLOW           0X0001
#define PRINT_FLAG_DECODING           0x0002
#define PRINT_FLAG_PTS                0x0004
#define PRINT_FLAG_VFRAME_DETAIL	  0x0010
#define PRINT_FLAG_VLD_DETAIL         0x0020
#define PRINT_FLAG_DEC_DETAIL         0x0040
#define PRINT_FLAG_BUFFER_DETAIL      0x0080
#define PRINT_FLAG_FORCE_DONE         0x0100
#define PRINT_FLAG_COUNTER            0X0200
#define PRINT_FRAMEBASE_DATA          0x0400
#define PRINT_FLAG_PARA_DATA          0x1000
#define DEBUG_FLAG_PREPARE_MORE_INPUT 0x2000
#define DEBUG_FLAG_PRINT_REG          0x4000
#define DEBUG_FLAG_DISABLE_TIMEOUT    0x10000
#define DEBUG_WAIT_DECODE_DONE_WHEN_STOP 0x20000
#define DEBUG_PIC_DONE_WHEN_UCODE_PAUSE 0x40000

#undef DEBUG_REG
#ifdef DEBUG_REG
static void WRITE_VREG_DBG2(unsigned adr, unsigned val)
{
	if (debug & DEBUG_FLAG_PRINT_REG)
		pr_info("%s(%x, %x)\n", __func__, adr, val);
	if (adr != 0)
		WRITE_VREG(adr, val);
}

#undef WRITE_VREG
#define WRITE_VREG WRITE_VREG_DBG2
#endif

#undef pr_info
#define pr_info pr_cont
static int debug_print(struct vdec_avs_hw_s *hw,
	int flag, const char *fmt, ...)
{
#define AVS_PRINT_BUF		256
	unsigned char buf[AVS_PRINT_BUF];
	int len = 0;
	int index = 0;
	if (hw)
		index = hw->m_ins_flag ? DECODE_ID(hw) : 0;
	if (hw == NULL ||
		(flag == 0) ||
		((debug_mask &
		(1 << index))
		&& (debug & flag))) {
		va_list args;
		va_start(args, fmt);
		if (hw)
			len = sprintf(buf, "[%d]", index);
		vsnprintf(buf + len, AVS_PRINT_BUF - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static int debug_print_cont(struct vdec_avs_hw_s *hw,
	int flag, const char *fmt, ...)
{
	unsigned char buf[AVS_PRINT_BUF];
	int len = 0;
	int index = 0;
	if (hw)
		index = hw->m_ins_flag ? DECODE_ID(hw) : 0;
	if (hw == NULL ||
		(flag == 0) ||
		((debug_mask &
		(1 << index))
		&& (debug & flag))) {
		va_list args;

		va_start(args, fmt);
		vsnprintf(buf + len, AVS_PRINT_BUF - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static bool is_enough_free_buffer(struct vdec_avs_hw_s *hw)
{
	int i;

	for (i = 0; i < hw->vf_buf_num_used; i++) {
		if ((hw->vfbuf_use[i] == 0) && (hw->ref_use[i] == 0) &&
			(hw->buf_use[i] == 0))
			break;
	}

	return (i == hw->vf_buf_num_used) ? false : true;
}

static int find_free_buffer(struct vdec_avs_hw_s *hw)
{
	int i;

	for (i = 0; i < hw->vf_buf_num_used; i++) {
		if ((hw->vfbuf_use[i] == 0) &&
			(hw->ref_use[i] == 0) &&
			(hw->buf_use[i] == 0))
			break;
	}

	return i;
}

int update_reference(struct vdec_avs_hw_s *hw,
	int index)
{
	hw->ref_use[index]++;
	if (hw->refs[1] == -1) {
		hw->refs[1] = index;
		/*
		* first pic need output to show
		* usecnt do not decrease.
		*/
	} else if (hw->refs[0] == -1) {
		hw->refs[0] = hw->refs[1];
		hw->refs[1] = index;
		/* second pic do not output */
		index = hw->vf_buf_num_used;
	} else {
		hw->ref_use[hw->refs[0]]--; 	//old ref0 ununsed
		hw->refs[0] = hw->refs[1];
		hw->refs[1] = index;
		index = hw->refs[0];
	}
	debug_print(hw, PRINT_FLAG_DEC_DETAIL,
		"hw->refs[0] = %d, hw->refs[1] = %d\n", hw->refs[0], hw->refs[1]);
	return index;
}


static void avs_pts_check_in(struct vdec_avs_hw_s *hw,
	unsigned short decode_pic_count, struct vframe_chunk_s *chunk)
{
	if (chunk)
		debug_print(hw, PRINT_FLAG_PTS,
			"%s %d (wr pos %d), pts %d pts64 %ld timestamp %ld\n",
			__func__, decode_pic_count, hw->pic_pts_wr_pos,
			chunk->pts, (u64)(chunk->pts64), (u64)(chunk->timestamp));
	else
		debug_print(hw, PRINT_FLAG_PTS,
			"%s %d, chunk is null\n",
			__func__, decode_pic_count);

	if (chunk) {
		hw->pic_pts[hw->pic_pts_wr_pos].pts = chunk->pts;
		hw->pic_pts[hw->pic_pts_wr_pos].pts64 = chunk->pts64;
		hw->pic_pts[hw->pic_pts_wr_pos].timestamp = chunk->timestamp;
	} else {
		hw->pic_pts[hw->pic_pts_wr_pos].pts = 0;
		hw->pic_pts[hw->pic_pts_wr_pos].pts64 = 0;
		hw->pic_pts[hw->pic_pts_wr_pos].timestamp = 0;
	}
	hw->pic_pts[hw->pic_pts_wr_pos].decode_pic_count
		= decode_pic_count;
	hw->pic_pts_wr_pos++;
	if (hw->pic_pts_wr_pos >= PIC_PTS_NUM)
		hw->pic_pts_wr_pos = 0;
	return;
}

static void clear_pts_buf(struct vdec_avs_hw_s *hw)
{
	int i;
	debug_print(hw, PRINT_FLAG_PTS,
			"%s\n",	__func__);
	hw->pic_pts_wr_pos = 0;
	for (i = 0; i < PIC_PTS_NUM; i++) {
		hw->pic_pts[hw->pic_pts_wr_pos].pts = 0;
		hw->pic_pts[hw->pic_pts_wr_pos].pts64 = 0;
		hw->pic_pts[hw->pic_pts_wr_pos].timestamp = 0;
		hw->pic_pts[hw->pic_pts_wr_pos].decode_pic_count = 0;
	}
}

static int set_vframe_pts(struct vdec_avs_hw_s *hw,
	unsigned short decode_pic_count, struct vframe_s *vf)
{
	int i;
	int ret = -1;
	for (i = 0; i < PIC_PTS_NUM; i++) {
		if (hw->pic_pts[i].decode_pic_count == decode_pic_count) {
			vf->pts = hw->pic_pts[i].pts;
			vf->pts_us64 = hw->pic_pts[i].pts64;
			vf->timestamp = hw->pic_pts[i].timestamp;
			ret = 0;
			debug_print(hw, PRINT_FLAG_PTS,
				"%s %d (rd pos %d), pts %d pts64 %ld timestamp %ld\n",
				__func__, decode_pic_count, i,
				vf->pts, vf->pts_us64, vf->timestamp);

			break;
		}
	}
	return ret;
}

static void avs_vf_notify_receiver(struct vdec_avs_hw_s *hw,
	const char *provider_name, int event_type, void *data)
{
	if (hw->m_ins_flag)
		vf_notify_receiver(hw_to_vdec(hw)->vf_provider_name,
			event_type, data);
	else
		vf_notify_receiver(provider_name, event_type, data);
}

static void set_frame_info(struct vdec_avs_hw_s *hw, struct vframe_s *vf,
	unsigned int *duration)
{
	int ar = 0;

	unsigned int pixel_ratio = READ_VREG(AVS_PIC_RATIO);
	hw->prepare_num++;
#ifndef USE_AVS_SEQ_INFO
	if (hw->vavs_amstream_dec_info.width > 0
		&& hw->vavs_amstream_dec_info.height > 0) {
		vf->width = hw->vavs_amstream_dec_info.width;
		vf->height = hw->vavs_amstream_dec_info.height;
	} else
#endif
	{
		vf->width = READ_VREG(AVS_PIC_INFO) & 0x3fff;
		vf->height = (READ_VREG(AVS_PIC_INFO) >> 14) & 0x3fff;
		hw->frame_width = vf->width;
		hw->frame_height = vf->height;
	}

#ifndef USE_AVS_SEQ_INFO
	if (hw->vavs_amstream_dec_info.rate > 0)
		*duration = hw->vavs_amstream_dec_info.rate;
	else
#endif
	{
		*duration = frame_rate_tab[READ_VREG(AVS_FRAME_RATE) & 0xf];
		hw->frame_dur = *duration;
		schedule_work(&hw->notify_work);
	}

	if (hw->vavs_ratio == 0) {
		/* always stretch to 16:9 */
		vf->ratio_control |= (0x90 <<
				DISP_RATIO_ASPECT_RATIO_BIT);
		vf->sar_width = 1;
		vf->sar_height = 1;
	} else {
		switch (pixel_ratio) {
		case 1:
			vf->sar_width = 1;
			vf->sar_height = 1;
			ar = (vf->height * hw->vavs_ratio) / vf->width;
			break;
		case 2:
			vf->sar_width = 4;
			vf->sar_height = 3;
			ar = (vf->height * 3 * hw->vavs_ratio) / (vf->width * 4);
			break;
		case 3:
			vf->sar_width = 16;
			vf->sar_height = 9;
			ar = (vf->height * 9 * hw->vavs_ratio) / (vf->width * 16);
			break;
		case 4:
			vf->sar_width = 221;
			vf->sar_height = 100;
			ar = (vf->height * 100 * hw->vavs_ratio) / (vf->width *
					221);
			break;
		default:
			vf->sar_width = 1;
			vf->sar_height = 1;
			ar = (vf->height * hw->vavs_ratio) / vf->width;
			break;
		}
	}

	ar = min(ar, DISP_RATIO_ASPECT_RATIO_MAX);

	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);

	vf->flag = 0;
	buf_of_vf(vf)->detached = 0;

}

#ifdef ENABLE_USER_DATA

static void userdata_push_process(struct vdec_avs_hw_s *hw)
{
	unsigned int user_data_flags;
	unsigned int user_data_wp;
	unsigned int user_data_length;
	struct userdata_poc_info_t user_data_poc;
#ifdef DUMP_LAST_REPORTED_USER_DATA
	int user_data_len;
	int wp_start;
	unsigned char *pdata;
	int nLeft;
#endif

	user_data_flags = READ_VREG(AV_SCRATCH_N);
	user_data_wp = (user_data_flags >> 16) & 0xffff;
	user_data_length = user_data_flags & 0x7fff;

#ifdef DUMP_LAST_REPORTED_USER_DATA
	dma_sync_single_for_cpu(amports_get_dma_device(),
			hw->user_data_buffer_phys, USER_DATA_SIZE,
			DMA_FROM_DEVICE);

	if (user_data_length & 0x07)
		user_data_len = (user_data_length + 8) & 0xFFFFFFF8;
	else
		user_data_len = user_data_length;

	if (user_data_wp >= user_data_len) {
		wp_start = user_data_wp - user_data_len;

		pdata = (unsigned char *)hw->user_data_buffer;
		pdata += wp_start;
		nLeft = user_data_len;
		while (nLeft >= 8) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
				pdata[0], pdata[1], pdata[2], pdata[3],
				pdata[4], pdata[5], pdata[6], pdata[7]);
			nLeft -= 8;
			pdata += 8;
		}
	} else {
		wp_start = user_data_wp + USER_DATA_SIZE - user_data_len;

		pdata = (unsigned char *)hw->user_data_buffer;
		pdata += wp_start;
		nLeft = USER_DATA_SIZE - wp_start;

		while (nLeft >= 8) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
				pdata[0], pdata[1], pdata[2], pdata[3],
				pdata[4], pdata[5], pdata[6], pdata[7]);
			nLeft -= 8;
			pdata += 8;
		}

		pdata = (unsigned char *)hw->user_data_buffer;
		nLeft = user_data_wp;
		while (nLeft >= 8) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
				pdata[0], pdata[1], pdata[2], pdata[3],
				pdata[4], pdata[5], pdata[6], pdata[7]);
			nLeft -= 8;
			pdata += 8;
		}
	}
#endif

	user_data_poc.poc_info = READ_VREG(AV_SCRATCH_L);
	user_data_poc.poc_number = READ_VREG(AV_SCRATCH_M);

	WRITE_VREG(AV_SCRATCH_N, 0);
}

static void userdata_push_do_work(struct work_struct *work)
{
	struct vdec_avs_hw_s *hw =
	container_of(work, struct vdec_avs_hw_s, userdata_push_work);
	userdata_push_process(hw);
}

static u8 UserDataHandler(struct vdec_avs_hw_s *hw)
{
	unsigned int user_data_flags;

	user_data_flags = READ_VREG(AV_SCRATCH_N);
	if (user_data_flags & (1 << 15)) {	/* data ready */
		if (hw->m_ins_flag) {
			hw->dec_result = DEC_RESULT_USERDATA;
			vdec_schedule_work(&hw->work);
			return 1;
		} else
			schedule_work(&hw->userdata_push_work);
	}
	return 0;
}
#endif


static inline void avs_update_gvs(struct vdec_avs_hw_s *hw)
{
	if (hw->gvs->frame_height != hw->frame_height) {
		hw->gvs->frame_width = hw->frame_width;
		hw->gvs->frame_height = hw->frame_height;
	}
	if (hw->gvs->frame_dur != hw->frame_dur) {
		hw->gvs->frame_dur = hw->frame_dur;
		if (hw->frame_dur != 0)
			hw->gvs->frame_rate = ((96000 * 10 / hw->frame_dur) % 10) < 5 ?
					96000 / hw->frame_dur : (96000 / hw->frame_dur +1);
		else
			hw->gvs->frame_rate = -1;
	}

	hw->gvs->status = hw->stat;
	hw->gvs->error_count = READ_VREG(AV_SCRATCH_C);
	hw->gvs->drop_frame_count = hw->drop_frame_count;

}

#ifdef HANDLE_AVS_IRQ
static irqreturn_t vavs_isr(int irq, void *dev_id)
#else
static void vavs_isr(void)
#endif
{
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	return IRQ_WAKE_THREAD;
}

static int error_recovery_mode;   /*0: blocky  1: mosaic*/

static struct vframe_s *vavs_vf_peek(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_avs_hw_s *hw =
	(struct vdec_avs_hw_s *)op_arg;
	hw->peek_num++;
	if (step == 2)
		return NULL;
	if (hw->recover_flag)
		return NULL;

	if (kfifo_len(&hw->display_q) > VF_POOL_SIZE) {
		debug_print(hw, PRINT_FLAG_RUN_FLOW,
			"kfifo len:%d invaild, peek error\n",
			kfifo_len(&hw->display_q));
		return NULL;
	}

	if (kfifo_peek(&hw->display_q, &vf)) {
		if (vf) {
			if (force_fps & 0x100) {
				u32 rate = force_fps & 0xff;

				if (rate)
					vf->duration = 96000/rate;
				else
					vf->duration = 0;
			}

		}
		return vf;
	}

	return NULL;
}

static struct vframe_s *vavs_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_avs_hw_s *hw =
	(struct vdec_avs_hw_s *)op_arg;
	unsigned long flags;

	if (hw->recover_flag)
		return NULL;

	if (step == 2)
		return NULL;
	else if (step == 1)
		step = 2;

	spin_lock_irqsave(&lock, flags);
	if (kfifo_get(&hw->display_q, &vf)) {
		if (vf) {
			hw->get_num++;
			if (force_fps & 0x100) {
				u32 rate = force_fps & 0xff;

				if (rate)
					vf->duration = 96000/rate;
				else
					vf->duration = 0;
			}

			debug_print(hw, PRINT_FLAG_VFRAME_DETAIL,
				"%s, index = %d, w %d h %d, type 0x%x detached %d\n",
				__func__,
				vf->index,
				vf->width,
				vf->height,
				vf->type,
				buf_of_vf(vf)->detached);
		}
		spin_unlock_irqrestore(&lock, flags);
		return vf;
	}
	spin_unlock_irqrestore(&lock, flags);
	return NULL;

}

static void vavs_vf_put(struct vframe_s *vf, void *op_arg)
{
	int i;
	struct vdec_avs_hw_s *hw =
	(struct vdec_avs_hw_s *)op_arg;

	if (vf) {
		hw->put_num++;
		debug_print(hw, PRINT_FLAG_VFRAME_DETAIL,
			"%s, index = %d, w %d h %d, type 0x%x detached 0x%x\n",
			__func__,
			vf->index,
			vf->width,
			vf->height,
			vf->type,
			buf_of_vf(vf)->detached);
	}
	if (hw->recover_flag)
		return;

	for (i = 0; i < VF_POOL_SIZE; i++) {
		if (vf == &hw->vfpool[i].vf)
			break;
	}
	if (i < VF_POOL_SIZE)

		kfifo_put(&hw->recycle_q, (const struct vframe_s *)vf);

}

static int vavs_event_cb(int type, void *data, void *private_data)
{
	struct vdec_avs_hw_s *hw = (struct vdec_avs_hw_s *)private_data;

	if (type & VFRAME_EVENT_RECEIVER_REQ_STATE) {
		struct provider_state_req_s *req =
			(struct provider_state_req_s *)data;
		if (req->req_type == REQ_STATE_SECURE)
			req->req_result[0] = vdec_secure(hw_to_vdec(hw));
		else
			req->req_result[0] = 0xffffffff;
	}

	return 0;
}

static int vavs_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	struct vdec_avs_hw_s *hw =
	(struct vdec_avs_hw_s *)vdec->private;

	if (!hw)
		return -1;

	vstatus->frame_width = hw->frame_width;
	vstatus->frame_height = hw->frame_height;
	if (hw->frame_dur != 0)
		vstatus->frame_rate = ((96000 * 10 / hw->frame_dur) % 10) < 5 ?
				96000 / hw->frame_dur : (96000 / hw->frame_dur +1);
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = READ_VREG(AV_SCRATCH_C);
	vstatus->status = hw->stat;
	vstatus->bit_rate = hw->gvs->bit_rate;
	vstatus->frame_dur = hw->frame_dur;
	vstatus->frame_data = hw->gvs->frame_data;
	vstatus->total_data = hw->gvs->total_data;
	vstatus->frame_count = hw->gvs->frame_count;
	vstatus->error_frame_count = hw->gvs->error_frame_count;
	vstatus->drop_frame_count = hw->gvs->drop_frame_count;
	vstatus->i_decoded_frames = hw->gvs->i_decoded_frames;
	vstatus->i_lost_frames = hw->gvs->i_lost_frames;
	vstatus->i_concealed_frames = hw->gvs->i_concealed_frames;
	vstatus->p_decoded_frames = hw->gvs->p_decoded_frames;
	vstatus->p_lost_frames = hw->gvs->p_lost_frames;
	vstatus->p_concealed_frames = hw->gvs->p_concealed_frames;
	vstatus->b_decoded_frames = hw->gvs->b_decoded_frames;
	vstatus->b_lost_frames = hw->gvs->b_lost_frames;
	vstatus->b_concealed_frames = hw->gvs->b_concealed_frames;
	vstatus->total_data = hw->gvs->total_data;
	vstatus->samp_cnt = hw->gvs->samp_cnt;
	vstatus->offset = hw->gvs->offset;
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
		"%s", DRIVER_NAME);

	return 0;
}

static int vavs_set_isreset(struct vdec_s *vdec, int isreset)
{
	struct vdec_avs_hw_s *hw =
		(struct vdec_avs_hw_s *)vdec->private;

	hw->is_reset = isreset;
	return 0;
}

static int vavs_vdec_info_init(struct vdec_avs_hw_s *hw)
{

	hw->gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);
	if (NULL == hw->gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}

	return 0;
}
/****************************************/
static int vavs_canvas_init(struct vdec_avs_hw_s *hw)
{
	int i, ret;
	u32 canvas_width, canvas_height;
	u32 decbuf_size, decbuf_y_size, decbuf_uv_size;
	unsigned long buf_start;
	int need_alloc_buf_num;
	struct vdec_s *vdec = NULL;

	if (hw->m_ins_flag)
		vdec = hw_to_vdec(hw);

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

	for (i = 0; i < hw->vf_buf_num_used; i++) {
		unsigned canvas;
		if (vdec->parallel_dec == 1) {
			unsigned tmp;
			if (canvas_u(hw->canvas_spec[i]) == 0xff) {
				tmp = vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
				hw->canvas_spec[i] &= ~(0xffff << 8);
				hw->canvas_spec[i] |= tmp << 8;
				hw->canvas_spec[i] |= tmp << 16;
			}
			if (canvas_y(hw->canvas_spec[i]) == 0xff) {
				tmp = vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
				hw->canvas_spec[i] &= ~0xff;
				hw->canvas_spec[i] |= tmp;
			}
			canvas = hw->canvas_spec[i];
		} else {
			canvas = vdec->get_canvas(i, 2);
			hw->canvas_spec[i] = canvas;
		}
	}

#ifdef AVSP_LONG_CABAC
	need_alloc_buf_num = hw->vf_buf_num_used + 2;
#else
	need_alloc_buf_num = hw->vf_buf_num_used + 1;
#endif
	for (i = 0; i < need_alloc_buf_num; i++) {

		if (i == (need_alloc_buf_num - 1))
			decbuf_size = WORKSPACE_SIZE;
#ifdef AVSP_LONG_CABAC
		else if (i == (need_alloc_buf_num - 2))
			decbuf_size = WORKSPACE_SIZE_A;
#endif
		ret = decoder_bmmu_box_alloc_buf_phy(hw->mm_blk_handle, i,
				decbuf_size, DRIVER_NAME, &buf_start);
		if (ret < 0)
			return ret;
		if (i == (need_alloc_buf_num - 1)) {
			if (firmware_sel == 1)
				hw->buf_offset = buf_start -
					RV_AI_BUFF_START_ADDR;
			else
				hw->buf_offset = buf_start -
					LONG_CABAC_RV_AI_BUFF_START_ADDR;
			continue;
		}
#ifdef AVSP_LONG_CABAC
		else if (i == (need_alloc_buf_num - 2)) {
			avsp_heap_adr = codec_mm_phys_to_virt(buf_start);
			continue;
		}
#endif
		if (i < hw->vf_buf_num_used) {
			if (hw->m_ins_flag) {

				hw->canvas_config[i][0].phy_addr =
				buf_start;
				hw->canvas_config[i][0].width =
				canvas_width;
				hw->canvas_config[i][0].height =
				canvas_height;
				hw->canvas_config[i][0].block_mode =
					vdec->canvas_mode;
				hw->canvas_config[i][0].endian = 0;

				hw->canvas_config[i][1].phy_addr =
				buf_start + decbuf_y_size;
				hw->canvas_config[i][1].width =
				canvas_width;
				hw->canvas_config[i][1].height =
				canvas_height / 2;
				hw->canvas_config[i][1].block_mode =
					vdec->canvas_mode;
				hw->canvas_config[i][1].endian = 0;
			} else {
#ifdef NV21
				canvas_config(canvas_base + canvas_num * i + 0,
						buf_start,
						canvas_width, canvas_height,
						CANVAS_ADDR_NOWRAP,
						CANVAS_BLKMODE_32X32);
				canvas_config(canvas_base + canvas_num * i + 1,
						buf_start +
						decbuf_y_size, canvas_width,
						canvas_height / 2,
						CANVAS_ADDR_NOWRAP,
						CANVAS_BLKMODE_32X32);
#else
				canvas_config(canvas_num * i + 0,
						buf_start,
						canvas_width, canvas_height,
						CANVAS_ADDR_NOWRAP,
						CANVAS_BLKMODE_32X32);
				canvas_config(canvas_num * i + 1,
						buf_start +
						decbuf_y_size, canvas_width / 2,
						canvas_height / 2,
						CANVAS_ADDR_NOWRAP,
						CANVAS_BLKMODE_32X32);
				canvas_config(canvas_num * i + 2,
						buf_start +
						decbuf_y_size + decbuf_uv_size,
						canvas_width / 2, canvas_height / 2,
						CANVAS_ADDR_NOWRAP,
						CANVAS_BLKMODE_32X32);
#endif
				debug_print(hw, PRINT_FLAG_VFRAME_DETAIL,
					"canvas config %d, addr %p\n", i,
						   (void *)buf_start);
			}
		}
	}
	return 0;
}

static void vavs_recover(struct vdec_avs_hw_s *hw)
{
	vavs_canvas_init(hw);

	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) | (1 << 4));
	WRITE_VREG(DOS_SW_RESET0, 0);

	READ_VREG(DOS_SW_RESET0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) | (1 << 4));
	WRITE_VREG(DOS_SW_RESET0, 0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 9) | (1 << 8));
	WRITE_VREG(DOS_SW_RESET0, 0);

	if (firmware_sel == 1) {
		WRITE_VREG(POWER_CTL_VLD, 0x10);
		WRITE_VREG_BITS(VLD_MEM_VIFIFO_CONTROL, 2,
			MEM_FIFO_CNT_BIT, 2);
		WRITE_VREG_BITS(VLD_MEM_VIFIFO_CONTROL, 8,
			MEM_LEVEL_CNT_BIT, 6);
	}


	if (firmware_sel == 0) {
		/* fixed canvas index */
		WRITE_VREG(AV_SCRATCH_0, canvas_base);
		WRITE_VREG(AV_SCRATCH_1, hw->vf_buf_num_used);
	} else {
		int ii;
#ifndef USE_DYNAMIC_BUF_NUM
		for (ii = 0; ii < 4; ii++) {
			WRITE_VREG(AV_SCRATCH_0 + ii,
				(canvas_base + canvas_num * ii) |
				((canvas_base + canvas_num * ii + 1)
					<< 8) |
				((canvas_base + canvas_num * ii + 1)
					<< 16)
			);
		}
#else
		for (ii = 0; ii < hw->vf_buf_num_used; ii += 2) {
			WRITE_VREG(buf_spec_reg[ii >> 1],
				(canvas_base + canvas_num * ii) |
				((canvas_base + canvas_num * ii + 1)
					<< 8) |
				((canvas_base + canvas_num * ii + 2)
					<< 16) |
				((canvas_base + canvas_num * ii + 3)
					<< 24)
			);
		}
#endif
	}

	/* notify ucode the buffer offset */
	WRITE_VREG(AV_SCRATCH_F, hw->buf_offset);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);

#ifndef USE_DYNAMIC_BUF_NUM
	WRITE_VREG(AVS_SOS_COUNT, 0);
#endif
	WRITE_VREG(AVS_BUFFERIN, 0);
	WRITE_VREG(AVS_BUFFEROUT, 0);
	if (error_recovery_mode)
		WRITE_VREG(AVS_ERROR_RECOVERY_MODE, 0);
	else
		WRITE_VREG(AVS_ERROR_RECOVERY_MODE, 1);
	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);
#ifndef USE_DYNAMIC_BUF_NUM				/* def DEBUG_UCODE */
	WRITE_VREG(AV_SCRATCH_D, 0);
#endif

#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
#endif
	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);

#ifdef PIC_DC_NEED_CLEAR
	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 31);
#endif

#ifdef AVSP_LONG_CABAC
	if (firmware_sel == 0) {
		WRITE_VREG(LONG_CABAC_DES_ADDR, es_write_addr_phy);
		WRITE_VREG(LONG_CABAC_REQ, 0);
		WRITE_VREG(LONG_CABAC_PIC_SIZE, 0);
		WRITE_VREG(LONG_CABAC_SRC_ADDR, 0);
	}
#endif
	WRITE_VREG(AV_SCRATCH_5, 0);

}

#define MBY_MBX                 MB_MOTION_MODE /*0xc07*/
#define AVS_CO_MB_WR_ADDR        0xc38
#define AVS_CO_MB_RW_CTL         0xc3d
#define AVS_CO_MB_RD_ADDR        0xc39
#define AVSP_IQ_WQ_PARAM_01                        0x0e19
#define AVSP_IQ_WQ_PARAM_23                        0x0e1a
#define AVSP_IQ_WQ_PARAM_45                        0x0e1b

static void vavs_save_regs(struct vdec_avs_hw_s *hw)
{
	hw->reg_scratch_0 = READ_VREG(AV_SCRATCH_0);
	hw->reg_scratch_1 = READ_VREG(AV_SCRATCH_1);
	hw->reg_scratch_2 = READ_VREG(AV_SCRATCH_2);
	hw->reg_scratch_3 = READ_VREG(AV_SCRATCH_3);
	hw->reg_scratch_5 = READ_VREG(AV_SCRATCH_5);
	hw->reg_scratch_6 = READ_VREG(AV_SCRATCH_6);
	hw->reg_scratch_7 = READ_VREG(AV_SCRATCH_7);
	hw->reg_scratch_8 = READ_VREG(AV_SCRATCH_8);
	hw->reg_scratch_9 = READ_VREG(AV_SCRATCH_9);
	hw->reg_scratch_A = READ_VREG(AV_SCRATCH_A);
	hw->reg_scratch_C = READ_VREG(AV_SCRATCH_C);
	hw->reg_scratch_D = READ_VREG(AV_SCRATCH_D);
	hw->reg_scratch_E = READ_VREG(AV_SCRATCH_E);
	hw->reg_scratch_F = READ_VREG(AV_SCRATCH_F);
	hw->reg_scratch_G = READ_VREG(AV_SCRATCH_G);
	hw->reg_scratch_H = READ_VREG(AV_SCRATCH_H);
	hw->reg_scratch_I = READ_VREG(AV_SCRATCH_I);

	hw->reg_mb_width = READ_VREG(MB_WIDTH);
	hw->reg_viff_bit_cnt = READ_VREG(VIFF_BIT_CNT);

	hw->reg_canvas_addr = READ_VREG(REC_CANVAS_ADDR);
	hw->reg_dbkr_canvas_addr = READ_VREG(DBKR_CANVAS_ADDR);
	hw->reg_dbkw_canvas_addr = READ_VREG(DBKW_CANVAS_ADDR);
	hw->reg_anc2_canvas_addr = READ_VREG(ANC2_CANVAS_ADDR);
	hw->reg_anc0_canvas_addr = READ_VREG(ANC0_CANVAS_ADDR);
	hw->reg_anc1_canvas_addr = READ_VREG(ANC1_CANVAS_ADDR);
	hw->reg_anc3_canvas_addr = READ_VREG(ANC3_CANVAS_ADDR);
	hw->reg_anc4_canvas_addr = READ_VREG(ANC4_CANVAS_ADDR);
	hw->reg_anc5_canvas_addr = READ_VREG(ANC5_CANVAS_ADDR);

	debug_print(hw, PRINT_FLAG_DEC_DETAIL,
		"%s ANC0_CANVAS_ADDR = 0x%x, ANC1_CANVAS_ADDR = 0x%x, ANC2_CANVAS_ADDR = 0x%x\n",
		__func__, READ_VREG(ANC0_CANVAS_ADDR), READ_VREG(ANC1_CANVAS_ADDR), READ_VREG(ANC2_CANVAS_ADDR));

	hw->slice_ver_pos_pic_type = READ_VREG(SLICE_VER_POS_PIC_TYPE);

	hw->vc1_control_reg = READ_VREG(VC1_CONTROL_REG);
	hw->avs_co_mb_wr_addr = READ_VREG(AVS_CO_MB_WR_ADDR);
	hw->slice_start_byte_01 = READ_VREG(SLICE_START_BYTE_01);
	hw->slice_start_byte_23 = READ_VREG(SLICE_START_BYTE_23);
	hw->vcop_ctrl_reg = READ_VREG(VCOP_CTRL_REG);
	hw->iqidct_control = READ_VREG(IQIDCT_CONTROL);
	hw->rv_ai_mb_count = READ_VREG(RV_AI_MB_COUNT);
	hw->slice_qp = READ_VREG(SLICE_QP);

	hw->dc_scaler = READ_VREG(DC_SCALER);
	hw->avsp_iq_wq_param_01 = READ_VREG(AVSP_IQ_WQ_PARAM_01);
	hw->avsp_iq_wq_param_23 = READ_VREG(AVSP_IQ_WQ_PARAM_23);
	hw->avsp_iq_wq_param_45 = READ_VREG(AVSP_IQ_WQ_PARAM_45);
	hw->avs_co_mb_rd_addr = READ_VREG(AVS_CO_MB_RD_ADDR);
	hw->dblk_mb_wid_height = READ_VREG(DBLK_MB_WID_HEIGHT);
	hw->mc_pic_w_h = READ_VREG(MC_PIC_W_H);
	hw->avs_co_mb_rw_ctl = READ_VREG(AVS_CO_MB_RW_CTL);

	hw->vld_decode_control = READ_VREG(VLD_DECODE_CONTROL);
}

static void vavs_restore_regs(struct vdec_avs_hw_s *hw)
{
	debug_print(hw, PRINT_FLAG_DECODING,
		"%s scratch_8 (AVS_BUFFERIN) 0x%x, decode_pic_count = %d\n",
		__func__, hw->reg_scratch_8, hw->decode_pic_count);

	WRITE_VREG(AV_SCRATCH_0, hw->reg_scratch_0);
	WRITE_VREG(AV_SCRATCH_1, hw->reg_scratch_1);
	WRITE_VREG(AV_SCRATCH_2, hw->reg_scratch_2);
	WRITE_VREG(AV_SCRATCH_3, hw->reg_scratch_3);
	WRITE_VREG(AV_SCRATCH_5, hw->reg_scratch_5);
	WRITE_VREG(AV_SCRATCH_6, hw->reg_scratch_6);
	WRITE_VREG(AV_SCRATCH_7, hw->reg_scratch_7);
	WRITE_VREG(AV_SCRATCH_8, hw->reg_scratch_8);
	WRITE_VREG(AV_SCRATCH_9, hw->reg_scratch_9);
	WRITE_VREG(AV_SCRATCH_A, hw->reg_scratch_A);
	WRITE_VREG(AV_SCRATCH_C, hw->reg_scratch_C);
	WRITE_VREG(AV_SCRATCH_D, hw->reg_scratch_D);
	WRITE_VREG(AV_SCRATCH_E, hw->reg_scratch_E);
	WRITE_VREG(AV_SCRATCH_F, hw->reg_scratch_F);
	WRITE_VREG(AV_SCRATCH_G, hw->reg_scratch_G);
	WRITE_VREG(AV_SCRATCH_H, hw->reg_scratch_H);
	WRITE_VREG(AV_SCRATCH_I, hw->reg_scratch_I);

	WRITE_VREG(MB_WIDTH, hw->reg_mb_width);
	WRITE_VREG(VIFF_BIT_CNT, hw->reg_viff_bit_cnt);

	WRITE_VREG(REC_CANVAS_ADDR, hw->reg_canvas_addr);
	WRITE_VREG(DBKR_CANVAS_ADDR, hw->reg_dbkr_canvas_addr);
	WRITE_VREG(DBKW_CANVAS_ADDR, hw->reg_dbkw_canvas_addr);
	WRITE_VREG(ANC2_CANVAS_ADDR, hw->reg_anc2_canvas_addr);
	WRITE_VREG(ANC0_CANVAS_ADDR, hw->reg_anc0_canvas_addr);
	WRITE_VREG(ANC1_CANVAS_ADDR, hw->reg_anc1_canvas_addr);
	WRITE_VREG(ANC3_CANVAS_ADDR, hw->reg_anc3_canvas_addr);
	WRITE_VREG(ANC4_CANVAS_ADDR, hw->reg_anc4_canvas_addr);
	WRITE_VREG(ANC5_CANVAS_ADDR, hw->reg_anc5_canvas_addr);
	if (hw->refs[1] == -1)
		WRITE_VREG(ANC0_CANVAS_ADDR, 0xffffffff);
	else
		WRITE_VREG(ANC0_CANVAS_ADDR, hw->canvas_spec[hw->refs[1]]);

	if (hw->refs[0] == -1)
		WRITE_VREG(ANC1_CANVAS_ADDR, hw->canvas_spec[hw->refs[1]]);
	else
		WRITE_VREG(ANC1_CANVAS_ADDR, hw->canvas_spec[hw->refs[0]]);

	debug_print(hw, PRINT_FLAG_DEC_DETAIL,
		"%s ANC0_CANVAS_ADDR = 0x%x, ANC1_CANVAS_ADDR = 0x%x, ANC2_CANVAS_ADDR = 0x%x\n",
		__func__, READ_VREG(ANC0_CANVAS_ADDR), READ_VREG(ANC1_CANVAS_ADDR), READ_VREG(ANC2_CANVAS_ADDR));

    WRITE_VREG(SLICE_VER_POS_PIC_TYPE, hw->slice_ver_pos_pic_type);

    WRITE_VREG(VC1_CONTROL_REG, hw->vc1_control_reg);
    WRITE_VREG(AVS_CO_MB_WR_ADDR, hw->avs_co_mb_wr_addr);
    WRITE_VREG(SLICE_START_BYTE_01, hw->slice_start_byte_01);
    WRITE_VREG(SLICE_START_BYTE_23, hw->slice_start_byte_23);
    WRITE_VREG(VCOP_CTRL_REG, hw->vcop_ctrl_reg);
    WRITE_VREG(IQIDCT_CONTROL, hw->iqidct_control);
    WRITE_VREG(RV_AI_MB_COUNT, hw->rv_ai_mb_count);
    WRITE_VREG(SLICE_QP, hw->slice_qp);

    WRITE_VREG(DC_SCALER, hw->dc_scaler);
    WRITE_VREG(AVSP_IQ_WQ_PARAM_01, hw->avsp_iq_wq_param_01);
    WRITE_VREG(AVSP_IQ_WQ_PARAM_23, hw->avsp_iq_wq_param_23);
    WRITE_VREG(AVSP_IQ_WQ_PARAM_45, hw->avsp_iq_wq_param_45);
    WRITE_VREG(AVS_CO_MB_RD_ADDR, hw->avs_co_mb_rd_addr);
    WRITE_VREG(DBLK_MB_WID_HEIGHT, hw->dblk_mb_wid_height);
    WRITE_VREG(MC_PIC_W_H, hw->mc_pic_w_h);
    WRITE_VREG(AVS_CO_MB_RW_CTL, hw->avs_co_mb_rw_ctl);

    WRITE_VREG(VLD_DECODE_CONTROL, hw->vld_decode_control);

}

static int vavs_prot_init(struct vdec_avs_hw_s *hw)
{
	int r = 0;
#if DEBUG_MULTI_FLAG > 0
	if (hw->decode_pic_count == 0) {
#endif

	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) | (1 << 4));
	WRITE_VREG(DOS_SW_RESET0, 0);

	READ_VREG(DOS_SW_RESET0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) | (1 << 4));
	WRITE_VREG(DOS_SW_RESET0, 0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 9) | (1 << 8));
	WRITE_VREG(DOS_SW_RESET0, 0);

#if DEBUG_MULTI_FLAG > 0
	}
#endif
	/***************** reset vld   **********************************/
	WRITE_VREG(POWER_CTL_VLD, 0x10);
	WRITE_VREG_BITS(VLD_MEM_VIFIFO_CONTROL, 2, MEM_FIFO_CNT_BIT, 2);
	WRITE_VREG_BITS(VLD_MEM_VIFIFO_CONTROL,	8, MEM_LEVEL_CNT_BIT, 6);
	/*************************************************************/
	if (hw->m_ins_flag) {
		int i;
		u32 index = -1;
		index = find_free_buffer(hw);
		if (index < 0)
			return -1;
		hw->decoding_index = index;
		debug_print(hw, PRINT_FLAG_DECODING,
			"hw->decoding_index = %d\n",
			hw->decoding_index);
		WRITE_VREG(AV_SCRATCH_4, index);
		WRITE_VREG(AV_SCRATCH_B, 0);
		if (hw->decode_pic_count == 0) {
			r = vavs_canvas_init(hw);
#ifndef USE_DYNAMIC_BUF_NUM
			for (i = 0; i < 4; i++) {
				WRITE_VREG(AV_SCRATCH_0 + i,
					hw->canvas_spec[i]
				);
			}
#else
			for (i = 0; i < hw->vf_buf_num_used; i++)
				WRITE_VREG(buf_spec_reg[i], 0);
			for (i = 0; i < hw->vf_buf_num_used; i += 2) {
				WRITE_VREG(buf_spec_reg[i >> 1],
					(hw->canvas_spec[i] & 0xffff) |
					((hw->canvas_spec[i + 1] & 0xffff)
						<< 16)
				);
				debug_print(hw, PRINT_FLAG_DECODING,
					"%s WRITE_VREG(0x%x, 0x%x)\n",
					__func__, buf_spec_reg[i >> 1], READ_VREG(buf_spec_reg[i >> 1]));
			}
#endif
		} else
			vavs_restore_regs(hw);

		for (i = 0; i < hw->vf_buf_num_used; i++) {
			if (hw->canvas_config[i][0].phy_addr &&
				hw->canvas_config[i][1].phy_addr) {
				config_cav_lut_ex(canvas_y(hw->canvas_spec[i]),
					hw->canvas_config[i][0].phy_addr,
					hw->canvas_config[i][0].width,
					hw->canvas_config[i][0].height,
					CANVAS_ADDR_NOWRAP,
					hw->canvas_config[i][0].block_mode,
					0, VDEC_1);

				config_cav_lut_ex(canvas_u(hw->canvas_spec[i]),
					hw->canvas_config[i][1].phy_addr,
					hw->canvas_config[i][1].width,
					hw->canvas_config[i][1].height,
					CANVAS_ADDR_NOWRAP,
					hw->canvas_config[i][1].block_mode,
					0, VDEC_1);
			}
		}
	} else {
		r = vavs_canvas_init(hw);
#ifdef NV21
		if (firmware_sel == 0) {
			/* fixed canvas index */
			WRITE_VREG(AV_SCRATCH_0, canvas_base);
			WRITE_VREG(AV_SCRATCH_1, hw->vf_buf_num_used);
		} else {
			int ii;
#ifndef USE_DYNAMIC_BUF_NUM
			for (ii = 0; ii < 4; ii++) {
				WRITE_VREG(AV_SCRATCH_0 + ii,
					(canvas_base + canvas_num * ii) |
					((canvas_base + canvas_num * ii + 1) << 8) |
					((canvas_base + canvas_num * ii + 1) << 16));
			}
#else
			for (ii = 0; ii < hw->vf_buf_num_used; ii += 2) {
				WRITE_VREG(buf_spec_reg[ii >> 1],
					(canvas_base + canvas_num * ii) |
					((canvas_base + canvas_num * ii + 1) << 8) |
					((canvas_base + canvas_num * ii + 2) << 16) |
					((canvas_base + canvas_num * ii + 3) << 24));
			}
#endif
		}
#else
		/* index v << 16 | u << 8 | y */
		WRITE_VREG(AV_SCRATCH_0, 0x020100);
		WRITE_VREG(AV_SCRATCH_1, 0x050403);
		WRITE_VREG(AV_SCRATCH_2, 0x080706);
		WRITE_VREG(AV_SCRATCH_3, 0x0b0a09);
#endif
	}
	/* notify ucode the buffer offset */
	if (hw->decode_pic_count == 0)
		WRITE_VREG(AV_SCRATCH_F, hw->buf_offset);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);

	if (hw->decode_pic_count == 0) {
#ifndef USE_DYNAMIC_BUF_NUM
		WRITE_VREG(AVS_SOS_COUNT, 0);
#endif
		WRITE_VREG(AVS_BUFFERIN, 0);
		WRITE_VREG(AVS_BUFFEROUT, 0);
	}
	if (error_recovery_mode)
		WRITE_VREG(AVS_ERROR_RECOVERY_MODE, 0);
	else
		WRITE_VREG(AVS_ERROR_RECOVERY_MODE, 1);
	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);
#ifndef USE_DYNAMIC_BUF_NUM				/* def DEBUG_UCODE */
	if (hw->decode_pic_count == 0)
		WRITE_VREG(AV_SCRATCH_D, 0);
#endif

#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
#endif
	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);

#ifdef PIC_DC_NEED_CLEAR
	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 31);
#endif
	if (hw->m_ins_flag && start_decoding_delay > 0)
		msleep(start_decoding_delay);

#ifdef AVSP_LONG_CABAC
	if (firmware_sel == 0) {
		WRITE_VREG(LONG_CABAC_DES_ADDR, es_write_addr_phy);
		WRITE_VREG(LONG_CABAC_REQ, 0);
		WRITE_VREG(LONG_CABAC_PIC_SIZE, 0);
		WRITE_VREG(LONG_CABAC_SRC_ADDR, 0);
	}
#endif

#ifdef ENABLE_USER_DATA
	if (hw->decode_pic_count == 0) {
		WRITE_VREG(AV_SCRATCH_N, (u32)(hw->user_data_buffer_phys - hw->buf_offset));
		pr_debug("AV_SCRATCH_N = 0x%x\n", READ_VREG(AV_SCRATCH_N));
	} else
		WRITE_VREG(AV_SCRATCH_N, 0);
#endif
	if (hw->m_ins_flag) {
		if (vdec_frame_based(hw_to_vdec(hw)))
			WRITE_VREG(DECODE_MODE, DECODE_MODE_MULTI_FRAMEBASE);
		else {
			if (hw->decode_status_skip_pic_done_flag) {
				WRITE_VREG(DECODE_CFG, hw->decode_decode_cont_start_code);
				WRITE_VREG(DECODE_MODE, DECODE_MODE_MULTI_STREAMBASE_CONT);
			} else
				WRITE_VREG(DECODE_MODE, DECODE_MODE_MULTI_STREAMBASE);
		}
		WRITE_VREG(DECODE_LMEM_BUF_ADR, (u32)hw->lmem_phy_addr);
	} else
		WRITE_VREG(DECODE_MODE, DECODE_MODE_SINGLE);

	if (ins_udebug_flag[DECODE_ID(hw)] &&
		(ins_udebug_flag[DECODE_ID(hw)] >> 16) == hw->decode_pic_count) {
		WRITE_VREG(DECODE_STOP_POS,
			ins_udebug_flag[DECODE_ID(hw)] & 0xffff);
	}
	else
		WRITE_VREG(DECODE_STOP_POS, udebug_flag);
	hw->old_udebug_flag = udebug_flag;

	return r;
}


#ifdef AVSP_LONG_CABAC
static unsigned char es_write_addr[MAX_CODED_FRAME_SIZE]  __aligned(64);
#endif
static void vavs_local_init(struct vdec_avs_hw_s *hw)
{
	int i;

	hw->vf_buf_num_used = vf_buf_num;

	hw->vavs_ratio = hw->vavs_amstream_dec_info.ratio;

	hw->avi_flag = (unsigned long) hw->vavs_amstream_dec_info.param;

	hw->frame_width = hw->frame_height = hw->frame_dur = hw->frame_prog = 0;

	hw->throw_pb_flag = 1;

	hw->total_frame = 0;
	hw->saved_resolution = 0;
	hw->next_pts = 0;

#ifdef DEBUG_PTS
	hw->pts_hit = hw->pts_missed = hw->pts_i_hit = hw->pts_i_missed = 0;
#endif
	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->recycle_q);
	INIT_KFIFO(hw->newframe_q);

	hw->refs[0] = -1;
	hw->refs[1] = -1;

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hw->vfpool[i].vf;

		hw->vfpool[i].vf.index = hw->vf_buf_num_used;
		hw->vfpool[i].vf.bufWidth = 1920;
		hw->vfpool[i].detached = 0;
		kfifo_put(&hw->newframe_q, vf);
	}
	for (i = 0; i < hw->vf_buf_num_used; i++)
		hw->vfbuf_use[i] = 0;

	if (hw->recover_flag == 1)
		return;

	if (hw->mm_blk_handle) {
		pr_info("decoder_bmmu_box_free\n");
		decoder_bmmu_box_free(hw->mm_blk_handle);
		hw->mm_blk_handle = NULL;
	}

	hw->mm_blk_handle = decoder_bmmu_box_alloc_box(
		DRIVER_NAME,
		0,
		MAX_BMMU_BUFFER_NUM,
		4 + PAGE_SHIFT,
		CODEC_MM_FLAGS_CMA_CLEAR |
		CODEC_MM_FLAGS_FOR_VDECODER,
		BMMU_ALLOC_FLAGS_WAITCLEAR);
	if (hw->mm_blk_handle == NULL)
		pr_info("Error, decoder_bmmu_box_alloc_box fail\n");

}

static int vavs_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;
	struct vdec_avs_hw_s *hw = (struct vdec_avs_hw_s *)op_arg;

	spin_lock_irqsave(&lock, flags);
	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hw->newframe_q);
	states->buf_avail_num = kfifo_len(&hw->display_q);
	states->buf_recycle_num = kfifo_len(&hw->recycle_q);
	if (step == 2)
		states->buf_avail_num = 0;
	spin_unlock_irqrestore(&lock, flags);
	return 0;
}

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
static void vavs_ppmgr_reset(void)
{
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_RESET, NULL);

	vavs_local_init(ghw);

	pr_info("vavs: vf_ppmgr_reset\n");
}
#endif

static void vavs_local_reset(struct vdec_avs_hw_s *hw)
{
	mutex_lock(&vavs_mutex);
	hw->recover_flag = 1;
	pr_info("error, local reset\n");
	amvdec_stop();
	msleep(100);
	avs_vf_notify_receiver(hw, PROVIDER_NAME, VFRAME_EVENT_PROVIDER_RESET, NULL);
	vavs_local_init(hw);
	vavs_recover(hw);

#ifdef ENABLE_USER_DATA
	reset_userdata_fifo(1);
#endif

	amvdec_start();
	hw->recover_flag = 0;

	mutex_unlock(&vavs_mutex);
}

static void vavs_fatal_error_handler(struct work_struct *work)
{
	struct vdec_avs_hw_s *hw =
	container_of(work, struct vdec_avs_hw_s, fatal_error_wd_work);
	if (debug & AVS_DEBUG_OLD_ERROR_HANDLE) {
		mutex_lock(&vavs_mutex);
		pr_info("vavs fatal error reset !\n");
		amvdec_stop();
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vavs_ppmgr_reset();
#else
		vf_light_unreg_provider(&vavs_vf_prov);
		vavs_local_init(hw);
		vf_reg_provider(&vavs_vf_prov);
#endif
		vavs_recover(hw);
		amvdec_start();
		mutex_unlock(&vavs_mutex);
	} else {
		pr_info("avs fatal_error_handler\n");
		vavs_local_reset(hw);
	}
	atomic_set(&hw->error_handler_run, 0);
}

static void vavs_notify_work(struct work_struct *work)
{
	struct vdec_avs_hw_s *hw =
	container_of(work, struct vdec_avs_hw_s, notify_work);
	if (hw->fr_hint_status == VDEC_NEED_HINT) {
		avs_vf_notify_receiver(hw, PROVIDER_NAME ,
			VFRAME_EVENT_PROVIDER_FR_HINT ,
			(void *)((unsigned long)hw->frame_dur));
		hw->fr_hint_status = VDEC_HINTED;
	}
	return;
}

static void avs_set_clk(struct work_struct *work)
{
	struct vdec_avs_hw_s *hw =
	container_of(work, struct vdec_avs_hw_s, set_clk_work);
	if (hw->frame_dur > 0 && hw->saved_resolution !=
		hw->frame_width * hw->frame_height * (96000 / hw->frame_dur)) {
		int fps = 96000 / hw->frame_dur;

		hw->saved_resolution = hw->frame_width * hw->frame_height * fps;
		if (firmware_sel == 0 &&
			(debug & AVS_DEBUG_USE_FULL_SPEED)) {
			vdec_source_changed(VFORMAT_AVS, 4096, 2048, 60);
		} else {
			vdec_source_changed(VFORMAT_AVS,
				hw->frame_width, hw->frame_height, fps);
		}

	}
}

#ifdef DEBUG_MULTI_WITH_AUTOMODE
int delay_count = 0;
#endif
static void vavs_put_timer_func(struct timer_list *arg)
{
	struct vdec_avs_hw_s *hw = container_of(arg,
		struct vdec_avs_hw_s, recycle_timer);
	struct timer_list *timer = &hw->recycle_timer;

#ifndef HANDLE_AVS_IRQ
	vavs_isr();
#endif
#ifdef DEBUG_MULTI_WITH_AUTOMODE
	if (delay_count > 0) {
		if (delay_count == 1)
			amvdec_start();
		delay_count--;
	}
#endif
	if (READ_VREG(AVS_SOS_COUNT)) {
		if (!error_recovery_mode) {
			if (!atomic_read(&hw->error_handler_run)) {
				atomic_set(&hw->error_handler_run, 1);
				pr_info("AVS_SOS_COUNT = %d\n",
					READ_VREG(AVS_SOS_COUNT));
				pr_info("WP = 0x%x, RP = 0x%x, LEVEL = 0x%x, AVAIL = 0x%x, CUR_PTR = 0x%x\n",
					READ_VREG(VLD_MEM_VIFIFO_WP),
					READ_VREG(VLD_MEM_VIFIFO_RP),
					READ_VREG(VLD_MEM_VIFIFO_LEVEL),
					READ_VREG(VLD_MEM_VIFIFO_BYTES_AVAIL),
					READ_VREG(VLD_MEM_VIFIFO_CURR_PTR));
				schedule_work(&hw->fatal_error_wd_work);
			}
		}
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
	if ((hw->ucode_pause_pos != 0) &&
		(hw->ucode_pause_pos != 0xffffffff) &&
		udebug_pause_pos != hw->ucode_pause_pos) {
		hw->ucode_pause_pos = 0;
		WRITE_VREG(DEBUG_REG1, 0);
	}

	if (!kfifo_is_empty(&hw->recycle_q) && (READ_VREG(AVS_BUFFERIN) == 0)) {
		struct vframe_s *vf;

		if (kfifo_get(&hw->recycle_q, &vf)) {
			if ((vf->index < hw->vf_buf_num_used) &&
			 (--hw->vfbuf_use[vf->index] == 0)) {
				debug_print(hw, PRINT_FLAG_DECODING,
					"%s WRITE_VREG(AVS_BUFFERIN, 0x%x) for vf index of %d\n",
					__func__,
					~(1 << vf->index), vf->index);
				WRITE_VREG(AVS_BUFFERIN, ~(1 << vf->index));
				vf->index = hw->vf_buf_num_used;
			}
				kfifo_put(&hw->newframe_q,
						  (const struct vframe_s *)vf);
		}

	}

	schedule_work(&hw->set_clk_work);

	timer->expires = jiffies + PUT_INTERVAL;

	add_timer(timer);
}

#ifdef AVSP_LONG_CABAC

static void long_cabac_do_work(struct work_struct *work)
{
	int status = 0;
	struct vdec_avs_hw_s *hw = gw;

	mutex_lock(&vavs_mutex);
	long_cabac_busy = 1;
	while (READ_VREG(LONG_CABAC_REQ)) {
		if (process_long_cabac() < 0) {
			status = -1;
			break;
		}
	}
	long_cabac_busy = 0;
	mutex_unlock(&vavs_mutex);

	if (status < 0) {
		pr_info("transcoding error, local reset\r\n");
		vavs_local_reset(hw);
	}

}
#endif

#ifdef AVSP_LONG_CABAC
static void init_avsp_long_cabac_buf(void)
{
	es_write_addr_virt = &es_write_addr[0];
	if (es_write_addr_virt == NULL) {
		pr_err("%s: failed to alloc es_write_addr_virt buffer\n", __func__);
		return;
	}

	es_write_addr_phy = dma_map_single(amports_get_dma_device(),
			es_write_addr_virt,
			MAX_CODED_FRAME_SIZE, DMA_BIDIRECTIONAL);
	if (dma_mapping_error(amports_get_dma_device(), es_write_addr_phy)) {
		pr_err("%s: failed to map es_write_addr_virt buffer\n", __func__);
		es_write_addr_virt = NULL;
		return;
	}

#ifdef BITSTREAM_READ_TMP_NO_CACHE
	bitstream_read_tmp =
		(void *)dma_alloc_coherent(amports_get_dma_device(),
			SVA_STREAM_BUF_SIZE, &bitstream_read_tmp_phy,
			 GFP_KERNEL);

#else

	bitstream_read_tmp = kmalloc(SVA_STREAM_BUF_SIZE, GFP_KERNEL);

	if (bitstream_read_tmp == NULL) {
		pr_err("%s: failed to alloc bitstream_read_tmp buffer\n",
			__func__);
		return;
	}

	bitstream_read_tmp_phy = dma_map_single(amports_get_dma_device(),
			bitstream_read_tmp,
			SVA_STREAM_BUF_SIZE, DMA_FROM_DEVICE);
	if (dma_mapping_error(amports_get_dma_device(),
			bitstream_read_tmp_phy)) {
		pr_err("%s: failed to map rpm buffer\n", __func__);
		kfree(bitstream_read_tmp);
		bitstream_read_tmp = NULL;
		return;
	}
#endif
}
#endif


static s32 vavs_init(struct vdec_avs_hw_s *hw)
{
	int ret, size = -1;
	struct firmware_s *fw;
	u32 fw_size = 0x1000 * 16;

	fw = vmalloc(sizeof(struct firmware_s) + fw_size);
	if (IS_ERR_OR_NULL(fw))
		return -ENOMEM;

	pr_info("vavs_init\n");

	vavs_local_init(hw);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXM)
		size = get_firmware_data(VIDEO_DEC_AVS_MULTI, fw->data);
	else {
		if (firmware_sel == 1)
			size = get_firmware_data(VIDEO_DEC_AVS_NOCABAC, fw->data);
#ifdef AVSP_LONG_CABAC
		else {
			init_avsp_long_cabac_buf();
			size = get_firmware_data(VIDEO_DEC_AVS_MULTI, fw->data);
		}
#endif
	}

	if (size < 0) {
		amvdec_disable();
		pr_err("get firmware fail.");
		vfree(fw);
		return -1;
	}

	fw->len = size;
	hw->fw = fw;

	if (hw->m_ins_flag) {
		timer_setup(&hw->check_timer, check_timer_func, 0);
		hw->check_timer.expires = jiffies + CHECK_INTERVAL;

		hw->stat |= STAT_TIMER_ARM;

		INIT_WORK(&hw->work, vavs_work);

		hw->fw = fw;
		return 0;
	}

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXM)
		ret = amvdec_loadmc_ex(VFORMAT_AVS, NULL, fw->data);
	else if (firmware_sel == 1)
		ret = amvdec_loadmc_ex(VFORMAT_AVS, "avs_no_cabac", fw->data);
	else
		ret = amvdec_loadmc_ex(VFORMAT_AVS, NULL, fw->data);

	if (ret < 0) {
		amvdec_disable();
		pr_err("AVS: the %s fw loading failed, err: %x\n",
			tee_enabled() ? "TEE" : "local", ret);
		return -EBUSY;
	}

	hw->stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	ret = vavs_prot_init(hw);
	if (ret < 0)
		return ret;

#ifdef HANDLE_AVS_IRQ
	if (vdec_request_irq(VDEC_IRQ_1, vavs_isr,
			"vavs-irq", (void *)hw)) {
		amvdec_disable();
		pr_info("vavs irq register error.\n");
		return -ENOENT;
	}
#endif

	hw->stat |= STAT_ISR_REG;

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
	vf_provider_init(&vavs_vf_prov, PROVIDER_NAME, &vavs_vf_provider, hw);
	vf_reg_provider(&vavs_vf_prov);
	avs_vf_notify_receiver(hw, PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
#else
	vf_provider_init(&vavs_vf_prov, PROVIDER_NAME, &vavs_vf_provider, hw);
	vf_reg_provider(&vavs_vf_prov);
#endif

	if (hw->vavs_amstream_dec_info.rate != 0) {
		if (!hw->is_reset)
			avs_vf_notify_receiver(hw, PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_FR_HINT,
					(void *)((unsigned long)
					hw->vavs_amstream_dec_info.rate));
		hw->fr_hint_status = VDEC_HINTED;
	} else
		hw->fr_hint_status = VDEC_NEED_HINT;

	hw->stat |= STAT_VF_HOOK;

	timer_setup(&hw->recycle_timer, vavs_put_timer_func, 0);
	//hw->recycle_timer.data = (ulong)(hw);
	//hw->recycle_timer.function = vavs_put_timer_func;
	hw->recycle_timer.expires = jiffies + PUT_INTERVAL;

	add_timer(&hw->recycle_timer);

	hw->stat |= STAT_TIMER_ARM;

#ifdef AVSP_LONG_CABAC
	if (firmware_sel == 0)
		INIT_WORK(&long_cabac_wd_work, long_cabac_do_work);
#endif
	vdec_source_changed(VFORMAT_AVS,
					1920, 1080, 30);
#ifdef DEBUG_MULTI_WITH_AUTOMODE
	if (start_decoding_delay == 0)
		amvdec_start();
	else
		delay_count = start_decoding_delay/10;
#else
	amvdec_start();
#endif
	hw->stat |= STAT_VDEC_RUN;
	return 0;
}

static int amvdec_avs_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_avs_hw_s *hw = NULL;

	if (pdata == NULL) {
		pr_info("amvdec_avs memory resource undefined.\n");
		return -EFAULT;
	}

	hw = (struct vdec_avs_hw_s *)vzalloc(sizeof(struct vdec_avs_hw_s));
	if (hw == NULL) {
		pr_info("\nammvdec_avs decoder driver alloc failed\n");
		return -ENOMEM;
	}
	pdata->private = hw;
	ghw = hw;
	atomic_set(&hw->error_handler_run, 0);
	hw->m_ins_flag = 0;

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXM || disable_longcabac_trans)
		firmware_sel = 1;

	if (firmware_sel == 1) {
#ifndef USE_DYNAMIC_BUF_NUM
		vf_buf_num = 4;
#endif
		canvas_base = 0;
		canvas_num = 3;
	} else {

		canvas_base = 128;
		canvas_num = 2; /*NV21*/
	}


	if (pdata->sys_info)
		hw->vavs_amstream_dec_info = *pdata->sys_info;

	pr_info("%s (%d,%d) %d\n", __func__, hw->vavs_amstream_dec_info.width,
		   hw->vavs_amstream_dec_info.height, hw->vavs_amstream_dec_info.rate);

	pdata->dec_status = vavs_dec_status;
	pdata->set_isreset = vavs_set_isreset;
	hw->is_reset = 0;

	pdata->user_data_read = NULL;
	pdata->reset_userdata_fifo = NULL;

	vavs_vdec_info_init(hw);

#ifdef ENABLE_USER_DATA
	if (NULL == hw->user_data_buffer) {
		hw->user_data_buffer =
			dma_alloc_coherent(amports_get_dma_device(),
				USER_DATA_SIZE,
				&hw->user_data_buffer_phys, GFP_KERNEL);
		if (!hw->user_data_buffer) {
			pr_info("%s: Can not allocate hw->user_data_buffer\n",
				   __func__);
			return -ENOMEM;
		}
		pr_debug("hw->user_data_buffer = 0x%p, hw->user_data_buffer_phys = 0x%x\n",
			hw->user_data_buffer, (u32)hw->user_data_buffer_phys);
	}
#endif
	INIT_WORK(&hw->set_clk_work, avs_set_clk);
	if (vavs_init(hw) < 0) {
		pr_info("amvdec_avs init failed.\n");
		kfree(hw->gvs);
		hw->gvs = NULL;
		pdata->dec_status = NULL;
		if (hw->fw)
			vfree(hw->fw);
		hw->fw = NULL;
		return -ENODEV;
	}
	/*vdec = pdata;*/

	INIT_WORK(&hw->fatal_error_wd_work, vavs_fatal_error_handler);
	atomic_set(&hw->error_handler_run, 0);
#ifdef ENABLE_USER_DATA
	INIT_WORK(&hw->userdata_push_work, userdata_push_do_work);
#endif
	INIT_WORK(&hw->notify_work, vavs_notify_work);

	return 0;
}

static int amvdec_avs_remove(struct platform_device *pdev)
{
	struct vdec_avs_hw_s *hw = ghw;

	cancel_work_sync(&hw->fatal_error_wd_work);
	atomic_set(&hw->error_handler_run, 0);
#ifdef ENABLE_USER_DATA
	cancel_work_sync(&hw->userdata_push_work);
#endif
	cancel_work_sync(&hw->notify_work);
	cancel_work_sync(&hw->set_clk_work);
	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}

	if (hw->stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_1, (void *)vavs_dec_id);
		hw->stat &= ~STAT_ISR_REG;
	}

	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->recycle_timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}
#ifdef AVSP_LONG_CABAC
	if (firmware_sel == 0) {
		mutex_lock(&vavs_mutex);
		cancel_work_sync(&long_cabac_wd_work);
		mutex_unlock(&vavs_mutex);

		if (es_write_addr_virt) {
#if 0
			codec_mm_free_for_dma("vavs", es_write_addr_phy);
#else
			dma_unmap_single(amports_get_dma_device(),
				es_write_addr_phy,
				MAX_CODED_FRAME_SIZE, DMA_FROM_DEVICE);
			/*kfree(es_write_addr_virt);*/
			es_write_addr_virt = NULL;
#endif
		}

#ifdef BITSTREAM_READ_TMP_NO_CACHE
		if (bitstream_read_tmp) {
			dma_free_coherent(amports_get_dma_device(),
				SVA_STREAM_BUF_SIZE, bitstream_read_tmp,
				bitstream_read_tmp_phy);
			bitstream_read_tmp = NULL;
		}
#else
		if (bitstream_read_tmp) {
			dma_unmap_single(amports_get_dma_device(),
				bitstream_read_tmp_phy,
				SVA_STREAM_BUF_SIZE, DMA_FROM_DEVICE);
			kfree(bitstream_read_tmp);
			bitstream_read_tmp = NULL;
		}
#endif
	}
#endif
	if (hw->stat & STAT_VF_HOOK) {
		if (hw->fr_hint_status == VDEC_HINTED && !hw->is_reset)
			avs_vf_notify_receiver(hw, PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_FR_END_HINT, NULL);
		hw->fr_hint_status = VDEC_NO_NEED_HINT;
		vf_unreg_provider(&vavs_vf_prov);
		hw->stat &= ~STAT_VF_HOOK;
	}

#ifdef ENABLE_USER_DATA
	if (hw->user_data_buffer != NULL) {
		dma_free_coherent(
			amports_get_dma_device(),
			USER_DATA_SIZE,
			hw->user_data_buffer,
			hw->user_data_buffer_phys);
		hw->user_data_buffer = NULL;
		hw->user_data_buffer_phys = 0;
	}
#endif

	if (hw->fw) {
		vfree(hw->fw);
		hw->fw = NULL;
	}

	//amvdec_disable();
	//vdec_disable_DMC(NULL);

	hw->pic_type = 0;
	if (hw->mm_blk_handle) {
		decoder_bmmu_box_free(hw->mm_blk_handle);
		hw->mm_blk_handle = NULL;
	}
#ifdef DEBUG_PTS
	pr_debug("pts hit %d, pts missed %d, i hit %d, missed %d\n", hw->pts_hit,
		   hw->pts_missed, hw->pts_i_hit, hw->pts_i_missed);
	pr_debug("total frame %d, hw->avi_flag %d, rate %d\n", hw->total_frame, hw->avi_flag,
		   hw->vavs_amstream_dec_info.rate);
#endif
	kfree(hw->gvs);
	hw->gvs = NULL;
	vfree(hw);
	return 0;
}

/****************************************/
#if 0
static struct platform_driver amvdec_avs_driver = {
	.probe = amvdec_avs_probe,
	.remove = amvdec_avs_remove,
	.driver = {
		.name = DRIVER_NAME,
	}
};
#endif

static void recycle_frames(struct vdec_avs_hw_s *hw);

static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	struct vdec_avs_hw_s *hw =
	(struct vdec_avs_hw_s *)vdec->private;
	int ret = 1;
#ifdef DEBUG_MULTI_FRAME_INS
	if ((DECODE_ID(hw) == 0) && run_count[0] > run_count[1] &&
		run_count[1] < max_run_count[1])
		return 0;

	if ((DECODE_ID(hw) == 1) && run_count[1] >= run_count[0] &&
		run_count[0] < max_run_count[0])
		return 0;

	if (max_run_count[DECODE_ID(hw)] > 0 &&
		run_count[DECODE_ID(hw)] >= max_run_count[DECODE_ID(hw)])
		return 0;
#endif
	if (vdec_stream_based(vdec) && (hw->init_flag == 0)
		&& pre_decode_buf_level != 0) {
		u32 rp, wp, level;

		rp = STBUF_READ(&vdec->vbuf, get_rp);
		wp = STBUF_READ(&vdec->vbuf, get_wp);
		if (wp < rp)
			level = vdec->input.size + wp - rp;
		else
			level = wp - rp;

		if (level < pre_decode_buf_level) {
			hw->not_run_ready++;
			return 0;
		}
	}

	recycle_frames(hw);

	if (!is_enough_free_buffer(hw)) {
		hw->buffer_not_ready++;
		return 0;
	}

	if (again_threshold > 0 &&
		hw->pre_parser_wr_ptr != 0 &&
		hw->again_flag &&
		(!vdec_frame_based(vdec))) {
		u32 parser_wr_ptr =
			STBUF_READ(&vdec->vbuf, get_rp);
		if (parser_wr_ptr >= hw->pre_parser_wr_ptr &&
			(parser_wr_ptr - hw->pre_parser_wr_ptr) <
			again_threshold) {
			int r = vdec_sync_input(vdec);
				debug_print(hw, PRINT_FLAG_VFRAME_DETAIL,
					"%s buf lelvel:%x\n",  __func__, r);
			ret = 0;
		}
	}

	if (ret)
		hw->not_run_ready = 0;
	else
		hw->not_run_ready++;

	if (ret != 0) {
		if (vdec->parallel_dec == 1)
			return (unsigned long)(CORE_MASK_VDEC_1);
		else
			return (unsigned long)(CORE_MASK_VDEC_1 | CORE_MASK_HEVC);
	} else
		return 0;
}

static void vavs_work(struct work_struct *work)
{
	struct vdec_avs_hw_s *hw =
	container_of(work, struct vdec_avs_hw_s, work);
	struct vdec_s *vdec = hw_to_vdec(hw);
	if (hw->dec_result != DEC_RESULT_AGAIN)
		debug_print(hw, PRINT_FLAG_RUN_FLOW,
	"ammvdec_avs: vavs_work,result=%d,status=%d\n",
	hw->dec_result, hw_to_vdec(hw)->next_status);
	hw->again_flag = 0;
	if (hw->dec_result == DEC_RESULT_USERDATA) {
		userdata_push_process(hw);
		return;
	} else if (hw->dec_result == DEC_RESULT_DONE) {

		if (!hw->ctx_valid)
			hw->ctx_valid = 1;
#ifdef DEBUG_MULTI_FRAME_INS
			msleep(delay);
#endif
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
	} else if (hw->dec_result == DEC_RESULT_AGAIN
	&& (hw_to_vdec(hw)->next_status !=
		VDEC_STATUS_DISCONNECTED)) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		hw->again_flag = 1;
		if (!vdec_has_more_input(hw_to_vdec(hw))) {
			hw->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hw->work);
			return;
		}
	}  else if (hw->dec_result == DEC_RESULT_GET_DATA
		&& (hw_to_vdec(hw)->next_status !=
		VDEC_STATUS_DISCONNECTED)) {
		if (!vdec_has_more_input(hw_to_vdec(hw))) {
			hw->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hw->work);
			return;
		}
		debug_print(hw, PRINT_FLAG_VLD_DETAIL,
		"%s DEC_RESULT_GET_DATA %x %x %x\n",
		__func__,
		READ_VREG(VLD_MEM_VIFIFO_LEVEL),
		READ_VREG(VLD_MEM_VIFIFO_WP),
		READ_VREG(VLD_MEM_VIFIFO_RP));
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
		vdec_clean_input(hw_to_vdec(hw));
		return;
	} else if (hw->dec_result == DEC_RESULT_FORCE_EXIT) {
		debug_print(hw, PRINT_FLAG_ERROR,
		"%s: force exit\n", __func__);
		if (hw->stat & STAT_ISR_REG) {
			amvdec_stop();
			/*disable mbox interrupt */
			WRITE_VREG(ASSIST_MBOX1_MASK, 0);
			vdec_free_irq(VDEC_IRQ_1, (void *)hw);
			hw->stat &= ~STAT_ISR_REG;
		}
	} else if (hw->dec_result == DEC_RESULT_EOS) {
		debug_print(hw, PRINT_FLAG_DECODING,
			"%s: end of stream\n", __func__);
		if (hw->stat & STAT_VDEC_RUN) {
			amvdec_stop();
			hw->stat &= ~STAT_VDEC_RUN;
		}
		hw->eos = 1;
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
		vdec_clean_input(hw_to_vdec(hw));
	}
	if (hw->stat & STAT_VDEC_RUN) {
#if DEBUG_MULTI_FLAG == 1
#else
		amvdec_stop();
#endif
		hw->stat &= ~STAT_VDEC_RUN;
	}
	/*wait_vmmpeg12_search_done(hw);*/
	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->check_timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}
	if (hw->dec_result == DEC_RESULT_DONE)
		hw->buf_recycle_status = 0;
	debug_print(hw, PRINT_FLAG_RUN_FLOW, "work end %d\n", hw->dec_result);
	if (vdec->parallel_dec == 1)
		vdec_core_finish_run(hw_to_vdec(hw), CORE_MASK_VDEC_1);
	else
		vdec_core_finish_run(hw_to_vdec(hw), CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

	if (hw->vdec_cb) {
		hw->vdec_cb(hw_to_vdec(hw), hw->vdec_cb_arg);
		debug_print(hw, 0x80000,
		"%s:\n", __func__);
	}
}


static void reset_process_time(struct vdec_avs_hw_s *hw)
{
	if (!hw->m_ins_flag)
		return;
	if (hw->start_process_time) {
		unsigned process_time =
			1000 * (jiffies - hw->start_process_time) / HZ;
		hw->start_process_time = 0;
		if (process_time > max_process_time[DECODE_ID(hw)])
			max_process_time[DECODE_ID(hw)] = process_time;
	}
}
static void start_process_time(struct vdec_avs_hw_s *hw)
{
	hw->decode_timeout_count = 2;
	hw->start_process_time = jiffies;
}

static void handle_decoding_error(struct vdec_avs_hw_s *hw)
{
	int i;
	unsigned long flags;
	struct vframe_s *vf;
	spin_lock_irqsave(&lock, flags);
	for (i = 0; i < VF_POOL_SIZE; i++) {
		vf = &hw->vfpool[i].vf;
		if (vf->index < hw->vf_buf_num_used) {
			hw->vfpool[i].detached = 1;
			hw->vfbuf_use[vf->index] = 0;
		}
	}
	for (i = 0; i < VF_BUF_NUM_MAX; i++) {
		hw->ref_use[i] = 0;
		hw->buf_use[i] = 0;
	}
	hw->refs[0] = -1;
	hw->refs[1] = -1;
	if (error_handle_policy & 0x2) {
		while (!kfifo_is_empty(&hw->display_q)) {
			if (kfifo_get(&hw->display_q, &vf)) {
				if (buf_of_vf(vf)->detached !=0) {
					debug_print(hw, PRINT_FLAG_DECODING,
						"%s recycle %d => newframe_q\n",
						__func__,
						vf->index);
					vf->index = hw->vf_buf_num_used;
					buf_of_vf(vf)->detached = 0;
					kfifo_put(&hw->newframe_q,
						(const struct vframe_s *)vf);
				}
			}

		}
	}
	clear_pts_buf(hw);
	hw->decode_pic_count = 0;
	hw->reset_decode_flag = 1;
	hw->pre_parser_wr_ptr = 0;
	hw->buf_status = 0;
	hw->throw_pb_flag = 1;
	spin_unlock_irqrestore(&lock, flags);
}

static void timeout_process(struct vdec_avs_hw_s *hw)
{
	struct vdec_s *vdec = hw_to_vdec(hw);
	amvdec_stop();
	if (error_handle_policy & 0x1) {
		handle_decoding_error(hw);
	} else {
		vavs_save_regs(hw);

		//if (hw->decode_pic_count == 0)
		hw->decode_pic_count++;
		if ((hw->decode_pic_count & 0xffff) == 0) {
		/*make ucode do not handle it as first picture*/
			hw->decode_pic_count++;
		}
	}
	hw->dec_result = DEC_RESULT_DONE;

	debug_print(hw, PRINT_FLAG_ERROR,
	"%s decoder timeout, status=%d, level=%d, bit_cnt=0x%x\n",
	__func__, vdec->status, READ_VREG(VLD_MEM_VIFIFO_LEVEL), READ_VREG(VIFF_BIT_CNT));
	reset_process_time(hw);
	vdec_schedule_work(&hw->work);
}


static void recycle_frame_bufferin(struct vdec_avs_hw_s *hw)
{
	if (!kfifo_is_empty(&hw->recycle_q) && (READ_VREG(AVS_BUFFERIN) == 0)) {
		struct vframe_s *vf;

		if (kfifo_get(&hw->recycle_q, &vf)) {
			if (buf_of_vf(vf)->detached) {
				debug_print(hw, 0,
					"%s recycle detached vf, index=%d detched %d used %d\n",
					__func__, vf->index,
					buf_of_vf(vf)->detached,
					hw->vfbuf_use[vf->index]);
			}
			if ((vf->index < hw->vf_buf_num_used) &&
				(buf_of_vf(vf)->detached == 0) &&
			 (--hw->vfbuf_use[vf->index] == 0)) {
				hw->buf_recycle_status |= (1 << vf->index);
				WRITE_VREG(AVS_BUFFERIN, ~(1 << vf->index));
				debug_print(hw, PRINT_FLAG_DECODING,
					"%s WRITE_VREG(AVS_BUFFERIN, 0x%x) for vf index of %d => buf_recycle_status 0x%x\n",
					__func__,
					READ_VREG(AVS_BUFFERIN), vf->index,
					hw->buf_recycle_status);
			}
			vf->index = hw->vf_buf_num_used;
			buf_of_vf(vf)->detached = 0;
			kfifo_put(&hw->newframe_q,
					  (const struct vframe_s *)vf);
		}

	}

}

static void recycle_frames(struct vdec_avs_hw_s *hw)
{
	while (!kfifo_is_empty(&hw->recycle_q)) {
		struct vframe_s *vf;

		if (kfifo_get(&hw->recycle_q, &vf)) {
			if (buf_of_vf(vf)->detached) {
				debug_print(hw, 0,
					"%s recycle detached vf, index=%d detched %d used %d\n",
					__func__, vf->index,
					buf_of_vf(vf)->detached,
					hw->vfbuf_use[vf->index]);
			}


			if ((vf->index < hw->vf_buf_num_used) &&
				(buf_of_vf(vf)->detached == 0) &&
			 (--hw->vfbuf_use[vf->index] == 0)) {
				hw->buf_use[vf->index] --;
				hw->buf_recycle_status |= (1 << vf->index);
				debug_print(hw, PRINT_FLAG_DECODING,
					"%s for vf index of %d => buf_recycle_status 0x%x\n",
					__func__,
					vf->index,
					hw->buf_recycle_status);
			}
			vf->index = hw->vf_buf_num_used;
			buf_of_vf(vf)->detached = 0;
			kfifo_put(&hw->newframe_q,
				(const struct vframe_s *)vf);
		}

	}

}


static void check_timer_func(struct timer_list *timer)
{
	struct vdec_avs_hw_s *hw = container_of(timer,
		struct vdec_avs_hw_s, check_timer);
	struct vdec_s *vdec = hw_to_vdec(hw);
	unsigned int timeout_val = decode_timeout_val;
	unsigned long flags;

	if (hw->m_ins_flag &&
		(debug &
		DEBUG_WAIT_DECODE_DONE_WHEN_STOP) == 0 &&
		vdec->next_status ==
		VDEC_STATUS_DISCONNECTED) {
		hw->dec_result = DEC_RESULT_FORCE_EXIT;
		vdec_schedule_work(&hw->work);
		debug_print(hw,
			0, "vdec requested to be disconnected\n");
		return;
	}

	/*recycle*/
	if (!hw->m_ins_flag) {
		spin_lock_irqsave(&lock, flags);
		recycle_frame_bufferin(hw);
		spin_unlock_irqrestore(&lock, flags);
	}

	if (hw->m_ins_flag) {
		if ((READ_VREG(AV_SCRATCH_5) & 0xf) != 0 &&
			(READ_VREG(AV_SCRATCH_5) & 0xff00) != 0){
			/*ucode buffer empty*/
			if ((kfifo_len(&hw->recycle_q) == 0) &&
				(kfifo_len(&hw->display_q) == 0)) {
				debug_print(hw,
					0, "AV_SCRATCH_5=0x%x, recover ucode buffer_status\n",
					READ_VREG(AV_SCRATCH_5));
				WRITE_VREG(AV_SCRATCH_5, 0x10);
				/*let ucode to recover buffer_status*/
			}
		}
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

	if (udebug_flag != hw->old_udebug_flag) {
		WRITE_VREG(DECODE_STOP_POS, udebug_flag);
		hw->old_udebug_flag = udebug_flag;
	}
	if (dbg_cmd != 0) {
		if (dbg_cmd == 1) {
			int r = vdec_sync_input(vdec);
			dbg_cmd = 0;
			pr_info(
				"vdec_sync_input=>0x%x, (lev %x, wp %x rp %x, prp %x, pwp %x)\n",
				r,
				READ_VREG(VLD_MEM_VIFIFO_LEVEL),
				READ_VREG(VLD_MEM_VIFIFO_WP),
				READ_VREG(VLD_MEM_VIFIFO_RP),
				STBUF_READ(&vdec->vbuf, get_rp),
				STBUF_READ(&vdec->vbuf, get_wp));
		}
	}

	if ((debug & DEBUG_FLAG_DISABLE_TIMEOUT) == 0 &&
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

	if (READ_VREG(AVS_SOS_COUNT)) {
		if (!error_recovery_mode) {
			amvdec_stop();
			if (error_handle_policy & 0x1) {
				handle_decoding_error(hw);
			} else {
				vavs_save_regs(hw);

				//if (hw->decode_pic_count == 0)
				hw->decode_pic_count++;
				if ((hw->decode_pic_count & 0xffff) == 0) {
				/*make ucode do not handle it as first picture*/
					hw->decode_pic_count++;
				}
			}
			hw->dec_result = DEC_RESULT_DONE;

			debug_print(hw, PRINT_FLAG_ERROR,
			"%s decoder error, status=%d, level=%d, AVS_SOS_COUNT=0x%x\n",
			__func__, vdec->status, READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(AVS_SOS_COUNT));
			reset_process_time(hw);
			vdec_schedule_work(&hw->work);
		}
	}

	if ((hw->ucode_pause_pos != 0) &&
		(hw->ucode_pause_pos != 0xffffffff) &&
		udebug_pause_pos != hw->ucode_pause_pos) {
		hw->ucode_pause_pos = 0;
		WRITE_VREG(DEBUG_REG1, 0);
	}

	if (vdec->next_status == VDEC_STATUS_DISCONNECTED) {
		hw->dec_result = DEC_RESULT_FORCE_EXIT;
		vdec_schedule_work(&hw->work);
		pr_info("vdec requested to be disconnected\n");
		return;
	}

	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
}

static int avs_hw_ctx_restore(struct vdec_avs_hw_s *hw)
{
	/*int r = 0;*/
	vavs_prot_init(hw);

	return 0;
}

static unsigned char get_data_check_sum
	(struct vdec_avs_hw_s *hw, int size)
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
void (*callback)(struct vdec_s *, void *),
		void *arg)
{
	struct vdec_avs_hw_s *hw =
	(struct vdec_avs_hw_s *)vdec->private;
	int save_reg;
	int size, ret;
	int i;
	if (!hw->vdec_pg_enable_flag) {
		hw->vdec_pg_enable_flag = 1;
		amvdec_enable();
	}
	save_reg = READ_VREG(POWER_CTL_VLD);
	/* reset everything except DOS_TOP[1] and APB_CBUS[0]*/
	debug_print(hw, PRINT_FLAG_RUN_FLOW,"run in\n");
	if (vdec_stream_based(vdec)) {
		hw->pre_parser_wr_ptr =
			STBUF_READ(&vdec->vbuf, get_wp);
	}
#if 1
#if DEBUG_MULTI_FLAG > 0
	if (hw->decode_pic_count == 0) {
#endif
	WRITE_VREG(DOS_SW_RESET0, 0xfffffff0);
	WRITE_VREG(DOS_SW_RESET0, 0);
	WRITE_VREG(POWER_CTL_VLD, save_reg);
	hw->run_count++;
	run_count[DECODE_ID(hw)] = hw->run_count;
	vdec_reset_core(vdec);
#if DEBUG_MULTI_FLAG > 0
	}
#endif
#else
	vdec_reset_core(vdec);
#endif
	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;

	size = vdec_prepare_input(vdec, &hw->chunk);
	if (debug & DEBUG_FLAG_PREPARE_MORE_INPUT) {
		if (size < start_decode_buf_level) {
			/*debug_print(hw, PRINT_FLAG_VLD_DETAIL,
				"DEC_RESULT_AGAIN %x %x %x\n",
				READ_VREG(VLD_MEM_VIFIFO_LEVEL),
				READ_VREG(VLD_MEM_VIFIFO_WP),
				READ_VREG(VLD_MEM_VIFIFO_RP));*/

			hw->input_empty++;
			hw->dec_result = DEC_RESULT_AGAIN;
			vdec_schedule_work(&hw->work);
			return;
		}
	} else {
		if (size < 0) {
			hw->input_empty++;
			hw->dec_result = DEC_RESULT_AGAIN;
			vdec_schedule_work(&hw->work);
			return;
		}
	}
	if (input_frame_based(vdec)) {
		u8 *data = NULL;

		if (!hw->chunk->block->is_mapped)
			data = codec_mm_vmap(hw->chunk->block->start +
				hw->chunk->offset, size);
		else
			data = ((u8 *)hw->chunk->block->start_virt) +
				hw->chunk->offset;

		if (debug & PRINT_FLAG_RUN_FLOW
			) {
			debug_print(hw, 0,
			"%s decode_pic_count %d buf_recycle_status 0x%x: size 0x%x sum 0x%x %02x %02x %02x %02x %02x %02x .. %02x %02x %02x %02x\n",
			__func__, hw->decode_pic_count,
			hw->buf_recycle_status,
			size, get_data_check_sum(hw, size),
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[size - 4],
			data[size - 3],	data[size - 2],
			data[size - 1]);
		}
		if (debug & PRINT_FRAMEBASE_DATA
			) {
			int jj;

			for (jj = 0; jj < size; jj++) {
				if ((jj & 0xf) == 0)
					debug_print(hw,
					PRINT_FRAMEBASE_DATA,
						"%06x:", jj);
				debug_print(hw,
				PRINT_FRAMEBASE_DATA,
					"%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					debug_print(hw,
					PRINT_FRAMEBASE_DATA,
						"\n");
			}
		}

		if (!hw->chunk->block->is_mapped)
			codec_mm_unmap_phyaddr(data);
	} else
		debug_print(hw, PRINT_FLAG_RUN_FLOW,
			"%s decode_pic_count %d buf_recycle_status 0x%x: %x %x %x %x %x size 0x%x\n",
			__func__,
			hw->decode_pic_count,
			hw->buf_recycle_status,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			STBUF_READ(&vdec->vbuf, get_rp),
			STBUF_READ(&vdec->vbuf, get_wp),
			size);


	hw->input_empty = 0;
	debug_print(hw, PRINT_FLAG_RUN_FLOW,
	"%s,%d, size=%d\n", __func__, __LINE__, size);

	/*vdec_enable_input(vdec);
		need run after VC1_CONTROL_REG is configured
	*/
	hw->init_flag = 1;

	if (hw->chunk)
		debug_print(hw, PRINT_FLAG_RUN_FLOW,
		"input chunk offset %d, size %d\n",
			hw->chunk->offset, hw->chunk->size);

	hw->dec_result = DEC_RESULT_NONE;
	/*vdec->mc_loaded = 0;*/
	if (vdec->mc_loaded) {
	/*firmware have load before,
	  and not changes to another.
	  ignore reload.
	*/
	} else {
		ret = amvdec_vdec_loadmc_buf_ex(VFORMAT_AVS, "avs_multi", vdec,
			hw->fw->data, hw->fw->len);
		if (ret < 0) {
			pr_err("[%d] %s: the %s fw loading failed, err: %x\n", vdec->id,
				hw->fw->name, tee_enabled() ? "TEE" : "local", ret);
			hw->dec_result = DEC_RESULT_FORCE_EXIT;
			vdec_schedule_work(&hw->work);
			return;
		}
		vdec->mc_loaded = 1;
		vdec->mc_type = VFORMAT_AVS;
	}
	debug_print(hw, PRINT_FLAG_BUFFER_DETAIL, "vfbuf_use:\n");
	for (i = 0; i < hw->vf_buf_num_used; i++)
		debug_print(hw, PRINT_FLAG_BUFFER_DETAIL, "%d: vf_buf_use %d\n",
			i, hw->vfbuf_use[i]);
	debug_print(hw, PRINT_FLAG_BUFFER_DETAIL, "ref_use:\n");
	for (i = 0; i < hw->vf_buf_num_used; i++)
		debug_print(hw, PRINT_FLAG_BUFFER_DETAIL, "%d: ref_use %d\n",
			i, hw->ref_use[i]);
	debug_print(hw, PRINT_FLAG_BUFFER_DETAIL, "buf_use:\n");
	for (i = 0; i < hw->vf_buf_num_used; i++)
		debug_print(hw, PRINT_FLAG_BUFFER_DETAIL, "%d: buf_use %d\n",
			i, hw->buf_use[i]);
	if (avs_hw_ctx_restore(hw) < 0) {
		hw->dec_result = DEC_RESULT_ERROR;
		debug_print(hw, PRINT_FLAG_ERROR,
		"ammvdec_avs: error HW context restore\n");
		vdec_schedule_work(&hw->work);
		return;
	}

	/*
		This configureation of VC1_CONTROL_REG will
		pop bits (even no data in the stream buffer) if input is enabled,
		so it can only be configured before vdec_enable_input() is called.
		So move this code from ucode to here
	*/
#define DISABLE_DBLK_HCMD   0
#define DISABLE_MC_HCMD 0
	WRITE_VREG(VC1_CONTROL_REG, (DISABLE_DBLK_HCMD<<6) |
		(DISABLE_MC_HCMD<<5) | (1 << 7) | (0xc <<8) | (1<<14));
	if (vdec_frame_based(vdec)) {
			size = hw->chunk->size +
				(hw->chunk->offset & (VDEC_FIFO_ALIGN - 1));
		}


	vdec_enable_input(vdec);
	/**/

	/*wmb();*/
	hw->stat |= STAT_MC_LOAD;
	hw->last_vld_level = 0;

	debug_print(hw, PRINT_FLAG_DECODING,
		"%s READ_VREG(AVS_BUFFERIN)=0x%x, recycle_q num %d\n",
		__func__, READ_VREG(AVS_BUFFERIN),
		kfifo_len(&hw->recycle_q));

	WRITE_VREG(VIFF_BIT_CNT, size * 8);
	if (hw->reset_decode_flag)
		WRITE_VREG(DECODE_STATUS, 0);
	else {
		recycle_frames(hw);
		avs_pts_check_in(hw,
			hw->decode_pic_count & 0xffff,
			hw->chunk);

		WRITE_VREG(DECODE_STATUS,
			(hw->decode_pic_count & 0xffff) |
			((~hw->buf_recycle_status) << 16));
	}

	hw->reset_decode_flag = 0;
	//hw->decode_status_skip_pic_done_flag = 0;
	start_process_time(hw);
#if DEBUG_MULTI_FLAG == 1
	if (hw->decode_pic_count > 0)
		WRITE_VREG(DECODE_STATUS, 0xff);
	else
#endif
	amvdec_start();
	hw->stat |= STAT_VDEC_RUN;

	hw->stat |= STAT_TIMER_ARM;

	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
}

static void reset(struct vdec_s *vdec)
{
}

static int prepare_display_buf(struct vdec_avs_hw_s *hw,
	struct pic_info_t *pic)
{
	struct vdec_s *vdec = hw_to_vdec(hw);
	struct vframe_s *vf = NULL;
	u32 reg = pic->buffer_info;
	bool force_interlaced_frame = false;
	u32 picture_type = pic->picture_type;
	u32 repeat_count = pic->repeat_cnt;
	unsigned int pts = pic->pts;
	unsigned int pts_valid = pic->pts_valid;
	unsigned int offset = pic->offset;
	u64 pts_us64 = pic->pts64;
	u32 buffer_index = pic->index;
	u32 dur;
	unsigned short decode_pic_count = pic->decode_pic_count;

	if ((dec_control & DEC_CONTROL_FLAG_FORCE_2500_1080P_INTERLACE)
		&& hw->frame_width == 1920 && hw->frame_height == 1080) {
			force_interlaced_frame = true;
	}

	if (reg & INTERLACE_FLAG || force_interlaced_frame) {	/* interlace */
			hw->throw_pb_flag = 0;

			debug_print(hw, PRINT_FLAG_VFRAME_DETAIL,
				"interlace, picture type %d\n",
					   picture_type);

			if (kfifo_get(&hw->newframe_q, &vf) == 0) {
				pr_info
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}
			set_frame_info(hw, vf, &dur);
			vf->bufWidth = 1920;
			hw->pic_type = 2;
			if ((picture_type == I_PICTURE) && pts_valid) {
				vf->pts = pts;
				vf->pts_us64 = pts_us64;
				if ((repeat_count > 1) && hw->avi_flag) {
					/* hw->next_pts = pts +
					 *	 (hw->vavs_amstream_dec_info.rate *
					 *	 repeat_count >> 1)*15/16;
					 */
					hw->next_pts =
						pts +
						(dur * repeat_count >> 1) *
						15 / 16;
				} else
					hw->next_pts = 0;
			} else {
				vf->pts = hw->next_pts;
				if (vf->pts == 0) {
					vf->pts_us64 = 0;
				}
				if ((repeat_count > 1) && hw->avi_flag) {
					/* vf->duration =
					 *	 hw->vavs_amstream_dec_info.rate *
					 *	 repeat_count >> 1;
					 */
					vf->duration = dur * repeat_count >> 1;
					if (hw->next_pts != 0) {
						hw->next_pts +=
							((vf->duration) -
							 ((vf->duration) >> 4));
					}
				} else {
					/* vf->duration =
					 *	 hw->vavs_amstream_dec_info.rate >> 1;
					 */
					vf->duration = dur >> 1;
					hw->next_pts = 0;
				}
			}
			vf->signal_type = 0;
			vf->index = buffer_index;
			vf->duration_pulldown = 0;
			if (force_interlaced_frame) {
				vf->type = VIDTYPE_INTERLACE_TOP;
			}else{
				vf->type =
				(reg & TOP_FIELD_FIRST_FLAG)
				? VIDTYPE_INTERLACE_TOP
				: VIDTYPE_INTERLACE_BOTTOM;
				}
#ifdef NV21
			vf->type |= VIDTYPE_VIU_NV21;
#endif
			if (hw->m_ins_flag) {
				vf->canvas0Addr = vf->canvas1Addr = -1;
				vf->plane_num = 2;

				vf->canvas0_config[0] = hw->canvas_config[buffer_index][0];
				vf->canvas0_config[1] = hw->canvas_config[buffer_index][1];

				vf->canvas1_config[0] = hw->canvas_config[buffer_index][0];
				vf->canvas1_config[1] = hw->canvas_config[buffer_index][1];
			} else
				vf->canvas0Addr = vf->canvas1Addr =
					index2canvas(buffer_index);
			vf->type_original = vf->type;

			debug_print(hw, PRINT_FLAG_VFRAME_DETAIL,
				"buffer_index %d, canvas addr %x\n",
					   buffer_index, vf->canvas0Addr);
			vf->pts = (pts_valid)?pts:0;
			//vf->pts_us64 = (pts_valid) ? pts_us64 : 0;
			hw->vfbuf_use[buffer_index]++;
			vf->mem_handle =
				decoder_bmmu_box_get_mem_handle(
					hw->mm_blk_handle,
					buffer_index);

			if (hw->m_ins_flag && vdec_frame_based(hw_to_vdec(hw)))
				set_vframe_pts(hw, decode_pic_count, vf);

			if (vdec_stream_based(vdec) && (!vdec->vbuf.use_ptsserv)) {
				vf->pts_us64 =
					(((u64)vf->duration << 32) & 0xffffffff00000000) | offset;
				vf->pts = 0;
			}

			debug_print(hw, PRINT_FLAG_PTS,
				"interlace1 vf->pts = %d, vf->pts_us64 = %lld, pts_valid = %d\n", vf->pts, vf->pts_us64, pts_valid);
			vdec_vframe_ready(vdec, vf);
			kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
			ATRACE_COUNTER(hw->pts_name, vf->pts);
			avs_vf_notify_receiver(hw, PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);

			if (kfifo_get(&hw->newframe_q, &vf) == 0) {
				pr_info("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
						}
			set_frame_info(hw, vf, &dur);
			vf->bufWidth = 1920;
			if (force_interlaced_frame)
				vf->pts = 0;
			else
			vf->pts = hw->next_pts;

			if (vf->pts == 0) {
				vf->pts_us64 = 0;
			}

			if ((repeat_count > 1) && hw->avi_flag) {
				/* vf->duration = hw->vavs_amstream_dec_info.rate *
				 *	 repeat_count >> 1;
				 */
				vf->duration = dur * repeat_count >> 1;
				if (hw->next_pts != 0) {
					hw->next_pts +=
						((vf->duration) -
						 ((vf->duration) >> 4));
				}
			} else {
				/* vf->duration = hw->vavs_amstream_dec_info.rate
				 *	 >> 1;
				 */
				vf->duration = dur >> 1;
				hw->next_pts = 0;
			}
			vf->signal_type = 0;
			vf->index = buffer_index;
			vf->duration_pulldown = 0;
			if (force_interlaced_frame) {
				vf->type = VIDTYPE_INTERLACE_BOTTOM;
			} else {
						vf->type =
						(reg & TOP_FIELD_FIRST_FLAG) ?
						VIDTYPE_INTERLACE_BOTTOM :
						VIDTYPE_INTERLACE_TOP;
					}
#ifdef NV21
			vf->type |= VIDTYPE_VIU_NV21;
#endif
			if (hw->m_ins_flag) {
				vf->canvas0Addr = vf->canvas1Addr = -1;
				vf->plane_num = 2;

				vf->canvas0_config[0] = hw->canvas_config[buffer_index][0];
				vf->canvas0_config[1] = hw->canvas_config[buffer_index][1];

				vf->canvas1_config[0] = hw->canvas_config[buffer_index][0];
				vf->canvas1_config[1] = hw->canvas_config[buffer_index][1];
			} else
				vf->canvas0Addr = vf->canvas1Addr =
					index2canvas(buffer_index);
			vf->type_original = vf->type;
			vf->pts_us64 = 0;
			hw->vfbuf_use[buffer_index]++;
			vf->mem_handle =
				decoder_bmmu_box_get_mem_handle(
					hw->mm_blk_handle,
					buffer_index);

			if (hw->m_ins_flag && vdec_frame_based(hw_to_vdec(hw)))
				set_vframe_pts(hw, decode_pic_count, vf);

			if (vdec_stream_based(vdec) && (!vdec->vbuf.use_ptsserv)) {
				vf->pts_us64 = (u64)-1;
				vf->pts = 0;
			}
			debug_print(hw, PRINT_FLAG_PTS,
				"interlace2 vf->pts = %d, vf->pts_us64 = %lld, pts_valid = %d\n", vf->pts, vf->pts_us64, pts_valid);
			vdec_vframe_ready(vdec, vf);
			kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
			ATRACE_COUNTER(hw->pts_name, vf->pts);
			avs_vf_notify_receiver(hw, PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
			hw->total_frame++;
		} else {	/* progressive */
			hw->throw_pb_flag = 0;

			debug_print(hw, PRINT_FLAG_VFRAME_DETAIL,
				"progressive picture type %d\n",
					   picture_type);
			if (kfifo_get(&hw->newframe_q, &vf) == 0) {
				pr_info
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}
			set_frame_info(hw, vf, &dur);
			vf->bufWidth = 1920;
			hw->pic_type = 1;

			if ((picture_type == I_PICTURE) && pts_valid) {
				vf->pts = pts;
				if ((repeat_count > 1) && hw->avi_flag) {
					/* hw->next_pts = pts +
					 *	 (hw->vavs_amstream_dec_info.rate *
					 *	 repeat_count)*15/16;
					 */
					hw->next_pts =
						pts +
						(dur * repeat_count) * 15 / 16;
				} else
					hw->next_pts = 0;
			} else {
				vf->pts = hw->next_pts;
				if (vf->pts == 0) {
					vf->pts_us64 = 0;
				}
				if ((repeat_count > 1) && hw->avi_flag) {
					/* vf->duration =
					 *	 hw->vavs_amstream_dec_info.rate *
					 *	 repeat_count;
					 */
					vf->duration = dur * repeat_count;
					if (hw->next_pts != 0) {
						hw->next_pts +=
							((vf->duration) -
							 ((vf->duration) >> 4));
					}
				} else {
					/* vf->duration =
					 *	 hw->vavs_amstream_dec_info.rate;
					 */
					vf->duration = dur;
					hw->next_pts = 0;
				}
			}
			vf->signal_type = 0;
			vf->index = buffer_index;
			vf->duration_pulldown = 0;
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
#ifdef NV21
			vf->type |= VIDTYPE_VIU_NV21;
#endif
			if (hw->m_ins_flag) {
				vf->canvas0Addr = vf->canvas1Addr = -1;
				vf->plane_num = 2;

				vf->canvas0_config[0] = hw->canvas_config[buffer_index][0];
				vf->canvas0_config[1] = hw->canvas_config[buffer_index][1];

				vf->canvas1_config[0] = hw->canvas_config[buffer_index][0];
				vf->canvas1_config[1] = hw->canvas_config[buffer_index][1];
			} else
				vf->canvas0Addr = vf->canvas1Addr =
					index2canvas(buffer_index);
			vf->type_original = vf->type;

			vf->pts = (pts_valid)?pts:0;
			//vf->pts_us64 = (pts_valid) ? pts_us64 : 0;
			debug_print(hw, PRINT_FLAG_VFRAME_DETAIL,
				"buffer_index %d, canvas addr %x\n",
					   buffer_index, vf->canvas0Addr);
			debug_print(hw, PRINT_FLAG_PTS,
				"progressive vf->pts = %d, vf->pts_us64 = %lld, pts_valid = %d\n", vf->pts, vf->pts_us64, pts_valid);
			hw->vfbuf_use[buffer_index]++;
			vf->mem_handle =
				decoder_bmmu_box_get_mem_handle(
					hw->mm_blk_handle,
					buffer_index);

			if (hw->m_ins_flag && vdec_frame_based(hw_to_vdec(hw)))
				set_vframe_pts(hw, decode_pic_count, vf);

			if (vdec_stream_based(vdec) && (!vdec->vbuf.use_ptsserv)) {
				vf->pts_us64 =
					(((u64)vf->duration << 32) & 0xffffffff00000000) | offset;
				vf->pts = 0;
			}
			decoder_do_frame_check(hw_to_vdec(hw), vf);
			vdec_vframe_ready(vdec, vf);
			kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
			ATRACE_COUNTER(hw->pts_name, vf->pts);
			ATRACE_COUNTER(hw->new_q_name, kfifo_len(&hw->newframe_q));
			ATRACE_COUNTER(hw->disp_q_name, kfifo_len(&hw->display_q));
			avs_vf_notify_receiver(hw, PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
			hw->total_frame++;
		}

		/*count info*/
		vdec_count_info(hw->gvs, 0, offset);
		if (offset) {
			if (picture_type == I_PICTURE) {
				hw->gvs->i_decoded_frames++;
			} else if (picture_type == P_PICTURE) {
				hw->gvs->p_decoded_frames++;
			} else if (picture_type == B_PICTURE) {
				hw->gvs->b_decoded_frames++;
			}
		}
		avs_update_gvs(hw);
		vdec_fill_vdec_frame(hw_to_vdec(hw), NULL, hw->gvs, vf, 0);
		return 0;
}


static irqreturn_t vmavs_isr_thread_fn(struct vdec_s *vdec, int irq)
{
		struct vdec_avs_hw_s *hw =
			(struct vdec_avs_hw_s *)vdec->private;
		u32 reg;
		u32 picture_type;
		u32 buffer_index;
		u32 frame_size;
		unsigned int pts, offset = 0;
		u64 pts_us64;
		u32 debug_tag;
		u32 buffer_status_debug;

		debug_print(hw, PRINT_FLAG_RUN_FLOW, "READ_VREG(AVS_BUFFEROUT) 0x%x, READ_VREG(DECODE_STATUS) 0x%x READ_VREG(AV_SCRATCH_N) 0x%x, READ_VREG(DEBUG_REG1) 0x%x, READ_VREG(AV_SCRATCH_2) 0x%x\n",
				READ_VREG(AVS_BUFFEROUT),READ_VREG(DECODE_STATUS), READ_VREG(AV_SCRATCH_N), READ_VREG(DEBUG_REG1), READ_VREG(AV_SCRATCH_2));

		debug_tag = READ_VREG(DEBUG_REG1);
		buffer_status_debug = debug_tag >> 16;
		debug_tag &= 0xffff;
		if (debug_tag != 0) {
			debug_print(hw, 1,
				"dbg%x: %x buffer_status 0x%x l/w/r %x %x %x bitcnt %x AVAIL %x\n",
				debug_tag,
				READ_VREG(DEBUG_REG2),
				buffer_status_debug,
				READ_VREG(VLD_MEM_VIFIFO_LEVEL),
				READ_VREG(VLD_MEM_VIFIFO_WP),
				READ_VREG(VLD_MEM_VIFIFO_RP),
				READ_VREG(VIFF_BIT_CNT),
				READ_VREG(VLD_MEM_VIFIFO_BYTES_AVAIL));

			if (((udebug_pause_pos & 0xffff)
				== (debug_tag & 0xffff)) &&
				(udebug_pause_decode_idx == 0 ||
				udebug_pause_decode_idx == hw->decode_pic_count) &&
				(udebug_pause_val == 0 ||
				udebug_pause_val == READ_VREG(DEBUG_REG2)) &&
				(udebug_pause_ins_id == 0 ||
					DECODE_ID(hw) == (udebug_pause_ins_id -1))) {
				udebug_pause_pos &= 0xffff;
				hw->ucode_pause_pos = udebug_pause_pos;
				if (debug & DEBUG_PIC_DONE_WHEN_UCODE_PAUSE) {
					hw->decode_pic_count++;
					if ((hw->decode_pic_count & 0xffff) == 0) {
						/*make ucode do not handle it as first picture*/
						hw->decode_pic_count++;
					}
					reset_process_time(hw);
					hw->dec_result = DEC_RESULT_DONE;
					amvdec_stop();
					vavs_save_regs(hw);
					debug_print(hw, PRINT_FLAG_DECODING,
						"%s ucode pause, force done, decode_pic_count = %d, bit_cnt=0x%x\n",
						__func__,
						hw->decode_pic_count,
						READ_VREG(VIFF_BIT_CNT));
					vdec_schedule_work(&hw->work);
					return IRQ_HANDLED;
				}
			}
			if (hw->ucode_pause_pos)
				reset_process_time(hw);
			else
				WRITE_VREG(DEBUG_REG1, 0);
			return IRQ_HANDLED;
		} else {
			debug_print(hw, PRINT_FLAG_DECODING,
				"%s decode_status 0x%x, buffer_status 0x%x\n",
				__func__,
				READ_VREG(DECODE_STATUS),
				buffer_status_debug);
		}
		reg = READ_VREG(DECODE_STATUS); // need find a null register pyx
		if (reg == DECODE_STATUS_INFO) {
			WRITE_VREG(DECODE_STATUS, 0);
			pr_err("READ_VREG(AVS_PIC_INFO) = 0x%x\n", READ_VREG(AVS_PIC_INFO));
			return IRQ_HANDLED;
		}

#ifdef AVSP_LONG_CABAC
		if (firmware_sel == 0 && READ_VREG(LONG_CABAC_REQ)) {
			pr_info("schedule long_cabac_wd_work and requested from %d\n",
				(READ_VREG(LONG_CABAC_REQ) >> 8) & 0xFF);
			schedule_work(&long_cabac_wd_work);
		}
#endif

#ifdef ENABLE_USER_DATA
		if (UserDataHandler(hw))
			return IRQ_HANDLED;
#endif
		reg = READ_VREG(AVS_BUFFEROUT);
		if (reg) {
			unsigned short decode_pic_count
				= READ_VREG(DECODE_PIC_COUNT);
			debug_print(hw, PRINT_FLAG_DECODING, "AVS_BUFFEROUT=0x%x decode_pic_count %d\n",
				reg, decode_pic_count);
			hw->pics[hw->decoding_index].offset = READ_VREG(AVS_OFFSET_REG);
			hw->pics[hw->decoding_index].repeat_cnt = READ_VREG(AVS_REPEAT_COUNT);
			hw->pics[hw->decoding_index].buffer_info = reg;
			hw->pics[hw->decoding_index].index = hw->decoding_index;
			hw->pics[hw->decoding_index].decode_pic_count = decode_pic_count;
			if (pts_by_offset) {
				offset = READ_VREG(AVS_OFFSET_REG);
				debug_print(hw, PRINT_FLAG_DECODING, "AVS OFFSET=%x\n", offset);
				if ((vdec->vbuf.no_parser == 0) || (vdec->vbuf.use_ptsserv)) {
					if (pts_lookup_offset_us64(PTS_TYPE_VIDEO, offset, &pts,
						&frame_size, 0, &pts_us64) == 0) {
						hw->pics[hw->decoding_index].pts_valid = 1;
						hw->pics[hw->decoding_index].pts = pts;
						hw->pics[hw->decoding_index].pts64 = pts_us64;
#ifdef DEBUG_PTS
						hw->pts_hit++;
#endif
					} else {
						hw->pics[hw->decoding_index].pts_valid = 0;
						hw->pics[hw->decoding_index].pts = 0;
						hw->pics[hw->decoding_index].pts64 = 0;
#ifdef DEBUG_PTS
						hw->pts_missed++;
#endif
					}
				}
			}

#ifdef USE_DYNAMIC_BUF_NUM
			buffer_index = ((reg & 0x7) + (((reg >> 8) & 0x3) << 3) - 1) & 0x1f;
#else
			if (firmware_sel == 0)
				buffer_index = ((reg & 0x7) +
					(((reg >> 8) & 0x3) << 3) - 1) & 0x1f;
			else
				buffer_index = ((reg & 0x7) - 1) & 3;
#endif
			picture_type = (reg >> 3) & 7;
			hw->pics[hw->decoding_index].picture_type = picture_type;
#ifdef DEBUG_PTS
			if (picture_type == I_PICTURE) {
				if (!hw->pics[hw->decoding_index].pts_valid)
					hw->pts_i_missed++;
				else
					hw->pts_i_hit++;
			}
#endif

			if (hw->throw_pb_flag && picture_type != I_PICTURE) {
				debug_print(hw, PRINT_FLAG_DECODING,
					"%s WRITE_VREG(AVS_BUFFERIN, 0x%x) for throwing picture with type of %d\n",
					__func__,
					~(1 << hw->decoding_index), picture_type);

				WRITE_VREG(AVS_BUFFERIN, ~(1 << hw->decoding_index));
				hw->buf_use[hw->decoding_index]--;
			} else {
				if ((picture_type == I_PICTURE) ||
					(picture_type == P_PICTURE)) {
					buffer_index = update_reference(hw, hw->decoding_index);
				} else {
					/* drop b frame before reference pic ready */
					if (hw->refs[0] == -1)
						buffer_index = hw->vf_buf_num_used;
				}

				if (buffer_index < hw->vf_buf_num_used) {
					debug_print(hw, PRINT_FLAG_RUN_FLOW,
						"avs: show num %d, type %d, index %d, offset %x\n",
						decode_pic_count, picture_type, buffer_index, offset);
					prepare_display_buf(hw, &hw->pics[buffer_index]);
				} else {
					debug_print(hw, PRINT_FLAG_RUN_FLOW,
						"avs: drop pic num %d, type %d, index %d, offset %x\n",
						decode_pic_count, picture_type, buffer_index, offset);
				}
			}
			WRITE_VREG(AVS_BUFFEROUT, 0);
		}
		//WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

		if (hw->m_ins_flag) {
			u32 status_reg = READ_VREG(DECODE_STATUS);
			u32 decode_status = status_reg & 0xff;
			if (hw->dec_result == DEC_RESULT_DONE ||
				hw->dec_result == DEC_RESULT_AGAIN) {
				debug_print(hw, PRINT_FLAG_DECODING,
					"%s !!! READ_VREG(DECODE_STATUS) = 0x%x, decode_status 0x%x, buf_status 0x%x, dec_result = 0x%x, decode_pic_count = %d bit_cnt=0x%x\n",
					__func__, status_reg, decode_status,
					hw->buf_status,
					hw->dec_result, hw->decode_pic_count,
					READ_VREG(VIFF_BIT_CNT));
				return IRQ_HANDLED;
			} else if (decode_status == DECODE_STATUS_PIC_DONE ||
				decode_status == DECODE_STATUS_SKIP_PIC_DONE) {
				hw->buf_status = (status_reg >> 16) & 0xffff;
				if (decode_status == DECODE_STATUS_SKIP_PIC_DONE) {
					hw->decode_status_skip_pic_done_flag = 1;
					hw->decode_decode_cont_start_code = (status_reg >> 8) & 0xff;
				} else
					hw->decode_status_skip_pic_done_flag = 0;
				hw->decode_pic_count++;
				if ((hw->decode_pic_count & 0xffff) == 0) {
					/*make ucode do not handle it as first picture*/
					hw->decode_pic_count++;
				}
				reset_process_time(hw);
				hw->dec_result = DEC_RESULT_DONE;
#if DEBUG_MULTI_FLAG == 1
				WRITE_VREG(DECODE_STATUS, 0);
#else
				amvdec_stop();
#endif
				vavs_save_regs(hw);
				if (reg) {
					hw->buf_use[hw->decoding_index]++;
				}

				debug_print(hw, PRINT_FLAG_DECODING,
					"%s %s, READ_VREG(DECODE_STATUS) = 0x%x, decode_status 0x%x, buf_status 0x%x, dec_result = 0x%x, decode_pic_count = %d, bit_cnt=0x%x, AV_SCRATCH_B=0x%x\n",
					__func__,
					(decode_status == DECODE_STATUS_PIC_DONE) ?
					"DECODE_STATUS_PIC_DONE" : "DECODE_STATUS_SKIP_PIC_DONE",
					status_reg, decode_status,
					hw->buf_status,
					hw->dec_result, hw->decode_pic_count,
					READ_VREG(VIFF_BIT_CNT), READ_VREG(AV_SCRATCH_B));
				vdec_schedule_work(&hw->work);
				return IRQ_HANDLED;
			} else if (decode_status == DECODE_STATUS_DECODE_BUF_EMPTY ||
				decode_status == DECODE_STATUS_SEARCH_BUF_EMPTY) {
				hw->buf_status = (status_reg >> 16) & 0xffff;
				reset_process_time(hw);
#if DEBUG_MULTI_FLAG == 1
				WRITE_VREG(DECODE_STATUS, 0);
#else
				amvdec_stop();
#endif
				if (vdec_frame_based(hw_to_vdec(hw))) {
					hw->dec_result = DEC_RESULT_DONE;
					//if (hw->decode_pic_count == 0) {
						hw->decode_pic_count++;
					//}
					if ((hw->decode_pic_count & 0xffff) == 0) {
					/*make ucode do not handle it as first picture*/
						hw->decode_pic_count++;
					}
					vavs_save_regs(hw);
					if (reg) {
						hw->buf_use[hw->decoding_index]++;
					}
				} else
					hw->dec_result = DEC_RESULT_AGAIN;

				debug_print(hw, PRINT_FLAG_DECODING,
					"%s BUF_EMPTY, READ_VREG(DECODE_STATUS) = 0x%x, decode_status 0x%x, buf_status 0x%x, scratch_8 (AVS_BUFFERIN) 0x%x, dec_result = 0x%x, decode_pic_count = %d, bit_cnt=0x%x, hw->decode_status_skip_pic_done_flag = %d, hw->decode_decode_cont_start_code = 0x%x, AV_SCRATCH_B=0x%x\n",
					__func__, status_reg, decode_status,
					hw->buf_status,
					hw->reg_scratch_8,
					hw->dec_result, hw->decode_pic_count,
					READ_VREG(VIFF_BIT_CNT), hw->decode_status_skip_pic_done_flag, hw->decode_decode_cont_start_code, READ_VREG(AV_SCRATCH_B));
				vdec_schedule_work(&hw->work);
				return IRQ_HANDLED;
			}
		}


#ifdef HANDLE_AVS_IRQ
		return IRQ_HANDLED;
#else
		return;
#endif
}

static irqreturn_t vmavs_isr(struct vdec_s *vdec, int irq)
{

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	return IRQ_WAKE_THREAD;
	//return vavs_isr(0, hw);

}

static void vmavs_dump_state(struct vdec_s *vdec)
{
	struct vdec_avs_hw_s *hw =
	(struct vdec_avs_hw_s *)vdec->private;
	int i;
	debug_print(hw, 0,
		"====== %s\n", __func__);

	debug_print(hw, 0,
		"width/height (%d/%d), dur %d\n",
		hw->frame_width,
		hw->frame_height,
		hw->frame_dur
		);

	debug_print(hw, 0,
		"is_framebase(%d), decode_status 0x%x, buf_status 0x%x, buf_recycle_status 0x%x, throw %d, eos %d, state 0x%x, dec_result 0x%x dec_frm %d disp_frm %d run %d not_run_ready %d input_empty %d\n",
		vdec_frame_based(vdec),
		READ_VREG(DECODE_STATUS) & 0xff,
		hw->buf_status,
		hw->buf_recycle_status,
		hw->throw_pb_flag,
		hw->eos,
		hw->stat,
		hw->dec_result,
		hw->decode_pic_count,
		hw->display_frame_count,
		hw->run_count,
		hw->not_run_ready,
		hw->input_empty
		);

	if (vf_get_receiver(vdec->vf_provider_name)) {
		enum receviver_start_e state =
		vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_QUREY_STATE,
			NULL);
		debug_print(hw, 0,
			"\nreceiver(%s) state %d\n",
			vdec->vf_provider_name,
			state);
	}

	debug_print(hw, 0,
	"%s, newq(%d/%d), dispq(%d/%d)recycleq(%d/%d) drop %d vf peek %d, prepare/get/put (%d/%d/%d)\n",
	__func__,
	kfifo_len(&hw->newframe_q),
	VF_POOL_SIZE,
	kfifo_len(&hw->display_q),
	VF_POOL_SIZE,
	kfifo_len(&hw->recycle_q),
	VF_POOL_SIZE,
	hw->drop_frame_count,
	hw->peek_num,
	hw->prepare_num,
	hw->get_num,
	hw->put_num
	);

	debug_print(hw, 0, "vfbuf_use:\n");
	for (i = 0; i < hw->vf_buf_num_used; i++)
		debug_print(hw, 0, "%d: vf_buf_use %d\n",
			i, hw->vfbuf_use[i]);
	debug_print(hw, 0, "ref_use:\n");
	for (i = 0; i < hw->vf_buf_num_used; i++)
		debug_print(hw, 0, "%d: ref_use %d\n",
			i, hw->ref_use[i]);
	debug_print(hw, 0, "buf_use:\n");
	for (i = 0; i < hw->vf_buf_num_used; i++)
		debug_print(hw, 0, "%d: buf_use %d\n",
			i, hw->buf_use[i]);

	debug_print(hw, 0,
		"DECODE_STATUS=0x%x\n",
		READ_VREG(DECODE_STATUS));
	debug_print(hw, 0,
		"MPC_E=0x%x\n",
		READ_VREG(MPC_E));
	debug_print(hw, 0,
		"DECODE_MODE=0x%x\n",
		READ_VREG(DECODE_MODE));
	debug_print(hw, 0,
		"wait_buf_status, AV_SCRATCH_5=0x%x\n",
		READ_VREG(AV_SCRATCH_5));
	debug_print(hw, 0,
		"MBY_MBX=0x%x\n",
		READ_VREG(MBY_MBX));
	debug_print(hw, 0,
		"VIFF_BIT_CNT=0x%x\n",
		READ_VREG(VIFF_BIT_CNT));
	debug_print(hw, 0,
		"VLD_MEM_VIFIFO_LEVEL=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_LEVEL));
	debug_print(hw, 0,
		"VLD_MEM_VIFIFO_WP=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_WP));
	debug_print(hw, 0,
		"VLD_MEM_VIFIFO_RP=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_RP));
	debug_print(hw, 0,
		"PARSER_VIDEO_RP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_rp));
	debug_print(hw, 0,
		"PARSER_VIDEO_WP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_wp));

	if (vdec_frame_based(vdec) &&
		(debug &	PRINT_FRAMEBASE_DATA)
		) {
		int jj;
		if (hw->chunk && hw->chunk->block &&
			hw->chunk->size > 0) {
			u8 *data = NULL;

			if (!hw->chunk->block->is_mapped)
				data = codec_mm_vmap(hw->chunk->block->start +
					hw->chunk->offset, hw->chunk->size);
			else
				data = ((u8 *)hw->chunk->block->start_virt)
					+ hw->chunk->offset;

			debug_print(hw, 0,
				"frame data size 0x%x\n",
				hw->chunk->size);
			for (jj = 0; jj < hw->chunk->size; jj++) {
				if ((jj & 0xf) == 0)
					debug_print(hw,
					PRINT_FRAMEBASE_DATA,
						"%06x:", jj);
				debug_print_cont(hw,
				PRINT_FRAMEBASE_DATA,
					"%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					debug_print_cont(hw,
					PRINT_FRAMEBASE_DATA,
						"\n");
			}

			if (!hw->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		}
	}

}

 int ammvdec_avs_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_avs_hw_s *hw = NULL;
	int r = 0;

	if (vdec_get_debug_flags() & 0x8)
		return amvdec_avs_probe(pdev);

	pr_info("ammvdec_avs probe start.\n");

	if (pdata == NULL) {
		pr_info("ammvdec_avs platform data undefined.\n");
		return -EFAULT;
	}

	hw = (struct vdec_avs_hw_s *)vzalloc(sizeof(struct vdec_avs_hw_s));
	if (hw == NULL) {
		pr_info("\nammvdec_avs decoder driver alloc failed\n");
		return -ENOMEM;
	}
	/*atomic_set(&hw->error_handler_run, 0);*/
	hw->m_ins_flag = 1;

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXM || disable_longcabac_trans)
		firmware_sel = 1;

	if (firmware_sel == 1) {
#ifndef USE_DYNAMIC_BUF_NUM
		vf_buf_num = 4;
#endif
		canvas_base = 0;
		canvas_num = 3;
	} else {
		pr_info("Error, do not support longcabac work around!!!");
		r = -ENOMEM;
		goto error1;
	}

	if (pdata->sys_info)
		hw->vavs_amstream_dec_info = *pdata->sys_info;

	hw->is_reset = 0;
	pdata->user_data_read = NULL;
	pdata->reset_userdata_fifo = NULL;

	pdata->private = hw;
	pdata->dec_status = vavs_dec_status;
	pdata->set_isreset = vavs_set_isreset;
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vmavs_isr;
	pdata->threaded_irq_handler = vmavs_isr_thread_fn;
	pdata->dump_state = vmavs_dump_state;

	snprintf(hw->vdec_name, sizeof(hw->vdec_name),
		"avs-%d", pdev->id);
	snprintf(hw->pts_name, sizeof(hw->pts_name),
		"%s-pts", hw->vdec_name);
	snprintf(hw->new_q_name, sizeof(hw->new_q_name),
		"%s-newframe_q", hw->vdec_name);
	snprintf(hw->disp_q_name, sizeof(hw->disp_q_name),
		"%s-dispframe_q", hw->vdec_name);

	vavs_vdec_info_init(hw);

#ifdef ENABLE_USER_DATA
	if (NULL == hw->user_data_buffer) {
		hw->user_data_buffer =
			dma_alloc_coherent(amports_get_dma_device(),
				USER_DATA_SIZE,
				&hw->user_data_buffer_phys, GFP_KERNEL);
		if (!hw->user_data_buffer) {
			pr_info("%s: Can not allocate hw->user_data_buffer\n",
				   __func__);
			r = -ENOMEM;
			goto error2;
		}
		pr_debug("hw->user_data_buffer = 0x%p, hw->user_data_buffer_phys = 0x%x\n",
			hw->user_data_buffer, (u32)hw->user_data_buffer_phys);
	}
#endif
	/*hw->lmem_addr = kmalloc(LMEM_BUF_SIZE, GFP_KERNEL);
	if (hw->lmem_addr == NULL) {
		pr_err("%s: failed to alloc lmem buffer\n", __func__);
		return -1;
	}
	hw->lmem_phy_addr = dma_map_single(amports_get_dma_device(),
		hw->lmem_addr, LMEM_BUF_SIZE, DMA_FROM_DEVICE);
	if (dma_mapping_error(amports_get_dma_device(),
		hw->lmem_phy_addr)) {
		pr_err("%s: failed to map lmem buffer\n", __func__);
		kfree(hw->lmem_addr);
		hw->lmem_addr = NULL;
		return -1;
	}*/
	/*INIT_WORK(&hw->set_clk_work, avs_set_clk);*/
	hw->lmem_addr = (dma_addr_t)dma_alloc_coherent(amports_get_dma_device(),
	               LMEM_BUF_SIZE, (dma_addr_t *)&hw->lmem_phy_addr, GFP_KERNEL);
	if (hw->lmem_addr == 0) {
		pr_err("%s: failed to alloc lmem buffer\n", __func__);
		r = -1;
		goto error3;
	}

	if (vavs_init(hw) < 0) {
		pr_info("amvdec_avs init failed.\n");
		r = -ENODEV;
		goto error4;
	}

	/*INIT_WORK(&hw->fatal_error_wd_work, vavs_fatal_error_handler);
	atomic_set(&hw->error_handler_run, 0);*/
#if 0
#ifdef ENABLE_USER_DATA
	INIT_WORK(&hw->userdata_push_work, userdata_push_do_work);
#endif
#endif
	INIT_WORK(&hw->notify_work, vavs_notify_work);

	if (pdata->use_vfm_path) {
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			    VFM_DEC_PROVIDER_NAME);
		hw->frameinfo_enable = 1;
	}
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			MULTI_INSTANCE_PROVIDER_NAME ".%02x", pdev->id & 0xff);
	if (pdata->parallel_dec == 1) {
		int i;
		for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
			hw->canvas_spec[i] = 0xffffff;
	}
	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vavs_vf_provider, hw);

	platform_set_drvdata(pdev, pdata);

	hw->platform_dev = pdev;

	vdec_set_prepare_level(pdata, start_decode_buf_level);

	vdec_set_vframe_comm(pdata, DRIVER_NAME);

	if (pdata->parallel_dec == 1)
		vdec_core_request(pdata, CORE_MASK_VDEC_1);
	else {
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
					| CORE_MASK_COMBINE);
	}

	/*INIT_WORK(&hw->userdata_push_work, userdata_push_do_work);*/

	return 0;

error4:
	dma_free_coherent(amports_get_dma_device(),
		LMEM_BUF_SIZE, (void *)hw->lmem_addr,
		hw->lmem_phy_addr);
error3:
	dma_free_coherent(
		amports_get_dma_device(),
		USER_DATA_SIZE,
		hw->user_data_buffer,
		hw->user_data_buffer_phys);
error2:
	kfree(hw->gvs);
	hw->gvs = NULL;
	pdata->dec_status = NULL;
error1:
	vfree(hw);
	return r;
}

 int ammvdec_avs_remove(struct platform_device *pdev)
{

	if (vdec_get_debug_flags() & 0x8)
		return amvdec_avs_remove(pdev);
	else {
		struct vdec_avs_hw_s *hw =
			(struct vdec_avs_hw_s *)
			(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
		struct vdec_s *vdec = hw_to_vdec(hw);
		int i;

		if (hw->stat & STAT_VDEC_RUN) {
			amvdec_stop();
			hw->stat &= ~STAT_VDEC_RUN;
		}

		if (hw->stat & STAT_ISR_REG) {
			vdec_free_irq(VDEC_IRQ_1, (void *)hw);
			hw->stat &= ~STAT_ISR_REG;
		}

		if (hw->stat & STAT_TIMER_ARM) {
			del_timer_sync(&hw->check_timer);
			hw->stat &= ~STAT_TIMER_ARM;
		}

		cancel_work_sync(&hw->work);
		cancel_work_sync(&hw->notify_work);

		if (hw->mm_blk_handle) {
			decoder_bmmu_box_free(hw->mm_blk_handle);
			hw->mm_blk_handle = NULL;
		}
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
	#ifdef ENABLE_USER_DATA
		if (hw->user_data_buffer != NULL) {
			dma_free_coherent(
				amports_get_dma_device(),
				USER_DATA_SIZE,
				hw->user_data_buffer,
				hw->user_data_buffer_phys);
			hw->user_data_buffer = NULL;
			hw->user_data_buffer_phys = 0;
		}
	#endif
		/*if (hw->lmem_addr) {
			dma_unmap_single(amports_get_dma_device(),
				hw->lmem_phy_addr, LMEM_BUF_SIZE, DMA_FROM_DEVICE);
			kfree(hw->lmem_addr);
			hw->lmem_addr = NULL;
		}*/
	if (hw->lmem_addr) {
		dma_free_coherent(amports_get_dma_device(),
					LMEM_BUF_SIZE, (void *)hw->lmem_addr,
					hw->lmem_phy_addr);
		hw->lmem_addr = 0;
	}

		if (hw->fw) {
			vfree(hw->fw);
			hw->fw = NULL;
		}

		pr_info("ammvdec_avs removed.\n");
		if (hw->gvs) {
			kfree(hw->gvs);
			hw->gvs = NULL;
		}

		vfree(hw);
		return 0;
	}
}


#ifdef DEBUG_MULTI_WITH_AUTOMODE
struct stream_buf_s *get_vbuf(void);
s32 esparser_init(struct stream_buf_s *buf, struct vdec_s *vdec);


static s32 vavs_init2(struct vdec_avs_hw_s *hw)
{
	int  size = -1;
	struct firmware_s *fw;
	u32 fw_size = 0x1000 * 16;

	fw = vmalloc(sizeof(struct firmware_s) + fw_size);
	if (IS_ERR_OR_NULL(fw))
		return -ENOMEM;

	pr_info("vavs_init\n");

	amvdec_enable();


	vavs_local_init(hw);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXM)
		size = get_firmware_data(VIDEO_DEC_AVS_MULTI, fw->data);
	else {
		if (firmware_sel == 1)
			size = get_firmware_data(VIDEO_DEC_AVS_NOCABAC, fw->data);
#ifdef AVSP_LONG_CABAC
		else {
			init_avsp_long_cabac_buf();
			size = get_firmware_data(VIDEO_DEC_AVS_MULTI, fw->data);
		}
#endif
	}

	if (size < 0) {
		amvdec_disable();
		pr_err("get firmware fail.");
		/*vfree(buf);*/
		return -1;
	}

	fw->len = size;
	hw->fw = fw;
	if (hw->m_ins_flag) {
		init_timer(&hw->check_timer);
		hw->check_timer.data = (ulong) hw;
		hw->check_timer.function = check_timer_func;
		hw->check_timer.expires = jiffies + CHECK_INTERVAL;


		//add_timer(&hw->check_timer);
		hw->stat |= STAT_TIMER_ARM;

		INIT_WORK(&hw->work, vavs_work);

		hw->fw = fw;
	}
	return 0;
}

unsigned int debug_flag2;
static int vavs_prot_init2(struct vdec_avs_hw_s *hw, unsigned char post_flag)
{
	int r = 0;
	/*
	 * 2: assist
	 * 3: vld_reset
	 * 4: vld_part_reset
	 * 5: vfifo reset
	 * 6: iqidct
	 * 7: mc
	 * 8: dblk
	 * 9: pic_dc
	 * 10: psc
	 * 11: mcpu
	 * 12: ccpu
	 * 13: ddr
	 * 14: afifo
	 */
	unsigned char run_flag;
#ifdef OOO
	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) /*| (1 << 4)*/);
	WRITE_VREG(DOS_SW_RESET0, 0);

	READ_VREG(DOS_SW_RESET0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) /*| (1 << 4)*/);
	WRITE_VREG(DOS_SW_RESET0, 0);

	WRITE_VREG(DOS_SW_RESET0, (1 << 9) | (1 << 8));
	WRITE_VREG(DOS_SW_RESET0, 0);
#endif
	/***************** reset vld   **********************************/
#ifdef OOO
	WRITE_VREG(POWER_CTL_VLD, 0x10);
	WRITE_VREG_BITS(VLD_MEM_VIFIFO_CONTROL, 2, MEM_FIFO_CNT_BIT, 2);
	WRITE_VREG_BITS(VLD_MEM_VIFIFO_CONTROL,	8, MEM_LEVEL_CNT_BIT, 6);
#endif
   if (start_decoding_delay & 0x80000)
		msleep(start_decoding_delay&0xffff);

if (debug_flag2 & 0x1)
	run_flag = post_flag;
else
	run_flag = !post_flag;
if (run_flag) {
	if (hw->m_ins_flag) {
		int i;
		if (hw->decode_pic_count == 0) {
			r = vavs_canvas_init(hw);
#ifndef USE_DYNAMIC_BUF_NUM
			for (i = 0; i < 4; i++) {
				WRITE_VREG(AV_SCRATCH_0 + i,
					hw->canvas_spec[i]
				);
			}
#else
		for (i = 0; i < hw->vf_buf_num_used; i += 2) {
			WRITE_VREG(buf_spec_reg[i >> 1],
				(hw->canvas_spec[i] & 0xffff) |
				((hw->canvas_spec[i + 1] & 0xffff)
					<< 16)
			);
		}
#endif
		} else
			vavs_restore_regs(hw);

		for (i = 0; i < hw->vf_buf_num_used; i++) {
			config_cav_lut_ex(canvas_y(hw->canvas_spec[i]),
				hw->canvas_config[i][0].phy_addr,
				hw->canvas_config[i][0].width,
				hw->canvas_config[i][0].height,
				CANVAS_ADDR_NOWRAP,
				hw->canvas_config[i][0].block_mode,
				0, VDEC_1);

			config_cav_lut_ex(canvas_u(hw->canvas_spec[i]),
				hw->canvas_config[i][1].phy_addr,
				hw->canvas_config[i][1].width,
				hw->canvas_config[i][1].height,
				CANVAS_ADDR_NOWRAP,
				hw->canvas_config[i][1].block_mode,
				0, VDEC_1);
		}
	}
}

if (debug_flag2 & 0x2)
	run_flag = post_flag;
else
	run_flag = !post_flag;
if (run_flag) {

	/* notify ucode the buffer offset */
	if (hw->decode_pic_count == 0)
		WRITE_VREG(AV_SCRATCH_F, hw->buf_offset);
#ifdef OOO
	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);
#endif
	}
	if (start_decoding_delay & 0x40000)
		msleep(start_decoding_delay&0xffff);

	if (debug_flag2 & 0x4)
		run_flag = post_flag;
	else
		run_flag = !post_flag;
	if (run_flag) {
	if (hw->decode_pic_count == 0) {
#ifndef USE_DYNAMIC_BUF_NUM
		WRITE_VREG(AVS_SOS_COUNT, 0);
#endif
		WRITE_VREG(AVS_BUFFERIN, 0);
		WRITE_VREG(AVS_BUFFEROUT, 0);
	}
	if (error_recovery_mode)
		WRITE_VREG(AVS_ERROR_RECOVERY_MODE, 0);
	else
		WRITE_VREG(AVS_ERROR_RECOVERY_MODE, 1);
	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);
}

if (debug_flag2 & 0x8)
	run_flag = post_flag;
else
	run_flag = !post_flag;
if (run_flag) {

#ifndef USE_DYNAMIC_BUF_NUM				/* def DEBUG_UCODE */
	if (hw->decode_pic_count == 0)
		WRITE_VREG(AV_SCRATCH_D, 0);
#endif
	if (start_decoding_delay & 0x10000)
		msleep(start_decoding_delay&0xffff);
#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
#endif
	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);

	if (start_decoding_delay & 0x20000)
		msleep(start_decoding_delay&0xffff);


#ifdef PIC_DC_NEED_CLEAR
	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 31);
#endif
}
if (debug_flag2 & 0x10)
	run_flag = post_flag;
else
	run_flag = !post_flag;
if (run_flag) {
#ifdef ENABLE_USER_DATA
	if (firmware_sel == 0) {
		pr_info("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! firmware_sel is 0\n");
		WRITE_VREG(AV_SCRATCH_N, (u32)(hw->user_data_buffer_phys - hw->buf_offset));
		pr_debug("AV_SCRATCH_N = 0x%x\n", READ_VREG(AV_SCRATCH_N));
	}
#endif
}

if (debug_flag2 & 0x20)
	run_flag = post_flag;
else
	run_flag = !post_flag;
if (run_flag) {
	if (hw->m_ins_flag) {
		if (vdec_frame_based(hw_to_vdec(hw)))
			WRITE_VREG(DECODE_MODE, DECODE_MODE_MULTI_FRAMEBASE);
		else
			WRITE_VREG(DECODE_MODE, DECODE_MODE_MULTI_STREAMBASE);
		WRITE_VREG(DECODE_LMEM_BUF_ADR, (u32)hw->lmem_phy_addr);
	} else
		WRITE_VREG(DECODE_MODE, DECODE_MODE_SINGLE);
	WRITE_VREG(DECODE_STOP_POS, udebug_flag);
	hw->old_udebug_flag = udebug_flag;
}
	return r;
}

static void init_hw(struct vdec_s *vdec)
{
	struct vdec_avs_hw_s *hw =
	(struct vdec_avs_hw_s *)vdec->private;
	int ret;
	pr_info("%s, %d\n", __func__, __LINE__);
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXM)
		ret = amvdec_loadmc_ex(VFORMAT_AVS, NULL, hw->fw->data);
	else if (firmware_sel == 1)
		ret = amvdec_loadmc_ex(VFORMAT_AVS, "avs_no_cabac", hw->fw->data);
	else
		ret = amvdec_loadmc_ex(VFORMAT_AVS, NULL, hw->fw->data);

	if (ret < 0) {
		amvdec_disable();
		/*vfree(buf);*/
		pr_err("AVS: the %s fw loading failed, err: %x\n",
			tee_enabled() ? "TEE" : "local", ret);
	}
	pr_info("%s, %d\n", __func__, __LINE__);

	/*vfree(buf);*/

	hw->stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	ret = vavs_prot_init2(hw, 0);
	if (ret < 0)
		return;
	pr_info("%s, %d\n", __func__, __LINE__);

}


static unsigned long run_ready2(struct vdec_s *vdec, unsigned long mask)
{
	return 1;
}

static void run2(struct vdec_s *vdec, unsigned long mask,
void (*callback)(struct vdec_s *, void *),
		void *arg)
{
	struct vdec_avs_hw_s *hw =
	(struct vdec_avs_hw_s *)vdec->private;
	pr_info("%s, %d\n", __func__, __LINE__);

	vavs_prot_init2(hw, 1);

	vdec_source_changed(VFORMAT_AVS,
					1920, 1080, 30);

	amvdec_start();

	hw->stat |= STAT_VDEC_RUN;
	pr_info("%s %d\n", __func__, __LINE__);

}

static int ammvdec_avs_probe2(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_avs_hw_s *hw = NULL;

	pr_info("ammvdec_avs probe start.\n");

	if (pdata == NULL) {
		pr_info("ammvdec_avs platform data undefined.\n");
		return -EFAULT;
	}
	pr_info("%s %d\n", __func__, __LINE__);

	hw = (struct vdec_avs_hw_s *)vzalloc(sizeof(struct vdec_avs_hw_s));
	if (hw == NULL) {
		pr_info("\nammvdec_avs decoder driver alloc failed\n");
		return -ENOMEM;
	}
	pr_info("%s %d\n", __func__, __LINE__);
	/*atomic_set(&hw->error_handler_run, 0);*/
	hw->m_ins_flag = 1;
	pr_info("%s %d\n", __func__, __LINE__);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXM || disable_longcabac_trans)
		firmware_sel = 1;
	pr_info("%s %d\n", __func__, __LINE__);

	if (firmware_sel == 1) {
#ifndef USE_DYNAMIC_BUF_NUM
		vf_buf_num = 4;
#endif
		canvas_base = 0;
		canvas_num = 3;
	} else {
		pr_info("Error, do not support longcabac work around!!!");
		return -ENOMEM;
	}
	pr_info("%s %d\n", __func__, __LINE__);

	if (pdata->sys_info)
		hw->vavs_amstream_dec_info = *pdata->sys_info;
	pr_info("%s %d\n", __func__, __LINE__);

	hw->is_reset = 0;
	pdata->user_data_read = NULL;
	pdata->reset_userdata_fifo = NULL;

	pr_info("%s %d\n", __func__, __LINE__);

	pdata->private = hw;
	pdata->dec_status = vavs_dec_status;
	pdata->set_isreset = vavs_set_isreset;
	pdata->run_ready = run_ready2;
	pdata->run = run2;
	pdata->reset = reset;
	pdata->irq_handler = vmavs_isr;
	pdata->threaded_irq_handler = vmavs_isr_thread_fn;
	pdata->dump_state = vmavs_dump_state;

	pr_info("%s %d\n", __func__, __LINE__);

	vavs_vdec_info_init(hw);

	pr_info("%s %d\n", __func__, __LINE__);

#ifdef ENABLE_USER_DATA
	if (NULL == hw->user_data_buffer) {
		hw->user_data_buffer =
			dma_alloc_coherent(amports_get_dma_device(),
				USER_DATA_SIZE,
				&hw->user_data_buffer_phys, GFP_KERNEL);
		if (!hw->user_data_buffer) {
			pr_info("%s: Can not allocate hw->user_data_buffer\n",
				   __func__);
			return -ENOMEM;
		}
		pr_debug("hw->user_data_buffer = 0x%p, hw->user_data_buffer_phys = 0x%x\n",
			hw->user_data_buffer, (u32)hw->user_data_buffer_phys);
	}
#endif
	hw->lmem_addr = kmalloc(LMEM_BUF_SIZE, GFP_KERNEL);
	if (hw->lmem_addr == NULL) {
		pr_err("%s: failed to alloc lmem buffer\n", __func__);
		return -1;
	}
	hw->lmem_phy_addr = dma_map_single(amports_get_dma_device(),
		hw->lmem_addr, LMEM_BUF_SIZE, DMA_FROM_DEVICE);
	if (dma_mapping_error(amports_get_dma_device(),
		hw->lmem_phy_addr)) {
		pr_err("%s: failed to map lmem buffer\n", __func__);
		kfree(hw->lmem_addr);
		hw->lmem_addr = NULL;
		return -1;
	}

	pr_info("%s %d\n", __func__, __LINE__);

	/*INIT_WORK(&hw->set_clk_work, avs_set_clk);*/

	pr_info("%s %d\n", __func__, __LINE__);

	if (vavs_init2(hw) < 0) {
		pr_info("amvdec_avs init failed.\n");
		kfree(hw->gvs);
		hw->gvs = NULL;
		pdata->dec_status = NULL;
		return -ENODEV;
	}
	/*vdec = pdata;*/
	pr_info("%s, %d\n", __func__, __LINE__);

if (hw->m_ins_flag) {
	INIT_WORK(&hw->notify_work, vavs_notify_work);
#if 1
	if (pdata->use_vfm_path) {
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			    VFM_DEC_PROVIDER_NAME);
		hw->frameinfo_enable = 1;
	}
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			MULTI_INSTANCE_PROVIDER_NAME ".%02x", pdev->id & 0xff);
	if (pdata->parallel_dec == 1) {
		int i;
		for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
			hw->canvas_spec[i] = 0xffffff;
	}
	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vavs_vf_provider, hw);

	platform_set_drvdata(pdev, pdata);

	hw->platform_dev = pdev;

	vdec_set_prepare_level(pdata, start_decode_buf_level);

	if (pdata->parallel_dec == 1)
		vdec_core_request(pdata, CORE_MASK_VDEC_1);
	else {
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
					| CORE_MASK_COMBINE);
	}
	pr_info("%s, %d\n", __func__, __LINE__);
#endif
}else{
	/*INIT_WORK(&hw->fatal_error_wd_work, vavs_fatal_error_handler);
	atomic_set(&hw->error_handler_run, 0);*/
#ifdef ENABLE_USER_DATA
	INIT_WORK(&hw->userdata_push_work, userdata_push_do_work);
#endif
	INIT_WORK(&hw->notify_work, vavs_notify_work);
}

	init_hw(pdata);
	return 0;
}

static int ammvdec_avs_remove2(struct platform_device *pdev)
{
	struct vdec_avs_hw_s *hw = ghw;

	cancel_work_sync(&hw->fatal_error_wd_work);
	atomic_set(&hw->error_handler_run, 0);
#ifdef ENABLE_USER_DATA
	cancel_work_sync(&hw->userdata_push_work);
#endif
	cancel_work_sync(&hw->notify_work);
	cancel_work_sync(&hw->set_clk_work);
	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}

	if (hw->stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_1, (void *)vavs_dec_id);
		hw->stat &= ~STAT_ISR_REG;
	}

	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->recycle_timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}
#ifdef AVSP_LONG_CABAC
	if (firmware_sel == 0) {
		mutex_lock(&vavs_mutex);
		cancel_work_sync(&long_cabac_wd_work);
		mutex_unlock(&vavs_mutex);

		if (es_write_addr_virt) {
#if 0
			codec_mm_free_for_dma("vavs", es_write_addr_phy);
#else
			dma_unmap_single(amports_get_dma_device(),
				es_write_addr_phy,
				MAX_CODED_FRAME_SIZE, DMA_FROM_DEVICE);
			/*kfree(es_write_addr_virt);*/
			es_write_addr_virt = NULL;
#endif
		}

#ifdef BITSTREAM_READ_TMP_NO_CACHE
		if (bitstream_read_tmp) {
			dma_free_coherent(amports_get_dma_device(),
				SVA_STREAM_BUF_SIZE, bitstream_read_tmp,
				bitstream_read_tmp_phy);
			bitstream_read_tmp = NULL;
		}
#else
		if (bitstream_read_tmp) {
			dma_unmap_single(amports_get_dma_device(),
				bitstream_read_tmp_phy,
				SVA_STREAM_BUF_SIZE, DMA_FROM_DEVICE);
			kfree(bitstream_read_tmp);
			bitstream_read_tmp = NULL;
		}
#endif
	}
#endif
	if (hw->stat & STAT_VF_HOOK) {
		if (hw->fr_hint_status == VDEC_HINTED && !hw->is_reset)
			avs_vf_notify_receiver(hw, PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_FR_END_HINT, NULL);
		hw->fr_hint_status = VDEC_NO_NEED_HINT;
		vf_unreg_provider(&vavs_vf_prov);
		hw->stat &= ~STAT_VF_HOOK;
	}

#ifdef ENABLE_USER_DATA
	if (hw->user_data_buffer != NULL) {
		dma_free_coherent(
			amports_get_dma_device(),
			USER_DATA_SIZE,
			hw->user_data_buffer,
			hw->user_data_buffer_phys);
		hw->user_data_buffer = NULL;
		hw->user_data_buffer_phys = 0;
	}
#endif

	if (hw->fw) {
		vfree(hw->fw);
		hw->fw = NULL;
	}

	amvdec_disable();
	/*vdec_disable_DMC(NULL);*/

	hw->pic_type = 0;
	if (hw->mm_blk_handle) {
		decoder_bmmu_box_free(hw->mm_blk_handle);
		hw->mm_blk_handle = NULL;
	}
#ifdef DEBUG_PTS
	pr_debug("pts hit %d, pts missed %d, i hit %d, missed %d\n", hw->pts_hit,
		   hw->pts_missed, hw->pts_i_hit, hw->pts_i_missed);
	pr_debug("total frame %d, hw->avi_flag %d, rate %d\n", hw->total_frame, hw->avi_flag,
		   hw->vavs_amstream_dec_info.rate);
#endif
	kfree(hw->gvs);
	hw->gvs = NULL;
	vfree(hw);
	return 0;
}
#endif

static struct platform_driver ammvdec_avs_driver = {
#ifdef DEBUG_MULTI_WITH_AUTOMODE
	.probe = ammvdec_avs_probe2,
	.remove = ammvdec_avs_remove2,
#else
	.probe = ammvdec_avs_probe,
	.remove = ammvdec_avs_remove,
#endif
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = MULTI_DRIVER_NAME,
	}
};

static struct codec_profile_t ammvdec_avs_profile = {
	.name = "mavs",
	.profile = ""
};

static struct mconfig mavs_configs[] = {
	/*MC_PU32("stat", &stat),
	MC_PU32("debug_flag", &debug_flag),
	MC_PU32("error_recovery_mode", &error_recovery_mode),
	MC_PU32("hw->pic_type", &hw->pic_type),
	MC_PU32("radr", &radr),
	MC_PU32("vf_buf_num", &vf_buf_num),
	MC_PU32("vf_buf_num_used", &vf_buf_num_used),
	MC_PU32("canvas_base", &canvas_base),
	MC_PU32("firmware_sel", &firmware_sel),
	*/
};
static struct mconfig_node mavs_node;


static int __init ammvdec_avs_driver_init_module(void)
{
	pr_debug("ammvdec_avs module init\n");

	if (platform_driver_register(&ammvdec_avs_driver))
		pr_err("failed to register ammvdec_avs driver\n");
#ifdef DEBUG_WITH_SINGLE_MODE
	if (platform_driver_register(&amvdec_avs_driver)) {
		pr_info("failed to register amvdec_avs driver\n");
		return -ENODEV;
	}
#else
	//amvdec_avs_driver = amvdec_avs_driver;
#endif
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXBB)
		ammvdec_avs_profile.profile = "mavs+";

	vcodec_profile_register(&ammvdec_avs_profile);
	INIT_REG_NODE_CONFIGS("media.decoder", &mavs_node,
		"mavs", mavs_configs, CONFIG_FOR_RW);
	vcodec_feature_register(VFORMAT_AVS, 0);
	return 0;
}



static void __exit ammvdec_avs_driver_remove_module(void)
{
	pr_debug("ammvdec_avs module remove.\n");

	platform_driver_unregister(&ammvdec_avs_driver);
#ifdef DEBUG_WITH_SINGLE_MODE
	platform_driver_unregister(&amvdec_avs_driver);
#endif
}

/****************************************/
/*
module_param(stat, uint, 0664);
MODULE_PARM_DESC(stat, "\n amvdec_avs stat\n");
*/
/******************************************
 *module_param(run_flag, uint, 0664);
 *MODULE_PARM_DESC(run_flag, "\n run_flag\n");
 *
 *module_param(step_flag, uint, 0664);
 *MODULE_PARM_DESC(step_flag, "\n step_flag\n");
 *******************************************
 */
module_param(step, uint, 0664);
MODULE_PARM_DESC(step, "\n step\n");

module_param(debug, uint, 0664);
MODULE_PARM_DESC(debug, "\n debug\n");

module_param(debug_mask, uint, 0664);
MODULE_PARM_DESC(debug_mask, "\n debug_mask\n");

module_param(error_recovery_mode, uint, 0664);
MODULE_PARM_DESC(error_recovery_mode, "\n error_recovery_mode\n");

/******************************************
 *module_param(error_watchdog_threshold, uint, 0664);
 *MODULE_PARM_DESC(error_watchdog_threshold, "\n error_watchdog_threshold\n");
 *
 *module_param(error_watchdog_buf_threshold, uint, 0664);
 *MODULE_PARM_DESC(error_watchdog_buf_threshold,
 *			"\n error_watchdog_buf_threshold\n");
 *******************************************
 */
/*
module_param(pic_type, uint, 0444);
MODULE_PARM_DESC(pic_type, "\n amdec_vas picture type\n");
*/
module_param(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\nradr\n");

module_param(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\nrval\n");

module_param(dbg_cmd, uint, 0664);
MODULE_PARM_DESC(dbg_cmd, "\n dbg_cmd\n");

module_param(vf_buf_num, uint, 0664);
MODULE_PARM_DESC(vf_buf_num, "\nvf_buf_num\n");

/*
module_param(vf_buf_num_used, uint, 0664);
MODULE_PARM_DESC(vf_buf_num_used, "\nvf_buf_num_used\n");
*/
module_param(canvas_base, uint, 0664);
MODULE_PARM_DESC(canvas_base, "\ncanvas_base\n");


module_param(firmware_sel, uint, 0664);
MODULE_PARM_DESC(firmware_sel, "\n firmware_sel\n");

module_param(disable_longcabac_trans, uint, 0664);
MODULE_PARM_DESC(disable_longcabac_trans, "\n disable_longcabac_trans\n");

module_param(dec_control, uint, 0664);
MODULE_PARM_DESC(dec_control, "\n amvdec_vavs decoder control\n");

module_param(start_decode_buf_level, int, 0664);
MODULE_PARM_DESC(start_decode_buf_level,
		"\n avs start_decode_buf_level\n");

module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val,
	"\n avs decode_timeout_val\n");

module_param(error_handle_policy, uint, 0664);
MODULE_PARM_DESC(error_handle_policy,
	"\n avs error_handle_policy\n");

module_param(again_threshold, uint, 0664);
MODULE_PARM_DESC(again_threshold, "\n again_threshold\n");

module_param(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n amvdec_h265 udebug_flag\n");

module_param(udebug_pause_pos, uint, 0664);
MODULE_PARM_DESC(udebug_pause_pos, "\n udebug_pause_pos\n");

module_param(udebug_pause_val, uint, 0664);
MODULE_PARM_DESC(udebug_pause_val, "\n udebug_pause_val\n");

module_param(udebug_pause_decode_idx, uint, 0664);
MODULE_PARM_DESC(udebug_pause_decode_idx, "\n udebug_pause_decode_idx\n");

module_param(udebug_pause_ins_id, uint, 0664);
MODULE_PARM_DESC(udebug_pause_ins_id, "\n udebug_pause_ins_id\n");

module_param(start_decoding_delay, uint, 0664);
MODULE_PARM_DESC(start_decoding_delay, "\n start_decoding_delay\n");

module_param(pre_decode_buf_level, int, 0664);
MODULE_PARM_DESC(pre_decode_buf_level,
				"\n ammvdec_mavs pre_decode_buf_level\n");


#ifdef DEBUG_MULTI_WITH_AUTOMODE
module_param(debug_flag2, uint, 0664);
MODULE_PARM_DESC(debug_flag2, "\n debug_flag2\n");
#endif
module_param(force_fps, uint, 0664);
MODULE_PARM_DESC(force_fps, "\n force_fps\n");

#ifdef DEBUG_MULTI_FRAME_INS
module_param(delay, uint, 0664);
MODULE_PARM_DESC(delay, "\n delay\n");

module_param_array(max_run_count, uint, &max_decode_instance_num, 0664);

#endif

module_param_array(ins_udebug_flag, uint, &max_decode_instance_num, 0664);

module_param_array(max_process_time, uint, &max_decode_instance_num, 0664);

module_param_array(run_count, uint, &max_decode_instance_num, 0664);

module_param_array(max_get_frame_interval, uint,
	&max_decode_instance_num, 0664);


module_init(ammvdec_avs_driver_init_module);
module_exit(ammvdec_avs_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC AVS Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qi Wang <qi.wang@amlogic.com>");
