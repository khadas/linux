/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef __VAD_HW_H__
#define __VAD_HW_H__
#include <linux/types.h>

#include "regs.h"
#include "iomap.h"

void vad_set_ram_coeff(int len, int *params);
void vad_set_de_params(int len, int *params);
void vad_set_pwd(void);
void vad_set_cep(void);
void vad_set_src(int src, bool vad_top);
void vad_set_in(void);
void vad_set_enable(bool enable, bool vad_top);
void vad_force_clk_to_oscin(bool force, bool vad_top);
void vad_set_two_channel_en(bool en);

#endif
