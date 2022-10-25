// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/arm-smccc.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>

/* Local include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_hw_tl1.h"

/* for TL1 */
static const u32 eq_debug[] = {
	/* value	  eq gain */
	0xffffffff,
	0xffff1030,/* min */
	0x7fff1030,
	0x7f7f1030,
	0x3f7f1030,
	0x3f3f1030,
	0x1f3f1030,
	0x1f1f1030,
	0x0f0f1030,/* half of all range max */
	0x0f071030,
	0x07071030,
	0x07031030,
	0x03031030,
	0x03011030,
	0x01011030,
	0x01001030,
	0x00001030,/* max */
	0xffff0000,/* min */
	0x7fff0000,
	0x7f7f0000,
	0x3f7f0000,
	0x3f3f0000,
	0x1f3f0000,
	0x1f1f0000,
	0x0f0f0000,/* half of all range max */
	0x0f070000,
	0x07070000,
	0x07030000,
	0x03030000,
	0x03010000,
	0x01010000,
	0x01000000,
	0x00000000,/* max */
};

static const u32 phy_misci[][4] = {
		/* 0xd7	 0xd8		 0xe0		 0xe1 */
	{	 /* 24~45M */
		0x30037078, 0x00000080, 0x02218000, 0x00000010,
	},
	{	 /* 45~74.5M */
		0x30037078, 0x00000080, 0x02218000, 0x00000010,
	},
	{	 /* 77~155M */
		0x30037078, 0x00000080, 0x02218000, 0x00000010,
	},
	{	 /* 155~340M */
		0x30037078, 0x00000080, 0x02218000, 0x00000010,
	},
	{	 /* 340~525M */
		0x30037078, 0x007f0080, 0x02218000, 0x00000010,
	},
	{	 /* 525~600M */
		0x30037078, 0x007f8080, 0x02218000, 0x00000010,
	},
};

static const u32 phy_dcha[][3] = {
	/*  0xe2		 0xe3		 0xe4 */
	{	 /* 24~45M */
		0x00000280, 0x4400c202, 0x030088a2,
	},
	{	 /* 45~74.5M */
		0x00000280, 0x4400c202, 0x030088a2,
	},
	{	 /* 77~155M */
		0x000002a2, 0x6800c202, 0x01009126,
	},
	{	 /* 155~340M */
		0x00000280, 0x0800c202, 0x0100cc31,
	},
	{	 /* 340~525M */
		0x00000280, 0x0700003c, 0x1d00cc31,
	},
	{	 /* 525~600M */
		0x00000280, 0x07000000, 0x1d00cc31,
	},
};

static const u32 phy_dcha_reva[][3] = {
	/*  0xe2		 0xe3		 0xe4 */
	{	 /* 24~45M */
		0x00000280, 0x2400c202, 0x030088a2,
	},
	{	 /* 45~74.5M */
		0x00000280, 0x2400c202, 0x030088a2,
	},
	{	 /* 77~155M */
		0x000002a2, 0x4800c202, 0x01009126,
	},
	{	 /* 155~340M */
		0x000002a2, 0x0800c202, 0x0100cc31,
	},
	{	 /* 340~525M */
		0x000002a2, 0x0700003c, 0x1d00cc31,
	},
	{	 /* 525~600M */
		0x000002a2, 0x0700003c, 0x1d00cc31,
	},
};

/* long cable */
static const u32 phy_dchd_1[][3] = {
		 /*  0xe5		 0xe6		 0xe7 */
	{	 /* 24~45M */
		0x003e714a, 0x1e051630, 0x00018000,
	},
	{	 /* 45~74.5M */
		0x003e714a, 0x1e051630, 0x00018000,
	},
	{	 /* 77~155M */
		0x003c714a, 0x1e062620, 0x00018000,
	},
	{	 /* 155~340M */
		0x003c714a, 0x1e050064, 0x0001a000,
	},
	{	 /* 340~525M */
		0x003c714a, 0x1e050064, 0x0001a000,
	},
	{	 /* 525~600M */
		0x003e714a, 0x1e050560, 0x0001a000,
	},
};

/* short cable */
static const u32 phy_dchd_2[][3] = {
		/*  0xe5		 0xe6		 0xe7 */
	{	 /* 24~45M */
		0x003e714a, 0x1e022220, 0x00018000,
	},
	{	 /* 45~74.5M */
		0x003e714a, 0x1e022220, 0x00018000,
	},
	{	 /* 77~155M */
		0x003c714a, 0x1e022220, 0x00018000,
	},
	{	 /* 155~340M */
		0x003c714a, 0x1e022220, 0x0001a000,
	},
	{	 /* 340~525M */
		0x003c714a, 0x1e040460, 0x0001a000,
	},
	{	 /* 525~600M */
		0x003e714a, 0x1e040460, 0x0001a000,
	},
};

u32 tl1_tm2_reg360[] = {
	0x7ff,
	0x5ff,
	0x4ff,
	0x47f,
	0x43f,
	0x41f,
	0x40f,
	0x407,
	0x403,
	0x401,
	0x400
};

bool is_tl1_former(void)
{
	if (is_meson_tl1_cpu() &&
	    is_meson_rev_a())
		return 1;
	return 0;
}

void aml_eq_cfg_tl1(void)
{
	u32 data32 = 0;
	u32 idx = rx.phy.phy_bw;

	/* data channel release reset */
	data32 = rd_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL0);
	data32 |= (0x7 << 7);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL0, data32);
	usleep_range(50, 60);
	if (find_best_eq) {
		data32 = phy_dchd_1[idx][1] & (~(MSK(16, 4)));
		data32 |= find_best_eq << 4;
	} else if ((rx.phy.cablesel % 2) == 0) {
		data32 = phy_dchd_1[idx][1];
	} else if ((rx.phy.cablesel % 2) == 1) {
		data32 = phy_dchd_2[idx][1];
	}
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
	usleep_range(5, 10);
	data32 |= 0x00400000;
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
}

void aml_phy_cfg_tl1(void)
{
	u32 idx = rx.phy.phy_bw;
	u32 data32, c1, c2, r1, r2;
	u32 term_value =
		hdmirx_rd_top(TOP_HPD_PWR5V) & 0x7;

	c1 = eq_debug[eq_dbg_lvl] & 0xff;
	c2 = (eq_debug[eq_dbg_lvl] >> 8) & 0xff;
	r1 = (eq_debug[eq_dbg_lvl] >> 16) & 0xff;
	r2 = (eq_debug[eq_dbg_lvl] >> 24) & 0xff;
	data32 = phy_misci[idx][0];
	data32 = (data32 & (~0x7));
	data32 |= term_value;
	data32 &= ~(disable_port_num & 0x07);
	/* terminal en */
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL0, data32);
	usleep_range(2, 4);
	/* data channel and common block reset */
	/*update from "data channel and common block */
	/* reset"to"only common block reset" */
	data32 |= 0xf << 7;
	/* data32 |= 0x1 << 10; */
	usleep_range(5, 10);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL0, data32);
	usleep_range(2, 4);
	data32 = phy_misci[idx][1];
	aml_phy_get_trim_val_tl1_tm2();
	if (idx < PHY_BW_5 && phy_tdr_en) {
		phy_trim_val = (~(0xfff << 12)) & phy_trim_val;
		phy_trim_val = (tl1_tm2_reg360[rlevel] << 12) | phy_trim_val;
		if (term_cal_en) {
			data32 = (((data32 & (~(0x3ff << 12))) |
				(term_cal_val << 12)) | (1 << 22));
		} else {
			data32 = phy_trim_val;
		}
	} else {
		data32 = phy_trim_val;
	}
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL1, data32);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL2, phy_misci[idx][2]);

	/* reset and select data port */
	data32 = phy_misci[idx][3];
	data32 |= ((1 << rx.port) << 6);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL3, data32);

	/* release reset */
	data32 |= (1 << 11);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL3, data32);

	usleep_range(5, 10);
	if (is_tl1_former()) {
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL0, phy_dcha_reva[idx][0]);

		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL1, phy_dcha_reva[idx][1]);

		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL2, phy_dcha_reva[idx][2]);
	} else {
		if (eq_dbg_lvl == 0) {
			wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL0, phy_dcha[idx][0]);
			wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL1, phy_dcha[idx][1]);
			wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL2, phy_dcha[idx][2]);
		} else {
			wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL0,
				   (phy_dcha[idx][0] | (c1 << 10) | (c2 << 18) | (r1 << 26)));
			wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL1,
				   (phy_dcha[idx][1] | (r1 >> 6)) | (r2 << 2));
			wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL2,
				   (phy_dcha[idx][2] & (~(1 << 24))));
		}
	}
	if (cdr_lock_level == 0)
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL0, phy_dchd_1[idx][0]);
	else if (cdr_lock_level == 1)
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL0, 0x006f0041);
	else if (cdr_lock_level == 2)
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL0, 0x002f714a);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL2, phy_dchd_1[idx][2]);

	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL2, phy_dchd_1[idx][2]);
	if ((rx.phy.cablesel % 2) == 0)
		data32 = phy_dchd_1[idx][1];
	else if ((rx.phy.cablesel % 2) == 1)
		data32 = phy_dchd_2[idx][1];

	/* wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1, data32); */
	usleep_range(5, 10);
	/*data32 |= 0x00400000;*/
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1, data32);/*398*/
}

struct apll_param apll_tab_tl1[] = {
	/*od for tmds: 2/4/8/16/32*/
	/*od2 for audio: 1/2/4/8/16*/
	/* bw M, N, od, od_div, od2, od2_div, aud_div */
	/* {PLL_BW_0, 160, 1, 0x5, 32,0x2, 8, 2}, */
	{PLL_BW_0, 160, 1, 0x5, 32, 0x1, 8, 3},/* 16 x 27 */
	/* {PLL_BW_1, 80, 1, 0x4,	16, 0x2, 8, 1}, */
	{PLL_BW_1, 80, 1, 0x4, 16, 0x0, 8, 3},/* 8 x 74 */
	/* {PLL_BW_2, 40, 1, 0x3, 8,	0x2, 8, 0}, */
	{PLL_BW_2, 40, 1, 0x3, 8,	 0x0, 8, 2}, /* 4 x 148 */
	/* {PLL_BW_3, 40, 2, 0x2, 4,	0x1, 4, 0}, */
	{PLL_BW_3, 40, 2, 0x2, 4,	 0x0, 4, 1},/* 2 x 297 */
	/* {PLL_BW_4, 40, 1, 0x1, 2,	0x0, 2, 0}, */
	{PLL_BW_4, 40, 1, 0x1, 2,	 0x0, 2, 0},/* 594 */
	{PLL_BW_NULL, 40, 1, 0x3, 8,	0x2, 8, 0},
};

void aml_pll_bw_cfg_tl1(void)
{
	u32 M, N;
	u32 od, od_div;
	u32 od2, od2_div;
	u32 bw = rx.phy.pll_bw;
	u32 vco_clk;
	u32 data, data2;
	u32 cableclk = rx.clk.cable_clk / KHz;
	int pll_rst_cnt = 0;
	u32 clk_rate;

	clk_rate = rx_get_scdc_clkrate_sts();
	bw = aml_phy_pll_band(rx.clk.cable_clk, clk_rate);
	if (!is_clk_stable() || !cableclk)
		return;
	od_div = apll_tab_tl1[bw].od_div;
	od = apll_tab_tl1[bw].od;
	M = apll_tab_tl1[bw].M;
	N = apll_tab_tl1[bw].N;
	od2_div = apll_tab_tl1[bw].od2_div;
	od2 = apll_tab_tl1[bw].od2;

	/*set audio pll divider*/
	rx.phy.aud_div = apll_tab_tl1[bw].aud_div;
	vco_clk = (cableclk * M) / N; /*KHz*/
	if ((vco_clk < (2970 * KHz)) || (vco_clk > (6000 * KHz))) {
		if (log_level & VIDEO_LOG)
			rx_pr("err: M=%d,N=%d,vco_clk=%d\n", M, N, vco_clk);
	}

	if (is_tl1_former())
		od2 += 1;
	do {
		/*cntl0 M <7:0> N<14:10>*/
		data = 0x00090400 & 0xffff8300;
		data |= M;
		data |= (N << 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL0, data | 0x20000000);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL0, data | 0x30000000);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL1, 0x00000000);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL2, 0x00001118);
		usleep_range(5, 10);
		data2 = 0x10058f30 | od2;
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL3, data2);
		if (is_tl1_former())
			data2 = 0x000100c0;
		else
			data2 = 0x080130c0;
		data2 |= (od << 24);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL4, data2);
		usleep_range(5, 10);
		/*apll_vctrl_mon_en*/
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL4, data2 | 0x00800000);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL0, data | 0x34000000);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL0, data | 0x14000000);
		usleep_range(5, 10);

		/* bit'5: force lock bit'2: improve phy ldo voltage */
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL2, 0x0000303c);

		usleep_range(100, 110);
		if (pll_rst_cnt++ > pll_rst_max) {
			rx_pr("pll rst error\n");
			break;
		}
		if (log_level & VIDEO_LOG)
			rx_pr("pll init-cableclk=%d,pixelclk=%d,sq=%d\n",
			      rx.clk.cable_clk / MHz,
			      rx.clk.pixel_clk / MHz,
			      hdmirx_rd_top(TOP_MISC_STAT0) & 0x1);
	} while ((!is_tmds_clk_stable()) && is_clk_stable());

	/* data channel reset */
	data = rd_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL0);
	data &= (~(0x7 << 7));
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL0, data);
}

/*
 * aml phy initial
 */
 //For TL1 and TM2 rev1
void aml_phy_init_tl1(void)
{
	aml_phy_cfg_tl1();
	usleep_range(100, 110);
	aml_pll_bw_cfg_tl1();
	usleep_range(100, 110);
	aml_eq_cfg_tl1();
}

void dump_reg_phy_tl1_tm2(void)
{
	rx_pr("PHY Register:\n");
	rx_pr("0xd2(348)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL0));
	rx_pr("0xd3(34c)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL1));
	rx_pr("0xd4(350)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL2));
	rx_pr("0xd5(354)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL3));
	rx_pr("0xd6(358)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL4));
	rx_pr("0xd7(35c)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL0));
	rx_pr("0xd8(360)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL1));
	rx_pr("0xe0(380)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL2));
	rx_pr("0xe1(384)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL3));
	rx_pr("0xe2(388)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL0));
	rx_pr("0xe3(38c)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL1));
	rx_pr("0xe4(390)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL2));
	rx_pr("0xc5(314)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL3));
	rx_pr("0xe5(394)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL0));
	rx_pr("0xe6(398)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1));
	rx_pr("0xe7(39c)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL2));
	rx_pr("0xc6(318)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL3));
	/*rx_pr("0xe8(3a0)=0x%x\n", rd_reg_hhi(HHI_HDMIRX_PHY_ARC_CNTL));*/
	rx_pr("0xee(3b8)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_STAT));
	rx_pr("0xef(3bc)=0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
}

void dump_aml_phy_sts_tl1(void)
{
	u32 val0, val1, val2, data32;

	rx_pr("[PHY info]\n");
	rx_get_error_cnt(&val0, &val1, &val2);
	rx_pr("err cnt- ch0: %d,ch1:%d ch2:%d\n", val0, val1, val2);
	rx_pr("PLL_LCK_STS(tmds valid) = 0x%x\n", hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS));
	rx_pr("MISC_STAT0 sqo = 0x%x\n", hdmirx_rd_top(TOP_MISC_STAT0));
	rx_pr("PHY_DCHD_STAT = 0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
	rx_pr("APLL_CNTL0 = 0x%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL0));
	rx_pr("TMDS_ALIGN_STAT = 0x%x\n", hdmirx_rd_top(TOP_TMDS_ALIGN_STAT));
	rx_pr("all valid = 0x%x\n", aml_get_tmds_valid_tl1());

	data32 = rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1);
	data32 = data32 & 0xf0ffffff;
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
	rx_pr("0x3bc-0=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		usleep_range(100, 110);
	data32 = ((data32 & 0xf0ffffff) | (0x2 << 24));
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
	rx_pr("0x3bc-2=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
	usleep_range(100, 110);
	data32 = ((data32 & 0xf0ffffff) | (0x4 << 24));
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
	rx_pr("0x3bc-4=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
	usleep_range(100, 110);
	data32 = ((data32 & 0xf0ffffff) | (0x6 << 24));
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
	rx_pr("0x3bc-6=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
	usleep_range(100, 110);
	data32 = ((data32 & 0xf0ffffff) | (0xe << 24));
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
	if (log_level & VIDEO_LOG) {
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("0x3bc-e=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_STAT));
	}
}

void aml_phy_short_bist_tl1(void)
{
	int data32;
	int bist_mode = 3;
	int port;
	int ch0_lock = 0;
	int ch1_lock = 0;
	int ch2_lock = 0;
	int lock_sts = 0;

	rx_pr("bist start\n");
	for (port = 0; port < 3; port++) {
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL0, 0x10034079);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL0, 0x100347f9);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL1, 0x007f8fc0);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL2, 0x02200000);
		usleep_range(5, 10);
		data32 = 0;
		/*selector clock to digital from data ch*/
		data32 |= (1 << port) << 3;
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL3, data32);
		rx_pr("\n port=%x\n", rd_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL3));
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL0, 0x000002a2);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL1, 0x07000000);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL2, 0x1d00cc31);//0x1d004451
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL0, 0x003e714a);//0x002c714a
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1, 0x1e040460);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL2, 0x00018000);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL0, 0x200904f8);
		usleep_range(100, 110);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL0, 0x300904f8);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL1, 0x00000000);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL2, 0x00001108);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL3, 0x10058f30);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL4, 0x090100c0);
		usleep_range(100, 110);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL4, 0x098100c0);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL0, 0x340904f8);
		usleep_range(100, 110);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL0, 0x140904f8);
		usleep_range(5, 10);
		wr_reg_hhi(TL1_HHI_HDMIRX_APLL_CNTL2, 0x00003008);
		usleep_range(100, 110);
		/* Reset */
		data32	= 0x0;
		data32	|=	1 << 8;
		data32	|=	1 << 7;
		/* Configure BIST analyzer before BIST path out of reset */
		hdmirx_wr_top(TOP_SW_RESET, data32);
		usleep_range(5, 10);
		// Configure BIST analyzer before BIST path out of reset
		data32 = 0;
		// [23:22] prbs_ana_ch2_prbs_mode:
		// 0=prbs11; 1=prbs15; 2=prbs7; 3=prbs31.
		data32	|=	bist_mode << 22;
		// [21:20] prbs_ana_ch2_width:3=10-bit pattern
		data32	|=	3 << 20;
		// [   19] prbs_ana_ch2_clr_ber_meter	//0
		data32	|=	1 << 19;
		// [   18] prbs_ana_ch2_freez_ber
		data32	|=	0 << 18;
		// [	17] prbs_ana_ch2_bit_reverse
		data32	|=	1 << 17;
		// [15:14] prbs_ana_ch1_prbs_mode:
		// 0=prbs11; 1=prbs15; 2=prbs7; 3=prbs31.
		data32	|=	bist_mode << 14;
		// [13:12] prbs_ana_ch1_width:3=10-bit pattern
		data32	|=	3 << 12;
		// [	 11] prbs_ana_ch1_clr_ber_meter //0
		data32	|=	1 << 11;
		// [   10] prbs_ana_ch1_freez_ber
		data32	|=	0 << 10;
		// [	9] prbs_ana_ch1_bit_reverse
		data32	|=	1 << 9;
		// [ 7: 6] prbs_ana_ch0_prbs_mode:
		// 0=prbs11; 1=prbs15; 2=prbs7; 3=prbs31.
		data32	|=	bist_mode << 6;
		// [ 5: 4] prbs_ana_ch0_width:3=10-bit pattern
		data32	|=	3 << 4;
		// [	 3] prbs_ana_ch0_clr_ber_meter	//0
		data32	|=	1 << 3;
		// [	  2] prbs_ana_ch0_freez_ber
		data32	|=	0 << 2;
		// [	1] prbs_ana_ch0_bit_reverse
		data32	|=	1 << 1;
		hdmirx_wr_top(TOP_PRBS_ANA_0,  data32);
		usleep_range(5, 10);
		data32			= 0;
		// [19: 8] prbs_ana_time_window
		data32	|=	255 << 8;
		// [ 7: 0] prbs_ana_err_thr
		data32	|=	0;
		hdmirx_wr_top(TOP_PRBS_ANA_1,  data32);
		usleep_range(5, 10);
		// Configure channel switch
		data32			= 0;
		data32	|=	2 << 28;// [29:28] source_2
		data32	|=	1 << 26;// [27:26] source_1
		data32	|=	0 << 24;// [25:24] source_0
		data32	|=	0 << 22;// [22:20] skew_2
		data32	|=	0 << 16;// [18:16] skew_1
		data32	|=	0 << 12;// [14:12] skew_0
		data32	|=	0 << 10;// [   10] bitswap_2
		data32	|=	0 << 9;// [    9] bitswap_1
		data32	|=	0 << 8;// [    8] bitswap_0
		data32	|=	0 << 6;// [    6] polarity_2
		data32	|=	0 << 5;// [    5] polarity_1
		data32	|=	0 << 4;// [    4] polarity_0
		data32	|=	0;// [	  0] enable
		hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32);
		usleep_range(5, 10);
		// Configure BIST generator
		data32		   = 0;
		data32	|=	0 << 8;// [    8] bist_loopback
		data32	|=	3 << 5;// [ 7: 5] decoup_thresh
		// [ 4: 3] prbs_gen_mode:0=prbs11; 1=prbs15; 2=prbs7; 3=prbs31.
		data32	|=	bist_mode << 3;
		data32	|=	3 << 1;// [ 2: 1] prbs_gen_width:3=10-bit.
		data32	|=	0;// [	 0] prbs_gen_enable
		hdmirx_wr_top(TOP_PRBS_GEN, data32);
		usleep_range(100, 110);
		/* Reset */
		data32	= 0x0;
		data32	&=	~(1 << 8);
		data32	&=	~(1 << 7);
		/* Configure BIST analyzer before BIST path out of reset */
		hdmirx_wr_top(TOP_SW_RESET, data32);
		usleep_range(100, 110);
		// Configure channel switch
		data32 = 0;
		data32	|=	2 << 28;// [29:28] source_2
		data32	|=	1 << 26;// [27:26] source_1
		data32	|=	0 << 24;// [25:24] source_0
		data32	|=	0 << 22;// [22:20] skew_2
		data32	|=	0 << 16;// [18:16] skew_1
		data32	|=	0 << 12;// [14:12] skew_0
		data32	|=	0 << 10;// [   10] bitswap_2
		data32	|=	0 << 9;// [    9] bitswap_1
		data32	|=	0 << 8;// [    8] bitswap_0
		data32	|=	0 << 6;// [    6] polarity_2
		data32	|=	0 << 5;// [    5] polarity_1
		data32	|=	0 << 4;// [    4] polarity_0
		data32	|=	1;// [	  0] enable
		hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32);

		/* Configure BIST generator */
		data32			= 0;
		/* [	8] bist_loopback */
		data32	|=	0 << 8;
		/* [ 7: 5] decoup_thresh */
		data32	|=	3 << 5;
		// [ 4: 3] prbs_gen_mode:
		// 0=prbs11; 1=prbs15; 2=prbs7; 3=prbs31.
		data32	|=	bist_mode << 3;
		/* [ 2: 1] prbs_gen_width:3=10-bit. */
		data32	|=	3 << 1;
		/* [	0] prbs_gen_enable */
		data32	|=	1;
		hdmirx_wr_top(TOP_PRBS_GEN, data32);

		/* PRBS analyzer control */
		hdmirx_wr_top(TOP_PRBS_ANA_0, 0xf6f6f6);
		usleep_range(100, 110);
		hdmirx_wr_top(TOP_PRBS_ANA_0, 0xf2f2f2);

		//if ((hdmirx_rd_top(TOP_PRBS_GEN) & data32) != 0)
			//return;
		usleep_range(5000, 5050);

		/* Check BIST analyzer BER counters */
		if (port == 0)
			rx_pr("BER_CH0 = %x\n", hdmirx_rd_top(TOP_PRBS_ANA_BER_CH0));
		else if (port == 1)
			rx_pr("BER_CH1 = %x\n", hdmirx_rd_top(TOP_PRBS_ANA_BER_CH1));
		else if (port == 2)
			rx_pr("BER_CH2 = %x\n", hdmirx_rd_top(TOP_PRBS_ANA_BER_CH2));

		/* check BIST analyzer result */
		lock_sts = hdmirx_rd_top(TOP_PRBS_ANA_STAT) & 0x3f;
		rx_pr("ch%dsts=0x%x\n", port, lock_sts);
		if (port == 0) {
			ch0_lock = lock_sts & 3;
			if (ch0_lock == 1)
				rx_pr("ch0 PASS\n");
			else
				rx_pr("ch0 NG\n");
		}
		if (port == 1) {
			ch1_lock = (lock_sts >> 2) & 3;
			if (ch1_lock == 1)
				rx_pr("ch1 PASS\n");
			else
				rx_pr("ch1 NG\n");
		}
		if (port == 2) {
			ch2_lock = (lock_sts >> 4) & 3;
			if (ch2_lock == 1)
				rx_pr("ch2 PASS\n");
			else
				rx_pr("ch2 NG\n");
		}
		usleep_range(1000, 1010);
	}
	lock_sts = ch0_lock | (ch1_lock << 2) | (ch2_lock << 4);
	if (lock_sts == 0x15)/* lock_sts == b'010101' is PASS*/
		rx_pr("bist_test PASS\n");
	else
		rx_pr("bist_test FAIL\n");
	rx_pr("bist done\n");
}

bool aml_get_tmds_valid_tl1(void)
{
	unsigned int tmdsclk_valid;
	unsigned int sqofclk;
	/* unsigned int pll_lock; */
	unsigned int tmds_align;
	u32 ret;

	/* digital tmds valid depends on PLL lock from analog phy. */
	/* it is not necessary and T7 has not it */
	/* tmds_valid = hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS) & 0x01; */
	sqofclk = hdmirx_rd_top(TOP_MISC_STAT0) & 0x1;
	tmdsclk_valid = is_tmds_clk_stable();
	tmds_align = hdmirx_rd_top(TOP_TMDS_ALIGN_STAT) & 0x3f000000;
	if (sqofclk && tmdsclk_valid && tmds_align == 0x3f000000) {
		ret = 1;
	} else {
		if (log_level & VIDEO_LOG) {
			rx_pr("sqo:%x,tmdsclk_valid:%x,align:%x\n",
			      sqofclk, tmdsclk_valid, tmds_align);
			rx_pr("cable clk0:%d\n", rx.clk.cable_clk);
			rx_pr("cable clk1:%d\n", rx_get_clock(TOP_HDMI_CABLECLK));
		}
		ret = 0;
	}
	return ret;
}

void aml_phy_power_off_tl1(void)
{
	/* pll power down */
	wr_reg_hhi_bits(TL1_HHI_HDMIRX_APLL_CNTL0, MSK(2, 28), 2);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL0, 0x32037800);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL1, 0x1000000);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL2, 0x62208002);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL3, 0x7);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL0, 0x1e);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHA_CNTL1, 0x10000800);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL0, 0x200000);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_DCHD_CNTL1, 0x0);
}

void aml_phy_switch_port_tl1(void)
{
	u32 data32;

	/* reset and select data port */
	data32 = rd_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL3);
	data32 &= (~(0x7 << 6));
	data32 |= ((1 << rx.port) << 6);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL3, data32);
	data32 &= (~(1 << 11));
	/* release reset */
	data32 |= (1 << 11);
	wr_reg_hhi(TL1_HHI_HDMIRX_PHY_MISC_CNTL3, data32);
	udelay(5);

	data32 = 0;
	data32 |= rx.port << 2;
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);
}

