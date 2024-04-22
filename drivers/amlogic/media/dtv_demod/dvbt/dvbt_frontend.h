/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DVBT_FRONTEND_H__
#define __DVBT_FRONTEND_H__

int dvbt_read_ber(struct dvb_frontend *fe, u32 *ber);
int dvbt_read_snr(struct dvb_frontend *fe, u16 *snr);
int dvbt2_read_snr(struct dvb_frontend *fe, u16 *snr);
int gxtv_demod_dvbt_read_signal_strength(struct dvb_frontend *fe, s16 *strength);
int dvbt_set_frontend(struct dvb_frontend *fe);
int dvbt2_set_frontend(struct dvb_frontend *fe);
int dvbt_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status);
int dvbt2_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status);
int dvbtx_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status);
unsigned int dvbt_init(struct aml_dtvdemod *demod);
unsigned int dtvdemod_dvbt2_init(struct aml_dtvdemod *demod);
int gxtv_demod_dvbt_get_frontend(struct dvb_frontend *fe);
int gxtv_demod_dvbt_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks);
int amdemod_stat_dvbt_islock(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys);

#endif
