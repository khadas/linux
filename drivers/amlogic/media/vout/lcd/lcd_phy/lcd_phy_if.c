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

static struct lcd_phy_ctrl_s *lcd_phy_ctrl;

unsigned int lcd_phy_vswing_level_to_value(struct aml_lcd_drv_s *pdrv, unsigned int level)
{
	if (!lcd_phy_ctrl)
		return 0;

	if (!lcd_phy_ctrl->phy_vswing_level_to_val)
		return level;

	return lcd_phy_ctrl->phy_vswing_level_to_val(pdrv, level);
}

unsigned int lcd_phy_preem_level_to_value(struct aml_lcd_drv_s *pdrv, unsigned int level)
{
	if (!lcd_phy_ctrl)
		return 0;

	if (!lcd_phy_ctrl->phy_preem_level_to_val)
		return level;

	return lcd_phy_ctrl->phy_preem_level_to_val(pdrv, level);
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

	if (pdrv->status & LCD_STATUS_IF_ON)
		lcd_phy_set(pdrv, LCD_PHY_LOCK_LANE);

	return 0;
}

int lcd_phy_config_init(struct aml_lcd_drv_s *pdrv)
{
	lcd_phy_ctrl = NULL;

#ifdef CONFIG_AML_LCD_PXP
	return -1;
#endif

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_AXG:
		lcd_phy_ctrl = lcd_phy_config_init_axg(pdrv);
		break;
	case LCD_CHIP_G12A:
	case LCD_CHIP_G12B:
	case LCD_CHIP_SM1:
		lcd_phy_ctrl = lcd_phy_config_init_g12(pdrv);
		break;
	case LCD_CHIP_TL1:
	case LCD_CHIP_TM2:
		lcd_phy_ctrl = lcd_phy_config_init_tl1(pdrv);
		break;
	case LCD_CHIP_T5:
	case LCD_CHIP_T5D:
	case LCD_CHIP_T5W:
		lcd_phy_ctrl = lcd_phy_config_init_t5(pdrv);
		break;
	case LCD_CHIP_T7:
		lcd_phy_ctrl = lcd_phy_config_init_t7(pdrv);
		break;
	case LCD_CHIP_T3:
		lcd_phy_ctrl = lcd_phy_config_init_t3(pdrv);
		break;
	default:
		break;
	}

	return 0;
}
