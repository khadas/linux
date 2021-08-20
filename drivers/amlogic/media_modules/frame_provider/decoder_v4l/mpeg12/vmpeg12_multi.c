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
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/registers/register.h>
#include <media/v4l2-mem2mem.h>
#include <uapi/linux/tee.h>
#include "../../../stream_input/amports/amports_priv.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"
#include "../../decoder/utils/vdec_input.h"
#include "../../decoder/utils/vdec.h"
#include "../../decoder/utils/amvdec.h"
#include "../../decoder/utils/decoder_mmu_box.h"
#include "../../decoder/utils/decoder_bmmu_box.h"
#include "../../decoder/utils/config_parser.h"
#include "../../decoder/utils/firmware.h"
#include "../../decoder/utils/vdec_v4l2_buffer_ops.h"
#include "../../decoder/utils/config_parser.h"
#include "../../decoder/utils/vdec_feature.h"

#define MEM_NAME "codec_mmpeg12"
#define CHECK_INTERVAL        (HZ/100)

#define DRIVER_NAME "ammvdec_mpeg12_v4l"
#define MREG_REF0        AV_SCRATCH_2
#define MREG_REF1        AV_SCRATCH_3
/* protocol registers */
#define MREG_SEQ_INFO       AV_SCRATCH_4
#define MREG_PIC_INFO       AV_SCRATCH_5
#define MREG_PIC_WIDTH      AV_SCRATCH_6
#define MREG_PIC_HEIGHT     AV_SCRATCH_7
#define MREG_INPUT          AV_SCRATCH_8  /*input_type*/
#define MREG_BUFFEROUT      AV_SCRATCH_9  /*FROM_AMRISC_REG*/

#define MREG_CMD            AV_SCRATCH_A
#define MREG_CO_MV_START    AV_SCRATCH_B
#define MREG_ERROR_COUNT    AV_SCRATCH_C
#define MREG_FRAME_OFFSET   AV_SCRATCH_D
#define MREG_WAIT_BUFFER    AV_SCRATCH_E
#define MREG_FATAL_ERROR    AV_SCRATCH_F

#define MREG_CC_ADDR    AV_SCRATCH_0
#define AUX_BUF_ALIGN(adr) ((adr + 0xf) & (~0xf))

#define GET_SLICE_TYPE(type)  ("IPB##"[((type&PICINFO_TYPE_MASK)>>16)&0x3])
#define PICINFO_ERROR       0x80000000
#define PICINFO_TYPE_MASK   0x00030000
#define PICINFO_TYPE_I      0x00000000
#define PICINFO_TYPE_P      0x00010000
#define PICINFO_TYPE_B      0x00020000
#define PICINFO_PROG        0x8000
#define PICINFO_RPT_FIRST   0x4000
#define PICINFO_TOP_FIRST   0x2000
#define PICINFO_FRAME       0x1000
#define TOP_FIELD            0x1000
#define BOTTOM_FIELD         0x2000
#define FRAME_PICTURE        0x3000
#define FRAME_PICTURE_MASK   0x3000

#define SEQINFO_EXT_AVAILABLE   0x80000000
#define SEQINFO_PROG            0x00010000
#define CCBUF_SIZE      (5*1024)

#define VF_POOL_SIZE        64
#define DECODE_BUFFER_NUM_MAX 16
#define DECODE_BUFFER_NUM_DEF 8
#define MAX_BMMU_BUFFER_NUM (DECODE_BUFFER_NUM_MAX + 1)

#define PUT_INTERVAL        (HZ/100)
#define WORKSPACE_SIZE		(4*SZ_64K) /*swap&ccbuf&matirx&MV*/
#define CTX_LMEM_SWAP_OFFSET    0
#define CTX_CCBUF_OFFSET        0x800
#define CTX_QUANT_MATRIX_OFFSET (CTX_CCBUF_OFFSET + 5*1024)
#define CTX_CO_MV_OFFSET        (CTX_QUANT_MATRIX_OFFSET + 1*1024)
#define CTX_DECBUF_OFFSET       (CTX_CO_MV_OFFSET + 0x11000)

#define DEFAULT_MEM_SIZE	(32*SZ_1M)

#define INVALID_IDX 		(-1)  /* Invalid buffer index.*/

static int pre_decode_buf_level = 0x800;
static int start_decode_buf_level = 0x4000;
static u32 dec_control;
static u32 error_frame_skip_level = 1;
static u32 udebug_flag;
static unsigned int radr;
static unsigned int rval;

static u32 without_display_mode;
static u32 dynamic_buf_num_margin = 2;

#define VMPEG12_DEV_NUM        9
static unsigned int max_decode_instance_num = VMPEG12_DEV_NUM;
static unsigned int max_process_time[VMPEG12_DEV_NUM];
static unsigned int decode_timeout_val = 200;
#define INCPTR(p) ptr_atomic_wrap_inc(&p)

#define DEC_CONTROL_FLAG_FORCE_2500_720_576_INTERLACE  0x0002
#define DEC_CONTROL_FLAG_FORCE_3000_704_480_INTERLACE  0x0004
#define DEC_CONTROL_FLAG_FORCE_2500_704_576_INTERLACE  0x0008
#define DEC_CONTROL_FLAG_FORCE_2500_544_576_INTERLACE  0x0010
#define DEC_CONTROL_FLAG_FORCE_2500_480_576_INTERLACE  0x0020
#define DEC_CONTROL_INTERNAL_MASK                      0x0fff
#define DEC_CONTROL_FLAG_FORCE_SEQ_INTERLACE           0x1000

#define INTERLACE_SEQ_ALWAYS

#if 1/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
#define NV21
#endif

#define AGAIN_HAS_THRESHOLD

#ifdef AGAIN_HAS_THRESHOLD
static u32 again_threshold;
#endif

enum {
	FRAME_REPEAT_TOP,
	FRAME_REPEAT_BOT,
	FRAME_REPEAT_NONE
};

/*Send by AV_SCRATCH_9*/
#define MPEG12_PIC_DONE     1
#define MPEG12_DATA_EMPTY   2
#define MPEG12_SEQ_END      3
#define MPEG12_DATA_REQUEST 4

/*Send by AV_SCRATCH_G*/
#define MPEG12_V4L2_INFO_NOTIFY 1
/*Send by AV_SCRATCH_J*/
#define MPEG12_USERDATA_DONE 0x8000

#define DEC_RESULT_NONE     0
#define DEC_RESULT_DONE     1
#define DEC_RESULT_AGAIN    2
#define DEC_RESULT_ERROR    3
#define DEC_RESULT_FORCE_EXIT 4
#define DEC_RESULT_EOS 5
#define DEC_RESULT_GET_DATA         6
#define DEC_RESULT_GET_DATA_RETRY   7

#define DEC_DECODE_TIMEOUT         0x21
#define DECODE_ID(hw) (hw_to_vdec(hw)->id)
#define DECODE_STOP_POS         AV_SCRATCH_K
#define USERDATA_FIFO_NUM    256
#define MAX_FREE_USERDATA_NODES		5

struct mmpeg2_userdata_record_t {
	struct userdata_meta_info_t meta_info;
	u32 rec_start;
	u32 rec_len;
};

struct mmpeg2_userdata_info_t {
	struct mmpeg2_userdata_record_t records[USERDATA_FIFO_NUM];
	u8 *data_buf;
	u8 *data_buf_end;
	u32 buf_len;
	u32 read_index;
	u32 write_index;
	u32 last_wp;
};
#define MAX_UD_RECORDS	5

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
	u64 last_timestamp;
};

struct vdec_mpeg12_hw_s {
	spinlock_t lock;
	struct platform_device *platform_dev;
	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
	struct vframe_s vfpool[VF_POOL_SIZE];
	struct vframe_s vframe_dummy;
	s32 vfbuf_use[DECODE_BUFFER_NUM_MAX];
	s32 ref_use[DECODE_BUFFER_NUM_MAX];
	u32 frame_width;
	u32 frame_height;
	u32 frame_dur;
	u32 frame_prog;
	u32 seqinfo;
	u32 ctx_valid;
	u32 dec_control;
	void *mm_blk_handle;
	struct vframe_chunk_s *chunk;
	u32 stat;
	u8 init_flag;
	unsigned long buf_start;
	u32 buf_size;
	u32 reg_pic_width;
	u32 reg_pic_height;
	u32 reg_mpeg1_2_reg;
	u32 reg_pic_head_info;
	u32 reg_f_code_reg;
	u32 reg_slice_ver_pos_pic_type;
	u32 reg_vcop_ctrl_reg;
	u32 reg_mb_info;
	u32 reg_signal_type;
	u32 dec_num;
	struct timer_list check_timer;
	u32 decode_timeout_count;
	unsigned long int start_process_time;
	u32 last_vld_level;
	u32 eos;

	struct pic_info_t pics[DECODE_BUFFER_NUM_MAX];
	u32 canvas_spec[DECODE_BUFFER_NUM_MAX];
	u64 lastpts64;
	u32 last_chunk_pts;
	struct canvas_config_s canvas_config[DECODE_BUFFER_NUM_MAX][2];
	struct dec_sysinfo vmpeg12_amstream_dec_info;

	s32 refs[2];
	int dec_result;
	u32 timeout_processing;
	struct work_struct work;
	struct work_struct timeout_work;
	struct work_struct notify_work;
	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;
	dma_addr_t ccbuf_phyAddress;
	void *ccbuf_phyAddress_virt;
	u32 cc_buf_size;
	unsigned long ccbuf_phyAddress_is_remaped_nocache;
	u32 frame_rpt_state;
	/* for error handling */
	s32 frame_force_skip_flag;
	s32 error_frame_skip_level;
	s32 wait_buffer_counter;
	u32 first_i_frame_ready;
	u32 run_count;
	u32	not_run_ready;
	u32	input_empty;
	atomic_t disp_num;
	atomic_t put_num;
	atomic_t peek_num;
	atomic_t get_num;
	u32 drop_frame_count;
	u32 buffer_not_ready;
	u32 ratio_control;
	int frameinfo_enable;
	struct firmware_s *fw;
	u32 canvas_mode;
#ifdef AGAIN_HAS_THRESHOLD
	u32 pre_parser_wr_ptr;
	u8 next_again_flag;
#endif

	struct work_struct userdata_push_work;
	struct mutex userdata_mutex;
	struct mmpeg2_userdata_info_t userdata_info;
	struct mmpeg2_userdata_record_t ud_record[MAX_UD_RECORDS];
	int cur_ud_idx;
	u8 *user_data_buffer;
	int wait_for_udr_send;
	u32 ucode_cc_last_wp;
	u32 notify_ucode_cc_last_wp;
	u32 notify_data_cc_last_wp;
	u32 userdata_wp_ctx;
#ifdef DUMP_USER_DATA
#define MAX_USER_DATA_SIZE		1572864
	void *user_data_dump_buf;
	unsigned char *pdump_buf_cur_start;
	int total_len;
	int bskip;
	int n_userdata_id;
	u32 reference[MAX_UD_RECORDS];
#endif
	int tvp_flag;
	bool is_used_v4l;
	void *v4l2_ctx;
	bool v4l_params_parsed;
	u32 buf_num;
	u32 dynamic_buf_num_margin;
	struct vdec_info gvs;
	struct vframe_qos_s vframe_qos;
	u32 res_ch_flag;
	u32 i_only;
	u32 kpi_first_i_comming;
	u32 kpi_first_i_decoded;
	int sidebind_type;
	int sidebind_channel_id;
	u32 profile_idc;
	u32 level_idc;
	int dec_again_cnt;
	int vdec_pg_enable_flag;
	ulong fb_token;
	bool force_prog_only;
	char vdec_name[32];
	char pts_name[32];
	char new_q_name[32];
	char disp_q_name[32];
	u32 chunk_header_offset;
	u32 chunk_res_size;
	u64 first_field_timestamp;
	u64 first_field_timestamp_valid;
	u32 report_field;
};
static void vmpeg12_local_init(struct vdec_mpeg12_hw_s *hw);
static int vmpeg12_hw_ctx_restore(struct vdec_mpeg12_hw_s *hw);
static void reset_process_time(struct vdec_mpeg12_hw_s *hw);
static void vmpeg12_workspace_init(struct vdec_mpeg12_hw_s *hw);
static void flush_output(struct vdec_mpeg12_hw_s *hw);
static struct vframe_s *vmpeg_vf_peek(void *);
static struct vframe_s *vmpeg_vf_get(void *);
static void vmpeg_vf_put(struct vframe_s *, void *);
static int vmpeg_vf_states(struct vframe_states *states, void *);
static int vmpeg_event_cb(int type, void *data, void *private_data);
static int notify_v4l_eos(struct vdec_s *vdec);
static void start_process_time_set(struct vdec_mpeg12_hw_s *hw);
static int debug_enable;

#undef pr_info
#define pr_info pr_cont
unsigned int mpeg12_debug_mask = 0xff;
static u32 run_ready_min_buf_num = 2;
static int dirty_again_threshold = 100;
static int error_proc_policy = 0x1;

#define PRINT_FLAG_ERROR              0x0
#define PRINT_FLAG_RUN_FLOW           0X0001
#define PRINT_FLAG_TIMEINFO           0x0002
#define PRINT_FLAG_UCODE_DETAIL		  0x0004
#define PRINT_FLAG_VLD_DETAIL         0x0008
#define PRINT_FLAG_DEC_DETAIL         0x0010
#define PRINT_FLAG_BUFFER_DETAIL      0x0020
#define PRINT_FLAG_RESTORE            0x0040
#define PRINT_FRAME_NUM               0x0080
#define PRINT_FLAG_FORCE_DONE         0x0100
#define PRINT_FLAG_COUNTER            0X0200
#define PRINT_FRAMEBASE_DATA          0x0400
#define PRINT_FLAG_VDEC_STATUS        0x0800
#define PRINT_FLAG_PARA_DATA          0x1000
#define PRINT_FLAG_USERDATA_DETAIL    0x2000
#define PRINT_FLAG_TIMEOUT_STATUS     0x4000
#define PRINT_FLAG_V4L_DETAIL         0x8000
#define IGNORE_PARAM_FROM_CONFIG      0x8000000

int debug_print(int index, int debug_flag, const char *fmt, ...)
{
	if (((debug_enable & debug_flag) &&
		((1 << index) & mpeg12_debug_mask))
		|| (debug_flag == PRINT_FLAG_ERROR)) {
		unsigned char *buf = kzalloc(512, GFP_ATOMIC);
		int len = 0;
		va_list args;

		if (!buf)
			return 0;

		va_start(args, fmt);
		len = sprintf(buf, "%d: ", index);
		vsnprintf(buf + len, 512-len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
		kfree(buf);
	}
	return 0;
}

#define PROVIDER_NAME   "vdec.mpeg12"
static const struct vframe_operations_s vf_provider_ops = {
	.peek = vmpeg_vf_peek,
	.get = vmpeg_vf_get,
	.put = vmpeg_vf_put,
	.event_cb = vmpeg_event_cb,
	.vf_states = vmpeg_vf_states,
};


static const u32 frame_rate_tab[16] = {
	96000 / 30, 96000000 / 23976, 96000 / 24, 96000 / 25,
	9600000 / 2997, 96000 / 30, 96000 / 50, 9600000 / 5994,
	96000 / 60,
	/* > 8 reserved, use 24 */
	96000 / 24, 96000 / 24, 96000 / 24, 96000 / 24,
	96000 / 24, 96000 / 24, 96000 / 24
};

static void mpeg12_put_video_frame(void *vdec_ctx, struct vframe_s *vf)
{
	vmpeg_vf_put(vf, vdec_ctx);
}

static void mpeg12_get_video_frame(void *vdec_ctx, struct vframe_s **vf)
{
	*vf = vmpeg_vf_get(vdec_ctx);
}

static struct task_ops_s task_dec_ops = {
	.type		= TASK_TYPE_DEC,
	.get_vframe	= mpeg12_get_video_frame,
	.put_vframe	= mpeg12_put_video_frame,
};

static int vmpeg12_v4l_alloc_buff_config_canvas(struct vdec_mpeg12_hw_s *hw, int i)
{
	int ret;
	u32 canvas;
	ulong decbuf_start = 0, decbuf_uv_start = 0;
	int decbuf_y_size = 0, decbuf_uv_size = 0;
	u32 canvas_width = 0, canvas_height = 0;
	struct vdec_s *vdec = hw_to_vdec(hw);
	struct vdec_v4l2_buffer *fb = NULL;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);

	if (hw->pics[i].v4l_ref_buf_addr) {
		struct vdec_v4l2_buffer *fb =
			(struct vdec_v4l2_buffer *)
			hw->pics[i].v4l_ref_buf_addr;

		fb->status = FB_ST_DECODER;
		return 0;
	}

	ret = ctx->fb_ops.alloc(&ctx->fb_ops, hw->fb_token, &fb, AML_FB_REQ_DEC);
	if (ret < 0) {
		debug_print(DECODE_ID(hw), 0,
			"[%d] get fb fail %d/%d.\n",
			ctx->id, i, hw->buf_num);
		return ret;
	}

	fb->task->attach(fb->task, &task_dec_ops, hw_to_vdec(hw));
	fb->status = FB_ST_DECODER;

	if (!hw->frame_width || !hw->frame_height) {
		struct vdec_pic_info pic;
		vdec_v4l_get_pic_info(ctx, &pic);
		hw->frame_width = pic.visible_width;
		hw->frame_height = pic.visible_height;
		debug_print(DECODE_ID(hw), 0,
			"[%d] set %d x %d from IF layer\n", ctx->id,
			hw->frame_width, hw->frame_height);
	}

	hw->pics[i].v4l_ref_buf_addr = (ulong)fb;
	if (fb->num_planes == 1) {
		decbuf_start	= fb->m.mem[0].addr;
		decbuf_y_size	= fb->m.mem[0].offset;
		decbuf_uv_start	= decbuf_start + decbuf_y_size;
		decbuf_uv_size	= decbuf_y_size / 2;
		canvas_width	= ALIGN(hw->frame_width, 64);
		canvas_height	= ALIGN(hw->frame_height, 64);
		fb->m.mem[0].bytes_used = fb->m.mem[0].size;
	} else if (fb->num_planes == 2) {
		decbuf_start	= fb->m.mem[0].addr;
		decbuf_y_size	= fb->m.mem[0].size;
		decbuf_uv_start	= fb->m.mem[1].addr;
		decbuf_uv_size	= fb->m.mem[1].size;
		canvas_width	= ALIGN(hw->frame_width, 64);
		canvas_height	= ALIGN(hw->frame_height, 64);
		fb->m.mem[0].bytes_used = decbuf_y_size;
		fb->m.mem[1].bytes_used = decbuf_uv_size;
	}

	debug_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
		"[%d] %s(), v4l ref buf addr: 0x%x\n",
		ctx->id, __func__, fb);

	if (vdec->parallel_dec == 1) {
		u32 tmp;
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

	hw->canvas_config[i][0].phy_addr	= decbuf_start;
	hw->canvas_config[i][0].width		= canvas_width;
	hw->canvas_config[i][0].height		= canvas_height;
	hw->canvas_config[i][0].block_mode	= hw->canvas_mode;
	hw->canvas_config[i][0].endian		=
		(hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;
	config_cav_lut(canvas_y(canvas), &hw->canvas_config[i][0], VDEC_1);

	hw->canvas_config[i][1].phy_addr	= decbuf_uv_start;
	hw->canvas_config[i][1].width		= canvas_width;
	hw->canvas_config[i][1].height		= canvas_height / 2;
	hw->canvas_config[i][1].block_mode	= hw->canvas_mode;
	hw->canvas_config[i][1].endian		=
		(hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;
	config_cav_lut(canvas_u(canvas), &hw->canvas_config[i][1], VDEC_1);

	debug_print(DECODE_ID(hw), PRINT_FLAG_BUFFER_DETAIL,
		"[%d] %s(), canvas: 0x%x mode: %d y: %x uv: %x w: %d h: %d\n",
		ctx->id, __func__, canvas, hw->canvas_mode,
		decbuf_start, decbuf_uv_start,
		canvas_width, canvas_height);

	return 0;
}

static int find_free_buffer(struct vdec_mpeg12_hw_s *hw)
{
	int i;

	for (i = 0; i < hw->buf_num; i++) {
		if ((hw->vfbuf_use[i] == 0) &&
			(hw->ref_use[i] == 0))
			break;
	}

	if ((i == hw->buf_num) &&
		(hw->buf_num != 0)) {
		return -1;
	}

	if (vmpeg12_v4l_alloc_buff_config_canvas(hw, i))
		return -1;

	return i;
}

static u32 spec_to_index(struct vdec_mpeg12_hw_s *hw, u32 spec)
{
	u32 i;

	for (i = 0; i < hw->buf_num; i++) {
		if (hw->canvas_spec[i] == spec)
			return i;
	}

	return hw->buf_num;
}

/* +[SE][BUG-145343][huanghang] fixed:mpeg2 frame qos info notify */
static void fill_frame_info(struct vdec_mpeg12_hw_s *hw, u32 slice_type,
							int frame_size, u32 pts)
{
	unsigned char a[3];
	unsigned char i, j, t;
	unsigned long  data;
	struct vframe_qos_s *vframe_qos = &hw->vframe_qos;

	vframe_qos->type = ((slice_type & PICINFO_TYPE_MASK) ==
						PICINFO_TYPE_I) ? 1 :
						((slice_type &
						PICINFO_TYPE_MASK) ==
						PICINFO_TYPE_P) ? 2 : 3;
	vframe_qos->size = frame_size;
	vframe_qos->pts = pts;

	get_random_bytes(&data, sizeof(unsigned long));
	if (vframe_qos->type == 1)
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
	vframe_qos->max_mv = a[2];
	vframe_qos->avg_mv = a[1];
	vframe_qos->min_mv = a[0];

	get_random_bytes(&data, sizeof(unsigned long));
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
	vframe_qos->max_qp = a[2];
	vframe_qos->avg_qp = a[1];
	vframe_qos->min_qp = a[0];

	get_random_bytes(&data, sizeof(unsigned long));
	a[0] = data & 0x1f;
	a[1] = (data >> 8) & 0x3f;
	a[2] = (data >> 16) & 0x7f;

	for (i = 0; i < 3; i++) {
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
	}
	vframe_qos->max_skip = a[2];
	vframe_qos->avg_skip = a[1];
	vframe_qos->min_skip = a[0];

	vframe_qos->num++;

	return;
}

static void set_frame_info(struct vdec_mpeg12_hw_s *hw, struct vframe_s *vf)
{
	u32 ar_bits;
	u32 endian_tmp;
	u32 buffer_index = vf->index;
	struct aml_vdec_hdr_infos hdr;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);

	vf->width = hw->pics[buffer_index].width;
	vf->height = hw->pics[buffer_index].height;

	if (hw->frame_dur > 0)
		vf->duration = hw->frame_dur;
	else {
		vf->duration = hw->frame_dur =
		frame_rate_tab[(READ_VREG(MREG_SEQ_INFO) >> 4) & 0xf];
		vdec_schedule_work(&hw->notify_work);
	}

	vf->signal_type = hw->reg_signal_type;

	memset(&hdr, 0, sizeof(hdr));
	hdr.signal_type = hw->reg_signal_type;
	vdec_v4l_set_hdr_infos(ctx, &hdr);

	ar_bits = READ_VREG(MREG_SEQ_INFO) & 0xf;

	if (ar_bits == 0x2)
		vf->ratio_control = 0xc0 << DISP_RATIO_ASPECT_RATIO_BIT;

	else if (ar_bits == 0x3)
		vf->ratio_control = 0x90 << DISP_RATIO_ASPECT_RATIO_BIT;

	else if (ar_bits == 0x4)
		vf->ratio_control = 0x74 << DISP_RATIO_ASPECT_RATIO_BIT;
	else
		vf->ratio_control = 0;

	hw->ratio_control = vf->ratio_control;

	vf->canvas0Addr = vf->canvas1Addr = -1;
	vf->plane_num = 2;

	vf->canvas0_config[0] = hw->canvas_config[buffer_index][0];
	vf->canvas0_config[1] = hw->canvas_config[buffer_index][1];
	vf->canvas1_config[0] = hw->canvas_config[buffer_index][0];
	vf->canvas1_config[1] = hw->canvas_config[buffer_index][1];

	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) {
		endian_tmp = (hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 7 : 0;
	} else {
		endian_tmp = (hw->canvas_mode == CANVAS_BLKMODE_LINEAR) ? 0 : 7;
	}

	vf->canvas0_config[0].endian = endian_tmp;
	vf->canvas0_config[1].endian = endian_tmp;
	vf->canvas1_config[0].endian = endian_tmp;
	vf->canvas1_config[1].endian = endian_tmp;

	vf->sidebind_type = hw->sidebind_type;
	vf->sidebind_channel_id = hw->sidebind_channel_id;

	debug_print(DECODE_ID(hw), PRINT_FLAG_PARA_DATA,
	"mpeg2dec: w(%d), h(%d), dur(%d), dur-ES(%d)\n",
		hw->frame_width, hw->frame_height, hw->frame_dur,
		frame_rate_tab[(READ_VREG(MREG_SEQ_INFO) >> 4) & 0xf]);
}

static bool error_skip(struct vdec_mpeg12_hw_s *hw,
	u32 info, struct vframe_s *vf)
{
	if (hw->error_frame_skip_level) {
		/* skip error frame */
		if ((info & PICINFO_ERROR) || (hw->frame_force_skip_flag)) {
			if ((info & PICINFO_ERROR) == 0) {
				if ((info & PICINFO_TYPE_MASK) ==
					PICINFO_TYPE_I)
					hw->frame_force_skip_flag = 0;
			} else {
				if (hw->error_frame_skip_level >= 2)
					hw->frame_force_skip_flag = 1;
			}
			if ((info & PICINFO_ERROR)
			|| (hw->frame_force_skip_flag))
				return true;
		}
	}
	return false;
}

static inline void vmpeg12_save_hw_context(struct vdec_mpeg12_hw_s *hw, u32 reg)
{
	if (reg == 3) {
		hw->ctx_valid = 0;
	} else {
		hw->seqinfo = READ_VREG(MREG_SEQ_INFO);
		hw->reg_pic_width = READ_VREG(MREG_PIC_WIDTH);
		hw->reg_pic_height = READ_VREG(MREG_PIC_HEIGHT);
		hw->reg_mpeg1_2_reg = READ_VREG(MPEG1_2_REG);
		hw->reg_pic_head_info = READ_VREG(PIC_HEAD_INFO);
		hw->reg_f_code_reg = READ_VREG(F_CODE_REG);
		hw->reg_slice_ver_pos_pic_type = READ_VREG(SLICE_VER_POS_PIC_TYPE);
		hw->reg_vcop_ctrl_reg = READ_VREG(VCOP_CTRL_REG);
		hw->reg_mb_info = READ_VREG(MB_INFO);
		hw->reg_signal_type = READ_VREG(AV_SCRATCH_H);
		debug_print(DECODE_ID(hw), PRINT_FLAG_PARA_DATA,
			"signal_type = %x", hw->reg_signal_type);
		hw->ctx_valid = 1;
	}
}

static void vmmpeg2_reset_udr_mgr(struct vdec_mpeg12_hw_s *hw)
{
	hw->wait_for_udr_send = 0;
	hw->cur_ud_idx = 0;
	memset(&hw->ud_record, 0, sizeof(hw->ud_record));
}

static void vmmpeg2_crate_userdata_manager(struct vdec_mpeg12_hw_s *hw,
	u8 *userdata_buf, int buf_len)
{
	if (hw) {
		mutex_init(&hw->userdata_mutex);

		memset(&hw->userdata_info, 0,
			sizeof(struct mmpeg2_userdata_info_t));
		hw->userdata_info.data_buf = userdata_buf;
		hw->userdata_info.buf_len = buf_len;
		hw->userdata_info.data_buf_end = userdata_buf + buf_len;
		hw->userdata_wp_ctx = 0;

		vmmpeg2_reset_udr_mgr(hw);
	}
}

static void vmmpeg2_destroy_userdata_manager(struct vdec_mpeg12_hw_s *hw)
{
	if (hw)
		memset(&hw->userdata_info, 0, sizeof(struct mmpeg2_userdata_info_t));
}

static void aml_swap_data(uint8_t *user_data, int ud_size)
{
	int swap_blocks, i, j, k, m;
	unsigned char c_temp;

	/* swap byte order */
	swap_blocks = ud_size / 8;
	for (i = 0; i < swap_blocks; i++) {
		j = i * 8;
		k = j + 7;
		for (m = 0; m < 4; m++) {
			c_temp = user_data[j];
			user_data[j++] = user_data[k];
			user_data[k--] = c_temp;
		}
	}
}

#ifdef DUMP_USER_DATA
static void push_to_buf(struct vdec_mpeg12_hw_s *hw,
	u8 *pdata,
	int len,
	struct userdata_meta_info_t *pmeta,
	u32 reference)
{
	u32 *pLen;
	int info_cnt;
	u8 *pbuf_end;

	if (!hw->user_data_dump_buf)
		return;

	if (hw->bskip) {
		pr_info("over size, skip\n");
		return;
	}
	info_cnt = 0;
	pLen = (u32 *)hw->pdump_buf_cur_start;

	*pLen = len;
	hw->pdump_buf_cur_start += sizeof(u32);
	info_cnt++;
	pLen++;

	*pLen = pmeta->duration;
	hw->pdump_buf_cur_start += sizeof(u32);
	info_cnt++;
	pLen++;

	*pLen = pmeta->flags;
	hw->pdump_buf_cur_start += sizeof(u32);
	info_cnt++;
	pLen++;

	*pLen = pmeta->vpts;
	hw->pdump_buf_cur_start += sizeof(u32);
	info_cnt++;
	pLen++;

	*pLen = pmeta->vpts_valid;
	hw->pdump_buf_cur_start += sizeof(u32);
	info_cnt++;
	pLen++;


	*pLen = hw->n_userdata_id;
	hw->pdump_buf_cur_start += sizeof(u32);
	info_cnt++;
	pLen++;

	*pLen = reference;
	hw->pdump_buf_cur_start += sizeof(u32);
	info_cnt++;
	pLen++;

	pbuf_end = hw->userdata_info.data_buf_end;
	if (pdata + len > pbuf_end) {
		int first_section_len;

		first_section_len = pbuf_end - pdata;
		memcpy(hw->pdump_buf_cur_start, pdata, first_section_len);
		pdata = (u8 *)hw->userdata_info.data_buf;
		hw->pdump_buf_cur_start += first_section_len;
		memcpy(hw->pdump_buf_cur_start, pdata, len - first_section_len);
		hw->pdump_buf_cur_start += len - first_section_len;
	} else {
		memcpy(hw->pdump_buf_cur_start, pdata, len);
		hw->pdump_buf_cur_start += len;
	}

	hw->total_len += len + info_cnt * sizeof(u32);
	if (hw->total_len >= MAX_USER_DATA_SIZE-4096)
		hw->bskip = 1;
}

static void dump_userdata_info(struct vdec_mpeg12_hw_s *hw,
	void *puser_data,
	int len,
	struct userdata_meta_info_t *pmeta,
	u32 reference)
{
	u8 *pstart;

	pstart = (u8 *)puser_data;

#ifdef	DUMP_HEAD_INFO_DATA
	push_to_buf(hw, pstart, len, pmeta, reference);
#else
	push_to_buf(hw, pstart+8, len - 8, pmeta, reference);
#endif
}

static void print_data(unsigned char *pdata,
						int len,
						unsigned int flag,
						unsigned int duration,
						unsigned int vpts,
						unsigned int vpts_valid,
						int rec_id,
						u32 reference)
{
	int nLeft;

	nLeft = len;

	pr_info("%d len:%d, flag:0x%x, dur:%d, vpts:0x%x, valid:%d, refer:%d\n",
		rec_id,	len, flag, duration, vpts, vpts_valid, reference);

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

static void dump_data(u8 *pdata,
						unsigned int user_data_length,
						unsigned int flag,
						unsigned int duration,
						unsigned int vpts,
						unsigned int vpts_valid,
						int rec_id,
						u32 reference)
{
	unsigned char szBuf[256];

	memset(szBuf, 0, 256);
	memcpy(szBuf, pdata, user_data_length);

	aml_swap_data(szBuf, user_data_length);

	print_data(szBuf,
				user_data_length,
				flag,
				duration,
				vpts,
				vpts_valid,
				rec_id,
				reference);
}


static void show_user_data_buf(struct vdec_mpeg12_hw_s *hw)
{
	u8 *pbuf;
	int len;
	unsigned int flag;
	unsigned int duration;
	unsigned int vpts;
	unsigned int vpts_valid;
	int rec_id;
	u32 reference;

	pr_info("show user data buf\n");
	pbuf = hw->user_data_dump_buf;

	while (pbuf < hw->pdump_buf_cur_start) {
		u32 *pLen;

		pLen = (u32 *)pbuf;

		len = *pLen;
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

		reference = *pLen;
		pLen++;
		pbuf += sizeof(u32);


		dump_data(pbuf, len, flag, duration,
			vpts, vpts_valid, rec_id, reference);
		pbuf += len;
		msleep(30);
	}
}

static int amvdec_mmpeg12_init_userdata_dump(struct vdec_mpeg12_hw_s *hw)
{
	hw->user_data_dump_buf = kmalloc(MAX_USER_DATA_SIZE, GFP_KERNEL);
	if (hw->user_data_dump_buf)
		return 1;
	else
		return 0;
}

static void amvdec_mmpeg12_uninit_userdata_dump(struct vdec_mpeg12_hw_s *hw)
{
	if (hw->user_data_dump_buf) {
		show_user_data_buf(hw);
		kfree(hw->user_data_dump_buf);
		hw->user_data_dump_buf = NULL;
	}
}

static void reset_user_data_buf(struct vdec_mpeg12_hw_s *hw)
{
	hw->total_len = 0;
	hw->pdump_buf_cur_start = hw->user_data_dump_buf;
	hw->bskip = 0;
	hw->n_userdata_id = 0;
}
#endif

static void user_data_ready_notify(struct vdec_mpeg12_hw_s *hw,
	u32 pts, u32 pts_valid)
{
	struct mmpeg2_userdata_record_t *p_userdata_rec;
	int i;

	if (hw->wait_for_udr_send) {
		for (i = 0; i < hw->cur_ud_idx; i++) {
			mutex_lock(&hw->userdata_mutex);


			p_userdata_rec = hw->userdata_info.records
				+ hw->userdata_info.write_index;

			hw->ud_record[i].meta_info.vpts_valid = pts_valid;
			hw->ud_record[i].meta_info.vpts = pts;

			*p_userdata_rec = hw->ud_record[i];
#ifdef DUMP_USER_DATA
			dump_userdata_info(hw,
				hw->userdata_info.data_buf + p_userdata_rec->rec_start,
				p_userdata_rec->rec_len,
				&p_userdata_rec->meta_info,
				hw->reference[i]);
			hw->n_userdata_id++;
#endif
			hw->userdata_info.write_index++;
			if (hw->userdata_info.write_index >= USERDATA_FIFO_NUM)
				hw->userdata_info.write_index = 0;

			mutex_unlock(&hw->userdata_mutex);


			vdec_wakeup_userdata_poll(hw_to_vdec(hw));
		}
		hw->wait_for_udr_send = 0;
		hw->cur_ud_idx = 0;
	}
	hw->notify_ucode_cc_last_wp = hw->ucode_cc_last_wp;
	hw->notify_data_cc_last_wp = hw->userdata_info.last_wp;
}

static int vmmpeg2_user_data_read(struct vdec_s *vdec,
	struct userdata_param_t *puserdata_para)
{
	struct vdec_mpeg12_hw_s *hw = NULL;
	int rec_ri, rec_wi;
	int rec_len;
	u8 *rec_data_start;
	u8 *pdest_buf;
	struct mmpeg2_userdata_record_t *p_userdata_rec;
	u32 data_size;
	u32 res;
	int copy_ok = 1;

	hw = (struct vdec_mpeg12_hw_s *)vdec->private;

	pdest_buf = puserdata_para->pbuf_addr;

	mutex_lock(&hw->userdata_mutex);

	rec_ri = hw->userdata_info.read_index;
	rec_wi = hw->userdata_info.write_index;

	if (rec_ri == rec_wi) {
		mutex_unlock(&hw->userdata_mutex);
		return 0;
	}

	p_userdata_rec = hw->userdata_info.records + rec_ri;

	rec_len = p_userdata_rec->rec_len;
	rec_data_start = p_userdata_rec->rec_start + hw->userdata_info.data_buf;

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

static void vmmpeg2_reset_userdata_fifo(struct vdec_s *vdec, int bInit)
{
	struct vdec_mpeg12_hw_s *hw = NULL;

	hw = (struct vdec_mpeg12_hw_s *)vdec->private;

	if (hw) {
		mutex_lock(&hw->userdata_mutex);
		pr_info("mpeg2_reset_userdata_fifo: bInit: %d, ri: %d, wi: %d\n",
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

static void vmmpeg2_wakeup_userdata_poll(struct vdec_s *vdec)
{
	amstream_wakeup_userdata_poll(vdec);
}

static void userdata_push_do_work(struct work_struct *work)
{
	u32 reg;
	u8 *pdata;
	u8 *psrc_data;
	u8 head_info[8];
	struct userdata_meta_info_t meta_info;
	u32 wp;
	u32 index;
	u32 picture_struct;
	u32 reference;
	u32 picture_type;
	u32 temp;
	u32 data_length;
	u32 data_start;
	int i;
	u32 offset;
	u32 cur_wp;
#ifdef PRINT_HEAD_INFO
	u8 *ptype_str;
#endif
	struct mmpeg2_userdata_record_t *pcur_ud_rec;

	struct vdec_mpeg12_hw_s *hw = container_of(work,
					struct vdec_mpeg12_hw_s, userdata_push_work);

	memset(&meta_info, 0, sizeof(meta_info));

	meta_info.duration = hw->frame_dur;


	reg = READ_VREG(AV_SCRATCH_J);
	hw->userdata_wp_ctx = reg & (~(1<<16));
	meta_info.flags = ((reg >> 30) << 1);
	meta_info.flags |= (VFORMAT_MPEG12 << 3);
	/* check  top_field_first flag */
	if ((reg >> 28) & 0x1) {
		meta_info.flags |= (1 << 10);
		meta_info.flags |= (((reg >> 29) & 0x1) << 11);
	}

	cur_wp = reg & 0x7fff;
	if (cur_wp == hw->ucode_cc_last_wp || (cur_wp >= AUX_BUF_ALIGN(CCBUF_SIZE))) {
		debug_print(DECODE_ID(hw), 0,
			"Null or Over size user data package: wp = %d\n", cur_wp);
		WRITE_VREG(AV_SCRATCH_J, 0);
		return;
	}

	if (hw->cur_ud_idx >= MAX_UD_RECORDS) {
		debug_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"UD Records over: %d, skip it\n", MAX_UD_RECORDS);
		WRITE_VREG(AV_SCRATCH_J, 0);
		hw->cur_ud_idx = 0;
		return;
	}

	if (cur_wp < hw->ucode_cc_last_wp)
		hw->ucode_cc_last_wp = 0;

	offset = READ_VREG(AV_SCRATCH_I);

	codec_mm_dma_flush(
		hw->ccbuf_phyAddress_virt,
		CCBUF_SIZE,
		DMA_FROM_DEVICE);

	mutex_lock(&hw->userdata_mutex);
	if (hw->ccbuf_phyAddress_virt) {
		pdata = (u8 *)hw->ccbuf_phyAddress_virt + hw->ucode_cc_last_wp;
		memcpy(head_info, pdata, 8);
	} else
		memset(head_info, 0, 8);
	mutex_unlock(&hw->userdata_mutex);
	aml_swap_data(head_info, 8);

	wp = (head_info[0] << 8 | head_info[1]);
	index = (head_info[2] << 8 | head_info[3]);

	picture_struct = (head_info[6] << 8 | head_info[7]);
	temp = (head_info[4] << 8 | head_info[5]);
	reference = temp & 0x3FF;
	picture_type = (temp >> 10) & 0x7;

	if (debug_enable & PRINT_FLAG_USERDATA_DETAIL)
		pr_info("index:%d, wp:%d, ref:%d, type:%d, struct:0x%x, u_last_wp:0x%x\n",
			index, wp, reference,
			picture_type, picture_struct,
			hw->ucode_cc_last_wp);

	switch (picture_type) {
	case 1:
			meta_info.flags |= (1<<7);
#ifdef PRINT_HEAD_INFO
			ptype_str = " I";
#endif
			break;
	case 2:
			meta_info.flags |= (2<<7);
#ifdef PRINT_HEAD_INFO
			ptype_str = " P";
#endif
			break;
	case 3:
			meta_info.flags |= (3<<7);
#ifdef PRINT_HEAD_INFO
			ptype_str = " B";
#endif
			break;
	case 4:
			meta_info.flags |= (4<<7);
#ifdef PRINT_HEAD_INFO
			ptype_str = " D";
#endif
			break;
	default:
#ifdef PRINT_HEAD_INFO
			ptype_str = " U";
#endif
			break;
	}
#ifdef PRINT_HEAD_INFO
	pr_info("ref:%d, type:%s, ext:%d, first:%d, data_length:%d\n",
		reference, ptype_str,
		(reg >> 30),
		(reg >> 28)&0x3,
		reg & 0xffff);
#endif
	data_length = cur_wp - hw->ucode_cc_last_wp;
	data_start = reg & 0xffff;
	psrc_data = (u8 *)hw->ccbuf_phyAddress_virt + hw->ucode_cc_last_wp;

	pdata = hw->userdata_info.data_buf + hw->userdata_info.last_wp;
	for (i = 0; i < data_length && hw->ccbuf_phyAddress_virt != NULL && psrc_data; i++) {
		*pdata++ = *psrc_data++;
		if (pdata >= hw->userdata_info.data_buf_end)
			pdata = hw->userdata_info.data_buf;
	}

	pcur_ud_rec = hw->ud_record + hw->cur_ud_idx;

	pcur_ud_rec->meta_info = meta_info;
	pcur_ud_rec->rec_start = hw->userdata_info.last_wp;
	pcur_ud_rec->rec_len = data_length;

	hw->userdata_info.last_wp += data_length;
	if (hw->userdata_info.last_wp >= USER_DATA_SIZE)
		hw->userdata_info.last_wp %= USER_DATA_SIZE;

	hw->wait_for_udr_send = 1;

	hw->ucode_cc_last_wp = cur_wp;

	if (debug_enable & PRINT_FLAG_USERDATA_DETAIL)
		pr_info("cur_wp:%d, rec_start:%d, rec_len:%d\n",
			cur_wp,
			pcur_ud_rec->rec_start,
			pcur_ud_rec->rec_len);

#ifdef DUMP_USER_DATA
	hw->reference[hw->cur_ud_idx] = reference;
#endif

	hw->cur_ud_idx++;
	WRITE_VREG(AV_SCRATCH_J, 0);
}


void userdata_pushed_drop(struct vdec_mpeg12_hw_s *hw)
{
	hw->userdata_info.last_wp = hw->notify_data_cc_last_wp;
	hw->ucode_cc_last_wp = hw->notify_ucode_cc_last_wp;
	hw->cur_ud_idx = 0;
	hw->wait_for_udr_send = 0;
}

static inline void hw_update_gvs(struct vdec_mpeg12_hw_s *hw)
{
	if (hw->gvs.frame_height != hw->frame_height) {
		hw->gvs.frame_width = hw->frame_width;
		hw->gvs.frame_height = hw->frame_height;
	}
	if (hw->gvs.frame_dur != hw->frame_dur) {
		hw->gvs.frame_dur = hw->frame_dur;
		if (hw->frame_dur != 0)
			hw->gvs.frame_rate = ((96000 * 10 / hw->frame_dur) % 10) < 5 ?
					96000 / hw->frame_dur : (96000 / hw->frame_dur +1);
		else
			hw->gvs.frame_rate = -1;
	}
	if (hw->gvs.ratio_control != hw->ratio_control)
		hw->gvs.ratio_control = hw->ratio_control;

	hw->gvs.status = hw->stat;
	hw->gvs.error_count = hw->gvs.error_frame_count;
	hw->gvs.drop_frame_count = hw->drop_frame_count;

}

static int prepare_display_buf(struct vdec_mpeg12_hw_s *hw,
	struct pic_info_t *pic)
{
	u32 field_num = 0, i;
	u32 first_field_type = 0, type = 0;
	struct vframe_s *vf = NULL;
	u32 index = pic->index;
	u32 info = pic->buffer_info;
	struct vdec_s *vdec = hw_to_vdec(hw);
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;
	struct vdec_v4l2_buffer *fb = NULL;
	ulong nv_order = VIDTYPE_VIU_NV21;
	bool pb_skip = false;

	/* swap uv */
	if ((v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12) ||
		(v4l2_ctx->cap_pix_fmt == V4L2_PIX_FMT_NV12M))
		nv_order = VIDTYPE_VIU_NV12;

#ifdef NV21
	type = nv_order;
#endif
	if (hw->i_only) {
		pb_skip = 1;
	}

	user_data_ready_notify(hw, pic->pts, pic->pts_valid);

	if (hw->frame_prog & PICINFO_PROG) {
		field_num = 1;
		type |= VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | nv_order;
	} else {
#ifdef INTERLACE_SEQ_ALWAYS
		/* once an interlace seq, force interlace, to make di easy. */
		hw->dec_control |= DEC_CONTROL_FLAG_FORCE_SEQ_INTERLACE;
#endif
		hw->frame_rpt_state = FRAME_REPEAT_NONE;

		first_field_type = (info & PICINFO_TOP_FIRST) ?
			VIDTYPE_INTERLACE_TOP : VIDTYPE_INTERLACE_BOTTOM;
		field_num = (info & PICINFO_RPT_FIRST) ? 3 : 2;
	}

	if ((vdec->prog_only) || (hw->report_field & V4L2_FIELD_NONE) ||
		(!v4l2_ctx->vpp_is_need)) {
		field_num = 1;
		type |= VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | nv_order;
	}

	for (i = 0; i < field_num; i++) {
		if (kfifo_get(&hw->newframe_q, &vf) == 0) {
			debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
				"fatal error, no available buffer slot.");
			hw->dec_result = DEC_RESULT_ERROR;
			vdec_schedule_work(&hw->work);
			return -1;
		}

		vf->v4l_mem_handle
			= hw->pics[index].v4l_ref_buf_addr;
		fb = (struct vdec_v4l2_buffer *)vf->v4l_mem_handle;
		debug_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
			"[%d] %s(), v4l mem handle: 0x%lx\n",
			((struct aml_vcodec_ctx *)(hw->v4l2_ctx))->id,
			__func__, vf->v4l_mem_handle);

		hw->vfbuf_use[index]++;
		vf->index = index;
		set_frame_info(hw, vf);
		if (field_num > 1) {
			vf->duration = vf->duration / field_num;
			vf->duration_pulldown = (field_num == 3) ?
				(vf->duration >> 1):0;
			type = nv_order;
			if (i == 1) /* second field*/
				type |= (first_field_type == VIDTYPE_INTERLACE_TOP) ?
					VIDTYPE_INTERLACE_BOTTOM : VIDTYPE_INTERLACE_TOP;
			else
				type |= (first_field_type == VIDTYPE_INTERLACE_TOP) ?
					VIDTYPE_INTERLACE_TOP : VIDTYPE_INTERLACE_BOTTOM;
		} else {
			if ((hw->seqinfo & SEQINFO_EXT_AVAILABLE) &&
				(hw->seqinfo & SEQINFO_PROG)) {
				if (info & PICINFO_RPT_FIRST) {
					if (info & PICINFO_TOP_FIRST)
						vf->duration *= 3;
					else
						vf->duration *= 2;
				}
				vf->duration_pulldown = 0;
			} else {
				vf->duration_pulldown =
					(info & PICINFO_RPT_FIRST) ?
						vf->duration >> 1 : 0;
			}
		}
		vf->duration += vf->duration_pulldown;
		vf->type = type;
		vf->orientation = 0;
		if (i > 0) {
			vf->pts = 0;
			vf->pts_us64 = 0;
			vf->timestamp = pic->timestamp;
			if (v4l2_ctx->second_field_pts_mode) {
				vf->timestamp = 0;
			}
		} else {
			vf->pts = (pic->pts_valid) ? pic->pts : 0;
			vf->pts_us64 = (pic->pts_valid) ? pic->pts64 : 0;
			if (field_num == 1)
				vf->timestamp = pic->timestamp;
			else
				vf->timestamp = pic->last_timestamp;
		}
		vf->type_original = vf->type;

		if ((error_skip(hw, pic->buffer_info, vf)) ||
			(((hw->first_i_frame_ready == 0) || pb_skip) &&
			((PICINFO_TYPE_MASK & pic->buffer_info) !=
			 PICINFO_TYPE_I))) {
			unsigned long flags;
			hw->drop_frame_count++;
			if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) {
				hw->gvs.i_lost_frames++;
			} else if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_P) {
				hw->gvs.p_lost_frames++;
			} else if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_B) {
				hw->gvs.b_lost_frames++;
			}
			/* Though we drop it, it is still an error frame, count it.
			 * Becase we've counted the error frame in vdec_count_info
			 * function, avoid count it twice.
			 */
		if (!(info & PICINFO_ERROR)) {
			hw->gvs.error_frame_count++;
			if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) {
				hw->gvs.i_concealed_frames++;
			} else if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_P) {
				hw->gvs.p_concealed_frames++;
			} else if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_B) {
				hw->gvs.b_concealed_frames++;
			}
		}
			hw->vfbuf_use[index]--;
			spin_lock_irqsave(&hw->lock, flags);
			kfifo_put(&hw->newframe_q,
				(const struct vframe_s *)vf);
			spin_unlock_irqrestore(&hw->lock, flags);
		} else {
			debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
				"%s, vf: %lx, num[%d]: %d(%c), dur: %d, type: %x, pts: %d(%lld), ts(%lld)\n",
				__func__, (ulong)vf, i, hw->disp_num, GET_SLICE_TYPE(info),
				vf->duration, vf->type, vf->pts, vf->pts_us64, vf->timestamp);
			atomic_add(1, &hw->disp_num);
			if (i == 0) {
				decoder_do_frame_check(vdec, vf);
				hw_update_gvs(hw);
				vdec_fill_vdec_frame(vdec, &hw->vframe_qos,
					&hw->gvs, vf, pic->hw_decode_time);
			}
			vdec->vdec_fps_detec(vdec->id);
			vf->mem_handle =
				decoder_bmmu_box_get_mem_handle(
				hw->mm_blk_handle, index);
			if (!vdec->vbuf.use_ptsserv && vdec_stream_based(vdec)) {
				/* offset for tsplayer pts lookup */
				if (i == 0) {
					vf->pts_us64 =
						(((u64)vf->duration << 32) &
						0xffffffff00000000) | pic->offset;
					vf->pts = 0;
				} else {
					vf->pts_us64 = (u64)-1;
					vf->pts = 0;
				}
			}
			vdec_vframe_ready(vdec, vf);
			kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
			ATRACE_COUNTER(hw->pts_name, vf->timestamp);
			ATRACE_COUNTER(hw->new_q_name, kfifo_len(&hw->newframe_q));
			ATRACE_COUNTER(hw->disp_q_name, kfifo_len(&hw->display_q));
			/* if (hw->disp_num == 1) { */
			if (hw->kpi_first_i_decoded == 0) {
				hw->kpi_first_i_decoded = 1;
				debug_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
					"[vdec_kpi][%s] First I frame decoded.\n",
					__func__);
			}
			if (without_display_mode == 0) {
				if (v4l2_ctx->is_stream_off) {
					vmpeg_vf_put(vmpeg_vf_get(vdec), vdec);
				} else {
					set_meta_data_to_vf(vf, UVM_META_DATA_VF_BASE_INFOS, hw->v4l2_ctx);
					ATRACE_COUNTER("VC_OUT_DEC-submit", fb->buf_idx);
					fb->task->submit(fb->task, TASK_TYPE_DEC);
				}
			} else
				vmpeg_vf_put(vmpeg_vf_get(vdec), vdec);
		}
	}
	return 0;
}

static void force_interlace_check(struct vdec_mpeg12_hw_s *hw)
{
	if ((hw->dec_control &
	DEC_CONTROL_FLAG_FORCE_2500_720_576_INTERLACE) &&
		(hw->frame_width == 720) &&
		(hw->frame_height == 576) &&
		(hw->frame_dur == 3840)) {
		hw->frame_prog = 0;
	} else if ((hw->dec_control
	& DEC_CONTROL_FLAG_FORCE_3000_704_480_INTERLACE) &&
		(hw->frame_width == 704) &&
		(hw->frame_height == 480) &&
		(hw->frame_dur == 3200)) {
		hw->frame_prog = 0;
	} else if ((hw->dec_control
	& DEC_CONTROL_FLAG_FORCE_2500_704_576_INTERLACE) &&
		(hw->frame_width == 704) &&
		(hw->frame_height == 576) &&
		(hw->frame_dur == 3840)) {
		hw->frame_prog = 0;
	} else if ((hw->dec_control
	& DEC_CONTROL_FLAG_FORCE_2500_544_576_INTERLACE) &&
		(hw->frame_width == 544) &&
		(hw->frame_height == 576) &&
		(hw->frame_dur == 3840)) {
		hw->frame_prog = 0;
	} else if ((hw->dec_control
	& DEC_CONTROL_FLAG_FORCE_2500_480_576_INTERLACE) &&
		(hw->frame_width == 480) &&
		(hw->frame_height == 576) &&
		(hw->frame_dur == 3840)) {
		hw->frame_prog = 0;
	} else if (hw->dec_control
	& DEC_CONTROL_FLAG_FORCE_SEQ_INTERLACE) {
		hw->frame_prog = 0;
	}

}

static int update_reference(struct vdec_mpeg12_hw_s *hw,
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
		index = hw->buf_num;
	} else {
		hw->ref_use[hw->refs[0]]--;		//old ref0 ununsed
		hw->refs[0] = hw->refs[1];
		hw->refs[1] = index;
		index = hw->refs[0];
	}
	return index;
}

static bool is_ref_error(struct vdec_mpeg12_hw_s *hw)
{
	if ((hw->pics[hw->refs[0]].buffer_info & PICINFO_ERROR) ||
		(hw->pics[hw->refs[1]].buffer_info & PICINFO_ERROR))
		return 1;
	return 0;
}

static int vmpeg2_get_ps_info(struct vdec_mpeg12_hw_s *hw, int width, int height,
	bool frame_prog, struct aml_vdec_ps_infos *ps)
{
	ps->visible_width	= width;
	ps->visible_height	= height;
	ps->coded_width		= ALIGN(width, 64);
	ps->coded_height 	= ALIGN(height, 64);
	ps->dpb_size 		= hw->buf_num;
	ps->dpb_frames		= DECODE_BUFFER_NUM_DEF;
	ps->dpb_margin		= hw->dynamic_buf_num_margin;
	ps->field 		= frame_prog ? V4L2_FIELD_NONE : V4L2_FIELD_INTERLACED;
	ps->field 		= hw->force_prog_only ? V4L2_FIELD_NONE : ps->field;

	return 0;
}

static int v4l_res_change(struct vdec_mpeg12_hw_s *hw, int width, int height, bool frame_prog)
{
	struct aml_vcodec_ctx *ctx =
			(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int ret = 0;

	if (ctx->param_sets_from_ucode &&
		hw->res_ch_flag == 0) {
		struct aml_vdec_ps_infos ps;

		if ((hw->frame_width != 0 &&
			hw->frame_height != 0) &&
			(hw->frame_width != width ||
			hw->frame_height != height)) {
			debug_print(DECODE_ID(hw), 0,
				"v4l_res_change Pic Width/Height Change (%d,%d)=>(%d,%d)\n",
				hw->frame_width, hw->frame_height,
				width,
				height);
			vmpeg2_get_ps_info(hw, width, height, frame_prog, &ps);
			vdec_v4l_set_ps_infos(ctx, &ps);
			vdec_v4l_res_ch_event(ctx);
			hw->v4l_params_parsed = false;
			hw->res_ch_flag = 1;
			ctx->v4l_resolution_change = 1;
			hw->eos = 1;
			flush_output(hw);
			ATRACE_COUNTER("V_ST_DEC-submit_eos", __LINE__);
			notify_v4l_eos(hw_to_vdec(hw));
			ATRACE_COUNTER("V_ST_DEC-submit_eos", 0);

			ret = 1;
		}
	}

	return ret;
}

void cal_chunk_offset_and_size(struct vdec_mpeg12_hw_s *hw)
{
	u32 consume_byte, res_byte;

	res_byte = READ_VREG(VIFF_BIT_CNT) >> 3;

	if (hw->chunk->size > res_byte) {
		consume_byte = hw->chunk->size - res_byte;

		if (consume_byte > VDEC_FIFO_ALIGN) {
			consume_byte -= VDEC_FIFO_ALIGN;
			res_byte += VDEC_FIFO_ALIGN;
		}
		hw->chunk_header_offset = hw->chunk->offset + consume_byte;
		hw->chunk_res_size = res_byte;
	}
}

static irqreturn_t vmpeg12_isr_thread_fn(struct vdec_s *vdec, int irq)
{
	u32 reg, index, info, seqinfo, offset, pts, frame_size=0, tmp_h, tmp_w;
	u64 pts_us64 = 0;
	struct pic_info_t *new_pic, *disp_pic;
	struct vdec_mpeg12_hw_s *hw =
		(struct vdec_mpeg12_hw_s *)(vdec->private);

	if (READ_VREG(AV_SCRATCH_M) != 0 &&
		(debug_enable & PRINT_FLAG_UCODE_DETAIL)) {

		debug_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"dbg %x: %x, level %x, wp %x, rp %x, cnt %x\n",
			READ_VREG(AV_SCRATCH_M), READ_VREG(AV_SCRATCH_N),
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			READ_VREG(VIFF_BIT_CNT));
		WRITE_VREG(AV_SCRATCH_M, 0);
		return IRQ_HANDLED;
	}

	reg = READ_VREG(AV_SCRATCH_G);
	if (reg == 1) {
		int frame_width = READ_VREG(MREG_PIC_WIDTH);
		int frame_height = READ_VREG(MREG_PIC_HEIGHT);
		int info = READ_VREG(MREG_SEQ_INFO);
		bool frame_prog = info & 0x10000;

		if (hw->kpi_first_i_comming == 0) {
			hw->kpi_first_i_comming = 1;
			debug_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
				"[vdec_kpi][%s] First I frame coming.\n",
				__func__);
		}

		if (!v4l_res_change(hw, frame_width, frame_height, frame_prog)) {
			struct aml_vcodec_ctx *ctx =
				(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
			if (ctx->param_sets_from_ucode && !hw->v4l_params_parsed) {
				struct aml_vdec_ps_infos ps;

				vmpeg2_get_ps_info(hw, frame_width, frame_height, frame_prog, &ps);
				hw->v4l_params_parsed = true;
				hw->report_field = frame_prog ? V4L2_FIELD_NONE : V4L2_FIELD_INTERLACED;
				vdec_v4l_set_ps_infos(ctx, &ps);
				cal_chunk_offset_and_size(hw);
				userdata_pushed_drop(hw);
				reset_process_time(hw);
				hw->dec_result = DEC_RESULT_AGAIN;
				vdec_schedule_work(&hw->work);
			} else {
				struct vdec_pic_info pic;

				if (!hw->buf_num) {
					vdec_v4l_get_pic_info(ctx, &pic);
					hw->buf_num = pic.dpb_frames +
						pic.dpb_margin;
					if (hw->buf_num > DECODE_BUFFER_NUM_MAX)
						hw->buf_num = DECODE_BUFFER_NUM_MAX;
				}

				WRITE_VREG(AV_SCRATCH_G, 0);

				hw->res_ch_flag = 0;
			}
		} else {
			userdata_pushed_drop(hw);
			reset_process_time(hw);
			hw->dec_result = DEC_RESULT_AGAIN;
			vdec_schedule_work(&hw->work);
		}
		return IRQ_HANDLED;
	}

	reg = READ_VREG(AV_SCRATCH_J);
	if (reg & (1<<16)) {
		vdec_schedule_work(&hw->userdata_push_work);
		return IRQ_HANDLED;
	}

	reg = READ_VREG(MREG_BUFFEROUT);

	ATRACE_COUNTER("V_ST_DEC-decode_state", reg);

	if (reg == MPEG12_DATA_REQUEST) {
		debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
			"%s: data request, bcnt=%x\n",
			__func__, READ_VREG(VIFF_BIT_CNT));
		if (vdec_frame_based(vdec)) {
			reset_process_time(hw);
			hw->dec_result = DEC_RESULT_GET_DATA;
			vdec_schedule_work(&hw->work);
		}
	} else if (reg == MPEG12_DATA_EMPTY) {
		/*timeout when decoding next frame*/
		debug_print(DECODE_ID(hw), PRINT_FLAG_VLD_DETAIL,
			"%s: Insufficient data, lvl=%x ctrl=%x bcnt=%x\n",
			__func__,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_CONTROL),
			READ_VREG(VIFF_BIT_CNT));

		if (vdec_frame_based(vdec)) {
			userdata_pushed_drop(hw);
			hw->dec_result = DEC_RESULT_DONE;
			vdec_schedule_work(&hw->work);
		} else {
			hw->dec_result = DEC_RESULT_AGAIN;
			vdec_schedule_work(&hw->work);
			userdata_pushed_drop(hw);
			reset_process_time(hw);
		}
		return IRQ_HANDLED;
	} else {  /* MPEG12_PIC_DONE, MPEG12_SEQ_END */
		reset_process_time(hw);

		info = READ_VREG(MREG_PIC_INFO);
		offset = READ_VREG(MREG_FRAME_OFFSET);
		index = spec_to_index(hw, READ_VREG(REC_CANVAS_ADDR));
		seqinfo = READ_VREG(MREG_SEQ_INFO);

		if (((seqinfo >> 8) & 0xff) &&
			((seqinfo >> 12 & 0x7) != hw->profile_idc ||
			(seqinfo >> 8 & 0xf) != hw->level_idc)) {
			hw->profile_idc = seqinfo >> 12 & 0x7;
			hw->level_idc = seqinfo >> 8 & 0xf;
			vdec_set_profile_level(vdec, hw->profile_idc, hw->level_idc);
			debug_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
				"profile_idc: %d  level_idc: %d\n",
				hw->profile_idc, hw->level_idc);
		}

		if (index >= hw->buf_num) {
			debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
				"mmpeg12: invalid buf index: %d\n", index);
			hw->dec_result = DEC_RESULT_ERROR;
			vdec_schedule_work(&hw->work);
			return IRQ_HANDLED;
		}
		hw->dec_num++;
		hw->dec_result = DEC_RESULT_DONE;
		new_pic = &hw->pics[index];
		if (vdec->mvfrm) {
			new_pic->frame_size = vdec->mvfrm->frame_size;
			new_pic->hw_decode_time =
			local_clock() - vdec->mvfrm->hw_decode_start;
		}

		tmp_w = READ_VREG(MREG_PIC_WIDTH);
		tmp_h = READ_VREG(MREG_PIC_HEIGHT);

		new_pic->width = tmp_w;
		hw->frame_width = tmp_w;
		new_pic->height = tmp_h;
		hw->frame_height = tmp_h;

		new_pic->buffer_info = info;
		new_pic->offset = offset;
		new_pic->index = index;
		if (((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) ||
			((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_P)) {
			if (hw->chunk) {
				new_pic->pts_valid = hw->chunk->pts_valid;
				new_pic->pts = hw->chunk->pts;
				new_pic->pts64 = hw->chunk->pts64;
				if (hw->first_field_timestamp_valid)
					new_pic->last_timestamp = hw->first_field_timestamp;
				else
					new_pic->last_timestamp = hw->chunk->timestamp;
				hw->first_field_timestamp_valid = false;
				new_pic->timestamp = hw->chunk->timestamp;
				if (hw->last_chunk_pts == hw->chunk->pts) {
					new_pic->pts_valid = 0;
					debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
						"pts invalid\n");
				}
			} else {
				if ((vdec->vbuf.no_parser == 0) || (vdec->vbuf.use_ptsserv)) {
					if (pts_lookup_offset_us64(PTS_TYPE_VIDEO, offset,
						&pts, &frame_size, 0, &pts_us64) == 0) {
						new_pic->pts_valid = true;
						new_pic->pts = pts;
						new_pic->pts64 = pts_us64;
					} else
						new_pic->pts_valid = false;
				}
			}
		} else {
			if (hw->chunk) {
				hw->last_chunk_pts = hw->chunk->pts;
				if (hw->first_field_timestamp_valid)
					new_pic->last_timestamp = hw->first_field_timestamp;
				else
					new_pic->last_timestamp = hw->chunk->timestamp;
				hw->first_field_timestamp_valid = false;
				new_pic->timestamp = hw->chunk->timestamp;
			}
			new_pic->pts_valid = false;
		}

		debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
			"mmpeg12: new_pic=%d, ind=%d, info=%x, seq=%x, offset=%d\n",
			hw->dec_num, index, info, seqinfo, offset);

		hw->frame_prog = info & PICINFO_PROG;
#if 1
		if ((seqinfo & SEQINFO_EXT_AVAILABLE) &&
			((seqinfo & SEQINFO_PROG) == 0))
			hw->frame_prog = 0;
#endif
		force_interlace_check(hw);

		if (is_ref_error(hw)) {
			if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_B)
				new_pic->buffer_info |= PICINFO_ERROR;
		}

		if (((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) ||
			((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_P)) {
			index = update_reference(hw, index);
		} else {
			/* drop b frame before reference pic ready */
			if (hw->refs[0] == -1)
				index = hw->buf_num;
		}
		vmpeg12_save_hw_context(hw, reg);

		if (index >= hw->buf_num) {
			if (hw->dec_num != 2) {
				debug_print(DECODE_ID(hw), 0,
				"mmpeg12: drop pic num %d, type %c, index %d, offset %x\n",
				hw->dec_num, GET_SLICE_TYPE(info), index, offset);
				hw->dec_result = DEC_RESULT_ERROR;
			}
			vdec_schedule_work(&hw->work);
			return IRQ_HANDLED;
		}

		vdec_count_info(&hw->gvs, info & PICINFO_ERROR, offset);

		disp_pic = &hw->pics[index];
		info = hw->pics[index].buffer_info;
		if (disp_pic->pts_valid && hw->lastpts64 == disp_pic->pts64)
			disp_pic->pts_valid = false;
		if (disp_pic->pts_valid)
			hw->lastpts64 = disp_pic->pts64;

		if (input_frame_based(hw_to_vdec(hw)))
			frame_size = new_pic->frame_size;

		fill_frame_info(hw, info, frame_size, new_pic->pts);

		if ((hw->first_i_frame_ready == 0) &&
			((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) &&
			((info & PICINFO_ERROR) == 0)) {
			hw->first_i_frame_ready = 1;
		}

		debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
			"mmpeg12: disp_pic=%d(%c), ind=%d, offst=%x, pts=(%d,%lld,%lld)(%d)\n",
			hw->disp_num, GET_SLICE_TYPE(info), index, disp_pic->offset,
			disp_pic->pts, disp_pic->pts64,
			disp_pic->timestamp, disp_pic->pts_valid);

		prepare_display_buf(hw, disp_pic);
		vdec_schedule_work(&hw->work);
	}

	return IRQ_HANDLED;
}
static irqreturn_t vmpeg12_isr(struct vdec_s *vdec, int irq)
{
	u32 info, offset;
	struct vdec_mpeg12_hw_s *hw =
	(struct vdec_mpeg12_hw_s *)(vdec->private);
	if (hw->eos)
		return IRQ_HANDLED;
	info = READ_VREG(MREG_PIC_INFO);
	offset = READ_VREG(MREG_FRAME_OFFSET);

	if (info &PICINFO_ERROR) {
		if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) {
			hw->gvs.i_concealed_frames++;
		} else if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_P) {
			hw->gvs.p_concealed_frames++;
		} else if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_B) {
			hw->gvs.b_concealed_frames++;
		}
	}
	if (offset) {
		if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) {
			hw->gvs.i_decoded_frames++;
		} else if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_P) {
			hw->gvs.p_decoded_frames++;
		} else if ((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_B) {
			hw->gvs.b_decoded_frames++;
		}
	}

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	return IRQ_WAKE_THREAD;
}

static void vmpeg12_notify_work(struct work_struct *work)
{

}

static void wait_vmmpeg12_search_done(struct vdec_mpeg12_hw_s *hw)
{
	u32 vld_rp = READ_VREG(VLD_MEM_VIFIFO_RP);
	int count = 0;

	do {
		usleep_range(100, 500);
		if (vld_rp == READ_VREG(VLD_MEM_VIFIFO_RP))
			break;
		if (count > 1000) {
			debug_print(DECODE_ID(hw), 0,
					"%s, count %d  vld_rp 0x%x VLD_MEM_VIFIFO_RP 0x%x\n",
					__func__, count, vld_rp, READ_VREG(VLD_MEM_VIFIFO_RP));
			break;
		} else
			vld_rp = READ_VREG(VLD_MEM_VIFIFO_RP);
		count++;
	} while (1);
}

static void flush_output(struct vdec_mpeg12_hw_s *hw)
{
	int index = hw->refs[1];

	/* video only one frame need not flush. */
	if (hw->dec_num < 2)
		return;

	if ((hw->refs[0] >= 0) &&
		(hw->refs[0] < hw->buf_num))
		hw->ref_use[hw->refs[0]] = 0;

	if (index >= 0 && index < hw->buf_num) {
		hw->ref_use[index] = 0;
		prepare_display_buf(hw, &hw->pics[index]);
	}
}

static bool is_avaliable_buffer(struct vdec_mpeg12_hw_s *hw);

static int notify_v4l_eos(struct vdec_s *vdec)
{
	struct vdec_mpeg12_hw_s *hw = (struct vdec_mpeg12_hw_s *)vdec->private;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	struct vframe_s *vf = &hw->vframe_dummy;
	struct vdec_v4l2_buffer *fb = NULL;
	int index = INVALID_IDX;
	ulong expires;

	if (hw->eos) {
		expires = jiffies + msecs_to_jiffies(2000);
		while (!is_avaliable_buffer(hw)) {
			if (time_after(jiffies, expires)) {
				pr_err("[%d] MPEG2 isn't enough buff for notify eos.\n", ctx->id);
				return 0;
			}
		}

		index = find_free_buffer(hw);
		if (INVALID_IDX == index) {
			pr_err("[%d] MPEG2 EOS get free buff fail.\n", ctx->id);
			return 0;
		}

		fb = (struct vdec_v4l2_buffer *)
			hw->pics[index].v4l_ref_buf_addr;

		vf->type		|= VIDTYPE_V4L_EOS;
		vf->timestamp		= ULONG_MAX;
		vf->v4l_mem_handle	= (ulong)fb;
		vf->flag		= VFRAME_FLAG_EMPTY_FRAME_V4L;

		vdec_vframe_ready(vdec, vf);
		kfifo_put(&hw->display_q, (const struct vframe_s *)vf);
		ATRACE_COUNTER(hw->pts_name, vf->timestamp);

		ATRACE_COUNTER("VC_OUT_DEC-submit", fb->buf_idx);
		fb->task->submit(fb->task, TASK_TYPE_DEC);

		pr_info("[%d] mpeg12 EOS notify.\n", ctx->id);
	}

	return 0;
}

static void vmpeg12_work_implement(struct vdec_mpeg12_hw_s *hw,
	struct vdec_s *vdec, int from)
{
	int r;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)(hw->v4l2_ctx);

	if (hw->dec_result != DEC_RESULT_DONE)
		debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
			"%s, result=%d, status=%d\n", __func__,
			hw->dec_result, vdec->next_status);

	ATRACE_COUNTER("V_ST_DEC-work_state", hw->dec_result);

	if (hw->dec_result == DEC_RESULT_DONE) {
		if (vdec->input.swap_valid)
			hw->dec_again_cnt = 0;
		vdec_vframe_dirty(vdec, hw->chunk);
		hw->chunk = NULL;
		hw->chunk_header_offset = 0;
		hw->chunk_res_size = 0;
	} else if (hw->dec_result == DEC_RESULT_AGAIN &&
	(vdec->next_status != VDEC_STATUS_DISCONNECTED)) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
		if (!vdec_has_more_input(vdec)) {
			hw->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hw->work);
			return;
		}
#ifdef AGAIN_HAS_THRESHOLD
		hw->next_again_flag = 1;
#endif
	} else if (hw->dec_result == DEC_RESULT_GET_DATA &&
		vdec->next_status != VDEC_STATUS_DISCONNECTED) {
		if (!vdec_has_more_input(vdec)) {
			hw->dec_result = DEC_RESULT_EOS;
			vdec_schedule_work(&hw->work);
			return;
		}
		debug_print(DECODE_ID(hw), PRINT_FLAG_VLD_DETAIL,
			"%s DEC_RESULT_GET_DATA %x %x %x\n",
			__func__,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP));
		if (hw->chunk != NULL) {
			hw->first_field_timestamp = hw->chunk->timestamp;
			hw->first_field_timestamp_valid = true;
		}

		vdec_vframe_dirty(vdec, hw->chunk);
		hw->chunk_header_offset = 0;
		hw->chunk_res_size = 0;
		hw->chunk = NULL;
		vdec_clean_input(vdec);

		r = vdec_prepare_input(vdec, &hw->chunk);
		if (r < 0) {
			hw->input_empty++;
			reset_process_time(hw);
			hw->dec_result = DEC_RESULT_GET_DATA;
			debug_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
				"%s: Insufficient data, get data retry\n", __func__);
			vdec_schedule_work(&hw->work);
			return;
		}

		hw->input_empty = 0;
		if (vdec_frame_based(vdec) && (hw->chunk != NULL)) {
			r = hw->chunk->size +
				(hw->chunk->offset & (VDEC_FIFO_ALIGN - 1));
			WRITE_VREG(VIFF_BIT_CNT, r * 8);
			if (vdec->mvfrm)
				vdec->mvfrm->frame_size += hw->chunk->size;
		}
		debug_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s: %x %x %x size %d, bitcnt %d\n",
			__func__,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			r, READ_VREG(VIFF_BIT_CNT));
		vdec_enable_input(vdec);
		hw->dec_result = DEC_RESULT_NONE;
		hw->last_vld_level = 0;
		start_process_time_set(hw);
		hw->init_flag = 1;
		mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
		WRITE_VREG(MREG_BUFFEROUT, 0);
		return;
	} else if (hw->dec_result == DEC_RESULT_FORCE_EXIT) {
		debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"%s: force exit\n", __func__);
		if (hw->stat & STAT_ISR_REG) {
			amvdec_stop();
			vdec_free_irq(VDEC_IRQ_1, (void *)hw);
			hw->stat &= ~STAT_ISR_REG;
		}
	} else if (hw->dec_result == DEC_RESULT_EOS) {
		if (hw->stat & STAT_VDEC_RUN) {
			amvdec_stop();
			hw->stat &= ~STAT_VDEC_RUN;
		}
		hw->eos = 1;
		vdec_vframe_dirty(vdec, hw->chunk);
		hw->chunk_header_offset = 0;
		hw->chunk_res_size = 0;
		hw->chunk = NULL;
		vdec_clean_input(vdec);
		flush_output(hw);

		ATRACE_COUNTER("V_ST_DEC-submit_eos", __LINE__);
		notify_v4l_eos(vdec);
		ATRACE_COUNTER("V_ST_DEC-submit_eos", 0);

		debug_print(DECODE_ID(hw), 0,
			"%s: end of stream, num %d(%d)\n",
			__func__, hw->disp_num, hw->dec_num);
	}
	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}
	/*disable mbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 0);
	del_timer_sync(&hw->check_timer);
	hw->stat &= ~STAT_TIMER_ARM;
	wait_vmmpeg12_search_done(hw);

	if (from == 1) {
		/*This is a timeout work*/
		if (work_pending(&hw->work)) {
			pr_err("timeout work return befor finishing.");
			/*
			 * The vmpeg12_work arrives at the last second,
			 * give it a chance to handle the scenario.
			 */
			return;
		}
	}

	if (vdec->parallel_dec == 1)
		vdec_core_finish_run(vdec, CORE_MASK_VDEC_1);
	else
		vdec_core_finish_run(vdec, CORE_MASK_VDEC_1 | CORE_MASK_HEVC);

	if (ctx->param_sets_from_ucode &&
		!hw->v4l_params_parsed)
		vdec_v4l_write_frame_sync(ctx);

	ATRACE_COUNTER("V_ST_DEC-chunk_size", 0);

	if (hw->vdec_cb)
		hw->vdec_cb(vdec, hw->vdec_cb_arg);
}

static void vmpeg12_work(struct work_struct *work)
{
	struct vdec_mpeg12_hw_s *hw =
	container_of(work, struct vdec_mpeg12_hw_s, work);
	struct vdec_s *vdec = hw_to_vdec(hw);

	vmpeg12_work_implement(hw, vdec, 0);
}
static void vmpeg12_timeout_work(struct work_struct *work)
{
	struct vdec_mpeg12_hw_s *hw =
	container_of(work, struct vdec_mpeg12_hw_s, timeout_work);
	struct vdec_s *vdec = hw_to_vdec(hw);

	if (work_pending(&hw->work)) {
		pr_err("timeout work return befor executing.");
		return;
	}

	hw->timeout_processing = 1;
	vmpeg12_work_implement(hw, vdec, 1);
}

static struct vframe_s *vmpeg_vf_peek(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg12_hw_s *hw =
	(struct vdec_mpeg12_hw_s *)vdec->private;
	atomic_add(1, &hw->peek_num);
	if (kfifo_peek(&hw->display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vmpeg_vf_get(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg12_hw_s *hw =
		(struct vdec_mpeg12_hw_s *)vdec->private;

	if (kfifo_get(&hw->display_q, &vf)) {
		vf->index_disp = atomic_read(&hw->get_num);
		atomic_add(1, &hw->get_num);
		ATRACE_COUNTER(hw->disp_q_name, kfifo_len(&hw->display_q));
		return vf;
	}
	return NULL;
}

static int mpeg12_valid_vf_check(struct vframe_s *vf, struct vdec_mpeg12_hw_s *hw)
{
	int i;

	if (vf == NULL || (vf->index == -1))
		return 0;

	for (i = 0; i < VF_POOL_SIZE; i++) {
		if (vf == &hw->vfpool[i])
			return 1;
	}

	return 0;
}

static void vmpeg_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg12_hw_s *hw =
		(struct vdec_mpeg12_hw_s *)vdec->private;
	unsigned long flags;

	if (!mpeg12_valid_vf_check(vf, hw)) {
		debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"invalid vf: %lx\n", (ulong)vf);
		return ;
	}

	if (vf->meta_data_buf) {
		vf->meta_data_buf = NULL;
		vf->meta_data_size = 0;
	}

	if (vf->v4l_mem_handle !=
		hw->pics[vf->index].v4l_ref_buf_addr) {
		hw->pics[vf->index].v4l_ref_buf_addr
			= vf->v4l_mem_handle;

		debug_print(DECODE_ID(hw), PRINT_FLAG_V4L_DETAIL,
			"MPEG12 update fb handle, old:%llx, new:%llx\n",
			hw->pics[vf->index].v4l_ref_buf_addr,
			vf->v4l_mem_handle);
	}
	spin_lock_irqsave(&hw->lock, flags);
	hw->vfbuf_use[vf->index]--;
	if  (hw->vfbuf_use[vf->index] < 0) {
		debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"warn: vf %lx, index %d putback repetitive, set use to 0\n", (ulong)vf, vf->index);
		hw->vfbuf_use[vf->index] = 0;
	}
	atomic_add(1, &hw->put_num);
	debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
		"%s: vf: %lx, index: %d, use: %d\n", __func__, (ulong)vf,
		vf->index, hw->vfbuf_use[vf->index]);

	kfifo_put(&hw->newframe_q,
		(const struct vframe_s *)vf);
	ATRACE_COUNTER(hw->new_q_name, kfifo_len(&hw->newframe_q));
	spin_unlock_irqrestore(&hw->lock, flags);
}

static int vmpeg_event_cb(int type, void *data, void *op_arg)
{
	struct vdec_s *vdec = op_arg;

	if (type & VFRAME_EVENT_RECEIVER_REQ_STATE) {
		struct provider_state_req_s *req =
			(struct provider_state_req_s *)data;
		if (req->req_type == REQ_STATE_SECURE)
			req->req_result[0] = vdec_secure(vdec);
		else
			req->req_result[0] = 0xffffffff;
	}

	return 0;
}

static int vmpeg_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg12_hw_s *hw =
		(struct vdec_mpeg12_hw_s *)vdec->private;

	spin_lock_irqsave(&hw->lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&hw->newframe_q);
	states->buf_avail_num = kfifo_len(&hw->display_q);
	states->buf_recycle_num = 0;

	spin_unlock_irqrestore(&hw->lock, flags);
	return 0;
}
static int vmmpeg12_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	struct vdec_mpeg12_hw_s *hw =
	(struct vdec_mpeg12_hw_s *)vdec->private;

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
	vstatus->bit_rate = hw->gvs.bit_rate;
	vstatus->frame_dur = hw->frame_dur;
	vstatus->frame_data = hw->gvs.frame_data;
	vstatus->total_data = hw->gvs.total_data;
	vstatus->frame_count = hw->gvs.frame_count;
	vstatus->error_frame_count = hw->gvs.error_frame_count;
	vstatus->drop_frame_count = hw->drop_frame_count;
	vstatus->i_decoded_frames = hw->gvs.i_decoded_frames;
	vstatus->i_lost_frames = hw->gvs.i_lost_frames;
	vstatus->i_concealed_frames = hw->gvs.i_concealed_frames;
	vstatus->p_decoded_frames = hw->gvs.p_decoded_frames;
	vstatus->p_lost_frames = hw->gvs.p_lost_frames;
	vstatus->p_concealed_frames = hw->gvs.p_concealed_frames;
	vstatus->b_decoded_frames = hw->gvs.b_decoded_frames;
	vstatus->b_lost_frames = hw->gvs.b_lost_frames;
	vstatus->b_concealed_frames = hw->gvs.b_concealed_frames;
	vstatus->total_data = hw->gvs.total_data;
	vstatus->samp_cnt = hw->gvs.samp_cnt;
	vstatus->offset = hw->gvs.offset;
	vstatus->ratio_control = hw->ratio_control;
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
			"%s", DRIVER_NAME);

	return 0;
}

/****************************************/
static void vmpeg12_workspace_init(struct vdec_mpeg12_hw_s *hw)
{
	int ret;

	ret = decoder_bmmu_box_alloc_buf_phy(hw->mm_blk_handle,
			DECODE_BUFFER_NUM_MAX,
			WORKSPACE_SIZE,
			DRIVER_NAME,
			&hw->buf_start);
	if (ret < 0) {
		pr_err("mpeg2 workspace alloc size %d failed.\n",
			WORKSPACE_SIZE);
		return;
	}

	if (!hw->ccbuf_phyAddress_virt) {
		hw->cc_buf_size = AUX_BUF_ALIGN(CCBUF_SIZE);
		hw->ccbuf_phyAddress_virt =
			dma_alloc_coherent(amports_get_dma_device(),
					  hw->cc_buf_size, &hw->ccbuf_phyAddress,
					  GFP_KERNEL);
		if (hw->ccbuf_phyAddress_virt == NULL) {
			pr_err("%s: failed to alloc cc buffer\n", __func__);
			return;
		}
	}

	WRITE_VREG(MREG_CO_MV_START, hw->buf_start);
	WRITE_VREG(MREG_CC_ADDR, hw->ccbuf_phyAddress);

	return;
}

static void vmpeg2_dump_state(struct vdec_s *vdec)
{
	struct vdec_mpeg12_hw_s *hw =
		(struct vdec_mpeg12_hw_s *)(vdec->private);
	u32 i;
	debug_print(DECODE_ID(hw), 0,
		"====== %s\n", __func__);
	debug_print(DECODE_ID(hw), 0,
		"width/height (%d/%d),i_first %d, buf_num %d\n",
		hw->frame_width,
		hw->frame_height,
		hw->first_i_frame_ready,
		hw->buf_num
		);
	debug_print(DECODE_ID(hw), 0,
		"is_framebase(%d), eos %d, state 0x%x, dec_result 0x%x dec_frm %d put_frm %d run %d not_run_ready %d,input_empty %d\n",
		vdec_frame_based(vdec),
		hw->eos,
		hw->stat,
		hw->dec_result,
		hw->dec_num,
		hw->put_num,
		hw->run_count,
		hw->not_run_ready,
		hw->input_empty
		);

	for (i = 0; i < hw->buf_num; i++) {
		debug_print(DECODE_ID(hw), 0,
			"index %d, used %d, ref %d\n", i,
			hw->vfbuf_use[i], hw->ref_use[i]);
	}

	debug_print(DECODE_ID(hw), 0,
	"%s, newq(%d/%d), dispq(%d/%d) vf pre/get/put (%d/%d/%d),drop=%d, buffer_not_ready %d\n",
	__func__,
	kfifo_len(&hw->newframe_q),
	VF_POOL_SIZE,
	kfifo_len(&hw->display_q),
	VF_POOL_SIZE,
	hw->disp_num,
	hw->get_num,
	hw->put_num,
	hw->drop_frame_count,
	hw->buffer_not_ready
	);
	debug_print(DECODE_ID(hw), 0,
		"VIFF_BIT_CNT=0x%x\n",
		READ_VREG(VIFF_BIT_CNT));
	debug_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_LEVEL=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_LEVEL));
	debug_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_WP=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_WP));
	debug_print(DECODE_ID(hw), 0,
		"VLD_MEM_VIFIFO_RP=0x%x\n",
		READ_VREG(VLD_MEM_VIFIFO_RP));
	debug_print(DECODE_ID(hw), 0,
		"PARSER_VIDEO_RP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_rp));
	debug_print(DECODE_ID(hw), 0,
		"PARSER_VIDEO_WP=0x%x\n",
		STBUF_READ(&vdec->vbuf, get_wp));

	if (vdec_frame_based(vdec) &&
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

			debug_print(DECODE_ID(hw), 0,
				"frame data size 0x%x\n",
				hw->chunk->size);
			for (jj = 0; jj < hw->chunk->size; jj++) {
				if ((jj & 0xf) == 0)
					debug_print(DECODE_ID(hw),
						PRINT_FRAMEBASE_DATA,
						"%06x:", jj);
				debug_print(DECODE_ID(hw),
				PRINT_FRAMEBASE_DATA,
					"%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					debug_print(DECODE_ID(hw),
						PRINT_FRAMEBASE_DATA, "\n");
			}

			if (!hw->chunk->block->is_mapped)
				codec_mm_unmap_phyaddr(data);
		}
	}
}

static void reset_process_time(struct vdec_mpeg12_hw_s *hw)
{
	if (hw->start_process_time) {
		unsigned process_time =
			1000 * (jiffies - hw->start_process_time) / HZ;
		hw->start_process_time = 0;
		if (process_time > max_process_time[DECODE_ID(hw)])
			max_process_time[DECODE_ID(hw)] = process_time;
	}
}

static void start_process_time_set(struct vdec_mpeg12_hw_s *hw)
{
	if ((hw->refs[1] != -1) && (hw->refs[0] == -1))
		hw->decode_timeout_count = 1;
	else
		hw->decode_timeout_count = 10;
	hw->start_process_time = jiffies;
}
static void timeout_process(struct vdec_mpeg12_hw_s *hw)
{
	struct vdec_s *vdec = hw_to_vdec(hw);

	if (work_pending(&hw->work) ||
	    work_busy(&hw->work) ||
	    work_busy(&hw->timeout_work) ||
	    work_pending(&hw->timeout_work)) {
		pr_err("%s mpeg12[%d] timeout_process return befor do anything.\n",__func__, vdec->id);
		return;
	}
	reset_process_time(hw);
	amvdec_stop();
	debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
		"%s decoder timeout, status=%d, level=%d\n",
		__func__, vdec->status, READ_VREG(VLD_MEM_VIFIFO_LEVEL));
	hw->dec_result = DEC_RESULT_DONE;
	if ((hw->refs[1] != -1) && (hw->refs[0] != -1))
		hw->first_i_frame_ready = 0;

	/*
	 * In this very timeout point,the vmpeg12_work arrives,
	 * let it to handle the scenario.
	 */
	if (work_pending(&hw->work)) {
		pr_err("%s mpeg12[%d] return befor schedule.", __func__, vdec->id);
		return;
	}
	vdec_schedule_work(&hw->timeout_work);
}

static void check_timer_func(struct timer_list *timer)
{
	struct vdec_mpeg12_hw_s *hw = container_of(timer,
		struct vdec_mpeg12_hw_s, check_timer);
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

	if (((debug_enable & PRINT_FLAG_TIMEOUT_STATUS) == 0) &&
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

	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
}

static int vmpeg12_hw_ctx_restore(struct vdec_mpeg12_hw_s *hw)
{
	u32 index = -1;
	struct aml_vcodec_ctx * v4l2_ctx = hw->v4l2_ctx;
	int i;

	if (!hw->init_flag)
		vmpeg12_workspace_init(hw);

	if (hw->v4l_params_parsed) {
		struct vdec_pic_info pic;

		if (!hw->buf_num) {
			vdec_v4l_get_pic_info(v4l2_ctx, &pic);
			hw->buf_num = pic.dpb_frames +
				pic.dpb_margin;
			if (hw->buf_num > DECODE_BUFFER_NUM_MAX)
				hw->buf_num = DECODE_BUFFER_NUM_MAX;
		}

		index = find_free_buffer(hw);
		if ((index < 0) || (index >= hw->buf_num))
			return -1;

		WRITE_VREG(MREG_CO_MV_START, hw->buf_start);
		WRITE_VREG(MREG_CC_ADDR, hw->ccbuf_phyAddress);

		for (i = 0; i < hw->buf_num; i++) {
			if (hw->pics[i].v4l_ref_buf_addr) {
				config_cav_lut(canvas_y(hw->canvas_spec[i]),
					&hw->canvas_config[i][0], VDEC_1);
				config_cav_lut(canvas_u(hw->canvas_spec[i]),
					&hw->canvas_config[i][1], VDEC_1);
			}
		}

		/* prepare REF0 & REF1
		points to the past two IP buffers
		prepare REC_CANVAS_ADDR and ANC2_CANVAS_ADDR
		points to the output buffer*/
		WRITE_VREG(MREG_REF0,
			(hw->refs[0] == -1) ? 0xffffffff :
			hw->canvas_spec[hw->refs[0]]);
		WRITE_VREG(MREG_REF1,
			(hw->refs[1] == -1) ? 0xffffffff :
			hw->canvas_spec[hw->refs[1]]);
		WRITE_VREG(REC_CANVAS_ADDR, hw->canvas_spec[index]);
		WRITE_VREG(ANC2_CANVAS_ADDR, hw->canvas_spec[index]);

		debug_print(DECODE_ID(hw), PRINT_FLAG_RESTORE,
			"%s,ref0=0x%x, ref1=0x%x,rec=0x%x, ctx_valid=%d,index=%d\n",
			__func__,
			READ_VREG(MREG_REF0),
			READ_VREG(MREG_REF1),
			READ_VREG(REC_CANVAS_ADDR),
			hw->ctx_valid, index);
	}

	/* set to mpeg1 default */
	WRITE_VREG(MPEG1_2_REG,
	(hw->ctx_valid) ? hw->reg_mpeg1_2_reg : 0);
	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);
	/* for Mpeg1 default value */
	WRITE_VREG(PIC_HEAD_INFO,
	(hw->ctx_valid) ? hw->reg_pic_head_info : 0x380);
	/* disable mpeg4 */
	WRITE_VREG(M4_CONTROL_REG, 0);
	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);
	/* clear buffer IN/OUT registers */
	WRITE_VREG(MREG_BUFFEROUT, 0);
	/* enable mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_MASK, 1);
	/* set reference width and height */
	if ((hw->frame_width != 0) && (hw->frame_height != 0))
		WRITE_VREG(MREG_CMD,
		(hw->frame_width << 16) | hw->frame_height);
	else
		WRITE_VREG(MREG_CMD, 0);

	debug_print(DECODE_ID(hw), PRINT_FLAG_RESTORE,
		"0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		hw->frame_width, hw->frame_height, hw->seqinfo,
		hw->reg_f_code_reg, hw->reg_slice_ver_pos_pic_type,
		hw->reg_mb_info);

	WRITE_VREG(MREG_PIC_WIDTH, hw->reg_pic_width);
	WRITE_VREG(MREG_PIC_HEIGHT, hw->reg_pic_height);
	WRITE_VREG(MREG_SEQ_INFO, hw->seqinfo);
	WRITE_VREG(F_CODE_REG, hw->reg_f_code_reg);
	WRITE_VREG(SLICE_VER_POS_PIC_TYPE,
		hw->reg_slice_ver_pos_pic_type);
	WRITE_VREG(MB_INFO, hw->reg_mb_info);
	WRITE_VREG(VCOP_CTRL_REG, hw->reg_vcop_ctrl_reg);
	WRITE_VREG(AV_SCRATCH_H, hw->reg_signal_type);

	if (READ_VREG(MREG_ERROR_COUNT) != 0 ||
			READ_VREG(MREG_FATAL_ERROR) == 1)
		debug_print(DECODE_ID(hw), PRINT_FLAG_RESTORE,
				"err_cnt:%d fa_err:%d\n",
				READ_VREG(MREG_ERROR_COUNT),
				READ_VREG(MREG_FATAL_ERROR));

	/* clear error count */
	WRITE_VREG(MREG_ERROR_COUNT, 0);
	/*Use MREG_FATAL_ERROR bit1, the ucode determine
		whether to report the interruption of width and
		height information,in order to be compatible
		with the old version of ucode.
		1: Report the width and height information
		0: No Report
	  bit0:
	        1: Use cma cc buffer for new driver
	        0: use codec mm cc buffer for old driver
		*/
	WRITE_VREG(MREG_FATAL_ERROR, 3);
	/* clear wait buffer status */
	WRITE_VREG(MREG_WAIT_BUFFER, 0);
#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1<<17);
#endif

	/* cbcr_merge_swap_en */
	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_T7) {
		if ((v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21) ||
			(v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21M))
			CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);
		else
			SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);
	} else {
		if ((v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21) ||
			(v4l2_ctx->q_data[AML_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_NV21M))
			SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);
		else
			CLEAR_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 16);
	}

	if (!hw->ctx_valid)
		WRITE_VREG(AV_SCRATCH_J, hw->userdata_wp_ctx);
	else
		WRITE_VREG(AV_SCRATCH_J, 0);
	if (hw->chunk) {
		/*frame based input*/
		WRITE_VREG(MREG_INPUT,
		(hw->chunk->offset & 7) | (1<<7) | (hw->ctx_valid<<6));
	} else {
		/*stream based input*/
		WRITE_VREG(MREG_INPUT, (hw->ctx_valid<<6));
	}
	return 0;
}

static void vmpeg12_local_init(struct vdec_mpeg12_hw_s *hw)
{
	int i;
	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf;
		vf = &hw->vfpool[i];
		hw->vfpool[i].index = DECODE_BUFFER_NUM_MAX;
		kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
	}

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		hw->vfbuf_use[i] = 0;
		hw->ref_use[i] = 0;
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
			CODEC_MM_FLAGS_FOR_VDECODER |
			hw->tvp_flag,
			BMMU_ALLOC_FLAGS_WAIT);

	hw->eos = 0;
	hw->frame_width = hw->frame_height = 0;
	hw->frame_dur = hw->frame_prog = 0;
	hw->frame_force_skip_flag = 0;
	hw->wait_buffer_counter = 0;
	hw->first_i_frame_ready = 0;
	hw->dec_control &= DEC_CONTROL_INTERNAL_MASK;
	hw->refs[0] = -1;
	hw->refs[1] = -1;
	hw->dec_num = 0;
	hw->run_count = 0;
	hw->not_run_ready = 0;
	hw->input_empty = 0;
	hw->drop_frame_count = 0;
	hw->buffer_not_ready = 0;
	hw->start_process_time = 0;
	hw->init_flag = 0;
	hw->dec_again_cnt = 0;
	hw->error_frame_skip_level = error_frame_skip_level;
	atomic_set(&hw->disp_num, 0);
	atomic_set(&hw->put_num, 0);
	atomic_set(&hw->get_num, 0);
	atomic_set(&hw->peek_num, 0);

	if (dec_control)
		hw->dec_control = dec_control;
}

static s32 vmpeg12_init(struct vdec_mpeg12_hw_s *hw)
{
	int size;
	u32 fw_size = 16*0x1000;
	struct firmware_s *fw;

	vmpeg12_local_init(hw);

	fw = vmalloc(sizeof(struct firmware_s) + fw_size);
	if (IS_ERR_OR_NULL(fw))
		return -ENOMEM;

	pr_debug("get firmware ...\n");
	size = get_firmware_data(VIDEO_DEC_MPEG12_MULTI, fw->data);
	if (size < 0) {
		pr_err("get firmware fail.\n");
		vfree(fw);
		return -1;
	}

	fw->len = size;
	hw->fw = fw;

	INIT_WORK(&hw->userdata_push_work, userdata_push_do_work);
	INIT_WORK(&hw->work, vmpeg12_work);
	INIT_WORK(&hw->timeout_work, vmpeg12_timeout_work);
	INIT_WORK(&hw->notify_work, vmpeg12_notify_work);

	if (NULL == hw->user_data_buffer) {
		hw->user_data_buffer = kmalloc(USER_DATA_SIZE,
							GFP_KERNEL);
		if (!hw->user_data_buffer) {
			pr_info("%s: Can not allocate user_data_buffer\n",
				   __func__);
			return -1;
		}
	}

	vmmpeg2_crate_userdata_manager(hw,
			hw->user_data_buffer,
			USER_DATA_SIZE);

	timer_setup(&hw->check_timer, check_timer_func, 0);
	hw->check_timer.expires = jiffies + CHECK_INTERVAL;

	hw->stat |= STAT_TIMER_ARM;
	hw->stat |= STAT_ISR_REG;

	hw->buf_start = 0;
	WRITE_VREG(DECODE_STOP_POS, udebug_flag);

	return 0;
}

static bool is_avaliable_buffer(struct vdec_mpeg12_hw_s *hw)
{
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int i, free_count = 0;
	int used_count = 0;

	if ((hw->buf_num == 0) ||
		(ctx->cap_pool.dec < hw->buf_num)) {
		if (ctx->fb_ops.query(&ctx->fb_ops, &hw->fb_token)) {
			free_count =
				v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx) + 1;
		}
	}

	for (i = 0; i < hw->buf_num; ++i) {
		if ((hw->vfbuf_use[i] == 0) &&
			(hw->ref_use[i] == 0) &&
			hw->pics[i].v4l_ref_buf_addr) {
			free_count++;
		} else if (hw->pics[i].v4l_ref_buf_addr)
			used_count++;
	}

	ATRACE_COUNTER("V_ST_DEC-free_buff_count", free_count);
	ATRACE_COUNTER("V_ST_DEC-used_buff_count", used_count);

	return free_count >= run_ready_min_buf_num ? 1 : 0;
}

static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	struct vdec_mpeg12_hw_s *hw =
		(struct vdec_mpeg12_hw_s *)vdec->private;
	struct aml_vcodec_ctx *ctx =
		(struct aml_vcodec_ctx *)(hw->v4l2_ctx);
	int ret = 0;

	if (hw->eos)
		return 0;

	if (hw->timeout_processing &&
	    (work_pending(&hw->work) || work_busy(&hw->work) ||
	    work_pending(&hw->timeout_work) || work_busy(&hw->timeout_work))) {
		debug_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"mpeg12 work pending,not ready for run.\n");
		return 0;
	}
	hw->timeout_processing = 0;
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

#ifdef AGAIN_HAS_THRESHOLD
	if (hw->next_again_flag&&
		(!vdec_frame_based(vdec))) {
		u32 parser_wr_ptr =
			STBUF_READ(&vdec->vbuf, get_wp);
		if (parser_wr_ptr >= hw->pre_parser_wr_ptr &&
			(parser_wr_ptr - hw->pre_parser_wr_ptr) <
			again_threshold) {
			int r = vdec_sync_input(vdec);
			debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
			"%s buf level%x\n",
			__func__, r);
			return 0;
		}
	}
#endif

	if (hw->v4l_params_parsed) {
		ret = is_avaliable_buffer(hw) ? 1 : 0;
	} else {
		ret = ctx->v4l_resolution_change ? 0 : 1;
	}

	hw->not_run_ready = 0;
	hw->buffer_not_ready = 0;

	return ret ? CORE_MASK_VDEC_1 : 0;
}

static unsigned char get_data_check_sum
	(struct vdec_mpeg12_hw_s *hw, int size)
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

static int check_dirty_data(struct vdec_s *vdec)
{
	struct vdec_mpeg12_hw_s *hw =
		(struct vdec_mpeg12_hw_s *)(vdec->private);
	u32 wp, rp, level;

	rp = STBUF_READ(&vdec->vbuf, get_rp);
	wp = STBUF_READ(&vdec->vbuf, get_wp);

	if (wp > rp)
		level = wp - rp;
	else
		level = wp + vdec->input.size - rp ;

	if (hw->next_again_flag &&
		hw->pre_parser_wr_ptr !=
			STBUF_READ(&vdec->vbuf, get_wp))
		hw->dec_again_cnt++;
	if ((level > (vdec->input.size * 2 / 3) ) &&
			(hw->dec_again_cnt > dirty_again_threshold)) {
		debug_print(DECODE_ID(hw), 0, "mpeg12 data skipped %x, level %x\n", ((level / 2) >> 20) << 20, level);
		if (vdec->input.swap_valid) {
			vdec_stream_skip_data(vdec, ((level / 2) >> 20) << 20);
			hw->dec_again_cnt = 0;
		}
		return 1;
	}
	return 0;
}


static void run(struct vdec_s *vdec, unsigned long mask,
void (*callback)(struct vdec_s *, void *),
		void *arg)
{
	struct vdec_mpeg12_hw_s *hw =
		(struct vdec_mpeg12_hw_s *)vdec->private;
	int save_reg;
	int size, ret;
	if (!hw->vdec_pg_enable_flag) {
		hw->vdec_pg_enable_flag = 1;
		amvdec_enable();
	}
	save_reg = READ_VREG(POWER_CTL_VLD);
	/* reset everything except DOS_TOP[1] and APB_CBUS[0]*/
	WRITE_VREG(DOS_SW_RESET0, 0xfffffff0);
	WRITE_VREG(DOS_SW_RESET0, 0);
	WRITE_VREG(POWER_CTL_VLD, save_reg);
	hw->run_count++;
	vdec_reset_core(vdec);
	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;

	if ((vdec_stream_based(vdec)) &&
			(error_proc_policy & 0x1) &&
			check_dirty_data(vdec)) {
		hw->dec_result = DEC_RESULT_AGAIN;
		if (!vdec->input.swap_valid) {
			debug_print(DECODE_ID(hw), 0, "mpeg12 start dirty data skipped\n");
			vdec_prepare_input(vdec, &hw->chunk);
			hw->dec_result = DEC_RESULT_DONE;
		}
		vdec_schedule_work(&hw->work);
		return;
	}

#ifdef AGAIN_HAS_THRESHOLD
	if (vdec_stream_based(vdec)) {
		hw->pre_parser_wr_ptr =
			STBUF_READ(&vdec->vbuf, get_wp);
		hw->next_again_flag = 0;
	}
#endif

	if ((vdec_frame_based(vdec)) && (hw->chunk_header_offset != 0) &&
		(!hw->v4l_params_parsed) && (hw->chunk != NULL) &&
		(hw->chunk_res_size != 0)) {
		hw->chunk->offset = hw->chunk_header_offset;
		hw->chunk->size = hw->chunk_res_size;
		hw->chunk_header_offset = 0;
		hw->chunk_res_size = 0;
		debug_print(DECODE_ID(hw), 0, "Multiple heads are parsed in a chunk and resolution changed.\n");
	}

	size = vdec_prepare_input(vdec, &hw->chunk);
	if (size < 0) {
		hw->input_empty++;
		hw->dec_result = DEC_RESULT_AGAIN;

		debug_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"vdec_prepare_input: Insufficient data\n");
		vdec_schedule_work(&hw->work);
		return;
	}

	ATRACE_COUNTER("V_ST_DEC-chunk_size", size);

	hw->input_empty = 0;
	if ((vdec_frame_based(vdec)) &&
		(hw->chunk != NULL)) {
		size = hw->chunk->size +
			(hw->chunk->offset & (VDEC_FIFO_ALIGN - 1));
		WRITE_VREG(VIFF_BIT_CNT, size * 8);
		if (vdec->mvfrm)
			vdec->mvfrm->frame_size = hw->chunk->size;
	}
	if (vdec_frame_based(vdec) && !vdec_secure(vdec)) {
		/* HW needs padding (NAL start) for frame ending */
		char* tail = (char *)hw->chunk->block->start_virt;

		tail += hw->chunk->offset + hw->chunk->size;
		tail[0] = 0;
		tail[1] = 0;
		tail[2] = 1;
		tail[3] = 0;
		codec_mm_dma_flush(tail, 4, DMA_TO_DEVICE);
	}

	if (vdec_frame_based(vdec) && debug_enable && !vdec_secure(vdec)) {
		u8 *data = NULL;
		if (hw->chunk)
			debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
				"run: chunk offset 0x%x, size %d\n",
				hw->chunk->offset, hw->chunk->size);

		if (!hw->chunk->block->is_mapped)
			data = codec_mm_vmap(hw->chunk->block->start +
				hw->chunk->offset, size);
		else
			data = ((u8 *)hw->chunk->block->start_virt) +
				hw->chunk->offset;

		if (debug_enable & PRINT_FLAG_VDEC_STATUS
			) {
			debug_print(DECODE_ID(hw), 0,
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
			debug_print(DECODE_ID(hw), PRINT_FRAMEBASE_DATA,
				"frame data:\n");
			for (jj = 0; jj < size; jj++) {
				if ((jj & 0xf) == 0)
					pr_info("%06x:", jj);
				pr_info("%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					pr_info("\n");
			}
			pr_info("\n");
		}

		if (!hw->chunk->block->is_mapped)
			codec_mm_unmap_phyaddr(data);
	} else {
		debug_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s: %x %x %x %x %x size 0x%x, bitcnt %d\n",
			__func__,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			STBUF_READ(&vdec->vbuf, get_rp),
			STBUF_READ(&vdec->vbuf, get_wp),
			size, READ_VREG(VIFF_BIT_CNT));
	}

	if (vdec->mc_loaded) {
	/*firmware have load before,
	  and not changes to another.
	  ignore reload.
	*/
	} else {
		ret = amvdec_vdec_loadmc_buf_ex(VFORMAT_MPEG12, "mmpeg12", vdec,
			hw->fw->data, hw->fw->len);
		if (ret < 0) {
			pr_err("[%d] %s: the %s fw loading failed, err: %x\n", vdec->id,
				hw->fw->name, tee_enabled() ? "TEE" : "local", ret);
			hw->dec_result = DEC_RESULT_FORCE_EXIT;
			vdec_schedule_work(&hw->work);
			return;
		}
		vdec->mc_loaded = 1;
		vdec->mc_type = VFORMAT_MPEG12;
	}

	if (vmpeg12_hw_ctx_restore(hw) < 0) {
		hw->dec_result = DEC_RESULT_ERROR;
		debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
		"ammvdec_mpeg12: error HW context restore\n");
		vdec_schedule_work(&hw->work);
		return;
	}

	hw->dec_result = DEC_RESULT_NONE;
	hw->stat |= STAT_MC_LOAD;
	vdec_enable_input(vdec);
	hw->last_vld_level = 0;
	start_process_time_set(hw);
	if (vdec->mvfrm)
		vdec->mvfrm->hw_decode_start = local_clock();
	amvdec_start();
	hw->stat |= STAT_VDEC_RUN;
	hw->stat |= STAT_TIMER_ARM;
	hw->init_flag = 1;
	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
}

static void reset(struct vdec_s *vdec)
{
	struct vdec_mpeg12_hw_s *hw =
		(struct vdec_mpeg12_hw_s *)vdec->private;
	int i;

	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}

	flush_work(&hw->work);
	flush_work(&hw->timeout_work);
	flush_work(&hw->notify_work);
	flush_work(&hw->userdata_push_work);
	reset_process_time(hw);

	for (i = 0; i < hw->buf_num; i++) {
		hw->pics[i].v4l_ref_buf_addr = 0;
		hw->vfbuf_use[i] = 0;
		hw->ref_use[i] = 0;
	}

	INIT_KFIFO(hw->display_q);
	INIT_KFIFO(hw->newframe_q);

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf = &hw->vfpool[i];

		memset((void *)vf, 0, sizeof(*vf));
		hw->vfpool[i].index = -1;
		kfifo_put(&hw->newframe_q, vf);
	}

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		vdec->free_canvas_ex(canvas_y(hw->canvas_spec[i]), vdec->id);
		vdec->free_canvas_ex(canvas_u(hw->canvas_spec[i]), vdec->id);
		hw->canvas_spec[i] = 0xffffff;
	}

	hw->refs[0]		= -1;
	hw->refs[1]		= -1;
	hw->ctx_valid		= 0;
	hw->dec_num		= 0;
	hw->eos			= 0;
	hw->buf_num		= 0;
	hw->frame_width		= 0;
	hw->frame_height	= 0;
	hw->first_i_frame_ready = 0;
	hw->first_field_timestamp = 0;
	hw->first_field_timestamp_valid = false;

	atomic_set(&hw->disp_num, 0);
	atomic_set(&hw->get_num, 0);
	atomic_set(&hw->put_num, 0);

	pr_info("mpeg12: reset.\n");
}

static int vmpeg12_set_trickmode(struct vdec_s *vdec, unsigned long trickmode)
{
	struct vdec_mpeg12_hw_s *hw =
	(struct vdec_mpeg12_hw_s *)vdec->private;
	if (!hw)
		return 0;

	if (trickmode == TRICKMODE_I) {
		hw->i_only = 0x3;
	} else if (trickmode == TRICKMODE_NONE) {
		hw->i_only = 0x0;
	}
	return 0;
}

static int ammvdec_mpeg12_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_mpeg12_hw_s *hw = NULL;
	int config_val = 0;

	pr_info("ammvdec_mpeg12 probe start.\n");

	if (pdata == NULL) {
		pr_info("ammvdec_mpeg12 platform data undefined.\n");
		return -EFAULT;
	}

	hw = vzalloc(sizeof(struct vdec_mpeg12_hw_s));
	if (hw == NULL) {
		pr_info("\nammvdec_mpeg12 decoder driver alloc failed\n");
		return -ENOMEM;
	}

	/* the ctx from v4l2 driver. */
	hw->v4l2_ctx = pdata->private;

	pdata->private = hw;
	pdata->dec_status = vmmpeg12_dec_status;
	pdata->set_trickmode = vmpeg12_set_trickmode;
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vmpeg12_isr;
	pdata->threaded_irq_handler = vmpeg12_isr_thread_fn;
	pdata->dump_state = vmpeg2_dump_state;

	pdata->user_data_read = vmmpeg2_user_data_read;
	pdata->reset_userdata_fifo = vmmpeg2_reset_userdata_fifo;
	pdata->wakeup_userdata_poll = vmmpeg2_wakeup_userdata_poll;

	snprintf(hw->vdec_name, sizeof(hw->vdec_name),
		"mpeg12-%d", pdev->id);
	snprintf(hw->pts_name, sizeof(hw->pts_name),
		"%s-timestamp", hw->vdec_name);
	snprintf(hw->new_q_name, sizeof(hw->new_q_name),
		"%s-newframe_q", hw->vdec_name);
	snprintf(hw->disp_q_name, sizeof(hw->disp_q_name),
		"%s-dispframe_q", hw->vdec_name);

	if (pdata->use_vfm_path) {
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			    VFM_DEC_PROVIDER_NAME);
		hw->frameinfo_enable = 1;
	}
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			PROVIDER_NAME ".%02x", pdev->id & 0xff);

	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vf_provider_ops, pdata);

	if (pdata->parallel_dec == 1) {
		int i;
		for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
			hw->canvas_spec[i] = 0xffffff;
	}
	platform_set_drvdata(pdev, pdata);

	hw->canvas_mode = pdata->canvas_mode;
	if (pdata->config_len) {
		if (get_config_int(pdata->config,
			"parm_v4l_codec_enable",
			&config_val) == 0)
			hw->is_used_v4l = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_canvas_mem_mode",
			&config_val) == 0)
			hw->canvas_mode = config_val;

		if (get_config_int(pdata->config,
			"parm_v4l_metadata_config_flag",
			&config_val) == 0)
			hw->force_prog_only = (config_val & VDEC_CFG_FLAG_PROG_ONLY) ? 1 : 0;

		if ((debug_enable & IGNORE_PARAM_FROM_CONFIG) == 0 &&
			get_config_int(pdata->config,
			"parm_v4l_buffer_margin",
			&config_val) == 0)
			hw->dynamic_buf_num_margin= config_val;
		if (get_config_int(pdata->config, "sidebind_type",
				&config_val) == 0)
			hw->sidebind_type = config_val;

		if (get_config_int(pdata->config, "sidebind_channel_id",
				&config_val) == 0)
			hw->sidebind_channel_id = config_val;
	}
	hw->platform_dev = pdev;
	hw->chunk_header_offset = 0;
	hw->chunk_res_size = 0;
	hw->first_field_timestamp = 0;
	hw->first_field_timestamp_valid = false;

	if (hw->force_prog_only) {
		pdata->prog_only = 1;
		debug_print(DECODE_ID(hw), 0,
			"forced progressive output\n");
	}

	hw->tvp_flag = vdec_secure(pdata) ? CODEC_MM_FLAGS_TVP : 0;
	if (pdata->sys_info)
		hw->vmpeg12_amstream_dec_info = *pdata->sys_info;

	debug_print(DECODE_ID(hw), 0,
		"%s, sysinfo: %dx%d, tvp_flag = 0x%x\n",
		__func__,
		hw->vmpeg12_amstream_dec_info.width,
		hw->vmpeg12_amstream_dec_info.height,
		hw->tvp_flag);

	if (vmpeg12_init(hw) < 0) {
		pr_info("ammvdec_mpeg12 init failed.\n");
		if (hw) {
			vfree(hw);
			hw = NULL;
		}
		pdata->dec_status = NULL;
		return -ENODEV;
	}
	vdec_set_prepare_level(pdata, start_decode_buf_level);

	vdec_set_vframe_comm(pdata, DRIVER_NAME);

	if (pdata->parallel_dec == 1)
		vdec_core_request(pdata, CORE_MASK_VDEC_1);
	else {
		vdec_core_request(pdata, CORE_MASK_VDEC_1 | CORE_MASK_HEVC
					| CORE_MASK_COMBINE);
	}
#ifdef DUMP_USER_DATA
	amvdec_mmpeg12_init_userdata_dump(hw);
	reset_user_data_buf(hw);
#endif

	return 0;
}

static int ammvdec_mpeg12_remove(struct platform_device *pdev)

{
	struct vdec_mpeg12_hw_s *hw =
		(struct vdec_mpeg12_hw_s *)
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
	cancel_work_sync(&hw->userdata_push_work);
	cancel_work_sync(&hw->notify_work);
	cancel_work_sync(&hw->work);
	cancel_work_sync(&hw->timeout_work);

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

	if (hw->ccbuf_phyAddress_virt) {
		dma_free_coherent(amports_get_dma_device(),hw->cc_buf_size,
			hw->ccbuf_phyAddress_virt, hw->ccbuf_phyAddress);
		hw->ccbuf_phyAddress_virt = NULL;
		hw->ccbuf_phyAddress = 0;
	}

	if (hw->user_data_buffer != NULL) {
		kfree(hw->user_data_buffer);
		hw->user_data_buffer = NULL;
	}
	vmmpeg2_destroy_userdata_manager(hw);

#ifdef DUMP_USER_DATA
	amvdec_mmpeg12_uninit_userdata_dump(hw);
#endif

	if (hw->fw) {
		vfree(hw->fw);
		hw->fw = NULL;
	}

	vfree(hw);

	pr_info("ammvdec_mpeg12 removed.\n");

	return 0;
}

/****************************************/

static struct platform_driver ammvdec_mpeg12_driver = {
	.probe = ammvdec_mpeg12_probe,
	.remove = ammvdec_mpeg12_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

static struct codec_profile_t ammvdec_mpeg12_profile = {
	.name = "MPEG12-V4L",
	.profile = ""
};

static struct mconfig mmpeg12_configs[] = {
	MC_PU32("radr", &radr),
	MC_PU32("rval", &rval),
	MC_PU32("dec_control", &dec_control),
	MC_PU32("error_frame_skip_level", &error_frame_skip_level),
	MC_PU32("decode_timeout_val", &decode_timeout_val),
	MC_PU32("start_decode_buf_level", &start_decode_buf_level),
	MC_PU32("pre_decode_buf_level", &pre_decode_buf_level),
	MC_PU32("debug_enable", &debug_enable),
	MC_PU32("udebug_flag", &udebug_flag),
	MC_PU32("without_display_mode", &without_display_mode),
	MC_PU32("dynamic_buf_num_margin", &dynamic_buf_num_margin),
#ifdef AGAIN_HAS_THRESHOLD
	MC_PU32("again_threshold", &again_threshold),
#endif
};
static struct mconfig_node mmpeg12_node;

static int __init ammvdec_mpeg12_driver_init_module(void)
{
	pr_info("ammvdec_mpeg12 module init\n");

	if (platform_driver_register(&ammvdec_mpeg12_driver)) {
		pr_info("failed to register ammvdec_mpeg12 driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&ammvdec_mpeg12_profile);
	INIT_REG_NODE_CONFIGS("media.decoder", &mmpeg12_node,
		"mmpeg12-v4l", mmpeg12_configs, CONFIG_FOR_RW);
	vcodec_feature_register(VFORMAT_MPEG12, 1);
	return 0;
}

static void __exit ammvdec_mpeg12_driver_remove_module(void)
{
	pr_info("ammvdec_mpeg12 module exit.\n");
	platform_driver_unregister(&ammvdec_mpeg12_driver);
}

/****************************************/
module_param(dec_control, uint, 0664);
MODULE_PARM_DESC(dec_control, "\n ammvdec_mpeg12 decoder control\n");
module_param(error_frame_skip_level, uint, 0664);
MODULE_PARM_DESC(error_frame_skip_level,
				 "\n ammvdec_mpeg12 error_frame_skip_level\n");

module_param(radr, uint, 0664);
MODULE_PARM_DESC(radr, "\nradr\n");

module_param(rval, uint, 0664);
MODULE_PARM_DESC(rval, "\nrval\n");

module_param(debug_enable, uint, 0664);
MODULE_PARM_DESC(debug_enable,
					 "\n ammvdec_mpeg12 debug enable\n");
module_param(pre_decode_buf_level, int, 0664);
MODULE_PARM_DESC(pre_decode_buf_level,
		"\n ammvdec_mpeg12 pre_decode_buf_level\n");

module_param(start_decode_buf_level, int, 0664);
MODULE_PARM_DESC(start_decode_buf_level,
		"\n ammvdec_mpeg12 start_decode_buf_level\n");

module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val, "\n ammvdec_mpeg12 decode_timeout_val\n");

module_param(dynamic_buf_num_margin, uint, 0664);
MODULE_PARM_DESC(dynamic_buf_num_margin, "\n ammvdec_mpeg12 dynamic_buf_num_margin\n");

module_param_array(max_process_time, uint, &max_decode_instance_num, 0664);

module_param(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n ammvdec_mpeg12 udebug_flag\n");

module_param(dirty_again_threshold, int, 0664);
MODULE_PARM_DESC(dirty_again_threshold, "\n ammvdec_mpeg12 dirty_again_threshold\n");


#ifdef AGAIN_HAS_THRESHOLD
module_param(again_threshold, uint, 0664);
MODULE_PARM_DESC(again_threshold, "\n again_threshold\n");
#endif

module_param(without_display_mode, uint, 0664);
MODULE_PARM_DESC(without_display_mode, "\n ammvdec_mpeg12 without_display_mode\n");

module_param(error_proc_policy, uint, 0664);
MODULE_PARM_DESC(error_proc_policy, "\n ammvdec_mpeg12 error_proc_policy\n");

module_init(ammvdec_mpeg12_driver_init_module);
module_exit(ammvdec_mpeg12_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC MULTI MPEG1/2 Video Decoder Driver");
MODULE_LICENSE("GPL");


