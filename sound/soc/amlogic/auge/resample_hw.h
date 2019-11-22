/*
 * sound/soc/amlogic/auge/resample_hw.h
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
#ifndef __AML_AUDIO_RESAMPLE_HW_H__
#define __AML_AUDIO_RESAMPLE_HW_H__

#include "ddr_mngr.h"

enum samplerate_index {
	RATE_OFF,
	RATE_32K,
	RATE_44K,
	RATE_48K,
	RATE_88K,
	RATE_96K,
	RATE_176K,
	RATE_192K,
	RATE_16K,
	RATE_MAX,
};

#define DEFAULT_SPK_SAMPLERATE 48000
#define DEFAULT_MIC_SAMPLERATE 16000

bool resample_get_status(enum resample_idx id);
void resample_enable(enum resample_idx id, bool enable);
int resample_init(enum resample_idx id, int input_sr);
int resample_set_hw_param(enum resample_idx id,
			  enum samplerate_index rate_index);
void resample_src_select(int src);
void resample_src_select_ab(enum resample_idx id, enum resample_src src);
void resample_format_set(enum resample_idx id, int ch_num, int bits);
int resample_ctrl_read(enum resample_idx id);
void resample_ctrl_write(enum resample_idx id, int value);
int resample_set_hw_pause_thd(enum resample_idx id, unsigned int thd);

void new_resample_set_ram_coeff_aa(enum resample_idx id, int len,
				   unsigned int *params);
void new_resample_set_ram_coeff_sinc(enum resample_idx id, int len,
				     unsigned int *params);
void new_resample_init_param(enum resample_idx id);
void new_resample_enable(enum resample_idx id, bool enable);
void new_resampleA_set_format(enum resample_idx id, int source, int channel);
void new_resampleB_set_format(enum resample_idx id, int output_sr);
void new_resample_set_ratio(enum resample_idx id, int input_sr, int output_sr);
bool new_resample_get_status(enum resample_idx id);
void new_resample_src_select(enum resample_idx id, enum resample_src src);

#endif
