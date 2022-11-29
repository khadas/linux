// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_tablet.h"
#include "mipi_dsi_util.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

static int lcd_type_supported(struct lcd_config_s *pconf)
{
	int lcd_type = pconf->basic.lcd_type;
	int ret = -1;

	switch (lcd_type) {
	case LCD_TTL:
	case LCD_LVDS:
	case LCD_VBYONE:
	case LCD_MIPI:
	case LCD_EDP:
		ret = 0;
		break;
	default:
		LCDERR("invalid lcd type: %s(%d)\n",
		       lcd_type_type_to_str(lcd_type), lcd_type);
		break;
	}
	return ret;
}

static void lcd_ttl_control_set(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int clk_pol, rb_swap, bit_swap;

	clk_pol = pconf->control.ttl_cfg.clk_pol;
	rb_swap = (pconf->control.ttl_cfg.swap_ctrl >> 1) & 1;
	bit_swap = (pconf->control.ttl_cfg.swap_ctrl >> 0) & 1;

	lcd_vcbus_setb(L_POL_CNTL_ADDR, clk_pol, 6, 1);
	lcd_vcbus_setb(L_DUAL_PORT_CNTL_ADDR, rb_swap, 1, 1);
	lcd_vcbus_setb(L_DUAL_PORT_CNTL_ADDR, bit_swap, 0, 1);
}

static void lcd_mipi_control_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int bit_lane_sel;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	switch (pdrv->index) {
	case 0:
		bit_lane_sel = 0;
		break;
	case 1:
		bit_lane_sel = 8;
		break;
	default:
		LCDERR("[%d]: %s: invalid drv_index\n",
		       pdrv->index, __func__);
		return;
	}

	if (pdrv->data->chip_type == LCD_CHIP_T7) {
		// sel dphy lane
		lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1,
				    0x0, bit_lane_sel, 10);
	}

	mipi_dsi_tx_ctrl(pdrv, 1);
}

static void lcd_mipi_disable(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	mipi_dsi_tx_ctrl(pdrv, 0);
}

static void lcd_edp_control_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_dphy_tx_ctrl0, reg_dphy_tx_ctrl1;
	unsigned int bit_data_in_lvds, bit_data_in_edp, bit_lane_sel;

	switch (pdrv->index) {
	case 0:
		reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
		reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
		bit_data_in_lvds = 0;
		bit_data_in_edp = 1;
		bit_lane_sel = 0;
		break;
	case 1:
		reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
		reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
		bit_data_in_lvds = 2;
		bit_data_in_edp = 3;
		bit_lane_sel = 8;
		break;
	default:
		LCDERR("[%d]: %s: invalid drv_index\n",
		       pdrv->index, __func__);
		return;
	}

	// sel dphy data_in
	lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 0, bit_data_in_lvds, 1);
	lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 1, bit_data_in_edp, 1);
	// sel dphy lane
	lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, 0x155, bit_lane_sel, 10);

	// sel edp fifo clkdiv 20, enable lane
	lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl0,
			     ((0x4 << 5) | (0x1f << 16)));

	// fifo enable
	lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 6, 10);
	// fifo wr enable
	lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 7, 10);

	edp_tx_ctrl(pdrv, 1);
}

static void lcd_edp_disable(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_dphy_tx_ctrl0, reg_dphy_tx_ctrl1;

	switch (pdrv->index) {
	case 0:
		reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
		reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
		break;
	case 1:
		reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
		reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
		break;
	default:
		LCDERR("[%d]: %s: invalid drv_index\n",
		       pdrv->index, __func__);
		return;
	}

	edp_tx_ctrl(pdrv, 0);

	// fifo wr disable
	lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 7, 10);
	// fifo disable
	lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 6, 10);
	// lane disable
	lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0, 16, 8);
}

static void lcd_lvds_clk_util_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_phy_tx_ctrl0, reg_phy_tx_ctrl1;
	unsigned int bit_data_in_lvds, bit_data_in_edp, bit_lane_sel;
	unsigned int phy_div, val_lane_sel, len_lane_sel;

	if (pdrv->config.control.lvds_cfg.dual_port)
		phy_div = 2;
	else
		phy_div = 1;

	if (pdrv->data->chip_type == LCD_CHIP_T7) {
		switch (pdrv->index) {
		case 0: /* lane0~lane4 */
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			reg_phy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
			bit_data_in_lvds = 0;
			bit_data_in_edp = 1;
			bit_lane_sel = 0;
			val_lane_sel = 0x155;
			len_lane_sel = 10;
			break;
		case 1: /* lane10~lane14 */
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			reg_phy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
			bit_data_in_lvds = 2;
			bit_data_in_edp = 3;
			bit_lane_sel = 20;
			val_lane_sel = 0x155;
			len_lane_sel = 10;
			break;
		case 2:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0;
			reg_phy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1;
			bit_data_in_lvds = 4;
			bit_data_in_edp = 0xff;
			if (pdrv->config.control.lvds_cfg.dual_port) { /* lane5~lane14 */
				bit_lane_sel = 10;
				val_lane_sel = 0xaaaaa;
				len_lane_sel = 20;
			} else { /* lane5~lane9 */
				bit_lane_sel = 10;
				val_lane_sel = 0x2aa;
				len_lane_sel = 10;
			}
			break;
		default:
			LCDERR("[%d]: %s: invalid drv_index\n",
			       pdrv->index, __func__);
			return;
		}

		// sel dphy data_in
		if (bit_data_in_edp < 0xff) {
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 0,
					    bit_data_in_edp, 1);
		}
		lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 1,
				    bit_data_in_lvds, 1);
		// sel dphy lane
		lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, val_lane_sel,
				    bit_lane_sel, len_lane_sel);

		/* set fifo_clk_sel: div 7 */
		lcd_combo_dphy_write(pdrv, reg_phy_tx_ctrl0, (1 << 5));
		/* set cntl_ser_en:  8-channel */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0x3ff, 16, 10);

		/* decoupling fifo enable, gated clock enable */
		lcd_combo_dphy_write(pdrv, reg_phy_tx_ctrl1,
				     (1 << 6) | (1 << 0));
		/* decoupling fifo write enable after fifo enable */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl1, 1, 7, 1);
	} else if (pdrv->data->chip_type == LCD_CHIP_T3) {
		/* set fifo_clk_sel: div 7 */
		lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL0, (1 << 6));
		/* set cntl_ser_en:  8-channel to 1 */
		lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
		/* pn swap */
		lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 1, 2, 1);

		/* decoupling fifo enable, gated clock enable */
		lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL1, (1 << 30) | (1 << 24));
		/* decoupling fifo write enable after fifo enable */
		lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 1, 31, 1);
	} else {
		/* set fifo_clk_sel: div 7 */
		lcd_ana_write(HHI_LVDS_TX_PHY_CNTL0, (1 << 6));
		/* set cntl_ser_en:  8-channel to 1 */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
		/* pn swap */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 1, 2, 1);

		/* decoupling fifo enable, gated clock enable */
		lcd_ana_write(HHI_LVDS_TX_PHY_CNTL1,
			      (1 << 30) | ((phy_div - 1) << 25) | (1 << 24));
		/* decoupling fifo write enable after fifo enable */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 1, 31, 1);
	}
}

static void lcd_lvds_control_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_lvds_pack_ctrl, reg_lvds_gen_ctrl;
	unsigned int bit_num, pn_swap, port_swap, lane_reverse;
	unsigned int dual_port, fifo_mode, lvds_repack;
	unsigned int ch_reg0, ch_reg1, offset;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	lcd_lvds_clk_util_set(pdrv);

	offset = pdrv->data->offset_venc_if[pdrv->index];
	lvds_repack = (pdrv->config.control.lvds_cfg.lvds_repack) & 0x3;
	pn_swap   = (pdrv->config.control.lvds_cfg.pn_swap) & 0x1;
	dual_port = (pdrv->config.control.lvds_cfg.dual_port) & 0x1;
	port_swap = (pdrv->config.control.lvds_cfg.port_swap) & 0x1;
	lane_reverse = (pdrv->config.control.lvds_cfg.lane_reverse) & 0x1;

	switch (pdrv->config.basic.lcd_bits) {
	case 10:
		bit_num = 0;
		break;
	case 6:
		bit_num = 2;
		break;
	case 8:
	default:
		bit_num = 1;
		break;
	}
	if (dual_port)
		fifo_mode = 0x3;
	else
		fifo_mode = 0x1;

	if (pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3 ||
	    pdrv->data->chip_type == LCD_CHIP_T5W) {
		reg_lvds_pack_ctrl = LVDS_PACK_CNTL_ADDR_T7 + offset;
		reg_lvds_gen_ctrl = LVDS_GEN_CNTL_T7 + offset;
		lcd_vcbus_write(LVDS_SER_EN_T7 + offset, 0xfff);
	} else {
		reg_lvds_pack_ctrl = LVDS_PACK_CNTL_ADDR;
		reg_lvds_gen_ctrl = LVDS_GEN_CNTL;
	}

	lcd_vcbus_write(reg_lvds_pack_ctrl,
			(lvds_repack << 0) | /* repack //[1:0] */
			(0 << 3) |	/* reserve */
			(0 << 4) |	/* lsb first */
			(pn_swap << 5) | /* pn swap */
			(dual_port << 6) | /* dual port */
			(0 << 7) |	/* use tcon control */
			/* 0:10bits, 1:8bits, 2:6bits, 3:4bits */
			(bit_num << 8) |
			(0 << 10) |	/* r_select  //0:R, 1:G, 2:B, 3:0 */
			(1 << 12) |	/* g_select  //0:R, 1:G, 2:B, 3:0 */
			(2 << 14));	/* b_select  //0:R, 1:G, 2:B, 3:0 */

	/* lvsd swap */
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TL1:
	case LCD_CHIP_TM2:
		/* lvds channel:    //tx 12 channels
		 *    0: d0_a
		 *    1: d1_a
		 *    2: d2_a
		 *    3: clk_a
		 *    4: d3_a
		 *    5: d4_a
		 *    6: d0_b
		 *    7: d1_b
		 *    8: d2_b
		 *    9: clk_b
		 *    a: d3_b
		 *    b: d4_b
		 */
		if (port_swap) {
			if (lane_reverse) {
				lcd_vcbus_write(P2P_CH_SWAP0, 0x456789ab);
				lcd_vcbus_write(P2P_CH_SWAP1, 0x0123);
			} else {
				lcd_vcbus_write(P2P_CH_SWAP0, 0x10ba9876);
				lcd_vcbus_write(P2P_CH_SWAP1, 0x5432);
			}
		} else {
			if (lane_reverse) {
				lcd_vcbus_write(P2P_CH_SWAP0, 0xab012345);
				lcd_vcbus_write(P2P_CH_SWAP1, 0x6789);
			} else {
				lcd_vcbus_write(P2P_CH_SWAP0, 0x76543210);
				lcd_vcbus_write(P2P_CH_SWAP1, 0xba98);
			}
		}
		break;
	case LCD_CHIP_T5:
	case LCD_CHIP_T5D:
		/* lvds channel:    //tx 12 channels
		 *    0: d0_a
		 *    1: d1_a
		 *    2: d2_a
		 *    3: clk_a
		 *    4: d3_a
		 *    5: d4_a
		 *    6: d0_b
		 *    7: d1_b
		 *    8: d2_b
		 *    9: clk_b
		 *    a: d3_b
		 *    b: d4_b
		 */
		if (port_swap) {
			if (lane_reverse) {
				lcd_vcbus_write(P2P_CH_SWAP0, 0x345789ab);
				lcd_vcbus_write(P2P_CH_SWAP1, 0x0612);
			} else {
				lcd_vcbus_write(P2P_CH_SWAP0, 0x210a9876);
				lcd_vcbus_write(P2P_CH_SWAP1, 0x5b43);
			}
		} else {
			if (lane_reverse) {
				lcd_vcbus_write(P2P_CH_SWAP0, 0xab12345);
				lcd_vcbus_write(P2P_CH_SWAP1, 0x60789);
			} else {
				lcd_vcbus_write(P2P_CH_SWAP0, 0x87643210);
				lcd_vcbus_write(P2P_CH_SWAP1, 0xb5a9);
			}
		}
		break;
	case LCD_CHIP_T7:
		/* lvds channel:    //tx 12 channels
		 *    0: d0_a
		 *    1: d1_a
		 *    2: d2_a
		 *    3: clk_a
		 *    4: d3_a
		 *    5: d4_a
		 *    6: d0_b
		 *    7: d1_b
		 *    8: d2_b
		 *    9: clk_b
		 *    a: d3_b
		 *    b: d4_b
		 */
		ch_reg0 = P2P_CH_SWAP0_T7 + offset;
		ch_reg1 = P2P_CH_SWAP1_T7 + offset;
		if (pdrv->index == 0) {
			//don't support port_swap and lane_reverse
			lcd_vcbus_write(ch_reg0, 0xfff43210);
			lcd_vcbus_write(ch_reg1, 0xffff);
		} else if (pdrv->index == 1) {
			lcd_vcbus_write(ch_reg0, 0xf43210ff);
			lcd_vcbus_write(ch_reg1, 0xffff);
		} else {
			if (dual_port) {
				if (port_swap) {
					if (lane_reverse) {
						lcd_vcbus_write(ch_reg0, 0x345789ab);
						lcd_vcbus_write(ch_reg1, 0x0612);
					} else {
						lcd_vcbus_write(ch_reg0, 0x210a9876);
						lcd_vcbus_write(ch_reg1, 0x5b43);
					}
				} else {
					if (lane_reverse) {
						lcd_vcbus_write(ch_reg0, 0x9ab12345);
						lcd_vcbus_write(ch_reg1, 0x6078);
					} else {
						lcd_vcbus_write(ch_reg0, 0x87643210);
						lcd_vcbus_write(ch_reg1, 0xb5a9);
					}
				}
			} else {
				lcd_vcbus_write(ch_reg0, 0xfff43210);
				lcd_vcbus_write(ch_reg1, 0xffff);
			}
		}
		lcd_vcbus_write(P2P_BIT_REV_T7 + offset, 2);
		break;
	case LCD_CHIP_T3:
	case LCD_CHIP_T5W:
		/* lvds channel:    //tx 12 channels
		 *    0: d0_a
		 *    1: d1_a
		 *    2: d2_a
		 *    3: clk_a
		 *    4: d3_a
		 *    5: d4_a
		 *    6: d0_b
		 *    7: d1_b
		 *    8: d2_b
		 *    9: clk_b
		 *    a: d3_b
		 *    b: d4_b
		 */
		ch_reg0 = P2P_CH_SWAP0_T7 + offset;
		ch_reg1 = P2P_CH_SWAP1_T7 + offset;
		if (port_swap) {
			if (lane_reverse) {
				lcd_vcbus_write(ch_reg0, 0x456789ab);
				lcd_vcbus_write(ch_reg1, 0x0123);
			} else {
				lcd_vcbus_write(ch_reg0, 0x10ba9876);
				lcd_vcbus_write(ch_reg1, 0x5432);
			}
		} else {
			if (lane_reverse) {
				lcd_vcbus_write(ch_reg0, 0xab012345);
				lcd_vcbus_write(ch_reg1, 0x6789);
			} else {
				lcd_vcbus_write(ch_reg0, 0x76543210);
				lcd_vcbus_write(ch_reg1, 0xba98);
			}
		}
		lcd_vcbus_write(P2P_BIT_REV_T7 + offset, 2);
		break;
	default:
		break;
	}

	lcd_vcbus_setb(reg_lvds_gen_ctrl, 1, 4, 1);
	lcd_vcbus_setb(reg_lvds_gen_ctrl, fifo_mode, 0, 2);
	lcd_vcbus_setb(reg_lvds_gen_ctrl, 1, 3, 1);
}

static void lcd_lvds_disable(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_dphy_tx_ctrl0, reg_dphy_tx_ctrl1, offset = 0;

	if (pdrv->data->chip_type == LCD_CHIP_T7) {
		switch (pdrv->index) {
		case 0:
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
			break;
		case 1:
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
			break;
		case 2:
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1;
			break;
		default:
			LCDERR("[%d]: %s: invalid drv_index\n",
			       pdrv->index, __func__);
			return;
		}
		offset = pdrv->data->offset_venc_if[pdrv->index];

		/* disable lvds fifo */
		lcd_vcbus_setb(LVDS_GEN_CNTL_T7 + offset, 0, 3, 1);
		lcd_vcbus_setb(LVDS_GEN_CNTL_T7 + offset, 0, 0, 2);
		/* disable fifo */
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 6, 2);
		/* disable lane */
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0, 16, 10);
	} else if (pdrv->data->chip_type == LCD_CHIP_T3) {
		/* disable lvds fifo */
		lcd_vcbus_setb(LVDS_GEN_CNTL_T7, 0, 3, 1);
		lcd_vcbus_setb(LVDS_GEN_CNTL_T7, 0, 0, 2);
		/* disable fifo */
		lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 0, 30, 2);
		/* disable lane */
		lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0, 16, 12);
	} else if (pdrv->data->chip_type == LCD_CHIP_T5W) {
		/* disable lvds fifo */
		lcd_vcbus_setb(LVDS_GEN_CNTL_T7, 0, 3, 1);
		lcd_vcbus_setb(LVDS_GEN_CNTL_T7, 0, 0, 2);
		/* disable fifo */
		lcd_clk_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
		/* disable lane */
		lcd_clk_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
	} else {
		/* disable lvds fifo */
		lcd_vcbus_setb(LVDS_GEN_CNTL, 0, 3, 1);
		lcd_vcbus_setb(LVDS_GEN_CNTL, 0, 0, 2);
		/* disable fifo */
		lcd_clk_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
		/* disable lane */
		lcd_clk_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
	}
}

static void lcd_vbyone_clk_util_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int lcd_bits, div_sel, phy_div;
	unsigned int reg_phy_tx_ctrl0, reg_phy_tx_ctrl1;
	unsigned int bit_data_in_lvds, bit_data_in_edp, bit_lane_sel;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	phy_div = pdrv->config.control.vbyone_cfg.phy_div;
	lcd_bits = pdrv->config.basic.lcd_bits;
	switch (lcd_bits) {
	case 6:
		div_sel = 0;
		break;
	case 8:
		div_sel = 2;
		break;
	case 10:
	default:
		div_sel = 3;
		break;
	}

	if (pdrv->data->chip_type == LCD_CHIP_T7) {
		switch (pdrv->index) {
		case 0:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			reg_phy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
			bit_data_in_lvds = 0;
			bit_data_in_edp = 1;
			bit_lane_sel = 0;
			break;
		case 1:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			reg_phy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
			bit_data_in_lvds = 2;
			bit_data_in_edp = 3;
			bit_lane_sel = 16;
			break;
		default:
			LCDERR("[%d]: %s: invalid drv_index\n",
			       pdrv->index, __func__);
			return;
		}

		// sel dphy data_in
		lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0,
				    0, bit_data_in_edp, 1);
		lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0,
				    1, bit_data_in_lvds, 1);
		// sel dphy lane
		lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1,
				    0x5555, bit_lane_sel, 16);

		/* set fifo_clk_sel: div 10 */
		lcd_combo_dphy_write(pdrv, reg_phy_tx_ctrl0, (div_sel << 5));
		/* set cntl_ser_en:  8-channel to 1 */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0xff, 16, 8);

		/* decoupling fifo enable, gated clock enable */
		lcd_combo_dphy_write(pdrv, reg_phy_tx_ctrl1,
				     (1 << 6) | (1 << 0));
		/* decoupling fifo write enable after fifo enable */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl1, 1, 7, 1);
	} else if (pdrv->data->chip_type == LCD_CHIP_T3) {
		switch (pdrv->index) {
		case 0:
			reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL0;
			reg_phy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL1;
			/* set fifo_clk_sel: div 10 */
			lcd_ana_write(reg_phy_tx_ctrl0, (div_sel << 6));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_ana_setb(reg_phy_tx_ctrl0, 0xfff, 16, 12);
			/* pn swap */
			lcd_ana_setb(reg_phy_tx_ctrl0, 1, 2, 1);

			/* decoupling fifo enable, gated clock enable */
			lcd_ana_write(reg_phy_tx_ctrl1, (1 << 30) | (1 << 24));
			/* decoupling fifo write enable after fifo enable */
			lcd_ana_setb(reg_phy_tx_ctrl1, 1, 31, 1);
			break;
		case 1:
			reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL2;
			reg_phy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL3;
			/* set fifo_clk_sel: div 10 */
			lcd_ana_write(reg_phy_tx_ctrl0, (div_sel << 6));
			/* set cntl_ser_en:  4-channel to 1 */
			lcd_ana_setb(reg_phy_tx_ctrl0, 0xf, 16, 4);
			/* pn swap */
			lcd_ana_setb(reg_phy_tx_ctrl0, 1, 2, 1);

			/* decoupling fifo enable, gated clock enable */
			lcd_ana_write(reg_phy_tx_ctrl1, (1 << 30) | (3 << 24));
			/* decoupling fifo write enable after fifo enable */
			lcd_ana_setb(reg_phy_tx_ctrl1, 1, 31, 1);
			break;
		default:
			LCDERR("[%d]: %s: invalid drv_index\n",
			       pdrv->index, __func__);
			return;
		}
	} else {
		/* set fifo_clk_sel: div 10 */
		lcd_ana_write(HHI_LVDS_TX_PHY_CNTL0, (div_sel << 6));
		/* set cntl_ser_en:  8-channel to 1 */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
		/* pn swap */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 1, 2, 1);

		/* decoupling fifo enable, gated clock enable */
		lcd_ana_write(HHI_LVDS_TX_PHY_CNTL1,
			      (1 << 30) | ((phy_div - 1) << 25) | (1 << 24));
		/* decoupling fifo write enable after fifo enable */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 1, 31, 1);
	}
}

static void lcd_vbyone_control_set(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	lcd_vbyone_clk_util_set(pdrv);

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
	case LCD_CHIP_T3:
	case LCD_CHIP_T5W:
		lcd_vbyone_enable_t7(pdrv);
		break;
	default:
		lcd_vbyone_enable_dft(pdrv);
		break;
	}
}

static void lcd_vbyone_control_off(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_dphy_tx_ctrl0, reg_dphy_tx_ctrl1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
		switch (pdrv->index) {
		case 0:
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
			break;
		case 1:
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
			break;
		default:
			LCDERR("[%d]: %s: invalid drv_index\n",
				pdrv->index, __func__);
			return;
		}

		lcd_vbyone_disable_t7(pdrv);
		/* disable fifo */
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 6, 2);
		/* disable lane */
		lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0, 16, 8);
		break;
	case LCD_CHIP_T3:
		switch (pdrv->index) {
		case 0:
			reg_dphy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL0;
			reg_dphy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL1;
			break;
		case 1:
			reg_dphy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL2;
			reg_dphy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL3;
			break;
		default:
			LCDERR("[%d]: %s: invalid drv_index\n",
				pdrv->index, __func__);
			return;
		}

		lcd_vbyone_disable_t7(pdrv);
		/* disable fifo */
		lcd_ana_setb(reg_dphy_tx_ctrl1, 0, 30, 2);
		/* disable lane */
		lcd_ana_setb(reg_dphy_tx_ctrl0, 0, 16, 12);
		break;
	case LCD_CHIP_T5W:
		lcd_vbyone_disable_t7(pdrv);
		/* disable fifo */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
		/* disable lane */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		break;
	default:
		lcd_vbyone_disable_dft(pdrv);
		/* disable fifo */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
		/* disable lane */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		break;
	}
}

void lcd_tablet_clk_config_change(struct aml_lcd_drv_s *pdrv)
{
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.lcd_clk);
#endif

	switch (pdrv->config.basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_config_set(pdrv);
		break;
	case LCD_MIPI:
		lcd_mipi_dsi_config_set(pdrv);
		break;
	default:
		break;
	}

	lcd_clk_generate_parameter(pdrv);
}

void lcd_tablet_clk_update(struct aml_lcd_drv_s *pdrv)
{
	lcd_tablet_clk_config_change(pdrv);

	if (pdrv->config.basic.lcd_type == LCD_VBYONE)
		lcd_vbyone_interrupt_enable(pdrv, 0);
	lcd_set_clk(pdrv);
	if (pdrv->config.basic.lcd_type == LCD_VBYONE)
		lcd_vbyone_wait_stable(pdrv);
}

void lcd_tablet_config_update(struct aml_lcd_drv_s *pdrv)
{
	struct vinfo_s *info;

	/* update vinfo */
	info = &pdrv->vinfo;
	info->sync_duration_num = pdrv->config.timing.sync_duration_num;
	info->sync_duration_den = pdrv->config.timing.sync_duration_den;
	info->frac = pdrv->config.timing.frac;
	info->std_duration = pdrv->config.timing.frame_rate;

	/* update clk & timing config */
	lcd_vmode_change(pdrv);
	info->video_clk = pdrv->config.timing.lcd_clk;
	info->htotal = pdrv->config.basic.h_period;
	info->vtotal = pdrv->config.basic.v_period;
	/* update interface timing */
	switch (pdrv->config.basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_config_set(pdrv);
		break;
	case LCD_MIPI:
		lcd_mipi_dsi_config_set(pdrv);
		break;
	case LCD_EDP:
		lcd_edp_config_set(pdrv);
	default:
		break;
	}
}

void lcd_tablet_config_post_update(struct aml_lcd_drv_s *pdrv)
{
	/* update interface timing */
	switch (pdrv->config.basic.lcd_type) {
	case LCD_MIPI:
		lcd_mipi_dsi_config_post(pdrv);
		break;
	default:
		break;
	}
}

void lcd_tablet_driver_init_pre(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	LCDPR("[%d]: tablet driver init(ver %s): %s\n",
	      pdrv->index, LCD_DRV_VERSION,
	      lcd_type_type_to_str(pdrv->config.basic.lcd_type));
	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return;

	lcd_tablet_config_update(pdrv);
	lcd_tablet_config_post_update(pdrv);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.lcd_clk);
	vpu_dev_mem_power_on(pdrv->lcd_vpu_dev);
#endif
	lcd_clk_gate_switch(pdrv, 1);

	/* init driver */
	switch (pdrv->config.basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_interrupt_enable(pdrv, 0);
		break;
	default:
		break;
	}

	lcd_set_clk(pdrv);
	lcd_set_venc(pdrv);
	pdrv->mute_state = 1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
}

void lcd_tablet_driver_disable_post(struct aml_lcd_drv_s *pdrv)
{
	lcd_venc_enable(pdrv, 0); /* disable encl */

	lcd_disable_clk(pdrv);
	lcd_clk_gate_switch(pdrv, 0);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_mem_power_down(pdrv->lcd_vpu_dev);
	vpu_dev_clk_release(pdrv->lcd_vpu_dev);
#endif

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
}

int lcd_tablet_driver_init(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return -1;

	switch (pdrv->config.basic.lcd_type) {
	case LCD_TTL:
		lcd_ttl_control_set(pdrv);
		lcd_ttl_pinmux_set(pdrv, 1);
		break;
	case LCD_LVDS:
		lcd_lvds_control_set(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		break;
	case LCD_VBYONE:
		lcd_vbyone_pinmux_set(pdrv, 1);
		lcd_vbyone_control_set(pdrv);
		lcd_vbyone_wait_hpd(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		lcd_vbyone_power_on_wait_stable(pdrv);
		break;
	case LCD_MIPI:
		lcd_phy_set(pdrv, LCD_PHY_ON);
		lcd_mipi_control_set(pdrv);
		break;
	case LCD_EDP:
		lcd_edp_pinmux_set(pdrv, 1);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		lcd_edp_control_set(pdrv);
		break;
	default:
		break;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
	return 0;
}

void lcd_tablet_driver_disable(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	LCDPR("[%d]: disable driver\n", pdrv->index);
	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return;

	switch (pdrv->config.basic.lcd_type) {
	case LCD_TTL:
		lcd_ttl_pinmux_set(pdrv, 0);
		break;
	case LCD_LVDS:
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_lvds_disable(pdrv);
		break;
	case LCD_VBYONE:
		lcd_vbyone_link_maintain_clear();
		lcd_vbyone_interrupt_enable(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_vbyone_pinmux_set(pdrv, 0);
		lcd_vbyone_control_off(pdrv);
		break;
	case LCD_MIPI:
		mipi_dsi_link_off(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_mipi_disable(pdrv);
		break;
	case LCD_EDP:
		lcd_edp_disable(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_edp_pinmux_set(pdrv, 0);
		break;
	default:
		break;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
}
