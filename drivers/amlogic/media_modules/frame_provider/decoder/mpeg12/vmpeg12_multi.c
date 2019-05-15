/*
 * drivers/amlogic/amports/vmpeg12.c
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
#include <linux/random.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/tee.h>

#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/registers/register.h>
#include "../../../stream_input/amports/amports_priv.h"

#include "../utils/vdec_input.h"
#include "../utils/vdec.h"
#include "../utils/amvdec.h"
#include "../utils/decoder_mmu_box.h"
#include "../utils/decoder_bmmu_box.h"
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include "../utils/firmware.h"


#define MEM_NAME "codec_mmpeg12"
#define CHECK_INTERVAL        (HZ/100)

#define DRIVER_NAME "ammvdec_mpeg12"
#define MODULE_NAME "ammvdec_mpeg12"
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

#define PICINFO_ERROR       0x80000000
#define PICINFO_TYPE_MASK   0x00030000
#define PICINFO_TYPE_I      0x00000000
#define PICINFO_TYPE_P      0x00010000
#define PICINFO_TYPE_B      0x00020000

#define GET_SLICE_TYPE(type)  ("IPB###"[(type&PICINFO_TYPE_MASK)>>16])
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

#define VF_POOL_SIZE        32
#define DECODE_BUFFER_NUM_MAX 8
#define PUT_INTERVAL        (HZ/100)
#define WORKSPACE_SIZE		(4*SZ_64K) /*swap&ccbuf&matirx&MV*/
#define CTX_LMEM_SWAP_OFFSET    0
#define CTX_CCBUF_OFFSET        0x800
#define CTX_QUANT_MATRIX_OFFSET (CTX_CCBUF_OFFSET + 5*1024)
#define CTX_CO_MV_OFFSET        (CTX_QUANT_MATRIX_OFFSET + 1*1024)
#define CTX_DECBUF_OFFSET       (CTX_CO_MV_OFFSET + 0x11000)

#define MAX_BMMU_BUFFER_NUM (DECODE_BUFFER_NUM_MAX + 1)
#define DEFAULT_MEM_SIZE	(32*SZ_1M)
static u32 buf_size = 32 * 1024 * 1024;
static int pre_decode_buf_level = 0x800;
static u32 dec_control;
static u32 error_frame_skip_level;
static u32 stat;
static u32 udebug_flag;
static unsigned int radr;
static unsigned int rval;

#define VMPEG12_DEV_NUM        9
static unsigned int max_decode_instance_num = VMPEG12_DEV_NUM;
static unsigned int max_process_time[VMPEG12_DEV_NUM];
static unsigned int decode_timeout_val = 100;
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

/*
#define DUMP_USER_DATA
*/

enum {
	FRAME_REPEAT_TOP,
	FRAME_REPEAT_BOT,
	FRAME_REPEAT_NONE
};
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

static struct vframe_s *vmpeg_vf_peek(void *);
static struct vframe_s *vmpeg_vf_get(void *);
static void vmpeg_vf_put(struct vframe_s *, void *);
static int vmpeg_vf_states(struct vframe_states *states, void *);
static int vmpeg_event_cb(int type, void *data, void *private_data);

struct mmpeg2_userdata_record_t {
	struct userdata_meta_info_t meta_info;
	u32 rec_start;
	u32 rec_len;
};

#define USERDATA_FIFO_NUM    256
#define MAX_FREE_USERDATA_NODES		5

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

struct vdec_mpeg12_hw_s {
	spinlock_t lock;
	struct platform_device *platform_dev;
	DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
	struct vframe_s vfpool[VF_POOL_SIZE];
	s32 vfbuf_use[DECODE_BUFFER_NUM_MAX];
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
	u32 frame_num;
	struct timer_list check_timer;
	u32 decode_timeout_count;
	u32 start_process_time;
	u32 last_vld_level;
	u32 eos;
	u32 buffer_info[DECODE_BUFFER_NUM_MAX];
	u32 pts[DECODE_BUFFER_NUM_MAX];
	u64 pts64[DECODE_BUFFER_NUM_MAX];
	bool pts_valid[DECODE_BUFFER_NUM_MAX];
	u32 canvas_spec[DECODE_BUFFER_NUM_MAX];
	struct canvas_config_s canvas_config[DECODE_BUFFER_NUM_MAX][2];
	struct dec_sysinfo vmpeg12_amstream_dec_info;

	s32 refs[2];
	int dec_result;
	struct work_struct work;
	struct work_struct notify_work;
	void (*vdec_cb)(struct vdec_s *, void *);
	void *vdec_cb_arg;
	unsigned long ccbuf_phyAddress;
	void *ccbuf_phyAddress_virt;
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
	u32 put_num;
	u32 peek_num;
	u32 get_num;
	u32 drop_frame_count;
	u32 buffer_not_ready;
	int frameinfo_enable;
	struct firmware_s *fw;

	struct work_struct userdata_push_work;
	struct mutex userdata_mutex;
	struct mmpeg2_userdata_info_t userdata_info;
	struct mmpeg2_userdata_record_t ud_record[MAX_UD_RECORDS];
	int cur_ud_idx;
	u8 *user_data_buffer;
	int wait_for_udr_send;
	u32 ucode_cc_last_wp;

#ifdef DUMP_USER_DATA
#define MAX_USER_DATA_SIZE		1572864
	void *user_data_dump_buf;
	unsigned char *pdump_buf_cur_start;
	int total_len;
	int bskip;
	int n_userdata_id;
	u32 reference[MAX_UD_RECORDS];
#endif
};
static void vmpeg12_local_init(struct vdec_mpeg12_hw_s *hw);
static int vmpeg12_hw_ctx_restore(struct vdec_mpeg12_hw_s *hw);
static void reset_process_time(struct vdec_mpeg12_hw_s *hw);
static struct vdec_info gvs;
static int debug_enable;
/*static struct work_struct userdata_push_work;*/
#undef pr_info
#define pr_info printk
unsigned int mpeg12_debug_mask = 0xff;
/*static int counter_max = 5;*/

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


int debug_print(int index, int debug_flag, const char *fmt, ...)
{
	if (((debug_enable & debug_flag) &&
		((1 << index) & mpeg12_debug_mask))
		|| (debug_flag == PRINT_FLAG_ERROR)) {
		unsigned char buf[512];
		int len = 0;
		va_list args;
		va_start(args, fmt);
		len = sprintf(buf, "%d: ", index);
		vsnprintf(buf + len, 512-len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}


/*static bool is_reset;*/
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


static u32 find_buffer(struct vdec_mpeg12_hw_s *hw)
{
	u32 i;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		if (hw->vfbuf_use[i] == 0)
			return i;
	}

	return DECODE_BUFFER_NUM_MAX;
}

static u32 spec_to_index(struct vdec_mpeg12_hw_s *hw, u32 spec)
{
	u32 i;

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		if (hw->canvas_spec[i] == spec)
			return i;
	}

	return DECODE_BUFFER_NUM_MAX;
}

static void set_frame_info(struct vdec_mpeg12_hw_s *hw, struct vframe_s *vf)
{
	unsigned ar_bits;
	u32 temp;
	u32 buffer_index = vf->index;
#ifdef CONFIG_AM_VDEC_MPEG12_LOG
	bool first = (hw->frame_width == 0) && (hw->frame_height == 0);
#endif

	temp = READ_VREG(MREG_PIC_WIDTH);
	if (temp > 1920 || temp == 0)
		vf->width = hw->frame_width = 1920;
	else
		vf->width = hw->frame_width = temp;

	temp = READ_VREG(MREG_PIC_HEIGHT);
	if (temp > 1088 || temp == 0)
		vf->height = hw->frame_height = 1088;
	else
		vf->height = hw->frame_height = temp;

	if (hw->frame_dur > 0)
		vf->duration = hw->frame_dur;
	else {
		vf->duration = hw->frame_dur =
		frame_rate_tab[(READ_VREG(MREG_SEQ_INFO) >> 4) & 0xf];
		schedule_work(&hw->notify_work);
	}

	ar_bits = READ_VREG(MREG_SEQ_INFO) & 0xf;

	if (ar_bits == 0x2)
		vf->ratio_control = 0xc0 << DISP_RATIO_ASPECT_RATIO_BIT;

	else if (ar_bits == 0x3)
		vf->ratio_control = 0x90 << DISP_RATIO_ASPECT_RATIO_BIT;

	else if (ar_bits == 0x4)
		vf->ratio_control = 0x74 << DISP_RATIO_ASPECT_RATIO_BIT;
	else
		vf->ratio_control = 0;

	vf->canvas0Addr = vf->canvas1Addr = -1;
	vf->plane_num = 2;

	vf->canvas0_config[0] = hw->canvas_config[buffer_index][0];
	vf->canvas0_config[1] = hw->canvas_config[buffer_index][1];

	vf->canvas1_config[0] = hw->canvas_config[buffer_index][0];
	vf->canvas1_config[1] = hw->canvas_config[buffer_index][1];


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

static inline void vmpeg12_save_hw_context(struct vdec_mpeg12_hw_s *hw)
{
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
}

static void vmmpeg2_reset_udr_mgr(struct vdec_mpeg12_hw_s *hw)
{
	hw->wait_for_udr_send = 0;
	hw->cur_ud_idx = 0;
	memset(&hw->ud_record, 0, sizeof(hw->ud_record));
}

static void vmmpeg2_crate_userdata_manager(
						struct vdec_mpeg12_hw_s *hw,
						u8 *userdata_buf,
						int buf_len)
{
	if (hw) {
		mutex_init(&hw->userdata_mutex);

		memset(&hw->userdata_info, 0,
			sizeof(struct mmpeg2_userdata_info_t));
		hw->userdata_info.data_buf = userdata_buf;
		hw->userdata_info.buf_len = buf_len;
		hw->userdata_info.data_buf_end = userdata_buf + buf_len;

		vmmpeg2_reset_udr_mgr(hw);
	}
}

static void vmmpeg2_destroy_userdata_manager(struct vdec_mpeg12_hw_s *hw)
{
	if (hw)
		memset(&hw->userdata_info,
				0,
				sizeof(struct mmpeg2_userdata_info_t));
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
				rec_id,	len, flag,
				duration, vpts, vpts_valid,
				reference);
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
/*
			pr_info("notify: rec_start:%d, rec_len:%d, wi:%d, reference:%d\n",
				p_userdata_rec->rec_start,
				p_userdata_rec->rec_len,
				hw->userdata_info.write_index,
				hw->reference[i]);
*/
			hw->userdata_info.write_index++;
			if (hw->userdata_info.write_index >= USERDATA_FIFO_NUM)
				hw->userdata_info.write_index = 0;

			mutex_unlock(&hw->userdata_mutex);


			vdec_wakeup_userdata_poll(hw_to_vdec(hw));
		}
		hw->wait_for_udr_send = 0;
		hw->cur_ud_idx = 0;
	}
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

/*
	pr_info("ri = %d, wi = %d\n",
		hw->userdata_info.read_index,
		hw->userdata_info.write_index);
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
	pr_info("ri:%d, wi:%d, rec_len:%d, rec_start:%d, buf_len:%d\n",
		rec_ri, rec_wi,
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

static void vmmpeg2_reset_userdata_fifo(struct vdec_s *vdec, int bInit)
{
	struct vdec_mpeg12_hw_s *hw = NULL;

	hw = (struct vdec_mpeg12_hw_s *)vdec->private;

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

static void vmmpeg2_wakeup_userdata_poll(struct vdec_s *vdec)
{
	amstream_wakeup_userdata_poll(vdec);
}

/*
#define PRINT_HEAD_INFO
*/
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
	meta_info.flags = ((reg >> 30) << 1);
	meta_info.flags |= (VFORMAT_MPEG12 << 3);
	/* check  top_field_first flag */
	if ((reg >> 28) & 0x1) {
		meta_info.flags |= (1 << 10);
		meta_info.flags |= (((reg >> 29) & 0x1) << 11);
	}

	cur_wp = reg & 0x7fff;
	if (cur_wp == hw->ucode_cc_last_wp) {
		debug_print(DECODE_ID(hw), 0,
			"Null user data package: wp = %d\n", cur_wp);
		WRITE_VREG(AV_SCRATCH_J, 0);
		return;
	}

	if (hw->cur_ud_idx >= MAX_UD_RECORDS) {
		debug_print(DECODE_ID(hw), 0,
			"UD Records over: %d, skip it\n", MAX_UD_RECORDS);
		WRITE_VREG(AV_SCRATCH_J, 0);
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
			/* pr_info("I type, pos:%d\n",
					(meta_info.flags>>1)&0x3); */
			meta_info.flags |= (1<<7);
#ifdef PRINT_HEAD_INFO
			ptype_str = " I";
#endif
			break;
	case 2:
			/* pr_info("P type, pos:%d\n",
					(meta_info.flags>>1)&0x3); */
			meta_info.flags |= (2<<7);
#ifdef PRINT_HEAD_INFO
			ptype_str = " P";
#endif
			break;
	case 3:
			/* pr_info("B type, pos:%d\n",
					(meta_info.flags>>1)&0x3); */
			meta_info.flags |= (3<<7);
#ifdef PRINT_HEAD_INFO
			ptype_str = " B";
#endif
			break;
	case 4:
			/* pr_info("D type, pos:%d\n",
					(meta_info.flags>>1)&0x3); */
			meta_info.flags |= (4<<7);
#ifdef PRINT_HEAD_INFO
			ptype_str = " D";
#endif
			break;
	default:
			/* pr_info("Unknown type:0x%x, pos:%d\n",
					pheader->picture_coding_type,
					(meta_info.flags>>1)&0x3); */
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
	for (i = 0; i < data_length; i++) {
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
		hw->userdata_info.last_wp -= USER_DATA_SIZE;

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


static irqreturn_t vmpeg12_isr_thread_fn(struct vdec_s *vdec, int irq)
{
	u32 reg, info, seqinfo, offset, pts, pts_valid = 0;
	struct vframe_s *vf = NULL;
	u32 index;
	u64 pts_us64 = 0;
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

	reg = READ_VREG(AV_SCRATCH_J);
	if (reg & (1<<16)) {
		vdec_schedule_work(&hw->userdata_push_work);
		return IRQ_HANDLED;
	}

	reg = READ_VREG(MREG_BUFFEROUT);

	if (reg == 2) {
		/*timeout when decoding next frame*/

		debug_print(DECODE_ID(hw), PRINT_FLAG_VLD_DETAIL,
		"ammvdec_mpeg12: Insufficient data\n");
		debug_print(DECODE_ID(hw), PRINT_FLAG_VLD_DETAIL,
		"level=%x, vtl=%x,bcnt=%d\n",
		READ_VREG(VLD_MEM_VIFIFO_LEVEL),
		READ_VREG(VLD_MEM_VIFIFO_CONTROL),
		READ_VREG(VIFF_BIT_CNT));
		if (input_frame_based(vdec))
			vmpeg12_save_hw_context(hw);
		else {
			hw->dec_result = DEC_RESULT_AGAIN;
			vdec_schedule_work(&hw->work);
		}
		return IRQ_HANDLED;
	} else {
		reset_process_time(hw);
		info = READ_VREG(MREG_PIC_INFO);
		offset = READ_VREG(MREG_FRAME_OFFSET);
		index = spec_to_index(hw, READ_VREG(REC_CANVAS_ADDR));
		seqinfo = READ_VREG(MREG_SEQ_INFO);
		if ((info & PICINFO_PROG) == 0 &&
		(info & FRAME_PICTURE_MASK) != FRAME_PICTURE)
			hw->first_i_frame_ready = 1; /* for field struct case*/
		debug_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
			"struct: %d %x\n", (info & PICINFO_FRAME), info);
		if (index >= DECODE_BUFFER_NUM_MAX) {

			debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
			"invalid buffer index,index=%d\n",
				index);
			hw->dec_result = DEC_RESULT_ERROR;
			vdec_schedule_work(&hw->work);
			return IRQ_HANDLED;
		}

		hw->dec_result = DEC_RESULT_DONE;

		/*debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
		"ammvdec_mpeg12: error = 0x%x, offset = 0x%x\n",
			info & PICINFO_ERROR, offset);*/

		if (((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I)) {
			if (hw->chunk) {
				hw->pts_valid[index] = hw->chunk->pts_valid;
				hw->pts[index] = hw->chunk->pts;
				hw->pts64[index] = hw->chunk->pts64;

		debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
		"!!!cpts=%d,pts64=%lld,size=%d,offset=%d\n",
			hw->pts[index], hw->pts64[index],
			hw->chunk->size, hw->chunk->offset);
		} else {
				if (pts_lookup_offset_us64(PTS_TYPE_VIDEO,
				offset, &pts, 0, &pts_us64) == 0) {
					hw->pts_valid[index] = true;
					hw->pts[index] = pts;
					hw->pts64[index] = pts_us64;
				} else {
					hw->pts_valid[index] = false;
				}
			}
		} else {
			hw->pts_valid[index] = false;
		}
		/*if (frame_prog == 0) */
		{
			hw->frame_prog = info & PICINFO_PROG;
			if ((seqinfo & SEQINFO_EXT_AVAILABLE)
				&& (!(seqinfo & SEQINFO_PROG)))
				hw->frame_prog = 0;
		}
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

		hw->buffer_info[index] = info;

		debug_print(DECODE_ID(hw), PRINT_FLAG_DEC_DETAIL,
		"ammvdec_mpeg12: decoded buffer %d, frame_type %c\n",
		index, GET_SLICE_TYPE(info));

		/* Buffer management
		todo: add sequence-end flush */
		if (((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) ||
			((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_P)) {
			hw->vfbuf_use[index]++;
			if (hw->refs[1] == -1) {
				hw->refs[1] = index;
				index = DECODE_BUFFER_NUM_MAX;
			} else if (hw->refs[0] == -1) {
				hw->refs[0] = hw->refs[1];
				hw->refs[1] = index;
				index = hw->refs[0];
			} else {
				hw->vfbuf_use[hw->refs[0]]--;
				hw->refs[0] = hw->refs[1];
				hw->refs[1] = index;
				index = hw->refs[0];
			}
		} else {
		/* if this is a B frame, then drop
		(depending on if there are two reference frames)
		or display immediately*/
			if (hw->refs[0] == -1)
				index = DECODE_BUFFER_NUM_MAX;

		}

		vmpeg12_save_hw_context(hw);

		if (index >= DECODE_BUFFER_NUM_MAX) {
			debug_print(DECODE_ID(hw), 0,
				"invalid buffer index,index=%d\n", index);
			hw->dec_result = DEC_RESULT_ERROR;
			vdec_schedule_work(&hw->work);
			return IRQ_HANDLED;
		}

		info = hw->buffer_info[index];
		pts_valid = hw->pts_valid[index];
		pts = hw->pts[index];
		pts_us64 = hw->pts64[index];

		user_data_ready_notify(hw, pts, pts_valid);

		if ((hw->first_i_frame_ready == 0) &&
			((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) &&
			((info & PICINFO_ERROR) == 0))
			hw->first_i_frame_ready = 1;
		    debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
					"ammvdec_mpeg12: display buffer %d, frame_type %c\n",
					index, GET_SLICE_TYPE(info));
		if (hw->frame_prog & PICINFO_PROG) {

			seqinfo = READ_VREG(MREG_SEQ_INFO);

			if (kfifo_get(&hw->newframe_q, &vf) == 0) {
				debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
				"fatal error, no available buffer slot.");
				hw->dec_result = DEC_RESULT_ERROR;
				vdec_schedule_work(&hw->work);

				return IRQ_HANDLED;
			}

			vf->index = index;
			set_frame_info(hw, vf);
			vf->signal_type = hw->reg_signal_type;

#ifdef NV21
			vf->type =
				VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
#else
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
#endif
			if ((hw->seqinfo & SEQINFO_EXT_AVAILABLE)
				&& (hw->seqinfo & SEQINFO_PROG)) {
				if (info & PICINFO_RPT_FIRST) {
					if (info & PICINFO_TOP_FIRST) {
						vf->duration =
							vf->duration * 3;
						/* repeat three times */
					} else {
						vf->duration =
							vf->duration * 2;
						/* repeat two times */
					}
				}
				vf->duration_pulldown = 0;
					/* no pull down */

			} else {
				vf->duration_pulldown =
					(info & PICINFO_RPT_FIRST) ?
						vf->duration >> 1 : 0;
			}

			vf->duration += vf->duration_pulldown;

			vf->orientation = 0;
			vf->pts = (pts_valid) ? pts : 0;
			vf->pts_us64 = (pts_valid) ? pts_us64 : 0;
			vf->type_original = vf->type;
			hw->vfbuf_use[index]++;

			if ((error_skip(hw, info, vf)) ||
				((hw->first_i_frame_ready == 0)
				 && ((PICINFO_TYPE_MASK & info) !=
					 PICINFO_TYPE_I))) {
				hw->drop_frame_count++;
				hw->vfbuf_use[index]--;
				kfifo_put(&hw->newframe_q,
				(const struct vframe_s *)vf);

			} else {
				debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
							"cpts=%d,pts64=%lld\n",
				vf->pts, vf->pts_us64);
				kfifo_put(&hw->display_q,
				(const struct vframe_s *)vf);
				hw->frame_num++;
				vdec->vdec_fps_detec(vdec->id);
				vf_notify_receiver(vdec->vf_provider_name,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
			}
		}
/*interlace temp*/
		else {
			int first_field_type = (info & PICINFO_TOP_FIRST) ?
					VIDTYPE_INTERLACE_TOP :
					VIDTYPE_INTERLACE_BOTTOM;

#ifdef INTERLACE_SEQ_ALWAYS
			/* once an interlaced sequence exist,
			always force interlaced type */
			/* to make DI easy. */
			hw->dec_control |= DEC_CONTROL_FLAG_FORCE_SEQ_INTERLACE;
#endif

			hw->frame_rpt_state = FRAME_REPEAT_NONE;

			if (kfifo_get(&hw->newframe_q, &vf) == 0) {
				debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
				"fatal error, no available buffer slot.");
				vdec_schedule_work(&hw->work);
				return IRQ_HANDLED;
			}

			hw->vfbuf_use[index] += 2;
			vf->signal_type = hw->reg_signal_type;
			vf->index = index;
			set_frame_info(hw, vf);
			vf->type =
				(first_field_type == VIDTYPE_INTERLACE_TOP) ?
				VIDTYPE_INTERLACE_TOP :
				VIDTYPE_INTERLACE_BOTTOM;
#ifdef NV21
			vf->type |= VIDTYPE_VIU_NV21;
#endif
			if (info & PICINFO_RPT_FIRST)
				vf->duration /= 3;
			else
				vf->duration >>= 1;
			vf->duration_pulldown = (info & PICINFO_RPT_FIRST) ?
						vf->duration >> 1 : 0;
			vf->duration += vf->duration_pulldown;
			vf->orientation = 0;
			vf->pts = (pts_valid) ? pts : 0;
			vf->pts_us64 = (pts_valid) ? pts_us64 : 0;

			if ((error_skip(hw, info, vf)) ||
				((hw->first_i_frame_ready == 0)
				 && ((PICINFO_TYPE_MASK & info) !=
					 PICINFO_TYPE_I))) {
				hw->vfbuf_use[index]--;
				hw->drop_frame_count++;
				kfifo_put(&hw->newframe_q,
				(const struct vframe_s *)vf);
			} else {
			debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
			"cpts0=%d,pts64=%lld,dur=%d, index %d , use %d\n",
				vf->pts, vf->pts_us64, vf->duration,
				vf->index, hw->vfbuf_use[index]);
				kfifo_put(&hw->display_q,
				(const struct vframe_s *)vf);
				hw->frame_num++;
				vdec->vdec_fps_detec(vdec->id);
				vf_notify_receiver(vdec->vf_provider_name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
			}

			if (kfifo_get(&hw->newframe_q, &vf) == 0) {
				debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
				"ammvdec_mpeg12: fatal error, no available buffer slot.");

				hw->vfbuf_use[index]--;

				vdec_schedule_work(&hw->work);

				return IRQ_HANDLED;
			}
			vf->signal_type = hw->reg_signal_type;
			vf->index = index;
			set_frame_info(hw, vf);
			vf->type = (first_field_type ==
				VIDTYPE_INTERLACE_TOP) ?
				VIDTYPE_INTERLACE_BOTTOM :
				VIDTYPE_INTERLACE_TOP;
#ifdef NV21
			vf->type |= VIDTYPE_VIU_NV21;
#endif
			if (info & PICINFO_RPT_FIRST)
				vf->duration /= 3;
			else
				vf->duration >>= 1;
			vf->duration_pulldown = (info & PICINFO_RPT_FIRST) ?
				vf->duration >> 1 : 0;
			vf->duration += vf->duration_pulldown;
			vf->orientation = 0;
			vf->pts = 0;
			vf->pts_us64 = 0;
			vf->type_original = vf->type;

			if ((error_skip(hw, info, vf)) ||
				((hw->first_i_frame_ready == 0)
				 && ((PICINFO_TYPE_MASK & info) !=
					PICINFO_TYPE_I))) {
				hw->drop_frame_count++;
				hw->vfbuf_use[index]--;

				kfifo_put(&hw->newframe_q,
				(const struct vframe_s *)vf);
			} else {
				debug_print(DECODE_ID(hw), PRINT_FLAG_TIMEINFO,
				"cpts1=%d,pts64=%lld,dur=%d index %d, used %d\n",
				vf->pts, vf->pts_us64, vf->duration, vf->index,
				hw->vfbuf_use[index]);
				kfifo_put(&hw->display_q,
				(const struct vframe_s *)vf);
				hw->frame_num++;
				vdec->vdec_fps_detec(vdec->id);
				vf_notify_receiver(vdec->vf_provider_name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
			}

			if (info & PICINFO_RPT_FIRST) {
				if (kfifo_get(&hw->newframe_q, &vf) == 0) {
					debug_print(DECODE_ID(hw),
					PRINT_FLAG_ERROR,
					"error, no available buffer slot.");
					return IRQ_HANDLED;
				}
				hw->vfbuf_use[index]++;
				vf->index = index;
				set_frame_info(hw, vf);
				vf->type = (first_field_type ==
						VIDTYPE_INTERLACE_TOP) ?
						VIDTYPE_INTERLACE_TOP :
						VIDTYPE_INTERLACE_BOTTOM;
#ifdef NV21
				vf->type |= VIDTYPE_VIU_NV21;
#endif
				vf->duration /= 3;
				vf->duration_pulldown =
					(info & PICINFO_RPT_FIRST) ?
						vf->duration >> 1 : 0;
				vf->duration += vf->duration_pulldown;
				vf->orientation = 0;

				vf->pts = 0;
				vf->pts_us64 = 0;
				if ((error_skip(hw, info, vf)) ||
					((hw->first_i_frame_ready == 0)
						&& ((PICINFO_TYPE_MASK & info)
							!= PICINFO_TYPE_I))) {
					hw->vfbuf_use[index]--;
					hw->drop_frame_count++;
					kfifo_put(&hw->newframe_q,
					(const struct vframe_s *)vf);
				} else {
					hw->frame_num++;
					debug_print(DECODE_ID(hw),
					PRINT_FLAG_TIMEINFO,
					"cpts2=%d,pts64=%lld,dur=%d index %d, used %d\n",
					vf->pts, vf->pts_us64, vf->duration,
					vf->index, hw->vfbuf_use[index]);
					kfifo_put(&hw->display_q,
					(const struct vframe_s *)vf);
					vdec->vdec_fps_detec(vdec->id);
					vf_notify_receiver(
					vdec->vf_provider_name,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
				}
			}
		}
		vdec_schedule_work(&hw->work);

		debug_print(DECODE_ID(hw), PRINT_FRAME_NUM,
		"frame_num=%d\n", hw->frame_num);
		if (hw->frame_num == 1)
			debug_print(DECODE_ID(hw), PRINT_FRAME_NUM,
		"frame_num==1\n");
		if (hw->frame_num == 1000)
			debug_print(DECODE_ID(hw), PRINT_FRAME_NUM,
		"frame_num==1000\n");
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

	vdec_count_info(&gvs, info & PICINFO_ERROR, offset);

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	return IRQ_WAKE_THREAD;
}

static void vmpeg12_notify_work(struct work_struct *work)
{
	struct vdec_mpeg12_hw_s *hw = container_of(work,
					struct vdec_mpeg12_hw_s, notify_work);
	struct vdec_s *vdec = hw_to_vdec(hw);

	if (vdec->fr_hint_state == VDEC_NEED_HINT) {
		vf_notify_receiver(vdec->vf_provider_name,
				VFRAME_EVENT_PROVIDER_FR_HINT,
				(void *)((unsigned long)hw->frame_dur));
		vdec->fr_hint_state = VDEC_HINTED;
	}
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

static void vmpeg12_work(struct work_struct *work)
{
	struct vdec_mpeg12_hw_s *hw =
	container_of(work, struct vdec_mpeg12_hw_s, work);
	struct vdec_s *vdec = hw_to_vdec(hw);
	if (hw->dec_result != DEC_RESULT_DONE)
		debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
	"ammvdec_mpeg12: vmpeg_work,result=%d,status=%d\n",
	hw->dec_result, hw_to_vdec(hw)->next_status);

	if (hw->dec_result == DEC_RESULT_DONE) {
		if (!hw->ctx_valid)
			hw->ctx_valid = 1;

		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
	} else if (hw->dec_result == DEC_RESULT_AGAIN
	&& (hw_to_vdec(hw)->next_status !=
		VDEC_STATUS_DISCONNECTED)) {
		/*
			stream base: stream buf empty or timeout
			frame base: vdec_prepare_input fail
		*/
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
		debug_print(DECODE_ID(hw), PRINT_FLAG_VLD_DETAIL,
		"%s DEC_RESULT_GET_DATA %x %x %x\n",
		__func__,
		READ_VREG(VLD_MEM_VIFIFO_LEVEL),
		READ_VREG(VLD_MEM_VIFIFO_WP),
		READ_VREG(VLD_MEM_VIFIFO_RP));
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
		vdec_clean_input(hw_to_vdec(hw));
		return;
	} else if (hw->dec_result == DEC_RESULT_FORCE_EXIT) {
		debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
		"%s: force exit\n", __func__);
		if (hw->stat & STAT_ISR_REG) {
			amvdec_stop();
			/*disable mbox interrupt */
			WRITE_VREG(ASSIST_MBOX1_MASK, 0);
			vdec_free_irq(VDEC_IRQ_1, (void *)hw);
			hw->stat &= ~STAT_ISR_REG;
		}
	} else if (hw->dec_result == DEC_RESULT_EOS) {
		pr_info("%s: end of stream\n", __func__);
		if (hw->stat & STAT_VDEC_RUN) {
			amvdec_stop();
			hw->stat &= ~STAT_VDEC_RUN;
		}
		hw->eos = 1;
		vdec_vframe_dirty(hw_to_vdec(hw), hw->chunk);
		vdec_clean_input(hw_to_vdec(hw));
	}
	if (hw->stat & STAT_VDEC_RUN) {
		amvdec_stop();
		hw->stat &= ~STAT_VDEC_RUN;
	}
	wait_vmmpeg12_search_done(hw);
	if (vdec->parallel_dec == 1)
		vdec_core_finish_run(hw_to_vdec(hw), CORE_MASK_VDEC_1);
	else
		vdec_core_finish_run(hw_to_vdec(hw), CORE_MASK_VDEC_1 | CORE_MASK_HEVC);
	del_timer_sync(&hw->check_timer);
	hw->stat &= ~STAT_TIMER_ARM;

	if (hw->vdec_cb) {
		hw->vdec_cb(hw_to_vdec(hw), hw->vdec_cb_arg);
		debug_print(DECODE_ID(hw), 0x80000,
		"%s:\n", __func__);
	}
}

static struct vframe_s *vmpeg_vf_peek(void *op_arg)
{
	struct vframe_s *vf;
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg12_hw_s *hw =
	(struct vdec_mpeg12_hw_s *)vdec->private;
	hw->peek_num++;
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

	hw->get_num++;
	if (kfifo_get(&hw->display_q, &vf))
		return vf;

	return NULL;
}

static void vmpeg_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vdec_s *vdec = op_arg;
	struct vdec_mpeg12_hw_s *hw =
	(struct vdec_mpeg12_hw_s *)vdec->private;

	hw->vfbuf_use[vf->index]--;
	hw->put_num++;
	kfifo_put(&hw->newframe_q, (const struct vframe_s *)vf);
}

static int vmpeg_event_cb(int type, void *data, void *private_data)
{
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
		vstatus->frame_rate = 96000 / hw->frame_dur;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = READ_VREG(AV_SCRATCH_C);
	vstatus->status = hw->stat;
	vstatus->bit_rate = gvs.bit_rate;
	vstatus->frame_dur = hw->frame_dur;
	vstatus->frame_data = gvs.frame_data;
	vstatus->total_data = gvs.total_data;
	vstatus->frame_count = gvs.frame_count;
	vstatus->error_frame_count = gvs.error_frame_count;
	vstatus->drop_frame_count = hw->drop_frame_count;
	vstatus->total_data = gvs.total_data;
	vstatus->samp_cnt = gvs.samp_cnt;
	vstatus->offset = gvs.offset;
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
			"%s", DRIVER_NAME);

	return 0;
}



/****************************************/
static void vmpeg12_canvas_init(struct vdec_mpeg12_hw_s *hw)
{
	int i, ret;
	u32 canvas_width, canvas_height;
	u32 decbuf_size, decbuf_y_size, decbuf_uv_size;
	unsigned long decbuf_start;
	/*u32 disp_addr = 0xffffffff;*/
	struct vdec_s *vdec = hw_to_vdec(hw);

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

	for (i = 0; i < MAX_BMMU_BUFFER_NUM; i++) {

		unsigned canvas;
		if (i == (MAX_BMMU_BUFFER_NUM - 1)) /* SWAP&CCBUF&MATIRX&MV */
			decbuf_size = WORKSPACE_SIZE;
		ret = decoder_bmmu_box_alloc_buf_phy(hw->mm_blk_handle, i,
				decbuf_size, DRIVER_NAME, &decbuf_start);
		if (ret < 0) {
			pr_err("mmu alloc failed! size 0x%d  idx %d\n",
				decbuf_size, i);
			return;
		}

		if (i == (MAX_BMMU_BUFFER_NUM - 1)) {
			if (hw->ccbuf_phyAddress_is_remaped_nocache)
				codec_mm_unmap_phyaddr(hw->ccbuf_phyAddress_virt);
			hw->ccbuf_phyAddress_virt = NULL;
			hw->ccbuf_phyAddress = 0;
			hw->ccbuf_phyAddress_is_remaped_nocache = 0;

			hw->buf_start = decbuf_start;
			hw->ccbuf_phyAddress = hw->buf_start + CTX_CCBUF_OFFSET;
			hw->ccbuf_phyAddress_virt
				= codec_mm_phys_to_virt(
					hw->ccbuf_phyAddress);
			if (!hw->ccbuf_phyAddress_virt) {
				hw->ccbuf_phyAddress_virt
					= codec_mm_vmap(
						hw->ccbuf_phyAddress,
						CCBUF_SIZE);
				hw->ccbuf_phyAddress_is_remaped_nocache = 1;
			}

			WRITE_VREG(MREG_CO_MV_START, hw->buf_start);
		} else {
			if (vdec->parallel_dec == 1) {
				unsigned tmp;
				if (canvas_u(hw->canvas_spec[i]) == 0xff) {
					tmp =
						vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
					hw->canvas_spec[i] &= ~(0xffff << 8);
					hw->canvas_spec[i] |= tmp << 8;
					hw->canvas_spec[i] |= tmp << 16;
				}
				if (canvas_y(hw->canvas_spec[i]) == 0xff) {
					tmp =
						vdec->get_canvas_ex(CORE_MASK_VDEC_1, vdec->id);
					hw->canvas_spec[i] &= ~0xff;
					hw->canvas_spec[i] |= tmp;
				}
				canvas = hw->canvas_spec[i];
			} else {
				canvas = vdec->get_canvas(i, 2);
				hw->canvas_spec[i] = canvas;
			}

			hw->canvas_config[i][0].phy_addr =
			decbuf_start;
			hw->canvas_config[i][0].width =
			canvas_width;
			hw->canvas_config[i][0].height =
			canvas_height;
			hw->canvas_config[i][0].block_mode =
			CANVAS_BLKMODE_32X32;

			canvas_config_config(canvas_y(canvas),
			&hw->canvas_config[i][0]);

			hw->canvas_config[i][1].phy_addr =
			decbuf_start + decbuf_y_size;
			hw->canvas_config[i][1].width =
			canvas_width;
			hw->canvas_config[i][1].height =
			canvas_height / 2;
			hw->canvas_config[i][1].block_mode =
			CANVAS_BLKMODE_32X32;

			canvas_config_config(canvas_u(canvas),
			&hw->canvas_config[i][1]);
		}
	}
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
		"width/height (%d/%d),i_first %d\n",
		hw->frame_width,
		hw->frame_height,
		hw->first_i_frame_ready
		);
	debug_print(DECODE_ID(hw), 0,
		"is_framebase(%d), eos %d, state 0x%x, dec_result 0x%x dec_frm %d put_frm %d run %d not_run_ready %d,input_empty %d\n",
		input_frame_based(vdec),
		hw->eos,
		hw->stat,
		hw->dec_result,
		hw->frame_num,
		hw->put_num,
		hw->run_count,
		hw->not_run_ready,
		hw->input_empty
		);

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++) {
		debug_print(DECODE_ID(hw), 0,
			"index %d, used %d\n", i, hw->vfbuf_use[i]);
	}

	if (vf_get_receiver(vdec->vf_provider_name)) {
		enum receviver_start_e state =
		vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_QUREY_STATE,
			NULL);
		debug_print(DECODE_ID(hw), 0,
			"\nreceiver(%s) state %d\n",
			vdec->vf_provider_name,
			state);
	}
	debug_print(DECODE_ID(hw), 0,
	"%s, newq(%d/%d), dispq(%d/%d) vf peek/get/put (%d/%d/%d),drop=%d, buffer_not_ready %d\n",
	__func__,
	kfifo_len(&hw->newframe_q),
	VF_POOL_SIZE,
	kfifo_len(&hw->display_q),
	VF_POOL_SIZE,
	hw->peek_num,
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
		READ_PARSER_REG(PARSER_VIDEO_RP));
	debug_print(DECODE_ID(hw), 0,
		"PARSER_VIDEO_WP=0x%x\n",
		READ_PARSER_REG(PARSER_VIDEO_WP));
	if (input_frame_based(vdec) &&
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
					PRINT_FRAMEBASE_DATA,
						"\n");
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
static void start_process_time(struct vdec_mpeg12_hw_s *hw)
{
	hw->decode_timeout_count = 2;
	hw->start_process_time = jiffies;
}
static void timeout_process(struct vdec_mpeg12_hw_s *hw)
{
	struct vdec_s *vdec = hw_to_vdec(hw);
	amvdec_stop();
	debug_print(DECODE_ID(hw), PRINT_FLAG_ERROR,
	"%s decoder timeout, status=%d, level=%d\n",
	__func__, vdec->status, READ_VREG(VLD_MEM_VIFIFO_LEVEL));
	hw->dec_result = DEC_RESULT_DONE;
	reset_process_time(hw);
	hw->first_i_frame_ready = 0;
	vdec_schedule_work(&hw->work);
}

static void check_timer_func(unsigned long arg)
{
	struct vdec_mpeg12_hw_s *hw = (struct vdec_mpeg12_hw_s *)arg;
	struct vdec_s *vdec = hw_to_vdec(hw);
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

	if (debug_enable == 0 &&
		(input_frame_based(vdec) ||
		(READ_VREG(VLD_MEM_VIFIFO_LEVEL) > 0x100)) &&
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

	if (vdec->next_status == VDEC_STATUS_DISCONNECTED) {
		hw->dec_result = DEC_RESULT_FORCE_EXIT;
		vdec_schedule_work(&hw->work);
		pr_info("vdec requested to be disconnected\n");
		return;
	}

	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
}

static int vmpeg12_hw_ctx_restore(struct vdec_mpeg12_hw_s *hw)
{
	u32 index;
	index = find_buffer(hw);
	if (index >= DECODE_BUFFER_NUM_MAX)
		return -1;
	vmpeg12_canvas_init(hw);

	/* prepare REF0 & REF1
	points to the past two IP buffers
	prepare REC_CANVAS_ADDR and ANC2_CANVAS_ADDR
	points to the output buffer*/
	WRITE_VREG(MREG_REF0,
	(hw->refs[1] == -1) ? 0xffffffff : hw->canvas_spec[hw->refs[0]]);
	WRITE_VREG(MREG_REF1,
	(hw->refs[0] == -1) ? 0xffffffff : hw->canvas_spec[hw->refs[1]]);
	WRITE_VREG(REC_CANVAS_ADDR, hw->canvas_spec[index]);
	WRITE_VREG(ANC2_CANVAS_ADDR, hw->canvas_spec[index]);

	debug_print(DECODE_ID(hw), PRINT_FLAG_RESTORE,
	"%s,ref0=0x%x, ref1=0x%x,rec=0x%x, ctx_valid=%d,index=%d\n",
	__func__,
	READ_VREG(MREG_REF0),
	READ_VREG(MREG_REF1),
	READ_VREG(REC_CANVAS_ADDR),
	hw->ctx_valid, index);

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

	/* clear error count */
	WRITE_VREG(MREG_ERROR_COUNT, 0);
	WRITE_VREG(MREG_FATAL_ERROR, 0);
	/* clear wait buffer status */
	WRITE_VREG(MREG_WAIT_BUFFER, 0);
#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1<<17);
#endif

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

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
		hw->vfbuf_use[i] = 0;


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
			CODEC_MM_FLAGS_FOR_VDECODER);
	hw->eos = 0;
	hw->frame_width = hw->frame_height = 0;
	hw->frame_dur = hw->frame_prog = 0;
	hw->frame_force_skip_flag = 0;
	hw->wait_buffer_counter = 0;
	hw->first_i_frame_ready = 0;
	hw->dec_control &= DEC_CONTROL_INTERNAL_MASK;
	hw->refs[0] = -1;
	hw->refs[1] = -1;
	hw->frame_num = 0;
	hw->put_num = 0;
	hw->run_count = 0;
	hw->not_run_ready = 0;
	hw->input_empty = 0;
	hw->peek_num = 0;
	hw->get_num = 0;
	hw->drop_frame_count = 0;
	hw->buffer_not_ready = 0;
	hw->start_process_time = 0;
	hw->error_frame_skip_level = error_frame_skip_level;
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

	amvdec_enable();

	init_timer(&hw->check_timer);
	hw->check_timer.data = (unsigned long)hw;
	hw->check_timer.function = check_timer_func;
	hw->check_timer.expires = jiffies + CHECK_INTERVAL;

	hw->stat |= STAT_TIMER_ARM;
	hw->stat |= STAT_ISR_REG;

	hw->buf_start = 0;
	hw->init_flag = 0;
	WRITE_VREG(DECODE_STOP_POS, udebug_flag);

	return 0;
}

static unsigned long run_ready(struct vdec_s *vdec, unsigned long mask)
{
	int index;

	struct vdec_mpeg12_hw_s *hw =
		(struct vdec_mpeg12_hw_s *)vdec->private;
	if (hw->eos)
		return 0;
	if (vdec_stream_based(vdec) && (hw->init_flag == 0)
		&& pre_decode_buf_level != 0) {
		u32 rp, wp, level;

		rp = READ_PARSER_REG(PARSER_VIDEO_RP);
		wp = READ_PARSER_REG(PARSER_VIDEO_WP);
		if (wp < rp)
			level = vdec->input.size + wp - rp;
		else
			level = wp - rp;

		if (level < pre_decode_buf_level) {
			hw->not_run_ready++;
			return 0;
		}
	}

	index = find_buffer(hw);
	if (index >= DECODE_BUFFER_NUM_MAX) {
		hw->buffer_not_ready++;
		return 0;
	}
	hw->not_run_ready = 0;
	hw->buffer_not_ready = 0;
	if (vdec->parallel_dec == 1)
		return (unsigned long)(CORE_MASK_VDEC_1);
	else
		return (unsigned long)(CORE_MASK_VDEC_1 | CORE_MASK_HEVC);
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

static void run(struct vdec_s *vdec, unsigned long mask,
void (*callback)(struct vdec_s *, void *),
		void *arg)
{
	struct vdec_mpeg12_hw_s *hw =
	(struct vdec_mpeg12_hw_s *)vdec->private;
	int save_reg = READ_VREG(POWER_CTL_VLD);
	int size, ret;
	/* reset everything except DOS_TOP[1] and APB_CBUS[0]*/
	WRITE_VREG(DOS_SW_RESET0, 0xfffffff0);
	WRITE_VREG(DOS_SW_RESET0, 0);
	WRITE_VREG(POWER_CTL_VLD, save_reg);
	hw->run_count++;
	vdec_reset_core(vdec);
	hw->vdec_cb_arg = arg;
	hw->vdec_cb = callback;

	size = vdec_prepare_input(vdec, &hw->chunk);
	if (size < 0) {
		hw->input_empty++;
		hw->dec_result = DEC_RESULT_AGAIN;
		vdec_schedule_work(&hw->work);
		return;
	}
	if (input_frame_based(vdec)) {
		u8 *data = NULL;

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

			for (jj = 0; jj < size; jj++) {
				if ((jj & 0xf) == 0)
					debug_print(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA,
						"%06x:", jj);
				debug_print(DECODE_ID(hw),
				PRINT_FRAMEBASE_DATA,
					"%02x ", data[jj]);
				if (((jj + 1) & 0xf) == 0)
					debug_print(DECODE_ID(hw),
					PRINT_FRAMEBASE_DATA,
						"\n");
			}
		}

		if (!hw->chunk->block->is_mapped)
			codec_mm_unmap_phyaddr(data);
	} else
		debug_print(DECODE_ID(hw), PRINT_FLAG_VDEC_STATUS,
			"%s: %x %x %x %x %x size 0x%x\n",
			__func__,
			READ_VREG(VLD_MEM_VIFIFO_LEVEL),
			READ_VREG(VLD_MEM_VIFIFO_WP),
			READ_VREG(VLD_MEM_VIFIFO_RP),
			READ_PARSER_REG(PARSER_VIDEO_RP),
			READ_PARSER_REG(PARSER_VIDEO_WP),
			size);


	hw->input_empty = 0;
	debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
	"%s,%d, size=%d\n", __func__, __LINE__, size);
	vdec_enable_input(vdec);
	hw->init_flag = 1;

	if (hw->chunk)
		debug_print(DECODE_ID(hw), PRINT_FLAG_RUN_FLOW,
		"input chunk offset %d, size %d\n",
			hw->chunk->offset, hw->chunk->size);

	hw->dec_result = DEC_RESULT_NONE;
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
	/*wmb();*/
	hw->stat |= STAT_MC_LOAD;
	hw->last_vld_level = 0;
	start_process_time(hw);
	amvdec_start();
	hw->stat |= STAT_VDEC_RUN;
	mod_timer(&hw->check_timer, jiffies + CHECK_INTERVAL);
}

static void reset(struct vdec_s *vdec)
{
	pr_info("ammvdec_mpeg12: reset.\n");
}

static int ammvdec_mpeg12_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;
	struct vdec_mpeg12_hw_s *hw = NULL;

	pr_info("ammvdec_mpeg12 probe start.\n");

	if (pdata == NULL) {
		pr_info("ammvdec_mpeg12 platform data undefined.\n");
		return -EFAULT;
	}

	hw = (struct vdec_mpeg12_hw_s *)devm_kzalloc(&pdev->dev,
		sizeof(struct vdec_mpeg12_hw_s), GFP_KERNEL);
	if (hw == NULL) {
		pr_info("\nammvdec_mpeg12 decoder driver alloc failed\n");
		return -ENOMEM;
	}

	pdata->private = hw;
	pdata->dec_status = vmmpeg12_dec_status;
	pdata->run_ready = run_ready;
	pdata->run = run;
	pdata->reset = reset;
	pdata->irq_handler = vmpeg12_isr;
	pdata->threaded_irq_handler = vmpeg12_isr_thread_fn;
	pdata->dump_state = vmpeg2_dump_state;

	pdata->user_data_read = vmmpeg2_user_data_read;
	pdata->reset_userdata_fifo = vmmpeg2_reset_userdata_fifo;
	pdata->wakeup_userdata_poll = vmmpeg2_wakeup_userdata_poll;

	if (pdata->use_vfm_path) {
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			    VFM_DEC_PROVIDER_NAME);
		hw->frameinfo_enable = 1;
	}
	else
		snprintf(pdata->vf_provider_name, VDEC_PROVIDER_NAME_SIZE,
			PROVIDER_NAME ".%02x", pdev->id & 0xff);
	if (pdata->parallel_dec == 1) {
		int i;
		for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
			hw->canvas_spec[i] = 0xffffff;
	}
	vf_provider_init(&pdata->vframe_provider, pdata->vf_provider_name,
		&vf_provider_ops, pdata);

	platform_set_drvdata(pdev, pdata);

	hw->platform_dev = pdev;

	if (pdata->sys_info)
		hw->vmpeg12_amstream_dec_info = *pdata->sys_info;

	if (vmpeg12_init(hw) < 0) {
		pr_info("ammvdec_mpeg12 init failed.\n");
		devm_kfree(&pdev->dev, (void *)hw);
		pdata->dec_status = NULL;
		return -ENODEV;
	}
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

	/*INIT_WORK(&userdata_push_work, userdata_push_do_work);*/
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

	if (hw->ccbuf_phyAddress_is_remaped_nocache)
		codec_mm_unmap_phyaddr(hw->ccbuf_phyAddress_virt);
	hw->ccbuf_phyAddress_virt = NULL;
	hw->ccbuf_phyAddress = 0;
	hw->ccbuf_phyAddress_is_remaped_nocache = 0;

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

	pr_info("ammvdec_mpeg12 removed.\n");
	memset(&gvs, 0x0, sizeof(gvs));

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
	.name = "mmpeg12",
	.profile = ""
};

static struct mconfig mmpeg12_configs[] = {
	MC_PU32("stat", &stat),
	MC_PU32("radr", &radr),
	MC_PU32("rval", &rval),
	MC_PU32("dec_control", &dec_control),
	MC_PU32("error_frame_skip_level", &error_frame_skip_level),
	MC_PU32("decode_timeout_val", &decode_timeout_val),
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
		"mmpeg12", mmpeg12_configs, CONFIG_FOR_RW);
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
module_param(decode_timeout_val, uint, 0664);
MODULE_PARM_DESC(decode_timeout_val, "\n ammvdec_mpeg12 decode_timeout_val\n");

module_param_array(max_process_time, uint, &max_decode_instance_num, 0664);

module_param(udebug_flag, uint, 0664);
MODULE_PARM_DESC(udebug_flag, "\n ammvdec_mpeg12 udebug_flag\n");

module_init(ammvdec_mpeg12_driver_init_module);
module_exit(ammvdec_mpeg12_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC MULTI MPEG1/2 Video Decoder Driver");
MODULE_LICENSE("GPL");


