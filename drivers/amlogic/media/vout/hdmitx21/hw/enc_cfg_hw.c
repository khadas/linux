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

static void config_tv_enc_new(u32 enc_index, enum tvenc_type output_type)
{
	u32 T_ENCI_VFIFO2VD_PIXEL_START;
	u32 T_ENCI_VFIFO2VD_PIXEL_END;
	u32 T_ENCI_VFIFO2VD_LINE_TOP_START;
	u32 T_ENCI_VFIFO2VD_LINE_TOP_END;
	u32 T_ENCI_VFIFO2VD_LINE_BOT_START;
	u32 T_ENCI_VFIFO2VD_LINE_BOT_END;
	u32 T_ENCI_VFIFO2VD_CTL;     // enable vfifo2vd
	u32 T_ENCI_DBG_FLDLN_RST;
	u32 T_ENCI_SYNC_VSO_EVNLN;
	u32 T_ENCI_SYNC_VSO_ODDLN;
	u32 T_ENCI_SYNC_HSO_BEGIN;
	u32 T_ENCI_SYNC_HSO_END;
	u32 T_ENCP_DVI_VSO_BLINE_EVN;
	u32 T_ENCP_DVI_VSO_ELINE_EVN;
	u32 T_ENCP_DVI_VSO_BEGIN_EVN;
	u32 T_ENCP_DVI_VSO_END_EVN;
	u32 T_ENCP_DVI_HSO_BEGIN;
	u32 T_ENCP_DVI_HSO_END;
	u32 T_ENCP_DE_H_BEGIN;
	u32 T_ENCP_DE_H_END;
	u32 T_ENCP_DE_V_BEGIN_EVEN;
	u32 T_ENCP_DE_V_END_EVEN;
	u32 T_ENCP_VIDEO_MODE; // cfg_de_v = 1
	u32 T_VENC_DVI_SETTING; // sel_dvi = 1
	u32 T_ENCP_VIDEO_MODE_ADV;       // Use FIFO interface
	u32 T_ENCP_VIDEO_VAVON_BLINE;
	u32 T_ENCP_VIDEO_VAVON_ELINE;

	u32 T_ENCP_VIDEO_YFP1_HTIME;
	u32 T_ENCP_VIDEO_YFP2_HTIME;
	u32 T_ENCP_VIDEO_MAX_PXCNT;
	u32 T_ENCP_VIDEO_HSPULS_BEGIN;
	u32 T_ENCP_VIDEO_HSPULS_END;
	u32 T_ENCP_VIDEO_HSPULS_SWITCH;
	u32 T_ENCP_VIDEO_VSPULS_BEGIN;
	u32 T_ENCP_VIDEO_VSPULS_END;
	u32 T_ENCP_VIDEO_VSPULS_BLINE;
	u32 T_ENCP_VIDEO_VSPULS_ELINE;
	u32 T_ENCP_VIDEO_HAVON_BEGIN;
	u32 T_ENCP_VIDEO_HAVON_END;
	u32 T_ENCP_VIDEO_HSO_BEGIN;
	u32 T_ENCP_VIDEO_HSO_END;
	u32 T_ENCP_VIDEO_VSO_BEGIN;
	u32 T_ENCP_VIDEO_VSO_END;
	u32 T_ENCP_VIDEO_VSO_BLINE;
	u32 T_ENCP_VIDEO_VSO_ELINE;
	u32 T_ENCP_VIDEO_MAX_LNCNT;
	u32 T_ENCP_VIDEO_FILT_CTRL;
	u32 T_ENCP_VFIFO2VD_PIXEL_START;
	u32 T_ENCP_VFIFO2VD_PIXEL_END;
	u32 T_ENCP_VFIFO2VD_LINE_TOP_START;
	u32 T_ENCP_VFIFO2VD_LINE_TOP_END;
	u32 T_ENCP_VFIFO2VD_LINE_BOT_START;
	u32 T_ENCP_VFIFO2VD_LINE_BOT_END;
	u32 T_ENCP_VFIFO2VD_CTL;
	u32 T_ENCP_MACV_TS_CNT_MAX_L;
	u32 T_ENCP_MACV_TS_CNT_MAX_H;
	u32 T_ENCP_MACV_TIME_DOWN;
	u32 T_ENCP_MACV_TIME_LO;
	u32 T_ENCP_MACV_TIME_UP;
	u32 T_ENCP_MACV_TIME_RST;
	u32 T_ENCP_MACV_MAXY_VAL;
	u32 T_ENCP_MACV_1ST_PSSYNC_STRT;
	u32 T_ENCI_SYNC_HOFFST;
	u32 T_ENCP_VIDEO_YC_DLY;
	u32 T_ENCP_MACV_STRTLINE;
	u32 T_ENCP_MACV_ENDLINE;
	u32 T_ENCP_VIDEO_SYNC_MODE;
	u32 T_ENCP_VIDEO_OFLD_VPEQ_OFST;
	u32 T_ENCP_VIDEO_OFLD_VOAV_OFST;
	u32 T_ENCI_CFILT_CTRL;
	u32 T_ENCI_CFILT_CTRL2;
	u32 T_ENCI_MACV_MAX_AMP;
	u32 T_ENCI_VIDEO_MODE;
	u32 T_ENCI_VIDEO_MODE_ADV;
	u32 T_ENCI_VIDEO_SCH;
	u32 T_ENCI_SYNC_MODE;
	u32 T_ENCI_DBG_PX_RST;

	u32 read_32;
	u32 reg_offset;

	reg_offset = enc_index == 0 ? 0 : enc_index == 1 ? 0x600 : 0x800;

	T_ENCI_VFIFO2VD_PIXEL_START = ENCI_VFIFO2VD_PIXEL_START + (reg_offset << 2);
	T_ENCI_VFIFO2VD_PIXEL_END = ENCI_VFIFO2VD_PIXEL_END + (reg_offset << 2);
	T_ENCI_VFIFO2VD_LINE_TOP_START = ENCI_VFIFO2VD_LINE_TOP_START + (reg_offset << 2);
	T_ENCI_VFIFO2VD_LINE_TOP_END = ENCI_VFIFO2VD_LINE_TOP_END + (reg_offset << 2);
	T_ENCI_VFIFO2VD_LINE_BOT_START = ENCI_VFIFO2VD_LINE_BOT_START + (reg_offset << 2);
	T_ENCI_VFIFO2VD_LINE_BOT_END = ENCI_VFIFO2VD_LINE_BOT_END + (reg_offset << 2);
	T_ENCI_VFIFO2VD_CTL = ENCI_VFIFO2VD_CTL + (reg_offset << 2); // enable vfifo2vd
	T_ENCI_DBG_FLDLN_RST = ENCI_DBG_FLDLN_RST + (reg_offset << 2);
	T_ENCI_SYNC_VSO_EVNLN = ENCI_SYNC_VSO_EVNLN + (reg_offset << 2);
	T_ENCI_SYNC_VSO_ODDLN = ENCI_SYNC_VSO_ODDLN + (reg_offset << 2);
	T_ENCI_SYNC_HSO_BEGIN = ENCI_SYNC_HSO_BEGIN + (reg_offset << 2);
	T_ENCI_SYNC_HSO_END = ENCI_SYNC_HSO_END + (reg_offset << 2);
	T_ENCI_DBG_FLDLN_RST = ENCI_DBG_FLDLN_RST + (reg_offset << 2);
	// delay fldln rst to make sure it synced by clk27
	T_ENCI_DBG_FLDLN_RST = ENCI_DBG_FLDLN_RST + (reg_offset << 2);
	T_ENCP_DVI_VSO_BLINE_EVN = ENCP_DVI_VSO_BLINE_EVN + (reg_offset << 2);
	T_ENCP_DVI_VSO_ELINE_EVN = ENCP_DVI_VSO_ELINE_EVN + (reg_offset << 2);
	T_ENCP_DVI_VSO_BEGIN_EVN = ENCP_DVI_VSO_BEGIN_EVN + (reg_offset << 2);
	T_ENCP_DVI_VSO_END_EVN = ENCP_DVI_VSO_END_EVN + (reg_offset << 2);
	T_ENCP_DVI_HSO_BEGIN = ENCP_DVI_HSO_BEGIN + (reg_offset << 2);
	T_ENCP_DVI_HSO_END = ENCP_DVI_HSO_END + (reg_offset << 2);
	T_ENCP_DE_H_BEGIN = ENCP_DE_H_BEGIN + (reg_offset << 2);
	T_ENCP_DE_H_END = ENCP_DE_H_END + (reg_offset << 2);
	T_ENCP_DE_V_BEGIN_EVEN = ENCP_DE_V_BEGIN_EVEN + (reg_offset << 2);
	T_ENCP_DE_V_END_EVEN = ENCP_DE_V_END_EVEN + (reg_offset << 2);
	T_ENCP_VIDEO_MODE = ENCP_VIDEO_MODE + (reg_offset << 2); // cfg_de_v = 1
	T_VENC_DVI_SETTING = VENC_DVI_SETTING + (reg_offset << 2); // sel_dvi = 1
	T_ENCP_VIDEO_MODE_ADV = ENCP_VIDEO_MODE_ADV + (reg_offset << 2); // Use FIFO interface
	T_ENCP_VIDEO_VAVON_BLINE = ENCP_VIDEO_VAVON_BLINE + (reg_offset << 2);
	T_ENCP_VIDEO_VAVON_ELINE = ENCP_VIDEO_VAVON_ELINE + (reg_offset << 2);

	T_ENCI_VFIFO2VD_CTL = ENCI_VFIFO2VD_CTL + (reg_offset << 2);
	T_ENCP_VIDEO_YFP1_HTIME = ENCP_VIDEO_YFP1_HTIME + (reg_offset << 2);
	T_ENCP_VIDEO_YFP2_HTIME = ENCP_VIDEO_YFP2_HTIME + (reg_offset << 2);
	T_ENCP_VIDEO_MAX_PXCNT = ENCP_VIDEO_MAX_PXCNT + (reg_offset << 2);
	T_ENCP_VIDEO_HSPULS_BEGIN = ENCP_VIDEO_HSPULS_BEGIN + (reg_offset << 2);
	T_ENCP_VIDEO_HSPULS_END = ENCP_VIDEO_HSPULS_END + (reg_offset << 2);
	T_ENCP_VIDEO_HSPULS_SWITCH = ENCP_VIDEO_HSPULS_SWITCH + (reg_offset << 2);
	T_ENCP_VIDEO_VSPULS_BEGIN = ENCP_VIDEO_VSPULS_BEGIN + (reg_offset << 2);
	T_ENCP_VIDEO_VSPULS_END = ENCP_VIDEO_VSPULS_END + (reg_offset << 2);
	T_ENCP_VIDEO_VSPULS_BLINE = ENCP_VIDEO_VSPULS_BLINE + (reg_offset << 2);
	T_ENCP_VIDEO_VSPULS_ELINE = ENCP_VIDEO_VSPULS_ELINE + (reg_offset << 2);
	T_ENCP_VIDEO_HAVON_BEGIN = ENCP_VIDEO_HAVON_BEGIN + (reg_offset << 2);
	T_ENCP_VIDEO_HAVON_END = ENCP_VIDEO_HAVON_END + (reg_offset << 2);
	T_ENCP_VIDEO_HSO_BEGIN = ENCP_VIDEO_HSO_BEGIN + (reg_offset << 2);
	T_ENCP_VIDEO_HSO_END = ENCP_VIDEO_HSO_END + (reg_offset << 2);
	T_ENCP_VIDEO_VSO_BEGIN = ENCP_VIDEO_VSO_BEGIN + (reg_offset << 2);
	T_ENCP_VIDEO_VSO_END = ENCP_VIDEO_VSO_END + (reg_offset << 2);
	T_ENCP_VIDEO_VSO_BLINE = ENCP_VIDEO_VSO_BLINE + (reg_offset << 2);
	T_ENCP_VIDEO_VSO_ELINE = ENCP_VIDEO_VSO_ELINE + (reg_offset << 2);
	T_ENCP_VIDEO_MAX_LNCNT = ENCP_VIDEO_MAX_LNCNT + (reg_offset << 2);
	T_ENCP_VIDEO_FILT_CTRL = ENCP_VIDEO_FILT_CTRL + (reg_offset << 2);
	T_ENCP_VFIFO2VD_PIXEL_START = ENCP_VFIFO2VD_PIXEL_START + (reg_offset << 2);
	T_ENCP_VFIFO2VD_PIXEL_END = ENCP_VFIFO2VD_PIXEL_END + (reg_offset << 2);
	T_ENCP_VFIFO2VD_LINE_TOP_START = ENCP_VFIFO2VD_LINE_TOP_START + (reg_offset << 2);
	T_ENCP_VFIFO2VD_LINE_TOP_END = ENCP_VFIFO2VD_LINE_TOP_END + (reg_offset << 2);
	T_ENCP_VFIFO2VD_LINE_BOT_START = ENCP_VFIFO2VD_LINE_BOT_START + (reg_offset << 2);
	T_ENCP_VFIFO2VD_LINE_BOT_END = ENCP_VFIFO2VD_LINE_BOT_END + (reg_offset << 2);
	T_ENCP_VFIFO2VD_CTL = ENCP_VFIFO2VD_CTL + (reg_offset << 2);
	T_ENCP_MACV_TS_CNT_MAX_L = ENCP_MACV_TS_CNT_MAX_L + (reg_offset << 2);
	T_ENCP_MACV_TS_CNT_MAX_H = ENCP_MACV_TS_CNT_MAX_H + (reg_offset << 2);
	T_ENCP_MACV_TIME_DOWN = ENCP_MACV_TIME_DOWN + (reg_offset << 2);
	T_ENCP_MACV_TIME_LO = ENCP_MACV_TIME_LO + (reg_offset << 2);
	T_ENCP_MACV_TIME_UP = ENCP_MACV_TIME_UP + (reg_offset << 2);
	T_ENCP_MACV_TIME_RST = ENCP_MACV_TIME_RST + (reg_offset << 2);
	T_ENCP_MACV_MAXY_VAL = ENCP_MACV_MAXY_VAL + (reg_offset << 2);
	T_ENCP_MACV_1ST_PSSYNC_STRT = ENCP_MACV_1ST_PSSYNC_STRT + (reg_offset << 2);
	T_ENCI_SYNC_HOFFST = ENCI_SYNC_HOFFST + (reg_offset << 2);
	T_ENCP_VIDEO_YC_DLY = ENCP_VIDEO_YC_DLY + (reg_offset << 2);
	T_ENCP_MACV_STRTLINE = ENCP_MACV_STRTLINE + (reg_offset << 2);
	T_ENCP_MACV_ENDLINE = ENCP_MACV_ENDLINE + (reg_offset << 2);
	T_ENCP_VIDEO_SYNC_MODE = ENCP_VIDEO_SYNC_MODE + (reg_offset << 2);
	T_ENCP_VIDEO_OFLD_VPEQ_OFST = ENCP_VIDEO_OFLD_VPEQ_OFST + (reg_offset << 2);
	T_ENCP_VIDEO_OFLD_VOAV_OFST = ENCP_VIDEO_OFLD_VOAV_OFST + (reg_offset << 2);
	T_ENCI_CFILT_CTRL = ENCI_CFILT_CTRL + (reg_offset << 2);
	T_ENCI_CFILT_CTRL2 = ENCI_CFILT_CTRL2 + (reg_offset << 2);
	T_ENCI_MACV_MAX_AMP = ENCI_MACV_MAX_AMP + (reg_offset << 2);
	T_ENCI_VIDEO_MODE = ENCI_VIDEO_MODE + (reg_offset << 2);
	T_ENCI_VIDEO_MODE_ADV = ENCI_VIDEO_MODE_ADV + (reg_offset << 2);
	T_ENCI_VIDEO_SCH = ENCI_VIDEO_SCH + (reg_offset << 2);
	T_ENCI_SYNC_MODE = ENCI_SYNC_MODE + (reg_offset << 2);
	T_ENCI_DBG_PX_RST = ENCI_DBG_PX_RST + (reg_offset << 2);

	switch (output_type) {
	case TV_ENC_480i:
		hd21_write_reg(VENC_SYNC_ROUTE, 0); // Set sync route on vpins
		// Set hsync/vsync source from interlace vencoder
		hd21_write_reg(VENC_VIDEO_PROG_MODE, 0xf0);

		hd21_write_reg(T_ENCI_VFIFO2VD_PIXEL_START, 233);
		hd21_write_reg(T_ENCI_VFIFO2VD_PIXEL_END, 233 + 720 * 2);
		hd21_write_reg(T_ENCI_VFIFO2VD_LINE_TOP_START, 17);
		hd21_write_reg(T_ENCI_VFIFO2VD_LINE_TOP_END, 17 + 240);
		hd21_write_reg(T_ENCI_VFIFO2VD_LINE_BOT_START, 18);
		hd21_write_reg(T_ENCI_VFIFO2VD_LINE_BOT_END, 18 + 240);
		hd21_write_reg(T_ENCI_VFIFO2VD_CTL, (0x4e << 8) | 1);     // enable vfifo2vd

		// Force line/field change (field =7, line = 261 )
		hd21_write_reg(T_ENCI_DBG_FLDLN_RST, 0x0f05);
		hd21_write_reg(T_ENCI_SYNC_VSO_EVNLN, 0x0508);
		hd21_write_reg(T_ENCI_SYNC_VSO_ODDLN, 0x0508);
		hd21_write_reg(T_ENCI_SYNC_HSO_BEGIN, 11 - 2);
		hd21_write_reg(T_ENCI_SYNC_HSO_END, 31 - 2);
		hd21_write_reg(T_ENCI_DBG_FLDLN_RST, 0xcf05);
		 // delay fldln rst to make sure it synced by clk27
		hd21_read_reg(T_ENCI_DBG_FLDLN_RST);
		hd21_read_reg(T_ENCI_DBG_FLDLN_RST);
		hd21_read_reg(T_ENCI_DBG_FLDLN_RST);
		hd21_read_reg(T_ENCI_DBG_FLDLN_RST);
		hd21_write_reg(T_ENCI_DBG_FLDLN_RST, 0x0f05);
		break;

	case TV_ENC_480p:
		// USE_DELAYED_VSYNC -- delay 3 line
		//       read_32 = hd21_write_reg(T_ENCP_VIDEO_SYNC_MODE);
		//       read_32 = (read_32 & 0x00ff) | (3<<8);
		//       hd21_write_reg(T_ENCP_VIDEO_SYNC_MODE, read_32);
		//       hd21_write_reg(VENC_DVI_SETTING, 0x2011);

		hd21_write_reg(T_ENCP_DVI_VSO_BLINE_EVN, 40);
		hd21_write_reg(T_ENCP_DVI_VSO_ELINE_EVN, 42);
		hd21_write_reg(T_ENCP_DVI_VSO_BEGIN_EVN, 16);
		hd21_write_reg(T_ENCP_DVI_VSO_END_EVN, 32);
		hd21_write_reg(T_ENCP_DVI_HSO_BEGIN, 16);
		hd21_write_reg(T_ENCP_DVI_HSO_END, 32);
		hd21_write_reg(T_ENCP_DE_H_BEGIN, 217);
		hd21_write_reg(T_ENCP_DE_H_END, 1657);
		hd21_write_reg(T_ENCP_DE_V_BEGIN_EVEN, 42);
		hd21_write_reg(T_ENCP_DE_V_END_EVEN, 522);
		hd21_write_reg(T_ENCP_VIDEO_MODE, 1 << 14); // cfg_de_v = 1
		hd21_write_reg(VENC_DVI_SETTING, 0x8011); // sel_dvi = 1

		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x09); // Use FIFO interface

		// SETVAVON
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 42);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 522);
		break;

	case TV_ENCP_480p:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 40);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 830);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 857);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 840);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 18);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 18);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 40);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 524);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 80);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 799);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 11);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 490);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 18);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 840);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 830);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 848);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 3);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 5);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 524);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter

		break;

	case TV_ENC_640x480p:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 40);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 772);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 799);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 782);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 18);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 18);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 40);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 524);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 80);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 719);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 37);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 516);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 18);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 782);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 772);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 790);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 3);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 5);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 524);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter

		break;

	case TV_ENC_576p:
		hd21_write_reg(VENC_VIDEO_PROG_MODE, 0x100); // select cfg_sel_prog

		hd21_write_reg(T_ENCP_VFIFO2VD_PIXEL_START, 245);
		hd21_write_reg(T_ENCP_VFIFO2VD_PIXEL_END, 245 + 720 * 2);
		hd21_write_reg(T_ENCP_VFIFO2VD_LINE_TOP_START, 44);
		hd21_write_reg(T_ENCP_VFIFO2VD_LINE_TOP_END, 44 + 576);
		hd21_write_reg(T_ENCP_VFIFO2VD_LINE_BOT_START, 44);
		hd21_write_reg(T_ENCP_VFIFO2VD_LINE_BOT_END, 44 + 576);
		hd21_write_reg(T_ENCP_VFIFO2VD_CTL, (0x4e << 8) | 1); // enable vfifo2vd

		hd21_write_reg(T_ENCP_VIDEO_MODE, 0);
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x01); // do not use VFIFO interface

		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 34);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 148);

		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 1599);
		//hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 624);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 5);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 248);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 1688);

		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 44);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 624);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 11);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 16);

		hd21_write_reg(T_ENCP_MACV_TS_CNT_MAX_L, 0x7dcd);
		hd21_write_reg(T_ENCP_MACV_TS_CNT_MAX_H, 0x3);
		hd21_write_reg(T_ENCP_MACV_TIME_DOWN, 3068);
		hd21_write_reg(T_ENCP_MACV_TIME_LO, 3658);
		hd21_write_reg(T_ENCP_MACV_TIME_UP, 4366);
		hd21_write_reg(T_ENCP_MACV_TIME_RST, 4956);
		hd21_write_reg(T_ENCP_MACV_MAXY_VAL, 590);
		hd21_write_reg(T_ENCP_MACV_1ST_PSSYNC_STRT, 237);
		hd21_write_reg(T_ENCI_SYNC_HOFFST, 0x12); // adjust for interlace output

		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 263);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 1646);
		hd21_write_reg(T_ENCP_VIDEO_YC_DLY, 0);
		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 1727);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 624);
		//hd21_write_reg(T_ENCP_MACV_STRTLINE, 4);
		hd21_write_reg(T_ENCP_MACV_STRTLINE, 5);
		//hd21_write_reg(T_ENCP_MACV_ENDLINE, 12);
		hd21_write_reg(T_ENCP_MACV_ENDLINE, 13);

		hd21_write_reg(T_ENCP_VIDEO_SYNC_MODE, 0x07); /* Set for master mode */

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 0x3);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 0x5);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 0x3);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 0x5);

		// USE_DELAYED_VSYNC -- delay 3 line
		read_32 = hd21_read_reg(T_ENCP_VIDEO_SYNC_MODE);
		read_32 = (read_32 & 0x00ff) | (3 << 8);
		hd21_write_reg(T_ENCP_VIDEO_SYNC_MODE, read_32);
		break;

	case TV_ENC_720p:
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0019);
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 520);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 3080);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 3299);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 3220);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 80);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 80);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 520);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 3079);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 5);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 519);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 3078);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 25);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 744);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 15);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 31);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 15);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 31);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 19);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 21);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 749);

		break;

	case TV_ENC_1080p:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 140);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 2060);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 2199);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 2156);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 44);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 140);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 2059);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 148);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 2067);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 45);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 1124);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 2156);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 2100);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 2164);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 3);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 5);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 1124);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter

		break;

	case TV_ENC_320p:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 140);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 652); //2060-(1920-512)

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 791);  //2199-(1920-512)
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 748);  //2156-1408);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 44);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 140);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 641); //2059-1408);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 148);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 659);//2067-1408);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 45);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 364);//1124-(1080-320));

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 748);//2156);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 692);//2100);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 756);//2164);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 3);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 5);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 665);//1124);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter

		break;

	case TV_ENC_512p:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 140);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 780); //-1280

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 919); //-1280
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 876); //-1280
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 44);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 140);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 779); //-1280
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 148);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 787); //-1280
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 45);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 556); //-568

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 876); //-1280
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 820); //-1280
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 884); //-1280

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 3);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 5);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 556); //-568

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter

		break;

	case TV_ENC_1080i:
		hd21_write_reg(VENC_VIDEO_PROG_MODE, 0x0100);

		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x1ffc | (1 << 14)); // bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0019);
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 384);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 4223);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 4399);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 4312);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 88);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 88);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 264);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 2024);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 383);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 4222);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 20);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 559);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 15);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 31);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 15);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 31);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 10);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 12);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 1124);

		hd21_write_reg(T_ENCP_VIDEO_OFLD_VPEQ_OFST, 0x0100);
		hd21_write_reg(T_ENCP_VIDEO_OFLD_VOAV_OFST, 0x0111);

		break;

	case TV_ENC_576i:
		hd21_write_reg(T_ENCI_CFILT_CTRL, 0x0800);
		hd21_write_reg(T_ENCI_CFILT_CTRL2, 0x0010);

		//adjust the hsync start point and end point
		hd21_write_reg(T_ENCI_SYNC_HSO_BEGIN, 1);
		hd21_write_reg(T_ENCI_SYNC_HSO_END, 127);

		//adjust the vsync start line and end line
		hd21_write_reg(T_ENCI_SYNC_VSO_EVNLN, (0 << 8) | 3);
		hd21_write_reg(T_ENCI_SYNC_VSO_ODDLN, (0 << 8) | 3);

		// adjust for interlace output, Horizontal offset after HSI in slave mode,
		hd21_write_reg(T_ENCI_SYNC_HOFFST, 0x16);

		hd21_write_reg(T_ENCI_MACV_MAX_AMP, 0x8107); // ENCI_MACV_MAX_AMP
		hd21_write_reg(VENC_VIDEO_PROG_MODE, 0xff); /* Set for interlace mode */
		hd21_write_reg(T_ENCI_VIDEO_MODE, 0x13); /* Set for PAL mode */
		/* Set for High Bandwidth for CBW&YBW */
		hd21_write_reg(T_ENCI_VIDEO_MODE_ADV, 0x26);
		hd21_write_reg(T_ENCI_VIDEO_SCH, 0x28); /* Set SCH */
		hd21_write_reg(T_ENCI_SYNC_MODE, 0x07); /* Set for master mode */

		hd21_write_reg(T_ENCI_VFIFO2VD_PIXEL_START, 267);
		hd21_write_reg(T_ENCI_VFIFO2VD_PIXEL_END, 267 + 1440);
		hd21_write_reg(T_ENCI_VFIFO2VD_LINE_TOP_START, 21);
		hd21_write_reg(T_ENCI_VFIFO2VD_LINE_TOP_END, 21 + 288);
		hd21_write_reg(T_ENCI_VFIFO2VD_LINE_BOT_START, 22);
		hd21_write_reg(T_ENCI_VFIFO2VD_LINE_BOT_END, 22 + 288);
		hd21_write_reg(T_ENCI_VFIFO2VD_CTL, (0x4e << 8) | 1);     // enable vfifo2vd
		hd21_write_reg(T_ENCI_DBG_PX_RST, 0x0); /* Enable TV encoder */
		break;

	case TV_ENC_2205p:  // 1920x(1080+45+1080), 3D frame packing for 1920x1080p
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 140);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 2060);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 2749);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 2156);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 44);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 140);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 2059);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 148);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 2067);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 43);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 2247);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 2156);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 2100);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 2164);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 3);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 5);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 2249);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter

		break;

	case TV_ENC_2440p:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 140);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 140 + 1920 * 2);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 3840 + 500 - 1);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 2156 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 44);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 140);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 2059 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 148);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 2067 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 41);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 1120 + 1360);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 2156 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 2100 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 2164 + 1920);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 3);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 5);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 1124 + 1360);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter

		break;

	case TV_ENC_3840x2160p_vic01:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 140);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 140 + 3840);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 4439);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 148);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 3987);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 89);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 2248);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 2156 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 2100 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 2164 + 1920);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 51);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 53);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 2249);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter

		break;

	case TV_ENC_2048x1536:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 140);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 140 + 2048);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 2719);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 1576);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 139);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 2047 + 139);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 43);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 1535 + 43);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 27);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 59);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 27);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 59);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 1);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 7);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter

		break;

	case TV_ENC_3840x2160p_vic03:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 140);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 140 + 3840);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 5499);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 2156 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 44);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 140);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 2059 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 148);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 3987);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 89);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 2248);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 2156 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 2100 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 2164 + 1920);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 51);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 53);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 2249);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter

		break;

	case TV_ENC_4096x2160p_vic04:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 140);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 140 + 4096);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 5499);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 2156 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 44);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 140);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 2059 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 148);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 4243);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 89);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 2248);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 2156 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 2100 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 2164 + 1920);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 51);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 53);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 2249);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter

		break;

	case TV_ENC_4096x2160p_vic102:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 140);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 140 + 4096);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 4399);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 2249);

		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 2156 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 44);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 140);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 2059 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 148);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 4243);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 89);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 2248);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 2156 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 2100 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 2164 + 1920);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 51);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 53);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter
		break;
	case TV_ENC_4096x540p_240hz:
		// Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1
		hd21_write_reg(T_ENCP_VIDEO_MODE, 0x0040 | (1 << 14));
		hd21_write_reg(T_ENCP_VIDEO_MODE_ADV, 0x0008); // Sampling rate: 1
		hd21_write_reg(T_ENCP_VIDEO_YFP1_HTIME, 140);
		hd21_write_reg(T_ENCP_VIDEO_YFP2_HTIME, 140 + 4096);

		hd21_write_reg(T_ENCP_VIDEO_MAX_PXCNT, 4399);
		hd21_write_reg(T_ENCP_VIDEO_MAX_LNCNT, 562);

		hd21_write_reg(T_ENCP_VIDEO_HSPULS_BEGIN, 2156 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_END, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSPULS_SWITCH, 44);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BEGIN, 140);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_END, 2059 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_BLINE, 0);
		hd21_write_reg(T_ENCP_VIDEO_VSPULS_ELINE, 4);

		hd21_write_reg(T_ENCP_VIDEO_HAVON_BEGIN, 148);
		hd21_write_reg(T_ENCP_VIDEO_HAVON_END, 4243);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_BLINE, 22);
		hd21_write_reg(T_ENCP_VIDEO_VAVON_ELINE, 561);

		hd21_write_reg(T_ENCP_VIDEO_HSO_BEGIN, 44);
		hd21_write_reg(T_ENCP_VIDEO_HSO_END, 2156 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_BEGIN, 2100 + 1920);
		hd21_write_reg(T_ENCP_VIDEO_VSO_END, 2164 + 1920);

		hd21_write_reg(T_ENCP_VIDEO_VSO_BLINE, 12);
		hd21_write_reg(T_ENCP_VIDEO_VSO_ELINE, 13);

		hd21_write_reg(T_ENCP_VIDEO_FILT_CTRL, 0x1000); //bypass filter
		break;
	default:
		break;
	}
} /* config_tv_enc_new */

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

void adjust_encp_for_hdmi(u8     enc_index,
				u8     interlace_mode,
				u32    total_pixels_venc,
				u32    active_pixels_venc,
				u32    front_porch_venc,
				u32    hsync_pixels_venc,
				u32    total_lines,
				u32    active_lines,
				u32    sof_lines,
				u32    vsync_lines,
				u32    vs_adjust_420)
{
	u32    reg_offset;
	// Latency in pixel clock from ENCP_VFIFO2VD request to data ready to HDMI
	u32    vfifo2vd_to_hdmi_latency = 2;
	u32    de_h_begin, de_h_end;
	u32    de_v_begin_even, de_v_end_even, de_v_begin_odd, de_v_end_odd;
	u32    hs_begin, hs_end;
	u32    vs_adjust;
	u32    vs_bline_evn, vs_eline_evn, vs_bline_odd, vs_eline_odd;
	u32    vso_begin_evn, vso_begin_odd;

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

void set_tv_encp_new(u32 enc_index, enum tvenc_type output_type, u32 enable)
{
	u32 reg_offset;
	// VENC0A 0x1b
	// VENC1A 0x21
	// VENC2A 0x23
	reg_offset = enc_index == 0 ? 0 : enc_index == 1 ? 0x600 : 0x800;

	config_tv_enc_new(enc_index, output_type);

	set_encp_to_hdmi(enc_index);

	if (enable) {
		hd21_write_reg(ENCP_VIDEO_EN + (reg_offset << 2), 1);  // Enable VENCP
		hd21_write_reg(VPU_VENC_CTRL + (reg_offset << 2), 1);  // sel encp timming
	}
} /* set_tv_encp_new */
