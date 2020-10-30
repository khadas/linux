/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _LUT_DMA_IO_H_
#define _LUT_DMA_IO_H_
#include <linux/amlogic/iomap.h>

u32 lut_dma_reg_read(u32 reg);
void lut_dma_reg_write(u32 reg, const u32 val);
void lut_dma_reg_set_mask(u32 reg,
			  const u32 mask);
void lut_dma_reg_clr_mask(u32 reg,
			  const u32 mask);
void lut_dma_reg_set_bits(u32 reg,
			  const u32 value,
			  const u32 start,
			  const u32 len);
#endif
