/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __ATSC_FRONTEND_H__

#define __ATSC_FRONTEND_H__

void gxtv_demod_atsc_release(struct dvb_frontend *fe);
int gxtv_demod_atsc_read_status(struct dvb_frontend *fe, enum fe_status *status);
int gxtv_demod_atsc_read_ber(struct dvb_frontend *fe, u32 *ber);
int gxtv_demod_atsc_read_signal_strength(struct dvb_frontend *fe, s16 *strength);
int gxtv_demod_atsc_read_snr(struct dvb_frontend *fe, u16 *snr);
int gxtv_demod_atsc_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks);
int gxtv_demod_atsc_set_frontend(struct dvb_frontend *fe);
int gxtv_demod_atsc_get_frontend(struct dvb_frontend *fe);
void atsc_polling(struct dvb_frontend *fe, enum fe_status *status);
void atsc_optimize_cn(bool reset);
void atsc_read_status(struct dvb_frontend *fe, enum fe_status *status, unsigned int re_tune);
int gxtv_demod_atsc_tune(struct dvb_frontend *fe, bool re_tune, unsigned int mode_flags,
	unsigned int *delay, enum fe_status *status);
int dtvdemod_atsc_init(struct aml_dtvdemod *demod);
int amdemod_stat_atsc_islock(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys);
unsigned int dtvdemod_get_atsc_lock_sts(struct aml_dtvdemod *demod);

#endif
