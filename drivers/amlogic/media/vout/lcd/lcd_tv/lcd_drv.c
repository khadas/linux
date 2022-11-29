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
#include <linux/sched/clock.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_tv.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

static int lcd_type_supported(struct lcd_config_s *pconf)
{
	int lcd_type = pconf->basic.lcd_type;
	int ret = -1;

	switch (lcd_type) {
	case LCD_LVDS:
	case LCD_VBYONE:
	case LCD_MLVDS:
	case LCD_P2P:
		ret = 0;
		break;
	default:
		LCDERR("invalid lcd type: %s(%d)\n",
		       lcd_type_type_to_str(lcd_type), lcd_type);
		break;
	}
	return ret;
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
			/* set cntl_ser_en:  8-channel to 1 */
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

static void lcd_mlvds_control_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int div_sel, channel_sel0, channel_sel1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s\n", __func__);

	/* phy_div: 0=div6, 1=div 7, 2=div8, 3=div10 */
	switch (pdrv->config.basic.lcd_bits) {
	case 6:
		div_sel = 0;
		break;
	case 8:
	default:
		div_sel = 2;
		break;
	}

	channel_sel0 = pdrv->config.control.mlvds_cfg.channel_sel0;
	channel_sel1 = pdrv->config.control.mlvds_cfg.channel_sel1;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T3:
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

		lcd_vcbus_write(P2P_CH_SWAP0_T7, channel_sel0);
		lcd_vcbus_write(P2P_CH_SWAP1_T7, channel_sel1);
		break;
	case LCD_CHIP_T5W:
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

		lcd_vcbus_write(P2P_CH_SWAP0_T7, channel_sel0);
		lcd_vcbus_write(P2P_CH_SWAP1_T7, channel_sel1);
		break;
	default:
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

		lcd_vcbus_write(P2P_CH_SWAP0, channel_sel0);
		lcd_vcbus_write(P2P_CH_SWAP1, channel_sel1);
		break;
	}

	lcd_tcon_enable(pdrv);
}

static void lcd_mlvds_disable(struct aml_lcd_drv_s *pdrv)
{
	lcd_tcon_disable(pdrv);

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T3:
		/* disable fifo */
		lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 0, 30, 2);
		/* disable lane */
		lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		break;
	default:
		/* disable fifo */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
		/* disable lane */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		break;
	}
}

static void lcd_p2p_control_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int phy_div, channel_sel0, channel_sel1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s\n", __func__);

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

		lcd_vcbus_write(P2P_CH_SWAP0_T7, channel_sel0);
		lcd_vcbus_write(P2P_CH_SWAP1_T7, channel_sel1);
		break;
	case LCD_CHIP_T5W:
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

		lcd_vcbus_write(P2P_CH_SWAP0_T7, channel_sel0);
		lcd_vcbus_write(P2P_CH_SWAP1_T7, channel_sel1);
		break;
	default:
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

		lcd_vcbus_write(P2P_CH_SWAP0, channel_sel0);
		lcd_vcbus_write(P2P_CH_SWAP1, channel_sel1);
		break;
	}

	lcd_tcon_enable(pdrv);
}

static void lcd_p2p_disable(struct aml_lcd_drv_s *pdrv)
{
	lcd_tcon_disable(pdrv);

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T3:
		/* disable fifo */
		lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 0, 30, 2);
		/* disable lane */
		lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		break;
	default:
		/* disable fifo */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
		/* disable lane */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		break;
	}
}

void lcd_tv_clk_config_change(struct aml_lcd_drv_s *pdrv)
{
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.lcd_clk);
#endif

	switch (pdrv->config.basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_config_set(pdrv);
		break;
	case LCD_MLVDS:
		lcd_mlvds_config_set(pdrv);
		break;
	case LCD_P2P:
		lcd_p2p_config_set(pdrv);
		break;
	default:
		break;
	}

	lcd_clk_generate_parameter(pdrv);
}

void lcd_tv_clk_update(struct aml_lcd_drv_s *pdrv)
{
	lcd_tv_clk_config_change(pdrv);

	if (pdrv->config.basic.lcd_type == LCD_VBYONE)
		lcd_vbyone_interrupt_enable(pdrv, 0);
	lcd_set_clk(pdrv);
	if (pdrv->config.basic.lcd_type == LCD_VBYONE)
		lcd_vbyone_wait_stable(pdrv);
}

void lcd_tv_config_update(struct aml_lcd_drv_s *pdrv)
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
	case LCD_MLVDS:
		lcd_mlvds_config_set(pdrv);
		break;
	case LCD_P2P:
		lcd_p2p_config_set(pdrv);
		break;
	default:
		break;
	}
}

void lcd_tv_driver_init_pre(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	LCDPR("[%d]: tv driver init(ver %s): %s\n",
	      pdrv->index, LCD_DRV_VERSION,
	      lcd_type_type_to_str(pdrv->config.basic.lcd_type));
	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return;

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

void lcd_tv_driver_disable_post(struct aml_lcd_drv_s *pdrv)
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

int lcd_tv_driver_init(struct aml_lcd_drv_s *pdrv)
{
	int ret;
	unsigned long long local_time[3];

	local_time[0] = sched_clock();

	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return -1;

	switch (pdrv->config.basic.lcd_type) {
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
	case LCD_MLVDS:
		lcd_mlvds_control_set(pdrv);
		lcd_mlvds_pinmux_set(pdrv, 1);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		break;
	case LCD_P2P:
		lcd_p2p_pinmux_set(pdrv, 1);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		lcd_p2p_control_set(pdrv);
		break;
	default:
		break;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
	local_time[1] = sched_clock();
	pdrv->config.cus_ctrl.driver_init_time = local_time[1] - local_time[0];
	return 0;
}

void lcd_tv_driver_disable(struct aml_lcd_drv_s *pdrv)
{
	int ret;
	unsigned long long local_time[3];

	local_time[0] = sched_clock();

	LCDPR("[%d]: disable driver\n", pdrv->index);
	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return;

	switch (pdrv->config.basic.lcd_type) {
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
	case LCD_MLVDS:
		lcd_mlvds_disable(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_mlvds_pinmux_set(pdrv, 0);
		break;
	case LCD_P2P:
		lcd_p2p_disable(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_p2p_pinmux_set(pdrv, 0);
		break;
	default:
		break;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
	local_time[1] = sched_clock();
	pdrv->config.cus_ctrl.driver_disable_time = local_time[1] - local_time[0];
}

int lcd_tv_driver_change(struct aml_lcd_drv_s *pdrv)
{
	int ret;
	unsigned long long local_time[3];

	local_time[0] = sched_clock();

	LCDPR("[%d]: tv driver change(ver %s): %s\n",
	      pdrv->index, LCD_DRV_VERSION,
	      lcd_type_type_to_str(pdrv->config.basic.lcd_type));
	ret = lcd_type_supported(&pdrv->config);
	if (ret)
		return -1;

	lcd_tv_config_update(pdrv);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_clk_request(pdrv->lcd_vpu_dev, pdrv->config.timing.lcd_clk);
#endif

	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		if (pdrv->config.basic.lcd_type == LCD_VBYONE) {
			if (pdrv->status & LCD_STATUS_IF_ON)
				lcd_vbyone_interrupt_enable(pdrv, 0);
		}

		lcd_clk_change(pdrv);
		lcd_venc_change(pdrv);

		if (pdrv->config.basic.lcd_type == LCD_VBYONE) {
			if (pdrv->status & LCD_STATUS_IF_ON)
				lcd_vbyone_wait_stable(pdrv);
		}
	} else {
		/* only change parameters when panel is off */
		switch (pdrv->config.timing.clk_change) {
		case LCD_CLK_PLL_CHANGE:
			lcd_clk_generate_parameter(pdrv);
			break;
		default:
			break;
		}
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s finished\n", pdrv->index, __func__);
	local_time[1] = sched_clock();
	pdrv->config.cus_ctrl.driver_change_time = local_time[1] - local_time[0];
	return 0;
}
