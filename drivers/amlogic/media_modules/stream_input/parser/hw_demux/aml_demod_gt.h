/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#ifndef __AML_DEMOD_GT_H__
#define __AML_DEMOD_GT_H__

#include  "dvb_frontend.h"

struct amlfe_exp_config {
	/*config by aml_fe ?*/
	/* */
	int set_mode;
};

struct amlfe_demod_config {
	int	dev_id;
	u32 ts;
	struct i2c_adapter *i2c_adap;
	int i2c_addr;
	int	reset_gpio;
	int	reset_value;
};

/* For configure different tuners */
/* It can add fields as extensions */
struct tuner_config {
	u8 id;
	u8 i2c_addr;
	u8 xtal; /* 0: 16MHz, 1: 24MHz, 3: 27MHz */
	u8 xtal_cap;
	u8 xtal_mode;
};

static inline struct dvb_frontend* aml_dtvdm_attach (const struct amlfe_exp_config *config) {
	return NULL;
}

static inline struct dvb_frontend* si2151_attach (struct dvb_frontend *fe,struct i2c_adapter *i2c, struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend* mxl661_attach (struct dvb_frontend *fe,struct i2c_adapter *i2c, struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend* si2159_attach (struct dvb_frontend *fe,struct i2c_adapter *i2c, struct tuner_config *cfg)
{
    return NULL;
}

static inline struct dvb_frontend* r842_attach (struct dvb_frontend *fe, struct i2c_adapter *i2c, struct tuner_config *cfg)
{
    return NULL;
}

static inline struct dvb_frontend* r840_attach (struct dvb_frontend *fe, struct i2c_adapter *i2c, struct tuner_config *cfg)
{
    return NULL;
}

static inline struct dvb_frontend* atbm8881_attach (const struct amlfe_demod_config *config)
{
	return NULL;
}

#endif	/*__AML_DEMOD_GT_H__*/
