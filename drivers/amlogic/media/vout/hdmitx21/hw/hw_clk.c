// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include "common.h"

#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif

/* local frac_rate flag */
static u32 frac_rate;

/*
 * HDMITX Clock configuration
 */

static inline int check_div(u32 div)
{
	if (div == -1)
		return -1;
	switch (div) {
	case 1:
		div = 0;
		break;
	case 2:
		div = 1;
		break;
	case 4:
		div = 2;
		break;
	case 6:
		div = 3;
		break;
	case 12:
		div = 4;
		break;
	default:
		break;
	}
	return div;
}

void hdmitx21_set_sys_clk(struct hdmitx_dev *hdev, u8 flag)
{
	if (flag & 4)
		hdmitx21_set_cts_sys_clk(hdev);

	if (flag & 2)
		hdmitx21_set_top_pclk(hdev);
}

static void hdmitx_disable_encp_clk(struct hdmitx_dev *hdev)
{
	//hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL2, 0, 2, 1);
}

static void hdmitx_enable_encp_clk(struct hdmitx_dev *hdev)
{
	//hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL2, 1, 2, 1);
}

static void hdmitx_disable_enci_clk(struct hdmitx_dev *hdev)
{
	//hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL2, 0, 0, 1);

	if (hdev->hdmitx_clk_tree.venci_top_gate)
		clk_disable_unprepare(hdev->hdmitx_clk_tree.venci_top_gate);

	if (hdev->hdmitx_clk_tree.venci_0_gate)
		clk_disable_unprepare(hdev->hdmitx_clk_tree.venci_0_gate);

	if (hdev->hdmitx_clk_tree.venci_1_gate)
		clk_disable_unprepare(hdev->hdmitx_clk_tree.venci_1_gate);
}

static void hdmitx_enable_enci_clk(struct hdmitx_dev *hdev)
{
}

static void hdmitx_disable_tx_pixel_clk(struct hdmitx_dev *hdev)
{
	//hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL2, 0, 5, 1);
}

void hdmitx21_set_cts_sys_clk(struct hdmitx_dev *hdev)
{
	/* Enable cts_hdmitx_sys_clk */
	/* .clk0 ( cts_oscin_clk ), */
	/* .clk1 ( fclk_div4 ), */
	/* .clk2 ( fclk_div3 ), */
	/* .clk3 ( fclk_div5 ), */
	/* [10: 9] clk_sel. select cts_oscin_clk=24MHz */
	/* [	8] clk_en. Enable gated clock */
	/* [ 6: 0] clk_div. Divide by 1. = 24/1 = 24 MHz */
	hd21_set_reg_bits(CLKCTRL_HDMI_CLK_CTRL, 0, 9, 3);
	hd21_set_reg_bits(CLKCTRL_HDMI_CLK_CTRL, 0, 0, 7);
	hd21_set_reg_bits(CLKCTRL_HDMI_CLK_CTRL, 1, 8, 1);
}

void hdmitx21_set_top_pclk(struct hdmitx_dev *hdev)
{
	/* top hdmitx pixel clock */
	hd21_set_reg_bits(CLKCTRL_SYS_CLK_EN0_REG2, 1, 4, 1);
}

void hdmitx21_set_cts_hdcp22_clk(struct hdmitx_dev *hdev)
{
	//hd21_write_reg(CLKCTRL_HDCP22_CLK_CTRL, 0x01000100);
}

void hdmitx21_set_hdcp_pclk(struct hdmitx_dev *hdev)
{
	/* top hdcp pixel clock */
	hd21_set_reg_bits(CLKCTRL_SYS_CLK_EN0_REG2, 1, 3, 1);
}

static void set_hpll_clk_out(u32 clk)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	pr_info("config HPLL = %d frac_rate = %d\n", clk, frac_rate);

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		set21_t7_hpll_clk_out(frac_rate, clk);
	default:
		break;
	}

	pr_info("config HPLL done\n");
}

/* HERE MUST BE BIT OPERATION!!! */
static void set_hpll_sspll(enum hdmi_vic vic)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
		set21_hpll_sspll_t7(vic);
		break;
	default:
		break;
	}
}

static void set_hpll_od1(u32 div)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
	default:
		set21_hpll_od1_t7(div);
		break;
	}
}

static void set_hpll_od2(u32 div)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
	default:
		set21_hpll_od2_t7(div);
		break;
	}
}

static void set_hpll_od3(u32 div)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_T7:
	default:
		set21_hpll_od3_t7(div);
		break;
	}
}

/* --------------------------------------------------
 *              clocks_set_vid_clk_div
 * --------------------------------------------------
 * wire            clk_final_en    = control[19];
 * wire            clk_div1        = control[18];
 * wire    [1:0]   clk_sel         = control[17:16];
 * wire            set_preset      = control[15];
 * wire    [14:0]  shift_preset    = control[14:0];
 */
static void set_hpll_od3_clk_div(int div_sel)
{
	int shift_val = 0;
	int shift_sel = 0;
	u32 reg_vid_pll = CLKCTRL_HDMI_VID_PLL_CLK_DIV;

	pr_info("%s[%d] div = %d\n", __func__, __LINE__, div_sel);

	/* Disable the output clock */
	hd21_set_reg_bits(reg_vid_pll, 0, 18, 2);
	hd21_set_reg_bits(reg_vid_pll, 0, 15, 1);

	switch (div_sel) {
	case VID_PLL_DIV_1:
		shift_val = 0xFFFF;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_2:
		shift_val = 0x0aaa;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_3:
		shift_val = 0x0db6;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_3p5:
		shift_val = 0x36cc;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_3p75:
		shift_val = 0x6666;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_4:
		shift_val = 0x0ccc;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_5:
		shift_val = 0x739c;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_6:
		shift_val = 0x0e38;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_6p25:
		shift_val = 0x0000;
		shift_sel = 3;
		break;
	case VID_PLL_DIV_7:
		shift_val = 0x3c78;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_7p5:
		shift_val = 0x78f0;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_12:
		shift_val = 0x0fc0;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_14:
		shift_val = 0x3f80;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_15:
		shift_val = 0x7f80;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_2p5:
		shift_val = 0x5294;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_3p25:
		shift_val = 0x66cc;
		shift_sel = 2;
		break;
	default:
		pr_info("Error: clocks_set_vid_clk_div:  Invalid parameter\n");
		break;
	}

	if (shift_val == 0xffff) {      /* if divide by 1 */
		hd21_set_reg_bits(reg_vid_pll, 1, 18, 1);
	} else {
		hd21_set_reg_bits(reg_vid_pll, 0, 18, 1);
		hd21_set_reg_bits(reg_vid_pll, 0, 16, 2);
		hd21_set_reg_bits(reg_vid_pll, 0, 15, 1);
		hd21_set_reg_bits(reg_vid_pll, 0, 0, 15);

		hd21_set_reg_bits(reg_vid_pll, shift_sel, 16, 2);
		hd21_set_reg_bits(reg_vid_pll, 1, 15, 1);
		hd21_set_reg_bits(reg_vid_pll, shift_val, 0, 15);
		hd21_set_reg_bits(reg_vid_pll, 0, 15, 1);
	}
	/* Enable the final output clock */
	hd21_set_reg_bits(reg_vid_pll, 1, 19, 1);
}

static void set_vid_clk_div(struct hdmitx_dev *hdev, u32 div)
{
}

static void set_hdmi_tx_pixel_div(struct hdmitx_dev *hdev, u32 div)
{
	div = check_div(div);
	if (div == -1)
		return;
}

static void set_encp_div(struct hdmitx_dev *hdev, u32 div)
{
	div = check_div(div);
	if (div == -1)
		return;
}

static void set_enci_div(struct hdmitx_dev *hdev, u32 div)
{
	div = check_div(div);
	if (div == -1)
		return;
}

/* mode hpll_clk_out od1 od2(PHY) od3
 * vid_pll_div vid_clk_div hdmi_tx_pixel_div encp_div enci_div
 */
static struct hw_enc_clk_val_group setting_enc_clk_val_24[] = {
	{{HDMI_7_720x480i60_16x9, HDMI_6_720x480i60_4x3,
	  HDMI_22_720x576i50_16x9, HDMI_21_720x576i50_4x3,
	  HDMI_VIC_END},
		4324320, 4, 4, 1, VID_PLL_DIV_5, 1, 2, -1, 2},
	{{HDMI_18_720x576p50_16x9, HDMI_17_720x576p50_4x3,
	  HDMI_3_720x480p60_16x9, HDMI_2_720x480p60_4x3,
	  HDMI_VIC_END},
		4324320, 4, 4, 1, VID_PLL_DIV_5, 1, 2, 2, -1},
	{{HDMI_19_1280x720p50_16x9,
	  HDMI_4_1280x720p60_16x9,
	  HDMI_VIC_END},
		5940000, 4, 2, 1, VID_PLL_DIV_5, 1, 2, 2, -1},
	{{HDMI_5_1920x1080i60_16x9,
	  HDMI_20_1920x1080i50_16x9,
	  HDMI_VIC_END},
		5940000, 4, 2, 1, VID_PLL_DIV_5, 1, 2, 2, -1},
	{{HDMI_16_1920x1080p60_16x9,
	  HDMI_31_1920x1080p50_16x9,
	  HDMI_VIC_END},
		5940000, 4, 1, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_34_1920x1080p30_16x9,
	  HDMI_32_1920x1080p24_16x9,
	  HDMI_33_1920x1080p25_16x9,
	  HDMI_VIC_END},
		5940000, 4, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_89_2560x1080p50_64x27,
	  HDMI_VIC_END},
		3712500, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_90_2560x1080p60_64x27,
	  HDMI_VIC_END},
		3960000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_95_3840x2160p30_16x9,
	  HDMI_94_3840x2160p25_16x9,
	  HDMI_93_3840x2160p24_16x9,
	  HDMI_63_1920x1080p120_16x9,
	  HDMI_98_4096x2160p24_256x135,
	  HDMI_99_4096x2160p25_256x135,
	  HDMI_100_4096x2160p30_256x135,
	  HDMI_VIC_END},
		5940000, 2, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMI_97_3840x2160p60_16x9,
	  HDMI_96_3840x2160p50_16x9,
	  HDMI_102_4096x2160p60_256x135,
	  HDMI_101_4096x2160p50_256x135,
	  HDMI_VIC_END},
		5940000, 1, 1, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_102_4096x2160p60_256x135,
	  HDMI_101_4096x2160p50_256x135,
	  HDMI_97_3840x2160p60_16x9,
	  HDMI_96_3840x2160p50_16x9,
	  HDMI_VIC_END},
		5940000, 2, 1, 1, VID_PLL_DIV_5, 1, 2, 1, -1},
	{{HDMI_VIC_FAKE,
	  HDMI_VIC_END},
		3450000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	/* pll setting for VESA modes */
	{{HDMIV_640x480p60hz, /* 4.028G / 16 = 251.75M */
	  HDMI_VIC_END},
		4028000, 4, 4, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_800x480p60hz,
	  HDMI_VIC_END},
		4761600, 4, 4, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_800x600p60hz,
	  HDMI_VIC_END},
		3200000, 4, 2, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_852x480p60hz,
	   HDMIV_854x480p60hz,
	  HDMI_VIC_END},
		4838400, 4, 4, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_1024x600p60hz,
	  HDMI_VIC_END},
		4032000, 4, 2, 1, VID_PLL_DIV_5, 1, 2, 2, -1},
	{{HDMIV_1024x768p60hz,
	  HDMI_VIC_END},
		5200000, 4, 2, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_1280x768p60hz,
	  HDMI_VIC_END},
		3180000, 4, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_1280x800p60hz,
	  HDMI_VIC_END},
		5680000, 4, 2, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_1152x864p75hz,
	  HDMIV_1280x960p60hz,
	  HDMIV_1280x1024p60hz,
	  HDMIV_1600x900p60hz,
	  HDMI_VIC_END},
		4320000, 4, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_1600x1200p60hz,
	  HDMI_VIC_END},
		3240000, 2, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_1360x768p60hz,
	  HDMIV_1366x768p60hz,
	  HDMI_VIC_END},
		3420000, 4, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_1400x1050p60hz,
	  HDMI_VIC_END},
		4870000, 4, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_1440x900p60hz,
	  HDMI_VIC_END},
		4260000, 4, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_1440x2560p60hz,
	  HDMI_VIC_END},
		4897000, 2, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_1680x1050p60hz,
	  HDMI_VIC_END},
		5850000, 4, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_1920x1200p60hz,
	  HDMI_VIC_END},
		3865000, 2, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_2160x1200p90hz,
	  HDMI_VIC_END},
		5371100, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMIV_2560x1600p60hz,
	  HDMI_VIC_END},
		3485000, 1, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_3440x1440p60hz,
	  HDMI_VIC_END},
		3197500, 1, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_2400x1200p90hz,
	  HDMI_VIC_END},
		5600000, 2, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
};

/* For colordepth 10bits */
static struct hw_enc_clk_val_group setting_enc_clk_val_30[] = {
	{{HDMI_7_720x480i60_16x9, HDMI_6_720x480i60_4x3,
	  HDMI_22_720x576i50_16x9, HDMI_21_720x576i50_4x3,
	  HDMI_VIC_END},
		5405400, 4, 4, 1, VID_PLL_DIV_6p25, 1, 2, -1, 2},
	{{HDMI_18_720x576p50_16x9, HDMI_17_720x576p50_4x3,
	  HDMI_3_720x480p60_16x9, HDMI_2_720x480p60_4x3,
	  HDMI_VIC_END},
		5405400, 4, 4, 1, VID_PLL_DIV_6p25, 1, 2, 2, -1},
	{{HDMI_19_1280x720p50_16x9,
	  HDMI_4_1280x720p60_16x9,
	  HDMI_VIC_END},
		3712500, 4, 1, 1, VID_PLL_DIV_6p25, 1, 2, 2, -1},
	{{HDMI_5_1920x1080i60_16x9,
	  HDMI_20_1920x1080i50_16x9,
	  HDMI_VIC_END},
		3712500, 4, 1, 1, VID_PLL_DIV_6p25, 1, 2, 2, -1},
	{{HDMI_16_1920x1080p60_16x9,
	  HDMI_31_1920x1080p50_16x9,
	  HDMI_VIC_END},
		3712500, 1, 2, 2, VID_PLL_DIV_6p25, 1, 1, 1, -1},
	{{HDMI_34_1920x1080p30_16x9,
	  HDMI_32_1920x1080p24_16x9,
	  HDMI_33_1920x1080p25_16x9,
	  HDMI_VIC_END},
		3712500, 2, 2, 2, VID_PLL_DIV_6p25, 1, 1, 1, -1},
	{{HDMI_89_2560x1080p50_64x27,
	  HDMI_VIC_END},
		4640625, 1, 2, 2, VID_PLL_DIV_6p25, 1, 1, 1, -1},
	{{HDMI_90_2560x1080p60_64x27,
	  HDMI_VIC_END},
		4950000, 1, 2, 2, VID_PLL_DIV_6p25, 1, 1, 1, -1},
	{{HDMI_102_4096x2160p60_256x135,
	  HDMI_101_4096x2160p50_256x135,
	  HDMI_97_3840x2160p60_16x9,
	  HDMI_96_3840x2160p50_16x9,
	  HDMI_VIC_END},
		3712500, 1, 1, 1, VID_PLL_DIV_6p25, 1, 2, 1, -1},
	{{HDMI_93_3840x2160p24_16x9,
	  HDMI_94_3840x2160p25_16x9,
	  HDMI_95_3840x2160p30_16x9,
	  HDMI_63_1920x1080p120_16x9,
	  HDMI_98_4096x2160p24_256x135,
	  HDMI_99_4096x2160p25_256x135,
	  HDMI_100_4096x2160p30_256x135,
	  HDMI_VIC_END},
		3712500, 1, 1, 1, VID_PLL_DIV_6p25, 1, 2, 2, -1},
	{{HDMI_VIC_FAKE,
	  HDMI_VIC_END},
		3450000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
};

/* For colordepth 12bits */
static struct hw_enc_clk_val_group setting_enc_clk_val_36[] = {
	{{HDMI_7_720x480i60_16x9, HDMI_6_720x480i60_4x3,
	  HDMI_22_720x576i50_16x9, HDMI_21_720x576i50_4x3,
	  HDMI_VIC_END},
		3243240, 2, 4, 1, VID_PLL_DIV_7p5, 1, 2, -1, 2},
	{{HDMI_18_720x576p50_16x9, HDMI_17_720x576p50_4x3,
	  HDMI_3_720x480p60_16x9, HDMI_2_720x480p60_4x3,
	  HDMI_VIC_END},
		3243240, 2, 4, 1, VID_PLL_DIV_7p5, 1, 2, 2, -1},
	{{HDMI_19_1280x720p50_16x9,
	  HDMI_4_1280x720p60_16x9,
	  HDMI_VIC_END},
		4455000, 4, 1, 1, VID_PLL_DIV_7p5, 1, 2, 2, -1},
	{{HDMI_5_1920x1080i60_16x9,
	  HDMI_20_1920x1080i50_16x9,
	  HDMI_VIC_END},
		4455000, 4, 1, 1, VID_PLL_DIV_7p5, 1, 2, 2, -1},
	{{HDMI_16_1920x1080p60_16x9,
	  HDMI_31_1920x1080p50_16x9,
	  HDMI_VIC_END},
		4455000, 1, 2, 2, VID_PLL_DIV_7p5, 1, 1, 1, -1},
	{{HDMI_89_2560x1080p50_64x27,
	  HDMI_VIC_END},
		5568750, 1, 2, 2, VID_PLL_DIV_7p5, 1, 1, 1, -1},
	{{HDMI_90_2560x1080p60_64x27,
	  HDMI_VIC_END},
		5940000, 1, 2, 2, VID_PLL_DIV_7p5, 1, 1, 1, -1},
	{{HDMI_34_1920x1080p30_16x9,
	  HDMI_32_1920x1080p24_16x9,
	  HDMI_33_1920x1080p25_16x9,
	  HDMI_VIC_END},
		4455000, 2, 2, 2, VID_PLL_DIV_7p5, 1, 1, 1, -1},
	{{HDMI_102_4096x2160p60_256x135,
	  HDMI_101_4096x2160p50_256x135,
	  HDMI_97_3840x2160p60_16x9,
	  HDMI_96_3840x2160p50_16x9,
	  HDMI_VIC_END},
		4455000, 1, 1, 2, VID_PLL_DIV_3p25, 1, 2, 1, -1},
	{{HDMI_93_3840x2160p24_16x9,
	  HDMI_94_3840x2160p25_16x9,
	  HDMI_95_3840x2160p30_16x9,
	  HDMI_63_1920x1080p120_16x9,
	  HDMI_98_4096x2160p24_256x135,
	  HDMI_99_4096x2160p25_256x135,
	  HDMI_100_4096x2160p30_256x135,
	  HDMI_VIC_END},
		4455000, 1, 1, 1, VID_PLL_DIV_7p5, 1, 2, 2, -1},
	{{HDMI_VIC_FAKE,
	  HDMI_VIC_END},
		3450000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
};

/* For 3D Frame Packing Clock Setting
 * mode hpll_clk_out od1 od2(PHY) od3
 * vid_pll_div vid_clk_div hdmi_tx_pixel_div encp_div enci_div
 */
static struct hw_enc_clk_val_group setting_3dfp_enc_clk_val[] = {
	{{HDMI_16_1920x1080p60_16x9,
	  HDMI_31_1920x1080p50_16x9,
	  HDMI_VIC_END},
		5940000, 2, 1, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_19_1280x720p50_16x9,
	  HDMI_4_1280x720p60_16x9,
	  HDMI_34_1920x1080p30_16x9,
	  HDMI_32_1920x1080p24_16x9,
	  HDMI_33_1920x1080p25_16x9,
	  HDMI_VIC_END},
		5940000, 2, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	/* NO 2160p mode*/
	{{HDMI_VIC_FAKE,
	  HDMI_VIC_END},
		3450000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
};

static void set_hdmitx_fe_clk(struct hdmitx_dev *hdev)
{
	u32 tmp = 0;
	enum hdmi_vic vic = hdev->cur_VIC;
	u32 vid_clk_cntl2 = CLKCTRL_VID_CLK0_CTRL2;
	u32 vid_clk_div = CLKCTRL_VID_CLK0_CTRL;
	u32 hdmi_clk_cntl = CLKCTRL_HDMI_CLK_CTRL;

	hd21_set_reg_bits(vid_clk_cntl2, 1, 9, 1);

	switch (vic) {
	case HDMI_7_720x480i60_16x9:
	case HDMI_22_720x576i50_16x9:
		tmp = (hd21_read_reg(vid_clk_div) >> 28) & 0xf;
		break;
	default:
		tmp = (hd21_read_reg(vid_clk_div) >> 24) & 0xf;
		break;
	}

	hd21_set_reg_bits(hdmi_clk_cntl, tmp, 20, 4);
}

static void hdmitx21_set_clk_(struct hdmitx_dev *hdev)
{
	int i = 0;
	int j = 0;
	struct hw_enc_clk_val_group *p_enc = NULL;
	enum hdmi_vic vic = hdev->cur_VIC;
	enum hdmi_color_space cs = hdev->para->cs;
	enum hdmi_color_depth cd = hdev->para->cd;

	/* YUV 422 always use 24B mode */
	if (cs == COLORSPACE_YUV422)
		cd = COLORDEPTH_24B;

	if (hdev->flag_3dfp) {
		p_enc = &setting_3dfp_enc_clk_val[0];
		for (j = 0; j < sizeof(setting_3dfp_enc_clk_val)
			/ sizeof(struct hw_enc_clk_val_group); j++) {
			for (i = 0; ((i < GROUP_MAX) && (p_enc[j].group[i]
				!= HDMI_VIC_END)); i++) {
				if (vic == p_enc[j].group[i])
					goto next;
			}
		}
		if (j == sizeof(setting_3dfp_enc_clk_val)
			/ sizeof(struct hw_enc_clk_val_group)) {
			pr_info("Not find VIC = %d for hpll setting\n", vic);
			return;
		}
	} else if (cd == COLORDEPTH_24B) {
		p_enc = &setting_enc_clk_val_24[0];
		for (j = 0; j < sizeof(setting_enc_clk_val_24)
			/ sizeof(struct hw_enc_clk_val_group); j++) {
			for (i = 0; ((i < GROUP_MAX) && (p_enc[j].group[i]
				!= HDMI_VIC_END)); i++) {
				if (vic == p_enc[j].group[i])
					goto next;
			}
		}
		if (j == sizeof(setting_enc_clk_val_24)
			/ sizeof(struct hw_enc_clk_val_group)) {
			pr_info("Not find VIC = %d for hpll setting\n", vic);
			return;
		}
	} else if (cd == COLORDEPTH_30B) {
		p_enc = &setting_enc_clk_val_30[0];
		for (j = 0; j < sizeof(setting_enc_clk_val_30)
			/ sizeof(struct hw_enc_clk_val_group); j++) {
			for (i = 0; ((i < GROUP_MAX) && (p_enc[j].group[i]
				!= HDMI_VIC_END)); i++) {
				if (vic == p_enc[j].group[i])
					goto next;
			}
		}
		if (j == sizeof(setting_enc_clk_val_30) /
			sizeof(struct hw_enc_clk_val_group)) {
			pr_info("Not find VIC = %d for hpll setting\n", vic);
			return;
		}
	} else if (cd == COLORDEPTH_36B) {
		p_enc = &setting_enc_clk_val_36[0];
		for (j = 0; j < sizeof(setting_enc_clk_val_36)
			/ sizeof(struct hw_enc_clk_val_group); j++) {
			for (i = 0; ((i < GROUP_MAX) && (p_enc[j].group[i]
				!= HDMI_VIC_END)); i++) {
				if (vic == p_enc[j].group[i])
					goto next;
			}
		}
		if (j == sizeof(setting_enc_clk_val_36) /
			sizeof(struct hw_enc_clk_val_group)) {
			pr_info("Not find VIC = %d for hpll setting\n", vic);
			return;
		}
	} else {
		pr_info("not support colordepth 48bits\n");
		return;
	}
next:
	hdmitx21_set_cts_sys_clk(hdev);
	set_hpll_clk_out(p_enc[j].hpll_clk_out);
	if (cd == COLORDEPTH_24B && hdev->sspll)
		set_hpll_sspll(vic);
	set_hpll_od1(p_enc[j].od1);
	set_hpll_od2(p_enc[j].od2);
	set_hpll_od3(p_enc[j].od3);
	set_hpll_od3_clk_div(p_enc[j].vid_pll_div);
	pr_info("j = %d  vid_clk_div = %d\n", j, p_enc[j].vid_clk_div);
	set_vid_clk_div(hdev, p_enc[j].vid_clk_div);
	set_hdmi_tx_pixel_div(hdev, p_enc[j].hdmi_tx_pixel_div);

	if (hdev->para->hdmitx_vinfo.viu_mux == VIU_MUX_ENCI) {
		set_enci_div(hdev, p_enc[j].enci_div);
		hdmitx_enable_enci_clk(hdev);
	} else {
		set_encp_div(hdev, p_enc[j].encp_div);
		hdmitx_enable_encp_clk(hdev);
	}
	set_hdmitx_fe_clk(hdev);
}

static int likely_frac_rate_mode(char *m)
{
	if (strstr(m, "24hz") || strstr(m, "30hz") || strstr(m, "60hz") ||
	    strstr(m, "120hz") || strstr(m, "240hz"))
		return 1;
	else
		return 0;
}

static void hdmitx_check_frac_rate(struct hdmitx_dev *hdev)
{
	enum hdmi_vic vic = hdev->cur_VIC;
	struct hdmi_format_para *para = NULL;

	frac_rate = hdev->frac_rate_policy;
	para = hdmi21_get_fmt_paras(vic);
	if (para && para->name && likely_frac_rate_mode(para->name)) {
		;
	} else {
		pr_info("this mode doesn't have frac_rate\n");
		frac_rate = 0;
	}

	pr_info("frac_rate = %d\n", hdev->frac_rate_policy);
}

void hdmitx21_set_clk(struct hdmitx_dev *hdev)
{
	hdmitx_check_frac_rate(hdev);

	hdmitx21_set_clk_(hdev);
}

void hdmitx21_disable_clk(struct hdmitx_dev *hdev)
{
	/* cts_encp/enci_clk */
	if (hdev->para->hdmitx_vinfo.viu_mux == VIU_MUX_ENCI)
		hdmitx_disable_enci_clk(hdev);
	else
		hdmitx_disable_encp_clk(hdev);

	/* hdmi_tx_pixel_clk */
	hdmitx_disable_tx_pixel_clk(hdev);
}

// --------------------------------------------------
//              clocks_set_vid_clk_div_for_hdmi
// --------------------------------------------------
// wire            clk_final_en    = control[19];
// wire            clk_div1        = control[18];
// wire    [1:0]   clk_sel         = control[17:16];
// wire            set_preset      = control[15];
// wire    [14:0]  shift_preset    = control[14:0];
static void clocks_set_vid_clk_div_for_hdmi(u32 div_sel)
{
	u32 shift_val = 0;
	u32 shift_sel = 0;

	pr_info("%s\n", __func__);
	// Disable the output clock
	hd21_set_reg_bits(CLKCTRL_HDMI_VID_PLL_CLK_DIV, 0, 19, 1);
	hd21_set_reg_bits(CLKCTRL_HDMI_VID_PLL_CLK_DIV, 0, 15, 1);
	// Select hdmi_vid_pll_clk to be vid_pll0_clk and vid_pll2_clk
	hd21_set_reg_bits(CLKCTRL_HDMI_VID_PLL_CLK_DIV, 1, 24, 1);

	switch (div_sel) {
	case VID_PLL_DIV_1:
		shift_val = 0xFFFF;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_2:
		shift_val = 0x0aaa;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_3:
		shift_val = 0x0db6;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_3p5:
		shift_val = 0x36cc;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_3p75:
		shift_val = 0x6666;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_4:
		shift_val = 0x0ccc;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_5:
		shift_val = 0x739c;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_6:
		shift_val = 0x0e38;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_6p25:
		shift_val = 0x0000;
		shift_sel = 3;
		break;
	case VID_PLL_DIV_7:
		shift_val = 0x3c78;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_7p5:
		shift_val = 0x78f0;
		shift_sel = 2;
		break;
	case VID_PLL_DIV_12:
		shift_val = 0x0fc0;
		shift_sel = 0;
		break;
	case VID_PLL_DIV_14:
		shift_val = 0x3f80;
		shift_sel = 1;
		break;
	case VID_PLL_DIV_15:
		shift_val = 0x7f80;
		shift_sel = 2;
		break;
	default:
		pr_info("Error: %s:  Invalid parameter\n", __func__);
		break;
	}

	if (shift_val == 0xffff) { // if divide by 1
		hd21_set_reg_bits(CLKCTRL_HDMI_VID_PLL_CLK_DIV, 1, 18, 1);
	} else {
		hd21_set_reg_bits(CLKCTRL_HDMI_VID_PLL_CLK_DIV, 0, 0, 18);
		hd21_set_reg_bits(CLKCTRL_HDMI_VID_PLL_CLK_DIV, shift_sel, 16, 2);
		hd21_set_reg_bits(CLKCTRL_HDMI_VID_PLL_CLK_DIV, 1, 15, 1);
		hd21_set_reg_bits(CLKCTRL_HDMI_VID_PLL_CLK_DIV, shift_val, 0, 15);
	}
	// Set the selector low
	hd21_set_reg_bits(CLKCTRL_HDMI_VID_PLL_CLK_DIV, 0, 15, 1);
	// Enable the final output clock
	hd21_set_reg_bits(CLKCTRL_HDMI_VID_PLL_CLK_DIV, 1, 19, 1);
}   // clocks_set_vid_clk_div_for_hdmi

//===============================================================
//  CRT_VIDEO SETTING FUNCTIONS
//===============================================================
static void set_crt_video_enc(u32 vidx, u32 in_sel, u32 div_n)
//input :
//vidx      : 0:V1; 1:V2;     there have 2 parallel set clock generator: V1 and V2
//in_sel     : 0:vid_pll_clk;  1:fclk_div4; 2:flck_div3; 3:fclk_div5;
//            4:vid_pll2_clk; 5:fclk_div7; 6:vid_pll2_clk;
//div_n      : clock divider for enci_clk/encp_clk/encl_clk/vda_clk/hdmi_tx_pixel_clk;
{
	if (vidx == 0) { //V1
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 0, 19, 1); //[19] -disable clk_div0
		udelay(2);
		// [18:16] - cntl_clk_in_sel
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, in_sel, 16, 3);
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_DIV, div_n - 1, 0, 8); // [7:0]   - cntl_xd0
		udelay(5);
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 1, 19, 1); //[19] -enable clk_div0
	} else { //V2
		hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 0, 19, 1); //[19] -disable clk_div0
		udelay(2);
		// [18:16] - cntl_clk_in_sel
		hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, in_sel, 16, 3);
		hd21_set_reg_bits(CLKCTRL_VIID_CLK0_DIV, div_n - 1, 0, 8); // [7:0]   - cntl_xd0
		udelay(5);
		hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 1, 19, 1); //[19] -enable clk_div0
	}
}

//===============================================================
//  CRT_VIDEO SETTING FUNCTIONS
//===============================================================
static void set_crt_video_enc2(u32 vidx, u32 in_sel, u32 div_n)
//input :
//vidx      : 0:V1; 1:V2;     there have 2 parallel set clock generator: V1 and V2
//in_sel     : 0:vid_pll_clk;  1:fclk_div4; 2:flck_div3; 3:fclk_div5;
//            4:vid_pll2_clk; 5:fclk_div7; 6:vid_pll2_clk;
//div_n      : clock divider for enci_clk/encp_clk/encl_clk/vda_clk/hdmi_tx_pixel_clk;
{
	if (vidx == 0) { //V1
		hd21_set_reg_bits(CLKCTRL_VID_CLK2_CTRL, 0, 19, 1); //[19] -disable clk_div0
		udelay(2);
		// [18:16] - cntl_clk_in_sel
		hd21_set_reg_bits(CLKCTRL_VID_CLK2_CTRL, in_sel, 16, 3);
		hd21_set_reg_bits(CLKCTRL_VID_CLK2_DIV, (div_n - 1), 0, 8); // [7:0]   - cntl_xd0
		udelay(5);
		hd21_set_reg_bits(CLKCTRL_VID_CLK2_CTRL, 1, 19, 1); //[19] -enable clk_div0
	} else { //V2
		hd21_set_reg_bits(CLKCTRL_VIID_CLK2_CTRL, 0, 19, 1); //[19] -disable clk_div0
		udelay(2);
		// [18:16] - cntl_clk_in_sel
		hd21_set_reg_bits(CLKCTRL_VIID_CLK2_CTRL, in_sel, 16, 3);
		// [7:0]   - cntl_xd0
		hd21_set_reg_bits(CLKCTRL_VIID_CLK2_DIV, (div_n - 1), 0, 8);
		udelay(5);
		hd21_set_reg_bits(CLKCTRL_VIID_CLK2_CTRL, 1, 19, 1); //[19] -enable clk_div0
	}
}

//=======================================================================================
// function: vclk_set_hdmitx
// clock setting for HDMI TX
// vformat: video format
// color_depth: 4 - 24-bit per pixel. Not for 422 video, always choose 4, even if the
// bit-per-pixel may be 30-bit.
//              5 - 30-bit per pixel
//              6 - 36-bit per pixel
// mode420: 0 - Non-420 mode;
//          1 - HDMI output 420 video, hdmi_phy_clk and tmds_clk are half of the usual speed.
void vclk_set_hdmitx(u32 vformat, u32 color_depth, u32 mode420)
{
	u32 vid_pll_clk_div;
	u32 xd;

	if (color_depth != 4 && color_depth != 5 && color_depth != 6) {
		pr_info("Error: %s sees illegal color_depth = %d!\n", __func__, color_depth);
		return;
	}

	if (mode420 != 0 && mode420 != 1) {
		pr_info("Error: %s sees illegal mode420 = %d!\n", __func__, mode420);
		return;
	}

	if (color_depth == 5)
		vid_pll_clk_div = VID_PLL_DIV_6p25;
	else if (color_depth == 6)
		vid_pll_clk_div = VID_PLL_DIV_7p5;
	else
		vid_pll_clk_div = VID_PLL_DIV_5;
	xd = 1;
	//configure vid_clk_div_top
	clocks_set_vid_clk_div_for_hdmi(vid_pll_clk_div);

	//configure crt_video V1: in_sel=vid_pll_clk(0),div_n=xd)
	if (vformat == TV_ENC_480i || vformat == TV_ENC_576i)
		set_crt_video_enc(0, 0, xd);
	else
		set_crt_video_enc2(0, 0, xd);
}   /* vclk_set_hdmitx */
