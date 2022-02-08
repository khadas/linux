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

static struct lcd_phy_ctrl_s *lcd_phy_ctrl;

static void lcd_mipi_phy_set_g12a(struct aml_lcd_drv_s *pdrv, int status)
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
		switch (pdrv->config.control.mipi_cfg.lane_num) {
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

static void lcd_phy_cntl_set_tl1(int status, unsigned int chreg, int bypass,
				 unsigned int ckdi)
{
	unsigned int tmp = 0;
	unsigned int data = 0;
	unsigned int cntl16 = 0;

	if (!lcd_phy_ctrl)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		chreg |= ((lcd_phy_ctrl->ctrl_bit_on << 16) |
			  (lcd_phy_ctrl->ctrl_bit_on << 0));
		if (bypass)
			tmp |= ((1 << 18) | (1 << 2));
		cntl16 = ckdi | 0x80000000;
	} else {
		if (lcd_phy_ctrl->ctrl_bit_on)
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

	if (!lcd_phy_ctrl)
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

	if (lcd_phy_ctrl->ctrl_bit_on)
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

/*
 *    chreg: channel ctrl
 *    bypass: 1=bypass
 *    mode: 1=normal mode, 0=low common mode
 *    ckdi: clk phase for minilvds
 */
static void lcd_phy_cntl_set_t5(struct phy_config_s *phy, int status, int bypass,
				unsigned int mode, unsigned int ckdi)
{
	unsigned int cntl15 = 0, cntl16 = 0;
	unsigned int data = 0, chreg = 0;
	unsigned int tmp = 0;

	if (!lcd_phy_ctrl)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		chreg |= ((lcd_phy_ctrl->ctrl_bit_on << 16) |
			  (lcd_phy_ctrl->ctrl_bit_on << 0));
		if (bypass)
			tmp |= ((1 << 18) | (1 << 2));
		if (mode) {
			chreg |= lvds_vx1_p2p_phy_ch_tl1;
			cntl15 = 0x00070000;
		} else {
			chreg |= p2p_low_common_phy_ch_tl1;
			if (phy->weakly_pull_down)
				chreg &= ~((1 << 19) | (1 << 3));
			cntl15 = 0x000e0000;
		}
		cntl16 = ckdi | 0x80000000;
	} else {
		if (lcd_phy_ctrl->ctrl_bit_on)
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
	data = ((phy->lane[0].preem & 0xff) << 8 |
	       (phy->lane[1].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL1, chreg | data);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL9, tmp);
	data = ((phy->lane[2].preem & 0xff) << 8 |
	       (phy->lane[3].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL2, chreg | data);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL10, tmp);
	data = ((phy->lane[4].preem & 0xff) << 8 |
	       (phy->lane[5].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL3, chreg | data);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL11, tmp);
	data = ((phy->lane[6].preem & 0xff) << 8 |
	       (phy->lane[7].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL4, chreg | data);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL12, tmp);
	data = ((phy->lane[8].preem & 0xff) << 8 |
	       (phy->lane[9].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL6, chreg | data);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL13, tmp);
	data = ((phy->lane[10].preem & 0xff) << 8 |
	       (phy->lane[11].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL7, chreg | data);
}

static void lcd_lvds_phy_set_t5(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int cntl14 = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing_level=0x%x\n", phy->vswing_level);

		cntl14 = 0xff2027e0 | phy->vswing;
		/* vcm */
		if ((phy->flag & (1 << 1))) {
			cntl14 &= ~(0x7ff << 4);
			cntl14 |= (phy->vcm & 0x7ff) << 4;
		}
		/* ref bias switch */
		if ((phy->flag & (1 << 2))) {
			cntl14 &= ~(1 << 15);
			cntl14 |= (phy->ref_bias & 0x1) << 15;
		}
		/* odt */
		if ((phy->flag & (1 << 3))) {
			cntl14 &= ~(0xff << 24);
			cntl14 |= (phy->odt & 0xff) << 24;
		}
		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, cntl14);
		lcd_phy_cntl_set_t5(phy, status, 0, 1, 0);
	} else {
		lcd_phy_cntl_set_t5(phy, status, 0, 0, 0);
	}
}

static void lcd_vbyone_phy_set_t5(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int cntl14 = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("vswing_level=0x%x, ext_pullup=%d\n",
			      phy->vswing_level, phy->ext_pullup);
		}

		if (phy->ext_pullup)
			cntl14 = 0xff2027e0 | phy->vswing;
		else
			cntl14 = 0xf02027a0 | phy->vswing;
		/* vcm */
		if ((phy->flag & (1 << 1))) {
			cntl14 &= ~(0x7ff << 4);
			cntl14 |= (phy->vcm & 0x7ff) << 4;
		}
		/* ref bias switch */
		if ((phy->flag & (1 << 2))) {
			cntl14 &= ~(1 << 15);
			cntl14 |= (phy->ref_bias & 0x1) << 15;
		}
		/* odt */
		if ((phy->flag & (1 << 3))) {
			cntl14 &= ~(0xff << 24);
			cntl14 |= (phy->odt & 0xff) << 24;
		}

		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, cntl14);
		lcd_phy_cntl_set_t5(phy, status, 1, 1, 0);
	} else {
		lcd_phy_cntl_set_t5(phy, status, 1, 0, 0);
	}
}

static void lcd_mlvds_phy_set_t5(struct aml_lcd_drv_s *pdrv, int status)
{
	struct mlvds_config_s *mlvds_conf;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int ckdi, cntl14 = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	mlvds_conf = &pdrv->config.control.mlvds_cfg;
	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing_level=0x%x\n", phy->vswing_level);

		cntl14 = 0xff2027e0 | phy->vswing;
		/* vcm */
		if ((phy->flag & (1 << 1))) {
			cntl14 &= ~(0x7ff << 4);
			cntl14 |= (phy->vcm & 0x7ff) << 4;
		}
		/* ref bias switch */
		if ((phy->flag & (1 << 2))) {
			cntl14 &= ~(1 << 15);
			cntl14 |= (phy->ref_bias & 0x1) << 15;
		}
		/* odt */
		if ((phy->flag & (1 << 3))) {
			cntl14 &= ~(0xff << 24);
			cntl14 |= (phy->odt & 0xff) << 24;
		}
		ckdi = (mlvds_conf->pi_clk_sel << 12);

		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, cntl14);
		lcd_phy_cntl_set_t5(phy, status, 0, 1, ckdi);
	} else {
		lcd_phy_cntl_set_t5(phy, status, 0, 0, 0);
	}
}

static void lcd_p2p_phy_set_t5(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int p2p_type, vcm_flag;
	struct p2p_config_s *p2p_conf;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int cntl14 = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	p2p_conf = &pdrv->config.control.p2p_cfg;
	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing_level=0x%x\n", phy->vswing_level);
		p2p_type = p2p_conf->p2p_type & 0x1f;
		vcm_flag = (p2p_conf->p2p_type >> 5) & 0x1;

		switch (p2p_type) {
		case P2P_CEDS:
		case P2P_CMPI:
		case P2P_ISP:
		case P2P_EPI:
			cntl14 = 0xff2027a0 | phy->vswing;
			/* vcm */
			if ((phy->flag & (1 << 1))) {
				cntl14 &= ~(0x7ff << 4);
				cntl14 |= (phy->vcm & 0x7ff) << 4;
			}
			/* ref bias switch */
			if ((phy->flag & (1 << 2))) {
				cntl14 &= ~(1 << 15);
				cntl14 |= (phy->ref_bias & 0x1) << 15;
			}
			/* odt */
			if ((phy->flag & (1 << 3))) {
				cntl14 &= ~(0xff << 24);
				cntl14 |= (phy->odt & 0xff) << 24;
			}
			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, cntl14);
			lcd_phy_cntl_set_t5(phy, status, 1, 1, 0);
			break;
		case P2P_CHPI: /* low common mode */
		case P2P_CSPI:
		case P2P_USIT:
			if (p2p_type == P2P_CHPI)
				phy->weakly_pull_down = 1;
			if (vcm_flag) /* 580mV */
				cntl14 = 0xe0600272;
			else /* default 385mV */
				cntl14 = 0xfe60027f;
			/* vcm */
			if ((phy->flag & (1 << 1))) {
				cntl14 &= ~(0x7ff << 4);
				cntl14 |= (phy->vcm & 0x7ff) << 4;
			}
			/* ref bias switch */
			if ((phy->flag & (1 << 2))) {
				cntl14 &= ~(1 << 15);
				cntl14 |= (phy->ref_bias & 0x1) << 15;
			}
			/* odt */
			if ((phy->flag & (1 << 3))) {
				cntl14 &= ~(0xff << 24);
				cntl14 |= (phy->odt & 0xff) << 24;
			}

			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, cntl14);
			lcd_phy_cntl_set_t5(phy, status, 1, 0, 0);
			break;
		default:
			LCDERR("%s: invalid p2p_type %d\n", __func__, p2p_type);
			break;
		}
	} else {
		lcd_phy_cntl_set_t5(phy, status, 1, 0, 0);
	}
}

static void lcd_phy_cntl_set_t7(unsigned int flag,
				unsigned int data_lane0_aux,
				unsigned int data_lane1_aux,
				unsigned int data_lane)
{
	if (flag & (1 << 0)) {
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL1, data_lane0_aux);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL2, data_lane1_aux);
	}
	if (flag & (1 << 1))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL3, data_lane);
	if (flag & (1 << 2))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL4, data_lane);
	if (flag & (1 << 3))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL5, data_lane);
	if (flag & (1 << 4))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL6, data_lane);
	if (flag & (1 << 5))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL7, data_lane);
	if (flag & (1 << 6))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL8, data_lane);
	if (flag & (1 << 7))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL9, data_lane);
	if (flag & (1 << 8)) {
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL10, data_lane0_aux);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL11, data_lane1_aux);
	}
	if (flag & (1 << 9))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL12, data_lane);
	if (flag & (1 << 10))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL13, data_lane);
	if (flag & (1 << 11))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, data_lane);
	if (flag & (1 << 12))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL15, data_lane);
	if (flag & (1 << 13))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL16, data_lane);
	if (flag & (1 << 14))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL17, data_lane);
	if (flag & (1 << 15))
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL18, data_lane);
}

static void lcd_lvds_phy_set_t7(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int flag, data_lane0_aux, data_lane1_aux, data_lane;
	struct lvds_config_s *lvds_conf;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;

	if (!lcd_phy_ctrl)
		return;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	lvds_conf = &pdrv->config.control.lvds_cfg;
	switch (pdrv->index) {
	case 0:
		if (lvds_conf->dual_port) {
			LCDERR("don't suuport lvds dual_port for drv_index %d\n",
			       pdrv->index);
			return;
		}
		flag = 0x1f;
		break;
	case 1:
		if (lvds_conf->dual_port) {
			LCDERR("don't suuport lvds dual_port for drv_index %d\n",
			       pdrv->index);
			return;
		}
		flag = (0x1f << 5);
		break;
	case 2:
		if (lvds_conf->dual_port)
			flag = (0x3ff << 5);
		else
			flag = (0x1f << 10);
		break;
	default:
		LCDERR("invalid drv_index %d for lvds\n", pdrv->index);
		return;
	}

	if (status) {
		if (lcd_phy_ctrl->lane_lock & flag) {
			LCDERR("phy lane already locked: 0x%x, invalid 0x%x\n",
				lcd_phy_ctrl->lane_lock, flag);
			return;
		}
		lcd_phy_ctrl->lane_lock |= flag;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("vswing_level=0x%x, preem_level=0x%x\n",
			      phy->vswing_level, phy->preem_level);
		}

		data_lane0_aux = 0x16430028;
		data_lane1_aux = 0x0100ffff;
		data_lane = 0x06530028 | (phy->preem_level << 28);
		lcd_phy_cntl_set_t7(flag, data_lane0_aux, data_lane1_aux, data_lane);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00406240 | phy->vswing_level);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0);
	} else {
		lcd_phy_ctrl->lane_lock &= ~flag;
		lcd_phy_cntl_set_t7(flag, 0, 0, 0);
		if (lcd_phy_ctrl->lane_lock == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("phy lane_lock: 0x%x\n", lcd_phy_ctrl->lane_lock);
}

static void lcd_vbyone_phy_set_t7(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int flag, data_lane0_aux, data_lane1_aux, data_lane;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;

	if (!lcd_phy_ctrl)
		return;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	switch (pdrv->index) {
	case 0:
		flag = 0xff;
		break;
	case 1:
		flag = (0xff << 8);
		break;
	default:
		LCDERR("invalid drv_index %d for vbyone\n", pdrv->index);
		return;
	}

	if (status) {
		if (lcd_phy_ctrl->lane_lock & flag) {
			LCDERR("phy lane already locked: 0x%x, invalid 0x%x\n",
				lcd_phy_ctrl->lane_lock, flag);
			return;
		}
		lcd_phy_ctrl->lane_lock |= flag;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("vswing_level=0x%x, preem_level=0x%x\n",
			      phy->vswing_level, phy->preem_level);
		}

		data_lane0_aux = 0x26430028;
		data_lane1_aux = 0x0000ffff;
		data_lane = 0x06530028 | (phy->preem_level << 28);
		lcd_phy_cntl_set_t7(flag, data_lane0_aux, data_lane1_aux, data_lane);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00401640 | phy->vswing_level);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0);
	} else {
		lcd_phy_ctrl->lane_lock &= ~flag;
		lcd_phy_cntl_set_t7(flag, 0, 0, 0);
		if (lcd_phy_ctrl->lane_lock == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("phy lane_lock: 0x%x\n", lcd_phy_ctrl->lane_lock);
}

static void lcd_mipi_phy_set_t7(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int flag, data_lane0_aux, data_lane1_aux, data_lane, temp;

	if (!lcd_phy_ctrl)
		return;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	switch (pdrv->index) {
	case 0:
		flag = 0x1f;
		break;
	case 1:
		flag = (0x1f << 8);
		break;
	default:
		LCDERR("invalid drv_index %d for mipi-dsi\n", pdrv->index);
		return;
	}

	if (status) {
		if (lcd_phy_ctrl->lane_lock & flag) {
			LCDERR("phy lane already locked: 0x%x, invalid 0x%x\n",
				lcd_phy_ctrl->lane_lock, flag);
			return;
		}
		lcd_phy_ctrl->lane_lock |= flag;

		data_lane0_aux = 0x022a0028;
		data_lane1_aux = 0x0000ffcf;
		data_lane = 0x022a0028;
		lcd_phy_cntl_set_t7(flag, data_lane0_aux, data_lane1_aux, data_lane);
		if (pdrv->index)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL13, 0x822a0028);
		else
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL4, 0x822a0028);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x1e406253);
		if (pdrv->index) {
			temp = 0xffff;
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, temp);
		} else {
			temp = (0xffff << 16);
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, temp);
		}
	} else {
		lcd_phy_ctrl->lane_lock &= ~flag;
		lcd_phy_cntl_set_t7(flag, 0, 0, 0);
		if (lcd_phy_ctrl->lane_lock == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
		if (pdrv->index)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0);
		else
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("phy lane_lock: 0x%x\n", lcd_phy_ctrl->lane_lock);
}

static void lcd_edp_phy_set_t7(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int flag, data_lane0_aux, data_lane1_aux, data_lane;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;

	if (!lcd_phy_ctrl)
		return;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	switch (pdrv->index) {
	case 0:
		flag = 0x1f;
		break;
	case 1:
		flag = (0x1f << 8);
		break;
	default:
		LCDERR("invalid drv_index %d for edp\n", pdrv->index);
		return;
	}

	if (status) {
		if (lcd_phy_ctrl->lane_lock & flag) {
			LCDERR("phy lane already locked: 0x%x, invalid 0x%x\n",
				lcd_phy_ctrl->lane_lock, flag);
			return;
		}
		lcd_phy_ctrl->lane_lock |= flag;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("vswing_level=0x%x, preem_level=0x%x\n",
			      phy->vswing_level, phy->preem_level);
		}

		data_lane0_aux = 0x46770038;
		data_lane1_aux = 0x0000ffff;
		data_lane = 0x06530028 | (phy->preem_level << 28);
		lcd_phy_cntl_set_t7(flag, data_lane0_aux, data_lane1_aux, data_lane);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00406240 | phy->vswing_level);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0);
	} else {
		lcd_phy_ctrl->lane_lock &= ~flag;
		lcd_phy_cntl_set_t7(flag, 0, 0, 0);
		if (lcd_phy_ctrl->lane_lock == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("phy lane_lock: 0x%x\n", lcd_phy_ctrl->lane_lock);
}

/*
 *    chreg: channel ctrl
 *    bypass: 1=bypass
 *    mode: 1=normal mode, 0=low common mode
 *    ckdi: clk phase for minilvds
 */
static void lcd_phy_cntl_set_t3(struct phy_config_s *phy, int status, int bypass,
				unsigned int mode, unsigned int ckdi)
{
	unsigned int cntl15 = 0, cntl16 = 0;
	unsigned int data = 0, chreg = 0, chctl = 0;

	if (!lcd_phy_ctrl)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		chreg |= ((lcd_phy_ctrl->ctrl_bit_on << 16) |
			  (lcd_phy_ctrl->ctrl_bit_on << 0));
		if (bypass)
			chctl |= ((1 << 18) | (1 << 2));
		if (mode) {
			chreg |= lvds_vx1_p2p_phy_ch_tl1;
			cntl15 = 0x00070000;
		} else {
			chreg |= p2p_low_common_phy_ch_tl1;
			if (phy->weakly_pull_down)
				chreg &= ~((1 << 19) | (1 << 3));
			cntl15 = 0x000e0000;
		}
		cntl16 = ckdi | 0x80000000;
	} else {
		if (lcd_phy_ctrl->ctrl_bit_on)
			data = 0;
		else
			data = 1;
		chreg |= ((data << 16) | (data << 0));
		cntl15 = 0;
		cntl16 = 0;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, 0);
	}

	lcd_ana_write(ANACTRL_DIF_PHY_CNTL15, cntl15);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL16, cntl16);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL8, chctl);
	data = ((phy->lane[0].preem & 0xff) << 8 |
	       (phy->lane[1].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL1, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL9, chctl);
	data = ((phy->lane[2].preem & 0xff) << 8 |
	       (phy->lane[3].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL2, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL10, chctl);
	data = ((phy->lane[4].preem & 0xff) << 8 |
	       (phy->lane[5].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL3, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL11, chctl);
	data = ((phy->lane[6].preem & 0xff) << 8 |
	       (phy->lane[7].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL4, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL12, chctl);
	data = ((phy->lane[8].preem & 0xff) << 8 |
	       (phy->lane[9].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL6, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL13, chctl);
	data = ((phy->lane[10].preem & 0xff) << 8 |
	       (phy->lane[11].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL7, chreg | data);
}

static void lcd_lvds_phy_set_t3(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int cntl14 = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing_level=0x%x\n", phy->vswing_level);

		cntl14 = 0xff2027e0 | phy->vswing;
		/* vcm */
		if ((phy->flag & (1 << 1))) {
			cntl14 &= ~(0x7ff << 4);
			cntl14 |= (phy->vcm & 0x7ff) << 4;
		}
		/* ref bias switch */
		if ((phy->flag & (1 << 2))) {
			cntl14 &= ~(1 << 15);
			cntl14 |= (phy->ref_bias & 0x1) << 15;
		}
		/* odt */
		if ((phy->flag & (1 << 3))) {
			cntl14 &= ~(0xff << 24);
			cntl14 |= (phy->odt & 0xff) << 24;
		}

		lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, cntl14);
		lcd_phy_cntl_set_t3(phy, status, 0, 1, 0);
	} else {
		lcd_phy_cntl_set_t3(phy, status, 0, 0, 0);
	}
}

static void lcd_vbyone_phy_set_t3(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int cntl14 = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("vswing_level=0x%x, ext_pullup=%d\n",
			      phy->vswing_level, phy->ext_pullup);
		}

		if (phy->ext_pullup)
			cntl14 = 0xff2027e0 | phy->vswing;
		else
			cntl14 = 0xf02027a0 | phy->vswing;
		/* vcm */
		if ((phy->flag & (1 << 1))) {
			cntl14 &= ~(0x7ff << 4);
			cntl14 |= (phy->vcm & 0x7ff) << 4;
		}
		/* ref bias switch */
		if ((phy->flag & (1 << 2))) {
			cntl14 &= ~(1 << 15);
			cntl14 |= (phy->ref_bias & 0x1) << 15;
		}
		/* odt */
		if ((phy->flag & (1 << 3))) {
			cntl14 &= ~(0xff << 24);
			cntl14 |= (phy->odt & 0xff) << 24;
		}

		lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, cntl14);
		lcd_phy_cntl_set_t3(phy, status, 1, 1, 0);
	} else {
		lcd_phy_cntl_set_t3(phy, status, 1, 0, 0);
	}
}

static void lcd_mlvds_phy_set_t3(struct aml_lcd_drv_s *pdrv, int status)
{
	struct mlvds_config_s *mlvds_conf;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int ckdi, cntl14 = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	mlvds_conf = &pdrv->config.control.mlvds_cfg;
	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing_level=0x%x\n", phy->vswing_level);

		cntl14 = 0xff2027e0 | phy->vswing;
		/* vcm */
		if ((phy->flag & (1 << 1))) {
			cntl14 &= ~(0x7ff << 4);
			cntl14 |= (phy->vcm & 0x7ff) << 4;
		}
		/* ref bias switch */
		if ((phy->flag & (1 << 2))) {
			cntl14 &= ~(1 << 15);
			cntl14 |= (phy->ref_bias & 0x1) << 15;
		}
		/* odt */
		if ((phy->flag & (1 << 3))) {
			cntl14 &= ~(0xff << 24);
			cntl14 |= (phy->odt & 0xff) << 24;
		}
		ckdi = (mlvds_conf->pi_clk_sel << 12);

		lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, cntl14);
		lcd_phy_cntl_set_t3(phy, status, 0, 1, ckdi);
	} else {
		lcd_phy_cntl_set_t3(phy, status, 0, 0, 0);
	}
}

static void lcd_p2p_phy_set_t3(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int p2p_type, vcm_flag;
	struct p2p_config_s *p2p_conf;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int cntl14;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	p2p_conf = &pdrv->config.control.p2p_cfg;
	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing_level=0x%x\n", phy->vswing_level);
		p2p_type = p2p_conf->p2p_type & 0x1f;
		vcm_flag = (p2p_conf->p2p_type >> 5) & 0x1;

		switch (p2p_type) {
		case P2P_CEDS:
		case P2P_CMPI:
		case P2P_ISP:
		case P2P_EPI:
			cntl14 = 0xff2027a0 | phy->vswing;
			/* vcm */
			if ((phy->flag & (1 << 1))) {
				cntl14 &= ~(0x7ff << 4);
				cntl14 |= (phy->vcm & 0x7ff) << 4;
			}
			/* ref bias switch */
			if ((phy->flag & (1 << 2))) {
				cntl14 &= ~(1 << 15);
				cntl14 |= (phy->ref_bias & 0x1) << 15;
			}
			/* odt */
			if ((phy->flag & (1 << 3))) {
				cntl14 &= ~(0xff << 24);
				cntl14 |= (phy->odt & 0xff) << 24;
			}

			lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, cntl14);
			lcd_phy_cntl_set_t3(phy, status, 1, 1, 0);
			break;
		case P2P_CHPI: /* low common mode */
		case P2P_CSPI:
		case P2P_USIT:
			if (p2p_type == P2P_CHPI)
				phy->weakly_pull_down = 1;
			if (vcm_flag) /* 580mV */
				cntl14 = 0xe0600272;
			else /* default 385mV */
				cntl14 = 0xfe60027f;
			/* vcm */
			if ((phy->flag & (1 << 1))) {
				cntl14 &= ~(0x7ff << 4);
				cntl14 |= (phy->vcm & 0x7ff) << 4;
			}
			/* ref bias switch */
			if ((phy->flag & (1 << 2))) {
				cntl14 &= ~(1 << 15);
				cntl14 |= (phy->ref_bias & 0x1) << 15;
			}
			/* odt */
			if ((phy->flag & (1 << 3))) {
				cntl14 &= ~(0xff << 24);
				cntl14 |= (phy->odt & 0xff) << 24;
			}

			lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, cntl14);
			lcd_phy_cntl_set_t3(phy, status, 1, 0, 0);
			break;
		default:
			LCDERR("%s: invalid p2p_type 0x%x\n", __func__, p2p_type);
			break;
		}
	} else {
		lcd_phy_cntl_set_t3(phy, status, 1, 0, 0);
	}
}

static void lcd_phy_cntl_set_t5w(struct phy_config_s *phy, int status, int bypass,
				 unsigned int mode, unsigned int ckdi)
{
	unsigned int cntl15 = 0, cntl16 = 0;
	unsigned int data = 0, chreg = 0, chctl = 0;

	if (!lcd_phy_ctrl)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		chreg |= ((lcd_phy_ctrl->ctrl_bit_on << 16) |
			  (lcd_phy_ctrl->ctrl_bit_on << 0));
		chctl |= ((0x7 << 19) | (0x7 << 3));
		if (bypass)
			chctl |= ((1 << 18) | (1 << 2));
		if (mode) {
			chreg |= lvds_vx1_p2p_phy_ch_tl1;
			cntl15 = 0x00070000;
		} else {
			chreg |= p2p_low_common_phy_ch_tl1;
			if (phy->weakly_pull_down)
				chreg &= ~((1 << 19) | (1 << 3));
			cntl15 = 0x000e0000;
		}
		cntl16 = ckdi | 0x80000000;
	} else {
		if (lcd_phy_ctrl->ctrl_bit_on)
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
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL8, chctl);
	data = ((phy->lane[0].preem & 0xff) << 8 |
		(phy->lane[1].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL1, chreg | data);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL9, chctl);
	data = ((phy->lane[2].preem & 0xff) << 8 |
		(phy->lane[3].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL2, chreg | data);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL10, chctl);
	data = ((phy->lane[4].preem & 0xff) << 8 |
		(phy->lane[5].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL3, chreg | data);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL11, chctl);
	data = ((phy->lane[6].preem & 0xff) << 8 |
		(phy->lane[7].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL4, chreg | data);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL12, chctl);
	data = ((phy->lane[8].preem & 0xff) << 8 |
		(phy->lane[9].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL6, chreg | data);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL13, chctl);
	data = ((phy->lane[10].preem & 0xff) << 8 |
		(phy->lane[11].preem & 0xff) << 24);
	lcd_ana_write(HHI_DIF_CSI_PHY_CNTL7, chreg | data);
}

static void lcd_lvds_phy_set_t5w(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int cntl14 = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing_level=0x%x\n", phy->vswing_level);

		cntl14 = 0xff2027e0 | phy->vswing;
		/* vcm */
		if ((phy->flag & (1 << 1))) {
			cntl14 &= ~(0x7ff << 4);
			cntl14 |= (phy->vcm & 0x7ff) << 4;
		}
		/* ref bias switch */
		if ((phy->flag & (1 << 2))) {
			cntl14 &= ~(1 << 15);
			cntl14 |= (phy->ref_bias & 0x1) << 15;
		}
		/* odt */
		if ((phy->flag & (1 << 3))) {
			cntl14 &= ~(0xff << 24);
			cntl14 |= (phy->odt & 0xff) << 24;
		}

		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, cntl14);
		lcd_phy_cntl_set_t5w(phy, status, 0, 1, 0);
	} else {
		lcd_phy_cntl_set_t5w(phy, status, 0, 0, 0);
	}
}

static void lcd_vbyone_phy_set_t5w(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int cntl14 = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("vswing_level=0x%x, ext_pullup=%d\n",
			      phy->vswing_level, phy->ext_pullup);
		}

		if (phy->ext_pullup)
			cntl14 = 0xff2027e0 | phy->vswing;
		else
			cntl14 = 0xf02027a0 | phy->vswing;
		/* vcm */
		if ((phy->flag & (1 << 1))) {
			cntl14 &= ~(0x7ff << 4);
			cntl14 |= (phy->vcm & 0x7ff) << 4;
		}
		/* ref bias switch */
		if ((phy->flag & (1 << 2))) {
			cntl14 &= ~(1 << 15);
			cntl14 |= (phy->ref_bias & 0x1) << 15;
		}
		/* odt */
		if ((phy->flag & (1 << 3))) {
			cntl14 &= ~(0xff << 24);
			cntl14 |= (phy->odt & 0xff) << 24;
		}

		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, cntl14);
		lcd_phy_cntl_set_t5w(phy, status, 1, 1, 0);
	} else {
		lcd_phy_cntl_set_t5w(phy, status, 1, 0, 0);
	}
}

static void lcd_mlvds_phy_set_t5w(struct aml_lcd_drv_s *pdrv, int status)
{
	struct mlvds_config_s *mlvds_conf;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int ckdi, cntl14 = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	mlvds_conf = &pdrv->config.control.mlvds_cfg;
	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing_level=0x%x\n", phy->vswing_level);

		cntl14 = 0xff2027e0 | phy->vswing;
		/* vcm */
		if ((phy->flag & (1 << 1))) {
			cntl14 &= ~(0x7ff << 4);
			cntl14 |= (phy->vcm & 0x7ff) << 4;
		}
		/* ref bias switch */
		if ((phy->flag & (1 << 2))) {
			cntl14 &= ~(1 << 15);
			cntl14 |= (phy->ref_bias & 0x1) << 15;
		}
		/* odt */
		if ((phy->flag & (1 << 3))) {
			cntl14 &= ~(0xff << 24);
			cntl14 |= (phy->odt & 0xff) << 24;
		}
		ckdi = (mlvds_conf->pi_clk_sel << 12);

		lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, cntl14);
		lcd_phy_cntl_set_t5w(phy, status, 0, 1, ckdi);
	} else {
		lcd_phy_cntl_set_t5w(phy, status, 0, 0, 0);
	}
}

static void lcd_p2p_phy_set_t5w(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int p2p_type, vcm_flag;
	struct p2p_config_s *p2p_conf;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int cntl14 = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	p2p_conf = &pdrv->config.control.p2p_cfg;
	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing_level=0x%x\n", phy->vswing_level);
		p2p_type = p2p_conf->p2p_type & 0x1f;
		vcm_flag = (p2p_conf->p2p_type >> 5) & 0x1;

		switch (p2p_type) {
		case P2P_CEDS:
		case P2P_CMPI:
		case P2P_ISP:
		case P2P_EPI:
			cntl14 = 0xff2027a0 | phy->vswing;
			/* vcm */
			if ((phy->flag & (1 << 1))) {
				cntl14 &= ~(0x7ff << 4);
				cntl14 |= (phy->vcm & 0x7ff) << 4;
			}
			/* ref bias switch */
			if ((phy->flag & (1 << 2))) {
				cntl14 &= ~(1 << 15);
				cntl14 |= (phy->ref_bias & 0x1) << 15;
			}
			/* odt */
			if ((phy->flag & (1 << 3))) {
				cntl14 &= ~(0xff << 24);
				cntl14 |= (phy->odt & 0xff) << 24;
			}
			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, cntl14);
			lcd_phy_cntl_set_t5w(phy, status, 1, 1, 0);
			break;
		case P2P_CHPI: /* low common mode */
		case P2P_CSPI:
		case P2P_USIT:
			if (p2p_type == P2P_CHPI)
				phy->weakly_pull_down = 1;
			if (vcm_flag) /* 580mV */
				cntl14 = 0xe0600272;
			else /* default 385mV */
				cntl14 = 0xfe60027f;
			/* vcm */
			if ((phy->flag & (1 << 1))) {
				cntl14 &= ~(0x7ff << 4);
				cntl14 |= (phy->vcm & 0x7ff) << 4;
			}
			/* ref bias switch */
			if ((phy->flag & (1 << 2))) {
				cntl14 &= ~(1 << 15);
				cntl14 |= (phy->ref_bias & 0x1) << 15;
			}
			/* odt */
			if ((phy->flag & (1 << 3))) {
				cntl14 &= ~(0xff << 24);
				cntl14 |= (phy->odt & 0xff) << 24;
			}

			lcd_ana_write(HHI_DIF_CSI_PHY_CNTL14, cntl14);
			lcd_phy_cntl_set_t5w(phy, status, 1, 0, 0);
			break;
		default:
			LCDERR("%s: invalid p2p_type %d\n", __func__, p2p_type);
			break;
		}
	} else {
		lcd_phy_cntl_set_t5w(phy, status, 1, 0, 0);
	}
}

unsigned int lcd_phy_vswing_level_to_value(struct aml_lcd_drv_s *pdrv, unsigned int level)
{
	unsigned int vswing_value = 0;

	vswing_value = level;

	return vswing_value;
}

unsigned int lcd_phy_preem_level_to_value(struct aml_lcd_drv_s *pdrv, unsigned int level)
{
	unsigned int p2p_type, size, preem_value = 0;

	switch (pdrv->config.basic.lcd_type) {
	case LCD_LVDS:
	case LCD_VBYONE:
	case LCD_MLVDS:
		size = sizeof(lvds_vx1_p2p_phy_preem_tl1) / sizeof(unsigned int);
		if (level >= size) {
			LCDERR("[%d]: %s: level %d invalid\n",
			       pdrv->index, __func__, level);
			level = size - 1;
		}
		preem_value = lvds_vx1_p2p_phy_preem_tl1[level];
		break;
	case LCD_P2P:
		p2p_type = pdrv->config.control.p2p_cfg.p2p_type & 0x1f;
		switch (p2p_type) {
		case P2P_CEDS:
		case P2P_CMPI:
		case P2P_ISP:
		case P2P_EPI:
			size = sizeof(lvds_vx1_p2p_phy_preem_tl1) / sizeof(unsigned int);
			if (level >= size) {
				LCDERR("[%d]: %s: level %d invalid\n",
					pdrv->index, __func__, level);
				level = size - 1;
			}
			preem_value = lvds_vx1_p2p_phy_preem_tl1[level];
			break;
		case P2P_CHPI: /* low common mode */
		case P2P_CSPI:
		case P2P_USIT:
			size = sizeof(p2p_low_common_phy_preem_tl1) / sizeof(unsigned int);
			if (level >= size) {
				LCDERR("[%d]: %s: level %d invalid\n",
					pdrv->index, __func__, level);
				level = size - 1;
			}
			preem_value = p2p_low_common_phy_preem_tl1[level];
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return preem_value;
}

void lcd_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	if (!pdrv->phy_set) {
		LCDPR("[%d]: %s: phy_set is null\n", pdrv->index, __func__);
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: %d, flag=0x%x\n",
		      pdrv->index, __func__, status, pdrv->config.phy_cfg.flag);
	}
	pdrv->phy_set(pdrv, status);
}

struct lcd_phy_ctrl_s lcd_phy_ctrl_g12a = {
	.ctrl_bit_on = 0,
	.lane_lock = 0,
	.phy_set_lvds = NULL,
	.phy_set_vx1 = NULL,
	.phy_set_mlvds = NULL,
	.phy_set_p2p = NULL,
	.phy_set_mipi = lcd_mipi_phy_set_g12a,
	.phy_set_edp = NULL,
};

struct lcd_phy_ctrl_s lcd_phy_ctrl_tl1 = {
	.ctrl_bit_on = 1,
	.lane_lock = 0,
	.phy_set_lvds = lcd_lvds_phy_set_tl1,
	.phy_set_vx1 = lcd_vbyone_phy_set_tl1,
	.phy_set_mlvds = lcd_mlvds_phy_set_tl1,
	.phy_set_p2p = lcd_p2p_phy_set_tl1,
	.phy_set_mipi = NULL,
	.phy_set_edp = NULL,
};

struct lcd_phy_ctrl_s lcd_phy_ctrl_t5 = {
	.ctrl_bit_on = 1,
	.lane_lock = 0,
	.phy_set_lvds = lcd_lvds_phy_set_t5,
	.phy_set_vx1 = lcd_vbyone_phy_set_t5,
	.phy_set_mlvds = lcd_mlvds_phy_set_t5,
	.phy_set_p2p = lcd_p2p_phy_set_t5,
	.phy_set_mipi = NULL,
	.phy_set_edp = NULL,
};

struct lcd_phy_ctrl_s lcd_phy_ctrl_t7 = {
	.ctrl_bit_on = 1,
	.lane_lock = 0,
	.phy_set_lvds = lcd_lvds_phy_set_t7,
	.phy_set_vx1 = lcd_vbyone_phy_set_t7,
	.phy_set_mlvds = NULL,
	.phy_set_p2p = NULL,
	.phy_set_mipi = lcd_mipi_phy_set_t7,
	.phy_set_edp = lcd_edp_phy_set_t7,
};

struct lcd_phy_ctrl_s lcd_phy_ctrl_t3 = {
	.ctrl_bit_on = 1,
	.lane_lock = 0,
	.phy_set_lvds = lcd_lvds_phy_set_t3,
	.phy_set_vx1 = lcd_vbyone_phy_set_t3,
	.phy_set_mlvds = lcd_mlvds_phy_set_t3,
	.phy_set_p2p = lcd_p2p_phy_set_t3,
	.phy_set_mipi = NULL,
	.phy_set_edp = NULL,
};

struct lcd_phy_ctrl_s lcd_phy_ctrl_t5w = {
	.ctrl_bit_on = 1,
	.lane_lock = 0,
	.phy_set_lvds = lcd_lvds_phy_set_t5w,
	.phy_set_vx1 = lcd_vbyone_phy_set_t5w,
	.phy_set_mlvds = lcd_mlvds_phy_set_t5w,
	.phy_set_p2p = lcd_p2p_phy_set_t5w,
	.phy_set_mipi = NULL,
	.phy_set_edp = NULL,
};

int lcd_phy_probe(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->lcd_pxp) {
		LCDPR("[%d]: %s: lcd_pxp bypass\n", pdrv->index, __func__);
		pdrv->phy_set = NULL;
		return 0;
	}

	if (!lcd_phy_ctrl)
		return 0;

	switch (pdrv->config.basic.lcd_type) {
	case LCD_LVDS:
		pdrv->phy_set = lcd_phy_ctrl->phy_set_lvds;
		break;
	case LCD_VBYONE:
		pdrv->phy_set = lcd_phy_ctrl->phy_set_vx1;
		break;
	case LCD_MLVDS:
		pdrv->phy_set = lcd_phy_ctrl->phy_set_mlvds;
		break;
	case LCD_P2P:
		pdrv->phy_set = lcd_phy_ctrl->phy_set_p2p;
		break;
	case LCD_MIPI:
		pdrv->phy_set = lcd_phy_ctrl->phy_set_mipi;
		break;
	case LCD_EDP:
		pdrv->phy_set = lcd_phy_ctrl->phy_set_edp;
		break;
	default:
		pdrv->phy_set = NULL;
		break;
	}

	return 0;
}

int lcd_phy_config_init(struct aml_lcd_drv_s *pdrv)
{
	lcd_phy_ctrl = NULL;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_G12A:
	case LCD_CHIP_G12B:
	case LCD_CHIP_SM1:
		lcd_phy_ctrl = &lcd_phy_ctrl_g12a;
		break;
	case LCD_CHIP_TL1:
		lcd_phy_ctrl = &lcd_phy_ctrl_tl1;
		if (is_meson_rev_a() || is_meson_rev_b())
			lcd_phy_ctrl->ctrl_bit_on = 0;
		else
			lcd_phy_ctrl->ctrl_bit_on = 1;
		break;
	case LCD_CHIP_TM2:
		lcd_phy_ctrl = &lcd_phy_ctrl_tl1;
		if (is_meson_rev_a())
			lcd_phy_ctrl->ctrl_bit_on = 0;
		else
			lcd_phy_ctrl->ctrl_bit_on = 1;
		break;
	case LCD_CHIP_T5:
	case LCD_CHIP_T5D:
		lcd_phy_ctrl = &lcd_phy_ctrl_t5;
		break;
	case LCD_CHIP_T7:
		lcd_phy_ctrl = &lcd_phy_ctrl_t7;
		break;
	case LCD_CHIP_T3:
		lcd_phy_ctrl = &lcd_phy_ctrl_t3;
		break;
	case LCD_CHIP_T5W:
		lcd_phy_ctrl = &lcd_phy_ctrl_t5w;
		break;
	default:
		break;
	}

	return 0;
}
