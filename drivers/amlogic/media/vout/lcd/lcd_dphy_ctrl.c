// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "./lcd_reg.h"
#include "./lcd_common.h"

void lcd_mipi_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	unsigned int bit_lane_sel;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
		if (pdrv->index == 0) {
			bit_lane_sel = 0;
		} else if (pdrv->index == 1) {
			bit_lane_sel = 16;
		} else {
			LCDERR("[%d]: %s: invalid drv_index\n", pdrv->index, __func__);
			return;
		}
		if (on_off) {
			// sel dphy lane
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, 0x0, bit_lane_sel, 10);
		}
		break;
	default:
		break;
	}
}

void lcd_edp_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	unsigned int reg_dphy_tx_ctrl0, reg_dphy_tx_ctrl1;
	unsigned int bit_data_in_lvds, bit_data_in_edp, bit_lane_sel;

	if (pdrv->index == 0) {
		reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
		reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
		bit_data_in_lvds = 0;
		bit_data_in_edp = 1;
		bit_lane_sel = 0;
	} else if (pdrv->index == 1) {
		reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
		reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
		bit_data_in_lvds = 2;
		bit_data_in_edp = 3;
		bit_lane_sel = 16;
	} else {
		LCDERR("[%d]: %s: invalid drv_index\n", pdrv->index, __func__);
		return;
	}

	if (on_off) {
		// sel dphy data_in
		lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 0, bit_data_in_lvds, 1);
		lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 1, bit_data_in_edp, 1);
		// sel dphy lane
		lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, 0x155, bit_lane_sel, 10);
		// sel edp fifo clkdiv 20, enable lane
		lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl0, ((0x4 << 5) | (0x1f << 16)));
		// fifo enable
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 6, 10);
		// fifo wr enable
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 7, 10);
	} else {
		// fifo wr disable
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 7, 10);
		// fifo disable
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 6, 10);
		// lane disable
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0, 16, 8);
	}
}

void lcd_lvds_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	unsigned int reg_dphy_tx_ctrl0, reg_dphy_tx_ctrl1;
	unsigned int bit_data_in_lvds = 0, bit_data_in_edp = 0, bit_lane_sel = 0;
	unsigned int val_lane_sel = 0, len_lane_sel = 0;
	unsigned int dual_port, phy_div;

	phy_div = pdrv->config.control.lvds_cfg.dual_port ? 2 : 1;
	dual_port = pdrv->config.control.lvds_cfg.dual_port & 0x1;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
		if (pdrv->index == 0) { /* lane0~lane4 */
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
			bit_data_in_lvds = 0;
			bit_data_in_edp = 1;
			bit_lane_sel = 0;
			val_lane_sel = 0x155;
			len_lane_sel = 10;
		} else if (pdrv->index == 1) { /* lane10~lane14 */
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
			bit_data_in_lvds = 2;
			bit_data_in_edp = 3;
			bit_lane_sel = 20;
			val_lane_sel = 0x155;
			len_lane_sel = 10;
		} else {
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1;
			bit_data_in_lvds = 4;
			bit_data_in_edp = 0xff;
			bit_lane_sel = 10;
			// dual_port ? lane5~lane14 : lane5~lane9
			val_lane_sel = dual_port ? 0xaaaaa : 0x2aa;
			len_lane_sel = dual_port ? 20 : 10;
		}
		break;
	case LCD_CHIP_T3:
		reg_dphy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL0;
		reg_dphy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL1;
		break;
	default:
		reg_dphy_tx_ctrl0 = HHI_LVDS_TX_PHY_CNTL0;
		reg_dphy_tx_ctrl1 = HHI_LVDS_TX_PHY_CNTL1;
		break;
	}

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
		if (on_off) {
			// sel dphy data_in
			if (bit_data_in_edp < 0xff)
				lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 0, bit_data_in_edp, 1);
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 1, bit_data_in_lvds, 1);
			// sel dphy lane
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, val_lane_sel,
				bit_lane_sel, len_lane_sel);
			/* set fifo_clk_sel: div 7 */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl0, (1 << 5));
			/* set cntl_ser_en:  8-channel */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0x3ff, 16, 10);
			/* decoupling fifo enable, gated clock enable */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl1, (1 << 6) | (1 << 0));
			/* decoupling fifo write enable after fifo enable */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 7, 1);
		} else {
			/* disable fifo */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 6, 2);
			/* disable lane */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0, 16, 16);
		}
		break;
	case LCD_CHIP_T3:
		if (on_off) {
			/* set fifo_clk_sel: div 7 */
			lcd_ana_write(reg_dphy_tx_ctrl0, (1 << 6));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 0xfff, 16, 12);
			/* pn swap */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 1, 2, 1);
			/* decoupling fifo enable, gated clock enable */
			lcd_ana_write(reg_dphy_tx_ctrl1, (1 << 30) | (1 << 24));
			/* decoupling fifo write enable after fifo enable */
			lcd_ana_setb(reg_dphy_tx_ctrl1, 1, 31, 1);
		} else {
			/* disable fifo */
			lcd_ana_setb(reg_dphy_tx_ctrl1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 0, 16, 12);
		}
		break;
	default:
		if (on_off) {
			/* set fifo_clk_sel: div 7 */
			lcd_ana_write(reg_dphy_tx_ctrl0, (1 << 6));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 0xfff, 16, 12);
			/* pn swap */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 1, 2, 1);
			/* decoupling fifo enable, gated clock enable */
			lcd_ana_write(reg_dphy_tx_ctrl1,
				(1 << 30) | ((phy_div - 1) << 25) | (1 << 24));
			/* decoupling fifo write enable after fifo enable */
			lcd_ana_setb(reg_dphy_tx_ctrl1, 1, 31, 1);
		} else {
			/* disable fifo */
			lcd_ana_setb(reg_dphy_tx_ctrl1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 0, 16, 12);
		}
		break;
	}
}

void lcd_vbyone_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	unsigned int div_sel, phy_div;
	unsigned int reg_dphy_tx_ctrl0, reg_dphy_tx_ctrl1;
	unsigned int bit_data_in_lvds = 0, bit_data_in_edp = 0, bit_lane_sel = 0;
	unsigned int bit_fifo_clk = 0, cntl_ser_mask = 0, cntl_ser_bit = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	phy_div = pdrv->config.control.vbyone_cfg.phy_div;

	if (pdrv->config.basic.lcd_bits == 6)
		div_sel = 0;
	else if (pdrv->config.basic.lcd_bits == 8)
		div_sel = 2;
	else // pdrv->config.basic.lcd_bits == 10
		div_sel = 3;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
		if (pdrv->index == 0) {
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
			bit_data_in_lvds = 0;
			bit_data_in_edp = 1;
			bit_lane_sel = 0;
		} else { // drv1
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
			bit_data_in_lvds = 2;
			bit_data_in_edp = 3;
			bit_lane_sel = 16;
		}
		break;
	case LCD_CHIP_T3:
		if (pdrv->index == 0) {
			reg_dphy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL0;
			reg_dphy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL1;
			cntl_ser_bit = 12;
			cntl_ser_mask = 0xfff;
			bit_fifo_clk = 1;
		} else { // drv1
			reg_dphy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL2;
			reg_dphy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL3;
			cntl_ser_bit = 4;
			cntl_ser_mask = 0xf;
			bit_fifo_clk = 3;
		}
		break;
	default:
		reg_dphy_tx_ctrl0 = HHI_LVDS_TX_PHY_CNTL0;
		reg_dphy_tx_ctrl1 = HHI_LVDS_TX_PHY_CNTL1;
		break;
	}

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
		if (on_off) {
			// sel dphy data_in
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 0, bit_data_in_edp, 1);
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 1, bit_data_in_lvds, 1);
			// sel dphy lane
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, 0x5555, bit_lane_sel, 16);
			/* set fifo_clk_sel: div 7 */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl0, (div_sel << 5));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0xff, 16, 8);
			/* decoupling fifo enable, gated clock enable */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl1, (1 << 6) | (1 << 0));
			/* decoupling fifo write enable after fifo enable */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 7, 1);
		} else {
			/* disable fifo */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 6, 2);
			/* disable lane */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0, 16, 16);
		}
		break;
	case LCD_CHIP_T3:
		if (on_off) {
			/* set fifo_clk_sel: div 7 */
			lcd_ana_write(reg_dphy_tx_ctrl0, (div_sel << 6));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_ana_setb(reg_dphy_tx_ctrl0, cntl_ser_mask, 16, cntl_ser_bit);
			/* pn swap */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 1, 2, 1);
			/* decoupling fifo enable, gated clock enable */
			lcd_ana_write(reg_dphy_tx_ctrl1, (1 << 30) | (bit_fifo_clk << 24));
			/* decoupling fifo write enable after fifo enable */
			lcd_ana_setb(reg_dphy_tx_ctrl1, 1, 31, 1);
		} else {
			/* disable fifo */
			lcd_ana_setb(reg_dphy_tx_ctrl1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 0, 16, 12);
		}
		break;
	default:
		if (on_off) {
			/* set fifo_clk_sel: div 7 */
			lcd_ana_write(reg_dphy_tx_ctrl0, (div_sel << 6));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 0xfff, 16, 12);
			/* pn swap */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 1, 2, 1);
			/* decoupling fifo enable, gated clock enable */
			lcd_ana_write(reg_dphy_tx_ctrl1,
				(1 << 30) | ((phy_div - 1) << 25) | (1 << 24));
			/* decoupling fifo write enable after fifo enable */
			lcd_ana_setb(reg_dphy_tx_ctrl1, 1, 31, 1);
		} else {
			/* disable fifo */
			lcd_ana_setb(reg_dphy_tx_ctrl1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 0, 16, 12);
		}
		break;
	}
}

void lcd_mlvds_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	unsigned int div_sel;
	unsigned int channel_sel0, channel_sel1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	/* phy_div: 0=div6, 1=div 7, 2=div8, 3=div10 */
	if (pdrv->config.basic.lcd_bits == 6)
		div_sel = 0;
	else // lcd_bits == 8
		div_sel = 2;

	channel_sel0 = pdrv->config.control.mlvds_cfg.channel_sel0;
	channel_sel1 = pdrv->config.control.mlvds_cfg.channel_sel1;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T3:
		if (on_off) {
			/* fifo_clk_sel[7:6]: 0=div6, 1=div 7, 2=div8, 3=div10 */
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL0, (div_sel << 6));
			/* serializer_en[27:16] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
			/* pn swap[2] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 1, 2, 1);
			/* fifo enable[30], phy_clock gating[24] */
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL1, (1 << 30) | (1 << 24));
			/* fifo write enable[31] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 1, 31, 1);
			// lane_swap_set
			lcd_vcbus_write(P2P_CH_SWAP0_T7, channel_sel0);
			lcd_vcbus_write(P2P_CH_SWAP1_T7, channel_sel1);
		} else {
			/* disable fifo */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		}
		break;
	default:
		if (on_off) {
			/* fifo_clk_sel[7:6]: 0=div6, 1=div 7, 2=div8, 3=div10 */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL0, (div_sel << 6));
			/* serializer_en[27:16] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
			/* pn swap[2] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 1, 2, 1);
			/* fifo enable[30], phy_clock gating[24] */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL1, (1 << 30) | (1 << 24));
			/* fifo write enable[31] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 1, 31, 1);
			// lane_swap_set
			if (pdrv->data->chip_type == LCD_CHIP_T5W) {
				lcd_vcbus_write(P2P_CH_SWAP0_T7, channel_sel0);
				lcd_vcbus_write(P2P_CH_SWAP1_T7, channel_sel1);
			} else {
				lcd_vcbus_write(P2P_CH_SWAP0, channel_sel0);
				lcd_vcbus_write(P2P_CH_SWAP1, channel_sel1);
			}
		} else {
			/* disable fifo */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		}
		break;
	}
}

void lcd_p2p_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	unsigned int phy_div, channel_sel0, channel_sel1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	/* phy_div: 0=div6, 1=div 7, 2=div8, 3=div10 */
	switch (pdrv->config.control.p2p_cfg.p2p_type) {
	case P2P_CHPI: /* 8/10 coding */
	case P2P_USIT:
		phy_div = 3;
		break;
	default:
		phy_div = 2;
		break;
	}

	channel_sel0 = pdrv->config.control.p2p_cfg.channel_sel0;
	channel_sel1 = pdrv->config.control.p2p_cfg.channel_sel1;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T3:
		if (on_off) {
			/* fifo_clk_sel[7:6]: 0=div6, 1=div 7, 2=div8, 3=div10 */
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL0, (phy_div << 6));
			/* serializer_en[27:16] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
			/* pn swap[2] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 1, 2, 1);

			/* fifo enable[30], phy_clock gating[24] */
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL1, (1 << 30) | (1 << 24));
			/* fifo write enable[31] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 1, 31, 1);
			// lane_swap_set
			lcd_vcbus_write(P2P_CH_SWAP0_T7, channel_sel0);
			lcd_vcbus_write(P2P_CH_SWAP1_T7, channel_sel1);
		} else {
			/* disable fifo */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		}
		break;
	default:
		if (on_off) {
			/* fifo_clk_sel[7:6]: 0=div6, 1=div 7, 2=div8, 3=div10 */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL0, (phy_div << 6));
			/* serializer_en[27:16] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
			/* pn swap[2] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 1, 2, 1);
			/* fifo enable[30], phy_clock gating[24] */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL1, (1 << 30) | (1 << 24));
			/* fifo write enable[31] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 1, 31, 1);
			// lane_swap_set
			if (pdrv->data->chip_type == LCD_CHIP_T5W) {
				lcd_vcbus_write(P2P_CH_SWAP0_T7, channel_sel0);
				lcd_vcbus_write(P2P_CH_SWAP1_T7, channel_sel1);
			} else {
				lcd_vcbus_write(P2P_CH_SWAP0, channel_sel0);
				lcd_vcbus_write(P2P_CH_SWAP1, channel_sel1);
			}
		} else {
			/* disable fifo */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		}
		break;
	}
}
