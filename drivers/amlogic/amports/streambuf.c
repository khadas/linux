/*
 * drivers/amlogic/amports/streambuf.c
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
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/amlogic/amports/ptsserv.h>
#include <linux/amlogic/amports/vformat.h>
#include <linux/amlogic/iomap.h>
#include <asm/cacheflush.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
/* #include <mach/am_regs.h> */

#include "vdec_reg.h"
#include "vdec.h"
#include "streambuf_reg.h"
#include "streambuf.h"
#include "amports_config.h"
#include "amports_priv.h"
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/codec_mm/codec_mm.h>

#define STBUF_SIZE   (64*1024)
#define STBUF_WAIT_INTERVAL  (HZ/100)
#define MEM_NAME "streambuf"

void *fetchbuf = 0;

static s32 _stbuf_alloc(struct stream_buf_s *buf)
{
	if (buf->buf_size == 0)
		return -ENOBUFS;

	while (buf->buf_start == 0) {
		int flags = CODEC_MM_FLAGS_DMA;

		buf->buf_page_num = PAGE_ALIGN(buf->buf_size) / PAGE_SIZE;
		if (buf->type == BUF_TYPE_SUBTITLE)
			flags = CODEC_MM_FLAGS_DMA_CPU;

		/*if 4k,
		used cma first,for less mem fragments.
		*/
		if (((buf->type == BUF_TYPE_HEVC) ||
			(buf->type == BUF_TYPE_VIDEO)) &&
			buf->for_4k)
			flags |= CODEC_MM_FLAGS_CMA_FIRST;
		if (buf->buf_size > 20 * 1024 * 1024)
			flags |= CODEC_MM_FLAGS_CMA_FIRST;

		if ((buf->type == BUF_TYPE_HEVC) ||
			(buf->type == BUF_TYPE_VIDEO)) {
			flags |= CODEC_MM_FLAGS_FOR_VDECODER;
		} else if (buf->type == BUF_TYPE_AUDIO) {
			flags |= CODEC_MM_FLAGS_FOR_ADECODER;
			flags |= CODEC_MM_FLAGS_DMA_CPU;
		}
		if ((flags & CODEC_MM_FLAGS_FOR_VDECODER) &&
			codec_mm_video_tvp_enabled())/*TVP TODO for MULTI*/
			flags |= CODEC_MM_FLAGS_TVP;

		buf->buf_start = codec_mm_alloc_for_dma(MEM_NAME,
			buf->buf_page_num, 4 + PAGE_SHIFT, flags);
		if (!buf->buf_start) {
			int is_video = (buf->type == BUF_TYPE_HEVC) ||
					(buf->type == BUF_TYPE_VIDEO);
			if (is_video && buf->buf_size >= 9 * SZ_1M) {/*min 6M*/
				int old_size = buf->buf_size;
				buf->buf_size  =
					PAGE_ALIGN(buf->buf_size * 2/3);
				pr_info("%s stbuf alloced size = %d failed try small %d size\n",
				(buf->type == BUF_TYPE_HEVC) ? "HEVC" :
				(buf->type == BUF_TYPE_VIDEO) ? "Video" :
				(buf->type == BUF_TYPE_AUDIO) ? "Audio" :
				"Subtitle", old_size, buf->buf_size);
				continue;
			}
			pr_info("%s stbuf alloced size = %d failed\n",
				(buf->type == BUF_TYPE_HEVC) ? "HEVC" :
				(buf->type == BUF_TYPE_VIDEO) ? "Video" :
				(buf->type == BUF_TYPE_AUDIO) ? "Audio" :
				"Subtitle", buf->buf_size);
			return -ENOMEM;
		}
		pr_info("%s stbuf alloced at %p, size = %d\n",
				(buf->type == BUF_TYPE_HEVC) ? "HEVC" :
				(buf->type == BUF_TYPE_VIDEO) ? "Video" :
				(buf->type == BUF_TYPE_AUDIO) ? "Audio" :
				"Subtitle", (void *)buf->buf_start,
				buf->buf_size);
	}
	if (buf->buf_size < buf->canusebuf_size)
		buf->canusebuf_size = buf->buf_size;
	buf->flag |= BUF_FLAG_ALLOC;

	return 0;
}

int stbuf_change_size(struct stream_buf_s *buf, int size)
{
	unsigned long old_buf;
	int old_size, old_pagenum;
	int ret;

	pr_info("buffersize=%d,%d,start=%p\n", size, buf->buf_size,
			(void *)buf->buf_start);

	if (buf->buf_size == size && buf->buf_start != 0)
		return 0;

	old_buf = buf->buf_start;
	old_size = buf->buf_size;
	old_pagenum = buf->buf_page_num;
	buf->buf_start = 0;
	buf->buf_size = size;
	ret = size;

	if (size == 0 || _stbuf_alloc(buf) == 0) {
		/*
		 * size=0:We only free the old memory;
		 * alloc ok,changed to new buffer
		 */
		if (old_buf != 0) {
			codec_mm_free_for_dma(MEM_NAME, old_buf);
		}

		pr_info("changed the (%d) buffer size from %d to %d\n",
				buf->type, old_size, size);
		return 0;
	} else {
		/* alloc failed */
		buf->buf_start = old_buf;
		buf->buf_size = old_size;
		buf->buf_page_num = old_pagenum;
		pr_info("changed the (%d) buffer size from %d to %d,failed\n",
				buf->type, old_size, size);
	}

	return ret;
}

int stbuf_fetch_init(void)
{
	if (NULL != fetchbuf)
		return 0;

	fetchbuf = (void *)__get_free_pages(GFP_KERNEL,
						get_order(FETCHBUF_SIZE));

	if (!fetchbuf) {
		pr_info("%s: Can not allocate fetch working buffer\n",
				__func__);
		return -ENOMEM;
	}
	return 0;
}

void stbuf_fetch_release(void)
{
	if (0 && fetchbuf) {
		/* always don't free.for safe alloc/free*/
		free_pages((unsigned long)fetchbuf, get_order(FETCHBUF_SIZE));
		fetchbuf = 0;
	}

	return;
}

static void _stbuf_timer_func(unsigned long arg)
{
	struct stream_buf_s *p = (struct stream_buf_s *)arg;

	if (stbuf_space(p) < p->wcnt) {
		p->timer.expires = jiffies + STBUF_WAIT_INTERVAL;

		add_timer(&p->timer);
	} else
		wake_up_interruptible(&p->wq);

}

u32 stbuf_level(struct stream_buf_s *buf)
{
	if ((buf->type == BUF_TYPE_HEVC) || (buf->type == BUF_TYPE_VIDEO)) {
		if (READ_MPEG_REG(PARSER_ES_CONTROL) & 1) {
			int level = READ_MPEG_REG(PARSER_VIDEO_WP) -
				READ_MPEG_REG(PARSER_VIDEO_RP);
			if (level < 0)
				level += READ_MPEG_REG(PARSER_VIDEO_END_PTR) -
				READ_MPEG_REG(PARSER_VIDEO_START_PTR) + 8;
			return (u32)level;
		} else
			return (buf->type == BUF_TYPE_HEVC) ?
				READ_VREG(HEVC_STREAM_LEVEL) :
				_READ_ST_REG(LEVEL);
	}

	return _READ_ST_REG(LEVEL);
}

u32 stbuf_rp(struct stream_buf_s *buf)
{
	if ((buf->type == BUF_TYPE_HEVC) || (buf->type == BUF_TYPE_VIDEO)) {
		if (READ_MPEG_REG(PARSER_ES_CONTROL) & 1)
			return READ_MPEG_REG(PARSER_VIDEO_RP);
		else
			return (buf->type == BUF_TYPE_HEVC) ?
				READ_VREG(HEVC_STREAM_RD_PTR) :
				_READ_ST_REG(RP);
	}

	return _READ_ST_REG(RP);
}

u32 stbuf_space(struct stream_buf_s *buf)
{
	/* reserved space for safe write,
	   the parser fifo size is 1024byts, so reserve it */
	int size;

	size = buf->canusebuf_size - stbuf_level(buf);

	if (buf->canusebuf_size >= buf->buf_size / 2) {
		/* old reversed value,tobe full, reversed only... */
		size = size - 6 * 1024;
	}

	if ((buf->type == BUF_TYPE_VIDEO)
		|| (has_hevc_vdec() && buf->type == BUF_TYPE_HEVC))
		size -= READ_MPEG_REG(PARSER_VIDEO_HOLE);

	return size > 0 ? size : 0;
}

u32 stbuf_size(struct stream_buf_s *buf)
{
	return buf->buf_size;
}

u32 stbuf_canusesize(struct stream_buf_s *buf)
{
	return buf->canusebuf_size;
}

s32 stbuf_init(struct stream_buf_s *buf, struct vdec_s *vdec)
{
	s32 r;
	u32 dummy;
	u32 addr32;

	if (!buf->buf_start) {
		r = _stbuf_alloc(buf);
		if (r < 0)
			return r;
	}
	addr32 = buf->buf_start & 0xffffffff;
	init_waitqueue_head(&buf->wq);

	if ((buf->type == BUF_TYPE_VIDEO) || (buf->type == BUF_TYPE_HEVC)) {
		if (vdec) {
			if (vdec_stream_based(vdec))
				vdec_input_set_buffer(&vdec->input, addr32,
						buf->buf_size);
			else
				return vdec_input_set_buffer(&vdec->input,
					addr32,	buf->buf_size);
		}
	}

	buf->write_thread = 0;
	if (has_hevc_vdec() && buf->type == BUF_TYPE_HEVC) {
		CLEAR_VREG_MASK(HEVC_STREAM_CONTROL, 1);
		WRITE_VREG(HEVC_STREAM_START_ADDR, addr32);
		WRITE_VREG(HEVC_STREAM_END_ADDR, addr32 + buf->buf_size);
		WRITE_VREG(HEVC_STREAM_RD_PTR, addr32);
		WRITE_VREG(HEVC_STREAM_WR_PTR, addr32);

		return 0;
	}

	if (buf->type == BUF_TYPE_VIDEO) {
		_WRITE_ST_REG(CONTROL, 0);
		/* reset VLD before setting all pointers */
		WRITE_VREG(VLD_MEM_VIFIFO_WRAP_COUNT, 0);
		/*TODO: only > m6*/
#if 1/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		WRITE_VREG(DOS_SW_RESET0, (1 << 4));
		WRITE_VREG(DOS_SW_RESET0, 0);
#else
		WRITE_MPEG_REG(RESET0_REGISTER, RESET_VLD);
#endif

		dummy = READ_MPEG_REG(RESET0_REGISTER);
		WRITE_VREG(POWER_CTL_VLD, 1 << 4);
	} else if (buf->type == BUF_TYPE_AUDIO) {
		_WRITE_ST_REG(CONTROL, 0);

		WRITE_MPEG_REG(AIU_AIFIFO_GBIT, 0x80);
	}

	if (buf->type == BUF_TYPE_SUBTITLE) {
		WRITE_MPEG_REG(PARSER_SUB_RP, addr32);
		WRITE_MPEG_REG(PARSER_SUB_START_PTR, addr32);
		WRITE_MPEG_REG(PARSER_SUB_END_PTR,
					   addr32 + buf->buf_size - 8);

		return 0;
	}

	_WRITE_ST_REG(START_PTR, addr32);
	_WRITE_ST_REG(CURR_PTR, addr32);
	_WRITE_ST_REG(END_PTR, addr32 + buf->buf_size - 8);

	_SET_ST_REG_MASK(CONTROL, MEM_BUFCTRL_INIT);
	_CLR_ST_REG_MASK(CONTROL, MEM_BUFCTRL_INIT);

	_WRITE_ST_REG(BUF_CTRL, MEM_BUFCTRL_MANUAL);
	_WRITE_ST_REG(WP, addr32);

	_SET_ST_REG_MASK(BUF_CTRL, MEM_BUFCTRL_INIT);
	_CLR_ST_REG_MASK(BUF_CTRL, MEM_BUFCTRL_INIT);

	_SET_ST_REG_MASK(CONTROL,
			(0x11 << 16) | MEM_FILL_ON_LEVEL | MEM_CTRL_FILL_EN |
			MEM_CTRL_EMPTY_EN);
	return 0;
}

void stbuf_vdec2_init(struct stream_buf_s *buf)
{

	_WRITE_VDEC2_ST_REG(CONTROL, 0);

	_WRITE_VDEC2_ST_REG(START_PTR, _READ_ST_REG(START_PTR));
	_WRITE_VDEC2_ST_REG(END_PTR, _READ_ST_REG(END_PTR));
	_WRITE_VDEC2_ST_REG(CURR_PTR, _READ_ST_REG(CURR_PTR));

	_WRITE_VDEC2_ST_REG(CONTROL, MEM_FILL_ON_LEVEL | MEM_BUFCTRL_INIT);
	_WRITE_VDEC2_ST_REG(CONTROL, MEM_FILL_ON_LEVEL);

	_WRITE_VDEC2_ST_REG(BUF_CTRL, MEM_BUFCTRL_INIT);
	_WRITE_VDEC2_ST_REG(BUF_CTRL, 0);

	_WRITE_VDEC2_ST_REG(CONTROL,
			(0x11 << 16) | MEM_FILL_ON_LEVEL | MEM_CTRL_FILL_EN
			| MEM_CTRL_EMPTY_EN);
}

s32 stbuf_wait_space(struct stream_buf_s *stream_buf, size_t count)
{
	struct stream_buf_s *p = stream_buf;
	long time_out = 20;

	p->wcnt = count;

	setup_timer(&p->timer, _stbuf_timer_func, (ulong) p);

	mod_timer(&p->timer, jiffies + STBUF_WAIT_INTERVAL);

	if (wait_event_interruptible_timeout
		(p->wq, stbuf_space(p) >= count, time_out) == 0) {
		del_timer_sync(&p->timer);

		return -EAGAIN;
	}

	del_timer_sync(&p->timer);

	return 0;
}

void stbuf_release(struct stream_buf_s *buf)
{
	buf->first_tstamp = INVALID_PTS;

	stbuf_init(buf, NULL);	/* reinit buffer */

	if (buf->flag & BUF_FLAG_ALLOC && buf->buf_start) {
		codec_mm_free_for_dma(MEM_NAME, buf->buf_start);
		buf->flag &= ~BUF_FLAG_ALLOC;
		buf->buf_start = 0;
	}
	buf->flag &= ~BUF_FLAG_IN_USE;
}

u32 stbuf_sub_rp_get(void)
{
	return READ_MPEG_REG(PARSER_SUB_RP);
}

void stbuf_sub_rp_set(unsigned int sub_rp)
{
	WRITE_MPEG_REG(PARSER_SUB_RP, sub_rp);
	return;
}

u32 stbuf_sub_wp_get(void)
{
	return READ_MPEG_REG(PARSER_SUB_WP);
}

u32 stbuf_sub_start_get(void)
{
	return READ_MPEG_REG(PARSER_SUB_START_PTR);
}
