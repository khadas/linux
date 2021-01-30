/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AUGE_UTILS_H__
#define __AUGE_UTILS_H__

#define DATA_SEL_SHIFT_VERSION0 (0)
#define DATA_SEL_SHIFT_VERSION1 (1)

void auge_acodec_reset(void);
void auge_toacodec_ctrl(int tdmout_id);
void auge_toacodec_ctrl_ext(int tdmout_id, int ch0_sel, int ch1_sel,
	bool separate_toacodec_en, int data_sel_shift);

#if ((defined CONFIG_AMLOGIC_SND_SOC_AUGE ||\
		defined CONFIG_AMLOGIC_SND_SOC_AUGE_MODULE))
void fratv_src_select(bool src);
void fratv_LR_swap(bool swap);
#else
static inline __maybe_unused void fratv_src_select(bool src)
{
}

static inline __maybe_unused void fratv_LR_swap(bool swap)
{
}
#endif

#endif
