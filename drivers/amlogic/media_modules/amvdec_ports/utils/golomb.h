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
#ifndef AVCODEC_GOLOMB_H
#define AVCODEC_GOLOMB_H

#include <linux/kernel.h>
#include <linux/types.h>

#include "get_bits.h"
#include "put_bits.h"
#include "common.h"

#define INVALID_VLC           0x80000000

extern const u8 ff_golomb_vlc_len[512];
extern const u8 ff_ue_golomb_vlc_code[512];
extern const char ff_se_golomb_vlc_code[512];
extern const u8 ff_ue_golomb_len[256];

extern const u8 ff_interleaved_golomb_vlc_len[256];
extern const u8 ff_interleaved_ue_golomb_vlc_code[256];
extern const char ff_interleaved_se_golomb_vlc_code[256];
extern const u8 ff_interleaved_dirac_golomb_vlc_code[256];

/**
 * Read an u32 Exp-Golomb code in the range 0 to 8190.
 *
 * @returns the read value or a negative error code.
 */
static inline int get_ue_golomb(struct get_bits_context *gb)
{
	u32 buf;

	OPEN_READER(re, gb);
	UPDATE_CACHE(re, gb);
	buf = GET_CACHE(re, gb);

	if (buf >= (1 << 27)) {
		buf >>= 32 - 9;
		LAST_SKIP_BITS(re, gb, ff_golomb_vlc_len[buf]);
		CLOSE_READER(re, gb);

		return ff_ue_golomb_vlc_code[buf];
	} else {
		int log = 2 * av_log2(buf) - 31;
		LAST_SKIP_BITS(re, gb, 32 - log);
		CLOSE_READER(re, gb);
		if (log < 7) {
			pr_err("Invalid UE golomb code\n");
			return -1;
		}
		buf >>= log;
		buf--;

		return buf;
	}
}

/**
 * Read an u32 Exp-Golomb code in the range 0 to UINT_MAX-1.
 */
static inline u32 get_ue_golomb_long(struct get_bits_context *gb)
{
	u32 buf, log;

	buf = show_bits_long(gb, 32);
	log = 31 - av_log2(buf);
	skip_bits_long(gb, log);

	return get_bits_long(gb, log + 1) - 1;
}

/**
 * read u32 exp golomb code, constraint to a max of 31.
 * the return value is undefined if the stored value exceeds 31.
 */
static inline int get_ue_golomb_31(struct get_bits_context *gb)
{
	u32 buf;

	OPEN_READER(re, gb);
	UPDATE_CACHE(re, gb);
	buf = GET_CACHE(re, gb);

	buf >>= 32 - 9;
	LAST_SKIP_BITS(re, gb, ff_golomb_vlc_len[buf]);
	CLOSE_READER(re, gb);

	return ff_ue_golomb_vlc_code[buf];
}

static inline u32 get_interleaved_ue_golomb(struct get_bits_context *gb)
{
	u32 buf;

	OPEN_READER(re, gb);
	UPDATE_CACHE(re, gb);
	buf = GET_CACHE(re, gb);

	if (buf & 0xAA800000) {
		buf >>= 32 - 8;
		LAST_SKIP_BITS(re, gb, ff_interleaved_golomb_vlc_len[buf]);
		CLOSE_READER(re, gb);

		return ff_interleaved_ue_golomb_vlc_code[buf];
	} else {
		u32 ret = 1;

		do {
			buf >>= 32 - 8;
			LAST_SKIP_BITS(re, gb,
			FFMIN(ff_interleaved_golomb_vlc_len[buf], 8));

			if (ff_interleaved_golomb_vlc_len[buf] != 9) {
				ret <<= (ff_interleaved_golomb_vlc_len[buf] - 1) >> 1;
				ret  |= ff_interleaved_dirac_golomb_vlc_code[buf];
				break;
			}
			ret = (ret << 4) | ff_interleaved_dirac_golomb_vlc_code[buf];
			UPDATE_CACHE(re, gb);
			buf = GET_CACHE(re, gb);
		} while (ret<0x8000000U && BITS_AVAILABLE(re, gb));

		CLOSE_READER(re, gb);
		return ret - 1;
	}
}

/**
 * read u32 truncated exp golomb code.
 */
static inline int get_te0_golomb(struct get_bits_context *gb, int range)
{
	if (range == 1)
		return 0;
	else if (range == 2)
		return get_bits1(gb) ^ 1;
	else
		return get_ue_golomb(gb);
}

/**
 * read u32 truncated exp golomb code.
 */
static inline int get_te_golomb(struct get_bits_context *gb, int range)
{
	if (range == 2)
		return get_bits1(gb) ^ 1;
	else
		return get_ue_golomb(gb);
}

/**
 * read signed exp golomb code.
 */
static inline int get_se_golomb(struct get_bits_context *gb)
{
	u32 buf;

	OPEN_READER(re, gb);
	UPDATE_CACHE(re, gb);
	buf = GET_CACHE(re, gb);

	if (buf >= (1 << 27)) {
		buf >>= 32 - 9;
		LAST_SKIP_BITS(re, gb, ff_golomb_vlc_len[buf]);
		CLOSE_READER(re, gb);

		return ff_se_golomb_vlc_code[buf];
	} else {
		int log = av_log2(buf), sign;
		LAST_SKIP_BITS(re, gb, 31 - log);
		UPDATE_CACHE(re, gb);
		buf = GET_CACHE(re, gb);

		buf >>= log;

		LAST_SKIP_BITS(re, gb, 32 - log);
		CLOSE_READER(re, gb);

		sign = -(buf & 1);
		buf  = ((buf >> 1) ^ sign) - sign;

		return buf;
	}
}

static inline int get_se_golomb_long(struct get_bits_context *gb)
{
	u32 buf = get_ue_golomb_long(gb);
	int sign = (buf & 1) - 1;

	return ((buf >> 1) ^ sign) + 1;
}

static inline int get_interleaved_se_golomb(struct get_bits_context *gb)
{
	u32 buf;

	OPEN_READER(re, gb);
	UPDATE_CACHE(re, gb);
	buf = GET_CACHE(re, gb);

	if (buf & 0xAA800000) {
		buf >>= 32 - 8;
		LAST_SKIP_BITS(re, gb, ff_interleaved_golomb_vlc_len[buf]);
		CLOSE_READER(re, gb);

		return ff_interleaved_se_golomb_vlc_code[buf];
	} else {
		int log;
		LAST_SKIP_BITS(re, gb, 8);
		UPDATE_CACHE(re, gb);
		buf |= 1 | (GET_CACHE(re, gb) >> 8);

		if ((buf & 0xAAAAAAAA) == 0)
			return INVALID_VLC;

		for (log = 31; (buf & 0x80000000) == 0; log--)
			buf = (buf << 2) - ((buf << log) >> (log - 1)) + (buf >> 30);

		LAST_SKIP_BITS(re, gb, 63 - 2 * log - 8);
		CLOSE_READER(re, gb);
		return (signed) (((((buf << log) >> log) - 1) ^ -(buf & 0x1)) + 1) >> 1;
	}
}

static inline int dirac_get_se_golomb(struct get_bits_context *gb)
{
	u32 ret = get_interleaved_ue_golomb(gb);

	if (ret) {
		int sign = -get_bits1(gb);
		ret = (ret ^ sign) - sign;
	}

	return ret;
}

/**
 * read u32 golomb rice code (ffv1).
 */
static inline int get_ur_golomb(struct get_bits_context *gb,
	int k, int limit, int esc_len)
{
	u32 buf;
	int log;

	OPEN_READER(re, gb);
	UPDATE_CACHE(re, gb);
	buf = GET_CACHE(re, gb);

	log = av_log2(buf);

	if (log > 31 - limit) {
		buf >>= log - k;
		buf  += (30U - log) << k;
		LAST_SKIP_BITS(re, gb, 32 + k - log);
		CLOSE_READER(re, gb);

		return buf;
	} else {
		LAST_SKIP_BITS(re, gb, limit);
		UPDATE_CACHE(re, gb);

		buf = SHOW_UBITS(re, gb, esc_len);

		LAST_SKIP_BITS(re, gb, esc_len);
		CLOSE_READER(re, gb);

		return buf + limit - 1;
	}
}

/**
 * read u32 golomb rice code (jpegls).
 */
static inline int get_ur_golomb_jpegls(struct get_bits_context *gb,
	int k, int limit, int esc_len)
{
	u32 buf;
	int log;

	OPEN_READER(re, gb);
	UPDATE_CACHE(re, gb);
	buf = GET_CACHE(re, gb);

	log = av_log2(buf);

	if (log - k >= 32 - MIN_CACHE_BITS + (MIN_CACHE_BITS == 32) &&
		32 - log < limit) {
		buf >>= log - k;
		buf  += (30U - log) << k;
		LAST_SKIP_BITS(re, gb, 32 + k - log);
		CLOSE_READER(re, gb);

		return buf;
	} else {
		int i;
		for (i = 0; i + MIN_CACHE_BITS <= limit && SHOW_UBITS(re, gb, MIN_CACHE_BITS) == 0; i += MIN_CACHE_BITS) {
			if (gb->size_in_bits <= re_index) {
				CLOSE_READER(re, gb);
				return -1;
			}
			LAST_SKIP_BITS(re, gb, MIN_CACHE_BITS);
			UPDATE_CACHE(re, gb);
		}
		for (; i < limit && SHOW_UBITS(re, gb, 1) == 0; i++) {
			SKIP_BITS(re, gb, 1);
		}
		LAST_SKIP_BITS(re, gb, 1);
		UPDATE_CACHE(re, gb);

		if (i < limit - 1) {
			if (k) {
				if (k > MIN_CACHE_BITS - 1) {
					buf = SHOW_UBITS(re, gb, 16) << (k-16);
					LAST_SKIP_BITS(re, gb, 16);
					UPDATE_CACHE(re, gb);
					buf |= SHOW_UBITS(re, gb, k-16);
					LAST_SKIP_BITS(re, gb, k-16);
				} else {
					buf = SHOW_UBITS(re, gb, k);
					LAST_SKIP_BITS(re, gb, k);
				}
			} else {
				buf = 0;
			}
			buf += ((u32)i << k);
		} else if (i == limit - 1) {
			buf = SHOW_UBITS(re, gb, esc_len);
			LAST_SKIP_BITS(re, gb, esc_len);

			buf ++;
		} else {
			buf = -1;
		}
		CLOSE_READER(re, gb);
		return buf;
	}
}

/**
 * read signed golomb rice code (ffv1).
 */
static inline int get_sr_golomb(struct get_bits_context *gb,
	int k, int limit, int esc_len)
{
	u32 v = get_ur_golomb(gb, k, limit, esc_len);

	return (v >> 1) ^ -(v & 1);
}

/**
 * read signed golomb rice code (flac).
 */
static inline int get_sr_golomb_flac(struct get_bits_context *gb,
	int k, int limit, int esc_len)
{
	u32 v = get_ur_golomb_jpegls(gb, k, limit, esc_len);

	return (v >> 1) ^ -(v & 1);
}

/**
 * read u32 golomb rice code (shorten).
 */
static inline u32 get_ur_golomb_shorten(struct get_bits_context *gb, int k)
{
	return get_ur_golomb_jpegls(gb, k, INT_MAX, 0);
}

/**
 * read signed golomb rice code (shorten).
 */
static inline int get_sr_golomb_shorten(struct get_bits_context *gb, int k)
{
	int uvar = get_ur_golomb_jpegls(gb, k + 1, INT_MAX, 0);

	return (uvar >> 1) ^ -(uvar & 1);
}

/**
 * write u32 exp golomb code. 2^16 - 2 at most
 */
static inline void set_ue_golomb(struct put_bits_context *pb, int i)
{
	if (i < 256)
		put_bits(pb, ff_ue_golomb_len[i], i + 1);
	else {
		int e = av_log2(i + 1);
		put_bits(pb, 2 * e + 1, i + 1);
	}
}

/**
 * write u32 exp golomb code. 2^32-2 at most.
 */
static inline void set_ue_golomb_long(struct put_bits_context *pb, u32 i)
{
	if (i < 256)
		put_bits(pb, ff_ue_golomb_len[i], i + 1);
	else {
		int e = av_log2(i + 1);
		put_bits64(pb, 2 * e + 1, i + 1);
	}
}

/**
 * write truncated u32 exp golomb code.
 */
static inline void set_te_golomb(struct put_bits_context *pb, int i, int range)
{
	if (range == 2)
		put_bits(pb, 1, i ^ 1);
	else
		set_ue_golomb(pb, i);
}

/**
 * write signed exp golomb code. 16 bits at most.
 */
static inline void set_se_golomb(struct put_bits_context *pb, int i)
{
	i = 2 * i - 1;

	if (i < 0)
		i ^= -1;    //FIXME check if gcc does the right thing
	set_ue_golomb(pb, i);
}

/**
 * write u32 golomb rice code (ffv1).
 */
static inline void set_ur_golomb(struct put_bits_context *pb, int i, int k, int limit,
                                 int esc_len)
{
	int e;

	e = i >> k;
	if (e < limit)
		put_bits(pb, e + k + 1, (1 << k) + av_mod_uintp2(i, k));
	else
		put_bits(pb, limit + esc_len, i - limit + 1);
}

/**
 * write u32 golomb rice code (jpegls).
 */
static inline void set_ur_golomb_jpegls(struct put_bits_context *pb,
	int i, int k, int limit, int esc_len)
{
	int e;

	e = (i >> k) + 1;
	if (e < limit) {
		while (e > 31) {
			put_bits(pb, 31, 0);
			e -= 31;
		}
		put_bits(pb, e, 1);
		if (k)
			put_sbits(pb, k, i);
	} else {
		while (limit > 31) {
			put_bits(pb, 31, 0);
			limit -= 31;
		}
		put_bits(pb, limit, 1);
		put_bits(pb, esc_len, i - 1);
	}
}

/**
 * write signed golomb rice code (ffv1).
 */
static inline void set_sr_golomb(struct put_bits_context *pb,
	int i, int k, int limit, int esc_len)
{
	int v;

	v  = -2 * i - 1;
	v ^= (v >> 31);

	set_ur_golomb(pb, v, k, limit, esc_len);
}

/**
 * write signed golomb rice code (flac).
 */
static inline void set_sr_golomb_flac(struct put_bits_context *pb,
	int i, int k, int limit, int esc_len)
{
	int v;

	v  = -2 * i - 1;
	v ^= (v >> 31);

	set_ur_golomb_jpegls(pb, v, k, limit, esc_len);
}

#endif /* AVCODEC_GOLOMB_H */
