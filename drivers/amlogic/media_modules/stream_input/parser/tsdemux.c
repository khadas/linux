/*
 * drivers/amlogic/media/stream_input/parser/tsdemux.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/device.h>
#include <linux/delay.h>

#include <linux/uaccess.h>
/* #include <mach/am_regs.h> */
#include <linux/clk.h>
/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
/* #include <mach/mod_gate.h> */
/* #endif */

#include "../../frame_provider/decoder/utils/vdec.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include "streambuf_reg.h"
#include "streambuf.h"
#include <linux/amlogic/media/utils/amports_config.h>

#include "tsdemux.h"
#include <linux/reset.h>
#include "../amports/amports_priv.h"

#define MAX_DRM_PACKAGE_SIZE 0x500000


static const char tsdemux_fetch_id[] = "tsdemux-fetch-id";
static const char tsdemux_irq_id[] = "tsdemux-irq-id";

static u32 curr_pcr_num = 0xffff;
static u32 curr_vid_id = 0xffff;
static u32 curr_aud_id = 0xffff;
static u32 curr_sub_id = 0xffff;
static u32 curr_pcr_id = 0xffff;

static DECLARE_WAIT_QUEUE_HEAD(wq);
static u32 fetch_done;
static u32 discontinued_counter;
static u32 first_pcr;
static u8 pcrscr_valid;
static u8 pcraudio_valid;
static u8 pcrvideo_valid;
static u8 pcr_init_flag;

static int demux_skipbyte;

static struct tsdemux_ops *demux_ops;
static DEFINE_SPINLOCK(demux_ops_lock);

static int enable_demux_driver(void)
{
	return demux_ops ? 1 : 0;
}

void tsdemux_set_ops(struct tsdemux_ops *ops)
{
	unsigned long flags;

	spin_lock_irqsave(&demux_ops_lock, flags);
	demux_ops = ops;
	spin_unlock_irqrestore(&demux_ops_lock, flags);
}
EXPORT_SYMBOL(tsdemux_set_ops);

int tsdemux_set_reset_flag_ext(void)
{
	int r = 0;

	if (demux_ops && demux_ops->set_reset_flag)
		r = demux_ops->set_reset_flag();

	return r;
}

int tsdemux_set_reset_flag(void)
{
	unsigned long flags;
	int r;

	spin_lock_irqsave(&demux_ops_lock, flags);
	r = tsdemux_set_reset_flag_ext();
	spin_unlock_irqrestore(&demux_ops_lock, flags);

	return r;
}

static int tsdemux_reset(void)
{
	unsigned long flags;
	int r = 0;

	spin_lock_irqsave(&demux_ops_lock, flags);
	if (demux_ops && demux_ops->reset) {
		tsdemux_set_reset_flag_ext();
		r = demux_ops->reset();
	}
	spin_unlock_irqrestore(&demux_ops_lock, flags);

	return r;
}

static int tsdemux_request_irq(irq_handler_t handler, void *data)
{
	unsigned long flags;
	int r = 0;

	spin_lock_irqsave(&demux_ops_lock, flags);
	if (demux_ops && demux_ops->request_irq)
		r = demux_ops->request_irq(handler, data);
	spin_unlock_irqrestore(&demux_ops_lock, flags);

	return r;
}

static int tsdemux_free_irq(void)
{
	unsigned long flags;
	int r = 0;

	spin_lock_irqsave(&demux_ops_lock, flags);
	if (demux_ops && demux_ops->free_irq)
		r = demux_ops->free_irq();
	spin_unlock_irqrestore(&demux_ops_lock, flags);

	return r;
}

static int tsdemux_set_vid(int vpid)
{
	unsigned long flags;
	int r = 0;

	spin_lock_irqsave(&demux_ops_lock, flags);
	if (demux_ops && demux_ops->set_vid)
		r = demux_ops->set_vid(vpid);
	spin_unlock_irqrestore(&demux_ops_lock, flags);

	return r;
}

static int tsdemux_set_aid(int apid)
{
	unsigned long flags;
	int r = 0;

	spin_lock_irqsave(&demux_ops_lock, flags);
	if (demux_ops && demux_ops->set_aid)
		r = demux_ops->set_aid(apid);
	spin_unlock_irqrestore(&demux_ops_lock, flags);

	return r;
}

static int tsdemux_set_sid(int spid)
{
	unsigned long flags;
	int r = 0;

	spin_lock_irqsave(&demux_ops_lock, flags);
	if (demux_ops && demux_ops->set_sid)
		r = demux_ops->set_sid(spid);
	spin_unlock_irqrestore(&demux_ops_lock, flags);

	return r;
}

static int tsdemux_set_pcrid(int pcrpid)
{
	unsigned long flags;
	int r = 0;

	spin_lock_irqsave(&demux_ops_lock, flags);
	if (demux_ops && demux_ops->set_pcrid)
		r = demux_ops->set_pcrid(pcrpid);
	spin_unlock_irqrestore(&demux_ops_lock, flags);

	return r;
}

static int tsdemux_set_skip_byte(int skipbyte)
{
	unsigned long flags;
	int r = 0;

	spin_lock_irqsave(&demux_ops_lock, flags);
	if (demux_ops && demux_ops->set_skipbyte)
		r = demux_ops->set_skipbyte(skipbyte);
	spin_unlock_irqrestore(&demux_ops_lock, flags);

	return r;
}

static int tsdemux_config(void)
{
	return 0;
}

static void tsdemux_pcr_set(unsigned int pcr);
/*TODO irq*/
static irqreturn_t tsdemux_isr(int irq, void *dev_id)
{
	u32 int_status = 0;
	int id = (long)dev_id;

	if (!enable_demux_driver()) {
		int_status = READ_DEMUX_REG(STB_INT_STATUS);
	} else {
		if (id == 0)
			int_status = READ_DEMUX_REG(STB_INT_STATUS);
		else if (id == 1)
			int_status = READ_DEMUX_REG(STB_INT_STATUS_2);
		else if (id == 2)
			int_status = READ_DEMUX_REG(STB_INT_STATUS_3);
	}

	if (int_status & (1 << NEW_PDTS_READY)) {
		if (!enable_demux_driver()) {
			u32 pdts_status = READ_DEMUX_REG(STB_PTS_DTS_STATUS);

			if (pdts_status & (1 << VIDEO_PTS_READY))
				pts_checkin_wrptr(PTS_TYPE_VIDEO,
					READ_DEMUX_REG(VIDEO_PDTS_WR_PTR),
					READ_DEMUX_REG(VIDEO_PTS_DEMUX));

			if (pdts_status & (1 << AUDIO_PTS_READY))
				pts_checkin_wrptr(PTS_TYPE_AUDIO,
					READ_DEMUX_REG(AUDIO_PDTS_WR_PTR),
					READ_DEMUX_REG(AUDIO_PTS_DEMUX));

			WRITE_DEMUX_REG(STB_PTS_DTS_STATUS, pdts_status);
		} else {
#define DMX_READ_REG(i, r)\
	((i) ? ((i == 1) ? READ_DEMUX_REG(r##_2) : \
		READ_DEMUX_REG(r##_3)) : READ_DEMUX_REG(r))

			u32 pdts_status = DMX_READ_REG(id, STB_PTS_DTS_STATUS);

			if (pdts_status & (1 << VIDEO_PTS_READY))
				pts_checkin_wrptr(PTS_TYPE_VIDEO,
					DMX_READ_REG(id, VIDEO_PDTS_WR_PTR),
					DMX_READ_REG(id, VIDEO_PTS_DEMUX));

			if (pdts_status & (1 << AUDIO_PTS_READY))
				pts_checkin_wrptr(PTS_TYPE_AUDIO,
					DMX_READ_REG(id, AUDIO_PDTS_WR_PTR),
					DMX_READ_REG(id, AUDIO_PTS_DEMUX));

			if (id == 1)
				WRITE_DEMUX_REG(STB_PTS_DTS_STATUS_2,
							pdts_status);
			else if (id == 2)
				WRITE_DEMUX_REG(STB_PTS_DTS_STATUS_3,
							pdts_status);
			else
				WRITE_DEMUX_REG(STB_PTS_DTS_STATUS,
							pdts_status);
		}
	}
	if (int_status & (1 << DIS_CONTINUITY_PACKET)) {
		discontinued_counter++;
		/* pr_info("discontinued counter=%d\n",discontinued_counter); */
	}
	if (int_status & (1 << SUB_PES_READY)) {
		/* TODO: put data to somewhere */
		/* pr_info("subtitle pes ready\n"); */
		wakeup_sub_poll();
	}
	if (int_status & (1<<PCR_READY)) {
		unsigned int pcr_pts = 0xffffffff;
		pcr_pts = DMX_READ_REG(id, PCR_DEMUX);
		tsdemux_pcr_set(pcr_pts);
	}

	if (!enable_demux_driver())
		WRITE_DEMUX_REG(STB_INT_STATUS, int_status);

	return IRQ_HANDLED;
}

static irqreturn_t parser_isr(int irq, void *dev_id)
{
	u32 int_status = READ_PARSER_REG(PARSER_INT_STATUS);

	WRITE_PARSER_REG(PARSER_INT_STATUS, int_status);

	if (int_status & PARSER_INTSTAT_FETCH_CMD) {
		fetch_done = 1;

		wake_up_interruptible(&wq);
	}

	return IRQ_HANDLED;
}

static ssize_t _tsdemux_write(const char __user *buf, size_t count,
							  int isphybuf)
{
	size_t r = count;
	const char __user *p = buf;
	u32 len;
	int ret;
	dma_addr_t dma_addr = 0;

	if (r > 0) {
		if (isphybuf)
			len = count;
		else {
			len = min_t(size_t, r, FETCHBUF_SIZE);
			if (copy_from_user(fetchbuf, p, len))
				return -EFAULT;

			dma_addr =
				dma_map_single(amports_get_dma_device(),
						fetchbuf,
						FETCHBUF_SIZE, DMA_TO_DEVICE);
			if (dma_mapping_error(amports_get_dma_device(),
						dma_addr))
				return -EFAULT;


		}

		fetch_done = 0;

		wmb();		/* Ensure fetchbuf  contents visible */

		if (isphybuf) {
			u32 buf_32 = (unsigned long)buf & 0xffffffff;
			WRITE_PARSER_REG(PARSER_FETCH_ADDR, buf_32);
		} else {
			WRITE_PARSER_REG(PARSER_FETCH_ADDR, dma_addr);
			dma_unmap_single(amports_get_dma_device(), dma_addr,
					FETCHBUF_SIZE, DMA_TO_DEVICE);
		}

		WRITE_PARSER_REG(PARSER_FETCH_CMD, (7 << FETCH_ENDIAN) | len);


		ret =
			wait_event_interruptible_timeout(wq, fetch_done != 0,
					HZ / 2);
		if (ret == 0) {
			WRITE_PARSER_REG(PARSER_FETCH_CMD, 0);
			pr_info("write timeout, retry\n");
			return -EAGAIN;
		} else if (ret < 0)
			return -ERESTARTSYS;

		p += len;
		r -= len;
	}

	return count - r;
}

#define PCR_EN                     12

static int reset_pcr_regs(void)
{
	u32 pcr_num;
	u32 pcr_regs = 0;
	if (curr_pcr_id >= 0x1FFF)
		return 0;
	/* set paramater to fetch pcr */
	pcr_num = 0;
	if (curr_pcr_id == curr_vid_id)
		pcr_num = 0;
	else if (curr_pcr_id == curr_aud_id)
		pcr_num = 1;
	else if (curr_pcr_id == curr_sub_id)
		pcr_num = 2;
	else
		pcr_num = 3;
	if (pcr_num != curr_pcr_num) {
		u32 clk_unit = 0;
		u32 clk_81 = 0;
		struct clk *clk;
		//clk = clk_get(NULL,"clk81");
		clk= devm_clk_get(amports_get_dma_device(),"clk_81");
		if (IS_ERR(clk) || clk == 0) {
			pr_info("[%s:%d] error clock\n", __func__, __LINE__);
			return 0;
		}
		clk_81 = clk_get_rate(clk);
		clk_unit = clk_81 / 90000;
		pr_info("[%s:%d] clk_81 = %x clk_unit =%x\n", __func__,
				__LINE__, clk_81, clk_unit);
		pcr_regs = 1 << PCR_EN | clk_unit;
		pr_info("[tsdemux_init] the set pcr_regs =%x\n", pcr_regs);
		if (READ_DEMUX_REG(TS_HIU_CTL_2) & 0x80) {
			WRITE_DEMUX_REG(PCR90K_CTL_2, pcr_regs);
			WRITE_DEMUX_REG(ASSIGN_PID_NUMBER_2, pcr_num);
			pr_info("[tsdemux_init] To use device 2,pcr_num=%d\n",
					pcr_num);
			pr_info("tsdemux_init] the read  pcr_regs= %x\n",
				READ_DEMUX_REG(PCR90K_CTL_2));
		} else if (READ_DEMUX_REG(TS_HIU_CTL_3) & 0x80) {
			WRITE_DEMUX_REG(PCR90K_CTL_3, pcr_regs);
			WRITE_DEMUX_REG(ASSIGN_PID_NUMBER_3, pcr_num);
			pr_info("[tsdemux_init] To use device 3,pcr_num=%d\n",
					pcr_num);
			pr_info("tsdemux_init] the read  pcr_regs= %x\n",
				READ_DEMUX_REG(PCR90K_CTL_3));
		} else {
			WRITE_DEMUX_REG(PCR90K_CTL, pcr_regs);
			WRITE_DEMUX_REG(ASSIGN_PID_NUMBER, pcr_num);
			pr_info("[tsdemux_init] To use device 1,pcr_num=%d\n",
					pcr_num);
			pr_info("tsdemux_init] the read  pcr_regs= %x\n",
				READ_DEMUX_REG(PCR90K_CTL));
		}
		curr_pcr_num = pcr_num;
	}
	return 1;
}

s32 tsdemux_init(u32 vid, u32 aid, u32 sid, u32 pcrid, bool is_hevc,
		struct vdec_s *vdec)
{
	s32 r;
	u32 parser_sub_start_ptr;
	u32 parser_sub_end_ptr;
	u32 parser_sub_rp;
	pcrvideo_valid = 0;
	pcraudio_valid = 0;
	pcr_init_flag = 0;

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	/*TODO clk */
	/*
	 *switch_mod_gate_by_type(MOD_DEMUX, 1);
	 */
	/* #endif */

	amports_switch_gate("demux", 1);

	parser_sub_start_ptr = READ_PARSER_REG(PARSER_SUB_START_PTR);
	parser_sub_end_ptr = READ_PARSER_REG(PARSER_SUB_END_PTR);
	parser_sub_rp = READ_PARSER_REG(PARSER_SUB_RP);

	WRITE_RESET_REG(RESET1_REGISTER, RESET_PARSER);

	if (enable_demux_driver()) {
		tsdemux_reset();
	} else {
		WRITE_RESET_REG(RESET1_REGISTER, RESET_PARSER | RESET_DEMUXSTB);

		WRITE_DEMUX_REG(STB_TOP_CONFIG, 0);
		WRITE_DEMUX_REG(DEMUX_CONTROL, 0);
	}

	/* set PID filter */
	pr_info
		("tsdemux video_pid = 0x%x, audio_pid = 0x%x,",
		 vid, aid);
	pr_info
		("sub_pid = 0x%x, pcrid = 0x%x\n",
		 sid, pcrid);

	if (!enable_demux_driver()) {
		WRITE_DEMUX_REG(FM_WR_DATA,
				(((vid < 0x1fff)
					? (vid & 0x1fff) | (VIDEO_PACKET << 13)
					: 0xffff) << 16)
				| ((aid < 0x1fff)
					? (aid & 0x1fff) | (AUDIO_PACKET << 13)
					: 0xffff));
		WRITE_DEMUX_REG(FM_WR_ADDR, 0x8000);
		while (READ_DEMUX_REG(FM_WR_ADDR) & 0x8000)
			;

		WRITE_DEMUX_REG(FM_WR_DATA,
				(((sid < 0x1fff)
					? (sid & 0x1fff) | (SUB_PACKET << 13)
					: 0xffff) << 16)
				| 0xffff);
		WRITE_DEMUX_REG(FM_WR_ADDR, 0x8001);
		while (READ_DEMUX_REG(FM_WR_ADDR) & 0x8000)
			;

		WRITE_DEMUX_REG(MAX_FM_COMP_ADDR, 1);

		WRITE_DEMUX_REG(STB_INT_MASK, 0);
		WRITE_DEMUX_REG(STB_INT_STATUS, 0xffff);

		/* TS data path */
		WRITE_DEMUX_REG(FEC_INPUT_CONTROL, 0x7000);
		WRITE_DEMUX_REG(DEMUX_MEM_REQ_EN,
				(1 << VIDEO_PACKET) |
				(1 << AUDIO_PACKET) | (1 << SUB_PACKET));
		WRITE_DEMUX_REG(DEMUX_ENDIAN,
				(7 << OTHER_ENDIAN) |
				(7 << BYPASS_ENDIAN) | (0 << SECTION_ENDIAN));
		WRITE_DEMUX_REG(TS_HIU_CTL, 1 << USE_HI_BSF_INTERFACE);
		WRITE_DEMUX_REG(TS_FILE_CONFIG,
				(demux_skipbyte << 16) |
				(6 << DES_OUT_DLY) |
				(3 << TRANSPORT_SCRAMBLING_CONTROL_ODD) |
				(1 << TS_HIU_ENABLE) | (4 << FEC_FILE_CLK_DIV));

		/* enable TS demux */
		WRITE_DEMUX_REG(DEMUX_CONTROL,
				(1 << STB_DEMUX_ENABLE) |
				(1 << KEEP_DUPLICATE_PACKAGE));
	}

	if (fetchbuf == 0) {
		pr_info("%s: no fetchbuf\n", __func__);
		return -ENOMEM;
	}

	/* hook stream buffer with PARSER */
	if (has_hevc_vdec() && is_hevc) {
		WRITE_PARSER_REG(PARSER_VIDEO_START_PTR, vdec->input.start);
		WRITE_PARSER_REG(PARSER_VIDEO_END_PTR, vdec->input.start +
			vdec->input.size - 8);

		if (vdec_single(vdec)) {
			CLEAR_PARSER_REG_MASK(PARSER_ES_CONTROL,
					ES_VID_MAN_RD_PTR);
			/* set vififo_vbuf_rp_sel=>hevc */
			WRITE_VREG(DOS_GEN_CTRL0, 3 << 1);
			/* set use_parser_vbuf_wp */
			SET_VREG_MASK(HEVC_STREAM_CONTROL,
					  (1 << 3) | (0 << 4));
			/* set stream_fetch_enable */
			SET_VREG_MASK(HEVC_STREAM_CONTROL, 1);
			/* set stream_buffer_hole with 256 bytes */
			SET_VREG_MASK(HEVC_STREAM_FIFO_CTL,
					  (1 << 29));
		} else {
			SET_PARSER_REG_MASK(PARSER_ES_CONTROL,
					ES_VID_MAN_RD_PTR);
			WRITE_PARSER_REG(PARSER_VIDEO_WP, vdec->input.start);
			WRITE_PARSER_REG(PARSER_VIDEO_RP, vdec->input.start);
		}
	} else {
		WRITE_PARSER_REG(PARSER_VIDEO_START_PTR, vdec->input.start);
		WRITE_PARSER_REG(PARSER_VIDEO_END_PTR, vdec->input.start +
			vdec->input.size - 8);

		if (vdec_single(vdec)) {
			CLEAR_PARSER_REG_MASK(PARSER_ES_CONTROL,
					ES_VID_MAN_RD_PTR);

			WRITE_VREG(VLD_MEM_VIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
			CLEAR_VREG_MASK(VLD_MEM_VIFIFO_BUF_CNTL,
					MEM_BUFCTRL_INIT);
			/* set vififo_vbuf_rp_sel=>vdec */
			if (has_hevc_vdec())
				WRITE_VREG(DOS_GEN_CTRL0, 0);
		} else {
			SET_PARSER_REG_MASK(PARSER_ES_CONTROL,
					ES_VID_MAN_RD_PTR);
			WRITE_PARSER_REG(PARSER_VIDEO_WP, vdec->input.start);
			WRITE_PARSER_REG(PARSER_VIDEO_RP, vdec->input.start);
		}
	}

	WRITE_PARSER_REG(PARSER_AUDIO_START_PTR,
				   READ_AIU_REG(AIU_MEM_AIFIFO_START_PTR));
	WRITE_PARSER_REG(PARSER_AUDIO_END_PTR,
				   READ_AIU_REG(AIU_MEM_AIFIFO_END_PTR));
	CLEAR_PARSER_REG_MASK(PARSER_ES_CONTROL, ES_AUD_MAN_RD_PTR);

	WRITE_PARSER_REG(PARSER_CONFIG,
				   (10 << PS_CFG_PFIFO_EMPTY_CNT_BIT) |
				   (1 << PS_CFG_MAX_ES_WR_CYCLE_BIT) |
				   (16 << PS_CFG_MAX_FETCH_CYCLE_BIT));

	WRITE_AIU_REG(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
	CLEAR_AIU_REG_MASK(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

	WRITE_PARSER_REG(PARSER_SUB_START_PTR, parser_sub_start_ptr);
	WRITE_PARSER_REG(PARSER_SUB_END_PTR, parser_sub_end_ptr);
	WRITE_PARSER_REG(PARSER_SUB_RP, parser_sub_rp);
	SET_PARSER_REG_MASK(PARSER_ES_CONTROL,
			(7 << ES_SUB_WR_ENDIAN_BIT) | ES_SUB_MAN_RD_PTR);

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if (has_hevc_vdec())
		r = pts_start((is_hevc) ? PTS_TYPE_HEVC : PTS_TYPE_VIDEO);
	else
		/* #endif */
		r = pts_start(PTS_TYPE_VIDEO);

	if (r < 0) {
		pr_info("Video pts start failed.(%d)\n", r);
		goto err1;
	}
	r = pts_start(PTS_TYPE_AUDIO);
	if (r < 0) {
		pr_info("Audio pts start failed.(%d)\n", r);
		goto err2;
	}
	/*TODO irq */

	r = vdec_request_irq(PARSER_IRQ, parser_isr,
			"tsdemux-fetch", (void *)tsdemux_fetch_id);

	if (r)
		goto err3;

	WRITE_PARSER_REG(PARSER_INT_STATUS, 0xffff);
	WRITE_PARSER_REG(PARSER_INT_ENABLE,
			PARSER_INTSTAT_FETCH_CMD << PARSER_INT_HOST_EN_BIT);

	WRITE_PARSER_REG(PARSER_VIDEO_HOLE, 0x400);
	WRITE_PARSER_REG(PARSER_AUDIO_HOLE, 0x400);

	discontinued_counter = 0;

	if (!enable_demux_driver()) {
		/*TODO irq */

		r = vdec_request_irq(DEMUX_IRQ, tsdemux_isr,
				"tsdemux-irq", (void *)tsdemux_irq_id);

		WRITE_DEMUX_REG(STB_INT_MASK, (1 << SUB_PES_READY)
					   | (1 << NEW_PDTS_READY)
					   | (1 << DIS_CONTINUITY_PACKET));
		if (r)
			goto err4;
	} else {
		tsdemux_config();
		tsdemux_request_irq(tsdemux_isr, (void *)tsdemux_irq_id);
		if (vid < 0x1FFF) {
			curr_vid_id = vid;
			tsdemux_set_vid(vid);
			pcrvideo_valid = 1;
		}
		if (aid < 0x1FFF) {
			curr_aud_id = aid;
			tsdemux_set_aid(aid);
			pcraudio_valid = 1;
		}
		if (sid < 0x1FFF) {
			curr_sub_id = sid;
			tsdemux_set_sid(sid);
		}

		curr_pcr_id = pcrid;
		pcrscr_valid = reset_pcr_regs();

		if ((pcrid < 0x1FFF) && (pcrid != vid) && (pcrid != aid)
			&& (pcrid != sid))
			tsdemux_set_pcrid(pcrid);
	}

	first_pcr = 0;

	return 0;

err4:
	/*TODO irq */

	if (!enable_demux_driver())
		vdec_free_irq(PARSER_IRQ, (void *)tsdemux_fetch_id);

err3:
	pts_stop(PTS_TYPE_AUDIO);
err2:
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if (has_hevc_vdec())
		pts_stop((is_hevc) ? PTS_TYPE_HEVC : PTS_TYPE_VIDEO);
	else
		/* #endif */
		pts_stop(PTS_TYPE_VIDEO);
err1:
	pr_info("TS Demux init failed.\n");
	return -ENOENT;
}

void tsdemux_release(void)
{
	pcrscr_valid = 0;
	first_pcr = 0;
	pcr_init_flag = 0;

	WRITE_PARSER_REG(PARSER_INT_ENABLE, 0);
	WRITE_PARSER_REG(PARSER_VIDEO_HOLE, 0);
	WRITE_PARSER_REG(PARSER_AUDIO_HOLE, 0);

	/*TODO irq */

	vdec_free_irq(PARSER_IRQ, (void *)tsdemux_fetch_id);

	if (!enable_demux_driver()) {
		WRITE_DEMUX_REG(STB_INT_MASK, 0);
		/*TODO irq */

		vdec_free_irq(DEMUX_IRQ, (void *)tsdemux_irq_id);
	} else {

		tsdemux_set_aid(0xffff);
		tsdemux_set_vid(0xffff);
		tsdemux_set_sid(0xffff);
		tsdemux_set_pcrid(0xffff);
		tsdemux_free_irq();

		curr_vid_id = 0xffff;
		curr_aud_id = 0xffff;
		curr_sub_id = 0xffff;
		curr_pcr_id = 0xffff;
		curr_pcr_num = 0xffff;
	}

	pts_stop(PTS_TYPE_VIDEO);
	pts_stop(PTS_TYPE_AUDIO);

	WRITE_RESET_REG(RESET1_REGISTER, RESET_PARSER);
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	SET_PARSER_REG_MASK(PARSER_ES_CONTROL, ES_VID_MAN_RD_PTR);
	WRITE_PARSER_REG(PARSER_VIDEO_WP, 0);
	WRITE_PARSER_REG(PARSER_VIDEO_RP, 0);
#endif

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
	/*TODO clk */
	/*
	 *switch_mod_gate_by_type(MOD_DEMUX, 0);
	 */
	/* #endif */
	amports_switch_gate("demux", 0);

}
EXPORT_SYMBOL(tsdemux_release);

static int limited_delay_check(struct file *file,
		struct stream_buf_s *vbuf,
		struct stream_buf_s *abuf,
		const char __user *buf, size_t count)
{
	int write_size;

	if (vbuf->max_buffer_delay_ms > 0 && abuf->max_buffer_delay_ms > 0 &&
		stbuf_level(vbuf) > 1024 && stbuf_level(abuf) > 256) {
		int vdelay =
			calculation_stream_delayed_ms(PTS_TYPE_VIDEO,
					NULL, NULL);
		int adelay =
			calculation_stream_delayed_ms(PTS_TYPE_AUDIO,
					NULL, NULL);
		/*max wait 100ms,if timeout,try again top level. */
		int maxretry = 10;
		/*too big  delay,do wait now. */
		/*if noblock mode,don't do wait. */
		if (!(file->f_flags & O_NONBLOCK)) {
			while (vdelay > vbuf->max_buffer_delay_ms
				   && adelay > abuf->max_buffer_delay_ms
				   && maxretry-- > 0) {
				msleep(20);
				vdelay =
					calculation_stream_delayed_ms
					(PTS_TYPE_VIDEO, NULL, NULL);
				adelay =
					calculation_stream_delayed_ms
					(PTS_TYPE_AUDIO, NULL, NULL);
			}
		}
		if (vdelay > vbuf->max_buffer_delay_ms
			&& adelay > abuf->max_buffer_delay_ms)
			return 0;
	}
	write_size = min(stbuf_space(vbuf), stbuf_space(abuf));
	write_size = min_t(int, count, write_size);
	return write_size;
}

ssize_t drm_tswrite(struct file *file,
					struct stream_buf_s *vbuf,
					struct stream_buf_s *abuf,
					const char __user *buf, size_t count)
{
	s32 r;
	u32 realcount = count;
	u32 re_count = count;
	u32 havewritebytes = 0;

	struct drm_info tmpmm;
	struct drm_info *drm = &tmpmm;
	u32 res = 0;
	int isphybuf = 0;
	unsigned long realbuf;

	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;
	struct stream_port_s *port = priv->port;
	size_t wait_size, write_size;

	if (buf == NULL || count == 0)
		return -EINVAL;

	res = copy_from_user(drm, buf, sizeof(struct drm_info));
	if (res) {
		pr_info("drm kmalloc failed res[%d]\n", res);
		return -EFAULT;
	}

	if (drm->drm_flag == TYPE_DRMINFO && drm->drm_level == DRM_LEVEL1) {
		/* buf only has drminfo not have esdata; */
		if (drm->drm_pktsize <= MAX_DRM_PACKAGE_SIZE)
			realcount = drm->drm_pktsize;
		else {
			pr_err("drm package size is error, size is %u\n", drm->drm_pktsize);
			return -EINVAL;
		}
		realbuf = drm->drm_phy;
		isphybuf = 1;
	} else
	    realbuf = (unsigned long)buf;
	/* pr_info("drm->drm_flag = 0x%x,realcount = %d , buf = 0x%x ",*/
	   /*drm->drm_flag,realcount, buf); */

	count = realcount;

	while (count > 0) {
		if ((stbuf_space(vbuf) < count) ||
				(stbuf_space(abuf) < count)) {
			if (file->f_flags & O_NONBLOCK) {
				int v_stbuf_space = stbuf_space(vbuf);
				int a_stbuf_space = stbuf_space(abuf);

				write_size = min(v_stbuf_space, a_stbuf_space);
				/*have 188 bytes,write now., */
				if (write_size <= 188)
					return -EAGAIN;
			} else {
				wait_size =
					min(stbuf_canusesize(vbuf) / 8,
						stbuf_canusesize(abuf) / 4);
				if ((port->flag & PORT_FLAG_VID)
					&& (stbuf_space(vbuf) < wait_size)) {
					r = stbuf_wait_space(vbuf, wait_size);

					if (r < 0) {
						pr_info
						("write no space--- ");
						pr_info
						("no space,%d--%d,r-%d\n",
						 stbuf_space(vbuf),
						 stbuf_space(abuf), r);
						return r;
					}
				}

				if ((port->flag & PORT_FLAG_AID)
					&& (stbuf_space(abuf) < wait_size)) {
					r = stbuf_wait_space(abuf, wait_size);

					if (r < 0) {
						pr_info
						("write no stbuf_wait_space--");
						pr_info
						("no space,%d--%d,r-%d\n",
						 stbuf_space(vbuf),
						 stbuf_space(abuf), r);
						return r;
					}
				}
			}
		}

		write_size = min(stbuf_space(vbuf), stbuf_space(abuf));
		write_size = min(count, write_size);
		/* pr_info("write_size = %d,count = %d,\n",*/
		   /*write_size, count); */
		if (write_size > 0)
			r = _tsdemux_write((const char __user *)realbuf,
					 write_size, isphybuf);
		else
			return -EAGAIN;

		havewritebytes += r;

		/* pr_info("havewritebytes = %d, r = %d,\n",*/
		   /*havewritebytes,  r); */
		if (havewritebytes == realcount)
			break;	/* write ok; */
		else if (havewritebytes > realcount)
			pr_info(" error ! write too much\n");

		count -= r;
	}
	return re_count;
}

ssize_t tsdemux_write(struct file *file,
					  struct stream_buf_s *vbuf,
					  struct stream_buf_s *abuf,
					  const char __user *buf, size_t count)
{
	s32 r;
	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;
	struct stream_port_s *port = priv->port;
	size_t wait_size, write_size;

	if ((stbuf_space(vbuf) < count) || (stbuf_space(abuf) < count)) {
		if (file->f_flags & O_NONBLOCK) {
			write_size = min(stbuf_space(vbuf), stbuf_space(abuf));
			if (write_size <= 188)	/*have 188 bytes,write now., */
				return -EAGAIN;
		} else {
			wait_size =
				min(stbuf_canusesize(vbuf) / 8,
					stbuf_canusesize(abuf) / 4);
			if ((port->flag & PORT_FLAG_VID)
				&& (stbuf_space(vbuf) < wait_size)) {
				r = stbuf_wait_space(vbuf, wait_size);

				if (r < 0) {
					/* pr_info("write no space--- ");
					 *   pr_info("no space,%d--%d,r-%d\n",
					 *   stbuf_space(vbuf),
					 *   stbuf_space(abuf),r);
					 */
					return r;
				}
			}

			if ((port->flag & PORT_FLAG_AID)
				&& (stbuf_space(abuf) < wait_size)) {
				r = stbuf_wait_space(abuf, wait_size);

				if (r < 0) {
					/* pr_info("write no stbuf_wait_space")'
					 * pr_info{"---no space,%d--%d,r-%d\n",
					 * stbuf_space(vbuf),
					 * stbuf_space(abuf),r);
					 */
					return r;
				}
			}
		}
	}
	vbuf->last_write_jiffies64 = jiffies_64;
	abuf->last_write_jiffies64 = jiffies_64;
	write_size = limited_delay_check(file, vbuf, abuf, buf, count);
	if (write_size > 0)
		return _tsdemux_write(buf, write_size, 0);
	else
		return -EAGAIN;
}

static ssize_t show_discontinue_counter(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", discontinued_counter);
}

static struct class_attribute tsdemux_class_attrs[] = {
	__ATTR(discontinue_counter, S_IRUGO, show_discontinue_counter, NULL),
	__ATTR_NULL
};

static struct class tsdemux_class = {
		.name = "tsdemux",
		.class_attrs = tsdemux_class_attrs,
	};

int tsdemux_class_register(void)
{
	int r = class_register(&tsdemux_class);

	if (r < 0)
		pr_info("register tsdemux class error!\n");
	discontinued_counter = 0;
	return r;
}

void tsdemux_class_unregister(void)
{
	class_unregister(&tsdemux_class);
}

void tsdemux_change_avid(unsigned int vid, unsigned int aid)
{
	if (!enable_demux_driver()) {
		WRITE_DEMUX_REG(FM_WR_DATA,
				(((vid & 0x1fff) | (VIDEO_PACKET << 13)) << 16)
				| ((aid & 0x1fff) | (AUDIO_PACKET << 13)));
		WRITE_DEMUX_REG(FM_WR_ADDR, 0x8000);
		while (READ_DEMUX_REG(FM_WR_ADDR) & 0x8000)
			;
	} else {
		curr_vid_id = vid;
		curr_aud_id = aid;

		tsdemux_set_vid(vid);
		tsdemux_set_aid(aid);

		reset_pcr_regs();
	}

}

void tsdemux_change_sid(unsigned int sid)
{
	if (!enable_demux_driver()) {
		WRITE_DEMUX_REG(FM_WR_DATA,
				(((sid & 0x1fff) | (SUB_PACKET << 13)) << 16)
				| 0xffff);
		WRITE_DEMUX_REG(FM_WR_ADDR, 0x8001);
		while (READ_DEMUX_REG(FM_WR_ADDR) & 0x8000)
			;
	} else {
		curr_sub_id = sid;

		tsdemux_set_sid(sid);

		reset_pcr_regs();
	}

}

void tsdemux_audio_reset(void)
{
	ulong flags;

	DEFINE_SPINLOCK(lock);

	spin_lock_irqsave(&lock, flags);

	WRITE_PARSER_REG(PARSER_AUDIO_WP,
				   READ_AIU_REG(AIU_MEM_AIFIFO_START_PTR));
	WRITE_PARSER_REG(PARSER_AUDIO_RP,
				   READ_AIU_REG(AIU_MEM_AIFIFO_START_PTR));

	WRITE_PARSER_REG(PARSER_AUDIO_START_PTR,
				   READ_AIU_REG(AIU_MEM_AIFIFO_START_PTR));
	WRITE_PARSER_REG(PARSER_AUDIO_END_PTR,
				   READ_AIU_REG(AIU_MEM_AIFIFO_END_PTR));
	CLEAR_PARSER_REG_MASK(PARSER_ES_CONTROL, ES_AUD_MAN_RD_PTR);

	WRITE_AIU_REG(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);
	CLEAR_AIU_REG_MASK(AIU_MEM_AIFIFO_BUF_CNTL, MEM_BUFCTRL_INIT);

	spin_unlock_irqrestore(&lock, flags);

}

void tsdemux_sub_reset(void)
{
	ulong flags;
	DEFINE_SPINLOCK(lock);
	u32 parser_sub_start_ptr;
	u32 parser_sub_end_ptr;

	spin_lock_irqsave(&lock, flags);

	parser_sub_start_ptr = READ_PARSER_REG(PARSER_SUB_START_PTR);
	parser_sub_end_ptr = READ_PARSER_REG(PARSER_SUB_END_PTR);

	WRITE_PARSER_REG(PARSER_SUB_START_PTR, parser_sub_start_ptr);
	WRITE_PARSER_REG(PARSER_SUB_END_PTR, parser_sub_end_ptr);
	WRITE_PARSER_REG(PARSER_SUB_RP, parser_sub_start_ptr);
	WRITE_PARSER_REG(PARSER_SUB_WP, parser_sub_start_ptr);
	SET_PARSER_REG_MASK(PARSER_ES_CONTROL,
			(7 << ES_SUB_WR_ENDIAN_BIT) | ES_SUB_MAN_RD_PTR);

	spin_unlock_irqrestore(&lock, flags);

}

void tsdemux_set_skipbyte(int skipbyte)
{
	if (!enable_demux_driver())
		demux_skipbyte = skipbyte;
	else
		tsdemux_set_skip_byte(skipbyte);

}

void tsdemux_set_demux(int dev)
{
	if (enable_demux_driver()) {
		unsigned long flags;
		int r = 0;

		spin_lock_irqsave(&demux_ops_lock, flags);
		if (demux_ops && demux_ops->set_demux)
			r = demux_ops->set_demux(dev);
		spin_unlock_irqrestore(&demux_ops_lock, flags);
	}
}

u32 tsdemux_pcrscr_get(void)
{
	u32 pcr = 0;

	if (pcrscr_valid == 0)
		return 0;

	if (READ_DEMUX_REG(TS_HIU_CTL_2) & 0x80)
		pcr = READ_DEMUX_REG(PCR_DEMUX_2);
	else if (READ_DEMUX_REG(TS_HIU_CTL_3) & 0x80)
		pcr = READ_DEMUX_REG(PCR_DEMUX_3);
	else
		pcr = READ_DEMUX_REG(PCR_DEMUX);
	if (first_pcr == 0)
		first_pcr = pcr;
	return pcr;
}

u32 tsdemux_first_pcrscr_get(void)
{
	if (pcrscr_valid == 0)
		return 0;

	if (first_pcr == 0) {
		u32 pcr;
		if (READ_DEMUX_REG(TS_HIU_CTL_2) & 0x80)
			pcr = READ_DEMUX_REG(PCR_DEMUX_2);
		else if (READ_DEMUX_REG(TS_HIU_CTL_3) & 0x80)
			pcr = READ_DEMUX_REG(PCR_DEMUX_3);
		else
			pcr = READ_DEMUX_REG(PCR_DEMUX);
		first_pcr = pcr;
		/* pr_info("set first_pcr = 0x%x\n", pcr); */
	}

	return first_pcr;
}

u8 tsdemux_pcrscr_valid(void)
{
	return pcrscr_valid;
}

u8 tsdemux_pcraudio_valid(void)
{
	return pcraudio_valid;
}

u8 tsdemux_pcrvideo_valid(void)
{
	return pcrvideo_valid;
}

void tsdemux_pcr_set(unsigned int pcr)
{
	if (pcr_init_flag == 0) {
		/*timestamp_pcrscr_set(pcr);
		timestamp_pcrscr_enable(1);*/
		pcr_init_flag = 1;
	}
}

void tsdemux_tsync_func_init(void)
{
	register_tsync_callbackfunc(
		TSYNC_PCRSCR_VALID, (void *)(tsdemux_pcrscr_valid));
	register_tsync_callbackfunc(
		TSYNC_PCRSCR_GET, (void *)(tsdemux_pcrscr_get));
	register_tsync_callbackfunc(
		TSYNC_FIRST_PCRSCR_GET, (void *)(tsdemux_first_pcrscr_get));
	register_tsync_callbackfunc(
		TSYNC_PCRAUDIO_VALID, (void *)(tsdemux_pcraudio_valid));
	register_tsync_callbackfunc(
		TSYNC_PCRVIDEO_VALID, (void *)(tsdemux_pcrvideo_valid));
	register_tsync_callbackfunc(
		TSYNC_BUF_BY_BYTE, (void *)(get_buf_by_type));
	register_tsync_callbackfunc(
		TSYNC_STBUF_LEVEL, (void *)(stbuf_level));
	register_tsync_callbackfunc(
		TSYNC_STBUF_SPACE, (void *)(stbuf_space));
	register_tsync_callbackfunc(
		TSYNC_STBUF_SIZE, (void *)(stbuf_size));
}


