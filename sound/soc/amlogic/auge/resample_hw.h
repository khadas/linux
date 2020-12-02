/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
void aml_resample_chsync_enable(enum resample_idx id, bool enable);
void aml_resample_chsync_set(enum resample_idx id, int channel);

#ifdef AA_FILTER_DEBUG
void check_ram_coeff_aa(enum resample_idx id, int len,
			unsigned int *params);
void check_ram_coeff_sinc(enum resample_idx id, int len,
			  unsigned int *params);
void new_resample_set_ram_coeff_aa(enum resample_idx id, int len,
				   unsigned int *params);
#endif

void new_resample_set_ram_coeff_sinc(enum resample_idx id, int len,
				     unsigned int *params);
void new_resample_init_param(enum resample_idx id);
void new_resample_enable(enum resample_idx id, bool enable, int channel);
void new_resampleA_set_format(enum resample_idx id, int source, int channel);
void new_resampleB_set_format(enum resample_idx id, int output_sr, int channel);
void new_resample_set_ratio(enum resample_idx id, int input_sr, int output_sr);
bool new_resample_get_status(enum resample_idx id);
void new_resample_src_select(enum resample_idx id, enum resample_src src);
void new_resample_src_select_v2(enum resample_idx id, unsigned int src);
void new_resample_enable_watchdog(enum resample_idx id, bool enable);

#endif
