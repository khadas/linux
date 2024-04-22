/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __ISDBT_FRONTEND_H__

#define __ISDBT_FRONTEND_H__

int dvbt_isdbt_read_ber(struct dvb_frontend *fe, u32 *ber);
int gxtv_demod_dvbt_isdbt_read_snr(struct dvb_frontend *fe, u16 *snr);

void isdbt_reset_demod(void);
int dvbt_isdbt_read_status(struct dvb_frontend *fe, enum fe_status *status, bool re_tune);
int dvbt_isdbt_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status);
int gxtv_demod_isdbt_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks);
int gxtv_demod_isdbt_read_signal_strength(struct dvb_frontend *fe,
		s16 *strength);
int dvbt_isdbt_set_frontend(struct dvb_frontend *fe);
int gxtv_demod_isdbt_get_frontend(struct dvb_frontend *fe);
int dvbt_isdbt_init(struct aml_dtvdemod *demod);

#endif
