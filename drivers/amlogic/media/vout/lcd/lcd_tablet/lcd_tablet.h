/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_TABLET_H__
#define __AML_LCD_TABLET_H__
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

void lcd_tablet_config_post_update(struct aml_lcd_drv_s *pdrv);
void lcd_tablet_driver_init_pre(struct aml_lcd_drv_s *pdrv);
void lcd_tablet_driver_disable_post(struct aml_lcd_drv_s *pdrv);
int lcd_tablet_driver_init(struct aml_lcd_drv_s *pdrv);
void lcd_tablet_driver_disable(struct aml_lcd_drv_s *pdrv);

void mipi_dsi_link_off(struct aml_lcd_drv_s *pdrv);
void mipi_dsi_tx_ctrl(struct aml_lcd_drv_s *pdrv, int status);
void edp_tx_ctrl(struct aml_lcd_drv_s *pdrv, int flag);
int DPCD_capability_detect(struct aml_lcd_drv_s *pdrv);
void dptx_uboot_config_load(struct aml_lcd_drv_s *pdrv);
void dptx_load_uboot_edid_timing(struct aml_lcd_drv_s *pdrv);

#endif
