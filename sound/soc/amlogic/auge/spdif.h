/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_SPDIF_H__
#define __AML_SPDIF_H__
#include <linux/clk.h>

/* For spdifout mask lane register offset V1:
 * EE_AUDIO_SPDIFOUT_CTRL0, offset: 4 - 11
 */
#define SPDIFOUT_LANE_MASK_V1 1
/* For spdifout mask lane register offset V2:
 * EE_AUDIO_SPDIFOUT_CTRL0, offset: 0 - 15
 */
#define SPDIFOUT_LANE_MASK_V2 2

int spdif_set_audio_clk(int id, struct clk *clk_src, int rate, int same);
int spdifout_get_lane_mask_version(int id);
unsigned int spdif_get_codec(void);
unsigned int get_spdif_source_l_config(int id);
#endif
