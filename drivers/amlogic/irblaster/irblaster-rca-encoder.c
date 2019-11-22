/*
 * drivers/amlogic/irblaster/irblaster-rca-encoder.c
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

#define RCA_NBITS		24
#define RCA_UNIT		500  /* ns */
#define RCA_HEADER_PULSE	4000
#define RCA_HEADER_SPACE	4000
#define RCA_BIT_PULSE		500
#define RCA_BIT_0_SPACE		1000
#define RCA_BIT_1_SPACE		2000
#define	RCA_TRAILER_PULSE	500
#define	RCA_TRAILER_SPACE	5600 /* even longer in reality */

static struct irblaster_raw_timings irblaster_rca_timings = {
	.header_pulse	= RCA_HEADER_PULSE,
	.header_space	= RCA_HEADER_SPACE,
	.bit_pulse	= RCA_BIT_PULSE,
	.bit_space[0]	= RCA_BIT_0_SPACE,
	.bit_space[1]	= RCA_BIT_1_SPACE,
	.trailer_pulse	= RCA_TRAILER_PULSE,
	.trailer_space	= RCA_TRAILER_SPACE,
	.msb_first	= 1,
	.raw_nbits	= RCA_NBITS,
	.data_size	= (RCA_NBITS + 2) * 2,
};

static u32 irblaster_rca_scancode_to_raw(enum irblaster_protocol protocol,
					 unsigned int addrs,
					 unsigned int commmand)
{
	unsigned int addr, addr_inv, data, data_inv;

	data = commmand & 0xff;
	addr       = addrs & 0x0f;
	addr_inv   = addr ^ 0x0f;
	data_inv   = data ^ 0xff;

	return addr << 20 |
	       data << 12 |
	       addr_inv << 8 |
	       data_inv;
}

int irblaster_rca_encode(enum irblaster_protocol protocol, unsigned int addr,
			 unsigned int commmand, unsigned int *data)
{
	int ret;
	u32 raw;

	/* Convert a RCA scancode to raw rca data */
	raw = irblaster_rca_scancode_to_raw(protocol, addr, commmand);

	/* Modulate the raw data using a pulse distance modulation */
	ret = irblaster_raw_gen(data, &irblaster_rca_timings, raw);
	if (ret < 0)
		return ret;

	return ret;
}

static struct irblaster_raw_handler irblaster_rca_handler = {
	.name		= "RCA",
	.protocol	= IRBLASTER_PROTOCOL_RCA,
	.encode		= irblaster_rca_encode,
	.freq		= 38000,
	.duty		= 50,
	.timing		= &irblaster_rca_timings,
};

static int __init irblaster_rca_decode_init(void)
{
	return irblaster_raw_handler_register(&irblaster_rca_handler);
}

static void __exit irblaster_rca_decode_exit(void)
{
	irblaster_raw_handler_unregister(&irblaster_rca_handler);
}

fs_initcall(irblaster_rca_decode_init);
module_exit(irblaster_rca_decode_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("RCA IR protocol decoder");
