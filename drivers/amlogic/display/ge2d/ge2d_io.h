/*
 * drivers/amlogic/display/ge2d/ge2d_io.h
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


#ifndef _GE2D_IO_H_
#define _GE2D_IO_H_

#include <linux/amlogic/iomap.h>

static inline uint32_t ge2d_vcbus_read(uint32_t reg)
{
	return (uint32_t)aml_read_vcbus(reg);
};

static inline uint32_t ge2d_cbus_read(uint32_t reg)
{
	return (uint32_t)aml_read_cbus(reg);
};

static inline void ge2d_cbus_write(uint32_t reg,
				   const uint32_t val)
{
	aml_write_cbus(reg, val);
};

static inline uint32_t ge2d_cbus_get_bits(uint32_t reg,
		const uint32_t start,
		const uint32_t len)
{
	uint32_t val;

	val = (ge2d_cbus_read(reg) >> (start)) & ((1L << (len)) - 1);
	return val;
}

static inline void ge2d_cbus_set_bits(uint32_t reg,
				      const uint32_t value,
				      const uint32_t start,
				      const uint32_t len)
{
	ge2d_cbus_write(reg, ((ge2d_cbus_read(reg) &
			       ~(((1L << (len)) - 1) << (start))) |
			      (((value) & ((1L << (len)) - 1)) << (start))));
}


#endif
