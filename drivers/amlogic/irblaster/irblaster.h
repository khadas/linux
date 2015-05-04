/*
 * drivers/amlogic/irblaster/irblaster.h
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

#ifndef AM_IRBLASTER_H
#define AM_IRBLASTER_H
#define MAX_PLUSE 1024
struct aml_blaster {
	unsigned int consumerir_freqs;
	unsigned int pattern[MAX_PLUSE];
	unsigned int pattern_len;
	unsigned int fisrt_pulse_width;
};
enum  {	/* Modulation level*/
	fisrt_low  = 0,
	fisrt_high = 1
};
#define AO_IR_BLASTER_ADDR0 ((0x00 << 10) | (0x30 << 2))
#define AO_IR_BLASTER_ADDR1 ((0x00 << 10) | (0x31 << 2))
#define AO_IR_BLASTER_ADDR2 ((0x00 << 10) | (0x32 << 2))
#define AO_RTI_PIN_MUX_REG (0x14)

#define CONSUMERIR_TRANSMIT     0x5500
#define GET_CARRIER         0x5501
#define SET_CARRIER         0x5502
#define SET_MODLEVEL         0x5503
#endif
