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

// Use this self-made function rather than %, because % appears to produce wrong
// value for divisor which are not 2's exponential.
static u32 modulo(u32 a, u32 b)
{
	if (a >= b)
		return a - b;
	else
		return a;
}

static int32_t to_signed4b(u32 a)
{
	if (a <= 7)
		return a;
	else
		return a - 16;
}

/* ENCP_VIDEO_H/V are used for fetching video data from VPP
 * and send data to ENCP_DVI/DE_H/V* via a fifo (pixel delay)
 * The input timing of HDMITx is from ENCP_DVI/DE_H/V*
 */
static void config_tv_enc_calc(enum hdmi_vic vic)
{
	struct hdmi_timing *tp = NULL;
	struct hdmi_timing timing = {0};

	tp = hdmitx21_gettiming(vic);
	if (!tp) {
		pr_info("not find hdmitx vic %d timing\n", vic);
		return;
	}
	pr_info("%s[%d]\n", __func__, __LINE__);

	timing = *tp;
	tp = &timing;

	hd21_write_reg(ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
	hd21_write_reg(ENCP_VIDEO_MODE_ADV, 0x0018); // Sampling rate: 1
	hd21_write_reg(ENCP_VIDEO_YFP1_HTIME, 140);
	hd21_write_reg(ENCP_VIDEO_YFP2_HTIME, 2060);

	hd21_write_reg(ENCP_VIDEO_MAX_PXCNT, tp->h_total - 1);

	hd21_write_reg(ENCP_VIDEO_HAVON_BEGIN, tp->h_sync + tp->h_back);
	hd21_write_reg(ENCP_VIDEO_HAVON_END,
			hd21_read_reg(ENCP_VIDEO_HAVON_BEGIN) + tp->h_active - 1);
	hd21_write_reg(ENCP_VIDEO_VAVON_BLINE, tp->v_sync + tp->v_back);
	hd21_write_reg(ENCP_VIDEO_VAVON_ELINE,
			hd21_read_reg(ENCP_VIDEO_VAVON_BLINE) + tp->v_active - 1);

	hd21_write_reg(ENCP_VIDEO_HSO_BEGIN, 0);
	hd21_write_reg(ENCP_VIDEO_HSO_END, tp->h_sync);
	hd21_write_reg(ENCP_VIDEO_VSO_BEGIN, 30);
	hd21_write_reg(ENCP_VIDEO_VSO_END, 50);

	hd21_write_reg(ENCP_VIDEO_VSO_BLINE, 0);
	hd21_write_reg(ENCP_VIDEO_VSO_ELINE, tp->v_sync);
	hd21_write_reg(ENCP_VIDEO_MAX_LNCNT, tp->v_total - 1);
}

/*  */
static void set_encp_to_hdmi(int enc_index)
{
	u32 reg_offset;

	reg_offset = enc_index == 0 ? 0 : enc_index == 1 ? 0x600 : 0x800;

	// Program DE timing
	hd21_write_reg((reg_offset << 2) + ENCP_DE_H_BEGIN,
			hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_HAVON_BEGIN) + 2);
	hd21_write_reg((reg_offset << 2) + ENCP_DE_H_END,
			hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_HAVON_END) + 3);
	// Program DE timing for even field
	hd21_write_reg((reg_offset << 2) + ENCP_DE_V_BEGIN_EVEN,
			hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_VAVON_BLINE));
	hd21_write_reg((reg_offset << 2) + ENCP_DE_V_END_EVEN,
			hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_VAVON_ELINE) + 1);

	hd21_write_reg((reg_offset << 2) + ENCP_DVI_HSO_BEGIN,
			hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_HSO_BEGIN) + 2);
	hd21_write_reg((reg_offset << 2) + ENCP_DVI_HSO_END,
			hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_HSO_END) + 2);

	hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_BEGIN_EVN,
			hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_HSO_BEGIN) + 2);
	hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_END_EVN,
			hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_HSO_END) + 2);
	hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_BLINE_EVN,
			hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_VSO_BLINE));
	hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_ELINE_EVN,
			hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_VSO_ELINE));
} // set_encp_to_hdmi

void adjust_encp_for_hdmi(u8 enc_index, struct hdmi_timing *timing, u32 vs_adjust_420)
{
	u32 reg_offset;
	// Latency in pixel clock from ENCP_VFIFO2VD request to data ready to HDMI
	u32 vfifo2vd_to_hdmi_latency = 2;
	u32 de_h_begin, de_h_end;
	u32 de_v_begin_even, de_v_end_even, de_v_begin_odd, de_v_end_odd;
	u32 hs_begin, hs_end;
	u32 vs_adjust;
	u32 vs_bline_evn, vs_eline_evn, vs_bline_odd, vs_eline_odd;
	u32 vso_begin_evn, vso_begin_odd;
	u8 interlace_mode = !timing->pi_mode;
	u32 total_pixels_venc = timing->h_total;
	u32 active_pixels_venc = timing->h_active;
	u32 front_porch_venc = timing->h_front;
	u32 hsync_pixels_venc = timing->h_sync;
	u32 total_lines = timing->v_total;
	u32 active_lines = timing->v_active;
	u32 sof_lines = timing->v_front;
	u32 vsync_lines = timing->v_sync;

	reg_offset = (enc_index == 0) ? 0 : (enc_index == 1) ? 0x600 : 0x800;

	// Program DE timing
	de_h_begin = modulo(hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_HAVON_BEGIN) +
			vfifo2vd_to_hdmi_latency, total_pixels_venc);
	de_h_end = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd21_write_reg((reg_offset << 2) + ENCP_DE_H_BEGIN, de_h_begin);
	hd21_write_reg((reg_offset << 2) + ENCP_DE_H_END, de_h_end);
	// Program DE timing for even field
	de_v_begin_even = hd21_read_reg((reg_offset << 2) + ENCP_VIDEO_VAVON_BLINE);
	de_v_end_even = modulo(de_v_begin_even + active_lines, total_lines);
	hd21_write_reg((reg_offset << 2) + ENCP_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd21_write_reg((reg_offset << 2) + ENCP_DE_V_END_EVEN, de_v_end_even);
	// Program DE timing for odd field if needed
	if (interlace_mode) {
		// Calculate de_v_begin_odd according to enc480p_timing.v
		//wire[10:0] cfg_ofld_vavon_bline =
		//		{{7{ofld_vavon_ofst1 [3]}}, ofld_vavon_ofst1 [3:0]} +
		//		cfg_video_vavon_bline	+ ofld_line;
		de_v_begin_odd = to_signed4b((hd21_read_reg((reg_offset << 2) +
					ENCP_VIDEO_OFLD_VOAV_OFST) & 0xf0) >> 4) +
					de_v_begin_even + (total_lines - 1) / 2;
		de_v_end_odd = modulo(de_v_begin_odd + active_lines, total_lines);
		hd21_write_reg((reg_offset << 2) + ENCP_DE_V_BEGIN_ODD, de_v_begin_odd);
		hd21_write_reg((reg_offset << 2) + ENCP_DE_V_END_ODD, de_v_end_odd);
	}

	// Program Hsync timing
	if ((de_h_end + front_porch_venc) >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust = 0;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd21_write_reg((reg_offset << 2) + ENCP_DVI_HSO_BEGIN, hs_begin);
	hd21_write_reg((reg_offset << 2) + ENCP_DVI_HSO_END, hs_end);

	// Program Vsync timing for even field
	if ((de_v_begin_even + vs_adjust_420) >= (sof_lines + vsync_lines + (1 - vs_adjust))) {
		vs_bline_evn = de_v_begin_even + vs_adjust_420 - sof_lines -
				vsync_lines - (1 - vs_adjust);
	} else {
		vs_bline_evn = total_lines + de_v_begin_even + vs_adjust_420 -
				sof_lines - vsync_lines - (1 - vs_adjust);
	}
	vs_eline_evn = modulo(vs_bline_evn + vsync_lines, total_lines);
	hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_BLINE_EVN, vs_bline_evn);
	hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_ELINE_EVN, vs_eline_evn);
	vso_begin_evn = hs_begin;
	hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_BEGIN_EVN, vso_begin_evn);
	hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_END_EVN, vso_begin_evn);
	// Program Vsync timing for odd field if needed
	if (interlace_mode) {
		vs_bline_odd = vs_adjust_420 + de_v_begin_odd - 1 - sof_lines - vsync_lines;
		vs_eline_odd = vs_adjust_420 + de_v_begin_odd - 1 - sof_lines;
		vso_begin_odd = modulo(hs_begin + (total_pixels_venc >> 1), total_pixels_venc);
		hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_BLINE_ODD, vs_bline_odd);
		hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_ELINE_ODD, vs_eline_odd);
		hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_BEGIN_ODD, vso_begin_odd);
		hd21_write_reg((reg_offset << 2) + ENCP_DVI_VSO_END_ODD, vso_begin_odd);
	}
}   // adjust_encp_for_hdmi

void set_tv_encp_new(u32 enc_index, enum hdmi_vic vic, u32 enable)
{
	u32 reg_offset;
	// VENC0A 0x1b
	// VENC1A 0x21
	// VENC2A 0x23
	reg_offset = enc_index == 0 ? 0 : enc_index == 1 ? 0x600 : 0x800;

	config_tv_enc_calc(vic);

	set_encp_to_hdmi(enc_index);

} /* set_tv_encp_new */

void hdmitx21_venc_en(bool en)
{
	hd21_write_reg(ENCP_VIDEO_EN, en);
	hd21_write_reg(VPU_VENC_CTRL, en);
}
