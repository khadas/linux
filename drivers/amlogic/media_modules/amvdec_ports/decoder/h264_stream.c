/*
 * drivers/amlogic/media_modules/amvdec_ports/decoder/h264_stream.c
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

#include <linux/mm.h>
#include <linux/slab.h>
#include "h264_stream.h"

void h264_stream_set(struct h264_stream_t *s, unsigned char *data, int size)
{
	s->data = data;
	s->size = size;
	s->bit_pos = 7;
	s->byte_pos = 0;
}

unsigned int h264_stream_read_bits(struct h264_stream_t *s, unsigned int n)
{
	unsigned int ret = 0;
	unsigned char b = 0;
	int i;

	if (n == 0)
		return 0;

	for (i = 0; i < n; ++i) {
		if (h264_stream_bits_remaining(s) == 0)
			ret <<= n - i - 1;

		b = s->data[s->byte_pos];
		if (n - i <= 32)
			ret = ret << 1 | BITAT(b, s->bit_pos);

		if (s->bit_pos == 0) {
			s->bit_pos = 7;
			s->byte_pos++;
		} else
			s->bit_pos--;
	}

	return ret;
}

unsigned int h264_stream_peek_bits(struct h264_stream_t *s, unsigned int n)
{
	int prev_bit_pos = s->bit_pos;
	int prev_byte_pos = s->byte_pos;
	unsigned int ret = h264_stream_read_bits(s, n);

	s->bit_pos = prev_bit_pos;
	s->byte_pos = prev_byte_pos;

	return ret;
}

unsigned int h264_stream_read_bytes(struct h264_stream_t *s, unsigned int n)
{
	unsigned int ret = 0;
	int i;

	if (n == 0)
		return 0;

	for (i = 0; i < n; ++i) {
		if (h264_stream_bytes_remaining(s) == 0) {
			ret <<= (n - i - 1) * 8;
			break;
		}

		if (n - i <= 4)
			ret = ret << 8 | s->data[s->byte_pos];

		s->byte_pos++;
	}

	return ret;
}

unsigned int h264_stream_peek_bytes(struct h264_stream_t *s, unsigned int n)
{
	int prev_byte_pos = s->byte_pos;
	unsigned int ret = h264_stream_read_bytes(s, n);

	s->byte_pos = prev_byte_pos;

	return ret;
}

int h264_stream_bits_remaining(struct h264_stream_t *s)
{
	return (s->size - s->byte_pos) * 8 + s->bit_pos;
}

int h264_stream_bytes_remaining(struct h264_stream_t *s)
{
	return s->size - s->byte_pos;
}

