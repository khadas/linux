/*
 * drivers/amlogic/amlkaraoke/aml_audio_resampler.c
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

#include <trace/events/printk.h>
#include "aml_audio_resampler.h"

#include <linux/types.h>

/*Clip from 16.16 fixed-point to 0.15 fixed-point*/
static inline short clip(int x)
{
	if (x < -32768)
		return -32768;
	else if (x > 32767)
		return 32767;
	else
		return x;
}

int resampler_init(struct resample_para *resample)
{
	/* 64bit:long long */
	static const long int k_phase_multiplier = 1L << 28;

	resample->fraction_step = (unsigned int)
		(resample->input_sr * k_phase_multiplier
			/ resample->output_sr);
	resample->sample_fraction = 0;
	resample->lastsample_left = 0;
	resample->lastsample_right = 0;
	return 0;
}

int resample_process(
		struct resample_para *resample,
		unsigned int in_frame,
		short *input, short *output) {
	unsigned int input_index = 0;
	unsigned int output_index = 0;
	unsigned int fraction_step = resample->fraction_step;
	static const unsigned int k_phase_mask = (1LU << 28) - 1;
	unsigned int frac = resample->sample_fraction;
	short lastsample_left = resample->lastsample_left;
	short lastsample_right = resample->lastsample_right;

	if (resample->channels == 2) {
		while (input_index == 0) {
			*output++ = clip((int)lastsample_left +
				((((int)input[0] - (int)lastsample_left)
					* ((int)frac >> 13)) >> 15));
			*output++ = clip((int)lastsample_right +
				((((int)input[1] - (int)lastsample_right)
					* ((int)frac >> 13)) >> 15));
				frac += fraction_step;
				input_index += (frac >> 28);
				frac = (frac & k_phase_mask);
				output_index++;
		}
		while (input_index < in_frame) {
			*output++ = clip((int)input[2 * input_index - 2]
				+ ((((int)input[2 * input_index]
					- (int)input[2 * input_index - 2])
						* ((int)frac >> 13)) >> 15));
			*output++ = clip((int)input[2 * input_index - 1]
				+ ((((int)input[2 * input_index + 1]
					- (int)input[2 * input_index - 1])
						* ((int)frac >> 13)) >> 15));

			frac += fraction_step;
			input_index += (frac >> 28);
			frac = (frac & k_phase_mask);
			output_index++;
		}
		resample->lastsample_left = input[2 * in_frame - 2];
		resample->lastsample_right = input[2 * in_frame - 1];
		resample->sample_fraction = frac;
	} else {
		/*left channel as output*/
		while (input_index == 0) {
			*output++ = clip((int)lastsample_left +
				((((int)input[0] - (int)lastsample_left)
					* ((int)frac >> 13)) >> 15));
			frac += fraction_step;
			input_index += (frac >> 28);
			frac = (frac & k_phase_mask);
			output_index++;
		}
		while (input_index < in_frame) {
			*output++ = clip((int)input[2 * input_index - 2]
				+ ((((int)input[2 * input_index]
					- (int)input[2 * input_index - 2])
						* ((int)frac >> 13)) >> 15));
			frac += fraction_step;
			input_index += (frac >> 28);
			frac = (frac & k_phase_mask);
			output_index++;
		}
		resample->lastsample_left = input[2 * in_frame - 2];
		resample->sample_fraction = frac;
	}
	return output_index;
}
