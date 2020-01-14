/*
 * include/linux/amlogic/aml_demod_common.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __AML_DEMOD_COMMON_H__
#define __AML_DEMOD_COMMON_H__

#include <linux/i2c.h>
#include <uapi/linux/videodev2.h>

enum tuner_type {
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
	AM_TUNER_ATBM253 = 15
};

enum atv_demod_type {
	AM_ATV_DEMOD_NONE   = 0,
	AM_ATV_DEMOD_SI2176 = 1,
	AM_ATV_DEMOD_SI2196 = 2,
	AM_ATV_DEMOD_FQ1216 = 3,
	AM_ATV_DEMOD_HTM    = 4,
	AM_ATV_DEMOD_CTC703 = 5,
	AM_ATV_DEMOD_SI2177 = 6,
	AM_ATV_DEMOD_AML    = 7,
	AM_ATV_DEMOD_R840   = 8
};

enum dtv_demod_type {
	AM_DTV_DEMOD_NONE     = 0,
	AM_DTV_DEMOD_AML      = 1,
	AM_DTV_DEMOD_M1       = 2,
	AM_DTV_DEMOD_SI2176   = 3,
	AM_DTV_DEMOD_MXL101   = 4,
	AM_DTV_DEMOD_SI2196   = 5,
	AM_DTV_DEMOD_AVL6211  = 6,
	AM_DTV_DEMOD_SI2168   = 7,
	AM_DTV_DEMOD_SI2168_1 = 8,
	AM_DTV_DEMOD_ITE9133  = 9,
	AM_DTV_DEMOD_ITE9173  = 10,
	AM_DTV_DEMOD_DIB8096  = 11,
	AM_DTV_DEMOD_ATBM8869 = 12,
	AM_DTV_DEMOD_MXL241   = 13,
	AM_DTV_DEMOD_AVL68xx  = 14,
	AM_DTV_DEMOD_MXL683   = 15,
	AM_DTV_DEMOD_ATBM8881 = 16,
	AM_DTV_DEMOD_ATBM7821 = 17,
	AM_DTV_DEMOD_AVL6762  = 18,
	AM_DTV_DEMOD_CXD2856  = 19,
	AM_DTV_DEMOD_MXL248   = 20
};

enum aml_fe_dev_type {
	AM_DEV_TUNER,
	AM_DEV_ATV_DEMOD,
	AM_DEV_DTV_DEMOD
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

/* For configure different demod */
struct demod_config {
	int mode; /* 0: internal, 1: external */
	int dev_id;
	u32 ts;
	int ts_out_mode; /* serial or parallel; 0:serial, 1:parallel */
	struct i2c_adapter *i2c_adap;
	int i2c_addr;
	int reset_gpio;
	int reset_value;
	int ant_power_gpio;
	int ant_power_value;

	int tuner0_i2c_addr;
	int tuner1_i2c_addr;
	int tuner0_code;
	int tuner1_code;
};

/* For configure multi-tuner */
struct aml_tuner {
	struct tuner_config cfg;
	unsigned int i2c_adapter_id;
	struct i2c_adapter *i2c_adp;
};

#endif /* __AML_DEMOD_COMMON_H__ */
