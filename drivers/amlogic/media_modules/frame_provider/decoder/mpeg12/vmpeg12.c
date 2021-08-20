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
#include <linux/timer.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/module.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include "vmpeg12.h"
#include <linux/amlogic/media/registers/register.h>
#include "../../../stream_input/amports/amports_priv.h"
#include "../utils/decoder_mmu_box.h"
#include "../utils/decoder_bmmu_box.h"
#include <linux/uaccess.h>
#include <linux/amlogic/media/codec_mm/configs.h>
//#include <linux/amlogic/tee.h>
#include <uapi/linux/tee.h>



#ifdef CONFIG_AM_VDEC_MPEG12_LOG
#define AMLOG
#define LOG_LEVEL_VAR       amlog_level_vmpeg
#define LOG_MASK_VAR        amlog_mask_vmpeg
#define LOG_LEVEL_ERROR     0
#define LOG_LEVEL_INFO      1
#define LOG_LEVEL_DESC  "0:ERROR, 1:INFO"
#endif
#include <linux/amlogic/media/utils/amlog.h>
MODULE_AMLOG(LOG_LEVEL_ERROR, 0, LOG_LEVEL_DESC, LOG_DEFAULT_MASK_DESC);

#include "../utils/amvdec.h"
#include "../utils/vdec.h"
#include "../utils/firmware.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"

#define DRIVER_NAME "amvdec_mpeg12"
#define MODULE_NAME "amvdec_mpeg12"

/* protocol registers */
#define MREG_SEQ_INFO       AV_SCRATCH_4
#define MREG_PIC_INFO       AV_SCRATCH_5
#define MREG_PIC_WIDTH      AV_SCRATCH_6
#define MREG_PIC_HEIGHT     AV_SCRATCH_7
#define MREG_BUFFERIN       AV_SCRATCH_8
#define MREG_BUFFEROUT      AV_SCRATCH_9

#define MREG_CMD            AV_SCRATCH_A
#define MREG_CO_MV_START    AV_SCRATCH_B
#define MREG_ERROR_COUNT    AV_SCRATCH_C
#define MREG_FRAME_OFFSET   AV_SCRATCH_D
#define MREG_WAIT_BUFFER    AV_SCRATCH_E
#define MREG_FATAL_ERROR    AV_SCRATCH_F
#define MREG_FORCE_I_RDY    AV_SCRATCH_G

#define PICINFO_ERROR       0x80000000
#define PICINFO_TYPE_MASK   0x00030000
#define PICINFO_TYPE_I      0x00000000
#define PICINFO_TYPE_P      0x00010000
#define PICINFO_TYPE_B      0x00020000

#define PICINFO_PROG        0x8000
#define PICINFO_RPT_FIRST   0x4000
#define PICINFO_TOP_FIRST   0x2000
#define PICINFO_FRAME       0x1000

#define SEQINFO_EXT_AVAILABLE   0x80000000
#define SEQINFO_PROG            0x00010000
#define CCBUF_SIZE      (5*1024)

#define VF_POOL_SIZE        32
#define DECODE_BUFFER_NUM_MAX 8
#define PUT_INTERVAL        (HZ/100)
#define WORKSPACE_SIZE		(2*SZ_64K)
#define MAX_BMMU_BUFFER_NUM (DECODE_BUFFER_NUM_MAX + 1)


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


enum {
	FRAME_REPEAT_TOP,
	FRAME_REPEAT_BOT,
	FRAME_REPEAT_NONE
};

static struct vframe_s *vmpeg_vf_peek(void *);
static struct vframe_s *vmpeg_vf_get(void *);
static void vmpeg_vf_put(struct vframe_s *, void *);
static int vmpeg_vf_states(struct vframe_states *states, void *);
static int vmpeg_event_cb(int type, void *data, void *private_data);

static int vmpeg12_prot_init(void);
static void vmpeg12_local_init(void);

static const char vmpeg12_dec_id[] = "vmpeg12-dev";
#define PROVIDER_NAME   "decoder.mpeg12"
static const struct vframe_operations_s vmpeg_vf_provider = {
	.peek = vmpeg_vf_peek,
	.get = vmpeg_vf_get,
	.put = vmpeg_vf_put,
	.event_cb = vmpeg_event_cb,
	.vf_states = vmpeg_vf_states,
};
static void *mm_blk_handle;
static struct vframe_provider_s vmpeg_vf_prov;
static int tvp_flag;

static DECLARE_KFIFO(newframe_q, struct vframe_s *, VF_POOL_SIZE);
static DECLARE_KFIFO(display_q, struct vframe_s *, VF_POOL_SIZE);
static DECLARE_KFIFO(recycle_q, struct vframe_s *, VF_POOL_SIZE);

static const u32 frame_rate_tab[16] = {
	96000 / 30, 96000000 / 23976, 96000 / 24, 96000 / 25,
	9600000 / 2997, 96000 / 30, 96000 / 50, 9600000 / 5994,
	96000 / 60,
	/* > 8 reserved, use 24 */
	96000 / 24, 96000 / 24, 96000 / 24, 96000 / 24,
	96000 / 24, 96000 / 24, 96000 / 24
};

static struct vframe_s vfpool[VF_POOL_SIZE];
static struct vframe_s vfpool2[VF_POOL_SIZE];
static int cur_pool_idx;
static s32 vfbuf_use[DECODE_BUFFER_NUM_MAX];
static u32 dec_control;
static u32 frame_width, frame_height, frame_dur, frame_prog;
static u32 saved_resolution;
static struct timer_list recycle_timer;
static u32 stat;
static u32 buf_size = 32 * 1024 * 1024;
static u32 ccbuf_phyAddress;
static void *ccbuf_phyAddress_virt;
static int ccbuf_phyAddress_is_remaped_nocache;
static u32 lastpts;
static u32 fr_hint_status;
static u32 last_offset;
static u32 ratio_control;


static DEFINE_SPINLOCK(lock);

static u32 frame_rpt_state;

static struct dec_sysinfo vmpeg12_amstream_dec_info;
static struct vdec_info *gvs;
static struct vdec_s *vdec;

/* for error handling */
static s32 frame_force_skip_flag;
static s32 error_frame_skip_level;
static s32 wait_buffer_counter;
static u32 first_i_frame_ready;
static u32 force_first_i_ready;

static struct work_struct userdata_push_work;
static struct work_struct notify_work;
static struct work_struct reset_work;
static struct work_struct set_clk_work;
static bool is_reset;

static DEFINE_MUTEX(userdata_mutex);

static void vmpeg12_create_userdata_manager(u8 *userdata_buf, int buf_len);

struct mpeg12_userdata_recored_t {
	struct userdata_meta_info_t meta_info;
	u32 rec_start;
	u32 rec_len;
};

#define USERDATA_FIFO_NUM    256

struct mpeg12_userdata_info_t {
	struct mpeg12_userdata_recored_t records[USERDATA_FIFO_NUM];
	u8 *data_buf;
	u8 *data_buf_end;
	u32 buf_len;
	u32 read_index;
	u32 write_index;
	u32 last_wp;
};

static struct mpeg12_userdata_info_t *p_userdata_mgr;


static inline int pool_index(struct vframe_s *vf)
{
	if ((vf >= &vfpool[0]) && (vf <= &vfpool[VF_POOL_SIZE - 1]))
		return 0;
	else if ((vf >= &vfpool2[0]) && (vf <= &vfpool2[VF_POOL_SIZE - 1]))
		return 1;
	else
		return -1;
}

static inline u32 index2canvas(u32 index)
{
	const u32 canvas_tab[8] = {
#ifdef NV21
		0x010100, 0x030302, 0x050504, 0x070706,
		0x090908, 0x0b0b0a, 0x0d0d0c, 0x0f0f0e
#else
		0x020100, 0x050403, 0x080706, 0x0b0a09,
		0x0e0d0c, 0x11100f, 0x141312, 0x171615
#endif
	};

	return canvas_tab[index];
}

static void set_frame_info(struct vframe_s *vf)
{
	unsigned int ar_bits;
	u32 temp;

#ifdef CONFIG_AM_VDEC_MPEG12_LOG
	bool first = (frame_width == 0) && (frame_height == 0);
#endif
	temp = READ_VREG(MREG_PIC_WIDTH);
	if (temp > 1920)
		vf->width = frame_width = 1920;
	else
		vf->width = frame_width = temp;

	temp = READ_VREG(MREG_PIC_HEIGHT);
	if (temp > 1088)
		vf->height = frame_height = 1088;
	else
		vf->height = frame_height = temp;

	vf->flag = 0;

	if (frame_dur > 0)
		vf->duration = frame_dur;
	else {
		int index = (READ_VREG(MREG_SEQ_INFO) >> 4) & 0xf;
		vf->duration = frame_dur = frame_rate_tab[index];
		schedule_work(&notify_work);
	}

	gvs->frame_dur = vf->duration;

	ar_bits = READ_VREG(MREG_SEQ_INFO) & 0xf;

	if (ar_bits == 0x2)
		vf->ratio_control = 0xc0 << DISP_RATIO_ASPECT_RATIO_BIT;

	else if (ar_bits == 0x3)
		vf->ratio_control = 0x90 << DISP_RATIO_ASPECT_RATIO_BIT;

	else if (ar_bits == 0x4)
		vf->ratio_control = 0x74 << DISP_RATIO_ASPECT_RATIO_BIT;

	else
		vf->ratio_control = 0;

	ratio_control = vf->ratio_control;

	amlog_level_if(first, LOG_LEVEL_INFO,
		"mpeg2dec: w(%d), h(%d), dur(%d), dur-ES(%d)\n",
		frame_width, frame_height, frame_dur,
		frame_rate_tab[(READ_VREG(MREG_SEQ_INFO) >> 4) & 0xf]);
}

static bool error_skip(u32 info, struct vframe_s *vf)
{
	if (error_frame_skip_level) {
		/* skip error frame */
		if ((info & PICINFO_ERROR) || (frame_force_skip_flag)) {
			if ((info & PICINFO_ERROR) == 0) {
				if ((info & PICINFO_TYPE_MASK) ==
					PICINFO_TYPE_I)
					frame_force_skip_flag = 0;
			} else {
				if (error_frame_skip_level >= 2)
					frame_force_skip_flag = 1;
			}
			if ((info & PICINFO_ERROR) || (frame_force_skip_flag))
				return true;
		}
	}

	return false;
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

/*
#define DUMP_USER_DATA
*/
#ifdef DUMP_USER_DATA
static int last_wp;
#define DUMP_USER_DATA_HEX


#ifdef DUMP_USER_DATA_HEX
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
#endif


#define DEBUG_CC_DUMP_ASCII

#ifdef DEBUG_CC_DUMP_ASCII
static int vbi_to_ascii(int c)
{
	if (c < 0)
		return '?';

	c &= 0x7F;

	if (c < 0x20 || c >= 0x7F)
		return '.';

	return c;
}

static void dump_cc_ascii(const uint8_t *buf, int poc)
{
	int cc_flag;
	int cc_count;
	int i;
	int szAscii[32];
	int index = 0;

	cc_flag = buf[1] & 0x40;
	if (!cc_flag) {
		pr_info("### cc_flag is invalid\n");
		return;
	}
	cc_count = buf[1] & 0x1f;

	for (i = 0; i < cc_count; ++i) {
		unsigned int b0;
		unsigned int cc_valid;
		unsigned int cc_type;
		unsigned char cc_data1;
		unsigned char cc_data2;

		b0 = buf[3 + i * 3];
		cc_valid = b0 & 4;
		cc_type = b0 & 3;
		cc_data1 = buf[4 + i * 3];
		cc_data2 = buf[5 + i * 3];


		if (cc_type == 0) {
			/* NTSC pair, Line 21 */
			szAscii[index++] = vbi_to_ascii(cc_data1);
			szAscii[index++] = vbi_to_ascii(cc_data2);
			if ((!cc_valid) || (i >= 3))
				break;
		}
	}
	switch (index) {
	case 8:
		pr_info("push poc:%d : %c %c %c %c %c %c %c %c\n",
			poc,
			szAscii[0], szAscii[1], szAscii[2], szAscii[3],
			szAscii[4], szAscii[5], szAscii[6], szAscii[7]);
		break;
	case 7:
		pr_info("push poc:%d : %c %c %c %c %c %c %c\n",
			poc,
			szAscii[0], szAscii[1], szAscii[2], szAscii[3],
			szAscii[4], szAscii[5], szAscii[6]);
		break;
	case 6:
		pr_info("push poc:%d : %c %c %c %c %c %c\n", poc,
			szAscii[0], szAscii[1], szAscii[2], szAscii[3],
			szAscii[4], szAscii[5]);
		break;
	case 5:
		pr_info("push poc:%d : %c %c %c %c %c\n", poc,
			szAscii[0], szAscii[1], szAscii[2], szAscii[3],
			szAscii[4]);
		break;
	case 4:
		pr_info("push poc:%d : %c %c %c %c\n", poc,
			szAscii[0], szAscii[1], szAscii[2], szAscii[3]);
		break;
	case 3:
		pr_info("push poc:%d : %c %c %c\n", poc,
			szAscii[0], szAscii[1], szAscii[2]);
		break;
	case 2:
		pr_info("push poc:%d : %c %c\n", poc,
			szAscii[0], szAscii[1]);
		break;
	case 1:
		pr_info("push poc:%d : %c\n", poc, szAscii[0]);
		break;
	default:
		pr_info("push poc:%d and no CC data: index = %d\n",
			poc, index);
		break;
	}
}
#endif


static int is_atsc(u8 *pdata)
{
	if ((pdata[0] == 0x47) &&
		(pdata[1] == 0x41) &&
		(pdata[2] == 0x39) &&
		(pdata[3] == 0x34))
		return 1;
	else
		return 0;
}
/*
#define DUMP_HEAD_INFO_DATA
*/
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
#ifdef DUMP_USER_DATA_HEX
	print_data(szBuf,
				user_data_length,
				flag,
				duration,
				vpts,
				vpts_valid,
				rec_id,
				reference);
#endif

#ifdef DEBUG_CC_DUMP_ASCII
#ifdef DUMP_HEAD_INFO_DATA
	if (is_atsc(szBuf+8))
		dump_cc_ascii(szBuf+8+4, reference);
#else
	if (is_atsc(szBuf))
		dump_cc_ascii(szBuf+4, reference);
#endif
#endif
}




#define MAX_USER_DATA_SIZE		1572864
static void *user_data_buf;
static unsigned char *pbuf_start;
static int total_len;
static int bskip;
static int n_userdata_id;

static void reset_user_data_buf(void)
{
	total_len = 0;
	pbuf_start = user_data_buf;
	bskip = 0;
	n_userdata_id = 0;
}

static void push_to_buf(u8 *pdata, int len, struct userdata_meta_info_t *pmeta,
	u32 reference)
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

	*pLen = reference;
	pbuf_start += sizeof(u32);
	info_cnt++;
	pLen++;



	pbuf_end = (u8 *)ccbuf_phyAddress_virt + CCBUF_SIZE;
	if (pdata + len > pbuf_end) {
		int first_section_len;

		first_section_len = pbuf_end - pdata;
		memcpy(pbuf_start, pdata, first_section_len);
		pdata = (u8 *)ccbuf_phyAddress_virt;
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

static void dump_userdata_info(void *puser_data,
					int len,
					struct userdata_meta_info_t *pmeta,
					u32 reference)
{
	u8 *pstart;

	pstart = (u8 *)puser_data;

#ifdef	DUMP_HEAD_INFO_DATA
	push_to_buf(pstart, len, pmeta, reference);
#else
	push_to_buf(pstart+8, len - 8, pmeta, reference);
#endif
}

static void show_user_data_buf(void)
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
	pbuf = user_data_buf;

	while (pbuf < pbuf_start) {
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
#endif

static void vmpeg12_add_userdata(struct userdata_meta_info_t meta_info,
								int wp,
								u32 reference);
/*
#define PRINT_HEAD_INFO
*/
static void userdata_push_do_work(struct work_struct *work)
{
	u32 reg;
	u32 offset, pts;
	u64 pts_us64 = 0;
	u8 *pdata;
	u8 head_info[8];
	struct userdata_meta_info_t meta_info;
	u32 wp;
	u32 index;
	u32 picture_struct;
	u32 reference;
	u32 picture_type;
	u32 temp;
#ifdef PRINT_HEAD_INFO
	u8 *ptype_str;
#endif
	memset(&meta_info, 0, sizeof(meta_info));

	meta_info.duration = frame_dur;

	reg = READ_VREG(AV_SCRATCH_M);
	meta_info.flags = ((reg >> 30) << 1);
	meta_info.flags |= (VFORMAT_MPEG12 << 3);
	/* check  top_field_first flag */
	if ((reg >> 28) & 0x1) {
		meta_info.flags |= (1 << 10);
		meta_info.flags |= (((reg >> 29) & 0x1) << 11);
	}

	offset = READ_VREG(AV_SCRATCH_N);
	if (offset != last_offset) {
		meta_info.flags |= 1;
		last_offset = offset;
	}

	if (pts_pickout_offset_us64
			 (PTS_TYPE_VIDEO, offset, &pts, 0, &pts_us64) != 0) {
		pr_info("pick out pts failed by offset = 0x%x\n", offset);
		pts = -1;
		meta_info.vpts_valid = 0;
	} else
		meta_info.vpts_valid = 1;
	meta_info.vpts = pts;

	if (!ccbuf_phyAddress_is_remaped_nocache &&
		ccbuf_phyAddress &&
		ccbuf_phyAddress_virt) {
		codec_mm_dma_flush(
			ccbuf_phyAddress_virt,
			CCBUF_SIZE,
			DMA_FROM_DEVICE);
	}

	mutex_lock(&userdata_mutex);
	if (p_userdata_mgr && ccbuf_phyAddress_virt) {
		int new_wp;

		new_wp = reg & 0xffff;
		if (new_wp < p_userdata_mgr->last_wp)
			pdata = (u8 *)ccbuf_phyAddress_virt;
		else
			pdata = (u8 *)ccbuf_phyAddress_virt +
						p_userdata_mgr->last_wp;
		memcpy(head_info, pdata, 8);
	} else
		memset(head_info, 0, 8);
	mutex_unlock(&userdata_mutex);
	aml_swap_data(head_info, 8);

	wp = (head_info[0] << 8 | head_info[1]);
	index = (head_info[2] << 8 | head_info[3]);

	picture_struct = (head_info[6] << 8 | head_info[7]);
	temp = (head_info[4] << 8 | head_info[5]);
	reference = temp & 0x3FF;
	picture_type = (temp >> 10) & 0x7;

#if 0
	pr_info("index = %d, wp = %d, ref = %d, type = %d, struct = 0x%x, vpts:0x%x\n",
		index, wp, reference,
		picture_type, picture_struct, meta_info.vpts);
#endif
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
	pr_info("ref:%d, type:%s, ext:%d, offset:0x%x, first:%d, id:%d\n",
		reference, ptype_str,
		(reg >> 30), offset,
		(reg >> 28)&0x3,
		n_userdata_id);
#endif
	vmpeg12_add_userdata(meta_info, reg & 0xffff, reference);

	WRITE_VREG(AV_SCRATCH_M, 0);
}

static void vmpeg12_notify_work(struct work_struct *work)
{
	pr_info("frame duration changed %d\n", frame_dur);
	if (fr_hint_status == VDEC_NEED_HINT) {
		vf_notify_receiver(PROVIDER_NAME,
			VFRAME_EVENT_PROVIDER_FR_HINT,
			(void *)((unsigned long)frame_dur));
		fr_hint_status = VDEC_HINTED;
	}
	return;
}
static irqreturn_t vmpeg12_isr(int irq, void *dev_id)
{
	u32 reg, info, seqinfo, offset, pts, pts_valid = 0;
	struct vframe_s *vf;
	u64 pts_us64 = 0;
	u32 frame_size;

	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);

	reg = READ_VREG(MREG_BUFFEROUT);

	if (reg) {
		info = READ_VREG(MREG_PIC_INFO);
		offset = READ_VREG(MREG_FRAME_OFFSET);
		seqinfo = READ_VREG(MREG_SEQ_INFO);

		if ((first_i_frame_ready == 0) &&
			((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I) &&
			((info & PICINFO_ERROR) == 0))
			first_i_frame_ready = 1;

		if ((pts_lookup_offset_us64
			 (PTS_TYPE_VIDEO, offset, &pts,
			 &frame_size, 0, &pts_us64) == 0)
			&& (((info & PICINFO_TYPE_MASK) == PICINFO_TYPE_I)
				|| ((info & PICINFO_TYPE_MASK) ==
					PICINFO_TYPE_P)))
			pts_valid = 1;

		if (pts_valid && lastpts == pts)
			pts_valid = 0;
		if (pts_valid)
			lastpts = pts;
		/*if (frame_prog == 0) */
		{
			frame_prog = info & PICINFO_PROG;
			if ((seqinfo & SEQINFO_EXT_AVAILABLE)
				&& (!(seqinfo & SEQINFO_PROG)))
				frame_prog = 0;
		}

		if ((dec_control &
			 DEC_CONTROL_FLAG_FORCE_2500_720_576_INTERLACE)
			&& (frame_width == 720 || frame_width == 480)
			&& (frame_height == 576)
			&& (frame_dur == 3840))
			frame_prog = 0;
		else if ((dec_control &
			DEC_CONTROL_FLAG_FORCE_3000_704_480_INTERLACE)
			&& (frame_width == 704) && (frame_height == 480)
			&& (frame_dur == 3200))
			frame_prog = 0;
		else if ((dec_control &
			DEC_CONTROL_FLAG_FORCE_2500_704_576_INTERLACE)
			&& (frame_width == 704) && (frame_height == 576)
			&& (frame_dur == 3840))
			frame_prog = 0;
		else if ((dec_control &
			DEC_CONTROL_FLAG_FORCE_2500_544_576_INTERLACE)
			&& (frame_width == 544) && (frame_height == 576)
			&& (frame_dur == 3840))
			frame_prog = 0;
		else if ((dec_control &
			DEC_CONTROL_FLAG_FORCE_2500_480_576_INTERLACE)
			&& (frame_width == 480) && (frame_height == 576)
			&& (frame_dur == 3840))
			frame_prog = 0;
		else if (dec_control & DEC_CONTROL_FLAG_FORCE_SEQ_INTERLACE)
			frame_prog = 0;
		if (frame_prog & PICINFO_PROG) {
			u32 index = ((reg & 0xf) - 1) & 7;

			seqinfo = READ_VREG(MREG_SEQ_INFO);

			if (kfifo_get(&newframe_q, &vf) == 0) {
				pr_info
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}

			set_frame_info(vf);
			/*pr_info("video signal type:0x%x\n",
				READ_VREG(AV_SCRATCH_H));*/
			vf->signal_type = READ_VREG(AV_SCRATCH_H);
			vf->index = index;
#ifdef NV21
			vf->type =
				VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
#else
			vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD;
#endif
			if ((seqinfo & SEQINFO_EXT_AVAILABLE)
				&& (seqinfo & SEQINFO_PROG)) {
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

			/*count info*/
			vdec_count_info(gvs, info & PICINFO_ERROR, offset);

			vf->duration += vf->duration_pulldown;
			vf->canvas0Addr = vf->canvas1Addr =
						index2canvas(index);
			vf->orientation = 0;
			vf->pts = (pts_valid) ? pts : 0;
			vf->pts_us64 = (pts_valid) ? pts_us64 : 0;
			vf->type_original = vf->type;

			vfbuf_use[index] = 1;

			if ((error_skip(info, vf)) ||
				((first_i_frame_ready == 0)
				 && ((PICINFO_TYPE_MASK & info) !=
					 PICINFO_TYPE_I))) {
				gvs->drop_frame_count++;
				kfifo_put(&recycle_q,
						  (const struct vframe_s *)vf);
			} else {
				vf->mem_handle =
					decoder_bmmu_box_get_mem_handle(
						mm_blk_handle,
						index);
				kfifo_put(&display_q,
						  (const struct vframe_s *)vf);
				ATRACE_COUNTER(MODULE_NAME, vf->pts);
				vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
			}

		} else {
			u32 index = ((reg & 0xf) - 1) & 7;
			int first_field_type = (info & PICINFO_TOP_FIRST) ?
					VIDTYPE_INTERLACE_TOP :
					VIDTYPE_INTERLACE_BOTTOM;

#ifdef INTERLACE_SEQ_ALWAYS
			/* once an interlaced sequence exist,
			 *always force interlaced type
			 */
			/* to make DI easy. */
			dec_control |= DEC_CONTROL_FLAG_FORCE_SEQ_INTERLACE;
#endif
#if 0
			if (info & PICINFO_FRAME) {
				frame_rpt_state =
					(info & PICINFO_TOP_FIRST) ?
					FRAME_REPEAT_TOP : FRAME_REPEAT_BOT;
			} else {
				if (frame_rpt_state == FRAME_REPEAT_TOP) {
					first_field_type =
						VIDTYPE_INTERLACE_TOP;
				} else if (frame_rpt_state ==
						FRAME_REPEAT_BOT) {
					first_field_type =
						VIDTYPE_INTERLACE_BOTTOM;
				}
				frame_rpt_state = FRAME_REPEAT_NONE;
			}
#else
			frame_rpt_state = FRAME_REPEAT_NONE;
#endif
			if (kfifo_get(&newframe_q, &vf) == 0) {
				pr_info
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}
			if (info & PICINFO_RPT_FIRST)
				vfbuf_use[index] = 3;
			else
				vfbuf_use[index] = 2;

			set_frame_info(vf);
			vf->signal_type = 0;
			vf->index = index;
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
			vf->canvas0Addr = vf->canvas1Addr =
						index2canvas(index);
			vf->pts = (pts_valid) ? pts : 0;
			vf->pts_us64 = (pts_valid) ? pts_us64 : 0;
			vf->type_original = vf->type;

			if ((error_skip(info, vf)) ||
				((first_i_frame_ready == 0)
				 && ((PICINFO_TYPE_MASK & info) !=
					 PICINFO_TYPE_I))) {
				gvs->drop_frame_count++;
				kfifo_put(&recycle_q,
						  (const struct vframe_s *)vf);
			} else {
				vf->mem_handle =
					decoder_bmmu_box_get_mem_handle(
						mm_blk_handle,
						index);
				kfifo_put(&display_q,
						  (const struct vframe_s *)vf);
				ATRACE_COUNTER(MODULE_NAME, vf->pts);
				vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
			}

			if (kfifo_get(&newframe_q, &vf) == 0) {
				pr_info
				("fatal error, no available buffer slot.");
				return IRQ_HANDLED;
			}

			set_frame_info(vf);
			vf->signal_type = 0;
			vf->index = index;
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
			vf->canvas0Addr = vf->canvas1Addr =
					index2canvas(index);
			vf->pts = 0;
			vf->pts_us64 = 0;
			vf->type_original = vf->type;

			/*count info*/
			vdec_count_info(gvs, info & PICINFO_ERROR, offset);

			if ((error_skip(info, vf)) ||
				((first_i_frame_ready == 0)
				 && ((PICINFO_TYPE_MASK & info) !=
					PICINFO_TYPE_I))) {
				gvs->drop_frame_count++;
				kfifo_put(&recycle_q,
					(const struct vframe_s *)vf);
			} else {
				vf->mem_handle =
					decoder_bmmu_box_get_mem_handle(
						mm_blk_handle,
						index);
				kfifo_put(&display_q,
					(const struct vframe_s *)vf);
				ATRACE_COUNTER(MODULE_NAME, vf->pts);
				vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
					NULL);
			}

			if (info & PICINFO_RPT_FIRST) {
				if (kfifo_get(&newframe_q, &vf) == 0) {
					pr_info("error, no available buffer slot.");
					return IRQ_HANDLED;
				}

				set_frame_info(vf);

				vf->index = index;
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
				vf->canvas0Addr = vf->canvas1Addr =
							index2canvas(index);
				vf->pts = 0;
				vf->pts_us64 = 0;
				if ((error_skip(info, vf)) ||
					((first_i_frame_ready == 0)
						&& ((PICINFO_TYPE_MASK & info)
							!= PICINFO_TYPE_I))) {
					kfifo_put(&recycle_q,
					(const struct vframe_s *)vf);
				} else {
					kfifo_put(&display_q,
						(const struct vframe_s *)vf);
					ATRACE_COUNTER(MODULE_NAME, vf->pts);
					vf_notify_receiver(PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_VFRAME_READY,
						NULL);
				}
			}
		}
		WRITE_VREG(MREG_BUFFEROUT, 0);
	}

	reg = READ_VREG(AV_SCRATCH_M);
	if (reg & (1<<16))
		schedule_work(&userdata_push_work);

	return IRQ_HANDLED;
}

static struct vframe_s *vmpeg_vf_peek(void *op_arg)
{
	struct vframe_s *vf;

	if (kfifo_peek(&display_q, &vf))
		return vf;

	return NULL;
}

static struct vframe_s *vmpeg_vf_get(void *op_arg)
{
	struct vframe_s *vf;

	if (kfifo_get(&display_q, &vf))
		return vf;

	return NULL;
}

static void vmpeg_vf_put(struct vframe_s *vf, void *op_arg)
{
	if (pool_index(vf) == cur_pool_idx)
		kfifo_put(&recycle_q, (const struct vframe_s *)vf);
}

static int vmpeg_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_RESET) {
		unsigned long flags;

		amvdec_stop();
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vf_light_unreg_provider(&vmpeg_vf_prov);
#endif
		spin_lock_irqsave(&lock, flags);
		vmpeg12_local_init();
		vmpeg12_prot_init();
		spin_unlock_irqrestore(&lock, flags);
#ifndef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
		vf_reg_provider(&vmpeg_vf_prov);
#endif
		amvdec_start();
	}
	return 0;
}

static int vmpeg_vf_states(struct vframe_states *states, void *op_arg)
{
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	states->vf_pool_size = VF_POOL_SIZE;
	states->buf_free_num = kfifo_len(&newframe_q);
	states->buf_avail_num = kfifo_len(&display_q);
	states->buf_recycle_num = kfifo_len(&recycle_q);

	spin_unlock_irqrestore(&lock, flags);

	return 0;
}

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
static void vmpeg12_ppmgr_reset(void)
{
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_RESET, NULL);

	vmpeg12_local_init();

	pr_info("vmpeg12dec: vf_ppmgr_reset\n");
}
#endif

static void vmpeg12_reset_userdata_fifo(struct vdec_s *vdec, int bInit);
static void vmpeg12_wakeup_userdata_poll(struct vdec_s *vdec);

static void reset_do_work(struct work_struct *work)
{
	amvdec_stop();

#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
	vmpeg12_ppmgr_reset();
#else
	vf_light_unreg_provider(&vmpeg_vf_prov);
	vmpeg12_local_init();
	vf_reg_provider(&vmpeg_vf_prov);
#endif
	vmpeg12_prot_init();
	vmpeg12_create_userdata_manager(ccbuf_phyAddress_virt, CCBUF_SIZE);
	vmpeg12_reset_userdata_fifo(vdec, 1);
#ifdef DUMP_USER_DATA
	last_wp = 0;
#endif

	amvdec_start();
}

static void vmpeg12_set_clk(struct work_struct *work)
{
	 {
		int fps = 96000 / frame_dur;

		saved_resolution = frame_width * frame_height * fps;
		vdec_source_changed(VFORMAT_MPEG12,
			frame_width, frame_height, fps);
	}
}


static void vmpeg_put_timer_func(struct timer_list *timer)
{
	int fatal_reset = 0;
	enum receviver_start_e state = RECEIVER_INACTIVE;

	if (vf_get_receiver(PROVIDER_NAME)) {
		state = vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_QUREY_STATE, NULL);
		if ((state == RECEIVER_STATE_NULL)
			|| (state == RECEIVER_STATE_NONE)) {
			/* receiver has no event_cb or
			 *receiver's event_cb does not process this event
			 */
			state = RECEIVER_INACTIVE;
		}
	} else
		state = RECEIVER_INACTIVE;

	if (READ_VREG(MREG_FATAL_ERROR) == 1)
		fatal_reset = 1;

	if ((READ_VREG(MREG_WAIT_BUFFER) != 0) &&
		(kfifo_is_empty(&recycle_q)) &&
		(kfifo_is_empty(&display_q)) && (state == RECEIVER_INACTIVE)) {
		if (++wait_buffer_counter > 4)
			fatal_reset = 1;

	} else
		wait_buffer_counter = 0;

	if (fatal_reset && (kfifo_is_empty(&display_q))) {
		pr_info("$$$$decoder is waiting for buffer or fatal reset.\n");
		schedule_work(&reset_work);
	}

	while (!kfifo_is_empty(&recycle_q) && (READ_VREG(MREG_BUFFERIN) == 0)) {
		struct vframe_s *vf;

		if (kfifo_get(&recycle_q, &vf)) {
			if ((vf->index < DECODE_BUFFER_NUM_MAX) &&
			 (--vfbuf_use[vf->index] == 0)) {
				WRITE_VREG(MREG_BUFFERIN, vf->index + 1);
				vf->index = DECODE_BUFFER_NUM_MAX;
			}

			if (pool_index(vf) == cur_pool_idx) {
				kfifo_put(&newframe_q,
						  (const struct vframe_s *)vf);
			}
		}
	}

	if (frame_dur > 0 && saved_resolution !=
		frame_width * frame_height * (96000 / frame_dur))
		schedule_work(&set_clk_work);

	timer->expires = jiffies + PUT_INTERVAL;

	add_timer(timer);
}

int vmpeg12_dec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	if (!(stat & STAT_VDEC_RUN))
		return -1;

	vstatus->frame_width = frame_width;
	vstatus->frame_height = frame_height;
	if (frame_dur != 0)
		vstatus->frame_rate = 96000 / frame_dur;
	else
		vstatus->frame_rate = -1;
	vstatus->error_count = READ_VREG(AV_SCRATCH_C);
	vstatus->status = stat;
	vstatus->bit_rate = gvs->bit_rate;
	vstatus->frame_dur = frame_dur;
	vstatus->frame_data = gvs->frame_data;
	vstatus->total_data = gvs->total_data;
	vstatus->frame_count = gvs->frame_count;
	vstatus->error_frame_count = gvs->error_frame_count;
	vstatus->drop_frame_count = gvs->drop_frame_count;
	vstatus->total_data = gvs->total_data;
	vstatus->samp_cnt = gvs->samp_cnt;
	vstatus->offset = gvs->offset;
	vstatus->ratio_control = ratio_control;
	snprintf(vstatus->vdec_name, sizeof(vstatus->vdec_name),
		"%s", DRIVER_NAME);

	return 0;
}

int vmpeg12_set_isreset(struct vdec_s *vdec, int isreset)
{
	is_reset = isreset;
	return 0;
}




static void vmpeg12_create_userdata_manager(u8 *userdata_buf, int buf_len)
{
	mutex_lock(&userdata_mutex);

	p_userdata_mgr = (struct mpeg12_userdata_info_t *)
			vmalloc(sizeof(struct mpeg12_userdata_info_t));
	if (p_userdata_mgr) {
		memset(p_userdata_mgr, 0,
			sizeof(struct mpeg12_userdata_info_t));
		p_userdata_mgr->data_buf = userdata_buf;
		p_userdata_mgr->buf_len = buf_len;
		p_userdata_mgr->data_buf_end = userdata_buf + buf_len;
	}
	mutex_unlock(&userdata_mutex);
}

static void vmpeg12_destroy_userdata_manager(void)
{
	mutex_lock(&userdata_mutex);

	if (p_userdata_mgr) {
		vfree(p_userdata_mgr);
		p_userdata_mgr = NULL;
	}
	mutex_unlock(&userdata_mutex);
}

static void vmpeg12_add_userdata(struct userdata_meta_info_t meta_info,
						int wp,
						u32 reference)
{
	struct mpeg12_userdata_recored_t *p_userdata_rec;
	int data_length;

	mutex_lock(&userdata_mutex);

	if (p_userdata_mgr) {
		if (wp > p_userdata_mgr->last_wp)
			data_length = wp - p_userdata_mgr->last_wp;
		else {
			p_userdata_mgr->last_wp = 0;
			data_length = wp - p_userdata_mgr->last_wp;
		}

		if (data_length & 0x7)
			data_length = (((data_length + 8) >> 3) << 3);
/*
pr_info("wakeup_push: ri:%d, wi:%d, data_len:%d, last_wp:%d, wp:%d, id = %d\n",
			p_userdata_mgr->read_index,
			p_userdata_mgr->write_index,
			data_length,
			p_userdata_mgr->last_wp,
			wp,
			n_userdata_id);
*/
		p_userdata_rec = p_userdata_mgr->records
			+ p_userdata_mgr->write_index;
		p_userdata_rec->meta_info = meta_info;
		p_userdata_rec->rec_start = p_userdata_mgr->last_wp;
		p_userdata_rec->rec_len = data_length;
		p_userdata_mgr->last_wp = wp;

#ifdef DUMP_USER_DATA
		dump_userdata_info(p_userdata_mgr->data_buf
			+ p_userdata_rec->rec_start,
			data_length,
			&meta_info,
			reference);
		n_userdata_id++;
#endif

		p_userdata_mgr->write_index++;
		if (p_userdata_mgr->write_index >= USERDATA_FIFO_NUM)
			p_userdata_mgr->write_index = 0;
	}
	mutex_unlock(&userdata_mutex);

	vdec_wakeup_userdata_poll(vdec);
}


static int vmpeg12_user_data_read(struct vdec_s *vdec,
	struct userdata_param_t *puserdata_para)
{
	int rec_ri, rec_wi;
	int rec_len;
	u8 *rec_data_start;
	u8 *pdest_buf;
	struct mpeg12_userdata_recored_t *p_userdata_rec;
	u32 data_size;
	u32 res;
	int copy_ok = 1;

	pdest_buf = puserdata_para->pbuf_addr;

	mutex_lock(&userdata_mutex);

	if (!p_userdata_mgr) {
		mutex_unlock(&userdata_mutex);
		return 0;
	}
/*
	pr_info("ri = %d, wi = %d\n",
		p_userdata_mgr->read_index,
		p_userdata_mgr->write_index);
*/
	rec_ri = p_userdata_mgr->read_index;
	rec_wi = p_userdata_mgr->write_index;

	if (rec_ri == rec_wi) {
		mutex_unlock(&userdata_mutex);
		return 0;
	}

	p_userdata_rec = p_userdata_mgr->records + rec_ri;

	rec_len = p_userdata_rec->rec_len;
	rec_data_start = p_userdata_rec->rec_start + p_userdata_mgr->data_buf;
/*
	pr_info("rec_len:%d, rec_start:%d, buf_len:%d\n",
		p_userdata_rec->rec_len,
		p_userdata_rec->rec_start,
		puserdata_para->buf_len);
*/
	if (rec_len <= puserdata_para->buf_len) {
		/* dvb user data buffer is enought to copy the whole recored. */
		data_size = rec_len;
		if (rec_data_start + data_size
			> p_userdata_mgr->data_buf_end) {
			int first_section_len;

			first_section_len = p_userdata_mgr->buf_len
				- p_userdata_rec->rec_start;
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
					(void *)p_userdata_mgr->data_buf,
					data_size - first_section_len);
				if (res) {
					pr_info("p2 read not end res=%d, request=%d\n",
						res, data_size);
					copy_ok = 0;
				}
				p_userdata_rec->rec_len -= data_size - res;
				p_userdata_rec->rec_start =
					data_size - first_section_len - res;
				puserdata_para->data_size = data_size - res;
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
			p_userdata_mgr->read_index++;
			if (p_userdata_mgr->read_index >= USERDATA_FIFO_NUM)
				p_userdata_mgr->read_index = 0;
		}
	} else {
		/* dvb user data buffer is not enought
		to copy the whole recored. */
		data_size = puserdata_para->buf_len;
		if (rec_data_start + data_size
			> p_userdata_mgr->data_buf_end) {
			int first_section_len;

			first_section_len = p_userdata_mgr->buf_len
						- p_userdata_rec->rec_start;
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
					(void *)p_userdata_mgr->data_buf,
					data_size - first_section_len);
				if (res) {
					pr_info("p5 read not end res=%d, request=%d\n",
						res,
						data_size - first_section_len);
					copy_ok = 0;
				}

				p_userdata_rec->rec_len -= data_size - res;
				p_userdata_rec->rec_start =
					data_size - first_section_len - res;
				puserdata_para->data_size = data_size - res;
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
			p_userdata_mgr->read_index++;
			if (p_userdata_mgr->read_index
				>= USERDATA_FIFO_NUM)
				p_userdata_mgr->read_index = 0;
		}

	}

	puserdata_para->meta_info = p_userdata_rec->meta_info;

	if (p_userdata_mgr->read_index <= p_userdata_mgr->write_index)
		puserdata_para->meta_info.records_in_que =
			p_userdata_mgr->write_index -
			p_userdata_mgr->read_index;
	else
		puserdata_para->meta_info.records_in_que =
			p_userdata_mgr->write_index +
			USERDATA_FIFO_NUM -
			p_userdata_mgr->read_index;
	puserdata_para->version = (0<<24|0<<16|0<<8|1);

	mutex_unlock(&userdata_mutex);

	return 1;
}

static void vmpeg12_reset_userdata_fifo(struct vdec_s *vdec, int bInit)
{
	mutex_lock(&userdata_mutex);

	if (p_userdata_mgr) {
		pr_info("vmpeg12_reset_userdata_fifo: bInit: %d, ri: %d, wi: %d\n",
			bInit, p_userdata_mgr->read_index,
			p_userdata_mgr->write_index);
		p_userdata_mgr->read_index = 0;
		p_userdata_mgr->write_index = 0;

		if (bInit)
			p_userdata_mgr->last_wp = 0;
	}

	mutex_unlock(&userdata_mutex);
}

static void vmpeg12_wakeup_userdata_poll(struct vdec_s *vdec)
{
	amstream_wakeup_userdata_poll(vdec);
}

static int vmpeg12_vdec_info_init(void)
{
	gvs = kzalloc(sizeof(struct vdec_info), GFP_KERNEL);
	if (NULL == gvs) {
		pr_info("the struct of vdec status malloc failed.\n");
		return -ENOMEM;
	}
	return 0;
}

/****************************************/
static int vmpeg12_canvas_init(void)
{
	int i, ret;
	u32 canvas_width, canvas_height;
	u32 decbuf_size, decbuf_y_size, decbuf_uv_size;
	static unsigned long buf_start;

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

		if (i == (MAX_BMMU_BUFFER_NUM - 1)) /* workspace mem */
			decbuf_size = WORKSPACE_SIZE;

		ret = decoder_bmmu_box_alloc_buf_phy(mm_blk_handle, i,
				decbuf_size, DRIVER_NAME, &buf_start);
		if (ret < 0)
			return ret;

		if (i == (MAX_BMMU_BUFFER_NUM - 1)) {

			WRITE_VREG(MREG_CO_MV_START, (buf_start + CCBUF_SIZE));
			if (!ccbuf_phyAddress) {
				ccbuf_phyAddress
				= (u32)buf_start;

				ccbuf_phyAddress_virt
					= codec_mm_phys_to_virt(
						ccbuf_phyAddress);
				if ((!ccbuf_phyAddress_virt) && (!tvp_flag)) {
					ccbuf_phyAddress_virt
						= codec_mm_vmap(
							ccbuf_phyAddress,
							CCBUF_SIZE);
					ccbuf_phyAddress_is_remaped_nocache = 1;
				}
			}

		} else {
#ifdef NV21
			config_cav_lut_ex(2 * i + 0,
				buf_start,
				canvas_width, canvas_height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32, 0, VDEC_1);
			config_cav_lut_ex(2 * i + 1,
				buf_start +
				decbuf_y_size, canvas_width,
				canvas_height / 2, CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_32X32, 0, VDEC_1);
#else
			config_cav_lut_ex(3 * i + 0,
				buf_start,
				canvas_width, canvas_height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32, 0, VDEC_1);
			config_cav_lut_ex(3 * i + 1,
				buf_start +
				decbuf_y_size, canvas_width / 2,
				canvas_height / 2, CANVAS_ADDR_NOWRAP,
				CANVAS_BLKMODE_32X32, 0, VDEC_1);
			config_cav_lut_ex(3 * i + 2,
				buf_start +
				decbuf_y_size + decbuf_uv_size,
				canvas_width / 2, canvas_height / 2,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_32X32, 0, VDEC_1);
#endif
		}
	}

	return 0;
}

static int vmpeg12_prot_init(void)
{
	int ret;
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
		int save_reg = READ_VREG(POWER_CTL_VLD);

		WRITE_VREG(DOS_SW_RESET0, (1 << 7) | (1 << 6) | (1 << 4));
		WRITE_VREG(DOS_SW_RESET0, 0);

		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8) {

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

			WRITE_VREG(MDEC_SW_RESET, (1 << 7));
			WRITE_VREG(MDEC_SW_RESET, 0);
		}

		WRITE_VREG(POWER_CTL_VLD, save_reg);

	} else
		WRITE_RESET_REG(RESET0_REGISTER, RESET_IQIDCT | RESET_MC);

	ret = vmpeg12_canvas_init();

#ifdef NV21
	WRITE_VREG(AV_SCRATCH_0, 0x010100);
	WRITE_VREG(AV_SCRATCH_1, 0x030302);
	WRITE_VREG(AV_SCRATCH_2, 0x050504);
	WRITE_VREG(AV_SCRATCH_3, 0x070706);
	WRITE_VREG(AV_SCRATCH_4, 0x090908);
	WRITE_VREG(AV_SCRATCH_5, 0x0b0b0a);
	WRITE_VREG(AV_SCRATCH_6, 0x0d0d0c);
	WRITE_VREG(AV_SCRATCH_7, 0x0f0f0e);
#else
	WRITE_VREG(AV_SCRATCH_0, 0x020100);
	WRITE_VREG(AV_SCRATCH_1, 0x050403);
	WRITE_VREG(AV_SCRATCH_2, 0x080706);
	WRITE_VREG(AV_SCRATCH_3, 0x0b0a09);
	WRITE_VREG(AV_SCRATCH_4, 0x0e0d0c);
	WRITE_VREG(AV_SCRATCH_5, 0x11100f);
	WRITE_VREG(AV_SCRATCH_6, 0x141312);
	WRITE_VREG(AV_SCRATCH_7, 0x171615);
#endif

	/* set to mpeg1 default */
	WRITE_VREG(MPEG1_2_REG, 0);
	/* disable PSCALE for hardware sharing */
	WRITE_VREG(PSCALE_CTRL, 0);
	/* for Mpeg1 default value */
	WRITE_VREG(PIC_HEAD_INFO, 0x380);
	/* disable mpeg4 */
	WRITE_VREG(M4_CONTROL_REG, 0);
	/* clear mailbox interrupt */
	WRITE_VREG(ASSIST_MBOX1_CLR_REG, 1);
	/* clear buffer IN/OUT registers */
	WRITE_VREG(MREG_BUFFERIN, 0);
	WRITE_VREG(MREG_BUFFEROUT, 0);
	/* set reference width and height */
	if ((frame_width != 0) && (frame_height != 0))
		WRITE_VREG(MREG_CMD, (frame_width << 16) | frame_height);
	else
		WRITE_VREG(MREG_CMD, 0);
	WRITE_VREG(MREG_FORCE_I_RDY, (force_first_i_ready & 0x01));
	/* clear error count */
	WRITE_VREG(MREG_ERROR_COUNT, 0);
	WRITE_VREG(MREG_FATAL_ERROR, 0);
	/* clear wait buffer status */
	WRITE_VREG(MREG_WAIT_BUFFER, 0);
#ifdef NV21
	SET_VREG_MASK(MDEC_PIC_DC_CTRL, 1 << 17);
#endif
	return ret;
}

static void vmpeg12_local_init(void)
{
	int i;

	INIT_KFIFO(display_q);
	INIT_KFIFO(recycle_q);
	INIT_KFIFO(newframe_q);

	cur_pool_idx ^= 1;

	for (i = 0; i < VF_POOL_SIZE; i++) {
		const struct vframe_s *vf;

		if (cur_pool_idx == 0) {
			vf = &vfpool[i];
			vfpool[i].index = DECODE_BUFFER_NUM_MAX;
			} else {
			vf = &vfpool2[i];
			vfpool2[i].index = DECODE_BUFFER_NUM_MAX;
			}
		kfifo_put(&newframe_q, (const struct vframe_s *)vf);
	}

	for (i = 0; i < DECODE_BUFFER_NUM_MAX; i++)
		vfbuf_use[i] = 0;
	if (mm_blk_handle) {
		mutex_lock(&userdata_mutex);
		if (p_userdata_mgr) {
			vfree(p_userdata_mgr);
			p_userdata_mgr = NULL;
		}
		if (ccbuf_phyAddress_is_remaped_nocache)
			codec_mm_unmap_phyaddr(ccbuf_phyAddress_virt);
		ccbuf_phyAddress_virt = NULL;
		ccbuf_phyAddress = 0;
		ccbuf_phyAddress_is_remaped_nocache = 0;
		decoder_bmmu_box_free(mm_blk_handle);
		mm_blk_handle = NULL;
		mutex_unlock(&userdata_mutex);
	}

		mm_blk_handle = decoder_bmmu_box_alloc_box(
			DRIVER_NAME,
			0,
			MAX_BMMU_BUFFER_NUM,
			4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_FOR_VDECODER |
			tvp_flag,
			BMMU_ALLOC_FLAGS_WAITCLEAR);


	frame_width = frame_height = frame_dur = frame_prog = 0;
	frame_force_skip_flag = 0;
	wait_buffer_counter = 0;
	first_i_frame_ready = force_first_i_ready;
	saved_resolution = 0;
	dec_control &= DEC_CONTROL_INTERNAL_MASK;
}

static s32 vmpeg12_init(void)
{
	int ret = -1, size = -1;
	char *buf = vmalloc(0x1000 * 16);

	if (IS_ERR_OR_NULL(buf))
		return -ENOMEM;

	timer_setup(&recycle_timer, vmpeg_put_timer_func, 0);

	stat |= STAT_TIMER_INIT;

	vmpeg12_local_init();

	amvdec_enable();

	size = get_firmware_data(VIDEO_DEC_MPEG12, buf);
	if (size < 0) {
		pr_err("get firmware fail.");
		vfree(buf);
		return -1;
	}

	ret = amvdec_loadmc_ex(VFORMAT_MPEG12, "mpeg12", buf);
	if (ret < 0) {
		amvdec_disable();
		vfree(buf);
		pr_err("MPEG12: the %s fw loading failed, err: %x\n",
			tee_enabled() ? "TEE" : "local", ret);
		return -EBUSY;
	}

	vfree(buf);

	stat |= STAT_MC_LOAD;

	/* enable AMRISC side protocol */
	vmpeg12_prot_init();

	ret = vdec_request_irq(VDEC_IRQ_1, vmpeg12_isr,
		    "vmpeg12-irq", (void *)vmpeg12_dec_id);

	if (ret) {
		amvdec_disable();
		amlog_level(LOG_LEVEL_ERROR, "vmpeg12 irq register error.\n");
		return -ENOENT;
	}

	stat |= STAT_ISR_REG;
#ifdef CONFIG_AMLOGIC_POST_PROCESS_MANAGER
	vf_provider_init(&vmpeg_vf_prov, PROVIDER_NAME, &vmpeg_vf_provider,
					 NULL);
	vf_reg_provider(&vmpeg_vf_prov);
	vf_notify_receiver(PROVIDER_NAME, VFRAME_EVENT_PROVIDER_START, NULL);
#else
	vf_provider_init(&vmpeg_vf_prov, PROVIDER_NAME, &vmpeg_vf_provider,
					 NULL);
	vf_reg_provider(&vmpeg_vf_prov);
#endif
	if (vmpeg12_amstream_dec_info.rate != 0) {
		if (!is_reset) {
			vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_FR_HINT,
				(void *)
				((unsigned long)
				vmpeg12_amstream_dec_info.rate));
			fr_hint_status = VDEC_HINTED;
		}
	} else
		fr_hint_status = VDEC_NEED_HINT;

	stat |= STAT_VF_HOOK;

	recycle_timer.expires = jiffies + PUT_INTERVAL;
	add_timer(&recycle_timer);

	stat |= STAT_TIMER_ARM;

	amvdec_start();

	stat |= STAT_VDEC_RUN;

	return 0;
}

#ifdef DUMP_USER_DATA
static int amvdec_mpeg12_init_userdata_dump(void)
{
	user_data_buf = kmalloc(MAX_USER_DATA_SIZE, GFP_KERNEL);
	if (user_data_buf)
		return 1;
	else
		return 0;
}
#endif

static int amvdec_mpeg12_probe(struct platform_device *pdev)
{
	struct vdec_s *pdata = *(struct vdec_s **)pdev->dev.platform_data;

	amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg12 probe start.\n");

	if (pdata == NULL) {
		amlog_level(LOG_LEVEL_ERROR,
			"amvdec_mpeg12 platform data undefined.\n");
		return -EFAULT;
	}

	tvp_flag = vdec_secure(pdata) ? CODEC_MM_FLAGS_TVP : 0;
	if (pdata->sys_info)
		vmpeg12_amstream_dec_info = *pdata->sys_info;

	pdata->dec_status = vmpeg12_dec_status;
	pdata->set_isreset = vmpeg12_set_isreset;

	pdata->user_data_read = vmpeg12_user_data_read;
	pdata->reset_userdata_fifo = vmpeg12_reset_userdata_fifo;
	pdata->wakeup_userdata_poll = vmpeg12_wakeup_userdata_poll;
	is_reset = 0;

	vmpeg12_vdec_info_init();

	INIT_WORK(&set_clk_work, vmpeg12_set_clk);
	if (vmpeg12_init() < 0) {
		amlog_level(LOG_LEVEL_ERROR, "amvdec_mpeg12 init failed.\n");
		kfree(gvs);
		gvs = NULL;
		pdata->dec_status = NULL;
		return -ENODEV;
	}
	vdec = pdata;
#ifdef DUMP_USER_DATA
	amvdec_mpeg12_init_userdata_dump();
#endif
	vmpeg12_create_userdata_manager(ccbuf_phyAddress_virt, CCBUF_SIZE);

	INIT_WORK(&userdata_push_work, userdata_push_do_work);
	INIT_WORK(&notify_work, vmpeg12_notify_work);
	INIT_WORK(&reset_work, reset_do_work);


	last_offset = 0xFFFFFFFF;
#ifdef DUMP_USER_DATA
	last_wp = 0;
	reset_user_data_buf();
#endif

	amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg12 probe end.\n");

	return 0;
}

static int amvdec_mpeg12_remove(struct platform_device *pdev)
{
	cancel_work_sync(&userdata_push_work);
	cancel_work_sync(&notify_work);
	cancel_work_sync(&reset_work);

	if (stat & STAT_VDEC_RUN) {
		amvdec_stop();
		stat &= ~STAT_VDEC_RUN;
	}

	if (stat & STAT_ISR_REG) {
		vdec_free_irq(VDEC_IRQ_1, (void *)vmpeg12_dec_id);
		stat &= ~STAT_ISR_REG;
	}

	if (stat & STAT_TIMER_ARM) {
		del_timer_sync(&recycle_timer);
		stat &= ~STAT_TIMER_ARM;
	}

	cancel_work_sync(&set_clk_work);
	if (stat & STAT_VF_HOOK) {
		if (fr_hint_status == VDEC_HINTED)
			vf_notify_receiver(PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_FR_END_HINT, NULL);
		fr_hint_status = VDEC_NO_NEED_HINT;

		vf_unreg_provider(&vmpeg_vf_prov);
		stat &= ~STAT_VF_HOOK;
	}

	amvdec_disable();
	if (ccbuf_phyAddress_is_remaped_nocache)
		codec_mm_unmap_phyaddr(ccbuf_phyAddress_virt);

	ccbuf_phyAddress_virt = NULL;
	ccbuf_phyAddress = 0;
	ccbuf_phyAddress_is_remaped_nocache = 0;
	vmpeg12_destroy_userdata_manager();

	if (mm_blk_handle) {
		decoder_bmmu_box_free(mm_blk_handle);
		mm_blk_handle = NULL;
	}
	amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg12 remove.\n");

	kfree(gvs);
	gvs = NULL;
	vdec = NULL;

#ifdef DUMP_USER_DATA
	if (user_data_buf) {
		show_user_data_buf();
		kfree(user_data_buf);
		user_data_buf = NULL;
	}
#endif

	return 0;
}

/****************************************/

static struct platform_driver amvdec_mpeg12_driver = {
	.probe = amvdec_mpeg12_probe,
	.remove = amvdec_mpeg12_remove,
#ifdef CONFIG_PM
	.suspend = amvdec_suspend,
	.resume = amvdec_resume,
#endif
	.driver = {
		.name = DRIVER_NAME,
	}
};

static struct codec_profile_t amvdec_mpeg12_profile = {
	.name = "mpeg12",
	.profile = ""
};


static struct mconfig mpeg12_configs[] = {
	MC_PU32("stat", &stat),
	MC_PU32("dec_control", &dec_control),
	MC_PU32("error_frame_skip_level", &error_frame_skip_level),
};
static struct mconfig_node mpeg12_node;


static int __init amvdec_mpeg12_driver_init_module(void)
{
	amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg12 module init\n");

	if (platform_driver_register(&amvdec_mpeg12_driver)) {
		amlog_level(LOG_LEVEL_ERROR,
			"failed to register amvdec_mpeg12 driver\n");
		return -ENODEV;
	}
	vcodec_profile_register(&amvdec_mpeg12_profile);
	INIT_REG_NODE_CONFIGS("media.decoder", &mpeg12_node,
		"mpeg12", mpeg12_configs, CONFIG_FOR_RW);
	return 0;
}

static void __exit amvdec_mpeg12_driver_remove_module(void)
{
	amlog_level(LOG_LEVEL_INFO, "amvdec_mpeg12 module remove.\n");

	platform_driver_unregister(&amvdec_mpeg12_driver);
}

/****************************************/
module_param(dec_control, uint, 0664);
MODULE_PARM_DESC(dec_control, "\n amvmpeg12 decoder control\n");
module_param(error_frame_skip_level, uint, 0664);
MODULE_PARM_DESC(error_frame_skip_level,
				 "\n amvdec_mpeg12 error_frame_skip_level\n");
module_param(force_first_i_ready, uint, 0664);
MODULE_PARM_DESC(force_first_i_ready, "\n amvmpeg12 force_first_i_ready\n");

module_init(amvdec_mpeg12_driver_init_module);
module_exit(amvdec_mpeg12_driver_remove_module);

MODULE_DESCRIPTION("AMLOGIC MPEG1/2 Video Decoder Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
