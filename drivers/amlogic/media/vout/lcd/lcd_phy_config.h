/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_PHY_CONFIG_H__
#define __AML_LCD_PHY_CONFIG_H__

struct lcd_phy_ctrl_s {
	void (*phy_set_lvds)(struct lcd_config_s *pconf, int status);
	void (*phy_set_vx1)(struct lcd_config_s *pconf, int status);
	void (*phy_set_mlvds)(struct lcd_config_s *pconf, int status);
	void (*phy_set_p2p)(struct lcd_config_s *pconf, int status);
	void (*phy_set_mipi)(struct lcd_config_s *pconf, int status);
};

/* -------------------------- */
/* lvsd phy parameters define */
/* -------------------------- */
#define LVDS_PHY_CNTL1_G9TV    0x606cca80
#define LVDS_PHY_CNTL2_G9TV    0x0000006c
#define LVDS_PHY_CNTL3_G9TV    0x00000800

#define LVDS_PHY_CNTL1_TXHD    0x6c60ca80
#define LVDS_PHY_CNTL2_TXHD    0x00000070
#define LVDS_PHY_CNTL3_TXHD    0x03ff0c00
/* -------------------------- */

/* -------------------------- */
/* vbyone phy parameters define */
/* -------------------------- */
#define VX1_PHY_CNTL1_G9TV            0x6e0ec900
#define VX1_PHY_CNTL1_G9TV_PULLUP     0x6e0f4d00
#define VX1_PHY_CNTL2_G9TV            0x0000007c
#define VX1_PHY_CNTL3_G9TV            0x00ff0800
/* -------------------------- */

/* -------------------------- */
/* minilvds phy parameters define */
/* -------------------------- */
#define MLVDS_PHY_CNTL1_TXHD   0x6c60ca80
#define MLVDS_PHY_CNTL2_TXHD   0x00000070
#define MLVDS_PHY_CNTL3_TXHD   0x03ff0c00
/* -------------------------- */

/* ******** MIPI_DSI_PHY ******** */
/* bit[15:11] */
#define MIPI_PHY_LANE_BIT        11
#define MIPI_PHY_LANE_WIDTH      5

/* MIPI-DSI */
#define DSI_LANE_0              BIT(4)
#define DSI_LANE_1              BIT(3)
#define DSI_LANE_CLK            BIT(2)
#define DSI_LANE_2              BIT(1)
#define DSI_LANE_3              BIT(0)
#define DSI_LANE_COUNT_1        (DSI_LANE_CLK | DSI_LANE_0)
#define DSI_LANE_COUNT_2        (DSI_LANE_CLK | DSI_LANE_0 | DSI_LANE_1)
#define DSI_LANE_COUNT_3        (DSI_LANE_CLK | DSI_LANE_0 |\
					DSI_LANE_1 | DSI_LANE_2)
#define DSI_LANE_COUNT_4        (DSI_LANE_CLK | DSI_LANE_0 |\
					DSI_LANE_1 | DSI_LANE_2 | DSI_LANE_3)

static unsigned int lvds_vx1_p2p_phy_preem_tl1[] = {
	0x06020602,
	0x26022602,
	0x46024602,
	0x66026602,
	0x86028602,
	0xa602a602,
	0xf602f602,
};

static unsigned int p2p_low_common_phy_preem_tl1[] = {
	0x070b070b,
	0x170b170b,
	0x370b370b,
	0x770b770b,
	0xf70bf70b,
	0xff0bff0b,
};

#endif
