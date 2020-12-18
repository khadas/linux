/*
 * drivers/amlogic/clk/t5/t5_peripheral.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifndef __T5_PER_H
#define __T5_PER_H

/*
 * Peripheral Clock controller register offsets, base: FF646000
 */
#define HHI_CHECK_CLK_RESULT	(0x04 << 2)
#define HHI_VIID_CLK_DIV	(0x4a << 2)
#define HHI_VIID_CLK_CNTL	(0x4b << 2)
#define HHI_VID_CLK_DIV		(0x59 << 2)
#define HHI_MPEG_CLK_CNTL	(0x5d << 2)
#define HHI_VID_CLK_CNTL	(0x5f << 2)
#define HHI_TSIN_DEGLITCH_CLK_CNTL	(0x60 << 2)
#define HHI_TS_CLK_CNTL		(0x64 << 2)
#define HHI_VID_CLK_CNTL2	(0x65 << 2)
#define HHI_MALI_CLK_CNTL	(0x6c << 2)
#define HHI_VPU_CLKC_CNTL	(0x6d << 2)
#define HHI_VPU_CLK_CNTL	(0x6f << 2)
#define HHI_DEMOD_CLK_CNTL	(0x74 << 2)
#define HHI_DEMOD_CLK_CNTL1	(0x75 << 2) /* T5D newly*/
#define HHI_ETH_CLK_CNTL	(0x76 << 2)
#define HHI_VDEC_CLK_CNTL	(0x78 << 2)
#define HHI_VDEC2_CLK_CNTL	(0x79 << 2)
#define HHI_VDEC3_CLK_CNTL	(0x7a << 2)
#define HHI_VDEC4_CLK_CNTL	(0x7b << 2)
#define HHI_HDCP22_CLK_CNTL	(0x7c << 2)
#define HHI_VAPBCLK_CNTL	(0x7d << 2)
#define HHI_HDMIRX_CLK_CNTL	(0x80 << 2)
#define HHI_HDMIRX_AUD_CLK_CNTL	(0x81 << 2)
#define HHI_VPU_CLKB_CNTL	(0x83 << 2)
#define HHI_GEN_CLK_CNTL	(0x8a << 2)
#define HHI_AUDPLL_CLK_OUT_CNTL	(0x8c << 2)
#define HHI_HDMIRX_METER_CLK_CNTL	(0x8d << 2)
#define HHI_VDIN_MEAS_CLK_CNTL	(0x94 << 2)
#define HHI_NAND_CLK_CNTL	(0x97 << 2)
#define HHI_TCON_CLK_CNTL	(0x9c << 2)
#define HHI_HDMI_AXI_CLK_CNTL	(0xb8 << 2)
#define HHI_VID_LOCK_CLK_CNTL	(0xf2 << 2)
#define HHI_ATV_DMD_SYS_CLK_CNTL	(0xf3 << 2)
#define HHI_CDAC_CLK_CNTL	(0xf6 << 2)
#define HHI_SPICC_CLK_CNTL	(0xf7 << 2)

#include <dt-bindings/clock/t5d-periph-clkc.h>
#endif /* __T5_PER_H */
