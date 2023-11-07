/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_PHY_CONFIG_H__
#define __AML_LCD_PHY_CONFIG_H__
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

extern unsigned short lvds_lane_map_flag_6lane_map0[2][3];
// extern lvds_lane_map_flag_5lane = lvds_lane_map_flag_6lane_map1
extern unsigned short lvds_lane_map_flag_6lane_map1[2][3];

struct lcd_phy_ctrl_s {
	unsigned int lane_lock;
	unsigned int ctrl_bit_on;

	unsigned int (*phy_vswing_level_to_val)(struct aml_lcd_drv_s *pdrv, unsigned int level);
	unsigned int (*phy_amp_dft_val)(struct aml_lcd_drv_s *pdrv);
	unsigned int (*phy_preem_level_to_val)(struct aml_lcd_drv_s *pdrv, unsigned int level);

	void (*phy_set_lvds)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_vx1)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_mlvds)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_p2p)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_mipi)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_edp)(struct aml_lcd_drv_s *pdrv, int status);
};

struct lcd_phy_ctrl_s *lcd_phy_config_init_axg(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_g12(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t3(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t5(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t5w(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t7(struct aml_lcd_drv_s *pdrv);
struct lcd_phy_ctrl_s *lcd_phy_config_init_tl1(struct aml_lcd_drv_s *pdrv);

unsigned int lcd_phy_vswing_level_to_value_dft(struct aml_lcd_drv_s *pdrv, unsigned int level);
unsigned int lcd_phy_preem_level_to_value_dft(struct aml_lcd_drv_s *pdrv, unsigned int level);
unsigned int lcd_phy_preem_level_to_value_legacy(struct aml_lcd_drv_s *pdrv, unsigned int level);

#endif
