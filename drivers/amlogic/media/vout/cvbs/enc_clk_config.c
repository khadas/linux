// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/vout/cvbs/enc_clk_config.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/iomap.h>

/* Local Headers */
#include "cvbs_out.h"
#include "enc_clk_config.h"
#include "cvbs_out_reg.h"

static DEFINE_MUTEX(setclk_mutex);

static int pll_wait_lock(unsigned int reg, unsigned int lock_bit)
{
	unsigned int pll_lock;
	int wait_loop = 2000;
	int ret = 0;

	do {
		usleep_range(50, 60);
		pll_lock = cvbs_out_ana_getb(reg, lock_bit, 1);
		wait_loop--;
	} while ((pll_lock == 0) && (wait_loop > 0));
	if (wait_loop == 0)
		ret = -1;
	return ret;
}

static void disable_vid1_clk_out(void)
{
	struct meson_cvbsout_data *cvbs_data;

	cvbs_data = get_cvbs_data();
	if (!cvbs_data)
		return;

	/* close vclk_div gate */
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_ctrl, 0, VCLK_DIV1_EN, 1);
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_ctrl, 0, VCLK_EN0, 1);

	/* close pll_div gate */
	cvbs_out_vid_pll_set(cvbs_data->reg_vid_pll_clk_div, 0, 19, 1);
}

static void disable_vid2_clk_out(void)
{
	struct meson_cvbsout_data *cvbs_data;

	cvbs_data = get_cvbs_data();
	if (!cvbs_data)
		return;

	/* close vclk2_div gate */
	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_ctrl, 0, VCLK2_DIV1_EN, 1);
	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);

	/* close pll_div gate */
	cvbs_out_vid_pll_set(cvbs_data->reg_vid_pll_clk_div, 0, 19, 1);
}

static void cvbs_set_vid1_clk(unsigned int src_pll)
{
	struct meson_cvbsout_data *cvbs_data;
	int sel = 0;

	cvbs_data = get_cvbs_data();
	if (!cvbs_data)
		return;

	pr_info("%s\n", __func__);

	if (src_pll == 0) { /* hpll */
		/* divider: 1 */
		/* Disable the div output clock */
		cvbs_out_vid_pll_set(cvbs_data->reg_vid_pll_clk_div, 0, 19, 1);
		cvbs_out_vid_pll_set(cvbs_data->reg_vid_pll_clk_div, 0, 15, 1);

		cvbs_out_vid_pll_set(cvbs_data->reg_vid_pll_clk_div, 1, 18, 1);
		/* Enable the final output clock */
		cvbs_out_vid_pll_set(cvbs_data->reg_vid_pll_clk_div, 1, 19, 1);

		sel = 0;
	} else { /* gp0_pll */
		sel = 1;
	}

	/* xd: 55 */
	/* setup the XD divider value */
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_div, (55 - 1), VCLK_XD0, 8);
	usleep_range(5, 7);
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_ctrl, sel, VCLK_CLK_IN_SEL, 1);
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_ctrl, 1, VCLK_EN0, 1);
	usleep_range(2, 5);

	/* vclk: 27M */
	/* [31:28]=0 enci_clk_sel, select vclk_div1 */
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_div, 0, 28, 4);
	/* [31:28]=0 vdac_clk_sel, select vclk_div1 */
	/* [19]=0 disable atv_demod clk for vdac */
	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_div, 0, 19, 1);
	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_div, 0, 28, 4);
	/* release vclk_div_reset and enable vclk_div */
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_div, 1, VCLK_XD_EN, 2);
	usleep_range(5, 7);

	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_ctrl, 1, VCLK_DIV1_EN, 1);
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_ctrl, 1, VCLK_SOFT_RST, 1);
	usleep_range(10, 15);
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_ctrl, 0, VCLK_SOFT_RST, 1);
	usleep_range(5, 7);
}

static void cvbs_set_vid2_clk(unsigned int src_pll)
{
	struct meson_cvbsout_data *cvbs_data;
	int sel = 0;

	cvbs_data = get_cvbs_data();
	if (!cvbs_data)
		return;

	pr_info("%s\n", __func__);

	if (src_pll == 0) { /* hpll */
		/* divider: 1 */
		/* Disable the div output clock */
		cvbs_out_vid_pll_set(cvbs_data->reg_vid_pll_clk_div, 0, 19, 1);
		cvbs_out_vid_pll_set(cvbs_data->reg_vid_pll_clk_div, 0, 15, 1);

		cvbs_out_vid_pll_set(cvbs_data->reg_vid_pll_clk_div, 1, 18, 1);
		/* Enable the final output clock */
		cvbs_out_vid_pll_set(cvbs_data->reg_vid_pll_clk_div, 1, 19, 1);

		sel = 0;
	} else { /* gp0_pll */
		sel = 1;
	}

	/* xd: 55 */
	/* setup the XD divider value */
	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_div, (55 - 1), VCLK2_XD, 8);
	usleep_range(5, 7);
	/* Bit[18:16] - v2_cntl_clk_in_sel */
		cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_ctrl,
				  sel, VCLK2_CLK_IN_SEL, 3);
	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_ctrl, 1, VCLK2_EN, 1);
	usleep_range(2, 4);

	/* vclk: 27M */
	/* [31:28]=8 enci_clk_sel, select vclk2_div1 */
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_div, 8, 28, 4);
	/* [31:28]=8 vdac_clk_sel, select vclk2_div1 */
	/* [19]=0 disable atv_demod clk for vdac */
	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_div, 0, 19, 1);
	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_div, 8, 28, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_div, 1, VCLK2_XD_EN, 2);
	usleep_range(5, 7);

	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_ctrl, 1, VCLK2_DIV1_EN, 1);
	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_ctrl, 1, VCLK2_SOFT_RST, 1);
	usleep_range(10, 15);
	cvbs_out_hiu_setb(cvbs_data->reg_vid2_clk_ctrl, 0, VCLK2_SOFT_RST, 1);
	usleep_range(5, 7);
}

void set_vmode_clk(void)
{
	struct meson_cvbsout_data *cvbs_data;
	int ret;

	cvbs_data = get_cvbs_data();
	if (!cvbs_data)
		return;

	pr_info("%s start\n",  __func__);
	mutex_lock(&setclk_mutex);
	if (cvbs_cpu_type() == CVBS_CPU_TYPE_G12A ||
	    cvbs_cpu_type() == CVBS_CPU_TYPE_G12B ||
	    cvbs_cpu_type() == CVBS_CPU_TYPE_SM1) {
		if (cvbs_clk_path & 0x1) {
			pr_info("config g12a gp0_pll\n");
			cvbs_out_ana_write(HHI_GP0_PLL_CNTL0, 0x180204f7);
			cvbs_out_ana_write(HHI_GP0_PLL_CNTL1, 0x00010000);
			cvbs_out_ana_write(HHI_GP0_PLL_CNTL2, 0x00000000);
			cvbs_out_ana_write(HHI_GP0_PLL_CNTL3, 0x6a28dc00);
			cvbs_out_ana_write(HHI_GP0_PLL_CNTL4, 0x65771290);
			cvbs_out_ana_write(HHI_GP0_PLL_CNTL5, 0x39272000);
			cvbs_out_ana_write(HHI_GP0_PLL_CNTL6, 0x56540000);
			cvbs_out_ana_write(HHI_GP0_PLL_CNTL0, 0x380204f7);
			usleep_range(100, 150);
			cvbs_out_ana_write(HHI_GP0_PLL_CNTL0, 0x180204f7);
			ret = pll_wait_lock(HHI_GP0_PLL_CNTL0, 31);
		} else {
			pr_info("config g12a hdmi_pll\n");
			cvbs_out_ana_write(HHI_HDMI_PLL_CNTL, 0x1a0504f7);
			cvbs_out_ana_write(HHI_HDMI_PLL_CNTL2, 0x00010000);
			cvbs_out_ana_write(HHI_HDMI_PLL_CNTL3, 0x00000000);
			cvbs_out_ana_write(HHI_HDMI_PLL_CNTL4, 0x6a28dc00);
			cvbs_out_ana_write(HHI_HDMI_PLL_CNTL5, 0x65771290);
			cvbs_out_ana_write(HHI_HDMI_PLL_CNTL6, 0x39272000);
			cvbs_out_ana_write(HHI_HDMI_PLL_CNTL7, 0x56540000);
			cvbs_out_ana_write(HHI_HDMI_PLL_CNTL, 0x3a0504f7);
			usleep_range(100, 150);
			cvbs_out_ana_write(HHI_HDMI_PLL_CNTL, 0x1a0504f7);
			ret = pll_wait_lock(HHI_HDMI_PLL_CNTL, 31);
		}
		if (ret)
			pr_info("[error]:hdmi_pll lock failed\n");
	} else if (cvbs_cpu_type() == CVBS_CPU_TYPE_TL1) {
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x202f04f7);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x302f04f7);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL1,	0x10110000);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL2,	0x00001108);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL3,	0x10051400);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL4,	0x010100c0);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL4,	0x038300c0);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x342f04f7);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x142f04f7);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL2,	0x00003008);
		usleep_range(100, 150);
		ret = pll_wait_lock(HHI_TCON_PLL_CNTL0, 31);
		if (ret)
			pr_info("[error]:tl1 tcon_pll lock failed\n");
	} else if (cvbs_cpu_type() == CVBS_CPU_TYPE_TM2) {
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x202f04f7);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x302f04f7);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL1,	0x10110000);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL2,	0x00001108);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL3,	0x10051400);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL4,	0x010100c0);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL4,	0x038300c0);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x342f04f7);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x142f04f7);
		usleep_range(100, 150);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL2,	0x00003008);
		usleep_range(100, 150);
		ret = pll_wait_lock(HHI_TCON_PLL_CNTL0, 31);
		if (ret)
			pr_info("[error]:tl1 tcon_pll lock failed\n");
		/* mux tcon pll */
		cvbs_out_ana_setb(HHI_LVDS_TX_PHY_CNTL1_TL1, 0, 29, 1);
	} else if (cvbs_cpu_type() == CVBS_CPU_TYPE_SC2 ||
		   cvbs_cpu_type() == CVBS_CPU_TYPE_S4) {
		cvbs_out_ana_write(ANACTRL_HDMIPLL_CTRL0, 0x3b01047b);
		cvbs_out_ana_write(ANACTRL_HDMIPLL_CTRL1, 0x00018000);
		cvbs_out_ana_write(ANACTRL_HDMIPLL_CTRL2, 0x00000000);
		cvbs_out_ana_write(ANACTRL_HDMIPLL_CTRL3, 0x0a691c00);
		cvbs_out_ana_write(ANACTRL_HDMIPLL_CTRL4, 0x33771290);
		cvbs_out_ana_write(ANACTRL_HDMIPLL_CTRL5, 0x39270000);
		cvbs_out_ana_write(ANACTRL_HDMIPLL_CTRL6, 0x50540000);
		usleep_range(100, 101);
		cvbs_out_ana_write(ANACTRL_HDMIPLL_CTRL0, 0x1b01047b);
		ret = pll_wait_lock(ANACTRL_HDMIPLL_CTRL0, 31);
		if (ret)
			pr_info("[error]:hdmi_pll lock failed\n");
	} else if (cvbs_cpu_type() == CVBS_CPU_TYPE_T5 ||
		   cvbs_cpu_type() == CVBS_CPU_TYPE_T5D) {
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x202f04f7);
		usleep_range(100, 110);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x302f04f7);
		usleep_range(100, 110);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL1,	0x10110000);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL2,	0x00001108);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL3,	0x10051400);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL4,	0x010100c0);
		usleep_range(100, 110);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL4,	0x038300c0);
		usleep_range(100, 110);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x342f04f7);
		usleep_range(100, 110);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL0,	0x142f04f7);
		usleep_range(100, 110);
		cvbs_out_ana_write(HHI_TCON_PLL_CNTL2,	0x00003008);
		usleep_range(100, 110);
		ret = pll_wait_lock(HHI_TCON_PLL_CNTL0, 31);
		if (ret)
			pr_info("[error]:tl1 tcon_pll lock failed\n");
		cvbs_out_ana_setb(HHI_LVDS_TX_PHY_CNTL1_TL1, 0, 29, 1);
	} else {
		pr_info("config eqafter gxl hdmi pll\n");
		cvbs_out_ana_write(HHI_HDMI_PLL_CNTL, 0x4000027b);
		cvbs_out_ana_write(HHI_HDMI_PLL_CNTL2, 0x800cb300);
		cvbs_out_ana_write(HHI_HDMI_PLL_CNTL3, 0xa6212844);
		cvbs_out_ana_write(HHI_HDMI_PLL_CNTL4, 0x0c4d000c);
		cvbs_out_ana_write(HHI_HDMI_PLL_CNTL5, 0x001fa729);
		cvbs_out_ana_write(HHI_HDMI_PLL_CNTL6, 0x01a31500);
		cvbs_out_ana_setb(HHI_HDMI_PLL_CNTL, 0x1, 28, 1);
		cvbs_out_ana_setb(HHI_HDMI_PLL_CNTL, 0x0, 28, 1);
		ret = pll_wait_lock(HHI_HDMI_PLL_CNTL, 31);
		if (ret)
			pr_info("[error]: hdmi_pll lock failed\n");
	}

	if (cvbs_cpu_type() == CVBS_CPU_TYPE_G12A ||
	    cvbs_cpu_type() == CVBS_CPU_TYPE_G12B ||
	    cvbs_cpu_type() == CVBS_CPU_TYPE_SM1) {
		if (cvbs_clk_path & 0x2)
			cvbs_set_vid1_clk(cvbs_clk_path & 0x1);
		else
			cvbs_set_vid2_clk(cvbs_clk_path & 0x1);
	} else if (cvbs_cpu_type() == CVBS_CPU_TYPE_TL1 ||
		cvbs_cpu_type() == CVBS_CPU_TYPE_TM2 ||
		cvbs_cpu_type() == CVBS_CPU_TYPE_T5 ||
		cvbs_cpu_type() == CVBS_CPU_TYPE_T5D) {
		if (cvbs_clk_path & 0x2)
			cvbs_set_vid1_clk(0);
		else
			cvbs_set_vid2_clk(0);
	} else {
		cvbs_set_vid2_clk(0);
	}

	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_ctrl2, 1, 0, 1);
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_ctrl2, 1, 4, 1);

	mutex_unlock(&setclk_mutex);
	pr_info("%s DONE\n", __func__);
}

void disable_vmode_clk(void)
{
	struct meson_cvbsout_data *cvbs_data;

	cvbs_data = get_cvbs_data();
	if (!cvbs_data)
		return;

	/* close cts_enci_clk & cts_vdac_clk */
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_ctrl2, 0, 0, 1);
	cvbs_out_hiu_setb(cvbs_data->reg_vid_clk_ctrl2, 0, 4, 1);

	if (cvbs_cpu_type() == CVBS_CPU_TYPE_G12A ||
	    cvbs_cpu_type() == CVBS_CPU_TYPE_G12B ||
	    cvbs_cpu_type() == CVBS_CPU_TYPE_SM1) {
		if (cvbs_clk_path & 0x2)
			disable_vid1_clk_out();
		else
			disable_vid2_clk_out();

		/* disable pll */
		if (cvbs_clk_path & 0x1)
			cvbs_out_ana_setb(HHI_GP0_PLL_CNTL0, 0, 28, 1);
		else
			cvbs_out_ana_setb(HHI_HDMI_PLL_CNTL, 0, 28, 1);
	} else if (cvbs_cpu_type() == CVBS_CPU_TYPE_SC2 ||
		   cvbs_cpu_type() == CVBS_CPU_TYPE_S4) {
		disable_vid2_clk_out();
		/* disable pll */
		cvbs_out_ana_setb(ANACTRL_HDMIPLL_CTRL0, 0, 28, 1);
	} else {
		disable_vid2_clk_out();
		/* disable pll */
		cvbs_out_ana_setb(HHI_HDMI_PLL_CNTL, 0, 30, 1);
	}
}

void cvbs_out_vid_pll_set(unsigned int _reg, unsigned int _value,
			  unsigned int _start, unsigned int _len)
{
	if (cvbs_cpu_type() == CVBS_CPU_TYPE_T5 ||
	    cvbs_cpu_type() == CVBS_CPU_TYPE_T5D)
		cvbs_out_ana_setb(_reg, _value, _start, _len);
	else
		cvbs_out_hiu_setb(_reg, _value, _start, _len);
}

unsigned int cvbs_out_vid_pll_read(unsigned int _reg)
{
	if (cvbs_cpu_type() == CVBS_CPU_TYPE_T5 ||
	    cvbs_cpu_type() == CVBS_CPU_TYPE_T5D)
		return cvbs_out_ana_read(_reg);
	else
		return cvbs_out_hiu_read(_reg);
}

