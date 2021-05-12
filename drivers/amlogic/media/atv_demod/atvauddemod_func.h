/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __ATV_AUD_DEMOD_H__
#define __ATV_AUD_DEMOD_H__

#include "aud_demod_reg.h"


extern unsigned int signal_audmode;

extern uint32_t adec_rd_reg(uint32_t addr);
extern void adec_wr_reg(uint32_t reg, uint32_t val);
extern int aml_atvdemod_get_btsc_sap_mode(void);

void set_outputmode(uint32_t standard, uint32_t outmode);
void aud_demod_clk_gate(int on);
void configure_adec(int Audio_mode);
void adec_soft_reset(void);
void audio_thd_init(void);
void audio_thd_det(void);
void audio_carrier_offset_det(void);
void set_nicam_outputmode(uint32_t outmode);
void set_a2_eiaj_outputmode(uint32_t outmode);
void set_btsc_outputmode(uint32_t outmode);
int get_nicam_lock_status(void);
int get_audio_signal_input_mode(void);
void update_nicam_mode(int *nicam_flag, int *nicam_mono_flag,
		int *nicam_stereo_flag, int *nicam_dual_flag);
void update_btsc_mode(int auto_en, int *stereo_flag, int *sap_flag);
void update_a2_eiaj_mode(int auto_en, int *stereo_flag, int *dual_flag);
void set_outputmode_status_init(void);

void set_output_left_right_exchange(unsigned int ch);
void audio_source_select(int source);

#endif /* __ATV_AUD_DEMOD_H__ */
