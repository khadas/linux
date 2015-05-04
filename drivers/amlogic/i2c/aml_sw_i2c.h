/*
 * drivers/amlogic/i2c/aml_sw_i2c.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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


#ifndef AML_PIO_I2C_H
#define AML_PIO_I2C_H
#include <linux/i2c-aml.h>

#define AML_PIO_OUTPUT		1
#define AML_GPIO_INPUT		0

struct aml_sw_i2c {
	struct aml_sw_i2c_pins		*sw_pins;
	struct i2c_adapter			adapter;
	struct i2c_algo_bit_data	algo_data;
	struct class                class;
};

#endif


