// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/highmem.h>
#include <linux/page-flags.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/sched/clock.h>

#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "vdin_ctl.h"
#include "vdin_regs.h"
#include "vdin_drv.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"
#include "vdin_afbce.h"
#include "vdin_dv.h"

#define VDIN2_DV_HSIZE	2048
#define VDIN2_DV_VSIZE	10

#define DBG_ON	0

#define dprintk(level, fmt, arg...) \
	do { \
		if (vdin_dbg_en >= (level)) \
			pr_info("vdin:dv " fmt, ## arg); \
	} while (0)

void vdin_wrmif2_enable(struct vdin_dev_s *devp, u32 en, unsigned int rdma_enable)
{
	if (devp->dtdata->hw_ver != VDIN_HW_T7 || devp->index)
		return;

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable) {
		/*clear int status*/
		rdma_write_reg_bits(devp->rdma_handle, VDIN2_WR_CTRL, 1,
				DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
		rdma_write_reg_bits(devp->rdma_handle, VDIN2_WR_CTRL, 0,
				DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);

		/*write mif2 int*/
		if (en)
			rdma_write_reg_bits(devp->rdma_handle, VDIN_TOP_DOUBLE_CTRL, 0x3,
				VDIN1_INT_MASK_BIT, 3);
		else
			rdma_write_reg_bits(devp->rdma_handle, VDIN_TOP_DOUBLE_CTRL, 0x7,
				VDIN1_INT_MASK_BIT, 3);

		if (en)
			rdma_write_reg_bits(devp->rdma_handle, VDIN2_WR_CTRL, 1, 8, 1);
		else
			rdma_write_reg_bits(devp->rdma_handle, VDIN2_WR_CTRL, 0, 8, 1);
	} else {
#endif
		/*clear int status*/
		wr_bits(0, VDIN2_WR_CTRL, 1,
				DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
		wr_bits(0, VDIN2_WR_CTRL, 0,
				DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
		/*write mif2 int*/
		if (en)
			wr_bits(0, VDIN_TOP_DOUBLE_CTRL, 0x3,
				VDIN1_INT_MASK_BIT, 3);
		else
			wr_bits(0, VDIN_TOP_DOUBLE_CTRL, 0x7,
				VDIN1_INT_MASK_BIT, 3);

		if (en)
			wr_bits(0, VDIN2_WR_CTRL, 1, 8, 1);
		else
			wr_bits(0, VDIN2_WR_CTRL, 0, 8, 1);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	}
#endif
	dprintk(1, "%s %d\n", __func__, en);
}

/*
 * mode_10b:0:8bit 1:10bit;
 * in_fmt: [1:0]: 0->422; 1->444
 *
 */
void vdin_wrmif2_initial(struct vdin_dev_s *devp)
{
	u32 offset = 0;
	u32 hsize = VDIN2_DV_HSIZE;
	u32 vsize = VDIN2_DV_VSIZE;
	u32 md_10b = 0;/*0:8bit 1:10bit*/
	u32 in_fmt = MIF_FMT_NV12_21;

	if (devp->dtdata->hw_ver != VDIN_HW_T7)
		return;

	/* vdin2 mif sel t7 vdin1 normal is meta data*/
	wr_bits(offset, VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN1_NOR, MIF2_OUT_SEL_BIT,
		VDIN_REORDER_SEL_WID);
	wr_bits(offset, VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN1_SML, MIF1_OUT_SEL_BIT,
		VDIN_REORDER_SEL_WID);

	/* scope */
	wr_bits(offset, VDIN2_WR_H_START_END, 0, 16, 13);//v start
	wr_bits(offset, VDIN2_WR_H_START_END, hsize - 1, 0, 13);//v end
	wr_bits(offset, VDIN2_WR_V_START_END, 0, 16, 13);//v start
	wr_bits(offset, VDIN2_WR_V_START_END, vsize - 1, 0, 13);//v end

	wr_bits(offset, VDIN2_WR_CTRL2, md_10b, 19, 1);//10bit mode
	wr_bits(offset, VDIN2_WR_CTRL2, 1, 18, 1);//data_ext_en

	wr_bits(offset, VDIN2_WR_CTRL, 1, 19, 1);       //swap between 64bit
	wr_bits(offset, VDIN2_WR_CTRL, in_fmt, 12, 2);  //vdin_wr_format
	wr_bits(offset, VDIN2_WR_CTRL, 1, 9, 1);        //vdin_wr_req_urgent
	//wr_bits(offset, VDIN2_WR_CTRL, 1, 8, 1);        //vdin_wr_req_en
	wr_bits(offset, VDIN2_WR_CTRL, 0, 27, 1); //eol from pixel count

	wr_bits(0, VDIN2_WR_URGENT_CTRL, 1, 9, 1);/*write done last sel*/
	wr_bits(0, VDIN2_WR_URGENT_CTRL, 1, 8, 1);/*reg Bvalid enable*/

	wr_bits(0, VDIN2_WR_CTRL, 1,
			DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
	wr_bits(0, VDIN2_WR_CTRL, 0,
			DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
}

/*
 * for t7 dv meta data issue
 */
void vdin_wrmif2_addr_update(struct vdin_dev_s *devp)
{
	u32 offset = 0;
	u32 stride_luma;
	u32 hsize = VDIN2_DV_HSIZE;
	u32 baddr;

	if (devp->dtdata->hw_ver != VDIN_HW_T7)
		return;

	baddr = devp->dv.meta_data_raw_p_buffer0;
	if (!baddr)
		dprintk(0, "err: meta_data_raw_p_buffer0\n");
	stride_luma = ((hsize * 8) + 511) >> 9;

	/*dprintk(0, "%s baddr:0x%x stride:0x%x\n", __func__,*/
	/*	baddr, stride_luma);*/

	wr(offset, VDIN2_WR_BADDR_LUMA, baddr >> 4);
	wr(offset, VDIN2_WR_STRIDE_LUMA, stride_luma << 2);
}

irqreturn_t vdin_wrmif2_dv_meta_wr_done_isr(int irq, void *dev_id)
{
	/*struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;*/
	irqreturn_t sts = IRQ_HANDLED;
	struct vdin_dev_s *devp = (struct vdin_dev_s *)dev_id;
	char *src_dv_meta_vaddr;
	u32 len_raw_data;
	char *dst_dv_meta_vaddr;
	u32 src_idx = 0, dst_idx = 0;
	/*u32 meta_len = 0;*/
	u8 data;
	u32 max_pkt = 15;
	static u32 irq_cnt;

	devp->stats.meta_wr_done_irq_cnt++;

	if (devp->dtdata->hw_ver != VDIN_HW_T7 ||
	    !(devp->flags & VDIN_FLAG_ISR_EN))
		return sts;

	src_dv_meta_vaddr = devp->dv.meta_data_raw_v_buffer0;
	dst_dv_meta_vaddr = devp->dv.meta_data_raw_buffer1;

	if (IS_ERR_OR_NULL(src_dv_meta_vaddr) ||
	    IS_ERR_OR_NULL(dst_dv_meta_vaddr)) {
		if (irq_cnt % dv_dbg_log_du)
			dprintk(0, "%s err: null meta addr\n", __func__);
		return sts;
	}

	/* not dv input */
	if (!vdin_is_dolby_signal_in(devp))
		return sts;

	dma_sync_single_for_device(&devp->this_pdev->dev,
				   devp->dv.meta_data_raw_p_buffer0,
				   K_DV_META_RAW_BUFF0,
				   DMA_TO_DEVICE);
	/*for debug*/
	//if ((irq_cnt % dv_dbg_log_du) == 0)
	//	vdin_dolby_pr_meta_data(src_dv_meta_vaddr, 128);

	len_raw_data = 128 * max_pkt * 8; /*K_DV_META_RAW_BUFF0;*/
	//if ((dv_dbg_log & (1 << 5)) && (irq_cnt % dv_dbg_log_du))
	//	pr_info("\n\n");
	dst_idx = 0;
	/* write meta raw data to meta_data_raw_buff1 */
	for (src_idx = 0; src_idx < len_raw_data;) {
		data = 0;
		/* 8 bytes raw data got one byte meta*/
		if (src_dv_meta_vaddr[src_idx + 0] == 0x80)
			data |= 0x1;
		if (src_dv_meta_vaddr[src_idx + 1] == 0x80)
			data |= 0x2;
		if (src_dv_meta_vaddr[src_idx + 2] == 0x80)
			data |= 0x4;
		if (src_dv_meta_vaddr[src_idx + 3] == 0x80)
			data |= 0x8;
		if (src_dv_meta_vaddr[src_idx + 4] == 0x80)
			data |= 0x10;
		if (src_dv_meta_vaddr[src_idx + 5] == 0x80)
			data |= 0x20;
		if (src_dv_meta_vaddr[src_idx + 6] == 0x80)
			data |= 0x40;
		if (src_dv_meta_vaddr[src_idx + 7] == 0x80)
			data |= 0x80;
		dst_dv_meta_vaddr[dst_idx] = data;
		dst_idx++;
		src_idx += 8;
	}

	//if ((dv_dbg_log & (1 << 5)) && (irq_cnt % dv_dbg_log_du))
	//	vdin_dolby_pr_meta_data(dst_dv_meta_vaddr,
	//				6 * DV_META_PACKET_SIZE);
	irq_cnt++;
	return sts;
}

/*
 * return value:
 *	true: dv is need tunnel
 *	false: dv not need tunnel
 */
bool vdin_dv_is_need_tunnel(struct vdin_dev_s *devp)
{
	if (devp->dv.dv_flag && /* && (devp->dv.low_latency) */ /* && is_amdv_enable() */
	    (!(is_amdv_stb_mode() && cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) ||
	    (is_amdv_stb_mode() && !is_hdmi_ll_as_hdr10())) &&
	    devp->prop.color_format == TVIN_YUV422 &&
	     !devp->debug.bypass_tunnel)
		return true;
	else
		return false;
}

/*
 * return value:
 *	true: dv is visf data
 *	false: dv no visf data
 */
bool vdin_dv_is_visf_data(struct vdin_dev_s *devp)
{
	if (((dv_dbg_mask & DV_UPDATE_DATA_MODE_DOLBY_WORK) == 0) &&
	    devp->dv.dv_config && !devp->dv.low_latency &&
	    devp->prop.dolby_vision == 1)
		return true;
	else
		return false;
}

/*
 * return value:
 *	true: dv is auto game not support manual set game
 *	false: support set game
 */
bool vdin_dv_not_manual_game(struct vdin_dev_s *devp)
{
	if (devp->dv.dv_flag && !devp->prop.latency.allm_mode &&
	    !devp->prop.low_latency && !devp->vrr_data.vrr_mode)
		return true;
	else
		return false;
}

/* some device force send dv 444 source-led need convert to 422 and bypass detunnel
 * return value:
 *	true: dv is not standard source-led
 *	false: dv is yuv422/420 12bit source-led
 */
bool vdin_dv_is_not_std_source_led(struct vdin_dev_s *devp)
{
	if (devp->dv.dv_flag &&
	    (devp->dv.low_latency || devp->prop.latency.allm_mode ||
	     devp->vrr_data.vrr_mode) &&
	    ((devp->prop.color_format == TVIN_RGB444 ||
	      devp->prop.color_format == TVIN_YUV444) ||
	     devp->prop.colordepth == 8))
		return true;
	else
		return false;
}
