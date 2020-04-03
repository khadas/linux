/*
 * drivers/amlogic/clk/tl1/tl1.h
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved.
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

#ifndef __TM2_H
#define __TM2_H

/*
 * Clock controller register offsets
 *
 * Register offsets from the data sheet are listed in comment blocks below.
 * Those offsets must be multiplied by 4 before adding them to the base address
 * to get the right value
 */
#define HHI_GP0_PLL_CNTL0		0x40 /* 0x10 offset in datasheet very*/
#define HHI_GP0_PLL_CNTL1		0x44 /* 0x11 offset in datasheet */
#define HHI_GP0_PLL_CNTL2		0x48 /* 0x12 offset in datasheet */
#define HHI_GP0_PLL_CNTL3		0x4c /* 0x13 offset in datasheet */
#define HHI_GP0_PLL_CNTL4		0x50 /* 0x14 offset in datasheet */
#define HHI_GP0_PLL_CNTL5		0x54 /* 0x15 offset in datasheet */
#define HHI_GP0_PLL_CNTL6		0x58 /* 0x16 offset in datasheet */
#define HHI_GP0_PLL_STS			0x5c /* 0x17 offset in datasheet */
#define HHI_GP1_PLL_CNTL0		0x60 /* 0x18 offset in datasheet */
#define HHI_GP1_PLL_CNTL1		0x64 /* 0x19 offset in datasheet */
#define HHI_GP1_PLL_CNTL2		0x68 /* 0x1a offset in datasheet */
#define HHI_GP1_PLL_CNTL3		0x6c /* 0x1b offset in datasheet */
#define HHI_GP1_PLL_CNTL4		0x70 /* 0x1c offset in datasheet */
#define HHI_GP1_PLL_CNTL5		0x74 /* 0x1d offset in datasheet */
#define HHI_GP1_PLL_CNTL6		0x78 /* 0x1e offset in datasheet */
#define HHI_GP1_PLL_STS			0x7c /* 0x1f offset in datasheet very*/

#define HHI_PCIE_PLL_CNTL0		0x94  /* 0x25 offset in data sheet */
#define HHI_PCIE_PLL_CNTL1		0x98  /* 0x26 offset in data sheet */
#define HHI_PCIE_PLL_CNTL2		0xb0  /* 0x2c offset in data sheet */
#define HHI_PCIE_PLL_CNTL3		0xb4  /* 0x2d offset in data sheet */
#define HHI_PCIE_PLL_CNTL4		0x168 /* 0x5a offset in data sheet */
#define HHI_PCIE_PLL_CNTL5		0x16c /* 0x5b offset in data sheet */
#define HHI_PCIE_PLL_STS		0x170 /* 0x5c offset in data sheet */

#define HHI_HIFI_PLL_CNTL0		0xd8 /* 0x36 offset in datasheet very*/
#define HHI_HIFI_PLL_CNTL1		0xdc /* 0x37 offset in datasheet */
#define HHI_HIFI_PLL_CNTL2		0xe0 /* 0x38 offset in datasheet */
#define HHI_HIFI_PLL_CNTL3		0xe4 /* 0x39 offset in datasheet */
#define HHI_HIFI_PLL_CNTL4		0xe8 /* 0x3a offset in datasheet */
#define HHI_HIFI_PLL_CNTL5		0xec /* 0x3b offset in datasheet */
#define HHI_HIFI_PLL_CNTL6		0xf0 /* 0x3c offset in datasheet */
#define HHI_HIFI_PLL_STS		0xf4 /* 0x3d offset in datasheet very*/

#define HHI_GCLK_MPEG0			0xc0 /* 0x30 offset in datasheet */
#define HHI_GCLK_MPEG1			0xc4 /* 0x31 offset in datasheet */
#define HHI_GCLK_MPEG2			0xc8 /* 0x32 offset in datasheet */
#define HHI_GCLK_OTHER			0xd0 /* 0x34 offset in datasheet */

#define HHI_GCLK_SP_MPEG		0x154 /* 0x55 offset in datasheet */
#define HHI_SYS_CPU_CLK_CNTL1		0x15C /* 0x57 offset in datasheet1 */
#define HHI_VID_CLK_DIV			0x164 /* 0x59 offset in datasheet */
#define HHI_SPICC_HCLK_CNTL		0x168 /* 0x5a offset in datasheet */

#define HHI_MPEG_CLK_CNTL		0x174 /* 0x5d offset in datasheet */
#define HHI_VID_CLK_CNTL		0x17c /* 0x5f offset in datasheet */
#define HHI_TS_CLK_CNTL			0x190 /* 0x64 offset in datasheet1 */
#define HHI_VID_CLK_CNTL2		0x194 /* 0x65 offset in datasheet */
#define HHI_SYS_CPU_CLK_CNTL0		0x19c /* 0x67 offset in datasheet */
#define HHI_AUD_CLK_CNTL3		0x1A4 /* 0x69 offset in datasheet */
#define HHI_AUD_CLK_CNTL4		0x1A8 /* 0x6a offset in datasheet */
#define HHI_MALI_CLK_CNTL		0x1b0 /* 0x6c offset in datasheet */
#define HHI_VPU_CLKC_CNTL		0x1b4 /* 0x6d offset in datasheet1 */
#define HHI_VPU_CLK_CNTL		0x1bC /* 0x6f offset in datasheet1 */
#define HHI_AUDPLL_CLK_OUT_CNTL		0x1E0 /* 0x74 offset in datasheet1 */
#define HHI_VDEC_CLK_CNTL		0x1E0 /* 0x78 offset in datasheet1 */
#define HHI_VDEC2_CLK_CNTL		0x1E4 /* 0x79 offset in datasheet1 */
#define HHI_VDEC3_CLK_CNTL		0x1E8 /* 0x7a offset in datasheet1 */
#define HHI_VDEC4_CLK_CNTL		0x1EC /* 0x7b offset in datasheet1 */
#define HHI_HDCP22_CLK_CNTL		0x1F0 /* 0x7c offset in datasheet1 */
#define HHI_VAPBCLK_CNTL		0x1F4 /* 0x7d offset in datasheet1 */
#define HHI_HDMIRX_CLK_CNTL		0x200 /* 0x80 offset in datasheet1 */
#define HHI_HDMIRX_AUD_CLK_CNTL		0x204 /* 0x81 offset in datasheet1 */
#define HHI_VPU_CLKB_CNTL		0x20C /* 0x83 offset in datasheet1 */
#define HHI_SYS_CPU_CLK_CNTL5		0x21C /* 0x87 offset in datasheet1 */
#define HHI_SYS_CPU_CLK_CNTL6		0x220 /* 0x88 offset in datasheet1 */
#define HHI_HDMIRX_METER_CLK_CNTL	0x234 /* 0x8d offset in datasheet1 */

#define HHI_VDIN_MEAS_CLK_CNTL		0x250 /* 0x94 offset in datasheet1 */
#define HHI_NAND_CLK_CNTL		0x25C /* 0x97 offset in datasheet1*/
#define HHI_SD_EMMC_CLK_CNTL		0x264 /* 0x99 offset in datasheet1*/
#define HHI_TCON_CLK_CNTL		0x270 /* 0x9c offset in datasheet1*/

#define HHI_MPLL_CNTL0			0x278 /* 0x9e offset in datasheet very*/
#define HHI_MPLL_CNTL1			0x27c /* 0x9f offset in datasheet */
#define HHI_MPLL_CNTL2			0x280 /* 0xa0 offset in datasheet */
#define HHI_MPLL_CNTL3			0x284 /* 0xa1 offset in datasheet */
#define HHI_MPLL_CNTL4			0x288 /* 0xa2 offset in datasheet */
#define HHI_MPLL_CNTL5			0x28c /* 0xa3 offset in datasheet */
#define HHI_MPLL_CNTL6			0x290 /* 0xa4 offset in datasheet */
#define HHI_MPLL_CNTL7			0x294 /* 0xa5 offset in datasheet */
#define HHI_MPLL_CNTL8			0x298 /* 0xa6 offset in datasheet */
#define HHI_MPLL_STS			0x29c /* 0xa7 offset in datasheet very*/

#define HHI_VIID_CLK_DIV		0x128
#define HHI_VIID_CLK_CNTL		0x12C
#define HHI_VID_PLL_CLK_DIV		0x1A0
#define HHI_VIID_CLK_DIV		0x128
#define HHI_VIID_CLK_CNTL		0x12C
#define HHI_HDMI_CLK_CNTL		0x1CC

#define HHI_FIX_PLL_CNTL0		0x2a0 /* 0xa8 offset in datasheet very*/
#define HHI_FIX_PLL_CNTL1		0x2a4 /* 0xa9 offset in datasheet */
#define HHI_FIX_PLL_CNTL2		0x2a8 /* 0xaa offset in datasheet */
#define HHI_FIX_PLL_CNTL3		0x2ac /* 0xab offset in datasheet */
#define HHI_FIX_PLL_CNTL4		0x2b0 /* 0xac offset in datasheet */
#define HHI_FIX_PLL_CNTL5		0x2b4 /* 0xad offset in datasheet */
#define HHI_FIX_PLL_CNTL6		0x2b8 /* 0xae offset in datasheet */
#define HHI_FIX_PLL_STS			0x2bc /* 0xaf offset in datasheet very*/

#define HHI_MPLL3_CNTL0			0x2E0 /* 0xb8 offset in datasheet */
#define HHI_MPLL3_CNTL1			0x2E4 /* 0xb9 offset in datasheet */
#define HHI_PLL_TOP_MISC		0x2E8 /* 0xba offset in datasheet */

#define HHI_ADC_PLL_CNTL		0x2A8 /* 0xaa offset in datasheet */
#define HHI_ADC_PLL_CNTL2		0x2AC /* 0xab offset in datasheet */
#define HHI_ADC_PLL_CNTL3		0x2B0 /* 0xac offset in datasheet */
#define HHI_ADC_PLL_CNTL4		0x2B4 /* 0xad offset in datasheet */
#define HHI_HDMIRX_AXI_CLK_CNTL		0x2E0 /* 0xb8 offset in datasheet */

#define HHI_SYS_PLL_CNTL0		0x2f4 /* 0xbd offset in datasheet */
#define HHI_SYS_PLL_CNTL1		0x2f8 /* 0xbe offset in datasheet */
#define HHI_SYS_PLL_CNTL2		0x2fc /* 0xbf offset in datasheet */
#define HHI_SYS_PLL_CNTL3		0x300 /* 0xc0 offset in datasheet */
#define HHI_SYS_PLL_CNTL4		0x304 /* 0xc1 offset in datasheet */
#define HHI_SYS_PLL_CNTL5		0x308 /* 0xc2 offset in datasheet */
#define HHI_SYS_PLL_CNTL6		0x30c /* 0xc3 offset in datasheet */

#define HHI_HDMI_PLL_CNTL0		0x320 /* 0xc8 offset in datasheet */
#define HHI_HDMI_PLL_CNTL1		0x324 /* 0xc9 offset in datasheet */
#define HHI_HDMI_PLL_CNTL2		0x328 /* 0xca offset in datasheet */
#define HHI_HDMI_PLL_CNTL3		0x32C /* 0xcb offset in datasheet */
#define HHI_HDMI_PLL_CNTL4		0x330 /* 0xcc offset in datasheet */
#define HHI_HDMI_PLL_CNTL5		0x334 /* 0xcd offset in datasheet */
#define HHI_HDMI_PLL_CNTL6		0x338 /* 0xce offset in datasheet */
#define HHI_HDMI_PLL_STS		0x33c /* 0xcf offset in datasheet */

#define HHI_VID_LOCK_CLK_CNTL		0x3c8 /* 0xf2 offset in datasheet1 */
#define HHI_BT656_CLK_CNTL		0x3d4 /* 0xf5 offset in datasheet1 */
#define HHI_SPICC_CLK_CNTL		0x3dc /* 0xf7 offset in datasheet1 */

#define HHI_DSP_CLK_CNTL		0x3f0 /* 0xfc offset in data sheet */
#define HHI_VIPNANOQ_CLK_CNTL		0x1c8 /* 0x72 offset in data sheet */

/* include the CLKIDs that have been made part of the DT binding */
#include <dt-bindings/clock/tm2-clkc.h>

#define NR_CLKS                                 CLKID_END

#endif /* __TM2_H */
