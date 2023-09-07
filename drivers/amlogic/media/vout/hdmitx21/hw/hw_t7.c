// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/printk.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/delay.h>
#include "common.h"

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
static bool set_hpll_hclk_v1(u32 m, u32 frac_val)
{
	int ret = 0;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x0b3a0400 | (m & 0xff));
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x3, 28, 2);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, frac_val);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);

	if (frac_val == 0x8148) {
		if ((hdev->para->timing.vic == HDMI_96_3840x2160p50_16x9 ||
		     hdev->para->timing.vic == HDMI_97_3840x2160p60_16x9 ||
		     hdev->para->timing.vic == HDMI_106_3840x2160p50_64x27 ||
		     hdev->para->timing.vic == HDMI_107_3840x2160p60_64x27) &&
		     hdev->para->cs != HDMI_COLORSPACE_YUV420) {
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x11551293);
		} else {
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x44331290);
		}
	} else {
		if ((hdev->para->timing.vic == HDMI_96_3840x2160p50_16x9 ||
		    hdev->para->timing.vic == HDMI_97_3840x2160p60_16x9 ||
		    hdev->para->timing.vic == HDMI_106_3840x2160p50_64x27 ||
		    hdev->para->timing.vic == HDMI_107_3840x2160p60_64x27 ||
		    hdev->para->timing.vic == HDMI_101_4096x2160p50_256x135 ||
		    hdev->para->timing.vic == HDMI_102_4096x2160p60_256x135) &&
		    hdev->para->cs != HDMI_COLORSPACE_YUV420) {
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x11551293);
		} else {
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x6a68dc00);
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x65771290);
		}
	}
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927200a);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x56540000);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
	WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);

	ret = (((hd21_read_reg(ANACTRL_HDMIPLL_CTRL0) >> 30) & 0x3) == 0x3);
	return ret; /* return hpll locked status */
}

static bool set_hpll_hclk_v2(u32 m, u32 frac_val)
{
	int ret = 0;

	hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x0b3a0400 | (m & 0xff));
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x3, 28, 2);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, frac_val);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0xea68dc00);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x65771290);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927200a);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x56540000);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
	WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);

	ret = (((hd21_read_reg(ANACTRL_HDMIPLL_CTRL0) >> 30) & 0x3) == 0x3);
	return ret; /* return hpll locked status */
}

static bool set_hpll_hclk_v3(u32 m, u32 frac_val)
{
	int ret = 0;

	hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x0b3a0400 | (m & 0xff));
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x3, 28, 2);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, frac_val);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0xea68dc00);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x65771290);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927200a);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x55540000);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
	WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);

	ret = (((hd21_read_reg(ANACTRL_HDMIPLL_CTRL0) >> 30) & 0x3) == 0x3);
	return ret; /* return hpll locked status */
}

void set21_t7_hpll_clk_out(u32 frac_rate, u32 clk)
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
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b0004e9);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x0000aaab);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);/*test*/
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927000a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 5405400:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b0004e1);
		if (frac_rate)
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00000000);
		else
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00007333);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927000a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 5035000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b0004d1);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00019555);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x43231290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x2927200a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x56540028);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 4897000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b0004cc);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x0000d560);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x43231290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x2927200a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x56540028);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 4455000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b0004b9);
		if (frac_rate)
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x0000e10e);
		else
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00014000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x43231290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x2927200a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x56540028);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 4324320:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b0004b4);
		if (frac_rate)
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00000000);
		else
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00005c29);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927000a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 4032000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b0004a8);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927000a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 4028000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b0004a7);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x0001aa80);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927000a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 3712500:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b00049a);
		if (frac_rate)
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x000110e1);
		else
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00016000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x6a685c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x43231290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x2927200a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x56540028);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 3450000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b00048f);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00018000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927000a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 3243240:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b000487);
		if (frac_rate)
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00000000);
		else
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x0000451f);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927000a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 3197500:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b000485);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00007555);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927000a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 3021000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b00047d);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x0001c000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927000a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	case 2970000:
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x3b00047b);
		if (frac_rate)
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x000140b4);
		else
			hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x00018000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x4a691c00);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL5, 0x3927000a);
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0x0, 29, 1);
		WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
		pr_info("HPLL: 0x%x\n", hd21_read_reg(ANACTRL_HDMIPLL_CTRL0));
		break;
	default:
		pr_info("error hpll clk: %d\n", clk);
		break;
	}
}

void set21_hpll_od1_t7(u32 div)
{
	switch (div) {
	case 1:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0, 16, 2);
		break;
	case 2:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 16, 2);
		break;
	case 4:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 2, 16, 2);
		break;
	case 8:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 3, 16, 2);
		break;
	default:
		break;
	}
}

void set21_hpll_od2_t7(u32 div)
{
	switch (div) {
	case 1:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0, 18, 2);
		break;
	case 2:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 18, 2);
		break;
	case 4:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 2, 18, 2);
		break;
	default:
		break;
	}
}

void set21_hpll_od3_t7(u32 div)
{
	switch (div) {
	case 1:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0, 20, 2);
		break;
	case 2:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 20, 2);
		break;
	case 4:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 2, 20, 2);
		break;
	default:
		break;
	}
}

void hdmitx21_phy_bandgap_en_t7(void)
{
	u32 val = 0;

	val = hd21_read_reg(ANACTRL_HDMIPHY_CTRL0);
	if (val == 0)
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x0b4242);
}

void set21_phy_by_mode_t7(u32 mode)
{
	unsigned int phyctrl5;
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	if (hdev->data->chip_type == MESON_CPU_ID_T7)
		phyctrl5 = ANACTRL_HDMIPHY_CTRL5;
	else
		phyctrl5 = ANACTRL_HDMIPHY_CTRL6;

	switch (mode) {
	case HDMI_PHYPARA_6G: /* 5.94/4.5/3.7Gbps */
	case HDMI_PHYPARA_4p5G:
	case HDMI_PHYPARA_3p7G:
		hd21_write_reg(phyctrl5, 0x0000080b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x37eb65c4);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0x2ab0ff3b);
		break;
	case HDMI_PHYPARA_3G: /* 2.97Gbps */
		hd21_write_reg(phyctrl5, 0x00000003);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x33eb42a2);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0x2ab0ff3b);
		break;
	case HDMI_PHYPARA_270M: /* 1.485Gbps, and below */
	case HDMI_PHYPARA_DEF:
	default:
		hd21_write_reg(phyctrl5, 0x00000003);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x33eb4252);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0x2ab0ff3b);
		break;
	}
}

void hdmitx21_sys_reset_t7(void)
{
	/* Refer to system-Registers.docx */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 29); /* hdmi_tx */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 22); /* hdmitxphy */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 19); /* vid_pll_div */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 16); /* hdmitx_apb */
}

void set21_hpll_sspll_t7(enum hdmi_vic vic)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	switch (vic) {
	case HDMI_16_1920x1080p60_16x9:
	case HDMI_31_1920x1080p50_16x9:
	case HDMI_4_1280x720p60_16x9:
	case HDMI_19_1280x720p50_16x9:
	case HDMI_5_1920x1080i60_16x9:
	case HDMI_20_1920x1080i50_16x9:
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 29, 1);
		/* bit[22:20] hdmi_dpll_fref_sel
		 * bit[8] hdmi_dpll_ssc_en
		 * bit[7:4] hdmi_dpll_ssc_dep_sel
		 */
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL2, 1, 20, 3);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL2, 1, 8, 1);
		/* 2: 1000ppm  1: 500ppm */
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL2, 2, 4, 4);
		if (hdev->dongle_mode)
			hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL2, 4, 4, 4);
		/* bit[15] hdmi_dpll_sdmnc_en */
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3, 0, 15, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0, 29, 1);
		break;
	default:
		break;
	}
}
