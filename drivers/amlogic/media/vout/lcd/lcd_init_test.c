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
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_reg.h"
#include "lcd_common.h"

/*******************************************************************************
 * vpu/display init
 ******************************************************************************/
//#define PHY_PRBS
//#define EDP_INIT_TEST
#define LVDS_INIT_TEST

#ifdef LVDS_INIT_TEST
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
#endif

#if (defined EDP_INIT_TEST || defined LVDS_INIT_TEST)
static void lcd_venc_set(void)
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
#ifdef LVDS_INIT_TEST
	//select venc to lvds
	data32 = (1 << 31) | (0 << 30) | (0 << 29) | (0 << 28);
	lcd_vcbus_write(VPU_DISP_VIU2_CTRL, data32);

#endif

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
#endif

void lcd_display_init_test(void)
{
#ifdef LVDS_INIT_TEST
	lcd_init_pre_lvds();
#endif
#if (defined EDP_INIT_TEST || defined LVDS_INIT_TEST)
	lcd_venc_set();
#endif

#ifdef EDP_INIT_TEST
	edp_power_init();
	edp_tx_init();
#endif
#ifdef LVDS_INIT_TEST
	lvds_init();
#endif
	LCDPR("display init test done\n");
}

void lcd_display_init_reg_dump(void)
{
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
}
