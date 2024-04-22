/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DTMB_FRONTEND_H__

#define __DTMB_FRONTEND_H__

#define DTMBM_NO_SIGNEL_CHECK    0x01
#define DTMBM_HV_SIGNEL_CHECK    0x02
#define DTMBM_BCH_OVER_CHEK      0x04

#define DTMBM_CHEK_NO            (DTMBM_NO_SIGNEL_CHECK)
#define DTMBM_CHEK_HV            (DTMBM_HV_SIGNEL_CHECK | DTMBM_BCH_OVER_CHEK)
#define DTMBM_WORK               (DTMBM_NO_SIGNEL_CHECK | DTMBM_HV_SIGNEL_CHECK\
									| DTMBM_BCH_OVER_CHEK)

#define DTMBM_POLL_CNT_NO_SIGNAL       (10)
#define DTMBM_POLL_CNT_WAIT_LOCK       (3) /*from 30 to 3 */
#define DTMBM_POLL_DELAY_NO_SIGNAL     (120)
#define DTMBM_POLL_DELAY_HAVE_SIGNAL    (100)

extern unsigned char dtmb_new_driver;

void dtmb_save_status(struct aml_dtvdemod *demod, unsigned int s);
void dtmb_poll_start(struct aml_dtvdemod *demod);
void dtmb_poll_stop(struct aml_dtvdemod *demod);
void dtmb_set_delay(struct aml_dtvdemod *demod, unsigned int delay);
unsigned int dtmb_is_update_delay(struct aml_dtvdemod *demod);
unsigned int dtmb_get_delay_clear(struct aml_dtvdemod *demod);
void dtmb_poll_clear(struct aml_dtvdemod *demod);
void dtmb_poll_v3(struct aml_dtvdemod *demod);
void dtmb_poll_start_tune(struct aml_dtvdemod *demod, unsigned int state);
int dtmb_poll_v2(struct dvb_frontend *fe, enum fe_status *status);
void gxtv_demod_dtmb_release(struct dvb_frontend *fe);
int gxtv_demod_dtmb_read_status(struct dvb_frontend *fe, enum fe_status *status);
int gxtv_demod_dtmb_read_status_old(struct dvb_frontend *fe, enum fe_status *status);
int gxtv_demod_dtmb_read_ber(struct dvb_frontend *fe, u32 *ber);
int gxtv_demod_dtmb_read_signal_strength(struct dvb_frontend *fe, s16 *strength);
int gxtv_demod_dtmb_read_snr(struct dvb_frontend *fe, u16 *snr);
int gxtv_demod_dtmb_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks);
int gxtv_demod_dtmb_set_frontend(struct dvb_frontend *fe);
int gxtv_demod_dtmb_get_frontend(struct dvb_frontend *fe);
int gxtv_demod_dtmb_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status);
int dtmb_tune(struct dvb_frontend *fe, bool re_tune,
	unsigned int mode_flags, unsigned int *delay, enum fe_status *status);
int Gxtv_Demod_Dtmb_Init(struct aml_dtvdemod *demod);
int amdemod_stat_dtmb_islock(struct aml_dtvdemod *demod,
		enum fe_delivery_system delsys);
#endif

