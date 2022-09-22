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
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <uapi/linux/sched/types.h>
#include <linux/signal.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/slab.h>
#include <linux/amlogic/tee.h>
#include <linux/crc32.h>
#include <media/v4l2-mem2mem.h>

#include "../../../stream_input/amports/amports_priv.h"
#include "../../decoder/utils/decoder_mmu_box.h"
#include "../../decoder/utils/decoder_bmmu_box.h"
#include "../../decoder/utils/vdec.h"
#include "../../decoder/utils/amvdec.h"
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
#include "../../decoder/utils/vdec_profile.h"
#endif
#include "../../decoder/utils/config_parser.h"
#include "../../decoder/utils/firmware.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "../../../amvdec_ports/vdec_drv_base.h"
#include "../../decoder/utils/vdec_v4l2_buffer_ops.h"
#include "../../../amvdec_ports/utils/common.h"
#include "../../decoder/utils/vdec_feature.h"
#include "../../decoder/utils/vdec_ge2d_utils.h"
#include "aom_av1_define.h"
#include "av1_global.h"
#include "vav1.h"
#include "../../decoder/utils/decoder_dma_alloc.h"

#define DEBUG_CMD
#define DEBUG_CRC_ERROR

#define MEM_NAME "codec_av1"

#define DUMP_FILMGRAIN
#define MIX_STREAM_SUPPORT

#define AOM_AV1_DBLK_INIT
#define AOM_AV1_UPSCALE_INIT

#define USE_DEC_PIC_END

#define SANITY_CHECK
#define CO_MV_COMPRESS

#define FGS_TABLE_SIZE  (512 * 128 / 8)

#define AV1_GMC_PARAM_BUFF_ADDR 	               0x316d
#define HEVCD_MPP_DECOMP_AXIURG_CTL                0x34c7
#define HEVC_FGS_IDX                               0x3660
#define HEVC_FGS_DATA                              0x3661
#define HEVC_FGS_CTRL                              0x3662
#define AV1_SKIP_MODE_INFO                         0x316c
#define AV1_QUANT_WR                               0x3146
#define   AV1_SEG_W_ADDR                           0x3165
#define   AV1_SEG_R_ADDR                           0x3166
#define AV1_REF_SEG_INFO                           0x3171
#define HEVC_ASSIST_PIC_SIZE_FB_READ               0x300d
#define PARSER_REF_SCALE_ENBL                      0x316b
#define HEVC_MPRED_MV_RPTR_1                       0x3263
#define HEVC_MPRED_MV_RPTR_2                       0x3264
#define HEVC_SAO_CTRL9                             0x362d
#define HEVC_FGS_TABLE_START                       0x3666
#define HEVC_FGS_TABLE_LENGTH                      0x3667
#define HEVC_DBLK_CDEF0                            0x3515
#define HEVC_DBLK_CDEF1                            0x3516
#define HEVC_DBLK_UPS1                             0x351c
#define HEVC_DBLK_UPS2                             0x351d
#define HEVC_DBLK_UPS3                             0x351e
#define HEVC_DBLK_UPS4                             0x351f
#define HEVC_DBLK_UPS5                             0x3520
#define AV1_UPSCALE_X0_QN                          0x316e
#define AV1_UPSCALE_STEP_QN                        0x316f
#define HEVC_DBLK_DBLK0                            0x3523
#define HEVC_DBLK_DBLK1                            0x3524
#define HEVC_DBLK_DBLK2                            0x3525

#define HW_MASK_FRONT    0x1
#define HW_MASK_BACK     0x2

#define AV1D_MPP_REFINFO_TBL_ACCCONFIG             0x3442
#define AV1D_MPP_REFINFO_DATA                      0x3443
#define AV1D_MPP_REF_SCALE_ENBL                    0x3441
#define HEVC_MPRED_CTRL4                           0x324c
#define HEVC_CM_HEADER_START_ADDR                  0x3628
#define HEVC_DBLK_CFGB                             0x350b
#define HEVCD_MPP_ANC2AXI_TBL_DATA                 0x3464
#define HEVC_SAO_MMU_VH1_ADDR                      0x363b
#define HEVC_SAO_MMU_VH0_ADDR                      0x363a

#define HEVC_MV_INFO                               0x310d
#define HEVC_QP_INFO                               0x3137
#define HEVC_SKIP_INFO                             0x3136

#define HEVC_CM_BODY_LENGTH2                       0x3663
#define HEVC_CM_HEADER_OFFSET2                     0x3664
#define HEVC_CM_HEADER_LENGTH2                     0x3665

#define HEVC_CM_HEADER_START_ADDR2                 0x364a
#define HEVC_SAO_MMU_DMA_CTRL2                     0x364c
#define HEVC_SAO_MMU_VH0_ADDR2                     0x364d
#define HEVC_SAO_MMU_VH1_ADDR2                     0x364e
#define HEVC_SAO_MMU_STATUS2                       0x3650
#define HEVC_DW_VH0_ADDDR                          0x365e
#define HEVC_DW_VH1_ADDDR                          0x365f

#ifdef BUFMGR_ONLY_OLD_CHIP
#undef AV1_SKIP_MODE_INFO
#define AV1_SKIP_MODE_INFO  HEVC_ASSIST_SCRATCH_B
#endif


#define AOM_AV1_DEC_IDLE                     0
#define AOM_AV1_DEC_FRAME_HEADER             1
#define AOM_AV1_DEC_TILE_END                 2
#define AOM_AV1_DEC_TG_END                   3
#define AOM_AV1_DEC_LCU_END                  4
#define AOM_AV1_DECODE_SLICE                 5
#define AOM_AV1_SEARCH_HEAD                  6
#define AOM_AV1_DUMP_LMEM                    7
#define AOM_AV1_FGS_PARAM_CONT               8
#define AOM_AV1_DISCARD_NAL                  0x10
#define AOM_AV1_RESULT_NEED_MORE_BUFFER      0x11
#define DEC_RESULT_UNFINISH	        		 0x12

/*status*/
#define AOM_AV1_DEC_PIC_END                  0xe0
/*AOM_AV1_FGS_PARA:
Bit[11] - 0 Read, 1 - Write
Bit[10:8] - film_grain_params_ref_idx, For Write request
*/
#define AOM_AV1_FGS_PARAM                    0xe1
#define AOM_AV1_DEC_PIC_END_PRE              0xe2
#define AOM_AV1_HEAD_PARSER_DONE          0xf0
#define AOM_AV1_HEAD_SEARCH_DONE          0xf1
#define AOM_AV1_SEQ_HEAD_PARSER_DONE      0xf2
#define AOM_AV1_FRAME_HEAD_PARSER_DONE    0xf3
#define AOM_AV1_FRAME_PARSER_DONE   0xf4
#define AOM_AV1_REDUNDANT_FRAME_HEAD_PARSER_DONE   0xf5
#define HEVC_ACTION_DONE            0xff

#define AOM_DECODE_BUFEMPTY        0x20
#define AOM_DECODE_TIMEOUT         0x21
#define AOM_SEARCH_BUFEMPTY        0x22
#define AOM_DECODE_OVER_SIZE       0x23
#define AOM_EOS                     0x24
#define AOM_NAL_DECODE_DONE            0x25

#define VF_POOL_SIZE        32

#undef pr_info
#define pr_info pr_cont

#define DECODE_MODE_SINGLE		((0x80 << 24) | 0)
#define DECODE_MODE_MULTI_STREAMBASE	((0x80 << 24) | 1)
#define DECODE_MODE_MULTI_FRAMEBASE	((0x80 << 24) | 2)
#define DECODE_MODE_SINGLE_LOW_LATENCY ((0x80 << 24) | 3)
#define DECODE_MODE_MULTI_FRAMEBASE_NOHEAD ((0x80 << 24) | 4)

#define  AV1_TRIGGER_FRAME_DONE		0x100
#define  AV1_TRIGGER_FRAME_ENABLE	0x200

#define MV_MEM_UNIT 0x240
/*---------------------------------------------------
 * Include "parser_cmd.h"
 *---------------------------------------------------
 */
#define PARSER_CMD_SKIP_CFG_0 0x0000090b

#define PARSER_CMD_SKIP_CFG_1 0x1b14140f

#define PARSER_CMD_SKIP_CFG_2 0x001b1910

#define PARSER_CMD_NUMBER 37

#define MULTI_INSTANCE_SUPPORT
#define SUPPORT_10BIT

#ifndef STAT_KTHREAD
#define STAT_KTHREAD 0x40
#endif

#ifdef MULTI_INSTANCE_SUPPORT
#define MAX_DECODE_INSTANCE_NUM     9

#ifdef DEBUG_USE_VP9_DEVICE_NAME
#define MULTI_DRIVER_NAME "ammvdec_vp9_v4l"
#else
#define MULTI_DRIVER_NAME "ammvdec_av1_v4l"
#endif

#define AUX_BUF_ALIGN(adr) ((adr + 0xf) & (~0xf))
#ifdef DEBUG_UCODE_LOG
static u32 prefix_aux_buf_size;
static u32 suffix_aux_buf_size;
#else
static u32 prefix_aux_buf_size = (16 * 1024);
static u32 suffix_aux_buf_size;
#endif
#if (defined DEBUG_UCODE_LOG) || (defined DEBUG_CMD)
#define UCODE_LOG_BUF_SIZE   (1024 * 1024)
#endif
static unsigned int max_decode_instance_num = MAX_DECODE_INSTANCE_NUM;
static unsigned int decode_frame_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int display_frame_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int max_process_time[MAX_DECODE_INSTANCE_NUM];
static unsigned int run_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int input_empty[MAX_DECODE_INSTANCE_NUM];
static unsigned int not_run_ready[MAX_DECODE_INSTANCE_NUM];
#ifdef AOM_AV1_MMU_DW
static unsigned int dw_mmu_enable[MAX_DECODE_INSTANCE_NUM];
#endif

static u32 decode_timeout_val = 600;
static u32 enable_single_slice = 1;

static int start_decode_buf_level = 0x8000;
static u32 force_pts_unstable;
static u32 mv_buf_margin = REF_FRAMES;
static u32 mv_buf_dynamic_alloc;
static u32 force_max_one_mv_buffer_size;

/* DOUBLE_WRITE_MODE is enabled only when NV21 8 bit output is needed */
/* double_write_mode:
 *	0, no double write;
 *	1, 1:1 ratio;
 *	2, (1/4):(1/4) ratio;
 *	3, (1/4):(1/4) ratio, with both compressed frame included
 *	4, (1/2):(1/2) ratio;
 *	5, (1/2):(1/2) ratio, with both compressed frame included
 *	8, (1/8):(1/8) ratio;
 *	0x10, double write only
 *	0x20, mmu double write
 *	0x100, if > 1080p,use mode 4,else use mode 1;
 *	0x200, if > 1080p,use mode 2,else use mode 1;
 *	0x300, if > 720p, use mode 4, else use mode 1;
 */
static u32 double_write_mode;

#ifdef DEBUG_USE_VP9_DEVICE_NAME
#define DRIVER_NAME "amvdec_vp9_v4l"
#define MODULE_NAME "amvdec_vp9_v4l"
#define DRIVER_HEADER_NAME "amvdec_vp9_header"
#else
#define DRIVER_NAME "amvdec_av1_v4l"
#define DRIVER_HEADER_NAME "amvdec_av1_header"
#endif

#define PUT_INTERVAL        (HZ/100)
#define ERROR_SYSTEM_RESET_COUNT   200

#define PTS_NORMAL                0
#define PTS_NONE_REF_USE_DURATION 1

#define PTS_MODE_SWITCHING_THRESHOLD           3
#define PTS_MODE_SWITCHING_RECOVERY_THREASHOLD 3

#define DUR2PTS(x) ((x)*90/96)
#define PTS2DUR(x) ((x)*96/90)
#define PTS2DUR_u64(x) (div_u64((x)*96, 90))

struct AV1HW_s;
static int vav1_vf_states(struct vframe_states *states, void *);
static struct vframe_s *vav1_vf_peek(void *);
static struct vframe_s *vav1_vf_get(void *);
static void vav1_vf_put(struct vframe_s *, void *);
static int vav1_event_cb(int type, void *data, void *private_data);
#ifdef MULTI_INSTANCE_SUPPORT
static s32 vav1_init(struct vdec_s *vdec);
#else
static s32 vav1_init(struct AV1HW_s *hw);
#endif
static void vav1_prot_init(struct AV1HW_s *hw, u32 mask);
static int vav1_local_init(struct AV1HW_s *hw, bool reset_flag);
static void vav1_put_timer_func(struct timer_list *timer);
static void dump_data(struct AV1HW_s *hw, int size);
static unsigned int get_data_check_sum(struct AV1HW_s *hw, int size);
static void dump_pic_list(struct AV1HW_s *hw);
static int vav1_mmu_map_alloc(struct AV1HW_s *hw);
static void vav1_mmu_map_free(struct AV1HW_s *hw);
static int av1_alloc_mmu(
		struct AV1HW_s *hw,
		int cur_buf_idx,
		int pic_width,
		int pic_height,
		unsigned short bit_depth,
		unsigned int *mmu_index_adr);

#ifdef DEBUG_USE_VP9_DEVICE_NAME
static const char vav1_dec_id[] = "vvp9-dev";

#define PROVIDER_NAME   "decoder.vp9"
#define MULTI_INSTANCE_PROVIDER_NAME    "vdec.vp9"
#else
static const char vav1_dec_id[] = "vav1-dev";

#define PROVIDER_NAME   "decoder.av1"
#define MULTI_INSTANCE_PROVIDER_NAME    "vdec.av1"
#endif
#define DV_PROVIDER_NAME  "dvbldec"

static const struct vframe_operations_s vav1_vf_provider = {
	.peek = vav1_vf_peek,
	.get = vav1_vf_get,
	.put = vav1_vf_put,
	.event_cb = vav1_event_cb,
	.vf_states = vav1_vf_states,
};

#ifndef MULTI_INSTANCE_SUPPORT
static struct vframe_provider_s vav1_vf_prov;
#endif

static u32 bit_depth_luma;
static u32 bit_depth_chroma;
static u32 frame_width;
static u32 frame_height;
static u32 video_signal_type;
static u32 on_no_keyframe_skiped;
static u32 without_display_mode;
static u32 v4l_bitstream_id_enable = 1;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
static u32 force_dv_enable;
#endif

#define PROB_SIZE    (496 * 2 * 4)
#define PROB_BUF_SIZE    (0x5000)
#define COUNT_BUF_SIZE   (0x300 * 4 * 4)
/*compute_losless_comp_body_size(4096, 2304, 1) = 18874368(0x1200000)*/
#define MAX_FRAME_4K_NUM 0x1200
#define MAX_FRAME_8K_NUM 0x4800

#define HEVC_ASSIST_MMU_MAP_ADDR                   0x3009


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
	ulong v4l_ref_buf_addr;
	ulong	header_addr;
	u32 	header_size;
	u32	luma_size;
	ulong	chroma_addr;
	u32	chroma_size;
	ulong	header_dw_addr;
} /*BUF_t */;

struct MVBUF_s {
	unsigned long start_adr;
	unsigned int size;
	int used_flag;
} /*MVBUF_t */;

#define WR_PTR_INC_NUM 1

#define DOS_PROJECT
#undef MEMORY_MAP_IN_REAL_CHIP

#ifndef BUFMGR_ONLY_OLD_CHIP
#define   MCRCC_ENABLE
#endif

#ifdef AV1_10B_NV21
#else
#define LOSLESS_COMPRESS_MODE
#endif

static u32 get_picture_qos;

static u32 debug;
static u32 disable_fg;
/*for debug*/
static u32 use_dw_mmu;
static u32 mmu_mem_save = 1;

static bool is_reset;
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

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
static u32 dv_toggle_prov_name;
#endif

static u32 run_ready_min_buf_num = 2;
#ifdef DEBUG_CRC_ERROR
/*
  bit[4] fill zero in header before starting
  bit[5] dump mmu header always

  bit[6] dump mv buffer always

  bit[8] delay after decoding
  bit[31~16] delayed mseconds
*/
static u32 crc_debug_flag;
#endif
#ifdef DEBUG_CMD
static u32 header_dump_size = 0x10000;
static u32 debug_cmd_wait_count;
static u32 debug_cmd_wait_type;
#endif
#define DEBUG_REG
#ifdef DEBUG_REG
void AV1_WRITE_VREG_DBG2(unsigned int adr, unsigned int val, int line)
{
	if (debug & AV1_DEBUG_REG)
		pr_info("%d:%s(%x, %x)\n", line, __func__, adr, val);
	if (adr != 0)
		WRITE_VREG(adr, val);
}

#undef WRITE_VREG
#define WRITE_VREG(a,v) AV1_WRITE_VREG_DBG2(a,v,__LINE__)
#endif

#define FRAME_CNT_WINDOW_SIZE 59
#define RATE_CORRECTION_THRESHOLD 5
/**************************************************

AV1 buffer management start

***************************************************/
#define MMU_COMPRESS_HEADER_SIZE_1080P  0x10000
#define MMU_COMPRESS_HEADER_SIZE_4K  0x48000
#define MMU_COMPRESS_HEADER_SIZE_8K  0x120000

#define MAX_ONE_MV_BUFFER_SIZE_1080P 0x20400
#define MAX_ONE_MV_BUFFER_SIZE_4K  0x91400
#define MAX_ONE_MV_BUFFER_SIZE_8K 0x244800
/*to support tm2revb and sc2*/
#define MAX_ONE_MV_BUFFER_SIZE_1080P_TM2REVB 0x26400
#define MAX_ONE_MV_BUFFER_SIZE_4K_TM2REVB  0xac400
#define MAX_ONE_MV_BUFFER_SIZE_8K_TM2REVB 0x2b0800

#define MAX_SIZE_8K (8192 * 4608)
#define MAX_SIZE_4K (4096 * 2304)
#define MAX_SIZE_2K (1920 * 1088)
#define IS_8K_SIZE(w, h)	(((w) * (h)) > MAX_SIZE_4K)
#define IS_4K_SIZE(w, h)  (((w) * (h)) > (1920*1088))

#define INVALID_IDX -1  /* Invalid buffer index.*/


/*4 scratch frames for the new frames to support a maximum of 4 cores decoding
 *in parallel, 3 for scaled references on the encoder.
 *TODO(hkuang): Add ondemand frame buffers instead of hardcoding the number
 * // of framebuffers.
 *TODO(jkoleszar): These 3 extra references could probably come from the
 *normal reference pool.
 */
#define REF_FRAMES_4K REF_FRAMES

#ifdef USE_SPEC_BUF_FOR_MMU_HEAD
#define HEADER_FRAME_BUFFERS (0)
#elif (defined AOM_AV1_MMU_DW)
#define HEADER_FRAME_BUFFERS (2 * FRAME_BUFFERS)
#else
#define HEADER_FRAME_BUFFERS (FRAME_BUFFERS)
#endif
#define MAX_BUF_NUM (FRAME_BUFFERS)
#define MV_BUFFER_NUM	FRAME_BUFFERS

/*buffer + header buffer + workspace*/
#ifdef MV_USE_FIXED_BUF
#define MAX_BMMU_BUFFER_NUM (FRAME_BUFFERS + HEADER_FRAME_BUFFERS + 1)
#define VF_BUFFER_IDX(n) (n)
#define HEADER_BUFFER_IDX(n) (FRAME_BUFFERS + n)
#define WORK_SPACE_BUF_ID (FRAME_BUFFERS + HEADER_FRAME_BUFFERS)
#else
#define MAX_BMMU_BUFFER_NUM \
	(FRAME_BUFFERS + HEADER_FRAME_BUFFERS + MV_BUFFER_NUM + 1)
#define VF_BUFFER_IDX(n) (n)
#define HEADER_BUFFER_IDX(n) (FRAME_BUFFERS + n)
#define MV_BUFFER_IDX(n) (FRAME_BUFFERS + HEADER_FRAME_BUFFERS + n)
#define WORK_SPACE_BUF_ID \
	(FRAME_BUFFERS + HEADER_FRAME_BUFFERS + MV_BUFFER_NUM)
#endif
#ifdef AOM_AV1_MMU_DW
#define DW_HEADER_BUFFER_IDX(n) (HEADER_BUFFER_IDX(HEADER_FRAME_BUFFERS/2) + n)
#endif

static void set_canvas(struct AV1HW_s *hw,
	struct PIC_BUFFER_CONFIG_s *pic_config);

static void fill_frame_info(struct AV1HW_s *hw,
	struct PIC_BUFFER_CONFIG_s *frame,
	unsigned int framesize,
	unsigned int pts);

static int  compute_losless_comp_body_size(int width, int height,
	uint8_t is_bit_depth_10);

void clear_frame_buf_ref_count(AV1Decoder *pbi);

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
#define DEC_RESULT_DISCARD_DATA     11

#define DEC_S1_RESULT_NONE          0
#define DEC_S1_RESULT_DONE          1
#define DEC_S1_RESULT_FORCE_EXIT       2
#define DEC_S1_RESULT_TEST_TRIGGER_DONE	0xf0

#ifdef FB_DECODING_TEST_SCHEDULE
#define TEST_SET_NONE        0
#define TEST_SET_PIC_DONE    1
#define TEST_SET_S2_DONE     2
#endif

static void av1_work(struct work_struct *work);
#endif

#define PROC_STATE_INIT			0
#define PROC_STATE_DECODESLICE	1
#define PROC_STATE_SENDAGAIN	2

#define HEVC_PRINT_BUF		512

#ifdef DUMP_FILMGRAIN
u32 fg_dump_index = 0xff;
#endif

#ifdef AOM_AV1_DBLK_INIT
struct loop_filter_info_n_s;
struct loopfilter;
struct segmentation_lf;
#endif

struct vav1_assit_task {
	bool                 use_sfgs;
	bool                 running;
	struct mutex         assit_mutex;
	struct semaphore     sem;
	struct task_struct  *task;
	void                *private;
};

struct AV1HW_s {
	AV1Decoder *pbi;
	union param_u aom_param;
	unsigned char frame_decoded;
	unsigned char one_compressed_data_done;
	unsigned char new_compressed_data;
	/*def CHECK_OBU_REDUNDANT_FRAME_HEADER*/
	int obu_frame_frame_head_come_after_tile;
	unsigned char index;

	struct device *cma_dev;
	struct platform_device *platform_dev;
	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;
	struct vframe_chunk_s *chunk;
	int dec_result;
	struct work_struct work;
	struct work_struct set_clk_work;
	u32 start_shift_bytes;
	u32 data_size;

	struct BuffInfo_s work_space_buf_store;
	unsigned long buf_start;
	u32 buf_size;
	u32 cma_alloc_count;
	unsigned long cma_alloc_addr;
	uint8_t eos;
	unsigned long int start_process_time;
	unsigned last_lcu_idx;
	int decode_timeout_count;
	unsigned timeout_num;
	int save_buffer_mode;

	int double_write_mode;

	long used_4k_num;

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
	uint8_t config_next_ref_info_flag;
	uint8_t first_sc_checked;
	uint8_t process_busy;

	uint8_t process_state;
	u32 ucode_pause_pos;

	int show_frame_num;
	struct buff_s mc_buf_spec;
	struct dec_sysinfo vav1_amstream_dec_info;
	void *rpm_addr;
	void *lmem_addr;
	dma_addr_t rpm_phy_addr;
	dma_addr_t lmem_phy_addr;
	unsigned short *lmem_ptr;
	unsigned short *debug_ptr;
#ifdef DUMP_FILMGRAIN
	dma_addr_t fg_phy_addr;
	unsigned char *fg_ptr;
	void *fg_addr;
#endif
	u32 fgs_valid;

	u8 aux_data_dirty;
	u32 prefix_aux_size;
	u32 suffix_aux_size;
	void *aux_addr;
	dma_addr_t aux_phy_addr;
	char *dv_data_buf;
	int dv_data_size;
#if (defined DEBUG_UCODE_LOG) || (defined DEBUG_CMD)
	void *ucode_log_addr;
	dma_addr_t ucode_log_phy_addr;
#endif

	void *prob_buffer_addr;
	void *count_buffer_addr;
	dma_addr_t prob_buffer_phy_addr;
	dma_addr_t count_buffer_phy_addr;

	void *frame_mmu_map_addr;
	dma_addr_t frame_mmu_map_phy_addr;
#ifdef AOM_AV1_MMU_DW
	void *dw_frame_mmu_map_addr;
	dma_addr_t dw_frame_mmu_map_phy_addr;
#endif
	unsigned int use_cma_flag;

	struct BUF_s m_BUF[MAX_BUF_NUM];
	struct MVBUF_s m_mv_BUF[MV_BUFFER_NUM];
	u32 used_buf_num;
	u32 mv_buf_margin;
	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(pending_q, struct vframe_s *, VF_POOL_SIZE);
	struct vframe_s vfpool[VF_POOL_SIZE];
	atomic_t  vf_pre_count;
	atomic_t  vf_get_count;
	atomic_t  vf_put_count;
	int buf_num;
	int pic_num;
	int lcu_size_log2;
	unsigned int losless_comp_body_size;

	u32 video_signal_type;

	u32 pts_unstable;
	bool av1_first_pts_ready;
	bool dur_recalc_flag;
	u8  first_pts_index;
	u32 frame_mode_pts_save[FRAME_BUFFERS];
	u64 frame_mode_pts64_save[FRAME_BUFFERS];

	int last_pts;
	u64 last_pts_us64;
	u64 shift_byte_count;

	u32 shift_byte_count_lo;
	u32 shift_byte_count_hi;
	int pts_mode_switching_count;
	int pts_mode_recovery_count;
	bool get_frame_dur;

	u32 saved_resolution;

	struct AV1_Common_s common;
	struct RefCntBuffer_s *cur_buf;
	int refresh_frame_flags;
	uint8_t need_resync;
	uint8_t hold_ref_buf;
	uint8_t ready_for_new_data;
	struct BufferPool_s av1_buffer_pool;

	struct BuffInfo_s *work_space_buf;

	struct buff_s *mc_buf;

	unsigned int frame_width;
	unsigned int frame_height;

	unsigned short *rpm_ptr;
	int     init_pic_w;
	int     init_pic_h;
	int     lcu_total;

	int     current_lcu_size;

	int     slice_type;

	int skip_flag;
	int decode_idx;
	int result_done_count;
	uint8_t has_keyframe;
	uint8_t has_sequence;
	uint8_t wait_buf;
	uint8_t error_flag;

	/* bit 0, for decoding; bit 1, for displaying */
	uint8_t ignore_bufmgr_error;
	int PB_skip_mode;
	int PB_skip_count_after_decoding;

	struct vdec_info *gvs;

	u32 pre_stream_offset;

	unsigned int dec_status;
	u32 last_put_idx;
	int new_frame_displayed;
	void *mmu_box;
	void *bmmu_box;
	int mmu_enable;
#ifdef AOM_AV1_MMU_DW
	void *mmu_box_dw;
	int dw_mmu_enable;
#endif
	struct vframe_master_display_colour_s vf_dp;
	struct firmware_s *fw;
	int max_pic_w;
	int max_pic_h;
	int buffer_spec_index;
	int32_t max_one_mv_buffer_size;

	int need_cache_size;
	u64 sc_start_time;
	bool postproc_done;
	int low_latency_flag;
	bool no_head;
	bool pic_list_init_done;
	bool pic_list_init_done2;
	bool is_used_v4l;
	void *v4l2_ctx;
	bool v4l_params_parsed;
	int frameinfo_enable;
	struct vframe_qos_s vframe_qos;

#ifdef AOM_AV1_DBLK_INIT
	/*
	 * malloc may not work in real chip, please allocate memory for the following structures
	 */
	struct loop_filter_info_n_s *lfi;
	struct loopfilter *lf;
	struct segmentation_lf *seg_4lf;
#endif
	u32 mem_map_mode;
	u32 dynamic_buf_num_margin;
	struct vframe_s vframe_dummy;
	u32 res_ch_flag;
	int buffer_wrap[FRAME_BUFFERS];
	int sidebind_type;
	int sidebind_channel_id;
	u32 cur_obu_type;
	u32 multi_frame_cnt;
	u32 endian;
	u32 run_ready_min_buf_num;
	int one_package_frame_cnt;
	ulong fb_token;
	bool wait_more_buf;
	dma_addr_t rdma_phy_adr;
	unsigned *rdma_adr;
	u32 aux_data_size;
	bool no_need_aux_data;
	struct trace_decoder_name trace;
	struct vav1_assit_task assit_task;
	bool high_bandwidth_flag;
	int film_grain_present;
	ulong fg_table_handle;
	spinlock_t wait_buf_lock;
	int double_write_mode_original;
	u32 data_offset;
	u32 data_invalid;
	u32 consume_byte;
	ulong rpm_mem_handle;
	ulong lmem_phy_handle;
	ulong fg_mem_hanlde;
	ulong aux_mem_handle;
	ulong ucode_log_handle;
	ulong frame_mmu_map_handle;
	ulong frame_dw_mmu_map_handle;
	ulong rdma_handle;
	struct vdec_ge2d *ge2d;
};
static void av1_dump_state(struct vdec_s *vdec);

int av1_print(struct AV1HW_s *hw,
	int flag, const char *fmt, ...)
{
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;

	if (hw == NULL ||
		(flag == 0) ||
		(debug & flag)) {
		va_list args;

		va_start(args, fmt);
		if (hw)
			len = sprintf(buf, "[%d]", hw->index);
		vsnprintf(buf + len, HEVC_PRINT_BUF - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

unsigned char av1_is_debug(int flag)
{
	if ((flag == 0) || (debug & flag))
		return 1;

	return 0;
}

int av1_print2(int flag, const char *fmt, ...)
{
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;

	if ((flag == 0) ||
		(debug & flag)) {
		va_list args;

		va_start(args, fmt);
		vsnprintf(buf + len, HEVC_PRINT_BUF - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static int is_oversize(int w, int h)
{
	int max = (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)?
		MAX_SIZE_8K : MAX_SIZE_4K;

	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D)
		max = MAX_SIZE_2K;

	if (w <= 0 || h <= 0)
		return true;

	if (h != 0 && (w > max / h))
		return true;

	return false;
}

static int v4l_alloc_and_config_pic(struct AV1HW_s *hw,
	struct PIC_BUFFER_CONFIG_s *pic);

static inline bool close_to(int a, int b, int m)
{
	return (abs(a - b) < m) ? true : false;
}

#ifdef MULTI_INSTANCE_SUPPORT
static int av1_print_cont(struct AV1HW_s *hw,
	int flag, const char *fmt, ...)
{
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;

	if (hw == NULL ||
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

static void trigger_schedule(struct AV1HW_s *hw)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);

	if (ctx->param_sets_from_ucode &&
		!hw->v4l_params_parsed)
		vdec_v4l_write_frame_sync(ctx);

	ATRACE_COUNTER("V_ST_DEC-chunk_size", 0);

	if (hw->vdec_cb)
		hw->vdec_cb(hw_to_vdec(hw), hw->vdec_cb_arg);
}

static void reset_process_time(struct AV1HW_s *hw)
{
	if (hw->start_process_time) {
		unsigned process_time =
			1000 * (jiffies - hw->start_process_time) / HZ;
		hw->start_process_time = 0;
		if (process_time > max_process_time[hw->index])
			max_process_time[hw->index] = process_time;
	}
}

static void start_process_time(struct AV1HW_s *hw)
{
	hw->start_process_time = jiffies;
	hw->decode_timeout_count = 0;
	hw->last_lcu_idx = 0;
}

static void timeout_process(struct AV1HW_s *hw)
{
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;

	reset_process_time(hw);
	if (hw->process_busy) {
		av1_print(hw,
			0, "%s decoder timeout but process_busy\n", __func__);
		if (debug)
			av1_print(hw, 0, "debug disable timeout notify\n");
		return;
	}
	hw->timeout_num++;
	amhevc_stop();

	av1_print(hw, 0, "%s decoder timeout\n", __func__);

	vdec_v4l_post_error_event(ctx, DECODER_WARNING_DECODER_TIMEOUT);

	hw->dec_result = DEC_RESULT_DONE;
	vdec_schedule_work(&hw->work);
}

static u32 get_valid_double_write_mode(struct AV1HW_s *hw)
{
	u32 dw = ((double_write_mode & 0x80000000) == 0) ?
		hw->double_write_mode :
		(double_write_mode & 0x7fffffff);
	if ((dw & 0x20) &&
		((dw & 0xf) == 2 || (dw & 0xf) == 3)) {
		pr_info("MMU doueble write 1:4 not supported !!!\n");
		dw = 0;
	}
	return dw;
}

static int get_double_write_mode(struct AV1HW_s *hw)
{
	u32 dw;

	vdec_v4l_get_dw_mode(hw->v4l2_ctx, &dw);

	return dw;
}

/* for double write buf alloc */
static int get_double_write_mode_init(struct AV1HW_s *hw)
{
	u32 valid_dw_mode = get_valid_double_write_mode(hw);
	u32 dw;
	int w = hw->init_pic_w;
	int h = hw->init_pic_h;

	dw = 0x1; /*1:1*/
	switch (valid_dw_mode) {
	case 0x100:
		if (w > 1920 && h > 1088)
			dw = 0x4; /*1:2*/
		break;
	case 0x200:
		if (w > 1920 && h > 1088)
			dw = 0x2; /*1:4*/
		break;
	case 0x300:
		if (w > 1280 && h > 720)
			dw = 0x4; /*1:2*/
		break;
	default:
		dw = valid_dw_mode;
		break;
	}
	return dw;
}
#endif

#define FILM_GRAIN_TASK
#ifdef FILM_GRAIN_TASK

static u32 use_sfgs;
module_param(use_sfgs, uint, 0664);

void film_grain_task_wakeup(struct AV1HW_s *hw)
{
	u32 fg_reg0, fg_reg1, num_y_points, num_cb_points, num_cr_points;
	struct AV1_Common_s *cm = &hw->common;

	if (!hw->assit_task.use_sfgs || hw->eos)
		return;

	fg_reg0 = cm->cur_frame->film_grain_reg[0];
	fg_reg1 = cm->cur_frame->film_grain_reg[1];
	num_y_points = fg_reg1 & 0xf;
	num_cr_points = (fg_reg1 >> 8) & 0xf;
	num_cb_points = (fg_reg1 >> 4) & 0xf;
	if ((num_y_points > 0) ||
	((num_cb_points > 0) | ((fg_reg0 >> 17) & 0x1)) ||
	((num_cr_points > 0) | ((fg_reg0 >> 17) & 0x1)))
		hw->fgs_valid = 1;
	else
		hw->fgs_valid = 0;

	if (cm->cur_frame) {
		init_waitqueue_head(&cm->cur_frame->wait_sfgs);
		if (((get_debug_fgs() & DEBUG_FGS_BYPASS) == 0)
			&& hw->fgs_valid) {
			atomic_set(&cm->cur_frame->fgs_done, 0);
			hw->assit_task.private = cm->cur_frame;
			up(&hw->assit_task.sem);
		} else {
			atomic_set(&cm->cur_frame->fgs_done, 1);
		}
	}
}
EXPORT_SYMBOL(film_grain_task_wakeup);

static int film_grain_task(void *args)
{
	struct AV1HW_s *hw = (struct AV1HW_s *)args;
	struct vav1_assit_task *assit = &hw->assit_task;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO/2};
	RefCntBuffer *cur_frame;

	sched_setscheduler(current, SCHED_FIFO, &param);

	allow_signal(SIGTERM);

	while (down_interruptible(&assit->sem) == 0) {
		if (assit->running == false)
			break;

		if (assit->private == NULL)
			continue;

		mutex_lock(&assit->assit_mutex);
		cur_frame = (RefCntBuffer *)assit->private;
		if ((!hw->eos) && (atomic_read(&cur_frame->fgs_done) == 0)) {
				pic_film_grain_run(hw->frame_count, cur_frame->buf.sfgs_table_ptr,
					cur_frame->film_grain_ctrl, cur_frame->film_grain_reg);
				atomic_set(&cur_frame->fgs_done, 1);
				wake_up_interruptible(&cur_frame->wait_sfgs);
				assit->private = NULL;
				vdec_sync_irq(VDEC_IRQ_0);
		}
		mutex_unlock(&assit->assit_mutex);
	}

	while (!kthread_should_stop()) {
		usleep_range(500, 1000);
	}

	return 0;
}

static int film_grain_task_create(struct AV1HW_s *hw)
{
	struct vav1_assit_task *assit = &hw->assit_task;

	mutex_init(&assit->assit_mutex);

	if ((!vdec_secure(hw_to_vdec(hw)) || (get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_SC2))
		&& !use_sfgs)
		return 0;

	assit->use_sfgs = 1;
	sema_init(&assit->sem, 0);

	assit->task = kthread_run(film_grain_task, hw, "fgs_task");
	if (IS_ERR(assit->task)) {
		pr_err("%s, creat film grain task thread faild %ld\n",
			__func__, PTR_ERR(assit->task));
		return PTR_ERR(assit->task);
	}
	assit->running = true;
	assit->private = NULL;
	av1_print(hw, 0, "%s, task %px create sucess\n", __func__, assit->task);

	return 0;
}

static void film_grain_task_exit(struct AV1HW_s *hw)
{
	struct vav1_assit_task *assit = &hw->assit_task;

	if ((!assit->use_sfgs) || (IS_ERR(assit->task)))
		return;

	assit->running = false;
	up(&assit->sem);

	if (assit->task) {
		kthread_stop(assit->task);
		assit->task = NULL;
	}
	assit->use_sfgs = 0;
	av1_print(hw, 0, "%s, task kthread stoped\n", __func__);
}
#endif


/* return page number */
static int av1_mmu_page_num(struct AV1HW_s *hw,
		int w, int h, int save_mode)
{
	int picture_size;
	int cur_mmu_4k_number, max_frame_num;
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;

	picture_size = compute_losless_comp_body_size(w, h, save_mode);
	cur_mmu_4k_number = ((picture_size + (PAGE_SIZE - 1)) >> PAGE_SHIFT);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
		max_frame_num = MAX_FRAME_8K_NUM;
	else
		max_frame_num = MAX_FRAME_4K_NUM;

	if (cur_mmu_4k_number > max_frame_num) {
		vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
		pr_err("over max !! cur_mmu_4k_number 0x%x width %d height %d\n",
			cur_mmu_4k_number, w, h);
		return -1;
	}

	return cur_mmu_4k_number;
}

static struct internal_comp_buf* v4lfb_to_icomp_buf(
		struct AV1HW_s *hw,
		struct vdec_v4l2_buffer *fb)
{
	struct aml_video_dec_buf *aml_fb = NULL;
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;

	aml_fb = container_of(fb, struct aml_video_dec_buf, frame_buffer);

	return &v4l2_ctx->comp_bufs[aml_fb->internal_index];
}

static struct internal_comp_buf* index_to_icomp_buf(
		struct AV1HW_s *hw, int index)
{
	struct aml_video_dec_buf *aml_fb = NULL;
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;
	struct vdec_v4l2_buffer *fb = NULL;

	fb = (struct vdec_v4l2_buffer *)
		hw->m_BUF[index].v4l_ref_buf_addr;
	aml_fb = container_of(fb, struct aml_video_dec_buf, frame_buffer);

	return &v4l2_ctx->comp_bufs[aml_fb->internal_index];
}

int av1_alloc_mmu(
	struct AV1HW_s *hw,
	int cur_buf_idx,
	int pic_width,
	int pic_height,
	unsigned short bit_depth,
	unsigned int *mmu_index_adr)
{
	int ret = 0;
	int bit_depth_10 = (bit_depth == AOM_BITS_10);
	int cur_mmu_4k_number;
	struct internal_comp_buf *ibuf;
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;


	if (get_double_write_mode(hw) & 0x10)
		return 0;

	if (bit_depth >= AOM_BITS_12) {
		hw->fatal_error = DECODER_FATAL_ERROR_SIZE_OVERFLOW;
		pr_err("fatal_error, un support bit depth 12!\n\n");
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_UNSUPPORT);
		return -1;
	}

	cur_mmu_4k_number = av1_mmu_page_num(hw,
				pic_width,
				pic_height,
				bit_depth_10);
	if (cur_mmu_4k_number < 0)
		return -1;
	ATRACE_COUNTER(hw->trace.decode_header_memory_time_name, TRACE_HEADER_MEMORY_START);

	ibuf = index_to_icomp_buf(hw, cur_buf_idx);

	ret = decoder_mmu_box_alloc_idx(
			ibuf->mmu_box,
			ibuf->index,
			ibuf->frame_buffer_size,
			mmu_index_adr);

	if (!ret)
		ibuf->used |= 1;

	av1_print(hw, AV1_DEBUG_BUFMGR, "%s idx %d used %d cur_mmu_4k_number: %d, frame_buffer_size %u\n",
			__func__, cur_buf_idx, ibuf->used,
			cur_mmu_4k_number, ibuf->frame_buffer_size);

	ATRACE_COUNTER(hw->trace.decode_header_memory_time_name, TRACE_HEADER_MEMORY_END);
	return ret;
}

#ifdef AOM_AV1_MMU_DW
static int compute_losless_comp_body_size_dw(int width, int height,
				uint8_t is_bit_depth_10);

int av1_alloc_mmu_dw(
	struct AV1HW_s *hw,
	int cur_buf_idx,
	int pic_width,
	int pic_height,
	unsigned short bit_depth,
	unsigned int *mmu_index_adr)
{
	int ret = 0;
	int bit_depth_10 = (bit_depth == AOM_BITS_10);
	int picture_size;
	int cur_mmu_4k_number, max_frame_num;

	if (get_double_write_mode(hw) & 0x10)
		return 0;
	if (bit_depth >= AOM_BITS_12) {
		hw->fatal_error = DECODER_FATAL_ERROR_SIZE_OVERFLOW;
		pr_err("fatal_error, un support bit depth 12!\n\n");
		return -1;
	}
	picture_size = compute_losless_comp_body_size_dw(pic_width, pic_height,
				   bit_depth_10);
	cur_mmu_4k_number = ((picture_size + (1 << 12) - 1) >> 12);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
		max_frame_num = MAX_FRAME_8K_NUM;
	else
		max_frame_num = MAX_FRAME_4K_NUM;

	if (cur_mmu_4k_number > max_frame_num) {
		pr_err("over max !! cur_mmu_4k_number 0x%x width %d height %d\n",
			cur_mmu_4k_number, pic_width, pic_height);
		return -1;
	}
	if (hw->is_used_v4l) {
		struct internal_comp_buf *ibuf =
			index_to_icomp_buf(hw, cur_buf_idx);

		ret = decoder_mmu_box_alloc_idx(
				ibuf->mmu_box_dw,
				ibuf->index,
				ibuf->frame_buffer_size,
				mmu_index_adr);
	} else {
		ret = decoder_mmu_box_alloc_idx(
			hw->mmu_box_dw,
			cur_buf_idx,
			cur_mmu_4k_number,
			mmu_index_adr);
	}
	return ret;
}
#endif

#ifndef MV_USE_FIXED_BUF
static void dealloc_mv_bufs(struct AV1HW_s *hw)
{
	int i;
	for (i = 0; i < MV_BUFFER_NUM; i++) {
		if (hw->m_mv_BUF[i].start_adr) {
			if (debug)
				pr_info(
				"dealloc mv buf(%d) adr %ld size 0x%x used_flag %d\n",
				i, hw->m_mv_BUF[i].start_adr,
				hw->m_mv_BUF[i].size,
				hw->m_mv_BUF[i].used_flag);
			decoder_bmmu_box_free_idx(
				hw->bmmu_box,
				MV_BUFFER_IDX(i));
			hw->m_mv_BUF[i].start_adr = 0;
			hw->m_mv_BUF[i].size = 0;
			hw->m_mv_BUF[i].used_flag = 0;
		}
	}
}

static int alloc_mv_buf(struct AV1HW_s *hw,
	int i, int size)
{
	int ret = 0;
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;

	if (hw->m_mv_BUF[i].start_adr &&
		size > hw->m_mv_BUF[i].size) {
		dealloc_mv_bufs(hw);
	} else if (hw->m_mv_BUF[i].start_adr)
		return 0;

	if (decoder_bmmu_box_alloc_buf_phy
		(hw->bmmu_box,
		MV_BUFFER_IDX(i), size,
		DRIVER_NAME,
		&hw->m_mv_BUF[i].start_adr) < 0) {
		hw->m_mv_BUF[i].start_adr = 0;
		ret = -1;
		vdec_v4l_post_error_event(ctx, DECODER_ERROR_ALLOC_BUFFER_FAIL);
	} else {
		hw->m_mv_BUF[i].size = size;
		hw->m_mv_BUF[i].used_flag = 0;
		ret = 0;
		if (debug) {
			pr_info(
			"MV Buffer %d: start_adr %p size %x\n",
			i,
			(void *)hw->m_mv_BUF[i].start_adr,
			hw->m_mv_BUF[i].size);
		}
	}
	return ret;
}

static int cal_mv_buf_size(struct AV1HW_s *hw, int pic_width, int pic_height)
{
	unsigned lcu_size = hw->current_lcu_size;
	int extended_pic_width = (pic_width + lcu_size -1)
				& (~(lcu_size - 1));
	int extended_pic_height = (pic_height + lcu_size -1)
				& (~(lcu_size - 1));

	int lcu_x_num = extended_pic_width / lcu_size;
	int lcu_y_num = extended_pic_height / lcu_size;
	int size_a, size_b, size;

	if (get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_SC2)
		/*tm2revb and sc2*/
		size_a = lcu_x_num * lcu_y_num * 16 *
			((lcu_size == 64) ? 16 : 64);
	else
		size_a = lcu_x_num * lcu_y_num * 16 *
			((lcu_size == 64) ? 19 : 76);

	size_b = lcu_x_num * ((lcu_y_num >> 3) +
			(lcu_y_num & 0x7)) * 16;
	size = ((size_a + size_b) + 0xffff) & (~0xffff);

	if (debug & AOM_DEBUG_USE_FIXED_MV_BUF_SIZE)
		size = hw->max_one_mv_buffer_size;
	if (force_max_one_mv_buffer_size)
		size = force_max_one_mv_buffer_size;
	return size;
}

static int init_mv_buf_list(struct AV1HW_s *hw)
{
	int i;
	int ret = 0;
	int count = MV_BUFFER_NUM;
	int pic_width = hw->init_pic_w;
	int pic_height = hw->init_pic_h;
	int size = cal_mv_buf_size(hw, pic_width, pic_height);

	if (mv_buf_dynamic_alloc)
		return 0;

	if (debug)
		pr_info("%s, calculated mv size 0x%x\n",
			__func__, size);

	if (!IS_8K_SIZE(pic_width, pic_height)) {
		if (vdec_is_support_4k())
			size = 0xb0000;
		else
			size = 0x30000;
	}

	if (hw->init_pic_w > 4096 && hw->init_pic_h > 2048)
		count = REF_FRAMES_4K + hw->mv_buf_margin;
	else if (hw->init_pic_w > 2048 && hw->init_pic_h > 1088)
		count = REF_FRAMES_4K + hw->mv_buf_margin;
	else
		count = REF_FRAMES + hw->mv_buf_margin;

	if (debug) {
		pr_info("%s w:%d, h:%d, count: %d, size 0x%x\n",
		__func__, hw->init_pic_w, hw->init_pic_h,
		count, size);
	}

	for (i = 0;
		i < count && i < MV_BUFFER_NUM; i++) {
		if (alloc_mv_buf(hw, i, size) < 0) {
			ret = -1;
			break;
		}
	}
	return ret;
}

static int get_mv_buf(struct AV1HW_s *hw,
		struct PIC_BUFFER_CONFIG_s *pic_config)
{
	int i;
	int ret = -1;
	if (mv_buf_dynamic_alloc) {
		int size = cal_mv_buf_size(hw,
			pic_config->y_crop_width, pic_config->y_crop_height);
		for (i = 0; i < MV_BUFFER_NUM; i++) {
			if (hw->m_mv_BUF[i].start_adr == 0) {
				ret = i;
				break;
			}
		}
		if (i == MV_BUFFER_NUM) {
			pr_info(
			"%s: Error, mv buf MV_BUFFER_NUM is not enough\n",
			__func__);
			return ret;
		}

		if (alloc_mv_buf(hw, ret, size) >= 0) {
			pic_config->mv_buf_index = ret;
			pic_config->mpred_mv_wr_start_addr =
				(hw->m_mv_BUF[ret].start_adr + 0xffff) &
				(~0xffff);
		} else {
			pr_info("%s: Error, mv buf alloc fail\n", __func__);
		}
		return ret;
	}

	for (i = 0; i < MV_BUFFER_NUM; i++) {
		if (hw->m_mv_BUF[i].start_adr &&
			hw->m_mv_BUF[i].used_flag == 0) {
			hw->m_mv_BUF[i].used_flag = 1;
			ret = i;
			break;
		}
	}

	if (ret >= 0) {
		pic_config->mv_buf_index = ret;
		pic_config->mpred_mv_wr_start_addr =
			(hw->m_mv_BUF[ret].start_adr + 0xffff) &
			(~0xffff);
		if (debug & AV1_DEBUG_BUFMGR)
			pr_info("%s => %d (%d) size 0x%x\n", __func__, ret,
				pic_config->mpred_mv_wr_start_addr, hw->m_mv_BUF[ret].size);
	} else {
		pr_info("%s: Error, mv buf is not enough\n", __func__);
		if (debug & AV1_DEBUG_BUFMGR) {
			dump_pic_list(hw);
			for (i = 0; i < MAX_BUF_NUM; i++) {
				av1_print(hw, 0,
					"mv_Buf(%d) start_adr 0x%x size 0x%x used %d\n",
					i,
					hw->m_mv_BUF[i].start_adr,
					hw->m_mv_BUF[i].size,
					hw->m_mv_BUF[i].used_flag);
			}
		}
	}
	return ret;
}
static void put_mv_buf(struct AV1HW_s *hw,
				int *mv_buf_index)
{
	int i = *mv_buf_index;
	if (i >= MV_BUFFER_NUM) {
		if (debug & AV1_DEBUG_BUFMGR)
			pr_info("%s: index %d beyond range\n", __func__, i);
		return;
	}
	if (mv_buf_dynamic_alloc) {
		if (hw->m_mv_BUF[i].start_adr) {
			if (debug)
				pr_info("dealloc mv buf(%d) adr %ld size 0x%x used_flag %d\n",
					i, hw->m_mv_BUF[i].start_adr,
					hw->m_mv_BUF[i].size,
					hw->m_mv_BUF[i].used_flag);
			decoder_bmmu_box_free_idx(hw->bmmu_box, MV_BUFFER_IDX(i));
			hw->m_mv_BUF[i].start_adr = 0;
			hw->m_mv_BUF[i].size = 0;
			hw->m_mv_BUF[i].used_flag = 0;
		}
		*mv_buf_index = -1;
		return;
	}

	if (debug & AV1_DEBUG_BUFMGR)
		pr_info("%s(%d): used_flag(%d)\n", __func__, i, hw->m_mv_BUF[i].used_flag);

	*mv_buf_index = -1;
	if (hw->m_mv_BUF[i].start_adr &&
		hw->m_mv_BUF[i].used_flag)
		hw->m_mv_BUF[i].used_flag = 0;
}
static void	put_un_used_mv_bufs(struct AV1HW_s *hw)
{
	struct AV1_Common_s *const cm = &hw->common;
	struct RefCntBuffer_s *const frame_bufs = cm->buffer_pool->frame_bufs;
	int i;
	for (i = 0; i < hw->used_buf_num; ++i) {
		if ((frame_bufs[i].ref_count == 0) &&
			(frame_bufs[i].buf.index != -1) &&
			(frame_bufs[i].buf.mv_buf_index >= 0)
			)
			put_mv_buf(hw, &frame_bufs[i].buf.mv_buf_index);
	}
}
#endif

static void init_pic_list_hw(struct AV1HW_s *pbi);

static void update_hide_frame_timestamp(struct AV1HW_s *hw)
{
	RefCntBuffer *const frame_bufs = hw->common.buffer_pool->frame_bufs;
	int i;

	for (i = 0; i < hw->used_buf_num; ++i) {
		if ((!frame_bufs[i].show_frame) &&
			(frame_bufs[i].showable_frame) &&
			(!frame_bufs[i].buf.vf_ref) &&
			(frame_bufs[i].buf.BUF_index != -1)) {
			frame_bufs[i].buf.timestamp = hw->chunk->timestamp;
			av1_print(hw, AV1_DEBUG_OUT_PTS,
				"%s, update %d hide frame ts: %lld\n",
				__func__, i, frame_bufs[i].buf.timestamp);
		}
	}
}

static int get_free_fb_idx(AV1_COMMON *cm)
{
	int i;
	RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;

	for (i = 0; i < FRAME_BUFFERS; ++i) {
		if ((frame_bufs[i].ref_count == 0) &&
			(frame_bufs[i].buf.vf_ref == 0) &&
			(frame_bufs[i].buf.repeat_count == 0))
			break;
	}

	return (i != FRAME_BUFFERS) ? i : -1;
}

static int v4l_get_free_fb(struct AV1HW_s *hw)
{
	struct AV1_Common_s *const cm = &hw->common;
	struct RefCntBuffer_s *const frame_bufs = cm->buffer_pool->frame_bufs;
	struct aml_vcodec_ctx * v4l = hw->v4l2_ctx;
	struct v4l_buff_pool *pool = &v4l->cap_pool;
	struct PIC_BUFFER_CONFIG_s *pic = NULL;
	struct PIC_BUFFER_CONFIG_s *free_pic = NULL;
	ulong flags;
	int idx, i;

	lock_buffer_pool(cm->buffer_pool, flags);

	for (i = 0; i < pool->in; ++i) {
		u32 state = (pool->seq[i] >> 16);
		u32 index = (pool->seq[i] & 0xffff);

		switch (state) {
		case V4L_CAP_BUFF_IN_DEC:
			pic = &frame_bufs[i].buf;
			if ((frame_bufs[i].ref_count == 0) &&
				(pic->vf_ref == 0) &&
				(pic->repeat_count == 0) &&
				(pic->index != -1) &&
				pic->cma_alloc_addr) {
				free_pic = pic;
			}
			break;
		case V4L_CAP_BUFF_IN_M2M:
			idx = get_free_fb_idx(cm);
			if (idx < 0)
				break;

			pic = &frame_bufs[idx].buf;
			pic->y_crop_width = hw->frame_width;
			pic->y_crop_height = hw->frame_height;
			hw->buffer_wrap[idx] = index;
			if (!v4l_alloc_and_config_pic(hw, pic)) {
				set_canvas(hw, pic);
				init_pic_list_hw(hw);
				free_pic = pic;
			}
			break;
		default:
			break;
		}

		if (free_pic) {
			if (frame_bufs[i].buf.use_external_reference_buffers) {
				// If this frame buffer's y_buffer, u_buffer, and v_buffer point to the
				// external reference buffers. Restore the buffer pointers to point to the
				// internally allocated memory.
				PIC_BUFFER_CONFIG *ybf = &frame_bufs[i].buf;

				ybf->y_buffer = ybf->store_buf_adr[0];
				ybf->u_buffer = ybf->store_buf_adr[1];
				ybf->v_buffer = ybf->store_buf_adr[2];
				ybf->use_external_reference_buffers = 0;
			}

			frame_bufs[i].ref_count = 1;
			break;
		}
	}

	if (free_pic && hw->chunk) {
		free_pic->timestamp = hw->chunk->timestamp;
		update_hide_frame_timestamp(hw);
	}

	unlock_buffer_pool(cm->buffer_pool, flags);

	if (free_pic) {
		struct vdec_v4l2_buffer *fb =
			(struct vdec_v4l2_buffer *)
			hw->m_BUF[free_pic->index].v4l_ref_buf_addr;

		fb->status = FB_ST_DECODER;

		v4l->aux_infos.bind_sei_buffer(v4l, &free_pic->aux_data_buf,
			&free_pic->aux_data_size, &free_pic->ctx_buf_idx);

	}

	if (debug & AV1_DEBUG_OUT_PTS) {
		if (free_pic) {
			pr_debug("%s, idx: %d, ts: %lld\n",
				__func__, free_pic->index, free_pic->timestamp);
		} else {
			pr_debug("%s, av1 get free pic null\n", __func__);
		}
	}

	return free_pic ? free_pic->index : INVALID_IDX;
}

int get_free_frame_buffer(struct AV1_Common_s *cm)
{
	struct AV1HW_s *hw = container_of(cm, struct AV1HW_s, common);

	put_un_used_mv_bufs(hw);

	return v4l_get_free_fb(hw);
}

static int get_free_buf_count(struct AV1HW_s *hw)
{
	struct AV1_Common_s *const cm = &hw->common;
	struct RefCntBuffer_s *const frame_bufs = cm->buffer_pool->frame_bufs;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int i, free_buf_count = 0;

	for (i = 0; i < hw->used_buf_num; ++i) {
		if ((frame_bufs[i].ref_count == 0) &&
			(frame_bufs[i].buf.vf_ref == 0) &&
			(frame_bufs[i].buf.repeat_count == 0) &&
			frame_bufs[i].buf.cma_alloc_addr) {
			free_buf_count++;
		}
	}

	if (ctx->cap_pool.dec < hw->used_buf_num) {
		if (ctx->fb_ops.query(&ctx->fb_ops, &hw->fb_token)) {
			free_buf_count +=
				v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx) + 1;
		}
	}

	/* trigger to parse head data. */
	if (!hw->v4l_params_parsed) {
		free_buf_count = hw->run_ready_min_buf_num;
	}
	if ((debug & AV1_DEBUG_BUFMGR_MORE) &&
		(free_buf_count <= 0)) {
		pr_info("%s, free count %d, m2m_ready %d\n",__func__,
			free_buf_count, v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx));
	}

	return free_buf_count;
}

int aom_bufmgr_init(struct AV1HW_s *hw, struct BuffInfo_s *buf_spec_i,
		struct buff_s *mc_buf_i) {
	struct AV1_Common_s *cm = &hw->common;
	if (debug)
		pr_info("%s %d %p\n", __func__, __LINE__, hw->pbi);
	hw->frame_count = 0;
	hw->pic_count = 0;
	hw->pre_stream_offset = 0;
	spin_lock_init(&cm->buffer_pool->lock);
	cm->prev_fb_idx = INVALID_IDX;
	cm->new_fb_idx = INVALID_IDX;
	hw->used_4k_num = -1;
	cm->cur_fb_idx_mmu = INVALID_IDX;
	pr_debug("After aom_bufmgr_init, prev_fb_idx : %d, new_fb_idx : %d\r\n",
		cm->prev_fb_idx, cm->new_fb_idx);
	hw->need_resync = 1;

	cm->current_video_frame = 0;
	hw->ready_for_new_data = 1;

	/* private init */
	hw->work_space_buf = buf_spec_i;
	if ((get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_GXL) ||
		(get_double_write_mode(hw) == 0x10)) {
		hw->mc_buf = mc_buf_i;
	}

	hw->rpm_addr = NULL;
	hw->lmem_addr = NULL;

	hw->use_cma_flag = 0;
	hw->decode_idx = 0;
	hw->result_done_count = 0;
	hw->has_keyframe = 0;
	hw->has_sequence = 0;
	hw->skip_flag = 0;
	hw->wait_buf = 0;
	hw->error_flag = 0;

	hw->last_pts = 0;
	hw->last_pts_us64 = 0;
	hw->shift_byte_count = 0;
	hw->shift_byte_count_lo = 0;
	hw->shift_byte_count_hi = 0;
	hw->pts_mode_switching_count = 0;
	hw->pts_mode_recovery_count = 0;

	hw->buf_num = 0;
	hw->pic_num = 0;

	return 0;
}

/*
struct AV1HW_s av1_decoder;
union param_u av1_param;
*/
/**************************************************
 *
 *AV1 buffer management end
 *
 ***************************************************
 */


#define HEVC_CM_BODY_START_ADDR    			    0x3626
#define HEVC_CM_BODY_LENGTH    				    0x3627
#define HEVC_CM_HEADER_LENGTH    				  0x3629
#define HEVC_CM_HEADER_OFFSET    				  0x362b

#define LOSLESS_COMPRESS_MODE

#ifdef AV1_10B_NV21
static u32 mem_map_mode = 2  /* 0:linear 1:32x32 2:64x32*/
#else
static u32 mem_map_mode; /* 0:linear 1:32x32 2:64x32 ; m8baby test1902 */
#endif
static u32 enable_mem_saving = 1;
static u32 force_w_h;

static u32 force_fps;


const u32 av1_version = 201602101;
static u32 debug;
static u32 radr;
static u32 rval;
static u32 pop_shorts;
static u32 dbg_cmd;
static u32 dbg_skip_decode_index;
/*
 * bit 0~3, for HEVCD_IPP_AXIIF_CONFIG endian config
 * bit 8~23, for HEVC_SAO_CTRL1 endian config
 */
static u32 endian;
#define HEVC_CONFIG_BIG_ENDIAN     ((0x880 << 8) | 0x8)
#define HEVC_CONFIG_LITTLE_ENDIAN  ((0xff0 << 8) | 0xf)

static u32 multi_frames_in_one_pack = 1;
#ifdef ERROR_HANDLE_DEBUG
static u32 dbg_nal_skip_flag;
/* bit[0], skip vps; bit[1], skip sps; bit[2], skip pps */
static u32 dbg_nal_skip_count;
#endif
/*for debug*/
static u32 decode_pic_begin;
static uint slice_parse_begin;
static u32 step;
#ifdef MIX_STREAM_SUPPORT
static u32 buf_alloc_width = 4096;
static u32 buf_alloc_height = 2304;
static u32 av1_max_pic_w = 4096;
static u32 av1_max_pic_h = 2304;

static u32 dynamic_buf_num_margin = 3;
#else
static u32 buf_alloc_width;
static u32 buf_alloc_height;
static u32 dynamic_buf_num_margin = 7;
#endif
static u32 buf_alloc_depth = 10;
static u32 buf_alloc_size;
/*
 *bit[0]: 0,
 *    bit[1]: 0, always release cma buffer when stop
 *    bit[1]: 1, never release cma buffer when stop
 *bit[0]: 1, when stop, release cma buffer if blackout is 1;
 *do not release cma buffer is blackout is not 1
 *
 *bit[2]: 0, when start decoding, check current displayed buffer
 *	 (only for buffer decoded by AV1) if blackout is 0
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
 *bit 0, 1: only display I picture;
 *bit 1, 1: only decode I picture;
 */
static u32 i_only_flag;

static u32 low_latency_flag;

static u32 no_head;

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
 *0, auto search after error recovery (av1_recover() called);
 *1, manual search after error recovery
 *(change to auto search after get IDR: WRITE_VREG(NAL_SEARCH_CTL, 0x2))
 *
 *bit 4: 0, set error_mark after reset/recover
 *    1, do not set error_mark after reset/recover
 *bit 5: 0, check total lcu for every picture
 *    1, do not check total lcu
 *
 */

static u32 error_handle_policy;
#define MAX_BUF_NUM_NORMAL     16
/*less bufs num 12 caused frame drop, nts failed*/
#define MAX_BUF_NUM_LESS   14
static u32 max_buf_num = MAX_BUF_NUM_NORMAL;
#define MAX_BUF_NUM_SAVE_BUF  8

static DEFINE_MUTEX(vav1_mutex);
#ifndef MULTI_INSTANCE_SUPPORT
static struct device *cma_dev;
#endif
#define HEVC_DEC_STATUS_REG       HEVC_ASSIST_SCRATCH_0
#define HEVC_FG_STATUS			HEVC_ASSIST_SCRATCH_B
#define HEVC_RPM_BUFFER           HEVC_ASSIST_SCRATCH_1
#define AOM_AV1_ADAPT_PROB_REG        HEVC_ASSIST_SCRATCH_3
#define AOM_AV1_MMU_MAP_BUFFER        HEVC_ASSIST_SCRATCH_4 // changed to use HEVC_ASSIST_MMU_MAP_ADDR
#define AOM_AV1_DAALA_TOP_BUFFER      HEVC_ASSIST_SCRATCH_5
//#define HEVC_SAO_UP               HEVC_ASSIST_SCRATCH_6
//#define HEVC_STREAM_SWAP_BUFFER   HEVC_ASSIST_SCRATCH_7
#define AOM_AV1_CDF_BUFFER_W      HEVC_ASSIST_SCRATCH_8
#define AOM_AV1_CDF_BUFFER_R      HEVC_ASSIST_SCRATCH_9
#define AOM_AV1_COUNT_SWAP_BUFFER     HEVC_ASSIST_SCRATCH_A
#define AOM_AV1_SEG_MAP_BUFFER_W  AV1_SEG_W_ADDR  //    HEVC_ASSIST_SCRATCH_B
#define AOM_AV1_SEG_MAP_BUFFER_R  AV1_SEG_R_ADDR  //    HEVC_ASSIST_SCRATCH_C
//#define HEVC_sao_vb_size          HEVC_ASSIST_SCRATCH_B
//#define HEVC_SAO_VB               HEVC_ASSIST_SCRATCH_C
//#define HEVC_SCALELUT             HEVC_ASSIST_SCRATCH_D
#define HEVC_WAIT_FLAG	          HEVC_ASSIST_SCRATCH_E
#define RPM_CMD_REG               HEVC_ASSIST_SCRATCH_F
//#define HEVC_STREAM_SWAP_TEST     HEVC_ASSIST_SCRATCH_L

#ifdef MULTI_INSTANCE_SUPPORT
#define HEVC_DECODE_COUNT       HEVC_ASSIST_SCRATCH_M
#define HEVC_DECODE_SIZE		HEVC_ASSIST_SCRATCH_N
#else
#define HEVC_DECODE_PIC_BEGIN_REG HEVC_ASSIST_SCRATCH_M
#define HEVC_DECODE_PIC_NUM_REG   HEVC_ASSIST_SCRATCH_N
#endif
#define AOM_AV1_SEGMENT_FEATURE   AV1_QUANT_WR

#define DEBUG_REG1              HEVC_ASSIST_SCRATCH_G
#define DEBUG_REG2              HEVC_ASSIST_SCRATCH_H

#define LMEM_DUMP_ADR     	        HEVC_ASSIST_SCRATCH_I
#define CUR_NAL_UNIT_TYPE       HEVC_ASSIST_SCRATCH_J
#define DECODE_STOP_POS           HEVC_ASSIST_SCRATCH_K

#define PIC_END_LCU_COUNT       HEVC_ASSIST_SCRATCH_2

#define HEVC_AUX_ADR			HEVC_ASSIST_SCRATCH_L
#define HEVC_AUX_DATA_SIZE		HEVC_ASSIST_SCRATCH_7
#if (defined DEBUG_UCODE_LOG) || (defined DEBUG_CMD)
#define HEVC_DBG_LOG_ADR       HEVC_ASSIST_SCRATCH_C
#ifdef DEBUG_CMD
#define HEVC_D_ADR               HEVC_ASSIST_SCRATCH_4
#endif
#endif
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
 *	0, do not parse SEI in ucode; 1, parse SEI in ucode
 *bit [31:20]: used by ucode for debug purpose
 */
#define NAL_SEARCH_CTL            HEVC_ASSIST_SCRATCH_I
	/*[31:24] chip feature
		31: 0, use MBOX1; 1, use MBOX0
	  [24:16] debug
	    0x1, bufmgr only
	*/
#define DECODE_MODE              HEVC_ASSIST_SCRATCH_J
#define DECODE_STOP_POS         HEVC_ASSIST_SCRATCH_K

#define RPM_BUF_SIZE ((RPM_END - RPM_BEGIN) * 2)
#define LMEM_BUF_SIZE (0x600 * 2)

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

/* necessary 4K page size align for t7/t3 decoder and after. fix case1440 dec timeout */
#define WORKBUF_ALIGN(addr) (ALIGN(addr, PAGE_SIZE))

#define WORK_BUF_SPEC_NUM 3

static struct BuffInfo_s aom_workbuff_spec[WORK_BUF_SPEC_NUM] = {
	{ //8M bytes
		.max_width = 1920,
		.max_height = 1088,
		.ipp = {
			// IPP work space calculation : 4096 * (Y+CbCr+Flags) = 12k, round to 16k
			.buf_size = 0x1E00,
		},
		.sao_abv = {
			.buf_size = 0x0, //0x30000,
		},
		.sao_vb = {
			.buf_size = 0x0, //0x30000,
		},
		.short_term_rps = {
			// SHORT_TERM_RPS - Max 64 set, 16 entry every set, total 64x16x2 = 2048 bytes (0x800)
			.buf_size = 0x800,
		},
		.vps = {
			// VPS STORE AREA - Max 16 VPS, each has 0x80 bytes, total 0x0800 bytes
			.buf_size = 0x800,
		},
		.seg_map = {
			// SEGMENT MAP AREA(roundup 128) - 1920x 1152/4/4 * 3 bits = 0xBF40 Bytes * 16 = 0xBF400
			.buf_size = 0xCA800,
		},
		.daala_top = {
			// DAALA TOP STORE AREA - 224 Bytes (use 256 Bytes for LPDDR4) per 128. Total 4096/128*256 = 0x2000
			.buf_size = 0xf00,
		},
		.sao_up = {
			// SAO UP STORE AREA - Max 640(10240/16) LCU, each has 16 bytes total 0x2800 bytes
			.buf_size = 0x0, //0x2800,
		},
		.swap_buf = {
			// 256cyclex64bit = 2K bytes 0x800 (only 144 cycles valid)
			.buf_size = 0x800,
		},
		.cdf_buf = {
			// for context store/load 1024x256 x16 = 512K bytes 16*0x8000
			.buf_size = 0x80000,
		},
		.gmc_buf = {
			// for gmc_parameter store/load 128 x 16 = 2K bytes 0x800
			.buf_size = 0x800,
		},
		.scalelut = {
			// support up to 32 SCALELUT 1024x32 = 32Kbytes (0x8000)
			.buf_size = 0x0, //0x8000,
		},
		.dblk_para = {
			// DBLK -> Max 256(4096/16) LCU, each para 1024bytes(total:0x40000), data 1024bytes(total:0x40000)
			.buf_size = 0xd00, /*0xc40*/
		},
		.dblk_data = {
			.buf_size = 0x49000,
		},
		.cdef_data = {
			.buf_size = 0x22400,
		},
		.ups_data = {
			.buf_size = 0x36000,
		},
		.fgs_table = {
			.buf_size = FGS_TABLE_SIZE * FRAME_BUFFERS, // 512x128bits
		},
#ifdef AOM_AV1_MMU
		.mmu_vbh = {
			.buf_size = VBH_BUF_SIZE_1080P, //2*16*(more than 2304)/4, 4K
		},
		.cm_header = {
	#ifdef USE_SPEC_BUF_FOR_MMU_HEAD
			.buf_size = MMU_COMPRESS_HEADER_SIZE_1080P * FRAME_BUFFERS, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
	#else
			.buf_size = 0,
	#endif
		},
#endif
#ifdef AOM_AV1_MMU_DW
		.mmu_vbh_dw = {
			.buf_size = DW_VBH_BUF_SIZE_1080P, //2*16*(more than 2304)/4, 4K
		},
		.cm_header_dw = {
	#ifdef USE_SPEC_BUF_FOR_MMU_HEAD
			.buf_size = MMU_COMPRESS_HEADER_SIZE_1080P*FRAME_BUFFERS, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
	#else
			.buf_size = 0,
	#endif
		},
#endif
		.mpred_above = {
			.buf_size = 0x2800, /*round from 0x2760*/ /* 2 * size of hw*/
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
		   .buf_size = MAX_ONE_MV_BUFFER_SIZE_1080P_TM2REVB * FRAME_BUFFERS,/*round from 203A0*/ //1080p, 0x40000 per buffer
		},
#endif
		.rpm = {
		   .buf_size = 0x80*2,
		},
		.lmem = {
			.buf_size = 0x400 * 2,
		}
	},
	{
#ifdef VPU_FILMGRAIN_DUMP
		.max_width = 640,
		.max_height = 480,
#else
		.max_width = 4096,
		.max_height = 2304,
#endif
		.ipp = {
			// IPP work space calculation : 4096 * (Y+CbCr+Flags) = 12k, round to 16k
			.buf_size = 0x4000,
		},
		.sao_abv = {
			.buf_size = 0x0, //0x30000,
		},
		.sao_vb = {
			.buf_size = 0x0, //0x30000,
		},
		.short_term_rps = {
			// SHORT_TERM_RPS - Max 64 set, 16 entry every set, total 64x16x2 = 2048 bytes (0x800)
			.buf_size = 0x800,
		},
		.vps = {
			// VPS STORE AREA - Max 16 VPS, each has 0x80 bytes, total 0x0800 bytes
			.buf_size = 0x800,
		},
		.seg_map = {
			// SEGMENT MAP AREA - 4096x2304/4/4 * 3 bits = 0x36000 Bytes * 16 = 0x360000
			.buf_size = 0x360000,
		},
		.daala_top = {
			// DAALA TOP STORE AREA - 224 Bytes (use 256 Bytes for LPDDR4) per 128. Total 4096/128*256 = 0x2000
			.buf_size = 0x2000,
		},
		.sao_up = {
			// SAO UP STORE AREA - Max 640(10240/16) LCU, each has 16 bytes total 0x2800 bytes
			.buf_size = 0x0, //0x2800,
		},
		.swap_buf = {
			// 256cyclex64bit = 2K bytes 0x800 (only 144 cycles valid)
			.buf_size = 0x800,
		},
		.cdf_buf = {
		// for context store/load 1024x256 x16 = 512K bytes 16*0x8000
			.buf_size = 0x80000,
		},
		.gmc_buf = {
		// for gmc_parameter store/load 128 x 16 = 2K bytes 0x800
			.buf_size = 0x800,
		},
		.scalelut = {
			// support up to 32 SCALELUT 1024x32 = 32Kbytes (0x8000)
			.buf_size = 0x0, //0x8000,
		},
		.dblk_para = {
			// DBLK -> Max 256(4096/16) LCU, each para 1024bytes(total:0x40000), data 1024bytes(total:0x40000)
			.buf_size = 0x1a00, /*0x1980*/
		},
		.dblk_data = {
			.buf_size = 0x52800,
		},
		.cdef_data = {
			.buf_size = 0x24a00,
		},
		.ups_data = {
			.buf_size = 0x6f000,
		},
		.fgs_table = {
			.buf_size = FGS_TABLE_SIZE * FRAME_BUFFERS, // 512x128bits
		},
#ifdef AOM_AV1_MMU
		.mmu_vbh = {
			.buf_size = VBH_BUF_SIZE_4K, //2*16*(more than 2304)/4, 4K
		},
		.cm_header = {
	#ifdef USE_SPEC_BUF_FOR_MMU_HEAD
			.buf_size = MMU_COMPRESS_HEADER_SIZE_4K*FRAME_BUFFERS, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
	#else
			.buf_size = 0,
	#endif
		},
#endif
#ifdef AOM_AV1_MMU_DW
		.mmu_vbh_dw = {
			.buf_size = DW_VBH_BUF_SIZE_4K, //2*16*(more than 2304)/4, 4K
		},
		.cm_header_dw = {
	#ifdef USE_SPEC_BUF_FOR_MMU_HEAD
			.buf_size = MMU_COMPRESS_HEADER_SIZE_4K*FRAME_BUFFERS, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
	#else
			.buf_size = 0,
	#endif
		},
#endif
		.mpred_above = {
			.buf_size = 0x5400, /* 2 * size of hw*/
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
		  /* .buf_size = 0x100000*16,
		  //4k2k , 0x100000 per buffer */
		  /* 4096x2304 , 0x120000 per buffer */
		  .buf_size = MAX_ONE_MV_BUFFER_SIZE_4K_TM2REVB * FRAME_BUFFERS,
		},
#endif
		.rpm = {
		   .buf_size = 0x80*2,
		},
		.lmem = {
			.buf_size = 0x400 * 2,
		}

	},
	{
		.max_width = 8192,
		.max_height = 4608,
		.ipp = {
			// IPP work space calculation : 4096 * (Y+CbCr+Flags) = 12k, round to 16k
			.buf_size = 0x4000,
		},
		.sao_abv = {
			.buf_size = 0x0, //0x30000,
		},
		.sao_vb = {
			.buf_size = 0x0, //0x30000,
		},
		.short_term_rps = {
			// SHORT_TERM_RPS - Max 64 set, 16 entry every set, total 64x16x2 = 2048 bytes (0x800)
			.buf_size = 0x800,
		},
		.vps = {
			// VPS STORE AREA - Max 16 VPS, each has 0x80 bytes, total 0x0800 bytes
			.buf_size = 0x800,
		},
		.seg_map = {
			// SEGMENT MAP AREA - 4096x2304/4/4 * 3 bits = 0x36000 Bytes * 16 = 0x360000
			.buf_size = 0xd80000,
		},
		.daala_top = {
			// DAALA TOP STORE AREA - 224 Bytes (use 256 Bytes for LPDDR4) per 128. Total 4096/128*256 = 0x2000
			.buf_size = 0x2000,
		},
		.sao_up = {
			// SAO UP STORE AREA - Max 640(10240/16) LCU, each has 16 bytes total 0x2800 bytes
			.buf_size = 0x0, //0x2800,
		},
		.swap_buf = {
			// 256cyclex64bit = 2K bytes 0x800 (only 144 cycles valid)
			.buf_size = 0x800,
		},
		.cdf_buf = {
		// for context store/load 1024x256 x16 = 512K bytes 16*0x8000
			.buf_size = 0x80000,
		},
		.gmc_buf = {
		// for gmc_parameter store/load 128 x 16 = 2K bytes 0x800
			.buf_size = 0x800,
		},
		.scalelut = {
			// support up to 32 SCALELUT 1024x32 = 32Kbytes (0x8000)
			.buf_size = 0x0, //0x8000,
		},
		.dblk_para = {
			// DBLK -> Max 256(4096/16) LCU, each para 1024bytes(total:0x40000), data 1024bytes(total:0x40000)
			.buf_size = 0x3300, /*0x32a0*/
		},
		.dblk_data = {
			.buf_size = 0xa4800,
		},
		.cdef_data = {
			.buf_size = 0x29200,
		},
		.ups_data = {
			.buf_size = 0xdb000,
		},
		.fgs_table = {
			.buf_size = FGS_TABLE_SIZE * FRAME_BUFFERS, // 512x128bits
		},
#ifdef AOM_AV1_MMU
		.mmu_vbh = {
			.buf_size = VBH_BUF_SIZE_8K, //2*16*(more than 2304)/4, 4K
		},
		.cm_header = {
	#ifdef USE_SPEC_BUF_FOR_MMU_HEAD
			.buf_size = MMU_COMPRESS_HEADER_SIZE_8K*FRAME_BUFFERS, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
#else
			.buf_size = 0,
#endif
		},
#endif
#ifdef AOM_AV1_MMU_DW
		.mmu_vbh_dw = {
			.buf_size = DW_VBH_BUF_SIZE_8K, //2*16*(more than 2304)/4, 4K
		},
		.cm_header_dw = {
	#ifdef USE_SPEC_BUF_FOR_MMU_HEAD
			.buf_size = MMU_COMPRESS_HEADER_SIZE_8K*FRAME_BUFFERS, // 0x44000 = ((1088*2*1024*4)/32/4)*(32/8)
	#else
			.buf_size = 0,
	#endif
		},
#endif
		.mpred_above = {
			.buf_size = 0xA800, /* 2 * size of hw*/
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
		  /* .buf_size = 0x100000*16,
		  //4k2k , 0x100000 per buffer */
		  /* 4096x2304 , 0x120000 per buffer */
		  .buf_size = MAX_ONE_MV_BUFFER_SIZE_8K_TM2REVB * FRAME_BUFFERS,
		},
#endif
		.rpm = {
		   .buf_size = 0x80*2,
		},
		.lmem = {
			.buf_size = 0x400 * 2,
		}

	}
};

/* AUX DATA Process */
static u32 init_aux_size;
static int aux_data_is_avaible(struct AV1HW_s *hw)
{
	u32 reg_val;

	reg_val = READ_VREG(HEVC_AUX_DATA_SIZE);
	if (reg_val != 0 && reg_val != init_aux_size)
		return 1;
	else
		return 0;
}

static void config_aux_buf(struct AV1HW_s *hw)
{
	WRITE_VREG(HEVC_AUX_ADR, hw->aux_phy_addr);
	init_aux_size = ((hw->prefix_aux_size >> 4) << 16) |
		(hw->suffix_aux_size >> 4);
	WRITE_VREG(HEVC_AUX_DATA_SIZE, init_aux_size);
}

/*
* dv_meta_flag: 1, dolby meta (T35) only; 2, not include dolby meta (T35)
*/
static void set_aux_data(struct AV1HW_s *hw,
	    char **aux_data_buf, int *aux_data_size,
	unsigned char suffix_flag,
	unsigned char dv_meta_flag)
{
	int i;
	unsigned short *aux_adr;
	unsigned int size_reg_val =
		READ_VREG(HEVC_AUX_DATA_SIZE);
	unsigned int aux_count = 0;
	int aux_size = 0;
	if (0 == aux_data_is_avaible(hw))
		return;

	if (hw->aux_data_dirty ||
		hw->m_ins_flag == 0) {

		hw->aux_data_dirty = 0;
	}

	if (suffix_flag) {
		aux_adr = (unsigned short *)
			(hw->aux_addr +
			hw->prefix_aux_size);
		aux_count =
		((size_reg_val & 0xffff) << 4)
			>> 1;
		aux_size =
			hw->suffix_aux_size;
	} else {
		aux_adr =
		(unsigned short *)hw->aux_addr;
		aux_count =
		((size_reg_val >> 16) << 4)
			>> 1;
		aux_size =
			hw->prefix_aux_size;
	}
	if (debug & AV1_DEBUG_BUFMGR_MORE) {
		av1_print(hw, 0, "%s:old size %d count %d,suf %d dv_flag %d\r\n",
			__func__, *aux_data_size, aux_count, suffix_flag, dv_meta_flag);
	}
	if (aux_size > 0 && aux_count > 0) {
		int heads_size = 0;

		for (i = 0; i < aux_count; i++) {
			unsigned char tag = aux_adr[i] >> 8;
			if (tag != 0 && tag != 0xff) {
				if (dv_meta_flag == 0)
					heads_size += 8;
				else if (dv_meta_flag == 1 && tag == 0x14)
					heads_size += 8;
				else if (dv_meta_flag == 2 && tag != 0x14)
					heads_size += 8;
			}
		}

		if (*aux_data_buf) {
			unsigned char valid_tag = 0;
			unsigned char *h =
				*aux_data_buf +
				*aux_data_size;
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
						&& tag == 0x14)
						valid_tag = 1;
					else if (dv_meta_flag == 2
						&& tag != 0x14)
						valid_tag = 1;
					else
						valid_tag = 0;
					if (valid_tag && len > 0) {
						*aux_data_size +=
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
						if ((aa >> 8) == 0xff)
							padding_len++;
					}
				}
			}
			if (len > 0) {
				*aux_data_size += (len + 8);
				h[0] = (len >> 24) & 0xff;
				h[1] = (len >> 16) & 0xff;
				h[2] = (len >> 8) & 0xff;
				h[3] = (len >> 0) & 0xff;
				h[6] = (padding_len >> 8) & 0xff;
				h[7] = (padding_len) & 0xff;
			}
			if (debug & AOM_DEBUG_AUX_DATA) {
				av1_print(hw, 0, "aux: (size %d) suffix_flag %d\n",
					*aux_data_size, suffix_flag);
				for (i = 0; i < *aux_data_size; i++) {
					av1_print_cont(hw, 0, "%02x ", (*aux_data_buf)[i]);
					if (((i + 1) & 0xf) == 0)
						av1_print_cont(hw, 0, "\n");
				}
				av1_print_cont(hw, 0, "\n");
			}
		}
	}

}

static void set_dv_data(struct AV1HW_s *hw)
{
	set_aux_data(hw, &hw->dv_data_buf,
		&hw->dv_data_size, 0, 1);
}

static void set_pic_aux_data(struct AV1HW_s *hw,
	struct PIC_BUFFER_CONFIG_s *pic, unsigned char suffix_flag,
	unsigned char dv_meta_flag)
{
	if (pic == NULL)
		return;
	set_aux_data(hw, &pic->aux_data_buf,
		&pic->aux_data_size, suffix_flag, dv_meta_flag);
}

static void copy_dv_data(struct AV1HW_s *hw,
	struct PIC_BUFFER_CONFIG_s *pic)
{
	if (pic->aux_data_buf) {
		if (debug & AV1_DEBUG_BUFMGR_MORE) {
			av1_print(hw, 0, "%s: (size %d) pic index %d\n",
				__func__, hw->dv_data_size, pic->index);
		}
		memcpy(pic->aux_data_buf + pic->aux_data_size, hw->dv_data_buf, hw->dv_data_size);
		pic->aux_data_size += hw->dv_data_size;
		memset(hw->dv_data_buf, 0, hw->aux_data_size);
		hw->dv_data_size = 0;
	}
}

static void release_aux_data(struct AV1HW_s *hw,
	struct PIC_BUFFER_CONFIG_s *pic)
{
}

static void dump_aux_buf(struct AV1HW_s *hw)
{
	int i;
	unsigned short *aux_adr = (unsigned short *)hw->aux_addr;
	unsigned int aux_size = (READ_VREG(HEVC_AUX_DATA_SIZE) >> 16) << 4;

	if (hw->prefix_aux_size > 0) {
		av1_print(hw, 0, "prefix aux: (size %d)\n", aux_size);
		for (i = 0; i < (aux_size >> 1); i++) {
			av1_print_cont(hw, 0, "%04x ", *(aux_adr + i));
			if (((i + 1) & 0xf)== 0)
				av1_print_cont(hw, 0, "\n");
		}
	}
	if (hw->suffix_aux_size > 0) {
		aux_adr = (unsigned short *)(hw->aux_addr + hw->prefix_aux_size);
		aux_size = (READ_VREG(HEVC_AUX_DATA_SIZE) & 0xffff) << 4;
		av1_print(hw, 0, "suffix aux: (size %d)\n", aux_size);
		for (i = 0; i < (aux_size >> 1); i++) {
			av1_print_cont(hw, 0, "%04x ", *(aux_adr + i));
			if (((i + 1) & 0xf) == 0)
				av1_print_cont(hw, 0, "\n");
		}
	}
}

/*Losless compression body buffer size 4K per 64x32 (jt)*/
static int compute_losless_comp_body_size(int width, int height,
				uint8_t is_bit_depth_10)
{
	int     width_x64;
	int     height_x32;
	int     bsize;

	width_x64 = width + 63;
	width_x64 >>= 6;
	height_x32 = height + 31;
	height_x32 >>= 5;
	bsize = (is_bit_depth_10?4096:3200)*width_x64*height_x32;
	if (debug & AV1_DEBUG_BUFMGR_MORE)
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
	if (debug & AV1_DEBUG_BUFMGR_MORE)
		pr_info("%s(%d,%d)=>%d\n",
			__func__, width, height,
			hsize);

	return  hsize;
}

#ifdef AOM_AV1_MMU_DW
static int compute_losless_comp_body_size_dw(int width, int height,
	uint8_t is_bit_depth_10)
{
	return compute_losless_comp_body_size(width, height, is_bit_depth_10);
}

/* Losless compression header buffer size 32bytes per 128x64 (jt)*/
static int compute_losless_comp_header_size_dw(int width, int height)
{
	return compute_losless_comp_header_size(width, height);
}
#endif

static void init_buff_spec(struct AV1HW_s *hw,
	struct BuffInfo_s *buf_spec)
{
	void *mem_start_virt;

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
	buf_spec->seg_map.buf_start =
		WORKBUF_ALIGN(buf_spec->vps.buf_start + buf_spec->vps.buf_size);
	buf_spec->daala_top.buf_start =
		WORKBUF_ALIGN(buf_spec->seg_map.buf_start + buf_spec->seg_map.buf_size);
	buf_spec->sao_up.buf_start =
		WORKBUF_ALIGN(buf_spec->daala_top.buf_start + buf_spec->daala_top.buf_size);
	buf_spec->swap_buf.buf_start =
		WORKBUF_ALIGN(buf_spec->sao_up.buf_start + buf_spec->sao_up.buf_size);
	buf_spec->cdf_buf.buf_start =
		WORKBUF_ALIGN(buf_spec->swap_buf.buf_start + buf_spec->swap_buf.buf_size);
	buf_spec->gmc_buf.buf_start =
		WORKBUF_ALIGN(buf_spec->cdf_buf.buf_start + buf_spec->cdf_buf.buf_size);
	buf_spec->scalelut.buf_start =
		WORKBUF_ALIGN(buf_spec->gmc_buf.buf_start + buf_spec->gmc_buf.buf_size);
	buf_spec->dblk_para.buf_start =
		WORKBUF_ALIGN(buf_spec->scalelut.buf_start + buf_spec->scalelut.buf_size);
	buf_spec->dblk_data.buf_start =
		WORKBUF_ALIGN(buf_spec->dblk_para.buf_start + buf_spec->dblk_para.buf_size);
	buf_spec->cdef_data.buf_start =
		WORKBUF_ALIGN(buf_spec->dblk_data.buf_start + buf_spec->dblk_data.buf_size);
	buf_spec->ups_data.buf_start =
		WORKBUF_ALIGN(buf_spec->cdef_data.buf_start + buf_spec->cdef_data.buf_size);
	buf_spec->fgs_table.buf_start =
		WORKBUF_ALIGN(buf_spec->ups_data.buf_start + buf_spec->ups_data.buf_size);
#ifdef AOM_AV1_MMU
	buf_spec->mmu_vbh.buf_start =
		WORKBUF_ALIGN(buf_spec->fgs_table.buf_start + buf_spec->fgs_table.buf_size);
	buf_spec->cm_header.buf_start =
		WORKBUF_ALIGN(buf_spec->mmu_vbh.buf_start + buf_spec->mmu_vbh.buf_size);
#ifdef AOM_AV1_MMU_DW
	buf_spec->mmu_vbh_dw.buf_start =
		WORKBUF_ALIGN(buf_spec->cm_header.buf_start + buf_spec->cm_header.buf_size);
	buf_spec->cm_header_dw.buf_start =
		WORKBUF_ALIGN(buf_spec->mmu_vbh_dw.buf_start + buf_spec->mmu_vbh_dw.buf_size);
	buf_spec->mpred_above.buf_start =
		WORKBUF_ALIGN(buf_spec->cm_header_dw.buf_start + buf_spec->cm_header_dw.buf_size);
#else
	buf_spec->mpred_above.buf_start =
		WORKBUF_ALIGN(buf_spec->cm_header.buf_start + buf_spec->cm_header.buf_size);
#endif
#else
	buf_spec->mpred_above.buf_start =
		WORKBUF_ALIGN(buf_spec->fgs_table.buf_start + buf_spec->fgs_table.buf_size);
#endif
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

	if (!hw)
		return;

	if (!vdec_secure(hw_to_vdec(hw))) {
		mem_start_virt =
			codec_mm_phys_to_virt(buf_spec->dblk_para.buf_start);
		if (mem_start_virt) {
			memset(mem_start_virt, 0,
				buf_spec->dblk_para.buf_size);
			codec_mm_dma_flush(mem_start_virt,
				buf_spec->dblk_para.buf_size,
				DMA_TO_DEVICE);
		} else {
			mem_start_virt = codec_mm_vmap(
				buf_spec->dblk_para.buf_start,
				buf_spec->dblk_para.buf_size);
			if (mem_start_virt) {
				memset(mem_start_virt, 0,
					buf_spec->dblk_para.buf_size);
				codec_mm_dma_flush(mem_start_virt,
					buf_spec->dblk_para.buf_size,
					DMA_TO_DEVICE);
				codec_mm_unmap_phyaddr(mem_start_virt);
			} else {
				/*not virt for tvp playing,
				may need clear on ucode.*/
				pr_err("mem_start_virt failed\n");
			}
		}
	}

	if (debug) {
		pr_info("%s workspace (%x %x) size = %x\n", __func__,
			   buf_spec->start_adr, buf_spec->end_adr,
			   buf_spec->end_adr - buf_spec->start_adr);
	}

	if (debug) {
		pr_info("ipp.buf_start    		 :%x\n",
			   buf_spec->ipp.buf_start);
		pr_info("sao_abv.buf_start    	  :%x\n",
			   buf_spec->sao_abv.buf_start);
		pr_info("sao_vb.buf_start    	  :%x\n",
			   buf_spec->sao_vb.buf_start);
		pr_info("short_term_rps.buf_start  :%x\n",
			   buf_spec->short_term_rps.buf_start);
		pr_info("vps.buf_start    		 :%x\n",
			   buf_spec->vps.buf_start);
		pr_info("seg_map.buf_start    	  :%x\n",
			   buf_spec->seg_map.buf_start);
		pr_info("daala_top.buf_start    	  :%x\n",
			   buf_spec->daala_top.buf_start);
		pr_info("swap_buf.buf_start    	:%x\n",
			   buf_spec->swap_buf.buf_start);
		pr_info("cdf_buf.buf_start    	:%x\n",
			   buf_spec->cdf_buf.buf_start);
		pr_info("gmc_buf.buf_start    	:%x\n",
			   buf_spec->gmc_buf.buf_start);
		pr_info("scalelut.buf_start    	:%x\n",
			   buf_spec->scalelut.buf_start);
		pr_info("dblk_para.buf_start       :%x\n",
			   buf_spec->dblk_para.buf_start);
		pr_info("dblk_data.buf_start       :%x\n",
			   buf_spec->dblk_data.buf_start);
		pr_info("cdef_data.buf_start       :%x\n",
				buf_spec->cdef_data.buf_start);
		pr_info("ups_data.buf_start       :%x\n",
				buf_spec->ups_data.buf_start);

#ifdef AOM_AV1_MMU
		pr_info("mmu_vbh.buf_start     :%x\n",
			buf_spec->mmu_vbh.buf_start);
#endif
		pr_info("mpred_above.buf_start     :%x\n",
			   buf_spec->mpred_above.buf_start);
#ifdef MV_USE_FIXED_BUF
		pr_info("mpred_mv.buf_start    	:%x\n",
			   buf_spec->mpred_mv.buf_start);
#endif
		if ((debug & AOM_AV1_DEBUG_SEND_PARAM_WITH_REG) == 0) {
			pr_info("rpm.buf_start    		 :%x\n",
				   buf_spec->rpm.buf_start);
		}
	}
}

static void uninit_mmu_buffers(struct AV1HW_s *hw)
{
#ifndef MV_USE_FIXED_BUF
	dealloc_mv_bufs(hw);
#endif
	if (hw->mmu_box)
		decoder_mmu_box_free(hw->mmu_box);
	hw->mmu_box = NULL;

#ifdef AOM_AV1_MMU_DW
	if (hw->mmu_box_dw)
		decoder_mmu_box_free(hw->mmu_box_dw);
	hw->mmu_box_dw = NULL;
#endif
	if (hw->bmmu_box)
		decoder_bmmu_box_free(hw->bmmu_box);
	hw->bmmu_box = NULL;
}

static int calc_luc_quantity(int lcu_size, u32 w, u32 h)
{
	int pic_width_64 = (w + 63) & (~0x3f);
	int pic_height_32 = (h + 31) & (~0x1f);
	int pic_width_lcu  = (pic_width_64 % lcu_size) ?
		pic_width_64 / lcu_size  + 1 : pic_width_64 / lcu_size;
	int pic_height_lcu = (pic_height_32 % lcu_size) ?
		pic_height_32 / lcu_size + 1 : pic_height_32 / lcu_size;

	return pic_width_lcu * pic_height_lcu;
}

/* return in MB */
static int av1_max_mmu_buf_size(int max_w, int max_h)
{
	int buf_size = 48;

	if ((max_w * max_h > 1280*736) &&
		(max_w * max_h <= 1920*1088)) {
		buf_size = 12;
	} else if ((max_w * max_h > 0) &&
		(max_w * max_h <= 1280*736)) {
		buf_size = 4;
	}

	return buf_size;
}

static int av1_get_header_size(int w, int h)
{
	w = ALIGN(w, 64);
	h = ALIGN(h, 64);

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
		IS_8K_SIZE(w, h))
		return MMU_COMPRESS_HEADER_SIZE_8K;
	if (IS_4K_SIZE(w, h))
		return MMU_COMPRESS_HEADER_SIZE_4K;

	return MMU_COMPRESS_HEADER_SIZE_1080P;
}

static void av1_put_video_frame(void *vdec_ctx, struct vframe_s *vf)
{
	vav1_vf_put(vf, vdec_ctx);
}

static void av1_get_video_frame(void *vdec_ctx, struct vframe_s **vf)
{
	*vf = vav1_vf_get(vdec_ctx);
}

static struct task_ops_s task_dec_ops = {
	.type		= TASK_TYPE_DEC,
	.get_vframe	= av1_get_video_frame,
	.put_vframe	= av1_put_video_frame,
};

static int v4l_alloc_and_config_pic(struct AV1HW_s *hw,
	struct PIC_BUFFER_CONFIG_s *pic)
{
	int ret = -1;
	int i = pic->index;
	int dw_mode = get_double_write_mode_init(hw);
	int lcu_total = calc_luc_quantity(hw->current_lcu_size,
		hw->frame_width, hw->frame_height);
#ifdef MV_USE_FIXED_BUF
	u32 mpred_mv_end = hw->work_space_buf->mpred_mv.buf_start +
		hw->work_space_buf->mpred_mv.buf_size;
	int32_t mv_buffer_size = hw->max_one_mv_buffer_size;
#endif
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)hw->v4l2_ctx;
	struct vdec_v4l2_buffer *fb = NULL;

	if (i < 0)
		return ret;

	ret = ctx->fb_ops.alloc(&ctx->fb_ops, hw->fb_token, &fb, AML_FB_REQ_DEC);
	if (ret < 0) {
		av1_print(hw, 0, "[%d] AV1 get buffer fail.\n", ctx->id);
		return ret;
	}

	fb->task->attach(fb->task, &task_dec_ops, hw);
	fb->status = FB_ST_DECODER;

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
		(get_double_write_mode(hw) != 0x10)) {
		struct internal_comp_buf *ibuf = v4lfb_to_icomp_buf(hw, fb);

		hw->m_BUF[i].header_addr = ibuf->header_addr;
		if (debug & AV1_DEBUG_BUFMGR_MORE) {
			pr_info("MMU header_adr %d: %ld\n",
				i, hw->m_BUF[i].header_addr);
		}
	}

	if (get_double_write_mode(hw) & 0x20) {
		struct internal_comp_buf *ibuf = v4lfb_to_icomp_buf(hw, fb);

		hw->m_BUF[i].header_dw_addr = ibuf->header_dw_addr;
		if (debug & AV1_DEBUG_BUFMGR_MORE) {
			pr_info("MMU header_adr %d: %ld\n",
				i, hw->m_BUF[i].header_addr);
		}
	}
	pic->repeat_pic = NULL;
	pic->repeat_count = 0;
#ifdef MV_USE_FIXED_BUF
	if ((hw->work_space_buf->mpred_mv.buf_start +
		((i + 1) * mv_buffer_size))
		<= mpred_mv_end) {
#endif
		hw->m_BUF[i].v4l_ref_buf_addr = (ulong)fb;
		pic->cma_alloc_addr = fb->m.mem[0].addr;
		if (fb->num_planes == 1) {
			hw->m_BUF[i].start_adr = fb->m.mem[0].addr;
			hw->m_BUF[i].luma_size = fb->m.mem[0].offset;
			hw->m_BUF[i].size = fb->m.mem[0].size;
			fb->m.mem[0].bytes_used = fb->m.mem[0].size;
			pic->dw_y_adr = hw->m_BUF[i].start_adr;
			pic->dw_u_v_adr = pic->dw_y_adr + hw->m_BUF[i].luma_size;
		} else if (fb->num_planes == 2) {
			hw->m_BUF[i].start_adr = fb->m.mem[0].addr;
			hw->m_BUF[i].size = fb->m.mem[0].size;
			hw->m_BUF[i].chroma_addr = fb->m.mem[1].addr;
			hw->m_BUF[i].chroma_size = fb->m.mem[1].size;
			fb->m.mem[0].bytes_used = fb->m.mem[0].size;
			fb->m.mem[1].bytes_used = fb->m.mem[1].size;
			pic->dw_y_adr = hw->m_BUF[i].start_adr;
			pic->dw_u_v_adr = hw->m_BUF[i].chroma_addr;
		}

		/* config frame buffer */
		if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
			(get_double_write_mode(hw) != 0x10)) {
			pic->header_adr = hw->m_BUF[i].header_addr;
		}

		if (get_double_write_mode(hw) & 0x20) {
			pic->header_dw_adr = hw->m_BUF[i].header_dw_addr;
		}

		pic->BUF_index		= i;
		pic->lcu_total		= lcu_total;
		pic->mc_canvas_y	= pic->index;
		pic->mc_canvas_u_v	= pic->index;

		if (dw_mode & 0x10) {
			pic->mc_canvas_y = (pic->index << 1);
			pic->mc_canvas_u_v = (pic->index << 1) + 1;
		}

#ifdef MV_USE_FIXED_BUF
		pic->mpred_mv_wr_start_addr =
			hw->work_space_buf->mpred_mv.buf_start +
			(pic->index * mv_buffer_size);
#endif

#ifdef DUMP_FILMGRAIN
		if (pic->index == fg_dump_index) {
			pic->fgs_table_adr = hw->fg_phy_addr;
			pr_info("set buffer %d film grain table 0x%x\n",
				pic->index, pic->fgs_table_adr);
		} else {
#endif
			if (hw->assit_task.use_sfgs) {
				pic->sfgs_table_phy = hw->fg_phy_addr + (pic->index * FGS_TABLE_SIZE);
				pic->sfgs_table_ptr = hw->fg_ptr + (pic->index * FGS_TABLE_SIZE);
			}
			pic->fgs_table_adr = hw->work_space_buf->fgs_table.buf_start +
				(pic->index * FGS_TABLE_SIZE);
		}

		if (debug) {
			pr_info("%s index %d BUF_index %d ",
				__func__, pic->index,
				pic->BUF_index);
			pr_info("comp_body_size %x comp_buf_size %x ",
				pic->comp_body_size,
				pic->buf_size);
			pr_info("mpred_mv_wr_start_adr %d\n",
				pic->mpred_mv_wr_start_addr);
			pr_info("dw_y_adr %d, pic_config->dw_u_v_adr =%d\n",
				pic->dw_y_adr,
				pic->dw_u_v_adr);
		}
#ifdef MV_USE_FIXED_BUF
	}
#endif
	return ret;
}

static int vav1_frame_mmu_map_size(struct AV1HW_s *hw)
{
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
		IS_8K_SIZE(hw->max_pic_w, hw->max_pic_h))
		return (MAX_FRAME_8K_NUM * 4);

	return (MAX_FRAME_4K_NUM * 4);
}

#ifdef AOM_AV1_MMU_DW
static int vaom_dw_frame_mmu_map_size(struct AV1HW_s *hw)
{
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
		IS_8K_SIZE(hw->max_pic_w, hw->max_pic_h))
		return (MAX_FRAME_8K_NUM * 4);

	return (MAX_FRAME_4K_NUM * 4);
}
#endif

static void init_pic_list(struct AV1HW_s *hw)
{
	int i;
	struct AV1_Common_s *cm = &hw->common;
	struct PIC_BUFFER_CONFIG_s *pic_config;
	struct vdec_s *vdec = hw_to_vdec(hw);

	for (i = 0; i < hw->used_buf_num; i++) {
		pic_config = &cm->buffer_pool->frame_bufs[i].buf;
		pic_config->index = i;
		pic_config->BUF_index = -1;
		pic_config->mv_buf_index = -1;
		if (vdec->parallel_dec == 1) {
			pic_config->y_canvas_index = -1;
			pic_config->uv_canvas_index = -1;
		}
		pic_config->y_crop_width = hw->init_pic_w;
		pic_config->y_crop_height = hw->init_pic_h;
		pic_config->double_write_mode = get_double_write_mode(hw);
		hw->buffer_wrap[i] = i;
	}

	for (; i < hw->used_buf_num; i++) {
		pic_config = &cm->buffer_pool->frame_bufs[i].buf;
		pic_config->index = -1;
		pic_config->BUF_index = -1;
		pic_config->mv_buf_index = -1;
		hw->buffer_wrap[i] = i;
		if (vdec->parallel_dec == 1) {
			pic_config->y_canvas_index = -1;
			pic_config->uv_canvas_index = -1;
		}
	}
	av1_print(hw, AV1_DEBUG_BUFMGR, "%s ok, used_buf_num = %d\n",
		__func__, hw->used_buf_num);

}

static void init_pic_list_hw(struct AV1HW_s *hw)
{
	int i;
	struct AV1_Common_s *cm = &hw->common;
	struct PIC_BUFFER_CONFIG_s *pic_config;
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR,
		(0x1 << 1) | (0x1 << 2));

	for (i = 0; i < hw->used_buf_num; i++) {
		pic_config = &cm->buffer_pool->frame_bufs[i].buf;
		if (pic_config->index < 0)
			break;

		if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
			(get_double_write_mode(hw) != 0x10)) {
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
				pic_config->header_adr >> 5);
		} else {
			WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
				pic_config->dw_y_adr >> 5);
		}
#ifndef LOSLESS_COMPRESS_MODE
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
			pic_config->dw_u_v_adr >> 5);
#else
		if (pic_config->double_write_mode & 0x10) {
				WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,
				pic_config->dw_u_v_adr >> 5);
		}
#endif
	}
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x1);

#ifdef CHANGE_REMOVED
	/*Zero out canvas registers in IPP -- avoid simulation X*/
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(0 << 8) | (0 << 1) | 1);
#else
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(1 << 8) | (0 << 1) | 1);
#endif
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);
}


static void dump_pic_list(struct AV1HW_s *hw)
{
	struct AV1_Common_s *const cm = &hw->common;
	struct PIC_BUFFER_CONFIG_s *pic_config;
	int i;
	for (i = 0; i < FRAME_BUFFERS; i++) {
		pic_config = &cm->buffer_pool->frame_bufs[i].buf;
		av1_print(hw, 0,
			"Buf(%d) index %d mv_buf_index %d ref_count %d vf_ref %d repeat_count %d dec_idx %d slice_type %d w/h %d/%d adr%ld\n",
			i,
			pic_config->index,
#ifndef MV_USE_FIXED_BUF
			pic_config->mv_buf_index,
#else
			-1,
#endif
			cm->buffer_pool->frame_bufs[i].ref_count,
			pic_config->vf_ref,
			pic_config->repeat_count,
			pic_config->decode_idx,
			pic_config->slice_type,
			pic_config->y_crop_width,
			pic_config->y_crop_height,
			pic_config->cma_alloc_addr
			);
	}
	return;
}

void av1_release_buf(AV1Decoder *pbi, RefCntBuffer *const buf)
{
}

void av1_release_bufs(struct AV1HW_s *hw)
{
	AV1_COMMON *cm = &hw->common;
	RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
	int i;

	for (i = 0; i < FRAME_BUFFERS; ++i) {
		if (frame_bufs[i].buf.vf_ref == 0 &&
			frame_bufs[i].ref_count == 0 &&
			frame_bufs[i].buf.index >= 0) {
			if (frame_bufs[i].buf.aux_data_buf)
				release_aux_data(hw, &frame_bufs[i].buf);
		}
	}
}

#ifdef DEBUG_CMD
static void d_fill_zero(struct AV1HW_s *hw, unsigned int phyadr, int size)
{
	WRITE_VREG(HEVC_DBG_LOG_ADR, phyadr);
	WRITE_VREG(DEBUG_REG1,
		0x20000000 | size);
	debug_cmd_wait_count = 0;
	debug_cmd_wait_type = 1;
	while ((READ_VREG(DEBUG_REG1) & 0x1) == 0
		&& debug_cmd_wait_count < 0x7fffffff) {
		debug_cmd_wait_count++;
	}

	WRITE_VREG(DEBUG_REG1, 0);
	debug_cmd_wait_type = 0;
}

static void d_dump(struct AV1HW_s *hw, unsigned int phyadr, int size,
	struct file *fp, loff_t *wr_off)
{

	int jj;
	unsigned char *data = (unsigned char *)
		(hw->ucode_log_addr);
	WRITE_VREG(HEVC_DBG_LOG_ADR, hw->ucode_log_phy_addr);

	WRITE_VREG(HEVC_D_ADR, phyadr);
	WRITE_VREG(DEBUG_REG1,
		0x10000000 | size);

	debug_cmd_wait_count = 0;
	debug_cmd_wait_type = 3;
	while ((READ_VREG(DEBUG_REG1) & 0x1) == 0
		&& debug_cmd_wait_count < 0x7fffffff) {
		debug_cmd_wait_count++;
	}

	if (fp) {
		vfs_write(fp, data,
			size, wr_off);

	} else {
		for (jj = 0; jj < size; jj++) {
			if ((jj & 0xf) == 0)
				av1_print(hw, 0,
					"%06x:", jj);
			av1_print_cont(hw, 0,
				"%02x ", data[jj]);
			if (((jj + 1) & 0xf) == 0)
				av1_print_cont(hw, 0,
					"\n");
		}
		av1_print(hw, 0, "\n");
	}

	WRITE_VREG(DEBUG_REG1, 0);
	debug_cmd_wait_type = 0;

}

static void mv_buffer_fill_zero(struct AV1HW_s *hw, struct PIC_BUFFER_CONFIG_s *pic_config)
{
	pr_info("fill dummy data pic index %d colocate addreses %x size %x\n",
		pic_config->index, pic_config->mpred_mv_wr_start_addr,
		hw->m_mv_BUF[pic_config->mv_buf_index].size);
	d_fill_zero(hw, pic_config->mpred_mv_wr_start_addr,
		hw->m_mv_BUF[pic_config->mv_buf_index].size);
}

static void dump_mv_buffer(struct AV1HW_s *hw, struct PIC_BUFFER_CONFIG_s *pic_config)
{

	unsigned int adr, size;
	unsigned int adr_end = pic_config->mpred_mv_wr_start_addr +
		hw->m_mv_BUF[pic_config->mv_buf_index].size;
	mm_segment_t old_fs;
	loff_t off = 0;
	int mode = O_CREAT | O_WRONLY | O_TRUNC;
	char file[64];
	struct file *fp;
	sprintf(&file[0], "/data/tmp/colocate%d", hw->frame_count-1);
	fp = filp_open(file, mode, 0666);
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	for (adr = pic_config->mpred_mv_wr_start_addr;
		adr < adr_end;
		adr += UCODE_LOG_BUF_SIZE) {
		size = UCODE_LOG_BUF_SIZE;
		if (size > (adr_end - adr))
			size = adr_end - adr;
		pr_info("dump pic index %d colocate addreses %x size %x\n",
			pic_config->index, adr, size);
		d_dump(hw, adr, size, fp, &off);
	}
	set_fs(old_fs);
	vfs_fsync(fp, 0);

	filp_close(fp, current->files);
}

#endif

static int config_pic_size(struct AV1HW_s *hw, unsigned short bit_depth)
{
	uint32_t data32;
	struct AV1_Common_s *cm = &hw->common;
	struct PIC_BUFFER_CONFIG_s *cur_pic_config = &cm->cur_frame->buf;
	int losless_comp_header_size, losless_comp_body_size;
#ifdef AOM_AV1_MMU_DW
	int losless_comp_header_size_dw, losless_comp_body_size_dw;
#endif
    av1_print(hw, AOM_DEBUG_HW_MORE,
		"	#### config_pic_size ####, bit_depth = %d\n", bit_depth);

	frame_width = cur_pic_config->y_crop_width;
	frame_height = cur_pic_config->y_crop_height;
	cur_pic_config->bit_depth = bit_depth;
	cur_pic_config->double_write_mode = get_double_write_mode(hw);

	WRITE_VREG(HEVC_PARSER_PICTURE_SIZE,
		(frame_height << 16) | frame_width);
#ifdef DUAL_DECODE
#else
	WRITE_VREG(HEVC_ASSIST_PIC_SIZE_FB_READ,
		(frame_height << 16) | frame_width);
#endif

#ifdef AOM_AV1_MMU_DW
    losless_comp_header_size_dw =
		compute_losless_comp_header_size_dw(frame_width, frame_height);
    losless_comp_body_size_dw =
		compute_losless_comp_body_size_dw(frame_width, frame_height,
			(bit_depth == AOM_BITS_10));
#endif

    losless_comp_header_size =
		compute_losless_comp_header_size
		(frame_width, frame_height);
    losless_comp_body_size =
		compute_losless_comp_body_size(frame_width,
		frame_height, (bit_depth == AOM_BITS_10));

	cur_pic_config->comp_body_size = losless_comp_body_size;

	av1_print(hw, AOM_DEBUG_HW_MORE,
		"%s: width %d height %d depth %d head_size 0x%x body_size 0x%x\r\n",
		__func__, frame_width, frame_height, bit_depth,
		losless_comp_header_size, losless_comp_body_size);
#ifdef LOSLESS_COMPRESS_MODE
	data32 = READ_VREG(HEVC_SAO_CTRL5);
	if (bit_depth == AOM_BITS_10)
		data32 &= ~(1<<9);
	else
		data32 |= (1<<9);

	WRITE_VREG(HEVC_SAO_CTRL5, data32);

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
		(get_double_write_mode(hw) != 0x10)) {
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1,(0x1<< 4)); // bit[4] : paged_mem_mode
	} else {
		if (bit_depth == AOM_BITS_10)
			WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0<<3)); // bit[3] smem mdoe
		else
			WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (1<<3)); // bit[3] smem mdoe
	}
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2,
		(losless_comp_body_size >> 5));
	WRITE_VREG(HEVC_CM_BODY_LENGTH,
		losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET,
		losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH,
		losless_comp_header_size);

	if (get_double_write_mode(hw) & 0x10)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);

#else
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1,0x1 << 31);
#endif
#ifdef AOM_AV1_MMU_DW
    if (get_double_write_mode(hw) & 0x20) {
		WRITE_VREG(HEVC_CM_BODY_LENGTH2, losless_comp_body_size_dw);
		WRITE_VREG(HEVC_CM_HEADER_OFFSET2, losless_comp_body_size_dw);
		WRITE_VREG(HEVC_CM_HEADER_LENGTH2, losless_comp_header_size_dw);
	}
#endif
    return 0;

}

static int config_mc_buffer(struct AV1HW_s *hw, unsigned short bit_depth, unsigned char inter_flag)
{
	int32_t i;
	AV1_COMMON *cm = &hw->common;
	PIC_BUFFER_CONFIG* cur_pic_config = &cm->cur_frame->buf;
	uint8_t scale_enable = 0;

	av1_print(hw, AOM_DEBUG_HW_MORE,
		" #### config_mc_buffer %s ####\n",
		inter_flag ? "inter" : "intra");

#ifdef DEBUG_PRINT
	if (debug&AOM_AV1_DEBUG_BUFMGR)
		av1_print(hw, AOM_DEBUG_HW_MORE,
			"config_mc_buffer entered .....\n");
#endif

	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
		(0 << 8) | (0<<1) | 1);
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
		(cur_pic_config->order_hint<<24) |
		(cur_pic_config->mc_canvas_u_v<<16) |
		(cur_pic_config->mc_canvas_u_v<<8)|
		cur_pic_config->mc_canvas_y);
	for (i = LAST_FRAME; i <= ALTREF_FRAME; i++) {
		PIC_BUFFER_CONFIG *pic_config; //cm->frame_refs[i].buf;
		if (inter_flag)
			pic_config = av1_get_ref_frame_spec_buf(cm, i);
		else
			pic_config = cur_pic_config;
		if (pic_config) {
			WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
				(pic_config->order_hint<<24) |
				(pic_config->mc_canvas_u_v<<16) |
				(pic_config->mc_canvas_u_v<<8) |
				pic_config->mc_canvas_y);
			if (inter_flag)
				av1_print(hw, AOM_DEBUG_HW_MORE,
				"refid 0x%x mc_canvas_u_v 0x%x mc_canvas_y 0x%x order_hint 0x%x\n",
				i, pic_config->mc_canvas_u_v,
				pic_config->mc_canvas_y, pic_config->order_hint);
		} else {
			WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);
		}
	}

	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
		(16 << 8) | (0 << 1) | 1);
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
		(cur_pic_config->order_hint << 24) |
		(cur_pic_config->mc_canvas_u_v << 16) |
		(cur_pic_config->mc_canvas_u_v << 8) |
		cur_pic_config->mc_canvas_y);
	for (i = LAST_FRAME; i <= ALTREF_FRAME; i++) {
		PIC_BUFFER_CONFIG *pic_config;
		if (inter_flag)
			pic_config = av1_get_ref_frame_spec_buf(cm, i);
		else
			pic_config = cur_pic_config;

		if (pic_config) {
			WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
			(pic_config->order_hint << 24)|
			(pic_config->mc_canvas_u_v << 16) |
			(pic_config->mc_canvas_u_v << 8) |
			pic_config->mc_canvas_y);
		} else {
			WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);
		}
	}

	WRITE_VREG(AV1D_MPP_REFINFO_TBL_ACCCONFIG,
		(0x1 << 2) | (0x0 <<3)); // auto_inc start index:0 field:0
	for (i = 0; i <= ALTREF_FRAME; i++) {
		int32_t ref_pic_body_size;
		struct scale_factors * sf = NULL;
		PIC_BUFFER_CONFIG *pic_config;

		if (inter_flag && i >= LAST_FRAME)
			pic_config = av1_get_ref_frame_spec_buf(cm, i);
		else
			pic_config = cur_pic_config;

		if (pic_config) {
			ref_pic_body_size =
				compute_losless_comp_body_size(pic_config->y_crop_width,
			pic_config->y_crop_height, (bit_depth == AOM_BITS_10));

			WRITE_VREG(AV1D_MPP_REFINFO_DATA, pic_config->y_crop_width);
			WRITE_VREG(AV1D_MPP_REFINFO_DATA, pic_config->y_crop_height);
			if (inter_flag && i >= LAST_FRAME) {
				av1_print(hw, AOM_DEBUG_HW_MORE,
				"refid %d: ref width/height(%d,%d), cur width/height(%d,%d) ref_pic_body_size 0x%x\n",
				i, pic_config->y_crop_width, pic_config->y_crop_height,
				cur_pic_config->y_crop_width, cur_pic_config->y_crop_height,
				ref_pic_body_size);
			}
		} else {
			ref_pic_body_size = 0;
			WRITE_VREG(AV1D_MPP_REFINFO_DATA, 0);
			WRITE_VREG(AV1D_MPP_REFINFO_DATA, 0);
		}

		if (inter_flag && i >= LAST_FRAME)
			sf = av1_get_ref_scale_factors(cm, i);

		if ((sf != NULL) && av1_is_scaled(sf)) {
			scale_enable |= (1 << i);
		}

		if (sf) {
			WRITE_VREG(AV1D_MPP_REFINFO_DATA, sf->x_scale_fp);
			WRITE_VREG(AV1D_MPP_REFINFO_DATA, sf->y_scale_fp);

			av1_print(hw, AOM_DEBUG_HW_MORE,
			"x_scale_fp %d, y_scale_fp %d\n",
			sf->x_scale_fp, sf->y_scale_fp);
		} else {
			WRITE_VREG(AV1D_MPP_REFINFO_DATA, REF_NO_SCALE); //1<<14
			WRITE_VREG(AV1D_MPP_REFINFO_DATA, REF_NO_SCALE);
		}
		if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
			(get_double_write_mode(hw) != 0x10)) {
			WRITE_VREG(AV1D_MPP_REFINFO_DATA, 0);
		} else
			WRITE_VREG(AV1D_MPP_REFINFO_DATA, ref_pic_body_size >> 5);
	}
	WRITE_VREG(AV1D_MPP_REF_SCALE_ENBL, scale_enable);
	WRITE_VREG(PARSER_REF_SCALE_ENBL, scale_enable);
	av1_print(hw, AOM_DEBUG_HW_MORE,
		"WRITE_VREG(PARSER_REF_SCALE_ENBL, 0x%x)\n",
		scale_enable);
	return 0;
}

static void clear_mpred_hw(struct AV1HW_s *hw)
{
	unsigned int data32;

	data32 = READ_VREG(HEVC_MPRED_CTRL4);
	data32 &=  (~(1 << 6));
	WRITE_VREG(HEVC_MPRED_CTRL4, data32);
}

static void config_mpred_hw(struct AV1HW_s *hw, unsigned char inter_flag)
{
	AV1_COMMON *cm = &hw->common;
	PIC_BUFFER_CONFIG *cur_pic_config = &cm->cur_frame->buf;
	int i, j, pos, reg_i;
	int mv_cal_tpl_count = 0;
	unsigned int mv_ref_id[MFMV_STACK_SIZE] = {0, 0, 0};
	unsigned ref_offset_reg[] = {
		HEVC_MPRED_L0_REF06_POC,
		HEVC_MPRED_L0_REF07_POC,
		HEVC_MPRED_L0_REF08_POC,
		HEVC_MPRED_L0_REF09_POC,
		HEVC_MPRED_L0_REF10_POC,
		HEVC_MPRED_L0_REF11_POC,
	};
	unsigned ref_buf_reg[] = {
		HEVC_MPRED_L0_REF03_POC,
		HEVC_MPRED_L0_REF04_POC,
		HEVC_MPRED_L0_REF05_POC
	};
	unsigned ref_offset_val[6] =
		{0, 0, 0, 0, 0, 0};
	unsigned ref_buf_val[3] = {0, 0, 0};

	uint32_t data32;
	int32_t  mpred_curr_lcu_x;
	int32_t  mpred_curr_lcu_y;

	av1_print(hw, AOM_DEBUG_HW_MORE,
	" #### config_mpred_hw ####\n");

	data32 = READ_VREG(HEVC_MPRED_CURR_LCU);
	mpred_curr_lcu_x   =data32 & 0xffff;
	mpred_curr_lcu_y   =(data32>>16) & 0xffff;

	av1_print(hw, AOM_DEBUG_HW_MORE,
		"cur pic index %d\n", cur_pic_config->index);

#ifdef CO_MV_COMPRESS
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_T5D) {
		WRITE_VREG(HEVC_MPRED_CTRL3,0x10151015); // 'd10, 'd21 for AV1
	} else {
		WRITE_VREG(HEVC_MPRED_CTRL3,0x13151315); // 'd19, 'd21 for AV1
	}
#else
	WRITE_VREG(HEVC_MPRED_CTRL3,0x13151315); // 'd19, 'd21 for AV1
#endif
	WRITE_VREG(HEVC_MPRED_ABV_START_ADDR,
	hw->pbi->work_space_buf->mpred_above.buf_start);

	if (inter_flag) {
		data32 = READ_VREG(HEVC_MPRED_CTRL4);
		data32 &= (~(0xff << 12));

		/* HEVC_MPRED_CTRL4[bit 12] is for cm->ref_frame_sign_bias[0]
		instead of cm->ref_frame_sign_bias[LAST_FRAME] */
		for (i = 0; i <= ALTREF_FRAME; i++) {
			data32 |= ((cm->ref_frame_sign_bias[i] & 0x1) << (12 + i));
		}
		WRITE_VREG(HEVC_MPRED_CTRL4, data32);
		av1_print(hw, AOM_DEBUG_HW_MORE,
			"WRITE_VREG(HEVC_MPRED_CTRL4, 0x%x)\n", data32);
	}
	data32 = ((cm->seq_params.order_hint_info.enable_order_hint << 27) |
		(cm->seq_params.order_hint_info.order_hint_bits_minus_1 << 24) |
		(cm->cur_frame->order_hint << 16 ));
#ifdef CO_MV_COMPRESS
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_T5D) {
		data32 |= (0x10 << 8) | (0x10 << 0);
	} else {
		data32 |= (0x13 << 8) | (0x13 << 0);
	}
#else
		data32 |= (0x13 << 8) | (0x13 << 0);
#endif
	WRITE_VREG(HEVC_MPRED_L0_REF00_POC, data32);
	av1_print(hw, AOM_DEBUG_HW_MORE,
	"WRITE_VREG(HEVC_MPRED_L0_REF00_POC, 0x%x)\n", data32);

	if (inter_flag) {
		/* config ref_buf id and order hint */
		data32 = 0;
		pos = 25;
		reg_i = 0;
		for (i = ALTREF_FRAME; i >= LAST_FRAME; i--) {
				PIC_BUFFER_CONFIG *pic_config =
					av1_get_ref_frame_spec_buf(cm, i);
			if (pic_config) {
				av1_print(hw, AOM_DEBUG_HW_MORE,
					"pic_config for %d th ref: index %d, reg[%d] pos %d\n",
					i, pic_config->index, reg_i, pos);
				data32 |= ((pic_config->index < 0)? 0 : pic_config->index) << pos;
			} else
				av1_print(hw, AOM_DEBUG_HW_MORE,
				"pic_config is null for %d th ref\n", i);
			if (pos == 0) {
				ref_buf_val[reg_i] = data32;
				av1_print(hw, AOM_DEBUG_HW_MORE,
				"ref_buf_reg[%d], WRITE_VREG(0x%x, 0x%x)\n",
				reg_i, ref_buf_reg[reg_i], data32);
				reg_i++;
				data32 = 0;
				pos = 24; //for P_HEVC_MPRED_L0_REF04_POC
			} else {
				if (pos == 24)
				pos -= 8; //for P_HEVC_MPRED_L0_REF04_POC
				else
				pos -= 5; //for P_HEVC_MPRED_L0_REF03_POC
			}
		}
		for (i = ALTREF_FRAME; i >= LAST_FRAME; i--) {
			PIC_BUFFER_CONFIG *pic_config =
				av1_get_ref_frame_spec_buf(cm, i);
			if (pic_config) {
				av1_print(hw, AOM_DEBUG_HW_MORE,
					"pic_config for %d th ref: order_hint %d, reg[%d] pos %d\n",
					i, pic_config->order_hint, reg_i, pos);
				data32 |= ((pic_config->index < 0)? 0 : pic_config->order_hint) << pos;
			} else
				av1_print(hw, AOM_DEBUG_HW_MORE,
				"pic_config is null for %d th ref\n", i);
			if (pos == 0) {
				ref_buf_val[reg_i] = data32;
				av1_print(hw, AOM_DEBUG_HW_MORE,
				"ref_buf_reg[%d], WRITE_VREG(0x%x, 0x%x)\n",
				reg_i, ref_buf_reg[reg_i], data32);
				reg_i++;
				data32 = 0;
				pos = 24;
			} else
				pos -= 8;
		}
		if (pos != 24) {
			ref_buf_val[reg_i] = data32;
			av1_print(hw, AOM_DEBUG_HW_MORE,
				"ref_buf_reg[%d], WRITE_VREG(0x%x, 0x%x)\n",
			reg_i, ref_buf_reg[reg_i], data32);
		}
		/* config ref_offset */
		data32 = 0;
		pos = 24;
		mv_cal_tpl_count = 0;
		reg_i = 0;
		for (i = 0; i < cm->mv_ref_id_index; i++) {
			if (cm->mv_cal_tpl_mvs[i]) {
				mv_ref_id[mv_cal_tpl_count] = cm->mv_ref_id[i];
				mv_cal_tpl_count++;
				for (j = LAST_FRAME; j <= ALTREF_FRAME; j++) {
					/*offset can be negative*/
					unsigned char offval =
						cm->mv_ref_offset[i][j] & 0xff;
					data32 |= (offval << pos);
					if (pos == 0) {
						ref_offset_val[reg_i] = data32;
						av1_print(hw, AOM_DEBUG_HW_MORE,
						"ref_offset_reg[%d], WRITE_VREG(0x%x, 0x%x)\n",
						reg_i, ref_offset_reg[reg_i], data32);
						reg_i++;
						data32 = 0;
						pos = 24;
					} else
						pos -= 8;
				}
			}
		}
		if (pos != 24) {
			ref_offset_val[reg_i] = data32;
			av1_print(hw, AOM_DEBUG_HW_MORE,
			"ref_offset_reg[%d], WRITE_VREG(0x%x, 0x%x)\n",
			reg_i, ref_offset_reg[reg_i], data32);
		}

		data32 = ref_offset_val[5] |
			mv_cal_tpl_count | (mv_ref_id[0] << 2) |
			(mv_ref_id[1] << 5) | (mv_ref_id[2] << 8);
		ref_offset_val[5] = data32;
		av1_print(hw, AOM_DEBUG_HW_MORE,
			"WRITE_VREG(HEVC_MPRED_L0_REF11_POC 0x%x, 0x%x)\n",
			HEVC_MPRED_L0_REF11_POC, data32);
	}
	for (i = 0; i < 3; i++)
		WRITE_VREG(ref_buf_reg[i], ref_buf_val[i]);
	for (i = 0; i < 6; i++)
		WRITE_VREG(ref_offset_reg[i], ref_offset_val[i]);

	WRITE_VREG(HEVC_MPRED_MV_WR_START_ADDR,
		cur_pic_config->mpred_mv_wr_start_addr);
	WRITE_VREG(HEVC_MPRED_MV_WPTR,
		cur_pic_config->mpred_mv_wr_start_addr);

	if (inter_flag) {
		for (i = 0; i < mv_cal_tpl_count; i++) {
			PIC_BUFFER_CONFIG *pic_config =
			av1_get_ref_frame_spec_buf(cm, mv_ref_id[i]);
			if (pic_config == NULL)
				continue;
			if (i == 0) {
				WRITE_VREG(HEVC_MPRED_MV_RD_START_ADDR,
					pic_config->mpred_mv_wr_start_addr);
				WRITE_VREG(HEVC_MPRED_MV_RPTR,
					pic_config->mpred_mv_wr_start_addr);
			} else if (i == 1) {
				WRITE_VREG(HEVC_MPRED_L0_REF01_POC,
					pic_config->mpred_mv_wr_start_addr);
				WRITE_VREG(HEVC_MPRED_MV_RPTR_1,
					pic_config->mpred_mv_wr_start_addr);
			} else if (i == 2) {
				WRITE_VREG(HEVC_MPRED_L0_REF02_POC,
					pic_config->mpred_mv_wr_start_addr);
				WRITE_VREG(HEVC_MPRED_MV_RPTR_2,
					pic_config->mpred_mv_wr_start_addr);
			} else {
				av1_print(hw, AOM_DEBUG_HW_MORE,
					"%s: mv_ref_id error\n", __func__);
			}
		}
	}
	data32 = READ_VREG(HEVC_MPRED_CTRL0);
	data32 &= ~((1 << 10) | (1 << 11));
	data32 |= (1 << 10); /*write enable*/
	av1_print(hw, AOM_DEBUG_HW_MORE,
		"current_frame.frame_type=%d, cur_frame->frame_type=%d, allow_ref_frame_mvs=%d\n",
		cm->current_frame.frame_type, cm->cur_frame->frame_type,
		cm->allow_ref_frame_mvs);

	if (av1_frame_is_inter(&hw->common)) {
		if (cm->allow_ref_frame_mvs) {
			data32 |= (1 << 11); /*read enable*/
		}
	}
	av1_print(hw, AOM_DEBUG_HW_MORE,
		"WRITE_VREG(HEVC_MPRED_CTRL0 0x%x, 0x%x)\n",
		HEVC_MPRED_CTRL0, data32);
	WRITE_VREG(HEVC_MPRED_CTRL0, data32);

}

static void config_sao_hw(struct AV1HW_s *hw, union param_u *params)
{
	/*
	!!!!!!!!!!!!!!!!!!!!!!!!!TODO .... !!!!!!!!!!!
	mem_map_mode, endian, get_double_write_mode
	*/
	AV1_COMMON *cm = &hw->common;
    PIC_BUFFER_CONFIG* pic_config = &cm->cur_frame->buf;
    uint32_t data32;
    int32_t lcu_size =
		((params->p.seq_flags >> 6) & 0x1) ? 128 : 64;
    int32_t mc_buffer_size_u_v =
		pic_config->lcu_total*lcu_size*lcu_size/2;
    int32_t mc_buffer_size_u_v_h =
		(mc_buffer_size_u_v + 0xffff)>>16; //64k alignment
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;

    av1_print(hw, AOM_DEBUG_HW_MORE,
		"[test.c] #### config_sao_hw ####, lcu_size %d\n", lcu_size);
    av1_print(hw, AOM_DEBUG_HW_MORE,
		"[config_sao_hw] lcu_total : %d\n", pic_config->lcu_total);
    av1_print(hw, AOM_DEBUG_HW_MORE,
		"[config_sao_hw] mc_y_adr : 0x%x\n", pic_config->mc_y_adr);
    av1_print(hw, AOM_DEBUG_HW_MORE,
		"[config_sao_hw] mc_u_v_adr : 0x%x\n", pic_config->mc_u_v_adr);
    av1_print(hw, AOM_DEBUG_HW_MORE,
		"[config_sao_hw] header_adr : 0x%x\n", pic_config->header_adr);
#ifdef AOM_AV1_MMU_DW
    if (get_double_write_mode(hw) & 0x20)
		av1_print(hw, AOM_DEBUG_HW_MORE,
		"[config_sao_hw] header_dw_adr : 0x%x\n", pic_config->header_dw_adr);
#endif
    data32 = READ_VREG(HEVC_SAO_CTRL9) | (1 << 1);
    WRITE_VREG(HEVC_SAO_CTRL9, data32);

    data32 = READ_VREG(HEVC_SAO_CTRL5);
    data32 |= (0x1 << 14); /* av1 mode */
    data32 |= (0xff << 16); /* dw {v1,v0,h1,h0} ctrl_y_cbus */
    WRITE_VREG(HEVC_SAO_CTRL5, data32);

    WRITE_VREG(HEVC_SAO_CTRL0,
		lcu_size == 128 ? 0x7 : 0x6); /*lcu_size_log2*/
#ifdef LOSLESS_COMPRESS_MODE
    WRITE_VREG(HEVC_CM_BODY_START_ADDR, pic_config->mc_y_adr);
#ifdef AOM_AV1_MMU
    WRITE_VREG(HEVC_CM_HEADER_START_ADDR, pic_config->header_adr);
#endif
#ifdef AOM_AV1_MMU_DW
    if (get_double_write_mode(hw) & 0x20) {
		WRITE_VREG(HEVC_CM_HEADER_START_ADDR2, pic_config->header_dw_adr);
	}
#endif
#else
/*!LOSLESS_COMPRESS_MODE*/
    WRITE_VREG(HEVC_SAO_Y_START_ADDR, pic_config->mc_y_adr);
#endif

    av1_print(hw, AOM_DEBUG_HW_MORE,
		"[config_sao_hw] sao_body_addr:%x\n", pic_config->mc_y_adr);

#ifdef VPU_FILMGRAIN_DUMP
	// Let Microcode to increase
	// WRITE_VREG(HEVC_FGS_TABLE_START, pic_config->fgs_table_adr);
#else
    WRITE_VREG(HEVC_FGS_TABLE_START, pic_config->fgs_table_adr);
#endif
    WRITE_VREG(HEVC_FGS_TABLE_LENGTH, FGS_TABLE_SIZE * 8);
    av1_print(hw, AOM_DEBUG_HW_MORE,
		"[config_sao_hw] fgs_table adr:0x%x , length 0x%x bits\n",
		pic_config->fgs_table_adr, FGS_TABLE_SIZE * 8);

    data32 = (mc_buffer_size_u_v_h<<16)<<1;
    WRITE_VREG(HEVC_SAO_Y_LENGTH ,data32);

#ifndef LOSLESS_COMPRESS_MODE
    WRITE_VREG(HEVC_SAO_C_START_ADDR, pic_config->mc_u_v_adr);
#else
#endif

    data32 = (mc_buffer_size_u_v_h<<16);
    WRITE_VREG(HEVC_SAO_C_LENGTH  ,data32);

#ifndef LOSLESS_COMPRESS_MODE
	/* multi tile to do... */
    WRITE_VREG(HEVC_SAO_Y_WPTR, pic_config->mc_y_adr);

    WRITE_VREG(HEVC_SAO_C_WPTR, pic_config->mc_u_v_adr);
#else
    if (get_double_write_mode(hw) &&
		(get_double_write_mode(hw) & 0x20) == 0) {
	    WRITE_VREG(HEVC_SAO_Y_START_ADDR, pic_config->dw_y_adr);
	    WRITE_VREG(HEVC_SAO_C_START_ADDR, pic_config->dw_u_v_adr);
	    WRITE_VREG(HEVC_SAO_Y_WPTR, pic_config->dw_y_adr);
	    WRITE_VREG(HEVC_SAO_C_WPTR, pic_config->dw_u_v_adr);
	} else {
		//WRITE_VREG(HEVC_SAO_Y_START_ADDR, 0xffffffff);
		//WRITE_VREG(HEVC_SAO_C_START_ADDR, 0xffffffff);
	}
#endif


#ifndef AOM_AV1_NV21
#ifdef AOM_AV1_MMU_DW
    if (get_double_write_mode(hw) & 0x20) {
	}
#endif
#endif

#ifdef AOM_AV1_NV21
#ifdef DOS_PROJECT
    data32 = READ_VREG(HEVC_SAO_CTRL1);
    data32 &= (~0x3000);
    data32 |= (hw->mem_map_mode << 12); // [13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32
    data32 &= (~0x3);
    data32 |= 0x1; // [1]:dw_disable [0]:cm_disable
    WRITE_VREG(HEVC_SAO_CTRL1, data32);

    data32 = READ_VREG(HEVC_SAO_CTRL5); // [23:22] dw_v1_ctrl [21:20] dw_v0_ctrl [19:18] dw_h1_ctrl [17:16] dw_h0_ctrl
    data32 &= ~(0xff << 16);			   // set them all 0 for AOM_AV1_NV21 (no down-scale)
    WRITE_VREG(HEVC_SAO_CTRL5, data32);

    data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
    data32 &= (~0x30);
    data32 |= (hw->mem_map_mode << 4); // [5:4]	-- address_format 00:linear 01:32x32 10:64x32
    WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#else
    // m8baby test1902
    data32 = READ_VREG(HEVC_SAO_CTRL1);
    data32 &= (~0x3000);
    data32 |= (hw->mem_map_mode << 12); // [13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32
    data32 &= (~0xff0);
	//data32 |= 0x670;  // Big-Endian per 64-bit
    data32 |= 0x880;  // Big-Endian per 64-bit
    data32 &= (~0x3);
    data32 |= 0x1; // [1]:dw_disable [0]:cm_disable
    WRITE_VREG(HEVC_SAO_CTRL1, data32);

    data32 = READ_VREG(HEVC_SAO_CTRL5); // [23:22] dw_v1_ctrl [21:20] dw_v0_ctrl [19:18] dw_h1_ctrl [17:16] dw_h0_ctrl
    data32 &= ~(0xff << 16);			   // set them all 0 for AOM_AV1_NV21 (no down-scale)
    WRITE_VREG(HEVC_SAO_CTRL5, data32);

    data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	data32 |= (hw->mem_map_mode << 4); // [5:4]	-- address_format 00:linear 01:32x32 10:64x32
	data32 &= (~0xF);
    data32 |= 0x8;	// Big-Endian per 64-bit
    WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#endif
#else
/*CHANGE_DONE nnn*/
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	data32 |= (hw->mem_map_mode << 12); /* [13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32 */
	data32 &= (~0xff0);
	/* data32 |= 0x670;  // Big-Endian per 64-bit */
#ifdef AOM_AV1_MMU_DW
	if ((get_double_write_mode(hw) & 0x20) == 0)
		data32 |= ((hw->endian >> 8) & 0xfff);	/* Big-Endian per 64-bit */
#else
	data32 |= ((hw->endian >> 8) & 0xfff);	/* Big-Endian per 64-bit */
#endif
	data32 &= (~0x3); /*[1]:dw_disable [0]:cm_disable*/
	if (get_double_write_mode(hw) == 0)
		data32 |= 0x2; /*disable double write*/
	else if (get_double_write_mode(hw) & 0x10)
		data32 |= 0x1; /*disable cm*/
	 if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) { /* >= G12A dw write control */
		unsigned int data;
		data = READ_VREG(HEVC_DBLK_CFGB);
		data &= (~0x300); /*[8]:first write enable (compress)  [9]:double write enable (uncompress)*/
		if (get_double_write_mode(hw) == 0)
			data |= (0x1 << 8); /*enable first write*/
		else if (get_double_write_mode(hw) & 0x10)
			data |= (0x1 << 9); /*double write only*/
		else
			data |= ((0x1 << 8)  |(0x1 << 9));
		WRITE_VREG(HEVC_DBLK_CFGB, data);
	}

	/* swap uv */
	if ((v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21) ||
		(v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21M))
		data32 &= ~(1 << 8); /* NV21 */
	else
		data32 |= (1 << 8); /* NV12 */
	if (get_double_write_mode(hw) & 0x20)
		data32 &= ~(1 << 8); /* NV21 */

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

	if (get_double_write_mode(hw) & 0x10) {
		/* [23:22] dw_v1_ctrl
		 *[21:20] dw_v0_ctrl
		 *[19:18] dw_h1_ctrl
		 *[17:16] dw_h0_ctrl
		 */
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		/*set them all 0 for H265_NV21 (no down-scale)*/
		data32 &= ~(0xff << 16);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	} else {
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_T7)
			WRITE_VREG(HEVC_SAO_CTRL26, 0);
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		data32 &= (~(0xff << 16));
		if ((get_double_write_mode(hw) & 0xf) == 8) {
			WRITE_VREG(HEVC_SAO_CTRL26, 0xf);
			data32 |= (0xff << 16);
		} else if ((get_double_write_mode(hw) & 0xf) == 2 ||
			(get_double_write_mode(hw) & 0xf) == 3)
			data32 |= (0xff<<16);
		else if ((get_double_write_mode(hw) & 0xf) == 4 ||
			(get_double_write_mode(hw) & 0xf) == 5)
			data32 |= (0x33<<16);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	/* [5:4]	-- address_format 00:linear 01:32x32 10:64x32 */
	data32 |= (hw->mem_map_mode << 4);
	data32 &= (~0xf);
	data32 |= (hw->endian & 0xf);  /* valid only when double write only */

	/* swap uv */
	if ((v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21) ||
		(v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21M))
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

#endif

}


#ifdef AOM_AV1_DBLK_INIT
/*
 * Defines, declarations, sub-functions for av1 de-block loop filter Thr/Lvl table update
 * - struct segmentation_lf is for loop filter only (removed something)
 * - function "av1_loop_filter_init" and "av1_loop_filter_frame_init" will be instantiated in C_Entry
 * - av1_loop_filter_init run once before decoding start
 * - av1_loop_filter_frame_init run before every frame decoding start
 * - set video format to AOM_AV1 is in av1_loop_filter_init
 */
#define MAX_LOOP_FILTER 63
#define MAX_MODE_LF_DELTAS 2
#define MAX_SEGMENTS 8
#define MAX_MB_PLANE 3

typedef enum {
  SEG_LVL_ALT_Q,	   // Use alternate Quantizer ....
  SEG_LVL_ALT_LF_Y_V,  // Use alternate loop filter value on y plane vertical
  SEG_LVL_ALT_LF_Y_H,  // Use alternate loop filter value on y plane horizontal
  SEG_LVL_ALT_LF_U,	// Use alternate loop filter value on u plane
  SEG_LVL_ALT_LF_V,	// Use alternate loop filter value on v plane
  SEG_LVL_REF_FRAME,   // Optional Segment reference frame
  SEG_LVL_SKIP,		// Optional Segment (0,0) + skip mode
  SEG_LVL_GLOBALMV,
  SEG_LVL_MAX
} SEG_LVL_FEATURES;

static const SEG_LVL_FEATURES seg_lvl_lf_lut[MAX_MB_PLANE][2] = {
  { SEG_LVL_ALT_LF_Y_V, SEG_LVL_ALT_LF_Y_H },
  { SEG_LVL_ALT_LF_U, SEG_LVL_ALT_LF_U },
  { SEG_LVL_ALT_LF_V, SEG_LVL_ALT_LF_V }
};

struct segmentation_lf { // for loopfilter only
  uint8_t enabled;
		  /*
		   SEG_LVL_ALT_LF_Y_V feature_enable: seg_lf_info_y[bit7]
		   SEG_LVL_ALT_LF_Y_V data: seg_lf_info_y[bit0~6]
		   SEG_LVL_ALT_LF_Y_H feature enable: seg_lf_info_y[bit15]
		   SEG_LVL_ALT_LF_Y_H data: seg_lf_info_y[bit8~14]
		   */
  uint16_t seg_lf_info_y[8];
		  /*
		   SEG_LVL_ALT_LF_U feature_enable: seg_lf_info_c[bit7]
		   SEG_LVL_ALT_LF_U data: seg_lf_info_c[bit0~6]
		   SEG_LVL_ALT_LF_V feature enable: seg_lf_info_c[bit15]
		   SEG_LVL_ALT_LF_V data: seg_lf_info_c[bit8~14]
		   */
  uint16_t seg_lf_info_c[8];
};

typedef struct {
  uint8_t mblim;
  uint8_t lim;
  uint8_t hev_thr;
} loop_filter_thresh;

typedef struct loop_filter_info_n_s {
  loop_filter_thresh lfthr[MAX_LOOP_FILTER + 1];
  uint8_t lvl[MAX_MB_PLANE][MAX_SEGMENTS][2][REF_FRAMES][MAX_MODE_LF_DELTAS];
} loop_filter_info_n;

struct loopfilter {
  int32_t filter_level[2];
  int32_t filter_level_u;
  int32_t filter_level_v;

  int32_t sharpness_level;

  uint8_t mode_ref_delta_enabled;
  uint8_t mode_ref_delta_update;

  // 0 = Intra, Last, Last2+Last3,
  // GF, BRF, ARF2, ARF
  int8_t ref_deltas[REF_FRAMES];

  // 0 = ZERO_MV, MV
  int8_t mode_deltas[MAX_MODE_LF_DELTAS];

  int32_t combine_vert_horz_lf;

  int32_t lf_pic_cnt;
};
#ifdef DBG_LPF_DBLK_LVL
static int32_t myclamp(int32_t value, int32_t low, int32_t high) {
  return value < low ? low : (value > high ? high : value);
}
#endif

// convert data to int8_t variable
// value : signed data (with any bitwidth<8) which is assigned to uint8_t variable as an input
// bw    : bitwidth of signed data, (from 1 to 7)
static int8_t conv2int8 (uint8_t value, uint8_t bw) {
    if (bw<1 || bw>7) return (int8_t)value;
    else {
	    const uint8_t data_bits = value & ((1<<bw)-1);
	    const uint8_t sign_bit = (value>>(bw-1)) & 0x1;
	    const uint8_t sign_bit_ext = sign_bit | sign_bit<<1 | sign_bit<<2 | sign_bit<<3 | sign_bit<<4 | sign_bit<<5 | sign_bit<<6 | sign_bit<<7;
	    return (int8_t)((sign_bit_ext<<bw) | data_bits);
	}
}

static void av1_update_sharpness(loop_filter_info_n *lfi, int32_t sharpness_lvl) {
  int32_t lvl;

  // For each possible value for the loop filter fill out limits
  for (lvl = 0; lvl <= MAX_LOOP_FILTER; lvl++) {
	// Set loop filter parameters that control sharpness.
    int32_t block_inside_limit =
		lvl >> ((sharpness_lvl > 0) + (sharpness_lvl > 4));

    if (sharpness_lvl > 0) {
	  if (block_inside_limit > (9 - sharpness_lvl))
	    block_inside_limit = (9 - sharpness_lvl);
	}

    if (block_inside_limit < 1)
	  block_inside_limit = 1;

    lfi->lfthr[lvl].lim = (uint8_t)block_inside_limit;
    lfi->lfthr[lvl].mblim = (uint8_t)(2 * (lvl + 2) + block_inside_limit);
  }
}

// instantiate this function once when decode is started
void av1_loop_filter_init(loop_filter_info_n *lfi, struct loopfilter *lf) {
	int32_t i;
	uint32_t data32;

	// init limits for given sharpness
	av1_update_sharpness(lfi, lf->sharpness_level);

	// Write to register
	for (i = 0; i < 32; i++) {
		uint32_t thr;
		thr = ((lfi->lfthr[i*2+1].lim & 0x3f)<<8) |
			(lfi->lfthr[i*2+1].mblim & 0xff);
		thr = (thr<<16) | ((lfi->lfthr[i*2].lim & 0x3f)<<8) |
			(lfi->lfthr[i*2].mblim & 0xff);
		WRITE_VREG(HEVC_DBLK_CFG9, thr);
	}
	// video format is AOM_AV1
	data32 = (0x57 << 8) |  // 1st/2nd write both enable
		(0x4  << 0);   // aom_av1 video format
	WRITE_VREG(HEVC_DBLK_CFGB, data32);
	av1_print2(AOM_DEBUG_HW_MORE,
		"[DBLK DEBUG] CFGB : 0x%x\n", data32);
}

// perform this function per frame
void av1_loop_filter_frame_init(AV1Decoder* pbi, struct segmentation_lf *seg,
	loop_filter_info_n *lfi,
	struct loopfilter *lf,
	int32_t pic_width) {
	BuffInfo_t* buf_spec = pbi->work_space_buf;
	int32_t i;
#ifdef DBG_LPF_DBLK_LVL
	int32_t dir;
	int32_t filt_lvl[MAX_MB_PLANE], filt_lvl_r[MAX_MB_PLANE];
	int32_t plane;
	int32_t seg_id;
#endif
	// n_shift is the multiplier for lf_deltas
	// the multiplier is 1 for when filter_lvl is between 0 and 31;
	// 2 when filter_lvl is between 32 and 63

	// update limits if sharpness has changed
	av1_update_sharpness(lfi, lf->sharpness_level);

	// Write to register
	for (i = 0; i < 32; i++) {
		uint32_t thr;
		thr = ((lfi->lfthr[i*2+1].lim & 0x3f)<<8)
			| (lfi->lfthr[i*2+1].mblim & 0xff);
		thr = (thr<<16) | ((lfi->lfthr[i*2].lim & 0x3f)<<8)
			| (lfi->lfthr[i*2].mblim & 0xff);
		WRITE_VREG(HEVC_DBLK_CFG9, thr);
	}
#ifdef DBG_LPF_DBLK_LVL
	filt_lvl[0] = lf->filter_level[0];
	filt_lvl[1] = lf->filter_level_u;
	filt_lvl[2] = lf->filter_level_v;

	filt_lvl_r[0] = lf->filter_level[1];
	filt_lvl_r[1] = lf->filter_level_u;
	filt_lvl_r[2] = lf->filter_level_v;

#ifdef DBG_LPF_PRINT
	printk("LF_PRINT: pic_cnt(%d) base_filter_level(%d,%d,%d,%d)\n",
		lf->lf_pic_cnt, lf->filter_level[0],
		lf->filter_level[1], lf->filter_level_u, lf->filter_level_v);
#endif

	for (plane = 0; plane < 3; plane++) {
		if (plane == 0 && !filt_lvl[0] && !filt_lvl_r[0])
		  break;
		else if (plane == 1 && !filt_lvl[1])
		  continue;
		else if (plane == 2 && !filt_lvl[2])
		  continue;

		for (seg_id = 0; seg_id < MAX_SEGMENTS; seg_id++) { // MAX_SEGMENTS==8
			for (dir = 0; dir < 2; ++dir) {
				int32_t lvl_seg = (dir == 0) ? filt_lvl[plane] : filt_lvl_r[plane];
				//assert(plane >= 0 && plane <= 2);
				const uint8_t seg_lf_info_y0 = seg->seg_lf_info_y[seg_id] & 0xff;
				const uint8_t seg_lf_info_y1 = (seg->seg_lf_info_y[seg_id]>>8) & 0xff;
				const uint8_t seg_lf_info_u = seg->seg_lf_info_c[seg_id] & 0xff;
				const uint8_t seg_lf_info_v = (seg->seg_lf_info_c[seg_id]>>8) & 0xff;
				const uint8_t seg_lf_info = (plane==2) ? seg_lf_info_v : (plane==1) ?
					seg_lf_info_u : ((dir==0) ?  seg_lf_info_y0 : seg_lf_info_y1);
				const int8_t seg_lf_active = ((seg->enabled) && ((seg_lf_info>>7) & 0x1));
				const int8_t seg_lf_data = conv2int8(seg_lf_info,7);
#ifdef DBG_LPF_PRINT
				const int8_t seg_lf_data_clip = (seg_lf_data>63) ? 63 :
					(seg_lf_data<-63) ? -63 : seg_lf_data;
#endif
				if (seg_lf_active) {
				  lvl_seg = myclamp(lvl_seg + (int32_t)seg_lf_data, 0, MAX_LOOP_FILTER);
				}

#ifdef DBG_LPF_PRINT
				printk("LF_PRINT:plane(%d) seg_id(%d) dir(%d) seg_lf_info(%d,0x%x),lvl_seg(0x%x)\n",
					plane,seg_id,dir,seg_lf_active,seg_lf_data_clip,lvl_seg);
#endif

				if (!lf->mode_ref_delta_enabled) {
				  // we could get rid of this if we assume that deltas are set to
				  // zero when not in use; encoder always uses deltas
				  memset(lfi->lvl[plane][seg_id][dir], lvl_seg,
						 sizeof(lfi->lvl[plane][seg_id][dir]));
				} else {
					int32_t ref, mode;
					const int32_t scale = 1 << (lvl_seg >> 5);
					const int32_t intra_lvl = lvl_seg + lf->ref_deltas[INTRA_FRAME] * scale;
					lfi->lvl[plane][seg_id][dir][INTRA_FRAME][0] =
					  myclamp(intra_lvl, 0, MAX_LOOP_FILTER);
#ifdef DBG_LPF_PRINT
					printk("LF_PRINT:ref_deltas[INTRA_FRAME](%d)\n",lf->ref_deltas[INTRA_FRAME]);
#endif
					for (ref = LAST_FRAME; ref < REF_FRAMES; ++ref) {		 // LAST_FRAME==1 REF_FRAMES==8
						for (mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode) {	 // MAX_MODE_LF_DELTAS==2
							const int32_t inter_lvl =
								lvl_seg + lf->ref_deltas[ref] * scale +
								lf->mode_deltas[mode] * scale;
							lfi->lvl[plane][seg_id][dir][ref][mode] =
								myclamp(inter_lvl, 0, MAX_LOOP_FILTER);
#ifdef DBG_LPF_PRINT
							printk("LF_PRINT:ref_deltas(%d) mode_deltas(%d)\n",
							lf->ref_deltas[ref], lf->mode_deltas[mode]);
#endif
						}
					}
				}
			}
		}
	}

#ifdef DBG_LPF_PRINT
	for (i = 0; i <= MAX_LOOP_FILTER; i++) {
		printk("LF_PRINT:(%2d) thr=%d,blim=%3d,lim=%2d\n",
			i, lfi->lfthr[i].hev_thr,
			lfi->lfthr[i].mblim, lfi->lfthr[i].lim);
	}
	for (plane = 0; plane < 3; plane++) {
		for (seg_id = 0; seg_id < MAX_SEGMENTS; seg_id++) { // MAX_SEGMENTS==8
			for (dir = 0; dir < 2; ++dir) {
				int32_t mode;
				for (mode = 0; mode < 2; ++mode) {
					printk("assign {lvl[%d][%d][%d][0][%d],lvl[%d][%d][%d][1][%d],lvl[%d][%d][%d][2][%d],lvl[%d][%d][%d][3][%d],lvl[%d][%d][%d][4][%d],lvl[%d][%d][%d][5][%d],lvl[%d][%d][%d][6][%d],lvl[%d][%d][%d][7][%d]}={6'd%2d,6'd%2d,6'd%2d,6'd%2d,6'd%2d,6'd%2d,6'd%2d,6'd%2d};\n",
					plane, seg_id, dir, mode,
					plane, seg_id, dir, mode,
					plane, seg_id, dir, mode,
					plane, seg_id, dir, mode,
					plane, seg_id, dir, mode,
					plane, seg_id, dir, mode,
					plane, seg_id, dir, mode,
					plane, seg_id, dir, mode,
					lfi->lvl[plane][seg_id][dir][0][mode],
					lfi->lvl[plane][seg_id][dir][1][mode],
					lfi->lvl[plane][seg_id][dir][2][mode],
					lfi->lvl[plane][seg_id][dir][3][mode],
					lfi->lvl[plane][seg_id][dir][4][mode],
					lfi->lvl[plane][seg_id][dir][5][mode],
					lfi->lvl[plane][seg_id][dir][6][mode],
					lfi->lvl[plane][seg_id][dir][7][mode]);
				}
			}
		}
	}
#endif
	// Write to register
	for (i = 0; i < 192; i++) {
		uint32_t level;
		level = ((lfi->lvl[i>>6&3][i>>3&7][1][i&7][1] & 0x3f)<<24) |
			((lfi->lvl[i>>6&3][i>>3&7][1][i&7][0] & 0x3f)<<16) |
			((lfi->lvl[i>>6&3][i>>3&7][0][i&7][1] & 0x3f)<<8) |
			(lfi->lvl[i>>6&3][i>>3&7][0][i&7][0] & 0x3f);
		if (!lf->filter_level[0] && !lf->filter_level[1])
			level = 0;
		WRITE_VREG(HEVC_DBLK_CFGA, level);
	}
#endif  // DBG_LPF_DBLK_LVL

#ifdef DBG_LPF_DBLK_FORCED_OFF
	if (lf->lf_pic_cnt == 2) {
		printk("LF_PRINT: pic_cnt(%d) dblk forced off !!!\n", lf->lf_pic_cnt);
		WRITE_VREG(HEVC_DBLK_DBLK0, 0);
	} else
		WRITE_VREG(HEVC_DBLK_DBLK0,
			lf->filter_level[0] | lf->filter_level[1] << 6 |
			lf->filter_level_u << 12 | lf->filter_level_v << 18);
#else
	WRITE_VREG(HEVC_DBLK_DBLK0,
		lf->filter_level[0] | lf->filter_level[1]<<6 |
		lf->filter_level_u<<12 | lf->filter_level_v<<18);
#endif
	for (i =0; i < 10; i++)
		WRITE_VREG(HEVC_DBLK_DBLK1,
			((i<2) ? lf->mode_deltas[i&1] : lf->ref_deltas[(i-2)&7]));
	for (i = 0; i < 8; i++)
		WRITE_VREG(HEVC_DBLK_DBLK2,
			(uint32_t)(seg->seg_lf_info_y[i]) | (uint32_t)(seg->seg_lf_info_c[i]<<16));

	// Set P_HEVC_DBLK_CFGB again
	{
		uint32_t lpf_data32 = READ_VREG(HEVC_DBLK_CFGB);
		if (lf->mode_ref_delta_enabled)
			lpf_data32 |=  (0x1<<28); // mode_ref_delta_enabled
		else
			lpf_data32 &= ~(0x1<<28);
		if (seg->enabled)
			lpf_data32 |=  (0x1<<29); // seg enable
		else
			lpf_data32 &= ~(0x1<<29);
		if (pic_width >= 1280)
			lpf_data32 |= (0x1 << 4); // dblk pipeline mode=1 for performance
		else
			lpf_data32 &= ~(0x3 << 4);
		WRITE_VREG(HEVC_DBLK_CFGB, lpf_data32);
	}
	  // Set CDEF
	WRITE_VREG(HEVC_DBLK_CDEF0, buf_spec->cdef_data.buf_start);
	{
		uint32_t cdef_data32 = (READ_VREG(HEVC_DBLK_CDEF1) & 0xffffff00);
		cdef_data32 |= 17;	// TODO ERROR :: cdef temp dma address left offset
#ifdef DBG_LPF_CDEF_NO_PIPELINE
		cdef_data32 |= (1<<17); // cdef test no pipeline for very small picture
#endif
		WRITE_VREG(HEVC_DBLK_CDEF1, cdef_data32);
	}
	// Picture count
	lf->lf_pic_cnt++;
}
#endif  // #ifdef AOM_AV1_DBLK_INIT

#ifdef AOM_AV1_UPSCALE_INIT
/*
 * these functions here for upscaling updated in every picture
 */
#define RS_SUBPEL_BITS 6
#define RS_SUBPEL_MASK ((1 << RS_SUBPEL_BITS) - 1)
#define RS_SCALE_SUBPEL_BITS 14
#define RS_SCALE_SUBPEL_MASK ((1 << RS_SCALE_SUBPEL_BITS) - 1)
#define RS_SCALE_EXTRA_BITS (RS_SCALE_SUBPEL_BITS - RS_SUBPEL_BITS)
#define RS_SCALE_EXTRA_OFF (1 << (RS_SCALE_EXTRA_BITS - 1))

static int32_t av1_get_upscale_convolve_step(int32_t in_length, int32_t out_length) {
  return ((in_length << RS_SCALE_SUBPEL_BITS) + out_length / 2) / out_length;
}

static int32_t get_upscale_convolve_x0(int32_t in_length, int32_t out_length,
									   int32_t x_step_qn) {
  const int32_t err = out_length * x_step_qn - (in_length << RS_SCALE_SUBPEL_BITS);
  const int32_t x0 =
	  (-((out_length - in_length) << (RS_SCALE_SUBPEL_BITS - 1)) +
	   out_length / 2) /
		  out_length +
	  RS_SCALE_EXTRA_OFF - err / 2;
  return (int32_t)((uint32_t)x0 & RS_SCALE_SUBPEL_MASK);
}

void av1_upscale_frame_init(AV1Decoder* pbi, AV1_COMMON *cm, param_t* params)
{
	BuffInfo_t* buf_spec = pbi->work_space_buf;
	const int32_t width                   = cm->dec_width;
	const int32_t superres_upscaled_width = cm->superres_upscaled_width;
	const int32_t x_step_qn_luma          = av1_get_upscale_convolve_step(width, superres_upscaled_width);
	const int32_t x0_qn_luma              = get_upscale_convolve_x0(width, superres_upscaled_width, x_step_qn_luma);
	const int32_t x_step_qn_chroma        = av1_get_upscale_convolve_step((width+1)>>1, (superres_upscaled_width+1)>>1);
	const int32_t x0_qn_chroma            = get_upscale_convolve_x0((width+1)>>1, (superres_upscaled_width+1)>>1, x_step_qn_chroma);
    av1_print2(AOM_DEBUG_HW_MORE,
		"UPS_PRINT: width(%d -> %d)\n",
		width, superres_upscaled_width);
    av1_print2(AOM_DEBUG_HW_MORE,
		"UPS_PRINT: xstep(%d,%d)(0x%X, 0x%X) x0qn(%d,%d)(0x%X, 0x%X)\n",
	  x_step_qn_luma,x_step_qn_chroma,
	  x_step_qn_luma,x_step_qn_chroma,
	  x0_qn_luma,x0_qn_chroma,
	  x0_qn_luma,x0_qn_chroma);
    WRITE_VREG(HEVC_DBLK_UPS1, buf_spec->ups_data.buf_start);
    WRITE_VREG(HEVC_DBLK_UPS2, x0_qn_luma);		 // x0_qn y
    WRITE_VREG(HEVC_DBLK_UPS3, x0_qn_chroma);	   // x0_qn c
    WRITE_VREG(HEVC_DBLK_UPS4, x_step_qn_luma);	 // x_step y
    WRITE_VREG(HEVC_DBLK_UPS5, x_step_qn_chroma);   // x_step c
    WRITE_VREG(AV1_UPSCALE_X0_QN, (x0_qn_chroma<<16)|x0_qn_luma);
    WRITE_VREG(AV1_UPSCALE_STEP_QN, (x_step_qn_chroma<<16)|x_step_qn_luma);

/*
 * TileR calculation here if cm needs an exactly accurate value
 */
#ifdef AV1_UPSCALE_TILER_CALCULATION
    uint32_t upscl_enabled = 1; // 1 just for example, actually this is use_superres flag
    uint32_t tiler_x = 192; // 192 just for example, actually this is tile end
    uint32_t ux;
    uint32_t ux_tiler,ux_tiler_rnd32;
    uint32_t xqn_y;
    uint32_t xqn_c;
    uint32_t tiler_x_y = tiler_x     - 8 - 3; // dblk/cdef left-shift-8 plus upscaling extra-3
    uint32_t tiler_x_c = (tiler_x/2) - 4 - 3; // dblk/cdef left-shift-4 plus upscaling extra-3

    xqn_y = x0_qn_luma;
    xqn_c = x0_qn_chroma;
    ux_tiler = 0;
    ux_tiler_rnd32 = 0;
    for (ux=0; ux<16384; ux+=8) {
	    uint32_t x1qn_y     = xqn_y + x_step_qn_luma  *(  7+3); // extra-3 is for lrf
	    uint32_t x1qn_c     = xqn_c + x_step_qn_chroma*(  3+3); // extra-3 is for lrf
	    uint32_t x1qn_y_nxt = xqn_y + x_step_qn_luma  *(8+7+3); // extra-3 is for lrf
	    uint32_t x1qn_c_nxt = xqn_c + x_step_qn_chroma*(4+3+3); // extra-3 is for lrf

	    uint32_t x1_y = upscl_enabled ? (x1qn_y>>14) : ux    +7+3;
	    uint32_t x1_c = upscl_enabled ? (x1qn_c>>14) : (ux/2)+3+3;
	    uint32_t x1_y_nxt = upscl_enabled ? (x1qn_y_nxt>>14) : ux    +8+7+3;
	    uint32_t x1_c_nxt = upscl_enabled ? (x1qn_c_nxt>>14) : (ux/2)+4+3+3;

	    if ((x1_y<tiler_x_y && x1_c<tiler_x_c) &&
			(x1_y_nxt>=tiler_x_y || x1_c_nxt>=tiler_x_c)) {
		    ux_tiler = ux;
		    ux_tiler_rnd32 = (ux_tiler/32 + (ux_tiler%32 ? 1 : 0)) * 32;
		    break;
		}

	    xqn_y += x_step_qn_luma*8;
	    xqn_c += x_step_qn_chroma*4;
	}

    av1_print(hw, AOM_DEBUG_HW_MORE,
		"UPS_PRINT: xqn_y(0x%x), xqn_c(0x%x), x1qn_y(0x%x), x1qn_c(0x%x)\n",
		xqn_y, xqn_c, x1qn_y, x1qn_c);
    av1_print(hw, AOM_DEBUG_HW_MORE,
		"UPS_PRINT: ux_tiler(%d)(0x%x), ux_tiler_rnd32(%d)(0x%x)\n",
		ux_tiler, ux_tiler, ux_tiler_rnd32, ux_tiler_rnd32);
#endif
}

#endif // #ifdef AOM_AV1_UPSCALE_INIT

static void release_dblk_struct(struct AV1HW_s *hw)
{
#ifdef AOM_AV1_DBLK_INIT
	if (hw->lfi)
		vfree(hw->lfi);
	if (hw->lf)
		vfree(hw->lf);
	if (hw->seg_4lf)
		vfree(hw->seg_4lf);
	hw->lfi = NULL;
	hw->lf = NULL;
	hw->seg_4lf = NULL;
#endif
}

static int init_dblk_struc(struct AV1HW_s *hw)
{
#ifdef AOM_AV1_DBLK_INIT
    hw->lfi = vmalloc(sizeof(loop_filter_info_n));
    hw->lf = vmalloc(sizeof(struct loopfilter));
    hw->seg_4lf = vmalloc(sizeof(struct segmentation_lf));

    if (hw->lfi == NULL || hw->lf == NULL || hw->seg_4lf == NULL) {
		printk("[test.c] aom_loop_filter init malloc error!!!\n");
		release_dblk_struct(hw);
		return -1;
	}

    hw->lf->mode_ref_delta_enabled = 1; // set default here
    hw->lf->mode_ref_delta_update = 1; // set default here
    hw->lf->sharpness_level = 0; // init to 0
    hw->lf->lf_pic_cnt = 0; // init to 0
#endif
    return 0;
}

static void config_dblk_hw(struct AV1HW_s *hw)
{
    AV1Decoder *pbi = hw->pbi;
    AV1_COMMON *cm = &hw->common;
    loop_filter_info_n *lfi = hw->lfi;
    struct loopfilter *lf = hw->lf;
    struct segmentation_lf *seg_4lf = hw->seg_4lf;
    BuffInfo_t* buf_spec = pbi->work_space_buf;
	PIC_BUFFER_CONFIG* cur_pic_config = &cm->cur_frame->buf;
	PIC_BUFFER_CONFIG* prev_pic_config = &cm->prev_frame->buf;
    int i;

#ifdef AOM_AV1_DBLK_INIT
#ifdef DUAL_DECODE
#else
	av1_print(hw, AOM_DEBUG_HW_MORE,
	"[test.c ref_delta] cur_frame : %p prev_frame : %p - %p \n",
	cm->cur_frame, cm->prev_frame,
	av1_get_primary_ref_frame_buf(cm));
	// get lf parameters from parser
	lf->mode_ref_delta_enabled =
		(hw->aom_param.p.loop_filter_mode_ref_delta_enabled & 1);
	lf->mode_ref_delta_update =
		((hw->aom_param.p.loop_filter_mode_ref_delta_enabled >> 1) & 1);
	lf->sharpness_level =
		hw->aom_param.p.loop_filter_sharpness_level;
	if (((hw->aom_param.p.loop_filter_mode_ref_delta_enabled)&3) == 3) { // enabled but and update
		if (cm->prev_frame <= 0) {
		  // already initialized in Microcode
			lf->ref_deltas[0] = conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_0),7);
			lf->ref_deltas[1]	= conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_0>>8),7);
			lf->ref_deltas[2]	= conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_1),7);
			lf->ref_deltas[3]	= conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_1>>8),7);
			lf->ref_deltas[4]	= conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_2),7);
			lf->ref_deltas[5]	= conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_2>>8),7);
			lf->ref_deltas[6]	= conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_3),7);
			lf->ref_deltas[7]	= conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_3>>8),7);
			lf->mode_deltas[0] = conv2int8((uint8_t)(hw->aom_param.p.loop_filter_mode_deltas_0),7);
			lf->mode_deltas[1] = conv2int8((uint8_t)(hw->aom_param.p.loop_filter_mode_deltas_0>>8),7);
		} else {
			lf->ref_deltas[0] = (hw->aom_param.p.loop_filter_ref_deltas_0 & 0x80) ?
				conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_0),7) :
			    cm->prev_frame->ref_deltas[0];
			lf->ref_deltas[1]	= (hw->aom_param.p.loop_filter_ref_deltas_0 & 0x8000) ?
				conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_0>>8),7) :
			    cm->prev_frame->ref_deltas[1];
			lf->ref_deltas[2]	= (hw->aom_param.p.loop_filter_ref_deltas_1 & 0x80) ?
				conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_1),7) :
			    cm->prev_frame->ref_deltas[2];
			lf->ref_deltas[3]	= (hw->aom_param.p.loop_filter_ref_deltas_1 & 0x8000) ?
				conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_1>>8),7) :
			    cm->prev_frame->ref_deltas[3];
			lf->ref_deltas[4]	= (hw->aom_param.p.loop_filter_ref_deltas_2 & 0x80) ?
				conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_2),7) :
			    cm->prev_frame->ref_deltas[4];
			lf->ref_deltas[5]	= (hw->aom_param.p.loop_filter_ref_deltas_2 & 0x8000) ?
				conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_2>>8),7) :
			    cm->prev_frame->ref_deltas[5];
			lf->ref_deltas[6] = (hw->aom_param.p.loop_filter_ref_deltas_3 & 0x80) ?
				conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_3),7) :
			    cm->prev_frame->ref_deltas[6];
			lf->ref_deltas[7]	= (hw->aom_param.p.loop_filter_ref_deltas_3 & 0x8000) ?
				conv2int8((uint8_t)(hw->aom_param.p.loop_filter_ref_deltas_3>>8),7) :
			    cm->prev_frame->ref_deltas[7];
			lf->mode_deltas[0] = (hw->aom_param.p.loop_filter_mode_deltas_0 & 0x80) ?
				conv2int8((uint8_t)(hw->aom_param.p.loop_filter_mode_deltas_0),7) :
			    cm->prev_frame->mode_deltas[0];
			lf->mode_deltas[1] = (hw->aom_param.p.loop_filter_mode_deltas_0 & 0x8000) ?
				conv2int8((uint8_t)(hw->aom_param.p.loop_filter_mode_deltas_0>>8),7) :
			    cm->prev_frame->mode_deltas[1];
		}
	}
	  else {
		if ((cm->prev_frame <= 0) | (hw->aom_param.p.loop_filter_mode_ref_delta_enabled & 4)) {
			av1_print(hw, AOM_DEBUG_HW_MORE,
				"[test.c] mode_ref_delta set to default\n");
			lf->ref_deltas[0] = conv2int8((uint8_t)1,7);
			lf->ref_deltas[1] = conv2int8((uint8_t)0,7);
			lf->ref_deltas[2] = conv2int8((uint8_t)0,7);
			lf->ref_deltas[3] = conv2int8((uint8_t)0,7);
			lf->ref_deltas[4] = conv2int8((uint8_t)0xff,7);
			lf->ref_deltas[5] = conv2int8((uint8_t)0,7);
			lf->ref_deltas[6] = conv2int8((uint8_t)0xff,7);
			lf->ref_deltas[7] = conv2int8((uint8_t)0xff,7);
			lf->mode_deltas[0] = conv2int8((uint8_t)0,7);
			lf->mode_deltas[1] = conv2int8((uint8_t)0,7);
		} else {
			av1_print(hw, AOM_DEBUG_HW_MORE,
				"[test.c] mode_ref_delta copy from prev_frame\n");
			lf->ref_deltas[0]			   = cm->prev_frame->ref_deltas[0];
			lf->ref_deltas[1]			   = cm->prev_frame->ref_deltas[1];
			lf->ref_deltas[2]			   = cm->prev_frame->ref_deltas[2];
			lf->ref_deltas[3]			   = cm->prev_frame->ref_deltas[3];
			lf->ref_deltas[4]			   = cm->prev_frame->ref_deltas[4];
			lf->ref_deltas[5]			   = cm->prev_frame->ref_deltas[5];
			lf->ref_deltas[6]			   = cm->prev_frame->ref_deltas[6];
			lf->ref_deltas[7]			   = cm->prev_frame->ref_deltas[7];
			lf->mode_deltas[0]			  = cm->prev_frame->mode_deltas[0];
			lf->mode_deltas[1]			  = cm->prev_frame->mode_deltas[1];
		}
	}
	lf->filter_level[0]			 = hw->aom_param.p.loop_filter_level_0;
	lf->filter_level[1]			 = hw->aom_param.p.loop_filter_level_1;
	lf->filter_level_u    		  = hw->aom_param.p.loop_filter_level_u;
	lf->filter_level_v    		  = hw->aom_param.p.loop_filter_level_v;

	cm->cur_frame->ref_deltas[0] = lf->ref_deltas[0];
	cm->cur_frame->ref_deltas[1] = lf->ref_deltas[1];
	cm->cur_frame->ref_deltas[2] = lf->ref_deltas[2];
	cm->cur_frame->ref_deltas[3] = lf->ref_deltas[3];
	cm->cur_frame->ref_deltas[4] = lf->ref_deltas[4];
	cm->cur_frame->ref_deltas[5] = lf->ref_deltas[5];
	cm->cur_frame->ref_deltas[6] = lf->ref_deltas[6];
	cm->cur_frame->ref_deltas[7] = lf->ref_deltas[7];
	cm->cur_frame->mode_deltas[0] = lf->mode_deltas[0];
	cm->cur_frame->mode_deltas[1] = lf->mode_deltas[1];

	// get seg_4lf parameters from parser
	seg_4lf->enabled = hw->aom_param.p.segmentation_enabled & 1;
	cm->cur_frame->segmentation_enabled = hw->aom_param.p.segmentation_enabled & 1;
	cm->cur_frame->intra_only = (hw->aom_param.p.segmentation_enabled >> 2) & 1;
	cm->cur_frame->segmentation_update_map = (hw->aom_param.p.segmentation_enabled >> 3) & 1;

	if (hw->aom_param.p.segmentation_enabled & 1) { // segmentation_enabled
		if (hw->aom_param.p.segmentation_enabled & 2) { // segmentation_update_data
			for (i = 0; i < MAX_SEGMENTS; i++) {
				seg_4lf->seg_lf_info_y[i] = hw->aom_param.p.seg_lf_info_y[i];
				seg_4lf->seg_lf_info_c[i] = hw->aom_param.p.seg_lf_info_c[i];
				#ifdef DBG_LPF_PRINT
					printk(" read seg_lf_info [%d] : 0x%x, 0x%x\n",
					i, seg_4lf->seg_lf_info_y[i], seg_4lf->seg_lf_info_c[i]);
				#endif
				}
			} // segmentation_update_data
			else { // no segmentation_update_data
				if (cm->prev_frame <= 0) {
					for (i=0;i<MAX_SEGMENTS;i++) {
						seg_4lf->seg_lf_info_y[i] = 0;
						seg_4lf->seg_lf_info_c[i] = 0;
					}
				} else {
					for (i = 0; i < MAX_SEGMENTS; i++) {
						seg_4lf->seg_lf_info_y[i] = cm->prev_frame->seg_lf_info_y[i];
						seg_4lf->seg_lf_info_c[i] = cm->prev_frame->seg_lf_info_c[i];
		#ifdef DBG_LPF_PRINT
					  printk(" Refrence seg_lf_info [%d] : 0x%x, 0x%x\n",
					  i, seg_4lf->seg_lf_info_y[i], seg_4lf->seg_lf_info_c[i]);
		#endif
					}
				}
			} // no segmentation_update_data
		} // segmentation_enabled
		else {
			for (i=0;i<MAX_SEGMENTS;i++) {
				seg_4lf->seg_lf_info_y[i] = 0;
				seg_4lf->seg_lf_info_c[i] = 0;
			}
		} // NOT segmentation_enabled
		for (i=0;i<MAX_SEGMENTS;i++) {
			cm->cur_frame->seg_lf_info_y[i] = seg_4lf->seg_lf_info_y[i];
			cm->cur_frame->seg_lf_info_c[i] = seg_4lf->seg_lf_info_c[i];
#ifdef DBG_LPF_PRINT
			printk(" SAVE seg_lf_info [%d] : 0x%x, 0x%x\n",
			i, cm->cur_frame->seg_lf_info_y[i],
			cm->cur_frame->seg_lf_info_c[i]);
#endif
		}

	/*
	* Update loop filter Thr/Lvl table for every frame
	*/
	av1_print(hw, AOM_DEBUG_HW_MORE,
		"[test.c] av1_loop_filter_frame_init (run before every frame decoding start)\n");
	av1_loop_filter_frame_init(pbi, seg_4lf, lfi, lf, cm->dec_width);
#endif // not DUAL_DECODE
#endif

#ifdef AOM_AV1_UPSCALE_INIT
	/*
	* init for upscaling
	*/
	av1_print(hw, AOM_DEBUG_HW_MORE,
		"[test.c] av1_upscale_frame_init (run before every frame decoding start)\n");
	av1_upscale_frame_init(pbi,
		pbi->common, &hw->aom_param);
#endif // #ifdef AOM_AV1_UPSCALE_INIT

	av1_print(hw, AOM_DEBUG_HW_MORE,
		"[test.c] cur_frame : %p prev_frame : %p - %p \n",
		cm->cur_frame, cm->prev_frame, av1_get_primary_ref_frame_buf(cm));
	if (cm->cur_frame <= 0) {
		WRITE_VREG(AOM_AV1_CDF_BUFFER_W, buf_spec->cdf_buf.buf_start);
		WRITE_VREG(AOM_AV1_SEG_MAP_BUFFER_W, buf_spec->seg_map.buf_start);
	}
	else {
		av1_print(hw, AOM_DEBUG_HW_MORE,
			"[test.c] Config WRITE CDF_BUF/SEG_MAP_BUF  : %d\n",
			cur_pic_config->index);
		WRITE_VREG(AOM_AV1_CDF_BUFFER_W,
			buf_spec->cdf_buf.buf_start + (0x8000*cur_pic_config->index));
		WRITE_VREG(AOM_AV1_SEG_MAP_BUFFER_W,
			buf_spec->seg_map.buf_start + ((buf_spec->seg_map.buf_size / 16) * cur_pic_config->index));
	}
	cm->cur_frame->seg_mi_rows = cm->cur_frame->mi_rows;
	cm->cur_frame->seg_mi_cols = cm->cur_frame->mi_cols;
	if (cm->prev_frame <= 0) {
		WRITE_VREG(AOM_AV1_CDF_BUFFER_R, buf_spec->cdf_buf.buf_start);
		WRITE_VREG(AOM_AV1_SEG_MAP_BUFFER_R, buf_spec->seg_map.buf_start);
	} else {
		av1_print(hw, AOM_DEBUG_HW_MORE,
			"[test.c] Config READ  CDF_BUF/SEG_MAP_BUF  : %d\n",
			prev_pic_config->index);
		WRITE_VREG(AOM_AV1_CDF_BUFFER_R,
			buf_spec->cdf_buf.buf_start + (0x8000*prev_pic_config->index));
		WRITE_VREG(AOM_AV1_SEG_MAP_BUFFER_R,
			buf_spec->seg_map.buf_start + ((buf_spec->seg_map.buf_size / 16) * prev_pic_config->index));

		// segmentation_enabled but no segmentation_update_data
		if ((hw->aom_param.p.segmentation_enabled & 3) == 1) {
			av1_print(hw, AOM_DEBUG_HW_MORE,
				"[test.c] segfeatures_copy from prev_frame\n");
			for (i = 0; i < 8; i++) {
				WRITE_VREG(AOM_AV1_SEGMENT_FEATURE,
					cm->prev_frame->segment_feature[i]);
			}
		}
		// segmentation_enabled but no segmentation_update_map
		if ((hw->aom_param.p.segmentation_enabled & 9) == 1) {
			av1_print(hw, AOM_DEBUG_HW_MORE,
				"[test.c] seg_map_size copy from prev_frame\n");
			cm->cur_frame->seg_mi_rows = cm->prev_frame->seg_mi_rows;
			cm->cur_frame->seg_mi_cols = cm->prev_frame->seg_mi_cols;
		}
	}
#ifdef PRINT_HEVC_DATA_PATH_MONITOR
	{
		uint32_t total_clk_count;
		uint32_t path_transfer_count;
		uint32_t path_wait_count;
		float path_wait_ratio;
		if (pbi->decode_idx > 1) {
			WRITE_VREG(HEVC_PATH_MONITOR_CTRL, 0); // Disabble monitor and set rd_idx to 0
			total_clk_count = READ_VREG(HEVC_PATH_MONITOR_DATA);

			WRITE_VREG(HEVC_PATH_MONITOR_CTRL, (1<<4)); // Disabble monitor and set rd_idx to 0

			// parser --> iqit
			path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			if (path_transfer_count == 0)
				path_wait_ratio = 0.0;
			else
				path_wait_ratio =
					(float)path_wait_count/(float)path_transfer_count;
			printk("[P%d HEVC PATH] Parser/IQIT/IPP/DBLK/OW/DDR/CMD WAITING \% : %.2f",
				pbi->decode_idx - 2,
				path_wait_ratio);

			// iqit --> ipp
			path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			if (path_transfer_count == 0)
				path_wait_ratio = 0.0;
			else
				path_wait_ratio = (float)path_wait_count/(float)path_transfer_count;
			printk(" %.2f", path_wait_ratio);

			// dblk <-- ipp
			path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			if (path_transfer_count == 0)
				path_wait_ratio = 0.0;
			else
				path_wait_ratio = (float)path_wait_count/(float)path_transfer_count;
			printk(" %.2f", path_wait_ratio);

			// dblk --> ow
			path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			if (path_transfer_count == 0)
				path_wait_ratio = 0.0;
			else path_wait_ratio =
				(float)path_wait_count/(float)path_transfer_count;
			printk(" %.2f", path_wait_ratio);

			// <--> DDR
			path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			if (path_transfer_count == 0)
				path_wait_ratio = 0.0;
			else path_wait_ratio =
				(float)path_wait_count/(float)path_transfer_count;
			printk(" %.2f", path_wait_ratio);

			// CMD
			path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
			if (path_transfer_count == 0)
				path_wait_ratio = 0.0;
			else
				path_wait_ratio = (float)path_wait_count/(float)path_transfer_count;
			printk(" %.2f\n", path_wait_ratio);
		}
	}
#endif
}

static void aom_config_work_space_hw(struct AV1HW_s *hw, u32 mask)
{
	struct BuffInfo_s *buf_spec = hw->work_space_buf;
	unsigned int data32;
	av1_print(hw, AOM_DEBUG_HW_MORE, "%s %d\n", __func__, __LINE__);
	if (debug && hw->init_flag == 0)
		av1_print(hw, AOM_DEBUG_HW_MORE, "%s %x %x %x %x %x %x %x %x\n",
			__func__,
			buf_spec->ipp.buf_start,
			buf_spec->start_adr,
			buf_spec->short_term_rps.buf_start,
			buf_spec->sao_up.buf_start,
			buf_spec->swap_buf.buf_start,
			buf_spec->scalelut.buf_start,
			buf_spec->dblk_para.buf_start,
			buf_spec->dblk_data.buf_start);
	if (mask & HW_MASK_FRONT) {
		av1_print(hw, AOM_DEBUG_HW_MORE, "%s %d\n", __func__, __LINE__);
		if ((debug & AOM_AV1_DEBUG_SEND_PARAM_WITH_REG) == 0)
			WRITE_VREG(HEVC_RPM_BUFFER, (u32)hw->rpm_phy_addr);

		WRITE_VREG(LMEM_DUMP_ADR, (u32)hw->lmem_phy_addr);

	}

	av1_print(hw, AOM_DEBUG_HW_MORE, "%s %d\n", __func__, __LINE__);

    WRITE_VREG(AOM_AV1_DAALA_TOP_BUFFER,
		buf_spec->daala_top.buf_start);
    WRITE_VREG(AV1_GMC_PARAM_BUFF_ADDR,
		buf_spec->gmc_buf.buf_start);

    WRITE_VREG(HEVC_DBLK_CFG4,
		buf_spec->dblk_para.buf_start); // cfg_addr_cif
    WRITE_VREG(HEVC_DBLK_CFG5,
		buf_spec->dblk_data.buf_start); // cfg_addr_xio

	if (mask & HW_MASK_BACK) {
#ifdef LOSLESS_COMPRESS_MODE
		int losless_comp_header_size =
			compute_losless_comp_header_size(hw->init_pic_w,
			hw->init_pic_h);
		int losless_comp_body_size =
			compute_losless_comp_body_size(hw->init_pic_w,
			hw->init_pic_h, buf_alloc_depth == 10);
#endif
#ifdef AOM_AV1_MMU_DW
		int losless_comp_header_size_dw =
			compute_losless_comp_header_size_dw(hw->init_pic_w,
			hw->init_pic_h);
		int losless_comp_body_size_dw =
			compute_losless_comp_body_size_dw(hw->init_pic_w,
			hw->init_pic_h, buf_alloc_depth == 10);
#endif
		WRITE_VREG(HEVCD_IPP_LINEBUFF_BASE,
			buf_spec->ipp.buf_start);
#ifdef CHANGE_REMOVED
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
			/* cfg_addr_adp*/
			WRITE_VREG(HEVC_DBLK_CFGE, buf_spec->dblk_para.buf_start);
			if (debug & AV1_DEBUG_BUFMGR_MORE)
				pr_info("Write HEVC_DBLK_CFGE\n");
		}
#endif
		/* cfg_p_addr */
		WRITE_VREG(HEVC_DBLK_CFG4, buf_spec->dblk_para.buf_start);
		/* cfg_d_addr */
		WRITE_VREG(HEVC_DBLK_CFG5, buf_spec->dblk_data.buf_start);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) {
		if (buf_spec->max_width <= 4096 && buf_spec->max_height <= 2304)
			WRITE_VREG(HEVC_DBLK_CFG3, 0x404010); //default value
		else
			WRITE_VREG(HEVC_DBLK_CFG3, 0x808020); // make left storage 2 x 4k]
		av1_print(hw, AV1_DEBUG_BUFMGR_MORE,
			"HEVC_DBLK_CFG3 = %x\n", READ_VREG(HEVC_DBLK_CFG3));
	}

#ifdef LOSLESS_COMPRESS_MODE
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
		(get_double_write_mode(hw) != 0x10)) {
		/*bit[4] : paged_mem_mode*/
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0x1 << 4));
#ifdef CHANGE_REMOVED
		if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_SM1)
#endif
			WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, 0);
	} else {
		/*bit[3] smem mdoe*/
		/*else WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (1<<3));*/
		/*bit[3] smem mdoe*/
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL2,
			(losless_comp_body_size >> 5));
	}

	/*8-bit mode */
	WRITE_VREG(HEVC_CM_BODY_LENGTH, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH, losless_comp_header_size);
	if (get_double_write_mode(hw) & 0x10)
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);
#else
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);
#endif

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
		(get_double_write_mode(hw) != 0x10)) {
		WRITE_VREG(HEVC_SAO_MMU_VH0_ADDR, buf_spec->mmu_vbh.buf_start);
		WRITE_VREG(HEVC_SAO_MMU_VH1_ADDR, buf_spec->mmu_vbh.buf_start
				+ VBH_BUF_SIZE(buf_spec));

		/* use HEVC_CM_HEADER_START_ADDR */
		data32 = READ_VREG(HEVC_SAO_CTRL5);
		data32 |= (1<<10);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}
#ifdef AOM_AV1_MMU_DW
    data32 = READ_VREG(HEVC_SAO_CTRL5);
	if (get_double_write_mode(hw) & 0x20) {
		u32 data_tmp;
		data_tmp = READ_VREG(HEVC_SAO_CTRL9);
		data_tmp |= (1<<10);
		WRITE_VREG(HEVC_SAO_CTRL9, data_tmp);

	    WRITE_VREG(HEVC_CM_BODY_LENGTH2,losless_comp_body_size_dw);
	    WRITE_VREG(HEVC_CM_HEADER_OFFSET2,losless_comp_body_size_dw);
	    WRITE_VREG(HEVC_CM_HEADER_LENGTH2,losless_comp_header_size_dw);

	    WRITE_VREG(HEVC_SAO_MMU_VH0_ADDR2, buf_spec->mmu_vbh_dw.buf_start);
	    WRITE_VREG(HEVC_SAO_MMU_VH1_ADDR2, buf_spec->mmu_vbh_dw.buf_start
			+ DW_VBH_BUF_SIZE(buf_spec));

		WRITE_VREG(HEVC_DW_VH0_ADDDR, buf_spec->mmu_vbh_dw.buf_start
			+ (2 * DW_VBH_BUF_SIZE(buf_spec)));
		WRITE_VREG(HEVC_DW_VH1_ADDDR, buf_spec->mmu_vbh_dw.buf_start
			+ (3 * DW_VBH_BUF_SIZE(buf_spec)));

		/* use HEVC_CM_HEADER_START_ADDR */
	    data32 |= (1<<15);
	} else
	    data32 &= ~(1<<15);
    WRITE_VREG(HEVC_SAO_CTRL5, data32);
#endif

	WRITE_VREG(LMEM_DUMP_ADR, (u32)hw->lmem_phy_addr);
#ifdef CHANGE_REMOVED

		WRITE_VREG(AV1_SEG_MAP_BUFFER, buf_spec->seg_map.buf_start);

		 /**/
		WRITE_VREG(AV1_PROB_SWAP_BUFFER, hw->prob_buffer_phy_addr);
		WRITE_VREG(AV1_COUNT_SWAP_BUFFER, hw->count_buffer_phy_addr);
		if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
			(get_double_write_mode(hw) != 0x10)) {
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A)
				WRITE_VREG(HEVC_ASSIST_MMU_MAP_ADDR, hw->frame_mmu_map_phy_addr);
			else
				WRITE_VREG(AV1_MMU_MAP_BUFFER, hw->frame_mmu_map_phy_addr);
		}
#else
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
		(get_double_write_mode(hw) != 0x10)) {
	    WRITE_VREG(HEVC_SAO_MMU_DMA_CTRL, hw->frame_mmu_map_phy_addr);
	}
#ifdef AOM_AV1_MMU_DW
	if (get_double_write_mode(hw) & 0x20) {
		WRITE_VREG(HEVC_SAO_MMU_DMA_CTRL2, hw->dw_frame_mmu_map_phy_addr);
		//default of 0xffffffff will disable dw
	    WRITE_VREG(HEVC_SAO_Y_START_ADDR, 0);
	    WRITE_VREG(HEVC_SAO_C_START_ADDR, 0);
	}
#endif
#endif
#ifdef CO_MV_COMPRESS
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_T5D) {
	    data32 = READ_VREG(HEVC_MPRED_CTRL4);
	    data32 |=  (1 << 1);
	    WRITE_VREG(HEVC_MPRED_CTRL4, data32);
	}
#endif
	}

	config_aux_buf(hw);
}

#ifdef MCRCC_ENABLE
static u32 mcrcc_cache_alg_flag = 1;
static void mcrcc_perfcount_reset(struct AV1HW_s *hw);
static void decomp_perfcount_reset(struct AV1HW_s *hw);
#endif

static void aom_init_decoder_hw(struct AV1HW_s *hw, u32 mask)
{
	unsigned int data32;
	int i;
	const unsigned short parser_cmd[PARSER_CMD_NUMBER] = {
		0x0401, 0x8401, 0x0800, 0x0402, 0x9002, 0x1423,
		0x8CC3, 0x1423, 0x8804, 0x9825, 0x0800, 0x04FE,
		0x8406, 0x8411, 0x1800, 0x8408, 0x8409, 0x8C2A,
		0x9C2B, 0x1C00, 0x840F, 0x8407, 0x8000, 0x8408,
		0x2000, 0xA800, 0x8410, 0x04DE, 0x840C, 0x840D,
		0xAC00, 0xA000, 0x08C0, 0x08E0, 0xA40E, 0xFC00,
		0x7C00
	};

	/*if (debug & AV1_DEBUG_BUFMGR_MORE)
		pr_info("%s\n", __func__);*/
	if (mask & HW_MASK_FRONT) {
		data32 = READ_VREG(HEVC_PARSER_INT_CONTROL);
#ifdef CHANGE_REMOVED
		/* set bit 31~29 to 3 if HEVC_STREAM_FIFO_CTL[29] is 1 */
		data32 &= ~(7 << 29);
		data32 |= (3 << 29);
		data32 = data32 |
		(1 << 24) |/*stream_buffer_empty_int_amrisc_enable*/
		(1 << 22) |/*stream_fifo_empty_int_amrisc_enable*/
		(1 << 7) |/*dec_done_int_cpu_enable*/
		(1 << 4) |/*startcode_found_int_cpu_enable*/
		(0 << 3) |/*startcode_found_int_amrisc_enable*/
		(1 << 0)	/*parser_int_enable*/
		;
#else
		data32 = data32 & 0x03ffffff;
		data32 = data32 |
			(3 << 29) |  // stream_buffer_empty_int_ctl ( 0x200 interrupt)
			(3 << 26) |  // stream_fifo_empty_int_ctl ( 4 interrupt)
			(1 << 24) |  // stream_buffer_empty_int_amrisc_enable
			(1 << 22) |  // stream_fifo_empty_int_amrisc_enable
#ifdef AOM_AV1_HED_FB
#ifdef DUAL_DECODE
		// For HALT CCPU test. Use Pull inside CCPU to generate interrupt
		// (1 << 9) |  // fed_fb_slice_done_int_amrisc_enable
#else
			(1 << 10) |  // fed_fb_slice_done_int_cpu_enable
#endif
#endif
			(1 << 7) |  // dec_done_int_cpu_enable
			(1 << 4) |  // startcode_found_int_cpu_enable
			(0 << 3) |  // startcode_found_int_amrisc_enable
			(1 << 0)    // parser_int_enable
			;
#endif
		WRITE_VREG(HEVC_PARSER_INT_CONTROL, data32);

		data32 = READ_VREG(HEVC_SHIFT_STATUS);
		data32 = data32 |
		(0 << 1) |/*emulation_check_off AV1
			do not have emulation*/
		(1 << 0)/*startcode_check_on*/
		;
		WRITE_VREG(HEVC_SHIFT_STATUS, data32);
		WRITE_VREG(HEVC_SHIFT_CONTROL,
		(0 << 14) | /*disable_start_code_protect*/
		(1 << 10) | /*length_zero_startcode_en for AV1*/
		(1 << 9) | /*length_valid_startcode_en for AV1*/
		(3 << 6) | /*sft_valid_wr_position*/
		(2 << 4) | /*emulate_code_length_sub_1*/
		(3 << 1) | /*start_code_length_sub_1
		AV1 use 0x00000001 as startcode (4 Bytes)*/
		(1 << 0)   /*stream_shift_enable*/
		);

		WRITE_VREG(HEVC_CABAC_CONTROL,
			(1 << 0)/*cabac_enable*/
		);

		WRITE_VREG(HEVC_PARSER_CORE_CONTROL,
			(1 << 0)/* hevc_parser_core_clk_en*/
		);

		WRITE_VREG(HEVC_DEC_STATUS_REG, 0);
	}

	if (mask & HW_MASK_BACK) {
		/*Initial IQIT_SCALELUT memory
		-- just to avoid X in simulation*/
		if (is_rdma_enable())
			rdma_back_end_work(hw->rdma_phy_adr, RDMA_SIZE);
		else {
			WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0);/*cfg_p_addr*/
			for (i = 0; i < 1024; i++)
				WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, 0);
		}
	}

	if (mask & HW_MASK_FRONT) {
		u32 decode_mode;
#ifdef MULTI_INSTANCE_SUPPORT
		if (!hw->m_ins_flag) {
			if (hw->low_latency_flag)
				decode_mode = DECODE_MODE_SINGLE_LOW_LATENCY;
			else
				decode_mode = DECODE_MODE_SINGLE;
		} else if (vdec_frame_based(hw_to_vdec(hw)))
			decode_mode = hw->no_head ?
				DECODE_MODE_MULTI_FRAMEBASE_NOHEAD :
				DECODE_MODE_MULTI_FRAMEBASE;
		else
			decode_mode = DECODE_MODE_MULTI_STREAMBASE;
		if (debug & AOM_DEBUG_BUFMGR_ONLY)
			decode_mode |= (1 << 16);
		WRITE_VREG(DECODE_MODE, decode_mode);
		WRITE_VREG(HEVC_DECODE_SIZE, 0);
		WRITE_VREG(HEVC_DECODE_COUNT, 0);
#else
	WRITE_VREG(DECODE_MODE, DECODE_MODE_SINGLE);
	WRITE_VREG(HEVC_DECODE_PIC_BEGIN_REG, 0);
	WRITE_VREG(HEVC_DECODE_PIC_NUM_REG, 0x7fffffff); /*to remove*/
#endif
	/*Send parser_cmd*/
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
	}

	if (mask & HW_MASK_BACK) {
		/*Changed to Start MPRED in microcode*/
		WRITE_VREG(HEVCD_IPP_TOP_CNTL,
			(0 << 1) | /*enable ipp*/
			(1 << 0)   /*software reset ipp and mpp*/
		);
#ifdef CHANGE_REMOVED
		WRITE_VREG(HEVCD_IPP_TOP_CNTL,
			(1 << 1) | /*enable ipp*/
			(0 << 0)   /*software reset ipp and mpp*/
		);
#else
		WRITE_VREG(HEVCD_IPP_TOP_CNTL,
			(3 << 4) | // av1
			(1 << 1) | /*enable ipp*/
			(0 << 0)   /*software reset ipp and mpp*/
		);
#endif
	if (get_double_write_mode(hw) & 0x10) {
		/*Enable NV21 reference read mode for MC*/
		WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);
	}
#ifdef MCRCC_ENABLE
		/*Initialize mcrcc and decomp perf counters*/
		if (mcrcc_cache_alg_flag &&
			hw->init_flag == 0) {
			mcrcc_perfcount_reset(hw);
			decomp_perfcount_reset(hw);
		}
#endif
	}
#ifdef CHANGE_REMOVED
#else
// Set MCR fetch priorities
    data32 = 0x1 | (0x1 << 2) | (0x1 <<3) |
	(24 << 4) | (32 << 11) | (24 << 18) | (32 << 25);
    WRITE_VREG(HEVCD_MPP_DECOMP_AXIURG_CTL, data32);
#endif
	return;
}


#ifdef CONFIG_HEVC_CLK_FORCED_ON
static void config_av1_clk_forced_on(void)
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


static int vav1_mmu_map_alloc(struct AV1HW_s *hw)
{
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
		(get_double_write_mode(hw) != 0x10)) {
		u32 mmu_map_size = vav1_frame_mmu_map_size(hw);
		hw->frame_mmu_map_addr =
			decoder_dma_alloc_coherent(&hw->frame_mmu_map_handle,
				mmu_map_size,
				&hw->frame_mmu_map_phy_addr, "AV1_MMU_MAP");
		if (hw->frame_mmu_map_addr == NULL) {
			pr_err("%s: failed to alloc count_buffer\n", __func__);
			return -1;
		}
		memset(hw->frame_mmu_map_addr, 0, mmu_map_size);
	}
#ifdef AOM_AV1_MMU_DW
	if (get_double_write_mode(hw) & 0x20) {
		u32 mmu_map_size = vaom_dw_frame_mmu_map_size(hw);
		hw->dw_frame_mmu_map_addr =
			decoder_dma_alloc_coherent(&hw->frame_dw_mmu_map_handle,
				mmu_map_size,
				&hw->dw_frame_mmu_map_phy_addr, "AV1_DWMMU_MAP");
		if (hw->dw_frame_mmu_map_addr == NULL) {
			pr_err("%s: failed to alloc count_buffer\n", __func__);
			return -1;
		}
		memset(hw->dw_frame_mmu_map_addr, 0, mmu_map_size);
	}
#endif
	return 0;
}

static void vav1_mmu_map_free(struct AV1HW_s *hw)
{
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
		(get_double_write_mode(hw) != 0x10)) {
		u32 mmu_map_size = vav1_frame_mmu_map_size(hw);
		if (hw->frame_mmu_map_addr) {
			if (hw->frame_mmu_map_phy_addr)
				decoder_dma_free_coherent(hw->frame_mmu_map_handle,
					mmu_map_size,
					hw->frame_mmu_map_addr,
					hw->frame_mmu_map_phy_addr);
			hw->frame_mmu_map_addr = NULL;
		}
	}
#ifdef AOM_AV1_MMU_DW
	if (get_double_write_mode(hw) & 0x20) {
		u32 mmu_map_size = vaom_dw_frame_mmu_map_size(hw);
		if (hw->dw_frame_mmu_map_addr) {
			if (hw->dw_frame_mmu_map_phy_addr)
				decoder_dma_free_coherent(hw->frame_dw_mmu_map_handle,
					mmu_map_size,
					hw->dw_frame_mmu_map_addr,
					hw->dw_frame_mmu_map_phy_addr);
			hw->dw_frame_mmu_map_addr = NULL;
		}
	}
#endif
}


static void av1_local_uninit(struct AV1HW_s *hw, bool reset_flag)
{
	hw->rpm_ptr = NULL;
	hw->lmem_ptr = NULL;

	if (!reset_flag) {
		hw->fg_ptr = NULL;
		if (hw->fg_addr) {
			if (hw->fg_phy_addr)
				codec_mm_dma_free_coherent(hw->fg_table_handle);
			hw->fg_addr = NULL;
		}
	}

	if (hw->rpm_addr) {
		decoder_dma_free_coherent(hw->rpm_mem_handle,
					RPM_BUF_SIZE,
					hw->rpm_addr,
					hw->rpm_phy_addr);
		hw->rpm_addr = NULL;
	}
	if (hw->aux_addr) {
		decoder_dma_free_coherent(hw->aux_mem_handle,
				hw->prefix_aux_size + hw->suffix_aux_size, hw->aux_addr,
					hw->aux_phy_addr);
		hw->aux_addr = NULL;
	}
#if (defined DEBUG_UCODE_LOG) || (defined DEBUG_CMD)
	if (hw->ucode_log_addr) {
		decoder_dma_free_coherent(hw->ucode_log_handle,
				UCODE_LOG_BUF_SIZE, hw->ucode_log_addr,
					hw->ucode_log_phy_addr);
		hw->ucode_log_addr = NULL;
	}
#endif
	if (hw->lmem_addr) {
		if (hw->lmem_phy_addr)
			decoder_dma_free_coherent(hw->lmem_phy_handle,
				LMEM_BUF_SIZE, hw->lmem_addr,
				hw->lmem_phy_addr);
		hw->lmem_addr = NULL;
	}

	vav1_mmu_map_free(hw);

	if (hw->gvs)
		vfree(hw->gvs);
	hw->gvs = NULL;
}

static int av1_local_init(struct AV1HW_s *hw, bool reset_flag)
{
	int ret = -1;

	struct BuffInfo_s *cur_buf_info = NULL;

	memset(&hw->param, 0, sizeof(union param_u));
#ifdef MULTI_INSTANCE_SUPPORT
	cur_buf_info = &hw->work_space_buf_store;
    hw->pbi->work_space_buf = cur_buf_info;

	memcpy(cur_buf_info, &aom_workbuff_spec[hw->buffer_spec_index],
		sizeof(struct BuffInfo_s));

	cur_buf_info->start_adr = hw->buf_start;
	if ((get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_GXL) ||
		(get_double_write_mode(hw) == 0x10))
		hw->mc_buf_spec.buf_end = hw->buf_start + hw->buf_size;

#else
	memcpy(cur_buf_info, &aom_workbuff_spec[hw->buffer_spec_index],
		sizeof(struct BuffInfo_s));
#endif

	init_buff_spec(hw, cur_buf_info);
	aom_bufmgr_init(hw, cur_buf_info, NULL);

	if (!vdec_is_support_4k()
		&& (buf_alloc_width > 1920 &&  buf_alloc_height > 1088)) {
		buf_alloc_width = 1920;
		buf_alloc_height = 1088;
		if (hw->max_pic_w > 1920 && hw->max_pic_h > 1088) {
			hw->max_pic_w = 1920;
			hw->max_pic_h = 1088;
		}
	} else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) {
		buf_alloc_width = 8192;
		buf_alloc_height = 4608;
	}

	hw->init_pic_w = hw->max_pic_w ? hw->max_pic_w :
		(hw->vav1_amstream_dec_info.width ? hw->vav1_amstream_dec_info.width :
		(buf_alloc_width ? buf_alloc_width : hw->work_space_buf->max_width));
	hw->init_pic_h = hw->max_pic_h ? hw->max_pic_h :
		(hw->vav1_amstream_dec_info.height ? hw->vav1_amstream_dec_info.height :
		(buf_alloc_height ? buf_alloc_height : hw->work_space_buf->max_height));

    hw->pbi->frame_width = hw->init_pic_w;
    hw->pbi->frame_height = hw->init_pic_h;

	/* video is not support unaligned with 64 in tl1
	** vdec canvas mode will be linear when dump yuv is set
	*/
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
		(get_double_write_mode(hw) != 0) &&
		(((hw->max_pic_w % 64) != 0) ||
		(hw->vav1_amstream_dec_info.width % 64) != 0)) {
		if (hw_to_vdec(hw)->canvas_mode !=
			CANVAS_BLKMODE_LINEAR)
			hw->mem_map_mode = 2;
		else {
			hw->mem_map_mode = 0;
			av1_print(hw, AOM_DEBUG_HW_MORE, "vdec blkmod linear, force mem_map_mode 0\n");
		}
	}

	hw->mv_buf_margin = mv_buf_margin;

	hw->pts_unstable = ((unsigned long)(hw->vav1_amstream_dec_info.param)
			& 0x40) >> 6;

	if ((debug & AOM_AV1_DEBUG_SEND_PARAM_WITH_REG) == 0) {
		hw->rpm_addr = decoder_dma_alloc_coherent(&hw->rpm_mem_handle,
				RPM_BUF_SIZE,
				&hw->rpm_phy_addr, "AV1_RPM_BUF");
		if (hw->rpm_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}
		hw->rpm_ptr = hw->rpm_addr;
	}

	if (prefix_aux_buf_size > 0 ||
		suffix_aux_buf_size > 0) {
		u32 aux_buf_size;

		hw->prefix_aux_size = AUX_BUF_ALIGN(prefix_aux_buf_size);
		hw->suffix_aux_size = AUX_BUF_ALIGN(suffix_aux_buf_size);
		aux_buf_size = hw->prefix_aux_size + hw->suffix_aux_size;
		hw->aux_addr = decoder_dma_alloc_coherent(&hw->aux_mem_handle,
				aux_buf_size, &hw->aux_phy_addr, "AV1_AUX_BUF");
		if (hw->aux_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			goto dma_alloc_fail;
		}
	}
#if (defined DEBUG_UCODE_LOG) || (defined DEBUG_CMD)
	hw->ucode_log_addr = decoder_dma_alloc_coherent(&hw->ucode_log_handle,
			UCODE_LOG_BUF_SIZE, &hw->ucode_log_phy_addr,  "UCODE_LOG_BUF");
	if (hw->ucode_log_addr == NULL) {
		hw->ucode_log_phy_addr = 0;
	}
	pr_info("%s: alloc ucode log buffer %p\n",
		__func__, hw->ucode_log_addr);
#endif

	if (!reset_flag) {
		int alloc_num = 1;
		if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_SC2) ||
			(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3) ||
			(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) ||
			(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5W)) {
			alloc_num = FRAME_BUFFERS;
		}
		hw->fg_addr = codec_mm_dma_alloc_coherent(&hw->fg_table_handle,
				(ulong *)&hw->fg_phy_addr,FGS_TABLE_SIZE * alloc_num, MEM_NAME);
		if (hw->fg_addr == NULL) {
			pr_err("%s: failed to alloc fg buffer\n", __func__);
		}
		hw->fg_ptr = hw->fg_addr;
		pr_info("%s, alloc fg table addr %lx, size 0x%x\n", __func__,
			(ulong)hw->fg_phy_addr, FGS_TABLE_SIZE * alloc_num);
	}
	if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5W)) {
		cur_buf_info->fgs_table.buf_start = hw->fg_phy_addr;
	}

	hw->lmem_addr = decoder_dma_alloc_coherent(&hw->lmem_phy_handle,
			LMEM_BUF_SIZE,
			&hw->lmem_phy_addr, "LMEM_BUF");
	if (hw->lmem_addr == NULL) {
		pr_err("%s: failed to alloc lmem buffer\n", __func__);
		goto dma_alloc_fail;
	}
	hw->lmem_ptr = hw->lmem_addr;

	vdec_set_vframe_comm(hw_to_vdec(hw), DRIVER_NAME);
	ret = vav1_mmu_map_alloc(hw);
	if (ret < 0)
		goto dma_alloc_fail;

	return ret;

dma_alloc_fail:
	av1_local_uninit(hw, reset_flag);
	return -1;
}


#define spec2canvas(x)  \
	(((x)->uv_canvas_index << 16) | \
	 ((x)->uv_canvas_index << 8)  | \
	 ((x)->y_canvas_index << 0))


static void set_canvas(struct AV1HW_s *hw,
	struct PIC_BUFFER_CONFIG_s *pic_config)
{
	struct vdec_s *vdec = hw_to_vdec(hw);
	int canvas_w = ALIGN(pic_config->y_crop_width, 64)/4;
	int canvas_h = ALIGN(pic_config->y_crop_height, 32)/4;
	int blkmode = hw->mem_map_mode;
	/*CANVAS_BLKMODE_64X32*/
	if	(pic_config->double_write_mode) {
		if (pic_config->double_write_mode == 0x21) {
			canvas_w = pic_config->y_crop_width / 4;
			canvas_h = pic_config->y_crop_height / 4;
			av1_print(hw, AV1_DEBUG_BUFMGR_DETAIL,"%s use double_write_mode = 0x21!\n",__func__);
		} else {
			canvas_w = pic_config->y_crop_width	/
				get_double_write_ratio(
					pic_config->double_write_mode & 0xf);
			canvas_h = pic_config->y_crop_height /
					get_double_write_ratio(
						pic_config->double_write_mode & 0xf);
		}
		/* sao ctrl1 config aligned with 64, so aligned with 64 same */
		canvas_w = ALIGN(canvas_w, 64);
		canvas_h = ALIGN(canvas_h, 32);

		if (vdec->parallel_dec == 1) {
			if (pic_config->y_canvas_index == -1)
				pic_config->y_canvas_index =
					vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
			if (pic_config->uv_canvas_index == -1)
				pic_config->uv_canvas_index =
					vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
		} else {
			pic_config->y_canvas_index = 128 + pic_config->index * 2;
			pic_config->uv_canvas_index = 128 + pic_config->index * 2 + 1;
		}

		config_cav_lut_ex(pic_config->y_canvas_index,
			pic_config->dw_y_adr, canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0, VDEC_HEVC);
		config_cav_lut_ex(pic_config->uv_canvas_index,
			pic_config->dw_u_v_adr,	canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0, VDEC_HEVC);

#ifdef MULTI_INSTANCE_SUPPORT
		pic_config->canvas_config[0].phy_addr =
				pic_config->dw_y_adr;
		pic_config->canvas_config[0].width =
				canvas_w;
		pic_config->canvas_config[0].height =
				canvas_h;
		pic_config->canvas_config[0].block_mode =
				blkmode;
		pic_config->canvas_config[0].endian = 0;

		pic_config->canvas_config[1].phy_addr =
				pic_config->dw_u_v_adr;
		pic_config->canvas_config[1].width =
				canvas_w;
		pic_config->canvas_config[1].height =
				canvas_h;
		pic_config->canvas_config[1].block_mode =
				blkmode;
		pic_config->canvas_config[1].endian = 0;
#endif
	}
}

#define OBU_METADATA_TYPE_ITUT_T35 4

void parse_metadata(struct AV1HW_s *hw, struct vframe_s *vf, struct PIC_BUFFER_CONFIG_s *pic)
{
	if (pic->aux_data_buf && pic->aux_data_size) {
		u32 size = 0, type = 0;
		char *p = pic->aux_data_buf;

		/* parser metadata */
		while (p + 8 < pic->aux_data_buf + pic->aux_data_size) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (p + size <= pic->aux_data_buf + pic->aux_data_size) {
				char metadata_type = ((type >> 24) & 0xff) - 0x10;

				switch (metadata_type) {
				case OBU_METADATA_TYPE_ITUT_T35:
					if ((p + 5 < pic->aux_data_buf + pic->aux_data_size) &&
						p[0] == 0xB5 && p[1] == 0x00 && p[2] == 0x3C &&
						p[3] == 0x00 && p[4] == 0x01 && p[5] == 0x04) {
						u32 data;
						data = hw->video_signal_type;
						data = data & 0xFFFF00FF;
						data = data | (0x30<<8);
						hw->video_signal_type = data;
						vf->discard_dv_data = true;
					}
					break;
				default:
					break;
				}
			}
			p += size;
		}
	}
}

static void set_frame_info(struct AV1HW_s *hw, struct vframe_s *vf, struct PIC_BUFFER_CONFIG_s *pic)
{
	unsigned int ar;

	parse_metadata(hw, vf, pic);

	vf->duration = hw->frame_dur;
	vf->duration_pulldown = 0;
	vf->flag = 0;
	vf->prop.master_display_colour = hw->vf_dp;
	vf->signal_type = hw->video_signal_type;
	if (vf->compWidth && vf->compHeight)
		hw->frame_ar = vf->compHeight * 0x100 / vf->compWidth;
	ar = min_t(u32, hw->frame_ar, DISP_RATIO_ASPECT_RATIO_MAX);
	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);

	if (vf->signal_type != 0) {
		struct aml_vdec_hdr_infos hdr;
		struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hw->v4l2_ctx);

		memset(&hdr, 0, sizeof(hdr));
		hdr.signal_type = vf->signal_type;
		hdr.color_parms = hw->vf_dp;
		hdr.color_parms.luminance[0] = hdr.color_parms.luminance[0] / 1000;
		vdec_v4l_set_hdr_infos(ctx, &hdr);
	}

	vf->sidebind_type = hw->sidebind_type;
	vf->sidebind_channel_id = hw->sidebind_channel_id;
}

static int vav1_vf_states(struct vframe_states *states, void *op_arg)
{
	struct AV1HW_s *hw = (struct AV1HW_s *)op_arg;

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hw->newframe_q);
	states->buf_avail_num = kfifo_len(&hw->display_q);

	if (step == 2)
		states->buf_avail_num = 0;
	return 0;
}

static struct vframe_s *vav1_vf_peek(void *op_arg)
{
	struct vframe_s *vf[2] = {0, 0};
	struct AV1HW_s *hw = (struct AV1HW_s *)op_arg;

	if (step == 2)
		return NULL;

	if (kfifo_out_peek(&hw->display_q, (void *)&vf, 2)) {
		if (vf[1]) {
			vf[0]->next_vf_pts_valid = true;
			vf[0]->next_vf_pts = vf[1]->pts;
		} else
			vf[0]->next_vf_pts_valid = false;
		return vf[0];
	}

	return NULL;
}

static struct vframe_s *vav1_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct AV1HW_s *hw = (struct AV1HW_s *)op_arg;

	if (step == 2)
		return NULL;
	else if (step == 1)
			step = 2;

	if (kfifo_get(&hw->display_q, &vf)) {
		struct vframe_s *next_vf = NULL;
		uint8_t index = vf->index & 0xff;
		ATRACE_COUNTER(hw->trace.disp_q_name, kfifo_len(&hw->display_q));
		if (index < hw->used_buf_num ||
			(vf->type & VIDTYPE_V4L_EOS)) {
			vf->index_disp =  atomic_read(&hw->vf_get_count);
			vf->omx_index = atomic_read(&hw->vf_get_count);
			atomic_add(1, &hw->vf_get_count);
			if (debug & AOM_DEBUG_VFRAME) {
				struct BufferPool_s *pool = hw->common.buffer_pool;
				struct PIC_BUFFER_CONFIG_s *pic =
					&pool->frame_bufs[index].buf;
				unsigned long flags;
				lock_buffer_pool(hw->common.buffer_pool, flags);
				av1_print(hw, AOM_DEBUG_VFRAME, "%s index 0x%x type 0x%x w/h %d/%d, aux size %d, pts %d, %lld, ts: %llu\n",
					__func__, vf->index, vf->type,
					vf->width, vf->height,
					pic->aux_data_size,
					vf->pts,
					vf->pts_us64,
					vf->timestamp);
				unlock_buffer_pool(hw->common.buffer_pool, flags);
			}

			if (kfifo_peek(&hw->display_q, &next_vf) && next_vf) {
				vf->next_vf_pts_valid = true;
				vf->next_vf_pts = next_vf->pts;
			} else
				vf->next_vf_pts_valid = false;
#ifdef DUMP_FILMGRAIN
			if (index == fg_dump_index) {
				unsigned long flags;
				int ii;
				lock_buffer_pool(hw->common.buffer_pool, flags);
				pr_info("FGS_TABLE for buffer %d:\n", index);
				for (ii = 0; ii < FGS_TABLE_SIZE; ii++) {
			        pr_info("%02x ", hw->fg_ptr[ii]);
	        		if (((ii+ 1) & 0xf) == 0)
						pr_info("\n");
	    		}
				unlock_buffer_pool(hw->common.buffer_pool, flags);
			}
#endif
			return vf;
		}
	}
	return NULL;
}

static void vav1_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct AV1HW_s *hw = (struct AV1HW_s *)op_arg;
	uint8_t index = vf->index & 0xff;
	unsigned long flags;

	if ((vf == NULL) || (hw == NULL))
		return;

	kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
	ATRACE_COUNTER(hw->trace.new_q_name, kfifo_len(&hw->newframe_q));
	atomic_add(1, &hw->vf_put_count);
	if (debug & AOM_DEBUG_VFRAME) {
		lock_buffer_pool(hw->common.buffer_pool, flags);
		av1_print(hw, AOM_DEBUG_VFRAME, "%s vf %px index 0x%x type 0x%x w/h %d/%d, pts %d, %lld, ts: %llu\n",
			__func__, vf, vf->index, vf->type,
			vf->width, vf->height,
			vf->pts,
			vf->pts_us64,
			vf->timestamp);
		unlock_buffer_pool(hw->common.buffer_pool, flags);
	}

	if (index < hw->used_buf_num) {
		struct AV1_Common_s *cm = &hw->common;
		struct BufferPool_s *pool = cm->buffer_pool;
		PIC_BUFFER_CONFIG *pic = &pool->frame_bufs[index].buf;

		if (vf->v4l_mem_handle !=
			hw->m_BUF[pic->BUF_index].v4l_ref_buf_addr) {
			av1_print(hw, PRINT_FLAG_V4L_DETAIL,
				"AV1 update fb handle, old:%llx, new:%llx\n",
				hw->m_BUF[pic->BUF_index].v4l_ref_buf_addr,
				vf->v4l_mem_handle);

			hw->m_BUF[pic->BUF_index].v4l_ref_buf_addr =
				vf->v4l_mem_handle;
		}

		lock_buffer_pool(hw->common.buffer_pool, flags);
		if (pic->repeat_pic) {
			if (pic->repeat_pic->repeat_count > 0)
				pic->repeat_pic->repeat_count --;
			else
				av1_print(hw, PRINT_FLAG_ERROR, "repeat_count <= 0 pic:%px\n", pic);
			pic->repeat_pic = NULL;
		}
		if ((debug & AV1_DEBUG_IGNORE_VF_REF) == 0) {
			if (pool->frame_bufs[index].buf.vf_ref > 0)
				pool->frame_bufs[index].buf.vf_ref--;
		}
		if (hw->wait_buf)
			WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);

		hw->last_put_idx = index;
		hw->new_frame_displayed++;

		unlock_buffer_pool(hw->common.buffer_pool, flags);

		spin_lock_irqsave(&hw->wait_buf_lock, flags);
		if (hw->wait_more_buf) {
			hw->wait_more_buf = false;
			hw->dec_result = AOM_AV1_RESULT_NEED_MORE_BUFFER;
			vdec_schedule_work(&hw->work);
		}
		spin_unlock_irqrestore(&hw->wait_buf_lock, flags);
	}
}

static int vav1_event_cb(int type, void *data, void *op_arg)
{
	unsigned long flags;
	struct AV1HW_s *hw = (struct AV1HW_s *)op_arg;
	struct AV1_Common_s *cm = &hw->common;
	struct BufferPool_s *pool = cm->buffer_pool;

	if (type & VFRAME_EVENT_RECEIVER_RESET) {

	} else if (type & VFRAME_EVENT_RECEIVER_GET_AUX_DATA) {
		struct provider_aux_req_s *req =
			(struct provider_aux_req_s *)data;
		unsigned char index;

		lock_buffer_pool(hw->common.buffer_pool, flags);
		index = req->vf->index & 0xff;
		req->aux_buf = NULL;
		req->aux_size = 0;
		if (req->bot_flag)
			index = (req->vf->index >> 8) & 0xff;
		if (index != 0xff
			&& index < hw->used_buf_num) {
			struct PIC_BUFFER_CONFIG_s *pic_config =
				&pool->frame_bufs[index].buf;
			req->aux_buf = pic_config->aux_data_buf;
			req->aux_size = pic_config->aux_data_size;
			req->dv_enhance_exist = 0;
		}
		unlock_buffer_pool(hw->common.buffer_pool, flags);

		if (debug & AOM_DEBUG_AUX_DATA)
			av1_print(hw, 0,
			"%s(type 0x%x vf index 0x%x)=>size 0x%x\n",
			__func__, type, index, req->aux_size);
	} else if (type & VFRAME_EVENT_RECEIVER_REQ_STATE) {
		struct provider_state_req_s *req =
			(struct provider_state_req_s *)data;
		if (req->req_type == REQ_STATE_SECURE)
			req->req_result[0] = vdec_secure(hw_to_vdec(hw));
		else
			req->req_result[0] = 0xffffffff;
	}
	return 0;
}

void av1_inc_vf_ref(struct AV1HW_s *hw, int index)
{
	struct AV1_Common_s *cm = &hw->common;

	if ((debug & AV1_DEBUG_IGNORE_VF_REF) == 0) {
		cm->buffer_pool->frame_bufs[index].buf.vf_ref++;

		av1_print(hw, AV1_DEBUG_BUFMGR, "%s index = %d new vf_ref = %d\r\n",
			__func__, index,
			cm->buffer_pool->frame_bufs[index].buf.vf_ref);
	}
}

static inline void av1_update_gvs(struct AV1HW_s *hw, struct vframe_s *vf,
				  struct PIC_BUFFER_CONFIG_s *pic_config)
{
	if (hw->gvs->frame_height != pic_config->y_crop_height) {
		hw->gvs->frame_width = pic_config->y_crop_width;
		hw->gvs->frame_height = pic_config->y_crop_height;
	}
	if (hw->gvs->frame_dur != hw->frame_dur) {
		hw->gvs->frame_dur = hw->frame_dur;
		if (hw->frame_dur != 0)
			hw->gvs->frame_rate = ((96000 * 10 / hw->frame_dur) % 10) < 5 ?
					96000 / hw->frame_dur : (96000 / hw->frame_dur +1);
		else
			hw->gvs->frame_rate = -1;
	}
	if (vf && hw->gvs->ratio_control != vf->ratio_control)
		hw->gvs->ratio_control = vf->ratio_control;

	hw->gvs->status = hw->stat | hw->fatal_error;
	hw->gvs->error_count = hw->gvs->error_frame_count;

}

static int prepare_display_buf(struct AV1HW_s *hw,
				struct PIC_BUFFER_CONFIG_s *pic_config)
{
	struct vframe_s *vf = NULL;
	struct vdec_info tmp4x;
	int stream_offset = pic_config->stream_offset;
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;
	struct vdec_v4l2_buffer *fb = NULL;
	struct vdec_s *vdec = hw_to_vdec(hw);
	ulong nv_order = VIDTYPE_VIU_NV21;
	u32 pts_valid = 0, pts_us64_valid = 0;
	u32 frame_size;
	int i, reclac_flag = 0;

	av1_print(hw, AOM_DEBUG_VFRAME, "%s index = %d\r\n", __func__, pic_config->index);
	if (kfifo_get(&hw->newframe_q, &vf) == 0) {
		av1_print(hw, 0, "fatal error, no available buffer slot.");
		return -1;
	}

	/* swap uv */
	if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12) ||
		(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12M))
		nv_order = VIDTYPE_VIU_NV12;

	if (pic_config->double_write_mode &&
		(pic_config->double_write_mode & 0x20) == 0)
		set_canvas(hw, pic_config);

	display_frame_count[hw->index]++;
	if (vf) {
		if (!force_pts_unstable && hw->av1_first_pts_ready) {
			if ((pic_config->pts == 0) || ((pic_config->pts <= hw->last_pts) &&
				(pic_config->pts64 <= hw->last_pts_us64))) {
				for (i = (FRAME_BUFFERS - 1); i > 0; i--) {
					if ((hw->last_pts == hw->frame_mode_pts_save[i]) ||
						(hw->last_pts_us64 == hw->frame_mode_pts64_save[i])) {
						pic_config->pts = hw->frame_mode_pts_save[i - 1];
						pic_config->pts64 = hw->frame_mode_pts64_save[i - 1];
						break;
					}
				}

				if ((i == 0) || (pic_config->pts <= hw->last_pts)) {
					av1_print(hw, AV1_DEBUG_OUT_PTS,
						"no found pts %d, set 0. %d, %d\n",
						i, pic_config->pts, hw->last_pts);
					pic_config->pts = 0;
					pic_config->pts64 = 0;
				}
			}
		}

		vf->v4l_mem_handle
			= hw->m_BUF[pic_config->v4l_buf_index].v4l_ref_buf_addr;
		fb = (struct vdec_v4l2_buffer *)vf->v4l_mem_handle;
		if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
			(get_double_write_mode(hw) != 0x10)) {
			vf->mm_box.bmmu_box	= hw->bmmu_box;
			vf->mm_box.bmmu_idx	= HEADER_BUFFER_IDX(hw->buffer_wrap[pic_config->v4l_buf_index]);
			vf->mm_box.mmu_box	= hw->mmu_box;
			vf->mm_box.mmu_idx	= hw->buffer_wrap[pic_config->v4l_buf_index];
		}

#ifdef MULTI_INSTANCE_SUPPORT
		if (vdec_frame_based(hw_to_vdec(hw))) {
			vf->pts = pic_config->pts;
			vf->pts_us64 = pic_config->pts64;

			if (v4l_bitstream_id_enable)
				vf->timestamp = pic_config->timestamp;
			else
				vf->timestamp = pic_config->pts64;

			if (vf->pts != 0 || vf->pts_us64 != 0) {
				pts_valid = 1;
				pts_us64_valid = 1;
			} else {
				pts_valid = 0;
				pts_us64_valid = 0;
			}
		} else
#endif
		if (pts_lookup_offset_us64
			(PTS_TYPE_VIDEO, stream_offset, &vf->pts,
			&frame_size, 0,
			&vf->pts_us64) != 0) {
#ifdef DEBUG_PTS
			hw->pts_missed++;
#endif
			vf->pts = 0;
			vf->pts_us64 = 0;
			pts_valid = 0;
			pts_us64_valid = 0;
		} else {
#ifdef DEBUG_PTS
			hw->pts_hit++;
#endif
			pts_valid = 1;
			pts_us64_valid = 1;
		}

		if (hw->av1_first_pts_ready) {
			if (hw->frame_dur && ((vf->pts == 0) || (vf->pts_us64 == 0))) {
				vf->pts = hw->last_pts + DUR2PTS(hw->frame_dur);
				vf->pts_us64 = hw->last_pts_us64 +
					(DUR2PTS(hw->frame_dur) * 100 / 9);
				reclac_flag = 1;
			}

			if (!close_to(vf->pts, (hw->last_pts + DUR2PTS(hw->frame_dur)), 100)) {
				vf->pts = hw->last_pts + DUR2PTS(hw->frame_dur);
				vf->pts_us64 = hw->last_pts_us64 +
					(DUR2PTS(hw->frame_dur) * 100 / 9);
				reclac_flag = 2;
			}

			reclac_flag = 0;

			/* try find the closed pts in saved pts pool */
			if (reclac_flag) {
				for (i = 0; i < FRAME_BUFFERS - 1; i++) {
					if ((hw->frame_mode_pts_save[i] > vf->pts) &&
						(hw->frame_mode_pts_save[i + 1] < vf->pts)) {
						if ((hw->frame_mode_pts_save[i] - vf->pts) >
							(vf->pts - hw->frame_mode_pts_save[i + 1])) {
							vf->pts = hw->frame_mode_pts_save[i + 1];
							vf->pts_us64 = hw->frame_mode_pts64_save[i + 1];
						} else {
							vf->pts = hw->frame_mode_pts_save[i];
							vf->pts_us64 = hw->frame_mode_pts64_save[i];
						}
						break;
					}
				}
				if (i == (FRAME_BUFFERS - 1))
					hw->dur_recalc_flag = 1;
			}
		} else {
			av1_print(hw, AV1_DEBUG_OUT_PTS,
				"first pts %d change to save[%d] %d\n",
				vf->pts, hw->first_pts_index - 1,
				hw->frame_mode_pts_save[hw->first_pts_index - 1]);
			vf->pts = hw->frame_mode_pts_save[hw->first_pts_index - 1];
			vf->pts_us64 = hw->frame_mode_pts64_save[hw->first_pts_index - 1];
		}
		hw->last_pts = vf->pts;
		hw->last_pts_us64 = vf->pts_us64;
		hw->av1_first_pts_ready = true;
		av1_print(hw, AV1_DEBUG_OUT_PTS,
			"av1 output slice type %d, dur %d, pts %d, pts64 %lld, ts: %llu\n",
			pic_config->slice_type, hw->frame_dur, vf->pts, vf->pts_us64, vf->timestamp);

		fill_frame_info(hw, pic_config, frame_size, vf->pts);

		vf->index = 0xff00 | pic_config->v4l_buf_index;

		if (pic_config->double_write_mode & 0x10) {
			/* double write only */
			vf->compBodyAddr = 0;
			vf->compHeadAddr = 0;
#ifdef AOM_AV1_MMU_DW
			vf->dwBodyAddr = 0;
			vf->dwHeadAddr = 0;
#endif
		} else {
			if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
				(get_double_write_mode(hw) != 0x10)) {
				vf->compBodyAddr = 0;
				vf->compHeadAddr = pic_config->header_adr;
				if (((get_debug_fgs() & DEBUG_FGS_BYPASS) == 0)
					&& hw->assit_task.use_sfgs)
					vf->fgs_table_adr = pic_config->sfgs_table_phy;
				else
					vf->fgs_table_adr = pic_config->fgs_table_adr;
				vf->fgs_valid = hw->fgs_valid;
#ifdef AOM_AV1_MMU_DW
				vf->dwBodyAddr = 0;
				vf->dwHeadAddr = 0;
				if (pic_config->double_write_mode & 0x20) {
					u32 mode = pic_config->double_write_mode & 0xf;
					if (mode == 5 || mode == 3)
						vf->dwHeadAddr = pic_config->header_dw_adr;
					else if ((mode == 1 || mode == 2 || mode == 4)
					&& (debug & AOM_DEBUG_DW_DISP_MAIN) == 0) {
					vf->compHeadAddr = pic_config->header_dw_adr;
					vf->fgs_valid = 0;
					av1_print(hw, AOM_DEBUG_VFRAME,
						"Use dw mmu for display\n");
					}
				}
#endif
			}
			vf->canvas0Addr = vf->canvas1Addr = 0;
		}
		if (pic_config->double_write_mode &&
			(pic_config->double_write_mode & 0x20) == 0) {
			vf->type = VIDTYPE_PROGRESSIVE |
				VIDTYPE_VIU_FIELD;
			vf->type |= nv_order;
			if ((pic_config->double_write_mode == 3 ||
				pic_config->double_write_mode == 5) &&
				(!IS_8K_SIZE(pic_config->y_crop_width,
				pic_config->y_crop_height))) {
				vf->type |= VIDTYPE_COMPRESS;
				if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
					(get_double_write_mode(hw) != 0x10)) {
					vf->type |= VIDTYPE_SCATTER;
				}
			}
			if (pic_config->double_write_mode != 16 &&
				(!IS_8K_SIZE(pic_config->y_crop_width,
				pic_config->y_crop_height))) {
				if ((get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_S4 &&
					get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_S4D)
					|| (!hw->film_grain_present) || (get_double_write_mode(hw) != 1)) {
					vf->type |= VIDTYPE_COMPRESS | VIDTYPE_SCATTER;
				}
			}
#ifdef MULTI_INSTANCE_SUPPORT
			if (hw->m_ins_flag) {
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
			if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
				(get_double_write_mode(hw) != 0x10)) {
				vf->type |= VIDTYPE_SCATTER;
			}
		}

		if (pic_config->double_write_mode == 0x21) {
#ifdef MULTI_INSTANCE_SUPPORT
			if (hw->m_ins_flag) {
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
		}

		switch (pic_config->bit_depth) {
		case AOM_BITS_8:
			vf->bitdepth = BITDEPTH_Y8 |
				BITDEPTH_U8 | BITDEPTH_V8;
			break;
		case AOM_BITS_10:
		case AOM_BITS_12:
			vf->bitdepth = BITDEPTH_Y10 |
				BITDEPTH_U10 | BITDEPTH_V10;
			break;
		default:
			vf->bitdepth = BITDEPTH_Y10 |
				BITDEPTH_U10 | BITDEPTH_V10;
			break;
		}
		if ((vf->type & VIDTYPE_COMPRESS) == 0)
			vf->bitdepth =
				BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		if (pic_config->bit_depth == AOM_BITS_8)
			vf->bitdepth |= BITDEPTH_SAVING_MODE;

		vf->width = pic_config->y_crop_width /
			get_double_write_ratio(
				pic_config->double_write_mode & 0xf);
		vf->height = pic_config->y_crop_height /
			get_double_write_ratio(
				pic_config->double_write_mode & 0xf);
		if (force_w_h != 0) {
			vf->width = (force_w_h >> 16) & 0xffff;
			vf->height = force_w_h & 0xffff;
		}
		if ((pic_config->double_write_mode & 0x20) &&
			((pic_config->double_write_mode & 0xf) == 2 ||
			(pic_config->double_write_mode & 0xf) == 4)) {
			vf->compWidth = pic_config->y_crop_width /
				get_double_write_ratio(
					pic_config->double_write_mode & 0xf);
			vf->compHeight = pic_config->y_crop_height /
				get_double_write_ratio(
					pic_config->double_write_mode & 0xf);
		} else {
			vf->compWidth = pic_config->y_crop_width;
			vf->compHeight = pic_config->y_crop_height;
		}
		set_frame_info(hw, vf, pic_config);

		if (hw->high_bandwidth_flag) {
			vf->flag |= VFRAME_FLAG_HIGH_BANDWIDTH;
		}

		if (force_fps & 0x100) {
			u32 rate = force_fps & 0xff;

			if (rate)
				vf->duration = 96000/rate;
			else
				vf->duration = 0;
		}

		vf->src_fmt.play_id = vdec->inst_cnt;

		av1_inc_vf_ref(hw, pic_config->v4l_buf_index);
		vdec_vframe_ready(hw_to_vdec(hw), vf);
		if (pic_config->v4l_buf_index != pic_config->BUF_index)	{
			struct PIC_BUFFER_CONFIG_s *dst_pic =
				&hw->common.buffer_pool->frame_bufs[pic_config->v4l_buf_index].buf;
			struct PIC_BUFFER_CONFIG_s *src_pic =
				&hw->common.buffer_pool->frame_bufs[pic_config->BUF_index].buf;
			struct vdec_ge2d_info ge2d_info;

			ge2d_info.dst_vf = vf;
			ge2d_info.src_canvas0Addr = ge2d_info.src_canvas1Addr = 0;
			if (dst_pic->double_write_mode) {
#ifdef MULTI_INSTANCE_SUPPORT
				if (hw->m_ins_flag) {
					vf->canvas0Addr = vf->canvas1Addr = -1;
					ge2d_info.src_canvas0Addr = ge2d_info.src_canvas1Addr = -1;
					vf->plane_num = 2;
					vf->canvas0_config[0] =
						dst_pic->canvas_config[0];
					vf->canvas0_config[1] =
						dst_pic->canvas_config[1];
					vf->canvas1_config[0] =
						dst_pic->canvas_config[0];
					vf->canvas1_config[1] =
						dst_pic->canvas_config[1];
					ge2d_info.src_canvas0_config[0] =
						src_pic->canvas_config[0];
					ge2d_info.src_canvas0_config[1] =
						src_pic->canvas_config[1];
					ge2d_info.src_canvas1_config[0] =
						src_pic->canvas_config[0];
					ge2d_info.src_canvas1_config[1] =
						src_pic->canvas_config[1];
				} else
#endif
				{
					vf->canvas0Addr = vf->canvas1Addr =
						spec2canvas(dst_pic);
					ge2d_info.src_canvas0Addr = ge2d_info.src_canvas1Addr =
						spec2canvas(src_pic);
				}
			}

			if (!hw->ge2d) {
				int mode = nv_order == VIDTYPE_VIU_NV21 ? GE2D_MODE_CONVERT_NV21 : GE2D_MODE_CONVERT_NV12;
				mode |= GE2D_MODE_CONVERT_LE;
				vdec_ge2d_init(&hw->ge2d,  mode);
			}
			vdec_ge2d_copy_data(hw->ge2d, &ge2d_info);
		}
		decoder_do_frame_check(hw_to_vdec(hw), vf);
		kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
		ATRACE_COUNTER(hw->trace.pts_name, vf->timestamp);
		ATRACE_COUNTER(hw->trace.new_q_name, kfifo_len(&hw->newframe_q));
		ATRACE_COUNTER(hw->trace.disp_q_name, kfifo_len(&hw->display_q));

		atomic_add(1, &hw->vf_pre_count);
		/*count info*/
		hw->gvs->frame_dur = hw->frame_dur;
		vdec_count_info(hw->gvs, 0, stream_offset);
		hw_to_vdec(hw)->vdec_fps_detec(hw_to_vdec(hw)->id);

#ifdef AUX_DATA_CRC
		decoder_do_aux_data_check(hw_to_vdec(hw), pic_config->aux_data_buf,
			pic_config->aux_data_size);
#endif

		av1_print(hw, AV1_DEBUG_SEI_DETAIL,
			"%s aux_data_size:%d inst_cnt:%d vf:%p\n",
			__func__, pic_config->aux_data_size, vdec->inst_cnt, vf);

		if (debug & AV1_DEBUG_SEI_DETAIL) {
			int i = 0;
			PR_INIT(128);
			for (i = 0; i < pic_config->aux_data_size; i++) {
				PR_FILL("%02x ", pic_config->aux_data_buf[i]);
				if (((i + 1) & 0xf) == 0)
					PR_INFO(hw->index);
			}
			PR_INFO(hw->index);
		}

		if ((pic_config->aux_data_size == 0) &&
			(pic_config->slice_type == KEY_FRAME) &&
			(atomic_read(&hw->vf_pre_count) == 1)) {
			hw->no_need_aux_data = true;
		}

		if (hw->no_need_aux_data) {
			v4l2_ctx->aux_infos.free_buffer(v4l2_ctx, DV_TYPE);
			v4l2_ctx->aux_infos.free_one_sei_buffer(v4l2_ctx,
				&pic_config->aux_data_buf,
				&pic_config->aux_data_size,
				pic_config->ctx_buf_idx);
		} else {
			v4l2_ctx->aux_infos.bind_dv_buffer(v4l2_ctx, &vf->src_fmt.comp_buf,
				&vf->src_fmt.md_buf);
		}

		update_vframe_src_fmt(vf,
			pic_config->aux_data_buf,
			pic_config->aux_data_size,
			false, hw->provider_name, NULL);

		av1_update_gvs(hw, vf, pic_config);
		memcpy(&tmp4x, hw->gvs, sizeof(struct vdec_info));
		tmp4x.bit_depth_luma = bit_depth_luma;
		tmp4x.bit_depth_chroma = bit_depth_chroma;
		tmp4x.double_write_mode = pic_config->double_write_mode;
		vdec_fill_vdec_frame(hw_to_vdec(hw), &hw->vframe_qos, &tmp4x, vf, pic_config->hw_decode_time);
		if (without_display_mode == 0) {
			if (v4l2_ctx->is_stream_off) {
				vav1_vf_put(vav1_vf_get(hw), hw);
			} else {
				ATRACE_COUNTER("VC_OUT_DEC-submit", fb->buf_idx);
				fb->task->submit(fb->task, TASK_TYPE_DEC);
			}
		} else
			vav1_vf_put(vav1_vf_get(hw), hw);
	}

	return 0;
}

void av1_raw_write_image(AV1Decoder *pbi, PIC_BUFFER_CONFIG *sd)
{
	sd->stream_offset = pbi->pre_stream_offset;
	prepare_display_buf((struct AV1HW_s *)(pbi->private_data), sd);
	pbi->pre_stream_offset = READ_VREG(HEVC_SHIFT_BYTE_COUNT);
}

static bool is_avaliable_buffer(struct AV1HW_s *hw);

static int notify_v4l_eos(struct vdec_s *vdec)
{
	struct AV1HW_s *hw = (struct AV1HW_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct vframe_s *vf = &hw->vframe_dummy;
	struct vdec_v4l2_buffer *fb = NULL;
	int index = INVALID_IDX;
	ulong expires;

	if (hw->eos) {
		expires = jiffies + msecs_to_jiffies(2000);
		while (!is_avaliable_buffer(hw)) {
			if (time_after(jiffies, expires)) {
				pr_err("[%d] AV1 isn't enough buff for notify eos.\n", ctx->id);
				return 0;
			}
		}

		index = v4l_get_free_fb(hw);
		if (INVALID_IDX == index) {
			pr_err("[%d] AV1 EOS get free buff fail.\n", ctx->id);
			return 0;
		}

		fb = (struct vdec_v4l2_buffer *)
			hw->m_BUF[index].v4l_ref_buf_addr;

		vf->type		|= VIDTYPE_V4L_EOS;
		vf->timestamp		= ULONG_MAX;
		vf->flag		= VFRAME_FLAG_EMPTY_FRAME_V4L;
		vf->v4l_mem_handle	= (ulong)fb;

		vdec_vframe_ready(vdec, vf);
		kfifo_put(&hw->display_q, (const struct vframe_s *)vf);

		ATRACE_COUNTER("VC_OUT_DEC-submit", fb->buf_idx);
		fb->task->submit(fb->task, TASK_TYPE_DEC);

		av1_print(hw, 0, "[%d] AV1 EOS notify.\n", ctx->id);
	}

	return 0;
}

static void get_rpm_param(union param_u *params)
{
	int i;
	unsigned int data32;

	if (debug & AV1_DEBUG_BUFMGR)
		pr_info("enter %s\r\n", __func__);
	for (i = 0; i < 128; i++) {
		do {
			data32 = READ_VREG(RPM_CMD_REG);
		} while ((data32 & 0x10000) == 0);
		params->l.data[i] = data32&0xffff;
		WRITE_VREG(RPM_CMD_REG, 0);
	}
	if (debug & AV1_DEBUG_BUFMGR)
		pr_info("leave %s\r\n", __func__);
}

#ifdef CHANGE_REMOVED
static int recycle_mmu_buf_tail(struct AV1HW_s *hw,
		bool check_dma)
{
	struct AV1_Common_s *const cm = &hw->common;
	int index = cm->cur_fb_idx_mmu;
	struct internal_comp_buf *ibuf = index_to_icomp_buf(hw, index);

	hw->used_4k_num =
		READ_VREG(HEVC_SAO_MMU_STATUS) >> 16;

	av1_print(hw, 0, "pic index %d page_start %d\n",
		cm->cur_fb_idx_mmu, hw->used_4k_num);

	if (check_dma)
		hevc_mmu_dma_check(hw_to_vdec(hw));

	decoder_mmu_box_free_idx_tail(
			ibuf->mmu_box,
			ibuf->index,
			hw->used_4k_num);

	cm->cur_fb_idx_mmu = INVALID_IDX;
	hw->used_4k_num = -1;

	return 0;
}
#endif

#ifdef CHANGE_REMOVED
static void av1_recycle_mmu_buf_tail(struct AV1HW_s *hw)
{
	struct AV1_Common_s *const cm = &hw->common;
	if (get_double_write_mode(hw) & 0x10)
		return;

	if (cm->cur_fb_idx_mmu != INVALID_IDX) {
		recycle_mmu_buf_tail(hw,
			((hw->used_4k_num == -1) &&
			hw->m_ins_flag) ? 1 : 0);
	}
}
#endif

static void dec_again_process(struct AV1HW_s *hw)
{
	amhevc_stop();
	hw->dec_result = DEC_RESULT_AGAIN;

	if (hw->process_state == PROC_STATE_DECODESLICE) {
		hw->process_state = PROC_STATE_SENDAGAIN;
	}
	reset_process_time(hw);
	vdec_schedule_work(&hw->work);
}

static void read_film_grain_reg(struct AV1HW_s *hw)
{
    AV1_COMMON *cm = &hw->common;
    int i;
    if (cm->cur_frame == NULL) {
	    av1_print(hw, AOM_DEBUG_HW_MORE, "%s, cur_frame not exist!!!\n", __func__);
	    return;
	} else
	    av1_print(hw, AOM_DEBUG_HW_MORE, "%s\n", __func__);
    WRITE_VREG(HEVC_FGS_IDX, 0);
    for (i = 0; i < FILM_GRAIN_REG_SIZE; i++) {
	    cm->cur_frame->film_grain_reg[i] = READ_VREG(HEVC_FGS_DATA);
	}
	cm->cur_frame->film_grain_reg_valid = 1;
	cm->cur_frame->film_grain_ctrl = READ_VREG(HEVC_FGS_CTRL);
}

static void config_film_grain_reg(struct AV1HW_s *hw, int film_grain_params_ref_idx)
{
	AV1_COMMON *cm = &hw->common;
	int i;
	unsigned char found = 0;
	RefCntBuffer *buf;

	av1_print(hw, AOM_DEBUG_HW_MORE,
		" ## %s frome reference idx %d\n",
		__func__, film_grain_params_ref_idx);
	for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
		if (film_grain_params_ref_idx == cm->remapped_ref_idx[i]) {
			found = 1;
			break;
		}
	}
	if (!found) {
		av1_print(hw, AOM_DEBUG_HW_MORE,
			"%s Error, Invalid film grain reference idx %d\n",
			__func__, film_grain_params_ref_idx);
		return;
	}
	buf = cm->ref_frame_map[film_grain_params_ref_idx];

	if (buf->film_grain_reg_valid == 0) {
		av1_print(hw, AOM_DEBUG_HW_MORE,
			"%s Error, film grain register data invalid for reference idx %d\n",
			__func__, film_grain_params_ref_idx);
		return;
	}

	if (cm->cur_frame == NULL) {
		av1_print(hw, AOM_DEBUG_HW_MORE,
		"%s, cur_frame not exist!!!\n", __func__);
	}
	WRITE_VREG(HEVC_FGS_IDX, 0);
	for (i = 0; i < FILM_GRAIN_REG_SIZE; i++) {
		WRITE_VREG(HEVC_FGS_DATA, buf->film_grain_reg[i]);
		if (cm->cur_frame)
			cm->cur_frame->film_grain_reg[i] = buf->film_grain_reg[i];
	}
	if (cm->cur_frame)
		cm->cur_frame->film_grain_reg_valid = 1;
	WRITE_VREG(HEVC_FGS_CTRL, READ_VREG(HEVC_FGS_CTRL) | 1); // set fil_grain_start
	if (cm->cur_frame)
		cm->cur_frame->film_grain_ctrl = READ_VREG(HEVC_FGS_CTRL);
}

void config_next_ref_info_hw(struct AV1HW_s *hw)
{
	int j;
	AV1_COMMON *const cm = &hw->common;

	av1_set_next_ref_frame_map(hw->pbi);
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SC2)
		WRITE_VREG(HEVC_PARSER_MEM_WR_ADDR, 0x11a0);
	else
		WRITE_VREG(HEVC_PARSER_MEM_WR_ADDR, 0x1000);

	for (j = 0; j < 12; j++) {
		unsigned int info =
			av1_get_next_used_ref_info(cm, j);

		WRITE_VREG(HEVC_PARSER_MEM_RW_DATA, info);
		av1_print(hw, AOM_DEBUG_HW_MORE,
			"config next ref info %d 0x%x\n", j, info);
	}
}



#ifdef PRINT_HEVC_DATA_PATH_MONITOR
void datapath_monitor(struct AV1HW_s *hw)
{
		  uint32_t total_clk_count;
		  uint32_t path_transfer_count;
		  uint32_t path_wait_count;
  float path_wait_ratio;
  if (pbi->decode_idx > 1) {
    WRITE_VREG(HEVC_PATH_MONITOR_CTRL, 0); // Disabble monitor and set rd_idx to 0
    total_clk_count = READ_VREG(HEVC_PATH_MONITOR_DATA);

    WRITE_VREG(HEVC_PATH_MONITOR_CTRL, (1<<4)); // Disabble monitor and set rd_idx to 0

// parser --> iqit
    path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    if (path_transfer_count == 0) path_wait_ratio = 0.0;
    else path_wait_ratio = (float)path_wait_count/(float)path_transfer_count;
    printk("[P%d HEVC PATH] Parser/IQIT/IPP/DBLK/OW/DDR/CMD WAITING \% : %.2f",
        pbi->decode_idx - 2, path_wait_ratio);

// iqit --> ipp
    path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    if (path_transfer_count == 0) path_wait_ratio = 0.0;
    else path_wait_ratio = (float)path_wait_count/(float)path_transfer_count;
    printk(" %.2f", path_wait_ratio);

// dblk <-- ipp
    path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    if (path_transfer_count == 0) path_wait_ratio = 0.0;
    else path_wait_ratio = (float)path_wait_count/(float)path_transfer_count;
    printk(" %.2f", path_wait_ratio);

// dblk --> ow
    path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    if (path_transfer_count == 0) path_wait_ratio = 0.0;
    else path_wait_ratio = (float)path_wait_count/(float)path_transfer_count;
    printk(" %.2f", path_wait_ratio);

// <--> DDR
    path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    if (path_transfer_count == 0) path_wait_ratio = 0.0;
    else path_wait_ratio = (float)path_wait_count/(float)path_transfer_count;
    printk(" %.2f", path_wait_ratio);

// CMD
    path_transfer_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    path_wait_count = READ_VREG(HEVC_PATH_MONITOR_DATA);
    if (path_transfer_count == 0) path_wait_ratio = 0.0;
    else path_wait_ratio = (float)path_wait_count/(float)path_transfer_count;
    printk(" %.2f\n", path_wait_ratio);
  }
}

#endif

#ifdef MCRCC_ENABLE

static int mcrcc_hit_rate;
static int mcrcc_bypass_rate;

#define C_Reg_Wr  WRITE_VREG
static void C_Reg_Rd(unsigned int adr, unsigned int *val)
{
	*val = READ_VREG(adr);
}

static void mcrcc_perfcount_reset(struct AV1HW_s *hw)
{
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE,
		"[cache_util.c] Entered mcrcc_perfcount_reset...\n");
	C_Reg_Wr(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)0x1);
	C_Reg_Wr(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)0x0);
	return;
}

static unsigned raw_mcr_cnt_total_prev;
static unsigned hit_mcr_0_cnt_total_prev;
static unsigned hit_mcr_1_cnt_total_prev;
static unsigned byp_mcr_cnt_nchcanv_total_prev;
static unsigned byp_mcr_cnt_nchoutwin_total_prev;

static void mcrcc_get_hitrate(struct AV1HW_s *hw, unsigned reset_pre)
{
	unsigned  delta_hit_mcr_0_cnt;
	unsigned  delta_hit_mcr_1_cnt;
	unsigned  delta_raw_mcr_cnt;
	unsigned  delta_mcr_cnt_nchcanv;
	unsigned  delta_mcr_cnt_nchoutwin;

	unsigned   tmp;
	unsigned   raw_mcr_cnt;
	unsigned   hit_mcr_cnt;
	unsigned   byp_mcr_cnt_nchoutwin;
	unsigned   byp_mcr_cnt_nchcanv;
	int      hitrate;

	if (reset_pre) {
		raw_mcr_cnt_total_prev = 0;
		hit_mcr_0_cnt_total_prev = 0;
		hit_mcr_1_cnt_total_prev = 0;
		byp_mcr_cnt_nchcanv_total_prev = 0;
		byp_mcr_cnt_nchoutwin_total_prev = 0;
	}
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "[cache_util.c] Entered mcrcc_get_hitrate...\n");
	C_Reg_Wr(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)(0x0<<1));
	C_Reg_Rd(HEVCD_MCRCC_PERFMON_DATA, &raw_mcr_cnt);
	C_Reg_Wr(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)(0x1<<1));
	C_Reg_Rd(HEVCD_MCRCC_PERFMON_DATA, &hit_mcr_cnt);
	C_Reg_Wr(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)(0x2<<1));
	C_Reg_Rd(HEVCD_MCRCC_PERFMON_DATA, &byp_mcr_cnt_nchoutwin);
	C_Reg_Wr(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)(0x3<<1));
	C_Reg_Rd(HEVCD_MCRCC_PERFMON_DATA, &byp_mcr_cnt_nchcanv);

	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "raw_mcr_cnt_total: %d\n",raw_mcr_cnt);
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "hit_mcr_cnt_total: %d\n",hit_mcr_cnt);
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "byp_mcr_cnt_nchoutwin_total: %d\n",byp_mcr_cnt_nchoutwin);
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "byp_mcr_cnt_nchcanv_total: %d\n",byp_mcr_cnt_nchcanv);

	delta_raw_mcr_cnt = raw_mcr_cnt - raw_mcr_cnt_total_prev;
	delta_mcr_cnt_nchcanv = byp_mcr_cnt_nchcanv - byp_mcr_cnt_nchcanv_total_prev;
	delta_mcr_cnt_nchoutwin = byp_mcr_cnt_nchoutwin - byp_mcr_cnt_nchoutwin_total_prev;
	raw_mcr_cnt_total_prev = raw_mcr_cnt;
	byp_mcr_cnt_nchcanv_total_prev = byp_mcr_cnt_nchcanv;
	byp_mcr_cnt_nchoutwin_total_prev = byp_mcr_cnt_nchoutwin;

	C_Reg_Wr(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)(0x4<<1));
	C_Reg_Rd(HEVCD_MCRCC_PERFMON_DATA, &tmp);
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "miss_mcr_0_cnt_total: %d\n",tmp);
	C_Reg_Wr(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)(0x5<<1));
	C_Reg_Rd(HEVCD_MCRCC_PERFMON_DATA, &tmp);
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "miss_mcr_1_cnt_total: %d\n",tmp);
	C_Reg_Wr(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)(0x6<<1));
	C_Reg_Rd(HEVCD_MCRCC_PERFMON_DATA, &tmp);
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "hit_mcr_0_cnt_total: %d\n",tmp);
	delta_hit_mcr_0_cnt = tmp - hit_mcr_0_cnt_total_prev;
	hit_mcr_0_cnt_total_prev = tmp;
	C_Reg_Wr(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)(0x7<<1));
	C_Reg_Rd(HEVCD_MCRCC_PERFMON_DATA, &tmp);
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "hit_mcr_1_cnt_total: %d\n",tmp);
	delta_hit_mcr_1_cnt = tmp - hit_mcr_1_cnt_total_prev;
	hit_mcr_1_cnt_total_prev = tmp;

	if ( delta_raw_mcr_cnt != 0 ) {
		hitrate = 100 * delta_hit_mcr_0_cnt/ delta_raw_mcr_cnt;
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "CANV0_HIT_RATE : %d\n", hitrate);
		hitrate = 100 * delta_hit_mcr_1_cnt/delta_raw_mcr_cnt;
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "CANV1_HIT_RATE : %d\n", hitrate);
		hitrate = 100 * delta_mcr_cnt_nchcanv/delta_raw_mcr_cnt;
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "NONCACH_CANV_BYP_RATE : %d\n", hitrate);
		hitrate = 100 * delta_mcr_cnt_nchoutwin/delta_raw_mcr_cnt;
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "CACHE_OUTWIN_BYP_RATE : %d\n", hitrate);
	}

	if (raw_mcr_cnt != 0)
	{
		hitrate = 100*hit_mcr_cnt/raw_mcr_cnt;
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "MCRCC_HIT_RATE : %d\n", hitrate);
		hitrate = 100*(byp_mcr_cnt_nchoutwin + byp_mcr_cnt_nchcanv)/raw_mcr_cnt;
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "MCRCC_BYP_RATE : %d\n", hitrate);
	} else {
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "MCRCC_HIT_RATE : na\n");
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "MCRCC_BYP_RATE : na\n");
	}
	mcrcc_hit_rate = 100*hit_mcr_cnt/raw_mcr_cnt;
	mcrcc_bypass_rate = 100*(byp_mcr_cnt_nchoutwin + byp_mcr_cnt_nchcanv)/raw_mcr_cnt;

	return;
}

static void decomp_perfcount_reset(struct AV1HW_s *hw)
{
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "[cache_util.c] Entered decomp_perfcount_reset...\n");
	C_Reg_Wr(HEVCD_MPP_DECOMP_PERFMON_CTL, (unsigned int)0x1);
	C_Reg_Wr(HEVCD_MPP_DECOMP_PERFMON_CTL, (unsigned int)0x0);
	return;
}

static void decomp_get_hitrate(struct AV1HW_s *hw)
{
	unsigned   raw_mcr_cnt;
	unsigned   hit_mcr_cnt;
	int  hitrate;

	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "[cache_util.c] Entered decomp_get_hitrate...\n");
	C_Reg_Wr(HEVCD_MPP_DECOMP_PERFMON_CTL, (unsigned int)(0x0<<1));
	C_Reg_Rd(HEVCD_MPP_DECOMP_PERFMON_DATA, &raw_mcr_cnt);
	C_Reg_Wr(HEVCD_MPP_DECOMP_PERFMON_CTL, (unsigned int)(0x1<<1));
	C_Reg_Rd(HEVCD_MPP_DECOMP_PERFMON_DATA, &hit_mcr_cnt);

	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "hcache_raw_cnt_total: %d\n",raw_mcr_cnt);
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "hcache_hit_cnt_total: %d\n",hit_mcr_cnt);

	if ( raw_mcr_cnt != 0 ) {
		hitrate = 100*hit_mcr_cnt/raw_mcr_cnt;
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "DECOMP_HCACHE_HIT_RATE : %.2f\%\n", hitrate);
	} else {
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "DECOMP_HCACHE_HIT_RATE : na\n");
	}
	C_Reg_Wr(HEVCD_MPP_DECOMP_PERFMON_CTL, (unsigned int)(0x2<<1));
	C_Reg_Rd(HEVCD_MPP_DECOMP_PERFMON_DATA, &raw_mcr_cnt);
	C_Reg_Wr(HEVCD_MPP_DECOMP_PERFMON_CTL, (unsigned int)(0x3<<1));
	C_Reg_Rd(HEVCD_MPP_DECOMP_PERFMON_DATA, &hit_mcr_cnt);

	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "dcache_raw_cnt_total: %d\n",raw_mcr_cnt);
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "dcache_hit_cnt_total: %d\n",hit_mcr_cnt);

	if ( raw_mcr_cnt != 0 ) {
		hitrate = 100*hit_mcr_cnt/raw_mcr_cnt;
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "DECOMP_DCACHE_HIT_RATE : %d\n", hitrate);

		hitrate = mcrcc_hit_rate + (mcrcc_bypass_rate * hit_mcr_cnt/raw_mcr_cnt);
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "MCRCC_DECOMP_DCACHE_EFFECTIVE_HIT_RATE : %d\n", hitrate);

	} else {
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "DECOMP_DCACHE_HIT_RATE : na\n");
	}

	return;
}

static void decomp_get_comprate(struct AV1HW_s *hw)
{
	unsigned   raw_ucomp_cnt;
	unsigned   fast_comp_cnt;
	unsigned   slow_comp_cnt;
	int      comprate;

	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "[cache_util.c] Entered decomp_get_comprate...\n");
	C_Reg_Wr(HEVCD_MPP_DECOMP_PERFMON_CTL, (unsigned int)(0x4<<1));
	C_Reg_Rd(HEVCD_MPP_DECOMP_PERFMON_DATA, &fast_comp_cnt);
	C_Reg_Wr(HEVCD_MPP_DECOMP_PERFMON_CTL, (unsigned int)(0x5<<1));
	C_Reg_Rd(HEVCD_MPP_DECOMP_PERFMON_DATA, &slow_comp_cnt);
	C_Reg_Wr(HEVCD_MPP_DECOMP_PERFMON_CTL, (unsigned int)(0x6<<1));
	C_Reg_Rd(HEVCD_MPP_DECOMP_PERFMON_DATA, &raw_ucomp_cnt);

	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "decomp_fast_comp_total: %d\n",fast_comp_cnt);
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "decomp_slow_comp_total: %d\n",slow_comp_cnt);
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "decomp_raw_uncomp_total: %d\n",raw_ucomp_cnt);

	if ( raw_ucomp_cnt != 0 )
	{
		comprate = 100*(fast_comp_cnt + slow_comp_cnt)/raw_ucomp_cnt;
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "DECOMP_COMP_RATIO : %d\n", comprate);
	} else
	{
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "DECOMP_COMP_RATIO : na\n");
	}
	return;
}

static void dump_hit_rate(struct AV1HW_s *hw)
{
	if (debug & AV1_DEBUG_CACHE_HIT_RATE) {
		mcrcc_get_hitrate(hw, hw->m_ins_flag);
		decomp_get_hitrate(hw);
		decomp_get_comprate(hw);
	}
}

static  uint32_t  mcrcc_get_abs_frame_distance(struct AV1HW_s *hw, uint32_t refid, uint32_t ref_ohint, uint32_t curr_ohint, uint32_t ohint_bits_min1)
{
	int32_t diff_ohint0;
	int32_t diff_ohint1;
	uint32_t abs_dist;
	uint32_t m;
	uint32_t m_min1;

	diff_ohint0 = ref_ohint - curr_ohint;

	m = (1 << ohint_bits_min1);
	m_min1 = m -1;

	diff_ohint1 = (diff_ohint0 & m_min1 ) - (diff_ohint0 & m);

	abs_dist = (diff_ohint1 < 0) ? -diff_ohint1 : diff_ohint1;

	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE,
		"[cache_util.c] refid:%0x ref_orderhint:%0x curr_orderhint:%0x orderhint_bits_min1:%0x abd_dist:%0x\n",
		refid, ref_ohint, curr_ohint, ohint_bits_min1,abs_dist);

	return  abs_dist;
}

static void  config_mcrcc_axi_hw_nearest_ref(struct AV1HW_s *hw)
{
	uint32_t i;
	uint32_t rdata32;
	uint32_t dist_array[8];
	uint32_t refcanvas_array[2];
	uint32_t orderhint_bits;
	unsigned char is_inter;
	AV1_COMMON *cm = &hw->common;
	PIC_BUFFER_CONFIG *curr_pic_config;
	int32_t  curr_orderhint;
	int cindex0 = LAST_FRAME;
	uint32_t    last_ref_orderhint_dist = 1023; // large distance
	uint32_t    curr_ref_orderhint_dist = 1023; // large distance
	int cindex1;
	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE,
		"[test.c] #### config_mcrcc_axi_hw ####\n");

	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2); // reset mcrcc

	is_inter = av1_frame_is_inter(&hw->common); //((pbi->common.frame_type != KEY_FRAME) && (!pbi->common.intra_only)) ? 1 : 0;
	if ( !is_inter ) { // I-PIC
		//WRITE_VREG(HEVCD_MCRCC_CTL1, 0x1); // remove reset -- disables clock
		WRITE_VREG(HEVCD_MCRCC_CTL2, 0xffffffff); // Replace with current-frame canvas
		WRITE_VREG(HEVCD_MCRCC_CTL3, 0xffffffff); //
		WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0); // enable mcrcc progressive-mode
		return;
	}

	// Find absolute orderhint delta
	curr_pic_config =  &cm->cur_frame->buf;
	curr_orderhint = curr_pic_config->order_hint;
	orderhint_bits = cm->seq_params.order_hint_info.order_hint_bits_minus_1;
	for (i = LAST_FRAME; i <= ALTREF_FRAME; i++) {
		int32_t  ref_orderhint = 0;
		PIC_BUFFER_CONFIG *pic_config;
		pic_config = av1_get_ref_frame_spec_buf(cm,i);
		if (pic_config)
			ref_orderhint = pic_config->order_hint;
		dist_array[i] =  mcrcc_get_abs_frame_distance(hw, i,ref_orderhint, curr_orderhint, orderhint_bits);
	}
	// Get smallest orderhint distance refid
	for (i = LAST_FRAME; i <= ALTREF_FRAME; i++) {
		PIC_BUFFER_CONFIG *pic_config;
		pic_config = av1_get_ref_frame_spec_buf(cm, i);
		curr_ref_orderhint_dist = dist_array[i];
		if ( curr_ref_orderhint_dist < last_ref_orderhint_dist) {
			cindex0 = i;
			last_ref_orderhint_dist = curr_ref_orderhint_dist;
		}
	}
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (cindex0 << 8) | (1<<1) | 0);
	refcanvas_array[0] = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR) & 0xffff;

	last_ref_orderhint_dist = 1023; // large distance
	curr_ref_orderhint_dist = 1023; // large distance
	// Get 2nd smallest orderhint distance refid
	cindex1 = LAST_FRAME;
	for (i = LAST_FRAME; i <= ALTREF_FRAME; i++) {
		PIC_BUFFER_CONFIG *pic_config;
		pic_config = av1_get_ref_frame_spec_buf(cm, i);
		curr_ref_orderhint_dist = dist_array[i];
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (i << 8) | (1<<1) | 0);
		refcanvas_array[1] = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR) & 0xffff;
		av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "[cache_util.c] curr_ref_orderhint_dist:%x last_ref_orderhint_dist:%x refcanvas_array[0]:%x refcanvas_array[1]:%x\n",
		curr_ref_orderhint_dist, last_ref_orderhint_dist, refcanvas_array[0],refcanvas_array[1]);
		if ((curr_ref_orderhint_dist < last_ref_orderhint_dist) && (refcanvas_array[0] != refcanvas_array[1])) {
			cindex1 = i;
			last_ref_orderhint_dist = curr_ref_orderhint_dist;
		}
	}

	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (cindex0 << 8) | (1<<1) | 0);
	refcanvas_array[0] = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (cindex1 << 8) | (1<<1) | 0);
	refcanvas_array[1] = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);

	av1_print(hw, AV1_DEBUG_CACHE_HIT_RATE, "[cache_util.c] refcanvas_array[0](index %d):%x refcanvas_array[1](index %d):%x\n",
	cindex0, refcanvas_array[0], cindex1, refcanvas_array[1]);

	// lowest delta_picnum
	rdata32 = refcanvas_array[0];
	rdata32 = rdata32 & 0xffff;
	rdata32 = rdata32 | ( rdata32 << 16);
	WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

	// 2nd-lowest delta_picnum
	rdata32 = refcanvas_array[1];
	rdata32 = rdata32 & 0xffff;
	rdata32 = rdata32 | ( rdata32 << 16);
	WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32);

	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0); // enable mcrcc progressive-mode
	return;
}


#endif

int av1_continue_decoding(struct AV1HW_s *hw, int obu_type)
{
	int ret = 0;
#ifdef SANITY_CHECK
	param_t* params = &hw->aom_param;
#endif

	//def CHANGE_DONE
	AV1Decoder *pbi = hw->pbi;
	AV1_COMMON *const cm = pbi->common;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int i;

	av1_print(hw, AOM_DEBUG_HW_MORE,
		"%s: pbi %p cm %p cur_frame %p %d has_seq %d has_keyframe %d\n",
		__func__, pbi, cm, cm->cur_frame,
		pbi->bufmgr_proc_count,
		hw->has_sequence,
		hw->has_keyframe);

	if (hw->has_sequence == 0) {
		av1_print(hw, 0,
			"no sequence head, skip\n");
		if (!hw->m_ins_flag)
			WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_SEARCH_HEAD);
		return -2;
	} else if (hw->has_keyframe == 0 &&
		hw->aom_param.p.frame_type != KEY_FRAME){
		av1_print(hw, 0,
			"no key frame, skip\n");
		on_no_keyframe_skiped++;
		if (!hw->m_ins_flag)
			WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_SEARCH_HEAD);
		return -2;
	}
	hw->has_keyframe = 1;
	on_no_keyframe_skiped = 0;

	if (ctx->param_sets_from_ucode)
		hw->res_ch_flag = 0;

	if (pbi->bufmgr_proc_count == 0 ||
	hw->one_compressed_data_done) {
		hw->new_compressed_data = 1;
		hw->one_compressed_data_done = 0;
	} else {
		hw->new_compressed_data = 0;
	}
#ifdef SANITY_CHECK
	ret = 0;
	av1_print(hw, AOM_DEBUG_HW_MORE,
		"Check Picture size, max (%d, %d), width/height (%d, %d), dec_width %d\n",
		params->p.max_frame_width,
		params->p.max_frame_height,
		params->p.frame_width_scaled,
		params->p.frame_height,
		params->p.dec_frame_width
	);

	if ((params->p.frame_width_scaled * params->p.frame_height) > MAX_SIZE_8K ||
	(params->p.dec_frame_width * params->p.frame_height) > MAX_SIZE_8K ||
	params->p.frame_width_scaled <= 0 ||
	params->p.dec_frame_width <= 0 ||
	params->p.frame_height <= 0) {
		av1_print(hw, 0, "!!Picture size error, max (%d, %d), width/height (%d, %d), dec_width %d\n",
			params->p.max_frame_width,
			params->p.max_frame_height,
			params->p.frame_width_scaled,
			params->p.frame_height,
			params->p.dec_frame_width
		);
		vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
		ret = -1;
	}
#endif
	if (ret >= 0) {
		ret = av1_bufmgr_process(pbi, &hw->aom_param,
			hw->new_compressed_data, obu_type);
		if (ret < 0)
			return -1;
		av1_print(hw, AOM_DEBUG_HW_MORE,
			"%s: pbi %p cm %p cur_frame %p\n",
			__func__, pbi, cm, cm->cur_frame);

		av1_print(hw, AOM_DEBUG_HW_MORE,
			"1+++++++++++++++++++++++++++++++++++%d %p\n",
			ret, cm->cur_frame);
		if (cm->cur_frame) {
			init_waitqueue_head(&cm->cur_frame->wait_sfgs);
			atomic_set(&cm->cur_frame->fgs_done, 1);
		}
		if (hw->new_compressed_data)
			WRITE_VREG(PIC_END_LCU_COUNT, 0);
	}
	if (ret > 0) {
		/* the case when cm->show_existing_frame is 1 */
		/*case 3016*/
		av1_print(hw, AOM_DEBUG_HW_MORE,
			"Decoding done (index=%d, show_existing_frame = %d)\n",
			cm->cur_frame? cm->cur_frame->buf.index:-1,
			cm->show_existing_frame
			);

		if (cm->cur_frame) {
			PIC_BUFFER_CONFIG* cur_pic_config = &cm->cur_frame->buf;
			if (debug &
				AOM_DEBUG_AUX_DATA)
				dump_aux_buf(hw);
			set_pic_aux_data(hw,
				cur_pic_config, 0, 0);
		}
		config_next_ref_info_hw(hw);

		av1_print(hw, AOM_DEBUG_HW_MORE,
			"aom_bufmgr_process=> %d,decode done, AOM_AV1_SEARCH_HEAD\r\n", ret);
		WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_SEARCH_HEAD);
		pbi->decode_idx++;
		pbi->bufmgr_proc_count++;
		hw->frame_decoded = 1;
		return 0;
	} else if (ret < 0) {
		hw->frame_decoded = 1;
		av1_print(hw, AOM_DEBUG_HW_MORE,
		"aom_bufmgr_process=> %d, bufmgr e.r.r.o.r. %d, AOM_AV1_SEARCH_HEAD\r\n",
		ret, cm->error.error_code);
		WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_SEARCH_HEAD);
		return 0;
	}
	else if (ret == 0) {
		PIC_BUFFER_CONFIG* cur_pic_config = &cm->cur_frame->buf;
		PIC_BUFFER_CONFIG* prev_pic_config = &cm->prev_frame->buf;
		if (debug &
			AOM_DEBUG_AUX_DATA)
			dump_aux_buf(hw);
		set_dv_data(hw);
		if (cm->show_frame &&
			hw->dv_data_buf != NULL)
			copy_dv_data(hw, cur_pic_config);

		hw->frame_decoded = 0;
		pbi->bufmgr_proc_count++;
		if (hw->new_compressed_data == 0) {
			WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_DECODE_SLICE);
			return 0;
		}
		av1_print(hw, AOM_DEBUG_HW_MORE,
		" [PICTURE %d] cm->cur_frame->mi_size : (%d X %d) y_crop_size :(%d X %d)\n",
		hw->frame_count,
		cm->cur_frame->mi_cols,
		cm->cur_frame->mi_rows,
		cur_pic_config->y_crop_width,
		cur_pic_config->y_crop_height);
		if (cm->prev_frame > 0) {
			av1_print(hw, AOM_DEBUG_HW_MORE,
			" [SEGMENT] cm->prev_frame->segmentation_enabled : %d\n",
			cm->prev_frame->segmentation_enabled);
			av1_print(hw, AOM_DEBUG_HW_MORE,
			" [SEGMENT] cm->prev_frame->mi_size : (%d X %d)\n",
			cm->prev_frame->mi_cols, cm->prev_frame->mi_rows);
		}
		cm->cur_frame->prev_segmentation_enabled = (cm->prev_frame > 0) ?
		(cm->prev_frame->segmentation_enabled & (cm->prev_frame->segmentation_update_map
		| cm->prev_frame->prev_segmentation_enabled) &
		(cm->cur_frame->mi_rows == cm->prev_frame->mi_rows) &
		(cm->cur_frame->mi_cols == cm->prev_frame->mi_cols)) : 0;
		WRITE_VREG(AV1_SKIP_MODE_INFO,
			(cm->cur_frame->prev_segmentation_enabled << 31) |
			(((cm->prev_frame > 0) ? cm->prev_frame->intra_only : 0) << 30) |
			(((cm->prev_frame > 0) ? prev_pic_config->index : 0x1f) << 24) |
			(((cm->cur_frame > 0) ? cur_pic_config->index : 0x1f) << 16) |
			(cm->current_frame.skip_mode_info.ref_frame_idx_0 & 0xf) |
			((cm->current_frame.skip_mode_info.ref_frame_idx_1 & 0xf) << 4) |
			(cm->current_frame.skip_mode_info.skip_mode_allowed << 8));
		cur_pic_config->decode_idx = pbi->decode_idx;

		av1_print(hw, AOM_DEBUG_HW_MORE,
			"Decode Frame Data %d frame_type %d (%d) bufmgr_proc_count %d\n",
		pbi->decode_idx,
		cm->cur_frame->frame_type,
		cm->current_frame.frame_type,
		pbi->bufmgr_proc_count);
		pbi->decode_idx++;
		hw->frame_count++;
		cur_pic_config->slice_type = cm->cur_frame->frame_type;
		if (hw->chunk) {
			av1_print(hw, AV1_DEBUG_OUT_PTS,
				"%s, config pic pts %d, pts64 %lld, ts: %lld\n",
				__func__, hw->chunk->pts, hw->chunk->pts64, hw->chunk->timestamp);
			cur_pic_config->pts = hw->chunk->pts;
			cur_pic_config->pts64 = hw->chunk->pts64;

			if (!v4l_bitstream_id_enable) {
				cur_pic_config->pts64 = hw->chunk->timestamp;
				hw->chunk->timestamp = 0;
			}

			hw->chunk->pts = 0;
			hw->chunk->pts64 = 0;
		}
		ATRACE_COUNTER(hw->trace.decode_header_memory_time_name, TRACE_HEADER_REGISTER_START);
#ifdef DUAL_DECODE
#else
		config_pic_size(hw, hw->aom_param.p.bit_depth);
#endif
		if (get_mv_buf(hw, &cm->cur_frame->buf) < 0) {
			av1_print(hw, 0,
				"%s: Error get_mv_buf fail\n",
				__func__);
			ret = -1;
		}

		if (ret >= 0 && (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
			(get_double_write_mode(hw) != 0x10)) {
			ret = av1_alloc_mmu(hw,
				cm->cur_frame->buf.index,
				cur_pic_config->y_crop_width,
				cur_pic_config->y_crop_height,
				hw->aom_param.p.bit_depth,
				hw->frame_mmu_map_addr);
			if (ret >= 0)
				cm->cur_fb_idx_mmu = cm->cur_frame->buf.index;
			else {
				pr_err("can't alloc need mmu1,idx %d ret =%d\n",
					cm->cur_frame->buf.index, ret);
				vdec_v4l_post_error_event(ctx, DECODER_ERROR_ALLOC_BUFFER_FAIL);
			}
#ifdef AOM_AV1_MMU_DW
			if (get_double_write_mode(hw) & 0x20) {
				ret = av1_alloc_mmu_dw(hw,
				cm->cur_frame->buf.index,
				cur_pic_config->y_crop_width,
				cur_pic_config->y_crop_height,
				hw->aom_param.p.bit_depth,
				hw->dw_frame_mmu_map_addr);
				if (ret >= 0)
					cm->cur_fb_idx_mmu_dw = cm->cur_frame->buf.index;
				else {
					pr_err("can't alloc need dw mmu1,idx %d ret =%d\n",
					cm->cur_frame->buf.index, ret);
					vdec_v4l_post_error_event(ctx, DECODER_ERROR_ALLOC_BUFFER_FAIL);
				}
			}
#endif
#ifdef DEBUG_CRC_ERROR
			if (crc_debug_flag & 0x40)
				mv_buffer_fill_zero(hw, &cm->cur_frame->buf);
#endif
		} else {
			ret = 0;
		}
		if (av1_frame_is_inter(&hw->common)) {
			//if ((pbi->common.frame_type != KEY_FRAME) && (!pbi->common.intra_only)) {
#ifdef DUAL_DECODE
#else
			config_mc_buffer(hw, hw->aom_param.p.bit_depth, 1);
#endif
			config_mpred_hw(hw, 1);
		}
		else {
			config_mc_buffer(hw, hw->aom_param.p.bit_depth, 0);
			clear_mpred_hw(hw);
			config_mpred_hw(hw, 0);
		}
#ifdef DUAL_DECODE
#else
#ifdef MCRCC_ENABLE
		config_mcrcc_axi_hw_nearest_ref(hw);
#endif
		config_sao_hw(hw, &hw->aom_param);
#endif

		config_dblk_hw(hw);

		/* store segment_feature before shared sub-module run to fix mosaic on t5d */
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SC2)
			WRITE_VREG(HEVC_PARSER_MEM_WR_ADDR, 0x11b0 + (cur_pic_config->index));
		else
			WRITE_VREG(HEVC_PARSER_MEM_WR_ADDR, 0x1010 + (cur_pic_config->index));
		if (hw->aom_param.p.segmentation_enabled & 1) // segmentation_enabled
			WRITE_VREG(HEVC_PARSER_MEM_RW_DATA, READ_VREG(AV1_REF_SEG_INFO));
		else
			WRITE_VREG(HEVC_PARSER_MEM_RW_DATA, 0);

		// Save segment_feature while hardware decoding
		if (hw->seg_4lf->enabled) {
			for (i = 0; i < 8; i++) {
			cm->cur_frame->segment_feature[i] = READ_VREG(AOM_AV1_SEGMENT_FEATURE);
			}
		} else {
			for (i = 0; i < 8; i++) {
				cm->cur_frame->segment_feature[i] = (0x80000000 | (i << 22));
			}
		}

		if (mmu_mem_save && hw->mmu_enable &&
			(ctx->config.parm.dec.cfg.double_write_mode == 0x21)) {
			struct RefCntBuffer_s *frame_bufs =
				cm->buffer_pool->frame_bufs;
			/*free not used mmu buffers.*/
			for (i = 0; i< hw->used_buf_num; i++) {
				if (frame_bufs[i].buf.cma_alloc_addr &&
					(frame_bufs[i].ref_count == 0) &&
					(frame_bufs[i].buf.index != -1) &&
					(cm->cur_frame != &frame_bufs[i])) {
					struct internal_comp_buf *ibuf = index_to_icomp_buf(hw, i);
						if (!(ibuf->used & 1))
							continue;
					decoder_mmu_box_free_idx(ibuf->mmu_box, ibuf->index);
					ibuf->used &= ~0x1;
					av1_print(hw, AV1_DEBUG_BUFMGR, "free mmu buffer frame idx %d used: %d\n", i, ibuf->used);
				}
			}
		}

		av1_print(hw, AOM_DEBUG_HW_MORE, "HEVC_DEC_STATUS_REG <= AOM_AV1_DECODE_SLICE\n");
		WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_DECODE_SLICE);
	} else {
		av1_print(hw, AOM_DEBUG_HW_MORE, "Sequence head, Search next start code\n");
		cm->prev_fb_idx = INVALID_IDX;
		//skip, search next start code
		WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_DECODE_SLICE);
	}

	ATRACE_COUNTER(hw->trace.decode_header_memory_time_name, TRACE_HEADER_REGISTER_END);
	return ret;
}

static void fill_frame_info(struct AV1HW_s *hw,
	struct PIC_BUFFER_CONFIG_s *frame,
	unsigned int framesize,
	unsigned int pts)
{
	struct vframe_qos_s *vframe_qos = &hw->vframe_qos;

	if (frame->slice_type == KEY_FRAME)
		vframe_qos->type = 1;
	else if (frame->slice_type == INTER_FRAME)
		vframe_qos->type = 2;

	vframe_qos->size = framesize;
	vframe_qos->pts = pts;
#ifdef SHOW_QOS_INFO
	av1_print(hw, 0, "slice:%d\n", frame->slice_type);
#endif
	vframe_qos->max_mv = frame->max_mv;
	vframe_qos->avg_mv = frame->avg_mv;
	vframe_qos->min_mv = frame->min_mv;
#ifdef SHOW_QOS_INFO
	av1_print(hw, 0, "mv: max:%d,  avg:%d, min:%d\n",
			vframe_qos->max_mv,
			vframe_qos->avg_mv,
			vframe_qos->min_mv);
#endif
	vframe_qos->max_qp = frame->max_qp;
	vframe_qos->avg_qp = frame->avg_qp;
	vframe_qos->min_qp = frame->min_qp;
#ifdef SHOW_QOS_INFO
	av1_print(hw, 0, "qp: max:%d,  avg:%d, min:%d\n",
			vframe_qos->max_qp,
			vframe_qos->avg_qp,
			vframe_qos->min_qp);
#endif
	vframe_qos->max_skip = frame->max_skip;
	vframe_qos->avg_skip = frame->avg_skip;
	vframe_qos->min_skip = frame->min_skip;
#ifdef SHOW_QOS_INFO
	av1_print(hw, 0, "skip: max:%d,	avg:%d, min:%d\n",
			vframe_qos->max_skip,
			vframe_qos->avg_skip,
			vframe_qos->min_skip);
#endif
	vframe_qos->num++;
}

/* only when we decoded one field or one frame,
we can call this function to get qos info*/
static void get_picture_qos_info(struct AV1HW_s *hw)
{
	struct PIC_BUFFER_CONFIG_s *frame = &hw->cur_buf->buf;

	if (!frame)
		return;

	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_G12A) {
		unsigned char a[3];
		unsigned char i, j, t;
		unsigned long  data;

		data = READ_VREG(HEVC_MV_INFO);
		if (frame->slice_type == KEY_FRAME)
			data = 0;
		a[0] = data & 0xff;
		a[1] = (data >> 8) & 0xff;
		a[2] = (data >> 16) & 0xff;

		for (i = 0; i < 3; i++) {
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
		}
		frame->max_mv = a[2];
		frame->avg_mv = a[1];
		frame->min_mv = a[0];

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"mv data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data, a[0], a[1], a[2]);

		data = READ_VREG(HEVC_QP_INFO);
		a[0] = data & 0x1f;
		a[1] = (data >> 8) & 0x3f;
		a[2] = (data >> 16) & 0x7f;

		for (i = 0; i < 3; i++) {
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
		}
		frame->max_qp = a[2];
		frame->avg_qp = a[1];
		frame->min_qp = a[0];

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"qp data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data, a[0], a[1], a[2]);

		data = READ_VREG(HEVC_SKIP_INFO);
		a[0] = data & 0x1f;
		a[1] = (data >> 8) & 0x3f;
		a[2] = (data >> 16) & 0x7f;

		for (i = 0; i < 3; i++) {
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
		}
		frame->max_skip = a[2];
		frame->avg_skip = a[1];
		frame->min_skip = a[0];

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"skip data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data, a[0], a[1], a[2]);
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
		int pic_number = frame->decode_idx;

		frame->max_mv = 0;
		frame->avg_mv = 0;
		frame->min_mv = 0;

		frame->max_skip = 0;
		frame->avg_skip = 0;
		frame->min_skip = 0;

		frame->max_qp = 0;
		frame->avg_qp = 0;
		frame->min_qp = 0;

		av1_print(hw, AV1_DEBUG_QOS_INFO, "slice_type:%d, poc:%d\n",
			frame->slice_type,
			pic_number);

		/* set rd_idx to 0 */
		WRITE_VREG(HEVC_PIC_QUALITY_CTRL, 0);

		blk88_y_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
		if (blk88_y_count == 0) {

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] NO Data yet.\n",
			pic_number);

			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
		}
		/* qp_y_sum */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] Y QP AVG : %d (%d/%d)\n",
			pic_number, rdata32/blk88_y_count,
			rdata32, blk88_y_count);

		frame->avg_qp = rdata32/blk88_y_count;
		/* intra_y_count */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] Y intra rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_y_count,
			'%', rdata32);

		/* skipped_y_count */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] Y skipped rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_y_count,
			'%', rdata32);

		frame->avg_skip = rdata32*100/blk88_y_count;
		/* coeff_non_zero_y_count */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] Y ZERO_Coeff rate : %d%c (%d)\n",
			pic_number, (100 - rdata32*100/(blk88_y_count*1)),
			'%', rdata32);

		/* blk66_c_count */
		blk88_c_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
		if (blk88_c_count == 0) {
			av1_print(hw, AV1_DEBUG_QOS_INFO,
				"[Picture %d Quality] NO Data yet.\n",
				pic_number);
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
		}
		/* qp_c_sum */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		av1_print(hw, AV1_DEBUG_QOS_INFO,
				"[Picture %d Quality] C QP AVG : %d (%d/%d)\n",
				pic_number, rdata32/blk88_c_count,
				rdata32, blk88_c_count);

		/* intra_c_count */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] C intra rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_c_count,
			'%', rdata32);

		/* skipped_cu_c_count */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] C skipped rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_c_count,
			'%', rdata32);

		/* coeff_non_zero_c_count */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] C ZERO_Coeff rate : %d%c (%d)\n",
			pic_number, (100 - rdata32*100/(blk88_c_count*1)),
			'%', rdata32);

		/* 1'h0, qp_c_max[6:0], 1'h0, qp_c_min[6:0],
		1'h0, qp_y_max[6:0], 1'h0, qp_y_min[6:0] */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] Y QP min : %d\n",
			pic_number, (rdata32>>0)&0xff);

		frame->min_qp = (rdata32>>0)&0xff;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] Y QP max : %d\n",
			pic_number, (rdata32>>8)&0xff);

		frame->max_qp = (rdata32>>8)&0xff;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] C QP min : %d\n",
			pic_number, (rdata32>>16)&0xff);
		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] C QP max : %d\n",
			pic_number, (rdata32>>24)&0xff);

		/* blk22_mv_count */
		blk22_mv_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
		if (blk22_mv_count == 0) {
			av1_print(hw, AV1_DEBUG_QOS_INFO,
				"[Picture %d Quality] NO MV Data yet.\n",
				pic_number);
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
		}
		/* mvy_L1_count[39:32], mvx_L1_count[39:32],
		mvy_L0_count[39:32], mvx_L0_count[39:32] */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		/* should all be 0x00 or 0xff */
		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MV AVG High Bits: 0x%X\n",
			pic_number, rdata32);

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

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVX_L0 AVG : %d (%lld/%d)\n",
			pic_number, (int)value,
			value, blk22_mv_count);

		frame->avg_mv = value;

		/* mvy_L0_count[31:0] */
		rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvy_L0_hi;
		temp_value = (temp_value << 32) | rdata32_l;

		if (mvy_L0_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVY_L0 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);

		/* mvx_L1_count[31:0] */
		rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvx_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvx_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVX_L1 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);

		/* mvy_L1_count[31:0] */
		rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvy_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvy_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVY_L1 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);

		/* {mvx_L0_max, mvx_L0_min} // format : {sign, abs[14:0]}  */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		mv_hi = (rdata32>>16)&0xffff;
		if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVX_L0 MAX : %d\n",
			pic_number, mv_hi);

		frame->max_mv = mv_hi;

		mv_lo = (rdata32>>0)&0xffff;
		if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVX_L0 MIN : %d\n",
			pic_number, mv_lo);

		frame->min_mv = mv_lo;

		/* {mvy_L0_max, mvy_L0_min} */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		mv_hi = (rdata32>>16)&0xffff;
		if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVY_L0 MAX : %d\n",
			pic_number, mv_hi);

		mv_lo = (rdata32>>0)&0xffff;
		if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVY_L0 MIN : %d\n",
			pic_number, mv_lo);

		/* {mvx_L1_max, mvx_L1_min} */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		mv_hi = (rdata32>>16)&0xffff;
		if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVX_L1 MAX : %d\n",
			pic_number, mv_hi);

		mv_lo = (rdata32>>0)&0xffff;
		if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVX_L1 MIN : %d\n",
			pic_number, mv_lo);

		/* {mvy_L1_max, mvy_L1_min} */
		rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
		mv_hi = (rdata32>>16)&0xffff;
		if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVY_L1 MAX : %d\n",
			pic_number, mv_hi);

		mv_lo = (rdata32>>0)&0xffff;
		if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] MVY_L1 MIN : %d\n",
			pic_number, mv_lo);

		rdata32 = READ_VREG(HEVC_PIC_QUALITY_CTRL);

		av1_print(hw, AV1_DEBUG_QOS_INFO,
			"[Picture %d Quality] After Read : VDEC_PIC_QUALITY_CTRL : 0x%x\n",
			pic_number, rdata32);

		/* reset all counts */
		WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
	}
}

static int load_param(struct AV1HW_s *hw, union param_u *params, uint32_t dec_status)
{
    int i;
    unsigned long flags;
    int head_type = 0;
    if (dec_status == AOM_AV1_SEQ_HEAD_PARSER_DONE)
	  head_type = OBU_SEQUENCE_HEADER;
    else if (dec_status == AOM_AV1_FRAME_HEAD_PARSER_DONE)
	  head_type = OBU_FRAME_HEADER;
    else if (dec_status == AOM_AV1_FRAME_PARSER_DONE)
	  head_type = OBU_FRAME;
    else if (dec_status == AOM_AV1_REDUNDANT_FRAME_HEAD_PARSER_DONE)
	  head_type = OBU_REDUNDANT_FRAME_HEADER;
    else {
	  return -1;
	}
	av1_print2(AOM_DEBUG_HW_MORE, "load_param: ret 0x%x\n", head_type);
	ATRACE_COUNTER(hw->trace.decode_header_memory_time_name, TRACE_HEADER_RPM_START);
	if (debug&AOM_AV1_DEBUG_SEND_PARAM_WITH_REG) {
	    get_rpm_param(params);
	}
	else {
		for (i = 0; i < (RPM_END-RPM_BEGIN); i += 4) {
			int32_t ii;
			for (ii = 0; ii < 4; ii++) {
				params->l.data[i+ii]=hw->rpm_ptr[i+3-ii];
			}
		}
	}
	ATRACE_COUNTER(hw->trace.decode_header_memory_time_name, TRACE_HEADER_RPM_END);
	params->p.enable_ref_frame_mvs = (params->p.seq_flags >> 7) & 0x1;
	params->p.enable_superres = (params->p.seq_flags >> 15) & 0x1;

    if (debug & AV1_DEBUG_DUMP_DATA) {
		lock_buffer_pool(hw->common.buffer_pool, flags);
	    pr_info("aom_param: (%d)\n", hw->pbi->decode_idx);
	    for ( i = 0; i < (RPM_END-RPM_BEGIN); i++) {
		    pr_info("%04x ", params->l.data[i]);
		    if (((i + 1) & 0xf) == 0)
			    pr_info("\n");
		}
		unlock_buffer_pool(hw->common.buffer_pool, flags);
	}
  return head_type;
}

static int av1_postproc(struct AV1HW_s *hw)
{
	if (hw->postproc_done)
		return 0;
	hw->postproc_done = 1;

	return av1_bufmgr_postproc(hw->pbi, hw->frame_decoded);
}

static void vav1_get_comp_buf_info(struct AV1HW_s *hw,
					struct vdec_comp_buf_info *info)
{
	u16 bit_depth = hw->param.p.bit_depth;

	info->max_size = av1_max_mmu_buf_size(
			hw->max_pic_w,
			hw->max_pic_h);
	info->header_size = av1_get_header_size(
			hw->frame_width,
			hw->frame_height);
	info->frame_buffer_size = av1_mmu_page_num(
			hw, hw->frame_width,
			hw->frame_height,
			bit_depth == 0);
}

static int vav1_get_ps_info(struct AV1HW_s *hw, struct aml_vdec_ps_infos *ps)
{
	ps->visible_width 	= hw->frame_width;
	ps->visible_height 	= hw->frame_height;
	ps->coded_width 	= ALIGN(hw->frame_width, 64);
	ps->coded_height 	= ALIGN(hw->frame_height, 64);
	ps->dpb_size 		= hw->used_buf_num;
	ps->dpb_margin		= hw->dynamic_buf_num_margin;
	if (hw->frame_width > 1920 && hw->frame_height > 1088)
		ps->dpb_frames = 8;
	else
		ps->dpb_frames = 10;

	ps->dpb_frames += 2;

	if (ps->dpb_margin + ps->dpb_frames > MAX_BUF_NUM_NORMAL) {
		u32 delta;
		delta = ps->dpb_margin + ps->dpb_frames - MAX_BUF_NUM_NORMAL;
		ps->dpb_margin -= delta;
		hw->dynamic_buf_num_margin = ps->dpb_margin;
	}
	ps->field = V4L2_FIELD_NONE;

	return 0;
}

static int v4l_res_change(struct AV1HW_s *hw)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct AV1_Common_s *const cm = &hw->common;
	int ret = 0;

	if (ctx->param_sets_from_ucode &&
		hw->res_ch_flag == 0) {
		struct aml_vdec_ps_infos ps;
		struct vdec_comp_buf_info comp;

		if ((cm->width != 0 &&
			cm->height != 0) &&
			(hw->frame_width != cm->width ||
			hw->frame_height != cm->height)) {

			av1_print(hw, 0,
				"%s (%d,%d)=>(%d,%d)\r\n", __func__, cm->width,
				cm->height, hw->frame_width, hw->frame_height);

			if (get_valid_double_write_mode(hw) != 16) {
				vav1_get_comp_buf_info(hw, &comp);
				vdec_v4l_set_comp_buf_info(ctx, &comp);
			}

			vav1_get_ps_info(hw, &ps);
			vdec_v4l_set_ps_infos(ctx, &ps);
			vdec_v4l_res_ch_event(ctx);
			hw->v4l_params_parsed = false;
			hw->res_ch_flag = 1;
			ctx->v4l_resolution_change = 1;
			mutex_lock(&hw->assit_task.assit_mutex);
			hw->eos = 1;

			av1_postproc(hw);

			ATRACE_COUNTER("V_ST_DEC-submit_eos", __LINE__);
			notify_v4l_eos(hw_to_vdec(hw));
			ATRACE_COUNTER("V_ST_DEC-submit_eos", 0);
			mutex_unlock(&hw->assit_task.assit_mutex);
			ret = 1;
		}
	}

	return ret;
}

static int work_space_size_update(struct AV1HW_s *hw)
{
	int workbuf_size, cma_alloc_cnt, ret;
	struct BuffInfo_s *p_buf_info = &hw->work_space_buf_store;
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;

	/* only for 8k workspace update */
	if (!IS_8K_SIZE(hw->max_pic_w, hw->max_pic_h))
		return 0;

	if (hw->cma_alloc_addr && hw->buf_size) {
		memcpy(p_buf_info, &aom_workbuff_spec[2],
			sizeof(struct BuffInfo_s));
		workbuf_size = (p_buf_info->end_adr -
			p_buf_info->start_adr + 0xffff) & (~0xffff);
		cma_alloc_cnt = PAGE_ALIGN(workbuf_size) / PAGE_SIZE;
		if (hw->cma_alloc_count < cma_alloc_cnt) {
			decoder_bmmu_box_free_idx(hw->bmmu_box, WORK_SPACE_BUF_ID);
			hw->buffer_spec_index = 2;
			hw->cma_alloc_count = cma_alloc_cnt;
			ret = decoder_bmmu_box_alloc_buf_phy(hw->bmmu_box,
				WORK_SPACE_BUF_ID, hw->cma_alloc_count * PAGE_SIZE,
				DRIVER_NAME, &hw->cma_alloc_addr);
			if(ret < 0) {
				hw->fatal_error |= DECODER_FATAL_ERROR_NO_MEM;
				vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_NO_MEM);
				pr_err("8k workspace recalloc failed\n");
				return ret;
			}
			hw->buf_start = hw->cma_alloc_addr;
			hw->buf_size = workbuf_size;
			pr_info("8k work_space_buf recalloc, size 0x%x\n", hw->buf_size);
			p_buf_info->start_adr = hw->buf_start;
			if ((get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_GXL) ||
				(get_double_write_mode(hw) == 0x10)) {
				hw->mc_buf_spec.buf_end = hw->buf_start + hw->buf_size;
			}
			init_buff_spec(hw, p_buf_info);
			if ((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) ||
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T3) ||
				(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5W))
				p_buf_info->fgs_table.buf_start = hw->fg_phy_addr;
			hw->work_space_buf = p_buf_info;
			hw->pbi->work_space_buf = p_buf_info;
		}
	}

	return 0;
}

static irqreturn_t vav1_isr_thread_fn(int irq, void *data)
{
	struct AV1HW_s *hw = (struct AV1HW_s *)data;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	unsigned int dec_status = hw->dec_status;
	int obu_type;
	int ret = 0;

	if (dec_status == AOM_AV1_FRAME_HEAD_PARSER_DONE ||
		dec_status == AOM_AV1_SEQ_HEAD_PARSER_DONE ||
		dec_status == AOM_AV1_FRAME_PARSER_DONE) {
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_START);
	}
	else if (dec_status == AOM_AV1_DEC_PIC_END ||
		dec_status == AOM_NAL_DECODE_DONE) {
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_PIC_DONE_START);
	}

	if (hw->eos)
		return IRQ_HANDLED;
	hw->wait_buf = 0;
	if ((dec_status == AOM_NAL_DECODE_DONE) ||
			(dec_status == AOM_SEARCH_BUFEMPTY) ||
			(dec_status == AOM_DECODE_BUFEMPTY)
		) {
		if (hw->m_ins_flag) {
			reset_process_time(hw);
			if (!vdec_frame_based(hw_to_vdec(hw)))
				dec_again_process(hw);
			else {
				hw->dec_result = DEC_RESULT_DONE;
				ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_EDN);
				vdec_schedule_work(&hw->work);
			}
		}
		hw->process_busy = 0;
		return IRQ_HANDLED;
	} else if (dec_status == AOM_AV1_DEC_PIC_END) {
		struct AV1_Common_s *const cm = &hw->common;
		struct PIC_BUFFER_CONFIG_s *frame = &cm->cur_frame->buf;
		struct vdec_s *vdec = hw_to_vdec(hw);

		u32 fg_reg0, fg_reg1, num_y_points, num_cb_points, num_cr_points;
		WRITE_VREG(HEVC_FGS_IDX, 0);
		fg_reg0 = READ_VREG(HEVC_FGS_DATA);
		fg_reg1 = READ_VREG(HEVC_FGS_DATA);
		num_y_points = fg_reg1 & 0xf;
		num_cr_points = (fg_reg1 >> 8) & 0xf;
		num_cb_points = (fg_reg1 >> 4) & 0xf;
		if ((num_y_points > 0) ||
		((num_cb_points > 0) | ((fg_reg0 >> 17) & 0x1)) ||
		((num_cr_points > 0) | ((fg_reg0 >> 17) & 0x1)))
			hw->fgs_valid = 1;
		else
			hw->fgs_valid = 0;
		av1_print(hw, AOM_DEBUG_HW_MORE,
			"fg_data0 0x%x fg_data1 0x%x fg_valid %d\n",
			fg_reg0, fg_reg1, hw->fgs_valid);

		ctx->decoder_status_info.decoder_count++;
		decode_frame_count[hw->index] = hw->frame_count;
		if (hw->m_ins_flag) {
#ifdef USE_DEC_PIC_END
			if (READ_VREG(PIC_END_LCU_COUNT) != 0) {
				hw->frame_decoded = 1;
				if (cm->cur_frame && vdec->mvfrm && frame) {
					frame->hw_decode_time =
					local_clock() - vdec->mvfrm->hw_decode_start;
					frame->frame_size2 = vdec->mvfrm->frame_size;
				}
				hw->gvs->frame_count = hw->frame_count;
				/*
				In c module, multi obus are put in one packet, which is decoded
				with av1_receive_compressed_data().
				For STREAM_MODE or SINGLE_MODE, there is no packet boundary,
				we assume each packet must and only include one picture of data (LCUs)
				 or cm->show_existing_frame is 1
				*/
				av1_print(hw, AOM_DEBUG_HW_MORE,
					"Decoding done (index %d), fgs_valid %d data_size 0x%x shiftbyte 0x%x\n",
					cm->cur_frame? cm->cur_frame->buf.index:-1,
					hw->fgs_valid,
					hw->data_size,
					READ_VREG(HEVC_SHIFT_BYTE_COUNT));
				hw->config_next_ref_info_flag = 1; /*to do: low_latency_flag  case*/
			}
#endif

			if (get_picture_qos)
				get_picture_qos_info(hw);

			reset_process_time(hw);

			if (hw->m_ins_flag &&
				(get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
				(get_double_write_mode(hw) != 0x10) &&
				(debug & AOM_DEBUG_DIS_RECYCLE_MMU_TAIL) == 0) {
				long used_4k_num = (READ_VREG(HEVC_SAO_MMU_STATUS) >> 16);

				if ((cm->cur_frame != NULL) && (cm->cur_fb_idx_mmu != INVALID_IDX)) {
					struct internal_comp_buf *ibuf = index_to_icomp_buf(hw, cm->cur_fb_idx_mmu);
					hevc_mmu_dma_check(hw_to_vdec(hw));

					av1_print(hw, AV1_DEBUG_BUFMGR, "mmu free tail, index %d used_num %d\n",
						cm->cur_fb_idx_mmu, used_4k_num);

					decoder_mmu_box_free_idx_tail(
						ibuf->mmu_box,
						ibuf->index,
						used_4k_num);
#ifdef AOM_AV1_MMU_DW
					if (get_double_write_mode(hw) & 0x20) {
						used_4k_num =
							(READ_VREG(HEVC_SAO_MMU_STATUS2) >> 16);
						decoder_mmu_box_free_idx_tail(
							ibuf->mmu_box_dw,
							ibuf->index,
							used_4k_num);
						av1_print(hw, AOM_DEBUG_HW_MORE, "dw mmu free tail, index %d used_num 0x%x\n",
							cm->cur_frame->buf.index, used_4k_num);
					}
#endif
					cm->cur_fb_idx_mmu = INVALID_IDX;
				}
			}

			if (hw->assit_task.use_sfgs) {
				ulong start_time;
				start_time = local_clock();
				if (cm->cur_frame)
					wait_event_interruptible_timeout(cm->cur_frame->wait_sfgs,
						(atomic_read(&cm->cur_frame->fgs_done) == 1), msecs_to_jiffies(50));
				if (get_debug_fgs() & DEBUG_FGS_CONSUME_TIME) {
					pr_info("%s, pic %d, fgs_valid %d, wait consume time %ld us\n", __func__,
						hw->frame_count - 1, hw->fgs_valid,
						(ulong)(div64_u64(local_clock() - start_time, 1000)));
				}
			}
			if (hw->low_latency_flag)
				av1_postproc(hw);

			if (multi_frames_in_one_pack &&
			hw->frame_decoded &&
			READ_VREG(HEVC_SHIFT_BYTE_COUNT) < hw->data_size) {
				if (enable_single_slice == 1) {
					hw->consume_byte = READ_VREG(HEVC_SHIFT_BYTE_COUNT) - 4;
					hw->dec_result = DEC_RESULT_UNFINISH;
					amhevc_stop();
#ifdef MCRCC_ENABLE
					if (mcrcc_cache_alg_flag)
						dump_hit_rate(hw);
#endif
					ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_EDN);
					vdec_schedule_work(&hw->work);
				}else {
#ifdef DEBUG_CRC_ERROR
					if ((crc_debug_flag & 0x40) && cm->cur_frame)
						dump_mv_buffer(hw, &cm->cur_frame->buf);
#endif
					WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_SEARCH_HEAD);
					av1_print(hw, AOM_DEBUG_HW_MORE,
						"PIC_END, fgs_valid %d search head ...\n",
						hw->fgs_valid);
					if (hw->config_next_ref_info_flag)
						config_next_ref_info_hw(hw);
					ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_EDN);
				}
			} else {
				hw->data_size = 0;
				hw->data_offset = 0;
#ifdef DEBUG_CRC_ERROR
				if ((crc_debug_flag & 0x40) && cm->cur_frame)
					dump_mv_buffer(hw, &cm->cur_frame->buf);
#endif
				hw->dec_result = DEC_RESULT_DONE;
				amhevc_stop();
#ifdef MCRCC_ENABLE
				if (mcrcc_cache_alg_flag)
					dump_hit_rate(hw);
#endif
				ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_EDN);
				vdec_schedule_work(&hw->work);
			}
		} else {
			av1_print(hw, AOM_DEBUG_HW_MORE,
				"PIC_END, fgs_valid %d search head ...\n",
				hw->fgs_valid);
#ifdef USE_DEC_PIC_END
			if (READ_VREG(PIC_END_LCU_COUNT) != 0) {
				hw->frame_decoded = 1;
				/*
				In c module, multi obus are put in one packet, which is decoded
				with av1_receive_compressed_data().
				For STREAM_MODE or SINGLE_MODE, there is no packet boundary,
				we assume each packet must and only include one picture of data (LCUs)
				or cm->show_existing_frame is 1
				*/
				if (cm->cur_frame)
					av1_print(hw, AOM_DEBUG_HW_MORE, "Decoding done (index %d)\n",
						cm->cur_frame? cm->cur_frame->buf.index:-1);
				config_next_ref_info_hw(hw);
			}
#endif
			WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_SEARCH_HEAD);

			if (hw->low_latency_flag) {
				av1_postproc(hw);
				vdec_profile(hw_to_vdec(hw), VDEC_PROFILE_EVENT_CB);
				if (debug & PRINT_FLAG_VDEC_DETAIL)
					pr_info("%s AV1 frame done \n", __func__);
			}
		}

		start_process_time(hw);
		hw->process_busy = 0;
		return IRQ_HANDLED;
	}

	if (dec_status == AOM_EOS) {
		if (hw->m_ins_flag)
			reset_process_time(hw);

		av1_print(hw, AOM_DEBUG_HW_MORE, "AV1_EOS, flush buffer\r\n");

		av1_postproc(hw);

		av1_print(hw, AOM_DEBUG_HW_MORE, "send AV1_10B_DISCARD_NAL\r\n");
		WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_DISCARD_NAL);
		hw->process_busy = 0;
		if (hw->m_ins_flag) {
			hw->dec_result = DEC_RESULT_DONE;
			amhevc_stop();
			vdec_schedule_work(&hw->work);
		}
		return IRQ_HANDLED;
	} else if (dec_status == AOM_DECODE_OVER_SIZE) {
		av1_print(hw, AOM_DEBUG_HW_MORE, "av1  decode oversize !!\n");

		vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
		hw->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
		hw->process_busy = 0;
		if (hw->m_ins_flag)
			reset_process_time(hw);
		return IRQ_HANDLED;
	}

	obu_type = load_param(hw, &hw->aom_param, dec_status);
	if (obu_type < 0) {
		hw->process_busy = 0;
		return IRQ_HANDLED;
	}

	if (obu_type == OBU_SEQUENCE_HEADER) {
		int next_lcu_size;
		int mmu_ret = 0;

		av1_bufmgr_process(hw->pbi, &hw->aom_param, 0, obu_type);

		if ((hw->max_pic_w < hw->aom_param.p.max_frame_width) ||
			(hw->max_pic_h < hw->aom_param.p.max_frame_height)) {
			av1_print(hw, 0, "%s, max size change (%d, %d) -> (%d, %d)\n",
				__func__, hw->max_pic_w, hw->max_pic_h,
				hw->aom_param.p.max_frame_width, hw->aom_param.p.max_frame_height);

			vav1_mmu_map_free(hw);

			hw->max_pic_w = hw->aom_param.p.max_frame_width;
			hw->max_pic_h = hw->aom_param.p.max_frame_height;
			hw->init_pic_w = hw->max_pic_w;
			hw->init_pic_h = hw->max_pic_h;
			hw->pbi->frame_width = hw->init_pic_w;
			hw->pbi->frame_height = hw->init_pic_h;

			mmu_ret = vav1_mmu_map_alloc(hw);
			if (mmu_ret < 0) {
				vdec_v4l_post_error_event(ctx, DECODER_ERROR_ALLOC_BUFFER_FAIL);
			}

			if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
				(get_double_write_mode(hw) != 0x10)) {
				WRITE_VREG(HEVC_SAO_MMU_DMA_CTRL, hw->frame_mmu_map_phy_addr);
			}
#ifdef AOM_AV1_MMU_DW
			if (get_double_write_mode(hw) & 0x20) {
				WRITE_VREG(HEVC_SAO_MMU_DMA_CTRL2, hw->dw_frame_mmu_map_phy_addr);
				//default of 0xffffffff will disable dw
				WRITE_VREG(HEVC_SAO_Y_START_ADDR, 0);
				WRITE_VREG(HEVC_SAO_C_START_ADDR, 0);
			}
#endif

			/*v4l2 alloc new mv when max size changed */
			if (IS_8K_SIZE(hw->max_pic_w, hw->max_pic_h)) {
				/* now less than 8k use fix mv buf size */
				if (hw->pic_list_init_done) {
					if (init_mv_buf_list(hw) < 0)
						pr_err("%s: !!!!Error, reinit_mv_buf_list fail\n", __func__);
				}
			}
		}

		bit_depth_luma = hw->aom_param.p.bit_depth;
		bit_depth_chroma = hw->aom_param.p.bit_depth;
		hw->film_grain_present = hw->aom_param.p.film_grain_present_flag;

		next_lcu_size = ((hw->aom_param.p.seq_flags >> 6) & 0x1) ? 128 : 64;
		hw->video_signal_type = (hw->aom_param.p.video_signal_type << 16
			| hw->aom_param.p.color_description);
		/* When the matrix_coeffiecents, transfer_characteristics and colour_primaries
		 * syntax elements are absent, their values shall be presumed to be equal to 2
		 */
		if ((hw->video_signal_type & 0x1000000) == 0) {
			hw->video_signal_type = hw->video_signal_type & 0xff000000;
			hw->video_signal_type = hw->video_signal_type | 0x20202;
		}

		if (next_lcu_size != hw->current_lcu_size) {
			av1_print(hw, AOM_DEBUG_HW_MORE,
				" ## lcu_size changed from %d to %d\n",
				hw->current_lcu_size, next_lcu_size);
				hw->current_lcu_size = next_lcu_size;
		}

		av1_print(hw, AOM_DEBUG_HW_MORE,
			"AOM_AV1_SEQ_HEAD_PARSER_DONE, search head ...\n");
		WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_SEARCH_HEAD);

		hw->process_busy = 0;
		hw->has_sequence = 1;
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);
		return IRQ_HANDLED;
	}

	// get pixelaspect
	ctx->height_aspect_ratio = 1;
	ctx->width_aspect_ratio = 1;

	hw->frame_width = hw->common.seq_params.max_frame_width;
	hw->frame_height = hw->common.seq_params.max_frame_height;

	if (hw->frame_width == 0 || hw->frame_height == 0) {
		hw->dec_result = DEC_RESULT_DISCARD_DATA;
		hw->process_busy = 0;
		amhevc_stop();
		vdec_schedule_work(&hw->work);
		return IRQ_HANDLED;
	}

	if (!v4l_res_change(hw)) {
		if (ctx->param_sets_from_ucode && !hw->v4l_params_parsed) {
			struct aml_vdec_ps_infos ps;
			struct vdec_comp_buf_info comp;

				pr_info("set ucode parse\n");

				ctx->film_grain_present = hw->film_grain_present;
				if (use_dw_mmu ||
					(ctx->film_grain_present &&
					!disable_fg &&
					(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S4 ||
					get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_S4D))) {
#ifdef AOM_AV1_MMU_DW
					hw->double_write_mode_original = get_double_write_mode(hw);
					ctx->config.parm.dec.cfg.double_write_mode = 0x21;
					pr_info("AV1 has fg, original dw:0x%x use dw 0x21!\n",
						hw->double_write_mode_original);
					if (hw->dw_frame_mmu_map_addr == NULL) {
						u32 mmu_map_size = vaom_dw_frame_mmu_map_size(hw);
						hw->dw_frame_mmu_map_addr =
							decoder_dma_alloc_coherent(&hw->frame_dw_mmu_map_handle,
								mmu_map_size, &hw->dw_frame_mmu_map_phy_addr, "AV1_DWMMU_MAP");
						if (hw->dw_frame_mmu_map_addr == NULL) {
							pr_err("%s: failed to alloc count_buffer\n", __func__);
							return -1;
						}
						memset(hw->dw_frame_mmu_map_addr, 0, mmu_map_size);
					}
#endif
				}
				if (get_valid_double_write_mode(hw) != 16) {
					vav1_get_comp_buf_info(hw, &comp);
				vdec_v4l_set_comp_buf_info(ctx, &comp);
			}

			vav1_get_ps_info(hw, &ps);
			/*notice the v4l2 codec.*/
			vdec_v4l_set_ps_infos(ctx, &ps);
			ctx->decoder_status_info.frame_height = ps.visible_height;
			ctx->decoder_status_info.frame_width = ps.visible_width;
			hw->v4l_params_parsed = true;
			work_space_size_update(hw);
			hw->postproc_done = 0;
			hw->process_busy = 0;
			ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);
			dec_again_process(hw);
			return IRQ_HANDLED;
		} else {
			struct vdec_pic_info pic;

			if (!hw->pic_list_init_done) {
				vdec_v4l_get_pic_info(ctx, &pic);
				hw->used_buf_num = pic.dpb_frames +
					pic.dpb_margin;

				if (IS_4K_SIZE(hw->init_pic_w, hw->init_pic_h)) {
					hw->used_buf_num = MAX_BUF_NUM_LESS + pic.dpb_margin;
					if (hw->used_buf_num > REF_FRAMES_4K)
						hw->mv_buf_margin = hw->used_buf_num - REF_FRAMES_4K + 1;
				}

				if (IS_8K_SIZE(hw->max_pic_w, hw->max_pic_h)) {
					hw->double_write_mode = 4;
					hw->used_buf_num = MAX_BUF_NUM_LESS;
					if (hw->used_buf_num > REF_FRAMES_4K)
						hw->mv_buf_margin = hw->used_buf_num - REF_FRAMES_4K + 1;
					if (((hw->max_pic_w % 64) != 0) &&
						(hw_to_vdec(hw)->canvas_mode != CANVAS_BLKMODE_LINEAR))
						hw->mem_map_mode = 2;
					av1_print(hw, 0,
						"force 8k double write 4, mem_map_mode %d\n", hw->mem_map_mode);
				}

				if (hw->used_buf_num > MAX_BUF_NUM)
					hw->used_buf_num = MAX_BUF_NUM;

				init_pic_list(hw);
				init_pic_list_hw(hw);
#ifndef MV_USE_FIXED_BUF
				if (init_mv_buf_list(hw) < 0) {
					pr_err("%s: !!!!Error, init_mv_buf_list fail\n", __func__);
				}
#endif
				hw->pic_list_init_done = true;
			}
		}
	} else {
		hw->postproc_done = 0;
		hw->process_busy = 0;
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);
		dec_again_process(hw);
		return IRQ_HANDLED;
	}

#ifndef USE_DEC_PIC_END
    if (pbi->bufmgr_proc_count > 0) {
	    if (READ_VREG(PIC_END_LCU_COUNT) != 0) {
		    hw->frame_decoded = 1;
			/*
		    In c module, multi obus are put in one packet, which is decoded
		    with av1_receive_compressed_data().
		    For STREAM_MODE or SINGLE_MODE, there is no packet boundary,
		    we assume each packet must and only include one picture of data (LCUs)
			 or cm->show_existing_frame is 1
			*/
			if (cm->cur_frame)
				av1_print(hw, AOM_DEBUG_HW_MORE, "Decoding done (index %d)\n",
					cm->cur_frame? cm->cur_frame->buf.index:-1);
		}
	}
#endif
/*def CHECK_OBU_REDUNDANT_FRAME_HEADER*/
    if (debug & AOM_DEBUG_BUFMGR_ONLY) {
	    if (READ_VREG(PIC_END_LCU_COUNT) != 0)
		    hw->obu_frame_frame_head_come_after_tile = 0;

	    if (obu_type == OBU_FRAME_HEADER ||
		  obu_type == OBU_FRAME) {
		  hw->obu_frame_frame_head_come_after_tile = 1;
		} else if (obu_type == OBU_REDUNDANT_FRAME_HEADER &&
			hw->obu_frame_frame_head_come_after_tile == 0) {
			if (hw->frame_decoded == 1) {
				av1_print(hw, AOM_DEBUG_HW_MORE,
					"Warning, OBU_REDUNDANT_FRAME_HEADER come without OBU_FRAME or OBU_FRAME_HEAD\n");
				hw->frame_decoded = 0;
			}
		}
	}
	if (hw->frame_decoded)
		hw->one_compressed_data_done = 1;

	if (hw->m_ins_flag)
		reset_process_time(hw);

	if (hw->process_state != PROC_STATE_SENDAGAIN) {
	    if (hw->one_compressed_data_done) {
	        av1_postproc(hw);
	        av1_release_bufs(hw);
#ifndef MV_USE_FIXED_BUF
	        put_un_used_mv_bufs(hw);
#endif
	    }
	}

	if (hw->one_package_frame_cnt) {
		if (get_free_buf_count(hw) <= 0) {
			hw->dec_result = AOM_AV1_RESULT_NEED_MORE_BUFFER;
			hw->cur_obu_type = obu_type;
			hw->process_busy = 0;
			ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);
			vdec_schedule_work(&hw->work);
			return IRQ_HANDLED;
		}
	}
	hw->one_package_frame_cnt++;

	ret = av1_continue_decoding(hw, obu_type);
	hw->postproc_done = 0;
	hw->process_busy = 0;

	if (hw->m_ins_flag) {
		if (ret >= 0)
			start_process_time(hw);
		else {
			hw->dec_result = DEC_RESULT_DONE;
			amhevc_stop();
			vdec_schedule_work(&hw->work);
		}
	}
	ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);
	return IRQ_HANDLED;
}

static irqreturn_t vav1_isr(int irq, void *data)
{
	int i;
	unsigned int dec_status;
	struct AV1HW_s *hw = (struct AV1HW_s *)data;
	uint debug_tag;

	WRITE_VREG(HEVC_ASSIST_MBOX0_CLR_REG, 1);

	dec_status = READ_VREG(HEVC_DEC_STATUS_REG) & 0xff;

	if (dec_status == AOM_AV1_FRAME_HEAD_PARSER_DONE ||
		dec_status == AOM_AV1_SEQ_HEAD_PARSER_DONE ||
		dec_status == AOM_AV1_FRAME_PARSER_DONE) {
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_HEAD_DONE);
	}
	else if (dec_status == AOM_AV1_DEC_PIC_END ||
		dec_status == AOM_NAL_DECODE_DONE) {
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_PIC_DONE);
	}
	if (!hw)
		return IRQ_HANDLED;
	if (hw->init_flag == 0)
		return IRQ_HANDLED;
	if (hw->process_busy) {/*on process.*/
		pr_err("err: %s, process busy\n", __func__);
		return IRQ_HANDLED;
	}

	ATRACE_COUNTER("V_ST_DEC-decode_state", dec_status);

	hw->dec_status = dec_status;
	hw->process_busy = 1;
	if (debug & AV1_DEBUG_BUFMGR)
		av1_print(hw, AV1_DEBUG_BUFMGR,
			"av1 isr (%d) dec status  = 0x%x (0x%x), lcu 0x%x shiftbyte 0x%x shifted_data 0x%x (%x %x lev %x, wr %x, rd %x) log %x\n",
			irq,
			dec_status, READ_VREG(HEVC_DEC_STATUS_REG),
			READ_VREG(HEVC_PARSER_LCU_START),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFTED_DATA),
			READ_VREG(HEVC_STREAM_START_ADDR),
			READ_VREG(HEVC_STREAM_END_ADDR),
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR),
#ifdef DEBUG_UCODE_LOG
			READ_VREG(HEVC_DBG_LOG_ADR)
#else
			0
#endif
		);
#ifdef DEBUG_UCODE_LOG
	if ((udebug_flag & 0x8) &&
		(hw->ucode_log_addr != 0) &&
		(READ_VREG(HEVC_DEC_STATUS_REG) & 0x100)) {
	    unsigned long flags;
		unsigned short *log_adr =
			(unsigned short *)hw->ucode_log_addr;
		lock_buffer_pool(hw->pbi->common.buffer_pool, flags);
		while (*(log_adr + 3)) {
			pr_info("dbg%04x %04x %04x %04x\n",
				*(log_adr + 3), *(log_adr + 2), *(log_adr + 1), *(log_adr + 0)
				);
			log_adr += 4;
		}
		unlock_buffer_pool(hw->pbi->common.buffer_pool, flags);
	}
#endif
	debug_tag = READ_HREG(DEBUG_REG1);
	if (debug_tag & 0x10000) {
		pr_info("LMEM<tag %x>:\n", READ_HREG(DEBUG_REG1));
		for (i = 0; i < 0x400; i += 4) {
			int ii;
			if ((i & 0xf) == 0)
				pr_info("%03x: ", i);
			for (ii = 0; ii < 4; ii++) {
				pr_info("%04x ",
					   hw->lmem_ptr[i + 3 - ii]);
			}
			if (((i + ii) & 0xf) == 0)
				pr_info("\n");
		}
		if (((udebug_pause_pos & 0xffff)
			== (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx == hw->result_done_count) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_HREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			hw->ucode_pause_pos = udebug_pause_pos;
		}
		else if (debug_tag & 0x20000)
			hw->ucode_pause_pos = 0xffffffff;
		if (hw->ucode_pause_pos)
			reset_process_time(hw);
		else
			WRITE_HREG(DEBUG_REG1, 0);
	} else if (debug_tag != 0) {
		pr_info(
			"dbg%x: %x lcu %x\n", READ_HREG(DEBUG_REG1),
			   READ_HREG(DEBUG_REG2),
			   READ_VREG(HEVC_PARSER_LCU_START));

		if (((udebug_pause_pos & 0xffff)
			== (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx == hw->result_done_count) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_HREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			hw->ucode_pause_pos = udebug_pause_pos;
		}
		if (hw->ucode_pause_pos)
			reset_process_time(hw);
		else
			WRITE_HREG(DEBUG_REG1, 0);
		hw->process_busy = 0;
		return IRQ_HANDLED;
	}

	if (hw->dec_status == AOM_AV1_FGS_PARAM) {
	    uint32_t status_val = READ_VREG(HEVC_FG_STATUS);
	    WRITE_VREG(HEVC_FG_STATUS, AOM_AV1_FGS_PARAM_CONT);
	    WRITE_VREG(HEVC_DEC_STATUS_REG, AOM_AV1_FGS_PARAM_CONT);
			// Bit[11] - 0 Read, 1 - Write
			// Bit[10:8] - film_grain_params_ref_idx // For Write request
	    if ((status_val >> 11) & 0x1) {
		    uint32_t film_grain_params_ref_idx = (status_val >> 8) & 0x7;
		    config_film_grain_reg(hw, film_grain_params_ref_idx);
		}
	    else
		    read_film_grain_reg(hw);

		film_grain_task_wakeup(hw);

		hw->process_busy = 0;
	    return IRQ_HANDLED;
	}

	if (!hw->m_ins_flag) {
		av1_print(hw, AV1_DEBUG_BUFMGR,
			"error flag = %d\n", hw->error_flag);
		if (hw->error_flag == 1) {
			hw->error_flag = 2;
			hw->process_busy = 0;
			return IRQ_HANDLED;
		} else if (hw->error_flag == 3) {
			hw->process_busy = 0;
			return IRQ_HANDLED;
		}
		if (get_free_buf_count(hw) <= 0) {
			hw->wait_buf = 1;
			hw->process_busy = 0;
			av1_print(hw, AV1_DEBUG_BUFMGR,
				"free buf not enough = %d\n",
				get_free_buf_count(hw));
			return IRQ_HANDLED;
		}
	}
	ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_END);
	return IRQ_WAKE_THREAD;
}

static void av1_set_clk(struct work_struct *work)
{
	struct AV1HW_s *hw = container_of(work,
		struct AV1HW_s, set_clk_work);
	int fps = 96000 / hw->frame_dur;

	if (hevc_source_changed(VFORMAT_AV1,
		frame_width, frame_height, fps) > 0)
		hw->saved_resolution = frame_width *
		frame_height * fps;
}

static void vav1_put_timer_func(struct timer_list *timer)
{
	struct AV1HW_s *hw = container_of(timer,
		struct AV1HW_s, timer);
	uint8_t empty_flag;
	unsigned int buf_level;

	enum receviver_start_e state = RECEIVER_INACTIVE;

	if (hw->init_flag == 0) {
		if (hw->stat & STAT_TIMER_ARM) {
			timer->expires = jiffies + PUT_INTERVAL;
			add_timer(&hw->timer);
		}
		return;
	}
	if (hw->m_ins_flag == 0) {
		if (vf_get_receiver(hw->provider_name)) {
			state =
				vf_notify_receiver(hw->provider_name,
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
			if ((debug & AV1_DEBUG_DIS_LOC_ERROR_PROC) == 0) {

				buf_level = READ_VREG(HEVC_STREAM_LEVEL);
				/* receiver has no buffer to recycle */
				if ((state == RECEIVER_INACTIVE) &&
					(kfifo_is_empty(&hw->display_q) &&
					 buf_level > 0x200)) {
					WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
				}
			}

		}
	}
#ifdef MULTI_INSTANCE_SUPPORT
	else {
		if (
			(decode_timeout_val > 0) &&
			(hw->start_process_time > 0) &&
			((1000 * (jiffies - hw->start_process_time) / HZ)
				> decode_timeout_val)
		) {
			int current_lcu_idx =
				READ_VREG(HEVC_PARSER_LCU_START)
				& 0xffffff;
			av1_print(hw, AV1_DEBUG_TIMEOUT_INFO, "%s:current_lcu_idx = %u last_lcu_idx = %u decode_timeout_count = %d\n",
						__func__, current_lcu_idx, hw->last_lcu_idx, hw->decode_timeout_count);
			if (hw->last_lcu_idx == current_lcu_idx) {
				if (hw->decode_timeout_count > 0)
					hw->decode_timeout_count--;
				if (hw->decode_timeout_count == 0) {
					if (input_frame_based(
						hw_to_vdec(hw)) ||
					(READ_VREG(HEVC_STREAM_LEVEL) > 0x200))
						timeout_process(hw);
					else {
						av1_print(hw, 0,
							"timeout & empty, again\n");
						dec_again_process(hw);
					}
				}
			} else {
				start_process_time(hw);
				hw->last_lcu_idx = current_lcu_idx;
			}
		}
	}
#endif

	if ((hw->ucode_pause_pos != 0) &&
		(hw->ucode_pause_pos != 0xffffffff) &&
		udebug_pause_pos != hw->ucode_pause_pos) {
		hw->ucode_pause_pos = 0;
		WRITE_HREG(DEBUG_REG1, 0);
	}
#ifdef MULTI_INSTANCE_SUPPORT
	if (debug & AV1_DEBUG_DUMP_DATA) {
		debug &= ~AV1_DEBUG_DUMP_DATA;
		av1_print(hw, 0,
			"%s: chunk size 0x%x off 0x%x sum 0x%x\n",
			__func__,
			hw->data_size,
			hw->data_offset,
			get_data_check_sum(hw, hw->data_size)
			);
		dump_data(hw, hw->data_size);
	}
#endif
	if (debug & AV1_DEBUG_DUMP_PIC_LIST) {
		av1_dump_state(hw_to_vdec(hw));
		debug &= ~AV1_DEBUG_DUMP_PIC_LIST;
	}
	if (debug & AV1_DEBUG_TRIG_SLICE_SEGMENT_PROC) {
		WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
		debug &= ~AV1_DEBUG_TRIG_SLICE_SEGMENT_PROC;
	}

	if (radr != 0) {
		if ((radr >> 24) != 0) {
			int count = radr >> 24;
			int adr = radr & 0xffffff;
			int i;
			for (i = 0; i < count; i++)
				pr_info("READ_VREG(%x)=%x\n", adr+i, READ_VREG(adr+i));
		} else if (rval != 0) {
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

			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXBB &&
				get_double_write_mode(hw) == 0) {
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
	if (hw->get_frame_dur && hw->show_frame_num > 60 &&
		hw->frame_dur > 0 && hw->saved_resolution !=
		frame_width * frame_height *
			(96000 / hw->frame_dur))
		vdec_schedule_work(&hw->set_clk_work);

	timer->expires = jiffies + PUT_INTERVAL;
	add_timer(timer);
}

int vav1_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	struct AV1HW_s *av1 =
		(struct AV1HW_s *)vdec->private;

	if (!av1)
		return -1;

	vstatus->frame_width = frame_width;
	vstatus->frame_height = frame_height;
	if (av1->frame_dur != 0)
		vstatus->frame_rate = 96000 / av1->frame_dur;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = 0;
	vstatus->status = av1->stat | av1->fatal_error;
	vstatus->frame_dur = av1->frame_dur;
	vstatus->bit_rate = av1->gvs->bit_rate;
	vstatus->frame_data = av1->gvs->frame_data;
	vstatus->total_data = av1->gvs->total_data;
	vstatus->frame_count = av1->gvs->frame_count;
	vstatus->error_frame_count = av1->gvs->error_frame_count;
	vstatus->drop_frame_count = av1->gvs->drop_frame_count;
	vstatus->samp_cnt = av1->gvs->samp_cnt;
	vstatus->offset = av1->gvs->offset;
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
		"%s", DRIVER_NAME);
	return 0;
}

int vav1_set_isreset(struct vdec_s *vdec, int isreset)
{
	is_reset = isreset;
	return 0;
}

static void vav1_prot_init(struct AV1HW_s *hw, u32 mask)
{
	unsigned int data32;
	av1_print(hw, AOM_DEBUG_HW_MORE, "%s %d\n", __func__, __LINE__);

	aom_config_work_space_hw(hw, mask);
	if (mask & HW_MASK_BACK) {
		//to do: .. for single instance, called after init_pic_list()
		if (hw->m_ins_flag)
			init_pic_list_hw(hw);
	}

	aom_init_decoder_hw(hw, mask);

#ifdef AOM_AV1_DBLK_INIT
	av1_print(hw, AOM_DEBUG_HW_MORE,
	"[test.c] av1_loop_filter_init (run once before decoding start)\n");
    av1_loop_filter_init(hw->lfi, hw->lf);
#endif
	if ((mask & HW_MASK_FRONT) == 0)
		return;
#if 1
	if (debug & AV1_DEBUG_BUFMGR_MORE)
		pr_info("%s\n", __func__);
	data32 = READ_VREG(HEVC_STREAM_CONTROL);
	data32 = data32 |
		(1 << 0)/*stream_fetch_enable*/
		;
	WRITE_VREG(HEVC_STREAM_CONTROL, data32);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
	    if (debug & AV1_DEBUG_BUFMGR)
		    pr_info("[test.c] Config STREAM_FIFO_CTL\n");
	    data32 = READ_VREG(HEVC_STREAM_FIFO_CTL);
	    data32 = data32 |
				 (1 << 29) // stream_fifo_hole
				 ;
	    WRITE_VREG(HEVC_STREAM_FIFO_CTL, data32);
	}
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x000000001);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x00000300);
#endif

	WRITE_VREG(HEVC_WAIT_FLAG, 1);

	/* WRITE_VREG(HEVC_MPSR, 1); */

	/* clear mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_CLR_REG, 1);

	/* enable mailbox interrupt */
	WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 1);

	/* disable PSCALE for hardware sharing */
	WRITE_VREG(HEVC_PSCALE_CTRL, 0);

	WRITE_VREG(DEBUG_REG1, 0x0);
	/*check vps/sps/pps/i-slice in ucode*/
	WRITE_VREG(NAL_SEARCH_CTL, 0x8);

	WRITE_VREG(DECODE_STOP_POS, udebug_flag);
//#if (defined DEBUG_UCODE_LOG) || (defined DEBUG_CMD)
//	WRITE_VREG(HEVC_DBG_LOG_ADR, hw->ucode_log_phy_addr);
//#endif
}

static int vav1_local_init(struct AV1HW_s *hw, bool reset_flag)
{
	int i;
	int ret;
	int width, height;
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;

	hw->gvs = vzalloc(sizeof(struct vdec_info));
	if (NULL == hw->gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -1;
	}
#ifdef DEBUG_PTS
	hw->pts_missed = 0;
	hw->pts_hit = 0;
#endif
	hw->new_frame_displayed = 0;
	hw->last_put_idx = -1;
	hw->saved_resolution = 0;
	hw->get_frame_dur = false;
	on_no_keyframe_skiped = 0;
	hw->first_pts_index = 0;
	hw->dur_recalc_flag = 0;
	hw->av1_first_pts_ready = false;
	width = hw->vav1_amstream_dec_info.width;
	height = hw->vav1_amstream_dec_info.height;
	hw->frame_dur =
		(hw->vav1_amstream_dec_info.rate ==
		 0) ? 3200 : hw->vav1_amstream_dec_info.rate;
	if (width && height)
		hw->frame_ar = height * 0x100 / width;
/*
 *TODO:FOR VERSION
 */
	pr_info("av1: ver (%d,%d) decinfo: %dx%d rate=%d\n", av1_version,
		   0, width, height, hw->frame_dur);

	if (hw->frame_dur == 0)
		hw->frame_dur = 96000 / 24;

	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->newframe_q);

	for (i = 0; i < FRAME_BUFFERS; i++) {
		hw->buffer_wrap[i] = i;
	}

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hw->vfpool[i];

		hw->vfpool[i].index = -1;
		kfifo_put(&hw->newframe_q, vf);
	}

	ret = av1_local_init(hw, reset_flag);

	if (ret < 0) {
		vdec_v4l_post_error_event(ctx, DECODER_ERROR_ALLOC_BUFFER_FAIL);
	}

	if (force_pts_unstable) {
		if (!hw->pts_unstable) {
			hw->pts_unstable =
			(hw->vav1_amstream_dec_info.rate == 0)?1:0;
			pr_info("set pts unstable\n");
		}
	}

	return ret;
}


#ifdef MULTI_INSTANCE_SUPPORT
static s32 vav1_init(struct vdec_s *vdec)
{
	struct AV1HW_s *hw = (struct AV1HW_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;
#else
static s32 vav1_init(struct AV1HW_s *hw)
{
#endif
	int ret;
	int i;
	int fw_size = 0x1000 * 16;
	struct firmware_s *fw = NULL;

	hw->stat |= STAT_TIMER_INIT;

	if (vav1_local_init(hw, false) < 0)
		return -EBUSY;

	fw = vmalloc(sizeof(struct firmware_s) + fw_size);
	if (IS_ERR_OR_NULL(fw))
		return -ENOMEM;

	av1_print(hw, AOM_DEBUG_HW_MORE, "%s %d\n", __func__, __LINE__);
#ifdef DEBUG_USE_VP9_DEVICE_NAME
	if (get_firmware_data(VIDEO_DEC_VP9_MMU, fw->data) < 0) {
#else
	if (get_firmware_data(VIDEO_DEC_AV1_MMU, fw->data) < 0) {
#endif
		pr_err("get firmware fail.\n");
		printk("%s %d\n", __func__, __LINE__);
		vfree(fw);
		return -1;
	}
	av1_print(hw, AOM_DEBUG_HW_MORE, "%s %d\n", __func__, __LINE__);
	fw->len = fw_size;

	INIT_WORK(&hw->set_clk_work, av1_set_clk);
	timer_setup(&hw->timer, vav1_put_timer_func, 0);
	spin_lock_init(&hw->wait_buf_lock);

	for (i = 0; i < FILM_GRAIN_REG_SIZE; i++) {
		WRITE_VREG(HEVC_FGS_DATA, 0);
	}
	WRITE_VREG(HEVC_FGS_CTRL, 0);

#ifdef MULTI_INSTANCE_SUPPORT
	if (hw->m_ins_flag) {
		hw->timer.expires = jiffies + PUT_INTERVAL;

		INIT_WORK(&hw->work, av1_work);
		hw->fw = fw;

		return 0;	/*multi instance return */
	}
#endif
	amhevc_enable();

	ret = amhevc_loadmc_ex(VFORMAT_AV1, NULL, fw->data);
	if (ret < 0) {
		amhevc_disable();
		vfree(fw);
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_FW_LOAD_ERROR);
		pr_err("AV1: the %s fw loading failed, err: %x\n",
			tee_enabled() ? "TEE" : "local", ret);
		return -EBUSY;
	}

	vfree(fw);

	hw->stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	vav1_prot_init(hw, HW_MASK_FRONT | HW_MASK_BACK);

	if (vdec_request_threaded_irq(VDEC_IRQ_0,
				vav1_isr,
				vav1_isr_thread_fn,
				IRQF_ONESHOT,/*run thread on this irq disabled*/
				"vav1-irq", (void *)hw)) {
		pr_info("vav1 irq register error.\n");
		amhevc_disable();
		return -ENOENT;
	}

	hw->stat |= STAT_ISR_REG;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (force_dv_enable)
		hw->provider_name = DV_PROVIDER_NAME;
	else
#endif
		hw->provider_name = PROVIDER_NAME;
#ifndef MULTI_INSTANCE_SUPPORT
	vf_provider_init(&vav1_vf_prov, hw->provider_name, &vav1_vf_provider,
					 hw);
	vf_reg_provider(&vav1_vf_prov);
	vf_notify_receiver(hw->provider_name, VFRAME_EVENT_PROVIDER_START, NULL);
	if (!is_reset)
		vf_notify_receiver(hw->provider_name, VFRAME_EVENT_PROVIDER_FR_HINT,
		(void *)((unsigned long)hw->frame_dur));
#endif
	hw->stat |= STAT_VF_HOOK;

	hw->timer.expires = jiffies + PUT_INTERVAL;
	add_timer(&hw->timer);

	hw->stat |= STAT_VDEC_RUN;
	hw->stat |= STAT_TIMER_ARM;

	amhevc_start();

	hw->init_flag = 1;
	hw->process_busy = 0;
	pr_info("%d, vav1_init, RP=0x%x\n",
		__LINE__, READ_VREG(HEVC_STREAM_RD_PTR));
	return 0;
}

static int vmav1_stop(struct AV1HW_s *hw)
{
	hw->init_flag = 0;

	if (hw->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}
	if (hw->stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_0, (void *)hw);
		hw->stat &= ~STAT_ISR_REG;
	}
	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}

	av1_local_uninit(hw, false);

	reset_process_time(hw);
	cancel_work_sync(&hw->work);
	cancel_work_sync(&hw->set_clk_work);
	uninit_mmu_buffers(hw);
	if (hw->fw)
		vfree(hw->fw);
	hw->fw = NULL;
	return 0;
}

static int amvdec_av1_mmu_init(struct AV1HW_s *hw)
{
	int tvp_flag = vdec_secure(hw_to_vdec(hw)) ?
		CODEC_MM_FLAGS_TVP : 0;
	int buf_size = 48;
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;

	if ((hw->max_pic_w * hw->max_pic_h > 1280*736) &&
		(hw->max_pic_w * hw->max_pic_h <= 1920*1088)) {
		buf_size = 12;
	} else if ((hw->max_pic_w * hw->max_pic_h > 0) &&
		(hw->max_pic_w * hw->max_pic_h <= 1280*736)) {
		buf_size = 4;
	}
	hw->need_cache_size = buf_size * SZ_1M;
	hw->sc_start_time = get_jiffies_64();

	hw->bmmu_box = decoder_bmmu_box_alloc_box(
			DRIVER_NAME,
			hw->index,
			MAX_BMMU_BUFFER_NUM,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER |
			tvp_flag,
			BMMU_ALLOC_FLAGS_WAIT);
	av1_print(hw, AV1_DEBUG_BUFMGR,
		"%s, MAX_BMMU_BUFFER_NUM = %d\n",
		__func__,
		MAX_BMMU_BUFFER_NUM);
	if (!hw->bmmu_box) {
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_NO_MEM);
		pr_err("av1 alloc bmmu box failed!!\n");
		return -1;
	}
	return 0;
}

/****************************************/
#ifdef CONFIG_PM
static int av1_suspend(struct device *dev)
{
	amhevc_suspend(to_platform_device(dev), dev->power.power_state);
	return 0;
}

static int av1_resume(struct device *dev)
{
	amhevc_resume(to_platform_device(dev));
	return 0;
}

static const struct dev_pm_ops av1_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(av1_suspend, av1_resume)
};
#endif

static struct codec_profile_t amvdec_av1_profile = {
	.name = "AV1-V4L",
	.profile = ""
};

static unsigned int get_data_check_sum
	(struct AV1HW_s *hw, int size)
{
	int sum = 0;
	u8 *data = NULL;

	if (!hw->chunk->block->is_mapped)
		data = codec_mm_vmap(hw->chunk->block->start +
			hw->data_offset, size);
	else
		data = ((u8 *)hw->chunk->block->start_virt) +
			hw->data_offset;

	sum = crc32_le(0, data, size);

	if (!hw->chunk->block->is_mapped)
		codec_mm_unmap_phyaddr(data);
	return sum;
}

static void dump_data(struct AV1HW_s *hw, int size)
{
	int jj;
	u8 *data = NULL;
	int padding_size = hw->data_offset &
		(VDEC_FIFO_ALIGN - 1);

	if (!hw->chunk->block->is_mapped)
		data = codec_mm_vmap(hw->chunk->block->start +
			hw->data_offset, size);
	else
		data = ((u8 *)hw->chunk->block->start_virt) +
			hw->data_offset;

	av1_print(hw, 0, "padding: ");
	for (jj = padding_size; jj > 0; jj--)
		av1_print_cont(hw, 0, "%02x ", *(data - jj));
	av1_print_cont(hw, 0, "data adr %p\n", data);

	for (jj = 0; jj < size; jj++) {
		if ((jj & 0xf) == 0)
			av1_print(hw, 0, "%06x:", jj);
		av1_print_cont(hw, 0, "%02x ", data[jj]);
		if (((jj + 1) & 0xf) == 0)
			av1_print(hw, 0, "\n");
	}
	av1_print(hw, 0, "\n");

	if (!hw->chunk->block->is_mapped)
		codec_mm_unmap_phyaddr(data);
}

static int av1_wait_cap_buf(void *args)
{
	struct AV1HW_s *hw =
		(struct AV1HW_s *) args;
	struct aml_vcodec_ctx * ctx =
		(struct aml_vcodec_ctx *)hw->v4l2_ctx;
	ulong flags;
	int ret = 0;

	ret = wait_event_interruptible_timeout(ctx->cap_wq,
		(ctx->is_stream_off || (get_free_buf_count(hw) > 0)),
		msecs_to_jiffies(300));
	if (ret <= 0){
		av1_print(hw, PRINT_FLAG_V4L_DETAIL, "%s, wait cap buf %d\n",
			__func__, ret);
	}

	spin_lock_irqsave(&hw->wait_buf_lock, flags);
	if (hw->wait_more_buf) {
		hw->wait_more_buf = false;
		hw->dec_result = ctx->is_stream_off ?
		DEC_RESULT_FORCE_EXIT :
		AOM_AV1_RESULT_NEED_MORE_BUFFER;
		vdec_schedule_work(&hw->work);
	}
	spin_unlock_irqrestore(&hw->wait_buf_lock, flags);

	av1_print(hw, PRINT_FLAG_V4L_DETAIL,
		"%s wait capture buffer end, ret:%d\n",
		__func__, ret);
	return 0;
}

static void av1_work(struct work_struct *work)
{
	struct AV1HW_s *hw = container_of(work,
		struct AV1HW_s, work);
	struct vdec_s *vdec = hw_to_vdec(hw);
	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */
	if (hw->dec_result == DEC_RESULT_AGAIN)
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_WORKER_AGAIN);
	if (hw->dec_result != AOM_AV1_RESULT_NEED_MORE_BUFFER)
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_WORKER_START);
	av1_print(hw, PRINT_FLAG_VDEC_DETAIL,
		"%s dec_result %d %x %x %x\n",
		__func__,
		hw->dec_result,
		READ_VREG(HEVC_STREAM_LEVEL),
		READ_VREG(HEVC_STREAM_WR_PTR),
		READ_VREG(HEVC_STREAM_RD_PTR));

	ATRACE_COUNTER("V_ST_DEC-work_state", hw->dec_result);

	if (hw->dec_result == AOM_AV1_RESULT_NEED_MORE_BUFFER) {
		reset_process_time(hw);
		if (get_free_buf_count(hw) <= 0) {
			ulong flags;
			int ret;

			spin_lock_irqsave(&hw->wait_buf_lock, flags);

			hw->dec_result = AOM_AV1_RESULT_NEED_MORE_BUFFER;
			if (vdec->next_status == VDEC_STATUS_DISCONNECTED) {
				hw->dec_result = DEC_RESULT_AGAIN;
				vdec_schedule_work(&hw->work);
			} else {
				hw->wait_more_buf = true;
			}
			spin_unlock_irqrestore(&hw->wait_buf_lock, flags);

			if (hw->wait_more_buf) {
				ATRACE_COUNTER("V_ST_DEC-wait_more_buff", __LINE__);
				ret = vdec_post_task(av1_wait_cap_buf, hw);
				if (ret != 0) {
					pr_err("post task create failed!!!! ret %d\n", ret);
					spin_lock_irqsave(&hw->wait_buf_lock, flags);
					hw->wait_more_buf = false;
					hw->dec_result = AOM_AV1_RESULT_NEED_MORE_BUFFER;
					vdec_schedule_work(&hw->work);
					spin_unlock_irqrestore(&hw->wait_buf_lock, flags);
				}
			}

		} else {
			ATRACE_COUNTER("V_ST_DEC-wait_more_buff", 0);
			av1_release_bufs(hw);
			hw->postproc_done = 0;
			av1_continue_decoding(hw, hw->cur_obu_type);
			start_process_time(hw);
			av1_print(hw, PRINT_FLAG_VDEC_DETAIL,
				"NEED_MORE_BUFFER work done!\n");
		}
		return;
	}

	if (((hw->dec_result == DEC_RESULT_GET_DATA) ||
		(hw->dec_result == DEC_RESULT_GET_DATA_RETRY))
		&& (hw_to_vdec(hw)->next_status !=
		VDEC_STATUS_DISCONNECTED)) {
		if (!vdec_has_more_input(vdec)) {
			hw->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hw->work);
			return;
		}

		if (hw->dec_result == DEC_RESULT_GET_DATA) {
			av1_print(hw, PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_GET_DATA %x %x %x\n",
				__func__,
				READ_VREG(HEVC_STREAM_LEVEL),
				READ_VREG(HEVC_STREAM_WR_PTR),
				READ_VREG(HEVC_STREAM_RD_PTR));
			vdec_vframe_dirty(vdec, hw->chunk);
			vdec_clean_input(vdec);
		}

		if (get_free_buf_count(hw) >=
			hw->run_ready_min_buf_num) {
			int r;
			int decode_size;
			r = vdec_prepare_input(vdec, &hw->chunk);
			if (r < 0) {
				hw->dec_result = DEC_RESULT_GET_DATA_RETRY;

				av1_print(hw,
					PRINT_FLAG_VDEC_DETAIL,
					"amvdec_vh265: Insufficient data\n");

				vdec_schedule_work(&hw->work);
				return;
			}
			hw->dec_result = DEC_RESULT_NONE;
			av1_print(hw, PRINT_FLAG_VDEC_STATUS,
				"%s: chunk size 0x%x sum 0x%x\n",
				__func__, r,
				(debug & PRINT_FLAG_VDEC_STATUS) ?
				get_data_check_sum(hw, r) : 0
				);

			if (debug & PRINT_FLAG_VDEC_DATA)
				dump_data(hw, hw->data_size);

			decode_size = hw->data_size +
				(hw->data_offset & (VDEC_FIFO_ALIGN - 1));

			WRITE_VREG(HEVC_DECODE_SIZE,
				READ_VREG(HEVC_DECODE_SIZE) + decode_size);

			vdec_enable_input(vdec);

			WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);

			start_process_time(hw);

		} else {
			hw->dec_result = DEC_RESULT_GET_DATA_RETRY;

			av1_print(hw, PRINT_FLAG_VDEC_DETAIL,
				"amvdec_vh265: Insufficient data\n");

			vdec_schedule_work(&hw->work);
		}
		return;
	} else if (hw->dec_result == DEC_RESULT_DONE) {
		hw->result_done_count++;
		hw->process_state = PROC_STATE_INIT;

		av1_print(hw, PRINT_FLAG_VDEC_STATUS,
			"%s (===> %d) dec_result %d (%d) %x %x %x shiftbytes 0x%x decbytes 0x%x\n",
			__func__,
			hw->frame_count,
			hw->dec_result,
			hw->result_done_count,
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
			hw->start_shift_bytes
			);
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
	} else if (hw->dec_result == DEC_RESULT_AGAIN) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		if (!vdec_has_more_input(vdec)) {
			hw->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hw->work);
			return;
		}
	} else if (hw->dec_result == DEC_RESULT_EOS) {
		av1_print(hw, PRINT_FLAG_VDEC_STATUS,
			"%s: end of stream\n", __func__);
		mutex_lock(&hw->assit_task.assit_mutex);
		hw->eos = 1;
		av1_postproc(hw);

		ATRACE_COUNTER("V_ST_DEC-submit_eos", __LINE__);
		notify_v4l_eos(hw_to_vdec(hw));
		ATRACE_COUNTER("V_ST_DEC-submit_eos", 0);
		mutex_unlock(&hw->assit_task.assit_mutex);

		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
	} else if (hw->dec_result == DEC_RESULT_FORCE_EXIT) {
		av1_print(hw, PRINT_FLAG_VDEC_STATUS,
			"%s: force exit\n", __func__);
		if (hw->stat & STAT_VDEC_RUN) {
			amhevc_stop();
			hw->stat &= ~STAT_VDEC_RUN;
		}

		if (hw->stat & STAT_ISR_REG) {
#ifdef MULTI_INSTANCE_SUPPORT
			if (!hw->m_ins_flag)
#endif
				WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 0);
			vdec_free_irq(VDEC_IRQ_0, (void *)hw);
			hw->stat &= ~STAT_ISR_REG;
		}
	} else if (hw->dec_result == DEC_RESULT_DISCARD_DATA) {
		av1_print(hw, PRINT_FLAG_VDEC_STATUS,
			"%s (===> %d) dec_result %d (%d) %x %x %x shiftbytes 0x%x decbytes 0x%x discard pic!\n",
			__func__,
			hw->frame_count,
			hw->dec_result,
			hw->result_done_count,
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT) -
			hw->start_shift_bytes
			);
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
	} else if (hw->dec_result == DEC_RESULT_UNFINISH) {
		hw->result_done_count++;
		hw->process_state = PROC_STATE_INIT;

		av1_print(hw, PRINT_FLAG_VDEC_STATUS,
			"%s (===> %d) dec_result %d (%d) %x %x %x shiftbytes 0x%x decbytes 0x%x\n",
			__func__,
			hw->frame_count,
			hw->dec_result,
			hw->result_done_count,
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT) - hw->start_shift_bytes);
		amhevc_stop();
	}

	if (hw->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}

	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}
	ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_WORKER_END);
	/* mark itself has all HW resource released and input released */
	if (vdec->parallel_dec == 1)
		vdec_core_finish_run(vdec, CORE_MASK_HEVC);
	else
		vdec_core_finish_run(hw_to_vdec(hw), CORE_MASK_VDEC_1
					| CORE_MASK_HEVC);
	trigger_schedule(hw);
}

static int av1_hw_ctx_restore(struct AV1HW_s *hw)
{
	vav1_prot_init(hw, HW_MASK_FRONT | HW_MASK_BACK);
	return 0;
}

static bool is_avaliable_buffer(struct AV1HW_s *hw)
{
	AV1_COMMON *cm = &hw->common;
	RefCntBuffer *const frame_bufs = cm->buffer_pool->frame_bufs;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int i, free_count = 0;
	int used_count = 0;

	if ((hw->used_buf_num == 0) ||
		(ctx->cap_pool.dec < hw->used_buf_num)) {
		if (ctx->fb_ops.query(&ctx->fb_ops, &hw->fb_token)) {
			free_count =
				v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx) + 1;
		}
	}

	for (i = 0; i < hw->used_buf_num; ++i) {
		if ((frame_bufs[i].ref_count == 0) &&
			(frame_bufs[i].buf.vf_ref == 0) &&
			(frame_bufs[i].buf.repeat_count == 0) &&
			(frame_bufs[i].buf.index >= 0) &&
			frame_bufs[i].buf.cma_alloc_addr) {
			free_count++;
		} else if (frame_bufs[i].buf.cma_alloc_addr)
			used_count++;
	}

	ATRACE_COUNTER("av1_free_buff_count", free_count);
	ATRACE_COUNTER("av1_used_buff_count", used_count);

	return free_count >= hw->run_ready_min_buf_num ? 1 : 0;
}

static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	struct AV1HW_s *hw = (struct AV1HW_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int tvp = vdec_secure(hw_to_vdec(hw)) ?
		CODEC_MM_FLAGS_TVP : 0;
	unsigned long ret = 0;

	if (!hw->pic_list_init_done2 || hw->eos)
		return ret;

	if (!hw->first_sc_checked &&
		(get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) &&
		(get_double_write_mode(hw) != 0x10)) {
		int size = decoder_mmu_box_sc_check(ctx->mmu_box, tvp);
		hw->first_sc_checked = 1;
		av1_print(hw, 0, "av1 cached=%d  need_size=%d speed= %d ms\n",
			size, (hw->need_cache_size >> PAGE_SHIFT),
			(int)(get_jiffies_64() - hw->sc_start_time) * 1000/HZ);
#ifdef AOM_AV1_MMU_DW
		/*!!!!!! To do ... */
		if (get_double_write_mode(hw) & 0x20) {

		}
#endif
	}

	if (hw->v4l_params_parsed) {
		if (is_avaliable_buffer(hw))
			ret = CORE_MASK_HEVC;
		else
			ret = 0;
	} else {
		if (ctx->v4l_resolution_change)
			ret = 0;
		else
			ret = CORE_MASK_HEVC;
	}

	if (ret)
		not_run_ready[hw->index] = 0;
	else
		not_run_ready[hw->index]++;

	return ret;
}

static void av1_frame_mode_pts_save(struct AV1HW_s *hw)
{
	u64 i, valid_pts_diff_cnt, pts_diff_sum;
	u64 in_pts_diff, last_valid_pts_diff, calc_dur;

	if (hw->chunk == NULL)
		return;

	av1_print(hw, AV1_DEBUG_OUT_PTS,
		"run_front: pts %d, pts64 %lld, ts: %lld\n",
		hw->chunk->pts, hw->chunk->pts64, hw->chunk->timestamp);

	for (i = (FRAME_BUFFERS - 1); i > 0; i--) {
		hw->frame_mode_pts_save[i] = hw->frame_mode_pts_save[i - 1];
		hw->frame_mode_pts64_save[i] = hw->frame_mode_pts64_save[i - 1];
	}
	hw->frame_mode_pts_save[0] = hw->chunk->pts;
	hw->frame_mode_pts64_save[0] = hw->chunk->pts64;

	if (!v4l_bitstream_id_enable)
		hw->frame_mode_pts64_save[0] = hw->chunk->timestamp;

	if (hw->first_pts_index < ARRAY_SIZE(hw->frame_mode_pts_save))
		hw->first_pts_index++;
	/* frame duration check, vdec_secure return for nts problem */
	if ((!hw->first_pts_index) ||
		hw->get_frame_dur ||
		vdec_secure(hw_to_vdec(hw)))
		return;
	valid_pts_diff_cnt = 0;
	pts_diff_sum = 0;

	for (i = 0; i < FRAME_BUFFERS - 1; i++) {
		if ((hw->frame_mode_pts_save[i] > hw->frame_mode_pts_save[i + 1]) &&
			(hw->frame_mode_pts_save[i + 1] != 0))
			in_pts_diff = hw->frame_mode_pts_save[i]
				- hw->frame_mode_pts_save[i + 1];
		else
			in_pts_diff = 0;

		if (in_pts_diff < 100 ||
			(valid_pts_diff_cnt && (!close_to(in_pts_diff, last_valid_pts_diff, 100))))
			in_pts_diff = 0;
		else {
			last_valid_pts_diff = in_pts_diff;
			valid_pts_diff_cnt++;
		}

		pts_diff_sum += in_pts_diff;
	}

	if (!valid_pts_diff_cnt) {
		av1_print(hw, AV1_DEBUG_OUT_PTS, "checked no avaliable pts\n");
		return;
	}

	calc_dur = PTS2DUR_u64(div_u64(pts_diff_sum, valid_pts_diff_cnt));

	if ((!close_to(calc_dur, hw->frame_dur, 10)) &&
		(calc_dur < 4800) && (calc_dur > 800)) {
		av1_print(hw, 0, "change to calc dur %llu, old dur %u\n", calc_dur, hw->frame_dur);
		hw->frame_dur = calc_dur;
		hw->get_frame_dur = true;
	} else {
		if (hw->frame_count > FRAME_BUFFERS)
			hw->get_frame_dur = true;
	}
}

static void run_front(struct vdec_s *vdec)
{
	struct AV1HW_s *hw =
		(struct AV1HW_s *)vdec->private;
	int ret, size;
	struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;

	run_count[hw->index]++;
	hevc_reset_core(vdec);

	if ((vdec_frame_based(vdec)) &&
		(hw->dec_result == DEC_RESULT_UNFINISH)) {
		u32 res_byte = hw->data_size - hw->consume_byte;

		av1_print(hw, AV1_DEBUG_BUFMGR,
			"%s before, consume 0x%x, size 0x%x, offset 0x%x, res 0x%x\n", __func__,
			hw->consume_byte, hw->data_size, hw->data_offset + hw->consume_byte, res_byte);

		hw->data_invalid = vdec_offset_prepare_input(vdec, hw->consume_byte, hw->data_offset, hw->data_size);
		hw->data_offset -= (hw->data_invalid - hw->consume_byte);
		hw->data_size += (hw->data_invalid - hw->consume_byte);
		size = hw->data_size;
		WRITE_VREG(HEVC_ASSIST_SCRATCH_C, hw->data_invalid);

		av1_print(hw, AV1_DEBUG_BUFMGR,
			"%s after, consume 0x%x, size 0x%x, offset 0x%x, invalid 0x%x, res 0x%x\n", __func__,
			hw->consume_byte, hw->data_size, hw->data_offset, hw->data_invalid, res_byte);
	} else {
		size = vdec_prepare_input(vdec, &hw->chunk);
		if (size < 0) {
			input_empty[hw->index]++;

			hw->dec_result = DEC_RESULT_AGAIN;

			av1_print(hw, PRINT_FLAG_VDEC_DETAIL,
				"ammvdec_av1: Insufficient data\n");

			vdec_schedule_work(&hw->work);
			return;
		}
		if ((vdec_frame_based(vdec)) &&
			(hw->chunk != NULL)) {
			hw->data_offset = hw->chunk->offset;
			hw->data_size = size;
		}
		WRITE_VREG(HEVC_ASSIST_SCRATCH_C, 0);
	}

	ATRACE_COUNTER("V_ST_DEC-chunk_size", size);

	input_empty[hw->index] = 0;
	hw->dec_result = DEC_RESULT_NONE;
	hw->start_shift_bytes = READ_VREG(HEVC_SHIFT_BYTE_COUNT);

	av1_frame_mode_pts_save(hw);
	if (debug & PRINT_FLAG_VDEC_STATUS) {
		if (vdec_frame_based(vdec) && hw->chunk && !vdec_secure(vdec)) {
			u8 *data = NULL;

			if (!hw->chunk->block->is_mapped)
				data = codec_mm_vmap(hw->chunk->block->start +
					hw->data_offset, size);
			else
				data = ((u8 *)hw->chunk->block->start_virt) +
					hw->data_offset;

			//print_hex_debug(data, size, size > 64 ? 64 : size);
			av1_print(hw, 0,
				"%s: size 0x%x sum 0x%x %02x %02x %02x %02x %02x %02x .. %02x %02x %02x %02x\n",
				__func__, size, get_data_check_sum(hw, size),
				data[0], data[1], data[2], data[3],
				data[4], data[5], data[size - 4],
				data[size - 3], data[size - 2],
				data[size - 1]);
			av1_print(hw, 0,
				"%s frm cnt (%d): chunk (0x%x 0x%x) (%x %x %x %x %x) bytes 0x%x\n",
				__func__, hw->frame_count, hw->data_size, hw->data_offset,
				READ_VREG(HEVC_STREAM_START_ADDR),
				READ_VREG(HEVC_STREAM_END_ADDR),
				READ_VREG(HEVC_STREAM_LEVEL),
				READ_VREG(HEVC_STREAM_WR_PTR),
				READ_VREG(HEVC_STREAM_RD_PTR),
				hw->start_shift_bytes);

			if (!hw->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		} else {
			av1_print(hw, 0,
				"%s (%d): size 0x%x (0x%x 0x%x) (%x %x %x %x %x) bytes 0x%x\n",
				__func__,
				hw->frame_count, size,
				hw->chunk ? hw->data_size : 0,
				hw->chunk ? hw->data_offset : 0,
				READ_VREG(HEVC_STREAM_START_ADDR),
				READ_VREG(HEVC_STREAM_END_ADDR),
				READ_VREG(HEVC_STREAM_LEVEL),
				READ_VREG(HEVC_STREAM_WR_PTR),
				READ_VREG(HEVC_STREAM_RD_PTR),
				hw->start_shift_bytes);
		}
	}
	ATRACE_COUNTER(hw->trace.decode_run_time_name, TRACE_RUN_LOADING_FW_START);
	if (vdec->mc_loaded) {
	/*firmware have load before,
	  and not changes to another.
	  ignore reload.
	*/
	} else {
#ifdef DEBUG_USE_VP9_DEVICE_NAME
		ret = amhevc_loadmc_ex(VFORMAT_VP9, NULL, hw->fw->data);
#else
		ret = amhevc_loadmc_ex(VFORMAT_AV1, NULL, hw->fw->data);
#endif
		if (ret < 0) {
			amhevc_disable();
			av1_print(hw, PRINT_FLAG_ERROR,
				"AV1: the %s fw loading failed, err: %x\n",
				tee_enabled() ? "TEE" : "local", ret);
			vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_FW_LOAD_ERROR);
			hw->dec_result = DEC_RESULT_FORCE_EXIT;
			vdec_schedule_work(&hw->work);
			return;
		}
		vdec->mc_loaded = 1;
#ifdef DEBUG_USE_VP9_DEVICE_NAME
		vdec->mc_type = VFORMAT_VP9;
#else
		vdec->mc_type = VFORMAT_AV1;
#endif
	}
	ATRACE_COUNTER(hw->trace.decode_run_time_name, TRACE_RUN_LOADING_FW_END);

	ATRACE_COUNTER(hw->trace.decode_run_time_name, TRACE_RUN_LOADING_RESTORE_START);
	if (av1_hw_ctx_restore(hw) < 0) {
		vdec_schedule_work(&hw->work);
		return;
	}
	ATRACE_COUNTER(hw->trace.decode_run_time_name, TRACE_RUN_LOADING_RESTORE_END);
	vdec_enable_input(vdec);

	WRITE_VREG(HEVC_DEC_STATUS_REG, HEVC_ACTION_DONE);

	if (vdec_frame_based(vdec)) {
		if (debug & PRINT_FLAG_VDEC_DATA)
			dump_data(hw, hw->data_size);

		WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, 0);
		size = hw->data_size +
			(hw->data_offset & (VDEC_FIFO_ALIGN - 1));
		if (vdec->mvfrm)
			vdec->mvfrm->frame_size = hw->data_size;
	}
	hw->data_size = size;
	WRITE_VREG(HEVC_DECODE_SIZE, size);
	WRITE_VREG(HEVC_DECODE_COUNT, hw->result_done_count);
	WRITE_VREG(LMEM_DUMP_ADR, (u32)hw->lmem_phy_addr);
	if (hw->config_next_ref_info_flag)
	    config_next_ref_info_hw(hw);
	hw->config_next_ref_info_flag = 0;
	hw->init_flag = 1;

	av1_print(hw, PRINT_FLAG_VDEC_DETAIL,
		"%s: start hw (%x %x %x) HEVC_DECODE_SIZE 0x%x\n",
		__func__,
		READ_VREG(HEVC_DEC_STATUS_REG),
		READ_VREG(HEVC_MPC_E),
		READ_VREG(HEVC_MPSR),
		READ_VREG(HEVC_DECODE_SIZE));

	start_process_time(hw);
	mod_timer(&hw->timer, jiffies);
	hw->stat |= STAT_TIMER_ARM;
	hw->stat |= STAT_ISR_REG;
	amhevc_start();
	hw->stat |= STAT_VDEC_RUN;
}

static void run(struct vdec_s *vdec, unsigned long mask,
	void (*callback)(struct vdec_s *, void *), void *arg)
{
	struct AV1HW_s *hw =
		(struct AV1HW_s *)vdec->private;
	ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_RUN_START);
	av1_print(hw,
		PRINT_FLAG_VDEC_DETAIL, "%s mask %lx\r\n",
		__func__, mask);

	run_count[hw->index]++;
	if (vdec->mvfrm)
		vdec->mvfrm->hw_decode_start = local_clock();
	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;
	hw->one_package_frame_cnt = 0;
	run_front(vdec);
	ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_RUN_END);
}

static void  av1_decode_ctx_reset(struct AV1HW_s *hw)
{
	struct AV1_Common_s *const cm = &hw->common;
	struct RefCntBuffer_s *const frame_bufs = cm->buffer_pool->frame_bufs;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)hw->v4l2_ctx;
	int i;

	for (i = 0; i < FRAME_BUFFERS; ++i) {
		frame_bufs[i].ref_count		= 0;
		frame_bufs[i].buf.vf_ref	= 0;
		frame_bufs[i].buf.decode_idx	= 0;
		frame_bufs[i].buf.cma_alloc_addr = 0;
		frame_bufs[i].buf.index		= i;
		frame_bufs[i].buf.BUF_index	= -1;
		frame_bufs[i].buf.mv_buf_index	= -1;
		frame_bufs[i].buf.repeat_pic = NULL;
		frame_bufs[i].buf.repeat_count = 0;
	}

	for (i = 0; i < MV_BUFFER_NUM; i++) {
		if (hw->m_mv_BUF[i].start_adr) {
			if (mv_buf_dynamic_alloc) {
				decoder_bmmu_box_free_idx(hw->bmmu_box, MV_BUFFER_IDX(i));
				hw->m_mv_BUF[i].start_adr = 0;
				hw->m_mv_BUF[i].size = 0;
			}
			hw->m_mv_BUF[i].used_flag = 0;
		}
	}

	if (ctx->comp_bufs) {
		for (i = 0; i < V4L_CAP_BUFF_MAX; i++) {
			struct internal_comp_buf *buf;
			buf = &ctx->comp_bufs[i];
			buf->used = 0;
		}
	}

	hw->one_compressed_data_done = 0;
	hw->config_next_ref_info_flag = 0;
	hw->init_flag		= 0;
	hw->first_sc_checked	= 0;
	hw->fatal_error		= 0;
	hw->show_frame_num	= 0;
	hw->postproc_done	= 0;
	hw->process_busy	= 0;
	hw->process_state	= 0;
	hw->frame_decoded	= 0;
	hw->eos			= 0;
	hw->dec_result = DEC_RESULT_NONE;
}

static void reset(struct vdec_s *vdec)
{
	struct AV1HW_s *hw = (struct AV1HW_s *)vdec->private;

	cancel_work_sync(&hw->work);
	cancel_work_sync(&hw->set_clk_work);

	if (hw->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}

	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}

	reset_process_time(hw);

	av1_bufmgr_ctx_reset(hw->pbi, &hw->av1_buffer_pool, &hw->common);
	hw->pbi->private_data = hw;
	mutex_lock(&hw->assit_task.assit_mutex);
	av1_local_uninit(hw, true);
	if (vav1_local_init(hw, true) < 0)
		av1_print(hw, 0, "%s local_init failed \r\n", __func__);
	mutex_unlock(&hw->assit_task.assit_mutex);

	av1_decode_ctx_reset(hw);

	atomic_set(&hw->vf_pre_count, 0);
	atomic_set(&hw->vf_get_count, 0);
	atomic_set(&hw->vf_put_count, 0);

	if (hw->ge2d) {
		vdec_ge2d_destroy(hw->ge2d);
		hw->ge2d = NULL;
	}

	av1_print(hw, PRINT_FLAG_VDEC_DETAIL, "%s\r\n", __func__);
}

static irqreturn_t av1_irq_cb(struct vdec_s *vdec, int irq)
{
	struct AV1HW_s *hw =
		(struct AV1HW_s *)vdec->private;
	return vav1_isr(0, hw);
}

static irqreturn_t av1_threaded_irq_cb(struct vdec_s *vdec, int irq)
{
	struct AV1HW_s *hw =
		(struct AV1HW_s *)vdec->private;
	return vav1_isr_thread_fn(0, hw);
}

static void av1_dump_state(struct vdec_s *vdec)
{
	struct AV1HW_s *hw =
		(struct AV1HW_s *)vdec->private;
	struct AV1_Common_s *const cm = &hw->common;
	int i;
	av1_print(hw, 0, "====== %s\n", __func__);

	av1_print(hw, 0,
		"width/height (%d/%d), used_buf_num %d, video_signal_type 0x%x\n",
		cm->width,
		cm->height,
		hw->used_buf_num,
		hw->video_signal_type
		);

	av1_print(hw, 0,
		"is_framebase(%d), eos %d, dec_result 0x%x dec_frm %d disp_frm %d run %d not_run_ready %d input_empty %d low_latency %d no_head %d \n",
		input_frame_based(vdec),
		hw->eos,
		hw->dec_result,
		decode_frame_count[hw->index],
		display_frame_count[hw->index],
		run_count[hw->index],
		not_run_ready[hw->index],
		input_empty[hw->index],
		hw->low_latency_flag,
		hw->no_head
		);

	av1_print(hw, 0,
	"%s, newq(%d/%d), dispq(%d/%d), vf prepare/get/put (%d/%d/%d), free_buf_count %d (min %d for run_ready)\n",
	__func__,
	kfifo_len(&hw->newframe_q),
	VF_POOL_SIZE,
	kfifo_len(&hw->display_q),
	VF_POOL_SIZE,
	hw->vf_pre_count,
	hw->vf_get_count,
	hw->vf_put_count,
	get_free_buf_count(hw),
	hw->run_ready_min_buf_num
	);

	dump_pic_list(hw);

	for (i = 0; i < MAX_BUF_NUM; i++) {
		av1_print(hw, 0,
			"mv_Buf(%d) start_adr 0x%x size 0x%x used %d\n",
			i,
			hw->m_mv_BUF[i].start_adr,
			hw->m_mv_BUF[i].size,
			hw->m_mv_BUF[i].used_flag);
	}

	av1_print(hw, 0,
		"HEVC_DEC_STATUS_REG=0x%x\n",
		READ_VREG(HEVC_DEC_STATUS_REG));
	av1_print(hw, 0,
		"HEVC_MPC_E=0x%x\n",
		READ_VREG(HEVC_MPC_E));
	av1_print(hw, 0,
		"DECODE_MODE=0x%x\n",
		READ_VREG(DECODE_MODE));
	av1_print(hw, 0,
		"NAL_SEARCH_CTL=0x%x\n",
		READ_VREG(NAL_SEARCH_CTL));
	av1_print(hw, 0,
		"HEVC_PARSER_LCU_START=0x%x\n",
		READ_VREG(HEVC_PARSER_LCU_START));
	av1_print(hw, 0,
		"HEVC_DECODE_SIZE=0x%x\n",
		READ_VREG(HEVC_DECODE_SIZE));
	av1_print(hw, 0,
		"HEVC_SHIFT_BYTE_COUNT=0x%x\n",
		READ_VREG(HEVC_SHIFT_BYTE_COUNT));
	av1_print(hw, 0,
		"HEVC_SHIFTED_DATA=0x%x\n",
		READ_VREG(HEVC_SHIFTED_DATA));
	av1_print(hw, 0,
		"HEVC_STREAM_START_ADDR=0x%x\n",
		READ_VREG(HEVC_STREAM_START_ADDR));
	av1_print(hw, 0,
		"HEVC_STREAM_END_ADDR=0x%x\n",
		READ_VREG(HEVC_STREAM_END_ADDR));
	av1_print(hw, 0,
		"HEVC_STREAM_LEVEL=0x%x\n",
		READ_VREG(HEVC_STREAM_LEVEL));
	av1_print(hw, 0,
		"HEVC_STREAM_WR_PTR=0x%x\n",
		READ_VREG(HEVC_STREAM_WR_PTR));
	av1_print(hw, 0,
		"HEVC_STREAM_RD_PTR=0x%x\n",
		READ_VREG(HEVC_STREAM_RD_PTR));
	av1_print(hw, 0,
		"PARSER_VIDEO_RP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_rp));
	av1_print(hw, 0,
		"PARSER_VIDEO_WP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_wp));

	if (input_frame_based(vdec) && (debug & PRINT_FLAG_VDEC_DATA)) {
		int jj;
		if (hw->chunk && hw->chunk->block &&
			hw->data_size > 0) {
			u8 *data = NULL;

			if (!hw->chunk->block->is_mapped)
				data = codec_mm_vmap(
					hw->chunk->block->start +
					hw->data_offset,
					hw->data_size);
			else
				data = ((u8 *)hw->chunk->block->start_virt)
					+ hw->data_offset;
			av1_print(hw, 0,
				"frame data size 0x%x\n",
				hw->data_size);
			for (jj = 0; jj < hw->data_size; jj++) {
				if ((jj & 0xf) == 0)
					av1_print(hw, 0,
						"%06x:", jj);
				av1_print_cont(hw, 0,
					"%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					av1_print_cont(hw, 0,
						"\n");
			}

			if (!hw->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		}
	}

}

static int ammvdec_av1_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	int ret;
	int config_val;
	int i;
	struct vframe_content_light_level_s content_light_level;
	struct vframe_master_display_colour_s vf_dp;
	u32 work_buf_size;
	struct BuffInfo_s *p_buf_info;
	struct AV1HW_s *hw = NULL;
	struct aml_vcodec_ctx *ctx = NULL;

	if ((get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_TM2) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5) ||
		((get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_TM2) && !is_meson_rev_b())) {
		pr_err("av1 unsupported on cpu %d, is_tm2_revb %d\n",
			get_cpu_major_id(), is_cpu_tm2_revb());
		return -EINVAL;
	}

	if (pdata == NULL) {
		av1_print(hw, 0, "\nammvdec_av1 memory resource undefined.\n");
		return -EFAULT;
	}
	memset(&vf_dp, 0, sizeof(struct vframe_master_display_colour_s));

	hw = vzalloc(sizeof(struct AV1HW_s));
	if (hw == NULL) {
		av1_print(hw, 0, "\nammvdec_av1 device data allocation failed\n");
		return -ENOMEM;
	}

	if (init_dblk_struc(hw) < 0) {
		av1_print(hw, 0, "\nammvdec_av1 device data allocation failed\n");
		vfree(hw);
		return -ENOMEM;
	}

	hw->pbi = av1_decoder_create(&hw->av1_buffer_pool, &hw->common); //&aom_decoder;
	if (hw->pbi == NULL) {
		av1_print(hw, 0, "\nammvdec_av1 device data allocation failed\n");
		release_dblk_struct(hw);
		vfree(hw);
		return -ENOMEM;
	}

	hw->pbi->private_data = hw;
	/* the ctx from v4l2 driver. */
	hw->v4l2_ctx = pdata->private;
	ctx = hw->v4l2_ctx;

	pdata->private = hw;
	pdata->dec_status = vav1_dec_status;
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = av1_irq_cb;
	pdata->threaded_irq_handler = av1_threaded_irq_cb;
	pdata->dump_state = av1_dump_state;

	hw->index = pdev->id;
	if (is_rdma_enable()) {
		hw->rdma_adr = decoder_dma_alloc_coherent(&hw->rdma_handle,
				RDMA_SIZE, &hw->rdma_phy_adr, "AV1_RDMA");
		for (i = 0; i < SCALELUT_DATA_WRITE_NUM; i++) {
			hw->rdma_adr[i * 4] = HEVC_IQIT_SCALELUT_WR_ADDR & 0xfff;
			hw->rdma_adr[i * 4 + 1] = i;
			hw->rdma_adr[i * 4 + 2] = HEVC_IQIT_SCALELUT_DATA & 0xfff;
			hw->rdma_adr[i * 4 + 3] = 0;
			if (i == SCALELUT_DATA_WRITE_NUM - 1) {
				hw->rdma_adr[i * 4 + 2] = (HEVC_IQIT_SCALELUT_DATA & 0xfff) | 0x20000;
			}
		}
	}
	snprintf(hw->trace.vdec_name, sizeof(hw->trace.vdec_name),
		"av1-%d", hw->index);
	snprintf(hw->trace.pts_name, sizeof(hw->trace.pts_name),
		"%s-timestamp", hw->trace.vdec_name);
	snprintf(hw->trace.new_q_name, sizeof(hw->trace.new_q_name),
		"%s-newframe_q", hw->trace.vdec_name);
	snprintf(hw->trace.disp_q_name, sizeof(hw->trace.disp_q_name),
		"%s-dispframe_q", hw->trace.vdec_name);
	snprintf(hw->trace.decode_time_name, sizeof(hw->trace.decode_time_name),
		"decoder_time%d", pdev->id);
	snprintf(hw->trace.decode_run_time_name, sizeof(hw->trace.decode_run_time_name),
		"decoder_run_time%d", pdev->id);
	snprintf(hw->trace.decode_header_memory_time_name, sizeof(hw->trace.decode_header_memory_time_name),
		"decoder_header_time%d", pdev->id);
	snprintf(hw->trace.decode_work_time_name, sizeof(hw->trace.decode_work_time_name),
		"decoder_work_time%d", pdev->id);
	if (pdata->use_vfm_path)
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	else if (vdec_dual(pdata)) {
		struct AV1HW_s *hevc_pair = NULL;

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
		if (pdata->master)
			hevc_pair = (struct AV1HW_s *)pdata->master->private;
		else if (pdata->slave)
			hevc_pair = (struct AV1HW_s *)pdata->slave->private;

		if (hevc_pair)
			hw->shift_byte_count_lo = hevc_pair->shift_byte_count_lo;
	}
#endif
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			MULTI_INSTANCE_PROVIDER_NAME ".%02x", pdev->id & 0xff);

	hw->provider_name = pdata->vf_provider_name;
	platform_set_drvdata(pdev, pdata);

	hw->platform_dev = pdev;
	hw->video_signal_type = 0;
	hw->m_ins_flag = 1;

	film_grain_task_create(hw);

	if (pdata->sys_info) {
		hw->vav1_amstream_dec_info = *pdata->sys_info;
		if ((unsigned long) hw->vav1_amstream_dec_info.param
				& 0x08) {
				hw->low_latency_flag = 1;
			} else
				hw->low_latency_flag = 0;
	} else {
		hw->vav1_amstream_dec_info.width = 0;
		hw->vav1_amstream_dec_info.height = 0;
		hw->vav1_amstream_dec_info.rate = 30;
	}

	if ((debug & IGNORE_PARAM_FROM_CONFIG) == 0 &&
			pdata->config_len) {
#ifdef MULTI_INSTANCE_SUPPORT
		int av1_buf_width = 0;
		int av1_buf_height = 0;
		/*use ptr config for doubel_write_mode, etc*/
		av1_print(hw, 0, "pdata->config=%s\n", pdata->config);
		if (get_config_int(pdata->config, "av1_double_write_mode",
				&config_val) == 0)
			hw->double_write_mode = config_val;
		else
			hw->double_write_mode = double_write_mode;

		if (get_config_int(pdata->config, "save_buffer_mode",
				&config_val) == 0)
			hw->save_buffer_mode = config_val;
		else
			hw->save_buffer_mode = 0;
		if (get_config_int(pdata->config, "av1_buf_width",
				&config_val) == 0) {
			av1_buf_width = config_val;
		}
		if (get_config_int(pdata->config, "av1_buf_height",
				&config_val) == 0) {
			av1_buf_height = config_val;
		}

		if (get_config_int(pdata->config, "no_head",
				&config_val) == 0)
			hw->no_head = config_val;
		else
			hw->no_head = no_head;

		/*use ptr config for max_pic_w, etc*/
		if (get_config_int(pdata->config, "av1_max_pic_w",
				&config_val) == 0) {
				hw->max_pic_w = config_val;
		}
		if (get_config_int(pdata->config, "av1_max_pic_h",
				&config_val) == 0) {
				hw->max_pic_h = config_val;
		}
		if ((hw->max_pic_w * hw->max_pic_h)
			< (av1_buf_width * av1_buf_height)) {
			hw->max_pic_w = av1_buf_width;
			hw->max_pic_h = av1_buf_height;
			av1_print(hw, 0, "use buf resolution\n");
		}

		if (get_config_int(pdata->config, "sidebind_type",
				&config_val) == 0)
			hw->sidebind_type = config_val;

		if (get_config_int(pdata->config, "sidebind_channel_id",
				&config_val) == 0)
			hw->sidebind_channel_id = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_codec_enable",
			&config_val) == 0)
			hw->is_used_v4l = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_buffer_margin",
			&config_val) == 0)
			hw->dynamic_buf_num_margin = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_canvas_mem_mode",
			&config_val) == 0)
			hw->mem_map_mode = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_low_latency_mode",
			&config_val) == 0)
			hw->low_latency_flag = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_metadata_config_flag",
			&config_val) == 0) {
			hw->high_bandwidth_flag = config_val & VDEC_CFG_FLAG_HIGH_BANDWIDTH;
			if (hw->high_bandwidth_flag)
				av1_print(hw, 0, "high bandwidth\n");
		}
#endif
		if (get_config_int(pdata->config, "HDRStaticInfo",
				&vf_dp.present_flag) == 0
				&& vf_dp.present_flag == 1) {
			get_config_int(pdata->config, "signal_type",
					&hw->video_signal_type);
			get_config_int(pdata->config, "mG.x",
					&vf_dp.primaries[0][0]);
			get_config_int(pdata->config, "mG.y",
					&vf_dp.primaries[0][1]);
			get_config_int(pdata->config, "mB.x",
					&vf_dp.primaries[1][0]);
			get_config_int(pdata->config, "mB.y",
					&vf_dp.primaries[1][1]);
			get_config_int(pdata->config, "mR.x",
					&vf_dp.primaries[2][0]);
			get_config_int(pdata->config, "mR.y",
					&vf_dp.primaries[2][1]);
			get_config_int(pdata->config, "mW.x",
					&vf_dp.white_point[0]);
			get_config_int(pdata->config, "mW.y",
					&vf_dp.white_point[1]);
			get_config_int(pdata->config, "mMaxDL",
					&vf_dp.luminance[0]);
			get_config_int(pdata->config, "mMinDL",
					&vf_dp.luminance[1]);
			vf_dp.content_light_level.present_flag = 1;
			get_config_int(pdata->config, "mMaxCLL",
					&content_light_level.max_content);
			get_config_int(pdata->config, "mMaxFALL",
					&content_light_level.max_pic_average);
			vf_dp.content_light_level = content_light_level;
			if (!hw->video_signal_type) {
				hw->video_signal_type = (1 << 29)
					| (5 << 26)	/* unspecified */
					| (0 << 25)	/* limit */
					| (1 << 24)	/* color available */
					| (9 << 16)	/* 2020 */
					| (16 << 8)	/* 2084 */
					| (9 << 0);	/* 2020 */
                        }
		}
		hw->vf_dp = vf_dp;
	} else {
		u32 force_w, force_h;
		if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D) {
			force_w = 1920;
			force_h = 1088;
		} else {
			force_w = 8192;
			force_h = 4608;
		}
		if (hw->vav1_amstream_dec_info.width)
			hw->max_pic_w = hw->vav1_amstream_dec_info.width;
		else
			hw->max_pic_w = force_w;

		if (hw->vav1_amstream_dec_info.height)
			hw->max_pic_h = hw->vav1_amstream_dec_info.height;
		else
			hw->max_pic_h = force_h;
		hw->double_write_mode = double_write_mode;
	}

	hw->endian = HEVC_CONFIG_LITTLE_ENDIAN;
	if (endian)
		hw->endian = endian;

	if (is_oversize(hw->max_pic_w, hw->max_pic_h)) {
		pr_err("over size: %dx%d, probe failed\n",
			hw->max_pic_w, hw->max_pic_h);
		vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
		return -1;
	}
	if (force_bufspec) {
		hw->buffer_spec_index = force_bufspec & 0xf;
		pr_info("force buffer spec %d\n", force_bufspec & 0xf);
	} else if (vdec_is_support_4k()) {
		hw->buffer_spec_index = 1;
	} else
		hw->buffer_spec_index = 0;

	if (hw->buffer_spec_index == 0)
		hw->max_one_mv_buffer_size =
			(get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_SC2) ?
				MAX_ONE_MV_BUFFER_SIZE_1080P : MAX_ONE_MV_BUFFER_SIZE_1080P_TM2REVB;
	else if (hw->buffer_spec_index == 1)
		hw->max_one_mv_buffer_size =
		(get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_SC2) ?
			MAX_ONE_MV_BUFFER_SIZE_4K : MAX_ONE_MV_BUFFER_SIZE_4K_TM2REVB;
	else
		hw->max_one_mv_buffer_size =
		(get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_SC2) ?
			MAX_ONE_MV_BUFFER_SIZE_8K : MAX_ONE_MV_BUFFER_SIZE_8K_TM2REVB;

	p_buf_info = &aom_workbuff_spec[hw->buffer_spec_index];
	work_buf_size = (p_buf_info->end_adr - p_buf_info->start_adr
		+ 0xffff) & (~0xffff);

	if (vdec_is_support_4k() &&
		(hw->max_pic_w * hw->max_pic_h < MAX_SIZE_4K)) {
		hw->max_pic_w = 4096;
		hw->max_pic_h = 2304;
	}
	av1_print(hw, 0,
		"vdec_is_support_4k() %d  max_pic_w %d max_pic_h %d buffer_spec_index %d work_buf_size 0x%x\n",
			vdec_is_support_4k(), hw->max_pic_w, hw->max_pic_h,
			hw->buffer_spec_index, work_buf_size);

	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_GXL ||
		hw->double_write_mode == 0x10)
		hw->mmu_enable = 0;
	else
		hw->mmu_enable = 1;

	video_signal_type = hw->video_signal_type;

	if (pdata->sys_info) {
		hw->vav1_amstream_dec_info = *pdata->sys_info;
		if ((unsigned long) hw->vav1_amstream_dec_info.param
				& 0x08) {
				hw->low_latency_flag = 1;
			} else
				hw->low_latency_flag = 0;
	} else {
		hw->vav1_amstream_dec_info.width = 0;
		hw->vav1_amstream_dec_info.height = 0;
		hw->vav1_amstream_dec_info.rate = 30;
	}

#ifdef AOM_AV1_MMU_DW
	hw->dw_mmu_enable =
		get_double_write_mode_init(hw) & 0x20 ? 1 : 0;

#endif
	av1_print(hw, 0,
			"no_head %d  low_latency %d video_signal_type 0x%x\n",
			hw->no_head, hw->low_latency_flag, hw->video_signal_type);

	if (amvdec_av1_mmu_init(hw) < 0) {
		pr_err("av1 alloc bmmu box failed!!\n");
		/* devm_kfree(&pdev->dev, (void *)hw); */
		vfree((void *)hw);
		pdata->dec_status = NULL;
		return -1;
	}

	hw->cma_alloc_count = PAGE_ALIGN(work_buf_size) / PAGE_SIZE;
	ret = decoder_bmmu_box_alloc_buf_phy(hw->bmmu_box, WORK_SPACE_BUF_ID,
			hw->cma_alloc_count * PAGE_SIZE, DRIVER_NAME,
			&hw->cma_alloc_addr);
	if (ret < 0) {
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_NO_MEM);
		uninit_mmu_buffers(hw);
		/* devm_kfree(&pdev->dev, (void *)hw); */
		vfree((void *)hw);
		pdata->dec_status = NULL;
		return ret;
	}
	hw->buf_start = hw->cma_alloc_addr;
	hw->buf_size = work_buf_size;

	hw->init_flag = 0;
	hw->first_sc_checked = 0;
	hw->fatal_error = 0;
	hw->show_frame_num = 0;
	hw->run_ready_min_buf_num = run_ready_min_buf_num;

	if (hw->v4l2_ctx != NULL) {
		struct aml_vcodec_ctx *ctx = hw->v4l2_ctx;

		ctx->aux_infos.alloc_buffer(ctx, SEI_TYPE | DV_TYPE);
	}

	hw->aux_data_size = AUX_BUF_ALIGN(prefix_aux_buf_size) +
		AUX_BUF_ALIGN(suffix_aux_buf_size);
	hw->dv_data_buf = vmalloc(hw->aux_data_size);
	hw->dv_data_size = 0;

	if (debug) {
		av1_print(hw, AOM_DEBUG_HW_MORE, "===AV1 decoder mem resource 0x%lx size 0x%x\n",
			hw->buf_start, hw->buf_size);
	}

	hw->cma_dev = pdata->cma_dev;
	if (vav1_init(pdata) < 0) {
		av1_print(hw, 0, "\namvdec_av1 init failed.\n");
		av1_local_uninit(hw, false);
		uninit_mmu_buffers(hw);
		vfree((void *)hw);
		pdata->dec_status = NULL;
		return -ENODEV;
	}

#ifdef AUX_DATA_CRC
	vdec_aux_data_check_init(pdata);
#endif

	vdec_set_prepare_level(pdata, start_decode_buf_level);
	hevc_source_changed(VFORMAT_AV1, 4096, 2048, 60);

	if (pdata->parallel_dec == 1)
		vdec_core_request(pdata, CORE_MASK_HEVC);
	else
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
			| CORE_MASK_COMBINE);

	hw->pic_list_init_done2 = true;
	return 0;
}

static int ammvdec_av1_remove(struct platform_device *pdev)
{
	struct AV1HW_s *hw = (struct AV1HW_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	struct vdec_s *vdec = hw_to_vdec(hw);
	int i;
	if (debug)
		av1_print(hw, AOM_DEBUG_HW_MORE, "amvdec_av1_remove\n");

#ifdef AUX_DATA_CRC
	vdec_aux_data_check_exit(vdec);
#endif

	if (hw->dv_data_buf != NULL) {
		vfree(hw->dv_data_buf);
		hw->dv_data_buf = NULL;
	}

	vmav1_stop(hw);

	film_grain_task_exit(hw);

	if (hw->ge2d) {
		vdec_ge2d_destroy(hw->ge2d);
		hw->ge2d = NULL;
	}

	if (vdec->parallel_dec == 1)
		vdec_core_release(hw_to_vdec(hw), CORE_MASK_HEVC);
	else
		vdec_core_release(hw_to_vdec(hw), CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_DISCONNECTED);

	if (vdec->parallel_dec == 1) {
		for (i = 0; i < FRAME_BUFFERS; i++) {
			vdec->free_canvas_ex
				(hw->common.buffer_pool->frame_bufs[i].buf.y_canvas_index,
				vdec->id);
			vdec->free_canvas_ex
				(hw->common.buffer_pool->frame_bufs[i].buf.uv_canvas_index,
				vdec->id);
		}
	}

#ifdef DEBUG_PTS
	pr_info("pts missed %ld, pts hit %ld, duration %d\n",
		   hw->pts_missed, hw->pts_hit, hw->frame_dur);
#endif
	if (is_rdma_enable())
		decoder_dma_free_coherent(hw->rdma_handle,
			RDMA_SIZE, hw->rdma_adr, hw->rdma_phy_adr);
	vfree(hw->pbi);
	release_dblk_struct(hw);
	vfree((void *)hw);
	return 0;
}

static struct platform_driver ammvdec_av1_driver = {
	.probe = ammvdec_av1_probe,
	.remove = ammvdec_av1_remove,
	.driver = {
		.name = MULTI_DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &av1_pm_ops,
#endif
	}
};
#endif
static struct mconfig av1_configs[] = {
	MC_PU32("bit_depth_luma", &bit_depth_luma),
	MC_PU32("bit_depth_chroma", &bit_depth_chroma),
	MC_PU32("frame_width", &frame_width),
	MC_PU32("frame_height", &frame_height),
	MC_PU32("debug", &debug),
	MC_PU32("radr", &radr),
	MC_PU32("rval", &rval),
	MC_PU32("pop_shorts", &pop_shorts),
	MC_PU32("dbg_cmd", &dbg_cmd),
	MC_PU32("dbg_skip_decode_index", &dbg_skip_decode_index),
	MC_PU32("endian", &endian),
	MC_PU32("step", &step),
	MC_PU32("udebug_flag", &udebug_flag),
	MC_PU32("decode_pic_begin", &decode_pic_begin),
	MC_PU32("slice_parse_begin", &slice_parse_begin),
	MC_PU32("i_only_flag", &i_only_flag),
	MC_PU32("error_handle_policy", &error_handle_policy),
	MC_PU32("buf_alloc_width", &buf_alloc_width),
	MC_PU32("buf_alloc_height", &buf_alloc_height),
	MC_PU32("buf_alloc_depth", &buf_alloc_depth),
	MC_PU32("buf_alloc_size", &buf_alloc_size),
	MC_PU32("buffer_mode", &buffer_mode),
	MC_PU32("buffer_mode_dbg", &buffer_mode_dbg),
	MC_PU32("max_buf_num", &max_buf_num),
	MC_PU32("dynamic_buf_num_margin", &dynamic_buf_num_margin),
	MC_PU32("mem_map_mode", &mem_map_mode),
	MC_PU32("double_write_mode", &double_write_mode),
	MC_PU32("enable_mem_saving", &enable_mem_saving),
	MC_PU32("force_w_h", &force_w_h),
	MC_PU32("force_fps", &force_fps),
	MC_PU32("max_decoding_time", &max_decoding_time),
	MC_PU32("on_no_keyframe_skiped", &on_no_keyframe_skiped),
	MC_PU32("start_decode_buf_level", &start_decode_buf_level),
	MC_PU32("decode_timeout_val", &decode_timeout_val),
	MC_PU32("av1_max_pic_w", &av1_max_pic_w),
	MC_PU32("av1_max_pic_h", &av1_max_pic_h),
};
static struct mconfig_node av1_node;

static int __init amvdec_av1_driver_init_module(void)
{
	int i;
#ifdef BUFMGR_ONLY_OLD_CHIP
	debug |= AOM_DEBUG_BUFMGR_ONLY;
#endif

	for (i = 0; i < WORK_BUF_SPEC_NUM; i++)
		init_buff_spec(NULL, &aom_workbuff_spec[i]);

	pr_debug("amvdec_av1 module init\n");

	error_handle_policy = 0;

#ifdef ERROR_HANDLE_DEBUG
	dbg_nal_skip_flag = 0;
	dbg_nal_skip_count = 0;
#endif
	udebug_flag = 0;
	decode_pic_begin = 0;
	slice_parse_begin = 0;
	step = 0;
	buf_alloc_size = 0;

	if (platform_driver_register(&ammvdec_av1_driver)) {
		pr_err("failed to register ammvdec_av1 driver\n");
		return -ENODEV;
	}
	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D) {
		amvdec_av1_profile.profile =
				"10bit, dwrite, compressed, no_head, uvm, multi_frame_dv";
	} else if (((get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_TM2) || is_cpu_tm2_revb())
		&& (get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_T5)) {
		amvdec_av1_profile.profile =
				"8k, 10bit, dwrite, compressed, no_head, frame_dv, uvm, multi_frame_dv";
	} else {
		amvdec_av1_profile.name = "av1_unsupport";
	}

	vcodec_profile_register(&amvdec_av1_profile);

	INIT_REG_NODE_CONFIGS("media.decoder", &av1_node,
		"av1-v4l", av1_configs, CONFIG_FOR_RW);
	vcodec_feature_register(VFORMAT_AV1, 1);

	return 0;
}

static void __exit amvdec_av1_driver_remove_module(void)
{
	pr_debug("amvdec_av1 module remove.\n");

	platform_driver_unregister(&ammvdec_av1_driver);
}

/****************************************/
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
module_param(force_dv_enable, uint, 0664);
MODULE_PARM_DESC(force_dv_enable, "\n amvdec_av1 force_dv_enable\n");
#endif

module_param(bit_depth_luma, uint, 0664);
MODULE_PARM_DESC(bit_depth_luma, "\n amvdec_av1 bit_depth_luma\n");

module_param(bit_depth_chroma, uint, 0664);
MODULE_PARM_DESC(bit_depth_chroma, "\n amvdec_av1 bit_depth_chroma\n");

module_param(frame_width, uint, 0664);
MODULE_PARM_DESC(frame_width, "\n amvdec_av1 frame_width\n");

module_param(frame_height, uint, 0664);
MODULE_PARM_DESC(frame_height, "\n amvdec_av1 frame_height\n");

module_param(multi_frames_in_one_pack, uint, 0664);
MODULE_PARM_DESC(multi_frames_in_one_pack, "\n multi_frames_in_one_pack\n");

module_param(debug, uint, 0664);
MODULE_PARM_DESC(debug, "\n amvdec_av1 debug\n");

module_param(disable_fg, uint, 0664);
MODULE_PARM_DESC(disable_fg, "\n amvdec_av1 disable_fg\n");

module_param(use_dw_mmu, uint, 0664);
MODULE_PARM_DESC(use_dw_mmu, "\n amvdec_av1 use_dw_mmu\n");

module_param(mmu_mem_save, uint, 0664);
MODULE_PARM_DESC(mmu_mem_save, "\n amvdec_av1 mmu_mem_save\n");

module_param(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\n radr\n");

module_param(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\n rval\n");

module_param(pop_shorts, uint, 0664);
MODULE_PARM_DESC(pop_shorts, "\n rval\n");

module_param(dbg_cmd, uint, 0664);
MODULE_PARM_DESC(dbg_cmd, "\n dbg_cmd\n");

module_param(dbg_skip_decode_index, uint, 0664);
MODULE_PARM_DESC(dbg_skip_decode_index, "\n dbg_skip_decode_index\n");

module_param(endian, uint, 0664);
MODULE_PARM_DESC(endian, "\n rval\n");

module_param(step, uint, 0664);
MODULE_PARM_DESC(step, "\n amvdec_av1 step\n");

module_param(decode_pic_begin, uint, 0664);
MODULE_PARM_DESC(decode_pic_begin, "\n amvdec_av1 decode_pic_begin\n");

module_param(slice_parse_begin, uint, 0664);
MODULE_PARM_DESC(slice_parse_begin, "\n amvdec_av1 slice_parse_begin\n");

module_param(i_only_flag, uint, 0664);
MODULE_PARM_DESC(i_only_flag, "\n amvdec_av1 i_only_flag\n");

module_param(low_latency_flag, uint, 0664);
MODULE_PARM_DESC(low_latency_flag, "\n amvdec_av1 low_latency_flag\n");

module_param(no_head, uint, 0664);
MODULE_PARM_DESC(no_head, "\n amvdec_av1 no_head\n");

module_param(error_handle_policy, uint, 0664);
MODULE_PARM_DESC(error_handle_policy, "\n amvdec_av1 error_handle_policy\n");

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

module_param(mv_buf_margin, uint, 0664);
MODULE_PARM_DESC(mv_buf_margin, "\n mv_buf_margin\n");

module_param(mv_buf_dynamic_alloc, uint, 0664);
MODULE_PARM_DESC(mv_buf_dynamic_alloc, "\n mv_buf_dynamic_alloc\n");

module_param(force_max_one_mv_buffer_size, uint, 0664);
MODULE_PARM_DESC(force_max_one_mv_buffer_size, "\n force_max_one_mv_buffer_size\n");

module_param(run_ready_min_buf_num, uint, 0664);
MODULE_PARM_DESC(run_ready_min_buf_num, "\n run_ready_min_buf_num\n");

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

#ifdef MCRCC_ENABLE
module_param(mcrcc_cache_alg_flag, uint, 0664);
MODULE_PARM_DESC(mcrcc_cache_alg_flag, "\n mcrcc_cache_alg_flag\n");
#endif

#ifdef MULTI_INSTANCE_SUPPORT
module_param(start_decode_buf_level, int, 0664);
MODULE_PARM_DESC(start_decode_buf_level,
		"\n av1 start_decode_buf_level\n");

module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val,
	"\n av1 decode_timeout_val\n");

module_param(av1_max_pic_w, uint, 0664);
MODULE_PARM_DESC(av1_max_pic_w, "\n av1_max_pic_w\n");

module_param(av1_max_pic_h, uint, 0664);
MODULE_PARM_DESC(av1_max_pic_h, "\n av1_max_pic_h\n");

module_param_array(decode_frame_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(display_frame_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(max_process_time, uint,
	&max_decode_instance_num, 0664);

module_param_array(run_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(input_empty, uint,
	&max_decode_instance_num, 0664);

module_param_array(not_run_ready, uint,
	&max_decode_instance_num, 0664);

#ifdef AOM_AV1_MMU_DW
module_param_array(dw_mmu_enable, uint,
	&max_decode_instance_num, 0664);
#endif

module_param(prefix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(prefix_aux_buf_size, "\n prefix_aux_buf_size\n");

module_param(suffix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(suffix_aux_buf_size, "\n suffix_aux_buf_size\n");

#endif

#ifdef DUMP_FILMGRAIN
module_param(fg_dump_index, uint, 0664);
MODULE_PARM_DESC(fg_dump_index, "\n fg_dump_index\n");
#endif

module_param(get_picture_qos, uint, 0664);
MODULE_PARM_DESC(get_picture_qos, "\n amvdec_av1 get_picture_qos\n");

module_param(force_bufspec, uint, 0664);
MODULE_PARM_DESC(force_bufspec, "\n amvdec_h265 force_bufspec\n");

module_param(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n amvdec_h265 udebug_flag\n");

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
module_param(dv_toggle_prov_name, uint, 0664);
MODULE_PARM_DESC(dv_toggle_prov_name, "\n dv_toggle_prov_name\n");
#endif

module_param(udebug_pause_pos, uint, 0664);
MODULE_PARM_DESC(udebug_pause_pos, "\n udebug_pause_pos\n");

module_param(udebug_pause_val, uint, 0664);
MODULE_PARM_DESC(udebug_pause_val, "\n udebug_pause_val\n");

module_param(udebug_pause_decode_idx, uint, 0664);
MODULE_PARM_DESC(udebug_pause_decode_idx, "\n udebug_pause_decode_idx\n");

#ifdef DEBUG_CRC_ERROR
module_param(crc_debug_flag, uint, 0664);
MODULE_PARM_DESC(crc_debug_flag, "\n crc_debug_flag\n");
#endif

#ifdef DEBUG_CMD
module_param(debug_cmd_wait_type, uint, 0664);
MODULE_PARM_DESC(debug_cmd_wait_type, "\n debug_cmd_wait_type\n");

module_param(debug_cmd_wait_count, uint, 0664);
MODULE_PARM_DESC(debug_cmd_wait_count, "\n debug_cmd_wait_count\n");

module_param(header_dump_size, uint, 0664);
MODULE_PARM_DESC(header_dump_size, "\n header_dump_size\n");
#endif

module_param(force_pts_unstable, uint, 0664);
MODULE_PARM_DESC(force_pts_unstable, "\n force_pts_unstable\n");

module_param(without_display_mode, uint, 0664);
MODULE_PARM_DESC(without_display_mode, "\n without_display_mode\n");

module_param(v4l_bitstream_id_enable, uint, 0664);
MODULE_PARM_DESC(v4l_bitstream_id_enable, "\n v4l_bitstream_id_enable\n");

module_param(enable_single_slice, uint, 0664);
MODULE_PARM_DESC(enable_single_slice, "\n  enable_single_slice\n");

module_init(amvdec_av1_driver_init_module);
module_exit(amvdec_av1_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC av1 Video Decoder Driver");
MODULE_LICENSE("GPL");

