/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AML_LCD_TABLET_H__
#define __AML_LCD_TABLET_H__

void lcd_tablet_config_update(struct lcd_config_s *pconf);
void lcd_tablet_config_post_update(struct lcd_config_s *pconf);
void lcd_tablet_driver_init_pre(void);
void lcd_tablet_driver_disable_post(void);
int lcd_tablet_driver_init(void);
void lcd_tablet_driver_disable(void);

#endif
