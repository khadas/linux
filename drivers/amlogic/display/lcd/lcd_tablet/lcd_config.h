/*
 * drivers/amlogic/display/vout/lcd/lcd_config.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

#include <linux/types.h>
#include <linux/amlogic/vout/lcdoutc.h>

/* **********************************
 * lcd driver version
 * ********************************** */
#define LCD_DRV_DATE      "20150326"

/* **********************************
 * lcd config api
 * ********************************** */
extern void lcd_control_assign(struct Lcd_Config_s *pConf);
extern void lcd_control_probe(struct Lcd_Config_s *pConf);
extern void lcd_control_remove(struct Lcd_Config_s *pConf);

extern int dsi_init_table_detect(struct device_node *m_node,
		struct DSI_Config_s *mConf, int on_off);
extern int creat_dsi_attr(struct class *debug_class);
extern int remove_dsi_attr(struct class *debug_class);

extern void edp_link_config_select(struct Lcd_Config_s *pConf);
extern int creat_edp_attr(struct class *debug_class);
extern int remove_edp_attr(struct class *debug_class);
#endif
