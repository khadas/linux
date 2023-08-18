// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/compiler.h>
#include <linux/arm-smccc.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/hdmi_tx21/enc_clk_config.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>

#include "common.h"
#include "../hdmi_tx.h"
/*
 * Config hdmitx Data Flow metering
 * bw_type: 1: Full BW
 *          2: non Full BW
 *          0: default
 */
void hdmitx_dfm_cfg(u8 bw_type, u16 val)
{
	switch (bw_type) {
	case 1:
		pr_info("%s Full BW tribytes %d\n", __func__, val);
		hdmitx21_wr_reg(H21TXSB_SPARE_0_IVCTX, 0xf3);
		hdmitx21_wr_reg(H21TXSB_SPARE_1_IVCTX, 0x7d);
		hdmitx21_wr_reg(H21TXSB_SPARE_3_IVCTX, 0xff);
		hdmitx21_wr_reg(H21TXSB_SPARE_2_IVCTX, 0xd0);
		hdmitx21_wr_reg(H21TXSB_SPARE_4_IVCTX, val & 0xff);
		hdmitx21_wr_reg(H21TXSB_SPARE_5_IVCTX, (val >> 8) & 0xff);
		hdmitx21_wr_reg(H21TXSB_SPARE_6_IVCTX, 0xf3);
		break;
	case 2:
		pr_info("%s non Full BW\n", __func__);
		hdmitx21_wr_reg(H21TXSB_SPARE_0_IVCTX, 0xf0);
		hdmitx21_wr_reg(H21TXSB_SPARE_1_IVCTX, 0x00);
		hdmitx21_wr_reg(H21TXSB_SPARE_3_IVCTX, 0xff);
		hdmitx21_wr_reg(H21TXSB_SPARE_2_IVCTX, 0x00);
		hdmitx21_wr_reg(H21TXSB_SPARE_4_IVCTX, 0x00);
		hdmitx21_wr_reg(H21TXSB_SPARE_5_IVCTX, 0x00);
		hdmitx21_wr_reg(H21TXSB_SPARE_6_IVCTX, 0x00);
		break;
	case 0:
	default:
		pr_info("%s default BW\n", __func__);
		hdmitx21_wr_reg(H21TXSB_SPARE_0_IVCTX, 0x00);
		hdmitx21_wr_reg(H21TXSB_SPARE_1_IVCTX, 0x00);
		hdmitx21_wr_reg(H21TXSB_SPARE_3_IVCTX, 0x00);
		hdmitx21_wr_reg(H21TXSB_SPARE_2_IVCTX, 0x00);
		hdmitx21_wr_reg(H21TXSB_SPARE_4_IVCTX, 0x00);
		hdmitx21_wr_reg(H21TXSB_SPARE_5_IVCTX, 0x00);
		hdmitx21_wr_reg(H21TXSB_SPARE_6_IVCTX, 0x00);
		break;
	}
}

//================== LTS 1============================
void scdc_bus_stall_set(bool en)
{
	hdmitx21_set_reg_bits(SCDC_CTL_IVCTX, en, 5, 1);
}

bool flt_tx_cmd_execute(u8 lt_cmd)
{
	hdmitx21_set_reg_bits(HT_DIG_CTL0_PHY_IVCTX, lt_cmd | 0x10, 0, 5);

	return hdmitx21_rd_reg(HT_LTP_ST_PHY_IVCTX & 0x1);
}

/* TODO */
void frl_tx_tx_enable(bool enable)
{
	hdmitx21_wr_reg(HT_TOP_CTL_PHY_IVCTX, 0x24 | !!enable);
}

void frl_tx_av_enable(bool enable)
{
	hdmitx21_set_reg_bits(H21TXSB_CTRL_1_IVCTX, !enable, 1, 1);
}

void frl_tx_sb_enable(bool enable, enum frl_rate_enum frl_rate)
{
	hdmitx21_set_reg_bits(H21TXSB_CTRL_IVCTX, !!enable, 0, 1);
	hdmitx21_set_reg_bits(H21TXSB_CTRL_IVCTX, frl_rate > FRL_6G3L ? 9 : 1, 4, 4);
}

void tmds_tx_phy_set(void)
{
	hdmitx21_set_reg_bits(SOC_FUNC_SEL_IVCTX, 0, 5, 1);
	hdmitx21_set_reg_bits(SOC_FUNC_SEL_IVCTX, 0, 4, 1);
	hdmitx21_set_reg_bits(SW_RST_IVCTX, 0, 7, 1);
}

void frl_tx_tx_phy_set(void)
{ /* TODO */
	hdmitx21_set_reg_bits(SOC_FUNC_SEL_IVCTX, 1, 5, 1);
	hdmitx21_set_reg_bits(SOC_FUNC_SEL_IVCTX, 1, 4, 1);
	hdmitx21_set_reg_bits(SW_RST_IVCTX, 1, 7, 1);
}

/* sync from uboot */
void frl_tx_lts_1_hdmi21_config(void)
{
	u8 data8;

	//Step1:Tx reads EDID
	//Assume that Rx supports FRL

	//Step2:initial Tx Phy,TODO

	//Step3:initial Tx Controller
	data8  = 0;
	data8 |= (1 << 0); //[7:0] reg_pkt_period
	hdmitx21_wr_reg(H21TXSB_PKT_PRD_IVCTX, data8);
}

/* FRL link training pattern transmission start */
bool frl_tx_pattern_init(u16 patterns)
{
	hdmitx21_wr_reg(HT_DIG_CTL9_PHY_IVCTX, 0x07);
	hdmitx21_wr_reg(HT_DIG_CTL23_PHY_IVCTX, 0x38);
	hdmitx21_wr_reg(HT_DIG_CTL24_PHY_IVCTX, 0x00);

	/* Disable SCDC reg pattern select */
	hdmitx21_wr_reg(HT_DIG_CTL0_PHY_IVCTX, 0x00);

	/* Update local pattern registers */
	hdmitx21_wr_reg(HT_DIG_CTL1_PHY_IVCTX, patterns & 0xff);
	hdmitx21_wr_reg(HT_DIG_CTL2_PHY_IVCTX, (patterns >> 8) & 0xff);

	/* Initialize PRBS value */
	hdmitx21_wr_reg(HT_DIG_CTL3_PHY_IVCTX, 0x00);
	hdmitx21_wr_reg(HT_DIG_CTL4_PHY_IVCTX, 0x00);

	hdmitx21_wr_reg(HT_DIG_CTL21_PHY_IVCTX, 0x04);

	/* Enable calibration pattern transmit */
	hdmitx21_wr_reg(HT_DIG_CTL22_PHY_IVCTX, 0x00);
	hdmitx21_wr_reg(HT_DIG_CTL22_PHY_IVCTX, 0x02);

	hdmitx21_wr_reg(HT_DIG_CTL23_PHY_IVCTX, 0x58);

	return true;
}

void frl_tx_pattern_stop(void)
{
	hdmitx21_wr_reg(HT_DIG_CTL22_PHY_IVCTX, 0x00);

	/* Put the state machine into state 0 and then set reg to default */
	hdmitx21_wr_reg(HT_DIG_CTL23_PHY_IVCTX, 0x08);
	hdmitx21_wr_reg(HT_DIG_CTL23_PHY_IVCTX, 0x00);
}

void frl_tx_pin_swap_set(bool en)
{
	hdmitx21_wr_reg(HDMICTL2_IVCTX, en ? 0x23 : 0x32);
}

bool frl_tx_pattern_set(enum ltp_patterns frl_pat, u8 lane)
{
	u16 pat_reg, lane_mask, pat_val;

	pat_val = frl_pat;
	pat_reg = hdmitx21_rd_reg(HT_DIG_CTL1_PHY_IVCTX);
	pat_reg |= hdmitx21_rd_reg(HT_DIG_CTL2_PHY_IVCTX) << 8;

	lane_mask = 0x0F << (lane * 4);
	pat_val <<= (lane * 4);
	pat_reg &= ~lane_mask;
	pat_reg |= pat_val;
	hdmitx21_wr_reg(HT_DIG_CTL1_PHY_IVCTX, pat_reg & 0xff);
	hdmitx21_wr_reg(HT_DIG_CTL2_PHY_IVCTX, (pat_reg >> 8) & 0xff);
	/* Initialize PRBS value */
	hdmitx21_wr_reg(HT_DIG_CTL3_PHY_IVCTX, 0x00);
	hdmitx21_wr_reg(HT_DIG_CTL4_PHY_IVCTX, 0x00);

	return true;
}

bool frl_tx_ffe_set(enum ffe_levels ffe_level, u8 lane)
{
	/* TODO */
	return true;
}
