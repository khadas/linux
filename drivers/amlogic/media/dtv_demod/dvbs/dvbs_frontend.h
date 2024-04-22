/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DVBS_FRONTEND_H__

#define __DVBS_FRONTEND_H__

int dtvdemod_dvbs_read_ber(struct dvb_frontend *fe, u32 *ber);
int dtvdemod_dvbs_read_signal_strength(struct dvb_frontend *fe, s16 *strength);
int dvbs_read_snr(struct dvb_frontend *fe, u16 *snr);
int dtvdemod_dvbs_unicable_change_channel(struct dvb_frontend *fe);
int dtvdemod_dvbs_blind_check_signal(struct dvb_frontend *fe,
		unsigned int freq_khz, int *next_step_khz, int *signal_state);
void dvbs_blind_scan_new_work(struct work_struct *work);
int dtvdemod_dvbs_set_ch(struct aml_demod_sta *demod_sta);
unsigned int dvbs_get_bitrate(int sr);
int dtvdemod_dvbs_read_status(struct dvb_frontend *fe, enum fe_status *status,
		unsigned int if_freq_khz, bool re_tune);
int dtvdemod_dvbs_set_frontend(struct dvb_frontend *fe);
int dtvdemod_dvbs_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status);
int dtvdemod_dvbs2_init(struct aml_dtvdemod *demod);
int amdemod_stat_dvbs_islock(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys);

#endif

