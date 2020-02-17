/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * include/linux/amlogic/iomap.h
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

#ifndef __SOC_IO_H
#define __SOC_IO_H

#include <linux/amlogic/media/registers/register_map.h>
#include <linux/io.h>

/* vpp simple io */
static inline int aml_read_vcbus_s(unsigned int reg)
{
	return readl((vpp_base + (reg << 2)));
}

static inline void aml_write_vcbus_s(unsigned int reg, unsigned int val)
{
	writel(val, (vpp_base + (reg << 2)));
}

static inline void aml_vcbus_update_bits_s(unsigned int reg,
					   unsigned int value,
					   unsigned int start,
					   unsigned int len)
{
	unsigned int tmp, orig;
	unsigned int mask = (((1L << len) - 1) << start);
	int r = (reg << 2);

	orig =  readl((vpp_base + r));
	tmp = orig  & ~mask;
	tmp |= (value << start) & mask;
	writel(tmp, (vpp_base + r));
}

/* hiu simple io */
static inline int aml_read_hiubus_s(unsigned int reg)
{
	return readl((hiu_base + (reg << 2)));
}

static inline void aml_write_hiubus_s(unsigned int reg,
				      unsigned int val)
{
	writel(val, (hiu_base + (reg << 2)));
}

static inline void aml_hiubus_update_bits_s(unsigned int reg,
					    unsigned int value,
					    unsigned int start,
					    unsigned int len)
{
	unsigned int tmp, orig;
	unsigned int mask = (((1L << len) - 1) << start);
	int r = (reg << 2);

	orig =  readl((hiu_base + r));
	tmp = orig  & ~mask;
	tmp |= (value << start) & mask;
	writel(tmp, (hiu_base + r));
}

#endif
