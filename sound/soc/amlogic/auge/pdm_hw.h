/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_PDM_HW_H__
#define __AML_PDM_HW_H__
#include "audio_io.h"

#define PDM_CHANNELS_MAX		8 /* 8ch pdm in */
#define PDM_LANE_MAX            4 /* 4 data pins, for 8ch maxs*/

struct pdm_info {
	int bitdepth;
	int channels;
	int lane_masks;

	int dclk_idx;  /* mapping for dclk value */
	int bypass;    /* bypass all filter, capture raw data */
	int sample_count;
};

void aml_pdm_ctrl(struct pdm_info *info, int id);
void pdm_force_sysclk_to_oscin(bool force, int id, bool vad_top);
void pdm_force_dclk_to_oscin(int id, bool vad_top);
void pdm_set_channel_ctrl(int sample_count, int id);
void aml_pdm_arb_config(struct aml_audio_controller *actrl, bool use_arb);
int aml_pmd_set_HPF_filter_parameters(void *array);
void aml_pdm_filter_ctrl(int pdm_gain_index, int osr, int set, int id);
void pdm_enable(int is_enable, int id);
void pdm_fifo_reset(int id);
int pdm_get_mute_value(int id);
void pdm_set_mute_value(int val, int id);
int pdm_get_mute_channel(int id);
void pdm_set_mute_channel(int mute_chmask, int id);
void pdm_set_bypass_data(bool bypass, int id);
void pdm_init_truncate_data(int freq, int id);
void pdm_train_en(bool en, int id);
void pdm_train_clr(int id);
int pdm_train_sts(int id);
int pdm_dclkidx2rate(int idx);
int pdm_get_sample_count(int low_power, int dclk_idx);
int pdm_get_ors(int dclk_idx, int sample_rate);
int pdm_auto_train_algorithm(int pdm_id, int enable);

extern int pdm_hcic_shift_gain;

#endif /*__AML_PDM_HW_H__*/
