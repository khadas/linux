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
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/tsync_pcr.h>
#include "../../frame_provider/decoder/utils/vdec.h"
#include "../../common/chips/decoder_cpu_ver_info.h"
#include "stream_buffer_base.h"
#include "amports_priv.h"
#include "thread_rw.h"

#define MEM_NAME "stbuf"
#define MAP_RANGE (SZ_1M)

static void stream_buffer_release(struct stream_buf_s *stbuf);

static const char *type_to_str(int t)
{
	switch (t) {
	case BUF_TYPE_VIDEO:
		return "VIDEO";
	case BUF_TYPE_AUDIO:
		return "AUDIO";
	case BUF_TYPE_SUBTITLE:
		return "SUB";
	case BUF_TYPE_USERDATA:
		return "USER";
	case BUF_TYPE_HEVC:
		return "HEVC";
	default:
		return "ERR";
	}
}

static int stream_buffer_init(struct stream_buf_s *stbuf, struct vdec_s *vdec)
{
	int ret = 0;
	u32 flags = CODEC_MM_FLAGS_DMA;
	bool is_secure = 0;
	u32 addr = 0;
	int pages = 0;
	u32 size;

	if (stbuf->buf_start)
		return 0;

	snprintf(stbuf->name, sizeof(stbuf->name),
		"%s-%d", MEM_NAME, vdec->id);

	if (stbuf->ext_buf_addr) {
		addr	= stbuf->ext_buf_addr;
		size	= stbuf->buf_size;
		is_secure = stbuf->is_secure;
		pages = (size >> PAGE_SHIFT);
	} else {
		flags |= CODEC_MM_FLAGS_FOR_VDECODER;
		if (vdec->port_flag & PORT_FLAG_DRM) {
			flags |= CODEC_MM_FLAGS_TVP;
			is_secure = true;
		}

		size = PAGE_ALIGN(stbuf->buf_size);
		pages = (size >> PAGE_SHIFT);
		addr = codec_mm_alloc_for_dma(stbuf->name,
				pages, PAGE_SHIFT + 4, flags);
		if (!addr) {
			ret = -ENOMEM;
			goto err;
		}
		stbuf->use_ptsserv = 1;
	}
	vdec_config_vld_reg(vdec, addr, size);

	ret = vdec_set_input_buffer(vdec, addr, size);
	if (ret) {
		pr_err("[%d]: set input buffer err.\n", stbuf->id);
		goto err;
	}

	atomic_set(&stbuf->payload, 0);
	init_waitqueue_head(&stbuf->wq);

	stbuf->buf_start	= addr;
	stbuf->buf_wp		= addr;
	stbuf->buf_rp		= addr;
	stbuf->buf_size		= size;
	stbuf->is_secure	= is_secure;
	stbuf->no_parser	= true;
	stbuf->buf_page_num	= pages;
	stbuf->canusebuf_size	= size;
	stbuf->stream_offset	= 0;

	/* init thread write. */
	if (!(vdec_get_debug_flags() & 1) &&
		!codec_mm_video_tvp_enabled() &&
		(!stbuf->ext_buf_addr)) {
		int block_size = PAGE_SIZE << 4;
		int buf_num = (2 * SZ_1M) / (PAGE_SIZE << 4);

		stbuf->write_thread =
			threadrw_alloc(buf_num, block_size,
				       stream_buffer_write_ex, 0);
	}

	stbuf->flag |= BUF_FLAG_ALLOC;
	stbuf->flag |= BUF_FLAG_IN_USE;
	if (vdec_single(vdec))
		pts_start(stbuf->type);
	pr_info("[%d]: [%s-%s] addr: %lx, size: %x, thrRW: %d, extbuf: %d, secure: %d\n",
		stbuf->id, type_to_str(stbuf->type), stbuf->name,
		stbuf->buf_start, stbuf->buf_size,
		!!stbuf->write_thread,
		!!stbuf->ext_buf_addr,
		stbuf->is_secure);

	return 0;
err:
	stream_buffer_release(stbuf);

	return ret;
}

static void stream_buffer_release(struct stream_buf_s *stbuf)
{
	if (stbuf->write_thread)
		threadrw_release(stbuf);
	if (vdec_single(container_of(stbuf, struct vdec_s, vbuf)))
		pts_stop(stbuf->type);

	if (stbuf->flag & BUF_FLAG_ALLOC && stbuf->buf_start) {
		if (!stbuf->ext_buf_addr)
			codec_mm_free_for_dma(MEM_NAME, stbuf->buf_start);

		stbuf->flag		&= ~BUF_FLAG_ALLOC;
		stbuf->ext_buf_addr	= 0;
		stbuf->buf_start	= 0;
		stbuf->is_secure	= false;
	}
	stbuf->flag &= ~BUF_FLAG_IN_USE;
}

static int get_free_space(struct stream_buf_s *stbuf)
{
	u32 len = stbuf->buf_size;
	int idle = 0;

	if (!atomic_read(&stbuf->payload) && (stbuf->buf_rp == stbuf->buf_wp))
		idle = len;
	else if (stbuf->buf_wp > stbuf->buf_rp)
		idle = len - (stbuf->buf_wp - stbuf->buf_rp);
	else if (stbuf->buf_wp < stbuf->buf_rp)
		idle = stbuf->buf_rp - stbuf->buf_wp;

	/*pr_info("[%d]: wp: %x, rp: %x, payload: %d, free space: %d\n",
		stbuf->id, stbuf->buf_wp, stbuf->buf_rp,
		atomic_read(&stbuf->payload), idle);*/

	return idle;
}

static int aml_copy_from_user(void *to, const void *from, ulong n)
{
	int ret =0;

	if (likely(access_ok(from, n)))
		ret = copy_from_user(to, from, n);
	else
		memcpy(to, from, n);

	return ret;
}

static int stream_buffer_copy(struct stream_buf_s *stbuf, const u8 *buf, u32 size)
{
	int ret = 0;
	void *src = NULL, *dst = NULL;
	int i, len;

	for (i = 0; i < size; i += MAP_RANGE) {
		len = ((size - i) > MAP_RANGE) ? MAP_RANGE : size - i;
		src = stbuf->is_phybuf ?
			codec_mm_vmap((ulong) buf + i, len) :
			(void *) buf;
		dst = codec_mm_vmap(stbuf->buf_wp + i, len);
		if (!src || !dst) {
			ret = -EFAULT;
			pr_err("[%d]: %s, src or dst is invalid.\n",
				stbuf->id,  __func__);
			goto err;
		}

		if (aml_copy_from_user(dst, src, len)) {
			ret = -EAGAIN;
			goto err;
		}

		codec_mm_dma_flush(dst, len, DMA_TO_DEVICE);
		codec_mm_unmap_phyaddr(dst);

		if (stbuf->is_phybuf)
			codec_mm_unmap_phyaddr(src);
	}

	return 0;
err:
	if (stbuf->is_phybuf && src)
		codec_mm_unmap_phyaddr(src);
	if (dst)
		codec_mm_unmap_phyaddr(dst);
	return ret;
}

static int rb_push_data(struct stream_buf_s *stbuf, const u8 *in, u32 size)
{
	int ret, len;
	u32 wp = stbuf->buf_wp;
	u32 sp = (stbuf->buf_wp + size);
	u32 ep = (stbuf->buf_start + stbuf->buf_size);

	len = sp > ep ? ep - wp : size;

	if (!stbuf->ext_buf_addr) {
		ret = stream_buffer_copy(stbuf, in, len);
		if (ret)
			return ret;
	}

	stbuf->ops->set_wp(stbuf, (wp + len >= ep) ?
		stbuf->buf_start : (wp + len));

	if (stbuf->buf_wp == stbuf->buf_rp) {
		pr_debug("[%d]: stream buffer is full, payload: %d\n",
			stbuf->id, atomic_read(&stbuf->payload));
	}

	return len;
}

static int stream_buffer_write_inner(struct stream_buf_s *stbuf,
				     const u8 *in, u32 size)
{
	if (in == NULL || size > stbuf->buf_size) {
		pr_err("[%d]: params are not valid.\n", stbuf->id);
		return -1;
	}

	if (get_free_space(stbuf) < size)
		return -EAGAIN;

	return rb_push_data(stbuf, in, size);
}

static u32 stream_buffer_get_wp(struct stream_buf_s *stbuf)
{
	return stbuf->buf_wp;
}

static void stream_buffer_set_wp(struct stream_buf_s *stbuf, u32 val)
{
	int len = (val >= stbuf->buf_wp) ? (val - stbuf->buf_wp) :
		(stbuf->buf_size - stbuf->buf_wp + val);

	stbuf->buf_wp = val;
	vdec_set_vld_wp(container_of(stbuf, struct vdec_s, vbuf), stbuf->buf_wp);

	atomic_add(len, &stbuf->payload);
}

static u32 stream_buffer_get_rp(struct stream_buf_s *stbuf)
{
	return stbuf->buf_rp;
}

static void stream_buffer_set_rp(struct stream_buf_s *stbuf, u32 val)
{
	int len = (val >= stbuf->buf_rp) ? (val - stbuf->buf_rp) :
		(stbuf->buf_size - stbuf->buf_rp  + val);

	stbuf->buf_rp = val;
	atomic_sub(len, &stbuf->payload);
}

static struct stream_buf_ops stream_buffer_ops = {
	.init	= stream_buffer_init,
	.release = stream_buffer_release,
	.write	= stream_buffer_write_inner,
	.get_wp	= stream_buffer_get_wp,
	.set_wp	= stream_buffer_set_wp,
	.get_rp	= stream_buffer_get_rp,
	.set_rp	= stream_buffer_set_rp,
};

struct stream_buf_ops *get_stbuf_ops(void)
{
	return &stream_buffer_ops;
}
EXPORT_SYMBOL(get_stbuf_ops);

