/*
 * drivers/amlogic/amports/vdec.c
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
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/sched/rt.h>
#include <linux/interrupt.h>
#include <linux/amlogic/amports/vformat.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/canvas/canvas.h>
#include <linux/amlogic/amports/vframe.h>
#include <linux/amlogic/amports/vframe_provider.h>
#include <linux/amlogic/amports/vframe_receiver.h>
#include <linux/amlogic/amports/ionvideo_ext.h>
#include <linux/amlogic/amports/vfm_ext.h>

#include "vdec_reg.h"
#include "vdec.h"
#ifdef CONFIG_MULTI_DEC
#include "vdec_profile.h"
#endif
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include "amports_priv.h"

#include "amports_config.h"
#include "amvdec.h"
#include "vvp9.h"
#include "vdec_input.h"

#include "arch/clk.h"
#include <linux/reset.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/codec_mm/codec_mm.h>

static DEFINE_MUTEX(vdec_mutex);

#define MC_SIZE (4096 * 4)
#define CMA_ALLOC_SIZE SZ_64M
#define MEM_NAME "vdec_prealloc"
static int inited_vcodec_num;
static int poweron_clock_level;
static int keep_vdec_mem;
static unsigned int debug_trace_num = 16 * 20;
static int step_mode;
static unsigned int clk_config;

static struct page *vdec_cma_page;
int vdec_mem_alloced_from_codec, delay_release;
static unsigned long reserved_mem_start, reserved_mem_end;
static int hevc_max_reset_count;

static DEFINE_SPINLOCK(vdec_spin_lock);

#define HEVC_TEST_LIMIT 100
#define GXBB_REV_A_MINOR 0xA

struct am_reg {
	char *name;
	int offset;
};

struct vdec_isr_context_s {
	int index;
	int irq;
	irq_handler_t dev_isr;
	irq_handler_t dev_threaded_isr;
	void *dev_id;
};

struct vdec_core_s {
	struct list_head connected_vdec_list;
	spinlock_t lock;

	atomic_t vdec_nr;
	struct vdec_s *vfm_vdec;
	struct vdec_s *active_vdec;
	struct platform_device *vdec_core_platform_device;
	struct device *cma_dev;
	unsigned long mem_start;
	unsigned long mem_end;

	struct semaphore sem;
	struct task_struct *thread;

	struct vdec_isr_context_s isr_context[VDEC_IRQ_MAX];
	int power_ref_count[VDEC_MAX];
};

static struct vdec_core_s *vdec_core;

unsigned long vdec_core_lock(struct vdec_core_s *core)
{
	unsigned long flags;

	spin_lock_irqsave(&core->lock, flags);

	return flags;
}

void vdec_core_unlock(struct vdec_core_s *core, unsigned long flags)
{
	spin_unlock_irqrestore(&core->lock, flags);
}

static int get_canvas(unsigned int index, unsigned int base)
{
	int start;
	int canvas_index = index * base;

	if ((base > 4) || (base == 0))
		return -1;

	if ((AMVDEC_CANVAS_START_INDEX + canvas_index + base - 1)
		<= AMVDEC_CANVAS_MAX1) {
		start = AMVDEC_CANVAS_START_INDEX + base * index;
	} else {
		canvas_index -= (AMVDEC_CANVAS_MAX1 -
			AMVDEC_CANVAS_START_INDEX + 1) / base * base;
		if (canvas_index <= AMVDEC_CANVAS_MAX2)
			start = canvas_index / base;
		else
			return -1;
	}

	if (base == 1) {
		return start;
	} else if (base == 2) {
		return ((start + 1) << 16) | ((start + 1) << 8) | start;
	} else if (base == 3) {
		return ((start + 2) << 16) | ((start + 1) << 8) | start;
	} else if (base == 4) {
		return (((start + 3) << 24) | (start + 2) << 16) |
			((start + 1) << 8) | start;
	}

	return -1;
}


int vdec_status(struct vdec_s *vdec, struct vdec_info *vstatus)
{
	if (vdec->dec_status)
		return vdec->dec_status(vdec, vstatus);

	return -1;
}

int vdec_set_trickmode(struct vdec_s *vdec, unsigned long trickmode)
{
	int r;

	if (vdec->set_trickmode) {
		r = vdec->set_trickmode(vdec, trickmode);

		if ((r == 0) && (vdec->slave) && (vdec->slave->set_trickmode))
			r = vdec->slave->set_trickmode(vdec->slave,
				trickmode);
	}

	return -1;
}

void  vdec_count_info(struct vdec_info *vs, unsigned int err,
	unsigned int offset)
{
	if (err)
		vs->error_frame_count++;
	if (offset) {
		if (0 == vs->frame_count) {
			vs->offset = 0;
			vs->samp_cnt = 0;
		}
		vs->frame_data = offset > vs->total_data ?
			offset - vs->total_data : vs->total_data - offset;
		vs->total_data = offset;
		if (vs->samp_cnt < 96000 * 2) { /* 2s */
			if (0 == vs->samp_cnt)
				vs->offset = offset;
			vs->samp_cnt += vs->frame_dur;
		} else {
			vs->bit_rate = (offset - vs->offset) / 2;
			/*pr_info("bitrate : %u\n",vs->bit_rate);*/
			vs->samp_cnt = 0;
		}
		vs->frame_count++;
	}
	/*pr_info("size : %u, offset : %u, dur : %u, cnt : %u\n",
		vs->offset,offset,vs->frame_dur,vs->samp_cnt);*/
	return;
}

/*
 clk_config:
 0:default
 1:no gp0_pll;
 2:always used gp0_pll;
 >=10:fixed n M clk;
 == 100 , 100M clks;
*/
unsigned int get_vdec_clk_config_settings(void)
{
	return clk_config;
}
void update_vdec_clk_config_settings(unsigned int config)
{
	clk_config = config;
}

static bool hevc_workaround_needed(void)
{
	return (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) &&
		(get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR)
			== GXBB_REV_A_MINOR);
}

struct device *get_codec_cma_device(void)
{
	return vdec_core->cma_dev;
}

#ifdef CONFIG_MULTI_DEC
static const char * const vdec_device_name[] = {
	"amvdec_mpeg12",     "ammvdec_mpeg12",
	"amvdec_mpeg4",      "ammvdec_mpeg4",
	"amvdec_h264",       "ammvdec_h264",
	"amvdec_mjpeg",      "ammvdec_mjpeg",
	"amvdec_real",       "ammvdec_real",
	"amjpegdec",         "ammjpegdec",
	"amvdec_vc1",        "ammvdec_vc1",
	"amvdec_avs",        "ammvdec_avs",
	"amvdec_yuv",        "ammvdec_yuv",
	"amvdec_h264mvc",    "ammvdec_h264mvc",
	"amvdec_h264_4k2k",  "ammvdec_h264_4k2k",
	"amvdec_h265",       "ammvdec_h265",
	"amvenc_avc",        "amvenc_avc",
	"jpegenc",           "jpegenc",
	"amvdec_vp9",        "ammvdec_vp9"
};

static int vdec_default_buf_size[] = {
	32, 32, /*"amvdec_mpeg12",*/
	32, 0,  /*"amvdec_mpeg4",*/
	48, 0,  /*"amvdec_h264",*/
	32, 32, /*"amvdec_mjpeg",*/
	32, 32, /*"amvdec_real",*/
	32, 32, /*"amjpegdec",*/
	32, 32, /*"amvdec_vc1",*/
	32, 32, /*"amvdec_avs",*/
	32, 32, /*"amvdec_yuv",*/
	64, 64, /*"amvdec_h264mvc",*/
	64, 64, /*"amvdec_h264_4k2k", else alloc on decoder*/
	48, 48, /*"amvdec_h265", else alloc on decoder*/
	0, 0,   /* avs encoder */
	0, 0,   /* jpg encoder */
#ifdef VP9_10B_MMU
	24, 24, /*"amvdec_vp9", else alloc on decoder*/
#else
	32, 32,
#endif
	0
};

#else

static const char * const vdec_device_name[] = {
	"amvdec_mpeg12",
	"amvdec_mpeg4",
	"amvdec_h264",
	"amvdec_mjpeg",
	"amvdec_real",
	"amjpegdec",
	"amvdec_vc1",
	"amvdec_avs",
	"amvdec_yuv",
	"amvdec_h264mvc",
	"amvdec_h264_4k2k",
	"amvdec_h265",
	"amvenc_avc",
	"jpegenc",
	"amvdec_vp9"
};

static int vdec_default_buf_size[] = {
	32, /*"amvdec_mpeg12",*/
	32, /*"amvdec_mpeg4",*/
	48, /*"amvdec_h264",*/
	32, /*"amvdec_mjpeg",*/
	32, /*"amvdec_real",*/
	32, /*"amjpegdec",*/
	32, /*"amvdec_vc1",*/
	32, /*"amvdec_avs",*/
	32, /*"amvdec_yuv",*/
	64, /*"amvdec_h264mvc",*/
	64, /*"amvdec_h264_4k2k", else alloc on decoder*/
	48, /*"amvdec_h265", else alloc on decoder*/
	0,  /* avs encoder */
	0,  /* jpg encoder */
#ifdef VP9_10B_MMU
	24, /*"amvdec_vp9", else alloc on decoder*/
#else
	32,
#endif
};
#endif

int vdec_set_decinfo(struct vdec_s *vdec, struct dec_sysinfo *p)
{
	if (copy_from_user((void *)&vdec->sys_info_store, (void *)p,
		sizeof(struct dec_sysinfo)))
		return -EFAULT;

	return 0;
}

/* construct vdec strcture */
struct vdec_s *vdec_create(struct stream_port_s *port,
			struct vdec_s *master)
{
	struct vdec_s *vdec;
	int type = VDEC_TYPE_SINGLE;

	if (port->type & PORT_TYPE_DECODER_SCHED)
		type = (port->type & PORT_TYPE_FRAME) ?
			VDEC_TYPE_FRAME_BLOCK :
			VDEC_TYPE_STREAM_PARSER;

	vdec = vzalloc(sizeof(struct vdec_s));

	/* TBD */
	if (vdec) {
		vdec->magic = 0x43454456;
		vdec->id = 0;
		vdec->type = type;
		vdec->port = port;
		vdec->sys_info = &vdec->sys_info_store;

		INIT_LIST_HEAD(&vdec->list);

		vdec_input_init(&vdec->input, vdec);

		atomic_inc(&vdec_core->vdec_nr);

		if (master) {
			vdec->master = master;
			master->slave = vdec;
			master->sched = 1;
		}
	}

	pr_info("vdec_create instance %p, total %d\n", vdec,
		atomic_read(&vdec_core->vdec_nr));

	return vdec;
}

int vdec_set_format(struct vdec_s *vdec, int format)
{
	vdec->format = format;

	if (vdec->slave)
		vdec->slave->format = format;

	return 0;
}

int vdec_set_pts(struct vdec_s *vdec, u32 pts)
{
	vdec->pts = pts;
	vdec->pts_valid = true;
	return 0;
}

int vdec_set_pts64(struct vdec_s *vdec, u64 pts64)
{
	vdec->pts64 = pts64;
	vdec->pts_valid = true;
	return 0;
}

void vdec_set_status(struct vdec_s *vdec, int status)
{
	vdec->status = status;
}

void vdec_set_next_status(struct vdec_s *vdec, int status)
{
	vdec->next_status = status;
}

int vdec_set_video_path(struct vdec_s *vdec, int video_path)
{
	vdec->frame_base_video_path = video_path;
	return 0;
}

/* add frame data to input chain */
int vdec_write_vframe(struct vdec_s *vdec, const char *buf, size_t count)
{
	return vdec_input_add_frame(&vdec->input, buf, count);
}

/* get next frame from input chain */
/* THE VLD_FIFO is 512 bytes and Video buffer level
 * empty interrupt is set to 0x80 bytes threshold
 */
#define VLD_PADDING_SIZE 1024
#define HEVC_PADDING_SIZE (1024*16)
#define FIFO_ALIGN 8
int vdec_prepare_input(struct vdec_s *vdec, struct vframe_chunk_s **p)
{
	struct vdec_input_s *input = (vdec->master) ?
		&vdec->master->input : &vdec->input;
	struct vframe_chunk_s *chunk = NULL;
	struct vframe_block_list_s *block = NULL;
	int dummy;

	/* full reset to HW input */
	if (input->target == VDEC_INPUT_TARGET_VLD) {
		WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);

		/* reset VLD fifo for all vdec */
		WRITE_VREG(DOS_SW_RESET0, (1<<5) | (1<<4) | (1<<3));
		WRITE_VREG(DOS_SW_RESET0, 0);

		dummy = READ_MPEG_REG(RESET0_REGISTER);
		WRITE_VREG(POWER_CTL_VLD, 1 << 4);
	} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
#if 0
		/*move to driver*/
		if (input_frame_based(input))
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
#endif
	}

	/* setup HW decoder input buffer (VLD context)
	 * based on input->type and input->target
	 */
	if (input_frame_based(input)) {
		chunk = vdec_input_next_chunk(&vdec->input);

		if (chunk == NULL) {
			*p = NULL;
			return -1;
		}

		block = chunk->block;

		if (input->target == VDEC_INPUT_TARGET_VLD) {
			WRITE_VREG(VLD_MEM_VIFIFO_START_PTR, block->start);
			WRITE_VREG(VLD_MEM_VIFIFO_END_PTR, block->start +
					block->size - 8);
			WRITE_VREG(VLD_MEM_VIFIFO_CURR_PTR,
					round_down(block->start + chunk->offset,
						FIFO_ALIGN));

			WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 1);
			WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);

			/* set to manual mode */
			WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 2);
			WRITE_VREG(VLD_MEM_VIFIFO_RP,
					round_down(block->start + chunk->offset,
						FIFO_ALIGN));
			dummy = chunk->offset + chunk->size +
				VLD_PADDING_SIZE;
			if (dummy >= block->size)
				dummy -= block->size;
			WRITE_VREG(VLD_MEM_VIFIFO_WP,
				round_down(block->start + dummy, FIFO_ALIGN));

			WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 3);
			WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 2);

			WRITE_VREG(VLD_MEM_VIFIFO_CONTROL,
				(0x11 << 16) | (1<<10) | (7<<3));

		} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
			WRITE_VREG(HEVC_STREAM_START_ADDR, block->start);
			WRITE_VREG(HEVC_STREAM_END_ADDR, block->start +
					block->size);
			WRITE_VREG(HEVC_STREAM_RD_PTR, block->start +
					chunk->offset);
			dummy = chunk->offset + chunk->size +
					HEVC_PADDING_SIZE;
			if (dummy >= block->size)
				dummy -= block->size;
			WRITE_VREG(HEVC_STREAM_WR_PTR,
				round_down(block->start + dummy, FIFO_ALIGN));

			/* set endian */
			SET_VREG_MASK(HEVC_STREAM_CONTROL, 7 << 4);
		}

		*p = chunk;
		return chunk->size;

	} else {
		u32 rp = 0, wp = 0, fifo_len = 0;
		int size;
		/* stream based */
		if (input->swap_valid) {
			if (input->target == VDEC_INPUT_TARGET_VLD) {
				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);

				/* restore read side */
				WRITE_VREG(VLD_MEM_SWAP_ADDR,
					page_to_phys(input->swap_page));
				WRITE_VREG(VLD_MEM_SWAP_CTL, 1);

				while (READ_VREG(VLD_MEM_SWAP_CTL) & (1<<7))
					;
				WRITE_VREG(VLD_MEM_SWAP_CTL, 0);

				/* restore wrap count */
				WRITE_VREG(VLD_MEM_VIFIFO_WRAP_COUNT,
					input->stream_cookie);

				rp = READ_VREG(VLD_MEM_VIFIFO_RP);
				fifo_len = READ_VREG(VLD_MEM_VIFIFO_LEVEL);

				/* enable */
				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL,
					(0x11 << 16) | (1<<10));

				/* update write side */
				WRITE_VREG(VLD_MEM_VIFIFO_WP,
					READ_MPEG_REG(PARSER_VIDEO_WP));

				wp = READ_VREG(VLD_MEM_VIFIFO_WP);
			} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
				SET_VREG_MASK(HEVC_STREAM_CONTROL, 1);

				/* restore read side */
				WRITE_VREG(HEVC_STREAM_SWAP_ADDR,
					page_to_phys(input->swap_page));
				WRITE_VREG(HEVC_STREAM_SWAP_CTRL, 1);

				while (READ_VREG(HEVC_STREAM_SWAP_CTRL)
					& (1<<7))
					;
				WRITE_VREG(HEVC_STREAM_SWAP_CTRL, 0);

				/* restore stream offset */
				WRITE_VREG(HEVC_SHIFT_BYTE_COUNT,
					input->stream_cookie);

				rp = READ_VREG(HEVC_STREAM_RD_PTR);
				fifo_len = (READ_VREG(HEVC_STREAM_FIFO_CTL)
						>> 16) & 0x7f;


				/* enable */

				/* update write side */
				WRITE_VREG(HEVC_STREAM_WR_PTR,
					READ_MPEG_REG(PARSER_VIDEO_WP));

				wp = READ_VREG(HEVC_STREAM_WR_PTR);
			}

		} else {
			if (input->target == VDEC_INPUT_TARGET_VLD) {
				WRITE_VREG(VLD_MEM_VIFIFO_START_PTR,
					input->start);
				WRITE_VREG(VLD_MEM_VIFIFO_END_PTR,
					input->start + input->size - 8);
				WRITE_VREG(VLD_MEM_VIFIFO_CURR_PTR,
					input->start);

				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 1);
				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 0);

				/* set to manual mode */
				WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, 2);
				WRITE_VREG(VLD_MEM_VIFIFO_RP, input->start);
				WRITE_VREG(VLD_MEM_VIFIFO_WP,
					READ_MPEG_REG(PARSER_VIDEO_WP));

				rp = READ_VREG(VLD_MEM_VIFIFO_RP);

				/* enable */
				WRITE_VREG(VLD_MEM_VIFIFO_CONTROL,
					(0x11 << 16) | (1<<10));

				wp = READ_VREG(VLD_MEM_VIFIFO_WP);

			} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
				WRITE_VREG(HEVC_STREAM_START_ADDR,
					input->start);
				WRITE_VREG(HEVC_STREAM_END_ADDR,
					input->start + input->size);
				WRITE_VREG(HEVC_STREAM_RD_PTR,
					input->start);
				WRITE_VREG(HEVC_STREAM_WR_PTR,
					READ_MPEG_REG(PARSER_VIDEO_WP));

				rp = READ_VREG(HEVC_STREAM_RD_PTR);
				wp = READ_VREG(HEVC_STREAM_WR_PTR);
				fifo_len = (READ_VREG(HEVC_STREAM_FIFO_CTL)
						>> 16) & 0x7f;

				/* enable */
			}
		}
		*p = NULL;
		if (wp >= rp)
			size = wp - rp + fifo_len;
		else
			size = wp + input->size - rp + fifo_len;
		if (size < 0) {
			pr_info("%s error: input->size %x wp %x rp %x fifo_len %x => size %x\r\n",
				__func__, input->size, wp, rp, fifo_len, size);
			size = 0;
		}
		return size;
	}
}

void vdec_enable_input(struct vdec_s *vdec)
{
	struct vdec_input_s *input = &vdec->input;

	if (vdec->status != VDEC_STATUS_ACTIVE)
		return;

	if (input->target == VDEC_INPUT_TARGET_VLD)
		SET_VREG_MASK(VLD_MEM_VIFIFO_CONTROL, (1<<2) | (1<<1));
	else if (input->target == VDEC_INPUT_TARGET_HEVC) {
		SET_VREG_MASK(HEVC_STREAM_CONTROL, 1);
		if (vdec_stream_based(vdec))
			CLEAR_VREG_MASK(HEVC_STREAM_CONTROL, 7 << 4);
		else
			SET_VREG_MASK(HEVC_STREAM_CONTROL, 7 << 4);
		SET_VREG_MASK(HEVC_STREAM_FIFO_CTL, (1<<29));
	}
}

void vdec_set_flag(struct vdec_s *vdec, u32 flag)
{
	vdec->flag = flag;
}

void vdec_set_next_sched(struct vdec_s *vdec, struct vdec_s *next_vdec)
{
	if (vdec && next_vdec) {
		vdec->sched = 0;
		next_vdec->sched = 1;
	}
}

void vdec_vframe_dirty(struct vdec_s *vdec, struct vframe_chunk_s *chunk)
{
	if (chunk)
		chunk->flag |= VFRAME_CHUNK_FLAG_CONSUMED;

	if (vdec_stream_based(vdec)) {
		if (vdec->slave &&
			((vdec->slave->flag &
			VDEC_FLAG_INPUT_KEEP_CONTEXT) == 0)) {
			vdec->input.swap_needed = false;
		} else
			vdec->input.swap_needed = true;

		if (vdec->input.target == VDEC_INPUT_TARGET_VLD) {
			WRITE_MPEG_REG(PARSER_VIDEO_RP,
				READ_VREG(VLD_MEM_VIFIFO_RP));
			WRITE_VREG(VLD_MEM_VIFIFO_WP,
				READ_MPEG_REG(PARSER_VIDEO_WP));
		} else if (vdec->input.target == VDEC_INPUT_TARGET_HEVC) {
			WRITE_MPEG_REG(PARSER_VIDEO_RP,
				READ_VREG(HEVC_STREAM_RD_PTR));
			WRITE_VREG(HEVC_STREAM_WR_PTR,
				READ_MPEG_REG(PARSER_VIDEO_WP));
		}
	}
}

void vdec_save_input_context(struct vdec_s *vdec)
{
	struct vdec_input_s *input = (vdec->master) ?
		&vdec->master->input : &vdec->input;

#ifdef CONFIG_MULTI_DEC
	vdec_profile(vdec, VDEC_PROFILE_EVENT_SAVE_INPUT);
#endif

	if (input->target == VDEC_INPUT_TARGET_VLD)
		WRITE_VREG(VLD_MEM_VIFIFO_CONTROL, 1<<15);

	if (input_stream_based(input) && (input->swap_needed)) {
		if (input->target == VDEC_INPUT_TARGET_VLD) {
			WRITE_VREG(VLD_MEM_SWAP_ADDR,
				page_to_phys(input->swap_page));
			WRITE_VREG(VLD_MEM_SWAP_CTL, 3);
			while (READ_VREG(VLD_MEM_SWAP_CTL) & (1<<7))
				;
			WRITE_VREG(VLD_MEM_SWAP_CTL, 0);
			vdec->input.stream_cookie =
				READ_VREG(VLD_MEM_VIFIFO_WRAP_COUNT);
		} else if (input->target == VDEC_INPUT_TARGET_HEVC) {
			WRITE_VREG(HEVC_STREAM_SWAP_ADDR,
				page_to_phys(input->swap_page));
			WRITE_VREG(HEVC_STREAM_SWAP_CTRL, 3);

			while (READ_VREG(HEVC_STREAM_SWAP_CTRL) & (1<<7))
				;
			WRITE_VREG(HEVC_STREAM_SWAP_CTRL, 0);

			vdec->input.stream_cookie =
				READ_VREG(HEVC_SHIFT_BYTE_COUNT);
		}

		input->swap_valid = true;

		if (input->target == VDEC_INPUT_TARGET_VLD)
			WRITE_MPEG_REG(PARSER_VIDEO_RP,
				READ_VREG(VLD_MEM_VIFIFO_RP));
		else
			WRITE_MPEG_REG(PARSER_VIDEO_RP,
				READ_VREG(HEVC_STREAM_RD_PTR));
	}
}

void vdec_clean_input(struct vdec_s *vdec)
{
	struct vdec_input_s *input = &vdec->input;

	while (!list_empty(&input->vframe_chunk_list)) {
		struct vframe_chunk_s *chunk =
			vdec_input_next_chunk(input);
		if (chunk->flag & VFRAME_CHUNK_FLAG_CONSUMED)
			vdec_input_release_chunk(input, chunk);
		else
			break;
	}
	vdec_save_input_context(vdec);
}

const char *vdec_status_str(struct vdec_s *vdec)
{
	switch (vdec->status) {
	case VDEC_STATUS_UNINITIALIZED:
		return "VDEC_STATUS_UNINITIALIZED";
	case VDEC_STATUS_DISCONNECTED:
		return "VDEC_STATUS_DISCONNECTED";
	case VDEC_STATUS_CONNECTED:
		return "VDEC_STATUS_CONNECTED";
	case VDEC_STATUS_ACTIVE:
		return "VDEC_STATUS_ACTIVE";
	default:
		return "invalid status";
	}
}

const char *vdec_type_str(struct vdec_s *vdec)
{
	switch (vdec->type) {
	case VDEC_TYPE_SINGLE:
		return "VDEC_TYPE_SINGLE";
	case VDEC_TYPE_STREAM_PARSER:
		return "VDEC_TYPE_STREAM_PARSER";
	case VDEC_TYPE_FRAME_BLOCK:
		return "VDEC_TYPE_FRAME_BLOCK";
	case VDEC_TYPE_FRAME_CIRCULAR:
		return "VDEC_TYPE_FRAME_CIRCULAR";
	default:
		return "VDEC_TYPE_INVALID";
	}
}

const char *vdec_device_name_str(struct vdec_s *vdec)
{
	return vdec_device_name[vdec->format * 2 + 1];
}

void walk_vdec_core_list(char *s)
{
	struct vdec_s *vdec;
	struct vdec_core_s *core = vdec_core;
	unsigned long flags;

	pr_info("%s --->\n", s);

	flags = vdec_core_lock(vdec_core);

	if (list_empty(&core->connected_vdec_list)) {
		pr_info("connected vdec list empty\n");
	} else {
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			pr_info("\tvdec (%p), status = %s\n", vdec,
				vdec_status_str(vdec));
		}
	}

	vdec_core_unlock(vdec_core, flags);
}

/* insert vdec to vdec_core for scheduling,
 * for dual running decoders, connect/disconnect always runs in pairs
 */
int vdec_connect(struct vdec_s *vdec)
{
	unsigned long flags;

	if (vdec->status != VDEC_STATUS_DISCONNECTED)
		return 0;

	vdec_set_status(vdec, VDEC_STATUS_CONNECTED);
	vdec_set_next_status(vdec, VDEC_STATUS_CONNECTED);

	init_completion(&vdec->inactive_done);

	if (vdec->slave) {
		vdec_set_status(vdec->slave, VDEC_STATUS_CONNECTED);
		vdec_set_next_status(vdec->slave, VDEC_STATUS_CONNECTED);

		init_completion(&vdec->slave->inactive_done);
	}

	flags = vdec_core_lock(vdec_core);

	list_add_tail(&vdec->list, &vdec_core->connected_vdec_list);

	if (vdec->slave) {
		list_add_tail(&vdec->slave->list,
			&vdec_core->connected_vdec_list);
	}

	vdec_core_unlock(vdec_core, flags);

	up(&vdec_core->sem);

	return 0;
}

/* remove vdec from vdec_core scheduling */
int vdec_disconnect(struct vdec_s *vdec)
{
#ifdef CONFIG_MULTI_DEC
	vdec_profile(vdec, VDEC_PROFILE_EVENT_DISCONNECT);
#endif

	if ((vdec->status != VDEC_STATUS_CONNECTED) &&
		(vdec->status != VDEC_STATUS_ACTIVE)) {
		return 0;
	}

	/* when a vdec is under the management of scheduler
	 * the status change will only be from vdec_core_thread
	 */
	vdec_set_next_status(vdec, VDEC_STATUS_DISCONNECTED);

	if (vdec->slave)
		vdec_set_next_status(vdec->slave, VDEC_STATUS_DISCONNECTED);
	else if (vdec->master)
		vdec_set_next_status(vdec->master, VDEC_STATUS_DISCONNECTED);

	up(&vdec_core->sem);

	wait_for_completion(&vdec->inactive_done);

	if (vdec->slave)
		wait_for_completion(&vdec->slave->inactive_done);
	else if (vdec->master)
		wait_for_completion(&vdec->master->inactive_done);

	return 0;
}

/* release vdec structure */
int vdec_destroy(struct vdec_s *vdec)
{
	if (!vdec->master)
		vdec_input_release(&vdec->input);

#ifdef CONFIG_MULTI_DEC
	vdec_profile_flush(vdec);
#endif

	vfree(vdec);

	atomic_dec(&vdec_core->vdec_nr);

	return 0;
}

/*
 * Only support time sliced decoding for frame based input,
 * so legacy decoder can exist with time sliced decoder.
 */
static const char *get_dev_name(bool use_legacy_vdec, int format)
{
#ifdef CONFIG_MULTI_DEC
	if (use_legacy_vdec)
		return vdec_device_name[format * 2];
	else
		return vdec_device_name[format * 2 + 1];
#else
	return vdec_device_name[format];
#endif
}

/* register vdec_device
 * create output, vfm or create ionvideo output
 */
s32 vdec_init(struct vdec_s *vdec, int is_4k)
{
	int r = 0;
	struct vdec_s *p = vdec;
	int retry_num = 0;
	int more_buffers = 0;
	const char *dev_name;
	int tvp_flags;
	tvp_flags = codec_mm_video_tvp_enabled() ?
		CODEC_MM_FLAGS_TVP : 0;
	/*TODO.changed to vdec-tvp flags*/

	if (is_4k && vdec->format < VFORMAT_H264) {
		/*old decoder don't support 4k
			but size is bigger;
			clear 4k flag, and used more buffers;
		*/
		more_buffers = 1;
		is_4k = 0;
	}

	dev_name = get_dev_name(vdec_single(vdec), vdec->format);

	if (dev_name == NULL)
		return -ENODEV;

	pr_info("vdec_init, dev_name:%s, vdec_type=%s\n",
		dev_name, vdec_type_str(vdec));

	/* todo: VFM patch control should be configurable,
	 * for now all stream based input uses default VFM path.
	 */
	if (vdec_stream_based(vdec) && !vdec_dual(vdec)) {
		if (vdec_core->vfm_vdec == NULL) {
			pr_info("vdec_init set vfm decoder %p\n", vdec);
			vdec_core->vfm_vdec = vdec;
		} else {
			pr_info("vdec_init vfm path busy.\n");
			return -EBUSY;
		}
	}

	if (vdec_single(vdec) &&
		((vdec->format == VFORMAT_H264_4K2K) ||
		(vdec->format == VFORMAT_HEVC && is_4k))) {
		try_free_keep_video(0);
	}

	/*when blackout_policy was set, vdec would not free cma buffer, if
		current vformat require larger buffer size than current
		buf size, reallocated it
	*/
	if (vdec_single(vdec) &&
		((vdec_core->mem_start != vdec_core->mem_end &&
			vdec_core->mem_end - vdec_core->mem_start + 1 <
			vdec_default_buf_size[vdec->format] * SZ_1M))) {
#ifdef CONFIG_MULTI_DEC
		pr_info("current vdec size %ld, vformat %d need size %d\n",
			vdec_core->mem_end - vdec_core->mem_start,
			vdec->format,
			vdec_default_buf_size[vdec->format * 2] * SZ_1M);
#else
		pr_info("current vdec size %ld, vformat %d need size %d\n",
			vdec_core->mem_end - vdec_core->mem_start,
			vdec->format,
			vdec_default_buf_size[vdec->format] * SZ_1M);
#endif
		try_free_keep_video(0);
		vdec_free_cmabuf();
	}

	mutex_lock(&vdec_mutex);
	inited_vcodec_num++;
	mutex_unlock(&vdec_mutex);

	vdec_input_set_type(&vdec->input, vdec->type,
			(vdec->format == VFORMAT_HEVC ||
			vdec->format == VFORMAT_VP9) ?
				VDEC_INPUT_TARGET_HEVC :
				VDEC_INPUT_TARGET_VLD);

	p->cma_dev = vdec_core->cma_dev;
	p->get_canvas = get_canvas;
	/* todo */
	if (!vdec_dual(vdec))
		p->use_vfm_path = vdec_stream_based(vdec);

	if (vdec_single(vdec)) {
		pr_info("vdec_dev_reg.mem[0x%lx -- 0x%lx]\n",
			vdec_core->mem_start,
			vdec_core->mem_end);
		p->mem_start = vdec_core->mem_start;
		p->mem_end = vdec_core->mem_end;
	}

	/* allocate base memory for decoder instance */
	while ((p->mem_start == p->mem_end) && (vdec_single(vdec))) {
		int alloc_size;

#ifdef CONFIG_MULTI_DEC
		alloc_size =
			vdec_default_buf_size[vdec->format * 2 + 1]
			* SZ_1M;
#else
		alloc_size = vdec_default_buf_size[vdec->format] * SZ_1M;
#endif
		if (alloc_size == 0)
			break;/*alloc end*/
		if (is_4k) {
			/*used 264 4k's setting for 265.*/
#ifdef CONFIG_MULTI_DEC
			int m4k_size =
				vdec_default_buf_size[VFORMAT_H264_4K2K * 2] *
				SZ_1M;
#else
			int m4k_size =
				vdec_default_buf_size[VFORMAT_H264_4K2K] *
				SZ_1M;
#endif
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXTVBB)
				m4k_size = 32 * SZ_1M;
			if ((m4k_size > 0) && (m4k_size < 200 * SZ_1M))
				alloc_size = m4k_size;

#ifdef VP9_10B_MMU
			if ((vdec->format == VFORMAT_VP9) &&
				(get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)) {
#ifdef CONFIG_MULTI_DEC
				if (p->use_vfm_path)
					alloc_size =
					vdec_default_buf_size[VFORMAT_VP9 * 2]
					* SZ_1M;
				else
					alloc_size =
					vdec_default_buf_size[VFORMAT_VP9
						* 2 + 1] * SZ_1M;

#else
				alloc_size =
				vdec_default_buf_size[VFORMAT_VP9] * SZ_1M;
#endif
			}
#endif
		} else if (more_buffers) {
			alloc_size = alloc_size + 16 * SZ_1M;
		}

		if ((vdec->format == VFORMAT_HEVC)
			&& get_mmu_mode()
			&& (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)) {
#ifdef CONFIG_MULTI_DEC
				if (p->use_vfm_path)
					alloc_size = 33 * SZ_1M;
				else
					alloc_size = 33 * SZ_1M;
#else
				alloc_size = 33 * SZ_1M;
#endif
		}

		if ((vdec->format == VFORMAT_H264)
			&& (get_cpu_type() >= MESON_CPU_MAJOR_ID_GXL)
			&& codec_mm_get_total_size() <= 80 * SZ_1M) {
#ifdef CONFIG_MULTI_DEC
			if (p->use_vfm_path)
				alloc_size = 32 * SZ_1M;
			else
				alloc_size = 32 * SZ_1M;
#else
			alloc_size = 32 * SZ_1M;
#endif
		}


		p->mem_start = codec_mm_alloc_for_dma(MEM_NAME,
			alloc_size / PAGE_SIZE, 4 + PAGE_SHIFT,
			CODEC_MM_FLAGS_CMA_CLEAR | CODEC_MM_FLAGS_CPU |
			CODEC_MM_FLAGS_FOR_VDECODER | tvp_flags);
		if (!p->mem_start) {
			if (retry_num < 1) {
				pr_err("vdec base CMA allocation failed,try again\\n");
				retry_num++;
				try_free_keep_video(0);
				continue;/*retry alloc*/
			}
			pr_err("vdec base CMA allocation failed.\n");

			mutex_lock(&vdec_mutex);
			inited_vcodec_num--;
			mutex_unlock(&vdec_mutex);

			return -ENOMEM;
		}

		p->mem_end = p->mem_start + alloc_size - 1;
		pr_info("vdec base memory alloced [%p -- %p]\n",
			(void *)p->mem_start,
			(void *)p->mem_end);

		break;/*alloc end*/
	}

	if (vdec_single(vdec)) {
		vdec_core->mem_start = p->mem_start;
		vdec_core->mem_end = p->mem_end;
		vdec_mem_alloced_from_codec = 1;
	}

/*alloc end:*/
	/* vdec_dev_reg.flag = 0; */

	p->dev =
		platform_device_register_data(
				&vdec_core->vdec_core_platform_device->dev,
				dev_name,
				PLATFORM_DEVID_AUTO,
				&p, sizeof(struct vdec_s *));

	if (IS_ERR(p->dev)) {
		r = PTR_ERR(p->dev);
		pr_err("vdec: Decoder device %s register failed (%d)\n",
			dev_name, r);

		mutex_lock(&vdec_mutex);
		inited_vcodec_num--;
		mutex_unlock(&vdec_mutex);

		goto error;
	}

	if ((p->type == VDEC_TYPE_FRAME_BLOCK) && (p->run == NULL)) {
		r = -ENODEV;
		pr_err("vdec: Decoder device not handled (%s)\n", dev_name);

		mutex_lock(&vdec_mutex);
		inited_vcodec_num--;
		mutex_unlock(&vdec_mutex);

		goto error;
	}

	if (p->use_vfm_path) {
		vdec->vf_receiver_inst = -1;
	} else if (!vdec_dual(vdec)) {
		/* create IONVIDEO instance and connect decoder's
		 * vf_provider interface to it
		 */
		if (p->type != VDEC_TYPE_FRAME_BLOCK) {
			r = -ENODEV;
			pr_err("vdec: Incorrect decoder type\n");

			mutex_lock(&vdec_mutex);
			inited_vcodec_num--;
			mutex_unlock(&vdec_mutex);

			goto error;
		}
		if (p->frame_base_video_path == FRAME_BASE_PATH_IONVIDEO) {
#ifdef CONFIG_AMLOGIC_IONVIDEO
#if 1
			r = ionvideo_alloc_map(&vdec->vf_receiver_name,
					&vdec->vf_receiver_inst);
#else
			/*
			 * temporarily just use decoder instance ID as iondriver
			 * ID to solve OMX iondriver instance number check time
			 * sequence only the limitation is we can NOT mix
			 * different video decoders since same ID will be used
			 * for differentdecoder formats.
			 */
			vdec->vf_receiver_inst = p->dev->id;
			r = ionvideo_assign_map(&vdec->vf_receiver_name,
					&vdec->vf_receiver_inst);
#endif
			if (r < 0) {
				pr_err("IonVideo frame receiver allocation failed.\n");

				mutex_lock(&vdec_mutex);
				inited_vcodec_num--;
				mutex_unlock(&vdec_mutex);

				goto error;
			}
#endif

			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name,
				vdec->vf_receiver_name);
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"%s-%s", vdec->vf_provider_name,
				vdec->vf_receiver_name);

		} else if (p->frame_base_video_path ==
				FRAME_BASE_PATH_AMLVIDEO_AMVIDEO) {
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name,
				"amlvideo.0 amvideo");
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"%s-%s", vdec->vf_provider_name,
				"amlvideo.0 amvideo");
		} else if (p->frame_base_video_path ==
				FRAME_BASE_PATH_AMLVIDEO1_AMVIDEO2) {
			snprintf(vdec->vfm_map_chain, VDEC_MAP_NAME_SIZE,
				"%s %s", vdec->vf_provider_name,
				"ppmgr amlvideo.1 amvide2");
			snprintf(vdec->vfm_map_id, VDEC_MAP_NAME_SIZE,
				"%s-%s", vdec->vf_provider_name,
				"ppmgr amlvideo.1 amvide2");
		}

		if (vfm_map_add(vdec->vfm_map_id,
					vdec->vfm_map_chain) < 0) {
			r = -ENOMEM;
			pr_err("Decoder pipeline map creation failed %s.\n",
				vdec->vfm_map_id);
			vdec->vfm_map_id[0] = 0;

			mutex_lock(&vdec_mutex);
			inited_vcodec_num--;
			mutex_unlock(&vdec_mutex);

			goto error;
		}

		pr_info("vfm map %s created\n", vdec->vfm_map_id);

		/* assume IONVIDEO driver already have a few vframe_receiver
		 * registered.
		 * 1. Call iondriver function to allocate a IONVIDEO path and
		 *    provide receiver's name and receiver op.
		 * 2. Get decoder driver's provider name from driver instance
		 * 3. vfm_map_add(name, "<decoder provider name>
		 *    <iondriver receiver name>"), e.g.
		 *    vfm_map_add("vdec_ion_map_0", "mpeg4_0 iondriver_1");
		 * 4. vf_reg_provider and vf_reg_receiver
		 * Note: the decoder provider's op uses vdec as op_arg
		 *       the iondriver receiver's op uses iondev device as
		 *       op_arg
		 */
	}

	if (!vdec_single(vdec)) {
		vf_reg_provider(&p->vframe_provider);

		vf_notify_receiver(p->vf_provider_name,
			VFRAME_EVENT_PROVIDER_START,
			vdec);
	}

	pr_info("vdec_init, vf_provider_name = %s\n", p->vf_provider_name);

	/* vdec is now ready to be active */
	vdec_set_status(vdec, VDEC_STATUS_DISCONNECTED);

	return 0;

error:
	return r;
}

/* vdec_create/init/release/destroy are applied to both dual running decoders
 */
void vdec_release(struct vdec_s *vdec)
{
	vdec_disconnect(vdec);

	if (vdec->vframe_provider.name)
		vf_unreg_provider(&vdec->vframe_provider);

	if (vdec_core->vfm_vdec == vdec)
		vdec_core->vfm_vdec = NULL;

	if (vdec->vf_receiver_inst >= 0) {
		if (vdec->vfm_map_id[0]) {
			vfm_map_remove(vdec->vfm_map_id);
			vdec->vfm_map_id[0] = 0;
		}

		/* vf_receiver_inst should be > 0 since 0 is
		 * for either un-initialized vdec or a ionvideo
		 * instance reserved for legacy path.
		 */
#ifdef CONFIG_AMLOGIC_IONVIDEO
		ionvideo_release_map(vdec->vf_receiver_inst);
#endif
	}

	platform_device_unregister(vdec->dev);

	if (!vdec->use_vfm_path) {
		if (vdec->mem_start) {
			codec_mm_free_for_dma(MEM_NAME, vdec->mem_start);
			vdec->mem_start = 0;
			vdec->mem_end = 0;
		}
	} else if (delay_release-- <= 0 &&
			!keep_vdec_mem &&
			vdec_mem_alloced_from_codec &&
			vdec_core->mem_start &&
			get_blackout_policy()) {
		codec_mm_free_for_dma(MEM_NAME, vdec_core->mem_start);
		vdec_cma_page = NULL;
		vdec_core->mem_start = reserved_mem_start;
		vdec_core->mem_end = reserved_mem_end;
	}

	vdec_destroy(vdec);

	mutex_lock(&vdec_mutex);
	inited_vcodec_num--;
	mutex_unlock(&vdec_mutex);
}

/* For dual running decoders, vdec_reset is only called with master vdec.
 */
int vdec_reset(struct vdec_s *vdec)
{
	vdec_disconnect(vdec);

	if (vdec->vframe_provider.name)
		vf_unreg_provider(&vdec->vframe_provider);

	if ((vdec->slave) && (vdec->slave->vframe_provider.name))
		vf_unreg_provider(&vdec->slave->vframe_provider);

	if (vdec->reset) {
		vdec->reset(vdec);
		if (vdec->slave)
			vdec->slave->reset(vdec->slave);
	}

	vdec_input_release(&vdec->input);

	vf_reg_provider(&vdec->vframe_provider);
	vf_notify_receiver(vdec->vf_provider_name,
			VFRAME_EVENT_PROVIDER_START, vdec);

	if (vdec->slave) {
		vf_reg_provider(&vdec->slave->vframe_provider);
		vf_notify_receiver(vdec->slave->vf_provider_name,
			VFRAME_EVENT_PROVIDER_START, vdec->slave);
	}

	vdec_connect(vdec);

	return 0;
}

void vdec_free_cmabuf(void)
{
	mutex_lock(&vdec_mutex);

	if (inited_vcodec_num > 0) {
		mutex_unlock(&vdec_mutex);
		return;
	}

	if (vdec_mem_alloced_from_codec && vdec_core->mem_start) {
		codec_mm_free_for_dma(MEM_NAME, vdec_core->mem_start);
		vdec_cma_page = NULL;
		vdec_core->mem_start = reserved_mem_start;
		vdec_core->mem_end = reserved_mem_end;
		pr_info("force free vdec memory\n");
	}

	mutex_unlock(&vdec_mutex);
}

static struct vdec_s *active_vdec(struct vdec_core_s *core)
{
	struct vdec_s *vdec;
	struct list_head *p;

	list_for_each(p, &core->connected_vdec_list) {
		vdec = list_entry(p, struct vdec_s, list);
		if (vdec->status == VDEC_STATUS_ACTIVE)
			return vdec;
	}

	return NULL;
}

/* Decoder callback
 * Each decoder instance uses this callback to notify status change, e.g. when
 * decoder finished using HW resource.
 * a sample callback from decoder's driver is following:
 *
 *        if (hw->vdec_cb) {
 *            vdec_set_next_status(vdec, VDEC_STATUS_CONNECTED);
 *            hw->vdec_cb(vdec, hw->vdec_cb_arg);
 *        }
 */
static void vdec_callback(struct vdec_s *vdec, void *data)
{
	struct vdec_core_s *core = (struct vdec_core_s *)data;

#ifdef CONFIG_MULTI_DEC
	vdec_profile(vdec, VDEC_PROFILE_EVENT_CB);
#endif

	up(&core->sem);
}

static irqreturn_t vdec_isr(int irq, void *dev_id)
{
	struct vdec_isr_context_s *c =
		(struct vdec_isr_context_s *)dev_id;
	struct vdec_s *vdec = vdec_core->active_vdec;

	if (c->dev_isr)
		return c->dev_isr(irq, c->dev_id);

	if (c != &vdec_core->isr_context[VDEC_IRQ_1]) {
#if 0
		pr_warn("vdec interrupt w/o a valid receiver\n");
#endif
		return IRQ_HANDLED;
	}

	if (!vdec) {
#if 0
		pr_warn("vdec interrupt w/o an active instance running. core = %p\n",
			core);
#endif
		return IRQ_HANDLED;
	}

	if (!vdec->irq_handler) {
#if 0
		pr_warn("vdec instance has no irq handle.\n");
#endif
		return IRQ_HANDLED;
	}

	return vdec->irq_handler(vdec);
}

static irqreturn_t vdec_thread_isr(int irq, void *dev_id)
{
	struct vdec_isr_context_s *c =
		(struct vdec_isr_context_s *)dev_id;
	struct vdec_s *vdec = vdec_core->active_vdec;

	if (c->dev_threaded_isr)
		return c->dev_threaded_isr(irq, c->dev_id);

	if (!vdec)
		return IRQ_HANDLED;

	if (!vdec->threaded_irq_handler)
		return IRQ_HANDLED;

	return vdec->threaded_irq_handler(vdec);
}

static inline bool vdec_ready_to_run(struct vdec_s *vdec)
{
	bool r;

	if (vdec->status != VDEC_STATUS_CONNECTED)
		return false;

	if (!vdec->run_ready)
		return false;

	if ((vdec->slave || vdec->master) &&
		(vdec->sched == 0))
		return false;

	if (step_mode) {
		if ((step_mode & 0xff) != vdec->id)
			return false;
	}

	step_mode &= ~0xff;

#ifdef CONFIG_MULTI_DEC
	vdec_profile(vdec, VDEC_PROFILE_EVENT_CHK_RUN_READY);
#endif

	r = vdec->run_ready(vdec);

#ifdef CONFIG_MULTI_DEC
	if (r)
		vdec_profile(vdec, VDEC_PROFILE_EVENT_RUN_READY);
#endif

	return r;
}

/* struct vdec_core_shread manages all decoder instance in active list. When
 * a vdec is added into the active list, it can onlt be in two status:
 * VDEC_STATUS_CONNECTED(the decoder does not own HW resource and ready to run)
 * VDEC_STATUS_ACTIVE(the decoder owns HW resources and is running).
 * Removing a decoder from active list is only performed within core thread.
 * Adding a decoder into active list is performed from user thread.
 */
static int vdec_core_thread(void *data)
{
	unsigned long flags;
	struct vdec_core_s *core = (struct vdec_core_s *)data;

	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};

	sched_setscheduler(current, SCHED_FIFO, &param);

	allow_signal(SIGTERM);

	while (down_interruptible(&core->sem) == 0) {
		struct vdec_s *vdec, *tmp;
		LIST_HEAD(disconnecting_list);

		if (kthread_should_stop())
			break;

		/* clean up previous active vdec's input */
		if ((core->active_vdec) &&
		    (core->active_vdec->status == VDEC_STATUS_CONNECTED)) {
			struct vdec_input_s *input = &core->active_vdec->input;

			while (!list_empty(&input->vframe_chunk_list)) {
				struct vframe_chunk_s *chunk =
					vdec_input_next_chunk(input);
				if (chunk->flag & VFRAME_CHUNK_FLAG_CONSUMED)
					vdec_input_release_chunk(input, chunk);
				else
					break;
			}

			vdec_save_input_context(core->active_vdec);
		}

		/* todo:
		 * this is the case when the decoder is in active mode and
		 * the system side wants to stop it. Currently we rely on
		 * the decoder instance to go back to VDEC_STATUS_CONNECTED
		 * from VDEC_STATUS_ACTIVE by its own. However, if for some
		 * reason the decoder can not exist by itself (dead decoding
		 * or whatever), then we may have to add another vdec API
		 * to kill the vdec and release its HW resource and make it
		 * become inactive again.
		 * if ((core->active_vdec) &&
		 * (core->active_vdec->status == VDEC_STATUS_DISCONNECTED)) {
		 * }
		 */

		flags = vdec_core_lock(core);

		/* check disconnected decoders */
		list_for_each_entry_safe(vdec, tmp,
			&core->connected_vdec_list, list) {
			if ((vdec->status == VDEC_STATUS_CONNECTED) &&
			    (vdec->next_status == VDEC_STATUS_DISCONNECTED)) {
				if (core->active_vdec == vdec)
					core->active_vdec = NULL;
				list_move(&vdec->list, &disconnecting_list);
			}
		}

		/* activate next decoder instance if there is none */
		vdec = active_vdec(core);

		if (!vdec) {
			/* round-robin decoder scheduling
			 * start from the decoder after previous active
			 * decoder instance, if not, then start from beginning
			 */
			if (core->active_vdec)
				vdec = list_entry(
						core->active_vdec->list.next,
						struct vdec_s, list);
			else
				vdec = list_entry(
						core->connected_vdec_list.next,
						struct vdec_s, list);

			list_for_each_entry_from(vdec,
				&core->connected_vdec_list, list) {
				if (vdec_ready_to_run(vdec))
					break;
			}

			if ((&vdec->list == &core->connected_vdec_list) &&
				(core->active_vdec)) {
				/* search from beginning */
				list_for_each_entry(vdec,
					&core->connected_vdec_list, list) {
					if (vdec_ready_to_run(vdec))
						break;

					if (vdec == core->active_vdec) {
						vdec = NULL;
						break;
					}
				}
			}

			if (&vdec->list == &core->connected_vdec_list)
				vdec = NULL;

			core->active_vdec = NULL;
		}

		vdec_core_unlock(core, flags);

		/* start the vdec instance */
		if ((vdec) && (vdec->status != VDEC_STATUS_ACTIVE)) {
			vdec_set_status(vdec, VDEC_STATUS_ACTIVE);

			/* activatate the decoder instance to run */
			core->active_vdec = vdec;
#ifdef CONFIG_MULTI_DEC
			vdec_profile(vdec, VDEC_PROFILE_EVENT_RUN);
#endif
			vdec->run(vdec, vdec_callback, core);
		}

		/* remove disconnected decoder from active list */
		list_for_each_entry_safe(vdec, tmp, &disconnecting_list, list) {
			list_del(&vdec->list);
			vdec_set_status(vdec, VDEC_STATUS_DISCONNECTED);
			complete(&vdec->inactive_done);
		}

		if (!core->active_vdec) {
			msleep(20);
			up(&core->sem);
		}
	}

	return 0;
}

#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
static bool test_hevc(u32 decomp_addr, u32 us_delay)
{
	int i;

	/* SW_RESET IPP */
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 1);
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0);

	/* initialize all canvas table */
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 0);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CMD_ADDR,
			0x1 | (i << 8) | decomp_addr);
	WRITE_VREG(HEVCD_MPP_ANC2AXI_TBL_CONF_ADDR, 1);
	WRITE_VREG(HEVCD_MPP_ANC_CANVAS_ACCCONFIG_ADDR, (0 << 8) | (0<<1) | 1);
	for (i = 0; i < 32; i++)
		WRITE_VREG(HEVCD_MPP_ANC_CANVAS_DATA_ADDR, 0);

	/* Intialize mcrcc */
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0x2);
	WRITE_VREG(HEVCD_MCRCC_CTL2, 0x0);
	WRITE_VREG(HEVCD_MCRCC_CTL3, 0x0);
	WRITE_VREG(HEVCD_MCRCC_CTL1, 0xff0);

	/* Decomp initialize */
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL1, 0x0);
	WRITE_VREG(HEVCD_MPP_DECOMP_CTL2, 0x0);

	/* Frame level initialization */
	WRITE_VREG(HEVCD_IPP_TOP_FRMCONFIG, 0x100 | (0x100 << 16));
	WRITE_VREG(HEVCD_IPP_TOP_TILECONFIG3, 0x0);
	WRITE_VREG(HEVCD_IPP_TOP_LCUCONFIG, 0x1 << 5);
	WRITE_VREG(HEVCD_IPP_BITDEPTH_CONFIG, 0x2 | (0x2 << 2));

	WRITE_VREG(HEVCD_IPP_CONFIG, 0x0);
	WRITE_VREG(HEVCD_IPP_LINEBUFF_BASE, 0x0);

	/* Enable SWIMP mode */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_CONFIG, 0x1);

	/* Enable frame */
	WRITE_VREG(HEVCD_IPP_TOP_CNTL, 0x2);
	WRITE_VREG(HEVCD_IPP_TOP_FRMCTL, 0x1);

	/* Send SW-command CTB info */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_CTBINFO, 0x1 << 31);

	/* Send PU_command */
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO0, (0x4 << 9) | (0x4 << 16));
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO1, 0x1 << 3);
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO2, 0x0);
	WRITE_VREG(HEVCD_IPP_SWMPREDIF_PUINFO3, 0x0);

	udelay(us_delay);

	WRITE_VREG(HEVCD_IPP_DBG_SEL, 0x2 << 4);

	return (READ_VREG(HEVCD_IPP_DBG_DATA) & 3) == 1;
}

void vdec_poweron(enum vdec_type_e core)
{
	void *decomp_addr = NULL;
	dma_addr_t decomp_dma_addr;
	u32 decomp_addr_aligned = 0;
	int hevc_loop = 0;

	if (core >= VDEC_MAX)
		return;

	mutex_lock(&vdec_mutex);

	vdec_core->power_ref_count[core]++;
	if (vdec_core->power_ref_count[core] > 1) {
		mutex_unlock(&vdec_mutex);
		return;
	}

	if (vdec_on(core)) {
		mutex_unlock(&vdec_mutex);
		return;
	}

	if (hevc_workaround_needed() &&
		(core == VDEC_HEVC)) {
		decomp_addr = dma_alloc_coherent(amports_get_dma_device(),
			SZ_64K + SZ_4K, &decomp_dma_addr, GFP_KERNEL);

		if (decomp_addr) {
			decomp_addr_aligned = ALIGN(decomp_dma_addr, SZ_64K);
			memset((u8 *)decomp_addr +
				(decomp_addr_aligned - decomp_dma_addr),
				0xff, SZ_4K);
		} else
			pr_err("vdec: alloc HEVC gxbb decomp buffer failed.\n");
	}

	if (core == VDEC_1) {
		/* vdec1 power on */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & ~0xc);
		/* wait 10uS */
		udelay(10);
		/* vdec1 soft reset */
		WRITE_VREG(DOS_SW_RESET0, 0xfffffffc);
		WRITE_VREG(DOS_SW_RESET0, 0);
		/* enable vdec1 clock */
		/*add power on vdec clock level setting,only for m8 chip,
		   m8baby and m8m2 can dynamic adjust vdec clock,
		   power on with default clock level */
		vdec_clock_hi_enable();
		/* power up vdec memories */
		WRITE_VREG(DOS_MEM_PD_VDEC, 0);
		/* remove vdec1 isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) & ~0xC0);
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC_MCRCC_STALL_CTRL, 0);
		if (get_cpu_type() >=
			MESON_CPU_MAJOR_ID_GXBB) {
			/*
			enable VDEC_1 DMC request
			*/
			unsigned long flags;
			spin_lock_irqsave(&vdec_spin_lock, flags);
			codec_dmcbus_write(DMC_REQ_CTRL,
				codec_dmcbus_read(DMC_REQ_CTRL) | (1 << 13));
			spin_unlock_irqrestore(&vdec_spin_lock, flags);
		}
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			/* vdec2 power on */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					~0x30);
			/* wait 10uS */
			udelay(10);
			/* vdec2 soft reset */
			WRITE_VREG(DOS_SW_RESET2, 0xffffffff);
			WRITE_VREG(DOS_SW_RESET2, 0);
			/* enable vdec1 clock */
			vdec2_clock_hi_enable();
			/* power up vdec memories */
			WRITE_VREG(DOS_MEM_PD_VDEC2, 0);
			/* remove vdec2 isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					~0x300);
			/* reset DOS top registers */
			WRITE_VREG(DOS_VDEC2_MCRCC_STALL_CTRL, 0);
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			/* hcodec power on */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					~0x3);
			/* wait 10uS */
			udelay(10);
			/* hcodec soft reset */
			WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
			WRITE_VREG(DOS_SW_RESET1, 0);
			/* enable hcodec clock */
			hcodec_clock_enable();
			/* power up hcodec memories */
			WRITE_VREG(DOS_MEM_PD_HCODEC, 0);
			/* remove hcodec isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					~0x30);
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			bool hevc_fixed = false;

			while (!hevc_fixed) {
				/* hevc power on */
				WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) &
					~0xc0);
				/* wait 10uS */
				udelay(10);
				/* hevc soft reset */
				WRITE_VREG(DOS_SW_RESET3, 0xffffffff);
				WRITE_VREG(DOS_SW_RESET3, 0);
				/* enable hevc clock */
				hevc_clock_hi_enable();
				/* power up hevc memories */
				WRITE_VREG(DOS_MEM_PD_HEVC, 0);
				/* remove hevc isolation */
				WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) &
					~0xc00);

				if (!hevc_workaround_needed())
					break;

				if (decomp_addr)
					hevc_fixed = test_hevc(
						decomp_addr_aligned, 20);

				if (!hevc_fixed) {
					hevc_loop++;

					mutex_unlock(&vdec_mutex);

					if (hevc_loop >= HEVC_TEST_LIMIT) {
						pr_warn("hevc power sequence over limit\n");
						pr_warn("=====================================================\n");
						pr_warn(" This chip is identified to have HW failure.\n");
						pr_warn(" Please contact sqa-platform to replace the platform.\n");
						pr_warn("=====================================================\n");

						panic("Force panic for chip detection !!!\n");

						break;
					}

					vdec_poweroff(VDEC_HEVC);

					mdelay(10);

					mutex_lock(&vdec_mutex);
				}
			}

			if (hevc_loop > hevc_max_reset_count)
				hevc_max_reset_count = hevc_loop;

			WRITE_VREG(DOS_SW_RESET3, 0xffffffff);
			udelay(10);
			WRITE_VREG(DOS_SW_RESET3, 0);
		}
	}

	if (decomp_addr)
		dma_free_coherent(amports_get_dma_device(),
			SZ_64K + SZ_4K, decomp_addr, decomp_dma_addr);

	mutex_unlock(&vdec_mutex);
}

void vdec_poweroff(enum vdec_type_e core)
{
	if (core >= VDEC_MAX)
		return;

	mutex_lock(&vdec_mutex);

	vdec_core->power_ref_count[core]--;
	if (vdec_core->power_ref_count[core] > 0) {
		mutex_unlock(&vdec_mutex);
		return;
	}

	if (core == VDEC_1) {
		if (get_cpu_type() >=
			MESON_CPU_MAJOR_ID_GXBB) {
			/* disable VDEC_1 DMC REQ
			*/
			unsigned long flags;
			spin_lock_irqsave(&vdec_spin_lock, flags);
			codec_dmcbus_write(DMC_REQ_CTRL,
				codec_dmcbus_read(DMC_REQ_CTRL) & (~(1 << 13)));
			spin_unlock_irqrestore(&vdec_spin_lock, flags);
			udelay(10);
		}
		/* enable vdec1 isolation */
		WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
				READ_AOREG(AO_RTI_GEN_PWR_ISO0) | 0xc0);
		/* power off vdec1 memories */
		WRITE_VREG(DOS_MEM_PD_VDEC, 0xffffffffUL);
		/* disable vdec1 clock */
		vdec_clock_off();
		/* vdec1 power off */
		WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
				READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 0xc);
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			/* enable vdec2 isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					0x300);
			/* power off vdec2 memories */
			WRITE_VREG(DOS_MEM_PD_VDEC2, 0xffffffffUL);
			/* disable vdec2 clock */
			vdec2_clock_off();
			/* vdec2 power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
					0x30);
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			/* enable hcodec isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					0x30);
			/* power off hcodec memories */
			WRITE_VREG(DOS_MEM_PD_HCODEC, 0xffffffffUL);
			/* disable hcodec clock */
			hcodec_clock_off();
			/* hcodec power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) | 3);
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			/* enable hevc isolation */
			WRITE_AOREG(AO_RTI_GEN_PWR_ISO0,
					READ_AOREG(AO_RTI_GEN_PWR_ISO0) |
					0xc00);
			/* power off hevc memories */
			WRITE_VREG(DOS_MEM_PD_HEVC, 0xffffffffUL);
			/* disable hevc clock */
			hevc_clock_off();
			/* hevc power off */
			WRITE_AOREG(AO_RTI_GEN_PWR_SLEEP0,
					READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) |
					0xc0);
		}
	}
	mutex_unlock(&vdec_mutex);
}

bool vdec_on(enum vdec_type_e core)
{
	bool ret = false;

	if (core == VDEC_1) {
		if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0xc) == 0) &&
			(READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x100))
			ret = true;
	} else if (core == VDEC_2) {
		if (has_vdec2()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0x30) == 0) &&
				(READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0x100))
				ret = true;
		}
	} else if (core == VDEC_HCODEC) {
		if (has_hdec()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0x3) == 0) &&
				(READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x1000000))
				ret = true;
		}
	} else if (core == VDEC_HEVC) {
		if (has_hevc_vdec()) {
			if (((READ_AOREG(AO_RTI_GEN_PWR_SLEEP0) & 0xc0) == 0) &&
				(READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0x1000000))
				ret = true;
		}
	}

	return ret;
}

#elif 0				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD */
void vdec_poweron(enum vdec_type_e core)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (core == VDEC_1) {
		/* vdec1 soft reset */
		WRITE_VREG(DOS_SW_RESET0, 0xfffffffc);
		WRITE_VREG(DOS_SW_RESET0, 0);
		/* enable vdec1 clock */
		vdec_clock_enable();
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC_MCRCC_STALL_CTRL, 0);
	} else if (core == VDEC_2) {
		/* vdec2 soft reset */
		WRITE_VREG(DOS_SW_RESET2, 0xffffffff);
		WRITE_VREG(DOS_SW_RESET2, 0);
		/* enable vdec2 clock */
		vdec2_clock_enable();
		/* reset DOS top registers */
		WRITE_VREG(DOS_VDEC2_MCRCC_STALL_CTRL, 0);
	} else if (core == VDEC_HCODEC) {
		/* hcodec soft reset */
		WRITE_VREG(DOS_SW_RESET1, 0xffffffff);
		WRITE_VREG(DOS_SW_RESET1, 0);
		/* enable hcodec clock */
		hcodec_clock_enable();
	}

	spin_unlock_irqrestore(&lock, flags);
}

void vdec_poweroff(enum vdec_type_e core)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (core == VDEC_1) {
		/* disable vdec1 clock */
		vdec_clock_off();
	} else if (core == VDEC_2) {
		/* disable vdec2 clock */
		vdec2_clock_off();
	} else if (core == VDEC_HCODEC) {
		/* disable hcodec clock */
		hcodec_clock_off();
	}

	spin_unlock_irqrestore(&lock, flags);
}

bool vdec_on(enum vdec_type_e core)
{
	bool ret = false;

	if (core == VDEC_1) {
		if (READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x100)
			ret = true;
	} else if (core == VDEC_2) {
		if (READ_HHI_REG(HHI_VDEC2_CLK_CNTL) & 0x100)
			ret = true;
	} else if (core == VDEC_HCODEC) {
		if (READ_HHI_REG(HHI_VDEC_CLK_CNTL) & 0x1000000)
			ret = true;
	}

	return ret;
}
#endif

int vdec_source_changed(int format, int width, int height, int fps)
{
	/* todo: add level routines for clock adjustment per chips */
	int ret = -1;
	static int on_setting;
	if (on_setting > 0)
		return ret;/*on changing clk,ignore this change*/

	if (vdec_source_get(VDEC_1) == width * height * fps)
		return ret;


	on_setting = 1;
	ret = vdec_source_changed_for_clk_set(format, width, height, fps);
	pr_info("vdec1 video changed to %d x %d %d fps clk->%dMHZ\n",
			width, height, fps, vdec_clk_get(VDEC_1));
	on_setting = 0;
	return ret;

}

int vdec2_source_changed(int format, int width, int height, int fps)
{
	int ret = -1;
	static int on_setting;

	if (has_vdec2()) {
		/* todo: add level routines for clock adjustment per chips */
		if (on_setting != 0)
			return ret;/*on changing clk,ignore this change*/

		if (vdec_source_get(VDEC_2) == width * height * fps)
			return ret;

		on_setting = 1;
		ret = vdec_source_changed_for_clk_set(format,
					width, height, fps);
		pr_info("vdec2 video changed to %d x %d %d fps clk->%dMHZ\n",
			width, height, fps, vdec_clk_get(VDEC_2));
		on_setting = 0;
		return ret;
	}
	return 0;
}

int hevc_source_changed(int format, int width, int height, int fps)
{
	/* todo: add level routines for clock adjustment per chips */
	int ret = -1;
	static int on_setting;

	if (on_setting != 0)
			return ret;/*on changing clk,ignore this change*/

	if (vdec_source_get(VDEC_HEVC) == width * height * fps)
		return ret;

	on_setting = 1;
	ret = vdec_source_changed_for_clk_set(format, width, height, fps);
	pr_info("hevc video changed to %d x %d %d fps clk->%dMHZ\n",
			width, height, fps, vdec_clk_get(VDEC_HEVC));
	on_setting = 0;

	return ret;
}

static enum vdec2_usage_e vdec2_usage = USAGE_NONE;
void set_vdec2_usage(enum vdec2_usage_e usage)
{
	if (has_vdec2()) {
		mutex_lock(&vdec_mutex);
		vdec2_usage = usage;
		mutex_unlock(&vdec_mutex);
	}
}

enum vdec2_usage_e get_vdec2_usage(void)
{
	if (has_vdec2())
		return vdec2_usage;
	else
		return 0;
}

static struct am_reg am_risc[] = {
	{"MSP", 0x300},
	{"MPSR", 0x301},
	{"MCPU_INT_BASE", 0x302},
	{"MCPU_INTR_GRP", 0x303},
	{"MCPU_INTR_MSK", 0x304},
	{"MCPU_INTR_REQ", 0x305},
	{"MPC-P", 0x306},
	{"MPC-D", 0x307},
	{"MPC_E", 0x308},
	{"MPC_W", 0x309},
	{"CSP", 0x320},
	{"CPSR", 0x321},
	{"CCPU_INT_BASE", 0x322},
	{"CCPU_INTR_GRP", 0x323},
	{"CCPU_INTR_MSK", 0x324},
	{"CCPU_INTR_REQ", 0x325},
	{"CPC-P", 0x326},
	{"CPC-D", 0x327},
	{"CPC_E", 0x328},
	{"CPC_W", 0x329},
	{"AV_SCRATCH_0", 0x09c0},
	{"AV_SCRATCH_1", 0x09c1},
	{"AV_SCRATCH_2", 0x09c2},
	{"AV_SCRATCH_3", 0x09c3},
	{"AV_SCRATCH_4", 0x09c4},
	{"AV_SCRATCH_5", 0x09c5},
	{"AV_SCRATCH_6", 0x09c6},
	{"AV_SCRATCH_7", 0x09c7},
	{"AV_SCRATCH_8", 0x09c8},
	{"AV_SCRATCH_9", 0x09c9},
	{"AV_SCRATCH_A", 0x09ca},
	{"AV_SCRATCH_B", 0x09cb},
	{"AV_SCRATCH_C", 0x09cc},
	{"AV_SCRATCH_D", 0x09cd},
	{"AV_SCRATCH_E", 0x09ce},
	{"AV_SCRATCH_F", 0x09cf},
	{"AV_SCRATCH_G", 0x09d0},
	{"AV_SCRATCH_H", 0x09d1},
	{"AV_SCRATCH_I", 0x09d2},
	{"AV_SCRATCH_J", 0x09d3},
	{"AV_SCRATCH_K", 0x09d4},
	{"AV_SCRATCH_L", 0x09d5},
	{"AV_SCRATCH_M", 0x09d6},
	{"AV_SCRATCH_N", 0x09d7},
};

static ssize_t amrisc_regs_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct am_reg *regs = am_risc;
	int rsize = sizeof(am_risc) / sizeof(struct am_reg);
	int i;
	unsigned val;
	ssize_t ret;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		mutex_lock(&vdec_mutex);
		if (!vdec_on(VDEC_1)) {
			mutex_unlock(&vdec_mutex);
			pbuf += sprintf(pbuf, "amrisc is power off\n");
			ret = pbuf - buf;
			return ret;
		}
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 1);
		 */
		amports_switch_gate("vdec", 1);
	}
	pbuf += sprintf(pbuf, "amrisc registers show:\n");
	for (i = 0; i < rsize; i++) {
		val = READ_VREG(regs[i].offset);
		pbuf += sprintf(pbuf, "%s(%#x)\t:%#x(%d)\n",
				regs[i].name, regs[i].offset, val, val);
	}
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		mutex_unlock(&vdec_mutex);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 0);
		 */
		amports_switch_gate("vdec", 0);
	}
	ret = pbuf - buf;
	return ret;
}

static ssize_t dump_trace_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int i;
	char *pbuf = buf;
	ssize_t ret;
	u16 *trace_buf = kmalloc(debug_trace_num * 2, GFP_KERNEL);
	if (!trace_buf) {
		pbuf += sprintf(pbuf, "No Memory bug\n");
		ret = pbuf - buf;
		return ret;
	}
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		mutex_lock(&vdec_mutex);
		if (!vdec_on(VDEC_1)) {
			mutex_unlock(&vdec_mutex);
			kfree(trace_buf);
			pbuf += sprintf(pbuf, "amrisc is power off\n");
			ret = pbuf - buf;
			return ret;
		}
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 1);
		 */
		amports_switch_gate("vdec", 1);
	}
	pr_info("dump trace steps:%d start\n", debug_trace_num);
	i = 0;
	while (i <= debug_trace_num - 16) {
		trace_buf[i] = READ_VREG(MPC_E);
		trace_buf[i + 1] = READ_VREG(MPC_E);
		trace_buf[i + 2] = READ_VREG(MPC_E);
		trace_buf[i + 3] = READ_VREG(MPC_E);
		trace_buf[i + 4] = READ_VREG(MPC_E);
		trace_buf[i + 5] = READ_VREG(MPC_E);
		trace_buf[i + 6] = READ_VREG(MPC_E);
		trace_buf[i + 7] = READ_VREG(MPC_E);
		trace_buf[i + 8] = READ_VREG(MPC_E);
		trace_buf[i + 9] = READ_VREG(MPC_E);
		trace_buf[i + 10] = READ_VREG(MPC_E);
		trace_buf[i + 11] = READ_VREG(MPC_E);
		trace_buf[i + 12] = READ_VREG(MPC_E);
		trace_buf[i + 13] = READ_VREG(MPC_E);
		trace_buf[i + 14] = READ_VREG(MPC_E);
		trace_buf[i + 15] = READ_VREG(MPC_E);
		i += 16;
	};
	pr_info("dump trace steps:%d finished\n", debug_trace_num);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		mutex_unlock(&vdec_mutex);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 0);
		 */
		amports_switch_gate("vdec", 0);
	}
	for (i = 0; i < debug_trace_num; i++) {
		if (i % 4 == 0) {
			if (i % 16 == 0)
				pbuf += sprintf(pbuf, "\n");
			else if (i % 8 == 0)
				pbuf += sprintf(pbuf, "  ");
			else	/* 4 */
				pbuf += sprintf(pbuf, " ");
		}
		pbuf += sprintf(pbuf, "%04x:", trace_buf[i]);
	}
	while (i < debug_trace_num)
		;
	kfree(trace_buf);
	pbuf += sprintf(pbuf, "\n");
	ret = pbuf - buf;
	return ret;
}

static ssize_t clock_level_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	size_t ret;
	pbuf += sprintf(pbuf, "%dMHZ\n", vdec_clk_get(VDEC_1));

	if (has_vdec2())
		pbuf += sprintf(pbuf, "%dMHZ\n", vdec_clk_get(VDEC_2));

	if (has_hevc_vdec())
		pbuf += sprintf(pbuf, "%dMHZ\n", vdec_clk_get(VDEC_HEVC));

	ret = pbuf - buf;
	return ret;
}

static ssize_t store_poweron_clock_level(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned val;
	ssize_t ret;

	ret = sscanf(buf, "%d", &val);
	if (ret != 1)
		return -EINVAL;
	poweron_clock_level = val;
	return size;
}

static ssize_t show_poweron_clock_level(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", poweron_clock_level);
}

/*
if keep_vdec_mem == 1
always don't release
vdec 64 memory for fast play.
*/
static ssize_t store_keep_vdec_mem(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned val;
	ssize_t ret;

	ret = sscanf(buf, "%d", &val);
	if (ret != 1)
		return -EINVAL;
	keep_vdec_mem = val;
	return size;
}

static ssize_t show_keep_vdec_mem(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", keep_vdec_mem);
}


/*irq num as same as .dts*/
/*
	interrupts = <0 3 1
		0 23 1
		0 32 1
		0 43 1
		0 44 1
		0 45 1>;
	interrupt-names = "vsync",
		"demux",
		"parser",
		"mailbox_0",
		"mailbox_1",
		"mailbox_2";
*/
s32 vdec_request_threaded_irq(enum vdec_irq_num num,
			irq_handler_t handler,
			irq_handler_t thread_fn,
			unsigned long irqflags,
			const char *devname, void *dev)
{
	s32 res_irq;
	s32 ret = 0;
	if (num >= VDEC_IRQ_MAX) {
		pr_err("[%s] request irq error, irq num too big!", __func__);
		return -EINVAL;
	}

	if (vdec_core->isr_context[num].irq < 0) {
		res_irq = platform_get_irq(
			vdec_core->vdec_core_platform_device, num);
		if (res_irq < 0) {
			pr_err("[%s] get irq error!", __func__);
			return -EINVAL;
		}

		vdec_core->isr_context[num].irq = res_irq;
		vdec_core->isr_context[num].dev_isr = handler;
		vdec_core->isr_context[num].dev_threaded_isr = thread_fn;
		vdec_core->isr_context[num].dev_id = dev;

		ret = request_threaded_irq(res_irq,
			vdec_isr,
			vdec_thread_isr,
			(thread_fn) ? IRQF_ONESHOT : irqflags,
			devname,
			&vdec_core->isr_context[num]);

		if (ret) {
			vdec_core->isr_context[num].irq = -1;
			vdec_core->isr_context[num].dev_isr = NULL;
			vdec_core->isr_context[num].dev_threaded_isr = NULL;
			vdec_core->isr_context[num].dev_id = NULL;

			pr_err("vdec irq register error for %s.\n", devname);
			return -EIO;
		}
	} else {
		vdec_core->isr_context[num].dev_isr = handler;
		vdec_core->isr_context[num].dev_threaded_isr = thread_fn;
		vdec_core->isr_context[num].dev_id = dev;
	}

	return ret;
}

s32 vdec_request_irq(enum vdec_irq_num num, irq_handler_t handler,
				const char *devname, void *dev)
{
	pr_info("vdec_request_irq %p, %s\n", handler, devname);

	return vdec_request_threaded_irq(num,
		handler,
		NULL,/*no thread_fn*/
		IRQF_SHARED,
		devname,
		dev);
}

void vdec_free_irq(enum vdec_irq_num num, void *dev)
{
	if (num >= VDEC_IRQ_MAX) {
		pr_err("[%s] request irq error, irq num too big!", __func__);
		return;
	}

	synchronize_irq(vdec_core->isr_context[num].irq);

	/* assume amrisc is stopped already and there is no mailbox interrupt
	 * when we reset pointers here.
	 */
	vdec_core->isr_context[num].dev_isr = NULL;
	vdec_core->isr_context[num].dev_threaded_isr = NULL;
	vdec_core->isr_context[num].dev_id = NULL;
}

static int dump_mode;
static ssize_t dump_risc_mem_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)/*set*/
{
	unsigned val;
	ssize_t ret;
	char dump_mode_str[4] = "PRL";
	ret = sscanf(buf, "%d", &val);
	if (ret != 1)
		return -EINVAL;
	dump_mode = val & 0x3;
	pr_info("set dump mode to %d,%c_mem\n",
		dump_mode, dump_mode_str[dump_mode]);
	return size;
}
static u32 read_amrisc_reg(int reg)
{
	WRITE_VREG(0x31b, reg);
	return READ_VREG(0x31c);
}

static void dump_pmem(void){
	int i;
	WRITE_VREG(0x301, 0x8000);
	WRITE_VREG(0x31d, 0);
	pr_info("start dump amrisc pmem of risc\n");
	for (i = 0; i < 0xfff; i++) {
		/*same as .o format*/
		pr_info("%08x // 0x%04x:\n", read_amrisc_reg(i), i);
	}
	return;
}


static void dump_lmem(void){
	int i;
	WRITE_VREG(0x301, 0x8000);
	WRITE_VREG(0x31d, 2);
	pr_info("start dump amrisc lmem\n");
	for (i = 0; i < 0x3ff; i++) {
		/*same as */
		pr_info("[%04x] = 0x%08x:\n", i, read_amrisc_reg(i));
	}
	return;
}

static ssize_t dump_risc_mem_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	int ret;
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
		mutex_lock(&vdec_mutex);
		if (!vdec_on(VDEC_1)) {
			mutex_unlock(&vdec_mutex);
			pbuf += sprintf(pbuf, "amrisc is power off\n");
			ret = pbuf - buf;
			return ret;
		}
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 1);
		 */
		amports_switch_gate("vdec", 1);
	}
	/*start do**/
	switch (dump_mode) {
	case 0:
		dump_pmem();
		break;
	case 2:
		dump_lmem();
		break;
	default:
		break;
	}

	/*done*/
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		mutex_unlock(&vdec_mutex);
	else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/*TODO:M6 define */
		/*
		   switch_mod_gate_by_type(MOD_VDEC, 0);
		 */
		amports_switch_gate("vdec", 0);
	}
	return sprintf(buf, "done\n");
}

static ssize_t core_show(struct class *class, struct class_attribute *attr,
			char *buf)
{
	struct vdec_core_s *core = vdec_core;
	char *pbuf = buf;

	if (list_empty(&core->connected_vdec_list))
		pbuf += sprintf(pbuf, "connected vdec list empty\n");
	else {
		struct vdec_s *vdec;
		list_for_each_entry(vdec, &core->connected_vdec_list, list) {
			pbuf += sprintf(pbuf,
				"\tvdec (%p (%s)), status = %s,\ttype = %s\n",
				vdec, vdec_device_name[vdec->format * 2],
				vdec_status_str(vdec),
				vdec_type_str(vdec));
		}
	}

	return pbuf - buf;
}

static ssize_t vdec_status_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct vdec_s *vdec;
	struct vdec_info vs;
	unsigned char vdec_num = 0;
	struct vdec_core_s *core = vdec_core;
	unsigned long flags = vdec_core_lock(vdec_core);

	if (list_empty(&core->connected_vdec_list)) {
		pbuf += sprintf(pbuf, "No vdec.\n");
		goto out;
	}

	list_for_each_entry(vdec, &core->connected_vdec_list, list) {
		if (VDEC_STATUS_CONNECTED == vdec->status) {
			if (vdec_status(vdec, &vs)) {
				pbuf += sprintf(pbuf, "err.\n");
				goto out;
			}
			pbuf += sprintf(pbuf,
				"vdec channel %u statistics:\n",
				vdec_num);
			pbuf += sprintf(pbuf,
				"%13s : %s\n", "device name",
				vs.vdec_name);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "frame width",
				vs.frame_width);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "frame height",
				vs.frame_height);
			pbuf += sprintf(pbuf,
				"%13s : %u %s\n", "frame rate",
				vs.frame_rate, "fps");
			pbuf += sprintf(pbuf,
				"%13s : %u %s\n", "bit rate",
				vs.bit_rate / 1024 * 8, "kbps");
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "status",
				vs.status);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "frame dur",
				vs.frame_dur);
			pbuf += sprintf(pbuf,
				"%13s : %u %s\n", "frame data",
				vs.frame_data / 1024, "KB");
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "frame count",
				vs.frame_count);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "drop count",
				vs.drop_frame_count);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "fra err count",
				vs.error_frame_count);
			pbuf += sprintf(pbuf,
				"%13s : %u\n", "hw err count",
				vs.error_count);
			pbuf += sprintf(pbuf,
				"%13s : %llu %s\n\n", "total data",
				vs.total_data / 1024, "KB");

			vdec_num++;
		}
	}
out:
	vdec_core_unlock(vdec_core, flags);
	return pbuf - buf;
}

static struct class_attribute vdec_class_attrs[] = {
	__ATTR_RO(amrisc_regs),
	__ATTR_RO(dump_trace),
	__ATTR_RO(clock_level),
	__ATTR(poweron_clock_level, S_IRUGO | S_IWUSR | S_IWGRP,
	show_poweron_clock_level, store_poweron_clock_level),
	__ATTR(dump_risc_mem, S_IRUGO | S_IWUSR | S_IWGRP,
	dump_risc_mem_show, dump_risc_mem_store),
	__ATTR(keep_vdec_mem, S_IRUGO | S_IWUSR | S_IWGRP,
	show_keep_vdec_mem, store_keep_vdec_mem),
	__ATTR_RO(core),
	__ATTR_RO(vdec_status),
	__ATTR_NULL
};

static struct class vdec_class = {
		.name = "vdec",
		.class_attrs = vdec_class_attrs,
	};


/*
pre alloced enough memory for decoder
fast start.
*/
void pre_alloc_vdec_memory(void)
{
	if (!keep_vdec_mem || vdec_core->mem_start)
		return;

	vdec_core->mem_start = codec_mm_alloc_for_dma(MEM_NAME,
		CMA_ALLOC_SIZE / PAGE_SIZE, 4 + PAGE_SHIFT,
		CODEC_MM_FLAGS_CMA_CLEAR |
		CODEC_MM_FLAGS_FOR_VDECODER);
	if (!vdec_core->mem_start)
		return;
	pr_debug("vdec base memory alloced %p\n",
	(void *)vdec_core->mem_start);

	vdec_core->mem_end = vdec_core->mem_start + CMA_ALLOC_SIZE - 1;
	vdec_mem_alloced_from_codec = 1;
	delay_release = 3;
}

static int vdec_probe(struct platform_device *pdev)
{
	s32 i, r;

	vdec_core = (struct vdec_core_s *)devm_kzalloc(&pdev->dev,
			sizeof(struct vdec_core_s), GFP_KERNEL);
	if (vdec_core == NULL) {
		pr_err("vdec core allocation failed.\n");
		return -ENOMEM;
	}

	atomic_set(&vdec_core->vdec_nr, 0);
	sema_init(&vdec_core->sem, 1);

	r = class_register(&vdec_class);
	if (r) {
		pr_info("vdec class create fail.\n");
		return r;
	}

	vdec_core->vdec_core_platform_device = pdev;

	platform_set_drvdata(pdev, vdec_core);

	for (i = 0; i < VDEC_IRQ_MAX; i++) {
		vdec_core->isr_context[i].index = i;
		vdec_core->isr_context[i].irq = -1;
	}

	r = vdec_request_threaded_irq(VDEC_IRQ_1, NULL, NULL,
		IRQF_ONESHOT, "vdec-1", NULL);
	if (r < 0) {
		pr_err("vdec interrupt request failed\n");
		return r;
	}

	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		pr_info("vdec_probe done\n");

	vdec_core->cma_dev = &pdev->dev;

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M8) {
		/* default to 250MHz */
		vdec_clock_hi_enable();
	}

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_GXBB) {
		/* set vdec dmc request to urgent */
		WRITE_DMCREG(DMC_AM5_CHAN_CTRL, 0x3f203cf);
	}
	if (codec_mm_get_reserved_size() >= 48 * SZ_1M
		&& codec_mm_get_reserved_size() <=  96 * SZ_1M) {
#ifdef CONFIG_MULTI_DEC
		vdec_default_buf_size[VFORMAT_H264_4K2K * 2] =
			codec_mm_get_reserved_size() / SZ_1M;
#else
		vdec_default_buf_size[VFORMAT_H264_4K2K] =
			codec_mm_get_reserved_size() / SZ_1M;
#endif

		/*all reserved size for prealloc*/
	}
	pre_alloc_vdec_memory();

	INIT_LIST_HEAD(&vdec_core->connected_vdec_list);
	spin_lock_init(&vdec_core->lock);

	vdec_core->thread = kthread_run(vdec_core_thread, vdec_core,
					"vdec-core");

	return 0;
}

static int vdec_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < VDEC_IRQ_MAX; i++) {
		if (vdec_core->isr_context[i].irq >= 0) {
			free_irq(vdec_core->isr_context[i].irq,
				&vdec_core->isr_context[i]);
			vdec_core->isr_context[i].irq = -1;
			vdec_core->isr_context[i].dev_isr = NULL;
			vdec_core->isr_context[i].dev_threaded_isr = NULL;
			vdec_core->isr_context[i].dev_id = NULL;
		}
	}

	kthread_stop(vdec_core->thread);

	class_unregister(&vdec_class);

	return 0;
}

static const struct of_device_id amlogic_vdec_dt_match[] = {
	{
		.compatible = "amlogic, vdec",
	},
	{},
};

static struct platform_driver vdec_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		.name = "vdec",
		.of_match_table = amlogic_vdec_dt_match,
	}
};

static int __init vdec_module_init(void)
{
	if (platform_driver_register(&vdec_driver)) {
		pr_info("failed to register vdec module\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit vdec_module_exit(void)
{
	platform_driver_unregister(&vdec_driver);
	return;
}

static int vdec_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	unsigned long start, end;
	start = rmem->base;
	end = rmem->base + rmem->size - 1;
	pr_info("init vdec memsource %lx->%lx\n", start, end);

	vdec_core->mem_start = start;
	vdec_core->mem_end = end;
	vdec_core->cma_dev = dev;

	return 0;
}

static const struct reserved_mem_ops rmem_vdec_ops = {
	.device_init = vdec_mem_device_init,
};

static int __init vdec_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_vdec_ops;
	pr_info("vdec: reserved mem setup\n");

	return 0;
}

RESERVEDMEM_OF_DECLARE(vdec, "amlogic, vdec-memory", vdec_mem_setup);

module_param(debug_trace_num, uint, 0664);
module_param(hevc_max_reset_count, int, 0664);
module_param(clk_config, uint, 0664);
module_param(step_mode, int, 0664);

module_init(vdec_module_init);
module_exit(vdec_module_exit);

MODULE_DESCRIPTION("AMLOGIC vdec driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
