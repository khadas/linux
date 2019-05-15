/*
 * drivers/amlogic/media_modules/amvdec_ports/decoder/h264_stream.h
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

#ifndef _H264_STREAM_H
#define _H264_STREAM_H

#include "utils.h"

struct h264_stream_t {
	unsigned char *data;
	unsigned int size;
	int bit_pos;
	int byte_pos;
};

void h264_stream_set(struct h264_stream_t *s, unsigned char *data, int size);
unsigned int h264_stream_read_bits(struct h264_stream_t *s, unsigned int n);
unsigned int h264_stream_peek_bits(struct h264_stream_t *s, unsigned int n);
unsigned int h264_stream_read_bytes(struct h264_stream_t *s, unsigned int n);
unsigned int h264_stream_peek_bytes(struct h264_stream_t *s, unsigned int n);
int h264_stream_bits_remaining(struct h264_stream_t *s);
int h264_stream_bytes_remaining(struct h264_stream_t *s);

#endif //_H264_STREAM_H

