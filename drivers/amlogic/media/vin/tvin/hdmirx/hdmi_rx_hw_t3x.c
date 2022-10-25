// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
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
#include "hdmi_rx_hw_t3x.h"

/* FT trim flag:1-valid, 0-not valid */
//u32 rterm_trim_flag_t3x;
/* FT trim value 4 bits */
//u32 rterm_trim_val_t3x;

/* for T3x */
static const u32 phy_misc_t3x[][2] = {
		/*  0x0d	0x0e	*/
	{	 /* 24~45M */
		0xfff00080, 0x0007b003,
	},
	{	 /* 45~74.5M */
		0xfff00000, 0x00073002,
	},
	{	 /* 77~155M */
		0xfff00000, 0x00073001,
	},
	{	 /* 155~340M */
		0xfff00000, 0x00073000,
	},
	{	 /* 340~525M */
		0xfff00000, 0x00073000,
	},
	{	 /* 525~600M */
		0xfff00100, 0x00073000,
	},
};

static const u32 phy_dcha_t3x[][2] = {
		 /* 0x08	 0x0c*/
	{	 /* 24~45M */
		0x02f27ccc, 0x40000c59,
	},
	{	 /* 45~74.5M */
		0x02f27ccc, 0x40000c59,
	},
	{	 /* 77~155M */
		0x02f27666, 0x40000459,
	},
	{	 /* 155~340M */
		0x02b27666, 0x7ff00459,
	},
	{	 /* 340~525M */
		0x02821666, 0x7ff00459,
	},
	{	 /* 525~600M */
		0x02821666, 0x7ff00459,
	},
};

static const u32 phy_dchd_t3x[][2] = {
		/*  0x10	 0x14 */
	{	 /* 24~45M */
		0x04007a57, 0x30083060,
	},
	{	 /* 45~74.5M */
		0x04007255, 0x30083060,
	},
	{	 /* 77~155M */
		0x04007155, 0x30083069,
	},
	{	 /* 155~340M */
		0x04007053, 0x30033050,
	},
	{	 /* 340~525M */
		0x04007053, 0x30033050
	},
	{	 /* 525~600M */
		0x04007053, 0x30033050,
	},
};

struct apll_param apll_tab_t3x[] = {
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
unsigned int decimaltogray_t3x(unsigned int x)
{
	return x ^ (x >> 1);
}

/* Gray code to Decimal */
unsigned int graytodecimal_t3x(unsigned int x)
{
	unsigned int y = x;

	while (x >>= 1)
		y ^= x;
	return y;
}

bool is_pll_lock_t3x(void)
{
	return ((hdmirx_rd_amlphy(T3X_HDMIRX20PLL_CTRL0) >> 31) & 0x1);
}

void aml_pll_bw_cfg_t3x(void)
{
	u32 idx = rx.phy.phy_bw;
	int pll_rst_cnt = 0;

	if (rx.aml_phy.osc_mode && idx == PHY_BW_5) {
		/* sel osc as pll clock */
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHA_MISC2, T3X_PLL_CLK_SEL, 1);
		/* 9028: select tmds_clk from tclk or tmds_ch_clk */
		/* cdr = tmds_ch_ck,  vco =tclk */
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHA_MISC1, T3X_VCO_TMDS_EN, 0);
	}

	if (rx.aml_phy.phy_bwth) {
		/* lock det rst */
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_CDR_LKDET_EN, 0);
		/* dfe rst*/
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_DFE_RSTB, 0);
		/* cdr rst*/
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_CDR_RSTB, 0);
		/*eq rst*/
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_EQ_RSTB, 0);
		/*eq en*/
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_EQ_EN, 1);
		/*dfe hold*/
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_DFE_HOLD_EN, 0);
		/*cdr fr en*/
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_CDR_FR_EN, 0);
		//udelay(100);
		usleep_range(10, 20);
		/* enable byp eq */
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_EN_BYP_EQ,
				      1);
		/* eq initial val*/
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_BYP_EQ,
				      0x10);
		/*set os rate*/
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR,
				      T3X_CDR_OS_RATE, (phy_dchd_t3x[idx][0] >> 8) & 0x3);
		/* tap1~8 bypass en */
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ,
				      T3X_BYP_TAP_EN, 1);
		/* tap0 bypass en */
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ,
				      T3X_BYP_TAP0_EN, 1);
		rx_pr("phy_bw\n");
	}
	do {
		/* pll cfg */
		hdmirx_wr_amlphy(T3X_RG_RX20PLL_0, 0x05302800);
		hdmirx_wr_amlphy(T3X_RG_RX20PLL_1, 0x01481236);
		usleep_range(10, 30);
		hdmirx_wr_amlphy(T3X_RG_RX20PLL_0, 0x05302801);
		usleep_range(10, 60);
		hdmirx_wr_amlphy(T3X_RG_RX20PLL_0, 0x05302803);
		usleep_range(10, 15);
		hdmirx_wr_amlphy(T3X_RG_RX20PLL_1, 0x01401236);
		usleep_range(10, 60);
		hdmirx_wr_amlphy(T3X_RG_RX20PLL_0, 0x05302807);
		usleep_range(10, 15);
		hdmirx_wr_amlphy(T3X_RG_RX20PLL_0, 0x45302807);
		usleep_range(10, 60);
		rx_pr("PLL0=0x%x\n", hdmirx_rd_amlphy(T3X_RG_RX20PLL_0));
		if (pll_rst_cnt++ > pll_rst_max) {
			if (log_level & VIDEO_LOG)
				rx_pr("pll rst error\n");
			break;
		}
		if (log_level & VIDEO_LOG) {
			rx_pr("sq=%d,pll_lock=%d",
			      hdmirx_rd_top(TOP_MISC_STAT0) & 0x1,
			      is_pll_lock_t3x());
		}
	} while (!is_pll_lock_t3x());
	rx_pr("pll done\n");
	/* t9028 debug */
	/* manual VGA mode for debug,hyper gain=1 */
	if (rx.aml_phy.vga_gain <= 0xfff) {
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHA_AFE, MSK(12, 0),
				      (decimaltogray_t3x(rx.aml_phy.vga_gain & 0x7) |
				      (decimaltogray_t3x(rx.aml_phy.vga_gain >> 4 & 0x7) << 4) |
				      (decimaltogray_t3x(rx.aml_phy.vga_gain >> 8 & 0x7) << 8)));
	}
	/* manual EQ mode for debug */
	if (rx.aml_phy.eq_stg1 < 0x1f) {
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ,
				      T3X_BYP_EQ, rx.aml_phy.eq_stg1 & 0x1f);
		/* eq adaptive:0-adaptive 1-manual */
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_EN_BYP_EQ, 1);
	}
	/*tap2 byp*/
	if (rx.aml_phy.tap2_byp && rx.phy.phy_bw >= PHY_BW_3)
		/* dfe_tap_en [28:20]*/
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHA_DFE, _BIT(22), 0);
}

int get_tap2_t3x(int val)
{
	if ((val >> 4) == 0)
		return val;
	else
		return (0 - (val & 0xf));
}

void is_dfe_sts_ok_t3x(void)
{
}

/* long cable detection for <3G need to be change */
void aml_phy_long_cable_det_t3x(void)
{
}

/* aml_hyper_gain_tuning */
void aml_hyper_gain_tuning_t3x(void)
{
}

void aml_eq_retry_t3x(void)
{
}

void aml_dfe_en_t3x(void)
{
	if (rx.aml_phy.dfe_en) {
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_DFE_EN, 1);
		if (rx.aml_phy.eq_hold)
			hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_EQ_MODE, 1);
		if (rx.aml_phy.eq_retry)
			aml_eq_retry_t3x();
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_DFE_RSTB, 0);
		usleep_range(10, 20);
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ,
				      T3X_DFE_RSTB, 1);
		usleep_range(10, 20);
		if (rx.aml_phy.dfe_hold)
			hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ,
					      T3X_DFE_HOLD_EN, 1);
		rx_pr("dfe\n");
	}
}

/* phy offset calibration based on different chip and board */
void aml_phy_offset_cal_t3x(void)
{
}

/* hardware eye monitor */
void aml_eq_eye_monitor_t3x(void)
{
}

void get_eq_val_t3x(void)
{
}

/* check eq_boost1 & tap0 status */
void is_eq1_tap0_err_t3x(void)
{
}

void aml_eq_cfg_t3x(void)
{
	/* dont need to run eq if no sqo_clk or pll not lock */
	if (!aml_phy_pll_lock())
		return;
	/* lock det rst */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, _BIT(14), 0);
	/* dfe rst*/
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, _BIT(17), 0);
	/* cdr rst*/
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, _BIT(13), 0);
	/*eq rst*/
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, _BIT(13), 0);
	/*eq en*/
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, _BIT(12), 1);
	/*dfe hold*/
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, _BIT(18), 0);
	/*cdr fr en*/
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, _BIT(6), 0);
	/* cdr rst*/
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, _BIT(13), 1);
	/*eq rst*/
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, _BIT(13), 1);
	if (rx.aml_phy.cdr_fr_en) {
		udelay(rx.aml_phy.cdr_fr_en);
		/*cdr fr en*/
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, _BIT(6), 1);
	}
	usleep_range(100, 110);
	get_eq_val_t3x();
	/*if (rx.aml_phy.eq_retry)*/
		/*aml_eq_retry_t3X();*/
	if (rx.phy.phy_bw >= PHY_BW_4) {
		/* step12 */
		/* aml_dfe_en(); */
		/* udelay(100); */
	} else if (rx.phy.phy_bw == PHY_BW_3) {//3G
		/* aml_dfe_en(); */
		/*udelay(100);*/
		/*t3 removed, tap1 min value*/
		/* if (rx.aml_phy.tap1_byp) { */
			/* aml_phy_tap1_byp_t3(); */
			/* hdmirx_wr_bits_amlphy( */
				/* HHI_RX_PHY_DCHD_CNTL2, */
				/* DFE_EN, 0); */
		/* } */
		/*udelay(100);*/
		/* hdmirx_wr_bits_amlphy(HHI_RX_PHY_DCHD_CNTL0, */
			/* _BIT(28), 1); */
		if (rx.aml_phy.long_cable)
			;/* aml_phy_long_cable_det_t3X(); */
		if (rx.aml_phy.vga_dbg)
			;/* aml_vga_tuning_t3();*/
	} else if (rx.phy.phy_bw == PHY_BW_2) {
		if (rx.aml_phy.long_cable) {
			/*1.5G should enable DFE first*/
			/* aml_dfe_en(); */
			/* long cable detection*/
			/* aml_phy_long_cable_det_t3X();*/
			/* 1.5G should disable DFE at the end*/
			/* udelay(100); */
			/* aml_dfe_en(); */
		}
	}
	/* enable dfe for all frequency */
	aml_dfe_en_t3x();
	/*enable HYPER_GAIN calibration for 6G to fix 2.0 cts HF2-1 issue*/
	aml_hyper_gain_tuning_t3x();
	usleep_range(100, 110);
	/*tmds valid det*/
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_CDR_LKDET_EN, 1);
}

void aml_phy_get_trim_val_t3x(void)
{
}

void aml_phy_cfg_t3x(void)
{
	u32 idx = rx.phy.phy_bw;
	u32 data32;

	if (rx.aml_phy.pre_int) {
		rx_pr("\nphy reg init\n");
		if (rx.aml_phy.ofst_en)
			aml_phy_offset_cal_t3x();
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR,
				      T3X_OFSET_CAL_START, 0);
		usleep_range(100, 110);
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHA_DFE,
				      T3X_SLICER_OFSTCAL_START, 0);
		usleep_range(20, 30);
		data32 = phy_dcha_t3x[idx][0];
		hdmirx_wr_amlphy(T3X_HDMIRX20PHY_DCHA_AFE, data32);
		data32 = phy_dcha_t3x[idx][1];
		hdmirx_wr_amlphy(T3X_HDMIRX20PHY_DCHA_DFE, data32);
		data32 = phy_dchd_t3x[idx][0];
		hdmirx_wr_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, data32);
		data32 = phy_dchd_t3x[idx][1];
		hdmirx_wr_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, data32);
		data32 = phy_misc_t3x[idx][0];
		hdmirx_wr_amlphy(T3X_HDMIRX20PHY_DCHA_MISC1, data32);
		data32 = phy_misc_t3x[idx][0];
		hdmirx_wr_amlphy(T3X_HDMIRX20PHY_DCHA_MISC2, data32);

		if (!rx.aml_phy.pre_int_en)
			rx.aml_phy.pre_int = 0;
	}
	if (rx.aml_phy.sqrst_en) {
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHA_MISC1, T3X_SQ_RSTN, 0);
		usleep_range(5, 10);
		/*sq_rst*/
		hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHA_MISC1, T3X_SQ_RSTN, 1);
	}
	/* step5 */
	/* disable cdr */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_CDR_RSTB, 0);
	/* disable dfe */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_DFE_RSTB, 0);
	/* disable eq */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_EQ_RSTB, 0);
	/* disable lckdet */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_CDR_LKDET_EN, 0);
}

 /* For t3x */
void aml_phy_init_t3x(void)
{
	aml_phy_cfg_t3x();
	usleep_range(10, 20);
	aml_pll_bw_cfg_t3x();
	usleep_range(10, 20);
	aml_eq_cfg_t3x();
}

void dump_reg_phy_t3x(void)
{
	rx_pr("PHY Register:\n");
	rx_pr("dcha_afe-0x8=0x%x\n", hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHA_AFE));
	rx_pr("dcha_dfe-0xc=0x%x\n", hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHA_DFE));
	rx_pr("dchd_cdr-0x10=0x%x\n", hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_CDR));
	rx_pr("dchd_eq-0x14=0x%x\n", hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_EQ));
	rx_pr("misc1-0x18=0x%x\n", hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHA_MISC1));
	rx_pr("misc2-0x1c=0x%x\n", hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHA_MISC2));
}

/*
 * rx phy v2 debug
 */
int count_one_bits_t3x(u32 value)
{
	int count = 0;

	for (; value != 0; value >>= 1) {
		if (value & 1)
			count++;
	}
	return count;
}

void get_val_t3x(char *temp, unsigned int val, int len)
{
	if ((val >> (len - 1)) == 0)
		sprintf(temp, "+%d", val & (~(1 << (len - 1))));
	else
		sprintf(temp, "-%d", val & (~(1 << (len - 1))));
}

void comb_val_t3x(char *type, unsigned int val_0, unsigned int val_1,
		 unsigned int val_2, int len)
{
	char out[32], v0_buf[16], v1_buf[16], v2_buf[16];
	int pos = 0;

	get_val_t3x(v0_buf, val_0, len);
	get_val_t3x(v1_buf, val_1, len);
	get_val_t3x(v2_buf, val_2, len);
	pos += snprintf(out + pos, 32 - pos, "%s[", type);
	pos += snprintf(out + pos, 32 - pos, " %s,", v0_buf);
	pos += snprintf(out + pos, 32 - pos, " %s,", v1_buf);
	pos += snprintf(out + pos, 32 - pos, " %s]", v2_buf);
	rx_pr("%s\n", out);
}

void dump_aml_phy_sts_t3x(void)
{
	u32 data32;
	u32 terminal;
	u32 ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1;
	u32 ch0_eq_err, ch1_eq_err, ch2_eq_err;
	u32 dfe0_tap0, dfe1_tap0, dfe2_tap0, dfe3_tap0;
	u32 dfe0_tap1, dfe1_tap1, dfe2_tap1, dfe3_tap1;
	u32 dfe0_tap2, dfe1_tap2, dfe2_tap2, dfe3_tap2;
	u32 dfe0_tap3, dfe1_tap3, dfe2_tap3, dfe3_tap3;
	u32 dfe0_tap4, dfe1_tap4, dfe2_tap4, dfe3_tap4;
	u32 dfe0_tap5, dfe1_tap5, dfe2_tap5, dfe3_tap5;
	u32 dfe0_tap6, dfe1_tap6, dfe2_tap6, dfe3_tap6;
	u32 dfe0_tap7, dfe1_tap7, dfe2_tap7, dfe3_tap7;
	u32 dfe0_tap8, dfe1_tap8, dfe2_tap8, dfe3_tap8;

	u32 cdr0_lock, cdr1_lock, cdr2_lock;
	u32 cdr0_int, cdr1_int, cdr2_int;
	u32 cdr0_code, cdr1_code, cdr2_code;

	bool pll_lock;
	bool squelch;

	u32 sli0_ofst0, sli1_ofst0, sli2_ofst0;
	u32 sli0_ofst1, sli1_ofst1, sli2_ofst1;
	u32 sli0_ofst2, sli1_ofst2, sli2_ofst2;

	/* rterm */
	terminal = (hdmirx_rd_bits_amlphy(T3X_HDMIRX20PHY_DCHA_MISC1, T3X_RTERM_CNTL));

	/* eq_boost1 status */
	/* mux_eye_en */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	/* mux_block_sel */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x3);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	ch0_eq_boost1 = data32 & 0x1f;
	ch0_eq_err = (data32 >> 5) & 0x3;
	ch1_eq_boost1 = (data32 >> 8) & 0x1f;
	ch1_eq_err = (data32 >> 13) & 0x3;
	ch2_eq_boost1 = (data32 >> 16) & 0x1f;
	ch2_eq_err = (data32 >> 21) & 0x3;

	/* dfe tap0 sts */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_DFE_OFST_EYE, 0x0);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap0 = data32 & 0x7f;
	dfe1_tap0 = (data32 >> 8) & 0x7f;
	dfe2_tap0 = (data32 >> 16) & 0x7f;
	dfe3_tap0 = (data32 >> 24) & 0x7f;
	/* dfe tap1 sts */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_DFE_OFST_EYE, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap1 = data32 & 0x3f;
	dfe1_tap1 = (data32 >> 8) & 0x3f;
	dfe2_tap1 = (data32 >> 16) & 0x3f;
	dfe3_tap1 = (data32 >> 24) & 0x3f;
	/* dfe tap2 sts */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_DFE_OFST_EYE, 0x2);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap2 = data32 & 0x1f;
	dfe1_tap2 = (data32 >> 8) & 0x1f;
	dfe2_tap2 = (data32 >> 16) & 0x1f;
	dfe3_tap2 = (data32 >> 24) & 0x1f;
	/* dfe tap3 sts */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_DFE_OFST_EYE, 0x3);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap3 = data32 & 0xf;
	dfe1_tap3 = (data32 >> 8) & 0xf;
	dfe2_tap3 = (data32 >> 16) & 0xf;
	dfe3_tap3 = (data32 >> 24) & 0xf;
	/* dfe tap4 sts */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_DFE_OFST_EYE, 0x4);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap4 = data32 & 0xf;
	dfe1_tap4 = (data32 >> 8) & 0xf;
	dfe2_tap4 = (data32 >> 16) & 0xf;
	dfe3_tap4 = (data32 >> 24) & 0xf;
	/* dfe tap5 sts */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_DFE_OFST_EYE, 0x5);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap5 = data32 & 0xf;
	dfe1_tap5 = (data32 >> 8) & 0xf;
	dfe2_tap5 = (data32 >> 16) & 0xf;
	dfe3_tap5 = (data32 >> 24) & 0xf;
	/* dfe tap6 sts */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_DFE_OFST_EYE, 0x6);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap6 = data32 & 0xf;
	dfe1_tap6 = (data32 >> 8) & 0xf;
	dfe2_tap6 = (data32 >> 16) & 0xf;
	dfe3_tap6 = (data32 >> 24) & 0xf;
	/* dfe tap7/8 sts */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_DFE_OFST_EYE, 0x7);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap7 = data32 & 0xf;
	dfe1_tap7 = (data32 >> 8) & 0xf;
	dfe2_tap7 = (data32 >> 16) & 0xf;
	dfe3_tap7 = (data32 >> 24) & 0xf;
	dfe0_tap8 = (data32 >> 4) & 0xf;
	dfe1_tap8 = (data32 >> 12) & 0xf;
	dfe2_tap8 = (data32 >> 20) & 0xf;
	dfe3_tap8 = (data32 >> 24) & 0xf;

	/* CDR status */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x22);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_CDR_DBG_SEL, 0x0);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	cdr0_code = data32 & 0x7f;
	cdr0_lock = (data32 >> 7) & 0x1;
	cdr1_code = (data32 >> 8) & 0x7f;
	cdr1_lock = (data32 >> 15) & 0x1;
	cdr2_code = (data32 >> 16) & 0x7f;
	cdr2_lock = (data32 >> 23) & 0x1;
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_CDR_DBG_SEL, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	cdr0_int = data32 & 0x7f;
	cdr1_int = (data32 >> 8) & 0x7f;
	cdr2_int = (data32 >> 16) & 0x7f;

	/* pll lock */
	pll_lock = hdmirx_rd_amlphy(T3X_RG_RX20PLL_0) >> 31;

	/* squelch */
	squelch = hdmirx_rd_top(TOP_MISC_STAT0) & 0x1;

	/* slicer offset status */
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x1);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_DFE_OFST_EYE, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_CDR_DBG_SEL, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	sli0_ofst0 = data32 & 0x1f;
	sli1_ofst0 = (data32 >> 8) & 0x1f;
	sli2_ofst0 = (data32 >> 16) & 0x1f;

	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x1);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_DFE_OFST_EYE, 0x1);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_CDR_DBG_SEL, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	sli0_ofst1 = data32 & 0x1f;
	sli1_ofst1 = (data32 >> 8) & 0x1f;
	sli2_ofst1 = (data32 >> 16) & 0x1f;

	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_EYE_EN, 0x0);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_EQ, T3X_MUX_BLOCK_SEL, 0x1);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_DFE_OFST_EYE, 0x2);
	hdmirx_wr_bits_amlphy(T3X_HDMIRX20PHY_DCHD_CDR, T3X_MUX_CDR_DBG_SEL, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(T3X_HDMIRX20PHY_DCHD_STAT);
	sli0_ofst2 = data32 & 0x1f;
	sli1_ofst2 = (data32 >> 8) & 0x1f;
	sli2_ofst2 = (data32 >> 16) & 0x1f;

	rx_pr("\nhdmirx phy status:\n");
	rx_pr("pll_lock=%d, squelch=%d, terminal=%d\n", pll_lock, squelch, terminal);
	rx_pr("eq_boost1=[%d,%d,%d]\n",
	      ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1);
	rx_pr("eq_err=[%d,%d,%d]\n",
	      ch0_eq_err, ch1_eq_err, ch2_eq_err);

	comb_val_t3x("	 dfe_tap0", dfe0_tap0, dfe1_tap0, dfe2_tap0, 7);
	comb_val_t3x("	 dfe_tap1", dfe0_tap1, dfe1_tap1, dfe2_tap1, 6);
	comb_val_t3x("	 dfe_tap2", dfe0_tap2, dfe1_tap2, dfe2_tap2, 5);
	comb_val_t3x("	 dfe_tap3", dfe0_tap3, dfe1_tap3, dfe2_tap3, 4);
	comb_val_t3x("	 dfe_tap4", dfe0_tap4, dfe1_tap4, dfe2_tap4, 4);
	comb_val_t3x("	 dfe_tap5", dfe0_tap5, dfe1_tap5, dfe2_tap5, 4);
	comb_val_t3x("	 dfe_tap6", dfe0_tap6, dfe1_tap6, dfe2_tap6, 4);
	comb_val_t3x("	 dfe_tap7", dfe0_tap7, dfe1_tap7, dfe2_tap7, 4);
	comb_val_t3x("	 dfe_tap8", dfe0_tap8, dfe1_tap8, dfe2_tap8, 4);

	comb_val_t3x("slicer_ofst0", sli0_ofst0, sli1_ofst0, sli2_ofst0, 5);
	comb_val_t3x("slicer_ofst1", sli0_ofst1, sli1_ofst1, sli2_ofst1, 5);
	comb_val_t3x("slicer_ofst2", sli0_ofst2, sli1_ofst2, sli2_ofst2, 5);

	rx_pr("cdr_code=[%d,%d,%d]\n", cdr0_code, cdr1_code, cdr2_code);
	rx_pr("cdr_lock=[%d,%d,%d]\n", cdr0_lock, cdr1_lock, cdr2_lock);
	comb_val_t3x("cdr_int", cdr0_int, cdr1_int, cdr2_int, 7);
}

void aml_phy_short_bist_t3x(void)
{
}

void aml_phy_get_cdr_int_avr_t3x(u32 cdr_ed_mode, u32 *ch0, u32 *ch1, u32 *ch2)
{
}

int aml_phy_get_iq_skew_val_t3x(u32 val_0, u32 val_1)
{
	int val = val_0 - val_1;

	rx_pr("val=%d\n", val);
	if (val)
		return (val - 32);
	else
		return (val + 128 - 32);
}

/* IQ skew monitor */
void aml_phy_iq_skew_monitor_t3x(void)
{
}

bool aml_get_tmds_valid_t3x(void)
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

void aml_phy_power_off_t3x(void)
{
}

void aml_phy_switch_port_t3x(void)
{
}

void dump_vsi_reg_t3x(void)
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

unsigned int rx_sec_hdcp_cfg_t3x(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDMI_RX_HDCP_CFG, 0, 0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0) & 0xffffffff);
}

void rx_set_irq_t3x(bool en)
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

		hdmirx_wr_cor(RX_GRP_INTR1_MASK_PWD_IVCRX, 0x25);
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
		hdmirx_wr_cor(RX_INTR13_MASK_PWD_IVCRX, 0x02);// int
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
void rx_set_aud_output_t3x(u32 param)
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

void rx_sw_reset_t3x(int level)
{
	/* deep color fifo */
	hdmirx_wr_bits_cor(RX_PWD_SRST_PWD_IVCRX, _BIT(4), 1);
	udelay(1);
	hdmirx_wr_bits_cor(RX_PWD_SRST_PWD_IVCRX, _BIT(4), 0);
	//TODO..
}

void hdcp_init_t3x(void)
{
	u8 data8;
	//key config and crc check
	//rx_sec_hdcp_cfg_t3x();
	//hdcp config

	//======================================
	// HDCP 2.X Config ---- RX
	//======================================
	hdmirx_wr_cor(RX_HPD_C_CTRL_AON_IVCRX, 0x1);//HPD
	//todo: enable hdcp22 according hdcp burning
	hdmirx_wr_cor(RX_HDCP2x_CTRL_PWD_IVCRX, 0x01);//ri_hdcp2x_en
	//hdmirx_wr_cor(RX_INTR13_MASK_PWD_IVCRX, 0x02);// irq
	hdmirx_wr_cor(PWD_SW_CLMP_AUE_OIF_PWD_IVCRX, 0x0);

	data8 = 0;
	data8 |= (hdmirx_repeat_support() && rx.hdcp.repeat) << 1;
	hdmirx_wr_cor(CP2PAX_CTRL_0_HDCP2X_IVCRX, data8);
	//depth
	hdmirx_wr_cor(CP2PAX_RPT_DEPTH_HDCP2X_IVCRX, 0);
	//dev cnt
	hdmirx_wr_cor(CP2PAX_RPT_DEVCNT_HDCP2X_IVCRX, 0);
	//
	data8 = 0;
	data8 |= 0 << 0; //hdcp1dev
	data8 |= 0 << 1; //hdcp1dev
	data8 |= 0 << 2; //max_casc
	data8 |= 0 << 3; //max_devs
	hdmirx_wr_cor(CP2PAX_RPT_DETAIL_HDCP2X_IVCRX, data8);

	hdmirx_wr_cor(CP2PAX_RX_CTRL_0_HDCP2X_IVCRX, 0x83);
	hdmirx_wr_cor(CP2PAX_RX_CTRL_0_HDCP2X_IVCRX, 0x80);

	//======================================
	// HDCP 1.X Config ---- RX
	//======================================
	hdmirx_wr_cor(RX_SYS_SWTCHC_AON_IVCRX, 0x86);//SYS_SWTCHC,Enable HDCP DDC,SCDC DDC

	//----clear ksv fifo rdy --------
	data8  =  0;
	data8 |= (1 << 3);//bit[  3] reg_hdmi_clr_en
	data8 |= (7 << 0);//bit[2:0] reg_fifordy_clr_en
	hdmirx_wr_cor(RX_RPT_RDY_CTRL_PWD_IVCRX, data8);//register address: 0x1010 (0x0f)

	//----BCAPS config-----
	data8 = 0;
	data8 |= (0 << 4);//bit[4] reg_fast I2C transfers speed.
	data8 |= (0 << 5);//bit[5] reg_fifo_rdy
	data8 |= ((hdmirx_repeat_support() && rx.hdcp.repeat) << 6);//bit[6] reg_repeater
	data8 |= (1 << 7);//bit[7] reg_hdmi_capable  HDMI capable
	hdmirx_wr_cor(RX_BCAPS_SET_HDCP1X_IVCRX, data8);//register address: 0x169e (0x80)

	//for (data8 = 0; data8 < 10; data8++) //ksv list number
		//hdmirx_wr_cor(RX_KSV_FIFO_HDCP1X_IVCRX, ksvlist[data8]);

	//----Bstatus1 config-----
	data8 =  0;
	// data8 |= (2 << 0); //bit[6:0] reg_dev_cnt
	data8 |= (0 << 7);//bit[  7] reg_dve_exceed
	hdmirx_wr_cor(RX_SHD_BSTATUS1_HDCP1X_IVCRX, data8);//register address: 0x169f (0x00)

		//----Bstatus2 config-----
	data8 =  0;
	// data8 |= (2 << 0);//bit[2:0] reg_depth
	data8 |= (0 << 3);//bit[  3] reg_casc_exceed
	hdmirx_wr_cor(RX_SHD_BSTATUS2_HDCP1X_IVCRX, data8);//register address: 0x169f (0x00)

	//----Rx Sha length in bytes----
	hdmirx_wr_cor(RX_SHA_length1_HDCP1X_IVCRX, 0x0a);//[7:0] 10=2ksv*5byte
	hdmirx_wr_cor(RX_SHA_length2_HDCP1X_IVCRX, 0x00);//[9:8]

	//----Rx Sha repeater KSV fifo start addr----
	hdmirx_wr_cor(RX_KSV_SHA_start1_HDCP1X_IVCRX, 0x00);//[7:0]
	hdmirx_wr_cor(RX_KSV_SHA_start2_HDCP1X_IVCRX, 0x00);//[9:8]
	//hdmirx_wr_cor(CP2PAX_INTR0_MASK_HDCP2X_IVCRX, 0x3); irq
	//hdmirx_wr_cor(RX_HDCP2x_CTRL_PWD_IVCRX, 0x1); //same as L3309
	//hdmirx_wr_cor(CP2PA_TP1_HDCP2X_IVCRX, 0x9e);
	//hdmirx_wr_cor(CP2PA_TP3_HDCP2X_IVCRX, 0x32);
	//hdmirx_wr_cor(CP2PA_TP5_HDCP2X_IVCRX, 0x32);
	//hdmirx_wr_cor(CP2PAX_GP_IN1_HDCP2X_IVCRX, 0x2);
	//hdmirx_wr_cor(CP2PAX_GP_CTL_HDCP2X_IVCRX, 0xdb);
	hdmirx_wr_cor(RX_PWD_SRST2_PWD_IVCRX, 0x8);
	hdmirx_wr_cor(RX_PWD_SRST2_PWD_IVCRX, 0x2);
}

