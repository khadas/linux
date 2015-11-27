
/*
 * drivers/amlogic/display/lcd/lcd_tv/vbyone_drv.c
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
#ifdef CONFIG_AML_VPU
#include <linux/amlogic/vpu.h>
#endif
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/lcd_vout.h>
#include <linux/amlogic/vout/lcd_notify.h>
#include "lcd_tv.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

/* interrupt source */
#define INT_VIU_VSYNC    35
#define INT_VENC_VX1     110

static int vx1_fsm_acq_st;

/* set VX1_LOCKN && VX1_HTPDN */
static void lcd_vbyone_pinmux_set(int status)
{
	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

#if 0
	struct pinctrl_state *s;
	unsigned int pinmux_num;
	int ret;

	/* get pinmux control */
	if (IS_ERR(pconf->pin)) {
		LCDERR("%s\n", __func__);
		return;
	}

	/* select pinmux */
	s = pinctrl_lookup_state(pconf->pin, "vbyone");
	if (IS_ERR(s)) {
		LCDERR("%s\n", __func__);
		/* pinctrl_put(pin); //release pins */
		devm_pinctrl_put(pconf->pin);
		return;
	}

	/* set pinmux and lock pins */
	ret = pinctrl_select_state(pconf->pin, s);
	if (ret < 0) {
		LCDERR("%s\n", __func__);
		devm_pinctrl_put(pconf->pin);
		return;
	}
#else
	lcd_pinmux_clr_mask(7, ((1 << 1) | (1 << 2) | (1 << 9) | (1 << 10)));
	lcd_pinmux_set_mask(7, ((1 << 11) | (1 << 12)));
#endif
}

static void lcd_vbyone_phy_set(int status)
{
	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL1, 0x6e0ec918);
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL2, 0x00000a7c);
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL3, 0x00ff0800);
	} else {
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL1, 0x0);
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL2, 0x0);
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL3, 0x0);
	}
}

static void lcd_lvds_phy_set(int status)
{
	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL1, 0x6c6cca80);
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL2, 0x0000006c);
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL3, 0x0fff0800);
	} else {
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL1, 0x0);
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL2, 0x0);
		lcd_hiu_write(HHI_DIF_CSI_PHY_CNTL3, 0x0);
	}
}

static void lcd_tcon_set(struct lcd_config_s *pconf)
{
	vpp_set_matrix_ycbcr2rgb(2, 0);

	lcd_vcbus_write(L_RGB_BASE_ADDR, 0);
	lcd_vcbus_write(L_RGB_COEFF_ADDR, 0x400);

	if (pconf->lcd_basic.lcd_bits == 8)
		lcd_vcbus_write(L_DITH_CNTL_ADDR,  0x400);

	if (pconf->lcd_basic.lcd_type == LCD_LVDS)
		lcd_vcbus_setb(L_POL_CNTL_ADDR, 1, 0, 1);

	lcd_vcbus_write(VPP_MISC,
		lcd_vcbus_read(VPP_MISC) & ~(VPP_OUT_SATURATE));
}

static void lcd_lvds_clk_util_set(struct lcd_config_s *pconf)
{
	unsigned int fifo_mode, phy_div, dual_port;

	dual_port = pconf->lcd_control.lvds_config->dual_port;
	if (dual_port) {
		fifo_mode = 0x3;
		phy_div = 2;
	} else {
		fifo_mode = 0x1;
		phy_div = 1;
	}

	lcd_vcbus_write(LVDS_GEN_CNTL,
		(lcd_vcbus_read(LVDS_GEN_CNTL) | (1 << 4) | (fifo_mode << 0)));
	lcd_vcbus_setb(LVDS_GEN_CNTL, 1, 3, 1);

	/* set fifo_clk_sel: div 7 */
	lcd_hiu_write(HHI_LVDS_TX_PHY_CNTL0, (1 << 6));
	/* set cntl_ser_en:  8-channel to 1 */
	lcd_hiu_setb(HHI_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);

	/* decoupling fifo enable, gated clock enable */
	lcd_hiu_write(HHI_LVDS_TX_PHY_CNTL1,
		(1 << 30) | ((phy_div - 1) << 25) | (1 << 24));
	/* decoupling fifo write enable after fifo enable */
	lcd_hiu_setb(HHI_LVDS_TX_PHY_CNTL1, 1, 31, 1);
}

static void lcd_lvds_control_set(struct lcd_config_s *pconf)
{
	unsigned int bit_num = 1;
	unsigned int pn_swap = 0;
	unsigned int dual_port = 1;
	unsigned int lvds_repack = 1;
	unsigned int port_swap = 0;

	if (lcd_debug_print_flag)
		LCDPR("%s\n", __func__);

	lcd_lvds_clk_util_set(pconf);

	lvds_repack = (pconf->lcd_control.lvds_config->lvds_repack) & 0x1;
	pn_swap   = (pconf->lcd_control.lvds_config->pn_swap) & 0x1;
	dual_port = (pconf->lcd_control.lvds_config->dual_port) & 0x1;
	port_swap = (pconf->lcd_control.lvds_config->port_swap) & 0x1;

	switch (pconf->lcd_basic.lcd_bits) {
	case 10:
		bit_num = 0;
		break;
	case 8:
		bit_num = 1;
		break;
	case 6:
		bit_num = 2;
		break;
	case 4:
		bit_num = 3;
		break;
	default:
		bit_num = 1;
		break;
	}

	lcd_vcbus_write(LVDS_PACK_CNTL_ADDR,
		(lvds_repack << 0) | /* repack */
		(port_swap << 2) | /* odd_even */
		(0 << 3) |	/* reserve */
		(0 << 4) |	/* lsb first */
		(pn_swap << 5) | /* pn swap */
		(dual_port << 6) | /* dual port */
		(0 << 7) |	/* use tcon control */
		(bit_num << 8) | /* 0:10bits, 1:8bits, 2:6bits, 3:4bits */
		(0 << 10) |	/* r_select  //0:R, 1:G, 2:B, 3:0 */
		(1 << 12) |	/* g_select  //0:R, 1:G, 2:B, 3:0 */
		(2 << 14));	/* b_select  //0:R, 1:G, 2:B, 3:0 */
}

#if 0
static void lcd_vbyone_ctlbits(int p3d_en, int p3d_lr, int mode)
{
	if (mode == 0) { /* insert at the first pixel */
		lcd_vcbus_setb(VBO_PXL_CTRL,
			(1 << p3d_en) | (p3d_lr & 0x1), 0, 4);
	} else {
		lcd_vcbus_setb(VBO_VBK_CTRL_0,
			(1 << p3d_en) | (p3d_lr & 0x1), 0, 2);
	}
}
#endif

static void lcd_vbyone_sync_pol(int hsync_pol, int vsync_pol)
{
	lcd_vcbus_setb(VBO_VIN_CTRL, hsync_pol, 4, 1);
	lcd_vcbus_setb(VBO_VIN_CTRL, vsync_pol, 5, 1);

	lcd_vcbus_setb(VBO_VIN_CTRL, hsync_pol, 6, 1);
	lcd_vcbus_setb(VBO_VIN_CTRL, vsync_pol, 7, 1);
}

static void lcd_vbyone_clk_util_set(struct lcd_config_s *pconf)
{
	unsigned int lcd_bits;
	unsigned int div_sel, phy_div;

	phy_div = pconf->lcd_control.vbyone_config->phy_div;

	lcd_bits = 10;
	switch (lcd_bits) {
	case 6:
		div_sel = 0;
		break;
	case 8:
		div_sel = 2;
		break;
	case 10:
		div_sel = 3;
		break;
	default:
		div_sel = 3;
		break;
	}
	/* set fifo_clk_sel */
	lcd_hiu_write(HHI_LVDS_TX_PHY_CNTL0, (div_sel << 6));
	/* set cntl_ser_en:  8-channel to 1 */
	lcd_hiu_setb(HHI_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);

	/* decoupling fifo enable, gated clock enable */
	lcd_hiu_write(HHI_LVDS_TX_PHY_CNTL1,
		(1 << 30) | ((phy_div - 1) << 25) | (1 << 24));
	/* decoupling fifo write enable after fifo enable */
	lcd_hiu_setb(HHI_LVDS_TX_PHY_CNTL1, 1, 31, 1);
}

static int lcd_vbyone_lanes_set(int lane_num, int byte_mode, int region_num,
		int hsize, int vsize)
{
	int sublane_num;
	int region_size[4];
	int tmp;

	switch (lane_num) {
	case 1:
	case 2:
	case 4:
	case 8:
		break;
	default:
		return -1;
	}
	switch (region_num) {
	case 1:
	case 2:
	case 4:
		break;
	default:
		return -1;
	}
	if (lane_num % region_num)
		return -1;
	switch (byte_mode) {
	case 3:
	case 4:
		break;
	default:
		return -1;
	}
	LCDPR("byte_mode=%d, lane_num=%d, region_num=%d\n",
		byte_mode, lane_num, region_num);

	sublane_num = lane_num / region_num; /* lane num in each region */
	lcd_vcbus_setb(VBO_LANES, (lane_num - 1), 0, 3);
	lcd_vcbus_setb(VBO_LANES, (region_num - 1), 4, 2);
	lcd_vcbus_setb(VBO_LANES, (sublane_num - 1), 8, 3);
	lcd_vcbus_setb(VBO_LANES, (byte_mode - 1), 11, 2);

	if (region_num > 1) {
		region_size[3] = (hsize / lane_num) * sublane_num;
		tmp = (hsize % lane_num);
		region_size[0] = region_size[3] + (((tmp / sublane_num) > 0) ?
			sublane_num : (tmp % sublane_num));
		region_size[1] = region_size[3] + (((tmp / sublane_num) > 1) ?
			sublane_num : (tmp % sublane_num));
		region_size[2] = region_size[3] + (((tmp / sublane_num) > 2) ?
			sublane_num : (tmp % sublane_num));
		lcd_vcbus_write(VBO_REGION_00, region_size[0]);
		lcd_vcbus_write(VBO_REGION_01, region_size[1]);
		lcd_vcbus_write(VBO_REGION_02, region_size[2]);
		lcd_vcbus_write(VBO_REGION_03, region_size[3]);
	}
	lcd_vcbus_write(VBO_ACT_VSIZE, vsize);
	/* different from FBC code!!! */
	/* lcd_vcbus_setb(VBO_CTRL_H,0x80,11,5); */
	/* different from simulation code!!! */
	lcd_vcbus_setb(VBO_CTRL_H, 0x0, 0, 4);
	lcd_vcbus_setb(VBO_CTRL_H, 0x1, 9, 1);
	/* lcd_vcbus_setb(VBO_CTRL_L,enable,0,1); */

	return 0;
}

static void lcd_vbyone_control_set(struct lcd_config_s *pconf)
{
	int lane_count, byte_mode, region_num, hsize, vsize, color_fmt;
	int vin_color, vin_bpp;

	if (lcd_debug_print_flag)
		LCDPR("%s\n", __func__);

	hsize = pconf->lcd_basic.h_active;
	vsize = pconf->lcd_basic.v_active;
	lane_count = pconf->lcd_control.vbyone_config->lane_count; /* 8 */
	region_num = pconf->lcd_control.vbyone_config->region_num; /* 2 */
	byte_mode = pconf->lcd_control.vbyone_config->byte_mode; /* 4 */
	color_fmt = pconf->lcd_control.vbyone_config->color_fmt; /* 4 */

	lcd_vbyone_clk_util_set(pconf);
#if 0
	switch (color_fmt) {
	case 0:/* SDVT_VBYONE_18BPP_RGB */
		vin_color = 4;
		vin_bpp   = 2;
		break;
	case 1:/* SDVT_VBYONE_18BPP_YCBCR444 */
		vin_color = 0;
		vin_bpp   = 2;
		break;
	case 2:/* SDVT_VBYONE_24BPP_RGB */
		vin_color = 4;
		vin_bpp   = 1;
		break;
	case 3:/* SDVT_VBYONE_24BPP_YCBCR444 */
		vin_color = 0;
		vin_bpp   = 1;
		break;
	case 4:/* SDVT_VBYONE_30BPP_RGB */
		vin_color = 4;
		vin_bpp   = 0;
		break;
	case 5:/* SDVT_VBYONE_30BPP_YCBCR444 */
		vin_color = 0;
		vin_bpp   = 0;
		break;
	default:
		LCDPR("error: vbyone COLOR_FORMAT unsupport\n");
		return;
	}
#else
	vin_color = 4; /* fixed RGB */
	vin_bpp   = 0; /* fixed 30bbp 4:4:4 */
#endif

	/* set Vbyone vin color format */
	lcd_vcbus_setb(VBO_VIN_CTRL, vin_color, 8, 3);
	lcd_vcbus_setb(VBO_VIN_CTRL, vin_bpp, 11, 2);

	lcd_vbyone_lanes_set(lane_count, byte_mode, region_num, hsize, vsize);
	/*set hsync/vsync polarity to let the polarity is low active
	inside the VbyOne */
	lcd_vbyone_sync_pol(0, 0);

	/* below line copy from simulation */
	/* gate the input when vsync asserted */
	lcd_vcbus_setb(VBO_VIN_CTRL, 1, 0, 2);
	/* lcd_vcbus_write(VBO_VBK_CTRL_0,0x13);
	//lcd_vcbus_write(VBO_VBK_CTRL_1,0x56);
	//lcd_vcbus_write(VBO_HBK_CTRL,0x3478);
	//lcd_vcbus_setb(VBO_PXL_CTRL,0x2,0,4);
	//lcd_vcbus_setb(VBO_PXL_CTRL,0x3,VBO_PXL_CTR1_BIT,VBO_PXL_CTR1_WID);
	//set_vbyone_ctlbits(1,0,0); */

	/* PAD select: */
	if ((lane_count == 1) || (lane_count == 2))
		lcd_vcbus_setb(LCD_PORT_SWAP, 1, 9, 2);
	else if (lane_count == 4)
		lcd_vcbus_setb(LCD_PORT_SWAP, 2, 9, 2);
	else
		lcd_vcbus_setb(LCD_PORT_SWAP, 0, 9, 2);
	/* lcd_vcbus_setb(LCD_PORT_SWAP, 1, 8, 1);//reverse lane output order */

	/* Mux pads in combo-phy: 0 for dsi; 1 for lvds or vbyone; 2 for edp */
	lcd_hiu_write(HHI_DSI_LVDS_EDP_CNTL0, 0x1);
	lcd_vcbus_setb(VBO_CTRL_L, 1, 0, 1);

	/*force vencl clk enable, otherwise, it might auto turn off by mipi DSI
	//lcd_vcbus_setb(VPU_MISC_CTRL, 1, 0, 1); */
}

#define VBYONE_INTR_UNMASK    0x2a00 /* enable htpdn_fail,lockn_fail,acq_hold */
static void lcd_vbyone_interrupt_enable(int flag)
{
	if (flag) {
		/* enable interrupt */
		lcd_vcbus_setb(VBO_INTR_UNMASK, VBYONE_INTR_UNMASK, 0, 15);
	} else {
		/* mask interrupt */
		lcd_vcbus_write(VBO_INTR_UNMASK, 0x0);
		/* release hold in FSM_ACQ */
		lcd_vcbus_setb(VBO_FSM_HOLDER_L, 0, 0, 16);
	}
}

static void lcd_vbyone_interrupt_init(void)
{
	/* set hold in FSM_ACQ */
	lcd_vcbus_setb(VBO_FSM_HOLDER_L, 0xffff, 0, 16);
	/* set hold in FSM_CDR */
	lcd_vcbus_setb(VBO_FSM_HOLDER_H, 0, 0, 16);
	/* not wait lockn to 1 in FSM_ACQ */
	lcd_vcbus_setb(VBO_CTRL_L, 1, 10, 1);
	/* lcd_vcbus_setb(VBO_CTRL_L, 0, 9, 1);   //use sw pll_lock */
	/* reg_pll_lock = 1 to realease force to FSM_ACQ
	//lcd_vcbus_setb(VBO_CTRL_H, 1, 13, 1); */

	/* vx1 interrupt setting */
	lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 1, 12, 1);    /* intr pulse width */
	lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0x01ff, 0, 9); /* clear interrupt */
	lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0, 0, 9);

	lcd_vbyone_interrupt_enable(1);
	vx1_fsm_acq_st = 0;

	if (lcd_debug_print_flag)
		LCDPR("%s\n", __func__);
}

static void lcd_vbyone_wait_stable(void)
{
	int i = 1000;

	while ((lcd_vcbus_read(VBO_STATUS_L) & 0x3f) != 0x20) {
		udelay(5);
		if (i-- == 0)
			break;
	}
	LCDPR("%s status: 0x%x\n", __func__, lcd_vcbus_read(VBO_STATUS_L));
	lcd_vbyone_interrupt_init();
}

static irqreturn_t lcd_vbyone_vsync_isr(int irq, void *dev_id)
{
	lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 1, 0, 1);
	lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0, 0, 1);

	return IRQ_HANDLED;
}

static irqreturn_t lcd_vbyone_interrupt_handler(int irq, void *dev_id)
{
	unsigned int data32, data32_1;

	lcd_vcbus_write(VBO_INTR_UNMASK, 0x0);  /* mask interrupt */

	data32 = (lcd_vcbus_read(VBO_INTR_STATE) & 0x7fff);
	/* clear the interrupt */
	data32_1 = ((data32 >> 9) << 3);
	if (data32 & 0x1c0)
		data32_1 |= (1 << 2);
	if (data32 & 0x38)
		data32_1 |= (1 << 1);
	if (data32 & 0x7)
		data32_1 |= (1 << 0);
	lcd_vcbus_setb(VBO_INTR_STATE_CTRL, data32_1, 0, 9);
	lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0, 0, 9);
	LCDPR("vx1 interrupt status = 0x%04x\n", data32);

	if (data32 & 0x200) {
		LCDPR("vx1 htpdn fall edge occurred\n");
		vx1_fsm_acq_st = 0;
		lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0, 15, 1);
	}
#if 0
	if (data32 & 0x400) {
		LCDPR("vx1 htpdn raise edge occurred\n");
		vx1_fsm_acq_st = 0;
		lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0, 15, 1);
	}
#endif
	if (data32 & 0x800) {
		LCDPR("vx1 lockn fall edge occurred\n");
		vx1_fsm_acq_st = 0;
		lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0, 15, 1);
	}
#if 0
	if (data32 & 0x1000) {
		LCDPR("vx1 lockn raise edge occurred\n");
		vx1_fsm_acq_st = 0;
		lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0, 15, 1);
	}
#endif
	if (data32 & 0x2000) {
		LCDPR("vx1 fsm_acqu wait end\n");
		LCDPR("vx1 status 0: 0x%x\n", lcd_vcbus_read(VBO_STATUS_L));
		if (vx1_fsm_acq_st == 0) {
			/* clear FSM_continue */
			lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0, 15, 1);
			LCDPR("vx1 sw reset\n");
			/* force PHY to 0 */
			lcd_hiu_setb(HHI_LVDS_TX_PHY_CNTL0, 3, 8, 2);
			lcd_vcbus_write(VBO_SOFT_RST, 0x1ff);
			udelay(5);
			/* clear lockn raising flag */
			lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 1, 7, 1);
			/* realease PHY */
			lcd_hiu_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 8, 2);
			/* clear lockn raising flag */
			lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0, 7, 1);
			lcd_vcbus_write(VBO_SOFT_RST, 0);
			vx1_fsm_acq_st = 1;
		} else {
			vx1_fsm_acq_st = 2;
			/* set FSM_continue */
			lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 1, 15, 1);
		}
		LCDPR("vx1 status 1: 0x%x\n", lcd_vcbus_read(VBO_STATUS_L));
	}
	udelay(20);

	/* enable interrupt */
	lcd_vcbus_setb(VBO_INTR_UNMASK, VBYONE_INTR_UNMASK, 0, 15);

	if (lcd_debug_print_flag) {
		LCDPR("vx1 vx1_fsm_acq_st: %d\n", vx1_fsm_acq_st);
		LCDPR("vx1 status: 0x%x\n\n", lcd_vcbus_read(VBO_STATUS_L));
	}

	return IRQ_HANDLED;
}

static void lcd_venc_set(struct lcd_config_s *pconf)
{
	unsigned int h_active, v_active;
	unsigned int video_on_pixel, video_on_line;

	if (lcd_debug_print_flag)
		LCDPR("%s\n", __func__);

	h_active = pconf->lcd_basic.h_active;
	v_active = pconf->lcd_basic.v_active;
	video_on_pixel = pconf->lcd_timing.video_on_pixel;
	video_on_line = pconf->lcd_timing.video_on_line;

	lcd_vcbus_write(ENCL_VIDEO_EN, 0);

	lcd_vcbus_write(VPU_VIU_VENC_MUX_CTRL, (0 << 0) | (3 << 2));
	/* Enable Hsync and equalization pulse switch in center;
	bit[14] cfg_de_v = 1 */
	lcd_vcbus_write(ENCL_VIDEO_MODE,       40);
	lcd_vcbus_write(ENCL_VIDEO_MODE_ADV,   0x18); /* Sampling rate: 1 */

	/* bypass filter */
	lcd_vcbus_write(ENCL_VIDEO_FILT_CTRL, 0x1000);
	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT, pconf->lcd_basic.h_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT, pconf->lcd_basic.v_period - 1);

	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN, video_on_pixel);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END,   h_active - 1 + video_on_pixel);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE, video_on_line);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE, v_active - 1  + video_on_line);

	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN,   pconf->lcd_timing.hs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END,     pconf->lcd_timing.hs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN,   pconf->lcd_timing.vs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END,     pconf->lcd_timing.vs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE,   pconf->lcd_timing.vs_vs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE,   pconf->lcd_timing.vs_ve_addr);

	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, 3);

	lcd_vcbus_write(ENCL_VIDEO_EN, 1);
}

static void lcd_venc_change(struct lcd_config_s *pconf)
{
	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT, pconf->lcd_basic.h_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT, pconf->lcd_basic.v_period - 1);
	LCDPR("venc changed: %d,%d\n",
		pconf->lcd_basic.h_period, pconf->lcd_basic.v_period);

	aml_lcd_notifier_call_chain(LCD_EVENT_BACKLIGHT_UPDATE, NULL);
}

static unsigned int vbyone_lane_num[] = {
	1,
	2,
	4,
	8,
	8,
};

#define VBYONE_BIT_RATE_MAX		2970 /* MHz */
#define VBYONE_BIT_RATE_MIN		600
static void lcd_vbyone_config_set(struct lcd_config_s *pconf)
{
	unsigned int band_width, bit_rate, pclk, phy_div;
	unsigned int byte_mode, lane_count, minlane;
	unsigned int lcd_bits;
	unsigned int temp, i;

	if (lcd_debug_print_flag)
		LCDPR("%s\n", __func__);

	/* auto calculate bandwidth, clock */
	lane_count = pconf->lcd_control.vbyone_config->lane_count;
	lcd_bits = 10; /* pconf->lcd_basic.lcd_bits */
	byte_mode = (lcd_bits == 10) ? 4 : 3;
	/* byte_mode * byte2bit * 8/10_encoding * pclk =
	   byte_mode * 8 * 10 / 8 * pclk */
	pclk = pconf->lcd_timing.lcd_clk / 1000; /* kHz */
	band_width = byte_mode * 10 * pclk;

	temp = VBYONE_BIT_RATE_MAX * 1000;
	temp = (band_width + temp - 1) / temp;
	for (i = 0; i < 4; i++) {
		if (temp <= vbyone_lane_num[i])
			break;
	}
	minlane = vbyone_lane_num[i];
	if (lane_count < minlane) {
		LCDPR("error: vbyone lane_num(%d) is less than min(%d)\n",
			lane_count, minlane);
		lane_count = minlane;
		pconf->lcd_control.vbyone_config->lane_count = lane_count;
		LCDPR("change to min lane_num %d\n", minlane);
	}

	bit_rate = band_width / minlane;
	phy_div = lane_count / minlane;
	if (phy_div == 8) {
		phy_div /= 2;
		bit_rate /= 2;
	}
	if (bit_rate > (VBYONE_BIT_RATE_MAX * 1000)) {
		LCDPR("error: vbyone bit rate(%dKHz) is out of max(%dKHz)\n",
			bit_rate, (VBYONE_BIT_RATE_MAX * 1000));
	}
	if (bit_rate < (VBYONE_BIT_RATE_MIN * 1000)) {
		LCDPR("error: vbyone bit rate(%dKHz) is out of min(%dKHz)\n",
			bit_rate, (VBYONE_BIT_RATE_MIN * 1000));
	}
	bit_rate = bit_rate * 1000; /* Hz */

	pconf->lcd_control.vbyone_config->phy_div = phy_div;
	pconf->lcd_control.vbyone_config->bit_rate = bit_rate;

	if (lcd_debug_print_flag) {
		LCDPR("lane_count=%u, bit_rate = %uMHz, pclk=%u.%03uMhz\n",
			lane_count, (bit_rate / 1000000),
			(pclk / 1000), (pclk % 1000));
	}
}

static void lcd_config_update(struct lcd_config_s *pconf)
{
	/* update clk & timing config */
	lcd_vmode_change(pconf);
	switch (pconf->lcd_basic.lcd_type) {
	case LCD_LVDS:
		break;
	case LCD_VBYONE:
		lcd_vbyone_config_set(pconf);
		break;
	default:
		LCDPR("invalid lcd type\n");
		break;
	}
	lcd_clk_generate_parameter(pconf);
}

int lcd_tv_driver_init(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_config_s *pconf;

	LCDPR("tv driver init\n");
	pconf = lcd_drv->lcd_config;

	lcd_config_update(pconf);
#ifdef CONFIG_AML_VPU
	request_vpu_clk_vmod(pconf->lcd_timing.lcd_clk, VPU_VENCL);
	switch_vpu_mem_pd_vmod(VPU_VENCL, VPU_MEM_POWER_ON);
#endif
	lcd_clk_gate_switch(1);

	/* init driver */
	switch (pconf->lcd_basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_interrupt_enable(0);
		break;
	default:
		break;
	}
	if (lcd_drv->lcd_status == 0) { /* init */
		lcd_clk_set(pconf);
		lcd_venc_set(pconf);
		lcd_tcon_set(pconf);
		switch (pconf->lcd_basic.lcd_type) {
		case LCD_LVDS:
			lcd_lvds_control_set(pconf);
			lcd_lvds_phy_set(1);
			break;
		case LCD_VBYONE:
			lcd_vbyone_control_set(pconf);
			lcd_vbyone_phy_set(1);
			lcd_vbyone_pinmux_set(1);
			break;
		default:
			LCDPR("invalid lcd type\n");
			break;
		}
	} else { /* change */
		switch (pconf->lcd_timing.fr_adjust_type) {
		case 0: /* clk adjust */
			lcd_clk_set(pconf);
			break;
		case 1: /* htotal adjust */
		case 2: /* vtotal adjust */
			lcd_venc_change(pconf);
			break;
		default:
			break;
		}
	}
	switch (pconf->lcd_basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_wait_stable();
		break;
	default:
		break;
	}

	lcd_vcbus_write(VENC_INTCTRL, 0x200);

	if (lcd_debug_print_flag)
		LCDPR("%s finished\n", __func__);
	return 0;
}

void lcd_tv_driver_disable(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	struct lcd_config_s *pconf;

	pconf = lcd_drv->lcd_config;
	switch (pconf->lcd_basic.lcd_type) {
	case LCD_LVDS:
		lcd_lvds_phy_set(0);
		lcd_vcbus_setb(LVDS_GEN_CNTL, 0, 3, 1); /* disable lvds fifo */
		break;
	case LCD_VBYONE:
		lcd_vbyone_interrupt_enable(0);
		lcd_vbyone_pinmux_set(0);
		lcd_vbyone_phy_set(0);
		break;
	default:
		break;
	}

	lcd_vcbus_write(ENCL_VIDEO_EN, 0); /* disable encl */

	lcd_clk_disable();
	lcd_clk_gate_switch(0);
#ifdef CONFIG_AML_VPU
	switch_vpu_mem_pd_vmod(VPU_VENCL, VPU_MEM_POWER_DOWN);
	release_vpu_clk_vmod(VPU_VENCL);
#endif

	LCDPR("disable driver\n");
}

#define VBYONE_IRQF   IRQF_SHARED /* IRQF_DISABLED */ /* IRQF_SHARED */

void lcd_vbyone_interrupt_up(void)
{
	if (request_irq(INT_VIU_VSYNC, &lcd_vbyone_vsync_isr,
		IRQF_SHARED, "vbyone_vsync", (void *)"vbyone_vsync")) {
		LCDERR("can't request vsync_irq for vbyone\n");
	} else {
		if (lcd_debug_print_flag)
			LCDPR("request vbyone vsync_irq successful\n");
	}

	if (request_irq(INT_VENC_VX1, &lcd_vbyone_interrupt_handler,
		VBYONE_IRQF, "vbyone", (void *)"vbyone")) {
		LCDERR("can't request irq for vbyone\n");
	} else {
		if (lcd_debug_print_flag)
			LCDPR("request vbyone irq successful\n");
	}
}

void lcd_vbyone_interrupt_down(void)
{
	free_irq(INT_VENC_VX1, (void *)"vbyone");
	free_irq(INT_VIU_VSYNC, (void *)"vbyone");
	if (lcd_debug_print_flag)
			LCDPR("free vbyone irq\n");
}
