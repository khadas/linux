/*
 * Copyright (c) 2015 Amlogic, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for Meson8 clock controller.
*/

#ifndef _DT_BINDINGS_CLOCK_MESON_8_H
#define _DT_BINDINGS_CLOCK_MESON_8_H

#define CLK_XTAL		2
/* core clocks */
#define CLK_FIXED_PLL 1
#define CLK_DDR_PLL 3
#define CLK_G_PLL 4
#define CLK_VID_PLL 5
#define CLK_HDMITX_PLL 6
#define CLK_FPLL_DIV2 7
#define CLK_FPLL_DIV3 8
#define CLK_FPLL_DIV4 9
#define CLK_81       10
#define CLK_HDMITX_SYS 11
#define CLK_HDMITX_ENCP 12
#define CLK_HDMITX_ENCI 13
#define CLK_HDMITX_PIXEL 14
#define CLK_HDMITX_PHY 15
#define CLK_SYS_PLL 16
#define CLK_FPLL_DIV5   17
#define CLK_FPLL_DIV7   18
#define CLK_GPU_0       19
#define CLK_GPU_1       20
#define CLK_GPU         21
/* must be greater than maximal clock id */
#define CLK_NR_CLKS		100
#define CLK_MALI_0      CLK_GPU_0
#define CLK_MALI_1      CLK_GPU_1
#define CLK_MALI        CLK_GPU

#endif /* _DT_BINDINGS_CLOCK_MESON_8_H */
