// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/hdmi_tx21/enc_clk_config.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include "common.h"
#include "register.h"
#include "../hdmi_tx.h"

/* ENCP_VIDEO_H/V are used for fetching video data from VPP
 * and send data to ENCP_DVI/DE_H/V* via a fifo (pixel delay)
 * The input timing of HDMITx is from ENCP_DVI/DE_H/V*
 */
static void config_tv_enc_calc(enum hdmi_vic vic)
{
	struct hdmi_timing *tp = NULL;
	struct hdmi_timing timing = {0};
	const u32 hsync_st = 4; // hsync start pixel count
	const u32 vsync_st = 1; // vsync start line count
	// Latency in pixel clock from ENCP_VFIFO2VD request to data ready to HDMI
	const u32 vfifo2vd_to_hdmi_latency = 2;
	u32 de_h_begin = 0;
	u32 de_h_end = 0;
	u32 de_v_begin = 0;
	u32 de_v_end = 0;

	tp = hdmitx21_gettiming(vic);
	if (!tp) {
		pr_info("not find hdmitx vic %d timing\n", vic);
		return;
	}
	pr_info("%s[%d] vic = %d\n", __func__, __LINE__, tp->vic);

	timing = *tp;
	tp = &timing;

	de_h_end = tp->h_total - (tp->h_front - hsync_st);
	de_h_begin = de_h_end - tp->h_active;
	de_v_end = tp->v_total - (tp->v_front - vsync_st);
	de_v_begin = de_v_end - tp->v_active;

	// set DVI/HDMI transfer timing
	// generate hsync
	hd21_write_reg(ENCP_DVI_HSO_BEGIN, hsync_st);
	hd21_write_reg(ENCP_DVI_HSO_END, hsync_st + tp->h_sync);

	// generate vsync
	hd21_write_reg(ENCP_DVI_VSO_BLINE_EVN, vsync_st);
	hd21_write_reg(ENCP_DVI_VSO_ELINE_EVN, vsync_st + tp->v_sync);
	hd21_write_reg(ENCP_DVI_VSO_BEGIN_EVN, hsync_st);
	hd21_write_reg(ENCP_DVI_VSO_END_EVN, hsync_st);

	// generate data valid
	hd21_write_reg(ENCP_DE_H_BEGIN, de_h_begin);
	hd21_write_reg(ENCP_DE_H_END, de_h_end);
	hd21_write_reg(ENCP_DE_V_BEGIN_EVEN, de_v_begin);
	hd21_write_reg(ENCP_DE_V_END_EVEN, de_v_end);

	// set mode
	// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
	hd21_write_reg(ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
	hd21_write_reg(ENCP_VIDEO_MODE_ADV, 0x18); // Sampling rate: 1
	// hd21_write_reg(ENCP_VIDEO_YFP1_HTIME, 140);
	// hd21_write_reg(ENCP_VIDEO_YFP2_HTIME, 2060);

	// set active region
	hd21_write_reg(ENCP_VIDEO_HAVON_BEGIN, de_h_begin - vfifo2vd_to_hdmi_latency);
	hd21_write_reg(ENCP_VIDEO_HAVON_END, de_h_end - vfifo2vd_to_hdmi_latency - 1);
	hd21_write_reg(ENCP_VIDEO_VAVON_BLINE, de_v_begin);
	hd21_write_reg(ENCP_VIDEO_VAVON_ELINE, de_v_end - 1);

	//set hsync
	hd21_write_reg(ENCP_VIDEO_HSO_BEGIN, hsync_st - vfifo2vd_to_hdmi_latency);
	hd21_write_reg(ENCP_VIDEO_HSO_END, hsync_st + tp->h_sync - vfifo2vd_to_hdmi_latency);

	//set vsync
	hd21_write_reg(ENCP_VIDEO_VSO_BEGIN, 0);
	hd21_write_reg(ENCP_VIDEO_VSO_END, 0);
	hd21_write_reg(ENCP_VIDEO_VSO_BLINE, vsync_st);
	hd21_write_reg(ENCP_VIDEO_VSO_ELINE, vsync_st + tp->v_sync);

	//set vtotal & htotal
	hd21_write_reg(ENCP_VIDEO_MAX_PXCNT, tp->h_total - 1);
	hd21_write_reg(ENCP_VIDEO_MAX_LNCNT, tp->v_total - 1);
	// set vfifo2vd
	// if(vfifo2vd_en) {
		// hd21_write_reg(ENCP_VFIFO2VD_CTL, vfifo2vd_en);
		// hd21_write_reg(ENCP_VFIFO2VD_PIXEL_START, de_h_begin - 2);
		// hd21_write_reg(ENCP_VFIFO2VD_PIXEL_END, de_h_end - 2);
		// hd21_write_reg(ENCP_VFIFO2VD_LINE_TOP_START, de_v_begin);
		// hd21_write_reg(ENCP_VFIFO2VD_LINE_TOP_END, de_v_end);
	// }
}

void set_tv_encp_new(u32 enc_index, enum hdmi_vic vic, u32 enable)
{
	u32 reg_offset;
	// VENC0A 0x1b
	// VENC1A 0x21
	// VENC2A 0x23
	reg_offset = enc_index == 0 ? 0 : enc_index == 1 ? 0x600 : 0x800;

	config_tv_enc_calc(vic);
} /* set_tv_encp_new */

void hdmitx21_venc_en(bool en)
{
	hd21_write_reg(ENCP_VIDEO_EN, en);
	hd21_write_reg(VPU_VENC_CTRL, en);
}
