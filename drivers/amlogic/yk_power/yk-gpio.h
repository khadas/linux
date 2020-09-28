/*
 * gpio.h  --  GPIO Driver for YEKER
 *
 * Copyright (C) 2013 YEKER, Ltd.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef _LINUX_YK_GPIO_H_
#define _LINUX_YK_GPIO_H_

/*    YK   */
#define YK_GPIO0_CFG                   (YK_GPIO0_CTL)//0x90
#define YK_GPIO1_CFG                   (YK_GPIO1_CTL)//0x92
#define YK_GPIO2_CFG                   (YK_LDO_DC_EN2)//0x12
#define YK_GPIO3_CFG                   (YK_OFF_CTL)//0x32
#define YK_GPIO4_CFG                   (YK_HOTOVER_CTL)//0x8f
#define YK_GPIO4_STA                   (YK_IPS_SET)//0x30

#define YK_GPIO01_STATE               (YK_GPIO01_SIGNAL)

extern int yk_gpio_set_io(int gpio, int io_state);
extern int yk_gpio_get_io(int gpio, int *io_state);
extern int yk_gpio_set_value(int gpio, int value);
extern int yk_gpio_get_value(int gpio, int *value);
#endif
