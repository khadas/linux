/*
 * Amlogic VOUT Module
 *
 * Copyright (C) 2015 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author: Platform-BJ <platform.bj@amlogic.com>
 *
 */

#ifndef _VOUT_IO_H_
#define _VOUT_IO_H_

#include <linux/amlogic/iomap.h>

static inline uint32_t vout_cbus_read(uint32_t reg)
{
	return (uint32_t)aml_read_cbus(reg);
};

static inline void vout_cbus_write(uint32_t reg,
				   const uint32_t val)
{
	aml_write_cbus(reg, val);
};

static inline void vout_cbus_set_bits(uint32_t reg,
				      const uint32_t value,
				      const uint32_t start,
				      const uint32_t len)
{
	vout_cbus_write(reg, ((vout_cbus_read(reg) &
			       ~(((1L << (len)) - 1) << (start))) |
			      (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline uint32_t vout_vcbus_read(uint32_t reg)
{
	return (uint32_t)aml_read_vcbus(reg);
};

static inline void vout_vcbus_write(uint32_t reg,
				    const uint32_t val)
{
	aml_write_vcbus(reg, val);
};

static inline void vout_vcbus_set_mask(uint32_t reg,
				       const uint32_t mask)
{
	vout_vcbus_write(reg, (vout_vcbus_read(reg) | (mask)));
}

static inline void vout_vcbus_clr_mask(uint32_t reg,
				       const uint32_t mask)
{
	vout_vcbus_write(reg, (vout_vcbus_read(reg) & (~(mask))));
}

static inline void vout_vcbus_set_bits(uint32_t reg,
				       const uint32_t value,
				       const uint32_t start,
				       const uint32_t len)
{
	vout_vcbus_write(reg, ((vout_vcbus_read(reg) &
				~(((1L << (len)) - 1) << (start))) |
			       (((value) & ((1L << (len)) - 1)) << (start))));
}
#endif
