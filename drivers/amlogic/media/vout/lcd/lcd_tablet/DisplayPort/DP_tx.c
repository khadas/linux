// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "DP_tx.h"
#include "../../lcd_reg.h"
#include "../../lcd_common.h"

/* @note:
 * use_known invalid for eDP
 * use DP_cfg->last_good if use_known, else use 0
 */
int dptx_set_phy_config(struct aml_lcd_drv_s *pdrv, unsigned char use_known)
{
	unsigned char aux_data[4];
	unsigned int vswing[4], preem[4];
	int ret, i;
	struct edp_config_s *DP_cfg = &pdrv->config.control.edp_cfg;

	for (i = 0; i < 4; i++)	{
		//vswing[i] = use_known ? DP_cfg->last_good_vswing[i] : 0;
		//preem[i] = use_known ? DP_cfg->last_good_preem[i] : 0;
		vswing[i] = dptx_vswing_phy_to_std(DP_cfg->phy_vswing_preset);
		preem[i] = dptx_preem_phy_to_std(DP_cfg->phy_preem_preset);
	}

	dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_0, vswing[0]);
	dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_1, vswing[1]);
	dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_2, vswing[2]);
	dptx_reg_write(pdrv, EDP_TX_PHY_VOLTAGE_DIFF_LANE_3, vswing[3]);
	dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_0, preem[0]);
	dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_1, preem[1]);
	dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_2, preem[2]);
	dptx_reg_write(pdrv, EDP_TX_PHY_PRE_EMPHASIS_LANE_3, preem[3]);

	//write the preset values to the sink device
	aux_data[0] = to_DPCD_LANESET(vswing[0], preem[0]);
	aux_data[1] = to_DPCD_LANESET(vswing[1], preem[1]);
	aux_data[2] = to_DPCD_LANESET(vswing[2], preem[2]);
	aux_data[3] = to_DPCD_LANESET(vswing[3], preem[3]);
	ret = dptx_aux_write(pdrv, DPCD_TRAINING_LANE0_SET, 4, aux_data);
	if (ret)
		LCDERR("[%d]: DP sink set phy failed.....\n", pdrv->index);

	return ret;
}

int dptx_set_lane_config(struct aml_lcd_drv_s *pdrv)
{
	unsigned int lane_cnt, link_rate, enhance_framing, ss_enable;
	unsigned char auxdata[2];
	int ret;

	lane_cnt = pdrv->config.control.edp_cfg.lane_count;
	link_rate = pdrv->config.control.edp_cfg.link_rate;
	ss_enable = pdrv->config.control.edp_cfg.down_ss;

	LCDPR("[%d]: %s: %s, %d lane, ss_en: %d\n", pdrv->index, __func__,
		DP_link_rate_str[link_rate], lane_cnt, ss_enable);

	enhance_framing = dptx_reg_read(pdrv, EDP_TX_ENHANCED_FRAME_EN) & 0x01;
	enhance_framing = 0;

	// tx Link-rate and Lane_count
	dptx_reg_write(pdrv, EDP_TX_LINK_BW_SET, DP_link_rate_to_val[link_rate]);
	dptx_reg_write(pdrv, EDP_TX_LINK_COUNT_SET, lane_cnt);
	dptx_reg_write(pdrv, EDP_TX_ENHANCED_FRAME_EN, enhance_framing);

	dptx_reg_write(pdrv, EDP_TX_PHY_POWER_DOWN, (0xf << lane_cnt) & 0xf);

	dptx_reg_write(pdrv, EDP_TX_DOWNSPREAD_CTRL, ss_enable);

	// sink Link-rate and Lane_count
	auxdata[0] = DP_link_rate_to_val[link_rate];  //DPCD_LINK_BANDWIDTH_SET
	auxdata[1] = lane_cnt | enhance_framing << 7; //DPCD_LANE_COUNT_SET
	ret = dptx_aux_write(pdrv, DPCD_LINK_BW_SET, 2, auxdata);
	if (ret)
		LCDERR("[%d]: edp sink set lane rate & count failed.....\n", pdrv->index);

	auxdata[0] = (ss_enable << 4);
	ret = dptx_aux_write(pdrv, DPCD_DOWNSPREAD_CONTROL, 1, auxdata);
	if (ret)
		LCDERR("[%d]: edp sink set downspread failed.....\n", pdrv->index);

	return ret;
}

void dptx_set_clk_config(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_generate_parameter(pdrv);
	lcd_set_clk(pdrv);
}

void dptx_set_msa(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int hactive, vactive, htotal, vtotal, hsw, hbp, vsw, vbp;
	unsigned int bpc, data_per_lane, misc0_data, bit_depth, sync_mode;
	unsigned int m_vid; /*pclk/1000 */
	unsigned int n_vid; /*162000, 270000, 540000 */
	unsigned int ppc = 1; /* 1 pix per clock pix0 only */
	unsigned int cfmt = 0; /* RGB */

	hactive = pconf->basic.h_active;
	vactive = pconf->basic.v_active;
	htotal = pconf->basic.h_period;
	vtotal = pconf->basic.v_period;
	hsw = pconf->timing.hsync_width;
	hbp = pconf->timing.hsync_bp;
	vsw = pconf->timing.vsync_width;
	vbp = pconf->timing.vsync_bp;

	m_vid = pconf->timing.lcd_clk / 1000;
	switch (pconf->control.edp_cfg.link_rate) {
	case DP_LINK_RATE_HBR2: /* 5.4G */
		n_vid = 540000;
		break;
	case DP_LINK_RATE_HBR: /* 2.7G */
		n_vid = 270000;
		break;
	case DP_LINK_RATE_RBR: /* 1.62G */
	default:
		n_vid = 162000;
		break;
	}
	 /*6bit:0x0, 8bit:0x1, 10bit:0x2, 12bit:0x3 */
	if (pconf->basic.lcd_bits == 6)
		bit_depth = 0x0;
	else if (pconf->basic.lcd_bits == 10)
		bit_depth = 0x2;
	else if (pconf->basic.lcd_bits == 12)
		bit_depth = 0x3;
	else if (pconf->basic.lcd_bits == 16)
		bit_depth = 0x4;
	else
		bit_depth = 0x1;

	bpc = pconf->basic.lcd_bits; /* bits per color */
	sync_mode = pconf->control.edp_cfg.sync_clk_mode;
	data_per_lane = ((hactive * bpc * 3) + 15) / 16 - 1;

	/*bit[0] sync mode (1=sync 0=async) */
	misc0_data = (bit_depth << 5) | (cfmt << 1) | (sync_mode << 0);

	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HTOTAL, htotal);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VTOTAL, vtotal);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_POLARITY, (0 << 1) | (0 << 0));
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HSWIDTH, hsw);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VSWIDTH, vsw);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HRES, hactive);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VRES, vactive);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_HSTART, (hsw + hbp));
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_VSTART, (vsw + vbp));
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_MISC0, misc0_data);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_MISC1, 0x00000000);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_M_VID, m_vid); /*unit: 1kHz */
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_N_VID, n_vid); /*unit: 10kHz */
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_TRANSFER_UNIT_SIZE, 48);
		/*Temporary change to 48 */
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_DATA_COUNT_PER_LANE, data_per_lane);
	dptx_reg_write(pdrv, EDP_TX_MAIN_STREAM_USER_PIXEL_WIDTH, ppc);
}

void dptx_reset_part(struct aml_lcd_drv_s *pdrv, unsigned char part)
{
	unsigned int bit;

	if (part == 0)
		bit = pdrv->index ? 18 : 17; //combo dphy
	else if (part == 1)
		bit = pdrv->index ? 26 : 27; //eDP pipeline
	else
		bit = pdrv->index ? 20 : 19; //eDP ctrl

	lcd_reset_setb(pdrv, RESETCTRL_RESET1_MASK, 0, bit, 1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 0, bit, 1);
	udelay(1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET1_LEVEL, 1, bit, 1);
	udelay(5);
}

void dptx_reset(struct aml_lcd_drv_s *pdrv)
{
	dptx_reg_write(pdrv, EDP_TX_PHY_RESET, 0xf); //reset the PHY
	usleep_range(1000, 1200);

	dptx_reset_part(pdrv, 0);
	dptx_reset_part(pdrv, 1);
	dptx_reset_part(pdrv, 2);

	usleep_range(1000, 1200);

	// Set Aux channel clk-div: 24MHz
	dptx_reg_write(pdrv, EDP_TX_AUX_CLOCK_DIVIDER, 24);

	// Enable the transmitter
	// remove the reset on the PHY
	dptx_reg_write(pdrv, EDP_TX_PHY_RESET, 0);
}

int dptx_wait_phy_ready(struct aml_lcd_drv_s *pdrv)
{
	unsigned int data = 0;
	unsigned int done = 100;

	do {
		data = dptx_reg_read(pdrv, EDP_TX_PHY_STATUS);
		if (done < 20) {
			LCDPR("dptx%d wait phy ready: reg_val=0x%x, wait_count=%u\n",
			      pdrv->index, data, (100 - done));
		}
		done--;
		usleep_range(100, 120);
	} while (((data & 0x7f) != 0x7f) && (done > 0));

	if ((data & 0x7f) == 0x7f)
		return 0;

	LCDERR("[%d]: edp tx phy error!\n", pdrv->index);
	return -1;
}

int dptx_band_width_check(enum DP_link_rate_e link_rate, unsigned char lane_cnt,
					unsigned long pclk, unsigned char bit_per_pixel)
{
	unsigned long bw_req, bw_cal, bit_rate;

	switch (link_rate) {
	case DP_LINK_RATE_HBR2:
		bit_rate = 5400000; //khz
		break;
	case DP_LINK_RATE_HBR:
		bit_rate = 2700000; //khz
		break;
	case DP_LINK_RATE_RBR:
	default:
		bit_rate = 1620000; //khz
		break;
	}

	bw_cal = bit_rate * lane_cnt * 8 / 10;
	bw_req = ((pclk + 999) / 1000) * bit_per_pixel; //kbps
	if (bw_cal < bw_req) {
		LCDERR("%s: bw_cal %ld not enough, bw_req %ld\n", __func__, bw_cal, bw_req);
		return 1;
	}
	return 0;
}

int DPCD_capability_detect(struct aml_lcd_drv_s *pdrv)
{
	unsigned char auxdata[5];
	int ret = 0;
	struct DP_dev_support_s sink_sp, *source_sp;
	struct edp_config_s *DP_cfg = &pdrv->config.control.edp_cfg;

	ret = dptx_aux_read(pdrv, DPCD_DPCD_REV, 5, auxdata);
	if (ret)
		return ret;

	//DPCD_REVISION
	sink_sp.DPCD_rev       = auxdata[0];
	//DPCD_MAX_LINK_RATE
	sink_sp.link_rate      = DP_val_to_link_rate(auxdata[1]);
	//DPCD_MAX_LANE_COUNT
	sink_sp.line_cnt       = auxdata[2] & 0xf;
	sink_sp.enhanced_frame = (auxdata[2] >> 7) & 0x1;
	sink_sp.TPS3           = (auxdata[2] >> 6) & 0x1;
	//DPCD_MAX_DOWNSPREAD
	sink_sp.down_spread    = auxdata[3] & 0x1;
	sink_sp.TPS4           = (auxdata[3] >> 7) & 0x1;

	ret = dptx_aux_read(pdrv, DPCD_TRAINING_AUX_RD_INTERVAL, 1, auxdata);
	if (ret)
		return ret;
	sink_sp.train_aux_rd_interval = auxdata[0];

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
	default:
		source_sp = &source_support_T7;
		break;
	}

	DP_cfg->max_lane_count = CAP_COMP(source_sp->line_cnt, sink_sp.line_cnt);
	DP_cfg->max_link_rate = CAP_COMP(source_sp->link_rate, sink_sp.link_rate);
	DP_cfg->enhanced_framing_en = CAP_COMP(source_sp->enhanced_frame, sink_sp.enhanced_frame);
	DP_cfg->down_ss = CAP_COMP(source_sp->down_spread, sink_sp.down_spread);
	DP_cfg->TPS_support =
		(source_sp->TPS3 && sink_sp.TPS3) << 0 |
		(source_sp->TPS4 && sink_sp.TPS4) << 1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		sink_sp.link_rate = sink_sp.link_rate > 6 ? 7 : sink_sp.link_rate;
		DP_cfg->max_link_rate = DP_cfg->max_link_rate > 6 ? 7 : DP_cfg->max_link_rate;
		LCDPR("[%d]: %s:\n"
			" - DPCD reversion: %01x.%01x\n"
			" - link_rate:      %s -> %s\n"
			" - lane_cnt:       %d\n"
			" - enhanced_frame: %d\n"
			" - TPS3 :          %d\n"
			" - TPS4 :          %d\n"
			" - down_spread:    %d\n",
			pdrv->index, __func__, sink_sp.DPCD_rev >> 4, sink_sp.DPCD_rev & 0xf,
			DP_link_rate_str[sink_sp.link_rate],
			DP_link_rate_str[DP_cfg->max_link_rate],
			DP_cfg->max_lane_count, DP_cfg->enhanced_framing_en,
			sink_sp.TPS3, sink_sp.TPS4, sink_sp.down_spread);
	}
	return 0;
}
