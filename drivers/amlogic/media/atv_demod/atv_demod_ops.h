/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __ATV_DEMOD_OPS_H__
#define __ATV_DEMOD_OPS_H__

#include <dvb_frontend.h>
#include <tuner-i2c.h>

#include "atv_demod_driver.h"
#include "atv_demod_afc.h"
#include "atv_demod_monitor.h"
#include "atv_demod_isr.h"


#define AML_ATVDEMOD_UNINIT         0x0
#define AML_ATVDEMOD_INIT           0x1
#define AML_ATVDEMOD_RESUME         0x2
#define AML_ATVDEMOD_SCAN_MODE      0x3
#define AML_ATVDEMOD_UNSCAN_MODE    0x4

#define ATVDEMOD_STATE_IDEL  0
#define ATVDEMOD_STATE_WORK  1
#define ATVDEMOD_STATE_SLEEP 2

#define AFC_BEST_LOCK    50
#define ATV_AFC_500KHZ   500000
#define ATV_AFC_1_0MHZ   1000000
#define ATV_AFC_2_0MHZ   2000000

#define ATV_SECAM_LC_100MHZ 100000000

#define ATVDEMOD_INTERVAL  (HZ / 100) /* 10ms, #define HZ 100 */

#define AUTO_DETECT_COLOR (1 << 0)
#define AUTO_DETECT_AUDIO (1 << 1)

struct atv_demod_sound {
	unsigned int broadcast_std; /* PAL-I/BG/DK/M, NTSC-M */
	unsigned int soundsys;      /* A2/BTSC/EIAJ/NICAM */
	unsigned int input_mode;    /* Mono/Stereo/Dual/Sap */
	unsigned int output_mode;   /* Mono/Stereo/Dual/Sap */
	int sif_over_modulation;
};

struct atv_demod_parameters {

	struct analog_parameters param;

	bool secam_l;
	bool secam_lc;

	unsigned int last_frequency;
	unsigned int lock_range;
	unsigned int leap_step;

	unsigned int afc_range;
	unsigned int tuner_id;
	unsigned int if_freq;
	unsigned int if_inv;
	unsigned int reserved;
};

struct atv_demod_priv {
	struct tuner_i2c_props i2c_props;
	struct list_head hybrid_tuner_instance_list;

	bool standby;

	struct atv_demod_parameters atvdemod_param;
	struct atv_demod_sound atvdemod_sound;

	struct atv_demod_afc afc;

	struct atv_demod_monitor monitor;

	struct atv_demod_isr isr;

	int state;
	bool scanning;
};


extern int atv_demod_enter_mode(struct dvb_frontend *fe);
#ifdef CONFIG_AMLOGIC_MEDIA_VDIN
extern int tvin_get_av_status(void);
#endif

struct dvb_frontend *aml_atvdemod_attach(struct dvb_frontend *fe,
		struct v4l2_frontend *v4l2_fe,
		struct i2c_adapter *i2c_adap, u8 i2c_addr, u32 tuner_id);


#endif /* __ATV_DEMOD_OPS_H__ */
