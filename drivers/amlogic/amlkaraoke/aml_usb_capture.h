/*
 * drivers/amlogic/amlkaraoke/aml_usb_capture.h
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

#ifndef __AML_USB_CAPTURE_H
#define __AML_USB_CAPTURE_H

/*
 *   A virtual path to get usb audio capture data.
 *
 */
#include <sound/pcm.h>

#include "aml_audio_resampler.h"

/* Keep same with aml_i2s_out_mix.h */
struct usb_audio_buffer {
	dma_addr_t paddr;
	unsigned char *addr;
	unsigned int size;
	unsigned int wr;
	unsigned int rd;
	unsigned int level;
	unsigned int channels;   /* channels */
	unsigned int rate;       /* rate in Hz */
	unsigned int running;
	spinlock_t lock;         /*lock the ringbuffer */
};

struct usb_input {
	struct usb_audio_buffer *usb_buf;
	/* Resample if needed */
	bool resample_request;
	struct resample_para resampler;
	unsigned char *out_buffer;
	unsigned char *mono2stereo;
};

#define USB_AUDIO_CAPTURE_BUFFER_SIZE   (1024 * 4)
#define USB_AUDIO_CAPTURE_PACKAGE_SIZE  (512)

extern int usb_mic_digital_gain;
extern int builtin_mixer;
extern int reverb_enable;

#endif
