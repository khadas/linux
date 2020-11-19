/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DEMOD_GT_H__
#define __AML_DEMOD_GT_H__

#include  "dvb_frontend.h"

enum aml_tuner_type_t {
	AM_TUNER_NONE = 0,
	AM_TUNER_SI2176 = 1,
	AM_TUNER_SI2196 = 2,
	AM_TUNER_FQ1216 = 3,
	AM_TUNER_HTM = 4,
	AM_TUNER_CTC703 = 5,
	AM_TUNER_SI2177 = 6,
	AM_TUNER_R840 = 7,
	AM_TUNER_SI2157 = 8,
	AM_TUNER_SI2151 = 9,
	AM_TUNER_MXL661 = 10,
	AM_TUNER_MXL608 = 11,
	AM_TUNER_SI2159 = 12,
	AM_TUNER_R842 = 13,
	AM_TUNER_ATBM2040 = 14,
};

enum aml_atv_demod_type_t {
	AM_ATV_DEMOD_SI2176 = 1,
	AM_ATV_DEMOD_SI2196 = 2,
	AM_ATV_DEMOD_FQ1216 = 3,
	AM_ATV_DEMOD_HTM = 4,
	AM_ATV_DEMOD_CTC703 = 5,
	AM_ATV_DEMOD_SI2177 = 6,
	AM_ATV_DEMOD_AML = 7,
	AM_ATV_DEMOD_R840 = 8
};

enum aml_dtv_demod_type_t {
	AM_DTV_DEMOD_M1 = 0,
	AM_DTV_DEMOD_SI2176 = 1,
	AM_DTV_DEMOD_MXL101 = 2,
	AM_DTV_DEMOD_SI2196 = 3,
	AM_DTV_DEMOD_AVL6211 = 4,
	AM_DTV_DEMOD_SI2168 = 5,
	AM_DTV_DEMOD_ITE9133 = 6,
	AM_DTV_DEMOD_ITE9173 = 7,
	AM_DTV_DEMOD_DIB8096 = 8,
	AM_DTV_DEMOD_ATBM8869 = 9,
	AM_DTV_DEMOD_MXL241 = 10,
	AM_DTV_DEMOD_AVL68xx = 11,
	AM_DTV_DEMOD_MXL683 = 12,
	AM_DTV_DEMOD_ATBM8881 = 13
};

enum aml_fe_dev_type_t {
	AM_DEV_TUNER,
	AM_DEV_ATV_DEMOD,
	AM_DEV_DTV_DEMOD
};

struct amlfe_exp_config {
	/*config by aml_fe ? */
	/* */
	int set_mode;
};

struct amlfe_demod_config {
	int dev_id;
	u32 ts;
	struct i2c_adapter *i2c_adap;
	int i2c_addr;
	int reset_gpio;
	int reset_value;
};

/* For configure different tuners */
/* It can add fields as extensions */
struct tuner_config {
	u8 id;
	u8 i2c_addr;
	u8 xtal;		/* 0: 16MHz, 1: 24MHz, 3: 27MHz */
	u8 xtal_cap;
	u8 xtal_mode;
};

static inline struct dvb_frontend *aml_dtvdm_attach(const struct amlfe_exp_config *config)
{
	return NULL;
}

static inline struct dvb_frontend *si2151_attach(struct dvb_frontend *fe,
						 struct i2c_adapter *i2c,
						 struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *mxl661_attach(struct dvb_frontend *fe,
						 struct i2c_adapter *i2c,
						 struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *si2159_attach(struct dvb_frontend *fe,
						 struct i2c_adapter *i2c,
						 struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *r842_attach(struct dvb_frontend *fe,
					       struct i2c_adapter *i2c,
					       struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *r840_attach(struct dvb_frontend *fe,
					       struct i2c_adapter *i2c,
					       struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *atbm2040_attach(struct dvb_frontend *fe,
						   struct i2c_adapter *i2c,
						   struct tuner_config *cfg)
{
	return NULL;
}

static inline struct dvb_frontend *atbm8881_attach(const struct amlfe_demod_config *config)
{
	return NULL;
}

static inline struct dvb_frontend *si2168_attach(const struct amlfe_demod_config *config)
{
	return NULL;
}

static inline struct dvb_frontend *avl6762_attach(const struct amlfe_demod_config *config)
{
	return NULL;
}

#endif	/*__AML_DEMOD_GT_H__*/
