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
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_reg.h"
#include "lcd_common.h"

static int vx1_fsm_acq_st;

#define VX1_TRAINING_TIMEOUT    60  /* vsync cnt */
static int vx1_training_wait_cnt;
static int vx1_training_stable_cnt;
static int vx1_timeout_reset_flag;
static int lcd_vx1_intr_request;
static int lcd_vx1_vsync_isr_en;
static int lcd_vx1_isr_flag;

#define VX1_LOCKN_WAIT_TIMEOUT    5000  /* *50us */
#define VX1_HPD_WAIT_TIMEOUT      10000  /* *50us */

#define VX1_HPLL_INTERVAL (HZ)
/* enable htpdn_fail,lockn_fail,acq_hold */
#define VBYONE_INTR_UNMASK   0x2bff /* 0x2a00 */

void lcd_vbyone_link_maintain_clear(void)
{
	vx1_training_wait_cnt = 0;
	vx1_timeout_reset_flag = 0;
}

static unsigned int lcd_vbyone_get_fsm_state(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_status, offset, state;

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_status = VBO_STATUS_L_T7 + offset;
	} else {
		reg_status = VBO_STATUS_L;
	}

	state = lcd_vcbus_read(reg_status) & 0x3f;
	return state;
}

static void lcd_vbyone_force_cdr(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_insgn_ctrl, offset;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	if (pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3 ||
	    pdrv->data->chip_type == LCD_CHIP_T5W) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_insgn_ctrl = VBO_INSGN_CTRL_T7 + offset;
	} else {
		reg_insgn_ctrl = VBO_INSGN_CTRL;
	}

	lcd_vcbus_setb(reg_insgn_ctrl, 7, 0, 4);
}

static void lcd_vbyone_force_lock(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_insgn_ctrl, offset;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	if (pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3 ||
	    pdrv->data->chip_type == LCD_CHIP_T5W) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_insgn_ctrl = VBO_INSGN_CTRL_T7 + offset;
	} else {
		reg_insgn_ctrl = VBO_INSGN_CTRL;
	}

	lcd_vcbus_setb(reg_insgn_ctrl, 7, 0, 4);
	msleep(100);
	lcd_vcbus_setb(reg_insgn_ctrl, 5, 0, 4);
}

static void lcd_vbyone_sw_reset(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_phy_tx_ctrl0, reg_rst, offset;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	if (pdrv->data->chip_type == LCD_CHIP_T7) {
		switch (pdrv->index) {
		case 0:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			break;
		case 1:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			break;
		default:
			LCDERR("[%d]: %s: invalid drv_index\n", pdrv->index, __func__);
			return;
		}
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_rst = VBO_SOFT_RST_T7 + offset;

		/* force PHY to 0 */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 3, 8, 2);
		lcd_vcbus_write(reg_rst, 0x1ff);
		udelay(5);
		/* realease PHY */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 2);
		lcd_vcbus_write(reg_rst, 0);
	} else if (pdrv->data->chip_type == LCD_CHIP_T3) {
		switch (pdrv->index) {
		case 0:
			reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL0;
			break;
		case 1:
			reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL2;
			break;
		default:
			LCDERR("[%d]: %s: invalid drv_index\n", pdrv->index, __func__);
			return;
		}
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_rst = VBO_SOFT_RST_T7 + offset;

		/* force PHY to 0 */
		lcd_ana_setb(reg_phy_tx_ctrl0, 3, 8, 2);
		lcd_vcbus_write(reg_rst, 0x1ff);
		udelay(5);
		/* realease PHY */
		lcd_ana_setb(reg_phy_tx_ctrl0, 0, 8, 2);
		lcd_vcbus_write(reg_rst, 0);
	} else if (pdrv->data->chip_type == LCD_CHIP_T5W) {
		switch (pdrv->index) {
		case 0:
			reg_phy_tx_ctrl0 = HHI_LVDS_TX_PHY_CNTL0;
			break;
		case 1:
			reg_phy_tx_ctrl0 = HHI_LVDS_TX_PHY_CNTL2;
			break;
		default:
			LCDERR("[%d]: %s: invalid drv_index\n", pdrv->index, __func__);
			return;
		}
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_rst = VBO_SOFT_RST_T7 + offset;

		/* force PHY to 0 */
		lcd_ana_setb(reg_phy_tx_ctrl0, 3, 8, 2);
		lcd_vcbus_write(reg_rst, 0x1ff);
		udelay(5);
		/* realease PHY */
		lcd_ana_setb(reg_phy_tx_ctrl0, 0, 8, 2);
		lcd_vcbus_write(reg_rst, 0);
	} else {
		/* force PHY to 0 */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 3, 8, 2);
		lcd_vcbus_write(VBO_SOFT_RST, 0x1ff);
		udelay(5);
		/* realease PHY */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 8, 2);
		lcd_vcbus_write(VBO_SOFT_RST, 0);
	}
}

static void lcd_vbyone_hw_filter(struct aml_lcd_drv_s *pdrv, int flag)
{
	struct vbyone_config_s *vx1_conf;
	unsigned int reg_filter_l, reg_filter_h, reg_ctrl;
	unsigned int temp, period, offset;
	unsigned int tick_period[] = {
		0xfff,
		0xff,    /* 1: 0.8us */
		0x1ff,   /* 2: 1.7us */
		0x3ff,   /* 3: 3.4us */
		0x7ff,   /* 4: 6.9us */
		0xfff,   /* 5: 13.8us */
		0x1fff,  /* 6: 27us */
		0x3fff,  /* 7: 55us */
		0x7fff,  /* 8: 110us */
		0xffff,  /* 9: 221us */
		0x1ffff, /* 10: 441us */
		0x3ffff, /* 11: 883us */
		0x7ffff, /* 12: 1.76ms */
		0xfffff, /* 13: 3.53ms */
	};

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_filter_l = VBO_INFILTER_CTRL_T7 + offset;
		reg_filter_h = VBO_INFILTER_CTRL_H_T7 + offset;
		reg_ctrl = VBO_INSGN_CTRL_T7 + offset;
	} else {
		reg_filter_l = VBO_INFILTER_TICK_PERIOD_L;
		reg_filter_h = VBO_INFILTER_TICK_PERIOD_H;
		reg_ctrl = VBO_INSGN_CTRL;
	}

	vx1_conf = &pdrv->config.control.vbyone_cfg;
	if (flag) {
		period = vx1_conf->hw_filter_time & 0xff;
		if (period >=
			(sizeof(tick_period) / sizeof(unsigned int)))
			period = tick_period[0];
		else
			period = tick_period[period];
		temp = period & 0xffff;
		lcd_vcbus_write(reg_filter_l, temp);
		temp = (period >> 16) & 0xf;
		lcd_vcbus_write(reg_filter_h, temp);
		/* hpd */
		temp = vx1_conf->hw_filter_cnt & 0xff;
		if (temp == 0xff) {
			lcd_vcbus_setb(reg_ctrl, 0, 8, 4);
		} else {
			temp = (temp == 0) ? 0x7 : temp;
			lcd_vcbus_setb(reg_ctrl, temp, 8, 4);
		}
		/* lockn */
		temp = (vx1_conf->hw_filter_cnt >> 8) & 0xff;
		if (temp == 0xff) {
			lcd_vcbus_setb(reg_ctrl, 0, 12, 4);
		} else {
			temp = (temp == 0) ? 0x7 : temp;
			lcd_vcbus_setb(reg_ctrl, temp, 12, 4);
		}
	} else {
		temp = (vx1_conf->hw_filter_time >> 8) & 0x1;
		if (temp) {
			lcd_vcbus_write(reg_filter_l, 0xff);
			lcd_vcbus_write(reg_filter_h, 0x0);
			lcd_vcbus_setb(reg_ctrl, 0, 8, 4);
			lcd_vcbus_setb(reg_ctrl, 0, 12, 4);
			LCDPR("[%d]: %s: %d disable for debug\n",
			      pdrv->index, __func__, flag);
		}
	}
}

static void lcd_vbyone_sync_pol(struct aml_lcd_drv_s *pdrv,
				int hsync_pol, int vsync_pol)
{
	unsigned int offset;

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, hsync_pol, 4, 1);
		lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, vsync_pol, 5, 1);

		lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, hsync_pol, 6, 1);
		lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, vsync_pol, 7, 1);
	} else {
		lcd_vcbus_setb(VBO_VIN_CTRL, hsync_pol, 4, 1);
		lcd_vcbus_setb(VBO_VIN_CTRL, vsync_pol, 5, 1);

		lcd_vcbus_setb(VBO_VIN_CTRL, hsync_pol, 6, 1);
		lcd_vcbus_setb(VBO_VIN_CTRL, vsync_pol, 7, 1);
	}
}

static int lcd_vbyone_lanes_set(struct aml_lcd_drv_s *pdrv, int lane_num,
				int byte_mode, int region_num,
				int hsize, int vsize)
{
	unsigned int offset;
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
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("byte_mode=%d, lane_num=%d, region_num=%d\n",
		      byte_mode, lane_num, region_num);
	}

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		sublane_num = lane_num / region_num; /* lane num in each region */
		lcd_vcbus_setb(VBO_LANES_T7 + offset, (lane_num - 1), 0, 3);
		lcd_vcbus_setb(VBO_LANES_T7 + offset, (region_num - 1), 4, 2);
		lcd_vcbus_setb(VBO_LANES_T7 + offset, (sublane_num - 1), 8, 3);
		lcd_vcbus_setb(VBO_LANES_T7 + offset, (byte_mode - 1), 11, 2);

		if (region_num > 1) {
			region_size[3] = (hsize / lane_num) * sublane_num;
			tmp = (hsize % lane_num);
			region_size[0] = region_size[3] + (((tmp / sublane_num) > 0) ?
				sublane_num : (tmp % sublane_num));
			region_size[1] = region_size[3] + (((tmp / sublane_num) > 1) ?
				sublane_num : (tmp % sublane_num));
			region_size[2] = region_size[3] + (((tmp / sublane_num) > 2) ?
				sublane_num : (tmp % sublane_num));
			lcd_vcbus_write(VBO_REGION_00_T7 + offset, region_size[0]);
			lcd_vcbus_write(VBO_REGION_01_T7 + offset, region_size[1]);
			lcd_vcbus_write(VBO_REGION_02_T7 + offset, region_size[2]);
			lcd_vcbus_write(VBO_REGION_03_T7 + offset, region_size[3]);
		}
		lcd_vcbus_write(VBO_ACT_VSIZE_T7 + offset, vsize);
		/* different from FBC code!!! */
		/* lcd_vcbus_setb(VBO_CTRL_H_T7 + offset,0x80,11,5); */
		/* different from simulation code!!! */
		lcd_vcbus_setb(VBO_CTRL_H_T7 + offset, 0x0, 0, 4);
		lcd_vcbus_setb(VBO_CTRL_H_T7 + offset, 0x1, 9, 1);
		/* lcd_vcbus_setb(VBO_CTRL_L_T7 + offset,enable,0,1); */
	} else {
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
	}

	return 0;
}

void lcd_vbyone_enable_dft(struct aml_lcd_drv_s *pdrv)
{
	int lane_count, byte_mode, region_num, hsize, vsize;
	/* int color_fmt; */
	int vin_color, vin_bpp;

	hsize = pdrv->config.basic.h_active;
	vsize = pdrv->config.basic.v_active;
	lane_count = pdrv->config.control.vbyone_cfg.lane_count; /* 8 */
	region_num = pdrv->config.control.vbyone_cfg.region_num; /* 2 */
	byte_mode = pdrv->config.control.vbyone_cfg.byte_mode; /* 4 */
	/* color_fmt = pdrv->config.control.vbyone_cfg.color_fmt; // 4 */

	vin_color = 4; /* fixed RGB */
	switch (pdrv->config.basic.lcd_bits) {
	case 6:
		vin_bpp = 2; /* 18bbp 4:4:4 */
		break;
	case 8:
		vin_bpp = 1; /* 24bbp 4:4:4 */
		break;
	case 10:
	default:
		vin_bpp = 0; /* 30bbp 4:4:4 */
		break;
	}

	/* set Vbyone vin color format */
	lcd_vcbus_setb(VBO_VIN_CTRL, vin_color, 8, 3);
	lcd_vcbus_setb(VBO_VIN_CTRL, vin_bpp, 11, 2);

	lcd_vbyone_lanes_set(pdrv, lane_count, byte_mode, region_num, hsize, vsize);
	/*set hsync/vsync polarity to let the polarity is low active*/
	/*inside the VbyOne */
	lcd_vbyone_sync_pol(pdrv, 0, 0);

	/* below line copy from simulation */
	/* gate the input when vsync asserted */
	lcd_vcbus_setb(VBO_VIN_CTRL, 1, 0, 2);
	/* lcd_vcbus_write(VBO_VBK_CTRL_0,0x13);
	 * lcd_vcbus_write(VBO_VBK_CTRL_1,0x56);
	 * lcd_vcbus_write(VBO_HBK_CTRL,0x3478);
	 * lcd_vcbus_setb(VBO_PXL_CTRL,0x2,0,4);
	 * lcd_vcbus_setb(VBO_PXL_CTRL,0x3,VBO_PXL_CTR1_BIT,VBO_PXL_CTR1_WID);
	 * set_vbyone_ctlbits(1,0,0);
	 */
	/* VBO_RGN_GEN clk always on */
	lcd_vcbus_setb(VBO_GCLK_MAIN, 2, 2, 2);

	lcd_vcbus_setb(LCD_PORT_SWAP, 0, 9, 2);
	/* lcd_vcbus_setb(LCD_PORT_SWAP, 1, 8, 1);//reverse lane output order */

	lcd_vbyone_hw_filter(pdrv, 1);
	lcd_vcbus_setb(VBO_INSGN_CTRL, 0, 2, 2);

	lcd_vcbus_setb(VBO_CTRL_L, 1, 0, 1);

	lcd_vbyone_wait_timing_stable(pdrv);
	lcd_vbyone_sw_reset(pdrv);

	/* training hold */
	if (pdrv->config.control.vbyone_cfg.ctrl_flag & 0x4)
		lcd_vbyone_cdr_training_hold(pdrv, 1);
}

void lcd_vbyone_disable_dft(struct aml_lcd_drv_s *pdrv)
{
	lcd_vcbus_setb(VBO_CTRL_L, 0, 0, 1);
	/* clear insig setting */
	lcd_vcbus_setb(VBO_INSGN_CTRL, 0, 2, 1);
	lcd_vcbus_setb(VBO_INSGN_CTRL, 0, 0, 1);
}

void lcd_vbyone_enable_t7(struct aml_lcd_drv_s *pdrv)
{
	int lane_count, byte_mode, region_num, hsize, vsize;
	/* int color_fmt; */
	int vin_color, vin_bpp;
	unsigned int offset;

	offset = pdrv->data->offset_venc_if[pdrv->index];

	hsize = pdrv->config.basic.h_active;
	vsize = pdrv->config.basic.v_active;
	lane_count = pdrv->config.control.vbyone_cfg.lane_count; /* 8 */
	region_num = pdrv->config.control.vbyone_cfg.region_num; /* 2 */
	byte_mode = pdrv->config.control.vbyone_cfg.byte_mode; /* 4 */
	/* color_fmt = pdrv->config.control.vbyone_cfg.color_fmt; // 4 */

	vin_color = 4; /* fixed RGB */
	switch (pdrv->config.basic.lcd_bits) {
	case 6:
		vin_bpp = 2; /* 18bbp 4:4:4 */
		break;
	case 8:
		vin_bpp = 1; /* 24bbp 4:4:4 */
		break;
	case 10:
	default:
		vin_bpp = 0; /* 30bbp 4:4:4 */
		break;
	}

	/* set Vbyone vin color format */
	lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, vin_color, 8, 3);
	lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, vin_bpp, 11, 2);

	lcd_vbyone_lanes_set(pdrv, lane_count, byte_mode, region_num, hsize, vsize);
	/*set hsync/vsync polarity to let the polarity is low active*/
	/*inside the VbyOne */
	lcd_vbyone_sync_pol(pdrv, 0, 0);

	/* below line copy from simulation */
	/* gate the input when vsync asserted */
	lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, 1, 0, 2);
	/* lcd_vcbus_write(VBO_VBK_CTRL_0_T7 + offset,0x13);
	 * lcd_vcbus_write(VBO_VBK_CTRL_1_T7 + offset,0x56);
	 * lcd_vcbus_write(VBO_HBK_CTRL_T7 + offset,0x3478);
	 * lcd_vcbus_setb(VBO_PXL_CTRL_T7 + offset,0x2,0,4);
	 * lcd_vcbus_setb(VBO_PXL_CTRL_T7 + offset,0x3,VBO_PXL_CTR1_BIT,VBO_PXL_CTR1_WID);
	 * set_vbyone_ctlbits(1,0,0);
	 */
	/* VBO_RGN_GEN clk always on */
	lcd_vcbus_setb(VBO_GCLK_MAIN_T7 + offset, 2, 2, 2);

	lcd_vcbus_setb(LCD_PORT_SWAP_T7 + offset, 0, 9, 2);
	/* lcd_vcbus_setb(LCD_PORT_SWAP + offset, 1, 8, 1);//reverse lane output order */

	lcd_vbyone_hw_filter(pdrv, 1);
	lcd_vcbus_setb(VBO_INSGN_CTRL_T7 + offset, 0, 2, 2);

	lcd_vcbus_setb(VBO_CTRL_L_T7 + offset, 1, 0, 1);

	lcd_vbyone_wait_timing_stable(pdrv);
	lcd_vbyone_sw_reset(pdrv);

	/* training hold */
	if (pdrv->config.control.vbyone_cfg.ctrl_flag & 0x4)
		lcd_vbyone_cdr_training_hold(pdrv, 1);
}

void lcd_vbyone_disable_t7(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset;

	offset = pdrv->data->offset_venc_if[pdrv->index];

	lcd_vcbus_setb(VBO_CTRL_L_T7 + offset, 0, 0, 1);
	/* clear insig setting */
	lcd_vcbus_setb(VBO_INSGN_CTRL_T7 + offset, 0, 2, 1);
	lcd_vcbus_setb(VBO_INSGN_CTRL_T7 + offset, 0, 0, 1);
}

void lcd_vbyone_wait_timing_stable(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset, timing_state;
	int i = 200;

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc[pdrv->index];

		timing_state = lcd_vcbus_read(VBO_INTR_STATE_T7 + offset) & 0x1ff;
		while ((timing_state) && (i > 0)) {
			/* clear video timing error intr */
			lcd_vcbus_setb(VBO_INTR_STATE_CTRL_T7 + offset, 0x7, 0, 3);
			lcd_vcbus_setb(VBO_INTR_STATE_CTRL_T7 + offset, 0, 0, 3);
			lcd_delay_ms(2);
			timing_state = lcd_vcbus_read(VBO_INTR_STATE_T7 + offset) & 0x1ff;
			i--;
		};
	} else {
		timing_state = lcd_vcbus_read(VBO_INTR_STATE) & 0x1ff;
		while ((timing_state) && (i > 0)) {
			/* clear video timing error intr */
			lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0x7, 0, 3);
			lcd_vcbus_setb(VBO_INTR_STATE_CTRL, 0, 0, 3);
			lcd_delay_ms(2);
			timing_state = lcd_vcbus_read(VBO_INTR_STATE) & 0x1ff;
			i--;
		};
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: vbyone timing state: 0x%03x, i=%d\n",
		      pdrv->index, timing_state, (200 - i));
	}
	lcd_delay_ms(2);
}

void lcd_vbyone_cdr_training_hold(struct aml_lcd_drv_s *pdrv, int flag)
{
	unsigned int offset, reg;

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc[pdrv->index];
		reg = VBO_FSM_HOLDER_H_T7 + offset;
	} else {
		reg = VBO_FSM_HOLDER_H;
	}
	if (flag) {
		LCDPR("[%d]: ctrl_flag for cdr_training_hold\n", pdrv->index);
		lcd_vcbus_setb(reg, 0xffff, 0, 16);
	} else {
		lcd_delay_ms(pdrv->config.control.vbyone_cfg.cdr_training_hold);
		lcd_vcbus_setb(reg, 0, 0, 16);
	}
}

void lcd_vbyone_wait_hpd(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_status, reg_ctrl, offset, val;
	int i = 0;

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_status = VBO_STATUS_L_T7 + offset;
		reg_ctrl = VBO_INSGN_CTRL_T7 + offset;
	} else {
		reg_status = VBO_STATUS_L;
		reg_ctrl = VBO_INSGN_CTRL;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s ...\n", pdrv->index, __func__);
	while (i++ < VX1_HPD_WAIT_TIMEOUT) {
		if (lcd_vcbus_getb(reg_status, 6, 1) == 0)
			break;
		lcd_delay_us(50);
	}

	val = lcd_vcbus_getb(reg_status, 6, 1);
	if (val) {
		LCDPR("[%d]: %s: hpd=%d\n", pdrv->index, __func__, val);
	} else {
		LCDPR("[%d]: %s: hpd=%d, i=%d\n", pdrv->index, __func__, val, i);
		/* force low only activated for actual hpd is low */
		lcd_vcbus_setb(reg_ctrl, 1, 2, 2);
	}

	if (pdrv->config.control.vbyone_cfg.ctrl_flag & 0x2) {
		LCDPR("[%d]: ctrl_flag for hpd_data delay\n", pdrv->index);
		lcd_delay_ms(pdrv->config.control.vbyone_cfg.hpd_data_delay);
	} else {
		usleep_range(10000, 10500);
		/* add 10ms delay for compatibility */
	}
}

static void lcd_vbyone_power_on_wait_lockn(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_status, offset;
	int i = 0;

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_status = VBO_STATUS_L_T7 + offset;
	} else {
		reg_status = VBO_STATUS_L;
	}

	/* training hold release */
	if (pdrv->config.control.vbyone_cfg.ctrl_flag & 0x4)
		lcd_vbyone_cdr_training_hold(pdrv, 0);
	while (i++ < VX1_LOCKN_WAIT_TIMEOUT) {
		if ((lcd_vcbus_read(reg_status) & 0x3f) == 0x20)
			break;
		lcd_delay_us(50);
	}
	LCDPR("[%d]: %s status: 0x%x, i=%d\n",
	      pdrv->index, __func__, lcd_vcbus_read(reg_status), i);
	/* power on reset */
	if (pdrv->config.control.vbyone_cfg.ctrl_flag & 0x1) {
		LCDPR("[%d]: ctrl_flag for power on reset\n", pdrv->index);
		lcd_delay_ms(pdrv->config.control.vbyone_cfg.power_on_reset_delay);
		lcd_vbyone_sw_reset(pdrv);
		i = 0;
		while (i++ < VX1_LOCKN_WAIT_TIMEOUT) {
			if ((lcd_vcbus_read(reg_status) & 0x3f) == 0x20)
				break;
			lcd_delay_us(50);
		}
		LCDPR("[%d]: %s status: 0x%x, i=%d\n",
		      pdrv->index, __func__, lcd_vcbus_read(reg_status), i);
	}

	vx1_training_wait_cnt = 0;
	vx1_training_stable_cnt = 0;
	vx1_fsm_acq_st = 0;
	lcd_vbyone_interrupt_enable(pdrv, 1);
}

#define LCD_VX1_WAIT_STABLE_POWER_ON_DELAY    300 /* ms */
void lcd_vbyone_power_on_wait_stable(struct aml_lcd_drv_s *pdrv)
{
	lcd_vx1_intr_request = 1;

	lcd_delay_ms(LCD_VX1_WAIT_STABLE_POWER_ON_DELAY);
	if (lcd_vx1_intr_request == 0)
		return;

	lcd_vbyone_power_on_wait_lockn(pdrv);
}

void lcd_vbyone_wait_stable(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_status, offset;
	int i = 0;

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_status = VBO_STATUS_L_T7 + offset;
	} else {
		reg_status = VBO_STATUS_L;
	}

	while (i++ < VX1_LOCKN_WAIT_TIMEOUT) {
		if ((lcd_vcbus_read(reg_status) & 0x3f) == 0x20)
			break;
		lcd_delay_us(50);
	}
	LCDPR("[%d]: %s status: 0x%x, i=%d\n",
	      pdrv->index, __func__, lcd_vcbus_read(reg_status), i);

	vx1_training_wait_cnt = 0;
	vx1_training_stable_cnt = 0;
	vx1_fsm_acq_st = 0;
	lcd_vbyone_interrupt_enable(pdrv, 1);
}

void lcd_vbyone_interrupt_enable(struct aml_lcd_drv_s *pdrv, int flag)
{
	struct vbyone_config_s *vx1_conf;
	unsigned int reg_ctrl, reg_holder_l, reg_unmask;
	unsigned int offset;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, flag);

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_ctrl = VBO_INTR_STATE_CTRL_T7 + offset;
		reg_holder_l = VBO_FSM_HOLDER_L_T7 + offset;
		reg_unmask = VBO_INTR_UNMASK_T7 + offset;
	} else {
		reg_ctrl = VBO_INTR_STATE_CTRL;
		reg_holder_l = VBO_FSM_HOLDER_L;
		reg_unmask = VBO_INTR_UNMASK;
	}

	vx1_conf = &pdrv->config.control.vbyone_cfg;
	if (flag) {
		lcd_vbyone_hw_filter(pdrv, 1);
		if (vx1_conf->intr_en) {
			vx1_fsm_acq_st = 0;
			/* clear interrupt */
			lcd_vcbus_setb(reg_ctrl, 0x01ff, 0, 9);
			lcd_vcbus_setb(reg_ctrl, 0, 0, 9);

			/* set hold in FSM_ACQ */
			if (vx1_conf->vsync_intr_en == 3 ||
			    vx1_conf->vsync_intr_en == 4)
				lcd_vcbus_setb(reg_holder_l, 0, 0, 16);
			else
				lcd_vcbus_setb(reg_holder_l, 0xffff, 0, 16);
			/* enable interrupt */
			lcd_vcbus_setb(reg_unmask, VBYONE_INTR_UNMASK, 0, 15);
		} else {
			/* mask interrupt */
			lcd_vcbus_write(reg_unmask, 0x0);
			if (vx1_conf->vsync_intr_en) {
				/* keep holder for vsync monitor enabled */
				/* set hold in FSM_ACQ */
				if (vx1_conf->vsync_intr_en == 3 ||
				    vx1_conf->vsync_intr_en == 4)
					lcd_vcbus_setb(reg_holder_l, 0, 0, 16);
				else
					lcd_vcbus_setb(reg_holder_l, 0xffff, 0, 16);
			} else {
				/* release holder for vsync monitor disabled */
				/* release hold in FSM_ACQ */
				lcd_vcbus_setb(reg_holder_l, 0, 0, 16);
			}

			vx1_fsm_acq_st = 0;
			/* clear interrupt */
			lcd_vcbus_setb(reg_ctrl, 0x01ff, 0, 9);
			lcd_vcbus_setb(reg_ctrl, 0, 0, 9);
		}
		lcd_vx1_isr_flag = 1;
	} else {
		lcd_vx1_isr_flag = 0;
		/* mask interrupt */
		lcd_vcbus_write(reg_unmask, 0x0);
		/* release hold in FSM_ACQ */
		lcd_vcbus_setb(reg_holder_l, 0, 0, 16);
		lcd_vbyone_hw_filter(pdrv, 0);
		lcd_vx1_intr_request = 0;
	}
}

#define LCD_PCLK_TOLERANCE          2000000  /* 2M */
#define LCD_ENCL_CLK_ERR_CNT_MAX    3
static unsigned char lcd_encl_clk_err_cnt;
static void lcd_pll_monitor_timer_handler(struct timer_list *timer)
{
	struct aml_lcd_drv_s *pdrv = from_timer(pdrv, timer, pll_mnt_timer);
	int encl_clk;

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		goto vx1_hpll_timer_end;

	encl_clk = lcd_encl_clk_msr(pdrv);
	if (encl_clk == 0)
		lcd_encl_clk_err_cnt++;
	else
		lcd_encl_clk_err_cnt = 0;
	if (lcd_encl_clk_err_cnt >= LCD_ENCL_CLK_ERR_CNT_MAX) {
		LCDPR("[%d]: %s: pll frequency error: %d\n",
		      pdrv->index, __func__, encl_clk);
		lcd_pll_reset(pdrv);
		lcd_encl_clk_err_cnt = 0;
	}

vx1_hpll_timer_end:
	pdrv->pll_mnt_timer.expires = jiffies + VX1_HPLL_INTERVAL;
	add_timer(&pdrv->pll_mnt_timer);
}

static void lcd_vx1_hold_reset(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_ctrl, reg_unmask;
	unsigned int offset;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_ctrl = VBO_INTR_STATE_CTRL_T7 + offset;
		reg_unmask = VBO_INTR_UNMASK_T7 + offset;
	} else {
		reg_ctrl = VBO_INTR_STATE_CTRL;
		reg_unmask = VBO_INTR_UNMASK;
	}

	vx1_fsm_acq_st = 0;
	lcd_vcbus_write(reg_unmask, 0x0); /* mask interrupt */
	/* clear interrupt */
	lcd_vcbus_setb(reg_ctrl, 0x01ff, 0, 9);
	lcd_vcbus_setb(reg_ctrl, 0, 0, 9);

	/* clear FSM_continue */
	lcd_vcbus_setb(reg_ctrl, 0, 15, 1);

	lcd_vbyone_sw_reset(pdrv);
	/* clear lockn raising flag */
	lcd_vcbus_setb(reg_ctrl, 1, 7, 1);
	/* clear lockn raising flag */
	lcd_vcbus_setb(reg_ctrl, 0, 7, 1);

	/* enable interrupt */
	lcd_vcbus_setb(reg_unmask, VBYONE_INTR_UNMASK, 0, 15);
}

static void lcd_vx1_timeout_reset(struct work_struct *work)
{
	struct aml_lcd_drv_s *pdrv;

	pdrv = container_of(work, struct aml_lcd_drv_s, vx1_reset_work);

	if (vx1_timeout_reset_flag == 0)
		return;

	LCDPR("[%d]: %s\n", pdrv->index, __func__);
	pdrv->module_reset(pdrv);
	if (pdrv->config.control.vbyone_cfg.intr_en)
		lcd_vx1_hold_reset(pdrv);
	vx1_timeout_reset_flag = 0;
}

#define VSYNC_CNT_VX1_RESET   5
#define VSYNC_CNT_VX1_STABLE  20
static unsigned short vsync_cnt = VSYNC_CNT_VX1_STABLE;
static int lcd_vbyone_vsync_handler(struct aml_lcd_drv_s *pdrv)
{
	struct vbyone_config_s *vx1_conf;
	unsigned int reg_status, reg_intr_ctrl, reg_intr_state, offset;

	if (lcd_vx1_vsync_isr_en == 0)
		return 0;

	vx1_conf = &pdrv->config.control.vbyone_cfg;
	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return 0;
	if (lcd_vx1_isr_flag == 0)
		return 0;

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_status = VBO_STATUS_L_T7 + offset;
		reg_intr_ctrl = VBO_INTR_STATE_CTRL_T7 + offset;
		reg_intr_state = VBO_INTR_STATE_T7 + offset;
	} else {
		reg_status = VBO_STATUS_L;
		reg_intr_ctrl = VBO_INTR_STATE_CTRL;
		reg_intr_state = VBO_INTR_STATE;
	}

	if (lcd_vcbus_read(reg_status) & 0x40) /* hpd detect */
		return 0;

	lcd_vcbus_setb(reg_intr_ctrl, 1, 0, 1);
	lcd_vcbus_setb(reg_intr_ctrl, 0, 0, 1);

	if (vx1_conf->vsync_intr_en == 0) {
		vx1_training_wait_cnt = 0;
		return 0;
	}

	if (vx1_conf->vsync_intr_en == 4) {
		if (vsync_cnt == 3) {
			lcd_vcbus_setb(reg_intr_ctrl, 0x3ff, 0, 10);
			lcd_vcbus_setb(reg_intr_ctrl, 0, 0, 10);
			vsync_cnt++;
		} else if (vsync_cnt >= 5) {
			vsync_cnt = 0;
			if ((lcd_vcbus_read(reg_intr_state) & 0x40)) {
				lcd_vbyone_hw_filter(pdrv, 0);
				lcd_vbyone_sw_reset(pdrv);
				LCDPR("[%d]: vx1 sw_reset 4\n", pdrv->index);
				while (lcd_vcbus_read(reg_status) & 0x4)
					break;
				lcd_vcbus_setb(reg_intr_ctrl, 0x3ff, 0, 10);
				lcd_vcbus_setb(reg_intr_ctrl, 0, 0, 10);
				lcd_vbyone_hw_filter(pdrv, 1);
			}
		} else {
			vsync_cnt++;
		}
	} else if (vx1_conf->vsync_intr_en == 3) {
		if (vsync_cnt < VSYNC_CNT_VX1_RESET) {
			vsync_cnt++;
		} else if (vsync_cnt == VSYNC_CNT_VX1_RESET) {
			lcd_vbyone_hw_filter(pdrv, 0);
			lcd_vbyone_sw_reset(pdrv);
			vsync_cnt++;
		} else if ((vsync_cnt > VSYNC_CNT_VX1_RESET) &&
			(vsync_cnt < VSYNC_CNT_VX1_STABLE)) {
			if (lcd_vcbus_read(reg_status) & 0x20) {
				vsync_cnt = VSYNC_CNT_VX1_STABLE;
				lcd_vbyone_hw_filter(pdrv, 1);
			} else {
				vsync_cnt++;
			}
		}
	} else if (vx1_conf->vsync_intr_en == 2) {
		if (vsync_cnt >= 5) {
			vsync_cnt = 0;
			if (!(lcd_vcbus_read(reg_status) & 0x20)) {
				lcd_vbyone_hw_filter(pdrv, 0);
				lcd_vbyone_sw_reset(pdrv);
				LCDPR("[%d]: vx1 sw_reset 2\n", pdrv->index);
				while (lcd_vcbus_read(reg_status) & 0x4)
					break;

				lcd_vcbus_setb(reg_intr_ctrl, 0, 15, 1);
				lcd_vcbus_setb(reg_intr_ctrl, 1, 15, 1);
			} else {
				lcd_vbyone_hw_filter(pdrv, 1);
			}
		} else {
			vsync_cnt++;
		}
	} else {
		if (vx1_training_wait_cnt >= VX1_TRAINING_TIMEOUT) {
			if ((lcd_vcbus_read(reg_status) & 0x3f) != 0x20) {
				if (vx1_timeout_reset_flag == 0) {
					vx1_timeout_reset_flag = 1;
					lcd_queue_work(&pdrv->vx1_reset_work);
				}
			} else {
				vx1_training_stable_cnt++;
				if (vx1_training_stable_cnt >= 5) {
					vx1_training_wait_cnt = 0;
					vx1_training_stable_cnt = 0;
				}
			}
		} else {
			vx1_training_wait_cnt++;
		}
	}

	return 0;
}

#define VX1_LOCKN_WAIT_CNT_MAX    50
static int vx1_lockn_wait_cnt;

#define VX1_FSM_ACQ_NEXT_STEP_CONTINUE     0
#define VX1_FSM_ACQ_NEXT_RELEASE_HOLDER    1
#define VX1_FSM_ACQ_NEXT                   VX1_FSM_ACQ_NEXT_STEP_CONTINUE
static irqreturn_t lcd_vbyone_interrupt_handler(int irq, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct vbyone_config_s *vx1_conf;
	unsigned int reg_status, reg_intr_ctrl, reg_intr_state, reg_unmask;
#if (VX1_FSM_ACQ_NEXT == VX1_FSM_ACQ_NEXT_RELEASE_HOLDER)
	unsigned int reg_holder_l;
#endif
	unsigned int data32, data32_1, offset;
	int encl_clk;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return IRQ_HANDLED;
	if (lcd_vx1_isr_flag == 0)
		return IRQ_HANDLED;

	vx1_conf = &pdrv->config.control.vbyone_cfg;
	if (vx1_conf->vsync_intr_en == 2 ||
	    vx1_conf->vsync_intr_en == 4)
		return IRQ_HANDLED;

	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_status = VBO_STATUS_L_T7 + offset;
		reg_intr_ctrl = VBO_INTR_STATE_CTRL_T7 + offset;
		reg_intr_state = VBO_INTR_STATE_T7 + offset;
		reg_unmask = VBO_INTR_UNMASK_T7 + offset;
#if (VX1_FSM_ACQ_NEXT == VX1_FSM_ACQ_NEXT_RELEASE_HOLDER)
		reg_holder_l = VBO_FSM_HOLDER_L_T7 + offset;
#endif
	} else {
		reg_status = VBO_STATUS_L;
		reg_intr_ctrl = VBO_INTR_STATE_CTRL;
		reg_intr_state = VBO_INTR_STATE;
		reg_unmask = VBO_INTR_UNMASK;
#if (VX1_FSM_ACQ_NEXT == VX1_FSM_ACQ_NEXT_RELEASE_HOLDER)
		reg_holder_l = VBO_FSM_HOLDER_L;
#endif
	}

	lcd_vcbus_write(reg_unmask, 0x0);  /* mask interrupt */

	encl_clk = lcd_encl_clk_msr(pdrv);
	data32 = (lcd_vcbus_read(reg_intr_state) & 0x7fff);
	/* clear the interrupt */
	data32_1 = ((data32 >> 9) << 3);
	if (data32 & 0x1c0)
		data32_1 |= (1 << 2);
	if (data32 & 0x38)
		data32_1 |= (1 << 1);
	if (data32 & 0x7)
		data32_1 |= (1 << 0);
	lcd_vcbus_setb(reg_intr_ctrl, data32_1, 0, 9);
	lcd_vcbus_setb(reg_intr_ctrl, 0, 0, 9);
	if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
		LCDPR("[%d]: vx1 intr status = 0x%04x, encl_clkmsr = %d",
		      pdrv->index, data32, encl_clk);
	}

	if (vx1_conf->vsync_intr_en == 3) {
		if (data32 & 0x1000) {
			if (vsync_cnt >= VSYNC_CNT_VX1_STABLE) {
				vsync_cnt = 0;
				LCDPR("[%d]: vx1 lockn rise edge occurred\n",
				      pdrv->index);
			}
		}
	} else {
		if (data32 & 0x200) {
			LCDPR("[%d]: vx1 htpdn fall occurred\n", pdrv->index);
			vx1_fsm_acq_st = 0;
			lcd_vcbus_setb(reg_intr_ctrl, 0, 15, 1);
		}
		if (data32 & 0x800) {
			if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
				LCDPR("[%d]: vx1 lockn fall occurred\n",
				      pdrv->index);
			}
			vx1_fsm_acq_st = 0;
			lcd_vcbus_setb(reg_intr_ctrl, 0, 15, 1);
			if (vx1_lockn_wait_cnt++ > VX1_LOCKN_WAIT_CNT_MAX) {
				if (vx1_timeout_reset_flag == 0) {
					vx1_timeout_reset_flag = 1;
					lcd_queue_work(&pdrv->vx1_reset_work);
					vx1_lockn_wait_cnt = 0;
					return IRQ_HANDLED;
				}
			}
		}
		if (data32 & 0x2000) {
			/* LCDPR("vx1 fsm_acq wait end\n"); */
			if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
				LCDPR("[%d]: vx1 status 0: 0x%x, fsm_acq_st: %d\n",
				      pdrv->index, lcd_vcbus_read(reg_status),
				      vx1_fsm_acq_st);
			}
			if (vx1_fsm_acq_st == 0) {
				/* clear FSM_continue */
				lcd_vcbus_setb(reg_intr_ctrl, 0, 15, 1);
				LCDPR("[%d]: vx1 sw reset\n", pdrv->index);
				lcd_vbyone_sw_reset(pdrv);
				/* clear lockn raising flag */
				lcd_vcbus_setb(reg_intr_ctrl, 1, 7, 1);
				/* clear lockn raising flag */
				lcd_vcbus_setb(reg_intr_ctrl, 0, 7, 1);
				vx1_fsm_acq_st = 1;
			} else {
				vx1_fsm_acq_st = 2;
				/* set FSM_continue */
#if (VX1_FSM_ACQ_NEXT == VX1_FSM_ACQ_NEXT_RELEASE_HOLDER)
				lcd_vcbus_setb(reg_holder_l, 0, 0, 16);
#else
				lcd_vcbus_setb(reg_intr_ctrl, 0, 15, 1);
				lcd_vcbus_setb(reg_intr_ctrl, 1, 15, 1);
#endif
			}
			if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
				LCDPR("[%d]: vx1 status 1: 0x%x, fsm_acq_st: %d\n",
				      pdrv->index, lcd_vcbus_read(reg_status),
				      vx1_fsm_acq_st);
			}
		}

		if (data32 & 0x1ff) {
			if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
				LCDPR("[%d]: vx1 reset for timing err\n",
				      pdrv->index);
			}
			vx1_fsm_acq_st = 0;
			lcd_vbyone_sw_reset(pdrv);
			/* clear lockn raising flag */
			lcd_vcbus_setb(reg_intr_ctrl, 1, 7, 1);
			/* clear lockn raising flag */
			lcd_vcbus_setb(reg_intr_ctrl, 0, 7, 1);
		}

		lcd_delay_us(20);
		if ((lcd_vcbus_read(reg_status) & 0x3f) == 0x20) {
			vx1_lockn_wait_cnt = 0;
			/* vx1_training_wait_cnt = 0; */
#if (VX1_FSM_ACQ_NEXT == VX1_FSM_ACQ_NEXT_RELEASE_HOLDER)
			lcd_vcbus_setb(reg_holder_l, 0xffff, 0, 16);
#endif
			lcd_vbyone_hw_filter(pdrv, 1);
			LCDPR("[%d]: vx1 fsm stable\n", pdrv->index);
		}
	}
	/* enable interrupt */
	lcd_vcbus_setb(reg_unmask, VBYONE_INTR_UNMASK, 0, 15);

	return IRQ_HANDLED;
}

static void lcd_vbyone_interrupt_init(struct aml_lcd_drv_s *pdrv)
{
	struct vbyone_config_s *vx1_conf;
	unsigned int reg_insig_ctrl, reg_holder_l, reg_holder_h, reg_ctrl_l;
	unsigned int reg_intr_ctrl, offset;

	vx1_conf = &pdrv->config.control.vbyone_cfg;
	if (pdrv->data->chip_type == LCD_CHIP_T5W ||
	    pdrv->data->chip_type == LCD_CHIP_T7 ||
	    pdrv->data->chip_type == LCD_CHIP_T3) {
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_insig_ctrl = VBO_INSGN_CTRL_T7 + offset;
		reg_holder_l = VBO_FSM_HOLDER_L_T7 + offset;
		reg_holder_h = VBO_FSM_HOLDER_H_T7 + offset;
		reg_ctrl_l = VBO_CTRL_L_T7 + offset;
		reg_intr_ctrl = VBO_INTR_STATE_CTRL_T7 + offset;
	} else {
		reg_insig_ctrl = VBO_INSGN_CTRL;
		reg_holder_l = VBO_FSM_HOLDER_L;
		reg_holder_h = VBO_FSM_HOLDER_H;
		reg_ctrl_l = VBO_CTRL_L;
		reg_intr_ctrl = VBO_INTR_STATE_CTRL;
	}

	/* release sw filter ctrl in uboot */
	lcd_vcbus_setb(reg_insig_ctrl, 0, 0, 1);
	lcd_vbyone_hw_filter(pdrv, 1);

	/* set hold in FSM_ACQ */
	if (vx1_conf->vsync_intr_en == 3 ||
	    vx1_conf->vsync_intr_en == 4)
		lcd_vcbus_setb(reg_holder_l, 0, 0, 16);
	else
		lcd_vcbus_setb(reg_holder_l, 0xffff, 0, 16);
	/* set hold in FSM_CDR */
	lcd_vcbus_setb(reg_holder_h, 0, 0, 16);
	/* not wait lockn to 1 in FSM_ACQ */
	lcd_vcbus_setb(reg_ctrl_l, 1, 10, 1);
	/* lcd_vcbus_setb(VBO_CTRL_L, 0, 9, 1);*/   /*use sw pll_lock */
	/* reg_pll_lock = 1 to realease force to FSM_ACQ*/
	/*lcd_vcbus_setb(VBO_CTRL_H, 1, 13, 1); */

	/* vx1 interrupt setting */
	lcd_vcbus_setb(reg_intr_ctrl, 1, 12, 1);    /* intr pulse width */
	lcd_vcbus_setb(reg_intr_ctrl, 0x01ff, 0, 9); /* clear interrupt */
	lcd_vcbus_setb(reg_intr_ctrl, 0, 0, 9);

	vx1_fsm_acq_st = 0;
	vx1_lockn_wait_cnt = 0;
	vx1_training_wait_cnt = 0;
	vx1_timeout_reset_flag = 0;
	vx1_training_stable_cnt = 0;
	lcd_encl_clk_err_cnt = 0;
	lcd_vx1_isr_flag = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

#define VBYONE_IRQF   IRQF_SHARED /* IRQF_DISABLED */ /* IRQF_SHARED */

int lcd_vbyone_interrupt_up(struct aml_lcd_drv_s *pdrv)
{
	struct vbyone_config_s *vx1_conf = &pdrv->config.control.vbyone_cfg;
	unsigned int venc_vx1_irq = 0;

	lcd_vbyone_interrupt_init(pdrv);

	INIT_WORK(&pdrv->vx1_reset_work, lcd_vx1_timeout_reset);

	if (!pdrv->res_vx1_irq) {
		LCDERR("[%d]: res_vx1_irq is null\n", pdrv->index);
		return -1;
	}
	venc_vx1_irq = pdrv->res_vx1_irq->start;
	LCDPR("[%d]: venc_vx1_irq: %d\n", pdrv->index, venc_vx1_irq);

	snprintf(pdrv->vbyone_isr_name, 10, "vbyone%d", pdrv->index);
	if (request_irq(venc_vx1_irq, lcd_vbyone_interrupt_handler, 0,
			pdrv->vbyone_isr_name, (void *)pdrv)) {
		LCDERR("[%d]: can't request %s\n",
		       pdrv->index, pdrv->vbyone_isr_name);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: request %s successful\n",
			      pdrv->index, pdrv->vbyone_isr_name);
		}
	}
	pdrv->vbyone_vsync_handler = lcd_vbyone_vsync_handler;

	lcd_vx1_intr_request = 1;
	lcd_vx1_vsync_isr_en = 1;
	vx1_conf->intr_state = 1;
	lcd_vbyone_interrupt_enable(pdrv, 1);

	/* add timer to monitor pll frequency */
	timer_setup(&pdrv->pll_mnt_timer, &lcd_pll_monitor_timer_handler, 0);
	/* vx1_hpll_timer.data = NULL; */
	pdrv->pll_mnt_timer.expires = jiffies + VX1_HPLL_INTERVAL;
	/*add_timer(&pdrv->pll_mnt_timer);*/
	/*LCDPR("[%d]: add vbyone hpll timer handler\n", pdrv->index);*/

	return 0;
}

void lcd_vbyone_interrupt_down(struct aml_lcd_drv_s *pdrv)
{
	del_timer_sync(&pdrv->pll_mnt_timer);

	lcd_vx1_vsync_isr_en = 0;
	lcd_vbyone_interrupt_enable(pdrv, 0);
	free_irq(pdrv->res_vx1_irq->start, (void *)pdrv);
	cancel_work_sync(&pdrv->vx1_reset_work);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: free vbyone irq\n", pdrv->index);
}

void lcd_vbyone_debug_cdr(struct aml_lcd_drv_s *pdrv)
{
	struct vbyone_config_s *vx1_conf = &pdrv->config.control.vbyone_cfg;
	unsigned int state;

	/* disable vx1 interrupt and vx1 vsync interrupt */
	vx1_conf->intr_en = 0;
	vx1_conf->vsync_intr_en = 0;
	lcd_vbyone_interrupt_enable(pdrv, 0);

	lcd_vbyone_force_cdr(pdrv);

	msleep(100);
	state = lcd_vbyone_get_fsm_state(pdrv);
	LCDPR("vbyone cdr: fsm state: 0x%02x\n", state);
}

void lcd_vbyone_debug_lock(struct aml_lcd_drv_s *pdrv)
{
	struct vbyone_config_s *vx1_conf = &pdrv->config.control.vbyone_cfg;
	unsigned int state;

	/* disable vx1 interrupt and vx1 vsync interrupt */
	vx1_conf->intr_en = 0;
	vx1_conf->vsync_intr_en = 0;
	lcd_vbyone_interrupt_enable(pdrv, 0);

	lcd_vbyone_force_lock(pdrv);

	msleep(20);
	state = lcd_vbyone_get_fsm_state(pdrv);
	LCDPR("vbyone cdr: fsm state: 0x%02x\n", state);
}

void lcd_vbyone_debug_reset(struct aml_lcd_drv_s *pdrv)
{
	struct vbyone_config_s *vx1_conf = &pdrv->config.control.vbyone_cfg;
	unsigned int intr_en, vintr_en, state;

	/* disable vx1 interrupt and vx1 vsync interrupt */
	intr_en = vx1_conf->intr_en;
	vintr_en = vx1_conf->vsync_intr_en;
	vx1_conf->intr_en = 0;
	vx1_conf->vsync_intr_en = 0;
	lcd_vbyone_interrupt_enable(pdrv, 0);

	lcd_vbyone_sw_reset(pdrv);

	/* recover vx1 interrupt and vx1 vsync interrupt */
	vx1_conf->intr_en = intr_en;
	vx1_conf->vsync_intr_en = vintr_en;
	lcd_vbyone_interrupt_enable(pdrv, vx1_conf->intr_state);
	msleep(200);
	state = lcd_vbyone_get_fsm_state(pdrv);
	LCDPR("vbyone reset: fsm state: 0x%02x\n", state);
}
