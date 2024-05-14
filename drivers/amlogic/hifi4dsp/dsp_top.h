/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DSP_TOP_H
#define _DSP_TOP_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/irqreturn.h>

#include "hifi4dsp_priv.h"
#include "hifi4dsp_firmware.h"
#include "hifi4dsp_dsp.h"

#define SMC_HIFI_DSP_CMD		0x82000090
#define SMC_SUBID_HIFI_DSP_BOOT		0x10
#define SMC_SUBID_HIFI_DSP_REMAP	0x11
#define SMC_SUBID_HIFI_DSP_PWRCTRL	0x12
#define SMC_SUBID_SHIFT			0x8
#define PACK_SMC_SUBID_ID(subid, id) (((subid) << SMC_SUBID_SHIFT) | (id))

void soc_dsp_bootup(int dsp_id, u32 reset_addr, int freq_sel);
void soc_dsp_regs_iomem_init(void);
void soc_dsp_hw_init(int dsp_id, int freq_sel);
void soc_dsp_top_regs_dump(int dsp_id);
void soc_dsp_poweroff(int dsp_id);

#endif /*_DSP_TOP_H*/
