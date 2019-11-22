/*
 * drivers/amlogic/amlkaraoke/aml_i2s_out_mix.h
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

#ifndef __AML_I2S_OUT_MIX_H
#define __AML_I2S_OUT_MIX_H

/*
 *   A virtual path to get usb audio capture data to i2s mixed out.
 *
 */

extern unsigned long aml_i2s_playback_start_addr;
extern unsigned long aml_i2s_playback_phy_start_addr;
extern unsigned long aml_i2s_alsa_write_addr;

extern int builtin_mixer;
extern struct usb_audio_buffer *snd_usb_pcm_capture_buffer;

extern unsigned int aml_i2s_playback_channel;
extern unsigned int aml_i2s_playback_format;
extern int irq_karaoke;

/*Keep same struct with usb_capture.h */
typedef
struct usb_audio_buffer {
	dma_addr_t paddr;
	unsigned char *addr;
	unsigned int size;
	unsigned int wr;
	unsigned int rd;
	unsigned int level;
	unsigned int channels;
	unsigned int rate; /* rate in Hz */
	unsigned int running;
	spinlock_t lock; /* lock the ringbuffer */
} i2s_audio_buffer;

#endif
