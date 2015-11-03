/*
 * drivers/amlogic/display/vout/lcd/mipi_dsi_reg.h
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

#ifndef MIPI_DSI_PHY_REG
#define MIPI_DSI_PHY_REG
#include <linux/amlogic/iomap.h>

/* ***********************************************
 * DSI Host Controller register offset address define
 * VCBUS_BASE = 0x2c(0x2c00 - 0x2cff)
 * *********************************************** */
/* DWC IP registers */
#define MIPI_DSI_DWC_VERSION_OS                    0x2c00
#define MIPI_DSI_DWC_PWR_UP_OS                     0x2c01
#define MIPI_DSI_DWC_CLKMGR_CFG_OS                 0x2c02
#define MIPI_DSI_DWC_DPI_VCID_OS                   0x2c03
#define MIPI_DSI_DWC_DPI_COLOR_CODING_OS           0x2c04
#define MIPI_DSI_DWC_DPI_CFG_POL_OS                0x2c05
#define MIPI_DSI_DWC_DPI_LP_CMD_TIM_OS             0x2c06
#define MIPI_DSI_DWC_PCKHDL_CFG_OS                 0x2c0b
#define MIPI_DSI_DWC_GEN_VCID_OS                   0x2c0c
#define MIPI_DSI_DWC_MODE_CFG_OS                   0x2c0d
#define MIPI_DSI_DWC_VID_MODE_CFG_OS               0x2c0e
#define MIPI_DSI_DWC_VID_PKT_SIZE_OS               0x2c0f
#define MIPI_DSI_DWC_VID_NUM_CHUNKS_OS             0x2c10
#define MIPI_DSI_DWC_VID_NULL_SIZE_OS              0x2c11
#define MIPI_DSI_DWC_VID_HSA_TIME_OS               0x2c12
#define MIPI_DSI_DWC_VID_HBP_TIME_OS               0x2c13
#define MIPI_DSI_DWC_VID_HLINE_TIME_OS             0x2c14
#define MIPI_DSI_DWC_VID_VSA_LINES_OS              0x2c15
#define MIPI_DSI_DWC_VID_VBP_LINES_OS              0x2c16
#define MIPI_DSI_DWC_VID_VFP_LINES_OS              0x2c17
#define MIPI_DSI_DWC_VID_VACTIVE_LINES_OS          0x2c18
#define MIPI_DSI_DWC_EDPI_CMD_SIZE_OS              0x2c19
#define MIPI_DSI_DWC_CMD_MODE_CFG_OS               0x2c1a
#define MIPI_DSI_DWC_GEN_HDR_OS                    0x2c1b
#define MIPI_DSI_DWC_GEN_PLD_DATA_OS               0x2c1c
#define MIPI_DSI_DWC_CMD_PKT_STATUS_OS             0x2c1d
#define MIPI_DSI_DWC_TO_CNT_CFG_OS                 0x2c1e
#define MIPI_DSI_DWC_HS_RD_TO_CNT_OS               0x2c1f
#define MIPI_DSI_DWC_LP_RD_TO_CNT_OS               0x2c20
#define MIPI_DSI_DWC_HS_WR_TO_CNT_OS               0x2c21
#define MIPI_DSI_DWC_LP_WR_TO_CNT_OS               0x2c22
#define MIPI_DSI_DWC_BTA_TO_CNT_OS                 0x2c23
#define MIPI_DSI_DWC_SDF_3D_OS                     0x2c24
#define MIPI_DSI_DWC_LPCLK_CTRL_OS                 0x2c25
#define MIPI_DSI_DWC_PHY_TMR_LPCLK_CFG_OS          0x2c26
#define MIPI_DSI_DWC_PHY_TMR_CFG_OS                0x2c27
#define MIPI_DSI_DWC_PHY_RSTZ_OS                   0x2c28
#define MIPI_DSI_DWC_PHY_IF_CFG_OS                 0x2c29
#define MIPI_DSI_DWC_PHY_ULPS_CTRL_OS              0x2c2a
#define MIPI_DSI_DWC_PHY_TX_TRIGGERS_OS            0x2c2b
#define MIPI_DSI_DWC_PHY_STATUS_OS                 0x2c2c
#define MIPI_DSI_DWC_PHY_TST_CTRL0_OS              0x2c2d
#define MIPI_DSI_DWC_PHY_TST_CTRL1_OS              0x2c2e
#define MIPI_DSI_DWC_INT_ST0_OS                    0x2c2f
#define MIPI_DSI_DWC_INT_ST1_OS                    0x2c30
#define MIPI_DSI_DWC_INT_MSK0_OS                   0x2c31
#define MIPI_DSI_DWC_INT_MSK1_OS                   0x2c32
/* Top-level registers */
/* [31: 3]    Reserved.     Default 0.
 *     [2] RW dpi_rst_n: Default 1.
		1=Assert SW reset on mipi_dsi_host_dpi block.   0=Release reset.
 *     [1] RW intr_rst_n: Default 1.
		1=Assert SW reset on mipi_dsi_host_intr block.  0=Release reset.
 *     [0] RW dwc_rst_n:  Default 1.
		1=Assert SW reset on IP core.   0=Release reset. */
#define MIPI_DSI_TOP_SW_RESET                      0x2cf0
/* [31: 5] Reserved.   Default 0.
 *     [4] RW manual_edpihalt: Default 0.
		1=Manual suspend VencL; 0=do not suspend VencL.
 *     [3] RW auto_edpihalt_en: Default 0.
		1=Enable IP's edpihalt signal to suspend VencL;
		0=IP's edpihalt signal does not affect VencL.
 *     [2] RW clock_freerun: Apply to auto-clock gate only. Default 0.
		0=Default, use auto-clock gating to save power;
		1=use free-run clock, disable auto-clock gating, for debug mode.
 *     [1] RW enable_pixclk: A manual clock gate option, due to DWC IP does not
		have auto-clock gating. 1=Enable pixclk.      Default 0.
 *     [0] RW enable_sysclk: A manual clock gate option, due to DWC IP does not
		have auto-clock gating. 1=Enable sysclk.      Default 0. */
#define MIPI_DSI_TOP_CLK_CNTL                      0x2cf1
/* [31:24]    Reserved. Default 0.
 * [23:20] RW dpi_color_mode: Define DPI pixel format. Default 0.
		0=16-bit RGB565 config 1;
		1=16-bit RGB565 config 2;
		2=16-bit RGB565 config 3;
		3=18-bit RGB666 config 1;
		4=18-bit RGB666 config 2;
		5=24-bit RGB888;
		6=20-bit YCbCr 4:2:2;
		7=24-bit YCbCr 4:2:2;
		8=16-bit YCbCr 4:2:2;
		9=30-bit RGB;
		10=36-bit RGB;
		11=12-bit YCbCr 4:2:0.
 *    [19] Reserved. Default 0.
 * [18:16] RW in_color_mode:  Define VENC data width. Default 0.
		0=30-bit pixel;
		1=24-bit pixel;
		2=18-bit pixel, RGB666;
		3=16-bit pixel, RGB565.
 * [15:14] RW chroma_subsample: Define method of chroma subsampling. Default 0.
		Applicable to YUV422 or YUV420 only.
		0=Use even pixel's chroma;
		1=Use odd pixel's chroma;
		2=Use averaged value between even and odd pair.
 * [13:12] RW comp2_sel:  Select which component to be Cr or B: Default 2.
		0=comp0; 1=comp1; 2=comp2.
 * [11:10] RW comp1_sel:  Select which component to be Cb or G: Default 1.
		0=comp0; 1=comp1; 2=comp2.
 *  [9: 8] RW comp0_sel:  Select which component to be Y  or R: Default 0.
		0=comp0; 1=comp1; 2=comp2.
 *     [7]    Reserved. Default 0.
 *     [6] RW de_pol:  Default 0.
		If DE input is active low, set to 1 to invert to active high.
 *     [5] RW hsync_pol: Default 0.
		If HS input is active low, set to 1 to invert to active high.
 *     [4] RW vsync_pol: Default 0.
		If VS input is active low, set to 1 to invert to active high.
 *     [3] RW dpicolorm: Signal to IP.   Default 0.
 *     [2] RW dpishutdn: Signal to IP.   Default 0.
 *     [1]    Reserved.  Default 0.
 *     [0]    Reserved.  Default 0. */
#define MIPI_DSI_TOP_CNTL                          0x2cf2
#define MIPI_DSI_TOP_SUSPEND_CNTL                  0x2cf3
#define MIPI_DSI_TOP_SUSPEND_LINE                  0x2cf4
#define MIPI_DSI_TOP_SUSPEND_PIX                   0x2cf5
#define MIPI_DSI_TOP_MEAS_CNTL                     0x2cf6
/* [0] R  stat_edpihalt:  edpihalt signal from IP.    Default 0. */
#define MIPI_DSI_TOP_STAT                          0x2cf7
#define MIPI_DSI_TOP_MEAS_STAT_TE0                 0x2cf8
#define MIPI_DSI_TOP_MEAS_STAT_TE1                 0x2cf9
#define MIPI_DSI_TOP_MEAS_STAT_VS0                 0x2cfa
#define MIPI_DSI_TOP_MEAS_STAT_VS1                 0x2cfb
/* [31:16] RW intr_stat/clr. Default 0.
		For each bit, read as this interrupt level status,
		write 1 to clear.
	[31:22] Reserved
	[   21] stat/clr of eof interrupt
	[   21] vde_fall interrupt
	[   19] stat/clr of de_rise interrupt
	[   18] stat/clr of vs_fall interrupt
	[   17] stat/clr of vs_rise interrupt
	[   16] stat/clr of dwc_edpite interrupt
 * [15: 0] RW intr_enable. Default 0.
		For each bit, 1=enable this interrupt, 0=disable.
	[15: 6] Reserved
	[    5] eof interrupt
	[    4] de_fall interrupt
	[    3] de_rise interrupt
	[    2] vs_fall interrupt
	[    1] vs_rise interrupt
	[    0] dwc_edpite interrupt */
#define MIPI_DSI_TOP_INTR_CNTL_STAT                0x2cfc

/* ***********************************************
 * DSI PHY register offset address define
 * *********************************************** */
#define MIPI_DSI_PHY_START      0xd0150000
#define MIPI_DSI_PHY_END        0xd015ffff
/* [31] soft reset for the phy.
		1: reset. 0: dessert the reset.
 * [30] clock lane soft reset.
 * [29] data byte lane 3 soft reset.
 * [28] data byte lane 2 soft reset.
 * [27] data byte lane 1 soft reset.
 * [26] data byte lane 0 soft reset.
 * [25] mipi dsi pll clock selection.
		1:  clock from fixed 850Mhz clock source. 0: from VID2 PLL.
 * [12] mipi HSbyteclk enable.
 * [11] mipi divider clk selection.
		1: select the mipi DDRCLKHS from clock divider.
		0: from PLL clock.
 * [10] mipi clock divider control.
		1: /4. 0: /2.
 * [9]  mipi divider output enable.
 * [8]  mipi divider counter enable.
 * [7]  PLL clock enable.
 * [5]  LPDT data endian.
		1 = transfer the high bit first. 0 : transfer the low bit first.
 * [4]  HS data endian.
 * [3]  force data byte lane in stop mode.
 * [2]  force data byte lane 0 in reciever mode.
 * [1]  write 1 to sync the txclkesc input. the internal logic have to
	use txclkesc to decide Txvalid and Txready.
 * [0]  enalbe the MIPI DSI PHY TxDDRClk. */
#define MIPI_DSI_PHY_CTRL       0x0
/* [31] clk lane tx_hs_en control selection.
		1: from register. 0: use clk lane state machine.
 * [30] register bit for clock lane tx_hs_en.
 * [29] clk lane tx_lp_en contrl selection.
		1: from register. 0: from clk lane state machine.
 * [28] register bit for clock lane tx_lp_en.
 * [27] chan0 tx_hs_en control selection.
		1: from register. 0: from chan0 state machine.
 * [26] register bit for chan0 tx_hs_en.
 * [25] chan0 tx_lp_en control selection.
		1: from register. 0: from chan0 state machine.
 * [24] register bit from chan0 tx_lp_en.
 * [23] chan0 rx_lp_en control selection.
		1: from register. 0: from chan0 state machine.
 * [22] register bit from chan0 rx_lp_en.
 * [21] chan0 contention detection enable control selection.
		1: from register. 0: from chan0 state machine.
 * [20] register bit from chan0 contention dectection enable.
 * [19] chan1 tx_hs_en control selection.
		1: from register. 0: from chan0 state machine.
 * [18] register bit for chan1 tx_hs_en.
 * [17] chan1 tx_lp_en control selection.
		1: from register. 0: from chan0 state machine.
 * [16] register bit from chan1 tx_lp_en.
 * [15] chan2 tx_hs_en control selection.
		1: from register. 0: from chan0 state machine.
 * [14] register bit for chan2 tx_hs_en.
 * [13] chan2 tx_lp_en control selection.
		1: from register. 0: from chan0 state machine.
 * [12] register bit from chan2 tx_lp_en.
 * [11] chan3 tx_hs_en control selection.
		1: from register. 0: from chan0 state machine.
 * [10] register bit for chan3 tx_hs_en.
 * [9]  chan3 tx_lp_en control selection.
		1: from register. 0: from chan0 state machine.
 * [8]  register bit from chan3 tx_lp_en.
 * [4]  clk chan power down. this bit is also used as the power down
	of the whole MIPI_DSI_PHY.
 * [3]  chan3 power down.
 * [2]  chan2 power down.
 * [1]  chan1 power down.
 * [0]  chan0 power down. */
#define MIPI_DSI_CHAN_CTRL      0x1
/* [24]   rx turn watch dog triggered.
 * [23]   rx esc watchdog  triggered.
 * [22]   mbias ready.
 * [21]   txclkesc  synced and ready.
 * [20:17] clk lane state. {mbias_ready, tx_stop, tx_ulps, tx_hs_active}
 * [16:13] chan3 state{0, tx_stop, tx_ulps, tx_hs_active}
 * [12:9]  chan2 state.{0, tx_stop, tx_ulps, tx_hs_active}
 * [8:5]   chan1 state. {0, tx_stop, tx_ulps, tx_hs_active}
 * [4:0]   chan0 state. {TX_STOP, tx_ULPS, hs_active, direction, rxulpsesc} */
#define MIPI_DSI_CHAN_STS       0x2
/* [31:24] TCLK_PREPARE.
 * [23:16] TCLK_ZERO.
 * [15:8]  TCLK_POST.
 * [7:0]   TCLK_TRAIL. */
#define MIPI_DSI_CLK_TIM        0x3
/* [31:24] THS_PREPARE.
 * [23:16] THS_ZERO.
 * [15:8]  THS_TRAIL.
 * [7:0]   THS_EXIT. */
#define MIPI_DSI_HS_TIM         0x4
/* [31:24] tTA_GET.
 * [23:16] tTA_GO.
 * [15:8]  tTA_SURE.
 * [7:0]   tLPX. */
#define MIPI_DSI_LP_TIM         0x5
/* wait time to  MIPI DIS analog ready. */
#define MIPI_DSI_ANA_UP_TIM     0x6
/* TINIT. */
#define MIPI_DSI_INIT_TIM       0x7
/* TWAKEUP. */
#define MIPI_DSI_WAKEUP_TIM     0x8
/* when in RxULPS check state, after the the logic enable the analog,
	how long we should wait to check the lP state . */
#define MIPI_DSI_LPOK_TIM       0x9
/* Watchdog for RX low power state no finished. */
#define MIPI_DSI_LP_WCHDOG      0xa
/* tMBIAS,  after send power up signals to analog,
	how long we should wait for analog powered up. */
#define MIPI_DSI_ANA_CTRL       0xb
/* [31:8]  reserved for future.
 * [7:0]   tCLK_PRE. */
#define MIPI_DSI_CLK_TIM1       0xc
/* watchdog for turn around waiting time. */
#define MIPI_DSI_TURN_WCHDOG    0xd
/* When in RxULPS state, how frequency we should to check
	if the TX side out of ULPS state. */
#define MIPI_DSI_ULPS_CHECK     0xe
/* *********************************************** */

/* ***********************************************
 * register access api
 * *********************************************** */
#define DSI_REG_BASE    0x100000
#define DSI_PHY_BASE    0x150000
static inline unsigned int dsi_reg_read(unsigned int reg)
{
	int ret, val;
	ret = aml_reg_read(IO_APB_BUS_BASE, (DSI_REG_BASE+(reg<<2)), &val);
	if (ret) {
		pr_err("read dsi reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}

static inline void dsi_reg_write(unsigned int reg, unsigned int val)
{
	int ret;
	ret = aml_reg_write(IO_APB_BUS_BASE, (DSI_REG_BASE+(reg<<2)), val);
	if (ret) {
		pr_err("write dsi reg %x error %d\n", reg, ret);
		return;
	} else {
		return;
	}
}

static inline void dsi_reg_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	dsi_reg_write(reg, ((dsi_reg_read(reg) &
		(~(((1L << _len)-1) << _start))) |
		((value & ((1L << _len)-1)) << _start)));
}

static inline unsigned int dsi_reg_getb(unsigned int reg,
		unsigned int _start, unsigned int _len)
{
	return (dsi_reg_read(reg) >> _start) & ((1L << _len)-1);
}

static inline void dsi_reg_set_mask(unsigned int reg, unsigned int _mask)
{
	dsi_reg_write(reg, (dsi_reg_read(reg) | (_mask)));
}

static inline void dsi_reg_clr_mask(unsigned int reg, unsigned int _mask)
{
	dsi_reg_write(reg, (dsi_reg_read(reg) & (~(_mask))));
}

static inline unsigned int dsi_phy_read(unsigned int reg)
{
	int ret, val;
	ret = aml_reg_read(IO_APB_BUS_BASE, (DSI_PHY_BASE+(reg<<2)), &val);
	if (ret) {
		pr_err("read vcbus reg %x error %d\n", reg, ret);
		return -1;
	} else {
		return val;
	}
}

static inline void dsi_phy_write(unsigned int reg, unsigned int val)
{
	int ret;
	ret = aml_reg_write(IO_APB_BUS_BASE, (DSI_PHY_BASE+(reg<<2)), val);
	if (ret) {
		pr_err("write vcbus reg %x error %d\n", reg, ret);
		return;
	} else {
		return;
	}
}

static inline void dsi_phy_setb(unsigned int reg, unsigned int value,
		unsigned int _start, unsigned int _len)
{
	dsi_phy_write(reg, ((dsi_phy_read(reg) &
		(~(((1L << _len)-1) << _start))) |
		((value & ((1L << _len)-1)) << _start)));
}

static inline unsigned int dsi_phy_getb(unsigned int reg,
		unsigned int _start, unsigned int _len)
{
	return (dsi_phy_read(reg) >> _start) & ((1L << _len)-1);
}

static inline void dsi_phy_set_mask(unsigned int reg, unsigned int _mask)
{
	dsi_phy_write(reg, (dsi_phy_read(reg) | (_mask)));
}

static inline void dsi_phy_clr_mask(unsigned int reg, unsigned int _mask)
{
	dsi_phy_write(reg, (dsi_phy_read(reg) & (~(_mask))));
}

#endif
