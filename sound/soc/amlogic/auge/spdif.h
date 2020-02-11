/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_SPDIF_H__
#define __AML_SPDIF_H__
#include <linux/clk.h>

int spdif_set_audio_clk(int id, struct clk *clk_src, int rate, int same);

#endif
