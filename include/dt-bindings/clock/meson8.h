/*
 * Copyright (c) 2015 Amlogic, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#ifndef _DT_BINDINGS_CLOCK_MESON_8_H
#define _DT_BINDINGS_CLOCK_MESON_8_H

#define CLK_XTAL		2
/* core clocks */
#define CLK_FIXED_PLL 1
#define CLK_SYS_PLL 2
#define CLK_DDR_PLL 3
#define CLK_G_PLL 4
#define CLK_VID_PLL 5
#define CLK_HDMI_PLL 6

#define CLK_UART_AO 10


/* must be greater than maximal clock id */
#define CLK_NR_CLKS		37

#endif /* _DT_BINDINGS_CLOCK_MESON_8_H */
