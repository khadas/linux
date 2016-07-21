/*
 * drivers/amlogic/display/vout/enc_clk_config.c
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



/* Linux Headers */
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/mutex.h>

/* Amlogic Headers */
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/vout/vinfo.h>

/* Local Headers */
#include "vout_reg.h"
#include "vout_io.h"
#include "vout_log.h"
#include "enc_clk_config.h"
#include "enc_clk_val.h"
#include "tv_out_reg.h"


#define check_clk_config(para) \
	do { \
		if (para == -1) \
			return; \
	} while (0)

static DEFINE_MUTEX(setclk_mutex);

static int set_viu_path(unsigned viu_channel_sel, enum viu_type_e viu_type_sel);

static void h_delay(void)
{
	int i = 1000;
	while (i--)
		;
}

static unsigned check_div(unsigned div)
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

static void wait_for_pll_locked(unsigned reg)
{
	unsigned int st = 0, cnt = 10;

	while (cnt--) {
		msleep_interruptible(10);
		st = !!(vout_cbus_read(reg) & (1 << 31));
		if (st)
			break;
		else {
			/* reset pll */
			vout_cbus_set_bits(reg, 0x3, 29, 2);
			vout_cbus_set_bits(reg, 0x2, 29, 2);
		}
	}
	if (cnt < 9)
		vout_log_info("pll[0x%x] reset %d times\n", reg, 9 - cnt);
}

static void set_hpll_clk_out_m6(unsigned int clk)
{
	vout_log_info("%s[%d] clk = %d\n", __func__, __LINE__, clk);
	switch (clk) {
	case 1488:
		vout_cbus_write(HHI_VID_PLL_CNTL, 0x43e);
		break;
	case 1080:
		vout_cbus_write(HHI_VID_PLL_CNTL, 0x42d);
		break;
	case 1066:
		vout_cbus_write(HHI_VID_PLL_CNTL, 0x42a);
		break;
	case 1058:
		vout_cbus_write(HHI_VID_PLL_CNTL, 0x422);
		break;
	case 1086:
		vout_cbus_write(HHI_VID_PLL_CNTL, 0x43e);
		break;
	case 1296:
		break;
	default:
		vout_log_info("error hpll clk: %d\n", clk);
		break;
	}
}

static void set_hpll_clk_out_m8(unsigned int clk)
{
	vout_log_info("%s[%d] clk = %d\n", __func__, __LINE__, clk);
	vout_cbus_write(HHI_VID_PLL_CNTL2, 0x69c88000);
	vout_cbus_write(HHI_VID_PLL_CNTL3, 0xca563823);
	vout_cbus_write(HHI_VID_PLL_CNTL4, 0x40238100);
	vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00012286);
	/* internal LDO share with HPLL & VIID PLL */
	vout_cbus_write(HHI_VID2_PLL_CNTL2, 0x430a800);
	switch (clk) {
	case 2971: /* only for 4k mode */
		vout_cbus_write(HHI_VID_PLL_CNTL2, 0x69c84000);
		vout_cbus_write(HHI_VID_PLL_CNTL3, 0xce49c022);
		vout_cbus_write(HHI_VID_PLL_CNTL4, 0x4123b100);
		vout_cbus_set_bits(HHI_VID2_PLL_CNTL2, 1, 16, 1);
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00012385);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x6000043d);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x4000043d);
		wait_for_pll_locked(HHI_VID_PLL_CNTL);
		h_delay();
		/* optimise HPLL VCO 2.97GHz performance */
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00016385);
		vout_cbus_write(HHI_VID_PLL_CNTL2, 0x69c84e00);
		break;
	case 2970: /* for 1080p/i 720p mode */
		vout_cbus_write(HHI_VID_PLL_CNTL2, 0x69c84000);
		vout_cbus_write(HHI_VID_PLL_CNTL3, 0x8a46c023);
		vout_cbus_write(HHI_VID_PLL_CNTL4, 0x4123b100);
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00012385);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x6000043d);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x4000043d);
		wait_for_pll_locked(HHI_VID_PLL_CNTL);
		h_delay();
		/* optimise HPLL VCO 2.97GHz performance */
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00016385);
		vout_cbus_write(HHI_VID_PLL_CNTL2, 0x69c84e00);
		break;
	case 2160:
		vout_cbus_write(HHI_VID_PLL_CNTL2, 0x69c84000);
		vout_cbus_write(HHI_VID_PLL_CNTL3, 0x8a46c023);
		vout_cbus_write(HHI_VID_PLL_CNTL4, 0x0123b100);
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x12385);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x6001042d);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x4001042d);
		wait_for_pll_locked(HHI_VID_PLL_CNTL);
		break;
	case 1080:
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x6000042d);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x4000042d);
		wait_for_pll_locked(HHI_VID_PLL_CNTL);
		break;
	case 1296:
		vout_cbus_write(HHI_VID_PLL_CNTL2, 0x59c88000);
		vout_cbus_write(HHI_VID_PLL_CNTL3, 0xca49b022);
		vout_cbus_write(HHI_VID_PLL_CNTL4, 0x0023b100);
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00012385);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x600c0436);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x400c0436);
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00016385);
		wait_for_pll_locked(HHI_VID_PLL_CNTL);
		break;
	default:
		vout_log_info("error hpll clk: %d\n", clk);
		break;
	}
	if (clk < 2970)
		vout_cbus_write(HHI_VID_PLL_CNTL5,
				(vout_cbus_read(HHI_VID_PLL_CNTL5)
				& (~(0xf << 12))) | (0x6 << 12));
}

static void set_hpll_clk_out_m8b(unsigned int clk)
{
	vout_log_info("%s[%d] clk = %d\n", __func__, __LINE__, clk);
	vout_cbus_write(HHI_VID_PLL_CNTL2, 0x69c88000);
	vout_cbus_write(HHI_VID_PLL_CNTL3, 0xca563823);
	vout_cbus_write(HHI_VID_PLL_CNTL4, 0x40238100);
	vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00012286);
	/* internal LDO share with HPLL & VIID PLL */
	vout_cbus_write(HHI_VID2_PLL_CNTL2, 0x430a800);
	switch (clk) {
	case 2970:
		vout_cbus_write(HHI_VID_PLL_CNTL2, 0x69c84000);
		vout_cbus_write(HHI_VID_PLL_CNTL3, 0x8a46c023);
		vout_cbus_write(HHI_VID_PLL_CNTL4, 0x4123b100);
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00012385);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x6000043d);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x4000043d);
		wait_for_pll_locked(HHI_VID_PLL_CNTL);
		h_delay();
		/* optimise HPLL VCO 2.97GHz performance */
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00016385);
		vout_cbus_write(HHI_VID_PLL_CNTL2, 0x69c84e00);
		break;
	case 2160:
		vout_cbus_write(HHI_VID_PLL_CNTL2, 0x69c84000);
		vout_cbus_write(HHI_VID_PLL_CNTL3, 0x8a46c023);
		vout_cbus_write(HHI_VID_PLL_CNTL4, 0x0123b100);
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x12385);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x6001042d);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x4001042d);
		wait_for_pll_locked(HHI_VID_PLL_CNTL);
		break;
	case 1296:
		vout_cbus_write(HHI_VID_PLL_CNTL2, 0x59c88000);
		vout_cbus_write(HHI_VID_PLL_CNTL3, 0xca49b022);
		vout_cbus_write(HHI_VID_PLL_CNTL4, 0x0023b100);
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00012385);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x600c0436);
		vout_cbus_write(HHI_VID_PLL_CNTL,  0x400c0436);
		vout_cbus_write(HHI_VID_PLL_CNTL5, 0x00016385);
		wait_for_pll_locked(HHI_VID_PLL_CNTL);
		break;
	default:
		vout_log_info("error hpll clk: %d\n", clk);
		break;
	}
	if (clk < 2970)
		vout_cbus_write(HHI_VID_PLL_CNTL5,
				(vout_cbus_read(HHI_VID_PLL_CNTL5)
				 & (~(0xf << 12))) | (0x6 << 12));
} /* meson8b */

static int hpll_wait_lock(unsigned int reg, unsigned int lock_bit)
{
	unsigned int pll_lock;
	int wait_loop = 200;
	int ret = 0;

	do {
		udelay(50);
		pll_lock = tv_out_hiu_getb(reg, lock_bit, 1);
		wait_loop--;
	} while ((pll_lock == 0) && (wait_loop > 0));
	if (wait_loop == 0)
		ret = -1;
	return ret;
}

static void set_hpll_clk_out_gxbb(unsigned int clk)
{
	int ret;

	tv_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x5800023d);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL2, 0x00404e00);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL4, 0x801da72c);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL5, 0x71486980);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL6, 0x00000e55);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x4800023d);
	ret = hpll_wait_lock(HHI_HDMI_PLL_CNTL, 31);
	if (ret)
		pr_info("[error]: hdmi_pll lock failed\n");

	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
	udelay(5);

	/* Disable the div output clock */
	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 18, 1);
	/* Enable the final output clock */
	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 19, 1);

	/* setup the XD divider value */
	tv_out_hiu_setb(HHI_VIID_CLK_DIV, (55 - 1), VCLK2_XD, 8);
	udelay(5);
	/* Bit[18:16] - v2_cntl_clk_in_sel */
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 4, VCLK2_CLK_IN_SEL, 3);
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
	udelay(2);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	tv_out_hiu_setb(HHI_VID_CLK_DIV, 8, 28, 4);
	tv_out_hiu_setb(HHI_VIID_CLK_DIV, 8, 28, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	tv_out_hiu_setb(HHI_VIID_CLK_DIV, 1, VCLK2_XD_EN, 2);
	udelay(5);

	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_DIV1_EN, 1);
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_SOFT_RST, 1);
	udelay(10);
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	tv_out_hiu_setb(HHI_VID_CLK_CNTL2, 1, 0, 1);
	tv_out_hiu_setb(HHI_VID_CLK_CNTL2, 1, 4, 1);
}

static void set_hpll_clk_out_gxl(unsigned int clk)
{
	int ret;

	/* config hpll to vco = 2970MHz */
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x4000027b);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL2, 0x800cb300);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL3, 0xa6212844);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL4, 0x0c4d000c);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL5, 0x001fa729);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL6, 0x01a31500);
	tv_out_hiu_setb(HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
	tv_out_hiu_setb(HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
	ret = hpll_wait_lock(HHI_HDMI_PLL_CNTL, 31);
	if (ret)
		pr_info("[error]: hdmi_pll lock failed\n");

	/* clk div */
	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 18, 1);
	/* Enable the final output clock */
	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 19, 1);

	/* setup the XD divider value */
	tv_out_hiu_setb(HHI_VIID_CLK_DIV, (55 - 1), VCLK2_XD, 8);
	udelay(5);
	/* Bit[18:16] - v2_cntl_clk_in_sel */
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 4, VCLK2_CLK_IN_SEL, 3);
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
	udelay(2);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	tv_out_hiu_setb(HHI_VID_CLK_DIV, 8, 28, 4);
	tv_out_hiu_setb(HHI_VIID_CLK_DIV, 8, 28, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	tv_out_hiu_setb(HHI_VIID_CLK_DIV, 1, VCLK2_XD_EN, 2);
	udelay(5);

	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_DIV1_EN, 1);
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_SOFT_RST, 1);
	udelay(10);
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	tv_out_hiu_setb(HHI_VID_CLK_CNTL2, 1, 0, 1);
	tv_out_hiu_setb(HHI_VID_CLK_CNTL2, 1, 4, 1);
	return;
}

static void set_hpll_clk_out_gxtvbb(unsigned int clk)
{
	int ret;

	tv_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x5800023d);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL2, 0x00404380);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL4, 0x801da72c);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL5, 0x71486980);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL6, 0x00000e55);
	tv_out_hiu_write(HHI_HDMI_PLL_CNTL, 0x4800023d);
	ret = hpll_wait_lock(HHI_HDMI_PLL_CNTL, 31);
	if (ret)
		pr_info("[error]: hdmi_pll lock failed\n");

	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
	udelay(5);

	/* Disable the div output clock */
	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 18, 1);
	/* Enable the final output clock */
	tv_out_hiu_setb(HHI_VID_PLL_CLK_DIV, 1, 19, 1);

	/* setup the XD divider value */
	tv_out_hiu_setb(HHI_VIID_CLK_DIV, (55 - 1), VCLK2_XD, 8);
	udelay(5);
	/* Bit[18:16] - v2_cntl_clk_in_sel */
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 4, VCLK2_CLK_IN_SEL, 3);
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
	udelay(2);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	tv_out_hiu_setb(HHI_VID_CLK_DIV, 8, 28, 4);
	tv_out_hiu_setb(HHI_VIID_CLK_DIV, 8, 28, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	tv_out_hiu_setb(HHI_VIID_CLK_DIV, 1, VCLK2_XD_EN, 2);
	udelay(5);

	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_DIV1_EN, 1);
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_SOFT_RST, 1);
	udelay(10);
	tv_out_hiu_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	tv_out_hiu_setb(HHI_VID_CLK_CNTL2, 1, 0, 1);
	tv_out_hiu_setb(HHI_VID_CLK_CNTL2, 1, 4, 1);
}

static void set_hpll_clk_out(unsigned clk)
{
	unsigned int cpu_type;

	check_clk_config(clk);
	vout_log_info("config HPLL\n");

	if (is_meson_g9tv_cpu()) {
		vout_log_info("%s[%d]\n", __FILE__, __LINE__);
		vout_log_info("TODO\n");
		return;
	}

	cpu_type = get_cpu_type();
	switch (cpu_type) {
	case MESON_CPU_MAJOR_ID_M6:
		set_hpll_clk_out_m6(clk);
		break;
	case MESON_CPU_MAJOR_ID_M8:
	case MESON_CPU_MAJOR_ID_M8M2:
		set_hpll_clk_out_m8(clk);
		break;
	case MESON_CPU_MAJOR_ID_M8B:
		set_hpll_clk_out_m8b(clk);
		break;
	case MESON_CPU_MAJOR_ID_GXBB:
		set_hpll_clk_out_gxbb(clk);
		break;
	case MESON_CPU_MAJOR_ID_GXTVBB:
		set_hpll_clk_out_gxtvbb(clk);
		break;
	case MESON_CPU_MAJOR_ID_GXL:
	case MESON_CPU_MAJOR_ID_GXM:
	case MESON_CPU_MAJOR_ID_TXL:
		set_hpll_clk_out_gxl(clk);
		break;
	default:
		break;
	}

	vout_log_info("config HPLL done\n");
}

static void set_hpll_hdmi_od(unsigned div)
{
	check_clk_config(div);
	switch (div) {
	case 1:
		vout_cbus_set_bits(HHI_VID_PLL_CNTL, 0, 18, 2);
		break;
	case 2:
		vout_cbus_set_bits(HHI_VID_PLL_CNTL, 1, 18, 2);
		break;
	case 4:
		/* #if MESON_CPU_TYPE == MESON_CPU_TYPE_MESON6 */
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_M6)
			vout_cbus_set_bits(HHI_VID_PLL_CNTL, 3, 18, 2);
		else {
			/* #else */
			vout_cbus_set_bits(HHI_VID_PLL_CNTL, 2, 18, 2);
		}
		/* #endif */
		break;
	case 8:
		vout_cbus_set_bits(HHI_VID_PLL_CNTL, 1, 16, 2);
		vout_cbus_set_bits(HHI_VID_PLL_CNTL, 3, 18, 2);
		break;
	default:
		break;
	}
}

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
static void set_hpll_lvds_od(unsigned div)
{
	check_clk_config(div);
	switch (div) {
	case 1:
		vout_cbus_set_bits(HHI_VID_PLL_CNTL, 0, 16, 2);
		break;
	case 2:
		vout_cbus_set_bits(HHI_VID_PLL_CNTL, 1, 16, 2);
		break;
	case 4:
		vout_cbus_set_bits(HHI_VID_PLL_CNTL, 2, 16, 2);
		break;
	case 8:     /* note: need test */
		vout_cbus_set_bits(HHI_VID_PLL_CNTL, 3, 16, 2);
		break;
	default:
		break;
	}
}
/* #endif */

/* viu_channel_sel: 1 or 2 */
/* viu_type_sel: 0: 0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT. */
static int set_viu_path(unsigned viu_channel_sel, enum viu_type_e viu_type_sel)
{
	if ((viu_channel_sel > 2) || (viu_channel_sel == 0))
		return -1;
	if (is_meson_m8_cpu() || is_meson_m8m2_cpu()) {
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
		vout_log_info("VPU_VIU_VENC_MUX_CTRL: 0x%x\n",
			      vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL));
		if (viu_channel_sel == 1) {
			vout_vcbus_set_bits(VPU_VIU_VENC_MUX_CTRL,
					viu_type_sel, 0, 2);
			vout_log_info("viu chan = 1\n");
		} else {
			/* viu_channel_sel ==2 */
			vout_vcbus_set_bits(VPU_VIU_VENC_MUX_CTRL,
					viu_type_sel, 2, 2);
			vout_log_info("viu chan = 2\n");
		}
		vout_log_info("VPU_VIU_VENC_MUX_CTRL: 0x%x\n",
			      vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL));
	}
	/* #endif */
	return 0;
}

static void set_vid_pll_div(unsigned div)
{
	check_clk_config(div);
	/* #if MESON_CPU_TYPE == MESON_CPU_TYPE_MESON6 */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M6) {
		/* Gate disable */
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 16, 1);
		switch (div) {
		case 10:
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 4, 4, 3);
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 1, 8, 2);
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 1, 12, 3);
			break;
		case 5:
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 4, 4, 3);
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 8, 2);
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 12, 3);
			break;
		default:
			break;
		}
		/* Soft Reset div_post/div_pre */
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 0, 2);
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 1, 3, 1);
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 1, 7, 1);
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 3, 0, 2);
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 3, 1);
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 7, 1);
		/* Gate enable */
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 1, 16, 1);
	}
	/* #endif */
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if (is_meson_m8_cpu() || is_meson_m8m2_cpu()) {
		/* Gate disable */
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 16, 1);
		switch (div) {
		case 10:
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 4, 4, 3);
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 1, 8, 2);
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 1, 12, 3);
			break;
		case 5:
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 4, 4, 3);
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 8, 2);
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 12, 3);
			break;
		case 6:
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 5, 4, 3);
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 8, 2);
			vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 12, 3);
			break;
		default:
			break;
		}
		/* Soft Reset div_post/div_pre */
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 0, 2);
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 1, 3, 1);
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 1, 7, 1);
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 3, 0, 2);
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 3, 1);
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 0, 7, 1);
		/* Gate enable */
		vout_cbus_set_bits(HHI_VID_DIVIDER_CNTL, 1, 16, 1);
	}
	/* #endif */
}

static void set_clk_final_div(unsigned div)
{
	check_clk_config(div);
	if (div == 0)
		div = 1;
	vout_cbus_set_bits(HHI_VID_CLK_CNTL, 1, 19, 1);
	vout_cbus_set_bits(HHI_VID_CLK_CNTL, 0, 16, 3);
	vout_cbus_set_bits(HHI_VID_CLK_DIV, div - 1, 0, 8);
	vout_cbus_set_bits(HHI_VID_CLK_CNTL, 7, 0, 3);
}

static void set_hdmi_tx_pixel_div(unsigned div)
{
	check_clk_config(div);
	div = check_div(div);
	vout_cbus_set_bits(HHI_HDMI_CLK_CNTL, div, 16, 4);
}
static void set_encp_div(unsigned div)
{
	check_clk_config(div);
	div = check_div(div);
	vout_cbus_set_bits(HHI_VID_CLK_DIV, div, 24, 4);
}

static void set_enci_div(unsigned div)
{
	check_clk_config(div);
	div = check_div(div);
	vout_cbus_set_bits(HHI_VID_CLK_DIV, div, 28, 4);
}

static void set_vdac0_div(unsigned div)
{
	check_clk_config(div);
	div = check_div(div);
	vout_cbus_set_bits(HHI_VIID_CLK_DIV, div, 28, 4);
}

static void set_vdac1_div(unsigned div)
{
	check_clk_config(div);
	div = check_div(div);
	vout_cbus_set_bits(HHI_VIID_CLK_DIV, div, 24, 4);
}

void set_vmode_clk(enum vmode_e mode)
{
	struct enc_clk_val_s *p_enc = NULL;
	int i = 0;
	int j = 0;

	mutex_lock(&setclk_mutex);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_GXL)) {
		p_enc = &setting_enc_clk_val_gxl[0];
		i = sizeof(setting_enc_clk_val_gxl)
			/ sizeof(struct enc_clk_val_s);
	} else if (is_meson_gxbb_cpu() || is_meson_gxtvbb_cpu()) {
		p_enc = &setting_enc_clk_val_gxbb[0];
		i = sizeof(setting_enc_clk_val_gxbb)
			/ sizeof(struct enc_clk_val_s);
	} else if (is_meson_m8m2_cpu()) {
		p_enc = &setting_enc_clk_val_m8m2[0];
		i = sizeof(setting_enc_clk_val_m8m2)
			/ sizeof(struct enc_clk_val_s);
	} else if (is_meson_m8_cpu()) {
		p_enc = &setting_enc_clk_val_m8[0];
		i = sizeof(setting_enc_clk_val_m8)
			/ sizeof(struct enc_clk_val_s);
	} else if (is_meson_m8b_cpu()) {
		p_enc = &setting_enc_clk_val_m8b[0];
		i = sizeof(setting_enc_clk_val_m8b)
			/ sizeof(struct enc_clk_val_s);
	} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_M6) {
		p_enc = &setting_enc_clk_val_m6[0];
		i = sizeof(setting_enc_clk_val_m6)
			/ sizeof(struct enc_clk_val_s);
	}

	if (p_enc == NULL) {
		vout_log_err("can't find clock table!\n");
		return;
	}

	vout_log_info("mode is: %d\n", mode);
	for (j = 0; j < i; j++) {
		if (mode == p_enc[j].mode)
			break;
	}
	if (j == sizeof(setting_enc_clk_val_gxbb)
		/ sizeof(struct enc_clk_val_s)) {
		mutex_unlock(&setclk_mutex);
		pr_info("not for hdmitx clk set\n");
		return;
	}
	set_viu_path(1, p_enc[j].viu_type); /* default viu1 */
	set_hpll_clk_out(p_enc[j].hpll_clk_out);

	if (is_meson_m8_cpu() || is_meson_m8m2_cpu())
		set_hpll_lvds_od(p_enc[j].hpll_lvds_od);
	if (get_cpu_type() < MESON_CPU_MAJOR_ID_GXBB) {
		set_hpll_hdmi_od(p_enc[j].hpll_hdmi_od);
		set_vid_pll_div(p_enc[j].vid_pll_div);
		set_clk_final_div(p_enc[j].clk_final_div);
		set_hdmi_tx_pixel_div(p_enc[j].hdmi_tx_pixel_div);
		set_encp_div(p_enc[j].encp_div);
		set_enci_div(p_enc[j].enci_div);
		set_vdac0_div(p_enc[j].vdac0_div);
		set_vdac1_div(p_enc[j].vdac1_div);
	}

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_M6) {
		/* If VCO outputs 1488, then we will reset it to exact 1485 */
		/* please note, don't forget to re-config CNTL3/4 */
		if (((vout_cbus_read(HHI_VID_PLL_CNTL) & 0x7fff) == 0x43e) ||
		    ((vout_cbus_read(HHI_VID_PLL_CNTL) & 0x7fff) == 0x21ef)) {
			vout_cbus_set_bits(HHI_VID_PLL_CNTL, 0x21ef, 0, 14);
			vout_cbus_write(HHI_VID_PLL_CNTL3, 0x4b525012);
			vout_cbus_write(HHI_VID_PLL_CNTL4, 0x42000101);
		}
	}

	mutex_unlock(&setclk_mutex);
	/* For debug only */
#if 0
	vout_log_info("hdmi debug tag\n%s\n%s[%d]\n",
			__FILE__, __func__, __LINE__);
#define P(a)  vout_log_info("%s 0x%04x: 0x%08x\n", #a, a, vout_cbus_read(a))
	P(HHI_VID_PLL_CNTL);
	P(HHI_VID_DIVIDER_CNTL);
	P(HHI_VID_CLK_CNTL);
	P(HHI_VID_CLK_DIV);
	P(HHI_HDMI_CLK_CNTL);
	P(HHI_VIID_CLK_DIV);
#define PP(a) vout_log_info("%s(%d): %d MHz\n", #a, a, clk_util_clk_msr(a))
	PP(CTS_PWM_A_CLK);
	PP(CTS_PWM_B_CLK);
	PP(CTS_PWM_C_CLK);
	PP(CTS_PWM_D_CLK);
	PP(CTS_ETH_RX_TX);
	PP(CTS_PCM_MCLK);
	PP(CTS_PCM_SCLK);
	PP(CTS_VDIN_MEAS_CLK);
	PP(CTS_VDAC_CLK1);
	PP(CTS_HDMI_TX_PIXEL_CLK);
	PP(CTS_MALI_CLK);
	PP(CTS_SDHC_CLK1);
	PP(CTS_SDHC_CLK0);
	PP(CTS_AUDAC_CLKPI);
	PP(CTS_A9_CLK);
	PP(CTS_DDR_CLK);
	PP(CTS_VDAC_CLK0);
	PP(CTS_SAR_ADC_CLK);
	PP(CTS_ENCI_CLK);
	PP(SC_CLK_INT);
	PP(USB_CLK_12MHZ);
	PP(LVDS_FIFO_CLK);
	PP(HDMI_CH3_TMDSCLK);
	PP(MOD_ETH_CLK50_I);
	PP(MOD_AUDIN_AMCLK_I);
	PP(CTS_BTCLK27);
	PP(CTS_HDMI_SYS_CLK);
	PP(CTS_LED_PLL_CLK);
	PP(CTS_VGHL_PLL_CLK);
	PP(CTS_FEC_CLK_2);
	PP(CTS_FEC_CLK_1);
	PP(CTS_FEC_CLK_0);
	PP(CTS_AMCLK);
	PP(VID2_PLL_CLK);
	PP(CTS_ETH_RMII);
	PP(CTS_ENCT_CLK);
	PP(CTS_ENCL_CLK);
	PP(CTS_ENCP_CLK);
	PP(CLK81);
	PP(VID_PLL_CLK);
	PP(AUD_PLL_CLK);
	PP(MISC_PLL_CLK);
	PP(DDR_PLL_CLK);
	PP(SYS_PLL_CLK);
	PP(AM_RING_OSC_CLK_OUT1);
	PP(AM_RING_OSC_CLK_OUT0);
#endif
}
