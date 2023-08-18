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
/* note, venc setting will be override on dsc encoder side
 * so this function is optional when dsc_en = 1
 */
static void config_tv_enc_calc(struct hdmitx_dev *hdev, enum hdmi_vic vic)
{
	const struct hdmi_timing *tp = NULL;
	struct hdmi_timing timing = {0};
	/* adjust to align upsample and video enable */
	u32 hsync_st = 5; // hsync start pixel count
	u32 vsync_st = 1; // vsync start line count
	// Latency in pixel clock from ENCP_VFIFO2VD request to data ready to HDMI
	const u32 vfifo2vd_to_hdmi_latency = 2;
	u32 de_h_begin = 0;
	u32 de_h_end = 0;
	u32 de_v_begin = 0;
	u32 de_v_end = 0;
	bool y420_mode = 0;
	int hpara_div = 1;

	if (!hdev || !hdev->para)
		return;

	if (hdev->para->cs == HDMI_COLORSPACE_YUV420)
		y420_mode = 1;
	tp = hdmitx21_gettiming_from_vic(vic);
	if (!tp) {
		pr_info("not find hdmitx vic %d timing\n", vic);
		return;
	}
	pr_info("%s[%d] vic = %d\n", __func__, __LINE__, tp->vic);

	timing = *tp;
	tp = &timing;

	/* the FRL works at dual mode, so the horizon parameters will reduce to half */
	if (hdev->frl_rate && y420_mode == 1)
		hpara_div = 4;
	if (hdev->frl_rate && y420_mode == 0)
		hpara_div = 2;
	/* force 4 slices input to dsc encoder when dsc enabled */
	if (hdev->dsc_en)
		hpara_div = 4;
	timing.h_total /= hpara_div;
	timing.h_blank /= hpara_div;
	timing.h_front /= hpara_div;
	timing.h_sync /= hpara_div;
	timing.h_back /= hpara_div;
	timing.h_active /= hpara_div;

	/* it will flash screen under 8k50/60hz if with this sync_st */
	/* if (hdev->dsc_en) { */
	/* hsync_st = tp->h_front - 1; */
	/* vsync_st = tp->v_front - 1; */
	/* } */

	de_h_end = tp->h_total - (tp->h_front - hsync_st);
	de_h_begin = de_h_end - tp->h_active;
	de_v_end = tp->v_total - (tp->v_front - vsync_st);
	de_v_begin = de_v_end - tp->v_active;

	// set DVI/HDMI transfer timing
	// generate hsync
	hd21_write_reg(ENCP_DVI_HSO_BEGIN, hsync_st);
	hd21_write_reg(ENCP_DVI_HSO_END, hsync_st + tp->h_sync);

	// generate vsync
	hd21_write_reg(ENCP_DVI_VSO_BLINE_EVN, vsync_st + y420_mode);
	hd21_write_reg(ENCP_DVI_VSO_ELINE_EVN,
		vsync_st + tp->v_sync + y420_mode);
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
}

static unsigned long modulo(unsigned long a, unsigned long b)
{
	if (a >= b)
		return a - b;
	else
		return a;
}

static signed int to_signed(unsigned int a)
{
	if (a <= 7)
		return a;
	else
		return a - 16;
}

#define MREG_END_MARKER 0xffff

static const struct reg_s tvregs_1080i60[] = {
	{ENCP_VIDEO_EN, 0},
	{ENCI_VIDEO_EN, 0},
	{VENC_DVI_SETTING, 0x2029},
	{ENCP_VIDEO_MAX_PXCNT, 2199},
	{ENCP_VIDEO_MAX_LNCNT, 1124},
	{ENCP_VIDEO_HSPULS_BEGIN, 88},
	{ENCP_VIDEO_HSPULS_END, 264},
	{ENCP_VIDEO_HSPULS_SWITCH, 88},
	{ENCP_VIDEO_HAVON_BEGIN, 192},
	{ENCP_VIDEO_HAVON_END, 2111},
	{ENCP_VIDEO_HSO_BEGIN, 0},
	{ENCP_VIDEO_HSO_END, 44},
	{ENCP_VIDEO_EQPULS_BEGIN, 2288},
	{ENCP_VIDEO_EQPULS_END, 2464},
	{ENCP_VIDEO_VSPULS_BEGIN, 440},
	{ENCP_VIDEO_VSPULS_END, 2200},
	{ENCP_VIDEO_VSPULS_BLINE, 0},
	{ENCP_VIDEO_VSPULS_ELINE, 4},
	{ENCP_VIDEO_EQPULS_BLINE, 0},
	{ENCP_VIDEO_EQPULS_ELINE, 4},
	{ENCP_VIDEO_VAVON_BLINE, 20},
	{ENCP_VIDEO_VAVON_ELINE, 559},
	{ENCP_VIDEO_VSO_BEGIN, 30},
	{ENCP_VIDEO_VSO_END, 50},
	{ENCP_VIDEO_VSO_BLINE, 0},
	{ENCP_VIDEO_VSO_ELINE, 5},
	{ENCP_VIDEO_YFP1_HTIME, 516},
	{ENCP_VIDEO_YFP2_HTIME, 4355},
	{VENC_VIDEO_PROG_MODE, 0x100},
	{ENCP_VIDEO_OFLD_VOAV_OFST, 0x11},
	{ENCP_VIDEO_MODE, 0x5ffc},
	{ENCP_VIDEO_MODE_ADV, 0x0018},
	{ENCP_VIDEO_SYNC_MODE, 0x207},
	{ENCI_VIDEO_EN, 0},
	{MREG_END_MARKER, 0},
};

static const struct reg_s tvregs_1080i50[] = {
	{ENCP_VIDEO_EN, 0},
	{ENCI_VIDEO_EN, 0},
	{VENC_DVI_SETTING, 0x202d},
	{ENCP_VIDEO_MAX_PXCNT, 2639},
	{ENCP_VIDEO_MAX_LNCNT, 1124},
	{ENCP_VIDEO_HSPULS_BEGIN, 88},
	{ENCP_VIDEO_HSPULS_END, 264},
	{ENCP_VIDEO_HSPULS_SWITCH, 88},
	{ENCP_VIDEO_HAVON_BEGIN, 192},
	{ENCP_VIDEO_HAVON_END, 2111},
	{ENCP_VIDEO_HSO_BEGIN, 0},
	{ENCP_VIDEO_HSO_END, 44},
	{ENCP_VIDEO_VSPULS_BEGIN, 440},
	{ENCP_VIDEO_VSPULS_END, 2200},
	{ENCP_VIDEO_VSPULS_BLINE, 0},
	{ENCP_VIDEO_VSPULS_ELINE, 4},
	{ENCP_VIDEO_VAVON_BLINE, 20},
	{ENCP_VIDEO_VAVON_ELINE, 559},
	{ENCP_VIDEO_VSO_BEGIN, 30},
	{ENCP_VIDEO_VSO_END, 50},
	{ENCP_VIDEO_VSO_BLINE, 0},
	{ENCP_VIDEO_VSO_ELINE, 5},
	{ENCP_VIDEO_YFP1_HTIME, 526},
	{ENCP_VIDEO_YFP2_HTIME, 4365},
	{VENC_VIDEO_PROG_MODE, 0x100},
	{ENCP_VIDEO_OFLD_VOAV_OFST, 0x11},
	{ENCP_VIDEO_MODE, 0x5ffc},
	{ENCP_VIDEO_MODE_ADV, 0x0018},
	{ENCP_VIDEO_SYNC_MODE, 0x7},
	{ENCI_VIDEO_EN, 0},
	{MREG_END_MARKER, 0},
};

static void set_vmode_enc_hw(const struct reg_s *s)
{
	if (!s)
		return;

	while (s->reg != MREG_END_MARKER) {
		hd21_write_reg(s->reg, s->val);
		s++;
	}
}

static void hdmi_tvenc1080i_set(enum hdmi_vic vic)
{
	unsigned long VFIFO2VD_TO_HDMI_LATENCY = 2;
	unsigned long TOTAL_PIXELS = 0, PIXEL_REPEAT_HDMI = 0,
		PIXEL_REPEAT_VENC = 0, ACTIVE_PIXELS = 0;
	unsigned int FRONT_PORCH = 88, HSYNC_PIXELS = 0, ACTIVE_LINES = 0,
		INTERLACE_MODE = 0, TOTAL_LINES = 0, SOF_LINES = 0,
		VSYNC_LINES = 0;
	unsigned int LINES_F0 = 0, LINES_F1 = 563, BACK_PORCH = 0,
		EOF_LINES = 2, TOTAL_FRAMES = 0;

	unsigned long total_pixels_venc = 0;
	unsigned long active_pixels_venc = 0;
	unsigned long front_porch_venc = 0;
	unsigned long hsync_pixels_venc = 0;

	unsigned long de_h_begin = 0, de_h_end = 0;
	unsigned long de_v_begin_even = 0, de_v_end_even = 0,
		de_v_begin_odd = 0, de_v_end_odd = 0;
	unsigned long hs_begin = 0, hs_end = 0;
	unsigned long vs_adjust = 0;
	unsigned long vs_bline_evn = 0, vs_eline_evn = 0,
		vs_bline_odd = 0, vs_eline_odd = 0;
	unsigned long vso_begin_evn = 0, vso_begin_odd = 0;
	const struct hdmi_timing *tp = NULL;

	tp = hdmitx21_gettiming_from_vic(vic);
	if (!tp) {
		pr_info("not find hdmitx vic %d timing\n", vic);
		return;
	}
	pr_info("%s[%d] vic = %d\n", __func__, __LINE__, tp->vic);

	switch (vic) {
	case HDMI_5_1920x1080i60_16x9:
	case HDMI_46_1920x1080i120_16x9:
		INTERLACE_MODE = 1;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (1920 * (1 + PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080 / (1 + INTERLACE_MODE));
		LINES_F0 = 562;
		LINES_F1 = 563;
		FRONT_PORCH = 88;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 2;
		VSYNC_LINES = 5;
		SOF_LINES = 15;
		TOTAL_FRAMES = 4;
		set_vmode_enc_hw(tvregs_1080i60);
		break;
	case HDMI_20_1920x1080i50_16x9:
	case HDMI_40_1920x1080i100_16x9:
		INTERLACE_MODE = 1;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (1920 * (1 + PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080 / (1 + INTERLACE_MODE));
		LINES_F0 = 562;
		LINES_F1 = 563;
		FRONT_PORCH = 528;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 2;
		VSYNC_LINES = 5;
		SOF_LINES = 15;
		TOTAL_FRAMES = 4;
		set_vmode_enc_hw(tvregs_1080i50);
		break;
	default:
		break;
	}
	TOTAL_PIXELS = FRONT_PORCH + HSYNC_PIXELS + BACK_PORCH + ACTIVE_PIXELS;
	TOTAL_LINES = LINES_F0 + LINES_F1 * INTERLACE_MODE;

	total_pixels_venc = (TOTAL_PIXELS / (1 + PIXEL_REPEAT_HDMI)) *
		(1 + PIXEL_REPEAT_VENC);
	active_pixels_venc = (ACTIVE_PIXELS / (1 + PIXEL_REPEAT_HDMI)) *
		(1 + PIXEL_REPEAT_VENC);
	front_porch_venc = (FRONT_PORCH / (1 + PIXEL_REPEAT_HDMI)) *
		(1 + PIXEL_REPEAT_VENC);
	hsync_pixels_venc =
	(HSYNC_PIXELS / (1 + PIXEL_REPEAT_HDMI)) * (1 + PIXEL_REPEAT_VENC);

	hd21_write_reg(ENCP_VIDEO_MODE, hd21_read_reg(ENCP_VIDEO_MODE)
		| (1 << 14));

	/* Program DE timing */
	de_h_begin = modulo(hd21_read_reg(ENCP_VIDEO_HAVON_BEGIN) +
		VFIFO2VD_TO_HDMI_LATENCY, total_pixels_venc);
	de_h_end = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd21_write_reg(ENCP_DE_H_BEGIN, de_h_begin);
	hd21_write_reg(ENCP_DE_H_END, de_h_end);
	/* Program DE timing for even field */
	de_v_begin_even = hd21_read_reg(ENCP_VIDEO_VAVON_BLINE);
	de_v_end_even = de_v_begin_even + ACTIVE_LINES;
	hd21_write_reg(ENCP_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd21_write_reg(ENCP_DE_V_END_EVEN, de_v_end_even);
	/* Program DE timing for odd field if needed */
	if (INTERLACE_MODE) {
		de_v_begin_odd =
		to_signed((hd21_read_reg(ENCP_VIDEO_OFLD_VOAV_OFST) & 0xf0)
			  >> 4) + de_v_begin_even + (TOTAL_LINES - 1) / 2;
		de_v_end_odd = de_v_begin_odd + ACTIVE_LINES;
		hd21_write_reg(ENCP_DE_V_BEGIN_ODD, de_v_begin_odd);/* 583 */
		hd21_write_reg(ENCP_DE_V_END_ODD, de_v_end_odd);  /* 1123 */
	}

	/* Program Hsync timing */
	if (de_h_end + front_porch_venc >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust = 0;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd21_write_reg(ENCP_DVI_HSO_BEGIN, hs_begin);
	hd21_write_reg(ENCP_DVI_HSO_END, hs_end);

	/* Program Vsync timing for even field */
	if (de_v_begin_even >= SOF_LINES + VSYNC_LINES + (1 - vs_adjust))
		vs_bline_evn = de_v_begin_even - SOF_LINES - VSYNC_LINES
			- (1 - vs_adjust);
	else
		vs_bline_evn = TOTAL_LINES + de_v_begin_even - SOF_LINES
			- VSYNC_LINES - (1 - vs_adjust);

	vs_eline_evn = modulo(vs_bline_evn + VSYNC_LINES, TOTAL_LINES);
	hd21_write_reg(ENCP_DVI_VSO_BLINE_EVN, vs_bline_evn);   /* 0 */
	hd21_write_reg(ENCP_DVI_VSO_ELINE_EVN, vs_eline_evn);   /* 5 */
	vso_begin_evn = hs_begin; /* 2 */
	hd21_write_reg(ENCP_DVI_VSO_BEGIN_EVN, vso_begin_evn);  /* 2 */
	hd21_write_reg(ENCP_DVI_VSO_END_EVN, vso_begin_evn);  /* 2 */
	/* Program Vsync timing for odd field if needed */
	if (INTERLACE_MODE) {
		vs_bline_odd = de_v_begin_odd - 1 - SOF_LINES - VSYNC_LINES;
		vs_eline_odd = de_v_begin_odd - 1 - SOF_LINES;
		vso_begin_odd = modulo(hs_begin + (total_pixels_venc >> 1),
					total_pixels_venc);
		hd21_write_reg(ENCP_DVI_VSO_BLINE_ODD, vs_bline_odd);
		hd21_write_reg(ENCP_DVI_VSO_ELINE_ODD, vs_eline_odd);
		hd21_write_reg(ENCP_DVI_VSO_BEGIN_ODD, vso_begin_odd);
		hd21_write_reg(ENCP_DVI_VSO_END_ODD, vso_begin_odd);
	}

	hd21_write_reg(VPU_HDMI_SETTING, (0 << 0) |
		(0 << 1) |
		(tp->h_pol << 2) |
		(tp->v_pol << 3) |
		(0 << 4) |
		(4 << 5) |
		(0 << 8) |
		(0 << 12)
	);
	hd21_set_reg_bits(VPU_HDMI_SETTING, 1, 1, 1);
}

void set_tv_encp_new(struct hdmitx_dev *hdev, u32 enc_index, enum hdmi_vic vic,
	u32 enable)
{
	u32 reg_offset;
	// VENC0A 0x1b
	// VENC1A 0x21
	// VENC2A 0x23
	reg_offset = enc_index == 0 ? 0 : enc_index == 1 ? 0x600 : 0x800;

	if (!hdev || !hdev->para)
		return;

	switch (vic) {
	case HDMI_5_1920x1080i60_16x9:
	case HDMI_46_1920x1080i120_16x9:
	case HDMI_20_1920x1080i50_16x9:
	case HDMI_40_1920x1080i100_16x9:
		hdmi_tvenc1080i_set(vic);
		break;
	default:
		config_tv_enc_calc(hdev, vic);
		break;
	}
	if (hdev->bist_lock)
		hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
	/* for dsc mode enable, vpp post 4 slice->4ppc to hdmi, no up_sample
	 * for frl mode enable & non_y420 mode, vpp post 4 slice into VENC
	 * with vpp horizontal size divide 4(7680 / 4), VENC get sample from
	 * vpp every two clk(upsample = 1) with 4ppc, and then split to 2ppc
	 * to hdmi module
	 */
	if (hdev->dsc_en)
		hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 0, 3);
	else if (hdev->frl_rate && hdev->para->cs != HDMI_COLORSPACE_YUV420)
		hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 1, 0, 3);
	else
		hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 0, 3);
} /* set_tv_encp_new */

static void config_tv_enci(enum hdmi_vic vic)
{
	switch (vic) {
	/* for 480i */
	case HDMI_6_720x480i60_4x3:
	case HDMI_7_720x480i60_16x9:
	case HDMI_10_2880x480i60_4x3:
	case HDMI_11_2880x480i60_16x9:
	case HDMI_50_720x480i120_4x3:
	case HDMI_51_720x480i120_16x9:
	case HDMI_58_720x480i240_4x3:
	case HDMI_59_720x480i240_16x9:
		hd21_write_reg(VENC_SYNC_ROUTE, 0); // Set sync route on vpins
		// Set hsync/vsync source from interlace vencoder
		hd21_write_reg(VENC_VIDEO_PROG_MODE, 0xf0);
		hd21_write_reg(ENCI_VIDEO_MODE, 0x8); /* Set for 480I mode */
		hd21_write_reg(ENCI_VIDEO_MODE_ADV, 0x26); /* Set for High Bandwidth for CBW&YBW */
		hd21_write_reg(ENCI_YC_DELAY, 0x22); // both Y and C delay 2 clock
		hd21_write_reg(ENCI_VFIFO2VD_PIXEL_START, 233);
		hd21_write_reg(ENCI_VFIFO2VD_PIXEL_END, 233 + 720 * 2);
		hd21_write_reg(ENCI_VFIFO2VD_LINE_TOP_START, 17);
		hd21_write_reg(ENCI_VFIFO2VD_LINE_TOP_END, 17 + 240);
		hd21_write_reg(ENCI_VFIFO2VD_LINE_BOT_START, 18);
		hd21_write_reg(ENCI_VFIFO2VD_LINE_BOT_END, 18 + 240);
		hd21_write_reg(ENCI_VFIFO2VD_CTL, (0x4e << 8) | 1);     // enable vfifo2vd
		hd21_write_reg(ENCI_DBG_FLDLN_RST, 0x0f05);
		/* adujst timming for s5 480i */
		hd21_write_reg(ENCI_SYNC_VSO_EVNLN, 0x0003);
		hd21_write_reg(ENCI_SYNC_VSO_ODDLN, 0x0003);
		hd21_write_reg(ENCI_SYNC_HSO_BEGIN, 11 - 2);
		hd21_write_reg(ENCI_SYNC_HSO_END, 31 - 2);
		hd21_write_reg(ENCI_DBG_FLDLN_RST, 0xcf05);
		// delay fldln rst to make sure it synced by clk27
		hd21_read_reg(ENCI_DBG_FLDLN_RST);
		hd21_read_reg(ENCI_DBG_FLDLN_RST);
		hd21_read_reg(ENCI_DBG_FLDLN_RST);
		hd21_read_reg(ENCI_DBG_FLDLN_RST);
		hd21_write_reg(ENCI_DBG_FLDLN_RST, 0x0f05);
		break;
	/* for 576i */
	default:
		hd21_write_reg(ENCI_CFILT_CTRL, 0x0800);
		hd21_write_reg(ENCI_CFILT_CTRL2, 0x0010);
		// adjust the hsync start point and end point
		hd21_write_reg(ENCI_SYNC_HSO_BEGIN, 1);
		hd21_write_reg(ENCI_SYNC_HSO_END, 127);
		// adjust the vsync start line and end line
		hd21_write_reg(ENCI_SYNC_VSO_EVNLN, (0 << 8) | 3);
		hd21_write_reg(ENCI_SYNC_VSO_ODDLN, (0 << 8) | 3);
		// adjust for interlace output, Horizontal offset after HSI in slave mode
		hd21_write_reg(ENCI_SYNC_HOFFST, 0x16);
		hd21_write_reg(ENCI_MACV_MAX_AMP, 0x8107); // ENCI_MACV_MAX_AMP
		hd21_write_reg(VENC_VIDEO_PROG_MODE, 0xff); /* Set for interlace mode */
		hd21_write_reg(ENCI_VIDEO_MODE, 0x13); /* Set for PAL mode */
		hd21_write_reg(ENCI_VIDEO_MODE_ADV, 0x26); /* Set for High Bandwidth for CBW&YBW */
		hd21_write_reg(ENCI_VIDEO_SCH, 0x28); /* Set SCH */
		hd21_write_reg(ENCI_SYNC_MODE, 0x07); /* Set for master mode */
		/* Set hs/vs out timing */
		hd21_write_reg(ENCI_YC_DELAY, 0x341);
		hd21_write_reg(ENCI_VFIFO2VD_PIXEL_START, 267);
		hd21_write_reg(ENCI_VFIFO2VD_PIXEL_END, 267 + 1440);
		hd21_write_reg(ENCI_VFIFO2VD_LINE_TOP_START, 21);
		hd21_write_reg(ENCI_VFIFO2VD_LINE_TOP_END, 21 + 288);
		hd21_write_reg(ENCI_VFIFO2VD_LINE_BOT_START, 22);
		hd21_write_reg(ENCI_VFIFO2VD_LINE_BOT_END, 22 + 288);
		hd21_write_reg(ENCI_VFIFO2VD_CTL, (0x4e << 8) | 1); // enable vfifo2vd
		hd21_write_reg(ENCI_DBG_PX_RST, 0x0); /* Enable TV encoder */
		break;
	}
}

static void adjust_enci_for_hdmi(u8 enc_index,
	u8 interlace_mode,
	u32 total_pixels_venc,
	u32 active_pixels_venc,
	u32 front_porch_venc,
	u32 hsync_pixels_venc,
	u32 total_lines,
	u32 active_lines,
	u32 lines_f0,
	u32 lines_f1,
	u32 eof_lines,
	u32 sof_lines,
	u32 vsync_lines,
	u32 vs_adjust_420)
{
	u32 reg_offset;
	// Latency in pixel clock from ENCI_VFIFO2VD request to data ready to HDMI
	u32 vfifo2vd_to_hdmi_latency = 1;
	u32 de_h_begin, de_h_end;
	u32 de_v_begin_even, de_v_end_even, de_v_begin_odd, de_v_end_odd;
	u32 hs_begin, hs_end;
	u32 vs_adjust;
	u32 vs_bline_evn, vs_eline_evn, vs_bline_odd, vs_eline_odd;
	u32 vso_begin_evn, vso_begin_odd;
	u32 vso_bline_evn_reg_wr_cnt = 0, vso_bline_odd_reg_wr_cnt = 0;
	u32 vso_eline_evn_reg_wr_cnt = 0, vso_eline_odd_reg_wr_cnt = 0;

	reg_offset = (enc_index == 0) ? 0 : (enc_index == 1) ? 0x600 : 0x800;

	// Program DE timing
	de_h_begin = modulo(hd21_read_reg(ENCI_VFIFO2VD_PIXEL_START) +
		vfifo2vd_to_hdmi_latency, total_pixels_venc);
	de_h_end = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd21_write_reg(ENCI_DE_H_BEGIN, de_h_begin);
	hd21_write_reg(ENCI_DE_H_END, de_h_end);

	de_v_begin_even = hd21_read_reg(ENCI_VFIFO2VD_LINE_TOP_START);
	de_v_end_even = de_v_begin_even + active_lines;
	de_v_begin_odd = hd21_read_reg(ENCI_VFIFO2VD_LINE_BOT_START);
	de_v_end_odd = de_v_begin_odd + active_lines;
	hd21_write_reg(ENCI_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd21_write_reg(ENCI_DE_V_END_EVEN, de_v_end_even);
	hd21_write_reg(ENCI_DE_V_BEGIN_ODD, de_v_begin_odd);
	hd21_write_reg(ENCI_DE_V_END_ODD, de_v_end_odd);

	// Program Hsync timing
	if (de_h_end + front_porch_venc >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust = 0;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd21_write_reg(ENCI_DVI_HSO_BEGIN, hs_begin);
	hd21_write_reg(ENCI_DVI_HSO_END, hs_end);

	// Program Vsync timing for even field
	if (de_v_end_odd - 1 + eof_lines + vs_adjust + vs_adjust_420 >= lines_f1) {
		vs_bline_evn = de_v_end_odd - 1 + eof_lines + vs_adjust + vs_adjust_420 - lines_f1;
		vs_eline_evn = vs_bline_evn + vsync_lines;
		hd21_write_reg(ENCI_DVI_VSO_BLINE_EVN, vs_bline_evn);
		vso_bline_evn_reg_wr_cnt++;
		hd21_write_reg(ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);
		vso_eline_evn_reg_wr_cnt++;
		hd21_write_reg(ENCI_DVI_VSO_BEGIN_EVN, hs_begin);
		hd21_write_reg(ENCI_DVI_VSO_END_EVN,   hs_begin);
	} else {
		vs_bline_odd = de_v_end_odd - 1 + eof_lines + vs_adjust + vs_adjust_420;
		hd21_write_reg(ENCI_DVI_VSO_BLINE_ODD, vs_bline_odd);
		vso_bline_odd_reg_wr_cnt++;
		hd21_write_reg(ENCI_DVI_VSO_BEGIN_ODD, hs_begin);
		if (vs_bline_odd + vsync_lines >= lines_f1) {
			vs_eline_evn = vs_bline_odd + vsync_lines - lines_f1;
			hd21_write_reg(ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);
			vso_eline_evn_reg_wr_cnt++;
			hd21_write_reg(ENCI_DVI_VSO_END_EVN, hs_begin);
		} else {
			vs_eline_odd = vs_bline_odd + vsync_lines;
			hd21_write_reg(ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);
			vso_eline_odd_reg_wr_cnt++;
			hd21_write_reg(ENCI_DVI_VSO_END_ODD,   hs_begin);
		}
	}
	// Program Vsync timing for odd field
	if (de_v_end_even - 1 + eof_lines + 1 + vs_adjust_420 >= lines_f0) {
		vs_bline_odd = de_v_end_even - 1 + eof_lines + 1 + vs_adjust_420 - lines_f0;
		vs_eline_odd = vs_bline_odd + vsync_lines;
		hd21_write_reg(ENCI_DVI_VSO_BLINE_ODD, vs_bline_odd);
		vso_bline_odd_reg_wr_cnt++;
		hd21_write_reg(ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);
		vso_eline_odd_reg_wr_cnt++;
		vso_begin_odd   = modulo(hs_begin + (total_pixels_venc >> 1), total_pixels_venc);
		hd21_write_reg(ENCI_DVI_VSO_BEGIN_ODD, vso_begin_odd);
		hd21_write_reg(ENCI_DVI_VSO_END_ODD,   vso_begin_odd);
	} else {
		vs_bline_evn = de_v_end_even - 1 + eof_lines + 1 + vs_adjust_420;
		hd21_write_reg(ENCI_DVI_VSO_BLINE_EVN, vs_bline_evn);
		vso_bline_evn_reg_wr_cnt++;
		vso_begin_evn   = modulo(hs_begin + (total_pixels_venc >> 1), total_pixels_venc);
		hd21_write_reg(ENCI_DVI_VSO_BEGIN_EVN, vso_begin_evn);
		if (vs_bline_evn + vsync_lines >= lines_f0) {
			vs_eline_odd = vs_bline_evn + vsync_lines - lines_f0;
			hd21_write_reg(ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);
			vso_eline_odd_reg_wr_cnt++;
			hd21_write_reg(ENCI_DVI_VSO_END_ODD, vso_begin_evn);
		} else {
			vs_eline_evn = vs_bline_evn + vsync_lines;
			hd21_write_reg(ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);
			vso_eline_evn_reg_wr_cnt++;
			hd21_write_reg(ENCI_DVI_VSO_END_EVN, vso_begin_evn);
		}
	}

	// Check if there are duplicate or missing timing settings
	if (vso_bline_evn_reg_wr_cnt != 1 || vso_bline_odd_reg_wr_cnt != 1 ||
		vso_eline_evn_reg_wr_cnt != 1 || vso_eline_odd_reg_wr_cnt != 1)
		pr_info("Multiple or missing timing settings on reg ENCI_DVI_VSO_B(E)LINE_EVN(ODD)!\n");
}

void set_tv_enci_new(struct hdmitx_dev *hdev, u32 enc_index, enum hdmi_vic vic,
	u32 enable)
{
	u32 reg_offset;
	u8 INTERLACE_MODE = 1;
	u8 PIXEL_REPEAT_VENC = 2 - 1; // Pixel repeat factor seen in VENC
	u8 PIXEL_REPEAT_HDMI = 2 - 1; // Pixel repeat factor seen by HDMI TX

	u32 ACTIVE_PIXELS;
	u32 ACTIVE_LINES;
	u32 LINES_F0;
	u32 LINES_F1;
	u32 FRONT_PORCH;
	u32 HSYNC_PIXELS;
	u32 BACK_PORCH;
	u32 EOF_LINES;
	u32 VSYNC_LINES;
	u32 SOF_LINES;
	u32 TOTAL_PIXELS;
	u32 TOTAL_LINES;
	/* u8 input_color_format = HDMI_COLORSPACE_YUV444; */

	u32 total_pixels_venc;
	u32 active_pixels_venc;
	u32 front_porch_venc;
	u32 hsync_pixels_venc;

	reg_offset = enc_index == 0 ? 0 : enc_index == 1 ? 0x600 : 0x800;

	config_tv_enci(vic);
	switch (vic) {
	/* for 480i */
	case HDMI_6_720x480i60_4x3:
	case HDMI_7_720x480i60_16x9:
	case HDMI_10_2880x480i60_4x3:
	case HDMI_11_2880x480i60_16x9:
	case HDMI_50_720x480i120_4x3:
	case HDMI_51_720x480i120_16x9:
	case HDMI_58_720x480i240_4x3:
	case HDMI_59_720x480i240_16x9:
		// Number of active pixels per line.
		ACTIVE_PIXELS = (720 * (1 + PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (480 / (1 + INTERLACE_MODE)); // Number of active lines per field.
		LINES_F0 = 262; // Number of lines in the even field.
		LINES_F1 = 263; // Number of lines in the odd field.

		FRONT_PORCH = 38; // Number of pixels from DE Low to HSYNC high.
		HSYNC_PIXELS = 124; // Number of pixels of HSYNC pulse.
		BACK_PORCH = 114; // Number of pixels from HSYNC low to DE high.
		EOF_LINES = 4; // HSYNC count between last line of active video and start of VSYNC
		// a.k.a. End of Field (EOF). In interlaced mode,
		// HSYNC count will be eof_lines at the end of even field
		// and eof_lines+1 at the end of odd field.
		VSYNC_LINES = 3; // HSYNC count of VSYNC assertion
		// In interlaced mode VSYNC will be in-phase with HSYNC in the even field and
		// out-of-phase with HSYNC in the odd field.
		SOF_LINES = 15; // HSYNC count between VSYNC de-assertion
		// and first line of active video

		// Number of total pixels per line.
		TOTAL_PIXELS = (FRONT_PORCH + HSYNC_PIXELS + BACK_PORCH + ACTIVE_PIXELS);
		TOTAL_LINES = (LINES_F0 + (LINES_F1 * INTERLACE_MODE));
		total_pixels_venc =
			(TOTAL_PIXELS / (1 + PIXEL_REPEAT_HDMI)) * (1 + PIXEL_REPEAT_VENC);
		active_pixels_venc =
			(ACTIVE_PIXELS / (1 + PIXEL_REPEAT_HDMI)) * (1 + PIXEL_REPEAT_VENC);
		front_porch_venc  =
			(FRONT_PORCH / (1 + PIXEL_REPEAT_HDMI)) * (1 + PIXEL_REPEAT_VENC);
		hsync_pixels_venc =
			(HSYNC_PIXELS / (1 + PIXEL_REPEAT_HDMI)) * (1 + PIXEL_REPEAT_VENC);
		break;
	/* for 576i */
	default:
		// Number of active pixels per line.
		ACTIVE_PIXELS = (720 * (1 + PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (576 / (1 + INTERLACE_MODE));    // Number of active lines per field.
		LINES_F0 = 312; // Number of lines in the even field.
		LINES_F1 = 313; // Number of lines in the odd field.

		FRONT_PORCH = 24; // Number of pixels from DE Low to HSYNC high.
		HSYNC_PIXELS = 126; // Number of pixels of HSYNC pulse.
		BACK_PORCH = 138; // Number of pixels from HSYNC low to DE high.
		EOF_LINES = 2; // HSYNC count between last line of active video and start of VSYNC
		// a.k.a. End of Field (EOF). In interlaced mode,
		// HSYNC count will be eof_lines at the end of even field
		// and eof_lines+1 at the end of odd field.
		VSYNC_LINES = 3; // HSYNC count of VSYNC assertion
		// In interlaced mode VSYNC will be in-phase with HSYNC in the even field and
		// out-of-phase with HSYNC in the odd field.
		SOF_LINES = 19; // HSYNC count between VSYNC de-assertion
		// and first line of active video

		// Number of total pixels per line.
		TOTAL_PIXELS = (FRONT_PORCH + HSYNC_PIXELS + BACK_PORCH + ACTIVE_PIXELS);
		TOTAL_LINES = (LINES_F0 + (LINES_F1 * INTERLACE_MODE));
		total_pixels_venc =
			(TOTAL_PIXELS / (1 + PIXEL_REPEAT_HDMI)) * (1 + PIXEL_REPEAT_VENC);
		active_pixels_venc =
			(ACTIVE_PIXELS / (1 + PIXEL_REPEAT_HDMI)) * (1 + PIXEL_REPEAT_VENC);
		front_porch_venc  =
			(FRONT_PORCH / (1 + PIXEL_REPEAT_HDMI)) * (1 + PIXEL_REPEAT_VENC);
		hsync_pixels_venc =
			(HSYNC_PIXELS / (1 + PIXEL_REPEAT_HDMI)) * (1 + PIXEL_REPEAT_VENC);
		break;
	}
	adjust_enci_for_hdmi(enc_index,
		INTERLACE_MODE,
		total_pixels_venc,
		active_pixels_venc,
		front_porch_venc,
		hsync_pixels_venc,
		TOTAL_LINES,
		ACTIVE_LINES,
		LINES_F0,
		LINES_F1,
		EOF_LINES,
		SOF_LINES,
		VSYNC_LINES,
		// Due to 444->420 line buffer latency, the active
		// line output from 444->420 conversion will be delayed by 1 line.
		// So for 420 mode, we need to delay Vsync by 1 line as well,
		//to meet the VESA display timing spec.
		0);
} /* set_tv_enci_new */

void hdmitx21_venc_en(bool en, bool pi_mode)
{
	if (en == 0) {
		if (pi_mode == 1)
			hd21_write_reg(ENCP_VIDEO_EN, en);
		else
			hd21_write_reg(ENCI_VIDEO_EN, en);
		return;
	}
	if (pi_mode == 1) {
		hd21_write_reg(ENCP_VIDEO_EN, 1);
		hd21_write_reg(ENCI_VIDEO_EN, 0);
	} else {
		hd21_write_reg(ENCP_VIDEO_EN, 0);
		hd21_write_reg(ENCI_VIDEO_EN, 1);
	}
}
