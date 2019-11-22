/*
 * drivers/amlogic/amlkaraoke/aml_reverb.c
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

#include <linux/init.h>
#include <linux/slab.h>

#include "aml_reverb.h"

struct af_equalizer filer;

/* Compute a realistic decay */

static int decay_table[][8] = {
	{0, 0, 0, 0, 0, 0, 0, 0}, /*decay 0ms*/
	{2068, 0, 0, 0, 0, 0, 0, 0}, /*decay time 20ms*/
	{13045, 130, 1, 0, 0, 0, 0, 0}, /*decay time 60ms*/
	{20675, 2068, 207, 21, 2, 0, 0, 0}, /*decay time 120ms*/
	{19284, 5193, 1119, 241, 52, 11, 2, 1}, /*decay time 180ms*/
	{20615, 7751, 2331, 701, 211, 63, 19, 6}, /*decay time 230ms*/
	{27255, 10851, 4320, 1720, 685, 273, 109, 43},
	/*decay time 300ms, max value*/
};

static inline int clip16(int x)
{
	if (x < -32768)
		return -32768;
	if (x > 32767)
		return 32767;

	return x;
}

int reverb_start(struct reverb_t *aml_reverb)
{
	struct reverb_t *reverb = aml_reverb;
	int i;

	reverb->counter = 0;
	reverb->in_gain = ((long long)(32767 * reverb_in_gain + 16384))
							>> 7;
	reverb->out_gain = ((long long)(32767 * reverb_out_gain + 16384))
							>> 7;
	reverb->time = 0;
	reverb->numdelays = 8;
	reverb->maxsamples = 0;
	reverb->reverbbuf = NULL;

	for (i = 0; i < MAXREVERBS; i++) {
		reverb->delay[i] = 40 * i + 8;
		reverb->decay[i] = decay_table[reverb->time][i];
	}

	for (i = 0; i < reverb->numdelays; i++) {
		/* stereo channel */
		reverb->samples[i] = reverb->delay[i] * MAXRATE * 2;
		if (reverb->samples[i] > reverb->maxsamples)
			reverb->maxsamples = reverb->samples[i];
	}

	i = sizeof(s16) * reverb->maxsamples;
	reverb->reverbbuf = kzalloc(i, GFP_KERNEL);
	if (!reverb->reverbbuf)
		return -1;

	for (i = 0; i < reverb->numdelays; i++)
		reverb->in_gain = (reverb->in_gain *
			(32768 - ((reverb->decay[i] * reverb->decay[i]
				+ 16384) >> 15)) + 16384) >> 15;
	pr_info("reverb: reverb->in_gain = %d, reverb->maxsamples = %d\n",
		reverb->in_gain, reverb->maxsamples);

	if (reverb_highpass)
		eq_init(&filer);

	return 0;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int reverb_process(struct reverb_t *aml_reverb,
		   s16 *ibuf, s16 *obuf, int isamp,
		   int channels, int channel)
{
	struct reverb_t *reverb = aml_reverb;
	int len, done, offset;
	int i, j, k;
	int d_in, d_out;
	long long tmp;

	/* check whether reverb is enabled */
	if (!reverb_enable)
		return -1;

	if (reverb->time != reverb_time) {
		for (i = 0; i < MAXREVERBS; i++)
			reverb->decay[i] = decay_table[reverb_time][i];

		memset(reverb->reverbbuf, 0, reverb->maxsamples);
		pr_info("reverb: reset reverb parameters!\n");
	}

	i = reverb->counter;
	len = isamp >> (channels >> 1);
	offset = channels;
	for (done = 0; done < len; done++) {
		d_in = (int)*ibuf;
		ibuf += offset;
		tmp = 0;

		if (reverb_highpass)
			d_in = fourth_order_IIR(d_in, &filer, channel);

		tmp = ((long long)d_in * reverb->in_gain)
				>> (15 - DATA_FRACTION_BIT);
		/* Mix decay of delay and input as output */
		for (j = 0; j < reverb->numdelays; j++) {
			k = (i + reverb->maxsamples - reverb->samples[j])
						% reverb->maxsamples;
			tmp += ((long long)reverb->reverbbuf[k]
						* reverb->decay[j]) >> 15;
		}
		d_in = clip16((tmp + HALF_ERROR) >> DATA_FRACTION_BIT);
		tmp = (tmp * reverb->out_gain) >> 15;
		d_out = clip16((tmp + HALF_ERROR) >> DATA_FRACTION_BIT);

		*obuf = (s16)d_out;
		obuf += offset;
		reverb->reverbbuf[i] = (s16)d_in;
		i++;
		i += (offset >> 1);
		i %= reverb->maxsamples;
	}
	reverb->counter = i;
	reverb->time = reverb_time;

	return 0;
}

/*
 * Clean up reverb effect.
 */
int reverb_stop(struct reverb_t *aml_reverb)
{
	struct reverb_t *reverb = aml_reverb;

	kfree(reverb->reverbbuf);
	reverb->reverbbuf = NULL;
	return 0;
}

static int eq_coefficients[2][SECTION][COEFF_COUNT] = {
	{ /* B coefficients */
		{16777216, -33554433, 16777216},  /*B1*/
		{16777216, -32950274, 16179410},  /*B2*/
	},
	{ /* A coefficients*/
		{16777216, -33554431, 16777216},  /*A1*/
		{16777216, -33297782, 16526985},  /*A2*/
	},
};

void eq_init(struct af_equalizer *eq)
{
	int i, j;

	for (i = 0; i < SECTION; i++) {
		for (j = 0; j < COEFF_COUNT; j++) {
			eq->b[i][j] = eq_coefficients[0][i][j];
			eq->a[i][j] = eq_coefficients[1][i][j];
		}
	}

	memset(eq->cx, 0, sizeof(int) * CF * SECTION * 2);
	memset(eq->cy, 0, sizeof(int) * CF * SECTION * 2);
}

int fourth_order_IIR(int input, struct af_equalizer *eq_ptr, int channel)
{
	int sample = input;
	int y = 0, i = 0;
	long long temp = 0;
	int cx[2][2], cy[2][2];

	cx[0][0] = eq_ptr->cx[channel][0][0];
	cx[0][1] = eq_ptr->cx[channel][0][1];
	cx[1][0] = eq_ptr->cx[channel][1][0];
	cx[1][1] = eq_ptr->cx[channel][1][1];
	cy[0][0] = eq_ptr->cy[channel][0][0];
	cy[0][1] = eq_ptr->cy[channel][0][1];
	cy[1][0] = eq_ptr->cy[channel][1][0];
	cy[1][1] = eq_ptr->cy[channel][1][1];

	sample <<= DATA_FRACTION_BIT;

	for (i = 0; i < SECTION; i++) {
		temp = (long long)sample * (eq_ptr->b[i][0]);
		temp += (long long)cx[i][0] * (eq_ptr->b[i][1]);
		temp += (long long)cx[i][1] * (eq_ptr->b[i][2]);
		temp -= (long long)cy[i][0] * (eq_ptr->a[i][1]);
		temp -= (long long)cy[i][1] * (eq_ptr->a[i][2]);
		y = (int)(temp >> COEFF_FRACTION_BIT);
		cx[i][1] = cx[i][0];
		cx[i][0] = sample;
		cy[i][1] = cy[i][0];
		cy[i][0] = y;
		sample = y;
	}
	eq_ptr->cx[channel][0][0] = cx[0][0];
	eq_ptr->cx[channel][0][1] = cx[0][1];
	eq_ptr->cx[channel][1][0] = cx[1][0];
	eq_ptr->cx[channel][1][1] = cx[1][1];
	eq_ptr->cy[channel][0][0] = cy[0][0];
	eq_ptr->cy[channel][0][1] = cy[0][1];
	eq_ptr->cy[channel][1][0] = cy[1][0];
	eq_ptr->cy[channel][1][1] = cy[1][1];

	return (y + HALF_ERROR) >> DATA_FRACTION_BIT;
}
