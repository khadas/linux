// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/printk.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/delay.h>
#include "common.h"

static int check_od(u8 od)
{
	if (od == 1 || od == 2 || od == 4 || od == 8)
		return 1;
	return 0;
}

const static char od_map[9] = {
	0, 0, 1, 0, 2, 0, 0, 0, 3,
};

#define WAIT_FOR_PLL_LOCKED(_reg) \
	do { \
		u32 st = 0; \
		int cnt = 10; \
		u32 reg = _reg; \
		while (cnt--) { \
			usleep_range(50, 60); \
			st = (((hd21_read_reg(reg) >> 30) & 0x3) == 3); \
			if (st) \
				break; \
			else { \
				/* reset hpll */ \
				hd21_set_reg_bits(reg, 1, 29, 1); \
				hd21_set_reg_bits(reg, 0, 29, 1); \
			} \
		} \
		if (cnt < 9) \
			pr_info("pll[0x%x] reset %d times\n", reg, 9 - cnt);\
	} while (0)

/*
 * When VCO outputs 6.0 GHz, if VCO unlock with default v1
 * steps, then need reset with v2 or v3
 */

void set21_s5_hpll_clk_out(u32 frac_rate, u32 clk)
{
	switch (clk) {
	case 6000000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x812cfa00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x80016101);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0xf0002211);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x558130b0);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x43443212);
		if (clk == 6000000) {
			hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL4, 0xf, 12, 4);
			hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL4, 0xf, 4, 4);
		}
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x00000203);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x0);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x812cfa01);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x812cfa03);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x812cfa07);	//3c yes?
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 5934060:
		/* 5940 / 1.001, Lp_spll_div_0p5_en */
		/* 5940 / 1.001 / 24 * 2 = 494.505 2^17 */
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00016101);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0xf0002217);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x558130b0);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0, 1, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 2, 1);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x11451292);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x00000203);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, (1 << 31) | (0x102d0 << 0));
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x1dee01);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 1, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 2, 1);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 5940000:
		/* 5940, Lp_spll_div_0p5_en */
		/* 5940 / 24 * 2 = 494.505 2^17 */
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00016101);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0xf0002217);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x558130b0);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0, 1, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 2, 1);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x11451292);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x00000203);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, (0 << 31) | (0 << 0));
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x1def01);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 1, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 2, 1);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 5600000:
	case 5405400:
	case 5000000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00066402);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00066403);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0xf00022a5);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x55813081);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00066401);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00066405);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x010cf700);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x03400293);	//
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x00000203);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x00016666);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x000c3221);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x000c3223);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x000c3227);	//3c yes?
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 4897000:
	case 4455000:
	case 4324320:
	case 4000000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00066402);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00066403);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0xf00022a5);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x55813081);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00066401);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00066405);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x010cf700);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x43400293);	//
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x00000203);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x00016666);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x010c1411);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x010c1413);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x010c1417);	//3c yes?
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 3712500:
	case 3450000:
	case 3243240:
	case 3197500:
	case 2970000:
	case 4032000:
	default:
		/* TODO */
		pr_info("error hpll clk: %d\n", clk);
		break;
	}
}

// fpll: 2376M  2376/24=99=0x63
//       2376/16=148.5M
/* div: 1, 2, 4, 8, ... 64 */
/* pixel_od: 1:1, 1:1.25, 1:1.5, 1:2 */
void hdmitx_set_s5_fpll(u32 clk, u32 div, u32 pixel_od)
{
	u32 div1;
	u32 div2;
	u32 quotient;
	u32 remainder;

	pr_info("%s[%d] clk %d div %d pixel_od %d\n", __func__, __LINE__, clk, div, pixel_od);
	/* setting fpll vco */
	quotient = clk / 24000;
	remainder = clk - quotient * 24000;
	/* remainder range: 0 ~ 23999, 0x5dbf, 15bits */
	remainder *= 1 << 17;
	remainder /= 24000;
	hd21_write_reg(CLKCTRL_FPLL_CTRL0, 0x21210000 | quotient);
	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL0, 1, 28, 1);
	hd21_write_reg(CLKCTRL_FPLL_CTRL1, 0x03a00000 | remainder);
	hd21_write_reg(CLKCTRL_FPLL_CTRL2, 0x00040000);
	hd21_write_reg(CLKCTRL_FPLL_CTRL3, 0x090da000);
	usleep_range(20, 30);
	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL0, 0, 29, 1);
	usleep_range(20, 30);
	hd21_write_reg(CLKCTRL_FPLL_CTRL3, 0x090da200);

	/* setting fpll div */
	if (div > 8) {
		div1 = 8;
		div2 = div / 8;
	} else {
		div1 = div;
		div2 = 1;
	}
	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL0, od_map[div1], 23, 2); // fpll_tmds_od<3:2> div8
	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL0, od_map[div2], 21, 2); // fpll_tmds_od<1:0> div2

	/* setting pixel_od */
	hd21_set_reg_bits(CLKCTRL_FPLL_CTRL0, pixel_od, 13, 3); // pixel_od
}

void set21_txpll_3_od0_s5(u8 od)
{
	if (!check_od(od))
		return;

	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, od_map[od], 20, 2);
}

void set21_txpll_3_od1_s5(u8 od)
{
	if (!check_od(od))
		return;

	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, od_map[od], 22, 2);
}

void set21_txpll_3_od2_s5(u8 od)
{
	if (!check_od(od))
		return;

	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, od_map[od], 24, 2);
}

void set21_txpll_4_od_s5(u8 od)
{
	if (!check_od(od))
		return;

	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL4, od_map[od], 30, 2);
}

void set_frl_hpll_od(enum frl_rate_enum rate)
{
	if (rate == FRL_NONE || rate > FRL_12G4L) {
		pr_info("hdmitx: frl: wrong rate %d\n", rate);
		return;
	}

	/* fixed OD */
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 0, 22, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 0, 24, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL4, 0, 30, 2);
	switch (rate) {
	case FRL_3G3L:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 2, 20, 2);
		break;
	case FRL_6G3L:
	case FRL_6G4L:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 1, 20, 2);
		break;
	case FRL_8G4L:
	case FRL_10G4L:
	case FRL_12G4L:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 0, 20, 2);
		break;
	default:
		break;
	};
}

void hdmitx_set_s5_clkdiv(struct hdmitx_dev *hdev)
{
	if (!hdev && !hdev->para)
		return;

	/* cts_htx_tmds_clk selects the htx_tmds20_clk or fll_tmds_clk */
	hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL1, hdev->frl_rate ? 1 : 0, 25, 2);
	if (!hdev->frl_rate && hdev->para->cs == HDMI_COLORSPACE_YUV420)
		hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL1, 1, 16, 7);
	else
		hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL1, 0, 16, 7);
	/* master_clk selects the vid_pll_clk or fpll_pixel_clk */
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, hdev->frl_rate ? 4 : 0, 16, 3);
}

void hdmitx21_phy_bandgap_en_s5(void)
{
	u32 val = 0;

	val = hd21_read_reg(ANACTRL_HDMIPHY_CTRL0);
	if (val == 0)
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x0b4242);
}

void set21_phy_by_mode_s5(u32 mode)
{
	switch (mode) {
	case HDMI_PHYPARA_6G: /* 5.94/4.5/3.7Gbps */
	case HDMI_PHYPARA_4p5G:
	case HDMI_PHYPARA_3p7G:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x0000ff03);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000730b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0xf7c890d3);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbc110600);
		break;
	case HDMI_PHYPARA_3G: /* 2.97Gbps */
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x0000ff03);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000730b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0xf7c890d3);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbc110600);
		break;
	case HDMI_PHYPARA_270M: /* 1.485Gbps, and below */
	case HDMI_PHYPARA_DEF:
	default:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x0000ff03);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000730b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0xf7c890d3);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbc110600);
		break;
	}
}

void set21_phy_by_frl_mode_s5(enum frl_rate_enum mode)
{
	switch (mode) {
	case FRL_3G3L:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x00003f03);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000570b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x37d800d5);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbd030600);
		break;
	case FRL_6G3L:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x00003f03);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000570b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x37d800cf);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbd030600);
		break;
	case FRL_6G4L:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x0000ff03);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000770b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x37d8cfcf);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbd330600);
		break;
	case FRL_8G4L:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x0000ff03);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000770b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x37d8cfcf);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbd770601);
		break;
	case FRL_10G4L:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x0000ff03);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000770b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x37d8cfcf);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbd770606);
		break;
	case FRL_12G4L:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x0000ff03);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000770b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x37d8afaf);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbd77062b);
		break;
	default:
		pr_info("unsupport frl_mode %d\n", mode);
		break;
	}
}

void hdmitx21_sys_reset_s5(void)
{
	/* Refer to system-Registers.docx */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 29); /* hdmi_tx */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 22); /* hdmitxphy */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 19); /* vid_pll_div */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 16); /* hdmitx_apb */
}

void set21_hpll_sspll_s5(enum hdmi_vic vic)
{
	switch (vic) {
	case HDMI_16_1920x1080p60_16x9:
	case HDMI_31_1920x1080p50_16x9:
	case HDMI_4_1280x720p60_16x9:
	case HDMI_19_1280x720p50_16x9:
	case HDMI_5_1920x1080i60_16x9:
	case HDMI_20_1920x1080i50_16x9:
		break;
	default:
		break;
	}
}
