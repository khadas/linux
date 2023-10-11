// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "common.h"
#include "mach_reg.h"
#include "reg_sc2.h"
#include "hw_clk.h"
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif

#define SET_CLK_MAX_TIMES 10
#define CLK_TOLERANCE 2 /* Unit: MHz */

/* local frac_rate flag */
static u32 frac_rate;

/*
 * HDMITX Clock configuration
 */

static inline int check_div(unsigned int div)
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

void hdmitx_set_sys_clk(struct hdmitx_dev *hdev, unsigned char flag)
{
	if (flag & 4)
		hdmitx_set_cts_sys_clk(hdev);

	if (flag & 2) {
		hdmitx_set_top_pclk(hdev);

		if (hdev->data->chip_type < MESON_CPU_ID_SC2)
			hd_set_reg_bits(P_HHI_GCLK_OTHER, 1, 17, 1);
	}
}

static void hdmitx_disable_encp_clk(struct hdmitx_dev *hdev)
{
	if (hdev->data->chip_type >= MESON_CPU_ID_SC2)
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_CTRL2, 0, 2, 1);
	else
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 0, 2, 1);
#ifdef CONFIG_AMLOGIC_VPU
	if (hdev->encp_vpu_dev) {
		vpu_dev_clk_gate_off(hdev->encp_vpu_dev);
		vpu_dev_mem_power_down(hdev->encp_vpu_dev);
	}
#endif
}

static void hdmitx_enable_encp_clk(struct hdmitx_dev *hdev)
{
#ifdef CONFIG_AMLOGIC_VPU
	if (hdev->encp_vpu_dev) {
		vpu_dev_clk_gate_on(hdev->encp_vpu_dev);
		vpu_dev_mem_power_on(hdev->encp_vpu_dev);
	}
#endif

	if (hdev->data->chip_type >= MESON_CPU_ID_SC2)
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_CTRL2, 1, 2, 1);
	else
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 1, 2, 1);
}

static void hdmitx_disable_enci_clk(struct hdmitx_dev *hdev)
{
	if (hdev->data->chip_type >= MESON_CPU_ID_SC2)
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_CTRL2, 0, 0, 1);
	else
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 0, 0, 1);
#ifdef CONFIG_AMLOGIC_VPU
	if (hdev->enci_vpu_dev) {
		vpu_dev_clk_gate_off(hdev->enci_vpu_dev);
		vpu_dev_mem_power_down(hdev->enci_vpu_dev);
	}
#endif
	if (hdev->hdmitx_clk_tree.venci_top_gate)
		clk_disable_unprepare(hdev->hdmitx_clk_tree.venci_top_gate);

	if (hdev->hdmitx_clk_tree.venci_0_gate)
		clk_disable_unprepare(hdev->hdmitx_clk_tree.venci_0_gate);

	if (hdev->hdmitx_clk_tree.venci_1_gate)
		clk_disable_unprepare(hdev->hdmitx_clk_tree.venci_1_gate);
}

static void hdmitx_enable_enci_clk(struct hdmitx_dev *hdev)
{
	if (hdev->hdmitx_clk_tree.venci_top_gate)
		clk_prepare_enable(hdev->hdmitx_clk_tree.venci_top_gate);

	if (hdev->hdmitx_clk_tree.venci_0_gate)
		clk_prepare_enable(hdev->hdmitx_clk_tree.venci_0_gate);

	if (hdev->hdmitx_clk_tree.venci_1_gate)
		clk_prepare_enable(hdev->hdmitx_clk_tree.venci_1_gate);

#ifdef CONFIG_AMLOGIC_VPU
	if (hdev->enci_vpu_dev) {
		vpu_dev_clk_gate_on(hdev->enci_vpu_dev);
		vpu_dev_mem_power_on(hdev->enci_vpu_dev);
	}
#endif

	if (hdev->data->chip_type >= MESON_CPU_ID_SC2)
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_CTRL2, 1, 0, 1);
	else
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 1, 0, 1);
}

static void hdmitx_disable_tx_pixel_clk(struct hdmitx_dev *hdev)
{
	if (hdev->data->chip_type >= MESON_CPU_ID_SC2)
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_CTRL2, 0, 5, 1);
	else
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 0, 5, 1);
}

void hdmitx_set_cts_sys_clk(struct hdmitx_dev *hdev)
{
	/* Enable cts_hdmitx_sys_clk */
	/* .clk0 ( cts_oscin_clk ), */
	/* .clk1 ( fclk_div4 ), */
	/* .clk2 ( fclk_div3 ), */
	/* .clk3 ( fclk_div5 ), */
	/* [10: 9] clk_sel. select cts_oscin_clk=24MHz */
	/* [	8] clk_en. Enable gated clock */
	/* [ 6: 0] clk_div. Divide by 1. = 24/1 = 24 MHz */
	if (hdev->data->chip_type >= MESON_CPU_ID_SC2) {
		hd_set_reg_bits(P_CLKCTRL_HDMI_CLK_CTRL, 0, 9, 3);
		hd_set_reg_bits(P_CLKCTRL_HDMI_CLK_CTRL, 0, 0, 7);
		hd_set_reg_bits(P_CLKCTRL_HDMI_CLK_CTRL, 1, 8, 1);
		return;
	}
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0, 9, 3);
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0, 0, 7);
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 1, 8, 1);
}

void hdmitx_set_top_pclk(struct hdmitx_dev *hdev)
{
	/* top hdmitx pixel clock */
	if (hdev->data->chip_type >= MESON_CPU_ID_SC2)
		hd_set_reg_bits(P_CLKCTRL_SYS_CLK_EN0_REG2, 1, 4, 1);
	else
		hd_set_reg_bits(P_HHI_GCLK_MPEG2, 1, 4, 1);
}

void hdmitx_set_cts_hdcp22_clk(struct hdmitx_dev *hdev)
{
	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_SC2:
		hd_write_reg(P_CLKCTRL_HDCP22_CLK_CTRL, 0x01000100);
		break;
	case MESON_CPU_ID_TXLX:
		/* Enable cts_hdcp22_skpclk */
		/* .clk0 ( cts_oscin_clk ), */
		/* .clk1 ( fclk_div4 ), */
		/* .clk2 ( fclk_div3 ), */
		/* .clk3 ( fclk_div5 ), */
		/* [26: 25] clk_sel. select cts_oscin_clk=24MHz */
		/* [	24] clk_en. Enable gated clock */
		/* [22: 16] clk_div. Divide by 1. = 24/1 = 24 MHz */
		WARN_ON(clk_set_rate(hdev->hdmitx_clk_tree.hdcp22_tx_skp,
				     24000000));
		clk_prepare_enable(hdev->hdmitx_clk_tree.hdcp22_tx_skp);

		/* Enable cts_hdcp22_esmclk */
		/* .clk0 ( fclk_div7 ), */
		/* .clk1 ( fclk_div4 ), */
		/* .clk2 ( fclk_div3 ), */
		/* .clk3 ( fclk_div5 ), */
		/* [10: 9] clk_sel. select fclk_div7*/
		/* [	8] clk_en. Enable gated clock */
		/* [ 6: 0] clk_div. Divide by 1.*/
		WARN_ON(clk_set_rate(hdev->hdmitx_clk_tree.hdcp22_tx_esm,
				     285714285));
		clk_prepare_enable(hdev->hdmitx_clk_tree.hdcp22_tx_esm);
	break;
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
	case MESON_CPU_ID_G12A:
	case MESON_CPU_ID_G12B:
	case MESON_CPU_ID_SM1:
	case MESON_CPU_ID_TM2:
	case MESON_CPU_ID_TM2B:
	default:
		hd_write_reg(P_HHI_HDCP22_CLK_CNTL, 0x01000100);
	break;
	}
}

void hdmitx_set_hdcp_pclk(struct hdmitx_dev *hdev)
{
	/* top hdcp pixel clock */
	if (hdev->data->chip_type >= MESON_CPU_ID_SC2)
		hd_set_reg_bits(P_CLKCTRL_SYS_CLK_EN0_REG2, 1, 3, 1);
	else
		hd_set_reg_bits(P_HHI_GCLK_MPEG2, 1, 3, 1);
}

void hdmitx_set_hdmi_axi_clk(struct hdmitx_dev *hdev)
{
	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_TM2:
	case MESON_CPU_ID_TM2B:
		if (hdev->hdmitx_clk_tree.cts_hdmi_axi_clk) {
			WARN_ON(clk_set_rate(hdev->hdmitx_clk_tree.cts_hdmi_axi_clk,
				667000000));
			clk_prepare_enable(hdev->hdmitx_clk_tree.cts_hdmi_axi_clk);
		}
		break;
	default:
		break;
	}
}

static void set_hpll_clk_out(unsigned int clk)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	pr_debug("config HPLL = %d frac_rate = %d\n", clk, frac_rate);

	switch (hdev->data->chip_type) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
	case MESON_CPU_ID_TXLX:
		set_gxl_hpll_clk_out(frac_rate, clk);
		break;
#endif
	case MESON_CPU_ID_G12A:
	case MESON_CPU_ID_G12B:
	case MESON_CPU_ID_SM1:
	case MESON_CPU_ID_TM2:
	case MESON_CPU_ID_TM2B:
		set_g12a_hpll_clk_out(frac_rate, clk);
		break;
	case MESON_CPU_ID_SC2:
		set_sc2_hpll_clk_out(frac_rate, clk);
	default:
		break;
	}

	pr_debug("config HPLL done\n");
}

/* HERE MUST BE BIT OPERATION!!! */
static void set_hpll_sspll(enum hdmi_vic vic)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	switch (hdev->data->chip_type) {
	case MESON_CPU_ID_G12A:
	case MESON_CPU_ID_G12B:
	case MESON_CPU_ID_SM1:
	case MESON_CPU_ID_TM2:
	case MESON_CPU_ID_TM2B:
		set_hpll_sspll_g12a(vic);
		break;
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case MESON_CPU_ID_GXBB:
		break;
	case MESON_CPU_ID_GXTVBB:
		break;
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
		set_hpll_sspll_gxl(vic);
		break;
#endif
	case MESON_CPU_ID_SC2:
		set_hpll_sspll_sc2(vic);
		break;
	default:
		break;
	}
}

static void set_hpll_od1(unsigned int div)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	switch (hdev->data->chip_type) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case MESON_CPU_ID_GXBB:
	case MESON_CPU_ID_GXTVBB:
		switch (div) {
		case 1:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 16, 2);
			break;
		case 2:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 16, 2);
			break;
		case 4:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 16, 2);
			break;
		case 8:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 3, 16, 2);
			break;
		default:
			break;
		}
		break;
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
		set_hpll_od1_gxl(div);
		break;
#endif
	case MESON_CPU_ID_G12A:
	case MESON_CPU_ID_G12B:
	case MESON_CPU_ID_SM1:
	case MESON_CPU_ID_TM2:
	case MESON_CPU_ID_TM2B:
		set_hpll_od1_g12a(div);
		break;
	case MESON_CPU_ID_SC2:
		set_hpll_od1_sc2(div);
		break;
	default:
		break;
	}
}

static void set_hpll_od2(unsigned int div)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	switch (hdev->data->chip_type) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case MESON_CPU_ID_GXBB:
	case MESON_CPU_ID_GXTVBB:
		switch (div) {
		case 1:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 22, 2);
			break;
		case 2:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 22, 2);
			break;
		case 4:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 22, 2);
			break;
		case 8:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 3, 22, 2);
			break;
		default:
			break;
		}
		break;
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
		set_hpll_od2_gxl(div);
		break;
#endif
	case MESON_CPU_ID_G12A:
	case MESON_CPU_ID_G12B:
	case MESON_CPU_ID_SM1:
	case MESON_CPU_ID_TM2:
	case MESON_CPU_ID_TM2B:
		set_hpll_od2_g12a(div);
		break;
	case MESON_CPU_ID_SC2:
		set_hpll_od2_sc2(div);
		break;
	default:
		break;
	}
}

static void set_hpll_od3(unsigned int div)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	switch (hdev->data->chip_type) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case MESON_CPU_ID_GXBB:
	case MESON_CPU_ID_GXTVBB:
		switch (div) {
		case 1:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 0, 18, 2);
			break;
		case 2:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 1, 18, 2);
			break;
		case 4:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 2, 18, 2);
			break;
		case 8:
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL2, 3, 18, 2);
			break;
		default:
			break;
		}
		break;
	case MESON_CPU_ID_GXL:
	case MESON_CPU_ID_GXM:
		set_hpll_od3_gxl(div);
		break;
#endif
	case MESON_CPU_ID_G12A:
	case MESON_CPU_ID_G12B:
	case MESON_CPU_ID_SM1:
		set_hpll_od3_g12a(div);
		break;
	case MESON_CPU_ID_SC2:
		set_hpll_od3_sc2(div);
		break;
	case MESON_CPU_ID_TM2:
	case MESON_CPU_ID_TM2B:
		set_hpll_od3_g12a(div);
		/* new added in TM2 */
		hd_set_reg_bits(P_HHI_LVDS_TX_PHY_CNTL1, 1, 29, 1);
		break;
	default:
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
	struct hdmitx_dev *hdev = get_hdmitx_device();
	unsigned int reg_vid_pll = P_HHI_VID_PLL_CLK_DIV;

	pr_debug("%s[%d] div = %d\n", __func__, __LINE__, div_sel);

	if (hdev->data->chip_type >= MESON_CPU_ID_SC2)
		reg_vid_pll = P_CLKCTRL_VID_PLL_CLK_DIV;

	/* Disable the output clock */
	hd_set_reg_bits(reg_vid_pll, 0, 18, 2);
	hd_set_reg_bits(reg_vid_pll, 0, 15, 1);

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
		hd_set_reg_bits(reg_vid_pll, 1, 18, 1);
	} else {
		hd_set_reg_bits(reg_vid_pll, 0, 18, 1);
		hd_set_reg_bits(reg_vid_pll, 0, 16, 2);
		hd_set_reg_bits(reg_vid_pll, 0, 15, 1);
		hd_set_reg_bits(reg_vid_pll, 0, 0, 15);

		hd_set_reg_bits(reg_vid_pll, shift_sel, 16, 2);
		hd_set_reg_bits(reg_vid_pll, 1, 15, 1);
		hd_set_reg_bits(reg_vid_pll, shift_val, 0, 15);
		hd_set_reg_bits(reg_vid_pll, 0, 15, 1);
	}
	/* Enable the final output clock */
	hd_set_reg_bits(reg_vid_pll, 1, 19, 1);
}

static void set_vid_clk_div(struct hdmitx_dev *hdev, unsigned int div)
{
	if (hdev->data->chip_type >= MESON_CPU_ID_SC2) {
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_CTRL, 0, 16, 3);
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_DIV, div - 1, 0, 8);
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_CTRL, 7, 0, 3);
	} else {
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 0, 16, 3);
		hd_set_reg_bits(P_HHI_VID_CLK_DIV, div - 1, 0, 8);
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 7, 0, 3);
	}
}

static void set_hdmi_tx_pixel_div(struct hdmitx_dev *hdev, unsigned int div)
{
	div = check_div(div);
	if (div == -1)
		return;
	if (hdev->data->chip_type >= MESON_CPU_ID_SC2) {
		hd_set_reg_bits(P_CLKCTRL_HDMI_CLK_CTRL, div, 16, 4);
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_CTRL2, 1, 5, 1);
	} else {
		hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, div, 16, 4);
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 1, 5, 1);
	}
}

static void set_encp_div(struct hdmitx_dev *hdev, unsigned int div)
{
	div = check_div(div);
	if (div == -1)
		return;
	if (hdev->data->chip_type >= MESON_CPU_ID_SC2) {
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_DIV, div, 24, 4);
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_CTRL, 1, 19, 1);
	} else {
		hd_set_reg_bits(P_HHI_VID_CLK_DIV, div, 24, 4);
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 1, 2, 1);
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 1, 19, 1);
	}
}

static void set_enci_div(struct hdmitx_dev *hdev, unsigned int div)
{
	div = check_div(div);
	if (div == -1)
		return;
	if (hdev->data->chip_type >= MESON_CPU_ID_SC2) {
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_DIV, div, 28, 4);
		hd_set_reg_bits(P_CLKCTRL_VID_CLK_CTRL, 1, 19, 1);
	} else {
		hd_set_reg_bits(P_HHI_VID_CLK_DIV, div, 28, 4);
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 1, 0, 1);
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 1, 19, 1);
	}
}

/* mode hpll_clk_out od1 od2(PHY) od3
 * vid_pll_div vid_clk_div hdmi_tx_pixel_div encp_div enci_div
 */
static struct hw_enc_clk_val_group setting_enc_clk_val_24[] = {
	{{HDMI_720x480i60_16x9, HDMI_720x480i60_4x3,
	  HDMI_720x576i50_16x9, HDMI_720x576i50_4x3,
	  HDMI_VIC_END},
		4324320, 4, 4, 1, VID_PLL_DIV_5, 1, 2, -1, 2},
	{{HDMI_720x576p50_16x9, HDMI_720x576p50_4x3,
	  HDMI_720x480p60_16x9, HDMI_720x480p60_4x3,
	  HDMI_VIC_END},
		4324320, 4, 4, 1, VID_PLL_DIV_5, 1, 2, 2, -1},
	{{HDMI_1280x720p50_16x9,
	  HDMI_1280x720p60_16x9,
	  HDMI_VIC_END},
		5940000, 4, 2, 1, VID_PLL_DIV_5, 1, 2, 2, -1},
	{{HDMI_1920x1080i60_16x9,
	  HDMI_1920x1080i50_16x9,
	  HDMI_VIC_END},
		5940000, 4, 2, 1, VID_PLL_DIV_5, 1, 2, 2, -1},
	{{HDMI_1920x1080p60_16x9,
	  HDMI_1920x1080p50_16x9,
	  HDMI_VIC_END},
		5940000, 4, 1, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_1920x1080p30_16x9,
	  HDMI_1920x1080p24_16x9,
	  HDMI_1920x1080p25_16x9,
	  HDMI_VIC_END},
		5940000, 4, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_2560x1080p50_64x27,
	  HDMI_VIC_END},
		3712500, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_2560x1080p60_64x27,
	  HDMI_VIC_END},
		3960000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_3840x2160p30_16x9,
	  HDMI_3840x2160p25_16x9,
	  HDMI_3840x2160p24_16x9,
	  HDMI_1920x1080p120_16x9,
	  HDMI_4096x2160p24_256x135,
	  HDMI_4096x2160p25_256x135,
	  HDMI_4096x2160p30_256x135,
	  HDMI_VIC_END},
		5940000, 2, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMI_3840x2160p60_16x9,
	  HDMI_3840x2160p50_16x9,
	  HDMI_4096x2160p60_256x135,
	  HDMI_4096x2160p50_256x135,
	  HDMI_VIC_END},
		5940000, 1, 1, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_4096x2160p60_256x135_Y420,
	  HDMI_4096x2160p50_256x135_Y420,
	  HDMI_3840x2160p60_16x9_Y420,
	  HDMI_3840x2160p50_16x9_Y420,
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
		4032000, 4, 2, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
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
	{{HDMIV_2048x1080p24hz,
	  HDMI_VIC_END},
		5940000, 4, 2, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
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
	{{HDMIV_3840x1080p60hz,
	  HDMI_VIC_END},
		5940000, 2, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
	{{HDMIV_2560x1440p60hz,
	  HDMI_VIC_END},
		4830000, 2, 1, 1, VID_PLL_DIV_5, 2, 1, 1, -1},
};

/* For colordepth 10bits */
static struct hw_enc_clk_val_group setting_enc_clk_val_30[] = {
	{{HDMI_720x480i60_16x9, HDMI_720x480i60_4x3,
	  HDMI_720x576i50_16x9, HDMI_720x576i50_4x3,
	  HDMI_VIC_END},
		5405400, 4, 4, 1, VID_PLL_DIV_6p25, 1, 2, -1, 2},
	{{HDMI_720x576p50_16x9, HDMI_720x576p50_4x3,
	  HDMI_720x480p60_16x9, HDMI_720x480p60_4x3,
	  HDMI_VIC_END},
		5405400, 4, 4, 1, VID_PLL_DIV_6p25, 1, 2, 2, -1},
	{{HDMI_1280x720p50_16x9,
	  HDMI_1280x720p60_16x9,
	  HDMI_VIC_END},
		3712500, 4, 1, 1, VID_PLL_DIV_6p25, 1, 2, 2, -1},
	{{HDMI_1920x1080i60_16x9,
	  HDMI_1920x1080i50_16x9,
	  HDMI_VIC_END},
		3712500, 4, 1, 1, VID_PLL_DIV_6p25, 1, 2, 2, -1},
	{{HDMI_1920x1080p60_16x9,
	  HDMI_1920x1080p50_16x9,
	  HDMI_VIC_END},
		3712500, 1, 2, 2, VID_PLL_DIV_6p25, 1, 1, 1, -1},
	{{HDMI_1920x1080p30_16x9,
	  HDMI_1920x1080p24_16x9,
	  HDMI_1920x1080p25_16x9,
	  HDMI_VIC_END},
		3712500, 2, 2, 2, VID_PLL_DIV_6p25, 1, 1, 1, -1},
	{{HDMI_2560x1080p50_64x27,
	  HDMI_VIC_END},
		4640625, 1, 2, 2, VID_PLL_DIV_6p25, 1, 1, 1, -1},
	{{HDMI_2560x1080p60_64x27,
	  HDMI_VIC_END},
		4950000, 1, 2, 2, VID_PLL_DIV_6p25, 1, 1, 1, -1},
	{{HDMI_4096x2160p60_256x135_Y420,
	  HDMI_4096x2160p50_256x135_Y420,
	  HDMI_3840x2160p60_16x9_Y420,
	  HDMI_3840x2160p50_16x9_Y420,
	  HDMI_VIC_END},
		3712500, 1, 1, 1, VID_PLL_DIV_6p25, 1, 2, 1, -1},
	{{HDMI_3840x2160p24_16x9,
	  HDMI_3840x2160p25_16x9,
	  HDMI_3840x2160p30_16x9,
	  HDMI_1920x1080p120_16x9,
	  HDMI_4096x2160p24_256x135,
	  HDMI_4096x2160p25_256x135,
	  HDMI_4096x2160p30_256x135,
	  HDMI_VIC_END},
		3712500, 1, 1, 1, VID_PLL_DIV_6p25, 1, 2, 2, -1},
	{{HDMI_VIC_FAKE,
	  HDMI_VIC_END},
		3450000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
};

/* For colordepth 12bits */
static struct hw_enc_clk_val_group setting_enc_clk_val_36[] = {
	{{HDMI_720x480i60_16x9, HDMI_720x480i60_4x3,
	  HDMI_720x576i50_16x9, HDMI_720x576i50_4x3,
	  HDMI_VIC_END},
		3243240, 2, 4, 1, VID_PLL_DIV_7p5, 1, 2, -1, 2},
	{{HDMI_720x576p50_16x9, HDMI_720x576p50_4x3,
	  HDMI_720x480p60_16x9, HDMI_720x480p60_4x3,
	  HDMI_VIC_END},
		3243240, 2, 4, 1, VID_PLL_DIV_7p5, 1, 2, 2, -1},
	{{HDMI_1280x720p50_16x9,
	  HDMI_1280x720p60_16x9,
	  HDMI_VIC_END},
		4455000, 4, 1, 1, VID_PLL_DIV_7p5, 1, 2, 2, -1},
	{{HDMI_1920x1080i60_16x9,
	  HDMI_1920x1080i50_16x9,
	  HDMI_VIC_END},
		4455000, 4, 1, 1, VID_PLL_DIV_7p5, 1, 2, 2, -1},
	{{HDMI_1920x1080p60_16x9,
	  HDMI_1920x1080p50_16x9,
	  HDMI_VIC_END},
		4455000, 1, 2, 2, VID_PLL_DIV_7p5, 1, 1, 1, -1},
	{{HDMI_2560x1080p50_64x27,
	  HDMI_VIC_END},
		5568750, 1, 2, 2, VID_PLL_DIV_7p5, 1, 1, 1, -1},
	{{HDMI_2560x1080p60_64x27,
	  HDMI_VIC_END},
		5940000, 1, 2, 2, VID_PLL_DIV_7p5, 1, 1, 1, -1},
	{{HDMI_1920x1080p30_16x9,
	  HDMI_1920x1080p24_16x9,
	  HDMI_1920x1080p25_16x9,
	  HDMI_VIC_END},
		4455000, 2, 2, 2, VID_PLL_DIV_7p5, 1, 1, 1, -1},
	{{HDMI_4096x2160p60_256x135_Y420,
	  HDMI_4096x2160p50_256x135_Y420,
	  HDMI_3840x2160p60_16x9_Y420,
	  HDMI_3840x2160p50_16x9_Y420,
	  HDMI_VIC_END},
		4455000, 1, 1, 2, VID_PLL_DIV_3p25, 1, 2, 1, -1},
	{{HDMI_3840x2160p24_16x9,
	  HDMI_3840x2160p25_16x9,
	  HDMI_3840x2160p30_16x9,
	  HDMI_1920x1080p120_16x9,
	  HDMI_4096x2160p24_256x135,
	  HDMI_4096x2160p25_256x135,
	  HDMI_4096x2160p30_256x135,
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
	{{HDMI_1920x1080p60_16x9,
	  HDMI_1920x1080p50_16x9,
	  HDMI_VIC_END},
		5940000, 2, 1, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	{{HDMI_1280x720p50_16x9,
	  HDMI_1280x720p60_16x9,
	  HDMI_1920x1080p30_16x9,
	  HDMI_1920x1080p24_16x9,
	  HDMI_1920x1080p25_16x9,
	  HDMI_VIC_END},
		5940000, 2, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
	/* NO 2160p mode*/
	{{HDMI_VIC_FAKE,
	  HDMI_VIC_END},
		3450000, 1, 2, 2, VID_PLL_DIV_5, 1, 1, 1, -1},
};

static void set_hdmitx_fe_clk(struct hdmitx_dev *hdev)
{
	unsigned int tmp = 0;
	enum hdmi_vic vic = hdev->cur_VIC;
	unsigned int vid_clk_cntl2 = P_HHI_VID_CLK_CNTL2;
	unsigned int vid_clk_div = P_HHI_VID_CLK_DIV;
	unsigned int hdmi_clk_cntl = P_HHI_HDMI_CLK_CNTL;

	if (hdev->data->chip_type < MESON_CPU_ID_TM2B)
		return;
	if (hdev->data->chip_type >= MESON_CPU_ID_SC2) {
		vid_clk_cntl2 = P_CLKCTRL_VID_CLK_CTRL2;
		vid_clk_div = P_CLKCTRL_VID_CLK_DIV;
		hdmi_clk_cntl = P_CLKCTRL_HDMI_CLK_CTRL;
	}

	hd_set_reg_bits(vid_clk_cntl2, 1, 9, 1);

	switch (vic) {
	case HDMI_720x480i60_16x9:
	case HDMI_720x576i50_16x9:
	case HDMI_720x480i60_4x3:
	case HDMI_720x576i50_4x3:
		tmp = (hd_read_reg(vid_clk_div) >> 28) & 0xf;
		break;
	default:
		tmp = (hd_read_reg(vid_clk_div) >> 24) & 0xf;
		break;
	}

	hd_set_reg_bits(hdmi_clk_cntl, tmp, 20, 4);
}

static void hdmitx_set_clk_(struct hdmitx_dev *hdev,
		struct hw_enc_clk_val_group *test_clk)
{
	int i = 0;
	int j = 0;
	struct hw_enc_clk_val_group *p_enc = NULL;
	enum hdmi_vic vic = hdev->cur_VIC;
	enum hdmi_color_space cs = hdev->para->cs;
	enum hdmi_color_depth cd = hdev->para->cd;

	if (!test_clk)
		return;

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
	*test_clk = p_enc[j];
	hdmitx_set_cts_sys_clk(hdev);
	set_hpll_clk_out(p_enc[j].hpll_clk_out);
	if (cd == COLORDEPTH_24B && hdev->sspll)
		set_hpll_sspll(vic);
	set_hpll_od1(p_enc[j].od1);
	set_hpll_od2(p_enc[j].od2);
	set_hpll_od3(p_enc[j].od3);
	set_hpll_od3_clk_div(p_enc[j].vid_pll_div);
	pr_debug("j = %d  vid_clk_div = %d\n", j, p_enc[j].vid_clk_div);
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
	para = hdmi_get_fmt_paras(vic);
	if (para && para->name && likely_frac_rate_mode(para->name)) {
		;
	} else {
		pr_info("this mode doesn't have frac_rate\n");
		frac_rate = 0;
	}

	pr_debug("frac_rate = %d\n", hdev->frac_rate_policy);
}

/*
 * calculate the pixel clock with current clock parameters
 * and measure the pixel clock from hardware clkmsr
 * then compare above 2 clocks
 */
static bool test_pixel_clk(struct hdmitx_dev *hdev, const struct hw_enc_clk_val_group *t)
{
	u32 idx;
	u32 calc_pixel_clk;
	u32 msr_pixel_clk;

	if (!hdev || !t)
		return 0;

	/* refer to meson-clk-measure.c, here can see that before SC2,
	 * the pixel index is 36, and since or after SC2, the index is 59
	 * the index may change in later chips
	 */
	idx = 59;

	/* calculate the pixel_clk firstly */
	calc_pixel_clk = t->hpll_clk_out;
	if (frac_rate)
		calc_pixel_clk = calc_pixel_clk - calc_pixel_clk / 1001;
	calc_pixel_clk /= (t->od1 > 0) ? t->od1 : 1;
	calc_pixel_clk /= (t->od2 > 0) ? t->od2 : 1;
	calc_pixel_clk /= (t->od3 > 0) ? t->od3 : 1;
	switch (t->vid_pll_div) {
	case VID_PLL_DIV_2:
		calc_pixel_clk /= 2;
		break;
	case VID_PLL_DIV_2p5:
		calc_pixel_clk = calc_pixel_clk * 2 / 5;
		break;
	case VID_PLL_DIV_3:
		calc_pixel_clk /= 3;
		break;
	case VID_PLL_DIV_3p25:
		calc_pixel_clk = calc_pixel_clk * 4 / 13;
		break;
	case VID_PLL_DIV_3p5:
		calc_pixel_clk = calc_pixel_clk * 2 / 7;
		break;
	case VID_PLL_DIV_3p75:
		calc_pixel_clk = calc_pixel_clk * 4 / 15;
		break;
	case VID_PLL_DIV_4:
		calc_pixel_clk /= 4;
		break;
	case VID_PLL_DIV_5:
		calc_pixel_clk /= 5;
		break;
	case VID_PLL_DIV_6:
		calc_pixel_clk /= 6;
		break;
	case VID_PLL_DIV_6p25:
		calc_pixel_clk = calc_pixel_clk * 4 / 25;
		break;
	case VID_PLL_DIV_7:
		calc_pixel_clk /= 7;
		break;
	case VID_PLL_DIV_7p5:
		calc_pixel_clk = calc_pixel_clk * 2 / 15;
		break;
	case VID_PLL_DIV_12:
		calc_pixel_clk /= 12;
		break;
	case VID_PLL_DIV_14:
		calc_pixel_clk /= 14;
		break;
	case VID_PLL_DIV_15:
		calc_pixel_clk /= 15;
		break;
	case VID_PLL_DIV_1:
	default:
		calc_pixel_clk /= 1;
		break;
	}
	calc_pixel_clk /= (t->vid_clk_div > 0) ? t->vid_clk_div : 1;
	calc_pixel_clk /= (t->hdmi_tx_pixel_div > 0) ? t->hdmi_tx_pixel_div : 1;

	/* measure the current HW pixel_clk */
	msr_pixel_clk = meson_clk_measure_with_precision(idx, 32);

	/* convert both unit to MHz and compare */
	calc_pixel_clk /= 1000;
	msr_pixel_clk /= 1000000;
	if (calc_pixel_clk == msr_pixel_clk)
		return 1;
	if (calc_pixel_clk > msr_pixel_clk && ((calc_pixel_clk - msr_pixel_clk) <= CLK_TOLERANCE))
		return 1;
	if (calc_pixel_clk < msr_pixel_clk && ((msr_pixel_clk - calc_pixel_clk) <= CLK_TOLERANCE))
		return 1;
	pr_info("calc_pixel_clk %dMHz msr_pixel_clk %dMHz\n", calc_pixel_clk, msr_pixel_clk);
	return 0;
}

void hdmitx_set_clk(struct hdmitx_dev *hdev)
{
	int i = 0;
	struct hw_enc_clk_val_group test_clks = {0};

	hdmitx_check_frac_rate(hdev);

	if (hdev->data->chip_type == MESON_CPU_ID_SC2) {
		/* set the clock and test the pixel clock */
		for (i = 0; i < SET_CLK_MAX_TIMES; i++) {
			hdmitx_set_clk_(hdev, &test_clks);
			if (test_pixel_clk(hdev, &test_clks))
				break;
		}
		if (i == SET_CLK_MAX_TIMES)
			pr_info("need check hdmitx clocks\n");
	} else {
		hdmitx_set_clk_(hdev, &test_clks);
	}
}

void hdmitx_disable_clk(struct hdmitx_dev *hdev)
{
	/* cts_encp/enci_clk */
	if (hdev->para->hdmitx_vinfo.viu_mux == VIU_MUX_ENCI)
		hdmitx_disable_enci_clk(hdev);
	else
		hdmitx_disable_encp_clk(hdev);

	/* hdmi_tx_pixel_clk */
	hdmitx_disable_tx_pixel_clk(hdev);
}

