/*
 * drivers/amlogic/amports/rmparser.c
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
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/amlogic/amports/amstream.h>
#include <linux/amlogic/amports/ptsserv.h>
/* #include <mach/am_regs.h> */
#include <linux/uaccess.h>

#include "vdec_reg.h"
#include "streambuf.h"
#include "streambuf_reg.h"
#include "rmparser.h"
#include "amports_priv.h"
#include <linux/delay.h>

#define MANAGE_PTS

static u32 fetch_done;
static u32 parse_halt;

static DECLARE_WAIT_QUEUE_HEAD(rm_wq);
static const char rmparser_id[] = "rmparser-id";

static irqreturn_t rm_parser_isr(int irq, void *dev_id)
{
	u32 int_status = READ_MPEG_REG(PARSER_INT_STATUS);

	if (int_status & PARSER_INTSTAT_FETCH_CMD) {
		WRITE_MPEG_REG(PARSER_INT_STATUS, PARSER_INTSTAT_FETCH_CMD);
		fetch_done = 1;

		wake_up_interruptible(&rm_wq);
	}

	return IRQ_HANDLED;
}

s32 rmparser_init(struct vdec_s *vdec)
{
	s32 r;
	parse_halt = 0;
	if (fetchbuf == 0) {
		pr_info("%s: no fetchbuf\n", __func__);
		return -ENOMEM;
	}

	WRITE_MPEG_REG(RESET1_REGISTER, RESET_PARSER);

	/* TS data path */
#ifndef CONFIG_AM_DVB
	WRITE_MPEG_REG(FEC_INPUT_CONTROL, 0);
#else
	tsdemux_set_reset_flag();
#endif
	CLEAR_MPEG_REG_MASK(TS_HIU_CTL, 1 << USE_HI_BSF_INTERFACE);
	CLEAR_MPEG_REG_MASK(TS_HIU_CTL_2, 1 << USE_HI_BSF_INTERFACE);
	CLEAR_MPEG_REG_MASK(TS_HIU_CTL_3, 1 << USE_HI_BSF_INTERFACE);

	CLEAR_MPEG_REG_MASK(TS_FILE_CONFIG, (1 << TS_HIU_ENABLE));

	/* hook stream buffer with PARSER */
	WRITE_MPEG_REG(PARSER_VIDEO_START_PTR, vdec->input.start);
	WRITE_MPEG_REG(PARSER_VIDEO_END_PTR,
		vdec->input.start + vdec->input.size - 8);

	CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_VID_MAN_RD_PTR);

	WRITE_MPEG_REG(PARSER_AUDIO_START_PTR,
				   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));
	WRITE_MPEG_REG(PARSER_AUDIO_END_PTR,
				   READ_MPEG_REG(AIU_MEM_AIFIFO_END_PTR));
	CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_AUD_MAN_RD_PTR);

	WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
	CLEAR_VREG_MASK(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

	WRITE_MPEG_REG(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
	CLEAR_MPEG_REG_MASK(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

	WRITE_MPEG_REG(PFIFO_RD_PTR, 0);
	WRITE_MPEG_REG(PFIFO_WR_PTR, 0);

	WRITE_MPEG_REG(PARSER_SEARCH_MASK, 0);
	WRITE_MPEG_REG(PARSER_CONTROL, (ES_SEARCH | ES_PARSER_START));

#ifdef MANAGE_PTS
	if (pts_start(PTS_TYPE_VIDEO) < 0)
		goto Err_1;

	if (pts_start(PTS_TYPE_AUDIO) < 0)
		goto Err_2;
#endif
	/*TODO irq */

	/* enable interrupt */

	r = vdec_request_irq(PARSER_IRQ, rm_parser_isr,
			"rmparser", (void *)rmparser_id);

	if (r) {
		pr_info("RM parser irq register failed.\n");
		goto Err_3;
	}

	WRITE_MPEG_REG(PARSER_INT_STATUS, 0xffff);
	WRITE_MPEG_REG(PARSER_INT_ENABLE,
			((PARSER_INT_ALL & (~PARSER_INTSTAT_FETCH_CMD)) <<
					PARSER_INT_AMRISC_EN_BIT)
			| (PARSER_INTSTAT_FETCH_CMD << PARSER_INT_HOST_EN_BIT));

	return 0;

Err_3:
	pts_stop(PTS_TYPE_AUDIO);
Err_2:
	pts_stop(PTS_TYPE_VIDEO);
Err_1:
	return -ENOENT;
}

void rmparser_release(void)
{
	WRITE_MPEG_REG(PARSER_INT_ENABLE, 0);
	/*TODO irq */

	vdec_free_irq(PARSER_IRQ, (void *)rmparser_id);

#ifdef MANAGE_PTS
	pts_stop(PTS_TYPE_VIDEO);
	pts_stop(PTS_TYPE_AUDIO);
#endif

	return;
}

static inline u32 buf_wp(u32 type)
{
	return (type == BUF_TYPE_VIDEO) ? READ_VREG(VLD_MEM_VIFIFO_WP) :
		(type == BUF_TYPE_AUDIO) ?
		READ_MPEG_REG(AIU_MEM_AIFIFO_MAN_WP) :
		READ_MPEG_REG(PARSER_SUB_START_PTR);
}

static ssize_t _rmparser_write(const char __user *buf, size_t count)
{
	size_t r = count;
	const char __user *p = buf;
	u32 len;
	int ret;
	static int halt_droped_len;
	u32 vwp, awp;
	dma_addr_t dma_addr = 0;

	if (r > 0) {
		len = min_t(size_t, r, FETCHBUF_SIZE);

		if (copy_from_user(fetchbuf, p, len))
			return -EFAULT;
		dma_addr =
			dma_map_single(amports_get_dma_device(),
					fetchbuf, FETCHBUF_SIZE,
					DMA_TO_DEVICE);
		if (dma_mapping_error(amports_get_dma_device(), dma_addr))
			return -EFAULT;

		fetch_done = 0;

		wmb(); /* Ensure fetchbuf  contents visible */
		vwp = buf_wp(BUF_TYPE_VIDEO);
		awp = buf_wp(BUF_TYPE_AUDIO);
		WRITE_MPEG_REG(PARSER_FETCH_ADDR, dma_addr);

		WRITE_MPEG_REG(PARSER_FETCH_CMD, (7 << FETCH_ENDIAN) | len);
		dma_unmap_single(amports_get_dma_device(), dma_addr,
						 FETCHBUF_SIZE, DMA_TO_DEVICE);
		ret =
			wait_event_interruptible_timeout(rm_wq, fetch_done != 0,
					HZ / 10);
		if (ret == 0) {
			WRITE_MPEG_REG(PARSER_FETCH_CMD, 0);
			parse_halt++;
			pr_info
			("write timeout,retry,halt_count=%d parse_control=%x\n",
			 parse_halt, READ_MPEG_REG(PARSER_CONTROL));

			vreal_set_fatal_flag(1);

			if (parse_halt > 10) {
				WRITE_MPEG_REG(PARSER_CONTROL,
						(ES_SEARCH | ES_PARSER_START));
				pr_info("reset parse_control=%x\n",
					   READ_MPEG_REG(PARSER_CONTROL));
			}
			return -EAGAIN;
		} else if (ret < 0)
			return -ERESTARTSYS;

		if (vwp == buf_wp(BUF_TYPE_VIDEO)
			&& awp == buf_wp(BUF_TYPE_AUDIO)) {
			struct stream_buf_s *v_buf_t =
				get_buf_by_type(BUF_TYPE_VIDEO);
			struct stream_buf_s *a_buf_t =
				get_buf_by_type(BUF_TYPE_AUDIO);
			int v_st_lv = stbuf_level(v_buf_t);
			int a_st_lv = stbuf_level(a_buf_t);
			if ((parse_halt + 1) % 10 == 1) {
				pr_info("V&A WP not changed after write");
				pr_info(",video %x->%x", vwp,
						buf_wp(BUF_TYPE_VIDEO));
				pr_info(",Audio:%x-->%x,parse_halt=%d\n",
						awp, buf_wp(BUF_TYPE_AUDIO),
						parse_halt);
			}
			parse_halt++;	/*wp not changed ,
					  we think have bugs on parser now. */
			if (parse_halt > 10 &&
					(v_st_lv < 1000 || a_st_lv < 100)) {
				/*reset while at  least one is underflow. */
				WRITE_MPEG_REG(PARSER_CONTROL,
						(ES_SEARCH | ES_PARSER_START));
				pr_info("reset parse_control=%x\n",
					   READ_MPEG_REG(PARSER_CONTROL));
			}
			if (parse_halt <= 10 ||
				halt_droped_len < 100 * 1024) {
				/*drops first 10 pkt ,
				  some times maybe no av data */
				pr_info("drop this pkt=%d,len=%d\n", parse_halt,
					   len);
				p += len;
				r -= len;
				halt_droped_len += len;
			} else
				return -EAGAIN;
		} else {
			halt_droped_len = 0;
			parse_halt = 0;
			p += len;
			r -= len;
		}
	}
	return count - r;
}

ssize_t rmparser_write(struct file *file,
					   struct stream_buf_s *vbuf,
					   struct stream_buf_s *abuf,
					   const char __user *buf, size_t count)
{
	s32 r;
	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;
	struct stream_port_s *port = priv->port;
	size_t towrite = count;
	if ((stbuf_space(vbuf) < count) || (stbuf_space(abuf) < count)) {
		if (file->f_flags & O_NONBLOCK) {
			towrite = min(stbuf_space(vbuf), stbuf_space(abuf));
			if (towrite < 1024)	/*? can write small? */
				return -EAGAIN;
		} else {
			if ((port->flag & PORT_FLAG_VID)
				&& (stbuf_space(vbuf) < count)) {
				r = stbuf_wait_space(vbuf, count);
				if (r < 0)
					return r;
			}
			if ((port->flag & PORT_FLAG_AID)
				&& (stbuf_space(abuf) < count)) {
				r = stbuf_wait_space(abuf, count);
				if (r < 0)
					return r;
			}
		}
	}
	towrite = min(towrite, count);
	return _rmparser_write(buf, towrite);
}

void rm_set_vasid(u32 vid, u32 aid)
{
	pr_info("rm_set_vasid aid %d, vid %d\n", aid, vid);
	WRITE_MPEG_REG(VAS_STREAM_ID, (aid << 8) | vid);

	return;
}

void rm_audio_reset(void)
{
	ulong flags;
	DEFINE_SPINLOCK(lock);

	spin_lock_irqsave(&lock, flags);

	WRITE_MPEG_REG(PARSER_AUDIO_WP,
				   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));
	WRITE_MPEG_REG(PARSER_AUDIO_RP,
				   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));

	WRITE_MPEG_REG(PARSER_AUDIO_START_PTR,
				   READ_MPEG_REG(AIU_MEM_AIFIFO_START_PTR));
	WRITE_MPEG_REG(PARSER_AUDIO_END_PTR,
				   READ_MPEG_REG(AIU_MEM_AIFIFO_END_PTR));
	CLEAR_MPEG_REG_MASK(PARSER_ES_CONTROL, ES_AUD_MAN_RD_PTR);

	WRITE_MPEG_REG(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
	CLEAR_MPEG_REG_MASK(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

	spin_unlock_irqrestore(&lock, flags);

	return;
}
