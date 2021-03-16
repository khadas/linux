/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_TV_H__
#define __AML_LCD_TV_H__
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

void lcd_tv_config_update(struct aml_lcd_drv_s *pdrv);
void lcd_tv_driver_init_pre(struct aml_lcd_drv_s *pdrv);
void lcd_tv_driver_disable_post(struct aml_lcd_drv_s *pdrv);
int lcd_tv_driver_init(struct aml_lcd_drv_s *pdrv);
void lcd_tv_driver_disable(struct aml_lcd_drv_s *pdrv);
int lcd_tv_driver_change(struct aml_lcd_drv_s *pdrv);

void lcd_vbyone_wait_stable(struct aml_lcd_drv_s *pdrv);
int lcd_vbyone_interrupt_up(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_interrupt_down(struct aml_lcd_drv_s *pdrv);

#endif
