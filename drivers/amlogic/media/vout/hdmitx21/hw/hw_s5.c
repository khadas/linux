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
	case 5940000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x812cfa00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x80016101);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0xf0002211);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x558130b0);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x43443212);	//
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x00000203);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x1000aaaa);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x812cfa01);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x812cfa03);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x812cfa07);	//3c yes?
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 5600000:
	case 5405400:
	case 4897000:
	case 4455000:
	case 4324320:
	case 3712500:
	case 3450000:
	case 3243240:
	case 3197500:
	case 2970000:
	case 4032000:
	default:
		pr_info("error hpll clk: %d\n", clk);
		break;
	}
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
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000731b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0xf7c890d3);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL1, 0x0390000e);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbc110600);
		break;
	case HDMI_PHYPARA_3G: /* 2.97Gbps */
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x0000ff03);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000731b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0xf7c890d3);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL1, 0x0390000e);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbc110600);
		break;
	case HDMI_PHYPARA_270M: /* 1.485Gbps, and below */
	case HDMI_PHYPARA_DEF:
	default:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x0000ff03);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL6, 0x0000731b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0xf7c890d3);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL1, 0x0390000e);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xbc110600);
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
