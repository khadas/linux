// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#ifdef CONFIG_AMLOGIC_POWER
#include <linux/amlogic/power_domain.h>
#endif
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_reg.h"
#include "lcd_common.h"
#include "edp_tx_reg.h"

#define EDP_AUX_RETRY_CNT    5
#define EDP_AUX_TIMEOUT      1000
#define EDP_AUX_INTERVAL     200
static int dptx_aux_write(unsigned int addr, unsigned int len,
			  unsigned char *buf)
{
	unsigned int data, i, state;
	int retry_cnt = 0, timeout = 0;

aux_write_retry:
	do {
		state = dptx_reg_read(EDP_TX_AUX_STATE);
	} while (state & (1 << 1));

	dptx_reg_write(EDP_TX_AUX_ADDRESS, addr);
	for (i = 0; i < len; i++)
		dptx_reg_write(EDP_TX_AUX_WRITE_FIFO, buf[i]);

	dptx_reg_write(EDP_TX_AUX_COMMAND, (0x800 | ((len - 1) & 0xf)));

	while (timeout++ < EDP_AUX_TIMEOUT) {
		lcd_delay_us(EDP_AUX_INTERVAL);
		data = dptx_reg_read(EDP_TX_AUX_TRANSFER_STATUS);
		if (data & (1 << 0)) {
			state = dptx_reg_read(EDP_TX_AUX_REPLY_CODE);
			if (state == 0)
				return 0;
			if (state == 1) {
				LCDPR("edp aux write addr 0x%x NACK!\n", addr);
				return -1;
			}
			if (state == 2)
				LCDPR("  edp aux write addr 0x%x Defer!\n", addr);
			break;
		}

		if (data & (1 << 3)) {
			LCDPR("  edp aux write addr 0x%x Error!\n", addr);
			break;
		}
	}

	if (retry_cnt++ < EDP_AUX_RETRY_CNT) {
		lcd_delay_us(EDP_AUX_INTERVAL);
		LCDPR("edp aux write addr 0x%x timeout, retry %d\n", addr, retry_cnt);
		goto aux_write_retry;
	}

	LCDPR("edp aux write addr 0x%x failed\n", addr);
	return -1;
}

static int dptx_aux_read(unsigned int addr, unsigned int len, unsigned char *buf)
{
	unsigned int data, i, state;
	int retry_cnt = 0, timeout = 0;

aux_read_retry:
	do {
		state = dptx_reg_read(EDP_TX_AUX_STATE);
	} while (state & (1 << 1));

	dptx_reg_write(EDP_TX_AUX_ADDRESS, addr);
	dptx_reg_write(EDP_TX_AUX_COMMAND, (0x900 | ((len - 1) & 0xf)));

	while (timeout++ < EDP_AUX_TIMEOUT) {
		lcd_delay_us(EDP_AUX_INTERVAL);
		data = dptx_reg_read(EDP_TX_AUX_TRANSFER_STATUS);
		if (data & (1 << 0)) {
			state = dptx_reg_read(EDP_TX_AUX_REPLY_CODE);
			if (state == 0)
				goto aux_read_succeed;
			if (state == 1) {
				LCDPR("edp aux read addr 0x%x NACK!\n", addr);
				return -1;
			}
			if (state == 2)
				LCDPR("  edp aux read addr 0x%x Defer!\n", addr);
			break;
		}

		if (data & (1 << 3)) {
			LCDPR("  edp aux read addr 0x%x Error!\n", addr);
			break;
		}
	}

	if (retry_cnt++ < EDP_AUX_RETRY_CNT) {
		lcd_delay_us(EDP_AUX_INTERVAL);
		LCDPR("  edp aux read addr 0x%x timeout, retry %d\n", addr, retry_cnt);
		goto aux_read_retry;
	}

	LCDPR("edp aux read addr 0x%x failed\n", addr);
	return -1;

aux_read_succeed:
	for (i = 0; i < len; i++)
		buf[i] = (unsigned char)(dptx_reg_read(EDP_TX_AUX_REPLY_DATA));

	return 0;
}

static void dptx_link_fast_training(void)
{
	unsigned char p_data = 0;
	int ret;

	LCDPR("..... Sending edp training pattern 1 .....\n");
	dptx_reg_write(EDP_TX_SCRAMBLING_DISABLE, 0x1);
	dptx_reg_write(EDP_TX_TRAINING_PATTERN_SET, 0x1);

	p_data = 0x21;
	ret = dptx_aux_write(0x102, 1, &p_data);
	if (ret)
		LCDPR("..... edp training pattern 1 failed.....\n");

	lcd_delay_us(10);

	LCDPR("..... Sending training pattern 2 .....\n");
	dptx_reg_write(EDP_TX_TRAINING_PATTERN_SET, 0x2);
	p_data = 0x22;
	ret = dptx_aux_write(0x102, 1, &p_data);
	if (ret)
		LCDPR("..... edp training pattern 2 failed.....\n");

	lcd_delay_us(10);

	LCDPR("..... Sending training pattern 3 .....\n");
	dptx_reg_write(EDP_TX_TRAINING_PATTERN_SET, 0x3);
	p_data = 0x23;
	ret = dptx_aux_write(0x102, 1, &p_data);
	if (ret)
		LCDPR("..... edp training pattern 3 failed.....\n");

	lcd_delay_us(10);

	p_data = 0x20;
	ret = dptx_aux_write(0x102, 1, &p_data);
	if (ret)
		LCDPR("..... edp training pattern off failed.....\n");

	dptx_reg_write(EDP_TX_TRAINING_PATTERN_SET, 0x0);
}

static void dptx_dpcd_dump(void)
{
	unsigned char p_data[12];
	int ret, i;

	memset(p_data, 0, 12);
	ret = dptx_aux_read(0x00e, 1, p_data);
	if (ret == 0) {
		LCDPR("0x000e: 0x%02x\n", p_data[0]);
		LCDPR("\n");
	}

	memset(p_data, 0, 12);
	LCDPR("edp DPCD link status:\n");
	ret = dptx_aux_read(0x100, 8, p_data);
	if (ret == 0) {
		for (i = 0; i < 8; i++)
			LCDPR("0x%04x: 0x%02x\n", (0x100 + i), p_data[i]);
		LCDPR("\n");
	}

	memset(p_data, 0, 12);
	LCDPR("edp DPCD training status:\n");
	ret = dptx_aux_read(0x200, 12, p_data);
	if (ret == 0) {
		for (i = 0; i < 12; i++)
			LCDPR("0x%04x: 0x%02x\n", (0x200 + i), p_data[i]);
		LCDPR("\n");
	}
}

static void dptx_set_msa(void)
{
	unsigned int hactive = 1920;
	unsigned int vactive = 1080;
	unsigned int htotal = 2200;
	unsigned int vtotal = 1120;
	unsigned int hsw = 44;
	unsigned int hbp = 148;
	unsigned int vsw = 5;
	unsigned int vbp = 30;
	/*unsigned int pclk = 147840000;*/

	/*unsigned int lane_count = 2;*/
	unsigned int data_per_lane;
	unsigned int misc0_data;
	unsigned int m_vid = 147840; /*pclk/1000*/
	unsigned int n_vid = 270000; /*162000, 270000, 540000*/

	unsigned int ppc = 1;/* 1 pix per clock pix0 only*/
	unsigned int cfmt = 0;/* RGB*/
	unsigned int bpc = 8;/* bits per color*/

	data_per_lane = ((hactive * bpc * 3) + 15) / 16 - 1;

	/*bit[0] sync mode (1=sync 0=async)*/
	misc0_data = (cfmt << 1) | (1 << 0);
	misc0_data |= (0x1 << 5); /*6bit:0x0, 8bit:0x1, 10bit:0x2, 12bit:0x3*/

	dptx_reg_write(EDP_TX_MAIN_STREAM_HTOTAL, htotal);
	dptx_reg_write(EDP_TX_MAIN_STREAM_VTOTAL, vtotal);
	dptx_reg_write(EDP_TX_MAIN_STREAM_POLARITY, (0 << 1) | (0 << 0));
	dptx_reg_write(EDP_TX_MAIN_STREAM_HSWIDTH, hsw);
	dptx_reg_write(EDP_TX_MAIN_STREAM_VSWIDTH, vsw);
	dptx_reg_write(EDP_TX_MAIN_STREAM_HRES, hactive);
	dptx_reg_write(EDP_TX_MAIN_STREAM_VRES, vactive);
	dptx_reg_write(EDP_TX_MAIN_STREAM_HSTART, (hsw + hbp));
	dptx_reg_write(EDP_TX_MAIN_STREAM_VSTART, (vsw + vbp));
	dptx_reg_write(EDP_TX_MAIN_STREAM_MISC0, misc0_data);
	dptx_reg_write(EDP_TX_MAIN_STREAM_MISC1, 0x00000000);
	dptx_reg_write(EDP_TX_MAIN_STREAM_M_VID, m_vid); /*unit: 1kHz*/
	dptx_reg_write(EDP_TX_MAIN_STREAM_N_VID, n_vid); /*unit: 10kHz*/
	dptx_reg_write(EDP_TX_MAIN_STREAM_TRANSFER_UNIT_SIZE, 32);
	dptx_reg_write(EDP_TX_MAIN_STREAM_DATA_COUNT_PER_LANE, data_per_lane);
	dptx_reg_write(EDP_TX_MAIN_STREAM_USER_PIXEL_WIDTH, ppc);

	LCDPR(" edp MSA done\n");
}

static void dptx_reset(void)
{
	/* Level reset mail*/
	lcd_rst_setb(RESETCTRL_RESET1_MASK, 0, 17, 1);
	lcd_rst_setb(RESETCTRL_RESET1_LEVEL, 0, 17, 1);
	lcd_delay_us(1);
	lcd_rst_setb(RESETCTRL_RESET1_LEVEL, 1, 17, 1);

	/* Set clock-divider for EDP-APB*/
	dptx_reg_write(EDP_TX_AUX_CLOCK_DIVIDER, 24);
}

static void dptx_phy_reset(void)
{
	/* Level reset mail*/
	lcd_rst_setb(RESETCTRL_RESET1_MASK, 0, 19, 1);
	lcd_rst_setb(RESETCTRL_RESET1_LEVEL, 0, 19, 1);
	lcd_delay_us(1);
	lcd_rst_setb(RESETCTRL_RESET1_LEVEL, 1, 19, 1);
}

static void dptx_init(void)
{
	unsigned int data32;

	/* Select Data-in*/
	data32 = lcd_combo_dphy_read(COMBO_DPHY_CNTL0);
	data32 &= (~((0x1 << 0) | (0x1 << 1)));
	data32 |= ((0x0 << 0) | (0x1 << 1)); /*reg_phy0_din_sel_edp*/
	lcd_combo_dphy_write(COMBO_DPHY_CNTL0, data32);

	data32 = (0x1 | (0x1 << 4) | (0x0 << 5) | (0x1 << 6) | (0x1 << 7));
	lcd_combo_dphy_write(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1, data32);
	data32 = ((0x4 << 5) | (0x1f << 16)); /*select clkdiv20 , datachan-sel*/
	lcd_combo_dphy_write(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0, data32);
	data32 = lcd_combo_dphy_read(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0);
	data32 &= ~(0x1 << 2);  /*no pn sawp*/
	lcd_combo_dphy_write(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0, data32);
	/* Mux pads in combo-phy -- Lanes 0 through 4*/
	data32 = lcd_combo_dphy_read(COMBO_DPHY_CNTL1);
	data32 &= (~(0x3 | (0x3 << 2) | (0x3 << 4) | (0x3 << 6) | (0x3 << 8)));
	data32 |= (0x1 | (0x1 << 2) | (0x1 << 4) | (0x1 << 6) | (0x1 << 8));
	lcd_combo_dphy_write(COMBO_DPHY_CNTL1, data32); /* Select EDP*/

	/*Hot-plug GPIO*/
	data32 = lcd_periphs_read(PADCTRL_PIN_MUX_REGK);
	data32 &= (~(0xf << 8));
	data32 |= (0x4 << 8);
	lcd_periphs_write(PADCTRL_PIN_MUX_REGK, data32);

	/* Set clock-divider for EDP-APB*/
	dptx_reg_write(EDP_TX_AUX_CLOCK_DIVIDER, 24);
}

static void dptx_set_phy_clk(uint16_t lnkclk_x100)
{
	dptx_phy_reset();
	dptx_reset();
}

static void dptx_set_pixclk_divN(uint32_t N)
{
	unsigned int sel0; /* 1,2,3,4,5,7,8,9,11,13,17,19,23,29,31*/
	unsigned int sel1; /* 1,2,3,4,5,6,7,8,9,13*/

	switch (N) {
	case 10:
		sel0 = 4;
		sel1 = 1;
		break;
	case 13:
		sel0 = 9;
		sel1 = 0;
		break;
	case 18:
		sel0 = 2;
		sel1 = 5;
		break;
	case 64:
		sel0 = 6;
		sel1 = 7;
		break;
	default:
		sel0 = 2;
		sel1 = 1;
		break;
	}

	/* Disable edp_div clock*/
	lcd_combo_dphy_setb(COMBO_DPHY_EDP_PIXEL_CLK_DIV, 0, 24, 1);

	/* Disable hdmi clk_div clock output + bypass*/
	lcd_combo_dphy_write(COMBO_DPHY_VID_PLL0_DIV, (0 << 19) | (1 << 18));

	/* Set EDP clock divider*/
	lcd_combo_dphy_setb(COMBO_DPHY_EDP_PIXEL_CLK_DIV, sel0, 0, 4);
	lcd_combo_dphy_setb(COMBO_DPHY_EDP_PIXEL_CLK_DIV, sel1, 4, 4);

	/* Enable hdmi clk_div clock output + bypass*/
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 1, 19, 1);

	/* Enable edp_div clock*/
	lcd_combo_dphy_setb(COMBO_DPHY_EDP_PIXEL_CLK_DIV, 1, 24, 1);

	/* Reset controller : IP is sync reset, may not have been reset if
	 * first time video clock
	 */
	dptx_reset();
}

static int dptx_wait_phy_ready(void)
{
	unsigned int data = 0;
	unsigned int done = 100;

	do {
		data = dptx_reg_read(EDP_TX_PHY_STATUS);
		if (done < 20) {
			LCDPR("dptx wait phy ready: reg_val=0x%x, wait_count=%u\n",
				data, (100 - done));
		}
		done--;
		lcd_delay_us(100);
	} while (((data & 0x7f) != 0x7f) && (done > 0));

	if ((data & 0x7f) == 0x7f)
		return 0;

	LCDPR(" edp tx phy error!\n");
	return -1;
}

static void edp_tx_init(void)
{
	unsigned char edptx_auxdata[2];
	int ret;

	lcd_vcbus_write(ENCL_VIDEO_EN, 0);

	dptx_reset();
	dptx_init();  /* initialize aux-channel clk_div*/

	dptx_set_phy_clk(270);  /* 2.7GHz link*/
	dptx_set_pixclk_divN(18);

	dptx_reset();
	mdelay(10);

	LCDPR(" edp tx reset done, start host\n");

	/* Enable the transmitter*/
	/*dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0x1);*/
	dptx_reg_write(EDP_TX_PHY_RESET, 0);  /* remove the reset on the PHY*/
	dptx_wait_phy_ready();
	mdelay(10);
	dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0x1);

	/* Program Link-rate and Lane_count*/
	dptx_reg_write(EDP_TX_LINK_BW_SET, 0x0a); /* Link-rate*/
	dptx_reg_write(EDP_TX_LINK_COUNT_SET, 0x02); /* Number of Lanes*/

	/* Program Lane count*/
	LCDPR("edp set lane bw & count\n");
	edptx_auxdata[0] = 0x0a; /* 2.7GHz*/
	edptx_auxdata[1] = 2;
	ret = dptx_aux_write(0x100, 2, edptx_auxdata);
	if (ret)
		LCDPR("..... edp set lane bw & count failed.....\n");

	/* Power up link*/
	LCDPR("edp power up link\n");
	edptx_auxdata[0] = 0x1;
	ret = dptx_aux_write(0x600, 1, edptx_auxdata);
	if (ret)
		LCDPR("..... edp power up link failed.....\n");

	/* Fast Link train*/
	dptx_link_fast_training();
	/*dptx_dpcd_dump();*/

	dptx_set_msa();
	lcd_vcbus_write(ENCL_VIDEO_EN, 1);

	LCDPR("edp enabling the main stream video\n");
	dptx_reg_write(EDP_TX_MAIN_STREAM_ENABLE, 0x1); /* Enable main-link*/

	LCDPR(" edp tx finish\n");
}

static void edp_power_init(void)
{
/*#define PDID_T7_MIPI_DSI0	41*/
/*#define PDID_T7_EDP0		48*/
/*#define PDID_T7_EDP1		49*/
/*#define PDID_T7_MIPI_DSI1	50*/
	pwr_ctrl_psci_smc(48, 1);
	pwr_ctrl_psci_smc(49, 1);

	LCDPR(" edp power domain on\n");
}

static void lcd_init_pre_edp(void)
{
	int cnt = 0, i;

	/*1.config pll*/
set_pll_retry_edp:
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x000f04e1);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x200f04e1);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x300f04e1);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1, 0x10000000);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x0000110c);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3, 0x10051400);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x000100c0);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x008300c0);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x340f04e1);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x140f04e1);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x0000300c);
	lcd_delay_us(100);
	i = 0;
	while (i++ < 200) {
		lcd_delay_us(50);
		if (lcd_ana_read(ANACTRL_TCON_PLL0_STS) & 0x80000000)
			break;
	}
	if (!(lcd_ana_read(ANACTRL_TCON_PLL0_STS) & 0x80000000)) {
		if (cnt++ < 20)
			goto set_pll_retry_edp;
		else
			LCDPR(" pll lock failed!!!\n");
	}

	/*2.config divider*/
	/*cntrl_clk_en0 disable*/
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 0, 19, 1);
	/* Disable the div output clock */
	/*clk_final_en disable ?*/
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0, 19, 1);
	/*set_preset disable ?*/
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0, 15, 1);

	/*used COMBO not VID*/
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 1, 18, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 1, 19, 1);

	/*3.config vclk*/
	lcd_clk_write(CLKCTRL_VIID_CLK0_DIV, 0x00000000);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00080000);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK0_DIV, 0x00008000);
	lcd_clk_write(CLKCTRL_VIID_CLK0_DIV, 0x00018000);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00080001);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00088001);
	lcd_delay_us(10);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00080001);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VID_CLK0_CTRL2, 0x00000008);

	/*4. config phy clk*/
	/*done by edp init*/

	/*5. reset phy*/
	lcd_rst_clr_mask(RESETCTRL_RESET1_MASK,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_rst_clr_mask(RESETCTRL_RESET1_LEVEL,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_delay_us(1);
	lcd_rst_set_mask(RESETCTRL_RESET1_LEVEL,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_delay_us(10);

	lcd_combo_dphy_write(COMBO_DPHY_CNTL0, 0x2);
	lcd_combo_dphy_write(COMBO_DPHY_CNTL1, 0x55555555);

	/*6. config phy*/
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL1, 0x46770038);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL2, 0x0000ffff);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL3, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL4, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL5, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL6, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL10, 0x46770038);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL11, 0x0000ffff);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL12, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL13, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL15, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00406253);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0x0);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0x0);

	LCDPR(" lcd init pre\n");
}

static void lcd_venc_set_edp(void)
{
	unsigned int hactive = 1920;
	unsigned int vactive = 1080;
	unsigned int htotal = 2200;
	unsigned int vtotal = 1120;
	unsigned int hsw = 44;
	unsigned int hbp = 148;
	unsigned int vsw = 5;
	unsigned int vbp = 30;
	/*unsigned int pclk = 147840000;*/
	unsigned int data32;
	unsigned int offset = 0;

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 0);

	lcd_vcbus_write(ENCL_VIDEO_MODE + offset, 0x8000);
	lcd_vcbus_write(ENCL_VIDEO_MODE_ADV + offset, 0x0418);

	lcd_vcbus_write(ENCL_VIDEO_FILT_CTRL + offset, 0x1000);
	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset, htotal - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, vtotal - 1);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN + offset, hsw + hbp);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END + offset, hactive - 1 + hsw + hbp);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE + offset, vsw + vbp);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE + offset,
			vactive - 1 + vsw + vbp);

	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END + offset, hsw);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE + offset, vsw);
	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, 3); //yuv: 1, rgb: 3

	lcd_vcbus_write(ENCL_INBUF_CNTL1 + offset, (5 << 13) | (hactive - 1));
	lcd_vcbus_write(ENCL_INBUF_CNTL0 + offset, 0x200);

	/* default colorbar pattern */
	lcd_vcbus_write(ENCL_TST_MDSEL + offset, 1);
	lcd_vcbus_write(ENCL_TST_Y + offset, 0x200);
	lcd_vcbus_write(ENCL_TST_CB + offset, 0x200);
	lcd_vcbus_write(ENCL_TST_CR + offset, 0x200);
	lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, hsw + hbp);
	lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, 240);
	lcd_vcbus_write(ENCL_TST_EN + offset, 0);

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 1);

	/*select venc to edp*/
	data32 = (0 << 31) | (0 << 30) | (0 << 29) | (1 << 28);
	lcd_vcbus_write(VPU_DISP_VIU0_CTRL, data32);

	/*config venc_tcon*/
	offset = 0;
	lcd_vcbus_write(LCD_RGB_BASE_ADDR + offset, 0x0);
	lcd_vcbus_write(LCD_RGB_COEFF_ADDR + offset, 0x400);
	lcd_vcbus_write(LCD_POL_CNTL_ADDR + offset, (1 << 0));

	offset = 0;
	/* DE signal */
	lcd_vcbus_write(DE_HS_ADDR_T7 + offset, hsw + hbp);
	lcd_vcbus_write(DE_HE_ADDR_T7 + offset, hsw + hbp + hactive);
	lcd_vcbus_write(DE_VS_ADDR_T7 + offset, vsw + vbp);
	lcd_vcbus_write(DE_VE_ADDR_T7 + offset, vsw + vbp + vactive - 1);

	/* Hsync signal */
	lcd_vcbus_write(HSYNC_HS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(HSYNC_HE_ADDR_T7 + offset, hsw);
	lcd_vcbus_write(HSYNC_VS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(HSYNC_VE_ADDR_T7 + offset, vtotal - 1);

	/* Vsync signal */
	lcd_vcbus_write(VSYNC_HS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(VSYNC_HE_ADDR_T7 + offset, 0);
	lcd_vcbus_write(VSYNC_VS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(VSYNC_VE_ADDR_T7 + offset, vsw);

	/*select encl*/
	offset = 0;
	lcd_vcbus_write(VPU_VENC_CTRL + offset, 2);

	LCDPR(" lcd venc init\n");
}

static void mipi_dsi_power_init(void)
{
//#define PM_MIPI_DSI0     41
//#define PM_EDP0          48
//#define PM_EDP1          49
//#define PM_MIPI_DSI1     50
	pwr_ctrl_psci_smc(41, 1);
	pwr_ctrl_psci_smc(50, 1);

	LCDPR(" mipi-dsi power domain on\n");
}

static void mipi_dsi_reset(void)
{
	/* Level reset mail*/
	lcd_rst_setb(RESETCTRL_RESET1_MASK, 0, 30, 1);
	lcd_rst_setb(RESETCTRL_RESET1_LEVEL, 0, 30, 1);
	lcd_delay_us(1);
	lcd_rst_setb(RESETCTRL_RESET1_LEVEL, 1, 30, 1);

	lcd_rst_setb(RESETCTRL_RESET1_MASK, 0, 29, 1);
	lcd_rst_setb(RESETCTRL_RESET1_LEVEL, 0, 29, 1);
	lcd_delay_us(1);
	lcd_rst_setb(RESETCTRL_RESET1_LEVEL, 1, 29, 1);
}

static void mipi_dsi_tx_init(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	mipi_dsi_reset();

	lcd_drv->lcd_config->lcd_timing.bit_rate = 283500000;
	lcd_drv->lcd_config->lcd_control.mipi_config->clk_factor = 6;
	lcd_mipi_control_set(lcd_drv->lcd_config, 1);
}

static void lcd_init_pre_mipi_dsi(void)
{
	int cnt = 0, i;

	/*1.config pll*/
set_pll_retry_edp:
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x00b704bd);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x20b704bd);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x30b704bd);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1, 0x10000000);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x0000110c);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3, 0x10051400);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x000100c0);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x008300c0);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x34b704bd);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x14b704bd);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x0000300c);
	lcd_delay_us(100);
	i = 0;
	while (i++ < 200) {
		lcd_delay_us(50);
		if (lcd_ana_read(ANACTRL_TCON_PLL0_STS) & 0x80000000)
			break;
	}
	if (!(lcd_ana_read(ANACTRL_TCON_PLL0_STS) & 0x80000000)) {
		if (cnt++ < 20)
			goto set_pll_retry_edp;
		else
			LCDPR(" pll lock failed!!!\n");
	}

	/*2.config divider*/
	/*cntrl_clk_en0 disable*/
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 0, 19, 1);
	/* Disable the div output clock */
	/*clk_final_en disable ?*/
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0, 19, 1);
	/*set_preset disable ?*/
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0, 15, 1);

	/*used COMBO not VID*/
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 1, 18, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 1, 19, 1);

	/*3.config vclk*/
	lcd_clk_write(CLKCTRL_VIID_CLK0_DIV, 0x00000000);
	lcd_clk_write(CLKCTRL_VIID_CLK0_DIV, 0x00000005);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00080000);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK0_DIV, 0x00008005);
	lcd_clk_write(CLKCTRL_VIID_CLK0_DIV, 0x00018005);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00080001);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00088001);
	lcd_delay_us(10);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00080001);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VID_CLK0_CTRL2, 0x00000008);

	/* mipi-dsi phy clk*/
	lcd_clk_write(CLKCTRL_MIPIDSI_PHY_CLK_CTRL, 0x00000000);
	lcd_clk_write(CLKCTRL_MIPIDSI_PHY_CLK_CTRL, 0x00000100);
	/* mipi-dsi meas clk*/
	lcd_clk_write(CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL, 0x00000007);
	lcd_clk_write(CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL, 0x00000107);

	/* 4. reset phy*/
	lcd_rst_clr_mask(RESETCTRL_RESET1_MASK,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_rst_clr_mask(RESETCTRL_RESET1_LEVEL,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_delay_us(1);
	lcd_rst_set_mask(RESETCTRL_RESET1_LEVEL,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_delay_us(10);

	/* 5.select mipi-dsi*/
	lcd_combo_dphy_write(COMBO_DPHY_CNTL1, 0x0);

	/* 6. config phy*/
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL1, 0x822a0028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL2, 0x0000ffff);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL3, 0xc22a0028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL4, 0xc22a0028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL5, 0xc22a0028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL6, 0xc22a0028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL10, 0x822a0028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL11, 0x0000ffff);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL12, 0xc22a0028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL13, 0xc22a0028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, 0xc22a0028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL15, 0xc22a0028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x1e406243);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0xffff0000);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0xffff);

	LCDPR(" lcd init pre\n");
}

static void lcd_venc_set_mipi_dsi(void)
{
	unsigned int hactive = 1024;
	unsigned int vactive = 600;
	unsigned int htotal = 1250;
	unsigned int vtotal = 630;
	unsigned int hsw = 80;
	unsigned int hbp = 100;
	unsigned int vsw = 5;
	unsigned int vbp = 20;
	unsigned int data32;
	unsigned int offset = 0;

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 0);

	lcd_vcbus_write(ENCL_VIDEO_MODE + offset, 0x8000);
	lcd_vcbus_write(ENCL_VIDEO_MODE_ADV + offset, 0x0418);

	lcd_vcbus_write(ENCL_VIDEO_FILT_CTRL + offset, 0x1000);
	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset, htotal - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, vtotal - 1);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN + offset, hsw + hbp);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END + offset, hactive - 1 + hsw + hbp);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE + offset, vsw + vbp);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE + offset,
			vactive - 1 + vsw + vbp);

	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END + offset, hsw);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE + offset, vsw);
	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, 3); //yuv: 1, rgb: 3

	lcd_vcbus_write(ENCL_INBUF_CNTL1 + offset, (5 << 13) | (hactive - 1));
	lcd_vcbus_write(ENCL_INBUF_CNTL0 + offset, 0x200);

	/* default colorbar pattern */
	lcd_vcbus_write(ENCL_TST_MDSEL + offset, 1);
	lcd_vcbus_write(ENCL_TST_Y + offset, 0x200);
	lcd_vcbus_write(ENCL_TST_CB + offset, 0x200);
	lcd_vcbus_write(ENCL_TST_CR + offset, 0x200);
	lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, hsw + hbp);
	lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, 240);
	lcd_vcbus_write(ENCL_TST_EN + offset, 0);

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 1);

	/*select venc to mipi_dsi*/
	data32 = (0 << 31) | (0 << 30) | (0 << 29) | (1 << 28);
	lcd_vcbus_write(VPU_DISP_VIU0_CTRL, data32);

	/*config venc_tcon*/
	offset = 0;
	lcd_vcbus_write(LCD_RGB_BASE_ADDR + offset, 0x0);
	lcd_vcbus_write(LCD_RGB_COEFF_ADDR + offset, 0x400);
	lcd_vcbus_write(LCD_POL_CNTL_ADDR + offset, (1 << 0));

	offset = 0;
	/* DE signal */
	lcd_vcbus_write(DE_HS_ADDR_T7 + offset, hsw + hbp);
	lcd_vcbus_write(DE_HE_ADDR_T7 + offset, hsw + hbp + hactive);
	lcd_vcbus_write(DE_VS_ADDR_T7 + offset, vsw + vbp);
	lcd_vcbus_write(DE_VE_ADDR_T7 + offset, vsw + vbp + vactive - 1);

	/* Hsync signal */
	lcd_vcbus_write(HSYNC_HS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(HSYNC_HE_ADDR_T7 + offset, hsw);
	lcd_vcbus_write(HSYNC_VS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(HSYNC_VE_ADDR_T7 + offset, vtotal - 1);

	/* Vsync signal */
	lcd_vcbus_write(VSYNC_HS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(VSYNC_HE_ADDR_T7 + offset, 0);
	lcd_vcbus_write(VSYNC_VS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(VSYNC_VE_ADDR_T7 + offset, vsw);

	/*select encl*/
	offset = 0;
	lcd_vcbus_write(VPU_VENC_CTRL + offset, 2);

	LCDPR(" lcd venc init\n");
}

static void vx1_sw_reset(void)
{
	/* force PHY to 0 */
	lcd_combo_dphy_setb(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0, 3, 8, 2);
	lcd_vcbus_write(VBO_SOFT_RST_T7, 0x1ff);
	lcd_delay_us(5);
	/* realease PHY */
	lcd_combo_dphy_setb(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0, 0, 8, 2);
	lcd_vcbus_write(VBO_SOFT_RST_T7, 0);
}

#define VX1_HPD_WAIT_TIMEOUT    5000 /* 500ms */
static void vx1_wait_hpd(void)
{
	int i = 0;
	int ret = 1;

	LCDPR("%s:", __func__);
	while (i++ < VX1_HPD_WAIT_TIMEOUT) {
		ret = (lcd_vcbus_read(VBO_STATUS_L_T7) >> 6) & 1;
		if (ret == 0)
			break;
		lcd_delay_us(100);
	}

	if (ret) {
		LCDPR("hpd=%d\n",
		      ((lcd_vcbus_read(VBO_STATUS_L_T7) >> 6) & 1));
	} else {
		LCDPR("hpd=%d, i=%d\n",
		      ((lcd_vcbus_read(VBO_STATUS_L_T7) >> 6) & 1), i);
	}

	lcd_delay_ms(10); /* add 10ms delay for compatibility */
}

#define VX1_LOCKN_WAIT_TIMEOUT    500 /* 500ms */
static void vx1_wait_stable(void)
{
	int i = 0;
	int ret = 1;

	while (i++ < VX1_LOCKN_WAIT_TIMEOUT) {
		ret = lcd_vcbus_read(VBO_STATUS_L_T7) & 0x3f;
		if (ret == 0x20)
			break;
		mdelay(1);
	}
	LCDPR("%s status: 0x%x, i=%d\n", __func__,
	      lcd_vcbus_read(VBO_STATUS_L), i);
}

static void vx1_clk_util_set(void)
{
	/* set fifo_clk_sel*/
	lcd_combo_dphy_write(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0, (3 << 5));
	/* set cntl_ser_en:  8-channel to 1 */
	lcd_combo_dphy_setb(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0, 0xfff, 16, 8);

	/* decoupling fifo enable, gated clock enable */
	lcd_combo_dphy_write(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1,
			(1 << 6) | (1 << 0));
	/* decoupling fifo write enable after fifo enable */
	lcd_combo_dphy_setb(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1, 1, 7, 1);
}

static int vx1_lanes_set(void)
{
	unsigned int sublane_num, tmp;
	unsigned int region_size[4];

	sublane_num = 8 / 2; /* lane num in each region */
	lcd_vcbus_setb(VBO_LANES_T7, (8 - 1), 0, 3);
	lcd_vcbus_setb(VBO_LANES_T7, (2 - 1), 4, 2);
	lcd_vcbus_setb(VBO_LANES_T7, (sublane_num - 1), 8, 3);
	lcd_vcbus_setb(VBO_LANES_T7, (4 - 1), 11, 2);

	region_size[3] = (3840 / 8) * sublane_num;
	tmp = (3840 % 8);
	region_size[0] = region_size[3] + (((tmp / sublane_num) > 0) ?
		sublane_num : (tmp % sublane_num));
	region_size[1] = region_size[3] + (((tmp / sublane_num) > 1) ?
		sublane_num : (tmp % sublane_num));
	region_size[2] = region_size[3] + (((tmp / sublane_num) > 2) ?
		sublane_num : (tmp % sublane_num));
	lcd_vcbus_write(VBO_REGION_00_T7, region_size[0]);
	lcd_vcbus_write(VBO_REGION_01_T7, region_size[1]);
	lcd_vcbus_write(VBO_REGION_02_T7, region_size[2]);
	lcd_vcbus_write(VBO_REGION_03_T7, region_size[3]);

	lcd_vcbus_write(VBO_ACT_VSIZE_T7, 2160);
	/* different from FBC code!!! */
	/* lcd_vcbus_setb(VBO_CTRL_H_T7,0x80,11,5); */
	/* different from simulation code!!! */
	lcd_vcbus_setb(VBO_CTRL_H_T7, 0x0, 0, 4);
	lcd_vcbus_setb(VBO_CTRL_H_T7, 0x1, 9, 1);
	/* lcd_vcbus_setb(VBO_CTRL_L_T7,enable,0,1); */

	return 0;
}

static void vx1_sync_pol(int hsync_pol, int vsync_pol)
{
	lcd_vcbus_setb(VBO_VIN_CTRL_T7, hsync_pol, 4, 1);
	lcd_vcbus_setb(VBO_VIN_CTRL_T7, vsync_pol, 5, 1);

	lcd_vcbus_setb(VBO_VIN_CTRL_T7, hsync_pol, 6, 1);
	lcd_vcbus_setb(VBO_VIN_CTRL_T7, vsync_pol, 7, 1);
}

static void vx1_wait_timing_stable(void)
{
	unsigned int timing_state;
	int i = 200;

	timing_state = lcd_vcbus_read(VBO_INTR_STATE_T7) & 0x1ff;
	while ((timing_state) && (i > 0)) {
		/* clear video timing error intr */
		lcd_vcbus_setb(VBO_INTR_STATE_CTRL_T7, 0x7, 0, 3);
		lcd_vcbus_setb(VBO_INTR_STATE_CTRL_T7, 0, 0, 3);
		mdelay(2);
		timing_state = lcd_vcbus_read(VBO_INTR_STATE_T7) & 0x1ff;
		i--;
	};

	LCDPR("vbyone timing state: 0x%03x, i=%d\n",
		timing_state, (200 - i));

	mdelay(2);
}

static void vx1_control_set(void)
{
	int lane_count, byte_mode, region_num, hsize, vsize;
	/* int color_fmt; */
	int vin_color, vin_bpp;

	LCDPR("%s\n", __func__);

	hsize = 3840;
	vsize = 2160;
	lane_count = 8;
	region_num = 2;
	byte_mode = 4;

	vx1_clk_util_set();

	vin_color = 4; /* fixed RGB */
	vin_bpp   = 0; /* fixed 30bbp 4:4:4 */

	/* set Vbyone vin color format */
	lcd_vcbus_setb(VBO_VIN_CTRL_T7, vin_color, 8, 3);
	lcd_vcbus_setb(VBO_VIN_CTRL_T7, vin_bpp, 11, 2);

	vx1_lanes_set();
	/*set hsync/vsync polarity to let the polarity is low active
	 *inside the VbyOne
	 */
	vx1_sync_pol(0, 0);

	/* below line copy from simulation */
	/* gate the input when vsync asserted */
	lcd_vcbus_setb(VBO_VIN_CTRL_T7, 1, 0, 2);

	/* VBO_RGN_GEN clk always on */
	lcd_vcbus_setb(VBO_GCLK_MAIN_T7, 2, 2, 2);

	/* PAD select: */
	if (lane_count == 1 || lane_count == 2)
		lcd_vcbus_setb(LCD_PORT_SWAP_T7, 1, 9, 2);
	else if (lane_count == 4)
		lcd_vcbus_setb(LCD_PORT_SWAP_T7, 2, 9, 2);
	else
		lcd_vcbus_setb(LCD_PORT_SWAP_T7, 0, 9, 2);
	/* lcd_vcbus_setb(LCD_PORT_SWAP, 1, 8, 1);//reverse lane output order */

	/* Mux pads in combo-phy: 0 for dsi; 1 for lvds or vbyone; 2 for edp */
	//lcd_hiu_write(HHI_DSI_LVDS_EDP_CNTL0, 0x1);

	lcd_vcbus_setb(VBO_INSGN_CTRL_T7, 0, 2, 2);

	lcd_vcbus_setb(VBO_CTRL_L_T7, 1, 0, 1);

	vx1_wait_timing_stable();
	vx1_sw_reset();
}

static void vx1_tx_init(void)
{
	unsigned int data32;

	//Hot-plug GPIO
	data32 = lcd_periphs_read(PADCTRL_PIN_MUX_REGK);
	data32 &= (~(0xf << 8));
	data32 |= (0x4 << 8);
	lcd_periphs_write(PADCTRL_PIN_MUX_REGK, data32);

	vx1_control_set();
	vx1_wait_hpd();
	vx1_wait_stable();

	LCDPR(" vx1 tx finish\n");
}

static void lcd_init_pre_vx1(void)
{
	int cnt = 0, i;

	//1.config pll
set_pll_retry_vx1:
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x000f04f7);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x200f04f7);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x300f04f7);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1, 0x10110000);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00001108);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3, 0x10051400);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x010100c0);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x038300c0);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x340f04f7);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x140f04f7);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x0000300c);
	lcd_delay_us(100);

	i = 0;
	while (i++ < 200) {
		lcd_delay_us(50);
		if (lcd_ana_read(ANACTRL_TCON_PLL0_STS) & 0x80000000)
			break;
	}
	if (!(lcd_ana_read(ANACTRL_TCON_PLL0_STS) & 0x80000000)) {
		if (cnt++ < 20)
			goto set_pll_retry_vx1;
		else
			LCDPR(" pll lock failed!!!\n");
	}

	//2.config divider
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 0, 19, 1);
	/* Disable the div output clock */
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0, 19, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0, 15, 1);

	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0, 18, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0, 16, 2);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0, 15, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0, 0, 14);

	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 2, 16, 2);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 1, 15, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0x739c, 0, 15);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 0, 15, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL0_DIV, 1, 19, 1);

	//3.config vclk
	lcd_clk_write(CLKCTRL_VIID_CLK0_DIV, 0x00000000);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00080000);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK0_DIV, 0x00008000);
	lcd_clk_write(CLKCTRL_VIID_CLK0_DIV, 0x00018000);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00080001);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00088001);
	lcd_delay_us(10);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0x00080001);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VID_CLK0_CTRL2, 0x00000008);

	//4. reset phy
	lcd_rst_clr_mask(RESETCTRL_RESET1_MASK,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_rst_clr_mask(RESETCTRL_RESET1_LEVEL,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_delay_us(1);
	lcd_rst_set_mask(RESETCTRL_RESET1_LEVEL,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_delay_us(10);

	lcd_combo_dphy_write(COMBO_DPHY_CNTL0, 0x15);
	lcd_combo_dphy_write(COMBO_DPHY_CNTL1, 0x55555555);

	//6. config phy
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL1, 0x26430028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL2, 0x0000ffff);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL3, 0x26530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL4, 0x26530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL5, 0x26530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL6, 0x26530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL7, 0x26530028);//ch5
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL8, 0x26530028);//ch6
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL9, 0x26530028);//ch7
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL10, 0x0);//ch8
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL11, 0x0);//ch8 AUX
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL12, 0x0);//ch9
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL13, 0x0);//ch10
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, 0x0);//ch11
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL15, 0x0);//ch12
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL16, 0x0);//ch13
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL17, 0x0);//ch14
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00401648);//COMMON
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0x0);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0x0);

	LCDPR(" lcd init pre\n");
}

static void lcd_venc_set_vx1(void)
{
	unsigned int hactive = 3840;
	unsigned int vactive = 2160;
	unsigned int htotal = 4400;
	unsigned int vtotal = 2250;
	unsigned int hsw = 33;
	unsigned int hbp = 477;
	unsigned int vsw = 6;
	unsigned int vbp = 65;
	unsigned int data32;

	lcd_vcbus_write(ENCL_VIDEO_EN, 0);

	lcd_vcbus_write(ENCL_VIDEO_MODE, 0x8000);
	lcd_vcbus_write(ENCL_VIDEO_MODE_ADV, 0x0418);

	lcd_vcbus_write(ENCL_VIDEO_FILT_CTRL, 0x1000);
	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT, htotal - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT, vtotal - 1);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN, hsw + hbp);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END, hactive - 1 + hsw + hbp);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE, vsw + vbp);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE,
			vactive - 1 + vsw + vbp);

	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN, 0);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END, hsw);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE, vsw);
	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, 3); //yuv: 1, rgb: 3

	lcd_vcbus_write(ENCL_INBUF_CNTL1, (5 << 13) | (hactive - 1));
	lcd_vcbus_write(ENCL_INBUF_CNTL0, 0x200);

	/* default colorbar pattern */
	lcd_vcbus_write(ENCL_TST_MDSEL, 1);
	lcd_vcbus_write(ENCL_TST_Y, 0x200);
	lcd_vcbus_write(ENCL_TST_CB, 0x200);
	lcd_vcbus_write(ENCL_TST_CR, 0x200);
	lcd_vcbus_write(ENCL_TST_CLRBAR_STRT, hsw + hbp);
	lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH, 240);
	lcd_vcbus_write(ENCL_TST_EN, 0);

	lcd_vcbus_write(ENCL_VIDEO_EN, 1);
	//select venc to lvds
	data32 = (1 << 31) | (0 << 30) | (0 << 29) | (0 << 28);
	lcd_vcbus_write(VPU_DISP_VIU2_CTRL, data32);

	//config venc_tcon
	lcd_vcbus_write(LCD_RGB_BASE_ADDR, 0x0);
	lcd_vcbus_write(LCD_RGB_COEFF_ADDR, 0x400);

	/* DE signal */
	lcd_vcbus_write(DE_HS_ADDR_T7, hsw + hbp);
	lcd_vcbus_write(DE_HE_ADDR_T7, hsw + hbp + hactive);
	lcd_vcbus_write(DE_VS_ADDR_T7, vsw + vbp);
	lcd_vcbus_write(DE_VE_ADDR_T7, vsw + vbp + vactive - 1);

	/* Hsync signal */
	lcd_vcbus_write(HSYNC_HS_ADDR_T7, 0);
	lcd_vcbus_write(HSYNC_HE_ADDR_T7, hsw);
	lcd_vcbus_write(HSYNC_VS_ADDR_T7, 0);
	lcd_vcbus_write(HSYNC_VE_ADDR_T7, vtotal - 1);

	/* Vsync signal */
	lcd_vcbus_write(VSYNC_HS_ADDR_T7, 0);
	lcd_vcbus_write(VSYNC_HE_ADDR_T7, 0);
	lcd_vcbus_write(VSYNC_VS_ADDR_T7, 0);
	lcd_vcbus_write(VSYNC_VE_ADDR_T7, vsw);

	//select encl
	lcd_vcbus_write(VPU_VENC_CTRL, 2);

	LCDPR(" lcd venc init\n");
}

static void lcd_init_pre_lvds(void)
{
	int cnt = 0, i;

	//1.config pll
set_pll_retry:
	lcd_ana_write(ANACTRL_TCON_PLL2_CNTL0, 0x008e04ad);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL2_CNTL0, 0x208e04ad);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL2_CNTL0, 0x308e04ad);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL2_CNTL1, 0x10108000);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL2_CNTL2, 0x0000110c);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL2_CNTL3, 0x10051400);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL2_CNTL4, 0x000100c0);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL2_CNTL4, 0x008300c0);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL2_CNTL0, 0x348e04ad);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL2_CNTL0, 0x148e04ad);
	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_TCON_PLL2_CNTL2, 0x0000300c);
	lcd_delay_us(10);

	i = 0;
	while (i++ < 200) {
		lcd_delay_us(50);
		if (lcd_ana_read(ANACTRL_TCON_PLL2_STS) & 0x80000000)
			break;
	}
	if (!(lcd_ana_read(ANACTRL_TCON_PLL2_STS) & 0x80000000)) {
		if (cnt++ < 20)
			goto set_pll_retry;
		else
			LCDPR(" pll lock failed!!!\n");
	}

	//2.config divider
	lcd_clk_setb(CLKCTRL_VIID_CLK2_CTRL, 0, 19, 1);
	/* Disable the div output clock */
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL2_DIV, 0, 19, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL2_DIV, 0, 15, 1);

	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL2_DIV, 0, 18, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL2_DIV, 0, 16, 2);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL2_DIV, 0, 15, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL2_DIV, 0, 0, 14);

	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL2_DIV, 1, 16, 2);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL2_DIV, 1, 15, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL2_DIV, 0x3c78, 0, 15);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL2_DIV, 0, 15, 1);
	lcd_combo_dphy_setb(COMBO_DPHY_VID_PLL2_DIV, 1, 19, 1);

	//3.config vclk
	lcd_clk_write(CLKCTRL_VIID_CLK2_DIV, 0x00000000);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK2_CTRL, 0x00080000);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK2_DIV, 0x00008000);
	lcd_clk_write(CLKCTRL_VIID_CLK2_DIV, 0x00018000);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VIID_CLK2_CTRL, 0x00080001);
	lcd_clk_write(CLKCTRL_VIID_CLK2_CTRL, 0x00088001);
	lcd_delay_us(10);
	lcd_clk_write(CLKCTRL_VIID_CLK2_CTRL, 0x00080001);
	lcd_delay_us(5);
	lcd_clk_write(CLKCTRL_VID_CLK2_CTRL2, 0x00000008);

	//4. config phy clk
	//done by edp init

	//5. reset phy
	lcd_rst_clr_mask(RESETCTRL_RESET1_MASK,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_rst_clr_mask(RESETCTRL_RESET1_LEVEL,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_delay_us(1);
	lcd_rst_set_mask(RESETCTRL_RESET1_LEVEL,
			 ((0x1 << 20) | (0x1 << 19) | (0x1 << 7)));
	lcd_delay_us(10);

	lcd_combo_dphy_write(COMBO_DPHY_CNTL0, 0x10);
	lcd_combo_dphy_write(COMBO_DPHY_CNTL1, 0xaaaaaaaa);

	//6. config phy
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL1, 0xc6770038);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL2, 0x0100ffff);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL3, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL4, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL5, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL6, 0x16530028);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL7, 0x16530028);//ch5
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL8, 0x16530028);//ch6
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL9, 0x16530028);//ch7
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL10, 0x16430028);//ch8
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL11, 0x0100ffff);//ch8 AUX
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL12, 0x16530028);//ch9
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL13, 0x16530028);//ch10
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, 0x16530028);//ch11
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL15, 0x16530028);//ch12
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL16, 0x16530028);//ch13
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL17, 0x16530028);//ch14
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00406253);//COMMON
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0x0);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0x0);

	LCDPR(" lcd init pre\n");
}

static void lvds_init(void)
{
	unsigned int offset = 0x600;

	/* set fifo_clk_sel: div 7 */
	lcd_combo_dphy_write(COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0, (1 << 5));
	/* set cntl_ser_en:  8-channel to 1 */
	lcd_combo_dphy_setb(COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0, 0x3ff, 16, 10);

	/* decoupling fifo enable, gated clock enable */
	lcd_combo_dphy_write(COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1,
			(1 << 6) | (1 << 0));
	/* decoupling fifo write enable after fifo enable */
	lcd_combo_dphy_setb(COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1, 1, 7, 1);

	lcd_vcbus_write(LVDS_SER_EN_T7 + offset, 0xfff);
	lcd_vcbus_write(LVDS_PACK_CNTL_ADDR_T7 + offset,
			(1 << 0) | // repack //[1:0]
			(0 << 3) |	// reserve
			(0 << 4) |	// lsb first
			(0 << 5) |	// pn swap
			(1 << 6) |	// dual port
			(0 << 7) |	// use tcon control
			(1 << 8) |	// 0:10bits, 1:8bits, 2:6bits, 3:4bits.
			(0 << 10) |	//r_select  //0:R, 1:G, 2:B, 3:0
			(1 << 12) |	//g_select  //0:R, 1:G, 2:B, 3:0
			(2 << 14));	//b_select  //0:R, 1:G, 2:B, 3:0;

	lcd_vcbus_write(LVDS_GEN_CNTL_T7 + offset,
		(lcd_vcbus_read(LVDS_GEN_CNTL_T7 + offset) |
		 (1 << 4) | (0x3 << 0)));
	lcd_vcbus_setb(LVDS_GEN_CNTL_T7 + offset, 1, 3, 1);

	lcd_vcbus_write(P2P_CH_SWAP0_T7 + offset, 0x87643210);
	lcd_vcbus_write(P2P_CH_SWAP1_T7 + offset, 0x000000a9);
	LCDPR("CH swap: 0x%x=0x%x, 0x%x=0x%x\n",
		P2P_CH_SWAP0_T7 + offset,
		lcd_vcbus_read(P2P_CH_SWAP0_T7 + offset),
		P2P_CH_SWAP1_T7 + offset,
		lcd_vcbus_read(P2P_CH_SWAP1_T7 + offset));
	lcd_vcbus_write(P2P_BIT_REV_T7 + offset, 2);
	LCDPR("bit rev: 0x%x=0x%x\n",
		P2P_BIT_REV_T7 + offset,
		lcd_vcbus_read(P2P_BIT_REV_T7 + offset));

	LCDPR(" lvds init finish\n");
}

static void lcd_venc_set_lvds(void)
{
	unsigned int hactive = 1920;
	unsigned int vactive = 1080;
	unsigned int htotal = 2200;
	unsigned int vtotal = 1120;
	unsigned int hsw = 44;
	unsigned int hbp = 148;
	unsigned int vsw = 5;
	unsigned int vbp = 30;
	//unsigned int pclk = 147840000;
	unsigned int data32;
	unsigned int offset = 0x800;

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 0);

	lcd_vcbus_write(ENCL_VIDEO_MODE + offset, 0x8000);
	lcd_vcbus_write(ENCL_VIDEO_MODE_ADV + offset, 0x0418);

	lcd_vcbus_write(ENCL_VIDEO_FILT_CTRL + offset, 0x1000);
	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset, htotal - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, vtotal - 1);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN + offset, hsw + hbp);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END + offset, hactive - 1 + hsw + hbp);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE + offset, vsw + vbp);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE + offset,
			vactive - 1 + vsw + vbp);

	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END + offset, hsw);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE + offset, vsw);
	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, 3); //yuv: 1, rgb: 3

	lcd_vcbus_write(ENCL_INBUF_CNTL1 + offset, (5 << 13) | (hactive - 1));
	lcd_vcbus_write(ENCL_INBUF_CNTL0 + offset, 0x200);

	/* default colorbar pattern */
	lcd_vcbus_write(ENCL_TST_MDSEL + offset, 1);
	lcd_vcbus_write(ENCL_TST_Y + offset, 0x200);
	lcd_vcbus_write(ENCL_TST_CB + offset, 0x200);
	lcd_vcbus_write(ENCL_TST_CR + offset, 0x200);
	lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, hsw + hbp);
	lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, 240);
	lcd_vcbus_write(ENCL_TST_EN + offset, 0);

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 1);
	//select venc to lvds
	data32 = (1 << 31) | (0 << 30) | (0 << 29) | (0 << 28);
	lcd_vcbus_write(VPU_DISP_VIU2_CTRL, data32);

	//config venc_tcon
	offset = 0x200;
	lcd_vcbus_write(LCD_RGB_BASE_ADDR + offset, 0x0);
	lcd_vcbus_write(LCD_RGB_COEFF_ADDR + offset, 0x400);
	lcd_vcbus_write(LCD_POL_CNTL_ADDR + offset, (1 << 0));

	offset = 0x600;
	/* DE signal */
	lcd_vcbus_write(DE_HS_ADDR_T7 + offset, hsw + hbp);
	lcd_vcbus_write(DE_HE_ADDR_T7 + offset, hsw + hbp + hactive);
	lcd_vcbus_write(DE_VS_ADDR_T7 + offset, vsw + vbp);
	lcd_vcbus_write(DE_VE_ADDR_T7 + offset, vsw + vbp + vactive - 1);

	/* Hsync signal */
	lcd_vcbus_write(HSYNC_HS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(HSYNC_HE_ADDR_T7 + offset, hsw);
	lcd_vcbus_write(HSYNC_VS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(HSYNC_VE_ADDR_T7 + offset, vtotal - 1);

	/* Vsync signal */
	lcd_vcbus_write(VSYNC_HS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(VSYNC_HE_ADDR_T7 + offset, 0);
	lcd_vcbus_write(VSYNC_VS_ADDR_T7 + offset, 0);
	lcd_vcbus_write(VSYNC_VE_ADDR_T7 + offset, vsw);

	//select encl
	offset = 0x800;
	lcd_vcbus_write(VPU_VENC_CTRL + offset, 2);

	LCDPR(" lcd venc init\n");
}

void lcd_display_init_test(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	if (lcd_drv->lcd_config->lcd_basic.lcd_type == LCD_LVDS) {
		lcd_init_pre_lvds();
		lcd_venc_set_lvds();
		lvds_init();
		LCDPR("lvds init test done\n");
	} else if (lcd_drv->lcd_config->lcd_basic.lcd_type == LCD_EDP) {
		lcd_init_pre_edp();
		lcd_venc_set_edp();
		edp_power_init();
		edp_tx_init();
		LCDPR("edp init test done\n");
	} else if (lcd_drv->lcd_config->lcd_basic.lcd_type == LCD_MIPI) {
		lcd_init_pre_mipi_dsi();
		lcd_venc_set_mipi_dsi();
		mipi_dsi_power_init();
		mipi_dsi_tx_init();
		LCDPR("mipi_dsi init test done\n");
	} else if (lcd_drv->lcd_config->lcd_basic.lcd_type == LCD_VBYONE) {
		lcd_init_pre_vx1();
		lcd_venc_set_vx1();
		vx1_tx_init();
		LCDPR("vx1 init test done\n");
	}
}

void lcd_display_init_reg_dump(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	if (lcd_drv->lcd_config->lcd_basic.lcd_type == LCD_LVDS) {
		pr_info("pll regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			ANACTRL_TCON_PLL2_CNTL0,
			lcd_ana_read(ANACTRL_TCON_PLL2_CNTL0),
			ANACTRL_TCON_PLL2_CNTL1,
			lcd_ana_read(ANACTRL_TCON_PLL2_CNTL1),
			ANACTRL_TCON_PLL2_CNTL2,
			lcd_ana_read(ANACTRL_TCON_PLL2_CNTL2),
			ANACTRL_TCON_PLL2_CNTL3,
			lcd_ana_read(ANACTRL_TCON_PLL2_CNTL3),
			ANACTRL_TCON_PLL2_CNTL4,
			lcd_ana_read(ANACTRL_TCON_PLL2_CNTL4),
			ANACTRL_TCON_PLL2_STS,
			lcd_ana_read(ANACTRL_TCON_PLL2_STS));

		pr_info("clk regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			COMBO_DPHY_VID_PLL2_DIV,
			lcd_combo_dphy_read(COMBO_DPHY_VID_PLL2_DIV),
			CLKCTRL_VIID_CLK2_DIV,
			lcd_clk_read(CLKCTRL_VIID_CLK2_DIV),
			CLKCTRL_VIID_CLK2_CTRL,
			lcd_clk_read(CLKCTRL_VIID_CLK2_CTRL),
			CLKCTRL_VID_CLK2_CTRL2,
			lcd_clk_read(CLKCTRL_VID_CLK2_CTRL2));

		pr_info("lvds regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0,
			lcd_combo_dphy_read(COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0),
			COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1,
			lcd_combo_dphy_read(COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1),
			LVDS_SER_EN_T7 + 0x600,
			lcd_vcbus_read(LVDS_SER_EN_T7 + 0x600),
			LVDS_PACK_CNTL_ADDR_T7 + 0x600,
			lcd_vcbus_read(LVDS_PACK_CNTL_ADDR_T7 + 0x600),
			LVDS_GEN_CNTL_T7 + 0x600,
			lcd_vcbus_read(LVDS_GEN_CNTL_T7 + 0x600),
			P2P_CH_SWAP0_T7 + 0x600,
			lcd_vcbus_read(P2P_CH_SWAP0_T7 + 0x600),
			P2P_CH_SWAP1_T7 + 0x600,
			lcd_vcbus_read(P2P_CH_SWAP1_T7 + 0x600),
			P2P_BIT_REV_T7 + 0x600,
			lcd_vcbus_read(P2P_BIT_REV_T7 + 0x600));

		pr_info("venc regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			ENCL_VIDEO_EN + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_EN + 0x800),
			ENCL_VIDEO_MODE + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_MODE + 0x800),
			ENCL_VIDEO_MODE_ADV + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_MODE_ADV + 0x800),
			ENCL_VIDEO_FILT_CTRL + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_FILT_CTRL + 0x800),
			ENCL_VIDEO_MAX_PXCNT + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT + 0x800),
			ENCL_VIDEO_MAX_LNCNT + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT + 0x800),
			ENCL_VIDEO_HAVON_BEGIN + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN + 0x800),
			ENCL_VIDEO_HAVON_END + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_HAVON_END + 0x800),
			ENCL_VIDEO_VAVON_BLINE + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE + 0x800),
			ENCL_VIDEO_VAVON_ELINE + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE + 0x800),
			ENCL_VIDEO_HSO_BEGIN + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_HSO_BEGIN + 0x800),
			ENCL_VIDEO_HSO_END + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_HSO_END + 0x800),
			ENCL_VIDEO_VSO_BEGIN + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_VSO_BEGIN + 0x800),
			ENCL_VIDEO_VSO_END + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_VSO_END + 0x800),
			ENCL_VIDEO_VSO_BLINE + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_VSO_BLINE + 0x800),
			ENCL_VIDEO_VSO_ELINE + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_VSO_ELINE + 0x800),
			ENCL_VIDEO_RGBIN_CTRL + 0x800,
			lcd_vcbus_read(ENCL_VIDEO_RGBIN_CTRL + 0x800),
			ENCL_INBUF_CNTL1 + 0x800,
			lcd_vcbus_read(ENCL_INBUF_CNTL1 + 0x800),
			ENCL_INBUF_CNTL0 + 0x800,
			lcd_vcbus_read(ENCL_INBUF_CNTL0 + 0x800),
			VPU_DISP_VIU2_CTRL,
			lcd_vcbus_read(VPU_DISP_VIU2_CTRL),
			VPU_VENC_CTRL + 0x800,
			lcd_vcbus_read(VPU_VENC_CTRL + 0x800));
	} else if (lcd_drv->lcd_config->lcd_basic.lcd_type == LCD_EDP) {
		pr_info("pll regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			ANACTRL_TCON_PLL0_CNTL0,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL0),
			ANACTRL_TCON_PLL0_CNTL1,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL1),
			ANACTRL_TCON_PLL0_CNTL2,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2),
			ANACTRL_TCON_PLL0_CNTL3,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL3),
			ANACTRL_TCON_PLL0_CNTL4,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL4),
			ANACTRL_TCON_PLL0_STS,
			lcd_ana_read(ANACTRL_TCON_PLL0_STS));

		pr_info("clk regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			COMBO_DPHY_EDP_PIXEL_CLK_DIV,
			lcd_combo_dphy_read(COMBO_DPHY_EDP_PIXEL_CLK_DIV),
			COMBO_DPHY_VID_PLL0_DIV,
			lcd_combo_dphy_read(COMBO_DPHY_VID_PLL0_DIV),
			CLKCTRL_VIID_CLK0_DIV,
			lcd_clk_read(CLKCTRL_VIID_CLK0_DIV),
			CLKCTRL_VIID_CLK0_CTRL,
			lcd_clk_read(CLKCTRL_VIID_CLK0_CTRL),
			CLKCTRL_VID_CLK0_CTRL2,
			lcd_clk_read(CLKCTRL_VID_CLK0_CTRL2));

		pr_info("edp regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0,
			lcd_combo_dphy_read(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0),
			COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1,
			lcd_combo_dphy_read(COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1),
			EDP_TX_LINK_BW_SET,
			dptx_reg_read(EDP_TX_LINK_BW_SET),
			EDP_TX_LINK_COUNT_SET,
			dptx_reg_read(EDP_TX_LINK_COUNT_SET),
			EDP_TX_TRAINING_PATTERN_SET,
			dptx_reg_read(EDP_TX_TRAINING_PATTERN_SET),
			EDP_TX_SCRAMBLING_DISABLE,
			dptx_reg_read(EDP_TX_SCRAMBLING_DISABLE),
			EDP_TX_TRANSMITTER_OUTPUT_ENABLE,
			dptx_reg_read(EDP_TX_TRANSMITTER_OUTPUT_ENABLE),
			EDP_TX_MAIN_STREAM_ENABLE,
			dptx_reg_read(EDP_TX_MAIN_STREAM_ENABLE),
			EDP_TX_PHY_RESET,
			dptx_reg_read(EDP_TX_PHY_RESET),
			EDP_TX_PHY_STATUS,
			dptx_reg_read(EDP_TX_PHY_STATUS),
			EDP_TX_AUX_COMMAND,
			dptx_reg_read(EDP_TX_AUX_COMMAND),
			EDP_TX_AUX_ADDRESS,
			dptx_reg_read(EDP_TX_AUX_ADDRESS),
			EDP_TX_AUX_STATE,
			dptx_reg_read(EDP_TX_AUX_STATE),
			EDP_TX_AUX_REPLY_CODE,
			dptx_reg_read(EDP_TX_AUX_REPLY_CODE),
			EDP_TX_AUX_REPLY_COUNT,
			dptx_reg_read(EDP_TX_AUX_REPLY_COUNT),
			EDP_TX_AUX_REPLY_DATA_COUNT,
			dptx_reg_read(EDP_TX_AUX_REPLY_DATA_COUNT),
			EDP_TX_AUX_TRANSFER_STATUS,
			dptx_reg_read(EDP_TX_AUX_TRANSFER_STATUS));

		pr_info("venc regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			ENCL_VIDEO_EN,
			lcd_vcbus_read(ENCL_VIDEO_EN),
			ENCL_VIDEO_MODE,
			lcd_vcbus_read(ENCL_VIDEO_MODE),
			ENCL_VIDEO_MODE_ADV,
			lcd_vcbus_read(ENCL_VIDEO_MODE_ADV),
			ENCL_VIDEO_FILT_CTRL,
			lcd_vcbus_read(ENCL_VIDEO_FILT_CTRL),
			ENCL_VIDEO_MAX_PXCNT,
			lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT),
			ENCL_VIDEO_MAX_LNCNT,
			lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT),
			ENCL_VIDEO_HAVON_BEGIN,
			lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN),
			ENCL_VIDEO_HAVON_END,
			lcd_vcbus_read(ENCL_VIDEO_HAVON_END),
			ENCL_VIDEO_VAVON_BLINE,
			lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE),
			ENCL_VIDEO_VAVON_ELINE,
			lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE),
			ENCL_VIDEO_HSO_BEGIN,
			lcd_vcbus_read(ENCL_VIDEO_HSO_BEGIN),
			ENCL_VIDEO_HSO_END,
			lcd_vcbus_read(ENCL_VIDEO_HSO_END),
			ENCL_VIDEO_VSO_BEGIN,
			lcd_vcbus_read(ENCL_VIDEO_VSO_BEGIN),
			ENCL_VIDEO_VSO_END,
			lcd_vcbus_read(ENCL_VIDEO_VSO_END),
			ENCL_VIDEO_VSO_BLINE,
			lcd_vcbus_read(ENCL_VIDEO_VSO_BLINE),
			ENCL_VIDEO_VSO_ELINE,
			lcd_vcbus_read(ENCL_VIDEO_VSO_ELINE),
			ENCL_VIDEO_RGBIN_CTRL,
			lcd_vcbus_read(ENCL_VIDEO_RGBIN_CTRL),
			ENCL_INBUF_CNTL1,
			lcd_vcbus_read(ENCL_INBUF_CNTL1),
			ENCL_INBUF_CNTL0,
			lcd_vcbus_read(ENCL_INBUF_CNTL0),
			VPU_DISP_VIU0_CTRL,
			lcd_vcbus_read(VPU_DISP_VIU0_CTRL),
			VPU_VENC_CTRL,
			lcd_vcbus_read(VPU_VENC_CTRL));
	} else if (lcd_drv->lcd_config->lcd_basic.lcd_type == LCD_MIPI) {
		pr_info("pll regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			ANACTRL_TCON_PLL0_CNTL0,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL0),
			ANACTRL_TCON_PLL0_CNTL1,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL1),
			ANACTRL_TCON_PLL0_CNTL2,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2),
			ANACTRL_TCON_PLL0_CNTL3,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL3),
			ANACTRL_TCON_PLL0_CNTL4,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL4),
			ANACTRL_TCON_PLL0_STS,
			lcd_ana_read(ANACTRL_TCON_PLL0_STS));

		pr_info("clk regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			CLKCTRL_MIPIDSI_PHY_CLK_CTRL,
			lcd_clk_read(CLKCTRL_MIPIDSI_PHY_CLK_CTRL),
			CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL,
			lcd_clk_read(CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL),
			COMBO_DPHY_VID_PLL0_DIV,
			lcd_combo_dphy_read(COMBO_DPHY_VID_PLL0_DIV),
			CLKCTRL_VIID_CLK0_DIV,
			lcd_clk_read(CLKCTRL_VIID_CLK0_DIV),
			CLKCTRL_VIID_CLK0_CTRL,
			lcd_clk_read(CLKCTRL_VIID_CLK0_CTRL),
			CLKCTRL_VID_CLK0_CTRL2,
			lcd_clk_read(CLKCTRL_VID_CLK0_CTRL2));

		pr_info("mipi-dsi host regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			MIPI_DSI_TOP_CNTL,
			dsi_host_read(MIPI_DSI_TOP_CNTL),
			MIPI_DSI_TOP_SW_RESET,
			dsi_host_read(MIPI_DSI_TOP_SW_RESET),
			MIPI_DSI_TOP_CLK_CNTL,
			dsi_host_read(MIPI_DSI_TOP_CLK_CNTL),
			MIPI_DSI_TOP_MEM_PD,
			dsi_host_read(MIPI_DSI_TOP_MEM_PD),
			MIPI_DSI_TOP_INTR_CNTL_STAT,
			dsi_host_read(MIPI_DSI_TOP_INTR_CNTL_STAT),
			MIPI_DSI_TOP_MEAS_CNTL,
			dsi_host_read(MIPI_DSI_TOP_MEAS_CNTL));
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			MIPI_DSI_DWC_PHY_TST_CTRL0_OS,
			dsi_host_read(MIPI_DSI_DWC_PHY_TST_CTRL0_OS),
			MIPI_DSI_DWC_PHY_TST_CTRL1_OS,
			dsi_host_read(MIPI_DSI_DWC_PHY_TST_CTRL1_OS),
			MIPI_DSI_DWC_PHY_RSTZ_OS,
			dsi_host_read(MIPI_DSI_DWC_PHY_RSTZ_OS),
			MIPI_DSI_DWC_PHY_IF_CFG_OS,
			dsi_host_read(MIPI_DSI_DWC_PHY_IF_CFG_OS),
			MIPI_DSI_DWC_PHY_TMR_CFG_OS,
			dsi_host_read(MIPI_DSI_DWC_PHY_TMR_CFG_OS),
			MIPI_DSI_DWC_PHY_TMR_LPCLK_CFG_OS,
			dsi_host_read(MIPI_DSI_DWC_PHY_TMR_LPCLK_CFG_OS),
			MIPI_DSI_DWC_CMD_MODE_CFG_OS,
			dsi_host_read(MIPI_DSI_DWC_CMD_MODE_CFG_OS),
			MIPI_DSI_DWC_PCKHDL_CFG_OS,
			dsi_host_read(MIPI_DSI_DWC_PCKHDL_CFG_OS),
			MIPI_DSI_DWC_DPI_COLOR_CODING_OS,
			dsi_host_read(MIPI_DSI_DWC_DPI_COLOR_CODING_OS),
			MIPI_DSI_DWC_DPI_CFG_POL_OS,
			dsi_host_read(MIPI_DSI_DWC_DPI_CFG_POL_OS),
			MIPI_DSI_DWC_VID_MODE_CFG_OS,
			dsi_host_read(MIPI_DSI_DWC_VID_MODE_CFG_OS),
			MIPI_DSI_DWC_DPI_LP_CMD_TIM_OS,
			dsi_host_read(MIPI_DSI_DWC_DPI_LP_CMD_TIM_OS),
			MIPI_DSI_DWC_CLKMGR_CFG_OS,
			dsi_host_read(MIPI_DSI_DWC_CLKMGR_CFG_OS),
			MIPI_DSI_DWC_MODE_CFG_OS,
			dsi_host_read(MIPI_DSI_DWC_MODE_CFG_OS));
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			MIPI_DSI_DWC_VID_PKT_SIZE_OS,
			dsi_host_read(MIPI_DSI_DWC_VID_PKT_SIZE_OS),
			MIPI_DSI_DWC_VID_NUM_CHUNKS_OS,
			dsi_host_read(MIPI_DSI_DWC_VID_NUM_CHUNKS_OS),
			MIPI_DSI_DWC_VID_NULL_SIZE_OS,
			dsi_host_read(MIPI_DSI_DWC_VID_NULL_SIZE_OS),
			MIPI_DSI_DWC_VID_HLINE_TIME_OS,
			dsi_host_read(MIPI_DSI_DWC_VID_HLINE_TIME_OS),
			MIPI_DSI_DWC_VID_HSA_TIME_OS,
			dsi_host_read(MIPI_DSI_DWC_VID_HSA_TIME_OS),
			MIPI_DSI_DWC_VID_HBP_TIME_OS,
			dsi_host_read(MIPI_DSI_DWC_VID_HBP_TIME_OS),
			MIPI_DSI_DWC_VID_VSA_LINES_OS,
			dsi_host_read(MIPI_DSI_DWC_VID_VSA_LINES_OS),
			MIPI_DSI_DWC_VID_VBP_LINES_OS,
			dsi_host_read(MIPI_DSI_DWC_VID_VBP_LINES_OS),
			MIPI_DSI_DWC_VID_VFP_LINES_OS,
			dsi_host_read(MIPI_DSI_DWC_VID_VFP_LINES_OS),
			MIPI_DSI_DWC_VID_VACTIVE_LINES_OS,
			dsi_host_read(MIPI_DSI_DWC_VID_VACTIVE_LINES_OS));

		pr_info("mipi-dsi phy regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			MIPI_DSI_PHY_CTRL,
			dsi_phy_read(MIPI_DSI_PHY_CTRL),
			MIPI_DSI_CLK_TIM,
			dsi_phy_read(MIPI_DSI_CLK_TIM),
			MIPI_DSI_CLK_TIM1,
			dsi_phy_read(MIPI_DSI_CLK_TIM1),
			MIPI_DSI_HS_TIM,
			dsi_phy_read(MIPI_DSI_HS_TIM),
			MIPI_DSI_LP_TIM,
			dsi_phy_read(MIPI_DSI_LP_TIM),
			MIPI_DSI_ANA_UP_TIM,
			dsi_phy_read(MIPI_DSI_ANA_UP_TIM),
			MIPI_DSI_INIT_TIM,
			dsi_phy_read(MIPI_DSI_INIT_TIM),
			MIPI_DSI_WAKEUP_TIM,
			dsi_phy_read(MIPI_DSI_WAKEUP_TIM),
			MIPI_DSI_LPOK_TIM,
			dsi_phy_read(MIPI_DSI_LPOK_TIM),
			MIPI_DSI_ULPS_CHECK,
			dsi_phy_read(MIPI_DSI_ULPS_CHECK),
			MIPI_DSI_LP_WCHDOG,
			dsi_phy_read(MIPI_DSI_LP_WCHDOG),
			MIPI_DSI_TURN_WCHDOG,
			dsi_phy_read(MIPI_DSI_TURN_WCHDOG),
			MIPI_DSI_CHAN_CTRL,
			dsi_phy_read(MIPI_DSI_CHAN_CTRL),
			MIPI_DSI_PHY_CTRL,
			dsi_phy_read(MIPI_DSI_PHY_CTRL),
			MIPI_DSI_LP_WCHDOG,
			dsi_phy_read(MIPI_DSI_LP_WCHDOG));

		pr_info("venc regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			ENCL_VIDEO_EN,
			lcd_vcbus_read(ENCL_VIDEO_EN),
			ENCL_VIDEO_MODE,
			lcd_vcbus_read(ENCL_VIDEO_MODE),
			ENCL_VIDEO_MODE_ADV,
			lcd_vcbus_read(ENCL_VIDEO_MODE_ADV),
			ENCL_VIDEO_FILT_CTRL,
			lcd_vcbus_read(ENCL_VIDEO_FILT_CTRL),
			ENCL_VIDEO_MAX_PXCNT,
			lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT),
			ENCL_VIDEO_MAX_LNCNT,
			lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT),
			ENCL_VIDEO_HAVON_BEGIN,
			lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN),
			ENCL_VIDEO_HAVON_END,
			lcd_vcbus_read(ENCL_VIDEO_HAVON_END),
			ENCL_VIDEO_VAVON_BLINE,
			lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE),
			ENCL_VIDEO_VAVON_ELINE,
			lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE),
			ENCL_VIDEO_HSO_BEGIN,
			lcd_vcbus_read(ENCL_VIDEO_HSO_BEGIN),
			ENCL_VIDEO_HSO_END,
			lcd_vcbus_read(ENCL_VIDEO_HSO_END),
			ENCL_VIDEO_VSO_BEGIN,
			lcd_vcbus_read(ENCL_VIDEO_VSO_BEGIN),
			ENCL_VIDEO_VSO_END,
			lcd_vcbus_read(ENCL_VIDEO_VSO_END),
			ENCL_VIDEO_VSO_BLINE,
			lcd_vcbus_read(ENCL_VIDEO_VSO_BLINE),
			ENCL_VIDEO_VSO_ELINE,
			lcd_vcbus_read(ENCL_VIDEO_VSO_ELINE),
			ENCL_VIDEO_RGBIN_CTRL,
			lcd_vcbus_read(ENCL_VIDEO_RGBIN_CTRL),
			ENCL_INBUF_CNTL1,
			lcd_vcbus_read(ENCL_INBUF_CNTL1),
			ENCL_INBUF_CNTL0,
			lcd_vcbus_read(ENCL_INBUF_CNTL0),
			VPU_DISP_VIU0_CTRL,
			lcd_vcbus_read(VPU_DISP_VIU0_CTRL),
			VPU_VENC_CTRL,
			lcd_vcbus_read(VPU_VENC_CTRL));
	} else if (lcd_drv->lcd_config->lcd_basic.lcd_type == LCD_VBYONE) {
		pr_info("pll regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			ANACTRL_TCON_PLL0_CNTL0,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL0),
			ANACTRL_TCON_PLL0_CNTL1,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL1),
			ANACTRL_TCON_PLL0_CNTL2,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2),
			ANACTRL_TCON_PLL0_CNTL3,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL3),
			ANACTRL_TCON_PLL0_CNTL4,
			lcd_ana_read(ANACTRL_TCON_PLL0_CNTL4),
			ANACTRL_TCON_PLL0_STS,
			lcd_ana_read(ANACTRL_TCON_PLL0_STS));

		pr_info("clk regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			COMBO_DPHY_VID_PLL0_DIV,
			lcd_combo_dphy_read(COMBO_DPHY_VID_PLL0_DIV),
			CLKCTRL_VIID_CLK0_DIV,
			lcd_clk_read(CLKCTRL_VIID_CLK0_DIV),
			CLKCTRL_VIID_CLK0_CTRL,
			lcd_clk_read(CLKCTRL_VIID_CLK0_CTRL),
			CLKCTRL_VID_CLK0_CTRL2,
			lcd_clk_read(CLKCTRL_VID_CLK0_CTRL2));

		pr_info("vx1 regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			VBO_VIN_CTRL_T7,
			lcd_vcbus_read(VBO_VIN_CTRL_T7),
			VBO_STATUS_L_T7,
			lcd_vcbus_read(VBO_STATUS_L_T7),
			VBO_LANES_T7,
			lcd_vcbus_read(VBO_LANES_T7),
			VBO_REGION_00_T7,
			lcd_vcbus_read(VBO_REGION_00_T7),
			VBO_REGION_01_T7,
			lcd_vcbus_read(VBO_REGION_01_T7),
			VBO_REGION_02_T7,
			lcd_vcbus_read(VBO_REGION_02_T7),
			VBO_REGION_03_T7,
			lcd_vcbus_read(VBO_REGION_03_T7),
			VBO_ACT_VSIZE_T7,
			lcd_vcbus_read(VBO_ACT_VSIZE_T7),
			VBO_CTRL_H_T7,
			lcd_vcbus_read(VBO_CTRL_H_T7),
			VBO_INTR_STATE_CTRL_T7,
			lcd_vcbus_read(VBO_INTR_STATE_CTRL_T7),
			VBO_INTR_STATE_T7,
			lcd_vcbus_read(VBO_INTR_STATE_T7),
			VBO_GCLK_MAIN_T7,
			lcd_vcbus_read(VBO_GCLK_MAIN_T7),
			VBO_INSGN_CTRL_T7,
			lcd_vcbus_read(VBO_INSGN_CTRL_T7),
			VBO_CTRL_L_T7,
			lcd_vcbus_read(VBO_CTRL_L_T7));

		pr_info("venc regs:\n");
		pr_info("0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n"
			"0x%04x: 0x%08x\n",
			ENCL_VIDEO_EN,
			lcd_vcbus_read(ENCL_VIDEO_EN),
			ENCL_VIDEO_MODE,
			lcd_vcbus_read(ENCL_VIDEO_MODE),
			ENCL_VIDEO_MODE_ADV,
			lcd_vcbus_read(ENCL_VIDEO_MODE_ADV),
			ENCL_VIDEO_FILT_CTRL,
			lcd_vcbus_read(ENCL_VIDEO_FILT_CTRL),
			ENCL_VIDEO_MAX_PXCNT,
			lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT),
			ENCL_VIDEO_MAX_LNCNT,
			lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT),
			ENCL_VIDEO_HAVON_BEGIN,
			lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN),
			ENCL_VIDEO_HAVON_END,
			lcd_vcbus_read(ENCL_VIDEO_HAVON_END),
			ENCL_VIDEO_VAVON_BLINE,
			lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE),
			ENCL_VIDEO_VAVON_ELINE,
			lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE),
			ENCL_VIDEO_HSO_BEGIN,
			lcd_vcbus_read(ENCL_VIDEO_HSO_BEGIN),
			ENCL_VIDEO_HSO_END,
			lcd_vcbus_read(ENCL_VIDEO_HSO_END),
			ENCL_VIDEO_VSO_BEGIN,
			lcd_vcbus_read(ENCL_VIDEO_VSO_BEGIN),
			ENCL_VIDEO_VSO_END,
			lcd_vcbus_read(ENCL_VIDEO_VSO_END),
			ENCL_VIDEO_VSO_BLINE,
			lcd_vcbus_read(ENCL_VIDEO_VSO_BLINE),
			ENCL_VIDEO_VSO_ELINE,
			lcd_vcbus_read(ENCL_VIDEO_VSO_ELINE),
			ENCL_VIDEO_RGBIN_CTRL,
			lcd_vcbus_read(ENCL_VIDEO_RGBIN_CTRL),
			ENCL_INBUF_CNTL1,
			lcd_vcbus_read(ENCL_INBUF_CNTL1),
			ENCL_INBUF_CNTL0,
			lcd_vcbus_read(ENCL_INBUF_CNTL0),
			VPU_DISP_VIU0_CTRL,
			lcd_vcbus_read(VPU_DISP_VIU0_CTRL),
			VPU_VENC_CTRL,
			lcd_vcbus_read(VPU_VENC_CTRL));
	}
	pr_info("0x%04x: 0x%08x\n",
		VPU_VIU_VENC_MUX_CTRL,
		lcd_vcbus_read(VPU_VIU_VENC_MUX_CTRL));

	pr_info("phy regs:\n");
	pr_info("0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n"
		"0x%04x: 0x%08x\n",
		ANACTRL_DIF_PHY_CNTL1,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL1),
		ANACTRL_DIF_PHY_CNTL2,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL2),
		ANACTRL_DIF_PHY_CNTL3,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL3),
		ANACTRL_DIF_PHY_CNTL4,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL4),
		ANACTRL_DIF_PHY_CNTL5,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL5),
		ANACTRL_DIF_PHY_CNTL6,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL6),
		ANACTRL_DIF_PHY_CNTL7,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL7),
		ANACTRL_DIF_PHY_CNTL8,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL8),
		ANACTRL_DIF_PHY_CNTL9,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL9),
		ANACTRL_DIF_PHY_CNTL10,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL10),
		ANACTRL_DIF_PHY_CNTL11,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL11),
		ANACTRL_DIF_PHY_CNTL12,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL12),
		ANACTRL_DIF_PHY_CNTL13,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL13),
		ANACTRL_DIF_PHY_CNTL14,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL14),
		ANACTRL_DIF_PHY_CNTL15,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL15),
		ANACTRL_DIF_PHY_CNTL16,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL16),
		ANACTRL_DIF_PHY_CNTL17,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL17),
		ANACTRL_DIF_PHY_CNTL18,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL18),
		ANACTRL_DIF_PHY_CNTL19,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL19),
		ANACTRL_DIF_PHY_CNTL20,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL20),
		ANACTRL_DIF_PHY_CNTL21,
		lcd_ana_read(ANACTRL_DIF_PHY_CNTL21));
}

void lcd_edp_debug(void)
{
	dptx_dpcd_dump();
}
