 /*
 * drivers/amlogic/amports/vvp9.c
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
#include <linux/spinlock.h>
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

#define MEM_NAME "codec_vp9"
/* #include <mach/am_regs.h> */
#include "vdec_reg.h"
#include "video.h"

#include "vdec.h"
#include "amvdec.h"

#define MIX_STREAM_SUPPORT
#define SUPPORT_4K2K

#include "vvp9.h"
#define VP9D_MPP_REFINFO_TBL_ACCCONFIG             0x3442
#define VP9D_MPP_REFINFO_DATA                      0x3443
#define VP9D_MPP_REF_SCALE_ENBL                    0x3441
#define HEVC_MPRED_CTRL4                           0x324c
#define HEVC_CM_HEADER_START_ADDR                  0x3628
#define HEVC_DBLK_CFGB                             0x350b
#define HEVCD_MPP_ANC2AXI_TBL_DATA                 0x3464
#define HEVC_SAO_MMU_VH1_ADDR                      0x363b
#define HEVC_SAO_MMU_VH0_ADDR                      0x363a
#define HEVC_SAO_MMU_STATUS                        0x3639


#define VP9_10B_DEC_IDLE                           0
#define VP9_10B_DEC_FRAME_HEADER                   1
#define VP9_10B_DEC_SLICE_SEGMENT                  2
#define VP9_10B_DECODE_SLICE                     5
#define VP9_10B_DISCARD_NAL                  6
#define VP9_DUMP_LMEM                7
#define HEVC_DECPIC_DATA_DONE            0xa
#define HEVC_DECPIC_DATA_ERROR            0xb
#define HEVC_NAL_DECODE_DONE            0xe
#define HEVC_DECODE_BUFEMPTY        0x20
#define HEVC_DECODE_TIMEOUT         0x21
#define HEVC_SEARCH_BUFEMPTY        0x22
#define VP9_HEAD_PARSER_DONE            0xf0
#define VP9_HEAD_SEARCH_DONE          0xf1
#define VP9_EOS                        0xf2
#define HEVC_ACTION_DONE                0xff

#define MAX_BUF_NUM 24
#define MAX_REF_ACTIVE  16
#define VF_POOL_SIZE        32

#undef pr_info
#define pr_info printk

#define DECODE_MODE_SINGLE				0
#define DECODE_MODE_MULTI_STREAMBASE	1
#define DECODE_MODE_MULTI_FRAMEBASE		2

/*---------------------------------------------------
 Include "parser_cmd.h"
---------------------------------------------------*/
#define PARSER_CMD_SKIP_CFG_0 0x0000090b

#define PARSER_CMD_SKIP_CFG_1 0x1b14140f

#define PARSER_CMD_SKIP_CFG_2 0x001b1910

#define PARSER_CMD_NUMBER 37

unsigned short parser_cmd[PARSER_CMD_NUMBER] = {
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
/*#define HEVC_PIC_STRUCT_SUPPORT*/
/* to remove, fix build error */

/*#define CODEC_MM_FLAGS_FOR_VDECODER  0*/

#define MULTI_INSTANCE_SUPPORT
#define SUPPORT_10BIT
/* #define ERROR_HANDLE_DEBUG */
#if 0 /* MESON_CPU_TYPE == MESON_CPU_TYPE_MESON8B*/
#undef SUPPORT_4K2K
#else
#define SUPPORT_4K2K
#endif

#ifndef STAT_KTHREAD
#define STAT_KTHREAD 0x40
#endif

#ifdef MULTI_INSTANCE_SUPPORT
#define MAX_DECODE_INSTANCE_NUM     5
#define MULTI_DRIVER_NAME "ammvdec_vp9"
#endif

#define DRIVER_NAME "amvdec_vp9"
#define MODULE_NAME "amvdec_vp9"

#define PUT_INTERVAL        (HZ/100)
#define ERROR_SYSTEM_RESET_COUNT   200

#define PTS_NORMAL                0
#define PTS_NONE_REF_USE_DURATION 1

#define PTS_MODE_SWITCHING_THRESHOLD           3
#define PTS_MODE_SWITCHING_RECOVERY_THREASHOLD 3

#define DUR2PTS(x) ((x)*90/96)

struct VP9Decoder_s;
static int vvp9_vf_states(struct vframe_states *states, void *);
static struct vframe_s *vvp9_vf_peek(void *);
static struct vframe_s *vvp9_vf_get(void *);
static void vvp9_vf_put(struct vframe_s *, void *);
static int vvp9_event_cb(int type, void *data, void *private_data);

static int vvp9_stop(struct VP9Decoder_s *pbi);
static s32 vvp9_init(struct VP9Decoder_s *pbi);
static void vvp9_prot_init(struct VP9Decoder_s *pbi);
static int vvp9_local_init(struct VP9Decoder_s *pbi);
static void vvp9_put_timer_func(unsigned long arg);
#ifdef VP9_10B_MMU
static int vp9_alloc_mmu(
		struct VP9Decoder_s *pbi,
		int cur_buf_idx,
		int pic_width,
		int pic_height,
		unsigned short bit_depth,
		unsigned int *mmu_index_adr);
#endif

static const char vvp9_dec_id[] = "vvp9-dev";

#define PROVIDER_NAME   "decoder.vp9"
#define MULTI_INSTANCE_PROVIDER_NAME    "vdec.vp9"

static const struct vframe_operations_s vvp9_vf_provider = {
	.peek = vvp9_vf_peek,
	.get = vvp9_vf_get,
	.put = vvp9_vf_put,
	.event_cb = vvp9_event_cb,
	.vf_states = vvp9_vf_states,
};

static struct vframe_provider_s vvp9_vf_prov;

static u32 bit_depth_luma;
static u32 bit_depth_chroma;
static u32 frame_width;
static u32 frame_height;
static u32 video_signal_type;
static u32 pts_unstable;
static u32 on_no_keyframe_skiped;


#define PROB_SIZE    (496 * 2 * 4)
#define PROB_BUF_SIZE    (0x5000)
#define COUNT_BUF_SIZE   (0x300 * 4 * 4)
/*compute_losless_comp_body_size(4096, 2304, 1) = 18874368(0x1200000)*/
#define MAX_FRAME_4K_NUM 0x1200
#define FRAME_MMU_MAP_SIZE  (MAX_FRAME_4K_NUM * 4)

static inline int div_r32(int64_t m, int n)
{
/*
return (int)(m/n)
*/
#ifndef CONFIG_ARM64
	do_div(m, n);
	return (int)m;
#else
	return (int)(m/n);
#endif
}


/*USE_BUF_BLOCK*/
struct BUF_s {
	int index;
	unsigned int alloc_flag;
	/*buffer */
	unsigned int cma_page_count;
	unsigned long alloc_addr;
	unsigned long start_adr;
	unsigned int size;

	unsigned int free_start_adr;
} /*BUF_t */;


	/* #undef BUFMGR_ONLY to enable hardware configuration */

/*#define TEST_WR_PTR_INC*/
#define WR_PTR_INC_NUM 128

#define SIMULATION
#define DOS_PROJECT
#undef MEMORY_MAP_IN_REAL_CHIP

/*#undef DOS_PROJECT*/
/*#define MEMORY_MAP_IN_REAL_CHIP*/

/*#define BUFFER_MGR_ONLY*/
/*#define CONFIG_HEVC_CLK_FORCED_ON*/
/*#define ENABLE_SWAP_TEST*/
#define   MCRCC_ENABLE

#ifdef VP9_10B_NV21
#define MEM_MAP_MODE 2  /* 0:linear 1:32x32 2:64x32*/
#else
#define MEM_MAP_MODE 0  /* 0:linear 1:32x32 2:64x32*/
#endif

#define VP9_LPF_LVL_UPDATE
/*#define DBG_LF_PRINT*/

#ifdef VP9_10B_NV21
#else
#define LOSLESS_COMPRESS_MODE
#endif

#define DOUBLE_WRITE_YSTART_TEMP 0x02000000
#define DOUBLE_WRITE_CSTART_TEMP 0x02900000



typedef unsigned int u32;
typedef unsigned short u16;

#define VP9_DEBUG_BUFMGR                   0x01
#define VP9_DEBUG_BUFMGR_MORE              0x02
#define VP9_DEBUG_UCODE                    0x04
#define VP9_DEBUG_REG                      0x08
#define VP9_DEBUG_MERGE			0x10
#define VP9_DEBUG_BASIC				        0x40
#define VP9_DEBUG_SEND_PARAM_WITH_REG      0x100
#define VP9_DEBUG_NO_DISPLAY               0x200
#define VP9_DEBUG_DBG_LF_PRINT             0x400
#define VP9_DEBUG_OUT_PTS                  0x800
#define VP9_DEBUG_VF_REF				   0x1000
#define VP9_DEBUG_DIS_LOC_ERROR_PROC       0x10000
#define VP9_DEBUG_DIS_SYS_ERROR_PROC   0x20000
#define VP9_DEBUG_DUMP_PIC_LIST       0x40000
#define VP9_DEBUG_TRIG_SLICE_SEGMENT_PROC 0x80000
#define VP9_DEBUG_HW_RESET               0x100000
#define VP9_DEBUG_LOAD_UCODE_FROM_FILE   0x200000
#define VP9_DEBUG_ERROR_TRIG             0x400000
#define VP9_DEBUG_NOWAIT_DECODE_DONE_WHEN_STOP   0x4000000
#ifdef MULTI_INSTANCE_SUPPORT
#define PRINT_FLAG_ERROR				0
#define PRINT_FLAG_UCODE_EVT             0x10000000
#define PRINT_FLAG_VDEC_STATUS             0x20000000
#define PRINT_FLAG_VDEC_DETAIL             0x40000000
#define PRINT_FLAG_VDEC_DATA             0x80000000
#endif

static u32 debug;

#define DEBUG_REG
#ifdef DEBUG_REG
void WRITE_VREG_DBG2(unsigned adr, unsigned val)
{
	if (debug & VP9_DEBUG_REG)
		pr_info("%s(%x, %x)\n", __func__, adr, val);
	if (adr != 0)
		WRITE_VREG(adr, val);
}

#undef WRITE_VREG
#define WRITE_VREG WRITE_VREG_DBG2
#endif

static struct vdec_info *gvs;

/**************************************************

VP9 buffer management start

***************************************************/
#ifdef VP9_10B_MMU
#define MMU_COMPRESS_HEADER_SIZE  0x48000
#endif

#define INVALID_IDX -1  /* Invalid buffer index.*/

#define RPM_BEGIN                                              0x200
#define RPM_END                                                0x280

union param_u {
	struct {
		unsigned short data[RPM_END - RPM_BEGIN];
	} l;
	struct {
		/* from ucode lmem, do not change this struct */
		unsigned short profile;
		unsigned short show_existing_frame;
		unsigned short frame_to_show_idx;
		unsigned short frame_type; /*1 bit*/
		unsigned short show_frame; /*1 bit*/
		unsigned short error_resilient_mode; /*1 bit*/
		unsigned short intra_only; /*1 bit*/
		unsigned short display_size_present; /*1 bit*/
		unsigned short reset_frame_context;
		unsigned short refresh_frame_flags;
		unsigned short width;
		unsigned short height;
		unsigned short display_width;
		unsigned short display_height;
	/*
	bit[11:8] - ref_frame_info_0 (ref(3-bits), ref_frame_sign_bias(1-bit))
	bit[7:4]  - ref_frame_info_1 (ref(3-bits), ref_frame_sign_bias(1-bit))
	bit[3:0]  - ref_frame_info_2 (ref(3-bits), ref_frame_sign_bias(1-bit))
	*/
		unsigned short ref_info;
		/*
		bit[2]: same_frame_size0
		bit[1]: same_frame_size1
		bit[0]: same_frame_size2
		*/
		unsigned short same_frame_size;

		unsigned short mode_ref_delta_enabled;
		unsigned short ref_deltas[4];
		unsigned short mode_deltas[2];
		unsigned short filter_level;
		unsigned short sharpness_level;
		unsigned short bit_depth;
		unsigned short seg_quant_info[8];
		unsigned short seg_enabled;
		unsigned short seg_abs_delta;
		/* bit 15: feature enabled; bit 8, sign; bit[5:0], data */
		unsigned short seg_lf_info[8];
	} p;
};


struct vpx_codec_frame_buffer_s {
	uint8_t *data;  /**< Pointer to the data buffer */
	size_t size;  /**< Size of data in bytes */
	void *priv;  /**< Frame's private data */
};

enum vpx_color_space_t {
	VPX_CS_UNKNOWN    = 0,  /**< Unknown */
	VPX_CS_BT_601     = 1,  /**< BT.601 */
	VPX_CS_BT_709     = 2,  /**< BT.709 */
	VPX_CS_SMPTE_170  = 3,  /**< SMPTE.170 */
	VPX_CS_SMPTE_240  = 4,  /**< SMPTE.240 */
	VPX_CS_BT_2020    = 5,  /**< BT.2020 */
	VPX_CS_RESERVED   = 6,  /**< Reserved */
	VPX_CS_SRGB       = 7   /**< sRGB */
}; /**< alias for enum vpx_color_space */

enum vpx_bit_depth_t {
	VPX_BITS_8  =  8,  /**<  8 bits */
	VPX_BITS_10 = 10,  /**< 10 bits */
	VPX_BITS_12 = 12,  /**< 12 bits */
};

#define MAX_SLICE_NUM 1024
struct PIC_BUFFER_CONFIG_s {
	int index;
	int BUF_index;
	int comp_body_size;
	int buf_size;
	int vf_ref;
	int y_canvas_index;
	int uv_canvas_index;
#ifdef MULTI_INSTANCE_SUPPORT
	struct canvas_config_s canvas_config[2];
#endif
	int decode_idx;
	int slice_type;
	int num_reorder_pic;
	int stream_offset;
	uint8_t used_by_display;
	uint8_t referenced;
	uint8_t output_mark;
	uint8_t recon_mark;
	uint8_t output_ready;
	uint8_t error_mark;
	/**/
	int slice_idx;
	/*buffer*/
#ifdef VP9_10B_MMU
	unsigned long header_adr;
#endif
	unsigned long mpred_mv_wr_start_addr;
	unsigned long mc_y_adr;
	unsigned long mc_u_v_adr;
	unsigned int dw_y_adr;
	unsigned int dw_u_v_adr;
	int mc_canvas_y;
	int mc_canvas_u_v;

	int lcu_total;
	/**/
	int   y_width;
	int   y_height;
	int   y_crop_width;
	int   y_crop_height;
	int   y_stride;

	int   uv_width;
	int   uv_height;
	int   uv_crop_width;
	int   uv_crop_height;
	int   uv_stride;

	int   alpha_width;
	int   alpha_height;
	int   alpha_stride;

	uint8_t *y_buffer;
	uint8_t *u_buffer;
	uint8_t *v_buffer;
	uint8_t *alpha_buffer;

	uint8_t *buffer_alloc;
	int buffer_alloc_sz;
	int border;
	int frame_size;
	int subsampling_x;
	int subsampling_y;
	unsigned int bit_depth;
	enum vpx_color_space_t color_space;

	int corrupted;
	int flags;
	unsigned long cma_alloc_addr;
} PIC_BUFFER_CONFIG;

enum BITSTREAM_PROFILE {
	PROFILE_0,
	PROFILE_1,
	PROFILE_2,
	PROFILE_3,
	MAX_PROFILES
};

enum FRAME_TYPE {
	KEY_FRAME = 0,
	INTER_FRAME = 1,
	FRAME_TYPES,
};

enum REFERENCE_MODE {
	SINGLE_REFERENCE      = 0,
	COMPOUND_REFERENCE    = 1,
	REFERENCE_MODE_SELECT = 2,
	REFERENCE_MODES       = 3,
};

#define NONE           -1
#define INTRA_FRAME     0
#define LAST_FRAME      1
#define GOLDEN_FRAME    2
#define ALTREF_FRAME    3
#define MAX_REF_FRAMES  4

#define REFS_PER_FRAME 3

#define REF_FRAMES_LOG2 3
#define REF_FRAMES (1 << REF_FRAMES_LOG2)

/*4 scratch frames for the new frames to support a maximum of 4 cores decoding
in parallel, 3 for scaled references on the encoder.
TODO(hkuang): Add ondemand frame buffers instead of hardcoding the number
// of framebuffers.
TODO(jkoleszar): These 3 extra references could probably come from the
normal reference pool.*/
#define FRAME_BUFFERS (REF_FRAMES + 7)

#define FRAME_CONTEXTS_LOG2 2
#define FRAME_CONTEXTS (1 << FRAME_CONTEXTS_LOG2)
#define MAX_BMMU_BUFFER_NUM (FRAME_BUFFERS + 1)
#define WORK_SPACE_BUF_ID (FRAME_BUFFERS)

struct RefCntBuffer_s {
	int ref_count;
	/*MV_REF *mvs;*/
	int mi_rows;
	int mi_cols;
	struct vpx_codec_frame_buffer_s raw_frame_buffer;
	struct PIC_BUFFER_CONFIG_s buf;

/*The Following variables will only be used in frame parallel decode.

frame_worker_owner indicates which FrameWorker owns this buffer. NULL means
that no FrameWorker owns, or is decoding, this buffer.
VP9Worker *frame_worker_owner;

row and col indicate which position frame has been decoded to in real
pixel unit. They are reset to -1 when decoding begins and set to INT_MAX
when the frame is fully decoded.*/
	int row;
	int col;
} RefCntBuffer;

struct RefBuffer_s {
/*TODO(dkovalev): idx is not really required and should be removed, now it
is used in vp9_onyxd_if.c*/
	int idx;
	struct PIC_BUFFER_CONFIG_s *buf;
	/*struct scale_factors sf;*/
} RefBuffer;

struct InternalFrameBuffer_s {
	uint8_t *data;
	size_t size;
	int in_use;
} InternalFrameBuffer;

struct InternalFrameBufferList_s {
	int num_internal_frame_buffers;
	struct InternalFrameBuffer_s *int_fb;
} InternalFrameBufferList;

struct BufferPool_s {
/*Protect BufferPool from being accessed by several FrameWorkers at
the same time during frame parallel decode.
TODO(hkuang): Try to use atomic variable instead of locking the whole pool.

Private data associated with the frame buffer callbacks.
void *cb_priv;

vpx_get_frame_buffer_cb_fn_t get_fb_cb;
vpx_release_frame_buffer_cb_fn_t release_fb_cb;*/

	struct RefCntBuffer_s frame_bufs[FRAME_BUFFERS];

/*Frame buffers allocated internally by the codec.*/
	struct InternalFrameBufferList_s int_frame_buffers;
	unsigned long flags;
	spinlock_t lock;

} BufferPool;

static void lock_buffer_pool(struct BufferPool_s *pool)
{
	spin_lock_irqsave(&pool->lock, pool->flags);
}
static void unlock_buffer_pool(struct BufferPool_s *pool)
{
	spin_unlock_irqrestore(&pool->lock, pool->flags);
}

struct VP9_Common_s {
	enum vpx_color_space_t color_space;
	int width;
	int height;
	int display_width;
	int display_height;
	int last_width;
	int last_height;

	int subsampling_x;
	int subsampling_y;

	int use_highbitdepth;/*Marks if we need to use 16bit frame buffers.*/

	struct PIC_BUFFER_CONFIG_s *frame_to_show;
	struct RefCntBuffer_s *prev_frame;

	/*TODO(hkuang): Combine this with cur_buf in macroblockd.*/
	struct RefCntBuffer_s *cur_frame;

	int ref_frame_map[REF_FRAMES]; /* maps fb_idx to reference slot */

	/*Prepare ref_frame_map for the next frame.
	Only used in frame parallel decode.*/
	int next_ref_frame_map[REF_FRAMES];

	/* TODO(jkoleszar): could expand active_ref_idx to 4,
	with 0 as intra, and roll new_fb_idx into it.*/

	/*Each frame can reference REFS_PER_FRAME buffers*/
	struct RefBuffer_s frame_refs[REFS_PER_FRAME];

	int prev_fb_idx;
	int new_fb_idx;

	/*last frame's frame type for motion search*/
	enum FRAME_TYPE last_frame_type;
	enum FRAME_TYPE frame_type;

	int show_frame;
	int last_show_frame;
	int show_existing_frame;

	/*Flag signaling that the frame is encoded using only INTRA modes.*/
	uint8_t intra_only;
	uint8_t last_intra_only;

	int allow_high_precision_mv;

	/*Flag signaling that the frame context should be reset to default
	values. 0 or 1 implies don't reset, 2 reset just the context
	specified in the  frame header, 3 reset all contexts.*/
	int reset_frame_context;

	/*MBs, mb_rows/cols is in 16-pixel units; mi_rows/cols is in
		MODE_INFO (8-pixel) units.*/
	int MBs;
	int mb_rows, mi_rows;
	int mb_cols, mi_cols;
	int mi_stride;

	/*Whether to use previous frame's motion vectors for prediction.*/
	int use_prev_frame_mvs;

	int refresh_frame_context;    /* Two state 0 = NO, 1 = YES */

	int ref_frame_sign_bias[MAX_REF_FRAMES];    /* Two state 0, 1 */

	/*struct loopfilter lf;*/
	/*struct segmentation seg;*/

	/*TODO(hkuang):Remove this as it is the same as frame_parallel_decode*/
	/* in pbi.*/
	int frame_parallel_decode;  /* frame-based threading.*/

	/*Context probabilities for reference frame prediction*/
	/*MV_REFERENCE_FRAME comp_fixed_ref;*/
	/*MV_REFERENCE_FRAME comp_var_ref[2];*/
	enum REFERENCE_MODE reference_mode;

	/*FRAME_CONTEXT *fc; */ /* this frame entropy */
	/*FRAME_CONTEXT *frame_contexts; */  /*FRAME_CONTEXTS*/
	/*unsigned int  frame_context_idx; *//* Context to use/update */
	/*FRAME_COUNTS counts;*/

	unsigned int current_video_frame;
	enum BITSTREAM_PROFILE profile;

	enum vpx_bit_depth_t bit_depth;

	int error_resilient_mode;
	int frame_parallel_decoding_mode;

	int byte_alignment;
	int skip_loop_filter;

	/*External BufferPool passed from outside.*/
	struct BufferPool_s *buffer_pool;

	int above_context_alloc_cols;

} VP9_COMMON;

static void set_canvas(struct PIC_BUFFER_CONFIG_s *pic_config);
static int prepare_display_buf(struct VP9Decoder_s *pbi,
					struct PIC_BUFFER_CONFIG_s *pic_config);
static int get_free_fb(struct VP9_Common_s *cm)
{
	struct RefCntBuffer_s *const frame_bufs = cm->buffer_pool->frame_bufs;
	int i;

	lock_buffer_pool(cm->buffer_pool);
	for (i = 0; i < FRAME_BUFFERS; ++i) {
		if (debug & VP9_DEBUG_BUFMGR_MORE)
			pr_info("%s:%d, ref_count %d vf_ref %d used_by_d %d index %d\r\n",
			__func__, i, frame_bufs[i].ref_count,
			frame_bufs[i].buf.vf_ref,
			frame_bufs[i].buf.used_by_display,
			frame_bufs[i].buf.index);
		if ((frame_bufs[i].ref_count == 0) &&
			(frame_bufs[i].buf.vf_ref == 0) &&
			(frame_bufs[i].buf.used_by_display == 0) &&
			(frame_bufs[i].buf.index != -1)
			)
			break;
	}
	if (i != FRAME_BUFFERS) {
		frame_bufs[i].ref_count = 1;
		/*pr_info("[MMU DEBUG 1] set ref_count[%d] : %d\r\n",
					i, frame_bufs[i].ref_count);*/
	} else {
		/* Reset i to be INVALID_IDX to indicate
			no free buffer found*/
		i = INVALID_IDX;
	}

	unlock_buffer_pool(cm->buffer_pool);
	return i;
}

static unsigned char is_buffer_empty(struct VP9_Common_s *cm)
{
	struct RefCntBuffer_s *const frame_bufs = cm->buffer_pool->frame_bufs;
	int i;

	for (i = 0; i < FRAME_BUFFERS; ++i)
		if ((frame_bufs[i].ref_count == 0) &&
			(frame_bufs[i].buf.vf_ref == 0) &&
			(frame_bufs[i].buf.used_by_display == 0) &&
			(frame_bufs[i].buf.index != -1)
			)
			break;
	if (i != FRAME_BUFFERS)
		return 0;

	return 1;
}

static struct PIC_BUFFER_CONFIG_s *get_frame_new_buffer(struct VP9_Common_s *cm)
{
	return &cm->buffer_pool->frame_bufs[cm->new_fb_idx].buf;
}

static void ref_cnt_fb(struct RefCntBuffer_s *bufs, int *idx, int new_idx)
{
	const int ref_index = *idx;

	if (ref_index >= 0 && bufs[ref_index].ref_count > 0) {
		bufs[ref_index].ref_count--;
		/*pr_info("[MMU DEBUG 2] dec ref_count[%d] : %d\r\n",
				ref_index, bufs[ref_index].ref_count);*/
	}

	*idx = new_idx;

	bufs[new_idx].ref_count++;
	/*pr_info("[MMU DEBUG 3] inc ref_count[%d] : %d\r\n",
				new_idx, bufs[new_idx].ref_count);*/
}

int vp9_release_frame_buffer(struct vpx_codec_frame_buffer_s *fb)
{
	struct InternalFrameBuffer_s *const int_fb =
				(struct InternalFrameBuffer_s *)fb->priv;
	if (int_fb)
		int_fb->in_use = 0;
	return 0;
}

static int  compute_losless_comp_body_size(int width, int height,
				uint8_t is_bit_depth_10);

static void setup_display_size(struct VP9_Common_s *cm, union param_u *params,
						int print_header_info)
{
	cm->display_width = cm->width;
	cm->display_height = cm->height;
	if (params->p.display_size_present) {
		if (print_header_info)
			pr_info(" * 1-bit display_size_present read : 1\n");
		cm->display_width = params->p.display_width;
		cm->display_height = params->p.display_height;
		/*vp9_read_frame_size(rb, &cm->display_width,
							&cm->display_height);*/
	} else {
		if (print_header_info)
			pr_info(" * 1-bit display_size_present read : 0\n");
	}
}

static void resize_context_buffers(struct VP9_Common_s *cm, int width,
								int height)
{
	if (cm->width != width || cm->height != height) {
		/* to do ..*/
		cm->width = width;
		cm->height = height;
		pr_info("%s (%d,%d)=>(%d,%d)\r\n", __func__, cm->width,
			cm->height, width, height);
	}
	/*
	if (cm->cur_frame->mvs == NULL ||
		cm->mi_rows > cm->cur_frame->mi_rows ||
		cm->mi_cols > cm->cur_frame->mi_cols) {
		resize_mv_buffer(cm);
	}
	*/
}

static int valid_ref_frame_size(int ref_width, int ref_height,
				int this_width, int this_height) {
	return 2 * this_width >= ref_width &&
		2 * this_height >= ref_height &&
		this_width <= 16 * ref_width &&
		this_height <= 16 * ref_height;
}

/*
static int valid_ref_frame_img_fmt(enum vpx_bit_depth_t ref_bit_depth,
					int ref_xss, int ref_yss,
					enum vpx_bit_depth_t this_bit_depth,
					int this_xss, int this_yss) {
	return ref_bit_depth == this_bit_depth && ref_xss == this_xss &&
		ref_yss == this_yss;
}
*/


static int setup_frame_size(
		struct VP9Decoder_s *pbi,
		struct VP9_Common_s *cm, union param_u *params,
		unsigned int *mmu_index_adr,
		int print_header_info) {
	int width, height;
	struct BufferPool_s * const pool = cm->buffer_pool;
	struct PIC_BUFFER_CONFIG_s *ybf;
	int ret = 0;

	width = params->p.width;
	height = params->p.height;
	/*vp9_read_frame_size(rb, &width, &height);*/
	if (print_header_info)
		pr_info(" * 16-bits w read : %d (width : %d)\n", width, height);
	if (print_header_info)
		pr_info
		(" * 16-bits h read : %d (height : %d)\n", width, height);

	WRITE_VREG(HEVC_PARSER_PICTURE_SIZE, (height << 16) | width);

#ifdef VP9_10B_MMU
	/* if(cm->prev_fb_idx >= 0) release_unused_4k(cm->prev_fb_idx);*/
	/* cm->prev_fb_idx = cm->new_fb_idx;*/
	/*pr_info
	("[DEBUG DEBUG]Before alloc_mmu, prev_fb_idx : %d, new_fb_idx : %d\r\n",
	cm->prev_fb_idx, cm->new_fb_idx);*/
	ret = vp9_alloc_mmu(pbi,
		cm->new_fb_idx,
		params->p.width,
		params->p.height,
		params->p.bit_depth,
		mmu_index_adr);
	if (ret != 0) {
		pr_err("can't alloc need mmu1,idx %d ret =%d\n",
			cm->new_fb_idx,
			ret);
		return ret;
	}
#endif

	resize_context_buffers(cm, width, height);
	setup_display_size(cm, params, print_header_info);
#if 0
	lock_buffer_pool(pool);
	if (vp9_realloc_frame_buffer(
		get_frame_new_buffer(cm), cm->width, cm->height,
		cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
		cm->use_highbitdepth,
#endif
		VP9_DEC_BORDER_IN_PIXELS,
		cm->byte_alignment,
		&pool->frame_bufs[cm->new_fb_idx].raw_frame_buffer,
		pool->get_fb_cb, pool->cb_priv)) {
		unlock_buffer_pool(pool);
		vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
			"Failed to allocate frame buffer");
	}
	unlock_buffer_pool(pool);
#else
	/* porting */
	ybf = get_frame_new_buffer(cm);
	ybf->y_crop_width = width;
	ybf->y_crop_height = height;
	ybf->bit_depth = params->p.bit_depth;
#endif
	pool->frame_bufs[cm->new_fb_idx].buf.subsampling_x = cm->subsampling_x;
	pool->frame_bufs[cm->new_fb_idx].buf.subsampling_y = cm->subsampling_y;
	pool->frame_bufs[cm->new_fb_idx].buf.bit_depth =
						(unsigned int)cm->bit_depth;
	pool->frame_bufs[cm->new_fb_idx].buf.color_space = cm->color_space;
	return ret;
}

static int setup_frame_size_with_refs(
		struct VP9Decoder_s *pbi,
		struct VP9_Common_s *cm,
		union param_u *params,
		unsigned int *mmu_index_adr,
		int print_header_info) {

	int width, height;
	int found = 0, i;
	int has_valid_ref_frame = 0;
	struct PIC_BUFFER_CONFIG_s *ybf;
	struct BufferPool_s * const pool = cm->buffer_pool;
	int ret = 0;

	for (i = 0; i < REFS_PER_FRAME; ++i) {
		if ((params->p.same_frame_size >>
				(REFS_PER_FRAME - i - 1)) & 0x1) {
			struct PIC_BUFFER_CONFIG_s *const buf =
							cm->frame_refs[i].buf;
			/*if (print_header_info)
				pr_info
				("1-bit same_frame_size[%d] read : 1\n", i);*/
				width = buf->y_crop_width;
				height = buf->y_crop_height;
			/*if (print_header_info)
				pr_info
				(" - same_frame_size width : %d\n", width);*/
			/*if (print_header_info)
				pr_info
				(" - same_frame_size height : %d\n", height);*/
			found = 1;
			break;
		} else {
			/*if (print_header_info)
				pr_info
				("1-bit same_frame_size[%d] read : 0\n", i);*/
		}
	}

	if (!found) {
		/*vp9_read_frame_size(rb, &width, &height);*/
		width = params->p.width;
		height = params->p.height;
		/*if (print_header_info)
			pr_info
			(" * 16-bits w read : %d (width : %d)\n",
				width, height);
		if (print_header_info)
			pr_info
			(" * 16-bits h read : %d (height : %d)\n",
				width, height);*/
	}

	if (width <= 0 || height <= 0) {
		pr_err("Error: Invalid frame size\r\n");
		return -1;
	}

	params->p.width = width;
	params->p.height = height;

	WRITE_VREG(HEVC_PARSER_PICTURE_SIZE, (height << 16) | width);
#ifdef VP9_10B_MMU
	/*if(cm->prev_fb_idx >= 0) release_unused_4k(cm->prev_fb_idx);
	cm->prev_fb_idx = cm->new_fb_idx;*/
/*	pr_info
	("[DEBUG DEBUG]Before alloc_mmu, prev_fb_idx : %d, new_fb_idx : %d\r\n",
	cm->prev_fb_idx, cm->new_fb_idx);*/
	ret = vp9_alloc_mmu(pbi, cm->new_fb_idx,
			params->p.width, params->p.height,
	params->p.bit_depth, mmu_index_adr);
	if (ret != 0) {
		pr_err("can't alloc need mmu,idx %d\r\n",
			cm->new_fb_idx);
		return ret;
	}
#endif

	/*Check to make sure at least one of frames that this frame references
	has valid dimensions.*/
	for (i = 0; i < REFS_PER_FRAME; ++i) {
		struct RefBuffer_s * const ref_frame = &cm->frame_refs[i];
		has_valid_ref_frame |=
			valid_ref_frame_size(ref_frame->buf->y_crop_width,
			ref_frame->buf->y_crop_height,
			width, height);
	}
	if (!has_valid_ref_frame) {
		pr_err("Error: Referenced frame has invalid size\r\n");
		return -1;
	}
#if 0
	for (i = 0; i < REFS_PER_FRAME; ++i) {
			struct RefBuffer_s * const ref_frame =
							&cm->frame_refs[i];
			if (!valid_ref_frame_img_fmt(
				ref_frame->buf->bit_depth,
				ref_frame->buf->subsampling_x,
				ref_frame->buf->subsampling_y,
				cm->bit_depth,
				cm->subsampling_x,
				cm->subsampling_y))
				pr_err
				("Referenced frame incompatible color fmt\r\n");
				return -1;
	}
#endif
	resize_context_buffers(cm, width, height);
	setup_display_size(cm, params, print_header_info);

#if 0
	lock_buffer_pool(pool);
	if (vp9_realloc_frame_buffer(
		get_frame_new_buffer(cm), cm->width, cm->height,
		cm->subsampling_x, cm->subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
		cm->use_highbitdepth,
#endif
		VP9_DEC_BORDER_IN_PIXELS,
		cm->byte_alignment,
		&pool->frame_bufs[cm->new_fb_idx].raw_frame_buffer,
		pool->get_fb_cb,
		pool->cb_priv)) {
			unlock_buffer_pool(pool);
			vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
			"Failed to allocate frame buffer");
	}
	unlock_buffer_pool(pool);
#else
	/* porting */
	ybf = get_frame_new_buffer(cm);
	ybf->y_crop_width = width;
	ybf->y_crop_height = height;
	ybf->bit_depth = params->p.bit_depth;
#endif
	pool->frame_bufs[cm->new_fb_idx].buf.subsampling_x = cm->subsampling_x;
	pool->frame_bufs[cm->new_fb_idx].buf.subsampling_y = cm->subsampling_y;
	pool->frame_bufs[cm->new_fb_idx].buf.bit_depth =
						(unsigned int)cm->bit_depth;
	pool->frame_bufs[cm->new_fb_idx].buf.color_space = cm->color_space;
	return ret;
}

uint8_t print_header_info = 0;

struct buff_s {
	u32 buf_start;
	u32 buf_size;
	u32 buf_end;
} buff_t;

struct BuffInfo_s {
	u32 max_width;
	u32 max_height;
	u32 start_adr;
	u32 end_adr;
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
	struct buff_s seg_map;
#ifdef VP9_10B_MMU
	struct buff_s mmu_vbh;
	struct buff_s cm_header;
#endif
	struct buff_s mpred_above;
	struct buff_s mpred_mv;
	struct buff_s rpm;
	struct buff_s lmem;
} BuffInfo_t;

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

static void vp9_work(struct work_struct *work);
#endif

struct VP9Decoder_s {
#ifdef MULTI_INSTANCE_SUPPORT
	unsigned char index;

	struct device *cma_dev;
	struct platform_device *platform_dev;
	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;
	struct vframe_chunk_s *chunk;
	int dec_result;
	struct work_struct work;

	struct BuffInfo_s work_space_buf_store;
	unsigned long buf_start;
	u32 buf_size;
	u32 cma_alloc_count;
	unsigned long cma_alloc_addr;
#endif
	unsigned char m_ins_flag;
	char *provider_name;
	union param_u param;
	int frame_count;
	int pic_count;
	u32 stat;
	struct timer_list timer;
	u32 frame_dur;
	u32 frame_ar;
	int fatal_error;
	uint8_t init_flag;
	uint8_t process_busy;
	int show_frame_num;
	struct buff_s mc_buf_spec;
	struct dec_sysinfo vvp9_amstream_dec_info;
	void *rpm_addr;
	void *lmem_addr;
	dma_addr_t rpm_phy_addr;
	dma_addr_t lmem_phy_addr;
	unsigned short *lmem_ptr;
	unsigned short *debug_ptr;

	void *prob_buffer_addr;
	void *count_buffer_addr;
	dma_addr_t prob_buffer_phy_addr;
	dma_addr_t count_buffer_phy_addr;

#if 1
	/*VP9_10B_MMU*/
	void *frame_mmu_map_addr;
	dma_addr_t frame_mmu_map_phy_addr;
#endif
	unsigned int use_cma_flag;

	struct BUF_s m_BUF[MAX_BUF_NUM];
	u32 used_buf_num;
	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(pending_q, struct vframe_s *, VF_POOL_SIZE);
	struct vframe_s vfpool[VF_POOL_SIZE];
	int buf_num;
	int pic_num;
	int lcu_size_log2;
	unsigned int losless_comp_body_size;

	u32 video_signal_type;

	int pts_mode;
	int last_lookup_pts;
	int last_pts;
	u64 last_lookup_pts_us64;
	u64 last_pts_us64;
	u64 shift_byte_count;
	u32 shift_byte_count_lo;
	u32 shift_byte_count_hi;
	int pts_mode_switching_count;
	int pts_mode_recovery_count;

	bool get_frame_dur;
	u32 saved_resolution;

	/**/
	struct VP9_Common_s common;
	struct RefCntBuffer_s *cur_buf;
	int refresh_frame_flags;
	uint8_t need_resync;
	uint8_t hold_ref_buf;
	uint8_t ready_for_new_data;
	struct BufferPool_s vp9_buffer_pool;

	struct BuffInfo_s *work_space_buf;
	struct buff_s *mc_buf;

	unsigned int frame_width;
	unsigned int frame_height;

	unsigned short *rpm_ptr;
	int     init_pic_w;
	int     init_pic_h;
	int     lcu_total;
	int     lcu_size;

	int     slice_type;

	int skip_flag;
	int decode_idx;
	int slice_idx;
	uint8_t has_keyframe;
	uint8_t wait_buf;
	uint8_t error_flag;

	/* bit 0, for decoding; bit 1, for displaying */
	uint8_t ignore_bufmgr_error;
	int PB_skip_mode;
	int PB_skip_count_after_decoding;
	/*hw*/

	u32 pre_stream_offset;

	unsigned int dec_status;
	u32 last_put_idx;
	int new_frame_displayed;
	void *mmu_box;
	void *bmmu_box;
} VP9Decoder;

static int vp9_print(struct VP9Decoder_s *pbi,
	int flag, const char *fmt, ...)
{
#define HEVC_PRINT_BUF		128
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
	if (pbi == NULL ||
		(flag == 0) ||
		(debug & flag)) {
		va_list args;
		va_start(args, fmt);
		if (pbi)
			len = sprintf(buf, "[%d]", pbi->index);
		vsnprintf(buf + len, HEVC_PRINT_BUF - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

#ifdef MULTI_INSTANCE_SUPPORT
static int vp9_print_cont(struct VP9Decoder_s *pbi,
	int flag, const char *fmt, ...)
{
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
	if (pbi == NULL ||
		(flag == 0) ||
		(debug & flag)) {
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf + len, HEVC_PRINT_BUF - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}
#endif

#ifdef VP9_10B_MMU
int vp9_alloc_mmu(
	struct VP9Decoder_s *pbi,
	int cur_buf_idx,
	int pic_width,
	int pic_height,
	unsigned short bit_depth,
	unsigned int *mmu_index_adr)
{
	int bit_depth_10 = (bit_depth == VPX_BITS_10);
	int picture_size;
	int cur_mmu_4k_number;

	picture_size = compute_losless_comp_body_size(pic_width, pic_height,
				   bit_depth_10);
	cur_mmu_4k_number = ((picture_size + (1 << 12) - 1) >> 12);
	return decoder_mmu_box_alloc_idx(
		pbi->mmu_box,
		cur_buf_idx,
		cur_mmu_4k_number,
		mmu_index_adr);
}
#endif
static void decrease_ref_count(int idx, struct RefCntBuffer_s *const frame_bufs,
					struct BufferPool_s *const pool)
{
	if (idx >= 0) {
		--frame_bufs[idx].ref_count;
		/*pr_info("[MMU DEBUG 7] dec ref_count[%d] : %d\r\n", idx,
			frame_bufs[idx].ref_count);*/
		/*A worker may only get a free framebuffer index when
		calling get_free_fb. But the private buffer is not set up
		until finish decoding header. So any error happens during
		decoding header, the frame_bufs will not have valid priv
		buffer.*/

		if (frame_bufs[idx].ref_count == 0 &&
			frame_bufs[idx].raw_frame_buffer.priv)
			vp9_release_frame_buffer
			(&frame_bufs[idx].raw_frame_buffer);
	}
}

static void generate_next_ref_frames(struct VP9Decoder_s *pbi)
{
	struct VP9_Common_s *const cm = &pbi->common;
	struct RefCntBuffer_s *frame_bufs = cm->buffer_pool->frame_bufs;
	struct BufferPool_s *const pool = cm->buffer_pool;
	int mask, ref_index = 0;

	/* Generate next_ref_frame_map.*/
	lock_buffer_pool(pool);
	for (mask = pbi->refresh_frame_flags; mask; mask >>= 1) {
		if (mask & 1) {
			cm->next_ref_frame_map[ref_index] = cm->new_fb_idx;
			++frame_bufs[cm->new_fb_idx].ref_count;
			/*pr_info("[MMU DEBUG 4] inc ref_count[%d] : %d\r\n",
			cm->new_fb_idx, frame_bufs[cm->new_fb_idx].ref_count);*/
		} else
			cm->next_ref_frame_map[ref_index] =
				cm->ref_frame_map[ref_index];
		/* Current thread holds the reference frame.*/
		if (cm->ref_frame_map[ref_index] >= 0) {
			++frame_bufs[cm->ref_frame_map[ref_index]].ref_count;
			/*pr_info
			("[MMU DEBUG 5] inc ref_count[%d] : %d\r\n",
			cm->ref_frame_map[ref_index],
			frame_bufs[cm->ref_frame_map[ref_index]].ref_count);*/
		}
		++ref_index;
	}

	for (; ref_index < REF_FRAMES; ++ref_index) {
		cm->next_ref_frame_map[ref_index] =
			cm->ref_frame_map[ref_index];
		/* Current thread holds the reference frame.*/
		if (cm->ref_frame_map[ref_index] >= 0) {
			++frame_bufs[cm->ref_frame_map[ref_index]].ref_count;
			/*pr_info("[MMU DEBUG 6] inc ref_count[%d] : %d\r\n",
			cm->ref_frame_map[ref_index],
			frame_bufs[cm->ref_frame_map[ref_index]].ref_count);*/
		}
	}
	unlock_buffer_pool(pool);
	return;
}

static void refresh_ref_frames(struct VP9Decoder_s *pbi)

{
	struct VP9_Common_s *const cm = &pbi->common;
	struct BufferPool_s *pool = cm->buffer_pool;
	struct RefCntBuffer_s *frame_bufs = cm->buffer_pool->frame_bufs;
	int mask, ref_index = 0;

	lock_buffer_pool(pool);
	for (mask = pbi->refresh_frame_flags; mask; mask >>= 1) {
		const int old_idx = cm->ref_frame_map[ref_index];
		/*Current thread releases the holding of reference frame.*/
		decrease_ref_count(old_idx, frame_bufs, pool);

		/*Release the reference frame in reference map.*/
		if ((mask & 1) && old_idx >= 0)
			decrease_ref_count(old_idx, frame_bufs, pool);
		cm->ref_frame_map[ref_index] =
			cm->next_ref_frame_map[ref_index];
		++ref_index;
	}

	/*Current thread releases the holding of reference frame.*/
	for (; ref_index < REF_FRAMES && !cm->show_existing_frame;
		++ref_index) {
		const int old_idx = cm->ref_frame_map[ref_index];
		decrease_ref_count(old_idx, frame_bufs, pool);
		cm->ref_frame_map[ref_index] =
			cm->next_ref_frame_map[ref_index];
	}
	unlock_buffer_pool(pool);
	return;
}
int vp9_bufmgr_process(struct VP9Decoder_s *pbi, union param_u *params)
{
	struct VP9_Common_s *const cm = &pbi->common;
	struct BufferPool_s *pool = cm->buffer_pool;
	struct RefCntBuffer_s *frame_bufs = cm->buffer_pool->frame_bufs;
	int i;
	int ret;

	pbi->ready_for_new_data = 0;

	if (pbi->has_keyframe == 0 &&
		params->p.frame_type != KEY_FRAME){
		on_no_keyframe_skiped++;
		return -2;
	}
	pbi->has_keyframe = 1;
	on_no_keyframe_skiped = 0;
#ifdef VP9_10B_MMU
	if (cm->prev_fb_idx >= 0) {
		long used_4k_num = (READ_VREG(HEVC_SAO_MMU_STATUS) >> 16);
		decoder_mmu_box_free_idx_tail(pbi->mmu_box,
		cm->prev_fb_idx, used_4k_num);
	}
#endif
	if (cm->new_fb_idx >= 0
		&& frame_bufs[cm->new_fb_idx].ref_count == 0){
		vp9_release_frame_buffer
			(&frame_bufs[cm->new_fb_idx].raw_frame_buffer);
	}
	/*pr_info("Before get_free_fb, prev_fb_idx : %d, new_fb_idx : %d\r\n",
		cm->prev_fb_idx, cm->new_fb_idx);*/
	cm->new_fb_idx = get_free_fb(cm);
	cm->cur_frame = &pool->frame_bufs[cm->new_fb_idx];
	/*if (debug & VP9_DEBUG_BUFMGR)
		pr_info("[VP9 DEBUG]%s(get_free_fb): %d\r\n", __func__,
				cm->new_fb_idx);*/
	if (cm->new_fb_idx == INVALID_IDX) {
		pr_info("get_free_fb error\r\n");
		return -1;
	}
	pbi->cur_buf = &frame_bufs[cm->new_fb_idx];
#ifdef VP9_10B_MMU
	/* moved to after picture size ready
	alloc_mmu(cm, params->p.width, params->p.height,
	params->p.bit_depth, pbi->frame_mmu_map_addr);*/
	cm->prev_fb_idx = cm->new_fb_idx;
#endif
	/*read_uncompressed_header()*/
	cm->last_frame_type = cm->frame_type;
	cm->last_intra_only = cm->intra_only;
	cm->profile = params->p.profile;
	if (cm->profile >= MAX_PROFILES) {
		pr_err("Error: Unsupported profile %d\r\n", cm->profile);
		return -1;
	}
	cm->show_existing_frame = params->p.show_existing_frame;
	if (cm->show_existing_frame) {
		/* Show an existing frame directly.*/
		int frame_to_show_idx = params->p.frame_to_show_idx;
		int frame_to_show;
		if (frame_to_show_idx >= REF_FRAMES) {
			pr_info("frame_to_show_idx %d exceed max index\r\n",
					frame_to_show_idx);
			return -1;
		}

		frame_to_show = cm->ref_frame_map[frame_to_show_idx];
		/*pr_info("frame_to_show %d\r\n", frame_to_show);*/
		lock_buffer_pool(pool);
		if (frame_to_show < 0 ||
			frame_bufs[frame_to_show].ref_count < 1) {
			unlock_buffer_pool(pool);
			pr_err
			("Error:Buffer %d does not contain a decoded frame",
			frame_to_show);
			return -1;
		}

		ref_cnt_fb(frame_bufs, &cm->new_fb_idx, frame_to_show);
		unlock_buffer_pool(pool);
		pbi->refresh_frame_flags = 0;
		/*cm->lf.filter_level = 0;*/
		cm->show_frame = 1;

		/*
		if (pbi->frame_parallel_decode) {
			for (i = 0; i < REF_FRAMES; ++i)
				cm->next_ref_frame_map[i] =
				cm->ref_frame_map[i];
		}
		*/
		/* do not decode, search next start code */
		return 1;
	}
	cm->frame_type = params->p.frame_type;
	cm->show_frame = params->p.show_frame;
	cm->error_resilient_mode = params->p.error_resilient_mode;


	if (cm->frame_type == KEY_FRAME) {
		pbi->refresh_frame_flags = (1 << REF_FRAMES) - 1;

		for (i = 0; i < REFS_PER_FRAME; ++i) {
			cm->frame_refs[i].idx = INVALID_IDX;
			cm->frame_refs[i].buf = NULL;
		}

		ret = setup_frame_size(pbi,
			cm, params,  pbi->frame_mmu_map_addr,
				print_header_info);
		if (ret)
			return -1;
		if (pbi->need_resync) {
			memset(&cm->ref_frame_map, -1,
				sizeof(cm->ref_frame_map));
			pbi->need_resync = 0;
		}
	} else {
		cm->intra_only = cm->show_frame ? 0 : params->p.intra_only;
		/*if (print_header_info) {
			if (cm->show_frame)
				pr_info
				("intra_only set to 0 because of show_frame\n");
			else
				pr_info
				("1-bit intra_only read: %d\n", cm->intra_only);
		}*/


		cm->reset_frame_context = cm->error_resilient_mode ?
			0 : params->p.reset_frame_context;
		if (print_header_info) {
			if (cm->error_resilient_mode)
				pr_info
				("reset to 0 error_resilient_mode\n");
		else
			pr_info
				(" * 2-bits reset_frame_context read : %d\n",
				cm->reset_frame_context);
		}

		if (cm->intra_only) {
			if (cm->profile > PROFILE_0) {
				/*read_bitdepth_colorspace_sampling(cm,
					rb, print_header_info);*/
			} else {
				/*NOTE: The intra-only frame header
				does not include the specification
				of either the color format or color sub-sampling
				in profile 0. VP9 specifies that the default
				color format should be YUV 4:2:0 in this
				case (normative).*/
				cm->color_space = VPX_CS_BT_601;
				cm->subsampling_y = cm->subsampling_x = 1;
				cm->bit_depth = VPX_BITS_8;
				cm->use_highbitdepth = 0;
			}

			pbi->refresh_frame_flags =
				params->p.refresh_frame_flags;
			/*if (print_header_info)
				pr_info("*%d-bits refresh_frame read:0x%x\n",
				REF_FRAMES, pbi->refresh_frame_flags);*/
			ret = setup_frame_size(pbi,
				cm,
				params,
				pbi->frame_mmu_map_addr,
				print_header_info);
			if (ret) {
				return -1;
			}
			if (pbi->need_resync) {
				memset(&cm->ref_frame_map, -1,
					sizeof(cm->ref_frame_map));
				pbi->need_resync = 0;
			}
		} else if (pbi->need_resync != 1) {  /* Skip if need resync */
				pbi->refresh_frame_flags =
						params->p.refresh_frame_flags;
				if (print_header_info)
					pr_info
					("*%d-bits refresh_frame read:0x%x\n",
					REF_FRAMES, pbi->refresh_frame_flags);
				for (i = 0; i < REFS_PER_FRAME; ++i) {
					const int ref =
						(params->p.ref_info >>
						(((REFS_PER_FRAME-i-1)*4)+1))
						& 0x7;
					const int idx =
						cm->ref_frame_map[ref];
					struct RefBuffer_s * const ref_frame =
						&cm->frame_refs[i];
					if (print_header_info)
						pr_info
						("*%d-bits ref[%d]read:%d\n",
						REF_FRAMES_LOG2, i, ref);
					ref_frame->idx = idx;
					ref_frame->buf = &frame_bufs[idx].buf;
					cm->ref_frame_sign_bias[LAST_FRAME + i]
					= (params->p.ref_info >>
					((REFS_PER_FRAME-i-1)*4)) & 0x1;
					if (print_header_info)
						pr_info
						("1bit ref_frame_sign_bias");
						/*pr_info
						("%dread: %d\n",
						LAST_FRAME+i,
						cm->ref_frame_sign_bias
						[LAST_FRAME + i]);*/
					/*pr_info
					("[VP9 DEBUG]%s(get ref):%d\r\n",
					__func__, ref_frame->idx);*/

				}

				ret = setup_frame_size_with_refs(
					pbi,
					cm,
					params,
					pbi->frame_mmu_map_addr,
					print_header_info);
				if (ret) {
					return -1;
				}
				for (i = 0; i < REFS_PER_FRAME; ++i) {
					/*struct RefBuffer_s *const ref_buf =
					&cm->frame_refs[i];*/
					/* to do:
					vp9_setup_scale_factors_for_frame*/
				}
		}
	}

	get_frame_new_buffer(cm)->bit_depth = cm->bit_depth;
	get_frame_new_buffer(cm)->color_space = cm->color_space;
	get_frame_new_buffer(cm)->slice_type = cm->frame_type;

	if (pbi->need_resync)
		pr_err
		("Error: Keyframe/intra-only frame required to reset\r\n");
	generate_next_ref_frames(pbi);
	pbi->hold_ref_buf = 1;

#if 0
	if (frame_is_intra_only(cm) || cm->error_resilient_mode)
		vp9_setup_past_independence(cm);
	setup_loopfilter(&cm->lf, rb, print_header_info);
	setup_quantization(cm, &pbi->mb, rb, print_header_info);
	setup_segmentation(&cm->seg, rb, print_header_info);
	setup_segmentation_dequant(cm, print_header_info);

	setup_tile_info(cm, rb, print_header_info);
	sz = vp9_rb_read_literal(rb, 16);
	if (print_header_info)
		pr_info(" * 16-bits size read : %d (0x%x)\n", sz, sz);

	if (sz == 0)
		vpx_internal_error(&cm->error, VPX_CODEC_CORRUPT_FRAME,
		"Invalid header size");
#endif
	/*end read_uncompressed_header()*/
	cm->use_prev_frame_mvs = !cm->error_resilient_mode &&
			cm->width == cm->last_width &&
			cm->height == cm->last_height &&
			!cm->last_intra_only &&
			cm->last_show_frame &&
			(cm->last_frame_type != KEY_FRAME);

	/*pr_info
	("set use_prev_frame_mvs to %d (last_width %d last_height %d",
	cm->use_prev_frame_mvs, cm->last_width, cm->last_height);
	pr_info
	(" last_intra_only %d last_show_frame %d last_frame_type %d)\n",
	cm->last_intra_only, cm->last_show_frame, cm->last_frame_type);*/
	return 0;
}


void swap_frame_buffers(struct VP9Decoder_s *pbi)
{
	int ref_index = 0;
	struct VP9_Common_s *const cm = &pbi->common;
	struct BufferPool_s *const pool = cm->buffer_pool;
	struct RefCntBuffer_s *const frame_bufs = cm->buffer_pool->frame_bufs;
	refresh_ref_frames(pbi);
	pbi->hold_ref_buf = 0;
	cm->frame_to_show = get_frame_new_buffer(cm);

	/*if (!pbi->frame_parallel_decode || !cm->show_frame) {*/
	lock_buffer_pool(pool);
	--frame_bufs[cm->new_fb_idx].ref_count;
	/*pr_info("[MMU DEBUG 8] dec ref_count[%d] : %d\r\n", cm->new_fb_idx,
		frame_bufs[cm->new_fb_idx].ref_count);*/
	unlock_buffer_pool(pool);
	/*}*/

	/*Invalidate these references until the next frame starts.*/
	for (ref_index = 0; ref_index < 3; ref_index++)
		cm->frame_refs[ref_index].idx = -1;
}

#if 0
static void check_resync(vpx_codec_alg_priv_t *const ctx,
				const struct VP9Decoder_s *const pbi)
{
	/* Clear resync flag if worker got a key frame or intra only frame.*/
	if (ctx->need_resync == 1 && pbi->need_resync == 0 &&
		(pbi->common.intra_only || pbi->common.frame_type == KEY_FRAME))
		ctx->need_resync = 0;
}
#endif

int vp9_get_raw_frame(struct VP9Decoder_s *pbi, struct PIC_BUFFER_CONFIG_s *sd)
{
	struct VP9_Common_s *const cm = &pbi->common;
	int ret = -1;

	if (pbi->ready_for_new_data == 1)
		return ret;

	pbi->ready_for_new_data = 1;

	/* no raw frame to show!!! */
	if (!cm->show_frame)
		return ret;

	pbi->ready_for_new_data = 1;

	*sd = *cm->frame_to_show;
	ret = 0;

	return ret;
}

int vp9_bufmgr_init(struct VP9Decoder_s *pbi, struct BuffInfo_s *buf_spec_i,
		struct buff_s *mc_buf_i) {
	struct VP9_Common_s *cm = &pbi->common;

	/*memset(pbi, 0, sizeof(struct VP9Decoder));*/
	pbi->frame_count = 0;
	pbi->pic_count = 0;
	pbi->pre_stream_offset = 0;
	cm->buffer_pool = &pbi->vp9_buffer_pool;
	spin_lock_init(&cm->buffer_pool->lock);
	cm->prev_fb_idx = INVALID_IDX;
	cm->new_fb_idx = INVALID_IDX;
	pr_info
	("After vp9_bufmgr_init, prev_fb_idx : %d, new_fb_idx : %d\r\n",
		cm->prev_fb_idx, cm->new_fb_idx);
	pbi->need_resync = 1;
	/* Initialize the references to not point to any frame buffers.*/
	memset(&cm->ref_frame_map, -1, sizeof(cm->ref_frame_map));
	memset(&cm->next_ref_frame_map, -1, sizeof(cm->next_ref_frame_map));
	cm->current_video_frame = 0;
	pbi->ready_for_new_data = 1;

	/* private init */
	pbi->work_space_buf = buf_spec_i;
	pbi->mc_buf = mc_buf_i;
	pbi->rpm_addr = NULL;
	pbi->lmem_addr = NULL;

	pbi->use_cma_flag = 0;
	pbi->decode_idx = 0;
	pbi->slice_idx = 0;
	/*int m_uiMaxCUWidth = 1<<7;*/
	/*int m_uiMaxCUHeight = 1<<7;*/
	pbi->has_keyframe = 0;
	pbi->skip_flag = 0;
	pbi->wait_buf = 0;
	pbi->error_flag = 0;

	pbi->pts_mode = PTS_NORMAL;
	pbi->last_pts = 0;
	pbi->last_lookup_pts = 0;
	pbi->last_pts_us64 = 0;
	pbi->last_lookup_pts_us64 = 0;
	pbi->shift_byte_count = 0;
	pbi->shift_byte_count_lo = 0;
	pbi->shift_byte_count_hi = 0;
	pbi->pts_mode_switching_count = 0;
	pbi->pts_mode_recovery_count = 0;

	pbi->buf_num = 0;
	pbi->pic_num = 0;

	return 0;
}


int vp9_bufmgr_postproc(struct VP9Decoder_s *pbi)
{
	struct VP9_Common_s *cm = &pbi->common;
	struct PIC_BUFFER_CONFIG_s sd;
	swap_frame_buffers(pbi);
	if (!cm->show_existing_frame) {
		cm->last_show_frame = cm->show_frame;
		cm->prev_frame = cm->cur_frame;
#if 0
	if (cm->seg.enabled && !pbi->frame_parallel_decode)
		vp9_swap_current_and_last_seg_map(cm);
#endif
	}
	cm->last_width = cm->width;
	cm->last_height = cm->height;
	if (cm->show_frame)
		cm->current_video_frame++;

	if (vp9_get_raw_frame(pbi, &sd) == 0) {
		/*pr_info("Display frame index %d\r\n", sd.index);*/
		sd.stream_offset = pbi->pre_stream_offset;
		prepare_display_buf(pbi, &sd);
		pbi->pre_stream_offset = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
	} /*else
		pr_info
		("Not display this frame,ready_for_new_data%d show_frame%d\r\n",
		pbi->ready_for_new_data, cm->show_frame);*/
	return 0;
}

struct VP9Decoder_s vp9_decoder;
union param_u vp9_param;

/**************************************************

VP9 buffer management end

***************************************************/


#define HEVC_CM_BODY_START_ADDR                    0x3626
#define HEVC_CM_BODY_LENGTH                        0x3627
#define HEVC_CM_HEADER_LENGTH                      0x3629
#define HEVC_CM_HEADER_OFFSET                      0x362b

#define LOSLESS_COMPRESS_MODE
/* DOUBLE_WRITE_MODE is enabled only when NV21 8 bit output is needed */
/* double_write_mode: 0, no double write; 1, 1:1 ratio; 2, (1/4):(1/4) ratio
	0x10, double write only
*/
static u32 double_write_mode;

/*#define DECOMP_HEADR_SURGENT*/

static u32 mem_map_mode; /* 0:linear 1:32x32 2:64x32 ; m8baby test1902 */
static u32 enable_mem_saving = 1;
static u32 force_w_h;

static u32 force_fps;


const u32 vp9_version = 201602101;
static u32 debug;
static u32 radr;
static u32 rval;
static u32 pop_shorts;
static u32 dbg_cmd;
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
static u32 dynamic_buf_num_margin = 7;
#endif
static u32 buf_alloc_depth = 10;
static u32 buf_alloc_size;
/*
bit[0]: 0,
    bit[1]: 0, always release cma buffer when stop
    bit[1]: 1, never release cma buffer when stop
bit[0]: 1, when stop, release cma buffer if blackout is 1;
do not release cma buffer is blackout is not 1

bit[2]: 0, when start decoding, check current displayed buffer
	 (only for buffer decoded by vp9) if blackout is 0
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
bit 0, 1: only display I picture;
bit 1, 1: only decode I picture;
*/
static u32 i_only_flag;

/*
use_cma: 1, use both reserver memory and cma for buffers
2, only use cma for buffers
*/
static u32 use_cma = 2;

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
0, auto search after error recovery (vp9_recover() called);
1, manual search after error recovery
(change to auto search after get IDR: WRITE_VREG(NAL_SEARCH_CTL, 0x2))

bit 4: 0, set error_mark after reset/recover
    1, do not set error_mark after reset/recover
bit 5: 0, check total lcu for every picture
    1, do not check total lcu

*/

static u32 error_handle_policy;
/*static u32 parser_sei_enable = 1;*/

static u32 max_buf_num = 12;


static DEFINE_MUTEX(vvp9_mutex);
#ifndef MULTI_INSTANCE_SUPPORT
static struct device *cma_dev;
#endif

#define HEVC_DEC_STATUS_REG       HEVC_ASSIST_SCRATCH_0
#define HEVC_RPM_BUFFER           HEVC_ASSIST_SCRATCH_1
#define HEVC_SHORT_TERM_RPS       HEVC_ASSIST_SCRATCH_2
#define VP9_ADAPT_PROB_REG        HEVC_ASSIST_SCRATCH_3
#define VP9_MMU_MAP_BUFFER        HEVC_ASSIST_SCRATCH_4
#define HEVC_PPS_BUFFER           HEVC_ASSIST_SCRATCH_5
#define HEVC_SAO_UP               HEVC_ASSIST_SCRATCH_6
#define HEVC_STREAM_SWAP_BUFFER   HEVC_ASSIST_SCRATCH_7
#define HEVC_STREAM_SWAP_BUFFER2  HEVC_ASSIST_SCRATCH_8
#define VP9_PROB_SWAP_BUFFER      HEVC_ASSIST_SCRATCH_9
#define VP9_COUNT_SWAP_BUFFER     HEVC_ASSIST_SCRATCH_A
#define VP9_SEG_MAP_BUFFER        HEVC_ASSIST_SCRATCH_B
#define HEVC_SCALELUT             HEVC_ASSIST_SCRATCH_D
#define HEVC_WAIT_FLAG            HEVC_ASSIST_SCRATCH_E
#define RPM_CMD_REG               HEVC_ASSIST_SCRATCH_F
#define LMEM_DUMP_ADR                 HEVC_ASSIST_SCRATCH_F
#define HEVC_STREAM_SWAP_TEST     HEVC_ASSIST_SCRATCH_L
#ifdef MULTI_INSTANCE_SUPPORT
#define HEVC_DECODE_COUNT       HEVC_ASSIST_SCRATCH_M
#define HEVC_DECODE_SIZE		HEVC_ASSIST_SCRATCH_N
#else
#define HEVC_DECODE_PIC_BEGIN_REG HEVC_ASSIST_SCRATCH_M
#define HEVC_DECODE_PIC_NUM_REG   HEVC_ASSIST_SCRATCH_N
#endif
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
	0, do not parse SEI in ucode; 1, parse SEI in ucode
bit [31:20]: used by ucode for debug purpose
*/
#define NAL_SEARCH_CTL            HEVC_ASSIST_SCRATCH_I
#define DECODE_MODE              HEVC_ASSIST_SCRATCH_J
#define DECODE_STOP_POS         HEVC_ASSIST_SCRATCH_K

#ifdef MULTI_INSTANCE_SUPPORT
#define RPM_BUF_SIZE (0x400 * 2)
#else
#define RPM_BUF_SIZE (0x80*2)
#endif
#define LMEM_BUF_SIZE (0x400 * 2)

#define WORK_BUF_SPEC_NUM 2
static struct BuffInfo_s amvvp9_workbuff_spec[WORK_BUF_SPEC_NUM] = {
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
			/* DBLK -> Max 256(4096/16) LCU,
			each para 1024bytes(total:0x40000),
			data 1024bytes(total:0x40000)*/
			.buf_size = 0x80000,
		},
		.dblk_data = {
			.buf_size = 0x80000,
		},
		.seg_map = {
		/*4096x2304/64/64 *24 = 0xd800 Bytes*/
			.buf_size = 0xd800,
		},
#ifdef VP9_10B_MMU
		.mmu_vbh = {
			.buf_size = 0x5000, /*2*16*(more than 2304)/4, 4K*/
		},
		.cm_header = {
			/*add one for keeper.*/
			.buf_size = MMU_COMPRESS_HEADER_SIZE *
						(FRAME_BUFFERS + 1),
			/* 0x44000 = ((1088*2*1024*4)/32/4)*(32/8) */
		},
#endif
		.mpred_above = {
			.buf_size = 0x10000, /* 2 * size of hevc*/
		},
		.mpred_mv = {/* 1080p, 0x40000 per buffer */
			.buf_size = 0x40000 * FRAME_BUFFERS,
		},
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x400 * 2,
		}
	},
	{
		.max_width = 4096,
		.max_height = 2304,
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
			/* DBLK -> Max 256(4096/16) LCU,
			each para 1024bytes(total:0x40000),
			data 1024bytes(total:0x40000)*/
			.buf_size = 0x80000,
		},
		.dblk_data = {
			.buf_size = 0x80000,
		},
		.seg_map = {
			/*4096x2304/64/64 *24 = 0xd800 Bytes*/
			.buf_size = 0xd800,
		},
#ifdef VP9_10B_MMU
		.mmu_vbh = {
			.buf_size = 0x5000,/*2*16*(more than 2304)/4, 4K*/
		},
		.cm_header = {
			/*add one for keeper.*/
			.buf_size = MMU_COMPRESS_HEADER_SIZE *
						(FRAME_BUFFERS + 1),
			/* 0x44000 = ((1088*2*1024*4)/32/4)*(32/8) */
		},
#endif
		.mpred_above = {
			.buf_size = 0x10000, /* 2 * size of hevc*/
		},
		.mpred_mv = {
			/* .buf_size = 0x100000*16,
			//4k2k , 0x100000 per buffer */
			/* 4096x2304 , 0x120000 per buffer */
			.buf_size = 0x120000 * FRAME_BUFFERS,
		},
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x400 * 2,
		}
	}
};


/*Losless compression body buffer size 4K per 64x32 (jt)*/
int  compute_losless_comp_body_size(int width, int height,
				uint8_t is_bit_depth_10)
{
	int     width_x64;
	int     height_x32;
	int     bsize;
	width_x64 = width + 63;
	width_x64 >>= 6;
	height_x32 = height + 31;
	height_x32 >>= 5;
#ifdef VP9_10B_MMU
	 bsize = (is_bit_depth_10?4096:3200)*width_x64*height_x32;
#else
	 bsize = (is_bit_depth_10?4096:3072)*width_x64*height_x32;
#endif
	if (debug & VP9_DEBUG_BUFMGR_MORE)
		pr_info("%s(%d,%d,%d)=>%d\n",
			__func__, width, height,
			is_bit_depth_10, bsize);

	return  bsize;
}

/* Losless compression header buffer size 32bytes per 128x64 (jt)*/
static  int  compute_losless_comp_header_size(int width, int height)
{
	int     width_x128;
	int     height_x64;
	int     hsize;
	width_x128 = width + 127;
	width_x128 >>= 7;
	height_x64 = height + 63;
	height_x64 >>= 6;

	hsize = 32 * width_x128 * height_x64;
	if (debug & VP9_DEBUG_BUFMGR_MORE)
		pr_info("%s(%d,%d)=>%d\n",
			__func__, width, height,
			hsize);

	return  hsize;
}

static void init_buff_spec(struct VP9Decoder_s *pbi,
	struct BuffInfo_s *buf_spec)
{
	void *mem_start_virt;
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
	buf_spec->seg_map.buf_start =
	buf_spec->dblk_data.buf_start + buf_spec->dblk_data.buf_size;
#ifdef VP9_10B_MMU
	buf_spec->mmu_vbh.buf_start  =
		buf_spec->seg_map.buf_start + buf_spec->seg_map.buf_size;
	buf_spec->cm_header.buf_start =
		buf_spec->mmu_vbh.buf_start + buf_spec->mmu_vbh.buf_size;
	buf_spec->mpred_above.buf_start =
		buf_spec->cm_header.buf_start + buf_spec->cm_header.buf_size;
#else
	buf_spec->mpred_above.buf_start =
		buf_spec->seg_map.buf_start + buf_spec->seg_map.buf_size;
#endif
	buf_spec->mpred_mv.buf_start =
		buf_spec->mpred_above.buf_start +
		buf_spec->mpred_above.buf_size;
	if (debug & VP9_DEBUG_SEND_PARAM_WITH_REG) {
		buf_spec->end_adr =
			buf_spec->mpred_mv.buf_start +
			buf_spec->mpred_mv.buf_size;
	} else {
		buf_spec->rpm.buf_start =
			buf_spec->mpred_mv.buf_start +
			buf_spec->mpred_mv.buf_size;
		if (debug & VP9_DEBUG_UCODE) {
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
	mem_start_virt = codec_mm_phys_to_virt(buf_spec->dblk_para.buf_start);
	if (mem_start_virt) {
		memset(mem_start_virt, 0, buf_spec->dblk_para.buf_size);
		codec_mm_dma_flush(mem_start_virt,
			buf_spec->dblk_para.buf_size,
			DMA_TO_DEVICE);
	} else {
		/*not virt for tvp playing,
		may need clear on ucode.*/
		pr_err("mem_start_virt failed\n");
	}
	if (debug) {
		pr_info("%s workspace (%x %x) size = %x\n", __func__,
			   buf_spec->start_adr, buf_spec->end_adr,
			   buf_spec->end_adr - buf_spec->start_adr);
	}
	if (debug) {
		pr_info("ipp.buf_start             :%x\n",
			   buf_spec->ipp.buf_start);
		pr_info("sao_abv.buf_start          :%x\n",
			   buf_spec->sao_abv.buf_start);
		pr_info("sao_vb.buf_start          :%x\n",
			   buf_spec->sao_vb.buf_start);
		pr_info("short_term_rps.buf_start  :%x\n",
			   buf_spec->short_term_rps.buf_start);
		pr_info("vps.buf_start             :%x\n",
			   buf_spec->vps.buf_start);
		pr_info("sps.buf_start             :%x\n",
			   buf_spec->sps.buf_start);
		pr_info("pps.buf_start             :%x\n",
			   buf_spec->pps.buf_start);
		pr_info("sao_up.buf_start          :%x\n",
			   buf_spec->sao_up.buf_start);
		pr_info("swap_buf.buf_start        :%x\n",
			   buf_spec->swap_buf.buf_start);
		pr_info("swap_buf2.buf_start       :%x\n",
			   buf_spec->swap_buf2.buf_start);
		pr_info("scalelut.buf_start        :%x\n",
			   buf_spec->scalelut.buf_start);
		pr_info("dblk_para.buf_start       :%x\n",
			   buf_spec->dblk_para.buf_start);
		pr_info("dblk_data.buf_start       :%x\n",
			   buf_spec->dblk_data.buf_start);
		pr_info("seg_map.buf_start       :%x\n",
			buf_spec->seg_map.buf_start);
#ifdef VP9_10B_MMU
		pr_info("mmu_vbh.buf_start     :%x\n",
			buf_spec->mmu_vbh.buf_start);
		pr_info("cm_header.buf_start     :%x\n",
			buf_spec->cm_header.buf_start);
#endif
		pr_info("mpred_above.buf_start     :%x\n",
			   buf_spec->mpred_above.buf_start);
		pr_info("mpred_mv.buf_start        :%x\n",
			   buf_spec->mpred_mv.buf_start);
		if ((debug & VP9_DEBUG_SEND_PARAM_WITH_REG) == 0) {
			pr_info("rpm.buf_start             :%x\n",
				   buf_spec->rpm.buf_start);
		}
	}

}

/*====================================================
========================================================================
vp9_prob define
========================================================================*/
#define VP9_PARTITION_START      0
#define VP9_PARTITION_SIZE_STEP  (3 * 4)
#define VP9_PARTITION_ONE_SIZE   (4 * VP9_PARTITION_SIZE_STEP)
#define VP9_PARTITION_KEY_START  0
#define VP9_PARTITION_P_START    VP9_PARTITION_ONE_SIZE
#define VP9_PARTITION_SIZE       (2 * VP9_PARTITION_ONE_SIZE)
#define VP9_SKIP_START           (VP9_PARTITION_START + VP9_PARTITION_SIZE)
#define VP9_SKIP_SIZE            4 /* only use 3*/
#define VP9_TX_MODE_START        (VP9_SKIP_START+VP9_SKIP_SIZE)
#define VP9_TX_MODE_8_0_OFFSET   0
#define VP9_TX_MODE_8_1_OFFSET   1
#define VP9_TX_MODE_16_0_OFFSET  2
#define VP9_TX_MODE_16_1_OFFSET  4
#define VP9_TX_MODE_32_0_OFFSET  6
#define VP9_TX_MODE_32_1_OFFSET  9
#define VP9_TX_MODE_SIZE         12
#define VP9_COEF_START           (VP9_TX_MODE_START+VP9_TX_MODE_SIZE)
#define VP9_COEF_BAND_0_OFFSET   0
#define VP9_COEF_BAND_1_OFFSET   (VP9_COEF_BAND_0_OFFSET + 3 * 3 + 1)
#define VP9_COEF_BAND_2_OFFSET   (VP9_COEF_BAND_1_OFFSET + 6 * 3)
#define VP9_COEF_BAND_3_OFFSET   (VP9_COEF_BAND_2_OFFSET + 6 * 3)
#define VP9_COEF_BAND_4_OFFSET   (VP9_COEF_BAND_3_OFFSET + 6 * 3)
#define VP9_COEF_BAND_5_OFFSET   (VP9_COEF_BAND_4_OFFSET + 6 * 3)
#define VP9_COEF_SIZE_ONE_SET    100 /* ((3 +5*6)*3 + 1 padding)*/
#define VP9_COEF_4X4_START       (VP9_COEF_START + 0 * VP9_COEF_SIZE_ONE_SET)
#define VP9_COEF_8X8_START       (VP9_COEF_START + 4 * VP9_COEF_SIZE_ONE_SET)
#define VP9_COEF_16X16_START     (VP9_COEF_START + 8 * VP9_COEF_SIZE_ONE_SET)
#define VP9_COEF_32X32_START     (VP9_COEF_START + 12 * VP9_COEF_SIZE_ONE_SET)
#define VP9_COEF_SIZE_PLANE      (2 * VP9_COEF_SIZE_ONE_SET)
#define VP9_COEF_SIZE            (4 * 2 * 2 * VP9_COEF_SIZE_ONE_SET)
#define VP9_INTER_MODE_START     (VP9_COEF_START+VP9_COEF_SIZE)
#define VP9_INTER_MODE_SIZE      24 /* only use 21 ( #*7)*/
#define VP9_INTERP_START         (VP9_INTER_MODE_START+VP9_INTER_MODE_SIZE)
#define VP9_INTERP_SIZE          8
#define VP9_INTRA_INTER_START    (VP9_INTERP_START+VP9_INTERP_SIZE)
#define VP9_INTRA_INTER_SIZE     4
#define VP9_INTERP_INTRA_INTER_START  VP9_INTERP_START
#define VP9_INTERP_INTRA_INTER_SIZE   (VP9_INTERP_SIZE + VP9_INTRA_INTER_SIZE)
#define VP9_COMP_INTER_START     \
		(VP9_INTERP_INTRA_INTER_START+VP9_INTERP_INTRA_INTER_SIZE)
#define VP9_COMP_INTER_SIZE      5
#define VP9_COMP_REF_START       (VP9_COMP_INTER_START+VP9_COMP_INTER_SIZE)
#define VP9_COMP_REF_SIZE        5
#define VP9_SINGLE_REF_START     (VP9_COMP_REF_START+VP9_COMP_REF_SIZE)
#define VP9_SINGLE_REF_SIZE      10
#define VP9_REF_MODE_START       VP9_COMP_INTER_START
#define VP9_REF_MODE_SIZE        \
		(VP9_COMP_INTER_SIZE+VP9_COMP_REF_SIZE+VP9_SINGLE_REF_SIZE)
#define VP9_IF_Y_MODE_START      (VP9_REF_MODE_START+VP9_REF_MODE_SIZE)
#define VP9_IF_Y_MODE_SIZE       36
#define VP9_IF_UV_MODE_START     (VP9_IF_Y_MODE_START+VP9_IF_Y_MODE_SIZE)
#define VP9_IF_UV_MODE_SIZE      92 /* only use 90*/
#define VP9_MV_JOINTS_START      (VP9_IF_UV_MODE_START+VP9_IF_UV_MODE_SIZE)
#define VP9_MV_JOINTS_SIZE       3
#define VP9_MV_SIGN_0_START      (VP9_MV_JOINTS_START+VP9_MV_JOINTS_SIZE)
#define VP9_MV_SIGN_0_SIZE       1
#define VP9_MV_CLASSES_0_START   (VP9_MV_SIGN_0_START+VP9_MV_SIGN_0_SIZE)
#define VP9_MV_CLASSES_0_SIZE    10
#define VP9_MV_CLASS0_0_START    (VP9_MV_CLASSES_0_START+VP9_MV_CLASSES_0_SIZE)
#define VP9_MV_CLASS0_0_SIZE     1
#define VP9_MV_BITS_0_START      (VP9_MV_CLASS0_0_START+VP9_MV_CLASS0_0_SIZE)
#define VP9_MV_BITS_0_SIZE       10
#define VP9_MV_SIGN_1_START      (VP9_MV_BITS_0_START+VP9_MV_BITS_0_SIZE)
#define VP9_MV_SIGN_1_SIZE       1
#define VP9_MV_CLASSES_1_START   \
			(VP9_MV_SIGN_1_START+VP9_MV_SIGN_1_SIZE)
#define VP9_MV_CLASSES_1_SIZE    10
#define VP9_MV_CLASS0_1_START    \
			(VP9_MV_CLASSES_1_START+VP9_MV_CLASSES_1_SIZE)
#define VP9_MV_CLASS0_1_SIZE     1
#define VP9_MV_BITS_1_START      \
			(VP9_MV_CLASS0_1_START+VP9_MV_CLASS0_1_SIZE)
#define VP9_MV_BITS_1_SIZE       10
#define VP9_MV_CLASS0_FP_0_START \
			(VP9_MV_BITS_1_START+VP9_MV_BITS_1_SIZE)
#define VP9_MV_CLASS0_FP_0_SIZE  9
#define VP9_MV_CLASS0_FP_1_START \
			(VP9_MV_CLASS0_FP_0_START+VP9_MV_CLASS0_FP_0_SIZE)
#define VP9_MV_CLASS0_FP_1_SIZE  9
#define VP9_MV_CLASS0_HP_0_START \
			(VP9_MV_CLASS0_FP_1_START+VP9_MV_CLASS0_FP_1_SIZE)
#define VP9_MV_CLASS0_HP_0_SIZE  2
#define VP9_MV_CLASS0_HP_1_START \
			(VP9_MV_CLASS0_HP_0_START+VP9_MV_CLASS0_HP_0_SIZE)
#define VP9_MV_CLASS0_HP_1_SIZE  2
#define VP9_MV_START             VP9_MV_JOINTS_START
#define VP9_MV_SIZE              72 /*only use 69*/

#define VP9_TOTAL_SIZE           (VP9_MV_START + VP9_MV_SIZE)


/*========================================================================
	vp9_count_mem define
========================================================================*/
#define VP9_COEF_COUNT_START           0
#define VP9_COEF_COUNT_BAND_0_OFFSET   0
#define VP9_COEF_COUNT_BAND_1_OFFSET   \
			(VP9_COEF_COUNT_BAND_0_OFFSET + 3*5)
#define VP9_COEF_COUNT_BAND_2_OFFSET   \
			(VP9_COEF_COUNT_BAND_1_OFFSET + 6*5)
#define VP9_COEF_COUNT_BAND_3_OFFSET   \
			(VP9_COEF_COUNT_BAND_2_OFFSET + 6*5)
#define VP9_COEF_COUNT_BAND_4_OFFSET   \
			(VP9_COEF_COUNT_BAND_3_OFFSET + 6*5)
#define VP9_COEF_COUNT_BAND_5_OFFSET   \
			(VP9_COEF_COUNT_BAND_4_OFFSET + 6*5)
#define VP9_COEF_COUNT_SIZE_ONE_SET    165 /* ((3 +5*6)*5 */
#define VP9_COEF_COUNT_4X4_START       \
	(VP9_COEF_COUNT_START + 0*VP9_COEF_COUNT_SIZE_ONE_SET)
#define VP9_COEF_COUNT_8X8_START       \
	(VP9_COEF_COUNT_START + 4*VP9_COEF_COUNT_SIZE_ONE_SET)
#define VP9_COEF_COUNT_16X16_START     \
	(VP9_COEF_COUNT_START + 8*VP9_COEF_COUNT_SIZE_ONE_SET)
#define VP9_COEF_COUNT_32X32_START     \
	(VP9_COEF_COUNT_START + 12*VP9_COEF_COUNT_SIZE_ONE_SET)
#define VP9_COEF_COUNT_SIZE_PLANE      (2 * VP9_COEF_COUNT_SIZE_ONE_SET)
#define VP9_COEF_COUNT_SIZE            (4 * 2 * 2 * VP9_COEF_COUNT_SIZE_ONE_SET)

#define VP9_INTRA_INTER_COUNT_START    \
	(VP9_COEF_COUNT_START+VP9_COEF_COUNT_SIZE)
#define VP9_INTRA_INTER_COUNT_SIZE     (4*2)
#define VP9_COMP_INTER_COUNT_START     \
	(VP9_INTRA_INTER_COUNT_START+VP9_INTRA_INTER_COUNT_SIZE)
#define VP9_COMP_INTER_COUNT_SIZE      (5*2)
#define VP9_COMP_REF_COUNT_START       \
	(VP9_COMP_INTER_COUNT_START+VP9_COMP_INTER_COUNT_SIZE)
#define VP9_COMP_REF_COUNT_SIZE        (5*2)
#define VP9_SINGLE_REF_COUNT_START     \
	(VP9_COMP_REF_COUNT_START+VP9_COMP_REF_COUNT_SIZE)
#define VP9_SINGLE_REF_COUNT_SIZE      (10*2)
#define VP9_TX_MODE_COUNT_START        \
	(VP9_SINGLE_REF_COUNT_START+VP9_SINGLE_REF_COUNT_SIZE)
#define VP9_TX_MODE_COUNT_SIZE         (12*2)
#define VP9_SKIP_COUNT_START           \
	(VP9_TX_MODE_COUNT_START+VP9_TX_MODE_COUNT_SIZE)
#define VP9_SKIP_COUNT_SIZE            (3*2)
#define VP9_MV_SIGN_0_COUNT_START      \
	(VP9_SKIP_COUNT_START+VP9_SKIP_COUNT_SIZE)
#define VP9_MV_SIGN_0_COUNT_SIZE       (1*2)
#define VP9_MV_SIGN_1_COUNT_START      \
	(VP9_MV_SIGN_0_COUNT_START+VP9_MV_SIGN_0_COUNT_SIZE)
#define VP9_MV_SIGN_1_COUNT_SIZE       (1*2)
#define VP9_MV_BITS_0_COUNT_START      \
	(VP9_MV_SIGN_1_COUNT_START+VP9_MV_SIGN_1_COUNT_SIZE)
#define VP9_MV_BITS_0_COUNT_SIZE       (10*2)
#define VP9_MV_BITS_1_COUNT_START      \
	(VP9_MV_BITS_0_COUNT_START+VP9_MV_BITS_0_COUNT_SIZE)
#define VP9_MV_BITS_1_COUNT_SIZE       (10*2)
#define VP9_MV_CLASS0_HP_0_COUNT_START \
	(VP9_MV_BITS_1_COUNT_START+VP9_MV_BITS_1_COUNT_SIZE)
#define VP9_MV_CLASS0_HP_0_COUNT_SIZE  (2*2)
#define VP9_MV_CLASS0_HP_1_COUNT_START \
	(VP9_MV_CLASS0_HP_0_COUNT_START+VP9_MV_CLASS0_HP_0_COUNT_SIZE)
#define VP9_MV_CLASS0_HP_1_COUNT_SIZE  (2*2)
/* Start merge_tree*/
#define VP9_INTER_MODE_COUNT_START     \
	(VP9_MV_CLASS0_HP_1_COUNT_START+VP9_MV_CLASS0_HP_1_COUNT_SIZE)
#define VP9_INTER_MODE_COUNT_SIZE      (7*4)
#define VP9_IF_Y_MODE_COUNT_START      \
	(VP9_INTER_MODE_COUNT_START+VP9_INTER_MODE_COUNT_SIZE)
#define VP9_IF_Y_MODE_COUNT_SIZE       (10*4)
#define VP9_IF_UV_MODE_COUNT_START     \
	(VP9_IF_Y_MODE_COUNT_START+VP9_IF_Y_MODE_COUNT_SIZE)
#define VP9_IF_UV_MODE_COUNT_SIZE      (10*10)
#define VP9_PARTITION_P_COUNT_START    \
	(VP9_IF_UV_MODE_COUNT_START+VP9_IF_UV_MODE_COUNT_SIZE)
#define VP9_PARTITION_P_COUNT_SIZE     (4*4*4)
#define VP9_INTERP_COUNT_START         \
	(VP9_PARTITION_P_COUNT_START+VP9_PARTITION_P_COUNT_SIZE)
#define VP9_INTERP_COUNT_SIZE          (4*3)
#define VP9_MV_JOINTS_COUNT_START      \
	(VP9_INTERP_COUNT_START+VP9_INTERP_COUNT_SIZE)
#define VP9_MV_JOINTS_COUNT_SIZE       (1 * 4)
#define VP9_MV_CLASSES_0_COUNT_START   \
	(VP9_MV_JOINTS_COUNT_START+VP9_MV_JOINTS_COUNT_SIZE)
#define VP9_MV_CLASSES_0_COUNT_SIZE    (1*11)
#define VP9_MV_CLASS0_0_COUNT_START    \
	(VP9_MV_CLASSES_0_COUNT_START+VP9_MV_CLASSES_0_COUNT_SIZE)
#define VP9_MV_CLASS0_0_COUNT_SIZE     (1*2)
#define VP9_MV_CLASSES_1_COUNT_START   \
	(VP9_MV_CLASS0_0_COUNT_START+VP9_MV_CLASS0_0_COUNT_SIZE)
#define VP9_MV_CLASSES_1_COUNT_SIZE    (1*11)
#define VP9_MV_CLASS0_1_COUNT_START    \
	(VP9_MV_CLASSES_1_COUNT_START+VP9_MV_CLASSES_1_COUNT_SIZE)
#define VP9_MV_CLASS0_1_COUNT_SIZE     (1*2)
#define VP9_MV_CLASS0_FP_0_COUNT_START \
	(VP9_MV_CLASS0_1_COUNT_START+VP9_MV_CLASS0_1_COUNT_SIZE)
#define VP9_MV_CLASS0_FP_0_COUNT_SIZE  (3*4)
#define VP9_MV_CLASS0_FP_1_COUNT_START \
	(VP9_MV_CLASS0_FP_0_COUNT_START+VP9_MV_CLASS0_FP_0_COUNT_SIZE)
#define VP9_MV_CLASS0_FP_1_COUNT_SIZE  (3*4)


#define DC_PRED    0       /* Average of above and left pixels*/
#define V_PRED     1       /* Vertical*/
#define H_PRED     2       /* Horizontal*/
#define D45_PRED   3       /*Directional 45 deg = round(arctan(1/1) * 180/pi)*/
#define D135_PRED  4       /* Directional 135 deg = 180 - 45*/
#define D117_PRED  5       /* Directional 117 deg = 180 - 63*/
#define D153_PRED  6       /* Directional 153 deg = 180 - 27*/
#define D207_PRED  7       /* Directional 207 deg = 180 + 27*/
#define D63_PRED   8       /*Directional 63 deg = round(arctan(2/1) * 180/pi)*/
#define TM_PRED    9       /*True-motion*/

int clip_prob(int p)
{
	return (p > 255) ? 255 : (p < 1) ? 1 : p;
}

#define ROUND_POWER_OF_TWO(value, n) \
	(((value) + (1 << ((n) - 1))) >> (n))

#define MODE_MV_COUNT_SAT 20
static const int count_to_update_factor[MODE_MV_COUNT_SAT + 1] = {
	0, 6, 12, 19, 25, 32, 38, 44, 51, 57, 64,
	70, 76, 83, 89, 96, 102, 108, 115, 121, 128
};

void   vp9_tree_merge_probs(unsigned int *prev_prob, unsigned int *cur_prob,
	int coef_node_start, int tree_left, int tree_right, int tree_i,
	int node) {

	int prob_32, prob_res, prob_shift;
	int pre_prob, new_prob;
	int den, m_count, get_prob, factor;
	prob_32 = prev_prob[coef_node_start / 4 * 2];
	prob_res = coef_node_start & 3;
	prob_shift = prob_res * 8;
	pre_prob = (prob_32 >> prob_shift) & 0xff;

	den = tree_left + tree_right;

	if (den == 0)
		new_prob = pre_prob;
	else {
		m_count = (den < MODE_MV_COUNT_SAT) ?
			den : MODE_MV_COUNT_SAT;
		get_prob = clip_prob(
				div_r32(((int64_t)tree_left * 256 + (den >> 1)),
				den));
		/*weighted_prob*/
		factor = count_to_update_factor[m_count];
		new_prob = ROUND_POWER_OF_TWO(pre_prob * (256 - factor)
				+ get_prob * factor, 8);
	}
	cur_prob[coef_node_start / 4 * 2] = (cur_prob[coef_node_start / 4 * 2]
			& (~(0xff << prob_shift))) | (new_prob << prob_shift);

	/*pr_info(" - [%d][%d] 0x%02X --> 0x%02X (0x%X 0x%X) (%X)\n",
	tree_i, node, pre_prob, new_prob, tree_left, tree_right,
	cur_prob[coef_node_start/4*2]);*/
}


/*void adapt_coef_probs(void)*/
void adapt_coef_probs(int pic_count, int prev_kf, int cur_kf, int pre_fc,
	unsigned int *prev_prob, unsigned int *cur_prob, unsigned int *count)
{
	/* 80 * 64bits = 0xF00 ( use 0x1000 4K bytes)
	unsigned int prev_prob[496*2];
	unsigned int cur_prob[496*2];
	0x300 * 128bits = 0x3000 (32K Bytes)
	unsigned int count[0x300*4];*/

	int tx_size, coef_tx_size_start, coef_count_tx_size_start;
	int plane, coef_plane_start, coef_count_plane_start;
	int type, coef_type_start, coef_count_type_start;
	int band, coef_band_start, coef_count_band_start;
	int cxt_num;
	int cxt, coef_cxt_start, coef_count_cxt_start;
	int node, coef_node_start, coef_count_node_start;

	int tree_i, tree_left, tree_right;
	int mvd_i;

	int count_sat = 24;
	/*int update_factor = 112;*/ /*If COEF_MAX_UPDATE_FACTOR_AFTER_KEY,
	use 128*/
	/* If COEF_MAX_UPDATE_FACTOR_AFTER_KEY, use 128*/
	/*int update_factor = (pic_count == 1) ? 128 : 112;*/
	int update_factor =   cur_kf ? 112 :
			prev_kf ? 128 : 112;

	int prob_32;
	int prob_res;
	int prob_shift;
	int pre_prob;

	int num, den;
	int get_prob;
	int m_count;
	int factor;

	int new_prob;

	if (debug & VP9_DEBUG_MERGE)
		pr_info
	("\n ##adapt_coef_probs (pre_fc : %d ,prev_kf : %d,cur_kf : %d)##\n\n",
	pre_fc, prev_kf, cur_kf);

	/*adapt_coef_probs*/
	for (tx_size = 0; tx_size < 4; tx_size++) {
		coef_tx_size_start = VP9_COEF_START
			+ tx_size * 4 * VP9_COEF_SIZE_ONE_SET;
		coef_count_tx_size_start = VP9_COEF_COUNT_START
			+ tx_size * 4 * VP9_COEF_COUNT_SIZE_ONE_SET;
		coef_plane_start = coef_tx_size_start;
		coef_count_plane_start = coef_count_tx_size_start;
		for (plane = 0; plane < 2; plane++) {
			coef_type_start = coef_plane_start;
			coef_count_type_start = coef_count_plane_start;
			for (type = 0; type < 2; type++) {
				coef_band_start = coef_type_start;
				coef_count_band_start = coef_count_type_start;
				for (band = 0; band < 6; band++) {
					if (band == 0)
						cxt_num = 3;
					else
						cxt_num = 6;
					coef_cxt_start = coef_band_start;
					coef_count_cxt_start =
						coef_count_band_start;
					for (cxt = 0; cxt < cxt_num; cxt++) {
						const int n0 =
						count[coef_count_cxt_start];
						const int n1 =
						count[coef_count_cxt_start + 1];
						const int n2 =
						count[coef_count_cxt_start + 2];
						const int neob =
						count[coef_count_cxt_start + 3];
						const int nneob =
						count[coef_count_cxt_start + 4];
						const unsigned int
						branch_ct[3][2] = {
						{ neob, nneob },
						{ n0, n1 + n2 },
						{ n1, n2 }
						};
						coef_node_start =
							coef_cxt_start;
						for
						(node = 0; node < 3; node++) {
							prob_32 =
							prev_prob[
							coef_node_start
							/ 4 * 2];
							prob_res =
							coef_node_start & 3;
							prob_shift =
							prob_res * 8;
							pre_prob =
							(prob_32 >> prob_shift)
							& 0xff;

							/*get_binary_prob*/
							num =
							branch_ct[node][0];
							den =
							branch_ct[node][0] +
							 branch_ct[node][1];
							m_count = (den <
							count_sat)
							? den : count_sat;

							get_prob =
							(den == 0) ? 128u :
							clip_prob(
							div_r32(((int64_t)
							num * 256
							+ (den >> 1)),
							den));

							factor =
							update_factor * m_count
							/ count_sat;
							new_prob =
							ROUND_POWER_OF_TWO
							(pre_prob *
							(256 - factor) +
							get_prob * factor, 8);

							cur_prob[coef_node_start
							/ 4 * 2] =
							(cur_prob
							[coef_node_start
							/ 4 * 2] & (~(0xff <<
							prob_shift))) |
							(new_prob <<
							prob_shift);

							coef_node_start += 1;
						}

						coef_cxt_start =
							coef_cxt_start + 3;
						coef_count_cxt_start =
							coef_count_cxt_start
							+ 5;
					}
					if (band == 0) {
						coef_band_start += 10;
						coef_count_band_start += 15;
					} else {
						coef_band_start += 18;
						coef_count_band_start += 30;
					}
				}
				coef_type_start += VP9_COEF_SIZE_ONE_SET;
				coef_count_type_start +=
					VP9_COEF_COUNT_SIZE_ONE_SET;
			}
			coef_plane_start += 2 * VP9_COEF_SIZE_ONE_SET;
			coef_count_plane_start +=
					2 * VP9_COEF_COUNT_SIZE_ONE_SET;
		}
	}

if (cur_kf == 0) {
		/*mode_mv_merge_probs - merge_intra_inter_prob*/
		for (coef_count_node_start = VP9_INTRA_INTER_COUNT_START;
		coef_count_node_start < (VP9_MV_CLASS0_HP_1_COUNT_START +
		VP9_MV_CLASS0_HP_1_COUNT_SIZE);	coef_count_node_start += 2) {

			if (coef_count_node_start ==
				VP9_INTRA_INTER_COUNT_START) {
				if (debug & VP9_DEBUG_MERGE)
					pr_info(" # merge_intra_inter_prob\n");
				coef_node_start = VP9_INTRA_INTER_START;
			} else if (coef_count_node_start ==
				VP9_COMP_INTER_COUNT_START) {
				if (debug & VP9_DEBUG_MERGE)
					pr_info(" # merge_comp_inter_prob\n");
				coef_node_start = VP9_COMP_INTER_START;
			}
			/*
			else if (coef_count_node_start ==
				VP9_COMP_REF_COUNT_START) {
				pr_info(" # merge_comp_inter_prob\n");
				coef_node_start = VP9_COMP_REF_START;
			}
			else if (coef_count_node_start ==
				VP9_SINGLE_REF_COUNT_START) {
				pr_info(" # merge_comp_inter_prob\n");
				coef_node_start = VP9_SINGLE_REF_START;
			}
			*/
			else if (coef_count_node_start ==
				VP9_TX_MODE_COUNT_START) {
				if (debug & VP9_DEBUG_MERGE)
					pr_info(" # merge_tx_mode_probs\n");
				coef_node_start = VP9_TX_MODE_START;
			} else if (coef_count_node_start ==
				VP9_SKIP_COUNT_START) {
				if (debug & VP9_DEBUG_MERGE)
					pr_info(" # merge_skip_probs\n");
				coef_node_start = VP9_SKIP_START;
			} else if (coef_count_node_start ==
				VP9_MV_SIGN_0_COUNT_START) {
				if (debug & VP9_DEBUG_MERGE)
					pr_info(" # merge_sign_0\n");
				coef_node_start = VP9_MV_SIGN_0_START;
			} else if (coef_count_node_start ==
				VP9_MV_SIGN_1_COUNT_START) {
				if (debug & VP9_DEBUG_MERGE)
					pr_info(" # merge_sign_1\n");
				coef_node_start = VP9_MV_SIGN_1_START;
			} else if (coef_count_node_start ==
				VP9_MV_BITS_0_COUNT_START) {
				if (debug & VP9_DEBUG_MERGE)
					pr_info(" # merge_bits_0\n");
				coef_node_start = VP9_MV_BITS_0_START;
			} else if (coef_count_node_start ==
				VP9_MV_BITS_1_COUNT_START) {
				if (debug & VP9_DEBUG_MERGE)
					pr_info(" # merge_bits_1\n");
				coef_node_start = VP9_MV_BITS_1_START;
			} else if (coef_count_node_start ==
					VP9_MV_CLASS0_HP_0_COUNT_START) {
				if (debug & VP9_DEBUG_MERGE)
					pr_info(" # merge_class0_hp\n");
				coef_node_start = VP9_MV_CLASS0_HP_0_START;
			}


		den = count[coef_count_node_start] +
			count[coef_count_node_start + 1];

		prob_32 = prev_prob[coef_node_start / 4 * 2];
		prob_res = coef_node_start & 3;
		prob_shift = prob_res * 8;
		pre_prob = (prob_32 >> prob_shift) & 0xff;

		if (den == 0)
			new_prob = pre_prob;
		else {
			m_count = (den < MODE_MV_COUNT_SAT) ?
				den : MODE_MV_COUNT_SAT;
			get_prob =
				clip_prob(
				div_r32(((int64_t)count[coef_count_node_start]
				* 256 + (den >> 1)),
				den));
			/*weighted_prob*/
			factor = count_to_update_factor[m_count];
			new_prob =
				ROUND_POWER_OF_TWO(pre_prob * (256 - factor)
				+ get_prob * factor, 8);
		}
		cur_prob[coef_node_start / 4 * 2] =
			(cur_prob[coef_node_start / 4 * 2] &
			(~(0xff << prob_shift)))
			| (new_prob << prob_shift);

		coef_node_start = coef_node_start + 1;
	}
	if (debug & VP9_DEBUG_MERGE)
		pr_info(" # merge_vp9_inter_mode_tree\n");
	coef_node_start = VP9_INTER_MODE_START;
	coef_count_node_start = VP9_INTER_MODE_COUNT_START;
	for (tree_i = 0; tree_i < 7; tree_i++) {
		for (node = 0; node < 3; node++) {
			switch (node) {
			case 2:
				tree_left =
				count[coef_count_node_start + 1];
				tree_right =
				count[coef_count_node_start + 3];
				break;
			case 1:
				tree_left =
				count[coef_count_node_start + 0];
				tree_right =
				count[coef_count_node_start + 1]
				+ count[coef_count_node_start + 3];
				break;
			default:
				tree_left =
				count[coef_count_node_start + 2];
				tree_right =
				count[coef_count_node_start + 0]
				+ count[coef_count_node_start + 1]
				+ count[coef_count_node_start + 3];
				break;

			}

			vp9_tree_merge_probs(prev_prob, cur_prob,
				coef_node_start, tree_left, tree_right,
				tree_i, node);

			coef_node_start = coef_node_start + 1;
		}
		coef_count_node_start = coef_count_node_start + 4;
	}
	if (debug & VP9_DEBUG_MERGE)
		pr_info(" # merge_vp9_intra_mode_tree\n");
	coef_node_start = VP9_IF_Y_MODE_START;
	coef_count_node_start = VP9_IF_Y_MODE_COUNT_START;
	for (tree_i = 0; tree_i < 14; tree_i++) {
		for (node = 0; node < 9; node++) {
			switch (node) {
			case 8:
				tree_left =
				count[coef_count_node_start+D153_PRED];
				tree_right =
				count[coef_count_node_start+D207_PRED];
				break;
			case 7:
				tree_left =
				count[coef_count_node_start+D63_PRED];
				tree_right =
				count[coef_count_node_start+D207_PRED] +
				count[coef_count_node_start+D153_PRED];
				break;
			case 6:
				tree_left =
				count[coef_count_node_start + D45_PRED];
				tree_right =
				count[coef_count_node_start+D207_PRED] +
				count[coef_count_node_start+D153_PRED] +
				count[coef_count_node_start+D63_PRED];
				break;
			case 5:
				tree_left =
				count[coef_count_node_start+D135_PRED];
				tree_right =
				count[coef_count_node_start+D117_PRED];
				break;
			case 4:
				tree_left =
				count[coef_count_node_start+H_PRED];
				tree_right =
				count[coef_count_node_start+D117_PRED] +
				count[coef_count_node_start+D135_PRED];
				break;
			case 3:
				tree_left =
				count[coef_count_node_start+H_PRED] +
				count[coef_count_node_start+D117_PRED] +
				count[coef_count_node_start+D135_PRED];
				tree_right =
				count[coef_count_node_start+D45_PRED] +
				count[coef_count_node_start+D207_PRED] +
				count[coef_count_node_start+D153_PRED] +
				count[coef_count_node_start+D63_PRED];
				break;
			case 2:
				tree_left =
				count[coef_count_node_start+V_PRED];
				tree_right =
				count[coef_count_node_start+H_PRED] +
				count[coef_count_node_start+D117_PRED] +
				count[coef_count_node_start+D135_PRED] +
				count[coef_count_node_start+D45_PRED] +
				count[coef_count_node_start+D207_PRED] +
				count[coef_count_node_start+D153_PRED] +
				count[coef_count_node_start+D63_PRED];
				break;
			case 1:
				tree_left =
				count[coef_count_node_start+TM_PRED];
				tree_right =
				count[coef_count_node_start+V_PRED] +
				count[coef_count_node_start+H_PRED] +
				count[coef_count_node_start+D117_PRED] +
				count[coef_count_node_start+D135_PRED] +
				count[coef_count_node_start+D45_PRED] +
				count[coef_count_node_start+D207_PRED] +
				count[coef_count_node_start+D153_PRED] +
				count[coef_count_node_start+D63_PRED];
				break;
			default:
				tree_left =
				count[coef_count_node_start+DC_PRED];
				tree_right =
				count[coef_count_node_start+TM_PRED] +
				count[coef_count_node_start+V_PRED] +
				count[coef_count_node_start+H_PRED] +
				count[coef_count_node_start+D117_PRED] +
				count[coef_count_node_start+D135_PRED] +
				count[coef_count_node_start+D45_PRED] +
				count[coef_count_node_start+D207_PRED] +
				count[coef_count_node_start+D153_PRED] +
				count[coef_count_node_start+D63_PRED];
				break;

				}

			vp9_tree_merge_probs(prev_prob, cur_prob,
				coef_node_start, tree_left, tree_right,
				tree_i, node);

			coef_node_start = coef_node_start + 1;
		}
		coef_count_node_start = coef_count_node_start + 10;
	}

	if (debug & VP9_DEBUG_MERGE)
		pr_info(" # merge_vp9_partition_tree\n");
	coef_node_start = VP9_PARTITION_P_START;
	coef_count_node_start = VP9_PARTITION_P_COUNT_START;
	for (tree_i = 0; tree_i < 16; tree_i++) {
		for (node = 0; node < 3; node++) {
			switch (node) {
			case 2:
				tree_left =
				count[coef_count_node_start + 2];
				tree_right =
				count[coef_count_node_start + 3];
				break;
			case 1:
				tree_left =
				count[coef_count_node_start + 1];
				tree_right =
				count[coef_count_node_start + 2] +
				count[coef_count_node_start + 3];
				break;
			default:
				tree_left =
				count[coef_count_node_start + 0];
				tree_right =
				count[coef_count_node_start + 1] +
				count[coef_count_node_start + 2] +
				count[coef_count_node_start + 3];
				break;

			}

			vp9_tree_merge_probs(prev_prob, cur_prob,
				coef_node_start,
				tree_left, tree_right, tree_i, node);

			coef_node_start = coef_node_start + 1;
		}
		coef_count_node_start = coef_count_node_start + 4;
	}

	if (debug & VP9_DEBUG_MERGE)
		pr_info(" # merge_vp9_switchable_interp_tree\n");
	coef_node_start = VP9_INTERP_START;
	coef_count_node_start = VP9_INTERP_COUNT_START;
	for (tree_i = 0; tree_i < 4; tree_i++) {
		for (node = 0; node < 2; node++) {
			switch (node) {
			case 1:
				tree_left =
				count[coef_count_node_start + 1];
				tree_right =
				count[coef_count_node_start + 2];
				break;
			default:
				tree_left =
				count[coef_count_node_start + 0];
				tree_right =
				count[coef_count_node_start + 1] +
				count[coef_count_node_start + 2];
				break;

			}

			vp9_tree_merge_probs(prev_prob, cur_prob,
				coef_node_start,
				tree_left, tree_right, tree_i, node);

			coef_node_start = coef_node_start + 1;
		}
		coef_count_node_start = coef_count_node_start + 3;
	}

	if (debug & VP9_DEBUG_MERGE)
		pr_info("# merge_vp9_mv_joint_tree\n");
	coef_node_start = VP9_MV_JOINTS_START;
	coef_count_node_start = VP9_MV_JOINTS_COUNT_START;
	for (tree_i = 0; tree_i < 1; tree_i++) {
		for (node = 0; node < 3; node++) {
			switch (node) {
			case 2:
				tree_left =
				count[coef_count_node_start + 2];
				tree_right =
				count[coef_count_node_start + 3];
				break;
			case 1:
				tree_left =
				count[coef_count_node_start + 1];
				tree_right =
				count[coef_count_node_start + 2] +
				count[coef_count_node_start + 3];
				break;
			default:
				tree_left =
				count[coef_count_node_start + 0];
				tree_right =
				count[coef_count_node_start + 1] +
				count[coef_count_node_start + 2] +
				count[coef_count_node_start + 3];
				break;
			}

			vp9_tree_merge_probs(prev_prob, cur_prob,
				coef_node_start,
				tree_left, tree_right, tree_i, node);

			coef_node_start = coef_node_start + 1;
		}
		coef_count_node_start = coef_count_node_start + 4;
	}

	for (mvd_i = 0; mvd_i < 2; mvd_i++) {
		if (debug & VP9_DEBUG_MERGE)
			pr_info(" # merge_vp9_mv_class_tree [%d]  -\n", mvd_i);
		coef_node_start =
			mvd_i ? VP9_MV_CLASSES_1_START : VP9_MV_CLASSES_0_START;
		coef_count_node_start =
			mvd_i ? VP9_MV_CLASSES_1_COUNT_START
			: VP9_MV_CLASSES_0_COUNT_START;
		tree_i = 0;
		for (node = 0; node < 10; node++) {
			switch (node) {
			case 9:
				tree_left =
				count[coef_count_node_start + 9];
				tree_right =
				count[coef_count_node_start + 10];
				break;
			case 8:
				tree_left =
				count[coef_count_node_start + 7];
				tree_right =
				count[coef_count_node_start + 8];
				break;
			case 7:
				tree_left =
				count[coef_count_node_start + 7] +
				count[coef_count_node_start + 8];
				tree_right =
				count[coef_count_node_start + 9] +
				count[coef_count_node_start + 10];
				break;
			case 6:
				tree_left =
				count[coef_count_node_start + 6];
				tree_right =
				count[coef_count_node_start + 7] +
				count[coef_count_node_start + 8] +
				count[coef_count_node_start + 9] +
				count[coef_count_node_start + 10];
				break;
			case 5:
				tree_left =
				count[coef_count_node_start + 4];
				tree_right =
				count[coef_count_node_start + 5];
				break;
			case 4:
				tree_left =
				count[coef_count_node_start + 4] +
				count[coef_count_node_start + 5];
				tree_right =
				count[coef_count_node_start + 6] +
				count[coef_count_node_start + 7] +
				count[coef_count_node_start + 8] +
				count[coef_count_node_start + 9] +
				count[coef_count_node_start + 10];
				break;
			case 3:
				tree_left =
				count[coef_count_node_start + 2];
				tree_right =
				count[coef_count_node_start + 3];
				break;
			case 2:
				tree_left =
				count[coef_count_node_start + 2] +
				count[coef_count_node_start + 3];
				tree_right =
				count[coef_count_node_start + 4] +
				count[coef_count_node_start + 5] +
				count[coef_count_node_start + 6] +
				count[coef_count_node_start + 7] +
				count[coef_count_node_start + 8] +
				count[coef_count_node_start + 9] +
				count[coef_count_node_start + 10];
				break;
			case 1:
				tree_left =
				count[coef_count_node_start + 1];
				tree_right =
				count[coef_count_node_start + 2] +
				count[coef_count_node_start + 3] +
				count[coef_count_node_start + 4] +
				count[coef_count_node_start + 5] +
				count[coef_count_node_start + 6] +
				count[coef_count_node_start + 7] +
				count[coef_count_node_start + 8] +
				count[coef_count_node_start + 9] +
				count[coef_count_node_start + 10];
				break;
			default:
				tree_left =
				count[coef_count_node_start + 0];
				tree_right =
				count[coef_count_node_start + 1] +
				count[coef_count_node_start + 2] +
				count[coef_count_node_start + 3] +
				count[coef_count_node_start + 4] +
				count[coef_count_node_start + 5] +
				count[coef_count_node_start + 6] +
				count[coef_count_node_start + 7] +
				count[coef_count_node_start + 8] +
				count[coef_count_node_start + 9] +
				count[coef_count_node_start + 10];
				break;

			}

			vp9_tree_merge_probs(prev_prob, cur_prob,
				coef_node_start, tree_left, tree_right,
				tree_i, node);

			coef_node_start = coef_node_start + 1;
		}

		if (debug & VP9_DEBUG_MERGE)
			pr_info(" # merge_vp9_mv_class0_tree [%d]  -\n", mvd_i);
		coef_node_start =
			mvd_i ? VP9_MV_CLASS0_1_START : VP9_MV_CLASS0_0_START;
		coef_count_node_start =
			mvd_i ? VP9_MV_CLASS0_1_COUNT_START :
			VP9_MV_CLASS0_0_COUNT_START;
		tree_i = 0;
		node = 0;
		tree_left = count[coef_count_node_start + 0];
		tree_right = count[coef_count_node_start + 1];

		vp9_tree_merge_probs(prev_prob, cur_prob, coef_node_start,
			tree_left, tree_right, tree_i, node);
		if (debug & VP9_DEBUG_MERGE)
			pr_info(" # merge_vp9_mv_fp_tree_class0_fp [%d]  -\n",
				mvd_i);
		coef_node_start =
			mvd_i ? VP9_MV_CLASS0_FP_1_START :
			VP9_MV_CLASS0_FP_0_START;
		coef_count_node_start =
			mvd_i ? VP9_MV_CLASS0_FP_1_COUNT_START :
			VP9_MV_CLASS0_FP_0_COUNT_START;
		for (tree_i = 0; tree_i < 3; tree_i++) {
			for (node = 0; node < 3; node++) {
				switch (node) {
				case 2:
					tree_left =
					count[coef_count_node_start + 2];
					tree_right =
					count[coef_count_node_start + 3];
					break;
				case 1:
					tree_left =
					count[coef_count_node_start + 1];
					tree_right =
					count[coef_count_node_start + 2]
					+ count[coef_count_node_start + 3];
					break;
				default:
					tree_left =
					count[coef_count_node_start + 0];
					tree_right =
					count[coef_count_node_start + 1]
					+ count[coef_count_node_start + 2]
					+ count[coef_count_node_start + 3];
					break;

				}

				vp9_tree_merge_probs(prev_prob, cur_prob,
					coef_node_start, tree_left, tree_right,
					tree_i, node);

				coef_node_start = coef_node_start + 1;
			}
			coef_count_node_start = coef_count_node_start + 4;
		}

	} /* for mvd_i (mvd_y or mvd_x)*/
}

}


static void uninit_mmu_buffers(struct VP9Decoder_s *pbi)
{

	decoder_mmu_box_free(pbi->mmu_box);
	pbi->mmu_box = NULL;

	if (pbi->bmmu_box)
		decoder_bmmu_box_free(pbi->bmmu_box);
	pbi->bmmu_box = NULL;
}

#ifndef VP9_10B_MMU
static void init_buf_list(struct VP9Decoder_s *pbi)
{
	int i;
	int buf_size;
#ifndef VP9_10B_MMU
	int mc_buffer_end = pbi->mc_buf->buf_start + pbi->mc_buf->buf_size;
#endif
	pbi->used_buf_num = max_buf_num;

	if (pbi->used_buf_num > MAX_BUF_NUM)
		pbi->used_buf_num = MAX_BUF_NUM;
	if (buf_alloc_size > 0) {
		buf_size = buf_alloc_size;
		if (debug)
			pr_info("[Buffer Management] init_buf_list:\n");
	} else {
		int pic_width = pbi->init_pic_w;
		int pic_height = pbi->init_pic_h;

	/*SUPPORT_10BIT*/
	int losless_comp_header_size = compute_losless_comp_header_size
			(pic_width, pic_height);
	int losless_comp_body_size = compute_losless_comp_body_size
			(pic_width, pic_height, buf_alloc_depth == 10);
	int mc_buffer_size = losless_comp_header_size
		+ losless_comp_body_size;
	int mc_buffer_size_h = (mc_buffer_size + 0xffff)>>16;
	if (double_write_mode) {
		int pic_width_dw = (double_write_mode == 2) ?
			pic_width / 2 : pic_width;
		int pic_height_dw = (double_write_mode == 2) ?
			pic_height / 2 : pic_height;
		int lcu_size = 64; /*fixed 64*/
		int pic_width_64 = (pic_width_dw + 63) & (~0x3f);
		int pic_height_32 = (pic_height_dw + 31) & (~0x1f);
		int pic_width_lcu  =
			(pic_width_64 % lcu_size) ? pic_width_64 / lcu_size
			+ 1 : pic_width_64 / lcu_size;
		int pic_height_lcu =
			(pic_height_32 % lcu_size) ? pic_height_32 / lcu_size
			+ 1 : pic_height_32 / lcu_size;
		int lcu_total       = pic_width_lcu * pic_height_lcu;
		int mc_buffer_size_u_v = lcu_total * lcu_size * lcu_size / 2;
		int mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff) >> 16;
			/*64k alignment*/
		buf_size = ((mc_buffer_size_u_v_h << 16) * 3);
	} else
		buf_size = 0;

	if (mc_buffer_size & 0xffff) { /*64k alignment*/
		mc_buffer_size_h += 1;
	}
	if ((double_write_mode & 0x10) == 0)
		buf_size += (mc_buffer_size_h << 16);
		if (debug) {
			pr_info
			("init_buf_list num %d (width %d height %d):\n",
			 pbi->used_buf_num, pic_width, pic_height);
		}
	}

	for (i = 0; i < pbi->used_buf_num; i++) {
		if (((i + 1) * buf_size) > pbi->mc_buf->buf_size) {
			if (use_cma)
				pbi->use_cma_flag = 1;
			else {
				if (debug) {
					pr_info("%s maximum buf size is used\n",
						   __func__);
				}
				break;
			}
		}
#ifndef VP9_10B_MMU
		pbi->m_BUF[i].alloc_flag = 0;
		pbi->m_BUF[i].index = i;

		if (use_cma == 2)
			pbi->use_cma_flag = 1;
		if (pbi->use_cma_flag) {
			if (!decoder_bmmu_box_alloc_idx_wait(
					pbi->bmmu_box,
					i,
					buf_size,
					-1,
					-1,
					BMMU_ALLOC_FLAGS_WAITCLEAR)) {
				pbi->m_BUF[i].alloc_addr =
					decoder_bmmu_box_get_phy_addr(
						pbi->bmmu_box,
						i);
				pbi->m_BUF[i].cma_page_count =
					PAGE_ALIGN(buf_size) / PAGE_SIZE;
				pr_info("CMA malloc ok  %d\n", i);
			} else {
				pbi->m_BUF[i].cma_page_count = 0;
				pr_info("CMA malloc failed  %d\n", i);
				if (i <= 5) {
					pbi->fatal_error |=
					DECODER_FATAL_ERROR_NO_MEM;
				}
				break;
			}
			pbi->m_BUF[i].start_adr =  pbi->m_BUF[i].alloc_addr;
		} else {
			pbi->m_BUF[i].cma_page_count = 0;
			pbi->m_BUF[i].alloc_addr = 0;
			pbi->m_BUF[i].start_adr =
				pbi->mc_buf->buf_start + i * buf_size;
		}
		pbi->m_BUF[i].size = buf_size;
		pbi->m_BUF[i].free_start_adr = pbi->m_BUF[i].start_adr;

		if (((pbi->m_BUF[i].start_adr + buf_size) > mc_buffer_end)
			&& (pbi->m_BUF[i].alloc_addr == 0)) {
			if (debug) {
				pr_info
				("Max mc buffer or mpred_mv buffer is used\n");
			}
			break;
		}

		if (debug) {
			pr_info("Buffer %d: start_adr %p size %x\n", i,
				   (void *)pbi->m_BUF[i].start_adr,
				   pbi->m_BUF[i].size);
		}
#endif
	}
	pbi->buf_num = i;
}
#endif
static int config_pic(struct VP9Decoder_s *pbi,
				struct PIC_BUFFER_CONFIG_s *pic_config,
				unsigned long last_disp_addr)
{
	int ret = -1;
	int i;
	int pic_width = pbi->init_pic_w;
	int pic_height = pbi->init_pic_h;
	int MV_MEM_UNIT = 0x240;
	int lcu_size = 64; /*fixed 64*/
	int pic_width_64 = (pic_width + 63) & (~0x3f);
	int pic_height_32 = (pic_height + 31) & (~0x1f);
	int pic_width_lcu  = (pic_width_64 % lcu_size) ?
				pic_width_64 / lcu_size  + 1
				: pic_width_64 / lcu_size;
	int pic_height_lcu = (pic_height_32 % lcu_size) ?
				pic_height_32 / lcu_size + 1
				: pic_height_32 / lcu_size;
	int lcu_total       = pic_width_lcu * pic_height_lcu;

	u32 mpred_mv_end = pbi->work_space_buf->mpred_mv.buf_start +
			pbi->work_space_buf->mpred_mv.buf_size;
	u32 y_adr = 0;
	int buf_size = 0;

	int losless_comp_header_size =
			compute_losless_comp_header_size(pic_width ,
			pic_height);
	int losless_comp_body_size = compute_losless_comp_body_size(pic_width ,
			pic_height, buf_alloc_depth == 10);
	int mc_buffer_size = losless_comp_header_size + losless_comp_body_size;
	int mc_buffer_size_h = (mc_buffer_size + 0xffff) >> 16;
	int mc_buffer_size_u_v = 0;
	int mc_buffer_size_u_v_h = 0;
	if (double_write_mode) {
		int pic_width_dw = (double_write_mode == 2) ?
			pic_width / 2 : pic_width;
		int pic_height_dw = (double_write_mode == 2) ?
			pic_height / 2 : pic_height;
		int pic_width_64_dw = (pic_width_dw + 63) & (~0x3f);
		int pic_height_32_dw = (pic_height_dw + 31) & (~0x1f);
		int pic_width_lcu_dw  = (pic_width_64_dw % lcu_size) ?
					pic_width_64_dw / lcu_size  + 1
					: pic_width_64_dw / lcu_size;
		int pic_height_lcu_dw = (pic_height_32_dw % lcu_size) ?
					pic_height_32_dw / lcu_size + 1
					: pic_height_32_dw / lcu_size;
		int lcu_total_dw       = pic_width_lcu_dw * pic_height_lcu_dw;

		mc_buffer_size_u_v = lcu_total_dw * lcu_size * lcu_size / 2;
		mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff) >> 16;
		/*64k alignment*/
		buf_size = ((mc_buffer_size_u_v_h << 16) * 3);
		buf_size = ((buf_size + 0xffff) >> 16) << 16;
	}
	if (mc_buffer_size & 0xffff) /*64k alignment*/
		mc_buffer_size_h += 1;
#ifndef VP9_10B_MMU
	if ((double_write_mode & 0x10) == 0)
		buf_size += (mc_buffer_size_h << 16);
#endif

#ifdef VP9_10B_MMU
	if ((pbi->work_space_buf->cm_header.buf_start +
		((pic_config->index + 2)
		* MMU_COMPRESS_HEADER_SIZE))
		> (pbi->work_space_buf->cm_header.buf_start +
		pbi->work_space_buf->cm_header.buf_size)) {
		pr_info("MMU header_adr allocate fail\n");
		return -1;
	}

	pic_config->header_adr = pbi->work_space_buf->cm_header.buf_start
		+ (pic_config->index * MMU_COMPRESS_HEADER_SIZE);
	if (last_disp_addr && pic_config->header_adr == last_disp_addr) {
		/*if same as disp add used last one.*/
		pr_info("same as disp %d: %ld\n",
			pic_config->index, pic_config->header_adr);
		pic_config->header_adr =
			pbi->work_space_buf->cm_header.buf_start +
			(FRAME_BUFFERS * MMU_COMPRESS_HEADER_SIZE);
	}
	if (debug & VP9_DEBUG_BUFMGR) {
		pr_info("MMU header_adr %d: %ld\n",
			pic_config->index, pic_config->header_adr);
	}
#endif

	i = pic_config->index;
	if ((pbi->work_space_buf->mpred_mv.buf_start +
		(((i + 1) * lcu_total) * MV_MEM_UNIT))
		<= mpred_mv_end
#ifdef VP9_10B_MMU
#endif
	) {
		if (debug) {
			pr_err("start %x  .size=%d\n",
				pbi->mc_buf_spec.buf_start + i * buf_size,
				buf_size);
		}

#ifndef VP9_10B_MMU
		for (i = 0; i < pbi->buf_num; i++) {
			y_adr = ((pbi->m_BUF[i].free_start_adr
				+ 0xffff) >> 16) << 16;
			/*64k alignment*/
			if ((y_adr+buf_size) <=	(pbi->m_BUF[i].start_adr+
				pbi->m_BUF[i].size)) {
				pbi->m_BUF[i].free_start_adr =
					y_adr + buf_size;
				break;
			}
		}
		if (i < pbi->buf_num)
#else
		if ((pbi->mc_buf->buf_start + (i + 1) * buf_size) <
			pbi->mc_buf->buf_end)
			y_adr = pbi->mc_buf->buf_start + i * buf_size;
		else {
			if (!decoder_bmmu_box_alloc_idx_wait(
					pbi->bmmu_box,
					i,
					buf_size,
					-1,
					-1,
					BMMU_ALLOC_FLAGS_WAITCLEAR
					)) {
				pic_config->cma_alloc_addr =
					decoder_bmmu_box_get_phy_addr(
						pbi->bmmu_box,
						i);
			} else {
				pr_err("alloc cma buffer failed %d\n", i);
			}
			if (pic_config->cma_alloc_addr)
				y_adr = pic_config->cma_alloc_addr;
			else
				return -1;
		}
#endif
		{
			/*ensure get_pic_by_POC()
			not get the buffer not decoded*/
			pic_config->BUF_index = i;
			pic_config->lcu_total = lcu_total;

			pic_config->comp_body_size = losless_comp_body_size;
			pic_config->buf_size = buf_size;
#ifndef VP9_10B_MMU
			pic_config->mc_y_adr = y_adr;
#endif
			pic_config->mc_canvas_y = pic_config->index;
			pic_config->mc_canvas_u_v = pic_config->index;
#ifndef VP9_10B_MMU
			if (double_write_mode & 0x10) {
				pic_config->mc_u_v_adr = y_adr +
				((mc_buffer_size_u_v_h << 16) << 1);

				pic_config->mc_canvas_y =
					(pic_config->index << 1);
				pic_config->mc_canvas_u_v =
					(pic_config->index << 1) + 1;

				pic_config->dw_y_adr = y_adr;
				pic_config->dw_u_v_adr = pic_config->mc_u_v_adr;
			} else
#endif
			if (double_write_mode) {
				pic_config->dw_y_adr = y_adr
#ifndef VP9_10B_MMU
				+ (mc_buffer_size_h << 16)
#endif
				;
				pic_config->dw_u_v_adr = pic_config->dw_y_adr +
					((mc_buffer_size_u_v_h << 16) << 1);
			}

			pic_config->mpred_mv_wr_start_addr =
			pbi->work_space_buf->mpred_mv.buf_start +
					((pic_config->index * lcu_total)
					* MV_MEM_UNIT);

			if (debug) {
				pr_info
				("%s index %d BUF_index %d mc_y_adr %lx ",
				__func__, pic_config->index,
				pic_config->BUF_index,
				pic_config->mc_y_adr);
				pr_info
				("comp_body_size %x comp_buf_size %x ",
				pic_config->comp_body_size,
				pic_config->buf_size);
				pr_info
				("mpred_mv_wr_start_adr %ld\n",
				pic_config->mpred_mv_wr_start_addr);
				pr_info("dw_y_adr %d, pic_config->dw_u_v_adr =%d\n",
					pic_config->dw_y_adr,
					pic_config->dw_u_v_adr);
			}
			ret = 0;
		}
	}
	return ret;
}

static void init_pic_list(struct VP9Decoder_s *pbi)
{
	int i;
	struct VP9_Common_s *cm = &pbi->common;
	struct PIC_BUFFER_CONFIG_s *pic_config;
	struct vframe_s vf;
	unsigned long disp_addr = 0;
	if (!get_video0_frame_info(&vf)) {
		if (vf.type & VIDTYPE_SCATTER) {
			/*sc only used header.*/
			disp_addr = vf.compHeadAddr;
		} else if (vf.type & VIDTYPE_COMPRESS) {
			/*sc checked body.*/
			disp_addr = vf.compBodyAddr;
		} else {
			struct canvas_s cur_canvas;
			canvas_read(vf.canvas0Addr & 0xff, &cur_canvas);
			disp_addr = cur_canvas.addr;
		}
	}
	for (i = 0; i < FRAME_BUFFERS; i++) {
		pic_config = &cm->buffer_pool->frame_bufs[i].buf;
		pic_config->index = i;
		pic_config->BUF_index = -1;
		if (config_pic(pbi, pic_config, disp_addr) < 0) {
			if (debug)
				pr_info("Config_pic %d fail\n",
					pic_config->index);
			pic_config->index = -1;
			break;
		}
		pic_config->y_crop_width = pbi->init_pic_w;
		pic_config->y_crop_height = pbi->init_pic_h;
		/*set_canvas(pic_config);*/
	}
	for (; i < FRAME_BUFFERS; i++) {
		pic_config = &cm->buffer_pool->frame_bufs[i].buf;
		pic_config->index = -1;
		pic_config->BUF_index = -1;
	}

}


static void init_pic_list_hw(struct VP9Decoder_s *pbi)
{
	int i;
	struct VP9_Common_s *cm = &pbi->common;
	struct PIC_BUFFER_CONFIG_s *pic_config;
	/*WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x0);*/
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR,
		(0x1 << 1) | (0x1 << 2));


	for (i = 0; i < FRAME_BUFFERS; i++) {
		pic_config = &cm->buffer_pool->frame_bufs[i].buf;
		if (pic_config->index < 0)
			break;

#ifdef VP9_10B_MMU
	/*WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
		pic_config->header_adr
		| (pic_config->mc_canvas_y << 8)|0x1);*/
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA, pic_config->header_adr >> 5);
#else
	/*WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
		pic_config->mc_y_adr
		| (pic_config->mc_canvas_y << 8) | 0x1);*/
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA, pic_config->mc_y_adr >> 5);
#endif
#ifndef LOSLESS_COMPRESS_MODE
	/*WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
		pic_config->mc_u_v_adr
		| (pic_config->mc_canvas_u_v << 8)| 0x1);*/
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA, pic_config->mc_u_v_adr >> 5);
#endif
	}
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x1);

	/*Zero out canvas registers in IPP -- avoid simulation X*/
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(0 << 8) | (0 << 1) | 1);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);
}


static void dump_pic_list(struct VP9Decoder_s *pbi)
{
	return;
}

static int config_pic_size(struct VP9Decoder_s *pbi, unsigned short bit_depth)
{
#ifdef LOSLESS_COMPRESS_MODE
	unsigned int data32;
#endif
	int losless_comp_header_size, losless_comp_body_size;
	struct VP9_Common_s *cm = &pbi->common;
	struct PIC_BUFFER_CONFIG_s *cur_pic_config = &cm->cur_frame->buf;
	frame_width = cur_pic_config->y_crop_width;
	frame_height = cur_pic_config->y_crop_height;
	cur_pic_config->bit_depth = bit_depth;
	losless_comp_header_size =
		compute_losless_comp_header_size(cur_pic_config->y_crop_width,
		cur_pic_config->y_crop_height);
	losless_comp_body_size =
		compute_losless_comp_body_size(cur_pic_config->y_crop_width,
		cur_pic_config->y_crop_height, (bit_depth == VPX_BITS_10));
	cur_pic_config->comp_body_size = losless_comp_body_size;
#ifdef LOSLESS_COMPRESS_MODE
	data32 = READ_VREG(HEVC_SAO_CTRL5);
	if (bit_depth == VPX_BITS_10)
		data32 &= ~(1 << 9);
	else
		data32 |= (1 << 9);

	WRITE_VREG(HEVC_SAO_CTRL5, data32);

#ifdef VP9_10B_MMU
	/*bit[4] : paged_mem_mode*/
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0x1 << 4));
#else
	/*bit[3] smem mdoe*/
	if (bit_depth == VPX_BITS_10)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0 << 3));
	else
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (1 << 3));
#endif
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, (losless_comp_body_size >> 5));
	/*WRITE_VREG(HEVCD_MPP_DECOMP_CTL3,(0xff<<20) | (0xff<<10) | 0xff);*/
	WRITE_VREG(HEVC_CM_BODY_LENGTH, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH, losless_comp_header_size);
#else
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);
#endif
	return 0;
}

static int config_mc_buffer(struct VP9Decoder_s *pbi, unsigned short bit_depth)
{
	int i;
	struct VP9_Common_s *cm = &pbi->common;
	struct PIC_BUFFER_CONFIG_s *cur_pic_config = &cm->cur_frame->buf;
	uint8_t scale_enable = 0;

	if (debug&VP9_DEBUG_BUFMGR)
		pr_info("config_mc_buffer entered .....\n");

	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(0 << 8) | (0 << 1) | 1);
	for (i = 0; i < REFS_PER_FRAME; ++i) {
		struct PIC_BUFFER_CONFIG_s *pic_config = cm->frame_refs[i].buf;

		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
		(pic_config->mc_canvas_u_v << 16)
		| (pic_config->mc_canvas_u_v << 8)
		| pic_config->mc_canvas_y);
		if (debug & VP9_DEBUG_BUFMGR_MORE)
			pr_info("refid %x mc_canvas_u_v %x mc_canvas_y %x\n",
				i, pic_config->mc_canvas_u_v,
				pic_config->mc_canvas_y);
	}
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(16 << 8) | (0 << 1) | 1);
	for (i = 0; i < REFS_PER_FRAME; ++i) {
		struct PIC_BUFFER_CONFIG_s *pic_config = cm->frame_refs[i].buf;

		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
			(pic_config->mc_canvas_u_v << 16)
			| (pic_config->mc_canvas_u_v << 8)
			| pic_config->mc_canvas_y);
	}

	/*auto_inc start index:0 field:0*/
	WRITE_VREG(VP9D_MPP_REFINFO_TBL_ACCCONFIG, 0x1 << 2);
	/*index 0:last 1:golden 2:altref*/
	for (i = 0; i < REFS_PER_FRAME; i++) {
		int ref_pic_body_size;
		struct PIC_BUFFER_CONFIG_s *pic_config = cm->frame_refs[i].buf;

		WRITE_VREG(VP9D_MPP_REFINFO_DATA, pic_config->y_crop_width);
		WRITE_VREG(VP9D_MPP_REFINFO_DATA, pic_config->y_crop_height);

	if (pic_config->y_crop_width != cur_pic_config->y_crop_width ||
		pic_config->y_crop_height != cur_pic_config->y_crop_height) {
		scale_enable |= (1 << i);
	}
	ref_pic_body_size =
		compute_losless_comp_body_size(pic_config->y_crop_width,
		pic_config->y_crop_height, (bit_depth == VPX_BITS_10));
	WRITE_VREG(VP9D_MPP_REFINFO_DATA,
		(pic_config->y_crop_width << 14)
		/ cur_pic_config->y_crop_width);
	WRITE_VREG(VP9D_MPP_REFINFO_DATA,
		(pic_config->y_crop_height << 14)
		/ cur_pic_config->y_crop_height);
#ifdef VP9_10B_MMU
	WRITE_VREG(VP9D_MPP_REFINFO_DATA, 0);
#else
	WRITE_VREG(VP9D_MPP_REFINFO_DATA, ref_pic_body_size >> 5);
#endif
	}
	WRITE_VREG(VP9D_MPP_REF_SCALE_ENBL, scale_enable);
	return 0;
}

static void clear_mpred_hw(struct VP9Decoder_s *pbi)
{
	unsigned int data32;
	data32 = READ_VREG(HEVC_MPRED_CTRL4);
	data32 &=  (~(1 << 6));
	WRITE_VREG(HEVC_MPRED_CTRL4, data32);
}

static void config_mpred_hw(struct VP9Decoder_s *pbi)
{
	struct VP9_Common_s *cm = &pbi->common;
	struct PIC_BUFFER_CONFIG_s *cur_pic_config = &cm->cur_frame->buf;
	struct PIC_BUFFER_CONFIG_s *last_frame_pic_config =
						&cm->prev_frame->buf;

	unsigned int data32;
	int     mpred_curr_lcu_x;
	int     mpred_curr_lcu_y;
	int     mpred_mv_rd_end_addr;
	int     MV_MEM_UNIT = 0x240;


	mpred_mv_rd_end_addr = last_frame_pic_config->mpred_mv_wr_start_addr
			+ (last_frame_pic_config->lcu_total * MV_MEM_UNIT);

	data32 = READ_VREG(HEVC_MPRED_CURR_LCU);
	mpred_curr_lcu_x   = data32 & 0xffff;
	mpred_curr_lcu_y   = (data32 >> 16) & 0xffff;

	if (debug & VP9_DEBUG_BUFMGR)
		pr_info("cur pic_config index %d  col pic_config index %d\n",
			cur_pic_config->index, last_frame_pic_config->index);
	WRITE_VREG(HEVC_MPRED_CTRL3, 0x24122412);
	WRITE_VREG(HEVC_MPRED_ABV_START_ADDR,
			pbi->work_space_buf->mpred_above.buf_start);

	data32 = READ_VREG(HEVC_MPRED_CTRL4);

	data32 &=  (~(1 << 6));
	data32 |= (cm->use_prev_frame_mvs << 6);
	WRITE_VREG(HEVC_MPRED_CTRL4, data32);

	WRITE_VREG(HEVC_MPRED_MV_WR_START_ADDR,
			cur_pic_config->mpred_mv_wr_start_addr);
	WRITE_VREG(HEVC_MPRED_MV_WPTR, cur_pic_config->mpred_mv_wr_start_addr);

	WRITE_VREG(HEVC_MPRED_MV_RD_START_ADDR,
			last_frame_pic_config->mpred_mv_wr_start_addr);
	WRITE_VREG(HEVC_MPRED_MV_RPTR,
			last_frame_pic_config->mpred_mv_wr_start_addr);
	/*data32 = ((pbi->lcu_x_num - pbi->tile_width_lcu)*MV_MEM_UNIT);*/
	/*WRITE_VREG(HEVC_MPRED_MV_WR_ROW_JUMP,data32);*/
	/*WRITE_VREG(HEVC_MPRED_MV_RD_ROW_JUMP,data32);*/
	WRITE_VREG(HEVC_MPRED_MV_RD_END_ADDR, mpred_mv_rd_end_addr);

}

static void config_sao_hw(struct VP9Decoder_s *pbi, union param_u *params)
{
	struct VP9_Common_s *cm = &pbi->common;
	struct PIC_BUFFER_CONFIG_s *pic_config = &cm->cur_frame->buf;

	unsigned int data32;
	int lcu_size = 64;
	int mc_buffer_size_u_v =
		pic_config->lcu_total * lcu_size*lcu_size/2;
	int mc_buffer_size_u_v_h =
		(mc_buffer_size_u_v + 0xffff) >> 16;/*64k alignment*/

#ifndef VP9_10B_MMU
	if ((double_write_mode & 0x10) == 0)
		WRITE_VREG(HEVC_CM_BODY_START_ADDR, pic_config->mc_y_adr);
#endif
	if (double_write_mode) {
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, pic_config->dw_y_adr);
		WRITE_VREG(HEVC_SAO_C_START_ADDR, pic_config->dw_u_v_adr);
		WRITE_VREG(HEVC_SAO_Y_WPTR, pic_config->dw_y_adr);
		WRITE_VREG(HEVC_SAO_C_WPTR, pic_config->dw_u_v_adr);
	} else {
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, 0xffffffff);
		WRITE_VREG(HEVC_SAO_C_START_ADDR, 0xffffffff);
	}
#ifdef VP9_10B_MMU
	WRITE_VREG(HEVC_CM_HEADER_START_ADDR, pic_config->header_adr);
#endif
	data32 = (mc_buffer_size_u_v_h << 16) << 1;
	/*pr_info("data32=%x,mc_buffer_size_u_v_h=%x,lcu_total=%x\n",
		data32, mc_buffer_size_u_v_h, pic_config->lcu_total);*/
	WRITE_VREG(HEVC_SAO_Y_LENGTH, data32);

	data32 = (mc_buffer_size_u_v_h << 16);
	WRITE_VREG(HEVC_SAO_C_LENGTH, data32);

#ifdef VP9_10B_NV21
#ifdef DOS_PROJECT
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	/*[13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32*/
	data32 |= (MEM_MAP_MODE << 12);
	data32 &= (~0x3);
	data32 |= 0x1; /* [1]:dw_disable [0]:cm_disable*/
	WRITE_VREG(HEVC_SAO_CTRL1, data32);
	/*[23:22] dw_v1_ctrl [21:20] dw_v0_ctrl [19:18] dw_h1_ctrl
		[17:16] dw_h0_ctrl*/
	data32 = READ_VREG(HEVC_SAO_CTRL5);
	/*set them all 0 for H265_NV21 (no down-scale)*/
	data32 &= ~(0xff << 16);
	WRITE_VREG(HEVC_SAO_CTRL5, data32);
	ata32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	/*[5:4] address_format 00:linear 01:32x32 10:64x32*/
	data32 |= (MEM_MAP_MODE << 4);
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#else
	/*m8baby test1902*/
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	/*[13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32*/
	data32 |= (MEM_MAP_MODE << 12);
	data32 &= (~0xff0);
	/*data32 |= 0x670;*/ /*Big-Endian per 64-bit*/
	data32 |= 0x880;  /*.Big-Endian per 64-bit */
	data32 &= (~0x3);
	data32 |= 0x1; /*[1]:dw_disable [0]:cm_disable*/
	WRITE_VREG(HEVC_SAO_CTRL1, data32);
	/* [23:22] dw_v1_ctrl [21:20] dw_v0_ctrl
	[19:18] dw_h1_ctrl [17:16] dw_h0_ctrl*/
	data32 = READ_VREG(HEVC_SAO_CTRL5);
	/* set them all 0 for H265_NV21 (no down-scale)*/
	data32 &= ~(0xff << 16);
	WRITE_VREG(HEVC_SAO_CTRL5, data32);

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	/*[5:4] address_format 00:linear 01:32x32 10:64x32*/
	data32 |= (MEM_MAP_MODE << 4);
	data32 &= (~0xF);
	data32 |= 0x8; /*Big-Endian per 64-bit*/
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#endif
#else
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	data32 |= (MEM_MAP_MODE <<
			   12);	/* [13:12] axi_aformat, 0-Linear,
				   1-32x32, 2-64x32 */
	data32 &= (~0xff0);
	/* data32 |= 0x670;  // Big-Endian per 64-bit */
	data32 |= endian;	/* Big-Endian per 64-bit */
	data32 &= (~0x3); /*[1]:dw_disable [0]:cm_disable*/
	if (double_write_mode == 0)
		data32 |= 0x2; /*disable double write*/
#ifndef VP9_10B_MMU
	else
	if (double_write_mode & 0x10)
		data32 |= 0x1; /*disable cm*/
#endif
	WRITE_VREG(HEVC_SAO_CTRL1, data32);

	if (double_write_mode & 0x10) {
		/* [23:22] dw_v1_ctrl
		[21:20] dw_v0_ctrl
		[19:18] dw_h1_ctrl
		[17:16] dw_h0_ctrl
		*/
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		/*set them all 0 for H265_NV21 (no down-scale)*/
		data32 &= ~(0xff << 16);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	} else {
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		data32 &= (~(0xff << 16));
		if (double_write_mode != 1)
			data32 |= (0xff<<16);
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
}

static void vp9_config_work_space_hw(struct VP9Decoder_s *pbi)
{
	struct BuffInfo_s *buf_spec = pbi->work_space_buf;
#ifdef LOSLESS_COMPRESS_MODE
	int losless_comp_header_size =
		compute_losless_comp_header_size(pbi->init_pic_w,
		pbi->init_pic_h);
	int losless_comp_body_size =
		compute_losless_comp_body_size(pbi->init_pic_w,
		pbi->init_pic_h, buf_alloc_depth == 10);
#endif
#ifdef VP9_10B_MMU
	unsigned int data32;
#endif
	if (debug)
		pr_info("%s %x %x %x %x %x %x %x %x %x %x %x %x\n",
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
	if ((debug & VP9_DEBUG_SEND_PARAM_WITH_REG) == 0)
		WRITE_VREG(HEVC_RPM_BUFFER, (u32)pbi->rpm_phy_addr);
	WRITE_VREG(HEVC_SHORT_TERM_RPS, buf_spec->short_term_rps.buf_start);
	/*WRITE_VREG(HEVC_VPS_BUFFER, buf_spec->vps.buf_start);*/
	/*WRITE_VREG(HEVC_SPS_BUFFER, buf_spec->sps.buf_start);*/
	WRITE_VREG(HEVC_PPS_BUFFER, buf_spec->pps.buf_start);
	WRITE_VREG(HEVC_SAO_UP, buf_spec->sao_up.buf_start);
	WRITE_VREG(HEVC_STREAM_SWAP_BUFFER, buf_spec->swap_buf.buf_start);
	WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, buf_spec->swap_buf2.buf_start);
	WRITE_VREG(HEVC_SCALELUT, buf_spec->scalelut.buf_start);

	/* cfg_p_addr */
	WRITE_VREG(HEVC_DBLK_CFG4, buf_spec->dblk_para.buf_start);
	/* cfg_d_addr */
	WRITE_VREG(HEVC_DBLK_CFG5, buf_spec->dblk_data.buf_start);

#ifdef LOSLESS_COMPRESS_MODE
#ifdef VP9_10B_MMU
	/*bit[4] : paged_mem_mode*/
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0x1 << 4));
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, 0);
#else
	/*if(cur_pic_config->bit_depth == VPX_BITS_10)
		WRITE_VREG(P_HEVCD_MPP_DECOMP_CTL1, (0<<3));*/
	/*bit[3] smem mdoe*/
	/*else WRITE_VREG(P_HEVCD_MPP_DECOMP_CTL1, (1<<3));*/
	/*bit[3] smem mdoe*/
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, (losless_comp_body_size >> 5));
#endif
	/*WRITE_VREG(P_HEVCD_MPP_DECOMP_CTL2,(losless_comp_body_size >> 5));*/
	/*WRITE_VREG(P_HEVCD_MPP_DECOMP_CTL3,(0xff<<20) | (0xff<<10) | 0xff);*/
/*8-bit mode */
	WRITE_VREG(HEVC_CM_BODY_LENGTH, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH, losless_comp_header_size);
#else
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);
#endif

#ifdef VP9_10B_MMU
	WRITE_VREG(HEVC_SAO_MMU_VH0_ADDR, buf_spec->mmu_vbh.buf_start);
	WRITE_VREG(HEVC_SAO_MMU_VH1_ADDR, buf_spec->mmu_vbh.buf_start
			+ buf_spec->mmu_vbh.buf_size/2);
	/*data32 = READ_VREG(P_HEVC_SAO_CTRL9);*/
	/*data32 |= 0x1;*/
	/*WRITE_VREG(P_HEVC_SAO_CTRL9, data32);*/

	/* use HEVC_CM_HEADER_START_ADDR */
	data32 = READ_VREG(HEVC_SAO_CTRL5);
	data32 |= (1<<10);
	WRITE_VREG(HEVC_SAO_CTRL5, data32);

#endif

	WRITE_VREG(VP9_SEG_MAP_BUFFER, buf_spec->seg_map.buf_start);

	if (debug & VP9_DEBUG_UCODE)
		WRITE_VREG(LMEM_DUMP_ADR, (u32)pbi->lmem_phy_addr);

	 /**/
	WRITE_VREG(VP9_PROB_SWAP_BUFFER, pbi->prob_buffer_phy_addr);
	WRITE_VREG(VP9_COUNT_SWAP_BUFFER, pbi->count_buffer_phy_addr);
#ifdef VP9_10B_MMU
	WRITE_VREG(VP9_MMU_MAP_BUFFER, pbi->frame_mmu_map_phy_addr);
#endif

}


#ifdef VP9_LPF_LVL_UPDATE
/*
 * Defines, declarations, sub-functions for vp9 de-block loop
	filter Thr/Lvl table update
 * - struct segmentation is for loop filter only (removed something)
 * - function "vp9_loop_filter_init" and "vp9_loop_filter_frame_init" will
	be instantiated in C_Entry
 * - vp9_loop_filter_init run once before decoding start
 * - vp9_loop_filter_frame_init run before every frame decoding start
 * - set video format to VP9 is in vp9_loop_filter_init
 */
#define MAX_LOOP_FILTER 63
#define MAX_REF_LF_DELTAS 4
#define MAX_MODE_LF_DELTAS 2
/*#define INTRA_FRAME 0*/
/*#define LAST_FRAME 1*/
/*#define MAX_REF_FRAMES 4*/
#define SEGMENT_DELTADATA   0
#define SEGMENT_ABSDATA     1
#define MAX_SEGMENTS     8
/*.#define SEG_TREE_PROBS   (MAX_SEGMENTS-1)*/
/*no use for loop filter, if this struct for common use, pls add it back*/
/*#define PREDICTION_PROBS 3*/
/* no use for loop filter, if this struct for common use, pls add it back*/

enum SEG_LVL_FEATURES {
	SEG_LVL_ALT_Q = 0, /*Use alternate Quantizer ....*/
	SEG_LVL_ALT_LF = 1, /*Use alternate loop filter value...*/
	SEG_LVL_REF_FRAME = 2, /*Optional Segment reference frame*/
	SEG_LVL_SKIP = 3,  /*Optional Segment (0,0) + skip mode*/
	SEG_LVL_MAX = 4    /*Number of features supported*/
};

struct segmentation {
	uint8_t enabled;
	uint8_t update_map;
	uint8_t update_data;
	uint8_t abs_delta;
	uint8_t temporal_update;

	/*no use for loop filter, if this struct
	for common use, pls add it back*/
	/*vp9_prob tree_probs[SEG_TREE_PROBS]; */
	/* no use for loop filter, if this struct
		for common use, pls add it back*/
	/*vp9_prob pred_probs[PREDICTION_PROBS];*/

	int16_t feature_data[MAX_SEGMENTS][SEG_LVL_MAX];
	unsigned int feature_mask[MAX_SEGMENTS];
};

struct loop_filter_thresh {
	uint8_t mblim;
	uint8_t lim;
	uint8_t hev_thr;
};

struct loop_filter_info_n {
	struct loop_filter_thresh lfthr[MAX_LOOP_FILTER + 1];
	uint8_t lvl[MAX_SEGMENTS][MAX_REF_FRAMES][MAX_MODE_LF_DELTAS];
};

struct loopfilter {
	int filter_level;

	int sharpness_level;
	int last_sharpness_level;

	uint8_t mode_ref_delta_enabled;
	uint8_t mode_ref_delta_update;

	/*0 = Intra, Last, GF, ARF*/
	signed char ref_deltas[MAX_REF_LF_DELTAS];
	signed char last_ref_deltas[MAX_REF_LF_DELTAS];

	/*0 = ZERO_MV, MV*/
	signed char mode_deltas[MAX_MODE_LF_DELTAS];
	signed char last_mode_deltas[MAX_MODE_LF_DELTAS];
};

static int vp9_clamp(int value, int low, int high)
{
	return value < low ? low : (value > high ? high : value);
}

int segfeature_active(struct segmentation *seg,
			int segment_id,
			enum SEG_LVL_FEATURES feature_id) {
	return seg->enabled &&
		(seg->feature_mask[segment_id] & (1 << feature_id));
}

int get_segdata(struct segmentation *seg, int segment_id,
				enum SEG_LVL_FEATURES feature_id) {
	return seg->feature_data[segment_id][feature_id];
}

static void vp9_update_sharpness(struct loop_filter_info_n *lfi,
					int sharpness_lvl)
{
	int lvl;
	/*For each possible value for the loop filter fill out limits*/
	for (lvl = 0; lvl <= MAX_LOOP_FILTER; lvl++) {
		/*Set loop filter parameters that control sharpness.*/
		int block_inside_limit = lvl >> ((sharpness_lvl > 0) +
					(sharpness_lvl > 4));

		if (sharpness_lvl > 0) {
			if (block_inside_limit > (9 - sharpness_lvl))
				block_inside_limit = (9 - sharpness_lvl);
		}

		if (block_inside_limit < 1)
			block_inside_limit = 1;

		lfi->lfthr[lvl].lim = (uint8_t)block_inside_limit;
		lfi->lfthr[lvl].mblim = (uint8_t)(2 * (lvl + 2) +
				block_inside_limit);
	}
}

int default_filt_lvl;
struct loop_filter_info_n *lfi;
struct loopfilter *lf;
struct segmentation *seg_4lf;

/*instantiate this function once when decode is started*/
void vp9_loop_filter_init(void)
{
	int i;
	if (!lfi)
		lfi = kmalloc(sizeof(struct loop_filter_info_n), GFP_KERNEL);
	if (!lf)
		lf = kmalloc(sizeof(struct loopfilter), GFP_KERNEL);
	if (!seg_4lf)
		seg_4lf = kmalloc(sizeof(struct segmentation), GFP_KERNEL);
	if (lfi == NULL || lf == NULL || seg_4lf == NULL) {
		pr_err("[test.c] vp9_loop_filter init malloc error!!!\n");
		return;
	}
	memset(lfi, 0, sizeof(struct loop_filter_info_n));
	memset(lf, 0, sizeof(struct loopfilter));
	memset(seg_4lf, 0, sizeof(struct segmentation));
	lf->sharpness_level = 0; /*init to 0 */
	/*init limits for given sharpness*/
	vp9_update_sharpness(lfi, lf->sharpness_level);
	lf->last_sharpness_level = lf->sharpness_level;
	/*init hev threshold const vectors (actually no use)
	for (i = 0; i <= MAX_LOOP_FILTER; i++)
		lfi->lfthr[i].hev_thr = (uint8_t)(i >> 4);*/

	/*Write to register*/
	for (i = 0; i < 32; i++) {
		unsigned int thr;
		thr = ((lfi->lfthr[i * 2 + 1].lim & 0x3f)<<8) |
			(lfi->lfthr[i * 2 + 1].mblim & 0xff);
		thr = (thr<<16) | ((lfi->lfthr[i*2].lim & 0x3f)<<8) |
			(lfi->lfthr[i * 2].mblim & 0xff);
		WRITE_VREG(HEVC_DBLK_CFG9, thr);
	}

	/*video format is VP9*/
	WRITE_VREG(HEVC_DBLK_CFGB, 0x40400001);
}
	/* perform this function per frame*/
void vp9_loop_filter_frame_init(struct segmentation *seg,
		struct loop_filter_info_n *lfi, struct loopfilter *lf,
		int default_filt_lvl) {
	int i;
	int seg_id;
	/*n_shift is the multiplier for lf_deltas
	the multiplier is 1 for when filter_lvl is between 0 and 31;
	2 when filter_lvl is between 32 and 63*/
	const int scale = 1 << (default_filt_lvl >> 5);

	/*update limits if sharpness has changed*/
	if (lf->last_sharpness_level != lf->sharpness_level) {
		vp9_update_sharpness(lfi, lf->sharpness_level);
		lf->last_sharpness_level = lf->sharpness_level;

	/*Write to register*/
	for (i = 0; i < 32; i++) {
		unsigned int thr;
		thr = ((lfi->lfthr[i * 2 + 1].lim & 0x3f) << 8)
			| (lfi->lfthr[i * 2 + 1].mblim & 0xff);
		thr = (thr << 16) | ((lfi->lfthr[i * 2].lim & 0x3f) << 8)
			| (lfi->lfthr[i * 2].mblim & 0xff);
		WRITE_VREG(HEVC_DBLK_CFG9, thr);
		}
	}

	for (seg_id = 0; seg_id < MAX_SEGMENTS; seg_id++) {/*MAX_SEGMENTS = 8*/
		int lvl_seg = default_filt_lvl;
		if (segfeature_active(seg, seg_id, SEG_LVL_ALT_LF)) {
			const int data = get_segdata(seg, seg_id,
						SEG_LVL_ALT_LF);
			lvl_seg = vp9_clamp(seg->abs_delta == SEGMENT_ABSDATA ?
				data : default_filt_lvl + data,
				0, MAX_LOOP_FILTER);
#ifdef DBG_LF_PRINT
	pr_info("segfeature_active!!!seg_id=%d,lvl_seg=%d\n", seg_id, lvl_seg);
#endif
	}

	if (!lf->mode_ref_delta_enabled) {
		/*we could get rid of this if we assume that deltas are set to
		zero when not in use; encoder always uses deltas*/
		memset(lfi->lvl[seg_id], lvl_seg, sizeof(lfi->lvl[seg_id]));
	} else {
		int ref, mode;
		const int intra_lvl = lvl_seg +	lf->ref_deltas[INTRA_FRAME]
					* scale;
#ifdef DBG_LF_PRINT
	pr_info("LF_PRINT:vp9_loop_filter_frame_init,seg_id=%d\n", seg_id);
	pr_info("ref_deltas[INTRA_FRAME]=%d\n", lf->ref_deltas[INTRA_FRAME]);
#endif
		lfi->lvl[seg_id][INTRA_FRAME][0] =
				vp9_clamp(intra_lvl, 0, MAX_LOOP_FILTER);

		for (ref = LAST_FRAME; ref < MAX_REF_FRAMES; ++ref) {
			/* LAST_FRAME = 1, MAX_REF_FRAMES = 4*/
			for (mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode) {
				/*MAX_MODE_LF_DELTAS = 2*/
				const int inter_lvl =
					lvl_seg + lf->ref_deltas[ref] * scale
					+ lf->mode_deltas[mode] * scale;
#ifdef DBG_LF_PRINT
#endif
				lfi->lvl[seg_id][ref][mode] =
					vp9_clamp(inter_lvl, 0,
					MAX_LOOP_FILTER);
				}
			}
		}
	}

#ifdef DBG_LF_PRINT
	/*print out thr/lvl table per frame*/
	for (i = 0; i <= MAX_LOOP_FILTER; i++) {
		pr_info("LF_PRINT:(%d)thr=%d,blim=%d,lim=%d\n",
			i, lfi->lfthr[i].hev_thr, lfi->lfthr[i].mblim,
			lfi->lfthr[i].lim);
	}
	for (seg_id = 0; seg_id < MAX_SEGMENTS; seg_id++) {
		pr_info("LF_PRINT:lvl(seg_id=%d)(mode=0,%d,%d,%d,%d)\n",
			seg_id, lfi->lvl[seg_id][0][0],
			lfi->lvl[seg_id][1][0], lfi->lvl[seg_id][2][0],
			lfi->lvl[seg_id][3][0]);
		pr_info("i(mode=1,%d,%d,%d,%d)\n", lfi->lvl[seg_id][0][1],
			lfi->lvl[seg_id][1][1], lfi->lvl[seg_id][2][1],
			lfi->lvl[seg_id][3][1]);
	}
#endif

	/*Write to register */
	for (i = 0; i < 16; i++) {
		unsigned int level;
		level = ((lfi->lvl[i >> 1][3][i & 1] & 0x3f) << 24) |
			((lfi->lvl[i >> 1][2][i & 1] & 0x3f) << 16) |
			((lfi->lvl[i >> 1][1][i & 1] & 0x3f) << 8) |
			(lfi->lvl[i >> 1][0][i & 1] & 0x3f);
		if (!default_filt_lvl)
			level = 0;
		WRITE_VREG(HEVC_DBLK_CFGA, level);
	}
}
/* VP9_LPF_LVL_UPDATE */
#endif

static void vp9_init_decoder_hw(struct VP9Decoder_s *pbi)
{
	unsigned int data32;
	int i;

	if (debug & VP9_DEBUG_BUFMGR)
		pr_info("[test.c] Enable HEVC Parser Interrupt\n");
		data32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
#if 1
		/* set bit 31~29 to 3 if HEVC_STREAM_FIFO_CTL[29] is 1 */
		data32 &= ~(7 << 29);
		data32 |= (3 << 29);
#endif
		data32 = data32 |
		(1 << 24) |/*stream_buffer_empty_int_amrisc_enable*/
		(1 << 22) |/*stream_fifo_empty_int_amrisc_enable*/
		(1 << 7) |/*dec_done_int_cpu_enable*/
		(1 << 4) |/*startcode_found_int_cpu_enable*/
		(0 << 3) |/*startcode_found_int_amrisc_enable*/
		(1 << 0)    /*parser_int_enable*/
		;
	WRITE_VREG(HEVC_PARSER_INT_CONTROL, data32);

	if (debug & VP9_DEBUG_BUFMGR)
		pr_info("[test.c] Enable HEVC Parser Shift\n");

		data32 = READ_VREG(HEVC_SHIFT_STATUS);
		data32 = data32 |
		(0 << 1) |/*emulation_check_off VP9
			do not have emulation*/
		(1 << 0)/*startcode_check_on*/
		;
		WRITE_VREG(HEVC_SHIFT_STATUS, data32);
		WRITE_VREG(HEVC_SHIFT_CONTROL,
		(0 << 14) | /*disable_start_code_protect*/
		(1 << 10) | /*length_zero_startcode_en for VP9*/
		(1 << 9) | /*length_valid_startcode_en for VP9*/
		(3 << 6) | /*sft_valid_wr_position*/
		(2 << 4) | /*emulate_code_length_sub_1*/
		(3 << 1) | /*start_code_length_sub_1
		VP9 use 0x00000001 as startcode (4 Bytes)*/
		(1 << 0)   /*stream_shift_enable*/
		);

	WRITE_VREG(HEVC_CABAC_CONTROL,
		(1 << 0)/*cabac_enable*/
	);

	WRITE_VREG(HEVC_PARSER_CORE_CONTROL,
		(1 << 0)/* hevc_parser_core_clk_en*/
	);


	WRITE_VREG(HEVC_DEC_STATUS_REG, 0);

	/*Initial IQIT_SCALELUT memory -- just to avoid X in simulation*/
	if (debug & VP9_DEBUG_BUFMGR)
		pr_info("Initial IQIT_SCALELUT memory\n");
	WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0);/*cfg_p_addr*/
	for (i = 0; i < 1024; i++)
		WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, 0);


#ifdef ENABLE_SWAP_TEST
	WRITE_VREG(HEVC_STREAM_SWAP_TEST, 100);
#else
	WRITE_VREG(HEVC_STREAM_SWAP_TEST, 0);
#endif
#ifdef MULTI_INSTANCE_SUPPORT
	if (pbi->platform_dev && vdec_frame_based(hw_to_vdec(pbi)))
		WRITE_VREG(DECODE_MODE, DECODE_MODE_MULTI_FRAMEBASE);
	else
		WRITE_VREG(DECODE_MODE, DECODE_MODE_MULTI_STREAMBASE);
	WRITE_VREG(HEVC_DECODE_SIZE, 0);
	WRITE_VREG(HEVC_DECODE_COUNT, 0);
#else
	WRITE_VREG(DECODE_MODE, DECODE_MODE_SINGLE);
	WRITE_VREG(HEVC_DECODE_PIC_BEGIN_REG, 0);
	WRITE_VREG(HEVC_DECODE_PIC_NUM_REG, 0x7fffffff); /*to remove*/
#endif
	/*Send parser_cmd*/
	if (debug)
		pr_info("[test.c] SEND Parser Command ...\n");
	WRITE_VREG(HEVC_PARSER_CMD_WRITE, (1 << 16) | (0 << 0));
	for (i = 0; i < PARSER_CMD_NUMBER; i++)
		WRITE_VREG(HEVC_PARSER_CMD_WRITE, parser_cmd[i]);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_0, PARSER_CMD_SKIP_CFG_0);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_1, PARSER_CMD_SKIP_CFG_1);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_2, PARSER_CMD_SKIP_CFG_2);


	WRITE_VREG(HEVC_PARSER_IF_CONTROL,
		/*  (1 << 8) |*/ /*sao_sw_pred_enable*/
		(1 << 5) | /*parser_sao_if_en*/
		(1 << 2) | /*parser_mpred_if_en*/
		(1 << 0) /*parser_scaler_if_en*/
	);
	/*Changed to Start MPRED in microcode*/
	/*
	pr_info("[test.c] Start MPRED\n");
	WRITE_VREG(HEVC_MPRED_INT_STATUS,
	(1<<31)
	);
	*/
	if (debug)
		pr_info("[test.c] Reset IPP\n");
	WRITE_VREG(HEVCD_IPP_TOP_CNTL,
		(0 << 1) | /*enable ipp*/
		(1 << 0)   /*software reset ipp and mpp*/
	);
	WRITE_VREG(HEVCD_IPP_TOP_CNTL,
		(1 << 1) | /*enable ipp*/
		(0 << 0)   /*software reset ipp and mpp*/
	);
#ifdef VP9_10B_NV21
	/*Enable NV21 reference read mode for MC*/
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);
#endif

	/*Initialize mcrcc and decomp perf counters
	mcrcc_perfcount_reset();
	decomp_perfcount_reset();*/
	return;
}


#ifdef CONFIG_HEVC_CLK_FORCED_ON
static void config_vp9_clk_forced_on(void)
{
	unsigned int rdata32;
	/*IQIT*/
	rdata32 = READ_VREG(HEVC_IQIT_CLK_RST_CTRL);
	WRITE_VREG(HEVC_IQIT_CLK_RST_CTRL, rdata32 | (0x1 << 2));

	/* DBLK*/
	rdata32 = READ_VREG(HEVC_DBLK_CFG0);
	WRITE_VREG(HEVC_DBLK_CFG0, rdata32 | (0x1 << 2));

	/* SAO*/
	rdata32 = READ_VREG(HEVC_SAO_CTRL1);
	WRITE_VREG(HEVC_SAO_CTRL1, rdata32 | (0x1 << 2));

	/*MPRED*/
	rdata32 = READ_VREG(HEVC_MPRED_CTRL1);
	WRITE_VREG(HEVC_MPRED_CTRL1, rdata32 | (0x1 << 24));

	/* PARSER*/
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
			rdata32 | (0x1 << 6) | (0x1 << 3) | (0x1 << 1));

	/*IPP*/
	rdata32 = READ_VREG(HEVCD_IPP_DYNCLKGATE_CONFIG);
	WRITE_VREG(HEVCD_IPP_DYNCLKGATE_CONFIG, rdata32 | 0xffffffff);

	/* MCRCC*/
	rdata32 = READ_VREG(HEVCD_MCRCC_CTL1);
	WRITE_VREG(HEVCD_MCRCC_CTL1, rdata32 | (0x1 << 3));
}
#endif


#ifdef MCRCC_ENABLE
static void  config_mcrcc_axi_hw(struct VP9Decoder_s *pbi)
{
	unsigned int rdata32;
	unsigned short is_inter;
	/*pr_info("Entered config_mcrcc_axi_hw...\n");*/
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2);/* reset mcrcc*/
	is_inter = ((pbi->common.frame_type != KEY_FRAME) &&
			(!pbi->common.intra_only)) ? 1 : 0;
	if (!is_inter) { /* I-PIC*/
		/*remove reset -- disables clock*/
		WRITE_VREG(HEVCD_MCRCC_CTL1, 0x0);
		return;
	}

#if 0
	pr_info("before call mcrcc_get_hitrate\r\n");
	mcrcc_get_hitrate();
	decomp_get_hitrate();
	decomp_get_comprate();
#endif

	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(0 << 8) | (1 << 1) | 0);
	rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
	rdata32 = rdata32 & 0xffff;
	rdata32 = rdata32 | (rdata32 << 16);
	WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);
	/*Programme canvas1 */
	rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
	rdata32 = rdata32 & 0xffff;
	rdata32 = rdata32 | (rdata32 << 16);
	WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32);
	/*enable mcrcc progressive-mode*/
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0);
	return;
}
#endif


static struct VP9Decoder_s gHevc;

static void vp9_local_uninit(struct VP9Decoder_s *pbi)
{
	pbi->rpm_ptr = NULL;
	pbi->lmem_ptr = NULL;
	if (pbi->rpm_addr) {
		dma_unmap_single(amports_get_dma_device(),
			pbi->rpm_phy_addr, RPM_BUF_SIZE,
				DMA_FROM_DEVICE);
		kfree(pbi->rpm_addr);
		pbi->rpm_addr = NULL;
	}
	if (pbi->lmem_addr) {
			if (pbi->lmem_phy_addr)
				dma_free_coherent(amports_get_dma_device(),
						LMEM_BUF_SIZE, pbi->lmem_addr,
						pbi->lmem_phy_addr);
		pbi->lmem_addr = NULL;
	}
	if (pbi->prob_buffer_addr) {
		if (pbi->prob_buffer_phy_addr)
			dma_free_coherent(amports_get_dma_device(),
				PROB_BUF_SIZE, pbi->prob_buffer_addr,
				pbi->prob_buffer_phy_addr);
			pbi->prob_buffer_addr = NULL;
	}
	if (pbi->count_buffer_addr) {
		if (pbi->count_buffer_phy_addr)
			dma_free_coherent(amports_get_dma_device(),
				COUNT_BUF_SIZE, pbi->count_buffer_addr,
				pbi->count_buffer_phy_addr);
			pbi->count_buffer_addr = NULL;
	}
#ifdef VP9_10B_MMU
	if (pbi->frame_mmu_map_addr) {
		if (pbi->frame_mmu_map_phy_addr)
			dma_free_coherent(amports_get_dma_device(),
				FRAME_MMU_MAP_SIZE, pbi->frame_mmu_map_addr,
					pbi->frame_mmu_map_phy_addr);
			pbi->frame_mmu_map_addr = NULL;
	}
#endif

#ifdef VP9_LPF_LVL_UPDATE
	kfree(lfi);
	lfi = NULL;
	kfree(lf);
	lf = NULL;
	kfree(seg_4lf);
	seg_4lf = NULL;
#endif
	kfree(gvs);
	gvs = NULL;
}

static int vp9_local_init(struct VP9Decoder_s *pbi)
{
	int ret = -1;
	/*int losless_comp_header_size, losless_comp_body_size;*/

	struct BuffInfo_s *cur_buf_info = NULL;
	memset(&pbi->param, 0, sizeof(union param_u));
	memset(&pbi->common, 0, sizeof(struct VP9_Common_s));
#ifdef MULTI_INSTANCE_SUPPORT
	cur_buf_info = &pbi->work_space_buf_store;
#ifdef SUPPORT_4K2K
	memcpy(cur_buf_info, &amvvp9_workbuff_spec[1],	/* 4k2k work space */
		sizeof(struct BuffInfo_s));
#else
	memcpy(cur_buf_info, &amvvp9_workbuff_spec[0],	/* 1080p work space */
		sizeof(struct BuffInfo_s));
#endif
	cur_buf_info->start_adr = pbi->buf_start;
	pbi->mc_buf_spec.buf_end = pbi->buf_start + pbi->buf_size;
#else
/*! MULTI_INSTANCE_SUPPORT*/
#ifdef SUPPORT_4K2K
	cur_buf_info = &amvvp9_workbuff_spec[1];	/* 4k2k work space */
#else
	cur_buf_info = &amvvp9_workbuff_spec[0];	/* 1080p work space */
#endif
#endif

	init_buff_spec(pbi, cur_buf_info);
	pbi->mc_buf_spec.buf_start = (cur_buf_info->end_adr + 0xffff)
	    & (~0xffff);
	pbi->mc_buf_spec.buf_size = (pbi->mc_buf_spec.buf_end
	    - pbi->mc_buf_spec.buf_start);
	if (debug) {
		pr_err("pbi->mc_buf_spec.buf_start %x-%x\n",
			pbi->mc_buf_spec.buf_start,
			pbi->mc_buf_spec.buf_start +
			pbi->mc_buf_spec.buf_size);
	}
	vp9_bufmgr_init(pbi, cur_buf_info, &pbi->mc_buf_spec);

	pbi->init_pic_w = buf_alloc_width ? buf_alloc_width :
		(pbi->vvp9_amstream_dec_info.width ?
		pbi->vvp9_amstream_dec_info.width :
		pbi->work_space_buf->max_width);
	pbi->init_pic_h = buf_alloc_height ? buf_alloc_height :
		(pbi->vvp9_amstream_dec_info.height ?
		pbi->vvp9_amstream_dec_info.height :
		pbi->work_space_buf->max_height);
#ifndef VP9_10B_MMU
	init_buf_list(pbi);
#endif
	init_pic_list(pbi);

	pts_unstable = ((unsigned long)(pbi->vvp9_amstream_dec_info.param)
			& 0x40) >> 6;

	pbi->video_signal_type = 0;
	video_signal_type = pbi->video_signal_type;

	if ((debug & VP9_DEBUG_SEND_PARAM_WITH_REG) == 0) {
		pbi->rpm_addr = kmalloc(RPM_BUF_SIZE, GFP_KERNEL);
		if (pbi->rpm_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}

		pbi->rpm_phy_addr = dma_map_single(amports_get_dma_device(),
			pbi->rpm_addr, RPM_BUF_SIZE, DMA_FROM_DEVICE);
		if (dma_mapping_error(amports_get_dma_device(),
			pbi->rpm_phy_addr)) {
			pr_err("%s: failed to map rpm buffer\n", __func__);
			kfree(pbi->rpm_addr);
			pbi->rpm_addr = NULL;
			return -1;
		}

		pbi->rpm_ptr = pbi->rpm_addr;
	}

	if (debug & VP9_DEBUG_UCODE) {
		pbi->lmem_addr = dma_alloc_coherent(amports_get_dma_device(),
				LMEM_BUF_SIZE,
				&pbi->lmem_phy_addr, GFP_KERNEL);
		if (pbi->lmem_addr == NULL) {
			pr_err("%s: failed to alloc lmem buffer\n", __func__);
			return -1;
		}
/*
		pbi->lmem_phy_addr = dma_map_single(amports_get_dma_device(),
			pbi->lmem_addr, LMEM_BUF_SIZE, DMA_BIDIRECTIONAL);
		if (dma_mapping_error(amports_get_dma_device(),
			pbi->lmem_phy_addr)) {
			pr_err("%s: failed to map lmem buffer\n", __func__);
			kfree(pbi->lmem_addr);
			pbi->lmem_addr = NULL;
			return -1;
		}
*/
		pbi->lmem_ptr = pbi->lmem_addr;
	}
	pbi->prob_buffer_addr = dma_alloc_coherent(amports_get_dma_device(),
				PROB_BUF_SIZE,
				&pbi->prob_buffer_phy_addr, GFP_KERNEL);
	if (pbi->prob_buffer_addr == NULL) {
		pr_err("%s: failed to alloc prob_buffer\n", __func__);
		return -1;
	}
	memset(pbi->prob_buffer_addr, 0, PROB_BUF_SIZE);
/*	pbi->prob_buffer_phy_addr = dma_map_single(amports_get_dma_device(),
	pbi->prob_buffer_addr, PROB_BUF_SIZE, DMA_BIDIRECTIONAL);
	if (dma_mapping_error(amports_get_dma_device(),
	pbi->prob_buffer_phy_addr)) {
		pr_err("%s: failed to map prob_buffer\n", __func__);
		kfree(pbi->prob_buffer_addr);
		pbi->prob_buffer_addr = NULL;
		return -1;
	}
*/
	pbi->count_buffer_addr = dma_alloc_coherent(amports_get_dma_device(),
				COUNT_BUF_SIZE,
				&pbi->count_buffer_phy_addr, GFP_KERNEL);
	if (pbi->count_buffer_addr == NULL) {
		pr_err("%s: failed to alloc count_buffer\n", __func__);
		return -1;
	}
	memset(pbi->count_buffer_addr, 0, COUNT_BUF_SIZE);
/*	pbi->count_buffer_phy_addr = dma_map_single(amports_get_dma_device(),
	pbi->count_buffer_addr, COUNT_BUF_SIZE, DMA_BIDIRECTIONAL);
	if (dma_mapping_error(amports_get_dma_device(),
		pbi->count_buffer_phy_addr)) {
		pr_err("%s: failed to map count_buffer\n", __func__);
		kfree(pbi->count_buffer_addr);
		pbi->count_buffer_addr = NULL;
		return -1;
	}
*/
#ifdef VP9_10B_MMU
	pbi->frame_mmu_map_addr = dma_alloc_coherent(amports_get_dma_device(),
				FRAME_MMU_MAP_SIZE,
				&pbi->frame_mmu_map_phy_addr, GFP_KERNEL);
	if (pbi->frame_mmu_map_addr == NULL) {
		pr_err("%s: failed to alloc count_buffer\n", __func__);
		return -1;
	}
	memset(pbi->frame_mmu_map_addr, 0, COUNT_BUF_SIZE);
/*	pbi->frame_mmu_map_phy_addr = dma_map_single(amports_get_dma_device(),
	pbi->frame_mmu_map_addr, FRAME_MMU_MAP_SIZE, DMA_BIDIRECTIONAL);
	if (dma_mapping_error(amports_get_dma_device(),
	pbi->frame_mmu_map_phy_addr)) {
		pr_err("%s: failed to map count_buffer\n", __func__);
		kfree(pbi->frame_mmu_map_addr);
		pbi->frame_mmu_map_addr = NULL;
		return -1;
	}*/
#endif

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


static void set_canvas(struct PIC_BUFFER_CONFIG_s *pic_config)
{
	int canvas_w = ALIGN(pic_config->y_crop_width, 64)/4;
	int canvas_h = ALIGN(pic_config->y_crop_height, 32)/4;
	int blkmode = mem_map_mode;
	/*CANVAS_BLKMODE_64X32*/
	if	(double_write_mode) {
		canvas_w = pic_config->y_crop_width;
		canvas_h = pic_config->y_crop_height;
		if (double_write_mode == 2) {
			canvas_w >>= 2;
			canvas_h >>= 2;
		}

		if (mem_map_mode == 0)
			canvas_w = ALIGN(canvas_w, 32);
		else
			canvas_w = ALIGN(canvas_w, 64);
		canvas_h = ALIGN(canvas_h, 32);

		pic_config->y_canvas_index = 128 + pic_config->index * 2;
		pic_config->uv_canvas_index = 128 + pic_config->index * 2 + 1;

		canvas_config_ex(pic_config->y_canvas_index,
			pic_config->dw_y_adr, canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0x7);
		canvas_config_ex(pic_config->uv_canvas_index,
			pic_config->dw_u_v_adr,	canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0x7);
#ifdef MULTI_INSTANCE_SUPPORT
		pic_config->canvas_config[0].phy_addr =
				pic_config->dw_y_adr;
		pic_config->canvas_config[0].width =
				canvas_w;
		pic_config->canvas_config[0].height =
				canvas_h;
		pic_config->canvas_config[0].block_mode =
				blkmode;
		pic_config->canvas_config[0].endian = 7;

		pic_config->canvas_config[1].phy_addr =
				pic_config->dw_u_v_adr;
		pic_config->canvas_config[1].width =
				canvas_w;
		pic_config->canvas_config[1].height =
				canvas_h;
		pic_config->canvas_config[1].block_mode =
				blkmode;
		pic_config->canvas_config[1].endian = 7;
#endif
	} else {
	#ifndef VP9_10B_MMU
		pic_config->y_canvas_index = 128 + pic_config->index;
		pic_config->uv_canvas_index = 128 + pic_config->index;

		canvas_config_ex(pic_config->y_canvas_index,
			pic_config->mc_y_adr, canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0x7);
		canvas_config_ex(pic_config->uv_canvas_index,
		pic_config->mc_u_v_adr,	canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0x7);
	#endif
	}

}


static void set_frame_info(struct VP9Decoder_s *pbi, struct vframe_s *vf)
{
	unsigned int ar;

	vf->duration = pbi->frame_dur;
	vf->duration_pulldown = 0;
	vf->flag = 0;

	ar = min_t(u32, pbi->frame_ar, DISP_RATIO_ASPECT_RATIO_MAX);
	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);

	return;
}

static int vvp9_vf_states(struct vframe_states *states, void *op_arg)
{
	struct VP9Decoder_s *pbi = (struct VP9Decoder_s *)op_arg;

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&pbi->newframe_q);
	states->buf_avail_num = kfifo_len(&pbi->display_q);

	if (step == 2)
		states->buf_avail_num = 0;
	return 0;
}

static struct vframe_s *vvp9_vf_peek(void *op_arg)
{
	struct vframe_s *vf;
	struct VP9Decoder_s *pbi = (struct VP9Decoder_s *)op_arg;
	if (step == 2)
		return NULL;

	if (kfifo_peek(&pbi->display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vvp9_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct VP9Decoder_s *pbi = (struct VP9Decoder_s *)op_arg;
	if (step == 2)
		return NULL;
	else if (step == 1)
			step = 2;

	if (kfifo_get(&pbi->display_q, &vf)) {
		uint8_t index = vf->index & 0xff;
		 if (index >= 0	&& index < FRAME_BUFFERS)
			return vf;
	}
	return NULL;
}

static void vvp9_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct VP9Decoder_s *pbi = (struct VP9Decoder_s *)op_arg;
	uint8_t index = vf->index & 0xff;

	kfifo_put(&pbi->newframe_q, (const struct vframe_s *)vf);

	if (index >= 0
		&& index < FRAME_BUFFERS) {
		struct VP9_Common_s *cm = &pbi->common;
		struct BufferPool_s *pool = cm->buffer_pool;
		lock_buffer_pool(pool);
		if (pool->frame_bufs[index].buf.vf_ref > 0)
			pool->frame_bufs[index].buf.vf_ref--;

		if (pbi->wait_buf)
			WRITE_VREG(HEVC_ASSIST_MBOX1_IRQ_REG,
						0x1);
		pbi->last_put_idx = index;
		pbi->new_frame_displayed++;
		unlock_buffer_pool(pool);
	}

}

static int vvp9_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_RESET) {
#if 0
		unsigned long flags;
		amhevc_stop();
#ifndef CONFIG_POST_PROCESS_MANAGER
		vf_light_unreg_provider(&vvp9_vf_prov);
#endif
		spin_lock_irqsave(&pbi->lock, flags);
		vvp9_local_init();
		vvp9_prot_init();
		spin_unlock_irqrestore(&pbi->lock, flags);
#ifndef CONFIG_POST_PROCESS_MANAGER
		vf_reg_provider(&vvp9_vf_prov);
#endif
		amhevc_start();
#endif
	}

	return 0;
}

void inc_vf_ref(struct VP9Decoder_s *pbi, int index)
{
	struct VP9_Common_s *cm = &pbi->common;
	cm->buffer_pool->frame_bufs[index].buf.vf_ref++;

	if (debug & VP9_DEBUG_BUFMGR)
		pr_info("%s index = %d new vf_ref = %d\r\n",
			__func__, index,
			cm->buffer_pool->frame_bufs[index].buf.vf_ref);
}


static int prepare_display_buf(struct VP9Decoder_s *pbi,
				struct PIC_BUFFER_CONFIG_s *pic_config)
{
	struct vframe_s *vf = NULL;
	int stream_offset = pic_config->stream_offset;
	unsigned short slice_type = pic_config->slice_type;

	if (debug & VP9_DEBUG_BUFMGR)
		pr_info("%s index = %d\r\n", __func__, pic_config->index);
	if (kfifo_get(&pbi->newframe_q, &vf) == 0) {
		pr_info("fatal error, no available buffer slot.");
		return -1;
	}

	if (double_write_mode) {
		set_canvas(pic_config);
	}
	if (vf) {
		/* if (pts_lookup_offset(PTS_TYPE_VIDEO,
		   stream_offset, &vf->pts, 0) != 0) { */
		if (pts_lookup_offset_us64
			(PTS_TYPE_VIDEO, stream_offset, &vf->pts, 0,
			 &vf->pts_us64) != 0) {
#ifdef DEBUG_PTS
			pbi->pts_missed++;
#endif
			vf->pts = 0;
			vf->pts_us64 = 0;
		}
#ifdef DEBUG_PTS
		else
			pbi->pts_hit++;
#endif
		if (pts_unstable)
			pbi->pts_mode = PTS_NONE_REF_USE_DURATION;

		if ((pbi->pts_mode == PTS_NORMAL) && (vf->pts != 0)
			&& pbi->get_frame_dur) {
			int pts_diff = (int)vf->pts - pbi->last_lookup_pts;

			if (pts_diff < 0) {
				pbi->pts_mode_switching_count++;
				pbi->pts_mode_recovery_count = 0;

				if (pbi->pts_mode_switching_count >=
					PTS_MODE_SWITCHING_THRESHOLD) {
					pbi->pts_mode =
						PTS_NONE_REF_USE_DURATION;
					pr_info
					("HEVC: switch to n_d mode.\n");
				}

			} else {
				int p = PTS_MODE_SWITCHING_RECOVERY_THREASHOLD;
				pbi->pts_mode_recovery_count++;
				if (pbi->pts_mode_recovery_count > p) {
					pbi->pts_mode_switching_count = 0;
					pbi->pts_mode_recovery_count = 0;
				}
			}
		}

		if (vf->pts != 0)
			pbi->last_lookup_pts = vf->pts;

		if ((pbi->pts_mode == PTS_NONE_REF_USE_DURATION)
			&& (slice_type != KEY_FRAME))
			vf->pts = pbi->last_pts + DUR2PTS(pbi->frame_dur);
		pbi->last_pts = vf->pts;

		if (vf->pts_us64 != 0)
			pbi->last_lookup_pts_us64 = vf->pts_us64;

		if ((pbi->pts_mode == PTS_NONE_REF_USE_DURATION)
			&& (slice_type != KEY_FRAME)) {
			vf->pts_us64 =
				pbi->last_pts_us64 +
				(DUR2PTS(pbi->frame_dur) * 100 / 9);
		}
		pbi->last_pts_us64 = vf->pts_us64;
		if ((debug & VP9_DEBUG_OUT_PTS) != 0) {
			pr_info
			("VP9 dec out pts: vf->pts=%d, vf->pts_us64 = %lld\n",
			 vf->pts, vf->pts_us64);
		}

		vf->index = 0xff00 | pic_config->index;
#if 1
/*SUPPORT_10BIT*/
		if (double_write_mode & 0x10) {
			/* double write only */
			vf->compBodyAddr = 0;
			vf->compHeadAddr = 0;
		} else {
#ifdef VP9_10B_MMU
		vf->compBodyAddr = 0;
		vf->compHeadAddr = pic_config->header_adr;
#else
		vf->compBodyAddr = pic_config->mc_y_adr; /*body adr*/
		vf->compHeadAddr = pic_config->mc_y_adr +
					pic_config->comp_body_size;
		/*head adr*/
#endif
		}
		if (double_write_mode) {
			vf->type = VIDTYPE_PROGRESSIVE |
				VIDTYPE_VIU_FIELD;
			vf->type |= VIDTYPE_VIU_NV21;
#ifdef MULTI_INSTANCE_SUPPORT
			if (pbi->m_ins_flag) {
					vf->canvas0Addr = vf->canvas1Addr = -1;
					vf->plane_num = 2;
					vf->canvas0_config[0] =
						pic_config->canvas_config[0];
					vf->canvas0_config[1] =
						pic_config->canvas_config[1];

					vf->canvas1_config[0] =
						pic_config->canvas_config[0];
					vf->canvas1_config[1] =
						pic_config->canvas_config[1];

			} else
#endif
				vf->canvas0Addr = vf->canvas1Addr =
					spec2canvas(pic_config);
		} else {
			vf->canvas0Addr = vf->canvas1Addr = 0;
			vf->type = VIDTYPE_COMPRESS | VIDTYPE_VIU_FIELD;
#ifdef VP9_10B_MMU
			vf->type |= VIDTYPE_SCATTER;
#endif
			switch (pic_config->bit_depth) {
			case VPX_BITS_8:
				vf->bitdepth = BITDEPTH_Y8 |
					BITDEPTH_U8 | BITDEPTH_V8;
				break;
			case VPX_BITS_10:
			case VPX_BITS_12:
				vf->bitdepth = BITDEPTH_Y10 |
					BITDEPTH_U10 | BITDEPTH_V10;
				break;
			default:
				vf->bitdepth = BITDEPTH_Y10 |
					BITDEPTH_U10 | BITDEPTH_V10;
				break;
			}
			if (pic_config->bit_depth == VPX_BITS_8)
				vf->bitdepth |= BITDEPTH_SAVING_MODE;
	}
#else
		vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
		vf->type |= VIDTYPE_VIU_NV21;
		vf->canvas0Addr = vf->canvas1Addr = spec2canvas(pic_config);
#endif
		set_frame_info(pbi, vf);
		/* if((vf->width!=pic_config->width)|
			(vf->height!=pic_config->height)) */
		/* pr_info("aaa: %d/%d, %d/%d\n",
		   vf->width,vf->height, pic_config->width,
			pic_config->height); */
		if (double_write_mode == 2) {
			vf->width = pic_config->y_crop_width/4;
			vf->height = pic_config->y_crop_height/4;
		} else {
			vf->width = pic_config->y_crop_width;
			vf->height = pic_config->y_crop_height;
		}
		if (force_w_h != 0) {
			vf->width = (force_w_h >> 16) & 0xffff;
			vf->height = force_w_h & 0xffff;
		}
		vf->compWidth = pic_config->y_crop_width;
		vf->compHeight = pic_config->y_crop_height;
		if (force_fps & 0x100) {
			u32 rate = force_fps & 0xff;
			if (rate)
				vf->duration = 96000/rate;
			else
				vf->duration = 0;
		}
		if (vf->type & VIDTYPE_SCATTER) {
			vf->mem_handle = decoder_mmu_box_get_mem_handle(
				pbi->mmu_box,
				pic_config->index);
		} else {
			vf->mem_handle = decoder_bmmu_box_get_mem_handle(
				pbi->bmmu_box,
				pic_config->index);
		}
		inc_vf_ref(pbi, pic_config->index);
		kfifo_put(&pbi->display_q, (const struct vframe_s *)vf);

		/*count info*/
		gvs->frame_dur = pbi->frame_dur;
		vdec_count_info(gvs, 0, stream_offset);

		vf_notify_receiver(pbi->provider_name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	}

	return 0;
}

static void get_rpm_param(union param_u *params)
{
	int i;
	unsigned int data32;
	if (debug & VP9_DEBUG_BUFMGR)
		pr_info("enter %s\r\n", __func__);
	for (i = 0; i < 128; i++) {
		do {
			data32 = READ_VREG(RPM_CMD_REG);
			/*pr_info("%x\n", data32);*/
		} while ((data32 & 0x10000) == 0);
		params->l.data[i] = data32&0xffff;
		/*pr_info("%x\n", data32);*/
		WRITE_VREG(RPM_CMD_REG, 0);
	}
	if (debug & VP9_DEBUG_BUFMGR)
		pr_info("leave %s\r\n", __func__);
}
static void debug_buffer_mgr_more(struct VP9Decoder_s *pbi)
{
	int i;
	if (!(debug & VP9_DEBUG_BUFMGR_MORE))
		return;
	pr_info("vp9_param: (%d)\n", pbi->slice_idx);
	for (i = 0; i < (RPM_END-RPM_BEGIN); i++) {
		pr_info("%04x ", vp9_param.l.data[i]);
		if (((i + 1) & 0xf) == 0)
			pr_info("\n");
	}
	pr_info("=============param==========\r\n");
	pr_info("profile               %x\r\n", vp9_param.p.profile);
	pr_info("show_existing_frame   %x\r\n",
	vp9_param.p.show_existing_frame);
	pr_info("frame_to_show_idx     %x\r\n",
	vp9_param.p.frame_to_show_idx);
	pr_info("frame_type            %x\r\n", vp9_param.p.frame_type);
	pr_info("show_frame            %x\r\n", vp9_param.p.show_frame);
	pr_info("e.r.r.o.r_resilient_mode  %x\r\n",
	vp9_param.p.error_resilient_mode);
	pr_info("intra_only            %x\r\n", vp9_param.p.intra_only);
	pr_info("display_size_present  %x\r\n",
	vp9_param.p.display_size_present);
	pr_info("reset_frame_context   %x\r\n",
	vp9_param.p.reset_frame_context);
	pr_info("refresh_frame_flags   %x\r\n",
	vp9_param.p.refresh_frame_flags);
	pr_info("bit_depth             %x\r\n", vp9_param.p.bit_depth);
	pr_info("width                 %x\r\n", vp9_param.p.width);
	pr_info("height                %x\r\n", vp9_param.p.height);
	pr_info("display_width         %x\r\n", vp9_param.p.display_width);
	pr_info("display_height        %x\r\n", vp9_param.p.display_height);
	pr_info("ref_info              %x\r\n", vp9_param.p.ref_info);
	pr_info("same_frame_size       %x\r\n", vp9_param.p.same_frame_size);
	if (!(debug & VP9_DEBUG_DBG_LF_PRINT))
		return;
	pr_info("mode_ref_delta_enabled: 0x%x\r\n",
	vp9_param.p.mode_ref_delta_enabled);
	pr_info("sharpness_level: 0x%x\r\n",
	vp9_param.p.sharpness_level);
	pr_info("ref_deltas: 0x%x, 0x%x, 0x%x, 0x%x\r\n",
	vp9_param.p.ref_deltas[0], vp9_param.p.ref_deltas[1],
	vp9_param.p.ref_deltas[2], vp9_param.p.ref_deltas[3]);
	pr_info("mode_deltas: 0x%x, 0x%x\r\n", vp9_param.p.mode_deltas[0],
	vp9_param.p.mode_deltas[1]);
	pr_info("filter_level: 0x%x\r\n", vp9_param.p.filter_level);
	pr_info("seg_enabled: 0x%x\r\n", vp9_param.p.seg_enabled);
	pr_info("seg_abs_delta: 0x%x\r\n", vp9_param.p.seg_abs_delta);
	pr_info("seg_lf_feature_enabled: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\r\n",
	(vp9_param.p.seg_lf_info[0]>>15 & 1),
	(vp9_param.p.seg_lf_info[1]>>15 & 1),
	(vp9_param.p.seg_lf_info[2]>>15 & 1),
	(vp9_param.p.seg_lf_info[3]>>15 & 1),
	(vp9_param.p.seg_lf_info[4]>>15 & 1),
	(vp9_param.p.seg_lf_info[5]>>15 & 1),
	(vp9_param.p.seg_lf_info[6]>>15 & 1),
	(vp9_param.p.seg_lf_info[7]>>15 & 1));
	pr_info("seg_lf_feature_data: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\r\n",
	(vp9_param.p.seg_lf_info[0] & 0x13f),
	(vp9_param.p.seg_lf_info[1] & 0x13f),
	(vp9_param.p.seg_lf_info[2] & 0x13f),
	(vp9_param.p.seg_lf_info[3] & 0x13f),
	(vp9_param.p.seg_lf_info[4] & 0x13f),
	(vp9_param.p.seg_lf_info[5] & 0x13f),
	(vp9_param.p.seg_lf_info[6] & 0x13f),
	(vp9_param.p.seg_lf_info[7] & 0x13f));

}

static irqreturn_t vvp9_isr_thread_fn(int irq, void *data)
{
	struct VP9Decoder_s *pbi = (struct VP9Decoder_s *)data;
	unsigned int dec_status = pbi->dec_status;
	struct VP9_Common_s *const cm = &pbi->common;
	int i, ret;

	/*if (pbi->wait_buf)
		pr_info("set wait_buf to 0\r\n");
	*/
	pbi->wait_buf = 0;

	if (dec_status == VP9_EOS) {
		pr_info("VP9_EOS, flush buffer\r\n");

		vp9_bufmgr_postproc(pbi);

		pr_info("send VP9_10B_DISCARD_NAL\r\n");
		WRITE_VREG(HEVC_DEC_STATUS_REG, VP9_10B_DISCARD_NAL);
		pbi->process_busy = 0;
		return IRQ_HANDLED;
	}

	if (dec_status != VP9_HEAD_PARSER_DONE) {
		pbi->process_busy = 0;
		return IRQ_HANDLED;
	}
	if (pbi->frame_count > 0)
		vp9_bufmgr_postproc(pbi);

	if (debug & VP9_DEBUG_SEND_PARAM_WITH_REG) {
		get_rpm_param(&vp9_param);
	} else {
		dma_sync_single_for_cpu(
			amports_get_dma_device(),
			pbi->rpm_phy_addr,
			RPM_BUF_SIZE,
			DMA_FROM_DEVICE);

		for (i = 0; i < (RPM_END - RPM_BEGIN); i += 4) {
			int ii;
			for (ii = 0; ii < 4; ii++)
				vp9_param.l.data[i + ii] =
					pbi->rpm_ptr[i + 3 - ii];
		   }
	}
	debug_buffer_mgr_more(pbi);

	bit_depth_luma = vp9_param.p.bit_depth;
	bit_depth_chroma = vp9_param.p.bit_depth;

	ret = vp9_bufmgr_process(pbi, &vp9_param);
	pbi->slice_idx++;
	if (ret < 0) {
		pr_info("vp9_bufmgr_process=> %d, VP9_10B_DISCARD_NAL\r\n",
		 ret);
		WRITE_VREG(HEVC_DEC_STATUS_REG, VP9_10B_DISCARD_NAL);
		pbi->process_busy = 0;
		return IRQ_HANDLED;
	} else if (ret == 0) {
		pbi->frame_count++;
		/*pr_info("Decode Frame Data %d\n", pbi->frame_count);*/
		config_pic_size(pbi, vp9_param.p.bit_depth);
		if ((pbi->common.frame_type != KEY_FRAME)
			&& (!pbi->common.intra_only)) {
			config_mc_buffer(pbi, vp9_param.p.bit_depth);
			config_mpred_hw(pbi);
	} else {
		clear_mpred_hw(pbi);
	}
#ifdef MCRCC_ENABLE
	config_mcrcc_axi_hw(pbi);
#endif
	config_sao_hw(pbi, &vp9_param);

#ifdef VP9_LPF_LVL_UPDATE
    /*
     * Get loop filter related picture level parameters from Parser
     */
	lf->mode_ref_delta_enabled = vp9_param.p.mode_ref_delta_enabled;
	lf->sharpness_level = vp9_param.p.sharpness_level;
	for (i = 0; i < 4; i++)
		lf->ref_deltas[i] = vp9_param.p.ref_deltas[i];
	for (i = 0; i < 2; i++)
		lf->mode_deltas[i] = vp9_param.p.mode_deltas[i];
	default_filt_lvl = vp9_param.p.filter_level;
	seg_4lf->enabled = vp9_param.p.seg_enabled;
	seg_4lf->abs_delta = vp9_param.p.seg_abs_delta;
	for (i = 0; i < MAX_SEGMENTS; i++)
		seg_4lf->feature_mask[i] = (vp9_param.p.seg_lf_info[i] &
		0x8000) ? (1 << SEG_LVL_ALT_LF) : 0;
		for (i = 0; i < MAX_SEGMENTS; i++)
			seg_4lf->feature_data[i][SEG_LVL_ALT_LF]
			= (vp9_param.p.seg_lf_info[i]
			& 0x100) ? -(vp9_param.p.seg_lf_info[i]
			& 0x3f) : (vp9_param.p.seg_lf_info[i] & 0x3f);
	/*
	* Update loop filter Thr/Lvl table for every frame
	*/
	/*pr_info
	("vp9_loop_filter (run before every frame decoding start)\n");*/
	vp9_loop_filter_frame_init(seg_4lf, lfi, lf, default_filt_lvl);
#endif
	/*pr_info("HEVC_DEC_STATUS_REG <= VP9_10B_DECODE_SLICE\n");*/

	WRITE_VREG(HEVC_DEC_STATUS_REG, VP9_10B_DECODE_SLICE);
	} else {
	pr_info("Skip search next start code\n");
	cm->prev_fb_idx = INVALID_IDX;
	/*skip, search next start code*/
	WRITE_VREG(HEVC_DEC_STATUS_REG, VP9_10B_DECODE_SLICE);
	}
	pbi->process_busy = 0;
#ifdef VP9_10B_MMU
	if (pbi->last_put_idx >= 0 && pbi->last_put_idx < FRAME_BUFFERS) {
		struct RefCntBuffer_s *frame_bufs = cm->buffer_pool->frame_bufs;
		int i = pbi->last_put_idx;
		/*free not used buffers.*/
		if ((frame_bufs[i].ref_count == 0) &&
			(frame_bufs[i].buf.vf_ref == 0) &&
			(frame_bufs[i].buf.used_by_display == 0) &&
			(frame_bufs[i].buf.index != -1)) {
			decoder_mmu_box_free_idx(pbi->mmu_box, i);
		}
		pbi->last_put_idx = -1;
	}
#endif
	return IRQ_HANDLED;
}

static irqreturn_t vvp9_isr(int irq, void *data)
{
	int i;
	unsigned int dec_status;
	struct VP9Decoder_s *pbi = (struct VP9Decoder_s *)data;
	unsigned int adapt_prob_status;
	struct VP9_Common_s *const cm = &pbi->common;
	dec_status = READ_VREG(HEVC_DEC_STATUS_REG);
	adapt_prob_status = READ_VREG(VP9_ADAPT_PROB_REG);
	if (pbi->init_flag == 0)
		return IRQ_HANDLED;
	if (pbi->process_busy)/*on process.*/
		return IRQ_HANDLED;
	pbi->dec_status = dec_status;
	pbi->process_busy = 1;
	if (debug & VP9_DEBUG_BUFMGR)
		pr_info("vp9 isr dec status  = %d\n", dec_status);

	if (debug & VP9_DEBUG_UCODE) {
		if (READ_HREG(DEBUG_REG1) & 0x10000) {
			dma_sync_single_for_cpu(
				amports_get_dma_device(),
				pbi->lmem_phy_addr,
				LMEM_BUF_SIZE,
				DMA_FROM_DEVICE);

			pr_info("LMEM<tag %x>:\n", READ_HREG(DEBUG_REG1));
			for (i = 0; i < 0x400; i += 4) {
				int ii;
				if ((i & 0xf) == 0)
					pr_info("%03x: ", i);
				for (ii = 0; ii < 4; ii++) {
					pr_info("%04x ",
						   pbi->lmem_ptr[i + 3 - ii]);
				}
				if (((i + ii) & 0xf) == 0)
					pr_info("\n");
			}
			WRITE_HREG(DEBUG_REG1, 0);
		} else if (READ_HREG(DEBUG_REG1) != 0) {
			pr_info("dbg%x: %x\n", READ_HREG(DEBUG_REG1),
				   READ_HREG(DEBUG_REG2));
			WRITE_HREG(DEBUG_REG1, 0);
			pbi->process_busy = 0;
			return IRQ_HANDLED;
		}

	}

	if (pbi->error_flag == 1) {
		pbi->error_flag = 2;
		pbi->process_busy = 0;
		return IRQ_HANDLED;
	} else if (pbi->error_flag == 3) {
		pbi->process_busy = 0;
		return IRQ_HANDLED;
	}

	if (is_buffer_empty(cm)) {
		/*
		if (pbi->wait_buf == 0)
			pr_info("set wait_buf to 1\r\n");
		*/
		pbi->wait_buf = 1;
		pbi->process_busy = 0;
		return IRQ_HANDLED;
	}
	if ((adapt_prob_status & 0xff) == 0xfd) {
		/*VP9_REQ_ADAPT_PROB*/
		int pre_fc = (cm->frame_type == KEY_FRAME) ? 1 : 0;
		uint8_t *prev_prob_b =
		((uint8_t *)pbi->prob_buffer_addr) +
		((adapt_prob_status >> 8) * 0x1000);
		uint8_t *cur_prob_b =
		((uint8_t *)pbi->prob_buffer_addr) + 0x4000;
		uint8_t *count_b = (uint8_t *)pbi->count_buffer_addr;

		adapt_coef_probs(pbi->pic_count,
			(cm->last_frame_type == KEY_FRAME),
			pre_fc, (adapt_prob_status >> 8),
			(unsigned int *)prev_prob_b,
			(unsigned int *)cur_prob_b, (unsigned int *)count_b);

		memcpy(prev_prob_b, cur_prob_b, PROB_SIZE);
		WRITE_VREG(VP9_ADAPT_PROB_REG, 0);
		pbi->pic_count += 1;

		/*return IRQ_HANDLED;*/
	}
#ifdef MULTI_INSTANCE_SUPPORT
#if 0
	if ((dec_status == HEVC_DECPIC_DATA_DONE) && (pbi->m_ins_flag)) {
		if (pbi->chunk) {
			pbi->cur_pic->pts = pbi->chunk->pts;
			pbi->cur_pic->pts64 = pbi->chunk->pts64;
		} else if (pts_lookup_offset_us64
			(PTS_TYPE_VIDEO,
			pbi->cur_pic->stream_offset,
			&pbi->cur_pic->pts,
			0,
			&pbi->cur_pic->pts64) != 0) {
#ifdef DEBUG_PTS
			pbi->pts_missed++;
#endif
			pbi->cur_pic->pts = 0;
			pbi->cur_pic->pts64 = 0;
		}
	}
#endif
	if (dec_status == HEVC_NAL_DECODE_DONE) {
		if (pbi->m_ins_flag) {
#if 0
			if (!vdec_frame_based(hw_to_vdec(hevc))) {
				pbi->dec_result = DEC_RESULT_AGAIN;
				if ((debug &
				ONLY_RESET_AT_START) == 0)
					amhevc_stop();
			} else
				pbi->dec_result = DEC_RESULT_GET_DATA;
#else
			if (!vdec_frame_based(hw_to_vdec(pbi)))
				pbi->dec_result = DEC_RESULT_AGAIN;
			else
				pbi->dec_result = DEC_RESULT_DONE;
			amhevc_stop();
#endif
			schedule_work(&pbi->work);
		}
		pbi->process_busy = 0;
		return IRQ_HANDLED;
	} else if (dec_status == HEVC_DECPIC_DATA_DONE) {
		if (pbi->m_ins_flag) {
			pbi->dec_result = DEC_RESULT_DONE;
			amhevc_stop();
			schedule_work(&pbi->work);
		}

		pbi->process_busy = 0;
		return IRQ_HANDLED;
	} else if (
			(dec_status == HEVC_SEARCH_BUFEMPTY) ||
			(dec_status == HEVC_DECODE_BUFEMPTY) ||
			(dec_status == HEVC_DECODE_TIMEOUT)) {
		if (vdec_frame_based(hw_to_vdec(pbi)) ||
			(READ_VREG(HEVC_STREAM_LEVEL) > 0x200)) {
			if (debug & VP9_DEBUG_DIS_LOC_ERROR_PROC) {
				vp9_print(pbi, PRINT_FLAG_ERROR,
					"%s decoding error, level 0x%x\n",
					__func__, READ_VREG(HEVC_STREAM_LEVEL));
				goto send_again;
			}
			amhevc_stop();
			vp9_print(pbi, PRINT_FLAG_UCODE_EVT,
				"%s %s\n", __func__,
				(dec_status == HEVC_SEARCH_BUFEMPTY) ?
				"HEVC_SEARCH_BUFEMPTY" :
				(dec_status == HEVC_DECODE_BUFEMPTY) ?
				"HEVC_DECODE_BUFEMPTY" : "HEVC_DECODE_TIMEOUT");
			pbi->dec_result = DEC_RESULT_DONE;

			schedule_work(&pbi->work);
		} else {
			/* WRITE_VREG(DPB_STATUS_REG, H264_ACTION_INIT); */
			vp9_print(pbi, PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_AGAIN\n", __func__);
send_again:
			pbi->dec_result = DEC_RESULT_AGAIN;
			schedule_work(&pbi->work);
		}
		pbi->process_busy = 0;
		return IRQ_HANDLED;
	}
#endif

	return IRQ_WAKE_THREAD;
}

static void vvp9_put_timer_func(unsigned long arg)
{
	struct VP9Decoder_s *pbi = (struct VP9Decoder_s *)arg;
	struct timer_list *timer = &pbi->timer;
	uint8_t empty_flag;
	unsigned int buf_level;

	enum receviver_start_e state = RECEIVER_INACTIVE;
	if (pbi->m_ins_flag) {
		if (hw_to_vdec(pbi)->next_status
			== VDEC_STATUS_DISCONNECTED) {
			pbi->dec_result = DEC_RESULT_DONE;
			schedule_work(&pbi->work);
			pr_info(
			"vdec requested to be disconnected\n");
			return;
		}
	}
	if (pbi->init_flag == 0) {
		if (pbi->stat & STAT_TIMER_ARM) {
			timer->expires = jiffies + PUT_INTERVAL;
			add_timer(&pbi->timer);
		}
		return;
	}
	if (pbi->m_ins_flag == 0) {
		if (vf_get_receiver(pbi->provider_name)) {
			state =
				vf_notify_receiver(pbi->provider_name,
					VFRAME_EVENT_PROVIDER_QUREY_STATE,
					NULL);
			if ((state == RECEIVER_STATE_NULL)
				|| (state == RECEIVER_STATE_NONE))
				state = RECEIVER_INACTIVE;
		} else
			state = RECEIVER_INACTIVE;

		empty_flag = (READ_VREG(HEVC_PARSER_INT_STATUS) >> 6) & 0x1;
		/* error watchdog */
		if (empty_flag == 0) {
			/* decoder has input */
			if ((debug & VP9_DEBUG_DIS_LOC_ERROR_PROC) == 0) {

				buf_level = READ_VREG(HEVC_STREAM_LEVEL);
				/* receiver has no buffer to recycle */
				if ((state == RECEIVER_INACTIVE) &&
					(kfifo_is_empty(&pbi->display_q) &&
					 buf_level > 0x200)
					) {
						WRITE_VREG
						(HEVC_ASSIST_MBOX1_IRQ_REG,
						 0x1);
				}
			}

			if ((debug & VP9_DEBUG_DIS_SYS_ERROR_PROC) == 0) {
				/* receiver has no buffer to recycle */
				/*if ((state == RECEIVER_INACTIVE) &&
					(kfifo_is_empty(&pbi->display_q))) {
				pr_info("vp9 something error,need reset\n");
				}*/
			}
		}
	}

	if (decode_stop_pos != decode_stop_pos_pre) {
		WRITE_VREG(DECODE_STOP_POS, decode_stop_pos);
		decode_stop_pos_pre = decode_stop_pos;
	}

	if (debug & VP9_DEBUG_DUMP_PIC_LIST) {
		dump_pic_list(pbi);
		debug &= ~VP9_DEBUG_DUMP_PIC_LIST;
	}
	if (debug & VP9_DEBUG_TRIG_SLICE_SEGMENT_PROC) {
		WRITE_VREG(HEVC_ASSIST_MBOX1_IRQ_REG, 0x1);
		debug &= ~VP9_DEBUG_TRIG_SLICE_SEGMENT_PROC;
	}
	/*if (debug & VP9_DEBUG_HW_RESET) {
	}*/
	if (debug & VP9_DEBUG_ERROR_TRIG) {
		WRITE_VREG(DECODE_STOP_POS, 1);
		debug &= ~VP9_DEBUG_ERROR_TRIG;
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
	if (pop_shorts != 0) {
		int i;
		u32 sum = 0;
		pr_info("pop stream 0x%x shorts\r\n", pop_shorts);
		for (i = 0; i < pop_shorts; i++) {
			u32 data =
			(READ_HREG(HEVC_SHIFTED_DATA) >> 16);
			WRITE_HREG(HEVC_SHIFT_COMMAND,
			(1<<7)|16);
			if ((i & 0xf) == 0)
				pr_info("%04x:", i);
			pr_info("%04x ", data);
			if (((i + 1) & 0xf) == 0)
				pr_info("\r\n");
			sum += data;
		}
		pr_info("\r\nsum = %x\r\n", sum);
		pop_shorts = 0;
	}
	if (dbg_cmd != 0) {
		if (dbg_cmd == 1) {
			u32 disp_laddr;
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXBB &&
				double_write_mode == 0) {
				disp_laddr =
					READ_VCBUS_REG(AFBC_BODY_BADDR) << 4;
			} else {
				struct canvas_s cur_canvas;
				canvas_read((READ_VCBUS_REG(VD1_IF0_CANVAS0)
					& 0xff), &cur_canvas);
				disp_laddr = cur_canvas.addr;
			}
			pr_info("current displayed buffer address %x\r\n",
				disp_laddr);
		}
		dbg_cmd = 0;
	}
	/*don't changed at start.*/
	if (pbi->get_frame_dur && pbi->show_frame_num > 60 &&
		pbi->frame_dur > 0 && pbi->saved_resolution !=
		frame_width * frame_height *
			(96000 / pbi->frame_dur)) {
		int fps = 96000 / pbi->frame_dur;
		if (hevc_source_changed(VFORMAT_VP9,
			frame_width, frame_height, fps) > 0)
			pbi->saved_resolution = frame_width *
			frame_height * fps;
	}

	timer->expires = jiffies + PUT_INTERVAL;
	add_timer(timer);
}


int vvp9_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	struct VP9Decoder_s *vp9 = &gHevc;
	vstatus->frame_width = frame_width;
	vstatus->frame_height = frame_height;
	if (vp9->frame_dur != 0)
		vstatus->frame_rate = 96000 / vp9->frame_dur;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = 0;
	vstatus->status = vp9->stat | vp9->fatal_error;
	vstatus->bit_rate = gvs->bit_rate;
	vstatus->frame_dur = vp9->frame_dur;
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

static int vvp9_vdec_info_init(void)
{
	gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);
	if (NULL == gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}
	return 0;
}

#if 0
static void VP9_DECODE_INIT(void)
{
	/* enable vp9 clocks */
	WRITE_VREG(DOS_GCLK_EN3, 0xffffffff);
	/* *************************************************************** */
	/* Power ON HEVC */
	/* *************************************************************** */
	/* Powerup HEVC */
	WRITE_VREG(AO_RTI_GEN_PWR_SLEEP0,
			READ_VREG(AO_RTI_GEN_PWR_SLEEP0) & (~(0x3 << 6)));
	WRITE_VREG(DOS_MEM_PD_HEVC, 0x0);
	WRITE_VREG(DOS_SW_RESET3, READ_VREG(DOS_SW_RESET3) | (0x3ffff << 2));
	WRITE_VREG(DOS_SW_RESET3, READ_VREG(DOS_SW_RESET3) & (~(0x3ffff << 2)));
	/* remove isolations */
	WRITE_VREG(AO_RTI_GEN_PWR_ISO0,
			READ_VREG(AO_RTI_GEN_PWR_ISO0) & (~(0x3 << 10)));

}
#endif

static void vvp9_prot_init(struct VP9Decoder_s *pbi)
{
	unsigned int data32;
	/* VP9_DECODE_INIT(); */
	vp9_config_work_space_hw(pbi);
	init_pic_list_hw(pbi);

	vp9_init_decoder_hw(pbi);

#ifdef VP9_LPF_LVL_UPDATE
	vp9_loop_filter_init();
#endif

#if 1
	if (debug & VP9_DEBUG_BUFMGR)
		pr_info("[test.c] Enable BitStream Fetch\n");
	data32 = READ_VREG(HEVC_STREAM_CONTROL);
	data32 = data32 |
		(1 << 0)/*stream_fetch_enable*/
		;
	WRITE_VREG(HEVC_STREAM_CONTROL, data32);
#if 0
	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x00000100) {
		pr_info("vp9 prot init error %d\n", __LINE__);
		return;
	}
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x00000300) {
		pr_info("vp9 prot init error %d\n", __LINE__);
		return;
	}
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x12345678);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x9abcdef0);
	data32 = READ_VREG(HEVC_SHIFT_STARTCODE);
	if (data32 != 0x12345678) {
		pr_info("vp9 prot init error %d\n", __LINE__);
		return;
	}
	data32 = READ_VREG(HEVC_SHIFT_EMULATECODE);
	if (data32 != 0x9abcdef0) {
		pr_info("vp9 prot init error %d\n", __LINE__);
		return;
	}
#endif
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x000000001);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x00000300);
#endif



	WRITE_VREG(HEVC_WAIT_FLAG, 1);

	/* WRITE_VREG(HEVC_MPSR, 1); */

	/* clear mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX1_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX1_MASK, 1);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(HEVC_PSCALE_CTRL, 0);

	if (debug & VP9_DEBUG_UCODE)
		WRITE_VREG(DEBUG_REG1, 0x1);
	else
		WRITE_VREG(DEBUG_REG1, 0x0);
	/*check vps/sps/pps/i-slice in ucode*/
	WRITE_VREG(NAL_SEARCH_CTL, 0x8);

	WRITE_VREG(DECODE_STOP_POS, decode_stop_pos);

}

static int vvp9_local_init(struct VP9Decoder_s *pbi)
{
	int i;
	int ret;
	int width, height;
#ifdef DEBUG_PTS
	pbi->pts_missed = 0;
	pbi->pts_hit = 0;
#endif
	pbi->new_frame_displayed = 0;
	pbi->last_put_idx = -1;
	pbi->saved_resolution = 0;
	pbi->get_frame_dur = false;
	on_no_keyframe_skiped = 0;
	width = pbi->vvp9_amstream_dec_info.width;
	height = pbi->vvp9_amstream_dec_info.height;
	pbi->frame_dur =
		(pbi->vvp9_amstream_dec_info.rate ==
		 0) ? 3600 : pbi->vvp9_amstream_dec_info.rate;
	if (width && height)
		pbi->frame_ar = height * 0x100 / width;
/*
TODO:FOR VERSION
*/
	pr_info("vp9: ver (%d,%d) decinfo: %dx%d rate=%d\n", vp9_version,
		   0, width, height, pbi->frame_dur);

	if (pbi->frame_dur == 0)
		pbi->frame_dur = 96000 / 24;

	INIT_KFIFO(pbi->display_q);
	INIT_KFIFO(pbi->newframe_q);


	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &pbi->vfpool[i];
		pbi->vfpool[i].index = -1;
		kfifo_put(&pbi->newframe_q, vf);
	}


	ret = vp9_local_init(pbi);

	return ret;
}

static s32 vvp9_init(struct VP9Decoder_s *pbi)
{
	init_timer(&pbi->timer);

	pbi->stat |= STAT_TIMER_INIT;
	if (vvp9_local_init(pbi) < 0)
		return -EBUSY;

#ifdef MULTI_INSTANCE_SUPPORT
	if (pbi->m_ins_flag) {
		pbi->timer.data = (ulong) pbi;
		pbi->timer.function = vvp9_put_timer_func;
		pbi->timer.expires = jiffies + PUT_INTERVAL;

		add_timer(&pbi->timer);

		pbi->stat |= STAT_TIMER_ARM;

		INIT_WORK(&pbi->work, vp9_work);
		return 0;
	}
#endif

	amhevc_enable();
	if (debug & VP9_DEBUG_LOAD_UCODE_FROM_FILE) {
		pr_info("load ucode from file of vvp9_mc_debug\n");
		if (amhevc_loadmc_ex(VFORMAT_VP9,
				"vvp9_mc_debug", NULL) < 0) {
			amhevc_disable();
			return -EBUSY;
		}
#if 0
	} else if (double_write_mode & 0x10) {
		pr_info("load ucode from file of vvp9_mc_dw\n");
		if (amhevc_loadmc_ex(VFORMAT_VP9,
				"vvp9_mc_dw", NULL) < 0) {
			amhevc_disable();
			return -EBUSY;
		}
#endif
	} else if (amhevc_loadmc_ex(VFORMAT_VP9, "vvp9_mc", NULL) < 0) {
		amhevc_disable();
		return -EBUSY;
	}
	pbi->stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	vvp9_prot_init(pbi);

	if (vdec_request_threaded_irq(VDEC_IRQ_1,
				vvp9_isr,
				vvp9_isr_thread_fn,
				IRQF_ONESHOT,/*run thread on this irq disabled*/
				"vvp9-irq", (void *)pbi)) {
		pr_info("vvp9 irq register error.\n");
		amhevc_disable();
		return -ENOENT;
	}

	pbi->stat |= STAT_ISR_REG;

	pbi->provider_name = PROVIDER_NAME;
	vf_provider_init(&vvp9_vf_prov, PROVIDER_NAME, &vvp9_vf_provider,
					 pbi);
	vf_reg_provider(&vvp9_vf_prov);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);

	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_FR_HINT,
	(void *)((unsigned long)pbi->frame_dur));

	pbi->stat |= STAT_VF_HOOK;

	pbi->timer.data = (ulong)pbi;
	pbi->timer.function = vvp9_put_timer_func;
	pbi->timer.expires = jiffies + PUT_INTERVAL;


	add_timer(&pbi->timer);

	pbi->stat |= STAT_TIMER_ARM;

	/* pbi->stat |= STAT_KTHREAD; */

	amhevc_start();

	pbi->stat |= STAT_VDEC_RUN;

	pbi->init_flag = 1;
	pbi->process_busy = 0;
	pr_info("%d, vvp9_init, RP=0x%x\n",
		__LINE__, READ_VREG(HEVC_STREAM_RD_PTR));
	return 0;
}

static int vvp9_stop(struct VP9Decoder_s *pbi)
{

	pbi->init_flag = 0;
	/*
	if ((debug & VP9_DEBUG_NOWAIT_DECODE_DONE_WHEN_STOP) == 0) {
		int wait_timeout_count = 0;
		while ((READ_VREG(HEVC_DEC_STATUS_REG) ==
			   VP9_10B_DECODE_SLICE &&
				wait_timeout_count < 10) ||
				pbi->process_busy){
			wait_timeout_count++;
			msleep(20);
		}
	}
	*/
	if (pbi->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		pbi->stat &= ~STAT_VDEC_RUN;
	}

	if (pbi->stat & STAT_ISR_REG) {
		WRITE_VREG(HEVC_ASSIST_MBOX1_MASK, 0);
		vdec_free_irq(VDEC_IRQ_1, (void *)pbi);
		pbi->stat &= ~STAT_ISR_REG;
	}

	if (pbi->stat & STAT_TIMER_ARM) {
		del_timer_sync(&pbi->timer);
		pbi->stat &= ~STAT_TIMER_ARM;
	}

	if (pbi->stat & STAT_VF_HOOK) {
		vf_notify_receiver(pbi->provider_name,
				VFRAME_EVENT_PROVIDER_FR_END_HINT, NULL);

		vf_unreg_provider(&vvp9_vf_prov);
		pbi->stat &= ~STAT_VF_HOOK;
	}
	vp9_local_uninit(pbi);

#ifdef MULTI_INSTANCE_SUPPORT
	if (pbi->m_ins_flag) {
		cancel_work_sync(&pbi->work);
	} else {
		amhevc_disable();
	}
#else
	amhevc_disable();
#endif
	uninit_mmu_buffers(pbi);

	kfree(gvs);
	gvs = NULL;

	return 0;
}

static int amvdec_vp9_mmu_init(struct VP9Decoder_s *pbi)
{
#ifdef VP9_10B_MMU
	pbi->mmu_box = decoder_mmu_box_alloc_box(DRIVER_NAME,
		0, FRAME_BUFFERS,
		48 * SZ_1M
		);
	if (!pbi->mmu_box) {
		pr_err("vp9 alloc mmu box failed!!\n");
		return -1;
	}
#endif
	pbi->bmmu_box = decoder_bmmu_box_alloc_box(
			DRIVER_NAME,
			pbi->index,
			MAX_BMMU_BUFFER_NUM,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER);
	if (!pbi->bmmu_box) {
		pr_err("vp9 alloc bmmu box failed!!\n");
		return -1;
	}
	return 0;
}
static int amvdec_vp9_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct BUF_s BUF[MAX_BUF_NUM];
	struct VP9Decoder_s *pbi = &gHevc;
	pr_info("%s\n", __func__);
	mutex_lock(&vvp9_mutex);

	memcpy(&BUF[0], &pbi->m_BUF[0], sizeof(struct BUF_s) * MAX_BUF_NUM);
	memset(pbi, 0, sizeof(VP9Decoder));
	memcpy(&pbi->m_BUF[0], &BUF[0], sizeof(struct BUF_s) * MAX_BUF_NUM);

	pbi->init_flag = 0;
	pbi->fatal_error = 0;
	pbi->show_frame_num = 0;
	if (pdata == NULL) {
		pr_info("\namvdec_vp9 memory resource undefined.\n");
		mutex_unlock(&vvp9_mutex);
		return -EFAULT;
	}
	pbi->m_ins_flag = 0;
#ifdef MULTI_INSTANCE_SUPPORT
	pbi->buf_start = pdata->mem_start;
	pbi->buf_size = pdata->mem_end - pdata->mem_start + 1;
#else
	pbi->mc_buf_spec.buf_end = pdata->mem_end + 1;
	for (i = 0; i < WORK_BUF_SPEC_NUM; i++)
		amvvp9_workbuff_spec[i].start_adr = pdata->mem_start;
#endif
	if (amvdec_vp9_mmu_init(pbi) < 0) {
		pr_err("vp9 alloc bmmu box failed!!\n");
		return -1;
	}
	if (debug) {
		pr_info("===VP9 decoder mem resource 0x%lx -- 0x%lx\n",
			   pdata->mem_start, pdata->mem_end + 1);
	}

	if (pdata->sys_info)
		pbi->vvp9_amstream_dec_info = *pdata->sys_info;
	else {
		pbi->vvp9_amstream_dec_info.width = 0;
		pbi->vvp9_amstream_dec_info.height = 0;
		pbi->vvp9_amstream_dec_info.rate = 30;
	}
#ifdef MULTI_INSTANCE_SUPPORT
	pbi->cma_dev = pdata->cma_dev;
#else
	cma_dev = pdata->cma_dev;
#endif
	pdata->dec_status = vvp9_dec_status;

	vvp9_vdec_info_init();

	if (vvp9_init(pbi) < 0) {
		pr_info("\namvdec_vp9 init failed.\n");
		vp9_local_uninit(pbi);
		mutex_unlock(&vvp9_mutex);
		return -ENODEV;
	}
	/*set the max clk for smooth playing...*/
	hevc_source_changed(VFORMAT_VP9,
			4096, 2048, 60);
	mutex_unlock(&vvp9_mutex);

	return 0;
}

static int amvdec_vp9_remove(struct platform_device *pdev)
{
	struct VP9Decoder_s *pbi = &gHevc;
	if (debug)
		pr_info("amvdec_vp9_remove\n");

	mutex_lock(&vvp9_mutex);

	vvp9_stop(pbi);


	hevc_source_changed(VFORMAT_VP9, 0, 0, 0);


#ifdef DEBUG_PTS
	pr_info("pts missed %ld, pts hit %ld, duration %d\n",
		   pbi->pts_missed, pbi->pts_hit, pbi->frame_dur);
#endif

	mutex_unlock(&vvp9_mutex);

	return 0;
}

/****************************************/

static struct platform_driver amvdec_vp9_driver = {
	.probe = amvdec_vp9_probe,
	.remove = amvdec_vp9_remove,
#ifdef CONFIG_PM
	.suspend = amhevc_suspend,
	.resume = amhevc_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

static struct codec_profile_t amvdec_vp9_profile = {
	.name = "vp9",
	.profile = ""
};

#ifdef MULTI_INSTANCE_SUPPORT
static unsigned int start_decode_buf_level = 0x8000;
#ifdef VP9_10B_MMU
static u32 work_buf_size = 24 * 1024 * 1024;
#else
static u32 work_buf_size = 32 * 1024 * 1024;
#endif

static unsigned char decoder_id_used[MAX_DECODE_INSTANCE_NUM];
static unsigned int get_free_decoder_id(struct vdec_s *vdec)
{
	/*stream base decoder always has id of 0*/
	int i;
	if (vdec_frame_based(vdec)) {
		for (i = 1; i < decoder_id_used[i]; i++) {
			if (!decoder_id_used[i]) {
				decoder_id_used[i] = 1;
				return i;
			}
		}
	}
	return 0;
}

static unsigned char get_data_check_sum
	(struct VP9Decoder_s *pbi, int size)
{
	int jj;
	int sum = 0;
	u8 *data = ((u8 *)pbi->chunk->block->start_virt) +
		pbi->chunk->offset;
	for (jj = 0; jj < size; jj++)
		sum += data[jj];
	return sum;
}

static void dump_data(struct VP9Decoder_s *pbi, int size)
{
	int jj;
	u8 *data = ((u8 *)pbi->chunk->block->start_virt) +
		pbi->chunk->offset;
	for (jj = 0; jj < size; jj++) {
		if ((jj & 0xf) == 0)
			vp9_print(pbi,
				0,
				"%06x:", jj);
		vp9_print_cont(pbi,
			0,
			"%02x ", data[jj]);
		if (((jj + 1) & 0xf) == 0)
			vp9_print(pbi,
			 0,
				"\n");
	}
	vp9_print(pbi,
	 0,
		"\n");
}

static void vp9_work(struct work_struct *work)
{
	struct VP9Decoder_s *pbi = container_of(work,
		struct VP9Decoder_s, work);
	struct VP9_Common_s *const cm = &pbi->common;
	struct vdec_s *vdec = hw_to_vdec(pbi);
	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */

	if ((pbi->dec_result == DEC_RESULT_GET_DATA) ||
		(pbi->dec_result == DEC_RESULT_GET_DATA_RETRY)) {
		if (pbi->dec_result == DEC_RESULT_GET_DATA) {
			vp9_print(pbi, PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_GET_DATA %x %x %x\n",
				__func__,
				READ_VREG(HEVC_STREAM_LEVEL),
				READ_VREG(HEVC_STREAM_WR_PTR),
				READ_VREG(HEVC_STREAM_RD_PTR));
			vdec_vframe_dirty(vdec, pbi->chunk);
			vdec_clean_input(vdec);
		}

		if (!is_buffer_empty(cm)) {
			int r;
			r = vdec_prepare_input(vdec, &pbi->chunk);
			if (r < 0) {
				pbi->dec_result = DEC_RESULT_GET_DATA_RETRY;

				vp9_print(pbi,
					PRINT_FLAG_VDEC_DETAIL,
					"amvdec_vh265: Insufficient data\n");

				schedule_work(&pbi->work);
				return;
			}
			pbi->dec_result = DEC_RESULT_NONE;
			vp9_print(pbi, PRINT_FLAG_VDEC_STATUS,
				"%s: chunk size 0x%x sum 0x%x\n",
				__func__, r,
				(debug & PRINT_FLAG_VDEC_STATUS) ?
				get_data_check_sum(pbi, r) : 0
				);

			if (debug & PRINT_FLAG_VDEC_DATA)
				dump_data(pbi, pbi->chunk->size);
			WRITE_VREG(HEVC_DECODE_SIZE, r);

			vdec_enable_input(vdec);

			WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);
		} else{
			pbi->dec_result = DEC_RESULT_GET_DATA_RETRY;

			vp9_print(pbi, PRINT_FLAG_VDEC_DETAIL,
				"amvdec_vh265: Insufficient data\n");

			schedule_work(&pbi->work);
		}
		return;
	} else if (pbi->dec_result == DEC_RESULT_DONE) {
		/* if (!pbi->ctx_valid)
			pbi->ctx_valid = 1; */
		vp9_print(pbi, PRINT_FLAG_VDEC_STATUS,
			"%s dec_result %d %x %x %x\n",
			__func__,
			pbi->dec_result,
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR));
		vdec_vframe_dirty(hw_to_vdec(pbi), pbi->chunk);
	} else {
		vp9_print(pbi, PRINT_FLAG_VDEC_DETAIL,
			"%s dec_result %d %x %x %x\n",
			__func__,
			pbi->dec_result,
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR));
	}

	/* mark itself has all HW resource released and input released */
	vdec_set_status(hw_to_vdec(pbi), VDEC_STATUS_CONNECTED);

	if (pbi->vdec_cb)
		pbi->vdec_cb(hw_to_vdec(pbi), pbi->vdec_cb_arg);
}

static int vp9_hw_ctx_restore(struct VP9Decoder_s *pbi)
{
	/* new to do ... */
	vvp9_prot_init(pbi);
	return 0;
}

static bool run_ready(struct vdec_s *vdec)
{
	struct VP9Decoder_s *pbi =
		(struct VP9Decoder_s *)vdec->private;
	struct VP9_Common_s *const cm = &pbi->common;

	vp9_print(pbi,
		PRINT_FLAG_VDEC_DETAIL, "%s\r\n", __func__);

	if ((!vdec_frame_based(vdec)) && (start_decode_buf_level > 0)) {
		u32 rp, wp;
		u32 level;

		rp = READ_MPEG_REG(PARSER_VIDEO_RP);
		wp = READ_MPEG_REG(PARSER_VIDEO_WP);

		if (wp < rp)
			level = vdec->input.size + wp - rp;
		else
			level = wp - rp;

		if (level < start_decode_buf_level) {
			vp9_print(pbi, 0,
				"level %d not run_ready\n", level);
			return false;
		}
	} else if (vdec_frame_based(vdec)) {
		if (!vdec_input_next_input_chunk(&vdec->input))
			return false;
	}

	return !is_buffer_empty(cm);
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
	struct VP9Decoder_s *pbi =
		(struct VP9Decoder_s *)vdec->private;
	int r;

	pbi->vdec_cb_arg = arg;
	pbi->vdec_cb = callback;
	/* pbi->chunk = vdec_prepare_input(vdec); */
	reset_dec_hw(vdec);

	r = vdec_prepare_input(vdec, &pbi->chunk);
	if (r < 0) {
		pbi->dec_result = DEC_RESULT_AGAIN;

		vp9_print(pbi, PRINT_FLAG_VDEC_DETAIL,
			"ammvdec_vh265: Insufficient data\n");

		schedule_work(&pbi->work);
		return;
	}
	pbi->dec_result = DEC_RESULT_NONE;

	vp9_print(pbi, PRINT_FLAG_VDEC_STATUS,
		"%s: size 0x%x sum 0x%x (%x %x %x)\n",
		__func__, r,
		(vdec_frame_based(vdec) &&
		(debug & PRINT_FLAG_VDEC_STATUS)) ?
		get_data_check_sum(pbi, r) : 0,
	READ_VREG(HEVC_STREAM_LEVEL),
	READ_VREG(HEVC_STREAM_WR_PTR),
	READ_VREG(HEVC_STREAM_RD_PTR));

	if (amhevc_loadmc_ex(VFORMAT_VP9, "vvp9_mc", NULL) < 0) {
		amhevc_disable();
		vp9_print(pbi, 0,
			"%s: Error amvdec_loadmc fail\n", __func__);
		return;
	}

	if (vp9_hw_ctx_restore(pbi) < 0) {
		schedule_work(&pbi->work);
		return;
	}

	vdec_enable_input(vdec);

	WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);

	if (vdec_frame_based(vdec)) {
		if (debug & PRINT_FLAG_VDEC_DATA)
			dump_data(pbi, pbi->chunk->size);

		WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, 0);
	}
	WRITE_VREG(HEVC_DECODE_SIZE, r);
	WRITE_VREG(HEVC_DECODE_COUNT, pbi->slice_idx);
	pbi->init_flag = 1;

	vp9_print(pbi, PRINT_FLAG_VDEC_STATUS,
		"%s: start hevc (%x %x %x)\n",
		__func__,
		READ_VREG(HEVC_DEC_STATUS_REG),
		READ_VREG(HEVC_MPC_E),
		READ_VREG(HEVC_MPSR));

	amhevc_start();

}

static void reset(struct vdec_s *vdec)
{

	struct VP9Decoder_s *pbi =
		(struct VP9Decoder_s *)vdec->private;

	vp9_print(pbi,
		PRINT_FLAG_VDEC_DETAIL, "%s\r\n", __func__);

}

static irqreturn_t vp9_irq_cb(struct vdec_s *vdec)
{
	struct VP9Decoder_s *pbi =
		(struct VP9Decoder_s *)vdec->private;
	return vvp9_isr(0, pbi);
}

static irqreturn_t vp9_threaded_irq_cb(struct vdec_s *vdec)
{
	struct VP9Decoder_s *pbi =
		(struct VP9Decoder_s *)vdec->private;
	return vvp9_isr_thread_fn(0, pbi);
}


static int ammvdec_vp9_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;

	struct BUF_s BUF[MAX_BUF_NUM];
	struct VP9Decoder_s *pbi = NULL;
	pr_info("%s\n", __func__);
	if (pdata == NULL) {
		pr_info("\nammvdec_vp9 memory resource undefined.\n");
		return -EFAULT;
	}
	pbi = (struct VP9Decoder_s *)devm_kzalloc(&pdev->dev,
		sizeof(struct VP9Decoder_s), GFP_KERNEL);
	if (pbi == NULL) {
		pr_info("\nammvdec_vp9 device data allocation failed\n");
		return -ENOMEM;
	}
	pdata->private = pbi;
	pdata->dec_status = vvp9_dec_status;
	/* pdata->set_trickmode = set_trickmode; */
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vp9_irq_cb;
	pdata->threaded_irq_handler = vp9_threaded_irq_cb;

	pdata->id = pdev->id;


	memcpy(&BUF[0], &pbi->m_BUF[0], sizeof(struct BUF_s) * MAX_BUF_NUM);
	memset(pbi, 0, sizeof(VP9Decoder));
	memcpy(&pbi->m_BUF[0], &BUF[0], sizeof(struct BUF_s) * MAX_BUF_NUM);

	pbi->index = get_free_decoder_id(pdata);

	if (pdata->use_vfm_path)
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			MULTI_INSTANCE_PROVIDER_NAME ".%02x", pdev->id & 0xff);

	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vvp9_vf_provider, pbi);

	pbi->provider_name = pdata->vf_provider_name;
	platform_set_drvdata(pdev, pdata);

	pbi->platform_dev = pdev;
#if 0
	pbi->buf_start = pdata->mem_start;
	pbi->buf_size = pdata->mem_end - pdata->mem_start + 1;
#else
	if (amvdec_vp9_mmu_init(pbi) < 0) {
		pr_err("vp9 alloc bmmu box failed!!\n");
		devm_kfree(&pdev->dev, (void *)pbi);
		return -1;
	}

	pbi->cma_alloc_count = PAGE_ALIGN(work_buf_size) / PAGE_SIZE;
	if (!decoder_bmmu_box_alloc_idx_wait(
			pbi->bmmu_box,
			WORK_SPACE_BUF_ID,
			pbi->cma_alloc_count * PAGE_SIZE,
			-1,
			-1,
			BMMU_ALLOC_FLAGS_WAITCLEAR
			))	{
		pbi->cma_alloc_addr = decoder_bmmu_box_get_phy_addr(
					pbi->bmmu_box,
					WORK_SPACE_BUF_ID);
	} else {
		vp9_print(pbi, 0,
			"codec_mm alloc failed, request buf size 0x%lx\n",
				pbi->cma_alloc_count * PAGE_SIZE);
		pbi->cma_alloc_count = 0;
		uninit_mmu_buffers(pbi);
		devm_kfree(&pdev->dev, (void *)pbi);
		return -ENOMEM;
	}
	pbi->buf_start = pbi->cma_alloc_addr;
	pbi->buf_size = work_buf_size;
#endif
	pbi->m_ins_flag = 1;

	pbi->init_flag = 0;
	pbi->fatal_error = 0;
	pbi->show_frame_num = 0;
	if (pdata == NULL) {
		pr_info("\namvdec_vp9 memory resource undefined.\n");
		uninit_mmu_buffers(pbi);
		devm_kfree(&pdev->dev, (void *)pbi);
		return -EFAULT;
	}

	if (debug) {
		pr_info("===VP9 decoder mem resource 0x%lx -- 0x%lx\n",
			   pbi->buf_start,
			   pbi->buf_start + pbi->buf_size);
	}

	if (pdata->sys_info)
		pbi->vvp9_amstream_dec_info = *pdata->sys_info;
	else {
		pbi->vvp9_amstream_dec_info.width = 0;
		pbi->vvp9_amstream_dec_info.height = 0;
		pbi->vvp9_amstream_dec_info.rate = 30;
	}

	pbi->cma_dev = pdata->cma_dev;

	if (vvp9_init(pbi) < 0) {
		pr_info("\namvdec_vp9 init failed.\n");
		vp9_local_uninit(pbi);
		uninit_mmu_buffers(pbi);
		devm_kfree(&pdev->dev, (void *)pbi);
		return -ENODEV;
	}
	return 0;
}

static int ammvdec_vp9_remove(struct platform_device *pdev)
{
	struct VP9Decoder_s *pbi = (struct VP9Decoder_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	if (debug)
		pr_info("amvdec_vp9_remove\n");

	vvp9_stop(pbi);

	vdec_set_status(hw_to_vdec(pbi), VDEC_STATUS_DISCONNECTED);


#ifdef DEBUG_PTS
	pr_info("pts missed %ld, pts hit %ld, duration %d\n",
		   pbi->pts_missed, pbi->pts_hit, pbi->frame_dur);
#endif
	devm_kfree(&pdev->dev, (void *)pbi);
	return 0;
}

static struct platform_driver ammvdec_vp9_driver = {
	.probe = ammvdec_vp9_probe,
	.remove = ammvdec_vp9_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = MULTI_DRIVER_NAME,
	}
};
#endif

static int __init amvdec_vp9_driver_init_module(void)
{
	pr_debug("amvdec_vp9 module init\n");
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
	if (platform_driver_register(&ammvdec_vp9_driver))
		pr_err("failed to register ammvdec_vp9 driver\n");

#endif
	if (platform_driver_register(&amvdec_vp9_driver)) {
		pr_err("failed to register amvdec_vp9 driver\n");
		return -ENODEV;
	}

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL) {
		amvdec_vp9_profile.profile =
			"4k, 10bit, dwrite, compressed";
	} else {
		amvdec_vp9_profile.name = "vp9_unsupport";
	}

	vcodec_profile_register(&amvdec_vp9_profile);

	return 0;
}

static void __exit amvdec_vp9_driver_remove_module(void)
{
	pr_debug("amvdec_vp9 module remove.\n");
#ifdef MULTI_INSTANCE_SUPPORT
	platform_driver_unregister(&ammvdec_vp9_driver);
#endif
	platform_driver_unregister(&amvdec_vp9_driver);
}

/****************************************/
/*
module_param(stat, uint, 0664);
MODULE_PARM_DESC(stat, "\n amvdec_vp9 stat\n");
*/
module_param(use_cma, uint, 0664);
MODULE_PARM_DESC(use_cma, "\n amvdec_vp9 use_cma\n");

module_param(bit_depth_luma, uint, 0664);
MODULE_PARM_DESC(bit_depth_luma, "\n amvdec_vp9 bit_depth_luma\n");

module_param(bit_depth_chroma, uint, 0664);
MODULE_PARM_DESC(bit_depth_chroma, "\n amvdec_vp9 bit_depth_chroma\n");

module_param(frame_width, uint, 0664);
MODULE_PARM_DESC(frame_width, "\n amvdec_vp9 frame_width\n");

module_param(frame_height, uint, 0664);
MODULE_PARM_DESC(frame_height, "\n amvdec_vp9 frame_height\n");

module_param(debug, uint, 0664);
MODULE_PARM_DESC(debug, "\n amvdec_vp9 debug\n");

module_param(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\nradr\n");

module_param(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\nrval\n");

module_param(pop_shorts, uint, 0664);
MODULE_PARM_DESC(pop_shorts, "\nrval\n");

module_param(dbg_cmd, uint, 0664);
MODULE_PARM_DESC(dbg_cmd, "\dbg_cmd\n");

module_param(dbg_skip_decode_index, uint, 0664);
MODULE_PARM_DESC(dbg_skip_decode_index, "\dbg_skip_decode_index\n");

module_param(endian, uint, 0664);
MODULE_PARM_DESC(endian, "\nrval\n");

module_param(step, uint, 0664);
MODULE_PARM_DESC(step, "\n amvdec_vp9 step\n");

module_param(decode_stop_pos, uint, 0664);
MODULE_PARM_DESC(decode_stop_pos, "\n amvdec_vp9 decode_stop_pos\n");

module_param(decode_pic_begin, uint, 0664);
MODULE_PARM_DESC(decode_pic_begin, "\n amvdec_vp9 decode_pic_begin\n");

module_param(slice_parse_begin, uint, 0664);
MODULE_PARM_DESC(slice_parse_begin, "\n amvdec_vp9 slice_parse_begin\n");

module_param(i_only_flag, uint, 0664);
MODULE_PARM_DESC(i_only_flag, "\n amvdec_vp9 i_only_flag\n");

module_param(error_handle_policy, uint, 0664);
MODULE_PARM_DESC(error_handle_policy, "\n amvdec_vp9 error_handle_policy\n");

module_param(buf_alloc_width, uint, 0664);
MODULE_PARM_DESC(buf_alloc_width, "\n buf_alloc_width\n");

module_param(buf_alloc_height, uint, 0664);
MODULE_PARM_DESC(buf_alloc_height, "\n buf_alloc_height\n");

module_param(buf_alloc_depth, uint, 0664);
MODULE_PARM_DESC(buf_alloc_depth, "\n buf_alloc_depth\n");

module_param(buf_alloc_size, uint, 0664);
MODULE_PARM_DESC(buf_alloc_size, "\n buf_alloc_size\n");

module_param(buffer_mode, uint, 0664);
MODULE_PARM_DESC(buffer_mode, "\n buffer_mode\n");

module_param(buffer_mode_dbg, uint, 0664);
MODULE_PARM_DESC(buffer_mode_dbg, "\n buffer_mode_dbg\n");
/*USE_BUF_BLOCK*/
module_param(max_buf_num, uint, 0664);
MODULE_PARM_DESC(max_buf_num, "\n max_buf_num\n");

module_param(dynamic_buf_num_margin, uint, 0664);
MODULE_PARM_DESC(dynamic_buf_num_margin, "\n dynamic_buf_num_margin\n");
/**/

module_param(mem_map_mode, uint, 0664);
MODULE_PARM_DESC(mem_map_mode, "\n mem_map_mode\n");

#ifdef SUPPORT_10BIT
module_param(double_write_mode, uint, 0664);
MODULE_PARM_DESC(double_write_mode, "\n double_write_mode\n");

module_param(enable_mem_saving, uint, 0664);
MODULE_PARM_DESC(enable_mem_saving, "\n enable_mem_saving\n");

module_param(force_w_h, uint, 0664);
MODULE_PARM_DESC(force_w_h, "\n force_w_h\n");
#endif

module_param(force_fps, uint, 0664);
MODULE_PARM_DESC(force_fps, "\n force_fps\n");

module_param(max_decoding_time, uint, 0664);
MODULE_PARM_DESC(max_decoding_time, "\n max_decoding_time\n");

module_param(on_no_keyframe_skiped, uint, 0664);
MODULE_PARM_DESC(on_no_keyframe_skiped, "\n on_no_keyframe_skiped\n");

#ifdef MULTI_INSTANCE_SUPPORT
module_param(start_decode_buf_level, uint, 0664);
MODULE_PARM_DESC(start_decode_buf_level,
		"\n ammvdec_h264 start_decode_buf_level\n");
#endif

module_init(amvdec_vp9_driver_init_module);
module_exit(amvdec_vp9_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC vp9 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <tim.yao@amlogic.com>");
