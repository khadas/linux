/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DVBC_FRONTEND_H__

#define __DVBC_FRONTEND_H__

void dvbc_get_qam_name(enum qam_md_e qam_mode, char *str);
void store_dvbc_qam_mode(struct aml_dtvdemod *demod,
		int qam_mode, int symbolrate);
void gxtv_demod_dvbc_release(struct dvb_frontend *fe);
int gxtv_demod_dvbc_read_status_timer
	(struct dvb_frontend *fe, enum fe_status *status);
int gxtv_demod_dvbc_read_ber(struct dvb_frontend *fe, u32 *ber);
int gxtv_demod_dvbc_read_signal_strength
	(struct dvb_frontend *fe, s16 *strength);
int gxtv_demod_dvbc_read_snr(struct dvb_frontend *fe, u16 *snr);
int gxtv_demod_dvbc_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks);
int gxtv_demod_dvbc_init(struct aml_dtvdemod *demod, int mode);
int gxtv_demod_dvbc_set_frontend(struct dvb_frontend *fe);
int gxtv_demod_dvbc_get_frontend(struct dvb_frontend *fe);
enum qam_md_e dvbc_switch_qam(enum qam_md_e qam_mode);
unsigned int dvbc_auto_fast(struct dvb_frontend *fe, unsigned int *delay, bool re_tune);
unsigned int dvbc_fast_search(struct dvb_frontend *fe, unsigned int *delay, bool re_tune);
int gxtv_demod_dvbc_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status);
enum fe_modulation dvbc_get_dvbc_qam(enum qam_md_e am_qam);
void dvbc_set_srspeed(struct aml_dtvdemod *demod, int high);
int dvbc_set_frontend(struct dvb_frontend *fe);
int dvbc_read_status(struct dvb_frontend *fe, enum fe_status *status, bool re_tune);
int dvbc_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status);
void dvbc_blind_scan_work(struct aml_dtvdemod *demod);

#endif
