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
#include <linux/amlogic/cpu_version.h>
#include "../lcd_reg.h"
#include "lcd_phy_config.h"
#include "../lcd_common.h"

static struct lcd_phy_ctrl_s *phy_ctrl_p;

static unsigned int lvds_vx1_p2p_phy_ch_tl1 = 0x00020002;
static unsigned int p2p_low_common_phy_ch_tl1 = 0x000b000b;

static unsigned int p2p_low_common_phy_preem_tl1[6] = {0x07, 0x17, 0x37, 0x77, 0xf7, 0xff};

static void lcd_phy_cntl_set_tl1(struct phy_config_s *phy, int status, unsigned int flag,
				int bypass, unsigned int mode, unsigned int ckdi)
{
	unsigned int chreg = 0, cntl16 = 0;
	unsigned int chdig = 0;
	unsigned char i = 0, bit;

	unsigned int preem_reg[6] = {
		HHI_DIF_CSI_PHY_CNTL1, HHI_DIF_CSI_PHY_CNTL2,
		HHI_DIF_CSI_PHY_CNTL3, HHI_DIF_CSI_PHY_CNTL4,
		HHI_DIF_CSI_PHY_CNTL6, HHI_DIF_CSI_PHY_CNTL7,
	};
	unsigned int amp_reg[6] = {
		HHI_DIF_CSI_PHY_CNTL8, HHI_DIF_CSI_PHY_CNTL9,
		HHI_DIF_CSI_PHY_CNTL10, HHI_DIF_CSI_PHY_CNTL11,
		HHI_DIF_CSI_PHY_CNTL12, HHI_DIF_CSI_PHY_CNTL13,
	};

	if (!phy_ctrl_p)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		chreg = phy_ctrl_p->ctrl_bit_on << 0;
		if (mode) {
			chreg |= lvds_vx1_p2p_phy_ch_tl1;
		} else {
			chreg |= p2p_low_common_phy_ch_tl1;
			if (phy->weakly_pull_down)
				chreg &= ~(1 << 3);
		}
		cntl16 = (ckdi << 12) | 0x80000000;
	} else {
		chreg = (!phy_ctrl_p->ctrl_bit_on) << 0;
		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0);
	}

	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL15, 0);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL16, cntl16);

	for (i = 0; i < 12; i++) {
		if (flag & (1 << i)) {
			bit = i & 0x1 ? 16 : 0;
			chdig = ckdi & (0x1 << i) && bypass ? 0 : 0x4;
			lcd_ana_setb(preem_reg[i >> 1], chreg, bit, 16);
			lcd_ana_setb(amp_reg[i >> 1], chdig, bit, 16);
		}
	}
}

void lcd_phy_tcon_chpi_bbc_init_tl1(struct lcd_config_s *pconf)
{
	unsigned int data32 = 0x06020602;
	unsigned int preem;
	unsigned int size;
	unsigned int n = 10;
	struct p2p_config_s *p2p_conf;

	if (!phy_ctrl_p)
		return;

	p2p_conf = &pconf->control.p2p_cfg;
	size = sizeof(p2p_low_common_phy_preem_tl1) / sizeof(unsigned int);

	/*get tcon tx pre_emphasis*/
	preem = p2p_conf->phy_preem & 0xf;

	/*check tx pre_emphasis ok or no*/
	if (preem >= size) {
		LCDERR("%s: invalid preem=0x%x, use default\n",
		       __func__, preem);
		preem = 0x1;
	}

	udelay(n);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL1, 1, 3, 1);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL1, 1, 19, 1);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL2, 1, 3, 1);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL2, 1, 19, 1);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL3, 1, 3, 1);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL3, 1, 19, 1);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL4, 1, 3, 1);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL4, 1, 19, 1);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL6, 1, 3, 1);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL6, 1, 19, 1);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL7, 1, 3, 1);
	lcd_ana_setb(HHI_DIF_CSI_PHY_CNTL7, 1, 19, 1);
	LCDPR("%s: delay: %dus\n", __func__, n);

	/*follow pre-emphasis*/
	data32 = p2p_low_common_phy_preem_tl1[preem];

	if (phy_ctrl_p->ctrl_bit_on)
		data32 &= ~((1 << 16) | (1 << 0));
	else
		data32 |= ((1 << 16) | (1 << 0));

	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL1, data32);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL2, data32);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL3, data32);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL4, data32);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL6, data32);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL7, data32);
}

static void lcd_lvds_phy_set_tl1(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int flag;
	unsigned short lvds_flag_6lane_map0[2][3] = {
		{0x000f, 0x001f, 0x003f}, {0x03c0, 0x07c0, 0x0fc0}};
	unsigned char bit_idx;

	if (status == LCD_PHY_LOCK_LANE)
		return;

	if (pdrv->config.basic.lcd_bits == 6)
		bit_idx = 0;
	else if (pdrv->config.basic.lcd_bits == 8)
		bit_idx = 1;
	else //pdrv->config.basic.lcd_bits == 10
		bit_idx = 2;

	if (pdrv->config.control.lvds_cfg.dual_port) {
		flag = lvds_flag_6lane_map0[0][bit_idx] | lvds_flag_6lane_map0[1][bit_idx];
	} else {
		if (pdrv->config.control.lvds_cfg.port_swap)
			flag = lvds_flag_6lane_map0[1][bit_idx];
		else
			flag = lvds_flag_6lane_map0[0][bit_idx];
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s: %d, flag=0x%04x\n", pdrv->index, __func__, status, flag);

	if (status) {
		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xff2027e0 | phy->vswing);
		lcd_phy_cntl_set_tl1(phy, status, flag, 1, 1, 0);
	} else {
		lcd_phy_cntl_set_tl1(phy, status, flag, 1, 1, 0);
	}
}

static void lcd_vbyone_phy_set_tl1(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	struct vbyone_config_s *vbyone_conf = &pdrv->config.control.vbyone_cfg;
	if (status == LCD_PHY_LOCK_LANE)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	vbyone_conf = &pdrv->config.control.vbyone_cfg;
	if (status) {
		if (phy->ext_pullup)
			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xff2027e0 | phy->vswing);
		else
			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xf02027a0 | phy->vswing);
		lcd_phy_cntl_set_tl1(phy, status, 0xff, 1, 1, 0);
	} else {
		lcd_phy_cntl_set_tl1(phy, status, 0xff, 1, 1, 0);
	}
}

static void lcd_mlvds_phy_set_tl1(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int ckdi, bypass;
	struct mlvds_config_s *mlvds_conf = &pdrv->config.control.mlvds_cfg;

	if (status == LCD_PHY_LOCK_LANE)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xff2027e0 | phy->vswing);
		ckdi = (mlvds_conf->pi_clk_sel << 12);
		bypass = (mlvds_conf->clk_phase >> 12) & 0x1;
		lcd_phy_cntl_set_tl1(phy, status, 0x0fff, bypass, 1, ckdi);
	} else {
		lcd_phy_cntl_set_tl1(phy, status, 0x0fff, 1, 1, 0);
	}
}

static void lcd_p2p_phy_set_tl1(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int p2p_type;
	unsigned int data32 = 0;
	struct p2p_config_s *p2p_conf;

	if (status == LCD_PHY_LOCK_LANE)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	p2p_conf = &pdrv->config.control.p2p_cfg;
	if (status) {
		p2p_type = p2p_conf->p2p_type & 0x1f;
		switch (p2p_type) {
		case P2P_CEDS:
		case P2P_CMPI:
		case P2P_ISP:
		case P2P_EPI:
			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xff2027a0 | phy->vswing);
			data32 |= lvds_vx1_p2p_phy_ch_tl1;
			lcd_phy_cntl_set_tl1(phy, status, 0x0fff, 1, 1, 0);
			break;
		case P2P_CHPI: /* low common mode */
		case P2P_CSPI:
		case P2P_USIT:
			if (p2p_type == P2P_CHPI)
				phy->weakly_pull_down = 1;
			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xfe60027f);
			lcd_phy_cntl_set_tl1(phy, status, 0x0fff, 1, 0, 0);
			break;
		default:
			LCDERR("%s: invalid p2p_type %d\n", __func__, p2p_type);
			break;
		}
	} else {
		lcd_phy_cntl_set_tl1(phy, status, 0x0fff, 1, 1, 0);
	}
}

struct lcd_phy_ctrl_s lcd_phy_ctrl_tl1 = {
	.ctrl_bit_on = 1,
	.lane_lock = 0,
	.phy_vswing_level_to_val = lcd_phy_vswing_level_to_value_dft,
	.phy_amp_dft_val = NULL,
	.phy_preem_level_to_val = lcd_phy_preem_level_to_value_legacy,
	.phy_set_lvds = lcd_lvds_phy_set_tl1,
	.phy_set_vx1 = lcd_vbyone_phy_set_tl1,
	.phy_set_mlvds = lcd_mlvds_phy_set_tl1,
	.phy_set_p2p = lcd_p2p_phy_set_tl1,
	.phy_set_mipi = NULL,
	.phy_set_edp = NULL,
};

struct lcd_phy_ctrl_s *lcd_phy_config_init_tl1(struct aml_lcd_drv_s *pdrv)
{
	phy_ctrl_p = &lcd_phy_ctrl_tl1;

	if ((pdrv->data->chip_type == LCD_CHIP_TL1 && is_meson_rev_b()) || is_meson_rev_a())
		phy_ctrl_p->ctrl_bit_on = 0;
	else
		phy_ctrl_p->ctrl_bit_on = 1;

	return phy_ctrl_p;
}
