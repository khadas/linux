/*
 * drivers/amlogic/amlkaraoke/aml_reverb.h
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

#ifndef AML_REVERB_H
#define AML_REVERB_H

#define MAXRATE 48 /*sample num in ms */
#define MAXREVERBS 8

extern int reverb_time;
extern int reverb_enable;
extern int reverb_highpass;
extern int reverb_in_gain;
extern int reverb_out_gain;

struct reverb_t {
	int     counter;
	int     numdelays;
	int     in_gain;
	int     out_gain;
	int     time;
	int     delay[MAXREVERBS];
	int     decay[MAXREVERBS];
	int     samples[MAXREVERBS];
	int     maxsamples;
	s16	*reverbbuf;
};

#define   DATA_FRACTION_BIT 0
#define   HALF_ERROR        0   /*((0x1) << (DATA_FRACTION_BIT - 1))*/

/* Count if coefficients */
#define   COEFF_COUNT	    3
/*Section of cascaded two order IIR filter*/
#define   SECTION           2
/*Channel Count*/
#define   CF                2
#define   COEFF_FRACTION_BIT      24

struct af_equalizer {
	/*B coefficient array*/
	int   b[SECTION][COEFF_COUNT];
	/*A coefficient array*/
	int   a[SECTION][COEFF_COUNT];
	/*Circular buffer for channel input data*/
	int   cx[CF][SECTION][2];
	/*Circular buffer for channel output data*/
	int   cy[CF][SECTION][2];
};

struct audio_format {
	short left;
	short right;
};

int reverb_start(struct reverb_t *aml_reverb);
int reverb_process(struct reverb_t *aml_reverb,
		   s16 *ibuf, s16 *obuf, int isamp,
		   int channels, int channel);
int reverb_drain(struct reverb_t *aml_reverb, s16 *obuf, int osamp);
int reverb_stop(struct reverb_t *aml_reverb);

void eq_init(struct af_equalizer *eq);

int fourth_order_IIR(int input,
		     struct af_equalizer *eq_ptr,
		     int channel);

#endif

