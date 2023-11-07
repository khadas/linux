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

static void lcd_lvds_lane_swap_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned char ch_reg_idx = 0;
	unsigned int ch_reg0, ch_reg1, offset;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	offset = pdrv->data->offset_venc_if[pdrv->index];

	// 12 channel:
	// d0_a:0 d1_a:1 d2_a:2 clk_a:3 d3_a:4 d4_a:5 d0_b:6 d1_b:7 d2_b:8 clk_b:9 d3_b:a d4_b:b
	// DIF_CH0:d0_a DIF_CH1:d1_a DIF_CH2:d2_a DIF_CH3:clk_a DIF_CH4:d3_a DIF_CH5:d4_a
	// DIF_CH6:d0_b DIF_CH7:d1_b DIF_CH8:d2_b DIF_CH9:clk_b DIF_CH10:d3_b DIF_CH11:d4_b
								// port_swap, lane_reverse
	unsigned int ch_swap_reg_6lane_map0[8] = {0x456789ab, 0x0123,  // 1, 1
						  0x10ba9876, 0x5432,  // 1, 0
						  0xab012345, 0x6789,  // 0, 1
						  0x76543210, 0xba98}; // 0, 0
	// 10/12 channel:
	// d0_a:0 d1_a:1 d2_a:2 clk_a:3 d3_a:4 d4_a:5 d0_b:6 d1_b:7 d2_b:8 clk_b:9 d3_b:a d4_b:b
	// DIF_CH0:d0_a DIF_CH1:d1_a DIF_CH2:d2_a DIF_CH3:clk_a DIF_CH4:d3_a
	// DIF_CH5:d0_b DIF_CH6:d1_b DIF_CH7:d2_b DIF_CH8:clk_b DIF_CH9:d3_b
	// DIF_CH10:d4_a/invalid  DIF_CH11:d4_b/invalid
	unsigned int ch_swap_reg_6lane_map1[8] = {0x345789ab, 0x0612,  // 1, 1
						  0x210a9876, 0x5b43,  // 1, 0
						  0x9ab12345, 0x6078,  // 0, 1
						  0x87643210, 0xb5a9}; // 0, 0

	ch_reg_idx  = pdrv->config.control.lvds_cfg.port_swap ? 0 : 4;
	ch_reg_idx += pdrv->config.control.lvds_cfg.lane_reverse ? 0 : 2;

	/* lvds swap */
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TL1:
	case LCD_CHIP_TM2:
		lcd_vcbus_write(P2P_CH_SWAP0, ch_swap_reg_6lane_map0[ch_reg_idx]);
		lcd_vcbus_write(P2P_CH_SWAP1, ch_swap_reg_6lane_map0[ch_reg_idx + 1]);
		break;
	case LCD_CHIP_T5W:
	case LCD_CHIP_T3:
		ch_reg0 = P2P_CH_SWAP0_T7 + offset;
		ch_reg1 = P2P_CH_SWAP1_T7 + offset;
		lcd_vcbus_write(ch_reg0, ch_swap_reg_6lane_map0[ch_reg_idx]);
		lcd_vcbus_write(ch_reg1, ch_swap_reg_6lane_map0[ch_reg_idx + 1]);
		lcd_vcbus_write(P2P_BIT_REV_T7 + offset, 2);
		break;
	case LCD_CHIP_T5:
	case LCD_CHIP_T5D:
		lcd_vcbus_write(P2P_CH_SWAP0, ch_swap_reg_6lane_map1[ch_reg_idx]);
		lcd_vcbus_write(P2P_CH_SWAP1, ch_swap_reg_6lane_map1[ch_reg_idx + 1]);
		break;
	case LCD_CHIP_T7:
		ch_reg0 = P2P_CH_SWAP0_T7 + offset;
		ch_reg1 = P2P_CH_SWAP1_T7 + offset;
		//don't support port_swap and lane_reverse when single port
		if (pdrv->index == 2 && pdrv->config.control.lvds_cfg.dual_port) {
			lcd_vcbus_write(ch_reg0, ch_swap_reg_6lane_map1[ch_reg_idx]);
			lcd_vcbus_write(ch_reg1, ch_swap_reg_6lane_map1[ch_reg_idx + 1]);
		} else if (pdrv->index == 1) {
			lcd_vcbus_write(ch_reg0, 0xf43210ff);
			lcd_vcbus_write(ch_reg1, 0xffff);
		} else { // (drv==2, single port) || drv==0
			lcd_vcbus_write(ch_reg0, 0xfff43210);
			lcd_vcbus_write(ch_reg1, 0xffff);
		}
		lcd_vcbus_write(P2P_BIT_REV_T7 + offset, 2);
		break;
	default:
		break;
	}
}

void lcd_lvds_enable(struct aml_lcd_drv_s *pdrv)
{
	unsigned int bit_num, pn_swap;
	unsigned int dual_port, fifo_mode, lvds_repack, sync_pol_reverse;
	unsigned int reg_lvds_pack_ctrl, reg_lvds_gen_ctrl;
	unsigned int offset;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	offset = pdrv->data->offset_venc_if[pdrv->index];
	lvds_repack = pdrv->config.control.lvds_cfg.lvds_repack & 0x3;
	pn_swap   = pdrv->config.control.lvds_cfg.pn_swap;
	dual_port = pdrv->config.control.lvds_cfg.dual_port;
	fifo_mode = dual_port ? 0x3 : 0x1;

	// H V:  L_POL_CNTL_ADDR LVDS_PACK_CNTL_ADDR
	// 0 0:  h: 1  v: 1      1
	// 0 1:  h: 1  v: 0      1
	// 1 0:  h: 1  v: 0      0
	// 1 1:  h: 1  v: 1      0
	sync_pol_reverse = !pdrv->config.timing.act_timing.hsync_pol; // reserve both h & v

	if (pdrv->config.basic.lcd_bits == 10) {
		bit_num = 0;
	} else if (pdrv->config.basic.lcd_bits == 6) {
		bit_num = 2;
	} else { // 8bit
		bit_num = 1;
	}

	if (pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3 ||
	    pdrv->data->chip_type == LCD_CHIP_T5W) {
		reg_lvds_pack_ctrl = LVDS_PACK_CNTL_ADDR_T7 + offset;
		reg_lvds_gen_ctrl = LVDS_GEN_CNTL_T7 + offset;
		lcd_vcbus_write(LVDS_SER_EN + offset, 0xfff);
	} else {
		reg_lvds_pack_ctrl = LVDS_PACK_CNTL_ADDR;
		reg_lvds_gen_ctrl = LVDS_GEN_CNTL;
	}

	lcd_vcbus_write(reg_lvds_pack_ctrl,
			(lvds_repack << 0) | // repack //[1:0]
			(sync_pol_reverse << 3) | // reserve
			(0 << 4) |		// lsb first
			(pn_swap << 5) |	// pn swap
			(dual_port << 6) |	// dual port
			(0 << 7) |		// use tcon control
			(bit_num << 8) |	// 0:10bits, 1:8bits, 2:6bits, 3:4bits.
			(0 << 10) |		//r_select  //0:R, 1:G, 2:B, 3:0
			(1 << 12) |		//g_select  //0:R, 1:G, 2:B, 3:0
			(2 << 14));		//b_select  //0:R, 1:G, 2:B, 3:0;

	lcd_lvds_lane_swap_set(pdrv);

	lcd_vcbus_setb(reg_lvds_gen_ctrl, 1, 4, 1);
	lcd_vcbus_setb(reg_lvds_gen_ctrl, fifo_mode, 0, 2);// fifo wr mode
	lcd_vcbus_setb(reg_lvds_gen_ctrl, 1, 3, 1);// fifo enable
}

void lcd_lvds_disable(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];

	/* disable lvds fifo */
	if (pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3 ||
	    pdrv->data->chip_type == LCD_CHIP_T5W) {
		lcd_vcbus_setb(LVDS_GEN_CNTL_T7 + offset, 0, 3, 1);
		lcd_vcbus_setb(LVDS_GEN_CNTL_T7 + offset, 0, 0, 2);
	} else {
		lcd_vcbus_setb(LVDS_GEN_CNTL + offset, 0, 3, 1);
		lcd_vcbus_setb(LVDS_GEN_CNTL + offset, 0, 0, 2);
	}
}
