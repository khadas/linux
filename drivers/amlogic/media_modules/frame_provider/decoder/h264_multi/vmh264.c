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
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>
#include <linux/random.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include "../../../stream_input/amports/amports_priv.h"
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "../utils/vdec_input.h"
//#include <linux/amlogic/tee.h>
#include <uapi/linux/tee.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include "../utils/vdec.h"
#include "../utils/amvdec.h"
#include "../h264/vh264.h"
#include "../../../stream_input/amports/streambuf.h"
#include <linux/delay.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include "../utils/decoder_mmu_box.h"
#include "../utils/decoder_bmmu_box.h"
#include "../utils/firmware.h"
#include <linux/uaccess.h>
#include "../utils/config_parser.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "../utils/vdec_v4l2_buffer_ops.h"
#include <linux/crc32.h>
#include <media/v4l2-mem2mem.h>
#include "../utils/vdec_feature.h"
#include "../utils/decoder_dma_alloc.h"

#define DETECT_WRONG_MULTI_SLICE

/*
to enable DV of frame mode
#define DOLBY_META_SUPPORT in ucode
*/

#undef pr_info
#define pr_info pr_cont
#define VDEC_DW
#define DEBUG_UCODE
#define MEM_NAME "codec_m264"
#define MULTI_INSTANCE_FRAMEWORK
/* #define ONE_COLOCATE_BUF_PER_DECODE_BUF */
#include "h264_dpb.h"
/* #define SEND_PARAM_WITH_REG */

#define DRIVER_NAME "ammvdec_h264"
#define DRIVER_HEADER_NAME "ammvdec_h264_header"

#define CHECK_INTERVAL        (HZ/100)

#define SEI_DATA_SIZE			(8*1024)
#define SEI_ITU_DATA_SIZE		(4*1024)

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

#define DECODE_ID(hw) (hw_to_vdec(hw)->id)

#define RATE_MEASURE_NUM 8
#define RATE_CORRECTION_THRESHOLD 5
#define RATE_24_FPS  4004	/* 23.97 */
#define RATE_25_FPS  3840	/* 25 */
#define DUR2PTS(x) ((x)*90/96)
#define PTS2DUR(x) ((x)*96/90)
#define DUR2PTS_REM(x) (x*90 - DUR2PTS(x)*96)
#define FIX_FRAME_RATE_CHECK_IDRFRAME_NUM 2

#define ALIGN_WIDTH(x) (ALIGN((x), 64))
#define ALIGN_HEIGHT(x) (ALIGN((x), 32))

#define H264_DEV_NUM        9

#define CONSTRAIN_MAX_BUF_NUM

#define H264_MMU
#define VIDEO_SIGNAL_TYPE_AVAILABLE_MASK	0x20000000
#define INVALID_IDX -1  /* Invalid buffer index.*/

#define H264_ERROR_FRAME_DISPLAY 0x80001BD5
#define H264_ERROR_FRAME_DROP 0x7fCfb6

static int mmu_enable;
/*mmu do not support mbaff*/
static int force_enable_mmu = 0;
unsigned int h264_debug_flag; /* 0xa0000000; */
unsigned int h264_debug_mask = 0xff;
	/*
	 *h264_debug_cmd:
	 *	0x1xx, force decoder id of xx to be disconnected
	 */
unsigned int h264_debug_cmd;

static int ref_b_frame_error_max_count = 50;

static unsigned int dec_control =
	DEC_CONTROL_FLAG_FORCE_2997_1080P_INTERLACE |
	DEC_CONTROL_FLAG_FORCE_2500_576P_INTERLACE;

static unsigned int force_rate_streambase;
static unsigned int force_rate_framebase;
static unsigned int force_disp_bufspec_num;
static unsigned int fixed_frame_rate_mode;
static unsigned int error_recovery_mode_in;
static int start_decode_buf_level = 0x4000;
static int pre_decode_buf_level = 0x1000;
static int stream_mode_start_num = 4;
static int dirty_again_threshold = 100;
static unsigned int colocate_old_cal;


static int check_dirty_data(struct vdec_s *vdec);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
/*to make reorder size difference of bl and el not too big*/
static unsigned int reorder_dpb_size_margin_dv = 16;
#endif
static unsigned int reorder_dpb_size_margin = 6;
static unsigned int reference_buf_margin = 4;

#ifdef CONSTRAIN_MAX_BUF_NUM
static u32 run_ready_max_vf_only_num;
static u32 run_ready_display_q_num;
	/*0: not check
	  0xff: mDPB.size
	  */
static u32 run_ready_max_buf_num = 0xff;
#endif

static u32 run_ready_min_buf_num = 2;

#define VDEC_ASSIST_CANVAS_BLK32		0x5


static unsigned int max_alloc_buf_count;
static unsigned int decode_timeout_val = 100;
static unsigned int errordata_timeout_val = 50;
static unsigned int get_data_timeout_val = 2000;
#if 1
/* H264_DATA_REQUEST does not work, disable it,
decode has error for data in none continuous address
*/
static unsigned int frame_max_data_packet;
#else
static unsigned int frame_max_data_packet = 8;
#endif
static unsigned int radr;
static unsigned int rval;
static u32 endian = 0xff0;

/*
	udebug_flag:
	bit 0, enable ucode print
	bit 1, enable ucode detail print
	bit 3, disable ucode watchdog
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

static unsigned int disp_vframe_valve_level;

static unsigned int max_decode_instance_num = H264_DEV_NUM;
static unsigned int decode_frame_count[H264_DEV_NUM];
static unsigned int display_frame_count[H264_DEV_NUM];
static unsigned int max_process_time[H264_DEV_NUM];
static unsigned int max_get_frame_interval[H264_DEV_NUM];
static unsigned int run_count[H264_DEV_NUM];
static unsigned int input_empty[H264_DEV_NUM];
static unsigned int not_run_ready[H264_DEV_NUM];
static unsigned int ref_frame_mark_flag[H264_DEV_NUM] =
{1, 1, 1, 1, 1, 1, 1, 1, 1};

#define VDEC_CLOCK_ADJUST_FRAME 30
static unsigned int clk_adj_frame_count;

/*
 *bit[3:0]: 0, run ; 1, pause; 3, step
 *bit[4]: 1, schedule run
 */
static unsigned int step[H264_DEV_NUM];

#define AUX_BUF_ALIGN(adr) ((adr + 0xf) & (~0xf))
static u32 prefix_aux_buf_size = (16 * 1024);
static u32 suffix_aux_buf_size;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
static u32 dv_toggle_prov_name;

static u32 dolby_meta_with_el;
#endif

/*
	bit[8]
		0: use sys_info[bit 3]
		not 0:use i_only_flag[7:0]
			bit[7:0]:
				bit 0, 1: only display I picture;
				bit 1, 1: only decode I picture;
*/
static unsigned int i_only_flag;

/*
	error_proc_policy:
	bit[0] send_error_frame_flag;
		(valid when bit[31] is 1, otherwise use sysinfo)
	bit[1] do not decode if config_decode_buf() fail
	bit[2] force release buf if in deadlock
	bit[3] force sliding window ref_frames_in_buffer > num_ref_frames
	bit[4] check inactive of receiver
	bit[5] reset buffmgr if in deadlock
	bit[6] reset buffmgr if bufspec, collocate buf, pic alloc fail
	bit[7] reset buffmgr if dpb error

	bit[8] check total mbx/mby of decoded frame
	bit[9] check ERROR_STATUS_REG
	bit[10] check reference list
	bit[11] mark error if dpb error
	bit[12] i_only when error happen
	bit[13] 0: mark error according to last pic, 1: ignore mark error
	bit[14] 0: result done when timeout from ucode. 1: reset bufmgr when timeout.
	bit[15] 1: dpb_frame_count If the dpb_frame_count difference is large, it moves out of the DPB buffer.
	bit[16] 1: check slice header number.
	bit[17] 1: If the decoded Mb count is insufficient but greater than the threshold, it is considered the correct frame.
	bit[18] 1: time out status, store pic to dpb buffer.
	bit[19] 1: If a lot b frames are wrong consecutively, the DPB queue reset.
	bit[20] 1: fixed some error stream will lead to the diffusion of the error, resulting playback stuck.
	bit[21] 1: fixed DVB loop playback cause jetter issue.
	bit[22] 1: In streaming mode, support for discarding data.
	bit[23] 0: set error flag on frame number gap error and drop it, 1: ignore error.
*/
static unsigned int error_proc_policy = 0x7fCfb6; /*0x1f14*/


/*
	error_skip_count:
	bit[11:0] error skip frame count
	bit[15:12] error skip i picture count
*/
static unsigned int error_skip_count = (0x2 << 12) | 0x40;

static unsigned int force_sliding_margin;
/*
	bit[1:0]:
	0, start playing from any frame
	1, start playing from I frame
		bit[15:8]: the count of skip frames after first I
	2, start playing from second I frame (decode from the first I)
		bit[15:8]: the max count of skip frames after first I
	3, start playing from IDR
*/
static unsigned int first_i_policy = 1;

/*
	fast_output_enable:
	bit [0], output frame if there is IDR in list
	bit [1], output frame if the current poc is 1 big than the previous poc
	bit [2], if even poc only, output frame ifthe cuurent poc
			is 2 big than the previous poc
	bit [3],  ip only
*/
static unsigned int fast_output_enable = H264_OUTPUT_MODE_NORMAL;

static unsigned int enable_itu_t35 = 1;

static unsigned int frmbase_cont_bitlevel = 0x40;

static unsigned int frmbase_cont_bitlevel2 = 0x1;

static unsigned int check_slice_num = 30;

static unsigned int mb_count_threshold = 5; /*percentage*/

#define MH264_USERDATA_ENABLE

/* DOUBLE_WRITE_MODE is enabled only when NV21 8 bit output is needed */
/* hevc->double_write_mode:
	0, no double write
	1, 1:1 ratio
	2, (1/4):(1/4) ratio
	3, (1/4):(1/4) ratio, with both compressed frame included
	4, (1/2):(1/2) ratio
	0x10, double write only
	0x10000: vdec dw horizotal 1/2
	0x20000: vdec dw horizotal/vertical  1/2
*/
static u32 double_write_mode;
static u32 without_display_mode;

static int loop_playback_poc_threshold = 400;
static int poc_threshold = 50;

static u32 lookup_check_conut = 30;


/*
 *[3:0] 0: default use config from omx.
 *      1: force enable fence.
 *      2: disable fence.
 *[7:4] 0: fence use for driver.
 *      1: fence fd use for app.
 */
static u32 force_config_fence;

#define IS_VDEC_DW(hw)  (hw->double_write_mode >> 16 & 0xf)

static void vmh264_dump_state(struct vdec_s *vdec);

#define is_in_parsing_state(status) \
		((status == H264_ACTION_SEARCH_HEAD) || \
			((status & 0xf0) == 0x80))

#define is_interlace(frame)	\
			((frame->frame &&\
			frame->top_field &&\
			frame->bottom_field &&\
			(!frame->frame->coded_frame)) || \
			(frame->frame && \
			 frame->frame->coded_frame && \
			 (!frame->frame->frame_mbs_only_flag) && \
			 frame->frame->structure == FRAME))

static inline bool close_to(int a, int b, int m)
{
	return (abs(a - b) < m) ? true : false;
}

#if 0
#define h264_alloc_hw_stru(dev, size, opt) devm_kzalloc(dev, size, opt)
#define h264_free_hw_stru(dev, hw) devm_kfree(dev, hw)
#else
#define h264_alloc_hw_stru(dev, size, opt) vzalloc(size)
#define h264_free_hw_stru(dev, hw) vfree(hw)
#endif

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#define NV21
/* #endif */

/* 12M for L41 */
#define MAX_DPB_BUFF_SIZE       (12*1024*1024)
#define DEFAULT_MEM_SIZE        (32*1024*1024)
#define AVIL_DPB_BUFF_SIZE      0x01ec2000

#define DEF_BUF_START_ADDR			0x00000000
#define mem_sps_base				0x01c3c00
#define mem_pps_base				0x01cbc00
/*#define V_BUF_ADDR_OFFSET             (0x13e000)*/
u32 V_BUF_ADDR_OFFSET = 0x200000;
#define DCAC_READ_MARGIN	(64 * 1024)


#define EXTEND_SAR                      0xff
#define BUFSPEC_POOL_SIZE		64
#define VF_POOL_SIZE        64
#define VF_POOL_NUM			2
#define MAX_VF_BUF_NUM          27
#define BMMU_MAX_BUFFERS	(BUFSPEC_POOL_SIZE + 3)
#define BMMU_REF_IDX	(BUFSPEC_POOL_SIZE)
#define BMMU_DPB_IDX	(BUFSPEC_POOL_SIZE + 1)
#define BMMU_EXTIF_IDX	(BUFSPEC_POOL_SIZE + 2)
#define EXTIF_BUF_SIZE   (0x10000 * 2)

#define HEADER_BUFFER_IDX(n) (n)
#define VF_BUFFER_IDX(n)	(n)


#define PUT_INTERVAL        (HZ/100)
#define NO_DISP_WD_COUNT    (3 * HZ / PUT_INTERVAL)

#define MMU_MAX_BUFFERS	BUFSPEC_POOL_SIZE
#define SWITCHING_STATE_OFF       0
#define SWITCHING_STATE_ON_CMD3   1
#define SWITCHING_STATE_ON_CMD1   2



#define INCPTR(p) ptr_atomic_wrap_inc(&p)

#define SLICE_TYPE_I 2
#define SLICE_TYPE_P 5
#define SLICE_TYPE_B 6

struct buffer_spec_s {
	/*
	used:
	-1, none allocated
	0, allocated, free
	1, used by dpb
	2, in disp queue;
	3, in disp queue, isolated,
		do not use for dpb when vf_put;
	4, to release
	5, in disp queue, isolated (but not to release)
		do not use for dpb when vf_put;
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

#ifdef VDEC_DW
	unsigned int vdec_dw_y_addr;
	unsigned int vdec_dw_u_addr;
	unsigned int vdec_dw_v_addr;

	int vdec_dw_y_canvas_index;
	int vdec_dw_u_canvas_index;
	int vdec_dw_v_canvas_index;
#ifdef NV21
	struct canvas_config_s vdec_dw_canvas_config[2];
#else
	struct canvas_config_s vdec_dw_canvas_config[3];
#endif
#endif

#ifdef NV21
	struct canvas_config_s canvas_config[2];
#else
	struct canvas_config_s canvas_config[3];
#endif
	unsigned long cma_alloc_addr;
	unsigned int buf_adr;
#ifdef	H264_MMU
	unsigned long alloc_header_addr;
#endif
	char *aux_data_buf;
	int aux_data_size;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	unsigned char dv_enhance_exist;
#endif
	int canvas_pos;
	int vf_ref;
	/*unsigned int comp_body_size;*/
	unsigned int dw_y_adr;
	unsigned int dw_u_v_adr;
	int fs_idx;
	char*  user_data_buf;
	struct userdata_param_t ud_param;
};

#define AUX_DATA_SIZE(pic) (hw->buffer_spec[pic->buf_spec_num].aux_data_size)
#define AUX_DATA_BUF(pic) (hw->buffer_spec[pic->buf_spec_num].aux_data_buf)
#define DEL_EXIST(h, p) (h->buffer_spec[p->buf_spec_num].dv_enhance_exist)


#define vdec_dw_spec2canvas(x)  \
	(((x)->vdec_dw_v_canvas_index << 16) | \
	 ((x)->vdec_dw_u_canvas_index << 8)  | \
	 ((x)->vdec_dw_y_canvas_index << 0))


#define spec2canvas(x)  \
	(((x)->v_canvas_index << 16) | \
	 ((x)->u_canvas_index << 8)  | \
	 ((x)->y_canvas_index << 0))

#define FRAME_INDEX(vf_index) (vf_index & 0xff)
#define BUFSPEC_INDEX(vf_index) ((vf_index >> 8) & 0xff)
#define VF_INDEX(frm_idx, bufspec_idx) (frm_idx | (bufspec_idx << 8))

static struct vframe_s *vh264_vf_peek(void *);
static struct vframe_s *vh264_vf_get(void *);
static void vh264_vf_put(struct vframe_s *, void *);
static int vh264_vf_states(struct vframe_states *states, void *);
static int vh264_event_cb(int type, void *data, void *private_data);
static void vh264_work(struct work_struct *work);
static void vh264_timeout_work(struct work_struct *work);
static void vh264_notify_work(struct work_struct *work);
#ifdef MH264_USERDATA_ENABLE
static void user_data_ready_notify_work(struct work_struct *work);
static void vmh264_wakeup_userdata_poll(struct vdec_s *vdec);
#endif

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
#define DEC_RESULT_EOS              7
#define DEC_RESULT_FORCE_EXIT       8
#define DEC_RESULT_TIMEOUT			9


/*
 *static const char *dec_result_str[] = {
 *    "DEC_RESULT_NONE        ",
 *    "DEC_RESULT_DONE        ",
 *    "DEC_RESULT_AGAIN       ",
 *    "DEC_RESULT_CONFIG_PARAM",
 *    "DEC_RESULT_GET_DATA    ",
 *    "DEC_RESULT_GET_DA_RETRY",
 *    "DEC_RESULT_ERROR       ",
 *};
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
#define H264_DECODE_MODE    AV_SCRATCH_4
#define H264_DECODE_SEQINFO	AV_SCRATCH_5
#define H264_AUX_ADR            AV_SCRATCH_C
#define H264_AUX_DATA_SIZE      AV_SCRATCH_H

#define H264_DECODE_INFO          M4_CONTROL_REG /* 0xc29 */
#define DPB_STATUS_REG       AV_SCRATCH_J
#define ERROR_STATUS_REG	AV_SCRATCH_9
	/*
	NAL_SEARCH_CTL: bit 0, enable itu_t35
	NAL_SEARCH_CTL: bit 1, enable mmu
	NAL_SEARCH_CTL: bit 2, detect frame_mbs_only_flag whether switch resolution
	NAL_SEARCH_CTL: bit 7-14,level_idc
	NAL_SEARCH_CTL: bit 15,bitstream_restriction_flag
	*/
#define NAL_SEARCH_CTL		AV_SCRATCH_9
#define MBY_MBX                 MB_MOTION_MODE /*0xc07*/

#define DECODE_MODE_SINGLE					0x0
#define DECODE_MODE_MULTI_FRAMEBASE			0x1
#define DECODE_MODE_MULTI_STREAMBASE		0x2
#define DECODE_MODE_MULTI_DVBAL				0x3
#define DECODE_MODE_MULTI_DVENL				0x4
static DEFINE_MUTEX(vmh264_mutex);

#ifdef MH264_USERDATA_ENABLE

struct mh264_userdata_record_t {
	struct userdata_meta_info_t meta_info;
	u32 rec_start;
	u32 rec_len;
};

struct mh264_ud_record_wait_node_t {
	struct list_head list;
	struct mh264_userdata_record_t ud_record;
};
#define USERDATA_FIFO_NUM    256
#define MAX_FREE_USERDATA_NODES		5

struct mh264_userdata_info_t {
	struct mh264_userdata_record_t records[USERDATA_FIFO_NUM];
	u8 *data_buf;
	u8 *data_buf_end;
	u32 buf_len;
	u32 read_index;
	u32 write_index;
	u32 last_wp;
};


#endif

struct mh264_fence_vf_t {
	u32 used_size;
	struct vframe_s *fence_vf[VF_POOL_SIZE];
};

struct vdec_h264_hw_s {
	spinlock_t lock;
	spinlock_t bufspec_lock;
	int id;
	struct platform_device *platform_dev;
	unsigned long cma_alloc_addr;
	/* struct page *collocate_cma_alloc_pages; */
	unsigned long collocate_cma_alloc_addr;

	u32 prefix_aux_size;
	u32 suffix_aux_size;
	void *aux_addr;
	dma_addr_t aux_phy_addr;

	/* buffer for store all sei data */
	void *sei_data_buf;
	u32	sei_data_len;

	/* buffer for storing one itu35 recored */
	void *sei_itu_data_buf;
	u32 sei_itu_data_len;

	/* recycle buffer for user data storing all itu35 records */
	void *sei_user_data_buffer;
	u32 sei_user_data_wp;
#ifdef MH264_USERDATA_ENABLE
	struct work_struct user_data_ready_work;
#endif
	struct StorablePicture *last_dec_picture;

	ulong lmem_phy_addr;
	dma_addr_t lmem_addr;

	void *bmmu_box;
#ifdef H264_MMU
	void *mmu_box;
	void *frame_mmu_map_addr;
	dma_addr_t frame_mmu_map_phy_addr;
	u32	 hevc_cur_buf_idx;
	u32 losless_comp_body_size;
	u32 losless_comp_body_size_sao;
	u32 losless_comp_header_size;
	u32 mc_buffer_size_u_v;
	u32 mc_buffer_size_u_v_h;
	u32  is_idr_frame;
	u32  is_new_pic;
	u32  frame_done;
	u32  frame_busy;
	unsigned long extif_addr;
	int double_write_mode;
	int mmu_enable;
	int error_proc_policy;
#endif

	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);

	int cur_pool;
	struct vframe_s vfpool[VF_POOL_NUM][VF_POOL_SIZE];
	struct buffer_spec_s buffer_spec[BUFSPEC_POOL_SIZE];
	struct vframe_s switching_fense_vf;
	struct h264_dpb_stru dpb;
	u8 init_flag;
	u8 first_sc_checked;
	u8 has_i_frame;
	u8 config_bufmgr_done;
	u32 max_reference_size;
	u32 decode_pic_count;
	u32 reflist_error_count;
	int start_search_pos;
	u32 reg_iqidct_control;
	bool reg_iqidct_control_init_flag;
	u32 reg_vcop_ctrl_reg;
	u32 reg_rv_ai_mb_count;
	u32 vld_dec_control;
	struct vframe_s vframe_dummy;

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
	u32 seq_info2;
	u32 seq_info3;
	u32 video_signal_from_vui; /*to do .. */
	u32 timing_info_present_flag;
	u32 fixed_frame_rate_flag;
	u32 bitstream_restriction_flag;
	u32 num_reorder_frames;
	u32 max_dec_frame_buffering;
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
	u32 pts_unstable;
	u32 unstable_pts;
	u32 last_checkout_pts;
	u32 max_refer_buf;

	s32 vh264_stream_switching_state;
	struct vframe_s *p_last_vf;
	u32 last_pts;
	u32 last_pts_remainder;
	u32 last_duration;
	u32 last_mb_width, last_mb_height;
	bool check_pts_discontinue;
	bool pts_discontinue;
	u32 wait_buffer_counter;
	u32 first_offset;
	u32 first_pts;
	u64 first_pts64;
	bool first_pts_cached;
	u64 last_pts64;
#if 0
	void *sei_data_buffer;
	dma_addr_t sei_data_buffer_phys;
#endif

	uint error_recovery_mode;
	uint mb_total;
	uint mb_width;
	uint mb_height;

	uint i_only;
	int skip_frame_count;
	bool no_poc_reorder_flag;
	bool send_error_frame_flag;
	dma_addr_t mc_dma_handle;
	void *mc_cpu_addr;
	int vh264_reset;

	atomic_t vh264_active;

	struct dec_sysinfo vh264_amstream_dec_info;

	int dec_result;
	u32 timeout_processing;
	struct work_struct work;
	struct work_struct notify_work;
	struct work_struct timeout_work;
	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;

	struct timer_list check_timer;

	/**/
	unsigned int last_frame_time;
	u32 vf_pre_count;
	atomic_t vf_get_count;
	atomic_t vf_put_count;

	/* timeout handle */
	unsigned long int start_process_time;
	unsigned int last_mby_mbx;
	unsigned int last_vld_level;
	unsigned int decode_timeout_count;
	unsigned int timeout_num;
	unsigned int search_dataempty_num;
	unsigned int decode_timeout_num;
	unsigned int decode_dataempty_num;
	unsigned int buffer_empty_recover_num;

	unsigned get_data_count;
	unsigned get_data_start_time;
	/**/

	/*log*/
	unsigned int packet_write_success_count;
	unsigned int packet_write_EAGAIN_count;
	unsigned int packet_write_ENOMEM_count;
	unsigned int packet_write_EFAULT_count;
	unsigned int total_read_size_pre;
	unsigned int total_read_size;
	unsigned int frame_count_pre;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	u8 switch_dvlayer_flag;
	u8 got_valid_nal;
#endif
	u8 eos;
	u8 data_flag;
	u32 no_error_count;
	u32 no_error_i_count;
	/*
	NODISP_FLAG
	*/
	u8 dec_flag;

	u32 ucode_pause_pos;

	u8 reset_bufmgr_flag;
	u32 reset_bufmgr_count;
	ulong timeout;
	u32 timeout_flag;
	u32 cfg_param1;
	u32 cfg_param2;
	u32 cfg_param3;
	u32 cfg_param4;
	int valve_count;
	u8 next_again_flag;
	u32 pre_parser_wr_ptr;
	struct firmware_s *fw;
	struct firmware_s *fw_mmu;
#ifdef MH264_USERDATA_ENABLE
	/*user data*/
	struct mutex userdata_mutex;
	struct mh264_userdata_info_t userdata_info;
	struct mh264_userdata_record_t ud_record;
	int wait_for_udr_send;
#endif
	u32 no_mem_count;
	u32 canvas_mode;
	bool is_used_v4l;
	void *v4l2_ctx;
	bool v4l_params_parsed;
	wait_queue_head_t wait_q;
	u32 reg_g_status;
	struct mutex chunks_mutex;
	int need_cache_size;
	u64 sc_start_time;
	u8 frmbase_cont_flag;
	struct vframe_qos_s vframe_qos;
	int frameinfo_enable;
	bool first_head_check_flag;
	unsigned int height_aspect_ratio;
	unsigned int width_aspect_ratio;
	unsigned int first_i_policy;
	u32 reorder_dpb_size_margin;
	bool wait_reset_done_flag;
#ifdef DETECT_WRONG_MULTI_SLICE
	unsigned int multi_slice_pic_check_count;
			/* multi_slice_pic_flag:
				0, unknown;
				1, single slice;
				2, multi slice
			*/
	unsigned int multi_slice_pic_flag;
	unsigned int picture_slice_count;
	unsigned int cur_picture_slice_count;
	unsigned char force_slice_as_picture_flag;
	unsigned int last_picture_slice_count;
	unsigned int first_pre_frame_num;
#endif
	u32 res_ch_flag;
	u32 b_frame_error_count;
	struct vdec_info gvs;
	u32 kpi_first_i_comming;
	u32 kpi_first_i_decoded;
	int sidebind_type;
	int sidebind_channel_id;
	u32 low_latency_mode;
	int ip_field_error_count;
	int buffer_wrap[BUFSPEC_POOL_SIZE];
	int loop_flag;
	int loop_last_poc;
	bool enable_fence;
	int fence_usage;
	bool discard_dv_data;
	u32 metadata_config_flag;
	int vdec_pg_enable_flag;
	u32 save_reg_f;
	u32 start_bit_cnt;
	u32 right_frame_count;
	u32 wrong_frame_count;
	u32 error_frame_width;
	u32 error_frame_height;
	ulong fb_token;
	int dec_again_cnt;
	struct mh264_fence_vf_t fence_vf_s;
	struct mutex fence_mutex;
	u32 no_decoder_buffer_flag;
	u32 video_signal_type;
	struct trace_decoder_name trace;
	bool high_bandwidth_flag;
	ulong lmem_phy_handle;
	ulong aux_mem_handle;
	ulong frame_mmu_map_handle;
	ulong mc_cpu_handle;
	struct mutex pic_mutex;
};

static u32 again_threshold;

static void timeout_process(struct vdec_h264_hw_s *hw);
static void dump_bufspec(struct vdec_h264_hw_s *hw,
	const char *caller);
static void h264_reconfig(struct vdec_h264_hw_s *hw);
static void h264_reset_bufmgr(struct vdec_s *vdec);
static void vh264_local_init(struct vdec_h264_hw_s *hw, bool is_reset);
static int vh264_hw_ctx_restore(struct vdec_h264_hw_s *hw);
static int vh264_stop(struct vdec_h264_hw_s *hw);
static s32 vh264_init(struct vdec_h264_hw_s *hw);
static void set_frame_info(struct vdec_h264_hw_s *hw, struct vframe_s *vf,
			u32 index);
static void release_aux_data(struct vdec_h264_hw_s *hw,
	int buf_spec_num);
#ifdef ERROR_HANDLE_TEST
static void h264_clear_dpb(struct vdec_h264_hw_s *hw);
#endif

#define		H265_PUT_SAO_4K_SET			0x03
#define		H265_ABORT_SAO_4K_SET			0x04
#define		H265_ABORT_SAO_4K_SET_DONE		0x05

#define		SYS_COMMAND			HEVC_ASSIST_SCRATCH_0
#define		H265_CHECK_AXI_INFO_BASE	HEVC_ASSIST_SCRATCH_8
#define		H265_SAO_4K_SET_BASE	HEVC_ASSIST_SCRATCH_9
#define		H265_SAO_4K_SET_COUNT	HEVC_ASSIST_SCRATCH_A
#define		HEVCD_MPP_ANC2AXI_TBL_DATA		0x3464


#define		HEVC_CM_HEADER_START_ADDR		0x3628
#define		HEVC_CM_BODY_START_ADDR			0x3626
#define		HEVC_CM_BODY_LENGTH			0x3627
#define		HEVC_CM_HEADER_LENGTH			0x3629
#define		HEVC_CM_HEADER_OFFSET			0x362b
#define		HEVC_SAO_CTRL9				0x362d
#define		HEVCD_MPP_DECOMP_CTL3			0x34c4
#define		HEVCD_MPP_VDEC_MCR_CTL			0x34c8
#define           HEVC_DBLK_CFGB                             0x350b
#define		HEVC_ASSIST_MMU_MAP_ADDR	0x3009

#define H265_DW_NO_SCALE
#define H265_MEM_MAP_MODE 0  /*0:linear 1:32x32 2:64x32*/
#define H265_LOSLESS_COMPRESS_MODE
#define MAX_FRAME_4K_NUM 0x1200
#define FRAME_MMU_MAP_SIZE  (MAX_FRAME_4K_NUM * 4)

/* 0:linear 1:32x32 2:64x32 ; m8baby test1902 */
static u32 mem_map_mode = H265_MEM_MAP_MODE;

#define MAX_SIZE_4K (4096 * 2304)
#define MAX_SIZE_2K (1920 * 1088)

static int is_oversize(int w, int h)
{
	int max = MAX_SIZE_4K;

	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D)
		max = MAX_SIZE_2K;

	if (w < 0 || h < 0)
		return true;

	if (h != 0 && (w > max / h))
		return true;

	return false;
}

static void vmh264_udc_fill_vpts(struct vdec_h264_hw_s *hw,
						int frame_type,
						u32 vpts,
						u32 vpts_valid);
static int  compute_losless_comp_body_size(int width,
				int height, int bit_depth_10);
static int  compute_losless_comp_header_size(int width, int height);

static int hevc_alloc_mmu(struct vdec_h264_hw_s *hw, int pic_idx,
		int pic_width, int pic_height, u16 bit_depth,
		unsigned int *mmu_index_adr) {
	int cur_buf_idx;
	int bit_depth_10 = (bit_depth != 0x00);
	int picture_size;
	u32 cur_mmu_4k_number;

	WRITE_VREG(CURR_CANVAS_CTRL, pic_idx<<24);
	cur_buf_idx = READ_VREG(CURR_CANVAS_CTRL)&0xff;
	picture_size = compute_losless_comp_body_size(pic_width,
					pic_height, bit_depth_10);
	cur_mmu_4k_number = ((picture_size+(1<<12)-1) >> 12);
	dpb_print(DECODE_ID(hw),
		PRINT_FLAG_MMU_DETAIL,
		"alloc_mmu new_fb_idx %d picture_size %d cur_mmu_4k_number %d\n",
		cur_buf_idx, picture_size, cur_mmu_4k_number);

	if (cur_mmu_4k_number > MAX_FRAME_4K_NUM) {
		pr_err("hevc_alloc_mmu cur_mmu_4k_number %d unsupport\n",
		cur_mmu_4k_number);
		return -1;
	}

	return decoder_mmu_box_alloc_idx(
		hw->mmu_box,
		cur_buf_idx,
		cur_mmu_4k_number,
		mmu_index_adr);
}

static int  compute_losless_comp_body_size(int width,
					int height, int bit_depth_10)
{
	int    width_x64;
	int    height_x32;
	int    bsize;

	width_x64 = width + 63;
	width_x64 >>= 6;

	height_x32 = height + 31;
	height_x32 >>= 5;

#ifdef H264_MMU
	bsize = (bit_depth_10 ? 4096 : 3264) * width_x64*height_x32;
#else
	bsize = (bit_depth_10 ? 4096 : 3072) * width_x64*height_x32;
#endif
	return bsize;
}

static int  compute_losless_comp_header_size(int width, int height)
{
	int	width_x64;
	int	width_x128;
	int    height_x64;
	int	hsize;

	width_x64 = width + 63;
	width_x64 >>= 6;

	width_x128 = width + 127;
	width_x128 >>= 7;

	height_x64 = height + 63;
	height_x64 >>= 6;

#ifdef	H264_MMU
	hsize = 128*width_x64*height_x64;
#else
	hsize = 32*width_x128*height_x64;
#endif
	return  hsize;
}

static int get_dw_size(struct vdec_h264_hw_s *hw, u32 *pdw_buffer_size_u_v_h)
{
	int pic_width, pic_height;
	int lcu_size = 16;
	int dw_buf_size;
	u32 dw_buffer_size_u_v;
	u32 dw_buffer_size_u_v_h;
	int dw_mode =  hw->double_write_mode;

	pic_width = hw->frame_width;
	pic_height = hw->frame_height;

	if (dw_mode) {
		int pic_width_dw = pic_width /
			get_double_write_ratio(hw->double_write_mode);
		int pic_height_dw = pic_height /
			get_double_write_ratio(hw->double_write_mode);

		int pic_width_lcu_dw = (pic_width_dw % lcu_size) ?
			pic_width_dw / lcu_size + 1 :
			pic_width_dw / lcu_size;
		int pic_height_lcu_dw = (pic_height_dw % lcu_size) ?
			pic_height_dw / lcu_size + 1 :
			pic_height_dw / lcu_size;
		int lcu_total_dw = pic_width_lcu_dw * pic_height_lcu_dw;


		dw_buffer_size_u_v = lcu_total_dw * lcu_size * lcu_size / 2;
		dw_buffer_size_u_v_h = (dw_buffer_size_u_v + 0xffff) >> 16;
			/*64k alignment*/
		dw_buf_size = ((dw_buffer_size_u_v_h << 16) * 3);
		*pdw_buffer_size_u_v_h = dw_buffer_size_u_v_h;
	} else {
		*pdw_buffer_size_u_v_h = 0;
		dw_buf_size = 0;
	}

	return dw_buf_size;
}


static void hevc_mcr_config_canv2axitbl(struct vdec_h264_hw_s *hw, int restore)
{
	int i, size;
	u32   canvas_addr;
	unsigned long maddr;
	int     num_buff = hw->dpb.mDPB.size;
	int dw_size = 0;
	u32 dw_buffer_size_u_v_h;
	u32 blkmode = hw->canvas_mode;
	int dw_mode =  hw->double_write_mode;
	struct vdec_s *vdec = hw_to_vdec(hw);

	canvas_addr = ANC0_CANVAS_ADDR;
	for (i = 0; i < num_buff; i++)
		WRITE_VREG((canvas_addr + i), i | (i << 8) | (i << 16));

	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, (0x1 << 1) | (0x1 << 2));
	size = hw->losless_comp_body_size + hw->losless_comp_header_size;


	dw_size = get_dw_size(hw, &dw_buffer_size_u_v_h);
	size += dw_size;
	if (size > 0)
		size += 0x10000;

	dpb_print(DECODE_ID(hw), PRINT_FLAG_MMU_DETAIL,
		"dw_buffer_size_u_v_h = %d, dw_size = 0x%x, size = 0x%x\n",
		dw_buffer_size_u_v_h, dw_size, size);

	dpb_print(DECODE_ID(hw), PRINT_FLAG_MMU_DETAIL,
		"body_size = %d, header_size = %d, body_size_sao = %d\n",
				hw->losless_comp_body_size,
				hw->losless_comp_header_size,
				hw->losless_comp_body_size_sao);

	for (i = 0; i < num_buff; i++) {
		if (!restore) {
			if (decoder_bmmu_box_alloc_buf_phy(hw->bmmu_box,
				HEADER_BUFFER_IDX(i), size,
				DRIVER_HEADER_NAME, &maddr) < 0) {
				dpb_print(DECODE_ID(hw), 0,
					"%s malloc compress header failed %d\n",
					DRIVER_HEADER_NAME, i);
				return;
			}

			if (vdec->vdata == NULL) {
				vdec->vdata = vdec_data_get();
			}

			if (vdec->vdata != NULL) {
				struct buffer_spec_s *pic = &hw->buffer_spec[i];
				int index = 0;

				if ((pic->user_data_buf != NULL)) {
					pic->user_data_buf = NULL;
				}

				index = vdec_data_get_index((ulong)vdec->vdata);
				if (index >= 0) {
					pic->user_data_buf = vzalloc(SEI_ITU_DATA_SIZE);
					if (pic->user_data_buf == NULL) {
						dpb_print(DECODE_ID(hw), 0, "alloc %dth userdata failed\n", i);
					}
					vdec_data_buffer_count_increase((ulong)vdec->vdata, index, i);
					vdec->vdata->data[index].user_data_buf = pic->user_data_buf;
					INIT_LIST_HEAD(&vdec->vdata->release_callback[i].node);
					decoder_bmmu_box_add_callback_func(hw->bmmu_box, i, (void *)&vdec->vdata->release_callback[i]);
				} else {
					dpb_print(DECODE_ID(hw), 0, "vdec data is full\n");
				}
			}

			if (hw->enable_fence) {
				vdec_fence_buffer_count_increase((ulong)vdec->sync);
				INIT_LIST_HEAD(&vdec->sync->release_callback[HEADER_BUFFER_IDX(i)].node);
				decoder_bmmu_box_add_callback_func(hw->bmmu_box, HEADER_BUFFER_IDX(i), (void *)&vdec->sync->release_callback[i]);
			}
		} else
			maddr = hw->buffer_spec[i].alloc_header_addr;
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,  maddr >> 5);
		hw->buffer_spec[i].alloc_header_addr = maddr;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_MMU_DETAIL,
			"%s : canvas: %d  axiaddr:%x size 0x%x\n",
			__func__, i, (u32)maddr, size);

		if (dw_mode) {
			u32 addr;
			int canvas_w;
			int canvas_h;

			canvas_w = hw->frame_width /
				get_double_write_ratio(hw->double_write_mode);
			canvas_h = hw->frame_height /
				get_double_write_ratio(hw->double_write_mode);

			if (hw->canvas_mode == 0)
				canvas_w = ALIGN(canvas_w, 32);
			else
				canvas_w = ALIGN(canvas_w, 64);
			canvas_h = ALIGN(canvas_h, 32);

			hw->buffer_spec[i].dw_y_adr =
				maddr + hw->losless_comp_header_size;

			hw->buffer_spec[i].dw_y_adr =
				((hw->buffer_spec[i].dw_y_adr + 0xffff) >> 16)
					<< 16;
			hw->buffer_spec[i].dw_u_v_adr =
				hw->buffer_spec[i].dw_y_adr
				+ (dw_buffer_size_u_v_h << 16) * 2;


			hw->buffer_spec[i].buf_adr
				= hw->buffer_spec[i].dw_y_adr;
			addr = hw->buffer_spec[i].buf_adr;


			dpb_print(DECODE_ID(hw), PRINT_FLAG_MMU_DETAIL,
				"dw_y_adr = 0x%x, dw_u_v_adr = 0x%x, y_addr = 0x%x, u_addr = 0x%x, v_addr = 0x%x, width = %d, height = %d\n",
				hw->buffer_spec[i].dw_y_adr,
				hw->buffer_spec[i].dw_u_v_adr,
				hw->buffer_spec[i].y_addr,
				hw->buffer_spec[i].u_addr,
				hw->buffer_spec[i].v_addr,
				canvas_w,
				canvas_h);

			hw->buffer_spec[i].canvas_config[0].phy_addr =
					hw->buffer_spec[i].dw_y_adr;
			hw->buffer_spec[i].canvas_config[0].width = canvas_w;
			hw->buffer_spec[i].canvas_config[0].height = canvas_h;
			hw->buffer_spec[i].canvas_config[0].block_mode =
				blkmode;
			hw->buffer_spec[i].canvas_config[0].endian = 7;

			hw->buffer_spec[i].canvas_config[1].phy_addr =
					hw->buffer_spec[i].dw_u_v_adr;
			hw->buffer_spec[i].canvas_config[1].width = canvas_w;
			hw->buffer_spec[i].canvas_config[1].height = canvas_h;
			hw->buffer_spec[i].canvas_config[1].block_mode =
				blkmode;
			hw->buffer_spec[i].canvas_config[1].endian = 7;
		}
	}
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x1);

	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (0 << 8) | (0<<1) | 1);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);
	return;
}
static void    hevc_mcr_config_mc_ref(struct vdec_h264_hw_s *hw)
{
	u32 i;
	u32 ref_canv;
	struct Slice *pSlice = &(hw->dpb.mSlice);
	/*REFLIST[0]*/
	for (i = 0; i < (unsigned int)(pSlice->listXsize[0]); i++) {
		struct StorablePicture *ref = pSlice->listX[0][i];
		if (ref == NULL)
			return;
		WRITE_VREG(CURR_CANVAS_CTRL, ref->buf_spec_num<<24);
		ref_canv = READ_VREG(CURR_CANVAS_CTRL)&0xffffff;
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
					(ref->buf_spec_num & 0x3f) << 8);
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, ref_canv);
	}
	/*REFLIST[1]*/
	for (i = 0; i < (unsigned int)(pSlice->listXsize[1]); i++) {
		struct StorablePicture *ref = pSlice->listX[1][i];
		if (ref == NULL)
			return;
		WRITE_VREG(CURR_CANVAS_CTRL, ref->buf_spec_num<<24);
		ref_canv = READ_VREG(CURR_CANVAS_CTRL)&0xffffff;
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
					(ref->buf_spec_num & 0x3f) << 8);
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, ref_canv);
	}
	return;
}

static void   hevc_mcr_config_mcrcc(struct vdec_h264_hw_s *hw)
{
	u32 rdata32;
	u32 rdata32_2;
	u32 slice_type;
	struct StorablePicture *ref;
	struct Slice *pSlice;
	slice_type = hw->dpb.mSlice.slice_type;
	pSlice = &(hw->dpb.mSlice);
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2);
	if (slice_type == I_SLICE) {
		WRITE_VREG(HEVCD_MCRCC_CTL1, 0x0);
		return;
	}
	if (slice_type == B_SLICE) {
		ref = pSlice->listX[0][0];
		if (ref == NULL)
			return;
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				((ref->buf_spec_num & 0x3f) << 8));
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		ref = pSlice->listX[1][0];
		if (ref == NULL)
			return;
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			((ref->buf_spec_num & 0x3f) << 8));
		rdata32_2 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32_2 = rdata32_2 & 0xffff;
		rdata32_2 = rdata32_2 | (rdata32_2 << 16);
		if (rdata32 == rdata32_2) {
			ref = pSlice->listX[1][1];
			if (ref == NULL)
				return;
			WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				((ref->buf_spec_num & 0x3f) << 8));
			rdata32_2 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
			rdata32_2 = rdata32_2 & 0xffff;
			rdata32_2 = rdata32_2 | (rdata32_2 << 16);
		}
		WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32_2);
	} else { /*P-PIC*/
		ref = pSlice->listX[0][0];
		if (ref == NULL)
			return;
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				((ref->buf_spec_num & 0x3f) << 8));
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		ref = pSlice->listX[0][1];
		if (ref == NULL)
			return;
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				((ref->buf_spec_num & 0x3f) << 8));
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32);
	}
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0);
	return;
}


static void  hevc_mcr_sao_global_hw_init(struct vdec_h264_hw_s *hw,
		u32 width, u32 height) {
	u32 data32;
	u32 lcu_x_num, lcu_y_num;
	u32 lcu_total;
	u32 mc_buffer_size_u_v;
	u32 mc_buffer_size_u_v_h;
	int  dw_mode = hw->double_write_mode;

	/*lcu_x_num = (width + 15) >> 4;*/
	// width need to be round to 64 pixel -- case0260 1/10/2020
	lcu_x_num = (((width + 63) >> 6) << 2);
	lcu_y_num = (height + 15) >> 4;
	lcu_total = lcu_x_num * lcu_y_num;

	hw->mc_buffer_size_u_v = mc_buffer_size_u_v = lcu_total*16*16/2;
	hw->mc_buffer_size_u_v_h =
		mc_buffer_size_u_v_h = (mc_buffer_size_u_v + 0xffff)>>16;

	hw->losless_comp_body_size = 0;

	hw->losless_comp_body_size_sao =
			compute_losless_comp_body_size(width, height, 0);
	hw->losless_comp_header_size =
			compute_losless_comp_header_size(width, height);

	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0x1); /*sw reset ipp10b_top*/
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0x0); /*sw reset ipp10b_top*/

	/* setup lcu_size = 16*/
	WRITE_VREG(HEVCD_IPP_TOP_LCUCONFIG, 16); /*set lcu size = 16*/
	/*pic_width/pic_height*/
	WRITE_VREG(HEVCD_IPP_TOP_FRMCONFIG,
		(height & 0xffff) << 16 | (width & 0xffff));
	/* bitdepth_luma = 8*/
	/* bitdepth_chroma = 8*/
	WRITE_VREG(HEVCD_IPP_BITDEPTH_CONFIG, 0x0);/*set bit-depth 8 */

#ifdef	H265_LOSLESS_COMPRESS_MODE
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, (0x1 << 4));
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, 0x0);
#else
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x1 << 31);
#endif
	data32 = READ_VREG(HEVCD_IPP_AXIIF_CONFIG);
	data32 &= (~0x30);
	data32 |= (hw->canvas_mode << 4);
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);

	WRITE_VREG(HEVCD_MPP_DECOMP_CTL3,
			(0x80 << 20) | (0x80 << 10) | (0xff));

	WRITE_VREG(HEVCD_MPP_VDEC_MCR_CTL, 0x1 | (0x1 << 4));

	/*comfig vdec:h264:mdec to use hevc mcr/mcrcc/decomp*/
	WRITE_VREG(MDEC_PIC_DC_MUX_CTRL,
			READ_VREG(MDEC_PIC_DC_MUX_CTRL) | 0x1 << 31);
	/* ipp_enable*/
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0x1 << 1);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
		  WRITE_VREG(HEVC_DBLK_CFG1, 0x2); // set ctusize==16
		  WRITE_VREG(HEVC_DBLK_CFG2, ((height & 0xffff)<<16) | (width & 0xffff));
		  if (dw_mode & 0x10)
			WRITE_VREG(HEVC_DBLK_CFGB, 0x40405603);
		  else if (dw_mode)
			WRITE_VREG(HEVC_DBLK_CFGB, 0x40405703);
		  else
			WRITE_VREG(HEVC_DBLK_CFGB, 0x40405503);
	}

	data32 = READ_VREG(HEVC_SAO_CTRL0);
	data32 &= (~0xf);
	data32 |= 0x4;
	WRITE_VREG(HEVC_SAO_CTRL0, data32);
	WRITE_VREG(HEVC_SAO_PIC_SIZE, (height & 0xffff) << 16 |
			(width & 0xffff));
	data32  = ((lcu_x_num-1) | (lcu_y_num-1) << 16);

	WRITE_VREG(HEVC_SAO_PIC_SIZE_LCU, data32);
	data32  =  (lcu_x_num  | lcu_y_num << 16);
	WRITE_VREG(HEVC_SAO_TILE_SIZE_LCU, data32);
	data32 = (mc_buffer_size_u_v_h << 16) << 1;
	WRITE_VREG(HEVC_SAO_Y_LENGTH, data32);
	data32 = (mc_buffer_size_u_v_h << 16);
	WRITE_VREG(HEVC_SAO_C_LENGTH, data32);

	data32 = READ_VREG(HEVC_SAO_CTRL1);
	data32 &= (~0x3000);
	data32 &= (~0xff0);
	data32 |= endian;	/* Big-Endian per 64-bit */

	if (hw->mmu_enable && (dw_mode & 0x10))
		data32 |= ((hw->canvas_mode << 12) |1);
	else if (hw->mmu_enable && dw_mode)
		data32 |= ((hw->canvas_mode << 12));
	else
		data32 |= ((hw->canvas_mode << 12)|2);

	WRITE_VREG(HEVC_SAO_CTRL1, data32);

#ifdef	H265_DW_NO_SCALE
	WRITE_VREG(HEVC_SAO_CTRL5, READ_VREG(HEVC_SAO_CTRL5) & ~(0xff << 16));
	if (hw->mmu_enable && dw_mode) {
		data32 =	READ_VREG(HEVC_SAO_CTRL5);
		data32 &= (~(0xff << 16));
		if (dw_mode == 2 ||
			dw_mode == 3)
			data32 |= (0xff<<16);
		else if (dw_mode == 4)
			data32 |= (0x33<<16);
		WRITE_VREG(HEVC_SAO_CTRL5, data32);
	}


#endif


#ifdef	H265_LOSLESS_COMPRESS_MODE
	data32 = READ_VREG(HEVC_SAO_CTRL5);
	data32 |= (1<<9); /*8-bit smem-mode*/
	WRITE_VREG(HEVC_SAO_CTRL5, data32);

	WRITE_VREG(HEVC_CM_BODY_LENGTH, hw->losless_comp_body_size_sao);
	WRITE_VREG(HEVC_CM_HEADER_OFFSET, hw->losless_comp_body_size);
	WRITE_VREG(HEVC_CM_HEADER_LENGTH, hw->losless_comp_header_size);
#endif

#ifdef	H265_LOSLESS_COMPRESS_MODE
	WRITE_VREG(HEVC_SAO_CTRL9, READ_VREG(HEVC_SAO_CTRL9) | (0x1 << 1));
	WRITE_VREG(HEVC_SAO_CTRL5, READ_VREG(HEVC_SAO_CTRL5) | (0x1 << 10));
#endif

	WRITE_VREG(HEVC_SAO_CTRL9, READ_VREG(HEVC_SAO_CTRL9) | 0x1 << 7);

	memset(hw->frame_mmu_map_addr, 0, FRAME_MMU_MAP_SIZE);

	WRITE_VREG(MDEC_EXTIF_CFG0, hw->extif_addr);
	WRITE_VREG(MDEC_EXTIF_CFG1, 0x80000000);
	return;
}

static void  hevc_sao_set_slice_type(struct vdec_h264_hw_s *hw,
		u32 is_new_pic, u32 is_idr)
{
	hw->is_new_pic = is_new_pic;
	hw->is_idr_frame = is_idr;
	return;
}

static void  hevc_sao_set_pic_buffer(struct vdec_h264_hw_s *hw,
			struct StorablePicture *pic) {
	u32 mc_y_adr;
	u32 mc_u_v_adr;
	u32 dw_y_adr;
	u32 dw_u_v_adr;
	u32 canvas_addr;
	int ret;
	int  dw_mode = hw->double_write_mode;
	if (hw->is_new_pic != 1)
		return;

	if (hw->is_idr_frame) {
		/* William TBD */
		memset(hw->frame_mmu_map_addr, 0, FRAME_MMU_MAP_SIZE);
	}

	WRITE_VREG(CURR_CANVAS_CTRL, pic->buf_spec_num << 24);
	canvas_addr = READ_VREG(CURR_CANVAS_CTRL)&0xffffff;
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, (0x0 << 1) |
			(0x0 << 2) | ((canvas_addr & 0xff) << 8));
	mc_y_adr = READ_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA) << 5;
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, (0x0 << 1) |
			(0x0 << 2) | (((canvas_addr >> 8) & 0xff) << 8));
	mc_u_v_adr = READ_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA) << 5;
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0x1);


	if (dw_mode) {
		dw_y_adr = hw->buffer_spec[pic->buf_spec_num].dw_y_adr;
		dw_u_v_adr = hw->buffer_spec[pic->buf_spec_num].dw_u_v_adr;
	} else {
		dw_y_adr = 0;
		dw_u_v_adr = 0;
	}
#ifdef	H265_LOSLESS_COMPRESS_MODE
	if (dw_mode)
		WRITE_VREG(HEVC_SAO_Y_START_ADDR, dw_y_adr);
	WRITE_VREG(HEVC_CM_BODY_START_ADDR, mc_y_adr);
#ifdef	H264_MMU
	WRITE_VREG(HEVC_CM_HEADER_START_ADDR, mc_y_adr);
#else
	WRITE_VREG(HEVC_CM_HEADER_START_ADDR,
			(mc_y_adr + hw->losless_comp_body_size));
#endif
#else
	WRITE_VREG(HEVC_SAO_Y_START_ADDR, mc_y_adr);
#endif

#ifndef H265_LOSLESS_COMPRESS_MODE
	WRITE_VREG(HEVC_SAO_C_START_ADDR, mc_u_v_adr);
#else
	if (dw_mode)
		WRITE_VREG(HEVC_SAO_C_START_ADDR, dw_u_v_adr);
#endif

#ifndef LOSLESS_COMPRESS_MODE
	if (dw_mode) {
		WRITE_VREG(HEVC_SAO_Y_WPTR, mc_y_adr);
		WRITE_VREG(HEVC_SAO_C_WPTR, mc_u_v_adr);
	}
#else
	WRITE_VREG(HEVC_SAO_Y_WPTR, dw_y_adr);
	WRITE_VREG(HEVC_SAO_C_WPTR, dw_u_v_adr);
#endif

	ret = hevc_alloc_mmu(hw, pic->buf_spec_num,
			(hw->mb_width << 4), (hw->mb_height << 4), 0x0,
			hw->frame_mmu_map_addr);
	if (ret != 0) {
		dpb_print(DECODE_ID(hw),
		PRINT_FLAG_MMU_DETAIL, "can't alloc need mmu1,idx %d ret =%d\n",
		pic->buf_spec_num,
		ret);
		return;
	}

	/*Reset SAO + Enable SAO slice_start*/
	if (hw->mmu_enable  && get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A)
		WRITE_VREG(HEVC_DBLK_CFG0, 0x1); // reset buffer32x4 in lpf for every picture
	WRITE_VREG(HEVC_SAO_INT_STATUS,
			READ_VREG(HEVC_SAO_INT_STATUS) | 0x1 << 28);
	WRITE_VREG(HEVC_SAO_INT_STATUS,
			READ_VREG(HEVC_SAO_INT_STATUS) | 0x1 << 31);
	/*pr_info("hevc_sao_set_pic_buffer:mc_y_adr: %x\n", mc_y_adr);*/
	/*Send coommand to hevc-code to supply 4k buffers to sao*/

	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VREG(H265_SAO_4K_SET_BASE, (u32)hw->frame_mmu_map_phy_addr);
		WRITE_VREG(H265_SAO_4K_SET_COUNT, MAX_FRAME_4K_NUM);
	} else
		WRITE_VREG(HEVC_ASSIST_MMU_MAP_ADDR, (u32)hw->frame_mmu_map_phy_addr);
	WRITE_VREG(SYS_COMMAND, H265_PUT_SAO_4K_SET);
	hw->frame_busy = 1;
	return;
}


static void  hevc_set_unused_4k_buff_idx(struct vdec_h264_hw_s *hw,
		u32 buf_spec_num) {
	WRITE_VREG(CURR_CANVAS_CTRL, buf_spec_num<<24);
	hw->hevc_cur_buf_idx = READ_VREG(CURR_CANVAS_CTRL)&0xff;
	dpb_print(DECODE_ID(hw),
		PRINT_FLAG_MMU_DETAIL, " %s  cur_buf_idx %d  buf_spec_num %d\n",
		__func__, hw->hevc_cur_buf_idx, buf_spec_num);
	return;
}


static void  hevc_set_frame_done(struct vdec_h264_hw_s *hw)
{
	ulong timeout = jiffies + HZ;
	dpb_print(DECODE_ID(hw),
		PRINT_FLAG_MMU_DETAIL, "hevc_frame_done...set\n");
	while ((READ_VREG(HEVC_SAO_INT_STATUS) & 0x1) == 0) {
		if (time_after(jiffies, timeout)) {
			dpb_print(DECODE_ID(hw),
			PRINT_FLAG_MMU_DETAIL, " %s..timeout!\n", __func__);
			break;
		}
	}
	timeout = jiffies + HZ;
	while (READ_VREG(HEVC_CM_CORE_STATUS) & 0x1) {
		if (time_after(jiffies, timeout)) {
			dpb_print(DECODE_ID(hw),
			PRINT_FLAG_MMU_DETAIL, " %s cm_core..timeout!\n", __func__);
			break;
		}
	}
	WRITE_VREG(HEVC_SAO_INT_STATUS, 0x1);
	hw->frame_done = 1;
	return;
}

static void release_cur_decoding_buf(struct vdec_h264_hw_s *hw)
{
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;

	mutex_lock(&hw->pic_mutex);
	if (p_H264_Dpb->mVideo.dec_picture) {
		release_picture(p_H264_Dpb,
			p_H264_Dpb->mVideo.dec_picture);
		p_H264_Dpb->mVideo.dec_picture->data_flag &= ~ERROR_FLAG;
		p_H264_Dpb->mVideo.dec_picture = NULL;
		if (hw->mmu_enable)
			hevc_set_frame_done(hw);
	}
	mutex_unlock(&hw->pic_mutex);
}

static void  hevc_sao_wait_done(struct vdec_h264_hw_s *hw)
{
	ulong timeout = jiffies + HZ;
	dpb_print(DECODE_ID(hw),
		PRINT_FLAG_MMU_DETAIL, "hevc_sao_wait_done...start\n");
	while ((READ_VREG(HEVC_SAO_INT_STATUS) >> 31)) {
		if (time_after(jiffies, timeout)) {
			dpb_print(DECODE_ID(hw),
			PRINT_FLAG_MMU_DETAIL,
			"hevc_sao_wait_done...wait timeout!\n");
			break;
		}
	}
	timeout = jiffies + HZ;
	if ((hw->frame_busy == 1) && (hw->frame_done == 1) ) {
		if (get_cpu_major_id() <  AM_MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VREG(SYS_COMMAND, H265_ABORT_SAO_4K_SET);
			while ((READ_VREG(SYS_COMMAND) & 0xff) !=
					H265_ABORT_SAO_4K_SET_DONE) {
					if (time_after(jiffies, timeout)) {
						dpb_print(DECODE_ID(hw),
						PRINT_FLAG_MMU_DETAIL,
						"wait h265_abort_sao_4k_set_done timeout!\n");
						break;
					}
			}
		}
		amhevc_stop();
		hw->frame_busy = 0;
		hw->frame_done = 0;
		dpb_print(DECODE_ID(hw),
			PRINT_FLAG_MMU_DETAIL,
			"sao wait done ,hevc stop!\n");
	}
	return;
}
static void buf_spec_init(struct vdec_h264_hw_s *hw, bool buffer_reset_flag)
{
	int i;
	unsigned long flags;
	spin_lock_irqsave(&hw->bufspec_lock, flags);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		struct vframe_s *vf = &hw->vfpool[hw->cur_pool][i];
		u32 ref_idx = BUFSPEC_INDEX(vf->index);
		if ((vf->index != -1) &&
			(hw->buffer_spec[ref_idx].vf_ref == 0) &&
			(hw->buffer_spec[ref_idx].used != -1)) {
			vf->index = -1;
		}
	}

	hw->cur_pool++;
	if (hw->cur_pool >= VF_POOL_NUM)
		hw->cur_pool = 0;

	for (i = 0; i < VF_POOL_SIZE; i++) {
		struct vframe_s *vf = &hw->vfpool[hw->cur_pool][i];
		u32 ref_idx = BUFSPEC_INDEX(vf->index);
		if ((vf->index != -1) &&
			(hw->buffer_spec[ref_idx].vf_ref == 0) &&
			(hw->buffer_spec[ref_idx].used != -1)) {
			vf->index = -1;
		}
	}
	/* buffers are alloced when error reset, v4l must find buffer by buffer_wrap[] */
	if (hw->reset_bufmgr_flag && buffer_reset_flag) {
		for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
			if (hw->buffer_spec[i].used == 1 || hw->buffer_spec[i].used == 2)
				hw->buffer_spec[i].used = 0;
		}
	} else {
		for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
			hw->buffer_spec[i].used = -1;
			hw->buffer_spec[i].canvas_pos = -1;
			hw->buffer_wrap[i] = -1;
		}
	}

	if (dpb_is_debug(DECODE_ID(hw),
		PRINT_FLAG_DUMP_BUFSPEC))
		dump_bufspec(hw, __func__);
	spin_unlock_irqrestore(&hw->bufspec_lock, flags);
}


/*is active in buf management */
static unsigned char is_buf_spec_in_use(struct vdec_h264_hw_s *hw,
	int buf_spec_num)
{
	unsigned char ret = 0;
	if (hw->buffer_spec[buf_spec_num].used == 1 ||
		hw->buffer_spec[buf_spec_num].used == 2 ||
		hw->buffer_spec[buf_spec_num].used == 3 ||
		hw->buffer_spec[buf_spec_num].used == 5)
		ret = 1;
	return ret;
}

static unsigned char is_buf_spec_in_disp_q(struct vdec_h264_hw_s *hw,
	int buf_spec_num)
{
	unsigned char ret = 0;
	if (hw->buffer_spec[buf_spec_num].used == 2 ||
		hw->buffer_spec[buf_spec_num].used == 3 ||
		hw->buffer_spec[buf_spec_num].used == 5)
		ret = 1;
	return ret;
}

static int alloc_one_buf_spec(struct vdec_h264_hw_s *hw, int i)
{
	struct vdec_s *vdec = hw_to_vdec(hw);
	if (hw->mmu_enable) {
		if (hw->buffer_spec[i].alloc_header_addr)
			return 0;
		else
			return -1;
	} else {

		int buf_size = (hw->mb_total << 8) + (hw->mb_total << 7);
		int addr;
#ifdef VDEC_DW
		int orig_buf_size;
		orig_buf_size = buf_size;
		if (IS_VDEC_DW(hw) == 1)
			buf_size += (hw->mb_total << 7) + (hw->mb_total << 6);
		else if (IS_VDEC_DW(hw) == 2)
			buf_size += (hw->mb_total << 6) + (hw->mb_total << 5);
		else if (IS_VDEC_DW(hw) == 4)
			buf_size += (hw->mb_total << 4) + (hw->mb_total << 3);
		else if (IS_VDEC_DW(hw) == 8)
			buf_size += (hw->mb_total << 2) + (hw->mb_total << 1);
		if (IS_VDEC_DW(hw)) {
			u32 align_size;
			/* add align padding size for blk64x32: (mb_w<<4)*32, (mb_h<<4)*64 */
			align_size = ((hw->mb_width << 9) + (hw->mb_height << 10)) / IS_VDEC_DW(hw);
			/* double align padding size for uv*/
			align_size <<= 1;
			buf_size += align_size + PAGE_SIZE;
		}
#endif
		if (hw->buffer_spec[i].cma_alloc_addr)
			return 0;

		if (decoder_bmmu_box_alloc_buf_phy(hw->bmmu_box, i,
			PAGE_ALIGN(buf_size), DRIVER_NAME,
			&hw->buffer_spec[i].cma_alloc_addr) < 0) {
			hw->buffer_spec[i].cma_alloc_addr = 0;
			if (hw->no_mem_count++ > 3) {
				hw->stat |= DECODER_FATAL_ERROR_NO_MEM;
				hw->reset_bufmgr_flag = 1;
			}
			dpb_print(DECODE_ID(hw), 0,
			"%s, fail to alloc buf for bufspec%d, try later\n",
					__func__, i
			);
			return -1;
		} else {
			if (vdec->vdata == NULL) {
				vdec->vdata = vdec_data_get();
			}

			if (vdec->vdata != NULL) {
				struct buffer_spec_s *pic = &hw->buffer_spec[i];
				int index = 0;

				if (pic->user_data_buf != NULL) {
					pic->user_data_buf = NULL;
				}

				index = vdec_data_get_index((ulong)vdec->vdata);
				if (index >= 0) {
					pic->user_data_buf = vzalloc(SEI_ITU_DATA_SIZE);
					if (pic->user_data_buf == NULL) {
						dpb_print(DECODE_ID(hw), 0, "alloc %dth userdata failed\n", i);
					}
					vdec->vdata->data[index].user_data_buf = pic->user_data_buf;
					vdec_data_buffer_count_increase((ulong)vdec->vdata, index, i);
					INIT_LIST_HEAD(&vdec->vdata->release_callback[i].node);
					decoder_bmmu_box_add_callback_func(hw->bmmu_box, i, (void *)&vdec->vdata->release_callback[i]);
				} else {
					dpb_print(DECODE_ID(hw), 0, "vdec data is full\n");
				}
			}

			if (hw->enable_fence) {
				vdec_fence_buffer_count_increase((ulong)vdec->sync);
				INIT_LIST_HEAD(&vdec->sync->release_callback[i].node);
				decoder_bmmu_box_add_callback_func(hw->bmmu_box, i, (void *)&vdec->sync->release_callback[i]);
			}
			hw->no_mem_count = 0;
			hw->stat &= ~DECODER_FATAL_ERROR_NO_MEM;
		}
		if (!vdec_secure(vdec)) {
			/*init internal buf*/
			char *tmpbuf = (char *)codec_mm_phys_to_virt(hw->buffer_spec[i].cma_alloc_addr);
			if (tmpbuf) {
				memset(tmpbuf, 0, PAGE_ALIGN(buf_size));
				codec_mm_dma_flush(tmpbuf,
					   PAGE_ALIGN(buf_size),
					   DMA_TO_DEVICE);
			} else {
				tmpbuf = codec_mm_vmap(hw->buffer_spec[i].cma_alloc_addr, PAGE_ALIGN(buf_size));
				if (tmpbuf) {
					memset(tmpbuf, 0, PAGE_ALIGN(buf_size));
					codec_mm_dma_flush(tmpbuf,
						   PAGE_ALIGN(buf_size),
						   DMA_TO_DEVICE);
					codec_mm_unmap_phyaddr(tmpbuf);
				}
			}
		}
		hw->buffer_spec[i].buf_adr =
		hw->buffer_spec[i].cma_alloc_addr;
		addr = hw->buffer_spec[i].buf_adr;


		hw->buffer_spec[i].y_addr = addr;
		addr += hw->mb_total << 8;
		hw->buffer_spec[i].u_addr = addr;
		hw->buffer_spec[i].v_addr = addr;
		addr += hw->mb_total << 7;

		hw->buffer_spec[i].canvas_config[0].phy_addr =
			hw->buffer_spec[i].y_addr;
		hw->buffer_spec[i].canvas_config[0].width =
			hw->mb_width << 4;
		hw->buffer_spec[i].canvas_config[0].height =
			hw->mb_height << 4;
		hw->buffer_spec[i].canvas_config[0].block_mode =
			hw->canvas_mode;

		hw->buffer_spec[i].canvas_config[1].phy_addr =
				hw->buffer_spec[i].u_addr;
		hw->buffer_spec[i].canvas_config[1].width =
				hw->mb_width << 4;
		hw->buffer_spec[i].canvas_config[1].height =
				hw->mb_height << 3;
		hw->buffer_spec[i].canvas_config[1].block_mode =
				hw->canvas_mode;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
		"%s, alloc buf for bufspec%d\n",
				__func__, i);
#ifdef  VDEC_DW
		if (!IS_VDEC_DW(hw))
			return 0;
		else {
			int w_shift = 3, h_shift = 3;

			if (IS_VDEC_DW(hw) == 1) {
				w_shift = 3;
				h_shift = 4;
			} else if (IS_VDEC_DW(hw) == 2) {
				w_shift = 3;
				h_shift = 3;
			} else if (IS_VDEC_DW(hw) == 4) {
				w_shift = 2;
				h_shift = 2;
			} else if (IS_VDEC_DW(hw) == 8) {
				w_shift = 1;
				h_shift = 1;
			}

			addr = hw->buffer_spec[i].cma_alloc_addr + PAGE_ALIGN(orig_buf_size);
			hw->buffer_spec[i].vdec_dw_y_addr = addr;
			addr += ALIGN_WIDTH(hw->mb_width << w_shift) * ALIGN_HEIGHT(hw->mb_height << h_shift);
			hw->buffer_spec[i].vdec_dw_u_addr = addr;
			hw->buffer_spec[i].vdec_dw_v_addr = addr;
			addr += hw->mb_total << (w_shift + h_shift - 1);

			hw->buffer_spec[i].vdec_dw_canvas_config[0].phy_addr =
				hw->buffer_spec[i].vdec_dw_y_addr;
			hw->buffer_spec[i].vdec_dw_canvas_config[0].width =
				ALIGN_WIDTH(hw->mb_width << w_shift);
			hw->buffer_spec[i].vdec_dw_canvas_config[0].height =
				ALIGN_HEIGHT(hw->mb_height << h_shift);
			hw->buffer_spec[i].vdec_dw_canvas_config[0].block_mode =
				hw->canvas_mode;

			hw->buffer_spec[i].vdec_dw_canvas_config[1].phy_addr =
				hw->buffer_spec[i].vdec_dw_u_addr;
			hw->buffer_spec[i].vdec_dw_canvas_config[1].width =
				ALIGN_WIDTH(hw->mb_width << w_shift);
			hw->buffer_spec[i].vdec_dw_canvas_config[1].height =
				ALIGN_HEIGHT(hw->mb_height << (h_shift - 1));
			hw->buffer_spec[i].vdec_dw_canvas_config[1].block_mode =
				hw->canvas_mode;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s, vdec_dw: alloc buf for bufspec%d blkmod %d\n",
					__func__, i, hw->canvas_mode);
		}
#endif
	}
	return 0;
}

static int alloc_one_buf_spec_from_queue(struct vdec_h264_hw_s *hw, int idx)
{
	int ret = 0;
	struct aml_vcodec_ctx *ctx = NULL;
	struct buffer_spec_s *bs = &hw->buffer_spec[idx];
	struct canvas_config_s *y_canvas_cfg = NULL;
	struct canvas_config_s *c_canvas_cfg = NULL;
	struct vdec_v4l2_buffer *fb = NULL;
	unsigned int y_addr = 0, c_addr = 0;

	if (IS_ERR_OR_NULL(hw->v4l2_ctx)) {
		pr_err("the v4l context has err.\n");
		return -1;
	}

	if (bs->cma_alloc_addr)
		return 0;

	ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	dpb_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
		"[%d] %s(), try alloc from v4l queue buf size: %d\n",
		ctx->id, __func__,
		(hw->mb_total << 8) + (hw->mb_total << 7));

	ret = ctx->fb_ops.alloc(&ctx->fb_ops, hw->fb_token, &fb, AML_FB_REQ_DEC);
	if (ret < 0) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
			"[%d] get fb fail.\n", ctx->id);
		return ret;
	}

	bs->cma_alloc_addr = (unsigned long)fb;
	dpb_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
		"[%d] %s(), cma alloc addr: 0x%x, out %d dec %d\n",
		ctx->id, __func__, bs->cma_alloc_addr,
		ctx->cap_pool.out, ctx->cap_pool.dec);

	if (fb->num_planes == 1) {
		y_addr = fb->m.mem[0].addr;
		c_addr = fb->m.mem[0].addr + fb->m.mem[0].offset;
		fb->m.mem[0].bytes_used = fb->m.mem[0].size;
	} else if (fb->num_planes == 2) {
		y_addr = fb->m.mem[0].addr;
		c_addr = fb->m.mem[1].addr;
		fb->m.mem[0].bytes_used = fb->m.mem[0].size;
		fb->m.mem[1].bytes_used = fb->m.mem[1].size;
	}

	fb->status	= FB_ST_DECODER;

	dpb_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
		"[%d] %s(), y_addr: %x, size: %u\n",
		ctx->id, __func__, y_addr, fb->m.mem[0].size);
	dpb_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
		"[%d] %s(), c_addr: %x, size: %u\n",
		ctx->id, __func__, c_addr, fb->m.mem[1].size);

	bs->y_addr = y_addr;
	bs->u_addr = c_addr;
	bs->v_addr = c_addr;

	y_canvas_cfg = &bs->canvas_config[0];
	c_canvas_cfg = &bs->canvas_config[1];

	y_canvas_cfg->phy_addr	= y_addr;
	y_canvas_cfg->width	= hw->mb_width << 4;
	y_canvas_cfg->height	= hw->mb_height << 4;
	y_canvas_cfg->block_mode = hw->canvas_mode;
	//fb->m.mem[0].bytes_used = y_canvas_cfg->width * y_canvas_cfg->height;
	dpb_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
		"[%d] %s(), y_w: %d, y_h: %d\n", ctx->id, __func__,
		y_canvas_cfg->width,y_canvas_cfg->height);

	c_canvas_cfg->phy_addr	= c_addr;
	c_canvas_cfg->width	= hw->mb_width << 4;
	c_canvas_cfg->height	= hw->mb_height << 3;
	c_canvas_cfg->block_mode = hw->canvas_mode;
	//fb->m.mem[1].bytes_used = c_canvas_cfg->width * c_canvas_cfg->height;
	dpb_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
		"[%d] %s(), c_w: %d, c_h: %d\n", ctx->id, __func__,
		c_canvas_cfg->width, c_canvas_cfg->height);

	dpb_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
		"[%d] %s(), alloc buf for bufspec%d\n", ctx->id, __func__, idx);

	return ret;
}

static void config_decode_canvas(struct vdec_h264_hw_s *hw, int i)
{
	int blkmode = hw->canvas_mode;
	int endian = 0;

	if (blkmode == CANVAS_BLKMODE_LINEAR) {
		if ((h264_debug_flag & IGNORE_PARAM_FROM_CONFIG) == 0)
			endian = 7;
		else
			endian = 0;
	}

	if (hw->is_used_v4l)
		endian = 7;

	config_cav_lut_ex(hw->buffer_spec[i].
		y_canvas_index,
		hw->buffer_spec[i].y_addr,
		hw->mb_width << 4,
		hw->mb_height << 4,
		CANVAS_ADDR_NOWRAP,
		blkmode,
		endian,
		VDEC_1);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VREG(VDEC_ASSIST_CANVAS_BLK32,
				(1 << 11) | /* canvas_blk32_wr */
				(blkmode << 10) | /* canvas_blk32*/
				 (1 << 8) | /* canvas_index_wr*/
				(hw->buffer_spec[i].y_canvas_index << 0) /* canvas index*/
				);
	}

	config_cav_lut_ex(hw->buffer_spec[i].
		u_canvas_index,
		hw->buffer_spec[i].u_addr,
		hw->mb_width << 4,
		hw->mb_height << 3,
		CANVAS_ADDR_NOWRAP,
		blkmode,
		endian,
		VDEC_1);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VREG(VDEC_ASSIST_CANVAS_BLK32,
				(1 << 11) |
				(blkmode << 10) |
				 (1 << 8) |
				(hw->buffer_spec[i].u_canvas_index << 0));
	}

	WRITE_VREG(ANC0_CANVAS_ADDR + hw->buffer_spec[i].canvas_pos,
		spec2canvas(&hw->buffer_spec[i]));


#ifdef  VDEC_DW
	if (!IS_VDEC_DW(hw))
		return;
	else {
		config_cav_lut_ex(hw->buffer_spec[i].
			vdec_dw_y_canvas_index,
			hw->buffer_spec[i].vdec_dw_canvas_config[0].phy_addr,
			hw->buffer_spec[i].vdec_dw_canvas_config[0].width,
			hw->buffer_spec[i].vdec_dw_canvas_config[0].height,
			CANVAS_ADDR_NOWRAP,
			blkmode,
			endian,
			VDEC_1);
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
			WRITE_VREG(VDEC_ASSIST_CANVAS_BLK32,
				(1 << 11) |
				(blkmode << 10) |
				(1 << 8) |
				(hw->buffer_spec[i].vdec_dw_y_canvas_index << 0));
		}

		config_cav_lut_ex(hw->buffer_spec[i].
			vdec_dw_u_canvas_index,
			hw->buffer_spec[i].vdec_dw_canvas_config[1].phy_addr,
			hw->buffer_spec[i].vdec_dw_canvas_config[1].width,
			hw->buffer_spec[i].vdec_dw_canvas_config[1].height,
			CANVAS_ADDR_NOWRAP,
			blkmode,
			endian,
			VDEC_1);
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
			WRITE_VREG(VDEC_ASSIST_CANVAS_BLK32,
				(1 << 11) |
				(blkmode << 10) |
				(1 << 8) |
				(hw->buffer_spec[i].vdec_dw_u_canvas_index << 0));
		}
	}
#endif
}

static void config_decode_canvas_ex(struct vdec_h264_hw_s *hw, int i)
{
	u32 blkmode = hw->canvas_mode;
	int canvas_w;
	int canvas_h;

	canvas_w = hw->frame_width /
		get_double_write_ratio(hw->double_write_mode);
	canvas_h = hw->frame_height /
		get_double_write_ratio(hw->double_write_mode);

	if (hw->canvas_mode == 0)
		canvas_w = ALIGN(canvas_w, 32);
	else
		canvas_w = ALIGN(canvas_w, 64);
	canvas_h = ALIGN(canvas_h, 32);

	config_cav_lut_ex(hw->buffer_spec[i].
		y_canvas_index,
		hw->buffer_spec[i].dw_y_adr,
		canvas_w,
		canvas_h,
		CANVAS_ADDR_NOWRAP,
		blkmode,
		7,
		VDEC_HEVC);

	config_cav_lut_ex(hw->buffer_spec[i].
		u_canvas_index,
		hw->buffer_spec[i].dw_u_v_adr,
		canvas_w,
		canvas_h,
		CANVAS_ADDR_NOWRAP,
		blkmode,
		7,
		VDEC_HEVC);
}

static int v4l_get_free_buffer_spec(struct vdec_h264_hw_s *hw)
{
	int i;

	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		if (hw->buffer_spec[i].cma_alloc_addr == 0)
			return i;
	}

	return -1;
}

static int v4l_find_buffer_spec_idx(struct vdec_h264_hw_s *hw, unsigned int v4l_indx)
{
	int i;

	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		if (hw->buffer_wrap[i] == v4l_indx)
			return i;
	}
	return -1;
}

static int v4l_get_free_buf_idx(struct vdec_s *vdec)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	struct aml_vcodec_ctx * v4l = hw->v4l2_ctx;
	struct v4l_buff_pool *pool = &v4l->cap_pool;
	struct buffer_spec_s *pic = NULL;
	int i, rt, idx = INVALID_IDX;
	ulong flags;
	u32 state = 0, index;

	spin_lock_irqsave(&hw->bufspec_lock, flags);
	for (i = 0; i < pool->in; ++i) {
		state = (pool->seq[i] >> 16);
		index = (pool->seq[i] & 0xffff);

		switch (state) {
		case V4L_CAP_BUFF_IN_DEC:
			rt = v4l_find_buffer_spec_idx(hw, index);
			if (rt >= 0) {
				pic = &hw->buffer_spec[rt];
				if ((pic->vf_ref == 0) &&
					(pic->used == 0) &&
					pic->cma_alloc_addr) {
					idx = rt;
				}
			}
			break;
		case V4L_CAP_BUFF_IN_M2M:
			rt = v4l_get_free_buffer_spec(hw);
			if (rt >= 0) {
				pic = &hw->buffer_spec[rt];
				if (!alloc_one_buf_spec_from_queue(hw, rt)) {
					struct vdec_v4l2_buffer *fb;
					config_decode_canvas(hw, rt);
					fb = (struct vdec_v4l2_buffer *)pic->cma_alloc_addr;
					hw->buffer_wrap[rt] = fb->buf_idx;
					idx = rt;
				}
			}
			break;
		default:
			break;
		}

		if (idx != INVALID_IDX) {
			pic->used = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&hw->bufspec_lock, flags);

	if (idx < 0) {
		dpb_print(DECODE_ID(hw), 0, "%s fail, state %d\n", __func__, state);
		for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
			dpb_print(DECODE_ID(hw), 0, "%s, %d\n",
				__func__, hw->buffer_wrap[i]);
		}
		vmh264_dump_state(vdec);
	} else {
		struct vdec_v4l2_buffer *fb =
			(struct vdec_v4l2_buffer *)pic->cma_alloc_addr;

		fb->status = FB_ST_DECODER;
	}

	return idx;
}

int get_free_buf_idx(struct vdec_s *vdec)
{
	int i;
	unsigned long addr, flags;
	int index = -1;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	int buf_total = BUFSPEC_POOL_SIZE;

	if (hw->is_used_v4l)
		return v4l_get_free_buf_idx(vdec);

	spin_lock_irqsave(&hw->bufspec_lock, flags);
	/*hw->start_search_pos = 0;*/
	for (i = hw->start_search_pos; i < buf_total; i++) {
		if (hw->mmu_enable)
			addr = hw->buffer_spec[i].alloc_header_addr;
		else
			addr = hw->buffer_spec[i].cma_alloc_addr;

		if (hw->buffer_spec[i].vf_ref == 0 &&
			hw->buffer_spec[i].used == 0 && addr) {
			hw->buffer_spec[i].used = 1;
			hw->start_search_pos = i+1;
			index = i;
			hw->buffer_wrap[i] = index;
			break;
		}
	}
	if (index < 0) {
		for (i = 0; i < hw->start_search_pos; i++) {
			if (hw->mmu_enable)
				addr = hw->buffer_spec[i].alloc_header_addr;
			else
				addr = hw->buffer_spec[i].cma_alloc_addr;

			if (hw->buffer_spec[i].vf_ref == 0 &&
				hw->buffer_spec[i].used == 0 && addr) {
				hw->buffer_spec[i].used = 1;
				hw->start_search_pos = i+1;
				index = i;
				hw->buffer_wrap[i] = index;
				break;
			}
		}
	}

	spin_unlock_irqrestore(&hw->bufspec_lock, flags);
	if (hw->start_search_pos >= buf_total)
		hw->start_search_pos = 0;
	dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
			"%s, buf_spec_num %d\n", __func__, index);

	if (index < 0) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"%s fail\n", __func__);
		vmh264_dump_state(vdec);
	}

	if (dpb_is_debug(DECODE_ID(hw),
		PRINT_FLAG_DUMP_BUFSPEC))
		dump_bufspec(hw, __func__);
	return index;
}

int release_buf_spec_num(struct vdec_s *vdec, int buf_spec_num)
{
	/*u32 cur_buf_idx;*/
	unsigned long flags;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	dpb_print(DECODE_ID(hw), PRINT_FLAG_MMU_DETAIL,
		"%s buf_spec_num %d used %d\n",
		__func__, buf_spec_num,
		buf_spec_num > 0 ? hw->buffer_spec[buf_spec_num].used : 0);
	if (buf_spec_num >= 0 &&
		buf_spec_num < BUFSPEC_POOL_SIZE
		) {
		spin_lock_irqsave(&hw->bufspec_lock, flags);
		hw->buffer_spec[buf_spec_num].used = 0;
		spin_unlock_irqrestore(&hw->bufspec_lock, flags);
		if (hw->mmu_enable) {
			/*WRITE_VREG(CURR_CANVAS_CTRL, buf_spec_num<<24);
			cur_buf_idx = READ_VREG(CURR_CANVAS_CTRL);
			cur_buf_idx = cur_buf_idx&0xff;*/
			decoder_mmu_box_free_idx(hw->mmu_box, buf_spec_num);
		}
		release_aux_data(hw, buf_spec_num);
	}
	if (dpb_is_debug(DECODE_ID(hw),
		PRINT_FLAG_DUMP_BUFSPEC))
		dump_bufspec(hw, __func__);
	return 0;
}

static void config_buf_specs(struct vdec_s *vdec)
{
	int i, j;
	unsigned long flags;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	int mode = IS_VDEC_DW(hw) ? 2 : 1;

	spin_lock_irqsave(&hw->bufspec_lock, flags);
	for (i = 0, j = 0;
		j < hw->dpb.mDPB.size
		&& i < BUFSPEC_POOL_SIZE;
		i++) {
		int canvas;
		if (hw->buffer_spec[i].used != -1)
			continue;
		if (vdec->parallel_dec == 1) {
			if (hw->buffer_spec[i].y_canvas_index == -1)
				hw->buffer_spec[i].y_canvas_index = vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
			if (hw->buffer_spec[i].u_canvas_index == -1) {
				hw->buffer_spec[i].u_canvas_index = vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
				hw->buffer_spec[i].v_canvas_index = hw->buffer_spec[i].u_canvas_index;
			}
#ifdef VDEC_DW
			if (IS_VDEC_DW(hw)) {
				if (hw->buffer_spec[i].vdec_dw_y_canvas_index == -1)
					hw->buffer_spec[i].vdec_dw_y_canvas_index =
						vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
				if (hw->buffer_spec[i].vdec_dw_u_canvas_index == -1) {
					hw->buffer_spec[i].vdec_dw_u_canvas_index =
						vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
					hw->buffer_spec[i].vdec_dw_v_canvas_index =
						hw->buffer_spec[i].vdec_dw_u_canvas_index;
				}
			}
#endif
		} else {
			canvas = vdec->get_canvas(j * mode, 2);
			hw->buffer_spec[i].y_canvas_index = canvas_y(canvas);
			hw->buffer_spec[i].u_canvas_index = canvas_u(canvas);
			hw->buffer_spec[i].v_canvas_index = canvas_v(canvas);
			dpb_print(DECODE_ID(hw),
					PRINT_FLAG_DPB_DETAIL,
					"config canvas (%d) %x for bufspec %d\r\n",
				j, canvas, i);
#ifdef VDEC_DW
		  if (IS_VDEC_DW(hw)) {
			canvas = vdec->get_canvas(j * mode + 1, 2);
			hw->buffer_spec[i].vdec_dw_y_canvas_index = canvas_y(canvas);
			hw->buffer_spec[i].vdec_dw_u_canvas_index = canvas_u(canvas);
			hw->buffer_spec[i].vdec_dw_v_canvas_index = canvas_v(canvas);
			dpb_print(DECODE_ID(hw),
				PRINT_FLAG_DPB_DETAIL,
				"vdec_dw: config canvas (%d) %x for bufspec %d\r\n",
				j, canvas, i);
		  }
#endif
		}

		hw->buffer_spec[i].used = 0;
		hw->buffer_spec[i].canvas_pos = j;


		j++;
	}
	spin_unlock_irqrestore(&hw->bufspec_lock, flags);
}

static void config_buf_specs_ex(struct vdec_s *vdec)
{
	int i, j;
	unsigned long flags;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	int mode = IS_VDEC_DW(hw) ? 2 : 1;

	spin_lock_irqsave(&hw->bufspec_lock, flags);
	for (i = 0, j = 0;
		j < hw->dpb.mDPB.size
		&& i < BUFSPEC_POOL_SIZE;
		i++) {
		int canvas = 0;
		if (hw->buffer_spec[i].used != -1)
			continue;
		if (vdec->parallel_dec == 1) {
			if (hw->buffer_spec[i].y_canvas_index == -1)
				hw->buffer_spec[i].y_canvas_index = vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
			if (hw->buffer_spec[i].u_canvas_index == -1) {
				hw->buffer_spec[i].u_canvas_index = vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
				hw->buffer_spec[i].v_canvas_index = hw->buffer_spec[i].u_canvas_index;
			}
#ifdef VDEC_DW
			if (IS_VDEC_DW(hw)) {
				if (hw->buffer_spec[i].vdec_dw_y_canvas_index == -1)
					hw->buffer_spec[i].vdec_dw_y_canvas_index =
						vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
				if (hw->buffer_spec[i].vdec_dw_u_canvas_index == -1) {
					hw->buffer_spec[i].vdec_dw_u_canvas_index =
						vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
					hw->buffer_spec[i].vdec_dw_v_canvas_index =
						hw->buffer_spec[i].vdec_dw_u_canvas_index;
				}
			}
#endif
		} else {
			canvas = vdec->get_canvas(j* mode, 2);
			hw->buffer_spec[i].y_canvas_index = canvas_y(canvas);
			hw->buffer_spec[i].u_canvas_index = canvas_u(canvas);
			hw->buffer_spec[i].v_canvas_index = canvas_v(canvas);

			dpb_print(DECODE_ID(hw),
					PRINT_FLAG_DPB_DETAIL,
					"config canvas (%d) %x for bufspec %d\r\n",
				j, canvas, i);
#ifdef VDEC_DW
			if (IS_VDEC_DW(hw)) {
				canvas = vdec->get_canvas(j*mode + 1, 2);
				hw->buffer_spec[i].vdec_dw_y_canvas_index = canvas_y(canvas);
				hw->buffer_spec[i].vdec_dw_u_canvas_index = canvas_u(canvas);
				hw->buffer_spec[i].vdec_dw_v_canvas_index = canvas_v(canvas);
				dpb_print(DECODE_ID(hw),
					PRINT_FLAG_DPB_DETAIL,
					"vdec_dw: config canvas (%d) %x for bufspec %d\r\n",
					j, canvas, i);
			}
#endif
		}

		hw->buffer_spec[i].used = 0;
		hw->buffer_spec[i].alloc_header_addr = 0;
		hw->buffer_spec[i].canvas_pos = j;

		j++;
	}
	spin_unlock_irqrestore(&hw->bufspec_lock, flags);
}


static void dealloc_buf_specs(struct vdec_h264_hw_s *hw,
	unsigned char release_all)
{
	int i;
	unsigned long flags;
	unsigned char dealloc_flag = 0;
	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		if (hw->buffer_spec[i].used == 4 ||
			release_all) {
			dealloc_flag = 1;
			dpb_print(DECODE_ID(hw),
				PRINT_FLAG_DPB_DETAIL,
				"%s buf_spec_num %d\n",
				__func__, i
				);
			spin_lock_irqsave
				(&hw->bufspec_lock, flags);
			hw->buffer_spec[i].used = -1;
			spin_unlock_irqrestore
				(&hw->bufspec_lock, flags);
			release_aux_data(hw, i);

			if (!hw->mmu_enable) {
				if (hw->buffer_spec[i].cma_alloc_addr) {
					if (!hw->is_used_v4l) {
						decoder_bmmu_box_free_idx(
							hw->bmmu_box,
							i);
					}
					spin_lock_irqsave
						(&hw->bufspec_lock, flags);
					hw->buffer_spec[i].cma_alloc_addr = 0;
					hw->buffer_spec[i].buf_adr = 0;
					spin_unlock_irqrestore
						(&hw->bufspec_lock, flags);
				}
			} else {
				if (hw->buffer_spec[i].alloc_header_addr) {
					decoder_mmu_box_free_idx(
						hw->mmu_box,
						i);
					spin_lock_irqsave
						(&hw->bufspec_lock, flags);
					hw->buffer_spec[i].
						alloc_header_addr = 0;
					hw->buffer_spec[i].buf_adr = 0;
					spin_unlock_irqrestore
						(&hw->bufspec_lock, flags);
				}
			}
		}
	}
	if (dealloc_flag &&
		dpb_is_debug(DECODE_ID(hw),
		PRINT_FLAG_DUMP_BUFSPEC))
		dump_bufspec(hw, __func__);
	return;
}

unsigned char have_free_buf_spec(struct vdec_s *vdec)
{
	int i;
	unsigned long addr;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	struct aml_vcodec_ctx * ctx = hw->v4l2_ctx;
	int canvas_pos_min = BUFSPEC_POOL_SIZE;
	int index = -1;
	int ret = 0;
	int allocated_count = 0;

	if (hw->is_used_v4l) {
		struct h264_dpb_stru *dpb = &hw->dpb;

		/* trigger to parse head data. */
		if (!hw->v4l_params_parsed)
			return 1;

		if (dpb->mDPB.used_size >= dpb->mDPB.size - 1)
			return 0;

		for (i = 0; i < hw->dpb.mDPB.size; i++) {
			if (hw->buffer_spec[i].used == 0 &&
				hw->buffer_spec[i].vf_ref == 0 &&
				hw->buffer_spec[i].cma_alloc_addr) {
				return 1;
			}
		}

		if (ctx->cap_pool.dec < hw->dpb.mDPB.size) {
			if (v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx) >=
				run_ready_min_buf_num) {
				if (ctx->fb_ops.query(&ctx->fb_ops, &hw->fb_token))
					return 1;
			}
		}

		return 0;
	}

	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		if (hw->mmu_enable)
			addr = hw->buffer_spec[i].alloc_header_addr;
		else
			addr = hw->buffer_spec[i].cma_alloc_addr;
		if (hw->buffer_spec[i].used == 0 &&
			hw->buffer_spec[i].vf_ref == 0) {

			if (addr)
				return 1;
			if (hw->buffer_spec[i].canvas_pos < canvas_pos_min) {
				canvas_pos_min = hw->buffer_spec[i].canvas_pos;
				index = i;
			}
		}
		if (addr)
			allocated_count++;
	}
	if (index >= 0) {
		mutex_lock(&vmh264_mutex);
		dealloc_buf_specs(hw, 0);
		if (max_alloc_buf_count == 0 ||
			allocated_count < max_alloc_buf_count) {
			if (alloc_one_buf_spec(hw, index) >= 0)
				ret = 1;
		}
		mutex_unlock(&vmh264_mutex);
	}

	return ret;
}

static int get_buf_spec_by_canvas_pos(struct vdec_h264_hw_s *hw,
	int canvas_pos)
{
	int i;
	int j = 0;
	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		if (hw->buffer_spec[i].canvas_pos >= 0) {
			if (j == canvas_pos)
				return i;
			j++;
		}
	}
	return -1;
}
static void update_vf_memhandle(struct vdec_h264_hw_s *hw,
	struct vframe_s *vf, int index)
{
	if (index < 0) {
		vf->mem_handle = NULL;
		vf->mem_head_handle = NULL;
	} else if (vf->type & VIDTYPE_SCATTER) {
		vf->mem_handle =
			decoder_mmu_box_get_mem_handle(
				hw->mmu_box, index);
		vf->mem_head_handle =
			decoder_bmmu_box_get_mem_handle(
				hw->bmmu_box, HEADER_BUFFER_IDX(index));
	} else {
		vf->mem_handle =
			decoder_bmmu_box_get_mem_handle(
				hw->bmmu_box, VF_BUFFER_IDX(index));
	/*	vf->mem_head_handle =
			decoder_bmmu_box_get_mem_handle(
				hw->bmmu_box, HEADER_BUFFER_IDX(index));*/
	}
	return;
}
static int check_force_interlace(struct vdec_h264_hw_s *hw,
	struct FrameStore *frame)
{
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int bForceInterlace = 0;
	/* no di in secure mode, disable force di */
	if (vdec_secure(hw_to_vdec(hw)))
		return 0;

	if (hw->i_only)
		return 0;

	if ((dec_control & DEC_CONTROL_FLAG_FORCE_2997_1080P_INTERLACE)
		&& hw->bitstream_restriction_flag
		&& (hw->frame_width == 1920)
		&& (hw->frame_height >= 1080) /* For being compatible with a fake progressive stream which is interlaced actually*/
		&& (hw->frame_dur == 3203 || (hw->frame_dur == 3840 && p_H264_Dpb->mSPS.profile_idc == 100 &&
		p_H264_Dpb->mSPS.level_idc == 40))) {
		bForceInterlace = 1;
	} else if ((dec_control & DEC_CONTROL_FLAG_FORCE_2500_576P_INTERLACE)
			 && (hw->frame_width == 720)
			 && (hw->frame_height == 576)
			 && (hw->frame_dur == 3840)) {
		bForceInterlace = 1;
	}

	return bForceInterlace;
}

static void fill_frame_info(struct vdec_h264_hw_s *hw, struct FrameStore *frame)
{
	struct vframe_qos_s *vframe_qos = &hw->vframe_qos;

	if (frame->slice_type == I_SLICE)
		vframe_qos->type = 1;
	else if (frame->slice_type == P_SLICE)
		vframe_qos->type = 2;
	else if (frame->slice_type == B_SLICE)
		vframe_qos->type = 3;

	if (input_frame_based(hw_to_vdec(hw)))
		vframe_qos->size = frame->frame_size2;
	else
		vframe_qos->size = frame->frame_size;
	vframe_qos->pts = frame->pts64;

	vframe_qos->max_mv = frame->max_mv;
	vframe_qos->avg_mv = frame->avg_mv;
	vframe_qos->min_mv = frame->min_mv;
/*
	pr_info("mv: max:%d,  avg:%d, min:%d\n",
		vframe_qos->max_mv,
		vframe_qos->avg_mv,
		vframe_qos->min_mv);
*/

	vframe_qos->max_qp = frame->max_qp;
	vframe_qos->avg_qp = frame->avg_qp;
	vframe_qos->min_qp = frame->min_qp;
/*
	pr_info("qp: max:%d,  avg:%d, min:%d\n",
		vframe_qos->max_qp,
		vframe_qos->avg_qp,
		vframe_qos->min_qp);
*/

	vframe_qos->max_skip = frame->max_skip;
	vframe_qos->avg_skip = frame->avg_skip;
	vframe_qos->min_skip = frame->min_skip;
/*
	pr_info("skip: max:%d,  avg:%d, min:%d\n",
		vframe_qos->max_skip,
		vframe_qos->avg_skip,
		vframe_qos->min_skip);
*/
	vframe_qos->num++;
}

static int is_iframe(struct FrameStore *frame) {

	if (frame->frame && frame->frame->slice_type == I_SLICE) {
		return 1;
	}
	return 0;
}

static int post_prepare_process(struct vdec_s *vdec, struct FrameStore *frame)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	int buffer_index = frame->buf_spec_num;

	if (buffer_index < 0 || buffer_index >= BUFSPEC_POOL_SIZE) {
		dpb_print(DECODE_ID(hw), 0,
			"%s, buffer_index 0x%x is beyond range\n",
			__func__, buffer_index);
		return -1;
	}

	if (force_disp_bufspec_num & 0x100) {
		/*recycle directly*/
		if (hw->buffer_spec[frame->buf_spec_num].used != 3 &&
			hw->buffer_spec[frame->buf_spec_num].used != 5)
			set_frame_output_flag(&hw->dpb, frame->index);

		/*make pre_output not set*/
		return -1;
	}
	if (hw->error_proc_policy & 0x1000) {
		int error_skip_i_count = (error_skip_count >> 12) & 0xf;
		int error_skip_frame_count = error_skip_count & 0xfff;
		if (((hw->no_error_count < error_skip_frame_count)
			&& (error_skip_i_count == 0 ||
			hw->no_error_i_count < error_skip_i_count))
			&& (!(frame->data_flag & I_FLAG)))
			frame->data_flag |= ERROR_FLAG;
	}

	dpb_print(DECODE_ID(hw), PRINT_FLAG_ERRORFLAG_DBG,
		"%s, buffer_index 0x%x  frame_error %x  poc %d hw error %x error_proc_policy %x\n",
		__func__, buffer_index,
		frame->data_flag & ERROR_FLAG,
		frame->poc, hw->data_flag & ERROR_FLAG,
		hw->error_proc_policy);

	if (frame->frame == NULL &&
			((frame->is_used == 1 && frame->top_field)
			|| (frame->is_used == 2 && frame->bottom_field))) {
			if (hw->i_only) {
				if (frame->is_used == 1)
					dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
						"%s   No bottom_field !!  frame_num %d  used %d\n",
						__func__, frame->frame_num, frame->is_used);
				if (frame->is_used == 2)
					dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
						"%s   No top_field !!  frame_num %d  used %d\n",
						__func__, frame->frame_num, frame->is_used);
			}
			else {
				frame->data_flag |= ERROR_FLAG;
					dpb_print(DECODE_ID(hw), PRINT_FLAG_ERRORFLAG_DBG,
					"%s Error  frame_num %d  used %d\n",
					__func__, frame->frame_num, frame->is_used);
			}
	}
	if (vdec_stream_based(vdec) && !(frame->data_flag & NODISP_FLAG)) {
		if ((vdec->vbuf.no_parser == 0) || (vdec->vbuf.use_ptsserv)) {
			if ((pts_lookup_offset_us64(PTS_TYPE_VIDEO,
				frame->offset_delimiter, &frame->pts, &frame->frame_size,
				0, &frame->pts64) == 0)) {
				if ((lookup_check_conut && (hw->vf_pre_count > lookup_check_conut) &&
					(hw->wrong_frame_count > hw->right_frame_count)) &&
					((frame->decoded_frame_size * 2 < frame->frame_size))) {
					/*resolve many frame only one check in pts, cause playback unsmooth issue*/
					frame->pts64 = hw->last_pts64 +(DUR2PTS(hw->frame_dur) * 100 / 9);
					frame->pts = hw->last_pts + DUR2PTS(hw->frame_dur);
				}
				hw->right_frame_count++;
			} else {
				frame->pts64 = hw->last_pts64 + (DUR2PTS(hw->frame_dur) * 100 / 9);
				frame->pts = hw->last_pts + DUR2PTS(hw->frame_dur);
				hw->wrong_frame_count++;
			}
		}

		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
		"%s error= 0x%x poc = %d  offset= 0x%x pts= 0x%x last_pts =0x%x  pts64 = %lld  last_pts64= %lld  duration = %d\n",
		__func__, (frame->data_flag & ERROR_FLAG), frame->poc,
		frame->offset_delimiter, frame->pts,hw->last_pts,
		frame->pts64, hw->last_pts64, hw->frame_dur);
		hw->last_pts64 = frame->pts64;
		hw->last_pts = frame->pts;
	}

	/* SWPL-18973 96000/15=6400, less than 15fps check */
	if ((!hw->duration_from_pts_done) && (hw->frame_dur > 6400ULL)) {
		if ((check_force_interlace(hw, frame)) &&
			(frame->slice_type == I_SLICE) &&
			(hw->pts_outside)) {
			if ((!hw->h264_pts_count) || (!hw->h264pts1)) {
				hw->h264pts1 = frame->pts;
				hw->h264_pts_count = 0;
			} else if (frame->pts > hw->h264pts1) {
				u32 calc_dur =
					PTS2DUR(frame->pts - hw->h264pts1);
				calc_dur = ((calc_dur/hw->h264_pts_count) << 1);
				if (hw->frame_dur < (calc_dur + 200) &&
					hw->frame_dur > (calc_dur - 200)) {
					hw->frame_dur >>= 1;
					vdec_schedule_work(&hw->notify_work);
					dpb_print(DECODE_ID(hw), 0,
						"correct frame_dur %d, calc_dur %d, count %d\n",
						hw->frame_dur, (calc_dur >> 1), hw->h264_pts_count);
					hw->duration_from_pts_done = 1;
					hw->h264_pts_count = 0;
				}
			}
		}
		hw->h264_pts_count++;
	}

	if (frame->data_flag & ERROR_FLAG) {
		vdec_count_info(&hw->gvs, 1, 0);
		if (frame->slice_type == I_SLICE) {
			hw->gvs.i_concealed_frames++;
		} else if (frame->slice_type == P_SLICE) {
			hw->gvs.p_concealed_frames++;
		} else if (frame->slice_type == B_SLICE) {
			hw->gvs.b_concealed_frames++;
		}
		if (!hw->send_error_frame_flag) {
			hw->gvs.drop_frame_count++;
			if (frame->slice_type == I_SLICE) {
				hw->gvs.i_lost_frames++;
			} else if (frame->slice_type == P_SLICE) {
				hw->gvs.p_lost_frames++;
			} else if (frame->slice_type == B_SLICE) {
				hw->gvs.b_lost_frames++;
			}
		}

	}

	if ((!hw->enable_fence) &&
		((frame->data_flag & NODISP_FLAG) ||
		(frame->data_flag & NULL_FLAG) ||
		((!hw->send_error_frame_flag) &&
		(frame->data_flag & ERROR_FLAG)) ||
		((hw->i_only & 0x1) &&
		(!(frame->data_flag & I_FLAG))))) {
		frame->show_frame = false;
		return 0;
	}

	if (dpb_is_debug(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL)) {
		dpb_print(DECODE_ID(hw), 0,
			"%s, fs[%d] poc %d, buf_spec_num %d\n",
			__func__, frame->index, frame->poc,
			frame->buf_spec_num);
		print_pic_info(DECODE_ID(hw), "predis_frm",
			frame->frame, -1);
		print_pic_info(DECODE_ID(hw), "predis_top",
			frame->top_field, -1);
		print_pic_info(DECODE_ID(hw), "predis_bot",
			frame->bottom_field, -1);
	}

	frame->show_frame = true;

	return 0;
}

static int post_video_frame(struct vdec_s *vdec, struct FrameStore *frame)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	struct vframe_s *vf = NULL;
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int buffer_index = frame->buf_spec_num;
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;
	struct vdec_v4l2_buffer *fb = NULL;
	ulong nv_order = VIDTYPE_VIU_NV21;
	int bForceInterlace = 0;
	int vf_count = 1;
	int i;
	struct buffer_spec_s *pic = &hw->buffer_spec[buffer_index];

	/* swap uv */
	if (hw->is_used_v4l) {
		if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12) ||
			(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12M))
			nv_order = VIDTYPE_VIU_NV12;
	}

	if (!is_interlace(frame))
		vf_count = 1;
	else
		vf_count = 2;

	bForceInterlace = check_force_interlace(hw, frame);
	if (bForceInterlace)
		vf_count = 2;

	if (!hw->enable_fence)
		hw->buffer_spec[buffer_index].vf_ref = 0;
	fill_frame_info(hw, frame);

	if ((hw->is_used_v4l) &&
		(vdec->prog_only))
		vf_count = 1;

	for (i = 0; i < vf_count; i++) {
		if (kfifo_get(&hw->newframe_q, &vf) == 0 ||
			vf == NULL) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
				"%s fatal error, no available buffer slot.\n",
				__func__);
			return -1;
		}
		vf->frame_type = 0;
		vf->duration_pulldown = 0;
		if (!(is_iframe(frame)) && hw->unstable_pts) {
			vf->pts = 0;
			vf->pts_us64 = 0;
			vf->timestamp = 0;
			vf->index = VF_INDEX(frame->index, buffer_index);
		} else {
			vf->pts = frame->pts;
			vf->pts_us64 = frame->pts64;
			vf->timestamp = frame->timestamp;
			vf->index = VF_INDEX(frame->index, buffer_index);
		}

		if (hw->is_used_v4l) {
			vf->v4l_mem_handle
				= hw->buffer_spec[buffer_index].cma_alloc_addr;
			fb = (struct vdec_v4l2_buffer *)vf->v4l_mem_handle;
		}

		if (hw->enable_fence) {
			/* fill fence information. */
			if (hw->fence_usage == FENCE_USE_FOR_DRIVER)
				vf->fence	= frame->fence;
		}

		if (hw->mmu_enable) {
			if (hw->double_write_mode & 0x10) {
				/* double write only */
				vf->compBodyAddr = 0;
				vf->compHeadAddr = 0;
			} else {
				/*head adr*/
				vf->compHeadAddr =
				hw->buffer_spec[buffer_index].alloc_header_addr;
				/*body adr*/
				vf->compBodyAddr = 0;
				vf->canvas0Addr = vf->canvas1Addr = 0;
			}

			vf->type = VIDTYPE_SCATTER;

			if (hw->double_write_mode) {
				vf->type |= VIDTYPE_PROGRESSIVE
					| VIDTYPE_VIU_FIELD;
				vf->type |= nv_order;
				if (hw->double_write_mode == 3)
					vf->type |= VIDTYPE_COMPRESS;

				vf->canvas0Addr = vf->canvas1Addr = -1;
				vf->plane_num = 2;
				vf->canvas0_config[0] =
					hw->buffer_spec[buffer_index].
						canvas_config[0];
				vf->canvas0_config[1] =
					hw->buffer_spec[buffer_index].
						canvas_config[1];

				vf->canvas1_config[0] =
					hw->buffer_spec[buffer_index].
						canvas_config[0];
				vf->canvas1_config[1] =
					hw->buffer_spec[buffer_index].
						canvas_config[1];

			} else {
				vf->type |=
					VIDTYPE_COMPRESS | VIDTYPE_VIU_FIELD;
				vf->canvas0Addr = vf->canvas1Addr = 0;
			}

			vf->bitdepth =
				BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;

			vf->compWidth = hw->frame_width;
			vf->compHeight = hw->frame_height;
		} else {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				nv_order;

			vf->canvas0Addr = vf->canvas1Addr =
			spec2canvas(&hw->buffer_spec[buffer_index]);
#ifdef VDEC_DW
			if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_T7) {
				if (IS_VDEC_DW(hw))
					vf->canvas0Addr = vf->canvas1Addr =
						vdec_dw_spec2canvas(&hw->buffer_spec[buffer_index]);
			} else {
				if (IS_VDEC_DW(hw))
					vf->canvas0Addr = vf->canvas1Addr = -1;
			}
#endif

		}
		set_frame_info(hw, vf, buffer_index);

		if (hw->mmu_enable && hw->double_write_mode) {
			vf->width = hw->frame_width /
				get_double_write_ratio(hw->double_write_mode);
			vf->height = hw->frame_height /
				get_double_write_ratio(hw->double_write_mode);
		}

		if (frame->slice_type == I_SLICE) {
			vf->frame_type |= V4L2_BUF_FLAG_KEYFRAME;
		} else if (frame->slice_type == P_SLICE) {
			vf->frame_type |= V4L2_BUF_FLAG_PFRAME;
		} else if (frame->slice_type == B_SLICE) {
			vf->frame_type |= V4L2_BUF_FLAG_BFRAME;
		}

		vf->flag = 0;
		if (frame->data_flag & I_FLAG)
			vf->flag |= VFRAME_FLAG_SYNCFRAME;
		if (frame->data_flag & ERROR_FLAG)
			vf->flag |= VFRAME_FLAG_ERROR_RECOVERY;
		update_vf_memhandle(hw, vf, buffer_index);

		if (hw->high_bandwidth_flag) {
			vf->flag |= VFRAME_FLAG_HIGH_BANDWIDTH;
		}

		if (!hw->enable_fence) {
			hw->buffer_spec[buffer_index].used = 2;
			hw->buffer_spec[buffer_index].vf_ref++;
		}

		dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
			"%s %d frame = %p top_field = %p bottom_field = %p\n", __func__, __LINE__, frame->frame,
			frame->top_field, frame->bottom_field);

		if (frame->frame != NULL) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
				"%s %d coded_frame = %d frame_mbs_only_flag = %d structure = %d\n", __func__, __LINE__,
				frame->frame->coded_frame, frame->frame->frame_mbs_only_flag, frame->frame->structure);
		}

		if (bForceInterlace || is_interlace(frame) || (!p_H264_Dpb->mSPS.frame_mbs_only_flag)) {
			vf->type =
				VIDTYPE_INTERLACE_FIRST |
				nv_order;

			if (frame->frame != NULL &&
				(frame->frame->pic_struct == PIC_TOP_BOT ||
				frame->frame->pic_struct == PIC_BOT_TOP) &&
				frame->frame->coded_frame) {
				if (frame->frame != NULL && frame->frame->pic_struct == PIC_TOP_BOT) {
				vf->type |= (i == 0 ?
					VIDTYPE_INTERLACE_TOP :
					VIDTYPE_INTERLACE_BOTTOM);
				} else if (frame->frame != NULL && frame->frame->pic_struct == PIC_BOT_TOP) {
					vf->type |= (i == 0 ?
					VIDTYPE_INTERLACE_BOTTOM :
					VIDTYPE_INTERLACE_TOP);
				}
			} else if (frame->top_field != NULL && frame->bottom_field != NULL) {/*top first*/
				if (frame->top_field->poc <= frame->bottom_field->poc)
					vf->type |= (i == 0 ?
						VIDTYPE_INTERLACE_TOP :
						VIDTYPE_INTERLACE_BOTTOM);
				else
					vf->type |= (i == 0 ?
						VIDTYPE_INTERLACE_BOTTOM :
						VIDTYPE_INTERLACE_TOP);
			} else if (frame->frame == NULL) {
				if (frame->top_field != NULL) {
					vf->type |= VIDTYPE_INTERLACE_TOP;
				} else {
					vf->type |= VIDTYPE_INTERLACE_BOTTOM;
				}
			} else {
				vf->type |= (i == 0 ?
					VIDTYPE_INTERLACE_TOP :
					VIDTYPE_INTERLACE_BOTTOM);
			}
			vf->duration = vf->duration/2;
			if (i == 1) {
				vf->pts = 0;
				vf->pts_us64 = 0;
			}

			if (frame->frame) {
				dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
					"%s %d type = 0x%x pic_struct = %d pts = 0x%x pts_us64 = 0x%llx bForceInterlace = %d\n",
					__func__, __LINE__, vf->type, frame->frame->pic_struct,
					vf->pts, vf->pts_us64, bForceInterlace);
			}
		}

		if (hw->i_only) {
			if (vf_count == 1 && frame->is_used == 1 && frame->top_field
				&& frame->bottom_field == NULL && frame->frame == NULL) {
				vf->type =
					VIDTYPE_INTERLACE_FIRST |
					nv_order;
				vf->type |= VIDTYPE_INTERLACE_TOP;
				vf->duration = vf->duration/2;
			}

			if (vf_count == 1 && frame->is_used == 2 && frame->bottom_field
				&& frame->top_field == NULL && frame->frame == NULL) {
				vf->type =
					VIDTYPE_INTERLACE_FIRST |
					nv_order;
				vf->type |= VIDTYPE_INTERLACE_BOTTOM;
				vf->duration = vf->duration/2;
			}
		}

		/*vf->ratio_control |= (0x3FF << DISP_RATIO_ASPECT_RATIO_BIT);*/
		vf->sar_width = hw->width_aspect_ratio;
		vf->sar_height = hw->height_aspect_ratio;
		if (!vdec->vbuf.use_ptsserv && vdec_stream_based(vdec)) {
			/* offset for tsplayer pts lookup */
			if (i == 0) {
				vf->pts_us64 =
					(((u64)vf->duration << 32) &
					0xffffffff00000000) | frame->offset_delimiter;
				vf->pts = 0;
			} else {
				vf->pts_us64 = (u64)-1;
				vf->pts = 0;
			}
		}

		hw->vf_pre_count++;
		vdec_vframe_ready(hw_to_vdec(hw), vf);
		if (!frame->show_frame) {
			vh264_vf_put(vf, vdec);
			atomic_add(1, &hw->vf_get_count);
			continue;
		}

		vf->src_fmt.play_id = vdec->inst_cnt;

		if (i == 0) {
			struct vdec_s *pvdec;
			struct vdec_info vs;

			pvdec = hw_to_vdec(hw);
			memset(&vs, 0, sizeof(struct vdec_info));
			pvdec->dec_status(pvdec, &vs);
			decoder_do_frame_check(pvdec, vf);
			vdec_fill_vdec_frame(pvdec, &hw->vframe_qos, &vs, vf, frame->hw_decode_time);

			dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
			"[%s:%d] i_decoded_frame = %d p_decoded_frame = %d b_decoded_frame = %d\n",
			__func__, __LINE__,vs.i_decoded_frames,vs.p_decoded_frames,vs.b_decoded_frames);
		}

		vf->vf_ud_param.magic_code = UD_MAGIC_CODE;
		vf->vf_ud_param.ud_param = pic->ud_param;

		if (dpb_is_debug(DECODE_ID(hw), PRINT_FLAG_UD_DETAIL))
		{
			struct userdata_param_t ud_param = pic->ud_param;
			{
				int i = 0;
				u8 *pstart = (u8 *)ud_param.pbuf_addr;
				PR_INIT(128);
				dpb_print(DECODE_ID(hw), 0, "%s:userdata len %d. vdec %p video_id %d\n",
					__func__,ud_param.buf_len, vdec, ud_param.instance_id);

				for (i = 0; i < ud_param.buf_len; i++) {
					PR_FILL("%02x ", pstart[i]);
					if (((i + 1) & 0xf) == 0)
						PR_INFO(DECODE_ID(hw));
				}
				PR_INFO(DECODE_ID(hw));
			}
		}

		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s: index %d poc %d frame_type %d dur %d type %x pts %d(0x%x), pts64 %lld(0x%x) ts %lld(0x%x)\n",
			__func__, vf->index, frame->poc, vf->frame_type, vf->duration, vf->type, vf->pts, vf->pts,
			vf->pts_us64, vf->pts_us64, vf->timestamp, vf->timestamp);

		kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
		ATRACE_COUNTER(hw->trace.pts_name, vf->pts);
		ATRACE_COUNTER(hw->trace.disp_q_name, kfifo_len(&hw->display_q));
		ATRACE_COUNTER(hw->trace.new_q_name, kfifo_len(&hw->newframe_q));
		hw->vf_pre_count++;
		vdec->vdec_fps_detec(vdec->id);
#ifdef AUX_DATA_CRC
		decoder_do_aux_data_check(vdec, hw->buffer_spec[buffer_index].aux_data_buf,
			hw->buffer_spec[buffer_index].aux_data_size);
#endif

		dpb_print(DECODE_ID(hw), PRINT_FLAG_SEI_DETAIL,
			"aux_data_size:%d signal_type:0x%x inst_cnt:%d vf:%p\n",
			hw->buffer_spec[buffer_index].aux_data_size, hw->video_signal_type,
			vdec->inst_cnt, vf);

		if (dpb_is_debug(DECODE_ID(hw), PRINT_FLAG_SEI_DETAIL)) {
			int i = 0;
			PR_INIT(128);

			for (i = 0; i < hw->buffer_spec[buffer_index].aux_data_size; i++) {
				PR_FILL("%02x ", hw->buffer_spec[buffer_index].aux_data_buf[i]);
				if (((i + 1) & 0xf) == 0)
					PR_INFO(hw->id);
			}
			PR_INFO(hw->id);
		}

		if (hw->is_used_v4l) {
			update_vframe_src_fmt(vf,
				hw->buffer_spec[buffer_index].aux_data_buf,
				hw->buffer_spec[buffer_index].aux_data_size,
				false, vdec->vf_provider_name, NULL);
		}

		if (without_display_mode == 0) {
			if (hw->is_used_v4l) {
				if (v4l2_ctx->is_stream_off) {
					vh264_vf_put(vh264_vf_get(vdec), vdec);
				} else {
					fb->task->submit(fb->task, TASK_TYPE_DEC);
				}
			} else
				vf_notify_receiver(vdec->vf_provider_name,
					VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
		} else
			vh264_vf_put(vh264_vf_get(vdec), vdec);
	}
	if (dpb_is_debug(DECODE_ID(hw),
		PRINT_FLAG_DUMP_BUFSPEC))
		dump_bufspec(hw, __func__);

	return 0;
}

int post_picture_early(struct vdec_s *vdec, int index)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	struct h264_dpb_stru *dpb_stru = &hw->dpb;
	struct FrameStore fs;
	u32 offset_lo, offset_hi;

	if (!hw->enable_fence)
		return 0;

	/* create fence for each buffers. */
	if (vdec_timeline_create_fence(vdec->sync))
		return -1;

	memset(&fs, 0, sizeof(fs));

	fs.buf_spec_num		= index;
	fs.fence		= vdec->sync->fence;
	fs.slice_type		= dpb_stru->mSlice.slice_type;
	fs.dpb_frame_count	= dpb_stru->dpb_frame_count;

	offset_lo = dpb_stru->dpb_param.l.data[OFFSET_DELIMITER_LO];
	offset_hi = dpb_stru->dpb_param.l.data[OFFSET_DELIMITER_HI];
	fs.offset_delimiter	= (offset_lo | offset_hi << 16);

	if (hw->chunk) {
		fs.pts		= hw->chunk->pts;
		fs.pts64	= hw->chunk->pts64;
		fs.timestamp	= hw->chunk->timestamp;
	}
	fs.show_frame = true;
	post_video_frame(vdec, &fs);

	display_frame_count[DECODE_ID(hw)]++;
	return 0;
}

int prepare_display_buf(struct vdec_s *vdec, struct FrameStore *frame)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)vdec->private;

	if (hw->enable_fence) {
		int i, j, used_size, ret;
		int signed_count = 0;
		struct vframe_s *signed_fence[VF_POOL_SIZE];

		post_prepare_process(vdec, frame);

		if (!frame->show_frame)
			pr_info("do not display.\n");

		hw->buffer_spec[frame->buf_spec_num].used = 2;
		hw->buffer_spec[frame->buf_spec_num].vf_ref = 1;
		hw->buffer_spec[frame->buf_spec_num].fs_idx = frame->index;

		/* notify signal to wake up wq of fence. */
		vdec_timeline_increase(vdec->sync, 1);

		mutex_lock(&hw->fence_mutex);
		used_size = hw->fence_vf_s.used_size;
		if (used_size) {
			for (i = 0, j = 0; i < VF_POOL_SIZE && j < used_size; i++) {
				if (hw->fence_vf_s.fence_vf[i] != NULL) {
					ret = dma_fence_get_status(hw->fence_vf_s.fence_vf[i]->fence);
					if (ret == 1) {
						signed_fence[signed_count] = hw->fence_vf_s.fence_vf[i];
						hw->fence_vf_s.fence_vf[i] = NULL;
						hw->fence_vf_s.used_size--;
						signed_count++;
					}
					j++;
				}
			}
		}
		mutex_unlock(&hw->fence_mutex);
		if (signed_count != 0) {
			for (i = 0; i < signed_count; i++)
				vh264_vf_put(signed_fence[i], vdec);
		}

		return 0;
	}

	if (post_prepare_process(vdec, frame))
		return -1;

	if (post_video_frame(vdec, frame))
		return -1;

	display_frame_count[DECODE_ID(hw)]++;
	return 0;
}

int notify_v4l_eos(struct vdec_s *vdec)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct vframe_s *vf = &hw->vframe_dummy;
	struct vdec_v4l2_buffer *fb = NULL;
	int index = INVALID_IDX;
	ulong expires;

	if (hw->eos) {
		if (hw->is_used_v4l) {
			expires = jiffies + msecs_to_jiffies(2000);
			while (INVALID_IDX == (index = v4l_get_free_buf_idx(vdec))) {
				if (time_after(jiffies, expires) ||
					v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx))
					break;
			}

			if (index == INVALID_IDX) {
				ctx->fb_ops.query(&ctx->fb_ops, &hw->fb_token);
				if (ctx->fb_ops.alloc(&ctx->fb_ops, hw->fb_token, &fb, AML_FB_REQ_DEC) < 0) {
					pr_err("[%d] EOS get free buff fail.\n", ctx->id);
					return -1;
				}
			}
		}

		vf->type		|= VIDTYPE_V4L_EOS;
		vf->timestamp		= ULONG_MAX;
		vf->flag		= VFRAME_FLAG_EMPTY_FRAME_V4L;
		vf->v4l_mem_handle	= (index == INVALID_IDX) ? (ulong)fb :
					hw->buffer_spec[index].cma_alloc_addr;
		fb = (struct vdec_v4l2_buffer *)vf->v4l_mem_handle;

		vdec_vframe_ready(vdec, vf);
		kfifo_put(&hw->display_q, (const struct vframe_s *)vf);

		ATRACE_COUNTER(hw->trace.pts_name, vf->pts);

		if (hw->is_used_v4l)
			fb->task->submit(fb->task, TASK_TYPE_DEC);
		else
			vf_notify_receiver(vdec->vf_provider_name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);

		pr_info("[%d] H264 EOS notify.\n", (hw->is_used_v4l)?ctx->id:vdec->id);
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
	if (pic)
		dpb_print(decindex, PRINT_FLAG_DEC_DETAIL,
		"%s: %s (original %s), %s, mb_aff_frame_flag %d  poc %d, pic_num %d, buf_spec_num %d data_flag 0x%x\n",
		info,
		picture_structure_name[pic->structure],
		pic->coded_frame ? "Frame" : "Field",
		(slice_type < 0 ||
		slice_type >= (sizeof(slice_type_name) / sizeof(slice_type_name[0]))) ? "" : slice_type_name[slice_type],
		pic->mb_aff_frame_flag,
		pic->poc,
		pic->pic_num,
		pic->buf_spec_num,
		pic->data_flag);
}

static void reset_process_time(struct vdec_h264_hw_s *hw)
{
	if (hw->start_process_time) {
		unsigned process_time =
			1000 * (jiffies - hw->start_process_time) / HZ;
		hw->start_process_time = 0;
		if (process_time > max_process_time[DECODE_ID(hw)])
			max_process_time[DECODE_ID(hw)] = process_time;
	}
}

static void start_process_time(struct vdec_h264_hw_s *hw)
{
	hw->decode_timeout_count = 10;
	hw->start_process_time = jiffies;
}

static void config_aux_buf(struct vdec_h264_hw_s *hw)
{
	WRITE_VREG(H264_AUX_ADR, hw->aux_phy_addr);
	WRITE_VREG(H264_AUX_DATA_SIZE,
		((hw->prefix_aux_size >> 4) << 16) |
		(hw->suffix_aux_size >> 4)
		);
}

/*
* dv_meta_flag: 1, dolby meta only; 2, not include dolby meta
*/
static void set_aux_data(struct vdec_h264_hw_s *hw,
	struct StorablePicture *pic, unsigned char suffix_flag,
	unsigned char dv_meta_flag, struct vdec_h264_hw_s *hw_b)
{
	int i;
	unsigned short *aux_adr;
	unsigned size_reg_val =
		READ_VREG(H264_AUX_DATA_SIZE);
	unsigned aux_count = 0;
	int aux_size = 0;
	struct vdec_h264_hw_s *hw_buf = hw_b ? hw_b : hw;
	if (pic == NULL || pic->buf_spec_num < 0 || pic->buf_spec_num >= BUFSPEC_POOL_SIZE
		|| (!is_buf_spec_in_use(hw, pic->buf_spec_num)))
		return;

	if (suffix_flag) {
		aux_adr = (unsigned short *)
			(hw_buf->aux_addr +
			hw_buf->prefix_aux_size);
		aux_count =
		((size_reg_val & 0xffff) << 4)
			>> 1;
		aux_size =
			hw_buf->suffix_aux_size;
	} else {
		aux_adr =
		(unsigned short *)hw_buf->aux_addr;
		aux_count =
		((size_reg_val >> 16) << 4)
			>> 1;
		aux_size =
			hw_buf->prefix_aux_size;
	}
	if (dpb_is_debug(DECODE_ID(hw),
		 PRINT_FLAG_SEI_DETAIL)) {
		dpb_print(DECODE_ID(hw), 0,
			"%s:poc %d old size %d count %d,suf %d dv_flag %d\r\n",
			__func__, pic->poc, AUX_DATA_SIZE(pic),
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
		new_size = AUX_DATA_SIZE(pic) + aux_count + heads_size;
		new_buf = krealloc(AUX_DATA_BUF(pic),
			new_size,
			GFP_KERNEL);
		if (new_buf) {
			unsigned char valid_tag = 0;
			unsigned char *h =
				new_buf +
				AUX_DATA_SIZE(pic);
			unsigned char *p = h + 8;
			int len = 0;
			int padding_len = 0;
			AUX_DATA_BUF(pic) = new_buf;
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
						AUX_DATA_SIZE(pic) +=
						(len + 8);
						h[0] =
						(len >> 24) & 0xff;
						h[1] =
						(len >> 16) & 0xff;
						h[2] =
						(len >> 8) & 0xff;
						h[3] =
						(len >> 0) & 0xff;
						h[6] =
						(padding_len >> 8)
						& 0xff;
						h[7] =
						(padding_len) & 0xff;
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
				AUX_DATA_SIZE(pic) += (len + 8);
				h[0] = (len >> 24) & 0xff;
				h[1] = (len >> 16) & 0xff;
				h[2] = (len >> 8) & 0xff;
				h[3] = (len >> 0) & 0xff;
				h[6] = (padding_len >> 8) & 0xff;
				h[7] = (padding_len) & 0xff;
			}
			if (dpb_is_debug(DECODE_ID(hw),
				PRINT_FLAG_SEI_DETAIL)) {
				dpb_print(DECODE_ID(hw), 0,
					"aux: (size %d) suffix_flag %d\n",
					AUX_DATA_SIZE(pic), suffix_flag);
				for (i = 0; i < AUX_DATA_SIZE(pic); i++) {
					dpb_print_cont(DECODE_ID(hw), 0,
						"%02x ", AUX_DATA_BUF(pic)[i]);
					if (((i + 1) & 0xf) == 0)
						dpb_print_cont(
						DECODE_ID(hw),
							0, "\n");
				}
				dpb_print_cont(DECODE_ID(hw),
					0, "\n");
			}

		}
	}

}

static void release_aux_data(struct vdec_h264_hw_s *hw,
	int buf_spec_num)
{
	kfree(hw->buffer_spec[buf_spec_num].aux_data_buf);
	hw->buffer_spec[buf_spec_num].aux_data_buf = NULL;
	hw->buffer_spec[buf_spec_num].aux_data_size = 0;
}

static void dump_aux_buf(struct vdec_h264_hw_s *hw)
{
	int i;
	unsigned short *aux_adr =
		(unsigned short *)
		hw->aux_addr;
	unsigned aux_size =
		(READ_VREG(H264_AUX_DATA_SIZE)
		>> 16) << 4;

	if (hw->prefix_aux_size > 0) {
		dpb_print(DECODE_ID(hw),
			0,
			"prefix aux: (size %d)\n",
			aux_size);
		for (i = 0; i <
		(aux_size >> 1); i++) {
			dpb_print_cont(DECODE_ID(hw),
				0,
				"%04x ",
				*(aux_adr + i));
			if (((i + 1) & 0xf)
				== 0)
				dpb_print_cont(
				DECODE_ID(hw),
				0, "\n");
		}
	}
	if (hw->suffix_aux_size > 0) {
		aux_adr = (unsigned short *)
			(hw->aux_addr +
			hw->prefix_aux_size);
		aux_size =
		(READ_VREG(H264_AUX_DATA_SIZE) & 0xffff)
			<< 4;
		dpb_print(DECODE_ID(hw),
			0,
			"suffix aux: (size %d)\n",
			aux_size);
		for (i = 0; i <
		(aux_size >> 1); i++) {
			dpb_print_cont(DECODE_ID(hw),
				0,
				"%04x ", *(aux_adr + i));
			if (((i + 1) & 0xf) == 0)
				dpb_print_cont(DECODE_ID(hw),
				0, "\n");
		}
	}
}

#ifdef VDEC_DW

struct vdec_dw_param_set{
	char dw_x_shrink_1st;
	char dw_x_shrink_2nd;
	char dw_x_shrink_3rd;
	char dw_y_shrink_1st;
	char dw_y_shrink_2nd;
	char dw_y_shrink_3rd;
	char dw_merge_8to16;
	char dw_merge_16to32;
	char dw_dma_blk_mode;
	char dw_bwsave_mode;
};
//#define FOR_LPDDR4_EFFICIENCY

static void h264_vdec_dw_cfg(struct vdec_h264_hw_s *hw, int canvas_pos)
{
	u32 data32 = 0, stride = 0;
	struct vdec_dw_param_set *p = NULL;
	struct vdec_dw_param_set dw_param_set_pool[] = {
		/*x1, x2, x3, y1, y2, y3, m8t6, m16to32 */
		//{0, 0, 0, 0, 0, 0, 0, 0, 0, 1},	/* 1/1, 1/1 */
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 1},		/* 1/2, 1/1 */
		{1, 0, 0, 1, 0, 0, 0, 0, 0, 1},		/* 1/2, 1/2 */
		//{1, 0, 0, 1, 1, 0, 0, 0, 0, 1},	/* 1/4, 1/2 */
		{2, 0, 1, 1, 3, 0, 0, 1, 0, 1},		/* 1/4, 1/4 */
		//{1, 1, 1, 0, 1, 1, 1, 1, 0, 1},	/*> 1080p 1/8, 1/4 */
		{1, 1, 1, 1, 1, 1, 1, 1, 0, 1},		/*> 1080p 1/8, 1/8 */
	};

	if (IS_VDEC_DW(hw))
		p = &dw_param_set_pool[__ffs(IS_VDEC_DW(hw))];
	else
		return;

	WRITE_VREG(MDEC_DOUBLEW_CFG3,
		hw->buffer_spec[canvas_pos].vdec_dw_y_addr); // luma start address
	WRITE_VREG(MDEC_DOUBLEW_CFG4,
		hw->buffer_spec[canvas_pos].vdec_dw_u_addr); // chroma start address

	stride = ALIGN_WIDTH((hw->mb_width << 4) / (IS_VDEC_DW(hw)));
	if ((IS_VDEC_DW(hw)) == 1)	//width 1/2
		stride >>= 1;
	data32 = (stride << 16) | stride;
	WRITE_VREG(MDEC_DOUBLEW_CFG5, data32); // chroma stride | luma stride

	data32 = 0;
	p->dw_dma_blk_mode = hw->canvas_mode;
	data32 |= ((p->dw_x_shrink_1st << 0 ) |     // 1st down-scale horizontal, 00:no-scale 01:1/2avg 10:left 11:right
		(p->dw_y_shrink_1st << 2 ) |     // 1st down-scale vertical,   00:no-scale 01:1/2avg 10:up   11:down
		(p->dw_x_shrink_2nd << 4 ) |     // 2nd down-scale horizontal, 00:no-scale 01:1/2avg 10:left 11:right
		(p->dw_y_shrink_2nd << 6 ) |     // 2nd down-scale vertical,   00:no-scale 01:1/2avg 10:up   11:down
		(p->dw_x_shrink_3rd << 8 ) |     // 3rd down-scale horizontal, 00:no-scale 01:1/2avg 10:left 11:right
		(p->dw_y_shrink_3rd << 10) |     // 3rd down-scale vertical,   00:no-scale 01:1/2avg 10:up   11:down
		(p->dw_merge_8to16 << 12 ) |     //  8->16 horizontal block merge for better ddr efficiency
		(p->dw_merge_16to32 << 13) |     // 16->32 horizontal block merge for better ddr efficiency
		(p->dw_dma_blk_mode << 14) |     // DMA block mode, 0:linear 1:32x32 2:64x32
#ifdef FOR_LPDDR4_EFFICIENCY
		(1 << 19) |
#endif
		(p->dw_bwsave_mode << 22));      // Save line buffers to save band width
	WRITE_VREG(MDEC_DOUBLEW_CFG1, data32); // add some special tests here

	data32 = 0;
	data32 |= (1 << 0) | (0 << 27);
	WRITE_VREG(MDEC_DOUBLEW_CFG0, data32); // Double Write Enable | source from dblk

	dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
		"vdec_double_write mode %d\n",
		IS_VDEC_DW(hw));
	dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
		"param {%d, %d, %d, %d, %d, %d, %d, %d, %d}\n",
		p->dw_x_shrink_1st,
		p->dw_y_shrink_1st,
		p->dw_x_shrink_2nd,
		p->dw_y_shrink_2nd,
		p->dw_x_shrink_3rd,
		p->dw_y_shrink_3rd,
		p->dw_merge_8to16,
		p->dw_merge_16to32,
		p->dw_dma_blk_mode);
	dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
		"cfg0,1,3,4,5 = {%x, %x, %x, %x, %x}\n",
		READ_VREG(MDEC_DOUBLEW_CFG0),
		READ_VREG(MDEC_DOUBLEW_CFG1),
		READ_VREG(MDEC_DOUBLEW_CFG3),
		READ_VREG(MDEC_DOUBLEW_CFG4),
		READ_VREG(MDEC_DOUBLEW_CFG5));
}
#endif

static void config_decode_mode(struct vdec_h264_hw_s *hw)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct vdec_s *vdec = hw_to_vdec(hw);
#endif
	if (input_frame_based(hw_to_vdec(hw)))
		WRITE_VREG(H264_DECODE_MODE,
			DECODE_MODE_MULTI_FRAMEBASE);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	else if (vdec->slave)
		WRITE_VREG(H264_DECODE_MODE,
			(hw->got_valid_nal << 8) |
			DECODE_MODE_MULTI_DVBAL);
	else if (vdec->master)
		WRITE_VREG(H264_DECODE_MODE,
			(hw->got_valid_nal << 8) |
			DECODE_MODE_MULTI_DVENL);
#endif
	else
		WRITE_VREG(H264_DECODE_MODE,
			DECODE_MODE_MULTI_STREAMBASE);
	WRITE_VREG(H264_DECODE_SEQINFO,
		hw->seq_info2);
	WRITE_VREG(HEAD_PADING_REG, 0);

	if (hw->init_flag == 0)
		WRITE_VREG(INIT_FLAG_REG, 0);
	else
		WRITE_VREG(INIT_FLAG_REG, 1);
}

int config_decode_buf(struct vdec_h264_hw_s *hw, struct StorablePicture *pic)
{
	/* static int count = 0; */
	int ret = 0;
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	struct Slice *pSlice = &(p_H264_Dpb->mSlice);
	unsigned int colocate_adr_offset;
	unsigned int val;
	struct StorablePicture *last_pic = hw->last_dec_picture;

#ifdef ONE_COLOCATE_BUF_PER_DECODE_BUF
	int colocate_buf_index;
#endif
#define H264_BUFFER_INFO_INDEX    PMV3_X /* 0xc24 */
#define H264_BUFFER_INFO_DATA   PMV2_X  /* 0xc22 */
#define H264_CURRENT_POC_IDX_RESET LAST_SLICE_MV_ADDR /* 0xc30 */
#define H264_CURRENT_POC          LAST_MVY /* 0xc32 shared with conceal MV */

#define H264_CO_MB_WR_ADDR        VLD_C38 /* 0xc38 */
/* bit 31:30 -- L1[0] picture coding structure,
 *	00 - top field,	01 - bottom field,
 *	10 - frame, 11 - mbaff frame
 *   bit 29 - L1[0] top/bot for B field pciture , 0 - top, 1 - bot
 *   bit 28:0 h264_co_mb_mem_rd_addr[31:3]
 *	-- only used for B Picture Direct mode [2:0] will set to 3'b000
 */
#define H264_CO_MB_RD_ADDR        VLD_C39 /* 0xc39 */

/* bit 15 -- flush co_mb_data to DDR -- W-Only
 *   bit 14 -- h264_co_mb_mem_wr_addr write Enable -- W-Only
 *   bit 13 -- h264_co_mb_info_wr_ptr write Enable -- W-Only
 *   bit 9 -- soft_reset -- W-Only
 *   bit 8 -- upgent
 *   bit 7:2 -- h264_co_mb_mem_wr_addr
 *   bit 1:0 -- h264_co_mb_info_wr_ptr
 */
#define H264_CO_MB_RW_CTL         VLD_C3D /* 0xc3d */
#define DCAC_DDR_BYTE64_CTL                   0x0e1d
	unsigned long canvas_adr;
	unsigned int ref_reg_val;
	unsigned int one_ref_cfg = 0;
	int h264_buffer_info_data_write_count;
	int i, j;
	unsigned int colocate_wr_adr;
	unsigned int colocate_rd_adr;
	unsigned char use_direct_8x8;
	int canvas_pos;
	canvas_pos = hw->buffer_spec[pic->buf_spec_num].canvas_pos;
	WRITE_VREG(H264_CURRENT_POC_IDX_RESET, 0);
	WRITE_VREG(H264_CURRENT_POC, pic->frame_poc);
	WRITE_VREG(H264_CURRENT_POC, pic->top_poc);
	WRITE_VREG(H264_CURRENT_POC, pic->bottom_poc);

	dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
		"%s: pic_num is %d, poc is %d (%d, %d, %d), buf_spec_num %d canvas_pos %d\n",
		__func__, pic->pic_num, pic->poc, pic->frame_poc,
		pic->top_poc, pic->bottom_poc, pic->buf_spec_num,
		canvas_pos);
	print_pic_info(DECODE_ID(hw), "cur", pic, pSlice->slice_type);

#ifdef VDEC_DW
	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_T7) {
		if (IS_VDEC_DW(hw) && pic->mb_aff_frame_flag)
			WRITE_VREG(MDEC_DOUBLEW_CFG0,
				(READ_VREG(MDEC_DOUBLEW_CFG0) & (~(1 << 30))));
	}
#endif
	WRITE_VREG(CURR_CANVAS_CTRL, canvas_pos << 24);
	canvas_adr = READ_VREG(CURR_CANVAS_CTRL) & 0xffffff;

	if (!hw->mmu_enable) {
		WRITE_VREG(REC_CANVAS_ADDR, canvas_adr);
		WRITE_VREG(DBKR_CANVAS_ADDR, canvas_adr);
		WRITE_VREG(DBKW_CANVAS_ADDR, canvas_adr);
#ifdef VDEC_DW
		if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_T7) {
			WRITE_VREG(MDEC_DOUBLEW_CFG1,
				(hw->buffer_spec[canvas_pos].vdec_dw_y_canvas_index |
				(hw->buffer_spec[canvas_pos].vdec_dw_u_canvas_index << 8)));
		} else {
				h264_vdec_dw_cfg(hw, canvas_pos);
		}
#endif
	} else
		hevc_sao_set_pic_buffer(hw, pic);

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

	for (j = 0; j < hw->dpb.mDPB.size; j++) {
		int long_term_flag;
		i = get_buf_spec_by_canvas_pos(hw, j);
		if (i < 0)
			break;
		long_term_flag =
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
	if (hw->mmu_enable) {
		hevc_mcr_config_mc_ref(hw);
		hevc_mcr_config_mcrcc(hw);
	}

	dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
		"list0 size %d\n", pSlice->listXsize[0]);
	WRITE_VREG(H264_BUFFER_INFO_INDEX, 0);
	ref_reg_val = 0;
	j = 0;
	h264_buffer_info_data_write_count = 0;

	//disable this read cache when frame width <= 64 (4MBs)
	//IQIDCT_CONTROL, bit[16] dcac_dma_read_cache_disable
	if (hw->frame_width <= 64) {
		SET_VREG_MASK(IQIDCT_CONTROL,(1 << 16));
		if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A))
			// Disable DDR_BYTE64_CACHE
			WRITE_VREG(DCAC_DDR_BYTE64_CTL,
			(READ_VREG(DCAC_DDR_BYTE64_CTL) & (~0xf)) | 0xa);
	}
	else
		CLEAR_VREG_MASK(IQIDCT_CONTROL,(1 << 16));

	if (last_pic)
		dpb_print(DECODE_ID(hw), PRINT_FLAG_ERRORFLAG_DBG,
				"last_pic->data_flag %x   slice_type %x last_pic->slice_type %x\n",
				last_pic->data_flag, pSlice->slice_type, last_pic->slice_type);
	if (!hw->i_only && !(hw->error_proc_policy & 0x2000) &&
		last_pic && (last_pic->data_flag & ERROR_FLAG)
		&& (!(last_pic->slice_type == B_SLICE))
		&& (!(pSlice->slice_type == I_SLICE))) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_ERRORFLAG_DBG,
				  "no i/idr error mark\n");
		hw->data_flag |= ERROR_FLAG;
		pic->data_flag |= ERROR_FLAG;
	}

	for (i = 0; i < (unsigned int)(pSlice->listXsize[0]); i++) {
		/*ref list 0 */
		struct StorablePicture *ref = pSlice->listX[0][i];
		unsigned int cfg;
		/* bit[6:5] - frame/field info,
		 * 01 - top, 10 - bottom, 11 - frame
		 */
	#ifdef ERROR_CHECK
		if (ref == NULL) {
			hw->data_flag |= ERROR_FLAG;
			pic->data_flag  |= ERROR_FLAG;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERRORFLAG_DBG, " ref list0 NULL\n");
			return -1;
		}
		if ((ref->data_flag & ERROR_FLAG) && ref_frame_mark_flag[DECODE_ID(hw)]) {
			hw->data_flag |= ERROR_FLAG;
			pic->data_flag |= ERROR_FLAG;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERRORFLAG_DBG, " ref error mark1 \n");
		}

		if (hw->error_proc_policy & 0x80000) {
			if (ref_b_frame_error_max_count &&
				ref->slice_type == B_SLICE) {
				if (ref->data_flag & ERROR_FLAG)
					hw->b_frame_error_count++;
				else
					hw->b_frame_error_count = 0;
				if (hw->b_frame_error_count > ref_b_frame_error_max_count) {
					hw->b_frame_error_count = 0;
					dpb_print(DECODE_ID(hw), 0,
						"error %d B frame, reset dpb buffer\n",
						ref_b_frame_error_max_count);
					return -1;
				}
			}
		}

		if (ref->data_flag & NULL_FLAG)
			hw->data_flag |= NULL_FLAG;
#endif
		canvas_pos = hw->buffer_spec[ref->buf_spec_num].canvas_pos;

		if (ref->structure == TOP_FIELD)
			cfg = 0x1;
		else if (ref->structure == BOTTOM_FIELD)
			cfg = 0x2;
		else /* FRAME */
			cfg = 0x3;

		one_ref_cfg = (canvas_pos & 0x1f) | (cfg << 5);
		ref_reg_val <<= 8;
		ref_reg_val |= one_ref_cfg;
		j++;

		if (j == 4) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
				"H264_BUFFER_INFO_DATA: %x\n", ref_reg_val);
			WRITE_VREG(H264_BUFFER_INFO_DATA, ref_reg_val);
			h264_buffer_info_data_write_count++;
			j = 0;
		}
		print_pic_info(DECODE_ID(hw), "list0",
			pSlice->listX[0][i], -1);
	}
	if (j != 0) {
		while (j != 4) {
			ref_reg_val <<= 8;
			ref_reg_val |= one_ref_cfg;
			j++;
		}
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"H264_BUFFER_INFO_DATA: %x\n",
					ref_reg_val);
		WRITE_VREG(H264_BUFFER_INFO_DATA, ref_reg_val);
		h264_buffer_info_data_write_count++;
	}
	ref_reg_val = (one_ref_cfg << 24) | (one_ref_cfg<<16) |
				(one_ref_cfg << 8) | one_ref_cfg;
	for (i = h264_buffer_info_data_write_count; i < 8; i++)
		WRITE_VREG(H264_BUFFER_INFO_DATA, ref_reg_val);

	dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
		"list1 size %d\n", pSlice->listXsize[1]);
	WRITE_VREG(H264_BUFFER_INFO_INDEX, 8);
	ref_reg_val = 0;
	j = 0;

	for (i = 0; i < (unsigned int)(pSlice->listXsize[1]); i++) {
		/* ref list 0 */
		struct StorablePicture *ref = pSlice->listX[1][i];
		unsigned int cfg;
		/* bit[6:5] - frame/field info,
		 * 01 - top, 10 - bottom, 11 - frame
		 */

	#ifdef ERROR_CHECK
		if (ref == NULL) {
			hw->data_flag |= ERROR_FLAG;
			pic->data_flag  |= ERROR_FLAG;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERRORFLAG_DBG, " ref error list1 NULL\n");
			return -2;
		}
		if ((ref->data_flag & ERROR_FLAG) && (ref_frame_mark_flag[DECODE_ID(hw)])) {
			pic->data_flag  |= ERROR_FLAG;
			hw->data_flag |= ERROR_FLAG;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERRORFLAG_DBG, " ref error mark2\n");
		}
		if (ref->data_flag & NULL_FLAG)
			hw->data_flag |= NULL_FLAG;
#endif
		canvas_pos = hw->buffer_spec[ref->buf_spec_num].canvas_pos;
		if (ref->structure == TOP_FIELD)
			cfg = 0x1;
		else if (ref->structure == BOTTOM_FIELD)
			cfg = 0x2;
		else /* FRAME */
			cfg = 0x3;
		one_ref_cfg = (canvas_pos & 0x1f) | (cfg << 5);
		ref_reg_val <<= 8;
		ref_reg_val |= one_ref_cfg;
		j++;

		if (j == 4) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
				"H264_BUFFER_INFO_DATA: %x\n",
				ref_reg_val);
			WRITE_VREG(H264_BUFFER_INFO_DATA, ref_reg_val);
			j = 0;
		}
		print_pic_info(DECODE_ID(hw), "list1",
			pSlice->listX[1][i], -1);
	}
	if (j != 0) {
		while (j != 4) {
			ref_reg_val <<= 8;
			ref_reg_val |= one_ref_cfg;
			j++;
		}
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
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

	dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
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
			p_H264_Dpb->colocated_mv_addr_end) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
				"Error, colocate buf is not enough, index is %d\n",
			pic->colocated_buf_index);
			ret = -3;
		}
		val = colocate_wr_adr + colocate_adr_offset;
		WRITE_VREG(H264_CO_MB_WR_ADDR, val);
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"WRITE_VREG(H264_CO_MB_WR_ADDR) = %x, first_mb_in_slice %x pic_structure %x  colocate_adr_offset %x mode_8x8_flags %x colocated_buf_size %x\n",
			val, pSlice->first_mb_in_slice, pic->structure,
			colocate_adr_offset, pSlice->mode_8x8_flags,
			p_H264_Dpb->colocated_buf_size);
	} else {
		WRITE_VREG(H264_CO_MB_WR_ADDR, 0xffffffff);
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"WRITE_VREG(H264_CO_MB_WR_ADDR) = 0xffffffff\n");
	}
#else
	colocate_buf_index = hw->buffer_spec[pic->buf_spec_num].canvas_pos;
	colocate_adr_offset =
	((pic->structure == FRAME && pic->mb_aff_frame_flag == 0) ? 1 : 2) * 96;
	if (use_direct_8x8)
		colocate_adr_offset >>= 2;

	dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
		"colocate buf size of each mb 0x%x first_mb_in_slice 0x%x colocate_adr_offset 0x%x\r\n",
		colocate_adr_offset, pSlice->first_mb_in_slice,
		colocate_adr_offset * pSlice->first_mb_in_slice);

	colocate_adr_offset *= pSlice->first_mb_in_slice;

	colocate_wr_adr = p_H264_Dpb->colocated_mv_addr_start +
		((p_H264_Dpb->colocated_buf_size * colocate_buf_index) >>
			(use_direct_8x8 ? 2 : 0));

	if ((colocate_wr_adr + p_H264_Dpb->colocated_buf_size) >
		p_H264_Dpb->colocated_mv_addr_end) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
		"Error, colocate buf is not enough, col buf index is %d\n",
				colocate_buf_index);
		ret = -4;
	}
	val = colocate_wr_adr + colocate_adr_offset;
	WRITE_VREG(H264_CO_MB_WR_ADDR, val);
	dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
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
		int l10_structure, cur_structure;
		int cur_colocate_ref_type;
		/* H264_CO_MB_RD_ADDR[bit 29], top/bot for B field pciture,
		 * 0 - top, 1 - bot
		 */
		unsigned int val;
		unsigned int colocate_rd_adr_offset;
		unsigned int mby_mbx;
		unsigned int mby, mbx;

#ifdef ERROR_CHECK
		if (colocate_pic == NULL) {
			hw->data_flag |= ERROR_FLAG;
			pic->data_flag |= ERROR_FLAG;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERRORFLAG_DBG, " colocate error pic NULL\n");
			return -5;
		}
		if (colocate_pic->data_flag & ERROR_FLAG) {
			pic->data_flag |= ERROR_FLAG;
			hw->data_flag |= ERROR_FLAG;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERRORFLAG_DBG, " colocare ref error mark\n");
			}
		if (colocate_pic->data_flag & NULL_FLAG)
			hw->data_flag |= NULL_FLAG;
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

		//ALLEGRO_FIX, ported from single mode ucode
		mby_mbx = READ_VREG(MBY_MBX);
		mby = pSlice->first_mb_in_slice / hw->mb_width;
		mbx = pSlice->first_mb_in_slice % hw->mb_width;
		if (pic->mb_aff_frame_flag)
			cur_structure = 3;
		else {
			if (pic->coded_frame)
				cur_structure = 2;
			else
				cur_structure =	(pic->structure ==
					BOTTOM_FIELD) ?	1 : 0;
		}
		if (cur_structure < 2) {
			//current_field_structure
			if (l10_structure != 2) {
				colocate_rd_adr_offset = pSlice->first_mb_in_slice * 2;
			} else {
				// field_ref_from_frame co_mv_rd_addr :
				// mby*2*mb_width + mbx
				colocate_rd_adr_offset = mby * 2 * hw->mb_width + mbx;
			}

		} else {
			//current_frame_structure
			if (l10_structure < 2) {
				//calculate_co_mv_offset_frame_ref_field:
				// frame_ref_from_field co_mv_rd_addr :
				// (mby/2*mb_width+mbx)*2
				colocate_rd_adr_offset = ((mby / 2) * hw->mb_width + mbx) * 2;
			} else if (cur_structure == 2) {
				colocate_rd_adr_offset = pSlice->first_mb_in_slice;
			} else {
				//mbaff frame case1196
				colocate_rd_adr_offset = pSlice->first_mb_in_slice * 2;
			}

		}

		colocate_rd_adr_offset *= 96;
		if (use_direct_8x8)
			colocate_rd_adr_offset >>= 2;

		if (colocate_old_cal)
			colocate_rd_adr_offset = colocate_adr_offset;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"first_mb_in_slice 0x%x 0x%x 0x%x (MBY_MBX reg 0x%x) use_direct_8x8 %d cur %d (mb_aff_frame_flag %d, coded_frame %d structure %d) col %d (mb_aff_frame_flag %d, coded_frame %d structure %d) offset 0x%x rdoffset 0x%x\n",
				pSlice->first_mb_in_slice, mby, mbx, mby_mbx, use_direct_8x8,
				cur_structure, pic->mb_aff_frame_flag, pic->coded_frame, pic->structure,
				l10_structure, colocate_pic->mb_aff_frame_flag, colocate_pic->coded_frame, colocate_pic->structure,
				colocate_adr_offset,
				colocate_rd_adr_offset);

#if 0
		/*case0016, p16,
		 *cur_colocate_ref_type should be configured base on current pic
		 */
		if (pic->structure == FRAME &&
			pic->mb_aff_frame_flag)
			cur_colocate_ref_type = 0;
		else if (pic->structure == BOTTOM_FIELD)
			cur_colocate_ref_type = 1;
		else
			cur_colocate_ref_type = 0;
#else
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			" CUR TMP DEBUG : mb_aff_frame_flag : %d, structure : %d coded_frame %d\n",
			pic->mb_aff_frame_flag,
			pic->structure,
			pic->coded_frame);
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
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
				p_H264_Dpb->colocated_mv_addr_end) {
				dpb_print(DECODE_ID(hw),
					PRINT_FLAG_ERROR,
				"Error, colocate buf is not enough, index is %d\n",
					colocate_pic->colocated_buf_index);
				ret = -6;
			}
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
			val = ((colocate_rd_adr+colocate_rd_adr_offset) >> 3) |
				(l10_structure << 30) |
				(cur_colocate_ref_type << 29);
			WRITE_VREG(H264_CO_MB_RD_ADDR, val);
			dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
				"co idx %d, WRITE_VREG(H264_CO_MB_RD_ADDR) = %x, addr %x L1(0) pic_structure %d mbaff %d\n",
				colocate_pic->colocated_buf_index,
				val, colocate_rd_adr + colocate_rd_adr_offset,
				colocate_pic->structure,
				colocate_pic->mb_aff_frame_flag);
		} else {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"Error, reference pic has no colocated buf\n");
			ret = -7;
		}
#else
		colocate_buf_index =
			hw->buffer_spec[colocate_pic->buf_spec_num].canvas_pos;
		colocate_rd_adr = p_H264_Dpb->colocated_mv_addr_start +
			((p_H264_Dpb->colocated_buf_size *
				colocate_buf_index)
				>> (use_direct_8x8 ? 2 : 0));
		if ((colocate_rd_adr + p_H264_Dpb->colocated_buf_size) >
			p_H264_Dpb->colocated_mv_addr_end) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
				"Error, colocate buf is not enough, col buf index is %d\n",
				colocate_buf_index);
			ret = -8;
		}
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
		val = ((colocate_rd_adr+colocate_rd_adr_offset)>>3) |
			(l10_structure << 30) | (cur_colocate_ref_type << 29);
		WRITE_VREG(H264_CO_MB_RD_ADDR, val);
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"WRITE_VREG(H264_CO_MB_RD_ADDR) = %x, L1(0) pic_structure %d mbaff %d\n",
			val, colocate_pic->structure,
			colocate_pic->mb_aff_frame_flag);
#endif
	}
	return ret;
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
	struct vframe_s *vf[2] = {0, 0};
	struct vdec_s *vdec = op_arg;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;

	if (!hw)
		return NULL;

	if (force_disp_bufspec_num & 0x100) {
		if (force_disp_bufspec_num & 0x200)
			return NULL;
		return &hw->vframe_dummy;
	}

	if (kfifo_len(&hw->display_q) > VF_POOL_SIZE) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"kfifo len:%d invaild, peek error\n",
			kfifo_len(&hw->display_q));
		return NULL;
	}

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

static struct vframe_s *vh264_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;
	ulong nv_order = VIDTYPE_VIU_NV21;

	if (!hw)
		return NULL;

	/* swap uv */
	if (hw->is_used_v4l) {
		if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12) ||
			(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12M))
			nv_order = VIDTYPE_VIU_NV12;
	}

	if (force_disp_bufspec_num & 0x100) {
		int buffer_index = force_disp_bufspec_num & 0xff;
		if (force_disp_bufspec_num & 0x200)
			return NULL;

		vf = &hw->vframe_dummy;
		vf->duration_pulldown = 0;
		vf->pts = 0;
		vf->pts_us64 = 0;
		set_frame_info(hw, vf, buffer_index);
		vf->flag = 0;
		if (hw->mmu_enable) {
			if (hw->double_write_mode & 0x10) {
				/* double write only */
				vf->compBodyAddr = 0;
				vf->compHeadAddr = 0;
			} else {
				/*head adr*/
				vf->compHeadAddr =
				hw->buffer_spec[buffer_index].alloc_header_addr;
				/*body adr*/
				vf->compBodyAddr = 0;
				vf->canvas0Addr = vf->canvas1Addr = 0;
			}

			vf->type = VIDTYPE_SCATTER;

			if (hw->double_write_mode) {
				vf->type |= VIDTYPE_PROGRESSIVE
					| VIDTYPE_VIU_FIELD;
				vf->type |= nv_order;
				if (hw->double_write_mode == 3)
					vf->type |= VIDTYPE_COMPRESS;

				vf->canvas0Addr = vf->canvas1Addr = -1;
				vf->plane_num = 2;
				vf->canvas0_config[0] =
					hw->buffer_spec[buffer_index].
						canvas_config[0];
				vf->canvas0_config[1] =
					hw->buffer_spec[buffer_index].
						canvas_config[1];

				vf->canvas1_config[0] =
					hw->buffer_spec[buffer_index].
						canvas_config[0];
				vf->canvas1_config[1] =
					hw->buffer_spec[buffer_index].
						canvas_config[1];
			} else {
				vf->type |=
					VIDTYPE_COMPRESS | VIDTYPE_VIU_FIELD;
				vf->canvas0Addr = vf->canvas1Addr = 0;
			}
			vf->bitdepth =
				BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;

			vf->compWidth = hw->frame_width;
			vf->compHeight = hw->frame_height;

			if (hw->double_write_mode) {
				vf->width = hw->frame_width /
					get_double_write_ratio(hw->double_write_mode);
				vf->height = hw->frame_height /
					get_double_write_ratio(hw->double_write_mode);
			}
		} else {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				nv_order;
			vf->canvas0Addr = vf->canvas1Addr =
			spec2canvas(&hw->buffer_spec[buffer_index]);
		}

		/*vf->mem_handle = decoder_bmmu_box_get_mem_handle(
			hw->bmmu_box, buffer_index);*/
		update_vf_memhandle(hw, vf, buffer_index);
		force_disp_bufspec_num |= 0x200;
		return vf;
	}

	if (kfifo_get(&hw->display_q, &vf)) {
		int time = jiffies;
		unsigned int frame_interval =
			1000*(time - hw->last_frame_time)/HZ;
		struct vframe_s *next_vf = NULL;
		ATRACE_COUNTER(hw->trace.disp_q_name, kfifo_len(&hw->display_q));

		if (hw->discard_dv_data || (vdec_stream_based(vdec) && (vf->type & VIDTYPE_INTERLACE))) {
			vf->discard_dv_data = true;
		}
		if (dpb_is_debug(DECODE_ID(hw),
			PRINT_FLAG_VDEC_DETAIL)) {
			struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
			int frame_index = FRAME_INDEX(vf->index);
			if (frame_index < 0 ||
					frame_index >= DPB_SIZE_MAX) {
				dpb_print(DECODE_ID(hw), 0,
						"%s vf index 0x%x error\r\n",
						__func__, vf->index);
			} else {
				dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_DETAIL,
				"%s buf_spec_num %d vf %p poc %d dur %d pts %d interval %dms, ts:%lld type:%x discard_dv:%d\n",
				__func__, BUFSPEC_INDEX(vf->index), vf,
				p_H264_Dpb->mFrameStore[frame_index].poc,
				vf->duration, vf->pts, frame_interval, vf->timestamp, vf->type, vf->discard_dv_data);
			}
		}
		if (hw->last_frame_time > 0) {
			if (frame_interval >
				max_get_frame_interval[DECODE_ID(hw)])
				max_get_frame_interval[DECODE_ID(hw)]
				= frame_interval;
		}
		hw->last_frame_time = time;
		vf->index_disp = atomic_read(&hw->vf_get_count);
		vf->omx_index = atomic_read(&hw->vf_get_count);
		atomic_add(1, &hw->vf_get_count);
		if (kfifo_peek(&hw->display_q, &next_vf) && next_vf) {
			vf->next_vf_pts_valid = true;
			vf->next_vf_pts = next_vf->pts;
		} else
			vf->next_vf_pts_valid = false;

		return vf;
	}

	return NULL;
}

static bool vf_valid_check(struct vframe_s *vf, struct vdec_h264_hw_s *hw) {
	int i,j;
	if (hw->is_used_v4l)
		return true;
	for (i = 0; i < VF_POOL_SIZE; i++) {
		for (j = 0; j < VF_POOL_NUM; j ++) {
			if (vf == &(hw->vfpool[j][i]) || vf == &hw->vframe_dummy)
				return true;
		}
	}
	dpb_print(DECODE_ID(hw), 0, " invalid vf been put, vf = %p\n", vf);
	for (i = 0; i < VF_POOL_SIZE; i++) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
		"dump vf [%d]= %p\n",  i, &(hw->vfpool[hw->cur_pool][i]));
	}
	return false;
}

static void vh264_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	unsigned long flags;
	int buf_spec_num;
	int frame_index;

	if (vf == (&hw->vframe_dummy))
		return;

	if (!vf)
		return;

	if (vf->index == -1) {
		dpb_print(DECODE_ID(hw), 0,
			"Warning: %s vf %p invalid index\r\n",
			__func__, vf);
		return;
	}

	if (hw->enable_fence && vf->fence) {
		int ret, i;

		mutex_lock(&hw->fence_mutex);
		ret = dma_fence_get_status(vf->fence);
		if (ret == 0) {
			for (i = 0; i < VF_POOL_SIZE; i++) {
				if (hw->fence_vf_s.fence_vf[i] == NULL) {
					hw->fence_vf_s.fence_vf[i] = vf;
					hw->fence_vf_s.used_size++;
					mutex_unlock(&hw->fence_mutex);
					return;
				}
			}
		}
		mutex_unlock(&hw->fence_mutex);
	}

	buf_spec_num = BUFSPEC_INDEX(vf->index);
	if (hw->enable_fence)
		frame_index = hw->buffer_spec[buf_spec_num].fs_idx;
	else
		frame_index = FRAME_INDEX(vf->index);

	if (frame_index < 0 ||
		frame_index >= DPB_SIZE_MAX ||
		buf_spec_num < 0 ||
		buf_spec_num >= BUFSPEC_POOL_SIZE) {
		dpb_print(DECODE_ID(hw), 0,
			"%s vf index 0x%x error\r\n",
			__func__, vf->index);
		return;
	}
		/*get_buf_spec_idx_by_canvas_config(hw,
			&vf->canvas0_config[0]);*/

	if (hw->enable_fence && vf->fence) {
		vdec_fence_put(vf->fence);
		vf->fence = NULL;
	}

	spin_lock_irqsave(&hw->bufspec_lock, flags);
	if (hw->buffer_spec[buf_spec_num].used == 2) {
		struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
		"%s %p to fs[%d], poc %d buf_spec_num %d used %d vf_ref %d\n",
		__func__, vf, frame_index,
		p_H264_Dpb->mFrameStore[frame_index].poc,
		buf_spec_num,
		hw->buffer_spec[buf_spec_num].used,
		hw->buffer_spec[buf_spec_num].vf_ref);
		hw->buffer_spec[buf_spec_num].vf_ref--;
		if (hw->buffer_spec[buf_spec_num].vf_ref <= 0)
			set_frame_output_flag(&hw->dpb, frame_index);
	} else {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
		"%s %p isolated vf, buf_spec_num %d used %d vf_ref %d\n",
		__func__, vf, buf_spec_num,
		hw->buffer_spec[buf_spec_num].used,
		hw->buffer_spec[buf_spec_num].vf_ref);
		hw->buffer_spec[buf_spec_num].vf_ref--;
		if (hw->buffer_spec[buf_spec_num].vf_ref <= 0) {
			if (hw->buffer_spec[buf_spec_num].used == 3)
				hw->buffer_spec[buf_spec_num].used = 4;
			else if (hw->buffer_spec[buf_spec_num].used == 5)
				hw->buffer_spec[buf_spec_num].used = 0;
		}
		if (dpb_is_debug(DECODE_ID(hw),
			PRINT_FLAG_DUMP_BUFSPEC))
			dump_bufspec(hw, __func__);

	}

	if (hw->is_used_v4l) {
		struct buffer_spec_s *pic = &hw->buffer_spec[buf_spec_num];

		if (vf->v4l_mem_handle != pic->cma_alloc_addr)
			pic->cma_alloc_addr = vf->v4l_mem_handle;
	}

	atomic_add(1, &hw->vf_put_count);
	if (vf && (vf_valid_check(vf, hw) == true)) {
		kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
		ATRACE_COUNTER(hw->trace.new_q_name, kfifo_len(&hw->newframe_q));
	}

#define ASSIST_MBOX1_IRQ_REG    VDEC_ASSIST_MBOX1_IRQ_REG
	if (hw->buffer_empty_flag)
		WRITE_VREG(ASSIST_MBOX1_IRQ_REG, 0x1);
	spin_unlock_irqrestore(&hw->bufspec_lock, flags);
}

void * vh264_get_bufspec_lock(struct vdec_s *vdec)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	if (hw)
		return (&hw->bufspec_lock);
	else
		return NULL;
}
static int vh264_event_cb(int type, void *data, void *op_arg)
{
	unsigned long flags;
	struct vdec_s *vdec = op_arg;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;

	if (type & VFRAME_EVENT_RECEIVER_GET_AUX_DATA) {
		struct provider_aux_req_s *req =
			(struct provider_aux_req_s *)data;
		int buf_spec_num;

		if (!req->vf) {
			req->aux_size = atomic_read(&hw->vf_put_count);
			return 0;
		}

		if (req->vf->discard_dv_data) {
			req->aux_size = atomic_read(&hw->vf_put_count);
			return 0;
		}

		buf_spec_num = BUFSPEC_INDEX(req->vf->index);
		spin_lock_irqsave(&hw->lock, flags);
		req->aux_buf = NULL;
		req->aux_size = 0;
		if (buf_spec_num >= 0 &&
			buf_spec_num < BUFSPEC_POOL_SIZE &&
			is_buf_spec_in_disp_q(hw, buf_spec_num)
			) {
			req->aux_buf =
				hw->buffer_spec[buf_spec_num].aux_data_buf;
			req->aux_size =
				hw->buffer_spec[buf_spec_num].aux_data_size;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			req->dv_enhance_exist =
				hw->buffer_spec[buf_spec_num].dv_enhance_exist;
#else
			req->dv_enhance_exist = 0;
#endif
		}
		spin_unlock_irqrestore(&hw->lock, flags);

		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
		"%s(type 0x%x vf %p discard_dv_data %d buf_spec_num 0x%x)=>buf %px size 0x%x\n",
		__func__, type, req->vf, req->vf->discard_dv_data, buf_spec_num, req->aux_size, req->aux_buf);
	} else if (type & VFRAME_EVENT_RECEIVER_REQ_STATE) {
		struct provider_state_req_s *req =
			(struct provider_state_req_s *)data;
		if (req->req_type == REQ_STATE_SECURE)
			req->req_result[0] = vdec_secure(vdec);
		else
			req->req_result[0] = 0xffffffff;
	}

	return 0;
}

static void set_frame_info(struct vdec_h264_hw_s *hw, struct vframe_s *vf,
				u32 index)
{
	struct canvas_config_s *p_canvas_config;
	int force_rate = input_frame_based(hw_to_vdec(hw)) ?
		force_rate_framebase : force_rate_streambase;
	dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
		"%s (%d,%d) dur %d, vf %p, index %d\n", __func__,
		hw->frame_width, hw->frame_height, hw->frame_dur, vf, index);

	/* signal_type */
	if (hw->video_signal_from_vui & VIDEO_SIGNAL_TYPE_AVAILABLE_MASK) {
		vf->signal_type = hw->video_signal_from_vui;
		if (hw->is_used_v4l) {
			struct aml_vdec_hdr_infos hdr;
			struct aml_vcodec_ctx *ctx =
				(struct aml_vcodec_ctx *)(hw->v4l2_ctx);

			memset(&hdr, 0, sizeof(hdr));
			hdr.signal_type = hw->video_signal_from_vui;
			vdec_v4l_set_hdr_infos(ctx, &hdr);
		}
	} else
		vf->signal_type = 0;
	hw->video_signal_type = vf->signal_type;

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

	vf->sidebind_type = hw->sidebind_type;
	vf->sidebind_channel_id = hw->sidebind_channel_id;

	if (hw->mmu_enable)
		return;

	vf->canvas0Addr = vf->canvas1Addr = -1;
#ifdef NV21
	vf->plane_num = 2;
#else
	vf->plane_num = 3;
#endif

	if (IS_VDEC_DW(hw)) {
		if (IS_VDEC_DW(hw) == 1)
			vf->width = hw->frame_width / 2;
		else
			vf->width = (hw->frame_width / IS_VDEC_DW(hw));
		vf->height = (hw->frame_height / IS_VDEC_DW(hw));
		p_canvas_config = &hw->buffer_spec[index].vdec_dw_canvas_config[0];
	} else
		p_canvas_config = &hw->buffer_spec[index].canvas_config[0];

	vf->canvas0_config[0] = p_canvas_config[0];
	vf->canvas0_config[1] = p_canvas_config[1];
#ifndef NV21
	vf->canvas0_config[2] = p_canvas_config[2];
#endif
	vf->canvas1_config[0] = p_canvas_config[0];
	vf->canvas1_config[1] = p_canvas_config[1];
#ifndef NV21
	vf->canvas1_config[2] = p_canvas_config[2];
#endif
}

static void get_picture_qos_info(struct StorablePicture *picture)
{
	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_G12A) {
		unsigned char a[3];
		unsigned char i, j, t;
		unsigned long  data;

		get_random_bytes(&data, sizeof(unsigned long));
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
		/*
		pr_info("mv data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data, a[0], a[1], a[2]);
		*/

		get_random_bytes(&data, sizeof(unsigned long));
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
		/*
		pr_info("qp data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data, a[0], a[1], a[2]);
		*/

		get_random_bytes(&data, sizeof(unsigned long));
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


		/*
		pr_info("skip data %x  a[0]= %x a[1]= %x a[2]= %x\n",
			data,a[0], a[1], a[2]);
		*/
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
/*
#define DEBUG_QOS
*/
#ifdef DEBUG_QOS
		int pic_number = picture->poc;
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





		/* set rd_idx to 0 */
	    WRITE_VREG(VDEC_PIC_QUALITY_CTRL, 0);
	    blk88_y_count = READ_VREG(VDEC_PIC_QUALITY_DATA);
	    if (blk88_y_count == 0) {
#ifdef DEBUG_QOS
			pr_info(" [Picture %d Quality] NO Data yet.\n",
				pic_number);
#endif
			/* reset all counts */
			WRITE_VREG(VDEC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* qp_y_sum */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] Y QP AVG : %d (%d/%d)\n",
			pic_number, rdata32/blk88_y_count,
			rdata32, blk88_y_count);
#endif
		picture->avg_qp = rdata32/blk88_y_count;
		/* intra_y_count */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] Y intra rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_y_count,
			'%', rdata32);
#endif
		/* skipped_y_count */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] Y skipped rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_y_count,
			'%', rdata32);
#endif
		picture->avg_skip = rdata32*100/blk88_y_count;
		/* coeff_non_zero_y_count */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] Y ZERO_Coeff rate : %d%c (%d)\n",
			pic_number, (100 - rdata32*100/(blk88_y_count*1)),
			'%', rdata32);
#endif
		/* blk66_c_count */
	    blk88_c_count = READ_VREG(VDEC_PIC_QUALITY_DATA);
	    if (blk88_c_count == 0) {
#ifdef DEBUG_QOS
			pr_info(" [Picture %d Quality] NO Data yet.\n",
				pic_number);
#endif
			/* reset all counts */
			WRITE_VREG(VDEC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* qp_c_sum */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] C QP AVG : %d (%d/%d)\n",
			pic_number, rdata32/blk88_c_count,
			rdata32, blk88_c_count);
#endif
		/* intra_c_count */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] C intra rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_c_count,
			'%', rdata32);
#endif
		/* skipped_cu_c_count */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] C skipped rate : %d%c (%d)\n",
			pic_number, rdata32*100/blk88_c_count,
			'%', rdata32);
#endif
		/* coeff_non_zero_c_count */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] C ZERO_Coeff rate : %d%c (%d)\n",
			pic_number, (100 - rdata32*100/(blk88_c_count*1)),
			'%', rdata32);
#endif

		/* 1'h0, qp_c_max[6:0], 1'h0, qp_c_min[6:0],
		1'h0, qp_y_max[6:0], 1'h0, qp_y_min[6:0] */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] Y QP min : %d\n",
			pic_number, (rdata32>>0)&0xff);
#endif
		picture->min_qp = (rdata32>>0)&0xff;

#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] Y QP max : %d\n",
			pic_number, (rdata32>>8)&0xff);
#endif
		picture->max_qp = (rdata32>>8)&0xff;

#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] C QP min : %d\n",
			pic_number, (rdata32>>16)&0xff);
	    pr_info(" [Picture %d Quality] C QP max : %d\n",
			pic_number, (rdata32>>24)&0xff);
#endif

		/* blk22_mv_count */
	    blk22_mv_count = READ_VREG(VDEC_PIC_QUALITY_DATA);
	    if (blk22_mv_count == 0) {
#ifdef DEBUG_QOS
			pr_info(" [Picture %d Quality] NO MV Data yet.\n",
				pic_number);
#endif
			/* reset all counts */
			WRITE_VREG(VDEC_PIC_QUALITY_CTRL, (1<<8));
			return;
	    }
		/* mvy_L1_count[39:32], mvx_L1_count[39:32],
		mvy_L0_count[39:32], mvx_L0_count[39:32] */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
	    /* should all be 0x00 or 0xff */
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] MV AVG High Bits: 0x%X\n",
			pic_number, rdata32);
#endif
	    mvx_L0_hi = ((rdata32>>0)&0xff);
	    mvy_L0_hi = ((rdata32>>8)&0xff);
	    mvx_L1_hi = ((rdata32>>16)&0xff);
	    mvy_L1_hi = ((rdata32>>24)&0xff);

		/* mvx_L0_count[31:0] */
	    rdata32_l = READ_VREG(VDEC_PIC_QUALITY_DATA);
		temp_value = mvx_L0_hi;
		temp_value = (temp_value << 32) | rdata32_l;

		if (mvx_L0_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
		value = div_s64(value, blk22_mv_count);
#ifdef DEBUG_QOS
		pr_info(" [Picture %d Quality] MVX_L0 AVG : %d (%lld/%d)\n",
			pic_number, (int)(value),
			value, blk22_mv_count);
#endif
		picture->avg_mv = value;

		/* mvy_L0_count[31:0] */
	    rdata32_l = READ_VREG(VDEC_PIC_QUALITY_DATA);
		temp_value = mvy_L0_hi;
		temp_value = (temp_value << 32) | rdata32_l;

		if (mvy_L0_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] MVY_L0 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);
#endif

		/* mvx_L1_count[31:0] */
	    rdata32_l = READ_VREG(VDEC_PIC_QUALITY_DATA);
		temp_value = mvx_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvx_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] MVX_L1 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);
#endif

		/* mvy_L1_count[31:0] */
	    rdata32_l = READ_VREG(VDEC_PIC_QUALITY_DATA);
		temp_value = mvy_L1_hi;
		temp_value = (temp_value << 32) | rdata32_l;
		if (mvy_L1_hi & 0x80)
			value = 0xFFFFFFF000000000 | temp_value;
		else
			value = temp_value;
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] MVY_L1 AVG : %d (%lld/%d)\n",
			pic_number, rdata32_l/blk22_mv_count,
			value, blk22_mv_count);
#endif

		/* {mvx_L0_max, mvx_L0_min} // format : {sign, abs[14:0]}  */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32>>16)&0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] MVX_L0 MAX : %d\n",
			pic_number, mv_hi);
#endif
		picture->max_mv = mv_hi;

	    mv_lo = (rdata32>>0)&0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] MVX_L0 MIN : %d\n",
			pic_number, mv_lo);
#endif
		picture->min_mv = mv_lo;

#ifdef DEBUG_QOS
		/* {mvy_L0_max, mvy_L0_min} */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32>>16)&0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;
	    pr_info(" [Picture %d Quality] MVY_L0 MAX : %d\n",
			pic_number, mv_hi);


	    mv_lo = (rdata32>>0)&0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;

	    pr_info(" [Picture %d Quality] MVY_L0 MIN : %d\n",
			pic_number, mv_lo);


		/* {mvx_L1_max, mvx_L1_min} */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32>>16)&0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;

	    pr_info(" [Picture %d Quality] MVX_L1 MAX : %d\n",
			pic_number, mv_hi);


	    mv_lo = (rdata32>>0)&0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;

	    pr_info(" [Picture %d Quality] MVX_L1 MIN : %d\n",
			pic_number, mv_lo);


		/* {mvy_L1_max, mvy_L1_min} */
	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_DATA);
	    mv_hi = (rdata32>>16)&0xffff;
	    if (mv_hi & 0x8000)
			mv_hi = 0x8000 - mv_hi;

	    pr_info(" [Picture %d Quality] MVY_L1 MAX : %d\n",
			pic_number, mv_hi);

	    mv_lo = (rdata32>>0)&0xffff;
	    if (mv_lo & 0x8000)
			mv_lo = 0x8000 - mv_lo;

	    pr_info(" [Picture %d Quality] MVY_L1 MIN : %d\n",
			pic_number, mv_lo);
#endif

	    rdata32 = READ_VREG(VDEC_PIC_QUALITY_CTRL);
#ifdef DEBUG_QOS
	    pr_info(" [Picture %d Quality] After Read : VDEC_PIC_QUALITY_CTRL : 0x%x\n",
			pic_number, rdata32);
#endif
		/* reset all counts */
	    WRITE_VREG(VDEC_PIC_QUALITY_CTRL, (1<<8));
	}
}

static int get_dec_dpb_size(struct vdec_h264_hw_s *hw, int mb_width,
		int mb_height, int level_idc)
{
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int pic_size = mb_width * mb_height * 384;
	int size = 0, size_vui;

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
	size = imin(size, 16);
	dpb_print(DECODE_ID(hw), 0,
				"level_idc = %d pic_size = %d size = %d\n",
				level_idc, pic_size, size);
	if (p_H264_Dpb->bitstream_restriction_flag) {
		if ((int)p_H264_Dpb->max_dec_frame_buffering > size) {
			dpb_print(DECODE_ID(hw), 0,
				"max_dec_frame_buffering larger than MaxDpbSize.\n");
		}
		size_vui = imax (1, p_H264_Dpb->max_dec_frame_buffering);
		if (size_vui < size) {
			dpb_print(DECODE_ID(hw), 0,
				"Warning: max_dec_frame_buffering(%d) is less than DPB size(%d) calculated from Profile/Level.\n",
				size_vui, size);
		}
		size = size_vui;
	}

	size += 2;	/* need two more buffer */

	return size;
}

static int get_dec_dpb_size_active(struct vdec_h264_hw_s *hw, u32 param1, u32 param4)
{
	int mb_width, mb_total;
	int mb_height = 0;
	int dec_dpb_size;
	int level_idc = param4 & 0xff;

	mb_width = param1 & 0xff;
	mb_total = (param1 >> 8) & 0xffff;
	if (!mb_width && mb_total) /*for 4k2k*/
		mb_width = 256;
	if (mb_width)
		mb_height = mb_total/mb_width;
	if (mb_width <= 0 || mb_height <= 0 ||
		is_oversize(mb_width << 4, mb_height << 4)) {
		dpb_print(DECODE_ID(hw), 0,
			"!!!wrong param1 0x%x mb_width/mb_height (0x%x/0x%x) %x\r\n",
			param1,
			mb_width,
			mb_height);
		hw->error_frame_width = mb_width << 4;
		hw->error_frame_height = mb_height << 4;
		return -1;
	}
	hw->error_frame_width = 0;
	hw->error_frame_height = 0;

	dec_dpb_size = get_dec_dpb_size(hw , mb_width, mb_height, level_idc);

	if (hw->no_poc_reorder_flag)
		dec_dpb_size = 1;

	return dec_dpb_size;
}

static void vh264_config_canvs_for_mmu(struct vdec_h264_hw_s *hw)
{
	int i, j;

	if (hw->double_write_mode) {
		mutex_lock(&vmh264_mutex);
		if (hw->decode_pic_count == 0) {
			for (j = 0; j < hw->dpb.mDPB.size; j++) {
				i = get_buf_spec_by_canvas_pos(hw, j);
				if (i >= 0)
					config_decode_canvas_ex(hw, i);
			}
		}
		mutex_unlock(&vmh264_mutex);
	}
}

static int vh264_set_params(struct vdec_h264_hw_s *hw,
	u32 param1, u32 param2, u32 param3, u32 param4, bool buffer_reset_flag)
{
	int i, j;
	int mb_width, mb_total;
	int max_reference_size, level_idc;
	int mb_height = 0;
	unsigned long flags;
	/*int mb_mv_byte;*/
	struct vdec_s *vdec = hw_to_vdec(hw);
	u32 seq_info2;
	u32 seq_info3;
	int ret = 0;
	int active_buffer_spec_num;
	unsigned int buf_size;
	unsigned int frame_mbs_only_flag;
	unsigned int chroma_format_idc;
	unsigned int crop_bottom, crop_right;
	unsigned int used_reorder_dpb_size_margin
		= hw->reorder_dpb_size_margin;
	u8 *colocate_vaddr = NULL;
	int dec_dpb_size_change = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (vdec->master || vdec->slave)
		used_reorder_dpb_size_margin =
			reorder_dpb_size_margin_dv;
#endif
	seq_info2 = param1;
	seq_info3 = param4;
	hw->seq_info = param2;

	mb_width = seq_info2 & 0xff;
	mb_total = (seq_info2 >> 8) & 0xffff;
	if (!mb_width && mb_total) /*for 4k2k*/
		mb_width = 256;
	if (mb_width)
		mb_height = mb_total/mb_width;
	if (mb_width <= 0 || mb_height <= 0 ||
		is_oversize(mb_width << 4, mb_height << 4)) {
		dpb_print(DECODE_ID(hw), 0,
			"!!!wrong seq_info2 0x%x mb_width/mb_height (0x%x/0x%x) %x\r\n",
			seq_info2,
			mb_width,
			mb_height);
			hw->error_frame_width = mb_width << 4;
			hw->error_frame_height = mb_height << 4;
		return -1;
	}
	hw->error_frame_width = 0;
	hw->error_frame_height = 0;

	dec_dpb_size_change = hw->dpb.dec_dpb_size != get_dec_dpb_size_active(hw, param1, param4);

	if (((seq_info2 != 0 &&
		hw->seq_info2 != seq_info2) || dec_dpb_size_change) &&
		hw->seq_info2 != 0
		) { /*picture size changed*/
		h264_reconfig(hw);
	} else {
		/*someting changes and not including dpb_size, width, height, ...*/
		struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
		u32 reg_val = param4;
		level_idc = reg_val & 0xff;
		p_H264_Dpb->mSPS.level_idc = level_idc;
		max_reference_size = (reg_val >> 8) & 0xff;
		hw->dpb.reorder_output = max_reference_size;

		hw->seq_info2 = seq_info2;
		hw->seq_info3 = seq_info3;

		if (p_H264_Dpb->bitstream_restriction_flag &&
			p_H264_Dpb->num_reorder_frames <= p_H264_Dpb->max_dec_frame_buffering &&
			p_H264_Dpb->num_reorder_frames >= 0) {
			hw->dpb.reorder_output = hw->num_reorder_frames + 1;
		}
	}

	if (hw->config_bufmgr_done == 0) {
		struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
		u32 reg_val;
		int sub_width_c = 0, sub_height_c = 0;

		hw->cfg_param1 = param1;
		hw->cfg_param2 = param2;
		hw->cfg_param3 = param3;
		hw->cfg_param4 = param4;

		hw->seq_info2 = seq_info2;
		hw->seq_info3 = seq_info3;
		dpb_print(DECODE_ID(hw), 0,
			"AV_SCRATCH_1 = %x, AV_SCRATCH_2 %x, AV_SCRATCH_B: = %x\r\n",
			seq_info2, hw->seq_info, seq_info3);

		dpb_init_global(&hw->dpb,
			DECODE_ID(hw), 0, 0);

		p_H264_Dpb->fast_output_enable = fast_output_enable;
		/*mb_mv_byte = (seq_info2 & 0x80000000) ? 24 : 96;*/
		if (hw->enable_fence)
			p_H264_Dpb->fast_output_enable = H264_OUTPUT_MODE_FAST;
#if 1

		p_H264_Dpb->mSPS.profile_idc =
					 (p_H264_Dpb->dpb_param.l.data[PROFILE_IDC_MMCO] >> 8) & 0xff;

		/*crop*/
		/* AV_SCRATCH_2
		   bit 15: frame_mbs_only_flag
		   bit 13-14: chroma_format_idc */
		frame_mbs_only_flag = (hw->seq_info >> 15) & 0x01;
		p_H264_Dpb->chroma_format_idc = (hw->seq_info >> 13) & 0x03;

		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
				"%s, %d chroma_format_idc %d\n",
				__func__, __LINE__, p_H264_Dpb->chroma_format_idc);

		if (p_H264_Dpb->mSPS.profile_idc != 100 &&
			p_H264_Dpb->mSPS.profile_idc != 110 &&
			p_H264_Dpb->mSPS.profile_idc != 122 &&
			p_H264_Dpb->mSPS.profile_idc != 144) {
			p_H264_Dpb->chroma_format_idc = 1;
		}
		chroma_format_idc = p_H264_Dpb->chroma_format_idc;

		/* @AV_SCRATCH_6.31-16 =  (left  << 8 | right ) << 1
		   @AV_SCRATCH_6.15-0   =  (top << 8  | bottom ) <<
		   (2 - frame_mbs_only_flag) */

		switch (chroma_format_idc) {
			case 1:
				sub_width_c = 2;
				sub_height_c = 2;
				break;

			case 2:
				sub_width_c = 2;
				sub_height_c = 1;
				break;

			case 3:
				sub_width_c = 1;
				sub_height_c = 1;
				break;

			default:
				break;
		}

		if (chroma_format_idc == 0) {
			crop_right = p_H264_Dpb->frame_crop_right_offset;
			crop_bottom = p_H264_Dpb->frame_crop_bottom_offset *
				(2 - frame_mbs_only_flag);
		} else {
			crop_right = sub_width_c * p_H264_Dpb->frame_crop_right_offset;
			crop_bottom = sub_height_c * p_H264_Dpb->frame_crop_bottom_offset *
				(2 - frame_mbs_only_flag);
		}

		p_H264_Dpb->mSPS.frame_mbs_only_flag = frame_mbs_only_flag;
		hw->frame_width = mb_width << 4;
		hw->frame_height = mb_height << 4;

		hw->frame_width = hw->frame_width - crop_right;
		hw->frame_height = hw->frame_height - crop_bottom;

		dpb_print(DECODE_ID(hw), 0,
			"chroma_format_idc = %d frame_mbs_only_flag %d, crop_bottom %d,  frame_height %d,\n",
			chroma_format_idc, frame_mbs_only_flag, crop_bottom, hw->frame_height);
		dpb_print(DECODE_ID(hw), 0,
			"mb_height %d,crop_right %d, frame_width %d, mb_width %d\n",
			mb_height, crop_right,
			hw->frame_width, mb_width);

		if (hw->frame_height == 1088 && (crop_right != 0 || crop_bottom != 0))
			hw->frame_height = 1080;
#endif
		reg_val = param4;
		level_idc = reg_val & 0xff;
		p_H264_Dpb->mSPS.level_idc = level_idc;
		max_reference_size = (reg_val >> 8) & 0xff;
		hw->dpb.reorder_output = max_reference_size;
		hw->dpb.dec_dpb_size =
			get_dec_dpb_size(hw , mb_width, mb_height, level_idc);
		if (!hw->mmu_enable) {
			mb_width = (mb_width+3) & 0xfffffffc;
			mb_height = (mb_height+3) & 0xfffffffc;
		}
		mb_total = mb_width * mb_height;
		hw->mb_width = mb_width;
		hw->mb_height = mb_height;
		hw->mb_total = mb_total;
		if (hw->mmu_enable)
			hevc_mcr_sao_global_hw_init(hw,
				(hw->mb_width << 4), (hw->mb_height << 4));

		dpb_print(DECODE_ID(hw), 0,
			"mb height/widht/total: %x/%x/%x level_idc %x max_ref_num %x\n",
			mb_height, mb_width, mb_total,
			level_idc, max_reference_size);

		p_H264_Dpb->colocated_buf_size = mb_total * 96;

		dpb_print(DECODE_ID(hw), 0,
			"restriction_flag=%d, max_dec_frame_buffering=%d, dec_dpb_size=%d num_reorder_frames %d used_reorder_dpb_size_margin %d\n",
			hw->bitstream_restriction_flag,
			hw->max_dec_frame_buffering,
			hw->dpb.dec_dpb_size,
			hw->num_reorder_frames,
			used_reorder_dpb_size_margin);

		if (p_H264_Dpb->bitstream_restriction_flag &&
			p_H264_Dpb->num_reorder_frames <= p_H264_Dpb->max_dec_frame_buffering &&
			p_H264_Dpb->num_reorder_frames >= 0) {
			hw->dpb.reorder_output = hw->num_reorder_frames + 1;
		}

		active_buffer_spec_num =
			hw->dpb.dec_dpb_size
			+ used_reorder_dpb_size_margin;
		hw->max_reference_size =
			max_reference_size + reference_buf_margin;

		if (active_buffer_spec_num > MAX_VF_BUF_NUM) {
			active_buffer_spec_num = MAX_VF_BUF_NUM;
			hw->dpb.dec_dpb_size = active_buffer_spec_num
				- used_reorder_dpb_size_margin;
			dpb_print(DECODE_ID(hw), 0,
				"active_buffer_spec_num is larger than MAX %d, set dec_dpb_size to %d\n",
				MAX_VF_BUF_NUM, hw->dpb.dec_dpb_size);
		}
		hw->dpb.mDPB.size = active_buffer_spec_num;
		if (hw->max_reference_size > MAX_VF_BUF_NUM)
			hw->max_reference_size = MAX_VF_BUF_NUM;
		hw->dpb.max_reference_size = hw->max_reference_size;

		if (hw->no_poc_reorder_flag)
			hw->dpb.dec_dpb_size = 1;
		dpb_print(DECODE_ID(hw), 0,
			"%s active_buf_spec_num %d dec_dpb_size %d collocate_buf_num %d\r\n",
			__func__, active_buffer_spec_num,
			hw->dpb.dec_dpb_size,
			hw->max_reference_size);

		if (hw->kpi_first_i_comming == 0) {
			hw->kpi_first_i_comming = 1;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_DETAIL,
				"[vdec_kpi][%s] First I frame comming.\n", __func__);
		}

		buf_size = (hw->mb_total << 8) + (hw->mb_total << 7);

		mutex_lock(&vmh264_mutex);
		if (!hw->mmu_enable) {
			if (!buffer_reset_flag || hw->is_used_v4l)
				config_buf_specs(vdec);
			i = get_buf_spec_by_canvas_pos(hw, 0);

			if (hw->is_used_v4l) {
				if (i != -1) {
					pr_info("v4l: delay alloc the buffer.\n");
				}
			} else {
				if ((i != -1) && alloc_one_buf_spec(hw, i) >= 0)
					config_decode_canvas(hw, i);
				else
					ret = -1;
			}
		} else {
			if (hw->double_write_mode) {
				config_buf_specs_ex(vdec);
			} else {
				spin_lock_irqsave(&hw->bufspec_lock, flags);
				for (i = 0, j = 0;
					j < active_buffer_spec_num
					&& i < BUFSPEC_POOL_SIZE;
					i++) {
					if (hw->buffer_spec[i].used != -1)
						continue;
					hw->buffer_spec[i].used = 0;
					hw->buffer_spec[i].
						alloc_header_addr = 0;
					hw->buffer_spec[i].canvas_pos = j;
					j++;
				}
				spin_unlock_irqrestore(&hw->bufspec_lock,
					flags);
			}
			hevc_mcr_config_canv2axitbl(hw, 0);
		}
		mutex_unlock(&vmh264_mutex);
		if (dpb_is_debug(DECODE_ID(hw),
			PRINT_FLAG_DUMP_BUFSPEC))
			dump_bufspec(hw, __func__);

#ifdef ONE_COLOCATE_BUF_PER_DECODE_BUF
		buf_size = PAGE_ALIGN(
			p_H264_Dpb->colocated_buf_size *
					active_buffer_spec_num);
#else
		buf_size = PAGE_ALIGN(
			p_H264_Dpb->colocated_buf_size *
					hw->max_reference_size);
#endif

		if (decoder_bmmu_box_alloc_buf_phy(hw->bmmu_box, BMMU_REF_IDX,
			buf_size, DRIVER_NAME,
			&hw->collocate_cma_alloc_addr) < 0)
			return -1;
		if (!vdec_secure(vdec)) {
			/* clear for some mosaic problem after reset bufmgr */
			colocate_vaddr = codec_mm_vmap(hw->collocate_cma_alloc_addr, buf_size);
			if (colocate_vaddr != NULL) {
				memset(colocate_vaddr, 0, buf_size);
				codec_mm_dma_flush(colocate_vaddr, buf_size, DMA_TO_DEVICE);
				codec_mm_unmap_phyaddr(colocate_vaddr);
			}
		}

		hw->dpb.colocated_mv_addr_start =
			hw->collocate_cma_alloc_addr;
#ifdef ONE_COLOCATE_BUF_PER_DECODE_BUF
		hw->dpb.colocated_mv_addr_end  =
			hw->dpb.colocated_mv_addr_start +
			(p_H264_Dpb->colocated_buf_size *
			active_buffer_spec_num);
#else
		hw->dpb.colocated_mv_addr_end  =
			hw->dpb.colocated_mv_addr_start +
			(p_H264_Dpb->colocated_buf_size *
			hw->max_reference_size);
#endif
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"callocate cma, %lx, %x\n",
			hw->collocate_cma_alloc_addr,
			hw->dpb.colocated_mv_addr_start);

		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"colocated_mv_addr_start %x colocated_mv_addr_end %x\n",
			hw->dpb.colocated_mv_addr_start,
			hw->dpb.colocated_mv_addr_end);
		if (!hw->mmu_enable) {
			mutex_lock(&vmh264_mutex);
			if (ret >= 0 && hw->decode_pic_count == 0) {
				int buf_cnt;
				/* h264_reconfig: alloc later*/
				buf_cnt = hw->dpb.mDPB.size;

				for (j = 1; j < buf_cnt; j++) {
					i = get_buf_spec_by_canvas_pos(hw, j);

					if (hw->is_used_v4l) {
						pr_info("v4l: delay alloc the buffer.\n");
						break;
					} else {
							if ((-1 != i) && alloc_one_buf_spec(hw, i) >= 0) {
								config_decode_canvas(hw, i);
							} else {
								return -1;
							}
					}
				}
			}
			mutex_unlock(&vmh264_mutex);
		} else {
			vh264_config_canvs_for_mmu(hw);
		}

		hw->config_bufmgr_done = 1;

	/*end of  config_bufmgr_done */
	}

	return ret;
}

static void vui_config(struct vdec_h264_hw_s *hw)
{
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int aspect_ratio_info_present_flag, aspect_ratio_idc;
	/*time*/
	hw->num_units_in_tick = p_H264_Dpb->num_units_in_tick;
	hw->time_scale = p_H264_Dpb->time_scale;
	hw->timing_info_present_flag = p_H264_Dpb->vui_status & 0x2;

	hw->bitstream_restriction_flag =
		p_H264_Dpb->bitstream_restriction_flag;
	hw->num_reorder_frames =
		p_H264_Dpb->num_reorder_frames;
	hw->max_dec_frame_buffering =
		p_H264_Dpb->max_dec_frame_buffering;

	dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
		"vui_config: pdb %d, %d, %d\n",
		p_H264_Dpb->bitstream_restriction_flag,
		p_H264_Dpb->num_reorder_frames,
		p_H264_Dpb->max_dec_frame_buffering);

	hw->fixed_frame_rate_flag = 0;
	if (hw->timing_info_present_flag) {
		hw->fixed_frame_rate_flag =
			p_H264_Dpb->fixed_frame_rate_flag;

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
					if (!hw->fixed_frame_rate_flag && (p_H264_Dpb->mSPS.profile_idc != BASELINE)) {
						if (frame_dur_es == 7680)
							hw->frame_dur = frame_dur_es /2;
					}
					vdec_schedule_work(&hw->notify_work);
					dpb_print(DECODE_ID(hw),
						PRINT_FLAG_DEC_DETAIL,
						"frame_dur %d from timing_info\n",
						hw->frame_dur);
				}

				/*hack to avoid use ES frame duration when
				 *it's half of the rate from system info
				 * sometimes the encoder is given a wrong
				 * frame rate but the system side information
				 * is more reliable
				 *if ((frame_dur * 2) != frame_dur_es) {
				 *    frame_dur = frame_dur_es;
				 *}
				 */
			}
		}
	} else {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"H.264: timing_info not present\n");
	}

	/*aspect ratio*/
	aspect_ratio_info_present_flag =
		p_H264_Dpb->vui_status & 0x1;
	aspect_ratio_idc = p_H264_Dpb->aspect_ratio_idc;

	if (aspect_ratio_info_present_flag) {
		if (aspect_ratio_idc == EXTEND_SAR) {
			hw->h264_ar = 0x3ff;
			hw->height_aspect_ratio =
				p_H264_Dpb->aspect_ratio_sar_height;
			hw->width_aspect_ratio =
				p_H264_Dpb->aspect_ratio_sar_width;
		} else {
			/* pr_info("v264dec: aspect_ratio_idc = %d\n",
			   aspect_ratio_idc); */

			switch (aspect_ratio_idc) {
			case 1:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 1;
				hw->width_aspect_ratio = 1;
				break;
			case 2:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 11;
				hw->width_aspect_ratio = 12;
				break;
			case 3:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 11;
				hw->width_aspect_ratio = 10;
				break;
			case 4:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 11;
				hw->width_aspect_ratio = 16;
				break;
			case 5:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 33;
				hw->width_aspect_ratio = 40;
				break;
			case 6:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 11;
				hw->width_aspect_ratio = 24;
				break;
			case 7:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 11;
				hw->width_aspect_ratio = 20;
				break;
			case 8:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 11;
				hw->width_aspect_ratio = 32;
				break;
			case 9:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 33;
				hw->width_aspect_ratio = 80;
				break;
			case 10:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 11;
				hw->width_aspect_ratio = 18;
				break;
			case 11:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 11;
				hw->width_aspect_ratio = 15;
				break;
			case 12:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 33;
				hw->width_aspect_ratio = 64;
				break;
			case 13:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 99;
				hw->width_aspect_ratio = 160;
				break;
			case 14:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 3;
				hw->width_aspect_ratio = 4;
				break;
			case 15:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 2;
				hw->width_aspect_ratio = 3;
				break;
			case 16:
				hw->h264_ar = 0x3ff;
				hw->height_aspect_ratio = 1;
				hw->width_aspect_ratio = 2;
				break;
			default:
				if (hw->vh264_ratio >> 16) {
					hw->h264_ar = (hw->frame_height *
						(hw->vh264_ratio & 0xffff) *
						0x100 +
						((hw->vh264_ratio >> 16) *
						 hw->frame_width / 2)) /
						((hw->vh264_ratio >> 16) *
						 hw->frame_width);
					hw->height_aspect_ratio = 1;
					hw->width_aspect_ratio = 1;
				} else {
					hw->h264_ar = 0x3ff;
					hw->height_aspect_ratio = 1;
					hw->width_aspect_ratio = 1;
				}
				break;
			}
		}
	} else {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"v264dec: aspect_ratio not available from source\n");
		if (hw->vh264_ratio >> 16) {
			/* high 16 bit is width, low 16 bit is height */
			hw->h264_ar =
				((hw->vh264_ratio & 0xffff) *
					hw->frame_height * 0x100 +
				 (hw->vh264_ratio >> 16) *
				 hw->frame_width / 2) /
				((hw->vh264_ratio >> 16) *
					hw->frame_width);
			hw->height_aspect_ratio = 1;
			hw->width_aspect_ratio = 1;
		} else {
			hw->h264_ar = 0x3ff;
			hw->height_aspect_ratio = 1;
			hw->width_aspect_ratio = 1;
		}
	}

	if (hw->pts_unstable && (hw->fixed_frame_rate_flag == 0)) {
		if (((hw->frame_dur == RATE_2397_FPS)
		&& (dec_control
		& DEC_CONTROL_FLAG_FORCE_RATE_2397_FPS_FIX_FRAME_RATE))
			|| ((RATE_2997_FPS ==
			hw->frame_dur) &&
		(dec_control &
			DEC_CONTROL_FLAG_FORCE_RATE_2997_FPS_FIX_FRAME_RATE))) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
				"force fix frame rate\n");
			hw->fixed_frame_rate_flag = 0x40;
		}
	}

	/*video_signal_from_vui: to do .. */
}

static void bufmgr_recover(struct vdec_h264_hw_s *hw)
{
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;

	bufmgr_h264_remove_unused_frame(p_H264_Dpb, 2);
	if (hw->error_proc_policy & 0x20) {
		if (!hw->is_used_v4l)
			hw->reset_bufmgr_flag = 1;
	}
}

void bufmgr_force_recover(struct h264_dpb_stru *p_H264_Dpb)
{
	struct vdec_h264_hw_s *hw =
		container_of(p_H264_Dpb, struct vdec_h264_hw_s, dpb);

	dpb_print(DECODE_ID(hw), 0,	"call %s\n", __func__);

	bufmgr_h264_remove_unused_frame(p_H264_Dpb, 2);
	hw->reset_bufmgr_flag = 1;
}

#ifdef CONSTRAIN_MAX_BUF_NUM
static int get_vf_ref_only_buf_count(struct vdec_h264_hw_s *hw)
{
	int i;
	int count = 0;
	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		if (is_buf_spec_in_disp_q(hw, i) &&
			hw->buffer_spec[i].vf_ref > 0)
			count++;
	}
	return count;
}

static int get_used_buf_count(struct vdec_h264_hw_s *hw)
{
	int i;
	int count = 0;
	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		if (is_buf_spec_in_use(hw, i))
			count++;
	}
	return count;
}
#endif


static bool is_buffer_available(struct vdec_s *vdec)
{
	bool buffer_available = 1;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	int i, frame_outside_count = 0, inner_size = 0;
	if ((kfifo_len(&hw->newframe_q) <= 0) ||
	    ((hw->config_bufmgr_done) && (!have_free_buf_spec(vdec))) ||
	    ((p_H264_Dpb->mDPB.init_done) &&
	     (p_H264_Dpb->mDPB.used_size >= (p_H264_Dpb->mDPB.size - 1)) &&
	     (is_there_unused_frame_from_dpb(&p_H264_Dpb->mDPB) == 0))) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_DETAIL,
		"%s, empty, newq(%d), free_spec(%d), initdon(%d), used_size(%d/%d), unused_fr_dpb(%d)\n",
		__func__,
		kfifo_len(&hw->newframe_q),
		have_free_buf_spec(vdec),
		p_H264_Dpb->mDPB.init_done,
		p_H264_Dpb->mDPB.used_size, p_H264_Dpb->mDPB.size,
		is_there_unused_frame_from_dpb(&p_H264_Dpb->mDPB)
		);
		buffer_available = 0;
		if (dpb_is_debug(DECODE_ID(hw),
			DEBUG_DISABLE_RUNREADY_RMBUF))
			return buffer_available;

		if ((hw->error_proc_policy & 0x4) &&
			(hw->error_proc_policy & 0x8)) {
			if ((kfifo_len(&hw->display_q) <= 0) &&
			(p_H264_Dpb->mDPB.used_size >=
				(p_H264_Dpb->mDPB.size - 1)) &&
				(p_Dpb->ref_frames_in_buffer >
				(imax(
				1, p_Dpb->num_ref_frames)
				- p_Dpb->ltref_frames_in_buffer +
				force_sliding_margin))){
				bufmgr_recover(hw);
			} else {
				bufmgr_h264_remove_unused_frame(p_H264_Dpb, 1);
			}
		} else if ((hw->error_proc_policy & 0x4) &&
			(kfifo_len(&hw->display_q) <= 0) &&
			((p_H264_Dpb->mDPB.used_size >=
				(p_H264_Dpb->mDPB.size - 1)) ||
			(!have_free_buf_spec(vdec)))) {
			unsigned long flags;
			spin_lock_irqsave(&hw->bufspec_lock, flags);

			for (i = 0; i < p_Dpb->used_size; i++) {
				if (p_Dpb->fs[i]->pre_output)
					frame_outside_count++;
				else if (p_Dpb->fs[i]->is_output && !is_used_for_reference(p_Dpb->fs[i])) {
					spin_unlock_irqrestore(&hw->bufspec_lock, flags);
					bufmgr_h264_remove_unused_frame(p_H264_Dpb, 0);
					return 0;
				}
			}
			spin_unlock_irqrestore(&hw->bufspec_lock, flags);
			inner_size = p_Dpb->used_size - frame_outside_count;

			if (inner_size >= p_H264_Dpb->dec_dpb_size) {
				bufmgr_recover(hw);
			}
			bufmgr_h264_remove_unused_frame(p_H264_Dpb, 0);
		} else if ((hw->error_proc_policy & 0x8) &&
			(p_Dpb->ref_frames_in_buffer >
			(imax(
			1, p_Dpb->num_ref_frames)
			- p_Dpb->ltref_frames_in_buffer +
			force_sliding_margin)))
			bufmgr_recover(hw);
		else
			bufmgr_h264_remove_unused_frame(p_H264_Dpb, 1);

		if (hw->reset_bufmgr_flag == 1)
			buffer_available = 1;
	}

	if (hw->is_used_v4l)
		buffer_available = have_free_buf_spec(vdec);

	return buffer_available;
}

#define AUX_TAG_SEI				0x2

#define SEI_BUFFERING_PERIOD	0
#define SEI_PicTiming			1
#define SEI_USER_DATA			4
#define SEI_RECOVERY_POINT		6

/*
 *************************************************************************
 * Function:Reads bits from the bitstream buffer
 * Input:
	byte buffer[]
		containing sei message data bits
	int totbitoffset
		bit offset from start of partition
	int bytecount
		total bytes in bitstream
	int numbits
		number of bits to read
 * Output:
	int *info
 * Return:
	-1: failed
	> 0: the count of bit read
 * Attention:
 *************************************************************************
 */

static int get_bits(unsigned char buffer[],
					int totbitoffset,
					int *info,
					int bytecount,
					int numbits)
{
	register int inf;
	long byteoffset;
	int bitoffset;

	int bitcounter = numbits;

	byteoffset = totbitoffset / 8;
	bitoffset = 7 - (totbitoffset % 8);

	inf = 0;
	while (numbits) {
		inf <<= 1;
		inf |= (buffer[byteoffset] & (0x01 << bitoffset)) >> bitoffset;
		numbits--;
		bitoffset--;
		if (bitoffset < 0) {
			byteoffset++;
			bitoffset += 8;
			if (byteoffset > bytecount)
				return -1;
		}
	}

	*info = inf;


	return bitcounter;
}

static int parse_one_sei_record(struct vdec_h264_hw_s *hw,
							u8 *sei_data_buf,
							u8 *sei_data_buf_end)
{
	int payload_type;
	int payload_size;
	u8 *p_sei;
	int temp = 0;
	int bit_offset;
	int read_size;
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;

	p_sei = sei_data_buf;
	read_size = 0;
	payload_type = 0;
	do {
		if (p_sei >= sei_data_buf_end)
			return read_size;

		payload_type += *p_sei;
		read_size++;
	} while (*p_sei++ == 255);


	payload_size = 0;
	do {
		if (p_sei >= sei_data_buf_end)
			return read_size;

		payload_size += *p_sei;
		read_size++;
	} while (*p_sei++ == 255);


	if (p_sei + payload_size > sei_data_buf_end) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
			"%s: payload_type = %d, payload_size = %d is over\n",
			__func__, payload_type, payload_size);
		return read_size;
	}
	bit_offset = 0;

	if (payload_size <= 0) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
			"%s warning: this is a null sei message for payload_type = %d\n",
			__func__, payload_type);
		return read_size;
	}
	p_H264_Dpb->vui_status = p_H264_Dpb->dpb_param.l.data[VUI_STATUS];
	switch (payload_type) {
	case SEI_BUFFERING_PERIOD:
		break;
	case SEI_PicTiming:
		if (p_H264_Dpb->vui_status & 0xc) {
			int cpb_removal_delay;
			int dpb_output_delay;
			u32 delay_len;

			delay_len = p_H264_Dpb->dpb_param.l.data[DELAY_LENGTH];
			cpb_removal_delay
				= (delay_len & 0x1F) + 1;
			dpb_output_delay
				= ((delay_len >> 5) & 0x1F) + 1;

			get_bits(p_sei, bit_offset,
				&temp, payload_size,
				dpb_output_delay+cpb_removal_delay);
			bit_offset += dpb_output_delay+cpb_removal_delay;
		}
		if (p_H264_Dpb->vui_status & 0x10) {
			get_bits(p_sei, bit_offset, &temp, payload_size, 4);
			bit_offset += 4;
			p_H264_Dpb->dpb_param.l.data[PICTURE_STRUCT] = temp;
		}
		break;
	case SEI_USER_DATA:
		if (enable_itu_t35) {
			int i;
			int j;
			int data_len;
			u8 *user_data_buf;

			user_data_buf
				= hw->sei_itu_data_buf + hw->sei_itu_data_len;
			/* user data length should be align with 8 bytes,
			if not, then padding with zero*/
			for (i = 0; i < payload_size; i += 8) {
				if (hw->sei_itu_data_len + i >= SEI_ITU_DATA_SIZE)
					break; // Avoid out-of-bound writing
				for (j = 0; j < 8; j++) {
					int index;

					index = i+7-j;
					if (index >= payload_size)
						user_data_buf[i+j] = 0;
					else
						user_data_buf[i+j]
							= p_sei[i+7-j];
				}
			}

			data_len = payload_size;
			if (payload_size % 8)
				data_len = ((payload_size + 8) >> 3) << 3;

			hw->sei_itu_data_len += data_len;
			if (hw->sei_itu_data_len >= SEI_ITU_DATA_SIZE)
				hw->sei_itu_data_len = SEI_ITU_DATA_SIZE;
			/*
			dpb_print(DECODE_ID(hw), 0,
				"%s: user data, and len = %d:\n",
				__func__, hw->sei_itu_data_len);
			*/
		}
		break;
	case SEI_RECOVERY_POINT:
		p_H264_Dpb->dpb_param.l.data[RECOVERY_POINT] = 1;
		break;
	}

	return read_size + payload_size;
}

static void parse_sei_data(struct vdec_h264_hw_s *hw,
							u8 *sei_data_buf,
							int len)
{
	char *p_sei;
	char *p_sei_end;
	int parsed_size;
	int read_size;


	p_sei = sei_data_buf;
	p_sei_end = p_sei + len;
	parsed_size = 0;
	while (parsed_size < len) {
		read_size = parse_one_sei_record(hw, p_sei, p_sei_end);
		p_sei += read_size;
		parsed_size += read_size;
		if (*p_sei == 0x80) {
			p_sei++;
			parsed_size++;
		}
	}
}

static void check_decoded_pic_error(struct vdec_h264_hw_s *hw)
{
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	struct StorablePicture *p = p_H264_Dpb->mVideo.dec_picture;
	unsigned mby_mbx = READ_VREG(MBY_MBX);
	unsigned mb_total = (hw->seq_info2 >> 8) & 0xffff;
	unsigned mb_width = hw->seq_info2 & 0xff;
	unsigned decode_mb_count;
	if (!mb_width && mb_total) /*for 4k2k*/
		mb_width = 256;
	decode_mb_count = ((mby_mbx & 0xff) * mb_width +
		(((mby_mbx >> 8) & 0xff) + 1));
	if ((mby_mbx == 0) && (p_H264_Dpb->dec_dpb_status != H264_SLICE_HEAD_DONE)) {
		dpb_print(DECODE_ID(hw), 0,
			"mby_mbx is zero\n");
		return;
	}
	if (get_cur_slice_picture_struct(p_H264_Dpb) != FRAME)
		mb_total /= 2;

	if ((hw->error_proc_policy & 0x200) &&
		READ_VREG(ERROR_STATUS_REG) != 0) {
		p->data_flag |= ERROR_FLAG;
	}

	if (hw->error_proc_policy & 0x100 && !(p->data_flag & ERROR_FLAG)) {
		if (decode_mb_count < mb_total) {
			p->data_flag |= ERROR_FLAG;
			if (((hw->error_proc_policy & 0x20000) &&
				decode_mb_count >= mb_total * (100 - mb_count_threshold) / 100)) {
				p->data_flag &= ~ERROR_FLAG;
			}
		}
	}

	if (hw->error_proc_policy & 0x100000) {
		if ((p->data_flag & ERROR_FLAG) && (decode_mb_count < mb_total)) {
			if (hw->ip_field_error_count > 0)
				hw->ip_field_error_count = 0;
		}
	}


	if ((hw->error_proc_policy & 0x100000) &&
			hw->last_dec_picture &&
				(hw->last_dec_picture->slice_type == I_SLICE) &&
				(hw->dpb.mSlice.slice_type == P_SLICE)) {
		if ((p->data_flag & ERROR_FLAG) &&
				(decode_mb_count >= mb_total)) {
				hw->ip_field_error_count++;
				if (hw->ip_field_error_count == 4) {
					unsigned int i;
					struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
					for (i = 0; i < p_Dpb->ref_frames_in_buffer; i++) {
						if (p_Dpb->fs_ref[i]->top_field)
							p_Dpb->fs_ref[i]->top_field->data_flag &= ~ERROR_FLAG;
						if (p_Dpb->fs_ref[i]->bottom_field)
							p_Dpb->fs_ref[i]->bottom_field->data_flag &= ~ERROR_FLAG;
						if (p_Dpb->fs_ref[i]->frame)
							p_Dpb->fs_ref[i]->frame->data_flag &= ~ERROR_FLAG;
					}
					hw->ip_field_error_count = 0;
					p->data_flag &= ~ERROR_FLAG;
					hw->data_flag &= ~ERROR_FLAG;
					dpb_print(DECODE_ID(hw), 0,
						"clear all ref frame error flag\n");
				}
		} else {
			if (hw->ip_field_error_count > 0)
				dpb_print(DECODE_ID(hw), 0,
					"clear error count %d\n", hw->ip_field_error_count);
			hw->ip_field_error_count = 0;
		}
	}

	if (p->data_flag & ERROR_FLAG) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_ERRORFLAG_DBG,
			"%s: decode error, seq_info2 0x%x, mby_mbx 0x%x, mb_total %d decoded mb_count %d ERROR_STATUS_REG 0x%x\n",
			__func__,
			hw->seq_info2,
			mby_mbx,
			mb_total,
			decode_mb_count,
			READ_VREG(ERROR_STATUS_REG)
			);

	}
}

static int vh264_pic_done_proc(struct vdec_s *vdec)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int ret;
	int i;
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;

	if (vdec->mvfrm)
		vdec->mvfrm->hw_decode_time =
		local_clock() - vdec->mvfrm->hw_decode_start;

	if (input_frame_based(vdec) &&
			(!(hw->i_only & 0x2)) &&
			frmbase_cont_bitlevel != 0 &&
			READ_VREG(VIFF_BIT_CNT) >
			frmbase_cont_bitlevel) {
			/*handle the case: multi pictures in one packet*/
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s H264_PIC_DATA_DONE decode slice count %d, continue (bitcnt 0x%x)\n",
			__func__,
			hw->decode_pic_count,
			READ_VREG(VIFF_BIT_CNT));
			hw->frmbase_cont_flag = 1;
		} else
			hw->frmbase_cont_flag = 0;

		if (p_H264_Dpb->mVideo.dec_picture) {
			get_picture_qos_info(p_H264_Dpb->mVideo.dec_picture);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			DEL_EXIST(hw,
				p_H264_Dpb->mVideo.dec_picture) = 0;
			if (vdec->master) {
				struct vdec_h264_hw_s *hw_ba =
				(struct vdec_h264_hw_s *)
					vdec->master->private;
				if (hw_ba->last_dec_picture)
					DEL_EXIST(hw_ba,
						hw_ba->last_dec_picture)
						= 1;
			}
#endif
			mutex_lock(&hw->chunks_mutex);
			if (hw->chunk) {
				p_H264_Dpb->mVideo.dec_picture->pts =
					hw->chunk->pts;
				p_H264_Dpb->mVideo.dec_picture->pts64 =
					hw->chunk->pts64;
				p_H264_Dpb->mVideo.dec_picture->timestamp =
					hw->chunk->timestamp;
#ifdef MH264_USERDATA_ENABLE
				vmh264_udc_fill_vpts(hw,
					p_H264_Dpb->mSlice.slice_type,
					hw->chunk->pts, 1);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			} else if (vdec->master) {
				/*dv enhance layer,
				do not checkout pts*/
				struct StorablePicture *pic =
					p_H264_Dpb->mVideo.dec_picture;
				pic->pts = 0;
				pic->pts64 = 0;
#endif
		} else {
				struct StorablePicture *pic =
					p_H264_Dpb->mVideo.dec_picture;
				u32 offset = pic->offset_delimiter;
				u32 vpts_valid = 0;
				u32 vpts = 0;
				checkout_pts_offset pts_info;

				pic->pic_size = (hw->start_bit_cnt - READ_VREG(VIFF_BIT_CNT)) >> 3;
				if (vdec->pts_server_id == 0) {
					if (pts_pickout_offset_us64(PTS_TYPE_VIDEO,
						offset, &pic->pts, 0, &pic->pts64)) {
						pic->pts = 0;
						pic->pts64 = 0;
					} else {
						vpts = pic->pts;
						vpts_valid = 1;
					}
				} else {

					if (!vdec->ptsserver_peek_pts_offset)
						vdec->ptsserver_peek_pts_offset = symbol_request(ptsserver_peek_pts_offset);

					if (vdec->ptsserver_peek_pts_offset) {
						pts_info.offset = (((u64)hw->frame_dur << 32) & 0xffffffff00000000) | offset;
						if (!vdec->ptsserver_peek_pts_offset((vdec->pts_server_id & 0xff), &pts_info)) {
							vpts = pts_info.pts;
							vpts_valid = 1;
						}
					}
				}
#ifdef MH264_USERDATA_ENABLE

				dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
					"%s: id = %x, offset: %x, vpts: %d, vpts_valid %d\n",
					__func__, vdec->pts_server_id, offset, vpts, vpts_valid);

				vmh264_udc_fill_vpts(hw,
					p_H264_Dpb->mSlice.slice_type,
					vpts, vpts_valid);
#endif
	}
			mutex_unlock(&hw->chunks_mutex);

			check_decoded_pic_error(hw);
#ifdef ERROR_HANDLE_TEST
			if ((hw->data_flag & ERROR_FLAG)
				&& (hw->error_proc_policy & 0x80)) {
				release_cur_decoding_buf(hw);
				h264_clear_dpb(hw);
				hw->dec_flag = 0;
				hw->data_flag = 0;
				hw->skip_frame_count = 0;
				hw->has_i_frame = 0;
				hw->no_error_count = 0xfff;
				hw->no_error_i_count = 0xf;
			} else
#endif
			if (hw->error_proc_policy & 0x200000) {
				if (!hw->loop_flag) {
					for (i = 0; i < p_Dpb->used_size; i++) {
						if ((p_H264_Dpb->mVideo.dec_picture->poc + loop_playback_poc_threshold < p_Dpb->fs[i]->poc) &&
								!p_Dpb->fs[i]->is_output &&
								!p_Dpb->fs[i]->pre_output) {
							hw->loop_flag = 1;
							hw->loop_last_poc = p_H264_Dpb->mVideo.dec_picture->poc;
							break;
						}
					}
				} else {
					if ((p_H264_Dpb->mVideo.dec_picture->poc >= hw->loop_last_poc - poc_threshold) &&
						(p_H264_Dpb->mVideo.dec_picture->poc <= hw->loop_last_poc + poc_threshold)) {
						if (hw->loop_flag >= 5) {
							for (i = 0; i < p_Dpb->used_size; i++) {
								if ((hw->loop_last_poc + loop_playback_poc_threshold < p_Dpb->fs[i]->poc) &&
										!p_Dpb->fs[i]->is_output &&
										!p_Dpb->fs[i]->pre_output) {
									p_Dpb->fs[i]->is_output = 1;
								}
							}
							hw->loop_flag = 0;
						} else
							hw->loop_flag++;
					} else
						hw->loop_flag = 0;
				}
			}
				ret = store_picture_in_dpb(p_H264_Dpb,
					p_H264_Dpb->mVideo.dec_picture,
					hw->data_flag | hw->dec_flag |
				p_H264_Dpb->mVideo.dec_picture->data_flag);



			if (ret == -1) {
				release_cur_decoding_buf(hw);
				bufmgr_force_recover(p_H264_Dpb);
			} else if (ret == -2) {
				release_cur_decoding_buf(hw);
			} else {
			if (hw->data_flag & ERROR_FLAG) {
				hw->no_error_count = 0;
				hw->no_error_i_count = 0;
			} else {
				hw->no_error_count++;
				if (hw->data_flag & I_FLAG)
					hw->no_error_i_count++;
			}
			if (hw->mmu_enable)
				hevc_set_unused_4k_buff_idx(hw,
					p_H264_Dpb->mVideo.
						dec_picture->buf_spec_num);
			bufmgr_post(p_H264_Dpb);
				hw->last_dec_picture =
					p_H264_Dpb->mVideo.dec_picture;
			mutex_lock(&hw->pic_mutex);
			p_H264_Dpb->mVideo.dec_picture = NULL;
			mutex_unlock(&hw->pic_mutex);
			/* dump_dpb(&p_H264_Dpb->mDPB); */
			hw->has_i_frame = 1;
			if (hw->mmu_enable)
				hevc_set_frame_done(hw);
			hw->decode_pic_count++;
			p_H264_Dpb->decode_pic_count = hw->decode_pic_count;
			if (hw->skip_frame_count > 0) {
				/*skip n frame after first I */
				hw->skip_frame_count--;
				if (hw->skip_frame_count == 0)
					hw->dec_flag &= (~NODISP_FLAG);
			} else if (hw->skip_frame_count < -1) {
				/*skip n frame after first I until second I */
				hw->skip_frame_count++;
				if (hw->skip_frame_count == -1)
					hw->dec_flag &= (~NODISP_FLAG);
				}
			}
		}
		return 0;
}

static void vh264_cal_frame_size(struct vdec_s *vdec)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	unsigned char i_flag =	(p_H264_Dpb->dpb_param.l.data[SLICE_TYPE] == I_Slice);
	unsigned int frame_crop_left_offset = 0;
	unsigned int frame_crop_right_offset = 0;
	unsigned int frame_crop_top_offset = 0;
	unsigned int frame_crop_bottom_offset = 0;
	unsigned int frame_mbs_only_flag = 0;
	unsigned int chroma_format_idc = 0;
	unsigned int crop_bottom = 0, crop_right = 0;
	int sub_width_c = 0, sub_height_c = 0;
	u32 frame_width = 0, frame_height = 0;
	int mb_width = 0, mb_total = 0, mb_height = 0;

	if (!i_flag)
		return ;

	mb_width = hw->seq_info2 & 0xff;
	mb_total = (hw->seq_info2 >> 8) & 0xffff;
	if (!mb_width && mb_total) /*for 4k2k*/
		mb_width = 256;
	if (mb_width)
		mb_height = mb_total/mb_width;
	if (mb_width <= 0 || mb_height <= 0 ||
		is_oversize(mb_width << 4, mb_height << 4)) {
		return ;
	}

	if (!i_flag)
		return ;

	chroma_format_idc = p_H264_Dpb->chroma_format_idc;
	frame_crop_left_offset = p_H264_Dpb->frame_crop_left_offset;
	frame_crop_right_offset = p_H264_Dpb->frame_crop_right_offset;
	frame_crop_top_offset = p_H264_Dpb->frame_crop_top_offset;
	frame_crop_bottom_offset = p_H264_Dpb->frame_crop_bottom_offset;

	frame_mbs_only_flag = (hw->seq_info >> 15) & 0x01;
	if (hw->dpb.mSPS.profile_idc != 100 &&
		hw->dpb.mSPS.profile_idc != 110 &&
		hw->dpb.mSPS.profile_idc != 122 &&
		hw->dpb.mSPS.profile_idc != 144) {
		chroma_format_idc = 1;
	}

	switch (chroma_format_idc) {
		case 1:
			sub_width_c = 2;
			sub_height_c = 2;
			break;

		case 2:
			sub_width_c = 2;
			sub_height_c = 1;
			break;

		case 3:
			sub_width_c = 1;
			sub_height_c = 1;
			break;

		default:
			break;
	}

	if (chroma_format_idc == 0) {
		crop_right = frame_crop_right_offset;
		crop_bottom = frame_crop_bottom_offset *
			(2 - frame_mbs_only_flag);
	} else {
		crop_right = sub_width_c * frame_crop_right_offset;
		crop_bottom = sub_height_c * frame_crop_bottom_offset *
			(2 - frame_mbs_only_flag);
	}

	frame_width = mb_width << 4;
	frame_height = mb_height << 4;

	frame_width = frame_width - crop_right;
	frame_height = frame_height - crop_bottom;

	if (((frame_width != hw->frame_width) || (frame_height != hw->frame_height)) &&
		(!is_oversize(frame_width, frame_height))) {
		dpb_print(DECODE_ID(hw), 0,
		"%s frame size from (%d x %d) change to (%d x %d)\n",
		__func__, hw->frame_width, hw->frame_height, frame_width, frame_height);
		hw->frame_width = frame_width;
		hw->frame_height = frame_height;
		dpb_print(p_H264_Dpb->decoder_index, 0,
		"%s chroma_format_idc %d crop offset: left %d right %d top %d bottom %d\n",
		__func__, chroma_format_idc,
		frame_crop_left_offset,
		frame_crop_right_offset,
		frame_crop_top_offset,
		frame_crop_bottom_offset);
	}
}

static irqreturn_t vh264_isr_thread_fn(struct vdec_s *vdec, int irq)
{
	int i;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	unsigned int dec_dpb_status = p_H264_Dpb->dec_dpb_status;
	u32 debug_tag;

	if (dec_dpb_status == H264_SLICE_HEAD_DONE ||
		p_H264_Dpb->dec_dpb_status == H264_CONFIG_REQUEST) {
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_START);
	}
	else if (dec_dpb_status == H264_PIC_DATA_DONE) {
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_PIC_DONE_START);
	}
	else if (dec_dpb_status == H264_SEI_DATA_READY)
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_SEI_START);
	else if (dec_dpb_status == H264_AUX_DATA_READY)
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_AUX_START);

	if (dec_dpb_status == H264_CONFIG_REQUEST) {
#if 1
		unsigned short *p = (unsigned short *)hw->lmem_addr;
		for (i = 0; i < (RPM_END-RPM_BEGIN); i += 4) {
			int ii;
			for (ii = 0; ii < 4; ii++) {
				p_H264_Dpb->dpb_param.l.data[i+ii] =
					p[i+3-ii];
				if (dpb_is_debug(DECODE_ID(hw),
					RRINT_FLAG_RPM)) {
					if (((i + ii) & 0xf) == 0)
						dpb_print(DECODE_ID(hw),
							RRINT_FLAG_RPM, "%04x:",
							i);
					dpb_print_cont(DECODE_ID(hw),
						0, "%04x ",
						p[i+3-ii]);
					if (((i + ii + 1) & 0xf) == 0)
						dpb_print_cont(
						DECODE_ID(hw),
							0, "\r\n");
				}
			}
		}
		if (p_H264_Dpb->bitstream_restriction_flag !=
			((p_H264_Dpb->dpb_param.l.data[SPS_FLAGS2] >> 3) & 0x1)) {
			dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
				"p_H264_Dpb->bitstream_restriction_flag 0x%x, new 0x%x\n",
				p_H264_Dpb->bitstream_restriction_flag, ((p_H264_Dpb->dpb_param.l.data[SPS_FLAGS2] >> 3) & 0x1));
		}
		p_H264_Dpb->bitstream_restriction_flag =
			(p_H264_Dpb->dpb_param.l.data[SPS_FLAGS2] >> 3) & 0x1;
		p_H264_Dpb->num_reorder_frames =
			p_H264_Dpb->dpb_param.l.data[NUM_REORDER_FRAMES];
		p_H264_Dpb->max_dec_frame_buffering =
			p_H264_Dpb->dpb_param.l.data[MAX_BUFFER_FRAME];

		dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
			"H264_CONFIG_REQUEST: pdb %d, %d, %d\n",
			p_H264_Dpb->bitstream_restriction_flag,
			p_H264_Dpb->num_reorder_frames,
			p_H264_Dpb->max_dec_frame_buffering);
		hw->bitstream_restriction_flag =
			p_H264_Dpb->bitstream_restriction_flag;
		hw->num_reorder_frames =
			p_H264_Dpb->num_reorder_frames;
		hw->max_dec_frame_buffering =
			p_H264_Dpb->max_dec_frame_buffering;

		/*crop*/
		p_H264_Dpb->chroma_format_idc = (p_H264_Dpb->dpb_param.l.data[MAX_REFERENCE_FRAME_NUM_IN_MEM]>>8 & 0x3);
		p_H264_Dpb->frame_crop_left_offset = p_H264_Dpb->dpb_param.l.data[FRAME_CROP_LEFT_OFFSET];
		p_H264_Dpb->frame_crop_right_offset = p_H264_Dpb->dpb_param.l.data[FRAME_CROP_RIGHT_OFFSET];
		p_H264_Dpb->frame_crop_top_offset = p_H264_Dpb->dpb_param.l.data[FRAME_CROP_TOP_OFFSET];
		p_H264_Dpb->frame_crop_bottom_offset = p_H264_Dpb->dpb_param.l.data[FRAME_CROP_BOTTOM_OFFSET];

		dpb_print(p_H264_Dpb->decoder_index, PRINT_FLAG_DPB_DETAIL,
		"%s chroma_format_idc %d crop offset: left %d right %d top %d bottom %d\n",
		__func__, p_H264_Dpb->chroma_format_idc,
		p_H264_Dpb->frame_crop_left_offset,
		p_H264_Dpb->frame_crop_right_offset,
		p_H264_Dpb->frame_crop_top_offset,
		p_H264_Dpb->frame_crop_bottom_offset);
#endif

		WRITE_VREG(DPB_STATUS_REG, H264_ACTION_CONFIG_DONE);
		reset_process_time(hw);
		hw->reg_iqidct_control = READ_VREG(IQIDCT_CONTROL);
		hw->reg_iqidct_control_init_flag = 1;
		hw->dec_result = DEC_RESULT_CONFIG_PARAM;
#ifdef DETECT_WRONG_MULTI_SLICE
		/*restart check count and set 'unknown'*/
		dpb_print(DECODE_ID(hw), PRINT_FLAG_UCODE_EVT,
		"%s MULTI_SLICE_DETECT (check_count %d slice_count %d cur_slice_count %d flag %d), H264_CONFIG_REQUEST => restart check\n",
		__func__,
		hw->multi_slice_pic_check_count,
		hw->picture_slice_count,
		hw->cur_picture_slice_count,
		hw->multi_slice_pic_flag);

		hw->multi_slice_pic_check_count = 0;
		hw->multi_slice_pic_flag = 0;
		hw->picture_slice_count = 0;
#endif
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);
		vdec_schedule_work(&hw->work);
	} else if (dec_dpb_status == H264_SLICE_HEAD_DONE) {
		u16 data_hight;
		u16 data_low;
		u32 video_signal;

		int slice_header_process_status = 0;
		int I_flag;
		int frame_num_gap = 0;
		union param dpb_param_bak;
		/*unsigned char is_idr;*/
		unsigned short *p = (unsigned short *)hw->lmem_addr;
		unsigned mb_width = hw->seq_info2 & 0xff;
		unsigned short first_mb_in_slice;
		unsigned int decode_mb_count, mby_mbx;
		struct StorablePicture *pic = p_H264_Dpb->mVideo.dec_picture;
		reset_process_time(hw);
		hw->frmbase_cont_flag = 0;

		if ((pic != NULL) && (pic->mb_aff_frame_flag == 1))
			first_mb_in_slice = p[FIRST_MB_IN_SLICE + 3] * 2;
		else
			first_mb_in_slice = p[FIRST_MB_IN_SLICE + 3];

#ifdef DETECT_WRONG_MULTI_SLICE
		hw->cur_picture_slice_count++;


		if ((hw->error_proc_policy & 0x10000) &&
			(hw->cur_picture_slice_count > 1) &&
			(first_mb_in_slice == 0) &&
			(hw->multi_slice_pic_flag == 0))
				hw->multi_slice_pic_check_count = 0;

		if ((hw->error_proc_policy & 0x10000) &&
			(hw->cur_picture_slice_count > 1) &&
			(hw->multi_slice_pic_flag == 1)) {
			dpb_print(DECODE_ID(hw), 0,
			"%s MULTI_SLICE_DETECT (check_count %d slice_count %d cur_slice_count %d flag %d), WRONG_MULTI_SLICE detected, insert picture\n",
			__func__,
			hw->multi_slice_pic_check_count,
			hw->picture_slice_count,
			hw->cur_picture_slice_count,
			hw->multi_slice_pic_flag);

			mby_mbx = READ_VREG(MBY_MBX);
			decode_mb_count = ((mby_mbx & 0xff) * mb_width +
					(((mby_mbx >> 8) & 0xff) + 1));

			if (first_mb_in_slice == decode_mb_count &&
				first_mb_in_slice != 0) {
				dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
				"%s first_mb_in_slice = %d \n",
				__func__, first_mb_in_slice);

				hw->multi_slice_pic_flag = 0;
				hw->multi_slice_pic_check_count = 0;
			} else if (hw->cur_picture_slice_count > hw->last_picture_slice_count) {
				vh264_pic_done_proc(vdec);
				//if (p_H264_Dpb->mDPB.used_size == p_H264_Dpb->mDPB.size) {
				if (!have_free_buf_spec(vdec)) {
					dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS, "dpb full, wait buffer\n");
					p_H264_Dpb->mVideo.pre_frame_num = hw->first_pre_frame_num;
					hw->last_picture_slice_count = hw->cur_picture_slice_count;
					hw->no_decoder_buffer_flag = 1;
					hw->dec_result = DEC_RESULT_AGAIN;
					vdec_schedule_work(&hw->work);
					return IRQ_HANDLED;
				}
			}
			else {
				if (p_H264_Dpb->mVideo.dec_picture) {
					if (p_H264_Dpb->mVideo.dec_picture->colocated_buf_index >= 0) {
						release_colocate_buf(p_H264_Dpb,
						p_H264_Dpb->mVideo.dec_picture->colocated_buf_index);
						p_H264_Dpb->mVideo.dec_picture->colocated_buf_index = -1;
					}
				}
				release_cur_decoding_buf(hw);
			}
		}

		if (hw->error_proc_policy & 0x10000) {
			if (hw->multi_slice_pic_flag == 0 && (first_mb_in_slice == 0 && decode_mb_count > 0) &&
				(hw->cur_picture_slice_count > 1 &&
				(hw->cur_picture_slice_count > hw->last_picture_slice_count))) {
				vh264_pic_done_proc(vdec);
			}
		}
#endif

		hw->reg_iqidct_control = READ_VREG(IQIDCT_CONTROL);
		hw->reg_iqidct_control_init_flag = 1;
		hw->reg_vcop_ctrl_reg = READ_VREG(VCOP_CTRL_REG);
		hw->reg_rv_ai_mb_count = READ_VREG(RV_AI_MB_COUNT);
		hw->vld_dec_control = READ_VREG(VLD_DECODE_CONTROL);
		if (input_frame_based(vdec) &&
			frmbase_cont_bitlevel2 != 0 &&
			READ_VREG(VIFF_BIT_CNT) <
			frmbase_cont_bitlevel2 &&
			hw->get_data_count >= 0x70000000) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s H264_SLICE_HEAD_DONE with small bitcnt %d, goto empty_proc\n",
			__func__,
			READ_VREG(VIFF_BIT_CNT));

			goto empty_proc;
		}

#if 0
		if (p_H264_Dpb->mVideo.dec_picture == NULL) {
			if (!is_buffer_available(vdec)) {
				hw->buffer_empty_flag = 1;
				dpb_print(DECODE_ID(hw),
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
		dpb_param_bak = p_H264_Dpb->dpb_param;

		ATRACE_COUNTER(hw->trace.decode_header_time_name, TRACE_HEADER_RPM_START);

		for (i = 0; i < (RPM_END-RPM_BEGIN); i += 4) {
			int ii;

			for (ii = 0; ii < 4; ii++) {
				p_H264_Dpb->dpb_param.l.data[i+ii] =
					p[i+3-ii];
				if (dpb_is_debug(DECODE_ID(hw),
					RRINT_FLAG_RPM)) {
					if (((i + ii) & 0xf) == 0)
						dpb_print(DECODE_ID(hw),
							RRINT_FLAG_RPM, "%04x:",
							i);
					dpb_print_cont(DECODE_ID(hw),
						0, "%04x ",
						p[i+3-ii]);
					if (((i + ii + 1) & 0xf) == 0)
						dpb_print_cont(
						DECODE_ID(hw),
							0, "\r\n");
				}
			}
		}
		ATRACE_COUNTER(hw->trace.decode_header_time_name, TRACE_HEADER_RPM_END);
#endif
#ifdef DETECT_WRONG_MULTI_SLICE

		if (p_H264_Dpb->mVideo.dec_picture &&
				hw->multi_slice_pic_flag == 2 &&
				(dpb_param_bak.l.data[FIRST_MB_IN_SLICE] > p_H264_Dpb->dpb_param.l.data[FIRST_MB_IN_SLICE])) {
			dpb_print(DECODE_ID(hw), 0,
				"decode next pic, save before, SLICE_TYPE BAK %d, SLICE_TYPE %d, FIRST_MB_IN_SLICE BAK %d, FIRST_MB_IN_SLICE %d\n",
					dpb_param_bak.l.data[SLICE_TYPE], p_H264_Dpb->dpb_param.l.data[SLICE_TYPE],
					dpb_param_bak.l.data[FIRST_MB_IN_SLICE], p_H264_Dpb->dpb_param.l.data[FIRST_MB_IN_SLICE]);
			vh264_pic_done_proc(vdec);
		}
#endif
		data_low = p_H264_Dpb->dpb_param.l.data[VIDEO_SIGNAL_LOW];
		data_hight = p_H264_Dpb->dpb_param.l.data[VIDEO_SIGNAL_HIGHT];

		video_signal = (data_hight << 16) | data_low;
		hw->video_signal_from_vui =
					((video_signal & 0xffff) << 8) |
					((video_signal & 0xff0000) >> 16) |
					((video_signal & 0x3f000000));

		/* When the matrix_coeffiecents, transfer_characteristics and colour_primaries
		 * syntax elements are absent, their values shall be presumed to be equal to 2
		 */
		 if ((hw->video_signal_from_vui & 0x1000000) == 0) {
			hw->video_signal_from_vui = hw->video_signal_from_vui & 0xff000000;
			hw->video_signal_from_vui = hw->video_signal_from_vui | 0x20202;
		}

		/*dpb_print(DECODE_ID(hw),
				0,
				"video_signal_from_vui:0x%x, "
				"data_low:0x%x, data_hight:0x%x\n",
				hw->video_signal_from_vui,
				data_low,
				data_hight);*/

		parse_sei_data(hw, hw->sei_data_buf, hw->sei_data_len);

		if (hw->config_bufmgr_done == 0) {
			hw->dec_result = DEC_RESULT_DONE;
			vdec_schedule_work(&hw->work);
			dpb_print(DECODE_ID(hw),
				PRINT_FLAG_UCODE_EVT,
				"config_bufmgr not done, discard frame\n");
			return IRQ_HANDLED;
		} else if ((hw->first_i_policy & 0x3) != 0) {
			unsigned char is_i_slice =
				(p_H264_Dpb->dpb_param.l.data[SLICE_TYPE]
					== I_Slice)
				? 1 : 0;
			unsigned char is_idr =
			((p_H264_Dpb->dpb_param.dpb.NAL_info_mmco & 0x1f)
				== 5);
			if ((hw->first_i_policy & 0x3) == 0x3)
				is_i_slice = is_idr;
			if (!is_i_slice) {
				if (hw->has_i_frame == 0) {
					amvdec_stop();
					vdec->mc_loaded = 0;
					hw->dec_result = DEC_RESULT_DONE;
					vdec_schedule_work(&hw->work);
					dpb_print(DECODE_ID(hw),
						PRINT_FLAG_UCODE_EVT,
						"has_i_frame is 0, discard none I(DR) frame silce_type %d is_idr %d\n", p_H264_Dpb->dpb_param.l.data[SLICE_TYPE], is_idr);
					return IRQ_HANDLED;
				}
			} else {
				if (hw->skip_frame_count < 0 || is_idr) {
					/* second I */
					hw->dec_flag &= (~NODISP_FLAG);
					hw->skip_frame_count = 0;
				}
				if (hw->has_i_frame == 0 &&
					(!is_idr)) {
					int skip_count =
						(hw->first_i_policy >> 8) & 0xff;
					/* first I (not IDR) */
					if ((hw->first_i_policy & 0x3) == 2)
						hw->skip_frame_count =
							-1 - skip_count;
					else
						hw->skip_frame_count =
							skip_count;
					if (hw->skip_frame_count != 0)
						hw->dec_flag |= NODISP_FLAG;
				}
			}
		}
		dpb_print(DECODE_ID(hw), PRINT_FLAG_UCODE_EVT,
			"current dpb index %d, poc %d, top/bot poc (%d,%d)\n",
			p_H264_Dpb->dpb_param.dpb.current_dpb_index,
			val(p_H264_Dpb->dpb_param.dpb.frame_pic_order_cnt),
			val(p_H264_Dpb->dpb_param.dpb.top_field_pic_order_cnt),
			val(p_H264_Dpb->dpb_param.dpb.top_field_pic_order_cnt));
		I_flag = (p_H264_Dpb->dpb_param.l.data[SLICE_TYPE] == I_Slice)
			? I_FLAG : 0;

		if ((hw->i_only & 0x2) && (I_flag & I_FLAG))
			flush_dpb(p_H264_Dpb);

		if ((hw->i_only & 0x2) && (!(I_flag & I_FLAG)) &&
			(p_H264_Dpb->mSlice.structure == FRAME)) {
				hw->data_flag = NULL_FLAG;
				goto pic_done_proc;
		}

		slice_header_process_status =
			h264_slice_header_process(p_H264_Dpb, &frame_num_gap);
		if (hw->mmu_enable)
			hevc_sao_set_slice_type(hw,
				slice_header_process_status,
					hw->dpb.mSlice.idr_flag);
		vui_config(hw);

		vh264_cal_frame_size(vdec);

		if (p_H264_Dpb->mVideo.dec_picture) {
			int cfg_ret = 0;
			bool field_pic_flag = false;
			unsigned mby_mbx = READ_VREG(MBY_MBX);
			struct StorablePicture *p =
				p_H264_Dpb->mVideo.dec_picture;

			if (slice_header_process_status == 1) {
				if (!p_H264_Dpb->mSPS.frame_mbs_only_flag) {
					field_pic_flag =
						(p_H264_Dpb->mSlice.structure == TOP_FIELD ||
						p_H264_Dpb->mSlice.structure == BOTTOM_FIELD) ?
						true : false;
				}

				vdec_set_profile_level(vdec, p_H264_Dpb->mSPS.profile_idc,
					p_H264_Dpb->mSPS.level_idc);

				if (!field_pic_flag && (((p_H264_Dpb->mSPS.profile_idc == BASELINE) &&
					(p_H264_Dpb->dec_dpb_size < 2)) ||
					(((unsigned long)(hw->vh264_amstream_dec_info
						.param)) & 0x8) || hw->low_latency_mode & 0x8)) {
					p_H264_Dpb->fast_output_enable =
					H264_OUTPUT_MODE_FAST;
				}
				else
					p_H264_Dpb->fast_output_enable
							= fast_output_enable;
				if (hw->enable_fence)
					p_H264_Dpb->fast_output_enable = H264_OUTPUT_MODE_FAST;

				hw->data_flag = I_flag;
				if ((p_H264_Dpb->
					dpb_param.dpb.NAL_info_mmco & 0x1f)
					== 5)
					hw->data_flag |= IDR_FLAG;
				if ((p_H264_Dpb->dpb_param.l.data[FIRST_MB_IN_SLICE]) && !mby_mbx) {
					p->data_flag |= ERROR_FLAG;
					dpb_print(DECODE_ID(hw),
						PRINT_FLAG_VDEC_STATUS,
						"one slice error in muulti-slice  first_mb 0x%x mby_mbx 0x%x  slice_type %d\n",
						p_H264_Dpb->dpb_param.l.
						data[FIRST_MB_IN_SLICE],
						READ_VREG(MBY_MBX),
						 p->slice_type);
				}
				dpb_print(DECODE_ID(hw),
				PRINT_FLAG_VDEC_STATUS,
				"==================> frame count %d to skip %d\n",
				hw->decode_pic_count+1,
				hw->skip_frame_count);
				} else if (hw->error_proc_policy & 0x100){
					unsigned decode_mb_count =
						((mby_mbx & 0xff) * hw->mb_width +
						(((mby_mbx >> 8) & 0xff) + 1));
					if (decode_mb_count <
						((p_H264_Dpb->dpb_param.l.data[FIRST_MB_IN_SLICE]) *
						(1 + p->mb_aff_frame_flag)) && decode_mb_count) {
						dpb_print(DECODE_ID(hw),
						PRINT_FLAG_VDEC_STATUS,
						"Error detect! first_mb 0x%x mby_mbx 0x%x decode_mb 0x%x\n",
						p_H264_Dpb->dpb_param.l.
						data[FIRST_MB_IN_SLICE],
						READ_VREG(MBY_MBX),
						decode_mb_count);
						p->data_flag |= ERROR_FLAG;
					}/* else if (!p_H264_Dpb->dpb_param.l.data[FIRST_MB_IN_SLICE] && decode_mb_count) {
						p->data_flag |= ERROR_FLAG;
						goto pic_done_proc;
					}*/
				}

			if (!I_flag && frame_num_gap && (!p_H264_Dpb->long_term_reference_flag)
				&& input_frame_based(vdec)) {
				if (!(hw->error_proc_policy & 0x800000)) {
					hw->data_flag |= ERROR_FLAG;
					p_H264_Dpb->mVideo.dec_picture->data_flag |= ERROR_FLAG;
					dpb_print(DECODE_ID(hw), 0, "frame number gap error\n");
				}
			}

			if ((hw->error_proc_policy & 0x400) && !hw->enable_fence) {
				int ret = dpb_check_ref_list_error(p_H264_Dpb);
				if (ret != 0) {
					hw->reflist_error_count ++;
					dpb_print(DECODE_ID(hw), 0,
						"reference list error %d frame count %d to skip %d reflist_error_count %d\n",
						ret,
						hw->decode_pic_count+1,
						hw->skip_frame_count,
						hw->reflist_error_count);

					p_H264_Dpb->mVideo.dec_picture->data_flag = NODISP_FLAG;
					if (((hw->error_proc_policy & 0x80)
						&& ((hw->dec_flag &
							NODISP_FLAG) == 0)) ||(hw->reflist_error_count > 50)) {
						hw->reset_bufmgr_flag = 1;
						hw->reflist_error_count = 0;
						amvdec_stop();
						vdec->mc_loaded = 0;
						hw->dec_result = DEC_RESULT_DONE;
						vdec_schedule_work(&hw->work);
						return IRQ_HANDLED;
					}
				} else
					hw->reflist_error_count = 0;
			}
			if ((hw->error_proc_policy & 0x800) && (!(hw->i_only & 0x2))
				&& p_H264_Dpb->dpb_error_flag != 0) {
				dpb_print(DECODE_ID(hw), 0,
					"dpb error %d\n",
					p_H264_Dpb->dpb_error_flag);
				hw->data_flag |= ERROR_FLAG;
				p_H264_Dpb->mVideo.dec_picture->data_flag |= ERROR_FLAG;
				if ((hw->error_proc_policy & 0x80) &&
					((hw->dec_flag & NODISP_FLAG) == 0)) {
					hw->reset_bufmgr_flag = 1;
					amvdec_stop();
					vdec->mc_loaded = 0;
					hw->dec_result = DEC_RESULT_DONE;
					vdec_schedule_work(&hw->work);
					return IRQ_HANDLED;
				}
			}
			ATRACE_COUNTER(hw->trace.decode_header_time_name, TRACE_HEADER_REGISTER_START);
			cfg_ret = config_decode_buf(hw,
				p_H264_Dpb->mVideo.dec_picture);
			ATRACE_COUNTER(hw->trace.decode_header_time_name, TRACE_HEADER_REGISTER_END);
			if (cfg_ret < 0) {
				dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
					"config_decode_buf fail (%d)\n",
					cfg_ret);
				if (hw->error_proc_policy & 0x2) {
					release_cur_decoding_buf(hw);
					/*hw->data_flag |= ERROR_FLAG;*/
					hw->reset_bufmgr_flag = 1;
					hw->dec_result = DEC_RESULT_DONE;
					vdec_schedule_work(&hw->work);
					return IRQ_HANDLED;
				} else
					hw->data_flag |= ERROR_FLAG;
				p_H264_Dpb->mVideo.dec_picture->data_flag |= ERROR_FLAG;
			}
		}

		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_HEAD_END);

		if (slice_header_process_status == 1)
			WRITE_VREG(DPB_STATUS_REG, H264_ACTION_DECODE_NEWPIC);
		else
			WRITE_VREG(DPB_STATUS_REG, H264_ACTION_DECODE_SLICE);
		hw->last_mby_mbx = 0;
		hw->last_vld_level = 0;
		start_process_time(hw);
	} else if (dec_dpb_status == H264_PIC_DATA_DONE
		||((dec_dpb_status == H264_DATA_REQUEST) && input_frame_based(vdec))) {
#ifdef DETECT_WRONG_MULTI_SLICE
				dpb_print(DECODE_ID(hw), PRINT_FLAG_UCODE_EVT,
				"%s MULTI_SLICE_DETECT (check_count %d slice_count %d cur_slice_count %d flag %d), H264_PIC_DATA_DONE\n",
				__func__,
				hw->multi_slice_pic_check_count,
				hw->picture_slice_count,
				hw->cur_picture_slice_count,
				hw->multi_slice_pic_flag);

				if (hw->multi_slice_pic_check_count < check_slice_num) {
					hw->multi_slice_pic_check_count++;
					if (hw->cur_picture_slice_count !=
						hw->picture_slice_count) {
						/*restart check count and set 'unknown'*/
						hw->multi_slice_pic_check_count = 0;
						hw->multi_slice_pic_flag = 0;
					}
					hw->picture_slice_count =
						hw->cur_picture_slice_count;
				} else if (hw->multi_slice_pic_check_count >= check_slice_num) {
					if (hw->picture_slice_count > 1)
						hw->multi_slice_pic_flag = 2;
					else
						hw->multi_slice_pic_flag = 1;
				}
#endif

pic_done_proc:
		reset_process_time(hw);
		if ((dec_dpb_status == H264_SEARCH_BUFEMPTY) ||
			(dec_dpb_status == H264_DECODE_BUFEMPTY) ||
			(dec_dpb_status == H264_DECODE_TIMEOUT) ||
			((dec_dpb_status == H264_DATA_REQUEST) && input_frame_based(vdec))) {
			hw->data_flag |= ERROR_FLAG;
			if (hw->dpb.mVideo.dec_picture)
				hw->dpb.mVideo.dec_picture->data_flag |= ERROR_FLAG;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_DETAIL,
				"%s, mark err_frame\n", __func__);
		}
		vh264_pic_done_proc(vdec);

		if (hw->frmbase_cont_flag) {
			/*do not DEC_RESULT_GET_DATA*/
			hw->get_data_count = 0x7fffffff;
			WRITE_VREG(DPB_STATUS_REG, H264_ACTION_SEARCH_HEAD);
			decode_frame_count[DECODE_ID(hw)]++;
			if (p_H264_Dpb->mSlice.slice_type == I_SLICE) {
				hw->gvs.i_decoded_frames++;
			} else if (p_H264_Dpb->mSlice.slice_type == P_SLICE) {
				hw->gvs.p_decoded_frames++;
			} else if (p_H264_Dpb->mSlice.slice_type == B_SLICE) {
				hw->gvs.b_decoded_frames++;
			}
			start_process_time(hw);
			return IRQ_HANDLED;
		}
		amvdec_stop();
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s %s decode slice count %d\n",
			__func__,
			(dec_dpb_status == H264_PIC_DATA_DONE) ?
			"H264_PIC_DATA_DONE" :
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			(dec_dpb_status == H264_FIND_NEXT_PIC_NAL) ?
			"H264_FIND_NEXT_PIC_NAL" : "H264_FIND_NEXT_DVEL_NAL",
#else
			" ",
#endif
			hw->decode_pic_count);
		if (hw->kpi_first_i_decoded == 0) {
			hw->kpi_first_i_decoded = 1;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_DETAIL,
				"[vdec_kpi][%s] First I frame decoded.\n", __func__);
		}
		/* WRITE_VREG(DPB_STATUS_REG, H264_ACTION_SEARCH_HEAD); */
		hw->dec_result = DEC_RESULT_DONE;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (vdec->slave &&
			dec_dpb_status == H264_FIND_NEXT_DVEL_NAL) {
			struct vdec_h264_hw_s *hw_el =
			 (struct vdec_h264_hw_s *)(vdec->slave->private);
			hw_el->got_valid_nal = 0;
			hw->switch_dvlayer_flag = 1;
		} else if (vdec->master &&
			dec_dpb_status == H264_FIND_NEXT_PIC_NAL) {
			struct vdec_h264_hw_s *hw_bl =
			 (struct vdec_h264_hw_s *)(vdec->master->private);
			hw_bl->got_valid_nal = 0;
			hw->switch_dvlayer_flag = 1;
		} else {
			hw->switch_dvlayer_flag = 0;
			hw->got_valid_nal = 1;
		}
#endif
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_EDN);

		hw->dec_result = DEC_RESULT_DONE;
		vdec_schedule_work(&hw->work);

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	} else if (
			(dec_dpb_status == H264_FIND_NEXT_PIC_NAL) ||
			(dec_dpb_status == H264_FIND_NEXT_DVEL_NAL)) {
		goto pic_done_proc;
#endif
	} else if (dec_dpb_status == H264_AUX_DATA_READY) {
		reset_process_time(hw);
		if (READ_VREG(H264_AUX_DATA_SIZE) != 0) {
			if (dpb_is_debug(DECODE_ID(hw),
				PRINT_FLAG_SEI_DETAIL))
				dump_aux_buf(hw);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (vdec_frame_based(vdec)) {
				if (hw->last_dec_picture)
					set_aux_data(hw,
						hw->last_dec_picture, 0, 0, NULL);
			} else if (vdec->dolby_meta_with_el || vdec->slave) {
				if (hw->last_dec_picture)
					set_aux_data(hw, hw->last_dec_picture,
						0, 0, NULL);
			} else {
				if (vdec->master) {
						struct vdec_h264_hw_s *hw_bl =
						(struct vdec_h264_hw_s *)
						(vdec->master->private);
					if (hw_bl->last_dec_picture != NULL) {
						set_aux_data(hw_bl,
							hw_bl->last_dec_picture,
							0, 1, hw);
					}
					set_aux_data(hw,
						hw->last_dec_picture,
						0, 2, NULL);
				}
			}
#else
			if (hw->last_dec_picture)
				set_aux_data(hw,
					hw->last_dec_picture, 0, 0, NULL);
#endif
		}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		hw->switch_dvlayer_flag = 0;
		hw->got_valid_nal = 1;
#endif
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s H264_AUX_DATA_READY\n", __func__);
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_EDN);
		hw->dec_result = DEC_RESULT_DONE;
		vdec_schedule_work(&hw->work);
	} else if (/*(dec_dpb_status == H264_DATA_REQUEST) ||*/
			(dec_dpb_status == H264_SEARCH_BUFEMPTY) ||
			(dec_dpb_status == H264_DECODE_BUFEMPTY) ||
			(dec_dpb_status == H264_DECODE_TIMEOUT)) {
empty_proc:
		reset_process_time(hw);
		if ((hw->error_proc_policy & 0x40000) &&
			((dec_dpb_status == H264_DECODE_TIMEOUT) ||
			(!hw->frmbase_cont_flag &&
			(dec_dpb_status == H264_SEARCH_BUFEMPTY || dec_dpb_status == H264_DECODE_BUFEMPTY)
			&& input_frame_based(vdec)))) {
			WRITE_VREG(ASSIST_MBOX1_MASK, 0);
			goto pic_done_proc;
		}
		if (!hw->frmbase_cont_flag)
			release_cur_decoding_buf(hw);

		amvdec_stop();
		if (input_frame_based(vdec) ||
			(READ_VREG(VLD_MEM_VIFIFO_LEVEL) > 0x200)) {
			if (h264_debug_flag &
				DISABLE_ERROR_HANDLE) {
				dpb_print(DECODE_ID(hw),
				PRINT_FLAG_ERROR,
					"%s decoding error, level 0x%x\n",
					__func__,
					READ_VREG(VLD_MEM_VIFIFO_LEVEL));
				goto send_again;
			}
			vdec->mc_loaded = 0;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
				"%s %s\n", __func__,
				(dec_dpb_status == H264_SEARCH_BUFEMPTY) ?
				"H264_SEARCH_BUFEMPTY" :
				(dec_dpb_status == H264_DECODE_BUFEMPTY) ?
				"H264_DECODE_BUFEMPTY" :
				(dec_dpb_status == H264_DECODE_TIMEOUT) ?
				"H264_DECODE_TIMEOUT" :
				"OTHER");
			hw->dec_result = DEC_RESULT_DONE;

			if (dec_dpb_status == H264_SEARCH_BUFEMPTY)
				hw->search_dataempty_num++;
			else if (dec_dpb_status == H264_DECODE_TIMEOUT) {
				hw->decode_timeout_num++;
				if (hw->error_proc_policy & 0x4000) {
					hw->data_flag |= ERROR_FLAG;
					if ((p_H264_Dpb->last_dpb_status == H264_DECODE_TIMEOUT) ||
						(p_H264_Dpb->last_dpb_status == H264_PIC_DATA_DONE) ||
						((p_H264_Dpb->last_dpb_status == H264_SLICE_HEAD_DONE) &&
						 (p_H264_Dpb->mSlice.slice_type != B_SLICE))) {
						 dpb_print(DECODE_ID(hw),
							PRINT_FLAG_ERROR, "%s last dpb status 0x%x need bugmgr reset \n",
							p_H264_Dpb->last_dpb_status, __func__);
							hw->reset_bufmgr_flag = 1;
					}
				}
			} else if (dec_dpb_status == H264_DECODE_BUFEMPTY)
				hw->decode_dataempty_num++;
			if (!hw->frmbase_cont_flag)
				hw->data_flag |= ERROR_FLAG;

			vdec_schedule_work(&hw->work);
		} else {
			/* WRITE_VREG(DPB_STATUS_REG, H264_ACTION_INIT); */
#ifdef DETECT_WRONG_MULTI_SLICE
			if (hw->error_proc_policy & 0x10000) {
				p_H264_Dpb->mVideo.pre_frame_num = hw->first_pre_frame_num;
			}
			hw->last_picture_slice_count = hw->cur_picture_slice_count;
#endif
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_AGAIN\n", __func__);
send_again:
			hw->dec_result = DEC_RESULT_AGAIN;
			vdec_schedule_work(&hw->work);
		}
	} else if (dec_dpb_status == H264_DATA_REQUEST) {
		reset_process_time(hw);
		if (input_frame_based(vdec)) {
			dpb_print(DECODE_ID(hw),
			PRINT_FLAG_VDEC_STATUS,
			"%s H264_DATA_REQUEST (%d)\n",
			__func__, hw->get_data_count);
			hw->dec_result = DEC_RESULT_GET_DATA;
			hw->reg_iqidct_control = READ_VREG(IQIDCT_CONTROL);
			hw->reg_iqidct_control_init_flag = 1;
			hw->get_data_start_time = jiffies;
			hw->get_data_count++;
			if (hw->get_data_count >= frame_max_data_packet)
				goto empty_proc;
			vdec_schedule_work(&hw->work);
		} else
			goto empty_proc;
	} else if (dec_dpb_status == H264_DECODE_OVER_SIZE) {
		dpb_print(DECODE_ID(hw), 0,
			"vmh264 decode oversize !!\n");
		release_cur_decoding_buf(hw);
		hw->data_flag |= ERROR_FLAG;
		hw->stat |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
		reset_process_time(hw);
		hw->dec_result = DEC_RESULT_DONE;
		vdec_schedule_work(&hw->work);
		return IRQ_HANDLED;
	} else if (dec_dpb_status == H264_SEI_DATA_READY) {
		int aux_data_len;
		aux_data_len =
			(READ_VREG(H264_AUX_DATA_SIZE) >> 16) << 4;

		if (aux_data_len > SEI_DATA_SIZE) {
				dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
					"sei data size more than 4K: %d, discarded it\n",
					hw->sei_itu_data_len);
				hw->sei_itu_data_len = 0;
		}

		if (aux_data_len != 0) {
			u8 *trans_data_buf;
			u8 *sei_data_buf;
			u8 swap_byte;

#if 0
			dump_aux_buf(hw);
#endif
			trans_data_buf = (u8 *)hw->aux_addr;

			if (trans_data_buf[7] == AUX_TAG_SEI) {
				int left_len;

				sei_data_buf = (u8 *)hw->sei_data_buf
							+ hw->sei_data_len;
				left_len = SEI_DATA_SIZE - hw->sei_data_len;
				if (aux_data_len/2 <= left_len) {
					for (i = 0; i < aux_data_len/2; i++)
						sei_data_buf[i]
							= trans_data_buf[i*2];

					aux_data_len = aux_data_len / 2;
					for (i = 0; i < aux_data_len; i = i+4) {
						swap_byte = sei_data_buf[i];
						sei_data_buf[i]
							= sei_data_buf[i+3];
						sei_data_buf[i+3] = swap_byte;

						swap_byte = sei_data_buf[i+1];
						sei_data_buf[i+1]
							= sei_data_buf[i+2];
						sei_data_buf[i+2] = swap_byte;
					}

					for (i = aux_data_len-1; i >= 0; i--)
						if (sei_data_buf[i] != 0)
							break;

					hw->sei_data_len += i+1;
				} else
					dpb_print(DECODE_ID(hw),
						PRINT_FLAG_ERROR,
						"sei data size %d and more than left space: %d, discarded it\n",
						hw->sei_itu_data_len,
						left_len);
			}
		}
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_THREAD_EDN);
		WRITE_VREG(DPB_STATUS_REG, H264_SEI_DATA_DONE);

		return IRQ_HANDLED;
	}


	/* ucode debug */
	debug_tag = READ_VREG(DEBUG_REG1);
	if (debug_tag & 0x10000) {
		unsigned short *p = (unsigned short *)hw->lmem_addr;

		dpb_print(DECODE_ID(hw), 0,
			"LMEM<tag %x>:\n", debug_tag);
		for (i = 0; i < 0x400; i += 4) {
			int ii;
			if ((i & 0xf) == 0)
				dpb_print_cont(DECODE_ID(hw), 0,
					"%03x: ", i);
			for (ii = 0; ii < 4; ii++)
				dpb_print_cont(DECODE_ID(hw), 0,
					"%04x ", p[i+3-ii]);
			if (((i+ii) & 0xf) == 0)
				dpb_print_cont(DECODE_ID(hw), 0,
					"\n");
		}
		if (((udebug_pause_pos & 0xffff)
			== (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx ==
			hw->decode_pic_count) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_VREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			hw->ucode_pause_pos = udebug_pause_pos;
		}
		else if (debug_tag & 0x20000)
			hw->ucode_pause_pos = 0xffffffff;
		if (hw->ucode_pause_pos)
			reset_process_time(hw);
		else
			WRITE_VREG(DEBUG_REG1, 0);
	} else if (debug_tag != 0) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_UCODE_EVT,
			"dbg%x: %x\n", debug_tag,
			READ_VREG(DEBUG_REG2));
		if (((udebug_pause_pos & 0xffff)
			== (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx ==
			hw->decode_pic_count) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_VREG(DEBUG_REG2))) {
			udebug_pause_pos &= 0xffff;
			hw->ucode_pause_pos = udebug_pause_pos;
		}
		if (hw->ucode_pause_pos)
			reset_process_time(hw);
		else
			WRITE_VREG(DEBUG_REG1, 0);
	}
	/**/
	return IRQ_HANDLED;
}

static irqreturn_t vh264_isr(struct vdec_s *vdec, int irq)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;


	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	if (!hw)
		return IRQ_HANDLED;

	if (hw->eos)
		return IRQ_HANDLED;

	p_H264_Dpb->vdec = vdec;
	p_H264_Dpb->dec_dpb_status = READ_VREG(DPB_STATUS_REG);
	if (p_H264_Dpb->dec_dpb_status == H264_SLICE_HEAD_DONE ||
		p_H264_Dpb->dec_dpb_status == H264_CONFIG_REQUEST) {
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_HEAD_DONE);
	}
	else if (p_H264_Dpb->dec_dpb_status == H264_PIC_DATA_DONE) {
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_PIC_DONE);
	}
	else if (p_H264_Dpb->dec_dpb_status == H264_SEI_DATA_READY)
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_SEI_DONE);
	else if (p_H264_Dpb->dec_dpb_status == H264_AUX_DATA_READY)
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_AUX_DONE);

	dpb_print(DECODE_ID(hw), PRINT_FLAG_UCODE_EVT,
			"%s DPB_STATUS_REG: 0x%x, run(%d) last_state (%x) ERROR_STATUS_REG 0x%x, sb (0x%x 0x%x 0x%x) bitcnt 0x%x mby_mbx 0x%x\n",
			__func__,
			p_H264_Dpb->dec_dpb_status,
			run_count[DECODE_ID(hw)],
			hw->dec_result,
			READ_VREG(ERROR_STATUS_REG),
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			READ_VREG(VIFF_BIT_CNT),
			READ_VREG(MBY_MBX));

	if (p_H264_Dpb->dec_dpb_status == H264_WRRSP_REQUEST) {
		if (hw->mmu_enable)
			hevc_sao_wait_done(hw);
		WRITE_VREG(DPB_STATUS_REG, H264_WRRSP_DONE);
		return IRQ_HANDLED;
	}
	ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_ISR_END);
	return IRQ_WAKE_THREAD;

}

static void timeout_process(struct vdec_h264_hw_s *hw)
{
	struct vdec_s *vdec = hw_to_vdec(hw);

	/*
	 * In this very timeout point,the vh264_work arrives,
	 * or in some cases the system become slow,  then come
	 * this second timeout. In both cases we return.
	 */
	if (work_pending(&hw->work) ||
	    work_busy(&hw->work) ||
	    work_busy(&hw->timeout_work) ||
	    work_pending(&hw->timeout_work)) {
		pr_err("%s h264[%d] work pending, do nothing.\n",__func__, vdec->id);
		return;
	}
	hw->timeout_num++;
	amvdec_stop();
	vdec->mc_loaded = 0;
	if (hw->mmu_enable) {
		hevc_set_frame_done(hw);
		hevc_sao_wait_done(hw);
	}
	dpb_print(DECODE_ID(hw),
		PRINT_FLAG_ERROR, "%s decoder timeout, DPB_STATUS_REG 0x%x\n", __func__, READ_VREG(DPB_STATUS_REG));

	hw->dec_result = DEC_RESULT_TIMEOUT;
	hw->data_flag |= ERROR_FLAG;
	if (hw->error_proc_policy & 0x100000) {
		if (hw->ip_field_error_count > 0)
			hw->ip_field_error_count = 0;
	}

	if (work_pending(&hw->work))
		return;
	vdec_schedule_work(&hw->timeout_work);
}

static void dump_bufspec(struct vdec_h264_hw_s *hw,
	const char *caller)
{
	int i;
	dpb_print(DECODE_ID(hw), 0,
		"%s in %s:\n", __func__, caller);
	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		if (hw->buffer_spec[i].used == -1)
			continue;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DUMP_BUFSPEC,
			"bufspec (%d): used %d adr 0x%x(%lx) canvas(%d) vf_ref(%d) ",
			i, hw->buffer_spec[i].used,
			hw->buffer_spec[i].buf_adr,
			hw->buffer_spec[i].cma_alloc_addr,
			hw->buffer_spec[i].canvas_pos,
			hw->buffer_spec[i].vf_ref
			);
#ifdef CONFIG_AM_VDEC_DV
		dpb_print_cont(DECODE_ID(hw), 0,
			"dv_el_exist %d",
			hw->buffer_spec[i].dv_enhance_exist
		);
#endif
		dpb_print_cont(DECODE_ID(hw), 0, "\n");
	}

}

static void vmh264_dump_state(struct vdec_s *vdec)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	dpb_print(DECODE_ID(hw), 0,
		"====== %s\n", __func__);
	dpb_print(DECODE_ID(hw), 0,
		"width/height (%d/%d), num_reorder_frames %d dec_dpb_size %d dpb size(bufspec count) %d max_reference_size(collocate count) %d i_only %d video_signal_type 0x%x send_err %d \n",
		hw->frame_width,
		hw->frame_height,
		hw->num_reorder_frames,
		hw->dpb.dec_dpb_size,
		hw->dpb.mDPB.size,
		hw->max_reference_size,
		hw->i_only,
		hw->video_signal_type,
		hw->send_error_frame_flag
		);

	dpb_print(DECODE_ID(hw), 0,
		"is_framebase(%d), eos %d, state 0x%x, dec_result 0x%x dec_frm %d disp_frm %d run %d not_run_ready %d input_empty %d bufmgr_reset_cnt %d error_frame_count = %d, drop_frame_count = %d\n",
		input_frame_based(vdec),
		hw->eos,
		hw->stat,
		hw->dec_result,
		decode_frame_count[DECODE_ID(hw)],
		display_frame_count[DECODE_ID(hw)],
		run_count[DECODE_ID(hw)],
		not_run_ready[DECODE_ID(hw)],
		input_empty[DECODE_ID(hw)],
		hw->reset_bufmgr_count,
		hw->gvs.error_frame_count,
		hw->gvs.drop_frame_count
		);

#ifdef DETECT_WRONG_MULTI_SLICE
		dpb_print(DECODE_ID(hw), 0,
		"MULTI_SLICE_DETECT (check_count %d slice_count %d cur_slice_count %d flag %d)\n",
		hw->multi_slice_pic_check_count,
		hw->picture_slice_count,
		hw->cur_picture_slice_count,
		hw->multi_slice_pic_flag);
#endif
	if (!hw->is_used_v4l && vf_get_receiver(vdec->vf_provider_name)) {
		enum receviver_start_e state =
		vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_QUREY_STATE,
			NULL);
		dpb_print(DECODE_ID(hw), 0,
			"\nreceiver(%s) state %d\n",
			vdec->vf_provider_name,
			state);
	}

	dpb_print(DECODE_ID(hw), 0,
	"%s, newq(%d/%d), dispq(%d/%d) vf prepare/get/put (%d/%d/%d), free_spec(%d), initdon(%d), used_size(%d/%d), unused_fr_dpb(%d)  fast_output_enable %x \n",
	__func__,
	kfifo_len(&hw->newframe_q),
	VF_POOL_SIZE,
	kfifo_len(&hw->display_q),
	VF_POOL_SIZE,
	hw->vf_pre_count,
	hw->vf_get_count,
	hw->vf_put_count,
	have_free_buf_spec(vdec),
	p_H264_Dpb->mDPB.init_done,
	p_H264_Dpb->mDPB.used_size, p_H264_Dpb->mDPB.size,
	is_there_unused_frame_from_dpb(&p_H264_Dpb->mDPB),
	p_H264_Dpb->fast_output_enable
	);

	dump_dpb(&p_H264_Dpb->mDPB, 1);
	dump_pic(p_H264_Dpb);
	dump_bufspec(hw, __func__);

	dpb_print(DECODE_ID(hw), 0,
		"DPB_STATUS_REG=0x%x\n",
		READ_VREG(DPB_STATUS_REG));
	dpb_print(DECODE_ID(hw), 0,
		"MPC_E=0x%x\n",
		READ_VREG(MPC_E));
	dpb_print(DECODE_ID(hw), 0,
		"H264_DECODE_MODE=0x%x\n",
		READ_VREG(H264_DECODE_MODE));
	dpb_print(DECODE_ID(hw), 0,
		"MBY_MBX=0x%x\n",
		READ_VREG(MBY_MBX));
	dpb_print(DECODE_ID(hw), 0,
		"H264_DECODE_SIZE=0x%x\n",
		READ_VREG(H264_DECODE_SIZE));
	dpb_print(DECODE_ID(hw), 0,
		"VIFF_BIT_CNT=0x%x\n",
		READ_VREG(VIFF_BIT_CNT));
	dpb_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_LEVEL=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_LEVEL));
	dpb_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_WP=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_WP));
	dpb_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_RP=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_RP));
	dpb_print(DECODE_ID(hw), 0,
		"PARSER_VIDEO_RP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_rp));
	dpb_print(DECODE_ID(hw), 0,
		"PARSER_VIDEO_WP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_wp));

	if (input_frame_based(vdec) &&
		dpb_is_debug(DECODE_ID(hw),
		PRINT_FRAMEBASE_DATA)
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

			dpb_print(DECODE_ID(hw), 0,
				"frame data size 0x%x\n",
				hw->chunk->size);
			for (jj = 0; jj < hw->chunk->size; jj++) {
				if ((jj & 0xf) == 0)
					dpb_print(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA,
						"%06x:", jj);
				dpb_print_cont(DECODE_ID(hw),
				PRINT_FRAMEBASE_DATA,
					"%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					dpb_print_cont(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA,
						"\n");
			}

			if (!hw->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		}
	}
}


static void check_timer_func(struct timer_list *timer)
{
	struct vdec_h264_hw_s *hw = container_of(timer,
		struct vdec_h264_hw_s, check_timer);
	struct vdec_s *vdec = hw_to_vdec(hw);
	int error_skip_frame_count = error_skip_count & 0xfff;
	unsigned int timeout_val = decode_timeout_val;
	if (timeout_val != 0 &&
		hw->no_error_count < error_skip_frame_count)
		timeout_val = errordata_timeout_val;
	if ((h264_debug_cmd & 0x100) != 0 &&
		DECODE_ID(hw) == (h264_debug_cmd & 0xff)) {
		hw->dec_result = DEC_RESULT_DONE;
		vdec_schedule_work(&hw->work);
		pr_info("vdec %d is forced to be disconnected\n",
			h264_debug_cmd & 0xff);
		h264_debug_cmd = 0;
		return;
	}
	if ((h264_debug_cmd & 0x200) != 0 &&
		DECODE_ID(hw) == (h264_debug_cmd & 0xff)) {
		pr_debug("vdec %d is forced to reset bufmgr\n",
			h264_debug_cmd & 0xff);
		hw->reset_bufmgr_flag = 1;
		h264_debug_cmd = 0;
		return;
	}

	if (vdec->next_status == VDEC_STATUS_DISCONNECTED &&
			!hw->is_used_v4l) {
		hw->dec_result = DEC_RESULT_FORCE_EXIT;
		WRITE_VREG(ASSIST_MBOX1_MASK, 0);
		vdec_schedule_work(&hw->work);
		pr_debug("vdec requested to be disconnected\n");
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

	if (((h264_debug_flag & DISABLE_ERROR_HANDLE) == 0) &&
		(timeout_val > 0) &&
		(hw->start_process_time > 0) &&
		((1000 * (jiffies - hw->start_process_time) / HZ)
			> timeout_val)
	) {
		u32 dpb_status = READ_VREG(DPB_STATUS_REG);
		u32 mby_mbx = READ_VREG(MBY_MBX);
		if ((dpb_status == H264_ACTION_DECODE_NEWPIC) ||
			(dpb_status == H264_ACTION_DECODE_SLICE) ||
			(dpb_status == H264_SEI_DATA_DONE) ||
			(dpb_status == H264_STATE_SEARCH_HEAD) ||
			(dpb_status == H264_SLICE_HEAD_DONE) ||
			(dpb_status == H264_SEI_DATA_READY)) {
			if (h264_debug_flag & DEBUG_TIMEOUT_DEC_STAT)
				pr_debug("%s dpb_status = 0x%x last_mby_mbx = %u mby_mbx = %u\n",
				__func__, dpb_status, hw->last_mby_mbx, mby_mbx);

			if (hw->last_mby_mbx == mby_mbx) {
				if (hw->decode_timeout_count > 0)
					hw->decode_timeout_count--;
				if (hw->decode_timeout_count == 0)
				{
					reset_process_time(hw);
					timeout_process(hw);
				}
			} else
				start_process_time(hw);
		} else if (is_in_parsing_state(dpb_status)) {
			if (hw->last_vld_level ==
				READ_VREG(VLD_MEM_VIFIFO_LEVEL)) {
				if (hw->decode_timeout_count > 0)
					hw->decode_timeout_count--;
				if (hw->decode_timeout_count == 0)
				{
					reset_process_time(hw);
					timeout_process(hw);
				}
			}
		}
		hw->last_vld_level =
			READ_VREG(VLD_MEM_VIFIFO_LEVEL);
		hw->last_mby_mbx = mby_mbx;
	}

	if ((hw->ucode_pause_pos != 0) &&
		(hw->ucode_pause_pos != 0xffffffff) &&
		udebug_pause_pos != hw->ucode_pause_pos) {
		hw->ucode_pause_pos = 0;
		WRITE_VREG(DEBUG_REG1, 0);
	}

	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
}

static int dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	u32 ar, ar_tmp;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;

	if (!hw)
		return -1;

	vstatus->frame_width = hw->frame_width;
	vstatus->frame_height = hw->frame_height;
	if (hw->error_frame_width &&
		hw->error_frame_height) {
		vstatus->frame_width = hw->error_frame_width;
		vstatus->frame_height = hw->error_frame_height;
	}
	if (hw->frame_dur != 0) {
		vstatus->frame_dur = hw->frame_dur;
		vstatus->frame_rate = ((96000 * 10 / hw->frame_dur) % 10) < 5 ?
		                    96000 / hw->frame_dur : (96000 / hw->frame_dur +1);
	}
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = hw->gvs.error_frame_count;
	vstatus->status = hw->stat;
	if (hw->h264_ar == 0x3ff)
		ar_tmp = (0x100 *
			hw->frame_height * hw->height_aspect_ratio) /
			(hw->frame_width * hw->width_aspect_ratio);
	else
		ar_tmp = hw->h264_ar;
	ar = min_t(u32,
			ar_tmp,
			DISP_RATIO_ASPECT_RATIO_MAX);
	vstatus->ratio_control =
		ar << DISP_RATIO_ASPECT_RATIO_BIT;

	vstatus->error_frame_count = hw->gvs.error_frame_count;
	vstatus->drop_frame_count = hw->gvs.drop_frame_count;
	vstatus->frame_count = decode_frame_count[DECODE_ID(hw)];
	vstatus->i_decoded_frames = hw->gvs.i_decoded_frames;
	vstatus->i_lost_frames = hw->gvs.i_lost_frames;
	vstatus->i_concealed_frames = hw->gvs.i_concealed_frames;
	vstatus->p_decoded_frames = hw->gvs.p_decoded_frames;
	vstatus->p_lost_frames = hw->gvs.p_lost_frames;
	vstatus->p_concealed_frames = hw->gvs.p_concealed_frames;
	vstatus->b_decoded_frames = hw->gvs.b_decoded_frames;
	vstatus->b_lost_frames = hw->gvs.b_lost_frames;
	vstatus->b_concealed_frames = hw->gvs.b_concealed_frames;
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
		"%s-%02d", DRIVER_NAME, hw->id);

	return 0;
}

static int vh264_hw_ctx_restore(struct vdec_h264_hw_s *hw)
{
	int i, j;
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;

	hw->frmbase_cont_flag = 0;
	/* if (hw->init_flag == 0) { */
	if (h264_debug_flag & 0x40000000) {
		/* if (1) */
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
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
		WRITE_RESET_REG(RESET0_REGISTER,
			RESET_IQIDCT | RESET_MC | RESET_VLD_PART);
		READ_RESET_REG(RESET0_REGISTER);
			WRITE_RESET_REG(RESET0_REGISTER,
			RESET_IQIDCT | RESET_MC | RESET_VLD_PART);

		WRITE_RESET_REG(RESET2_REGISTER, RESET_PIC_DC | RESET_DBLK);
#endif
		WRITE_VREG(POWER_CTL_VLD,
			READ_VREG(POWER_CTL_VLD) | (0 << 10) |
				(1 << 9) | (1 << 6));
	} else {
		/* WRITE_VREG(POWER_CTL_VLD,
		 *	READ_VREG(POWER_CTL_VLD) | (0 << 10) | (1 << 9) );
		 */
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

	/* cbcr_merge_swap_en */
	if (hw->is_used_v4l
		&& (v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21
		|| v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21M))
		SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);
	else
		CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);

	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 0xbf << 24);
	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 0xbf << 24);

	CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 31);
	if (hw->mmu_enable) {
		SET_VREG_MASK(MDEC_PIC_DC_MUX_CTRL, 1<<31);
		/* sw reset to extif hardware */
		SET_VREG_MASK(MDEC_EXTIF_CFG1, 1<<30);
		CLEAR_VREG_MASK(MDEC_EXTIF_CFG1, 1<<30);
	} else {
		CLEAR_VREG_MASK(MDEC_PIC_DC_MUX_CTRL, 1 << 31);
		WRITE_VREG(MDEC_EXTIF_CFG1, 0);
	}


#if 1 /* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	/* pr_info("vh264 meson8 prot init\n"); */
	WRITE_VREG(MDEC_PIC_DC_THRESH, 0x404038aa);
#endif

#ifdef VDEC_DW
	if (get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_T7) {
		if (IS_VDEC_DW(hw)) {
			u32 data = ((1   << 30) |(1   <<  0) |(1   <<  8));

			if (IS_VDEC_DW(hw) == 2)
				data |= (1   <<  9);
			WRITE_VREG(MDEC_DOUBLEW_CFG0, data); /* Double Write Enable*/
		}
	}
#endif
	if (hw->dpb.mDPB.size > 0) {
		WRITE_VREG(AV_SCRATCH_7, (hw->max_reference_size << 24) |
			(hw->dpb.mDPB.size << 16) |
			(hw->dpb.mDPB.size << 8));

		for (j = 0; j < hw->dpb.mDPB.size; j++) {
			i = get_buf_spec_by_canvas_pos(hw, j);
			if (i < 0)
				break;

			if (!hw->mmu_enable &&
				hw->buffer_spec[i].cma_alloc_addr)
				config_decode_canvas(hw, i);
			if (hw->mmu_enable && hw->double_write_mode)
				config_decode_canvas_ex(hw, i);
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
	if (!tee_enabled())
		WRITE_VREG(AV_SCRATCH_G, hw->mc_dma_handle);

	/* hw->error_recovery_mode = (error_recovery_mode != 0) ?
	 *	error_recovery_mode : error_recovery_mode_in;
	 */
	/* WRITE_VREG(AV_SCRATCH_F,
	 *	(READ_VREG(AV_SCRATCH_F) & 0xffffffc3) );
	 */
	WRITE_VREG(AV_SCRATCH_F, (hw->save_reg_f & 0xffffffc3) |
		((error_recovery_mode_in & 0x1) << 4));
	/*if (hw->ucode_type == UCODE_IP_ONLY_PARAM)
		SET_VREG_MASK(AV_SCRATCH_F, 1 << 6);
	else*/
		CLEAR_VREG_MASK(AV_SCRATCH_F, 1 << 6);

	WRITE_VREG(LMEM_DUMP_ADR, (u32)hw->lmem_phy_addr);
#if 1 /* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	WRITE_VREG(MDEC_PIC_DC_THRESH, 0x404038aa);
#endif

	WRITE_VREG(DEBUG_REG1, 0);
	WRITE_VREG(DEBUG_REG2, 0);

	/*Because CSD data is not found at playback start,
	  the IQIDCT_CONTROL register is not saved,
	  the initialized value 0x200 of IQIDCT_CONTROL is set*/
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXL) {
		if (hw->init_flag && (hw->reg_iqidct_control_init_flag == 0))
			WRITE_VREG(IQIDCT_CONTROL, 0x200);
	}

	if (hw->reg_iqidct_control)
		WRITE_VREG(IQIDCT_CONTROL, hw->reg_iqidct_control);
	dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
		"IQIDCT_CONTROL = 0x%x\n", READ_VREG(IQIDCT_CONTROL));

	if (hw->reg_vcop_ctrl_reg)
		WRITE_VREG(VCOP_CTRL_REG, hw->reg_vcop_ctrl_reg);
	if (hw->vld_dec_control)
		WRITE_VREG(VLD_DECODE_CONTROL, hw->vld_dec_control);
	return 0;
}

static int vmh264_set_trickmode(struct vdec_s *vdec, unsigned long trickmode)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)vdec->private;
	if (i_only_flag & 0x100)
		return 0;
	if (trickmode == TRICKMODE_I)
		hw->i_only = 0x3;
	else if (trickmode == TRICKMODE_NONE)
		hw->i_only = 0x0;
	return 0;
}

static unsigned char amvdec_enable_flag;
static void vh264_local_init(struct vdec_h264_hw_s *hw, bool is_reset)
{
	int i;
	hw->init_flag = 0;
	hw->first_sc_checked= 0;
	hw->eos = 0;
	hw->valve_count = 0;
	hw->config_bufmgr_done = 0;
	hw->start_process_time = 0;
	hw->has_i_frame = 0;
	hw->no_error_count = 0xfff;
	hw->no_error_i_count = 0xf;

	hw->dec_flag = 0;
	hw->data_flag = 0;
	hw->skip_frame_count = 0;
	hw->reg_iqidct_control = 0;
	hw->reg_iqidct_control_init_flag = 0;
	hw->reg_vcop_ctrl_reg = 0;
	hw->reg_rv_ai_mb_count = 0;
	hw->vld_dec_control = 0;
	hw->decode_timeout_count = 0;
	hw->no_mem_count = 0;
	hw->dec_again_cnt = 0;
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
		 *give default 30fps
		 */
		hw->frame_dur = 96000/30;
	}

	hw->unstable_pts = (((unsigned long) hw->vh264_amstream_dec_info.param & 0x40) >> 6);

	hw->first_i_policy = first_i_policy;

	pr_info("H264 sysinfo: %dx%d duration=%d, pts_outside=%d\n",
		hw->frame_width, hw->frame_height, hw->frame_dur, hw->pts_outside);
	pr_debug("sync_outside=%d, use_idr_framerate=%d, is_used_v4l: %d\n",
		hw->sync_outside, hw->use_idr_framerate, hw->is_used_v4l);

	if (i_only_flag & 0x100)
		hw->i_only = i_only_flag & 0xff;
	if (hw->i_only)
		hw->dpb.first_insert_frame = FirstInsertFrm_SKIPDONE;

	if ((unsigned long) hw->vh264_amstream_dec_info.param
		& 0x08)
		hw->no_poc_reorder_flag = 1;

	error_recovery_mode_in = 1; /*ucode control?*/
	if (hw->error_proc_policy & 0x80000000)
		hw->send_error_frame_flag = hw->error_proc_policy & 0x1;
	else if ((unsigned long) hw->vh264_amstream_dec_info.param & 0x20)
		hw->send_error_frame_flag = 0; /*Don't display mark err frames*/

	if (!is_reset) {
		INIT_KFIFO(hw->display_q);
		INIT_KFIFO(hw->newframe_q);

		for (i = 0; i < VF_POOL_SIZE; i++) {
			const struct vframe_s *vf = &(hw->vfpool[hw->cur_pool][i]);
			hw->vfpool[hw->cur_pool][i].index = -1; /* VF_BUF_NUM; */
			hw->vfpool[hw->cur_pool][i].bufWidth = 1920;
			kfifo_put(&hw->newframe_q, vf);
		}
	}

	hw->duration_from_pts_done = 0;

	hw->p_last_vf = NULL;
	hw->vh264_stream_switching_state = SWITCHING_STATE_OFF;
	hw->hevc_cur_buf_idx = 0xffff;

	init_waitqueue_head(&hw->wait_q);

	return;
}

static s32 vh264_init(struct vdec_h264_hw_s *hw)
{
	int size = -1;
	int fw_size = 0x1000 * 16;
	int fw_mmu_size = 0x1000 * 16;
	struct firmware_s *fw = NULL, *fw_mmu = NULL;

	/* int trickmode_fffb = 0; */

	/* pr_info("\nvh264_init\n"); */
	/* init_timer(&hw->recycle_timer); */

	/* timer init */
	timer_setup(&hw->check_timer, check_timer_func, 0);
	hw->check_timer.expires = jiffies + CHECK_INTERVAL;

	/* add_timer(&hw->check_timer); */
	hw->stat |= STAT_TIMER_ARM;
	hw->stat |= STAT_ISR_REG;

	mutex_init(&hw->chunks_mutex);
	vh264_local_init(hw, false);
	INIT_WORK(&hw->work, vh264_work);
	INIT_WORK(&hw->notify_work, vh264_notify_work);
	INIT_WORK(&hw->timeout_work, vh264_timeout_work);
#ifdef MH264_USERDATA_ENABLE
	INIT_WORK(&hw->user_data_ready_work, user_data_ready_notify_work);
#endif

	/*if (!amvdec_enable_flag) {
		amvdec_enable_flag = true;
		amvdec_enable();
		if (hw->mmu_enable)
			amhevc_enable();
	}*/
	if (hw->mmu_enable) {

		hw->frame_mmu_map_addr =
				decoder_dma_alloc_coherent(&hw->frame_mmu_map_handle,
				FRAME_MMU_MAP_SIZE,
				&hw->frame_mmu_map_phy_addr, "H264_MMU_BUF");
		if (hw->frame_mmu_map_addr == NULL) {
			pr_err("%s: failed to alloc count_buffer\n", __func__);
			return -ENOMEM;
		}
	}

	fw = vmalloc(sizeof(struct firmware_s) + fw_size);
	if (IS_ERR_OR_NULL(fw))
		return -ENOMEM;

	size = get_firmware_data(VIDEO_DEC_H264_MULTI, fw->data);
	if (size < 0) {
		pr_err("get firmware fail.\n");
		vfree(fw);
		return -1;
	}

	fw->len = size;
	hw->fw = fw;

	if (hw->mmu_enable) {
		fw_mmu = vmalloc(sizeof(struct firmware_s) + fw_mmu_size);
		if (IS_ERR_OR_NULL(fw_mmu))
			return -ENOMEM;

		size = get_firmware_data(VIDEO_DEC_H264_MULTI_MMU, fw_mmu->data);
		if (size < 0) {
			pr_err("get mmu fw fail.\n");
			vfree(fw_mmu);
			return -1;
		}

		fw_mmu->len = size;
		hw->fw_mmu = fw_mmu;
	}

	if (!tee_enabled()) {
		/* -- ucode loading (amrisc and swap code) */
		hw->mc_cpu_addr =
			decoder_dma_alloc_coherent(&hw->mc_cpu_handle,
					MC_TOTAL_SIZE, &hw->mc_dma_handle, "H264_MMU_BUF");
		if (!hw->mc_cpu_addr) {
			amvdec_enable_flag = false;
			amvdec_disable();
			hw->vdec_pg_enable_flag = 0;
			if (hw->mmu_enable)
				amhevc_disable();
			pr_info("vh264_init: Can not allocate mc memory.\n");
			return -ENOMEM;
		}

		/*pr_info("264 ucode swap area: phyaddr %p, cpu vaddr %p\n",
			(void *)hw->mc_dma_handle, hw->mc_cpu_addr);
		*/

		/*ret = amvdec_loadmc_ex(VFORMAT_H264, NULL, buf);*/

		/*header*/
		memcpy((u8 *) hw->mc_cpu_addr + MC_OFFSET_HEADER,
			fw->data + 0x4000, MC_SWAP_SIZE);
		/*data*/
		memcpy((u8 *) hw->mc_cpu_addr + MC_OFFSET_DATA,
			fw->data + 0x2000, MC_SWAP_SIZE);
		/*mmco*/
		memcpy((u8 *) hw->mc_cpu_addr + MC_OFFSET_MMCO,
			fw->data + 0x6000, MC_SWAP_SIZE);
		/*list*/
		memcpy((u8 *) hw->mc_cpu_addr + MC_OFFSET_LIST,
			fw->data + 0x3000, MC_SWAP_SIZE);
		/*slice*/
		memcpy((u8 *) hw->mc_cpu_addr + MC_OFFSET_SLICE,
			fw->data + 0x5000, MC_SWAP_SIZE);
		/*main*/
		memcpy((u8 *) hw->mc_cpu_addr + MC_OFFSET_MAIN,
			fw->data, 0x2000);
		/*data*/
		memcpy((u8 *) hw->mc_cpu_addr + MC_OFFSET_MAIN + 0x2000,
			fw->data + 0x2000, 0x1000);
		/*slice*/
		memcpy((u8 *) hw->mc_cpu_addr + MC_OFFSET_MAIN + 0x3000,
			fw->data + 0x5000, 0x1000);
	}

#if 1 /* #ifdef  BUFFER_MGR_IN_C */
	hw->lmem_addr = (dma_addr_t)decoder_dma_alloc_coherent(&hw->lmem_phy_handle,
			PAGE_SIZE, (dma_addr_t *)&hw->lmem_phy_addr, "H264_LMEM_BUF");

	if (hw->lmem_addr == 0) {
		pr_err("%s: failed to alloc lmem buffer\n", __func__);
		return -1;
	}
	pr_debug("%s, phy_addr=%lx vaddr=%p\n",
		__func__, hw->lmem_phy_addr, (void *)hw->lmem_addr);

	if (prefix_aux_buf_size > 0 ||
		suffix_aux_buf_size > 0) {
		u32 aux_buf_size;
		hw->prefix_aux_size = AUX_BUF_ALIGN(prefix_aux_buf_size);
		hw->suffix_aux_size = AUX_BUF_ALIGN(suffix_aux_buf_size);
		aux_buf_size = hw->prefix_aux_size + hw->suffix_aux_size;
		hw->aux_addr = decoder_dma_alloc_coherent(&hw->aux_mem_handle,
						  aux_buf_size, &hw->aux_phy_addr,
						  "H264_AUX_BUF");
		if (hw->aux_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}

		hw->sei_data_buf = kmalloc(SEI_DATA_SIZE, GFP_KERNEL);
		if (hw->sei_data_buf == NULL) {
			pr_err("%s: failed to alloc sei itu data buffer\n",
				__func__);
			return -1;
		}
		hw->sei_itu_data_buf = kmalloc(SEI_ITU_DATA_SIZE, GFP_KERNEL);
		if (hw->sei_itu_data_buf == NULL) {
			pr_err("%s: failed to alloc sei itu data buffer\n",
				__func__);
			decoder_dma_free_coherent(hw->aux_mem_handle,
				hw->prefix_aux_size + hw->suffix_aux_size, hw->aux_addr,
				hw->aux_phy_addr);
			hw->aux_addr = NULL;
			kfree(hw->sei_data_buf);
			hw->sei_data_buf = NULL;

			return -1;
		}

		if (NULL == hw->sei_user_data_buffer) {
			hw->sei_user_data_buffer = kmalloc(USER_DATA_SIZE,
								GFP_KERNEL);
			if (!hw->sei_user_data_buffer) {
				pr_info("%s: Can not allocate sei_data_buffer\n",
					   __func__);
				decoder_dma_free_coherent(hw->aux_mem_handle,
					hw->prefix_aux_size + hw->suffix_aux_size, hw->aux_addr,
					hw->aux_phy_addr);
				hw->aux_addr = NULL;
				kfree(hw->sei_data_buf);
				hw->sei_data_buf = NULL;
				kfree(hw->sei_itu_data_buf);
				hw->sei_itu_data_buf = NULL;

				return -1;
			}
			hw->sei_user_data_wp = 0;
		}
	}
/* BUFFER_MGR_IN_C */
#endif
	hw->stat |= STAT_MC_LOAD;

	/* add memory barrier */
	wmb();

	return 0;
}

static int vh264_stop(struct vdec_h264_hw_s *hw)
{
	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}
#ifdef VDEC_DW
	WRITE_VREG(MDEC_DOUBLEW_CFG0, 0);
	WRITE_VREG(MDEC_DOUBLEW_CFG1, 0);
#endif
#ifdef MH264_USERDATA_ENABLE
	cancel_work_sync(&hw->user_data_ready_work);
#endif
	cancel_work_sync(&hw->notify_work);
	cancel_work_sync(&hw->timeout_work);
	cancel_work_sync(&hw->work);

	if (hw->stat & STAT_MC_LOAD) {
		if (hw->mc_cpu_addr != NULL) {
			decoder_dma_free_coherent(hw->mc_cpu_handle,
					MC_TOTAL_SIZE, hw->mc_cpu_addr,
					hw->mc_dma_handle);
			hw->mc_cpu_addr = NULL;
		}
		if (hw->frame_mmu_map_addr != NULL) {
			decoder_dma_free_coherent(hw->frame_mmu_map_handle,
				FRAME_MMU_MAP_SIZE, hw->frame_mmu_map_addr,
					hw->frame_mmu_map_phy_addr);
			hw->frame_mmu_map_addr = NULL;
		}

	}
	if (hw->stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_1, (void *)hw);
		hw->stat &= ~STAT_ISR_REG;
	}
	if (hw->lmem_addr) {
		decoder_dma_free_coherent(hw->lmem_phy_handle,
			PAGE_SIZE, (void *)hw->lmem_addr,
			hw->lmem_phy_addr);
		hw->lmem_addr = 0;
	}

	if (hw->aux_addr) {
		decoder_dma_free_coherent(hw->aux_mem_handle,
			hw->prefix_aux_size + hw->suffix_aux_size, hw->aux_addr,
			hw->aux_phy_addr);
		hw->aux_addr = NULL;
	}
	if (hw->sei_data_buf != NULL) {
		kfree(hw->sei_data_buf);
		hw->sei_data_buf = NULL;
	}
	if (hw->sei_itu_data_buf != NULL) {
		kfree(hw->sei_itu_data_buf);
		hw->sei_itu_data_buf = NULL;
	}
	if (hw->sei_user_data_buffer != NULL) {
		kfree(hw->sei_user_data_buffer);
		hw->sei_user_data_buffer = NULL;
	}
	/* amvdec_disable(); */

	vfree(hw->fw);
	hw->fw = NULL;

	if (hw->mmu_enable) {
		vfree(hw->fw_mmu);
		hw->fw_mmu = NULL;
	}

	dpb_print(DECODE_ID(hw), 0,
		"%s\n",
		__func__);
	return 0;
}

static void wait_vmh264_search_done(struct vdec_h264_hw_s *hw)
{
	u32 vld_rp = READ_VREG(VLD_MEM_VIFIFO_RP);
	int count = 0;
	do {
		usleep_range(100, 500);
		if (vld_rp == READ_VREG(VLD_MEM_VIFIFO_RP))
			break;
		if (count > 2000) {
			dpb_print(DECODE_ID(hw),
			PRINT_FLAG_ERROR, "%s timeout count %d vld_rp 0x%x VLD_MEM_VIFIFO_RP 0x%x\n",
			 __func__, count, vld_rp, READ_VREG(VLD_MEM_VIFIFO_RP));
			break;
		} else
			vld_rp = READ_VREG(VLD_MEM_VIFIFO_RP);
		count++;
	} while (1);
}

static void vh264_notify_work(struct work_struct *work)
{
	struct vdec_h264_hw_s *hw = container_of(work,
					struct vdec_h264_hw_s, notify_work);
	struct vdec_s *vdec = hw_to_vdec(hw);

	if (hw->is_used_v4l)
		return;

	if (vdec->fr_hint_state == VDEC_NEED_HINT) {
		vf_notify_receiver(vdec->vf_provider_name,
				VFRAME_EVENT_PROVIDER_FR_HINT,
				(void *)((unsigned long)hw->frame_dur));
		vdec->fr_hint_state = VDEC_HINTED;
	}

	return;
}

#ifdef MH264_USERDATA_ENABLE
static void vmh264_reset_udr_mgr(struct vdec_h264_hw_s *hw)
{
	hw->wait_for_udr_send = 0;
	hw->sei_itu_data_len = 0;
	memset(&hw->ud_record, 0, sizeof(hw->ud_record));
}

static void vmh264_crate_userdata_manager(
						struct vdec_h264_hw_s *hw,
						u8 *userdata_buf,
						int buf_len)
{
	if (hw) {


		mutex_init(&hw->userdata_mutex);

		memset(&hw->userdata_info, 0,
			sizeof(struct mh264_userdata_info_t));
		hw->userdata_info.data_buf = userdata_buf;
		hw->userdata_info.buf_len = buf_len;
		hw->userdata_info.data_buf_end = userdata_buf + buf_len;

		vmh264_reset_udr_mgr(hw);

	}
}

static void vmh264_destroy_userdata_manager(struct vdec_h264_hw_s *hw)
{
	if (hw)
		memset(&hw->userdata_info,
				0,
				sizeof(struct mh264_userdata_info_t));
}

/*
#define DUMP_USERDATA_RECORD
*/
#ifdef DUMP_USERDATA_RECORD

#define MAX_USER_DATA_SIZE		3145728
static void *user_data_buf;
static unsigned char *pbuf_start;
static int total_len;
static int bskip;
static int n_userdata_id;

static void print_data(unsigned char *pdata,
						int len,
						unsigned int poc_number,
						unsigned int flag,
						unsigned int duration,
						unsigned int vpts,
						unsigned int vpts_valid,
						int rec_id)
{
	int nLeft;

	nLeft = len;
#if 0
	pr_info("%d len:%d, flag:%d, dur:%d, vpts:0x%x, valid:%d, poc:%d\n",
				rec_id,	len, flag,
				duration, vpts, vpts_valid, poc_number);
#endif
	pr_info("%d len = %d, flag = %d, vpts = 0x%x\n",
				rec_id,	len, flag, vpts);

	if (len == 96) {
		int i;
		nLeft = 72;
		while (nLeft >= 16) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				pdata[0], pdata[1], pdata[2], pdata[3],
				pdata[4], pdata[5], pdata[6], pdata[7],
				pdata[8], pdata[9], pdata[10], pdata[11],
				pdata[12], pdata[13], pdata[14], pdata[15]);
			nLeft -= 16;
			pdata += 16;
		}


		while (nLeft > 0) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
				pdata[0], pdata[1], pdata[2], pdata[3],
				pdata[4], pdata[5], pdata[6], pdata[7]);
			nLeft -= 8;
			pdata += 8;
		}

		i = 0;
		nLeft = 96-72;
		while (i < nLeft) {
			if (pdata[0] != 0) {
				pr_info("some data error\n");
				break;
			}
			pdata++;
			i++;
		}
	} else {
		while (nLeft >= 16) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				pdata[0], pdata[1], pdata[2], pdata[3],
				pdata[4], pdata[5], pdata[6], pdata[7],
				pdata[8], pdata[9], pdata[10], pdata[11],
				pdata[12], pdata[13], pdata[14], pdata[15]);
			nLeft -= 16;
			pdata += 16;
		}


		while (nLeft > 0) {
			pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
				pdata[0], pdata[1], pdata[2], pdata[3],
				pdata[4], pdata[5], pdata[6], pdata[7]);
			nLeft -= 8;
			pdata += 8;
		}

	}
}

static void push_to_buf(struct vdec_h264_hw_s *hw,
					u8 *pdata,
					int len,
					struct userdata_meta_info_t *pmeta);

static void dump_userdata_record(struct vdec_h264_hw_s *hw,
					struct mh264_userdata_record_t *record)
{
	if (record && hw) {
		u8 *pdata;

		pdata = hw->userdata_info.data_buf + record->rec_start;
/*
		print_data(pdata,
			record->rec_len,
			record->meta_info.flags,
			record->meta_info.duration,
			record->meta_info.vpts,
			record->meta_info.vpts_valid,
			n_record_id);
*/
		push_to_buf(hw, pdata, record->rec_len,  &record->meta_info);
		n_userdata_id++;
	}
}


static void push_to_buf(struct vdec_h264_hw_s *hw,
					u8 *pdata, int len,
					struct userdata_meta_info_t *pmeta)
{
	u32 *pLen;
	int info_cnt;
	u8 *pbuf_end;

	if (!user_data_buf)
		return;

	if (bskip) {
		pr_info("over size, skip\n");
		return;
	}
	info_cnt = 0;
	pLen = (u32 *)pbuf_start;

	*pLen = len;
	pbuf_start += sizeof(u32);
	info_cnt++;
	pLen++;

	*pLen = pmeta->poc_number;
	pbuf_start += sizeof(u32);
	info_cnt++;
	pLen++;

	*pLen = pmeta->duration;
	pbuf_start += sizeof(u32);
	info_cnt++;
	pLen++;

	*pLen = pmeta->flags;
	pbuf_start += sizeof(u32);
	info_cnt++;
	pLen++;

	*pLen = pmeta->vpts;
	pbuf_start += sizeof(u32);
	info_cnt++;
	pLen++;

	*pLen = pmeta->vpts_valid;
	pbuf_start += sizeof(u32);
	info_cnt++;
	pLen++;


	*pLen = n_userdata_id;
	pbuf_start += sizeof(u32);
	info_cnt++;
	pLen++;



	pbuf_end = (u8 *)hw->sei_user_data_buffer + USER_DATA_SIZE;
	if (pdata + len > pbuf_end) {
		int first_section_len;

		first_section_len = pbuf_end - pdata;
		memcpy(pbuf_start, pdata, first_section_len);
		pdata = (u8 *)hw->sei_user_data_buffer;
		pbuf_start += first_section_len;
		memcpy(pbuf_start, pdata, len - first_section_len);
		pbuf_start += len - first_section_len;
	} else {
		memcpy(pbuf_start, pdata, len);
		pbuf_start += len;
	}

	total_len += len + info_cnt * sizeof(u32);
	if (total_len >= MAX_USER_DATA_SIZE-4096)
		bskip = 1;
}

static void show_user_data_buf(void)
{
	u8 *pbuf;
	int len;
	unsigned int flag;
	unsigned int duration;
	unsigned int vpts;
	unsigned int vpts_valid;
	unsigned int poc_number;
	int rec_id;

	pr_info("show user data buf\n");
	pbuf = user_data_buf;

	while (pbuf < pbuf_start) {
		u32 *pLen;

		pLen = (u32 *)pbuf;

		len = *pLen;
		pLen++;
		pbuf += sizeof(u32);

		poc_number = *pLen;
		pLen++;
		pbuf += sizeof(u32);

		duration = *pLen;
		pLen++;
		pbuf += sizeof(u32);

		flag = *pLen;
		pLen++;
		pbuf += sizeof(u32);

		vpts = *pLen;
		pLen++;
		pbuf += sizeof(u32);

		vpts_valid = *pLen;
		pLen++;
		pbuf += sizeof(u32);

		rec_id = *pLen;
		pLen++;
		pbuf += sizeof(u32);

		print_data(pbuf, len, poc_number, flag,
			duration, vpts,
			vpts_valid, rec_id);
		pbuf += len;
		msleep(30);
	}
}

static int vmh264_init_userdata_dump(void)
{
	user_data_buf = kmalloc(MAX_USER_DATA_SIZE, GFP_KERNEL);
	if (user_data_buf)
		return 1;
	else
		return 0;
}

static void vmh264_dump_userdata(void)
{
	if (user_data_buf) {
		show_user_data_buf();
		kfree(user_data_buf);
		user_data_buf = NULL;
	}
}

static void vmh264_reset_user_data_buf(void)
{
	total_len = 0;
	pbuf_start = user_data_buf;
	bskip = 0;
	n_userdata_id = 0;
}
#endif


static void vmh264_udc_fill_vpts(struct vdec_h264_hw_s *hw,
						int frame_type,
						u32 vpts,
						u32 vpts_valid)
{
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	struct vdec_s *vdec = hw_to_vdec(hw);
	unsigned char *pdata;
	u8 *pmax_sei_data_buffer;
	u8 *sei_data_buf;
	int i;
	int wp;
	int data_length;
	struct mh264_userdata_record_t *p_userdata_rec;
	struct StorablePicture *p = p_H264_Dpb->mVideo.dec_picture;

#ifdef MH264_USERDATA_ENABLE
	struct userdata_meta_info_t meta_info;
	memset(&meta_info, 0, sizeof(meta_info));
#endif

	if (hw->sei_itu_data_len <= 0)
		return;

	if (p != NULL) {
		struct buffer_spec_s *pic = &hw->buffer_spec[p->buf_spec_num];

		if (pic->user_data_buf != NULL) {
			memset(pic->user_data_buf, 0, SEI_ITU_DATA_SIZE);
			if (hw->sei_itu_data_len < SEI_ITU_DATA_SIZE) {
				memcpy(pic->user_data_buf, hw->sei_itu_data_buf,
					hw->sei_itu_data_len);
				pic->ud_param.buf_len = hw->sei_itu_data_len;
			} else {
				pic->ud_param.buf_len = 0;
				dpb_print(DECODE_ID(hw), 0,
					"sei data len is over 4k\n", hw->sei_itu_data_len);
			}
		} else {
			pic->ud_param.buf_len = 0;
		}
		pic->ud_param.pbuf_addr = pic->user_data_buf;
		pic->ud_param.instance_id = vdec->afd_video_id;
		pic->ud_param.meta_info.duration = hw->frame_dur;
		pic->ud_param.meta_info.flags = (VFORMAT_H264 << 3);
		pic->ud_param.meta_info.poc_number =
			p_H264_Dpb->mVideo.dec_picture->poc;
		pic->ud_param.meta_info.flags |=
			p_H264_Dpb->mVideo.dec_picture->pic_struct << 12;
		pic->ud_param.meta_info.vpts = vpts;
		pic->ud_param.meta_info.vpts_valid = vpts_valid;
	}

	pdata = (u8 *)hw->sei_user_data_buffer + hw->sei_user_data_wp;
	pmax_sei_data_buffer = (u8 *)hw->sei_user_data_buffer + USER_DATA_SIZE;
	sei_data_buf = (u8 *)hw->sei_itu_data_buf;
	for (i = 0; i < hw->sei_itu_data_len; i++) {
		*pdata++ = sei_data_buf[i];
		if (pdata >= pmax_sei_data_buffer)
			pdata = (u8 *)hw->sei_user_data_buffer;
	}

	hw->sei_user_data_wp = (hw->sei_user_data_wp
		+ hw->sei_itu_data_len) % USER_DATA_SIZE;
	hw->sei_itu_data_len = 0;

#ifdef MH264_USERDATA_ENABLE
	meta_info.duration = hw->frame_dur;
	meta_info.flags |= (VFORMAT_H264 << 3);

	meta_info.vpts = vpts;
	meta_info.vpts_valid = vpts_valid;
	meta_info.poc_number =
		p_H264_Dpb->mVideo.dec_picture->poc;


	wp = hw->sei_user_data_wp;

	if (hw->sei_user_data_wp > hw->userdata_info.last_wp)
		data_length = wp - hw->userdata_info.last_wp;
	else
		data_length = wp + hw->userdata_info.buf_len
			- hw->userdata_info.last_wp;

	if (data_length & 0x7)
		data_length = (((data_length + 8) >> 3) << 3);

	p_userdata_rec = &hw->ud_record;
	p_userdata_rec->meta_info = meta_info;
	p_userdata_rec->rec_start = hw->userdata_info.last_wp;
	p_userdata_rec->rec_len = data_length;
	hw->userdata_info.last_wp = wp;

	p_userdata_rec->meta_info.flags |=
		p_H264_Dpb->mVideo.dec_picture->pic_struct << 12;

	hw->wait_for_udr_send = 1;
	vdec_schedule_work(&hw->user_data_ready_work);
#endif
}


static void user_data_ready_notify_work(struct work_struct *work)
{
	struct vdec_h264_hw_s *hw = container_of(work,
		struct vdec_h264_hw_s, user_data_ready_work);


	mutex_lock(&hw->userdata_mutex);

	hw->userdata_info.records[hw->userdata_info.write_index]
		= hw->ud_record;
	hw->userdata_info.write_index++;
	if (hw->userdata_info.write_index >= USERDATA_FIFO_NUM)
		hw->userdata_info.write_index = 0;

	mutex_unlock(&hw->userdata_mutex);

#ifdef DUMP_USERDATA_RECORD
	dump_userdata_record(hw, &hw->ud_record);
#endif
	vdec_wakeup_userdata_poll(hw_to_vdec(hw));

	hw->wait_for_udr_send = 0;
}

static int vmh264_user_data_read(struct vdec_s *vdec,
	struct userdata_param_t *puserdata_para)
{
	struct vdec_h264_hw_s *hw = NULL;
	int rec_ri, rec_wi;
	int rec_len;
	u8 *rec_data_start;
	u8 *pdest_buf;
	struct mh264_userdata_record_t *p_userdata_rec;
	u32 data_size;
	u32 res;
	int copy_ok = 1;

	hw = (struct vdec_h264_hw_s *)vdec->private;

	pdest_buf = puserdata_para->pbuf_addr;

	mutex_lock(&hw->userdata_mutex);

/*
	pr_info("ri = %d, wi = %d\n",
		lg_p_mpeg12_userdata_info->read_index,
		lg_p_mpeg12_userdata_info->write_index);
*/
	rec_ri = hw->userdata_info.read_index;
	rec_wi = hw->userdata_info.write_index;

	if (rec_ri == rec_wi) {
		mutex_unlock(&hw->userdata_mutex);
		return 0;
	}

	p_userdata_rec = hw->userdata_info.records + rec_ri;

	rec_len = p_userdata_rec->rec_len;
	rec_data_start = p_userdata_rec->rec_start + hw->userdata_info.data_buf;
/*
	pr_info("rec_len:%d, rec_start:%d, buf_len:%d\n",
		p_userdata_rec->rec_len,
		p_userdata_rec->rec_start,
		puserdata_para->buf_len);
*/
	if (rec_len <= puserdata_para->buf_len) {
		/* dvb user data buffer is enought to
		copy the whole recored. */
		data_size = rec_len;
		if (rec_data_start + data_size
			> hw->userdata_info.data_buf_end) {
			int first_section_len;

			first_section_len = hw->userdata_info.buf_len -
				p_userdata_rec->rec_start;
			res = (u32)copy_to_user((void *)pdest_buf,
							(void *)rec_data_start,
							first_section_len);
			if (res) {
				pr_info("p1 read not end res=%d, request=%d\n",
					res, first_section_len);
				copy_ok = 0;

				p_userdata_rec->rec_len -=
					first_section_len - res;
				p_userdata_rec->rec_start +=
					first_section_len - res;
				puserdata_para->data_size =
					first_section_len - res;
			} else {
				res = (u32)copy_to_user(
					(void *)(pdest_buf+first_section_len),
					(void *)hw->userdata_info.data_buf,
					data_size - first_section_len);
				if (res) {
					pr_info("p2 read not end res=%d, request=%d\n",
						res, data_size);
					copy_ok = 0;
				}
				p_userdata_rec->rec_len -=
					data_size - res;
				p_userdata_rec->rec_start =
					data_size - first_section_len - res;
				puserdata_para->data_size =
					data_size - res;
			}
		} else {
			res = (u32)copy_to_user((void *)pdest_buf,
							(void *)rec_data_start,
							data_size);
			if (res) {
				pr_info("p3 read not end res=%d, request=%d\n",
					res, data_size);
				copy_ok = 0;
			}
			p_userdata_rec->rec_len -= data_size - res;
			p_userdata_rec->rec_start += data_size - res;
			puserdata_para->data_size = data_size - res;
		}

		if (copy_ok) {
			hw->userdata_info.read_index++;
			if (hw->userdata_info.read_index >= USERDATA_FIFO_NUM)
				hw->userdata_info.read_index = 0;
		}
	} else {
		/* dvb user data buffer is not enought
		to copy the whole recored. */
		data_size = puserdata_para->buf_len;
		if (rec_data_start + data_size
			> hw->userdata_info.data_buf_end) {
			int first_section_len;

			first_section_len = hw->userdata_info.buf_len -
				p_userdata_rec->rec_start;
			res = (u32)copy_to_user((void *)pdest_buf,
						(void *)rec_data_start,
						first_section_len);
			if (res) {
				pr_info("p4 read not end res=%d, request=%d\n",
					res, first_section_len);
				copy_ok = 0;
				p_userdata_rec->rec_len -=
					first_section_len - res;
				p_userdata_rec->rec_start +=
					first_section_len - res;
				puserdata_para->data_size =
					first_section_len - res;
			} else {
				/* first secton copy is ok*/
				res = (u32)copy_to_user(
					(void *)(pdest_buf+first_section_len),
					(void *)hw->userdata_info.data_buf,
					data_size - first_section_len);
				if (res) {
					pr_info("p5 read not end res=%d, request=%d\n",
						res,
						data_size - first_section_len);
					copy_ok = 0;
				}

				p_userdata_rec->rec_len -=
					data_size - res;
				p_userdata_rec->rec_start =
					data_size - first_section_len - res;
				puserdata_para->data_size =
					data_size - res;
			}
		} else {
			res = (u32)copy_to_user((void *)pdest_buf,
							(void *)rec_data_start,
							data_size);
			if (res) {
				pr_info("p6 read not end res=%d, request=%d\n",
					res, data_size);
				copy_ok = 0;
			}

			p_userdata_rec->rec_len -= data_size - res;
			p_userdata_rec->rec_start += data_size - res;
			puserdata_para->data_size = data_size - res;
		}

		if (copy_ok) {
			hw->userdata_info.read_index++;
			if (hw->userdata_info.read_index >= USERDATA_FIFO_NUM)
				hw->userdata_info.read_index = 0;
		}

	}
	puserdata_para->meta_info = p_userdata_rec->meta_info;

	if (hw->userdata_info.read_index <= hw->userdata_info.write_index)
		puserdata_para->meta_info.records_in_que =
			hw->userdata_info.write_index -
			hw->userdata_info.read_index;
	else
		puserdata_para->meta_info.records_in_que =
			hw->userdata_info.write_index +
			USERDATA_FIFO_NUM -
			hw->userdata_info.read_index;

	puserdata_para->version = (0<<24|0<<16|0<<8|1);

	mutex_unlock(&hw->userdata_mutex);

	return 1;
}

static void vmh264_reset_userdata_fifo(struct vdec_s *vdec, int bInit)
{
	struct vdec_h264_hw_s *hw = NULL;

	hw = (struct vdec_h264_hw_s *)vdec->private;

	if (hw) {
		mutex_lock(&hw->userdata_mutex);
		pr_info("vmh264_reset_userdata_fifo: bInit: %d, ri: %d, wi: %d\n",
			bInit,
			hw->userdata_info.read_index,
			hw->userdata_info.write_index);
		hw->userdata_info.read_index = 0;
		hw->userdata_info.write_index = 0;

		if (bInit)
			hw->userdata_info.last_wp = 0;
		mutex_unlock(&hw->userdata_mutex);
	}
}

static void vmh264_wakeup_userdata_poll(struct vdec_s *vdec)
{
	amstream_wakeup_userdata_poll(vdec);
}

#endif

static int vmh264_get_ps_info(struct vdec_h264_hw_s *hw,
	u32 param1, u32 param2, u32 param3, u32 param4,
	struct aml_vdec_ps_infos *ps)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct vdec_s *vdec = hw_to_vdec(hw);
#endif
	int mb_width, mb_total;
	int mb_height = 0;
	int active_buffer_spec_num, dec_dpb_size;
	int max_reference_size ,level_idc;
	u32 frame_mbs_only_flag;
	u32 chroma_format_idc;
	u32 crop_bottom, crop_right;
	int sub_width_c = 0, sub_height_c = 0;
	u32 frame_width, frame_height;
	u32 used_reorder_dpb_size_margin
		= hw->reorder_dpb_size_margin;

	level_idc = param4 & 0xff;
	max_reference_size = (param4 >> 8) & 0xff;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (vdec->master || vdec->slave)
		used_reorder_dpb_size_margin =
			reorder_dpb_size_margin_dv;
#endif
	mb_width = param1 & 0xff;
	mb_total = (param1 >> 8) & 0xffff;
	if (!mb_width && mb_total) /*for 4k2k*/
		mb_width = 256;
	if (mb_width)
		mb_height = mb_total/mb_width;
	if (mb_width <= 0 || mb_height <= 0 ||
		is_oversize(mb_width << 4, mb_height << 4)) {
		dpb_print(DECODE_ID(hw), 0,
			"!!!wrong param1 0x%x mb_width/mb_height (0x%x/0x%x) %x\r\n",
			param1,
			mb_width,
			mb_height);
		hw->error_frame_width = mb_width << 4;
		hw->error_frame_height = mb_height << 4;
		return -1;
	}
	hw->error_frame_width = 0;
	hw->error_frame_height = 0;

	dec_dpb_size = get_dec_dpb_size(hw , mb_width, mb_height, level_idc);

	dpb_print(DECODE_ID(hw), 0,
		"restriction_flag=%d, max_dec_frame_buffering=%d, dec_dpb_size=%d num_reorder_frames %d used_reorder_dpb_size_margin %d\n",
		hw->bitstream_restriction_flag,
		hw->max_dec_frame_buffering,
		dec_dpb_size,
		hw->num_reorder_frames,
		used_reorder_dpb_size_margin);

	active_buffer_spec_num =
		dec_dpb_size
		+ used_reorder_dpb_size_margin;

	if (active_buffer_spec_num > MAX_VF_BUF_NUM) {
		active_buffer_spec_num = MAX_VF_BUF_NUM;
		dec_dpb_size = active_buffer_spec_num
			- used_reorder_dpb_size_margin;
	}

	hw->dpb.mDPB.size = active_buffer_spec_num;

	if (hw->no_poc_reorder_flag)
		dec_dpb_size = 1;

	/*
	 * crop
	 * AV_SCRATCH_2
	 * bit 15: frame_mbs_only_flag
	 * bit 13-14: chroma_format_idc
	 */
	hw->seq_info = param2;
	frame_mbs_only_flag = (hw->seq_info >> 15) & 0x01;
	if (hw->dpb.mSPS.profile_idc != 100 &&
		hw->dpb.mSPS.profile_idc != 110 &&
		hw->dpb.mSPS.profile_idc != 122 &&
		hw->dpb.mSPS.profile_idc != 144) {
		hw->dpb.chroma_format_idc = 1;
	}
	chroma_format_idc = hw->dpb.chroma_format_idc;

	/*
	 * AV_SCRATCH_6 bit 31-16 =  (left  << 8 | right ) << 1
	 * AV_SCRATCH_6 bit 15-0 =  (top << 8  | bottom ) <<
	 *                          (2 - frame_mbs_only_flag)
	 */
	switch (chroma_format_idc) {
		case 1:
			sub_width_c = 2;
			sub_height_c = 2;
			break;

		case 2:
			sub_width_c = 2;
			sub_height_c = 1;
			break;

		case 3:
			sub_width_c = 1;
			sub_height_c = 1;
			break;

		default:
			break;
	}

	if (chroma_format_idc == 0) {
		crop_right = hw->dpb.frame_crop_right_offset;
		crop_bottom = hw->dpb.frame_crop_bottom_offset *
			(2 - frame_mbs_only_flag);
	} else {
		crop_right = sub_width_c * hw->dpb.frame_crop_right_offset;
		crop_bottom = sub_height_c * hw->dpb.frame_crop_bottom_offset *
			(2 - frame_mbs_only_flag);
	}

	frame_width = mb_width << 4;
	frame_height = mb_height << 4;

	frame_width = frame_width - crop_right;
	frame_height = frame_height - crop_bottom;

	ps->profile 		= level_idc;
	ps->ref_frames 		= max_reference_size;
	ps->mb_width 		= mb_width;
	ps->mb_height 		= mb_height;
	ps->visible_width	= frame_width;
	ps->visible_height	= frame_height;
	ps->coded_width		= ALIGN(mb_width << 4, 64);
	ps->coded_height	= ALIGN(mb_height << 4, 64);
	ps->dpb_frames		= dec_dpb_size + 1; /* +1 for two frames in one packet */
	ps->dpb_size		= active_buffer_spec_num;

	return 0;
}

static int v4l_res_change(struct vdec_h264_hw_s *hw,
			  u32 param1, u32 param2,
			  u32 param3, u32 param4)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int ret = 0;
	int dec_dpb_size_change = hw->dpb.dec_dpb_size != get_dec_dpb_size_active(hw, param1, param4);

	if (ctx->param_sets_from_ucode &&
			hw->res_ch_flag == 0) {
		if (((param1 != 0 &&
			hw->seq_info2 != param1) || dec_dpb_size_change) &&
			hw->seq_info2 != 0) { /*picture size changed*/
			struct aml_vdec_ps_infos ps;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
				"h264 res_change\n");
			if (vmh264_get_ps_info(hw, param1,
				param2, param3, param4, &ps) < 0) {
				dpb_print(DECODE_ID(hw), 0,
					"set parameters error\n");
			}
			hw->v4l_params_parsed = false;
			vdec_v4l_set_ps_infos(ctx, &ps);
			vdec_v4l_res_ch_event(ctx);
			hw->res_ch_flag = 1;
			ctx->v4l_resolution_change = 1;
			amvdec_stop();
			if (hw->mmu_enable)
				amhevc_stop();
			hw->eos = 1;
			flush_dpb(p_H264_Dpb);
			//del_timer_sync(&hw->check_timer);
			notify_v4l_eos(hw_to_vdec(hw));
			ret = 1;
		}
	}

	return ret;

}

static int check_dirty_data(struct vdec_s *vdec)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)(vdec->private);
	u32 wp, rp, level;

	rp = STBUF_READ(&vdec->vbuf, get_rp);
	wp = STBUF_READ(&vdec->vbuf, get_wp);

	if (wp > rp)
		level = wp - rp;
	else
		level = wp + vdec->input.size - rp ;

	if (level > (vdec->input.size / 2))
		hw->dec_again_cnt++;

	if (hw->dec_again_cnt > dirty_again_threshold) {
		dpb_print(DECODE_ID(hw), 0, "h264 data skipped %x\n", level);
		hw->dec_again_cnt = 0;
		return 1;
	}
	return 0;
}

static void vh264_work_implement(struct vdec_h264_hw_s *hw,
	struct vdec_s *vdec, int from)
{
	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	u32 wcount = 0;

	if (hw->dec_result == DEC_RESULT_DONE) {
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_WORKER_START);
	} else if (hw->dec_result == DEC_RESULT_AGAIN)
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_WORKER_AGAIN);

	dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_DETAIL,
		"%s dec_result %d %x %x %x\n",
		__func__,
		hw->dec_result,
		READ_VREG(VLD_MEM_VIFIFO_LEVEL),
		READ_VREG(VLD_MEM_VIFIFO_WP),
		READ_VREG(VLD_MEM_VIFIFO_RP));
		if (!hw->mmu_enable) {
			mutex_lock(&vmh264_mutex);
			dealloc_buf_specs(hw, 0);
			mutex_unlock(&vmh264_mutex);
		}
	hw->save_reg_f = READ_VREG(AV_SCRATCH_F);
	hw->dpb.last_dpb_status = hw->dpb.dec_dpb_status;
	if (hw->dec_result == DEC_RESULT_CONFIG_PARAM) {
		u32 param1 = READ_VREG(AV_SCRATCH_1);
		u32 param2 = READ_VREG(AV_SCRATCH_2);
		u32 param3 = READ_VREG(AV_SCRATCH_6);
		u32 param4 = READ_VREG(AV_SCRATCH_B);
		struct aml_vcodec_ctx *ctx =
				(struct aml_vcodec_ctx *)(hw->v4l2_ctx);

		if (hw->is_used_v4l &&
			ctx->param_sets_from_ucode) {
			if (!v4l_res_change(hw, param1, param2, param3, param4)) {
				if (!hw->v4l_params_parsed) {
					struct aml_vdec_ps_infos ps;
					dpb_print(DECODE_ID(hw),
						PRINT_FLAG_DEC_DETAIL,
						"h264 parsered csd data\n");
					if (vmh264_get_ps_info(hw,
						param1, param2,
						param3, param4, &ps) < 0) {
						dpb_print(DECODE_ID(hw), 0,
							"set parameters error\n");
					}
					hw->v4l_params_parsed = true;
					vdec_v4l_set_ps_infos(ctx, &ps);

					amvdec_stop();
					if (hw->mmu_enable)
						amhevc_stop();
				} else {
					if (vh264_set_params(hw, param1,
					param2, param3, param4, false) < 0) {
						hw->init_flag = 0;
						dpb_print(DECODE_ID(hw), 0, "set parameters error, init_flag: %u\n",
							hw->init_flag);
					}

					WRITE_VREG(AV_SCRATCH_0, (hw->max_reference_size<<24) |
						(hw->dpb.mDPB.size<<16) |
						(hw->dpb.mDPB.size<<8));
					hw->res_ch_flag = 0;
					start_process_time(hw);
					return;
				}
			}
		} else {
			if (vh264_set_params(hw, param1,
				param2, param3, param4, false) < 0) {
				hw->init_flag = 0;
				dpb_print(DECODE_ID(hw), 0, "set parameters error, init_flag: %u\n",
					hw->init_flag);
			}

			WRITE_VREG(AV_SCRATCH_0, (hw->max_reference_size<<24) |
				(hw->dpb.mDPB.size<<16) |
				(hw->dpb.mDPB.size<<8));
			start_process_time(hw);
			return;
		}
	} else
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
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_GET_DATA %x %x %x\n",
				__func__,
				READ_VREG(VLD_MEM_VIFIFO_LEVEL),
				READ_VREG(VLD_MEM_VIFIFO_WP),
				READ_VREG(VLD_MEM_VIFIFO_RP));
			mutex_lock(&hw->chunks_mutex);
			vdec_vframe_dirty(vdec, hw->chunk);
			hw->chunk = NULL;
			mutex_unlock(&hw->chunks_mutex);
			vdec_clean_input(vdec);
		}
		if ((hw->dec_result == DEC_RESULT_GET_DATA_RETRY) &&
			((1000 * (jiffies - hw->get_data_start_time) / HZ)
			> get_data_timeout_val)) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
				"%s DEC_RESULT_GET_DATA_RETRY timeout\n",
				__func__);
			goto result_done;
		}
		if (is_buffer_available(vdec)) {
			int r;
			int decode_size;
			r = vdec_prepare_input(vdec, &hw->chunk);
			if (r < 0 && (hw_to_vdec(hw)->next_status !=
						VDEC_STATUS_DISCONNECTED)) {
				hw->dec_result = DEC_RESULT_GET_DATA_RETRY;

				dpb_print(DECODE_ID(hw),
					PRINT_FLAG_VDEC_DETAIL,
					"vdec_prepare_input: Insufficient data\n");
				vdec_schedule_work(&hw->work);
				return;
			}
			hw->dec_result = DEC_RESULT_NONE;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
				"%s: chunk size 0x%x\n",
				__func__, hw->chunk->size);

			if (dpb_is_debug(DECODE_ID(hw),
				PRINT_FRAMEBASE_DATA)) {
				int jj;
				u8 *data = NULL;

				if (!hw->chunk->block->is_mapped)
					data = codec_mm_vmap(
						hw->chunk->block->start +
						hw->chunk->offset, r);
				else
					data = ((u8 *)
						hw->chunk->block->start_virt)
						+ hw->chunk->offset;

				for (jj = 0; jj < r; jj++) {
					if ((jj & 0xf) == 0)
						dpb_print(DECODE_ID(hw),
						PRINT_FRAMEBASE_DATA,
							"%06x:", jj);
					dpb_print_cont(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA,
						"%02x ", data[jj]);
					if (((jj + 1) & 0xf) == 0)
						dpb_print_cont(DECODE_ID(hw),
						PRINT_FRAMEBASE_DATA,
							"\n");
				}

				if (!hw->chunk->block->is_mapped)
					codec_mm_unmap_phyaddr(data);
			}
			WRITE_VREG(POWER_CTL_VLD,
				READ_VREG(POWER_CTL_VLD) |
					(0 << 10) | (1 << 9) | (1 << 6));
			WRITE_VREG(H264_DECODE_INFO, (1<<13));
			decode_size = hw->chunk->size +
				(hw->chunk->offset & (VDEC_FIFO_ALIGN - 1));
			WRITE_VREG(H264_DECODE_SIZE, decode_size);
			WRITE_VREG(VIFF_BIT_CNT, decode_size * 8);
			vdec_enable_input(vdec);

			WRITE_VREG(DPB_STATUS_REG, H264_ACTION_SEARCH_HEAD);
			start_process_time(hw);
		} else{
			if (hw_to_vdec(hw)->next_status
				!=	VDEC_STATUS_DISCONNECTED) {
				hw->dec_result = DEC_RESULT_GET_DATA_RETRY;
				vdec_schedule_work(&hw->work);
			}
		}
		return;
	} else if (hw->dec_result == DEC_RESULT_DONE ||
					hw->dec_result == DEC_RESULT_TIMEOUT) {
		/* if (!hw->ctx_valid)
			hw->ctx_valid = 1; */
		hw->dec_again_cnt = 0;
		if ((hw->dec_result == DEC_RESULT_TIMEOUT) &&
				!hw->i_only && (hw->error_proc_policy & 0x2)) {
			struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
			hw->data_flag |= ERROR_FLAG;
			dpb_print(DECODE_ID(hw), 0,
				"%s, decode timeout store in dpb\n", __func__);
			if (p_H264_Dpb->mVideo.dec_picture != NULL) {
				hw->dpb.mVideo.dec_picture->data_flag |= ERROR_FLAG;
				dpb_print(DECODE_ID(hw), 0,
				"%s, dec_picture->data_flag %d hw->data_flag %d\n", __func__, hw->data_flag,
				hw->dpb.mVideo.dec_picture->data_flag);
			}
			vh264_pic_done_proc(vdec);

		}
result_done:
		{
			if (hw->error_proc_policy & 0x8000) {
				struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
				int i;
				struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;

				for (i = 0; i < p_Dpb->used_size; i++) {
					int i_flag = p_Dpb->fs[i]->bottom_field || p_Dpb->fs[i]->top_field;
					int threshold = (i_flag || (hw->max_reference_size >= 12)) ? ((50 + p_Dpb->used_size) * 2)  : 50 + p_Dpb->used_size;
					if ((p_Dpb->fs[i]->dpb_frame_count + threshold
							< p_H264_Dpb->dpb_frame_count) &&
						p_Dpb->fs[i]->is_reference &&
						!p_Dpb->fs[i]->is_long_term &&
						p_Dpb->fs[i]->is_output) {
						dpb_print(DECODE_ID(hw),
							0,
							"unmark reference dpb_frame_count diffrence large in dpb\n");
						unmark_for_reference(p_Dpb, p_Dpb->fs[i]);
						update_ref_list(p_Dpb);
					}
				}
			}
		}
			if (hw->mmu_enable
				&& hw->frame_busy && hw->frame_done) {
				long used_4k_num;
				hevc_sao_wait_done(hw);
				if (hw->hevc_cur_buf_idx != 0xffff) {
					used_4k_num =
					(READ_VREG(HEVC_SAO_MMU_STATUS) >> 16);
				if (used_4k_num >= 0)
					dpb_print(DECODE_ID(hw),
					PRINT_FLAG_MMU_DETAIL,
					"release unused buf , used_4k_num %ld index %d\n",
					used_4k_num, hw->hevc_cur_buf_idx);
				hevc_mmu_dma_check(hw_to_vdec(hw));
				decoder_mmu_box_free_idx_tail(
					hw->mmu_box,
					hw->hevc_cur_buf_idx,
					used_4k_num);
					hw->hevc_cur_buf_idx = 0xffff;
				}
			}
		decode_frame_count[DECODE_ID(hw)]++;
		if (hw->dpb.mSlice.slice_type == I_SLICE) {
			hw->gvs.i_decoded_frames++;
		} else if (hw->dpb.mSlice.slice_type == P_SLICE) {
			hw->gvs.p_decoded_frames++;
		} else if (hw->dpb.mSlice.slice_type == B_SLICE) {
			hw->gvs.b_decoded_frames++;
		}
		amvdec_stop();

		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s dec_result %d %x %x %x\n",
			__func__,
			hw->dec_result,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP));
		mutex_lock(&hw->chunks_mutex);
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
		hw->chunk = NULL;
		mutex_unlock(&hw->chunks_mutex);
	} else if (hw->dec_result == DEC_RESULT_AGAIN) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		amvdec_stop();
		if (hw->mmu_enable)
			amhevc_stop();
		if (!vdec_has_more_input(vdec) && (hw_to_vdec(hw)->next_status !=
			VDEC_STATUS_DISCONNECTED) && (hw->no_decoder_buffer_flag == 0)) {
			hw->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hw->work);
			return;
		}
		if ((vdec_stream_based(vdec)) &&
			(hw->error_proc_policy & 0x400000) &&
			check_dirty_data(vdec)) {
			hw->dec_result = DEC_RESULT_DONE;
			vdec_schedule_work(&hw->work);
			return;
		}
		hw->no_decoder_buffer_flag = 0;
		hw->next_again_flag = 1;
	} else if (hw->dec_result == DEC_RESULT_EOS) {
		struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s: end of stream\n",
			__func__);
		amvdec_stop();
		if (hw->mmu_enable)
			amhevc_stop();
		hw->eos = 1;
		flush_dpb(p_H264_Dpb);
		notify_v4l_eos(hw_to_vdec(hw));
		mutex_lock(&hw->chunks_mutex);
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
		hw->chunk = NULL;
		mutex_unlock(&hw->chunks_mutex);
		vdec_clean_input(vdec);
	} else if (hw->dec_result == DEC_RESULT_FORCE_EXIT) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s: force exit\n",
			__func__);
		amvdec_stop();
		if (hw->mmu_enable)
			amhevc_stop();
		if (hw->stat & STAT_ISR_REG) {
			do {
				vdec_free_irq(VDEC_IRQ_1, (void *)hw);

				if (++wcount > 1000)
					break;
				usleep_range(100, 200);
			} while (vdec->irq_cnt > vdec->irq_thread_cnt);
			hw->stat &= ~STAT_ISR_REG;

			if (work_pending(&hw->work)) {
				hw->dec_result = DEC_RESULT_FORCE_EXIT;
			}
		}
	}

	if (p_H264_Dpb->mVideo.dec_picture) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s, release decoded picture\n", __func__);
		release_cur_decoding_buf(hw);
	}

	WRITE_VREG(ASSIST_MBOX1_MASK, 0);
	del_timer_sync(&hw->check_timer);
	hw->stat &= ~STAT_TIMER_ARM;
#ifdef DETECT_WRONG_MULTI_SLICE
	if (hw->dec_result != DEC_RESULT_AGAIN)
		hw->last_picture_slice_count = 0;
#endif
	ATRACE_COUNTER(hw->trace.decode_work_time_name, TRACE_WORK_WAIT_SEARCH_DONE_START);
	wait_vmh264_search_done(hw);
	ATRACE_COUNTER(hw->trace.decode_work_time_name, TRACE_WORK_WAIT_SEARCH_DONE_END);
	/* mark itself has all HW resource released and input released */

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (hw->switch_dvlayer_flag) {
		if (vdec->slave)
			vdec_set_next_sched(vdec, vdec->slave);
		else if (vdec->master)
			vdec_set_next_sched(vdec, vdec->master);
	} else if (vdec->slave || vdec->master)
		vdec_set_next_sched(vdec, vdec);
#endif

	if (from == 1) {
		/* This is a timeout work */
		if (work_pending(&hw->work)) {
			/*
			 * The vh264_work arrives at the last second,
			 * give it a chance to handle the scenario.
			 */
			return;
		}
	}
	if (hw->dec_result == DEC_RESULT_DONE) {
		ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_WORKER_END);
	}

	/* mark itself has all HW resource released and input released */
	if (vdec->parallel_dec == 1) {
		if (hw->mmu_enable == 0)
			vdec_core_finish_run(vdec, CORE_MASK_VDEC_1);
		else
			vdec_core_finish_run(vdec, CORE_MASK_VDEC_1 | CORE_MASK_HEVC);
	} else
		vdec_core_finish_run(vdec, CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

	wake_up_interruptible(&hw->wait_q);

	if (hw->is_used_v4l) {
		struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hw->v4l2_ctx);

		if (ctx->param_sets_from_ucode &&
			!hw->v4l_params_parsed)
			vdec_v4l_write_frame_sync(ctx);
	}

	if (hw->vdec_cb)
		hw->vdec_cb(hw_to_vdec(hw), hw->vdec_cb_arg);
}


static void vh264_work(struct work_struct *work)
{
	struct vdec_h264_hw_s *hw = container_of(work,
		struct vdec_h264_hw_s, work);
	struct vdec_s *vdec = hw_to_vdec(hw);

	vh264_work_implement(hw, vdec, 0);
}


static void vh264_timeout_work(struct work_struct *work)
{
	struct vdec_h264_hw_s *hw = container_of(work,
		struct vdec_h264_hw_s, timeout_work);
	struct vdec_s *vdec = hw_to_vdec(hw);

	if (work_pending(&hw->work))
		return;

	hw->timeout_processing = 1;
	vh264_work_implement(hw, vdec, 1);
}

static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	bool ret = 0;
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)vdec->private;
	int tvp = vdec_secure(hw_to_vdec(hw)) ?
		CODEC_MM_FLAGS_TVP : 0;

	if (hw->timeout_processing &&
	    (work_pending(&hw->work) || work_busy(&hw->work) ||
	    work_pending(&hw->timeout_work) || work_busy(&hw->timeout_work))) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_DETAIL,
			  "h264 work pending, not ready for run.\n");
		return 0;
	}
	hw->timeout_processing = 0;
	if (!hw->first_sc_checked && hw->mmu_enable) {
		int size = decoder_mmu_box_sc_check(hw->mmu_box, tvp);
		hw->first_sc_checked =1;
		dpb_print(DECODE_ID(hw), 0,
			"vmh264 cached=%d  need_size=%d speed= %d ms\n",
			size, (hw->need_cache_size >> PAGE_SHIFT),
			(int)(get_jiffies_64() - hw->sc_start_time) * 1000/HZ);
	}

	if (vdec_stream_based(vdec) && (hw->init_flag == 0)
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

#ifndef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (vdec->master)
		return 0;
#endif
	if (hw->eos)
		return 0;

	if (hw->stat & DECODER_FATAL_ERROR_NO_MEM)
		return 0;

	if (disp_vframe_valve_level &&
		kfifo_len(&hw->display_q) >=
		disp_vframe_valve_level) {
		hw->valve_count--;
		if (hw->valve_count <= 0)
			hw->valve_count = 2;
		else
			return 0;
	}
	if (hw->next_again_flag &&
		(!vdec_frame_based(vdec))) {
		u32 parser_wr_ptr = STBUF_READ(&vdec->vbuf, get_wp);
		if (parser_wr_ptr >= hw->pre_parser_wr_ptr &&
			(parser_wr_ptr - hw->pre_parser_wr_ptr) <
			again_threshold) {
			int r = vdec_sync_input(vdec);
				dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_DETAIL,
					"%s buf lelvel:%x\n",  __func__, r);
			return 0;
		}
	}

	if (h264_debug_flag & 0x20000000) {
		/* pr_info("%s, a\n", __func__); */
		ret = 1;
	} else
		ret = is_buffer_available(vdec);

#ifdef CONSTRAIN_MAX_BUF_NUM
	if (ret && (hw->dpb.mDPB.size > 0)) { /*make sure initilized*/
		if (run_ready_max_vf_only_num > 0 &&
			get_vf_ref_only_buf_count(hw) >=
			run_ready_max_vf_only_num
			)
			ret = 0;
		if (run_ready_display_q_num > 0 &&
			kfifo_len(&hw->display_q) >=
			run_ready_display_q_num)
			ret = 0;
		/*avoid more buffers consumed when
		switching resolution*/
		if (run_ready_max_buf_num == 0xff &&
			get_used_buf_count(hw) >
			hw->dpb.mDPB.size)
			ret = 0;
		else if (run_ready_max_buf_num &&
			get_used_buf_count(hw) >=
			run_ready_max_buf_num)
			ret = 0;
		if (ret == 0)
			bufmgr_h264_remove_unused_frame(&hw->dpb, 0);
	}
#endif
	if (hw->is_used_v4l) {
		struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hw->v4l2_ctx);

		if (ctx->param_sets_from_ucode) {
			if (hw->v4l_params_parsed) {
				if (ctx->cap_pool.dec < hw->dpb.mDPB.size) {
					if (is_buffer_available(vdec))
						ret = 1;
					else
						ret = 0;
				}
			} else {
				if (ctx->v4l_resolution_change)
					ret = 0;
			}
		} else if (!ctx->v4l_codec_dpb_ready) {
			if (v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx) <
				run_ready_min_buf_num)
				ret = 0;
		}
	}

	if (ret)
		not_run_ready[DECODE_ID(hw)] = 0;
	else
		not_run_ready[DECODE_ID(hw)]++;
	if (vdec->parallel_dec == 1) {
		if (hw->mmu_enable == 0)
			return ret ? (CORE_MASK_VDEC_1) : 0;
		else
			return ret ? (CORE_MASK_VDEC_1 | CORE_MASK_HEVC) : 0;
	} else
		return ret ? (CORE_MASK_VDEC_1 | CORE_MASK_HEVC) : 0;
}

static unsigned char get_data_check_sum
	(struct vdec_h264_hw_s *hw, int size)
{
	int jj;
	int sum = 0;
	u8 *data = NULL;

	if (!hw->chunk->block->is_mapped)
		data = codec_mm_vmap(hw->chunk->block->start +
			hw->chunk->offset, size);
	else
		data = ((u8 *)hw->chunk->block->start_virt)
			+ hw->chunk->offset;

	for (jj = 0; jj < size; jj++)
		sum += data[jj];

	if (!hw->chunk->block->is_mapped)
		codec_mm_unmap_phyaddr(data);
	return sum;
}

static void run(struct vdec_s *vdec, unsigned long mask,
	void (*callback)(struct vdec_s *, void *), void *arg)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)vdec->private;
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int size, ret = -1;
	if (!hw->vdec_pg_enable_flag) {
		hw->vdec_pg_enable_flag = 1;
		amvdec_enable();
		if (hw->mmu_enable)
			amhevc_enable();
	}
	ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_RUN_START);

	run_count[DECODE_ID(hw)]++;
	vdec_reset_core(vdec);
	if (hw->mmu_enable)
		hevc_reset_core(vdec);
	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;

#ifdef DETECT_WRONG_MULTI_SLICE
	hw->cur_picture_slice_count = 0;
#endif

	if (kfifo_len(&hw->display_q) > VF_POOL_SIZE) {
		hw->reset_bufmgr_flag = 1;
		dpb_print(DECODE_ID(hw), 0,
			"kfifo len:%d invaild, need bufmgr reset\n",
			kfifo_len(&hw->display_q));
	}

	if (vdec_stream_based(vdec)) {
		hw->pre_parser_wr_ptr =
			STBUF_READ(&vdec->vbuf, get_wp);
		hw->next_again_flag = 0;
	}

	if (hw->reset_bufmgr_flag ||
		((hw->error_proc_policy & 0x40) &&
		p_H264_Dpb->buf_alloc_fail)) {
		h264_reset_bufmgr(vdec);
		//flag must clear after reset for v4l buf_spec_init use
		hw->reset_bufmgr_flag = 0;
	}

	if (h264_debug_cmd & 0xf000) {
		if (((h264_debug_cmd >> 12) & 0xf)
			== (DECODE_ID(hw) + 1)) {
			h264_reconfig(hw);
			h264_debug_cmd &= (~0xf000);
		}
	}
	/* hw->chunk = vdec_prepare_input(vdec); */
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (vdec->slave || vdec->master)
		vdec_set_flag(vdec, VDEC_FLAG_SELF_INPUT_CONTEXT);
#endif
	size = vdec_prepare_input(vdec, &hw->chunk);
	if ((size < 0) ||
		(input_frame_based(vdec) && hw->chunk == NULL)) {
		input_empty[DECODE_ID(hw)]++;
		hw->dec_result = DEC_RESULT_AGAIN;

		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_DETAIL,
			"vdec_prepare_input: Insufficient data\n");

		vdec_schedule_work(&hw->work);
		return;
	}
	input_empty[DECODE_ID(hw)] = 0;

	hw->dec_result = DEC_RESULT_NONE;
	hw->get_data_count = 0;
#if 0
	pr_info("VLD_MEM_VIFIFO_LEVEL = 0x%x, rp = 0x%x, wp = 0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_LEVEL),
		READ_VREG(VLD_MEM_VIFIFO_RP),
		READ_VREG(VLD_MEM_VIFIFO_WP));
#endif

	if (input_frame_based(vdec) && !vdec_secure(vdec)) {
		u8 *data = NULL;

		if (!hw->chunk->block->is_mapped)
			data = codec_mm_vmap(hw->chunk->block->start +
				hw->chunk->offset, size);
		else
			data = ((u8 *)hw->chunk->block->start_virt)
				+ hw->chunk->offset;

		if (dpb_is_debug(DECODE_ID(hw),
			PRINT_FLAG_VDEC_STATUS)
			) {
			dpb_print(DECODE_ID(hw), 0,
			"%s: size 0x%x sum 0x%x %02x %02x %02x %02x %02x %02x .. %02x %02x %02x %02x\n",
			__func__, size, get_data_check_sum(hw, size),
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[size - 4],
			data[size - 3],	data[size - 2],
			data[size - 1]);
		}
		if (dpb_is_debug(DECODE_ID(hw),
			PRINT_FRAMEBASE_DATA)
			) {
			int jj;

			for (jj = 0; jj < size; jj++) {
				if ((jj & 0xf) == 0)
					dpb_print(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA,
						"%06x:", jj);
				dpb_print_cont(DECODE_ID(hw),
				PRINT_FRAMEBASE_DATA,
					"%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					dpb_print_cont(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA,
						"\n");
			}
		}

		if (!hw->chunk->block->is_mapped)
			codec_mm_unmap_phyaddr(data);
	} else
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s: %x %x %x %x %x size 0x%x\n",
			__func__,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			STBUF_READ(&vdec->vbuf, get_rp),
			STBUF_READ(&vdec->vbuf, get_wp),
			size);

	start_process_time(hw);
	if (vdec->mc_loaded) {
			/*firmware have load before,
			  and not changes to another.
			  ignore reload.
			*/
		WRITE_VREG(AV_SCRATCH_G, hw->reg_g_status);
	} else {
		ATRACE_COUNTER(hw->trace.decode_run_time_name, TRACE_RUN_LOADING_FW_START);
		ret = amvdec_vdec_loadmc_ex(VFORMAT_H264, "mh264", vdec, hw->fw->data);
		if (ret < 0) {
			amvdec_enable_flag = false;
			amvdec_disable();
			hw->vdec_pg_enable_flag = 0;
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
				"MH264 the %s fw loading failed, err: %x\n",
				tee_enabled() ? "TEE" : "local", ret);
			hw->dec_result = DEC_RESULT_FORCE_EXIT;
			vdec_schedule_work(&hw->work);
			return;
		}
		vdec->mc_type  = VFORMAT_H264;
		hw->reg_g_status = READ_VREG(AV_SCRATCH_G);
		if (hw->mmu_enable) {
			ret = amhevc_loadmc_ex(VFORMAT_H264, "mh264_mmu",
				hw->fw_mmu->data);
			if (ret < 0) {
				amvdec_enable_flag = false;
				amhevc_disable();
				dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
					"MH264_MMU the %s fw loading failed, err: %x\n",
					tee_enabled() ? "TEE" : "local", ret);
				hw->dec_result = DEC_RESULT_FORCE_EXIT;
				vdec_schedule_work(&hw->work);
				return;
			}
			vdec->mc_type = ((1 << 16) | VFORMAT_H264);
		}
		vdec->mc_loaded = 0;
		ATRACE_COUNTER(hw->trace.decode_run_time_name, TRACE_RUN_LOADING_FW_END);
	}
	vmh264_reset_udr_mgr(hw);
	ATRACE_COUNTER(hw->trace.decode_run_time_name, TRACE_RUN_LOADING_RESTORE_START);
	if (vh264_hw_ctx_restore(hw) < 0) {
		vdec_schedule_work(&hw->work);
		return;
	}
	if (hw->error_proc_policy & 0x10000) {
		hw->first_pre_frame_num = p_H264_Dpb->mVideo.pre_frame_num;
	}
	ATRACE_COUNTER(hw->trace.decode_run_time_name, TRACE_RUN_LOADING_RESTORE_END);
	if (input_frame_based(vdec)) {
		int decode_size = 0;

		decode_size = hw->chunk->size +
			(hw->chunk->offset & (VDEC_FIFO_ALIGN - 1));
		WRITE_VREG(H264_DECODE_INFO, (1<<13));
		WRITE_VREG(H264_DECODE_SIZE, decode_size);
		WRITE_VREG(VIFF_BIT_CNT, decode_size * 8);
		if (vdec->mvfrm)
			vdec->mvfrm->frame_size = hw->chunk->size;
	} else {
		if (size <= 0)
			size = 0x7fffffff; /*error happen*/
		WRITE_VREG(H264_DECODE_INFO, (1<<13));
		WRITE_VREG(H264_DECODE_SIZE, size);
		WRITE_VREG(VIFF_BIT_CNT, size * 8);
		hw->start_bit_cnt = size * 8;
	}
	config_aux_buf(hw);
	config_decode_mode(hw);
	vdec_enable_input(vdec);
	WRITE_VREG(NAL_SEARCH_CTL, 0);
	hw->sei_data_len = 0;
	if (enable_itu_t35)
		WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | 0x1);
	if (!hw->init_flag) {
		if (hw->mmu_enable)
			WRITE_VREG(NAL_SEARCH_CTL,
					READ_VREG(NAL_SEARCH_CTL) | 0x2);
		else
			WRITE_VREG(NAL_SEARCH_CTL,
					READ_VREG(NAL_SEARCH_CTL) & (~0x2));
	}
	WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | (1 << 2) | (hw->bitstream_restriction_flag << 15) | (hw->seq_info3&0xff)<<7);

	if (udebug_flag)
		WRITE_VREG(AV_SCRATCH_K, udebug_flag);
	hw->stat |= STAT_TIMER_ARM;
	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {

		if (hw->mmu_enable)
			SET_VREG_MASK(VDEC_ASSIST_MMC_CTRL1, 1 << 3);
		else
			CLEAR_VREG_MASK(VDEC_ASSIST_MMC_CTRL1, 1 << 3);
	}
	if (hw->i_only)
		hw->dpb.first_insert_frame = FirstInsertFrm_SKIPDONE;
	if (vdec->mvfrm)
		vdec->mvfrm->hw_decode_start = local_clock();
	amvdec_start();
	if (hw->mmu_enable /*&& !hw->frame_busy && !hw->frame_done*/) {
		WRITE_VREG(HEVC_ASSIST_SCRATCH_0, 0x0);
		amhevc_start();
		if (hw->config_bufmgr_done) {
			hevc_mcr_sao_global_hw_init(hw,
					(hw->mb_width << 4), (hw->mb_height << 4));
			hevc_mcr_config_canv2axitbl(hw, 1);
		}
	}

	/* if (hw->init_flag) { */
		WRITE_VREG(DPB_STATUS_REG, H264_ACTION_SEARCH_HEAD);
	/* } */

	hw->init_flag = 1;
	ATRACE_COUNTER(hw->trace.decode_time_name, DECODER_RUN_END);
}

static void clear_refer_bufs(struct vdec_h264_hw_s *hw)
{
	int i;
	ulong flags;

	if (hw->is_used_v4l) {
		spin_lock_irqsave(&hw->bufspec_lock, flags);
		for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
			hw->buffer_spec[i].used = -1;
			hw->buffer_spec[i].cma_alloc_addr = 0;
			hw->buffer_spec[i].buf_adr = 0;
		}
		spin_unlock_irqrestore(&hw->bufspec_lock, flags);
	}

	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &(hw->vfpool[hw->cur_pool][i]);
		hw->vfpool[hw->cur_pool][i].index = -1; /* VF_BUF_NUM; */
		hw->vfpool[hw->cur_pool][i].bufWidth = 1920;
		kfifo_put(&hw->newframe_q, vf);
	}
}

static void reset(struct vdec_s *vdec)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)vdec->private;

	pr_info("vmh264 reset\n");

	cancel_work_sync(&hw->work);
	cancel_work_sync(&hw->notify_work);
	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		if (hw->mmu_enable)
			amhevc_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}

	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->check_timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}
	hw->eos = 0;
	hw->decode_pic_count = 0;

	reset_process_time(hw);
	h264_reset_bufmgr(vdec);
	clear_refer_bufs(hw);

	dpb_print(DECODE_ID(hw), 0, "%s\n", __func__);
}

static void h264_reconfig(struct vdec_h264_hw_s *hw)
{
	int i;
	unsigned long flags;
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	struct vdec_s *vdec = hw_to_vdec(hw);
	dpb_print(DECODE_ID(hw), 0,
	"%s\n", __func__);
	/* after calling flush_dpb() and bufmgr_h264_remove_unused_frame(),
		all buffers are in display queue (used == 2),
			or free (used == 0)
	*/
	if (dpb_is_debug(DECODE_ID(hw),
		PRINT_FLAG_DUMP_BUFSPEC))
		dump_bufspec(hw, "pre h264_reconfig");

	flush_dpb(p_H264_Dpb);
	bufmgr_h264_remove_unused_frame(p_H264_Dpb, 0);

	if (hw->collocate_cma_alloc_addr) {
		decoder_bmmu_box_free_idx(
			hw->bmmu_box,
			BMMU_REF_IDX);
		hw->collocate_cma_alloc_addr = 0;
		hw->dpb.colocated_mv_addr_start = 0;
		hw->dpb.colocated_mv_addr_end = 0;
	}
	spin_lock_irqsave(&hw->bufspec_lock, flags);
	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		if (vdec->parallel_dec == 1) {
			vdec->free_canvas_ex(hw->buffer_spec[i].y_canvas_index, vdec->id);
			vdec->free_canvas_ex(hw->buffer_spec[i].u_canvas_index, vdec->id);
			vdec->free_canvas_ex(hw->buffer_spec[i].v_canvas_index, vdec->id);
			hw->buffer_spec[i].y_canvas_index = -1;
			hw->buffer_spec[i].u_canvas_index = -1;
			hw->buffer_spec[i].v_canvas_index = -1;
#ifdef VDEC_DW
			if (IS_VDEC_DW(hw)) {
				vdec->free_canvas_ex(hw->buffer_spec[i].vdec_dw_y_canvas_index, vdec->id);
				vdec->free_canvas_ex(hw->buffer_spec[i].vdec_dw_u_canvas_index, vdec->id);
				vdec->free_canvas_ex(hw->buffer_spec[i].vdec_dw_v_canvas_index, vdec->id);
				hw->buffer_spec[i].vdec_dw_y_canvas_index = -1;
				hw->buffer_spec[i].vdec_dw_u_canvas_index = -1;
				hw->buffer_spec[i].vdec_dw_v_canvas_index = -1;
#endif
			}
		}
		/*make sure buffers not put back to bufmgr when
			vf_put is called*/
		if (hw->buffer_spec[i].used == 2)
			hw->buffer_spec[i].used = 3;

		/* ready to release "free buffers"
		*/
		if (hw->buffer_spec[i].used == 0)
			hw->buffer_spec[i].used = 4;

		hw->buffer_spec[i].canvas_pos = -1;

		if (hw->buffer_spec[i].used == 4 &&
			hw->buffer_spec[i].vf_ref != 0 &&
			hw->buffer_spec[i].cma_alloc_addr) {
			hw->buffer_spec[i].used = 3;
		}
	}
	spin_unlock_irqrestore(&hw->bufspec_lock, flags);
	hw->has_i_frame = 0;
	hw->config_bufmgr_done = 0;

	if (hw->is_used_v4l) {
		mutex_lock(&vmh264_mutex);
		dealloc_buf_specs(hw, 1);
		mutex_unlock(&vmh264_mutex);
	}

	if (dpb_is_debug(DECODE_ID(hw),
		PRINT_FLAG_DUMP_BUFSPEC))
		dump_bufspec(hw, "after h264_reconfig");
}

#ifdef ERROR_HANDLE_TEST
static void h264_clear_dpb(struct vdec_h264_hw_s *hw)
{
	int i;
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
		"%s\n", __func__);
	remove_dpb_pictures(p_H264_Dpb);
	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		/*make sure buffers not put back to bufmgr when
			vf_put is called*/
		if (hw->buffer_spec[i].used == 2)
			hw->buffer_spec[i].used = 5;
	}

}
#endif

static void h264_reset_bufmgr(struct vdec_s *vdec)
{
	ulong timeout;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
#if 0
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int actual_dpb_size, max_reference_size;
	int reorder_pic_num;
	unsigned int colocated_buf_size;
	unsigned int colocated_mv_addr_start;
	unsigned int colocated_mv_addr_end;
	dpb_print(DECODE_ID(hw), 0,
	"%s\n", __func__);

	for (i = 0; i < VF_POOL_SIZE; i++)
		hw->vfpool[hw->cur_pool][i].index = -1; /* VF_BUF_NUM; */

	actual_dpb_size = p_H264_Dpb->mDPB.size;
	max_reference_size = p_H264_Dpb->max_reference_size;
	reorder_pic_num = p_H264_Dpb->reorder_pic_num;

	colocated_buf_size = p_H264_Dpb->colocated_buf_size;
	colocated_mv_addr_start = p_H264_Dpb->colocated_mv_addr_start;
	colocated_mv_addr_end  = p_H264_Dpb->colocated_mv_addr_end;

	hw->cur_pool++;
	if (hw->cur_pool >= VF_POOL_NUM)
		hw->cur_pool = 0;

	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &(hw->vfpool[hw->cur_pool][i]);
		hw->vfpool[hw->cur_pool][i].index = -1; /* VF_BUF_NUM; */
		hw->vfpool[hw->cur_pool][i].bufWidth = 1920;
		kfifo_put(&hw->newframe_q, vf);
	}

	for (i = 0; i < BUFSPEC_POOL_SIZE; i++)
		hw->buffer_spec[i].used = 0;

	dpb_init_global(&hw->dpb,
		DECODE_ID(hw), 0, 0);
	p_H264_Dpb->mDPB.size = actual_dpb_size;
	p_H264_Dpb->max_reference_size = max_reference_size;
	p_H264_Dpb->reorder_pic_num = reorder_pic_num;

	p_H264_Dpb->colocated_buf_size = colocated_buf_size;
	p_H264_Dpb->colocated_mv_addr_start = colocated_mv_addr_start;
	p_H264_Dpb->colocated_mv_addr_end  = colocated_mv_addr_end;

	p_H264_Dpb->fast_output_enable = fast_output_enable;
	hw->has_i_frame = 0;
#else
	dpb_print(DECODE_ID(hw), 0,
	"%s frame count %d to skip %d\n\n",
	__func__, hw->decode_pic_count+1,
	hw->skip_frame_count);

	flush_dpb(&hw->dpb);

	if (!hw->is_used_v4l) {
		timeout = jiffies + HZ;
		while (kfifo_len(&hw->display_q) > 0) {
			if (time_after(jiffies, timeout))
				break;
			schedule();
		}
	}

	buf_spec_init(hw, true);

	vh264_local_init(hw, true);
	/*hw->decode_pic_count = 0;
	hw->seq_info2 = 0;*/

	if (vh264_set_params(hw,
		hw->cfg_param1,
		hw->cfg_param2,
		hw->cfg_param3,
		hw->cfg_param4, true) < 0)
		hw->stat |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
	else
		hw->stat &= (~DECODER_FATAL_ERROR_SIZE_OVERFLOW);

	/*drop 3 frames after reset bufmgr if bit0 is set 1 */
	if (first_i_policy & 0x01)
		hw->first_i_policy = (3 << 8) | first_i_policy;

	p_H264_Dpb->first_insert_frame = FirstInsertFrm_RESET;

	if (hw->stat & DECODER_FATAL_ERROR_SIZE_OVERFLOW)
		hw->init_flag = 0;
	else
		hw->init_flag = 1;

	hw->reset_bufmgr_count++;
#endif
}

int ammvdec_h264_mmu_init(struct vdec_h264_hw_s *hw)
{
	int ret = -1;
	int tvp_flag = vdec_secure(hw_to_vdec(hw)) ?
		CODEC_MM_FLAGS_TVP : 0;
	int buf_size = 64;

	pr_debug("ammvdec_h264_mmu_init tvp = 0x%x mmu_enable %d\n",
			tvp_flag, hw->mmu_enable);
	hw->need_cache_size = buf_size * SZ_1M;
	hw->sc_start_time = get_jiffies_64();
	if (hw->mmu_enable && !hw->mmu_box) {
		hw->mmu_box = decoder_mmu_box_alloc_box(DRIVER_NAME,
				hw->id,
				MMU_MAX_BUFFERS,
				hw->need_cache_size,
				tvp_flag);
		if (!hw->mmu_box) {
			pr_err("h264 4k alloc mmu box failed!!\n");
			return -1;
		}
		ret = 0;
	}
	if (!hw->bmmu_box) {
		hw->bmmu_box = decoder_bmmu_box_alloc_box(
			DRIVER_NAME,
			hw->id,
			BMMU_MAX_BUFFERS,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER |
			tvp_flag,
			BMMU_ALLOC_FLAGS_WAITCLEAR);
		if (hw->bmmu_box)
			ret = 0;
	}
	return ret;
}
int ammvdec_h264_mmu_release(struct vdec_h264_hw_s *hw)
{
	if (hw->mmu_box) {
		decoder_mmu_box_free(hw->mmu_box);
		hw->mmu_box = NULL;
	}
	if (hw->bmmu_box) {
		decoder_bmmu_box_free(hw->bmmu_box);
		hw->bmmu_box = NULL;
	}
	return 0;
}

static int ammvdec_h264_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_h264_hw_s *hw = NULL;
	char *tmpbuf;
	int config_val;
	static struct vframe_operations_s vf_tmp_ops;

	if (pdata == NULL) {
		pr_info("\nammvdec_h264 memory resource undefined.\n");
		return -EFAULT;
	}

	hw = (struct vdec_h264_hw_s *)h264_alloc_hw_stru(&pdev->dev,
		sizeof(struct vdec_h264_hw_s), GFP_KERNEL);
	if (hw == NULL) {
		pr_info("\nammvdec_h264 device data allocation failed\n");
		return -ENOMEM;
	}
	hw->id = pdev->id;
	hw->platform_dev = pdev;

	snprintf(hw->trace.vdec_name, sizeof(hw->trace.vdec_name),
		"h264-%d", hw->id);
	snprintf(hw->trace.pts_name, sizeof(hw->trace.pts_name),
		"%s-pts", hw->trace.vdec_name);
	snprintf(hw->trace.new_q_name, sizeof(hw->trace.new_q_name),
		"%s-newframe_q", hw->trace.vdec_name);
	snprintf(hw->trace.disp_q_name, sizeof(hw->trace.disp_q_name),
		"%s-dispframe_q", hw->trace.vdec_name);
	snprintf(hw->trace.decode_time_name, sizeof(hw->trace.decode_time_name),
		"decoder_time%d", pdev->id);
	snprintf(hw->trace.decode_run_time_name, sizeof(hw->trace.decode_run_time_name),
		"decoder_run_time%d", pdev->id);
	snprintf(hw->trace.decode_header_time_name, sizeof(hw->trace.decode_header_time_name),
		"decoder_header_time%d", pdev->id);
	snprintf(hw->trace.decode_work_time_name, sizeof(hw->trace.decode_work_time_name),
		"decoder_work_time%d", pdev->id);

	/* the ctx from v4l2 driver. */
	hw->v4l2_ctx = pdata->private;

	platform_set_drvdata(pdev, pdata);

	hw->mmu_enable = 0;
	hw->first_head_check_flag = 0;

	if (pdata->sys_info)
		hw->vh264_amstream_dec_info = *pdata->sys_info;

	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5)
		force_enable_mmu = 1;

	if (force_enable_mmu && pdata->sys_info &&
		    (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_TXLX) &&
		    (get_cpu_major_id() != AM_MESON_CPU_MAJOR_ID_GXLX) &&
			(pdata->sys_info->height * pdata->sys_info->width
			> 1920 * 1088))
			hw->mmu_enable = 1;

	if (hw->mmu_enable &&
		(pdata->frame_base_video_path == FRAME_BASE_PATH_IONVIDEO)) {
		hw->mmu_enable = 0;
		pr_info("ionvideo needs disable mmu, path= %d \n",
				pdata->frame_base_video_path);
	}

	if (ammvdec_h264_mmu_init(hw)) {
		h264_free_hw_stru(&pdev->dev, (void *)hw);
		pr_info("\nammvdec_h264 mmu alloc failed!\n");
		return -ENOMEM;
	}

	if (pdata->config_len) {
		dpb_print(DECODE_ID(hw), 0, "pdata->config=%s\n", pdata->config);
		/*use ptr config for doubel_write_mode, etc*/
		if (get_config_int(pdata->config,
			"mh264_double_write_mode", &config_val) == 0)
			hw->double_write_mode = config_val;
		else
			hw->double_write_mode = double_write_mode;

		if (get_config_int(pdata->config,
			"parm_v4l_codec_enable",
			&config_val) == 0)
			hw->is_used_v4l = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_buffer_margin",
			&config_val) == 0)
			hw->reorder_dpb_size_margin = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_canvas_mem_mode",
			&config_val) == 0)
			hw->canvas_mode = config_val;
		if (get_config_int(pdata->config,
			"parm_v4l_low_latency_mode",
			&config_val) == 0)
			hw->low_latency_mode = config_val ? 0x8:0;
		if (get_config_int(pdata->config, "sidebind_type",
				&config_val) == 0)
			hw->sidebind_type = config_val;

		if (get_config_int(pdata->config, "sidebind_channel_id",
				&config_val) == 0)
			hw->sidebind_channel_id = config_val;

		if (get_config_int(pdata->config,
			"parm_enable_fence",
			&config_val) == 0)
			hw->enable_fence = config_val;

		if (get_config_int(pdata->config,
			"parm_fence_usage",
			&config_val) == 0)
			hw->fence_usage = config_val;

		if (get_config_int(pdata->config,
			"negative_dv",
			&config_val) == 0) {
			hw->discard_dv_data = config_val;
			dpb_print(DECODE_ID(hw), 0, "discard dv data\n");
		}

		if (get_config_int(pdata->config,
			"parm_metadata_config_flag",
			&config_val) == 0) {
			hw->metadata_config_flag = config_val;
			hw->high_bandwidth_flag = config_val & VDEC_CFG_FLAG_HIGH_BANDWIDTH;
			if (hw->high_bandwidth_flag)
				dpb_print(DECODE_ID(hw), 0, "high bandwidth\n");
		}

		if (get_config_int(pdata->config,
			"api_error_policy", &config_val) == 0) {
			if (config_val == 0) {
				hw->error_proc_policy = H264_ERROR_FRAME_DISPLAY;
			} else if (config_val == 1) {
				hw->error_proc_policy = H264_ERROR_FRAME_DROP;
				mb_count_threshold = 0;
			} else {
				hw->error_proc_policy = error_proc_policy;
			}
		} else {
			hw->error_proc_policy = error_proc_policy;
		}
	} else {
		hw->double_write_mode = double_write_mode;
		hw->error_proc_policy = error_proc_policy;
	}


	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5)
		hw->double_write_mode = 3;

	if (force_config_fence) {
		hw->enable_fence = true;
		hw->fence_usage = (force_config_fence >> 4) & 0xf;
		if (force_config_fence & 0x2)
			hw->enable_fence = false;
		dpb_print(DECODE_ID(hw), 0,
			"enable fence: %d, fence usage: %d\n",
			hw->enable_fence, hw->fence_usage);
	}

	if (!hw->is_used_v4l) {
		hw->reorder_dpb_size_margin = reorder_dpb_size_margin;
		hw->canvas_mode = mem_map_mode;

		if ((h264_debug_flag & IGNORE_PARAM_FROM_CONFIG) == 0)
			hw->canvas_mode = pdata->canvas_mode;
	}

	if (hw->mmu_enable) {
			hw->canvas_mode = CANVAS_BLKMODE_LINEAR;
			hw->double_write_mode &= 0xffff;
	}

	if (pdata->parallel_dec == 1) {
		int i;
		for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
			hw->buffer_spec[i].y_canvas_index = -1;
			hw->buffer_spec[i].u_canvas_index = -1;
			hw->buffer_spec[i].v_canvas_index = -1;
#ifdef VDEC_DW
			if (IS_VDEC_DW(hw)) {
				hw->buffer_spec[i].vdec_dw_y_canvas_index = -1;
				hw->buffer_spec[i].vdec_dw_u_canvas_index = -1;
				hw->buffer_spec[i].vdec_dw_v_canvas_index = -1;
			}
#endif
		}
	}

	dpb_print(DECODE_ID(hw), 0,
		"%s mmu_enable %d double_write_mode 0x%x\n",
		__func__, hw->mmu_enable, hw->double_write_mode);

	pdata->private = hw;
	pdata->dec_status = dec_status;
	pdata->set_trickmode = vmh264_set_trickmode;
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vh264_isr;
	pdata->threaded_irq_handler = vh264_isr_thread_fn;
	pdata->dump_state = vmh264_dump_state;

#ifdef MH264_USERDATA_ENABLE
	pdata->wakeup_userdata_poll = vmh264_wakeup_userdata_poll;
	pdata->user_data_read = vmh264_user_data_read;
	pdata->reset_userdata_fifo = vmh264_reset_userdata_fifo;
#else
	pdata->wakeup_userdata_poll = NULL;
	pdata->user_data_read = NULL;
	pdata->reset_userdata_fifo = NULL;
#endif
	if (pdata->use_vfm_path) {
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			VFM_DEC_PROVIDER_NAME);
		hw->frameinfo_enable = 1;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	else if (vdec_dual(pdata)) {
		if (!pdata->is_stream_mode_dv_multi) {
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
		} else {
			if (dv_toggle_prov_name) /*debug purpose*/
				snprintf(pdata->vf_provider_name,
				VDEC_PROVIDER_NAME_SIZE,
					(pdata->master) ? VFM_DEC_DVBL_PROVIDER_NAME2 :
					VFM_DEC_DVEL_PROVIDER_NAME2);
			else
				snprintf(pdata->vf_provider_name,
				VDEC_PROVIDER_NAME_SIZE,
					(pdata->master) ? VFM_DEC_DVEL_PROVIDER_NAME2 :
					VFM_DEC_DVBL_PROVIDER_NAME2);
		}
	}
#endif
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			PROVIDER_NAME ".%02x", pdev->id & 0xff);

	if (!hw->is_used_v4l) {
		memcpy(&vf_tmp_ops, &vf_provider_ops, sizeof(struct vframe_operations_s));

		if (without_display_mode == 1) {
			vf_tmp_ops.get = NULL;
		}

		vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
			&vf_tmp_ops, pdata);
	}

	platform_set_drvdata(pdev, pdata);

	buf_spec_init(hw, false);

	hw->platform_dev = pdev;

#ifdef DUMP_USERDATA_RECORD
	vmh264_init_userdata_dump();
	vmh264_reset_user_data_buf();
#endif
	if (decoder_bmmu_box_alloc_buf_phy(hw->bmmu_box, BMMU_DPB_IDX,
		V_BUF_ADDR_OFFSET, DRIVER_NAME, &hw->cma_alloc_addr) < 0) {
		h264_free_hw_stru(&pdev->dev, (void *)hw);
		pdata->dec_status = NULL;
		return -ENOMEM;
	}

	hw->buf_offset = hw->cma_alloc_addr - DEF_BUF_START_ADDR +
			DCAC_READ_MARGIN;
	if (hw->mmu_enable) {
		u32 extif_size = EXTIF_BUF_SIZE;
		if (get_cpu_major_id() >=  AM_MESON_CPU_MAJOR_ID_G12A)
			extif_size <<= 1;
		if (decoder_bmmu_box_alloc_buf_phy(hw->bmmu_box, BMMU_EXTIF_IDX,
			extif_size, DRIVER_NAME, &hw->extif_addr) < 0) {
			h264_free_hw_stru(&pdev->dev, (void *)hw);
			pdata->dec_status = NULL;
			return -ENOMEM;
		}
	}
	if (!vdec_secure(pdata)) {
#if 1
		/*init internal buf*/
		tmpbuf = (char *)codec_mm_phys_to_virt(hw->cma_alloc_addr);
		if (tmpbuf) {
			memset(tmpbuf, 0, V_BUF_ADDR_OFFSET);
			codec_mm_dma_flush(tmpbuf,
				V_BUF_ADDR_OFFSET,
				DMA_TO_DEVICE);
		} else {
			tmpbuf = codec_mm_vmap(hw->cma_alloc_addr,
				V_BUF_ADDR_OFFSET);
			if (tmpbuf) {
				memset(tmpbuf, 0, V_BUF_ADDR_OFFSET);
				codec_mm_dma_flush(tmpbuf,
					V_BUF_ADDR_OFFSET,
					DMA_TO_DEVICE);
				codec_mm_unmap_phyaddr(tmpbuf);
			}
		}
#else
		/*init sps/pps internal buf 64k*/
		tmpbuf = (char *)codec_mm_phys_to_virt(hw->cma_alloc_addr
			+ (mem_sps_base - DEF_BUF_START_ADDR));
		memset(tmpbuf, 0, 0x10000);
		dma_sync_single_for_device(amports_get_dma_device(),
			hw->cma_alloc_addr +
			(mem_sps_base - DEF_BUF_START_ADDR),
			0x10000, DMA_TO_DEVICE);
#endif
	}
	/**/

#if 0
	if (NULL == hw->sei_data_buffer) {
		hw->sei_data_buffer =
			dma_alloc_coherent(amports_get_dma_device(),
				USER_DATA_SIZE,
				&hw->sei_data_buffer_phys, GFP_KERNEL);
		if (!hw->sei_data_buffer) {
			pr_info("%s: Can not allocate sei_data_buffer\n",
				   __func__);
			ammvdec_h264_mmu_release(hw);
			h264_free_hw_stru(&pdev->dev, (void *)hw);
			return -ENOMEM;
		}
		/* pr_info("buffer 0x%x, phys 0x%x, remap 0x%x\n",
		   sei_data_buffer, sei_data_buffer_phys,
		   (u32)sei_data_buffer_remap); */
	}
#endif
	dpb_print(DECODE_ID(hw), 0, "ammvdec_h264 mem-addr=%lx,buff_offset=%x,buf_start=%lx\n",
		pdata->mem_start, hw->buf_offset, hw->cma_alloc_addr);

	vdec_source_changed(VFORMAT_H264, 3840, 2160, 60);

	if (hw->mmu_enable)
		hevc_source_changed(VFORMAT_HEVC, 3840, 2160, 60);

	if (vh264_init(hw) < 0) {
		pr_info("\nammvdec_h264 init failed.\n");
		ammvdec_h264_mmu_release(hw);
		h264_free_hw_stru(&pdev->dev, (void *)hw);
		pdata->dec_status = NULL;
		return -ENODEV;
	}
#ifdef MH264_USERDATA_ENABLE
	vmh264_crate_userdata_manager(hw,
			hw->sei_user_data_buffer,
			USER_DATA_SIZE);
#endif

#ifdef AUX_DATA_CRC
	vdec_aux_data_check_init(pdata);
#endif

	vdec_set_prepare_level(pdata, start_decode_buf_level);
	if (pdata->parallel_dec == 1) {
		if (hw->mmu_enable == 0)
			vdec_core_request(pdata, CORE_MASK_VDEC_1);
		else {
			vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
				| CORE_MASK_COMBINE);
		}
	} else
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
				| CORE_MASK_COMBINE);

	atomic_set(&hw->vh264_active, 1);
	vdec_set_vframe_comm(pdata, DRIVER_NAME);
	display_frame_count[DECODE_ID(hw)] = 0;
	decode_frame_count[DECODE_ID(hw)] = 0;
	hw->dpb.without_display_mode = without_display_mode;
	mutex_init(&hw->fence_mutex);
	mutex_init(&hw->pic_mutex);
	if (hw->enable_fence) {
		pdata->sync = vdec_sync_get();
		if (!pdata->sync) {
			dpb_print(DECODE_ID(hw), 0, "alloc fence timeline error\n");
			ammvdec_h264_mmu_release(hw);
			h264_free_hw_stru(&pdev->dev, (void *)hw);
			pdata->dec_status = NULL;
			return -ENODEV;
		}
		pdata->sync->usage = hw->fence_usage;
		/* creat timeline. */
		vdec_timeline_create(pdata->sync, DRIVER_NAME);
	}

	return 0;
}

static void vdec_fence_release(struct vdec_h264_hw_s *hw,
			       struct vdec_sync *sync)
{
	ulong expires;

	/* clear display pool. */
	clear_refer_bufs(hw);

	/* notify signal to wake up all fences. */
	vdec_timeline_increase(sync, VF_POOL_SIZE);

	expires = jiffies + msecs_to_jiffies(2000);
	while (!check_objs_all_signaled(sync)) {
		if (time_after(jiffies, expires)) {
			pr_err("wait fence signaled timeout.\n");
			break;
		}
	}

	pr_info("fence start release\n");

	/* decreases refcnt of timeline. */
	vdec_timeline_put(sync);
}

static int ammvdec_h264_remove(struct platform_device *pdev)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	int i;

	struct vdec_s *vdec = hw_to_vdec(hw);

	if (vdec->next_status == VDEC_STATUS_DISCONNECTED
				&& (vdec->status == VDEC_STATUS_ACTIVE)) {
			dpb_print(DECODE_ID(hw), 0,
				"%s  force exit %d\n", __func__, __LINE__);
			hw->dec_result = DEC_RESULT_FORCE_EXIT;
			vdec_schedule_work(&hw->work);
			wait_event_interruptible_timeout(hw->wait_q,
				(vdec->status == VDEC_STATUS_CONNECTED),
				msecs_to_jiffies(1000));  /* wait for work done */
	}

	for (i = 0; i < BUFSPEC_POOL_SIZE; i++)
		release_aux_data(hw, i);

	atomic_set(&hw->vh264_active, 0);

	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->check_timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}

	vh264_stop(hw);
#ifdef MH264_USERDATA_ENABLE
#ifdef DUMP_USERDATA_RECORD
	vmh264_dump_userdata();
#endif
	vmh264_destroy_userdata_manager(hw);
#endif
	/* vdec_source_changed(VFORMAT_H264, 0, 0, 0); */

#ifdef AUX_DATA_CRC
	vdec_aux_data_check_exit(vdec);
#endif

	atomic_set(&hw->vh264_active, 0);
	if (vdec->parallel_dec == 1) {
		if (hw->mmu_enable == 0)
			vdec_core_release(vdec, CORE_MASK_VDEC_1);
		else
			vdec_core_release(vdec, CORE_MASK_VDEC_1 | CORE_MASK_HEVC |
				CORE_MASK_COMBINE);
	} else
		vdec_core_release(hw_to_vdec(hw), CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_DISCONNECTED);
	if (vdec->parallel_dec == 1) {
		for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
			vdec->free_canvas_ex(hw->buffer_spec[i].y_canvas_index, vdec->id);
			vdec->free_canvas_ex(hw->buffer_spec[i].u_canvas_index, vdec->id);
			vdec->free_canvas_ex(hw->buffer_spec[i].v_canvas_index, vdec->id);
			if (IS_VDEC_DW(hw)) {
				vdec->free_canvas_ex(hw->buffer_spec[i].vdec_dw_y_canvas_index, vdec->id);
				vdec->free_canvas_ex(hw->buffer_spec[i].vdec_dw_u_canvas_index, vdec->id);
				vdec->free_canvas_ex(hw->buffer_spec[i].vdec_dw_v_canvas_index, vdec->id);
			}
		}
	}

	if (hw->enable_fence)
		vdec_fence_release(hw, vdec->sync);

	ammvdec_h264_mmu_release(hw);
	h264_free_hw_stru(&pdev->dev, (void *)hw);
	clk_adj_frame_count = 0;

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

static struct mconfig hm264_configs[] = {
	MC_PU32("h264_debug_flag", &h264_debug_flag),
	MC_PI32("start_decode_buf_level", &start_decode_buf_level),
	MC_PU32("fixed_frame_rate_mode", &fixed_frame_rate_mode),
	MC_PU32("decode_timeout_val", &decode_timeout_val),
	MC_PU32("reorder_dpb_size_margin", &reorder_dpb_size_margin),
	MC_PU32("reference_buf_margin", &reference_buf_margin),
	MC_PU32("radr", &radr),
	MC_PU32("rval", &rval),
	MC_PU32("h264_debug_mask", &h264_debug_mask),
	MC_PU32("h264_debug_cmd", &h264_debug_cmd),
	MC_PI32("force_rate_streambase", &force_rate_streambase),
	MC_PI32("dec_control", &dec_control),
	MC_PI32("force_rate_framebase", &force_rate_framebase),
	MC_PI32("force_disp_bufspec_num", &force_disp_bufspec_num),
	MC_PU32("prefix_aux_buf_size", &prefix_aux_buf_size),
	MC_PU32("suffix_aux_buf_size", &suffix_aux_buf_size),
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	MC_PU32("reorder_dpb_size_margin_dv", &reorder_dpb_size_margin_dv),
	MC_PU32("dv_toggle_prov_name", &dv_toggle_prov_name),
	MC_PU32("dolby_meta_with_el", &dolby_meta_with_el),
#endif
	MC_PU32("i_only_flag", &i_only_flag),
	MC_PU32("force_rate_streambase", &force_rate_streambase),
};
static struct mconfig_node hm264_node;


static int __init ammvdec_h264_driver_init_module(void)
{

	pr_info("ammvdec_h264 module init\n");
	if (platform_driver_register(&ammvdec_h264_driver)) {
		pr_info("failed to register ammvdec_h264 driver\n");
		return -ENODEV;
	}

	if (vdec_is_support_4k()) {
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_TXLX) {
			ammvdec_h264_profile.profile =
					"4k, dwrite, compressed, frame_dv, fence, v4l, multi_frame_dv";
		} else if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_GXTVBB) {
			ammvdec_h264_profile.profile = "4k, frame_dv, fence, v4l, multi_frame_dv";
		}
	} else {
		if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T5D || is_cpu_s4_s805x2()) {
			ammvdec_h264_profile.profile =
						"dwrite, compressed, frame_dv, v4l, multi_frame_dv";
		} else {
			ammvdec_h264_profile.profile =
				        "dwrite, compressed, v4l";
		}
	}

	vcodec_profile_register(&ammvdec_h264_profile);
	INIT_REG_NODE_CONFIGS("media.decoder", &hm264_node,
	"mh264", hm264_configs, CONFIG_FOR_RW);

	vcodec_feature_register(VFORMAT_H264, 0);
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

module_param(start_decode_buf_level, int, 0664);
MODULE_PARM_DESC(start_decode_buf_level,
		"\n ammvdec_h264 start_decode_buf_level\n");

module_param(pre_decode_buf_level, int, 0664);
MODULE_PARM_DESC(pre_decode_buf_level, "\n ammvdec_h264 pre_decode_buf_level\n");

module_param(fixed_frame_rate_mode, uint, 0664);
MODULE_PARM_DESC(fixed_frame_rate_mode, "\namvdec_h264 fixed_frame_rate_mode\n");

module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val, "\n amvdec_h264 decode_timeout_val\n");

module_param(errordata_timeout_val, uint, 0664);
MODULE_PARM_DESC(errordata_timeout_val, "\n amvdec_h264 errordata_timeout_val\n");

module_param(get_data_timeout_val, uint, 0664);
MODULE_PARM_DESC(get_data_timeout_val, "\n amvdec_h264 get_data_timeout_val\n");

module_param(frame_max_data_packet, uint, 0664);
MODULE_PARM_DESC(frame_max_data_packet, "\n amvdec_h264 frame_max_data_packet\n");

module_param(reorder_dpb_size_margin, uint, 0664);
MODULE_PARM_DESC(reorder_dpb_size_margin, "\n ammvdec_h264 reorder_dpb_size_margin\n");

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
module_param(reorder_dpb_size_margin_dv, uint, 0664);
MODULE_PARM_DESC(reorder_dpb_size_margin_dv,
	"\n ammvdec_h264 reorder_dpb_size_margin_dv\n");
#endif

module_param(reference_buf_margin, uint, 0664);
MODULE_PARM_DESC(reference_buf_margin, "\n ammvdec_h264 reference_buf_margin\n");

#ifdef CONSTRAIN_MAX_BUF_NUM
module_param(run_ready_max_vf_only_num, uint, 0664);
MODULE_PARM_DESC(run_ready_max_vf_only_num, "\n run_ready_max_vf_only_num\n");

module_param(run_ready_display_q_num, uint, 0664);
MODULE_PARM_DESC(run_ready_display_q_num, "\n run_ready_display_q_num\n");

module_param(run_ready_max_buf_num, uint, 0664);
MODULE_PARM_DESC(run_ready_max_buf_num, "\n run_ready_max_buf_num\n");
#endif

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

module_param(force_disp_bufspec_num, int, 0664);
MODULE_PARM_DESC(force_disp_bufspec_num, "\n amvdec_h264 force_disp_bufspec_num\n");

module_param(V_BUF_ADDR_OFFSET, int, 0664);
MODULE_PARM_DESC(V_BUF_ADDR_OFFSET, "\n amvdec_h264 V_BUF_ADDR_OFFSET\n");

module_param(prefix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(prefix_aux_buf_size, "\n prefix_aux_buf_size\n");

module_param(suffix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(suffix_aux_buf_size, "\n suffix_aux_buf_size\n");

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
module_param(dv_toggle_prov_name, uint, 0664);
MODULE_PARM_DESC(dv_toggle_prov_name, "\n dv_toggle_prov_name\n");

module_param(dolby_meta_with_el, uint, 0664);
MODULE_PARM_DESC(dolby_meta_with_el, "\n dolby_meta_with_el\n");

#endif

module_param(fast_output_enable, uint, 0664);
MODULE_PARM_DESC(fast_output_enable, "\n amvdec_h264 fast_output_enable\n");

module_param(error_proc_policy, uint, 0664);
MODULE_PARM_DESC(error_proc_policy, "\n amvdec_h264 error_proc_policy\n");

module_param(error_skip_count, uint, 0664);
MODULE_PARM_DESC(error_skip_count, "\n amvdec_h264 error_skip_count\n");

module_param(force_sliding_margin, uint, 0664);
MODULE_PARM_DESC(force_sliding_margin, "\n amvdec_h264 force_sliding_margin\n");

module_param(i_only_flag, uint, 0664);
MODULE_PARM_DESC(i_only_flag, "\n amvdec_h264 i_only_flag\n");

module_param(first_i_policy, uint, 0664);
MODULE_PARM_DESC(first_i_policy, "\n amvdec_h264 first_i_policy\n");

module_param(frmbase_cont_bitlevel, uint, 0664);
MODULE_PARM_DESC(frmbase_cont_bitlevel,
	"\n amvdec_h264 frmbase_cont_bitlevel\n");

module_param(frmbase_cont_bitlevel2, uint, 0664);
MODULE_PARM_DESC(frmbase_cont_bitlevel2,
	"\n amvdec_h264 frmbase_cont_bitlevel\n");

module_param(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n amvdec_mh264 udebug_flag\n");

module_param(udebug_pause_pos, uint, 0664);
MODULE_PARM_DESC(udebug_pause_pos, "\n udebug_pause_pos\n");

module_param(udebug_pause_val, uint, 0664);
MODULE_PARM_DESC(udebug_pause_val, "\n udebug_pause_val\n");

module_param(udebug_pause_decode_idx, uint, 0664);
MODULE_PARM_DESC(udebug_pause_decode_idx, "\n udebug_pause_decode_idx\n");

module_param(max_alloc_buf_count, uint, 0664);
MODULE_PARM_DESC(max_alloc_buf_count, "\n amvdec_h264 max_alloc_buf_count\n");

module_param(enable_itu_t35, uint, 0664);
MODULE_PARM_DESC(enable_itu_t35, "\n amvdec_h264 enable_itu_t35\n");

module_param(endian, uint, 0664);
MODULE_PARM_DESC(endian, "\nrval\n");

module_param(mmu_enable, uint, 0664);
MODULE_PARM_DESC(mmu_enable, "\n mmu_enable\n");

module_param(force_enable_mmu, uint, 0664);
MODULE_PARM_DESC(force_enable_mmu, "\n force_enable_mmu\n");

module_param(again_threshold, uint, 0664);
MODULE_PARM_DESC(again_threshold, "\n again_threshold\n");

module_param(stream_mode_start_num, uint, 0664);
MODULE_PARM_DESC(stream_mode_start_num, "\n stream_mode_start_num\n");

module_param(colocate_old_cal, uint, 0664);
MODULE_PARM_DESC(colocate_old_cal, "\n amvdec_mh264 colocate_old_cal\n");

/*
module_param(trigger_task, uint, 0664);
MODULE_PARM_DESC(trigger_task, "\n amvdec_h264 trigger_task\n");
*/
module_param_array(decode_frame_count, uint, &max_decode_instance_num, 0664);

module_param_array(display_frame_count, uint, &max_decode_instance_num, 0664);

module_param_array(max_process_time, uint, &max_decode_instance_num, 0664);

module_param_array(run_count, uint,
	&max_decode_instance_num, 0664);

module_param_array(not_run_ready, uint,
	&max_decode_instance_num, 0664);

module_param_array(input_empty, uint,
	&max_decode_instance_num, 0664);

module_param_array(max_get_frame_interval, uint,
	&max_decode_instance_num, 0664);

module_param_array(step, uint, &max_decode_instance_num, 0664);

module_param_array(ref_frame_mark_flag, uint, &max_decode_instance_num, 0664);

module_param(disp_vframe_valve_level, uint, 0664);
MODULE_PARM_DESC(disp_vframe_valve_level, "\n disp_vframe_valve_level\n");

module_param(double_write_mode, uint, 0664);
MODULE_PARM_DESC(double_write_mode, "\n double_write_mode\n");

module_param(mem_map_mode, uint, 0664);
MODULE_PARM_DESC(mem_map_mode, "\n mem_map_mode\n");

module_param(without_display_mode, uint, 0664);
MODULE_PARM_DESC(without_display_mode, "\n without_display_mode\n");

module_param(check_slice_num, uint, 0664);
MODULE_PARM_DESC(check_slice_num, "\n check_slice_num\n");

module_param(mb_count_threshold, uint, 0664);
MODULE_PARM_DESC(mb_count_threshold, "\n mb_count_threshold\n");

module_param(loop_playback_poc_threshold, int, 0664);
MODULE_PARM_DESC(loop_playback_poc_threshold, "\n loop_playback_poc_threshold\n");

module_param(poc_threshold, int, 0664);
MODULE_PARM_DESC(poc_threshold, "\n poc_threshold\n");

module_param(force_config_fence, uint, 0664);
MODULE_PARM_DESC(force_config_fence, "\n force enable fence\n");

module_param(dirty_again_threshold, uint, 0664);
MODULE_PARM_DESC(dirty_again_threshold, "\n amvdec_h264 dirty_again_threshold\n");

module_init(ammvdec_h264_driver_init_module);
module_exit(ammvdec_h264_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC H264 Video Decoder Driver");
MODULE_LICENSE("GPL");
