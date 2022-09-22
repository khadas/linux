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
#define DEBUG
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/sched/clock.h>
#include <linux/crc32.h>
#include <uapi/linux/tee.h>
#include <media/v4l2-mem2mem.h>

#include "../../../stream_input/amports/amports_priv.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "../../decoder/utils/decoder_mmu_box.h"
#include "../../decoder/utils/decoder_bmmu_box.h"
#include "../../decoder/utils/config_parser.h"
#include "../../decoder/utils/firmware.h"
#include "../../decoder/utils/vdec_v4l2_buffer_ops.h"
#include "../../decoder/utils/vdec.h"
#include "../../decoder/utils/amvdec.h"
#include "../../decoder/utils/vdec_feature.h"
#include "../../decoder/utils/decoder_dma_alloc.h"


#define HEVC_8K_LFTOFFSET_FIX
#define SUPPORT_LONG_TERM_RPS

#define CONSTRAIN_MAX_BUF_NUM

#define SWAP_HEVC_UCODE
#define DETREFILL_ENABLE

#define AGAIN_HAS_THRESHOLD
#define HEVC_PIC_STRUCT_SUPPORT
#define MULTI_INSTANCE_SUPPORT
#define USE_UNINIT_SEMA

			/* .buf_size = 0x100000*16,
			//4k2k , 0x100000 per buffer */
			/* 4096x2304 , 0x120000 per buffer */
#define MPRED_8K_MV_BUF_SIZE		(0x120000*4)
#define MPRED_4K_MV_BUF_SIZE		(0x120000)
#define MPRED_MV_BUF_SIZE		(0x3fc00)

#define MMU_COMPRESS_HEADER_SIZE_1080P  0x10000
#define MMU_COMPRESS_HEADER_SIZE_4K  0x48000
#define MMU_COMPRESS_HEADER_SIZE_8K  0x120000
#define DB_NUM 20

#define MAX_FRAME_4K_NUM 0x1200
#define MAX_FRAME_8K_NUM ((MAX_FRAME_4K_NUM) * 4)

#define H265_MMU_MAP_BUFFER       HEVC_ASSIST_SCRATCH_7

#define HEVC_ASSIST_MMU_MAP_ADDR                   0x3009

#define HEVC_CM_HEADER_START_ADDR                  0x3628
#define HEVC_SAO_MMU_VH1_ADDR                      0x363b
#define HEVC_SAO_MMU_VH0_ADDR                      0x363a

#define HEVC_DBLK_CFGB                             0x350b
#define HEVCD_MPP_DECOMP_AXIURG_CTL                0x34c7
#define SWAP_HEVC_OFFSET (3 * 0x1000)

#define MEM_NAME "codec_265"

#define SEND_LMEM_WITH_RPM
#define SUPPORT_10BIT

#ifndef STAT_KTHREAD
#define STAT_KTHREAD 0x40
#endif

#ifdef MULTI_INSTANCE_SUPPORT
#define MAX_DECODE_INSTANCE_NUM     9
#define MULTI_DRIVER_NAME "ammvdec_h265_v4l"
#endif
#define DRIVER_NAME "amvdec_h265_v4l"
#define DRIVER_HEADER_NAME "amvdec_h265_header"

#define PUT_INTERVAL        (HZ/100)
#define ERROR_SYSTEM_RESET_COUNT   200

#define PTS_NORMAL                0
#define PTS_NONE_REF_USE_DURATION 1

#define PTS_MODE_SWITCHING_THRESHOLD           3
#define PTS_MODE_SWITCHING_RECOVERY_THREASHOLD 3

#define DUR2PTS(x) ((x)*90/96)

#define MAX_SIZE_8K (8192 * 4608)
#define MAX_SIZE_4K (4096 * 2304)
#define MAX_SIZE_2K (1920 * 1088)

#define IS_8K_SIZE(w, h)  (((w) * (h)) > MAX_SIZE_4K)
#define IS_4K_SIZE(w, h)  (((w) * (h)) > (1920*1088))

#define SEI_UserDataITU_T_T35	4
#define INVALID_IDX -1  /* Invalid buffer index.*/

static struct semaphore h265_sema;

struct hevc_state_s;
static int hevc_print(struct hevc_state_s *hevc,
	int debug_flag, const char *fmt, ...);
static int hevc_print_cont(struct hevc_state_s *hevc,
	int debug_flag, const char *fmt, ...);
static int vh265_vf_states(struct vframe_states *states, void *);
static struct vframe_s *vh265_vf_peek(void *);
static struct vframe_s *vh265_vf_get(void *);
static void vh265_vf_put(struct vframe_s *, void *);
static int vh265_event_cb(int type, void *data, void *private_data);
#ifdef MULTI_INSTANCE_SUPPORT
static int vmh265_stop(struct hevc_state_s *hevc);
static s32 vh265_init(struct vdec_s *vdec);
static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask);
static void reset_process_time(struct hevc_state_s *hevc);
static void start_process_time(struct hevc_state_s *hevc);
static void restart_process_time(struct hevc_state_s *hevc);
static void timeout_process(struct hevc_state_s *hevc);
#else
static s32 vh265_init(struct hevc_state_s *hevc);
#endif
static void vh265_prot_init(struct hevc_state_s *hevc);
static int vh265_local_init(struct hevc_state_s *hevc);
static void vh265_check_timer_func(struct timer_list *timer);
static void config_decode_mode(struct hevc_state_s *hevc);

static const char vh265_dec_id[] = "vh265-dev";

#define PROVIDER_NAME   "decoder.h265"
#define MULTI_INSTANCE_PROVIDER_NAME    "vdec.h265"

static const struct vframe_operations_s vh265_vf_provider = {
	.peek = vh265_vf_peek,
	.get = vh265_vf_get,
	.put = vh265_vf_put,
	.event_cb = vh265_event_cb,
	.vf_states = vh265_vf_states,
};

#ifdef MULTI_INSTANCE_SUPPORT
#else
static struct vframe_provider_s vh265_vf_prov;
#endif

//0.3.42-g8104942
#define UCODE_SWAP_VERSION 3
#define UCODE_SWAP_SUBMIT_COUNT 42

static u32 enable_swap = 1;
static u32 bit_depth_luma;
static u32 bit_depth_chroma;
static u32 video_signal_type;
static int start_decode_buf_level = 0x8000;
static unsigned int decode_timeout_val = 200;

static u32 run_ready_min_buf_num = 1;
static u32 disable_ip_mode;
static u32 print_lcu_error = 1;
/*data_resend_policy:
	bit 0, stream base resend data when decoding buf empty
*/
static u32 data_resend_policy = 1;
static int poc_num_margin = 1000;
static int poc_error_limit = 30;

static u32 dirty_time_threshold = 2000;
static u32 dirty_count_threshold = 200;
static u32 dirty_buffersize_threshold = 0x800000;


#define VIDEO_SIGNAL_TYPE_AVAILABLE_MASK	0x20000000

#ifdef SUPPORT_10BIT
#define HEVC_CM_BODY_START_ADDR                    0x3626
#define HEVC_CM_BODY_LENGTH                        0x3627
#define HEVC_CM_HEADER_LENGTH                      0x3629
#define HEVC_CM_HEADER_OFFSET                      0x362b
#define HEVC_SAO_CTRL9                             0x362d
#define LOSLESS_COMPRESS_MODE
/* DOUBLE_WRITE_MODE is enabled only when NV21 8 bit output is needed */
/* double_write_mode:
 *	0, no double write;
 *	1, 1:1 ratio;
 *	2, (1/4):(1/4) ratio;
 *	3, (1/4):(1/4) ratio, with both compressed frame included
 *	4, (1/2):(1/2) ratio;
 *	5, (1/2):(1/2) ratio, with both compressed frame included
 *	8, (1/8):(1/8) ratio,  from t7
 *	0x10, double write only
 *	0x100, if > 1080p,use mode 4,else use mode 1;
 *	0x200, if > 1080p,use mode 2,else use mode 1;
 *	0x300, if > 720p, use mode 4, else use mode 1;
 *	0x1000,if > 1080p,use mode 3, else if > 960*540, use mode 4, else use mode 1;
 */
static u32 double_write_mode;
static u32 mem_map_mode; /* 0:linear 1:32x32 2:64x32 ; m8baby test1902 */
static u32 enable_mem_saving = 1;
static u32 workaround_enable;
static u32 force_w_h;
#endif
static u32 force_fps;
static u32 pts_unstable;
#define H265_DEBUG_BUFMGR                   0x01
#define H265_DEBUG_BUFMGR_MORE              0x02
#define H265_DEBUG_DETAIL                   0x04
#define H265_DEBUG_REG                      0x08
#define H265_DEBUG_MAN_SEARCH_NAL           0x10
#define H265_DEBUG_MAN_SKIP_NAL             0x20
#define H265_DEBUG_DISPLAY_CUR_FRAME        0x40
#define H265_DEBUG_FORCE_CLK                0x80
#define H265_DEBUG_SEND_PARAM_WITH_REG      0x100
#define H265_DEBUG_NO_DISPLAY               0x200
#define H265_DEBUG_DISCARD_NAL              0x400
#define H265_DEBUG_OUT_PTS                  0x800
#define H265_DEBUG_DUMP_PIC_LIST            0x1000
#define H265_DEBUG_PRINT_SEI		        0x2000
#define H265_DEBUG_PIC_STRUCT				0x4000
#define H265_DEBUG_HAS_AUX_IN_SLICE			0x8000
#define H265_DEBUG_DIS_LOC_ERROR_PROC       0x10000
#define H265_DEBUG_DIS_SYS_ERROR_PROC       0x20000
#define H265_NO_CHANG_DEBUG_FLAG_IN_CODE    0x40000
#define H265_DEBUG_TRIG_SLICE_SEGMENT_PROC  0x80000
#define H265_DEBUG_HW_RESET                 0x100000
#define H265_CFG_CANVAS_IN_DECODE           0x200000
#define H265_DEBUG_DV                       0x400000
#define H265_DEBUG_NO_EOS_SEARCH_DONE       0x800000
#define H265_DEBUG_NOT_USE_LAST_DISPBUF     0x1000000
#define H265_DEBUG_IGNORE_CONFORMANCE_WINDOW	0x2000000
#define H265_DEBUG_WAIT_DECODE_DONE_WHEN_STOP   0x4000000
#ifdef MULTI_INSTANCE_SUPPORT
#define PRINT_FLAG_ERROR		0x0
#define IGNORE_PARAM_FROM_CONFIG	0x08000000
#define PRINT_FRAMEBASE_DATA		0x10000000
#define PRINT_FLAG_VDEC_STATUS		0x20000000
#define PRINT_FLAG_VDEC_DETAIL		0x40000000
#define PRINT_FLAG_V4L_DETAIL		0x80000000
#endif

#define BUF_POOL_SIZE	32
#define MAX_BUF_NUM 24
#define MAX_REF_PIC_NUM 24
#define MAX_REF_ACTIVE  16

#ifdef MV_USE_FIXED_BUF
#define BMMU_MAX_BUFFERS (BUF_POOL_SIZE + 1)
#define VF_BUFFER_IDX(n)	(n)
#define BMMU_WORKSPACE_ID	(BUF_POOL_SIZE)
#else
#define BMMU_MAX_BUFFERS (BUF_POOL_SIZE + 1 + MAX_REF_PIC_NUM)
#define VF_BUFFER_IDX(n)	(n)
#define BMMU_WORKSPACE_ID	(BUF_POOL_SIZE)
#define MV_BUFFER_IDX(n) (BUF_POOL_SIZE + 1 + n)
#endif

#define HEVC_MV_INFO   0x310d
#define HEVC_QP_INFO   0x3137
#define HEVC_SKIP_INFO 0x3136

const u32 h265_version = 201602101;
static u32 debug_mask = 0xffffffff;
static u32 log_mask;
static u32 debug;
static u32 radr;
static u32 rval;
static u32 dbg_cmd;
static u32 dump_nal;
static u32 dbg_skip_decode_index;
/*
 * bit 0~3, for HEVCD_IPP_AXIIF_CONFIG endian config
 * bit 8~23, for HEVC_SAO_CTRL1 endian config
 */
static u32 endian;
#define HEVC_CONFIG_BIG_ENDIAN     ((0x880 << 8) | 0x8)
#define HEVC_CONFIG_LITTLE_ENDIAN  ((0xff0 << 8) | 0xf)

/*for debug*/
static u32 force_bufspec;

/*
	udebug_flag:
	bit 0, enable ucode print
	bit 1, enable ucode detail print
	bit [31:16] not 0, pos to dump lmem
		bit 2, pop bits to lmem
		bit [11:8], pre-pop bits for alignment (when bit 2 is 1)
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

static u32 decode_pic_begin;
static uint slice_parse_begin;
static u32 step;
static bool is_reset;

#ifdef CONSTRAIN_MAX_BUF_NUM
static u32 run_ready_max_vf_only_num;
static u32 run_ready_display_q_num;
/*
	0: not check
	0xff: work_pic_num
*/
static u32 run_ready_max_buf_num = 0xff;
#endif

static u32 dynamic_buf_num_margin = 7;
static u32 buf_alloc_width;
static u32 buf_alloc_height;

static u32 max_buf_num = 18;
static u32 buf_alloc_size;
/*
 *bit[0]: 0,
 *bit[1]: 0, always release cma buffer when stop
 *bit[1]: 1, never release cma buffer when stop
 *bit[0]: 1, when stop, release cma buffer if blackout is 1;
 *do not release cma buffer is blackout is not 1
 *
 *bit[2]: 0, when start decoding, check current displayed buffer
 *	 (only for buffer decoded by h265) if blackout is 0
 *	 1, do not check current displayed buffer
 *
 *bit[3]: 1, if blackout is not 1, do not release current
 *			displayed cma buffer always.
 */
/* set to 1 for fast play;
 *	set to 8 for other case of "keep last frame"
 */
static u32 buffer_mode = 1;

/* buffer_mode_dbg: debug only*/
static u32 buffer_mode_dbg = 0xffff0000;
/*
 *bit[1:0]PB_skip_mode: 0, start decoding at begin;
 *1, start decoding after first I;
 *2, only decode and display none error picture;
 *3, start decoding and display after IDR,etc
 *bit[31:16] PB_skip_count_after_decoding (decoding but not display),
 *only for mode 0 and 1.
 */
static u32 nal_skip_policy = 2;

static u32 c2_nal_skip_policy = 0;

/*
 *bit 0, 1: only display I picture;
 *bit 1, 1: only decode I picture;
 */
static u32 i_only_flag;
static u32 skip_nal_count = 500;
/*
bit 0, fast output first I picture
*/
static u32 fast_output_enable = 1;

static u32 frmbase_cont_bitlevel = 0x60;

/*
use_cma: 1, use both reserver memory and cma for buffers
2, only use cma for buffers
*/
static u32 use_cma = 2;

#define AUX_BUF_ALIGN(adr) ((adr + 0xf) & (~0xf))
/*
static u32 prefix_aux_buf_size = (16 * 1024);
static u32 suffix_aux_buf_size;
*/
static u32 prefix_aux_buf_size = (12 * 1024);
static u32 suffix_aux_buf_size = (12 * 1024);

static u32 max_decoding_time;
/*
 *error handling
 */
/*error_handle_policy:
 *bit 0: 0, auto skip error_skip_nal_count nals before error recovery;
 *1, skip error_skip_nal_count nals before error recovery;
 *bit 1 (valid only when bit0 == 1):
 *1, wait vps/sps/pps after error recovery;
 *bit 2 (valid only when bit0 == 0):
 *0, auto search after error recovery (hevc_recover() called);
 *1, manual search after error recovery
 *(change to auto search after get IDR: WRITE_VREG(NAL_SEARCH_CTL, 0x2))
 *
 *bit 4: 0, set error_mark after reset/recover
 *	1, do not set error_mark after reset/recover
 *
 *bit 5: 0, check total lcu for every picture
 *	1, do not check total lcu
 *
 *bit 6: 0, do not check head error
 *	1, check head error
 *
 *bit 7: 0, allow to print over decode
 *       1, NOT allow to print over decode
 *
 *bit 8: 0, use interlace policy
 *       1, NOT use interlace policy
 *bit 9: 0, discard dirty data on playback start
 *       1, do not discard dirty data on playback start
 *
 */
static u32 error_handle_policy;
static u32 error_skip_nal_count = 6;
static u32 error_handle_threshold = 30;
static u32 error_handle_nal_skip_threshold = 10;
static u32 error_handle_system_threshold = 30;
static u32 interlace_enable = 1;
#ifdef MULTI_INSTANCE_SUPPORT
#else
static u32 fr_hint_status;
#endif
/*
 *	parser_sei_enable:
 *	bit 0, sei;
 *	bit 1, sei_suffix (fill aux buf)
 *	bit 2, fill sei to aux buf (when bit 0 is 1)
 *	bit 8, debug flag
 */
static u32 parser_sei_enable;
static u32 parser_dolby_vision_enable = 1;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
static u32 dolby_meta_with_el;
static u32 dolby_el_flush_th = 2;
#endif

/* this is only for h265 mmu enable */
static u32 mmu_enable = 1;
static u32 mmu_enable_force;
static u32 work_buf_size;
static unsigned int force_disp_pic_index;
static unsigned int disp_vframe_valve_level;
static int pre_decode_buf_level = 0x1000;
static unsigned int pic_list_debug;
#ifdef HEVC_8K_LFTOFFSET_FIX
/* performance_profile: bit 0, multi slice in ucode */
static unsigned int performance_profile = 1;
#endif
#ifdef MULTI_INSTANCE_SUPPORT
static unsigned int max_decode_instance_num
				= MAX_DECODE_INSTANCE_NUM;
static unsigned int decode_frame_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int display_frame_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int max_process_time[MAX_DECODE_INSTANCE_NUM];
static unsigned int max_get_frame_interval[MAX_DECODE_INSTANCE_NUM];
static unsigned int run_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int input_empty[MAX_DECODE_INSTANCE_NUM];
static unsigned int not_run_ready[MAX_DECODE_INSTANCE_NUM];
static unsigned int ref_frame_mark_flag[MAX_DECODE_INSTANCE_NUM] =
{1, 1, 1, 1, 1, 1, 1, 1, 1};

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static unsigned char get_idx(struct hevc_state_s *hevc);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
static u32 dv_toggle_prov_name;

static u32 dv_debug;

static u32 force_bypass_dvenl;
#endif
#endif

/*
 *[3:0] 0: default use config from omx.
 *      1: force enable fence.
 *      2: disable fence.
 *[7:4] 0: fence use for driver.
 *      1: fence fd use for app.
 */
static u32 force_config_fence;

/*
 *The parameter sps_max_dec_pic_buffering_minus1_0+1
 *in SPS is the minimum DPB size required for stream
 *(note: this parameter does not include the frame
 *currently being decoded) +1 (decoding the current
 *frame) +1 (decoding the current frame will only
 *update refrence frame information, such as reference
 *relation, when the next frame is decoded)
 */
static u32 detect_stuck_buffer_margin = 3;


#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
#define get_dbg_flag(hevc) ((debug_mask & (1 << hevc->index)) ? debug : 0)
#define get_dbg_flag2(hevc) ((debug_mask & (1 << get_idx(hevc))) ? debug : 0)
#define is_log_enable(hevc) ((log_mask & (1 << hevc->index)) ? 1 : 0)
#else
#define get_dbg_flag(hevc) debug
#define get_dbg_flag2(hevc) debug
#define is_log_enable(hevc) (log_mask ? 1 : 0)
#define get_valid_double_write_mode(hevc) double_write_mode
#define get_buf_alloc_width(hevc) buf_alloc_width
#define get_buf_alloc_height(hevc) buf_alloc_height
#define get_dynamic_buf_num_margin(hevc) dynamic_buf_num_margin
#endif
#define get_buffer_mode(hevc) buffer_mode


static DEFINE_SPINLOCK(h265_lock);
struct task_struct *h265_task = NULL;
#undef DEBUG_REG
#ifdef DEBUG_REG
void WRITE_VREG_DBG(unsigned adr, unsigned val)
{
	if (debug & H265_DEBUG_REG)
		pr_info("%s(%x, %x)\n", __func__, adr, val);
	WRITE_VREG(adr, val);
}

#undef WRITE_VREG
#define WRITE_VREG WRITE_VREG_DBG
#endif
extern u32 trickmode_i;

static DEFINE_MUTEX(vh265_mutex);
static DEFINE_MUTEX(vh265_log_mutex);

static u32 without_display_mode;
static u32 mv_buf_dynamic_alloc;

/**************************************************
 *
 *h265 buffer management include
 *
 ***************************************************
 */
enum NalUnitType {
	NAL_UNIT_CODED_SLICE_TRAIL_N = 0,	/* 0 */
	NAL_UNIT_CODED_SLICE_TRAIL_R,	/* 1 */

	NAL_UNIT_CODED_SLICE_TSA_N,	/* 2 */
	/* Current name in the spec: TSA_R */
	NAL_UNIT_CODED_SLICE_TLA,	/* 3 */

	NAL_UNIT_CODED_SLICE_STSA_N,	/* 4 */
	NAL_UNIT_CODED_SLICE_STSA_R,	/* 5 */

	NAL_UNIT_CODED_SLICE_RADL_N,	/* 6 */
	/* Current name in the spec: RADL_R */
	NAL_UNIT_CODED_SLICE_DLP,	/* 7 */

	NAL_UNIT_CODED_SLICE_RASL_N,	/* 8 */
	/* Current name in the spec: RASL_R */
	NAL_UNIT_CODED_SLICE_TFD,	/* 9 */

	NAL_UNIT_RESERVED_10,
	NAL_UNIT_RESERVED_11,
	NAL_UNIT_RESERVED_12,
	NAL_UNIT_RESERVED_13,
	NAL_UNIT_RESERVED_14,
	NAL_UNIT_RESERVED_15,

	/* Current name in the spec: BLA_W_LP */
	NAL_UNIT_CODED_SLICE_BLA,	/* 16 */
	/* Current name in the spec: BLA_W_DLP */
	NAL_UNIT_CODED_SLICE_BLANT,	/* 17 */
	NAL_UNIT_CODED_SLICE_BLA_N_LP,	/* 18 */
	/* Current name in the spec: IDR_W_DLP */
	NAL_UNIT_CODED_SLICE_IDR,	/* 19 */
	NAL_UNIT_CODED_SLICE_IDR_N_LP,	/* 20 */
	NAL_UNIT_CODED_SLICE_CRA,	/* 21 */
	NAL_UNIT_RESERVED_22,
	NAL_UNIT_RESERVED_23,

	NAL_UNIT_RESERVED_24,
	NAL_UNIT_RESERVED_25,
	NAL_UNIT_RESERVED_26,
	NAL_UNIT_RESERVED_27,
	NAL_UNIT_RESERVED_28,
	NAL_UNIT_RESERVED_29,
	NAL_UNIT_RESERVED_30,
	NAL_UNIT_RESERVED_31,

	NAL_UNIT_VPS,		/* 32 */
	NAL_UNIT_SPS,		/* 33 */
	NAL_UNIT_PPS,		/* 34 */
	NAL_UNIT_ACCESS_UNIT_DELIMITER,	/* 35 */
	NAL_UNIT_EOS,		/* 36 */
	NAL_UNIT_EOB,		/* 37 */
	NAL_UNIT_FILLER_DATA,	/* 38 */
	NAL_UNIT_SEI,		/* 39 Prefix SEI */
	NAL_UNIT_SEI_SUFFIX,	/* 40 Suffix SEI */
	NAL_UNIT_RESERVED_41,
	NAL_UNIT_RESERVED_42,
	NAL_UNIT_RESERVED_43,
	NAL_UNIT_RESERVED_44,
	NAL_UNIT_RESERVED_45,
	NAL_UNIT_RESERVED_46,
	NAL_UNIT_RESERVED_47,
	NAL_UNIT_UNSPECIFIED_48,
	NAL_UNIT_UNSPECIFIED_49,
	NAL_UNIT_UNSPECIFIED_50,
	NAL_UNIT_UNSPECIFIED_51,
	NAL_UNIT_UNSPECIFIED_52,
	NAL_UNIT_UNSPECIFIED_53,
	NAL_UNIT_UNSPECIFIED_54,
	NAL_UNIT_UNSPECIFIED_55,
	NAL_UNIT_UNSPECIFIED_56,
	NAL_UNIT_UNSPECIFIED_57,
	NAL_UNIT_UNSPECIFIED_58,
	NAL_UNIT_UNSPECIFIED_59,
	NAL_UNIT_UNSPECIFIED_60,
	NAL_UNIT_UNSPECIFIED_61,
	NAL_UNIT_UNSPECIFIED_62,
	NAL_UNIT_UNSPECIFIED_63,
	NAL_UNIT_INVALID,
};

/* --------------------------------------------------- */
/* Amrisc Software Interrupt */
/* --------------------------------------------------- */
#define AMRISC_STREAM_EMPTY_REQ 0x01
#define AMRISC_PARSER_REQ       0x02
#define AMRISC_MAIN_REQ         0x04

/* --------------------------------------------------- */
/* HEVC_DEC_STATUS define */
/* --------------------------------------------------- */
#define HEVC_DEC_IDLE                        0x0
#define HEVC_NAL_UNIT_VPS                    0x1
#define HEVC_NAL_UNIT_SPS                    0x2
#define HEVC_NAL_UNIT_PPS                    0x3
#define HEVC_NAL_UNIT_CODED_SLICE_SEGMENT    0x4
#define HEVC_CODED_SLICE_SEGMENT_DAT         0x5
#define HEVC_SLICE_DECODING                  0x6
#define HEVC_NAL_UNIT_SEI                    0x7
#define HEVC_SLICE_SEGMENT_DONE              0x8
#define HEVC_NAL_SEARCH_DONE                 0x9
#define HEVC_DECPIC_DATA_DONE                0xa
#define HEVC_DECPIC_DATA_ERROR               0xb
#define HEVC_SEI_DAT                         0xc
#define HEVC_SEI_DAT_DONE                    0xd
#define HEVC_NAL_DECODE_DONE				0xe
#define HEVC_OVER_DECODE					0xf

#define HEVC_DATA_REQUEST           0x12

#define HEVC_DECODE_BUFEMPTY        0x20
#define HEVC_DECODE_TIMEOUT         0x21
#define HEVC_SEARCH_BUFEMPTY        0x22
#define HEVC_DECODE_OVER_SIZE       0x23
#define HEVC_DECODE_BUFEMPTY2       0x24
#define HEVC_FIND_NEXT_PIC_NAL				0x50
#define HEVC_FIND_NEXT_DVEL_NAL				0x51

#define HEVC_DUMP_LMEM				0x30

#define HEVC_4k2k_60HZ_NOT_SUPPORT	0x80
#define HEVC_DISCARD_NAL         0xf0
#define HEVC_ACTION_DEC_CONT     0xfd
#define HEVC_ACTION_ERROR        0xfe
#define HEVC_ACTION_DONE         0xff

/* --------------------------------------------------- */
/* Include "parser_cmd.h" */
/* --------------------------------------------------- */
#define PARSER_CMD_SKIP_CFG_0 0x0000090b

#define PARSER_CMD_SKIP_CFG_1 0x1b14140f

#define PARSER_CMD_SKIP_CFG_2 0x001b1910

#define PARSER_CMD_NUMBER 37

/**************************************************
 *
 *h265 buffer management
 *
 ***************************************************
 */
/* #define BUFFER_MGR_ONLY */
/* #define CONFIG_HEVC_CLK_FORCED_ON */
#define   MCRCC_ENABLE
#define INVALID_POC 0x80000000

#define HEVC_DEC_STATUS_REG       HEVC_ASSIST_SCRATCH_0
#define HEVC_RPM_BUFFER           HEVC_ASSIST_SCRATCH_1
#define HEVC_SHORT_TERM_RPS       HEVC_ASSIST_SCRATCH_2
#define HEVC_VPS_BUFFER           HEVC_ASSIST_SCRATCH_3
#define HEVC_SPS_BUFFER           HEVC_ASSIST_SCRATCH_4
#define HEVC_PPS_BUFFER           HEVC_ASSIST_SCRATCH_5
#define HEVC_SAO_UP               HEVC_ASSIST_SCRATCH_6
#define HEVC_STREAM_SWAP_BUFFER   HEVC_ASSIST_SCRATCH_7
#define HEVC_STREAM_SWAP_BUFFER2  HEVC_ASSIST_SCRATCH_8
#define HEVC_sao_mem_unit         HEVC_ASSIST_SCRATCH_9
#define HEVC_SAO_ABV              HEVC_ASSIST_SCRATCH_A
#define HEVC_sao_vb_size          HEVC_ASSIST_SCRATCH_B
#define HEVC_SAO_VB               HEVC_ASSIST_SCRATCH_C
#define HEVC_SCALELUT             HEVC_ASSIST_SCRATCH_D
#define HEVC_WAIT_FLAG            HEVC_ASSIST_SCRATCH_E
#define RPM_CMD_REG               HEVC_ASSIST_SCRATCH_F
#define LMEM_DUMP_ADR             HEVC_ASSIST_SCRATCH_F
#define HEVC_DECODE_SIZE		HEVC_ASSIST_SCRATCH_N
	/*do not define ENABLE_SWAP_TEST*/
#define HEVC_AUX_ADR			HEVC_ASSIST_SCRATCH_L
#define HEVC_AUX_DATA_SIZE		HEVC_ASSIST_SCRATCH_M

#define DEBUG_REG1              HEVC_ASSIST_SCRATCH_G
#define DEBUG_REG2              HEVC_ASSIST_SCRATCH_H
/*
 *ucode parser/search control
 *bit 0:  0, header auto parse; 1, header manual parse
 *bit 1:  0, auto skip for noneseamless stream; 1, no skip
 *bit [3:2]: valid when bit1==0;
 *0, auto skip nal before first vps/sps/pps/idr;
 *1, auto skip nal before first vps/sps/pps
 *2, auto skip nal before first  vps/sps/pps,
 *	and not decode until the first I slice (with slice address of 0)
 *
 *3, auto skip before first I slice (nal_type >=16 && nal_type<=21)
 *bit [15:4] nal skip count (valid when bit0 == 1 (manual mode) )
 *bit [16]: for NAL_UNIT_EOS when bit0 is 0:
 *	0, send SEARCH_DONE to arm ;  1, do not send SEARCH_DONE to arm
 *bit [17]: for NAL_SEI when bit0 is 0:
 *	0, do not parse/fetch SEI in ucode;
 *	1, parse/fetch SEI in ucode
 *bit [18]: for NAL_SEI_SUFFIX when bit0 is 0:
 *	0, do not fetch NAL_SEI_SUFFIX to aux buf;
 *	1, fetch NAL_SEL_SUFFIX data to aux buf
 *bit [19]:
 *	0, parse NAL_SEI in ucode
 *	1, fetch NAL_SEI to aux buf
 *bit [20]: for DOLBY_VISION_META
 *	0, do not fetch DOLBY_VISION_META to aux buf
 *	1, fetch DOLBY_VISION_META to aux buf
 */
#define NAL_SEARCH_CTL            HEVC_ASSIST_SCRATCH_I
	/*read only*/
#define CUR_NAL_UNIT_TYPE       HEVC_ASSIST_SCRATCH_J
	/*
	[15 : 8] rps_set_id
	[7 : 0] start_decoding_flag
	*/
#define HEVC_DECODE_INFO       HEVC_ASSIST_SCRATCH_1
	/*set before start decoder*/
#define HEVC_DECODE_MODE		HEVC_ASSIST_SCRATCH_J
#define HEVC_DECODE_MODE2		HEVC_ASSIST_SCRATCH_H
#define DECODE_STOP_POS         HEVC_ASSIST_SCRATCH_K

#define DECODE_MODE_SINGLE					0x0
#define DECODE_MODE_MULTI_FRAMEBASE			0x1
#define DECODE_MODE_MULTI_STREAMBASE		0x2
#define DECODE_MODE_MULTI_DVBAL				0x3
#define DECODE_MODE_MULTI_DVENL				0x4

#define MAX_INT 0x7FFFFFFF

#define RPM_BEGIN                                              0x100
#define modification_list_cur                                  0x148
#define RPM_END                                                0x180
#ifdef SUPPORT_LONG_TERM_RPS
#define RPS_END  0x8000
#define RPS_LT_BIT 		14
#define RPS_USED_BIT        13
#define RPS_SIGN_BIT        12
#else
#define RPS_END		0x8000
#define RPS_USED_BIT        14
#define RPS_SIGN_BIT        13
#endif
/* MISC_FLAG0 */
#define PCM_LOOP_FILTER_DISABLED_FLAG_BIT       0
#define PCM_ENABLE_FLAG_BIT             1
#define LOOP_FILER_ACROSS_TILES_ENABLED_FLAG_BIT    2
#define PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT  3
#define DEBLOCKING_FILTER_OVERRIDE_ENABLED_FLAG_BIT 4
#define PPS_DEBLOCKING_FILTER_DISABLED_FLAG_BIT     5
#define DEBLOCKING_FILTER_OVERRIDE_FLAG_BIT     6
#define SLICE_DEBLOCKING_FILTER_DISABLED_FLAG_BIT   7
#define SLICE_SAO_LUMA_FLAG_BIT             8
#define SLICE_SAO_CHROMA_FLAG_BIT           9
#define SLICE_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT 10

union param_u {
	struct {
		unsigned short data[RPM_END - RPM_BEGIN];
	} l;
	struct {
		/* from ucode lmem, do not change this struct */
		unsigned short CUR_RPS[0x10];
		unsigned short num_ref_idx_l0_active;
		unsigned short num_ref_idx_l1_active;
		unsigned short slice_type;
		unsigned short slice_temporal_mvp_enable_flag;
		unsigned short dependent_slice_segment_flag;
		unsigned short slice_segment_address;
		unsigned short num_title_rows_minus1;
		unsigned short pic_width_in_luma_samples;
		unsigned short pic_height_in_luma_samples;
		unsigned short log2_min_coding_block_size_minus3;
		unsigned short log2_diff_max_min_coding_block_size;
		unsigned short log2_max_pic_order_cnt_lsb_minus4;
		unsigned short POClsb;
		unsigned short collocated_from_l0_flag;
		unsigned short collocated_ref_idx;
		unsigned short log2_parallel_merge_level;
		unsigned short five_minus_max_num_merge_cand;
		unsigned short sps_num_reorder_pics_0;
		unsigned short modification_flag;
		unsigned short tiles_enabled_flag;
		unsigned short num_tile_columns_minus1;
		unsigned short num_tile_rows_minus1;
		unsigned short tile_width[12];
		unsigned short tile_height[8];
		unsigned short misc_flag0;
		unsigned short pps_beta_offset_div2;
		unsigned short pps_tc_offset_div2;
		unsigned short slice_beta_offset_div2;
		unsigned short slice_tc_offset_div2;
		unsigned short pps_cb_qp_offset;
		unsigned short pps_cr_qp_offset;
		unsigned short first_slice_segment_in_pic_flag;
		unsigned short m_temporalId;
		unsigned short m_nalUnitType;

		unsigned short vui_num_units_in_tick_hi;
		unsigned short vui_num_units_in_tick_lo;
		unsigned short vui_time_scale_hi;
		unsigned short vui_time_scale_lo;
		unsigned short bit_depth;
		unsigned short profile_etc;
		unsigned short sei_frame_field_info;
		unsigned short video_signal_type;
		unsigned short modification_list[0x20];
		unsigned short conformance_window_flag;
		unsigned short conf_win_left_offset;
		unsigned short conf_win_right_offset;
		unsigned short conf_win_top_offset;
		unsigned short conf_win_bottom_offset;
		unsigned short chroma_format_idc;
		unsigned short color_description;
		unsigned short aspect_ratio_idc;
		unsigned short sar_width;
		unsigned short sar_height;
		unsigned short sps_max_dec_pic_buffering_minus1_0;
	} p;
};

#define RPM_BUF_SIZE (0x80*2)
/* non mmu mode lmem size : 0x400, mmu mode : 0x500*/
#define LMEM_BUF_SIZE (0x500 * 2)

struct buff_s {
	u32 buf_start;
	u32 buf_size;
	u32 buf_end;
};

struct BuffInfo_s {
	u32 max_width;
	u32 max_height;
	unsigned int start_adr;
	unsigned int end_adr;
	struct buff_s ipp;
	struct buff_s sao_abv;
	struct buff_s sao_vb;
	struct buff_s short_term_rps;
	struct buff_s vps;
	struct buff_s sps;
	struct buff_s pps;
	struct buff_s sao_up;
	struct buff_s swap_buf;
	struct buff_s swap_buf2;
	struct buff_s scalelut;
	struct buff_s dblk_para;
	struct buff_s dblk_data;
	struct buff_s dblk_data2;
	struct buff_s mmu_vbh;
	struct buff_s cm_header;
	struct buff_s mpred_above;
#ifdef MV_USE_FIXED_BUF
	struct buff_s mpred_mv;
#endif
	struct buff_s rpm;
	struct buff_s lmem;
};

	/*mmu_vbh buf is used by HEVC_SAO_MMU_VH0_ADDR, HEVC_SAO_MMU_VH1_ADDR*/
#define VBH_BUF_SIZE_1080P 0x3000
#define VBH_BUF_SIZE_4K 0x5000
#define VBH_BUF_SIZE_8K 0xa000
#define VBH_BUF_SIZE(bufspec) (bufspec->mmu_vbh.buf_size / 2)
	/*mmu_vbh_dw buf is used by HEVC_SAO_MMU_VH0_ADDR2,HEVC_SAO_MMU_VH1_ADDR2,
		HEVC_DW_VH0_ADDDR, HEVC_DW_VH1_ADDDR*/
#define DW_VBH_BUF_SIZE_1080P (VBH_BUF_SIZE_1080P * 2)
#define DW_VBH_BUF_SIZE_4K (VBH_BUF_SIZE_4K * 2)
#define DW_VBH_BUF_SIZE_8K (VBH_BUF_SIZE_8K * 2)
#define DW_VBH_BUF_SIZE(bufspec) (bufspec->mmu_vbh_dw.buf_size / 4)

/* necessary 4K page size align for t7/t3 decoder and after */
#define WORKBUF_ALIGN(addr) (ALIGN(addr, PAGE_SIZE))

#define WORK_BUF_SPEC_NUM 6
static struct BuffInfo_s amvh265_workbuff_spec[WORK_BUF_SPEC_NUM] = {
	{
		/* 8M bytes */
		.max_width = 1920,
		.max_height = 1088,
		.ipp = {
			/* IPP work space calculation :
			 *   4096 * (Y+CbCr+Flags) = 12k, round to 16k
			 */
			.buf_size = 0x4000,
		},
		.sao_abv = {
			.buf_size = 0x30000,
		},
		.sao_vb = {
			.buf_size = 0x30000,
		},
		.short_term_rps = {
			/* SHORT_TERM_RPS - Max 64 set, 16 entry every set,
			 *   total 64x16x2 = 2048 bytes (0x800)
			 */
			.buf_size = 0x800,
		},
		.vps = {
			/* VPS STORE AREA - Max 16 VPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.sps = {
			/* SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.pps = {
			/* PPS STORE AREA - Max 64 PPS, each has 0x80 bytes,
			 *   total 0x2000 bytes
			 */
			.buf_size = 0x2000,
		},
		.sao_up = {
			/* SAO UP STORE AREA - Max 640(10240/16) LCU,
			 *   each has 16 bytes total 0x2800 bytes
			 */
			.buf_size = 0x2800,
		},
		.swap_buf = {
			/* 256cyclex64bit = 2K bytes 0x800
			 *   (only 144 cycles valid)
			 */
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
			/* support up to 32 SCALELUT 1024x32 =
			 *   32Kbytes (0x8000)
			 */
			.buf_size = 0x8000,
		},
		.dblk_para = {
#ifdef SUPPORT_10BIT
			.buf_size = 0x40000,
#else
			/* DBLK -> Max 256(4096/16) LCU, each para
			 *512bytes(total:0x20000), data 1024bytes(total:0x40000)
			 */
			.buf_size = 0x20000,
#endif
		},
		.dblk_data = {
			.buf_size = 0x40000,
		},
		.dblk_data2 = {
			.buf_size = 0x80000 * 2,
		}, /*dblk data for adapter*/
		.mmu_vbh = {
			.buf_size = 0x5000, /*2*16*2304/4, 4K*/
		},
		.mpred_above = {
			.buf_size = 0x8000,
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {/* 1080p, 0x40000 per buffer */
			.buf_size = 0x40000 * MAX_REF_PIC_NUM,
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		}
	},
	{
		.max_width = 4096,
		.max_height = 2048,
		.ipp = {
			/* IPP work space calculation :
			 *   4096 * (Y+CbCr+Flags) = 12k, round to 16k
			 */
			.buf_size = 0x4000,
		},
		.sao_abv = {
			.buf_size = 0x30000,
		},
		.sao_vb = {
			.buf_size = 0x30000,
		},
		.short_term_rps = {
			/* SHORT_TERM_RPS - Max 64 set, 16 entry every set,
			 *   total 64x16x2 = 2048 bytes (0x800)
			 */
			.buf_size = 0x800,
		},
		.vps = {
			/* VPS STORE AREA - Max 16 VPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.sps = {
			/* SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.pps = {
			/* PPS STORE AREA - Max 64 PPS, each has 0x80 bytes,
			 *   total 0x2000 bytes
			 */
			.buf_size = 0x2000,
		},
		.sao_up = {
			/* SAO UP STORE AREA - Max 640(10240/16) LCU,
			 *   each has 16 bytes total 0x2800 bytes
			 */
			.buf_size = 0x2800,
		},
		.swap_buf = {
			/* 256cyclex64bit = 2K bytes 0x800
			 *   (only 144 cycles valid)
			 */
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
			/* support up to 32 SCALELUT 1024x32 = 32Kbytes
			 *   (0x8000)
			 */
			.buf_size = 0x8000,
		},
		.dblk_para = {
			/* DBLK -> Max 256(4096/16) LCU, each para
			 *   512bytes(total:0x20000),
			 *   data 1024bytes(total:0x40000)
			 */
			.buf_size = 0x20000,
		},
		.dblk_data = {
			.buf_size = 0x80000,
		},
		.dblk_data2 = {
			.buf_size = 0x80000,
		}, /*dblk data for adapter*/
		.mmu_vbh = {
			.buf_size = 0x5000, /*2*16*2304/4, 4K*/
		},
		.mpred_above = {
			.buf_size = 0x8000,
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
			/* .buf_size = 0x100000*16,
			//4k2k , 0x100000 per buffer */
			/* 4096x2304 , 0x120000 per buffer */
			.buf_size = MPRED_4K_MV_BUF_SIZE * MAX_REF_PIC_NUM,
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		}
	},

	{
		.max_width = 4096*2,
		.max_height = 2048*2,
		.ipp = {
			// IPP work space calculation : 4096 * (Y+CbCr+Flags) = 12k, round to 16k
			.buf_size = 0x4000*2,
		},
		.sao_abv = {
			.buf_size = 0x30000*2,
		},
		.sao_vb = {
			.buf_size = 0x30000*2,
		},
		.short_term_rps = {
			// SHORT_TERM_RPS - Max 64 set, 16 entry every set, total 64x16x2 = 2048 bytes (0x800)
			.buf_size = 0x800,
		},
		.vps = {
			// VPS STORE AREA - Max 16 VPS, each has 0x80 bytes, total 0x0800 bytes
			.buf_size = 0x800,
		},
		.sps = {
			// SPS STORE AREA - Max 16 SPS, each has 0x80 bytes, total 0x0800 bytes
			.buf_size = 0x800,
		},
		.pps = {
			// PPS STORE AREA - Max 64 PPS, each has 0x80 bytes, total 0x2000 bytes
			.buf_size = 0x2000,
		},
		.sao_up = {
			// SAO UP STORE AREA - Max 640(10240/16) LCU, each has 16 bytes total 0x2800 bytes
			.buf_size = 0x2800*2,
		},
		.swap_buf = {
			// 256cyclex64bit = 2K bytes 0x800 (only 144 cycles valid)
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
			// support up to 32 SCALELUT 1024x32 = 32Kbytes (0x8000)
			.buf_size = 0x8000*2,
		},
		.dblk_para  = {.buf_size = 0x40000*2, }, // dblk parameter
		.dblk_data  = {.buf_size = 0x80000*2, }, // dblk data for left/top
		.dblk_data2 = {.buf_size = 0x80000*2, }, // dblk data for adapter
		.mmu_vbh = {
			.buf_size = 0x5000*2, //2*16*2304/4, 4K
		},
		.mpred_above = {
			.buf_size = 0x8000*2,
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
			.buf_size = MPRED_8K_MV_BUF_SIZE * MAX_REF_PIC_NUM, //4k2k , 0x120000 per buffer
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		},
	},
	{
		/* 8M bytes */
		.max_width = 1920,
		.max_height = 1088,
		.ipp = {/*checked*/
			/* IPP work space calculation :
			 *   4096 * (Y+CbCr+Flags) = 12k, round to 16k
			 */
			.buf_size = 0x1e00,
		},
		.sao_abv = {
			.buf_size = 0, //0x30000,
		},
		.sao_vb = {
			.buf_size = 0, //0x30000,
		},
		.short_term_rps = {/*checked*/
			/* SHORT_TERM_RPS - Max 64 set, 16 entry every set,
			 *   total 64x16x2 = 2048 bytes (0x800)
			 */
			.buf_size = 0x800,
		},
		.vps = {/*checked*/
			/* VPS STORE AREA - Max 16 VPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.sps = {/*checked*/
			/* SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.pps = {/*checked*/
			/* PPS STORE AREA - Max 64 PPS, each has 0x80 bytes,
			 *   total 0x2000 bytes
			 */
			.buf_size = 0x2000,
		},
		.sao_up = {
			/* SAO UP STORE AREA - Max 640(10240/16) LCU,
			 *   each has 16 bytes total 0x2800 bytes
			 */
			.buf_size = 0, //0x2800,
		},
		.swap_buf = {/*checked*/
			/* 256cyclex64bit = 2K bytes 0x800
			 *   (only 144 cycles valid)
			 */
			.buf_size = 0x800,
		},
		.swap_buf2 = {/*checked*/
			.buf_size = 0x800,
		},
		.scalelut = {/*checked*/
			/* support up to 32 SCALELUT 1024x32 =
			 *   32Kbytes (0x8000)
			 */
			.buf_size = 0x8000,
		},
		.dblk_para  = {.buf_size = 0x14500, }, // dblk parameter
		.dblk_data  = {.buf_size = 0x62800, }, // dblk data for left/top
		.dblk_data2 = {.buf_size = 0x22800, }, // dblk data for adapter
		.mmu_vbh = {/*checked*/
			.buf_size = VBH_BUF_SIZE_1080P, /*2*16*2304/4, 4K*/
		},
		.mpred_above = {/*checked*/
			.buf_size = 0x1e00,
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {/*checked*//* 1080p, 0x40000 per buffer */
			.buf_size = MPRED_MV_BUF_SIZE * MAX_REF_PIC_NUM,
		},
#endif
		.rpm = {/*checked*/
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {/*checked*/
			.buf_size = 0x500 * 2,
		}
	},
	{
		.max_width = 4096,
		.max_height = 2048,
		.ipp = {
			/* IPP work space calculation :
			 *   4096 * (Y+CbCr+Flags) = 12k, round to 16k
			 */
			.buf_size = 0x4000,
		},
		.sao_abv = {
			.buf_size = 0, //0x30000,
		},
		.sao_vb = {
			.buf_size = 0, //0x30000,
		},
		.short_term_rps = {
			/* SHORT_TERM_RPS - Max 64 set, 16 entry every set,
			 *   total 64x16x2 = 2048 bytes (0x800)
			 */
			.buf_size = 0x800,
		},
		.vps = {
			/* VPS STORE AREA - Max 16 VPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.sps = {
			/* SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			 *   total 0x0800 bytes
			 */
			.buf_size = 0x800,
		},
		.pps = {
			/* PPS STORE AREA - Max 64 PPS, each has 0x80 bytes,
			 *   total 0x2000 bytes
			 */
			.buf_size = 0x2000,
		},
		.sao_up = {
			/* SAO UP STORE AREA - Max 640(10240/16) LCU,
			 *   each has 16 bytes total 0x2800 bytes
			 */
			.buf_size = 0, //0x2800,
		},
		.swap_buf = {
			/* 256cyclex64bit = 2K bytes 0x800
			 *   (only 144 cycles valid)
			 */
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
			/* support up to 32 SCALELUT 1024x32 = 32Kbytes
			 *   (0x8000)
			 */
			.buf_size = 0x8000,
		},
		.dblk_para  = {.buf_size = 0x19100, }, // dblk parameter
		.dblk_data  = {.buf_size = 0x88800, }, // dblk data for left/top
		.dblk_data2 = {.buf_size = 0x48800, }, // dblk data for adapter
		.mmu_vbh = {
			.buf_size = VBH_BUF_SIZE_4K, /*2*16*2304/4, 4K*/
		},
		.mpred_above = {
			.buf_size = 0x4000,
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
			/* 4096x2304 , 0x120000 per buffer */
			.buf_size = MPRED_4K_MV_BUF_SIZE * MAX_REF_PIC_NUM,
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		}
	},

	{
		.max_width = 4096*2,
		.max_height = 2048*2,
		.ipp = {
			// IPP work space calculation : 4096 * (Y+CbCr+Flags) = 12k, round to 16k
			.buf_size = 0x4000*2,
		},
		.sao_abv = {
			.buf_size = 0, //0x30000*2,
		},
		.sao_vb = {
			.buf_size = 0, //0x30000*2,
		},
		.short_term_rps = {
			// SHORT_TERM_RPS - Max 64 set, 16 entry every set, total 64x16x2 = 2048 bytes (0x800)
			.buf_size = 0x800,
		},
		.vps = {
			// VPS STORE AREA - Max 16 VPS, each has 0x80 bytes, total 0x0800 bytes
			.buf_size = 0x800,
		},
		.sps = {
			// SPS STORE AREA - Max 16 SPS, each has 0x80 bytes, total 0x0800 bytes
			.buf_size = 0x800,
		},
		.pps = {
			// PPS STORE AREA - Max 64 PPS, each has 0x80 bytes, total 0x2000 bytes
			.buf_size = 0x2000,
		},
		.sao_up = {
			// SAO UP STORE AREA - Max 640(10240/16) LCU, each has 16 bytes total 0x2800 bytes
			.buf_size = 0, //0x2800*2,
		},
		.swap_buf = {
			// 256cyclex64bit = 2K bytes 0x800 (only 144 cycles valid)
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
			// support up to 32 SCALELUT 1024x32 = 32Kbytes (0x8000)
			.buf_size = 0x8000, //0x8000*2,
		},
		.dblk_para  = {.buf_size = 0x32100, }, // dblk parameter
		.dblk_data  = {.buf_size = 0x110800, }, // dblk data for left/top
		.dblk_data2 = {.buf_size = 0x90800, }, // dblk data for adapter
		.mmu_vbh = {
			.buf_size = VBH_BUF_SIZE_8K, //2*16*2304/4, 4K
		},
		.mpred_above = {
			.buf_size = 0x8000,
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
			.buf_size = MPRED_8K_MV_BUF_SIZE * MAX_REF_PIC_NUM, //4k2k , 0x120000 per buffer
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		},
	}
};

static void init_buff_spec(struct hevc_state_s *hevc,
	struct BuffInfo_s *buf_spec)
{
	buf_spec->ipp.buf_start =
		WORKBUF_ALIGN(buf_spec->start_adr);
	buf_spec->sao_abv.buf_start =
		WORKBUF_ALIGN(buf_spec->ipp.buf_start + buf_spec->ipp.buf_size);
	buf_spec->sao_vb.buf_start =
		WORKBUF_ALIGN(buf_spec->sao_abv.buf_start + buf_spec->sao_abv.buf_size);
	buf_spec->short_term_rps.buf_start =
		WORKBUF_ALIGN(buf_spec->sao_vb.buf_start + buf_spec->sao_vb.buf_size);
	buf_spec->vps.buf_start =
		WORKBUF_ALIGN(buf_spec->short_term_rps.buf_start + buf_spec->short_term_rps.buf_size);
	buf_spec->sps.buf_start =
		WORKBUF_ALIGN(buf_spec->vps.buf_start + buf_spec->vps.buf_size);
	buf_spec->pps.buf_start =
		WORKBUF_ALIGN(buf_spec->sps.buf_start + buf_spec->sps.buf_size);
	buf_spec->sao_up.buf_start =
		WORKBUF_ALIGN(buf_spec->pps.buf_start + buf_spec->pps.buf_size);
	buf_spec->swap_buf.buf_start =
		WORKBUF_ALIGN(buf_spec->sao_up.buf_start + buf_spec->sao_up.buf_size);
	buf_spec->swap_buf2.buf_start =
		WORKBUF_ALIGN(buf_spec->swap_buf.buf_start + buf_spec->swap_buf.buf_size);
	buf_spec->scalelut.buf_start =
		WORKBUF_ALIGN(buf_spec->swap_buf2.buf_start + buf_spec->swap_buf2.buf_size);
	buf_spec->dblk_para.buf_start =
		WORKBUF_ALIGN(buf_spec->scalelut.buf_start + buf_spec->scalelut.buf_size);
	buf_spec->dblk_data.buf_start =
		WORKBUF_ALIGN(buf_spec->dblk_para.buf_start + buf_spec->dblk_para.buf_size);
	buf_spec->dblk_data2.buf_start =
		WORKBUF_ALIGN(buf_spec->dblk_data.buf_start + buf_spec->dblk_data.buf_size);
	buf_spec->mmu_vbh.buf_start  =
		WORKBUF_ALIGN(buf_spec->dblk_data2.buf_start + buf_spec->dblk_data2.buf_size);
	buf_spec->mpred_above.buf_start =
		WORKBUF_ALIGN(buf_spec->mmu_vbh.buf_start + buf_spec->mmu_vbh.buf_size);
#ifdef MV_USE_FIXED_BUF
	buf_spec->mpred_mv.buf_start =
		WORKBUF_ALIGN(buf_spec->mpred_above.buf_start + buf_spec->mpred_above.buf_size);
	buf_spec->rpm.buf_start =
		WORKBUF_ALIGN(buf_spec->mpred_mv.buf_start + buf_spec->mpred_mv.buf_size);
#else
	buf_spec->rpm.buf_start =
		WORKBUF_ALIGN(buf_spec->mpred_above.buf_start + buf_spec->mpred_above.buf_size);
#endif
	buf_spec->lmem.buf_start =
		WORKBUF_ALIGN(buf_spec->rpm.buf_start + buf_spec->rpm.buf_size);
	buf_spec->end_adr =
		WORKBUF_ALIGN(buf_spec->lmem.buf_start + buf_spec->lmem.buf_size);

	if (hevc && get_dbg_flag2(hevc)) {
		hevc_print(hevc, 0,
				"%s workspace (%x %x) size = %x\n", __func__,
			   buf_spec->start_adr, buf_spec->end_adr,
			   buf_spec->end_adr - buf_spec->start_adr);

		hevc_print(hevc, 0,
			"ipp.buf_start             :%x\n",
			buf_spec->ipp.buf_start);
		hevc_print(hevc, 0,
			"sao_abv.buf_start          :%x\n",
			buf_spec->sao_abv.buf_start);
		hevc_print(hevc, 0,
			"sao_vb.buf_start          :%x\n",
			buf_spec->sao_vb.buf_start);
		hevc_print(hevc, 0,
			"short_term_rps.buf_start  :%x\n",
			buf_spec->short_term_rps.buf_start);
		hevc_print(hevc, 0,
			"vps.buf_start             :%x\n",
			buf_spec->vps.buf_start);
		hevc_print(hevc, 0,
			"sps.buf_start             :%x\n",
			buf_spec->sps.buf_start);
		hevc_print(hevc, 0,
			"pps.buf_start             :%x\n",
			buf_spec->pps.buf_start);
		hevc_print(hevc, 0,
			"sao_up.buf_start          :%x\n",
			buf_spec->sao_up.buf_start);
		hevc_print(hevc, 0,
			"swap_buf.buf_start        :%x\n",
			buf_spec->swap_buf.buf_start);
		hevc_print(hevc, 0,
			"swap_buf2.buf_start       :%x\n",
			buf_spec->swap_buf2.buf_start);
		hevc_print(hevc, 0,
			"scalelut.buf_start        :%x\n",
			buf_spec->scalelut.buf_start);
		hevc_print(hevc, 0,
			"dblk_para.buf_start       :%x\n",
			buf_spec->dblk_para.buf_start);
		hevc_print(hevc, 0,
			"dblk_data.buf_start       :%x\n",
			buf_spec->dblk_data.buf_start);
		hevc_print(hevc, 0,
			"dblk_data2.buf_start       :%x\n",
			buf_spec->dblk_data2.buf_start);
		hevc_print(hevc, 0,
			"mpred_above.buf_start     :%x\n",
			buf_spec->mpred_above.buf_start);
#ifdef MV_USE_FIXED_BUF
		hevc_print(hevc, 0,
			"mpred_mv.buf_start        :%x\n",
			  buf_spec->mpred_mv.buf_start);
#endif
		if ((get_dbg_flag2(hevc)
			&
			H265_DEBUG_SEND_PARAM_WITH_REG)
			== 0) {
			hevc_print(hevc, 0,
				"rpm.buf_start             :%x\n",
				   buf_spec->rpm.buf_start);
		}
	}
}

enum SliceType {
	B_SLICE,
	P_SLICE,
	I_SLICE
};

/*USE_BUF_BLOCK*/
struct BUF_s {
	ulong	start_adr;
	u32	size;
	u32	luma_size;
	ulong	header_addr;
	u32 	header_size;
	int	used_flag;
	ulong	v4l_ref_buf_addr;
	ulong	chroma_addr;
	u32	chroma_size;
} /*BUF_t */;

/* level 6, 6.1 maximum slice number is 800; other is 200 */
#define MAX_SLICE_NUM 800
struct PIC_s {
	int index;
	int scatter_alloc;
	int BUF_index;
	int mv_buf_index;
	int POC;
	int decode_idx;
	int slice_type;
	int RefNum_L0;
	int RefNum_L1;
	int num_reorder_pic;
	int stream_offset;
	unsigned char referenced;
	unsigned char output_mark;
	unsigned char recon_mark;
	unsigned char output_ready;
	unsigned char error_mark;
	//dis_mark = 0:discard mark,dis_mark = 1:no discard mark
	unsigned char dis_mark;
	/**/ int slice_idx;
	int m_aiRefPOCList0[MAX_SLICE_NUM][16];
	int m_aiRefPOCList1[MAX_SLICE_NUM][16];
#ifdef SUPPORT_LONG_TERM_RPS
	unsigned char long_term_ref;
	unsigned char m_aiRefLTflgList0[MAX_SLICE_NUM][16];
	unsigned char m_aiRefLTflgList1[MAX_SLICE_NUM][16];
#endif
	/*buffer */
	unsigned int header_adr;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	unsigned char dv_enhance_exist;
#endif
	char *aux_data_buf;
	int aux_data_size;
	unsigned long cma_alloc_addr;
	struct page *alloc_pages;
	unsigned int mpred_mv_wr_start_addr;
	int mv_size;
	unsigned int mc_y_adr;
	unsigned int mc_u_v_adr;
#ifdef SUPPORT_10BIT
	/*unsigned int comp_body_size;*/
	unsigned int dw_y_adr;
	unsigned int dw_u_v_adr;
#endif
	u32	luma_size;
	u32	chroma_size;

	int mc_canvas_y;
	int mc_canvas_u_v;
	int width;
	int height;

	int y_canvas_index;
	int uv_canvas_index;
#ifdef MULTI_INSTANCE_SUPPORT
	struct canvas_config_s canvas_config[2];
#endif
#ifdef SUPPORT_10BIT
	int mem_saving_mode;
	u32 bit_depth_luma;
	u32 bit_depth_chroma;
#endif
#ifdef LOSLESS_COMPRESS_MODE
	unsigned int losless_comp_body_size;
#endif
	unsigned char pic_struct;
	int vf_ref;

	u32 pts;
	u64 pts64;
	u64 timestamp;

	u32 aspect_ratio_idc;
	u32 sar_width;
	u32 sar_height;
	u32 double_write_mode;
	u32 video_signal_type;
	unsigned short conformance_window_flag;
	unsigned short conf_win_left_offset;
	unsigned short conf_win_right_offset;
	unsigned short conf_win_top_offset;
	unsigned short conf_win_bottom_offset;
	unsigned short chroma_format_idc;

	/* picture qos infomation*/
	int max_qp;
	int avg_qp;
	int min_qp;
	int max_skip;
	int avg_skip;
	int min_skip;
	int max_mv;
	int min_mv;
	int avg_mv;

	u32 hw_decode_time;
	u32 frame_size; // For frame base mode
	bool ip_mode;
	u32 hdr10p_data_size;
	char *hdr10p_data_buf;
	struct dma_fence *fence;
	bool show_frame;
	int  ctx_buf_idx;
} /*PIC_t */;

#define MAX_TILE_COL_NUM    10
#define MAX_TILE_ROW_NUM   20
struct tile_s {
	int width;
	int height;
	int start_cu_x;
	int start_cu_y;

	unsigned int sao_vb_start_addr;
	unsigned int sao_abv_start_addr;
};

#define SEI_MASTER_DISPLAY_COLOR_MASK 0x00000001
#define SEI_CONTENT_LIGHT_LEVEL_MASK  0x00000002
#define SEI_HDR10PLUS_MASK			  0x00000004
#define SEI_HDR_CUVA_MASK	      0x00000008

#define VF_POOL_SIZE        32

#define pcm_enabled_flag                                  0x040
#define pcm_sample_bit_depth_luma                         0x041
#define pcm_sample_bit_depth_chroma                       0x042

#define HEVC_LOG_BUF		196

#define HEVC_PRINT_BUF		512

#ifdef MULTI_INSTANCE_SUPPORT
#define DEC_RESULT_NONE             0
#define DEC_RESULT_DONE             1
#define DEC_RESULT_AGAIN            2
#define DEC_RESULT_CONFIG_PARAM     3
#define DEC_RESULT_ERROR            4
#define DEC_INIT_PICLIST			5
#define DEC_UNINIT_PICLIST			6
#define DEC_RESULT_GET_DATA         7
#define DEC_RESULT_GET_DATA_RETRY   8
#define DEC_RESULT_EOS              9
#define DEC_RESULT_FORCE_EXIT       10
#define DEC_RESULT_FREE_CANVAS      11
#define DEC_RESULT_ERROR_DATA      	12

static void vh265_work(struct work_struct *work);
static void vh265_timeout_work(struct work_struct *work);
#endif

static void put_mv_buf(struct hevc_state_s *hevc,
	struct PIC_s *pic);

static void set_canvas(struct hevc_state_s *hevc, struct PIC_s *pic);

static void release_aux_data(struct hevc_state_s *hevc,
	struct PIC_s *pic);
static void release_pic_mmu_buf(struct hevc_state_s *hevc, struct PIC_s *pic);

static int post_picture_early(struct vdec_s *vdec, int index);

static int prepare_display_buf(struct vdec_s *vdec, struct PIC_s *pic);

static int H265_alloc_mmu(struct hevc_state_s *hevc,
			struct PIC_s *new_pic,	unsigned short bit_depth,
			unsigned int *mmu_index_adr);

static int init_buf_spec(struct hevc_state_s *hevc);

#define H265_USERDATA_ENABLE

#ifdef H265_USERDATA_ENABLE

#define SEI_ITU_DATA_SIZE		(4*1024)

static u32 itu_t_t35_enable = 1;

struct h265_userdata_record_t {
	struct userdata_meta_info_t meta_info;
	u32 rec_start;
	u32 rec_len;
};
/*
struct h265_ud_record_wait_node_t {
	struct list_head list;
	struct mh264_userdata_record_t ud_record;
};*/
#define USERDATA_FIFO_NUM    256
#define MAX_FREE_USERDATA_NODES		5

struct h265_userdata_info_t {
	struct h265_userdata_record_t records[USERDATA_FIFO_NUM];
	u8 *data_buf;
	u8 *data_buf_end;
	u32 buf_len;
	u32 read_index;
	u32 write_index;
	u32 last_wp;
};
#endif

struct debug_log_s {
	struct list_head list;
	uint8_t data; /*will alloc more size*/
};

struct mh265_fence_vf_t {
	u32 used_size;
	struct vframe_s *fence_vf[VF_POOL_SIZE];
};

struct hevc_state_s {
#ifdef MULTI_INSTANCE_SUPPORT
	struct platform_device *platform_dev;
	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;
	struct vframe_chunk_s *chunk;
	int dec_result;
	u32 timeout_processing;
	struct work_struct work;
	struct work_struct timeout_work;
	struct work_struct notify_work;
	struct work_struct set_clk_work;
	/* timeout handle */
	unsigned long int start_process_time;
	unsigned int last_lcu_idx;
	unsigned int decode_timeout_count;
	unsigned int timeout_num;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	unsigned char switch_dvlayer_flag;
	unsigned char no_switch_dvlayer_count;
	unsigned char bypass_dvenl_enable;
	unsigned char bypass_dvenl;
#endif
	unsigned char start_parser_type;
	/*start_decoding_flag:
	vps/pps/sps/idr info from ucode*/
	unsigned char start_decoding_flag;
	unsigned char rps_set_id;
	unsigned char eos;
	int pic_decoded_lcu_idx;
	u8 over_decode;
	u8 empty_flag;
#endif
	struct vframe_s vframe_dummy;
	char *provider_name;
	int index;
	struct device *cma_dev;
	unsigned char m_ins_flag;
	unsigned char dolby_enhance_flag;
	unsigned long buf_start;
	u32 buf_size;
	u32 mv_buf_size;

	struct BuffInfo_s work_space_buf_store;
	struct BuffInfo_s *work_space_buf;

	u8 aux_data_dirty;
	u32 prefix_aux_size;
	u32 suffix_aux_size;
	void *aux_addr;
	void *rpm_addr;
	void *lmem_addr;
	dma_addr_t aux_phy_addr;
	dma_addr_t rpm_phy_addr;
	dma_addr_t lmem_phy_addr;

	unsigned int pic_list_init_flag;
	unsigned int use_cma_flag;

	unsigned short *rpm_ptr;
	unsigned short *lmem_ptr;
	unsigned short *debug_ptr;
	int debug_ptr_size;
	int pic_w;
	int pic_h;
	int lcu_x_num;
	int lcu_y_num;
	int lcu_total;
	int lcu_size;
	int lcu_size_log2;
	int lcu_x_num_pre;
	int lcu_y_num_pre;
	int first_pic_after_recover;

	int num_tile_col;
	int num_tile_row;
	int tile_enabled;
	int tile_x;
	int tile_y;
	int tile_y_x;
	int tile_start_lcu_x;
	int tile_start_lcu_y;
	int tile_width_lcu;
	int tile_height_lcu;

	int slice_type;
	unsigned int slice_addr;
	unsigned int slice_segment_addr;

	unsigned char interlace_flag;
	unsigned char curr_pic_struct;
	unsigned char frame_field_info_present_flag;

	unsigned short sps_num_reorder_pics_0;
	unsigned short misc_flag0;
	int m_temporalId;
	int m_nalUnitType;
	int TMVPFlag;
	int isNextSliceSegment;
	int LDCFlag;
	int m_pocRandomAccess;
	int plevel;
	int MaxNumMergeCand;

	int new_pic;
	int new_tile;
	int curr_POC;
	int iPrevPOC;
#ifdef MULTI_INSTANCE_SUPPORT
	int decoded_poc;
	struct PIC_s *decoding_pic;
#endif
	int iPrevTid0POC;
	int list_no;
	int RefNum_L0;
	int RefNum_L1;
	int ColFromL0Flag;
	int LongTerm_Curr;
	int LongTerm_Col;
	int Col_POC;
	int LongTerm_Ref;
#ifdef MULTI_INSTANCE_SUPPORT
	int m_pocRandomAccess_bak;
	int curr_POC_bak;
	int iPrevPOC_bak;
	int iPrevTid0POC_bak;
	unsigned char start_parser_type_bak;
	unsigned char start_decoding_flag_bak;
	unsigned char rps_set_id_bak;
	int pic_decoded_lcu_idx_bak;
	int decode_idx_bak;
#endif
	struct PIC_s *cur_pic;
	struct PIC_s *col_pic;
	int skip_flag;
	int decode_idx;
	int slice_idx;
	unsigned char have_vps;
	unsigned char have_sps;
	unsigned char have_pps;
	unsigned char have_valid_start_slice;
	unsigned char wait_buf;
	unsigned char error_flag;
	unsigned int error_skip_nal_count;
	long used_4k_num;

	unsigned char
	ignore_bufmgr_error;	/* bit 0, for decoding;
			bit 1, for displaying
			bit 1 must be set if bit 0 is 1*/
	int PB_skip_mode;
	int PB_skip_count_after_decoding;
#ifdef SUPPORT_10BIT
	int mem_saving_mode;
#endif
#ifdef LOSLESS_COMPRESS_MODE
	unsigned int losless_comp_body_size;
#endif
	int pts_mode;
	int last_lookup_pts;
	int last_pts;
	u64 last_lookup_pts_us64;
	u64 last_pts_us64;
	u32 shift_byte_count_lo;
	u32 shift_byte_count_hi;
	int pts_mode_switching_count;
	int pts_mode_recovery_count;

	int pic_num;

	union param_u param;

	struct tile_s m_tile[MAX_TILE_ROW_NUM][MAX_TILE_COL_NUM];

	struct timer_list timer;
	struct BUF_s m_BUF[BUF_POOL_SIZE];
	struct BUF_s m_mv_BUF[MAX_REF_PIC_NUM];
	struct PIC_s *m_PIC[MAX_REF_PIC_NUM];

	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(pending_q, struct vframe_s *, VF_POOL_SIZE);
	struct vframe_s vfpool[VF_POOL_SIZE];

	u32 stat;
	u32 frame_width;
	u32 frame_height;
	u32 frame_dur;
	u32 frame_ar;
	u32 bit_depth_luma;
	u32 bit_depth_chroma;
	u32 video_signal_type;
	u32 video_signal_type_debug;
	u32 saved_resolution;
	bool get_frame_dur;
	u32 error_watchdog_count;
	u32 error_skip_nal_wt_cnt;
	u32 error_system_watchdog_count;

#ifdef DEBUG_PTS
	unsigned long pts_missed;
	unsigned long pts_hit;
#endif
	struct dec_sysinfo vh265_amstream_dec_info;
	unsigned char init_flag;
	unsigned char first_sc_checked;
	unsigned char uninit_list;
	u32 start_decoding_time;

	int show_frame_num;
#ifdef USE_UNINIT_SEMA
	struct semaphore h265_uninit_done_sema;
#endif
	int fatal_error;

	u32 sei_present_flag;
	void *frame_mmu_map_addr;
	dma_addr_t frame_mmu_map_phy_addr;
	unsigned int mmu_mc_buf_start;
	unsigned int mmu_mc_buf_end;
	unsigned int mmu_mc_start_4k_adr;
	void *mmu_box;
	void *bmmu_box;
	int mmu_enable;

	unsigned int dec_status;

	/* data for SEI_MASTER_DISPLAY_COLOR */
	unsigned int primaries[3][2];
	unsigned int white_point[2];
	unsigned int luminance[2];
	/* data for SEI_CONTENT_LIGHT_LEVEL */
	unsigned int content_light_level[2];

	struct PIC_s *pre_top_pic;
	struct PIC_s *pre_bot_pic;

#ifdef MULTI_INSTANCE_SUPPORT
	int double_write_mode;
	int dynamic_buf_num_margin;
	int start_action;
	int save_buffer_mode;
#endif
	u32 i_only;
	struct list_head log_list;
	u32 ucode_pause_pos;
	u32 start_shift_bytes;

	atomic_t vf_pre_count;
	atomic_t vf_get_count;
	atomic_t vf_put_count;
#ifdef SWAP_HEVC_UCODE
	dma_addr_t mc_dma_handle;
	void *mc_cpu_addr;
	int swap_size;
	ulong swap_addr;
#endif
#ifdef DETREFILL_ENABLE
	dma_addr_t detbuf_adr;
	u16 *detbuf_adr_virt;
	u8 delrefill_check;
#endif
	u8 head_error_flag;
	int valve_count;
	struct firmware_s *fw;
	int max_pic_w;
	int max_pic_h;
#ifdef AGAIN_HAS_THRESHOLD
	u8 next_again_flag;
	u32 pre_parser_wr_ptr;
#endif
	u32 ratio_control;
	u32 first_pic_flag;
	u32 decode_size;
	struct mutex chunks_mutex;
	int need_cache_size;
	u64 sc_start_time;
	u32 skip_nal_count;
	bool is_swap;
	bool is_4k;

	int frameinfo_enable;
	struct vframe_qos_s vframe_qos;
	bool is_used_v4l;
	void *v4l2_ctx;
	bool v4l_params_parsed;
	u32 mem_map_mode;
	u32 performance_profile;
	struct vdec_info *gvs;
	bool ip_mode;
	u32 kpi_first_i_comming;
	u32 kpi_first_i_decoded;
	int sidebind_type;
	int sidebind_channel_id;
	u32 again_count;
	u64 again_timeout_jiffies;
	u32 pre_parser_video_rp;
	u32 pre_parser_video_wp;
	bool dv_duallayer;
	u32 poc_error_count;
	u32 timeout_flag;
	ulong timeout;
	bool discard_dv_data;
	bool enable_fence;
	int fence_usage;
	int buffer_wrap[MAX_REF_PIC_NUM];
	int low_latency_flag;
	u32 metadata_config_flag;
	int last_width;
	int last_height;
	int used_buf_num;
	u32 dirty_shift_flag;
	u32 endian;
	ulong fb_token;
	int send_frame_flag;
	struct vdec_v4l2_buffer  *pair_fb[2];
	struct mh265_fence_vf_t fence_vf_s;
	struct mutex fence_mutex;
	bool resolution_change;
	dma_addr_t rdma_phy_adr;
	unsigned *rdma_adr;
	bool no_need_aux_data;
	struct trace_decoder_name trace;
	u32 nal_skip_policy;
	bool high_bandwidth_flag;
	bool enable_ucode_swap;
	ulong aux_mem_handle;
	ulong rpm_mem_handle;
	ulong lmem_phy_handle;
	ulong frame_mmu_map_handle;
	ulong mc_cpu_handle;
	ulong det_buf_handle;
	ulong rdma_mem_handle;
#ifdef H265_USERDATA_ENABLE
	/*user data*/
	struct mutex userdata_mutex;
	struct h265_userdata_info_t userdata_info;
	struct h265_userdata_record_t ud_record;
	int wait_for_udr_send;

	/* buffer for storing one itu35 recored */
	void *sei_itu_data_buf;
	u32 sei_itu_data_len;
	/* recycle buffer for user data storing all itu35 records */
	void *sei_user_data_buffer;
	u32 sei_user_data_wp;
	struct work_struct user_data_ready_work;
#endif
} /*hevc_stru_t */;

#ifdef AGAIN_HAS_THRESHOLD
static u32 again_threshold;
#endif
#ifdef SEND_LMEM_WITH_RPM
#define get_lmem_params(hevc, ladr) \
	hevc->lmem_ptr[ladr - (ladr & 0x3) + 3 - (ladr & 0x3)]

static int get_frame_mmu_map_size(void)
{
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
		return (MAX_FRAME_8K_NUM * 4);

	return (MAX_FRAME_4K_NUM * 4);
}

static int is_oversize(int w, int h)
{
	int max = (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)?
		MAX_SIZE_8K : MAX_SIZE_4K;

	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D)
		max = MAX_SIZE_2K;

	if (w < 0 || h < 0)
		return true;

	if (h != 0 && (w > max / h))
		return true;

	return false;
}

int is_oversize_ex(int w, int h)
{
	int max = (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) ?
		MAX_SIZE_8K : MAX_SIZE_4K;

	if (w == 0 || h == 0)
		return true;
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) {
		if (w > 8192 || h > 4608)
			return true;
	} else {
		if (w > 4096 || h > 2304)
			return true;
	}

	if (w < 0 || h < 0)
		return true;

	if (h != 0 && (w > max / h))
		return true;

	return false;
}

void check_head_error(struct hevc_state_s *hevc)
{
	hevc->head_error_flag = 0;
	if ((error_handle_policy & 0x40) == 0)
		return;
	if (get_lmem_params(hevc, pcm_enabled_flag)) {
		uint16_t pcm_depth_luma = get_lmem_params(
			hevc, pcm_sample_bit_depth_luma);
		uint16_t pcm_sample_chroma = get_lmem_params(
			hevc, pcm_sample_bit_depth_chroma);
		if (pcm_depth_luma >
			hevc->bit_depth_luma ||
			pcm_sample_chroma >
			hevc->bit_depth_chroma) {
			hevc_print(hevc, 0,
			"error, pcm bit depth %d, %d is greater than normal bit depth %d, %d\n",
			pcm_depth_luma,
			pcm_sample_chroma,
			hevc->bit_depth_luma,
			hevc->bit_depth_chroma);
			hevc->head_error_flag = 1;
		}
	}
}
#endif

#ifdef SUPPORT_10BIT
/* Losless compression body buffer size 4K per 64x32 (jt) */
static int compute_losless_comp_body_size(struct hevc_state_s *hevc,
	int width, int height, int mem_saving_mode)
{
	int width_x64;
	int     height_x32;
	int     bsize;

	width_x64 = width + 63;
	width_x64 >>= 6;

	height_x32 = height + 31;
	height_x32 >>= 5;
	if (mem_saving_mode == 1 && hevc->mmu_enable)
		bsize = 3200 * width_x64 * height_x32;
	else if (mem_saving_mode == 1)
		bsize = 3072 * width_x64 * height_x32;
	else
		bsize = 4096 * width_x64 * height_x32;

	return  bsize;
}

/* Losless compression header buffer size 32bytes per 128x64 (jt) */
static int compute_losless_comp_header_size(int width, int height)
{
	int     width_x128;
	int     height_x64;
	int     hsize;

	width_x128 = width + 127;
	width_x128 >>= 7;

	height_x64 = height + 63;
	height_x64 >>= 6;

	hsize = 32*width_x128*height_x64;

	return  hsize;
}
#endif

static int add_log(struct hevc_state_s *hevc,
	const char *fmt, ...)
{
	struct debug_log_s *log_item;
	unsigned char buf[HEVC_LOG_BUF];
	int len = 0;
	va_list args;
	mutex_lock(&vh265_log_mutex);
	va_start(args, fmt);
	len = sprintf(buf, "<%ld>   <%05d> ",
		jiffies, hevc->decode_idx);
	len += vsnprintf(buf + len,
		HEVC_LOG_BUF - len, fmt, args);
	va_end(args);
	log_item = kmalloc(
		sizeof(struct debug_log_s) + len,
		GFP_KERNEL);
	if (log_item) {
		INIT_LIST_HEAD(&log_item->list);
		strcpy(&log_item->data, buf);
		list_add_tail(&log_item->list,
			&hevc->log_list);
	}
	mutex_unlock(&vh265_log_mutex);
	return 0;
}

static void dump_log(struct hevc_state_s *hevc)
{
	int i = 0;
	struct debug_log_s *log_item, *tmp;
	mutex_lock(&vh265_log_mutex);
	list_for_each_entry_safe(log_item, tmp, &hevc->log_list, list) {
		hevc_print(hevc, 0,
			"[LOG%04d]%s\n",
			i++,
			&log_item->data);
		list_del(&log_item->list);
		kfree(log_item);
	}
	mutex_unlock(&vh265_log_mutex);
}

static unsigned char is_skip_decoding(struct hevc_state_s *hevc,
		struct PIC_s *pic)
{
	if (pic->error_mark
		&& ((hevc->ignore_bufmgr_error & 0x1) == 0))
		return 1;
	return 0;
}

static int get_pic_poc(struct hevc_state_s *hevc,
		unsigned int idx)
{
	if (idx != 0xff
		&& idx < MAX_REF_PIC_NUM
		&& hevc->m_PIC[idx])
		return hevc->m_PIC[idx]->POC;
	return INVALID_POC;
}

static int get_double_write_mode(struct hevc_state_s *hevc)
{
	u32 dw = 0x1; /*1:1*/
	unsigned int out;

	vdec_v4l_get_dw_mode(hevc->v4l2_ctx, &out);
	dw = out;
	return dw;
}

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static int get_valid_double_write_mode(struct hevc_state_s *hevc)
{
	return (hevc->m_ins_flag &&
		((double_write_mode & 0x80000000) == 0)) ?
		get_double_write_mode(hevc) :
		(double_write_mode & 0x7fffffff);
}

static int get_dynamic_buf_num_margin(struct hevc_state_s *hevc)
{
	return (hevc->m_ins_flag &&
		((dynamic_buf_num_margin & 0x80000000) == 0)) ?
		hevc->dynamic_buf_num_margin :
		(dynamic_buf_num_margin & 0x7fffffff);
}
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
static unsigned char get_idx(struct hevc_state_s *hevc)
{
	return hevc->index;
}
#endif

#undef pr_info
#define pr_info pr_cont
static int hevc_print(struct hevc_state_s *hevc,
	int flag, const char *fmt, ...)
{
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (hevc == NULL ||
		(flag == 0) ||
		((debug_mask &
		(1 << hevc->index))
		&& (debug & flag))) {
#endif
		va_list args;

		va_start(args, fmt);
		if (hevc)
			len = sprintf(buf, "[%d]", hevc->index);
		vsnprintf(buf + len, HEVC_PRINT_BUF - len, fmt, args);
		if (flag == 0)
			pr_debug("%s", buf);
		else
			pr_info("%s", buf);
		va_end(args);
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	}
#endif
	return 0;
}

static int hevc_print_cont(struct hevc_state_s *hevc,
	int flag, const char *fmt, ...)
{
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	if (hevc == NULL ||
		(flag == 0) ||
		((debug_mask &
		(1 << hevc->index))
		&& (debug & flag))) {
#endif
		va_list args;

		va_start(args, fmt);
		vsnprintf(buf + len, HEVC_PRINT_BUF - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	}
#endif
	return 0;
}

#ifdef MULTI_INSTANCE_SUPPORT
static void backup_decode_state(struct hevc_state_s *hevc)
{
	hevc->m_pocRandomAccess_bak = hevc->m_pocRandomAccess;
	hevc->curr_POC_bak = hevc->curr_POC;
	hevc->iPrevPOC_bak = hevc->iPrevPOC;
	hevc->iPrevTid0POC_bak = hevc->iPrevTid0POC;
	hevc->start_parser_type_bak = hevc->start_parser_type;
	hevc->start_decoding_flag_bak = hevc->start_decoding_flag;
	hevc->rps_set_id_bak = hevc->rps_set_id;
	hevc->pic_decoded_lcu_idx_bak = hevc->pic_decoded_lcu_idx;
	hevc->decode_idx_bak = hevc->decode_idx;

}

static void restore_decode_state(struct hevc_state_s *hevc)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);
	if (!vdec_has_more_input(vdec)) {
		hevc->pic_decoded_lcu_idx =
			READ_VREG(HEVC_PARSER_LCU_START)
			& 0xffffff;
		return;
	}
	hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
		"%s: discard pic index 0x%x\n",
		__func__, hevc->decoding_pic ?
		hevc->decoding_pic->index : 0xff);
	if (hevc->decoding_pic) {
		hevc->decoding_pic->error_mark = 0;
		hevc->decoding_pic->output_ready = 0;
		hevc->decoding_pic->show_frame = false;
		hevc->decoding_pic->output_mark = 0;
		hevc->decoding_pic->referenced = 0;
		hevc->decoding_pic->POC = INVALID_POC;
		put_mv_buf(hevc, hevc->decoding_pic);
		release_aux_data(hevc, hevc->decoding_pic);
		hevc->decoding_pic = NULL;
	}

	hevc->decode_idx = hevc->decode_idx_bak;
	hevc->m_pocRandomAccess = hevc->m_pocRandomAccess_bak;
	hevc->curr_POC = hevc->curr_POC_bak;
	hevc->iPrevPOC = hevc->iPrevPOC_bak;
	hevc->iPrevTid0POC = hevc->iPrevTid0POC_bak;
	hevc->start_parser_type = hevc->start_parser_type_bak;
	hevc->start_decoding_flag = hevc->start_decoding_flag_bak;
	hevc->rps_set_id = hevc->rps_set_id_bak;
	hevc->pic_decoded_lcu_idx = hevc->pic_decoded_lcu_idx_bak;

	if (hevc->pic_list_init_flag == 1)
		hevc->pic_list_init_flag = 0;

	hevc->slice_idx = 0;
	hevc->used_4k_num = -1;
}
#endif

static void hevc_init_stru(struct hevc_state_s *hevc,
		struct BuffInfo_s *buf_spec_i)
{
	int i;
	INIT_LIST_HEAD(&hevc->log_list);
	hevc->work_space_buf = buf_spec_i;
	hevc->prefix_aux_size = 0;
	hevc->suffix_aux_size = 0;
	hevc->aux_addr = NULL;
	hevc->rpm_addr = NULL;
	hevc->lmem_addr = NULL;

	hevc->curr_POC = INVALID_POC;

	hevc->pic_list_init_flag = 0;
	hevc->use_cma_flag = 0;
	hevc->decode_idx = 0;
	hevc->slice_idx = 0;
	hevc->new_pic = 0;
	hevc->new_tile = 0;
	hevc->iPrevPOC = 0;
	hevc->list_no = 0;
	hevc->m_pocRandomAccess = MAX_INT;
	hevc->tile_enabled = 0;
	hevc->tile_x = 0;
	hevc->tile_y = 0;
	hevc->iPrevTid0POC = 0;
	hevc->slice_addr = 0;
	hevc->slice_segment_addr = 0;
	hevc->skip_flag = 0;
	hevc->misc_flag0 = 0;

	hevc->cur_pic = NULL;
	hevc->col_pic = NULL;
	hevc->wait_buf = 0;
	hevc->error_flag = 0;
	hevc->head_error_flag = 0;
	hevc->error_skip_nal_count = 0;
	hevc->have_vps = 0;
	hevc->have_sps = 0;
	hevc->have_pps = 0;
	hevc->have_valid_start_slice = 0;

	hevc->pts_mode = PTS_NORMAL;
	hevc->last_pts = 0;
	hevc->last_lookup_pts = 0;
	hevc->last_pts_us64 = 0;
	hevc->last_lookup_pts_us64 = 0;
	hevc->pts_mode_switching_count = 0;
	hevc->pts_mode_recovery_count = 0;

	hevc->PB_skip_mode = hevc->nal_skip_policy & 0x3;
	hevc->PB_skip_count_after_decoding = (hevc->nal_skip_policy >> 16) & 0xffff;
	if (hevc->PB_skip_mode == 0)
		hevc->ignore_bufmgr_error = 0x1;
	else
		hevc->ignore_bufmgr_error = 0x0;

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		if (hevc->m_PIC[i] != NULL) {
			char *addr = NULL;
			int size;
			unsigned long flags;

			spin_lock_irqsave(&h265_lock, flags);
			addr = hevc->m_PIC[i]->aux_data_buf;
			size = hevc->m_PIC[i]->aux_data_size;

			memset(hevc->m_PIC[i], 0 ,sizeof(struct PIC_s));
			hevc->m_PIC[i]->index = i;
			hevc->m_PIC[i]->aux_data_buf = addr;
			hevc->m_PIC[i]->aux_data_size = size;
			spin_unlock_irqrestore(&h265_lock, flags);
		}
	}

	hevc->pic_num = 0;
	hevc->lcu_x_num_pre = 0;
	hevc->lcu_y_num_pre = 0;
	hevc->first_pic_after_recover = 0;

	hevc->pre_top_pic = NULL;
	hevc->pre_bot_pic = NULL;

	hevc->sei_present_flag = 0;
	hevc->valve_count = 0;
	hevc->first_pic_flag = 0;
#ifdef MULTI_INSTANCE_SUPPORT
	hevc->decoded_poc = INVALID_POC;
	hevc->start_process_time = 0;
	hevc->last_lcu_idx = 0;
	hevc->decode_timeout_count = 0;
	hevc->timeout_num = 0;
	hevc->eos = 0;
	hevc->pic_decoded_lcu_idx = -1;
	hevc->over_decode = 0;
	hevc->used_4k_num = -1;
	hevc->start_decoding_flag = 0;
	hevc->rps_set_id = 0;
	backup_decode_state(hevc);
#endif
#ifdef DETREFILL_ENABLE
	hevc->detbuf_adr = 0;
	hevc->detbuf_adr_virt = NULL;
#endif
}

#ifdef DETREFILL_ENABLE
#define DETREFILL_BUF_SIZE (4 * 0x4000)
#define HEVC_SAO_DBG_MODE0                         0x361e
#define HEVC_SAO_DBG_MODE1                         0x361f
#define HEVC_SAO_CTRL10                            0x362e
#define HEVC_SAO_CTRL11                            0x362f
static int init_detrefill_buf(struct hevc_state_s *hevc)
{
	struct aml_vcodec_ctx * ctx = hevc->v4l2_ctx;

	if (hevc->detbuf_adr_virt)
		return 0;

	hevc->detbuf_adr_virt =
		(void *)decoder_dma_alloc_coherent(&hevc->det_buf_handle,
			DETREFILL_BUF_SIZE, &hevc->detbuf_adr,
			"H265_DET_BUF");

	if (hevc->detbuf_adr_virt == NULL) {
		pr_err("%s: failed to alloc ETREFILL_BUF\n", __func__);
		vdec_v4l_post_error_event(ctx, DECODER_ERROR_ALLOC_BUFFER_FAIL);
		return -1;
	}
	return 0;
}

static void uninit_detrefill_buf(struct hevc_state_s *hevc)
{
	if (hevc->detbuf_adr_virt) {
		decoder_dma_free_coherent(hevc->det_buf_handle,
			DETREFILL_BUF_SIZE, hevc->detbuf_adr_virt,
			hevc->detbuf_adr);

		hevc->detbuf_adr_virt = NULL;
		hevc->detbuf_adr = 0;
	}
}

/*
 * convert uncompressed frame buffer data from/to ddr
 */
static void convUnc8x4blk(uint16_t* blk8x4Luma,
	uint16_t* blk8x4Cb, uint16_t* blk8x4Cr, uint16_t* cmBodyBuf, int32_t direction)
{
	if (direction == 0) {
		blk8x4Luma[3 + 0 * 8] = ((cmBodyBuf[0] >> 0)) & 0x3ff;
		blk8x4Luma[3 + 1 * 8] = ((cmBodyBuf[1] << 6)
			| (cmBodyBuf[0] >> 10)) & 0x3ff;
		blk8x4Luma[3 + 2 * 8] = ((cmBodyBuf[1] >> 4)) & 0x3ff;
		blk8x4Luma[3 + 3 * 8] = ((cmBodyBuf[2] << 2)
			| (cmBodyBuf[1] >> 14)) & 0x3ff;
		blk8x4Luma[7 + 0 * 8] = ((cmBodyBuf[3] << 8)
			| (cmBodyBuf[2] >> 8)) & 0x3ff;
		blk8x4Luma[7 + 1 * 8] = ((cmBodyBuf[3] >> 2)) & 0x3ff;
		blk8x4Luma[7 + 2 * 8] = ((cmBodyBuf[4] << 4)
			| (cmBodyBuf[3] >> 12)) & 0x3ff;
		blk8x4Luma[7 + 3 * 8] = ((cmBodyBuf[4] >> 6)) & 0x3ff;
		blk8x4Cb  [0 + 0 * 4] = ((cmBodyBuf[5] >> 0)) & 0x3ff;
		blk8x4Cr  [0 + 0 * 4] = ((cmBodyBuf[6]	<< 6)
			| (cmBodyBuf[5] >> 10)) & 0x3ff;
		blk8x4Cb  [0 + 1 * 4] = ((cmBodyBuf[6] >> 4)) & 0x3ff;
		blk8x4Cr  [0 + 1 * 4] = ((cmBodyBuf[7] << 2)
			| (cmBodyBuf[6] >> 14)) & 0x3ff;

		blk8x4Luma[0 + 0 * 8] = ((cmBodyBuf[0 + 8] >> 0)) & 0x3ff;
		blk8x4Luma[1 + 0 * 8] = ((cmBodyBuf[1 + 8] << 6) |
			(cmBodyBuf[0 + 8] >> 10)) & 0x3ff;
		blk8x4Luma[2 + 0 * 8] = ((cmBodyBuf[1 + 8] >> 4)) & 0x3ff;
		blk8x4Luma[0 + 1 * 8] = ((cmBodyBuf[2 + 8] << 2) |
			(cmBodyBuf[1 + 8] >> 14)) & 0x3ff;
		blk8x4Luma[1 + 1 * 8] = ((cmBodyBuf[3 + 8] << 8) |
			(cmBodyBuf[2 + 8] >> 8)) & 0x3ff;
		blk8x4Luma[2 + 1 * 8] = ((cmBodyBuf[3 + 8] >> 2)) & 0x3ff;
		blk8x4Luma[0 + 2 * 8] = ((cmBodyBuf[4 + 8] << 4) |
			(cmBodyBuf[3 + 8] >> 12)) & 0x3ff;
		blk8x4Luma[1 + 2 * 8] = ((cmBodyBuf[4 + 8] >> 6)) & 0x3ff;
		blk8x4Luma[2 + 2 * 8] = ((cmBodyBuf[5 + 8] >> 0)) & 0x3ff;
		blk8x4Luma[0 + 3 * 8] = ((cmBodyBuf[6 + 8] << 6) |
			(cmBodyBuf[5 + 8] >> 10)) & 0x3ff;
		blk8x4Luma[1 + 3 * 8] = ((cmBodyBuf[6 + 8] >> 4)) & 0x3ff;
		blk8x4Luma[2 + 3 * 8] = ((cmBodyBuf[7 + 8] << 2) |
			(cmBodyBuf[6 + 8] >> 14)) & 0x3ff;

		blk8x4Luma[4 + 0 * 8] = ((cmBodyBuf[0 + 16] >> 0)) & 0x3ff;
		blk8x4Luma[5 + 0 * 8] = ((cmBodyBuf[1 + 16] << 6) |
			(cmBodyBuf[0 + 16] >> 10)) & 0x3ff;
		blk8x4Luma[6 + 0 * 8] = ((cmBodyBuf[1 + 16] >> 4)) & 0x3ff;
		blk8x4Luma[4 + 1 * 8] = ((cmBodyBuf[2 + 16] << 2) |
			(cmBodyBuf[1 + 16] >> 14)) & 0x3ff;
		blk8x4Luma[5 + 1 * 8] = ((cmBodyBuf[3 + 16] << 8) |
			(cmBodyBuf[2 + 16] >> 8)) & 0x3ff;
		blk8x4Luma[6 + 1 * 8] = ((cmBodyBuf[3 + 16] >> 2)) & 0x3ff;
		blk8x4Luma[4 + 2 * 8] = ((cmBodyBuf[4 + 16] << 4) |
			(cmBodyBuf[3 + 16] >> 12)) & 0x3ff;
		blk8x4Luma[5 + 2 * 8] = ((cmBodyBuf[4 + 16] >> 6)) & 0x3ff;
		blk8x4Luma[6 + 2 * 8] = ((cmBodyBuf[5 + 16] >> 0)) & 0x3ff;
		blk8x4Luma[4 + 3 * 8] = ((cmBodyBuf[6 + 16] << 6) |
			(cmBodyBuf[5 + 16] >> 10)) & 0x3ff;
		blk8x4Luma[5 + 3 * 8] = ((cmBodyBuf[6 + 16] >> 4)) & 0x3ff;
		blk8x4Luma[6 + 3 * 8] = ((cmBodyBuf[7 + 16] << 2) |
			(cmBodyBuf[6 + 16] >> 14)) & 0x3ff;

		blk8x4Cb[1 + 0 * 4] = ((cmBodyBuf[0 + 24] >> 0)) & 0x3ff;
		blk8x4Cr[1 + 0 * 4] = ((cmBodyBuf[1 + 24] << 6) |
			(cmBodyBuf[0 + 24] >> 10)) & 0x3ff;
		blk8x4Cb[2 + 0 * 4] = ((cmBodyBuf[1 + 24] >> 4)) & 0x3ff;
		blk8x4Cr[2 + 0 * 4] = ((cmBodyBuf[2 + 24] << 2) |
			(cmBodyBuf[1 + 24] >> 14)) & 0x3ff;
		blk8x4Cb[3 + 0 * 4] = ((cmBodyBuf[3 + 24] << 8) |
			(cmBodyBuf[2 + 24] >> 8)) & 0x3ff;
		blk8x4Cr[3 + 0 * 4] = ((cmBodyBuf[3 + 24] >> 2)) & 0x3ff;
		blk8x4Cb[1 + 1 * 4] = ((cmBodyBuf[4 + 24] << 4) |
			(cmBodyBuf[3 + 24] >> 12)) & 0x3ff;
		blk8x4Cr[1 + 1 * 4] = ((cmBodyBuf[4 + 24] >> 6)) & 0x3ff;
		blk8x4Cb[2 + 1 * 4] = ((cmBodyBuf[5 + 24] >> 0)) & 0x3ff;
		blk8x4Cr[2 + 1 * 4] = ((cmBodyBuf[6 + 24] << 6) |
			(cmBodyBuf[5 + 24] >> 10)) & 0x3ff;
		blk8x4Cb[3 + 1 * 4] = ((cmBodyBuf[6 + 24] >> 4)) & 0x3ff;
		blk8x4Cr[3 + 1 * 4] = ((cmBodyBuf[7 + 24] << 2) |
			(cmBodyBuf[6 + 24] >> 14)) & 0x3ff;
	} else {
		cmBodyBuf[0 + 8 * 0] = (blk8x4Luma[3 + 1 * 8] << 10) |
			blk8x4Luma[3 + 0 * 8];
		cmBodyBuf[1 + 8 * 0] = (blk8x4Luma[3 + 3 * 8] << 14) |
			(blk8x4Luma[3 + 2 * 8] << 4) | (blk8x4Luma[3 + 1 * 8] >> 6);
		cmBodyBuf[2 + 8 * 0] = (blk8x4Luma[7 + 0 * 8] << 8) |
			(blk8x4Luma[3 + 3 * 8] >> 2);
		cmBodyBuf[3 + 8 * 0] = (blk8x4Luma[7 + 2 * 8] << 12) |
			(blk8x4Luma[7 + 1 * 8] << 2) | (blk8x4Luma[7 + 0 * 8] >>8);
		cmBodyBuf[4 + 8 * 0] = (blk8x4Luma[7 + 3 * 8] << 6) |
			(blk8x4Luma[7 + 2 * 8] >>4);
		cmBodyBuf[5 + 8 * 0] = (blk8x4Cr[0 + 0 * 4] << 10) |
			blk8x4Cb[0 + 0 * 4];
		cmBodyBuf[6 + 8 * 0] = (blk8x4Cr[0 + 1 * 4] << 14) |
			(blk8x4Cb[0 + 1 * 4] << 4)   | (blk8x4Cr[0 + 0 * 4] >> 6);
		cmBodyBuf[7 + 8 * 0] = (0<< 8) | (blk8x4Cr[0 + 1 * 4] >> 2);

		cmBodyBuf[0 + 8 * 1] = (blk8x4Luma[1 + 0 * 8] << 10) |
			blk8x4Luma[0 + 0 * 8];
		cmBodyBuf[1 + 8 * 1] = (blk8x4Luma[0 + 1 * 8] << 14) |
			(blk8x4Luma[2 + 0 * 8] << 4) | (blk8x4Luma[1 + 0 * 8] >> 6);
		cmBodyBuf[2 + 8 * 1] = (blk8x4Luma[1 + 1 * 8] << 8) |
			(blk8x4Luma[0 + 1 * 8] >> 2);
		cmBodyBuf[3 + 8 * 1] = (blk8x4Luma[0 + 2 * 8] << 12) |
			(blk8x4Luma[2 + 1 * 8] << 2) | (blk8x4Luma[1 + 1 * 8] >>8);
		cmBodyBuf[4 + 8 * 1] = (blk8x4Luma[1 + 2 * 8] << 6) |
			(blk8x4Luma[0 + 2 * 8] >>4);
		cmBodyBuf[5 + 8 * 1] = (blk8x4Luma[0 + 3 * 8] << 10) |
			blk8x4Luma[2 + 2 * 8];
		cmBodyBuf[6 + 8 * 1] = (blk8x4Luma[2 + 3 * 8] << 14) |
			(blk8x4Luma[1 + 3 * 8] << 4) | (blk8x4Luma[0 + 3 * 8] >> 6);
		cmBodyBuf[7 + 8 * 1] = (0<< 8) | (blk8x4Luma[2 + 3 * 8] >> 2);

		cmBodyBuf[0 + 8 * 2] = (blk8x4Luma[5 + 0 * 8] << 10) |
			blk8x4Luma[4 + 0 * 8];
		cmBodyBuf[1 + 8 * 2] = (blk8x4Luma[4 + 1 * 8] << 14) |
			(blk8x4Luma[6 + 0 * 8] << 4) | (blk8x4Luma[5 + 0 * 8] >> 6);
		cmBodyBuf[2 + 8 * 2] = (blk8x4Luma[5 + 1 * 8] << 8) |
			(blk8x4Luma[4 + 1 * 8] >> 2);
		cmBodyBuf[3 + 8 * 2] = (blk8x4Luma[4 + 2 * 8] << 12) |
			(blk8x4Luma[6 + 1 * 8] << 2) | (blk8x4Luma[5 + 1 * 8] >>8);
		cmBodyBuf[4 + 8 * 2] = (blk8x4Luma[5 + 2 * 8] << 6) |
			(blk8x4Luma[4 + 2 * 8] >>4);
		cmBodyBuf[5 + 8 * 2] = (blk8x4Luma[4 + 3 * 8] << 10) |
			blk8x4Luma[6 + 2 * 8];
		cmBodyBuf[6 + 8 * 2] = (blk8x4Luma[6 + 3 * 8] << 14) |
			(blk8x4Luma[5 + 3 * 8] << 4) | (blk8x4Luma[4 + 3 * 8] >> 6);
		cmBodyBuf[7 + 8 * 2] = (0<< 8) | (blk8x4Luma[6 + 3 * 8] >> 2);

		cmBodyBuf[0 + 8 * 3] = (blk8x4Cr[1 + 0 * 4] << 10) |
			blk8x4Cb[1 + 0 * 4];
		cmBodyBuf[1 + 8 * 3] = (blk8x4Cr[2 + 0 * 4] << 14) |
			(blk8x4Cb[2 + 0 * 4] << 4) | (blk8x4Cr[1 + 0 * 4] >> 6);
		cmBodyBuf[2 + 8 * 3] = (blk8x4Cb[3 + 0 * 4] << 8) |
			(blk8x4Cr[2 + 0 * 4] >> 2);
		cmBodyBuf[3 + 8 * 3] = (blk8x4Cb[1 + 1 * 4] << 12) |
			(blk8x4Cr[3 + 0 * 4] << 2) | (blk8x4Cb[3 + 0 * 4] >>8);
		cmBodyBuf[4 + 8 * 3] = (blk8x4Cr[1 + 1 * 4] << 6) |
			(blk8x4Cb[1 + 1 * 4] >>4);
		cmBodyBuf[5 + 8 * 3] = (blk8x4Cr[2 + 1 * 4] << 10) |
			blk8x4Cb[2 + 1 * 4];
		cmBodyBuf[6 + 8 * 3] = (blk8x4Cr[3 + 1 * 4] << 14) |
			(blk8x4Cb[3 + 1 * 4] << 4) | (blk8x4Cr[2 + 1 * 4] >> 6);
		cmBodyBuf[7 + 8 * 3] = (0 << 8) | (blk8x4Cr[3 + 1 * 4] >> 2);
	}
}

static void corrRefillWithAmrisc (
	struct hevc_state_s *hevc,
	uint32_t  cmHeaderBaseAddr,
	uint32_t  picWidth,
	uint32_t  ctuPosition)
{
	int32_t i;
	uint16_t ctux = (ctuPosition>>16) & 0xffff;
	uint16_t ctuy = (ctuPosition>> 0) & 0xffff;
	int32_t aboveCtuAvailable = (ctuy) ? 1 : 0;

	uint16_t *cmBodyBuf = NULL;

	uint32_t pic_width_x64_pre = picWidth + 0x3f;
	uint32_t pic_width_x64 = pic_width_x64_pre >> 6;
	uint32_t stride64x64 = pic_width_x64 * 128;
	uint32_t addr_offset64x64_abv = stride64x64 *
		(aboveCtuAvailable ? ctuy - 1 : ctuy) + 128 * ctux;
	uint32_t addr_offset64x64_cur = stride64x64*ctuy + 128 * ctux;
	uint32_t cmHeaderAddrAbv = cmHeaderBaseAddr + addr_offset64x64_abv;
	uint32_t cmHeaderAddrCur = cmHeaderBaseAddr + addr_offset64x64_cur;
	unsigned int tmpData32;

	uint16_t blkBuf0Y[32];
	uint16_t blkBuf0Cb[8];
	uint16_t blkBuf0Cr[8];
	uint16_t blkBuf1Y[32];
	uint16_t blkBuf1Cb[8];
	uint16_t blkBuf1Cr[8];
	int32_t  blkBufCnt = 0;

	int32_t blkIdx;
	PR_INIT(128);

	cmBodyBuf = vzalloc(sizeof(uint16_t) * 32 * 18);
	if (!cmBodyBuf)
		return;

	WRITE_VREG(HEVC_SAO_CTRL10, cmHeaderAddrAbv);
	WRITE_VREG(HEVC_SAO_CTRL11, cmHeaderAddrCur);
	WRITE_VREG(HEVC_SAO_DBG_MODE0, hevc->detbuf_adr);
	WRITE_VREG(HEVC_SAO_DBG_MODE1, 2);

	for (i = 0; i < 32 * 18; i++)
		cmBodyBuf[i] = 0;

	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s, %d\n", __func__, __LINE__);
	do {
		tmpData32 = READ_VREG(HEVC_SAO_DBG_MODE1);
	} while (tmpData32);
	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s, %d\n", __func__, __LINE__);

	hevc_print(hevc, H265_DEBUG_DETAIL,
		"cmBodyBuf from detbuf:\n");
	for (i = 0; i < 32 * 18; i++) {
		cmBodyBuf[i] = hevc->detbuf_adr_virt[i];
		if (get_dbg_flag(hevc) &
				H265_DEBUG_DETAIL) {
			if ((i & 0xf) == 0)
				PR_INFO(hevc->index);
			PR_FILL("%02x ", cmBodyBuf[i]);
		}
	}
	PR_INFO(hevc->index);

	for (i = 0; i < 32; i++)
		blkBuf0Y[i] = 0;
	for (i = 0; i < 8; i++)
		blkBuf0Cb[i] = 0;
	for (i = 0; i < 8; i++)
		blkBuf0Cr[i] = 0;
	for (i = 0; i < 32; i++)
		blkBuf1Y[i] = 0;
	for (i = 0; i < 8; i++)
		blkBuf1Cb[i] = 0;
	for (i = 0; i < 8; i++)
		blkBuf1Cr[i] = 0;

	for (blkIdx = 0; blkIdx < 18; blkIdx++) {
		int32_t   inAboveCtu = (blkIdx<2) ? 1 : 0;
		int32_t   restoreEnable = (blkIdx>0) ? 1 : 0;
		uint16_t* blkY = (blkBufCnt==0) ? blkBuf0Y : blkBuf1Y ;
		uint16_t* blkCb = (blkBufCnt==0) ? blkBuf0Cb : blkBuf1Cb;
		uint16_t* blkCr = (blkBufCnt==0) ? blkBuf0Cr : blkBuf1Cr;
		uint16_t* cmBodyBufNow = cmBodyBuf + (blkIdx * 32);

		if (!aboveCtuAvailable && inAboveCtu)
			continue;

		/* detRefillBuf --> 8x4block*/
		convUnc8x4blk(blkY, blkCb, blkCr, cmBodyBufNow, 0);

		if (restoreEnable) {
			blkY[3 + 0 * 8] = blkY[2 + 0 * 8] + 2;
			blkY[4 + 0 * 8] = blkY[1 + 0 * 8] + 3;
			blkY[5 + 0 * 8] = blkY[0 + 0 * 8] + 1;
			blkY[6 + 0 * 8] = blkY[0 + 0 * 8] + 2;
			blkY[7 + 0 * 8] = blkY[1 + 0 * 8] + 2;
			blkY[3 + 1 * 8] = blkY[2 + 1 * 8] + 1;
			blkY[4 + 1 * 8] = blkY[1 + 1 * 8] + 2;
			blkY[5 + 1 * 8] = blkY[0 + 1 * 8] + 2;
			blkY[6 + 1 * 8] = blkY[0 + 1 * 8] + 2;
			blkY[7 + 1 * 8] = blkY[1 + 1 * 8] + 3;
			blkY[3 + 2 * 8] = blkY[2 + 2 * 8] + 3;
			blkY[4 + 2 * 8] = blkY[1 + 2 * 8] + 1;
			blkY[5 + 2 * 8] = blkY[0 + 2 * 8] + 3;
			blkY[6 + 2 * 8] = blkY[0 + 2 * 8] + 3;
			blkY[7 + 2 * 8] = blkY[1 + 2 * 8] + 3;
			blkY[3 + 3 * 8] = blkY[2 + 3 * 8] + 0;
			blkY[4 + 3 * 8] = blkY[1 + 3 * 8] + 0;
			blkY[5 + 3 * 8] = blkY[0 + 3 * 8] + 1;
			blkY[6 + 3 * 8] = blkY[0 + 3 * 8] + 2;
			blkY[7 + 3 * 8] = blkY[1 + 3 * 8] + 1;
			blkCb[1 + 0 * 4] = blkCb[0 + 0 * 4];
			blkCb[2 + 0 * 4] = blkCb[0 + 0 * 4];
			blkCb[3 + 0 * 4] = blkCb[0 + 0 * 4];
			blkCb[1 + 1 * 4] = blkCb[0 + 1 * 4];
			blkCb[2 + 1 * 4] = blkCb[0 + 1 * 4];
			blkCb[3 + 1 * 4] = blkCb[0 + 1 * 4];
			blkCr[1 + 0 * 4] = blkCr[0 + 0 * 4];
			blkCr[2 + 0 * 4] = blkCr[0 + 0 * 4];
			blkCr[3 + 0 * 4] = blkCr[0 + 0 * 4];
			blkCr[1 + 1 * 4] = blkCr[0 + 1 * 4];
			blkCr[2 + 1 * 4] = blkCr[0 + 1 * 4];
			blkCr[3 + 1 * 4] = blkCr[0 + 1 * 4];

			/*Store data back to DDR*/
			convUnc8x4blk(blkY, blkCb, blkCr, cmBodyBufNow, 1);
		}

		blkBufCnt = (blkBufCnt==1) ? 0 : blkBufCnt + 1;
	}

	hevc_print(hevc, H265_DEBUG_DETAIL,
		"cmBodyBuf to detbuf:\n");
	for (i = 0; i < 32 * 18; i++) {
		hevc->detbuf_adr_virt[i] = cmBodyBuf[i];
		if (get_dbg_flag(hevc) &
				H265_DEBUG_DETAIL) {
			if ((i & 0xf) == 0)
				PR_INFO(hevc->index);
			PR_FILL("%02x ", cmBodyBuf[i]);
		}
	}

	PR_INFO(hevc->index);

	WRITE_VREG(HEVC_SAO_DBG_MODE1, 3);
	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s, %d\n", __func__, __LINE__);
	do {
		tmpData32 = READ_VREG(HEVC_SAO_DBG_MODE1);
	} while (tmpData32);
	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s, %d\n", __func__, __LINE__);
	vfree(cmBodyBuf);
}

static void delrefill(struct hevc_state_s *hevc)
{
	/*
	 * corrRefill
	 */
	/*HEVC_SAO_DBG_MODE0: picGlobalVariable
	[31:30]error number
	[29:20]error2([9:7]tilex[6:0]ctuy)
	[19:10]error1 [9:0]error0*/
	uint32_t detResult = READ_VREG(HEVC_ASSIST_SCRATCH_3);
	uint32_t errorIdx;
	uint32_t errorNum = (detResult>>30);

	if (detResult) {
		hevc_print(hevc, H265_DEBUG_BUFMGR,
			"[corrRefillWithAmrisc] detResult=%08x\n", detResult);
		for (errorIdx = 0; errorIdx < errorNum; errorIdx++) {
			uint32_t errorPos = errorIdx * 10;
			uint32_t errorResult = (detResult >> errorPos) & 0x3ff;
			uint32_t tilex = (errorResult >> 7) - 1;
			uint16_t ctux = hevc->m_tile[0][tilex].start_cu_x
				+ hevc->m_tile[0][tilex].width - 1;
			uint16_t ctuy = (uint16_t)(errorResult & 0x7f);
			uint32_t ctuPosition = (ctux<< 16) + ctuy;
			hevc_print(hevc, H265_DEBUG_BUFMGR,
				"Idx:%d tilex:%d ctu(%d(0x%x), %d(0x%x))\n",
				errorIdx,tilex,ctux,ctux, ctuy,ctuy);
			corrRefillWithAmrisc(
				hevc,
				(uint32_t)hevc->cur_pic->header_adr,
				hevc->pic_w,
				ctuPosition);
		}

		WRITE_VREG(HEVC_ASSIST_SCRATCH_3, 0); /*clear status*/
		WRITE_VREG(HEVC_SAO_DBG_MODE0, 0);
		WRITE_VREG(HEVC_SAO_DBG_MODE1, 1);
	}
}
#endif

static void get_rpm_param(union param_u *params)
{
	int i;
	unsigned int data32;

	for (i = 0; i < 128; i++) {
		do {
			data32 = READ_VREG(RPM_CMD_REG);
			/* hevc_print(hevc, 0, "%x\n", data32); */
		} while ((data32 & 0x10000) == 0);
		params->l.data[i] = data32 & 0xffff;
		/* hevc_print(hevc, 0, "%x\n", data32); */
		WRITE_VREG(RPM_CMD_REG, 0);
	}
}

static struct PIC_s *get_pic_by_POC(struct hevc_state_s *hevc, int POC)
{
	int i;
	struct PIC_s *pic;
	struct PIC_s *ret_pic = NULL;
	if (POC == INVALID_POC)
		return NULL;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1 ||
			pic->BUF_index == -1)
			continue;
		if (pic->POC == POC) {
			if (ret_pic == NULL)
				ret_pic = pic;
			else {
				if (pic->decode_idx > ret_pic->decode_idx)
					ret_pic = pic;
			}
		}
	}
	return ret_pic;
}

static struct PIC_s *get_ref_pic_by_POC(struct hevc_state_s *hevc, int POC)
{
	int i;
	struct PIC_s *pic;
	struct PIC_s *ret_pic = NULL;

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1 ||
			pic->BUF_index == -1)
			continue;
		/*Add width and height of ref picture detection,
			resolved incorrectly referenced frame.*/
		if ((pic->POC == POC) && (pic->referenced) &&
			(hevc->pic_w == pic->width) &&
			(hevc->pic_h == pic->height)) {
			if (ret_pic == NULL)
				ret_pic = pic;
			else {
				if (pic->decode_idx > ret_pic->decode_idx)
					ret_pic = pic;
			}
		}
	}

	return ret_pic;
}

static unsigned int log2i(unsigned int val)
{
	unsigned int ret = -1;

	while (val != 0) {
		val >>= 1;
		ret++;
	}
	return ret;
}

static void uninit_mmu_buffers(struct hevc_state_s *hevc)
{
	if (hevc->mmu_box) {
		decoder_mmu_box_free(hevc->mmu_box);
		hevc->mmu_box = NULL;
	}

	if (hevc->bmmu_box) {
		/* release workspace */
		decoder_bmmu_box_free_idx(hevc->bmmu_box,
			BMMU_WORKSPACE_ID);
		decoder_bmmu_box_free(hevc->bmmu_box);
		hevc->bmmu_box = NULL;
	}
}

/* return in MB */
static int hevc_max_mmu_buf_size(int max_w, int max_h)
{
	int buf_size = 64;

	if ((max_w * max_h) > 0 &&
		(max_w * max_h) <= 1920*1088) {
		buf_size = 24;
	}
	return buf_size;
}

static int init_mmu_buffers(struct hevc_state_s *hevc, bool bmmu_flag)
{
	int tvp_flag = vdec_secure(hw_to_vdec(hevc)) ?
		CODEC_MM_FLAGS_TVP : 0;
	int buf_size = hevc_max_mmu_buf_size(hevc->max_pic_w,
			hevc->max_pic_h);

	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0, "%s max_w %d max_h %d\n",
			__func__, hevc->max_pic_w, hevc->max_pic_h);
	}

	hevc->need_cache_size = buf_size * SZ_1M;
	hevc->sc_start_time = get_jiffies_64();

	if (!bmmu_flag)
		return 0;

	hevc->bmmu_box = decoder_bmmu_box_alloc_box(DRIVER_NAME,
			hevc->index,
			BMMU_MAX_BUFFERS,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER |
			tvp_flag,
			BMMU_ALLOC_FLAGS_WAIT);
	if (!hevc->bmmu_box) {
		if (hevc->mmu_box)
			decoder_mmu_box_free(hevc->mmu_box);
		hevc->mmu_box = NULL;
		pr_err("h265 alloc mmu box failed!!\n");
		return -1;
	}
	return 0;
}

struct buf_stru_s
{
	int lcu_total;
	int mc_buffer_size_h;
	int mc_buffer_size_u_v_h;
};

#ifndef MV_USE_FIXED_BUF
static void dealloc_mv_bufs(struct hevc_state_s *hevc)
{
	int i;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		if (hevc->m_mv_BUF[i].start_adr) {
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print(hevc, 0,
				"dealloc mv buf(%d) adr 0x%p size 0x%x used_flag %d\n",
				i, hevc->m_mv_BUF[i].start_adr,
				hevc->m_mv_BUF[i].size,
				hevc->m_mv_BUF[i].used_flag);
			decoder_bmmu_box_free_idx(
				hevc->bmmu_box,
				MV_BUFFER_IDX(i));
			hevc->m_mv_BUF[i].start_adr = 0;
			hevc->m_mv_BUF[i].size = 0;
			hevc->m_mv_BUF[i].used_flag = 0;
		}
	}
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		if (hevc->m_PIC[i] != NULL)
			hevc->m_PIC[i]->mv_buf_index = -1;
	}
}

static int alloc_mv_buf(struct hevc_state_s *hevc, int i)
{
	int ret = 0;

	if (decoder_bmmu_box_alloc_buf_phy
		(hevc->bmmu_box,
		MV_BUFFER_IDX(i), hevc->mv_buf_size,
		DRIVER_NAME,
		&hevc->m_mv_BUF[i].start_adr) < 0) {
		hevc->m_mv_BUF[i].start_adr = 0;
		ret = -1;
	} else {
		hevc->m_mv_BUF[i].size = hevc->mv_buf_size;
		hevc->m_mv_BUF[i].used_flag = 0;
		ret = 0;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
			"MV Buffer %d: start_adr %p size %x\n",
			i,
			(void *)hevc->m_mv_BUF[i].start_adr,
			hevc->m_mv_BUF[i].size);
		}
		if (!vdec_secure(hw_to_vdec(hevc)) && (hevc->m_mv_BUF[i].start_adr)) {
			void *mem_start_virt;
			mem_start_virt =
					codec_mm_phys_to_virt(hevc->m_mv_BUF[i].start_adr);
			if (mem_start_virt) {
					memset(mem_start_virt, 0, hevc->m_mv_BUF[i].size);
					codec_mm_dma_flush(mem_start_virt,
							hevc->m_mv_BUF[i].size, DMA_TO_DEVICE);
			} else {
					mem_start_virt = codec_mm_vmap(
							hevc->m_mv_BUF[i].start_adr,
							hevc->m_mv_BUF[i].size);
					if (mem_start_virt) {
							memset(mem_start_virt, 0, hevc->m_mv_BUF[i].size);
							codec_mm_dma_flush(mem_start_virt,
									hevc->m_mv_BUF[i].size,
									DMA_TO_DEVICE);
							codec_mm_unmap_phyaddr(mem_start_virt);
					} else {
							/*not virt for tvp playing,
							may need clear on ucode.*/
							pr_err("ref %s	mem_start_virt failed\n", __func__);
					}
			}
		}
	}
	return ret;
}
#endif

static int get_mv_buf(struct hevc_state_s *hevc, struct PIC_s *pic)
{
#ifdef MV_USE_FIXED_BUF
	if (pic && pic->index >= 0) {
		int mv_size;
		if (IS_8K_SIZE(pic->width, pic->height))
			mv_size = MPRED_8K_MV_BUF_SIZE;
		else if (IS_4K_SIZE(pic->width, pic->height))
			mv_size = MPRED_4K_MV_BUF_SIZE; /*0x120000*/
		else
			mv_size = MPRED_MV_BUF_SIZE;

		pic->mpred_mv_wr_start_addr =
			hevc->work_space_buf->mpred_mv.buf_start
			+ (pic->index * mv_size);
		pic->mv_size = mv_size;
	}
	return 0;
#else
	int i;
	int ret = -1;
	int new_size;
	if (mv_buf_dynamic_alloc) {
		int MV_MEM_UNIT =
			hevc->lcu_size_log2 == 6 ? 0x200 : hevc->lcu_size_log2 ==
			5 ? 0x80 : 0x20;
		int extended_pic_width = (pic->width + hevc->lcu_size -1)
				& (~(hevc->lcu_size - 1));
		int extended_pic_height = (pic->height + hevc->lcu_size -1)
				& (~(hevc->lcu_size - 1));
		int lcu_x_num = extended_pic_width / hevc->lcu_size;
		int lcu_y_num = extended_pic_height / hevc->lcu_size;
		new_size =  lcu_x_num * lcu_y_num * MV_MEM_UNIT;
		hevc->mv_buf_size = (new_size + 0xffff) & (~0xffff);
	} else {
		if (IS_8K_SIZE(pic->width, pic->height))
			new_size = MPRED_8K_MV_BUF_SIZE;
		else if (IS_4K_SIZE(pic->width, pic->height))
			new_size = MPRED_4K_MV_BUF_SIZE; /*0x120000*/
		else
			new_size = MPRED_MV_BUF_SIZE;

		if (new_size != hevc->mv_buf_size) {
			dealloc_mv_bufs(hevc);
			hevc->mv_buf_size = new_size;
		}
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			if (hevc->m_mv_BUF[i].start_adr &&
				hevc->m_mv_BUF[i].used_flag == 0) {
				hevc->m_mv_BUF[i].used_flag = 1;
				ret = i;
				break;
			}
		}
	}
	if (ret < 0) {
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			if (hevc->m_mv_BUF[i].start_adr == 0) {
				if (alloc_mv_buf(hevc, i) >= 0) {
					hevc->m_mv_BUF[i].used_flag = 1;
					ret = i;
				}
				break;
			}
		}
	}

	if (ret >= 0) {
		pic->mv_buf_index = ret;
		pic->mv_size = hevc->m_mv_BUF[ret].size;
		pic->mpred_mv_wr_start_addr =
			(hevc->m_mv_BUF[ret].start_adr + 0xffff) &
			(~0xffff);
		hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
			"%s => %d (0x%x) size 0x%x\n",
			__func__, ret,
			pic->mpred_mv_wr_start_addr,
			pic->mv_size);

	} else {
		hevc_print(hevc, 0,
		"%s: Error, mv buf is not enough\n",
		__func__);
	}
	return ret;
#endif
}

static void put_mv_buf(struct hevc_state_s *hevc,
	struct PIC_s *pic)
{
#ifndef MV_USE_FIXED_BUF
	int i = pic->mv_buf_index;
	if (i < 0 || i >= MAX_REF_PIC_NUM) {
		hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
			"%s: index %d beyond range\n", __func__, i);
		return;
	}
	if (mv_buf_dynamic_alloc) {
		hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
			"%s(%d)\n", __func__, i);

		decoder_bmmu_box_free_idx(
			hevc->bmmu_box,
			MV_BUFFER_IDX(i));
		hevc->m_mv_BUF[i].start_adr = 0;
		hevc->m_mv_BUF[i].size = 0;
		hevc->m_mv_BUF[i].used_flag = 0;
		pic->mv_buf_index = -1;
		return;
	}

	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s(%d): used_flag(%d)\n",
		__func__, i, hevc->m_mv_BUF[i].used_flag);

	if (hevc->m_mv_BUF[i].start_adr &&
		hevc->m_mv_BUF[i].used_flag)
		hevc->m_mv_BUF[i].used_flag = 0;
	pic->mv_buf_index = -1;
#endif
}

static int hevc_get_header_size(int w, int h)
{
	w = ALIGN(w, 64);
	h = ALIGN(h, 64);

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
			(IS_8K_SIZE(w, h)))
		return ALIGN(MMU_COMPRESS_HEADER_SIZE_8K, 0x10000);
	else
		return ALIGN(MMU_COMPRESS_HEADER_SIZE_4K, 0x10000);
}

static void hevc_put_video_frame(void *vdec_ctx, struct vframe_s *vf)
{
	vh265_vf_put(vf, vdec_ctx);
}

static void hevc_get_video_frame(void *vdec_ctx, struct vframe_s **vf)
{
	*vf = vh265_vf_get(vdec_ctx);
}

static struct internal_comp_buf* v4lfb_to_icomp_buf(
		struct hevc_state_s *hevc,
		struct vdec_v4l2_buffer *fb)
{
	struct aml_video_dec_buf *aml_fb = NULL;
	struct aml_vcodec_ctx * v4l2_ctx = hevc->v4l2_ctx;

	aml_fb = container_of(fb, struct aml_video_dec_buf, frame_buffer);
	return &v4l2_ctx->comp_bufs[aml_fb->internal_index];
}

static struct internal_comp_buf* index_to_icomp_buf(
		struct hevc_state_s *hevc, int index)
{
	struct aml_video_dec_buf *aml_fb = NULL;
	struct aml_vcodec_ctx * v4l2_ctx = hevc->v4l2_ctx;
	struct vdec_v4l2_buffer *fb = NULL;

	fb = (struct vdec_v4l2_buffer *)
		hevc->m_PIC[index]->cma_alloc_addr;
	aml_fb = container_of(fb, struct aml_video_dec_buf, frame_buffer);
	return &v4l2_ctx->comp_bufs[aml_fb->internal_index];
}

static struct task_ops_s task_dec_ops = {
	.type		= TASK_TYPE_DEC,
	.get_vframe	= hevc_get_video_frame,
	.put_vframe	= hevc_put_video_frame,
};

static int v4l_alloc_buf(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	int ret = -1;
	int i = pic->index;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)hevc->v4l2_ctx;
	struct vdec_v4l2_buffer *fb = NULL;

	ret = ctx->fb_ops.alloc(&ctx->fb_ops, hevc->fb_token, &fb, AML_FB_REQ_DEC);
	if (ret < 0) {
		hevc_print(hevc, 0, "[%d] H265 get buffer fail.\n", ctx->id);
		return ret;
	}

	fb->task->attach(fb->task, &task_dec_ops, hw_to_vdec(hevc));
	fb->status = FB_ST_DECODER;

	if (hevc->mmu_enable) {
		struct internal_comp_buf *ibuf = v4lfb_to_icomp_buf(hevc, fb);
		hevc->m_BUF[i].header_addr = ibuf->header_addr;
	}

	hevc->m_BUF[i].used_flag	= 1;
	hevc->m_BUF[i].v4l_ref_buf_addr	= (ulong)fb;
	pic->cma_alloc_addr		= hevc->m_BUF[i].v4l_ref_buf_addr;
	if (fb->num_planes == 1) {
		hevc->m_BUF[i].start_adr = fb->m.mem[0].addr;
		hevc->m_BUF[i].luma_size = fb->m.mem[0].offset;
		hevc->m_BUF[i].size = fb->m.mem[0].size;
		fb->m.mem[0].bytes_used = fb->m.mem[0].size;
		pic->dw_y_adr = hevc->m_BUF[i].start_adr;
		pic->dw_u_v_adr = pic->dw_y_adr + hevc->m_BUF[i].luma_size;
		pic->luma_size = fb->m.mem[0].offset;
		pic->chroma_size = fb->m.mem[0].size - fb->m.mem[0].offset;
	} else if (fb->num_planes == 2) {
		hevc->m_BUF[i].start_adr = fb->m.mem[0].addr;
		hevc->m_BUF[i].luma_size = fb->m.mem[0].size;
		hevc->m_BUF[i].chroma_addr = fb->m.mem[1].addr;
		hevc->m_BUF[i].chroma_size = fb->m.mem[1].size;
		hevc->m_BUF[i].size = fb->m.mem[0].size + fb->m.mem[1].size;
		fb->m.mem[0].bytes_used = fb->m.mem[0].size;
		fb->m.mem[1].bytes_used = fb->m.mem[1].size;
		pic->dw_y_adr = hevc->m_BUF[i].start_adr;
		pic->dw_u_v_adr = hevc->m_BUF[i].chroma_addr;
		pic->luma_size = fb->m.mem[0].size;
		pic->chroma_size = fb->m.mem[1].size;
	}

	return ret;
}

static void set_buf_unused(struct hevc_state_s *hevc, int i)
{
	if (i >= 0 && i < BUF_POOL_SIZE)
		hevc->m_BUF[i].used_flag = 0;
}

static void dealloc_unused_buf(struct hevc_state_s *hevc)
{
	int i;
	for (i = 0; i < BUF_POOL_SIZE; i++) {
		if (hevc->m_BUF[i].start_adr &&
			hevc->m_BUF[i].used_flag == 0) {
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
				hevc_print(hevc, 0,
					"dealloc buf(%d) adr 0x%p size 0x%x\n",
					i, hevc->m_BUF[i].start_adr,
					hevc->m_BUF[i].size);
			}

			hevc->m_BUF[i].start_adr = 0;
			hevc->m_BUF[i].header_addr = 0;
			hevc->m_BUF[i].size = 0;
		}
	}
}

static int v4l_parser_work_pic_num(struct hevc_state_s *hevc)
{
	int used_buf_num = 0;

	pr_debug("margin = %d, sps_max_dec_pic_buffering_minus1_0 = %d\n",
		get_dynamic_buf_num_margin(hevc),
		hevc->param.p.sps_max_dec_pic_buffering_minus1_0);

	used_buf_num = hevc->param.p.sps_max_dec_pic_buffering_minus1_0 + 1;
	/*
	1. need one more for multi instance, as apply_ref_pic_set()
	   has no chanch to run to clear referenced flag in some case
	2. for eos add more buffer to flush.
	*/
	used_buf_num += 2;

	if (hevc->save_buffer_mode)
		hevc_print(hevc, 0,
			"save buf _mode : dynamic_buf_num_margin %d ----> %d \n",
			dynamic_buf_num_margin,  hevc->dynamic_buf_num_margin);

	if (used_buf_num > max_buf_num)
		used_buf_num = max_buf_num;
	return used_buf_num;
}

static int get_alloc_pic_count(struct hevc_state_s *hevc)
{
	int alloc_pic_count = 0;
	int i;
	struct PIC_s *pic;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic && pic->index >= 0)
			alloc_pic_count++;
	}
	return alloc_pic_count;
}

static int v4l_config_pic(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	int i = pic->index;
	int dw_mode = get_double_write_mode(hevc);

	if (hevc->mmu_enable)
		pic->header_adr = hevc->m_BUF[i].header_addr;

	pic->BUF_index		= i;
	pic->POC		= INVALID_POC;
	pic->mc_canvas_y	= pic->index;
	pic->mc_canvas_u_v	= pic->index;

	if (dw_mode & 0x10) {
		pic->mc_canvas_y = (pic->index << 1);
		pic->mc_canvas_u_v = (pic->index << 1) + 1;
		pic->mc_y_adr = pic->dw_y_adr;
		pic->mc_u_v_adr = pic->dw_u_v_adr;
	}

	return 0;
}

static void init_pic_list(struct hevc_state_s *hevc)
{
	int i;
	int dw_mode = get_double_write_mode(hevc);
	struct vdec_s *vdec = hw_to_vdec(hevc);

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		struct PIC_s *pic = hevc->m_PIC[i];

		if (!pic) {
			pic = vzalloc(sizeof(struct PIC_s));
			if (pic == NULL) {
				hevc_print(hevc, 0,
					"%s: alloc pic %d fail!!!\n",
					__func__, i);
				break;
			}
			hevc->m_PIC[i] = pic;
		} else {
			char *aux_data_tmp;
			u32 aux_data_size;

			aux_data_tmp = pic->aux_data_buf;
			aux_data_size = pic->aux_data_size;
			memset(pic, 0, sizeof(struct PIC_s));
			pic->aux_data_buf = aux_data_tmp;
			pic->aux_data_size = aux_data_size;
		}

		pic->index = i;
		pic->BUF_index = -1;
		pic->mv_buf_index = -1;
		if (vdec->parallel_dec == 1) {
			pic->y_canvas_index = -1;
			pic->uv_canvas_index = -1;
		}

		pic->width = hevc->pic_w;
		pic->height = hevc->pic_h;
		pic->double_write_mode = dw_mode;
		pic->POC = INVALID_POC;
	}
}

static void uninit_pic_list(struct hevc_state_s *hevc)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);
	int i;
#ifndef MV_USE_FIXED_BUF
	dealloc_mv_bufs(hevc);
#endif
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		struct PIC_s *pic = hevc->m_PIC[i];

		if (pic) {
			if (vdec->parallel_dec == 1) {
				vdec->free_canvas_ex(pic->y_canvas_index, vdec->id);
				vdec->free_canvas_ex(pic->uv_canvas_index, vdec->id);
			}
			release_aux_data(hevc, pic);
			vfree(pic);
			hevc->m_PIC[i] = NULL;
		}
	}
}

#ifdef LOSLESS_COMPRESS_MODE
static void init_decode_head_hw(struct hevc_state_s *hevc)
{

	struct BuffInfo_s *buf_spec = hevc->work_space_buf;
	unsigned int data32;

	int losless_comp_header_size =
		compute_losless_comp_header_size(hevc->pic_w,
			 hevc->pic_h);
	int losless_comp_body_size = compute_losless_comp_body_size(hevc,
		hevc->pic_w, hevc->pic_h, hevc->mem_saving_mode);

	hevc->losless_comp_body_size = losless_comp_body_size;


	if (hevc->mmu_enable) {
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0x1 << 4));
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, 0x0);
	} else {
	if (hevc->mem_saving_mode == 1)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1,
			(1 << 3) | ((workaround_enable & 2) ? 1 : 0));
	else
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1,
			((workaround_enable & 2) ? 1 : 0));
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, (losless_comp_body_size >> 5));
	}
	WRITE_VREG(HEVC_CM_BODY_LENGTH, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH, losless_comp_header_size);

	if (hevc->mmu_enable) {
		WRITE_VREG(HEVC_SAO_MMU_VH0_ADDR, buf_spec->mmu_vbh.buf_start);
		WRITE_VREG(HEVC_SAO_MMU_VH1_ADDR,
			buf_spec->mmu_vbh.buf_start +
			buf_spec->mmu_vbh.buf_size/2);
		data32 = READ_VREG(HEVC_SAO_CTRL9);
		data32 |= 0x1;
		WRITE_VREG(HEVC_SAO_CTRL9, data32);

		/* use HEVC_CM_HEADER_START_ADDR */
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		data32 |= (1<<10);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}

	if (!hevc->m_ins_flag)
		hevc_print(hevc, 0,
			"%s: (%d, %d) body_size 0x%x header_size 0x%x\n",
			__func__, hevc->pic_w, hevc->pic_h,
			losless_comp_body_size, losless_comp_header_size);

}
#endif
#define HEVCD_MPP_ANC2AXI_TBL_DATA                 0x3464

static void init_pic_list_hw(struct hevc_state_s *hevc)
{
	int i;
	int cur_pic_num = MAX_REF_PIC_NUM;
	int dw_mode = get_double_write_mode(hevc);
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL)
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR,
			(0x1 << 1) | (0x1 << 2));
	else
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x0);

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		if (hevc->m_PIC[i] == NULL ||
			hevc->m_PIC[i]->index == -1) {
			cur_pic_num = i;
			break;
		}
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) {
			if (hevc->mmu_enable && ((dw_mode & 0x10) == 0))
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
					hevc->m_PIC[i]->header_adr>>5);
			else
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
					hevc->m_PIC[i]->mc_y_adr >> 5);
		} else
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
				hevc->m_PIC[i]->mc_y_adr |
				(hevc->m_PIC[i]->mc_canvas_y << 8) | 0x1);
		if (dw_mode & 0x10) {
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) {
					WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
					hevc->m_PIC[i]->mc_u_v_adr >> 5);
				}
			else
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
					hevc->m_PIC[i]->mc_u_v_adr |
					(hevc->m_PIC[i]->mc_canvas_u_v << 8)
					| 0x1);
		}
	}
	if (cur_pic_num == 0)
		return;

	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x1);

	/* Zero out canvas registers in IPP -- avoid simulation X */
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			   (0 << 8) | (0 << 1) | 1);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);

#ifdef LOSLESS_COMPRESS_MODE
	if ((dw_mode & 0x10) == 0)
		init_decode_head_hw(hevc);
#endif

}

static void dump_pic_list(struct hevc_state_s *hevc)
{
	int i;
	struct PIC_s *pic;
	PR_INIT(256);

	hevc_print(hevc, 0,
		"pic_list_init_flag is %d\r\n", hevc->pic_list_init_flag);
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		PR_FILL("index %d buf_idx %d mv_idx %d decode_idx:%d,	POC:%d,	referenced:%d (LT %d),	",
		 pic->index, pic->BUF_index,
#ifndef MV_USE_FIXED_BUF
		pic->mv_buf_index,
#else
		 -1,
#endif
		 pic->decode_idx, pic->POC, pic->referenced
#ifdef SUPPORT_LONG_TERM_RPS
		 , pic->long_term_ref
#else
		 , 0
#endif
		 );
		PR_FILL("num_reorder_pic:%d, output_mark:%d, error_mark:%d w/h %d,%d",
				pic->num_reorder_pic, pic->output_mark, pic->error_mark,
				pic->width, pic->height);
		PR_FILL("output_ready:%d, mv_wr_start %x vf_ref %d",
				pic->output_ready, pic->mpred_mv_wr_start_addr,
				pic->vf_ref);
		PR_INFO(hevc->index);
	}
}

static void clear_referenced_flag(struct hevc_state_s *hevc)
{
	int i;
	struct PIC_s *pic;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		if (pic->referenced) {
			pic->referenced = 0;
			put_mv_buf(hevc, pic);
		}
	}
}

static void clear_poc_flag(struct hevc_state_s *hevc)
{
	int i;
	struct PIC_s *pic;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		pic->POC = INVALID_POC;
	}
}

static struct PIC_s *output_pic(struct hevc_state_s *hevc,
		unsigned char flush_flag)
{
	int num_pic_not_yet_display = 0;
	int i, fisrt_pic_flag = 0;
	struct PIC_s *pic;
	struct PIC_s *pic_display = NULL;
	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (hevc->i_only & 0x4) {
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic == NULL ||
				(pic->index == -1) ||
				(pic->BUF_index == -1) ||
				(pic->POC == INVALID_POC))
				continue;
			if (pic->output_mark) {
				if (pic_display) {
					if (pic->decode_idx <
						pic_display->decode_idx)
						pic_display = pic;

				} else
					pic_display = pic;
			}
		}
		if (pic_display) {
			pic_display->output_mark = 0;
			pic_display->recon_mark = 0;
			pic_display->output_ready = 1;
			pic_display->referenced = 0;
			put_mv_buf(hevc, pic_display);
		}
	} else {
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic == NULL ||
				(pic->index == -1) ||
				(pic->BUF_index == -1) ||
				(pic->POC == INVALID_POC))
				continue;
			if (pic->output_mark)
				num_pic_not_yet_display++;
			if (pic->slice_type == 2 &&
				atomic_read(&hevc->vf_pre_count) == 0 &&
				fast_output_enable & 0x1) {
				/*fast output for first I picture*/
				pic->num_reorder_pic = 0;
				if (vdec->master || vdec->slave)
					pic_display = pic;
				fisrt_pic_flag = 1;
				hevc_print(hevc, 0, "VH265: output first frame\n");
			}
		}

		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic == NULL ||
				(pic->index == -1) ||
				(pic->BUF_index == -1) ||
				(pic->POC == INVALID_POC))
				continue;
			if (pic->output_mark) {
				if (pic_display) {
					if (pic->POC < pic_display->POC)
						pic_display = pic;
					else if ((pic->POC == pic_display->POC) &&
						(pic->decode_idx < pic_display->decode_idx))
						pic_display = pic;
				} else
					pic_display = pic;
			}
		}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		/* dv wait cur_pic all data get,
		some data may get after picture output */
		if ((vdec->master || vdec->slave)
			&& (pic_display == hevc->cur_pic) &&
			(!flush_flag) &&
			(hevc->bypass_dvenl && !dolby_meta_with_el)
			&& (!fisrt_pic_flag))
			pic_display = NULL;
#endif
		if (pic_display) {
			if ((num_pic_not_yet_display >
				pic_display->num_reorder_pic)
				|| flush_flag) {
				pic_display->output_mark = 0;
				pic_display->recon_mark = 0;
				pic_display->output_ready = 1;
			} else if (num_pic_not_yet_display >=
				(MAX_REF_PIC_NUM - 1)) {
				pic_display->output_mark = 0;
				pic_display->recon_mark = 0;
				pic_display->output_ready = 1;
				hevc_print(hevc, 0,
					"Warning, num_reorder_pic %d is byeond buf num\n",
					pic_display->num_reorder_pic);
			} else
				pic_display = NULL;
		}
	}

	if (pic_display && hevc->sps_num_reorder_pics_0 &&
		(atomic_read(&hevc->vf_pre_count) == 1) && (hevc->first_pic_flag == 1)) {
		pic_display = NULL;
		hevc->first_pic_flag = 2;
	}
	return pic_display;
}

static int config_mc_buffer(struct hevc_state_s *hevc, struct PIC_s *cur_pic)
{
	int i;
	struct PIC_s *pic;

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"config_mc_buffer entered .....\n");
	if (cur_pic->slice_type != 2) {	/* P and B pic */
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				   (0 << 8) | (0 << 1) | 1);
		for (i = 0; i < cur_pic->RefNum_L0; i++) {
			pic = get_ref_pic_by_POC(hevc,
				cur_pic->m_aiRefPOCList0[cur_pic->slice_idx][i]);
			if (pic) {
				if ((pic->width != hevc->pic_w) ||
					(pic->height != hevc->pic_h)) {
					hevc_print(hevc, 0,
						"%s: Wrong reference pic (poc %d) width/height %d/%d\n",
						__func__, pic->POC,
						pic->width, pic->height);
					cur_pic->error_mark = 1;
				}
				if (pic->error_mark && (ref_frame_mark_flag[hevc->index]))
					cur_pic->error_mark = 1;
				WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
						(pic->mc_canvas_u_v << 16)
						| (pic->mc_canvas_u_v
							<< 8) |
						   pic->mc_canvas_y);
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print_cont(hevc, 0,
					"refid %x mc_canvas_u_v %x",
					 i, pic->mc_canvas_u_v);
					hevc_print_cont(hevc, 0,
						" mc_canvas_y %x\n",
					 pic->mc_canvas_y);
				}
			} else
				cur_pic->error_mark = 1;

			if (pic == NULL || pic->error_mark) {
				hevc_print(hevc, 0,
				"Error %s, %dth poc (%d) %s",
				 __func__, i,
				 cur_pic->m_aiRefPOCList0[cur_pic->slice_idx][i],
				 pic ? "has error" :
				 "not in list0");
			}
		}
	}
	if (cur_pic->slice_type == 0) {	/* B pic */
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print(hevc, 0,
				"config_mc_buffer RefNum_L1\n");
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				   (16 << 8) | (0 << 1) | 1);

		for (i = 0; i < cur_pic->RefNum_L1; i++) {
			pic = get_ref_pic_by_POC(hevc,
				cur_pic->m_aiRefPOCList1[cur_pic->slice_idx][i]);
			if (pic) {
				if ((pic->width != hevc->pic_w) ||
					(pic->height != hevc->pic_h)) {
					hevc_print(hevc, 0,
						"%s: Wrong reference pic (poc %d) width/height %d/%d\n",
						__func__, pic->POC,
						pic->width, pic->height);
					cur_pic->error_mark = 1;
				}

				if (pic->error_mark && (ref_frame_mark_flag[hevc->index]))
					cur_pic->error_mark = 1;
				WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
						   (pic->mc_canvas_u_v << 16)
						   | (pic->mc_canvas_u_v
								   << 8) |
						   pic->mc_canvas_y);
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print_cont(hevc, 0,
					"refid %x mc_canvas_u_v %x",
					 i, pic->mc_canvas_u_v);
					hevc_print_cont(hevc, 0,
						" mc_canvas_y %x\n",
					 pic->mc_canvas_y);
				}
			} else
				cur_pic->error_mark = 1;

			if (pic == NULL || pic->error_mark) {
				hevc_print(hevc, 0,
				"Error %s, %dth poc (%d) %s",
				 __func__, i,
				 cur_pic->m_aiRefPOCList1[cur_pic->slice_idx][i],
				 pic ? "has error" :
				 "not in list1");
			}
		}
	}
	return 0;
}

#ifdef SUPPORT_LONG_TERM_RPS
static unsigned char is_ref_long_term(struct hevc_state_s *hevc, int poc)
{
	int ii;
	struct PIC_s *pic;
	for (ii = 0; ii < MAX_REF_PIC_NUM; ii++) {
		pic = hevc->m_PIC[ii];
		if (pic == NULL ||
			pic->index == -1 ||
			pic->BUF_index == -1
			)
			continue;

		if (pic->referenced && pic->POC == poc
			&& pic->long_term_ref)
			return 1;
	}
	return 0;
}
#endif

static void apply_ref_pic_set(struct hevc_state_s *hevc, int cur_poc,
							  union param_u *params)
{
	int ii, i;
	int poc_tmp;
	struct PIC_s *pic;
	unsigned char is_referenced;

	if (pic_list_debug & 0x2) {
		pr_err("cur poc %d\n", cur_poc);
	}
	for (ii = 0; ii < MAX_REF_PIC_NUM; ii++) {
		pic = hevc->m_PIC[ii];
		if (pic == NULL ||
			pic->index == -1 ||
			pic->BUF_index == -1
			)
			continue;

#ifdef SUPPORT_LONG_TERM_RPS
		pic->long_term_ref = 0;
#endif
		if ((pic->referenced == 0 || pic->POC == cur_poc))
			continue;
		is_referenced = 0;

		for (i = 0; i < 16; i++) {
			int delt;
#ifdef SUPPORT_LONG_TERM_RPS
			if (params->p.CUR_RPS[i] == RPS_END)
				break;
#else
			if (params->p.CUR_RPS[i] & 0x8000)
				break;
#endif
			delt =
				params->p.CUR_RPS[i] &
				((1 << (RPS_USED_BIT - 1)) - 1);
			if (params->p.CUR_RPS[i] & (1 << (RPS_USED_BIT - 1))) {
				poc_tmp =
					cur_poc - ((1 << (RPS_USED_BIT - 1)) -
							   delt);
			} else
				poc_tmp = cur_poc + delt;
			if (poc_tmp == pic->POC) {
#ifdef SUPPORT_LONG_TERM_RPS
				if (params->p.CUR_RPS[i] & (1 << (RPS_LT_BIT)))
					pic->long_term_ref = 1;
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE)
					hevc_print(hevc, 0, "%d: CUR_RPS 0x%x, LT %d\n",
						i, params->p.CUR_RPS[i],
						pic->long_term_ref);
#endif
				is_referenced = 1;
				break;
			}
		}
		if (is_referenced == 0) {
			pic->referenced = 0;
			put_mv_buf(hevc, pic);
			if (pic_list_debug & 0x2) {
				pr_err("set poc %d reference to 0\n", pic->POC);
			}
		}
	}
}

static void set_ref_pic_list(struct hevc_state_s *hevc, union param_u *params)
{
	struct PIC_s *pic = hevc->cur_pic;
	int i, rIdx;
	int num_neg = 0;
	int num_pos = 0;
	int total_num;
	int num_ref_idx_l0_active =
		(params->p.num_ref_idx_l0_active >
		 MAX_REF_ACTIVE) ? MAX_REF_ACTIVE :
		params->p.num_ref_idx_l0_active;
	int num_ref_idx_l1_active =
		(params->p.num_ref_idx_l1_active >
		 MAX_REF_ACTIVE) ? MAX_REF_ACTIVE :
		params->p.num_ref_idx_l1_active;

	int RefPicSetStCurr0[16];
	int RefPicSetStCurr1[16];
#ifdef SUPPORT_LONG_TERM_RPS
	int num_lt = 0;
	int RefPicSetLtCurr[16];
#endif
	PR_INIT(128);

	for (i = 0; i < 16; i++) {
		RefPicSetStCurr0[i] = 0;
		RefPicSetStCurr1[i] = 0;
		pic->m_aiRefPOCList0[pic->slice_idx][i] = 0;
		pic->m_aiRefPOCList1[pic->slice_idx][i] = 0;
	}
	for (i = 0; i < 16; i++) {
#ifdef SUPPORT_LONG_TERM_RPS
		if (params->p.CUR_RPS[i] == RPS_END)
			break;
#else
		if (params->p.CUR_RPS[i] & 0x8000)
			break;
#endif
		if ((params->p.CUR_RPS[i] >> RPS_USED_BIT) & 1) {
			int delt =
				params->p.CUR_RPS[i] &
				((1 << (RPS_USED_BIT - 1)) - 1);

			if ((params->p.CUR_RPS[i] >> (RPS_USED_BIT - 1)) & 1) {
#ifdef SUPPORT_LONG_TERM_RPS
				if ((params->p.CUR_RPS[i] >> RPS_LT_BIT) & 1) {
					RefPicSetLtCurr[num_lt] =
						pic->POC - ((1 << (RPS_USED_BIT - 1)) -
									delt);
					num_lt++;
					continue;
				}
#endif
				RefPicSetStCurr0[num_neg] =
					pic->POC - ((1 << (RPS_USED_BIT - 1)) -
								delt);
				num_neg++;
			} else {
#ifdef SUPPORT_LONG_TERM_RPS
				if ((params->p.CUR_RPS[i] >> RPS_LT_BIT) & 1) {
					RefPicSetLtCurr[num_lt] = pic->POC + delt;
					num_lt++;
					continue;
				}
#endif
				RefPicSetStCurr1[num_pos] = pic->POC + delt;
				num_pos++;
			}
		}
	}
#ifdef SUPPORT_LONG_TERM_RPS
	total_num = num_neg + num_pos + num_lt;
#else
	total_num = num_neg + num_pos;
#endif
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, H265_DEBUG_BUFMGR,
			"%s: curpoc %d slice_type %d, total %d ",
			__func__, pic->POC, params->p.slice_type, total_num);
#ifdef SUPPORT_LONG_TERM_RPS
		hevc_print_cont(hevc, 0,
			"num_neg %d num_lt %d num_list0 %d num_list1 %d\n",
		 num_neg, num_lt, num_ref_idx_l0_active, num_ref_idx_l1_active);
#else
		hevc_print_cont(hevc, 0,
			"num_neg %d num_list0 %d num_list1 %d\n",
		 num_neg, num_ref_idx_l0_active, num_ref_idx_l1_active);
#endif

	}

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, H265_DEBUG_BUFMGR,
			"HEVC Stream buf start ");
		hevc_print_cont(hevc, 0,
			"%x end %x wr %x rd %x lev %x ctl %x intctl %x\n",
			READ_VREG(HEVC_STREAM_START_ADDR),
			READ_VREG(HEVC_STREAM_END_ADDR),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR),
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_FIFO_CTL),
			READ_VREG(HEVC_PARSER_INT_CONTROL));
	}

	if (total_num > 0) {
		if (params->p.modification_flag & 0x1) {
			hevc_print(hevc, H265_DEBUG_BUFMGR, "ref0 POC (modification):");
			for (rIdx = 0; rIdx < num_ref_idx_l0_active; rIdx++) {
				int cIdx = params->p.modification_list[rIdx];

				pic->m_aiRefPOCList0[pic->slice_idx][rIdx] =
#ifdef SUPPORT_LONG_TERM_RPS
					cIdx >= (num_neg + num_pos) ?
						RefPicSetLtCurr[cIdx - num_neg - num_pos] :
#endif
					(cIdx >=
					num_neg ? RefPicSetStCurr1[cIdx -
					num_neg] :
					RefPicSetStCurr0[cIdx]);
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					PR_FILL("%d ",
						   pic->m_aiRefPOCList0[pic->slice_idx][rIdx]);
				}
			}
		} else {
			hevc_print(hevc, H265_DEBUG_BUFMGR, "ref0 POC:");
			for (rIdx = 0; rIdx < num_ref_idx_l0_active; rIdx++) {
				int cIdx = rIdx % total_num;

				pic->m_aiRefPOCList0[pic->slice_idx][rIdx] =
#ifdef SUPPORT_LONG_TERM_RPS
					cIdx >= (num_neg + num_pos) ?
						RefPicSetLtCurr[cIdx - num_neg - num_pos] :
#endif
					(cIdx >=
					num_neg ? RefPicSetStCurr1[cIdx -
					num_neg] :
					RefPicSetStCurr0[cIdx]);
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					PR_FILL("%d ",
						pic->m_aiRefPOCList0[pic->slice_idx][rIdx]);
				}
			}
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			PR_INFO(hevc->index);
		if (params->p.slice_type == B_SLICE) {
			if (params->p.modification_flag & 0x2) {
				hevc_print(hevc, H265_DEBUG_BUFMGR, "ref1 POC (modification):");
				for (rIdx = 0; rIdx < num_ref_idx_l1_active; rIdx++) {
					int cIdx;

					if (params->p.modification_flag & 0x1) {
						cIdx = params->p.modification_list[num_ref_idx_l0_active + rIdx];
					} else {
						cIdx =
							params->p.modification_list[rIdx];
					}
					pic->m_aiRefPOCList1[pic->slice_idx][rIdx] =
#ifdef SUPPORT_LONG_TERM_RPS
					cIdx >= (num_neg + num_pos) ?
						RefPicSetLtCurr[cIdx - num_neg - num_pos] :
#endif
						(cIdx >= num_pos ? RefPicSetStCurr0[cIdx -	num_pos] : RefPicSetStCurr1[cIdx]);
					if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
						PR_FILL("%d ", pic->m_aiRefPOCList1[pic->slice_idx][rIdx]);
					}
				}
			} else {
				hevc_print(hevc, H265_DEBUG_BUFMGR, "ref1 POC:");
				for (rIdx = 0; rIdx < num_ref_idx_l1_active; rIdx++) {
					int cIdx = rIdx % total_num;

					pic->m_aiRefPOCList1[pic->slice_idx][rIdx] =
#ifdef SUPPORT_LONG_TERM_RPS
					cIdx >= (num_neg + num_pos) ?
						RefPicSetLtCurr[cIdx - num_neg - num_pos] :
#endif
						(cIdx >= num_pos ? RefPicSetStCurr0[cIdx - num_pos] : RefPicSetStCurr1[cIdx]);
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						PR_FILL("%d ", pic->m_aiRefPOCList1[pic->slice_idx][rIdx]);
					}
				}
			}
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				PR_INFO(hevc->index);
		}
	}
	/*set m_PIC */
	pic->slice_type = (params->p.slice_type == I_SLICE) ? 2 :
		(params->p.slice_type == P_SLICE) ? 1 :
		(params->p.slice_type == B_SLICE) ? 0 : 3;
	pic->RefNum_L0 = num_ref_idx_l0_active;
	pic->RefNum_L1 = num_ref_idx_l1_active;
}

static void update_tile_info(struct hevc_state_s *hevc, int pic_width_cu,
		int pic_height_cu, int sao_mem_unit,
		union param_u *params)
{
	int i, j;
	int start_cu_x, start_cu_y;
	int sao_vb_size = (sao_mem_unit + (2 << 4)) * pic_height_cu;
	int sao_abv_size = sao_mem_unit * pic_width_cu;
	PR_INIT(128);
#ifdef DETREFILL_ENABLE
	if (hevc->is_swap && get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
		int tmpRefillLcuSize = 1 <<
			(params->p.log2_min_coding_block_size_minus3 +
			3 + params->p.log2_diff_max_min_coding_block_size);
		hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
			"%x, %x, %x, %x\n",
			params->p.slice_segment_address,
			params->p.bit_depth,
			params->p.tiles_enabled_flag,
			tmpRefillLcuSize);
		if (params->p.slice_segment_address == 0 &&
			params->p.bit_depth != 0 &&
			(params->p.tiles_enabled_flag & 1) &&
			tmpRefillLcuSize == 64)
			hevc->delrefill_check = 1;
		else
			hevc->delrefill_check = 0;
	}
#endif

	hevc->tile_enabled = params->p.tiles_enabled_flag & 1;
	if (params->p.tiles_enabled_flag & 1) {
		hevc->num_tile_col = params->p.num_tile_columns_minus1 + 1;
		hevc->num_tile_row = params->p.num_tile_rows_minus1 + 1;

		if (hevc->num_tile_row > MAX_TILE_ROW_NUM
			|| hevc->num_tile_row <= 0) {
			hevc->num_tile_row = 1;
			hevc_print(hevc, 0,
				"%s: num_tile_rows_minus1 (%d) error!!\n",
				   __func__, params->p.num_tile_rows_minus1);
		}
		if (hevc->num_tile_col > MAX_TILE_COL_NUM
			|| hevc->num_tile_col <= 0) {
			hevc->num_tile_col = 1;
			hevc_print(hevc, 0,
				"%s: num_tile_columns_minus1 (%d) error!!\n",
				   __func__, params->p.num_tile_columns_minus1);
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"%s pic_w_cu %d pic_h_cu %d tile_enabled ",
				 __func__, pic_width_cu, pic_height_cu);
			hevc_print_cont(hevc, 0,
				"num_tile_col %d num_tile_row %d:\n",
				hevc->num_tile_col, hevc->num_tile_row);
		}

		if (params->p.tiles_enabled_flag & 2) {	/* uniform flag */
			int w = pic_width_cu / hevc->num_tile_col;
			int h = pic_height_cu / hevc->num_tile_row;

			start_cu_y = 0;
			for (i = 0; i < hevc->num_tile_row; i++) {
				start_cu_x = 0;
				for (j = 0; j < hevc->num_tile_col; j++) {
					if (j == (hevc->num_tile_col - 1)) {
						hevc->m_tile[i][j].width =
							pic_width_cu -
							start_cu_x;
					} else
						hevc->m_tile[i][j].width = w;
					if (i == (hevc->num_tile_row - 1)) {
						hevc->m_tile[i][j].height =
							pic_height_cu -
							start_cu_y;
					} else
						hevc->m_tile[i][j].height = h;
					hevc->m_tile[i][j].start_cu_x
					    = start_cu_x;
					hevc->m_tile[i][j].start_cu_y
					    = start_cu_y;
					hevc->m_tile[i][j].sao_vb_start_addr =
						hevc->work_space_buf->sao_vb.
						buf_start + j * sao_vb_size;
					hevc->m_tile[i][j].sao_abv_start_addr =
						hevc->work_space_buf->sao_abv.
						buf_start + i * sao_abv_size;
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						PR_FILL(
						"{y=%d, x=%d w %d h %d ",
						 i, j, hevc->m_tile[i][j].width,
						 hevc->m_tile[i][j].height);
						PR_FILL(
						"start_x %d start_y %d ",
						 hevc->m_tile[i][j].start_cu_x,
						 hevc->m_tile[i][j].start_cu_y);
						PR_FILL(
						"sao_vb_start 0x%x ",
						 hevc->m_tile[i][j].
						 sao_vb_start_addr);
						PR_FILL(
						"sao_abv_start 0x%x}\n",
						 hevc->m_tile[i][j].
						 sao_abv_start_addr);
						PR_INFO(hevc->index);
					}
					start_cu_x += hevc->m_tile[i][j].width;

				}
				start_cu_y += hevc->m_tile[i][0].height;
			}
		} else {
			start_cu_y = 0;
			for (i = 0; i < hevc->num_tile_row; i++) {
				start_cu_x = 0;
				for (j = 0; j < hevc->num_tile_col; j++) {
					if (j == (hevc->num_tile_col - 1)) {
						hevc->m_tile[i][j].width =
							pic_width_cu -
							start_cu_x;
					} else {
						hevc->m_tile[i][j].width =
							params->p.tile_width[j];
					}
					if (i == (hevc->num_tile_row - 1)) {
						hevc->m_tile[i][j].height =
							pic_height_cu -
							start_cu_y;
					} else {
						hevc->m_tile[i][j].height =
							params->p.tile_height[i];
					}
					hevc->m_tile[i][j].start_cu_x
					    = start_cu_x;
					hevc->m_tile[i][j].start_cu_y
					    = start_cu_y;
					hevc->m_tile[i][j].sao_vb_start_addr =
						hevc->work_space_buf->sao_vb.buf_start +
						j * sao_vb_size;
					hevc->m_tile[i][j].sao_abv_start_addr =
						hevc->work_space_buf->sao_abv.buf_start +
						i * sao_abv_size;
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						PR_FILL(
						"{y=%d, x=%d w %d h %d ",
						 i, j, hevc->m_tile[i][j].width,
						 hevc->m_tile[i][j].height);
						PR_FILL(
						"start_x %d start_y %d ",
						 hevc->m_tile[i][j].start_cu_x,
						 hevc->m_tile[i][j].start_cu_y);
						PR_FILL(
						"sao_vb_start 0x%x ",
						 hevc->m_tile[i][j].sao_vb_start_addr);
						PR_FILL(
						"sao_abv_start 0x%x}\n",
						 hevc->m_tile[i][j].sao_abv_start_addr);
						PR_INFO(hevc->index);
					}
					start_cu_x += hevc->m_tile[i][j].width;
				}
				start_cu_y += hevc->m_tile[i][0].height;
			}
		}
	} else {
		hevc->num_tile_col = 1;
		hevc->num_tile_row = 1;
		hevc->m_tile[0][0].width = pic_width_cu;
		hevc->m_tile[0][0].height = pic_height_cu;
		hevc->m_tile[0][0].start_cu_x = 0;
		hevc->m_tile[0][0].start_cu_y = 0;
		hevc->m_tile[0][0].sao_vb_start_addr =
			hevc->work_space_buf->sao_vb.buf_start;
		hevc->m_tile[0][0].sao_abv_start_addr =
			hevc->work_space_buf->sao_abv.buf_start;
	}
}

static int get_tile_index(struct hevc_state_s *hevc, int cu_adr,
						  int pic_width_lcu)
{
	int cu_x;
	int cu_y;
	int tile_x = 0;
	int tile_y = 0;
	int i;

	if (pic_width_lcu == 0) {
		if (get_dbg_flag(hevc)) {
			hevc_print(hevc, 0,
			"%s Error, pic_width_lcu is 0, pic_w %d, pic_h %d\n",
			 __func__, hevc->pic_w, hevc->pic_h);
		}
		return -1;
	}
	cu_x = cu_adr % pic_width_lcu;
	cu_y = cu_adr / pic_width_lcu;
	if (hevc->tile_enabled) {
		for (i = 0; i < hevc->num_tile_col; i++) {
			if (cu_x >= hevc->m_tile[0][i].start_cu_x)
				tile_x = i;
			else
				break;
		}
		for (i = 0; i < hevc->num_tile_row; i++) {
			if (cu_y >= hevc->m_tile[i][0].start_cu_y)
				tile_y = i;
			else
				break;
		}
	}
	return (tile_x) | (tile_y << 8);
}

static void print_scratch_error(int error_num)
{
}

static void hevc_config_work_space_hw(struct hevc_state_s *hevc)
{
	struct BuffInfo_s *buf_spec = hevc->work_space_buf;

	if (get_dbg_flag(hevc))
		hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
			"%s %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			__func__,
			buf_spec->ipp.buf_start,
			buf_spec->start_adr,
			buf_spec->short_term_rps.buf_start,
			buf_spec->vps.buf_start,
			buf_spec->sps.buf_start,
			buf_spec->pps.buf_start,
			buf_spec->sao_up.buf_start,
			buf_spec->swap_buf.buf_start,
			buf_spec->swap_buf2.buf_start,
			buf_spec->scalelut.buf_start,
			buf_spec->dblk_para.buf_start,
			buf_spec->dblk_data.buf_start,
			buf_spec->dblk_data2.buf_start);
	WRITE_VREG(HEVCD_IPP_LINEBUFF_BASE, buf_spec->ipp.buf_start);
	if ((get_dbg_flag(hevc) & H265_DEBUG_SEND_PARAM_WITH_REG) == 0)
		WRITE_VREG(HEVC_RPM_BUFFER, (u32)hevc->rpm_phy_addr);
	WRITE_VREG(HEVC_SHORT_TERM_RPS, buf_spec->short_term_rps.buf_start);
	WRITE_VREG(HEVC_VPS_BUFFER, buf_spec->vps.buf_start);
	WRITE_VREG(HEVC_SPS_BUFFER, buf_spec->sps.buf_start);
	WRITE_VREG(HEVC_PPS_BUFFER, buf_spec->pps.buf_start);
	WRITE_VREG(HEVC_SAO_UP, buf_spec->sao_up.buf_start);
	if (hevc->mmu_enable) {
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
			WRITE_VREG(HEVC_ASSIST_MMU_MAP_ADDR, hevc->frame_mmu_map_phy_addr);
			hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
				"write HEVC_ASSIST_MMU_MAP_ADDR\n");
		} else
			WRITE_VREG(H265_MMU_MAP_BUFFER, hevc->frame_mmu_map_phy_addr);
	}
	WRITE_VREG(HEVC_SCALELUT, buf_spec->scalelut.buf_start);
#ifdef HEVC_8K_LFTOFFSET_FIX
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) {
		if (buf_spec->max_width <= 4096 && buf_spec->max_height <= 2304)
			WRITE_VREG(HEVC_DBLK_CFG3, 0x4010);
		else
			WRITE_VREG(HEVC_DBLK_CFG3, 0x8020);
		hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
			"write HEVC_DBLK_CFG3 to %x\n", READ_VREG(HEVC_DBLK_CFG3));
	}
#endif
	/* cfg_p_addr */
	WRITE_VREG(HEVC_DBLK_CFG4, buf_spec->dblk_para.buf_start);
	/* cfg_d_addr */
	WRITE_VREG(HEVC_DBLK_CFG5, buf_spec->dblk_data.buf_start);

	WRITE_VREG(HEVC_DBLK_CFGE, buf_spec->dblk_data2.buf_start);

	WRITE_VREG(LMEM_DUMP_ADR, (u32)hevc->lmem_phy_addr);
}

static void parser_cmd_write(void)
{
	u32 i;
	const unsigned short parser_cmd[PARSER_CMD_NUMBER] = {
		0x0401, 0x8401, 0x0800, 0x0402, 0x9002, 0x1423,
		0x8CC3, 0x1423, 0x8804, 0x9825, 0x0800, 0x04FE,
		0x8406, 0x8411, 0x1800, 0x8408, 0x8409, 0x8C2A,
		0x9C2B, 0x1C00, 0x840F, 0x8407, 0x8000, 0x8408,
		0x2000, 0xA800, 0x8410, 0x04DE, 0x840C, 0x840D,
		0xAC00, 0xA000, 0x08C0, 0x08E0, 0xA40E, 0xFC00,
		0x7C00
	};
	for (i = 0; i < PARSER_CMD_NUMBER; i++)
		WRITE_VREG(HEVC_PARSER_CMD_WRITE, parser_cmd[i]);
}

static void hevc_init_decoder_hw(struct hevc_state_s *hevc,
	int decode_pic_begin, int decode_pic_num)
{
	unsigned int data32;
	int i;

	/* m8baby test1902 */
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"%s\n", __func__);
	data32 = READ_VREG(HEVC_PARSER_VERSION);
	if (data32 != 0x00010001) {
		print_scratch_error(25);
		return;
	}
	WRITE_VREG(HEVC_PARSER_VERSION, 0x5a5a55aa);
	data32 = READ_VREG(HEVC_PARSER_VERSION);
	if (data32 != 0x5a5a55aa) {
		print_scratch_error(26);
		return;
	}
	/* reset iqit to start mem init again */
	WRITE_VREG(DOS_SW_RESET3, (1 << 14)
			  );
	CLEAR_VREG_MASK(HEVC_CABAC_CONTROL, 1);
	CLEAR_VREG_MASK(HEVC_PARSER_CORE_CONTROL, 1);

	if (!hevc->m_ins_flag) {
		data32 = READ_VREG(HEVC_STREAM_CONTROL);
		data32 = data32 | (1 << 0);      /* stream_fetch_enable */
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A)
			data32 |= (0xf << 25); /*arwlen_axi_max*/
		WRITE_VREG(HEVC_STREAM_CONTROL, data32);
	}
	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x00000100) {
		print_scratch_error(29);
		return;
	}
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x00000300) {
		print_scratch_error(30);
		return;
	}
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x12345678);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x9abcdef0);
	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x12345678) {
		print_scratch_error(31);
		return;
	}
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x9abcdef0) {
		print_scratch_error(32);
		return;
	}
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x00000100);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x00000300);

	data32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
	data32 &= 0x03ffffff;
	data32 = data32 | (3 << 29) | (2 << 26) | (1 << 24)
			 |	/* stream_buffer_empty_int_amrisc_enable */
			 (1 << 22) |	/* stream_fifo_empty_int_amrisc_enable*/
			 (1 << 7) |	/* dec_done_int_cpu_enable */
			 (1 << 4) |	/* startcode_found_int_cpu_enable */
			 (0 << 3) |	/* startcode_found_int_amrisc_enable */
			 (1 << 0)	/* parser_int_enable */
			 ;
	WRITE_VREG(HEVC_PARSER_INT_CONTROL, data32);

	data32 = READ_VREG(HEVC_SHIFT_STATUS);
	data32 = data32 | (1 << 1) |	/* emulation_check_on */
			 (1 << 0)		/* startcode_check_on */
			 ;
	WRITE_VREG(HEVC_SHIFT_STATUS, data32);

	WRITE_VREG(HEVC_SHIFT_CONTROL, (3 << 6) |/* sft_valid_wr_position */
			   (2 << 4) |	/* emulate_code_length_sub_1 */
			   (2 << 1) |	/* start_code_length_sub_1 */
			   (1 << 0)	/* stream_shift_enable */
			  );

	WRITE_VREG(HEVC_CABAC_CONTROL, (1 << 0)	/* cabac_enable */
			  );
	/* hevc_parser_core_clk_en */
	WRITE_VREG(HEVC_PARSER_CORE_CONTROL, (1 << 0)
			  );

	WRITE_VREG(HEVC_DEC_STATUS_REG, 0);

	/* Initial IQIT_SCALELUT memory -- just to avoid X in simulation */
	if (is_rdma_enable())
		rdma_back_end_work(hevc->rdma_phy_adr, RDMA_SIZE);
	else {
		WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0);/*cfg_p_addr*/
		for (i = 0; i < 1024; i++)
			WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, 0);
	}

	WRITE_VREG(HEVC_DECODE_SIZE, 0);
	/* Send parser_cmd */
	WRITE_VREG(HEVC_PARSER_CMD_WRITE, (1 << 16) | (0 << 0));

	parser_cmd_write();

	WRITE_VREG(HEVC_PARSER_CMD_SKIP_0, PARSER_CMD_SKIP_CFG_0);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_1, PARSER_CMD_SKIP_CFG_1);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_2, PARSER_CMD_SKIP_CFG_2);

	WRITE_VREG(HEVC_PARSER_IF_CONTROL,
			   /* (1 << 8) | // sao_sw_pred_enable */
			   (1 << 5) |	/* parser_sao_if_en */
			   (1 << 2) |	/* parser_mpred_if_en */
			   (1 << 0)	/* parser_scaler_if_en */
			  );

	/* Changed to Start MPRED in microcode */
	/*
	 *   hevc_print(hevc, 0, "[test.c] Start MPRED\n");
	 *   WRITE_VREG(HEVC_MPRED_INT_STATUS,
	 *   (1<<31)
	 *   );
	 */

	WRITE_VREG(HEVCD_IPP_TOP_CNTL, (0 << 1) |	/* enable ipp */
			   (1 << 0)	/* software reset ipp and mpp */
			  );
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, (1 << 1) |	/* enable ipp */
			   (0 << 0)	/* software reset ipp and mpp */
			  );

	if (get_double_write_mode(hevc) & 0x10)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1,
			0x1 << 31  /*/Enable NV21 reference read mode for MC*/
			);

}

static void decoder_hw_reset(void)
{
	int i;
	unsigned int data32;

	/* reset iqit to start mem init again */
	WRITE_VREG(DOS_SW_RESET3, (1 << 14)
			  );
	CLEAR_VREG_MASK(HEVC_CABAC_CONTROL, 1);
	CLEAR_VREG_MASK(HEVC_PARSER_CORE_CONTROL, 1);

	data32 = READ_VREG(HEVC_STREAM_CONTROL);
	data32 = data32 | (1 << 0)	/* stream_fetch_enable */
			 ;
	WRITE_VREG(HEVC_STREAM_CONTROL, data32);

	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x00000100) {
		print_scratch_error(29);
		return;
	}
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x00000300) {
		print_scratch_error(30);
		return;
	}
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x12345678);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x9abcdef0);
	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x12345678) {
		print_scratch_error(31);
		return;
	}
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x9abcdef0) {
		print_scratch_error(32);
		return;
	}
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x00000100);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x00000300);

	data32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
	data32 &= 0x03ffffff;
	data32 = data32 | (3 << 29) | (2 << 26) | (1 << 24)
			 |	/* stream_buffer_empty_int_amrisc_enable */
			 (1 << 22) |	/*stream_fifo_empty_int_amrisc_enable */
			 (1 << 7) |	/* dec_done_int_cpu_enable */
			 (1 << 4) |	/* startcode_found_int_cpu_enable */
			 (0 << 3) |	/* startcode_found_int_amrisc_enable */
			 (1 << 0)	/* parser_int_enable */
			 ;
	WRITE_VREG(HEVC_PARSER_INT_CONTROL, data32);

	data32 = READ_VREG(HEVC_SHIFT_STATUS);
	data32 = data32 | (1 << 1) |	/* emulation_check_on */
			 (1 << 0)		/* startcode_check_on */
			 ;
	WRITE_VREG(HEVC_SHIFT_STATUS, data32);

	WRITE_VREG(HEVC_SHIFT_CONTROL, (3 << 6) |/* sft_valid_wr_position */
			   (2 << 4) |	/* emulate_code_length_sub_1 */
			   (2 << 1) |	/* start_code_length_sub_1 */
			   (1 << 0)	/* stream_shift_enable */
			  );

	WRITE_VREG(HEVC_CABAC_CONTROL, (1 << 0)	/* cabac_enable */
			  );
	/* hevc_parser_core_clk_en */
	WRITE_VREG(HEVC_PARSER_CORE_CONTROL, (1 << 0)
			  );

	/* Initial IQIT_SCALELUT memory -- just to avoid X in simulation */
	WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0);	/* cfg_p_addr */
	for (i = 0; i < 1024; i++)
		WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, 0);

	/* Send parser_cmd */
	WRITE_VREG(HEVC_PARSER_CMD_WRITE, (1 << 16) | (0 << 0));

	parser_cmd_write();

	WRITE_VREG(HEVC_PARSER_CMD_SKIP_0, PARSER_CMD_SKIP_CFG_0);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_1, PARSER_CMD_SKIP_CFG_1);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_2, PARSER_CMD_SKIP_CFG_2);

	WRITE_VREG(HEVC_PARSER_IF_CONTROL,
			   /* (1 << 8) | // sao_sw_pred_enable */
			   (1 << 5) |	/* parser_sao_if_en */
			   (1 << 2) |	/* parser_mpred_if_en */
			   (1 << 0)	/* parser_scaler_if_en */
			  );

	WRITE_VREG(HEVCD_IPP_TOP_CNTL, (0 << 1) |	/* enable ipp */
			   (1 << 0)	/* software reset ipp and mpp */
			  );
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, (1 << 1) |	/* enable ipp */
			   (0 << 0)	/* software reset ipp and mpp */
			  );
}

#ifdef CONFIG_HEVC_CLK_FORCED_ON
static void config_hevc_clk_forced_on(void)
{
	unsigned int rdata32;
	/* IQIT */
	rdata32 = READ_VREG(HEVC_IQIT_CLK_RST_CTRL);
	WRITE_VREG(HEVC_IQIT_CLK_RST_CTRL, rdata32 | (0x1 << 2));

	/* DBLK */
	rdata32 = READ_VREG(HEVC_DBLK_CFG0);
	WRITE_VREG(HEVC_DBLK_CFG0, rdata32 | (0x1 << 2));

	/* SAO */
	rdata32 = READ_VREG(HEVC_SAO_CTRL1);
	WRITE_VREG(HEVC_SAO_CTRL1, rdata32 | (0x1 << 2));

	/* MPRED */
	rdata32 = READ_VREG(HEVC_MPRED_CTRL1);
	WRITE_VREG(HEVC_MPRED_CTRL1, rdata32 | (0x1 << 24));

	/* PARSER */
	rdata32 = READ_VREG(HEVC_STREAM_CONTROL);
	WRITE_VREG(HEVC_STREAM_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_SHIFT_CONTROL);
	WRITE_VREG(HEVC_SHIFT_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_CABAC_CONTROL);
	WRITE_VREG(HEVC_CABAC_CONTROL, rdata32 | (0x1 << 13));
	rdata32 = READ_VREG(HEVC_PARSER_CORE_CONTROL);
	WRITE_VREG(HEVC_PARSER_CORE_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
	WRITE_VREG(HEVC_PARSER_INT_CONTROL, rdata32 | (0x1 << 15));
	rdata32 = READ_VREG(HEVC_PARSER_IF_CONTROL);
	WRITE_VREG(HEVC_PARSER_IF_CONTROL,
			   rdata32 | (0x3 << 5) | (0x3 << 2) | (0x3 << 0));

	/* IPP */
	rdata32 = READ_VREG(HEVCD_IPP_DYNCLKGATE_CONFIG);
	WRITE_VREG(HEVCD_IPP_DYNCLKGATE_CONFIG, rdata32 | 0xffffffff);

	/* MCRCC */
	rdata32 = READ_VREG(HEVCD_MCRCC_CTL1);
	WRITE_VREG(HEVCD_MCRCC_CTL1, rdata32 | (0x1 << 3));
}
#endif

#ifdef MCRCC_ENABLE
static void config_mcrcc_axi_hw(struct hevc_state_s *hevc, int slice_type)
{
	unsigned int rdata32;
	unsigned int rdata32_2;
	int l0_cnt = 0;
	int l1_cnt = 0x7fff;

	if (get_double_write_mode(hevc) & 0x10) {
		l0_cnt = hevc->cur_pic->RefNum_L0;
		l1_cnt = hevc->cur_pic->RefNum_L1;
	}

	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2);	/* reset mcrcc */

	if (slice_type == 2) {	/* I-PIC */
		/* remove reset -- disables clock */
		WRITE_VREG(HEVCD_MCRCC_CTL1, 0x0);
		return;
	}

	if (slice_type == 0) {	/* B-PIC */
		/* Programme canvas0 */
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				   (0 << 8) | (0 << 1) | 0);
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		/* Programme canvas1 */
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				   (16 << 8) | (1 << 1) | 0);
		rdata32_2 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32_2 = rdata32_2 & 0xffff;
		rdata32_2 = rdata32_2 | (rdata32_2 << 16);
		if (rdata32 == rdata32_2 && l1_cnt > 1) {
			rdata32_2 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
			rdata32_2 = rdata32_2 & 0xffff;
			rdata32_2 = rdata32_2 | (rdata32_2 << 16);
		}
		WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32_2);
	} else {		/* P-PIC */
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				   (0 << 8) | (1 << 1) | 0);
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		if (l0_cnt == 1) {
			WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32);
		} else {
			/* Programme canvas1 */
			rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
			rdata32 = rdata32 & 0xffff;
			rdata32 = rdata32 | (rdata32 << 16);
			WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32);
		}
	}
	/* enable mcrcc progressive-mode */
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0);
}
#endif

static void config_title_hw(struct hevc_state_s *hevc, int sao_vb_size,
							int sao_mem_unit)
{
	WRITE_VREG(HEVC_sao_mem_unit, sao_mem_unit);
	WRITE_VREG(HEVC_SAO_ABV, hevc->work_space_buf->sao_abv.buf_start);
	WRITE_VREG(HEVC_sao_vb_size, sao_vb_size);
	WRITE_VREG(HEVC_SAO_VB, hevc->work_space_buf->sao_vb.buf_start);
}

static u32 init_aux_size;
static int aux_data_is_avaible(struct hevc_state_s *hevc)
{
	u32 reg_val;

	reg_val = READ_VREG(HEVC_AUX_DATA_SIZE);
	if (reg_val != 0 && reg_val != init_aux_size)
		return 1;
	else
		return 0;
}

static void config_aux_buf(struct hevc_state_s *hevc)
{
	WRITE_VREG(HEVC_AUX_ADR, hevc->aux_phy_addr);
	init_aux_size = ((hevc->prefix_aux_size >> 4) << 16) |
		(hevc->suffix_aux_size >> 4);
	WRITE_VREG(HEVC_AUX_DATA_SIZE, init_aux_size);
}

static void config_mpred_hw(struct hevc_state_s *hevc)
{
	int i;
	unsigned int data32;
	struct PIC_s *cur_pic = hevc->cur_pic;
	struct PIC_s *col_pic = hevc->col_pic;
	int AMVP_MAX_NUM_CANDS_MEM = 3;
	int AMVP_MAX_NUM_CANDS = 2;
	int NUM_CHROMA_MODE = 5;
	int DM_CHROMA_IDX = 36;
	int above_ptr_ctrl = 0;
	int buffer_linear = 1;
	int cu_size_log2 = 3;

	int mpred_mv_rd_start_addr;
	int mpred_curr_lcu_x;
	int mpred_curr_lcu_y;
	int mpred_above_buf_start;
	int mpred_mv_rd_ptr;
	int mpred_mv_rd_ptr_p1;
	int mpred_mv_rd_end_addr;
	int MV_MEM_UNIT;
	int mpred_mv_wr_ptr;
	int *ref_poc_L0, *ref_poc_L1;

	int above_en;
	int mv_wr_en;
	int mv_rd_en;
	int col_isIntra;

	if (hevc->slice_type != 2) {
		above_en = 1;
		mv_wr_en = 1;
		mv_rd_en = 1;
		col_isIntra = 0;
	} else {
		above_en = 1;
		mv_wr_en = 1;
		mv_rd_en = 0;
		col_isIntra = 0;
	}

	mpred_mv_rd_start_addr = col_pic->mpred_mv_wr_start_addr;
	data32 = READ_VREG(HEVC_MPRED_CURR_LCU);
	mpred_curr_lcu_x = data32 & 0xffff;
	mpred_curr_lcu_y = (data32 >> 16) & 0xffff;

	MV_MEM_UNIT =
		hevc->lcu_size_log2 == 6 ? 0x200 : hevc->lcu_size_log2 ==
		5 ? 0x80 : 0x20;
	mpred_mv_rd_ptr =
		mpred_mv_rd_start_addr + (hevc->slice_addr * MV_MEM_UNIT);

	mpred_mv_rd_ptr_p1 = mpred_mv_rd_ptr + MV_MEM_UNIT;
	mpred_mv_rd_end_addr =
		mpred_mv_rd_start_addr +
		col_pic->mv_size;
		//((hevc->lcu_x_num * hevc->lcu_y_num) * MV_MEM_UNIT);

	mpred_above_buf_start = hevc->work_space_buf->mpred_above.buf_start;

	mpred_mv_wr_ptr =
		cur_pic->mpred_mv_wr_start_addr +
		(hevc->slice_addr * MV_MEM_UNIT);

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
			"cur pic index %d  col pic index %d\n", cur_pic->index,
			col_pic->index);
	}

	WRITE_VREG(HEVC_MPRED_MV_WR_START_ADDR,
			   cur_pic->mpred_mv_wr_start_addr);
	WRITE_VREG(HEVC_MPRED_MV_RD_START_ADDR, mpred_mv_rd_start_addr);

	data32 = ((hevc->lcu_x_num - hevc->tile_width_lcu) * MV_MEM_UNIT);
	WRITE_VREG(HEVC_MPRED_MV_WR_ROW_JUMP, data32);
	WRITE_VREG(HEVC_MPRED_MV_RD_ROW_JUMP, data32);

	data32 = READ_VREG(HEVC_MPRED_CTRL0);
	data32 = ((hevc->slice_type & 3) |
			  (hevc->new_pic & 1) << 2 |
			  (hevc->new_tile & 1) << 3 |
			  (hevc->isNextSliceSegment  & 1)<< 4 |
			  (hevc->TMVPFlag  & 1)<< 5 |
			  (hevc->LDCFlag & 1) << 6 |
			  (hevc->ColFromL0Flag & 1)<< 7 |
			  (above_ptr_ctrl & 1)<< 8 |
			  (above_en  & 1) << 9 |
			  (mv_wr_en & 1) << 10 |
			  (mv_rd_en  & 1)<< 11 |
			  (col_isIntra & 1)<< 12 |
			  (buffer_linear & 1)<< 13 |
			  (hevc->LongTerm_Curr & 1) << 14 |
			  (hevc->LongTerm_Col & 1) << 15 |
			  (hevc->lcu_size_log2 & 0xf) << 16 |
			  (cu_size_log2  & 0xf) << 20 | (hevc->plevel & 0x7) << 24);
	data32 &=  ~(1<< 28);
	WRITE_VREG(HEVC_MPRED_CTRL0, data32);

	data32 = READ_VREG(HEVC_MPRED_CTRL1);
	data32 = (
#if 0
			/* no set in m8baby test1902 */
			/* Don't override clk_forced_on , */
			(data32 & (0x1 << 24)) |
#endif
				 hevc->MaxNumMergeCand |
				 AMVP_MAX_NUM_CANDS << 4 |
				 AMVP_MAX_NUM_CANDS_MEM << 8 |
				 NUM_CHROMA_MODE << 12 | DM_CHROMA_IDX << 16);
	WRITE_VREG(HEVC_MPRED_CTRL1, data32);

	data32 = (hevc->pic_w | hevc->pic_h << 16);
	WRITE_VREG(HEVC_MPRED_PIC_SIZE, data32);

	data32 = ((hevc->lcu_x_num - 1) | (hevc->lcu_y_num - 1) << 16);
	WRITE_VREG(HEVC_MPRED_PIC_SIZE_LCU, data32);

	data32 = (hevc->tile_start_lcu_x | hevc->tile_start_lcu_y << 16);
	WRITE_VREG(HEVC_MPRED_TILE_START, data32);

	data32 = (hevc->tile_width_lcu | hevc->tile_height_lcu << 16);
	WRITE_VREG(HEVC_MPRED_TILE_SIZE_LCU, data32);

	data32 = (hevc->RefNum_L0 | hevc->RefNum_L1 << 8 | 0
			  /* col_RefNum_L0<<16| */
			  /* col_RefNum_L1<<24 */
			 );
	WRITE_VREG(HEVC_MPRED_REF_NUM, data32);

#ifdef SUPPORT_LONG_TERM_RPS
	data32 = 0;
	for (i = 0; i < hevc->RefNum_L0; i++) {
		if (is_ref_long_term(hevc,
			cur_pic->m_aiRefPOCList0
				[cur_pic->slice_idx][i]))
			data32 = data32 | (1 << i);
	}
	for (i = 0; i < hevc->RefNum_L1; i++) {
		if (is_ref_long_term(hevc,
			cur_pic->m_aiRefPOCList1
				[cur_pic->slice_idx][i]))
			data32 = data32 | (1 << (i + 16));
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
			"LongTerm_Ref 0x%x\n", data32);
	}
#else
	data32 = hevc->LongTerm_Ref;
#endif
	WRITE_VREG(HEVC_MPRED_LT_REF, data32);

	data32 = 0;
	for (i = 0; i < hevc->RefNum_L0; i++)
		data32 = data32 | (1 << i);
	WRITE_VREG(HEVC_MPRED_REF_EN_L0, data32);

	data32 = 0;
	for (i = 0; i < hevc->RefNum_L1; i++)
		data32 = data32 | (1 << i);
	WRITE_VREG(HEVC_MPRED_REF_EN_L1, data32);

	WRITE_VREG(HEVC_MPRED_CUR_POC, hevc->curr_POC);
	WRITE_VREG(HEVC_MPRED_COL_POC, hevc->Col_POC);

	/* below MPRED Ref_POC_xx_Lx registers must follow Ref_POC_xx_L0 ->
	 *   Ref_POC_xx_L1 in pair write order!!!
	 */
	ref_poc_L0 = &(cur_pic->m_aiRefPOCList0[cur_pic->slice_idx][0]);
	ref_poc_L1 = &(cur_pic->m_aiRefPOCList1[cur_pic->slice_idx][0]);

	WRITE_VREG(HEVC_MPRED_L0_REF00_POC, ref_poc_L0[0]);
	WRITE_VREG(HEVC_MPRED_L1_REF00_POC, ref_poc_L1[0]);

	WRITE_VREG(HEVC_MPRED_L0_REF01_POC, ref_poc_L0[1]);
	WRITE_VREG(HEVC_MPRED_L1_REF01_POC, ref_poc_L1[1]);

	WRITE_VREG(HEVC_MPRED_L0_REF02_POC, ref_poc_L0[2]);
	WRITE_VREG(HEVC_MPRED_L1_REF02_POC, ref_poc_L1[2]);

	WRITE_VREG(HEVC_MPRED_L0_REF03_POC, ref_poc_L0[3]);
	WRITE_VREG(HEVC_MPRED_L1_REF03_POC, ref_poc_L1[3]);

	WRITE_VREG(HEVC_MPRED_L0_REF04_POC, ref_poc_L0[4]);
	WRITE_VREG(HEVC_MPRED_L1_REF04_POC, ref_poc_L1[4]);

	WRITE_VREG(HEVC_MPRED_L0_REF05_POC, ref_poc_L0[5]);
	WRITE_VREG(HEVC_MPRED_L1_REF05_POC, ref_poc_L1[5]);

	WRITE_VREG(HEVC_MPRED_L0_REF06_POC, ref_poc_L0[6]);
	WRITE_VREG(HEVC_MPRED_L1_REF06_POC, ref_poc_L1[6]);

	WRITE_VREG(HEVC_MPRED_L0_REF07_POC, ref_poc_L0[7]);
	WRITE_VREG(HEVC_MPRED_L1_REF07_POC, ref_poc_L1[7]);

	WRITE_VREG(HEVC_MPRED_L0_REF08_POC, ref_poc_L0[8]);
	WRITE_VREG(HEVC_MPRED_L1_REF08_POC, ref_poc_L1[8]);

	WRITE_VREG(HEVC_MPRED_L0_REF09_POC, ref_poc_L0[9]);
	WRITE_VREG(HEVC_MPRED_L1_REF09_POC, ref_poc_L1[9]);

	WRITE_VREG(HEVC_MPRED_L0_REF10_POC, ref_poc_L0[10]);
	WRITE_VREG(HEVC_MPRED_L1_REF10_POC, ref_poc_L1[10]);

	WRITE_VREG(HEVC_MPRED_L0_REF11_POC, ref_poc_L0[11]);
	WRITE_VREG(HEVC_MPRED_L1_REF11_POC, ref_poc_L1[11]);

	WRITE_VREG(HEVC_MPRED_L0_REF12_POC, ref_poc_L0[12]);
	WRITE_VREG(HEVC_MPRED_L1_REF12_POC, ref_poc_L1[12]);

	WRITE_VREG(HEVC_MPRED_L0_REF13_POC, ref_poc_L0[13]);
	WRITE_VREG(HEVC_MPRED_L1_REF13_POC, ref_poc_L1[13]);

	WRITE_VREG(HEVC_MPRED_L0_REF14_POC, ref_poc_L0[14]);
	WRITE_VREG(HEVC_MPRED_L1_REF14_POC, ref_poc_L1[14]);

	WRITE_VREG(HEVC_MPRED_L0_REF15_POC, ref_poc_L0[15]);
	WRITE_VREG(HEVC_MPRED_L1_REF15_POC, ref_poc_L1[15]);

	if (hevc->new_pic) {
		WRITE_VREG(HEVC_MPRED_ABV_START_ADDR, mpred_above_buf_start);
		WRITE_VREG(HEVC_MPRED_MV_WPTR, mpred_mv_wr_ptr);
		WRITE_VREG(HEVC_MPRED_MV_RPTR, mpred_mv_rd_start_addr);
	} else if (!hevc->isNextSliceSegment) {
		WRITE_VREG(HEVC_MPRED_MV_RPTR, mpred_mv_rd_ptr);
	}

	WRITE_VREG(HEVC_MPRED_MV_RD_END_ADDR, mpred_mv_rd_end_addr);
}

static void config_sao_hw(struct hevc_state_s *hevc, union param_u *params)
{
	unsigned int data32, data32_2;
	int misc_flag0 = hevc->misc_flag0;
	int slice_deblocking_filter_disabled_flag = 0;

	int mc_buffer_size_u_v =
		hevc->lcu_total * hevc->lcu_size * hevc->lcu_size / 2;
	int mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff) >> 16;
	struct PIC_s *cur_pic = hevc->cur_pic;
	struct aml_vcodec_ctx * v4l2_ctx = hevc->v4l2_ctx;
	PR_INIT(128);

	data32 = READ_VREG(HEVC_SAO_CTRL0);
	data32 &= (~0xf);
	data32 |= hevc->lcu_size_log2;
	WRITE_VREG(HEVC_SAO_CTRL0, data32);

	data32 = (hevc->pic_w | hevc->pic_h << 16);
	WRITE_VREG(HEVC_SAO_PIC_SIZE, data32);

	data32 = ((hevc->lcu_x_num - 1) | (hevc->lcu_y_num - 1) << 16);
	WRITE_VREG(HEVC_SAO_PIC_SIZE_LCU, data32);

	if (hevc->new_pic)
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, 0xffffffff);
#ifdef LOSLESS_COMPRESS_MODE
/*SUPPORT_10BIT*/
	if ((get_double_write_mode(hevc) & 0x10) == 0) {
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_T7)
			WRITE_VREG(HEVC_SAO_CTRL26, 0);

		data32 = READ_VREG(HEVC_SAO_CTRL5);
		data32 &= (~(0xff << 16));
		if ((get_double_write_mode(hevc) == 8) ||
			(get_double_write_mode(hevc) == 9)) {
			data32 |= (0xff << 16);
			WRITE_VREG(HEVC_SAO_CTRL5, data32);
			WRITE_VREG(HEVC_SAO_CTRL26, 0xf);
		} else {
			if (get_double_write_mode(hevc) == 2 ||
				get_double_write_mode(hevc) == 3)
				data32 |= (0xff<<16);
			else if (get_double_write_mode(hevc) == 4 ||
					get_double_write_mode(hevc) == 5)
				data32 |= (0x33<<16);

			if (hevc->mem_saving_mode == 1)
				data32 |= (1 << 9);
			else
				data32 &= ~(1 << 9);
			if (workaround_enable & 1)
				data32 |= (1 << 7);
			WRITE_VREG(HEVC_SAO_CTRL5, data32);
		}
	}
	data32 = cur_pic->mc_y_adr;
	if (get_double_write_mode(hevc))
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, cur_pic->dw_y_adr);

	if ((get_double_write_mode(hevc) & 0x10) == 0)
		WRITE_VREG(HEVC_CM_BODY_START_ADDR, data32);

	if (hevc->mmu_enable)
		WRITE_VREG(HEVC_CM_HEADER_START_ADDR, cur_pic->header_adr);
#else
	data32 = cur_pic->mc_y_adr;
	WRITE_VREG(HEVC_SAO_Y_START_ADDR, data32);
#endif
	data32 = (mc_buffer_size_u_v_h << 16) << 1;
	WRITE_VREG(HEVC_SAO_Y_LENGTH, data32);

#ifdef LOSLESS_COMPRESS_MODE
/*SUPPORT_10BIT*/
	if (get_double_write_mode(hevc))
		WRITE_VREG(HEVC_SAO_C_START_ADDR, cur_pic->dw_u_v_adr);
#else
	data32 = cur_pic->mc_u_v_adr;
	WRITE_VREG(HEVC_SAO_C_START_ADDR, data32);
#endif
	data32 = (mc_buffer_size_u_v_h << 16);
	WRITE_VREG(HEVC_SAO_C_LENGTH, data32);

	WRITE_VREG(HEVC_SAO_Y_LENGTH, cur_pic->luma_size);
	WRITE_VREG(HEVC_SAO_C_LENGTH, cur_pic->chroma_size);
	if (debug & PRINT_FLAG_V4L_DETAIL) {
		pr_info("[%d] config pic, id: %d, Y:(%x, %d) C:(%x, %d).\n",
			v4l2_ctx->id, cur_pic->index,
			cur_pic->dw_y_adr, cur_pic->luma_size,
			cur_pic->dw_u_v_adr, cur_pic->chroma_size);
	}

#ifdef LOSLESS_COMPRESS_MODE
/*SUPPORT_10BIT*/
	if (get_double_write_mode(hevc)) {
		WRITE_VREG(HEVC_SAO_Y_WPTR, cur_pic->dw_y_adr);
		WRITE_VREG(HEVC_SAO_C_WPTR, cur_pic->dw_u_v_adr);
	}
#else
	/* multi tile to do... */
	data32 = cur_pic->mc_y_adr;
	WRITE_VREG(HEVC_SAO_Y_WPTR, data32);

	data32 = cur_pic->mc_u_v_adr;
	WRITE_VREG(HEVC_SAO_C_WPTR, data32);
#endif
	/* DBLK CONFIG HERE */
	if (hevc->new_pic) {
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
				data32 = (0xff << 8) | (0x0  << 0);
			else
				data32 = (0x57 << 8) |  /* 1st/2nd write both enable*/
					(0x0  << 0);   /* h265 video format*/

			if (hevc->pic_w >= 1280)
				data32 |= (0x1 << 4); /*dblk pipeline mode=1 for performance*/
			data32 &= (~0x300); /*[8]:first write enable (compress)  [9]:double write enable (uncompress)*/
			if (get_double_write_mode(hevc) == 0)
				data32 |= (0x1 << 8); /*enable first write*/
			else if (get_double_write_mode(hevc) == 0x10)
				data32 |= (0x1 << 9); /*double write only*/
			else
				data32 |= ((0x1 << 8)  |(0x1 << 9));

			WRITE_VREG(HEVC_DBLK_CFGB, data32);
			hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
				"[DBLK DEBUG] HEVC1 CFGB : 0x%x\n", data32);
		}
		data32 = (hevc->pic_w | hevc->pic_h << 16);
		WRITE_VREG(HEVC_DBLK_CFG2, data32);

		if ((misc_flag0 >> PCM_ENABLE_FLAG_BIT) & 0x1) {
			data32 =
				((misc_flag0 >>
				  PCM_LOOP_FILTER_DISABLED_FLAG_BIT) &
				 0x1) << 3;
		} else
			data32 = 0;
		data32 |=
			(((params->p.pps_cb_qp_offset & 0x1f) << 4) |
			 ((params->p.pps_cr_qp_offset & 0x1f) << 9));
		data32 |=
			(hevc->lcu_size == 64) ? 0 : ((hevc->lcu_size == 32) ? 1 : 2);
		data32 |= (hevc->pic_w <= 64) ? (1 << 20) : 0;
		WRITE_VREG(HEVC_DBLK_CFG1, data32);

		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
			data32 = 1 << 28; /* Debug only: sts1 chooses dblk_main*/
			WRITE_VREG(HEVC_DBLK_STS1 + 4, data32); /* 0x3510 */
			hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
				"[DBLK DEBUG] HEVC1 STS1 : 0x%x\n",
				data32);
		}
	}
	/* m8baby test1902 */
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	/* [13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32 */
	data32 |= (hevc->mem_map_mode << 12);
	data32 &= (~0xff0);
	data32 |= ((hevc->endian >> 8) & 0xfff);	/* data32 |= 0x670; Big-Endian per 64-bit */
	data32 &= (~0x3); /*[1]:dw_disable [0]:cm_disable*/
	if (get_double_write_mode(hevc) == 0)
		data32 |= 0x2; /*disable double write*/
	else if (get_double_write_mode(hevc) & 0x10)
		data32 |= 0x1; /*disable cm*/
	 if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
			unsigned int data;
			data = (0x57 << 8) |  /* 1st/2nd write both enable*/
				(0x0  << 0);   /* h265 video format*/
			if (hevc->pic_w >= 1280)
				data |= (0x1 << 4); /*dblk pipeline mode=1 for performance*/
			data &= (~0x300); /*[8]:first write enable (compress)  [9]:double write enable (uncompress)*/
			if (get_double_write_mode(hevc) == 0)
				data |= (0x1 << 8); /*enable first write*/
			else if (get_double_write_mode(hevc) & 0x10)
				data |= (0x1 << 9); /*double write only*/
			else
				data |= ((0x1 << 8)  |(0x1 << 9));

			WRITE_VREG(HEVC_DBLK_CFGB, data);
			hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
				"[DBLK DEBUG] HEVC1 CFGB : 0x%x\n", data);
	}

	/* swap uv */
	if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21) ||
		(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21M))
		data32 &= ~(1 << 8); /* NV21 */
	else
		data32 |= (1 << 8); /* NV12 */

	data32 &= (~(3 << 14));
	data32 |= (2 << 14);
	/*
	*  [31:24] ar_fifo1_axi_thred
	*  [23:16] ar_fifo0_axi_thred
	*  [15:14] axi_linealign, 0-16bytes, 1-32bytes, 2-64bytes
	*  [13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32
	*  [11:08] axi_lendian_C
	*  [07:04] axi_lendian_Y
	*  [3]     reserved
	*  [2]     clk_forceon
	*  [1]     dw_disable:disable double write output
	*  [0]     cm_disable:disable compress output
	*/
	WRITE_VREG(HEVC_SAO_CTRL1, data32);
	if (get_double_write_mode(hevc) & 0x10) {
		/* [23:22] dw_v1_ctrl
		 *[21:20] dw_v0_ctrl
		 *[19:18] dw_h1_ctrl
		 *[17:16] dw_h0_ctrl
		 */
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		/*set them all 0 for H265_NV21 (no down-scale)*/
		data32 &= ~(0xff << 16);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	/* [5:4]    -- address_format 00:linear 01:32x32 10:64x32 */
	data32 |= (hevc->mem_map_mode << 4);
	data32 &= (~0xF);
	data32 |= (hevc->endian & 0xf);  /* valid only when double write only */

	/* swap uv */
	if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21) ||
		(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21M))
		data32 |= (1 << 12); /* NV21 */
	else
		data32 &= ~(1 << 12); /* NV12 */

	data32 &= (~(3 << 8));
	data32 |= (2 << 8);
	/*
	* [3:0]   little_endian
	* [5:4]   address_format 00:linear 01:32x32 10:64x32
	* [7:6]   reserved
	* [9:8]   Linear_LineAlignment 00:16byte 01:32byte 10:64byte
	* [11:10] reserved
	* [12]    CbCr_byte_swap
	* [31:13] reserved
	*/
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
	data32 = 0;
	data32_2 = READ_VREG(HEVC_SAO_CTRL0);
	data32_2 &= (~0x300);
	/* slice_deblocking_filter_disabled_flag = 0;
	 *	ucode has handle it , so read it from ucode directly
	 */
	if (hevc->tile_enabled) {
		data32 |= ((misc_flag0 >>
			 LOOP_FILER_ACROSS_TILES_ENABLED_FLAG_BIT) &
			 0x1) << 0;
		data32_2 |= ((misc_flag0 >>
			 LOOP_FILER_ACROSS_TILES_ENABLED_FLAG_BIT) &
			 0x1) << 8;
	}
	/* ucode has handle it,so read it from ucode directly */
	slice_deblocking_filter_disabled_flag = (misc_flag0 >>
			SLICE_DEBLOCKING_FILTER_DISABLED_FLAG_BIT) & 0x1;

	if ((misc_flag0 & (1 << DEBLOCKING_FILTER_OVERRIDE_ENABLED_FLAG_BIT))
		&& (misc_flag0 & (1 << DEBLOCKING_FILTER_OVERRIDE_FLAG_BIT))) {
		data32 |= slice_deblocking_filter_disabled_flag << 2;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			PR_FILL("(1,%x)", data32);
		if (!slice_deblocking_filter_disabled_flag) {
			data32 |= (params->p.slice_beta_offset_div2 & 0xf) << 3;
			data32 |= (params->p.slice_tc_offset_div2 & 0xf) << 7;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				PR_FILL("(2,%x)", data32);
		}
	} else {
		data32 |= ((misc_flag0 >>
			 PPS_DEBLOCKING_FILTER_DISABLED_FLAG_BIT) &
			 0x1) << 2;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			PR_FILL("(3,%x)", data32);
		if (((misc_flag0 >> PPS_DEBLOCKING_FILTER_DISABLED_FLAG_BIT) &
			 0x1) == 0) {
			data32 |= (params->p.pps_beta_offset_div2 & 0xf) << 3;
			data32 |= (params->p.pps_tc_offset_div2 & 0xf) << 7;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				PR_FILL("(4,%x)", data32);
		}
	}
	if ((misc_flag0 & (1 << PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT))
		&& ((misc_flag0 & (1 << SLICE_SAO_LUMA_FLAG_BIT))
			|| (misc_flag0 & (1 << SLICE_SAO_CHROMA_FLAG_BIT))
			|| (!slice_deblocking_filter_disabled_flag))) {
		data32 |= ((misc_flag0 >>
			 SLICE_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			 & 0x1)	<< 1;
		data32_2 |= ((misc_flag0 >>
			SLICE_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			& 0x1) << 9;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			PR_FILL("(5,%x)\n", data32);
	} else {
		data32 |= ((misc_flag0 >>
			 PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			 & 0x1) << 1;
		data32_2 |= ((misc_flag0 >>
			 PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			 & 0x1) << 9;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			PR_FILL("(6,%x)\n", data32);
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		PR_INFO(hevc->index);

	WRITE_VREG(HEVC_DBLK_CFG9, data32);
	WRITE_VREG(HEVC_SAO_CTRL0, data32_2);
}

static void pic_list_process(struct hevc_state_s *hevc)
{
	int work_pic_num = hevc->used_buf_num;
	int alloc_pic_count = 0;
	int i;
	struct PIC_s *pic;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		alloc_pic_count++;
		if (pic->output_mark == 0 && pic->referenced == 0
			&& pic->output_ready == 0
			&& (pic->width != hevc->pic_w ||
				pic->height != hevc->pic_h)
			) {
			set_buf_unused(hevc, pic->BUF_index);
			pic->BUF_index = -1;
			if (alloc_pic_count > work_pic_num) {
				pic->width = 0;
				pic->height = 0;
				ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_MEMORY_START);
				release_pic_mmu_buf(hevc, pic);
				ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_MEMORY_END);
				pic->index = -1;
			} else {
				pic->width = hevc->pic_w;
				pic->height = hevc->pic_h;
			}
		}
	}
	if (alloc_pic_count < work_pic_num) {
		int new_count = alloc_pic_count;
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic && pic->index == -1) {
				pic->index = i;
				pic->BUF_index = -1;
				pic->width = hevc->pic_w;
				pic->height = hevc->pic_h;
				new_count++;
				if (new_count >= work_pic_num)
					break;
			}
		}
	}
	dealloc_unused_buf(hevc);
	if (get_alloc_pic_count(hevc)
		!= alloc_pic_count) {
		hevc_print_cont(hevc, 0,
			"%s: work_pic_num is %d, Change alloc_pic_count from %d to %d\n",
			__func__,
			work_pic_num,
			alloc_pic_count,
			get_alloc_pic_count(hevc));
	}
}

static int get_free_fb_idx(struct hevc_state_s *hevc)
{
	int i;

	for (i = 0; i < hevc->used_buf_num; ++i) {
		if (hevc->m_PIC[i] == NULL)
			continue;

		if ((hevc->m_PIC[i]->referenced == 0) &&
			(hevc->m_PIC[i]->vf_ref == 0) &&
			(!hevc->m_PIC[i]->cma_alloc_addr))
			break;
	}

	return (hevc->m_PIC[i] &&
		(i != hevc->used_buf_num)) ? i : -1;
}

static struct PIC_s *v4l_get_new_pic(struct hevc_state_s *hevc,
		union param_u *rpm_param)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);
	int ret;
	struct aml_vcodec_ctx * v4l = hevc->v4l2_ctx;
	struct v4l_buff_pool *pool = &v4l->cap_pool;
	struct PIC_s *new_pic = NULL;
	struct PIC_s *pic = NULL;
	int i, j, idx;

	for (i = 0; i < pool->in; ++i) {
		u32 state = (pool->seq[i] >> 16);
		u32 index = (pool->seq[i] & 0xffff);

		switch (state) {
		case V4L_CAP_BUFF_IN_DEC:
			for (j = 0; j < hevc->used_buf_num; j++) {
				pic = hevc->m_PIC[j];
				if (pic == NULL || pic->index == -1)
					continue;

				if (pic->output_mark == 0 &&
					pic->referenced == 0 &&
					pic->output_ready == 0 &&
					pic->width == hevc->pic_w &&
					pic->height == hevc->pic_h &&
					pic->vf_ref == 0 &&
					pic->cma_alloc_addr) {
					if (new_pic) {
						if (new_pic->POC != INVALID_POC) {
							if (pic->POC == INVALID_POC ||
								pic->POC < new_pic->POC)
								new_pic = pic;
						}
					} else
						new_pic = pic;
				}
			}
			break;
		case V4L_CAP_BUFF_IN_M2M:
			idx = get_free_fb_idx(hevc);
			if (idx < 0)
				break;
			pic = hevc->m_PIC[idx];
			pic->width = hevc->pic_w;
			pic->height = hevc->pic_h;
			hevc->buffer_wrap[idx] = index;
			if ((pic->index != -1) &&
				!v4l_alloc_buf(hevc, pic)) {
				v4l_config_pic(hevc, pic);
				init_pic_list_hw(hevc);
				new_pic = pic;
			}
			break;
		default:
			break;
		}

		if (new_pic)
			break;
	}

	if (new_pic == NULL)
		return NULL;

	/* for notify eos. */
	if (!rpm_param)
		return new_pic;

	new_pic->double_write_mode = get_double_write_mode(hevc);
	if (new_pic->double_write_mode)
		set_canvas(hevc, new_pic);

	if (get_mv_buf(hevc, new_pic) < 0)
		return NULL;

	if (hevc->mmu_enable) {
		ret = H265_alloc_mmu(hevc, new_pic,
			rpm_param->p.bit_depth,
			hevc->frame_mmu_map_addr);
		if (ret != 0) {
			put_mv_buf(hevc, new_pic);
			hevc_print(hevc, 0,
				"can't alloc need mmu1,idx %d ret =%d\n",
				new_pic->decode_idx, ret);
			vdec_v4l_post_error_event(v4l, DECODER_ERROR_ALLOC_BUFFER_FAIL);
			return NULL;
		}
	}

	new_pic->referenced = 1;
	new_pic->decode_idx = hevc->decode_idx;
	new_pic->slice_idx = 0;
	new_pic->referenced = 1;
	new_pic->output_mark = 0;
	new_pic->recon_mark = 0;
	new_pic->error_mark = 0;
	new_pic->dis_mark = 0;
	new_pic->num_reorder_pic = rpm_param->p.sps_num_reorder_pics_0;
	new_pic->ip_mode = hevc->low_latency_flag ? true :
			(!new_pic->num_reorder_pic &&
			!(vdec->slave || vdec->master) &&
			!disable_ip_mode) ? true : false;
	new_pic->losless_comp_body_size = hevc->losless_comp_body_size;
	new_pic->POC = hevc->curr_POC;
	new_pic->pic_struct = hevc->curr_pic_struct;

	v4l->aux_infos.bind_sei_buffer(v4l, &new_pic->aux_data_buf,
		&new_pic->aux_data_size, &new_pic->ctx_buf_idx);

	new_pic->mem_saving_mode =
		hevc->mem_saving_mode;
	new_pic->bit_depth_luma =
		hevc->bit_depth_luma;
	new_pic->bit_depth_chroma =
		hevc->bit_depth_chroma;
	new_pic->video_signal_type =
		hevc->video_signal_type;

	new_pic->conformance_window_flag =
		hevc->param.p.conformance_window_flag;
	new_pic->conf_win_left_offset =
		hevc->param.p.conf_win_left_offset;
	new_pic->conf_win_right_offset =
		hevc->param.p.conf_win_right_offset;
	new_pic->conf_win_top_offset =
		hevc->param.p.conf_win_top_offset;
	new_pic->conf_win_bottom_offset =
		hevc->param.p.conf_win_bottom_offset;
	new_pic->chroma_format_idc =
			hevc->param.p.chroma_format_idc;

	if (new_pic) {
		struct vdec_v4l2_buffer *fb =
			(struct vdec_v4l2_buffer *)new_pic->cma_alloc_addr;

		fb->status = FB_ST_DECODER;
	}

	hevc_print(hevc, H265_DEBUG_BUFMGR,
		"%s: index %d, buf_idx %d, decode_idx %d, POC %d\n",
		__func__, new_pic->index,
		new_pic->BUF_index, new_pic->decode_idx,
		new_pic->POC);

	return new_pic;
}

static int get_display_pic_num(struct hevc_state_s *hevc)
{
	int i;
	struct PIC_s *pic;
	int num = 0;

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL ||
			pic->index == -1)
			continue;

		if (pic->output_ready == 1)
			num++;
	}
	return num;
}

static void flush_output(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	struct PIC_s *pic_display;

	if (pic) {
		/*PB skip control */
		if (pic->error_mark == 0 && hevc->PB_skip_mode == 1) {
			/* start decoding after first I */
			hevc->ignore_bufmgr_error |= 0x1;
		}
		if (hevc->ignore_bufmgr_error & 1) {
			if (hevc->PB_skip_count_after_decoding > 0)
				hevc->PB_skip_count_after_decoding--;
			else {
				/* start displaying */
				hevc->ignore_bufmgr_error |= 0x2;
			}
		}
		if (pic->POC != INVALID_POC && !pic->ip_mode)
			pic->output_mark = 1;
		pic->recon_mark = 1;
	}
	do {
		pic_display = output_pic(hevc, 1);

		if (pic_display) {
			pic_display->referenced = 0;
			put_mv_buf(hevc, pic_display);
			if ((pic_display->error_mark
				 && ((hevc->ignore_bufmgr_error & 0x2) == 0))
				|| (get_dbg_flag(hevc) &
					H265_DEBUG_DISPLAY_CUR_FRAME)
				|| (get_dbg_flag(hevc) &
					H265_DEBUG_NO_DISPLAY)) {
				struct aml_vcodec_ctx * ctx = hevc->v4l2_ctx;

				pic_display->output_ready = 0;
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print(hevc, H265_DEBUG_BUFMGR,
					"[BM] Display: POC %d, ",
					 pic_display->POC);
					hevc_print_cont(hevc, 0,
					"decoding index %d ==> ",
					 pic_display->decode_idx);
					hevc_print_cont(hevc, 0,
					"Debug mode or error, recycle it\n");
				}
				/*
				 * Here the pic/frame error_mark is 1,
				 * and it won't be displayed, so increase
				 * the drop count
				 */
				hevc->gvs->drop_frame_count++;
				if (pic_display->slice_type == I_SLICE) {
					hevc->gvs->i_lost_frames++;
				} else if (pic_display->slice_type == P_SLICE) {
					hevc->gvs->p_lost_frames++;
				} else if (pic_display->slice_type == B_SLICE) {
					hevc->gvs->b_lost_frames++;
				}

				ctx->decoder_status_info.decoder_error_count++;
				vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);

				/* error frame count also need increase */
				hevc->gvs->error_frame_count++;
				if (pic_display->slice_type == I_SLICE) {
					hevc->gvs->i_concealed_frames++;
				} else if (pic_display->slice_type == P_SLICE) {
					hevc->gvs->p_concealed_frames++;
				} else if (pic_display->slice_type == B_SLICE) {
					hevc->gvs->b_concealed_frames++;
				}
			} else {
				if (hevc->i_only & 0x1
					&& pic_display->slice_type != 2) {
					pic_display->output_ready = 0;
				} else {
					prepare_display_buf(hw_to_vdec(hevc), pic_display);
					if (get_dbg_flag(hevc)
						& H265_DEBUG_BUFMGR) {
						hevc_print(hevc, H265_DEBUG_BUFMGR,
						"[BM] flush Display: POC %d, ",
						 pic_display->POC);
						hevc_print_cont(hevc, 0,
						"decoding index %d\n",
						 pic_display->decode_idx);
					}
				}
			}
		}
	} while (pic_display);
	clear_referenced_flag(hevc);
}

/*
* dv_meta_flag: 1, dolby meta only; 2, not include dolby meta
*/
static void set_aux_data(struct hevc_state_s *hevc,
	struct PIC_s *pic, unsigned char suffix_flag,
	unsigned char dv_meta_flag)
{
	int i;
	unsigned short *aux_adr;
	unsigned int size_reg_val =
		READ_VREG(HEVC_AUX_DATA_SIZE);
	unsigned int aux_count = 0;
	int aux_size = 0;
	if (pic == NULL || 0 == aux_data_is_avaible(hevc))
		return;

	if (hevc->aux_data_dirty ||
		hevc->m_ins_flag == 0) {

		hevc->aux_data_dirty = 0;
	}

	if (suffix_flag) {
		aux_adr = (unsigned short *)
			(hevc->aux_addr +
			hevc->prefix_aux_size);
		aux_count = ((size_reg_val & 0xffff) << 4) >> 1;
		aux_size = hevc->suffix_aux_size;
	} else {
		aux_adr = (unsigned short *)hevc->aux_addr;
		aux_count = ((size_reg_val >> 16) << 4) >> 1;
		aux_size = hevc->prefix_aux_size;
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI) {
		hevc_print(hevc, 0,
			"%s:pic 0x%p old size %d count %d,suf %d dv_flag %d\r\n",
			__func__, pic, pic->aux_data_size,
			aux_count, suffix_flag, dv_meta_flag);
	}

	if (aux_count > aux_size) {
		hevc_print(hevc, 0,
			"%s:aux_count(%d) is over size\n", __func__, aux_count);
		aux_count = 0;
	}
	if (aux_size > 0 && aux_count > 0) {
		int heads_size = 0;

		for (i = 0; i < aux_count; i++) {
			unsigned char tag = aux_adr[i] >> 8;
			if (tag != 0 && tag != 0xff) {
				if (dv_meta_flag == 0)
					heads_size += 8;
				else if (dv_meta_flag == 1 && tag == 0x1)
					heads_size += 8;
				else if (dv_meta_flag == 2 && tag != 0x1)
					heads_size += 8;
			}
		}

		if (pic->aux_data_buf) {
			unsigned char valid_tag = 0;
			unsigned char *h =
				pic->aux_data_buf +
				pic->aux_data_size;
			unsigned char *p = h + 8;
			int len = 0;
			int padding_len = 0;

			for (i = 0; i < aux_count; i += 4) {
				int ii;
				unsigned char tag = aux_adr[i + 3] >> 8;
				if (tag != 0 && tag != 0xff) {
					if (dv_meta_flag == 0)
						valid_tag = 1;
					else if (dv_meta_flag == 1
						&& tag == 0x1)
						valid_tag = 1;
					else if (dv_meta_flag == 2
						&& tag != 0x1)
						valid_tag = 1;
					else
						valid_tag = 0;
					if (valid_tag && len > 0) {
						pic->aux_data_size += (len + 8);
						h[0] = (len >> 24) & 0xff;
						h[1] = (len >> 16) & 0xff;
						h[2] = (len >> 8) & 0xff;
						h[3] = (len >> 0) & 0xff;
						h[6] = (padding_len >> 8) & 0xff;
						h[7] = (padding_len) & 0xff;
						h += (len + 8);
						p += 8;
						len = 0;
						padding_len = 0;
					}
					if (valid_tag) {
						h[4] = tag;
						h[5] = 0;
						h[6] = 0;
						h[7] = 0;
					}
				}
				if (valid_tag) {
					for (ii = 0; ii < 4; ii++) {
						unsigned short aa = aux_adr[i + 3 - ii];
						*p = aa & 0xff;
						p++;
						len++;
					}
				}
			}
			if (len > 0) {
				pic->aux_data_size += (len + 8);
				h[0] = (len >> 24) & 0xff;
				h[1] = (len >> 16) & 0xff;
				h[2] = (len >> 8) & 0xff;
				h[3] = (len >> 0) & 0xff;
				h[6] = (padding_len >> 8) & 0xff;
				h[7] = (padding_len) & 0xff;
			}
			if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI) {
				PR_INIT(128);
				hevc_print(hevc, 0,
					"aux: (size %d) suffix_flag %d\n",
					pic->aux_data_size, suffix_flag);
				for (i = 0; i < pic->aux_data_size; i++) {
					PR_FILL("%02x ", pic->aux_data_buf[i]);
					if (((i + 1) & 0xf) == 0)
						PR_INFO(hevc->index);
				}
				PR_INFO(hevc->index);
			}
		}
	}

}

static void release_aux_data(struct hevc_state_s *hevc,
	struct PIC_s *pic)
{
}

static int recycle_mmu_buf_tail(struct hevc_state_s *hevc,
		bool check_dma)
{
	int index = hevc->cur_pic->BUF_index;
	struct internal_comp_buf *ibuf =
		index_to_icomp_buf(hevc, index);

	hevc_print(hevc,
			H265_DEBUG_BUFMGR_MORE,
			"%s pic index %d scatter_alloc %d page_start %d\n",
			"decoder_mmu_box_free_idx_tail",
			hevc->cur_pic->index,
			hevc->cur_pic->scatter_alloc,
			hevc->used_4k_num);
	if (check_dma)
		hevc_mmu_dma_check(hw_to_vdec(hevc));

	decoder_mmu_box_free_idx_tail(
		ibuf->mmu_box,
		ibuf->index,
		hevc->used_4k_num);

	hevc->cur_pic->scatter_alloc = 2;
	hevc->used_4k_num = -1;
	return 0;
}

static inline void hevc_pre_pic(struct hevc_state_s *hevc,
			struct PIC_s *pic)
{

	/* prev pic */
	int decoded_poc = hevc->iPrevPOC;
#ifdef MULTI_INSTANCE_SUPPORT
	if (hevc->m_ins_flag) {
		decoded_poc = hevc->decoded_poc;
		hevc->decoded_poc = INVALID_POC;
	}
#endif
	if (hevc->m_nalUnitType != NAL_UNIT_CODED_SLICE_IDR
			&& hevc->m_nalUnitType !=
			NAL_UNIT_CODED_SLICE_IDR_N_LP) {
		struct PIC_s *pic_display;

		pic = get_pic_by_POC(hevc, decoded_poc);
		if (pic && (pic->POC != INVALID_POC)) {
			struct vdec_s *vdec = hw_to_vdec(hevc);

			/*PB skip control */
			if (pic->error_mark == 0
					&& hevc->PB_skip_mode == 1) {
				/* start decoding after
				 *   first I
				 */
				hevc->ignore_bufmgr_error |= 0x1;
			}
			if (hevc->ignore_bufmgr_error & 1) {
				if (hevc->PB_skip_count_after_decoding > 0) {
					hevc->PB_skip_count_after_decoding--;
				} else {
					/* start displaying */
					hevc->ignore_bufmgr_error |= 0x2;
				}
			}
			if (hevc->mmu_enable
				&& ((get_double_write_mode(hevc) & 0x10) == 0)) {
				if (!hevc->m_ins_flag) {
					hevc->used_4k_num =
					READ_VREG(HEVC_SAO_MMU_STATUS) >> 16;

					if ((!is_skip_decoding(hevc, pic)) &&
						(hevc->used_4k_num >= 0) &&
						(hevc->cur_pic->scatter_alloc == 1)) {
						ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_MEMORY_START);
						recycle_mmu_buf_tail(hevc, true);
						ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_MEMORY_END);
					}
				}
			}
			if (!pic->ip_mode)
				pic->output_mark = 1;
			pic->recon_mark = 1;
			pic->dis_mark = 1;
			if (vdec->mvfrm) {
				pic->frame_size = vdec->mvfrm->frame_size;
				pic->hw_decode_time = (u32)vdec->mvfrm->hw_decode_time;
			}
		}
		do {
			pic_display = output_pic(hevc, 0);

			if (pic_display) {
				if ((pic_display->error_mark &&
					((hevc->ignore_bufmgr_error & 0x2) == 0))
					|| (get_dbg_flag(hevc) &
						H265_DEBUG_DISPLAY_CUR_FRAME)
					|| (get_dbg_flag(hevc) &
						H265_DEBUG_NO_DISPLAY)) {
					struct aml_vcodec_ctx * ctx = hevc->v4l2_ctx;

					pic_display->output_ready = 0;
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						hevc_print(hevc, H265_DEBUG_BUFMGR,
							"[BM] Display: POC %d, ",
							pic_display->POC);
						hevc_print_cont(hevc, 0,
							"decoding index %d ==> ",
							pic_display->decode_idx);
						hevc_print_cont(hevc, 0,
							"Debug or err,recycle it\n");
					}
					/*
					 * Here the pic/frame error_mark is 1,
					 * and it won't be displayed, so increase
					 * the drop count
					 */
					hevc->gvs->drop_frame_count++;
					if (pic_display->slice_type == I_SLICE) {
						hevc->gvs->i_lost_frames++;
					}else if (pic_display->slice_type == P_SLICE) {
						hevc->gvs->p_lost_frames++;
					} else if (pic_display->slice_type == B_SLICE) {
						hevc->gvs->b_lost_frames++;
					}

					ctx->decoder_status_info.decoder_error_count++;
					vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);

					/* error frame count also need increase */
					hevc->gvs->error_frame_count++;
					if (pic_display->slice_type == I_SLICE) {
						hevc->gvs->i_concealed_frames++;
					} else if (pic_display->slice_type == P_SLICE) {
						hevc->gvs->p_concealed_frames++;
					} else if (pic_display->slice_type == B_SLICE) {
						hevc->gvs->b_concealed_frames++;
					}
				} else {
					if (hevc->i_only & 0x1
						&& pic_display->slice_type != 2) {
						pic_display->output_ready = 0;
					} else {
						prepare_display_buf(hw_to_vdec(hevc),pic_display);
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						hevc_print(hevc, H265_DEBUG_BUFMGR,
							"[BM] Display: POC %d, ",
							pic_display->POC);
						hevc_print_cont(hevc, 0,
							"decoding index %d\n",
							pic_display->decode_idx);
						}
					}
				}
			}
		} while (pic_display);
	} else {
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, H265_DEBUG_BUFMGR,
				"[BM] current pic is IDR, ");
			hevc_print_cont(hevc, 0,
				"clear referenced flag of all buffers\n");
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE)
			dump_pic_list(hevc);
		if (atomic_read(&hevc->vf_pre_count) == 1 &&
				hevc->first_pic_flag == 1) {
			hevc->first_pic_flag = 2;
			pic = NULL;
		}
		else
			pic = get_pic_by_POC(hevc, decoded_poc);

		flush_output(hevc, pic);
	}
}

static void check_pic_decoded_error_pre(struct hevc_state_s *hevc,
	int decoded_lcu)
{
	int current_lcu_idx = decoded_lcu;
	if (decoded_lcu < 0)
		return;

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
			"cur lcu idx = %d, (total %d)\n",
			current_lcu_idx, hevc->lcu_total);
	}
	if ((error_handle_policy & 0x20) == 0 && hevc->cur_pic != NULL) {
		if (hevc->first_pic_after_recover) {
			if (current_lcu_idx !=
			 ((hevc->lcu_x_num_pre*hevc->lcu_y_num_pre) - 1))
				hevc->cur_pic->error_mark = 1;
		} else {
			if (hevc->lcu_x_num_pre != 0
			 && hevc->lcu_y_num_pre != 0
			 && current_lcu_idx != 0
			 && current_lcu_idx <
			 ((hevc->lcu_x_num_pre*hevc->lcu_y_num_pre) - 1))
				hevc->cur_pic->error_mark = 1;
		}
		if (hevc->cur_pic->error_mark) {
			if (print_lcu_error)
				hevc_print(hevc, 0,
					"cur lcu idx = %d, (total %d), set error_mark\n",
					current_lcu_idx,
					hevc->lcu_x_num_pre*hevc->lcu_y_num_pre);
			if (is_log_enable(hevc))
				add_log(hevc,
					"cur lcu idx = %d, (total %d), set error_mark",
					current_lcu_idx,
					hevc->lcu_x_num_pre *
					    hevc->lcu_y_num_pre);

		}
	}
	if (hevc->cur_pic && hevc->head_error_flag) {
		hevc->cur_pic->error_mark = 1;
		hevc_print(hevc, 0,
			"head has error, set error_mark\n");
	}

	if ((error_handle_policy & 0x80) == 0) {
		if (hevc->over_decode && hevc->cur_pic) {
			hevc_print(hevc, 0,
				"over decode, set error_mark\n");
			hevc->cur_pic->error_mark = 1;
		}
	}

	hevc->lcu_x_num_pre = hevc->lcu_x_num;
	hevc->lcu_y_num_pre = hevc->lcu_y_num;
}

static void check_pic_decoded_error(struct hevc_state_s *hevc,
	int decoded_lcu)
{
	int current_lcu_idx = decoded_lcu;
	if (decoded_lcu < 0)
		return;

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
			"cur lcu idx = %d, (total %d)\n",
			current_lcu_idx, hevc->lcu_total);
	}
	if ((error_handle_policy & 0x20) == 0 && hevc->cur_pic != NULL) {
		if (hevc->lcu_x_num != 0
		 && hevc->lcu_y_num != 0
		 && current_lcu_idx != 0
		 && current_lcu_idx <
		 ((hevc->lcu_x_num*hevc->lcu_y_num) - 1))
			hevc->cur_pic->error_mark = 1;

		if (hevc->cur_pic->error_mark) {
			if (print_lcu_error)
				hevc_print(hevc, 0,
					"cur lcu idx = %d, (total %d), set error_mark\n",
					current_lcu_idx,
					hevc->lcu_x_num*hevc->lcu_y_num);
			if (((hevc->i_only & 0x4)  == 0) && hevc->cur_pic->POC && ( hevc->cur_pic->slice_type == 0)
					&& ((hevc->cur_pic->POC + MAX_BUF_NUM) < hevc->iPrevPOC)) {
					hevc_print(hevc, 0,
					"Flush.. num_reorder_pic %d  pic->POC %d  hevc->iPrevPOC %d\n",
					hevc->sps_num_reorder_pics_0,hevc->cur_pic->POC ,hevc->iPrevPOC);
					flush_output(hevc, get_pic_by_POC(hevc, hevc->cur_pic->POC ));
			}
			if (is_log_enable(hevc))
				add_log(hevc,
					"cur lcu idx = %d, (total %d), set error_mark",
					current_lcu_idx,
					hevc->lcu_x_num * hevc->lcu_y_num);

		}

	}
	if (hevc->cur_pic && hevc->head_error_flag) {
		hevc->cur_pic->error_mark = 1;
		hevc_print(hevc, 0,
			"head has error, set error_mark\n");
	}

	if ((error_handle_policy & 0x80) == 0) {
		if (hevc->over_decode && hevc->cur_pic) {
			hevc_print(hevc, 0,
				"over decode, set error_mark\n");
			hevc->cur_pic->error_mark = 1;
		}
	}
}

/* only when we decoded one field or one frame,
we can call this function to get qos info*/
static void get_picture_qos_info(struct hevc_state_s *hevc)
{
	struct PIC_s *picture = hevc->cur_pic;

	if (!hevc->cur_pic)
		return;

	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_G12A) {
		unsigned char a[3];
		unsigned char i, j, t;
		unsigned long  data;

		data = READ_VREG(HEVC_MV_INFO);
		if (picture->slice_type == I_SLICE)
			data = 0;
		a[0] = data & 0xff;
		a[1] = (data >> 8) & 0xff;
		a[2] = (data >> 16) & 0xff;

		for (i = 0; i < 3; i++)
			for (j = i+1; j < 3; j++) {
				if (a[j] < a[i]) {
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				} else if (a[j] == a[i]) {
					a[i]++;
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				}
			}
		picture->max_mv = a[2];
		picture->avg_mv = a[1];
		picture->min_mv = a[0];

		data = READ_VREG(HEVC_QP_INFO);
		a[0] = data & 0x1f;
		a[1] = (data >> 8) & 0x3f;
		a[2] = (data >> 16) & 0x7f;

		for (i = 0; i < 3; i++)
			for (j = i+1; j < 3; j++) {
				if (a[j] < a[i]) {
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				} else if (a[j] == a[i]) {
					a[i]++;
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				}
			}
		picture->max_qp = a[2];
		picture->avg_qp = a[1];
		picture->min_qp = a[0];

		data = READ_VREG(HEVC_SKIP_INFO);
		a[0] = data & 0x1f;
		a[1] = (data >> 8) & 0x3f;
		a[2] = (data >> 16) & 0x7f;

		for (i = 0; i < 3; i++)
			for (j = i+1; j < 3; j++) {
				if (a[j] < a[i]) {
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				} else if (a[j] == a[i]) {
					a[i]++;
					t = a[j];
					a[j] = a[i];
					a[i] = t;
				}
			}
		picture->max_skip = a[2];
		picture->avg_skip = a[1];
		picture->min_skip = a[0];
	} else {
		uint32_t blk88_y_count;
		uint32_t blk88_c_count;
		uint32_t blk22_mv_count;
		uint32_t rdata32;
		int32_t mv_hi;
		int32_t mv_lo;
		uint32_t rdata32_l;
		uint32_t mvx_L0_hi;
		uint32_t mvy_L0_hi;
		uint32_t mvx_L1_hi;
		uint32_t mvy_L1_hi;
		int64_t value;
		uint64_t temp_value;

		picture->max_mv = 0;
		picture->avg_mv = 0;
		picture->min_mv = 0;

		picture->max_skip = 0;
		picture->avg_skip = 0;
		picture->min_skip = 0;

		picture->max_qp = 0;
		picture->avg_qp = 0;
		picture->min_qp = 0;

		/* set rd_idx to 0 */
	    WRITE_VREG(HEVC_PIC_QUALITY_CTRL, 0);

	    blk88_y_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk88_y_count == 0) {
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* qp_y_sum */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		picture->avg_qp = rdata32/blk88_y_count;
		/* intra_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		/* skipped_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		picture->avg_skip = rdata32*100/blk88_y_count;
		/* coeff_non_zero_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		/* blk66_c_count */
	    blk88_c_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk88_c_count == 0) {
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* qp_c_sum */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		/* intra_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		/* skipped_cu_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		/* coeff_non_zero_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		/* 1'h0, qp_c_max[6:0], 1'h0, qp_c_min[6:0],
		1'h0, qp_y_max[6:0], 1'h0, qp_y_min[6:0] */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		picture->min_qp = (rdata32>>0)&0xff;
		picture->max_qp = (rdata32>>8)&0xff;
		/* blk22_mv_count */
	    blk22_mv_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk22_mv_count == 0) {
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* mvy_L1_count[39:32], mvx_L1_count[39:32],
		mvy_L0_count[39:32], mvx_L0_count[39:32] */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    /* should all be 0x00 or 0xff */
	    mvx_L0_hi = ((rdata32>>0)&0xff);
	    mvy_L0_hi = ((rdata32>>8)&0xff);
	    mvx_L1_hi = ((rdata32>>16)&0xff);
	    mvy_L1_hi = ((rdata32>>24)&0xff);

		/* mvx_L0_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvx_L0_hi;
		temp_value = (temp_value << 32) | rdata32_l;

		if (mvx_L0_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
		value = div_s64(value, blk22_mv_count);
		picture->avg_mv = value;

		/* mvy_L0_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvy_L0_hi;
		temp_value = (temp_value << 32) | rdata32_l;

		if (mvy_L0_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;

		/* mvx_L1_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvx_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvx_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;

		/* mvy_L1_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvy_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvy_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;

		/* {mvx_L0_max, mvx_L0_min} // format : {sign, abs[14:0]}  */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32>>16)&0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;

		picture->max_mv = mv_hi;

	    mv_lo = (rdata32>>0)&0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;
		picture->min_mv = mv_lo;

	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_CTRL);
		/* reset all counts */
	    WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
	}
}

static int hevc_slice_segment_header_process(struct hevc_state_s *hevc,
		union param_u *rpm_param,
		int decode_pic_begin)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct vdec_s *vdec = hw_to_vdec(hevc);
#endif
	int i;
	int lcu_x_num_div;
	int lcu_y_num_div;
	int Col_ref;
	int dbg_skip_flag = 0;
	struct aml_vcodec_ctx * ctx = hevc->v4l2_ctx;

	if (hevc->wait_buf == 0) {
		hevc->sps_num_reorder_pics_0 =
			rpm_param->p.sps_num_reorder_pics_0;
		hevc->ip_mode = (!hevc->sps_num_reorder_pics_0 &&
							!(vdec->slave || vdec->master) &&
							!disable_ip_mode) ? true : false;
		hevc->m_temporalId = rpm_param->p.m_temporalId;
		hevc->m_nalUnitType = rpm_param->p.m_nalUnitType;
		hevc->interlace_flag =
			(rpm_param->p.profile_etc >> 2) & 0x1;
		hevc->curr_pic_struct =
			(rpm_param->p.sei_frame_field_info >> 3) & 0xf;

		hevc->frame_field_info_present_flag =
			(rpm_param->p.sei_frame_field_info >> 8) & 0x1;

		if (hevc->frame_field_info_present_flag) {
			if (hevc->curr_pic_struct == 0
				|| hevc->curr_pic_struct == 7
				|| hevc->curr_pic_struct == 8)
				hevc->interlace_flag = 0;
		}

		hevc_print(hevc, H265_DEBUG_PIC_STRUCT,
			"frame_field_info_present_flag = %d curr_pic_struct = %d interlace_flag = %d\n",
			hevc->frame_field_info_present_flag,
			hevc->curr_pic_struct,
			hevc->interlace_flag);

		if (interlace_enable == 0)
			hevc->interlace_flag = 0;
		if (interlace_enable & 0x100)
			hevc->interlace_flag = interlace_enable & 0x1;
		if (hevc->interlace_flag == 0)
			hevc->curr_pic_struct = 0;

		hevc->misc_flag0 = rpm_param->p.misc_flag0;
		if (rpm_param->p.first_slice_segment_in_pic_flag == 0) {
			hevc->slice_segment_addr =
				rpm_param->p.slice_segment_address;
			if (!rpm_param->p.dependent_slice_segment_flag)
				hevc->slice_addr = hevc->slice_segment_addr;
		} else {
			hevc->slice_segment_addr = 0;
			hevc->slice_addr = 0;
		}

		hevc->iPrevPOC = hevc->curr_POC;
		hevc->slice_type = (rpm_param->p.slice_type == I_SLICE) ? 2 :
			(rpm_param->p.slice_type == P_SLICE) ? 1 :
			(rpm_param->p.slice_type == B_SLICE) ? 0 : 3;
		hevc->TMVPFlag = rpm_param->p.slice_temporal_mvp_enable_flag;
		hevc->isNextSliceSegment =
			rpm_param->p.dependent_slice_segment_flag ? 1 : 0;
		if (is_oversize_ex(rpm_param->p.pic_width_in_luma_samples,
				rpm_param->p.pic_height_in_luma_samples)) {
			hevc_print(hevc, 0, "over size : %u x %u.\n",
				rpm_param->p.pic_width_in_luma_samples, rpm_param->p.pic_height_in_luma_samples);
			if ((!hevc->m_ins_flag) &&
				((debug &
				H265_NO_CHANG_DEBUG_FLAG_IN_CODE) == 0))
				debug |= (H265_DEBUG_DIS_LOC_ERROR_PROC |
				H265_DEBUG_DIS_SYS_ERROR_PROC);
			vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
			return 3;
		}

		if (hevc->pic_w != rpm_param->p.pic_width_in_luma_samples
			|| hevc->pic_h !=
			rpm_param->p.pic_height_in_luma_samples) {
			hevc_print(hevc, 0,
				"Pic Width/Height Change (%d,%d)=>(%d,%d), interlace %d\n",
				   hevc->pic_w, hevc->pic_h,
				   rpm_param->p.pic_width_in_luma_samples,
				   rpm_param->p.pic_height_in_luma_samples,
				   hevc->interlace_flag);

			hevc->pic_w = rpm_param->p.pic_width_in_luma_samples;
			hevc->pic_h = rpm_param->p.pic_height_in_luma_samples;
			hevc->frame_width = hevc->pic_w;
			hevc->frame_height = hevc->pic_h;
#ifdef LOSLESS_COMPRESS_MODE
			if ((get_double_write_mode(hevc) & 0x10) == 0)
				init_decode_head_hw(hevc);
#endif
		}

		if (hevc->bit_depth_chroma > 10 ||
			hevc->bit_depth_luma > 10) {
			hevc_print(hevc, 0, "unsupport bitdepth : %u,%u\n",
				hevc->bit_depth_chroma,
				hevc->bit_depth_luma);
			if (!hevc->m_ins_flag)
				debug |= (H265_DEBUG_DIS_LOC_ERROR_PROC |
				H265_DEBUG_DIS_SYS_ERROR_PROC);
			hevc->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
			vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_UNSUPPORT);
			return 4;
		}

		/* it will cause divide 0 error */
		if (hevc->pic_w == 0 || hevc->pic_h == 0) {
			if (get_dbg_flag(hevc)) {
				hevc_print(hevc, 0,
					"Fatal Error, pic_w = %d, pic_h = %d\n",
					   hevc->pic_w, hevc->pic_h);
			}
			return 3;
		}
		pic_list_process(hevc);

		hevc->lcu_size =
			1 << (rpm_param->p.log2_min_coding_block_size_minus3 +
					3 + rpm_param->p.log2_diff_max_min_coding_block_size);
		if (hevc->lcu_size == 0) {
			hevc_print(hevc, 0,
				"Error, lcu_size = 0 (%d,%d)\n",
				   rpm_param->p.log2_min_coding_block_size_minus3,
				   rpm_param->p.log2_diff_max_min_coding_block_size);
			vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
			return 3;
		}
		hevc->lcu_size_log2 = log2i(hevc->lcu_size);
		lcu_x_num_div = (hevc->pic_w / hevc->lcu_size);
		lcu_y_num_div = (hevc->pic_h / hevc->lcu_size);
		hevc->lcu_x_num =
			((hevc->pic_w % hevc->lcu_size) ==
			 0) ? lcu_x_num_div : lcu_x_num_div + 1;
		hevc->lcu_y_num =
			((hevc->pic_h % hevc->lcu_size) ==
			 0) ? lcu_y_num_div : lcu_y_num_div + 1;
		hevc->lcu_total = hevc->lcu_x_num * hevc->lcu_y_num;

		if (hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR
			|| hevc->m_nalUnitType ==
			NAL_UNIT_CODED_SLICE_IDR_N_LP) {
			hevc->curr_POC = 0;
			if ((hevc->m_temporalId - 1) == 0)
				hevc->iPrevTid0POC = hevc->curr_POC;
		} else {
			int iMaxPOClsb =
				1 << (rpm_param->p.log2_max_pic_order_cnt_lsb_minus4 + 4);
			int iPrevPOClsb;
			int iPrevPOCmsb;
			int iPOCmsb;
			int iPOClsb = rpm_param->p.POClsb;

			if (iMaxPOClsb == 0) {
				hevc_print(hevc, 0,
					"error iMaxPOClsb is 0\n");
				return 3;
			}

			iPrevPOClsb = hevc->iPrevTid0POC % iMaxPOClsb;
			iPrevPOCmsb = hevc->iPrevTid0POC - iPrevPOClsb;

			if ((iPOClsb < iPrevPOClsb)
				&& ((iPrevPOClsb - iPOClsb) >= (iMaxPOClsb / 2)))
				iPOCmsb = iPrevPOCmsb + iMaxPOClsb;
			else if ((iPOClsb > iPrevPOClsb)
					 && ((iPOClsb - iPrevPOClsb) > (iMaxPOClsb / 2)))
				iPOCmsb = iPrevPOCmsb - iMaxPOClsb;
			else
				iPOCmsb = iPrevPOCmsb;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
				hevc_print(hevc, 0,
				"iPrePOC%d iMaxPOClsb%d iPOCmsb%d iPOClsb%d\n",
				 hevc->iPrevTid0POC, iMaxPOClsb, iPOCmsb,
				 iPOClsb);
			}
			if (hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA
				|| hevc->m_nalUnitType ==
				NAL_UNIT_CODED_SLICE_BLANT
				|| hevc->m_nalUnitType ==
				NAL_UNIT_CODED_SLICE_BLA_N_LP) {
				/* For BLA picture types, POCmsb is set to 0. */
				iPOCmsb = 0;
			}
			hevc->curr_POC = (iPOCmsb + iPOClsb);
			if ((hevc->m_temporalId - 1) == 0)
				hevc->iPrevTid0POC = hevc->curr_POC;
			else {
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print(hevc, 0,
						"m_temporalID is %d\n",
						   hevc->m_temporalId);
				}
			}
		}
		hevc->RefNum_L0 =
			(rpm_param->p.num_ref_idx_l0_active > MAX_REF_ACTIVE) ?
			MAX_REF_ACTIVE : rpm_param->p.num_ref_idx_l0_active;
		hevc->RefNum_L1 =
			(rpm_param->p.num_ref_idx_l1_active > MAX_REF_ACTIVE) ?
			MAX_REF_ACTIVE : rpm_param->p.num_ref_idx_l1_active;

		/* skip RASL pictures after CRA/BLA pictures */
		if (hevc->m_pocRandomAccess == MAX_INT) {/* first picture */
			if (hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_CRA ||
				hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA
				|| hevc->m_nalUnitType ==
				NAL_UNIT_CODED_SLICE_BLANT
				|| hevc->m_nalUnitType ==
				NAL_UNIT_CODED_SLICE_BLA_N_LP)
				hevc->m_pocRandomAccess = hevc->curr_POC;
			else
				hevc->m_pocRandomAccess = -MAX_INT;
		} else if (hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_BLA
				   || hevc->m_nalUnitType ==
				   NAL_UNIT_CODED_SLICE_BLANT
				   || hevc->m_nalUnitType ==
				   NAL_UNIT_CODED_SLICE_BLA_N_LP)
			hevc->m_pocRandomAccess = hevc->curr_POC;
		else if ((hevc->curr_POC < hevc->m_pocRandomAccess) &&
				(hevc->nal_skip_policy >= 3) &&
				 (hevc->m_nalUnitType ==
				  NAL_UNIT_CODED_SLICE_RASL_N ||
				  hevc->m_nalUnitType ==
				  NAL_UNIT_CODED_SLICE_TFD)) {	/* skip */
			if (get_dbg_flag(hevc)) {
				hevc_print(hevc, 0,
				"RASL picture with POC %d < %d ",
				 hevc->curr_POC, hevc->m_pocRandomAccess);
				hevc_print(hevc, 0,
					"RandomAccess point POC), skip it\n");
			}
			return 1;
			}

		WRITE_VREG(HEVC_WAIT_FLAG, READ_VREG(HEVC_WAIT_FLAG) | 0x2);
		hevc->skip_flag = 0;

		if (rpm_param->p.slice_segment_address == 0) {
			struct PIC_s *pic = NULL;

			hevc->new_pic = 1;
#ifdef MULTI_INSTANCE_SUPPORT
			if (!hevc->m_ins_flag)
#endif
				check_pic_decoded_error_pre(hevc,
					READ_VREG(HEVC_PARSER_LCU_START)
					& 0xffffff);
			if (vdec_stream_based(vdec) && ((READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff) != 0)) {
				if (hevc->cur_pic)
					hevc->cur_pic->error_mark = 1;
			}
			if (use_cma == 0) {
				if (hevc->pic_list_init_flag == 0) {
					init_pic_list(hevc);
					init_pic_list_hw(hevc);
					init_buf_spec(hevc);
					hevc->pic_list_init_flag = 3;
				}
			}
			if (!hevc->m_ins_flag) {
				if (hevc->cur_pic)
					get_picture_qos_info(hevc);
			}
			hevc->first_pic_after_recover = 0;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE)
				dump_pic_list(hevc);

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (vdec->master) {
				struct hevc_state_s *hevc_ba =
				(struct hevc_state_s *)
					vdec->master->private;
				if (hevc_ba->cur_pic != NULL) {
					hevc_ba->cur_pic->dv_enhance_exist = 1;
					hevc_print(hevc, H265_DEBUG_DV,
					"To decode el (poc %d) => set bl (poc %d) dv_enhance_exist flag\n",
					hevc->curr_POC, hevc_ba->cur_pic->POC);
				}
			}
			if (vdec->master == NULL &&
				vdec->slave == NULL)
				set_aux_data(hevc,
					hevc->cur_pic, 1, 0); /*suffix*/
			if (hevc->bypass_dvenl && !dolby_meta_with_el)
				set_aux_data(hevc,
					hevc->cur_pic, 0, 1); /*dv meta only*/
#else
			set_aux_data(hevc, hevc->cur_pic, 1, 0);
#endif

			/* prev pic */
			hevc_pre_pic(hevc, pic);
			/*
			 *update referenced of old pictures
			 *(cur_pic->referenced is 1 and not updated)
			 */
			apply_ref_pic_set(hevc, hevc->curr_POC,
							  rpm_param);
			/* new pic */
			hevc->cur_pic = v4l_get_new_pic(hevc, rpm_param);
			if (hevc->cur_pic == NULL) {
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE)
					dump_pic_list(hevc);
				hevc->wait_buf = 1;
				return -1;
			}
#ifdef MULTI_INSTANCE_SUPPORT
			hevc->decoding_pic = hevc->cur_pic;
			if (!hevc->m_ins_flag)
				hevc->over_decode = 0;
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			hevc->cur_pic->dv_enhance_exist = 0;
			if (vdec->slave)
				hevc_print(hevc, H265_DEBUG_DV,
				"Clear bl (poc %d) dv_enhance_exist flag\n",
				hevc->curr_POC);
			if (vdec->master == NULL &&
				vdec->slave == NULL)
				set_aux_data(hevc,
					hevc->cur_pic, 0, 0); /*prefix*/

			if (hevc->bypass_dvenl && !dolby_meta_with_el)
				set_aux_data(hevc,
					hevc->cur_pic, 0, 2); /*pre sei only*/
#else
			set_aux_data(hevc, hevc->cur_pic, 0, 0);
#endif
			if (get_dbg_flag(hevc) & H265_DEBUG_DISPLAY_CUR_FRAME) {
				hevc->cur_pic->output_ready = 1;
				hevc->cur_pic->stream_offset =
					READ_VREG(HEVC_SHIFT_BYTE_COUNT);
				prepare_display_buf(vdec, hevc->cur_pic);
				hevc->wait_buf = 2;
				return -1;
			}
		} else {
			if (get_dbg_flag(hevc) & H265_DEBUG_HAS_AUX_IN_SLICE) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
				if (vdec->master == NULL &&
					vdec->slave == NULL) {
					set_aux_data(hevc, hevc->cur_pic, 1, 0);
					set_aux_data(hevc, hevc->cur_pic, 0, 0);
				}
#else
				set_aux_data(hevc, hevc->cur_pic, 1, 0);
				set_aux_data(hevc, hevc->cur_pic, 0, 0);
#endif
			}
			if (hevc->pic_list_init_flag != 3
				|| hevc->cur_pic == NULL) {
				/* make it dec from the first slice segment */
				return 3;
			}
			hevc->cur_pic->slice_idx++;
			hevc->new_pic = 0;
		}
	} else {
	if (hevc->wait_buf == 1) {
			pic_list_process(hevc);

			hevc->cur_pic = v4l_get_new_pic(hevc, rpm_param);
			if (hevc->cur_pic == NULL)
				return -1;

			if (!hevc->m_ins_flag)
				hevc->over_decode = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			hevc->cur_pic->dv_enhance_exist = 0;
			if (vdec->master == NULL &&
				vdec->slave == NULL)
				set_aux_data(hevc, hevc->cur_pic, 0, 0);
#else
			set_aux_data(hevc, hevc->cur_pic, 0, 0);
#endif
			hevc->wait_buf = 0;
		} else if (hevc->wait_buf ==
				   2) {
			if (get_display_pic_num(hevc) >
				1)
				return -1;
			hevc->wait_buf = 0;
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE)
			dump_pic_list(hevc);
	}

	if (hevc->new_pic) {
#if 1
		/*SUPPORT_10BIT*/
		int sao_mem_unit =
				(hevc->lcu_size == 16 ? 9 :
						hevc->lcu_size ==
						32 ? 14 : 24) << 4;
#else
		int sao_mem_unit = ((hevc->lcu_size / 8) * 2 + 4) << 4;
#endif
		int pic_height_cu =
			(hevc->pic_h + hevc->lcu_size - 1) / hevc->lcu_size;
		int pic_width_cu =
			(hevc->pic_w + hevc->lcu_size - 1) / hevc->lcu_size;
		int sao_vb_size = (sao_mem_unit + (2 << 4)) * pic_height_cu;

		/* int sao_abv_size = sao_mem_unit*pic_width_cu; */
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"==>%s dec idx %d, struct %d interlace %d pic idx %d\n",
				__func__,
				hevc->decode_idx,
				hevc->curr_pic_struct,
				hevc->interlace_flag,
				hevc->cur_pic->index);
		}
		if (dbg_skip_decode_index != 0 &&
			hevc->decode_idx == dbg_skip_decode_index)
			dbg_skip_flag = 1;

		hevc->decode_idx++;
		update_tile_info(hevc, pic_width_cu, pic_height_cu,
						 sao_mem_unit, rpm_param);

		config_title_hw(hevc, sao_vb_size, sao_mem_unit);
	}

	if (hevc->iPrevPOC != hevc->curr_POC) {
		hevc->new_tile = 1;
		hevc->tile_x = 0;
		hevc->tile_y = 0;
		hevc->tile_y_x = 0;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"new_tile (new_pic) tile_x=%d, tile_y=%d\n",
				   hevc->tile_x, hevc->tile_y);
		}
	} else if (hevc->tile_enabled) {
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"slice_segment_address is %d\n",
				   rpm_param->p.slice_segment_address);
		}
		hevc->tile_y_x =
			get_tile_index(hevc, rpm_param->p.slice_segment_address,
						   (hevc->pic_w + hevc->lcu_size - 1) / hevc->lcu_size);
		if ((hevc->tile_y_x != (hevc->tile_x | (hevc->tile_y << 8)))
			&& (hevc->tile_y_x != -1)) {
			hevc->new_tile = 1;
			hevc->tile_x = hevc->tile_y_x & 0xff;
			hevc->tile_y = (hevc->tile_y_x >> 8) & 0xff;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
				hevc_print(hevc, 0,
				"new_tile seg adr %d tile_x=%d, tile_y=%d\n",
				 rpm_param->p.slice_segment_address,
				 hevc->tile_x, hevc->tile_y);
			}
		} else
			hevc->new_tile = 0;
	} else
		hevc->new_tile = 0;

	if ((hevc->tile_x > (MAX_TILE_COL_NUM - 1))
	|| (hevc->tile_y > (MAX_TILE_ROW_NUM - 1)))
		hevc->new_tile = 0;

	if (hevc->new_tile) {
		hevc->tile_start_lcu_x =
			hevc->m_tile[hevc->tile_y][hevc->tile_x].start_cu_x;
		hevc->tile_start_lcu_y =
			hevc->m_tile[hevc->tile_y][hevc->tile_x].start_cu_y;
		hevc->tile_width_lcu =
		    hevc->m_tile[hevc->tile_y][hevc->tile_x].width;
		hevc->tile_height_lcu =
			hevc->m_tile[hevc->tile_y][hevc->tile_x].height;
	}

	set_ref_pic_list(hevc, rpm_param);

	Col_ref = rpm_param->p.collocated_ref_idx;

	hevc->LDCFlag = 0;
	if (rpm_param->p.slice_type != I_SLICE) {
		hevc->LDCFlag = 1;
		for (i = 0; (i < hevc->RefNum_L0) && hevc->LDCFlag; i++) {
			if (hevc->cur_pic->m_aiRefPOCList0[hevc->cur_pic->slice_idx][i] >
				hevc->curr_POC)
				hevc->LDCFlag = 0;
		}
		if (rpm_param->p.slice_type == B_SLICE) {
			for (i = 0; (i < hevc->RefNum_L1)
					&& hevc->LDCFlag; i++) {
				if (hevc->cur_pic->m_aiRefPOCList1[hevc->cur_pic->slice_idx][i] >
					hevc->curr_POC)
					hevc->LDCFlag = 0;
			}
		}
	}

	hevc->ColFromL0Flag = rpm_param->p.collocated_from_l0_flag;

	hevc->plevel =
		rpm_param->p.log2_parallel_merge_level;
	hevc->MaxNumMergeCand = 5 - rpm_param->p.five_minus_max_num_merge_cand;

	hevc->LongTerm_Curr = 0;	/* to do ... */
	hevc->LongTerm_Col = 0;	/* to do ... */

	hevc->list_no = 0;
	if (rpm_param->p.slice_type == B_SLICE)
		hevc->list_no = 1 - hevc->ColFromL0Flag;
	if (hevc->list_no == 0) {
		if (Col_ref < hevc->RefNum_L0) {
			hevc->Col_POC =
				hevc->cur_pic->m_aiRefPOCList0[hevc->cur_pic->slice_idx][Col_ref];
		} else
			hevc->Col_POC = INVALID_POC;
	} else {
		if (Col_ref < hevc->RefNum_L1) {
			hevc->Col_POC =
				hevc->cur_pic->m_aiRefPOCList1[hevc->cur_pic->slice_idx][Col_ref];
		} else
			hevc->Col_POC = INVALID_POC;
	}

	hevc->LongTerm_Ref = 0;	/* to do ... */

	if (hevc->slice_type != 2) {
		if (hevc->Col_POC != INVALID_POC) {
			hevc->col_pic = get_ref_pic_by_POC(hevc, hevc->Col_POC);
			if (hevc->col_pic == NULL) {
				hevc->cur_pic->error_mark = 1;
				if (get_dbg_flag(hevc)) {
					hevc_print(hevc, 0,
					"WRONG,fail to get the pic Col_POC\n");
				}
				if (is_log_enable(hevc))
					add_log(hevc,
					"WRONG,fail to get the pic Col_POC");
			} else if (hevc->col_pic->error_mark || hevc->col_pic->dis_mark == 0) {
				hevc->col_pic->error_mark = 1;
				hevc->cur_pic->error_mark = 1;
				if (get_dbg_flag(hevc)) {
					hevc_print(hevc, 0,
					"WRONG, Col_POC error_mark is 1\n");
				}
				if (is_log_enable(hevc))
					add_log(hevc,
					"WRONG, Col_POC error_mark is 1");
			} else {
				if ((hevc->col_pic->width
					!= hevc->pic_w) ||
					(hevc->col_pic->height
					!= hevc->pic_h)) {
					hevc_print(hevc, 0,
						"Wrong reference pic (poc %d) width/height %d/%d\n",
						hevc->col_pic->POC,
						hevc->col_pic->width,
						hevc->col_pic->height);
					hevc->cur_pic->error_mark = 1;
				}

			}

			if (hevc->cur_pic->error_mark
				&& ((hevc->ignore_bufmgr_error & 0x1) == 0)) {
				/*count info*/
				vdec_count_info(hevc->gvs, hevc->cur_pic->error_mark,
					hevc->cur_pic->stream_offset);
				if (hevc->cur_pic->slice_type == I_SLICE) {
					hevc->gvs->i_decoded_frames++;
				} else if (hevc->cur_pic->slice_type == P_SLICE) {
					hevc->gvs->p_decoded_frames++;
				} else if (hevc->cur_pic->slice_type == B_SLICE) {
					hevc->gvs->b_decoded_frames++;
				}
				if (hevc->cur_pic->error_mark) {
					if (hevc->cur_pic->slice_type == I_SLICE) {
						hevc->gvs->i_concealed_frames++;
					} else if (hevc->cur_pic->slice_type == P_SLICE) {
						hevc->gvs->p_concealed_frames++;
					} else if (hevc->cur_pic->slice_type == B_SLICE) {
						hevc->gvs->b_concealed_frames++;
					}
				}
				if (hevc->PB_skip_mode == 2) {
					hevc->gvs->drop_frame_count++;
					if (rpm_param->p.slice_type == I_SLICE) {
						hevc->gvs->i_lost_frames++;
					} else if (rpm_param->p.slice_type == P_SLICE) {
						hevc->gvs->p_lost_frames++;
					} else if (rpm_param->p.slice_type == B_SLICE) {
						hevc->gvs->b_lost_frames++;
					}
				}
			}
			if (is_skip_decoding(hevc, hevc->cur_pic)) {
				return 2;
			}
		} else
			hevc->col_pic = hevc->cur_pic;
	}			/*  */
	if (hevc->col_pic == NULL)
		hevc->col_pic = hevc->cur_pic;
#ifdef BUFFER_MGR_ONLY
	return 0xf;
#else
	if ((decode_pic_begin > 0 && hevc->decode_idx <= decode_pic_begin)
		|| (dbg_skip_flag))
		return 0xf;
#endif
	ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_REGISTER_START);
	config_mc_buffer(hevc, hevc->cur_pic);

	if (is_skip_decoding(hevc,
			hevc->cur_pic)) {
		if (get_dbg_flag(hevc))
			hevc_print(hevc, 0,
				"Discard this picture index %d\n",
				hevc->cur_pic->index);
		/*count info*/
		vdec_count_info(hevc->gvs, hevc->cur_pic->error_mark,
			hevc->cur_pic->stream_offset);
		if (hevc->cur_pic->slice_type == I_SLICE) {
			hevc->gvs->i_decoded_frames++;
		} else if (hevc->cur_pic->slice_type == P_SLICE) {
			hevc->gvs->p_decoded_frames++;
		} else if (hevc->cur_pic->slice_type == B_SLICE) {
			hevc->gvs->b_decoded_frames++;
		}
		if (hevc->cur_pic->error_mark) {
			if (hevc->cur_pic->slice_type == I_SLICE) {
				hevc->gvs->i_concealed_frames++;
			} else if (hevc->cur_pic->slice_type == P_SLICE) {
				hevc->gvs->p_concealed_frames++;
			} else if (hevc->cur_pic->slice_type == B_SLICE) {
				hevc->gvs->b_concealed_frames++;
			}
		}
		if (hevc->PB_skip_mode == 2) {
			hevc->gvs->drop_frame_count++;
			if (rpm_param->p.slice_type == I_SLICE) {
				hevc->gvs->i_lost_frames++;
			} else if (rpm_param->p.slice_type == P_SLICE) {
				hevc->gvs->p_lost_frames++;
			} else if (rpm_param->p.slice_type == B_SLICE) {
				hevc->gvs->b_lost_frames++;
			}
		}
		return 2;
	}
#ifdef MCRCC_ENABLE
	config_mcrcc_axi_hw(hevc, hevc->cur_pic->slice_type);
#endif
	if (!hevc->tile_width_lcu || !hevc->tile_height_lcu)
		return -1;
	config_mpred_hw(hevc);

	config_sao_hw(hevc, rpm_param);
	ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_REGISTER_START);
	if ((hevc->slice_type != 2) && (hevc->i_only & 0x2))
		return 0xf;

	if (post_picture_early(vdec, hevc->cur_pic->index))
		return -1;

	return 0;
}

/* return page number */
static int hevc_mmu_page_num(struct hevc_state_s *hevc,
		int w, int h, int save_mode)
{
	int picture_size;
	int page_num;
	int max_frame_num;

	picture_size =
		compute_losless_comp_body_size(hevc, w, h, save_mode);
	page_num = ((picture_size + PAGE_SIZE - 1) >> PAGE_SHIFT);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
		max_frame_num = MAX_FRAME_8K_NUM;
	else
		max_frame_num = MAX_FRAME_4K_NUM;

	if (page_num > max_frame_num) {
		hevc_print(hevc, 0, "over max !! 0x%x width %d height %d\n",
			page_num, w, h);
		return -1;
	}
	return page_num;
}

static int H265_alloc_mmu(struct hevc_state_s *hevc, struct PIC_s *new_pic,
		unsigned short bit_depth, unsigned int *mmu_index_adr) {
	int bit_depth_10 = (bit_depth != 0x00);
	int cur_mmu_4k_number;
	int ret;
	struct internal_comp_buf *ibuf =
		index_to_icomp_buf(hevc, new_pic->BUF_index);
	if (get_double_write_mode(hevc) == 0x10)
		return 0;

	cur_mmu_4k_number = hevc_mmu_page_num(hevc, new_pic->width,
			new_pic->height, !bit_depth_10);
	if (cur_mmu_4k_number < 0)
		return -1;
	ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_MEMORY_START);

	ret = decoder_mmu_box_alloc_idx(
			ibuf->mmu_box,
			ibuf->index,
			ibuf->frame_buffer_size,
			mmu_index_adr);

	ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_MEMORY_END);
	new_pic->scatter_alloc = 1;

	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
		"%s pic index %d page count(%d) ret =%d\n",
		__func__, new_pic->index,
		cur_mmu_4k_number, ret);
	return ret;
}


static void release_pic_mmu_buf(struct hevc_state_s *hevc,
	struct PIC_s *pic)
{
	hevc_print(hevc, H265_DEBUG_BUFMGR_MORE,
	"%s pic index %d scatter_alloc %d\n",
	__func__, pic->index,
	pic->scatter_alloc);

	if (hevc->mmu_enable
		&& !(get_double_write_mode(hevc) & 0x10)
		&& pic->scatter_alloc) {
		struct internal_comp_buf *ibuf =
			ibuf = index_to_icomp_buf(hevc, pic->BUF_index);
		decoder_mmu_box_free_idx(ibuf->mmu_box, ibuf->index);
	}
	pic->scatter_alloc = 0;
}

/*
 *************************************************
 *
 *h265 buffer management end
 *
 **************************************************
 */
static struct hevc_state_s *gHevc;

static void hevc_local_uninit(struct hevc_state_s *hevc)
{
	hevc->rpm_ptr = NULL;
	hevc->lmem_ptr = NULL;

#ifdef SWAP_HEVC_UCODE
	if (hevc->is_swap) {
		if (hevc->mc_cpu_addr != NULL) {
			decoder_dma_free_coherent(hevc->mc_cpu_handle,
				hevc->swap_size, hevc->mc_cpu_addr,
				hevc->mc_dma_handle);
				hevc->mc_cpu_addr = NULL;
		}

	}
#endif
#ifdef DETREFILL_ENABLE
	if (hevc->is_swap && get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM)
		uninit_detrefill_buf(hevc);
#endif
	if (hevc->aux_addr) {
		decoder_dma_free_coherent(hevc->aux_mem_handle,
				hevc->prefix_aux_size + hevc->suffix_aux_size, hevc->aux_addr,
					hevc->aux_phy_addr);
		hevc->aux_addr = NULL;
	}
	if (hevc->rpm_addr) {
		decoder_dma_free_coherent(hevc->rpm_mem_handle,
				RPM_BUF_SIZE, hevc->rpm_addr,
					hevc->rpm_phy_addr);
		hevc->rpm_addr = NULL;
	}
	if (hevc->lmem_addr) {
		decoder_dma_free_coherent(hevc->lmem_phy_handle,
				RPM_BUF_SIZE, hevc->lmem_addr,
					hevc->lmem_phy_addr);
		hevc->lmem_addr = NULL;
	}

	if (hevc->mmu_enable && hevc->frame_mmu_map_addr) {
		if (hevc->frame_mmu_map_phy_addr)
			decoder_dma_free_coherent(hevc->frame_mmu_map_handle,
				get_frame_mmu_map_size(), hevc->frame_mmu_map_addr,
					hevc->frame_mmu_map_phy_addr);

		hevc->frame_mmu_map_addr = NULL;
	}
}

static int hevc_local_init(struct hevc_state_s *hevc)
{
	int ret = -1;
	struct BuffInfo_s *cur_buf_info = NULL;

	memset(&hevc->param, 0, sizeof(union param_u));

	cur_buf_info = &hevc->work_space_buf_store;
	if (force_bufspec) {
		memcpy(cur_buf_info, &amvh265_workbuff_spec[force_bufspec & 0xf],
		sizeof(struct BuffInfo_s));
		pr_info("force buffer spec %d\n", force_bufspec & 0xf);
	} else {
		if (get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_TM2 && !is_cpu_tm2_revb()) {
			if (vdec_is_support_4k()) {
				if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
					memcpy(cur_buf_info, &amvh265_workbuff_spec[2],	/* 4k */
					sizeof(struct BuffInfo_s));
				else
					memcpy(cur_buf_info, &amvh265_workbuff_spec[1],	/* 4k */
					sizeof(struct BuffInfo_s));
			} else {
				memcpy(cur_buf_info, &amvh265_workbuff_spec[0],	/* 1080p */
				sizeof(struct BuffInfo_s));
			}
		} else { //get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_TM2 || is_cpu_tm2_revb()
			if (vdec_is_support_4k()) {
				memcpy(cur_buf_info, &amvh265_workbuff_spec[5],	/* 4k */
				sizeof(struct BuffInfo_s));
			} else {
				memcpy(cur_buf_info, &amvh265_workbuff_spec[3],	/* 1080p */
				sizeof(struct BuffInfo_s));
			}
		}
	}

	cur_buf_info->start_adr = hevc->buf_start;
	init_buff_spec(hevc, cur_buf_info);

	hevc_init_stru(hevc, cur_buf_info);

	hevc->bit_depth_luma = 8;
	hevc->bit_depth_chroma = 8;
	hevc->video_signal_type = 0;
	hevc->video_signal_type_debug = 0;
	bit_depth_luma = hevc->bit_depth_luma;
	bit_depth_chroma = hevc->bit_depth_chroma;
	video_signal_type = hevc->video_signal_type;

	if ((get_dbg_flag(hevc) & H265_DEBUG_SEND_PARAM_WITH_REG) == 0) {
		hevc->rpm_addr = decoder_dma_alloc_coherent(&hevc->rpm_mem_handle,
				RPM_BUF_SIZE, &hevc->rpm_phy_addr, "H265_RPM_BUF");
		if (hevc->rpm_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}
		hevc->rpm_ptr = hevc->rpm_addr;
	}

	if (prefix_aux_buf_size > 0 ||
		suffix_aux_buf_size > 0) {
		u32 aux_buf_size;

		hevc->prefix_aux_size = AUX_BUF_ALIGN(prefix_aux_buf_size);
		hevc->suffix_aux_size = AUX_BUF_ALIGN(suffix_aux_buf_size);
		aux_buf_size = hevc->prefix_aux_size + hevc->suffix_aux_size;
		hevc->aux_addr =decoder_dma_alloc_coherent(&hevc->aux_mem_handle,
				aux_buf_size, &hevc->aux_phy_addr, "H265_AUX_BUF");
		if (hevc->aux_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}
	}

	hevc->lmem_addr = decoder_dma_alloc_coherent(&hevc->lmem_phy_handle,
				LMEM_BUF_SIZE, &hevc->lmem_phy_addr, "H265_LMEM_BUF");
	if (hevc->lmem_addr == NULL) {
		pr_err("%s: failed to alloc lmem buffer\n", __func__);
		return -1;
	}
	hevc->lmem_ptr = hevc->lmem_addr;

	if (hevc->mmu_enable) {
		hevc->frame_mmu_map_addr =
				decoder_dma_alloc_coherent(&hevc->frame_mmu_map_handle,
				get_frame_mmu_map_size(),
				&hevc->frame_mmu_map_phy_addr, "H265_MMU_BUF");
		if (hevc->frame_mmu_map_addr == NULL) {
			pr_err("%s: failed to alloc count_buffer\n", __func__);
			return -1;
		}
		memset(hevc->frame_mmu_map_addr, 0, get_frame_mmu_map_size());
	}
	ret = 0;
	return ret;
}

/*
 *******************************************
 *  Mailbox command
 *******************************************
 */
#define CMD_FINISHED               0
#define CMD_ALLOC_VIEW             1
#define CMD_FRAME_DISPLAY          3
#define CMD_DEBUG                  10

#define DECODE_BUFFER_NUM_MAX    32
#define DISPLAY_BUFFER_NUM       6

#define video_domain_addr(adr) (adr&0x7fffffff)
#define DECODER_WORK_SPACE_SIZE 0x800000

#define spec2canvas(x)  \
	(((x)->uv_canvas_index << 16) | \
	 ((x)->uv_canvas_index << 8)  | \
	 ((x)->y_canvas_index << 0))

static void set_canvas(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);
	int canvas_w = ALIGN(pic->width, 64)/4;
	int canvas_h = ALIGN(pic->height, 32)/4;
	int blkmode = hevc->mem_map_mode;

	/*CANVAS_BLKMODE_64X32*/
#ifdef SUPPORT_10BIT
	if	(pic->double_write_mode) {
		canvas_w = pic->width /
			get_double_write_ratio(pic->double_write_mode);
		canvas_h = pic->height /
			get_double_write_ratio(pic->double_write_mode);

		canvas_w = ALIGN(canvas_w, 64);
		canvas_h = ALIGN(canvas_h, 32);

		if (vdec->parallel_dec == 1) {
			if (pic->y_canvas_index == -1)
				pic->y_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
			if (pic->uv_canvas_index == -1)
				pic->uv_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
		} else {
			pic->y_canvas_index = 128 + pic->index * 2;
			pic->uv_canvas_index = 128 + pic->index * 2 + 1;
		}

		config_cav_lut_ex(pic->y_canvas_index,
			pic->dw_y_adr, canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0, VDEC_HEVC);
		config_cav_lut_ex(pic->uv_canvas_index, pic->dw_u_v_adr,
			canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0, VDEC_HEVC);
#ifdef MULTI_INSTANCE_SUPPORT
		pic->canvas_config[0].phy_addr =
				pic->dw_y_adr;
		pic->canvas_config[0].width =
				canvas_w;
		pic->canvas_config[0].height =
				canvas_h;
		pic->canvas_config[0].block_mode =
				blkmode;
		pic->canvas_config[0].endian = 0;

		pic->canvas_config[1].phy_addr =
				pic->dw_u_v_adr;
		pic->canvas_config[1].width =
				canvas_w;
		pic->canvas_config[1].height =
				canvas_h;
		pic->canvas_config[1].block_mode =
				blkmode;
		pic->canvas_config[1].endian = 0;

		ATRACE_COUNTER(hevc->trace.set_canvas0_addr, pic->canvas_config[0].phy_addr);
		hevc_print(hevc, H265_DEBUG_PIC_STRUCT,"%s(canvas0 addr:0x%x)\n",
			__func__, pic->canvas_config[0].phy_addr);
#else
		ATRACE_COUNTER(hevc->trace.set_canvas0_addr, spec2canvas(pic));
		hevc_print(hevc, H265_DEBUG_PIC_STRUCT,"%s(canvas0 addr:0x%x)\n",
			__func__, spec2canvas(pic));
#endif
	} else {
		if (!hevc->mmu_enable) {
			/* to change after 10bit VPU is ready ... */
			if (vdec->parallel_dec == 1) {
				if (pic->y_canvas_index == -1)
					pic->y_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
				pic->uv_canvas_index = pic->y_canvas_index;
			} else {
				pic->y_canvas_index = 128 + pic->index;
				pic->uv_canvas_index = 128 + pic->index;
			}

			config_cav_lut_ex(pic->y_canvas_index,
				pic->mc_y_adr, canvas_w, canvas_h,
				CANVAS_ADDR_NOWRAP, blkmode, 0, VDEC_HEVC);
			config_cav_lut_ex(pic->uv_canvas_index, pic->mc_u_v_adr,
				canvas_w, canvas_h,
				CANVAS_ADDR_NOWRAP, blkmode, 0, VDEC_HEVC);
		}
		ATRACE_COUNTER(hevc->trace.set_canvas0_addr, spec2canvas(pic));
		hevc_print(hevc, H265_DEBUG_PIC_STRUCT,"%s(canvas0 addr:0x%x)\n",
			__func__, spec2canvas(pic));
	}
#else
	if (vdec->parallel_dec == 1) {
		if (pic->y_canvas_index == -1)
			pic->y_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
		if (pic->uv_canvas_index == -1)
			pic->uv_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
	} else {
		pic->y_canvas_index = 128 + pic->index * 2;
		pic->uv_canvas_index = 128 + pic->index * 2 + 1;
	}


	config_cav_lut_ex(pic->y_canvas_index, pic->mc_y_adr, canvas_w, canvas_h,
		CANVAS_ADDR_NOWRAP, blkmode, 0, VDEC_HEVC);
	config_cav_lut_ex(pic->uv_canvas_index, pic->mc_u_v_adr,
		canvas_w, canvas_h,
		CANVAS_ADDR_NOWRAP, blkmode, 0, VDEC_HEVC);

	ATRACE_COUNTER(hevc->trace.set_canvas0_addr, spec2canvas(pic));
	hevc_print(hevc, H265_DEBUG_PIC_STRUCT,"%s(canvas0 addr:0x%x)\n",
		__func__, spec2canvas(pic));
#endif
}

static int init_buf_spec(struct hevc_state_s *hevc)
{
	int pic_width = hevc->pic_w;
	int pic_height = hevc->pic_h;

	hevc_print(hevc, 0,
		"%s2 %d %d\n", __func__, pic_width, pic_height);

	if (hevc->frame_width == 0 || hevc->frame_height == 0) {
		hevc->frame_width = pic_width;
		hevc->frame_height = pic_height;

	}

	return 0;
}

#ifdef H265_USERDATA_ENABLE

static void vh265_destroy_userdata_manager(struct hevc_state_s *hevc)
{
	if (hevc) {
		memset(&hevc->userdata_info, 0,
			sizeof(struct h265_userdata_info_t));
	}
}

static void vh265_reset_udr_mgr(struct hevc_state_s *hevc)
{
	hevc->wait_for_udr_send = 0;
	hevc->sei_itu_data_len = 0;
	memset(&hevc->ud_record, 0, sizeof(hevc->ud_record));
}

static int vh265_crate_userdata_manager(struct hevc_state_s *hevc,
	char *userdata_buf, u32 buflen)
{
	if (hevc == NULL)
		return -1;

	mutex_init(&hevc->userdata_mutex);

	memset(&hevc->userdata_info, 0, sizeof(struct h265_userdata_info_t));
	hevc->userdata_info.data_buf = userdata_buf;
	hevc->userdata_info.buf_len = buflen;
	hevc->userdata_info.data_buf_end = userdata_buf + buflen;

	vh265_reset_udr_mgr(hevc);

	return 0;
}

static int vh265_user_data_read(struct vdec_s *vdec,
			struct userdata_param_t *puserdata_para)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
	int rec_ri, rec_wi;
	struct h265_userdata_record_t *rec;
	int rec_len;
	void *rec_start;
	void *dest_buf;
	unsigned long res;
	u32 data_size;
	int copy_ok = 0;

#if 0
	uint32_t version;
	uint32_t instance_id; /*input, 0~9*/
	uint32_t buf_len; /*input*/
	uint32_t data_size; /*output*/
	void *pbuf_addr; /*input*/
	struct userdata_meta_info_t meta_info; /*output*/
#endif

	mutex_lock(&hevc->userdata_mutex);
	rec_ri = hevc->userdata_info.read_index;
	rec_wi = hevc->userdata_info.write_index;

	if (rec_ri == rec_wi) {
		mutex_unlock(&hevc->userdata_mutex);
		return 0;
	}

	rec = &hevc->userdata_info.records[rec_ri];
	rec_start = hevc->userdata_info.data_buf + rec->rec_start;
	rec_len = rec->rec_len;

	dest_buf = puserdata_para->pbuf_addr;
	data_size = rec_len;


	/* pr_info("%s, ready to copy. size 0x%x, bufsize 0x%x, start %p, end %p\n",
		__func__, data_size, puserdata_para->buf_len, rec_start, hevc->userdata_info.data_buf_end);
	*/
	if (rec_len <= puserdata_para->buf_len) {
		if ((u8 *)(rec_start + rec_len) > hevc->userdata_info.data_buf_end) {
			u32 first_len = hevc->userdata_info.data_buf_end - (u8 *)rec_start;

			res = copy_to_user(dest_buf, rec_start, first_len);
			copy_ok = 1;
			if (res) {
				pr_info("%s[1], res %ld, request %d\n", __func__, res, first_len);
				copy_ok = 0;
				rec->rec_len -= (first_len - res);
				rec->rec_start += (first_len - res);
				puserdata_para->data_size += (first_len - res);
			} else {
				res = copy_to_user(dest_buf + first_len,
					hevc->userdata_info.data_buf, data_size - first_len);
				if (res) {
					pr_info("%s[2], res %ld, request %d\n", __func__, res, data_size);
					copy_ok = 0;
				}
				rec->rec_len -= (data_size - res);
				rec->rec_start = (data_size - first_len - res);
				puserdata_para->data_size = (data_size - res);
			}
		} else {
			res = copy_to_user(dest_buf, rec_start, data_size);
			if (res) {
				pr_info("%s[3], res %ld, request %d\n", __func__, res, data_size);
				copy_ok = 0;
			}
			rec->rec_len -= (data_size - res);
			rec->rec_start += (data_size - res);
			puserdata_para->data_size = (data_size - res);
			copy_ok = 1;

		}

		if (copy_ok) {
			hevc->userdata_info.read_index++;
			if (hevc->userdata_info.read_index >= USERDATA_FIFO_NUM)
				hevc->userdata_info.read_index = 0;
		}
	} else {
		res = (u32)copy_to_user(dest_buf,
							(void *)rec_start,
							data_size);
		if (res) {
			pr_info("%s[4], res %ld, request %d\n",
				__func__, res, data_size);
			copy_ok = 0;
		}

		rec->rec_len -= data_size - res;
		rec->rec_start += data_size - res;
		puserdata_para->data_size = data_size - res;
	}

	puserdata_para->meta_info = rec->meta_info;

	if (hevc->userdata_info.read_index <= hevc->userdata_info.write_index)
		puserdata_para->meta_info.records_in_que =
			hevc->userdata_info.write_index -
			hevc->userdata_info.read_index;
	else
		puserdata_para->meta_info.records_in_que =
			hevc->userdata_info.write_index +
			USERDATA_FIFO_NUM - hevc->userdata_info.read_index;

	puserdata_para->version = (0<<24|0<<16|0<<8|1);

	mutex_unlock(&hevc->userdata_mutex);

	return 1;
}

void vh265_reset_userdata_fifo(struct vdec_s *vdec, int bInit)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;

	if (hevc) {
		mutex_lock(&hevc->userdata_mutex);
		pr_info("%s, bInit %d, ri %d, wi %d\n", __func__,
			bInit, hevc->userdata_info.read_index,
			hevc->userdata_info.write_index);

		hevc->userdata_info.read_index = 0;
		hevc->userdata_info.write_index = 0;

		if (bInit)
			hevc->userdata_info.last_wp = 0;
		mutex_unlock(&hevc->userdata_mutex);
	}
}

static void vh265_wakeup_userdata_poll(struct vdec_s *vdec)
{
	amstream_wakeup_userdata_poll(vdec);
}

static void vh265_userdata_fill_vpts(struct hevc_state_s *hevc,
	u32 vpts, int pts_valid, u32 poc)
{
	u8 *pdata;
	u8 *pmax_sei_data_buffer;
	u8 *sei_data_buf;
	int i;
	int wp;
	int data_length;
	struct h265_userdata_record_t *p_rec;
	struct userdata_meta_info_t meta_info;

	if (hevc->sei_itu_data_len <= 0)
		return;
	sei_data_buf = hevc->sei_itu_data_buf;
	pdata = hevc->sei_user_data_buffer + hevc->sei_user_data_wp;
	pmax_sei_data_buffer = hevc->sei_user_data_buffer + USER_DATA_SIZE;
	memset(&meta_info, 0, sizeof(meta_info));

	for (i = 0; i < hevc->sei_itu_data_len; i++) {
		*pdata++ = sei_data_buf[i];
		if (pdata >= pmax_sei_data_buffer)
			pdata = hevc->sei_user_data_buffer;
	}

	hevc->sei_user_data_wp = (hevc->sei_user_data_wp
		+ hevc->sei_itu_data_len) % USER_DATA_SIZE;
	hevc->sei_itu_data_len = 0;

	meta_info.duration = hevc->frame_dur;
	meta_info.flags |= (VFORMAT_HEVC << 3);
	meta_info.flags |= (hevc->cur_pic->pic_struct << 12);
	meta_info.vpts = vpts;
	meta_info.vpts_valid = pts_valid;
	meta_info.poc_number = poc;

	/*pr_info("one record ready pts %d, poc %d\n", vpts, meta_info.poc_number);*/
	wp = hevc->sei_user_data_wp;
	if (hevc->sei_user_data_wp > hevc->userdata_info.last_wp)
		data_length = wp - hevc->userdata_info.last_wp;
	else
		data_length = wp + hevc->userdata_info.buf_len
			- hevc->userdata_info.last_wp;

	if (data_length & 0x7)
		data_length = (((data_length + 8) >> 3) << 3);

	p_rec = &hevc->ud_record;
	p_rec->meta_info = meta_info;
	p_rec->rec_start = hevc->userdata_info.last_wp;
	p_rec->rec_len = data_length;
	hevc->userdata_info.last_wp = wp;

	hevc->wait_for_udr_send = 1;

	/* notify userdata ready */
	mutex_lock(&hevc->userdata_mutex);
	hevc->userdata_info.records[hevc->userdata_info.write_index]
		= hevc->ud_record;
	hevc->userdata_info.write_index++;
	if (hevc->userdata_info.write_index >= USERDATA_FIFO_NUM)
		hevc->userdata_info.write_index = 0;
	mutex_unlock(&hevc->userdata_mutex);

	vdec_wakeup_userdata_poll(hw_to_vdec(hevc));
	hevc->wait_for_udr_send = 0;
}

#endif

#ifdef H265_USERDATA_ENABLE
static int check_hevc_cc_type(char *p_sei)
{
	return (p_sei[0] == 0xB5 && p_sei[1] == 0x00 && p_sei[2] == 0x31 && p_sei[3] == 0x47
					&& p_sei[4] == 0x41 && p_sei[5] == 0x39 && p_sei[6] == 0x34)
					|| (p_sei[0] == 0xB5 && p_sei[1] == 0x00 && p_sei[2] == 0x31
					&& p_sei[3] == 0x44 && p_sei[4] == 0x54 && p_sei[5] == 0x47 && p_sei[6] == 0x31);
}
#endif
static int parse_sei(struct hevc_state_s *hevc,
	struct PIC_s *pic, char *sei_buf, uint32_t size)
{
	char *p = sei_buf;
	char *p_sei;
	uint16_t header;
	uint16_t nal_unit_type;
	uint16_t payload_type, payload_size;
	int i, j;
	int data_len;
	u8 *user_data_buf;

	if (size < 2)
		return 0;
	header = *p++;
	header <<= 8;
	header += *p++;
	nal_unit_type = header >> 9;
	if ((nal_unit_type != NAL_UNIT_SEI)
	&& (nal_unit_type != NAL_UNIT_SEI_SUFFIX))
		return 0;
	while (p+4 <= sei_buf+size) {
		payload_type = *p++;
		if (payload_type == 0xff) {
			payload_type += *p++;
		}
		payload_size = *p++;
		if (payload_size == 0xff) {
			payload_size += *p++;
		}

		if (p+payload_size <= sei_buf+size) {
			switch (payload_type) {
			case SEI_PicTiming:
				if ((parser_sei_enable & 0x4) &&
					hevc->frame_field_info_present_flag) {
					p_sei = p;
					hevc->curr_pic_struct = (*p_sei >> 4)&0x0f;
					pic->pic_struct = hevc->curr_pic_struct;
					if (get_dbg_flag(hevc) &
						H265_DEBUG_PIC_STRUCT) {
						hevc_print(hevc, 0,
						"parse result pic_struct = %d\n",
						hevc->curr_pic_struct);
					}
				}
				break;
			case SEI_UserDataITU_T_T35:
				p_sei = p;
				if (p_sei[0] == 0xB5
					&& p_sei[1] == 0x00
					&& p_sei[2] == 0x3C
					&& p_sei[3] == 0x00
					&& p_sei[4] == 0x01
					&& p_sei[5] == 0x04) {
					char *new_buf;
					hevc->sei_present_flag |= SEI_HDR10PLUS_MASK;
					new_buf = vzalloc(payload_size);
					if (new_buf) {
						memcpy(new_buf, p_sei, payload_size);
						pic->hdr10p_data_buf = new_buf;
						pic->hdr10p_data_size = payload_size;
					} else {
						hevc_print(hevc, 0,
							"%s:hdr10p data vzalloc size(%d) fail\n",
							__func__, payload_size);
						pic->hdr10p_data_buf = NULL;
						pic->hdr10p_data_size = 0;
					}
				} else if (p_sei[0] == 0x26
					&& p_sei[1] == 0x00
					&& p_sei[2] == 0x04
					&& p_sei[3] == 0x00
					&& p_sei[4] == 0x05) {
					hevc->sei_present_flag |= SEI_HDR_CUVA_MASK;

					if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI) {
						PR_INIT(128);
						hevc_print(hevc, 0,
							"hdr cuva data: (size %d)\n",
							payload_size);
						for (i = 0; i < payload_size; i++) {
							PR_FILL("%02x ", p_sei[i]);
							if (((i + 1) & 0xf) == 0)
								PR_INFO(hevc->index);
						}
						PR_INFO(hevc->index);
					}
#ifndef H265_USERDATA_ENABLE
				}
#else
				} else if (check_hevc_cc_type(p_sei)) {
					user_data_buf = hevc->sei_itu_data_buf
						+ hevc->sei_itu_data_len;
					/* user data length should be align with 8 bytes,
						if not, then padding with zero*/
					for (i = 0; i < payload_size; i += 8) {
						for (j = 0; j < 8; j++) {
							int index;
							index = i+7-j;
							if (index >= payload_size)
								user_data_buf[i+j] = 0;
							else
								user_data_buf[i+j] = p_sei[i+7-j];
							/*pr_info("%x ", p_sei[i+j]);*/
						}
					}
					/*pr_info("\n");*/
					data_len = payload_size;
					if (payload_size % 8)
						data_len = ((payload_size + 8) >> 3) << 3;
					hevc->sei_itu_data_len += data_len;
				}
#endif
			break;
			case SEI_MasteringDisplayColorVolume:
				/* master_display_colour */
				p_sei = p;
				for (i = 0; i < 3; i++) {
					for (j = 0; j < 2; j++) {
						hevc->primaries[i][j]
							= (*p_sei<<8)
							| *(p_sei+1);
						p_sei += 2;
					}
				}
				for (i = 0; i < 2; i++) {
					hevc->white_point[i]
						= (*p_sei<<8)
						| *(p_sei+1);
					p_sei += 2;
				}
				for (i = 0; i < 2; i++) {
					hevc->luminance[i]
						= (*p_sei<<24)
						| (*(p_sei+1)<<16)
						| (*(p_sei+2)<<8)
						| *(p_sei+3);
					p_sei += 4;
				}
				hevc->sei_present_flag |=
					SEI_MASTER_DISPLAY_COLOR_MASK;
				break;
			case SEI_ContentLightLevel:
				if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI)
					hevc_print(hevc, 0,
						"sei type: max content light level %d, size %d\n",
					payload_type, payload_size);
				/* content_light_level */
				p_sei = p;
				hevc->content_light_level[0]
					= (*p_sei<<8) | *(p_sei+1);
				p_sei += 2;
				hevc->content_light_level[1]
					= (*p_sei<<8) | *(p_sei+1);
				p_sei += 2;
				hevc->sei_present_flag |=
					SEI_CONTENT_LIGHT_LEVEL_MASK;
				if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI)
					hevc_print(hevc, 0,
						"\tmax cll = %04x, max_pa_cll = %04x\n",
					hevc->content_light_level[0],
					hevc->content_light_level[1]);
				break;
			default:
				break;
			}
		}
		p += payload_size;
	}
	return 0;
}

static unsigned calc_ar(unsigned idc, unsigned sar_w, unsigned sar_h,
			unsigned w, unsigned h)
{
	unsigned ar;

	if (idc	== 255) {
		ar = div_u64(256ULL * sar_h * h,
				sar_w * w);
	} else {
		switch (idc) {
		case 1:
			ar = 0x100 * h / w;
			break;
		case 2:
			ar = 0x100 * h * 11 / (w * 12);
			break;
		case 3:
			ar = 0x100 * h * 11 / (w * 10);
			break;
		case 4:
			ar = 0x100 * h * 11 / (w * 16);
			break;
		case 5:
			ar = 0x100 * h * 33 / (w * 40);
			break;
		case 6:
			ar = 0x100 * h * 11 / (w * 24);
			break;
		case 7:
			ar = 0x100 * h * 11 / (w * 20);
			break;
		case 8:
			ar = 0x100 * h * 11 / (w * 32);
			break;
		case 9:
			ar = 0x100 * h * 33 / (w * 80);
			break;
		case 10:
			ar = 0x100 * h * 11 / (w * 18);
			break;
		case 11:
			ar = 0x100 * h * 11 / (w * 15);
			break;
		case 12:
			ar = 0x100 * h * 33 / (w * 64);
			break;
		case 13:
			ar = 0x100 * h * 99 / (w * 160);
			break;
		case 14:
			ar = 0x100 * h * 3 / (w * 4);
			break;
		case 15:
			ar = 0x100 * h * 2 / (w * 3);
			break;
		case 16:
			ar = 0x100 * h * 1 / (w * 2);
			break;
		default:
			ar = h * 0x100 / w;
			break;
		}
	}

	return ar;
}

static void set_frame_info(struct hevc_state_s *hevc, struct vframe_s *vf,
			struct PIC_s *pic)
{
	unsigned int ar;
	int i, j;
	char *p;
	unsigned size = 0;
	unsigned type = 0;
	struct vframe_master_display_colour_s *vf_dp
		= &vf->prop.master_display_colour;

	vf->width = pic->width /
		get_double_write_ratio(pic->double_write_mode);
	vf->height = pic->height /
		get_double_write_ratio(pic->double_write_mode);

	vf->duration = hevc->frame_dur;
	vf->duration_pulldown = 0;
	vf->flag = 0;

	ar = min_t(u32, hevc->frame_ar, DISP_RATIO_ASPECT_RATIO_MAX);
	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);

	if (((pic->aspect_ratio_idc == 255) &&
		pic->sar_width &&
		pic->sar_height) ||
		((pic->aspect_ratio_idc != 255) &&
		(pic->width))) {
		ar = min_t(u32,
			calc_ar(pic->aspect_ratio_idc,
			pic->sar_width,
			pic->sar_height,
			pic->width,
			pic->height),
			DISP_RATIO_ASPECT_RATIO_MAX);
		vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);
		vf->ratio_control <<= hevc->interlace_flag;
	}
	hevc->ratio_control = vf->ratio_control;
	if (pic->aux_data_buf
		&& pic->aux_data_size) {
		/* parser sei */
		p = pic->aux_data_buf;
		while (p < pic->aux_data_buf
			+ pic->aux_data_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (type == 0x02000000) {
				parse_sei(hevc, pic, p, size);
			}
			p += size;
		}
	}
	if (hevc->video_signal_type & VIDEO_SIGNAL_TYPE_AVAILABLE_MASK) {
		vf->signal_type = pic->video_signal_type;
		/* When the matrix_coeffiecents, transfer_characteristics and colour_primaries
		 * syntax elements are absent, their values shall be presumed to be equal to 2
		 */
		if ((vf->signal_type & 0x1000000) == 0) {
			vf->signal_type = vf->signal_type & 0xff000000;
			vf->signal_type = vf->signal_type | 0x20202;
		}
		if (hevc->sei_present_flag & SEI_HDR10PLUS_MASK) {
			u32 data;
			data = vf->signal_type;
			data = data & 0xFFFF00FF;
			data = data | (0x30<<8);
			vf->signal_type = data;
		}

		if (hevc->sei_present_flag & SEI_HDR_CUVA_MASK) {
			u32 data;
			data = vf->signal_type;
			data = data & 0x7FFFFFFF;
			data = data | (1<<31);
			vf->signal_type = data;
		}
	}
	else
		vf->signal_type = 0;
	hevc->video_signal_type_debug = vf->signal_type;

	/* master_display_colour */
	if (hevc->sei_present_flag & SEI_MASTER_DISPLAY_COLOR_MASK) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++)
				vf_dp->primaries[i][j] = hevc->primaries[i][j];
		for (i = 0; i < 2; i++) {
			vf_dp->white_point[i] = hevc->white_point[i];
			vf_dp->luminance[i]
				= hevc->luminance[i];
		}
		vf_dp->present_flag = 1;
	} else
		vf_dp->present_flag = 0;

	/* content_light_level */
	if (hevc->sei_present_flag & SEI_CONTENT_LIGHT_LEVEL_MASK) {
		vf_dp->content_light_level.max_content
			= hevc->content_light_level[0];
		vf_dp->content_light_level.max_pic_average
			= hevc->content_light_level[1];
		vf_dp->content_light_level.present_flag = 1;
	} else
		vf_dp->content_light_level.present_flag = 0;

	if ((hevc->video_signal_type & VIDEO_SIGNAL_TYPE_AVAILABLE_MASK) ||
		(hevc->sei_present_flag & SEI_HDR10PLUS_MASK) ||
		(vf_dp->present_flag) ||
		(vf_dp->content_light_level.present_flag)) {
		struct aml_vdec_hdr_infos hdr;
		struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);

		memset(&hdr, 0, sizeof(hdr));
		hdr.signal_type = vf->signal_type;
		hdr.color_parms = *vf_dp;
		vdec_v4l_set_hdr_infos(ctx, &hdr);
	}

	if ((hevc->sei_present_flag & SEI_HDR10PLUS_MASK) && (pic->hdr10p_data_buf != NULL)
		&& (pic->hdr10p_data_size != 0)) {
		if (pic->hdr10p_data_size <= 128) {
			char *new_buf;
			new_buf = kzalloc(pic->hdr10p_data_size, GFP_ATOMIC);

			if (new_buf) {
				memcpy(new_buf, pic->hdr10p_data_buf, pic->hdr10p_data_size);
				if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI) {
					PR_INIT(128);
					hevc_print(hevc, 0,
						"hdr10p data: (size %d)\n",
						pic->hdr10p_data_size);
					for (i = 0; i < pic->hdr10p_data_size; i++) {
						PR_FILL("%02x ", pic->hdr10p_data_buf[i]);
						if (((i + 1) & 0xf) == 0)
							PR_INFO(hevc->index);
					}
					PR_INFO(hevc->index);
				}

				vf->hdr10p_data_size = pic->hdr10p_data_size;
				vf->hdr10p_data_buf = new_buf;
				set_meta_data_to_vf(vf, UVM_META_DATA_HDR10P_DATA, hevc->v4l2_ctx);
			} else {
				hevc_print(hevc, 0,
					"%s:hdr10p data vzalloc size(%d) fail\n",
					__func__, pic->hdr10p_data_size);
				vf->hdr10p_data_buf = NULL;
				vf->hdr10p_data_size = 0;
			}
		}

		vfree(pic->hdr10p_data_buf);
		pic->hdr10p_data_buf = NULL;
		pic->hdr10p_data_size = 0;
	}

	vf->sidebind_type = hevc->sidebind_type;
	vf->sidebind_channel_id = hevc->sidebind_channel_id;
}

static int vh265_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif

	spin_lock_irqsave(&h265_lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hevc->newframe_q);
	states->buf_avail_num = kfifo_len(&hevc->display_q);

	if (step == 2)
		states->buf_avail_num = 0;
	spin_unlock_irqrestore(&h265_lock, flags);
	return 0;
}

static struct vframe_s *vh265_vf_peek(void *op_arg)
{
	struct vframe_s *vf[2] = {0, 0};
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif

	if (step == 2)
		return NULL;

	if (force_disp_pic_index & 0x100) {
		if (force_disp_pic_index & 0x200)
			return NULL;
		return &hevc->vframe_dummy;
	}


	if (kfifo_out_peek(&hevc->display_q, (void *)&vf, 2)) {
		if (vf[1]) {
			vf[0]->next_vf_pts_valid = true;
			vf[0]->next_vf_pts = vf[1]->pts;
		} else
			vf[0]->next_vf_pts_valid = false;
		return vf[0];
	}

	return NULL;
}

static struct vframe_s *vh265_vf_get(void *op_arg)
{
	struct vframe_s *vf;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif

	if (step == 2)
		return NULL;
	else if (step == 1)
		step = 2;

	if (kfifo_get(&hevc->display_q, &vf)) {
		struct vframe_s *next_vf = NULL;

		ATRACE_COUNTER(hevc->trace.vf_get_name, (long)vf);
		ATRACE_COUNTER(hevc->trace.disp_q_name, kfifo_len(&hevc->display_q));
#ifdef MULTI_INSTANCE_SUPPORT
		ATRACE_COUNTER(hevc->trace.set_canvas0_addr, vf->canvas0_config[0].phy_addr);
#else
		ATRACE_COUNTER(hevc->trace.get_canvas0_addr, vf->canvas0Addr);
#endif

		if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT) {
			hevc_print(hevc, 0,
				"%s(vf 0x%p type %d index 0x%x poc %d/%d) pts(%d,%d) dur %d\n",
				__func__, vf, vf->type, vf->index,
				get_pic_poc(hevc, vf->index & 0xff),
				get_pic_poc(hevc, (vf->index >> 8) & 0xff),
				vf->pts, vf->pts_us64,
				vf->duration);
#ifdef MULTI_INSTANCE_SUPPORT
			hevc_print(hevc, 0, "get canvas0 addr:0x%x\n", vf->canvas0_config[0].phy_addr);
#else
			hevc_print(hevc, 0, "get canvas0 addr:0x%x\n", vf->canvas0Addr);
#endif
		}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (get_dbg_flag(hevc) & H265_DEBUG_DV) {
			struct PIC_s *pic = hevc->m_PIC[vf->index & 0xff];
			hevc_print(hevc, 0, "pic 0x%p aux size %d:\n",
					pic, pic->aux_data_size);
			if (pic->aux_data_buf && pic->aux_data_size > 0) {
				PR_INIT(128);
				int i;
				for (i = 0; i < pic->aux_data_size; i++) {
					PR_FILL("%02x ", pic->aux_data_buf[i]);
					if (((i + 1) & 0xf) == 0)
						PR_INFO(hevc->index);
				}
				PR_INFO(hevc->index);
			}
		}
#endif
		hevc->show_frame_num++;
		vf->index_disp = atomic_read(&hevc->vf_get_count);
		vf->omx_index = atomic_read(&hevc->vf_get_count);
		atomic_add(1, &hevc->vf_get_count);

		if (kfifo_peek(&hevc->display_q, &next_vf) && next_vf) {
			vf->next_vf_pts_valid = true;
			vf->next_vf_pts = next_vf->pts;
		} else
			vf->next_vf_pts_valid = false;

		return vf;
	}

	return NULL;
}
static bool vf_valid_check(struct vframe_s *vf, struct hevc_state_s *hevc) {
	int i;
	for (i = 0; i < VF_POOL_SIZE; i++) {
		if (vf == &hevc->vfpool[i]  || vf == &hevc->vframe_dummy)
			return true;
	}
	hevc_print(hevc, 0," h265 invalid vf been put, vf = %p\n", vf);
	for (i = 0; i < VF_POOL_SIZE; i++) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,"valid vf[%d]= %p \n", i, &hevc->vfpool[i]);
	}
	return false;
}

static void vh265_vf_put(struct vframe_s *vf, void *op_arg)
{
	unsigned long flags;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif
	unsigned char index_top;
	unsigned char index_bot;

	if (!vf)
		return;
	if (vf == (&hevc->vframe_dummy))
		return;
	if (vf && (vf_valid_check(vf, hevc) == false))
		return;

	if (hevc->enable_fence && vf->fence) {
		int ret, i;

		mutex_lock(&hevc->fence_mutex);
		ret = dma_fence_get_status(vf->fence);
		if (ret == 0) {
			for (i = 0; i < VF_POOL_SIZE; i++) {
				if (hevc->fence_vf_s.fence_vf[i] == NULL) {
					hevc->fence_vf_s.fence_vf[i] = vf;
					hevc->fence_vf_s.used_size++;
					mutex_unlock(&hevc->fence_mutex);
					return;
				}
			}
		}
		mutex_unlock(&hevc->fence_mutex);
	}

	ATRACE_COUNTER(hevc->trace.vf_put_name, (long)vf);
#ifdef MULTI_INSTANCE_SUPPORT
	ATRACE_COUNTER(hevc->trace.put_canvas0_addr, vf->canvas0_config[0].phy_addr);
#else
	ATRACE_COUNTER(hevc->trace.put_canvas0_addr, vf->canvas0Addr);
#endif
	index_top = vf->index & 0xff;
	index_bot = (vf->index >> 8) & 0xff;
	if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
		hevc_print(hevc, 0,
			"%s(vf 0x%p type %d index 0x%x put canvas0 addr:0x%x)\n",
			__func__, vf, vf->type, vf->index
#ifdef MULTI_INSTANCE_SUPPORT
			, vf->canvas0_config[0].phy_addr
#else
			, vf->canvas0Addr
#endif
			);
	atomic_add(1, &hevc->vf_put_count);
	spin_lock_irqsave(&h265_lock, flags);
	kfifo_put(&hevc->newframe_q, (const struct vframe_s *)vf);
	ATRACE_COUNTER(hevc->trace.new_q_name, kfifo_len(&hevc->newframe_q));
	if (hevc->enable_fence && vf->fence) {
		vdec_fence_put(vf->fence);
		vf->fence = NULL;
	}

	if (vf->hdr10p_data_buf) {
		kfree(vf->hdr10p_data_buf);
		vf->hdr10p_data_buf = NULL;
		vf->hdr10p_data_size = 0;
	}

	if (vf->meta_data_buf) {
		vf->meta_data_buf = NULL;
		vf->meta_data_size = 0;
	}

	if (!hevc->interlace_flag &&
		(vf->v4l_mem_handle !=
		hevc->m_PIC[index_top]->cma_alloc_addr)) {
		hevc_print(hevc, PRINT_FLAG_V4L_DETAIL,
			"H265 update fb handle, old:%llx, new:%llx\n",
			hevc->m_PIC[index_top]->cma_alloc_addr,
			vf->v4l_mem_handle);

		hevc->m_PIC[index_top]->cma_alloc_addr
			= vf->v4l_mem_handle;
	}

	if (index_top != 0xff
		&& index_top < MAX_REF_PIC_NUM
		&& hevc->m_PIC[index_top]) {
		if (hevc->m_PIC[index_top]->vf_ref > 0) {
			hevc->m_PIC[index_top]->vf_ref--;

			if (hevc->m_PIC[index_top]->vf_ref == 0) {
				hevc->m_PIC[index_top]->output_ready = 0;
				hevc->m_PIC[index_top]->show_frame = false;

				if (hevc->wait_buf != 0)
					WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG,
						0x1);
			}
		}
	}

	if (index_bot != 0xff
		&& index_bot < MAX_REF_PIC_NUM
		&& hevc->m_PIC[index_bot]) {
		if (hevc->m_PIC[index_bot]->vf_ref > 0) {
			hevc->m_PIC[index_bot]->vf_ref--;

			if (hevc->m_PIC[index_bot]->vf_ref == 0) {
				hevc->m_PIC[index_bot]->output_ready = 0;
				hevc->m_PIC[index_bot]->show_frame = false;

				if (hevc->wait_buf != 0)
					WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG,
						0x1);
			}
		}
	}

	spin_unlock_irqrestore(&h265_lock, flags);
}


static int vh265_event_cb(int type, void *data, void *op_arg)
{
	unsigned long flags;
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *vdec = op_arg;
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = (struct hevc_state_s *)op_arg;
#endif
	if (type & VFRAME_EVENT_RECEIVER_GET_AUX_DATA) {
		struct provider_aux_req_s *req =
			(struct provider_aux_req_s *)data;
		unsigned char index;

		if (!req->vf) {
			req->aux_size = atomic_read(&hevc->vf_put_count);
			return 0;
		}
		spin_lock_irqsave(&h265_lock, flags);
		index = req->vf->index & 0xff;
		req->aux_buf = NULL;
		req->aux_size = 0;
		req->format = VFORMAT_HEVC;
		if (req->bot_flag)
			index = (req->vf->index >> 8) & 0xff;
		if (index != 0xff
			&& index < MAX_REF_PIC_NUM
			&& hevc->m_PIC[index]) {
			req->aux_buf = hevc->m_PIC[index]->aux_data_buf;
			req->aux_size = hevc->m_PIC[index]->aux_data_size;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (hevc->bypass_dvenl && !dolby_meta_with_el)
				req->dv_enhance_exist = false;
			else
				req->dv_enhance_exist =
					hevc->m_PIC[index]->dv_enhance_exist;
			if (vdec_frame_based(vdec) && (hevc->dv_duallayer == true))
				req->dv_enhance_exist = 1;
			hevc_print(hevc, H265_DEBUG_DV,
				"query dv_enhance_exist for (pic 0x%p, vf 0x%p, poc %d index %d) flag => %d, aux sizd 0x%x\n",
				hevc->m_PIC[index],
				req->vf,
				hevc->m_PIC[index]->POC, index,
				req->dv_enhance_exist, req->aux_size);
#else
			req->dv_enhance_exist = 0;
#endif
		}
		spin_unlock_irqrestore(&h265_lock, flags);

		if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
			hevc_print(hevc, 0,
				"%s(type 0x%x vf index 0x%x)=>size 0x%x\n",
				__func__, type, index, req->aux_size);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	else if (type & VFRAME_EVENT_RECEIVER_DOLBY_BYPASS_EL) {
		if ((force_bypass_dvenl & 0x80000000) == 0) {
			hevc_print(hevc, 0,
			"%s: VFRAME_EVENT_RECEIVER_DOLBY_BYPASS_EL\n",
			__func__);
			hevc->bypass_dvenl_enable = 1;
		}
	}
#endif
	else if (type & VFRAME_EVENT_RECEIVER_REQ_STATE) {
		struct provider_state_req_s *req =
			(struct provider_state_req_s *)data;
		if (req->req_type == REQ_STATE_SECURE)
			req->req_result[0] = vdec_secure(vdec);
		else
			req->req_result[0] = 0xffffffff;
	}

	return 0;
}

static void get_pair_fb(struct hevc_state_s *hevc, struct vframe_s *vf)
{
	int i;
	for (i = 0; i < 2; i ++) {
		if (hevc->pair_fb[i] == NULL) {
			hevc->pair_fb[i] = (struct vdec_v4l2_buffer *)vf->v4l_mem_handle;
			break;
		}
	}

	if (i >= 2) {
		hevc->pair_fb[0] = (struct vdec_v4l2_buffer *)vf->v4l_mem_handle;
		hevc->pair_fb[1] = NULL;
	}
}

static void clear_pair_fb(struct hevc_state_s *hevc)
{
	int i;
	for (i = 0; i < 2; i ++)
		hevc->pair_fb[i] = NULL;
}

static bool v4l_output_dw_with_compress(struct hevc_state_s *hevc, int dw)
{
	if ((dw == 0x10) ||
		IS_8K_SIZE(hevc->frame_width, hevc->frame_height) ||
		hevc->interlace_flag)
		return false;

	return true;
}

#ifdef HEVC_PIC_STRUCT_SUPPORT
static int process_pending_vframe(struct hevc_state_s *hevc,
	struct PIC_s *pair_pic, unsigned char pair_frame_top_flag)
{
	struct vframe_s *vf;

	if (!pair_pic)
		return -1;

	hevc_print(hevc, H265_DEBUG_PIC_STRUCT,
		"%s: pair_pic index 0x%x %s\n", __func__, pair_pic->index,
		pair_frame_top_flag ? "top" : "bot");

	if (kfifo_len(&hevc->pending_q) > 1) {
		unsigned long flags;
		int index1;
		int index2;
		/* do not pending more than 1 frame */
		if (kfifo_get(&hevc->pending_q, &vf) == 0) {
			hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
			return -1;
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
			hevc_print(hevc, 0,
				"%s warning(1), vf=>display_q: (index 0x%x), vf 0x%px\n",
				__func__, vf->index, vf);
		if (v4l_output_dw_with_compress(hevc, pair_pic->double_write_mode)) {
			vf->type |= VIDTYPE_COMPRESS;
			if (hevc->mmu_enable)
				vf->type |= VIDTYPE_SCATTER;
		}

		/* recycle vframe */
		atomic_add(1, &hevc->vf_pre_count);
		spin_lock_irqsave(&h265_lock, flags);
		kfifo_put(&hevc->newframe_q, (const struct vframe_s *)vf);
		index1 = vf->index & 0xff;
		index2 = (vf->index >> 8) & 0xff;
		if (index1 >= MAX_REF_PIC_NUM &&
			index2 >= MAX_REF_PIC_NUM) {
			spin_unlock_irqrestore(&h265_lock, flags);
			return -1;
		}

		if (index1 < MAX_REF_PIC_NUM) {
			hevc->m_PIC[index1]->vf_ref = 0;
			hevc->m_PIC[index1]->output_ready = 0;
		}
		if (index2 < MAX_REF_PIC_NUM) {
			hevc->m_PIC[index2]->vf_ref = 0;
			hevc->m_PIC[index2]->output_ready = 0;
		}
		if (hevc->wait_buf != 0)
			WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG,
				0x1);
		spin_unlock_irqrestore(&h265_lock, flags);

		ATRACE_COUNTER(hevc->trace.pts_name, vf->timestamp);
	}

	if (kfifo_peek(&hevc->pending_q, &vf)) {
		if (kfifo_get(&hevc->pending_q, &vf) == 0) {
			hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
		}
		if (vf == NULL)
			return -1;

		if (pair_pic == NULL || pair_pic->vf_ref <= 0) {
			/*
			 *if pair_pic is recycled (pair_pic->vf_ref <= 0),
			 *do not use it
			 */
			hevc_print(hevc, H265_DEBUG_PIC_STRUCT,
				"%s warning(2), vf=>display_q: (index 0x%x)\n",
				__func__, vf->index);

			if (v4l_output_dw_with_compress(hevc, pair_pic->double_write_mode)) {
				vf->type |= VIDTYPE_COMPRESS;
				if (hevc->mmu_enable)
					vf->type |= VIDTYPE_SCATTER;
			}
			atomic_add(1, &hevc->vf_pre_count);
			vdec_vframe_ready(hw_to_vdec(hevc), vf);
			hevc->send_frame_flag = 1;
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf);
			ATRACE_COUNTER(hevc->trace.pts_name, vf->timestamp);
		} else if ((!pair_frame_top_flag) && (((vf->index >> 8) & 0xff) == 0xff)) {
			if (v4l_output_dw_with_compress(hevc, pair_pic->double_write_mode)) {
				vf->type |= VIDTYPE_COMPRESS;
				if (hevc->mmu_enable)
					vf->type |= VIDTYPE_SCATTER;
			}

			vf->index &= 0xff;
			vf->index |= (pair_pic->index << 8);
			pair_pic->vf_ref++;
			vdec_vframe_ready(hw_to_vdec(hevc), vf);
			hevc->send_frame_flag = 1;
			get_pair_fb(hevc, vf);
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf);
			ATRACE_COUNTER(hevc->trace.pts_name, vf->timestamp);
			atomic_add(1, &hevc->vf_pre_count);
			hevc_print(hevc, H265_DEBUG_PIC_STRUCT,
				"%s vf => display_q: (index 0x%x), w %d, h %d, type 0x%x\n",
				__func__, vf->index, vf->width, vf->height, vf->type);
		} else if (pair_frame_top_flag && ((vf->index & 0xff) == 0xff)) {
			if (v4l_output_dw_with_compress(hevc, pair_pic->double_write_mode)) {
				vf->type |= VIDTYPE_COMPRESS;
				if (hevc->mmu_enable)
					vf->type |= VIDTYPE_SCATTER;
			}

			vf->index &= 0xff00;
			vf->index |= pair_pic->index;
			pair_pic->vf_ref++;
			vdec_vframe_ready(hw_to_vdec(hevc), vf);
			hevc->send_frame_flag = 1;
			get_pair_fb(hevc, vf);
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf);
			ATRACE_COUNTER(hevc->trace.pts_name, vf->timestamp);
			atomic_add(1, &hevc->vf_pre_count);
			hevc_print(hevc, H265_DEBUG_PIC_STRUCT,
				"%s vf => display_q: (index 0x%x), w %d, h %d, type 0x%x\n",
				__func__, vf->index, vf->width, vf->height, vf->type);
		}
	}
	return 0;
}
#endif

static void fill_frame_info(struct hevc_state_s *hevc,
	struct PIC_s *pic, unsigned int framesize, unsigned int pts)
{
	struct vframe_qos_s *vframe_qos = &hevc->vframe_qos;
	if (hevc->m_nalUnitType == NAL_UNIT_CODED_SLICE_IDR)
		vframe_qos->type = 4;
	else if (pic->slice_type == I_SLICE)
		vframe_qos->type = 1;
	else if (pic->slice_type == P_SLICE)
		vframe_qos->type = 2;
	else if (pic->slice_type == B_SLICE)
		vframe_qos->type = 3;

	if (input_frame_based(hw_to_vdec(hevc)))
		vframe_qos->size = pic->frame_size;
	else
		vframe_qos->size = framesize;
	vframe_qos->pts = pts;
	vframe_qos->max_mv = pic->max_mv;
	vframe_qos->avg_mv = pic->avg_mv;
	vframe_qos->min_mv = pic->min_mv;


	vframe_qos->max_qp = pic->max_qp;
	vframe_qos->avg_qp = pic->avg_qp;
	vframe_qos->min_qp = pic->min_qp;

	vframe_qos->max_skip = pic->max_skip;
	vframe_qos->avg_skip = pic->avg_skip;
	vframe_qos->min_skip = pic->min_skip;

	vframe_qos->num++;

}

static inline void hevc_update_gvs(struct hevc_state_s *hevc)
{
	if (hevc->gvs->frame_height != hevc->frame_height) {
		hevc->gvs->frame_width = hevc->frame_width;
		hevc->gvs->frame_height = hevc->frame_height;
	}
	if (hevc->gvs->frame_dur != hevc->frame_dur) {
		hevc->gvs->frame_dur = hevc->frame_dur;
		if (hevc->frame_dur != 0)
			hevc->gvs->frame_rate = ((96000 * 10 / hevc->frame_dur) % 10) < 5 ?
					96000 / hevc->frame_dur : (96000 / hevc->frame_dur +1);
		else
			hevc->gvs->frame_rate = -1;
	}
	hevc->gvs->error_count = hevc->gvs->error_frame_count;
	hevc->gvs->status = hevc->stat | hevc->fatal_error;
	if (hevc->gvs->ratio_control != hevc->ratio_control)
		hevc->gvs->ratio_control = hevc->ratio_control;
}

static void put_vf_to_display_q(struct hevc_state_s *hevc, struct vframe_s *vf)
{
	atomic_add(1, &hevc->vf_pre_count);
	decoder_do_frame_check(hw_to_vdec(hevc), vf);
	vdec_vframe_ready(hw_to_vdec(hevc), vf);
	hevc->send_frame_flag = 1;
	kfifo_put(&hevc->display_q, (const struct vframe_s *)vf);
	ATRACE_COUNTER(hevc->trace.pts_name, vf->timestamp);
}

static int post_prepare_process(struct vdec_s *vdec, struct PIC_s *frame)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;

	if (force_disp_pic_index & 0x100) {
		/*recycle directly*/
		frame->output_ready = 0;
		frame->show_frame = false;
		hevc_print(hevc, 0, "discard show frame.\n");
		return 0;
	}

	frame->show_frame = true;

	return 0;
}

static int post_video_frame(struct vdec_s *vdec, struct PIC_s *pic)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
	struct vframe_s *vf = NULL;
	int stream_offset = pic->stream_offset;
	unsigned short slice_type = pic->slice_type;
	ulong nv_order = VIDTYPE_VIU_NV21;
	u32 frame_size = 0;
	struct vdec_info tmp4x;
	struct aml_vcodec_ctx * v4l2_ctx = hevc->v4l2_ctx;
	struct vdec_v4l2_buffer *fb = NULL;
	int index;

	hevc->send_frame_flag = 0;
	/* swap uv */
	if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12) ||
		(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12M))
		nv_order = VIDTYPE_VIU_NV12;

	if (kfifo_get(&hevc->newframe_q, &vf) == 0) {
		hevc_print(hevc, 0,
			"fatal error, no available buffer slot.");
		return -1;
	}

	if (vf) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s: pic index 0x%x error_mark %d\n",
			__func__, pic->index, pic->error_mark);

		vf->frame_type = 0;

		if (pic->error_mark) {
			vf->frame_type |= V4L2_BUF_FLAG_ERROR;
		}

		vf->v4l_mem_handle = pic->cma_alloc_addr;
		fb = (struct vdec_v4l2_buffer *)vf->v4l_mem_handle;

		if (hevc->enable_fence) {
			/* fill fence information. */
			if (hevc->fence_usage == FENCE_USE_FOR_DRIVER)
				vf->fence	= pic->fence;
		}

#ifdef MULTI_INSTANCE_SUPPORT
		if (vdec_frame_based(vdec)) {
			vf->pts = pic->pts;
			vf->pts_us64 = pic->pts64;
			vf->timestamp = pic->timestamp;
		}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		else if (vdec->master == NULL) {
#else
		else {
#endif
#endif
			hevc_print(hevc, H265_DEBUG_OUT_PTS,
				"call pts_lookup_offset_us64(0x%x)\n",
				stream_offset);
			if ((vdec->vbuf.no_parser == 0) || (vdec->vbuf.use_ptsserv)) {
				if (pts_lookup_offset_us64
					(PTS_TYPE_VIDEO, stream_offset, &vf->pts,
					&frame_size, 0, &vf->pts_us64) != 0) {
#ifdef DEBUG_PTS
					hevc->pts_missed++;
#endif
					vf->pts = 0;
					vf->pts_us64 = 0;
				} else {
#ifdef DEBUG_PTS
					hevc->pts_hit++;
#endif
				}
			}

#ifdef MULTI_INSTANCE_SUPPORT
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		} else {
			vf->pts = 0;
			vf->pts_us64 = 0;
		}
#else
		}
#endif
#endif

		if (pts_unstable && (hevc->frame_dur > 0))
			hevc->pts_mode = PTS_NONE_REF_USE_DURATION;

		fill_frame_info(hevc, pic, frame_size, vf->pts);

		if (vf->pts != 0)
			hevc->last_lookup_pts = vf->pts;

		if ((hevc->pts_mode == PTS_NONE_REF_USE_DURATION)
			&& (slice_type != 2))
			vf->pts = hevc->last_pts + DUR2PTS(hevc->frame_dur);
		hevc->last_pts = vf->pts;

		if (vf->pts_us64 != 0)
			hevc->last_lookup_pts_us64 = vf->pts_us64;

		if ((hevc->pts_mode == PTS_NONE_REF_USE_DURATION)
			&& (slice_type != 2)) {
			vf->pts_us64 = hevc->last_pts_us64 +
				(DUR2PTS(hevc->frame_dur) * 100 / 9);
		}
		hevc->last_pts_us64 = vf->pts_us64;
		if ((get_dbg_flag(hevc) & H265_DEBUG_OUT_PTS) != 0) {
			hevc_print(hevc, 0,
				"H265 dec out pts: vf->pts=%d, vf->pts_us64 = %lld, ts: %llu\n",
				vf->pts, vf->pts_us64, vf->timestamp);
		}

		/*
		 *vf->index:
		 *(1) vf->type is VIDTYPE_INTERLACE
		 *	and vf->canvas0Addr !=  vf->canvas1Addr,
		 *	vf->index[7:0] is the index of top pic
		 *	vf->index[15:8] is the index of bot pic
		 *(2) other cases,
		 *	only vf->index[7:0] is used
		 *	vf->index[15:8] == 0xff
		 */
		vf->index = 0xff00 | pic->index;
/*SUPPORT_10BIT*/
		if (pic->double_write_mode & 0x10) {
			/* double write only */
			vf->compBodyAddr = 0;
			vf->compHeadAddr = 0;
		} else {

		if (hevc->mmu_enable) {
			vf->compBodyAddr = 0;
			vf->compHeadAddr = pic->header_adr;
		} else {
			vf->compBodyAddr = pic->mc_y_adr; /*body adr*/
			vf->compHeadAddr = pic->mc_y_adr +
						pic->losless_comp_body_size;
			vf->mem_head_handle = NULL;
		}

					/*head adr*/
			vf->canvas0Addr = vf->canvas1Addr = 0;
		}
		if (pic->double_write_mode) {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
			vf->type |= nv_order;

			if (v4l_output_dw_with_compress(hevc, pic->double_write_mode)) {
				vf->type |= VIDTYPE_COMPRESS;
				if (hevc->mmu_enable)
					vf->type |= VIDTYPE_SCATTER;
			}

#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag &&
				(get_dbg_flag(hevc)
				& H265_CFG_CANVAS_IN_DECODE) == 0) {
					vf->canvas0Addr = vf->canvas1Addr = -1;
					vf->plane_num = 2;
					vf->canvas0_config[0] =
						pic->canvas_config[0];
					vf->canvas0_config[1] =
						pic->canvas_config[1];

					vf->canvas1_config[0] =
						pic->canvas_config[0];
					vf->canvas1_config[1] =
						pic->canvas_config[1];

			} else
#endif
				vf->canvas0Addr = vf->canvas1Addr
				= spec2canvas(pic);
		} else {
			vf->canvas0Addr = vf->canvas1Addr = 0;
			vf->type = VIDTYPE_COMPRESS | VIDTYPE_VIU_FIELD;
			if (hevc->mmu_enable)
				vf->type |= VIDTYPE_SCATTER;
		}
		vf->compWidth = pic->width;
		vf->compHeight = pic->height;
		switch (pic->bit_depth_luma) {
		case 9:
			vf->bitdepth = BITDEPTH_Y9;
			break;
		case 10:
			vf->bitdepth = BITDEPTH_Y10;
			break;
		default:
			vf->bitdepth = BITDEPTH_Y8;
			break;
		}
		switch (pic->bit_depth_chroma) {
		case 9:
			vf->bitdepth |= (BITDEPTH_U9 | BITDEPTH_V9);
			break;
		case 10:
			vf->bitdepth |= (BITDEPTH_U10 | BITDEPTH_V10);
			break;
		default:
			vf->bitdepth |= (BITDEPTH_U8 | BITDEPTH_V8);
			break;
		}
		if ((vf->type & VIDTYPE_COMPRESS) == 0)
			vf->bitdepth =
				BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		if (pic->mem_saving_mode == 1)
			vf->bitdepth |= BITDEPTH_SAVING_MODE;

		set_frame_info(hevc, vf, pic);
		if (hevc->discard_dv_data) {
			vf->discard_dv_data = true;
		}

		if (hevc->high_bandwidth_flag) {
			vf->flag |= VFRAME_FLAG_HIGH_BANDWIDTH;
		}

		vf->width = pic->width;
		vf->height = pic->height;

		if (force_w_h != 0) {
			vf->width = (force_w_h >> 16) & 0xffff;
			vf->height = force_w_h & 0xffff;
		}
		if (force_fps & 0x100) {
			u32 rate = force_fps & 0xff;

			if (rate)
				vf->duration = 96000/rate;
			else
				vf->duration = 0;
		}
		if (force_fps & 0x200) {
			vf->pts = 0;
			vf->pts_us64 = 0;
		}
		if (!vdec->vbuf.use_ptsserv && vdec_stream_based(vdec)) {
			vf->pts_us64 = stream_offset;
			vf->pts = 0;
		}
		/*
		 *	!!! to do ...
		 *	need move below code to get_new_pic(),
		 *	hevc->xxx can only be used by current decoded pic
		 */
		if (pic->conformance_window_flag &&
			(get_dbg_flag(hevc) &
				H265_DEBUG_IGNORE_CONFORMANCE_WINDOW) == 0) {
			unsigned int SubWidthC, SubHeightC;

			switch (pic->chroma_format_idc) {
			case 1:
				SubWidthC = 2;
				SubHeightC = 2;
				break;
			case 2:
				SubWidthC = 2;
				SubHeightC = 1;
				break;
			default:
				SubWidthC = 1;
				SubHeightC = 1;
				break;
			}
			 vf->width -= SubWidthC *
				(pic->conf_win_left_offset +
				pic->conf_win_right_offset);
			 vf->height -= SubHeightC *
				(pic->conf_win_top_offset +
				pic->conf_win_bottom_offset);

			 vf->compWidth -= SubWidthC *
				(pic->conf_win_left_offset +
				pic->conf_win_right_offset);
			 vf->compHeight -= SubHeightC *
				(pic->conf_win_top_offset +
				pic->conf_win_bottom_offset);

			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print(hevc, 0,
					"conformance_window %d, %d, %d, %d, %d => cropped width %d, height %d com_w %d com_h %d\n",
					pic->chroma_format_idc,
					pic->conf_win_left_offset,
					pic->conf_win_right_offset,
					pic->conf_win_top_offset,
					pic->conf_win_bottom_offset,
					vf->width, vf->height, vf->compWidth, vf->compHeight);
		}

		vf->src_fmt.play_id = vdec->inst_cnt;

		vf->width = vf->width /
			get_double_write_ratio(pic->double_write_mode);
		vf->height = vf->height /
			get_double_write_ratio(pic->double_write_mode);

		if (vdec->prog_only || (!v4l2_ctx->vpp_is_need))
			pic->pic_struct = 0;

		vf->height <<= hevc->interlace_flag;
		/* vf->compHeight <<= hevc->interlace_flag; */
		vf->canvas0_config[0].height <<= hevc->interlace_flag;
		vf->canvas0_config[1].height <<= hevc->interlace_flag;

#ifdef HEVC_PIC_STRUCT_SUPPORT
		if (pic->pic_struct == 3 || pic->pic_struct == 4) {
			struct vframe_s *vf2;

			hevc_print(hevc, H265_DEBUG_PIC_STRUCT,
				"pic_struct = %d index 0x%x\n",
				pic->pic_struct, pic->index);

			if (kfifo_get(&hevc->newframe_q, &vf2) == 0) {
				hevc_print(hevc, 0,
					"fatal error, no available buffer slot.");
				return -1;
			}

			pic->vf_ref = 2;
			vf->duration = vf->duration>>1;
			memcpy(vf2, vf, sizeof(struct vframe_s));

			if (v4l2_ctx->second_field_pts_mode) {
				vf2->timestamp = 0;
			}
			if (pic->pic_struct == 3) {
				vf->type = VIDTYPE_INTERLACE_TOP
				| nv_order;
				vf2->type = VIDTYPE_INTERLACE_BOTTOM
				| nv_order;
			} else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM
				| nv_order;
				vf2->type = VIDTYPE_INTERLACE_TOP
				| nv_order;
			}
			if (pic->show_frame) {
				put_vf_to_display_q(hevc, vf);
				atomic_add(1, &hevc->vf_pre_count);
				vdec_vframe_ready(hw_to_vdec(hevc), vf2);
				kfifo_put(&hevc->display_q, (const struct vframe_s *)vf2);
				ATRACE_COUNTER(hevc->trace.pts_name, vf2->timestamp);
			} else {
				vh265_vf_put(vf, vdec);
				vh265_vf_put(vf2, vdec);
				atomic_add(2, &hevc->vf_get_count);
				atomic_add(2, &hevc->vf_pre_count);
				return 0;
			}
		} else if (pic->pic_struct == 5
			|| pic->pic_struct == 6) {
			struct vframe_s *vf2, *vf3;

			hevc_print(hevc, H265_DEBUG_PIC_STRUCT,
				"pic_struct = %d index 0x%x\n",
				pic->pic_struct, pic->index);

			if (kfifo_get(&hevc->newframe_q, &vf2) == 0) {
				hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
			}
			if (kfifo_get(&hevc->newframe_q, &vf3) == 0) {
				hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
			}
			pic->vf_ref = 3;
			vf->duration = vf->duration/3;
			memcpy(vf2, vf, sizeof(struct vframe_s));
			memcpy(vf3, vf, sizeof(struct vframe_s));

			if (v4l2_ctx->second_field_pts_mode) {
				vf2->timestamp = 0;
				vf3->timestamp = 0;
			}

			if (pic->pic_struct == 5) {
				vf->type = VIDTYPE_INTERLACE_TOP
					| nv_order;
				vf2->type = VIDTYPE_INTERLACE_BOTTOM
					| nv_order;
				vf3->type = VIDTYPE_INTERLACE_TOP
					| nv_order;
			} else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM
					| nv_order;
				vf2->type = VIDTYPE_INTERLACE_TOP
					| nv_order;
				vf3->type = VIDTYPE_INTERLACE_BOTTOM
					| nv_order;
			}
			if (pic->show_frame) {
				put_vf_to_display_q(hevc, vf);
				atomic_add(1, &hevc->vf_pre_count);
				vdec_vframe_ready(hw_to_vdec(hevc), vf2);
				kfifo_put(&hevc->display_q, (const struct vframe_s *)vf2);
				ATRACE_COUNTER(hevc->trace.pts_name, vf2->timestamp);
				atomic_add(1, &hevc->vf_pre_count);
				vdec_vframe_ready(hw_to_vdec(hevc), vf3);
				kfifo_put(&hevc->display_q, (const struct vframe_s *)vf3);
				ATRACE_COUNTER(hevc->trace.pts_name, vf3->timestamp);
			} else {
				vh265_vf_put(vf, vdec);
				vh265_vf_put(vf2, vdec);
				vh265_vf_put(vf3, vdec);
				atomic_add(3, &hevc->vf_get_count);
				atomic_add(3, &hevc->vf_pre_count);
				return 0;
			}
		} else if (pic->pic_struct == 9
			|| pic->pic_struct == 10) {
			hevc_print(hevc, H265_DEBUG_PIC_STRUCT,
				"pic_struct = %d index 0x%x\n",
				pic->pic_struct, pic->index);

			pic->vf_ref = 1;
			/* process previous pending vf*/
			process_pending_vframe(hevc,
			pic, (pic->pic_struct == 9));

			if (pic->show_frame) {
				decoder_do_frame_check(vdec, vf);
				vdec_vframe_ready(vdec, vf);
				/* process current vf */
				kfifo_put(&hevc->pending_q,
				(const struct vframe_s *)vf);
				if (pic->pic_struct == 9) {
					vf->type = VIDTYPE_INTERLACE_TOP
						| nv_order | VIDTYPE_VIU_FIELD;
					process_pending_vframe(hevc,
					hevc->pre_bot_pic, 0);
				} else {
					vf->type = VIDTYPE_INTERLACE_BOTTOM |
						nv_order | VIDTYPE_VIU_FIELD;
					vf->index = (pic->index << 8) | 0xff;
					process_pending_vframe(hevc,
					hevc->pre_top_pic, 1);
				}

				if (atomic_read(&hevc->vf_pre_count) == 0)
					atomic_add(1, &hevc->vf_pre_count);

				/**/
				if (pic->pic_struct == 9)
					hevc->pre_top_pic = pic;
				else
					hevc->pre_bot_pic = pic;
			} else {
				vh265_vf_put(vf, vdec);
				atomic_add(1, &hevc->vf_get_count);
				atomic_add(1, &hevc->vf_pre_count);
				return 0;
			}
		} else if (pic->pic_struct == 11
		    || pic->pic_struct == 12) {

			hevc_print(hevc, H265_DEBUG_PIC_STRUCT,
				"pic_struct = %d index 0x%x\n",
				pic->pic_struct, pic->index);
			pic->vf_ref = 1;

			/* process previous pending vf*/
			process_pending_vframe(hevc, pic, (pic->pic_struct == 11));

			/* put current into pending q */
			if (pic->pic_struct == 11)
				vf->type = VIDTYPE_INTERLACE_TOP |
				nv_order | VIDTYPE_VIU_FIELD;
			else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM |
				nv_order | VIDTYPE_VIU_FIELD;
				vf->index = (pic->index << 8) | 0xff;
			}
			if (pic->show_frame) {
				decoder_do_frame_check(vdec, vf);
				vdec_vframe_ready(vdec, vf);
				kfifo_put(&hevc->pending_q,
				(const struct vframe_s *)vf);
				if (atomic_read(&hevc->vf_pre_count) == 0)
					atomic_add(1, &hevc->vf_pre_count);

				if (pic->pic_struct == 11)
					hevc->pre_top_pic = pic;
				else
					hevc->pre_bot_pic = pic;
			} else {
				vh265_vf_put(vf, vdec);
				atomic_add(1, &hevc->vf_get_count);
				atomic_add(1, &hevc->vf_pre_count);
				return 0;
			}
		} else {
			pic->vf_ref = 1;

			hevc_print(hevc, H265_DEBUG_PIC_STRUCT,
				"pic_struct = %d index 0x%x\n",
				pic->pic_struct, pic->index);

			switch (pic->pic_struct) {
			case 7:
				vf->duration <<= 1;
				break;
			case 8:
				vf->duration = vf->duration * 3;
				break;
			case 1:
				vf->type = VIDTYPE_INTERLACE_TOP |
					nv_order | VIDTYPE_VIU_FIELD;
				process_pending_vframe(hevc, pic, 1);
				hevc->pre_top_pic = pic;
				break;
			case 2:
				vf->type = VIDTYPE_INTERLACE_BOTTOM
					| nv_order | VIDTYPE_VIU_FIELD;
				process_pending_vframe(hevc, pic, 0);
				hevc->pre_bot_pic = pic;
				break;
			}

			if (pic->show_frame) {
				put_vf_to_display_q(hevc, vf);
			} else {
				vh265_vf_put(vf, vdec);
				atomic_add(1, &hevc->vf_get_count);
				atomic_add(1, &hevc->vf_pre_count);
				return 0;
			}
		}
#else
		vf->type_original = vf->type;
		pic->vf_ref = 1;
		put_vf_to_display_q(hevc, vf);
#endif
		ATRACE_COUNTER(hevc->trace.new_q_name, kfifo_len(&hevc->newframe_q));
		ATRACE_COUNTER(hevc->trace.disp_q_name, kfifo_len(&hevc->display_q));
		/*count info*/
		vdec_count_info(hevc->gvs, 0, stream_offset);
		if (pic->slice_type == I_SLICE) {
			hevc->gvs->i_decoded_frames++;
			vf->frame_type |= V4L2_BUF_FLAG_KEYFRAME;
		} else if (pic->slice_type == P_SLICE) {
			hevc->gvs->p_decoded_frames++;
			vf->frame_type |= V4L2_BUF_FLAG_PFRAME;
		} else if (pic->slice_type == B_SLICE) {
			hevc->gvs->b_decoded_frames++;
			vf->frame_type |= V4L2_BUF_FLAG_BFRAME;
		}
		hevc_update_gvs(hevc);
		memcpy(&tmp4x, hevc->gvs, sizeof(struct vdec_info));
		tmp4x.bit_depth_luma = pic->bit_depth_luma;
		tmp4x.bit_depth_chroma = pic->bit_depth_chroma;
		tmp4x.double_write_mode = pic->double_write_mode;
		vdec_fill_vdec_frame(vdec, &hevc->vframe_qos, &tmp4x, vf, pic->hw_decode_time);
		vdec->vdec_fps_detec(vdec->id);
		hevc_print(hevc, H265_DEBUG_BUFMGR,
			"%s(type %d index 0x%x poc %d/%d) pts(%d,%d) dur %d\n",
			__func__, vf->type, vf->index,
			get_pic_poc(hevc, vf->index & 0xff),
			get_pic_poc(hevc, (vf->index >> 8) & 0xff),
			vf->pts, vf->pts_us64,
			vf->duration);
		if (pic->pic_struct == 10 || pic->pic_struct == 12) {
			index = (vf->index >> 8) & 0xff;
		} else {
			index = vf->index & 0xff;
		}
#ifdef AUX_DATA_CRC
		if (index <= MAX_REF_PIC_NUM)
			decoder_do_aux_data_check(vdec, hevc->m_PIC[index]->aux_data_buf,
				hevc->m_PIC[index]->aux_data_size);
#endif

		hevc_print(hevc, H265_DEBUG_PRINT_SEI,
			"aux_data_size:%d signal_type:0x%x sei_present_flag:%d inst_cnt:%d vf:%p\n",
			hevc->m_PIC[index]->aux_data_size, hevc->video_signal_type, hevc->sei_present_flag,
			vdec->inst_cnt, vf);

		if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI) {
			int i = 0;
			PR_INIT(128);
			for (i = 0; i < hevc->m_PIC[index]->aux_data_size; i++) {
				PR_FILL("%02x ", hevc->m_PIC[index]->aux_data_buf[i]);
				if (((i + 1) & 0xf) == 0)
					PR_INFO(hevc->index);
			}
			PR_INFO(hevc->index);
		}

		if ((hevc->m_PIC[index]->aux_data_size == 0) &&
			(pic->slice_type == I_SLICE) &&
			(atomic_read(&hevc->vf_pre_count) == 1)) {
			hevc->no_need_aux_data = true;
		}

		if (hevc->no_need_aux_data) {
			v4l2_ctx->aux_infos.free_one_sei_buffer(v4l2_ctx,
				&hevc->m_PIC[index]->aux_data_buf,
				&hevc->m_PIC[index]->aux_data_size,
				hevc->m_PIC[index]->ctx_buf_idx);
		} else {
			if (!hevc->discard_dv_data)
				v4l2_ctx->aux_infos.bind_dv_buffer(v4l2_ctx, &vf->src_fmt.comp_buf,
					&vf->src_fmt.md_buf);

			update_vframe_src_fmt(vf,
				hevc->m_PIC[index]->aux_data_buf,
				hevc->m_PIC[index]->aux_data_size,
				hevc->dv_duallayer, hevc->provider_name, NULL);
		}

		if (hevc->kpi_first_i_decoded == 0) {
			hevc->kpi_first_i_decoded = 1;
			pr_debug("[vdec_kpi][%s] First I frame decoded.\n",
				__func__);
		}

		if (without_display_mode == 0) {
			if (v4l2_ctx->is_stream_off) {
				vh265_vf_put(vh265_vf_get(vdec), vdec);
			} else {
				if (hevc->send_frame_flag == 1) {
					 while (kfifo_len(&hevc->display_q)) {
						if (hevc->pair_fb[0] != NULL && hevc->pair_fb[1] != NULL) {
							set_meta_data_to_vf(vf, UVM_META_DATA_VF_BASE_INFOS, hevc->v4l2_ctx);
							ATRACE_COUNTER("VC_OUT_DEC-submit", hevc->pair_fb[0]->buf_idx);
							hevc->pair_fb[0]->task->submit(hevc->pair_fb[0]->task, TASK_TYPE_DEC);
							ATRACE_COUNTER("VC_OUT_DEC-submit", hevc->pair_fb[1]->buf_idx);
							hevc->pair_fb[1]->task->submit(hevc->pair_fb[1]->task, TASK_TYPE_DEC);
							clear_pair_fb(hevc);
						} else {
							set_meta_data_to_vf(vf, UVM_META_DATA_VF_BASE_INFOS, hevc->v4l2_ctx);
							ATRACE_COUNTER("VC_OUT_DEC-submit", fb->buf_idx);
							fb->task->submit(fb->task, TASK_TYPE_DEC);
						}
					}
				}
			}
		}
		else
			vh265_vf_put(vh265_vf_get(vdec), vdec);
	}

	return 0;
}

static int post_picture_early(struct vdec_s *vdec, int index)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
	struct PIC_s *pic = hevc->m_PIC[index];

	if (!hevc->enable_fence)
		return 0;

	/* create fence for each buffers. */
	if (vdec_timeline_create_fence(vdec->sync))
		return -1;

	pic->fence		= vdec->sync->fence;
	pic->stream_offset	= READ_VREG(HEVC_SHIFT_BYTE_COUNT);

	if (hevc->chunk) {
		pic->pts	= hevc->chunk->pts;
		pic->pts64	= hevc->chunk->pts64;
		pic->timestamp	= hevc->chunk->timestamp;
	}
	pic->show_frame = true;
	post_video_frame(vdec, pic);

	display_frame_count[hevc->index]++;

	return 0;
}

static int prepare_display_buf(struct vdec_s *vdec, struct PIC_s *frame)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	if (hevc->enable_fence) {
		int i, j, used_size, ret;
		int signed_count = 0;
		struct vframe_s *signed_fence[VF_POOL_SIZE];

		post_prepare_process(vdec, frame);

		if (!frame->show_frame)
			pr_info("do not display.\n");

		hevc->m_PIC[frame->index]->vf_ref = 1;

		/* notify signal to wake up wq of fence. */
		vdec_timeline_increase(vdec->sync, 1);
		mutex_lock(&hevc->fence_mutex);
		used_size = hevc->fence_vf_s.used_size;
		if (used_size) {
			for (i = 0, j = 0; i < VF_POOL_SIZE && j < used_size; i++) {
				if (hevc->fence_vf_s.fence_vf[i] != NULL) {
					ret = dma_fence_get_status(hevc->fence_vf_s.fence_vf[i]->fence);
					if (ret == 1) {
						signed_fence[signed_count] = hevc->fence_vf_s.fence_vf[i];
						hevc->fence_vf_s.fence_vf[i] = NULL;
						hevc->fence_vf_s.used_size--;
						signed_count++;
					}
					j++;
				}
			}
		}
		mutex_unlock(&hevc->fence_mutex);
		if (signed_count != 0) {
			for (i = 0; i < signed_count; i++)
				vh265_vf_put(signed_fence[i], vdec);
		}
		return 0;
	}

	if (post_prepare_process(vdec, frame))
		return -1;

	if (post_video_frame(vdec, frame))
		return -1;

	display_frame_count[hevc->index]++;
	return 0;
}

static bool is_avaliable_buffer(struct hevc_state_s *hevc);

static int notify_v4l_eos(struct vdec_s *vdec)
{
	struct hevc_state_s *hw = (struct hevc_state_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct vframe_s *vf = &hw->vframe_dummy;
	struct vdec_v4l2_buffer *fb = NULL;
	static struct PIC_s *pic = NULL;
	ulong expires;

	if (hw->eos) {
		expires = jiffies + msecs_to_jiffies(2000);
		while (!is_avaliable_buffer(hw)) {
			if (time_after(jiffies, expires)) {
				pr_err("[%d] H265 isn't enough buff for notify eos.\n", ctx->id);
				return 0;
			}
		}

		pic = v4l_get_new_pic(hw, NULL);
		if (NULL == pic) {
			pr_err("[%d] H265 EOS get free buff fail.\n", ctx->id);
			return 0;
		}

		fb = (struct vdec_v4l2_buffer *)
			pic->cma_alloc_addr;

		vf->type		|= VIDTYPE_V4L_EOS;
		vf->timestamp		= ULONG_MAX;
		vf->flag		= VFRAME_FLAG_EMPTY_FRAME_V4L;
		vf->v4l_mem_handle	= (ulong)fb;

		vdec_vframe_ready(vdec, vf);
		kfifo_put(&hw->display_q, (const struct vframe_s *)vf);

		ATRACE_COUNTER("VC_OUT_DEC-submit", fb->buf_idx);
		fb->task->submit(fb->task, TASK_TYPE_DEC);

		pr_info("[%d] H265 EOS notify.\n", ctx->id);
	}

	return 0;
}

static void process_nal_sei(struct hevc_state_s *hevc,
	int payload_type, int payload_size)
{
	unsigned short data;

	if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI)
		hevc_print(hevc, 0,
			"\tsei message: payload_type = 0x%02x, payload_size = 0x%02x\n",
			payload_type, payload_size);

	if (payload_type == 137) {
		int i, j;
		/* MASTERING_DISPLAY_COLOUR_VOLUME */
		if (payload_size >= 24) {
			if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI)
				hevc_print(hevc, 0,
					"\tsei MASTERING_DISPLAY_COLOUR_VOLUME available\n");
			for (i = 0; i < 3; i++) {
				for (j = 0; j < 2; j++) {
					data = (READ_HREG(HEVC_SHIFTED_DATA) >> 16);
					hevc->primaries[i][j] = data;
					WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|16);
					if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI)
						hevc_print(hevc, 0,
							"\t\tprimaries[%1d][%1d] = %04x\n",
							i, j, hevc->primaries[i][j]);
				}
			}
			for (i = 0; i < 2; i++) {
				data = (READ_HREG(HEVC_SHIFTED_DATA) >> 16);
				hevc->white_point[i] = data;
				WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|16);
				if (get_dbg_flag(hevc) & H265_DEBUG_PRINT_SEI)
					hevc_print(hevc, 0,
						"\t\twhite_point[%1d] = %04x\n",
						i, hevc->white_point[i]);
			}
			for (i = 0; i < 2; i++) {
				data = (READ_HREG(HEVC_SHIFTED_DATA) >> 16);
				hevc->luminance[i] = data << 16;
				WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|16);
				data = (READ_HREG(HEVC_SHIFTED_DATA) >> 16);
				hevc->luminance[i] |= data;
				WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|16);
				if (get_dbg_flag(hevc) &
					H265_DEBUG_PRINT_SEI)
					hevc_print(hevc, 0,
						"\t\tluminance[%1d] = %08x\n",
						i, hevc->luminance[i]);
			}
			hevc->sei_present_flag |= SEI_MASTER_DISPLAY_COLOR_MASK;
		}
		payload_size -= 24;
		while (payload_size > 0) {
			data = (READ_HREG(HEVC_SHIFTED_DATA) >> 24);
			payload_size--;
			WRITE_HREG(HEVC_SHIFT_COMMAND, (1<<7)|8);
			hevc_print(hevc, 0, "\t\tskip byte %02x\n", data);
		}
	}
}

static int hevc_recover(struct hevc_state_s *hevc)
{
	int ret = -1;
	u32 rem;
	u64 shift_byte_count64;
	unsigned int hevc_shift_byte_count;
	unsigned int hevc_stream_start_addr;
	unsigned int hevc_stream_end_addr;
	unsigned int hevc_stream_rd_ptr;
	unsigned int hevc_stream_wr_ptr;
	unsigned int hevc_stream_control;
	unsigned int hevc_stream_fifo_ctl;
	unsigned int hevc_stream_buf_size;
	struct vdec_s *vdec = hw_to_vdec(hevc);

	mutex_lock(&vh265_mutex);

#define ES_VID_MAN_RD_PTR            (1<<0)
	if (!hevc->init_flag) {
		hevc_print(hevc, 0, "h265 has stopped, recover return!\n");
		mutex_unlock(&vh265_mutex);
		return ret;
	}
	amhevc_stop();
	msleep(20);
	ret = 0;
	/* reset */
	if (vdec_stream_based(vdec)) {
		STBUF_WRITE(&vdec->vbuf, set_rp,
			READ_VREG(HEVC_STREAM_RD_PTR));

		if (!vdec->vbuf.no_parser)
			SET_PARSER_REG_MASK(PARSER_ES_CONTROL,
				ES_VID_MAN_RD_PTR);
	}

	hevc_stream_start_addr = READ_VREG(HEVC_STREAM_START_ADDR);
	hevc_stream_end_addr = READ_VREG(HEVC_STREAM_END_ADDR);
	hevc_stream_rd_ptr = READ_VREG(HEVC_STREAM_RD_PTR);
	hevc_stream_wr_ptr = READ_VREG(HEVC_STREAM_WR_PTR);
	hevc_stream_control = READ_VREG(HEVC_STREAM_CONTROL);
	hevc_stream_fifo_ctl = READ_VREG(HEVC_STREAM_FIFO_CTL);
	hevc_stream_buf_size = hevc_stream_end_addr - hevc_stream_start_addr;

	/* HEVC streaming buffer will reset and restart
	 *   from current hevc_stream_rd_ptr position
	 */
	/* calculate HEVC_SHIFT_BYTE_COUNT value with the new position. */
	hevc_shift_byte_count = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
	if ((hevc->shift_byte_count_lo & (1 << 31))
		&& ((hevc_shift_byte_count & (1 << 31)) == 0))
		hevc->shift_byte_count_hi++;

	hevc->shift_byte_count_lo = hevc_shift_byte_count;
	shift_byte_count64 = ((u64)(hevc->shift_byte_count_hi) << 32) |
				hevc->shift_byte_count_lo;
	div_u64_rem(shift_byte_count64, hevc_stream_buf_size, &rem);
	shift_byte_count64 -= rem;
	shift_byte_count64 += hevc_stream_rd_ptr - hevc_stream_start_addr;

	if (rem > (hevc_stream_rd_ptr - hevc_stream_start_addr))
		shift_byte_count64 += hevc_stream_buf_size;

	hevc->shift_byte_count_lo = (u32)shift_byte_count64;
	hevc->shift_byte_count_hi = (u32)(shift_byte_count64 >> 32);

	WRITE_VREG(DOS_SW_RESET3,
			   /* (1<<2)| */
			   (1 << 3) | (1 << 4) | (1 << 8) |
			   (1 << 11) | (1 << 12) | (1 << 14)
			   | (1 << 15) | (1 << 17) | (1 << 18) | (1 << 19));
	WRITE_VREG(DOS_SW_RESET3, 0);

	WRITE_VREG(HEVC_STREAM_START_ADDR, hevc_stream_start_addr);
	WRITE_VREG(HEVC_STREAM_END_ADDR, hevc_stream_end_addr);
	WRITE_VREG(HEVC_STREAM_RD_PTR, hevc_stream_rd_ptr);
	WRITE_VREG(HEVC_STREAM_WR_PTR, hevc_stream_wr_ptr);
	WRITE_VREG(HEVC_STREAM_CONTROL, hevc_stream_control);
	WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, hevc->shift_byte_count_lo);
	WRITE_VREG(HEVC_STREAM_FIFO_CTL, hevc_stream_fifo_ctl);

	hevc_config_work_space_hw(hevc);
	decoder_hw_reset();

	hevc->have_vps = 0;
	hevc->have_sps = 0;
	hevc->have_pps = 0;

	hevc->have_valid_start_slice = 0;

	/*/Enable NV21 reference read mode for MC*/
	if (get_double_write_mode(hevc) & 0x10)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);

	WRITE_VREG(HEVC_WAIT_FLAG, 1);
	/* clear mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_CLR_REG, 1);
	/* enable mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 1);
	/* disable PSCALE for hardware sharing */
	WRITE_VREG(HEVC_PSCALE_CTRL, 0);

	CLEAR_PARSER_REG_MASK(PARSER_ES_CONTROL, ES_VID_MAN_RD_PTR);

	WRITE_VREG(DEBUG_REG1, 0x0);

	if ((error_handle_policy & 1) == 0) {
		if ((error_handle_policy & 4) == 0) {
			/* ucode auto mode, and do not check vps/sps/pps/idr */
			WRITE_VREG(NAL_SEARCH_CTL, 0xc);
		} else {
			WRITE_VREG(NAL_SEARCH_CTL, 0x1);/* manual parser NAL */
		}
	} else {
		WRITE_VREG(NAL_SEARCH_CTL, 0x1);/* manual parser NAL */
	}

	if (get_dbg_flag(hevc) & H265_DEBUG_NO_EOS_SEARCH_DONE)
		WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | 0x10000);
	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL) | ((parser_sei_enable & 0x7) << 17));

	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL) |
		((parser_dolby_vision_enable & 0x1) << 20));

	config_decode_mode(hevc);
	WRITE_VREG(DECODE_STOP_POS, udebug_flag);

	init_pic_list_hw(hevc);

	hevc_print(hevc, 0, "%s HEVC_SHIFT_BYTE_COUNT=0x%x\n", __func__,
		   READ_VREG(HEVC_SHIFT_BYTE_COUNT));

#ifdef SWAP_HEVC_UCODE
	if (!tee_enabled() && hevc->is_swap) {
		WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, hevc->mc_dma_handle);
		/*pr_info("write swap buffer %x\n", (u32)(hevc->mc_dma_handle));*/
	}
#endif
	amhevc_start();

	/* skip, search next start code */
	WRITE_VREG(HEVC_WAIT_FLAG, READ_VREG(HEVC_WAIT_FLAG) & (~0x2));
	hevc->skip_flag = 1;
	WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
	/* Interrupt Amrisc to excute */
	WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);
#ifdef MULTI_INSTANCE_SUPPORT
	if (!hevc->m_ins_flag)
#endif
		hevc->first_pic_after_recover = 1;
	mutex_unlock(&vh265_mutex);
	return ret;
}

static void dump_aux_buf(struct hevc_state_s *hevc)
{
	int i;
	unsigned short *aux_adr = (unsigned short *)hevc->aux_addr;
	unsigned int aux_size = (READ_VREG(HEVC_AUX_DATA_SIZE) >> 16) << 4;
	PR_INIT(128);

	if (hevc->prefix_aux_size > 0) {
		hevc_print(hevc, 0,
			"prefix aux: (size %d)\n",
			aux_size);
		if (aux_size > hevc->prefix_aux_size) {
			hevc_print(hevc, 0,
				"%s:aux_size(%d) is over size\n", __func__, aux_size);
			return ;
		}
		for (i = 0; i < (aux_size >> 1); i++) {
			PR_FILL("%04x ", *(aux_adr + i));
			if (((i + 1) & 0xf) == 0)
				PR_INFO(hevc->index);;
		}
		PR_INFO(hevc->index);;
	}
	if (hevc->suffix_aux_size > 0) {
		aux_adr = (unsigned short *)
			(hevc->aux_addr +
			hevc->prefix_aux_size);
		aux_size =
		(READ_VREG(HEVC_AUX_DATA_SIZE) & 0xffff) << 4;
		hevc_print(hevc, 0,
			"suffix aux: (size %d)\n",
			aux_size);
		if (aux_size > hevc->suffix_aux_size) {
			hevc_print(hevc, 0,
				"%s:aux_size(%d) is over size\n", __func__, aux_size);
			return ;
		}
		for (i = 0; i < (aux_size >> 1); i++) {
			PR_FILL("%04x ", *(aux_adr + i));
			if (((i + 1) & 0xf) == 0)
				PR_INFO(hevc->index);
		}
		PR_INFO(hevc->index);
	}
}

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
static void dolby_get_meta(struct hevc_state_s *hevc)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (get_dbg_flag(hevc) &
		H265_DEBUG_PRINT_SEI)
		dump_aux_buf(hevc);
	if (vdec->dolby_meta_with_el || vdec->slave) {
		set_aux_data(hevc, hevc->cur_pic, 0, 0);
	} else if (vdec->master) {
		struct hevc_state_s *hevc_ba =
			(struct hevc_state_s *)vdec->master->private;
		/*do not use hevc_ba*/
		set_aux_data(hevc,
		hevc_ba->cur_pic, 0, 1);
		set_aux_data(hevc, hevc->cur_pic, 0, 2);
	} else if (vdec_frame_based(vdec)) {
		set_aux_data(hevc, hevc->cur_pic, 1, 0);
	}
}
#endif

static void read_decode_info(struct hevc_state_s *hevc)
{
	uint32_t decode_info =
		READ_HREG(HEVC_DECODE_INFO);
	hevc->start_decoding_flag |=
		(decode_info & 0xff);
	hevc->rps_set_id = (decode_info >> 8) & 0xff;
}

static int vh265_get_ps_info(struct hevc_state_s *hevc,
			     union param_u *rpm_param,
			     struct aml_vdec_ps_infos *ps)
{
	u32 SubWidthC, SubHeightC;
	u32 width = rpm_param->p.pic_width_in_luma_samples;
	u32 height = rpm_param->p.pic_height_in_luma_samples;

	switch (rpm_param->p.chroma_format_idc) {
	case 1:
		SubWidthC = 2;
		SubHeightC = 2;
		break;
	case 2:
		SubWidthC = 2;
		SubHeightC = 1;
		break;
	default:
		SubWidthC = 1;
		SubHeightC = 1;
		break;
	}

	width -= SubWidthC *
		(rpm_param->p.conf_win_left_offset +
		rpm_param->p.conf_win_right_offset);
	height -= SubHeightC *
		(rpm_param->p.conf_win_top_offset +
		rpm_param->p.conf_win_bottom_offset);

	hevc->last_width = rpm_param->p.pic_width_in_luma_samples;
	hevc->last_height = rpm_param->p.pic_height_in_luma_samples;
	hevc->sps_num_reorder_pics_0 =
		rpm_param->p.sps_num_reorder_pics_0;

	height <<= hevc->interlace_flag;
	ps->visible_width 	= width;
	ps->visible_height 	= height;
	ps->coded_width 	= ALIGN(width, 64);
	ps->coded_height 	= ALIGN(height, 64);
	ps->field 		= hevc->interlace_flag ? V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE;
	ps->dpb_frames		= v4l_parser_work_pic_num(hevc);
	ps->dpb_margin		= get_dynamic_buf_num_margin(hevc);

	return 0;
}

static void get_comp_buf_info(struct hevc_state_s *hevc,
		struct vdec_comp_buf_info *info)
{
	u16 bit_depth = hevc->param.p.bit_depth;
	int w = hevc->param.p.pic_width_in_luma_samples;
	int h = hevc->param.p.pic_height_in_luma_samples;
	struct aml_vcodec_ctx * ctx = hevc->v4l2_ctx;

	info->max_size = hevc_max_mmu_buf_size(
			hevc->max_pic_w,
			hevc->max_pic_h);
	info->header_size = hevc_get_header_size(w,h);
	info->frame_buffer_size = hevc_mmu_page_num(
			hevc, w, h,	bit_depth == 0);

	if (info->frame_buffer_size < 0) {
		vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
	}

	pr_info("hevc get comp info: %d %d %d\n",
			info->max_size, info->header_size,
			info->frame_buffer_size);
}

static int is_interlace(struct hevc_state_s *hevc)
{
	int pic_struct = (hevc->param.p.sei_frame_field_info >> 3) & 0xf;
	int frame_field_info_present_flag =
			(hevc->param.p.sei_frame_field_info >> 8) & 0x1;

	if ((hevc->param.p.profile_etc & 0xc) == 0x4
		&& (frame_field_info_present_flag
		&& (pic_struct == 0
		|| pic_struct == 7
		|| pic_struct == 8)))
		return 0;
	else if ((hevc->param.p.profile_etc & 0xc) == 0x4)
		return 1;

	return 0;
}

static void hevc_interlace_check(struct hevc_state_s *hevc,
	union param_u *rpm_param)
{
	int w, h;

	w = hevc->param.p.pic_width_in_luma_samples;
	h = hevc->param.p.pic_height_in_luma_samples;
	/* interlace check, 4k force no interlace */
	if ((interlace_enable != 0) &&
		(!IS_4K_SIZE(w, h)) &&
		(is_interlace(hevc))) {
		hevc->interlace_flag = 1;
		hevc->frame_ar = (hevc->pic_h * 0x100 / hevc->pic_w) * 2;
		hevc_print(hevc, 0,
			"interlace (%d, %d), profile_etc %x, ar 0x%x, dw %d\n",
			hevc->pic_w, hevc->pic_h, hevc->param.p.profile_etc, hevc->frame_ar,
			get_double_write_mode(hevc));
	}
}

static int v4l_res_change(struct hevc_state_s *hevc, union param_u *rpm_param)
{
	struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	int i, ret = 0;

	if (ctx->param_sets_from_ucode) {
		struct aml_vdec_ps_infos ps;
		int width = rpm_param->p.pic_width_in_luma_samples;
		int height = rpm_param->p.pic_height_in_luma_samples;

		if ((hevc->last_width != 0 &&
			hevc->last_height != 0) &&
			(hevc->last_width != width ||
			hevc->last_height != height)) {
			hevc_print(hevc, 0,
				"v4l_res_change Pic Width/Height Change (%d,%d)=>(%d,%d), interlace %d\n",
				hevc->last_width, hevc->last_height,
				width,
				height,
				hevc->interlace_flag);

			if (get_valid_double_write_mode(hevc) != 16) {
				struct vdec_comp_buf_info info;

				get_comp_buf_info(hevc, &info);
				vdec_v4l_set_comp_buf_info(ctx, &info);
			}
			vh265_get_ps_info(hevc, &hevc->param, &ps);
			vdec_v4l_set_ps_infos(ctx, &ps);
			vdec_v4l_res_ch_event(ctx);
			hevc->v4l_params_parsed = false;
			ctx->v4l_resolution_change = 1;
			hevc->eos = 1;
			hevc->resolution_change = true;

			/*
			 * marks frame valid on the dpb is the ouput state,
			 * then via flush_output all frames can be flushed out.
			 */
			for (i = 0; i < MAX_REF_PIC_NUM; ++i) {
				if ((hevc->m_PIC[i] == NULL) ||
					(hevc->m_PIC[i]->index == -1) ||
					(hevc->m_PIC[i]->BUF_index == -1))
					continue;

				if ((hevc->m_PIC[i]->POC != INVALID_POC) &&
					(hevc->m_PIC[i]->output_ready == 0) &&
					hevc->m_PIC[i]->referenced &&
					(hevc->m_PIC[i]->POC >= hevc->decoded_poc)) {
						hevc->m_PIC[i]->output_mark = 1;
				}
			}

			flush_output(hevc, NULL);
			ATRACE_COUNTER("V_ST_DEC-submit_eos", __LINE__);
			notify_v4l_eos(hw_to_vdec(hevc));
			ATRACE_COUNTER("V_ST_DEC-submit_eos", 0);

			ret = 1;
		}
	}

	return ret;
}

#ifdef H265_USERDATA_ENABLE
static int userdata_prepare(struct hevc_state_s *hevc)
{
	struct PIC_s *pic = hevc->cur_pic;
	char *p;
	u32 size;
	int type;
	u32 vpts = 0;
	u64 pts64 = 0;
	int pts_valid = 0;

	if (!itu_t_t35_enable || pic == NULL)
		return 0;

	if (pic->aux_data_buf
	&& pic->aux_data_size) {
		/* parser sei */
		p = pic->aux_data_buf;
		while (p < pic->aux_data_buf
			+ pic->aux_data_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (type == 0x02000000) {
				/* hevc_print(hevc, 0,
				"sei(%d)\n", size); */
				parse_sei(hevc, pic, p, size);
			}
			p += size;
		}
		if (vdec_frame_based(hw_to_vdec(hevc))) {
			if (hevc->chunk) {
				vpts = hevc->chunk->pts;
				pts_valid = hevc->chunk->pts_valid;
			}
		} else {
			if (pts_pickout_offset_us64(PTS_TYPE_VIDEO,
					pic->stream_offset, &vpts, 0, &pts64)) {
				vpts = 0;
				pts_valid = 0;
			}
		}
		vh265_userdata_fill_vpts(hevc, vpts, pts_valid, pic->POC);
	}

	return 0;
}
#endif

static int hevc_skip_nal(struct hevc_state_s *hevc)
{
	if ((hevc->pic_h == 96) && (hevc->pic_w  == 160) &&
		(get_double_write_mode(hevc) == 0x10)) {
		if (get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_TXLX) {
			if (hevc->skip_nal_count < skip_nal_count)
				return 1;
		} else {
			if (hevc->skip_nal_count < 1)
				return 1;
		}
	}
	return 0;
}

static void aspect_ratio_set(struct hevc_state_s *hevc, u32 *out_frame_ar,
	u32 *out_sar_height, u32 *out_sar_width)
{
	int aspect_ratio_idc = hevc->param.p.aspect_ratio_idc;
	u32 frame_ar, sar_height, sar_width;

	switch (aspect_ratio_idc) {
		case 1:
			frame_ar = 0x3ff;
			sar_height = 1;
			sar_width = 1;
			break;
		case 2:
			frame_ar = 0x3ff;
			sar_height = 11;
			sar_width = 12;
			break;
		case 3:
			frame_ar = 0x3ff;
			sar_height = 11;
			sar_width = 10;
			break;
		case 4:
			frame_ar = 0x3ff;
			sar_height = 11;
			sar_width = 16;
			break;
		case 5:
			frame_ar = 0x3ff;
			sar_height = 33;
			sar_width = 40;
			break;
		case 6:
			frame_ar = 0x3ff;
			sar_height = 11;
			sar_width = 24;
			break;
		case 7:
			frame_ar = 0x3ff;
			sar_height = 11;
			sar_width = 20;
			break;
		case 8:
			frame_ar = 0x3ff;
			sar_height = 11;
			sar_width = 32;
			break;
		case 9:
			frame_ar = 0x3ff;
			sar_height = 33;
			sar_width = 80;
			break;
		case 10:
			frame_ar = 0x3ff;
			sar_height = 11;
			sar_width = 18;
			break;
		case 11:
			frame_ar = 0x3ff;
			sar_height = 11;
			sar_width = 15;
			break;
		case 12:
			frame_ar = 0x3ff;
			sar_height = 33;
			sar_width = 64;
			break;
		case 13:
			frame_ar = 0x3ff;
			sar_height = 99;
			sar_width = 160;
			break;
		case 14:
			frame_ar = 0x3ff;
			sar_height = 3;
			sar_width = 4;
			break;
		case 15:
			frame_ar = 0x3ff;
			sar_height = 2;
			sar_width = 3;
			break;
		case 16:
			frame_ar = 0x3ff;
			sar_height = 1;
			sar_width = 2;
			break;
		case 0xff:
			frame_ar = 0x3ff;
			sar_height = hevc->param.p.sar_height;
			sar_width = hevc->param.p.sar_width;
			break;
		default:
			frame_ar = 0x3ff;
			sar_height = 1;
			sar_width = 1;
			break;
	}

	(*out_frame_ar) = frame_ar;
	(*out_sar_height) = sar_height;
	(*out_sar_width) = sar_width;

	return;
}

static irqreturn_t vh265_isr_thread_fn(int irq, void *data)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *) data;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	unsigned int dec_status = hevc->dec_status;
	int i, ret;

	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (dec_status == HEVC_SLICE_SEGMENT_DONE) {
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_START);
	}
	else if (dec_status == HEVC_DECPIC_DATA_DONE) {
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_THREAD_PIC_DONE_START);
	}

	if (hevc->eos)
		return IRQ_HANDLED;
	if (
#ifdef MULTI_INSTANCE_SUPPORT
		(!hevc->m_ins_flag) &&
#endif
		hevc->error_flag == 1) {
		if ((error_handle_policy & 0x10) == 0) {
			if (hevc->cur_pic) {
				int current_lcu_idx = READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff;
				if (current_lcu_idx < ((hevc->lcu_x_num*hevc->lcu_y_num)-1))
					hevc->cur_pic->error_mark = 1;
			}
		}
		if ((error_handle_policy & 1) == 0) {
			hevc->error_skip_nal_count = 1;
			/* manual search nal, skip  error_skip_nal_count
			 *   of nal and trigger the HEVC_NAL_SEARCH_DONE irq
			 */
			WRITE_VREG(NAL_SEARCH_CTL,
					   (error_skip_nal_count << 4) | 0x1);
		} else {
			hevc->error_skip_nal_count = error_skip_nal_count;
			WRITE_VREG(NAL_SEARCH_CTL, 0x1);/* manual parser NAL */
		}
		if ((get_dbg_flag(hevc) & H265_DEBUG_NO_EOS_SEARCH_DONE)
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			|| vdec->master
			|| vdec->slave
#endif
			) {
			WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | 0x10000);
		}
		WRITE_VREG(NAL_SEARCH_CTL,
			READ_VREG(NAL_SEARCH_CTL)
			| ((parser_sei_enable & 0x7) << 17));

		WRITE_VREG(NAL_SEARCH_CTL,
			READ_VREG(NAL_SEARCH_CTL) |
			((parser_dolby_vision_enable & 0x1) << 20));

		config_decode_mode(hevc);
		/* search new nal */
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
		/* Interrupt Amrisc to excute */
		WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);

		hevc->error_flag = 2;
		return IRQ_HANDLED;
	} else if (
#ifdef MULTI_INSTANCE_SUPPORT
		(!hevc->m_ins_flag) &&
#endif
		hevc->error_flag == 3) {
		hevc_print(hevc, 0, "error_flag=3, hevc_recover\n");
		hevc_recover(hevc);
		hevc->error_flag = 0;

		if ((error_handle_policy & 0x10) == 0) {
			if (hevc->cur_pic) {
				int current_lcu_idx = READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff;
				if (current_lcu_idx <
					((hevc->lcu_x_num*hevc->lcu_y_num)-1))
					hevc->cur_pic->error_mark = 1;

			}
		}
		if ((error_handle_policy & 1) == 0) {
			/* need skip some data when
			 *   error_flag of 3 is triggered,
			 */
			/* to avoid hevc_recover() being called
			 *   for many times at the same bitstream position
			 */
			hevc->error_skip_nal_count = 1;
			/* manual search nal, skip  error_skip_nal_count
			 *   of nal and trigger the HEVC_NAL_SEARCH_DONE irq
			 */
			WRITE_VREG(NAL_SEARCH_CTL,
					   (error_skip_nal_count << 4) | 0x1);
		}

		if ((error_handle_policy & 0x2) == 0) {
			hevc->have_vps = 1;
			hevc->have_sps = 1;
			hevc->have_pps = 1;
		}
		return IRQ_HANDLED;
	}
	if (!hevc->m_ins_flag) {
		i = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
		if ((hevc->shift_byte_count_lo & (1 << 31))
			&& ((i & (1 << 31)) == 0))
			hevc->shift_byte_count_hi++;
		hevc->shift_byte_count_lo = i;
	}
#ifdef MULTI_INSTANCE_SUPPORT
	mutex_lock(&hevc->chunks_mutex);
	if ((dec_status == HEVC_DECPIC_DATA_DONE ||
		dec_status == HEVC_FIND_NEXT_PIC_NAL ||
		dec_status == HEVC_FIND_NEXT_DVEL_NAL)
		&& (hevc->chunk)) {
		hevc->cur_pic->pts = hevc->chunk->pts;
		hevc->cur_pic->pts64 = hevc->chunk->pts64;
		hevc->cur_pic->timestamp = hevc->chunk->timestamp;
	}
	mutex_unlock(&hevc->chunks_mutex);

	if (dec_status == HEVC_DECODE_BUFEMPTY ||
		dec_status == HEVC_DECODE_BUFEMPTY2) {
		if (hevc->m_ins_flag) {
			read_decode_info(hevc);
			if (vdec_frame_based(hw_to_vdec(hevc))) {
				hevc->empty_flag = 1;
				/*suffix sei or dv meta*/
				set_aux_data(hevc, hevc->cur_pic, 1, 0);
				goto pic_done;
			} else {
				if (
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
					vdec->master ||
					vdec->slave ||
#endif
					(data_resend_policy & 0x1)) {
					hevc->dec_result = DEC_RESULT_AGAIN;
					amhevc_stop();
					restore_decode_state(hevc);
				} else
					hevc->dec_result = DEC_RESULT_GET_DATA;
			}
			reset_process_time(hevc);
			vdec_schedule_work(&hevc->work);
		}
		return IRQ_HANDLED;
	} else if ((dec_status == HEVC_SEARCH_BUFEMPTY) ||
		(dec_status == HEVC_NAL_DECODE_DONE)) {
		if (hevc->m_ins_flag) {
			read_decode_info(hevc);
			if (vdec_frame_based(hw_to_vdec(hevc))) {
				/* Ucode multiplexes HEVC_ASSIST_SCRATCH_4 to output dual layer flags.
				 * In the decoder driver, the bit0 of the register is read to
				 * determine whether the DV stream is a dual layer stream
				 */
				bool dv_duallayer = READ_VREG(HEVC_ASSIST_SCRATCH_4) & 0x1;
				if ((!hevc->discard_dv_data) && (!hevc->dv_duallayer)
					&& (dv_duallayer)) {
					hevc->dv_duallayer = true;
					hevc_print(hevc, 0, "dv dual layer\n");
				}
				hevc->empty_flag = 1;
				/*suffix sei or dv meta*/
				set_aux_data(hevc, hevc->cur_pic, 1, 0);
				goto pic_done;
			} else {
				hevc->dec_result = DEC_RESULT_AGAIN;
				amhevc_stop();
				restore_decode_state(hevc);
			}

			reset_process_time(hevc);
			vdec_schedule_work(&hevc->work);
		}

		return IRQ_HANDLED;
	} else if (dec_status == HEVC_DECPIC_DATA_DONE) {
		if (hevc->m_ins_flag) {
			struct PIC_s *pic;
			struct PIC_s *pic_display;
			int decoded_poc;

			if (vdec->mvfrm)
				vdec->mvfrm->hw_decode_time =
				local_clock() - vdec->mvfrm->hw_decode_start;
#ifdef DETREFILL_ENABLE
			if (hevc->is_swap &&
				get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
				if (hevc->detbuf_adr_virt && hevc->delrefill_check
					&& READ_VREG(HEVC_SAO_DBG_MODE0))
					hevc->delrefill_check = 2;
			}
#endif
			hevc->empty_flag = 0;
pic_done:
			if (vdec->master == NULL && vdec->slave == NULL &&
				hevc->empty_flag == 0) {
				hevc->over_decode =
					(READ_VREG(HEVC_SHIFT_STATUS) >> 15) & 0x1;
				if (hevc->over_decode)
					hevc_print(hevc, 0,
						"!!!Over decode %d\n", __LINE__);
			}
			if (input_frame_based(hw_to_vdec(hevc)) &&
				frmbase_cont_bitlevel != 0 &&
				(hevc->decode_size > READ_VREG(HEVC_SHIFT_BYTE_COUNT)) &&
				(hevc->decode_size - (READ_VREG(HEVC_SHIFT_BYTE_COUNT))
				 >	frmbase_cont_bitlevel)) {
				 check_pic_decoded_error(hevc, READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff);
				/*handle the case: multi pictures in one packet*/
				hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
					"%s  has more data index= %d, size=0x%x shiftcnt=0x%x)\n",
					__func__,
					hevc->decode_idx, hevc->decode_size,
					READ_VREG(HEVC_SHIFT_BYTE_COUNT));
					WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
					start_process_time(hevc);
				return IRQ_HANDLED;
			}

			read_decode_info(hevc);
			get_picture_qos_info(hevc);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			hevc->start_parser_type = 0;
			hevc->switch_dvlayer_flag = 0;
#endif
			hevc->decoded_poc = hevc->curr_POC;
			hevc->decoding_pic = NULL;
			hevc->dec_result = DEC_RESULT_DONE;
#ifdef H265_USERDATA_ENABLE
			userdata_prepare(hevc);
#endif
#ifdef DETREFILL_ENABLE
			if (hevc->is_swap &&
				get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM)
				if (hevc->delrefill_check != 2)
#endif

			amhevc_stop();

			reset_process_time(hevc);

			if ((!input_stream_based(vdec) &&
					atomic_read(&hevc->vf_pre_count) == 0) || hevc->ip_mode) {
				decoded_poc = hevc->curr_POC;
				pic = get_pic_by_POC(hevc, decoded_poc);
				if (pic && (pic->POC != INVALID_POC)) {
					/*PB skip control */
					if (pic->error_mark == 0
							&& hevc->PB_skip_mode == 1) {
						/* start decoding after
						 *   first I
						 */
						hevc->ignore_bufmgr_error |= 0x1;
					}
					if (hevc->ignore_bufmgr_error & 1) {
						if (hevc->PB_skip_count_after_decoding > 0) {
							hevc->PB_skip_count_after_decoding--;
						} else {
							/* start displaying */
							hevc->ignore_bufmgr_error |= 0x2;
						}
					}
					if (hevc->mmu_enable
							&& ((get_double_write_mode(hevc) & 0x10) == 0)) {
						if (!hevc->m_ins_flag) {
							hevc->used_4k_num =
							READ_VREG(HEVC_SAO_MMU_STATUS) >> 16;

							if ((!is_skip_decoding(hevc, pic)) &&
								(hevc->used_4k_num >= 0) &&
								(hevc->cur_pic->scatter_alloc
								== 1))
								recycle_mmu_buf_tail(hevc, false);
						}
					}

					pic->output_mark = 1;
					pic->recon_mark = 1;
					if (vdec->mvfrm) {
						pic->frame_size =
							vdec->mvfrm->frame_size;
						pic->hw_decode_time =
						(u32)vdec->mvfrm->hw_decode_time;
					}
				}
				/*Detects the first frame whether has an over decode error*/
				if (vdec->master == NULL && vdec->slave == NULL &&
					hevc->empty_flag == 0) {
					hevc->over_decode =
						(READ_VREG(HEVC_SHIFT_STATUS) >> 15) & 0x1;
					if (hevc->over_decode)
						hevc_print(hevc, 0,
							"!!!Over decode %d\n", __LINE__);
				}
				check_pic_decoded_error(hevc,
					READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff);
				if (hevc->cur_pic != NULL &&
					(READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff) == 0
					&& (hevc->lcu_x_num * hevc->lcu_y_num != 1))
					hevc->cur_pic->error_mark = 1;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
force_output:
#endif
				pic_display = output_pic(hevc, 1);

				if (pic_display) {
					if ((pic_display->error_mark &&
						((hevc->ignore_bufmgr_error &
								  0x2) == 0))
						|| (get_dbg_flag(hevc) &
							H265_DEBUG_DISPLAY_CUR_FRAME)
						|| (get_dbg_flag(hevc) &
							H265_DEBUG_NO_DISPLAY)) {
						pic_display->output_ready = 0;
						if (get_dbg_flag(hevc) &
							H265_DEBUG_BUFMGR) {
							hevc_print(hevc, H265_DEBUG_BUFMGR,
							"[BM] Display: POC %d, ",
								 pic_display->POC);
							hevc_print_cont(hevc, 0,
							"decoding index %d ==> ",
								 pic_display->
								 decode_idx);
							hevc_print_cont(hevc, 0,
							"Debug or err,recycle it\n");
						}
					} else {
						if ((pic_display->
						slice_type != 2) && !pic_display->ip_mode) {
						pic_display->output_ready = 0;
						} else {
							prepare_display_buf
								(hw_to_vdec(hevc),
								 pic_display);
							hevc->first_pic_flag = 1;
						}
					}
				}
			}
			ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_THREAD_EDN);
			vdec_schedule_work(&hevc->work);
		}

		return IRQ_HANDLED;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	} else if (dec_status == HEVC_FIND_NEXT_PIC_NAL ||
		dec_status == HEVC_FIND_NEXT_DVEL_NAL) {
		if (hevc->m_ins_flag) {
			unsigned char next_parser_type =
					READ_HREG(CUR_NAL_UNIT_TYPE) & 0xff;
			read_decode_info(hevc);

			if (vdec->slave &&
				dec_status == HEVC_FIND_NEXT_DVEL_NAL) {
				/*cur is base, found enhance*/
				struct hevc_state_s *hevc_el =
				(struct hevc_state_s *)
					vdec->slave->private;
				hevc->switch_dvlayer_flag = 1;
				hevc->no_switch_dvlayer_count = 0;
				hevc_el->start_parser_type =
					next_parser_type;
				hevc_print(hevc, H265_DEBUG_DV,
					"switch (poc %d) to el\n",
					hevc->cur_pic ?
					hevc->cur_pic->POC :
					INVALID_POC);
			} else if (vdec->master &&
				dec_status == HEVC_FIND_NEXT_PIC_NAL) {
				/*cur is enhance, found base*/
				struct hevc_state_s *hevc_ba =
				(struct hevc_state_s *)
					vdec->master->private;
				hevc->switch_dvlayer_flag = 1;
				hevc->no_switch_dvlayer_count = 0;
				hevc_ba->start_parser_type =
					next_parser_type;
				hevc_print(hevc, H265_DEBUG_DV,
					"switch (poc %d) to bl\n",
					hevc->cur_pic ?
					hevc->cur_pic->POC :
					INVALID_POC);
			} else {
				hevc->switch_dvlayer_flag = 0;
				hevc->start_parser_type =
					next_parser_type;
				hevc->no_switch_dvlayer_count++;
				hevc_print(hevc, H265_DEBUG_DV,
					"%s: no_switch_dvlayer_count = %d\n",
					vdec->master ? "el" : "bl",
					hevc->no_switch_dvlayer_count);
				if (vdec->slave &&
					dolby_el_flush_th != 0 &&
					hevc->no_switch_dvlayer_count >
					dolby_el_flush_th) {
					struct hevc_state_s *hevc_el =
					(struct hevc_state_s *)
					vdec->slave->private;
					struct PIC_s *el_pic;
					check_pic_decoded_error(hevc_el,
					hevc_el->pic_decoded_lcu_idx);
					el_pic = get_pic_by_POC(hevc_el,
						hevc_el->curr_POC);
					hevc_el->curr_POC = INVALID_POC;
					hevc_el->m_pocRandomAccess = MAX_INT;
					flush_output(hevc_el, el_pic);
					hevc_el->decoded_poc = INVALID_POC; /*
					already call flush_output*/
					hevc_el->decoding_pic = NULL;
					hevc->no_switch_dvlayer_count = 0;
					if (get_dbg_flag(hevc) & H265_DEBUG_DV)
						hevc_print(hevc, 0,
						"no el anymore, flush_output el\n");
				}
			}
			hevc->decoded_poc = hevc->curr_POC;
			hevc->decoding_pic = NULL;
			hevc->dec_result = DEC_RESULT_DONE;
			amhevc_stop();
			reset_process_time(hevc);
			if (aux_data_is_avaible(hevc))
				dolby_get_meta(hevc);
			if(hevc->cur_pic && hevc->cur_pic->slice_type == 2 &&
				atomic_read(&hevc->vf_pre_count) == 0) {
				hevc_print(hevc, 0,
						"first slice_type %x no_switch_dvlayer_count %x\n",
						hevc->cur_pic->slice_type,
						hevc->no_switch_dvlayer_count);
				goto  force_output;
			}
			vdec_schedule_work(&hevc->work);
		}

		return IRQ_HANDLED;
#endif
	}

#endif

	if (dec_status == HEVC_SEI_DAT) {
		if (!hevc->m_ins_flag) {
			int payload_type =
				READ_HREG(CUR_NAL_UNIT_TYPE) & 0xffff;
			int payload_size =
				(READ_HREG(CUR_NAL_UNIT_TYPE) >> 16) & 0xffff;
				process_nal_sei(hevc,
					payload_type, payload_size);
		}
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_SEI_DAT_DONE);
	} else if (dec_status == HEVC_NAL_SEARCH_DONE) {
		int naltype = READ_HREG(CUR_NAL_UNIT_TYPE);
		int parse_type = HEVC_DISCARD_NAL;

		hevc->error_watchdog_count = 0;
		hevc->error_skip_nal_wt_cnt = 0;
#ifdef MULTI_INSTANCE_SUPPORT
		if (hevc->m_ins_flag)
			reset_process_time(hevc);
#endif
		if (slice_parse_begin > 0 &&
			get_dbg_flag(hevc) & H265_DEBUG_DISCARD_NAL) {
			hevc_print(hevc, 0,
				"nal type %d, discard %d\n", naltype,
				slice_parse_begin);
			if (naltype <= NAL_UNIT_CODED_SLICE_CRA)
				slice_parse_begin--;
		}
		if (naltype == NAL_UNIT_EOS) {
			struct PIC_s *pic;
			bool eos_in_head = false;

			hevc_print(hevc, 0, "get NAL_UNIT_EOS, flush output\n");
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if ((!hevc->discard_dv_data) && aux_data_is_avaible(hevc)) {
				if (hevc->decoding_pic)
					dolby_get_meta(hevc);
			}
#endif
			/*Detects frame whether has an over decode error*/
			if (vdec->master == NULL && vdec->slave == NULL &&
					hevc->empty_flag == 0 && input_stream_based(vdec)) {
					hevc->over_decode =
						(READ_VREG(HEVC_SHIFT_STATUS) >> 15) & 0x1;
					if (hevc->over_decode)
						hevc_print(hevc, 0,
							"!!!Over decode %d\n", __LINE__);
			}
			check_pic_decoded_error(hevc,
				hevc->pic_decoded_lcu_idx);
			pic = get_pic_by_POC(hevc, hevc->curr_POC);
			hevc->curr_POC = INVALID_POC;
			/* add to fix RAP_B_Bossen_1 */
			hevc->m_pocRandomAccess = MAX_INT;
			flush_output(hevc, pic);
			clear_poc_flag(hevc);
			if (input_frame_based(vdec)) {
				u32 shiftbyte = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
				if (shiftbyte < 0x8 && (hevc->decode_size - shiftbyte) > 0x100) {
					hevc_print(hevc, 0," shiftbytes 0x%x  decode_size 0x%x\n", shiftbyte, hevc->decode_size);
					eos_in_head = true;
				}
			}
			WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_DISCARD_NAL);
			/* Interrupt Amrisc to excute */
			WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);

			/* eos is in the head of the chunk and followed by sps/pps/IDR
			  * so need to go on decoding
			  */
			if (eos_in_head)
				return IRQ_HANDLED;

#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag) {
				hevc->decoded_poc = INVALID_POC; /*
					already call flush_output*/
				hevc->decoding_pic = NULL;
				hevc->dec_result = DEC_RESULT_DONE;
				amhevc_stop();

				vdec_schedule_work(&hevc->work);
			}
#endif
			return IRQ_HANDLED;
		}

		if (
#ifdef MULTI_INSTANCE_SUPPORT
			(!hevc->m_ins_flag) &&
#endif
			hevc->error_skip_nal_count > 0) {
			hevc_print(hevc, 0,
				"nal type %d, discard %d\n", naltype,
				hevc->error_skip_nal_count);
			hevc->error_skip_nal_count--;
			if (hevc->error_skip_nal_count == 0) {
				hevc_recover(hevc);
				hevc->error_flag = 0;
				if ((error_handle_policy & 0x2) == 0) {
					hevc->have_vps = 1;
					hevc->have_sps = 1;
					hevc->have_pps = 1;
				}
				return IRQ_HANDLED;
			}
		} else if (naltype == NAL_UNIT_VPS) {
				parse_type = HEVC_NAL_UNIT_VPS;
				hevc->have_vps = 1;
		} else if (hevc->have_vps) {
			if (naltype == NAL_UNIT_SPS) {
				parse_type = HEVC_NAL_UNIT_SPS;
				hevc->have_sps = 1;
			} else if (naltype == NAL_UNIT_PPS) {
				parse_type = HEVC_NAL_UNIT_PPS;
				hevc->have_pps = 1;
			} else if (hevc->have_sps && hevc->have_pps) {
				int seg = HEVC_NAL_UNIT_CODED_SLICE_SEGMENT;

				if ((naltype == NAL_UNIT_CODED_SLICE_IDR) ||
					(naltype == NAL_UNIT_CODED_SLICE_IDR_N_LP) ||
					(naltype == NAL_UNIT_CODED_SLICE_CRA) ||
					(naltype == NAL_UNIT_CODED_SLICE_BLA) ||
					(naltype == NAL_UNIT_CODED_SLICE_BLANT) ||
					(naltype == NAL_UNIT_CODED_SLICE_BLA_N_LP)) {
					if (slice_parse_begin > 0) {
						hevc_print(hevc, 0, "discard %d, for debugging\n", slice_parse_begin);
						slice_parse_begin--;
					} else {
						parse_type = seg;
					}
					hevc->have_valid_start_slice = 1;
				} else if (naltype <= NAL_UNIT_CODED_SLICE_CRA &&
					(hevc->have_valid_start_slice ||
					(hevc->PB_skip_mode != 3))) {
					if (slice_parse_begin > 0) {
						hevc_print(hevc, 0, "discard %d, dd\n", slice_parse_begin);
						slice_parse_begin--;
					} else
						parse_type = seg;

				}
			}
		}
		if (hevc->have_vps && hevc->have_sps && hevc->have_pps
			&& hevc->have_valid_start_slice &&
			hevc->error_flag == 0) {
			if ((get_dbg_flag(hevc) &
				H265_DEBUG_MAN_SEARCH_NAL) == 0) {
				WRITE_VREG(NAL_SEARCH_CTL, 0x2);
			}

			if ((get_dbg_flag(hevc) &
				H265_DEBUG_NO_EOS_SEARCH_DONE)
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
				|| vdec->master
				|| vdec->slave
#endif
				) {
				WRITE_VREG(NAL_SEARCH_CTL,
						READ_VREG(NAL_SEARCH_CTL) |
						0x10000);
			}
			WRITE_VREG(NAL_SEARCH_CTL,
				READ_VREG(NAL_SEARCH_CTL)
				| ((parser_sei_enable & 0x7) << 17));
/*#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION*/
			WRITE_VREG(NAL_SEARCH_CTL,
				READ_VREG(NAL_SEARCH_CTL) |
				((parser_dolby_vision_enable & 0x1) << 20));
/*#endif*/
			config_decode_mode(hevc);
		}

		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
				"naltype = %d  parse_type %d\n %d %d %d %d\n",
				naltype, parse_type, hevc->have_vps,
				hevc->have_sps, hevc->have_pps,
				hevc->have_valid_start_slice);
		}

		WRITE_VREG(HEVC_DEC_STATUS_REG, parse_type);
		/* Interrupt Amrisc to excute */
		WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);
#ifdef MULTI_INSTANCE_SUPPORT
		if (hevc->m_ins_flag)
			start_process_time(hevc);
#endif
	} else if (dec_status == HEVC_SLICE_SEGMENT_DONE) {
#ifdef MULTI_INSTANCE_SUPPORT
		if (hevc->m_ins_flag) {
			reset_process_time(hevc);
			read_decode_info(hevc);
		}
#endif
		if (hevc->start_decoding_time > 0) {
			u32 process_time = 1000 *
				(jiffies - hevc->start_decoding_time)/HZ;
			if (process_time > max_decoding_time)
				max_decoding_time = process_time;
		}

		hevc->error_watchdog_count = 0;
		if (hevc->pic_list_init_flag == 2) {
			hevc->pic_list_init_flag = 3;
			hevc_print(hevc, 0, "set pic_list_init_flag to 3\n");
			if (hevc->kpi_first_i_comming == 0) {
				hevc->kpi_first_i_comming = 1;
				pr_debug("[vdec_kpi][%s] First I frame coming.\n",
					__func__);
			}
		} else if (hevc->wait_buf == 0) {
			u32 vui_time_scale;
			u32 vui_num_units_in_tick;
			unsigned char reconfig_flag = 0;

			if (get_dbg_flag(hevc) & H265_DEBUG_SEND_PARAM_WITH_REG)
				get_rpm_param(&hevc->param);
			else {
				ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_RPM_START);
				for (i = 0; i < (RPM_END - RPM_BEGIN); i += 4) {
					int ii;

					for (ii = 0; ii < 4; ii++) {
						hevc->param.l.data[i + ii] = hevc->rpm_ptr[i + 3 - ii];
					}
				}
				ATRACE_COUNTER(hevc->trace.decode_header_memory_time_name, TRACE_HEADER_RPM_END);
#ifdef SEND_LMEM_WITH_RPM
				check_head_error(hevc);
#endif
			}


			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE) {
				PR_INIT(128);

				hevc_print(hevc, 0,
					"rpm_param: (%d)\n", hevc->slice_idx);
				hevc->slice_idx++;

				for (i = 0; i < (RPM_END - RPM_BEGIN); i++) {
					PR_FILL("%04x ", hevc->param.l.data[i]);
					if (((i + 1) & 0xf) == 0)
						PR_INFO(hevc->index);
				}
				PR_INFO(hevc->index);

				hevc_print(hevc, 0,
					"vui_timing_info: %x, %x, %x, %x\n",
					hevc->param.p.vui_num_units_in_tick_hi,
					hevc->param.p.vui_num_units_in_tick_lo,
					hevc->param.p.vui_time_scale_hi,
					hevc->param.p.vui_time_scale_lo);
			}

			if (aux_data_is_avaible(hevc)) {
				static struct PIC_s pic;
				u32 size = 0, type = 0;
				char *p;

				memset(&pic, 0, sizeof(pic));

				pic.aux_data_buf = vzalloc(hevc->prefix_aux_size);

				set_aux_data(hevc, &pic, 0, 0);

				if (pic.aux_data_buf
					&& pic.aux_data_size) {
					hevc->frame_field_info_present_flag =
						(hevc->param.p.sei_frame_field_info >> 8) & 0x1;

					/* parser sei */
					p = pic.aux_data_buf;
					while (p < pic.aux_data_buf
						+ pic.aux_data_size - 8) {
						size = *p++;
						size = (size << 8) | *p++;
						size = (size << 8) | *p++;
						size = (size << 8) | *p++;
						type = *p++;
						type = (type << 8) | *p++;
						type = (type << 8) | *p++;
						type = (type << 8) | *p++;
						if (type == 0x02000000) {
							parse_sei(hevc, &pic, p, size);
						}
						p += size;
					}

					hevc->param.p.sei_frame_field_info &=
						~(0xf << 3);
					hevc->param.p.sei_frame_field_info |=
						(pic.pic_struct << 3);
				}

				if (pic.hdr10p_data_buf)
					vfree(pic.hdr10p_data_buf);
				if (pic.aux_data_buf)
					vfree(pic.aux_data_buf);
			}

			aspect_ratio_set(hevc, &hevc->frame_ar, &ctx->height_aspect_ratio, &ctx->width_aspect_ratio);

			hevc->pic_w = hevc->param.p.pic_width_in_luma_samples;
			hevc->pic_h = hevc->param.p.pic_height_in_luma_samples;
			if (is_oversize_ex(hevc->pic_w, hevc->pic_h)) {
				hevc_print(hevc, 0,"is_oversize w:%d h:%d\n", hevc->pic_w, hevc->pic_h);
				hevc->dec_result = DEC_RESULT_ERROR_DATA;
				amhevc_stop();
				vdec_schedule_work(&hevc->work);
				return IRQ_HANDLED;
			}

			if (!v4l_res_change(hevc, &hevc->param)) {
				if (ctx->param_sets_from_ucode && !hevc->v4l_params_parsed) {
					struct aml_vdec_ps_infos ps;
					int log = hevc->param.p.log2_min_coding_block_size_minus3;
					int log_s = hevc->param.p.log2_diff_max_min_coding_block_size;

					hevc->pic_w = hevc->param.p.pic_width_in_luma_samples;
					hevc->pic_h = hevc->param.p.pic_height_in_luma_samples;
					hevc->lcu_size = 1 << (log + 3 + log_s);

					pr_debug("set ucode parse\n");
					hevc_interlace_check(hevc, &hevc->param);

					if (get_valid_double_write_mode(hevc) != 16) {
						struct vdec_comp_buf_info info;

						get_comp_buf_info(hevc, &info);
						vdec_v4l_set_comp_buf_info(ctx, &info);
					}
					vh265_get_ps_info(hevc, &hevc->param, &ps);
					/*notice the v4l2 codec.*/
					vdec_v4l_set_ps_infos(ctx, &ps);
					ctx->decoder_status_info.frame_height = ps.visible_height;
					ctx->decoder_status_info.frame_width = ps.visible_width;
					hevc->v4l_params_parsed = true;
					hevc->dec_result = DEC_RESULT_AGAIN;
					amhevc_stop();
					restore_decode_state(hevc);
					reset_process_time(hevc);
					vdec_schedule_work(&hevc->work);
					ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);
					return IRQ_HANDLED;
				} else {
					struct vdec_pic_info pic;

					vdec_v4l_get_pic_info(ctx, &pic);
					hevc->used_buf_num = pic.dpb_frames +
						pic.dpb_margin;
					if (hevc->used_buf_num > MAX_BUF_NUM)
						hevc->used_buf_num = MAX_BUF_NUM;
				}
			} else {
				pr_debug("resolution change\n");
				hevc->dec_result = DEC_RESULT_AGAIN;
				amhevc_stop();
				restore_decode_state(hevc);
				reset_process_time(hevc);
				vdec_schedule_work(&hevc->work);
				return IRQ_HANDLED;
			}

			if (
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
				vdec->master == NULL &&
				vdec->slave == NULL &&
#endif
				aux_data_is_avaible(hevc)
				) {

				if (get_dbg_flag(hevc) &
					H265_DEBUG_PRINT_SEI)
					dump_aux_buf(hevc);
			}

			vui_time_scale = (u32)(hevc->param.p.vui_time_scale_hi << 16) |
				hevc->param.p.vui_time_scale_lo;
			vui_num_units_in_tick = (u32)(hevc->param.p.vui_num_units_in_tick_hi << 16) |
				hevc->param.p.vui_num_units_in_tick_lo;
			if (hevc->bit_depth_luma !=
				((hevc->param.p.bit_depth & 0xf) + 8)) {
				reconfig_flag = 1;
				hevc_print(hevc, 0, "Bit depth luma = %d\n",
					(hevc->param.p.bit_depth & 0xf) + 8);
			}
			if (hevc->bit_depth_chroma !=
				(((hevc->param.p.bit_depth >> 4) & 0xf) + 8)) {
				reconfig_flag = 1;
				hevc_print(hevc, 0, "Bit depth chroma = %d\n",
					((hevc->param.p.bit_depth >> 4) & 0xf) + 8);
			}
			hevc->bit_depth_luma = (hevc->param.p.bit_depth & 0xf) + 8;
			hevc->bit_depth_chroma = ((hevc->param.p.bit_depth >> 4) & 0xf) + 8;
			bit_depth_luma = hevc->bit_depth_luma;
			bit_depth_chroma = hevc->bit_depth_chroma;
#ifdef SUPPORT_10BIT
			if (hevc->bit_depth_luma == 8 &&
				hevc->bit_depth_chroma == 8 &&
				enable_mem_saving)
				hevc->mem_saving_mode = 1;
			else
				hevc->mem_saving_mode = 0;
#endif
			if (reconfig_flag &&
				(get_double_write_mode(hevc) & 0x10) == 0)
				init_decode_head_hw(hevc);

			if ((vui_time_scale != 0)
				&& (vui_num_units_in_tick != 0)) {
				hevc->frame_dur = div_u64(96000ULL *
					vui_num_units_in_tick, vui_time_scale);
				hevc->get_frame_dur = true;
			}

			if (hevc->video_signal_type !=
				((hevc->param.p.video_signal_type << 16)
				| hevc->param.p.color_description)) {
				u32 v = hevc->param.p.video_signal_type;
				u32 c = hevc->param.p.color_description;
				hevc->video_signal_type = (v << 16) | c;
				video_signal_type = hevc->video_signal_type;
			}

			if (use_cma && (hevc->param.p.slice_segment_address == 0) &&
				(hevc->pic_list_init_flag == 0)) {
				int log = hevc->param.p.log2_min_coding_block_size_minus3;
				int log_s = hevc->param.p.log2_diff_max_min_coding_block_size;

				hevc->pic_w = hevc->param.p.pic_width_in_luma_samples;
				hevc->pic_h = hevc->param.p.pic_height_in_luma_samples;
				hevc->lcu_size = 1 << (log + 3 + log_s);
				hevc->lcu_size_log2 = log2i(hevc->lcu_size);
				if (performance_profile && ((!is_oversize(hevc->pic_w, hevc->pic_h)) && IS_8K_SIZE(hevc->pic_w,hevc->pic_h)))
					hevc->performance_profile = 1;
				else
					hevc->performance_profile = 0;
				hevc_print(hevc, 0, "hevc->performance_profile %d\n", hevc->performance_profile);
				if (hevc->pic_w == 0 || hevc->pic_h == 0 ||
					hevc->lcu_size == 0 || is_oversize(hevc->pic_w, hevc->pic_h) ||
					hevc_skip_nal(hevc)) {
					/* skip search next start code */
					WRITE_VREG(HEVC_WAIT_FLAG, READ_VREG(HEVC_WAIT_FLAG) & (~0x2));
					if  ((hevc->pic_h == 96) && (hevc->pic_w  == 160))
						hevc->skip_nal_count++;
					hevc->skip_flag = 1;
					WRITE_VREG(HEVC_DEC_STATUS_REG,	HEVC_ACTION_DONE);
					/* Interrupt Amrisc to excute */
					WRITE_VREG(HEVC_MCPU_INTR_REQ,	AMRISC_MAIN_REQ);
#ifdef MULTI_INSTANCE_SUPPORT
					if (hevc->m_ins_flag)
						start_process_time(hevc);
#endif
				} else {
					hevc->sps_num_reorder_pics_0 =
					hevc->param.p.sps_num_reorder_pics_0;
					hevc->ip_mode = hevc->low_latency_flag ? true :
							(!hevc->sps_num_reorder_pics_0 &&
							!(vdec->slave || vdec->master) &&
							!disable_ip_mode) ? true : false;
					hevc->pic_list_init_flag = 1;
#ifdef MULTI_INSTANCE_SUPPORT
					if (hevc->m_ins_flag) {
						vdec_schedule_work(&hevc->work);
					} else
#endif
					up(&h265_sema);
					hevc_print(hevc, 0, "set pic_list_init_flag 1\n");
				}
				ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);
				return IRQ_HANDLED;
			}

		}
		ret = hevc_slice_segment_header_process(hevc, &hevc->param, decode_pic_begin);
		if (ret < 0) {
#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag) {
				hevc->wait_buf = 0;
				hevc->dec_result = DEC_RESULT_AGAIN;
				amhevc_stop();
				restore_decode_state(hevc);
				reset_process_time(hevc);
				vdec_schedule_work(&hevc->work);
				return IRQ_HANDLED;
			}
#else
			;
#endif
		} else if (ret == 0) {
			if ((hevc->new_pic) && (hevc->cur_pic)) {
				hevc->cur_pic->stream_offset = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
				hevc_print(hevc, H265_DEBUG_OUT_PTS,
					"read stream_offset = 0x%x\n",
					hevc->cur_pic->stream_offset);
				hevc->cur_pic->aspect_ratio_idc =
					hevc->param.p.aspect_ratio_idc;
				hevc->cur_pic->sar_width =
					hevc->param.p.sar_width;
				hevc->cur_pic->sar_height =
					hevc->param.p.sar_height;
			}

			aspect_ratio_set(hevc, &hevc->frame_ar, &hevc->cur_pic->sar_height,
				&hevc->cur_pic->sar_width);

			WRITE_VREG(HEVC_DEC_STATUS_REG,
				HEVC_CODED_SLICE_SEGMENT_DAT);
			/* Interrupt Amrisc to excute */
			WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);

			hevc->start_decoding_time = jiffies;
#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag)
				start_process_time(hevc);
#endif
#ifdef MULTI_INSTANCE_SUPPORT
		} else if (hevc->m_ins_flag) {
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s, bufmgr ret %d skip, DEC_RESULT_DONE\n",
				__func__, ret);
			hevc->decoded_poc = INVALID_POC;
			hevc->decoding_pic = NULL;
			hevc->dec_result = DEC_RESULT_DONE;
			amhevc_stop();
			reset_process_time(hevc);
			vdec_schedule_work(&hevc->work);
#endif
		} else {
			/* skip, search next start code */
			hevc->gvs->drop_frame_count++;
			if (hevc->cur_pic->slice_type == I_SLICE) {
				hevc->gvs->i_lost_frames++;
			} else if (hevc->cur_pic->slice_type == P_SLICE) {
				hevc->gvs->i_lost_frames++;
			} else if (hevc->cur_pic->slice_type == B_SLICE) {
				hevc->gvs->i_lost_frames++;
			}
			WRITE_VREG(HEVC_WAIT_FLAG, READ_VREG(HEVC_WAIT_FLAG) & (~0x2));
				hevc->skip_flag = 1;
			WRITE_VREG(HEVC_DEC_STATUS_REG,	HEVC_ACTION_DONE);
			/* Interrupt Amrisc to excute */
			WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);
		}

	ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);
	} else if (dec_status == HEVC_DECODE_OVER_SIZE) {
		hevc_print(hevc, 0 , "hevc  decode oversize !!\n");
#ifdef MULTI_INSTANCE_SUPPORT
		if (!hevc->m_ins_flag)
			debug |= (H265_DEBUG_DIS_LOC_ERROR_PROC |
				H265_DEBUG_DIS_SYS_ERROR_PROC);
#endif
		vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
		hevc->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
	}
	return IRQ_HANDLED;
}

static void wait_hevc_search_done(struct hevc_state_s *hevc)
{
	int count = 0;
	WRITE_VREG(HEVC_SHIFT_STATUS, 0);
	while (READ_VREG(HEVC_STREAM_CONTROL) & 0x2) {
		msleep(20);
		count++;
		if (count > 100) {
			hevc_print(hevc, 0, "%s timeout\n", __func__);
			break;
		}
	}
}
static irqreturn_t vh265_isr(int irq, void *data)
{
	int i, temp;
	unsigned int dec_status;
	struct hevc_state_s *hevc = (struct hevc_state_s *)data;
	u32 debug_tag;
	dec_status = READ_VREG(HEVC_DEC_STATUS_REG);

	if (dec_status == HEVC_SLICE_SEGMENT_DONE) {
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_HEAD_DONE);
	}
	else if (dec_status == HEVC_DECPIC_DATA_DONE) {
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_PIC_DONE);
	}

	if (hevc->init_flag == 0)
		return IRQ_HANDLED;

	ATRACE_COUNTER("V_ST_DEC-decode_state", dec_status);

	hevc->dec_status = dec_status;
	if (is_log_enable(hevc))
		add_log(hevc,
			"isr: status = 0x%x dec info 0x%x lcu 0x%x shiftbyte 0x%x shiftstatus 0x%x",
			dec_status, READ_HREG(HEVC_DECODE_INFO),
			READ_VREG(HEVC_MPRED_CURR_LCU),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_STATUS));

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"265 isr dec status = 0x%x dec info 0x%x shiftbyte 0x%x shiftstatus 0x%x\n",
			dec_status, READ_HREG(HEVC_DECODE_INFO),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_STATUS));

	debug_tag = READ_HREG(DEBUG_REG1);
	if (debug_tag & 0x10000) {
		PR_INIT(128);
		hevc_print(hevc, 0,
			"LMEM<tag %x>:\n", READ_HREG(DEBUG_REG1));

		if (hevc->mmu_enable)
			temp = 0x500;
		else
			temp = 0x400;
		for (i = 0; i < temp; i += 4) {
			int ii;
			if ((i & 0xf) == 0)
				PR_FILL("%03x: ", i);
			for (ii = 0; ii < 4; ii++) {
				PR_FILL("%04x ",
					   hevc->lmem_ptr[i + 3 - ii]);
			}
			if (((i + ii) & 0xf) == 0)
				PR_INFO(hevc->index);
		}

		PR_INFO(hevc->index);

		if (((udebug_pause_pos & 0xffff) == (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx == hevc->decode_idx) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_HREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			hevc->ucode_pause_pos = udebug_pause_pos;
		}
		else if (debug_tag & 0x20000)
			hevc->ucode_pause_pos = 0xffffffff;
		if (hevc->ucode_pause_pos)
			reset_process_time(hevc);
		else
			WRITE_HREG(DEBUG_REG1, 0);
	} else if (debug_tag != 0) {
		hevc_print(hevc, 0,
			"dbg%x: %x l/w/r %x %x %x\n", READ_HREG(DEBUG_REG1),
			READ_HREG(DEBUG_REG2),
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR));
		if (((udebug_pause_pos & 0xffff) == (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx == hevc->decode_idx) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_HREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			hevc->ucode_pause_pos = udebug_pause_pos;
		}
		if (hevc->ucode_pause_pos)
			reset_process_time(hevc);
		else
			WRITE_HREG(DEBUG_REG1, 0);
		return IRQ_HANDLED;
	}


	if (hevc->pic_list_init_flag == 1)
		return IRQ_HANDLED;

	if (!hevc->m_ins_flag) {
		if (dec_status == HEVC_OVER_DECODE) {
			hevc->over_decode = 1;
			hevc_print(hevc, 0,
				"isr: over decode\n"),
				WRITE_VREG(HEVC_DEC_STATUS_REG, 0);
			return IRQ_HANDLED;
		}
	}
	ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_ISR_END);
	return IRQ_WAKE_THREAD;

}

static void vh265_set_clk(struct work_struct *work)
{
	struct hevc_state_s *hevc = container_of(work,
		struct hevc_state_s, set_clk_work);

		int fps = 96000 / hevc->frame_dur;

		if (hevc_source_changed(VFORMAT_HEVC,
			hevc->frame_width, hevc->frame_height, fps) > 0)
			hevc->saved_resolution = hevc->frame_width *
				hevc->frame_height * fps;
}

static void vh265_check_timer_func(struct timer_list *timer)
{
	struct hevc_state_s *hevc = container_of(timer,
		 struct hevc_state_s, timer);
	unsigned char empty_flag;
	unsigned int buf_level;

	enum receviver_start_e state = RECEIVER_INACTIVE;

	if (hevc->init_flag == 0) {
		if (hevc->stat & STAT_TIMER_ARM) {
			mod_timer(&hevc->timer, jiffies + PUT_INTERVAL);
		}
		return;
	}
#ifdef MULTI_INSTANCE_SUPPORT
	if (hevc->m_ins_flag) {
		if (((get_dbg_flag(hevc) &
			H265_DEBUG_DIS_LOC_ERROR_PROC) == 0) &&
			(decode_timeout_val > 0) &&
			(hevc->start_process_time > 0) &&
			((1000 * (jiffies - hevc->start_process_time) / HZ) > decode_timeout_val)) {
			u32 dec_status = READ_VREG(HEVC_DEC_STATUS_REG);
			int current_lcu_idx =
				READ_VREG(HEVC_PARSER_LCU_START)&0xffffff;
			if (dec_status == HEVC_CODED_SLICE_SEGMENT_DAT) {
				if (hevc->last_lcu_idx == current_lcu_idx) {
					if (hevc->decode_timeout_count > 0)
						hevc->decode_timeout_count--;
					if (hevc->decode_timeout_count == 0)
						timeout_process(hevc);
				} else
					restart_process_time(hevc);
				hevc->last_lcu_idx = current_lcu_idx;
			} else {
				hevc->pic_decoded_lcu_idx = current_lcu_idx;
				timeout_process(hevc);
			}
		}
	} else {
#endif
	if (hevc->m_ins_flag == 0 &&
		vf_get_receiver(hevc->provider_name)) {
		state = vf_notify_receiver(hevc->provider_name, VFRAME_EVENT_PROVIDER_QUREY_STATE, NULL);
		if ((state == RECEIVER_STATE_NULL) || (state == RECEIVER_STATE_NONE))
			state = RECEIVER_INACTIVE;
	} else
		state = RECEIVER_INACTIVE;

	empty_flag = (READ_VREG(HEVC_PARSER_INT_STATUS) >> 6) & 0x1;
	/* error watchdog */
	if (hevc->m_ins_flag == 0 &&
		(empty_flag == 0) &&
		(hevc->pic_list_init_flag == 0 ||
		hevc->pic_list_init_flag == 3)) {
		/* decoder has input */
		if ((get_dbg_flag(hevc) &
			H265_DEBUG_DIS_LOC_ERROR_PROC) == 0) {

			buf_level = READ_VREG(HEVC_STREAM_LEVEL);
			/* receiver has no buffer to recycle */
			if ((state == RECEIVER_INACTIVE) &&
				(kfifo_is_empty(&hevc->display_q) &&
				 buf_level > 0x200)
			   ) {
				if (hevc->error_flag == 0) {
					hevc->error_watchdog_count++;
					if (hevc->error_watchdog_count == error_handle_threshold) {
						hevc_print(hevc, 0,
							"H265 dec err local reset.\n");
						hevc->error_flag = 1;
						hevc->error_watchdog_count = 0;
						hevc->error_skip_nal_wt_cnt = 0;
						hevc->error_system_watchdog_count++;
						WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
					}
				} else if (hevc->error_flag == 2) {
					int th = error_handle_nal_skip_threshold;
					hevc->error_skip_nal_wt_cnt++;
					if (hevc->error_skip_nal_wt_cnt == th) {
						hevc->error_flag = 3;
						hevc->error_watchdog_count = 0;
						hevc->error_skip_nal_wt_cnt =	0;
						WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
					}
				}
			}
		}

		if ((get_dbg_flag(hevc)
			& H265_DEBUG_DIS_SYS_ERROR_PROC) == 0)
			/* receiver has no buffer to recycle */
			if ((state == RECEIVER_INACTIVE) &&
				(kfifo_is_empty(&hevc->display_q))) {
				/* no buffer to recycle */
				if ((get_dbg_flag(hevc) & H265_DEBUG_DIS_LOC_ERROR_PROC) != 0)
					hevc->error_system_watchdog_count++;
				if (hevc->error_system_watchdog_count == error_handle_system_threshold) {
					/* and it lasts for a while */
					hevc_print(hevc, 0,
						"H265 dec fatal error watchdog.\n");
					hevc->error_system_watchdog_count = 0;
					hevc->fatal_error |= DECODER_FATAL_ERROR_UNKNOWN;
				}
			}
	} else {
		hevc->error_watchdog_count = 0;
		hevc->error_system_watchdog_count = 0;
	}
#ifdef MULTI_INSTANCE_SUPPORT
	}
#endif
	if ((hevc->ucode_pause_pos != 0) &&
		(hevc->ucode_pause_pos != 0xffffffff) &&
		udebug_pause_pos != hevc->ucode_pause_pos) {
		hevc->ucode_pause_pos = 0;
		WRITE_HREG(DEBUG_REG1, 0);
	}

	if (get_dbg_flag(hevc) & H265_DEBUG_DUMP_PIC_LIST) {
		dump_pic_list(hevc);
		debug &= ~H265_DEBUG_DUMP_PIC_LIST;
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_TRIG_SLICE_SEGMENT_PROC) {
		WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
		debug &= ~H265_DEBUG_TRIG_SLICE_SEGMENT_PROC;
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_HW_RESET) {
		hevc->error_skip_nal_count = error_skip_nal_count;
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);

		debug &= ~H265_DEBUG_HW_RESET;
	}

	if (radr != 0) {
#ifdef SUPPORT_LONG_TERM_RPS
		if ((radr >> 24) != 0) {
			int count = radr >> 24;
			int adr = radr & 0xffffff;
			int i;
			for (i = 0; i < count; i++)
				pr_info("READ_VREG(%x)=%x\n", adr+i, READ_VREG(adr+i));
		} else
#endif
		if (rval != 0) {
			WRITE_VREG(radr, rval);
			pr_info("WRITE_VREG(%x,%x)\n", radr, rval);
		} else
			pr_info("READ_VREG(%x)=%x\n", radr, READ_VREG(radr));
		rval = 0;
		radr = 0;
	}
	if (dbg_cmd != 0) {
		if (dbg_cmd == 1) {
			u32 disp_laddr;

			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXBB &&
				get_double_write_mode(hevc) == 0) {
				disp_laddr = READ_VCBUS_REG(AFBC_BODY_BADDR) << 4;
			} else {
				struct canvas_s cur_canvas;

				canvas_read((READ_VCBUS_REG(VD1_IF0_CANVAS0) & 0xff), &cur_canvas);
				disp_laddr = cur_canvas.addr;
			}
			hevc_print(hevc, 0,
				"current displayed buffer address %x\r\n",
				disp_laddr);
		}
		dbg_cmd = 0;
	}
	/*don't changed at start.*/
	if (hevc->m_ins_flag == 0 &&
		hevc->get_frame_dur && hevc->show_frame_num > 60 &&
		hevc->frame_dur > 0 && hevc->saved_resolution !=
		hevc->frame_width * hevc->frame_height * (96000 / hevc->frame_dur))
		vdec_schedule_work(&hevc->set_clk_work);

	mod_timer(timer, jiffies + PUT_INTERVAL);
}

static int h265_task_handle(void *data)
{
	int ret = 0;
	struct hevc_state_s *hevc = (struct hevc_state_s *)data;

	set_user_nice(current, -10);
	while (1) {
		if (use_cma == 0) {
			hevc_print(hevc, 0,
			"ERROR: use_cma can not be changed dynamically\n");
		}
		ret = down_interruptible(&h265_sema);
		if ((hevc->init_flag != 0) && (hevc->pic_list_init_flag == 1)) {
			init_pic_list(hevc);
			init_pic_list_hw(hevc);
			init_buf_spec(hevc);
			hevc->pic_list_init_flag = 2;
			hevc_print(hevc, 0, "set pic_list_init_flag to 2\n");

			WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);

		}

		if (hevc->uninit_list) {
			/*USE_BUF_BLOCK*/
			uninit_pic_list(hevc);
			hevc_print(hevc, 0, "uninit list\n");
			hevc->uninit_list = 0;
#ifdef USE_UNINIT_SEMA
			if (use_cma) {
				up(&hevc->h265_uninit_done_sema);
				while (!kthread_should_stop())
					msleep(1);
				break;
			}
#endif
		}
	}

	return 0;
}

void vh265_free_cmabuf(void)
{
	struct hevc_state_s *hevc = gHevc;

	mutex_lock(&vh265_mutex);

	if (hevc->init_flag) {
		mutex_unlock(&vh265_mutex);
		return;
	}

	mutex_unlock(&vh265_mutex);
}

#ifdef MULTI_INSTANCE_SUPPORT
int vh265_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
#else
int vh265_dec_status(struct vdec_info *vstatus)
#endif
{
#ifdef MULTI_INSTANCE_SUPPORT
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
#else
	struct hevc_state_s *hevc = gHevc;
#endif
	if (!hevc)
		return -1;

	vstatus->frame_width = hevc->pic_w;
	/* for hevc interlace for disp height x2 */
	vstatus->frame_height = (hevc->pic_h << hevc->interlace_flag);
	if (hevc->frame_dur != 0)
		vstatus->frame_rate = ((96000 * 10 / hevc->frame_dur) % 10) < 5 ?
				96000 / hevc->frame_dur : (96000 / hevc->frame_dur +1);
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = hevc->gvs->error_frame_count;
	vstatus->status = hevc->stat | hevc->fatal_error;
	vstatus->bit_rate = hevc->gvs->bit_rate;
	vstatus->frame_dur = hevc->frame_dur;
	if (hevc->gvs) {
		vstatus->bit_rate = hevc->gvs->bit_rate;
		vstatus->frame_data = hevc->gvs->frame_data;
		vstatus->total_data = hevc->gvs->total_data;
		vstatus->frame_count = hevc->gvs->frame_count;
		vstatus->error_frame_count = hevc->gvs->error_frame_count;
		vstatus->drop_frame_count = hevc->gvs->drop_frame_count;
		vstatus->i_decoded_frames = hevc->gvs->i_decoded_frames;
		vstatus->i_lost_frames = hevc->gvs->i_lost_frames;
		vstatus->i_concealed_frames = hevc->gvs->i_concealed_frames;
		vstatus->p_decoded_frames = hevc->gvs->p_decoded_frames;
		vstatus->p_lost_frames = hevc->gvs->p_lost_frames;
		vstatus->p_concealed_frames = hevc->gvs->p_concealed_frames;
		vstatus->b_decoded_frames = hevc->gvs->b_decoded_frames;
		vstatus->b_lost_frames = hevc->gvs->b_lost_frames;
		vstatus->b_concealed_frames = hevc->gvs->b_concealed_frames;
		vstatus->samp_cnt = hevc->gvs->samp_cnt;
		vstatus->offset = hevc->gvs->offset;
	}

	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
		"%s", DRIVER_NAME);
	vstatus->ratio_control = hevc->ratio_control;
	return 0;
}

int vh265_set_isreset(struct vdec_s *vdec, int isreset)
{
	is_reset = isreset;
	return 0;
}

static int vh265_vdec_info_init(struct hevc_state_s  *hevc)
{
	hevc->gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);

	if (NULL == hevc->gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}
	vdec_set_vframe_comm(hw_to_vdec(hevc), DRIVER_NAME);
	return 0;
}

int vh265_set_trickmode(struct vdec_s *vdec, unsigned long trickmode)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
	hevc_print(hevc, 0,	"[%s %d] trickmode:%lu\n", __func__, __LINE__, trickmode);

	if (trickmode == TRICKMODE_I) {
		trickmode_i = 1;
		i_only_flag = 0x1;
	} else if (trickmode == TRICKMODE_NONE) {
		trickmode_i = 0;
		i_only_flag = 0x0;
	} else if (trickmode == 0x02) {
		trickmode_i = 0;
		i_only_flag = 0x02;
	} else if (trickmode == 0x03) {
		trickmode_i = 1;
		i_only_flag = 0x03;
	} else if (trickmode == 0x07) {
		trickmode_i = 1;
		i_only_flag = 0x07;
	}

	return 0;
}

static void config_decode_mode(struct hevc_state_s *hevc)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct vdec_s *vdec = hw_to_vdec(hevc);
#endif
	unsigned decode_mode;
#ifdef HEVC_8K_LFTOFFSET_FIX
	if (hevc->performance_profile)
		WRITE_VREG(NAL_SEARCH_CTL,
			READ_VREG(NAL_SEARCH_CTL) | (1 << 21));
#endif
	if (!hevc->m_ins_flag)
		decode_mode = DECODE_MODE_SINGLE;
	else if (vdec_frame_based(hw_to_vdec(hevc)))
		decode_mode = DECODE_MODE_MULTI_FRAMEBASE;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	else if (vdec->slave) {
		if (force_bypass_dvenl & 0x80000000)
			hevc->bypass_dvenl = force_bypass_dvenl & 0x1;
		else
			hevc->bypass_dvenl = hevc->bypass_dvenl_enable;
		if (dolby_meta_with_el && hevc->bypass_dvenl) {
			hevc->bypass_dvenl = 0;
			hevc_print(hevc, 0,
				"NOT support bypass_dvenl when meta_with_el\n");
		}
		if (hevc->bypass_dvenl)
			decode_mode = (hevc->start_parser_type << 8) | DECODE_MODE_MULTI_STREAMBASE;
		else
			decode_mode = (hevc->start_parser_type << 8) | DECODE_MODE_MULTI_DVBAL;
	} else if (vdec->master)
		decode_mode = (hevc->start_parser_type << 8) | DECODE_MODE_MULTI_DVENL;
#endif
	else
		decode_mode = DECODE_MODE_MULTI_STREAMBASE;

	if (hevc->m_ins_flag)
		decode_mode |= (hevc->start_decoding_flag << 16);
	/* set MBX0 interrupt flag */
	decode_mode |= (0x80 << 24);
	WRITE_VREG(HEVC_DECODE_MODE, decode_mode);
	WRITE_VREG(HEVC_DECODE_MODE2, hevc->rps_set_id);
}

static void vh265_prot_init(struct hevc_state_s *hevc)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct vdec_s *vdec = hw_to_vdec(hevc);
#endif

	hevc_config_work_space_hw(hevc);

	hevc_init_decoder_hw(hevc, 0, 0xffffffff);

	WRITE_VREG(HEVC_WAIT_FLAG, 1);

	/* clear mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 1);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(HEVC_PSCALE_CTRL, 0);

	WRITE_VREG(DEBUG_REG1, 0x0 | (dump_nal << 8));

	if ((get_dbg_flag(hevc) &
		(H265_DEBUG_MAN_SKIP_NAL |
		H265_DEBUG_MAN_SEARCH_NAL))) {
		WRITE_VREG(NAL_SEARCH_CTL, 0x1);	/* manual parser NAL */
	} else {
		/* check vps/sps/pps/i-slice in ucode */
		unsigned ctl_val = 0x8;
		if (hevc->PB_skip_mode == 0)
			ctl_val = 0x4;	/* check vps/sps/pps only in ucode */
		else if (hevc->PB_skip_mode == 3)
			ctl_val = 0x0;	/* check vps/sps/pps/idr in ucode */

		WRITE_VREG(NAL_SEARCH_CTL, ctl_val);
	}
	if ((get_dbg_flag(hevc) & H265_DEBUG_NO_EOS_SEARCH_DONE)
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		|| vdec->master
		|| vdec->slave
#endif
		)
		WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | 0x10000);

	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL)
		| ((parser_sei_enable & 0x7) << 17));

	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL) |
		((parser_dolby_vision_enable & 0x1) << 20));

	WRITE_VREG(DECODE_STOP_POS, udebug_flag);

	config_decode_mode(hevc);
	config_aux_buf(hevc);
#ifdef SWAP_HEVC_UCODE
	if (!tee_enabled() && hevc->is_swap) {
		WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, hevc->mc_dma_handle);
	}
#endif
#ifdef DETREFILL_ENABLE
	if (hevc->is_swap &&
		get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
		WRITE_VREG(HEVC_SAO_DBG_MODE0, 0);
		WRITE_VREG(HEVC_SAO_DBG_MODE1, 0);
	}
#endif
}

static int vh265_local_init(struct hevc_state_s *hevc)
{
	int i;
	int ret = -1;
	struct vdec_s *vdec = hw_to_vdec(hevc);
	struct aml_vcodec_ctx * ctx = hevc->v4l2_ctx;

#ifdef DEBUG_PTS
	hevc->pts_missed = 0;
	hevc->pts_hit = 0;
#endif
	hevc->saved_resolution = 0;
	hevc->get_frame_dur = false;
	hevc->frame_width = hevc->vh265_amstream_dec_info.width;
	hevc->frame_height = hevc->vh265_amstream_dec_info.height;
	if (is_oversize(hevc->frame_width, hevc->frame_height)) {
		pr_info("over size : %u x %u.\n", hevc->frame_width, hevc->frame_height);
		hevc->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
		vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
		return ret;
	}

	if (hevc->max_pic_w && hevc->max_pic_h) {
		hevc->is_4k = !(hevc->max_pic_w && hevc->max_pic_h) ||
			((hevc->max_pic_w * hevc->max_pic_h) >
			1920 * 1088) ? true : false;
	} else {
		hevc->is_4k = !(hevc->frame_width && hevc->frame_height) ||
			((hevc->frame_width * hevc->frame_height) >
			1920 * 1088) ? true : false;
	}

	hevc->frame_dur =
		(hevc->vh265_amstream_dec_info.rate ==
		 0) ? 3600 : hevc->vh265_amstream_dec_info.rate;
	if (hevc->frame_width && hevc->frame_height)
		hevc->frame_ar = hevc->frame_height * 0x100 / hevc->frame_width;

	if (i_only_flag)
		hevc->i_only = i_only_flag & 0xff;
	else if ((unsigned long) hevc->vh265_amstream_dec_info.param & 0x08)
		hevc->i_only = 0x7;
	else
		hevc->i_only = 0x0;
	hevc->error_watchdog_count = 0;
	hevc->sei_present_flag = 0;
	if (vdec->sys_info)
		pts_unstable = ((unsigned long)vdec->sys_info->param & 0x40) >> 6;
	hevc_print(hevc, 0,
		"h265:pts_unstable=%d\n", pts_unstable);
/*
 *TODO:FOR VERSION
 */
	hevc_print(hevc, 0,
		"h265: ver (%d,%d) decinfo: %dx%d rate=%d\n", h265_version,
		0, hevc->frame_width, hevc->frame_height, hevc->frame_dur);

	if (hevc->frame_dur == 0)
		hevc->frame_dur = 96000 / 24;

	INIT_KFIFO(hevc->display_q);
	INIT_KFIFO(hevc->newframe_q);
	INIT_KFIFO(hevc->pending_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hevc->vfpool[i];

		hevc->vfpool[i].index = -1;
		kfifo_put(&hevc->newframe_q, vf);
	}

	if (!hevc->resolution_change)
		ret = hevc_local_init(hevc);
	else
		ret = 0;

	if (ret < 0) {
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_NO_MEM);
	}

	return ret;
}
#ifdef MULTI_INSTANCE_SUPPORT
static s32 vh265_init(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
	struct aml_vcodec_ctx * ctx = hevc->v4l2_ctx;
#else
static s32 vh265_init(struct hevc_state_s *hevc)
{

#endif
	int ret, size = -1;
	int fw_size = 0x1000 * 16;
	struct firmware_s *fw = NULL;

	timer_setup(&hevc->timer, vh265_check_timer_func, 0);

	hevc->stat |= STAT_TIMER_INIT;

	if (hevc->m_ins_flag) {
#ifdef USE_UNINIT_SEMA
		sema_init(&hevc->h265_uninit_done_sema, 0);
#endif
		INIT_WORK(&hevc->work, vh265_work);
		INIT_WORK(&hevc->timeout_work, vh265_timeout_work);
	}

	if (vh265_local_init(hevc) < 0)
		return -EBUSY;

	mutex_init(&hevc->chunks_mutex);
	INIT_WORK(&hevc->set_clk_work, vh265_set_clk);

	if ((get_decoder_firmware_version() <= UCODE_SWAP_VERSION) &&
		(get_decoder_firmware_submit_count() < UCODE_SWAP_SUBMIT_COUNT)) {
		hevc->enable_ucode_swap = false;
		if ((get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) && (!hevc->is_4k)) {
			hevc->enable_ucode_swap = true;
		}
	} else {
		if (enable_swap)
			hevc->enable_ucode_swap = true;
		else
			hevc->enable_ucode_swap = false;
	}

	pr_debug("ucode version %d.%d, swap enable %d\n",
		get_decoder_firmware_version(), get_decoder_firmware_submit_count(),
		hevc->enable_ucode_swap);

	fw = vzalloc(sizeof(struct firmware_s) + fw_size);
	if (IS_ERR_OR_NULL(fw))
		return -ENOMEM;

	if (hevc->mmu_enable) {
		if (hevc->enable_ucode_swap) {
			size = get_firmware_data(VIDEO_DEC_HEVC_MMU_SWAP, fw->data);
			if (size < 0) {
				pr_info("hevc can not get swap fw code\n");
				size = get_firmware_data(VIDEO_DEC_HEVC_MMU, fw->data);
				hevc->enable_ucode_swap = false;
				hevc->is_swap = false;
			} else if (size)
				hevc->is_swap = true;	//local fw swap
		} else {
			size = get_firmware_data(VIDEO_DEC_HEVC_MMU, fw->data);
		}
	} else
		size = get_firmware_data(VIDEO_DEC_HEVC, fw->data);

	if (size < 0) {
		pr_err("get firmware fail.\n");
		vfree(fw);
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_FW_LOAD_ERROR);
		return -1;
	}

	fw->len = size;

#ifdef SWAP_HEVC_UCODE
	if (!tee_enabled() && hevc->is_swap) {
		if (hevc->mmu_enable) {
			hevc->swap_size = (4 * (4 * SZ_1K)); /*max 4 swap code, each 0x400*/
			hevc->mc_cpu_addr = decoder_dma_alloc_coherent(&hevc->mc_cpu_handle,
					hevc->swap_size, &hevc->mc_dma_handle, "H265_MC_CPU_BUF");
			if (!hevc->mc_cpu_addr) {
				amhevc_disable();
				pr_info("vh265 mmu swap ucode loaded fail.\n");
				return -ENOMEM;
			}

			memcpy((u8 *) hevc->mc_cpu_addr, fw->data + SWAP_HEVC_OFFSET,
				hevc->swap_size);

			hevc_print(hevc, 0,
				"vh265 mmu ucode swap loaded %x\n",
				hevc->mc_dma_handle);
		}
	}
#endif

#ifdef H265_USERDATA_ENABLE
		hevc->sei_itu_data_buf = kmalloc(SEI_ITU_DATA_SIZE, GFP_KERNEL);
		if (hevc->sei_itu_data_buf == NULL) {
			pr_err("%s: failed to alloc sei itu data buffer\n",
				__func__);
			return -1;
		} else if (NULL == hevc->sei_user_data_buffer) {
			hevc->sei_user_data_buffer = kmalloc(USER_DATA_SIZE, GFP_KERNEL);
			if (!hevc->sei_user_data_buffer) {
				pr_info("%s: Can not allocate sei_data_buffer\n", __func__);
				kfree(hevc->sei_itu_data_buf);
				hevc->sei_itu_data_buf = NULL;
			}
			hevc->sei_user_data_wp = 0;
		}
#endif

#ifdef MULTI_INSTANCE_SUPPORT
	if (hevc->m_ins_flag) {
		hevc->timer.expires = jiffies + PUT_INTERVAL;

		hevc->fw = fw;
		hevc->init_flag = 1;

		return 0;
	}
#endif
	amhevc_enable();

	if (hevc->mmu_enable) {
		if (hevc->enable_ucode_swap) {
			ret = amhevc_loadmc_ex(VFORMAT_HEVC, "hevc_mmu_swap", fw->data);
			if (ret < 0)
				ret = amhevc_loadmc_ex(VFORMAT_HEVC, "h265_mmu", fw->data);
			else
				hevc->is_swap = true;
		} else {
			ret = amhevc_loadmc_ex(VFORMAT_HEVC, "h265_mmu", fw->data);
		}
	} else
		ret = amhevc_loadmc_ex(VFORMAT_HEVC, NULL, fw->data);

	if (ret < 0) {
		amhevc_disable();
		vfree(fw);
		pr_err("H265: the %s fw loading failed, err: %x\n",
			tee_enabled() ? "TEE" : "local", ret);
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_FW_LOAD_ERROR);
		return -EBUSY;
	}

	vfree(fw);

	hevc->stat |= STAT_MC_LOAD;

#ifdef DETREFILL_ENABLE
	if (hevc->is_swap && get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM)
		init_detrefill_buf(hevc);
#endif
	/* enable AMRISC side protocol */
	vh265_prot_init(hevc);

	if (vdec_request_threaded_irq(VDEC_IRQ_0, vh265_isr,
		vh265_isr_thread_fn, IRQF_ONESHOT,/*run thread on this irq disabled*/
		"vh265-irq", (void *)hevc)) {
		hevc_print(hevc, 0, "vh265 irq register error.\n");
		amhevc_disable();
		return -ENOENT;
	}

	hevc->stat |= STAT_ISR_REG;
	hevc->provider_name = PROVIDER_NAME;

#ifdef MULTI_INSTANCE_SUPPORT
#else
	vf_provider_init(&vh265_vf_prov, PROVIDER_NAME, &vh265_vf_provider, hevc);
	vf_reg_provider(&vh265_vf_prov);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
	if (hevc->frame_dur != 0) {
		vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_FR_HINT,
				(void *)((unsigned long)hevc->frame_dur));
		fr_hint_status = VDEC_HINTED;
	} else
		fr_hint_status = VDEC_NEED_HINT;
#endif
	hevc->stat |= STAT_VF_HOOK;

	hevc->timer.expires = jiffies + PUT_INTERVAL;

	add_timer(&hevc->timer);

	hevc->stat |= STAT_TIMER_ARM;

	if (use_cma) {
#ifdef USE_UNINIT_SEMA
		sema_init(&hevc->h265_uninit_done_sema, 0);
#endif
		if (h265_task == NULL) {
			sema_init(&h265_sema, 1);
			h265_task = kthread_run(h265_task_handle, hevc, "kthread_h265");
		}
	}

#ifdef SWAP_HEVC_UCODE
	if (!tee_enabled() && hevc->is_swap) {
		WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, hevc->mc_dma_handle);
	}
#endif

#ifndef MULTI_INSTANCE_SUPPORT
	set_vdec_func(&vh265_dec_status);
#endif
	amhevc_start();

	WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, 0);

	hevc->stat |= STAT_VDEC_RUN;
	hevc->init_flag = 1;
	error_handle_threshold = 30;

	return 0;
}

static int check_dirty_data(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)(vdec->private);
	struct vdec_input_s *input = &vdec->input;
	u32 wp, rp, level;
	u32 rp_set;

	rp = STBUF_READ(&vdec->vbuf, get_rp);
	wp = hevc->pre_parser_wr_ptr;

	if (wp > rp)
		level = wp - rp;
	else
		level = wp + vdec->input.size - rp;

	if (level > 0x100000) {
		u32 skip_size = ((level >> 1) >> 19) << 19;
		if (!vdec->input.swap_valid) {
			hevc_print(hevc , 0, "h265 start data discard level 0x%x, buffer level 0x%x, RP 0x%x, WP 0x%x\n",
				((level >> 1) >> 19) << 19, level, rp, wp);
			if (wp >= rp) {
				rp_set = rp + skip_size;
			}
			else if ((rp + skip_size) < (input->start + input->size)) {
				rp_set = rp + skip_size;
			} else {
				rp_set = rp + skip_size - input->size;
			}
			STBUF_WRITE(&vdec->vbuf, set_rp, rp_set);
			vdec->discard_start_data_flag = 1;
			vdec->input.stream_cookie += skip_size;
			hevc->dirty_shift_flag = 1;
		}
		return 1;
	}
	return 0;
}

#ifdef MULTI_INSTANCE_SUPPORT
static void reset_process_time(struct hevc_state_s *hevc)
{
	if (hevc->start_process_time) {
		unsigned int process_time =
			1000 * (jiffies - hevc->start_process_time) / HZ;
		hevc->start_process_time = 0;
		if (process_time > max_process_time[hevc->index])
			max_process_time[hevc->index] = process_time;
	}
}

static void start_process_time(struct hevc_state_s *hevc)
{
	hevc->start_process_time = jiffies;
	hevc->decode_timeout_count = 2;
	hevc->last_lcu_idx = 0;
}

static void restart_process_time(struct hevc_state_s *hevc)
{
	hevc->start_process_time = jiffies;
	hevc->decode_timeout_count = 2;
}

static void timeout_process(struct hevc_state_s *hevc)
{
	struct aml_vcodec_ctx * ctx = hevc->v4l2_ctx;
	/*
	 * In this very timeout point,the vh265_work arrives,
	 * or in some cases the system become slow,  then come
	 * this second timeout. In both cases we return.
	 */
	if (work_pending(&hevc->work) ||
	    work_busy(&hevc->work) ||
	    work_busy(&hevc->timeout_work) ||
	    work_pending(&hevc->timeout_work)) {
		pr_err("%s h265[%d] work pending, do nothing.\n",__func__, hevc->index);
		return;
	}

	hevc->timeout_num++;
	amhevc_stop();
	read_decode_info(hevc);

	hevc_print(hevc, 0, "%s decoder timeout\n", __func__);
	check_pic_decoded_error(hevc,
				hevc->pic_decoded_lcu_idx);

	vdec_v4l_post_error_event(ctx, DECODER_WARNING_DECODER_TIMEOUT);

	/*The current decoded frame is marked
		error when the decode timeout*/
	if (hevc->cur_pic != NULL)
		hevc->cur_pic->error_mark = 1;
	hevc->decoded_poc = hevc->curr_POC;
	hevc->decoding_pic = NULL;
	hevc->dec_result = DEC_RESULT_DONE;
	reset_process_time(hevc);

	if (work_pending(&hevc->work))
		return;
	vdec_schedule_work(&hevc->timeout_work);
}

static bool is_avaliable_buffer(struct hevc_state_s *hevc)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	struct PIC_s *pic = NULL;
	int i, free_count = 0;
	int used_count = 0;

	if ((hevc->used_buf_num == 0) ||
		(ctx->cap_pool.dec < hevc->used_buf_num)) {
		if (ctx->fb_ops.query(&ctx->fb_ops, &hevc->fb_token)) {
			free_count =
				v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx) + 1;
		}
	}

	for (i = 0; i < hevc->used_buf_num; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL ||
			pic->index == -1 ||
			pic->BUF_index == -1)
			continue;

		if ((pic->output_mark == 0) &&
			(pic->referenced == 0) &&
			(pic->output_ready == 0) &&
			(pic->vf_ref == 0) &&
			pic->cma_alloc_addr) {
			free_count++;
		} else if (pic->cma_alloc_addr)
			used_count++;
	}

	ATRACE_COUNTER("V_ST_DEC-free_buff_count", free_count);
	ATRACE_COUNTER("V_ST_DEC-used_buff_count", used_count);

	return free_count >= run_ready_min_buf_num ? 1 : 0;
}

static unsigned char is_new_pic_available(struct hevc_state_s *hevc)
{
	struct PIC_s *new_pic = NULL;
	struct PIC_s *pic;
	/* recycle un-used pic */
	int i;
	int ref_pic = 0;
	unsigned long flags;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	/*return 1 if pic_list is not initialized yet*/
	if (hevc->pic_list_init_flag != 3)
		return 1;

	spin_lock_irqsave(&h265_lock, flags);
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1 || pic->BUF_index == -1)
			continue;
		if (pic->referenced == 1)
			ref_pic++;
		if (pic->output_mark == 0 && pic->referenced == 0
			&& pic->output_ready == 0
			&& pic->vf_ref == 0
			) {
			if (new_pic) {
				if (pic->POC < new_pic->POC)
					new_pic = pic;
			} else
				new_pic = pic;
		}
	}

	if ((new_pic == NULL) ||
		((ctx->param_sets_from_ucode) &&
		(hevc->v4l_params_parsed) &&
		(ctx->cap_pool.dec < hevc->used_buf_num) && !is_avaliable_buffer(hevc))) {
		int decode_count = 0;

		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic == NULL || pic->index == -1 || pic->BUF_index == -1)
				continue;
			if ((pic->output_ready == 0) && (pic->output_mark != 0 ||
				pic->referenced != 0 ||
				pic->vf_ref != 0))
				decode_count++;
		}

		if (decode_count >=
				hevc->param.p.sps_max_dec_pic_buffering_minus1_0 + detect_stuck_buffer_margin) {
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE)
				dump_pic_list(hevc);
			if (!(error_handle_policy & 0x400)) {
				spin_unlock_irqrestore(&h265_lock, flags);
				flush_output(hevc, NULL);
				hevc_print(hevc, H265_DEBUG_BUFMGR, "flush dpb, ref_error_count %d, sps_max_dec_pic_buffering_minus1_0 %d\n",
						decode_count, hevc->param.p.sps_max_dec_pic_buffering_minus1_0);
				return 0;
			}
		}
	}

	spin_unlock_irqrestore(&h265_lock, flags);

	return (new_pic != NULL) ? 1 : 0;
}

static int vmh265_stop(struct hevc_state_s *hevc)
{
	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}
	if (hevc->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hevc->stat &= ~STAT_VDEC_RUN;
	}
	if (hevc->stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_0, (void *)hevc);
		hevc->stat &= ~STAT_ISR_REG;
	}

	hevc_local_uninit(hevc);

	if (hevc->gvs)
		kfree(hevc->gvs);
	hevc->gvs = NULL;

	if (use_cma) {
		hevc->uninit_list = 1;
		reset_process_time(hevc);
		hevc->dec_result = DEC_RESULT_FREE_CANVAS;
		vdec_schedule_work(&hevc->work);
		flush_work(&hevc->work);
#ifdef USE_UNINIT_SEMA
		if (hevc->init_flag) {
			down(&hevc->h265_uninit_done_sema);
		}
#else
		while (hevc->uninit_list)	/* wait uninit complete */
			msleep(20);
#endif
	}
	hevc->init_flag = 0;
	hevc->first_sc_checked = 0;
	cancel_work_sync(&hevc->set_clk_work);
	cancel_work_sync(&hevc->timeout_work);
	cancel_work_sync(&hevc->work);
	uninit_mmu_buffers(hevc);
#ifdef H265_USERDATA_ENABLE
	if (hevc->sei_itu_data_buf) {
		kfree(hevc->sei_itu_data_buf);
		hevc->sei_itu_data_buf = NULL;
	}
	if (hevc->sei_user_data_buffer) {
		kfree(hevc->sei_user_data_buffer);
		hevc->sei_user_data_buffer = NULL;
	}
#endif

	vfree(hevc->fw);
	hevc->fw = NULL;

	dump_log(hevc);
	return 0;
}

static unsigned char get_data_check_sum
	(struct hevc_state_s *hevc, int size)
{
	int sum = 0;
	u8 *data = NULL;

	if (!hevc->chunk->block->is_mapped)
		data = codec_mm_vmap(hevc->chunk->block->start +
			hevc->chunk->offset, size);
	else
		data = ((u8 *)hevc->chunk->block->start_virt) +
			hevc->chunk->offset;

	sum = crc32_le(0, data, size);

	hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
		"%s: size 0x%x sum 0x%x %02x %02x %02x %02x %02x %02x .. %02x %02x %02x %02x\n",
		__func__, size, sum,
		data[0], data[1], data[2], data[3],
		data[4], data[5], data[size - 4],
		data[size - 3], data[size - 2],
		data[size - 1]);

	if (!hevc->chunk->block->is_mapped)
		codec_mm_unmap_phyaddr(data);
	return sum;
}

static void vh265_work_implement(struct hevc_state_s *hevc,
	struct vdec_s *vdec,int from)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	ATRACE_COUNTER("V_ST_DEC-work_state", hevc->dec_result);

	if (hevc->dec_result == DEC_RESULT_AGAIN)
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_WORKER_AGAIN);
	if (hevc->dec_result != DEC_RESULT_NONE)
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_WORKER_START);
	if (hevc->dec_result == DEC_RESULT_FREE_CANVAS) {
		/*USE_BUF_BLOCK*/
		uninit_pic_list(hevc);
		hevc->uninit_list = 0;
#ifdef USE_UNINIT_SEMA
		up(&hevc->h265_uninit_done_sema);
#endif
		return;
	}

	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */
	if (hevc->pic_list_init_flag == 1 &&
		(hevc->dec_result != DEC_RESULT_FORCE_EXIT)) {
		hevc->pic_list_init_flag = 2;
		init_pic_list(hevc);
		init_pic_list_hw(hevc);
		init_buf_spec(hevc);
		hevc_print(hevc, 0,
			"set pic_list_init_flag to 2\n");

		WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
		return;
	}

	hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
		"%s dec_result %d %x %x %x\n",
		__func__,
		hevc->dec_result,
		READ_VREG(HEVC_STREAM_LEVEL),
		READ_VREG(HEVC_STREAM_WR_PTR),
		READ_VREG(HEVC_STREAM_RD_PTR));

	if (((hevc->dec_result == DEC_RESULT_GET_DATA) ||
		(hevc->dec_result == DEC_RESULT_GET_DATA_RETRY)) &&
		(hw_to_vdec(hevc)->next_status != VDEC_STATUS_DISCONNECTED)) {
		if (!vdec_has_more_input(vdec)) {
			hevc->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hevc->work);
			return;
		}
		if (!input_frame_based(vdec)) {
			int r = vdec_sync_input(vdec);
			if (r >= 0x200) {
				WRITE_VREG(HEVC_DECODE_SIZE,
					READ_VREG(HEVC_DECODE_SIZE) + r);

				hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
					"%s DEC_RESULT_GET_DATA %x %x %x mpc %x size 0x%x\n",
					__func__,
					READ_VREG(HEVC_STREAM_LEVEL),
					READ_VREG(HEVC_STREAM_WR_PTR),
					READ_VREG(HEVC_STREAM_RD_PTR),
					READ_VREG(HEVC_MPC_E), r);

				start_process_time(hevc);
				if (READ_VREG(HEVC_DEC_STATUS_REG)
				 == HEVC_DECODE_BUFEMPTY2)
					WRITE_VREG(HEVC_DEC_STATUS_REG,
						HEVC_ACTION_DONE);
				else
					WRITE_VREG(HEVC_DEC_STATUS_REG,
						HEVC_ACTION_DEC_CONT);
			} else {
				hevc->dec_result = DEC_RESULT_GET_DATA_RETRY;
				vdec_schedule_work(&hevc->work);
			}
			return;
		}

		/*below for frame_base*/
		if (hevc->dec_result == DEC_RESULT_GET_DATA) {
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_GET_DATA %x %x %x mpc %x\n",
				__func__,
				READ_VREG(HEVC_STREAM_LEVEL),
				READ_VREG(HEVC_STREAM_WR_PTR),
				READ_VREG(HEVC_STREAM_RD_PTR),
				READ_VREG(HEVC_MPC_E));
			mutex_lock(&hevc->chunks_mutex);
			vdec_vframe_dirty(vdec, hevc->chunk);
			hevc->chunk = NULL;
			mutex_unlock(&hevc->chunks_mutex);
			vdec_clean_input(vdec);
		}

		if (run_ready(vdec, VDEC_HEVC)) {
			int r;
			int decode_size;

			r = vdec_prepare_input(vdec, &hevc->chunk);
			if (r < 0) {
				hevc->dec_result = DEC_RESULT_GET_DATA_RETRY;

				hevc_print(hevc,
					PRINT_FLAG_VDEC_DETAIL,
					"amvdec_vh265: Insufficient data\n");

				vdec_schedule_work(&hevc->work);
				return;
			}
			hevc->dec_result = DEC_RESULT_NONE;
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s: chunk size 0x%x sum 0x%x mpc %x\n",
				__func__, r,
				(get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS) ?
				get_data_check_sum(hevc, r) : 0,
				READ_VREG(HEVC_MPC_E));

			if (get_dbg_flag(hevc) & PRINT_FRAMEBASE_DATA) {
				int jj;
				u8 *data = NULL;
				PR_INIT(128);

				if (!hevc->chunk->block->is_mapped)
					data = codec_mm_vmap(hevc->chunk->block->start + hevc->chunk->offset, r);
				else
					data = ((u8 *)hevc->chunk->block->start_virt) + hevc->chunk->offset;

				for (jj = 0; jj < r; jj++) {
					if ((jj & 0xf) == 0)
						PR_FILL("%06x:", jj);
					PR_FILL("%02x ", data[jj]);
					if (((jj + 1) & 0xf) == 0)
						PR_INFO(hevc->index);
				}
				PR_INFO(hevc->index);

				if (!hevc->chunk->block->is_mapped)
					codec_mm_unmap_phyaddr(data);
			}

			decode_size = hevc->chunk->size +
				(hevc->chunk->offset & (VDEC_FIFO_ALIGN - 1));
			WRITE_VREG(HEVC_DECODE_SIZE,
				READ_VREG(HEVC_DECODE_SIZE) + decode_size);

			vdec_enable_input(vdec);

			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s: mpc %x\n",
				__func__, READ_VREG(HEVC_MPC_E));

			start_process_time(hevc);
			WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
		} else{
			hevc->dec_result = DEC_RESULT_GET_DATA_RETRY;

			vdec_schedule_work(&hevc->work);
		}
		ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_WORKER_END);
		return;
	} else if (hevc->dec_result == DEC_RESULT_DONE) {
			int i;
		struct aml_vcodec_ctx * ctx = hevc->v4l2_ctx;

		ctx->decoder_status_info.decoder_count++;
		decode_frame_count[hevc->index]++;
#ifdef DETREFILL_ENABLE
	if (hevc->is_swap &&
		get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM) {
		if (hevc->delrefill_check == 2) {
			delrefill(hevc);
			amhevc_stop();
		}
	}
#endif
		if (hevc->mmu_enable && ((get_double_write_mode(hevc) & 0x10) == 0)) {
			hevc->used_4k_num = READ_VREG(HEVC_SAO_MMU_STATUS) >> 16;
			if (hevc->used_4k_num >= 0 &&
				hevc->cur_pic &&
				hevc->cur_pic->scatter_alloc == 1)
				recycle_mmu_buf_tail(hevc, hevc->m_ins_flag);
		}
		hevc->pic_decoded_lcu_idx = READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff;

		if (vdec->master == NULL && vdec->slave == NULL &&
			hevc->empty_flag == 0) {
			hevc->over_decode =
				(READ_VREG(HEVC_SHIFT_STATUS) >> 15) & 0x1;
			if (hevc->over_decode)
				hevc_print(hevc, 0, "!!!Over decode\n");
		}

		if (is_log_enable(hevc))
			add_log(hevc,
				"%s dec_result %d lcu %d used_mmu %d shiftbyte 0x%x decbytes 0x%x",
				__func__,
				hevc->dec_result,
				hevc->pic_decoded_lcu_idx,
				hevc->used_4k_num,
				READ_VREG(HEVC_SHIFT_BYTE_COUNT),
				READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
				hevc->start_shift_bytes);

		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s dec_result %d (%x %x %x) lcu %d used_mmu %d shiftbyte 0x%x decbytes 0x%x\n",
			__func__,
			hevc->dec_result,
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR),
			hevc->pic_decoded_lcu_idx,
			hevc->used_4k_num,
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
			hevc->start_shift_bytes);

		hevc->used_4k_num = -1;

		check_pic_decoded_error(hevc,
			hevc->pic_decoded_lcu_idx);
		if ((error_handle_policy & 0x100) == 0 && hevc->cur_pic) {
			for (i = 0; i < MAX_REF_PIC_NUM; i++) {
				struct PIC_s *pic;
				pic = hevc->m_PIC[i];
				if (!pic || pic->index == -1)
					continue;
				if ((hevc->cur_pic->POC + poc_num_margin < pic->POC) && (pic->referenced == 0) &&
					(pic->output_mark == 1) && (pic->output_ready == 0)) {
					hevc->poc_error_count++;
					break;
				}
			}
			if (i == MAX_REF_PIC_NUM)
				hevc->poc_error_count = 0;
			if (hevc->poc_error_count >= poc_error_limit) {
				for (i = 0; i < MAX_REF_PIC_NUM; i++) {
					struct PIC_s *pic;
					pic = hevc->m_PIC[i];
					if (!pic || pic->index == -1)
						continue;
					if ((hevc->cur_pic->POC + poc_num_margin < pic->POC) && (pic->referenced == 0) &&
						(pic->output_mark == 1) && (pic->output_ready == 0)) {
						pic->output_mark = 0;
						hevc_print(hevc, 0, "DPB poc error, remove error frame\n");
					}
				}
			}
		}

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (vdec->slave) {
			if (dv_debug & 0x1)
				vdec_set_flag(vdec->slave,
					VDEC_FLAG_SELF_INPUT_CONTEXT);
			else
				vdec_set_flag(vdec->slave,
					VDEC_FLAG_OTHER_INPUT_CONTEXT);
		}
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		hevc->shift_byte_count_lo
			= READ_VREG(HEVC_SHIFT_BYTE_COUNT);
		if (vdec->slave) {
			/*cur is base, found enhance*/
			struct hevc_state_s *hevc_el =
			(struct hevc_state_s *)
				vdec->slave->private;
			if (hevc_el)
				hevc_el->shift_byte_count_lo =
				hevc->shift_byte_count_lo;
		} else if (vdec->master) {
			/*cur is enhance, found base*/
			struct hevc_state_s *hevc_ba =
			(struct hevc_state_s *)
				vdec->master->private;
			if (hevc_ba)
				hevc_ba->shift_byte_count_lo =
				hevc->shift_byte_count_lo;
		}
#endif
		mutex_lock(&hevc->chunks_mutex);
		vdec_vframe_dirty(hw_to_vdec(hevc), hevc->chunk);
		hevc->chunk = NULL;
		mutex_unlock(&hevc->chunks_mutex);
	} else if (hevc->dec_result == DEC_RESULT_AGAIN) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		if (!vdec_has_more_input(vdec)) {
			hevc->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hevc->work);
			return;
		}
#ifdef AGAIN_HAS_THRESHOLD
		hevc->next_again_flag = 1;
#endif
		if (input_stream_based(vdec)) {
			u32 rp, wp, level;
			struct vdec_input_s *input = &vdec->input;
			rp = STBUF_READ(&vdec->vbuf, get_rp);;
			wp = STBUF_READ(&vdec->vbuf, get_wp);
			if (wp < rp)
				level = input->size + wp - rp;
			else
				level = wp - rp;
			if ((level >= dirty_buffersize_threshold) &&
				(hevc->pre_parser_video_rp ==
					STBUF_READ(&vdec->vbuf, get_rp)) &&
				(hevc->pre_parser_video_wp ==
					STBUF_READ(&vdec->vbuf, get_wp))) {
				if (hevc->again_count == 0) {
					hevc->again_timeout_jiffies =
						get_jiffies_64() + dirty_time_threshold * HZ/1000;
				}
				hevc->again_count++;
			}
			else
				hevc->again_count = 0;

			hevc->pre_parser_video_rp = STBUF_READ(&vdec->vbuf, get_rp);
			hevc->pre_parser_video_wp = STBUF_READ(&vdec->vbuf, get_wp);

			if (((hevc->again_count > dirty_count_threshold) &&
					time_after64(get_jiffies_64(), hevc->again_timeout_jiffies))) {
				mutex_lock(&hevc->chunks_mutex);
				hevc->again_count = 0;
				vdec_vframe_dirty(hw_to_vdec(hevc), hevc->chunk);
				hevc->chunk = NULL;
				hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL, "Discard dirty data\n");
				mutex_unlock(&hevc->chunks_mutex);
			} else if ((((error_handle_policy & 0x200) == 0) &&
						(hevc->pic_list_init_flag == 0))) {
				check_dirty_data(vdec);
			}
		}
	} else if (hevc->dec_result == DEC_RESULT_EOS) {
		struct PIC_s *pic;
		hevc->eos = 1;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if ((!hevc->discard_dv_data) && aux_data_is_avaible(hevc))
			if (hevc->decoding_pic)
				dolby_get_meta(hevc);
#endif
		check_pic_decoded_error(hevc, hevc->pic_decoded_lcu_idx);
		pic = get_pic_by_POC(hevc, hevc->curr_POC);
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s: end of stream, last dec poc %d => 0x%pf\n",
			__func__, hevc->curr_POC, pic);
		flush_output(hevc, pic);
		/* dummy vf with eos flag to backend */
		ATRACE_COUNTER("V_ST_DEC-submit_eos", __LINE__);
		notify_v4l_eos(hw_to_vdec(hevc));
		ATRACE_COUNTER("V_ST_DEC-submit_eos", 0);

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		hevc->shift_byte_count_lo = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
		if (vdec->slave) {
			/*cur is base, found enhance*/
			struct hevc_state_s *hevc_el = (struct hevc_state_s *)vdec->slave->private;
			if (hevc_el)
				hevc_el->shift_byte_count_lo = hevc->shift_byte_count_lo;
		} else if (vdec->master) {
			/*cur is enhance, found base*/
			struct hevc_state_s *hevc_ba = (struct hevc_state_s *)vdec->master->private;
			if (hevc_ba)
				hevc_ba->shift_byte_count_lo = hevc->shift_byte_count_lo;
		}
#endif
		mutex_lock(&hevc->chunks_mutex);
		vdec_vframe_dirty(hw_to_vdec(hevc), hevc->chunk);
		hevc->chunk = NULL;
		mutex_unlock(&hevc->chunks_mutex);
	} else if (hevc->dec_result == DEC_RESULT_FORCE_EXIT) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS, "%s: force exit\n", __func__);
		if (hevc->stat & STAT_VDEC_RUN) {
			amhevc_stop();
			hevc->stat &= ~STAT_VDEC_RUN;
		}
		if (hevc->stat & STAT_ISR_REG) {
				WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 0);
			vdec_free_irq(VDEC_IRQ_0, (void *)hevc);
			hevc->stat &= ~STAT_ISR_REG;
		}
		hevc_print(hevc, 0, "%s: force exit end\n", __func__);
	} else if (hevc->dec_result == DEC_RESULT_ERROR_DATA) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s dec_result %d (%x %x %x) lcu %d used_mmu %d shiftbyte 0x%x decbytes 0x%x\n",
			__func__,
			hevc->dec_result,
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR),
			hevc->pic_decoded_lcu_idx,
			hevc->used_4k_num,
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
			hevc->start_shift_bytes);
		mutex_lock(&hevc->chunks_mutex);
		vdec_vframe_dirty(hw_to_vdec(hevc), hevc->chunk);
		hevc->chunk = NULL;
		mutex_unlock(&hevc->chunks_mutex);
	}

	if (hevc->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hevc->stat &= ~STAT_VDEC_RUN;
	}

	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}

	ATRACE_COUNTER(hevc->trace.decode_work_time_name, TRACE_WORK_WAIT_SEARCH_DONE_START);
	wait_hevc_search_done(hevc);
	ATRACE_COUNTER(hevc->trace.decode_work_time_name, TRACE_WORK_WAIT_SEARCH_DONE_END);

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (hevc->switch_dvlayer_flag) {
		if (vdec->slave)
			vdec_set_next_sched(vdec, vdec->slave);
		else if (vdec->master)
			vdec_set_next_sched(vdec, vdec->master);
	} else if (vdec->slave || vdec->master)
		vdec_set_next_sched(vdec, vdec);
#endif

	if (from == 1) {
		/* This is a timeout work */
		if (work_pending(&hevc->work)) {
			/*
			 * The vh265_work arrives at the last second,
			 * give it a chance to handle the scenario.
			 */
			return;
		}
	}

	ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_WORKER_END);

	/* mark itself has all HW resource released and input released */
	if (vdec->parallel_dec == 1)
		vdec_core_finish_run(vdec, CORE_MASK_HEVC);
	else
		vdec_core_finish_run(vdec, CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

	if (ctx->param_sets_from_ucode &&
		!hevc->v4l_params_parsed)
		vdec_v4l_write_frame_sync(ctx);

	ATRACE_COUNTER("V_ST_DEC-chunk_size", 0);

	if (hevc->vdec_cb)
		hevc->vdec_cb(hw_to_vdec(hevc), hevc->vdec_cb_arg);
}

static void vh265_work(struct work_struct *work)
{
	struct hevc_state_s *hevc = container_of(work,
			struct hevc_state_s, work);
	struct vdec_s *vdec = hw_to_vdec(hevc);

	vh265_work_implement(hevc, vdec, 0);
}

static void vh265_timeout_work(struct work_struct *work)
{
	struct hevc_state_s *hevc = container_of(work,
		struct hevc_state_s, timeout_work);
	struct vdec_s *vdec = hw_to_vdec(hevc);

	if (work_pending(&hevc->work))
		return;
	hevc->timeout_processing = 1;
	vh265_work_implement(hevc, vdec, 1);
}


static int vh265_hw_ctx_restore(struct hevc_state_s *hevc)
{
	/* new to do ... */
	vh265_prot_init(hevc);
	return 0;
}
static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);
	int tvp = vdec_secure(hw_to_vdec(hevc)) ?
		CODEC_MM_FLAGS_TVP : 0;
	bool ret = 0;
	if (step == 0x12)
		return 0;
	else if (step == 0x11)
		step = 0x12;

	if (hevc->fatal_error & DECODER_FATAL_ERROR_NO_MEM)
		return 0;

	if (hevc->eos)
		return 0;
	if (hevc->timeout_processing &&
	    (work_pending(&hevc->work) ||
	    work_busy(&hevc->work) ||
	    work_busy(&hevc->timeout_work) ||
	    work_pending(&hevc->timeout_work))) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			   "h265 work pending,not ready for run.\n");
		return 0;
	}
	hevc->timeout_processing = 0;
	if (!hevc->first_sc_checked && hevc->mmu_enable) {
		int size;
		void * mmu_box;
		struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hevc->v4l2_ctx);

		mmu_box = ctx->mmu_box;

		size = decoder_mmu_box_sc_check(mmu_box, tvp);
		hevc->first_sc_checked =1;
		hevc_print(hevc, 0,
			"vh265 cached=%d  need_size=%d speed= %d ms\n",
			size, (hevc->need_cache_size >> PAGE_SHIFT),
			(int)(get_jiffies_64() - hevc->sc_start_time) * 1000 / HZ);
	}
	if (vdec_stream_based(vdec) && (hevc->init_flag == 0)
			&& pre_decode_buf_level != 0) {
			u32 rp, wp, level;

			rp = STBUF_READ(&vdec->vbuf, get_rp);
			wp = STBUF_READ(&vdec->vbuf, get_wp);
			if (wp < rp)
				level = vdec->input.size + wp - rp;
			else
				level = wp - rp;

			if (level < pre_decode_buf_level)
				return 0;
	}

#ifdef AGAIN_HAS_THRESHOLD
	if (hevc->next_again_flag &&
		(!vdec_frame_based(vdec))) {
		u32 parser_wr_ptr =
			STBUF_READ(&vdec->vbuf, get_wp);
		if (parser_wr_ptr >= hevc->pre_parser_wr_ptr &&
			(parser_wr_ptr - hevc->pre_parser_wr_ptr) <
			again_threshold) {
			int r = vdec_sync_input(vdec);
			hevc_print(hevc,
				PRINT_FLAG_VDEC_DETAIL, "%s buf lelvel:%x\n",  __func__, r);
			return 0;
		}
	}
#endif

	if (disp_vframe_valve_level &&
		kfifo_len(&hevc->display_q) >=
		disp_vframe_valve_level) {
		hevc->valve_count--;
		if (hevc->valve_count <= 0)
			hevc->valve_count = 2;
		else
			return 0;
	}

	ret = is_new_pic_available(hevc);
	if (!ret) {
		hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL, "%s=>%d\r\n", __func__, ret);
	}

	if (ctx->param_sets_from_ucode) {
		if (hevc->v4l_params_parsed) {
			ret = is_avaliable_buffer(hevc) ? 1 : 0;
		} else {
			ret = ctx->v4l_resolution_change ? 0 : 1;
		}
	} else if (!ctx->v4l_codec_dpb_ready) {
		if (v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx) <
			run_ready_min_buf_num)
			ret = 0;
	}

	if (ret)
		not_run_ready[hevc->index] = 0;
	else
		not_run_ready[hevc->index]++;
	if (vdec->parallel_dec == 1)
		return ret ? (CORE_MASK_HEVC) : 0;
	else
		return ret ? (CORE_MASK_VDEC_1 | CORE_MASK_HEVC) : 0;
}

static void run(struct vdec_s *vdec, unsigned long mask,
	void (*callback)(struct vdec_s *, void *), void *arg)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	int r, loadr = 0;
	unsigned char check_sum = 0;
	struct aml_vcodec_ctx * ctx = hevc->v4l2_ctx;

	run_count[hevc->index]++;
	hevc->vdec_cb_arg = arg;
	hevc->vdec_cb = callback;
	hevc->aux_data_dirty = 1;

	ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_RUN_START);
	hevc_reset_core(vdec);

#ifdef AGAIN_HAS_THRESHOLD
	if (vdec_stream_based(vdec)) {
		hevc->pre_parser_wr_ptr =
			STBUF_READ(&vdec->vbuf, get_wp);
		hevc->next_again_flag = 0;
	}
#endif
	r = vdec_prepare_input(vdec, &hevc->chunk);
	if (r < 0) {
		input_empty[hevc->index]++;
		hevc->dec_result = DEC_RESULT_AGAIN;
		hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
			"ammvdec_vh265: Insufficient data\n");

		vdec_schedule_work(&hevc->work);
		return;
	}

	ATRACE_COUNTER("V_ST_DEC-chunk_size", r);

	input_empty[hevc->index] = 0;
	hevc->dec_result = DEC_RESULT_NONE;
	if (vdec_frame_based(vdec) &&
		((get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)
		|| is_log_enable(hevc)) &&
		!vdec_secure(vdec))
		check_sum = get_data_check_sum(hevc, r);

	if (is_log_enable(hevc))
		add_log(hevc,
			"%s: size %d sum 0x%x shiftbyte 0x%x",
			__func__, r,
			check_sum,
			READ_VREG(HEVC_SHIFT_BYTE_COUNT));
	if ((hevc->dirty_shift_flag == 1) && !(vdec->input.swap_valid)) {
		WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, vdec->input.stream_cookie);
	}
	hevc->start_shift_bytes = READ_VREG(HEVC_SHIFT_BYTE_COUNT);

	hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
		"%s: size %d sum 0x%x (%x %x %x %x %x) byte count %x\n",
		__func__, r,
		check_sum,
		READ_VREG(HEVC_STREAM_LEVEL),
		READ_VREG(HEVC_STREAM_WR_PTR),
		READ_VREG(HEVC_STREAM_RD_PTR),
		STBUF_READ(&vdec->vbuf, get_rp),
		STBUF_READ(&vdec->vbuf, get_wp),
		hevc->start_shift_bytes);
	if ((get_dbg_flag(hevc) & PRINT_FRAMEBASE_DATA) &&
		input_frame_based(vdec) &&
		!vdec_secure(vdec)) {
		int jj;
		u8 *data = NULL;
		PR_INIT(128);

		if (!hevc->chunk->block->is_mapped)
			data = codec_mm_vmap(hevc->chunk->block->start + hevc->chunk->offset, r);
		else
			data = ((u8 *)hevc->chunk->block->start_virt) + hevc->chunk->offset;

		for (jj = 0; jj < r; jj++) {
			if ((jj & 0xf) == 0)
				PR_FILL("%06x:", jj);
			PR_FILL("%02x ", data[jj]);
			if (((jj + 1) & 0xf) == 0)
				PR_INFO(hevc->index);
		}
		PR_INFO(hevc->index);

		if (!hevc->chunk->block->is_mapped)
			codec_mm_unmap_phyaddr(data);
	}
	ATRACE_COUNTER(hevc->trace.decode_run_time_name, TRACE_RUN_LOADING_FW_START);
	if (vdec->mc_loaded) {
		/*firmware have load before,
		  and not changes to another.
		  ignore reload.
		*/
		if (tee_enabled() && hevc->is_swap)
			WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, hevc->swap_addr);
	} else {
		if (hevc->mmu_enable) {
			if (hevc->enable_ucode_swap) {
				loadr = amhevc_vdec_loadmc_ex(VFORMAT_HEVC, vdec,
						"hevc_mmu_swap", hevc->fw->data);
				if (loadr < 0) {
					loadr = amhevc_vdec_loadmc_ex(VFORMAT_HEVC, vdec,
						"h265_mmu", hevc->fw->data);
					hevc->enable_ucode_swap = false;
				} else
					hevc->is_swap = true;
			} else {
				loadr = amhevc_vdec_loadmc_ex(VFORMAT_HEVC, vdec,
						"h265_mmu", hevc->fw->data);
			}
		} else
			loadr = amhevc_vdec_loadmc_ex(VFORMAT_HEVC, vdec,
					NULL, hevc->fw->data);

		if (loadr < 0) {
			amhevc_disable();
			hevc_print(hevc, 0, "H265: the %s fw loading failed, err: %x\n",
				tee_enabled() ? "TEE" : "local", loadr);
			hevc->dec_result = DEC_RESULT_FORCE_EXIT;
			vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_FW_LOAD_ERROR);
			vdec_schedule_work(&hevc->work);
			return;
		}

		if (tee_enabled() && hevc->is_swap)
			hevc->swap_addr = READ_VREG(HEVC_STREAM_SWAP_BUFFER2);
#ifdef DETREFILL_ENABLE
		if (hevc->is_swap &&
			get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_GXM)
			init_detrefill_buf(hevc);
#endif
		vdec->mc_loaded = 1;
		vdec->mc_type = VFORMAT_HEVC;
	}
	ATRACE_COUNTER(hevc->trace.decode_run_time_name, TRACE_RUN_LOADING_FW_END);

	ATRACE_COUNTER(hevc->trace.decode_run_time_name, TRACE_RUN_LOADING_RESTORE_START);
	if (vh265_hw_ctx_restore(hevc) < 0) {
		vdec_schedule_work(&hevc->work);
		return;
	}
	ATRACE_COUNTER(hevc->trace.decode_run_time_name, TRACE_RUN_LOADING_RESTORE_END);
	vdec_enable_input(vdec);

	WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);

	if (vdec_frame_based(vdec)) {
		WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, 0);
		r = hevc->chunk->size + (hevc->chunk->offset & (VDEC_FIFO_ALIGN - 1));
		hevc->decode_size = r;
		if (vdec->mvfrm)
			vdec->mvfrm->frame_size = hevc->chunk->size;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	else {
		if (vdec->master || vdec->slave)
			WRITE_VREG(HEVC_SHIFT_BYTE_COUNT,
				hevc->shift_byte_count_lo);
	}
#endif
	WRITE_VREG(HEVC_DECODE_SIZE, r);
	/*WRITE_VREG(HEVC_DECODE_COUNT, hevc->decode_idx);*/
	hevc->init_flag = 1;

	if (hevc->pic_list_init_flag == 3)
		init_pic_list_hw(hevc);

	backup_decode_state(hevc);

	start_process_time(hevc);
	mod_timer(&hevc->timer, jiffies);
	hevc->stat |= STAT_TIMER_ARM;
	hevc->stat |= STAT_ISR_REG;
	if (vdec->mvfrm)
		vdec->mvfrm->hw_decode_start = local_clock();
	amhevc_start();
	hevc->stat |= STAT_VDEC_RUN;

	ATRACE_COUNTER(hevc->trace.decode_time_name, DECODER_RUN_END);
}

static void aml_free_canvas(struct vdec_s *vdec)
{
	int i;
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		struct PIC_s *pic = hevc->m_PIC[i];

		if (pic) {
			if (vdec->parallel_dec == 1) {
				vdec->free_canvas_ex(pic->y_canvas_index, vdec->id);
				vdec->free_canvas_ex(pic->uv_canvas_index, vdec->id);
			}
			pic->cma_alloc_addr = 0;
		}
		hevc->buffer_wrap[i] = i;
	}
}

static void reset(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	int i;

	cancel_work_sync(&hevc->work);
	if (hevc->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hevc->stat &= ~STAT_VDEC_RUN;
	}

	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}
	hevc->dec_result = DEC_RESULT_NONE;
	reset_process_time(hevc);
	hevc->pic_list_init_flag = 0;
	dealloc_mv_bufs(hevc);
	aml_free_canvas(vdec);
	if (!hevc->resolution_change)
		hevc_local_uninit(hevc);
	if (vh265_local_init(hevc) < 0)
		pr_debug(" %s local init fail\n", __func__);
	for (i = 0; i < BUF_POOL_SIZE; i++) {
		hevc->m_BUF[i].start_adr = 0;
	}

	atomic_set(&hevc->vf_pre_count, 0);
	atomic_set(&hevc->vf_get_count, 0);
	atomic_set(&hevc->vf_put_count, 0);
	hevc->eos = 0;
	hevc->resolution_change = false;

	hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL, "%s\r\n", __func__);
}

static irqreturn_t vh265_irq_cb(struct vdec_s *vdec, int irq)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	return vh265_isr(0, hevc);
}

static irqreturn_t vh265_threaded_irq_cb(struct vdec_s *vdec, int irq)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	return vh265_isr_thread_fn(0, hevc);
}
#endif

/****************************************/
#ifdef CONFIG_PM
static int h265_suspend(struct device *dev)
{
	amhevc_suspend(to_platform_device(dev), dev->power.power_state);
	return 0;
}

static int h265_resume(struct device *dev)
{
	amhevc_resume(to_platform_device(dev));
	return 0;
}

static const struct dev_pm_ops h265_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(h265_suspend, h265_resume)
};
#endif

#ifdef MULTI_INSTANCE_SUPPORT
static void vh265_dump_state(struct vdec_s *vdec)
{
	int i;
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	hevc_print(hevc, 0, "====== %s\n", __func__);

	hevc_print(hevc, 0,
		"width/height (%d/%d), reorder_pic_num %d ip_mode %d buf count(bufspec size) %d, video_signal_type 0x%x, is_swap %d i_only 0x%x\n",
		hevc->frame_width,
		hevc->frame_height,
		hevc->sps_num_reorder_pics_0,
		hevc->ip_mode,
		hevc->used_buf_num,
		hevc->video_signal_type_debug,
		hevc->is_swap,
		hevc->i_only);

	hevc_print(hevc, 0,
		"is_framebase(%d), eos %d, dec_result 0x%x dec_frm %d disp_frm %d run %d not_run_ready %d input_empty %d error_frame_count %d drop_frame_count %d\n",
		input_frame_based(vdec),
		hevc->eos,
		hevc->dec_result,
		decode_frame_count[hevc->index],
		display_frame_count[hevc->index],
		run_count[hevc->index],
		not_run_ready[hevc->index],
		input_empty[hevc->index],
		hevc->gvs->error_frame_count,
		hevc->gvs->drop_frame_count);

	if (vf_get_receiver(vdec->vf_provider_name)) {
		enum receviver_start_e state =
		vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_QUREY_STATE, NULL);
		hevc_print(hevc, 0,
			"\nreceiver(%s) state %d\n",
			vdec->vf_provider_name,
			state);
	}

	hevc_print(hevc, 0,
		"%s, newq(%d/%d), dispq(%d/%d), vf prepare/get/put (%d/%d/%d), pic_list_init_flag(%d), is_new_pic_available(%d)\n",
		__func__,
		kfifo_len(&hevc->newframe_q),
		VF_POOL_SIZE,
		kfifo_len(&hevc->display_q),
		VF_POOL_SIZE,
		hevc->vf_pre_count,
		hevc->vf_get_count,
		hevc->vf_put_count,
		hevc->pic_list_init_flag,
		is_new_pic_available(hevc));

	dump_pic_list(hevc);

	for (i = 0; i < BUF_POOL_SIZE; i++) {
		hevc_print(hevc, 0,
			"Buf(%d) start_adr 0x%x header_addr 0x%x size 0x%x used %d\n",
			i,
			hevc->m_BUF[i].start_adr,
			hevc->m_BUF[i].header_addr,
			hevc->m_BUF[i].size,
			hevc->m_BUF[i].used_flag);
	}

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		hevc_print(hevc, 0,
			"mv_Buf(%d) start_adr 0x%x size 0x%x used %d\n",
			i,
			hevc->m_mv_BUF[i].start_adr,
			hevc->m_mv_BUF[i].size,
			hevc->m_mv_BUF[i].used_flag);
	}

	hevc_print(hevc, 0,
		"HEVC_DEC_STATUS_REG=0x%x\n",
		READ_VREG(HEVC_DEC_STATUS_REG));
	hevc_print(hevc, 0,
		"HEVC_MPC_E=0x%x\n",
		READ_VREG(HEVC_MPC_E));
	hevc_print(hevc, 0,
		"HEVC_DECODE_MODE=0x%x\n",
		READ_VREG(HEVC_DECODE_MODE));
	hevc_print(hevc, 0,
		"HEVC_DECODE_MODE2=0x%x\n",
		READ_VREG(HEVC_DECODE_MODE2));
	hevc_print(hevc, 0,
		"NAL_SEARCH_CTL=0x%x\n",
		READ_VREG(NAL_SEARCH_CTL));
	hevc_print(hevc, 0,
		"HEVC_PARSER_LCU_START=0x%x\n",
		READ_VREG(HEVC_PARSER_LCU_START));
	hevc_print(hevc, 0,
		"HEVC_DECODE_SIZE=0x%x\n",
		READ_VREG(HEVC_DECODE_SIZE));
	hevc_print(hevc, 0,
		"HEVC_SHIFT_BYTE_COUNT=0x%x\n",
		READ_VREG(HEVC_SHIFT_BYTE_COUNT));
	hevc_print(hevc, 0,
		"HEVC_STREAM_START_ADDR=0x%x\n",
		READ_VREG(HEVC_STREAM_START_ADDR));
	hevc_print(hevc, 0,
		"HEVC_STREAM_END_ADDR=0x%x\n",
		READ_VREG(HEVC_STREAM_END_ADDR));
	hevc_print(hevc, 0,
		"HEVC_STREAM_LEVEL=0x%x\n",
		READ_VREG(HEVC_STREAM_LEVEL));
	hevc_print(hevc, 0,
		"HEVC_STREAM_WR_PTR=0x%x\n",
		READ_VREG(HEVC_STREAM_WR_PTR));
	hevc_print(hevc, 0,
		"HEVC_STREAM_RD_PTR=0x%x\n",
		READ_VREG(HEVC_STREAM_RD_PTR));
	hevc_print(hevc, 0,
		"PARSER_VIDEO_RP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_rp));
	hevc_print(hevc, 0,
		"PARSER_VIDEO_WP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_wp));

	if (input_frame_based(vdec) &&
		(get_dbg_flag(hevc) & PRINT_FRAMEBASE_DATA)) {
		int jj;
		PR_INIT(128);

		if (hevc->chunk && hevc->chunk->block &&
			hevc->chunk->size > 0) {
			u8 *data = NULL;
			if (!hevc->chunk->block->is_mapped)
				data = codec_mm_vmap(hevc->chunk->block->start +
					hevc->chunk->offset, hevc->chunk->size);
			else
				data = ((u8 *)hevc->chunk->block->start_virt)
					+ hevc->chunk->offset;
			hevc_print(hevc, 0,
				"frame data size 0x%x\n",
				hevc->chunk->size);
			for (jj = 0; jj < hevc->chunk->size; jj++) {
				if ((jj & 0xf) == 0)
					PR_FILL("%06x:", jj);
				PR_FILL("%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					PR_INFO(hevc->index);
			}
			PR_INFO(hevc->index);

			if (!hevc->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		}
	}

}


static int ammvdec_h265_probe(struct platform_device *pdev)
{

	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct hevc_state_s *hevc = NULL;
	int ret;
	int i;
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	int config_val;
#endif
	struct aml_vcodec_ctx *ctx = NULL;

	if (pdata == NULL) {
		pr_info("\nammvdec_h265 memory resource undefined.\n");
		return -EFAULT;
	}

	hevc = vmalloc(sizeof(struct hevc_state_s));
	if (hevc == NULL) {
		pr_info("\nammvdec_h265 device data allocation failed\n");
		return -ENOMEM;
	}
	memset(hevc, 0, sizeof(struct hevc_state_s));

	/* the ctx from v4l2 driver. */
	hevc->v4l2_ctx = pdata->private;
	ctx = hevc->v4l2_ctx;

	pdata->private = hevc;
	pdata->dec_status = vh265_dec_status;
	pdata->set_trickmode = vh265_set_trickmode;
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vh265_irq_cb;
	pdata->threaded_irq_handler = vh265_threaded_irq_cb;
	pdata->dump_state = vh265_dump_state;
#ifdef H265_USERDATA_ENABLE
	pdata->wakeup_userdata_poll = vh265_wakeup_userdata_poll;
	pdata->user_data_read = vh265_user_data_read;
	pdata->reset_userdata_fifo = vh265_reset_userdata_fifo;
#else
	pdata->wakeup_userdata_poll = NULL;
	pdata->user_data_read = NULL;
	pdata->reset_userdata_fifo = NULL;
#endif

	hevc->index = pdev->id;
	hevc->m_ins_flag = 1;


	if (is_rdma_enable()) {
		hevc->rdma_adr = decoder_dma_alloc_coherent(&hevc->rdma_mem_handle,
			RDMA_SIZE, &hevc->rdma_phy_adr, "H265_RDMA_BUF");
		for (i = 0; i < SCALELUT_DATA_WRITE_NUM; i++) {
			hevc->rdma_adr[i * 4] = HEVC_IQIT_SCALELUT_WR_ADDR & 0xfff;
			hevc->rdma_adr[i * 4 + 1] = i;
			hevc->rdma_adr[i * 4 + 2] = HEVC_IQIT_SCALELUT_DATA & 0xfff;
			hevc->rdma_adr[i * 4 + 3] = 0;
			if (i == SCALELUT_DATA_WRITE_NUM - 1) {
				hevc->rdma_adr[i * 4 + 2] = (HEVC_IQIT_SCALELUT_DATA & 0xfff) | 0x20000;
			}
		}
	}
	snprintf(hevc->trace.vdec_name, sizeof(hevc->trace.vdec_name),
		"h265-%d", hevc->index);
	snprintf(hevc->trace.pts_name, sizeof(hevc->trace.pts_name),
		"%s-timestamp", hevc->trace.vdec_name);
	snprintf(hevc->trace.vf_get_name, sizeof(hevc->trace.vf_get_name),
		"%s-vf_get", hevc->trace.vdec_name);
	snprintf(hevc->trace.vf_put_name, sizeof(hevc->trace.vf_put_name),
		"%s-vf_put", hevc->trace.vdec_name);
	snprintf(hevc->trace.set_canvas0_addr, sizeof(hevc->trace.set_canvas0_addr),
		"%s-set_canvas0_addr", hevc->trace.vdec_name);
	snprintf(hevc->trace.get_canvas0_addr, sizeof(hevc->trace.get_canvas0_addr),
		"%s-get_canvas0_addr", hevc->trace.vdec_name);
	snprintf(hevc->trace.put_canvas0_addr, sizeof(hevc->trace.put_canvas0_addr),
		"%s-put_canvas0_addr", hevc->trace.vdec_name);
	snprintf(hevc->trace.new_q_name, sizeof(hevc->trace.new_q_name),
		"%s-newframe_q", hevc->trace.vdec_name);
	snprintf(hevc->trace.disp_q_name, sizeof(hevc->trace.disp_q_name),
		"%s-dispframe_q", hevc->trace.vdec_name);
	snprintf(hevc->trace.new_q_name, sizeof(hevc->trace.new_q_name),
		"%s-newframe_q", hevc->trace.vdec_name);
	snprintf(hevc->trace.disp_q_name, sizeof(hevc->trace.disp_q_name),
		"%s-dispframe_q", hevc->trace.vdec_name);
	snprintf(hevc->trace.decode_time_name, sizeof(hevc->trace.decode_time_name),
		"decoder_time%d", pdev->id);
	snprintf(hevc->trace.decode_run_time_name, sizeof(hevc->trace.decode_run_time_name),
		"decoder_run_time%d", pdev->id);
	snprintf(hevc->trace.decode_header_memory_time_name, sizeof(hevc->trace.decode_header_memory_time_name),
		"decoder_header_time%d", pdev->id);
	snprintf(hevc->trace.decode_work_time_name, sizeof(hevc->trace.decode_work_time_name),
		"decoder_work_time%d", pdev->id);

	if (pdata->use_vfm_path) {
		snprintf(pdata->vf_provider_name,
		VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
		hevc->frameinfo_enable = 1;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	else if (vdec_dual(pdata)) {
		struct hevc_state_s *hevc_pair = NULL;

		if (dv_toggle_prov_name) /*debug purpose*/
			snprintf(pdata->vf_provider_name,
			VDEC_PROVIDER_NAME_SIZE,
				(pdata->master) ? VFM_DEC_DVBL_PROVIDER_NAME :
				VFM_DEC_DVEL_PROVIDER_NAME);
		else
			snprintf(pdata->vf_provider_name,
			VDEC_PROVIDER_NAME_SIZE,
				(pdata->master) ? VFM_DEC_DVEL_PROVIDER_NAME :
				VFM_DEC_DVBL_PROVIDER_NAME);
		hevc->dolby_enhance_flag = pdata->master ? 1 : 0;
		if (pdata->master)
			hevc_pair = (struct hevc_state_s *)pdata->master->private;
		else if (pdata->slave)
			hevc_pair = (struct hevc_state_s *)pdata->slave->private;
		if (hevc_pair)
			hevc->shift_byte_count_lo = hevc_pair->shift_byte_count_lo;
	}
#endif
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			MULTI_INSTANCE_PROVIDER_NAME ".%02x", pdev->id & 0xff);

	hevc->provider_name = pdata->vf_provider_name;
	platform_set_drvdata(pdev, pdata);

	hevc->platform_dev = pdev;
	hevc->nal_skip_policy = nal_skip_policy;  //default;

	if (((get_dbg_flag(hevc) & IGNORE_PARAM_FROM_CONFIG) == 0) &&
			pdata->config_len) {
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
		/*use ptr config for doubel_write_mode, etc*/
		hevc_print(hevc, 0, "pdata->config=%s\n", pdata->config);

		if (get_config_int(pdata->config, "hevc_double_write_mode",
				&config_val) == 0)
			hevc->double_write_mode = config_val;
		else
			hevc->double_write_mode = double_write_mode;

		if (get_config_int(pdata->config, "save_buffer_mode",
				&config_val) == 0)
			hevc->save_buffer_mode = config_val;
		else
			hevc->save_buffer_mode = 0;

		/*use ptr config for max_pic_w, etc*/
		if (get_config_int(pdata->config, "hevc_buf_width",
				&config_val) == 0) {
				hevc->max_pic_w = config_val;
		}
		if (get_config_int(pdata->config, "hevc_buf_height",
				&config_val) == 0) {
				hevc->max_pic_h = config_val;
		}
		if (get_config_int(pdata->config, "sidebind_type",
				&config_val) == 0)
			hevc->sidebind_type = config_val;

		if (get_config_int(pdata->config, "sidebind_channel_id",
				&config_val) == 0)
			hevc->sidebind_channel_id = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_codec_enable",
			&config_val) == 0)
			hevc->is_used_v4l = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_buffer_margin",
			&config_val) == 0)
			hevc->dynamic_buf_num_margin = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_canvas_mem_mode",
			&config_val) == 0)
			hevc->mem_map_mode = config_val;

		if (get_config_int(pdata->config, "negative_dv",
			&config_val) == 0) {
			hevc->discard_dv_data = config_val;
			hevc_print(hevc, 0, "discard dv data\n");
		}

		if (get_config_int(pdata->config,
			"parm_enable_fence",
			&config_val) == 0)
			hevc->enable_fence = config_val;

		if (get_config_int(pdata->config,
			"parm_fence_usage",
			&config_val) == 0)
			hevc->fence_usage = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_low_latency_mode",
			&config_val) == 0) {
			hevc->low_latency_flag = (config_val & 1) ? 1 : 0;
			hevc->enable_fence = (config_val & 2) ? 1 : 0;
		}

		if (get_config_int(pdata->config,
			"parm_v4l_metadata_config_flag",
			&config_val) == 0) {
			hevc->metadata_config_flag = config_val;
			hevc->discard_dv_data = config_val & VDEC_CFG_FLAG_DV_NEGATIVE;
			hevc->dv_duallayer = config_val & VDEC_CFG_FLAG_DV_TWOLARYER;
			if (config_val & VDEC_CFG_FLAG_DIS_ERR_POLICY) {
				hevc->nal_skip_policy = c2_nal_skip_policy;
			}
			hevc->high_bandwidth_flag = config_val & VDEC_CFG_FLAG_HIGH_BANDWIDTH;
			if (hevc->discard_dv_data)
				hevc_print(hevc, 0, "discard dv data\n");
			if (hevc->dv_duallayer)
				hevc_print(hevc, 0, "dv_duallayer\n");
			if (hevc->high_bandwidth_flag)
				hevc_print(hevc, 0, "high bandwidth\n");
		}
#endif
	} else {
		if (pdata->sys_info)
			hevc->vh265_amstream_dec_info = *pdata->sys_info;
		else {
			hevc->vh265_amstream_dec_info.width = 0;
			hevc->vh265_amstream_dec_info.height = 0;
			hevc->vh265_amstream_dec_info.rate = 30;
		}
		hevc->double_write_mode = double_write_mode;
	}

	if (force_config_fence) {
		hevc->enable_fence = true;
		hevc->fence_usage = (force_config_fence >> 4) & 0xf;
		if (force_config_fence & 0x2)
			hevc->enable_fence = false;
		hevc_print(hevc, 0,
			"enable fence: %d, fence usage: %d\n",
			hevc->enable_fence, hevc->fence_usage);
	}

	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5) &&
			(hevc->double_write_mode == 3))
		hevc->double_write_mode = 0x1000;

	hevc->endian = HEVC_CONFIG_LITTLE_ENDIAN;

	if (endian)
		hevc->endian = endian;

	if (mmu_enable_force == 0) {
		if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_GXL
			|| hevc->double_write_mode == 0x10)
			hevc->mmu_enable = 0;
		else
			hevc->mmu_enable = 1;
	}

	if (hevc->double_write_mode & 0x10)
		hevc->mmu_enable = 0;
	else
		hevc->mmu_enable = 1;

	if (hevc->v4l2_ctx != NULL) {
		struct aml_vcodec_ctx *ctx = hevc->v4l2_ctx;

		ctx->aux_infos.alloc_buffer(ctx, SEI_TYPE);

		if (!hevc->discard_dv_data)
			ctx->aux_infos.alloc_buffer(ctx, DV_TYPE);
	}

	if (init_mmu_buffers(hevc, 1) < 0) {
		hevc_print(hevc, 0, "\n 265 mmu init failed!\n");
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_NO_MEM);
		mutex_unlock(&vh265_mutex);
		if (hevc)
			vfree((void *)hevc);
		pdata->dec_status = NULL;
		return -EFAULT;
	}

	ret = decoder_bmmu_box_alloc_buf_phy(hevc->bmmu_box,
			BMMU_WORKSPACE_ID, work_buf_size,
			DRIVER_NAME, &hevc->buf_start);
	if (ret < 0) {
		uninit_mmu_buffers(hevc);
		/* devm_kfree(&pdev->dev, (void *)hevc); */
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_NO_MEM);
		if (hevc)
			vfree((void *)hevc);
		pdata->dec_status = NULL;
		mutex_unlock(&vh265_mutex);
		return ret;
	}
	hevc->buf_size = work_buf_size;

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXTVBB) &&
		(parser_sei_enable & 0x100) == 0)
		parser_sei_enable = 7;
	hevc->init_flag = 0;
	hevc->first_sc_checked = 0;
	hevc->uninit_list = 0;
	hevc->fatal_error = 0;
	hevc->show_frame_num = 0;

	/*
	 *hevc->mc_buf_spec.buf_end = pdata->mem_end + 1;
	 *for (i = 0; i < WORK_BUF_SPEC_NUM; i++)
	 *	amvh265_workbuff_spec[i].start_adr = pdata->mem_start;
	 */
	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0,
			"===H.265 decoder mem resource 0x%lx size 0x%x\n",
			   hevc->buf_start, hevc->buf_size);
	}

	hevc_print(hevc, 0,
		"dynamic_buf_num_margin=%d\n",
		hevc->dynamic_buf_num_margin);
	hevc_print(hevc, 0,
		"double_write_mode=%d\n",
		hevc->double_write_mode);
	hevc_print(hevc, 0,
		"nal_skip_policy=%d\n",
		hevc->nal_skip_policy);

	hevc->cma_dev = pdata->cma_dev;
	vh265_vdec_info_init(hevc);

	if (vh265_init(pdata) < 0) {
		hevc_print(hevc, 0,
			"\namvdec_h265 init failed.\n");
		hevc_local_uninit(hevc);
		if (hevc->gvs)
			kfree(hevc->gvs);
		hevc->gvs = NULL;
		uninit_mmu_buffers(hevc);
		/* devm_kfree(&pdev->dev, (void *)hevc); */
		if (hevc)
			vfree((void *)hevc);
		pdata->dec_status = NULL;
		return -ENODEV;
	}

#ifdef AUX_DATA_CRC
	vdec_aux_data_check_init(pdata);
#endif

#ifdef H265_USERDATA_ENABLE
	vh265_crate_userdata_manager(hevc, hevc->sei_user_data_buffer, USER_DATA_SIZE);
#endif

	vdec_set_prepare_level(pdata, start_decode_buf_level);

	/*set the max clk for smooth playing...*/
	hevc_source_changed(VFORMAT_HEVC,
			3840, 2160, 60);
	if (pdata->parallel_dec == 1)
		vdec_core_request(pdata, CORE_MASK_HEVC);
	else
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
					| CORE_MASK_COMBINE);
	mutex_init(&hevc->fence_mutex);
	if (hevc->enable_fence) {
		struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hevc->v4l2_ctx);

		pdata->sync = vdec_sync_get();
		if (!pdata->sync) {
			hevc_print(hevc, 0, "alloc fence timeline error\n");
			hevc_local_uninit(hevc);
			if (hevc->gvs)
				kfree(hevc->gvs);
			hevc->gvs = NULL;
			uninit_mmu_buffers(hevc);
			/* devm_kfree(&pdev->dev, (void *)hevc); */
			if (hevc)
				vfree((void *)hevc);
			pdata->dec_status = NULL;
			return -ENODEV;
		}
		ctx->sync = pdata->sync;
		pdata->sync->usage = hevc->fence_usage;
		/* creat timeline. */
		vdec_timeline_create(pdata->sync, DRIVER_NAME);
		vdec_timeline_get(pdata->sync);
	}

	return 0;
}

static void vdec_fence_release(struct hevc_state_s *hw,
			       struct vdec_sync *sync)
{
	ulong expires;

	/* notify signal to wake up all fences. */
	vdec_timeline_increase(sync, VF_POOL_SIZE);

	expires = jiffies + msecs_to_jiffies(2000);
	while (!check_objs_all_signaled(sync)) {
		if (time_after(jiffies, expires)) {
			pr_err("wait fence signaled timeout.\n");
			break;
		}
	}

	/* decreases refcnt of timeline. */
	vdec_timeline_put(sync);
}

static int ammvdec_h265_remove(struct platform_device *pdev)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	struct vdec_s *vdec;

	if (hevc == NULL)
		return 0;
	vdec = hw_to_vdec(hevc);

#ifdef AUX_DATA_CRC
	vdec_aux_data_check_exit(vdec);
#endif

	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0, "%s\r\n", __func__);

	vmh265_stop(hevc);
#ifdef H265_USERDATA_ENABLE
		vh265_destroy_userdata_manager(hevc);
#endif

	/* vdec_source_changed(VFORMAT_H264, 0, 0, 0); */
	if (vdec->parallel_dec == 1)
		vdec_core_release(hw_to_vdec(hevc), CORE_MASK_HEVC);
	else
		vdec_core_release(hw_to_vdec(hevc), CORE_MASK_HEVC);

	vdec_set_status(hw_to_vdec(hevc), VDEC_STATUS_DISCONNECTED);

	if (hevc->enable_fence)
		vdec_fence_release(hevc, vdec->sync);
	if (is_rdma_enable())
		decoder_dma_free_coherent(hevc->rdma_mem_handle,
			RDMA_SIZE, hevc->rdma_adr, hevc->rdma_phy_adr);
	vfree((void *)hevc);

	return 0;
}

static struct platform_driver ammvdec_h265_driver = {
	.probe = ammvdec_h265_probe,
	.remove = ammvdec_h265_remove,
	.driver = {
		.name = MULTI_DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &h265_pm_ops,
#endif
	}
};
#endif

static struct codec_profile_t amvdec_h265_profile = {
	.name = "H.265-V4L",
	.profile = ""
};

static struct mconfig h265_configs[] = {
	MC_PU32("use_cma", &use_cma),
	MC_PU32("bit_depth_luma", &bit_depth_luma),
	MC_PU32("bit_depth_chroma", &bit_depth_chroma),
	MC_PU32("video_signal_type", &video_signal_type),
	MC_PU32("radr", &radr),
	MC_PU32("rval", &rval),
	MC_PU32("dbg_cmd", &dbg_cmd),
	MC_PU32("dbg_skip_decode_index", &dbg_skip_decode_index),
	MC_PU32("endian", &endian),
	MC_PU32("step", &step),
	MC_PU32("udebug_flag", &udebug_flag),
	MC_PU32("decode_pic_begin", &decode_pic_begin),
	MC_PU32("slice_parse_begin", &slice_parse_begin),
	MC_PU32("nal_skip_policy", &nal_skip_policy),
	MC_PU32("c2_nal_skip_policy", &c2_nal_skip_policy),
	MC_PU32("i_only_flag", &i_only_flag),
	MC_PU32("error_handle_policy", &error_handle_policy),
	MC_PU32("error_handle_threshold", &error_handle_threshold),
	MC_PU32("error_handle_nal_skip_threshold",
		&error_handle_nal_skip_threshold),
	MC_PU32("error_handle_system_threshold",
		&error_handle_system_threshold),
	MC_PU32("error_skip_nal_count", &error_skip_nal_count),
	MC_PU32("debug", &debug),
	MC_PU32("debug_mask", &debug_mask),
	MC_PU32("buffer_mode", &buffer_mode),
	MC_PU32("double_write_mode", &double_write_mode),
	MC_PU32("buf_alloc_width", &buf_alloc_width),
	MC_PU32("buf_alloc_height", &buf_alloc_height),
	MC_PU32("dynamic_buf_num_margin", &dynamic_buf_num_margin),
	MC_PU32("max_buf_num", &max_buf_num),
	MC_PU32("buf_alloc_size", &buf_alloc_size),
	MC_PU32("buffer_mode_dbg", &buffer_mode_dbg),
	MC_PU32("mem_map_mode", &mem_map_mode),
	MC_PU32("enable_mem_saving", &enable_mem_saving),
	MC_PU32("force_w_h", &force_w_h),
	MC_PU32("force_fps", &force_fps),
	MC_PU32("max_decoding_time", &max_decoding_time),
	MC_PU32("prefix_aux_buf_size", &prefix_aux_buf_size),
	MC_PU32("suffix_aux_buf_size", &suffix_aux_buf_size),
	MC_PU32("interlace_enable", &interlace_enable),
	MC_PU32("pts_unstable", &pts_unstable),
	MC_PU32("parser_sei_enable", &parser_sei_enable),
	MC_PU32("start_decode_buf_level", &start_decode_buf_level),
	MC_PU32("decode_timeout_val", &decode_timeout_val),
	MC_PU32("parser_dolby_vision_enable", &parser_dolby_vision_enable),
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	MC_PU32("dv_toggle_prov_name", &dv_toggle_prov_name),
	MC_PU32("dv_debug", &dv_debug),
#endif
};
static struct mconfig_node decoder_265_node;

static int __init amvdec_h265_driver_init_module(void)
{
	struct BuffInfo_s *p_buf_info;

	if (get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_TM2 && !is_cpu_tm2_revb()) {
		if (vdec_is_support_4k()) {
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
				p_buf_info = &amvh265_workbuff_spec[2];
			else
				p_buf_info = &amvh265_workbuff_spec[1];
		} else
			p_buf_info = &amvh265_workbuff_spec[0];
	} else { //get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_TM2 || is_cpu_tm2_revb()
		if (vdec_is_support_4k())
			p_buf_info = &amvh265_workbuff_spec[5];
		else
			p_buf_info = &amvh265_workbuff_spec[3];
	}

	init_buff_spec(NULL, p_buf_info);
	work_buf_size =
		(p_buf_info->end_adr - p_buf_info->start_adr
		 + 0xffff) & (~0xffff);

	pr_debug("amvdec_h265 module init\n");
	error_handle_policy = 0;
	udebug_flag = 0;
	decode_pic_begin = 0;
	slice_parse_begin = 0;
	step = 0;
	buf_alloc_size = 0;

	if (platform_driver_register(&ammvdec_h265_driver)) {
		pr_err("failed to register ammvdec_h265 driver\n");
		return -ENODEV;
	}

#if 1/*MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8*/
	if (!has_hevc_vdec()) {
		/* not support hevc */
		amvdec_h265_profile.name = "hevc_unsupport";
	}
	if ((vdec_is_support_4k())) {
		if (is_meson_m8m2_cpu()) {
			/* m8m2 support 4k */
			amvdec_h265_profile.profile = "4k";
		} else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) {
			amvdec_h265_profile.profile =
				"8k, 8bit, 10bit, dwrite, compressed, frame_dv, fence, uvm, multi_frame_dv";
		}else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXBB) {
			amvdec_h265_profile.profile =
				"4k, 8bit, 10bit, dwrite, compressed, frame_dv, fence, uvm, multi_frame_dv";
		} else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_MG9TV)
			amvdec_h265_profile.profile = "4k";
	} else {
		if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D || is_cpu_s4_s805x2()) {
			amvdec_h265_profile.profile =
				"8bit, 10bit, dwrite, compressed, frame_dv, uvm, v4l, multi_frame_dv";
		} else {
			amvdec_h265_profile.profile =
				"8bit, 10bit, dwrite, compressed, v4l";
		}
	}
#endif
	if (codec_mm_get_total_size() < 80 * SZ_1M) {
		pr_info("amvdec_h265 default mmu enabled.\n");
		mmu_enable = 1;
	}

	vcodec_profile_register(&amvdec_h265_profile);
	INIT_REG_NODE_CONFIGS("media.decoder", &decoder_265_node,
		"h265-v4l", h265_configs, CONFIG_FOR_RW);
	vcodec_feature_register(VFORMAT_HEVC, 1);
	return 0;
}

static void __exit amvdec_h265_driver_remove_module(void)
{
	pr_debug("amvdec_h265 module remove.\n");

	platform_driver_unregister(&ammvdec_h265_driver);

}

/****************************************/
/*
 *module_param(stat, uint, 0664);
 *MODULE_PARM_DESC(stat, "\n amvdec_h265 stat\n");
 */
module_param(use_cma, uint, 0664);
MODULE_PARM_DESC(use_cma, "\n amvdec_h265 use_cma\n");

module_param(bit_depth_luma, uint, 0664);
MODULE_PARM_DESC(bit_depth_luma, "\n amvdec_h265 bit_depth_luma\n");

module_param(bit_depth_chroma, uint, 0664);
MODULE_PARM_DESC(bit_depth_chroma, "\n amvdec_h265 bit_depth_chroma\n");

module_param(video_signal_type, uint, 0664);
MODULE_PARM_DESC(video_signal_type, "\n amvdec_h265 video_signal_type\n");

module_param(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\n radr\n");

module_param(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\n rval\n");

module_param(dbg_cmd, uint, 0664);
MODULE_PARM_DESC(dbg_cmd, "\n dbg_cmd\n");

module_param(dump_nal, uint, 0664);
MODULE_PARM_DESC(dump_nal, "\n dump_nal\n");

module_param(dbg_skip_decode_index, uint, 0664);
MODULE_PARM_DESC(dbg_skip_decode_index, "\n dbg_skip_decode_index\n");

module_param(endian, uint, 0664);
MODULE_PARM_DESC(endian, "\n rval\n");

module_param(step, uint, 0664);
MODULE_PARM_DESC(step, "\n amvdec_h265 step\n");

module_param(decode_pic_begin, uint, 0664);
MODULE_PARM_DESC(decode_pic_begin, "\n amvdec_h265 decode_pic_begin\n");

module_param(slice_parse_begin, uint, 0664);
MODULE_PARM_DESC(slice_parse_begin, "\n amvdec_h265 slice_parse_begin\n");

module_param(nal_skip_policy, uint, 0664);
MODULE_PARM_DESC(nal_skip_policy, "\n amvdec_h265 nal_skip_policy\n");

module_param(c2_nal_skip_policy, uint, 0664);
MODULE_PARM_DESC(c2_nal_skip_policy, "\n amvdec_h265 c2_nal_skip_policy\n");

module_param(i_only_flag, uint, 0664);
MODULE_PARM_DESC(i_only_flag, "\n amvdec_h265 i_only_flag\n");

module_param(fast_output_enable, uint, 0664);
MODULE_PARM_DESC(fast_output_enable, "\n amvdec_h265 fast_output_enable\n");

module_param(error_handle_policy, uint, 0664);
MODULE_PARM_DESC(error_handle_policy, "\n amvdec_h265 error_handle_policy\n");

module_param(error_handle_threshold, uint, 0664);
MODULE_PARM_DESC(error_handle_threshold,
		"\n amvdec_h265 error_handle_threshold\n");

module_param(error_handle_nal_skip_threshold, uint, 0664);
MODULE_PARM_DESC(error_handle_nal_skip_threshold,
		"\n amvdec_h265 error_handle_nal_skip_threshold\n");

module_param(error_handle_system_threshold, uint, 0664);
MODULE_PARM_DESC(error_handle_system_threshold,
		"\n amvdec_h265 error_handle_system_threshold\n");

module_param(error_skip_nal_count, uint, 0664);
MODULE_PARM_DESC(error_skip_nal_count,
				 "\n amvdec_h265 error_skip_nal_count\n");

module_param(itu_t_t35_enable, int, 0664);
MODULE_PARM_DESC(itu_t_t35_enable, "\n amvdec_h265 itu_t_t35_enable\n");

module_param(skip_nal_count, uint, 0664);
MODULE_PARM_DESC(skip_nal_count, "\n skip_nal_count\n");

module_param(debug, uint, 0664);
MODULE_PARM_DESC(debug, "\n amvdec_h265 debug\n");

module_param(debug_mask, uint, 0664);
MODULE_PARM_DESC(debug_mask, "\n amvdec_h265 debug mask\n");

module_param(log_mask, uint, 0664);
MODULE_PARM_DESC(log_mask, "\n amvdec_h265 log_mask\n");

module_param(buffer_mode, uint, 0664);
MODULE_PARM_DESC(buffer_mode, "\n buffer_mode\n");

module_param(double_write_mode, uint, 0664);
MODULE_PARM_DESC(double_write_mode, "\n double_write_mode\n");

module_param(buf_alloc_width, uint, 0664);
MODULE_PARM_DESC(buf_alloc_width, "\n buf_alloc_width\n");

module_param(buf_alloc_height, uint, 0664);
MODULE_PARM_DESC(buf_alloc_height, "\n buf_alloc_height\n");

module_param(dynamic_buf_num_margin, uint, 0664);
MODULE_PARM_DESC(dynamic_buf_num_margin, "\n dynamic_buf_num_margin\n");

module_param(max_buf_num, uint, 0664);
MODULE_PARM_DESC(max_buf_num, "\n max_buf_num\n");

module_param(buf_alloc_size, uint, 0664);
MODULE_PARM_DESC(buf_alloc_size, "\n buf_alloc_size\n");

#ifdef CONSTRAIN_MAX_BUF_NUM
module_param(run_ready_max_vf_only_num, uint, 0664);
MODULE_PARM_DESC(run_ready_max_vf_only_num, "\n run_ready_max_vf_only_num\n");

module_param(run_ready_display_q_num, uint, 0664);
MODULE_PARM_DESC(run_ready_display_q_num, "\n run_ready_display_q_num\n");

module_param(run_ready_max_buf_num, uint, 0664);
MODULE_PARM_DESC(run_ready_max_buf_num, "\n run_ready_max_buf_num\n");

module_param(run_ready_min_buf_num, uint, 0664);
MODULE_PARM_DESC(run_ready_min_buf_num, "\n run_ready_min_buf_num\n");
#endif

#if 0
module_param(re_config_pic_flag, uint, 0664);
MODULE_PARM_DESC(re_config_pic_flag, "\n re_config_pic_flag\n");
#endif

module_param(buffer_mode_dbg, uint, 0664);
MODULE_PARM_DESC(buffer_mode_dbg, "\n buffer_mode_dbg\n");

module_param(mem_map_mode, uint, 0664);
MODULE_PARM_DESC(mem_map_mode, "\n mem_map_mode\n");

module_param(enable_mem_saving, uint, 0664);
MODULE_PARM_DESC(enable_mem_saving, "\n enable_mem_saving\n");

module_param(force_w_h, uint, 0664);
MODULE_PARM_DESC(force_w_h, "\n force_w_h\n");

module_param(force_fps, uint, 0664);
MODULE_PARM_DESC(force_fps, "\n force_fps\n");

module_param(max_decoding_time, uint, 0664);
MODULE_PARM_DESC(max_decoding_time, "\n max_decoding_time\n");

module_param(prefix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(prefix_aux_buf_size, "\n prefix_aux_buf_size\n");

module_param(suffix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(suffix_aux_buf_size, "\n suffix_aux_buf_size\n");

module_param(interlace_enable, uint, 0664);
MODULE_PARM_DESC(interlace_enable, "\n interlace_enable\n");
module_param(pts_unstable, uint, 0664);
MODULE_PARM_DESC(pts_unstable, "\n amvdec_h265 pts_unstable\n");
module_param(parser_sei_enable, uint, 0664);
MODULE_PARM_DESC(parser_sei_enable, "\n parser_sei_enable\n");

module_param(parser_dolby_vision_enable, uint, 0664);
MODULE_PARM_DESC(parser_dolby_vision_enable,
	"\n parser_dolby_vision_enable\n");

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
module_param(dolby_meta_with_el, uint, 0664);
MODULE_PARM_DESC(dolby_meta_with_el,
	"\n dolby_meta_with_el\n");

module_param(dolby_el_flush_th, uint, 0664);
MODULE_PARM_DESC(dolby_el_flush_th,
	"\n dolby_el_flush_th\n");
#endif
module_param(mmu_enable, uint, 0664);
MODULE_PARM_DESC(mmu_enable, "\n mmu_enable\n");

module_param(mmu_enable_force, uint, 0664);
MODULE_PARM_DESC(mmu_enable_force, "\n mmu_enable_force\n");

#ifdef MULTI_INSTANCE_SUPPORT
module_param(start_decode_buf_level, int, 0664);
MODULE_PARM_DESC(start_decode_buf_level,
		"\n h265 start_decode_buf_level\n");

module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val,
	"\n h265 decode_timeout_val\n");

module_param(print_lcu_error, uint, 0664);
MODULE_PARM_DESC(print_lcu_error,
	"\n h265 print_lcu_error\n");

module_param(data_resend_policy, uint, 0664);
MODULE_PARM_DESC(data_resend_policy,
	"\n h265 data_resend_policy\n");

module_param(poc_num_margin, int, 0664);
MODULE_PARM_DESC(poc_num_margin,
	"\n h265 poc_num_margin\n");

module_param(poc_error_limit, int, 0664);
MODULE_PARM_DESC(poc_error_limit,
	"\n h265 poc_error_limit\n");

module_param_array(decode_frame_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(display_frame_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(max_process_time, uint,
	&max_decode_instance_num, 0664);

module_param_array(max_get_frame_interval,
	uint, &max_decode_instance_num, 0664);

module_param_array(run_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(input_empty, uint,
	&max_decode_instance_num, 0664);

module_param_array(not_run_ready, uint,
	&max_decode_instance_num, 0664);

module_param_array(ref_frame_mark_flag, uint,
	&max_decode_instance_num, 0664);

#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
module_param(dv_toggle_prov_name, uint, 0664);
MODULE_PARM_DESC(dv_toggle_prov_name, "\n dv_toggle_prov_name\n");

module_param(dv_debug, uint, 0664);
MODULE_PARM_DESC(dv_debug, "\n dv_debug\n");

module_param(force_bypass_dvenl, uint, 0664);
MODULE_PARM_DESC(force_bypass_dvenl, "\n force_bypass_dvenl\n");
#endif

#ifdef AGAIN_HAS_THRESHOLD
module_param(again_threshold, uint, 0664);
MODULE_PARM_DESC(again_threshold, "\n again_threshold\n");
#endif

module_param(force_disp_pic_index, int, 0664);
MODULE_PARM_DESC(force_disp_pic_index,
	"\n amvdec_h265 force_disp_pic_index\n");

module_param(frmbase_cont_bitlevel, uint, 0664);
MODULE_PARM_DESC(frmbase_cont_bitlevel,	"\n frmbase_cont_bitlevel\n");

module_param(force_bufspec, uint, 0664);
MODULE_PARM_DESC(force_bufspec, "\n amvdec_h265 force_bufspec\n");

module_param(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n amvdec_h265 udebug_flag\n");

module_param(udebug_pause_pos, uint, 0664);
MODULE_PARM_DESC(udebug_pause_pos, "\n udebug_pause_pos\n");

module_param(udebug_pause_val, uint, 0664);
MODULE_PARM_DESC(udebug_pause_val, "\n udebug_pause_val\n");

module_param(pre_decode_buf_level, int, 0664);
MODULE_PARM_DESC(pre_decode_buf_level, "\n ammvdec_h264 pre_decode_buf_level\n");

module_param(udebug_pause_decode_idx, uint, 0664);
MODULE_PARM_DESC(udebug_pause_decode_idx, "\n udebug_pause_decode_idx\n");

module_param(disp_vframe_valve_level, uint, 0664);
MODULE_PARM_DESC(disp_vframe_valve_level, "\n disp_vframe_valve_level\n");

module_param(enable_swap, uint, 0664);
MODULE_PARM_DESC(enable_swap, "\n enable_swap\n");

module_param(pic_list_debug, uint, 0664);
MODULE_PARM_DESC(pic_list_debug, "\n pic_list_debug\n");

module_param(without_display_mode, uint, 0664);
MODULE_PARM_DESC(without_display_mode, "\n amvdec_h265 without_display_mode\n");

#ifdef HEVC_8K_LFTOFFSET_FIX
module_param(performance_profile, uint, 0664);
MODULE_PARM_DESC(performance_profile, "\n amvdec_h265 performance_profile\n");
#endif
module_param(disable_ip_mode, uint, 0664);
MODULE_PARM_DESC(disable_ip_mode, "\n amvdec_h265 disable ip_mode\n");

module_param(dirty_time_threshold, uint, 0664);
MODULE_PARM_DESC(dirty_time_threshold, "\n dirty_time_threshold\n");

module_param(dirty_count_threshold, uint, 0664);
MODULE_PARM_DESC(dirty_count_threshold, "\n dirty_count_threshold\n");

module_param(dirty_buffersize_threshold, uint, 0664);
MODULE_PARM_DESC(dirty_buffersize_threshold, "\n dirty_buffersize_threshold\n");

module_param(force_config_fence, uint, 0664);
MODULE_PARM_DESC(force_config_fence, "\n force enable fence\n");

module_param(mv_buf_dynamic_alloc, uint, 0664);
MODULE_PARM_DESC(mv_buf_dynamic_alloc, "\n mv_buf_dynamic_alloc\n");

module_param(detect_stuck_buffer_margin, uint, 0664);
MODULE_PARM_DESC(detect_stuck_buffer_margin, "\n detect_stuck_buffer_margin\n");

module_init(amvdec_h265_driver_init_module);
module_exit(amvdec_h265_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC h265 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <tim.yao@amlogic.com>");
