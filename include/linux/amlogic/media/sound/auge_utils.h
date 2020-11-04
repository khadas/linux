/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __AUGE_UTILS_H__
#define __AUGE_UTILS_H__

void auge_acodec_reset(void);
void auge_toacodec_ctrl(int tdmout_id);
void auge_toacodec_ctrl_ext(int tdmout_id, int ch0_sel, int ch1_sel, bool separate_toacodec_en);
#endif
