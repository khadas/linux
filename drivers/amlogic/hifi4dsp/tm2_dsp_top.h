/*
 * drivers/amlogic/hifi4dsp/tm2_dsp_top.h
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

#ifndef _TM2_DSP_TOP_H
#define _TM2_DSP_TOP_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/irqreturn.h>

#include "hifi4dsp_priv.h"
#include "hifi4dsp_firmware.h"
#include "hifi4dsp_dsp.h"

extern void tm2_dsp_bootup(int dsp_id, uint32_t reset_addr, int freq_sel);
extern void tm2_dsp_regs_iomem_init(void);
extern void	tm2_dsp_hw_init(int dsp_id, int freq_sel);
extern void tm2_dsp_top_regs_dump(int dsp_id);

#endif /*_TM2_DSP_TOP_H*/
