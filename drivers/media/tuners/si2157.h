/*
 * Silicon Labs Si2146/2147/2148/2157/2158 silicon tuner driver
 *
 * Copyright (C) 2014 Antti Palosaari <crope@iki.fi>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 */

#ifndef SI2157_H
#define SI2157_H

#include <linux/kconfig.h>
#include "dvb_frontend.h"

struct si2157_config {
	/*
	 * I2C address
	 */
	u8 i2c_addr;

	/*
	 * Spectral Inversion
	 */
	bool inversion;
};

#if IS_ENABLED(CONFIG_MEDIA_TUNER_SI2157)
extern struct dvb_frontend *si2157_attach(struct dvb_frontend *fe,
		struct i2c_adapter *i2c, const struct si2157_config *cfg);
#else
static inline struct dvb_frontend *si2157_attach(struct dvb_frontend *fe,
		struct i2c_adapter *i2c, const struct si2157_config *cfg)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif
