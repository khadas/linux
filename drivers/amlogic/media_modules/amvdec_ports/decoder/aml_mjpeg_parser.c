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
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/string.h>

#include "aml_mjpeg_parser.h"
#include "../utils/get_bits.h"
#include "../utils/put_bits.h"
#include "../utils/golomb.h"
#include "../utils/common.h"
#include "utils.h"

/* return the 8 bit start code value and update the search
state. Return -1 if no start code found */
static int find_marker(const u8 **pbuf_ptr, const u8 *buf_end)
{
	const u8 *buf_ptr;
	u32 v, v2;
	int val;
	int skipped = 0;

	buf_ptr = *pbuf_ptr;
	while (buf_end - buf_ptr > 1) {
		v  = *buf_ptr++;
		v2 = *buf_ptr;
		if ((v == 0xff) && (v2 >= 0xc0) && (v2 <= 0xfe) && buf_ptr < buf_end) {
			val = *buf_ptr++;
			goto found;
		}
		skipped++;
	}
	buf_ptr = buf_end;
	val = -1;
found:
	v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "find_marker skipped %d bytes\n", skipped);
	*pbuf_ptr = buf_ptr;

	return val;
}

int ff_mjpeg_find_marker(struct MJpegDecodeContext *s,
	const u8 **buf_ptr, const u8 *buf_end,
	const u8 **unescaped_buf_ptr,
	int *unescaped_buf_size)
{
	int start_code;

	start_code = find_marker(buf_ptr, buf_end);

	/* unescape buffer of SOS, use special treatment for JPEG-LS */
	if (start_code == SOS && !s->ls) {
		const u8 *src = *buf_ptr;
		const u8 *ptr = src;
		u8 *dst = s->buffer;

		#define copy_data_segment(skip) do {			\
				int length = (ptr - src) - (skip);	\
				if (length > 0) {			\
					memcpy(dst, src, length);	\
					dst += length;			\
					src = ptr;			\
				}					\
			} while (0)


		while (ptr < buf_end) {
			u8 x = *(ptr++);

			if (x == 0xff) {
				int skip = 0;
				while (ptr < buf_end && x == 0xff) {
					x = *(ptr++);
					skip++;
				}

				/* 0xFF, 0xFF, ... */
				if (skip > 1) {
					copy_data_segment(skip);

					/* decrement src as it is equal to ptr after the
					* copy_data_segment macro and we might want to
					* copy the current value of x later on */
					src--;
				}

				if (x < 0xd0 || x > 0xd7) {
					copy_data_segment(1);
					if (x)
						break;
				}
			}
			if (src < ptr)
				copy_data_segment(0);
		}
		#undef copy_data_segment

		*unescaped_buf_ptr  = s->buffer;
		*unescaped_buf_size = dst - s->buffer;
		memset(s->buffer + *unescaped_buf_size, 0,
			AV_INPUT_BUFFER_PADDING_SIZE);

		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "escaping removed %d bytes\n",
			(int)((buf_end - *buf_ptr) - (dst - s->buffer)));
	} else if (start_code == SOS && s->ls) {
		const u8 *src = *buf_ptr;
		u8 *dst  = s->buffer;
		int bit_count = 0;
		int t = 0, b = 0;
		struct put_bits_context pb;

		/* find marker */
		while (src + t < buf_end) {
			u8 x = src[t++];
			if (x == 0xff) {
				while ((src + t < buf_end) && x == 0xff)
					x = src[t++];
				if (x & 0x80) {
					t -= FFMIN(2, t);
					break;
				}
			}
		}
		bit_count = t * 8;
		init_put_bits(&pb, dst, t);

		/* unescape bitstream */
		while (b < t) {
			u8 x = src[b++];
			put_bits(&pb, 8, x);
			if (x == 0xFF && b < t) {
				x = src[b++];
				if (x & 0x80) {
					v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid escape sequence\n");
					x &= 0x7f;
				}
				put_bits(&pb, 7, x);
				bit_count--;
			}
		}
		flush_put_bits(&pb);

		*unescaped_buf_ptr	= dst;
		*unescaped_buf_size = (bit_count + 7) >> 3;
		memset(s->buffer + *unescaped_buf_size, 0,
			AV_INPUT_BUFFER_PADDING_SIZE);
	} else {
		*unescaped_buf_ptr	= *buf_ptr;
		*unescaped_buf_size = buf_end - *buf_ptr;
	}

	return start_code;
}


int ff_mjpeg_decode_sof(struct MJpegDecodeContext *s)
{
	int len, nb_components, i, width, height, bits, size_change;
	int h_count[MAX_COMPONENTS] = { 0 };
	int v_count[MAX_COMPONENTS] = { 0 };

	s->cur_scan = 0;
	memset(s->upscale_h, 0, sizeof(s->upscale_h));
	memset(s->upscale_v, 0, sizeof(s->upscale_v));

	/* XXX: verify len field validity */
	len     = get_bits(&s->gb, 16);
	bits    = get_bits(&s->gb, 8);

	if (bits > 16 || bits < 1) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "bits %d is invalid\n", bits);
		return -1;
	}

	height = get_bits(&s->gb, 16);
	width  = get_bits(&s->gb, 16);

	v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "sof0: picture: %dx%d\n", width, height);

	nb_components = get_bits(&s->gb, 8);
	if (nb_components <= 0 ||
		nb_components > MAX_COMPONENTS)
		return -1;

	s->nb_components = nb_components;
	s->h_max         = 1;
	s->v_max         = 1;
	for (i = 0; i < nb_components; i++) {
		/* component id */
		s->component_id[i] = get_bits(&s->gb, 8) - 1;
		h_count[i]         = get_bits(&s->gb, 4);
		v_count[i]         = get_bits(&s->gb, 4);
		/* compute hmax and vmax (only used in interleaved case) */
		if (h_count[i] > s->h_max)
			s->h_max = h_count[i];
		if (v_count[i] > s->v_max)
			s->v_max = v_count[i];
		s->quant_index[i] = get_bits(&s->gb, 8);
		if (s->quant_index[i] >= 4) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "quant_index is invalid\n");
			return -1;
		}
		if (!h_count[i] || !v_count[i]) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid sampling factor in component %d %d:%d\n",
				i, h_count[i], v_count[i]);
			return -1;
		}

		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "component %d %d:%d id: %d quant:%d\n",
			i, h_count[i], v_count[i],
		s->component_id[i], s->quant_index[i]);
	}
	if (nb_components == 4
		&& s->component_id[0] == 'C' - 1
		&& s->component_id[1] == 'M' - 1
		&& s->component_id[2] == 'Y' - 1
		&& s->component_id[3] == 'K' - 1)
		s->adobe_transform = 0;

	/* if different size, realloc/alloc picture */
	if (width != s->width || height != s->height || bits != s->bits ||
		memcmp(s->h_count, h_count, sizeof(h_count))                ||
		memcmp(s->v_count, v_count, sizeof(v_count))) {
		size_change = 1;

		s->width      = width;
		s->height     = height;
		s->bits       = bits;
		memcpy(s->h_count, h_count, sizeof(h_count));
		memcpy(s->v_count, v_count, sizeof(v_count));
		s->interlaced = 0;
		s->got_picture = 0;
	} else {
		size_change = 0;
	}

	return 0;
}

static int ff_mjpeg_decode_frame(u8 *buf, int buf_size, struct MJpegDecodeContext *s)
{
	const u8 *buf_end, *buf_ptr;
	const u8 *unescaped_buf_ptr;
	int unescaped_buf_size;
	int start_code;
	int ret = 0;

	buf_ptr = buf;
	buf_end = buf + buf_size;
	while (buf_ptr < buf_end) {
		/* find start next marker */
		start_code = ff_mjpeg_find_marker(s, &buf_ptr, buf_end,
						&unescaped_buf_ptr,
						&unescaped_buf_size);
		/* EOF */
		if (start_code < 0) {
			break;
		} else if (unescaped_buf_size > INT_MAX / 8) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "MJPEG packet 0x%x too big (%d/%d), corrupt data?\n",
				start_code, unescaped_buf_size, buf_size);
			return -1;
		}
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "marker=%x avail_size_in_buf=%d\n",
			start_code, (int)(buf_end - buf_ptr));

		ret = init_get_bits8(&s->gb, unescaped_buf_ptr, unescaped_buf_size);
		if (ret < 0) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "invalid buffer\n");
			goto fail;
		}

		s->start_code = start_code;
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "startcode: %X\n", start_code);

		switch (start_code) {
		case SOF0:
		case SOF1:
		case SOF2:
		case SOF3:
		case SOF48:
		case SOI:
		case SOS:
		case EOI:
			break;
		default:
			goto skip;
		}

		switch (start_code) {
		case SOI:
			s->restart_interval = 0;
			s->restart_count    = 0;
			s->raw_image_buffer      = buf_ptr;
			s->raw_image_buffer_size = buf_end - buf_ptr;
			/* nothing to do on SOI */
			break;
		case SOF0:
		case SOF1:
			if (start_code == SOF0)
				s->profile = FF_PROFILE_MJPEG_HUFFMAN_BASELINE_DCT;
			else
				s->profile = FF_PROFILE_MJPEG_HUFFMAN_EXTENDED_SEQUENTIAL_DCT;
			s->lossless    = 0;
			s->ls          = 0;
			s->progressive = 0;
			if ((ret = ff_mjpeg_decode_sof(s)) < 0)
				goto fail;
			break;
		case SOF2:
			s->profile = FF_PROFILE_MJPEG_HUFFMAN_PROGRESSIVE_DCT;
			s->lossless    = 0;
			s->ls          = 0;
			s->progressive = 1;
			if ((ret = ff_mjpeg_decode_sof(s)) < 0)
				goto fail;
			break;
		case SOF3:
			s->profile     = FF_PROFILE_MJPEG_HUFFMAN_LOSSLESS;
			s->properties |= FF_CODEC_PROPERTY_LOSSLESS;
			s->lossless    = 1;
			s->ls          = 0;
			s->progressive = 0;
			if ((ret = ff_mjpeg_decode_sof(s)) < 0)
				goto fail;
			break;
		case SOF48:
			s->profile     = FF_PROFILE_MJPEG_JPEG_LS;
			s->properties |= FF_CODEC_PROPERTY_LOSSLESS;
			s->lossless    = 1;
			s->ls          = 1;
			s->progressive = 0;
			if ((ret = ff_mjpeg_decode_sof(s)) < 0)
				goto fail;
			break;
		case EOI:
			goto the_end;
		case DHT:
		case LSE:
		case SOS:
		case DRI:
		case SOF5:
		case SOF6:
		case SOF7:
		case SOF9:
		case SOF10:
		case SOF11:
		case SOF13:
		case SOF14:
		case SOF15:
		case JPG:
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "mjpeg: unsupported coding type (%x)\n", start_code);
			break;
		}
skip:
		/* eof process start code */
		buf_ptr += (get_bits_count(&s->gb) + 7) / 8;
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "marker parser used %d bytes (%d bits)\n",
			(get_bits_count(&s->gb) + 7) / 8, get_bits_count(&s->gb));
	}

	v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "No JPEG data found in image\n");
	return -1;
fail:
	s->got_picture = 0;
	return ret;
the_end:
	v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "decode frame unused %d bytes\n", (int)(buf_end - buf_ptr));

	return 0;
}

int mjpeg_decode_extradata_ps(u8 *buf, int size, struct mjpeg_param_sets *ps)
{
	int ret;

	ps->head_parsed = false;

	ps->dec_ps.buf_size = size;
	ps->dec_ps.buffer = vzalloc(size + AV_INPUT_BUFFER_PADDING_SIZE);
	if (!ps->dec_ps.buffer)
	    return -1;

	ret = ff_mjpeg_decode_frame(buf, size, &ps->dec_ps);
	if (ret) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "parse extra data failed. err: %d\n", ret);
		vfree(ps->dec_ps.buffer);
		return ret;
	}

	if (ps->dec_ps.width && ps->dec_ps.height)
		ps->head_parsed = true;

	vfree(ps->dec_ps.buffer);

	return 0;
}

