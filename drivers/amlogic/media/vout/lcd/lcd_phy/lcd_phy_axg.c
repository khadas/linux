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

static void lcd_mipi_phy_set_axg(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int lane;

	if (status == LCD_PHY_LOCK_LANE)
		return;

	if (status) {
		/* HHI_MIPI_CNTL0 */
		/* DIF_REF_CTL1:31-16bit, DIF_REF_CTL0:15-0bit */
		lcd_hiu_setb(HHI_MIPI_CNTL0, 0x1b8, 16, 10);
		lcd_hiu_setb(HHI_MIPI_CNTL0, 1, 26, 1); /* bandgap */
		lcd_hiu_setb(HHI_MIPI_CNTL0, 1, 29, 1); /* current */
		lcd_hiu_setb(HHI_MIPI_CNTL0, 1, 31, 1);
		lcd_hiu_setb(HHI_MIPI_CNTL0, 0x8, 0, 16);

		/* HHI_MIPI_CNTL1 */
		/* DIF_REF_CTL2:15-0bit */
		lcd_hiu_write(HHI_MIPI_CNTL1, (0x001e << 0));

		/* HHI_MIPI_CNTL2 */
		/* DIF_TX_CTL1:31-16bit, DIF_TX_CTL0:15-0bit */
		lcd_hiu_write(HHI_MIPI_CNTL2, (0x26e0 << 16) | (0x459 << 0));

		if (pdrv->config.control.mipi_cfg.lane_num == 1)
			lane = 0x14;
		else if (pdrv->config.control.mipi_cfg.lane_num == 2)
			lane = 0x1c;
		else if (pdrv->config.control.mipi_cfg.lane_num == 3)
			lane = 0x1e;
		else
			lane = 0x1f;

		lcd_hiu_setb(HHI_MIPI_CNTL2, lane, 11, 5);
	} else {
		lcd_hiu_setb(HHI_MIPI_CNTL0, 0, 16, 10);
		lcd_hiu_setb(HHI_MIPI_CNTL0, 0, 31, 1);
		lcd_hiu_setb(HHI_MIPI_CNTL0, 0, 0, 16);
		lcd_hiu_write(HHI_MIPI_CNTL1, 0x6);
		lcd_hiu_write(HHI_MIPI_CNTL2, 0x00200000);
	}
}

struct lcd_phy_ctrl_s lcd_phy_ctrl_axg = {
	.ctrl_bit_on = 0,
	.lane_lock = 0,
	.phy_vswing_level_to_val = NULL,
	.phy_amp_dft_val = NULL,
	.phy_preem_level_to_val = NULL,
	.phy_set_lvds = NULL,
	.phy_set_vx1 = NULL,
	.phy_set_mlvds = NULL,
	.phy_set_p2p = NULL,
	.phy_set_mipi = lcd_mipi_phy_set_axg,
	.phy_set_edp = NULL,
};

struct lcd_phy_ctrl_s *lcd_phy_config_init_axg(struct aml_lcd_drv_s *pdrv)
{
	phy_ctrl_p = &lcd_phy_ctrl_axg;
	return phy_ctrl_p;
}
