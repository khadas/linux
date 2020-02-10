/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_TV_H__
#define __AML_LCD_TV_H__
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

void lcd_tv_config_update(struct lcd_config_s *pconf);
void lcd_tv_driver_init_pre(void);
void lcd_tv_driver_disable_post(void);
int lcd_tv_driver_init(void);
void lcd_tv_driver_disable(void);
int lcd_tv_driver_change(void);

void lcd_vbyone_wait_stable(void);
int lcd_vbyone_interrupt_up(void);
void lcd_vbyone_interrupt_down(void);

#endif
