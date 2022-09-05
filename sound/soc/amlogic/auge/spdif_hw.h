/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_SPDIF_HW_H__
#define __AML_SPDIF_HW_H__
#include "audio_io.h"
#include "regs.h"

#include "../common/iec_info.h"

void aml_spdifin_chnum_en(struct aml_audio_controller *actrl,
			  int index,
			  bool is_enable);
void aml_spdif_enable(struct aml_audio_controller *actrl,
	int stream,
	int index,
	bool is_enable);

void aml_spdif_mute(struct aml_audio_controller *actrl,
	int stream,
	int index,
	bool is_mute);

void aml_spdifout_mute_without_actrl(int index,
	bool is_mute);

void aml_spdif_arb_config(struct aml_audio_controller *actrl, bool use_arb);

int aml_spdifin_status_check(struct aml_audio_controller *actrl);
void aml_spdifin_clr_irq(struct aml_audio_controller *actrl,
			 bool is_all_bits,
			 int clr_bits_val);

void aml_spdif_fifo_reset(struct aml_audio_controller *actrl,
	int stream, int index);

int spdifout_get_frddr_type(int bitwidth);

void aml_spdif_fifo_ctrl(struct aml_audio_controller *actrl,
	int bitwidth,
	int stream,
	int index,
	unsigned int fifo_id);

int spdifin_get_mode(void);

int spdif_get_channel_status(int reg);

unsigned int spdif_get_channel_status0(int spdif_id);
void spdif_set_channel_status0(int spdif_id, unsigned int status0);

void spdifin_set_channel_status(int ch, int bits);

void aml_spdifout_select_aed(bool enable, int spdifout_id);

void aml_spdifout_get_aed_info(int spdifout_id,
			       int *bitwidth,
			       int *frddrtype);

void enable_spdifout_to_hdmitx(int separated);

void spdifout_samesource_set(int spdif_index,
			     int fifo_id,
			     int bitwidth,
			     int channels,
			     bool is_enable,
			     int lane_i2s);
void spdifout_enable(int spdif_id, bool is_enable, bool reenable);

int spdifin_get_sample_rate(void);

int spdifin_get_ch_status0to31(void);

int spdifin_get_audio_type(void);

void spdif_set_channel_status_info(struct iec958_chsts *chsts, int spdif_id);

void spdifout_play_with_zerodata(unsigned int spdif_id,
				 bool reenable,
				 int separated);
void spdifout_play_with_zerodata_free(unsigned int spdif_id);
void spdifin_set_src(int src);
void aml_spdif_out_reset(unsigned int spdif_id, int offset);
void aml_spdifin_sample_mode_filter_en(void);
int get_spdif_to_hdmitx_id(void);
void set_spdif_to_hdmitx_id(int spdif_id);

#endif
