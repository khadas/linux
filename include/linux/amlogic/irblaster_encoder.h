/*
 * include/linux/amlogic/irblaster_encoder.h
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

#ifndef __LINUX_ENCODER_H
#define __LINUX_ENCODER_H

#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/of.h>

struct irblaster_chip;
int irblaster_send(struct irblaster_chip *chip, unsigned int *data,
		   unsigned int len);

static DEFINE_MUTEX(irblaster_raw_handler_lock);
static LIST_HEAD(irblaster_raw_handler_list);

/**
 * enum irblaster_protocol - protocol of irblaster
 * add a new protocol here
 * @IRBLASTER_PROTOCOL_MAX: Maximum number of support protocol
 * please add a new protocol before this
 */
enum irblaster_protocol {
	IRBLASTER_PROTOCOL_NEC,
	IRBLASTER_PROTOCOL_NECX,
	IRBLASTER_PROTOCOL_NEC32,
	IRBLASTER_PROTOCOL_RCA,
	IRBLASTER_PROTOCOL_MAX
};

#define IRBLASTER_PROTOCOL_BIT_NEC	BIT_ULL(IRBLASTER_PROTOCOL_NEC)
#define IRBLASTER_PROTOCOL_BIT_NECX	BIT_ULL(IRBLASTER_PROTOCOL_NECX)
#define IRBLASTER_PROTOCOL_BIT_NEC32	BIT_ULL(IRBLASTER_PROTOCOL_NEC32)
#define IRBLASTER_PROTOCOL_BIT_RCA	BIT_ULL(IRBLASTER_PROTOCOL_RCA)

/**
 * struct irblaster_raw_timings - pulse-length modulation timings
 * @header_pulse:	duration of header pulse in ns (0 for none)
 * @bit_space:		duration of bit space in ns
 * @bit_pulse:		duration of bit pulse (for logic 0 and 1) in ns
 * @trailer_space:	duration of trailer space in ns
 * @msb_first:		1 if most significant bit is sent first
 * @raw_nbits:		raw bit len
 * @data_size:		the total length of the array
 */
struct irblaster_raw_timings {
	unsigned int header_pulse;
	unsigned int header_space;
	unsigned int bit_pulse;
	unsigned int bit_space[2];
	unsigned int trailer_pulse;
	unsigned int trailer_space;
	unsigned int msb_first:1;
	unsigned int raw_nbits;
	unsigned int data_size;
};

struct irblaster_raw_handler {
	struct list_head list;
	char *name;
	int protocol;	/* which are handled by this handler */
	int (*encode)(enum irblaster_protocol protocol, unsigned int addr,
		      unsigned int commmand, unsigned int *data);
	struct irblaster_raw_timings *timing;
	struct mutex encode_lock; /* use to function encode */
	u32 freq;
	u32 duty;
};

/* irblaster encode APIs */
int irblaster_raw_handler_register(struct irblaster_raw_handler
				   *ir_raw_handler);
void irblaster_raw_handler_unregister(struct irblaster_raw_handler
				      *ir_raw_handler);
int irblaster_raw_gen(unsigned int *data,
		      const struct irblaster_raw_timings *timings,
		      u32 raw);

unsigned int protocol_store_select(const char *buf);
unsigned int protocol_show_select(struct irblaster_chip *chip, char *buf);

#endif /* __LINUX_ENCODER_H */
