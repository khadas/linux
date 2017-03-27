/*
 * drivers/amlogic/amports/arch/regs/vpp_dolbyvision_regs.h
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
#ifndef VPP_DOLBYVISION_REGS_H
#define VPP_DOLBYVISION_REGS_H

#define DOLBY_CORE1_REG_START		0x3300
#define DOLBY_CORE1_CLKGATE_CTRL	0x33f2
#define DOLBY_CORE1_SWAP_CTRL0		0x33f3
#define DOLBY_CORE1_SWAP_CTRL1		0x33f4
#define DOLBY_CORE1_SWAP_CTRL2		0x33f5
#define DOLBY_CORE1_SWAP_CTRL3		0x33f6
#define DOLBY_CORE1_SWAP_CTRL4		0x33f7
#define DOLBY_CORE1_SWAP_CTRL5		0x33f8
#define DOLBY_CORE1_DMA_CTRL		0x33f9
#define DOLBY_CORE1_DMA_STATUS		0x33fa
#define DOLBY_CORE1_STATUS0			0x33fb
#define DOLBY_CORE1_STATUS1			0x33fc
#define DOLBY_CORE1_STATUS2			0x33fd
#define DOLBY_CORE1_STATUS3			0x33fe
#define DOLBY_CORE1_DMA_PORT		0x33ff

#define DOLBY_CORE2A_REG_START		0x3400
#define DOLBY_CORE2A_CTRL			0x3401
#define DOLBY_CORE2A_CLKGATE_CTRL	0x3432
#define DOLBY_CORE2A_SWAP_CTRL0		0x3433
#define DOLBY_CORE2A_SWAP_CTRL1		0x3434
#define DOLBY_CORE2A_SWAP_CTRL2		0x3435
#define DOLBY_CORE2A_SWAP_CTRL3		0x3436
#define DOLBY_CORE2A_SWAP_CTRL4		0x3437
#define DOLBY_CORE2A_SWAP_CTRL5		0x3438
#define DOLBY_CORE2A_DMA_CTRL		0x3439
#define DOLBY_CORE2A_DMA_STATUS		0x343a
#define DOLBY_CORE2A_STATUS0		0x343b
#define DOLBY_CORE2A_STATUS1		0x343c
#define DOLBY_CORE2A_STATUS2		0x343d
#define DOLBY_CORE2A_STATUS3		0x343e
#define DOLBY_CORE2A_DMA_PORT		0x343f

#define DOLBY_CORE3_REG_START		0x3600
#define DOLBY_CORE3_CLKGATE_CTRL	0x36f0
#define DOLBY_CORE3_SWAP_CTRL0		0x36f1
#define DOLBY_CORE3_SWAP_CTRL1		0x36f2
#define DOLBY_CORE3_SWAP_CTRL2		0x36f3
#define DOLBY_CORE3_SWAP_CTRL3		0x36f4
#define DOLBY_CORE3_SWAP_CTRL4		0x36f5
#define DOLBY_CORE3_SWAP_CTRL5		0x36f6
#define DOLBY_CORE3_SWAP_CTRL6		0x36f7
#define DOLBY_CORE3_CRC_CTRL		0x36fb
#define DOLBY_CORE3_INPUT_CSC_CRC	0x36fc
#define DOLBY_CORE3_OUTPUT_CSC_CRC	0x36fd

#define VIU_MISC_CTRL1				0x1a07
#define VIU_OSD1_CTRL_STAT			0x1a10
#define VPP_DOLBY_CTRL				0x1d93
#define VIU_SW_RESET				0x1a01
#define VPU_HDMI_FMT_CTRL			0x2743

#define DOLBY_TV_REG_START			0x3300
#define DOLBY_TV_CLKGATE_CTRL		0x33f1
#define DOLBY_TV_SWAP_CTRL0			0x33f2
#define DOLBY_TV_SWAP_CTRL1			0x33f3
#define DOLBY_TV_SWAP_CTRL2			0x33f4
#define DOLBY_TV_SWAP_CTRL3			0x33f5
#define DOLBY_TV_SWAP_CTRL4			0x33f6
#define DOLBY_TV_SWAP_CTRL5			0x33f7
#define DOLBY_TV_SWAP_CTRL6			0x33f8
#define DOLBY_TV_SWAP_CTRL7			0x33f9
#define DOLBY_TV_DMA_CTRL			0x33f9
#define DOLBY_TV_AXI2DMA_CTRL0		0x33fa
#define DOLBY_TV_AXI2DMA_CTRL1		0x33fb
#define DOLBY_TV_AXI2DMA_CTRL2		0x33fc
#define DOLBY_TV_AXI2DMA_CTRL3		0x33fd
#define DOLBY_TV_STATUS0			0x33fe
#define DOLBY_TV_STATUS1			0x33ff
#endif
