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
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/amlogic/codec_mm/configs.h>
#include "decoder/decoder_bmmu_box.h"
#include "decoder/decoder_mmu_box.h"
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
#define MEM_NAME "codec_m264"
#define MULTI_INSTANCE_FRAMEWORK
/*#define ONE_COLOCATE_BUF_PER_DECODE_BUF*/
#include "h264_dpb.h"
/* #define SEND_PARAM_WITH_REG */

#define DRIVER_NAME "ammvdec_h264"
#define MODULE_NAME "ammvdec_h264"
#define DRIVER_HEADER_NAME "ammvdec_h264_header"

#define CHECK_INTERVAL        (HZ/100)

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

#define H264_DEV_NUM        9

#define H264_MMU
static int mmu_enable;
static int force_enable_mmu = 1;
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
static unsigned int force_disp_bufspec_num;
static unsigned int fixed_frame_rate_mode;
static unsigned int error_recovery_mode_in;
static int start_decode_buf_level = 0x8000;

#ifdef CONFIG_AM_VDEC_DV
/*to make reorder size difference of bl and el not too big*/
static unsigned int reorder_dpb_size_margin_dv = 16;
#endif
static unsigned int reorder_dpb_size_margin = 6;
static unsigned int reference_buf_margin = 4;

static unsigned int max_alloc_buf_count;
static unsigned int decode_timeout_val = 100;
static unsigned int get_data_timeout_val = 2000;
static unsigned int frame_max_data_packet = 8;

static unsigned int radr;
static unsigned int rval;

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

static unsigned int max_decode_instance_num = H264_DEV_NUM;
static unsigned int decode_frame_count[H264_DEV_NUM];
static unsigned int display_frame_count[H264_DEV_NUM];
static unsigned int max_process_time[H264_DEV_NUM];
static unsigned int max_get_frame_interval[H264_DEV_NUM];
static unsigned int run_count[H264_DEV_NUM];
static unsigned int input_empty[H264_DEV_NUM];
static unsigned int not_run_ready[H264_DEV_NUM];
		/* bit[3:0]:
		0, run ; 1, pause; 3, step
		bit[4]:
		1, schedule run
		*/
static unsigned int step[H264_DEV_NUM];

#define AUX_BUF_ALIGN(adr) ((adr + 0xf) & (~0xf))
static u32 prefix_aux_buf_size = (16 * 1024);
static u32 suffix_aux_buf_size;

#ifdef CONFIG_AM_VDEC_DV
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

	bit[8] check total mbx/mby of decoded frame
	bit[9] check ERROR_STATUS_REG

	bit[12] i_only when error happen
*/
static unsigned int error_proc_policy = 0x1214; /*0xc;*/

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
static unsigned int first_i_policy = (15 << 8) | 2;

/*
	fast_output_enable:
	bit [0], output frame if there is IDR in list
	bit [1], output frame if the current poc is 1 big than the previous poc
	bit [2], if even poc only, output frame ifthe cuurent poc
			is 2 big than the previous poc
*/
static unsigned int fast_output_enable = 3;

static unsigned int enable_itu_t35 = 1;

#define is_in_parsing_state(status) \
		((status == H264_ACTION_SEARCH_HEAD) || \
			((status & 0xf0) == 0x80))

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

#define DEF_BUF_START_ADDR			0x01000000
#define mem_sps_base				0x011c3c00
#define mem_pps_base				0x011cbc00
/*#define V_BUF_ADDR_OFFSET             (0x13e000)*/
u32 V_BUF_ADDR_OFFSET = 0x200000;
#define DCAC_READ_MARGIN	(64 * 1024)
#define PIC_SINGLE_FRAME        0
#define PIC_TOP_BOT_TOP         1
#define PIC_BOT_TOP_BOT         2
#define PIC_DOUBLE_FRAME        3
#define PIC_TRIPLE_FRAME        4
#define PIC_TOP_BOT              5
#define PIC_BOT_TOP              6
#define PIC_INVALID              7

#define EXTEND_SAR                      0xff

#define BUFSPEC_POOL_SIZE		64
#define VF_POOL_SIZE        64
#define MAX_VF_BUF_NUM          27
#define BMMU_MAX_BUFFERS	(BUFSPEC_POOL_SIZE + 3)
#define BMMU_REF_IDX	(BUFSPEC_POOL_SIZE)
#define BMMU_DPB_IDX	(BUFSPEC_POOL_SIZE + 1)
#define BMMU_EXTIF_IDX	(BUFSPEC_POOL_SIZE + 2)
#define EXTIF_BUF_SIZE   0x10000

#define HEADER_BUFFER_IDX(n) (n)
#define VF_BUFFER_IDX(n)	(n)


#define PUT_INTERVAL        (HZ/100)
#define NO_DISP_WD_COUNT    (3 * HZ / PUT_INTERVAL)

#define MMU_MAX_BUFFERS	BUFSPEC_POOL_SIZE
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
	int used;
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
	unsigned long cma_alloc_addr;
	unsigned int buf_adr;
#ifdef	H264_MMU
	unsigned long alloc_header_addr;
#endif
	char *aux_data_buf;
	int aux_data_size;
#ifdef CONFIG_AM_VDEC_DV
	unsigned char dv_enhance_exist;
#endif
	int canvas_pos;
	int vf_ref;
};

#define AUX_DATA_SIZE(pic) (hw->buffer_spec[pic->buf_spec_num].aux_data_size)
#define AUX_DATA_BUF(pic) (hw->buffer_spec[pic->buf_spec_num].aux_data_buf)
#define DEL_EXIST(h, p) (h->buffer_spec[p->buf_spec_num].dv_enhance_exist)

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
static void vh264_notify_work(struct work_struct *work);
static void user_data_push_work(struct work_struct *work);

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
	*/
#define NAL_SEARCH_CTL		AV_SCRATCH_9
#define MBY_MBX                 MB_MOTION_MODE /*0xc07*/

#define DECODE_MODE_SINGLE					0x0
#define DECODE_MODE_MULTI_FRAMEBASE			0x1
#define DECODE_MODE_MULTI_STREAMBASE		0x2
#define DECODE_MODE_MULTI_DVBAL				0x3
#define DECODE_MODE_MULTI_DVENL				0x4
static DEFINE_MUTEX(vmh264_mutex);

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
	/* buffer for storing one itu35 recored */
	void *sei_itu_data_buf;
	u32 sei_itu_data_len;

	/* recycle buffer for user data storing all itu35 records */
	void *sei_user_data_buffer;
	u32 sei_user_data_wp;
	int sei_poc;
	struct work_struct user_data_work;
	struct StorablePicture *last_dec_picture;

	ulong lmem_addr;
	dma_addr_t lmem_addr_remap;

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

#endif

	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);

	struct vframe_s vfpool[VF_POOL_SIZE];
	struct buffer_spec_s buffer_spec[BUFSPEC_POOL_SIZE];
	struct vframe_s switching_fense_vf;
	struct h264_dpb_stru dpb;
	u8 init_flag;
	u8 has_i_frame;
	u8 config_bufmgr_done;
	u32 max_reference_size;
	u32 decode_pic_count;
	int start_search_pos;
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
	u32 video_signal_from_vui; /*to do .. */
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
	struct work_struct work;
	struct work_struct notify_work;

	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;

	struct timer_list check_timer;

	/**/
	unsigned last_frame_time;
	u32 vf_pre_count;
	u32 vf_get_count;
	u32 vf_put_count;

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

#ifdef CONFIG_AM_VDEC_DV
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
};


static void h264_reconfig(struct vdec_h264_hw_s *hw);
static void h264_reset_bufmgr(struct vdec_h264_hw_s *hw);
static void vh264_local_init(struct vdec_h264_hw_s *hw);
static int vh264_hw_ctx_restore(struct vdec_h264_hw_s *hw);
static int vh264_stop(struct vdec_h264_hw_s *hw);
static s32 vh264_init(struct vdec_h264_hw_s *hw);
static void set_frame_info(struct vdec_h264_hw_s *hw, struct vframe_s *vf,
			u32 index);
static void release_aux_data(struct vdec_h264_hw_s *hw,
	int buf_spec_num);

#define		H265_PUT_SAO_4K_SET			0x03
#define		H265_ABORT_SAO_4K_SET			0x04
#define		H265_ABORT_SAO_4K_SET_DONE		0x05

#define		SYS_COMMAND			HEVC_ASSIST_SCRATCH_0
#define		H265_CHECK_AXI_INFO_BASE	HEVC_ASSIST_SCRATCH_8
#define		H265_SAO_4K_SET_BASE	HEVC_ASSIST_SCRATCH_9
#define		H265_SAO_4K_SET_COUNT	HEVC_ASSIST_SCRATCH_A
#define		HEVC_SAO_MMU_STATUS			0x3639
#define		HEVCD_MPP_ANC2AXI_TBL_DATA		0x3464

#define		HEVC_CM_HEADER_START_ADDR		0x3628
#define		HEVC_CM_BODY_START_ADDR			0x3626
#define		HEVC_CM_BODY_LENGTH			0x3627
#define		HEVC_CM_HEADER_LENGTH			0x3629
#define		HEVC_CM_HEADER_OFFSET			0x362b
#define		HEVC_SAO_CTRL9				0x362d
#define		HEVCD_MPP_DECOMP_CTL3			0x34c4
#define		HEVCD_MPP_VDEC_MCR_CTL			0x34c8


#define H265_DW_NO_SCALE
#define H265_MEM_MAP_MODE 0  /*0:linear 1:32x32 2:64x32*/
#define H265_LOSLESS_COMPRESS_MODE
#define MAX_FRAME_4K_NUM 0x1200
#define FRAME_MMU_MAP_SIZE  (MAX_FRAME_4K_NUM * 4)

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

static void    hevc_mcr_config_canv2axitbl(struct vdec_h264_hw_s *hw)
{
	int i, size;
	u32   canvas_addr;
	unsigned long maddr;
	int     num_buff = hw->dpb.mDPB.size;

	canvas_addr = ANC0_CANVAS_ADDR;
	for (i = 0; i < num_buff; i++)
		WRITE_VREG((canvas_addr + i), i | (i << 8) | (i << 16));

	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, (0x1 << 1) | (0x1 << 2));
	size = hw->losless_comp_body_size + hw->losless_comp_header_size;
	for (i = 0; i < num_buff; i++) {
		if (decoder_bmmu_box_alloc_buf_phy(hw->bmmu_box,
				HEADER_BUFFER_IDX(i), size,
				DRIVER_HEADER_NAME, &maddr) < 0) {
			dpb_print(DECODE_ID(hw), 0,
				"%s malloc compress header failed %d\n",
				DRIVER_HEADER_NAME, i);
			/*hw->fatal_error |= DECODER_FATAL_ERROR_NO_MEM;*/
			return;
		}
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_DATA,  maddr >> 5);
		hw->buffer_spec[i].alloc_header_addr = maddr;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_MMU_DETAIL,
			"%s : canvas: %d  axiaddr:%x size 0x%x\n",
			__func__, i, (u32)maddr, size);
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
		WRITE_VREG(CURR_CANVAS_CTRL, ref->buf_spec_num<<24);
		ref_canv = READ_VREG(CURR_CANVAS_CTRL)&0xffffff;
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
					(ref->buf_spec_num & 0x3f) << 8);
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, ref_canv);
	}
	/*REFLIST[1]*/
	for (i = 0; i < (unsigned int)(pSlice->listXsize[1]); i++) {
		struct StorablePicture *ref = pSlice->listX[1][i];
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
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				((ref->buf_spec_num & 0x3f) << 8));
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		ref = pSlice->listX[1][0];
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
			((ref->buf_spec_num & 0x3f) << 8));
		rdata32_2 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32_2 = rdata32_2 & 0xffff;
		rdata32_2 = rdata32_2 | (rdata32_2 << 16);
		if (rdata32 == rdata32_2) {
			ref = pSlice->listX[1][1];
			WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				((ref->buf_spec_num & 0x3f) << 8));
			rdata32_2 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
			rdata32_2 = rdata32_2 & 0xffff;
			rdata32_2 = rdata32_2 | (rdata32_2 << 16);
		}
		WRITE_VREG(HEVCD_MCRCC_CTL3, rdata32_2);
	} else { /*P-PIC*/
		ref = pSlice->listX[0][0];
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR,
				((ref->buf_spec_num & 0x3f) << 8));
		rdata32 = READ_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR);
		rdata32 = rdata32 & 0xffff;
		rdata32 = rdata32 | (rdata32 << 16);
		WRITE_VREG(HEVCD_MCRCC_CTL2, rdata32);

		ref = pSlice->listX[0][1];
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

	lcu_x_num = (width + 15) >> 4;
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
	data32 |= (H265_MEM_MAP_MODE << 4);
	WRITE_VREG(HEVCD_IPP_AXIIF_CONFIG, data32);

	WRITE_VREG(HEVCD_MPP_DECOMP_CTL3,
			(0x80 << 20) | (0x80 << 10) | (0xff));

	WRITE_VREG(HEVCD_MPP_VDEC_MCR_CTL, 0x1 | (0x1 << 4));

	/*comfig vdec:h264:mdec to use hevc mcr/mcrcc/decomp*/
	WRITE_VREG(MDEC_PIC_DC_MUX_CTRL,
			READ_VREG(MDEC_PIC_DC_MUX_CTRL) | 0x1 << 31);
	/* ipp_enable*/
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0x1 << 1);

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
	data32 |= ((H265_MEM_MAP_MODE << 12) | 2);  /* bit1 : 1 .disable dw */
	WRITE_VREG(HEVC_SAO_CTRL1, data32);

#ifdef	H265_DW_NO_SCALE
	WRITE_VREG(HEVC_SAO_CTRL5, READ_VREG(HEVC_SAO_CTRL5) & ~(0xff << 16));
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
	long used_4k_num;
	u32 mc_y_adr;
	u32 mc_u_v_adr;
	/*u32 dw_y_adr;*/
	/*u32 dw_u_v_adr;*/
	u32 canvas_addr;
	int ret;
	if (hw->is_new_pic != 1)
		return;

	if (hw->is_idr_frame) {
		/* William TBD */
		memset(hw->frame_mmu_map_addr, 0, FRAME_MMU_MAP_SIZE);
	}
	if (hw->hevc_cur_buf_idx != 0xffff) {
		used_4k_num = (READ_VREG(HEVC_SAO_MMU_STATUS) >> 16);
		if (used_4k_num >= 0)
			dpb_print(DECODE_ID(hw),
			PRINT_FLAG_MMU_DETAIL,
			"release unused buf , used_4k_num %ld index %d\n",
			used_4k_num, hw->hevc_cur_buf_idx);
			decoder_mmu_box_free_idx_tail(
				hw->mmu_box,
				hw->hevc_cur_buf_idx,
				used_4k_num);
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

	/*dw_y_adr = H265_DOUBLE_WRITE_YSTART_TEMP;*/
	/*dw_u_v_adr =  H265_DOUBLE_WRITE_CSTART_TEMP;*/
#ifdef	H265_LOSLESS_COMPRESS_MODE
	/*WRITE_VREG(HEVC_SAO_Y_START_ADDR, dw_y_adr);*/
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
	/*WRITE_VREG(HEVC_SAO_C_START_ADDR, dw_u_v_adr);*/
#endif

#ifndef LOSLESS_COMPRESS_MODE
/*	WRITE_VREG(HEVC_SAO_Y_WPTR, mc_y_adr);*/
/*	WRITE_VREG(HEVC_SAO_C_WPTR, mc_u_v_adr); */
#else
	WRITE_VREG(HEVC_SAO_Y_WPTR, dw_y_adr);
	WRITE_VREG(HEVC_SAO_C_WPTR, dw_u_v_adr);
#endif

	ret = hevc_alloc_mmu(hw, pic->buf_spec_num,
			hw->frame_width, hw->frame_height, 0x0,
			hw->frame_mmu_map_addr);
	if (ret != 0) {
		dpb_print(DECODE_ID(hw),
		PRINT_FLAG_MMU_DETAIL, "can't alloc need mmu1,idx %d ret =%d\n",
		pic->buf_spec_num,
		ret);
		return;
	}

	/*Reset SAO + Enable SAO slice_start*/
	WRITE_VREG(HEVC_SAO_INT_STATUS,
			READ_VREG(HEVC_SAO_INT_STATUS) | 0x1 << 28);
	WRITE_VREG(HEVC_SAO_INT_STATUS,
			READ_VREG(HEVC_SAO_INT_STATUS) | 0x1 << 31);
	/*pr_info("hevc_sao_set_pic_buffer:mc_y_adr: %x\n", mc_y_adr);*/
	/*Send coommand to hevc-code to supply 4k buffers to sao*/
	WRITE_VREG(H265_SAO_4K_SET_BASE, (u32)hw->frame_mmu_map_phy_addr);
	WRITE_VREG(H265_SAO_4K_SET_COUNT, MAX_FRAME_4K_NUM);
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
	dpb_print(DECODE_ID(hw),
		PRINT_FLAG_MMU_DETAIL, "hevc_frame_done...set\n");
	while ((READ_VREG(HEVC_SAO_INT_STATUS) & 0x1) == 0) {
		dpb_print(DECODE_ID(hw),
		PRINT_FLAG_MMU_DETAIL, " %s...wait\n", __func__);
	}
	WRITE_VREG(HEVC_SAO_INT_STATUS, 0x1);
	hw->frame_done = 1;
	return;
}

static void  hevc_sao_wait_done(struct vdec_h264_hw_s *hw)
{
	dpb_print(DECODE_ID(hw),
		PRINT_FLAG_MMU_DETAIL, "hevc_sao_wait_done...start\n");
	while ((READ_VREG(HEVC_SAO_INT_STATUS) >> 31))
		dpb_print(DECODE_ID(hw),
		PRINT_FLAG_MMU_DETAIL, "hevc_sao_wait_done...wait\n");

	if ((hw->frame_busy == 1) && (hw->frame_done == 1)) {
		WRITE_VREG(SYS_COMMAND, H265_ABORT_SAO_4K_SET);
		while ((READ_VREG(SYS_COMMAND) & 0xff) !=
				H265_ABORT_SAO_4K_SET_DONE) {
			dpb_print(DECODE_ID(hw),
			PRINT_FLAG_MMU_DETAIL,
			"hevc_sao_wait_done...wait h265_abort_sao_4k_set_done\n");
		}
		hw->frame_busy = 0;
		hw->frame_done = 0;
	}
	return;
}
static void buf_spec_init(struct vdec_h264_hw_s *hw)
{
	int i;
	unsigned long flags;
	spin_lock_irqsave(&hw->bufspec_lock, flags);
	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		hw->buffer_spec[i].used = -1;
		hw->buffer_spec[i].canvas_pos = -1;
	}
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
	if (mmu_enable) {
		if (hw->buffer_spec[i].alloc_header_addr)
			return 0;
		else
			return -1;
	} else {

		int	buf_size = (hw->mb_total << 8) + (hw->mb_total << 7);
		int addr;
		if (hw->buffer_spec[i].cma_alloc_addr)
			return 0;

	if (decoder_bmmu_box_alloc_buf_phy(hw->bmmu_box, i,
		PAGE_ALIGN(buf_size), DRIVER_NAME,
		&hw->buffer_spec[i].cma_alloc_addr) < 0) {
		hw->buffer_spec[i].cma_alloc_addr = 0;
		dpb_print(DECODE_ID(hw), 0,
		"%s, fail to alloc buf for bufspec%d, try later\n",
				__func__, i
		);
		return -1;
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
			CANVAS_BLKMODE_32X32;

		hw->buffer_spec[i].canvas_config[1].phy_addr =
				hw->buffer_spec[i].u_addr;
		hw->buffer_spec[i].canvas_config[1].width =
				hw->mb_width << 4;
		hw->buffer_spec[i].canvas_config[1].height =
				hw->mb_height << 3;
		hw->buffer_spec[i].canvas_config[1].block_mode =
				CANVAS_BLKMODE_32X32;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
		"%s, alloc buf for bufspec%d\n",
				__func__, i
		);
	}
	return 0;
}

static void config_decode_canvas(struct vdec_h264_hw_s *hw, int i)
{
	canvas_config(hw->buffer_spec[i].
		y_canvas_index,
		hw->buffer_spec[i].y_addr,
		hw->mb_width << 4,
		hw->mb_height << 4,
		CANVAS_ADDR_NOWRAP,
		CANVAS_BLKMODE_32X32);

	canvas_config(hw->buffer_spec[i].
		u_canvas_index,
		hw->buffer_spec[i].u_addr,
		hw->mb_width << 4,
		hw->mb_height << 3,
		CANVAS_ADDR_NOWRAP,
		CANVAS_BLKMODE_32X32);
	WRITE_VREG(ANC0_CANVAS_ADDR + hw->buffer_spec[i].canvas_pos,
		spec2canvas(&hw->buffer_spec[i]));
}

int get_free_buf_idx(struct vdec_s *vdec)
{
	int i;
	unsigned long addr, flags;
	int index = -1;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	int buf_total = BUFSPEC_POOL_SIZE;
	spin_lock_irqsave(&hw->bufspec_lock, flags);
	for (i = hw->start_search_pos; i < buf_total; i++) {
		if (mmu_enable)
			addr = hw->buffer_spec[i].alloc_header_addr;
		else
			addr = hw->buffer_spec[i].cma_alloc_addr;
		if (hw->buffer_spec[i].used == 0	&& addr) {
			hw->buffer_spec[i].used = 1;
			hw->start_search_pos = i+1;
			index = i;
			break;
		}
	}
	if (index < 0) {
		for (i = 0; i < hw->start_search_pos; i++) {
			if (mmu_enable)
				addr = hw->buffer_spec[i].alloc_header_addr;
			else
				addr = hw->buffer_spec[i].cma_alloc_addr;
			if (hw->buffer_spec[i].used == 0 && addr) {
				hw->buffer_spec[i].used = 1;
				hw->start_search_pos = i+1;
				index = i;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&hw->bufspec_lock, flags);
	if (hw->start_search_pos >= buf_total)
		hw->start_search_pos = 0;
	dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
			"%s, buf_spec_num %d\n", __func__, index);

	return index;
}

int release_buf_spec_num(struct vdec_s *vdec, int buf_spec_num)
{
	u32 cur_buf_idx;
	unsigned long flags;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	dpb_print(DECODE_ID(hw), PRINT_FLAG_MMU_DETAIL,
		"%s buf_spec_num %d used %d\n",
		__func__, buf_spec_num,
		hw->buffer_spec[buf_spec_num].used);
	if (buf_spec_num >= 0 &&
		buf_spec_num < BUFSPEC_POOL_SIZE
		) {
		spin_lock_irqsave(&hw->bufspec_lock, flags);
		hw->buffer_spec[buf_spec_num].used = 0;
		spin_unlock_irqrestore(&hw->bufspec_lock, flags);
		if (mmu_enable) {
			WRITE_VREG(CURR_CANVAS_CTRL, buf_spec_num<<24);
			cur_buf_idx = READ_VREG(CURR_CANVAS_CTRL);
			cur_buf_idx = cur_buf_idx&0xff;
			decoder_mmu_box_free_idx(hw->mmu_box, cur_buf_idx);
		}
		release_aux_data(hw, buf_spec_num);
	}
	return 0;
}

static void config_buf_specs(struct vdec_s *vdec)
{
	int i, j;
	unsigned long flags;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	spin_lock_irqsave(&hw->bufspec_lock, flags);
	for (i = 0, j = 0;
		j < hw->dpb.mDPB.size
		&& i < BUFSPEC_POOL_SIZE;
		i++) {
		int canvas;
		if (hw->buffer_spec[i].used != -1)
			continue;
		canvas = vdec->get_canvas(j, 2);
		hw->buffer_spec[i].y_canvas_index = canvas_y(canvas);
		hw->buffer_spec[i].u_canvas_index = canvas_u(canvas);
		hw->buffer_spec[i].v_canvas_index = canvas_v(canvas);
		hw->buffer_spec[i].used = 0;

		hw->buffer_spec[i].canvas_pos = j;

		/*pr_info("config canvas (%d) %x for bufspec %d\r\n",
			j, canvas, i);*/
		j++;
	}
	spin_unlock_irqrestore(&hw->bufspec_lock, flags);
}

void dealloc_buf_specs(struct vdec_h264_hw_s *hw)
{
	int i;
	unsigned long flags;
	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		if (hw->buffer_spec[i].used == 4) {
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

			if (!mmu_enable) {
				if (hw->buffer_spec[i].cma_alloc_addr) {
					decoder_bmmu_box_free_idx(
						hw->bmmu_box,
						i);
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
	return;
}

unsigned char have_free_buf_spec(struct vdec_s *vdec)
{
	int i;
	unsigned long addr;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	int canvas_pos_min = BUFSPEC_POOL_SIZE;
	int index = -1;
	int ret = 0;
	int allocated_count = 0;
	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		if (mmu_enable)
			addr = hw->buffer_spec[i].alloc_header_addr;
		else
			addr = hw->buffer_spec[i].cma_alloc_addr;
		if (hw->buffer_spec[i].used == 0) {

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
		dealloc_buf_specs(hw);
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
int prepare_display_buf(struct vdec_s *vdec, struct FrameStore *frame)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	struct vframe_s *vf = NULL;
	int buffer_index = frame->buf_spec_num;
	int vf_count = 1;
	int i;
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
	if (error_proc_policy & 0x1000) {
		int error_skip_i_count = (error_skip_count >> 12) & 0xf;
		int error_skip_frame_count = error_skip_count & 0xfff;
		if (((hw->no_error_count < error_skip_frame_count)
			&& (error_skip_i_count == 0 ||
			hw->no_error_i_count < error_skip_i_count))
			&& (!(frame->data_flag & I_FLAG)))
			frame->data_flag |= ERROR_FLAG;
	}
	if ((frame->data_flag & NODISP_FLAG) ||
		(frame->data_flag & NULL_FLAG) ||
		((!hw->send_error_frame_flag) &&
			(frame->data_flag & ERROR_FLAG)) ||
		((hw->i_only & 0x1) &&
		(!(frame->data_flag & I_FLAG)))
			) {
		set_frame_output_flag(&hw->dpb, frame->index);
		return -1;
	}
	display_frame_count[DECODE_ID(hw)]++;

	if (dpb_is_debug(DECODE_ID(hw),
	 PRINT_FLAG_DPB_DETAIL)) {
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

	if (frame->frame == NULL ||
		frame->top_field == NULL ||
		frame->bottom_field == NULL ||
		frame->frame->coded_frame)
		vf_count = 1;
	else
		vf_count = 2;
	hw->buffer_spec[buffer_index].vf_ref = 0;
	for (i = 0; i < vf_count; i++) {
		if (kfifo_get(&hw->newframe_q, &vf) == 0 ||
			vf == NULL) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
				"%s fatal error, no available buffer slot.\n",
				__func__);
			return -1;
		}
		vf->duration_pulldown = 0;
		vf->pts = frame->pts;
		vf->pts_us64 = frame->pts64;
		vf->index = VF_INDEX(frame->index, buffer_index);
		if (mmu_enable) {
			vf->type = VIDTYPE_COMPRESS | VIDTYPE_VIU_FIELD;
			vf->type |= VIDTYPE_SCATTER;
			vf->bitdepth =
				BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
			vf->bitdepth |= BITDEPTH_SAVING_MODE;
			vf->compWidth = hw->frame_width;
			vf->compHeight = hw->frame_height;
			vf->compHeadAddr =
				hw->buffer_spec[buffer_index].alloc_header_addr;
			vf->compBodyAddr = 0;
			vf->canvas0Addr = vf->canvas1Addr = 0;
		} else {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
			vf->canvas0Addr = vf->canvas1Addr =
			spec2canvas(&hw->buffer_spec[buffer_index]);
		}
		set_frame_info(hw, vf, buffer_index);
		vf->flag = 0;
		if (frame->data_flag & I_FLAG)
			vf->flag |= VFRAME_FLAG_SYNCFRAME;
		if (frame->data_flag & ERROR_FLAG)
			vf->flag |= VFRAME_FLAG_ERROR_RECOVERY;
		update_vf_memhandle(hw, vf, buffer_index);
		hw->buffer_spec[buffer_index].used = 2;
		hw->buffer_spec[buffer_index].vf_ref++;

		if (frame->frame &&
			frame->top_field &&
			frame->bottom_field &&
			(!frame->frame->coded_frame)) {
			vf->type =
				VIDTYPE_INTERLACE_FIRST |
				VIDTYPE_VIU_NV21;
			if (frame->top_field->poc <=
				frame->bottom_field->poc) /*top first*/
				vf->type |= (i == 0 ?
					VIDTYPE_INTERLACE_TOP :
					VIDTYPE_INTERLACE_BOTTOM);
			else
				vf->type |= (i == 0 ?
					VIDTYPE_INTERLACE_BOTTOM :
					VIDTYPE_INTERLACE_TOP);
			vf->duration = vf->duration/2;
		}
		kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
		hw->vf_pre_count++;
		vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
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
		(slice_type < 0) ? "" : slice_type_name[slice_type],
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
	hw->decode_timeout_count = 2;
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
	if (pic->buf_spec_num < 0 || pic->buf_spec_num >= BUFSPEC_POOL_SIZE
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
		 PRINT_FLAG_DPB_DETAIL)) {
		dpb_print(DECODE_ID(hw), 0,
			"%s:old size %d count %d,suf %d dv_flag %d\r\n",
			__func__, AUX_DATA_SIZE(pic),
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
				PRINT_FLAG_DPB_DETAIL)) {
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

static void config_decode_mode(struct vdec_h264_hw_s *hw)
{
#ifdef CONFIG_AM_VDEC_DV
	struct vdec_s *vdec = hw_to_vdec(hw);
#endif
	if (input_frame_based(hw_to_vdec(hw)))
		WRITE_VREG(H264_DECODE_MODE,
			DECODE_MODE_MULTI_FRAMEBASE);
#ifdef CONFIG_AM_VDEC_DV
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
#ifdef ONE_COLOCATE_BUF_PER_DECODE_BUF
	int colocate_buf_index;
#endif
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

	WRITE_VREG(CURR_CANVAS_CTRL, canvas_pos << 24);
	if (!mmu_enable) {
		canvas_adr = READ_VREG(CURR_CANVAS_CTRL) & 0xffffff;
		WRITE_VREG(REC_CANVAS_ADDR, canvas_adr);
		WRITE_VREG(DBKR_CANVAS_ADDR, canvas_adr);
		WRITE_VREG(DBKW_CANVAS_ADDR, canvas_adr);
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
	if (mmu_enable) {
		hevc_mcr_config_mc_ref(hw);
		hevc_mcr_config_mcrcc(hw);
	}

	dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
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
		if (ref == NULL) {
			hw->data_flag |= ERROR_FLAG;
			return -1;
		}
		if (ref->data_flag & ERROR_FLAG)
			hw->data_flag |= ERROR_FLAG;
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
		unsigned cfg;
		/* bit[6:5] - frame/field info,
		 * 01 - top, 10 - bottom, 11 - frame
		 */
#ifdef ERROR_CHECK
		if (ref == NULL) {
			hw->data_flag |= ERROR_FLAG;
			return -2;
		}
		if (ref->data_flag & ERROR_FLAG)
			hw->data_flag |= ERROR_FLAG;
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
		int l10_structure;
		int cur_colocate_ref_type;
		/* H264_CO_MB_RD_ADDR[bit 29], top/bot for B field pciture,
		 * 0 - top, 1 - bot
		 */
		unsigned int val;
#ifdef ERROR_CHECK
		if (colocate_pic == NULL) {
			hw->data_flag |= ERROR_FLAG;
			return -5;
		}
		if (colocate_pic->data_flag & ERROR_FLAG)
			hw->data_flag |= ERROR_FLAG;
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
			val = ((colocate_rd_adr+colocate_adr_offset) >> 3) |
				(l10_structure << 30) |
				(cur_colocate_ref_type << 29);
			WRITE_VREG(H264_CO_MB_RD_ADDR, val);
			dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
				"co idx %d, WRITE_VREG(H264_CO_MB_RD_ADDR) = %x, addr %x L1(0) pic_structure %d mbaff %d\n",
				colocate_pic->colocated_buf_index,
				val, colocate_rd_adr + colocate_adr_offset,
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
		val = ((colocate_rd_adr+colocate_adr_offset)>>3) |
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
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;

	if (!hw)
		return NULL;

	if (force_disp_bufspec_num & 0x100) {
		if (force_disp_bufspec_num & 0x200)
			return NULL;
		return &hw->vframe_dummy;
	}

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
		if (mmu_enable) {
			vf->type = VIDTYPE_COMPRESS | VIDTYPE_VIU_FIELD;
			vf->type |= VIDTYPE_SCATTER;
			vf->bitdepth =
				BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
			vf->compWidth = hw->frame_width;
			vf->compHeight = hw->frame_height;
			vf->compHeadAddr =
				hw->buffer_spec[buffer_index].alloc_header_addr;
			vf->compBodyAddr = 0;
			vf->canvas0Addr = vf->canvas1Addr = 0;
		} else {
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
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
		if (dpb_is_debug(DECODE_ID(hw),
			PRINT_FLAG_VDEC_STATUS)) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s buf_spec_num %d vf %p dur %d pts %d interval %dms\n",
			__func__, BUFSPEC_INDEX(vf->index), vf,
			vf->duration, vf->pts, frame_interval);
		}
		if (hw->last_frame_time > 0) {
			if (frame_interval >
				max_get_frame_interval[DECODE_ID(hw)])
				max_get_frame_interval[DECODE_ID(hw)]
				= frame_interval;
		}
		hw->last_frame_time = time;
		hw->vf_get_count++;
		return vf;
	}

	return NULL;
}

static void vh264_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	int buf_spec_num;
	int frame_index;
	if (vf == (&hw->vframe_dummy))
		return;

	frame_index = FRAME_INDEX(vf->index);
	buf_spec_num = BUFSPEC_INDEX(vf->index);
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
	if (hw->buffer_spec[buf_spec_num].used == 2) {
		struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
		"%s to fs[%d], poc %d buf_spec_num %d used %d vf_ref %d\n",
		__func__, frame_index,
		p_H264_Dpb->mFrameStore[frame_index].poc,
		buf_spec_num,
		hw->buffer_spec[buf_spec_num].used,
		hw->buffer_spec[buf_spec_num].vf_ref);
		hw->buffer_spec[buf_spec_num].vf_ref--;
		if (hw->buffer_spec[buf_spec_num].vf_ref <= 0)
			set_frame_output_flag(&hw->dpb, frame_index);
	} else {
		unsigned long flags;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
		"%s isolated vf, buf_spec_num %d used %d vf_ref %d\n",
		__func__, buf_spec_num,
		hw->buffer_spec[buf_spec_num].used,
		hw->buffer_spec[buf_spec_num].vf_ref);
		spin_lock_irqsave(&hw->bufspec_lock, flags);
		hw->buffer_spec[buf_spec_num].vf_ref--;
		if (hw->buffer_spec[buf_spec_num].vf_ref <= 0) {
			if (hw->buffer_spec[buf_spec_num].used == 3)
				hw->buffer_spec[buf_spec_num].used = 4;
			else if (hw->buffer_spec[buf_spec_num].used == 5)
				hw->buffer_spec[buf_spec_num].used = 0;
		}
		spin_unlock_irqrestore(&hw->bufspec_lock, flags);
	}

	hw->vf_put_count++;
	kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);

#define ASSIST_MBOX1_IRQ_REG    VDEC_ASSIST_MBOX1_IRQ_REG
	if (hw->buffer_empty_flag)
		WRITE_VREG(ASSIST_MBOX1_IRQ_REG, 0x1);
}

static int vh264_event_cb(int type, void *data, void *op_arg)
{
	unsigned long flags;
	struct vdec_s *vdec = op_arg;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;

	if (type & VFRAME_EVENT_RECEIVER_GET_AUX_DATA) {
		struct provider_aux_req_s *req =
			(struct provider_aux_req_s *)data;
		int buf_spec_num = BUFSPEC_INDEX(req->vf->index);
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
#ifdef CONFIG_AM_VDEC_DV
			req->dv_enhance_exist =
				hw->buffer_spec[buf_spec_num].dv_enhance_exist;
#else
			req->dv_enhance_exist = 0;
#endif
		}
		spin_unlock_irqrestore(&hw->lock, flags);

		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
		"%s(type 0x%x vf buf_spec_num 0x%x)=>size 0x%x\n",
		__func__, type, buf_spec_num, req->aux_size);
	}

	return 0;
}

static void set_frame_info(struct vdec_h264_hw_s *hw, struct vframe_s *vf,
				u32 index)
{
	int force_rate = input_frame_based(hw_to_vdec(hw)) ?
		force_rate_framebase : force_rate_streambase;
	dpb_print(DECODE_ID(hw), PRINT_FLAG_DPB_DETAIL,
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
	if (mmu_enable)
		return;
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
	int i, j;
	int mb_width, mb_total;
	int max_reference_size, level_idc;
	int mb_height;
	unsigned long flags;
	/*int mb_mv_byte;*/
	struct vdec_s *vdec = hw_to_vdec(hw);
	u32 seq_info2;
	int ret = 0;
	int active_buffer_spec_num;
	unsigned int buf_size;
	unsigned int frame_mbs_only_flag;
	unsigned int chroma_format_idc, chroma444;
	unsigned int crop_infor, crop_bottom, crop_right;
	unsigned int used_reorder_dpb_size_margin
		= reorder_dpb_size_margin;
#ifdef CONFIG_AM_VDEC_DV
	if (vdec->master || vdec->slave)
		used_reorder_dpb_size_margin =
			reorder_dpb_size_margin_dv;
#endif
	seq_info2 = READ_VREG(AV_SCRATCH_1);
	hw->seq_info = READ_VREG(AV_SCRATCH_2);

	mb_width = seq_info2 & 0xff;
	mb_total = (seq_info2 >> 8) & 0xffff;
	if (!mb_width && mb_total) /*for 4k2k*/
		mb_width = 256;
	mb_height = mb_total/mb_width;
	if (mb_width > 0x110 ||
		mb_height > 0xa0) {
		dpb_print(DECODE_ID(hw), 0,
			"!!!wrong seq_info2 0x%x mb_width/mb_height (0x%x/0x%x) %x\r\n",
			seq_info2,
			mb_width,
			mb_height);
		WRITE_VREG(AV_SCRATCH_0, (hw->max_reference_size<<24) |
			(hw->dpb.mDPB.size<<16) |
			(hw->dpb.mDPB.size<<8));
		return 0;
	}

	if (seq_info2 != 0 &&
		hw->seq_info2 != (seq_info2 & (~0x80000000)) &&
		hw->seq_info2 != 0
		) /*picture size changed*/
		h264_reconfig(hw);

	if (hw->config_bufmgr_done == 0) {
		struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
		u32 reg_val;
		hw->seq_info2 = seq_info2 & (~0x80000000);
		dpb_print(DECODE_ID(hw), 0,
			"AV_SCRATCH_1 = %x, AV_SCRATCH_2 %x\r\n",
			seq_info2, hw->seq_info);

		dpb_init_global(&hw->dpb,
			DECODE_ID(hw), 0, 0);

		p_H264_Dpb->fast_output_enable = fast_output_enable;
		/*mb_mv_byte = (seq_info2 & 0x80000000) ? 24 : 96;*/

#if 1
		/*crop*/
		/* AV_SCRATCH_2
		   bit 15: frame_mbs_only_flag
		   bit 13-14: chroma_format_idc */
		frame_mbs_only_flag = (hw->seq_info >> 15) & 0x01;
		chroma_format_idc = (hw->seq_info >> 13) & 0x03;
		chroma444 = (chroma_format_idc == 3) ? 1 : 0;

		/* @AV_SCRATCH_6.31-16 =  (left  << 8 | right ) << 1
		   @AV_SCRATCH_6.15-0   =  (top << 8  | bottom ) <<
		   (2 - frame_mbs_only_flag) */
		crop_infor = READ_VREG(AV_SCRATCH_6);
		crop_bottom = (crop_infor & 0xff) >> (2 - frame_mbs_only_flag);
		crop_right = ((crop_infor >> 16) & 0xff)
			>> (2 - frame_mbs_only_flag);

		hw->frame_width = mb_width << 4;
		hw->frame_height = mb_height << 4;
		if (frame_mbs_only_flag) {
			hw->frame_height =
				hw->frame_height - (2 >> chroma444) *
				min(crop_bottom,
					(unsigned int)((8 << chroma444) - 1));
			hw->frame_width =
				hw->frame_width -
					(2 >> chroma444) * min(crop_right,
						(unsigned
						 int)((8 << chroma444) - 1));
		} else {
			hw->frame_height =
				hw->frame_height - (4 >> chroma444) *
				min(crop_bottom,
					(unsigned int)((8 << chroma444)
							  - 1));
			hw->frame_width =
				hw->frame_width -
				(4 >> chroma444) * min(crop_right,
				(unsigned int)((8 << chroma444) - 1));
		}
		dpb_print(DECODE_ID(hw), 0,
			"frame_mbs_only_flag %d, crop_bottom %d,  frame_height %d, ",
			frame_mbs_only_flag, crop_bottom, hw->frame_height);
		dpb_print(DECODE_ID(hw), 0,
			"mb_height %d,crop_right %d, frame_width %d, mb_width %d\n",
			mb_height, crop_right,
			hw->frame_width, mb_width);

		if (hw->frame_height == 1088)
			hw->frame_height = 1080;
#endif

		mb_width = (mb_width+3) & 0xfffffffc;
		mb_height = (mb_height+3) & 0xfffffffc;
		mb_total = mb_width * mb_height;
		if (mmu_enable)
			hevc_mcr_sao_global_hw_init(hw,
				hw->frame_width, hw->frame_height);

		reg_val = READ_VREG(AV_SCRATCH_B);
		level_idc = reg_val & 0xff;
		max_reference_size = (reg_val >> 8) & 0xff;
		dpb_print(DECODE_ID(hw), 0,
			"mb height/widht/total: %x/%x/%x level_idc %x max_ref_num %x\n",
			mb_height, mb_width, mb_total,
			level_idc, max_reference_size);

		p_H264_Dpb->colocated_buf_size = mb_total * 96;
		hw->mb_total = mb_total;
		hw->mb_width = mb_width;
		hw->mb_height = mb_height;

		hw->dpb.reorder_pic_num =
			get_max_dec_frame_buf_size(level_idc,
			max_reference_size, mb_width, mb_height);
		active_buffer_spec_num =
			hw->dpb.reorder_pic_num
			+ used_reorder_dpb_size_margin;
		hw->max_reference_size =
			max_reference_size + reference_buf_margin;

		if (active_buffer_spec_num > MAX_VF_BUF_NUM) {
			active_buffer_spec_num = MAX_VF_BUF_NUM;
			hw->dpb.reorder_pic_num = active_buffer_spec_num
				- used_reorder_dpb_size_margin;
		}
		hw->dpb.mDPB.size = active_buffer_spec_num;
		if (hw->max_reference_size > MAX_VF_BUF_NUM)
			hw->max_reference_size = MAX_VF_BUF_NUM;
		hw->dpb.max_reference_size = hw->max_reference_size;

		if (hw->no_poc_reorder_flag)
			hw->dpb.reorder_pic_num = 1;
		dpb_print(DECODE_ID(hw), 0,
			"%s active_buf_spec_num %d reorder_pic_num %d collocate_buf_num %d\r\n",
			__func__, active_buffer_spec_num,
			hw->dpb.reorder_pic_num,
			hw->max_reference_size);

		buf_size = (hw->mb_total << 8) + (hw->mb_total << 7);

		mutex_lock(&vmh264_mutex);
		if (!mmu_enable) {
			config_buf_specs(vdec);
			i = get_buf_spec_by_canvas_pos(hw, 0);
			if (alloc_one_buf_spec(hw, i) >= 0)
				config_decode_canvas(hw, i);
			else
				ret = -1;
		} else {
			spin_lock_irqsave(&hw->bufspec_lock, flags);
			for (i = 0, j = 0;
				j < active_buffer_spec_num
				&& i < BUFSPEC_POOL_SIZE;
				i++) {
				if (hw->buffer_spec[i].used != -1)
					continue;
				hw->buffer_spec[i].used = 0;
				hw->buffer_spec[i].alloc_header_addr = 0;
				hw->buffer_spec[i].canvas_pos = j;
				j++;
			}
			spin_unlock_irqrestore(&hw->bufspec_lock, flags);
			hevc_mcr_config_canv2axitbl(hw);
		}
		mutex_unlock(&vmh264_mutex);

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
		if (!mmu_enable) {
			mutex_lock(&vmh264_mutex);
			if (ret >= 0 && hw->decode_pic_count == 0) {
				/* h264_reconfig: alloc later*/
				for (j = 1; j < hw->dpb.mDPB.size; j++) {
					i = get_buf_spec_by_canvas_pos(hw, j);
					if (alloc_one_buf_spec(hw, i) < 0)
						break;
					config_decode_canvas(hw, i);
				}
			}
			mutex_unlock(&vmh264_mutex);
		}

		hw->config_bufmgr_done = 1;

	/*end of  config_bufmgr_done */
	}


	WRITE_VREG(AV_SCRATCH_0, (hw->max_reference_size<<24) |
		(hw->dpb.mDPB.size<<16) |
		(hw->dpb.mDPB.size<<8));

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
					schedule_work(&hw->notify_work);
					dpb_print(DECODE_ID(hw),
						PRINT_FLAG_DEC_DETAIL,
						"frame_dur %d from timing_info\n",
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
		dpb_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"H.264: timing_info not present\n");
	}

	/*aspect ratio*/
	aspect_ratio_info_present_flag =
		p_H264_Dpb->vui_status & 0x1;
	aspect_ratio_idc = p_H264_Dpb->aspect_ratio_idc;

	if (aspect_ratio_info_present_flag) {
		if (aspect_ratio_idc == EXTEND_SAR) {
			hw->h264_ar =
				div_u64(256ULL *
					p_H264_Dpb->aspect_ratio_sar_height *
					hw->frame_height,
					p_H264_Dpb->aspect_ratio_sar_width *
					hw->frame_width);
		} else {
			/* pr_info("v264dec: aspect_ratio_idc = %d\n",
			   aspect_ratio_idc); */

			switch (aspect_ratio_idc) {
			case 1:
				hw->h264_ar = 0x100 * hw->frame_height /
					hw->frame_width;
				break;
			case 2:
				hw->h264_ar = 0x100 * hw->frame_height * 11 /
					(hw->frame_width * 12);
				break;
			case 3:
				hw->h264_ar = 0x100 * hw->frame_height * 11 /
					(hw->frame_width * 10);
				break;
			case 4:
				hw->h264_ar = 0x100 * hw->frame_height * 11 /
					(hw->frame_width * 16);
				break;
			case 5:
				hw->h264_ar = 0x100 * hw->frame_height * 33 /
					(hw->frame_width * 40);
				break;
			case 6:
				hw->h264_ar = 0x100 * hw->frame_height * 11 /
					(hw->frame_width * 24);
				break;
			case 7:
				hw->h264_ar = 0x100 * hw->frame_height * 11 /
					(hw->frame_width * 20);
				break;
			case 8:
				hw->h264_ar = 0x100 * hw->frame_height * 11 /
					(hw->frame_width * 32);
				break;
			case 9:
				hw->h264_ar = 0x100 * hw->frame_height * 33 /
					(hw->frame_width * 80);
				break;
			case 10:
				hw->h264_ar = 0x100 * hw->frame_height * 11 /
					(hw->frame_width * 18);
				break;
			case 11:
				hw->h264_ar = 0x100 * hw->frame_height * 11 /
					(hw->frame_width * 15);
				break;
			case 12:
				hw->h264_ar = 0x100 * hw->frame_height * 33 /
					(hw->frame_width * 64);
				break;
			case 13:
				hw->h264_ar = 0x100 * hw->frame_height * 99 /
					(hw->frame_width * 160);
				break;
			case 14:
				hw->h264_ar = 0x100 * hw->frame_height * 3 /
					(hw->frame_width * 4);
				break;
			case 15:
				hw->h264_ar = 0x100 * hw->frame_height * 2 /
					(hw->frame_width * 3);
				break;
			case 16:
				hw->h264_ar = 0x100 * hw->frame_height * 1 /
					(hw->frame_width * 2);
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
				} else {
					hw->h264_ar = hw->frame_height * 0x100 /
						hw->frame_width;
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
		} else
			hw->h264_ar = hw->frame_height * 0x100 /
				hw->frame_width;
	}

	if (hw->pts_unstable && (hw->fixed_frame_rate_flag == 0)) {
		if (((RATE_2397_FPS == hw->frame_dur)
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
	if (error_proc_policy & 0x20)
		hw->reset_bufmgr_flag = 1;
}

static bool is_buffer_available(struct vdec_s *vdec)
{
	bool buffer_available = 1;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	struct DecodedPictureBuffer *p_Dpb = &p_H264_Dpb->mDPB;
	if ((kfifo_len(&hw->newframe_q) <= 0) ||
	    ((hw->config_bufmgr_done) && (!have_free_buf_spec(vdec))) ||
	    ((p_H264_Dpb->mDPB.init_done) &&
	     (p_H264_Dpb->mDPB.used_size == p_H264_Dpb->mDPB.size) &&
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
		if ((error_proc_policy & 0x4) &&
			(error_proc_policy & 0x8)) {
			if ((kfifo_len(&hw->display_q) <= 0) &&
			(p_H264_Dpb->mDPB.used_size ==
				p_H264_Dpb->mDPB.size) &&
				(p_Dpb->ref_frames_in_buffer >
				(imax(
				1, p_Dpb->num_ref_frames)
				- p_Dpb->ltref_frames_in_buffer +
				force_sliding_margin)))
				bufmgr_recover(hw);
			else
				bufmgr_h264_remove_unused_frame(p_H264_Dpb, 1);
		} else if ((error_proc_policy & 0x4) &&
			(kfifo_len(&hw->display_q) <= 0) &&
			((p_H264_Dpb->mDPB.used_size ==
				p_H264_Dpb->mDPB.size) ||
			(!have_free_buf_spec(vdec)))) {
			enum receviver_start_e state = RECEIVER_INACTIVE;
			if ((error_proc_policy & 0x10) &&
				vf_get_receiver(vdec->vf_provider_name)) {
				state =
				vf_notify_receiver(vdec->vf_provider_name,
					VFRAME_EVENT_PROVIDER_QUREY_STATE,
					NULL);
				if ((state == RECEIVER_STATE_NULL)
					|| (state == RECEIVER_STATE_NONE))
					state = RECEIVER_INACTIVE;
			}
			if (state == RECEIVER_INACTIVE)
				bufmgr_recover(hw);
			else
				bufmgr_h264_remove_unused_frame(p_H264_Dpb, 1);
		} else if ((error_proc_policy & 0x8) &&
			(p_Dpb->ref_frames_in_buffer >
			(imax(
			1, p_Dpb->num_ref_frames)
			- p_Dpb->ltref_frames_in_buffer +
			force_sliding_margin)))
			bufmgr_recover(hw);
		else
			bufmgr_h264_remove_unused_frame(p_H264_Dpb, 1);
	}

	return buffer_available;
}

static void check_decoded_pic_error(struct vdec_h264_hw_s *hw)
{
	unsigned mby_mbx = READ_VREG(MBY_MBX);
	unsigned mb_total = (hw->seq_info2 >> 8) & 0xffff;
	unsigned decode_mb_count =
		(((mby_mbx & 0xff) + 1) *
		(((mby_mbx >> 8) & 0xff) + 1));
	if ((error_proc_policy & 0x100) &&
		decode_mb_count != mb_total)
		hw->data_flag |= ERROR_FLAG;

	if ((error_proc_policy & 0x200) &&
		READ_VREG(ERROR_STATUS_REG) != 0)
		hw->data_flag |= ERROR_FLAG;

	if (hw->data_flag & ERROR_FLAG) {
		dpb_print(DECODE_ID(hw), 0,
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

static irqreturn_t vh264_isr_thread_fn(struct vdec_s *vdec)
{
	int i;
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	unsigned int dec_dpb_status = p_H264_Dpb->dec_dpb_status;
	u32 debug_tag;

	if (dec_dpb_status == H264_CONFIG_REQUEST) {
		WRITE_VREG(DPB_STATUS_REG, H264_ACTION_CONFIG_DONE);
		reset_process_time(hw);
		hw->dec_result = DEC_RESULT_CONFIG_PARAM;
		vdec_schedule_work(&hw->work);
	} else if (dec_dpb_status == H264_SLICE_HEAD_DONE) {
		int slice_header_process_status = 0;
		/*unsigned char is_idr;*/
		unsigned short *p = (unsigned short *)hw->lmem_addr;
		reset_process_time(hw);
		dma_sync_single_for_cpu(
			amports_get_dma_device(),
			hw->lmem_addr_remap,
			PAGE_SIZE,
			DMA_FROM_DEVICE);
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
		for (i = 0; i < (RPM_END-RPM_BEGIN); i += 4) {
			int ii;
			for (ii = 0; ii < 4; ii++) {
				p_H264_Dpb->dpb_param.l.data[i+ii] =
					p[i+3-ii];
				if (dpb_is_debug(DECODE_ID(hw),
					RRINT_FLAG_RPM)) {
					if (((i + ii) & 0xf) == 0)
						dpb_print(DECODE_ID(hw),
							0, "%04x:",
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
#endif
		if ((first_i_policy & 0x3) != 0) {
			unsigned char is_i_slice =
				(p_H264_Dpb->dpb_param.l.data[SLICE_TYPE]
					== I_Slice)
				? 1 : 0;

			if ((first_i_policy & 0x3) == 0x3)
				is_i_slice =
				(p_H264_Dpb->dpb_param.dpb.NAL_info_mmco & 0x1f)
				== 5 ? 1 : 0;
			if (!is_i_slice) {
				if (hw->has_i_frame == 0) {
					hw->dec_result = DEC_RESULT_DONE;
					vdec_schedule_work(&hw->work);
					dpb_print(DECODE_ID(hw),
						PRINT_FLAG_UCODE_EVT,
						"has_i_frame is 0, discard none I(DR) frame\n");
					return IRQ_HANDLED;
				}
			} else {
				if (hw->skip_frame_count < 0) {
					/* second I */
					hw->dec_flag &= (~NODISP_FLAG);
					hw->skip_frame_count = 0;
				}
				if (hw->has_i_frame == 0 &&
					(p_H264_Dpb->
					dpb_param.dpb.NAL_info_mmco & 0x1f)
					!= 5) {
					int skip_count =
						(first_i_policy >> 8) & 0xff;
					/* first I (not IDR) */
					if ((first_i_policy & 0x3) == 2)
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

		if (hw->reset_bufmgr_flag) {
			h264_reset_bufmgr(hw);
			hw->reset_bufmgr_flag = 0;
		}

		slice_header_process_status =
			h264_slice_header_process(p_H264_Dpb);
		if (mmu_enable)
			hevc_sao_set_slice_type(hw,
				slice_header_process_status,
					hw->dpb.mSlice.idr_flag);
		vui_config(hw);

		if (p_H264_Dpb->mVideo.dec_picture) {
			int cfg_ret = 0;
			if (hw->sei_itu_data_len) {
				hw->sei_poc =
					p_H264_Dpb->mVideo.dec_picture->poc;
				schedule_work(&hw->user_data_work);
			}
			if (slice_header_process_status == 1) {
				/* for baseline , set fast_output mode */
				if (p_H264_Dpb->mSPS.profile_idc == BASELINE)
					p_H264_Dpb->fast_output_enable = 4;
				else
					p_H264_Dpb->fast_output_enable
							= fast_output_enable;
				hw->data_flag =
					(p_H264_Dpb->
						dpb_param.l.data[SLICE_TYPE]
						== I_Slice)
					? I_FLAG : 0;
				if ((hw->i_only & 0x2) &&
					(!(hw->data_flag & I_FLAG))) {
					hw->data_flag =	NULL_FLAG;
					goto pic_done_proc;
				}
				if ((p_H264_Dpb->
					dpb_param.dpb.NAL_info_mmco & 0x1f)
					== 5)
					hw->data_flag |= IDR_FLAG;
				dpb_print(DECODE_ID(hw),
				PRINT_FLAG_VDEC_STATUS,
				"==================> frame count %d to skip %d\n",
				hw->decode_pic_count+1,
				hw->skip_frame_count);
			}
			cfg_ret = config_decode_buf(hw,
				p_H264_Dpb->mVideo.dec_picture);
			if (cfg_ret < 0) {
				dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
					"config_decode_buf fail (%d)\n",
					cfg_ret);
				if (error_proc_policy & 0x2) {
					remove_picture(p_H264_Dpb,
						p_H264_Dpb->mVideo.dec_picture);
					p_H264_Dpb->mVideo.dec_picture = NULL;
					/*hw->data_flag |= ERROR_FLAG;*/
					hw->dec_result = DEC_RESULT_DONE;
					vdec_schedule_work(&hw->work);
					return IRQ_HANDLED;
				} else
					hw->data_flag |= ERROR_FLAG;
			}
		}

		if (slice_header_process_status == 1)
			WRITE_VREG(DPB_STATUS_REG, H264_ACTION_DECODE_NEWPIC);
		else
			WRITE_VREG(DPB_STATUS_REG, H264_ACTION_DECODE_SLICE);
		hw->last_mby_mbx = 0;
		hw->last_ucode_watchdog_reg_val = 0;
		start_process_time(hw);
	} else if (dec_dpb_status == H264_PIC_DATA_DONE) {
pic_done_proc:
		reset_process_time(hw);
		if (p_H264_Dpb->mVideo.dec_picture) {
#ifdef CONFIG_AM_VDEC_DV
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
			if (hw->chunk) {
				p_H264_Dpb->mVideo.dec_picture->pts =
					hw->chunk->pts;
				p_H264_Dpb->mVideo.dec_picture->pts64 =
					hw->chunk->pts64;
#ifdef CONFIG_AM_VDEC_DV
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
				u32 offset = pic->offset_delimiter_lo |
					(pic->offset_delimiter_hi << 16);
				if (pts_lookup_offset_us64(PTS_TYPE_VIDEO,
					offset, &pic->pts, 0, &pic->pts64)) {
					pic->pts = 0;
					pic->pts64 = 0;
				}
			}
			check_decoded_pic_error(hw);

			store_picture_in_dpb(p_H264_Dpb,
				p_H264_Dpb->mVideo.dec_picture,
				hw->data_flag | hw->dec_flag);
			if (hw->data_flag & ERROR_FLAG) {
				hw->no_error_count = 0;
				hw->no_error_i_count = 0;
			} else {
				hw->no_error_count++;
				if (hw->data_flag & I_FLAG)
					hw->no_error_i_count++;
			}
			if (mmu_enable)
				hevc_set_unused_4k_buff_idx(hw,
					p_H264_Dpb->mVideo.
						dec_picture->buf_spec_num);
			bufmgr_post(p_H264_Dpb);
			hw->last_dec_picture = p_H264_Dpb->mVideo.dec_picture;
			p_H264_Dpb->mVideo.dec_picture = NULL;
			/* dump_dpb(&p_H264_Dpb->mDPB); */
			hw->has_i_frame = 1;
			if (mmu_enable)
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

		amvdec_stop();
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s %s decode slice count %d\n",
			__func__,
			(dec_dpb_status == H264_PIC_DATA_DONE) ?
			"H264_PIC_DATA_DONE" :
			(dec_dpb_status == H264_FIND_NEXT_PIC_NAL) ?
			"H264_FIND_NEXT_PIC_NAL" : "H264_FIND_NEXT_DVEL_NAL",
			hw->decode_pic_count);
		/* WRITE_VREG(DPB_STATUS_REG, H264_ACTION_SEARCH_HEAD); */
		hw->dec_result = DEC_RESULT_DONE;
#ifdef CONFIG_AM_VDEC_DV
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
		vdec_schedule_work(&hw->work);
#ifdef CONFIG_AM_VDEC_DV
	} else if (
			(dec_dpb_status == H264_FIND_NEXT_PIC_NAL) ||
			(dec_dpb_status == H264_FIND_NEXT_DVEL_NAL)) {
		goto pic_done_proc;
#endif
	} else if (dec_dpb_status == H264_AUX_DATA_READY) {
		reset_process_time(hw);
		if (READ_VREG(H264_AUX_DATA_SIZE) != 0) {
			dma_sync_single_for_cpu(
			amports_get_dma_device(),
			hw->aux_phy_addr,
			hw->prefix_aux_size + hw->suffix_aux_size,
			DMA_FROM_DEVICE);
			if (dpb_is_debug(DECODE_ID(hw),
				PRINT_FLAG_DPB_DETAIL))
				dump_aux_buf(hw);
#ifdef CONFIG_AM_VDEC_DV
			if (vdec->dolby_meta_with_el || vdec->slave) {
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
#ifdef CONFIG_AM_VDEC_DV
		hw->switch_dvlayer_flag = 0;
		hw->got_valid_nal = 1;
#endif
		hw->dec_result = DEC_RESULT_DONE;
		vdec_schedule_work(&hw->work);
	} else if (/*(dec_dpb_status == H264_DATA_REQUEST) ||*/
			(dec_dpb_status == H264_SEARCH_BUFEMPTY) ||
			(dec_dpb_status == H264_DECODE_BUFEMPTY) ||
			(dec_dpb_status == H264_DECODE_TIMEOUT)) {
empty_proc:
		reset_process_time(hw);
		if (p_H264_Dpb->mVideo.dec_picture) {
			remove_picture(p_H264_Dpb,
				p_H264_Dpb->mVideo.dec_picture);
			p_H264_Dpb->mVideo.dec_picture = NULL;
		}

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
			amvdec_stop();
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
			else if (dec_dpb_status == H264_DECODE_TIMEOUT)
				hw->decode_timeout_num++;
			else if (dec_dpb_status == H264_DECODE_BUFEMPTY)
				hw->decode_dataempty_num++;

			hw->data_flag |= ERROR_FLAG;

			vdec_schedule_work(&hw->work);
		} else {
			/* WRITE_VREG(DPB_STATUS_REG, H264_ACTION_INIT); */
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
			hw->data_flag |= ERROR_FLAG;
			hw->stat |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
			reset_process_time(hw);
			return IRQ_HANDLED;
		}

	if (READ_VREG(AV_SCRATCH_G) == 1) {
		hw->sei_itu_data_len =
			(READ_VREG(H264_AUX_DATA_SIZE) >> 16) << 4;
		if (hw->sei_itu_data_len > SEI_ITU_DATA_SIZE * 2) {
				dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
					"itu data size more than 4K: %d, discarded it\n",
					hw->sei_itu_data_len);
				hw->sei_itu_data_len = 0;
		}

		if (hw->sei_itu_data_len != 0) {
			u8 *trans_data_buf;
			u8 *sei_data_buf;
			u32 temp;
			u32 *pswap_data;

			dma_sync_single_for_cpu(
			amports_get_dma_device(),
			hw->aux_phy_addr,
			hw->prefix_aux_size + hw->suffix_aux_size,
			DMA_FROM_DEVICE);
#if 0
			dump_aux_buf(hw);
#endif

			trans_data_buf = (u8 *)hw->aux_addr;
			sei_data_buf = (u8 *)hw->sei_itu_data_buf;
			for (i = 0; i < hw->sei_itu_data_len/2; i++)
				sei_data_buf[i] = trans_data_buf[i*2];
			hw->sei_itu_data_len = hw->sei_itu_data_len / 2;

			pswap_data = (u32 *)hw->sei_itu_data_buf;
			for (i = 0; i < hw->sei_itu_data_len/4; i = i+2) {
				temp = pswap_data[i];
				pswap_data[i] = pswap_data[i+1];
				pswap_data[i+1] = temp;
			}
		}
		WRITE_VREG(AV_SCRATCH_G, 0);
		return IRQ_HANDLED;
	}


	/* ucode debug */
	debug_tag = READ_VREG(DEBUG_REG1);
	if (debug_tag & 0x10000) {
		unsigned short *p = (unsigned short *)hw->lmem_addr;

		dma_sync_single_for_cpu(
			amports_get_dma_device(),
			hw->lmem_addr_remap,
			PAGE_SIZE,
			DMA_FROM_DEVICE);

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
		if ((udebug_pause_pos == (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx ==
			hw->decode_pic_count) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_VREG(DEBUG_REG2)))
			hw->ucode_pause_pos = udebug_pause_pos;
		else if (debug_tag & 0x20000)
			hw->ucode_pause_pos = 0xffffffff;
		if (hw->ucode_pause_pos)
			reset_process_time(hw);
		else
			WRITE_VREG(DEBUG_REG1, 0);
	} else if (debug_tag != 0) {
		dpb_print(DECODE_ID(hw), 0,
			"dbg%x: %x\n", debug_tag,
			READ_VREG(DEBUG_REG2));
		if ((udebug_pause_pos == (debug_tag & 0xffff)) &&
			(udebug_pause_decode_idx == 0 ||
			udebug_pause_decode_idx ==
			hw->decode_pic_count) &&
			(udebug_pause_val == 0 ||
			udebug_pause_val == READ_VREG(DEBUG_REG2)))
			hw->ucode_pause_pos = udebug_pause_pos;
		if (hw->ucode_pause_pos)
			reset_process_time(hw);
		else
			WRITE_VREG(DEBUG_REG1, 0);
	}
	/**/
	return IRQ_HANDLED;
}
static irqreturn_t vh264_isr(struct vdec_s *vdec)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;


	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	if (!hw) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"decoder is not running\n");
		return IRQ_HANDLED;
	}
	if (hw->eos)
		return IRQ_HANDLED;

	p_H264_Dpb->vdec = vdec;
	p_H264_Dpb->dec_dpb_status = READ_VREG(DPB_STATUS_REG);

	dpb_print(DECODE_ID(hw), PRINT_FLAG_UCODE_EVT,
			"%s DPB_STATUS_REG: 0x%x, ERROR_STATUS_REG 0x%x, sb (0x%x 0x%x 0x%x) bitcnt 0x%x mby_mbx 0x%x\n",
			__func__,
			p_H264_Dpb->dec_dpb_status,
			READ_VREG(ERROR_STATUS_REG),
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			READ_VREG(VIFF_BIT_CNT),
			READ_VREG(MBY_MBX));

	if (p_H264_Dpb->dec_dpb_status == H264_WRRSP_REQUEST) {
		if (mmu_enable)
			hevc_sao_wait_done(hw);
		WRITE_VREG(DPB_STATUS_REG, H264_WRRSP_DONE);
		return IRQ_HANDLED;
	}
	return IRQ_WAKE_THREAD;

}

static void timeout_process(struct vdec_h264_hw_s *hw)
{
	hw->timeout_num++;
	amvdec_stop();
	dpb_print(DECODE_ID(hw),
		PRINT_FLAG_ERROR, "%s decoder timeout\n", __func__);
	hw->dec_result = DEC_RESULT_DONE;
	hw->data_flag |= ERROR_FLAG;
	reset_process_time(hw);
	vdec_schedule_work(&hw->work);
}

static void vmh264_dump_state(struct vdec_s *vdec)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)(vdec->private);
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int i;
	dpb_print(DECODE_ID(hw), 0,
		"====== %s\n", __func__);
	dpb_print(DECODE_ID(hw), 0,
		"width/height (%d/%d), reorder_pic_num %d dpb size(bufspec count) %d max_reference_size(collocate count) %d\n",
		hw->frame_width,
		hw->frame_height,
		hw->dpb.reorder_pic_num,
		hw->dpb.mDPB.size,
		hw->max_reference_size
		);

	dpb_print(DECODE_ID(hw), 0,
		"is_framebase(%d), eos %d, state 0x%x, dec_result 0x%x dec_frm %d disp_frm %d run %d not_run_ready %d input_empty %d\n",
		input_frame_based(vdec),
		hw->eos,
		hw->stat,
		hw->dec_result,
		decode_frame_count[DECODE_ID(hw)],
		display_frame_count[DECODE_ID(hw)],
		run_count[DECODE_ID(hw)],
		not_run_ready[DECODE_ID(hw)],
		input_empty[DECODE_ID(hw)]
		);

	if (vf_get_receiver(vdec->vf_provider_name)) {
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
	"%s, newq(%d/%d), dispq(%d/%d) vf prepare/get/put (%d/%d/%d), free_spec(%d), initdon(%d), used_size(%d/%d), unused_fr_dpb(%d)\n",
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
	is_there_unused_frame_from_dpb(&p_H264_Dpb->mDPB)
	);

	dump_dpb(&p_H264_Dpb->mDPB, 1);
	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		dpb_print(DECODE_ID(hw), 0,
			"bufspec (%d): used %d adr 0x%x canvas(%d) vf_ref(%d) ",
			i, hw->buffer_spec[i].used,
			hw->buffer_spec[i].buf_adr,
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
		READ_PARSER_REG(PARSER_VIDEO_RP));
	dpb_print(DECODE_ID(hw), 0,
		"PARSER_VIDEO_WP=0x%x\n",
		READ_PARSER_REG(PARSER_VIDEO_WP));

	if (input_frame_based(vdec) &&
		dpb_is_debug(DECODE_ID(hw),
		PRINT_FRAMEBASE_DATA)
		) {
		int jj;
		if (hw->chunk && hw->chunk->block &&
			hw->chunk->size > 0) {
			u8 *data =
			((u8 *)hw->chunk->block->start_virt) +
				hw->chunk->offset;
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
		}
	}
}


static void check_timer_func(unsigned long arg)
{
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)arg;
	struct vdec_s *vdec = hw_to_vdec(hw);

	if ((h264_debug_cmd & 0x100) != 0 &&
		DECODE_ID(hw) == (h264_debug_cmd & 0xff)) {
		hw->dec_result = DEC_RESULT_DONE;
		vdec_schedule_work(&hw->work);
		pr_info("vdec %d is forced to be disconnected\n",
			h264_debug_cmd & 0xff);
		h264_debug_cmd = 0;
		return;
	}

	if (vdec->next_status == VDEC_STATUS_DISCONNECTED) {
		hw->dec_result = DEC_RESULT_FORCE_EXIT;
		vdec_schedule_work(&hw->work);
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

	if ((input_frame_based(vdec) ||
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
			} else
				start_process_time(hw);
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
	struct vdec_h264_hw_s *hw = (struct vdec_h264_hw_s *)vdec->private;
	vstatus->frame_width = hw->frame_width;
	vstatus->frame_height = hw->frame_height;
	if (hw->frame_dur != 0)
		vstatus->frame_rate = 96000 / hw->frame_dur;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = 0;
	vstatus->status = hw->stat;
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
		"%s-%02d", DRIVER_NAME, hw->id);

	return 0;
}

static int vh264_hw_ctx_restore(struct vdec_h264_hw_s *hw)
{
	int i, j;

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
	if (hw->dpb.mDPB.size > 0) {
		WRITE_VREG(AV_SCRATCH_7, (hw->max_reference_size << 24) |
			(hw->dpb.mDPB.size << 16) |
			(hw->dpb.mDPB.size << 8));

		for (j = 0; j < hw->dpb.mDPB.size; j++) {
			i = get_buf_spec_by_canvas_pos(hw, j);
			if (i < 0)
				break;

				if (!mmu_enable &&
					hw->buffer_spec[i].cma_alloc_addr)
					config_decode_canvas(hw, i);
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
	/*if (hw->ucode_type == UCODE_IP_ONLY_PARAM)
		SET_VREG_MASK(AV_SCRATCH_F, 1 << 6);
	else*/
		CLEAR_VREG_MASK(AV_SCRATCH_F, 1 << 6);

	WRITE_VREG(LMEM_DUMP_ADR, (u32)hw->lmem_addr_remap);
#if 1 /* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	WRITE_VREG(MDEC_PIC_DC_THRESH, 0x404038aa);
#endif

	WRITE_VREG(DEBUG_REG1, 0);
	WRITE_VREG(DEBUG_REG2, 0);
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
	if (i_only_flag & 0x100)
		hw->i_only = i_only_flag & 0xff;

	if ((unsigned long) hw->vh264_amstream_dec_info.param
		& 0x08)
		hw->no_poc_reorder_flag = 1;

	error_recovery_mode_in = 1; /*ucode control?*/
	if (error_proc_policy & 0x80000000)
		hw->send_error_frame_flag = error_proc_policy & 0x1;
	else if ((unsigned long) hw->vh264_amstream_dec_info.param & 0x20)
		hw->send_error_frame_flag = 1;

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
	hw->vh264_stream_switching_state = SWITCHING_STATE_OFF;
	hw->hevc_cur_buf_idx = 0xffff;

	INIT_WORK(&hw->work, vh264_work);
	INIT_WORK(&hw->notify_work, vh264_notify_work);
	INIT_WORK(&hw->user_data_work, user_data_push_work);

	return;
}

static s32 vh264_init(struct vdec_h264_hw_s *hw)
{
	/* int trickmode_fffb = 0; */
	int firmwareloaded = 0;

	hw->init_flag = 0;
	hw->eos = 0;
	hw->config_bufmgr_done = 0;
	hw->start_process_time = 0;
	hw->has_i_frame = 0;
	hw->no_error_count = 0xfff;
	hw->no_error_i_count = 0xf;
	/* pr_info("\nvh264_init\n"); */
	/* init_timer(&hw->recycle_timer); */

	/* timer init */
	init_timer(&hw->check_timer);

	hw->check_timer.data = (unsigned long)hw;
	hw->check_timer.function = check_timer_func;
	hw->check_timer.expires = jiffies + CHECK_INTERVAL;

	/* add_timer(&hw->check_timer); */
	hw->stat |= STAT_TIMER_ARM;
	hw->stat |= STAT_ISR_REG;

	hw->duration_on_correcting = 0;
	hw->fixed_frame_rate_check_count = 0;
	hw->saved_resolution = 0;

	vh264_local_init(hw);
	if (!amvdec_enable_flag) {
		amvdec_enable_flag = true;
		amvdec_enable();
		if (mmu_enable)
			amhevc_enable();
	}
	if (mmu_enable) {

		hw->frame_mmu_map_addr =
				dma_alloc_coherent(amports_get_dma_device(),
				FRAME_MMU_MAP_SIZE,
				&hw->frame_mmu_map_phy_addr, GFP_KERNEL);
		if (hw->frame_mmu_map_addr == NULL) {
			pr_err("%s: failed to alloc count_buffer\n", __func__);
			return -ENOMEM;
		}
	}

	/* -- ucode loading (amrisc and swap code) */
	hw->mc_cpu_addr =
		dma_alloc_coherent(amports_get_dma_device(), MC_TOTAL_SIZE,
				&hw->mc_dma_handle, GFP_KERNEL);
	if (!hw->mc_cpu_addr) {
		amvdec_enable_flag = false;
		amvdec_disable();
		if (mmu_enable)
			amhevc_disable();
		pr_info("vh264_init: Can not allocate mc memory.\n");
		return -ENOMEM;
	}

	/*pr_info("264 ucode swap area: phyaddr %p, cpu vaddr %p\n",
		(void *)hw->mc_dma_handle, hw->mc_cpu_addr);
	*/
	if (!firmwareloaded) {
		int r0 , r1 , r2 , r3 , r4 , r5, r6, r7, r8, r9;
		/*
		pr_info("start load orignal firmware ...\n");
		*/
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
		if (mmu_enable)
			r9 = amhevc_loadmc_ex(VFORMAT_HEVC,
					"vmh264_mc_mmu", NULL);
		else
			r9 = 0;
		if (r0 < 0 || r1 < 0 || r2 < 0 || r3 < 0 || r4 < 0 ||
			r5 < 0 || r6 < 0 || r7 < 0 || r8 < 0 || r9 < 0) {
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"264 load orignal firmware error %d,%d,%d,%d,%d,%d,%d,%d,%d\n",
				r0 , r1 , r2 , r3 , r4 , r5, r6, r7, r8);
			amvdec_disable();
			if (mmu_enable)
				amhevc_disable();
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
			dpb_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"%s: failed to map lmem_addr\n", __func__);
			free_page(hw->lmem_addr);
			hw->lmem_addr = 0;
			hw->lmem_addr_remap = 0;
			return -ENOMEM;
		}

		pr_info("%s, vaddr=%lx phy_addr=%p\n",
			__func__, hw->lmem_addr, (void *)hw->lmem_addr_remap);
	}

	if (prefix_aux_buf_size > 0 ||
		suffix_aux_buf_size > 0) {
		u32 aux_buf_size;
		hw->prefix_aux_size = AUX_BUF_ALIGN(prefix_aux_buf_size);
		hw->suffix_aux_size = AUX_BUF_ALIGN(suffix_aux_buf_size);
		aux_buf_size = hw->prefix_aux_size + hw->suffix_aux_size;
		hw->aux_addr = kmalloc(aux_buf_size, GFP_KERNEL);
		if (hw->aux_addr == NULL) {
			pr_err("%s: failed to alloc rpm buffer\n", __func__);
			return -1;
		}

		hw->aux_phy_addr = dma_map_single(amports_get_dma_device(),
			hw->aux_addr, aux_buf_size, DMA_FROM_DEVICE);
		if (dma_mapping_error(amports_get_dma_device(),
			hw->aux_phy_addr)) {
			pr_err("%s: failed to map rpm buffer\n", __func__);
			kfree(hw->aux_addr);
			hw->aux_addr = NULL;
			return -1;
		}
		hw->sei_itu_data_buf = kmalloc(SEI_ITU_DATA_SIZE, GFP_KERNEL);
		if (hw->sei_itu_data_buf == NULL) {
			pr_err("%s: failed to alloc sei itu data buffer\n",
				__func__);
			return -1;
		}

		if (NULL == hw->sei_user_data_buffer) {
			hw->sei_user_data_buffer = kmalloc(USER_DATA_SIZE,
								GFP_KERNEL);
			if (!hw->sei_user_data_buffer) {
				pr_info("%s: Can not allocate sei_data_buffer\n",
					   __func__);
				return -1;
			}
			hw->sei_user_data_wp = 0;
		}
	}
/* BUFFER_MGR_IN_C */
#endif
	hw->stat |= STAT_MC_LOAD;
	if (mmu_enable) {
		WRITE_VREG(HEVC_ASSIST_SCRATCH_0, 0x0);
		amhevc_start();
	}
	/* add memory barrier */
	wmb();

	return 0;
}

static int vh264_stop(struct vdec_h264_hw_s *hw)
{
	cancel_work_sync(&hw->work);
	cancel_work_sync(&hw->notify_work);
	cancel_work_sync(&hw->user_data_work);

	if (hw->stat & STAT_MC_LOAD) {
		if (hw->mc_cpu_addr != NULL) {
			dma_free_coherent(amports_get_dma_device(),
					MC_TOTAL_SIZE, hw->mc_cpu_addr,
					hw->mc_dma_handle);
			hw->mc_cpu_addr = NULL;
		}
		if (hw->frame_mmu_map_addr != NULL) {
			dma_free_coherent(amports_get_dma_device(),
				FRAME_MMU_MAP_SIZE, hw->frame_mmu_map_addr,
					hw->frame_mmu_map_phy_addr);
			hw->frame_mmu_map_addr = NULL;
		}

	}
	if (hw->stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_1, (void *)hw);
		hw->stat &= ~STAT_ISR_REG;
	}
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
	if (hw->aux_addr) {
		dma_unmap_single(amports_get_dma_device(),
			hw->aux_phy_addr,
			hw->prefix_aux_size + hw->suffix_aux_size,
			DMA_FROM_DEVICE);
		kfree(hw->aux_addr);
		hw->aux_addr = NULL;
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

	dpb_print(DECODE_ID(hw), 0,
		"%s\n",
		__func__);
	return 0;
}

static void vh264_notify_work(struct work_struct *work)
{
	struct vdec_h264_hw_s *hw = container_of(work,
					struct vdec_h264_hw_s, notify_work);
	struct vdec_s *vdec = hw_to_vdec(hw);
	if (vdec->fr_hint_state == VDEC_NEED_HINT) {
		vf_notify_receiver(vdec->vf_provider_name,
				VFRAME_EVENT_PROVIDER_FR_HINT,
				(void *)((unsigned long)hw->frame_dur));
		vdec->fr_hint_state = VDEC_HINTED;
	}

	return;
}

static void user_data_push_work(struct work_struct *work)
{
	struct vdec_h264_hw_s *hw = container_of(work,
		struct vdec_h264_hw_s, user_data_work);

	struct userdata_poc_info_t user_data_poc;
	unsigned char *pdata;
	u8 *pmax_sei_data_buffer;
	u8 *sei_data_buf;
	int i;

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
	user_data_poc.poc_number = hw->sei_poc;

	wakeup_userdata_poll(user_data_poc, hw->sei_user_data_wp,
					(unsigned long)hw->sei_user_data_buffer,
					USER_DATA_SIZE, hw->sei_itu_data_len);
	hw->sei_itu_data_len = 0;
/*
	pr_info("sei_itu35_wp = %d, poc = %d\n",
		hw->sei_user_data_wp, hw->sei_poc);
*/
}

static void vh264_work(struct work_struct *work)
{
	struct vdec_h264_hw_s *hw = container_of(work,
		struct vdec_h264_hw_s, work);
	struct vdec_s *vdec = hw_to_vdec(hw);

	/* finished decoding one frame or error,
	 * notify vdec core to switch context
	 */
	dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_DETAIL,
		"%s dec_result %d %x %x %x\n",
		__func__,
		hw->dec_result,
		READ_VREG(VLD_MEM_VIFIFO_LEVEL),
		READ_VREG(VLD_MEM_VIFIFO_WP),
		READ_VREG(VLD_MEM_VIFIFO_RP));
		if (!mmu_enable) {
			mutex_lock(&vmh264_mutex);
			dealloc_buf_specs(hw);
			mutex_unlock(&vmh264_mutex);
		}
	if (hw->dec_result == DEC_RESULT_CONFIG_PARAM) {
		if (vh264_set_params(hw) < 0)
			hw->stat |= DECODER_FATAL_ERROR_SIZE_OVERFLOW;
		start_process_time(hw);
		return;
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
			vdec_vframe_dirty(vdec, hw->chunk);
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
			if (r < 0) {
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
				u8 *data =
				((u8 *)hw->chunk->block->start_virt) +
					hw->chunk->offset;
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
			hw->dec_result = DEC_RESULT_GET_DATA_RETRY;
			vdec_schedule_work(&hw->work);
		}
		return;
	} else if (hw->dec_result == DEC_RESULT_DONE) {
		/* if (!hw->ctx_valid)
			hw->ctx_valid = 1; */
result_done:
		decode_frame_count[DECODE_ID(hw)]++;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s dec_result %d %x %x %x\n",
			__func__,
			hw->dec_result,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP));
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
		struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s: end of stream\n",
			__func__);

		hw->eos = 1;
		flush_dpb(p_H264_Dpb);
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
	} else if (hw->dec_result == DEC_RESULT_FORCE_EXIT) {
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s: force exit\n",
			__func__);
		amvdec_stop();
		if (hw->stat & STAT_ISR_REG) {
			vdec_free_irq(VDEC_IRQ_1, (void *)hw);
			hw->stat &= ~STAT_ISR_REG;
		}
	}

	del_timer_sync(&hw->check_timer);
	hw->stat &= ~STAT_TIMER_ARM;

	/* mark itself has all HW resource released and input released */
	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_CONNECTED);

#ifdef CONFIG_AM_VDEC_DV
	if (hw->switch_dvlayer_flag) {
		if (vdec->slave)
			vdec_set_next_sched(vdec, vdec->slave);
		else if (vdec->master)
			vdec_set_next_sched(vdec, vdec->master);
	} else if (vdec->slave || vdec->master)
		vdec_set_next_sched(vdec, vdec);
#endif

	if (hw->vdec_cb)
		hw->vdec_cb(hw_to_vdec(hw), hw->vdec_cb_arg);
}

static bool run_ready(struct vdec_s *vdec)
{
	bool ret = 0;
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)vdec->private;

#ifndef CONFIG_AM_VDEC_DV
	if (vdec->master)
		return false;
#endif
	if (hw->eos)
		return 0;

	if (h264_debug_flag & 0x20000000) {
		/* pr_info("%s, a\n", __func__); */
		ret = 1;
	} else
		ret = is_buffer_available(vdec);

	if (ret)
		not_run_ready[DECODE_ID(hw)] = 0;
	else
		not_run_ready[DECODE_ID(hw)]++;
	return ret;
}

static unsigned char get_data_check_sum
	(struct vdec_h264_hw_s *hw, int size)
{
	int jj;
	int sum = 0;
	u8 *data = ((u8 *)hw->chunk->block->start_virt) +
		hw->chunk->offset;
	for (jj = 0; jj < size; jj++)
		sum += data[jj];
	return sum;
}

static void run(struct vdec_s *vdec,
	void (*callback)(struct vdec_s *, void *), void *arg)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)vdec->private;
	int size;
	run_count[DECODE_ID(hw)]++;

	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;

	if (h264_debug_cmd & 0xf000) {
		h264_reconfig(hw);
		h264_debug_cmd &= (~0xf000);
	}
	/* hw->chunk = vdec_prepare_input(vdec); */
#ifdef CONFIG_AM_VDEC_DV
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

	if (input_frame_based(vdec)) {
		u8 *data = ((u8 *)hw->chunk->block->start_virt) +
			hw->chunk->offset;
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
			u8 *data =
			((u8 *)hw->chunk->block->start_virt) +
				hw->chunk->offset;
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

	} else
		dpb_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s: %x %x %x %x %x size 0x%x\n",
			__func__,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			READ_PARSER_REG(PARSER_VIDEO_RP),
			READ_PARSER_REG(PARSER_VIDEO_WP),
			size);

	start_process_time(hw);

	if (amvdec_vdec_loadmc_ex(vdec, "vmh264_mc") < 0) {
		amvdec_enable_flag = false;
		amvdec_disable();
			if (mmu_enable)
				amhevc_disable();
		pr_info("%s: Error amvdec_vdec_loadmc fail\n",
			__func__);
		return;
	}

	if (vh264_hw_ctx_restore(hw) < 0) {
		vdec_schedule_work(&hw->work);
		return;
	}
	if (input_frame_based(vdec)) {
		int decode_size = hw->chunk->size +
			(hw->chunk->offset & (VDEC_FIFO_ALIGN - 1));
		WRITE_VREG(H264_DECODE_INFO, (1<<13));
		WRITE_VREG(H264_DECODE_SIZE, decode_size);
		WRITE_VREG(VIFF_BIT_CNT, decode_size * 8);
	} else {
		if (size <= 0)
			size = 0x7fffffff; /*error happen*/
		WRITE_VREG(H264_DECODE_INFO, (1<<13));
		WRITE_VREG(H264_DECODE_SIZE, size);
		WRITE_VREG(VIFF_BIT_CNT, size * 8);
	}
	config_aux_buf(hw);
	config_decode_mode(hw);
	vdec_enable_input(vdec);
	WRITE_VREG(NAL_SEARCH_CTL, 0);
	if (enable_itu_t35)
		WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | 0x1);
	else if (mmu_enable)
		WRITE_VREG(NAL_SEARCH_CTL, READ_VREG(NAL_SEARCH_CTL) | 0x2);
	if (udebug_flag)
		WRITE_VREG(AV_SCRATCH_K, udebug_flag);
	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);

	amvdec_start();

	/* if (hw->init_flag) { */
		WRITE_VREG(DPB_STATUS_REG, H264_ACTION_SEARCH_HEAD);
	/* } */

	hw->init_flag = 1;
}

static void reset(struct vdec_s *vdec)
{
	pr_info("ammvdec_h264: reset.\n");
}

static void h264_reconfig(struct vdec_h264_hw_s *hw)
{
	int i;
	unsigned long flags;
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	dpb_print(DECODE_ID(hw), 0,
	"%s\n", __func__);
	/* after calling flush_dpb() and bufmgr_h264_remove_unused_frame(),
		all buffers are in display queue (used == 2),
			or free (used == 0)
	*/
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
		/*make sure buffers not put back to bufmgr when
			vf_put is called*/
		if (hw->buffer_spec[i].used == 2)
			hw->buffer_spec[i].used = 3;

		/* ready to release "free buffers"
		*/
		if (hw->buffer_spec[i].used == 0)
			hw->buffer_spec[i].used = 4;

		hw->buffer_spec[i].canvas_pos = -1;
	}
	spin_unlock_irqrestore(&hw->bufspec_lock, flags);
	hw->has_i_frame = 0;
	hw->config_bufmgr_done = 0;

}

static void h264_reset_bufmgr(struct vdec_h264_hw_s *hw)
{
	int i;
	struct h264_dpb_stru *p_H264_Dpb = &hw->dpb;
	int actual_dpb_size, max_reference_size;
	int reorder_pic_num;
	unsigned int colocated_buf_size;
	unsigned int colocated_mv_addr_start;
	unsigned int colocated_mv_addr_end;
	dpb_print(DECODE_ID(hw), 0,
	"%s\n", __func__);
	/* after calling flush_dpb() and bufmgr_h264_remove_unused_frame(),
		all buffers are in display queue (used == 2),
			or free (used == 0)
	*/
	flush_dpb(p_H264_Dpb);
	bufmgr_h264_remove_unused_frame(p_H264_Dpb, 0);

	for (i = 0; i < BUFSPEC_POOL_SIZE; i++) {
		/*make sure buffers not put back to bufmgr when
			vf_put is called*/
		if (hw->buffer_spec[i].used == 2)
			hw->buffer_spec[i].used = 5;
	}

	actual_dpb_size = p_H264_Dpb->mDPB.size;
	max_reference_size = p_H264_Dpb->max_reference_size;
	reorder_pic_num = p_H264_Dpb->reorder_pic_num;

	colocated_buf_size = p_H264_Dpb->colocated_buf_size;
	colocated_mv_addr_start = p_H264_Dpb->colocated_mv_addr_start;
	colocated_mv_addr_end  = p_H264_Dpb->colocated_mv_addr_end;

	dpb_init_global(&hw->dpb,
		DECODE_ID(hw), 0, 0);
	p_H264_Dpb->mDPB.size = actual_dpb_size;
	p_H264_Dpb->max_reference_size = max_reference_size;
	p_H264_Dpb->reorder_pic_num = reorder_pic_num;

	p_H264_Dpb->colocated_buf_size = colocated_buf_size;
	p_H264_Dpb->colocated_mv_addr_start = colocated_mv_addr_start;
	p_H264_Dpb->colocated_mv_addr_end  = colocated_mv_addr_end;

	p_H264_Dpb->fast_output_enable = fast_output_enable;
}

int ammvdec_h264_mmu_init(struct vdec_h264_hw_s *hw)
{
	int ret = -1;
	int tvp_flag = vdec_secure(hw_to_vdec(hw)) ?
		CODEC_MM_FLAGS_TVP : 0;

	pr_info("ammvdec_h264_mmu_init tvp = 0x%x mmu_enable %d\n",
			tvp_flag, mmu_enable);
	if (mmu_enable && !hw->mmu_box) {
		hw->mmu_box = decoder_mmu_box_alloc_box(DRIVER_NAME,
				hw->id,
				MMU_MAX_BUFFERS,
				64 * SZ_1M,
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
			tvp_flag);
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
	platform_set_drvdata(pdev, pdata);

	mmu_enable = 0;
	if (force_enable_mmu && pdata->sys_info &&
		    (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXLX) &&
			(pdata->sys_info->height * pdata->sys_info->width
			> 1920 * 1088))
			mmu_enable = 1;
	if (ammvdec_h264_mmu_init(hw)) {
		h264_free_hw_stru(&pdev->dev, (void *)hw);
		pr_info("\nammvdec_h264 mmu alloc failed!\n");
		return -ENOMEM;
	}
	pdata->private = hw;
	pdata->dec_status = dec_status;
	pdata->set_trickmode = vmh264_set_trickmode;
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vh264_isr;
	pdata->threaded_irq_handler = vh264_isr_thread_fn;
	pdata->dump_state = vmh264_dump_state;


	if (pdata->use_vfm_path)
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
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
	}
#endif
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			PROVIDER_NAME ".%02x", pdev->id & 0xff);

	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vf_provider_ops, pdata);

	platform_set_drvdata(pdev, pdata);

	buf_spec_init(hw);

	hw->platform_dev = pdev;


	if (decoder_bmmu_box_alloc_buf_phy(hw->bmmu_box, BMMU_DPB_IDX,
		V_BUF_ADDR_OFFSET, DRIVER_NAME, &hw->cma_alloc_addr) < 0) {
		h264_free_hw_stru(&pdev->dev, (void *)hw);
		return -ENOMEM;
	}

	hw->buf_offset = hw->cma_alloc_addr - DEF_BUF_START_ADDR +
			DCAC_READ_MARGIN;
	if (mmu_enable)
		if (decoder_bmmu_box_alloc_buf_phy(hw->bmmu_box, BMMU_EXTIF_IDX,
			EXTIF_BUF_SIZE, DRIVER_NAME, &hw->extif_addr) < 0) {
			h264_free_hw_stru(&pdev->dev, (void *)hw);
			return -ENOMEM;
		}
	if (!vdec_secure(pdata)) {
#if 1
		/*init internal buf*/
		tmpbuf = (char *)codec_mm_phys_to_virt(hw->cma_alloc_addr);
		memset(tmpbuf, 0, V_BUF_ADDR_OFFSET);
		dma_sync_single_for_device(amports_get_dma_device(),
			hw->cma_alloc_addr,
			V_BUF_ADDR_OFFSET, DMA_TO_DEVICE);
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

	if (pdata->sys_info)
		hw->vh264_amstream_dec_info = *pdata->sys_info;
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
	pr_info("ammvdec_h264 mem-addr=%lx,buff_offset=%x,buf_start=%lx\n",
		pdata->mem_start, hw->buf_offset, hw->cma_alloc_addr);


	vdec_source_changed(VFORMAT_H264, 3840, 2160, 60);

	if (vh264_init(hw) < 0) {
		pr_info("\nammvdec_h264 init failed.\n");
		ammvdec_h264_mmu_release(hw);
		h264_free_hw_stru(&pdev->dev, (void *)hw);
		return -ENODEV;
	}

	vdec_set_prepare_level(pdata, start_decode_buf_level);

	atomic_set(&hw->vh264_active, 1);

	return 0;
}

static int ammvdec_h264_remove(struct platform_device *pdev)
{
	struct vdec_h264_hw_s *hw =
		(struct vdec_h264_hw_s *)
		(((struct vdec_s *)(platform_get_drvdata(pdev)))->private);
	int i;

	for (i = 0; i < BUFSPEC_POOL_SIZE; i++)
		release_aux_data(hw, i);

	atomic_set(&hw->vh264_active, 0);

	if (hw->stat & STAT_TIMER_ARM) {
		del_timer_sync(&hw->check_timer);
		hw->stat &= ~STAT_TIMER_ARM;
	}

	vh264_stop(hw);

	/* vdec_source_changed(VFORMAT_H264, 0, 0, 0); */

	atomic_set(&hw->vh264_active, 0);

	vdec_set_status(hw_to_vdec(hw), VDEC_STATUS_DISCONNECTED);
	ammvdec_h264_mmu_release(hw);
	h264_free_hw_stru(&pdev->dev, (void *)hw);
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
#ifdef CONFIG_AM_VDEC_DV
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
	vcodec_profile_register(&ammvdec_h264_profile);
	INIT_REG_NODE_CONFIGS("media.decoder", &hm264_node,
	"mh264", hm264_configs, CONFIG_FOR_RW);
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

module_param(fixed_frame_rate_mode, uint, 0664);
MODULE_PARM_DESC(fixed_frame_rate_mode, "\namvdec_h264 fixed_frame_rate_mode\n");

module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val, "\n amvdec_h264 decode_timeout_val\n");

module_param(get_data_timeout_val, uint, 0664);
MODULE_PARM_DESC(get_data_timeout_val, "\n amvdec_h264 get_data_timeout_val\n");

module_param(frame_max_data_packet, uint, 0664);
MODULE_PARM_DESC(frame_max_data_packet, "\n amvdec_h264 frame_max_data_packet\n");

module_param(reorder_dpb_size_margin, uint, 0664);
MODULE_PARM_DESC(reorder_dpb_size_margin, "\n ammvdec_h264 reorder_dpb_size_margin\n");

#ifdef CONFIG_AM_VDEC_DV
module_param(reorder_dpb_size_margin_dv, uint, 0664);
MODULE_PARM_DESC(reorder_dpb_size_margin_dv,
	"\n ammvdec_h264 reorder_dpb_size_margin_dv\n");
#endif

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

module_param(force_disp_bufspec_num, int, 0664);
MODULE_PARM_DESC(force_disp_bufspec_num, "\n amvdec_h264 force_disp_bufspec_num\n");

module_param(V_BUF_ADDR_OFFSET, int, 0664);
MODULE_PARM_DESC(V_BUF_ADDR_OFFSET, "\n amvdec_h264 V_BUF_ADDR_OFFSET\n");

module_param(prefix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(prefix_aux_buf_size, "\n prefix_aux_buf_size\n");

module_param(suffix_aux_buf_size, uint, 0664);
MODULE_PARM_DESC(suffix_aux_buf_size, "\n suffix_aux_buf_size\n");

#ifdef CONFIG_AM_VDEC_DV
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

module_param(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n amvdec_h265 udebug_flag\n");

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

module_param(mmu_enable, uint, 0664);
MODULE_PARM_DESC(mmu_enable, "\n mmu_enable\n");

module_param(force_enable_mmu, uint, 0664);
MODULE_PARM_DESC(force_enable_mmu, "\n force_enable_mmu\n");

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

module_init(ammvdec_h264_driver_init_module);
module_exit(ammvdec_h264_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC H264 Video Decoder Driver");
MODULE_LICENSE("GPL");
