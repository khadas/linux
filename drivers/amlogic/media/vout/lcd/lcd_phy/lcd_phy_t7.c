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

static unsigned int lcd_phy_vswing_level_to_value_t7(struct aml_lcd_drv_s *pdrv, unsigned int level)
{
	unsigned int vswing_value = 0;

	vswing_value = level;

	return vswing_value;
}

static unsigned int lcd_phy_preem_level_to_value_t7(struct aml_lcd_drv_s *pdrv, unsigned int level)
{
	unsigned int preem_value = 0;

	preem_value = level;

	return preem_value;
}

static void lcd_phy_cntl_set(unsigned int flag,
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

static void lcd_lvds_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int flag, data_lane0_aux, data_lane1_aux, data_lane;
	struct lvds_config_s *lvds_conf;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;

	if (!phy_ctrl_p)
		return;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	lvds_conf = &pdrv->config.control.lvds_cfg;
	switch (pdrv->index) {
	case 0:
		if (lvds_conf->dual_port) {
			LCDERR("don't support lvds dual_port for drv_index %d\n",
			       pdrv->index);
			return;
		}
		flag = 0x1f;
		break;
	case 1:
		if (lvds_conf->dual_port) {
			LCDERR("don't support lvds dual_port for drv_index %d\n",
			       pdrv->index);
			return;
		}
		flag = (0x1f << 10);
		break;
	case 2:
		if (lvds_conf->dual_port)
			flag = (0x3ff << 5);
		else
			flag = (0x1f << 5);
		break;
	default:
		LCDERR("invalid drv_index %d for lvds\n", pdrv->index);
		return;
	}

	if (status) {
		if ((phy_ctrl_p->lane_lock & flag) &&
			((phy_ctrl_p->lane_lock & flag) != flag)) {
			LCDERR("phy lane already locked: 0x%x, invalid 0x%x\n",
				phy_ctrl_p->lane_lock, flag);
			return;
		}
		phy_ctrl_p->lane_lock |= flag;
		if (status == LCD_PHY_LOCK_LANE)
			goto phy_set_done;

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("vswing_level=0x%x, preem_level=0x%x\n",
			      phy->vswing_level, phy->preem_level);
		}

		data_lane0_aux = 0x06430028 | (phy->preem_level << 28);
		data_lane1_aux = 0x0100ffff;
		data_lane = 0x06530028 | (phy->preem_level << 28);
		lcd_phy_cntl_set(flag, data_lane0_aux, data_lane1_aux, data_lane);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00406240 | phy->vswing_level);
	} else {
		phy_ctrl_p->lane_lock &= ~flag;
		lcd_phy_cntl_set(flag, 0, 0, 0);
		if (phy_ctrl_p->lane_lock == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
	}

phy_set_done:
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("phy lane_lock: 0x%x\n", phy_ctrl_p->lane_lock);
}

static void lcd_vbyone_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int flag, data_lane0_aux, data_lane1_aux, data_lane;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;

	if (!phy_ctrl_p)
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
		if ((phy_ctrl_p->lane_lock & flag) &&
			((phy_ctrl_p->lane_lock & flag) != flag)) {
			LCDERR("phy lane already locked: 0x%x, invalid 0x%x\n",
				phy_ctrl_p->lane_lock, flag);
			return;
		}
		phy_ctrl_p->lane_lock |= flag;
		if (status == LCD_PHY_LOCK_LANE)
			goto phy_set_done;

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("vswing_level=0x%x, preem_level=0x%x\n",
			      phy->vswing_level, phy->preem_level);
		}

		data_lane0_aux = 0x06430028 | (phy->preem_level << 28);
		data_lane1_aux = 0x0000ffff;
		data_lane = 0x06530028 | (phy->preem_level << 28);
		lcd_phy_cntl_set(flag, data_lane0_aux, data_lane1_aux, data_lane);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00401640 | phy->vswing_level);
	} else {
		phy_ctrl_p->lane_lock &= ~flag;
		lcd_phy_cntl_set(flag, 0, 0, 0);
		if (phy_ctrl_p->lane_lock == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
	}

phy_set_done:
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("phy lane_lock: 0x%x\n", phy_ctrl_p->lane_lock);
}

static void lcd_mipi_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int flag, data_lane0_aux, data_lane1_aux, data_lane, temp;

	if (!phy_ctrl_p)
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
		if ((phy_ctrl_p->lane_lock & flag) &&
			((phy_ctrl_p->lane_lock & flag) != flag)) {
			LCDERR("phy lane already locked: 0x%x, invalid 0x%x\n",
				phy_ctrl_p->lane_lock, flag);
			return;
		}
		phy_ctrl_p->lane_lock |= flag;
		if (status == LCD_PHY_LOCK_LANE)
			goto phy_set_done;

		data_lane0_aux = 0x022a0028;
		data_lane1_aux = 0x0000ffcf;
		data_lane = 0x022a0028;
		lcd_phy_cntl_set(flag, data_lane0_aux, data_lane1_aux, data_lane);
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
		phy_ctrl_p->lane_lock &= ~flag;
		lcd_phy_cntl_set(flag, 0, 0, 0);
		if (phy_ctrl_p->lane_lock == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
		if (pdrv->index)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, 0);
		else
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, 0);
	}

phy_set_done:
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("phy lane_lock: 0x%x\n", phy_ctrl_p->lane_lock);
}

static void lcd_edp_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int flag, data_lane0_aux, data_lane1_aux, data_lane;
	struct phy_config_s *phy = &pdrv->config.phy_cfg;

	if (!phy_ctrl_p)
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
		if ((phy_ctrl_p->lane_lock & flag) &&
			((phy_ctrl_p->lane_lock & flag) != flag)) {
			LCDERR("phy lane already locked: 0x%x, invalid 0x%x\n",
				phy_ctrl_p->lane_lock, flag);
			return;
		}
		phy_ctrl_p->lane_lock |= flag;
		if (status == LCD_PHY_LOCK_LANE)
			goto phy_set_done;

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("vswing_level=0x%x, preem_level=0x%x\n",
			      phy->vswing_level, phy->preem_level);
		}

		data_lane0_aux = 0x46770038;
		data_lane1_aux = 0x0000ffff;
		data_lane = 0x06530028 | (phy->preem_level << 28);
		lcd_phy_cntl_set(flag, data_lane0_aux, data_lane1_aux, data_lane);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00406240 | phy->vswing_level);
	} else {
		phy_ctrl_p->lane_lock &= ~flag;
		lcd_phy_cntl_set(flag, 0, 0, 0);
		if (phy_ctrl_p->lane_lock == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
	}

phy_set_done:
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("phy lane_lock: 0x%x\n", phy_ctrl_p->lane_lock);
}

struct lcd_phy_ctrl_s lcd_phy_ctrl_t7 = {
	.ctrl_bit_on = 1,
	.lane_lock = 0,
	.phy_vswing_level_to_val = lcd_phy_vswing_level_to_value_t7,
	.phy_preem_level_to_val = lcd_phy_preem_level_to_value_t7,
	.phy_set_lvds = lcd_lvds_phy_set,
	.phy_set_vx1 = lcd_vbyone_phy_set,
	.phy_set_mlvds = NULL,
	.phy_set_p2p = NULL,
	.phy_set_mipi = lcd_mipi_phy_set,
	.phy_set_edp = lcd_edp_phy_set,
};

struct lcd_phy_ctrl_s *lcd_phy_config_init_t7(struct aml_lcd_drv_s *pdrv)
{
	phy_ctrl_p = &lcd_phy_ctrl_t7;
	return phy_ctrl_p;
}
