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
#ifndef AVCODEC_GET_BITS_H
#define AVCODEC_GET_BITS_H

#include <linux/kernel.h>
#include <linux/types.h>
#include "common.h"

/*
 * Safe bitstream reading:
 * optionally, the get_bits API can check to ensure that we
 * don't read past input buffer boundaries. This is protected
 * with CONFIG_SAFE_BITSTREAM_READER at the global level, and
 * then below that with UNCHECKED_BITSTREAM_READER at the per-
 * decoder level. This means that decoders that check internally
 * can "#define UNCHECKED_BITSTREAM_READER 1" to disable
 * overread checks.
 * Boundary checking causes a minor performance penalty so for
 * applications that won't want/need this, it can be disabled
 * globally using "#define CONFIG_SAFE_BITSTREAM_READER 0".
 */

struct get_bits_context {
	const u8 *buffer;
	const u8 *buffer_end;
	int index;
	int size_in_bits;
	int size_in_bits_plus8;
};

/* Bitstream reader API docs:
 * name
 *   arbitrary name which is used as prefix for the internal variables
 *
 * gb
 *   struct get_bits_context
 *
 * OPEN_READER(name, gb)
 *   load gb into local variables
 *
 * CLOSE_READER(name, gb)
 *   store local vars in gb
 *
 * UPDATE_CACHE(name, gb)
 *   Refill the internal cache from the bitstream.
 *   After this call at least MIN_CACHE_BITS will be available.
 *
 * GET_CACHE(name, gb)
 *   Will output the contents of the internal cache,
 *   next bit is MSB of 32 or 64 bits (FIXME 64 bits).
 *
 * SHOW_UBITS(name, gb, num)
 *   Will return the next num bits.
 *
 * SHOW_SBITS(name, gb, num)
 *   Will return the next num bits and do sign extension.
 *
 * SKIP_BITS(name, gb, num)
 *   Will skip over the next num bits.
 *   Note, this is equivalent to SKIP_CACHE; SKIP_COUNTER.
 *
 * SKIP_CACHE(name, gb, num)
 *   Will remove the next num bits from the cache (note SKIP_COUNTER
 *   MUST be called before UPDATE_CACHE / CLOSE_READER).
 *
 * SKIP_COUNTER(name, gb, num)
 *   Will increment the internal bit counter (see SKIP_CACHE & SKIP_BITS).
 *
 * LAST_SKIP_BITS(name, gb, num)
 *   Like SKIP_BITS, to be used if next call is UPDATE_CACHE or CLOSE_READER.
 *
 * BITS_LEFT(name, gb)
 *   Return the number of bits left
 *
 * For examples see get_bits, show_bits, skip_bits, get_vlc.
 */

#define OPEN_READER_NOSIZE(name, gb)		\
	u32 name ## _index = (gb)->index;	\
	u32 name ## _cache

#define OPEN_READER(name, gb) OPEN_READER_NOSIZE(name, gb)
#define BITS_AVAILABLE(name, gb) 1

#define CLOSE_READER(name, gb) (gb)->index = name ## _index

#define UPDATE_CACHE_LE(name, gb) name ##_cache = \
      AV_RL32((gb)->buffer + (name ## _index >> 3)) >> (name ## _index & 7)

#define UPDATE_CACHE_BE(name, gb) name ## _cache = \
      AV_RB32((gb)->buffer + (name ## _index >> 3)) << (name ## _index & 7)

#define SKIP_COUNTER(name, gb, num) name ## _index += (num)

#define BITS_LEFT(name, gb) ((int)((gb)->size_in_bits - name ## _index))

#define SKIP_BITS(name, gb, num)		\
	do {					\
		SKIP_CACHE(name, gb, num);	\
		SKIP_COUNTER(name, gb, num);	\
	} while (0)

#define GET_CACHE(name, gb) ((u32) name ## _cache)

#define LAST_SKIP_BITS(name, gb, num) SKIP_COUNTER(name, gb, num)

#define SHOW_UBITS_LE(name, gb, num) zero_extend(name ## _cache, num)
#define SHOW_SBITS_LE(name, gb, num) sign_extend(name ## _cache, num)

#define SHOW_UBITS_BE(name, gb, num) NEG_USR32(name ## _cache, num)
#define SHOW_SBITS_BE(name, gb, num) NEG_SSR32(name ## _cache, num)

#ifdef BITSTREAM_READER_LE
#define UPDATE_CACHE(name, gb) UPDATE_CACHE_LE(name, gb)
#define SKIP_CACHE(name, gb, num) name ## _cache >>= (num)

#define SHOW_UBITS(name, gb, num) SHOW_UBITS_LE(name, gb, num)
#define SHOW_SBITS(name, gb, num) SHOW_SBITS_LE(name, gb, num)
#else
#define UPDATE_CACHE(name, gb) UPDATE_CACHE_BE(name, gb)
#define SKIP_CACHE(name, gb, num) name ## _cache <<= (num)

#define SHOW_UBITS(name, gb, num) SHOW_UBITS_BE(name, gb, num)
#define SHOW_SBITS(name, gb, num) SHOW_SBITS_BE(name, gb, num)
#endif

static inline const int sign_extend(int val, u32 bits)
{
	u32 shift = 8 * sizeof(int) - bits;

	union { u32 u; int s; } v = { (u32) val << shift };
	return v.s >> shift;
}

static inline u32 zero_extend(u32 val, u32 bits)
{
	return (val << ((8 * sizeof(int)) - bits)) >> ((8 * sizeof(int)) - bits);
}

static inline int get_bits_count(const struct get_bits_context *s)
{
	return s->index;
}

/**
 * Skips the specified number of bits.
 * @param n the number of bits to skip,
 *          For the UNCHECKED_BITSTREAM_READER this must not cause the distance
 *          from the start to overflow int. Staying within the bitstream + padding
 *          is sufficient, too.
 */
static inline void skip_bits_long(struct get_bits_context *s, int n)
{
	s->index += n;
}

/**
 * Read MPEG-1 dc-style VLC (sign bit + mantissa with no MSB).
 * if MSB not set it is negative
 * @param n length in bits
 */
static inline int get_xbits(struct get_bits_context *s, int n)
{
	register int sign;
	register int cache;

	OPEN_READER(re, s);
	UPDATE_CACHE(re, s);
	cache = GET_CACHE(re, s);
	sign  = ~cache >> 31;
	LAST_SKIP_BITS(re, s, n);
	CLOSE_READER(re, s);

	return (NEG_USR32(sign ^ cache, n) ^ sign) - sign;
}


static inline int get_xbits_le(struct get_bits_context *s, int n)
{
	register int sign;
	register int cache;

	OPEN_READER(re, s);
	UPDATE_CACHE_LE(re, s);
	cache = GET_CACHE(re, s);
	sign  = sign_extend(~cache, n) >> 31;
	LAST_SKIP_BITS(re, s, n);
	CLOSE_READER(re, s);

	return (zero_extend(sign ^ cache, n) ^ sign) - sign;
}

static inline int get_sbits(struct get_bits_context *s, int n)
{
	register int tmp;

	OPEN_READER(re, s);
	UPDATE_CACHE(re, s);
	tmp = SHOW_SBITS(re, s, n);
	LAST_SKIP_BITS(re, s, n);
	CLOSE_READER(re, s);

	return tmp;
}

/**
 * Read 1-25 bits.
 */
static inline u32 get_bits(struct get_bits_context *s, int n)
{
	register u32 tmp;

	OPEN_READER(re, s);
	UPDATE_CACHE(re, s);
	tmp = SHOW_UBITS(re, s, n);
	LAST_SKIP_BITS(re, s, n);
	CLOSE_READER(re, s);

	return tmp;
}

/**
 * Read 0-25 bits.
 */
static inline int get_bitsz(struct get_bits_context *s, int n)
{
	return n ? get_bits(s, n) : 0;
}

static inline u32 get_bits_le(struct get_bits_context *s, int n)
{
	register int tmp;

	OPEN_READER(re, s);
	UPDATE_CACHE_LE(re, s);
	tmp = SHOW_UBITS_LE(re, s, n);
	LAST_SKIP_BITS(re, s, n);
	CLOSE_READER(re, s);

	return tmp;
}

/**
 * Show 1-25 bits.
 */
static inline u32 show_bits(struct get_bits_context *s, int n)
{
	register u32 tmp;

	OPEN_READER_NOSIZE(re, s);
	UPDATE_CACHE(re, s);
	tmp = SHOW_UBITS(re, s, n);

	return tmp;
}

static inline void skip_bits(struct get_bits_context *s, int n)
{
	u32 re_index = s->index;
	LAST_SKIP_BITS(re, s, n);
	CLOSE_READER(re, s);
}

static inline u32 get_bits1(struct get_bits_context *s)
{
	u32 index = s->index;
	u8 result = s->buffer[index >> 3];

#ifdef BITSTREAM_READER_LE
	result >>= index & 7;
	result  &= 1;
#else
	result <<= index & 7;
	result >>= 8 - 1;
#endif

	index++;
	s->index = index;

	return result;
}

static inline u32 show_bits1(struct get_bits_context *s)
{
	return show_bits(s, 1);
}

static inline void skip_bits1(struct get_bits_context *s)
{
	skip_bits(s, 1);
}

/**
 * Read 0-32 bits.
 */
static inline u32 get_bits_long(struct get_bits_context *s, int n)
{
	if (!n) {
		return 0;
	} else if (n <= MIN_CACHE_BITS) {
		return get_bits(s, n);
	} else {
#ifdef BITSTREAM_READER_LE
		u32 ret = get_bits(s, 16);
		return ret | (get_bits(s, n - 16) << 16);
#else
		u32 ret = get_bits(s, 16) << (n - 16);
		return ret | get_bits(s, n - 16);
#endif
	}
}

/**
 * Read 0-64 bits.
 */
static inline u64 get_bits64(struct get_bits_context *s, int n)
{
	if (n <= 32) {
		return get_bits_long(s, n);
	} else {
#ifdef BITSTREAM_READER_LE
		u64 ret = get_bits_long(s, 32);
		return ret | (u64) get_bits_long(s, n - 32) << 32;
#else
		u64 ret = (u64) get_bits_long(s, n - 32) << 32;
		return ret | get_bits_long(s, 32);
#endif
	}
}

/**
 * Read 0-32 bits as a signed integer.
 */
static inline int get_sbits_long(struct get_bits_context *s, int n)
{
	if (!n)
		return 0;

	return sign_extend(get_bits_long(s, n), n);
}

/**
 * Show 0-32 bits.
 */
static inline u32 show_bits_long(struct get_bits_context *s, int n)
{
	if (n <= MIN_CACHE_BITS) {
		return show_bits(s, n);
	} else {
		struct get_bits_context gb = *s;

		return get_bits_long(&gb, n);
	}
}

static inline int check_marker(struct get_bits_context *s, const char *msg)
{
	int bit = get_bits1(s);

	if (!bit)
		pr_err("Marker bit missing at %d of %d %s\n",
			get_bits_count(s) - 1, s->size_in_bits, msg);
	return bit;
}

static inline int init_get_bits_xe(struct get_bits_context *s,
	const u8 *buffer, int bit_size, int is_le)
{
	int buffer_size;
	int ret = 0;

	if (bit_size >= INT_MAX - FFMAX(7, AV_INPUT_BUFFER_PADDING_SIZE * 8) ||
		bit_size < 0 || !buffer) {
		bit_size    = 0;
		buffer      = NULL;
		ret         = -1;
	}

	buffer_size = (bit_size + 7) >> 3;

	s->buffer             = buffer;
	s->size_in_bits       = bit_size;
	s->size_in_bits_plus8 = bit_size + 8;
	s->buffer_end         = buffer + buffer_size;
	s->index              = 0;

	return ret;
}

/**
 * Initialize struct get_bits_context.
 * @param buffer bitstream buffer, must be AV_INPUT_BUFFER_PADDING_SIZE bytes
 *        larger than the actual read bits because some optimized bitstream
 *        readers read 32 or 64 bit at once and could read over the end
 * @param bit_size the size of the buffer in bits
 * @return 0 on success, -1 if the buffer_size would overflow.
 */
static inline int init_get_bits(struct get_bits_context *s,
	const u8 *buffer, int bit_size)
{
#ifdef BITSTREAM_READER_LE
	return init_get_bits_xe(s, buffer, bit_size, 1);
#else
	return init_get_bits_xe(s, buffer, bit_size, 0);
#endif
}

/**
 * Initialize struct get_bits_context.
 * @param buffer bitstream buffer, must be AV_INPUT_BUFFER_PADDING_SIZE bytes
 *        larger than the actual read bits because some optimized bitstream
 *        readers read 32 or 64 bit at once and could read over the end
 * @param byte_size the size of the buffer in bytes
 * @return 0 on success, -1 if the buffer_size would overflow.
 */
static inline int init_get_bits8(struct get_bits_context *s,
	const u8 *buffer, int byte_size)
{
	if (byte_size > INT_MAX / 8 || byte_size < 0)
		byte_size = -1;
	return init_get_bits(s, buffer, byte_size * 8);
}

static inline int init_get_bits8_le(struct get_bits_context *s,
	const u8 *buffer, int byte_size)
{
	if (byte_size > INT_MAX / 8 || byte_size < 0)
		byte_size = -1;
	return init_get_bits_xe(s, buffer, byte_size * 8, 1);
}

static inline const u8 *align_get_bits(struct get_bits_context *s)
{
	int n = -get_bits_count(s) & 7;

	if (n)
		skip_bits(s, n);
	return s->buffer + (s->index >> 3);
}

/**
 * If the vlc code is invalid and max_depth=1, then no bits will be removed.
 * If the vlc code is invalid and max_depth>1, then the number of bits removed
 * is undefined.
 */
#define GET_VLC(code, name, gb, table, bits, max_depth)         \
    do {                                                        \
        int n, nb_bits;                                         \
        u32 index;                                              \
                                                                \
        index = SHOW_UBITS(name, gb, bits);                     \
        code  = table[index][0];                                \
        n     = table[index][1];                                \
                                                                \
        if (max_depth > 1 && n < 0) {                           \
            LAST_SKIP_BITS(name, gb, bits);                     \
            UPDATE_CACHE(name, gb);                             \
                                                                \
            nb_bits = -n;                                       \
                                                                \
            index = SHOW_UBITS(name, gb, nb_bits) + code;       \
            code  = table[index][0];                            \
            n     = table[index][1];                            \
            if (max_depth > 2 && n < 0) {                       \
                LAST_SKIP_BITS(name, gb, nb_bits);              \
                UPDATE_CACHE(name, gb);                         \
                                                                \
                nb_bits = -n;                                   \
                                                                \
                index = SHOW_UBITS(name, gb, nb_bits) + code;   \
                code  = table[index][0];                        \
                n     = table[index][1];                        \
            }                                                   \
        }                                                       \
        SKIP_BITS(name, gb, n);                                 \
    } while (0)

#define GET_RL_VLC(level, run, name, gb, table, bits,           \
                   max_depth, need_update)                      \
    do {                                                        \
        int n, nb_bits;                                         \
        u32 index;                                              \
                                                                \
        index = SHOW_UBITS(name, gb, bits);                     \
        level = table[index].level;                             \
        n     = table[index].len;                               \
                                                                \
        if (max_depth > 1 && n < 0) {                           \
            SKIP_BITS(name, gb, bits);                          \
            if (need_update) {                                  \
                UPDATE_CACHE(name, gb);                         \
            }                                                   \
                                                                \
            nb_bits = -n;                                       \
                                                                \
            index = SHOW_UBITS(name, gb, nb_bits) + level;      \
            level = table[index].level;                         \
            n     = table[index].len;                           \
            if (max_depth > 2 && n < 0) {                       \
                LAST_SKIP_BITS(name, gb, nb_bits);              \
                if (need_update) {                              \
                    UPDATE_CACHE(name, gb);                     \
                }                                               \
                nb_bits = -n;                                   \
                                                                \
                index = SHOW_UBITS(name, gb, nb_bits) + level;  \
                level = table[index].level;                     \
                n     = table[index].len;                       \
            }                                                   \
        }                                                       \
        run = table[index].run;                                 \
        SKIP_BITS(name, gb, n);                                 \
    } while (0)

/* Return the LUT element for the given bitstream configuration. */
static inline int set_idx(struct get_bits_context *s,
	int code, int *n, int *nb_bits, int (*table)[2])
{
	u32 idx;

	*nb_bits = -*n;
	idx = show_bits(s, *nb_bits) + code;
	*n = table[idx][1];

	return table[idx][0];
}

/**
 * Parse a vlc code.
 * @param bits is the number of bits which will be read at once, must be
 *             identical to nb_bits in init_vlc()
 * @param max_depth is the number of times bits bits must be read to completely
 *                  read the longest vlc code
 *                  = (max_vlc_length + bits - 1) / bits
 * @returns the code parsed or -1 if no vlc matches
 */
static inline int get_vlc2(struct get_bits_context *s,
	int (*table)[2], int bits, int max_depth)
{
	int code;

	OPEN_READER(re, s);
	UPDATE_CACHE(re, s);

	GET_VLC(code, re, s, table, bits, max_depth);

	CLOSE_READER(re, s);

	return code;
}

static inline int decode012(struct get_bits_context *gb)
{
	int n;

	n = get_bits1(gb);
	if (n == 0)
		return 0;
	else
		return get_bits1(gb) + 1;
}

static inline int decode210(struct get_bits_context *gb)
{
	if (get_bits1(gb))
		return 0;
	else
		return 2 - get_bits1(gb);
}

static inline int get_bits_left(struct get_bits_context *gb)
{
	return gb->size_in_bits - get_bits_count(gb);
}

static inline int skip_1stop_8data_bits(struct get_bits_context *gb)
{
	if (get_bits_left(gb) <= 0)
	return -1;

	while (get_bits1(gb)) {
		skip_bits(gb, 8);
		if (get_bits_left(gb) <= 0)
			return -1;
	}

	return 0;
}

#endif /* AVCODEC_GET_BITS_H */

