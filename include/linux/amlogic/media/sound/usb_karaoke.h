/*
 * include/linux/amlogic/media/sound/usb_karaoke.h
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

#ifndef __USB_KARAOKE_H
#define __USB_KARAOKE_H

int i2s_out_mix_init(void);
int i2s_out_mix_deinit(void);
void aml_i2s_set_ch_r_info(
	unsigned int channels,
	unsigned int samplerate);

#endif
