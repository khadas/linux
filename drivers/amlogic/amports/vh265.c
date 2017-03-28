 /*
 * drivers/amlogic/amports/vh265.c
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
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/amports/vformat.h>
#include <linux/amlogic/amports/ptsserv.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/slab.h>
#include "amports_priv.h"
#include <linux/amlogic/codec_mm/codec_mm.h>
#include "decoder/decoder_mmu_box.h"
#include "decoder/decoder_bmmu_box.h"
#include "config_parser.h"

/*#define HEVC_PIC_STRUCT_SUPPORT*/
#define MULTI_INSTANCE_SUPPORT



#define MMU_COMPRESS_HEADER_SIZE  0x48000
#define MAX_FRAME_4K_NUM 0x1200
#define FRAME_MMU_MAP_SIZE  (MAX_FRAME_4K_NUM * 4)
#define H265_MMU_MAP_BUFFER       HEVC_ASSIST_SCRATCH_7
#define HEVC_CM_HEADER_START_ADDR                  0x3628
#define HEVC_SAO_MMU_VH1_ADDR                      0x363b
#define HEVC_SAO_MMU_VH0_ADDR                      0x363a
#define HEVC_SAO_MMU_STATUS                        0x3639



#define MEM_NAME "codec_265"
/* #include <mach/am_regs.h> */
#include "vdec_reg.h"

#include "vdec.h"
#include "amvdec.h"
#include "video.h"

#define SUPPORT_10BIT
/* #define ERROR_HANDLE_DEBUG */
#if 0/*MESON_CPU_TYPE == MESON_CPU_TYPE_MESON8B*/
#undef SUPPORT_4K2K
#else
#define SUPPORT_4K2K
#endif

#ifndef STAT_KTHREAD
#define STAT_KTHREAD 0x40
#endif

#ifdef MULTI_INSTANCE_SUPPORT
#define MAX_DECODE_INSTANCE_NUM     12
#define MULTI_DRIVER_NAME "ammvdec_h265"
#endif
#define DRIVER_NAME "amvdec_h265"
#define MODULE_NAME "amvdec_h265"
#define DRIVER_HEADER_NAME "amvdec_h265_header"

#define PUT_INTERVAL        (HZ/100)
#define ERROR_SYSTEM_RESET_COUNT   200

#define PTS_NORMAL                0
#define PTS_NONE_REF_USE_DURATION 1

#define PTS_MODE_SWITCHING_THRESHOLD           3
#define PTS_MODE_SWITCHING_RECOVERY_THREASHOLD 3

#define DUR2PTS(x) ((x)*90/96)
#define HEVC_SIZE (4096*2304)

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

static int vh265_stop(struct hevc_state_s *hevc);
#ifdef MULTI_INSTANCE_SUPPORT
static int vmh265_stop(struct hevc_state_s *hevc);
static s32 vh265_init(struct vdec_s *vdec);
static bool run_ready(struct vdec_s *vdec);
static void reset_process_time(struct hevc_state_s *hevc);
static void start_process_time(struct hevc_state_s *hevc);
static void timeout_process(struct hevc_state_s *hevc);
#else
static s32 vh265_init(struct hevc_state_s *hevc);
#endif
static void vh265_prot_init(struct hevc_state_s *hevc);
static int vh265_local_init(struct hevc_state_s *hevc);
static void vh265_check_timer_func(unsigned long arg);
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

static struct vframe_provider_s vh265_vf_prov;

static u32 bit_depth_luma;
static u32 bit_depth_chroma;
static u32 video_signal_type;

static int start_decode_buf_level = 0x8000;

static unsigned int decode_timeout_val = 100;
#define VIDEO_SIGNAL_TYPE_AVAILABLE_MASK	0x20000000

static const char * const video_format_names[] = {
	"component", "PAL", "NTSC", "SECAM",
	"MAC", "unspecified", "unspecified", "unspecified"
};

static const char * const color_primaries_names[] = {
	"unknown", "bt709", "undef", "unknown",
	"bt470m", "bt470bg", "smpte170m", "smpte240m",
	"film", "bt2020"
};

static const char * const transfer_characteristics_names[] = {
	"unknown", "bt709", "undef", "unknown",
	"bt470m", "bt470bg", "smpte170m", "smpte240m",
	"linear", "log100", "log316", "iec61966-2-4",
	"bt1361e", "iec61966-2-1", "bt2020-10", "bt2020-12",
	"smpte-st-2084", "smpte-st-428"
};

static const char * const matrix_coeffs_names[] = {
	"GBR", "bt709", "undef", "unknown",
	"fcc", "bt470bg", "smpte170m", "smpte240m",
	"YCgCo", "bt2020nc", "bt2020c"
};

#ifdef SUPPORT_10BIT
#define HEVC_CM_BODY_START_ADDR                    0x3626
#define HEVC_CM_BODY_LENGTH                        0x3627
#define HEVC_CM_HEADER_LENGTH                      0x3629
#define HEVC_CM_HEADER_OFFSET                      0x362b
#define HEVC_SAO_CTRL9                             0x362d
#define LOSLESS_COMPRESS_MODE
/* DOUBLE_WRITE_MODE is enabled only when NV21 8 bit output is needed */
/* hevc->double_write_mode:
	0, no double write;
	1, 1:1 ratio;
	2, (1/4):(1/4) ratio;
	3, (1/4):(1/4) ratio, with both compressed frame included
	0x10, double write only
*/

static u32 double_write_mode;

/*#define DECOMP_HEADR_SURGENT*/

static u32 mem_map_mode; /* 0:linear 1:32x32 2:64x32 ; m8baby test1902 */
static u32 enable_mem_saving = 1;
static u32 workaround_enable;
static u32 force_w_h;
#endif
static u32 force_fps;
static u32 pts_unstable;
#define H265_DEBUG_BUFMGR                   0x01
#define H265_DEBUG_BUFMGR_MORE              0x02
#define H265_DEBUG_UCODE                    0x04
#define H265_DEBUG_REG                      0x08
#define H265_DEBUG_MAN_SEARCH_NAL           0x10
#define H265_DEBUG_MAN_SKIP_NAL             0x20
#define H265_DEBUG_DISPLAY_CUR_FRAME        0x40
#define H265_DEBUG_FORCE_CLK                0x80
#define H265_DEBUG_SEND_PARAM_WITH_REG      0x100
#define H265_DEBUG_NO_DISPLAY               0x200
#define H265_DEBUG_DISCARD_NAL              0x400
#define H265_DEBUG_OUT_PTS                  0x800
#define H265_DEBUG_PRINT_SEI		      0x2000
#define H265_DEBUG_PIC_STRUCT				0x4000
#define H265_DEBUG_DIS_LOC_ERROR_PROC       0x10000
#define H265_DEBUG_DIS_SYS_ERROR_PROC   0x20000
#define H265_DEBUG_DUMP_PIC_LIST       0x40000
#define H265_DEBUG_TRIG_SLICE_SEGMENT_PROC 0x80000
#define H265_DEBUG_HW_RESET               0x100000
#define H265_DEBUG_ERROR_TRIG             0x400000
#define H265_DEBUG_NO_EOS_SEARCH_DONE     0x800000
#define H265_DEBUG_NOT_USE_LAST_DISPBUF   0x1000000
#define H265_DEBUG_IGNORE_CONFORMANCE_WINDOW	0x2000000
#define H265_DEBUG_WAIT_DECODE_DONE_WHEN_STOP   0x4000000
#ifdef MULTI_INSTANCE_SUPPORT
#define IGNORE_PARAM_FROM_CONFIG		0x08000000
#define PRINT_FRAMEBASE_DATA            0x10000000
#define PRINT_FLAG_VDEC_STATUS             0x20000000
#define PRINT_FLAG_VDEC_DETAIL             0x40000000
#endif
#define MAX_BUF_NUM 24
#define MAX_HEADER_BUF_NUM (MAX_BUF_NUM)
#define MAX_REF_PIC_NUM 24
#define MAX_REF_ACTIVE  16

#define BMMU_MAX_BUFFERS (MAX_BUF_NUM + MAX_HEADER_BUF_NUM + 1) /* workspace */
#define VF_BUFFER_IDX(n)	(n)
#define HEADER_BUFFER_IDX(n) (MAX_BUF_NUM + n)
#define BMMU_WORKSPACE_ID	(MAX_BUF_NUM + MAX_HEADER_BUF_NUM)

const u32 h265_version = 201602101;
static u32 debug_mask = 0xffffffff;
static u32 debug;
static u32 radr;
static u32 rval;
static u32 dbg_cmd;
static u32 dump_nal;
static u32 dbg_skip_decode_index;
static u32 endian = 0xff0;
#ifdef ERROR_HANDLE_DEBUG
static u32 dbg_nal_skip_flag;
		/* bit[0], skip vps; bit[1], skip sps; bit[2], skip pps */
static u32 dbg_nal_skip_count;
#endif
/*for debug*/
static u32 decode_stop_pos;
static u32 decode_stop_pos_pre;
static u32 decode_pic_begin;
static uint slice_parse_begin;
static u32 step;
#ifdef MIX_STREAM_SUPPORT
#ifdef SUPPORT_4K2K
static u32 buf_alloc_width = 4096;
static u32 buf_alloc_height = 2304;
#else
static u32 buf_alloc_width = 1920;
static u32 buf_alloc_height = 1088;
#endif
static u32 dynamic_buf_num_margin;
#else
static u32 buf_alloc_width;
static u32 buf_alloc_height;
static u32 dynamic_buf_num_margin = 8;
#endif
static u32 max_buf_num = 16;
static u32 buf_alloc_size;
/*static u32 re_config_pic_flag;*/
/*
bit[0]: 0,
    bit[1]: 0, always release cma buffer when stop
    bit[1]: 1, never release cma buffer when stop
bit[0]: 1, when stop, release cma buffer if blackout is 1;
do not release cma buffer is blackout is not 1

bit[2]: 0, when start decoding, check current displayed buffer
	 (only for buffer decoded by h265) if blackout is 0
	 1, do not check current displayed buffer

bit[3]: 1, if blackout is not 1, do not release current
			displayed cma buffer always.
*/
/* set to 1 for fast play;
	set to 8 for other case of "keep last frame"
*/
static u32 buffer_mode = 1;

/* buffer_mode_dbg: debug only*/
static u32 buffer_mode_dbg = 0xffff0000;
/**/
/*
bit[1:0]PB_skip_mode: 0, start decoding at begin;
1, start decoding after first I;
2, only decode and display none error picture;
3, start decoding and display after IDR,etc
bit[31:16] PB_skip_count_after_decoding (decoding but not display),
only for mode 0 and 1.
 */
static u32 nal_skip_policy = 2;

/*
bit 0, 1: only display I picture;
bit 1, 1: only decode I picture;
*/
static u32 i_only_flag;

/*
use_cma: 1, use both reserver memory and cma for buffers
2, only use cma for buffers
*/
static u32 use_cma = 2;

#define AUX_BUF_ALIGN(adr) ((adr + 0xf) & (~0xf))
static u32 prefix_aux_buf_size = (16 * 1024);
static u32 suffix_aux_buf_size;

static u32 max_decoding_time;
/*
error handling
*/
/*error_handle_policy:
bit 0: 0, auto skip error_skip_nal_count nals before error recovery;
1, skip error_skip_nal_count nals before error recovery;
bit 1 (valid only when bit0 == 1):
1, wait vps/sps/pps after error recovery;
bit 2 (valid only when bit0 == 0):
0, auto search after error recovery (hevc_recover() called);
1, manual search after error recovery
(change to auto search after get IDR: WRITE_VREG(NAL_SEARCH_CTL, 0x2))

bit 4: 0, set error_mark after reset/recover
    1, do not set error_mark after reset/recover
bit 5: 0, check total lcu for every picture
    1, do not check total lcu

*/

static u32 error_handle_policy;
static u32 error_skip_nal_count = 6;
static u32 error_handle_threshold = 30;
static u32 error_handle_nal_skip_threshold = 10;
static u32 error_handle_system_threshold = 30;
static u32 interlace_enable = 1;
	/*
	parser_sei_enable:
	  bit 0, sei;
	  bit 1, sei_suffix (fill aux buf)
	  bit 2, fill sei to aux buf (when bit 0 is 1)
	  bit 8, debug flag
	*/
static u32 parser_sei_enable;
#ifdef CONFIG_AM_VDEC_DV
static u32 parser_dolby_vision_enable = 1;
static u32 dolby_meta_with_el;
#endif
/* this is only for h265 mmu enable */

static u32 mmu_enable = 1;
static u32 mmu_enable_force;

#ifdef MULTI_INSTANCE_SUPPORT
static u32 work_buf_size = 33 * 1024 * 1024;
static unsigned int max_decode_instance_num
				= MAX_DECODE_INSTANCE_NUM;
static unsigned int decode_frame_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int max_process_time[MAX_DECODE_INSTANCE_NUM];
static unsigned int max_get_frame_interval[MAX_DECODE_INSTANCE_NUM];

#ifdef CONFIG_MULTI_DEC
static unsigned char get_idx(struct hevc_state_s *hevc);
#endif

#ifdef CONFIG_AM_VDEC_DV
static u32 dv_toggle_prov_name;

static u32 dv_debug;
#endif
#endif


#ifdef CONFIG_MULTI_DEC
#define get_dbg_flag(hevc) ((debug_mask & (1 << hevc->index)) ? debug : 0)
#define get_dbg_flag2(hevc) ((debug_mask & (1 << get_idx(hevc))) ? debug : 0)
#else
#define get_dbg_flag(hevc) debug
#define get_dbg_flag2(hevc) debug
#define get_double_write_mode(hevc) double_write_mode
#define get_buf_alloc_width(hevc) buf_alloc_width
#define get_buf_alloc_height(hevc) buf_alloc_height
#define get_dynamic_buf_num_margin(hevc) dynamic_buf_num_margin
#endif
#define get_buffer_mode(hevc) buffer_mode


DEFINE_SPINLOCK(lock);
struct task_struct *h265_task = NULL;
#undef DEBUG_REG
#ifdef DEBUG_REG
void WRITE_VREG_DBG(unsigned adr, unsigned val)
{
	WRITE_VREG(adr, val);
}

#undef WRITE_VREG
#define WRITE_VREG WRITE_VREG_DBG
#endif

static DEFINE_MUTEX(vh265_mutex);

static struct vdec_info *gvs;

/**************************************************

h265 buffer management include

***************************************************/
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

#define HEVC_DATA_REQUEST           0x12

#define HEVC_DECODE_BUFEMPTY        0x20
#define HEVC_DECODE_TIMEOUT         0x21
#define HEVC_SEARCH_BUFEMPTY        0x22

#define HEVC_FIND_NEXT_PIC_NAL				0x50
#define HEVC_FIND_NEXT_DVEL_NAL				0x51

#define HEVC_DUMP_LMEM				0x30

#define HEVC_4k2k_60HZ_NOT_SUPPORT	0x80
#define HEVC_DISCARD_NAL         0xf0
#define HEVC_ACTION_ERROR        0xfe
#define HEVC_ACTION_DONE         0xff

/* --------------------------------------------------- */
/* Include "parser_cmd.h" */
/* --------------------------------------------------- */
#define PARSER_CMD_SKIP_CFG_0 0x0000090b

#define PARSER_CMD_SKIP_CFG_1 0x1b14140f

#define PARSER_CMD_SKIP_CFG_2 0x001b1910

#define PARSER_CMD_NUMBER 37

static unsigned short parser_cmd[PARSER_CMD_NUMBER] = {
	0x0401,
	0x8401,
	0x0800,
	0x0402,
	0x9002,
	0x1423,
	0x8CC3,
	0x1423,
	0x8804,
	0x9825,
	0x0800,
	0x04FE,
	0x8406,
	0x8411,
	0x1800,
	0x8408,
	0x8409,
	0x8C2A,
	0x9C2B,
	0x1C00,
	0x840F,
	0x8407,
	0x8000,
	0x8408,
	0x2000,
	0xA800,
	0x8410,
	0x04DE,
	0x840C,
	0x840D,
	0xAC00,
	0xA000,
	0x08C0,
	0x08E0,
	0xA40E,
	0xFC00,
	0x7C00
};

/**************************************************

h265 buffer management

***************************************************/
/* #define BUFFER_MGR_ONLY */
/* #define CONFIG_HEVC_CLK_FORCED_ON */
/* #define ENABLE_SWAP_TEST */
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
#define LMEM_DUMP_ADR                 HEVC_ASSIST_SCRATCH_F
#ifdef ENABLE_SWAP_TEST
#define HEVC_STREAM_SWAP_TEST     HEVC_ASSIST_SCRATCH_L
#endif

/*#define HEVC_DECODE_PIC_BEGIN_REG HEVC_ASSIST_SCRATCH_M*/
/*#define HEVC_DECODE_PIC_NUM_REG   HEVC_ASSIST_SCRATCH_N*/
#define HEVC_DECODE_SIZE		HEVC_ASSIST_SCRATCH_N
	/*do not define ENABLE_SWAP_TEST*/
#define HEVC_AUX_ADR			HEVC_ASSIST_SCRATCH_L
#define HEVC_AUX_DATA_SIZE		HEVC_ASSIST_SCRATCH_M

#define DEBUG_REG1              HEVC_ASSIST_SCRATCH_G
#define DEBUG_REG2              HEVC_ASSIST_SCRATCH_H
/*
ucode parser/search control
bit 0:  0, header auto parse; 1, header manual parse
bit 1:  0, auto skip for noneseamless stream; 1, no skip
bit [3:2]: valid when bit1==0;
0, auto skip nal before first vps/sps/pps/idr;
1, auto skip nal before first vps/sps/pps
2, auto skip nal before first  vps/sps/pps,
	and not decode until the first I slice (with slice address of 0)

3, auto skip before first I slice (nal_type >=16 && nal_type<=21)
bit [15:4] nal skip count (valid when bit0 == 1 (manual mode) )
bit [16]: for NAL_UNIT_EOS when bit0 is 0:
	0, send SEARCH_DONE to arm ;  1, do not send SEARCH_DONE to arm
bit [17]: for NAL_SEI when bit0 is 0:
	0, do not parse/fetch SEI in ucode;
	1, parse/fetch SEI in ucode
bit [18]: for NAL_SEI_SUFFIX when bit0 is 0:
	0, do not fetch NAL_SEI_SUFFIX to aux buf;
	1, fetch NAL_SEL_SUFFIX data to aux buf
bit [19]:
	0, parse NAL_SEI in ucode
	1, fetch NAL_SEI to aux buf
bit [20]: for DOLBY_VISION_META
	0, do not fetch DOLBY_VISION_META to aux buf
	1, fetch DOLBY_VISION_META to aux buf
*/
#define NAL_SEARCH_CTL            HEVC_ASSIST_SCRATCH_I
	/*read only*/
#define CUR_NAL_UNIT_TYPE       HEVC_ASSIST_SCRATCH_J
	/*set before start decoder*/
#define HEVC_DECODE_MODE		HEVC_ASSIST_SCRATCH_J
#define DECODE_STOP_POS         HEVC_ASSIST_SCRATCH_K

#define DECODE_MODE_SINGLE					0x0
#define DECODE_MODE_MULTI_FRAMEBASE			0x1
#define DECODE_MODE_MULTI_STREAMBASE		0x2
#define DECODE_MODE_MULTI_DVBAL				0x3
#define DECODE_MODE_MULTI_DVENL				0x4

#define MAX_INT 0x7FFFFFFF

#define RPM_BEGIN                                              0x100
#define modification_list_cur                                  0x140
#define RPM_END                                                0x180

#define RPS_USED_BIT        14
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
		unsigned short tile_width[4];
		unsigned short tile_height[4];
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
	struct buff_s mmu_vbh;
	struct buff_s cm_header;
	struct buff_s mpred_above;
	struct buff_s mpred_mv;
	struct buff_s rpm;
	struct buff_s lmem;
};
#define WORK_BUF_SPEC_NUM 2
static struct BuffInfo_s amvh265_workbuff_spec[WORK_BUF_SPEC_NUM] = {
	{
		/* 8M bytes */
		.max_width = 1920,
		.max_height = 1088,
		.ipp = {
			/* IPP work space calculation :
			   4096 * (Y+CbCr+Flags) = 12k, round to 16k */
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
			   total 64x16x2 = 2048 bytes (0x800) */
			.buf_size = 0x800,
		},
		.vps = {
			/* VPS STORE AREA - Max 16 VPS, each has 0x80 bytes,
			   total 0x0800 bytes */
			.buf_size = 0x800,
		},
		.sps = {
			/* SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			   total 0x0800 bytes */
			.buf_size = 0x800,
		},
		.pps = {
			/* PPS STORE AREA - Max 64 PPS, each has 0x80 bytes,
			   total 0x2000 bytes */
			.buf_size = 0x2000,
		},
		.sao_up = {
			/* SAO UP STORE AREA - Max 640(10240/16) LCU,
			   each has 16 bytes total 0x2800 bytes */
			.buf_size = 0x2800,
		},
		.swap_buf = {
			/* 256cyclex64bit = 2K bytes 0x800
			   (only 144 cycles valid) */
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
			/* support up to 32 SCALELUT 1024x32 =
			   32Kbytes (0x8000) */
			.buf_size = 0x8000,
		},
		.dblk_para = {
#ifdef SUPPORT_10BIT
			.buf_size = 0x40000,
#else
			/* DBLK -> Max 256(4096/16) LCU, each para
 512bytes(total:0x20000), data 1024bytes(total:0x40000) */
			.buf_size = 0x20000,
#endif
		},
		.dblk_data = {
			.buf_size = 0x40000,
		},
		.mmu_vbh = {
			.buf_size = 0x5000, /*2*16*2304/4, 4K*/
		},
		.cm_header = {/* 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)*/
			.buf_size = MMU_COMPRESS_HEADER_SIZE *
			(MAX_REF_PIC_NUM + 1),
		},
		.mpred_above = {
			.buf_size = 0x8000,
		},
		.mpred_mv = {/* 1080p, 0x40000 per buffer */
			.buf_size = 0x40000 * MAX_REF_PIC_NUM,
		},
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
			   4096 * (Y+CbCr+Flags) = 12k, round to 16k */
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
			   total 64x16x2 = 2048 bytes (0x800) */
			.buf_size = 0x800,
		},
		.vps = {
			/* VPS STORE AREA - Max 16 VPS, each has 0x80 bytes,
			   total 0x0800 bytes */
			.buf_size = 0x800,
		},
		.sps = {
			/* SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			   total 0x0800 bytes */
			.buf_size = 0x800,
		},
		.pps = {
			/* PPS STORE AREA - Max 64 PPS, each has 0x80 bytes,
			   total 0x2000 bytes */
			.buf_size = 0x2000,
		},
		.sao_up = {
			/* SAO UP STORE AREA - Max 640(10240/16) LCU,
			   each has 16 bytes total 0x2800 bytes */
			.buf_size = 0x2800,
		},
		.swap_buf = {
			/* 256cyclex64bit = 2K bytes 0x800
			   (only 144 cycles valid) */
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
			/* support up to 32 SCALELUT 1024x32 = 32Kbytes
			   (0x8000) */
			.buf_size = 0x8000,
		},
		.dblk_para = {
			/* DBLK -> Max 256(4096/16) LCU, each para
			   512bytes(total:0x20000),
			   data 1024bytes(total:0x40000) */
			.buf_size = 0x20000,
		},
		.dblk_data = {
			.buf_size = 0x40000,
		},
		.mmu_vbh = {
			.buf_size = 0x5000, /*2*16*2304/4, 4K*/
		},
		.cm_header = {/*0x44000 = ((1088*2*1024*4)/32/4)*(32/8)*/
			.buf_size = MMU_COMPRESS_HEADER_SIZE *
			(MAX_REF_PIC_NUM + 1),
		},
		.mpred_above = {
			.buf_size = 0x8000,
		},
		.mpred_mv = {
			/* .buf_size = 0x100000*16,
			//4k2k , 0x100000 per buffer */
			/* 4096x2304 , 0x120000 per buffer */
			.buf_size = 0x120000 * MAX_REF_PIC_NUM,
		},
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x500 * 2,
		}
	}
};

unsigned int get_mmu_mode(void)
{
	return mmu_enable;
}

#ifdef SUPPORT_10BIT
/* Losless compression body buffer size 4K per 64x32 (jt) */
static  int  compute_losless_comp_body_size(int width, int height,
	int mem_saving_mode)
{
	int width_x64;
	int     height_x32;
	int     bsize;

	width_x64 = width + 63;
	width_x64 >>= 6;

	height_x32 = height + 31;
	height_x32 >>= 5;
	if (mem_saving_mode == 1 && mmu_enable)
		bsize = 3200 * width_x64 * height_x32;
	else if (mem_saving_mode == 1)
		bsize = 3072 * width_x64 * height_x32;
	else
		bsize = 4096 * width_x64 * height_x32;

	return  bsize;
}

/* Losless compression header buffer size 32bytes per 128x64 (jt) */
static  int  compute_losless_comp_header_size(int width, int height)
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

static void init_buff_spec(struct hevc_state_s *hevc,
	struct BuffInfo_s *buf_spec)
{
	buf_spec->ipp.buf_start = buf_spec->start_adr;
	buf_spec->sao_abv.buf_start =
		buf_spec->ipp.buf_start + buf_spec->ipp.buf_size;

	buf_spec->sao_vb.buf_start =
		buf_spec->sao_abv.buf_start + buf_spec->sao_abv.buf_size;
	buf_spec->short_term_rps.buf_start =
		buf_spec->sao_vb.buf_start + buf_spec->sao_vb.buf_size;
	buf_spec->vps.buf_start =
		buf_spec->short_term_rps.buf_start +
		buf_spec->short_term_rps.buf_size;
	buf_spec->sps.buf_start =
		buf_spec->vps.buf_start + buf_spec->vps.buf_size;
	buf_spec->pps.buf_start =
		buf_spec->sps.buf_start + buf_spec->sps.buf_size;
	buf_spec->sao_up.buf_start =
		buf_spec->pps.buf_start + buf_spec->pps.buf_size;
	buf_spec->swap_buf.buf_start =
		buf_spec->sao_up.buf_start + buf_spec->sao_up.buf_size;
	buf_spec->swap_buf2.buf_start =
		buf_spec->swap_buf.buf_start + buf_spec->swap_buf.buf_size;
	buf_spec->scalelut.buf_start =
		buf_spec->swap_buf2.buf_start + buf_spec->swap_buf2.buf_size;
	buf_spec->dblk_para.buf_start =
		buf_spec->scalelut.buf_start + buf_spec->scalelut.buf_size;
	buf_spec->dblk_data.buf_start =
		buf_spec->dblk_para.buf_start + buf_spec->dblk_para.buf_size;
	buf_spec->mmu_vbh.buf_start  =
		buf_spec->dblk_data.buf_start + buf_spec->dblk_data.buf_size;
	buf_spec->cm_header.buf_start =
		buf_spec->mmu_vbh.buf_start + buf_spec->mmu_vbh.buf_size;
	buf_spec->mpred_above.buf_start =
		buf_spec->cm_header.buf_start + buf_spec->cm_header.buf_size;
	buf_spec->mpred_mv.buf_start =
		buf_spec->mpred_above.buf_start +
		buf_spec->mpred_above.buf_size;
	if (get_dbg_flag2(hevc) & H265_DEBUG_SEND_PARAM_WITH_REG) {
		buf_spec->end_adr =
			buf_spec->mpred_mv.buf_start +
			buf_spec->mpred_mv.buf_size;
	} else {
		buf_spec->rpm.buf_start =
			buf_spec->mpred_mv.buf_start +
			buf_spec->mpred_mv.buf_size;
		if (get_dbg_flag2(hevc) & H265_DEBUG_UCODE) {
			buf_spec->lmem.buf_start =
				buf_spec->rpm.buf_start +
				buf_spec->rpm.buf_size;
			buf_spec->end_adr =
				buf_spec->lmem.buf_start +
				buf_spec->lmem.buf_size;
		} else {
			buf_spec->end_adr =
				buf_spec->rpm.buf_start +
				buf_spec->rpm.buf_size;
		}
	}

	if (get_dbg_flag2(hevc)) {
		hevc_print(hevc, 0,
				"%s workspace (%x %x) size = %x\n", __func__,
			   buf_spec->start_adr, buf_spec->end_adr,
			   buf_spec->end_adr - buf_spec->start_adr);
	}
	if (get_dbg_flag2(hevc)) {
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
			"mpred_above.buf_start     :%x\n",
			buf_spec->mpred_above.buf_start);
		hevc_print(hevc, 0,
			"mpred_mv.buf_start        :%x\n",
			  buf_spec->mpred_mv.buf_start);
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
	int index;
	/*buffer */
	unsigned long start_adr;
	unsigned int size;

	unsigned int free_start_adr;
} /*BUF_t */;

/* level 6, 6.1 maximum slice number is 800; other is 200 */
#define MAX_SLICE_NUM 800
struct PIC_s {
	int index;
	int BUF_index;
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
	unsigned char used_by_display;
	/**/ int slice_idx;
	int m_aiRefPOCList0[MAX_SLICE_NUM][16];
	int m_aiRefPOCList1[MAX_SLICE_NUM][16];
	/*buffer */
	unsigned int header_adr;
#ifdef CONFIG_AM_VDEC_DV
	unsigned char dv_enhance_exist;
#endif
	char *aux_data_buf;
	int aux_data_size;
	unsigned long cma_alloc_addr;
	struct page *alloc_pages;
	unsigned int mpred_mv_wr_start_addr;
	unsigned int mc_y_adr;
	unsigned int mc_u_v_adr;
#ifdef SUPPORT_10BIT
	unsigned int comp_body_size;
	unsigned int dw_y_adr;
	unsigned int dw_u_v_adr;
#endif
	unsigned int buf_size;
	int mc_canvas_y;
	int mc_canvas_u_v;
	int width;
	int height;

	int y_canvas_index;
	int uv_canvas_index;
#ifdef MULTI_INSTANCE_SUPPORT
	struct canvas_config_s canvas_config[2];
#endif
#ifdef LOSLESS_COMPRESS_MODE
	unsigned int losless_comp_body_size;
#endif
	unsigned char pic_struct;
	int vf_ref;

	u32 pts;
	u64 pts64;
} /*PIC_t */;

#define MAX_TILE_COL_NUM    5
#define MAX_TILE_ROW_NUM    5
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

#define VF_POOL_SIZE        32

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

static void vh265_work(struct work_struct *work);
#endif
struct hevc_state_s {
#ifdef MULTI_INSTANCE_SUPPORT
	struct platform_device *platform_dev;
	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;
	struct vframe_chunk_s *chunk;
	int dec_result;
	struct work_struct work;
	/* timeout handle */
	unsigned long int start_process_time;
	unsigned last_lcu_idx;
	unsigned decode_timeout_count;
	unsigned timeout_num;
#ifdef CONFIG_AM_VDEC_DV
	unsigned char switch_dvlayer_flag;
#endif
	unsigned start_parser_type;
	unsigned char eos;
#endif
	char *provider_name;
	int index;
	struct device *cma_dev;
	unsigned char m_ins_flag;
	unsigned char dolby_enhance_flag;
	unsigned long buf_start;
	u32 buf_size;

	struct BuffInfo_s work_space_buf_store;
	struct BuffInfo_s *work_space_buf;
	struct buff_s *mc_buf;

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
	int iPrevTid0POC;
	int list_no;
	int RefNum_L0;
	int RefNum_L1;
	int ColFromL0Flag;
	int LongTerm_Curr;
	int LongTerm_Col;
	int Col_POC;
	int LongTerm_Ref;

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
	ignore_bufmgr_error;	/* bit 0, for decoding; bit 1, for displaying */
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

	int buf_num;
	int pic_num;

	/**/
	struct buff_s mc_buf_spec;
	union param_u param;

	struct tile_s m_tile[MAX_TILE_ROW_NUM][MAX_TILE_COL_NUM];

	struct timer_list timer;
	struct BUF_s m_BUF[MAX_BUF_NUM];
	u32 used_buf_num;
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
	unsigned char uninit_list;
	u32 start_decoding_time;

	int show_frame_num;
	struct semaphore h265_sema;
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

	unsigned int last_put_idx_a;
	unsigned int last_put_idx_b;

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
	int buf_alloc_width;
	int buf_alloc_height;
	int dynamic_buf_num_margin;
	int start_action;
#endif
	u32 i_only;
} /*hevc_stru_t */;

#ifdef CONFIG_MULTI_DEC
static int get_double_write_mode(struct hevc_state_s *hevc)
{
	return hevc->double_write_mode;
}

static int get_buf_alloc_width(struct hevc_state_s *hevc)
{
	return hevc->buf_alloc_width;
}

static int get_buf_alloc_height(struct hevc_state_s *hevc)
{
	return hevc->buf_alloc_height;
}

static int get_dynamic_buf_num_margin(struct hevc_state_s *hevc)
{
	return hevc->dynamic_buf_num_margin;
}
#endif

#ifdef CONFIG_MULTI_DEC
static unsigned char get_idx(struct hevc_state_s *hevc)
{
	return hevc->index;
}
#endif

#undef pr_info
#define pr_info printk
static int hevc_print(struct hevc_state_s *hevc,
	int flag, const char *fmt, ...)
{
#define HEVC_PRINT_BUF		128
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
#ifdef CONFIG_MULTI_DEC
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
		pr_info("%s", buf);
		va_end(args);
#ifdef CONFIG_MULTI_DEC
	}
#endif
	return 0;
}

static int hevc_print_cont(struct hevc_state_s *hevc,
	int flag, const char *fmt, ...)
{
#define HEVC_PRINT_BUF		128
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
#ifdef CONFIG_MULTI_DEC
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
#ifdef CONFIG_MULTI_DEC
	}
#endif
	return 0;
}

static void set_canvas(struct hevc_state_s *hevc, struct PIC_s *pic);

static void release_aux_data(struct hevc_state_s *hevc,
	struct PIC_s *pic);

static void hevc_init_stru(struct hevc_state_s *hevc,
		struct BuffInfo_s *buf_spec_i,
		struct buff_s *mc_buf_i)
{
	int i;
	hevc->work_space_buf = buf_spec_i;
	hevc->mc_buf = mc_buf_i;
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
	/* int m_uiMaxCUWidth = 1<<7; */
	/* int m_uiMaxCUHeight = 1<<7; */
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
	hevc->shift_byte_count_lo = 0;
	hevc->shift_byte_count_hi = 0;
	hevc->pts_mode_switching_count = 0;
	hevc->pts_mode_recovery_count = 0;

	hevc->PB_skip_mode = nal_skip_policy & 0x3;
	hevc->PB_skip_count_after_decoding = (nal_skip_policy >> 16) & 0xffff;
	if (hevc->PB_skip_mode == 0)
		hevc->ignore_bufmgr_error = 0x1;
	else
		hevc->ignore_bufmgr_error = 0x0;

	for (i = 0; i < MAX_REF_PIC_NUM; i++)
		hevc->m_PIC[i] = NULL;
	hevc->buf_num = 0;
	hevc->pic_num = 0;
	hevc->lcu_x_num_pre = 0;
	hevc->lcu_y_num_pre = 0;
	hevc->first_pic_after_recover = 0;

	hevc->pre_top_pic = NULL;
	hevc->pre_bot_pic = NULL;

	hevc->sei_present_flag = 0;
#ifdef MULTI_INSTANCE_SUPPORT
	hevc->start_process_time = 0;
	hevc->last_lcu_idx = 0;
	hevc->decode_timeout_count = 0;
	hevc->timeout_num = 0;
	hevc->eos = 0;
#endif
}

static int prepare_display_buf(struct hevc_state_s *hevc, struct PIC_s *pic);
static int H265_alloc_mmu(struct hevc_state_s *hevc,
			struct PIC_s *new_pic,	unsigned short bit_depth,
			unsigned int *mmu_index_adr);

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
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
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
		if (pic == NULL || pic->index == -1)
			continue;
		if ((pic->POC == POC) && (pic->referenced)) {
			if (ret_pic == NULL)
				ret_pic = pic;
			else {
				if (pic->decode_idx > ret_pic->decode_idx)
					ret_pic = pic;
			}
		}
	}

	if (ret_pic == NULL) {
		if (get_dbg_flag(hevc)) {
			hevc_print(hevc, 0,
				"Wrong, POC of %d is not in referenced list\n",
				   POC);
		}
		ret_pic = get_pic_by_POC(hevc, POC);
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

static int init_buf_spec(struct hevc_state_s *hevc);

static void uninit_mmu_buffers(struct hevc_state_s *hevc)
{

	if (hevc->mmu_box)
		decoder_mmu_box_free(hevc->mmu_box);
	hevc->mmu_box = NULL;

	if (hevc->bmmu_box)
		decoder_bmmu_box_free(hevc->bmmu_box);
	hevc->bmmu_box = NULL;
}
static int init_mmu_buffers(struct hevc_state_s *hevc)
{
	int tvp_flag = vdec_secure(hw_to_vdec(hevc)) ?
		CODEC_MM_FLAGS_TVP : 0;

	if (mmu_enable) {
		hevc->mmu_box = decoder_mmu_box_alloc_box(DRIVER_NAME,
			hevc->index,
			MAX_REF_PIC_NUM,
			64 * SZ_1M,
			tvp_flag
			);
		if (!hevc->mmu_box) {
			pr_err("h265 alloc mmu box failed!!\n");
			return -1;
		}
	}
	hevc->bmmu_box = decoder_bmmu_box_alloc_box(DRIVER_NAME,
			hevc->index,
			BMMU_MAX_BUFFERS,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER |
			tvp_flag);
	if (!hevc->bmmu_box) {
		if (hevc->mmu_box)
			decoder_mmu_box_free(hevc->mmu_box);
		hevc->mmu_box = NULL;
		pr_err("h265 alloc mmu box failed!!\n");
		return -1;
	}
	return 0;
}

static void init_buf_list(struct hevc_state_s *hevc)
{
	int i;
	int buf_size;
	int mc_buffer_end = hevc->mc_buf->buf_start + hevc->mc_buf->buf_size;

	if (get_dynamic_buf_num_margin(hevc) > 0)
		hevc->used_buf_num = hevc->sps_num_reorder_pics_0
			+ get_dynamic_buf_num_margin(hevc);
	else
		hevc->used_buf_num = max_buf_num;

	if (hevc->used_buf_num > MAX_BUF_NUM)
		hevc->used_buf_num = MAX_BUF_NUM;
	if (buf_alloc_size > 0) {
		buf_size = buf_alloc_size;
		if (get_dbg_flag(hevc))
			hevc_print(hevc, 0,
				"[Buffer Management] init_buf_list:\n");
	} else {
		int pic_width = get_buf_alloc_width(hevc)
		? get_buf_alloc_width(hevc) : hevc->pic_w;
		int pic_height =
			get_buf_alloc_height(hevc)
			? get_buf_alloc_height(hevc) : hevc->pic_h;
#ifdef LOSLESS_COMPRESS_MODE
/*SUPPORT_10BIT*/
	int losless_comp_header_size = compute_losless_comp_header_size
			(pic_width, pic_height);
	int losless_comp_body_size = compute_losless_comp_body_size
			(pic_width, pic_height, hevc->mem_saving_mode);
	int mc_buffer_size = losless_comp_header_size
		+ losless_comp_body_size;
	int mc_buffer_size_h = (mc_buffer_size + 0xffff)>>16;
	if (get_double_write_mode(hevc)) {
		int pic_width_dw = ((get_double_write_mode(hevc) == 2) ||
			(get_double_write_mode(hevc) == 3)) ?
			pic_width / 4 : pic_width;
		int pic_height_dw = ((get_double_write_mode(hevc) == 2) ||
			(get_double_write_mode(hevc) == 3)) ?
			pic_height / 4 : pic_height;
		int lcu_size = hevc->lcu_size;
		int pic_width_lcu  = (pic_width_dw % lcu_size)
			? pic_width_dw / lcu_size
			+ 1 : pic_width_dw / lcu_size;
		int pic_height_lcu = (pic_height_dw % lcu_size)
			? pic_height_dw / lcu_size
				+ 1 : pic_height_dw / lcu_size;
		int lcu_total = pic_width_lcu * pic_height_lcu;
		int mc_buffer_size_u_v = lcu_total * lcu_size * lcu_size / 2;
		int mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff) >> 16;
			/*64k alignment*/
		buf_size = ((mc_buffer_size_u_v_h << 16) * 3);
	} else
		buf_size = 0;

	if (mc_buffer_size & 0xffff) { /*64k alignment*/
		mc_buffer_size_h += 1;
	}
	if ((!mmu_enable) &&
		((get_double_write_mode(hevc) & 0x10) == 0)) {
		/* use compress mode without mmu,
			need buf for compress decoding*/
		buf_size += (mc_buffer_size_h << 16);
	}
#else
		int lcu_size = hevc->lcu_size;
		int pic_width_lcu =
			(pic_width % lcu_size) ? pic_width / lcu_size
				+ 1 : pic_width / lcu_size;
		int pic_height_lcu =
			(pic_height % lcu_size) ? pic_height / lcu_size
				+ 1 : pic_height / lcu_size;
		int lcu_total = pic_width_lcu * pic_height_lcu;
		int mc_buffer_size_u_v = lcu_total * lcu_size * lcu_size / 2;
			int mc_buffer_size_u_v_h =
				(mc_buffer_size_u_v + 0xffff) >> 16;
					/*64k alignment*/
		buf_size = (mc_buffer_size_u_v_h << 16) * 3;
#endif
		if (get_dbg_flag(hevc)) {
			hevc_print(hevc, 0,
			"init_buf_list num %d (width %d height %d):\n",
			 hevc->used_buf_num, pic_width, pic_height);
		}
	}

	hevc_print(hevc, 0, "allocate begin\n");
	get_cma_alloc_ref();
	/*alloc compress header first*/
	for (i = 0; i < hevc->used_buf_num + 1; i++) {
		unsigned long buf_addr;
		if (decoder_bmmu_box_alloc_buf_phy
				(hevc->bmmu_box,
				HEADER_BUFFER_IDX(i), MMU_COMPRESS_HEADER_SIZE,
				DRIVER_HEADER_NAME,
				&buf_addr) < 0){
			pr_info("%s malloc compress header failed %d\n",
				DRIVER_HEADER_NAME, i);
			hevc->fatal_error |= DECODER_FATAL_ERROR_NO_MEM;
			return;
		}
	}
	hevc_print(hevc, 0, "allocate compress header ok\n");

	/*alloc decoder buf*/
	for (i = 0; i < hevc->used_buf_num; i++) {
		if (((i + 1) * buf_size) > hevc->mc_buf->buf_size) {
			if (use_cma)
				hevc->use_cma_flag = 1;
			else {
				if (get_dbg_flag(hevc)) {
					hevc_print(hevc, 0,
						"%s maximum buf size is used\n",
						   __func__);
				}
				break;
			}
		}


		if (buf_size > 0) {
			hevc->m_BUF[i].index = i;

			if (use_cma == 2)
				hevc->use_cma_flag = 1;
			if (hevc->use_cma_flag) {
				if (decoder_bmmu_box_alloc_buf_phy
					(hevc->bmmu_box,
					VF_BUFFER_IDX(i), buf_size,
					DRIVER_NAME,
					&hevc->m_BUF[i].start_adr) < 0) {
					hevc->m_BUF[i].start_adr = 0;
					if (i <= 8) {
						/*if alloced (i+1)>=9
						don't send errors.*/
						hevc->fatal_error |=
						DECODER_FATAL_ERROR_NO_MEM;
					}
					break;
				}
			} else {
				hevc->m_BUF[i].start_adr =
					hevc->mc_buf->buf_start + i * buf_size;
				if (((hevc->m_BUF[i].start_adr + buf_size) >
						mc_buffer_end)) {
					if (get_dbg_flag(hevc)) {
						hevc_print(hevc, 0,
						"Max mc buffer or mpred_mv buffer is used\n");
					}
					break;
				}
			}
			hevc->m_BUF[i].size = buf_size;
			hevc->m_BUF[i].free_start_adr =
					hevc->m_BUF[i].start_adr;

			if (get_dbg_flag(hevc)) {
				hevc_print(hevc, 0,
					"Buffer %d: start_adr %p size %x\n", i,
					   (void *)hevc->m_BUF[i].start_adr,
					   hevc->m_BUF[i].size);
			}
		}
	}
	put_cma_alloc_ref();
	hevc_print(hevc, 0, "allocate end\n");

	hevc->buf_num = i;

}

static int config_pic(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	int ret = -1;
	int i;
	int pic_width = (/*(re_config_pic_flag == 0) &&*/
		get_buf_alloc_width(hevc)) ?
		get_buf_alloc_width(hevc) : hevc->pic_w;
	int pic_height = (/*(re_config_pic_flag == 0) &&*/
		get_buf_alloc_height(hevc)) ?
		get_buf_alloc_height(hevc) : hevc->pic_h;
	int lcu_size = hevc->lcu_size;
	int pic_width_lcu = (pic_width % lcu_size) ? pic_width / lcu_size +
				 1 : pic_width / lcu_size;
	int pic_height_lcu = (pic_height % lcu_size) ? pic_height / lcu_size +
				 1 : pic_height / lcu_size;
	int lcu_total = pic_width_lcu * pic_height_lcu;
	int lcu_size_log2 = hevc->lcu_size_log2;
	/*int MV_MEM_UNIT=lcu_size_log2==
		6 ? 0x100 : lcu_size_log2==5 ? 0x40 : 0x10;*/
	int MV_MEM_UNIT = lcu_size_log2 == 6 ? 0x200 : lcu_size_log2 ==
					 5 ? 0x80 : 0x20;
	int mpred_mv_end = hevc->work_space_buf->mpred_mv.buf_start +
				 hevc->work_space_buf->mpred_mv.buf_size;
	unsigned int y_adr = 0;
	int buf_size = 0;
#ifdef LOSLESS_COMPRESS_MODE
/*SUPPORT_10BIT*/
	int losless_comp_header_size =
		compute_losless_comp_header_size(pic_width ,
				 pic_height);
	int losless_comp_body_size = compute_losless_comp_body_size(pic_width ,
			 pic_height, hevc->mem_saving_mode);
	int mc_buffer_size = losless_comp_header_size + losless_comp_body_size;
	int mc_buffer_size_h = (mc_buffer_size + 0xffff)>>16;
	int mc_buffer_size_u_v = 0;
	int mc_buffer_size_u_v_h = 0;
	if (get_double_write_mode(hevc)) {
		int pic_width_dw = ((get_double_write_mode(hevc) == 2) ||
			(get_double_write_mode(hevc) == 3)) ?
			pic_width / 4 : pic_width;
		int pic_height_dw = ((get_double_write_mode(hevc) == 2) ||
			(get_double_write_mode(hevc) == 3)) ?
			pic_height / 4 : pic_height;
		int pic_width_lcu_dw = (pic_width_dw % lcu_size) ?
			pic_width_dw / lcu_size + 1 :
			pic_width_dw / lcu_size;
		int pic_height_lcu_dw = (pic_height_dw % lcu_size) ?
			pic_height_dw / lcu_size + 1 :
			pic_height_dw / lcu_size;
		int lcu_total_dw = pic_width_lcu_dw * pic_height_lcu_dw;

		mc_buffer_size_u_v = lcu_total_dw * lcu_size * lcu_size / 2;
		mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff) >> 16;
			/*64k alignment*/
		buf_size = ((mc_buffer_size_u_v_h << 16) * 3);
	}
	if (mc_buffer_size & 0xffff) { /*64k alignment*/
		mc_buffer_size_h += 1;
	}
	if ((!mmu_enable) &&
		((get_double_write_mode(hevc) & 0x10) == 0)) {
		/* use compress mode without mmu,
			need buf for compress decoding*/
		buf_size += (mc_buffer_size_h << 16);
	}
#else
	int mc_buffer_size_u_v = lcu_total * lcu_size * lcu_size / 2;
	int mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff) >> 16;
			/*64k alignment*/
	buf_size = ((mc_buffer_size_u_v_h << 16) * 3);
#endif


		if (mmu_enable) {
			if ((hevc->work_space_buf->cm_header.buf_start +
			   ((pic->index + 1)
			   * MMU_COMPRESS_HEADER_SIZE))
			   > (hevc->work_space_buf->cm_header.buf_start +
			   hevc->work_space_buf->cm_header.buf_size)) {
				hevc_print(hevc, 0,
					"MMU header_adr allocate fail\n");
				return -1;
		}

	/*
	pic->header_adr = hevc->work_space_buf->cm_header.buf_start +
		(pic->index * MMU_COMPRESS_HEADER_SIZE);
	*/
	pic->header_adr = decoder_bmmu_box_get_phy_addr(
				hevc->bmmu_box, HEADER_BUFFER_IDX(pic->index));

			if (get_dbg_flag(hevc)&H265_DEBUG_BUFMGR) {
				hevc_print(hevc, 0,
					"MMU header_adr %d: %x\n",
					pic->index, pic->header_adr);
			}
		}


	if ((hevc->work_space_buf->mpred_mv.buf_start + (((pic->index + 1)
					* lcu_total) * MV_MEM_UNIT))
						<= mpred_mv_end) {

		if (buf_size > 0) {
			for (i = 0; i < hevc->buf_num; i++) {
				y_adr = ((hevc->m_BUF[i].free_start_adr
					+ 0xffff) >> 16) << 16;
					/*64k alignment*/
				if ((y_adr+buf_size) <=
						(hevc->m_BUF[i].start_adr+
						hevc->m_BUF[i].size)) {
					hevc->m_BUF[i].free_start_adr =
						y_adr + buf_size;
					break;
				}
		}
	} else
		i = pic->index;
		if (i < hevc->buf_num) {
			pic->POC = INVALID_POC;
			/*ensure get_pic_by_POC()
			not get the buffer not decoded*/
			pic->BUF_index = i;
#ifdef LOSLESS_COMPRESS_MODE
/*SUPPORT_10BIT*/
			pic->comp_body_size = losless_comp_body_size;
			pic->buf_size = buf_size;

			if ((!mmu_enable) &&
				((get_double_write_mode(hevc) & 0x10) == 0)
				) {
				pic->mc_y_adr = y_adr;
				y_adr += (mc_buffer_size_h << 16);
			}
			pic->mc_canvas_y = pic->index;
			pic->mc_canvas_u_v = pic->index;
			if (get_double_write_mode(hevc) & 0x10) {
				pic->mc_y_adr = y_adr;
				pic->mc_u_v_adr = y_adr +
					((mc_buffer_size_u_v_h << 16) << 1);

				pic->mc_canvas_y = (pic->index << 1);
				pic->mc_canvas_u_v = (pic->index << 1) + 1;

				pic->dw_y_adr = pic->mc_y_adr;
				pic->dw_u_v_adr = pic->mc_u_v_adr;
			} else if (get_double_write_mode(hevc)) {
				pic->dw_y_adr = y_adr;
				pic->dw_u_v_adr = pic->dw_y_adr +
				((mc_buffer_size_u_v_h << 16) << 1);
			}
#else
			pic->buf_size = (mc_buffer_size_u_v_h << 16) * 3;
			pic->mc_y_adr = y_adr;
			pic->mc_u_v_adr = y_adr +
				((mc_buffer_size_u_v_h << 16) << 1);

			pic->mc_canvas_y = (pic->index << 1);
			pic->mc_canvas_u_v = (pic->index << 1) + 1;
#endif
			pic->mpred_mv_wr_start_addr =
				hevc->work_space_buf->mpred_mv.buf_start +
						((pic->index * lcu_total)
						* MV_MEM_UNIT);

			if (get_dbg_flag(hevc)) {
				hevc_print(hevc, 0,
				"%s index %d BUF_index %d mc_y_adr %x ",
				 __func__, pic->index,
				 pic->BUF_index, pic->mc_y_adr);
#ifdef LOSLESS_COMPRESS_MODE
				hevc_print_cont(hevc, 0,
				"comp_body_size %x comp_buf_size %x ",
				 pic->comp_body_size, pic->buf_size);
				hevc_print_cont(hevc, 0,
				"mpred_mv_wr_start_adr %x\n",
				 pic->mpred_mv_wr_start_addr);
				if (mmu_enable && get_double_write_mode(hevc))
					hevc_print(hevc, 0,
					"mmu double write  adr %ld\n",
					 pic->cma_alloc_addr);

#else
				hevc_print(hevc, 0,
				("mc_u_v_adr %x mpred_mv_wr_start_adr %x\n",
				 pic->mc_u_v_adr, pic->mpred_mv_wr_start_addr);
#endif
			}
			ret = 0;
		}
	}
	return ret;
}

#if 0
/*
free hevc->m_BUF[..] for all free hevc->m_PIC[..]
 with the different size of hevc->pic_w,hevc->pic_h
*/
static int recycle_buf(struct hevc_state_s *hevc)
{
	int i, j;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		struct PIC_s *pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		if (pic->width != hevc->pic_w || pic->height != hevc->pic_h) {
			if (pic->output_mark == 0 && pic->referenced == 0
				&& pic->output_ready == 0) {
				if (mmu_enable) {
					decoder_mmu_box_free_idx(hevc->mmu_box,
						pic->index);
				}
				pic->BUF_index = -1;
				if (get_dbg_flag(hevc)) {
					hevc_print(hevc, 0,
						"%s: %d\n", __func__,
						   pic->index);
				}
			}
		}
	}

	for (i = 0; i < hevc->buf_num; i++) {
		if (hevc->m_BUF[i].free_start_adr !=
			hevc->m_BUF[i].start_adr) {
			for (j = 0; j < MAX_REF_PIC_NUM; j++) {
				struct PIC_s *pic = hevc->m_PIC[j];
				if (pic == NULL || pic->index == -1)
					continue;
				if (pic->BUF_index == i)
					break;
			}
			if (j == MAX_REF_PIC_NUM)
				hevc->m_BUF[i].free_start_adr =
				    hevc->m_BUF[i].start_adr;
		}
	}
	return 0;
}
#endif

static void init_pic_list(struct hevc_state_s *hevc)
{
	int i;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		struct PIC_s *pic =
			vmalloc(sizeof(struct PIC_s));
		if (pic == NULL) {
			hevc_print(hevc, 0,
				"alloc pic %d fail\n", i);
			break;
		}
		memset(pic, 0, sizeof(struct PIC_s));
		hevc->m_PIC[i] = pic;
		pic->index = i;
		pic->BUF_index = -1;
		if (config_pic(hevc, pic) < 0) {
			if (get_dbg_flag(hevc))
				hevc_print(hevc, 0,
					"Config_pic %d fail\n", pic->index);
			pic->index = -1;
			i++;
			break;
		}
		pic->width = hevc->pic_w;
		pic->height = hevc->pic_h;
		if (get_double_write_mode(hevc))
			set_canvas(hevc, pic);
	}

	for (; i < MAX_REF_PIC_NUM; i++) {
		struct PIC_s *pic =
			vmalloc(sizeof(struct PIC_s));
		if (pic == NULL) {
			hevc_print(hevc, 0,
				"alloc pic %d fail\n", i);
			break;
		}
		memset(pic, 0, sizeof(struct PIC_s));
		hevc->m_PIC[i] = pic;
		pic->index = -1;
		pic->BUF_index = -1;
	}

}

static void uninit_pic_list(struct hevc_state_s *hevc)
{
	int i;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		struct PIC_s *pic = hevc->m_PIC[i];
		if (pic) {
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
	int losless_comp_body_size = compute_losless_comp_body_size(hevc->pic_w,
		 hevc->pic_h, hevc->mem_saving_mode);

	hevc->losless_comp_body_size = losless_comp_body_size;


	if (mmu_enable) {
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
	/*WRITE_VREG(HEVCD_MPP_DECOMP_CTL3,(0xff<<20) | (0xff<<10) | 0xff);
		//8-bit mode */
	}
	WRITE_VREG(HEVC_CM_BODY_LENGTH, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH, losless_comp_header_size);

	if (mmu_enable) {
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
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
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
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
			if (mmu_enable)
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
					hevc->m_PIC[i]->header_adr>>5);
			else
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
					hevc->m_PIC[i]->mc_y_adr >> 5);
		} else
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
				hevc->m_PIC[i]->mc_y_adr |
				(hevc->m_PIC[i]->mc_canvas_y << 8) | 0x1);
		if (get_double_write_mode(hevc) & 0x10) {
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
				if (mmu_enable)
					WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
						hevc->m_PIC[i]->header_adr>>5);
				else
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
	for (; i < MAX_REF_PIC_NUM; i++) {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
			if (mmu_enable)
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
				hevc->m_PIC[cur_pic_num-1]->header_adr>>5);
			else
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
				hevc->m_PIC[cur_pic_num-1]->mc_y_adr >> 5);
#ifndef LOSLESS_COMPRESS_MODE
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
				hevc->m_PIC[cur_pic_num-1]->mc_u_v_adr >> 5);
#endif
		} else {
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
				hevc->m_PIC[cur_pic_num-1]->mc_y_adr|
				(hevc->m_PIC[cur_pic_num-1]->mc_canvas_y<<8)
				| 0x1);
#ifndef LOSLESS_COMPRESS_MODE
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
				hevc->m_PIC[cur_pic_num-1]->mc_u_v_adr|
				(hevc->m_PIC[cur_pic_num-1]->mc_canvas_u_v<<8)
				| 0x1);
#endif
		}
	}

	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x1);

	/* Zero out canvas registers in IPP -- avoid simulation X */
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			   (0 << 8) | (0 << 1) | 1);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);

#ifdef LOSLESS_COMPRESS_MODE
	if ((get_double_write_mode(hevc) & 0x10) == 0)
		init_decode_head_hw(hevc);
#endif

}


static void dump_pic_list(struct hevc_state_s *hevc)
{
	int i;
	struct PIC_s *pic;
	hevc_print(hevc, 0,
		"pic_list_init_flag is %d\r\n", hevc->pic_list_init_flag);
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		hevc_print_cont(hevc, 0,
		"index %d decode_idx:%d,	POC:%d,	referenced:%d,	",
		 pic->index, pic->decode_idx, pic->POC, pic->referenced);
		hevc_print_cont(hevc, 0,
			"num_reorder_pic:%d, output_mark:%d, w/h %d,%d",
				pic->num_reorder_pic, pic->output_mark,
				pic->width, pic->height);
		hevc_print_cont(hevc, 0,
			"output_ready:%d, mv_wr_start %x vf_ref %d\n",
				pic->output_ready, pic->mpred_mv_wr_start_addr,
				pic->vf_ref);
	}
}

static struct PIC_s *output_pic(struct hevc_state_s *hevc,
		unsigned char flush_flag)
{
	int num_pic_not_yet_display = 0;
	int i;
	struct PIC_s *pic;
	struct PIC_s *pic_display = NULL;
	if (hevc->i_only & 0x4) {
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic == NULL ||
				(pic->index == -1) || (pic->POC == INVALID_POC))
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
		}
	} else {
		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic == NULL ||
				(pic->index == -1) || (pic->POC == INVALID_POC))
				continue;
			if (pic->output_mark)
				num_pic_not_yet_display++;
		}

		for (i = 0; i < MAX_REF_PIC_NUM; i++) {
			pic = hevc->m_PIC[i];
			if (pic == NULL ||
				(pic->index == -1) || (pic->POC == INVALID_POC))
				continue;
			if (pic->output_mark) {
				if (pic_display) {
					if (pic->POC < pic_display->POC)
						pic_display = pic;
					else if ((pic->POC == pic_display->POC)
						&& (pic->decode_idx <
							pic_display->
							decode_idx))
								pic_display
								= pic;
				} else
					pic_display = pic;
			}
		}
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
			pic =
				get_ref_pic_by_POC(hevc,
						cur_pic->
						m_aiRefPOCList0[cur_pic->
						slice_idx][i]);
			if (pic) {
				if (pic->error_mark)
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
			} else {
				if (get_dbg_flag(hevc)) {
					hevc_print_cont(hevc, 0,
					"Error %s, %dth poc (%d)",
					 __func__, i,
					 cur_pic->m_aiRefPOCList0[cur_pic->
					 slice_idx][i]);
					hevc_print_cont(hevc, 0,
					" of RPS is not in the pic list0\n");
				}
				cur_pic->error_mark = 1;
				/* dump_lmem(); */
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
			pic =
				get_ref_pic_by_POC(hevc,
						cur_pic->
						m_aiRefPOCList1[cur_pic->
						slice_idx][i]);
			if (pic) {
				if (pic->error_mark)
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
			} else {
				if (get_dbg_flag(hevc)) {
					hevc_print_cont(hevc, 0,
					"Error %s, %dth poc (%d)",
					 __func__, i,
					 cur_pic->m_aiRefPOCList1[cur_pic->
					 slice_idx][i]);
					hevc_print_cont(hevc, 0,
					" of RPS is not in the pic list1\n");
				}
				cur_pic->error_mark = 1;
				/* dump_lmem(); */
			}
		}
	}
	return 0;
}

static void apply_ref_pic_set(struct hevc_state_s *hevc, int cur_poc,
							  union param_u *params)
{
	int ii, i;
	int poc_tmp;
	struct PIC_s *pic;
	unsigned char is_referenced;
	/* hevc_print(hevc, 0,
	"%s cur_poc %d\n", __func__, cur_poc); */
	for (ii = 0; ii < MAX_REF_PIC_NUM; ii++) {
		pic = hevc->m_PIC[ii];
		if (pic == NULL ||
			pic->index == -1)
			continue;

		if ((pic->referenced == 0 || pic->POC == cur_poc))
			continue;
		is_referenced = 0;
		for (i = 0; i < 16; i++) {
			int delt;
			if (params->p.CUR_RPS[i] & 0x8000)
				break;
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
				is_referenced = 1;
				/* hevc_print(hevc, 0, "i is %d\n", i); */
				break;
			}
		}
		if (is_referenced == 0) {
			pic->referenced = 0;
			/* hevc_print(hevc, 0,
			"set poc %d reference to 0\n", pic->POC); */
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
	for (i = 0; i < 16; i++) {
		RefPicSetStCurr0[i] = 0;
		RefPicSetStCurr1[i] = 0;
		pic->m_aiRefPOCList0[pic->slice_idx][i] = 0;
		pic->m_aiRefPOCList1[pic->slice_idx][i] = 0;
	}
	for (i = 0; i < 16; i++) {
		if (params->p.CUR_RPS[i] & 0x8000)
			break;
		if ((params->p.CUR_RPS[i] >> RPS_USED_BIT) & 1) {
			int delt =
				params->p.CUR_RPS[i] &
				((1 << (RPS_USED_BIT - 1)) - 1);
			if ((params->p.CUR_RPS[i] >> (RPS_USED_BIT - 1)) & 1) {
				RefPicSetStCurr0[num_neg] =
					pic->POC - ((1 << (RPS_USED_BIT - 1)) -
								delt);
				/* hevc_print(hevc, 0,
					"RefPicSetStCurr0 %x %x %x\n",
				   RefPicSetStCurr0[num_neg], pic->POC,
				   (0x800-(params[i]&0x7ff))); */
				num_neg++;
			} else {
				RefPicSetStCurr1[num_pos] = pic->POC + delt;
				/* hevc_print(hevc, 0,
					"RefPicSetStCurr1 %d\n",
				   RefPicSetStCurr1[num_pos]); */
				num_pos++;
			}
		}
	}
	total_num = num_neg + num_pos;
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
		"%s: curpoc %d slice_type %d, total %d ",
		 __func__, pic->POC, params->p.slice_type, total_num);
		hevc_print_cont(hevc, 0,
			"num_neg %d num_list0 %d num_list1 %d\n",
		 num_neg, num_ref_idx_l0_active, num_ref_idx_l1_active);
	}

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
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
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print(hevc, 0, "ref0 POC (modification):");
			for (rIdx = 0; rIdx < num_ref_idx_l0_active; rIdx++) {
				int cIdx = params->p.modification_list[rIdx];
				pic->m_aiRefPOCList0[pic->slice_idx][rIdx] =
					cIdx >=
					num_neg ? RefPicSetStCurr1[cIdx -
					num_neg] :
					RefPicSetStCurr0[cIdx];
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print_cont(hevc, 0, "%d ",
						   pic->m_aiRefPOCList0[pic->
						   slice_idx]
						   [rIdx]);
				}
			}
		} else {
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print(hevc, 0, "ref0 POC:");
			for (rIdx = 0; rIdx < num_ref_idx_l0_active; rIdx++) {
				int cIdx = rIdx % total_num;
				pic->m_aiRefPOCList0[pic->slice_idx][rIdx] =
					cIdx >=
					num_neg ? RefPicSetStCurr1[cIdx -
					num_neg] :
					RefPicSetStCurr0[cIdx];
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print_cont(hevc, 0, "%d ",
						   pic->m_aiRefPOCList0[pic->
						   slice_idx]
						   [rIdx]);
				}
			}
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print_cont(hevc, 0, "\n");
		if (params->p.slice_type == B_SLICE) {
			if (params->p.modification_flag & 0x2) {
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
					hevc_print(hevc, 0,
						"ref1 POC (modification):");
				for (rIdx = 0; rIdx < num_ref_idx_l1_active;
					 rIdx++) {
					int cIdx;
					if (params->p.modification_flag & 0x1) {
						cIdx =
							params->p.
							modification_list
							[num_ref_idx_l0_active +
							 rIdx];
					} else {
						cIdx =
							params->p.
							modification_list[rIdx];
					}
					pic->m_aiRefPOCList1[pic->
						slice_idx][rIdx] =
						cIdx >=
						num_pos ?
						RefPicSetStCurr0[cIdx -	num_pos]
						: RefPicSetStCurr1[cIdx];
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						hevc_print_cont(hevc, 0, "%d ",
							   pic->
							   m_aiRefPOCList1[pic->
							   slice_idx]
							   [rIdx]);
					}
				}
			} else {
				if (get_dbg_flag(hevc) &
					H265_DEBUG_BUFMGR)
					hevc_print(hevc, 0, "ref1 POC:");
				for (rIdx = 0; rIdx < num_ref_idx_l1_active;
					 rIdx++) {
					int cIdx = rIdx % total_num;
					pic->m_aiRefPOCList1[pic->
						slice_idx][rIdx] =
						cIdx >=
						num_pos ?
						RefPicSetStCurr0[cIdx -
						num_pos]
						: RefPicSetStCurr1[cIdx];
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						hevc_print_cont(hevc, 0, "%d ",
							   pic->
							   m_aiRefPOCList1[pic->
							   slice_idx]
							   [rIdx]);
					}
				}
			}
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print_cont(hevc, 0, "\n");
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
						hevc_print_cont(hevc, 0,
						"{y=%d, x=%d w %d h %d ",
						 i, j, hevc->m_tile[i][j].width,
						 hevc->m_tile[i][j].height);
						hevc_print_cont(hevc, 0,
						"start_x %d start_y %d ",
						 hevc->m_tile[i][j].start_cu_x,
						 hevc->m_tile[i][j].start_cu_y);
						hevc_print_cont(hevc, 0,
						"sao_vb_start 0x%x ",
						 hevc->m_tile[i][j].
						 sao_vb_start_addr);
						hevc_print_cont(hevc, 0,
						"sao_abv_start 0x%x}\n",
						 hevc->m_tile[i][j].
						 sao_abv_start_addr);
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
							params->
							p.tile_height[i];
					}
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
						hevc_print_cont(hevc, 0,
						"{y=%d, x=%d w %d h %d ",
						 i, j, hevc->m_tile[i][j].width,
						 hevc->m_tile[i][j].height);
						hevc_print_cont(hevc, 0,
						"start_x %d start_y %d ",
						 hevc->m_tile[i][j].start_cu_x,
						 hevc->m_tile[i][j].start_cu_y);
						hevc_print_cont(hevc, 0,
						"sao_vb_start 0x%x ",
						 hevc->m_tile[i][j].
						 sao_vb_start_addr);
						hevc_print_cont(hevc, 0,
						"sao_abv_start 0x%x}\n",
						 hevc->m_tile[i][j].
						 sao_abv_start_addr);

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
#if 0
	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0,
		" ERROR : HEVC_ASSIST_SCRATCH_TEST Error : %d\n",
			   error_num);
	}
#endif
}

static void hevc_config_work_space_hw(struct hevc_state_s *hevc)
{
	struct BuffInfo_s *buf_spec = hevc->work_space_buf;

	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0,
			"%s %x %x %x %x %x %x %x %x %x %x %x %x\n",
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
			buf_spec->dblk_data.buf_start);
	WRITE_VREG(HEVCD_IPP_LINEBUFF_BASE, buf_spec->ipp.buf_start);
	if ((get_dbg_flag(hevc) & H265_DEBUG_SEND_PARAM_WITH_REG) == 0)
		WRITE_VREG(HEVC_RPM_BUFFER, (u32)hevc->rpm_phy_addr);
	WRITE_VREG(HEVC_SHORT_TERM_RPS, buf_spec->short_term_rps.buf_start);
	WRITE_VREG(HEVC_VPS_BUFFER, buf_spec->vps.buf_start);
	WRITE_VREG(HEVC_SPS_BUFFER, buf_spec->sps.buf_start);
	WRITE_VREG(HEVC_PPS_BUFFER, buf_spec->pps.buf_start);
	WRITE_VREG(HEVC_SAO_UP, buf_spec->sao_up.buf_start);
	if (mmu_enable)
		WRITE_VREG(H265_MMU_MAP_BUFFER, hevc->frame_mmu_map_phy_addr);
	else
		WRITE_VREG(HEVC_STREAM_SWAP_BUFFER,
				buf_spec->swap_buf.buf_start);
	WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, buf_spec->swap_buf2.buf_start);
	WRITE_VREG(HEVC_SCALELUT, buf_spec->scalelut.buf_start);
	/* cfg_p_addr */
	WRITE_VREG(HEVC_DBLK_CFG4, buf_spec->dblk_para.buf_start);
	/* cfg_d_addr */
	WRITE_VREG(HEVC_DBLK_CFG5, buf_spec->dblk_data.buf_start);

	if (get_dbg_flag(hevc) & H265_DEBUG_UCODE)
		WRITE_VREG(LMEM_DUMP_ADR, (u32)hevc->lmem_phy_addr);

}

static void hevc_init_decoder_hw(struct hevc_state_s *hevc,
	int decode_pic_begin, int decode_pic_num)
{
	unsigned int data32;
	int i;

#if 1
	/* m8baby test1902 */
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"[test.c] Test Parser Register Read/Write\n");
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
#if 0
	/* test Parser Reset */
	/* reset iqit to start mem init again */
	WRITE_VREG(DOS_SW_RESET3, (1 << 14) |
			   (1 << 3)	/* reset_whole parser */
			  );
	WRITE_VREG(DOS_SW_RESET3, 0);	/* clear reset_whole parser */
	data32 = READ_VREG(HEVC_PARSER_VERSION);
	if (data32 != 0x00010001)
		hevc_print(hevc, 0,
		"Test Parser Fatal Error\n");
#endif
	/* reset iqit to start mem init again */
	WRITE_VREG(DOS_SW_RESET3, (1 << 14)
			  );
	CLEAR_VREG_MASK(HEVC_CABAC_CONTROL, 1);
	CLEAR_VREG_MASK(HEVC_PARSER_CORE_CONTROL, 1);

#endif

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"[test.c] Enable BitStream Fetch\n");
			 ;
	if (!hevc->m_ins_flag) {
		data32 = READ_VREG(HEVC_STREAM_CONTROL);
		data32 = data32 | (1 << 0);      /* stream_fetch_enable */
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

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"[test.c] Enable HEVC Parser Interrupt\n");
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

	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"[test.c] Enable HEVC Parser Shift\n");

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
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
		hevc_print(hevc, 0,
		"[test.c] Initial IQIT_SCALELUT memory --");
		hevc_print_cont(hevc, 0,
		" just to avoid X in simulation...\n");
	}
	WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0);	/* cfg_p_addr */
	for (i = 0; i < 1024; i++)
		WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, 0);

#ifdef ENABLE_SWAP_TEST
	WRITE_VREG(HEVC_STREAM_SWAP_TEST, 100);
#endif

	/*WRITE_VREG(HEVC_DECODE_PIC_BEGIN_REG, 0);*/
	/*WRITE_VREG(HEVC_DECODE_PIC_NUM_REG, 0xffffffff);*/
	WRITE_VREG(HEVC_DECODE_SIZE, 0);
	/*WRITE_VREG(HEVC_DECODE_COUNT, 0);*/
	/* Send parser_cmd */
	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0,
			"[test.c] SEND Parser Command ...\n");
	WRITE_VREG(HEVC_PARSER_CMD_WRITE, (1 << 16) | (0 << 0));
	for (i = 0; i < PARSER_CMD_NUMBER; i++)
		WRITE_VREG(HEVC_PARSER_CMD_WRITE, parser_cmd[i]);

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
	   hevc_print(hevc, 0, "[test.c] Start MPRED\n");
	   WRITE_VREG(HEVC_MPRED_INT_STATUS,
	   (1<<31)
	   );
	 */

	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0,
			"[test.c] Reset IPP\n");
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
	for (i = 0; i < PARSER_CMD_NUMBER; i++)
		WRITE_VREG(HEVC_PARSER_CMD_WRITE, parser_cmd[i]);

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
	return;
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

static void config_aux_buf(struct hevc_state_s *hevc)
{
	WRITE_VREG(HEVC_AUX_ADR, hevc->aux_phy_addr);
	WRITE_VREG(HEVC_AUX_DATA_SIZE,
		((hevc->prefix_aux_size >> 4) << 16) |
		(hevc->suffix_aux_size >> 4)
		);
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
		((hevc->lcu_x_num * hevc->lcu_y_num) * MV_MEM_UNIT);

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
	data32 = (hevc->slice_type |
			  hevc->new_pic << 2 |
			  hevc->new_tile << 3 |
			  hevc->isNextSliceSegment << 4 |
			  hevc->TMVPFlag << 5 |
			  hevc->LDCFlag << 6 |
			  hevc->ColFromL0Flag << 7 |
			  above_ptr_ctrl << 8 |
			  above_en << 9 |
			  mv_wr_en << 10 |
			  mv_rd_en << 11 |
			  col_isIntra << 12 |
			  buffer_linear << 13 |
			  hevc->LongTerm_Curr << 14 |
			  hevc->LongTerm_Col << 15 |
			  hevc->lcu_size_log2 << 16 |
			  cu_size_log2 << 20 | hevc->plevel << 24);
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

	data32 = (hevc->LongTerm_Ref);
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
	   Ref_POC_xx_L1 in pair write order!!! */
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
		/* WRITE_VREG(HEVC_MPRED_MV_RPTR,mpred_mv_rd_ptr); */
		WRITE_VREG(HEVC_MPRED_MV_RPTR, mpred_mv_rd_start_addr);
	} else if (!hevc->isNextSliceSegment) {
		/* WRITE_VREG(HEVC_MPRED_MV_RPTR,mpred_mv_rd_ptr_p1); */
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
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		data32 &= (~(0xff << 16));
		if (get_double_write_mode(hevc) != 1)
			data32 |= (0xff<<16);
		if (hevc->mem_saving_mode == 1)
			data32 |= (1 << 9);
		else
			data32 &= ~(1 << 9);
		if (workaround_enable & 1)
			data32 |= (1 << 7);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}
	data32 = cur_pic->mc_y_adr;
	if (get_double_write_mode(hevc))
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, cur_pic->dw_y_adr);

	if ((get_double_write_mode(hevc) & 0x10) == 0)
		WRITE_VREG(HEVC_CM_BODY_START_ADDR, data32);

	if (mmu_enable)
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
			 ((params->p.pps_cr_qp_offset
			   & 0x1f) <<
			  9));
		data32 |=
			(hevc->lcu_size ==
			 64) ? 0 : ((hevc->lcu_size == 32) ? 1 : 2);

		WRITE_VREG(HEVC_DBLK_CFG1, data32);
	}
#if 0
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	data32 |= (mem_map_mode <<
			12);/* [13:12] axi_aformat,
			       0-Linear, 1-32x32, 2-64x32 */
	WRITE_VREG(HEVC_SAO_CTRL1, data32);

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	data32 |= (mem_map_mode <<
			   4);	/* [5:4]    -- address_format
				00:linear 01:32x32 10:64x32 */
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#else
	/* m8baby test1902 */
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	data32 |= (mem_map_mode <<
			   12);	/* [13:12] axi_aformat, 0-Linear,
				   1-32x32, 2-64x32 */
	data32 &= (~0xff0);
	/* data32 |= 0x670;  // Big-Endian per 64-bit */
	data32 |= endian;	/* Big-Endian per 64-bit */
	data32 &= (~0x3); /*[1]:dw_disable [0]:cm_disable*/
	if (get_double_write_mode(hevc) == 0)
		data32 |= 0x2; /*disable double write*/
	else if (!mmu_enable && (get_double_write_mode(hevc) & 0x10))
		data32 |= 0x1; /*disable cm*/
	WRITE_VREG(HEVC_SAO_CTRL1, data32);

	if (get_double_write_mode(hevc) & 0x10) {
		/* [23:22] dw_v1_ctrl
		[21:20] dw_v0_ctrl
		[19:18] dw_h1_ctrl
		[17:16] dw_h0_ctrl
		*/
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		/*set them all 0 for H265_NV21 (no down-scale)*/
		data32 &= ~(0xff << 16);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	/* [5:4]    -- address_format 00:linear 01:32x32 10:64x32 */
	data32 |= (mem_map_mode <<
			   4);
	data32 &= (~0xF);
	data32 |= 0xf;  /* valid only when double write only */
		/*data32 |= 0x8;*/		/* Big-Endian per 64-bit */
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#endif
	data32 = 0;
	data32_2 = READ_VREG(HEVC_SAO_CTRL0);
	data32_2 &= (~0x300);
	/* slice_deblocking_filter_disabled_flag = 0;
		ucode has handle it , so read it from ucode directly */
	if (hevc->tile_enabled) {
		data32 |=
			((misc_flag0 >>
			  LOOP_FILER_ACROSS_TILES_ENABLED_FLAG_BIT) &
			 0x1) << 0;
		data32_2 |=
			((misc_flag0 >>
			  LOOP_FILER_ACROSS_TILES_ENABLED_FLAG_BIT) &
			 0x1) << 8;
	}
	slice_deblocking_filter_disabled_flag = (misc_flag0 >>
			SLICE_DEBLOCKING_FILTER_DISABLED_FLAG_BIT) &
		0x1;	/* ucode has handle it,so read it from ucode directly */
	if ((misc_flag0 & (1 << DEBLOCKING_FILTER_OVERRIDE_ENABLED_FLAG_BIT))
		&& (misc_flag0 & (1 << DEBLOCKING_FILTER_OVERRIDE_FLAG_BIT))) {
		/* slice_deblocking_filter_disabled_flag =
		   (misc_flag0>>SLICE_DEBLOCKING_FILTER_DISABLED_FLAG_BIT)&0x1;
		//ucode has handle it , so read it from ucode directly */
		data32 |= slice_deblocking_filter_disabled_flag << 2;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print_cont(hevc, 0,
			"(1,%x)", data32);
		if (!slice_deblocking_filter_disabled_flag) {
			data32 |= (params->p.slice_beta_offset_div2 & 0xf) << 3;
			data32 |= (params->p.slice_tc_offset_div2 & 0xf) << 7;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print_cont(hevc, 0,
				"(2,%x)", data32);
		}
	} else {
		data32 |=
			((misc_flag0 >>
			  PPS_DEBLOCKING_FILTER_DISABLED_FLAG_BIT) &
			 0x1) << 2;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print_cont(hevc, 0,
			"(3,%x)", data32);
		if (((misc_flag0 >> PPS_DEBLOCKING_FILTER_DISABLED_FLAG_BIT) &
			 0x1) == 0) {
			data32 |= (params->p.pps_beta_offset_div2 & 0xf) << 3;
			data32 |= (params->p.pps_tc_offset_div2 & 0xf) << 7;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print_cont(hevc, 0,
				"(4,%x)", data32);
		}
	}
	if ((misc_flag0 & (1 << PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT))
		&& ((misc_flag0 & (1 << SLICE_SAO_LUMA_FLAG_BIT))
			|| (misc_flag0 & (1 << SLICE_SAO_CHROMA_FLAG_BIT))
			|| (!slice_deblocking_filter_disabled_flag))) {
		data32 |=
			((misc_flag0 >>
			  SLICE_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			 & 0x1)	<< 1;
		data32_2 |=
			((misc_flag0 >>
			  SLICE_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			& 0x1) << 9;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print_cont(hevc, 0,
			"(5,%x)\n", data32);
	} else {
		data32 |=
			((misc_flag0 >>
			  PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			 & 0x1) << 1;
		data32_2 |=
			((misc_flag0 >>
			  PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED_FLAG_BIT)
			 & 0x1) << 9;
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			hevc_print_cont(hevc, 0,
			"(6,%x)\n", data32);
	}
	WRITE_VREG(HEVC_DBLK_CFG9, data32);
	WRITE_VREG(HEVC_SAO_CTRL0, data32_2);
}

static void clear_used_by_display_flag(struct hevc_state_s *hevc)
{
	struct PIC_s *pic;
	int i;
	if (get_dbg_flag(hevc) & H265_DEBUG_NOT_USE_LAST_DISPBUF)
		return;

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic)
			pic->used_by_display = 0;
	}
}

static struct PIC_s *get_new_pic(struct hevc_state_s *hevc,
		union param_u *rpm_param)
{
	struct PIC_s *new_pic = NULL;
	struct PIC_s *pic;
	/* recycle un-used pic */
	int i;
	int ret;
	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		if ((pic->used_by_display) && !mmu_enable
			&& ((READ_VCBUS_REG(AFBC_BODY_BADDR) << 4) !=
				pic->mc_y_adr))
			pic->used_by_display = 0;
		if (pic->output_mark == 0 && pic->referenced == 0
			&& pic->output_ready == 0
			&& pic->used_by_display == 0) {
			if (new_pic) {
				if (pic->POC < new_pic->POC)
					new_pic = pic;
			} else
				new_pic = pic;
		}
	}

	/*try to allocate more pic for new resolution*/
#if 0
	if (re_config_pic_flag && new_pic == NULL) {
		int ii;
		for (ii = 0; ii < MAX_REF_PIC_NUM; ii++) {
			if (hevc->m_PIC[ii] == NULL ||
				hevc->m_PIC[ii]->index == -1)
				break;
		}
		if (ii < MAX_REF_PIC_NUM) {
			new_pic = hevc->m_PIC[ii];
			if (new_pic) {
				memset(new_pic, 0, sizeof(struct PIC_s));
				new_pic->index = ii;
				new_pic->BUF_index = -1;
			}
		}
	}
#endif
	/**/

	if (new_pic == NULL) {
		/* hevc_print(hevc, 0,
		"Error: Buffer management, no free buffer\n"); */
		return new_pic;
	}

	new_pic->referenced = 1;
	if (new_pic->width != hevc->pic_w ||
		new_pic->height != hevc->pic_h) {
#if 0
		if (re_config_pic_flag) {
			/* re config pic for new resolution */
			recycle_buf(hevc);
			/* if(new_pic->BUF_index == -1){ */
			if (config_pic(hevc, new_pic, 0) < 0) {
				if (get_dbg_flag(hevc) &
					H265_DEBUG_BUFMGR_MORE) {
					hevc_print(hevc, 0,
					"Config_pic %d fail\n",
					new_pic->index);
					dump_pic_list(hevc);
				}
				new_pic->index = -1;
				new_pic = NULL;
			} else
				init_pic_list_hw(hevc);
		}
#endif
		if (new_pic) {
			new_pic->width = hevc->pic_w;
			new_pic->height = hevc->pic_h;
			set_canvas(hevc, new_pic);
		}
	}
	if (new_pic) {
		new_pic->decode_idx = hevc->decode_idx;
		new_pic->slice_idx = 0;
		new_pic->referenced = 1;
		new_pic->output_mark = 0;
		new_pic->recon_mark = 0;
		new_pic->error_mark = 0;
		/* new_pic->output_ready = 0; */
		new_pic->num_reorder_pic = rpm_param->p.sps_num_reorder_pics_0;
		new_pic->losless_comp_body_size = hevc->losless_comp_body_size;
		new_pic->POC = hevc->curr_POC;
		new_pic->pic_struct = hevc->curr_pic_struct;
		if (new_pic->aux_data_buf)
			release_aux_data(hevc, new_pic);
	}

	if (mmu_enable) {
		ret = H265_alloc_mmu(hevc, new_pic, rpm_param->p.bit_depth,
			hevc->frame_mmu_map_addr);
			/*hevc_print(hevc, 0,
			"get pic index %x\n",new_pic->index);*/
		if (ret != 0) {
			pr_err("can't alloc need mmu1,idx %d ret =%d\n",
			new_pic->decode_idx,
			ret);
		return NULL;
		}

	}

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
		/**/
		if (pic->POC != INVALID_POC) {
			pic->output_mark = 1;
			pic->recon_mark = 1;
		}
		pic->recon_mark = 1;
	}
	do {
		pic_display = output_pic(hevc, 1);

		if (pic_display) {
			pic_display->referenced = 0;
			if ((pic_display->error_mark
				 && ((hevc->ignore_bufmgr_error & 0x2) == 0))
				|| (get_dbg_flag(hevc) &
					H265_DEBUG_DISPLAY_CUR_FRAME)
				|| (get_dbg_flag(hevc) &
					H265_DEBUG_NO_DISPLAY)) {
				pic_display->output_ready = 0;
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
					hevc_print(hevc, 0,
					"[BM] Display: POC %d, ",
					 pic_display->POC);
					hevc_print_cont(hevc, 0,
					"decoding index %d ==> ",
					 pic_display->decode_idx);
					hevc_print_cont(hevc, 0,
					"Debug mode or error, recycle it\n");
				}
			} else {
				if (hevc->i_only & 0x1
					&& pic_display->slice_type != 2)
					pic_display->output_ready = 0;
				else {
					prepare_display_buf(hevc, pic_display);
					if (get_dbg_flag(hevc)
						& H265_DEBUG_BUFMGR) {
						hevc_print(hevc, 0,
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
	unsigned size_reg_val =
		READ_VREG(HEVC_AUX_DATA_SIZE);
	unsigned aux_count = 0;
	int aux_size = 0;
	if (suffix_flag) {
		aux_adr = (unsigned short *)
			(hevc->aux_addr +
			hevc->prefix_aux_size);
		aux_count =
		((size_reg_val & 0xffff) << 4)
			>> 1;
		aux_size =
			hevc->suffix_aux_size;
	} else {
		aux_adr =
		(unsigned short *)hevc->aux_addr;
		aux_count =
		((size_reg_val >> 16) << 4)
			>> 1;
		aux_size =
			hevc->prefix_aux_size;
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE) {
		hevc_print(hevc, 0,
			"%s:old size %d count %d,suf %d dv_flag %d\r\n",
			__func__, pic->aux_data_size,
			aux_count, suffix_flag, dv_meta_flag);
	}
	if (aux_size > 0 && aux_count > 0) {
		int heads_size = 0;
		int new_size;
		char *new_buf;
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
		new_size = pic->aux_data_size + aux_count + heads_size;
		new_buf = krealloc(pic->aux_data_buf,
			new_size,
			GFP_KERNEL);
		if (new_buf) {
			unsigned char valid_tag = 0;
			unsigned char *h =
				new_buf +
				pic->aux_data_size;
			unsigned char *p = h + 8;
			int len = 0;
			int padding_len = 0;
			pic->aux_data_buf = new_buf;
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
						pic->aux_data_size +=
						(len + 8);
						h[0] = (len >> 24)
						& 0xff;
						h[1] = (len >> 16)
						& 0xff;
						h[2] = (len >> 8)
						& 0xff;
						h[3] = (len >> 0)
						& 0xff;
						h[6] =
						(padding_len >> 8)
						& 0xff;
						h[7] = (padding_len)
						& 0xff;
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
						unsigned short aa =
							aux_adr[i + 3
							- ii];
						*p = aa & 0xff;
						p++;
						len++;
						/*if ((aa >> 8) == 0xff)
							padding_len++;*/
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
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE) {
				hevc_print(hevc, 0,
					"aux: (size %d) suffix_flag %d\n",
					pic->aux_data_size, suffix_flag);
				for (i = 0; i < pic->aux_data_size; i++) {
					hevc_print_cont(hevc, 0,
						"%02x ", pic->aux_data_buf[i]);
					if (((i + 1) & 0xf) == 0)
						hevc_print_cont(hevc, 0, "\n");
				}
				hevc_print_cont(hevc, 0, "\n");
			}

		}
	}

}

static void release_aux_data(struct hevc_state_s *hevc,
	struct PIC_s *pic)
{
	kfree(pic->aux_data_buf);
	pic->aux_data_buf = NULL;
	pic->aux_data_size = 0;
}

static inline void hevc_pre_pic(struct hevc_state_s *hevc,
			struct PIC_s *pic)
{

	/* prev pic */
	/*if (hevc->curr_POC != 0) {*/
	if (hevc->m_nalUnitType != NAL_UNIT_CODED_SLICE_IDR
			&& hevc->m_nalUnitType !=
			NAL_UNIT_CODED_SLICE_IDR_N_LP) {
		struct PIC_s *pic_display;
		pic = get_pic_by_POC(hevc, hevc->iPrevPOC);
		if (pic && (pic->POC != INVALID_POC)) {
			/*PB skip control */
			if (pic->error_mark == 0
					&& hevc->PB_skip_mode == 1) {
				/* start decoding after
				   first I */
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
			pic->output_mark = 1;
			pic->recon_mark = 1;
		}
		do {
			pic_display = output_pic(hevc, 0);

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
						hevc_print(hevc, 0,
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
					if (hevc->i_only & 0x1
						&& pic_display->
						slice_type != 2) {
						pic_display->output_ready = 0;
					} else {
						prepare_display_buf
							(hevc,
							 pic_display);
					if (get_dbg_flag(hevc) &
						H265_DEBUG_BUFMGR) {
						hevc_print(hevc, 0,
						"[BM] Display: POC %d, ",
							 pic_display->POC);
							hevc_print_cont(hevc, 0,
							"decoding index %d\n",
							 pic_display->
							 decode_idx);
						}
					}
				}
			}
		} while (pic_display);
	} else {
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR) {
			hevc_print(hevc, 0,
			"[BM] current pic is IDR, ");
			hevc_print(hevc, 0,
			"clear referenced flag of all buffers\n");
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
			dump_pic_list(hevc);
		pic = get_pic_by_POC(hevc, hevc->iPrevPOC);
		flush_output(hevc, pic);
	}

}

static void check_pic_decoded_lcu_count(struct hevc_state_s *hevc)
{
	int current_lcu_idx = READ_VREG(HEVC_PARSER_LCU_START)&0xffffff;
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
		if (hevc->cur_pic->error_mark)
			hevc_print(hevc, 0,
				"cur lcu idx = %d, (total %d), set error_mark\n",
				current_lcu_idx,
				hevc->lcu_x_num_pre*hevc->lcu_y_num_pre);

	}
	hevc->lcu_x_num_pre = hevc->lcu_x_num;
	hevc->lcu_y_num_pre = hevc->lcu_y_num;
}

static int hevc_slice_segment_header_process(struct hevc_state_s *hevc,
		union param_u *rpm_param,
		int decode_pic_begin)
{
#ifdef CONFIG_AM_VDEC_DV
	struct vdec_s *vdec = hw_to_vdec(hevc);
#endif
	int i;
	int lcu_x_num_div;
	int lcu_y_num_div;
	int Col_ref;
	int dbg_skip_flag = 0;
	if (hevc->wait_buf == 0) {
		hevc->sps_num_reorder_pics_0 =
			rpm_param->p.sps_num_reorder_pics_0;
		hevc->m_temporalId = rpm_param->p.m_temporalId;
		hevc->m_nalUnitType = rpm_param->p.m_nalUnitType;
		hevc->interlace_flag =
			(rpm_param->p.profile_etc >> 2) & 0x1;
		hevc->curr_pic_struct =
			(rpm_param->p.sei_frame_field_info >> 3) & 0xf;

		if (interlace_enable == 0)
			hevc->interlace_flag = 0;
		if (interlace_enable & 0x100)
			hevc->interlace_flag = interlace_enable & 0x1;
		if (hevc->interlace_flag == 0)
			hevc->curr_pic_struct = 0;
		/* if(hevc->m_nalUnitType == NAL_UNIT_EOS){ */
		/* hevc->m_pocRandomAccess = MAX_INT;
		//add to fix RAP_B_Bossen_1 */
		/* } */
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
		/* hevc->curr_predFlag_L0=(hevc->slice_type==2) ? 0:1; */
		/* hevc->curr_predFlag_L1=(hevc->slice_type==0) ? 1:0; */
		hevc->TMVPFlag = rpm_param->p.slice_temporal_mvp_enable_flag;
		hevc->isNextSliceSegment =
			rpm_param->p.dependent_slice_segment_flag ? 1 : 0;
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
			if (/*re_config_pic_flag == 0 &&*/
				(get_double_write_mode(hevc) & 0x10) == 0)
				init_decode_head_hw(hevc);
#endif
		}

		if (HEVC_SIZE < hevc->pic_w * hevc->pic_h) {
			pr_info("over size : %u x %u.\n",
				hevc->pic_w, hevc->pic_h);
			if (!hevc->m_ins_flag)
				debug |= (H265_DEBUG_DIS_LOC_ERROR_PROC |
				H265_DEBUG_DIS_SYS_ERROR_PROC);

			hevc->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
			return -1;
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
		hevc->lcu_size =
			1 << (rpm_param->p.log2_min_coding_block_size_minus3 +
					3 + rpm_param->
					p.log2_diff_max_min_coding_block_size);
		if (hevc->lcu_size == 0) {
			hevc_print(hevc, 0,
				"Error, lcu_size = 0 (%d,%d)\n",
				   rpm_param->p.
				   log2_min_coding_block_size_minus3,
				   rpm_param->p.
				   log2_diff_max_min_coding_block_size);
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
				1 << (rpm_param->p.
				log2_max_pic_order_cnt_lsb_minus4 + 4);
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
				&& ((iPrevPOClsb - iPOClsb) >=
					(iMaxPOClsb / 2)))
				iPOCmsb = iPrevPOCmsb + iMaxPOClsb;
			else if ((iPOClsb > iPrevPOClsb)
					 && ((iPOClsb - iPrevPOClsb) >
						 (iMaxPOClsb / 2)))
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
			(rpm_param->p.num_ref_idx_l0_active >
			 MAX_REF_ACTIVE) ? MAX_REF_ACTIVE : rpm_param->p.
			num_ref_idx_l0_active;
		hevc->RefNum_L1 =
			(rpm_param->p.num_ref_idx_l1_active >
			 MAX_REF_ACTIVE) ? MAX_REF_ACTIVE : rpm_param->p.
			num_ref_idx_l1_active;

		/* if(curr_POC==0x10) dump_lmem(); */

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
				(nal_skip_policy >= 3) &&
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
		/**/
		/* if((iPrevPOC != curr_POC)){ */
		if (rpm_param->p.slice_segment_address == 0) {
			struct PIC_s *pic;
			hevc->new_pic = 1;
			check_pic_decoded_lcu_count(hevc);
			/**/ if (use_cma == 0) {
				if (hevc->pic_list_init_flag == 0) {
					/*USE_BUF_BLOCK*/
					init_buf_list(hevc);
					/**/
					init_pic_list(hevc);
					init_pic_list_hw(hevc);
					init_buf_spec(hevc);
					hevc->pic_list_init_flag = 3;
				}
			}
			hevc->first_pic_after_recover = 0;
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE)
				dump_pic_list(hevc);
			/* prev pic */
			hevc_pre_pic(hevc, pic);
			/* update referenced of old pictures
			   (cur_pic->referenced is 1 and not updated) */
			apply_ref_pic_set(hevc, hevc->curr_POC,
							  rpm_param);
			if (mmu_enable && (!hevc->m_ins_flag))
				hevc->used_4k_num =
					READ_VREG(HEVC_SAO_MMU_STATUS) >> 16;

			if (mmu_enable &&
				hevc->cur_pic != NULL) {
				if (!(hevc->cur_pic->error_mark
					&& ((hevc->ignore_bufmgr_error
					& 0x1) == 0))) {
					decoder_mmu_box_free_idx_tail(
					hevc->mmu_box,
					hevc->cur_pic->index,
					hevc->used_4k_num);
				}

			}
#ifdef CONFIG_AM_VDEC_DV
			if (vdec->master) {
				struct hevc_state_s *hevc_ba =
				(struct hevc_state_s *)
					vdec->master->private;
				if (hevc_ba->cur_pic != NULL)
					hevc_ba->cur_pic->dv_enhance_exist = 1;
			}
			if (vdec->master == NULL &&
				vdec->slave == NULL) {
				if (hevc->cur_pic != NULL)
					set_aux_data(hevc, hevc->cur_pic, 1, 0);
			}
#else
			if (hevc->cur_pic != NULL)
				set_aux_data(hevc, hevc->cur_pic, 1, 0);
#endif
			/* new pic */
			hevc->cur_pic = get_new_pic(hevc, rpm_param);
			if (hevc->cur_pic == NULL) {
				if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
					dump_pic_list(hevc);
				hevc->wait_buf = 1;
				return -1;
			}
#ifdef CONFIG_AM_VDEC_DV
			hevc->cur_pic->dv_enhance_exist = 0;
			if (vdec->master == NULL &&
				vdec->slave == NULL)
				set_aux_data(hevc, hevc->cur_pic, 0, 0);
#else
			set_aux_data(hevc, hevc->cur_pic, 0, 0);
#endif
			if (get_dbg_flag(hevc) & H265_DEBUG_DISPLAY_CUR_FRAME) {
				hevc->cur_pic->output_ready = 1;
				hevc->cur_pic->stream_offset =
					READ_VREG(HEVC_SHIFT_BYTE_COUNT);
				prepare_display_buf(hevc, hevc->cur_pic);
				hevc->wait_buf = 2;
				return -1;
			}
		} else {
#ifdef CONFIG_AM_VDEC_DV
			if (vdec->master == NULL &&
				vdec->slave == NULL) {
				if (hevc->cur_pic != NULL) {
					set_aux_data(hevc, hevc->cur_pic, 1, 0);
					set_aux_data(hevc, hevc->cur_pic, 0, 0);
				}
			}
#else
			if (hevc->cur_pic != NULL) {
				set_aux_data(hevc, hevc->cur_pic, 1, 0);
				set_aux_data(hevc, hevc->cur_pic, 0, 0);
			}
#endif
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
		/*
		if (mmu_enable && hevc->cur_pic != NULL) {
			long used_4k_num =
				(READ_VREG(HEVC_SAO_MMU_STATUS) >> 16);
			decoder_mmu_box_free_idx_tail(hevc->mmu_box,
				hevc->cur_pic->index, used_4k_num);

			}
		*/
			hevc->cur_pic = get_new_pic(hevc, rpm_param);
			if (hevc->cur_pic == NULL)
				return -1;

#ifdef CONFIG_AM_VDEC_DV
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
				"==>%s dec idx %d, struct %d interlace %d\n",
				__func__,
				hevc->decode_idx,
				hevc->curr_pic_struct,
				hevc->interlace_flag);
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
						   (hevc->pic_w +
						    hevc->lcu_size -
							1) / hevc->lcu_size);
		if (hevc->tile_y_x != (hevc->tile_x | (hevc->tile_y << 8))) {
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
			if (hevc->cur_pic->
				m_aiRefPOCList0[hevc->cur_pic->slice_idx][i] >
				hevc->curr_POC)
				hevc->LDCFlag = 0;
		}
		if (rpm_param->p.slice_type == B_SLICE) {
			for (i = 0; (i < hevc->RefNum_L1)
					&& hevc->LDCFlag; i++) {
				if (hevc->cur_pic->
					m_aiRefPOCList1[hevc->cur_pic->
					slice_idx][i] >
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
				hevc->cur_pic->m_aiRefPOCList0[hevc->cur_pic->
				slice_idx][Col_ref];
		} else
			hevc->Col_POC = INVALID_POC;
	} else {
		if (Col_ref < hevc->RefNum_L1) {
			hevc->Col_POC =
				hevc->cur_pic->m_aiRefPOCList1[hevc->cur_pic->
				slice_idx][Col_ref];
		} else
			hevc->Col_POC = INVALID_POC;
	}

	hevc->LongTerm_Ref = 0;	/* to do ... */

	if (hevc->slice_type != 2) {
		/* if(hevc->i_only==1){ */
		/* return 0xf; */
		/* } */

		if (hevc->Col_POC != INVALID_POC) {
			hevc->col_pic = get_ref_pic_by_POC(hevc, hevc->Col_POC);
			if (hevc->col_pic == NULL) {
				hevc->cur_pic->error_mark = 1;
				if (get_dbg_flag(hevc)) {
					hevc_print(hevc, 0,
					"WRONG,fail to get the pic Col_POC\n");
				}
			} else if (hevc->col_pic->error_mark) {
				hevc->cur_pic->error_mark = 1;
				if (get_dbg_flag(hevc)) {
					hevc_print(hevc, 0,
					"WRONG, Col_POC error_mark is 1\n");
				}
			}

			if (hevc->cur_pic->error_mark
				&& ((hevc->ignore_bufmgr_error & 0x1) == 0)) {

				/*count info*/
				vdec_count_info(gvs, hevc->cur_pic->error_mark,
					hevc->cur_pic->stream_offset);

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

	config_mc_buffer(hevc, hevc->cur_pic);

	if (hevc->cur_pic->error_mark
		&& ((hevc->ignore_bufmgr_error & 0x1) == 0)) {
		if (get_dbg_flag(hevc))
			hevc_print(hevc, 0,
			"Discard this picture index %d\n",
					hevc->cur_pic->index);
		/*count info*/
		vdec_count_info(gvs, hevc->cur_pic->error_mark,
			hevc->cur_pic->stream_offset);

		return 2;
	}
#ifdef MCRCC_ENABLE
	config_mcrcc_axi_hw(hevc, hevc->cur_pic->slice_type);
#endif
	config_mpred_hw(hevc);

	config_sao_hw(hevc, rpm_param);

	if ((hevc->slice_type != 2) && (hevc->i_only & 0x2))
		return 0xf;

	return 0;
}



static int H265_alloc_mmu(struct hevc_state_s *hevc, struct PIC_s *new_pic,
		unsigned short bit_depth, unsigned int *mmu_index_adr) {
	int cur_buf_idx = new_pic->index;
	int bit_depth_10 = (bit_depth != 0x00);
	int picture_size;
	int cur_mmu_4k_number;
	picture_size = compute_losless_comp_body_size(new_pic->width,
				new_pic->height, !bit_depth_10);
	cur_mmu_4k_number = ((picture_size+(1<<12)-1) >> 12);

	/*hevc_print(hevc, 0,
	"alloc_mmu cur_idx : %d picture_size : %d mmu_4k_number : %d\r\n",
	cur_buf_idx, picture_size, cur_mmu_4k_number);*/
	return decoder_mmu_box_alloc_idx(
	  hevc->mmu_box,
	  cur_buf_idx,
	  cur_mmu_4k_number,
	  mmu_index_adr);
}






/**************************************************

h265 buffer management end

***************************************************/
static struct hevc_state_s gHevc;

static void hevc_local_uninit(struct hevc_state_s *hevc)
{
	hevc->rpm_ptr = NULL;
	hevc->lmem_ptr = NULL;

	if (hevc->aux_addr) {
		dma_unmap_single(amports_get_dma_device(),
			hevc->aux_phy_addr,
			hevc->prefix_aux_size + hevc->suffix_aux_size,
			DMA_FROM_DEVICE);
		kfree(hevc->aux_addr);
		hevc->aux_addr = NULL;
	}
	if (hevc->rpm_addr) {
		dma_unmap_single(amports_get_dma_device(),
			hevc->rpm_phy_addr, RPM_BUF_SIZE, DMA_FROM_DEVICE);
		kfree(hevc->rpm_addr);
		hevc->rpm_addr = NULL;
	}
	if (hevc->lmem_addr) {
		dma_unmap_single(amports_get_dma_device(),
			hevc->lmem_phy_addr, LMEM_BUF_SIZE, DMA_FROM_DEVICE);
		kfree(hevc->lmem_addr);
		hevc->lmem_addr = NULL;
	}

	if (mmu_enable && hevc->frame_mmu_map_addr) {
		if (hevc->frame_mmu_map_phy_addr)
			dma_free_coherent(amports_get_dma_device(),
				FRAME_MMU_MAP_SIZE, hevc->frame_mmu_map_addr,
					hevc->frame_mmu_map_phy_addr);
			hevc->frame_mmu_map_addr = NULL;
	}

	kfree(gvs);
	gvs = NULL;
}

static int hevc_local_init(struct hevc_state_s *hevc)
{
	int ret = -1;
	struct BuffInfo_s *cur_buf_info = NULL;
	memset(&hevc->param, 0, sizeof(union param_u));

	cur_buf_info = &hevc->work_space_buf_store;
#ifdef SUPPORT_4K2K
	memcpy(cur_buf_info, &amvh265_workbuff_spec[1],	/* 4k2k work space */
		sizeof(struct BuffInfo_s));
#else
	memcpy(cur_buf_info, &amvh265_workbuff_spec[0],	/* 1080p work space */
		sizeof(struct BuffInfo_s));
#endif
	cur_buf_info->start_adr = hevc->buf_start;
	hevc->mc_buf_spec.buf_end = hevc->buf_start + hevc->buf_size;
	init_buff_spec(hevc, cur_buf_info);



	hevc->mc_buf_spec.buf_start = (cur_buf_info->end_adr + 0xffff)
	    & (~0xffff);
	hevc->mc_buf_spec.buf_size = (hevc->mc_buf_spec.buf_end
	    - hevc->mc_buf_spec.buf_start);

	hevc_init_stru(hevc, cur_buf_info, &hevc->mc_buf_spec);

	hevc->bit_depth_luma = 8;
	hevc->bit_depth_chroma = 8;
	hevc->video_signal_type = 0;
	bit_depth_luma = hevc->bit_depth_luma;
	bit_depth_chroma = hevc->bit_depth_chroma;
	video_signal_type = hevc->video_signal_type;

	if ((get_dbg_flag(hevc) & H265_DEBUG_SEND_PARAM_WITH_REG) == 0) {
		hevc->rpm_addr = kmalloc(RPM_BUF_SIZE, GFP_KERNEL);
		if (hevc->rpm_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}

		hevc->rpm_phy_addr = dma_map_single(amports_get_dma_device(),
			hevc->rpm_addr, RPM_BUF_SIZE, DMA_FROM_DEVICE);
		if (dma_mapping_error(amports_get_dma_device(),
			hevc->rpm_phy_addr)) {
			pr_err("%s: failed to map rpm buffer\n", __func__);
			kfree(hevc->rpm_addr);
			hevc->rpm_addr = NULL;
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
		hevc->aux_addr = kmalloc(aux_buf_size, GFP_KERNEL);
		if (hevc->aux_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}

		hevc->aux_phy_addr = dma_map_single(amports_get_dma_device(),
			hevc->aux_addr, aux_buf_size, DMA_FROM_DEVICE);
		if (dma_mapping_error(amports_get_dma_device(),
			hevc->aux_phy_addr)) {
			pr_err("%s: failed to map rpm buffer\n", __func__);
			kfree(hevc->aux_addr);
			hevc->aux_addr = NULL;
			return -1;
		}
	}

	if (get_dbg_flag(hevc) & H265_DEBUG_UCODE) {
		hevc->lmem_addr = kmalloc(LMEM_BUF_SIZE, GFP_KERNEL);
		if (hevc->lmem_addr == NULL) {
			pr_err("%s: failed to alloc lmem buffer\n", __func__);
			return -1;
		}

		hevc->lmem_phy_addr = dma_map_single(amports_get_dma_device(),
			hevc->lmem_addr, LMEM_BUF_SIZE, DMA_FROM_DEVICE);
		if (dma_mapping_error(amports_get_dma_device(),
			hevc->lmem_phy_addr)) {
			pr_err("%s: failed to map lmem buffer\n", __func__);
			kfree(hevc->lmem_addr);
			hevc->lmem_addr = NULL;
			return -1;
		}

		hevc->lmem_ptr = hevc->lmem_addr;
	}

	if (mmu_enable) {
		hevc->frame_mmu_map_addr =
				dma_alloc_coherent(amports_get_dma_device(),
				FRAME_MMU_MAP_SIZE,
				&hevc->frame_mmu_map_phy_addr, GFP_KERNEL);
		if (hevc->frame_mmu_map_addr == NULL) {
			pr_err("%s: failed to alloc count_buffer\n", __func__);
			return -1;
		}
		memset(hevc->frame_mmu_map_addr, 0, FRAME_MMU_MAP_SIZE);
	}
	ret = 0;
	return ret;
}

/********************************************
 *  Mailbox command
 ********************************************/
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
	int canvas_w = ALIGN(pic->width, 64)/4;
	int canvas_h = ALIGN(pic->height, 32)/4;
	int blkmode = mem_map_mode;
	/*CANVAS_BLKMODE_64X32*/
#ifdef SUPPORT_10BIT
	if	(get_double_write_mode(hevc)) {
		canvas_w = pic->width;
		canvas_h = pic->height;
		if ((get_double_write_mode(hevc) == 2) ||
			(get_double_write_mode(hevc) == 3)) {
			canvas_w >>= 2;
			canvas_h >>= 2;
		}

		if (mem_map_mode == 0)
			canvas_w = ALIGN(canvas_w, 32);
		else
			canvas_w = ALIGN(canvas_w, 64);
		canvas_h = ALIGN(canvas_h, 32);

		pic->y_canvas_index = 128 + pic->index * 2;
		pic->uv_canvas_index = 128 + pic->index * 2 + 1;

		canvas_config_ex(pic->y_canvas_index,
			pic->dw_y_adr, canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0x7);
		canvas_config_ex(pic->uv_canvas_index, pic->dw_u_v_adr,
			canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0x7);
#ifdef MULTI_INSTANCE_SUPPORT
		pic->canvas_config[0].phy_addr =
				pic->dw_y_adr;
		pic->canvas_config[0].width =
				canvas_w;
		pic->canvas_config[0].height =
				canvas_h;
		pic->canvas_config[0].block_mode =
				blkmode;
		pic->canvas_config[0].endian = 7;

		pic->canvas_config[1].phy_addr =
				pic->dw_u_v_adr;
		pic->canvas_config[1].width =
				canvas_w;
		pic->canvas_config[1].height =
				canvas_h;
		pic->canvas_config[1].block_mode =
				blkmode;
		pic->canvas_config[1].endian = 7;
#endif
	} else {
		if (!mmu_enable) {
			/* to change after 10bit VPU is ready ... */
			pic->y_canvas_index = 128 + pic->index;
			pic->uv_canvas_index = 128 + pic->index;

			canvas_config_ex(pic->y_canvas_index,
				pic->mc_y_adr, canvas_w, canvas_h,
				CANVAS_ADDR_NOWRAP, blkmode, 0x7);
			canvas_config_ex(pic->uv_canvas_index, pic->mc_u_v_adr,
				canvas_w, canvas_h,
				CANVAS_ADDR_NOWRAP, blkmode, 0x7);
		}
	}
#else
	pic->y_canvas_index = 128 + pic->index * 2;
	pic->uv_canvas_index = 128 + pic->index * 2 + 1;

	canvas_config_ex(pic->y_canvas_index, pic->mc_y_adr, canvas_w, canvas_h,
		CANVAS_ADDR_NOWRAP, blkmode, 0x7);
	canvas_config_ex(pic->uv_canvas_index, pic->mc_u_v_adr,
		canvas_w, canvas_h,
		CANVAS_ADDR_NOWRAP, blkmode, 0x7);
#endif
}

static int init_buf_spec(struct hevc_state_s *hevc)
{
	int pic_width = hevc->pic_w;
	int pic_height = hevc->pic_h;

	/* hevc_print(hevc, 0,
	"%s1: %d %d\n", __func__, hevc->pic_w, hevc->pic_h); */
	hevc_print(hevc, 0,
		"%s2 %d %d\n", __func__, pic_width, pic_height);
	/* pic_width = hevc->pic_w; */
	/* pic_height = hevc->pic_h; */

	if (hevc->frame_width == 0 || hevc->frame_height == 0) {
		hevc->frame_width = pic_width;
		hevc->frame_height = pic_height;

	}

	return 0;
}

static int parse_sei(struct hevc_state_s *hevc, char *sei_buf, uint32_t size)
{
	char *p = sei_buf;
	char *p_sei;
	uint16_t header;
	uint8_t nal_unit_type;
	uint8_t payload_type, payload_size;
	int i, j;

	if (size < 2)
		return 0;
	header = *p++;
	header <<= 8;
	header += *p++;
	nal_unit_type = header >> 9;
	if ((nal_unit_type != NAL_UNIT_SEI)
	&& (nal_unit_type != NAL_UNIT_SEI_SUFFIX))
		return 0;
	while (p+2 <= sei_buf+size) {
		payload_type = *p++;
		payload_size = *p++;
		if (p+payload_size <= sei_buf+size) {
			switch (payload_type) {
			case SEI_MasteringDisplayColorVolume:
				hevc_print(hevc, 0,
					"sei type: primary display color volume %d, size %d\n",
					payload_type,
					payload_size);
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
				for (i = 0; i < 3; i++)
					for (j = 0; j < 2; j++)
						hevc_print(hevc, 0,
						"\tprimaries[%1d][%1d] = %04x\n",
						i, j,
						hevc->primaries[i][j]);
				hevc_print(hevc, 0,
					"\twhite_point = (%04x, %04x)\n",
					hevc->white_point[0],
					hevc->white_point[1]);
				hevc_print(hevc, 0,
					"\tmax,min luminance = %08x, %08x\n",
					hevc->luminance[0],
					hevc->luminance[1]);
				break;
			case SEI_ContentLightLevel:
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

static void set_frame_info(struct hevc_state_s *hevc, struct vframe_s *vf)
{
	unsigned int ar;
	int i, j;
	unsigned char index;
	char *p;
	unsigned size = 0;
	unsigned type = 0;
	struct vframe_master_display_colour_s *vf_dp
		= &vf->prop.master_display_colour;

	if ((get_double_write_mode(hevc) == 2) ||
		(get_double_write_mode(hevc) == 3)) {
		vf->width = hevc->frame_width/4;
		vf->height = hevc->frame_height/4;
	} else {
		vf->width = hevc->frame_width;
		vf->height = hevc->frame_height;
	}
	vf->duration = hevc->frame_dur;
	vf->duration_pulldown = 0;
	vf->flag = 0;

	ar = min_t(u32, hevc->frame_ar, DISP_RATIO_ASPECT_RATIO_MAX);
	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);

	/* signal_type */
	if (hevc->video_signal_type & VIDEO_SIGNAL_TYPE_AVAILABLE_MASK)
		vf->signal_type = hevc->video_signal_type;
	else
		vf->signal_type = 0;

	/* parser sei */
	index = vf->index & 0xff;
	if (index != 0xff && index >= 0
		&& index < MAX_REF_PIC_NUM
		&& hevc->m_PIC[index]
		&& hevc->m_PIC[index]->aux_data_buf
		&& hevc->m_PIC[index]->aux_data_size) {
		p = hevc->m_PIC[index]->aux_data_buf;
		while (p < hevc->m_PIC[index]->aux_data_buf
			+ hevc->m_PIC[index]->aux_data_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (type == 0x02000000) {
				/* hevc_print(hevc, 0, "sei(%d)\n", size); */
				parse_sei(hevc, p, size);
			}
			p += size;
		}
	}

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
	return;
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
	spin_lock_irqsave(&lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hevc->newframe_q);
	states->buf_avail_num = kfifo_len(&hevc->display_q);

	if (step == 2)
		states->buf_avail_num = 0;
	spin_unlock_irqrestore(&lock, flags);
	return 0;
}

static struct vframe_s *vh265_vf_peek(void *op_arg)
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

	if (kfifo_peek(&hevc->display_q, &vf))
		return vf;

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
		if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
			hevc_print(hevc, 0,
				"%s(type %d index 0x%x)\n",
				__func__, vf->type, vf->index);

		hevc->show_frame_num++;
		return vf;
	}

	return NULL;
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
	unsigned char index_top = vf->index & 0xff;
	unsigned char index_bot = (vf->index >> 8) & 0xff;
	if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
		hevc_print(hevc, 0,
			"%s(type %d index 0x%x)\n",
			__func__, vf->type, vf->index);

	kfifo_put(&hevc->newframe_q, (const struct vframe_s *)vf);
	spin_lock_irqsave(&lock, flags);

	if (index_top != 0xff && index_top >= 0
		&& index_top < MAX_REF_PIC_NUM
		&& hevc->m_PIC[index_top]) {
		if (hevc->m_PIC[index_top]->vf_ref > 0) {
			hevc->m_PIC[index_top]->vf_ref--;

			if (hevc->m_PIC[index_top]->vf_ref == 0) {
				hevc->m_PIC[index_top]->output_ready = 0;
				if (mmu_enable)
					hevc->m_PIC[index_top]->
						used_by_display	= 0;
				hevc->last_put_idx_a = index_top;
				if (hevc->wait_buf != 0)
					WRITE_VREG(HEVC_ASSIST_MBOX1_IRQ_REG,
						0x1);
			}
		}
	}

	if (index_bot != 0xff && index_bot >= 0
		&& index_bot < MAX_REF_PIC_NUM
		&& hevc->m_PIC[index_bot]) {
		if (hevc->m_PIC[index_bot]->vf_ref > 0) {
			hevc->m_PIC[index_bot]->vf_ref--;

			if (hevc->m_PIC[index_bot]->vf_ref == 0) {
				clear_used_by_display_flag(hevc);
				hevc->m_PIC[index_bot]->output_ready = 0;
				hevc->last_put_idx_b = index_bot;
				if (hevc->wait_buf != 0)
					WRITE_VREG(HEVC_ASSIST_MBOX1_IRQ_REG,
						0x1);
			}
		}
	}
	spin_unlock_irqrestore(&lock, flags);
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
	if (type & VFRAME_EVENT_RECEIVER_RESET) {
#if 0
		amhevc_stop();
#ifndef CONFIG_POST_PROCESS_MANAGER
		vf_light_unreg_provider(&vh265_vf_prov);
#endif
		spin_lock_irqsave(&hevc->lock, flags);
		vh265_local_init();
		vh265_prot_init();
		spin_unlock_irqrestore(&hevc->lock, flags);
#ifndef CONFIG_POST_PROCESS_MANAGER
		vf_reg_provider(&vh265_vf_prov);
#endif
		amhevc_start();
#endif
	} else if (type & VFRAME_EVENT_RECEIVER_GET_AUX_DATA) {
		struct provider_aux_req_s *req =
			(struct provider_aux_req_s *)data;
		unsigned char index;
		spin_lock_irqsave(&lock, flags);
		index = req->vf->index & 0xff;
		req->aux_buf = NULL;
		req->aux_size = 0;
		if (req->bot_flag)
			index = (req->vf->index >> 8) & 0xff;
		if (index != 0xff && index >= 0
			&& index < MAX_REF_PIC_NUM
			&& hevc->m_PIC[index]) {
			req->aux_buf = hevc->m_PIC[index]->aux_data_buf;
			req->aux_size = hevc->m_PIC[index]->aux_data_size;
#ifdef CONFIG_AM_VDEC_DV
			req->dv_enhance_exist =
				hevc->m_PIC[index]->dv_enhance_exist;
#else
			req->dv_enhance_exist = 0;
#endif
		}
		spin_unlock_irqrestore(&lock, flags);

		if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
			hevc_print(hevc, 0,
			"%s(type 0x%x vf index 0x%x)=>size 0x%x\n",
			__func__, type, index, req->aux_size);
	}

	return 0;
}

#ifdef HEVC_PIC_STRUCT_SUPPORT
static int process_pending_vframe(struct hevc_state_s *hevc,
	struct PIC_s *pair_pic, unsigned char pair_frame_top_flag)
{
	struct vframe_s *vf;
	if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
		hevc_print(hevc, 0,
			"%s: pair_pic index 0x%x %s\n",
			__func__, pair_pic->index,
			pair_frame_top_flag ?
			"top" : "bot");

	if (kfifo_len(&hevc->pending_q) > 1) {
		/* do not pending more than 1 frame */
		if (kfifo_get(&hevc->pending_q, &vf) == 0) {
			hevc_print(hevc, 0,
			"fatal error, no available buffer slot.");
			return -1;
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
			hevc_print(hevc, 0,
			"%s warning(1), vf=>display_q: (index 0x%x)\n",
				__func__, vf->index);
		kfifo_put(&hevc->display_q, (const struct vframe_s *)vf);
	}

	if (kfifo_peek(&hevc->pending_q, &vf)) {
		if (pair_pic == NULL || pair_pic->vf_ref <= 0) {
			/* if pair_pic is recycled (pair_pic->vf_ref <= 0),
			do not use it */
			if (kfifo_get(&hevc->pending_q, &vf) == 0) {
				hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
			}
			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
				"%s warning(2), vf=>display_q: (index 0x%x)\n",
				__func__, vf->index);
			if (vf)
				kfifo_put(&hevc->display_q,
				(const struct vframe_s *)vf);
		} else if ((!pair_frame_top_flag) &&
			(((vf->index >> 8) & 0xff) == 0xff)) {
			if (kfifo_get(&hevc->pending_q, &vf) == 0) {
				hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
			}
			if (vf) {
				vf->type = VIDTYPE_PROGRESSIVE
				| VIDTYPE_VIU_NV21;
				vf->index &= 0xff;
				vf->index |= (pair_pic->index << 8);
				vf->canvas1Addr = spec2canvas(pair_pic);
				pair_pic->vf_ref++;
				kfifo_put(&hevc->display_q,
				(const struct vframe_s *)vf);
				if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
					hevc_print(hevc, 0,
					"%s vf => display_q: (index 0x%x)\n",
					__func__, vf->index);
			}
		} else if (pair_frame_top_flag &&
			((vf->index & 0xff) == 0xff)) {
			if (kfifo_get(&hevc->pending_q, &vf) == 0) {
				hevc_print(hevc, 0,
				"fatal error, no available buffer slot.");
				return -1;
			}
			if (vf) {
				vf->type = VIDTYPE_PROGRESSIVE
				| VIDTYPE_VIU_NV21;
				vf->index &= 0xff00;
				vf->index |= pair_pic->index;
				vf->canvas0Addr = spec2canvas(pair_pic);
				pair_pic->vf_ref++;
				kfifo_put(&hevc->display_q,
				(const struct vframe_s *)vf);
				if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
					hevc_print(hevc, 0,
					"%s vf => display_q: (index 0x%x)\n",
					__func__, vf->index);
			}
		}
	}
	return 0;
}
#endif
static void update_vf_memhandle(struct hevc_state_s *hevc,
	struct vframe_s *vf, int index)
{
	if (index < 0) {
		vf->mem_handle = NULL;
		vf->mem_head_handle = NULL;
	} else if (vf->type & VIDTYPE_SCATTER) {
		vf->mem_handle =
			decoder_mmu_box_get_mem_handle(
				hevc->mmu_box, index);
		vf->mem_head_handle =
			decoder_bmmu_box_get_mem_handle(
				hevc->bmmu_box, HEADER_BUFFER_IDX(index));
	} else {
		vf->mem_handle =
			decoder_bmmu_box_get_mem_handle(
				hevc->bmmu_box, VF_BUFFER_IDX(index));
		vf->mem_head_handle =
			decoder_bmmu_box_get_mem_handle(
				hevc->bmmu_box, HEADER_BUFFER_IDX(index));
	}
	return;
}
static int prepare_display_buf(struct hevc_state_s *hevc, struct PIC_s *pic)
{
	struct vframe_s *vf = NULL;
	int stream_offset = pic->stream_offset;
	unsigned short slice_type = pic->slice_type;

	if (kfifo_get(&hevc->newframe_q, &vf) == 0) {
		hevc_print(hevc, 0,
			"fatal error, no available buffer slot.");
		return -1;
	}

	if (vf) {
#ifdef MULTI_INSTANCE_SUPPORT
		if (vdec_frame_based(hw_to_vdec(hevc))) {
			vf->pts = pic->pts;
			vf->pts_us64 = pic->pts64;
		}
		/* if (pts_lookup_offset(PTS_TYPE_VIDEO,
		   stream_offset, &vf->pts, 0) != 0) { */
		else
#endif
		if (pts_lookup_offset_us64
			(PTS_TYPE_VIDEO, stream_offset, &vf->pts, 0,
			 &vf->pts_us64) != 0) {
#ifdef DEBUG_PTS
			hevc->pts_missed++;
#endif
			vf->pts = 0;
			vf->pts_us64 = 0;
		}
#ifdef DEBUG_PTS
		else
			hevc->pts_hit++;
#endif
		if (pts_unstable && (hevc->frame_dur > 0))
			hevc->pts_mode = PTS_NONE_REF_USE_DURATION;

		if ((hevc->pts_mode == PTS_NORMAL) && (vf->pts != 0)
			&& hevc->get_frame_dur) {
			int pts_diff = (int)vf->pts - hevc->last_lookup_pts;

			if (pts_diff < 0) {
				hevc->pts_mode_switching_count++;
				hevc->pts_mode_recovery_count = 0;

				if (hevc->pts_mode_switching_count >=
					PTS_MODE_SWITCHING_THRESHOLD) {
					hevc->pts_mode =
						PTS_NONE_REF_USE_DURATION;
					hevc_print(hevc, 0,
					"HEVC: switch to n_d mode.\n");
				}

			} else {
				int p = PTS_MODE_SWITCHING_RECOVERY_THREASHOLD;
				hevc->pts_mode_recovery_count++;
				if (hevc->pts_mode_recovery_count > p) {
					hevc->pts_mode_switching_count = 0;
					hevc->pts_mode_recovery_count = 0;
				}
			}
		}

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
			vf->pts_us64 =
				hevc->last_pts_us64 +
				(DUR2PTS(hevc->frame_dur) * 100 / 9);
		}
		hevc->last_pts_us64 = vf->pts_us64;
		if ((get_dbg_flag(hevc) & H265_DEBUG_OUT_PTS) != 0) {
			hevc_print(hevc, 0,
			"H265 dec out pts: vf->pts=%d, vf->pts_us64 = %lld\n",
			 vf->pts, vf->pts_us64);
		}

		/*
		vf->index:
		(1) vf->type is VIDTYPE_PROGRESSIVE
			and vf->canvas0Addr !=  vf->canvas1Addr,
			vf->index[7:0] is the index of top pic
			vf->index[15:8] is the index of bot pic
		(2) other cases,
			only vf->index[7:0] is used
			vf->index[15:8] == 0xff
		*/
		vf->index = 0xff00 | pic->index;
#if 1
/*SUPPORT_10BIT*/
		if (get_double_write_mode(hevc) & 0x10) {
			/* double write only */
			vf->compBodyAddr = 0;
			vf->compHeadAddr = 0;
		} else {

		if (mmu_enable) {
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
		if (get_double_write_mode(hevc)) {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
			vf->type |= VIDTYPE_VIU_NV21;
			if (get_double_write_mode(hevc) == 3) {
				vf->type |= VIDTYPE_COMPRESS;
				if (mmu_enable)
					vf->type |= VIDTYPE_SCATTER;
			}
#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag) {
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
			if (mmu_enable)
				vf->type |= VIDTYPE_SCATTER;
		}
		vf->compWidth = pic->width;
		vf->compHeight = pic->height;
		update_vf_memhandle(hevc, vf, pic->index);
		switch (hevc->bit_depth_luma) {
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
		switch (hevc->bit_depth_chroma) {
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
		if (hevc->mem_saving_mode == 1)
			vf->bitdepth |= BITDEPTH_SAVING_MODE;
#else
		vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
		vf->type |= VIDTYPE_VIU_NV21;
		vf->canvas0Addr = vf->canvas1Addr = spec2canvas(pic);
#endif
		set_frame_info(hevc, vf);
		/* if((vf->width!=pic->width)||(vf->height!=pic->height)) */
		/* hevc_print(hevc, 0,
			"aaa: %d/%d, %d/%d\n",
		   vf->width,vf->height, pic->width, pic->height); */
		if ((get_double_write_mode(hevc) == 2) ||
			(get_double_write_mode(hevc) == 3)) {
			vf->width = pic->width/4;
			vf->height = pic->height/4;
		}	else {
			vf->width = pic->width;
			vf->height = pic->height;
		}
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

		/*
			!!! to do ...
			need move below code to get_new_pic(),
			hevc->xxx can only be used by current decoded pic
		*/
		if (hevc->param.p.conformance_window_flag &&
			(get_dbg_flag(hevc) &
				H265_DEBUG_IGNORE_CONFORMANCE_WINDOW) == 0) {
			unsigned SubWidthC, SubHeightC;
			switch (hevc->param.p.chroma_format_idc) {
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
				(hevc->param.p.conf_win_left_offset +
				hevc->param.p.conf_win_right_offset);
			vf->height -= SubHeightC *
				(hevc->param.p.conf_win_top_offset +
				hevc->param.p.conf_win_bottom_offset);
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
				hevc_print(hevc, 0,
					"conformance_window %d, %d, %d, %d, %d => cropped width %d, height %d\n",
					hevc->param.p.chroma_format_idc,
					hevc->param.p.conf_win_left_offset,
					hevc->param.p.conf_win_right_offset,
					hevc->param.p.conf_win_top_offset,
					hevc->param.p.conf_win_bottom_offset,
					vf->width, vf->height);
		}

#ifdef HEVC_PIC_STRUCT_SUPPORT
		if (pic->pic_struct == 3 || pic->pic_struct == 4) {
			struct vframe_s *vf2;
			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
					"pic_struct = %d index 0x%x\n",
					pic->pic_struct,
					pic->index);

			if (kfifo_get(&hevc->newframe_q, &vf2) == 0) {
				hevc_print(hevc, 0,
					"fatal error, no available buffer slot.");
				return -1;
			}
			pic->vf_ref = 2;
			vf->duration = vf->duration>>1;
			memcpy(vf2, vf, sizeof(struct vframe_s));

			if (pic->pic_struct == 3) {
				vf->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21;
				vf2->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21;
			} else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21;
				vf2->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21;
			}
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf);
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf2);
		} else if (pic->pic_struct == 5
			|| pic->pic_struct == 6) {
			struct vframe_s *vf2, *vf3;
			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
					"pic_struct = %d index 0x%x\n",
					pic->pic_struct,
					pic->index);

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

			if (pic->pic_struct == 5) {
				vf->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21;
				vf2->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21;
				vf3->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21;
			} else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21;
				vf2->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21;
				vf3->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21;
			}
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf);
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf2);
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf3);

		} else if (pic->pic_struct == 9
			|| pic->pic_struct == 10) {
			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
					"pic_struct = %d index 0x%x\n",
					pic->pic_struct,
					pic->index);

			pic->vf_ref = 1;
			/* process previous pending vf*/
			process_pending_vframe(hevc,
			pic, (pic->pic_struct == 9));

			/* process current vf */
			kfifo_put(&hevc->pending_q,
			(const struct vframe_s *)vf);
			vf->height <<= 1;
			if (pic->pic_struct == 9) {
				vf->type = VIDTYPE_INTERLACE_TOP
				| VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
				process_pending_vframe(hevc,
				hevc->pre_bot_pic, 0);
			} else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM |
				VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
				vf->index = (pic->index << 8) | 0xff;
				process_pending_vframe(hevc,
				hevc->pre_top_pic, 1);
			}

			/**/
			if (pic->pic_struct == 9)
				hevc->pre_top_pic = pic;
			else
				hevc->pre_bot_pic = pic;

		} else if (pic->pic_struct == 11
		    || pic->pic_struct == 12) {
			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
				"pic_struct = %d index 0x%x\n",
				pic->pic_struct,
				pic->index);
			pic->vf_ref = 1;
			/* process previous pending vf*/
			process_pending_vframe(hevc, pic,
			(pic->pic_struct == 11));

			/* put current into pending q */
			vf->height <<= 1;
			if (pic->pic_struct == 11)
				vf->type = VIDTYPE_INTERLACE_TOP |
				VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
			else {
				vf->type = VIDTYPE_INTERLACE_BOTTOM |
				VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
				vf->index = (pic->index << 8) | 0xff;
			}
			kfifo_put(&hevc->pending_q,
			(const struct vframe_s *)vf);

			/**/
			if (pic->pic_struct == 11)
				hevc->pre_top_pic = pic;
			else
				hevc->pre_bot_pic = pic;

		} else {
			pic->vf_ref = 1;

			if (get_dbg_flag(hevc) & H265_DEBUG_PIC_STRUCT)
				hevc_print(hevc, 0,
				"pic_struct = %d index 0x%x\n",
				pic->pic_struct,
				pic->index);

			switch (pic->pic_struct) {
			case 7:
				vf->duration <<= 1;
				break;
			case 8:
				vf->duration = vf->duration * 3;
				break;
			case 1:
				vf->height <<= 1;
				vf->type = VIDTYPE_INTERLACE_TOP |
				VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
				process_pending_vframe(hevc, pic, 1);
				hevc->pre_top_pic = pic;
				break;
			case 2:
				vf->height <<= 1;
				vf->type = VIDTYPE_INTERLACE_BOTTOM
				| VIDTYPE_VIU_NV21
				| VIDTYPE_VIU_FIELD;
				process_pending_vframe(hevc, pic, 0);
				hevc->pre_bot_pic = pic;
				break;
			}
			kfifo_put(&hevc->display_q,
			(const struct vframe_s *)vf);
		}
#else
		vf->type_original = vf->type;
		pic->vf_ref = 1;
		kfifo_put(&hevc->display_q, (const struct vframe_s *)vf);
#endif
		/*count info*/
		vdec_count_info(gvs, 0, stream_offset);

		vf_notify_receiver(hevc->provider_name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
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
					data =
					(READ_HREG(HEVC_SHIFTED_DATA) >> 16);
					hevc->primaries[i][j] = data;
					WRITE_HREG(HEVC_SHIFT_COMMAND,
					(1<<7)|16);
					if (get_dbg_flag(hevc) &
						H265_DEBUG_PRINT_SEI)
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
					data =
					(READ_HREG(HEVC_SHIFTED_DATA) >> 16);
					hevc->luminance[i] = data << 16;
					WRITE_HREG(HEVC_SHIFT_COMMAND,
					(1<<7)|16);
					data =
					(READ_HREG(HEVC_SHIFTED_DATA) >> 16);
					hevc->luminance[i] |= data;
					WRITE_HREG(HEVC_SHIFT_COMMAND,
					(1<<7)|16);
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
	unsigned hevc_shift_byte_count;
	unsigned hevc_stream_start_addr;
	unsigned hevc_stream_end_addr;
	unsigned hevc_stream_rd_ptr;
	unsigned hevc_stream_wr_ptr;
	unsigned hevc_stream_control;
	unsigned hevc_stream_fifo_ctl;
	unsigned hevc_stream_buf_size;
	mutex_lock(&vh265_mutex);
#if 0
	for (i = 0; i < (hevc->debug_ptr_size / 2); i += 4) {
		int ii;
		for (ii = 0; ii < 4; ii++)
			hevc_print(hevc, 0,
			"%04x ", hevc->debug_ptr[i + 3 - ii]);
		if (((i + ii) & 0xf) == 0)
			hevc_print(hevc, 0, "\n");
	}
#endif
#define ES_VID_MAN_RD_PTR            (1<<0)
	if (!hevc->init_flag) {
		hevc_print(hevc, 0, "h265 has stopped, recover return!\n");
		mutex_unlock(&vh265_mutex);
		return ret;
	}
	amhevc_stop();
	ret = 0;
	/* reset */
	WRITE_MPEG_REG(PARSER_VIDEO_RP, READ_VREG(HEVC_STREAM_RD_PTR));
	SET_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_VID_MAN_RD_PTR);

	hevc_stream_start_addr = READ_VREG(HEVC_STREAM_START_ADDR);
	hevc_stream_end_addr = READ_VREG(HEVC_STREAM_END_ADDR);
	hevc_stream_rd_ptr = READ_VREG(HEVC_STREAM_RD_PTR);
	hevc_stream_wr_ptr = READ_VREG(HEVC_STREAM_WR_PTR);
	hevc_stream_control = READ_VREG(HEVC_STREAM_CONTROL);
	hevc_stream_fifo_ctl = READ_VREG(HEVC_STREAM_FIFO_CTL);
	hevc_stream_buf_size = hevc_stream_end_addr - hevc_stream_start_addr;

	/* HEVC streaming buffer will reset and restart
	   from current hevc_stream_rd_ptr position */
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

	if (get_double_write_mode(hevc) & 0x10)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1,
			0x1 << 31  /*/Enable NV21 reference read mode for MC*/
			);

	WRITE_VREG(HEVC_WAIT_FLAG, 1);
	/* clear mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX1_CLR_REG, 1);
	/* enable mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX1_MASK, 1);
	/* disable PSCALE for hardware sharing */
	WRITE_VREG(HEVC_PSCALE_CTRL, 0);

	CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_VID_MAN_RD_PTR);

	if (get_dbg_flag(hevc) & H265_DEBUG_UCODE)
		WRITE_VREG(DEBUG_REG1, 0x1);
	else
		WRITE_VREG(DEBUG_REG1, 0x0);

	if ((error_handle_policy & 1) == 0) {
		if ((error_handle_policy & 4) == 0) {
			/* ucode auto mode, and do not check vps/sps/pps/idr */
			WRITE_VREG(NAL_SEARCH_CTL,
					   0xc);
		} else {
			WRITE_VREG(NAL_SEARCH_CTL, 0x1);/* manual parser NAL */
		}
	} else {
		WRITE_VREG(NAL_SEARCH_CTL, 0x1);/* manual parser NAL */
	}

	if (get_dbg_flag(hevc) & H265_DEBUG_NO_EOS_SEARCH_DONE)
		WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | 0x10000);
	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL)
		| ((parser_sei_enable & 0x7) << 17));
#ifdef CONFIG_AM_VDEC_DV
	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL) |
		((parser_dolby_vision_enable & 0x1) << 20));
#endif
	config_decode_mode(hevc);
	WRITE_VREG(DECODE_STOP_POS, decode_stop_pos);

	/* if (amhevc_loadmc(vh265_mc) < 0) { */
	/* amhevc_disable(); */
	/* return -EBUSY; */
	/* } */
#if 0
	for (i = 0; i < (hevc->debug_ptr_size / 2); i += 4) {
		int ii;
		for (ii = 0; ii < 4; ii++) {
			/* hevc->debug_ptr[i+3-ii]=ttt++; */
			hevc_print(hevc, 0,
			"%04x ", hevc->debug_ptr[i + 3 - ii]);
		}
		if (((i + ii) & 0xf) == 0)
			hevc_print(hevc, 0, "\n");
	}
#endif
	init_pic_list_hw(hevc);

	hevc_print(hevc, 0, "%s HEVC_SHIFT_BYTE_COUNT=%x\n", __func__,
		   READ_VREG(HEVC_SHIFT_BYTE_COUNT));

	amhevc_start();

	/* skip, search next start code */
	WRITE_VREG(HEVC_WAIT_FLAG, READ_VREG(HEVC_WAIT_FLAG) & (~0x2));
	hevc->skip_flag = 1;
#ifdef ERROR_HANDLE_DEBUG
	if (dbg_nal_skip_count & 0x20000) {
		dbg_nal_skip_count &= ~0x20000;
		mutex_unlock(&vh265_mutex);
		return ret;
	}
#endif
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
	unsigned short *aux_adr =
		(unsigned short *)
		hevc->aux_addr;
	unsigned aux_size =
		(READ_VREG(HEVC_AUX_DATA_SIZE)
		>> 16) << 4;

	if (hevc->prefix_aux_size > 0) {
		hevc_print(hevc, 0,
			"prefix aux: (size %d)\n",
			aux_size);
		for (i = 0; i <
		(aux_size >> 1); i++) {
			hevc_print_cont(hevc, 0,
				"%04x ",
				*(aux_adr + i));
			if (((i + 1) & 0xf)
				== 0)
				hevc_print_cont(hevc,
				0, "\n");
		}
	}
	if (hevc->suffix_aux_size > 0) {
		aux_adr = (unsigned short *)
			(hevc->aux_addr +
			hevc->prefix_aux_size);
		aux_size =
		(READ_VREG(HEVC_AUX_DATA_SIZE) & 0xffff)
			<< 4;
		hevc_print(hevc, 0,
			"suffix aux: (size %d)\n",
			aux_size);
		for (i = 0; i <
		(aux_size >> 1); i++) {
			hevc_print_cont(hevc, 0,
				"%04x ", *(aux_adr + i));
			if (((i + 1) & 0xf) == 0)
				hevc_print_cont(hevc, 0, "\n");
		}
	}
}

#ifdef CONFIG_AM_VDEC_DV
static void dolby_get_meta(struct hevc_state_s *hevc)
{
	struct vdec_s *vdec = hw_to_vdec(hevc);
	dma_sync_single_for_cpu(
	amports_get_dma_device(),
	hevc->aux_phy_addr,
	hevc->prefix_aux_size + hevc->suffix_aux_size,
	DMA_FROM_DEVICE);
	if (get_dbg_flag(hevc) &
		H265_DEBUG_BUFMGR_MORE)
		dump_aux_buf(hevc);
	if (dolby_meta_with_el || vdec->slave) {
		if (hevc->cur_pic)
			set_aux_data(hevc,
			hevc->cur_pic, 0, 0);
	} else if (vdec->master) {
		struct hevc_state_s *hevc_ba =
		(struct hevc_state_s *)
			vdec->master->private;
		if (hevc_ba->cur_pic
			!= NULL) {
			/*do not use hevc_ba*/
			set_aux_data(hevc,
			hevc_ba->cur_pic,
			0, 1);
		}
		set_aux_data(hevc,
		hevc->cur_pic, 0, 2);
	}
}
#endif

static irqreturn_t vh265_isr_thread_fn(int irq, void *data)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *) data;
	unsigned int dec_status = hevc->dec_status;
	int i, ret;
#ifdef CONFIG_AM_VDEC_DV
	struct vdec_s *vdec = hw_to_vdec(hevc);
#endif
	if (hevc->eos)
		return IRQ_HANDLED;
	if (
#ifdef MULTI_INSTANCE_SUPPORT
		(!hevc->m_ins_flag) &&
#endif
		hevc->error_flag == 1) {
		if ((error_handle_policy & 0x10) == 0) {
			if (hevc->cur_pic) {
				int current_lcu_idx =
					READ_VREG(HEVC_PARSER_LCU_START)
					& 0xffffff;
				if (current_lcu_idx <
					((hevc->lcu_x_num*hevc->lcu_y_num)-1))
					hevc->cur_pic->error_mark = 1;

			}
		}
		if ((error_handle_policy & 1) == 0) {
			hevc->error_skip_nal_count = 1;
			/* manual search nal, skip  error_skip_nal_count
			   of nal and trigger the HEVC_NAL_SEARCH_DONE irq */
			WRITE_VREG(NAL_SEARCH_CTL,
					   (error_skip_nal_count << 4) | 0x1);
		} else {
			hevc->error_skip_nal_count = error_skip_nal_count;
			WRITE_VREG(NAL_SEARCH_CTL, 0x1);/* manual parser NAL */
		}
		if (get_dbg_flag(hevc) & H265_DEBUG_NO_EOS_SEARCH_DONE) {
			WRITE_VREG(NAL_SEARCH_CTL,
					   READ_VREG(NAL_SEARCH_CTL) | 0x10000);
		}
		WRITE_VREG(NAL_SEARCH_CTL,
			READ_VREG(NAL_SEARCH_CTL)
			| ((parser_sei_enable & 0x7) << 17));
#ifdef CONFIG_AM_VDEC_DV
		WRITE_VREG(NAL_SEARCH_CTL,
			READ_VREG(NAL_SEARCH_CTL) |
			((parser_dolby_vision_enable & 0x1) << 20));
#endif
		config_decode_mode(hevc);
		/* search new nal */
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
		/* Interrupt Amrisc to excute */
		WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);

		/* hevc_print(hevc, 0,
		"%s: error handle\n", __func__); */
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
				int current_lcu_idx =
					READ_VREG(HEVC_PARSER_LCU_START)
					& 0xffffff;
				if (current_lcu_idx <
					((hevc->lcu_x_num*hevc->lcu_y_num)-1))
					hevc->cur_pic->error_mark = 1;

			}
		}
		if ((error_handle_policy & 1) == 0) {
			/* need skip some data when
			   error_flag of 3 is triggered, */
			/* to avoid hevc_recover() being called
			   for many times at the same bitstream position */
			hevc->error_skip_nal_count = 1;
			/* manual search nal, skip  error_skip_nal_count
			   of nal and trigger the HEVC_NAL_SEARCH_DONE irq */
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

	i = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
	if ((hevc->shift_byte_count_lo & (1 << 31)) && ((i & (1 << 31)) == 0))
		hevc->shift_byte_count_hi++;
	hevc->shift_byte_count_lo = i;

#ifdef MULTI_INSTANCE_SUPPORT
	if ((dec_status == HEVC_DECPIC_DATA_DONE ||
		dec_status == HEVC_FIND_NEXT_PIC_NAL ||
		dec_status == HEVC_FIND_NEXT_DVEL_NAL)
		&& (hevc->chunk)) {
		hevc->cur_pic->pts = hevc->chunk->pts;
		hevc->cur_pic->pts64 = hevc->chunk->pts64;
	}

	if ((dec_status == HEVC_SEARCH_BUFEMPTY) ||
		(dec_status == HEVC_DECODE_BUFEMPTY) ||
		(dec_status == HEVC_NAL_DECODE_DONE)
		) {
		if (hevc->m_ins_flag) {
#if 1
			if (!vdec_frame_based(hw_to_vdec(hevc))) {
				hevc->dec_result = DEC_RESULT_AGAIN;
				amhevc_stop();
			} else
				hevc->dec_result = DEC_RESULT_GET_DATA;
#else
			if (!vdec_frame_based(hw_to_vdec(hevc)))
				hevc->dec_result = DEC_RESULT_AGAIN;
			else
				hevc->dec_result = DEC_RESULT_DONE;
			amhevc_stop();
#endif
			reset_process_time(hevc);
			schedule_work(&hevc->work);
		}

		return IRQ_HANDLED;
	} else if (dec_status == HEVC_DECPIC_DATA_DONE) {
		if (hevc->m_ins_flag) {
			hevc->dec_result = DEC_RESULT_DONE;
			amhevc_stop();

			reset_process_time(hevc);
			schedule_work(&hevc->work);
		}

		return IRQ_HANDLED;
#ifdef CONFIG_AM_VDEC_DV
	} else if (dec_status == HEVC_FIND_NEXT_PIC_NAL ||
		dec_status == HEVC_FIND_NEXT_DVEL_NAL) {
		if (hevc->m_ins_flag) {
			unsigned next_parser_type =
					READ_HREG(CUR_NAL_UNIT_TYPE);
			if (vdec->slave &&
				dec_status == HEVC_FIND_NEXT_DVEL_NAL) {
				/*cur is base, found enhance*/
				struct hevc_state_s *hevc_el =
				(struct hevc_state_s *)
					vdec->slave->private;
				hevc->switch_dvlayer_flag = 1;
				hevc_el->start_parser_type =
					next_parser_type;
			} else if (vdec->master &&
				dec_status == HEVC_FIND_NEXT_PIC_NAL) {
				/*cur is enhance, found base*/
				struct hevc_state_s *hevc_ba =
				(struct hevc_state_s *)
					vdec->master->private;
				hevc->switch_dvlayer_flag = 1;
				hevc_ba->start_parser_type =
					next_parser_type;
			} else {
				hevc->switch_dvlayer_flag = 0;
				hevc->start_parser_type =
					next_parser_type;
			}
			hevc->dec_result = DEC_RESULT_DONE;
			amhevc_stop();
			reset_process_time(hevc);
			if (READ_VREG(HEVC_AUX_DATA_SIZE) != 0)
				dolby_get_meta(hevc);

			schedule_work(&hevc->work);
		}

		return IRQ_HANDLED;
#endif
	} else if (dec_status == HEVC_DECODE_TIMEOUT) {
		if (vdec_frame_based(hw_to_vdec(hevc)) ||
			(READ_VREG(HEVC_STREAM_LEVEL) > 0x200)) {
			if ((get_dbg_flag(hevc)
				& H265_DEBUG_DIS_LOC_ERROR_PROC)) {
				hevc_print(hevc, 0,
					"%s decoding error, level 0x%x\n",
					__func__, READ_VREG(HEVC_STREAM_LEVEL));
				goto send_again;
			}
			amhevc_stop();
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s %s\n", __func__,
				(dec_status == HEVC_SEARCH_BUFEMPTY) ?
				"HEVC_SEARCH_BUFEMPTY" :
				(dec_status == HEVC_DECODE_BUFEMPTY) ?
				"HEVC_DECODE_BUFEMPTY" : "HEVC_DECODE_TIMEOUT");
			hevc->dec_result = DEC_RESULT_DONE;

			reset_process_time(hevc);
			schedule_work(&hevc->work);
		} else {
			/* WRITE_VREG(dec_status_REG, H264_ACTION_INIT); */
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_AGAIN\n", __func__);
send_again:
			hevc->dec_result = DEC_RESULT_AGAIN;
			reset_process_time(hevc);
			schedule_work(&hevc->work);
		}
		return IRQ_HANDLED;
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
			hevc_print(hevc, 0, "get NAL_UNIT_EOS, flush output\n");
#ifdef CONFIG_AM_VDEC_DV
			if ((vdec->master || vdec->slave) &&
				READ_VREG(HEVC_AUX_DATA_SIZE) != 0)
				dolby_get_meta(hevc);
#endif
			check_pic_decoded_lcu_count(hevc);
			pic = get_pic_by_POC(hevc, hevc->curr_POC);
			hevc->curr_POC = INVALID_POC;
			/* add to fix RAP_B_Bossen_1 */
			hevc->m_pocRandomAccess = MAX_INT;
			flush_output(hevc, pic);
			WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_DISCARD_NAL);
			/* Interrupt Amrisc to excute */
			WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);
#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag) {
				hevc->dec_result = DEC_RESULT_DONE;
				amhevc_stop();

				schedule_work(&hevc->work);
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
#ifdef ERROR_HANDLE_DEBUG
				if (dbg_nal_skip_flag & 1)
					parse_type = HEVC_DISCARD_NAL;
#endif
		} else if (hevc->have_vps) {
			if (naltype == NAL_UNIT_SPS) {
				parse_type = HEVC_NAL_UNIT_SPS;
				hevc->have_sps = 1;
#ifdef ERROR_HANDLE_DEBUG
				if (dbg_nal_skip_flag & 2)
					parse_type = HEVC_DISCARD_NAL;
#endif
			} else if (naltype == NAL_UNIT_PPS) {
				parse_type = HEVC_NAL_UNIT_PPS;
				hevc->have_pps = 1;
#ifdef ERROR_HANDLE_DEBUG
				if (dbg_nal_skip_flag & 4)
					parse_type = HEVC_DISCARD_NAL;
#endif
			} else if (hevc->have_sps && hevc->have_pps) {
				int seg = HEVC_NAL_UNIT_CODED_SLICE_SEGMENT;
				if ((naltype == NAL_UNIT_CODED_SLICE_IDR) ||
					(naltype ==
					NAL_UNIT_CODED_SLICE_IDR_N_LP)
					|| (naltype ==
						NAL_UNIT_CODED_SLICE_CRA)
					|| (naltype ==
						NAL_UNIT_CODED_SLICE_BLA)
					|| (naltype ==
						NAL_UNIT_CODED_SLICE_BLANT)
					|| (naltype ==
						NAL_UNIT_CODED_SLICE_BLA_N_LP)
				) {
					if (slice_parse_begin > 0) {
						hevc_print(hevc, 0,
						"discard %d, for debugging\n",
						 slice_parse_begin);
						slice_parse_begin--;
					} else {
						parse_type = seg;
					}
					hevc->have_valid_start_slice = 1;
				} else if (naltype <=
						NAL_UNIT_CODED_SLICE_CRA
						&& (hevc->have_valid_start_slice
						|| (hevc->PB_skip_mode != 3))) {
					if (slice_parse_begin > 0) {
						hevc_print(hevc, 0,
						"discard %d, dd\n",
						slice_parse_begin);
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
				H265_DEBUG_MAN_SEARCH_NAL) == 0 /*&&
				(!hevc->m_ins_flag)*/) {
				/* auot parser NAL; do not check
				vps/sps/pps/idr */
				WRITE_VREG(NAL_SEARCH_CTL, 0x2);
			}

			if (get_dbg_flag(hevc) &
				H265_DEBUG_NO_EOS_SEARCH_DONE) {
				WRITE_VREG(NAL_SEARCH_CTL,
						READ_VREG(NAL_SEARCH_CTL) |
						0x10000);
			}
			WRITE_VREG(NAL_SEARCH_CTL,
				READ_VREG(NAL_SEARCH_CTL)
				| ((parser_sei_enable & 0x7) << 17));
#ifdef CONFIG_AM_VDEC_DV
			WRITE_VREG(NAL_SEARCH_CTL,
				READ_VREG(NAL_SEARCH_CTL) |
				((parser_dolby_vision_enable & 0x1) << 20));
#endif
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
		if (hevc->m_ins_flag)
			reset_process_time(hevc);
#endif
		if (hevc->start_decoding_time > 0) {
			u32 process_time = 1000*
				(jiffies - hevc->start_decoding_time)/HZ;
			if (process_time > max_decoding_time)
				max_decoding_time = process_time;
		}

		hevc->error_watchdog_count = 0;
		if (hevc->pic_list_init_flag == 2) {
			hevc->pic_list_init_flag = 3;
			hevc_print(hevc, 0, "set pic_list_init_flag to 3\n");
		} else if (hevc->wait_buf == 0) {
			u32 vui_time_scale;
			u32 vui_num_units_in_tick;

			if (get_dbg_flag(hevc) & H265_DEBUG_SEND_PARAM_WITH_REG)
				get_rpm_param(&hevc->param);
			else {
				dma_sync_single_for_cpu(
				amports_get_dma_device(),
				hevc->rpm_phy_addr,
				RPM_BUF_SIZE,
				DMA_FROM_DEVICE);

				for (i = 0; i < (RPM_END - RPM_BEGIN); i += 4) {
					int ii;
					for (ii = 0; ii < 4; ii++) {
						hevc->param.l.data[i + ii] =
							hevc->rpm_ptr[i + 3
							- ii];
					}
				}
			}
			if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR_MORE) {
				hevc_print(hevc, 0,
					"rpm_param: (%d)\n", hevc->slice_idx);
				hevc->slice_idx++;
				for (i = 0; i < (RPM_END - RPM_BEGIN); i++) {
					hevc_print_cont(hevc, 0,
						"%04x ", hevc->param.l.data[i]);
					if (((i + 1) & 0xf) == 0)
						hevc_print_cont(hevc, 0, "\n");
				}

				hevc_print(hevc, 0,
					"vui_timing_info: %x, %x, %x, %x\n",
					hevc->param.p.vui_num_units_in_tick_hi,
					hevc->param.p.vui_num_units_in_tick_lo,
					hevc->param.p.vui_time_scale_hi,
					hevc->param.p.vui_time_scale_lo);
			}
			if (
#ifdef CONFIG_AM_VDEC_DV
				vdec->master == NULL &&
				vdec->slave == NULL &&
#endif
				READ_VREG(HEVC_AUX_DATA_SIZE) != 0
				) {
				dma_sync_single_for_cpu(
				amports_get_dma_device(),
				hevc->aux_phy_addr,
				hevc->prefix_aux_size + hevc->suffix_aux_size,
				DMA_FROM_DEVICE);
				if (get_dbg_flag(hevc) &
					H265_DEBUG_BUFMGR_MORE)
					dump_aux_buf(hevc);
			}

			vui_time_scale =
				(u32)(hevc->param.p.vui_time_scale_hi << 16) |
				hevc->param.p.vui_time_scale_lo;
			vui_num_units_in_tick =
				(u32)(hevc->param.
				p.vui_num_units_in_tick_hi << 16) |
				hevc->param.
				p.vui_num_units_in_tick_lo;
			if (hevc->bit_depth_luma !=
				((hevc->param.p.bit_depth & 0xf) + 8)) {
				hevc_print(hevc, 0, "Bit depth luma = %d\n",
					(hevc->param.p.bit_depth & 0xf) + 8);
			}
			if (hevc->bit_depth_chroma !=
				(((hevc->param.p.bit_depth >> 4) & 0xf) + 8)) {
				hevc_print(hevc, 0, "Bit depth chroma = %d\n",
					((hevc->param.p.bit_depth >> 4) &
					0xf) + 8);
			}
			hevc->bit_depth_luma =
				(hevc->param.p.bit_depth & 0xf) + 8;
			hevc->bit_depth_chroma =
				((hevc->param.p.bit_depth >> 4) & 0xf) + 8;
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
			if ((vui_time_scale != 0)
				&& (vui_num_units_in_tick != 0)) {
				hevc->frame_dur =
					div_u64(96000ULL *
						vui_num_units_in_tick,
						vui_time_scale);
				hevc->get_frame_dur = true;
				gvs->frame_dur = hevc->frame_dur;
			}

			if (hevc->video_signal_type !=
				((hevc->param.p.video_signal_type << 16)
				| hevc->param.p.color_description)) {
				u32 v = hevc->param.p.video_signal_type;
				u32 c = hevc->param.p.color_description;
#if 0
			if (v & 0x2000) {
				hevc_print(hevc, 0,
				"video_signal_type present:\n");
				hevc_print(hevc, 0, " %s %s\n",
				video_format_names[(v >> 10) & 7],
					((v >> 9) & 1) ?
					"full_range" : "limited");
				if (v & 0x100) {
					hevc_print(hevc, 0,
					" color_description present:\n");
					hevc_print(hevc, 0,
					"  color_primarie = %s\n",
					color_primaries_names
					[v & 0xff]);
					hevc_print(hevc, 0,
					"  transfer_characteristic = %s\n",
					transfer_characteristics_names
					[(c >> 8) & 0xff]);
					hevc_print(hevc, 0,
					"  matrix_coefficient = %s\n",
					matrix_coeffs_names[c & 0xff]);
				}
			}
#endif
		hevc->video_signal_type = (v << 16) | c;
		video_signal_type = hevc->video_signal_type;
	}

	if (use_cma &&
		(hevc->param.p.slice_segment_address == 0)
		&& (hevc->pic_list_init_flag == 0)) {
		int log = hevc->param.p.log2_min_coding_block_size_minus3;
		int log_s = hevc->param.p.log2_diff_max_min_coding_block_size;
		hevc->pic_w = hevc->param.p.pic_width_in_luma_samples;
		hevc->pic_h = hevc->param.p.pic_height_in_luma_samples;
		hevc->lcu_size = 1 << (log + 3 + log_s);
		hevc->lcu_size_log2 = log2i(hevc->lcu_size);
		if (hevc->pic_w == 0 || hevc->pic_h == 0
						|| hevc->lcu_size == 0) {
			/* skip search next start code */
			WRITE_VREG(HEVC_WAIT_FLAG, READ_VREG(HEVC_WAIT_FLAG)
						& (~0x2));
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
			hevc->pic_list_init_flag = 1;
#ifdef MULTI_INSTANCE_SUPPORT
			if (hevc->m_ins_flag) {
				schedule_work(&hevc->work);
			} else
#endif
				up(&hevc->h265_sema);
			hevc_print(hevc, 0, "set pic_list_init_flag 1\n");
		}
		return IRQ_HANDLED;
	}

}
	ret =
		hevc_slice_segment_header_process(hevc,
			&hevc->param, decode_pic_begin);
	if (ret < 0)
		;
	else if (ret == 0) {
		if ((hevc->new_pic) && (hevc->cur_pic)) {
			hevc->cur_pic->stream_offset =
			READ_VREG(HEVC_SHIFT_BYTE_COUNT);
		}

		WRITE_VREG(HEVC_DEC_STATUS_REG,
			HEVC_CODED_SLICE_SEGMENT_DAT);
		/* Interrupt Amrisc to excute */
		WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);

		hevc->start_decoding_time = jiffies;
#ifdef MULTI_INSTANCE_SUPPORT
		if (hevc->m_ins_flag)
			start_process_time(hevc);
#endif
#if 1
		/*to do..., copy aux data to hevc->cur_pic*/
#endif
#ifdef MULTI_INSTANCE_SUPPORT
	} else if (hevc->m_ins_flag) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s, bufmgr ret %d skip, DEC_RESULT_DONE\n",
			__func__, ret);
		hevc->dec_result = DEC_RESULT_DONE;
		amhevc_stop();
		reset_process_time(hevc);
		schedule_work(&hevc->work);
#endif
	} else {
		/* skip, search next start code */
		gvs->drop_frame_count++;
		WRITE_VREG(HEVC_WAIT_FLAG, READ_VREG(HEVC_WAIT_FLAG) & (~0x2));
			hevc->skip_flag = 1;
		WRITE_VREG(HEVC_DEC_STATUS_REG,	HEVC_ACTION_DONE);
		/* Interrupt Amrisc to excute */
		WRITE_VREG(HEVC_MCPU_INTR_REQ, AMRISC_MAIN_REQ);
	}

	}

	if (mmu_enable) {
		if (hevc->last_put_idx_a >= 0
			&& hevc->last_put_idx_a < MAX_REF_PIC_NUM) {
			int i = hevc->last_put_idx_a;
			struct PIC_s *pic = hevc->m_PIC[i];

			/*free not used buffers.*/
			if (pic &&
				pic->output_mark == 0 && pic->referenced == 0
				&& pic->output_ready == 0
				&& pic->used_by_display == 0
				&& (pic->index != -1)) {
				decoder_mmu_box_free_idx(hevc->mmu_box, i);
				hevc->last_put_idx_a = -1;
			/*	hevc_print(hevc, 0, "release pic buf %x\n",i);*/
				}
		}
		if (hevc->last_put_idx_b >= 0
			&& hevc->last_put_idx_b < MAX_REF_PIC_NUM) {
			int i = hevc->last_put_idx_b;
			struct PIC_s *pic = hevc->m_PIC[i];

			/*free not used buffers.*/
			if (pic &&
				pic->output_mark == 0 && pic->referenced == 0
				&& pic->output_ready == 0
				&& pic->used_by_display == 0
				&& (pic->index != -1)) {
				decoder_mmu_box_free_idx(hevc->mmu_box, i);
				hevc->last_put_idx_b = -1;
				}
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t vh265_isr(int irq, void *data)
{
	int i, temp;
	unsigned int dec_status;
	struct hevc_state_s *hevc = (struct hevc_state_s *)data;
	dec_status = READ_VREG(HEVC_DEC_STATUS_REG);
	if (hevc->init_flag == 0)
		return IRQ_HANDLED;
	hevc->dec_status = dec_status;
	if (get_dbg_flag(hevc) & H265_DEBUG_BUFMGR)
		hevc_print(hevc, 0,
			"265 isr dec status = 0x%x\n", dec_status);

	if (get_dbg_flag(hevc) & H265_DEBUG_UCODE) {
		if (READ_HREG(DEBUG_REG1) & 0x10000) {
			dma_sync_single_for_cpu(
				amports_get_dma_device(),
				hevc->lmem_phy_addr,
				LMEM_BUF_SIZE,
				DMA_FROM_DEVICE);

			hevc_print(hevc, 0,
				"LMEM<tag %x>:\n", READ_HREG(DEBUG_REG1));

			if (mmu_enable)
				temp = 0x500;
			else
				temp = 0x400;
			for (i = 0; i < temp; i += 4) {
				int ii;
				if ((i & 0xf) == 0)
					hevc_print_cont(hevc, 0, "%03x: ", i);
				for (ii = 0; ii < 4; ii++) {
					hevc_print_cont(hevc, 0, "%04x ",
						   hevc->lmem_ptr[i + 3 - ii]);
				}
				if (((i + ii) & 0xf) == 0)
					hevc_print_cont(hevc, 0, "\n");
			}
			WRITE_HREG(DEBUG_REG1, 0);
		} else if (READ_HREG(DEBUG_REG1) != 0) {
			hevc_print(hevc, 0,
				"dbg%x: %x\n", READ_HREG(DEBUG_REG1),
				   READ_HREG(DEBUG_REG2));
			WRITE_HREG(DEBUG_REG1, 0);
			return IRQ_HANDLED;
		}

	}

	if (hevc->pic_list_init_flag == 1)
		return IRQ_HANDLED;

	return IRQ_WAKE_THREAD;

}

static void vh265_check_timer_func(unsigned long arg)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)arg;
	struct timer_list *timer = &hevc->timer;
	unsigned char empty_flag;
	unsigned int buf_level;

	enum receviver_start_e state = RECEIVER_INACTIVE;
	if (hevc->init_flag == 0) {
		if (hevc->stat & STAT_TIMER_ARM) {
			timer->expires = jiffies + PUT_INTERVAL;
			add_timer(&hevc->timer);
		}
		return;
	}
#ifdef MULTI_INSTANCE_SUPPORT
	if (hevc->m_ins_flag &&
		hw_to_vdec(hevc)->next_status ==
		VDEC_STATUS_DISCONNECTED) {
		hevc->dec_result = DEC_RESULT_FORCE_EXIT;
		schedule_work(&hevc->work);
		hevc_print(hevc,
			0, "vdec requested to be disconnected\n");
		return;
	}

	if (hevc->m_ins_flag) {
		if ((input_frame_based(hw_to_vdec(hevc)) ||
			(READ_VREG(HEVC_STREAM_LEVEL) > 0x200)) &&
			((get_dbg_flag(hevc) &
			H265_DEBUG_DIS_LOC_ERROR_PROC) == 0) &&
			(decode_timeout_val > 0) &&
			(hevc->start_process_time > 0) &&
			((1000 * (jiffies - hevc->start_process_time) / HZ)
				> decode_timeout_val)
		) {
			u32 dec_status = READ_VREG(HEVC_DEC_STATUS_REG);
			int current_lcu_idx =
				READ_VREG(HEVC_PARSER_LCU_START)&0xffffff;
			if (dec_status == HEVC_CODED_SLICE_SEGMENT_DAT) {
				if (hevc->last_lcu_idx == current_lcu_idx) {
					if (hevc->decode_timeout_count > 0)
						hevc->decode_timeout_count--;
					if (hevc->decode_timeout_count == 0)
						timeout_process(hevc);
				}
				hevc->last_lcu_idx = current_lcu_idx;
			} else
				timeout_process(hevc);
		}
	} else {
#endif
	if (hevc->m_ins_flag == 0 &&
		vf_get_receiver(hevc->provider_name)) {
		state =
			vf_notify_receiver(hevc->provider_name,
				VFRAME_EVENT_PROVIDER_QUREY_STATE,
				NULL);
		if ((state == RECEIVER_STATE_NULL)
			|| (state == RECEIVER_STATE_NONE))
			state = RECEIVER_INACTIVE;
	} else
		state = RECEIVER_INACTIVE;

	empty_flag = (READ_VREG(HEVC_PARSER_INT_STATUS) >> 6) & 0x1;
	/* error watchdog */
	if (hevc->m_ins_flag == 0 &&
		(empty_flag == 0)
		&& (hevc->pic_list_init_flag == 0
			|| hevc->pic_list_init_flag
			== 3)) {
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
					if (hevc->error_watchdog_count ==
						error_handle_threshold) {
						hevc_print(hevc, 0,
						"H265 dec err local reset.\n");
						hevc->error_flag = 1;
						hevc->error_watchdog_count = 0;
						hevc->error_skip_nal_wt_cnt = 0;
						hevc->
						error_system_watchdog_count++;
						WRITE_VREG
						(HEVC_ASSIST_MBOX1_IRQ_REG,
						0x1);
					}
				} else if (hevc->error_flag == 2) {
					int th =
						error_handle_nal_skip_threshold;
					hevc->error_skip_nal_wt_cnt++;
					if (hevc->error_skip_nal_wt_cnt
					== th) {
						hevc->error_flag = 3;
						hevc->error_watchdog_count = 0;
						hevc->
						error_skip_nal_wt_cnt =	0;
						WRITE_VREG
						(HEVC_ASSIST_MBOX1_IRQ_REG,
						 0x1);
					}
				}
			}
		}

		if ((get_dbg_flag(hevc)
			& H265_DEBUG_DIS_SYS_ERROR_PROC) == 0)
			/* receiver has no buffer to recycle */
			if ((state == RECEIVER_INACTIVE) &&
				(kfifo_is_empty(&hevc->display_q))
			   ) {	/* no buffer to recycle */
				if ((get_dbg_flag(hevc) &
					H265_DEBUG_DIS_LOC_ERROR_PROC) !=
					0)
					hevc->error_system_watchdog_count++;
				if (hevc->error_system_watchdog_count
					==
					error_handle_system_threshold) {
					/* and it lasts for a while */
					hevc_print(hevc, 0,
					"H265 dec fatal error watchdog.\n");
					hevc->
					error_system_watchdog_count = 0;
					hevc->fatal_error =
					DECODER_FATAL_ERROR_UNKNOW;
				}
			}
	} else {
		hevc->error_watchdog_count = 0;
		hevc->error_system_watchdog_count = 0;
	}
#ifdef MULTI_INSTANCE_SUPPORT
	}
#endif
	if (decode_stop_pos != decode_stop_pos_pre) {
		WRITE_VREG(DECODE_STOP_POS, decode_stop_pos);
		decode_stop_pos_pre = decode_stop_pos;
	}

	if (get_dbg_flag(hevc) & H265_DEBUG_DUMP_PIC_LIST) {
		dump_pic_list(hevc);
		debug &= ~H265_DEBUG_DUMP_PIC_LIST;
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_TRIG_SLICE_SEGMENT_PROC) {
		WRITE_VREG(HEVC_ASSIST_MBOX1_IRQ_REG, 0x1);
		debug &= ~H265_DEBUG_TRIG_SLICE_SEGMENT_PROC;
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_HW_RESET) {
		hevc->error_skip_nal_count = error_skip_nal_count;
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);

		debug &= ~H265_DEBUG_HW_RESET;
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_ERROR_TRIG) {
		WRITE_VREG(DECODE_STOP_POS, 1);
		debug &= ~H265_DEBUG_ERROR_TRIG;
	}
#ifdef ERROR_HANDLE_DEBUG
	if ((dbg_nal_skip_count > 0) && ((dbg_nal_skip_count & 0x10000) != 0)) {
		hevc->error_skip_nal_count = dbg_nal_skip_count & 0xffff;
		dbg_nal_skip_count &= ~0x10000;
		WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
	}
#endif

	if (radr != 0) {
		if (rval != 0) {
			WRITE_VREG(radr, rval);
			hevc_print(hevc, 0,
				"WRITE_VREG(%x,%x)\n", radr, rval);
		} else
			hevc_print(hevc, 0,
				"READ_VREG(%x)=%x\n", radr, READ_VREG(radr));
		rval = 0;
		radr = 0;
	}
	if (dbg_cmd != 0) {
		if (dbg_cmd == 1) {
			u32 disp_laddr;
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB &&
				get_double_write_mode(hevc) == 0) {
				disp_laddr =
					READ_VCBUS_REG(AFBC_BODY_BADDR) << 4;
			} else {
				struct canvas_s cur_canvas;
				canvas_read((READ_VCBUS_REG(VD1_IF0_CANVAS0)
					& 0xff), &cur_canvas);
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
		hevc->frame_width * hevc->frame_height *
			(96000 / hevc->frame_dur)) {
		int fps = 96000 / hevc->frame_dur;
		if (hevc_source_changed(VFORMAT_HEVC,
			hevc->frame_width, hevc->frame_height, fps) > 0)
			hevc->saved_resolution = hevc->frame_width *
			hevc->frame_height * fps;
	}


	timer->expires = jiffies + PUT_INTERVAL;
	add_timer(timer);
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
		ret = down_interruptible(&hevc->h265_sema);
		if ((hevc->init_flag != 0) && (hevc->pic_list_init_flag == 1)) {
			/*USE_BUF_BLOCK*/
			init_buf_list(hevc);
			/**/
			init_pic_list(hevc);
			init_pic_list_hw(hevc);
			init_buf_spec(hevc);
			hevc->pic_list_init_flag = 2;
			hevc_print(hevc, 0, "set pic_list_init_flag to 2\n");

			WRITE_VREG(HEVC_ASSIST_MBOX1_IRQ_REG, 0x1);

		}

		if (hevc->uninit_list) {
			/*USE_BUF_BLOCK*/
			uninit_pic_list(hevc);
			hevc_print(hevc, 0, "uninit list\n");
			hevc->uninit_list = 0;
#ifdef USE_UNINIT_SEMA
			up(&hevc->h265_uninit_done_sema);
#endif
		}

	}

	return 0;

}

void vh265_free_cmabuf(void)
{
	struct hevc_state_s *hevc = &gHevc;

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
	struct hevc_state_s *hevc = &gHevc;
	vstatus->frame_width = hevc->frame_width;
	vstatus->frame_height = hevc->frame_height;
	if (hevc->frame_dur != 0)
		vstatus->frame_rate = 96000 / hevc->frame_dur;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = 0;
	vstatus->status = hevc->stat | hevc->fatal_error;
	vstatus->bit_rate = gvs->bit_rate;
	vstatus->frame_dur = hevc->frame_dur;
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

static int vh265_vdec_info_init(void)
{
	gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);
	if (NULL == gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}
	return 0;
}

#if 0
static void H265_DECODE_INIT(void)
{
	/* enable hevc clocks */
	WRITE_VREG(DOS_GCLK_EN3, 0xffffffff);
	/* *************************************************************** */
	/* Power ON HEVC */
	/* *************************************************************** */
	/* Powerup HEVC */
	WRITE_VREG(P_AO_RTI_GEN_PWR_SLEEP0,
			READ_VREG(P_AO_RTI_GEN_PWR_SLEEP0) & (~(0x3 << 6)));
	WRITE_VREG(DOS_MEM_PD_HEVC, 0x0);
	WRITE_VREG(DOS_SW_RESET3, READ_VREG(DOS_SW_RESET3) | (0x3ffff << 2));
	WRITE_VREG(DOS_SW_RESET3, READ_VREG(DOS_SW_RESET3) & (~(0x3ffff << 2)));
	/* remove isolations */
	WRITE_VREG(AO_RTI_GEN_PWR_ISO0,
			READ_VREG(AO_RTI_GEN_PWR_ISO0) & (~(0x3 << 10)));

}
#endif

static void config_decode_mode(struct hevc_state_s *hevc)
{
#ifdef CONFIG_AM_VDEC_DV
	struct vdec_s *vdec = hw_to_vdec(hevc);
#endif
	if (!hevc->m_ins_flag)
		WRITE_VREG(HEVC_DECODE_MODE,
			DECODE_MODE_SINGLE);
	else if (vdec_frame_based(hw_to_vdec(hevc)))
		WRITE_VREG(HEVC_DECODE_MODE,
			DECODE_MODE_MULTI_FRAMEBASE);
#ifdef CONFIG_AM_VDEC_DV
	else if (vdec->slave)
		WRITE_VREG(HEVC_DECODE_MODE,
			(hevc->start_parser_type << 8)
			| DECODE_MODE_MULTI_DVBAL);
	else if (vdec->master)
		WRITE_VREG(HEVC_DECODE_MODE,
			(hevc->start_parser_type << 8)
			| DECODE_MODE_MULTI_DVENL);
#endif
	else
		WRITE_VREG(HEVC_DECODE_MODE,
			DECODE_MODE_MULTI_STREAMBASE);
}

static void vh265_prot_init(struct hevc_state_s *hevc)
{
	/* H265_DECODE_INIT(); */

	hevc_config_work_space_hw(hevc);

	hevc_init_decoder_hw(hevc, 0, 0xffffffff);

	WRITE_VREG(HEVC_WAIT_FLAG, 1);

	/* WRITE_VREG(P_HEVC_MPSR, 1); */

	/* clear mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX1_MASK, 1);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(HEVC_PSCALE_CTRL, 0);

	if (get_dbg_flag(hevc) & H265_DEBUG_UCODE)
		WRITE_VREG(DEBUG_REG1, 0x1 | (dump_nal << 8));
	else
		WRITE_VREG(DEBUG_REG1, 0x0 | (dump_nal << 8));

	if ((get_dbg_flag(hevc) &
		(H265_DEBUG_MAN_SKIP_NAL |
		H265_DEBUG_MAN_SEARCH_NAL))/* ||
		hevc->m_ins_flag*/) {
		WRITE_VREG(NAL_SEARCH_CTL, 0x1);	/* manual parser NAL */
	} else {
		/* check vps/sps/pps/i-slice in ucode */
		unsigned ctl_val = 0x8;
#ifdef MULTI_INSTANCE_SUPPORT
		if (hevc->m_ins_flag &&
			hevc->init_flag) {
			/* do not check vps/sps/pps/i-slice in ucode
			from the 2nd picture*/
			ctl_val = 0x2;
		} else
#endif
		if (hevc->PB_skip_mode == 0)
			ctl_val = 0x4;	/* check vps/sps/pps only in ucode */
		else if (hevc->PB_skip_mode == 3)
			ctl_val = 0x0;	/* check vps/sps/pps/idr in ucode */
		WRITE_VREG(NAL_SEARCH_CTL, ctl_val);
	}
	if (get_dbg_flag(hevc) & H265_DEBUG_NO_EOS_SEARCH_DONE)
		WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | 0x10000);

	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL)
		| ((parser_sei_enable & 0x7) << 17));
#ifdef CONFIG_AM_VDEC_DV
	WRITE_VREG(NAL_SEARCH_CTL,
		READ_VREG(NAL_SEARCH_CTL) |
		((parser_dolby_vision_enable & 0x1) << 20));
#endif
	WRITE_VREG(DECODE_STOP_POS, decode_stop_pos);

	config_decode_mode(hevc);
	config_aux_buf(hevc);
}

static int vh265_local_init(struct hevc_state_s *hevc)
{
	int i;
	int ret = -1;
#ifdef DEBUG_PTS
	hevc->pts_missed = 0;
	hevc->pts_hit = 0;
#endif

	hevc->last_put_idx_a = -1;
	hevc->last_put_idx_b = -1;
	hevc->saved_resolution = 0;
	hevc->get_frame_dur = false;
	hevc->frame_width = hevc->vh265_amstream_dec_info.width;
	hevc->frame_height = hevc->vh265_amstream_dec_info.height;
	if (HEVC_SIZE < hevc->frame_width * hevc->frame_height) {
		pr_info("over size : %u x %u.\n",
			hevc->frame_width, hevc->frame_height);
		hevc->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
		return ret;
	}
	hevc->frame_dur =
		(hevc->vh265_amstream_dec_info.rate ==
		 0) ? 3600 : hevc->vh265_amstream_dec_info.rate;
	gvs->frame_dur = hevc->frame_dur;
	if (hevc->frame_width && hevc->frame_height)
		hevc->frame_ar = hevc->frame_height * 0x100 / hevc->frame_width;

	if (i_only_flag)
		hevc->i_only = i_only_flag & 0xff;
	else if ((unsigned long) hevc->vh265_amstream_dec_info.param
		& 0x08)
		hevc->i_only = 0x7;
	else
		hevc->i_only = 0x0;
	hevc->error_watchdog_count = 0;
	hevc->sei_present_flag = 0;
	pts_unstable = ((unsigned long)hevc->vh265_amstream_dec_info.param
		& 0x40) >> 6;
	hevc_print(hevc, 0,
		"h265:pts_unstable=%d\n", pts_unstable);
/*
TODO:FOR VERSION
*/
	hevc_print(hevc, 0,
		"h265: ver (%d,%d) decinfo: %dx%d rate=%d\n", h265_version,
		   0, hevc->frame_width, hevc->frame_height, hevc->frame_dur);

	if (hevc->frame_dur == 0)
		hevc->frame_dur = 96000 / 24;

	INIT_KFIFO(hevc->display_q);
	INIT_KFIFO(hevc->newframe_q);


	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hevc->vfpool[i];
		hevc->vfpool[i].index = -1;
		kfifo_put(&hevc->newframe_q, vf);
	}


	ret = hevc_local_init(hevc);

	return ret;
}
#ifdef MULTI_INSTANCE_SUPPORT
static s32 vh265_init(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc = (struct hevc_state_s *)vdec->private;
#else
static s32 vh265_init(struct hevc_state_s *hevc)
{
#endif
	init_timer(&hevc->timer);

	hevc->stat |= STAT_TIMER_INIT;
	if (vh265_local_init(hevc) < 0)
		return -EBUSY;

#ifdef MULTI_INSTANCE_SUPPORT
	if (hevc->m_ins_flag) {
		hevc->timer.data = (ulong) hevc;
		hevc->timer.function = vh265_check_timer_func;
		hevc->timer.expires = jiffies + PUT_INTERVAL;

		/*add_timer(&hevc->timer);

		hevc->stat |= STAT_TIMER_ARM;*/

		INIT_WORK(&hevc->work, vh265_work);

		return 0;
	}
#endif
	amhevc_enable();
	if (mmu_enable && (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)) {
		if (amhevc_loadmc_ex(VFORMAT_HEVC, "vh265_mc_mmu", NULL)
			< 0) {
			amhevc_disable();
			return -EBUSY;
		}
		hevc_print(hevc, 0, "vh265 mmu ucode loaded!\n");
	} else if (amhevc_loadmc_ex(VFORMAT_HEVC, "vh265_mc", NULL) < 0) {
		amhevc_disable();
		return -EBUSY;
	}
	hevc->stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	vh265_prot_init(hevc);

	if (vdec_request_threaded_irq(VDEC_IRQ_1, vh265_isr,
				vh265_isr_thread_fn,
				IRQF_ONESHOT,/*run thread on this irq disabled*/
				"vh265-irq", (void *)hevc)) {
		hevc_print(hevc, 0, "vh265 irq register error.\n");
		amhevc_disable();
		return -ENOENT;
	}

	hevc->stat |= STAT_ISR_REG;
	hevc->provider_name = PROVIDER_NAME;

#ifdef MULTI_INSTANCE_SUPPORT
	vf_provider_init(&vh265_vf_prov, hevc->provider_name,
				&vh265_vf_provider, vdec);
	vf_reg_provider(&vh265_vf_prov);
	vf_notify_receiver(hevc->provider_name, VFRAME_EVENT_PROVIDER_START,
				NULL);
	vf_notify_receiver(hevc->provider_name, VFRAME_EVENT_PROVIDER_FR_HINT,
				(void *)((unsigned long)hevc->frame_dur));
#else
	vf_provider_init(&vh265_vf_prov, PROVIDER_NAME, &vh265_vf_provider,
					 hevc);
	vf_reg_provider(&vh265_vf_prov);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_FR_HINT,
				(void *)((unsigned long)hevc->frame_dur));
#endif
	hevc->stat |= STAT_VF_HOOK;

	hevc->timer.data = (ulong) hevc;
	hevc->timer.function = vh265_check_timer_func;
	hevc->timer.expires = jiffies + PUT_INTERVAL;

	add_timer(&hevc->timer);

	hevc->stat |= STAT_TIMER_ARM;

	if (use_cma) {
		if (h265_task == NULL) {
			sema_init(&hevc->h265_sema, 1);
#ifdef USE_UNINIT_SEMA
			sema_init(
			&hevc->h265_uninit_done_sema, 0);
#endif
			h265_task =
				kthread_run(h265_task_handle, hevc,
						"kthread_h265");
		}
	}
	/* hevc->stat |= STAT_KTHREAD; */

	if (get_dbg_flag(hevc) & H265_DEBUG_FORCE_CLK) {
		hevc_print(hevc, 0, "%s force clk\n", __func__);
		WRITE_VREG(HEVC_IQIT_CLK_RST_CTRL,
				   READ_VREG(HEVC_IQIT_CLK_RST_CTRL) |
				   ((1 << 2) | (1 << 1)));
		WRITE_VREG(HEVC_DBLK_CFG0,
				READ_VREG(HEVC_DBLK_CFG0) | ((1 << 2) |
					(1 << 1) | 0x3fff0000));/* 2,29:16 */
		WRITE_VREG(HEVC_SAO_CTRL1, READ_VREG(HEVC_SAO_CTRL1) |
				(1 << 2));	/* 2 */
		WRITE_VREG(HEVC_MPRED_CTRL1, READ_VREG(HEVC_MPRED_CTRL1) |
				(1 << 24));	/* 24 */
		WRITE_VREG(HEVC_STREAM_CONTROL,
				READ_VREG(HEVC_STREAM_CONTROL) |
				(1 << 15));	/* 15 */
		WRITE_VREG(HEVC_CABAC_CONTROL, READ_VREG(HEVC_CABAC_CONTROL) |
				(1 << 13));	/* 13 */
		WRITE_VREG(HEVC_PARSER_CORE_CONTROL,
				READ_VREG(HEVC_PARSER_CORE_CONTROL) |
				(1 << 15));	/* 15 */
		WRITE_VREG(HEVC_PARSER_INT_CONTROL,
				READ_VREG(HEVC_PARSER_INT_CONTROL) |
				(1 << 15));	/* 15 */
		WRITE_VREG(HEVC_PARSER_IF_CONTROL,
				READ_VREG(HEVC_PARSER_IF_CONTROL) | ((1 << 6) |
					(1 << 3) | (1 << 1)));	/* 6, 3, 1 */
		WRITE_VREG(HEVCD_IPP_DYNCLKGATE_CONFIG,
				READ_VREG(HEVCD_IPP_DYNCLKGATE_CONFIG) |
				0xffffffff);	/* 31:0 */
		WRITE_VREG(HEVCD_MCRCC_CTL1, READ_VREG(HEVCD_MCRCC_CTL1) |
				(1 << 3));	/* 3 */
	}

	amhevc_start();

	hevc->stat |= STAT_VDEC_RUN;
#ifndef MULTI_INSTANCE_SUPPORT
	set_vdec_func(&vh265_dec_status);
#endif
	hevc->init_flag = 1;
	if (mmu_enable)
		error_handle_threshold = 300;
	else
		error_handle_threshold = 30;
	/* pr_info("%d, vh265_init, RP=0x%x\n",
	   __LINE__, READ_VREG(HEVC_STREAM_RD_PTR)); */

	return 0;
}

static int vh265_stop(struct hevc_state_s *hevc)
{

	hevc->init_flag = 0;

	if (get_dbg_flag(hevc) &
		H265_DEBUG_WAIT_DECODE_DONE_WHEN_STOP) {
		int wait_timeout_count = 0;
		while (READ_VREG(HEVC_DEC_STATUS_REG) ==
			   HEVC_CODED_SLICE_SEGMENT_DAT &&
				wait_timeout_count < 10){
			wait_timeout_count++;
			msleep(20);
		}
	}
	if (hevc->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hevc->stat &= ~STAT_VDEC_RUN;
	}

	if (hevc->stat & STAT_ISR_REG) {
		WRITE_VREG(HEVC_ASSIST_MBOX1_MASK, 0);
		vdec_free_irq(VDEC_IRQ_1, (void *)hevc);
		hevc->stat &= ~STAT_ISR_REG;
	}

	hevc->stat &= ~STAT_TIMER_INIT;
	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}

	if (hevc->stat & STAT_VF_HOOK) {
		vf_notify_receiver(hevc->provider_name,
				VFRAME_EVENT_PROVIDER_FR_END_HINT, NULL);

		vf_unreg_provider(&vh265_vf_prov);
		hevc->stat &= ~STAT_VF_HOOK;
	}

	hevc_local_uninit(hevc);

	if (use_cma) {
#ifdef USE_UNINIT_SEMA
		int ret;
#endif
		hevc->uninit_list = 1;
		up(&hevc->h265_sema);
#ifdef USE_UNINIT_SEMA
		ret = down_interruptible(
			&hevc->h265_uninit_done_sema);
#else
		while (hevc->uninit_list)	/* wait uninit complete */
			msleep(20);
#endif

	}
	uninit_mmu_buffers(hevc);
	amhevc_disable();

	kfree(gvs);
	gvs = NULL;

	return 0;
}

#ifdef MULTI_INSTANCE_SUPPORT
static void reset_process_time(struct hevc_state_s *hevc)
{
	if (hevc->start_process_time) {
		unsigned process_time =
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

static void timeout_process(struct hevc_state_s *hevc)
{
	hevc->timeout_num++;
	amhevc_stop();
	hevc_print(hevc,
		0, "%s decoder timeout\n", __func__);

	hevc->dec_result = DEC_RESULT_DONE;
	reset_process_time(hevc);
	schedule_work(&hevc->work);
}

static unsigned char is_new_pic_available(struct hevc_state_s *hevc)
{
	struct PIC_s *new_pic = NULL;
	struct PIC_s *pic;
	/* recycle un-used pic */
	int i;
	/*return 1 if pic_list is not initialized yet*/
	if (hevc->pic_list_init_flag != 3)
		return 1;

	for (i = 0; i < MAX_REF_PIC_NUM; i++) {
		pic = hevc->m_PIC[i];
		if (pic == NULL || pic->index == -1)
			continue;
		if ((pic->used_by_display)
			&& ((READ_VCBUS_REG(AFBC_BODY_BADDR) << 4) !=
				pic->mc_y_adr))
			pic->used_by_display = 0;
		if (pic->output_mark == 0 && pic->referenced == 0
			&& pic->output_ready == 0
			&& pic->used_by_display == 0) {
			if (new_pic) {
				if (pic->POC < new_pic->POC)
					new_pic = pic;
			} else
				new_pic = pic;
		}
	}
	return (new_pic != NULL) ? 1 : 0;
}

static int vmh265_stop(struct hevc_state_s *hevc)
{
	hevc->init_flag = 0;

	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}

	if (hevc->stat & STAT_VF_HOOK) {
		vf_notify_receiver(hevc->provider_name,
				VFRAME_EVENT_PROVIDER_FR_END_HINT, NULL);

		vf_unreg_provider(&vh265_vf_prov);
		hevc->stat &= ~STAT_VF_HOOK;
	}

	hevc_local_uninit(hevc);

	if (use_cma) {
#ifdef USE_UNINIT_SEMA
		int ret;
#endif
		hevc->uninit_list = 1;
		reset_process_time(hevc);
		schedule_work(&hevc->work);
#ifdef USE_UNINIT_SEMA
		ret = down_interruptible(
			&hevc->h265_uninit_done_sema);
#else
		while (hevc->uninit_list)	/* wait uninit complete */
			msleep(20);
#endif
	}
	cancel_work_sync(&hevc->work);
	uninit_mmu_buffers(hevc);
	return 0;
}

static unsigned char get_data_check_sum
	(struct hevc_state_s *hevc, int size)
{
	int jj;
	int sum = 0;
	u8 *data = ((u8 *)hevc->chunk->block->start_virt) +
		hevc->chunk->offset;
	for (jj = 0; jj < size; jj++)
		sum += data[jj];
	return sum;
}

static void vh265_work(struct work_struct *work)
{
	struct hevc_state_s *hevc = container_of(work,
		struct hevc_state_s, work);
	struct vdec_s *vdec = hw_to_vdec(hevc);
	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */
	if ((hevc->init_flag != 0) && (hevc->pic_list_init_flag == 1)) {
		/*USE_BUF_BLOCK*/
		init_buf_list(hevc);
		/**/
		init_pic_list(hevc);
		init_pic_list_hw(hevc);
		init_buf_spec(hevc);
		hevc->pic_list_init_flag = 2;
		hevc_print(hevc, 0,
			"set pic_list_init_flag to 2\n");

		WRITE_VREG(HEVC_ASSIST_MBOX1_IRQ_REG, 0x1);
		return;
	}

	if (hevc->uninit_list) {
		/*USE_BUF_BLOCK*/
		uninit_pic_list(hevc);
		hevc_print(hevc, 0, "uninit list\n");
		hevc->uninit_list = 0;
#ifdef USE_UNINIT_SEMA
		up(&hevc->h265_uninit_done_sema);
#endif
		return;
	}

	hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
		"%s dec_result %d %x %x %x\n",
		__func__,
		hevc->dec_result,
		READ_VREG(HEVC_STREAM_LEVEL),
		READ_VREG(HEVC_STREAM_WR_PTR),
		READ_VREG(HEVC_STREAM_RD_PTR));

	if ((hevc->dec_result == DEC_RESULT_GET_DATA) ||
		(hevc->dec_result == DEC_RESULT_GET_DATA_RETRY)) {
		if (!vdec_has_more_input(vdec)) {
			hevc->dec_result = DEC_RESULT_EOS;
			schedule_work(&hevc->work);
			return;
		}

		if (hevc->dec_result == DEC_RESULT_GET_DATA) {
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_GET_DATA %x %x %x mpc %x\n",
				__func__,
				READ_VREG(HEVC_STREAM_LEVEL),
				READ_VREG(HEVC_STREAM_WR_PTR),
				READ_VREG(HEVC_STREAM_RD_PTR),
				READ_VREG(HEVC_MPC_E));
			vdec_vframe_dirty(vdec, hevc->chunk);
			vdec_clean_input(vdec);
		}

		/*if (is_new_pic_available(hevc)) {*/
		if (run_ready(vdec)) {
			int r;
			r = vdec_prepare_input(vdec, &hevc->chunk);
			if (r < 0) {
				hevc->dec_result = DEC_RESULT_GET_DATA_RETRY;

				hevc_print(hevc,
					PRINT_FLAG_VDEC_DETAIL,
					"amvdec_vh265: Insufficient data\n");

				schedule_work(&hevc->work);
				return;
			}
			hevc->dec_result = DEC_RESULT_NONE;
			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s: chunk size 0x%x sum 0x%x mpc %x\n",
				__func__, r,
				(get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS) ?
				get_data_check_sum(hevc, r) : 0,
				READ_VREG(HEVC_MPC_E));
			if ((get_dbg_flag(hevc) & PRINT_FRAMEBASE_DATA) &&
				input_frame_based(vdec)) {
				int jj;
				u8 *data =
				((u8 *)hevc->chunk->block->start_virt) +
					hevc->chunk->offset;
				for (jj = 0; jj < r; jj++) {
					if ((jj & 0xf) == 0)
						hevc_print(hevc,
						PRINT_FRAMEBASE_DATA,
							"%06x:", jj);
					hevc_print_cont(hevc,
					PRINT_FRAMEBASE_DATA,
						"%02x ", data[jj]);
					if (((jj + 1) & 0xf) == 0)
						hevc_print_cont(hevc,
						PRINT_FRAMEBASE_DATA,
							"\n");
				}
			}

			WRITE_VREG(HEVC_DECODE_SIZE, r);

			vdec_enable_input(vdec);

			hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
				"%s: mpc %x\n",
				__func__, READ_VREG(HEVC_MPC_E));

			start_process_time(hevc);
			WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
		} else{
			hevc->dec_result = DEC_RESULT_GET_DATA_RETRY;

			/*hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
				"amvdec_vh265: Insufficient data\n");
			*/

			schedule_work(&hevc->work);
		}
		return;
	} else if (hevc->dec_result == DEC_RESULT_DONE) {
		/* if (!hevc->ctx_valid)
			hevc->ctx_valid = 1; */
		if (mmu_enable)
			hevc->used_4k_num =
				READ_VREG(HEVC_SAO_MMU_STATUS) >> 16;
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s dec_result %d %x %x %x\n",
			__func__,
			hevc->dec_result,
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR));
#ifdef CONFIG_AM_VDEC_DV
#if 1
		if (vdec->slave) {
			if (dv_debug & 0x1)
				vdec_set_flag(vdec->slave,
					VDEC_FLAG_SELF_INPUT_CONTEXT);
			else
				vdec_set_flag(vdec->slave,
					VDEC_FLAG_OTHER_INPUT_CONTEXT);
		}
#else
		if (vdec->slave) {
			if (no_interleaved_el_slice)
				vdec_set_flag(vdec->slave,
				VDEC_FLAG_INPUT_KEEP_CONTEXT);
				/* this will move real HW pointer for input */
			else
				vdec_set_flag(vdec->slave, 0);
				/* this will not move real HW pointer
				and SL layer decoding
				will start from same stream position
				as current BL decoder */
		}
#endif
#endif
		vdec_vframe_dirty(hw_to_vdec(hevc), hevc->chunk);
	} else if (hevc->dec_result == DEC_RESULT_AGAIN) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		if (!vdec_has_more_input(vdec)) {
			hevc->dec_result = DEC_RESULT_EOS;
			schedule_work(&hevc->work);
			return;
		}

	} else if (hevc->dec_result == DEC_RESULT_EOS) {
		struct PIC_s *pic;
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s: end of stream\n",
			__func__);
		hevc->eos = 1;
#ifdef CONFIG_AM_VDEC_DV
		if ((vdec->master || vdec->slave) &&
			READ_VREG(HEVC_AUX_DATA_SIZE) != 0)
			dolby_get_meta(hevc);
#endif
		check_pic_decoded_lcu_count(hevc);
		pic = get_pic_by_POC(hevc, hevc->curr_POC);
		flush_output(hevc, pic);
		vdec_vframe_dirty(hw_to_vdec(hevc), hevc->chunk);
	} else if (hevc->dec_result == DEC_RESULT_FORCE_EXIT) {
		hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
			"%s: force exit\n",
			__func__);
		if (hevc->stat & STAT_VDEC_RUN) {
			amhevc_stop();
			hevc->stat &= ~STAT_VDEC_RUN;
		}
		if (hevc->stat & STAT_ISR_REG) {
			WRITE_VREG(HEVC_ASSIST_MBOX1_MASK, 0);
			vdec_free_irq(VDEC_IRQ_1, (void *)hevc);
			hevc->stat &= ~STAT_ISR_REG;
		}
	}

	if (hevc->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hevc->timer);
		hevc->stat &= ~STAT_TIMER_ARM;
	}
	/* mark itself has all HW resource released and input released */
	vdec_set_status(hw_to_vdec(hevc), VDEC_STATUS_CONNECTED);

#ifdef CONFIG_AM_VDEC_DV
	if (hevc->switch_dvlayer_flag) {
		if (vdec->slave)
			vdec_set_next_sched(vdec, vdec->slave);
		else if (vdec->master)
			vdec_set_next_sched(vdec, vdec->master);
	} else if (vdec->slave || vdec->master)
		vdec_set_next_sched(vdec, vdec);
#endif

	if (hevc->vdec_cb)
		hevc->vdec_cb(hw_to_vdec(hevc), hevc->vdec_cb_arg);
}

static int vh265_hw_ctx_restore(struct hevc_state_s *hevc)
{
	/* new to do ... */
	vh265_prot_init(hevc);
	return 0;
}

static bool run_ready(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	/*hevc_print(hevc,
		PRINT_FLAG_VDEC_DETAIL, "%s\r\n", __func__);
	*/
	if (hevc->eos)
		return 0;

	return is_new_pic_available(hevc);
}

static void reset_dec_hw(struct vdec_s *vdec)
{
	if (input_frame_based(vdec))
		WRITE_VREG(HEVC_STREAM_CONTROL, 0);

		/*
	 * 2: assist
	 * 3: parser
	 * 4: parser_state
	 * 8: dblk
	 * 11:mcpu
	 * 12:ccpu
	 * 13:ddr
	 * 14:iqit
	 * 15:ipp
	 * 17:qdct
	 * 18:mpred
	 * 19:sao
	 * 24:hevc_afifo
	 */
	WRITE_VREG(DOS_SW_RESET3,
		(1<<3)|(1<<4)|(1<<8)|(1<<11)|(1<<12)|(1<<14)|(1<<15)|
		(1<<17)|(1<<18)|(1<<19));
	WRITE_VREG(DOS_SW_RESET3, 0);
}

static void run(struct vdec_s *vdec,
	void (*callback)(struct vdec_s *, void *), void *arg)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	int r;

	hevc->vdec_cb_arg = arg;
	hevc->vdec_cb = callback;

	reset_dec_hw(vdec);

	r = vdec_prepare_input(vdec, &hevc->chunk);
	if (r < 0) {
		hevc->dec_result = DEC_RESULT_AGAIN;

		hevc_print(hevc, PRINT_FLAG_VDEC_DETAIL,
			"ammvdec_vh265: Insufficient data\n");

		schedule_work(&hevc->work);
		return;
	}
	hevc->dec_result = DEC_RESULT_NONE;

	hevc_print(hevc, PRINT_FLAG_VDEC_STATUS,
		"%s: size 0x%x sum 0x%x (%x %x %x %x %x) byte count %x\n",
		__func__, r,
		(vdec_frame_based(vdec) &&
		(get_dbg_flag(hevc) & PRINT_FLAG_VDEC_STATUS)) ?
		get_data_check_sum(hevc, r) : 0,
		READ_VREG(HEVC_STREAM_LEVEL),
		READ_VREG(HEVC_STREAM_WR_PTR),
		READ_VREG(HEVC_STREAM_RD_PTR),
		READ_MPEG_REG(PARSER_VIDEO_RP),
		READ_MPEG_REG(PARSER_VIDEO_WP),
		READ_VREG(HEVC_SHIFT_BYTE_COUNT)
		);
	if ((get_dbg_flag(hevc) & PRINT_FRAMEBASE_DATA) &&
		input_frame_based(vdec)) {
		int jj;
		u8 *data = ((u8 *)hevc->chunk->block->start_virt) +
			hevc->chunk->offset;
		for (jj = 0; jj < r; jj++) {
			if ((jj & 0xf) == 0)
				hevc_print(hevc, PRINT_FRAMEBASE_DATA,
					"%06x:", jj);
			hevc_print_cont(hevc, PRINT_FRAMEBASE_DATA,
				"%02x ", data[jj]);
			if (((jj + 1) & 0xf) == 0)
				hevc_print_cont(hevc, PRINT_FRAMEBASE_DATA,
					"\n");
		}
	}
	if (hevc->init_flag == 0) {
		if (mmu_enable && (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)) {
			if (amhevc_vdec_loadmc_ex(vdec, "vh265_mc_mmu") < 0) {
				amhevc_disable();
				hevc_print(hevc, 0,
					"%s: Error amvdec_loadmc fail\n",
					__func__);
				return;
			}
		} else {
			if (amhevc_vdec_loadmc_ex
				(vdec, "vh265_mc") < 0) {
				amhevc_disable();
				hevc_print(hevc, 0,
					"%s: Error amvdec_loadmc fail\n",
					__func__);
				return;
			}
		}
	}
	if (vh265_hw_ctx_restore(hevc) < 0) {
		schedule_work(&hevc->work);
		return;
	}

	vdec_enable_input(vdec);

	WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);

	if (vdec_frame_based(vdec))
		WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, 0);

	WRITE_VREG(HEVC_DECODE_SIZE, r);
	/*WRITE_VREG(HEVC_DECODE_COUNT, hevc->decode_idx);*/
	hevc->init_flag = 1;

	if (hevc->pic_list_init_flag == 3)
		init_pic_list_hw(hevc);

	start_process_time(hevc);
	add_timer(&hevc->timer);
	hevc->stat |= STAT_TIMER_ARM;
	hevc->stat |= STAT_ISR_REG;
	amhevc_start();
	hevc->stat |= STAT_VDEC_RUN;
}

static void reset(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;

	hevc_print(hevc,
		PRINT_FLAG_VDEC_DETAIL, "%s\r\n", __func__);

}

static irqreturn_t vh265_irq_cb(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	return vh265_isr(0, hevc);
}

static irqreturn_t vh265_threaded_irq_cb(struct vdec_s *vdec)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)vdec->private;
	return vh265_isr_thread_fn(0, hevc);
}
#endif




static int amvdec_h265_probe(struct platform_device *pdev)
{
#ifdef MULTI_INSTANCE_SUPPORT
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
#else
	struct vdec_dev_reg_s *pdata =
		(struct vdec_dev_reg_s *)pdev->dev.platform_data;
#endif
	struct hevc_state_s *hevc = &gHevc;
	int ret;

	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0, "%s\r\n", __func__);
	mutex_lock(&vh265_mutex);

	if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) &&
		(parser_sei_enable & 0x100) == 0)
		parser_sei_enable = 7; /*old 1*/
	hevc->m_ins_flag = 0;
	hevc->init_flag = 0;
	hevc->uninit_list = 0;
	hevc->fatal_error = 0;
	hevc->show_frame_num = 0;
#ifdef MULTI_INSTANCE_SUPPORT
	hevc->platform_dev = pdev;
	platform_set_drvdata(pdev, pdata);
#endif

	if (pdata == NULL) {
		hevc_print(hevc, 0,
			"\namvdec_h265 memory resource undefined.\n");
		mutex_unlock(&vh265_mutex);
		return -EFAULT;
	}
	if (mmu_enable_force == 0) {
		if (get_cpu_type() < MESON_CPU_MAJOR_ID_GXL
			|| double_write_mode == 0x10)
			mmu_enable = 0;
		else
			mmu_enable = 1;
	}
	if (init_mmu_buffers(hevc)) {
		hevc_print(hevc, 0,
			"\n 265 mmu init failed!\n");
		mutex_unlock(&vh265_mutex);
		return -EFAULT;
	}
	/* alloc_mem_size is work space size */
	hevc->buf_size = pdata->alloc_mem_size;

	ret = decoder_bmmu_box_alloc_buf_phy(hevc->bmmu_box, BMMU_WORKSPACE_ID,
			hevc->buf_size, DRIVER_NAME, &hevc->buf_start);
	if (ret < 0) {
		uninit_mmu_buffers(hevc);
		devm_kfree(&pdev->dev, (void *)hevc);
		mutex_unlock(&vh265_mutex);
		return ret;
	}

	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0,
			"===H.265 decoder mem resource 0x%lx -- 0x%lx\n",
			hevc->buf_size, hevc->buf_size);
	}

	if (pdata->sys_info)
		hevc->vh265_amstream_dec_info = *pdata->sys_info;
	else {
		hevc->vh265_amstream_dec_info.width = 0;
		hevc->vh265_amstream_dec_info.height = 0;
		hevc->vh265_amstream_dec_info.rate = 30;
	}
#ifndef MULTI_INSTANCE_SUPPORT
	if (pdata->flag & DEC_FLAG_HEVC_WORKAROUND) {
		workaround_enable |= 3;
		hevc_print(hevc, 0,
			"amvdec_h265 HEVC_WORKAROUND flag set.\n");
	} else
		workaround_enable &= ~3;
#endif
	hevc->cma_dev = pdata->cma_dev;
	vh265_vdec_info_init();

#ifdef MULTI_INSTANCE_SUPPORT
	pdata->private = hevc;
	pdata->dec_status = vh265_dec_status;
	if (vh265_init(pdata) < 0) {
#else
	if (vh265_init(hevc) < 0) {
#endif
		hevc_print(hevc, 0,
			"\namvdec_h265 init failed.\n");
		hevc_local_uninit(hevc);
		uninit_mmu_buffers(hevc);
		mutex_unlock(&vh265_mutex);
		return -ENODEV;
	}
	/*set the max clk for smooth playing...*/
	hevc_source_changed(VFORMAT_HEVC,
			3840, 2160, 60);
	mutex_unlock(&vh265_mutex);

	return 0;
}

static int amvdec_h265_remove(struct platform_device *pdev)
{
	struct hevc_state_s *hevc = &gHevc;

	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0, "%s\r\n", __func__);

	mutex_lock(&vh265_mutex);

	vh265_stop(hevc);

	hevc_source_changed(VFORMAT_HEVC, 0, 0, 0);


#ifdef DEBUG_PTS
	hevc_print(hevc, 0,
		"pts missed %ld, pts hit %ld, duration %d\n",
		hevc->pts_missed, hevc->pts_hit, hevc->frame_dur);
#endif

	mutex_unlock(&vh265_mutex);

	return 0;
}
/****************************************/

static struct platform_driver amvdec_h265_driver = {
	.probe = amvdec_h265_probe,
	.remove = amvdec_h265_remove,
#ifdef CONFIG_PM
	.suspend = amhevc_suspend,
	.resume = amhevc_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

#ifdef MULTI_INSTANCE_SUPPORT
static int ammvdec_h265_probe(struct platform_device *pdev)
{

	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct hevc_state_s *hevc = NULL;
	int ret;
#ifdef CONFIG_MULTI_DEC
	int config_val;
#endif
	if (pdata == NULL) {
		pr_info("\nammvdec_h265 memory resource undefined.\n");
		return -EFAULT;
	}

	hevc = (struct hevc_state_s *)devm_kzalloc(&pdev->dev,
		sizeof(struct hevc_state_s), GFP_KERNEL);
	if (hevc == NULL) {
		pr_info("\nammvdec_h265 device data allocation failed\n");
		return -ENOMEM;
	}
	pdata->private = hevc;
	pdata->dec_status = vh265_dec_status;
	/* pdata->set_trickmode = set_trickmode; */
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vh265_irq_cb;
	pdata->threaded_irq_handler = vh265_threaded_irq_cb;

	pdata->id = pdev->id;
	hevc->index = pdev->id;

	if (pdata->use_vfm_path)
		snprintf(pdata->vf_provider_name,
		VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
#ifdef CONFIG_AM_VDEC_DV
	else if (vdec_dual(pdata)) {
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
	}
#endif
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			MULTI_INSTANCE_PROVIDER_NAME ".%02x", pdev->id & 0xff);

	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vh265_vf_provider, pdata);

	hevc->provider_name = pdata->vf_provider_name;
	platform_set_drvdata(pdev, pdata);

	hevc->platform_dev = pdev;

	if (((get_dbg_flag(hevc) & IGNORE_PARAM_FROM_CONFIG) == 0) &&
			pdata->config && pdata->config_len) {
#ifdef CONFIG_MULTI_DEC
		/*use ptr config for doubel_write_mode, etc*/
		hevc_print(hevc, 0, "pdata->config=%s\n", pdata->config);
		if (get_config_int(pdata->config, "hevc_buf_width",
				&config_val) == 0)
			hevc->buf_alloc_width = config_val;
		else
			hevc->buf_alloc_width = buf_alloc_width;

		if (get_config_int(pdata->config, "hevc_buf_height",
				&config_val) == 0)
			hevc->buf_alloc_height = config_val;
		else
			hevc->buf_alloc_height = buf_alloc_height;

		if (get_config_int(pdata->config, "hevc_buf_margin",
				&config_val) == 0)
			hevc->dynamic_buf_num_margin = config_val;
		else
			hevc->dynamic_buf_num_margin = dynamic_buf_num_margin;

		if (get_config_int(pdata->config, "hevc_double_write_mode",
				&config_val) == 0)
			hevc->double_write_mode = config_val;
		else
			hevc->double_write_mode = double_write_mode;
#endif
	} else {
		hevc->vh265_amstream_dec_info.width = 0;
		hevc->vh265_amstream_dec_info.height = 0;
		hevc->vh265_amstream_dec_info.rate = 30;
		hevc->buf_alloc_width = buf_alloc_width;
		hevc->buf_alloc_height = buf_alloc_height;
		hevc->dynamic_buf_num_margin = dynamic_buf_num_margin;
		hevc->double_write_mode = double_write_mode;
	}

	if (mmu_enable_force == 0) {
		if (get_cpu_type() < MESON_CPU_MAJOR_ID_GXL
			|| hevc->double_write_mode == 0x10)
			mmu_enable = 0;
		else
			mmu_enable = 1;
	}

	if (init_mmu_buffers(hevc) < 0) {
		hevc_print(hevc, 0,
			"\n 265 mmu init failed!\n");
		mutex_unlock(&vh265_mutex);
		devm_kfree(&pdev->dev, (void *)hevc);
		return -EFAULT;
	}
#if 0
	hevc->buf_start = pdata->mem_start;
	hevc->buf_size = pdata->mem_end - pdata->mem_start + 1;
#else

	ret = decoder_bmmu_box_alloc_buf_phy(hevc->bmmu_box,
			BMMU_WORKSPACE_ID, work_buf_size,
			DRIVER_NAME, &hevc->buf_start);
	if (ret < 0) {
		uninit_mmu_buffers(hevc);
		devm_kfree(&pdev->dev, (void *)hevc);
		mutex_unlock(&vh265_mutex);
		return ret;
	}
	hevc->buf_size = work_buf_size;
#endif
	if ((get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB) &&
		(parser_sei_enable & 0x100) == 0)
		parser_sei_enable = 7;
	hevc->m_ins_flag = 1;
	hevc->init_flag = 0;
	hevc->uninit_list = 0;
	hevc->fatal_error = 0;
	hevc->show_frame_num = 0;
	if (pdata == NULL) {
		hevc_print(hevc, 0,
			"\namvdec_h265 memory resource undefined.\n");
		uninit_mmu_buffers(hevc);
		devm_kfree(&pdev->dev, (void *)hevc);
		return -EFAULT;
	}
	/*
	hevc->mc_buf_spec.buf_end = pdata->mem_end + 1;
	for (i = 0; i < WORK_BUF_SPEC_NUM; i++)
		amvh265_workbuff_spec[i].start_adr = pdata->mem_start;
	*/
	if (get_dbg_flag(hevc)) {
		hevc_print(hevc, 0,
			"===H.265 decoder mem resource 0x%lx -- 0x%lx\n",
			   hevc->buf_start, hevc->buf_start + hevc->buf_size);
	}

	hevc_print(hevc, 0,
		"buf_alloc_width=%d\n",
		hevc->buf_alloc_width);
	hevc_print(hevc, 0,
		"buf_alloc_height=%d\n",
		hevc->buf_alloc_height);
	hevc_print(hevc, 0,
		"dynamic_buf_num_margin=%d\n",
		hevc->dynamic_buf_num_margin);
	hevc_print(hevc, 0,
		"double_write_mode=%d\n",
		hevc->double_write_mode);

	hevc->cma_dev = pdata->cma_dev;

	if (vh265_init(pdata) < 0) {
		hevc_print(hevc, 0,
			"\namvdec_h265 init failed.\n");
		hevc_local_uninit(hevc);
		uninit_mmu_buffers(hevc);
		devm_kfree(&pdev->dev, (void *)hevc);
		return -ENODEV;
	}

	vdec_set_prepare_level(pdata, start_decode_buf_level);

	/*set the max clk for smooth playing...*/
	hevc_source_changed(VFORMAT_HEVC,
			3840, 2160, 60);

	return 0;
}

static int ammvdec_h265_remove(struct platform_device *pdev)
{
	struct hevc_state_s *hevc =
		(struct hevc_state_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);

	if (get_dbg_flag(hevc))
		hevc_print(hevc, 0, "%s\r\n", __func__);

	vmh265_stop(hevc);

	/* vdec_source_changed(VFORMAT_H264, 0, 0, 0); */

	vdec_set_status(hw_to_vdec(hevc), VDEC_STATUS_DISCONNECTED);

	return 0;
}

static struct platform_driver ammvdec_h265_driver = {
	.probe = ammvdec_h265_probe,
	.remove = ammvdec_h265_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = MULTI_DRIVER_NAME,
	}
};
#endif

static struct codec_profile_t amvdec_h265_profile = {
	.name = "hevc",
	.profile = ""
};

static int __init amvdec_h265_driver_init_module(void)
{
	pr_debug("amvdec_h265 module init\n");
	error_handle_policy = 0;

#ifdef ERROR_HANDLE_DEBUG
	dbg_nal_skip_flag = 0;
	dbg_nal_skip_count = 0;
#endif
	decode_stop_pos = 0;
	decode_stop_pos_pre = 0;
	decode_pic_begin = 0;
	slice_parse_begin = 0;
	step = 0;
	buf_alloc_size = 0;

#ifdef MULTI_INSTANCE_SUPPORT
	if (platform_driver_register(&ammvdec_h265_driver))
		pr_err("failed to register ammvdec_h265 driver\n");

#endif
	if (platform_driver_register(&amvdec_h265_driver)) {
		pr_err("failed to register amvdec_h265 driver\n");
		return -ENODEV;
	}
#if 1/*MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8*/
	if (!has_hevc_vdec()) {
		/* not support hevc */
		amvdec_h265_profile.name = "hevc_unsupport";
	} else if (is_meson_m8m2_cpu()) {
		/* m8m2 support 4k */
		amvdec_h265_profile.profile = "4k";
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB) {
		amvdec_h265_profile.profile =
			"4k, 9bit, 10bit, dwrite, compressed";
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_MG9TV)
		amvdec_h265_profile.profile = "4k";
#endif
	if (codec_mm_get_total_size() < 80 * SZ_1M) {
		pr_info("amvdec_h265 default mmu enabled.\n");
		mmu_enable = 1;
	}

	vcodec_profile_register(&amvdec_h265_profile);

	return 0;
}

static void __exit amvdec_h265_driver_remove_module(void)
{
	pr_debug("amvdec_h265 module remove.\n");

#ifdef MULTI_INSTANCE_SUPPORT
	platform_driver_unregister(&ammvdec_h265_driver);
#endif
	platform_driver_unregister(&amvdec_h265_driver);
}

/****************************************/
/*
module_param(stat, uint, 0664);
MODULE_PARM_DESC(stat, "\n amvdec_h265 stat\n");
*/
module_param(use_cma, uint, 0664);
MODULE_PARM_DESC(use_cma, "\n amvdec_h265 use_cma\n");

module_param(bit_depth_luma, uint, 0664);
MODULE_PARM_DESC(bit_depth_luma, "\n amvdec_h265 bit_depth_luma\n");

module_param(bit_depth_chroma, uint, 0664);
MODULE_PARM_DESC(bit_depth_chroma, "\n amvdec_h265 bit_depth_chroma\n");

module_param(video_signal_type, uint, 0664);
MODULE_PARM_DESC(video_signal_type, "\n amvdec_h265 video_signal_type\n");

#ifdef ERROR_HANDLE_DEBUG
module_param(dbg_nal_skip_flag, uint, 0664);
MODULE_PARM_DESC(dbg_nal_skip_flag, "\n amvdec_h265 dbg_nal_skip_flag\n");

module_param(dbg_nal_skip_count, uint, 0664);
MODULE_PARM_DESC(dbg_nal_skip_count, "\n amvdec_h265 dbg_nal_skip_count\n");
#endif

module_param(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\nradr\n");

module_param(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\nrval\n");

module_param(dbg_cmd, uint, 0664);
MODULE_PARM_DESC(dbg_cmd, "\dbg_cmd\n");

module_param(dump_nal, uint, 0664);
MODULE_PARM_DESC(dump_nal, "\dump_nal\n");

module_param(dbg_skip_decode_index, uint, 0664);
MODULE_PARM_DESC(dbg_skip_decode_index, "\dbg_skip_decode_index\n");

module_param(endian, uint, 0664);
MODULE_PARM_DESC(endian, "\nrval\n");

module_param(step, uint, 0664);
MODULE_PARM_DESC(step, "\n amvdec_h265 step\n");

module_param(decode_stop_pos, uint, 0664);
MODULE_PARM_DESC(decode_stop_pos, "\n amvdec_h265 decode_stop_pos\n");

module_param(decode_pic_begin, uint, 0664);
MODULE_PARM_DESC(decode_pic_begin, "\n amvdec_h265 decode_pic_begin\n");

module_param(slice_parse_begin, uint, 0664);
MODULE_PARM_DESC(slice_parse_begin, "\n amvdec_h265 slice_parse_begin\n");

module_param(nal_skip_policy, uint, 0664);
MODULE_PARM_DESC(nal_skip_policy, "\n amvdec_h265 nal_skip_policy\n");

module_param(i_only_flag, uint, 0664);
MODULE_PARM_DESC(i_only_flag, "\n amvdec_h265 i_only_flag\n");

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

module_param(debug, uint, 0664);
MODULE_PARM_DESC(debug, "\n amvdec_h265 debug\n");

module_param(debug_mask, uint, 0664);
MODULE_PARM_DESC(debug_mask, "\n amvdec_h265 debug mask\n");

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

#ifdef CONFIG_AM_VDEC_DV
module_param(parser_dolby_vision_enable, uint, 0664);
MODULE_PARM_DESC(parser_dolby_vision_enable,
	"\n parser_dolby_vision_enable\n");

module_param(dolby_meta_with_el, uint, 0664);
MODULE_PARM_DESC(dolby_meta_with_el,
	"\n dolby_meta_with_el\n");
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

module_param_array(decode_frame_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(max_process_time, uint,
	&max_decode_instance_num, 0664);

module_param_array(max_get_frame_interval,
	uint, &max_decode_instance_num, 0664);

#endif
#ifdef CONFIG_AM_VDEC_DV
module_param(dv_toggle_prov_name, uint, 0664);
MODULE_PARM_DESC(dv_toggle_prov_name, "\n dv_toggle_prov_name\n");

module_param(dv_debug, uint, 0664);
MODULE_PARM_DESC(dv_debug, "\n dv_debug\n");
#endif
module_init(amvdec_h265_driver_init_module);
module_exit(amvdec_h265_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC h265 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <tim.yao@amlogic.com>");
