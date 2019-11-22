/*
 * drivers/amlogic/clk/tm2/tm2.h
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

#ifndef __TM2_H
#define __TM2_H

/*
 * Clock controller register offsets
 *
 * Register offsets from the data sheet are listed in comment blocks below.
 * Those offsets must be multiplied by 4 before adding them to the base address
 * to get the right value
 */
#define HHI_PCIE_PLL_CNTL0	0x94  /* 0x25 offset in data sheet */
#define HHI_PCIE_PLL_CNTL1	0x98  /* 0x26 offset in data sheet */
#define HHI_PCIE_PLL_CNTL2	0xb0  /* 0x2c offset in data sheet */
#define HHI_PCIE_PLL_CNTL3	0xb4  /* 0x2d offset in data sheet */
#define HHI_PCIE_PLL_CNTL4	0x168 /* 0x5a offset in data sheet */
#define HHI_PCIE_PLL_CNTL5	0x16c /* 0x5b offset in data sheet */
#define HHI_PCIE_PLL_STS	0x170 /* 0x5c offset in data sheet */
#define HHI_DSP_CLK_CNTL	0x3f0 /* 0xfc offset in data sheet */

#endif /* __TL1_H */
