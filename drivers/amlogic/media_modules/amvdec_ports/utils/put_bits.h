/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef AVCODEC_PUT_BITS_H
#define AVCODEC_PUT_BITS_H

#include <linux/kernel.h>
#include <linux/types.h>
#include "common.h"

struct put_bits_context {
	u32 bit_buf;
	int bit_left;
	u8 *buf;
	u8 *buf_ptr;
	u8 *buf_end;
	int size_in_bits;
};

/**
 * Initialize the struct put_bits_context s.
 *
 * @param buffer the buffer where to put bits
 * @param buffer_size the size in bytes of buffer
 */
static inline void init_put_bits(struct put_bits_context *s,
	u8 *buffer, int buffer_size)
{
	if (buffer_size < 0) {
		buffer_size = 0;
		buffer      = NULL;
	}

	s->size_in_bits = 8 * buffer_size;
	s->buf          = buffer;
	s->buf_end      = s->buf + buffer_size;
	s->buf_ptr      = s->buf;
	s->bit_left     = 32;
	s->bit_buf      = 0;
}

/**
 * Rebase the bit writer onto a reallocated buffer.
 *
 * @param buffer the buffer where to put bits
 * @param buffer_size the size in bytes of buffer,
 *                    must be larger than the previous size
 */
static inline void rebase_put_bits(struct put_bits_context *s,
	u8 *buffer, int buffer_size)
{
	s->buf_end = buffer + buffer_size;
	s->buf_ptr = buffer + (s->buf_ptr - s->buf);
	s->buf     = buffer;
	s->size_in_bits = 8 * buffer_size;
}

/**
 * @return the total number of bits written to the bitstream.
 */
static inline int put_bits_count(struct put_bits_context *s)
{
	return (s->buf_ptr - s->buf) * 8 + 32 - s->bit_left;
}

/**
 * @return the number of bits available in the bitstream.
 */
static inline int put_bits_left(struct put_bits_context* s)
{
	return (s->buf_end - s->buf_ptr) * 8 - 32 + s->bit_left;
}

/**
 * Pad the end of the output stream with zeros.
 */
static inline void flush_put_bits(struct put_bits_context *s)
{
#ifndef BITSTREAM_WRITER_LE
	if (s->bit_left < 32)
		s->bit_buf <<= s->bit_left;
#endif
	while (s->bit_left < 32) {
#ifdef BITSTREAM_WRITER_LE
		*s->buf_ptr++ = s->bit_buf;
		s->bit_buf  >>= 8;
#else
		*s->buf_ptr++ = s->bit_buf >> 24;
		s->bit_buf  <<= 8;
#endif
		s->bit_left  += 8;
	}
	s->bit_left = 32;
	s->bit_buf  = 0;
}

static inline void flush_put_bits_le(struct put_bits_context *s)
{
	while (s->bit_left < 32) {
		*s->buf_ptr++ = s->bit_buf;
		s->bit_buf  >>= 8;
		s->bit_left  += 8;
	}
	s->bit_left = 32;
	s->bit_buf  = 0;
}

#ifdef BITSTREAM_WRITER_LE
#define avpriv_align_put_bits align_put_bits_unsupported_here
#define avpriv_put_string ff_put_string_unsupported_here
#define avpriv_copy_bits avpriv_copy_bits_unsupported_here
#else
/**
 * Pad the bitstream with zeros up to the next byte boundary.
 */
void avpriv_align_put_bits(struct put_bits_context *s);

/**
 * Put the string string in the bitstream.
 *
 * @param terminate_string 0-terminates the written string if value is 1
 */
void avpriv_put_string(struct put_bits_context *pb,
	const char *string, int terminate_string);

/**
 * Copy the content of src to the bitstream.
 *
 * @param length the number of bits of src to copy
 */
void avpriv_copy_bits(struct put_bits_context *pb, const u8 *src, int length);
#endif

/**
 * Write up to 31 bits into a bitstream.
 * Use put_bits32 to write 32 bits.
 */
static inline void put_bits(struct put_bits_context *s, int n, u32 value)
{
	u32 bit_buf;
	int bit_left;

	bit_buf  = s->bit_buf;
	bit_left = s->bit_left;

	/* XXX: optimize */
#ifdef BITSTREAM_WRITER_LE
	bit_buf |= value << (32 - bit_left);
	if (n >= bit_left) {
		if (3 < s->buf_end - s->buf_ptr) {
			AV_WL32(s->buf_ptr, bit_buf);
			s->buf_ptr += 4;
		} else {
			pr_err("Internal error, put_bits buffer too small\n");
		}
		bit_buf     = value >> bit_left;
		bit_left   += 32;
	}
	bit_left -= n;
#else
	if (n < bit_left) {
		bit_buf     = (bit_buf << n) | value;
		bit_left   -= n;
	} else {
		bit_buf   <<= bit_left;
		bit_buf    |= value >> (n - bit_left);
		if (3 < s->buf_end - s->buf_ptr) {
			AV_WB32(s->buf_ptr, bit_buf);
			s->buf_ptr += 4;
		} else {
			pr_err("Internal error, put_bits buffer too small\n");
		}
		bit_left   += 32 - n;
		bit_buf     = value;
	}
#endif
	s->bit_buf  = bit_buf;
	s->bit_left = bit_left;
}

static inline void put_bits_le(struct put_bits_context *s, int n, u32 value)
{
	u32 bit_buf;
	int bit_left;

	bit_buf  = s->bit_buf;
	bit_left = s->bit_left;

	bit_buf |= value << (32 - bit_left);
	if (n >= bit_left) {
		if (3 < s->buf_end - s->buf_ptr) {
			AV_WL32(s->buf_ptr, bit_buf);
			s->buf_ptr += 4;
		} else {
			pr_err("Internal error, put_bits buffer too small\n");
		}
		bit_buf     = value >> bit_left;
		bit_left   += 32;
	}
	bit_left -= n;

	s->bit_buf  = bit_buf;
	s->bit_left = bit_left;
}

static inline u32 av_mod_uintp2(u32 a, u32 p)
{
	return a & ((1 << p) - 1);
}

static inline void put_sbits(struct put_bits_context *pb, int n, int32_t value)
{
	put_bits(pb, n, av_mod_uintp2(value, n));
}

/**
 * Write exactly 32 bits into a bitstream.
 */
static void put_bits32(struct put_bits_context *s, u32 value)
{
	u32 bit_buf;
	int bit_left;

	bit_buf  = s->bit_buf;
	bit_left = s->bit_left;

#ifdef BITSTREAM_WRITER_LE
	bit_buf |= value << (32 - bit_left);
	if (3 < s->buf_end - s->buf_ptr) {
		AV_WL32(s->buf_ptr, bit_buf);
		s->buf_ptr += 4;
	} else {
		pr_err("Internal error, put_bits buffer too small\n");
	}
	bit_buf     = (uint64_t)value >> bit_left;
#else
	bit_buf     = (uint64_t)bit_buf << bit_left;
	bit_buf    |= value >> (32 - bit_left);
	if (3 < s->buf_end - s->buf_ptr) {
		AV_WB32(s->buf_ptr, bit_buf);
		s->buf_ptr += 4;
	} else {
		pr_err("Internal error, put_bits buffer too small\n");
	}
	bit_buf     = value;
#endif

	s->bit_buf  = bit_buf;
	s->bit_left = bit_left;
}

/**
 * Write up to 64 bits into a bitstream.
 */
static inline void put_bits64(struct put_bits_context *s, int n, uint64_t value)
{
	if (n < 32)
		put_bits(s, n, value);
	else if (n == 32)
		put_bits32(s, value);
	else if (n < 64) {
		u32 lo = value & 0xffffffff;
		u32 hi = value >> 32;
#ifdef BITSTREAM_WRITER_LE
		put_bits32(s, lo);
		put_bits(s, n - 32, hi);
#else
		put_bits(s, n - 32, hi);
		put_bits32(s, lo);
#endif
	} else {
		u32 lo = value & 0xffffffff;
		u32 hi = value >> 32;
#ifdef BITSTREAM_WRITER_LE
		put_bits32(s, lo);
		put_bits32(s, hi);
#else
		put_bits32(s, hi);
		put_bits32(s, lo);
#endif
	}
}

/**
 * Return the pointer to the byte where the bitstream writer will put
 * the next bit.
 */
static inline u8 *put_bits_ptr(struct put_bits_context *s)
{
	return s->buf_ptr;
}

/**
 * Skip the given number of bytes.
 * struct put_bits_context must be flushed & aligned to a byte boundary before calling this.
 */
static inline void skip_put_bytes(struct put_bits_context *s, int n)
{
	s->buf_ptr += n;
}

/**
 * Skip the given number of bits.
 * Must only be used if the actual values in the bitstream do not matter.
 * If n is 0 the behavior is undefined.
 */
static inline void skip_put_bits(struct put_bits_context *s, int n)
{
	s->bit_left -= n;
	s->buf_ptr  -= 4 * (s->bit_left >> 5);
	s->bit_left &= 31;
}

/**
 * Change the end of the buffer.
 *
 * @param size the new size in bytes of the buffer where to put bits
 */
static inline void set_put_bits_buffer_size(struct put_bits_context *s, int size)
{
	s->buf_end = s->buf + size;
	s->size_in_bits = 8*size;
}

#endif /* AVCODEC_PUT_BITS_H */

