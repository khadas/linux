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
#include <linux/arm-smccc.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>

/* Local Include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_wrapper.h"
/* FT trim flag:1-valid, 0-not valid */
bool rterm_trim_flag_t7;
/* FT trim value 4 bits */
u32 rterm_trim_val_t7;

/* for T7 */
static const u32 phy_misci_t7[][4] = {
		 /* 0x05	 0x06	 0x07	 0x08 */
	{	 /* 24~45M */
		0x200773ff, 0x000c01c0, 0x0, 0x00000817,
	},
	{	 /* 45~74.5M */
		0x200773ff, 0x000c01c0, 0x0, 0x00000817,
	},
	{	 /* 77~155M */
		0x200773ff, 0x000c01c0, 0x0, 0x00000817,
	},
	{	 /* 155~340M */
		0x200773ff, 0x000c01c0, 0x0, 0x00000817,
	},
	{	 /* 340~525M */
		0x200773ff, 0x000c01c0, 0x0, 0x00000817,
	},
	{	 /* 525~600M */
		0x200773ff, 0x000c01c0, 0x0, 0x00000817,
	},
};

static const u32 phy_dcha_t7[][4] = {
		/*  0x09	0x0a	 0x0b	0x0c */
	{	 /* 24~45M */
		0x17270888, 0x45ff4b58, 0x01ff28d4, 0x817,
	},
	{	 /* 45~74.5M */
		0x17270888, 0x45ff4b58, 0x01ff28d4, 0x817,
	},
	{	 /* 77~155M */
		0x17270fff, 0x45ff4b58, 0x01ff28c9, 0x630,
	},
	{	 /* 155~340M */
		0x13270fff, 0x45ff4b58, 0x01ff28c9, 0x630,
	},
	{	 /* 340~525M */
		0x10210fff, 0x45ff4b58, 0x01ff2a21, 0x1080,
	},
	{	 /* 525~600M */
		0x10210fff, 0x45ff4b58, 0x01ff2a21, 0x1080,
	},
};

static const u32 phy_dchd_t7[][5] = {
		/*  0x0d	0x0e	0x0f	0x10	0x11 */
	{	 /* 24~45M */
		0xe3372103, 0x40000000, 0x0d4aaaaa, 0x10000000, 0x3001,
	},
	{	 /* 45~74.5M */
		0xe3372103, 0x40000000, 0x0d4aaaaa, 0x10000000, 0x3001,
	},
	{	 /* 77~155M */
		0xe3372103, 0x40009000, 0x0d422222, 0x10000000, 0x3001,
	},
	{	 /* 155~340M */
		0xe3372903, 0x40009000, 0x0d422222, 0x10000000, 0x3001,
	},
	{	 /* 340~525M */
		0xe3372903, 0x4004f000, 0x0d422222, 0x10000000, 0x3001,
	},
	{	 /* 525~600M */
		0xe3372903, 0x4004f000, 0x0d422222, 0x10000000, 0x3001,
	},
};

struct apll_param apll_tab_t7[] = {
	/*od for tmds: 2/4/8/16/32*/
	/*od2 for audio: 1/2/4/8/16*/
	/* bw M, N, od, od_div, od2, od2_div, aud_div */
	/* {PLL_BW_0, 160, 1, 0x5, 32,0x2, 8, 2}, */
	{PLL_BW_0, 160, 1, 0x5, 32, 0x1, 8, 3},/* 16 x 27 */
	/* {PLL_BW_1, 80, 1, 0x4,	 16, 0x2, 8, 1}, */
	{PLL_BW_1, 80, 1, 0x4, 16, 0x0, 8, 3},/* 8 x 74 */
	/* {PLL_BW_2, 40, 1, 0x3, 8,	 0x2, 8, 0}, */
	{PLL_BW_2, 40, 1, 0x3, 8,	  0x0, 8, 2}, /* 4 x 148 */
	/* {PLL_BW_3, 40, 2, 0x2, 4,	 0x1, 4, 0}, */
	{PLL_BW_3, 40, 2, 0x2, 4,	  0x0, 4, 1},/* 2 x 297 */
	/* {PLL_BW_4, 40, 1, 0x1, 2,	 0x0, 2, 0}, */
	{PLL_BW_4, 40, 1, 0x1, 2,	  0x0, 2, 0},/* 594 */
	{PLL_BW_NULL, 40, 1, 0x3, 8,	 0x2, 8, 0},
};

/*Decimal to Gray code */
unsigned int decimaltogray_t7(unsigned int x)
{
	return x ^ (x >> 1);
}

/* Gray code to Decimal */
unsigned int graytodecimal_t7(unsigned int x)
{
	unsigned int y = x;

	while (x >>= 1)
		y ^= x;
	return y;
}

void aml_pll_bw_cfg_t7(void)
{
	u32 idx = rx.phy.phy_bw;
	u32 M, N;
	u32 od, od_div;
	u32 od2, od2_div;
	u32 bw = rx.phy.pll_bw;
	u32 vco_clk;
	u32 data, data2;
	u32 cableclk = rx.clk.cable_clk / KHz;
	int pll_rst_cnt = 0;

	od_div = apll_tab_t7[bw].od_div;
	od = apll_tab_t7[bw].od;
	if (rx.aml_phy.osc_mode && idx == PHY_BW_5) {
		M = 0xF7;
		/* sel osc as pll clock */
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_MISC_CNTL0, _BIT(29), 0);
		/* t7: select tmds_clk from tclk or tmds_ch_clk */
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_MISC_CNTL3, _BIT(12), 1);
	} else {
		M = apll_tab_t7[bw].M;
	}
	N = apll_tab_t7[bw].N;
	if (rx.aml_phy.pll_div && idx == PHY_BW_5) {
		M *= rx.aml_phy.pll_div;
		N *= rx.aml_phy.pll_div;
	}
	od2_div = apll_tab_t7[bw].od2_div;
	od2 = apll_tab_t7[bw].od2;

	if (rx.aml_phy.phy_bwth) {
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_LKDET_EN, 0);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_RSTB, 0);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_EQ_RSTB, 0);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, EQ_EN, 1);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_HOLD, 0);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_FR_EN, 0);
		usleep_range(100, 110);
		/* 0x09 */
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL0, phy_dcha_t7[idx][0]);
		/* 0x0b */
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2, MSK(10, 0),
				      (phy_dcha_t7[idx][2] & 0x3ff));
		/* 0x0e */
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL1, EQ_BYP_VAL1,
				      (phy_dchd_t7[idx][1] >> 12) & 0x1f);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL1, EQ_BYP_VAL2,
				      (phy_dchd_t7[idx][1] >> 17) & 0x1f);
		/* 0x0c */
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL3, phy_dcha_t7[idx][3]);
		/* 0x0d */
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, EQ_ADP_MODE,
				      (phy_dchd_t7[idx][0] >> 10) & 0x3);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0,
				      MSK(2, 6), (phy_dchd_t7[idx][0] >> 6) & 0x3);
		/* 0xb */
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2,
				      MSK(2, 25), (phy_dcha_t7[idx][2] >> 25) & 0x3);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
				      MSK(20, 0), (phy_dchd_t7[idx][2] & 0x1fffff));
		rx_pr("phy_bw\n");
	}

	/* over sample control */
	if (rx.aml_phy.os_rate == 0) {
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0,
				      MSK(2, 6), rx.aml_phy.os_rate);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2,
				      MSK(2, 25), rx.aml_phy.os_rate);
	} else if (rx.aml_phy.os_rate == 1) {
		od -= 1;
		/* 0x0c[13:0] + 0x0b[9:0] change to 3G setting ,t7*/
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL3, 0x630);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2,
				      MSK(10, 0), 0xc9);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0,
				      MSK(2, 6), rx.aml_phy.os_rate);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2,
				      MSK(2, 25), rx.aml_phy.os_rate);
	} else if (rx.aml_phy.os_rate == 2) {
		od -= 2;
		/* 0x0c[13:0] + 0x0b[9:0] change to 3G setting ,t7*/
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL3, 0x630);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2,
				      MSK(10, 0), 0xc9);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0,
				      MSK(2, 6), rx.aml_phy.os_rate);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2,
				      MSK(2, 25), rx.aml_phy.os_rate);
	} else if (rx.aml_phy.os_rate == 3 && idx <= PHY_BW_1) {
		od -= 2;
		/* 0x0c[13:0] + 0x0b[9:0] change to 3G setting ,t7*/
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL3, 0x630);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2, MSK(10, 0), 0xc9);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, MSK(2, 6), 0x2);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2, MSK(2, 25), 0x2);
	}
	rx.phy.aud_div = apll_tab_t7[bw].aud_div;
	vco_clk = (cableclk * M) / N; /*KHz*/
	if ((vco_clk < (2970 * KHz)) || (vco_clk > (6000 * KHz))) {
		if (log_level & VIDEO_LOG)
			rx_pr("err: M=%d,N=%d,vco_clk=%d\n", M, N, vco_clk);
	}

	do {
		/*cntl0 M <7:0> N<14:10>*/
		data = 0x00090400 & 0xffff8300;
		data |= M;
		data |= (N << 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, data | 0x20000000);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, data | 0x30000000);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL1, 0x00000000);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL2, 0x00001118);
		usleep_range(5, 10);
		if (idx <= PHY_BW_2)
			/* sz sast dvd flashing screen issue,*/
			/* There is a large jitter between 1MHz*/
			/* and 2MHz of this DVD source.*/
			/* Solution: increase pll Bandwidth*/
			data2 = 0x10058f30 | od2;
		else
			data2 = 0x10058f00 | od2;
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL3, data2);
		data2 = 0x080130c0;
		data2 |= (od << 24);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL4, data2);
		usleep_range(5, 10);
		/*apll_vctrl_mon_en*/
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL4, data2 | 0x00800000);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, data | 0x34000000);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, data | 0x14000000);
		usleep_range(5, 10);

		/* bit'5: force lock bit'2: improve phy ldo voltage */
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL2, 0x0000301c);

		usleep_range(100, 110);
		if (pll_rst_cnt++ > pll_rst_max) {
			if (log_level & VIDEO_LOG)
				rx_pr("pll rst error\n");
			break;
		}
		if (log_level & VIDEO_LOG) {
			//rx_pr("pll init-cableclk=%d,pixelclk=%d,\n",
			     // rx.clk.cable_clk / MHz,
			     // meson_clk_measure(44) / MHz);
			rx_pr("sq=%d,pll_lock=%d",
			      hdmirx_rd_top(TOP_MISC_STAT0) & 0x1,
			      aml_phy_pll_lock());
		}
	} while (!is_tmds_clk_stable() && is_clk_stable() && !aml_phy_pll_lock());
	rx_pr("pll done\n");
	/* t7 debug */
	/* manual VGA mode for debug */
	if (rx.aml_phy.vga_gain <= 0xfff) {
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL0, MSK(12, 0),
				      (decimaltogray_t7(rx.aml_phy.vga_gain & 0xf) |
				      (decimaltogray_t7(rx.aml_phy.vga_gain >> 4 & 0xf) << 4) |
				      (decimaltogray_t7(rx.aml_phy.vga_gain >> 8 & 0xf) << 8)));
	}
	/* manual EQ mode for debug */
	if (rx.aml_phy.eq_stg1 < 0x1f) {
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL1,
				      EQ_BYP_VAL1, rx.aml_phy.eq_stg1 & 0x1f);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, EQ_ADP_MODE, 0);
	}
	if (rx.aml_phy.eq_stg2 < 0x1f) {
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL1,
				      EQ_BYP_VAL2, rx.aml_phy.eq_stg2 & 0x1f);
	}
	/* cdr mode */
	if (rx.aml_phy.cdr_mode) {
		/* cdr 10bit mode */
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_MODE, 1);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_PH_DIV, 0x3);
	} else {
		/* cdr 4bit mode */
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_MODE, 0);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_PH_DIV, 0x4);
	}
	if (rx.aml_phy.tap2_byp && rx.phy.phy_bw >= PHY_BW_3)
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, _BIT(15), 1);
}

int get_tap2_t7(int val)
{
	if ((val >> 4) == 0)
		return val;
	else
		return (0 - (val & 0xf));
}

bool is_dfe_sts_ok_t7(void)
{
	u32 data32;
	u32 dfe0_tap2, dfe1_tap2, dfe2_tap2;
	u32 dfe0_tap3, dfe1_tap3, dfe2_tap3;
	u32 dfe0_tap4, dfe1_tap4, dfe2_tap4;
	u32 dfe0_tap5, dfe1_tap5, dfe2_tap5;
	u32 dfe0_tap6, dfe1_tap6, dfe2_tap6;
	u32 dfe0_tap7, dfe1_tap7, dfe2_tap7;
	u32 dfe0_tap8, dfe1_tap8, dfe2_tap8;
	bool ret = true;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x2);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap2 = data32 & 0xf;
	dfe1_tap2 = (data32 >> 8) & 0xf;
	dfe2_tap2 = (data32 >> 16) & 0xf;
	if (dfe0_tap2 >= 8 ||
	    dfe1_tap2 >= 8 ||
	    dfe2_tap2 >= 8)
		ret = false;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x3);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap3 = data32 & 0x7;
	dfe1_tap3 = (data32 >> 8) & 0x7;
	dfe2_tap3 = (data32 >> 16) & 0x7;
	if (dfe0_tap3 >= 6 ||
	    dfe1_tap3 >= 6 ||
	    dfe2_tap3 >= 6)
		ret = false;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x4);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap4 = data32 & 0x7;
	dfe1_tap4 = (data32 >> 8) & 0x7;
	dfe2_tap4 = (data32 >> 16) & 0x7;
	if (dfe0_tap4 >= 6 ||
	    dfe1_tap4 >= 6 ||
	    dfe2_tap4 >= 6)
		ret = false;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x5);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap5 = data32 & 0x7;
	dfe1_tap5 = (data32 >> 8) & 0x7;
	dfe2_tap5 = (data32 >> 16) & 0x7;
	if (dfe0_tap5 >= 6 ||
	    dfe1_tap5 >= 6 ||
	    dfe2_tap5 >= 6)
		ret = false;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x6);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap6 = data32 & 0x7;
	dfe1_tap6 = (data32 >> 8) & 0x7;
	dfe2_tap6 = (data32 >> 16) & 0x7;
	if (dfe0_tap6 >= 6 ||
	    dfe1_tap6 >= 6 ||
	    dfe2_tap6 >= 6)
		ret = false;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x7);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap7 = data32 & 0x7;
	dfe0_tap8 = (data32 >> 4) & 0x7;
	dfe1_tap7 = (data32 >> 8) & 0x7;
	dfe1_tap8 = (data32 >> 12) & 0x7;
	dfe2_tap7 = (data32 >> 16) & 0x7;
	dfe2_tap8 = (data32 >> 20) & 0x7;
	if (dfe0_tap7 >= 6 ||
	    dfe1_tap7 >= 6 ||
	    dfe2_tap7 >= 6 ||
	    dfe0_tap8 >= 6 ||
	    dfe1_tap8 >= 6 ||
	    dfe2_tap8 >= 6)
		ret = false;

	return ret;
}

/* long cable detection for <3G need to be change */
void aml_phy_long_cable_det_t7(void)
{
	int tap2_0, tap2_1, tap2_2;
	int tap2_max = 0;
	u32 data32 = 0;

	if (rx.phy.phy_bw > PHY_BW_3)
		return;
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x2);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	tap2_0 = get_tap2_t7(data32 & 0x1f);
	tap2_1 = get_tap2_t7(((data32 >> 8) & 0x1f));
	tap2_2 = get_tap2_t7(((data32 >> 16) & 0x1f));
	if (rx.phy.phy_bw == PHY_BW_2) {
		/*disable DFE*/
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_RSTB, 0);
		tap2_max = 6;
	} else if (rx.phy.phy_bw == PHY_BW_3) {
		tap2_max = 10;
	}
	if ((tap2_0 + tap2_1 + tap2_2) >= tap2_max) {
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL1, EQ_BYP_VAL1, 0x12);
		usleep_range(10, 20);
		rx_pr("long cable\n");
	}
}

/* aml_hyper_gain_tuning */
void aml_hyper_gain_tuning_t7(void)
{
	u32 data32;
	u32 tap0, tap1, tap2;
	u32 hyper_gain_0, hyper_gain_1, hyper_gain_2;

	/* use HYPER_GAIN calibartion instead of vga */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x0);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);

	tap0 = data32 & 0xff;
	tap1 = (data32 >> 8) & 0xff;
	tap2 = (data32 >> 16) & 0xff;
	if (tap0 < 0x12)
		hyper_gain_0 = 1;
	else
		hyper_gain_0 = 0;
	if (tap1 < 0x12)
		hyper_gain_1 = 1;
	else
		hyper_gain_1 = 0;
	if (tap2 < 0x12)
		hyper_gain_2 = 1;
	else
		hyper_gain_2 = 0;
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL0,
			      HYPER_GAIN,
			      (hyper_gain_0 | (hyper_gain_1 << 1) | (hyper_gain_2 << 2)));
}

void aml_eq_retry_t7(void)
{
	u32 data32 = 0;
	u32 eq_boost0, eq_boost1, eq_boost2;

	if (rx.phy.phy_bw >= PHY_BW_3) {
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x3);
		usleep_range(100, 110);
		data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
		eq_boost0 = data32 & 0x1f;
		eq_boost1 = (data32 >> 8)  & 0x1f;
		eq_boost2 = (data32 >> 16)	& 0x1f;
		if (eq_boost0 == 0 || eq_boost0 == 31 ||
		    eq_boost1 == 0 || eq_boost1 == 31 ||
		    eq_boost2 == 0 || eq_boost2 == 31) {
			rx_pr("eq_retry:%d-%d-%d\n", eq_boost0, eq_boost1, eq_boost2);
			hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, EQ_EN, 1);
			hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_EQ_RSTB, 0x2);
			usleep_range(100, 110);
			hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_EQ_RSTB, 3);
			usleep_range(10000, 10100);
			if (rx.aml_phy.eq_hold)
				hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, EQ_EN, 0);
		}
	}
}

void aml_dfe_en_t7(void)
{
	if (rx.aml_phy.dfe_en) {
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_EN, 1);
		if (rx.aml_phy.eq_hold)
			hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, EQ_EN, 0);
		if (rx.aml_phy.eq_retry)
			aml_eq_retry_t7();
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_RSTB, 0);
		usleep_range(10, 20);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
				      DFE_RSTB, 1);
		usleep_range(10, 20);
		if (rx.aml_phy.dfe_hold)
			hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
					      DFE_HOLD, 1);
		rx_pr("dfe\n");
	}
}

/* phy offset calibration based on different chip and board */
void aml_phy_offset_cal_t7(void)
{
	u32 data32;
	u32 idx = PHY_BW_2;

	data32 = phy_misci_t7[idx][0];
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL0, data32);
	usleep_range(5, 10);

	data32 = phy_misci_t7[idx][1];
	if (rterm_trim_flag_t7)
		data32 = ((data32 & (~((0xf << 12) | 0x1))) |
			(rterm_trim_val_t7 << 12) | rterm_trim_flag_t7);
	/* step2-0xd8 */
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL1, data32);
	/*step2-0xe0*/
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL2, phy_misci_t7[idx][2]);
	/*step2-0xe1*/
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL3, phy_misci_t7[idx][3]);
	/*step2-0xe2*/
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL0,
			 phy_dcha_t7[idx][0]);
	/*step2-0xe3*/
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL1,
			 phy_dcha_t7[idx][1]);
	/*step2-0xe4*/
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL2,
			 phy_dcha_t7[idx][2]);
	/*step2-0xe5*/
	data32 = phy_dchd_t7[idx][0];
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL0,
			 data32);
	/*step2-0xe6*/
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL1,
			 phy_dchd_t7[idx][1]);
	/*step2-0xe7*/
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL2,
			 phy_dchd_t7[idx][2]);
	/*step2-0xc5*/
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL3,
			 phy_dcha_t7[idx][3]);
	/*step2-0xc6*/
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL3,
			 phy_dchd_t7[idx][3]);
	/*0xc7*/
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL4,
			 phy_dchd_t7[idx][4]);

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0,
			      CDR_FR_EN, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2,
			      AFE_EN, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
			      DFE_RST, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0,
			      CDR_EQ_RSTB, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0,
			      CDR_LKDET_EN, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_MISC_CNTL0,
			      PLL_SRC_SEL, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_MISC_CNTL0,
			      SQO_GATE, 1);

	/* PLL */
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x200904f8);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x300904f8);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL1, 0x00000000);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL2, 0x00001108);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL3, 0x10058f30);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL4, 0x0b0100c0);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL4, 0x0b8100c0);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x340904f8);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x140904f8);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL2, 0x00000308);
	usleep_range(10, 20);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL1,
			      DFE_OFSETCAL_START, 1);
	usleep_range(100, 110);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL1,
			      OFST_CAL_START, 1);
	usleep_range(100, 110);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0,
		CDR_EQ_RSTB, 3);
	usleep_range(10, 20);

	rx_pr("ofst cal\n");
}

/* hardware eye monitor */
void aml_eq_eye_monitor_t7(void)
{
	u32 data32;
	u32 err_cnt0, err_cnt1, err_cnt2;
	u32 scan_state0, scan_state1, scan_state2;
	u32 positive_eye_height0, positive_eye_height1, positive_eye_height2;
	u32 negative_eye_height0, negative_eye_height1, negative_eye_height2;
	u32 left_eye_width0, left_eye_width1, left_eye_width2;
	u32 right_eye_width0, right_eye_width1, right_eye_width2;

	/* hold dfe tap1~tap8 */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
			      DFE_HOLD, 1);
	usleep_range(10, 20);
	/* disable hw scan mode */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4,
			      EYE_EN_HW_SCAN, 0);
	/* disable eye monitor */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4,
			      EYE_MONITOR_EN, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2,
			      EYE_MONITOR_EN1, 0);
	/* tb timer select for error accumulating */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4,
			      MSK(2, 12), 0x3);
	/* enable eye monitor */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4,
			      EYE_MONITOR_EN, 1);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2,
			      EYE_MONITOR_EN1, 1);

	/* enable hw scan mode */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4,
			      EYE_EN_HW_SCAN, 1);

	/* wait for scan done */
	usleep_range(rx.aml_phy.eye_delay, rx.aml_phy.eye_delay + 100);

	/* Check status */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4,
			      EYE_STATUS_EN, 1);
	/* error cnt */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
			      EYE_STATUS, ERROR_CNT);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	err_cnt0 = data32 & 0xff;
	err_cnt1 = (data32 >> 8) & 0xff;
	err_cnt2 = (data32 >> 16) & 0xff;
	/* scan state */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
			      EYE_STATUS, SCAN_STATE);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	scan_state0 = data32 & 0xff;
	scan_state1 = (data32 >> 8) & 0xff;
	scan_state2 = (data32 >> 16) & 0xff;
	/* positive eye height  */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
			      EYE_STATUS, POSITIVE_EYE_HEIGHT);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	positive_eye_height0 = data32 & 0xff;
	positive_eye_height1 = (data32 >> 8) & 0xff;
	positive_eye_height2 = (data32 >> 16) & 0xff;
	/* positive eye height  */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
			      EYE_STATUS, NEGATIVE_EYE_HEIGHT);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	negative_eye_height0 = data32 & 0xff;
	negative_eye_height1 = (data32 >> 8) & 0xff;
	negative_eye_height2 = (data32 >> 16) & 0xff;
	/* left eye width */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
			      EYE_STATUS, LEFT_EYE_WIDTH);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	left_eye_width0 = data32 & 0xff;
	left_eye_width1 = (data32 >> 8) & 0xff;
	left_eye_width2 = (data32 >> 16) & 0xff;
	/* left eye width */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
			      EYE_STATUS, RIGHT_EYE_WIDTH);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	right_eye_width0 = data32 & 0xff;
	right_eye_width1 = (data32 >> 8) & 0xff;
	right_eye_width2 = (data32 >> 16) & 0xff;

	/* exit eye monitor scan mode */
	/* disable hw scan mode */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4,
			      EYE_EN_HW_SCAN, 0);
	/* disable eye monitor */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4,
			      EYE_MONITOR_EN, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL2,
			      EYE_MONITOR_EN1, 0);
	usleep_range(10, 20);
	/* release dfe */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2,
			      DFE_HOLD, 0);

	rx_pr("eye monitor status:\n");
	rx_pr("          error cnt[%5d, %5d, %5d]\n",
	      err_cnt0, err_cnt1, err_cnt2);
	rx_pr("         scan state[%5d, %5d, %5d]\n",
	      scan_state0, scan_state1, scan_state2);
	rx_pr("positive eye height[%5d, %5d, %5d]\n",
	      positive_eye_height0, positive_eye_height1, positive_eye_height2);
	rx_pr("negative eye height[%5d, %5d, %5d]\n",
	      negative_eye_height0, negative_eye_height1, negative_eye_height2);
	rx_pr("     left eye width[%5d, %5d, %5d]\n",
	      left_eye_width0, left_eye_width1, left_eye_width2);
	rx_pr("	   right eye width[%5d, %5d, %5d]\n",
	      right_eye_width0, right_eye_width1, right_eye_width2);
}

void get_eq_val_t7(void)
{
	u32 data32 = 0;
	u32 eq_boost0, eq_boost1, eq_boost2;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x3);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	eq_boost0 = data32 & 0x1f;
	eq_boost1 = (data32 >> 8)  & 0x1f;
	eq_boost2 = (data32 >> 16)      & 0x1f;
	rx_pr("eq:%d-%d-%d\n", eq_boost0, eq_boost1, eq_boost2);
}

/* check eq_boost1 & tap0 status */
bool is_eq1_tap0_err(void)
{
	u32 data32 = 0;
	u32 eq0, eq1, eq2;
	u32 tap0, tap1, tap2;
	u32 eq_avr, tap0_avr;
	bool ret = false;

	if (rx.phy.phy_bw < PHY_BW_5)
		return ret;
	/* get eq_boost1 val */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x3);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	eq0 = data32 & 0x1f;
	eq1 = (data32 >> 8)  & 0x1f;
	eq2 = (data32 >> 16)      & 0x1f;
	eq_avr =  (eq0 + eq1 + eq2) / 3;
	if (log_level & EQ_LOG)
		rx_pr("eq0=0x%x, eq1=0x%x, eq2=0x%x avr=0x%x\n",
			eq0, eq1, eq2, eq_avr);

	/* get tap0 val */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x0);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	tap0 = data32 & 0xff;
	tap1 = (data32 >> 8) & 0xff;
	tap2 = (data32 >> 16) & 0xff;
	tap0_avr = (tap0 + tap1 + tap2) / 3;
	if (log_level & EQ_LOG)
		rx_pr("tap0=0x%x, tap1=0x%x, tap2=0x%x avr=0x%x\n",
			tap0, tap1, tap2, tap0_avr);
	if (eq_avr >= 21 && tap0_avr >= 40)
		ret = true;

	return ret;
}

void aml_eq_cfg_t7(void)
{
	/* dont need to run eq if no sqo_clk or pll not lock */
	if (!aml_phy_pll_lock())
		return;
	/* step10 */
	/* cdr_lkdet_en(0xe5[28])dfe_rstb(0xe7[26])), */
	/* eq_adp_stg<1:0>(0xe5[9:8])b01, cdr_rstb(0xe5[25]), */
	/* eq_rstb(0xe5[24]) */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_LKDET_EN, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_RSTB, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_EQ_RSTB, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, EQ_EN, 1);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_HOLD, 0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_FR_EN, 0);
	usleep_range(100, 110);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_EQ_RSTB, 3);
	if (rx.aml_phy.cdr_fr_en) {
		udelay(rx.aml_phy.cdr_fr_en);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_FR_EN, 1);
	}
	usleep_range(10000, 10100);
	get_eq_val_t7();
	/*if (rx.aml_phy.eq_retry)*/
		/*aml_eq_retry_t7();*/
	if (rx.phy.phy_bw >= PHY_BW_4) {
		/* step12 */
		/* aml_dfe_en(); */
		/* udelay(100); */
	} else if (rx.phy.phy_bw == PHY_BW_3) {//3G
		/* aml_dfe_en(); */
		/*udelay(100);*/
		/*t7 removed, tap1 min value*/
		/* if (rx.aml_phy.tap1_byp) { */
			/* aml_phy_tap1_byp_t7(); */
			/* hdmirx_wr_bits_amlphy( */
				/* HHI_RX_PHY_DCHD_CNTL2, */
				/* DFE_EN, 0); */
		/* } */
		/*udelay(100);*/
		/* hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, */
			/* _BIT(28), 1); */
		if (rx.aml_phy.long_cable)
			;/* aml_phy_long_cable_det_t7(); */
		if (rx.aml_phy.vga_dbg)
			;/* aml_vga_tuning_t7();*/
	} else if (rx.phy.phy_bw == PHY_BW_2) {
		if (rx.aml_phy.long_cable) {
			/*1.5G should enable DFE first*/
			/* aml_dfe_en(); */
			/* long cable detection*/
			/* aml_phy_long_cable_det_t7();*/
			/* 1.5G should disable DFE at the end*/
			/* udelay(100); */
			/* aml_dfe_en(); */
		}
	}
	/* enable dfe for all frequency */
	aml_dfe_en_t7();
	if (is_eq1_tap0_err()) {
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL0, LEQ_BUF_GAIN, 0x0);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL0, LEQ_POLE, 0x2);
		if (log_level & EQ_LOG)
			rx_pr("eq1 & tap0 err, tune eq setting\n");
	}

	/*enable HYPER_GAIN calibration for 6G to fix 2.0 cts HF2-1 issue*/
	aml_hyper_gain_tuning_t7();
	usleep_range(100, 110);
	/*tmds valid det*/
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, CDR_LKDET_EN, 1);
}

void aml_phy_get_trim_val_t7(void)
{
	u32 data32;

	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_MISC_CNTL1);
	/* bit [12: 15]*/
	rterm_trim_val_t7 = (data32 >> 12) & 0xf;
	/* bit'0*/
	rterm_trim_flag_t7 = data32 & 0x1;
	if (rterm_trim_flag_t7)
		rx_pr("rterm trim=0x%x\n", rterm_trim_val_t7);
}

void aml_phy_cfg_t7(void)
{
	u32 idx = rx.phy.phy_bw;
	u32 data32;
	u32 term_value = hdmirx_rd_top(TOP_HPD_PWR5V) & 0x7;

	if (rx.aml_phy.pre_int) {
		rx_pr("\nphy reg init\n");
		if (rx.aml_phy.ofst_en)
			aml_phy_offset_cal_t7();
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL1,
				      OFST_CAL_START, 0);
		usleep_range(100, 110);
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHA_CNTL1,
				      DFE_OFSETCAL_START, 0);
		usleep_range(20, 30);
		data32 = phy_misci_t7[idx][0];
		hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL0, data32);
		usleep_range(5, 10);

		data32 = phy_misci_t7[idx][1];
		if (rterm_trim_flag_t7)
			data32 = ((data32 & (~((0xf << 12) | 0x1))) |
				(rterm_trim_val_t7 << 12) | rterm_trim_flag_t7);
		/* step2-0xd8 */
		hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL1, data32);
		/*step2-0xe0*/
		hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL2, phy_misci_t7[idx][2]);
		/*step2-0xe1*/
		hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL3, phy_misci_t7[idx][3]);
		/*step2-0xe2*/
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL0, phy_dcha_t7[idx][0]);
		/*step2-0xe3*/
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL1, phy_dcha_t7[idx][1]);
		/*step2-0xe4*/
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL2, phy_dcha_t7[idx][2]);
		/*step2-0xe5*/
		data32 = phy_dchd_t7[idx][0];
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL0, data32);
		/*step2-0xe6*/
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL1, phy_dchd_t7[idx][1]);
		/*step2-0xe7*/
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL2, phy_dchd_t7[idx][2]);
		/*step2-0xc5*/
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL3, phy_dcha_t7[idx][3]);
		/*step2-0xc6*/
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL3, phy_dchd_t7[idx][3]);
		/*0xc7*/
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL4, phy_dchd_t7[idx][4]);
		if (!rx.aml_phy.pre_int_en)
			rx.aml_phy.pre_int = 0;
	}
	/*step3 sel port 0xe1[8:6]*/
	/*step4 sq_rst 0xe1[11]=0*/

	data32 = phy_misci_t7[idx][3];
	if (rx.aml_phy.sqrst_en)
		data32 &= ~(1 << 11);
	data32 |= ((1 << rx.port) << 6);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL3, data32);
	usleep_range(5, 10);
	/*step4 sq_rst 0xe1[11]=1*/
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_MISC_CNTL3, _BIT(11), 1);

	/*step4 0xd7*/
	data32 = phy_misci_t7[idx][0];
	data32 &= (~(1 << 10));
	data32 &= (~(0x7 << 7));
	data32 |= term_value;
	data32 &= ~(disable_port_num & 0x07);
	/* terminal en */
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL0, data32);
	usleep_range(5, 10);
	/* res cali block[10] reset */
	/* step4:data channel[7:9] */
	data32 |= 0x1 << 10;
	data32 |= 0x7 << 7;
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL0, data32);

	/* step5 */
	/* set 0xe5[25],disable cdr */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, _BIT(25), 0);
	/* 0xe7(26),disable dfe */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, _BIT(26), 0);
	/* 0xe5[24],disable eq */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, _BIT(24), 0);
	/* 0xe5[28]=0,disable lckdet */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, _BIT(28), 0);
}

 /* For t7 */
void aml_phy_init_t7(void)
{
	aml_phy_cfg_t7();
	usleep_range(10, 20);
	aml_pll_bw_cfg_t7();
	usleep_range(10, 20);
	aml_eq_cfg_t7();
}

void dump_reg_phy_t7(void)
{
	rx_pr("PHY Register:\n");
	rx_pr("apll_cntl0-0x0(0)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_APLL_CNTL0));
	rx_pr("apll_cntl1-0x1(4)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_APLL_CNTL1));
	rx_pr("apll_cntl2-0x2(8)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_APLL_CNTL2));
	rx_pr("apll_cntl3-0x3(c)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_APLL_CNTL3));
	rx_pr("apll_cntl4-0x4(10)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_APLL_CNTL4));

	rx_pr("misc_cntl0-0x5(14)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_MISC_CNTL0));
	rx_pr("misc_cntl1-0x6(18)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_MISC_CNTL1));
	rx_pr("misc_cntl2-0x7(1c)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_MISC_CNTL2));
	rx_pr("misc_cntl3-0x8(20)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_MISC_CNTL3));
	rx_pr("dcha_afe-0x9(24)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_DCHA_CNTL0));
	rx_pr("dcha_dfe-0xa(28)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_DCHA_CNTL1));
	rx_pr("dcha_ctrl-0xb(2c)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_DCHA_CNTL2));
	rx_pr("dcha_pi-0xc(30)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_DCHA_CNTL3));
	rx_pr("dchd_cdr-0xd(34)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_CNTL0));
	rx_pr("dchd_byp-0xe(38)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_CNTL1));
	rx_pr("dchd_dfe-0xf(3c)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_CNTL2));
	rx_pr("dchd_taps-0x10(40)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_CNTL3));
	rx_pr("dchd_eye-0x11(44)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_CNTL4));
	rx_pr("misc_stat-0x12(48)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_MISC_STAT));
	rx_pr("dchd_stat-0x13(4c)=0x%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT));
}

/*
 * rx phy v2 debug
 */
int count_one_bits_t7(u32 value)
{
	int count = 0;

	for (; value != 0; value >>= 1) {
		if (value & 1)
			count++;
	}
	return count;
}

void get_val_t7(char *temp, unsigned int val, int len)
{
	if ((val >> (len - 1)) == 0)
		sprintf(temp, "+%d", val & (~(1 << (len - 1))));
	else
		sprintf(temp, "-%d", val & (~(1 << (len - 1))));
}

void comb_val_t7(char *type, unsigned int val_0, unsigned int val_1,
		 unsigned int val_2, int len)
{
	char out[32], v0_buf[16], v1_buf[16], v2_buf[16];
	int pos = 0;

	get_val_t7(v0_buf, val_0, len);
	get_val_t7(v1_buf, val_1, len);
	get_val_t7(v2_buf, val_2, len);
	pos += snprintf(out + pos, 32 - pos, "%s[", type);
	pos += snprintf(out + pos, 32 - pos, " %s,", v0_buf);
	pos += snprintf(out + pos, 32 - pos, " %s,", v1_buf);
	pos += snprintf(out + pos, 32 - pos, " %s]", v2_buf);
	rx_pr("%s\n", out);
}

void dump_aml_phy_sts_t7(void)
{
	u32 data32;
	u32 terminal;
	u32 ch0_vga, ch1_vga, ch2_vga;
	u32 ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1;
	u32 ch0_eq_boost2, ch1_eq_boost2, ch2_eq_boost2;
	u32 dfe0_tap0, dfe1_tap0, dfe2_tap0;
	u32 dfe0_tap1, dfe1_tap1, dfe2_tap1;
	u32 dfe0_tap2, dfe1_tap2, dfe2_tap2;
	u32 dfe0_tap3, dfe1_tap3, dfe2_tap3;
	u32 dfe0_tap4, dfe1_tap4, dfe2_tap4;
	u32 dfe0_tap5, dfe1_tap5, dfe2_tap5;
	u32 dfe0_tap6, dfe1_tap6, dfe2_tap6;
	u32 dfe0_tap7, dfe1_tap7, dfe2_tap7;
	u32 dfe0_tap8, dfe1_tap8, dfe2_tap8;

	u32 cdr0_code, cdr0_lock, cdr1_code;
	u32 cdr1_lock, cdr2_code, cdr2_lock;
	u32 cdr0_init, cdr1_init, cdr2_init;

	bool pll_lock;
	bool squelch;

	u32 sli0_ofst0, sli1_ofst0, sli2_ofst0;
	u32 sli0_ofst1, sli1_ofst1, sli2_ofst1;
	u32 sli0_ofst2, sli1_ofst2, sli2_ofst2;
	u32 sli0_ofst3, sli1_ofst3, sli2_ofst3;
	u32 sli0_ofst4, sli1_ofst4, sli2_ofst4;
	u32 sli0_ofst5, sli1_ofst5, sli2_ofst5;

	/* status-1 t7:todo*/
	terminal = (hdmirx_rd_amlphy(HHI_RX_PHY_MISC_CNTL1) >> 12) & 0xf;
	//terminal = count_one_bits_t7(data32);
	usleep_range(100, 110);

	/* status-2 t7:todo*/
	data32 = (hdmirx_rd_amlphy(HHI_RX_PHY_DCHA_CNTL0) & 0xfff);
	ch0_vga = graytodecimal_t7(data32 & 0xf);
	ch1_vga = graytodecimal_t7((data32 >> 4) & 0xf);
	ch2_vga = graytodecimal_t7((data32 >> 8) & 0xf);
	usleep_range(100, 110);

	/* status-3 */
	/* disable eye monitor debug function */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x3);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	ch0_eq_boost1 = data32 & 0x1f;
	ch1_eq_boost1 = (data32 >> 8)  & 0x1f;
	ch2_eq_boost1 = (data32 >> 16)  & 0x1f;
	ch0_eq_boost2 = (data32 >> 5)  & 0x7;
	ch1_eq_boost2 = (data32 >> 13)  & 0x7;
	ch2_eq_boost2 = (data32 >> 21)  & 0x7;

	/* status-4 */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x0);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap0 = data32 & 0xff;
	dfe1_tap0 = (data32 >> 8) & 0xff;
	dfe2_tap0 = (data32 >> 16) & 0xff;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap1 = data32 & 0x3f;
	dfe1_tap1 = (data32 >> 8) & 0x3f;
	dfe2_tap1 = (data32 >> 16) & 0x3f;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x2);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap2 = data32 & 0x1f;
	dfe1_tap2 = (data32 >> 8) & 0x1f;
	dfe2_tap2 = (data32 >> 16) & 0x1f;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x3);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap3 = data32 & 0xf;
	dfe1_tap3 = (data32 >> 8) & 0xf;
	dfe2_tap3 = (data32 >> 16) & 0xf;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x4);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap4 = data32 & 0xf;
	dfe1_tap4 = (data32 >> 8) & 0xf;
	dfe2_tap4 = (data32 >> 16) & 0xf;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x5);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap5 = data32 & 0xf;
	dfe1_tap5 = (data32 >> 8) & 0xf;
	dfe2_tap5 = (data32 >> 16) & 0xf;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x6);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap6 = data32 & 0xf;
	dfe1_tap6 = (data32 >> 8) & 0xf;
	dfe2_tap6 = (data32 >> 16) & 0xf;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x7);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	dfe0_tap7 = data32 & 0xf;
	dfe0_tap8 = (data32 >> 4) & 0xf;
	dfe1_tap7 = (data32 >> 8) & 0xf;
	dfe1_tap8 = (data32 >> 12) & 0xf;
	dfe2_tap7 = (data32 >> 16) & 0xf;
	dfe2_tap8 = (data32 >> 20) & 0xf;
	/* status-5: CDR status */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, _BIT(22), 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x2);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	cdr0_code = data32 & 0x7f;
	cdr0_lock = (data32 >> 7) & 0x1;
	cdr1_code = (data32 >> 8) & 0x7f;
	cdr1_lock = (data32 >> 15) & 0x1;
	cdr2_code = (data32 >> 16) & 0x7f;
	cdr2_lock = (data32 >> 23) & 0x1;

	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, _BIT(22), 0x1);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x2);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	cdr0_init = (data32 & 0x7f);
	cdr1_init = (data32 >> 8) & 0x7f;
	cdr2_init = (data32 >> 16) & 0x7f;

	/* status-6: pll lock */
	pll_lock = hdmirx_rd_amlphy(HHI_RX_APLL_CNTL0) >> 31;
	/* status-7: squelch */
	//squelch = rd_reg_hhi(HHI_RX_PHY_MISC_STAT) >> 31;//0320
	squelch = hdmirx_rd_top(TOP_MISC_STAT0) & 0x1;
	/* status-8:slicer offset status */
	/* status-8.0 */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x1);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x0);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	sli0_ofst0 = data32 & 0x3f;
	sli1_ofst0 = (data32 >> 8) & 0x3f;
	sli2_ofst0 = (data32 >> 16) & 0x3f;

	/* status-8.1 */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x1);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	sli0_ofst1 = data32 & 0x3f;
	sli1_ofst1 = (data32 >> 8) & 0x3f;
	sli2_ofst1 = (data32 >> 16) & 0x3f;

	/* status-8.2 */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x1);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x2);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	sli0_ofst2 = data32 & 0x3f;
	sli1_ofst2 = (data32 >> 8) & 0x3f;
	sli2_ofst2 = (data32 >> 16) & 0x3f;

	/* status-8.3 */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x1);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x3);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	sli0_ofst3 = data32 & 0x3f;
	sli1_ofst3 = (data32 >> 8) & 0x3f;
	sli2_ofst3 = (data32 >> 16) & 0x3f;

	/* status-8.4 */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x1);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x4);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	sli0_ofst4 = data32 & 0x3f;
	sli1_ofst4 = (data32 >> 8) & 0x3f;
	sli2_ofst4 = (data32 >> 16) & 0x3f;

	/* status-8.5 */
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x1);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL2, DFE_DBG_STL, 0x5);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
	sli0_ofst5 = data32 & 0x3f;
	sli1_ofst5 = (data32 >> 8) & 0x3f;
	sli2_ofst5 = (data32 >> 16) & 0x3f;

	rx_pr("\nhdmirx phy status:\n");
	rx_pr("pll_lock=%d, squelch=%d, terminal=%d\n", pll_lock, squelch, terminal);
	rx_pr("vga_gain=[%d,%d,%d]\n",
	      ch0_vga, ch1_vga, ch2_vga);
	rx_pr("eq_boost1=[%d,%d,%d]\n",
	      ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1);
	rx_pr("eq_boost2=[%d,%d,%d]\n",
	      ch0_eq_boost2, ch1_eq_boost2, ch2_eq_boost2);

	comb_val_t7("	 dfe_tap0", dfe0_tap0, dfe1_tap0, dfe2_tap0, 8);
	comb_val_t7("	 dfe_tap1", dfe0_tap1, dfe1_tap1, dfe2_tap1, 6);
	comb_val_t7("	 dfe_tap2", dfe0_tap2, dfe1_tap2, dfe2_tap2, 5);
	comb_val_t7("	 dfe_tap3", dfe0_tap3, dfe1_tap3, dfe2_tap3, 4);
	comb_val_t7("	 dfe_tap4", dfe0_tap4, dfe1_tap4, dfe2_tap4, 4);
	comb_val_t7("	 dfe_tap5", dfe0_tap5, dfe1_tap5, dfe2_tap5, 4);
	comb_val_t7("	 dfe_tap6", dfe0_tap6, dfe1_tap6, dfe2_tap6, 4);
	comb_val_t7("	 dfe_tap7", dfe0_tap7, dfe1_tap7, dfe2_tap7, 4);
	comb_val_t7("	 dfe_tap8", dfe0_tap8, dfe1_tap8, dfe2_tap8, 4);

	comb_val_t7("slicer_ofst0", sli0_ofst0, sli1_ofst0, sli2_ofst0, 6);
	comb_val_t7("slicer_ofst1", sli0_ofst1, sli1_ofst1, sli2_ofst1, 6);
	comb_val_t7("slicer_ofst2", sli0_ofst2, sli1_ofst2, sli2_ofst2, 6);
	comb_val_t7("slicer_ofst3", sli0_ofst3, sli1_ofst3, sli2_ofst3, 6);
	comb_val_t7("slicer_ofst4", sli0_ofst4, sli1_ofst4, sli2_ofst4, 6);
	comb_val_t7("slicer_ofst5", sli0_ofst5, sli1_ofst5, sli2_ofst5, 6);

	rx_pr("cdr_code=[%d,%d,%d]\n", cdr0_code, cdr1_code, cdr2_code);
	rx_pr("cdr_lock=[%d,%d,%d]\n", cdr0_lock, cdr1_lock, cdr2_lock);
	comb_val_t7("cdr_int", cdr0_init, cdr1_init, cdr2_init, 7);
}

void aml_phy_short_bist_t7(void)
{
	int data32;
	int bist_mode = 3;
	int port;
	int ch0_lock = 0;
	int ch1_lock = 0;
	int ch2_lock = 0;
	int lock_sts = 0;

	if (rx.aml_phy.long_bist_en)
		rx_pr("t7 long bist start\n");
	else
		rx_pr("t7 short bist start\n");
	for (port = 0; port < 3; port++) {
		hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL0, 0x4003f07f);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL0, 0x4003f3ff);
		usleep_range(5, 10);
		if (rx.aml_phy.long_bist_en)
			hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL1, 0x000c01ce);
		else
			hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL1, 0x000c0fc0);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL2, 0x00000000);
		usleep_range(5, 10);
		data32 = 0;
		/*selector clock to digital from data ch*/
		data32 |= (1 << port) << 3;
		data32 |= 0x7 << 0;
		hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL3, data32);
		usleep_range(5, 10);
		data32 |= 1 << 11;
		hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL3, data32);
		rx_pr("\nport=%x\n", hdmirx_rd_amlphy(HHI_RX_PHY_MISC_CNTL3));
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL0, 0x10210fff);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL1, 0x45ff4b58);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL2, 0x01ff2a21);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL3, 0x00001080);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL0, 0xe0372913);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL1, 0x40000000);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL2, 0x09422222);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL3, 0x10000000);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL4, 0x00000001);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x200904f8);
		usleep_range(100, 110);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x300904f8);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL1, 0x00000000);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL2, 0x00001108);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL3, 0x10058f30);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL4, 0x090100c0);
		usleep_range(100, 110);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL4, 0x098100c0);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x340904f8);
		usleep_range(100, 110);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x140904f8);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_APLL_CNTL2, 0x00003008);
		usleep_range(5, 10);
		hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL0, 0xf3372913);
		usleep_range(5, 10);
		usleep_range(1000, 1100);
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
		usleep_range(1000, 1100);
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
			rx_pr("BER_CH0 = %x\n",
			      hdmirx_rd_top(TOP_PRBS_ANA_BER_CH0));
		else if (port == 1)
			rx_pr("BER_CH1 = %x\n",
			      hdmirx_rd_top(TOP_PRBS_ANA_BER_CH1));
		else if (port == 2)
			rx_pr("BER_CH2 = %x\n",
			      hdmirx_rd_top(TOP_PRBS_ANA_BER_CH2));

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
		usleep_range(1000, 1100);
	}
	lock_sts = ch0_lock | (ch1_lock << 2) | (ch2_lock << 4);
	if (lock_sts == 0x15)/* lock_sts == b'010101' is PASS*/
		rx_pr("bist_test PASS\n");
	else
		rx_pr("bist_test FAIL\n");
	if (rx.aml_phy.long_bist_en)
		rx_pr("long bist done\n");
	else
		rx_pr("short bist done\n");
	if (rx.open_fg)
		rx.aml_phy.pre_int = 1;
}

void aml_phy_get_cdr_int_avr_t7(u32 cdr_ed_mode, u32 *ch0, u32 *ch1, u32 *ch2)
{
	u32 cdr0_code[10], cdr1_code[10], cdr2_code[10];
	u32 data32;
	u32 sum_0 = 0;
	u32 sum_1 = 0;
	u32 sum_2 = 0;
	int i = 0;
	int flag_32 = 0;
	int flag_127 = 0;

	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL1, cdr_ed_mode);//clk0
	usleep_range(100, 110);
	//for(10 times)
	for (; i < 10; i++) {
		data32 = hdmirx_rd_amlphy(HHI_RX_PHY_DCHD_STAT);
		cdr0_code[i] = data32 & 0x7f;
		cdr1_code[i] = (data32 >> 8) & 0x7f;
		cdr2_code[i] = (data32 >> 16) & 0x7f;
		if (cdr0_code[i] > 120 ||
			cdr1_code[i] > 120 ||
			cdr2_code[i] > 120)
			flag_127++;
		if (cdr0_code[i] < 7 ||
			cdr1_code[i] < 7 ||
			cdr2_code[i] < 7)
			flag_32++;
		rx_pr("cdr_code[%d]=[%d,%d,%d]\n",
				i, cdr0_code[i], cdr1_code[i], cdr2_code[i]);
		rx_pr("flag:%d-%d\n", flag_32, flag_127);
		usleep_range(10, 20);
	}

	for (i = 0; i < 10; i++) {
		if (flag_127 && flag_32) {
			if (cdr0_code[i] < 7)
				cdr0_code[i] += 128;
			if (cdr1_code[i] < 7)
				cdr1_code[i] += 128;
			if (cdr2_code[i] < 7)
				cdr2_code[i] += 128;
		}
		sum_0 += cdr0_code[i];
		sum_1 += cdr1_code[i];
		sum_2 += cdr2_code[i];
	}
	*ch0 = (sum_0 / 10);
	*ch1 = (sum_1 / 10);
	*ch2 = (sum_2 / 10);
	rx_pr("ch0=%d, ch1=%d, ch2=%d\n", *ch0, *ch1, *ch2);
}

int aml_phy_get_iq_skew_val_t7(u32 val_0, u32 val_1)
{
	int val = val_0 - val_1;

	rx_pr("val=%d\n", val);
	if (val)
		return (val - 32);
	else
		return (val + 128 - 32);
}

/* IQ skew monitor */
void aml_phy_iq_skew_monitor_t7(void)
{
	int data32;
	int bist_mode = 3;
	u32 cdr0_code_0, cdr0_code_1, cdr0_code_2;/*clk0*/
	u32 cdr1_code_0, cdr1_code_1, cdr1_code_2;/*clk90*/
	u32 cdr2_code_0, cdr2_code_1, cdr2_code_2;/*clk180*/
	u32 cdr3_code_0, cdr3_code_1, cdr3_code_2;/*clk270*/
	//u32 cdr_code_0, cdr_code_1, cdr_code_2;
	int iq0_skew_ch0, iq0_skew_ch1, iq0_skew_ch2;
	int iq1_skew_ch0, iq1_skew_ch1, iq1_skew_ch2;
	int iq_skew;

	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL0, 0x4003707f);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL0, 0x400373ff);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL1, 0x000401ce);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL2, 0x00000000);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL3, 0x00000017);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL3, 0x00000817);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL0, 0x10210fff);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL1, 0x45ff4b58);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL2, 0x01ff2a21);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL3, 0x00001080);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL0, 0xe0b72900);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL1, 0x70000000);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL2, 0x09422222);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL3, 0x10000000);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL4, 0x00003001);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x200904f8);
	usleep_range(100, 110);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x300904f8);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL1, 0x00000000);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL2, 0x00001108);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL3, 0x10058f30);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL4, 0x090100c0);
	usleep_range(100, 110);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL4, 0x098100c0);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x340904f8);
	usleep_range(100, 110);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL0, 0x140904f8);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_APLL_CNTL2, 0x00003008);
	usleep_range(5, 10);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL0, 0xf3b72900);
	usleep_range(5, 10);
	usleep_range(1000, 1100);
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
	data32	|=	3 << 9;// [ 10:9] shift pattern en
	data32	|=	0 << 8;// [    8] bist_loopback
	data32	|=	3 << 5;// [ 7: 5] decoup_thresh
	// [ 4: 3] prbs_gen_mode:0=prbs11; 1=prbs15; 2=prbs7; 3=prbs31.
	data32	|=	bist_mode << 3;
	data32	|=	3 << 1;// [ 2: 1] prbs_gen_width:3=10-bit.
	data32	|=	0;// [	 0] prbs_gen_enable
	hdmirx_wr_top(TOP_PRBS_GEN, data32);
	hdmirx_wr_top(TOP_SHIFT_PTTN_012, 0xcccccccc);
	usleep_range(1000, 1100);
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
	data32	|=	3 << 9;// [ 10:9] shift pattern en
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
	usleep_range(1000, 1100);

	//read cdr_code 10 times, record the average value.
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL4, EYE_STATUS_EN, 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, _BIT(22), 0x0);
	hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL3, DBG_STS_SEL, 0x2);

	aml_phy_get_cdr_int_avr_t7(0x40000000, &cdr0_code_0, &cdr0_code_1, &cdr0_code_2);
	aml_phy_get_cdr_int_avr_t7(0x50000000, &cdr1_code_0, &cdr1_code_1, &cdr1_code_2);
	aml_phy_get_cdr_int_avr_t7(0x60000000, &cdr2_code_0, &cdr2_code_1, &cdr2_code_2);
	aml_phy_get_cdr_int_avr_t7(0x70000000, &cdr3_code_0, &cdr3_code_1, &cdr3_code_2);
	//iq_skew0 = value(clk0)-value(clk90)-32
	//iq_skew1 = value(clk180)-value(clk270)-32
	//iq_skew = average(iq_skew0,iq_skew1)
	//rx_pr("iq_skew=[%d,%d,%d]\n",

	iq0_skew_ch0 = aml_phy_get_iq_skew_val_t7(cdr0_code_0, cdr1_code_0);
	iq0_skew_ch1 = aml_phy_get_iq_skew_val_t7(cdr0_code_1, cdr1_code_1);
	iq0_skew_ch2 = aml_phy_get_iq_skew_val_t7(cdr0_code_2, cdr1_code_2);
	iq1_skew_ch0 = aml_phy_get_iq_skew_val_t7(cdr0_code_0, cdr1_code_0);
	iq1_skew_ch1 = aml_phy_get_iq_skew_val_t7(cdr0_code_1, cdr1_code_1);
	iq1_skew_ch2 = aml_phy_get_iq_skew_val_t7(cdr0_code_2, cdr1_code_2);
	rx_pr("iq_skew_0:[%d, %d, %d]\n", iq0_skew_ch0, iq0_skew_ch1, iq0_skew_ch2);
	rx_pr("iq_skew_1:[%d, %d, %d]\n", iq1_skew_ch0, iq1_skew_ch1, iq1_skew_ch2);
	iq_skew = (iq0_skew_ch0 + iq0_skew_ch1 + iq0_skew_ch2 +
		iq1_skew_ch0 + iq1_skew_ch0 + iq1_skew_ch0) / 6;
	rx_pr("iq_skew=%d\n", iq_skew);
	if (iq_skew)
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL1, IQ_OFST_SIGN, iq_skew);
	else
		hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL1, IQ_OFST_SIGN, 1 << 6 | abs(iq_skew));
}

bool aml_get_tmds_valid_t7(void)
{
	u32 tmdsclk_valid;
	u32 sqofclk;
	u32 tmds_align;
	u32 ret;

	/* digital tmds valid depends on PLL lock from analog phy. */
	/* it is not necessary and T7 has not it */
	/* tmds_valid = hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS) & 0x01; */
	sqofclk = hdmirx_rd_top(TOP_MISC_STAT0) & 0x1;
	tmdsclk_valid = is_tmds_clk_stable();
	/* modified in T7, 0x2b bit'0 tmds_align status */
	tmds_align = hdmirx_rd_top(TOP_TMDS_ALIGN_STAT) & 0x01;
	if (sqofclk && tmdsclk_valid && tmds_align) {
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

void aml_phy_power_off_t7(void)
{
	/* pll power down */
	hdmirx_wr_bits_amlphy(HHI_RX_APLL_CNTL0, MSK(2, 28), 2);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL0, 0x800800);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL1, 0x0);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL2, 0x60000002);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL3, 0x0);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL0, 0x10000000);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL1, 0x0);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL2, 0x0);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHA_CNTL3, 0x0);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL0, 0x0);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL1, 0x0);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL2, 0x0);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL3, 0x0);
	hdmirx_wr_amlphy(HHI_RX_PHY_DCHD_CNTL4, 0x0);
}

void aml_phy_switch_port_t7(void)
{
	u32 data32;

	/* reset and select data port */
	data32 = hdmirx_rd_amlphy(HHI_RX_PHY_MISC_CNTL3);
	data32 &= (~(0x7 << 6));
	data32 |= ((1 << rx.port) << 6);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL3, data32);
	data32 &= (~(1 << 11));
	/* release reset */
	data32 |= (1 << 11);
	hdmirx_wr_amlphy(HHI_RX_PHY_MISC_CNTL3, data32);
	udelay(5);

	hdmirx_wr_bits_top(TOP_PORT_SEL, MSK(3, 0), (1 << rx.port));
}

void dump_vsi_reg_t7(void)
{
	u8 data8, i;

	rx_pr("vsi data:\n");
	for (i = 0; i <= 30; i++) {
		data8 = hdmirx_rd_cor(VSIRX_TYPE_DP3_IVCRX + i);
		rx_pr("%d-[%x]\n", i, data8);
	}
	rx_pr("hf-vsi data:\n");
	for (i = 0; i <= 30; i++) {
		data8 = hdmirx_rd_cor(HF_VSIRX_TYPE_DP3_IVCRX + i);
		rx_pr("%d-[%x]\n", i, data8);
	}
	rx_pr("aif-vsi data:\n");
	for (i = 0; i <= 30; i++) {
		data8 = hdmirx_rd_cor(AUDRX_TYPE_DP2_IVCRX + i);
		rx_pr("%d-[%x]\n", i, data8);
	}
	rx_pr("unrec data:\n");
	for (i = 0; i <= 30; i++) {
		data8 = hdmirx_rd_cor(RX_UNREC_BYTE1_DP2_IVCRX + i);
		rx_pr("%d-[%x]\n", i, data8);
	}
}

unsigned int rx_sec_hdcp_cfg_t7(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDMI_RX_HDCP_CFG, 0, 0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0) & 0xffffffff);
}

void rx_set_irq_t7(bool en)
{
	u8 data8;

	if (en) {
		data8 = 0;
		data8 |= 1 << 4; /* intr_new_unrec en */
		data8 |= 1 << 2; /* intr_new_aud */
		data8 |= 1 << 1; /* intr_spd */
		hdmirx_wr_cor(RX_DEPACK_INTR2_MASK_DP2_IVCRX, data8);

		data8 = 0;
		data8 |= 1 << 4; /* intr_cea_repeat_hf_vsi en */
		data8 |= 1 << 3; /* intr_cea_new_hf_vsi en */
		data8 |= 1 << 2; /* intr_cea_new_vsi */
		hdmirx_wr_cor(RX_DEPACK_INTR3_MASK_DP2_IVCRX, data8);

		hdmirx_wr_cor(RX_GRP_INTR1_MASK_PWD_IVCRX, 5);
		hdmirx_wr_cor(RX_INTR1_MASK_PWD_IVCRX, 0x03);//register_address: 0x1050
		hdmirx_wr_cor(RX_INTR2_MASK_PWD_IVCRX, 0x00);//register_address: 0x1051
		hdmirx_wr_cor(RX_INTR3_MASK_PWD_IVCRX, 0x00);//register_address: 0x1052
		//must set 0.
		hdmirx_wr_cor(RX_INTR4_MASK_PWD_IVCRX, 0);//0x03);//register_address: 0x1053
		hdmirx_wr_cor(RX_INTR5_MASK_PWD_IVCRX, 0x00);//register_address: 0x1054
		hdmirx_wr_cor(RX_INTR6_MASK_PWD_IVCRX, 0x00);//register_address: 0x1055
		hdmirx_wr_cor(RX_INTR7_MASK_PWD_IVCRX, 0x00);//register_address: 0x1056
		hdmirx_wr_cor(RX_INTR8_MASK_PWD_IVCRX, 0x00);//register_address: 0x1057
		hdmirx_wr_cor(RX_INTR9_MASK_PWD_IVCRX, 0x00);//register_address: 0x1058

		data8 = 0;
		data8 |= 0 << 4; /* end of VSIF EMP data received */
		data8 |= 0 << 3;
		data8 |= 0 << 2;
		hdmirx_wr_cor(RX_DEPACK2_INTR2_MASK_DP0B_IVCRX, data8);

		//===for depack interrupt ====
		//hdmirx_wr_cor(CP2PAX_INTR0_MASK_HDCP2X_IVCRX, 0x3);
		//hdmirx_wr_cor(RX_INTR13_MASK_PWD_IVCRX, 0x02);// int
		//hdmirx_wr_cor(RX_PWD_INT_CTRL, 0x00);//[1] reg_intr_polarity, default = 1
		//hdmirx_wr_cor(RX_DEPACK_INTR4_MASK_DP2_IVCRX, 0x00);//interrupt mask
		//hdmirx_wr_cor(RX_DEPACK2_INTR0_MASK_DP0B_IVCRX, 0x0c);//interrupt mask
		//hdmirx_wr_cor(RX_DEPACK_INTR3_MASK_DP2_IVCRX, 0x20);//interrupt mask   [5] acr

		//HDCP irq
		// encrypted sts changed
		//hdmirx_wr_cor(RX_HDCP1X_INTR0_MASK_HDCP1X_IVCRX, 1);
		// AKE init received
		//hdmirx_wr_cor(CP2PAX_INTR1_MASK_HDCP2X_IVCRX, 4);
		// HDCP 2X_RX_ECC
		hdmirx_wr_cor(HDCP2X_RX_ECC_INTR_MASK, 1);
	} else {
		/* clear enable */
		hdmirx_wr_cor(RX_DEPACK_INTR2_MASK_DP2_IVCRX, 0);
		/* clear status */
		hdmirx_wr_cor(RX_DEPACK_INTR2_DP2_IVCRX, 0xff);
		/* clear enable */
		hdmirx_wr_cor(RX_DEPACK_INTR3_MASK_DP2_IVCRX, 0);
		/* clear status */
		hdmirx_wr_cor(RX_DEPACK_INTR3_DP2_IVCRX, 0xff);
		/* clear en */
		hdmirx_wr_cor(RX_GRP_INTR1_MASK_PWD_IVCRX, 0);
		/* clear status */
		hdmirx_wr_cor(RX_GRP_INTR1_STAT_PWD_IVCRX, 0xff);
		/* clear enable */
		hdmirx_wr_cor(RX_INTR1_MASK_PWD_IVCRX, 0);//register_address: 0x1050
		/* clear status */
		hdmirx_wr_cor(RX_INTR1_PWD_IVCRX, 0xff);
		/* clear enable */
		hdmirx_wr_cor(RX_INTR2_MASK_PWD_IVCRX, 0);//register_address: 0x1051
		/* clear status */
		hdmirx_wr_cor(RX_INTR2_PWD_IVCRX, 0xff);
		/* clear enable */
		hdmirx_wr_cor(RX_INTR3_MASK_PWD_IVCRX, 0);//register_address: 0x1052
		/* clear status */
		hdmirx_wr_cor(RX_INTR3_PWD_IVCRX, 0xff);
		/* clear enable */
		hdmirx_wr_cor(RX_INTR4_MASK_PWD_IVCRX, 0);//register_address: 0x1053
		/* clear status */
		hdmirx_wr_cor(RX_INTR4_PWD_IVCRX, 0xff);
		/* clear enable */
		hdmirx_wr_cor(RX_INTR5_MASK_PWD_IVCRX, 0);//register_address: 0x1054
		/* clear status */
		hdmirx_wr_cor(RX_INTR5_PWD_IVCRX, 0xff);
		/* clear enable */
		hdmirx_wr_cor(RX_INTR6_MASK_PWD_IVCRX, 0);//register_address: 0x1055
		/* clear status */
		hdmirx_wr_cor(RX_INTR6_PWD_IVCRX, 0xff);
		/* clear enable */
		hdmirx_wr_cor(RX_INTR7_MASK_PWD_IVCRX, 0);//register_address: 0x1056
		/* clear status */
		hdmirx_wr_cor(RX_INTR7_PWD_IVCRX, 0xff);
		/* clear enable */
		hdmirx_wr_cor(RX_INTR8_MASK_PWD_IVCRX, 0);//register_address: 0x1057
		/* clear status */
		hdmirx_wr_cor(RX_INTR8_PWD_IVCRX, 0xff);
		/* clear enable */
		hdmirx_wr_cor(RX_INTR9_MASK_PWD_IVCRX, 0);//register_address: 0x1058
		/* clear status */
		hdmirx_wr_cor(RX_INTR9_PWD_IVCRX, 0xff);
		/* clear enable */
		hdmirx_wr_cor(RX_DEPACK2_INTR2_MASK_DP0B_IVCRX, 0);
		/* clear status */
		hdmirx_wr_cor(RX_DEPACK2_INTR2_DP0B_IVCRX, 0xff);
		//===for depack interrupt ====
		//hdmirx_wr_cor(CP2PAX_INTR0_MASK_HDCP2X_IVCRX, 0x3);
		//hdmirx_wr_cor(RX_INTR13_MASK_PWD_IVCRX, 0x02);// int
		//hdmirx_wr_cor(RX_PWD_INT_CTRL, 0x00);//[1] reg_intr_polarity, default = 1
		/* clear status */
		hdmirx_wr_cor(RX_DEPACK_INTR2_DP2_IVCRX, 0xff);
		//hdmirx_wr_cor(RX_DEPACK_INTR4_MASK_DP2_IVCRX, 0x00);//interrupt mask
		//hdmirx_wr_cor(RX_DEPACK2_INTR0_MASK_DP0B_IVCRX, 0x0c);//interrupt mask
		//hdmirx_wr_cor(RX_DEPACK_INTR3_MASK_DP2_IVCRX, 0x20);//interrupt mask	 [5] acr

		//HDCP irq
		// encrypted sts changed
		//hdmirx_wr_cor(RX_HDCP1X_INTR0_MASK_HDCP1X_IVCRX, 0);
		// AKE init received
		//hdmirx_wr_cor(CP2PAX_INTR1_MASK_HDCP2X_IVCRX, 0);
		// HDCP 2X_RX_ECC
		hdmirx_wr_cor(HDCP2X_RX_ECC_INTR_MASK, 0);
	}

}

/*
 * 0 SPDIF
 * 1  I2S
 * 2  TDM
 */
void rx_set_aud_output_t7(u32 param)
{
	if (param == 2) {
		hdmirx_wr_cor(RX_TDM_CTRL1_AUD_IVCRX, 0x0f);
		hdmirx_wr_cor(RX_TDM_CTRL2_AUD_IVCRX, 0xff);
		hdmirx_wr_cor(AAC_MCLK_SEL_AUD_IVCRX, 0x90); //TDM
	} else if (param == 1) {
		hdmirx_wr_cor(RX_TDM_CTRL1_AUD_IVCRX, 0x00);
		hdmirx_wr_cor(RX_TDM_CTRL2_AUD_IVCRX, 0x10);
		hdmirx_wr_cor(AAC_MCLK_SEL_AUD_IVCRX, 0x80); //I2S
		//hdmirx_wr_bits_top(TOP_CLK_CNTL, _BIT(15), 0);
		hdmirx_wr_bits_top(TOP_CLK_CNTL, _BIT(4), 1);
	} else {
		hdmirx_wr_cor(RX_TDM_CTRL1_AUD_IVCRX, 0x00);
		hdmirx_wr_cor(RX_TDM_CTRL2_AUD_IVCRX, 0x10);
		hdmirx_wr_cor(AAC_MCLK_SEL_AUD_IVCRX, 0x80); //SPDIF
		//hdmirx_wr_bits_top(TOP_CLK_CNTL, _BIT(15), 1);
		hdmirx_wr_bits_top(TOP_CLK_CNTL, _BIT(4), 0);
	}
}

void rx_sw_reset_t7(int level)
{
	/* deep color fifo */
	hdmirx_wr_bits_cor(RX_PWD_SRST_PWD_IVCRX, _BIT(4), 1);
	udelay(1);
	hdmirx_wr_bits_cor(RX_PWD_SRST_PWD_IVCRX, _BIT(4), 0);
	//TODO..
}
