// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include "pwrdet_hw.h"

void pwrdet_threshold(int shift, int hi_th, int lo_th)
{
	audiobus_write(EE_AUDIO_POW_DET_TH_HI + shift, hi_th);
	audiobus_write(EE_AUDIO_POW_DET_TH_LO + shift, lo_th);
}

void pwrdet_downsample_ctrl(bool dw_en, bool fast_en, int dw_sel, int fast_sel)
{
	audiobus_update_bits(EE_AUDIO_POW_DET_CTRL0,
			     0x1 << 30 | 0xf << 16 | 0x1 << 28 | 0xf << 20,
			     dw_en << 30 | dw_sel << 16 |
			     fast_en << 28 | fast_sel);
}

void aml_pwrdet_format_set(int toddr_type, int msb, int lsb)
{
	audiobus_update_bits(EE_AUDIO_POW_DET_CTRL0,
			     0x7 << 13 | 0x1f << 8 | 0x1f << 3,
			     toddr_type << 13 | msb << 8 | lsb << 3);
}

void pwrdet_src_select(bool enable, int src)
{
	audiobus_update_bits(EE_AUDIO_POW_DET_CTRL0,
			     0x1 << 31 | 0x7 << 0, enable << 31 | src << 0);
}
