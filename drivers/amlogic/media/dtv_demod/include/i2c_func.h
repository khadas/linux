/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __I2C_FUNC_H__
#define __I2C_FUNC_H__

int aml_demod_i2c_read(struct i2c_adapter *i2c_adap, unsigned char slave_addr,
		unsigned char *data, unsigned short size);
int aml_demod_i2c_write(struct i2c_adapter *i2c_adap, unsigned char slave_addr,
		unsigned char *data, unsigned short size);

#endif /* __I2C_FUNC_H__ */
