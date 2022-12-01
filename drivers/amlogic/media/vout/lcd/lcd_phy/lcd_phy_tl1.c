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

static unsigned int lvds_vx1_p2p_phy_preem_tl1[] = {
	0x06,
	0x26,
	0x46,
	0x66,
	0x86,
	0xa6,
	0xf6
};

static unsigned int p2p_low_common_phy_preem_tl1[] = {
	0x07,
	0x17,
	0x37,
	0x77,
	0xf7,
	0xff
};

static void lcd_phy_cntl_set_tl1(int status, unsigned int chreg, int bypass,
				 unsigned int ckdi)
{
	unsigned int tmp = 0;
	unsigned int data = 0;
	unsigned int cntl16 = 0;

	if (!phy_ctrl_p)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		chreg |= ((phy_ctrl_p->ctrl_bit_on << 16) |
			  (phy_ctrl_p->ctrl_bit_on << 0));
		if (bypass)
			tmp |= ((1 << 18) | (1 << 2));
		cntl16 = ckdi | 0x80000000;
	} else {
		if (phy_ctrl_p->ctrl_bit_on)
			data = 0;
		else
			data = 1;
		chreg |= ((data << 16) | (data << 0));
		cntl16 = 0;
		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0);
	}

	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL15, tmp);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL16, cntl16);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL8, tmp);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL1, chreg);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL9, tmp);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL2, chreg);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL10, tmp);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL3, chreg);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL11, tmp);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL4, chreg);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL12, tmp);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL6, chreg);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL13, tmp);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL7, chreg);
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
	unsigned int vswing, preem;
	unsigned int data32 = 0, size;
	struct lvds_config_s *lvds_conf;

	if (status == LCD_PHY_LOCK_LANE)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	lvds_conf = &pdrv->config.control.lvds_cfg;
	if (status) {
		vswing = lvds_conf->phy_vswing & 0xf;
		preem = lvds_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing=0x%x, preem=0x%x\n", vswing, preem);

		size = sizeof(lvds_vx1_p2p_phy_preem_tl1) /
			sizeof(unsigned int);
		if (preem >= size) {
			LCDERR("%s: invalid preem=0x%x, use default\n",
				__func__, preem);
			preem = 0;
		}
		data32 = lvds_vx1_p2p_phy_preem_tl1[preem];
		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xff2027e0 | vswing);
		lcd_phy_cntl_set_tl1(status, data32, 0, 0);
	} else {
		lcd_phy_cntl_set_tl1(status, data32, 0, 0);
	}
}

static void lcd_vbyone_phy_set_tl1(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int vswing, preem, ext_pullup;
	unsigned int data32 = 0, size;
	struct vbyone_config_s *vbyone_conf;

	if (status == LCD_PHY_LOCK_LANE)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	vbyone_conf = &pdrv->config.control.vbyone_cfg;
	if (status) {
		ext_pullup = (vbyone_conf->phy_vswing >> 4) & 0x3;
		vswing = vbyone_conf->phy_vswing & 0xf;
		preem = vbyone_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("vswing=0x%x, preem=0x%x\n",
			      vbyone_conf->phy_vswing, preem);
		}

		size = sizeof(lvds_vx1_p2p_phy_preem_tl1) /
			sizeof(unsigned int);
		if (preem >= size) {
			LCDERR("%s: invalid preem=0x%x, use default\n",
				__func__, preem);
			preem = 0x1;
		}
		data32 = lvds_vx1_p2p_phy_preem_tl1[preem];
		if (ext_pullup) {
			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14,
				0xff2027e0 | vswing);
		} else {
			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14,
				0xf02027a0 | vswing);
		}
		lcd_phy_cntl_set_tl1(status, data32, 1, 0);
	} else {
		lcd_phy_cntl_set_tl1(status, data32, 1, 0);
	}
}

static void lcd_mlvds_phy_set_tl1(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int vswing, preem;
	unsigned int data32 = 0, size, ckdi;
	struct mlvds_config_s *mlvds_conf;

	if (status == LCD_PHY_LOCK_LANE)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	mlvds_conf = &pdrv->config.control.mlvds_cfg;
	if (status) {
		vswing = mlvds_conf->phy_vswing & 0xf;
		preem = mlvds_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing=0x%x, preem=0x%x\n", vswing, preem);

		size = sizeof(lvds_vx1_p2p_phy_preem_tl1) /
			sizeof(unsigned int);
		if (preem >= size) {
			LCDERR("%s: invalid preem=0x%x, use default\n",
				__func__, preem);
			preem = 0;
		}
		data32 = lvds_vx1_p2p_phy_preem_tl1[preem];
		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xff2027e0 | vswing);
		ckdi = (mlvds_conf->pi_clk_sel << 12);
		lcd_phy_cntl_set_tl1(status, data32, 0, ckdi);
	} else {
		lcd_phy_cntl_set_tl1(status, data32, 0, 0);
	}
}

static void lcd_p2p_phy_set_tl1(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int vswing, preem, p2p_type;
	unsigned int data32 = 0, size;
	struct p2p_config_s *p2p_conf;

	if (status == LCD_PHY_LOCK_LANE)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	p2p_conf = &pdrv->config.control.p2p_cfg;
	if (status) {
		vswing = p2p_conf->phy_vswing & 0xf;
		preem = p2p_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing=0x%x, preem=0x%x\n", vswing, preem);

		p2p_type = p2p_conf->p2p_type & 0x1f;
		switch (p2p_type) {
		case P2P_CEDS:
		case P2P_CMPI:
		case P2P_ISP:
		case P2P_EPI:
			size = sizeof(lvds_vx1_p2p_phy_preem_tl1) /
				sizeof(unsigned int);
			if (preem >= size) {
				LCDERR("%s: invalid preem=0x%x, use default\n",
					__func__, preem);
				preem = 0x1;
			}
			data32 = lvds_vx1_p2p_phy_preem_tl1[preem];
			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14,
				      0xff2027a0 | vswing);
			lcd_phy_cntl_set_tl1(status, data32, 1, 0);
			break;
		case P2P_CHPI: /* low common mode */
		case P2P_CSPI:
		case P2P_USIT:
			size = sizeof(p2p_low_common_phy_preem_tl1) /
				sizeof(unsigned int);
			if (preem >= size) {
				LCDERR("%s: invalid preem=0x%x, use default\n",
					__func__, preem);
				preem = 0x1;
			}
			data32 = p2p_low_common_phy_preem_tl1[preem];
			if (p2p_type == P2P_CHPI) {
				/* weakly pull down */
				data32 &= ~((1 << 19) | (1 << 3));
			}

			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xfe60027f);
			lcd_phy_cntl_set_tl1(status, data32, 1, 0);
			break;
		default:
			LCDERR("%s: invalid p2p_type %d\n", __func__, p2p_type);
			break;
		}
	} else {
		lcd_phy_cntl_set_tl1(status, data32, 1, 0);
	}
}

struct lcd_phy_ctrl_s lcd_phy_ctrl_tl1 = {
	.ctrl_bit_on = 1,
	.lane_lock = 0,
	.phy_vswing_level_to_val = lcd_phy_vswing_level_to_value_dft,
	.phy_preem_level_to_val = lcd_phy_preem_level_to_value_dft,
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
