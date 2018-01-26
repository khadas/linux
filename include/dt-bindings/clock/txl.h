/*
 * Copyright (c) 2016 Amlogic, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for Meson8 clock controller.
*/

#ifndef _DT_BINDINGS_CLOCK_TXL_H
#define _DT_BINDINGS_CLOCK_TXL_H

/* core clocks */
#define CLK_FIXED_PLL		1
#define CLK_XTAL		2
#define CLK_DDR_PLL		3
#define CLK_G_PLL		4
#define CLK_VID_PLL		5
#define CLK_HDMITX_PLL		6
#define CLK_FPLL_DIV2		7
#define CLK_FPLL_DIV3		8
#define CLK_FPLL_DIV4		9
#define CLK_81			10
#define CLK_HDMITX_SYS		11
#define CLK_HDMITX_ENCP		12
#define CLK_HDMITX_ENCI		13
#define CLK_HDMITX_PIXEL	14
#define CLK_HDMITX_PHY		15
#define CLK_SYS_PLL		16
#define CLK_FPLL_DIV5		17
#define CLK_FPLL_DIV7		18
#define CLK_GPU_0		19
#define CLK_GPU_1		20
#define CLK_GPU			21
#define CLK_VID			22
#define CLK_VAPB_0		23
#define CLK_VAPB_1		24
#define CLK_GE2D		25
#define CLK_CAMERA_12M		26
#define CLK_CAMERA_24M		27

#define GP0_PLL			30

#define CLK_MPLL0		31
#define CLK_MPLL1		32
#define CLK_MPLL2		33
#define CLK_AMCLK		34
#define CLK_PDM			35
#define CLK_I958		36
#define CLK_SPDIF		37
#define CLK_BT656_CLK0		38
#define CLK_BT656_CLK1		39
#define CLK_VID_LOCK_CLK	40
#define CLK_APB_P		41
/* hdmirx */
#define CLK_HDMIRX_MODET_CLK	42
#define CLK_HDMIRX_CFG_CLK		43
#define CLK_HDMIRX_ACR_REF_CLK	44
#define CLK_HDMIRX_AUDMEAS_CLK	45
#define CLK_VDIN_MEAS_CLK	46
/* pcm */
#define CLK_PCM_MCLK        47
#define CLK_PCM_SCLK        48
/* must be greater than maximal clock id */
#define CLK_NR_CLKS		100
#define CLK_MALI_0		CLK_GPU_0
#define CLK_MALI_1		CLK_GPU_1
#define CLK_MALI		CLK_GPU

#endif /* _DT_BINDINGS_CLOCK_TXL_H */
