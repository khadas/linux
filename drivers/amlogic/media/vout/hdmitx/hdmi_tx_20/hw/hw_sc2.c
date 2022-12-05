// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/printk.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "common.h"
#include "mach_reg.h"
#include "reg_sc2.h"

#ifdef WAIT_FOR_PLL_LOCKED
#undef WAIT_FOR_PLL_LOCKED
#endif

#define WAIT_FOR_PLL_LOCKED(_reg) \
	do { \
		unsigned int st = 0; \
		int cnt = 10; \
		unsigned int reg = _reg; \
		while (cnt--) { \
			usleep_range(50, 60); \
			st = (((hd_read_reg(reg) >> 30) & 0x3) == 3); \
			if (st) \
				break; \
			else { \
				/* reset hpll */ \
				hd_set_reg_bits(reg, 1, 29, 1); \
				hd_set_reg_bits(reg, 0, 29, 1); \
			} \
		} \
		if (cnt < 9) \
			pr_info("pll[0x%x] reset %d times\n", reg, 9 - cnt);\
	} while (0)

/*
 * When VCO outputs 6.0 GHz, if VCO unlock with default v1
 * steps, then need reset with v2 or v3
 */
static bool set_hpll_hclk_v1(unsigned int m, unsigned int frac_val)
{
	int ret = 0;
	struct hdmitx_dev *hdev = get_hdmitx_device();

	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x0b3a0400 | (m & 0xff));
	hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x3, 28, 2);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, frac_val);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);

	if (frac_val == 0x8148) {
		if ((hdev->para->vic == HDMI_3840x2160p50_16x9 ||
		     hdev->para->vic == HDMI_3840x2160p60_16x9 ||
		     hdev->para->vic == HDMI_3840x2160p50_64x27 ||
		     hdev->para->vic == HDMI_3840x2160p60_64x27) &&
		     hdev->para->cs != COLORSPACE_YUV420) {
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x11551293);
		} else {
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x44331290);
		}
	} else {
		if (hdmitx_find_vendor_6g(hdev) &&
		    (hdev->para->vic == HDMI_3840x2160p50_16x9 ||
		    hdev->para->vic == HDMI_3840x2160p60_16x9 ||
		    hdev->para->vic == HDMI_3840x2160p50_64x27 ||
		    hdev->para->vic == HDMI_3840x2160p60_64x27 ||
		    hdev->para->vic == HDMI_4096x2160p50_256x135 ||
		    hdev->para->vic == HDMI_4096x2160p60_256x135) &&
		    hdev->para->cs != COLORSPACE_YUV420) {
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x11551293);
		} else {
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x6a68dc00);
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x65771290);
		}
	}
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x39272008);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x56540000);
	hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
	WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);

	ret = (((hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0) >> 30) & 0x3) == 0x3);
	return ret; /* return hpll locked status */
}

static bool set_hpll_hclk_v2(unsigned int m, unsigned int frac_val)
{
	int ret = 0;

	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x0b3a0400 | (m & 0xff));
	hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x3, 28, 2);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, frac_val);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0xea68dc00);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x65771290);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x39272008);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x56540000);
	hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
	WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);

	ret = (((hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0) >> 30) & 0x3) == 0x3);
	return ret; /* return hpll locked status */
}

static bool set_hpll_hclk_v3(unsigned int m, unsigned int frac_val)
{
	int ret = 0;

	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x0b3a0400 | (m & 0xff));
	hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x3, 28, 2);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, frac_val);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0xea68dc00);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x65771290);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x39272008);
	hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x55540000);
	hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
	WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);

	ret = (((hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0) >> 30) & 0x3) == 0x3);
	return ret; /* return hpll locked status */
}

void set_sc2_hpll_clk_out(unsigned int frac_rate, unsigned int clk)
{
	switch (clk) {
	case 5940000:
		if (set_hpll_hclk_v1(0xf7, frac_rate ? 0x8148 : 0x10000))
			break;
		if (set_hpll_hclk_v2(0x7b, 0x18000))
			break;
		if (set_hpll_hclk_v3(0xf7, 0x10000))
			break;
		break;
	case 5600000:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b0004e9);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x0000aaab);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);/*test*/
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x39270008);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	case 5405400:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b0004e1);
		if (frac_rate)
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00000000);
		else
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00007333);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x39270008);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	case 4897000:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b0004cc);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x0000d560);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x43231290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x29272008);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x56540028);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	case 4830000:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b0004c9);
		if (frac_rate)
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00008000);
		else
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00001910);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x43231290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x29272000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x56540028);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	case 4455000:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b0004b9);
		if (frac_rate)
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x0000e10e);
		else
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00014000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x43231290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x29272008);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x56540028);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	case 4324320:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b0004b4);
		if (frac_rate)
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00000000);
		else
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00005c29);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x39270008);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	case 3712500:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b00049a);
		if (frac_rate)
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x000110e1);
		else
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00016000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x43231290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x29272008);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x56540028);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	case 3450000:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b00048f);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00018000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x39270008);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	case 3243240:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b000487);
		if (frac_rate)
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00000000);
		else
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x0000451f);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x39270008);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	case 3197500:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b000485);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00007555);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x39270008);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	case 2970000:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b00047b);
		if (frac_rate)
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x000140b4);
		else
			hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00018000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x39270008);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	case 4032000:
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL0, 0x3b0004a8);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL1, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL5, 0x39270008);
		hd_write_reg(P_ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(P_ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd_read_reg(P_ANACTRL_HDMIPLL_CTRL0));
		break;
	default:
		pr_info("error hpll clk: %d\n", clk);
		break;
	}
}

void set_hpll_od1_sc2(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0, 16, 2);
		break;
	case 2:
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 1, 16, 2);
		break;
	case 4:
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 2, 16, 2);
		break;
	default:
		break;
	}
}

void set_hpll_od2_sc2(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0, 18, 2);
		break;
	case 2:
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 1, 18, 2);
		break;
	case 4:
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 2, 18, 2);
		break;
	default:
		break;
	}
}

void set_hpll_od3_sc2(unsigned int div)
{
	switch (div) {
	case 1:
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0, 20, 2);
		break;
	case 2:
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 1, 20, 2);
		break;
	case 4:
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 2, 20, 2);
		break;
	default:
		break;
	}
}

void hdmitx_phy_bandgap_en_sc2(void)
{
	unsigned int val = 0;

	val = hd_read_reg(P_ANACTRL_HDMIPHY_CTRL0);
	if (val == 0)
		hd_write_reg(P_ANACTRL_HDMIPHY_CTRL0, 0x0b4242);
}

void set_phy_by_mode_sc2(unsigned int mode)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	switch (mode) {
	case HDMI_PHYPARA_6G: /* 5.94/4.5/3.7Gbps */
	case HDMI_PHYPARA_4p5G:
	case HDMI_PHYPARA_3p7G:
		hd_write_reg(P_ANACTRL_HDMIPHY_CTRL5, 0x0000080b);
		hd_write_reg(P_ANACTRL_HDMIPHY_CTRL0, 0x37eb65c4);
		hd_write_reg(P_ANACTRL_HDMIPHY_CTRL3, 0x2ab0ff3b);
		/* for hdmi_rext use the 1.3k resistor */
		if (mode == HDMI_PHYPARA_6G && hdev->hdmi_rext == 1300)
			hd_write_reg(P_ANACTRL_HDMIPHY_CTRL0, 0x37eb6584);
		break;
	case HDMI_PHYPARA_3G: /* 2.97Gbps */
		hd_write_reg(P_ANACTRL_HDMIPHY_CTRL5, 0x00000003);
		hd_write_reg(P_ANACTRL_HDMIPHY_CTRL0, 0x33eb42a2);
		hd_write_reg(P_ANACTRL_HDMIPHY_CTRL3, 0x2ab0ff3b);
		break;
	case HDMI_PHYPARA_270M: /* 1.485Gbps, and below */
	case HDMI_PHYPARA_DEF:
	default:
		hd_write_reg(P_ANACTRL_HDMIPHY_CTRL5, 0x00000003);
		hd_write_reg(P_ANACTRL_HDMIPHY_CTRL0, 0x33eb4252);
		hd_write_reg(P_ANACTRL_HDMIPHY_CTRL3, 0x2ab0ff3b);
		break;
	}
}

void hdmitx_sys_reset_sc2(void)
{
	/* Refer to SC2-system-Registers.docx */
	hd_write_reg(P_RESETCTRL_RESET0, 1 << 29); /* hdmi_tx */
	hd_write_reg(P_RESETCTRL_RESET0, 1 << 22); /* hdmitxphy */
	hd_write_reg(P_RESETCTRL_RESET0, 1 << 19); /* vid_pll_div */
	hd_write_reg(P_RESETCTRL_RESET0, 1 << 16); /* hdmitx_apb */
}

void set_hpll_sspll_sc2(enum hdmi_vic vic)
{
	struct hdmitx_dev *hdev = get_hdmitx_device();

	switch (vic) {
	case HDMI_1920x1080p60_16x9:
	case HDMI_1920x1080p50_16x9:
	case HDMI_1280x720p60_16x9:
	case HDMI_1280x720p50_16x9:
	case HDMI_1920x1080i60_16x9:
	case HDMI_1920x1080i50_16x9:
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 1, 29, 1);
		/* bit[22:20] hdmi_dpll_fref_sel
		 * bit[8] hdmi_dpll_ssc_en
		 * bit[7:4] hdmi_dpll_ssc_dep_sel
		 */
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL2, 1, 20, 3);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL2, 1, 8, 1);
		/* 2: 1000ppm  1: 500ppm */
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL2, 2, 4, 4);
		if (hdev->dongle_mode)
			hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL2, 4, 4, 4);
		/* bit[15] hdmi_dpll_sdmnc_en */
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL3, 0, 15, 1);
		hd_set_reg_bits(P_ANACTRL_HDMIPLL_CTRL0, 0, 29, 1);
		break;
	default:
		break;
	}
}
