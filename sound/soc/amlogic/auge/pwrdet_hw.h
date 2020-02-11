/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __POWER_DETECT_HW_H__
#define __POWER_DETECT_HW_H__
#include <linux/types.h>

#include "regs.h"
#include "iomap.h"

void pwrdet_threshold(int shift, int hi_th, int lo_th);
void pwrdet_downsample_ctrl(bool dw_en, bool fast_en,
			    int dw_sel, int fast_sel);
void aml_pwrdet_format_set(int toddr_type, int msb, int lsb);
void pwrdet_src_select(bool enable, int src);
#endif
