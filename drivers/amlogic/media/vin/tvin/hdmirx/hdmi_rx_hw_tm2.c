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
#include "hdmi_rx_hw_tm2.h"

/* for TM2 */
static const u32 phy_misci_b[][4] = {
		/* 0xd7	 0xd8		 0xe0		 0xe1 */
	{	 /* 24~45M */
		0x3003f7f8, 0x0041f080, 0x0, 0x00000817,
	},
	{	 /* 45~74.5M */
		0x3003f7f8, 0x0041f080, 0x0, 0x00000817,
	},
	{	 /* 77~155M */
		0x3003f7f8, 0x0041f080, 0x0, 0x00000817,
	},
	{	 /* 155~340M */
		0x3003f7f8, 0x0041f080, 0x0, 0x00000817,
	},
	{	 /* 340~525M */
		0x3003f7f8, 0x0041f080, 0x0, 0x00000817,
	},
	{	 /* 525~600M */
		0x3003f7f8, 0x0041f080, 0x0, 0x00000817,
	},
};

static const u32 phy_dcha_b[][4] = {
		/*  0xe2		 0xe3		 0xe4	   0xc5*/
	{	 /* 24~45M */ /*0x0123003f*/
		0x01237fff, 0x45ff5b59, 0x01ff1cd4, 0x817,
	},
	{	 /* 45~74.5M */ /*0x0123003f*/
		0x01237fff, 0x45ff5b59, 0x01ff1cd4, 0x817
	},
	{	 /* 77~155M */ /*0x0123003f*/
		0x01237fff, 0x45ff5b59, 0x01ff1cc9, 0x630,
	},
	{	 /* 155~340M */
		0x01230fff, 0x45ff4b09, 0x01ff2cc9, 0x630,
	},
	{	 /* 340~525M */
		0x01210fff, 0x45ff5b59, 0x01ff1e21, 0x1080,
	},
	{	 /* 525~600M */ /*0x0120007f 0x45ff5b59 0x01ff1e21*/
		0x0120007f, 0x45ff4b09, 0x01ff2e21, 0x1080,
	},
};

static const u32 phy_dchd_b[][4] = {
		/*  0xe5		 0xe6		 0xe7		0xc6*/
	{	 /* 24~45M */
		0xf3372113, 0x405c0000, 0x0d4a2222, 0x14000000,
	},
	{	 /* 45~74.5M */
		0xf3372113, 0x405c0000, 0x0d4a2222, 0x14000000,
	},
	{	 /* 77~155M */
		0xf3372113, 0x405c9000, 0x0d4a2222, 0x14000000,
	},
	{	 /* 155~340M */ /*0x14000000 0x0d422222*/
		0xf3372113, 0x405c9000, 0x0d4a2222, 0x10080000,
	},
	{	 /* 340~525M */ /*0x10080000*/
		0xf3372913, 0x405cf000, 0x0d4a2222, 0x0c000000,
	},
	{	 /* 525~600M */
		0xf3372913, 0x405cf000, 0x0d422222, 0x14000000,
	},
};

int get_tap2_tm2(int val)
{
	if ((val >> 4) == 0)
		return val;
	else
		return (0 - (val & 0xf));
}

bool is_dfe_sts_ok_tm2(void)
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

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x2);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap2 = data32 & 0xf;
	dfe1_tap2 = (data32 >> 8) & 0xf;
	dfe2_tap2 = (data32 >> 16) & 0xf;
	if (dfe0_tap2 >= 8 ||
	    dfe1_tap2 >= 8 ||
	    dfe2_tap2 >= 8)
		ret = false;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x3);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap3 = data32 & 0x7;
	dfe1_tap3 = (data32 >> 8) & 0x7;
	dfe2_tap3 = (data32 >> 16) & 0x7;
	if (dfe0_tap3 >= 6 ||
	    dfe1_tap3 >= 6 ||
	    dfe2_tap3 >= 6)
		ret = false;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x4);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap4 = data32 & 0x7;
	dfe1_tap4 = (data32 >> 8) & 0x7;
	dfe2_tap4 = (data32 >> 16) & 0x7;
	if (dfe0_tap4 >= 6 ||
	    dfe1_tap4 >= 6 ||
	    dfe2_tap4 >= 6)
		ret = false;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x5);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap5 = data32 & 0x7;
	dfe1_tap5 = (data32 >> 8) & 0x7;
	dfe2_tap5 = (data32 >> 16) & 0x7;
	if (dfe0_tap5 >= 6 ||
	    dfe1_tap5 >= 6 ||
	    dfe2_tap5 >= 6)
		ret = false;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x6);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap6 = data32 & 0x7;
	dfe1_tap6 = (data32 >> 8) & 0x7;
	dfe2_tap6 = (data32 >> 16) & 0x7;
	if (dfe0_tap6 >= 6 ||
	    dfe1_tap6 >= 6 ||
	    dfe2_tap6 >= 6)
		ret = false;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x7);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
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

/* detect low amplitude sig via tap0 value for TM2B*/
/* TL1:TODO...*/
int is_low_amplitude_sig_tm2(void)
{
	u32 data32;
	u32 ch0_tap0, ch1_tap0, ch2_tap0;
	u32 avr_tap0;
	bool ret = false;

	if (!rx.open_fg || (hdmirx_rd_top(TOP_MISC_STAT0) & 0x1))
		return ret;
	if (rx.phy.phy_bw <= PHY_BW_2) {
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
				TM2_DFE_EN, 1);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
				TM2_DFE_RSTB, 1);
	}
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x0);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	if (rx.phy.phy_bw <= PHY_BW_2) {
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
				TM2_DFE_EN, 0);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
				TM2_DFE_RSTB, 0);
	}
	ch0_tap0 = data32 & 0xff;
	ch1_tap0 = (data32 >> 8) & 0xff;
	ch2_tap0 = (data32 >> 16) & 0xff;
	avr_tap0 = (ch0_tap0 + ch1_tap0 + ch2_tap0) / 3;
	if (avr_tap0 > 10) {
		ret = true;
		if (log_level & EQ_LOG)
			rx_pr("low emp:%x-%x-%x\n", ch0_tap0, ch1_tap0, ch2_tap0);
	}
	return ret;
}

void aml_phy_tap1_byp_tm2(void)
{
	int tap1_tst, tap1_tst1, tap1_tst2;
	u32 data32 = 0;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3,
			TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
			TM2_DFE_DBG_STL, 0x1);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	tap1_tst = (data32 >> 4) & 0x3;
	tap1_tst1 = (data32 >> 12) & 0x3;
	tap1_tst2 = (data32 >> 20) & 0x3;
	if (tap1_tst == 0x3 || tap1_tst1 == 0x3 || tap1_tst2 == 0x3) {
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
				TM2_TAP1_BYP_EN, 1);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHA_CNTL1,
				TM2_DFE_TAP1_EN, 0);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
				TM2_DFE_EN, 1);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
				TM2_DFE_RSTB, 0);
		usleep_range(100, 110);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
				TM2_DFE_RSTB, 1);
		usleep_range(10, 15);
		rx_pr("tap1_byp\n");
	}
}

/* long cable detection for <3G */
void aml_phy_long_cable_det_tm2(void)
{
	int tap2_0, tap2_1, tap2_2;
	int tap2_max = 0;
	u32 data32 = 0;

	if (rx.phy.phy_bw > PHY_BW_3)
		return;
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x2);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	tap2_0 = get_tap2_tm2(data32 & 0x1f);
	tap2_1 = get_tap2_tm2((data32 >> 8 & 0x1f));
	tap2_2 = get_tap2_tm2((data32 >> 16 & 0x1f));
	if (rx.phy.phy_bw == PHY_BW_2) {
		/*disable DFE*/
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_RSTB, 0);
		tap2_max = 6;
	} else if (rx.phy.phy_bw == PHY_BW_3) {
		tap2_max = 10;
	}
	if ((tap2_0 + tap2_1 + tap2_2) >= tap2_max) {
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL1, TM2_EQ_BYP_VAL, 0x12);
		if (rx.phy.phy_bw == PHY_BW_2) {
			usleep_range(100, 110);
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_RSTB, 0);
			usleep_range(100, 110);
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_RSTB, 1);
		}
		usleep_range(10, 15);
		rx_pr("long cable\n");
	}
}

void aml_eq_vga_calibration_tm2(void)
{
	u32 vga_val, vga_ch0, vga_ch1, vga_ch2;
	int vga_cnt = 0;
	u32 data32 = 0;

	for (; vga_cnt < 20; vga_cnt++) {
		usleep_range(rx.aml_phy.vga_dbg_delay, rx.aml_phy.vga_dbg_delay + 20);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, MSK(3, 28), 0);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, MSK(2, 30), 0);
		/* get ch0/1/2 DFE TAP0 adaptive value*/
		data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
		vga_ch0 = data32 & 0xff;
		vga_ch1 = (data32 >> 8) & 0xff;
		vga_ch2 = (data32 >> 16) & 0xff;
		/* get the average val of ch0/1/2*/
		vga_val = (vga_ch0 + vga_ch1 + vga_ch2) / 3;
		/*vga_gain*/
		data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL0);
		if (vga_val < 30 && vga_val > 0) {
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHA_CNTL0,
					MSK(15, 0), ((data32 << 1) | 0x1));
			if (log_level & EQ_LOG)
				rx_pr("vga++\n");
		}
		if (vga_val >= 30) {
			if (log_level & EQ_LOG)
				rx_pr("vga done\n");
			break;
		}
	}
}

void aml_eq_cfg_tm2(void)
{
	u32 data32 = 0;
	//u32 idx = rx.phy.phy_bw;
	u32 eq_boost0, eq_boost1, eq_boost2;
	bool retry_flag = true;

EQ_RUN:
	if (rx.aml_phy.ofst_en) {
		//step8
		//dfe_ofstcal_start(0xe3[27])
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHA_CNTL1,
				TM2_DFE_OFSETCAL_START, 1);
		//TM2_OFST_CAL_START(0xe6[31])
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL1, TM2_OFST_CAL_START, 1);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHA_CNTL2, TM2_DFE_VADC_EN, 1);
		usleep_range(5, 10);
		/* 0xe7(26),disable dfe */
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_RSTB, 0);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, TM2_CDR_EQ_RSTB, 0);
		usleep_range(5, 10);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, TM2_CDR_EQ_RSTB, 3);
		//udelay(5);
		usleep_range(1, 2);

		//step9
		//dfe_ofstcal_start(0xe3[27]), TM2_OFST_CAL_START(0xe6[31])
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL1, TM2_OFST_CAL_START, 0);
		usleep_range(5, 10);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHA_CNTL1,
				TM2_DFE_OFSETCAL_START, 0);
		rx_pr("ofst\n");
	}
	//step10
	//cdr_lkdet_en(0xe5[28])TM2_DFE_RSTB(0xe7[26])),
	//eq_adp_stg<1:0>(0xe5[9:8])b01, cdr_rstb(0xe5[25]),
	//eq_rstb(0xe5[24])
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, TM2_CDR_LKDET_EN, 0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_RSTB, 0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, TM2_CDR_EQ_RSTB, 0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, TM2_EQ_ADP_STG, 1);
	usleep_range(5, 10);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, TM2_CDR_EQ_RSTB, 3);
	usleep_range(5, 10);
	/* auto eq-> fix eq */
	if (rx.phy.phy_bw <= PHY_BW_3 && rx.aml_phy.eq_byp) {
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x3);
		usleep_range(100, 110);
		data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
		eq_boost0 = data32 & 0xff;
		eq_boost1 = (data32 >> 8)  & 0xff;
		eq_boost2 = (data32 >> 16)	& 0xff;
		/* */
		if (eq_boost0 == 0 || eq_boost0 == 31 ||
		    eq_boost1 == 0 || eq_boost1 == 31 ||
		    eq_boost2 == 0 || eq_boost2 == 31) {
			/* fix eq */
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0,
					TM2_EQ_ADP_MODE, 0x0);
			rx_pr("eq_byp\n");
		}
	}
	if (rx.phy.phy_bw >= PHY_BW_4) {
		//step12
		if (rx.aml_phy.dfe_en) {
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
					TM2_DFE_EN, 1);
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0,
					TM2_EQ_ADP_STG, 0);
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
					TM2_DFE_RSTB, 1);
			usleep_range(10, 15);
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
					TM2_DFE_EN, 0);
			//if (rx.phy.phy_bw == PHY_BW_5) {
			//data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL0);
			//wr_reg_hhi_bits(
				//TM2_HHI_HDMIRX_PHY_DCHA_CNTL0,
				//MSK(15, 0), ((data32 >> 2) & 0x3fff));
			//}
			if (log_level & PHY_LOG)
				rx_pr("dfe\n");
		}
		usleep_range(100, 110);

		if (rx.aml_phy.tap1_byp) {
			aml_phy_tap1_byp_tm2();
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_EN, 0);
		}
		/* for usd test */
		if (!is_dfe_sts_ok_tm2()) {
			//wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_RSTB, 0);
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHA_CNTL0,
					MSK(15, 0), 0x7ff);
			rx_pr("dfe err. set default vga.\n");
		}

		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x3);
		usleep_range(100, 110);
		data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
		eq_boost0 = data32 & 0xff;
		eq_boost1 = (data32 >> 8)  & 0xff;
		eq_boost2 = (data32 >> 16)	& 0xff;
		if (eq_boost0 >= 26 ||
		    eq_boost1 >= 26 ||
		    eq_boost2 >= 26) {
			if (retry_flag) {
				retry_flag = false;
				rx_pr("eq_retry1\n");
				goto EQ_RUN;
			} else {
				wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL1,
						TM2_EQ_BYP_VAL, rx.aml_phy.eq_fix_val);
				wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0,
						TM2_EQ_ADP_MODE, 0x0);
				rx_pr("fix eq:%d\n", rx.aml_phy.eq_fix_val);
			}
		}
	} else if (rx.phy.phy_bw == PHY_BW_3) {//3G
		if (rx.aml_phy.dfe_en) {
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
					TM2_DFE_EN, 1);
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
					TM2_DFE_RSTB, 1);
			usleep_range(100, 110);
			//wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
				//_BIT(27), 0);
			if (log_level & PHY_LOG)
				rx_pr("dfe\n");
		}
		usleep_range(100, 110);

		if (rx.aml_phy.tap1_byp) {
			aml_phy_tap1_byp_tm2();
			//wr_reg_hhi_bits(
				//TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
				//TM2_DFE_EN, 0);
		}
		usleep_range(100, 110);
		//wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0,
			//_BIT(28), 1);
		if (rx.aml_phy.long_cable)
			aml_phy_long_cable_det_tm2();
		if (rx.aml_phy.vga_dbg)
			aml_eq_vga_calibration_tm2();
	} else if (rx.phy.phy_bw == PHY_BW_2) {
		if (rx.aml_phy.long_cable) {
			/*1.5G should enable DFE first*/
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
					TM2_DFE_EN, 1);
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0,
					TM2_EQ_ADP_STG, 0);
			wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2,
					TM2_DFE_RSTB, 1);
			usleep_range(10, 15);
			/*long cable detection*/
			aml_phy_long_cable_det_tm2();
		}
	}
	/* low_amplitude */
	rx.aml_phy.force_sqo = is_low_amplitude_sig_tm2();
	if (rx.aml_phy.force_sqo)
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_MISC_CNTL0, _BIT(30), 1);
	/*tmds valid det*/
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, TM2_CDR_LKDET_EN, 1);
}

void aml_phy_cfg_tm2(void)
{
	u32 idx = rx.phy.phy_bw;
	u32 data32;
	u32 term_value =
		hdmirx_rd_top(TOP_HPD_PWR5V) & 0x7;
	if (rx.aml_phy.pre_int) {
		if (log_level & PHY_LOG)
			rx_pr("\nphy reg init\n");
		data32 = phy_misci_b[idx][0];
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL0, data32);
		usleep_range(5, 10);
		data32 = phy_misci_b[idx][1];
		aml_phy_get_trim_val_tl1_tm2();
		if (phy_tdr_en) {
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
		/* step2-0xd8 */
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL1, data32);
		/*step2-0xe0*/
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL2, phy_misci_b[idx][2]);
		/*step2-0xe1*/
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL3, phy_misci_b[idx][3]);
		/*step2-0xe2*/
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL0, phy_dcha_b[idx][0]);
		/*step2-0xe3*/
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL1, phy_dcha_b[idx][1]);
		/*step2-0xe4*/
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL2, phy_dcha_b[idx][2]);
		/*step2-0xe5*/
		if (rx.aml_phy.cdr_mode)
			data32 = 0x73374940;
		else
			data32 = phy_dchd_b[idx][0];
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, data32);
		/*step2-0xe6*/
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL1, phy_dchd_b[idx][1]);
		/*step2-0xe7*/
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, phy_dchd_b[idx][2]);
		/*step2-0xc5*/
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL3, phy_dcha_b[idx][3]);
		/*step2-0xc6*/
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, phy_dchd_b[idx][3]);
		if (!rx.aml_phy.pre_int_en)
			rx.aml_phy.pre_int = 0;
	}
	/*step3 sel port 0xe1[8:6]*/
	/*step4 sq_rst 0xe1[11]=0*/

	data32 = phy_misci_b[idx][3];
	if (rx.aml_phy.sqrst_en)
		data32 &= ~(1 << 11);
	data32 |= ((1 << rx.port) << 6);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL3, data32);
	usleep_range(5, 10);
	/*step4 sq_rst 0xe1[11]=1*/
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_MISC_CNTL3, _BIT(11), 1);

	/*step4 0xd7*/
	data32 = phy_misci_b[idx][0];
	data32 &= (~(1 << 10));
	data32 &= (~(0x7 << 7));
	data32 |= term_value;
	data32 &= ~(disable_port_num & 0x07);
	/* terminal en */
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL0, data32);
	usleep_range(5, 10);
	/* res cali block[10] reset */
	/* step4:data channel[7:9] */
	data32 |= 0x1 << 10;
	data32 |= 0x7 << 7;
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL0, data32);

	/* step5 */
	/* set 0xe5[25],disable cdr */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, _BIT(25), 0);
	/* 0xe7(26),disable dfe */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, _BIT(26), 0);
	/* 0xe5[24],disable eq */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, _BIT(24), 0);
	/* 0xe5[28]=0,disable lckdet */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, _BIT(28), 0);
}

struct apll_param apll_tab_tm2[] = {
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

void aml_pll_bw_cfg_tm2(void)
{
	u32 idx = rx.phy.phy_bw;
	u32 M, N;
	u32 od, od_div;
	u32 od2, od2_div;
	u32 bw = rx.phy.pll_bw;
	u32 vco_clk;
	u32 data, data2;//data32;
	u32 cableclk = rx.clk.cable_clk / KHz;
	int pll_rst_cnt = 0;
	u32 clk_rate;

	clk_rate = rx_get_scdc_clkrate_sts();
	bw = aml_phy_pll_band(rx.clk.cable_clk, clk_rate);
	if (!is_clk_stable() || !cableclk)
		return;
	od_div = apll_tab_tm2[bw].od_div;
	od = apll_tab_tm2[bw].od;
	if (rx.aml_phy.osc_mode && idx == PHY_BW_5) {
		M = 0xF7;
		/* sel osc as pll clock */
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_MISC_CNTL0, _BIT(29), 0);
	} else {
		M = apll_tab_tm2[bw].M;
	}
	N = apll_tab_tm2[bw].N;
	if (rx.aml_phy.pll_div && idx == PHY_BW_5) {
		M *= rx.aml_phy.pll_div;
		N *= rx.aml_phy.pll_div;
	}
	od2_div = apll_tab_tm2[bw].od2_div;
	od2 = apll_tab_tm2[bw].od2;

	/* over sample rate select */
	/*set audio pll divider*/
	rx.phy.aud_div = apll_tab_tm2[bw].aud_div;
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
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL0, data | 0x20000000);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL0, data | 0x30000000);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL1, 0x00000000);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL2, 0x00001118);
		usleep_range(5, 10);
		if (idx <= PHY_BW_2)
			/* sz sast dvd flashing screen issue,*/
			/* There is a large jitter between 1MHz*/
			/* and 2MHz of this DVD source.*/
			/* Solution: increase pll Bandwidth*/
			data2 = 0x10058f30 | od2;
		else
			data2 = 0x10058f00 | od2;
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL3, data2);
		data2 = 0x080130c0;
		data2 |= (od << 24);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL4, data2);
		usleep_range(5, 10);
		/*apll_vctrl_mon_en*/
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL4, data2 | 0x00800000);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL0, data | 0x34000000);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL0, data | 0x14000000);
		usleep_range(5, 10);

		/* bit'5: force lock bit'2: improve phy ldo voltage */
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL2, 0x0000301c);

		usleep_range(100, 110);
		if (pll_rst_cnt++ > pll_rst_max) {
			rx_pr("pll rst error\n");
			break;
		}
		if (log_level & VIDEO_LOG) {
			//rx_pr("pll init-cableclk=%d,pixelclk=%d,\n",
			      //rx.clk.cable_clk / MHz,
			     // meson_clk_measure(29) / MHz);
			rx_pr("sq=%d,pll_lock=%d",
			      hdmirx_rd_top(TOP_MISC_STAT0) & 0x1,
			      aml_phy_pll_lock());
		}
	} while (!is_tmds_clk_stable() && !aml_phy_pll_lock());
	if (log_level & PHY_LOG)
		rx_pr("pll init done\n");
	if (rx.aml_phy.phy_bwth) {
		//phy config based on BW
		/*0xd8???????*/
		//data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL1);
		//wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL1, data32);
		/*0xc5*/
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL3, phy_dcha_b[idx][3]);
		/*0xe3*/
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHA_CNTL1, TM2_DFE_TAP1_EN,
				(phy_dcha_b[idx][1] >> 17) & 0x1);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHA_CNTL1, TM2_DFE_SUM_TRIM,
				(phy_dcha_b[idx][1] >> 4) & 0xf);
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHA_CNTL1, TM2_DEF_SUM_RS_TRIM,
				(phy_dcha_b[idx][1] >> 12) & 0x7);
		/*0xe4*/
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHA_CNTL2, MSK(14, 0),
				phy_dcha_b[idx][2] & 0x3fff);
		/*0xe7*/
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_TAP1_BYP_EN,
				(phy_dchd_b[idx][2] >> 19) & 0x1);
		/* 0xe2 */
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHA_CNTL0, MSK(18, 0),
				phy_dcha_b[idx][0] & 0x3ffff);
		/* 0xe6 */
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL1, TM2_EQ_BYP_VAL,
				(phy_dchd_b[idx][1] >> 12) & 0x1f);
		/* 0xe5 */
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, MSK(5, 10),
				(phy_dchd_b[idx][0] >> 10) & 0x1f);
		/* cdr_fr_en control */
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, _BIT(30),
				rx.aml_phy.cdr_fr_en);
		/* 0xc6 */
		wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, MSK(13, 17),
				(phy_dchd_b[idx][3] >> 17) & 0x1fff);
		if (log_level & PHY_LOG)
			rx_pr("phy_bw\n");
	}
}

/* For TM2 rev2 */
void aml_phy_init_tm2(void)
{
	aml_phy_cfg_tm2();
	usleep_range(10, 15);//band_gap stable 50us
	aml_pll_bw_cfg_tm2();
	usleep_range(10, 15);
	aml_eq_cfg_tm2();
	usleep_range(1000, 6000);
	if (rx.aml_phy.alirst_en) {
		hdmirx_wr_top(TOP_SW_RESET, 0x280);
		usleep_range(1, 2);
		hdmirx_wr_top(TOP_SW_RESET, 0);
	}
}

void aml_phy_short_bist_tm2(void)
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
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL0, 0x5003f07f);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL1, 0x0041ffc0);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL2, 0x00000000);
		usleep_range(5, 10);
		data32 = 0;
		/*selector clock to digital from data ch*/
		data32 |= (1 << port) << 3;
		data32 |= 0x7 << 0;
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL3, data32);
		rx_pr("\n port=%x\n", rd_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL3));
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL0, 0x012000ff);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL1, 0x45ff5959);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL2, 0x01ff1621);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL3, 0x00001080);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, 0xe0274913);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL1, 0x405c0000);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, 0x09644444);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, 0x00000000);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL0, 0x5003f7ff);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL3, 0x00000857);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL0, 0x200904f8);
		usleep_range(100, 110);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL0, 0x300904f8);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL1, 0x00000000);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL2, 0x0000111c);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL3, 0x10058f30);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL4, 0x090100c0);
		usleep_range(100, 110);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL4, 0x098100c0);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL0, 0x340904f8);
		usleep_range(100, 110);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL0, 0x140904f8);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL2, 0x0000301c);
		usleep_range(5, 10);
		wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, 0xf3274913);
		usleep_range(5, 10);
		usleep_range(1000, 1010);
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
		usleep_range(1000, 1010);
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
	if (rx.open_fg)
		rx.aml_phy.pre_int = 1;
}

/*
 * rx phy t5 debug
 */
int count_one_bits_tm2(u32 value)

{
	int count = 0;

	for (; value != 0; value >>= 1) {
		if (value & 1)
			count++;
	}
	return count;
}

void get_val_tm2(char *temp, unsigned int val, int len)
{
	if ((val >> (len - 1)) == 0)
		sprintf(temp, "+%d", val & (~(1 << (len - 1))));
	else
		sprintf(temp, "-%d", val & (~(1 << (len - 1))));
}

void comb_val_tm2(char *type, unsigned int val_0, unsigned int val_1,
		  unsigned int val_2, int len)
{
	char out[32], v0_buf[16], v1_buf[16], v2_buf[16];
	int pos = 0;

	get_val_tm2(v0_buf, val_0, len);
	get_val_tm2(v1_buf, val_1, len);
	get_val_tm2(v2_buf, val_2, len);
	pos += snprintf(out + pos, 32 - pos,  "%s[", type);
	pos += snprintf(out + pos, 32 - pos, " %s,", v0_buf);
	pos += snprintf(out + pos, 32 - pos, " %s,", v1_buf);
	pos += snprintf(out + pos, 32 - pos, " %s]", v2_buf);
	rx_pr("%s\n", out);
}

void dump_aml_phy_sts_tm2(void)
{
	u32 data32;
	u32 terminal;
	u32 vga_gain;
	u32 ch0_eq_boost, ch1_eq_boost, ch2_eq_boost;
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

	/* status-1 */
	data32 = (rd_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL1) >> 12) & 0x3ff;
	terminal = count_one_bits_tm2(data32);
	usleep_range(100, 110);

	/* status-2 */
	data32 = (rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL0) & 0x7fff);
	vga_gain = count_one_bits_tm2(data32);
	usleep_range(100, 110);

	/* status-3 */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x3);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	ch0_eq_boost = data32 & 0xff;
	ch1_eq_boost = (data32 >> 8)  & 0xff;
	ch2_eq_boost = (data32 >> 16)  & 0xff;

	/* status-4 */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x0);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap0 = data32 & 0xff;
	dfe1_tap0 = (data32 >> 8) & 0xff;
	dfe2_tap0 = (data32 >> 16) & 0xff;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x1);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap1 = data32 & 0x3f;
	dfe1_tap1 = (data32 >> 8) & 0x3f;
	dfe2_tap1 = (data32 >> 16) & 0x3f;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x2);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap2 = data32 & 0x1f;
	dfe1_tap2 = (data32 >> 8) & 0x1f;
	dfe2_tap2 = (data32 >> 16) & 0x1f;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x3);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap3 = data32 & 0xf;
	dfe1_tap3 = (data32 >> 8) & 0xf;
	dfe2_tap3 = (data32 >> 16) & 0xf;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x4);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap4 = data32 & 0xf;
	dfe1_tap4 = (data32 >> 8) & 0xf;
	dfe2_tap4 = (data32 >> 16) & 0xf;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x5);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap5 = data32 & 0xf;
	dfe1_tap5 = (data32 >> 8) & 0xf;
	dfe2_tap5 = (data32 >> 16) & 0xf;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x6);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap6 = data32 & 0xf;
	dfe1_tap6 = (data32 >> 8) & 0xf;
	dfe2_tap6 = (data32 >> 16) & 0xf;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x7);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	dfe0_tap7 = data32 & 0xf;
	dfe0_tap8 = (data32 >> 4) & 0xf;
	dfe1_tap7 = (data32 >> 8) & 0xf;
	dfe1_tap8 = (data32 >> 12) & 0xf;
	dfe2_tap7 = (data32 >> 16) & 0xf;
	dfe2_tap8 = (data32 >> 20) & 0xf;
	/* status-5: CDR status */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, _BIT(22), 0x0);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x2);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	cdr0_code = data32 & 0x7f;
	cdr0_lock = (data32 >> 7) & 0x1;
	cdr1_code = (data32 >> 8) & 0x7f;
	cdr1_lock = (data32 >> 15) & 0x1;
	cdr2_code = (data32 >> 16) & 0x7f;
	cdr2_lock = (data32 >> 23) & 0x1;

	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, _BIT(22), 0x1);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x2);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	cdr0_init = (data32 & 0x7f);
	cdr1_init = (data32 >> 8) & 0x7f;
	cdr2_init = (data32 >> 16) & 0x7f;

	/* status-6: pll lock */
	pll_lock = rd_reg_hhi(TM2_HHI_HDMIRX_APLL_CNTL0) >> 31;
	/* status-7: squelch */
	//squelch = rd_reg_hhi(HHI_HDMIRX_PHY_MISC_STAT) >> 31;//0320
	squelch = hdmirx_rd_top(TOP_MISC_STAT0) & 0x1;
	/* status-8:slicer offset status */
	/* status-8.0 */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x1);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x0);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	sli0_ofst0 = data32 & 0x1f;
	sli1_ofst0 = (data32 >> 8) & 0x1f;
	sli2_ofst0 = (data32 >> 16) & 0x1f;

	/* status-8.1 */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x1);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x1);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	sli0_ofst1 = data32 & 0x1f;
	sli1_ofst1 = (data32 >> 8) & 0x1f;
	sli2_ofst1 = (data32 >> 16) & 0x1f;

	/* status-8.2 */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x1);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x2);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	sli0_ofst2 = data32 & 0x1f;
	sli1_ofst2 = (data32 >> 8) & 0x1f;
	sli2_ofst2 = (data32 >> 16) & 0x1f;

	/* status-8.3 */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x1);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x3);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	sli0_ofst3 = data32 & 0x1f;
	sli1_ofst3 = (data32 >> 8) & 0x1f;
	sli2_ofst3 = (data32 >> 16) & 0x1f;

	/* status-8.4 */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x1);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x4);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	sli0_ofst4 = data32 & 0x1f;
	sli1_ofst4 = (data32 >> 8) & 0x1f;
	sli2_ofst4 = (data32 >> 16) & 0x1f;

	/* status-8.5 */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, TM2_DBG_STS_SEL, 0x1);
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, TM2_DFE_DBG_STL, 0x5);
	usleep_range(100, 110);
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_STAT);
	sli0_ofst5 = data32 & 0x1f;
	sli1_ofst5 = (data32 >> 8) & 0x1f;
	sli2_ofst5 = (data32 >> 16) & 0x1f;

	rx_pr("\n hdmirx phy status:\n");
	rx_pr("terminal=%d, vga_gain=%d, ", terminal, vga_gain);
	rx_pr("pll_lock=%d, squelch=%d\n", pll_lock, squelch);
	rx_pr("eq_boost=[%d,%d,%d]\n", ch0_eq_boost, ch1_eq_boost, ch2_eq_boost);

	comb_val_tm2("	  dfe_tap0", dfe0_tap0, dfe1_tap0, dfe2_tap0, 8);
	comb_val_tm2("	  dfe_tap1", dfe0_tap1, dfe1_tap1, dfe2_tap1, 6);
	comb_val_tm2("	  dfe_tap2", dfe0_tap2, dfe1_tap2, dfe2_tap2, 5);
	comb_val_tm2("	  dfe_tap3", dfe0_tap3, dfe1_tap3, dfe2_tap3, 4);
	comb_val_tm2("	  dfe_tap4", dfe0_tap4, dfe1_tap4, dfe2_tap4, 4);
	comb_val_tm2("	  dfe_tap5", dfe0_tap5, dfe1_tap5, dfe2_tap5, 4);
	comb_val_tm2("	  dfe_tap6", dfe0_tap6, dfe1_tap6, dfe2_tap6, 4);
	comb_val_tm2("	  dfe_tap7", dfe0_tap7, dfe1_tap7, dfe2_tap7, 4);
	comb_val_tm2("	  dfe_tap8", dfe0_tap8, dfe1_tap8, dfe2_tap8, 4);

	comb_val_tm2("slicer_ofst0", sli0_ofst0, sli1_ofst0, sli2_ofst0, 5);
	comb_val_tm2("slicer_ofst1", sli0_ofst1, sli1_ofst1, sli2_ofst1, 5);
	comb_val_tm2("slicer_ofst2", sli0_ofst2, sli1_ofst2, sli2_ofst2, 5);
	comb_val_tm2("slicer_ofst3", sli0_ofst3, sli1_ofst3, sli2_ofst3, 5);
	comb_val_tm2("slicer_ofst4", sli0_ofst4, sli1_ofst4, sli2_ofst4, 5);
	comb_val_tm2("slicer_ofst5", sli0_ofst5, sli1_ofst5, sli2_ofst5, 5);

	rx_pr("cdr_code=[%d,%d,%d]\n", cdr0_code, cdr1_code, cdr2_code);
	rx_pr("cdr_lock=[%d,%d,%d]\n", cdr0_lock, cdr1_lock, cdr2_lock);
	comb_val_tm2("cdr0_int", cdr0_init, cdr1_init, cdr2_init, 7);
}

bool aml_get_tmds_valid_tm2(void)
{
	unsigned int tmdsclk_valid;
	unsigned int sqofclk;
	/* unsigned int pll_lock; */
	unsigned int tmds_align;
	u32 ret;

	/* digital tmds valid depends on PLL lock from analog phy. */
	/* it is not necessary and T7 has not it */
	/* tmds_valid = hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS) & 0x01; */
	if (rx.aml_phy.force_sqo)
		sqofclk = rx.aml_phy.force_sqo;
	else
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

void aml_phy_power_off_tm2(void)
{
	/* pll power down */
	wr_reg_hhi_bits(TM2_HHI_HDMIRX_APLL_CNTL0, MSK(2, 28), 2);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL0, 0x22800800);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL1, 0x01000000);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL2, 0x60000000);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL3, 0x0);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL0, 0x01000000);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL1, 0x0);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL2, 0x0);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL0, 0x0);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL1, 0x0);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL2, 0x0);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHA_CNTL3, 0x0);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_DCHD_CNTL3, 0x0);
}

void aml_phy_switch_port_tm2(void)
{
	u32 data32;

	/* reset and select data port */
	data32 = rd_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL3);
	data32 &= (~(0x7 << 6));
	data32 |= ((1 << rx.port) << 6);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL3, data32);
	data32 &= (~(1 << 11));
	/* release reset */
	data32 |= (1 << 11);
	wr_reg_hhi(TM2_HHI_HDMIRX_PHY_MISC_CNTL3, data32);
	udelay(5);

	data32 = 0;
	data32 |= rx.port << 2;
	hdmirx_wr_dwc(DWC_SNPS_PHYG3_CTRL, data32);
}
