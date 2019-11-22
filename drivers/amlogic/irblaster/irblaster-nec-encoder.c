/*
 * drivers/amlogic/irblaster/irblaster-nec-encoder.c
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

#include <linux/module.h>
#include <linux/amlogic/irblaster_encoder.h>

#define NEC_NBITS		32
#define NEC_UNIT		562  /* ns */
#define NEC_HEADER_PULSE	9000
#define NEC_HEADER_SPACE	4500
#define NEC_BIT_PULSE		560
#define NEC_BIT_0_SPACE		560
#define NEC_BIT_1_SPACE		1690
#define	NEC_TRAILER_PULSE	560
#define	NEC_TRAILER_SPACE	5600 /* even longer in reality */

static struct irblaster_raw_timings irblaster_nec_timings = {
	.header_pulse	= NEC_HEADER_PULSE,
	.header_space	= NEC_HEADER_SPACE,
	.bit_pulse	= NEC_BIT_PULSE,
	.bit_space[0]	= NEC_BIT_0_SPACE,
	.bit_space[1]	= NEC_BIT_1_SPACE,
	.trailer_pulse	= NEC_TRAILER_PULSE,
	.trailer_space	= NEC_TRAILER_SPACE,
	.msb_first	= 0,
	.raw_nbits	= NEC_NBITS,
	.data_size	= (NEC_NBITS + 2) * 2,
};

static u32 irblaster_nec32_scancode_to_raw(enum irblaster_protocol protocol,
					   unsigned int addrs,
					   unsigned int commmand)
{
	unsigned int addr = 0, addr_inv, data, data_inv;

	data = commmand & 0xff;
	addr_inv = addr & 0xff;
	addr = addrs & 0xff;
	data_inv = data & 0xff;

	return data_inv << 24 |
	       data     << 16 |
	       addr_inv <<  8 |
	       addr;
}

static u32 irblaster_necx_scancode_to_raw(enum irblaster_protocol protocol,
					  unsigned int addrs,
					  unsigned int commmand)
{
	unsigned int addr, addr_inv, data, data_inv;

	data = commmand & 0xff;
	addr = addrs & 0xff;
	addr_inv = addr & 0xff;
	data_inv = data ^ 0xff;

	return data_inv << 24 |
	       data     << 16 |
	       addr_inv <<  8 |
	       addr;
}

static u32 irblaster_nec_scancode_to_raw(enum irblaster_protocol protocol,
					 unsigned int addrs,
					 unsigned int commmand)
{
	unsigned int addr, addr_inv, data, data_inv;

	data = commmand & 0xff;
	addr       = addrs & 0xff;
	addr_inv   = addr ^ 0xff;
	data_inv   = data ^ 0xff;

	return data_inv << 24 |
	       data     << 16 |
	       addr_inv <<  8 |
	       addr;
}

int irblaster_nec_encode(enum irblaster_protocol protocol,
			 unsigned int addr,
			 unsigned int commmand,
			 unsigned int *data)
{
	int ret;
	u32 raw;

	if (protocol >= IRBLASTER_PROTOCOL_MAX)
		return -ENODEV;

	/* Convert a NEC scancode to raw NEC data */
	switch (protocol) {
	case IRBLASTER_PROTOCOL_NEC:
		raw = irblaster_nec_scancode_to_raw(protocol, addr, commmand);
		break;
	case IRBLASTER_PROTOCOL_NECX:
		raw = irblaster_necx_scancode_to_raw(protocol, addr, commmand);
		break;
	case IRBLASTER_PROTOCOL_NEC32:
		raw = irblaster_nec32_scancode_to_raw(protocol, addr, commmand);
		break;
	default:
		raw = irblaster_nec_scancode_to_raw(protocol, addr, commmand);
		break;
	}

	/* Modulate the raw data using a pulse distance modulation */
	ret = irblaster_raw_gen(data, &irblaster_nec_timings, raw);
	if (ret < 0)
		return ret;

	return ret;
}

static struct irblaster_raw_handler irblaster_nec_handler[] = {
	{
		.name		= "NEC",
		.protocol	= IRBLASTER_PROTOCOL_NEC,
		.encode		= irblaster_nec_encode,
		.freq		= 38000,
		.duty		= 50,
		.timing		= &irblaster_nec_timings,
	},
	{
		.name		= "NECX",
		.protocol	= IRBLASTER_PROTOCOL_NECX,
		.encode		= irblaster_nec_encode,
		.freq		= 38000,
		.duty		= 50,
		.timing		= &irblaster_nec_timings,
	},
	{
		.name		= "NEC32",
		.protocol	= IRBLASTER_PROTOCOL_NEC32,
		.encode		= irblaster_nec_encode,
		.freq		= 38000,
		.duty		= 50,
		.timing		= &irblaster_nec_timings,
	}
};

static int __init irblaster_nec_decode_init(void)
{
	int i;

	for (i = 0; i < sizeof(irblaster_nec_handler) /
	     sizeof(struct irblaster_raw_handler); i++)
		irblaster_raw_handler_register(irblaster_nec_handler + i);

	return 0;
}

static void __exit irblaster_nec_decode_exit(void)
{
	int i;

	for (i = 0; i < sizeof(irblaster_nec_handler) /
	     sizeof(struct irblaster_raw_handler); i++)
		irblaster_raw_handler_unregister(irblaster_nec_handler + i);
}

fs_initcall(irblaster_nec_decode_init);
module_exit(irblaster_nec_decode_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("NEC IR protocol decoder");
