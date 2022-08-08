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
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/sched/clock.h>
#include <uapi/linux/tee.h>
#include <media/v4l2-mem2mem.h>

#include "../../decoder/utils/decoder_mmu_box.h"
#include "../../decoder/utils/decoder_bmmu_box.h"
#include "../../decoder/utils/vdec.h"
#include "../../decoder/utils/amvdec.h"
#include "../../decoder/utils/config_parser.h"
#include "../../decoder/utils/firmware.h"
#include "../../decoder/utils/vdec_feature.h"
#include "../../decoder/utils/vdec_v4l2_buffer_ops.h"

#include "../../../stream_input/amports/amports_priv.h"
#include "../../../amvdec_ports/vdec_drv_base.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "avs2_global.h"
#include "vavs2.h"

#define MEM_NAME "codec_avs2"
#define I_ONLY_SUPPORT
#define MIX_STREAM_SUPPORT
#define G12A_BRINGUP_DEBUG
#define CONSTRAIN_MAX_BUF_NUM

#define CO_MV_COMPRESS

#define HEVC_SHIFT_LENGTH_PROTECT                  0x313a
#define HEVC_MPRED_CTRL4                           0x324c
#define HEVC_MPRED_CTRL9                           0x325b
#define HEVC_DBLK_CFGD                             0x350d
#define HEVC_CM_HEADER_START_ADDR                  0x3628
#define HEVC_DBLK_CFGB                             0x350b
#define HEVCD_MPP_ANC2AXI_TBL_DATA                 0x3464
#define HEVC_SAO_MMU_VH1_ADDR                      0x363b
#define HEVC_SAO_MMU_VH0_ADDR                      0x363a

/*
 * AVS2_DEC_STATUS define
*/
/*internal*/
#define AVS2_DEC_IDLE                           0
#define AVS2_SEQUENCE                           1
#define AVS2_I_PICTURE                          2
#define AVS2_PB_PICTURE                         3
#define AVS2_DISCARD_STARTCODE                  4
#define AVS2_DISCARD_NAL                        4

#define AVS2_SLICE_DECODING                     6

#define SWAP_IN_CMD                          0x10
#define SWAP_OUT_CMD                         0x11
#define SWAP_OUTIN_CMD                       0x12
#define SWAP_DONE                            0x13
#define SWAP_POST_INIT                       0x14

/*head*/
#define AVS2_HEAD_SEQ_READY                  0x21
#define AVS2_HEAD_PIC_I_READY                0x22
#define AVS2_HEAD_PIC_PB_READY               0x23
#define AVS2_HEAD_SEQ_END_READY              0x24
#define AVS2_STARTCODE_SEARCH_DONE           0x25

/*pic done*/
#define HEVC_DECPIC_DATA_DONE       0x30
#define HEVC_DECPIC_DATA_ERROR      0x31
#define HEVC_NAL_DECODE_DONE        0x32
#define AVS2_DECODE_BUFEMPTY        0x33
#define AVS2_DECODE_TIMEOUT         0x34
#define AVS2_DECODE_OVER_SIZE       0x35
#define AVS2_EOS                    0x36

/*cmd*/
#define AVS2_10B_DISCARD_NAL                 0xf0
#define AVS2_SEARCH_NEW_PIC                  0xf1
#define AVS2_ACTION_ERROR                    0xfe
#define HEVC_ACTION_ERROR                    0xfe
#define AVS2_ACTION_DONE                     0xff
/*AVS2_DEC_STATUS end*/

#define VF_POOL_SIZE        32

#undef pr_info
#define pr_info pr_cont

#define DECODE_MODE_SINGLE				(0 | (0x80 << 24))
#define DECODE_MODE_MULTI_STREAMBASE	(1 | (0x80 << 24))
#define DECODE_MODE_MULTI_FRAMEBASE		(2 | (0x80 << 24))

#define  VP9_TRIGGER_FRAME_DONE		0x100
#define  VP9_TRIGGER_FRAME_ENABLE	0x200

#define MV_MEM_UNIT 0x200
/*---------------------------------------------------
 Include "parser_cmd.h"
---------------------------------------------------*/
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

static int32_t g_WqMDefault4x4[16] = {
	64,     64,     64,     68,
	64,     64,     68,     72,
	64,     68,     76,     80,
	72,     76,     84,     96
};


static int32_t g_WqMDefault8x8[64] = {
	64,     64,     64,     64,     68,     68,     72,     76,
	64,     64,     64,     68,     72,     76,     84,     92,
	64,     64,     68,     72,     76,     80,     88,     100,
	64,     68,     72,     80,     84,     92,     100,    112,
	68,     72,     80,     84,     92,     104,    112,    128,
	76,     80,     84,     92,     104,    116,    132,    152,
	96,     100,    104,    116,    124,    140,    164,    188,
	104,    108,    116,    128,    152,    172,    192,    216
};

/* to remove, fix build error */
#define MULTI_INSTANCE_SUPPORT

#ifndef STAT_KTHREAD
#define STAT_KTHREAD 0x40
#endif

#ifdef MULTI_INSTANCE_SUPPORT
#define MAX_DECODE_INSTANCE_NUM     12
#define MULTI_DRIVER_NAME "ammvdec_avs2_v4l"

#define lock_buffer(dec, flags) \
		spin_lock_irqsave(&dec->buffer_lock, flags)

#define unlock_buffer(dec, flags) \
		spin_unlock_irqrestore(&dec->buffer_lock, flags)

static u32 debug_mask = 0xffffffff;
#define get_dbg_flag(dec) ((debug_mask & (1 << dec->index)) ? debug : 0)

static unsigned int max_decode_instance_num
				= MAX_DECODE_INSTANCE_NUM;
static unsigned int decode_frame_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int display_frame_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int max_process_time[MAX_DECODE_INSTANCE_NUM];
static unsigned int run_count[MAX_DECODE_INSTANCE_NUM];
static unsigned int input_empty[MAX_DECODE_INSTANCE_NUM];
static unsigned int not_run_ready[MAX_DECODE_INSTANCE_NUM];

#ifdef G12A_BRINGUP_DEBUG
static u32 decode_timeout_val = 200;
#else
static u32 decode_timeout_val = 200;
#endif
static int start_decode_buf_level = 0x8000;
#ifdef AVS2_10B_MMU
static u32 work_buf_size; /* = 24 * 1024 * 1024*/;
#else
static u32 work_buf_size = 32 * 1024 * 1024;
#endif

static u32 mv_buf_margin;
static int pre_decode_buf_level = 0x1000;
static u32 again_threshold;


/* DOUBLE_WRITE_MODE is enabled only when NV21 8 bit output is needed */
/* double_write_mode:
 *	0, no double write;
 *	1, 1:1 ratio;
 *	2, (1/4):(1/4) ratio;
 *	3, (1/4):(1/4) ratio, with both compressed frame included
 *	4, (1/2):(1/2) ratio;
 *	8, (1/8):(1/8) ratio;
 *	0x10, double write only
 *	0x100, if > 1080p,use mode 4,else use mode 1;
 *	0x200, if > 1080p,use mode 2,else use mode 1;
 *	0x300, if > 720p, use mode 4, else use mode 1;
 */
static u32 double_write_mode;
static u32 without_display_mode;

static u32 mv_buf_dynamic_alloc;
#define DRIVER_NAME "amvdec_avs2_v4l"
#define DRIVER_HEADER_NAME "amvdec_avs2_header"


#define PUT_INTERVAL        (HZ/100)
#define ERROR_SYSTEM_RESET_COUNT   200

#define PTS_NORMAL                0
#define PTS_NONE_REF_USE_DURATION 1

#define PTS_MODE_SWITCHING_THRESHOLD           3
#define PTS_MODE_SWITCHING_RECOVERY_THREASHOLD 3

#define DUR2PTS(x) ((x)*90/96)

struct AVS2Decoder_s;
static int vavs2_vf_states(struct vframe_states *states, void *);
static struct vframe_s *vavs2_vf_peek(void *);
static struct vframe_s *vavs2_vf_get(void *);
static void vavs2_vf_put(struct vframe_s *, void *);
static int vavs2_event_cb(int type, void *data, void *private_data);
static void set_vframe(struct AVS2Decoder_s *dec,
	struct vframe_s *vf, struct avs2_frame_s *pic, u8 dummy);
static s32 vavs2_init(struct vdec_s *vdec);
static void vavs2_prot_init(struct AVS2Decoder_s *dec);
static int vavs2_local_init(struct AVS2Decoder_s *dec);
static void vavs2_put_timer_func(struct timer_list *timer);
static void dump_data(struct AVS2Decoder_s *dec, int size);
static unsigned char get_data_check_sum
	(struct AVS2Decoder_s *dec, int size);
static void dump_pic_list(struct AVS2Decoder_s *dec);

static const char vavs2_dec_id[] = "vavs2-dev";

#define PROVIDER_NAME   "decoder.avs2"
#define MULTI_INSTANCE_PROVIDER_NAME    "vdec.avs2"

static const struct vframe_operations_s vavs2_vf_provider = {
	.peek = vavs2_vf_peek,
	.get = vavs2_vf_get,
	.put = vavs2_vf_put,
	.event_cb = vavs2_event_cb,
	.vf_states = vavs2_vf_states,
};

static u32 bit_depth_luma;
static u32 bit_depth_chroma;
static u32 frame_width;
static u32 frame_height;
static u32 video_signal_type;
static u32 pts_unstable;
static u32 on_no_keyframe_skiped;

static u32 force_video_signal_type;
static u32 enable_force_video_signal_type;
#define VIDEO_SIGNAL_TYPE_AVAILABLE_MASK	0x20000000
#define HDR_CUVA_MASK                           0x40000000


static const char * const video_format_names[] = {
	"component", "PAL", "NTSC", "SECAM",
	"MAC", "unspecified", "Reserved", "Reserved"
};

static inline int div_r32(int64_t m, int n)
{
/*
return (int)(m/n)
*/
#ifndef CONFIG_ARM64
	int64_t qu = 0;
	qu = div_s64(m, n);
	return (int)qu;
#else
	return (int)(m/n);
#endif
}

enum vpx_bit_depth_t {
	AVS2_BITS_8  =  8,  /**<  8 bits */
	AVS2_BITS_10 = 10,  /**< 10 bits */
	AVS2_BITS_12 = 12,  /**< 12 bits */
};

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
} /*BUF_t */;

struct MVBUF_s {
	unsigned long start_adr;
	unsigned int size;
	int used_flag;
} /*MVBUF_t */;


#define WR_PTR_INC_NUM 128

#define SIMULATION
#define DOS_PROJECT
#undef MEMORY_MAP_IN_REAL_CHIP

#ifdef AVS2_10B_NV21
#define MEM_MAP_MODE 2  /* 0:linear 1:32x32 2:64x32*/
#else
#define MEM_MAP_MODE 0  /* 0:linear 1:32x32 2:64x32*/
#endif

#ifdef AVS2_10B_NV21
#else
#define LOSLESS_COMPRESS_MODE
#endif

#define DOUBLE_WRITE_YSTART_TEMP 0x02000000
#define DOUBLE_WRITE_CSTART_TEMP 0x02900000

#define AVS2_DBG_BUFMGR                   0x01
#define AVS2_DBG_BUFMGR_MORE              0x02
#define AVS2_DBG_BUFMGR_DETAIL            0x04
#define AVS2_DBG_IRQ_EVENT                0x08
#define AVS2_DBG_OUT_PTS                  0x10
#define AVS2_DBG_PRINT_SOURCE_LINE        0x20
#define AVS2_DBG_PRINT_PARAM              0x40
#define AVS2_DBG_PRINT_PIC_LIST           0x80
#define AVS2_DBG_SEND_PARAM_WITH_REG      0x100
#define AVS2_DBG_MERGE                    0x200
#define AVS2_DBG_DBG_LF_PRINT             0x400
#define AVS2_DBG_REG                      0x800
#define AVS2_DBG_PIC_LEAK                 0x1000
#define AVS2_DBG_PIC_LEAK_WAIT            0x2000
#define AVS2_DBG_HDR_INFO                 0x4000
#define AVS2_DBG_HDR_DATA                 0x8000
#define AVS2_DBG_DIS_LOC_ERROR_PROC       0x10000
#define AVS2_DBG_DIS_SYS_ERROR_PROC   0x20000
#define AVS2_DBG_DUMP_PIC_LIST       0x40000
#define AVS2_DBG_TRIG_SLICE_SEGMENT_PROC 0x80000
#define AVS2_DBG_FORCE_UNCOMPRESS       0x100000
#define AVS2_DBG_LOAD_UCODE_FROM_FILE   0x200000
#define AVS2_DBG_FORCE_SEND_AGAIN       0x400000
#define AVS2_DBG_DUMP_DATA              0x800000
#define AVS2_DBG_DUMP_LMEM_BUF         0x1000000
#define AVS2_DBG_DUMP_RPM_BUF          0x2000000
#define AVS2_DBG_CACHE                 0x4000000
#define IGNORE_PARAM_FROM_CONFIG         0x8000000
/*MULTI_INSTANCE_SUPPORT*/
#define PRINT_FLAG_ERROR				0
#define	PRINT_FLAG_V4L_DETAIL			0x10000000
#define PRINT_FLAG_VDEC_STATUS             0x20000000
#define PRINT_FLAG_VDEC_DETAIL             0x40000000
#define PRINT_FLAG_VDEC_DATA             0x80000000

#define PRINT_LINE() \
	do { \
		if (debug & AVS2_DBG_PRINT_SOURCE_LINE)\
			pr_info("%s line %d\n", __func__, __LINE__);\
	} while (0)

static u32 debug;

static u32 debug_again;

bool is_avs2_print_param(void)
{
	bool ret = false;
	if (debug & AVS2_DBG_PRINT_PARAM)
		ret = true;
	return ret;
}

bool is_avs2_print_bufmgr_detail(void)
{
	bool ret = false;
	if (debug & AVS2_DBG_BUFMGR_DETAIL)
		ret = true;
	return ret;
}
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

static u32 force_disp_pic_index;

#define AUX_BUF_ALIGN(adr) ((adr + 0xf) & (~0xf))
static u32 cuva_buf_size = 512;

static u32 v4l_bitstream_id_enable = 1;

#define DEBUG_REG
#ifdef DEBUG_REG
static void WRITE_VREG_DBG2(unsigned adr, unsigned val)
{
	if (debug & AVS2_DBG_REG)
		pr_info("%s(%x, %x)\n", __func__, adr, val);
	if (adr != 0)
		WRITE_VREG(adr, val);
}

#undef WRITE_VREG
#define WRITE_VREG WRITE_VREG_DBG2
#endif

#define MMU_COMPRESS_HEADER_SIZE_1080P  0x10000
#define MMU_COMPRESS_HEADER_SIZE_4K  0x48000
#define MMU_COMPRESS_HEADER_SIZE_8K  0x120000

#define INVALID_IDX -1  /* Invalid buffer index.*/


#define FRAME_BUFFERS (AVS2_MAX_BUFFER_NUM)
#define HEADER_FRAME_BUFFERS (FRAME_BUFFERS)
#define MAX_BUF_NUM (FRAME_BUFFERS)

#define FRAME_CONTEXTS_LOG2 2
#define FRAME_CONTEXTS (1 << FRAME_CONTEXTS_LOG2)
/*buffer + header buffer + workspace*/
#undef MV_USE_FIXED_BUF
#ifdef MV_USE_FIXED_BUF
#define MAX_BMMU_BUFFER_NUM ((FRAME_BUFFERS + HEADER_FRAME_BUFFERS + 1)+1)
#define VF_BUFFER_IDX(n) (n)
#define HEADER_BUFFER_IDX(n) (FRAME_BUFFERS + n+1)
#define WORK_SPACE_BUF_ID (FRAME_BUFFERS + HEADER_FRAME_BUFFERS+1)
#else
#define MAX_BMMU_BUFFER_NUM (((FRAME_BUFFERS*2)+HEADER_FRAME_BUFFERS+1)+1)
#define VF_BUFFER_IDX(n) (n)
#define HEADER_BUFFER_IDX(n) (FRAME_BUFFERS + n+1)
#define MV_BUFFER_IDX(n) ((FRAME_BUFFERS * 2) + n+1)
#define WORK_SPACE_BUF_ID ((FRAME_BUFFERS * 2) + HEADER_FRAME_BUFFERS+1)
#endif

#define CO_MV_BUF_SIZE_1080P  0x3fc00
#define CO_MV_BUF_SIZE_4K     0x120000
#define CO_MV_BUF_SIZE_8K     0x480000

struct buff_s {
	u32 buf_start;
	u32 buf_size;
	u32 buf_end;
};

struct BuffInfo_s {
	u32 max_width;
	u32 max_height;
	u32 start_adr;
	u32 end_adr;
	struct buff_s ipp;
	struct buff_s sao_abv;
	struct buff_s sao_vb;
	struct buff_s short_term_rps;
	struct buff_s rcs;
	struct buff_s sps;
	struct buff_s pps;
	struct buff_s sao_up;
	struct buff_s swap_buf;
	struct buff_s swap_buf2;
	struct buff_s scalelut;
	struct buff_s dblk_para;
	struct buff_s dblk_data;
	struct buff_s dblk_data2;
#ifdef AVS2_10B_MMU
	struct buff_s mmu_vbh;
	struct buff_s cm_header;
#endif
	struct buff_s mpred_above;
#ifdef MV_USE_FIXED_BUF
	struct buff_s mpred_mv;
#endif
	struct buff_s rpm;
	struct buff_s lmem;
};

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

#define PROC_STATE_INIT			0
#define PROC_STATE_HEAD_DONE	1
#define PROC_STATE_DECODING		2
#define PROC_STATE_HEAD_AGAIN	3
#define PROC_STATE_DECODE_AGAIN	4
#define PROC_STATE_TEST1		5

static void avs2_work(struct work_struct *work);
struct loop_filter_info_n;
struct loopfilter;
struct segmentation;

struct AVS2Decoder_s {
	int pic_list_init_flag;
	unsigned char index;
	spinlock_t buffer_lock;
	struct device *cma_dev;
	struct platform_device *platform_dev;
	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;
	struct vframe_chunk_s *chunk;
	int dec_result;
	struct work_struct work;
	u32 start_shift_bytes;

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

	int double_write_mode;

	unsigned char m_ins_flag;
	char *provider_name;
	int frame_count;
	u32 stat;
	struct timer_list timer;
	u32 frame_dur;
	u32 frame_ar;
	int fatal_error;
	uint8_t init_flag;
	uint8_t first_sc_checked;
	uint8_t process_busy;
	uint8_t process_state;
	u32 ucode_pause_pos;

	int show_frame_num;
#ifndef AVS2_10B_MMU
	struct buff_s mc_buf_spec;
#endif
	struct dec_sysinfo vavs2_amstream_dec_info;
	void *rpm_addr;
	void *lmem_addr;
	dma_addr_t rpm_phy_addr;
	dma_addr_t lmem_phy_addr;
	unsigned short *lmem_ptr;
	unsigned short *debug_ptr;

	u32 cuva_size;
	void *cuva_addr;
	dma_addr_t cuva_phy_addr;

	/*AVS2_10B_MMU*/
	void *frame_mmu_map_addr;
	dma_addr_t frame_mmu_map_phy_addr;

	unsigned int use_cma_flag;

	struct BUF_s m_BUF[MAX_BUF_NUM];
	struct MVBUF_s m_mv_BUF[MAX_BUF_NUM];
	u32 used_buf_num;
	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(pending_q, struct vframe_s *, VF_POOL_SIZE);
	struct vframe_s vfpool[VF_POOL_SIZE];
	atomic_t vf_pre_count;
	atomic_t vf_get_count;
	atomic_t vf_put_count;
	int buf_num;
	unsigned int losless_comp_body_size;

	u32 video_signal_type;
	u32 video_ori_signal_type;

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

	int refresh_frame_flags;
	uint8_t hold_ref_buf;
	struct BuffInfo_s *work_space_buf;
#ifndef AVS2_10B_MMU
	struct buff_s *mc_buf;
#endif
	unsigned int frame_width;
	unsigned int frame_height;
	unsigned short *rpm_ptr;

	int     init_pic_w;
	int     init_pic_h;
	int     slice_type;
	int decode_idx;
	int slice_idx;
	uint8_t wait_buf;
	uint8_t error_flag;
	unsigned int bufmgr_error_count;

	/* bit 0, for decoding; bit 1, for displaying */
	uint8_t ignore_bufmgr_error;
	uint8_t skip_PB_before_I;
	int PB_skip_mode;
	int PB_skip_count_after_decoding;

	struct vdec_info *gvs;
	unsigned int dec_status;
	u32 last_put_idx;
	int new_frame_displayed;
	void *mmu_box;
	void *bmmu_box;
	struct vframe_master_display_colour_s vf_dp;
	struct firmware_s *fw;
#ifdef AVS2_10B_MMU
	int cur_fb_idx_mmu;
	long used_4k_num;
#endif
	struct avs2_decoder avs2_dec;
#define ALF_NUM_BIT_SHIFT      6
#define NO_VAR_BINS            16
	int32_t m_filterCoeffSym[16][9];
	int32_t m_varIndTab[NO_VAR_BINS];

	struct vframe_s vframe_dummy;
		/* start_decoding_flag,
			bit 0, SEQ ready
			bit 1, I ready
		*/
	unsigned char start_decoding_flag;
	uint32_t mpred_abv_start_addr;
	uint32_t mpred_abv_start_addr_bak;
	u8 next_again_flag;
	u32 pre_parser_wr_ptr;
	int need_cache_size;
	u64 sc_start_time;
#ifdef I_ONLY_SUPPORT
	u32 i_only;
#endif
	int frameinfo_enable;
	struct vframe_qos_s vframe_qos;
	u32 dynamic_buf_margin;
	int sidebind_type;
	int sidebind_channel_id;
	u32 endian;
	char vdec_name[32];
	char pts_name[32];
	char new_q_name[32];
	char disp_q_name[32];
	dma_addr_t rdma_phy_adr;
	unsigned *rdma_adr;
	int hdr_flag;
	bool is_used_v4l;
	void *v4l2_ctx;
	u32 res_ch_flag;
	u32 run_ready_min_buf_num;
	bool v4l_params_parsed;
	int last_width;
	int last_height;
	ulong fb_token;
	bool high_bandwidth_flag;
};

static int  compute_losless_comp_body_size(
		struct AVS2Decoder_s *dec, int width, int height,
		uint8_t is_bit_depth_10);

static int avs2_print(struct AVS2Decoder_s *dec,
	int flag, const char *fmt, ...)
{
#define HEVC_PRINT_BUF		256
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
	if (dec == NULL ||
		(flag == 0) ||
		(debug & flag)) {
		va_list args;
		va_start(args, fmt);
		if (dec)
			len = sprintf(buf, "[%d]", dec->index);
		vsnprintf(buf + len, HEVC_PRINT_BUF - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static int avs2_print_cont(struct AVS2Decoder_s *dec,
	int flag, const char *fmt, ...)
{
	unsigned char buf[HEVC_PRINT_BUF];
	int len = 0;
	if (dec == NULL ||
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

#define PROB_SIZE    (496 * 2 * 4)
#define PROB_BUF_SIZE    (0x5000)
#define COUNT_BUF_SIZE   (0x300 * 4 * 4)

#define MAX_FRAME_4K_NUM 0x1200
#define MAX_FRAME_8K_NUM 0x4800
#define MAX_SIZE_4K (4096 * 2304)
#define IS_8K_SIZE(w, h)	(((w) * (h)) > MAX_SIZE_4K)
#define IS_4K_SIZE(w, h)  (((w) * (h)) > (1920*1088))

static int get_frame_mmu_map_size(struct AVS2Decoder_s *dec)
{
	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) &&
		(IS_8K_SIZE(dec->init_pic_w, dec->init_pic_h)))
		return (MAX_FRAME_8K_NUM * 4);
	return (MAX_FRAME_4K_NUM * 4);
}

static void reset_process_time(struct AVS2Decoder_s *dec)
{
	if (dec->start_process_time) {
		unsigned process_time =
			1000 * (jiffies - dec->start_process_time) / HZ;
		dec->start_process_time = 0;
		if (process_time > max_process_time[dec->index])
			max_process_time[dec->index] = process_time;
	}
}

static void start_process_time(struct AVS2Decoder_s *dec)
{
	dec->start_process_time = jiffies;
	dec->decode_timeout_count = 0;
	dec->last_lcu_idx = 0;
}

static void update_decoded_pic(struct AVS2Decoder_s *dec);

static void timeout_process(struct AVS2Decoder_s *dec)
{
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct avs2_frame_s *cur_pic = avs2_dec->hc.cur_pic;
	struct aml_vcodec_ctx *ctx = dec->v4l2_ctx;

	dec->timeout_num++;
	amhevc_stop();
	avs2_print(dec,
		0, "%s decoder timeout\n", __func__);
	if (cur_pic)
		cur_pic->error_mark = 1;
	vdec_v4l_post_error_event(ctx, DECODER_WARNING_DECODER_TIMEOUT);
	dec->dec_result = DEC_RESULT_DONE;
	update_decoded_pic(dec);
	reset_process_time(dec);
	vdec_schedule_work(&dec->work);
}

static u32 get_valid_double_write_mode(struct AVS2Decoder_s *dec)
{
	return (dec->m_ins_flag &&
		((double_write_mode & 0x80000000) == 0)) ?
		dec->double_write_mode :
		(double_write_mode & 0x7fffffff);
}

static int get_double_write_mode(struct AVS2Decoder_s *dec)
{
	unsigned int out;
	u32 dw = 0x1; /*1:1*/

	vdec_v4l_get_dw_mode(dec->v4l2_ctx, &out);
	dw = out;
	return dw;

}

/* for double write buf alloc */
static int get_double_write_mode_init(struct AVS2Decoder_s *dec)
{
	u32 valid_dw_mode = get_valid_double_write_mode(dec);
	u32 dw;
	int w = dec->init_pic_w;
	int h = dec->init_pic_h;

	dw = 0x1; /*1:1*/
	switch (valid_dw_mode) {
	case 0x100:
		if (w * h > 1920 * 1088)
			dw = 0x4; /*1:2*/
		break;
	case 0x200:
		if (w * h > 1920 * 1088)
			dw = 0x2; /*1:4*/
		break;
	case 0x300:
		if (w * h > 1280 * 768)
			dw = 0x4; /*1:2*/
		break;
	default:
		dw = valid_dw_mode;
		break;
	}
	return dw;
}

static struct internal_comp_buf* v4lfb_to_icomp_buf(
		struct AVS2Decoder_s *dec,
		struct vdec_v4l2_buffer *fb)
{
	struct aml_video_dec_buf *aml_fb = NULL;
	struct aml_vcodec_ctx * v4l2_ctx = dec->v4l2_ctx;

	aml_fb = container_of(fb, struct aml_video_dec_buf, frame_buffer);
	return &v4l2_ctx->comp_bufs[aml_fb->internal_index];
}

static struct internal_comp_buf* index_to_icomp_buf(
		struct AVS2Decoder_s *dec, int index)
{
	struct aml_video_dec_buf *aml_fb = NULL;
	struct aml_vcodec_ctx * v4l2_ctx = dec->v4l2_ctx;
	struct vdec_v4l2_buffer *fb = NULL;

	fb = (struct vdec_v4l2_buffer *)dec->m_BUF[index].v4l_ref_buf_addr;
	aml_fb = container_of(fb, struct aml_video_dec_buf, frame_buffer);

	return &v4l2_ctx->comp_bufs[aml_fb->internal_index];
}

#ifdef AVS2_10B_MMU
int avs2_alloc_mmu(
	struct AVS2Decoder_s *dec,
	int cur_buf_idx,
	int pic_width,
	int pic_height,
	unsigned short bit_depth,
	unsigned int *mmu_index_adr)
{
	int ret;
	int bit_depth_10 = (bit_depth == AVS2_BITS_10);
	int picture_size;
	int cur_mmu_4k_number, max_frame_num;
	struct internal_comp_buf *ibuf = index_to_icomp_buf(dec, cur_buf_idx);

	picture_size = compute_losless_comp_body_size(
		dec, pic_width, pic_height,
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

	ret = decoder_mmu_box_alloc_idx(
			ibuf->mmu_box,
			ibuf->index,
			ibuf->frame_buffer_size,
			mmu_index_adr);
	return ret;
}
#endif

static int v4l_alloc_and_config_pic(struct AVS2Decoder_s *dec,
	struct avs2_frame_s *pic);
static void set_canvas(struct AVS2Decoder_s *dec,
	struct avs2_frame_s *pic);
static void init_pic_list_hw(struct AVS2Decoder_s *dec);

static int get_free_fb_idx(struct AVS2Decoder_s *dec)
{
	int i;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;

	for (i = 0; i < avs2_dec->ref_maxbuffer; ++i) {
		if ((avs2_dec->fref[i]->imgcoi_ref < -256) &&
				avs2_dec->fref[i]->is_output == -1 &&
				avs2_dec->fref[i]->bg_flag == 0 &&
				avs2_dec->fref[i]->vf_ref == 0 &&
				avs2_dec->fref[i]->to_prepare_disp == 0) {
			break;
		}
	}

	return i != avs2_dec->ref_maxbuffer ? i : INVALID_IDX;
}

static int v4l_get_free_fb(struct AVS2Decoder_s *dec)
{
	struct aml_vcodec_ctx * v4l = dec->v4l2_ctx;
	struct v4l_buff_pool *pool = &v4l->cap_pool;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct avs2_frame_s *pic = NULL;
	struct avs2_frame_s *free_pic = NULL;
	ulong flags;
	int idx, i;
	int free_index = INVALID_IDX;

	lock_buffer(dec, flags);

	for (i = 0; i < pool->in; ++i) {
		u32 state = (pool->seq[i] >> 16);

		switch (state) {
		case V4L_CAP_BUFF_IN_DEC:
			pic = avs2_dec->fref[i];
			if ((pic->imgcoi_ref < -256) &&
				(pic->index != -1) &&
				(pic->is_output == -1) &&
				(pic->bg_flag == 0) &&
				(pic->vf_ref == 0) &&
				(pic->to_prepare_disp == 0) &&
				(pic->cma_alloc_addr)) {
				free_pic = pic;
				free_index = i;
			}
			break;
		case V4L_CAP_BUFF_IN_M2M:
			idx = get_free_fb_idx(dec);
			if (idx < 0)
				break;
			pic = avs2_dec->fref[idx];
			pic->pic_w = dec->frame_width;
			pic->pic_h = dec->frame_height;
			if (!v4l_alloc_and_config_pic(dec, pic)) {
				set_canvas(dec, pic);
				init_pic_list_hw(dec);
				free_pic = pic;
				free_index = idx;
			}
			break;
		default:
			break;
		}

		if (free_pic) {
			break;
		}
	}

	if (free_pic && dec->chunk) {
		free_pic->timestamp = dec->chunk->timestamp;
	}

	unlock_buffer(dec, flags);

	if (free_pic) {
		struct vdec_v4l2_buffer *fb =
			(struct vdec_v4l2_buffer *)dec->m_BUF[free_pic->index].v4l_ref_buf_addr;

		fb->status = FB_ST_DECODER;
	}

	if (debug & AVS2_DBG_OUT_PTS) {
		if (free_pic) {
			pr_debug("%s, idx: %d, ts: %lld\n",
				__func__, free_index, free_pic->timestamp);
		} else {
			pr_debug("%s, avs2 get free pic null\n", __func__);
			dump_pic_list(dec);
		}
	}

	return free_index;
}

int get_free_frame_buffer(struct avs2_decoder *avs2_dec)
{
	struct AVS2Decoder_s *dec = container_of(avs2_dec, struct AVS2Decoder_s, avs2_dec);

	return v4l_get_free_fb(dec);
}

static int get_free_buf_count(struct AVS2Decoder_s *dec)
{
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	int i;
	int count = 0;
	for (i = 0; i < avs2_dec->ref_maxbuffer; i++) {
		if ((avs2_dec->fref[i]->imgcoi_ref < -256) &&
				avs2_dec->fref[i]->is_output == -1
				&& avs2_dec->fref[i]->bg_flag == 0
				&& avs2_dec->fref[i]->vf_ref == 0
				&& avs2_dec->fref[i]->to_prepare_disp == 0) {
			count++;
		}
	}

	return count;
}

#ifdef CONSTRAIN_MAX_BUF_NUM
static int get_vf_ref_only_buf_count(struct AVS2Decoder_s *dec)
{
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	int i;
	int count = 0;
	for (i = 0; i < avs2_dec->ref_maxbuffer; i++) {
		if ((avs2_dec->fref[i]->imgcoi_ref < -256) &&
				avs2_dec->fref[i]->is_output == -1 &&
				avs2_dec->fref[i]->bg_flag == 0 &&
				avs2_dec->fref[i]->vf_ref > 0 &&
				avs2_dec->fref[i]->to_prepare_disp == 0) {
			count++;
		}
	}

	return count;
}

static int get_used_buf_count(struct AVS2Decoder_s *dec)
{
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	int i;
	int count = 0;
	for (i = 0; i < avs2_dec->ref_maxbuffer; i++) {
		if ((avs2_dec->fref[i]->imgcoi_ref >= -256) ||
				avs2_dec->fref[i]->is_output != -1 ||
				avs2_dec->fref[i]->bg_flag != 0 ||
				avs2_dec->fref[i]->vf_ref != 0 ||
				avs2_dec->fref[i]->to_prepare_disp != 0) {
			count++;
		}
	}

	return count;
}
#endif

int avs2_bufmgr_init(struct AVS2Decoder_s *dec, struct BuffInfo_s *buf_spec_i,
		struct buff_s *mc_buf_i) {

	dec->frame_count = 0;
#ifdef AVS2_10B_MMU
	dec->used_4k_num = -1;
	dec->cur_fb_idx_mmu = INVALID_IDX;
#endif
	spin_lock_init(&dec->buffer_lock);

	/* private init */
	dec->work_space_buf = buf_spec_i;
#ifndef AVS2_10B_MMU
	dec->mc_buf = mc_buf_i;
#endif
	dec->rpm_addr = NULL;
	dec->lmem_addr = NULL;
	dec->use_cma_flag = 0;
	dec->decode_idx = 0;
	dec->slice_idx = 0;
	dec->wait_buf = 0;
	dec->error_flag = 0;
	dec->skip_PB_before_I = 0;
	dec->pts_mode = PTS_NORMAL;
	dec->last_pts = 0;
	dec->last_lookup_pts = 0;
	dec->last_pts_us64 = 0;
	dec->last_lookup_pts_us64 = 0;
	dec->shift_byte_count = 0;
	dec->shift_byte_count_lo = 0;
	dec->shift_byte_count_hi = 0;
	dec->pts_mode_switching_count = 0;
	dec->pts_mode_recovery_count = 0;
	dec->buf_num = 0;
	dec->bufmgr_error_count = 0;
	return 0;
}

#define HEVC_CM_BODY_START_ADDR                    0x3626
#define HEVC_CM_BODY_LENGTH                        0x3627
#define HEVC_CM_HEADER_LENGTH                      0x3629
#define HEVC_CM_HEADER_OFFSET                      0x362b

#define LOSLESS_COMPRESS_MODE

static u32 mem_map_mode; /* 0:linear 1:32x32 2:64x32 ; m8baby test1902 */
static u32 enable_mem_saving = 1;
static u32 force_w_h;
static u32 force_fps;

const u32 avs2_version = 201602101;
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

static u32 dynamic_buf_num_margin;
#else
static u32 buf_alloc_width;
static u32 buf_alloc_height;
static u32 dynamic_buf_num_margin = 7;
#endif
#ifdef CONSTRAIN_MAX_BUF_NUM
static u32 run_ready_max_vf_only_num;
static u32 run_ready_display_q_num;
	/*0: not check
	  0xff: avs2_dec.ref_maxbuffer
	  */
static u32 run_ready_max_buf_num = 0xff;
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

/*
bit 0, 1: only display I picture;
bit 1, 1: only decode I picture;
*/
static u32 i_only_flag;


static u32 max_decoding_time;
/*
error handling
*/
/*error_handle_policy:
bit 0: search seq again if buffer mgr error occur
	(buffer mgr error count need big than
	re_search_seq_threshold)
bit 1:  1, display from I picture;
		0, display from any correct pic
*/

static u32 error_handle_policy = 1;
/*
re_search_seq_threshold:
	bit 7~0: buffer mgr error research seq count
	bit 15~8: frame count threshold
*/
static u32 re_search_seq_threshold = 0x800; /*0x8;*/

static u32 max_buf_num = (REF_BUFFER + 1);

static u32 run_ready_min_buf_num = 2;

static DEFINE_MUTEX(vavs2_mutex);

#define HEVC_DEC_STATUS_REG       HEVC_ASSIST_SCRATCH_0
#define HEVC_RPM_BUFFER           HEVC_ASSIST_SCRATCH_1
#define AVS2_ALF_SWAP_BUFFER      HEVC_ASSIST_SCRATCH_2
#define HEVC_RCS_BUFFER           HEVC_ASSIST_SCRATCH_3
#define HEVC_SPS_BUFFER           HEVC_ASSIST_SCRATCH_4
#define HEVC_PPS_BUFFER           HEVC_ASSIST_SCRATCH_5
#ifdef AVS2_10B_MMU
#define AVS2_MMU_MAP_BUFFER       HEVC_ASSIST_SCRATCH_7
#else
#define HEVC_STREAM_SWAP_BUFFER   HEVC_ASSIST_SCRATCH_7
#endif
#define HEVC_STREAM_SWAP_BUFFER2  HEVC_ASSIST_SCRATCH_8
#define AVS2_CUVA_ADR             HEVC_ASSIST_SCRATCH_A
#define AVS2_CUVA_DATA_SIZE       HEVC_ASSIST_SCRATCH_B

#define HEVC_WAIT_FLAG            HEVC_ASSIST_SCRATCH_E
#define RPM_CMD_REG               HEVC_ASSIST_SCRATCH_F
#define LMEM_DUMP_ADR             HEVC_ASSIST_SCRATCH_9
#define HEVC_STREAM_SWAP_TEST     HEVC_ASSIST_SCRATCH_L
/*!!!*/
#define HEVC_DECODE_COUNT       HEVC_ASSIST_SCRATCH_M
#define HEVC_DECODE_SIZE		HEVC_ASSIST_SCRATCH_N
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
	/*DECODE_MODE: set before start decoder
		bit 7~0: decode mode
		bit 23~16: start_decoding_flag
			bit [0]   - SEQ_ready
			bit [2:1] - I Picture Count
		bit 31~24: chip feature
	*/
#define DECODE_MODE              HEVC_ASSIST_SCRATCH_J
#define DECODE_STOP_POS         HEVC_ASSIST_SCRATCH_K
	/*read only*/
#define CUR_NAL_UNIT_TYPE       HEVC_ASSIST_SCRATCH_J

#define RPM_BUF_SIZE (0x600 * 2)
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

/* necessary 4K page size align for t7/t3 decoder and after */
#define WORKBUF_ALIGN(addr) (ALIGN(addr, PAGE_SIZE))

#define WORK_BUF_SPEC_NUM 6
static struct BuffInfo_s amvavs2_workbuff_spec[WORK_BUF_SPEC_NUM] = {
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
		.rcs = {
			/* RCS STORE AREA - Max 32 RCS, each has 32 bytes,
				total 0x0400 bytes */
			.buf_size = 0x400,
		},
		.sps = {
			/* SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			total 0x0800 bytes*/
			.buf_size = 0x800,
		},
		.pps = {
			/*PPS STORE AREA - Max 64 PPS, each has 0x80 bytes,
			total 0x2000 bytes*/
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
			.buf_size = 0x40000,
		},
		.dblk_data = {
			.buf_size = 0x40000,
		},
		.dblk_data2 = {
			.buf_size = 0x40000,
		},
#ifdef AVS2_10B_MMU
		.mmu_vbh = {
			.buf_size = 0x5000, /*2*16*(more than 2304)/4, 4K*/
		},
#endif
		.mpred_above = {
			.buf_size = 0x8000, /* 2 * size of hevc*/
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {/* 1080p, 0x40000 per buffer */
			.buf_size = 0x40000 * FRAME_BUFFERS,
		},
#endif
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
		.rcs = {
			/* RCS STORE AREA - Max 16 RCS, each has 32 bytes,
			total 0x0400 bytes */
			.buf_size = 0x400,
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
			/*DBLK -> Max 256(4096/16) LCU,
			each para 1024bytes(total:0x40000),
			data 1024bytes(total:0x40000)*/
			.buf_size = 0x80000,
		},
		.dblk_data2 = {
			.buf_size = 0x80000,
		},
#ifdef AVS2_10B_MMU
		.mmu_vbh = {
			.buf_size = 0x5000,/*2*16*(more than 2304)/4, 4K*/
		},
#endif
		.mpred_above = {
			.buf_size = 0x10000, /* 2 * size of hevc*/
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
			/* 4096x2304 , 0x120000 per buffer */
			.buf_size = 0x120000 * FRAME_BUFFERS,
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x400 * 2,
		}
	},
	{
		.max_width = 4096 * 2,
		.max_height = 2304 * 2,
		.ipp = {
		/*IPP work space calculation : 4096 * (Y+CbCr+Flags) = 12k,
		round to 16k*/
			.buf_size = 0x4000 * 2,
		},
		.sao_abv = {
			.buf_size = 0x30000 * 2,
		},
		.sao_vb = {
			.buf_size = 0x30000 * 2,
		},
		.short_term_rps = {
		/*SHORT_TERM_RPS - Max 64 set, 16 entry every set,
			total 64x16x2 = 2048 bytes (0x800)*/
			.buf_size = 0x800,
		},
		.rcs = {
		/*RCS STORE AREA - Max 16 RCS, each has 32 bytes,
		total 0x0400 bytes*/
			.buf_size = 0x400,
		},
		.sps = {
		/*SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			total 0x0800 bytes*/
			.buf_size = 0x800,
		},
		.pps = {
		/*PPS STORE AREA - Max 64 PPS, each has 0x80 bytes, total
			0x2000 bytes*/
			.buf_size = 0x2000,
		},
		.sao_up = {
		/*SAO UP STORE AREA - Max 640(10240/16) LCU, each has 16 bytes i
				total 0x2800 bytes*/
			.buf_size = 0x2800 * 2,
		},
		.swap_buf = {
		/*256cyclex64bit = 2K bytes 0x800 (only 144 cycles valid)*/
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
		/*support up to 32 SCALELUT 1024x32 = 32Kbytes (0x8000)*/
			.buf_size = 0x8000 * 2,
		},
		.dblk_para  = {
			.buf_size = 0x40000 * 2,
		},
		.dblk_data  = {
			.buf_size = 0x80000 * 2,
		},
		.dblk_data2 = {
			.buf_size = 0x80000 * 2,
		},
#ifdef AVS2_10B_MMU
		.mmu_vbh = {
		  .buf_size = 0x5000 * 2, /*2*16*2304/4, 4K*/
		},
#endif
		.mpred_above = {
			.buf_size = 0x8000 * 2,
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
			/*4k2k , 0x100000 per buffer*/
			.buf_size = 0x120000 * FRAME_BUFFERS * 4,
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x400 * 2,
		}
	},
	{
		/* 8M bytes */
		.max_width = 1920,
		.max_height = 1088,
		.ipp = {
			/* IPP work space calculation :
			   4096 * (Y+CbCr+Flags) = 12k, round to 16k */
			.buf_size = 0x1e00,
		},
		.sao_abv = {
			.buf_size = 0,
		},
		.sao_vb = {
			.buf_size = 0,
		},
		.short_term_rps = {
			/* SHORT_TERM_RPS - Max 64 set, 16 entry every set,
			   total 64x16x2 = 2048 bytes (0x800) */
			.buf_size = 0x800,
		},
		.rcs = {
			/* RCS STORE AREA - Max 32 RCS, each has 32 bytes,
				total 0x0400 bytes */
			.buf_size = 0x400,
		},
		.sps = {
			/* SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			total 0x0800 bytes*/
			.buf_size = 0x800,
		},
		.pps = {
			/*PPS STORE AREA - Max 64 PPS, each has 0x80 bytes,
			total 0x2000 bytes*/
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
			.buf_size = 0,
		},
		.dblk_para = {
			/* DBLK -> Max 256(4096/16) LCU,
			each para 1024bytes(total:0x40000),
			data 1024bytes(total:0x40000)*/
			.buf_size = 0x3d00, //0x3c80,
		},
		.dblk_data = {
			.buf_size = 0x62800,
		},
		.dblk_data2 = {
			.buf_size = 0x62800,
		},
#ifdef AVS2_10B_MMU
		.mmu_vbh = {
			.buf_size = VBH_BUF_SIZE_1080P, /*2*16*(more than 2304)/4, 4K*/
		},
#endif
		.mpred_above = {
			.buf_size = 0x1e00, /* 2 * size of hevc*/
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {/* 1080p, 0x40000 per buffer */
			.buf_size = CO_MV_BUF_SIZE_1080P * FRAME_BUFFERS,
		},
#endif
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
			.buf_size = 0,
		},
		.sao_vb = {
			.buf_size = 0,
		},
		.short_term_rps = {
			/* SHORT_TERM_RPS - Max 64 set, 16 entry every set,
			   total 64x16x2 = 2048 bytes (0x800) */
			.buf_size = 0x800,
		},
		.rcs = {
			/* RCS STORE AREA - Max 16 RCS, each has 32 bytes,
			total 0x0400 bytes */
			.buf_size = 0x400,
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
			.buf_size = 0,
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
			.buf_size = 0,
		},
		.dblk_para = {
			/* DBLK -> Max 256(4096/16) LCU,
			each para 1024bytes(total:0x40000),
			data 1024bytes(total:0x40000)*/
			.buf_size = 0x8100, //0x8080,
		},
		.dblk_data = {
			/*DBLK -> Max 256(4096/16) LCU,
			each para 1024bytes(total:0x40000),
			data 1024bytes(total:0x40000)*/
			.buf_size = 0x88800,
		},
		.dblk_data2 = {
			.buf_size = 0x88800,
		},
#ifdef AVS2_10B_MMU
		.mmu_vbh = {
			.buf_size = VBH_BUF_SIZE_4K,/*2*16*(more than 2304)/4, 4K*/
		},
#endif
		.mpred_above = {
			.buf_size = 0x4000, /* 2 * size of hevc*/
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
			/* 4096x2304 , 0x120000 per buffer */
			.buf_size = CO_MV_BUF_SIZE_4K * FRAME_BUFFERS,
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x400 * 2,
		}
	},
	{
		.max_width = 4096 * 2,
		.max_height = 2304 * 2,
		.ipp = {
		/*IPP work space calculation : 4096 * (Y+CbCr+Flags) = 12k,
		round to 16k*/
			.buf_size = 0x4000 * 2,
		},
		.sao_abv = {
			.buf_size = 0,
		},
		.sao_vb = {
			.buf_size = 0,
		},
		.short_term_rps = {
		/*SHORT_TERM_RPS - Max 64 set, 16 entry every set,
			total 64x16x2 = 2048 bytes (0x800)*/
			.buf_size = 0x800,
		},
		.rcs = {
		/*RCS STORE AREA - Max 16 RCS, each has 32 bytes,
		total 0x0400 bytes*/
			.buf_size = 0x400,
		},
		.sps = {
		/*SPS STORE AREA - Max 16 SPS, each has 0x80 bytes,
			total 0x0800 bytes*/
			.buf_size = 0x800,
		},
		.pps = {
		/*PPS STORE AREA - Max 64 PPS, each has 0x80 bytes, total
			0x2000 bytes*/
			.buf_size = 0x2000,
		},
		.sao_up = {
		/*SAO UP STORE AREA - Max 640(10240/16) LCU, each has 16 bytes i
				total 0x2800 bytes*/
			.buf_size = 0,
		},
		.swap_buf = {
		/*256cyclex64bit = 2K bytes 0x800 (only 144 cycles valid)*/
			.buf_size = 0x800,
		},
		.swap_buf2 = {
			.buf_size = 0x800,
		},
		.scalelut = {
		/*support up to 32 SCALELUT 1024x32 = 32Kbytes (0x8000)*/
			.buf_size = 0,
		},
		.dblk_para  = {
			.buf_size = 0x10100, //0x10080,
		},
		.dblk_data  = {
			.buf_size = 0x110800,
		},
		.dblk_data2 = {
			.buf_size = 0x110800,
		},
#ifdef AVS2_10B_MMU
		.mmu_vbh = {
		  .buf_size = VBH_BUF_SIZE_8K, /*2*16*2304/4, 4K*/
		},
#endif
		.mpred_above = {
			.buf_size = 0x8000,
		},
#ifdef MV_USE_FIXED_BUF
		.mpred_mv = {
			/*4k2k , 0x100000 per buffer*/
			.buf_size = CO_MV_BUF_SIZE_8K * FRAME_BUFFERS,
		},
#endif
		.rpm = {
			.buf_size = RPM_BUF_SIZE,
		},
		.lmem = {
			.buf_size = 0x400 * 2,
		}
	}
};

#define IS_8K_SIZE(w, h)  (((w) * (h)) > MAX_SIZE_4K)
#define IS_4K_SIZE(w, h)  (((w) * (h)) > (1920*1088))
#ifndef MV_USE_FIXED_BUF
static uint32_t get_mv_buf_size(struct AVS2Decoder_s *dec, int width, int height) {
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	uint32_t size;
	if (mv_buf_dynamic_alloc == 1) {
		int mv_mem_unit =
			avs2_dec->lcu_size_log2 == 6 ? 0x200 : avs2_dec->lcu_size_log2 == 5 ? 0x80 : 0x20;
		int extended_pic_width = (width + avs2_dec->lcu_size -1)
				& (~(avs2_dec->lcu_size - 1));
		int extended_pic_height = (height + avs2_dec->lcu_size -1)
				& (~(avs2_dec->lcu_size - 1));
		int lcu_x_num = extended_pic_width / avs2_dec->lcu_size;
		int lcu_y_num = extended_pic_height / avs2_dec->lcu_size;
		int new_size =  lcu_x_num * lcu_y_num * mv_mem_unit;
		size = (new_size + 0xffff) & (~0xffff);

	} else {
		if (IS_8K_SIZE(width, height))
			size = CO_MV_BUF_SIZE_8K;
		else if (IS_4K_SIZE(width, height))
			size = CO_MV_BUF_SIZE_4K;
		else
			size = CO_MV_BUF_SIZE_1080P;
	}
	return size;
}
#endif

/*Losless compression body buffer size 4K per 64x32 (jt)*/
static int  compute_losless_comp_body_size(struct AVS2Decoder_s *dec,
	int width, int height,
	uint8_t is_bit_depth_10)
{
	int     width_x64;
	int     height_x32;
	int     bsize;
	width_x64 = width + 63;
	width_x64 >>= 6;
	height_x32 = height + 31;
	height_x32 >>= 5;
#ifdef AVS2_10B_MMU
	 bsize = (is_bit_depth_10 ? 4096 : 3200)
		* width_x64 * height_x32;
#else
	 bsize = (is_bit_depth_10 ? 4096 : 3072)
		* width_x64 * height_x32;
#endif
	avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
		"%s(%d,%d,%d)=>%d\n",
		__func__, width, height,
		is_bit_depth_10, bsize);

	return  bsize;
}

/* Losless compression header buffer size 32bytes per 128x64 (jt)*/
static  int  compute_losless_comp_header_size(struct AVS2Decoder_s *dec,
	int width, int height)
{
	int     width_x128;
	int     height_x64;
	int     hsize;
	width_x128 = width + 127;
	width_x128 >>= 7;
	height_x64 = height + 63;
	height_x64 >>= 6;

	hsize = 32 * width_x128 * height_x64;
	avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
		"%s(%d,%d)=>%d\n",
		__func__, width, height,
		hsize);

	return  hsize;
}

static void init_buff_spec(struct AVS2Decoder_s *dec,
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
	buf_spec->rcs.buf_start =
		WORKBUF_ALIGN(buf_spec->short_term_rps.buf_start + buf_spec->short_term_rps.buf_size);
	buf_spec->sps.buf_start =
		WORKBUF_ALIGN(buf_spec->rcs.buf_start + buf_spec->rcs.buf_size);
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
#ifdef AVS2_10B_MMU
	buf_spec->mmu_vbh.buf_start  =
		WORKBUF_ALIGN(buf_spec->dblk_data2.buf_start + buf_spec->dblk_data2.buf_size);
	buf_spec->mpred_above.buf_start =
		WORKBUF_ALIGN(buf_spec->mmu_vbh.buf_start + buf_spec->mmu_vbh.buf_size);
#else
	buf_spec->mpred_above.buf_start =
		WORKBUF_ALIGN(buf_spec->dblk_data2.buf_start + buf_spec->dblk_data2.buf_size);
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

	if (dec) {
		mem_start_virt =
			codec_mm_phys_to_virt(buf_spec->dblk_para.buf_start);
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
			pr_info("rcs.buf_start             :%x\n",
				   buf_spec->rcs.buf_start);
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
			pr_info("dblk_data2.buf_start       :%x\n",
				   buf_spec->dblk_data2.buf_start);
	#ifdef AVS2_10B_MMU
			pr_info("mmu_vbh.buf_start     :%x\n",
				buf_spec->mmu_vbh.buf_start);
	#endif
			pr_info("mpred_above.buf_start     :%x\n",
				   buf_spec->mpred_above.buf_start);
#ifdef MV_USE_FIXED_BUF
			pr_info("mpred_mv.buf_start        :%x\n",
				   buf_spec->mpred_mv.buf_start);
#endif
			if ((debug & AVS2_DBG_SEND_PARAM_WITH_REG) == 0) {
				pr_info("rpm.buf_start             :%x\n",
					   buf_spec->rpm.buf_start);
			}
		}
	}

}

static void uninit_mmu_buffers(struct AVS2Decoder_s *dec)
{
	decoder_mmu_box_free(dec->mmu_box);
	dec->mmu_box = NULL;

	if (dec->bmmu_box)
		decoder_bmmu_box_free(dec->bmmu_box);
	dec->bmmu_box = NULL;
}

#ifndef AVS2_10B_MMU
static void init_buf_list(struct AVS2Decoder_s *dec)
{
	int i;
	int buf_size;
	int mc_buffer_end = dec->mc_buf->buf_start + dec->mc_buf->buf_size;
	dec->used_buf_num = max_buf_num;

	if (dec->used_buf_num > MAX_BUF_NUM)
		dec->used_buf_num = MAX_BUF_NUM;
	if (buf_alloc_size > 0) {
		buf_size = buf_alloc_size;
		avs2_print(dec, AVS2_DBG_BUFMGR,
			"[Buffer Management] init_buf_list:\n");
	} else {
		int pic_width = dec->init_pic_w;
		int pic_height = dec->init_pic_h;

	/*SUPPORT_10BIT*/
	int losless_comp_header_size = compute_losless_comp_header_size
			(dec, pic_width, pic_height);
	int losless_comp_body_size = compute_losless_comp_body_size
			(dec, pic_width, pic_height, buf_alloc_depth == 10);
	int mc_buffer_size = losless_comp_header_size
		+ losless_comp_body_size;
	int mc_buffer_size_h = (mc_buffer_size + 0xffff)>>16;

	int dw_mode = get_double_write_mode_init(dec);

	if (dw_mode) {
		int pic_width_dw = pic_width /
			get_double_write_ratio(dw_mode);
		int pic_height_dw = pic_height /
			get_double_write_ratio(dw_mode);
		int lcu_size = 64; /*fixed 64*/
		int pic_width_64 = (pic_width_dw + 63) & (~0x3f);
		int pic_height_32 = (pic_height_dw + 31) & (~0x1f);
		int pic_width_lcu  =
			(pic_width_64 % lcu_size) ? pic_width_64 / lcu_size + 1
			: pic_width_64 / lcu_size;
		int pic_height_lcu =
			(pic_height_32 % lcu_size) ? pic_height_32 / lcu_size + 1
			: pic_height_32 / lcu_size;
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
	if ((dw_mode & 0x10) == 0)
		buf_size += (mc_buffer_size_h << 16);
		avs2_print(dec, AVS2_DBG_BUFMGR,
			"init_buf_list num %d (width %d height %d):\n",
			 dec->used_buf_num, pic_width, pic_height);
	}

	for (i = 0; i < dec->used_buf_num; i++) {
		if (((i + 1) * buf_size) > dec->mc_buf->buf_size)
			dec->use_cma_flag = 1;
#ifndef AVS2_10B_MMU
		dec->m_BUF[i].alloc_flag = 0;
		dec->m_BUF[i].index = i;

		dec->use_cma_flag = 1;
		if (dec->use_cma_flag) {
			dec->m_BUF[i].cma_page_count = PAGE_ALIGN(buf_size) / PAGE_SIZE;
			if (decoder_bmmu_box_alloc_buf_phy(dec->bmmu_box,
					VF_BUFFER_IDX(i), buf_size, DRIVER_NAME,
					&dec->m_BUF[i].alloc_addr) < 0) {
				dec->m_BUF[i].cma_page_count = 0;
				if (i <= 5) {
					dec->fatal_error |= DECODER_FATAL_ERROR_NO_MEM;
				}
				break;
			}
			dec->m_BUF[i].start_adr =  dec->m_BUF[i].alloc_addr;
		} else {
			dec->m_BUF[i].cma_page_count = 0;
			dec->m_BUF[i].alloc_addr = 0;
			dec->m_BUF[i].start_adr = dec->mc_buf->buf_start + i * buf_size;
		}
		dec->m_BUF[i].size = buf_size;
		dec->m_BUF[i].free_start_adr = dec->m_BUF[i].start_adr;

		if (((dec->m_BUF[i].start_adr + buf_size) > mc_buffer_end)
			&& (dec->m_BUF[i].alloc_addr == 0)) {
			if (debug) {
				avs2_print(dec, 0, "Max mc buffer or mpred_mv buffer is used\n");
			}
			break;
		}

		avs2_print(dec, AVS2_DBG_BUFMGR,
			"Buffer %d: start_adr %p size %x\n", i,
			   (void *)dec->m_BUF[i].start_adr,
			   dec->m_BUF[i].size);
#endif
	}
	dec->buf_num = i;
}
#endif

static int calc_luc_quantity(u32 w, u32 h)
{
	int lcu_size = 64; /*fixed 64*/
	int pic_width_64 = (w + 63) & (~0x3f);
	int pic_height_32 = (h + 31) & (~0x1f);
	int pic_width_lcu  = (pic_width_64 % lcu_size) ?
		pic_width_64 / lcu_size  + 1 : pic_width_64 / lcu_size;
	int pic_height_lcu = (pic_height_32 % lcu_size) ?
		pic_height_32 / lcu_size + 1 : pic_height_32 / lcu_size;

	return pic_width_lcu * pic_height_lcu;
}

static void avs2_put_video_frame(void *vdec_ctx, struct vframe_s *vf)
{
	vavs2_vf_put(vf, vdec_ctx);
}

static void avs2_get_video_frame(void *vdec_ctx, struct vframe_s **vf)
{
	*vf = vavs2_vf_get(vdec_ctx);
}

static struct task_ops_s task_dec_ops = {
	.type		= TASK_TYPE_DEC,
	.get_vframe	= avs2_get_video_frame,
	.put_vframe	= avs2_put_video_frame,
};

static int v4l_alloc_and_config_pic(struct AVS2Decoder_s *dec,
	struct avs2_frame_s *pic)
{
	int ret = -1;
	int i = pic->index;
	int dw_mode = get_double_write_mode_init(dec);
	int lcu_total = calc_luc_quantity(dec->frame_width, dec->frame_height);
	struct aml_vcodec_ctx * ctx = (struct aml_vcodec_ctx *)dec->v4l2_ctx;
	struct vdec_v4l2_buffer *fb = NULL;
	struct internal_comp_buf *ibuf = NULL;

	if (i < 0)
		return ret;

	ret = ctx->fb_ops.alloc(&ctx->fb_ops, dec->fb_token, &fb, AML_FB_REQ_DEC);
	if (ret < 0) {
		avs2_print(dec, 0, "[%d] AVS2 get buffer fail.\n", ctx->id);
		return ret;
	}

	fb->task->attach(fb->task, &task_dec_ops, dec);
	fb->status = FB_ST_DECODER;

#ifdef AVS2_10B_MMU
	ibuf = v4lfb_to_icomp_buf(dec, fb);
	dec->m_BUF[i].header_addr = ibuf->header_addr;
	avs2_print(dec, AVS2_DBG_BUFMGR_MORE, "MMU header_adr %d: %ld\n",
			i, dec->m_BUF[i].header_addr);
#endif

	dec->m_BUF[i].v4l_ref_buf_addr = (ulong)fb;
	pic->cma_alloc_addr = fb->m.mem[0].addr;
	if (fb->num_planes == 1) {
		dec->m_BUF[i].start_adr = fb->m.mem[0].addr;
		dec->m_BUF[i].luma_size = fb->m.mem[0].offset;
		dec->m_BUF[i].size = fb->m.mem[0].size;
		fb->m.mem[0].bytes_used = fb->m.mem[0].size;
		pic->dw_y_adr = dec->m_BUF[i].start_adr;
		pic->dw_u_v_adr = pic->dw_y_adr + dec->m_BUF[i].luma_size;
	} else if (fb->num_planes == 2) {
		dec->m_BUF[i].start_adr = fb->m.mem[0].addr;
		dec->m_BUF[i].size = fb->m.mem[0].size;
		dec->m_BUF[i].chroma_addr = fb->m.mem[1].addr;
		dec->m_BUF[i].chroma_size = fb->m.mem[1].size;
		fb->m.mem[0].bytes_used = fb->m.mem[0].size;
		fb->m.mem[1].bytes_used = fb->m.mem[1].size;
		pic->dw_y_adr = dec->m_BUF[i].start_adr;
		pic->dw_u_v_adr = dec->m_BUF[i].chroma_addr;
	}

#ifdef AVS2_10B_MMU
		pic->header_adr = dec->m_BUF[i].header_addr;
#endif

	pic->BUF_index		= i;
	pic->lcu_total		= lcu_total;
	pic->mc_canvas_y	= pic->index;
	pic->mc_canvas_u_v	= pic->index;

	if (dw_mode & 0x10) {
		pic->mc_canvas_y = (pic->index << 1);
		pic->mc_canvas_u_v = (pic->index << 1) + 1;
	}

	if (debug) {
		pr_info("%s index %d BUF_index %d ",
			__func__, pic->index,
			pic->BUF_index);
		pr_info("comp_body_size %x comp_buf_size %x ",
			pic->comp_body_size,
			pic->buf_size);
		pr_info("dw_y_adr %d, pic_config->dw_u_v_adr =%d\n",
			pic->dw_y_adr,
			pic->dw_u_v_adr);
	}
	return ret;
}

static void init_pic_list(struct AVS2Decoder_s *dec,
	int32_t lcu_size_log2)
{
	int i;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct avs2_frame_s *pic;

	dec->frame_height = avs2_dec->img.height;
	dec->frame_width = avs2_dec->img.width;

	for (i = 0; i < dec->used_buf_num; i++) {
		if (i == (dec->used_buf_num - 1))
			pic = avs2_dec->m_bg;
		else
			pic = avs2_dec->fref[i];
		pic->index = i;
		pic->BUF_index = -1;
		pic->mv_buf_index = -1;
		pic->double_write_mode = get_double_write_mode(dec);

		pic->pic_w = avs2_dec->img.width;
		pic->pic_h = avs2_dec->img.height;
	}
	for (; i < dec->used_buf_num; i++) {
		if (i == (dec->used_buf_num - 1))
			pic = avs2_dec->m_bg;
		else
			pic = avs2_dec->fref[i];
		pic->index = -1;
		pic->BUF_index = -1;
		pic->mv_buf_index = -1;
	}
	avs2_print(dec, AVS2_DBG_BUFMGR,
		"%s ok, used_buf_num = %d\n",
		__func__, dec->used_buf_num);
	dec->pic_list_init_flag = 1;
}


static void init_pic_list_hw(struct AVS2Decoder_s *dec)
{
	int i;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct avs2_frame_s *pic;

	for (i = 0; i < dec->used_buf_num; i++) {
		if (i == (dec->used_buf_num - 1))
			pic = avs2_dec->m_bg;
		else
			pic = avs2_dec->fref[i];
		if (pic->index < 0)
			break;
#ifdef AVS2_10B_MMU
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR,
		(0x1 << 1) | (pic->index << 8));

	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA, pic->header_adr >> 5);
#else
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA, pic->mc_y_adr >> 5);
#endif
#ifndef LOSLESS_COMPRESS_MODE
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA, pic->mc_u_v_adr >> 5);
#endif
	}
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x1);
	/*Zero out canvas registers in IPP -- avoid simulation X*/
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(0 << 8) | (0 << 1) | 1);
	for (i = 0; i < 32; i++) {
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);
	}
}


static void dump_pic_list(struct AVS2Decoder_s *dec)
{
	int ii;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	for (ii = 0; ii < avs2_dec->ref_maxbuffer; ii++) {
		avs2_print(dec, 0,
		"fref[%d]: index %d decode_id %d mvbuf %d imgcoi_ref %d imgtr_fwRefDistance %d refered %d, pre %d is_out %d, bg %d, vf_ref %d error %d lcu %d ref_pos(%d,%d,%d,%d,%d,%d,%d)\n",
		ii, avs2_dec->fref[ii]->index,
		avs2_dec->fref[ii]->decode_idx,
		avs2_dec->fref[ii]->mv_buf_index,
		avs2_dec->fref[ii]->imgcoi_ref,
		avs2_dec->fref[ii]->imgtr_fwRefDistance,
		avs2_dec->fref[ii]->refered_by_others,
		avs2_dec->fref[ii]->to_prepare_disp,
		avs2_dec->fref[ii]->is_output,
		avs2_dec->fref[ii]->bg_flag,
		avs2_dec->fref[ii]->vf_ref,
		avs2_dec->fref[ii]->error_mark,
		avs2_dec->fref[ii]->decoded_lcu,
		avs2_dec->fref[ii]->ref_poc[0],
		avs2_dec->fref[ii]->ref_poc[1],
		avs2_dec->fref[ii]->ref_poc[2],
		avs2_dec->fref[ii]->ref_poc[3],
		avs2_dec->fref[ii]->ref_poc[4],
		avs2_dec->fref[ii]->ref_poc[5],
		avs2_dec->fref[ii]->ref_poc[6]
		);
	}
	return;
}

static int config_mc_buffer(struct AVS2Decoder_s *dec)
{
	int32_t i;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct avs2_frame_s *pic;
	struct avs2_frame_s *cur_pic = avs2_dec->hc.cur_pic;

	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"Entered config_mc_buffer....\n");
	if (avs2_dec->f_bg != NULL) {
		avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
			"config_mc_buffer for background (canvas_y %d, canvas_u_v %d)\n",
		avs2_dec->f_bg->mc_canvas_y, avs2_dec->f_bg->mc_canvas_u_v);
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(15 << 8) | (0<<1) | 1);   /* L0:BG*/
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
			(avs2_dec->f_bg->mc_canvas_u_v << 16) |
			(avs2_dec->f_bg->mc_canvas_u_v << 8) |
			avs2_dec->f_bg->mc_canvas_y);
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(31 << 8) | (0<<1) | 1);  /* L1:BG*/
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
			(avs2_dec->f_bg->mc_canvas_u_v << 16) |
			(avs2_dec->f_bg->mc_canvas_u_v << 8) |
			avs2_dec->f_bg->mc_canvas_y);
	}

	if (avs2_dec->img.type == I_IMG)
		return 0;

	if (avs2_dec->img.type == P_IMG) {
		avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
			"config_mc_buffer for P_IMG, img type %d\n",
			avs2_dec->img.type);
		/*refer to prepare_RefInfo()*/
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(0 << 8) | (0<<1) | 1);
		for (i = 0; i < avs2_dec->img.num_of_references; i++) {
			pic = avs2_dec->fref[i];
			WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
			(pic->mc_canvas_u_v << 16) |
			(pic->mc_canvas_u_v << 8) |
			pic->mc_canvas_y);

			if (pic->error_mark)
				cur_pic->error_mark = 1;

			avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
				"refid %x mc_canvas_u_v %x mc_canvas_y %x error_mark %x\n",
				i, pic->mc_canvas_u_v, pic->mc_canvas_y,
				pic->error_mark);
		}
	} else if (avs2_dec->img.type == F_IMG) {
		avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
			"config_mc_buffer for F_IMG, img type %d\n",
			avs2_dec->img.type);
		/*refer to prepare_RefInfo()*/
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(0 << 8) | (0<<1) | 1);
		for (i = 0; i < avs2_dec->img.num_of_references; i++) {
			pic = avs2_dec->fref[i];
			WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
				(pic->mc_canvas_u_v << 16) |
				(pic->mc_canvas_u_v << 8) |
				pic->mc_canvas_y);

			if (pic->error_mark)
				cur_pic->error_mark = 1;

			avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
				"refid %x mc_canvas_u_v %x mc_canvas_y %x error_mark %x\n",
				i, pic->mc_canvas_u_v, pic->mc_canvas_y,
				pic->error_mark);
		}
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(16 << 8) | (0<<1) | 1);
		for (i = 0; i < avs2_dec->img.num_of_references; i++) {
			pic = avs2_dec->fref[i];
			WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
				(pic->mc_canvas_u_v << 16) |
				(pic->mc_canvas_u_v << 8) |
				pic->mc_canvas_y);
			avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
				"refid %x mc_canvas_u_v %x mc_canvas_y %x\n",
				i, pic->mc_canvas_u_v, pic->mc_canvas_y);
		}
	} else {
		avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
			"config_mc_buffer for B_IMG\n");
		/*refer to prepare_RefInfo()*/
		pic = avs2_dec->fref[1];
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(0 << 8) | (0<<1) | 1);
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
			(pic->mc_canvas_u_v << 16) |
			(pic->mc_canvas_u_v << 8) |
			pic->mc_canvas_y);

		if (pic->error_mark)
			cur_pic->error_mark = 1;

		avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
			"refid %x mc_canvas_u_v %x mc_canvas_y %x error_mark %x\n",
			1, pic->mc_canvas_u_v, pic->mc_canvas_y,
			pic->error_mark);

		pic = avs2_dec->fref[0];
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(16 << 8) | (0<<1) | 1);
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR,
			(pic->mc_canvas_u_v<<16) |
			(pic->mc_canvas_u_v<<8) |
			pic->mc_canvas_y);

		if (pic->error_mark)
			cur_pic->error_mark = 1;

		avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
			"refid %x mc_canvas_u_v %x mc_canvas_y %x error_mark %x\n",
			0, pic->mc_canvas_u_v, pic->mc_canvas_y,
			pic->error_mark);
	}
	return 0;
}

static void config_mcrcc_axi_hw(struct AVS2Decoder_s *dec)
{
	uint32_t rdata32;
	uint32_t rdata32_2;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;

	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2); /* reset mcrcc*/

	if (avs2_dec->img.type == I_IMG) { /* I-PIC*/
		/* remove reset -- disables clock */
		WRITE_VREG(HEVCD_MCRCC_CTL1, 0x0);
		return;
	}

	if ((avs2_dec->img.type == B_IMG) ||
		(avs2_dec->img.type == F_IMG)) { /*B-PIC or F_PIC*/
		/*Programme canvas0 */
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(0 << 8) | (0 << 1) | 0);
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		/*Programme canvas1 */
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(16 << 8) | (1 << 1) | 0);
		rdata32_2 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32_2 = rdata32_2 & 0xffff;
		rdata32_2 = rdata32_2 | (rdata32_2 << 16);
		if (rdata32 == rdata32_2) {
			rdata32_2 =
				READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
			rdata32_2 = rdata32_2 & 0xffff;
			rdata32_2 = rdata32_2 | (rdata32_2 << 16);
		}
		WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32_2);
	} else { /* P-PIC */
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			(0 << 8) | (1 << 1) | 0);
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		/*Programme canvas1*/
		rdata32 =
			READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32);
	}
	/*enable mcrcc progressive-mode */
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0);
	return;
}

static void config_mpred_hw(struct AVS2Decoder_s *dec)
{
	uint32_t data32;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct avs2_frame_s *cur_pic = avs2_dec->hc.cur_pic;
	struct avs2_frame_s *col_pic = avs2_dec->fref[0];
	int32_t mpred_mv_rd_start_addr;
	int32_t mpred_curr_lcu_x;
	int32_t mpred_curr_lcu_y;
	int32_t mpred_mv_rd_end_addr;
	int32_t above_en;
	int32_t mv_wr_en;
	int32_t mv_rd_en;
	int32_t col_isIntra;
	int mv_mem_unit;
	if (avs2_dec->img.type != I_IMG) {
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

	mv_mem_unit = avs2_dec->lcu_size_log2 == 6 ?
		0x200 : (avs2_dec->lcu_size_log2 == 5 ?
			0x80 : 0x20);

	mpred_mv_rd_end_addr = mpred_mv_rd_start_addr +
		((avs2_dec->lcu_x_num * avs2_dec->lcu_y_num) * mv_mem_unit);

	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"cur pic index %d  col pic index %d\n",
		cur_pic->index, col_pic->index);


	WRITE_VREG(HEVC_MPRED_MV_WR_START_ADDR,
		cur_pic->mpred_mv_wr_start_addr);
	WRITE_VREG(HEVC_MPRED_MV_RD_START_ADDR,
		col_pic->mpred_mv_wr_start_addr);
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"[MPRED CO_MV] write 0x%x  read 0x%x\n",
		cur_pic->mpred_mv_wr_start_addr,
		col_pic->mpred_mv_wr_start_addr);

	data32 = ((avs2_dec->bk_img_is_top_field) << 13) |
		((avs2_dec->hd.background_picture_enable & 1) << 12) |
		((avs2_dec->hd.curr_RPS.num_of_ref & 7) << 8) |
		((avs2_dec->hd.b_pmvr_enabled & 1) << 6) |
		((avs2_dec->img.is_top_field & 1) << 5) |
		((avs2_dec->img.is_field_sequence & 1) << 4) |
		((avs2_dec->img.typeb & 7) << 1) |
		(avs2_dec->hd.background_reference_enable & 0x1);
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"HEVC_MPRED_CTRL9 <= 0x%x(num of ref %d)\n",
		data32, avs2_dec->hd.curr_RPS.num_of_ref);
	WRITE_VREG(HEVC_MPRED_CTRL9, data32);

	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"%s: dis %d %d %d %d %d %d %d fref0_ref_poc %d %d %d %d %d %d %d\n",
		__func__,
		avs2_dec->fref[0]->imgtr_fwRefDistance,
		avs2_dec->fref[1]->imgtr_fwRefDistance,
		avs2_dec->fref[2]->imgtr_fwRefDistance,
		avs2_dec->fref[3]->imgtr_fwRefDistance,
		avs2_dec->fref[4]->imgtr_fwRefDistance,
		avs2_dec->fref[5]->imgtr_fwRefDistance,
		avs2_dec->fref[6]->imgtr_fwRefDistance,
		avs2_dec->fref[0]->ref_poc[0],
		avs2_dec->fref[0]->ref_poc[1],
		avs2_dec->fref[0]->ref_poc[2],
		avs2_dec->fref[0]->ref_poc[3],
		avs2_dec->fref[0]->ref_poc[4],
		avs2_dec->fref[0]->ref_poc[5],
		avs2_dec->fref[0]->ref_poc[6]
		);
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"pic_distance %d, imgtr_next_P %d\n",
		avs2_dec->img.pic_distance, avs2_dec->img.imgtr_next_P);


	WRITE_VREG(HEVC_MPRED_CUR_POC, avs2_dec->img.pic_distance);
	WRITE_VREG(HEVC_MPRED_COL_POC, avs2_dec->img.imgtr_next_P);

	/*below MPRED Ref_POC_xx_Lx registers
		must follow Ref_POC_xx_L0 ->
		Ref_POC_xx_L1 in pair write order!!!*/
	WRITE_VREG(HEVC_MPRED_L0_REF00_POC,
		avs2_dec->fref[0]->imgtr_fwRefDistance);
	WRITE_VREG(HEVC_MPRED_L1_REF00_POC,
		avs2_dec->fref[0]->ref_poc[0]);

	WRITE_VREG(HEVC_MPRED_L0_REF01_POC,
		avs2_dec->fref[1]->imgtr_fwRefDistance);
	WRITE_VREG(HEVC_MPRED_L1_REF01_POC,
		avs2_dec->fref[0]->ref_poc[1]);

	WRITE_VREG(HEVC_MPRED_L0_REF02_POC,
		avs2_dec->fref[2]->imgtr_fwRefDistance);
	WRITE_VREG(HEVC_MPRED_L1_REF02_POC,
		avs2_dec->fref[0]->ref_poc[2]);

	WRITE_VREG(HEVC_MPRED_L0_REF03_POC,
		avs2_dec->fref[3]->imgtr_fwRefDistance);
	WRITE_VREG(HEVC_MPRED_L1_REF03_POC,
		avs2_dec->fref[0]->ref_poc[3]);

	WRITE_VREG(HEVC_MPRED_L0_REF04_POC,
		avs2_dec->fref[4]->imgtr_fwRefDistance);
	WRITE_VREG(HEVC_MPRED_L1_REF04_POC,
		avs2_dec->fref[0]->ref_poc[4]);

	WRITE_VREG(HEVC_MPRED_L0_REF05_POC,
		avs2_dec->fref[5]->imgtr_fwRefDistance);
	WRITE_VREG(HEVC_MPRED_L1_REF05_POC,
		avs2_dec->fref[0]->ref_poc[5]);

	WRITE_VREG(HEVC_MPRED_L0_REF06_POC,
		avs2_dec->fref[6]->imgtr_fwRefDistance);
	WRITE_VREG(HEVC_MPRED_L1_REF06_POC,
		avs2_dec->fref[0]->ref_poc[6]);

	WRITE_VREG(HEVC_MPRED_MV_RD_END_ADDR,
		mpred_mv_rd_end_addr);
}

static void config_dblk_hw(struct AVS2Decoder_s *dec)
{
	/*
	* Picture level de-block parameter configuration here
	*/
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	union param_u *rpm_param = &avs2_dec->param;
	uint32_t data32;

	data32 = READ_VREG(HEVC_DBLK_CFG1);
	data32 = (((data32 >> 20) & 0xfff) << 20) |
		(((avs2_dec->input.sample_bit_depth == 10)
		? 0xa : 0x0) << 16) |   /*[16 +: 4]: {luma_bd[1:0],
							chroma_bd[1:0]}*/
		(((data32 >> 2) & 0x3fff) << 2) |
		(((rpm_param->p.lcu_size == 6)
		? 0 : (rpm_param->p.lcu_size == 5)
		? 1 : 2) << 0);/*[ 0 +: 2]: lcu_size*/
	WRITE_VREG(HEVC_DBLK_CFG1, data32);

	data32 = (avs2_dec->img.height << 16) |
		avs2_dec->img.width;
	WRITE_VREG(HEVC_DBLK_CFG2, data32);
	/*
	[27 +: 1]: cross_slice_loopfilter_enable_flag
	[26 +: 1]: loop_filter_disable
	[25 +: 1]: useNSQT
	[22 +: 3]: imgtype
	[17 +: 5]: alpha_c_offset (-8~8)
	[12 +: 5]: beta_offset (-8~8)
	[ 6 +: 6]: chroma_quant_param_delta_u (-16~16)
	[ 0 +: 6]: chroma_quant_param_delta_v (-16~16)
	*/
	data32 = ((avs2_dec->input.crossSliceLoopFilter & 0x1) << 27) |
		((rpm_param->p.loop_filter_disable & 0x1) << 26) |
		((avs2_dec->input.useNSQT & 0x1) << 25) |
		((avs2_dec->img.type & 0x7) << 22) |
		((rpm_param->p.alpha_c_offset & 0x1f) << 17) |
		((rpm_param->p.beta_offset & 0x1f) << 12) |
		((rpm_param->p.chroma_quant_param_delta_cb & 0x3f) << 6) |
		((rpm_param->p.chroma_quant_param_delta_cr & 0x3f) << 0);

	WRITE_VREG(HEVC_DBLK_CFG9, data32);
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"[c] cfgDBLK: crossslice(%d),lfdisable(%d),bitDepth(%d),lcuSize(%d),NSQT(%d)\n",
		avs2_dec->input.crossSliceLoopFilter,
		rpm_param->p.loop_filter_disable,
		avs2_dec->input.sample_bit_depth,
		avs2_dec->lcu_size,
		avs2_dec->input.useNSQT);
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"[c] cfgDBLK: alphaCOffset(%d),betaOffset(%d),quantDeltaCb(%d),quantDeltaCr(%d)\n",
		rpm_param->p.alpha_c_offset,
		rpm_param->p.beta_offset,
		rpm_param->p.chroma_quant_param_delta_cb,
		rpm_param->p.chroma_quant_param_delta_cr);
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL, "[c] cfgDBLK: .done.\n");
}

static void config_sao_hw(struct AVS2Decoder_s *dec)
{
	uint32_t data32;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct avs2_frame_s *cur_pic = avs2_dec->hc.cur_pic;
	struct aml_vcodec_ctx * v4l2_ctx = dec->v4l2_ctx;
	int lcu_size = 64;
	int mc_buffer_size_u_v = cur_pic->lcu_total * lcu_size*lcu_size / 2;
	int mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff) >> 16;/*64k alignment*/

	data32 = READ_VREG(HEVC_SAO_CTRL0);
	data32 &= (~0xf);
	data32 |= avs2_dec->lcu_size_log2;
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"%s, lcu_size_log2 = %d, config HEVC_SAO_CTRL0 0x%x\n",
		__func__,
		avs2_dec->lcu_size_log2,
		data32);

	WRITE_VREG(HEVC_SAO_CTRL0, data32);

#ifndef AVS2_10B_MMU
	if ((get_double_write_mode(dec) & 0x10) == 0)
		WRITE_VREG(HEVC_CM_BODY_START_ADDR, cur_pic->mc_y_adr);
#endif
	if (get_double_write_mode(dec)) {
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, cur_pic->dw_y_adr);
		WRITE_VREG(HEVC_SAO_C_START_ADDR, cur_pic->dw_u_v_adr);
		WRITE_VREG(HEVC_SAO_Y_WPTR, cur_pic->dw_y_adr);
		WRITE_VREG(HEVC_SAO_C_WPTR, cur_pic->dw_u_v_adr);
	} else {
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, 0xffffffff);
		WRITE_VREG(HEVC_SAO_C_START_ADDR, 0xffffffff);
	}
#ifdef AVS2_10B_MMU
	WRITE_VREG(HEVC_CM_HEADER_START_ADDR, cur_pic->header_adr);
#endif
	data32 = (mc_buffer_size_u_v_h << 16) << 1;

	WRITE_VREG(HEVC_SAO_Y_LENGTH, data32);

	data32 = (mc_buffer_size_u_v_h << 16);
	WRITE_VREG(HEVC_SAO_C_LENGTH, data32);

#ifdef AVS2_10B_NV21
#ifdef DOS_PROJECT
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	/*[13:12] axi_aformat, 0-Linear, 1-32x32, 2-64x32*/
	data32 |= (MEM_MAP_MODE << 12);
	data32 &= (~0x3);
	data32 |= 0x1; /* [1]:dw_disable [0]:cm_disable*/

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
	data32 &= (~(3 << 14));
	data32 |= (2 << 14);

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

	/*
	* [3:0]   little_endian
	* [5:4]   address_format 00:linear 01:32x32 10:64x32
	* [7:6]   reserved
	* [9:8]   Linear_LineAlignment 00:16byte 01:32byte 10:64byte
	* [11:10] reserved
	* [12]    CbCr_byte_swap
	* [31:13] reserved
	*/
	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	/*[5:4] address_format 00:linear 01:32x32 10:64x32*/
	data32 |= (MEM_MAP_MODE << 4);
	data32 &= (~0xF);
	data32 |= 0x8; /*Big-Endian per 64-bit*/

	data32 &= (~(3 << 8));
	data32 |= (2 << 8);
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#endif
#else
	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~(3 << 14));
	data32 |= (2 << 14);	/* line align with 64*/
	data32 &= (~0x3000);
	data32 |= (MEM_MAP_MODE << 12);	/* [13:12] axi_aformat, 0-Linear,
				   1-32x32, 2-64x32 */
	data32 &= (~0xff0);
	data32 |= ((dec->endian >> 8) & 0xfff);	/* data32 |= 0x670; Big-Endian per 64-bit */
	data32 &= (~0x3); /*[1]:dw_disable [0]:cm_disable*/

	if (get_double_write_mode(dec) == 0)
		data32 |= 0x2; /*disable double write*/
	else if (get_double_write_mode(dec) & 0x10)
		data32 |= 0x1; /*disable cm*/

	/* swap uv */
	if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21) ||
		(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21M))
		data32 &= ~(1 << 8); /* NV21 */
	else
		data32 |= (1 << 8); /* NV12 */

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

	if (get_double_write_mode(dec) & 0x10) {
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
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_T7)
			WRITE_VREG(HEVC_SAO_CTRL26, 0);

		data32 = READ_VREG(HEVC_SAO_CTRL5);
		data32 &= (~(0xff << 16));
		if (get_double_write_mode(dec) == 8 ||
			get_double_write_mode(dec) == 9) {
			data32 |= (0xff<<16);
			WRITE_VREG(HEVC_SAO_CTRL26, 0xf);
		} else if (get_double_write_mode(dec) == 2 ||
			get_double_write_mode(dec) == 3)
			data32 |= (0xff<<16);
		else if (get_double_write_mode(dec) == 4)
			data32 |= (0x33<<16);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}

	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	/* [5:4]    -- address_format 00:linear 01:32x32 10:64x32 */
	data32 |= (mem_map_mode << 4);
	data32 &= (~0xF);
	data32 |= (dec->endian & 0xf);  /* valid only when double write only */
	/*data32 |= 0x8;*/		/* Big-Endian per 64-bit */

	/* swap uv */
	if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21) ||
		(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV21M))
		data32 |= (1 << 12); /* NV21 */
	else
		data32 &= ~(1 << 12); /* NV12 */

	data32 &= (~(3 << 8));
	data32 |= (2 << 8);		/* line align with 64 for dw only */
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);
#endif
}

static void reconstructCoefficients(struct AVS2Decoder_s *dec,
	struct ALFParam_s *alfParam)
{
	int32_t g, sum, i, coeffPred;
	for (g = 0; g < alfParam->filters_per_group; g++) {
		sum = 0;
		for (i = 0; i < alfParam->num_coeff - 1; i++) {
			sum += (2 * alfParam->coeffmulti[g][i]);
			dec->m_filterCoeffSym[g][i] = alfParam->coeffmulti[g][i];
		}
		coeffPred = (1 << ALF_NUM_BIT_SHIFT) - sum;
		dec->m_filterCoeffSym[g][alfParam->num_coeff - 1] =
			coeffPred + alfParam->coeffmulti[g][alfParam->num_coeff - 1];
	}
}

static void reconstructCoefInfo(struct AVS2Decoder_s *dec,
	int32_t compIdx, struct ALFParam_s *alfParam)
{
	int32_t i;
	if (compIdx == ALF_Y) {
		if (alfParam->filters_per_group > 1) {
			for (i = 1; i < NO_VAR_BINS; ++i) {
				if (alfParam->filterPattern[i])
					dec->m_varIndTab[i] =
						dec->m_varIndTab[i - 1] + 1;
				else
					dec->m_varIndTab[i] =
						dec->m_varIndTab[i - 1];
			}
		}
	}
	reconstructCoefficients(dec, alfParam);
}

static void config_alf_hw(struct AVS2Decoder_s *dec)
{
	/*
	* Picture level ALF parameter configuration here
	*/
	uint32_t data32;
	int32_t i, j;
	int32_t m_filters_per_group;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct ALFParam_s *m_alfPictureParam_y = &avs2_dec->m_alfPictureParam[0];
	struct ALFParam_s *m_alfPictureParam_cb = &avs2_dec->m_alfPictureParam[1];
	struct ALFParam_s *m_alfPictureParam_cr = &avs2_dec->m_alfPictureParam[2];

	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"[t]alfy,cidx(%d),flag(%d),filters_per_group(%d),filterPattern[0]=0x%x,[15]=0x%x\n",
		m_alfPictureParam_y->componentID,
		m_alfPictureParam_y->alf_flag,
		m_alfPictureParam_y->filters_per_group,
		m_alfPictureParam_y->filterPattern[0],
		m_alfPictureParam_y->filterPattern[15]);
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"[t]alfy,num_coeff(%d),coeffmulti[0][0]=0x%x,[0][1]=0x%x,[1][0]=0x%x,[1][1]=0x%x\n",
		m_alfPictureParam_y->num_coeff,
		m_alfPictureParam_y->coeffmulti[0][0],
		m_alfPictureParam_y->coeffmulti[0][1],
		m_alfPictureParam_y->coeffmulti[1][0],
		m_alfPictureParam_y->coeffmulti[1][1]);

	/*Cr*/
	for (i = 0; i < 16; i++)
		dec->m_varIndTab[i] = 0;
	for (j = 0; j < 16; j++)
		for (i = 0; i < 9; i++)
			dec->m_filterCoeffSym[j][i] = 0;
	reconstructCoefInfo(dec, 2, m_alfPictureParam_cr);
	data32 = ((dec->m_filterCoeffSym[0][4] & 0xf) << 28) |
		((dec->m_filterCoeffSym[0][3] & 0x7f) << 21) |
		((dec->m_filterCoeffSym[0][2] & 0x7f) << 14) |
		((dec->m_filterCoeffSym[0][1] & 0x7f) << 7) |
		((dec->m_filterCoeffSym[0][0] & 0x7f) << 0);
	WRITE_VREG(HEVC_DBLK_CFGD, data32);
	data32 = ((dec->m_filterCoeffSym[0][8] & 0x7f) << 24) |
		((dec->m_filterCoeffSym[0][7] & 0x7f) << 17) |
		((dec->m_filterCoeffSym[0][6] & 0x7f) << 10) |
		((dec->m_filterCoeffSym[0][5] & 0x7f) << 3) |
		(((dec->m_filterCoeffSym[0][4] >> 4) & 0x7) <<  0);
	WRITE_VREG(HEVC_DBLK_CFGD, data32);
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"[c] pic_alf_on_cr(%d), alf_cr_coef(%d %d %d %d %d %d %d %d %d)\n",
		m_alfPictureParam_cr->alf_flag,
		dec->m_filterCoeffSym[0][0],
		dec->m_filterCoeffSym[0][1],
		dec->m_filterCoeffSym[0][2],
		dec->m_filterCoeffSym[0][3],
		dec->m_filterCoeffSym[0][4],
		dec->m_filterCoeffSym[0][5],
		dec->m_filterCoeffSym[0][6],
		dec->m_filterCoeffSym[0][7],
		dec->m_filterCoeffSym[0][8]);

	/* Cb*/
	for (j = 0; j < 16; j++)
		for (i = 0; i < 9; i++)
			dec->m_filterCoeffSym[j][i] = 0;
	reconstructCoefInfo(dec, 1, m_alfPictureParam_cb);
	data32 = ((dec->m_filterCoeffSym[0][4] & 0xf) << 28) |
		((dec->m_filterCoeffSym[0][3] & 0x7f) << 21) |
		((dec->m_filterCoeffSym[0][2] & 0x7f) << 14) |
		((dec->m_filterCoeffSym[0][1] & 0x7f) << 7) |
		((dec->m_filterCoeffSym[0][0] & 0x7f) << 0);
	WRITE_VREG(HEVC_DBLK_CFGD, data32);
	data32 = ((dec->m_filterCoeffSym[0][8] & 0x7f) << 24) |
		((dec->m_filterCoeffSym[0][7] & 0x7f) << 17) |
		((dec->m_filterCoeffSym[0][6] & 0x7f) << 10) |
		((dec->m_filterCoeffSym[0][5] & 0x7f) << 3) |
		(((dec->m_filterCoeffSym[0][4] >> 4) & 0x7) << 0);
	WRITE_VREG(HEVC_DBLK_CFGD, data32);
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"[c] pic_alf_on_cb(%d), alf_cb_coef(%d %d %d %d %d %d %d %d %d)\n",
		m_alfPictureParam_cb->alf_flag,
		dec->m_filterCoeffSym[0][0],
		dec->m_filterCoeffSym[0][1],
		dec->m_filterCoeffSym[0][2],
		dec->m_filterCoeffSym[0][3],
		dec->m_filterCoeffSym[0][4],
		dec->m_filterCoeffSym[0][5],
		dec->m_filterCoeffSym[0][6],
		dec->m_filterCoeffSym[0][7],
		dec->m_filterCoeffSym[0][8]);

	/* Y*/
	for (j = 0; j < 16; j++)
		for (i = 0; i < 9; i++)
			dec->m_filterCoeffSym[j][i] = 0;
	reconstructCoefInfo(dec, 0, m_alfPictureParam_y);
	data32 = ((dec->m_varIndTab[7] & 0xf) << 28) |
		((dec->m_varIndTab[6] & 0xf) << 24) |
		((dec->m_varIndTab[5] & 0xf) << 20) |
		((dec->m_varIndTab[4] & 0xf) << 16) |
		((dec->m_varIndTab[3] & 0xf) << 12) |
		((dec->m_varIndTab[2] & 0xf) << 8) |
		((dec->m_varIndTab[1] & 0xf) << 4) |
		((dec->m_varIndTab[0] & 0xf) << 0);
	WRITE_VREG(HEVC_DBLK_CFGD, data32);
	data32 = ((dec->m_varIndTab[15] & 0xf) << 28) |
		((dec->m_varIndTab[14] & 0xf) << 24) |
		((dec->m_varIndTab[13] & 0xf) << 20) |
		((dec->m_varIndTab[12] & 0xf) << 16) |
		((dec->m_varIndTab[11] & 0xf) << 12) |
		((dec->m_varIndTab[10] & 0xf) << 8) |
		((dec->m_varIndTab[9] & 0xf) << 4) |
		((dec->m_varIndTab[8] & 0xf) << 0);
	WRITE_VREG(HEVC_DBLK_CFGD, data32);
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
		"[c] pic_alf_on_y(%d), alf_y_tab(%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d)\n",
		m_alfPictureParam_y->alf_flag,
		dec->m_varIndTab[0],
		dec->m_varIndTab[1],
		dec->m_varIndTab[2],
		dec->m_varIndTab[3],
		dec->m_varIndTab[4],
		dec->m_varIndTab[5],
		dec->m_varIndTab[6],
		dec->m_varIndTab[7],
		dec->m_varIndTab[8],
		dec->m_varIndTab[9],
		dec->m_varIndTab[10],
		dec->m_varIndTab[11],
		dec->m_varIndTab[12],
		dec->m_varIndTab[13],
		dec->m_varIndTab[14],
		dec->m_varIndTab[15]);

	m_filters_per_group = (m_alfPictureParam_y->alf_flag == 0) ?
		1 : m_alfPictureParam_y->filters_per_group;
	for (i = 0; i < m_filters_per_group; i++) {
		data32 = ((dec->m_filterCoeffSym[i][4] & 0xf) << 28) |
			((dec->m_filterCoeffSym[i][3] & 0x7f) << 21) |
			((dec->m_filterCoeffSym[i][2] & 0x7f) << 14) |
			((dec->m_filterCoeffSym[i][1] & 0x7f) << 7) |
			((dec->m_filterCoeffSym[i][0] & 0x7f) << 0);
		WRITE_VREG(HEVC_DBLK_CFGD, data32);
		/*[31] last indication*/
		data32 = ((i == m_filters_per_group-1) << 31) |
			((dec->m_filterCoeffSym[i][8] & 0x7f) << 24) |
			((dec->m_filterCoeffSym[i][7] & 0x7f) << 17) |
			((dec->m_filterCoeffSym[i][6] & 0x7f) << 10) |
			((dec->m_filterCoeffSym[i][5] & 0x7f) << 3) |
			(((dec->m_filterCoeffSym[i][4] >> 4) & 0x7) << 0);
		WRITE_VREG(HEVC_DBLK_CFGD, data32);
		avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
			"[c] alf_y_coef[%d](%d %d %d %d %d %d %d %d %d)\n",
			i, dec->m_filterCoeffSym[i][0],
			dec->m_filterCoeffSym[i][1],
			dec->m_filterCoeffSym[i][2],
			dec->m_filterCoeffSym[i][3],
			dec->m_filterCoeffSym[i][4],
			dec->m_filterCoeffSym[i][5],
			dec->m_filterCoeffSym[i][6],
			dec->m_filterCoeffSym[i][7],
			dec->m_filterCoeffSym[i][8]);
	}
	avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL, "[c] cfgALF .done.\n");
}

static void config_other_hw(struct AVS2Decoder_s *dec)
{
	uint32_t data32;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct avs2_frame_s *cur_pic = avs2_dec->hc.cur_pic;
	int bit_depth = cur_pic->bit_depth;
	int losless_comp_header_size =
		compute_losless_comp_header_size(
		dec, cur_pic->pic_w,
		cur_pic->pic_h);
	int losless_comp_body_size =
		compute_losless_comp_body_size(
		dec, cur_pic->pic_w,
		cur_pic->pic_h, (bit_depth == AVS2_BITS_10));
	cur_pic->comp_body_size = losless_comp_body_size;

#ifdef LOSLESS_COMPRESS_MODE
	data32 = READ_VREG(HEVC_SAO_CTRL5);
	if (bit_depth == AVS2_BITS_10)
		data32 &= ~(1 << 9);
	else
		data32 |= (1 << 9);

	WRITE_VREG(HEVC_SAO_CTRL5, data32);

#ifdef AVS2_10B_MMU
	/*bit[4] : paged_mem_mode*/
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0x1 << 4));
#else
	/*bit[3] smem mdoe*/
	if (bit_depth == AVS2_BITS_10)
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
}

static u32 init_cuva_size;
static int cuva_data_is_avaible(struct AVS2Decoder_s *dec)
{
	u32 reg_val;

	reg_val = READ_VREG(AVS2_CUVA_DATA_SIZE);
	avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
		"%s:reg_val: %u \n", __func__, reg_val);
	if (reg_val != 0 && reg_val != init_cuva_size)
		return 1;
	else
		return 0;
}

static void config_cuva_buf(struct AVS2Decoder_s *dec)
{
	WRITE_VREG(AVS2_CUVA_ADR, dec->cuva_phy_addr);
	init_cuva_size = (dec->cuva_size >> 4) << 16;
	WRITE_VREG(AVS2_CUVA_DATA_SIZE, init_cuva_size);
}

static void set_cuva_data(struct AVS2Decoder_s *dec)
{
	int i;
	unsigned short *cuva_adr;
	unsigned int size_reg_val = READ_VREG(AVS2_CUVA_DATA_SIZE);
	unsigned int cuva_count = 0;
	int cuva_size = 0;
	struct avs2_frame_s *pic = dec->avs2_dec.hc.cur_pic;
	if (pic == NULL || 0 == cuva_data_is_avaible(dec)) {
		avs2_print(dec, AVS2_DBG_HDR_INFO,
		"%s:pic 0x%p or data not avaible\n",
		__func__, pic);
		return;
	}

	cuva_adr = (unsigned short *)dec->cuva_addr;
	cuva_count = ((size_reg_val >> 16) << 4) >> 1;
	cuva_size = dec->cuva_size;
	dec->hdr_flag |= HDR_CUVA_MASK;

	avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
			"%s:pic 0x%p cuva_count(%d) cuva_size(%d) hdr_flag 0x%x\n",
			__func__, pic, cuva_count, cuva_size, dec->hdr_flag);
	if (cuva_size > 0 && cuva_count > 0) {
		int new_size;
		char *new_buf;

		new_size = cuva_size;
		new_buf = vzalloc(new_size);
		if (new_buf) {
			unsigned char *p = new_buf;
			int len = 0;
			pic->cuva_data_buf = new_buf;

			for (i = 0; i < cuva_count; i += 4) {
				int j;

				for (j = 0; j < 4; j++) {
					unsigned short aa = cuva_adr[i + 3 - j];
					*p = aa & 0xff;
					p++;
					len++;
				}
			}
			if (len > 0) {
				pic->cuva_data_size = len;
			}

			avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
				"cuva: (size %d)\n",
				pic->cuva_data_size);
			if (get_dbg_flag(dec) & AVS2_DBG_HDR_DATA) {
				for (i = 0; i < pic->cuva_data_size; i++) {
					pr_info("%02x ", pic->cuva_data_buf[i]);
					if (((i + 1) & 0xf) == 0)
						pr_info("\n");
				}
				pr_info("\n");
			}
		} else {
			avs2_print(dec, 0, "new buf alloc failed\n");
			if (pic->cuva_data_buf)
				vfree(pic->cuva_data_buf);
			pic->cuva_data_buf = NULL;
			pic->cuva_data_size = 0;
		}
	}
}

static void release_cuva_data(struct avs2_frame_s *pic)
{
	if (pic == NULL)
		return;
	if (pic->cuva_data_buf) {
		vfree(pic->cuva_data_buf);
	}
	pic->cuva_data_buf = NULL;
	pic->cuva_data_size = 0;
}

static void avs2_config_work_space_hw(struct AVS2Decoder_s *dec)
{
	struct BuffInfo_s *buf_spec = dec->work_space_buf;
#ifdef LOSLESS_COMPRESS_MODE
	int losless_comp_header_size =
		compute_losless_comp_header_size(
		dec, dec->init_pic_w,
		dec->init_pic_h);
	int losless_comp_body_size =
		compute_losless_comp_body_size(dec,
		dec->init_pic_w,
		dec->init_pic_h, buf_alloc_depth == 10);
#endif
#ifdef AVS2_10B_MMU
	unsigned int data32;
#endif
	if (debug && dec->init_flag == 0)
		avs2_print(dec, 0,
			"%s %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			__func__,
			buf_spec->ipp.buf_start,
			buf_spec->start_adr,
			buf_spec->short_term_rps.buf_start,
			buf_spec->rcs.buf_start,
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
	if ((debug & AVS2_DBG_SEND_PARAM_WITH_REG) == 0)
		WRITE_VREG(HEVC_RPM_BUFFER, (u32)dec->rpm_phy_addr);
	WRITE_VREG(AVS2_ALF_SWAP_BUFFER, buf_spec->short_term_rps.buf_start);
	WRITE_VREG(HEVC_RCS_BUFFER, buf_spec->rcs.buf_start);
	WRITE_VREG(HEVC_SPS_BUFFER, buf_spec->sps.buf_start);
	WRITE_VREG(HEVC_PPS_BUFFER, buf_spec->pps.buf_start);
#ifdef AVS2_10B_MMU
	WRITE_VREG(AVS2_MMU_MAP_BUFFER, dec->frame_mmu_map_phy_addr);
#else
	WRITE_VREG(HEVC_STREAM_SWAP_BUFFER, buf_spec->swap_buf.buf_start);
#endif
	WRITE_VREG(HEVC_STREAM_SWAP_BUFFER2, buf_spec->swap_buf2.buf_start);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) {
		if (buf_spec->max_width <= 4096 && buf_spec->max_height <= 2304)
			WRITE_VREG(HEVC_DBLK_CFG3, 0x404010); //default value
		else
			WRITE_VREG(HEVC_DBLK_CFG3, 0x808020); // make left storage 2 x 4k]
		avs2_print(dec, AVS2_DBG_BUFMGR,
			"HEVC_DBLK_CFG3 = %x\n", READ_VREG(HEVC_DBLK_CFG3));
	}

	/* cfg_p_addr */
	WRITE_VREG(HEVC_DBLK_CFG4, buf_spec->dblk_para.buf_start);
	/* cfg_d_addr */
	WRITE_VREG(HEVC_DBLK_CFG5, buf_spec->dblk_data.buf_start);

	WRITE_VREG(HEVC_DBLK_CFGE, buf_spec->dblk_data2.buf_start);

#ifdef LOSLESS_COMPRESS_MODE
	data32 = READ_VREG(HEVC_SAO_CTRL5);
	data32 &= ~(1<<9);

	WRITE_VREG(HEVC_SAO_CTRL5, data32);
#ifdef AVS2_10B_MMU
	/*bit[4] : paged_mem_mode*/
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0x1 << 4));
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, 0);
#else
	/* bit[3] smem mode*/
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0<<3));

	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, (losless_comp_body_size >> 5));
#endif
/*8-bit mode */
	WRITE_VREG(HEVC_CM_BODY_LENGTH, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET, losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH, losless_comp_header_size);
#else
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);
#endif

#ifdef AVS2_10B_MMU
	WRITE_VREG(HEVC_SAO_MMU_VH0_ADDR, buf_spec->mmu_vbh.buf_start);
	WRITE_VREG(HEVC_SAO_MMU_VH1_ADDR, buf_spec->mmu_vbh.buf_start
			+ VBH_BUF_SIZE(buf_spec));

	/* use HEVC_CM_HEADER_START_ADDR */
	data32 = READ_VREG(HEVC_SAO_CTRL5);
	data32 |= (1<<10);

	if (debug & AVS2_DBG_FORCE_UNCOMPRESS)
		data32 |= 0x80;

	WRITE_VREG(HEVC_SAO_CTRL5, data32);
#endif

	WRITE_VREG(LMEM_DUMP_ADR, (u32)dec->lmem_phy_addr);

/*MULTI_INSTANCE_SUPPORT*/
	/*new added in simulation???*/
	WRITE_VREG(HEVC_MPRED_ABV_START_ADDR, buf_spec->mpred_above.buf_start);

#ifdef CO_MV_COMPRESS
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_T7) {
	    data32 = READ_VREG(HEVC_MPRED_CTRL4);
	    data32 |=  (1 << 1);
	    WRITE_VREG(HEVC_MPRED_CTRL4, data32);
	}
#endif
}

static void decomp_perfcount_reset(void)
{
	if (debug & AVS2_DBG_CACHE)
		pr_info("[cache_util.c] Entered decomp_perfcount_reset...\n");
	WRITE_VREG(HEVCD_MPP_DECOMP_PERFMON_CTL, (unsigned int)0x1);
	WRITE_VREG(HEVCD_MPP_DECOMP_PERFMON_CTL, (unsigned int)0x0);
	return;
}

static void mcrcc_perfcount_reset(void)
{
	if (debug & AVS2_DBG_CACHE)
		pr_info("[cache_util.c] Entered mcrcc_perfcount_reset...\n");
	WRITE_VREG(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)0x1);
	WRITE_VREG(HEVCD_MCRCC_PERFMON_CTL, (unsigned int)0x0);
	return;
}

static void avs2_init_decoder_hw(struct AVS2Decoder_s *dec)
{
	unsigned int data32;
	unsigned int decode_mode;
	int i;

	data32 = READ_VREG(HEVC_PARSER_INT_CONTROL);

	/* set bit 31~29 to 3 if HEVC_STREAM_FIFO_CTL[29] is 1 */
	data32 &= ~(7 << 29);
	data32 |= (3 << 29);
	data32 = data32 |
		(1 << 24) |/*stream_buffer_empty_int_amrisc_enable*/
		(1 << 22) |/*stream_fifo_empty_int_amrisc_enable*/
		(1 << 7) |/*dec_done_int_cpu_enable*/
		(1 << 4) |/*startcode_found_int_cpu_enable*/
		(0 << 3) |/*startcode_found_int_amrisc_enable*/
		(1 << 0)    /*parser_int_enable*/
		;
	WRITE_VREG(HEVC_PARSER_INT_CONTROL, data32);

	data32 = READ_VREG(HEVC_SHIFT_STATUS);
	data32 = data32 |
	(0 << 1) |/*emulation_check_off VP9
		do not have emulation*/
	(1 << 0)/*startcode_check_on*/
	;
	WRITE_VREG(HEVC_SHIFT_STATUS, data32);
	WRITE_VREG(HEVC_SHIFT_CONTROL,
		(6 << 20) | /* emu_push_bits  (6-bits for AVS2)*/
		(0 << 19) | /* emu_3_enable, maybe turned on in microcode*/
		(0 << 18) | /* emu_2_enable, maybe turned on in microcode*/
		(0 << 17) | /* emu_1_enable, maybe turned on in microcode*/
		(0 << 16) | /* emu_0_enable, maybe turned on in microcode*/
		(0 << 14) | /*disable_start_code_protect*/
		(3 << 6) | /* sft_valid_wr_position*/
		(2 << 4) | /* emulate_code_length_sub_1*/
		(2 << 1) | /* start_code_length_sub_1*/
		(1 << 0)   /* stream_shift_enable*/
		);

	WRITE_VREG(HEVC_SHIFT_LENGTH_PROTECT,
		(0 << 30) |   /*data_protect_fill_00_enable*/
		(1 << 29)     /*data_protect_fill_ff_enable*/
		);
	WRITE_VREG(HEVC_CABAC_CONTROL,
		(1 << 0)/*cabac_enable*/
	);

	WRITE_VREG(HEVC_PARSER_CORE_CONTROL,
		(1 << 0)/* hevc_parser_core_clk_en*/
	);

	WRITE_VREG(HEVC_DEC_STATUS_REG, 0);

	/*Initial IQIT_SCALELUT memory -- just to avoid X in simulation*/
	if (is_rdma_enable())
		rdma_back_end_work(dec->rdma_phy_adr, RDMA_SIZE);
	else {
		WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0);/*cfg_p_addr*/
		for (i = 0; i < 1024; i++)
			WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, 0);
	}

#ifdef ENABLE_SWAP_TEST
	WRITE_VREG(HEVC_STREAM_SWAP_TEST, 100);
#else
	WRITE_VREG(HEVC_STREAM_SWAP_TEST, 0);
#endif
	if (!dec->m_ins_flag)
		decode_mode = DECODE_MODE_SINGLE;
	else if (vdec_frame_based(hw_to_vdec(dec)))
		decode_mode = DECODE_MODE_MULTI_FRAMEBASE;
	else
		decode_mode = DECODE_MODE_MULTI_STREAMBASE;
	if (dec->avs2_dec.bufmgr_error_flag &&
		(error_handle_policy & 0x1)) {
		dec->bufmgr_error_count++;
		dec->avs2_dec.bufmgr_error_flag = 0;
		if (dec->bufmgr_error_count > (re_search_seq_threshold & 0xff) &&
			dec->frame_count > ((re_search_seq_threshold >> 8) & 0xff)) {
			struct avs2_decoder *avs2_dec = &dec->avs2_dec;
			dec->start_decoding_flag = 0;
			avs2_dec->hd.vec_flag = 1;
			dec->skip_PB_before_I = 1;
			avs2_print(dec, 0,
				"!!Bufmgr error, search seq again (0x%x %d %d)\n",
				error_handle_policy,
				dec->frame_count,
				dec->bufmgr_error_count);
			dec->bufmgr_error_count = 0;
		}
	}
	decode_mode |= (dec->start_decoding_flag << 16);

	WRITE_VREG(DECODE_MODE, decode_mode);
	WRITE_VREG(HEVC_DECODE_SIZE, 0);
	WRITE_VREG(HEVC_DECODE_COUNT, 0);

	/*Send parser_cmd*/
	WRITE_VREG(HEVC_PARSER_CMD_WRITE, (1 << 16) | (0 << 0));
	for (i = 0; i < PARSER_CMD_NUMBER; i++)
		WRITE_VREG(HEVC_PARSER_CMD_WRITE, parser_cmd[i]);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_0, PARSER_CMD_SKIP_CFG_0);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_1, PARSER_CMD_SKIP_CFG_1);
	WRITE_VREG(HEVC_PARSER_CMD_SKIP_2, PARSER_CMD_SKIP_CFG_2);
	WRITE_VREG(HEVC_PARSER_IF_CONTROL,
		(1 << 9) | /* parser_alf_if_en*/
		/*  (1 << 8) |*/ /*sao_sw_pred_enable*/
		(1 << 5) | /*parser_sao_if_en*/
		(1 << 2) | /*parser_mpred_if_en*/
		(1 << 0) /*parser_scaler_if_en*/
	);

#ifdef MULTI_INSTANCE_SUPPORT
	WRITE_VREG(HEVC_MPRED_INT_STATUS, (1<<31));

	WRITE_VREG(HEVC_PARSER_RESULT_3, 0xffffffff);

	for (i = 0; i < 8; i++)
		data32 = READ_VREG(HEVC_MPRED_ABV_START_ADDR);

	WRITE_VREG(DOS_SW_RESET3, (1<<18)); /* reset mpred */
	WRITE_VREG(DOS_SW_RESET3, 0);
	WRITE_VREG(HEVC_MPRED_ABV_START_ADDR, data32);
	WRITE_VREG(HEVC_MPRED_ABV_START_ADDR, data32);
	WRITE_VREG(HEVC_MPRED_ABV_START_ADDR, data32);
#endif

	avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
		"Config AVS2 default seq_wq_matrix ...\n");
	/*4x4*/
	 /* default seq_wq_matrix_4x4 begin address*/
	WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 64);
	for (i = 0; i < 16; i++)
		WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, g_WqMDefault4x4[i]);

	/*8x8*/
	/*default seq_wq_matrix_8x8 begin address*/
	WRITE_VREG(HEVC_IQIT_SCALELUT_WR_ADDR, 0);
	for (i = 0; i < 64; i++)
		WRITE_VREG(HEVC_IQIT_SCALELUT_DATA, g_WqMDefault8x8[i]);

	WRITE_VREG(HEVCD_IPP_TOP_CNTL,
		(0 << 1) | /*enable ipp*/
		(1 << 0)   /*software reset ipp and mpp*/
	);
	WRITE_VREG(HEVCD_IPP_TOP_CNTL,
		(1 << 1) | /*enable ipp*/
		(0 << 0)   /*software reset ipp and mpp*/
	);

	/* Init dblk*/
	data32 = READ_VREG(HEVC_DBLK_CFGB);
	data32 |= (2 << 0);
	/* [3:0] cfg_video_type -> AVS2*/

	data32 &= (~0x300); /*[8]:first write enable (compress)
					[9]:double write enable (uncompress)*/
	if (get_double_write_mode(dec) == 0)
		data32 |= (0x1 << 8); /*enable first write*/
	else if (get_double_write_mode(dec) == 0x10)
		data32 |= (0x1 << 9); /*double write only*/
	else
		data32 |= ((0x1 << 8) | (0x1 << 9));
	WRITE_VREG(HEVC_DBLK_CFGB, data32);

	WRITE_VREG(HEVC_DBLK_CFG0, (1 << 0)); /* [0] rst_sync*/
	avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
		"Bitstream level Init for DBLK .Done.\n");

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) {
	    mcrcc_perfcount_reset();
	    decomp_perfcount_reset();
	}

	return;
}


#ifdef CONFIG_HEVC_CLK_FORCED_ON
static void config_avs2_clk_forced_on(void)
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

static void avs2_local_uninit(struct AVS2Decoder_s *dec)
{
	dec->rpm_ptr = NULL;
	dec->lmem_ptr = NULL;
	if (dec->rpm_addr) {
		dma_free_coherent(amports_get_dma_device(),
						RPM_BUF_SIZE, dec->rpm_addr,
						dec->rpm_phy_addr);
		dec->rpm_addr = NULL;
	}

	if (dec->cuva_addr) {
		dma_free_coherent(amports_get_dma_device(),
				dec->cuva_size, dec->cuva_addr,
					dec->cuva_phy_addr);
		dec->cuva_addr = NULL;
	}

	if (dec->lmem_addr) {
			if (dec->lmem_phy_addr)
				dma_free_coherent(amports_get_dma_device(),
						LMEM_BUF_SIZE, dec->lmem_addr,
						dec->lmem_phy_addr);
		dec->lmem_addr = NULL;
	}

#ifdef AVS2_10B_MMU
	if (dec->frame_mmu_map_addr) {
		if (dec->frame_mmu_map_phy_addr)
			dma_free_coherent(amports_get_dma_device(),
				get_frame_mmu_map_size(dec), dec->frame_mmu_map_addr,
					dec->frame_mmu_map_phy_addr);
		dec->frame_mmu_map_addr = NULL;
	}
#endif
	if (dec->gvs)
		vfree(dec->gvs);
	dec->gvs = NULL;
}

static int avs2_local_init(struct AVS2Decoder_s *dec)
{
	int ret = -1;
	/*int losless_comp_header_size, losless_comp_body_size;*/

	struct BuffInfo_s *cur_buf_info = NULL;

	cur_buf_info = &dec->work_space_buf_store;
	if (force_bufspec) {
		memcpy(cur_buf_info, &amvavs2_workbuff_spec[force_bufspec & 0xf],
		sizeof(struct BuffInfo_s));
		pr_info("force buffer spec %d\n", force_bufspec & 0xf);
	} else {
		if (get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_TM2 && !is_cpu_tm2_revb()) {
			if (vdec_is_support_4k()) {
				if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
					memcpy(cur_buf_info, &amvavs2_workbuff_spec[2],	/* 8k */
					sizeof(struct BuffInfo_s));
				else
					memcpy(cur_buf_info, &amvavs2_workbuff_spec[1],	/* 4k */
					sizeof(struct BuffInfo_s));
			} else
				memcpy(cur_buf_info, &amvavs2_workbuff_spec[0],/* 1080p */
				sizeof(struct BuffInfo_s));
		} else { //get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_TM2 || is_cpu_tm2_revb()
			if (vdec_is_support_4k()) {
				memcpy(cur_buf_info, &amvavs2_workbuff_spec[5],	/* 8k */
				sizeof(struct BuffInfo_s));
			} else
				memcpy(cur_buf_info, &amvavs2_workbuff_spec[3],/* 1080p */
				sizeof(struct BuffInfo_s));
		}
	}

	cur_buf_info->start_adr = dec->buf_start;
#ifndef AVS2_10B_MMU
	dec->mc_buf_spec.buf_end = dec->buf_start + dec->buf_size;
#endif

	init_buff_spec(dec, cur_buf_info);

	init_avs2_decoder(&dec->avs2_dec);
	dec->pic_list_init_flag = 0;

#ifdef AVS2_10B_MMU
	avs2_bufmgr_init(dec, cur_buf_info, NULL);
#else
	dec->mc_buf_spec.buf_start = (cur_buf_info->end_adr + 0xffff)
	    & (~0xffff);
	dec->mc_buf_spec.buf_size = (dec->mc_buf_spec.buf_end
	    - dec->mc_buf_spec.buf_start);
	if (debug) {
		pr_err("dec->mc_buf_spec.buf_start %x-%x\n",
			dec->mc_buf_spec.buf_start,
			dec->mc_buf_spec.buf_start +
			dec->mc_buf_spec.buf_size);
	}
	avs2_bufmgr_init(dec, cur_buf_info, &dec->mc_buf_spec);
#endif

	if (!vdec_is_support_4k()
		&& (buf_alloc_width > 1920 &&  buf_alloc_height > 1088)) {
		buf_alloc_width = 1920;
		buf_alloc_height = 1088;
	} else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1) {
		buf_alloc_width = 8192;
		buf_alloc_height = 4608;
	}
	dec->init_pic_w = buf_alloc_width ? buf_alloc_width :
		(dec->vavs2_amstream_dec_info.width ?
		dec->vavs2_amstream_dec_info.width :
		dec->work_space_buf->max_width);
	dec->init_pic_h = buf_alloc_height ? buf_alloc_height :
		(dec->vavs2_amstream_dec_info.height ?
		dec->vavs2_amstream_dec_info.height :
		dec->work_space_buf->max_height);

	pts_unstable = ((unsigned long)(dec->vavs2_amstream_dec_info.param)
			& 0x40) >> 6;

	if ((debug & AVS2_DBG_SEND_PARAM_WITH_REG) == 0) {
		dec->rpm_addr = dma_alloc_coherent(amports_get_dma_device(),
			RPM_BUF_SIZE,
			&dec->rpm_phy_addr, GFP_KERNEL);
		if (dec->rpm_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}
		avs2_print(dec, AVS2_DBG_BUFMGR,
			"rpm_phy_addr %x\n", (u32) dec->rpm_phy_addr);
		dec->rpm_ptr = dec->rpm_addr;
	}

	if (cuva_buf_size > 0) {
		dec->cuva_size = AUX_BUF_ALIGN(cuva_buf_size);

		dec->cuva_addr = dma_alloc_coherent(amports_get_dma_device(),
				dec->cuva_size, &dec->cuva_phy_addr, GFP_KERNEL);
	        avs2_print(dec, AVS2_DBG_BUFMGR,
			"%s, cuva_size = %d cuva_phy_addr %x dec->cuva_addr = %px\n",
			__func__, dec->cuva_size, (u32)dec->cuva_phy_addr, dec->cuva_addr);
		if (dec->cuva_addr == NULL) {
			pr_err("%s: failed to alloc cuva buffer\n", __func__);
			return -1;
		}
	}

	dec->lmem_addr = dma_alloc_coherent(amports_get_dma_device(),
			LMEM_BUF_SIZE,
			&dec->lmem_phy_addr, GFP_KERNEL);
	if (dec->lmem_addr == NULL) {
		pr_err("%s: failed to alloc lmem buffer\n", __func__);
		return -1;
	} else
		avs2_print(dec, AVS2_DBG_BUFMGR,
			"%s, lmem_phy_addr %x\n",
			__func__, (u32)dec->lmem_phy_addr);

	dec->lmem_ptr = dec->lmem_addr;

#ifdef AVS2_10B_MMU
	dec->frame_mmu_map_addr = dma_alloc_coherent(amports_get_dma_device(),
				get_frame_mmu_map_size(dec),
				&dec->frame_mmu_map_phy_addr, GFP_KERNEL);
	if (dec->frame_mmu_map_addr == NULL) {
		pr_err("%s: failed to alloc count_buffer\n", __func__);
		return -1;
	}
	memset(dec->frame_mmu_map_addr, 0, get_frame_mmu_map_size(dec));
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


static void set_canvas(struct AVS2Decoder_s *dec,
	struct avs2_frame_s *pic)
{
	int canvas_w = ALIGN(pic->pic_w, 64)/4;
	int canvas_h = ALIGN(pic->pic_h, 32)/4;
	int blkmode = mem_map_mode;
	struct vdec_s *vdec = hw_to_vdec(dec);
	/*CANVAS_BLKMODE_64X32*/
	if	(pic->double_write_mode) {
		canvas_w = pic->pic_w	/
				get_double_write_ratio(pic->double_write_mode);
		canvas_h = pic->pic_h /
				get_double_write_ratio(pic->double_write_mode);
		/*sao_crtl1 aligned with 64*/
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
		config_cav_lut_ex(pic->uv_canvas_index,
			pic->dw_u_v_adr,	canvas_w, canvas_h,
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
#endif
	} else {
	#ifndef AVS2_10B_MMU
		if (vdec->parallel_dec == 1) {
			if (pic->y_canvas_index == -1)
				pic->y_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
			if (pic->uv_canvas_index == -1)
				pic->uv_canvas_index = vdec->get_canvas_ex(CORE_MASK_HEVC, vdec->id);
		} else {
			pic->y_canvas_index = 128 + pic->index;
			pic->uv_canvas_index = 128 + pic->index;
		}

		config_cav_lut_ex(pic->y_canvas_index,
			pic->mc_y_adr, canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0x7, VDEC_HEVC);
		config_cav_lut_ex(pic->uv_canvas_index,
		pic->mc_u_v_adr,	canvas_w, canvas_h,
			CANVAS_ADDR_NOWRAP, blkmode, 0x7, VDEC_HEVC);
	#endif
	}
}

static void set_frame_info(struct AVS2Decoder_s *dec, struct vframe_s *vf)
{
	unsigned int ar;

	vf->duration = dec->frame_dur;
	vf->duration_pulldown = 0;
	vf->flag = 0;
	vf->prop.master_display_colour = dec->vf_dp;
	if (dec->hdr_flag & HDR_CUVA_MASK)
		dec->video_signal_type |= 1 << 31;
	vf->signal_type = dec->video_signal_type;

	avs2_print(dec, AVS2_DBG_HDR_INFO,
			"signal_typesignal_type 0x%x \n",
			vf->signal_type);

	ar = min_t(u32, dec->frame_ar, DISP_RATIO_ASPECT_RATIO_MAX);
	vf->ratio_control = (ar << DISP_RATIO_ASPECT_RATIO_BIT);

	if (dec->vf_dp.present_flag) {
		struct aml_vdec_hdr_infos hdr;
		struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(dec->v4l2_ctx);

		memset(&hdr, 0, sizeof(hdr));
		hdr.signal_type = vf->signal_type;
		hdr.color_parms = dec->vf_dp;
		vdec_v4l_set_hdr_infos(ctx, &hdr);
	}

	vf->sidebind_type = dec->sidebind_type;
	vf->sidebind_channel_id = dec->sidebind_channel_id;

	return;
}

static int vavs2_vf_states(struct vframe_states *states, void *op_arg)
{
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)op_arg;

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&dec->newframe_q);
	states->buf_avail_num = kfifo_len(&dec->display_q);

	if (step == 2)
		states->buf_avail_num = 0;
	return 0;
}

static struct vframe_s *vavs2_vf_peek(void *op_arg)
{
	struct vframe_s *vf;
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)op_arg;
	if (step == 2)
		return NULL;

	if (force_disp_pic_index & 0x100) {
		if (force_disp_pic_index & 0x200)
			return NULL;
		return &dec->vframe_dummy;
	}

	if (kfifo_peek(&dec->display_q, &vf))
		return vf;

	return NULL;
}

static struct avs2_frame_s *get_pic_by_index(
	struct AVS2Decoder_s *dec, int index)
{
	int i;
	struct avs2_frame_s *pic = NULL;
	if (index == (dec->used_buf_num - 1))
		pic = dec->avs2_dec.m_bg;
	else if (index >= 0	&& index < dec->used_buf_num) {
		for (i = 0; i < dec->used_buf_num; i++) {
			if (dec->avs2_dec.fref[i]->index == index)
				pic = dec->avs2_dec.fref[i];
		}
	}
	return pic;
}

static struct vframe_s *vavs2_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)op_arg;
	if (step == 2)
		return NULL;
	else if (step == 1)
			step = 2;

	if (force_disp_pic_index & 0x100) {
		int idx = force_disp_pic_index & 0xff;
		struct avs2_frame_s *pic = NULL;
		if (idx >= 0
			&& idx < dec->avs2_dec.ref_maxbuffer)
			pic = get_pic_by_index(dec, idx);
		if (pic == NULL)
			return NULL;
		if (force_disp_pic_index & 0x200)
			return NULL;

		vf = &dec->vframe_dummy;
		set_vframe(dec, vf, pic, 1);

		force_disp_pic_index |= 0x200;
		return vf;
	}

	if (kfifo_get(&dec->display_q, &vf)) {
		uint8_t index = vf->index & 0xff;
		ATRACE_COUNTER(dec->disp_q_name, kfifo_len(&dec->display_q));
		if (index < dec->used_buf_num ||
			(vf->type & VIDTYPE_V4L_EOS)) {
			struct avs2_frame_s *pic = get_pic_by_index(dec, index);
			if (pic == NULL &&
				(debug & AVS2_DBG_PIC_LEAK)) {
				int i;
				avs2_print(dec, 0,
					"%s error index 0x%x pic not exist\n",
					__func__, index);
				dump_pic_list(dec);
				for (i = 0; i < 10; i++) {
					pic = get_pic_by_index(dec, index);
					pr_info("pic = %p\n", pic);
				}

			if (debug & AVS2_DBG_PIC_LEAK)
				debug |= AVS2_DBG_PIC_LEAK_WAIT;
			return NULL;
		}
		vf->index_disp = atomic_read(&dec->vf_get_count);
		atomic_add(1, &dec->vf_get_count);
		if (pic)
			avs2_print(dec, AVS2_DBG_BUFMGR,
				"%s index 0x%x pos %d getcount %d type 0x%x w/h %d/%d, pts %d, %lld\n",
				__func__, index,
				pic->imgtr_fwRefDistance_bak,
				dec->vf_get_count,
				vf->type,
				vf->width, vf->height,
				vf->pts,
				vf->pts_us64);
		return vf;
		}
	}
	return NULL;
}

static void vavs2_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)op_arg;
	uint8_t index;

	if (vf == (&dec->vframe_dummy))
		return;

	if (!vf)
		return;

	index = vf->index & 0xff;

	kfifo_put(&dec->newframe_q, (const struct vframe_s *)vf);
	ATRACE_COUNTER(dec->new_q_name, kfifo_len(&dec->newframe_q));
	atomic_add(1, &dec->vf_put_count);
	avs2_print(dec, AVS2_DBG_BUFMGR,
		"%s index putcount 0x%x %d\n",
		__func__, vf->index,
		dec->vf_put_count);

	if (index < dec->used_buf_num) {
		unsigned long flags;
		struct avs2_frame_s *pic;
		lock_buffer(dec, flags);
		pic = get_pic_by_index(dec, index);
		if (pic && vf->v4l_mem_handle !=
			dec->m_BUF[pic->BUF_index].v4l_ref_buf_addr) {
			avs2_print(dec, PRINT_FLAG_V4L_DETAIL,
				"AVS2 update fb handle, old:%llx, new:%llx\n",
				dec->m_BUF[pic->BUF_index].v4l_ref_buf_addr,
				vf->v4l_mem_handle);

			dec->m_BUF[pic->BUF_index].v4l_ref_buf_addr =
				vf->v4l_mem_handle;
		}
		if (pic && pic->vf_ref > 0)
			pic->vf_ref--;
		else {
			if (pic)
				avs2_print(dec, 0,
					"%s, error pic (index %d) vf_ref is %d\n",
					__func__, index, pic->vf_ref);
			else
				avs2_print(dec, 0,
					"%s, error pic (index %d) is NULL\n",
					__func__, index);
		}
		if (dec->wait_buf)
			WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
		dec->last_put_idx = index;
		dec->new_frame_displayed++;
		unlock_buffer(dec, flags);
	}

}

static int vavs2_event_cb(int type, void *data, void *private_data)
{
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)private_data;

	if (type & VFRAME_EVENT_RECEIVER_REQ_STATE) {
		struct provider_state_req_s *req =
			(struct provider_state_req_s *)data;
		if (req->req_type == REQ_STATE_SECURE)
			req->req_result[0] = vdec_secure(hw_to_vdec(dec));
		else
			req->req_result[0] = 0xffffffff;
	} else if (type & VFRAME_EVENT_RECEIVER_GET_AUX_DATA) {
		struct provider_aux_req_s *req = (struct provider_aux_req_s *)data;
		unsigned char index;
		unsigned long flags;
		struct avs2_frame_s *pic;

		if (!req->vf) {
			req->aux_size = atomic_read(&dec->vf_put_count);
			return 0;
		}
		lock_buffer(dec, flags);
		index = req->vf->index & 0xff;
		req->aux_buf = NULL;
		req->aux_size = 0;
		req->format = VFORMAT_AVS2;
		if (index < dec->used_buf_num) {
			pic = get_pic_by_index(dec, index);
			req->aux_buf = pic->cuva_data_buf;
			req->aux_size = pic->cuva_data_size;
		}
		unlock_buffer(dec, flags);

		avs2_print(dec, PRINT_FLAG_VDEC_STATUS,
			"%s pic 0x%p index %d =>size %d\n",
			__func__, pic, index, req->aux_size);
	}

	return 0;
}

static struct avs2_frame_s *get_disp_pic(struct AVS2Decoder_s *dec)
{
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct avs2_frame_s *pic = NULL;
	int32_t j;
	int32_t pre_disp_count_min = 0x7fffffff;
	for (j = 0; j < avs2_dec->ref_maxbuffer; j++) {
		if (avs2_dec->fref[j]->to_prepare_disp &&
			avs2_dec->fref[j]->to_prepare_disp <
			pre_disp_count_min) {
			pre_disp_count_min =
				avs2_dec->fref[j]->to_prepare_disp;
			pic = avs2_dec->fref[j];
		}
	}
	if (pic)
		pic->to_prepare_disp = 0;

	return pic;

}



static void fill_frame_info(struct AVS2Decoder_s *dec,
	struct avs2_frame_s *pic, unsigned int framesize, unsigned int pts)
{
	struct vframe_qos_s *vframe_qos = &dec->vframe_qos;

	if (pic->slice_type == I_IMG)
		vframe_qos->type = 1;
	else if (pic->slice_type == P_IMG)
		vframe_qos->type = 2;
	else if (pic->slice_type == B_IMG)
		vframe_qos->type = 3;

	if (input_frame_based(hw_to_vdec(dec)))
		vframe_qos->size = pic->frame_size;
	else
		vframe_qos->size = framesize;
	vframe_qos->pts = pts;
#ifdef SHOW_QOS_INFO
	avs2_print(dec, 0, "slice:%d\n", pic->slice_type);
#endif


	vframe_qos->max_mv = pic->max_mv;
	vframe_qos->avg_mv = pic->avg_mv;
	vframe_qos->min_mv = pic->min_mv;
#ifdef SHOW_QOS_INFO
	avs2_print(dec, 0, "mv: max:%d,  avg:%d, min:%d\n",
		vframe_qos->max_mv,
		vframe_qos->avg_mv,
		vframe_qos->min_mv);
#endif

	vframe_qos->max_qp = pic->max_qp;
	vframe_qos->avg_qp = pic->avg_qp;
	vframe_qos->min_qp = pic->min_qp;
#ifdef SHOW_QOS_INFO
	avs2_print(dec, 0, "qp: max:%d,  avg:%d, min:%d\n",
		vframe_qos->max_qp,
		vframe_qos->avg_qp,
		vframe_qos->min_qp);
#endif

	vframe_qos->max_skip = pic->max_skip;
	vframe_qos->avg_skip = pic->avg_skip;
	vframe_qos->min_skip = pic->min_skip;
#ifdef SHOW_QOS_INFO
	avs2_print(dec, 0, "skip: max:%d,	avg:%d, min:%d\n",
		vframe_qos->max_skip,
		vframe_qos->avg_skip,
		vframe_qos->min_skip);
#endif

	vframe_qos->num++;
}

static void set_vframe(struct AVS2Decoder_s *dec,
	struct vframe_s *vf, struct avs2_frame_s *pic, u8 dummy)
{
	unsigned long flags;
	int stream_offset;
	unsigned int frame_size = 0;
	int pts_discontinue;
	struct vdec_s *vdec = hw_to_vdec(dec);
	struct aml_vcodec_ctx * v4l2_ctx = dec->v4l2_ctx;
	ulong nv_order = VIDTYPE_VIU_NV21;
	stream_offset = pic->stream_offset;
	avs2_print(dec, AVS2_DBG_BUFMGR,
		"%s index = %d pos = %d\r\n",
		__func__, pic->index,
		pic->imgtr_fwRefDistance);
	if (pic->double_write_mode)
		set_canvas(dec, pic);

	display_frame_count[dec->index]++;

	if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12) ||
		(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12M))
		nv_order = VIDTYPE_VIU_NV12;

	if (!dummy) {
		vf->v4l_mem_handle = dec->m_BUF[pic->BUF_index].v4l_ref_buf_addr;

#ifdef MULTI_INSTANCE_SUPPORT
		if (vdec_frame_based(vdec)) {
			vf->pts = pic->pts;
			vf->pts_us64 = pic->pts64;

			if (v4l_bitstream_id_enable)
					vf->timestamp = pic->timestamp;
				else
					vf->timestamp = pic->pts64;
		} else {
#endif
			if ((vdec->vbuf.no_parser == 0) || (vdec->vbuf.use_ptsserv)) {
				if (pts_lookup_offset_us64
					(PTS_TYPE_VIDEO, stream_offset,
					&vf->pts, &frame_size, 0,
					&vf->pts_us64) != 0) {
#ifdef DEBUG_PTS
					dec->pts_missed++;
#endif
					vf->pts = 0;
					vf->pts_us64 = 0;
				}
			}
		}
#ifdef DEBUG_PTS
		else
			dec->pts_hit++;
#endif
		if (pts_unstable)
			dec->pts_mode = PTS_NONE_REF_USE_DURATION;

		fill_frame_info(dec, pic, frame_size, vf->pts);

		if ((dec->pts_mode == PTS_NORMAL) && (vf->pts != 0)
			&& dec->get_frame_dur) {
			int pts_diff = (int)vf->pts - dec->last_lookup_pts;

			if (pts_diff < 0) {
				dec->pts_mode_switching_count++;
				dec->pts_mode_recovery_count = 0;

				if (dec->pts_mode_switching_count >=
					PTS_MODE_SWITCHING_THRESHOLD) {
					dec->pts_mode =
						PTS_NONE_REF_USE_DURATION;
					pr_info
					("HEVC: switch to n_d mode.\n");
				}

			} else {
				int p = PTS_MODE_SWITCHING_RECOVERY_THREASHOLD;
				dec->pts_mode_recovery_count++;
				if (dec->pts_mode_recovery_count > p) {
					dec->pts_mode_switching_count = 0;
					dec->pts_mode_recovery_count = 0;
				}
			}
		}

		pts_discontinue =
			(abs(dec->last_pts  - vf->pts) >=
			 tsync_vpts_discontinuity_margin());

		if (vf->pts != 0)
			dec->last_lookup_pts = vf->pts;

		if ((dec->pts_mode == PTS_NONE_REF_USE_DURATION)
			&& ((pic->slice_type != I_IMG) || (!pts_discontinue &&
			!first_pts_checkin_complete(PTS_TYPE_AUDIO))))
			vf->pts = dec->last_pts + DUR2PTS(dec->frame_dur);

		dec->last_pts = vf->pts;

		if (vf->pts_us64 != 0)
			dec->last_lookup_pts_us64 = vf->pts_us64;

		if ((dec->pts_mode == PTS_NONE_REF_USE_DURATION)
			&& ((pic->slice_type != I_IMG) || (!pts_discontinue &&
			!first_pts_checkin_complete(PTS_TYPE_AUDIO)))) {
			vf->pts_us64 = dec->last_pts_us64 +
				(DUR2PTS(dec->frame_dur) * 100 / 9);
		}

		dec->last_pts_us64 = vf->pts_us64;
		avs2_print(dec, AVS2_DBG_OUT_PTS,
			"avs2 dec out pts: vf->pts=%d, vf->pts_us64 = %lld\n",
			vf->pts, vf->pts_us64);
	}

	vf->index = 0xff00 | pic->index;

	if (pic->double_write_mode & 0x10) {
		/* double write only */
		vf->compBodyAddr = 0;
		vf->compHeadAddr = 0;
	} else {
#ifdef AVS2_10B_MMU
	vf->compBodyAddr = 0;
	vf->compHeadAddr = pic->header_adr;
#else
	vf->compBodyAddr = pic->mc_y_adr; /*body adr*/
	vf->compHeadAddr = pic->mc_y_adr +
				pic->comp_body_size;
	/*head adr*/
#endif
	}
	if (pic->double_write_mode) {
		vf->type = VIDTYPE_PROGRESSIVE |
			VIDTYPE_VIU_FIELD;
		vf->type |= nv_order;
		if (pic->double_write_mode == 3) {
			vf->type |= VIDTYPE_COMPRESS;
#ifdef AVS2_10B_MMU
			vf->type |= VIDTYPE_SCATTER;
#endif
		}
#ifdef MULTI_INSTANCE_SUPPORT
		if (dec->m_ins_flag) {
				vf->canvas0Addr = vf->canvas1Addr = -1;
				vf->plane_num = 2;
				vf->canvas0_config[0] = pic->canvas_config[0];
				vf->canvas0_config[1] = pic->canvas_config[1];
				vf->canvas1_config[0] = pic->canvas_config[0];
				vf->canvas1_config[1] = pic->canvas_config[1];
		} else
#endif
			vf->canvas0Addr = vf->canvas1Addr =
				spec2canvas(pic);
	} else {
		vf->canvas0Addr = vf->canvas1Addr = 0;
		vf->type = VIDTYPE_COMPRESS | VIDTYPE_VIU_FIELD;
#ifdef AVS2_10B_MMU
		vf->type |= VIDTYPE_SCATTER;
#endif
	}

	switch (pic->bit_depth) {
	case AVS2_BITS_8:
		vf->bitdepth = BITDEPTH_Y8 |
			BITDEPTH_U8 | BITDEPTH_V8;
		break;
	case AVS2_BITS_10:
	case AVS2_BITS_12:
		vf->bitdepth = BITDEPTH_Y10 |
			BITDEPTH_U10 | BITDEPTH_V10;
		break;
	default:
		vf->bitdepth = BITDEPTH_Y10 |
			BITDEPTH_U10 | BITDEPTH_V10;
		break;
	}
	if ((vf->type & VIDTYPE_COMPRESS) == 0)
		vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
	if (pic->bit_depth == AVS2_BITS_8)
		vf->bitdepth |= BITDEPTH_SAVING_MODE;

	set_frame_info(dec, vf);

	vf->width = pic->pic_w /
		get_double_write_ratio(pic->double_write_mode);
	vf->height = pic->pic_h /
		get_double_write_ratio(pic->double_write_mode);
	if (force_w_h != 0) {
		vf->width = (force_w_h >> 16) & 0xffff;
		vf->height = force_w_h & 0xffff;
	}
	vf->compWidth = pic->pic_w;
	vf->compHeight = pic->pic_h;
	if (force_fps & 0x100) {
		u32 rate = force_fps & 0xff;
		if (rate)
			vf->duration = 96000/rate;
		else
			vf->duration = 0;
	}

	switch (pic->bit_depth) {
	case AVS2_BITS_8:
		vf->bitdepth = BITDEPTH_Y8 |
			BITDEPTH_U8 | BITDEPTH_V8;
		break;
	case AVS2_BITS_10:
	case AVS2_BITS_12:
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
	if (pic->bit_depth == AVS2_BITS_8)
		vf->bitdepth |= BITDEPTH_SAVING_MODE;

	set_frame_info(dec, vf);

	if (dec->high_bandwidth_flag) {
		vf->flag |= VFRAME_FLAG_HIGH_BANDWIDTH;
	}

	vf->width = pic->pic_w /
		get_double_write_ratio(pic->double_write_mode);
	vf->height = pic->pic_h /
		get_double_write_ratio(pic->double_write_mode);
	if (force_w_h != 0) {
		vf->width = (force_w_h >> 16) & 0xffff;
		vf->height = force_w_h & 0xffff;
	}
	vf->compWidth = pic->pic_w;
	vf->compHeight = pic->pic_h;
	if (force_fps & 0x100) {
		u32 rate = force_fps & 0xff;
		if (rate)
			vf->duration = 96000/rate;
		else
			vf->duration = 0;
	}

	if (!vdec->vbuf.use_ptsserv && vdec_stream_based(vdec)) {
		vf->pts_us64 = stream_offset;
		vf->pts = 0;
	}

	if (!dummy) {
		lock_buffer(dec, flags);
		pic->vf_ref = 1;
		unlock_buffer(dec, flags);
	}
	atomic_add(1, &dec->vf_pre_count);
}

static inline void dec_update_gvs(struct AVS2Decoder_s *dec)
{
	if (dec->gvs->frame_height != dec->frame_height) {
		dec->gvs->frame_width = dec->frame_width;
		dec->gvs->frame_height = dec->frame_height;
	}
	if (dec->gvs->frame_dur != dec->frame_dur) {
		dec->gvs->frame_dur = dec->frame_dur;
		if (dec->frame_dur != 0)
			dec->gvs->frame_rate = ((96000 * 10 / dec->frame_dur) % 10) < 5 ?
					96000 / dec->frame_dur : (96000 / dec->frame_dur +1);
		else
			dec->gvs->frame_rate = -1;
	}
	dec->gvs->status = dec->stat | dec->fatal_error;
}

static int avs2_prepare_display_buf(struct AVS2Decoder_s *dec)
{
	struct vframe_s *vf = NULL;
	/*unsigned short slice_type;*/
	struct avs2_frame_s *pic;
	struct vdec_s *pvdec = hw_to_vdec(dec);
	struct aml_vcodec_ctx * v4l2_ctx = dec->v4l2_ctx;
	struct vdec_v4l2_buffer *fb = NULL;
	while (1) {
		pic = get_disp_pic(dec);
		if (pic == NULL) {
			break;
		}

		if (force_disp_pic_index & 0x100) {
			/*recycle directly*/
			continue;
		}

		if (pic->error_mark) {
			avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
				"!!!error pic, skip\n",
				0);
			v4l2_ctx->decoder_status_info.decoder_error_count++;
			vdec_v4l_post_error_event(v4l2_ctx, DECODER_WARNING_DATA_ERROR);
			continue;
		}

		if (dec->start_decoding_flag != 0) {
			if (dec->skip_PB_before_I &&
				pic->slice_type != I_IMG) {
				avs2_print(dec, AVS2_DBG_BUFMGR_DETAIL,
					"!!!slice type %d (not I) skip\n",
					0, pic->slice_type);
				continue;
			}
			dec->skip_PB_before_I = 0;
		}

		if (kfifo_get(&dec->newframe_q, &vf) == 0) {
			pr_info("fatal error, no available buffer slot.");
			return -1;
		}

		if (vf) {
			struct vdec_info tmp4x;
			int stream_offset = pic->stream_offset;

			vf->v4l_mem_handle = dec->m_BUF[pic->BUF_index].v4l_ref_buf_addr;
			fb = (struct vdec_v4l2_buffer *)vf->v4l_mem_handle;

			set_vframe(dec, vf, pic, 0);
			decoder_do_frame_check(pvdec, vf);
			vdec_vframe_ready(pvdec, vf);
			kfifo_put(&dec->display_q, (const struct vframe_s *)vf);
			ATRACE_COUNTER(dec->pts_name, vf->timestamp);
			ATRACE_COUNTER(dec->new_q_name, kfifo_len(&dec->newframe_q));
			ATRACE_COUNTER(dec->disp_q_name, kfifo_len(&dec->display_q));

			dec_update_gvs(dec);
			/*count info*/
			vdec_count_info(dec->gvs, 0, stream_offset);
		if (stream_offset) {
			if (pic->slice_type == I_IMG) {
				dec->gvs->i_decoded_frames++;
			} else if (pic->slice_type == P_IMG) {
				dec->gvs->p_decoded_frames++;
			} else if (pic->slice_type == B_IMG) {
				dec->gvs->b_decoded_frames++;
			}
		}
			memcpy(&tmp4x, dec->gvs, sizeof(struct vdec_info));
			tmp4x.bit_depth_luma = bit_depth_luma;
			tmp4x.bit_depth_chroma = bit_depth_chroma;
			tmp4x.double_write_mode = pic->double_write_mode;
			vdec_fill_vdec_frame(pvdec, &dec->vframe_qos, &tmp4x, vf, pic->hw_decode_time);
			pvdec->vdec_fps_detec(pvdec->id);
			if (without_display_mode == 0) {
				if (v4l2_ctx->is_stream_off) {
					vavs2_vf_put(vavs2_vf_get(dec), dec);
				} else {
					fb->task->submit(fb->task, TASK_TYPE_DEC);
				}
			} else
				vavs2_vf_put(vavs2_vf_get(dec), dec);
		}
	}
	return 0;
}

static bool is_avaliable_buffer(struct AVS2Decoder_s *dec);

static int notify_v4l_eos(struct vdec_s *vdec)
{
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(dec->v4l2_ctx);
	struct vframe_s *vf = &dec->vframe_dummy;
	struct vdec_v4l2_buffer *fb = NULL;
	int index = INVALID_IDX;
	int out_buff_index = INVALID_IDX;
	ulong expires;

	if (dec->eos) {
		expires = jiffies + msecs_to_jiffies(2000);
		while (!is_avaliable_buffer(dec)) {
			if (time_after(jiffies, expires)) {
				pr_err("[%d] AVS2 isn't enough buff for notify eos.\n", ctx->id);
				return 0;
			}
		}

		index = v4l_get_free_fb(dec);
		if (INVALID_IDX == index) {
			pr_err("[%d] AVS2 EOS get free buff fail.\n", ctx->id);
			return 0;
		}

		out_buff_index = dec->avs2_dec.fref[index]->index;
		if (INVALID_IDX == out_buff_index) {
			pr_err("[%d] AVS2 EOS get free buff fail. out_buff_index = INVALID_IDX\n", ctx->id);
			return 0;
		}

		fb = (struct vdec_v4l2_buffer *)
			dec->m_BUF[out_buff_index].v4l_ref_buf_addr;

		vf->type		|= VIDTYPE_V4L_EOS;
		vf->timestamp		= ULONG_MAX;
		vf->flag		= VFRAME_FLAG_EMPTY_FRAME_V4L;
		vf->v4l_mem_handle	= (ulong)fb;

		vdec_vframe_ready(vdec, vf);
		kfifo_put(&dec->display_q, (const struct vframe_s *)vf);

		fb->task->submit(fb->task, TASK_TYPE_DEC);

		pr_info("[%d] AVS2 EOS notify.\n", ctx->id);
	}

	return 0;
}

static void get_rpm_param(union param_u *params)
{
	int i;
	unsigned int data32;
	if (debug & AVS2_DBG_BUFMGR)
		pr_info("enter %s\r\n", __func__);
	for (i = 0; i < (RPM_END - RPM_BEGIN); i++) {
		do {
			data32 = READ_VREG(RPM_CMD_REG);
		} while ((data32 & 0x10000) == 0);
		params->l.data[i] = data32&0xffff;
		WRITE_VREG(RPM_CMD_REG, 0);
	}
	if (debug & AVS2_DBG_BUFMGR)
		pr_info("leave %s\r\n", __func__);
}
static void debug_buffer_mgr_more(struct AVS2Decoder_s *dec)
{
	int i;
	if (!(debug & AVS2_DBG_BUFMGR_MORE))
		return;
	pr_info("avs2_param: (%d)\n", dec->avs2_dec.img.number);
	for (i = 0; i < (RPM_END-RPM_BEGIN); i++) {
		pr_info("%04x ", dec->avs2_dec.param.l.data[i]);
		if (((i + 1) & 0xf) == 0)
			pr_info("\n");
	}
}

#ifdef AVS2_10B_MMU
static void avs2_recycle_mmu_buf_tail(struct AVS2Decoder_s *dec)
{
	int index = dec->cur_fb_idx_mmu;
	struct internal_comp_buf *ibuf = index_to_icomp_buf(dec, index);

	if (dec->cur_fb_idx_mmu != INVALID_IDX) {
		if (dec->used_4k_num == -1) {
			dec->used_4k_num = (READ_VREG(HEVC_SAO_MMU_STATUS) >> 16);

			avs2_print(dec, AVS2_DBG_BUFMGR_MORE, "pic index %d page_start %d\n",
				dec->cur_fb_idx_mmu, dec->used_4k_num);

			if (dec->m_ins_flag)
				hevc_mmu_dma_check(hw_to_vdec(dec));

			decoder_mmu_box_free_idx_tail(
				ibuf->mmu_box,
				ibuf->index,
				dec->used_4k_num);
		}
		dec->cur_fb_idx_mmu = INVALID_IDX;
		dec->used_4k_num = -1;
	}
}
#endif

static void dec_again_process(struct AVS2Decoder_s *dec)
{
	amhevc_stop();
	dec->dec_result = DEC_RESULT_AGAIN;
	if (dec->process_state == PROC_STATE_DECODING) {
		dec->process_state = PROC_STATE_DECODE_AGAIN;
	} else if (dec->process_state == PROC_STATE_HEAD_DONE) {
		dec->process_state = PROC_STATE_HEAD_AGAIN;
	}
	dec->next_again_flag = 1;
	reset_process_time(dec);
	vdec_schedule_work(&dec->work);
}

static uint32_t log2i(uint32_t val)
{
	uint32_t ret = -1;
	while (val != 0) {
		val >>= 1;
		ret++;
	}
	return ret;
}

static void check_pic_error(struct AVS2Decoder_s *dec,
	struct avs2_frame_s *pic)
{
	if (pic->decoded_lcu == 0) {
		pic->decoded_lcu = (READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff) + 1;
	}
	if (pic->decoded_lcu != dec->avs2_dec.lcu_total) {
		avs2_print(dec, AVS2_DBG_BUFMGR,
			"%s error pic(index %d imgtr_fwRefDistance %d) decoded lcu %d (total %d)\n",
			__func__, pic->index, pic->imgtr_fwRefDistance,
			pic->decoded_lcu, dec->avs2_dec.lcu_total);
		pic->error_mark = 1;
	} else {
		avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
			"%s pic(index %d imgtr_fwRefDistance %d) decoded lcu %d (total %d)\n",
			__func__, pic->index, pic->imgtr_fwRefDistance,
			pic->decoded_lcu, dec->avs2_dec.lcu_total);

	}
}
static void update_decoded_pic(struct AVS2Decoder_s *dec)
{
	struct avs2_frame_s *pic = dec->avs2_dec.hc.cur_pic;
	if (pic) {
		dec->avs2_dec.hc.cur_pic->decoded_lcu =
			(READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff) + 1;
		avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
			"%s pic(index %d imgtr_fwRefDistance %d) decoded lcu %d (total %d)\n",
			__func__, pic->index, pic->imgtr_fwRefDistance,
			pic->decoded_lcu, dec->avs2_dec.lcu_total);
	}
}
/* +[SE] [BUG][BUG-171463][chuanqi.wang]: get frame rate by video sequeue*/
static int get_frame_rate(union param_u *params, struct AVS2Decoder_s *dec)
{
	int tmp = 0;

	switch (params->p.frame_rate_code) {
	case 1:
	case 2:
		tmp = 24;
		break;
	case 3:
		tmp =  25;
		break;
	case 4:
	case 5:
		tmp =  30;
		break;
	case 6:
		tmp =  50;
		break;
	case 7:
	case 8:
		tmp =  60;
		break;
	case 9:
		tmp =  100;
		break;
	case 10:
		tmp = 120;
		break;
	default:
		tmp =  25;
		break;
	}

	if (!params->p.progressive_sequence)
		tmp = tmp / 2;
	dec->frame_dur = div_u64(96000ULL, tmp);
	dec->get_frame_dur = true;

	return 0;
}


#define HEVC_MV_INFO   0x310d
#define HEVC_QP_INFO   0x3137
#define HEVC_SKIP_INFO 0x3136

/* only when we decoded one field or one frame,
we can call this function to get qos info*/
static void get_picture_qos_info(struct AVS2Decoder_s *dec)
{
	struct avs2_frame_s *picture = dec->avs2_dec.hc.cur_pic;
	struct vdec_s *vdec = hw_to_vdec(dec);
	if (!picture) {
		avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
			"%s decode picture is none exist\n");

		return;
	}
	if (vdec->mvfrm) {
		picture->frame_size = vdec->mvfrm->frame_size;
		picture->hw_decode_time =
		local_clock() - vdec->mvfrm->hw_decode_start;
	}

	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_G12A) {
		unsigned char a[3];
		unsigned char i, j, t;
		unsigned long  data;

		data = READ_VREG(HEVC_MV_INFO);
		if (picture->slice_type == I_IMG)
			data = 0;
		a[0] = data & 0xff;
		a[1] = (data >> 8) & 0xff;
		a[2] = (data >> 16) & 0xff;

		for (i = 0; i < 3; i++)
			for (j = i + 1; j < 3; j++) {
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
#ifdef DEBUG_QOS
		avs2_print(dec, 0, "mv data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data, a[0], a[1], a[2]);
#endif

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
#ifdef DEBUG_QOS
		avs2_print(dec, 0, "qp data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data, a[0], a[1], a[2]);
#endif

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

#ifdef DEBUG_QOS
		avs2_print(dec, 0,
			"skip data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data, a[0], a[1], a[2]);
#endif
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
#ifdef DEBUG_QOS
		int pic_number = 0;
#endif

		picture->max_mv = 0;
		picture->avg_mv = 0;
		picture->min_mv = 0;

		picture->max_skip = 0;
		picture->avg_skip = 0;
		picture->min_skip = 0;

		picture->max_qp = 0;
		picture->avg_qp = 0;
		picture->min_qp = 0;

#ifdef DEBUG_QOS
		avs2_print(dec, 0, "slice_type:%d, poc:%d\n",
			picture->slice_type,
			pic_number);
#endif
		/* set rd_idx to 0 */
	    WRITE_VREG(HEVC_PIC_QUALITY_CTRL, 0);

	    blk88_y_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk88_y_count == 0) {
#ifdef DEBUG_QOS
			avs2_print(dec, 0,
				"[Picture %d Quality] NO Data yet.\n",
				pic_number);
#endif
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* qp_y_sum */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] Y QP AVG : %d (%d/%d)\n",
			pic_number, rdata32/blk88_y_count,
			rdata32, blk88_y_count);
#endif
		picture->avg_qp = rdata32/blk88_y_count;
		/* intra_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] Y intra rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_y_count,
			'%', rdata32);
#endif
		/* skipped_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] Y skipped rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_y_count,
			'%', rdata32);
#endif
		picture->avg_skip = rdata32*100/blk88_y_count;
		/* coeff_non_zero_y_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] Y ZERO_Coeff rate : %d%c (%d)\n",
			pic_number, (100 - rdata32*100/(blk88_y_count*1)),
			'%', rdata32);
#endif
		/* blk66_c_count */
	    blk88_c_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk88_c_count == 0) {
#ifdef DEBUG_QOS
			avs2_print(dec, 0,
				"[Picture %d Quality] NO Data yet.\n",
				pic_number);
#endif
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* qp_c_sum */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] C QP AVG : %d (%d/%d)\n",
			pic_number, rdata32/blk88_c_count,
			rdata32, blk88_c_count);
#endif
		/* intra_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] C intra rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_c_count,
			'%', rdata32);
#endif
		/* skipped_cu_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] C skipped rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_c_count,
			'%', rdata32);
#endif
		/* coeff_non_zero_c_count */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] C ZERO_Coeff rate : %d%c (%d)\n",
			pic_number, (100 - rdata32*100/(blk88_c_count*1)),
			'%', rdata32);
#endif
		/* 1'h0, qp_c_max[6:0], 1'h0, qp_c_min[6:0],
		1'h0, qp_y_max[6:0], 1'h0, qp_y_min[6:0] */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    avs2_print(dec, 0, "[Picture %d Quality] Y QP min : %d\n",
			pic_number, (rdata32>>0)&0xff);
#endif
		picture->min_qp = (rdata32>>0)&0xff;

#ifdef DEBUG_QOS
	    avs2_print(dec, 0, "[Picture %d Quality] Y QP max : %d\n",
			pic_number, (rdata32>>8)&0xff);
#endif
		picture->max_qp = (rdata32>>8)&0xff;

#ifdef DEBUG_QOS
	    avs2_print(dec, 0, "[Picture %d Quality] C QP min : %d\n",
			pic_number, (rdata32>>16)&0xff);
	    avs2_print(dec, 0, "[Picture %d Quality] C QP max : %d\n",
			pic_number, (rdata32>>24)&0xff);
#endif
		/* blk22_mv_count */
	    blk22_mv_count = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    if (blk22_mv_count == 0) {
#ifdef DEBUG_QOS
			avs2_print(dec, 0,
				"[Picture %d Quality] NO MV Data yet.\n",
				pic_number);
#endif
			/* reset all counts */
			WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* mvy_L1_count[39:32], mvx_L1_count[39:32],
		mvy_L0_count[39:32], mvx_L0_count[39:32] */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    /* should all be 0x00 or 0xff */
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] MV AVG High Bits: 0x%X\n",
			pic_number, rdata32);
#endif
	    mvx_L0_hi = ((rdata32 >> 0) & 0xff);
	    mvy_L0_hi = ((rdata32 >> 8) & 0xff);
	    mvx_L1_hi = ((rdata32 >> 16) & 0xff);
	    mvy_L1_hi = ((rdata32 >> 24) & 0xff);

		/* mvx_L0_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvx_L0_hi;
		temp_value = (temp_value << 32) | rdata32_l;

		if (mvx_L0_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
		 value = div_s64(value, blk22_mv_count);
#ifdef DEBUG_QOS
		avs2_print(dec, 0,
			"[Picture %d Quality] MVX_L0 AVG : %d (%lld/%d)\n",
			pic_number, (int)value,
			value, blk22_mv_count);
#endif
		picture->avg_mv = value;

		/* mvy_L0_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvy_L0_hi;
		temp_value = (temp_value << 32) | rdata32_l;

		if (mvy_L0_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] MVY_L0 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);
#endif
		/* mvx_L1_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvx_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvx_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] MVX_L1 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);
#endif
		/* mvy_L1_count[31:0] */
	    rdata32_l = READ_VREG(HEVC_PIC_QUALITY_DATA);
		temp_value = mvy_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvy_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] MVY_L1 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);
#endif
		/* {mvx_L0_max, mvx_L0_min} // format : {sign, abs[14:0]}  */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32 >> 16) & 0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;
#ifdef DEBUG_QOS
	    avs2_print(dec, 0, "[Picture %d Quality] MVX_L0 MAX : %d\n",
			pic_number, mv_hi);
#endif
		picture->max_mv = mv_hi;

	    mv_lo = (rdata32 >> 0) & 0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;
#ifdef DEBUG_QOS
	    avs2_print(dec, 0, "[Picture %d Quality] MVX_L0 MIN : %d\n",
			pic_number, mv_lo);
#endif
		picture->min_mv = mv_lo;

#ifdef DEBUG_QOS
		/* {mvy_L0_max, mvy_L0_min} */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32 >> 16) & 0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;
	    avs2_print(dec, 0, "[Picture %d Quality] MVY_L0 MAX : %d\n",
			pic_number, mv_hi);

	    mv_lo = (rdata32 >> 0) & 0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;

	    avs2_print(dec, 0, "[Picture %d Quality] MVY_L0 MIN : %d\n",
			pic_number, mv_lo);

		/* {mvx_L1_max, mvx_L1_min} */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32 >> 16) & 0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;

	    avs2_print(dec, 0, "[Picture %d Quality] MVX_L1 MAX : %d\n",
			pic_number, mv_hi);

	    mv_lo = (rdata32 >> 0) & 0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;

	    avs2_print(dec, 0, "[Picture %d Quality] MVX_L1 MIN : %d\n",
			pic_number, mv_lo);

		/* {mvy_L1_max, mvy_L1_min} */
	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32 >> 16) & 0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;

	    avs2_print(dec, 0, "[Picture %d Quality] MVY_L1 MAX : %d\n",
			pic_number, mv_hi);

	    mv_lo = (rdata32 >> 0) & 0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;

	    avs2_print(dec, 0, "[Picture %d Quality] MVY_L1 MIN : %d\n",
			pic_number, mv_lo);
#endif

	    rdata32 = READ_VREG(HEVC_PIC_QUALITY_CTRL);
#ifdef DEBUG_QOS
	    avs2_print(dec, 0,
			"[Picture %d Quality] After Read : VDEC_PIC_QUALITY_CTRL : 0x%x\n",
			pic_number, rdata32);
#endif
		/* reset all counts */
	    WRITE_VREG(HEVC_PIC_QUALITY_CTRL, (1<<8));
	}
}

static int avs2_mmu_page_num(struct AVS2Decoder_s *dec,
		int w, int h, int save_mode)
{
	int picture_size;
	int cur_mmu_4k_number, max_frame_num;

	picture_size = compute_losless_comp_body_size(dec, w, h, save_mode);
	cur_mmu_4k_number = ((picture_size + (PAGE_SIZE - 1)) >> PAGE_SHIFT);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
		max_frame_num = MAX_FRAME_8K_NUM;
	else
		max_frame_num = MAX_FRAME_4K_NUM;

	if (cur_mmu_4k_number > max_frame_num) {
		pr_err("over max !! cur_mmu_4k_number 0x%x width %d height %d\n",
			cur_mmu_4k_number, w, h);
		return -1;
	}

	return cur_mmu_4k_number;
}

static int avs2_get_header_size(int w, int h)
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

static void vavs2_get_comp_buf_info(struct AVS2Decoder_s *dec,
					struct vdec_comp_buf_info *info)
{
	u16 bit_depth = dec->avs2_dec.input.sample_bit_depth;
	info->max_size = 48;
	info->header_size = avs2_get_header_size(
		dec->frame_width,
		dec->frame_height);
	info->frame_buffer_size = avs2_mmu_page_num(
		dec, dec->frame_width,
		dec->frame_height,
		bit_depth == 0);
}

static int vavs2_get_ps_info(struct AVS2Decoder_s *dec, struct aml_vdec_ps_infos *ps)
{
	ps->visible_width 	= dec->frame_width;
	ps->visible_height 	= dec->frame_height;
	ps->coded_width 	= ALIGN(dec->frame_width, 64);
	ps->coded_height 	= ALIGN(dec->frame_height, 64);
	ps->dpb_size 		= dec->used_buf_num;
	ps->dpb_margin	= dec->dynamic_buf_margin;
	ps->dpb_frames	= 12;

	if (ps->dpb_margin + ps->dpb_frames > max_buf_num) {
		u32 delta;
		delta = ps->dpb_margin + ps->dpb_frames - max_buf_num;
		ps->dpb_margin -= delta;
		dec->dynamic_buf_margin = ps->dpb_margin;
	}

	ps->field = V4L2_FIELD_NONE;

	return 0;
}

static int v4l_res_change(struct AVS2Decoder_s *dec)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(dec->v4l2_ctx);
	int ret = 0;

	if (ctx->param_sets_from_ucode &&
		dec->res_ch_flag == 0) {
		struct aml_vdec_ps_infos ps;
		struct vdec_comp_buf_info comp;

		if ((dec->last_width != 0 &&
			dec->last_height != 0) &&
			(dec->frame_width != dec->last_width ||
			dec->frame_height != dec->last_height)) {

			avs2_print(dec, 0, "%s (%d,%d)=>(%d,%d)\r\n", __func__, dec->last_width,
				dec->last_height, dec->frame_width, dec->frame_height);

			if (get_valid_double_write_mode(dec) != 16) {
				vavs2_get_comp_buf_info(dec, &comp);
				vdec_v4l_set_comp_buf_info(ctx, &comp);
			}

			vavs2_get_ps_info(dec, &ps);
			vdec_v4l_set_ps_infos(ctx, &ps);
			vdec_v4l_res_ch_event(ctx);

			dec->init_pic_w = dec->frame_width;
			dec->init_pic_h = dec->frame_height;
			dec->v4l_params_parsed = false;
			dec->res_ch_flag = 1;
			ctx->v4l_resolution_change = 1;
			dec->eos = 1;
			avs2_prepare_display_buf(dec);
			ATRACE_COUNTER("V_ST_DEC-submit_eos", __LINE__);
			notify_v4l_eos(hw_to_vdec(dec));
			ATRACE_COUNTER("V_ST_DEC-submit_eos", 0);
			ret = 1;
		}
	}

	return ret;
}


static irqreturn_t vavs2_isr_thread_fn(int irq, void *data)
{
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)data;
	unsigned int dec_status = dec->dec_status;
	int i, ret;
	int32_t start_code = 0;
	struct aml_vcodec_ctx *ctx = dec->v4l2_ctx;

	avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
		"%s decode_status 0x%x process_state %d lcu 0x%x\n",
		__func__, dec_status, dec->process_state,
		READ_VREG(HEVC_PARSER_LCU_START));

#ifndef G12A_BRINGUP_DEBUG
	if (dec->eos) {
		PRINT_LINE();
		goto irq_handled_exit;
	}
#endif
	dec->wait_buf = 0;
	if (dec_status == AVS2_DECODE_BUFEMPTY) {
		PRINT_LINE();
		if (dec->m_ins_flag) {
			reset_process_time(dec);
			if (!vdec_frame_based(hw_to_vdec(dec)))
				dec_again_process(dec);
			else {
				dec->dec_result = DEC_RESULT_DONE;
				reset_process_time(dec);
				amhevc_stop();
				vdec_schedule_work(&dec->work);
			}
		}
		goto irq_handled_exit;
	} else if (dec_status == HEVC_DECPIC_DATA_DONE) {
		PRINT_LINE();
		dec->start_decoding_flag |= 0x3;
		if (dec->m_ins_flag) {
			set_cuva_data(dec);
			update_decoded_pic(dec);
			get_picture_qos_info(dec);
			reset_process_time(dec);
			dec->dec_result = DEC_RESULT_DONE;
			amhevc_stop();
			vdec_schedule_work(&dec->work);
		}
		goto irq_handled_exit;
	}
	PRINT_LINE();

	if (dec_status == AVS2_DECODE_OVER_SIZE) {
		avs2_print(dec, 0,
			"avs2  decode oversize !!\n");
		debug |= (AVS2_DBG_DIS_LOC_ERROR_PROC |
			AVS2_DBG_DIS_SYS_ERROR_PROC);
		dec->fatal_error |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
		vdec_v4l_post_error_event(ctx, DECODER_WARNING_DATA_ERROR);
		if (dec->m_ins_flag)
			reset_process_time(dec);
		goto irq_handled_exit;
	}
	PRINT_LINE();

	if (dec->m_ins_flag)
		reset_process_time(dec);

	if (dec_status == AVS2_HEAD_SEQ_READY)
		start_code = SEQUENCE_HEADER_CODE;
	else if (dec_status == AVS2_HEAD_PIC_I_READY)
		start_code = I_PICTURE_START_CODE;
	else if (dec_status == AVS2_HEAD_PIC_PB_READY)
		start_code = PB_PICTURE_START_CODE;
	else if (dec_status == AVS2_STARTCODE_SEARCH_DONE)
		/*SEQUENCE_END_CODE, VIDEO_EDIT_CODE*/
		start_code = READ_VREG(CUR_NAL_UNIT_TYPE);

	if (dec->process_state == PROC_STATE_HEAD_AGAIN) {
		if ((start_code == I_PICTURE_START_CODE) ||
			(start_code == PB_PICTURE_START_CODE)) {
			avs2_print(dec, 0,
				"PROC_STATE_HEAD_AGAIN error, start_code 0x%x!!!\r\n",
				start_code);
			goto irq_handled_exit;
		} else {
			avs2_print(dec, AVS2_DBG_BUFMGR,
				"PROC_STATE_HEAD_AGAIN, start_code 0x%x\r\n",
				start_code);
			dec->process_state = PROC_STATE_HEAD_DONE;
			WRITE_VREG(HEVC_DEC_STATUS_REG, AVS2_ACTION_DONE);
			goto irq_handled_exit;
		}
	} else if (dec->process_state == PROC_STATE_DECODE_AGAIN) {
		if ((start_code == I_PICTURE_START_CODE) ||
			(start_code == PB_PICTURE_START_CODE)) {
			avs2_print(dec, AVS2_DBG_BUFMGR,
				"PROC_STATE_DECODE_AGAIN=> decode_slice, start_code 0x%x\r\n",
				start_code);
			goto decode_slice;
		} else {
			avs2_print(dec, 0,
				"PROC_STATE_DECODE_AGAIN, start_code 0x%x!!!\r\n",
				start_code);
			WRITE_VREG(HEVC_DEC_STATUS_REG, AVS2_ACTION_DONE);
			goto irq_handled_exit;
		}
	}

	if ((start_code == I_PICTURE_START_CODE) ||
		(start_code == PB_PICTURE_START_CODE) ||
		(start_code == SEQUENCE_END_CODE) ||
		(start_code == VIDEO_EDIT_CODE)) {
		PRINT_LINE();

		if (dec->avs2_dec.hc.cur_pic != NULL) {
			struct internal_comp_buf *ibuf =
				index_to_icomp_buf(dec, dec->cur_fb_idx_mmu);
			int32_t ii;
#ifdef AVS2_10B_MMU
			avs2_recycle_mmu_buf_tail(dec);
#endif
			check_pic_error(dec, dec->avs2_dec.hc.cur_pic);
			avs2_post_process(&dec->avs2_dec);

			if (debug & AVS2_DBG_PRINT_PIC_LIST)
				dump_pic_list(dec);

			avs2_prepare_display_buf(dec);
			dec->avs2_dec.hc.cur_pic = NULL;
			for (ii = 0; ii < dec->avs2_dec.ref_maxbuffer;
					ii++) {
				struct avs2_frame_s *pic =
					dec->avs2_dec.fref[ii];
				if (pic->bg_flag == 0 &&
					pic->is_output == -1 &&
					dec->cur_fb_idx_mmu != INVALID_IDX &&
					pic->vf_ref == 0) {
					if (pic->refered_by_others == 0) {
						int ret = 99;
#ifdef AVS2_10B_MMU
						dec->cur_fb_idx_mmu = INVALID_IDX;

						decoder_mmu_box_free_idx(ibuf->mmu_box,
								ibuf->index);

#ifdef DYNAMIC_ALLOC_HEAD
						decoder_bmmu_box_free_idx(
							dec->bmmu_box,
							HEADER_BUFFER_IDX(pic->index));
						pic->header_adr = 0;
#endif
#endif
#ifndef MV_USE_FIXED_BUF
						ret = decoder_bmmu_box_free_idx(
							dec->bmmu_box,
							MV_BUFFER_IDX(pic->index));
						pic->mpred_mv_wr_start_addr = 0;
#endif
					}
				}
			}
		}
	}

	if ((dec_status == AVS2_HEAD_PIC_I_READY) ||
		(dec_status == AVS2_HEAD_PIC_PB_READY)) {
		struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(dec->v4l2_ctx);

		PRINT_LINE();

		if (debug & AVS2_DBG_SEND_PARAM_WITH_REG) {
			get_rpm_param(
				&dec->avs2_dec.param);
		} else {
			for (i = 0; i < (RPM_END - RPM_BEGIN); i += 4) {
				int ii;
				for (ii = 0; ii < 4; ii++)
					dec->avs2_dec.param.l.data[i + ii] =
						dec->rpm_ptr[i + 3 - ii];
			}
		}
#ifdef SANITY_CHECK
		if (dec->avs2_dec.param.p.num_of_ref_cur >
			dec->avs2_dec.ref_maxbuffer) {
			pr_info("Warning: Wrong num_of_ref_cur %d, force to %d\n",
				dec->avs2_dec.param.p.num_of_ref_cur,
				dec->avs2_dec.ref_maxbuffer);
			dec->avs2_dec.param.p.num_of_ref_cur =
				dec->avs2_dec.ref_maxbuffer;
		}
#endif
		PRINT_LINE();
		debug_buffer_mgr_more(dec);
		get_frame_rate(&dec->avs2_dec.param, dec);

// The video_signal_type is type of uint16_t and result false, so comment it out.
		if (dec->avs2_dec.param.p.video_signal_type & (1<<30)) {
			union param_u *pPara;

			avs2_print(dec, 0, "avs2 HDR meta data present\n");
			pPara = &dec->avs2_dec.param;

			/*clean this flag*/
			pPara->p.video_signal_type &= ~(1<<30);

			dec->vf_dp.present_flag = 1;

			dec->vf_dp.white_point[0] = pPara->p.white_point_x;
			avs2_print(dec, AVS2_DBG_HDR_INFO,
				"white_point[0]:0x%x\n",
				dec->vf_dp.white_point[0]);

			dec->vf_dp.white_point[1] = pPara->p.white_point_y;
			avs2_print(dec, AVS2_DBG_HDR_INFO,
				"white_point[1]:0x%x\n",
				dec->vf_dp.white_point[1]);

			for (i = 0; i < 3; i++) {
				dec->vf_dp.primaries[i][0] = pPara->p.display_primaries_x[i];
				avs2_print(dec, AVS2_DBG_HDR_INFO,
					"primaries[%d][0]:0x%x\n",
					i,
					dec->vf_dp.primaries[i][0]);
			}

			for (i = 0; i < 3; i++) {
				dec->vf_dp.primaries[i][1] = pPara->p.display_primaries_y[i];
				avs2_print(dec, AVS2_DBG_HDR_INFO,
					"primaries[%d][1]:0x%x\n",
					i,
					dec->vf_dp.primaries[i][1]);
			}

			dec->vf_dp.luminance[0] = pPara->p.max_display_mastering_luminance;
			avs2_print(dec, AVS2_DBG_HDR_INFO,
				"luminance[0]:0x%x\n",
				dec->vf_dp.luminance[0]);

			dec->vf_dp.luminance[1] = pPara->p.min_display_mastering_luminance;
			avs2_print(dec, AVS2_DBG_HDR_INFO,
				"luminance[1]:0x%x\n",
				dec->vf_dp.luminance[1]);

			dec->vf_dp.content_light_level.present_flag = 1;
			dec->vf_dp.content_light_level.max_content
				= pPara->p.max_content_light_level;
			avs2_print(dec, AVS2_DBG_HDR_INFO,
				"max_content:0x%x\n",
				dec->vf_dp.content_light_level.max_content);

			dec->vf_dp.content_light_level.max_pic_average =
				pPara->p.max_picture_average_light_level;

			avs2_print(dec, AVS2_DBG_HDR_INFO,
				"max_pic_average:0x%x\n",
				dec->vf_dp.content_light_level.max_pic_average);
		}

		if (dec->video_ori_signal_type !=
			((dec->avs2_dec.param.p.video_signal_type << 16) |
			dec->avs2_dec.param.p.color_description)) {
			u32 v = dec->avs2_dec.param.p.video_signal_type;
			u32 c = dec->avs2_dec.param.p.color_description;
			u32 convert_c = c;

			if (v & 0x2000) {
				avs2_print(dec, AVS2_DBG_HDR_INFO,
					"video_signal_type present:\n");
				avs2_print(dec, AVS2_DBG_HDR_INFO,
					" %s %s\n",
					video_format_names[(v >> 10) & 7],
					((v >> 9) & 1) ?
					"full_range" : "limited");
				if (v & 0x100) {
					u32 transfer;
					u32 maxtrix;

					avs2_print(dec, AVS2_DBG_HDR_INFO,
						"color_description present:\n");
					avs2_print(dec, AVS2_DBG_HDR_INFO,
						"color_primarie = %d\n",
						v & 0xff);
					avs2_print(dec, AVS2_DBG_HDR_INFO,
						"transfer_characteristic = %d\n",
						(c >> 8) & 0xff);
					avs2_print(dec, AVS2_DBG_HDR_INFO,
						"  matrix_coefficient = %d\n",
						c & 0xff);

					transfer = (c >> 8) & 0xFF;
					if (transfer >= 15)
						avs2_print(dec, AVS2_DBG_HDR_INFO,
							"unsupport transfer_characteristic\n");
					else if (transfer  == 14)
						transfer = 18; /* HLG */
					else if (transfer == 13)
						transfer = 32;
					else if (transfer == 12)
						transfer = 16;
					else if (transfer == 11)
						transfer = 15;

					maxtrix = c & 0xFF;
					if (maxtrix >= 10)
						avs2_print(dec, AVS2_DBG_HDR_INFO,
							"unsupport matrix_coefficient\n");
					else if (maxtrix == 9)
						maxtrix = 10;
					else if (maxtrix == 8)
						maxtrix = 9;

					convert_c = (transfer << 8) | (maxtrix);

					avs2_print(dec, AVS2_DBG_HDR_INFO,
						" convered c:0x%x\n",
						convert_c);
				}
			}

			if (enable_force_video_signal_type)
				dec->video_signal_type = force_video_signal_type;
			else {
				dec->video_signal_type = (v << 16) | convert_c;

				dec->video_ori_signal_type = (v << 16) | c;
			}

			video_signal_type = dec->video_signal_type;
		}

		dec->frame_width = dec->avs2_dec.param.p.horizontal_size;
		dec->frame_height = dec->avs2_dec.param.p.vertical_size;
		if (!v4l_res_change(dec)) {
			if (ctx->param_sets_from_ucode && !dec->v4l_params_parsed) {
				struct aml_vdec_ps_infos ps;
				struct vdec_comp_buf_info comp;

				pr_debug("set ucode parse\n");
				if (get_valid_double_write_mode(dec) != 16) {
					vavs2_get_comp_buf_info(dec, &comp);
					vdec_v4l_set_comp_buf_info(ctx, &comp);
				}

				vavs2_get_ps_info(dec, &ps);
				/*notice the v4l2 codec.*/
				vdec_v4l_set_ps_infos(ctx, &ps);
				dec->init_pic_w = dec->frame_width;
				dec->init_pic_h = dec->frame_height;
				dec->last_width = dec->frame_width;
				dec->last_height = dec->frame_height;
				ctx->decoder_status_info.frame_height = ps.visible_height;
				ctx->decoder_status_info.frame_width = ps.visible_width;
				dec->v4l_params_parsed = true;
				dec->process_busy = 0;
				dec_again_process(dec);
				return IRQ_HANDLED;
			} else {
				struct vdec_pic_info pic;

				vdec_v4l_get_pic_info(ctx, &pic);
				dec->used_buf_num = pic.dpb_frames +
					pic.dpb_margin;
				if (dec->used_buf_num > MAX_BUF_NUM)
					dec->used_buf_num = MAX_BUF_NUM;
				if (dec->used_buf_num > FRAME_BUFFERS)
					dec->used_buf_num = FRAME_BUFFERS;
				dec->avs2_dec.ref_maxbuffer = dec->used_buf_num - 1;
			}
		} else {
			dec->process_busy = 0;
			dec_again_process(dec);
			return IRQ_HANDLED;
		}
	}

	PRINT_LINE();
	avs2_prepare_header(&dec->avs2_dec, start_code);

	if (start_code == SEQUENCE_HEADER_CODE ||
		start_code == VIDEO_EDIT_CODE ||
		start_code == SEQUENCE_END_CODE) {
		if (dec->m_ins_flag &&
			vdec_frame_based(hw_to_vdec(dec)))
			dec->start_decoding_flag |= 0x1;
		dec->process_state = PROC_STATE_HEAD_DONE;
		WRITE_VREG(HEVC_DEC_STATUS_REG, AVS2_ACTION_DONE);
	} else if (start_code == I_PICTURE_START_CODE ||
		start_code == PB_PICTURE_START_CODE) {
		struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(dec->v4l2_ctx);
		ret = 0;
		if (dec->pic_list_init_flag == 0) {
			int32_t lcu_size_log2 = log2i(dec->avs2_dec.param.p.lcu_size);
			avs2_init_global_buffers(&dec->avs2_dec);
				/*avs2_dec->m_bg->index is
				set to dec->used_buf_num - 1*/
			init_pic_list(dec, lcu_size_log2);
			init_pic_list_hw(dec);
		}
		ret = avs2_process_header(&dec->avs2_dec);
		if (!dec->m_ins_flag)
			dec->slice_idx++;

		if (dec->m_ins_flag && ret
			&& dec->avs2_dec.hc.cur_pic->cuva_data_buf != NULL)
			release_cuva_data(dec->avs2_dec.hc.cur_pic);

		PRINT_LINE();
		if (ctx->param_sets_from_ucode)
				dec->res_ch_flag = 0;
#ifdef I_ONLY_SUPPORT
		if ((start_code == PB_PICTURE_START_CODE) &&
			(dec->i_only & 0x2))
			ret = -2;
#endif
#ifdef AVS2_10B_MMU
		if (ret >= 0) {
			ret = avs2_alloc_mmu(dec,
				dec->avs2_dec.hc.cur_pic->index,
				dec->avs2_dec.img.width,
				dec->avs2_dec.img.height,
				dec->avs2_dec.input.sample_bit_depth,
				dec->frame_mmu_map_addr);
			if (ret >= 0) {
				dec->cur_fb_idx_mmu =
					dec->avs2_dec.hc.cur_pic->index;
			} else {
				vdec_v4l_post_error_event(ctx, DECODER_ERROR_ALLOC_BUFFER_FAIL);
				pr_err("can't alloc need mmu1,idx %d ret =%d\n",
					dec->avs2_dec.hc.cur_pic->index,
					ret);
			}
		}
#endif

#ifndef MV_USE_FIXED_BUF
		if (ret >= 0 &&
			dec->avs2_dec.hc.cur_pic->
			mpred_mv_wr_start_addr == 0) {
			unsigned long buf_addr;
			unsigned mv_buf_size = get_mv_buf_size(
				dec,
				dec->avs2_dec.hc.cur_pic->pic_w,
				dec->avs2_dec.hc.cur_pic->pic_h);
			int i = dec->avs2_dec.hc.cur_pic->index;

			if (decoder_bmmu_box_alloc_buf_phy(dec->bmmu_box,
				MV_BUFFER_IDX(i),
				mv_buf_size,
				DRIVER_NAME,
				&buf_addr) < 0) {
				vdec_v4l_post_error_event(ctx, DECODER_ERROR_ALLOC_BUFFER_FAIL);
				ret = -1;
			} else
				dec->avs2_dec.hc.cur_pic->mpred_mv_wr_start_addr = buf_addr;
		}
#endif
		if (ret < 0) {
			avs2_print(dec, AVS2_DBG_BUFMGR,
				"avs2_bufmgr_process=> %d, AVS2_10B_DISCARD_NAL\r\n", ret);
			WRITE_VREG(HEVC_DEC_STATUS_REG, AVS2_10B_DISCARD_NAL);

			if (dec->m_ins_flag) {
				dec->dec_result = DEC_RESULT_DONE;
				amhevc_stop();
				vdec_schedule_work(&dec->work);
			}

			goto irq_handled_exit;
		} else {
			PRINT_LINE();
			dec->avs2_dec.hc.cur_pic->stream_offset =
				READ_VREG(HEVC_SHIFT_BYTE_COUNT);

			if (!dec->m_ins_flag) {
				dec->frame_count++;
				decode_frame_count[dec->index]
					= dec->frame_count;
			}
			/*MULTI_INSTANCE_SUPPORT*/
			if (dec->chunk) {
				dec->avs2_dec.hc.cur_pic->pts =
				dec->chunk->pts;
				dec->avs2_dec.hc.cur_pic->pts64 =
				dec->chunk->pts64;
				if (!v4l_bitstream_id_enable)
					dec->avs2_dec.hc.cur_pic->pts64 = dec->chunk->timestamp;
			}

			dec->avs2_dec.hc.cur_pic->bit_depth = dec->avs2_dec.input.sample_bit_depth;
			dec->avs2_dec.hc.cur_pic->double_write_mode = get_double_write_mode(dec);
decode_slice:
			PRINT_LINE();
			config_mc_buffer(dec);
			config_mcrcc_axi_hw(dec);
			config_mpred_hw(dec);
			config_dblk_hw(dec);
			config_sao_hw(dec);
			config_alf_hw(dec);
			config_other_hw(dec);

			avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
				"=>fref0 imgtr_fwRefDistance %d, fref1 imgtr_fwRefDistance %d, dis2/dis3/dis4 %d %d %d  img->tr %d\n",
			    dec->avs2_dec.fref[0]->imgtr_fwRefDistance,
			    dec->avs2_dec.fref[1]->imgtr_fwRefDistance,
			dec->avs2_dec.fref[2]->imgtr_fwRefDistance,
			dec->avs2_dec.fref[3]->imgtr_fwRefDistance,
			dec->avs2_dec.fref[4]->imgtr_fwRefDistance,
			dec->avs2_dec.img.tr);

			if ((debug_again & 0x2) &&
				dec->process_state ==
				PROC_STATE_INIT) {
				dec->process_state = PROC_STATE_DECODING;
				dec_again_process(dec);
				goto irq_handled_exit;
			}

			dec->process_state = PROC_STATE_DECODING;

			WRITE_VREG(HEVC_DEC_STATUS_REG, AVS2_ACTION_DONE);

		}

		if (dec->m_ins_flag)
			start_process_time(dec);
	}
irq_handled_exit:
	PRINT_LINE();
	dec->process_busy = 0;
	return IRQ_HANDLED;
}

static irqreturn_t vavs2_isr(int irq, void *data)
{
	int i;
	unsigned int dec_status;
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)data;
	uint debug_tag;

	WRITE_VREG(HEVC_ASSIST_MBOX0_CLR_REG, 1);

	dec_status = READ_VREG(HEVC_DEC_STATUS_REG);

	if (!dec)
		return IRQ_HANDLED;
	if (dec->init_flag == 0)
		return IRQ_HANDLED;
	if (dec->process_busy)/*on process.*/
		return IRQ_HANDLED;
	dec->dec_status = dec_status;
	dec->process_busy = 1;
	if (debug & AVS2_DBG_IRQ_EVENT)
		avs2_print(dec, 0,
			"avs2 isr dec status  = 0x%x, lcu 0x%x shiftbyte 0x%x (%x %x lev %x, wr %x, rd %x)\n",
			dec_status, READ_VREG(HEVC_PARSER_LCU_START),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_STREAM_START_ADDR),
			READ_VREG(HEVC_STREAM_END_ADDR),
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR));

	debug_tag = READ_HREG(DEBUG_REG1);
	if (debug_tag & 0x10000) {
		dma_sync_single_for_cpu(
			amports_get_dma_device(),
			dec->lmem_phy_addr,
			LMEM_BUF_SIZE,
			DMA_FROM_DEVICE);

		pr_info("LMEM<tag %x>:\n", READ_HREG(DEBUG_REG1));
		for (i = 0; i < 0x400; i += 4) {
			int ii;
			if ((i & 0xf) == 0)
				pr_info("%03x: ", i);
			for (ii = 0; ii < 4; ii++) {
				pr_info("%04x ", dec->lmem_ptr[i + 3 - ii]);
			}
			if (((i + ii) & 0xf) == 0)
				pr_info("\n");
		}

		if (((udebug_pause_pos & 0xffff)
			== (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx == dec->decode_idx) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_HREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			dec->ucode_pause_pos = udebug_pause_pos;
		} else if (debug_tag & 0x20000)
			dec->ucode_pause_pos = 0xffffffff;
		if (dec->ucode_pause_pos)
			reset_process_time(dec);
		else
			WRITE_HREG(DEBUG_REG1, 0);
	} else if (debug_tag != 0) {
		pr_info("dbg%x: %x lcu %x\n", READ_HREG(DEBUG_REG1),
			   READ_HREG(DEBUG_REG2),
			   READ_VREG(HEVC_PARSER_LCU_START));
		if (((udebug_pause_pos & 0xffff)
			== (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx == dec->decode_idx) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_HREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			dec->ucode_pause_pos = udebug_pause_pos;
		}
		if (dec->ucode_pause_pos)
			reset_process_time(dec);
		else
			WRITE_HREG(DEBUG_REG1, 0);
		dec->process_busy = 0;
		return IRQ_HANDLED;
	}

	if (!dec->m_ins_flag) {
		if (dec->error_flag == 1) {
			dec->error_flag = 2;
			dec->process_busy = 0;
			return IRQ_HANDLED;
		} else if (dec->error_flag == 3) {
			dec->process_busy = 0;
			return IRQ_HANDLED;
		}

		if ((dec->pic_list_init_flag) &&
			get_free_buf_count(dec) <= 0) {
			dec->wait_buf = 1;
			dec->process_busy = 0;
			if (debug & AVS2_DBG_IRQ_EVENT)
				avs2_print(dec, 0, "wait_buf\n");
			return IRQ_HANDLED;
		} else if (force_disp_pic_index) {
			dec->process_busy = 0;
			return IRQ_HANDLED;
		}
	}
	return IRQ_WAKE_THREAD;
}

static void vavs2_put_timer_func(struct timer_list *timer)
{
	struct AVS2Decoder_s *dec = container_of(timer,
		struct AVS2Decoder_s, timer);
	uint8_t empty_flag;
	unsigned int buf_level;

	enum receviver_start_e state = RECEIVER_INACTIVE;
	if (dec->init_flag == 0) {
		if (dec->stat & STAT_TIMER_ARM) {
			timer->expires = jiffies + PUT_INTERVAL;
			add_timer(&dec->timer);
		}
		return;
	}
	if (dec->m_ins_flag == 0) {
		if (vf_get_receiver(dec->provider_name)) {
			state = vf_notify_receiver(dec->provider_name,
						VFRAME_EVENT_PROVIDER_QUREY_STATE,
						NULL);
			if ((state == RECEIVER_STATE_NULL) || (state == RECEIVER_STATE_NONE))
				state = RECEIVER_INACTIVE;
		} else
			state = RECEIVER_INACTIVE;

		empty_flag = (READ_VREG(HEVC_PARSER_INT_STATUS) >> 6) & 0x1;
		/* error watchdog */
		if (empty_flag == 0) {
			/* decoder has input */
			if ((debug & AVS2_DBG_DIS_LOC_ERROR_PROC) == 0) {

				buf_level = READ_VREG(HEVC_STREAM_LEVEL);
				/* receiver has no buffer to recycle */
				if ((state == RECEIVER_INACTIVE) &&
					(kfifo_is_empty(&dec->display_q) &&
					 buf_level > 0x200)
					) {
						WRITE_VREG
						(HEVC_ASSIST_MBOX0_IRQ_REG,
						 0x1);
				}
			}
		}
	} else {
		if ((decode_timeout_val > 0) &&
			(dec->start_process_time > 0) &&
			((1000 * (jiffies - dec->start_process_time) / HZ) >
			decode_timeout_val)) {
			int current_lcu_idx = READ_VREG(HEVC_PARSER_LCU_START) & 0xffffff;
			if (dec->last_lcu_idx == current_lcu_idx) {
				if (dec->decode_timeout_count > 0)
					dec->decode_timeout_count--;
				if (dec->decode_timeout_count == 0) {
					if (input_frame_based(hw_to_vdec(dec)) ||
						(READ_VREG(HEVC_STREAM_LEVEL) > 0x200))
						timeout_process(dec);
					else {
						avs2_print(dec, 0,
							"timeout & empty, again\n");
						dec_again_process(dec);
					}
				}
			} else {
				start_process_time(dec);
				dec->last_lcu_idx = current_lcu_idx;
			}
		}
	}

	if ((dec->ucode_pause_pos != 0) &&
		(dec->ucode_pause_pos != 0xffffffff) &&
		udebug_pause_pos != dec->ucode_pause_pos) {
		dec->ucode_pause_pos = 0;
		WRITE_HREG(DEBUG_REG1, 0);
	}
	if (debug & AVS2_DBG_DUMP_DATA) {
		debug &= ~AVS2_DBG_DUMP_DATA;
		avs2_print(dec, 0,
			"%s: chunk size 0x%x off 0x%x sum 0x%x\n",
			__func__,
			dec->chunk->size,
			dec->chunk->offset,
			get_data_check_sum(dec, dec->chunk->size));
		dump_data(dec, dec->chunk->size);
	}
	if (debug & AVS2_DBG_DUMP_PIC_LIST) {
		dump_pic_list(dec);
		debug &= ~AVS2_DBG_DUMP_PIC_LIST;
	}
	if (debug & AVS2_DBG_TRIG_SLICE_SEGMENT_PROC) {
		WRITE_VREG(HEVC_ASSIST_MBOX0_IRQ_REG, 0x1);
		debug &= ~AVS2_DBG_TRIG_SLICE_SEGMENT_PROC;
	}
	if (debug & AVS2_DBG_DUMP_RPM_BUF) {
		int i;

		pr_info("RPM:\n");
		for (i = 0; i < RPM_BUF_SIZE; i += 4) {
			int ii;
			if ((i & 0xf) == 0)
				pr_info("%03x: ", i);
			for (ii = 0; ii < 4; ii++) {
				pr_info("%04x ",
					   dec->lmem_ptr[i + 3 - ii]);
			}
			if (((i + ii) & 0xf) == 0)
				pr_info("\n");
		}
		debug &= ~AVS2_DBG_DUMP_RPM_BUF;
	}
	if (debug & AVS2_DBG_DUMP_LMEM_BUF) {
		int i;

		pr_info("LMEM:\n");
		for (i = 0; i < LMEM_BUF_SIZE; i += 4) {
			int ii;
			if ((i & 0xf) == 0)
				pr_info("%03x: ", i);
			for (ii = 0; ii < 4; ii++) {
				pr_info("%04x ",
					   dec->lmem_ptr[i + 3 - ii]);
			}
			if (((i + ii) & 0xf) == 0)
				pr_info("\n");
		}
		debug &= ~AVS2_DBG_DUMP_LMEM_BUF;
	}
	/*if (debug & AVS2_DBG_HW_RESET) {
	}*/

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
			(1 << 7) | 16);
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
				get_double_write_mode(dec) == 0) {
				disp_laddr =
					READ_VCBUS_REG(AFBC_BODY_BADDR) << 4;
			} else {
				struct canvas_s cur_canvas;
				canvas_read((READ_VCBUS_REG(VD1_IF0_CANVAS0) & 0xff), &cur_canvas);
				disp_laddr = cur_canvas.addr;
			}
			pr_info("current displayed buffer address %x\r\n", disp_laddr);
		}
		dbg_cmd = 0;
	}
	/*don't changed at start.*/
	if (dec->get_frame_dur && dec->show_frame_num > 60 &&
		dec->frame_dur > 0 && dec->saved_resolution !=
		frame_width * frame_height * (96000 / dec->frame_dur)) {
		int fps = 96000 / dec->frame_dur;
		if (hevc_source_changed(VFORMAT_AVS2,
			frame_width, frame_height, fps) > 0)
			dec->saved_resolution = frame_width *
			frame_height * fps;
	}

	timer->expires = jiffies + PUT_INTERVAL;
	add_timer(timer);
}


int vavs2_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	struct AVS2Decoder_s *dec =
		(struct AVS2Decoder_s *)vdec->private;

	if (!dec)
		return -1;

	vstatus->frame_width = dec->frame_width;
	vstatus->frame_height = dec->frame_height;

	if (dec->frame_dur != 0)
		vstatus->frame_rate = ((96000 * 10 / dec->frame_dur) % 10) < 5 ?
			96000 / dec->frame_dur : (96000 / dec->frame_dur +1);
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = 0;
	vstatus->status = dec->stat | dec->fatal_error;
	vstatus->frame_dur = dec->frame_dur;
	vstatus->bit_rate = dec->gvs->bit_rate;
	vstatus->frame_data = dec->gvs->frame_data;
	vstatus->total_data = dec->gvs->total_data;
	vstatus->frame_count = dec->gvs->frame_count;
	vstatus->error_frame_count = dec->gvs->error_frame_count;
	vstatus->drop_frame_count = dec->gvs->drop_frame_count;
	vstatus->i_decoded_frames = dec->gvs->i_decoded_frames;
	vstatus->i_lost_frames =  dec->gvs->i_lost_frames;
	vstatus->i_concealed_frames =  dec->gvs->i_concealed_frames;
	vstatus->p_decoded_frames =  dec->gvs->p_decoded_frames;
	vstatus->p_lost_frames =  dec->gvs->p_lost_frames;
	vstatus->p_concealed_frames =  dec->gvs->p_concealed_frames;
	vstatus->b_decoded_frames =  dec->gvs->b_decoded_frames;
	vstatus->b_lost_frames =  dec->gvs->b_lost_frames;
	vstatus->b_concealed_frames =  dec->gvs->b_concealed_frames;
	vstatus->total_data = dec->gvs->total_data;
	vstatus->samp_cnt = dec->gvs->samp_cnt;
	vstatus->offset = dec->gvs->offset;
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
		"%s", DRIVER_NAME);
	return 0;
}

int vavs2_set_isreset(struct vdec_s *vdec, int isreset)
{
	is_reset = isreset;
	return 0;
}

static void vavs2_prot_init(struct AVS2Decoder_s *dec)
{
	unsigned int data32;

	avs2_config_work_space_hw(dec);
	if (dec->pic_list_init_flag)
		init_pic_list_hw(dec);

	avs2_init_decoder_hw(dec);

	avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
		"%s\n", __func__);
	data32 = READ_VREG(HEVC_STREAM_CONTROL);
	data32 = data32 |
		(1 << 0)/*stream_fetch_enable*/
		;
	WRITE_VREG(HEVC_STREAM_CONTROL, data32);
	WRITE_VREG(HEVC_SHIFT_STARTCODE, 0x00000100);
	WRITE_VREG(HEVC_SHIFT_EMULATECODE, 0x00000000);

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

	config_cuva_buf(dec);
}

#ifdef I_ONLY_SUPPORT
static int vavs2_set_trickmode(struct vdec_s *vdec, unsigned long trickmode)
{
	struct AVS2Decoder_s *dec =
		(struct AVS2Decoder_s *)vdec->private;
	if (i_only_flag & 0x100)
		return 0;
	if (trickmode == TRICKMODE_I || trickmode == TRICKMODE_I_HEVC)
		dec->i_only = 0x3;
	else if (trickmode == TRICKMODE_NONE)
		dec->i_only = 0x0;
	return 0;
}
#endif

static int vavs2_local_init(struct AVS2Decoder_s *dec)
{
	int i;
	int ret;
	int width, height;

	dec->gvs = vzalloc(sizeof(struct vdec_info));
	if (NULL == dec->gvs) {
		avs2_print(dec, 0,
			"the struct of vdec status malloc failed.\n");
		return -1;
	}
#ifdef DEBUG_PTS
	dec->pts_missed = 0;
	dec->pts_hit = 0;
#endif
	dec->new_frame_displayed = 0;
	dec->last_put_idx = -1;
	dec->saved_resolution = 0;
	dec->get_frame_dur = false;
	on_no_keyframe_skiped = 0;
	width = dec->vavs2_amstream_dec_info.width;
	height = dec->vavs2_amstream_dec_info.height;
	dec->frame_dur =
		(dec->vavs2_amstream_dec_info.rate ==
		 0) ? 3600 : dec->vavs2_amstream_dec_info.rate;
	if (width && height)
		dec->frame_ar = height * 0x100 / width;
/*
TODO:FOR VERSION
*/
	avs2_print(dec, AVS2_DBG_BUFMGR,
		"avs2: ver (%d,%d) decinfo: %dx%d rate=%d\n", avs2_version,
		0, width, height, dec->frame_dur);

	if (dec->frame_dur == 0)
		dec->frame_dur = 96000 / 24;
#ifdef I_ONLY_SUPPORT
	if (i_only_flag & 0x100)
		dec->i_only = i_only_flag & 0xff;
	else if ((unsigned long) dec->vavs2_amstream_dec_info.param & 0x08)
		dec->i_only = 0x7;
	else
		dec->i_only = 0x0;
#endif
	INIT_KFIFO(dec->display_q);
	INIT_KFIFO(dec->newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &dec->vfpool[i];
		dec->vfpool[i].index = -1;
		kfifo_put(&dec->newframe_q, vf);
	}

	ret = avs2_local_init(dec);

	return ret;
}


static s32 vavs2_init(struct vdec_s *vdec)
{
	int ret = -1, size = -1;
	int fw_size = 0x1000 * 16;
	struct firmware_s *fw = NULL;
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = dec->v4l2_ctx;

	timer_setup(&dec->timer, vavs2_put_timer_func, 0);

	dec->stat |= STAT_TIMER_INIT;
	if (vavs2_local_init(dec) < 0) {
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_NO_MEM);
		return -EBUSY;
	}

	vdec_set_vframe_comm(vdec, DRIVER_NAME);

	fw = vmalloc(sizeof(struct firmware_s) + fw_size);
	if (IS_ERR_OR_NULL(fw))
		return -ENOMEM;

	size = get_firmware_data(VIDEO_DEC_AVS2_MMU, fw->data);
	if (size < 0) {
		pr_err("get firmware fail.\n");
		vfree(fw);
		return -1;
	}

	fw->len = fw_size;

	if (dec->m_ins_flag) {
		dec->timer.expires = jiffies + PUT_INTERVAL;

		INIT_WORK(&dec->work, avs2_work);
		dec->fw = fw;

		return 0;
	}

	amhevc_enable();

	ret = amhevc_loadmc_ex(VFORMAT_AVS2, NULL, fw->data);
	if (ret < 0) {
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_FW_LOAD_ERROR);
		amhevc_disable();
		vfree(fw);
		pr_err("AVS2: the %s fw loading failed, err: %x\n",
			tee_enabled() ? "TEE" : "local", ret);
		return -EBUSY;
	}

	vfree(fw);

	dec->stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	vavs2_prot_init(dec);

	if (vdec_request_threaded_irq(VDEC_IRQ_0,
		vavs2_isr, vavs2_isr_thread_fn,
		IRQF_ONESHOT,/*run thread on this irq disabled*/
		"vavs2-irq", (void *)dec)) {
		pr_info("vavs2 irq register error.\n");
		amhevc_disable();
		return -ENOENT;
	}

	dec->stat |= STAT_ISR_REG;
	dec->provider_name = PROVIDER_NAME;

	dec->stat |= STAT_VF_HOOK;
	dec->timer.expires = jiffies + PUT_INTERVAL;
	add_timer(&dec->timer);

	dec->stat |= STAT_TIMER_ARM;

	dec->process_busy = 0;
	avs2_print(dec, AVS2_DBG_BUFMGR_MORE,
		"%d, vavs2_init, RP=0x%x\n",
		__LINE__, READ_VREG(HEVC_STREAM_RD_PTR));
	return 0;
}

static int vmavs2_stop(struct AVS2Decoder_s *dec)
{
	dec->init_flag = 0;
	dec->first_sc_checked = 0;
	if (dec->stat & STAT_TIMER_ARM) {
		del_timer_sync(&dec->timer);
		dec->stat &= ~STAT_TIMER_ARM;
	}

	avs2_local_uninit(dec);
	reset_process_time(dec);
	cancel_work_sync(&dec->work);
	uninit_mmu_buffers(dec);
	if (dec->fw) {
		vfree(dec->fw);
		dec->fw = NULL;
	}

	return 0;
}

static int amvdec_avs2_mmu_init(struct AVS2Decoder_s *dec)
{
	int tvp_flag = vdec_secure(hw_to_vdec(dec)) ?
		CODEC_MM_FLAGS_TVP : 0;
	int buf_size = 48;

#ifdef AVS2_10B_MMU
	dec->need_cache_size = buf_size * SZ_1M;
	dec->sc_start_time = get_jiffies_64();
#endif
	dec->bmmu_box = decoder_bmmu_box_alloc_box(
		DRIVER_NAME,
		dec->index,
		MAX_BMMU_BUFFER_NUM,
		4 + PAGE_SHIFT,
		CODEC_MM_FLAGS_CMA_CLEAR |
		CODEC_MM_FLAGS_FOR_VDECODER |
		tvp_flag,
		BMMU_ALLOC_FLAGS_WAIT);
	if (!dec->bmmu_box) {
		pr_err("avs2 alloc bmmu box failed!!\n");
		return -1;
	}
	return 0;
}

/****************************************/
static struct codec_profile_t amvdec_avs2_profile = {
	.name = "AVS2-V4L",
	.profile = ""
};

static unsigned char get_data_check_sum
	(struct AVS2Decoder_s *dec, int size)
{
	int jj;
	int sum = 0;
	u8 *data = NULL;

	if (!dec->chunk->block->is_mapped)
		data = codec_mm_vmap(dec->chunk->block->start +
			dec->chunk->offset, size);
	else
		data = ((u8 *)dec->chunk->block->start_virt) +
			dec->chunk->offset;

	for (jj = 0; jj < size; jj++)
		sum += data[jj];

	if (!dec->chunk->block->is_mapped)
		codec_mm_unmap_phyaddr(data);
	return sum;
}

static void dump_data(struct AVS2Decoder_s *dec, int size)
{
	int jj;
	u8 *data = NULL;
	int padding_size = dec->chunk->offset &
		(VDEC_FIFO_ALIGN - 1);

	if (!dec->chunk->block->is_mapped)
		data = codec_mm_vmap(dec->chunk->block->start +
			dec->chunk->offset, size);
	else
		data = ((u8 *)dec->chunk->block->start_virt) +
			dec->chunk->offset;

	avs2_print(dec, 0, "padding: ");
	for (jj = padding_size; jj > 0; jj--)
		avs2_print_cont(dec, 0, "%02x ", *(data - jj));
	avs2_print_cont(dec, 0, "data adr %p\n", data);

	for (jj = 0; jj < size; jj++) {
		if ((jj & 0xf) == 0)
			avs2_print(dec, 0, "%06x:", jj);
		avs2_print_cont(dec, 0, "%02x ", data[jj]);
		if (((jj + 1) & 0xf) == 0)
			avs2_print(dec, 0, "\n");
	}
	avs2_print(dec, 0, "\n");

	if (!dec->chunk->block->is_mapped)
		codec_mm_unmap_phyaddr(data);
}

static void avs2_work(struct work_struct *work)
{
	struct AVS2Decoder_s *dec = container_of(work, struct AVS2Decoder_s, work);
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(dec->v4l2_ctx);
	struct vdec_s *vdec = hw_to_vdec(dec);
	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */
	avs2_print(dec, PRINT_FLAG_VDEC_DETAIL,
		"%s dec_result %d %x %x %x\n",
		__func__,
		dec->dec_result,
		READ_VREG(HEVC_STREAM_LEVEL),
		READ_VREG(HEVC_STREAM_WR_PTR),
		READ_VREG(HEVC_STREAM_RD_PTR));

	if (((dec->dec_result == DEC_RESULT_GET_DATA) ||
		(dec->dec_result == DEC_RESULT_GET_DATA_RETRY)) &&
		(hw_to_vdec(dec)->next_status !=
		VDEC_STATUS_DISCONNECTED)) {
		if (!vdec_has_more_input(vdec)) {
			dec->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&dec->work);
			return;
		}

		if (dec->dec_result == DEC_RESULT_GET_DATA) {
			avs2_print(dec, PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_GET_DATA %x %x %x\n",
				__func__,
				READ_VREG(HEVC_STREAM_LEVEL),
				READ_VREG(HEVC_STREAM_WR_PTR),
				READ_VREG(HEVC_STREAM_RD_PTR));
			vdec_vframe_dirty(vdec, dec->chunk);
			vdec_clean_input(vdec);
		}

		if (get_free_buf_count(dec) >=
			dec->run_ready_min_buf_num) {
			int r;
			int decode_size;
			r = vdec_prepare_input(vdec, &dec->chunk);
			if (r < 0) {
				dec->dec_result = DEC_RESULT_GET_DATA_RETRY;

				avs2_print(dec,
					PRINT_FLAG_VDEC_DETAIL,
					"amvdec_vh265: Insufficient data\n");

				vdec_schedule_work(&dec->work);
				return;
			}
			dec->dec_result = DEC_RESULT_NONE;
			avs2_print(dec, PRINT_FLAG_VDEC_STATUS,
				"%s: chunk size 0x%x sum 0x%x\n",
				__func__, r,
				(debug & PRINT_FLAG_VDEC_STATUS) ?
				get_data_check_sum(dec, r) : 0
				);
			if (debug & PRINT_FLAG_VDEC_DATA)
				dump_data(dec, dec->chunk->size);

			decode_size = dec->chunk->size +
				(dec->chunk->offset & (VDEC_FIFO_ALIGN - 1));

			WRITE_VREG(HEVC_DECODE_SIZE,
				READ_VREG(HEVC_DECODE_SIZE) + decode_size);

			vdec_enable_input(vdec);

			WRITE_VREG(HEVC_DEC_STATUS_REG, AVS2_ACTION_DONE);

			start_process_time(dec);

		} else{
			dec->dec_result = DEC_RESULT_GET_DATA_RETRY;

			avs2_print(dec, PRINT_FLAG_VDEC_DETAIL,
				"amvdec_vh265: Insufficient data\n");

			vdec_schedule_work(&dec->work);
		}
		return;
	} else if (dec->dec_result == DEC_RESULT_DONE) {
		dec->slice_idx++;
		dec->frame_count++;
		dec->process_state = PROC_STATE_INIT;
		decode_frame_count[dec->index] = dec->frame_count;
		ctx->decoder_status_info.decoder_count++;

#ifdef AVS2_10B_MMU
		dec->used_4k_num =
			(READ_VREG(HEVC_SAO_MMU_STATUS) >> 16);
#endif
		avs2_print(dec, PRINT_FLAG_VDEC_STATUS,
			"%s (===> %d) dec_result %d %x %x %x shiftbytes 0x%x decbytes 0x%x\n",
			__func__,
			dec->frame_count,
			dec->dec_result,
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT),
			READ_VREG(HEVC_SHIFT_BYTE_COUNT) - dec->start_shift_bytes);
		vdec_vframe_dirty(hw_to_vdec(dec), dec->chunk);
	} else if (dec->dec_result == DEC_RESULT_AGAIN) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		if (!vdec_has_more_input(vdec)) {
			dec->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&dec->work);
			return;
		}
	} else if (dec->dec_result == DEC_RESULT_EOS) {
		avs2_print(dec, 0, "%s: end of stream\n", __func__);
		dec->eos = 1;
		if ( dec->avs2_dec.hc.cur_pic != NULL) {
			check_pic_error(dec, dec->avs2_dec.hc.cur_pic);
			avs2_post_process(&dec->avs2_dec);
			avs2_prepare_display_buf(dec);
		}
		notify_v4l_eos(hw_to_vdec(dec));
		vdec_vframe_dirty(hw_to_vdec(dec), dec->chunk);
	} else if (dec->dec_result == DEC_RESULT_FORCE_EXIT) {
		avs2_print(dec, PRINT_FLAG_VDEC_STATUS, "%s: force exit\n", __func__);
		if (dec->stat & STAT_VDEC_RUN) {
			amhevc_stop();
			dec->stat &= ~STAT_VDEC_RUN;
		}

		if (dec->stat & STAT_ISR_REG) {
			if (!dec->m_ins_flag)
				WRITE_VREG(HEVC_ASSIST_MBOX0_MASK, 0);
			vdec_free_irq(VDEC_IRQ_0, (void *)dec);
			dec->stat &= ~STAT_ISR_REG;
		}
	}

	if (dec->stat & STAT_TIMER_ARM) {
		del_timer_sync(&dec->timer);
		dec->stat &= ~STAT_TIMER_ARM;
	}
	/* mark itself has all HW resource released and input released */
	if (vdec->parallel_dec ==1)
		vdec_core_finish_run(vdec, CORE_MASK_HEVC);
	else
		vdec_core_finish_run(vdec, CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

	if (ctx->param_sets_from_ucode &&
		!dec->v4l_params_parsed)
		vdec_v4l_write_frame_sync(ctx);

	if (dec->vdec_cb)
		dec->vdec_cb(hw_to_vdec(dec), dec->vdec_cb_arg);
}

static int avs2_hw_ctx_restore(struct AVS2Decoder_s *dec)
{
	/* new to do ... */
	vavs2_prot_init(dec);
	return 0;
}

static bool is_avaliable_buffer(struct AVS2Decoder_s *dec)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(dec->v4l2_ctx);
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	int i, free_count = 0;
	if ((avs2_dec->ref_maxbuffer == 0) ||
		(ctx->cap_pool.dec < avs2_dec->ref_maxbuffer)) {
		if (ctx->fb_ops.query(&ctx->fb_ops, &dec->fb_token)) {
			free_count = v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx) + 1;
		}
	}

	for (i = 0; i < avs2_dec->ref_maxbuffer; ++i) {
		if (avs2_dec->fref[i] == NULL)
			continue;

		if ((avs2_dec->fref[i]->imgcoi_ref < -256) &&
				(avs2_dec->fref[i]->is_output == -1) &&
				(avs2_dec->fref[i]->bg_flag == 0) &&
				(avs2_dec->fref[i]->vf_ref == 0) &&
				(avs2_dec->fref[i]->to_prepare_disp == 0) &&
				(avs2_dec->fref[i]->index != -1) &&
				(avs2_dec->fref[i]->cma_alloc_addr)) {
			free_count++;
		}
	}
	return free_count < dec->run_ready_min_buf_num ? 0 : 1;
}

static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(dec->v4l2_ctx);
	int tvp = vdec_secure(hw_to_vdec(dec)) ?
		CODEC_MM_FLAGS_TVP : 0;
	unsigned long ret = 0;
	avs2_print(dec, PRINT_FLAG_VDEC_DETAIL, "%s\r\n", __func__);
	if (debug & AVS2_DBG_PIC_LEAK_WAIT)
		return ret;

	if (dec->eos)
		return ret;
#ifdef AVS2_10B_MMU
	if (!dec->first_sc_checked) {
		int size;
		void * mmu_box;
		struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(dec->v4l2_ctx);

		mmu_box = ctx->mmu_box;
		size = decoder_mmu_box_sc_check(mmu_box, tvp);
		dec->first_sc_checked = 1;
		avs2_print(dec, 0, "vavs2 cached=%d  need_size=%d speed= %d ms\n",
			size, (dec->need_cache_size >> PAGE_SHIFT),
			(int)(get_jiffies_64() - dec->sc_start_time) * 1000/HZ);
	}
#endif
	if (dec->next_again_flag &&
		(!vdec_frame_based(vdec))) {
		u32 parser_wr_ptr =
			STBUF_READ(&vdec->vbuf, get_wp);
		if (parser_wr_ptr >= dec->pre_parser_wr_ptr &&
			(parser_wr_ptr - dec->pre_parser_wr_ptr) <
			again_threshold) {
			int r = vdec_sync_input(vdec);
			avs2_print(dec,
			PRINT_FLAG_VDEC_DETAIL, "%s buf lelvel:%x\n", __func__, r);
			return 0;
		}
	}

	if ((dec->pic_list_init_flag == 0) ||
		get_free_buf_count(dec) >= dec->run_ready_min_buf_num)
		ret = 1;
#ifdef CONSTRAIN_MAX_BUF_NUM
	if (dec->pic_list_init_flag) {
		if (run_ready_max_vf_only_num > 0 &&
			get_vf_ref_only_buf_count(dec) >=
			run_ready_max_vf_only_num
			)
			ret = 0;
		if (run_ready_display_q_num > 0 &&
			kfifo_len(&dec->display_q) >=
			run_ready_display_q_num)
			ret = 0;

		if (run_ready_max_buf_num == 0xff &&
			get_used_buf_count(dec) >=
			dec->avs2_dec.ref_maxbuffer)
			ret = 0;
		else if (run_ready_max_buf_num &&
			get_used_buf_count(dec) >=
			run_ready_max_buf_num)
			ret = 0;
	}
#endif

	if (dec->v4l_params_parsed) {
		if (is_avaliable_buffer(dec))
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
		not_run_ready[dec->index] = 0;
	else
		not_run_ready[dec->index]++;

	if (vdec->parallel_dec == 1)
		return ret ? CORE_MASK_HEVC : 0;
	else
		return ret ? (CORE_MASK_VDEC_1 | CORE_MASK_HEVC) : 0;
}

static void run(struct vdec_s *vdec, unsigned long mask,
	void (*callback)(struct vdec_s *, void *), void *arg)
{
	struct AVS2Decoder_s *dec =
		(struct AVS2Decoder_s *)vdec->private;
	int r;
	struct aml_vcodec_ctx *ctx = dec->v4l2_ctx;

	run_count[dec->index]++;
	dec->vdec_cb_arg = arg;
	dec->vdec_cb = callback;
	/* dec->chunk = vdec_prepare_input(vdec); */
	hevc_reset_core(vdec);

	if (vdec_stream_based(vdec)) {
		dec->pre_parser_wr_ptr =
			STBUF_READ(&vdec->vbuf, get_wp);
		dec->next_again_flag = 0;
	}

	r = vdec_prepare_input(vdec, &dec->chunk);
	if (r < 0) {
		input_empty[dec->index]++;
		dec->dec_result = DEC_RESULT_AGAIN;

		avs2_print(dec, PRINT_FLAG_VDEC_DETAIL,
			"ammvdec_vavs2: Insufficient data\n");

		vdec_schedule_work(&dec->work);
		return;
	}
	input_empty[dec->index] = 0;
	dec->dec_result = DEC_RESULT_NONE;
	dec->start_shift_bytes = READ_VREG(HEVC_SHIFT_BYTE_COUNT);

	if (debug & PRINT_FLAG_VDEC_STATUS) {
		int ii;
		avs2_print(dec, 0,
			"%s (%d): size %d (0x%x 0x%x) sum 0x%x (%x %x %x %x %x) bytes 0x%x",
			__func__,
			dec->frame_count, r,
			dec->chunk ? dec->chunk->size : 0,
			dec->chunk ? dec->chunk->offset : 0,
			dec->chunk ? ((vdec_frame_based(vdec) &&
			(debug & PRINT_FLAG_VDEC_STATUS)) ?
			get_data_check_sum(dec, r) : 0) : 0,
			READ_VREG(HEVC_STREAM_START_ADDR),
			READ_VREG(HEVC_STREAM_END_ADDR),
			READ_VREG(HEVC_STREAM_LEVEL),
			READ_VREG(HEVC_STREAM_WR_PTR),
			READ_VREG(HEVC_STREAM_RD_PTR),
			dec->start_shift_bytes);
		if (vdec_frame_based(vdec) && dec->chunk) {
			u8 *data = NULL;
			if (!dec->chunk->block->is_mapped)
				data = codec_mm_vmap(dec->chunk->block->start +
					dec->chunk->offset, 8);
			else
				data = ((u8 *)dec->chunk->block->start_virt) +
					dec->chunk->offset;

			avs2_print_cont(dec, 0, "data adr %p:", data);
			for (ii = 0; ii < 8; ii++)
				avs2_print_cont(dec, 0, "%02x ", data[ii]);
			if (!dec->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		}
		avs2_print_cont(dec, 0, "\r\n");
	}
	if (vdec->mc_loaded) {
		/*firmware have load before,
		  and not changes to another.
		  ignore reload.
		*/
	} else if (amhevc_loadmc_ex(VFORMAT_AVS2, NULL, dec->fw->data) < 0) {
		vdec->mc_loaded = 0;
		amhevc_disable();
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_FW_LOAD_ERROR);
		avs2_print(dec, 0, "%s: Error amvdec_loadmc fail\n", __func__);
		dec->dec_result = DEC_RESULT_FORCE_EXIT;
		vdec_schedule_work(&dec->work);
		return;
	} else {
		vdec->mc_loaded = 1;
		vdec->mc_type = VFORMAT_AVS2;
	}

	if (avs2_hw_ctx_restore(dec) < 0) {
		vdec_schedule_work(&dec->work);
		return;
	}

	vdec_enable_input(vdec);

	WRITE_VREG(HEVC_DEC_STATUS_REG, AVS2_SEARCH_NEW_PIC);

	if (vdec_frame_based(vdec) && dec->chunk) {
		if (debug & PRINT_FLAG_VDEC_DATA)
			dump_data(dec, dec->chunk->size);

		WRITE_VREG(HEVC_SHIFT_BYTE_COUNT, 0);
		r = dec->chunk->size +
			(dec->chunk->offset & (VDEC_FIFO_ALIGN - 1));
		if (vdec->mvfrm)
			vdec->mvfrm->frame_size = dec->chunk->size;
	}

	WRITE_VREG(HEVC_DECODE_SIZE, r);
	WRITE_VREG(HEVC_DECODE_COUNT, dec->slice_idx);
	dec->init_flag = 1;

	avs2_print(dec, PRINT_FLAG_VDEC_DETAIL,
		"%s: start hevc (%x %x %x)\n",
		__func__,
		READ_VREG(HEVC_DEC_STATUS_REG),
		READ_VREG(HEVC_MPC_E),
		READ_VREG(HEVC_MPSR));

	start_process_time(dec);
	mod_timer(&dec->timer, jiffies);
	dec->stat |= STAT_TIMER_ARM;
	dec->stat |= STAT_ISR_REG;
	if (vdec->mvfrm)
		vdec->mvfrm->hw_decode_start = local_clock();
	amhevc_start();
	dec->stat |= STAT_VDEC_RUN;
}

static void  avs2_decode_ctx_reset(struct AVS2Decoder_s *dec)
{
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct vdec_s *vdec = hw_to_vdec(dec);
	int i;

	for (i = 0; i < REF_MAXBUFFER; ++i) {
		avs2_dec->frm_pool[i].imgcoi_ref = -257;
		avs2_dec->frm_pool[i].vf_ref = 0;
		avs2_dec->frm_pool[i].decode_idx = 0;
		avs2_dec->frm_pool[i].cma_alloc_addr = 0;
		avs2_dec->frm_pool[i].index = i;
		avs2_dec->frm_pool[i].BUF_index = -1;
		avs2_dec->frm_pool[i].mv_buf_index = -1;
	}

	if (vdec->parallel_dec == 1) {
		for (i = 0; i < REF_MAXBUFFER; i++) {
			vdec->free_canvas_ex(avs2_dec->frm_pool[i].y_canvas_index, vdec->id);
			vdec->free_canvas_ex(avs2_dec->frm_pool[i].uv_canvas_index, vdec->id);
		}
	}

	dec->init_flag		= 0;
	dec->first_sc_checked	= 0;
	dec->fatal_error		= 0;
	dec->show_frame_num	= 0;
	dec->process_busy	= 0;
	dec->process_state	= 0;
	dec->eos			= 0;
}

static void reset(struct vdec_s *vdec)
{
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = dec->v4l2_ctx;

	cancel_work_sync(&dec->work);

	if (dec->stat & STAT_VDEC_RUN) {
		amhevc_stop();
		dec->stat &= ~STAT_VDEC_RUN;
	}

	if (dec->stat & STAT_TIMER_ARM) {
		del_timer_sync(&dec->timer);
		dec->stat &= ~STAT_TIMER_ARM;
	}

	reset_process_time(dec);

	avs2_local_uninit(dec);
	if (vavs2_local_init(dec) < 0) {
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_NO_MEM);
		avs2_print(dec, 0, "%s local_init failed \r\n", __func__);
	}

	avs2_decode_ctx_reset(dec);
	atomic_set(&dec->vf_pre_count, 0);
	atomic_set(&dec->vf_get_count, 0);
	atomic_set(&dec->vf_put_count, 0);

	avs2_print(dec, PRINT_FLAG_VDEC_DETAIL, "%s\r\n", __func__);
}

static irqreturn_t avs2_irq_cb(struct vdec_s *vdec, int irq)
{
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)vdec->private;
	return vavs2_isr(0, dec);
}

static irqreturn_t avs2_threaded_irq_cb(struct vdec_s *vdec, int irq)
{
	struct AVS2Decoder_s *dec =
		(struct AVS2Decoder_s *)vdec->private;
	return vavs2_isr_thread_fn(0, dec);
}

static void avs2_dump_state(struct vdec_s *vdec)
{
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)vdec->private;
	int i;
	avs2_print(dec, 0, "====== %s\n", __func__);

	avs2_print(dec, 0,
		"width/height (%d/%d), used_buf_num %d\n",
		dec->avs2_dec.img.width,
		dec->avs2_dec.img.height,
		dec->used_buf_num
		);

	avs2_print(dec, 0,
		"is_framebase(%d), eos %d, dec_result 0x%x dec_frm %d disp_frm %d run %d not_run_ready %d input_empty %d\n",
		input_frame_based(vdec),
		dec->eos,
		dec->dec_result,
		decode_frame_count[dec->index],
		display_frame_count[dec->index],
		run_count[dec->index],
		not_run_ready[dec->index],
		input_empty[dec->index]
		);

	avs2_print(dec, 0,
		"%s, newq(%d/%d), dispq(%d/%d), vf prepare/get/put (%d/%d/%d), free_buf_count %d (min %d for run_ready)\n",
		__func__,
		kfifo_len(&dec->newframe_q),
		VF_POOL_SIZE,
		kfifo_len(&dec->display_q),
		VF_POOL_SIZE,
		dec->vf_pre_count,
		dec->vf_get_count,
		dec->vf_put_count,
		get_free_buf_count(dec),
		dec->run_ready_min_buf_num);

	dump_pic_list(dec);

	for (i = 0; i < MAX_BUF_NUM; i++) {
		avs2_print(dec, 0,
			"mv_Buf(%d) start_adr 0x%x size 0x%x used %d\n",
			i,
			dec->m_mv_BUF[i].start_adr,
			dec->m_mv_BUF[i].size,
			dec->m_mv_BUF[i].used_flag);
	}

	avs2_print(dec, 0,
		"HEVC_DEC_STATUS_REG=0x%x\n",
		READ_VREG(HEVC_DEC_STATUS_REG));
	avs2_print(dec, 0,
		"HEVC_MPC_E=0x%x\n",
		READ_VREG(HEVC_MPC_E));
	avs2_print(dec, 0,
		"DECODE_MODE=0x%x\n",
		READ_VREG(DECODE_MODE));
	avs2_print(dec, 0,
		"NAL_SEARCH_CTL=0x%x\n",
		READ_VREG(NAL_SEARCH_CTL));
	avs2_print(dec, 0,
		"HEVC_PARSER_LCU_START=0x%x\n",
		READ_VREG(HEVC_PARSER_LCU_START));
	avs2_print(dec, 0,
		"HEVC_DECODE_SIZE=0x%x\n",
		READ_VREG(HEVC_DECODE_SIZE));
	avs2_print(dec, 0,
		"HEVC_SHIFT_BYTE_COUNT=0x%x\n",
		READ_VREG(HEVC_SHIFT_BYTE_COUNT));
	avs2_print(dec, 0,
		"HEVC_STREAM_START_ADDR=0x%x\n",
		READ_VREG(HEVC_STREAM_START_ADDR));
	avs2_print(dec, 0,
		"HEVC_STREAM_END_ADDR=0x%x\n",
		READ_VREG(HEVC_STREAM_END_ADDR));
	avs2_print(dec, 0,
		"HEVC_STREAM_LEVEL=0x%x\n",
		READ_VREG(HEVC_STREAM_LEVEL));
	avs2_print(dec, 0,
		"HEVC_STREAM_WR_PTR=0x%x\n",
		READ_VREG(HEVC_STREAM_WR_PTR));
	avs2_print(dec, 0,
		"HEVC_STREAM_RD_PTR=0x%x\n",
		READ_VREG(HEVC_STREAM_RD_PTR));
	avs2_print(dec, 0,
		"PARSER_VIDEO_RP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_rp));
	avs2_print(dec, 0,
		"PARSER_VIDEO_WP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_wp));

	if (input_frame_based(vdec) &&
		(debug & PRINT_FLAG_VDEC_DATA)) {
		int jj;
		if (dec->chunk && dec->chunk->block &&
			dec->chunk->size > 0) {
			u8 *data = NULL;
			if (!dec->chunk->block->is_mapped)
				data = codec_mm_vmap(dec->chunk->block->start +
					dec->chunk->offset, dec->chunk->size);
			else
				data = ((u8 *)dec->chunk->block->start_virt) +
					dec->chunk->offset;
			avs2_print(dec, 0, "frame data size 0x%x\n", dec->chunk->size);
			for (jj = 0; jj < dec->chunk->size; jj++) {
				if ((jj & 0xf) == 0)
					avs2_print(dec, 0, "%06x:", jj);
				avs2_print_cont(dec, 0, "%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					avs2_print_cont(dec, 0, "\n");
			}

			if (!dec->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		}
	}

}

static int ammvdec_avs2_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	int ret;
	int config_val;
	int i;
	struct vframe_content_light_level_s content_light_level;
	struct vframe_master_display_colour_s vf_dp;
	struct AVS2Decoder_s *dec = NULL;
	struct aml_vcodec_ctx *ctx = NULL;

	pr_info("%s\n", __func__);
	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D) {
		pr_info("%s, chip id %d is not support avs2\n",
			__func__, get_cpu_major_id());
		return -1;
	}
	if (pdata == NULL) {
		pr_info("\nammvdec_avs2 memory resource undefined.\n");
		return -EFAULT;
	}

	memset(&vf_dp, 0, sizeof(struct vframe_master_display_colour_s));
	dec = vmalloc(sizeof(struct AVS2Decoder_s));
	memset(dec, 0, sizeof(struct AVS2Decoder_s));
	if (dec == NULL) {
		pr_info("\nammvdec_avs2 device data allocation failed\n");
		return -ENOMEM;
	}
	if (pdata->parallel_dec == 1) {
		int i;
		for (i = 0; i < AVS2_MAX_BUFFER_NUM; i++) {
			dec->avs2_dec.frm_pool[i].y_canvas_index = -1;
			dec->avs2_dec.frm_pool[i].uv_canvas_index = -1;
		}
	}
	dec->v4l2_ctx = pdata->private;
	ctx = dec->v4l2_ctx;
	pdata->private = dec;
	pdata->dec_status = vavs2_dec_status;
#ifdef I_ONLY_SUPPORT
	pdata->set_trickmode = vavs2_set_trickmode;
#endif
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = avs2_irq_cb;
	pdata->threaded_irq_handler = avs2_threaded_irq_cb;
	pdata->dump_state = avs2_dump_state;

	dec->index = pdev->id;
	dec->m_ins_flag = 1;

	if (is_rdma_enable()) {
		dec->rdma_adr = dma_alloc_coherent(amports_get_dma_device(), RDMA_SIZE , &dec->rdma_phy_adr, GFP_KERNEL);
		for (i = 0; i < SCALELUT_DATA_WRITE_NUM; i++) {
			dec->rdma_adr[i * 4] = HEVC_IQIT_SCALELUT_WR_ADDR & 0xfff;
			dec->rdma_adr[i * 4 + 1] = i;
			dec->rdma_adr[i * 4 + 2] = HEVC_IQIT_SCALELUT_DATA & 0xfff;
			dec->rdma_adr[i * 4 + 3] = 0;
			if (i == SCALELUT_DATA_WRITE_NUM - 1) {
				dec->rdma_adr[i * 4 + 2] = (HEVC_IQIT_SCALELUT_DATA & 0xfff) | 0x20000;
			}
		}
	}

	snprintf(dec->vdec_name, sizeof(dec->vdec_name),
		"avs2-%d", dec->index);
	snprintf(dec->pts_name, sizeof(dec->pts_name),
		"%s-timestamp", dec->vdec_name);
	snprintf(dec->new_q_name, sizeof(dec->new_q_name),
		"%s-newframe_q", dec->vdec_name);
	snprintf(dec->disp_q_name, sizeof(dec->disp_q_name),
		"%s-dispframe_q", dec->vdec_name);

	if (pdata->use_vfm_path) {
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
		dec->frameinfo_enable = 1;
	} else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			MULTI_INSTANCE_PROVIDER_NAME ".%02x", pdev->id & 0xff);

	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vavs2_vf_provider, dec);

	dec->provider_name = pdata->vf_provider_name;
	platform_set_drvdata(pdev, pdata);

	dec->platform_dev = pdev;
	dec->video_signal_type = 0;
	dec->video_ori_signal_type = 0;
	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_TXLX)
		dec->stat |= VP9_TRIGGER_FRAME_ENABLE;

	if ((debug & IGNORE_PARAM_FROM_CONFIG) == 0 && pdata->config_len) {
		/*use ptr config for doubel_write_mode, etc*/
		avs2_print(dec, 0, "pdata->config=%s\n", pdata->config);
		if (get_config_int(pdata->config, "avs2_double_write_mode",
				&config_val) == 0)
			dec->double_write_mode = config_val;
		else
			dec->double_write_mode = double_write_mode;

		if (get_config_int(pdata->config, "parm_v4l_buffer_margin",
				&config_val) == 0)
			dec->dynamic_buf_margin = config_val;
		else
			dec->dynamic_buf_margin = 0;

		if (get_config_int(pdata->config, "sidebind_type",
				&config_val) == 0)
			dec->sidebind_type = config_val;

		if (get_config_int(pdata->config, "sidebind_channel_id",
				&config_val) == 0)
			dec->sidebind_channel_id = config_val;

		if (get_config_int(pdata->config, "parm_v4l_codec_enable",
				&config_val) == 0)
			dec->is_used_v4l = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_metadata_config_flag",
			&config_val) == 0) {
			dec->high_bandwidth_flag = config_val & VDEC_CFG_FLAG_HIGH_BANDWIDTH;
			if (dec->high_bandwidth_flag)
				avs2_print(dec, 0, "high bandwidth\n");
		}

		if (get_config_int(pdata->config, "HDRStaticInfo",
				&vf_dp.present_flag) == 0
				&& vf_dp.present_flag == 1) {
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
			dec->video_signal_type = (1 << 29)
					| (5 << 26)	/* unspecified */
					| (0 << 25)	/* limit */
					| (1 << 24)	/* color available */
					| (9 << 16)	/* 2020 */
					| (16 << 8)	/* 2084 */
					| (9 << 0);	/* 2020 */
		}
		dec->vf_dp = vf_dp;
	} else {
		dec->double_write_mode = double_write_mode;
		dec->dynamic_buf_margin = dynamic_buf_num_margin;
	}

	video_signal_type = dec->video_signal_type;

	if (amvdec_avs2_mmu_init(dec) < 0) {
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_NO_MEM);
		pr_err("avs2 alloc bmmu box failed!!\n");
		/* devm_kfree(&pdev->dev, (void *)dec); */
		vfree((void *)dec);
		return -1;
	}
	dec->cma_alloc_count = PAGE_ALIGN(work_buf_size) / PAGE_SIZE;
	ret = decoder_bmmu_box_alloc_buf_phy(dec->bmmu_box, WORK_SPACE_BUF_ID,
			dec->cma_alloc_count * PAGE_SIZE, DRIVER_NAME,
			&dec->cma_alloc_addr);
	if (ret < 0) {
		vdec_v4l_post_error_event(ctx, DECODER_EMERGENCY_NO_MEM);
		uninit_mmu_buffers(dec);
		/* devm_kfree(&pdev->dev, (void *)dec); */
		vfree((void *)dec);
		return ret;
	}
	dec->buf_start = dec->cma_alloc_addr;
	dec->buf_size = work_buf_size;

	dec->init_flag = 0;
	dec->first_sc_checked = 0;
	dec->fatal_error = 0;
	dec->show_frame_num = 0;
	dec->run_ready_min_buf_num = run_ready_min_buf_num;
	if (debug) {
		pr_info("===AVS2 decoder mem resource 0x%lx size 0x%x\n",
			dec->buf_start,
			dec->buf_size);
	}

	if (pdata->sys_info) {
		dec->vavs2_amstream_dec_info = *pdata->sys_info;
		dec->frame_width = dec->vavs2_amstream_dec_info.width;
		dec->frame_height = dec->vavs2_amstream_dec_info.height;
	} else {
		dec->vavs2_amstream_dec_info.width = 0;
		dec->vavs2_amstream_dec_info.height = 0;
		dec->vavs2_amstream_dec_info.rate = 30;
	}

	dec->endian = HEVC_CONFIG_LITTLE_ENDIAN;

	if (endian)
		dec->endian = endian;

	dec->cma_dev = pdata->cma_dev;
	if (vavs2_init(pdata) < 0) {
		pr_info("\namvdec_avs2 init failed.\n");
		avs2_local_uninit(dec);
		uninit_mmu_buffers(dec);
		/* devm_kfree(&pdev->dev, (void *)dec); */
		vfree((void *)dec);
		pdata->dec_status = NULL;
		return -ENODEV;
	}
	vdec_set_prepare_level(pdata, start_decode_buf_level);
	hevc_source_changed(VFORMAT_AVS2, 4096, 2048, 60);
	if (pdata->parallel_dec == 1)
		vdec_core_request(pdata, CORE_MASK_HEVC);
	else {
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
				| CORE_MASK_COMBINE);
	}

	return 0;
}

static int ammvdec_avs2_remove(struct platform_device *pdev)
{
	struct AVS2Decoder_s *dec = (struct AVS2Decoder_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct avs2_decoder *avs2_dec = &dec->avs2_dec;
	struct avs2_frame_s *pic;
	int i;

	if (debug)
		pr_info("amvdec_avs2_remove\n");

	vmavs2_stop(dec);

	if (pdata->parallel_dec == 1)
		vdec_core_release(hw_to_vdec(dec), CORE_MASK_HEVC);
	else
		vdec_core_release(hw_to_vdec(dec), CORE_MASK_HEVC);

	vdec_set_status(hw_to_vdec(dec), VDEC_STATUS_DISCONNECTED);
	if (pdata->parallel_dec == 1) {
		for (i = 0; i < AVS2_MAX_BUFFER_NUM; i++) {
			pdata->free_canvas_ex(dec->avs2_dec.frm_pool[i].y_canvas_index, pdata->id);
			pdata->free_canvas_ex(dec->avs2_dec.frm_pool[i].uv_canvas_index, pdata->id);
		}
	}

	for (i = 0; i < dec->used_buf_num; i++) {
		if (i == (dec->used_buf_num - 1))
			pic = avs2_dec->m_bg;
		else
			pic = avs2_dec->fref[i];
		release_cuva_data(pic);
	}


#ifdef DEBUG_PTS
	pr_info("pts missed %ld, pts hit %ld, duration %d\n",
		   dec->pts_missed, dec->pts_hit, dec->frame_dur);
#endif
	if (is_rdma_enable())
		dma_free_coherent(amports_get_dma_device(), RDMA_SIZE, dec->rdma_adr, dec->rdma_phy_adr);

	vfree((void *)dec);
	return 0;
}

static struct platform_driver ammvdec_avs2_driver = {
	.probe = ammvdec_avs2_probe,
	.remove = ammvdec_avs2_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = MULTI_DRIVER_NAME,
	}
};
#endif
static struct mconfig avs2_configs[] = {
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
};
static struct mconfig_node avs2_node;

static int __init amvdec_avs2_driver_init_module(void)
{

#ifdef AVS2_10B_MMU

	struct BuffInfo_s *p_buf_info;

	if (get_cpu_major_id() <= AM_MESON_CPU_MAJOR_ID_TM2 && !is_cpu_tm2_revb()) {
		if (vdec_is_support_4k()) {
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_SM1)
				p_buf_info = &amvavs2_workbuff_spec[2];
			else
				p_buf_info = &amvavs2_workbuff_spec[1];
		} else
			p_buf_info = &amvavs2_workbuff_spec[0];
	} else { //get_cpu_major_id() > AM_MESON_CPU_MAJOR_ID_TM2 || is_cpu_tm2_revb()
		if (vdec_is_support_4k()) {
			p_buf_info = &amvavs2_workbuff_spec[5];
		} else
			p_buf_info = &amvavs2_workbuff_spec[3];
	}

	init_buff_spec(NULL, p_buf_info);
	work_buf_size = (p_buf_info->end_adr - p_buf_info->start_adr + 0xffff) & (~0xffff);

#endif
	pr_debug("amvdec_avs2 module init\n");

#ifdef ERROR_HANDLE_DEBUG
	dbg_nal_skip_flag = 0;
	dbg_nal_skip_count = 0;
#endif
	udebug_flag = 0;
	decode_pic_begin = 0;
	slice_parse_begin = 0;
	step = 0;
	buf_alloc_size = 0;

	if (platform_driver_register(&ammvdec_avs2_driver)) {
		pr_err("failed to register ammvdec_avs2 driver\n");
		return -ENODEV;
	}

	if ((get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_G12A) ||
		(get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D)) {
		amvdec_avs2_profile.name = "avs2_unsupport";
	} else if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_SM1) {
		if (vdec_is_support_4k())
			amvdec_avs2_profile.profile =
				"4k, 10bit, dwrite, compressed, uvm, v4l";
		else
			amvdec_avs2_profile.profile =
				"10bit, dwrite, compressed, uvm, v4l";
	} else {
		/* cpu id larger than sm1 support 8k */
		amvdec_avs2_profile.profile =
				"8k, 10bit, dwrite, compressed, uvm, v4l";
	}

	vcodec_profile_register(&amvdec_avs2_profile);

	INIT_REG_NODE_CONFIGS("media.decoder", &avs2_node,
		"avs2-v4l", avs2_configs, CONFIG_FOR_RW);
	vcodec_feature_register(VFORMAT_AVS2, 1);

	return 0;
}

static void __exit amvdec_avs2_driver_remove_module(void)
{
	pr_debug("amvdec_avs2 module remove.\n");
	platform_driver_unregister(&ammvdec_avs2_driver);
}

/****************************************/

module_param(bit_depth_luma, uint, 0664);
MODULE_PARM_DESC(bit_depth_luma, "\n amvdec_avs2 bit_depth_luma\n");

module_param(bit_depth_chroma, uint, 0664);
MODULE_PARM_DESC(bit_depth_chroma, "\n amvdec_avs2 bit_depth_chroma\n");

module_param(frame_width, uint, 0664);
MODULE_PARM_DESC(frame_width, "\n amvdec_avs2 frame_width\n");

module_param(frame_height, uint, 0664);
MODULE_PARM_DESC(frame_height, "\n amvdec_avs2 frame_height\n");

module_param(debug, uint, 0664);
MODULE_PARM_DESC(debug, "\n amvdec_avs2 debug\n");

module_param(debug_again, uint, 0664);
MODULE_PARM_DESC(debug_again, "\n amvdec_avs2 debug_again\n");

module_param(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\nradr\n");

module_param(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\nrval\n");

module_param(pop_shorts, uint, 0664);
MODULE_PARM_DESC(pop_shorts, "\nrval\n");

module_param(dbg_cmd, uint, 0664);
MODULE_PARM_DESC(dbg_cmd, "\ndbg_cmd\n");

module_param(dbg_skip_decode_index, uint, 0664);
MODULE_PARM_DESC(dbg_skip_decode_index, "\ndbg_skip_decode_index\n");

module_param(endian, uint, 0664);
MODULE_PARM_DESC(endian, "\nrval\n");

module_param(step, uint, 0664);
MODULE_PARM_DESC(step, "\n amvdec_avs2 step\n");

module_param(decode_pic_begin, uint, 0664);
MODULE_PARM_DESC(decode_pic_begin, "\n amvdec_avs2 decode_pic_begin\n");

module_param(slice_parse_begin, uint, 0664);
MODULE_PARM_DESC(slice_parse_begin, "\n amvdec_avs2 slice_parse_begin\n");

module_param(i_only_flag, uint, 0664);
MODULE_PARM_DESC(i_only_flag, "\n amvdec_avs2 i_only_flag\n");

module_param(error_handle_policy, uint, 0664);
MODULE_PARM_DESC(error_handle_policy, "\n amvdec_avs2 error_handle_policy\n");

module_param(re_search_seq_threshold, uint, 0664);
MODULE_PARM_DESC(re_search_seq_threshold, "\n amvdec_avs2 re_search_seq_threshold\n");

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

#ifdef CONSTRAIN_MAX_BUF_NUM
module_param(run_ready_max_vf_only_num, uint, 0664);
MODULE_PARM_DESC(run_ready_max_vf_only_num, "\n run_ready_max_vf_only_num\n");

module_param(run_ready_display_q_num, uint, 0664);
MODULE_PARM_DESC(run_ready_display_q_num, "\n run_ready_display_q_num\n");

module_param(run_ready_max_buf_num, uint, 0664);
MODULE_PARM_DESC(run_ready_max_buf_num, "\n run_ready_max_buf_num\n");
#endif

module_param(mv_buf_margin, uint, 0664);
MODULE_PARM_DESC(mv_buf_margin, "\n mv_buf_margin\n");

module_param(run_ready_min_buf_num, uint, 0664);
MODULE_PARM_DESC(run_ready_min_buf_num, "\n run_ready_min_buf_num\n");

/**/

module_param(mem_map_mode, uint, 0664);
MODULE_PARM_DESC(mem_map_mode, "\n mem_map_mode\n");

module_param(double_write_mode, uint, 0664);
MODULE_PARM_DESC(double_write_mode, "\n double_write_mode\n");

module_param(enable_mem_saving, uint, 0664);
MODULE_PARM_DESC(enable_mem_saving, "\n enable_mem_saving\n");

module_param(force_w_h, uint, 0664);
MODULE_PARM_DESC(force_w_h, "\n force_w_h\n");

module_param(force_fps, uint, 0664);
MODULE_PARM_DESC(force_fps, "\n force_fps\n");

module_param(max_decoding_time, uint, 0664);
MODULE_PARM_DESC(max_decoding_time, "\n max_decoding_time\n");

module_param(on_no_keyframe_skiped, uint, 0664);
MODULE_PARM_DESC(on_no_keyframe_skiped, "\n on_no_keyframe_skiped\n");


module_param(start_decode_buf_level, int, 0664);
MODULE_PARM_DESC(start_decode_buf_level,
		"\n avs2 start_decode_buf_level\n");

module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val,
	"\n avs2 decode_timeout_val\n");

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

module_param(video_signal_type, uint, 0664);
MODULE_PARM_DESC(video_signal_type, "\n amvdec_avs2 video_signal_type\n");

module_param(force_video_signal_type, uint, 0664);
MODULE_PARM_DESC(force_video_signal_type, "\n amvdec_avs2 force_video_signal_type\n");

module_param(enable_force_video_signal_type, uint, 0664);
MODULE_PARM_DESC(enable_force_video_signal_type, "\n amvdec_avs2 enable_force_video_signal_type\n");

module_param(force_bufspec, uint, 0664);
MODULE_PARM_DESC(force_bufspec, "\n amvdec_h265 force_bufspec\n");

module_param(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n amvdec_h265 udebug_flag\n");

module_param(udebug_pause_pos, uint, 0664);
MODULE_PARM_DESC(udebug_pause_pos, "\n udebug_pause_pos\n");

module_param(udebug_pause_val, uint, 0664);
MODULE_PARM_DESC(udebug_pause_val, "\n udebug_pause_val\n");

module_param(udebug_pause_decode_idx, uint, 0664);
MODULE_PARM_DESC(udebug_pause_decode_idx, "\n udebug_pause_decode_idx\n");

module_param(pre_decode_buf_level, int, 0664);
MODULE_PARM_DESC(pre_decode_buf_level,
		"\n amvdec_avs2 pre_decode_buf_level\n");

module_param(again_threshold, uint, 0664);
MODULE_PARM_DESC(again_threshold, "\n again_threshold\n");


module_param(force_disp_pic_index, int, 0664);
MODULE_PARM_DESC(force_disp_pic_index,
	"\n amvdec_h265 force_disp_pic_index\n");

module_param(without_display_mode, uint, 0664);
MODULE_PARM_DESC(without_display_mode, "\n without_display_mode\n");

module_param(mv_buf_dynamic_alloc, uint, 0664);
MODULE_PARM_DESC(mv_buf_dynamic_alloc, "\n mv_buf_dynamic_alloc\n");

module_init(amvdec_avs2_driver_init_module);
module_exit(amvdec_avs2_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC avs2 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <tim.yao@amlogic.com>");
