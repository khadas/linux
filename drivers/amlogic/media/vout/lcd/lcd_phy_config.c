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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/cpu_version.h>
#include "lcd_reg.h"
#include "lcd_phy_config.h"
#include "lcd_common.h"

static int phy_ctrl_bit_on;
static struct lcd_phy_ctrl_s *lcd_phy_ctrl;

static void lcd_phy_cntl_set_tl1(int status, unsigned int chreg, int bypass,
				 unsigned int ckdi)
{
	unsigned int tmp = 0;
	unsigned int data = 0;
	unsigned int cntl16 = 0;

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		chreg |= ((phy_ctrl_bit_on << 16) | (phy_ctrl_bit_on << 0));
		if (bypass)
			tmp |= ((1 << 18) | (1 << 2));
		cntl16 = ckdi | 0x80000000;
	} else {
		if (phy_ctrl_bit_on)
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

/*
 *    chreg: channel ctrl
 *    bypass: 1=bypass
 *    mode: 1=normal mode, 0=low common mode
 *    ckdi: clk phase for minilvds
 */
static void lcd_phy_cntl_set_t5(int status, unsigned int chreg, int bypass,
				unsigned int mode, unsigned int ckdi)
{
	unsigned int cntl15 = 0, cntl16 = 0;
	unsigned int data = 0;
	unsigned int tmp = 0;

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		chreg |= ((phy_ctrl_bit_on << 16) | (phy_ctrl_bit_on << 0));
		if (bypass)
			tmp |= ((1 << 18) | (1 << 2));
		if (mode)
			cntl15 = 0x00070000;
		else
			cntl15 = 0x000e0000;
		cntl16 = ckdi | 0x80000000;
	} else {
		if (phy_ctrl_bit_on)
			data = 0;
		else
			data = 1;
		chreg |= ((data << 16) | (data << 0));
		cntl15 = 0;
		cntl16 = 0;
		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0);
	}

	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL15, cntl15);
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

	p2p_conf = pconf->lcd_control.p2p_config;
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

	if (phy_ctrl_bit_on)
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

static void lcd_lvds_phy_set_tl1(struct lcd_config_s *pconf, int status)
{
	unsigned int vswing, preem;
	unsigned int data32 = 0, size;
	struct lvds_config_s *lvds_conf;

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	lvds_conf = pconf->lcd_control.lvds_config;
	if (status) {
		vswing = lvds_conf->phy_vswing & 0xf;
		preem = lvds_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag)
			LCDPR("vswing=0x%x, prrem=0x%x\n", vswing, preem);

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

static void lcd_lvds_phy_set_t5(struct lcd_config_s *pconf, int status)
{
	unsigned int vswing, preem;
	unsigned int data32 = 0, size;
	struct lvds_config_s *lvds_conf;

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	lvds_conf = pconf->lcd_control.lvds_config;
	if (status) {
		vswing = lvds_conf->phy_vswing & 0xf;
		preem = lvds_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag)
			LCDPR("vswing=0x%x, prrem=0x%x\n", vswing, preem);

		size = sizeof(lvds_vx1_p2p_phy_preem_tl1) /
			sizeof(unsigned int);
		if (preem >= size) {
			LCDERR("%s: invalid preem=0x%x, use default\n",
				__func__, preem);
			preem = 0;
		}
		data32 = lvds_vx1_p2p_phy_preem_tl1[preem];
		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xff2027e0 | vswing);
		lcd_phy_cntl_set_t5(status, data32, 0, 1, 0);
	} else {
		lcd_phy_cntl_set_t5(status, data32, 0, 0, 0);
	}
}

static void lcd_vbyone_phy_set_tl1(struct lcd_config_s *pconf, int status)
{
	unsigned int vswing, preem, ext_pullup;
	unsigned int data32 = 0, size;
	struct vbyone_config_s *vbyone_conf;

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	vbyone_conf = pconf->lcd_control.vbyone_config;
	if (status) {
		ext_pullup = (vbyone_conf->phy_vswing >> 4) & 0x3;
		vswing = vbyone_conf->phy_vswing & 0xf;
		preem = vbyone_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag) {
			LCDPR("vswing=0x%x, prrem=0x%x\n",
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

static void lcd_vbyone_phy_set_t5(struct lcd_config_s *pconf, int status)
{
	unsigned int vswing, preem, ext_pullup;
	unsigned int data32 = 0, size;
	struct vbyone_config_s *vbyone_conf;

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	vbyone_conf = pconf->lcd_control.vbyone_config;
	if (status) {
		ext_pullup = (vbyone_conf->phy_vswing >> 4) & 0x3;
		vswing = vbyone_conf->phy_vswing & 0xf;
		preem = vbyone_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag) {
			LCDPR("vswing=0x%x, prrem=0x%x\n",
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
		lcd_phy_cntl_set_t5(status, data32, 1, 1, 0);
	} else {
		lcd_phy_cntl_set_t5(status, data32, 1, 0, 0);
	}
}

static void lcd_mlvds_phy_set_tl1(struct lcd_config_s *pconf, int status)
{
	unsigned int vswing, preem;
	unsigned int data32 = 0, size, ckdi;
	struct mlvds_config_s *mlvds_conf;

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	mlvds_conf = pconf->lcd_control.mlvds_config;
	if (status) {
		vswing = mlvds_conf->phy_vswing & 0xf;
		preem = mlvds_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag)
			LCDPR("vswing=0x%x, prrem=0x%x\n", vswing, preem);

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

static void lcd_mlvds_phy_set_t5(struct lcd_config_s *pconf, int status)
{
	unsigned int vswing, preem;
	unsigned int data32 = 0, size, ckdi;
	struct mlvds_config_s *mlvds_conf;

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	mlvds_conf = pconf->lcd_control.mlvds_config;
	if (status) {
		vswing = mlvds_conf->phy_vswing & 0xf;
		preem = mlvds_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag)
			LCDPR("vswing=0x%x, prrem=0x%x\n", vswing, preem);

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
		lcd_phy_cntl_set_t5(status, data32, 0, 1, ckdi);
	} else {
		lcd_phy_cntl_set_t5(status, data32, 0, 0, 0);
	}
}

static void lcd_p2p_phy_set_tl1(struct lcd_config_s *pconf, int status)
{
	unsigned int vswing, preem;
	unsigned int data32 = 0, size;
	struct p2p_config_s *p2p_conf;

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	p2p_conf = pconf->lcd_control.p2p_config;
	if (status) {
		vswing = p2p_conf->phy_vswing & 0xf;
		preem = p2p_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag)
			LCDPR("vswing=0x%x, prrem=0x%x\n", vswing, preem);

		switch (p2p_conf->p2p_type) {
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
			if (p2p_conf->p2p_type == P2P_CHPI) {
				/* weakly pull down */
				data32 &= ~((1 << 19) | (1 << 3));
			}

			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xfe60027f);
			lcd_phy_cntl_set_tl1(status, data32, 1, 0);
			break;
		default:
			LCDERR("%s: invalid p2p_type %d\n",
				__func__, p2p_conf->p2p_type);
			break;
		}
	} else {
		lcd_phy_cntl_set_tl1(status, data32, 1, 0);
	}
}

static void lcd_p2p_phy_set_t5(struct lcd_config_s *pconf, int status)
{
	unsigned int vswing, preem;
	unsigned int data32 = 0, size;
	struct p2p_config_s *p2p_conf;

	if (lcd_debug_print_flag)
		LCDPR("%s: %d\n", __func__, status);

	p2p_conf = pconf->lcd_control.p2p_config;
	if (status) {
		vswing = p2p_conf->phy_vswing & 0xf;
		preem = p2p_conf->phy_preem & 0xf;
		if (lcd_debug_print_flag)
			LCDPR("vswing=0x%x, prrem=0x%x\n", vswing, preem);

		switch (p2p_conf->p2p_type) {
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
			lcd_phy_cntl_set_t5(status, data32, 1, 1, 0);
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
			if (p2p_conf->p2p_type == P2P_CHPI) {
				/* weakly pull down */
				data32 &= ~((1 << 19) | (1 << 3));
			}

			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, 0xfe60027f);
			lcd_phy_cntl_set_t5(status, data32, 1, 0, 0);
			break;
		default:
			LCDERR("%s: invalid p2p_type %d\n",
			       __func__, p2p_conf->p2p_type);
			break;
		}
	} else {
		lcd_phy_cntl_set_t5(status, data32, 1, 0, 0);
	}
}

static void lcd_mipi_phy_set_g12a(struct lcd_config_s *pconf, int status)
{
	unsigned int phy_reg, phy_bit, phy_width;
	unsigned int lane_cnt;

	if (status) {
		/* HHI_MIPI_CNTL0 */
		/* DIF_REF_CTL1:31-16bit, DIF_REF_CTL0:15-0bit */
		lcd_ana_write(HHI_MIPI_CNTL0,
			(0xa487 << 16) | (0x8 << 0));

		/* HHI_MIPI_CNTL1 */
		/* DIF_REF_CTL2:15-0bit; bandgap bit16 */
		lcd_ana_write(HHI_MIPI_CNTL1,
			(0x1 << 16) | (0x002e << 0));

		/* HHI_MIPI_CNTL2 */
		/* DIF_TX_CTL1:31-16bit, DIF_TX_CTL0:15-0bit */
		lcd_ana_write(HHI_MIPI_CNTL2,
			(0x2680 << 16) | (0x45a << 0));

		phy_reg = HHI_MIPI_CNTL2;
		phy_bit = MIPI_PHY_LANE_BIT;
		phy_width = MIPI_PHY_LANE_WIDTH;
		switch (pconf->lcd_control.mipi_config->lane_num) {
		case 1:
			lane_cnt = DSI_LANE_COUNT_1;
			break;
		case 2:
			lane_cnt = DSI_LANE_COUNT_2;
			break;
		case 3:
			lane_cnt = DSI_LANE_COUNT_3;
			break;
		case 4:
			lane_cnt = DSI_LANE_COUNT_4;
			break;
		default:
			lane_cnt = 0;
			break;
		}
		lcd_ana_setb(phy_reg, lane_cnt, phy_bit, phy_width);
	} else {
		lcd_ana_write(HHI_MIPI_CNTL0, 0);
		lcd_ana_write(HHI_MIPI_CNTL1, 0);
		lcd_ana_write(HHI_MIPI_CNTL2, 0);
	}
}

void lcd_lvds_phy_set(struct lcd_config_s *pconf, int status)
{
	if (!lcd_phy_ctrl)
		return;
	if (!lcd_phy_ctrl->phy_set_lvds)
		return;

	lcd_phy_ctrl->phy_set_lvds(pconf, status);
}

void lcd_vbyone_phy_set(struct lcd_config_s *pconf, int status)
{
	if (!lcd_phy_ctrl)
		return;
	if (!lcd_phy_ctrl->phy_set_vx1)
		return;

	lcd_phy_ctrl->phy_set_vx1(pconf, status);
}

void lcd_mlvds_phy_set(struct lcd_config_s *pconf, int status)
{
	if (!lcd_phy_ctrl)
		return;
	if (!lcd_phy_ctrl->phy_set_mlvds)
		return;

	lcd_phy_ctrl->phy_set_mlvds(pconf, status);
}

void lcd_p2p_phy_set(struct lcd_config_s *pconf, int status)
{
	if (!lcd_phy_ctrl)
		return;
	if (!lcd_phy_ctrl->phy_set_p2p)
		return;

	lcd_phy_ctrl->phy_set_p2p(pconf, status);
}

void lcd_mipi_phy_set(struct lcd_config_s *pconf, int status)
{
	if (!lcd_phy_ctrl)
		return;
	if (!lcd_phy_ctrl->phy_set_mipi)
		return;

	lcd_phy_ctrl->phy_set_mipi(pconf, status);
}

struct lcd_phy_ctrl_s lcd_phy_ctrl_g12a = {
	.phy_set_lvds = NULL,
	.phy_set_vx1 = NULL,
	.phy_set_mlvds = NULL,
	.phy_set_p2p = NULL,
	.phy_set_mipi = lcd_mipi_phy_set_g12a,
};

struct lcd_phy_ctrl_s lcd_phy_ctrl_tl1 = {
	.phy_set_lvds = lcd_lvds_phy_set_tl1,
	.phy_set_vx1 = lcd_vbyone_phy_set_tl1,
	.phy_set_mlvds = lcd_mlvds_phy_set_tl1,
	.phy_set_p2p = lcd_p2p_phy_set_tl1,
	.phy_set_mipi = NULL,
};

struct lcd_phy_ctrl_s lcd_phy_ctrl_t5 = {
	.phy_set_lvds = lcd_lvds_phy_set_t5,
	.phy_set_vx1 = lcd_vbyone_phy_set_t5,
	.phy_set_mlvds = lcd_mlvds_phy_set_t5,
	.phy_set_p2p = lcd_p2p_phy_set_t5,
	.phy_set_mipi = NULL,
};

int lcd_phy_probe(void)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();

	lcd_phy_ctrl = NULL;
	if (lcd_drv->lcd_pxp)
		return 0;

	switch (lcd_drv->data->chip_type) {
	case LCD_CHIP_G12A:
	case LCD_CHIP_G12B:
		lcd_phy_ctrl = &lcd_phy_ctrl_g12a;
		break;
	case LCD_CHIP_TL1:
		lcd_phy_ctrl = &lcd_phy_ctrl_tl1;
		if (is_meson_rev_a() || is_meson_rev_b())
			phy_ctrl_bit_on = 0;
		else
			phy_ctrl_bit_on = 1;
		break;
	case LCD_CHIP_TM2:
		lcd_phy_ctrl = &lcd_phy_ctrl_tl1;
		if (is_meson_rev_a())
			phy_ctrl_bit_on = 0;
		else
			phy_ctrl_bit_on = 1;
		break;
	case LCD_CHIP_T5:
	case LCD_CHIP_T5D:
	case LCD_CHIP_T7:
		lcd_phy_ctrl = &lcd_phy_ctrl_t5;
		phy_ctrl_bit_on = 1;
		break;
	default:
		break;
	}

	return 0;
}
