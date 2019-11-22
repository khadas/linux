/*
 * drivers/amlogic/amlkaraoke/aml_audio_resampler.h
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

#ifndef __AUDIO_RESAMPLER_H__
#define __AUDIO_RESAMPLER_H__

struct resample_para {
	unsigned int fraction_step;
	unsigned int sample_fraction;
	short lastsample_left;
	short lastsample_right;
	unsigned int input_sr;
	unsigned int output_sr;
	unsigned int channels;
};

int resampler_init(struct resample_para *resample);
int resample_process(
		struct resample_para *resample,
		unsigned int in_frame,
		short *input, short *output);

#endif
