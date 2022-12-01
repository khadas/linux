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

static void lcd_mipi_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int phy_reg, phy_bit, phy_width;
	unsigned int lane_cnt;

	if (status == LCD_PHY_LOCK_LANE)
		return;

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
			lane_cnt = DSI_LANE_COUNT_1_G12A;
			break;
		case 2:
			lane_cnt = DSI_LANE_COUNT_2_G12A;
			break;
		case 3:
			lane_cnt = DSI_LANE_COUNT_3_G12A;
			break;
		case 4:
			lane_cnt = DSI_LANE_COUNT_4_G12A;
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

struct lcd_phy_ctrl_s lcd_phy_ctrl_g12a = {
	.ctrl_bit_on = 0,
	.lane_lock = 0,
	.phy_vswing_level_to_val = NULL,
	.phy_preem_level_to_val = NULL,
	.phy_set_lvds = NULL,
	.phy_set_vx1 = NULL,
	.phy_set_mlvds = NULL,
	.phy_set_p2p = NULL,
	.phy_set_mipi = lcd_mipi_phy_set,
	.phy_set_edp = NULL,
};

struct lcd_phy_ctrl_s *lcd_phy_config_init_g12(struct aml_lcd_drv_s *pdrv)
{
	phy_ctrl_p = &lcd_phy_ctrl_g12a;
	return phy_ctrl_p;
}
