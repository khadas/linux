
/*
 * drivers/amlogic/display/lcd/lcd_tv/lvds_drv.c
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

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/lcd_vout.h>
#include "lcd_tv.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

static void set_tcon_lvds(void)
{
	vpp_set_matrix_ycbcr2rgb(2, 0);

	lcd_vcbus_write(L_RGB_BASE_ADDR, 0);
	lcd_vcbus_write(L_RGB_COEFF_ADDR, 0x400);

	lcd_vcbus_write(L_DITH_CNTL_ADDR,  0x400);

	lcd_vcbus_write(VPP_MISC,
		lcd_vcbus_read(VPP_MISC) & ~(VPP_OUT_SATURATE));
}

static void init_lvds_phy(void)
{
	lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL1, 0x6c6cca80);
	lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL2, 0x0000006c);
	lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL3, 0x0fff0800);
}

static void set_lvds_clk_util(struct lcd_config_s *pconf)
{
	unsigned int fifo_mode, phy_div, dual_port;

	dual_port = pconf->lcd_control.lvds_config->dual_port;
	if (dual_port) {
		fifo_mode = 0x3;
		phy_div = 2;
	} else {
		fifo_mode = 0x1;
		phy_div = 1;
	}

	lcd_vcbus_write(LVDS_GEN_CNTL,
		(lcd_vcbus_read(LVDS_GEN_CNTL) | (1 << 4) | (fifo_mode << 0)));
	lcd_vcbus_setb(LVDS_GEN_CNTL, 1, 3, 1);

	/* set fifo_clk_sel: div 7 */
	lcd_hiu_write(HHI_LVDS_TX_PHY_CNTL0, (1 << 6));
	/* set cntl_ser_en:  8-channel to 1 */
	lcd_hiu_setb(HHI_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);

	/* decoupling fifo enable, gated clock enable */
	lcd_hiu_write(HHI_LVDS_TX_PHY_CNTL1,
		(1 << 30) | ((phy_div - 1) << 25) | (1 << 24));
	/* decoupling fifo write enable after fifo enable */
	lcd_hiu_setb(HHI_LVDS_TX_PHY_CNTL1, 1, 31, 1);
}

static void set_control_lvds(struct lcd_config_s *pconf)
{
	unsigned int bit_num = 1;
	unsigned int pn_swap = 0;
	unsigned int dual_port = 1;
	unsigned int lvds_repack = 1;
	unsigned int port_swap = 0;

	set_lvds_clk_util(pconf);

	lvds_repack = (pconf->lcd_control.lvds_config->lvds_repack) & 0x1;
	pn_swap   = (pconf->lcd_control.lvds_config->pn_swap) & 0x1;
	dual_port = (pconf->lcd_control.lvds_config->dual_port) & 0x1;
	port_swap = (pconf->lcd_control.lvds_config->port_swap) & 0x1;

	switch (pconf->lcd_basic.lcd_bits) {
	case 10:
		bit_num = 0;
		break;
	case 8:
		bit_num = 1;
		break;
	case 6:
		bit_num = 2;
		break;
	case 4:
		bit_num = 3;
		break;
	default:
		bit_num = 1;
		break;
	}

	lcd_vcbus_write(LVDS_PACK_CNTL_ADDR,
		(lvds_repack << 0) | /* repack */
		(port_swap << 2) | /* odd_even */
		(0 << 3) |	/* reserve */
		(0 << 4) |	/* lsb first */
		(pn_swap << 5) | /* pn swap */
		(dual_port << 6) | /* dual port */
		(0 << 7) |	/* use tcon control */
		(bit_num << 8) | /* 0:10bits, 1:8bits, 2:6bits, 3:4bits */
		(0 << 10) |	/* r_select  //0:R, 1:G, 2:B, 3:0 */
		(1 << 12) |	/* g_select  //0:R, 1:G, 2:B, 3:0 */
		(2 << 14));	/* b_select  //0:R, 1:G, 2:B, 3:0 */
}

static void venc_set_lvds(struct lcd_config_s *pconf)
{
	unsigned int h_active, v_active;
	unsigned int video_on_pixel, video_on_line;

	h_active = pconf->lcd_basic.h_active;
	v_active = pconf->lcd_basic.v_active;
	video_on_pixel = pconf->lcd_timing.video_on_pixel;
	video_on_line = pconf->lcd_timing.video_on_line;

	lcd_vcbus_write(ENCL_VIDEO_EN, 0);

	/* viu1 select encl | viu2 select encl */
	lcd_vcbus_write(VPU_VIU_VENC_MUX_CTRL, ((0 << 0) | (0 << 2)));
	/* Enable Hsync and equalization pulse switch in center;
	   bit[14] cfg_de_v = 1 */
	lcd_vcbus_write(ENCL_VIDEO_MODE, 0);
	lcd_vcbus_write(ENCL_VIDEO_MODE_ADV, 0x0418); /* Sampling rate: 1 */

	lcd_vcbus_write(ENCL_VIDEO_FILT_CTRL, 0x1000); /* bypass filter */
	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT, pconf->lcd_basic.h_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT, pconf->lcd_basic.v_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN, video_on_pixel);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END,   h_active - 1 + video_on_pixel);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE, video_on_line);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE, v_active - 1  + video_on_line);

	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN, pconf->lcd_timing.hs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END,   pconf->lcd_timing.hs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN, pconf->lcd_timing.vs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END,   pconf->lcd_timing.vs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE, pconf->lcd_timing.vs_vs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE, pconf->lcd_timing.vs_ve_addr);
	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, 3);

	lcd_vcbus_write(ENCL_VIDEO_EN, 1);
}

static void lcd_set_clk_pll(void)
{
	unsigned int pll_lock;
	int wait_loop = 100;

	lcd_hiu_write(HHI_HDMI_PLL_CNTL, 0x58000256);
	lcd_hiu_write(HHI_HDMI_PLL_CNTL2, 0x00444280);
	lcd_hiu_write(HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
	lcd_hiu_write(HHI_HDMI_PLL_CNTL4, 0x801da72c);
	lcd_hiu_write(HHI_HDMI_PLL_CNTL5, 0x71486980);
	lcd_hiu_write(HHI_HDMI_PLL_CNTL6, 0x00000e55);
	lcd_hiu_write(HHI_HDMI_PLL_CNTL, 0x48000256);
	do {
		mdelay(10);
		pll_lock = (lcd_hiu_read(HHI_HDMI_PLL_CNTL) >> 31) & 0x1;
		wait_loop--;
	} while ((pll_lock == 0) && (wait_loop > 0));
	if (wait_loop == 0)
		LCDPR("error: hpll lock failed\n");
}

static void set_clk_lvds(struct lcd_config_s *pconf)
{
	lcd_clk_gate_on();
	lcd_set_clk_pll();
	lcd_clocks_set_vid_clk_div(CLK_DIV_SEL_7);
	lcd_set_crt_video_enc(0, 0, 1);
	lcd_enable_crt_video_encl(1, 0);
}

int lvds_init(struct lcd_config_s *pconf)
{
	set_clk_lvds(pconf);
	venc_set_lvds(pconf);
	set_control_lvds(pconf);
	set_tcon_lvds();
	init_lvds_phy();

	lcd_vcbus_write(VENC_INTCTRL, 0x200);

	return 0;
}

