/*
 * tca6408 GPIO platform support
 *
 * Copyright (C) 2019 Texas Instruments
 *
 * Author: waylon <wesion@khadas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _KHADAS_TCA6408_H
#define _KHADAS_TCA6408_H

#define TCA_LCD_RESET_MASK  (1<<0)
#define TCA_LCD_EN_MASK     (1<<1)
#define TCA_CAM_RESET_MASK  (1<<2)
#define TCA_CAM_PDN0_MASK   (1<<3)
#define TCA_CAM_PDN1_MASK   (1<<4)
#define TCA_RED_LED_MASK    (1<<5)
#define TCA_TP_RST_MASK     (1<<6)
#define TCA_EXPIO_P7_MASK   (1<<7)
#define TCA_ALL_MASK        (0xFF)

#endif

