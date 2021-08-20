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
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "../../frame_provider/decoder/utils/vdec.h"
#include "amports_priv.h"
#include "stream_buffer_base.h"
#include "thread_rw.h"

#define DEFAULT_VIDEO_BUFFER_SIZE		(1024 * 1024 * 3)
#define DEFAULT_VIDEO_BUFFER_SIZE_4K		(1024 * 1024 * 6)
#define DEFAULT_VIDEO_BUFFER_SIZE_TVP		(1024 * 1024 * 10)
#define DEFAULT_VIDEO_BUFFER_SIZE_4K_TVP	(1024 * 1024 * 15)

static struct stream_buf_s vdec_buf_def = {
	.reg_base	= VLD_MEM_VIFIFO_REG_BASE,
	.type		= BUF_TYPE_VIDEO,
	.buf_start	= 0,
	.buf_size	= DEFAULT_VIDEO_BUFFER_SIZE,
	.default_buf_size = DEFAULT_VIDEO_BUFFER_SIZE,
	.first_tstamp	= INVALID_PTS
};

static struct stream_buf_s hevc_buf_def = {
	.reg_base	= HEVC_STREAM_REG_BASE,
	.type		= BUF_TYPE_HEVC,
	.buf_start	= 0,
	.buf_size	= DEFAULT_VIDEO_BUFFER_SIZE_4K,
	.default_buf_size = DEFAULT_VIDEO_BUFFER_SIZE_4K,
	.first_tstamp	= INVALID_PTS
};

static struct stream_buf_s *get_def_parms(int f)
{
	switch (f) {
	case VFORMAT_HEVC:
	case VFORMAT_AVS2:
	case VFORMAT_AV1:
	case VFORMAT_VP9:
		return &hevc_buf_def;
	default:
		return &vdec_buf_def;
	}
}

int stream_buffer_base_init(struct stream_buf_s *stbuf,
				struct stream_buf_ops *ops,
				struct parser_args *pars)
{
	struct vdec_s *vdec =
		container_of(stbuf, struct vdec_s, vbuf);
	struct stream_port_s *port = NULL;
	u32 format, width, height;

	/* sanity check. */
	if (WARN_ON(!stbuf) || WARN_ON(!ops))
		return -EINVAL;

	port	= vdec->port;
	format	= vdec->port->vformat;
	width	= vdec->sys_info->width;
	height	= vdec->sys_info->height;

	if (!stbuf->ext_buf_addr) {
		memcpy(stbuf, get_def_parms(format),
			sizeof(*stbuf));
	}

	stbuf->id	= vdec->id;
	stbuf->is_hevc	= ((format == VFORMAT_HEVC) ||
			(format == VFORMAT_AVS2) ||
			(format == VFORMAT_AV1) ||
			(format == VFORMAT_VP9));
	stbuf->for_4k	= ((width * height) >
			(1920 * 1088)) ? 1 : 0;
	stbuf->is_multi_inst = !vdec_single(vdec);
	memcpy(&stbuf->pars, pars, sizeof(*pars));

	/* register ops func. */
	stbuf->ops	= ops;

	return 0;
}
EXPORT_SYMBOL(stream_buffer_base_init);

void stream_buffer_set_ext_buf(struct stream_buf_s *stbuf,
			       ulong addr,
			       u32 size,
			       u32 flag)
{
	stbuf->ext_buf_addr	= addr;
	stbuf->buf_size 	= size;
	stbuf->is_secure	= ((flag & STBUF_META_FLAG_SECURE) != 0);
	stbuf->use_ptsserv	= ((flag & STBUF_META_FLAG_PTS_SERV) != 0);
	/*
	pr_debug("%s, addr %lx, size 0x%x, secure %d\n", __func__,
		stbuf->ext_buf_addr, stbuf->buf_size, stbuf->is_secure);
	*/
}
EXPORT_SYMBOL(stream_buffer_set_ext_buf);

void stream_buffer_meta_write(struct stream_buf_s *stbuf,
	struct stream_buffer_metainfo *meta)
{
	u32 wp = stbuf->ops->get_wp(stbuf);

	if ((stbuf->stream_offset == 0) &&
		(wp == stbuf->ext_buf_addr) &&
		(meta->stbuf_pktaddr > stbuf->ext_buf_addr)) {
		struct vdec_s *vdec = container_of(stbuf, struct vdec_s, vbuf);
		u32 first_ptr;
		u32 round_down_size = 0;

		/*RP max alignment requirement*/
		if (vdec->input.target == VDEC_INPUT_TARGET_HEVC)
			round_down_size = 0x80;
		else if (vdec->input.target == VDEC_INPUT_TARGET_VLD)
			round_down_size = 0x100;

		if (stbuf->ext_buf_addr > (meta->stbuf_pktaddr - round_down_size))
			first_ptr = stbuf->ext_buf_addr;
		else {
			first_ptr = round_down(meta->stbuf_pktaddr, round_down_size);
			pr_info("warn: first packet_wp(%x round_down %x) is not stbuf start addr(%lx)\n",
				meta->stbuf_pktaddr, first_ptr, stbuf->ext_buf_addr);
		}

		stbuf->ops->set_wp(stbuf, first_ptr);
		stbuf->ops->set_rp(stbuf, first_ptr);
		vdec->input.swap_rp = first_ptr;
		if (vdec->slave)
			vdec->slave->input.swap_rp = first_ptr;
		if (vdec->input.target != VDEC_INPUT_TARGET_HEVC)
			stbuf->stream_offset += (meta->stbuf_pktaddr - stbuf->ext_buf_addr);
		else
			stbuf->stream_offset += (meta->stbuf_pktaddr - first_ptr);

	}

	if (meta->stbuf_pktaddr + meta->stbuf_pktsize < stbuf->buf_start + stbuf->buf_size)
		wp = meta->stbuf_pktaddr + meta->stbuf_pktsize;
	else
		wp = meta->stbuf_pktaddr + meta->stbuf_pktsize - stbuf->buf_size;

	stbuf->ops->set_wp(stbuf, wp);

	stbuf->stream_offset += meta->stbuf_pktsize;
	stbuf->last_offset[stbuf->write_count % 2] = stbuf->stream_offset;
	stbuf->write_count++;
	/*
	pr_debug("%s, update wp 0x%x + sz 0x%x --> 0x%x, stream_offset 0x%x\n",
		__func__, meta->stbuf_pktaddr, meta->stbuf_pktsize, wp, stbuf->stream_offset);
	*/
}
EXPORT_SYMBOL(stream_buffer_meta_write);

ssize_t stream_buffer_write_ex(struct file *file,
		       struct stream_buf_s *stbuf,
		       const char __user *buf,
		       size_t count, int flags)
{
	int r;
	u32 len = count;

	if (buf == NULL || count == 0)
		return -EINVAL;

	if (stbuf_space(stbuf) < count) {
		if ((flags & 2) || ((file != NULL) &&
				(file->f_flags & O_NONBLOCK))) {
			len = stbuf_space(stbuf);
			if (len < 256)	/* <1k.do eagain, */
				return -EAGAIN;
		} else {
			len = min(stbuf_canusesize(stbuf) / 8, len);
			if (stbuf_space(stbuf) < len) {
				r = stbuf_wait_space(stbuf, len);
				if (r < 0)
					return r;
			}
		}
	}

	stbuf->last_write_jiffies64 = jiffies_64;
	stbuf->is_phybuf = (flags & 1);

	len = min_t(u32, len, count);

	r = stbuf->ops->write(stbuf, buf, len);
	if (r > 0)
		stbuf->stream_offset += r;

	return r;
}
EXPORT_SYMBOL(stream_buffer_write_ex);

int stream_buffer_write(struct file *file,
		       struct stream_buf_s *stbuf,
		       const char *buf,
		       size_t count)
{
	if (stbuf->write_thread)
		return threadrw_write(file, stbuf, buf, count);
	else
		return stream_buffer_write_ex(file, stbuf, buf, count, 0);
}
EXPORT_SYMBOL(stream_buffer_write);

