/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_PHY_CONFIG_H__
#define __AML_LCD_PHY_CONFIG_H__
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

struct lcd_phy_ctrl_s {
	unsigned int lane_lock;
	unsigned int ctrl_bit_on;

	unsigned int (*phy_vswing_level_to_val)(struct aml_lcd_drv_s *pdrv, unsigned int level);
	unsigned int (*phy_preem_level_to_val)(struct aml_lcd_drv_s *pdrv, unsigned int level);

	void (*phy_set_lvds)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_vx1)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_mlvds)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_p2p)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_mipi)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_edp)(struct aml_lcd_drv_s *pdrv, int status);
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
#define DSI_LANE_COUNT_3        (DSI_LANE_CLK | DSI_LANE_0 | DSI_LANE_1 | DSI_LANE_2)
#define DSI_LANE_COUNT_4        (DSI_LANE_CLK | DSI_LANE_0 | DSI_LANE_1 | DSI_LANE_2 | DSI_LANE_3)

#define DSI_LANE_0_G12A         BIT(4)
#define DSI_LANE_CLK_G12A       BIT(3)
#define DSI_LANE_1_G12A         BIT(2)
#define DSI_LANE_2_G12A         BIT(1)
#define DSI_LANE_3_G12A         BIT(0)
#define DSI_LANE_COUNT_1_G12A   (DSI_LANE_CLK | DSI_LANE_0)
#define DSI_LANE_COUNT_2_G12A   (DSI_LANE_CLK | DSI_LANE_0 | DSI_LANE_1)
#define DSI_LANE_COUNT_3_G12A   (DSI_LANE_CLK | DSI_LANE_0 | DSI_LANE_1 | DSI_LANE_2)
#define DSI_LANE_COUNT_4_G12A   (DSI_LANE_CLK | DSI_LANE_0 | DSI_LANE_1 | DSI_LANE_2 | DSI_LANE_3)

struct lcd_phy_ctrl_s *lcd_phy_config_init_axg(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_g12(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t3(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t5(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t5w(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t7(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_tl1(struct aml_lcd_drv_s *pdrv);

unsigned int lcd_phy_vswing_level_to_value_dft(struct aml_lcd_drv_s *pdrv, unsigned int level);
unsigned int lcd_phy_preem_level_to_value_dft(struct aml_lcd_drv_s *pdrv, unsigned int level);

#endif
