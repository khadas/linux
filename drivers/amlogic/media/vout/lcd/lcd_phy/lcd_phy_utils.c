// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_phy_config.h"

// color depth: 6bit, 8bit, 10bit
// normal     :
// port_swap  :
unsigned short lvds_lane_map_flag_6lane_map0[2][3] = {{0x000f, 0x001f, 0x003f},
						      {0x03c0, 0x07c0, 0x0fc0}};
// lvds_lane_map_flag_5lane = lvds_lane_map_flag_6lane_map1
unsigned short lvds_lane_map_flag_6lane_map1[2][3] = {{0x000f, 0x001f, 0x041f},
						      {0x01e0, 0x03e0, 0x0be0}};

unsigned int lcd_phy_vswing_level_to_value_dft(struct aml_lcd_drv_s *pdrv, unsigned int level)
{
	unsigned int vswing_value = 0;

	vswing_value = level;

	return vswing_value;
}

unsigned int lcd_phy_preem_level_to_value_dft(struct aml_lcd_drv_s *pdrv, unsigned int level)
{
	unsigned int preem_value = 0;

	preem_value = level;

	return preem_value;
}

unsigned int lcd_phy_preem_level_to_value_legacy(struct aml_lcd_drv_s *pdrv, unsigned int level)
{
	unsigned int p2p_type;
	unsigned int lvds_vx1_p2p_phy_preem_tl1[7] = {0x06, 0x26, 0x46, 0x66, 0x86, 0xa6, 0xf6};
	unsigned int p2p_low_common_phy_preem_tl1[6] = {0x07, 0x17, 0x37, 0x77, 0xf7, 0xff};

	switch (pdrv->config.basic.lcd_type) {
	case LCD_LVDS:
	case LCD_VBYONE:
	case LCD_MLVDS:
		if (level < 7)
			return lvds_vx1_p2p_phy_preem_tl1[level];
		break;
	case LCD_P2P:
		p2p_type = pdrv->config.control.p2p_cfg.p2p_type & 0x1f;
		switch (p2p_type) {
		case P2P_CEDS:
		case P2P_CMPI:
		case P2P_ISP:
		case P2P_EPI:
			if (level < 7)
				return lvds_vx1_p2p_phy_preem_tl1[level];
			break;
		case P2P_CHPI: /* low common mode */
		case P2P_CSPI:
		case P2P_USIT:
			if (level < 6)
				return p2p_low_common_phy_preem_tl1[level];
			break;
		default:
			return 0;
		}
		break;
	default:
		return 0;
	}
	LCDERR("[%d]: %s: level %d invalid\n", pdrv->index, __func__, level);
	return 0;
}
