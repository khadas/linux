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

#include "aml_mpeg12_parser.h"
#include "../utils/get_bits.h"
#include "../utils/put_bits.h"
#include "../utils/golomb.h"
#include "../utils/common.h"
#include "utils.h"

const struct AVRational ff_mpeg12_frame_rate_tab[16] = {
	{    0,    0},
	{24000, 1001},
	{   24,    1},
	{   25,    1},
	{30000, 1001},
	{   30,    1},
	{   50,    1},
	{60000, 1001},
	{   60,    1},
	// Xing's 15fps: (9)
	{   15,    1},
	// libmpeg3's "Unofficial economy rates": (10-13)
	{    5,    1},
	{   10,    1},
	{   12,    1},
	{   15,    1},
	{    0,    0},
};

const u8 *avpriv_find_start_code(const u8 *p, const u8 *end, u32 *state)
{
	int i;

	if (p >= end)
		return end;

	for (i = 0; i < 3; i++) {
		u32 tmp = *state << 8;
		*state = tmp + *(p++);
		if (tmp == 0x100 || p == end)
			return p;
	}

	while (p < end) {
		if      (p[-1] > 1      ) p += 3;
		else if (p[-2]          ) p += 2;
		else if (p[-3]|(p[-1]-1)) p++;
		else {
			p++;
			break;
		}
	}

	p = FFMIN(p, end) - 4;
	*state = AV_RB32(p);

	return p + 4;
}

static void mpegvideo_extract_headers(const u8 *buf, int buf_size,
	struct mpeg12_param_sets *ps)
{
	struct MpvParseContext *pc = &ps->dec_ps;
	const u8 *buf_end = buf + buf_size;
	u32 start_code;
	int frame_rate_index, ext_type, bytes_left;
	int frame_rate_ext_n, frame_rate_ext_d;
	int top_field_first, repeat_first_field, progressive_frame;
	int horiz_size_ext, vert_size_ext, bit_rate_ext;
	int bit_rate = 0;
	int vbv_delay = 0;
	int chroma_format;
	enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
	//FIXME replace the crap with get_bits()
	pc->repeat_pict = 0;

	while (buf < buf_end) {
		start_code= -1;
		buf= avpriv_find_start_code(buf, buf_end, &start_code);
		bytes_left = buf_end - buf;
		switch (start_code) {
		case PICTURE_START_CODE:
			if (bytes_left >= 2) {
				pc->pict_type = (buf[1] >> 3) & 7;
				if (bytes_left >= 4)
					vbv_delay = ((buf[1] & 0x07) << 13) | (buf[2] << 5) | (buf[3] >> 3);
			}
			break;
		case SEQ_START_CODE:
			if (bytes_left >= 7) {
				pc->width  = (buf[0] << 4) | (buf[1] >> 4);
				pc->height = ((buf[1] & 0x0f) << 8) | buf[2];

				pix_fmt = AV_PIX_FMT_YUV420P;
				frame_rate_index = buf[3] & 0xf;
				pc->frame_rate = ff_mpeg12_frame_rate_tab[frame_rate_index];
				bit_rate = (buf[4]<<10) | (buf[5]<<2) | (buf[6]>>6);
				pc->ticks_per_frame = 1;
			}
			break;
		case EXT_START_CODE:
			if (bytes_left >= 1) {
				ext_type = (buf[0] >> 4);
				switch (ext_type) {
				case 0x1: /* sequence extension */
					if (bytes_left >= 6) {
						horiz_size_ext = ((buf[1] & 1) << 1) | (buf[2] >> 7);
						vert_size_ext = (buf[2] >> 5) & 3;
						bit_rate_ext = ((buf[2] & 0x1F)<<7) | (buf[3]>>1);
						frame_rate_ext_n = (buf[5] >> 5) & 3;
						frame_rate_ext_d = (buf[5] & 0x1f);
						pc->progressive_sequence = buf[1] & (1 << 3);
						pc->has_b_frames= !(buf[5] >> 7);

						chroma_format = (buf[1] >> 1) & 3;
						switch (chroma_format) {
						case 1: pix_fmt = AV_PIX_FMT_YUV420P; break;
						case 2: pix_fmt = AV_PIX_FMT_YUV422P; break;
						case 3: pix_fmt = AV_PIX_FMT_YUV444P; break;
						}

						pc->width  = (pc->width & 0xFFF) | (horiz_size_ext << 12);
						pc->height = (pc->height& 0xFFF) | ( vert_size_ext << 12);
						bit_rate = (bit_rate&0x3FFFF) | (bit_rate_ext << 18);
						//if(did_set_size)
						//set_dim_ret = ff_set_dimensions(avctx, pc->width, pc->height);
						pc->framerate.num = pc->frame_rate.num * (frame_rate_ext_n + 1);
						pc->framerate.den = pc->frame_rate.den * (frame_rate_ext_d + 1);
						pc->ticks_per_frame = 2;
					}
					break;
				case 0x8: /* picture coding extension */
					if (bytes_left >= 5) {
						top_field_first = buf[3] & (1 << 7);
						repeat_first_field = buf[3] & (1 << 1);
						progressive_frame = buf[4] & (1 << 7);

						/* check if we must repeat the frame */
						pc->repeat_pict = 1;
						if (repeat_first_field) {
							if (pc->progressive_sequence) {
								if (top_field_first)
									pc->repeat_pict = 5;
								else
									pc->repeat_pict = 3;
							} else if (progressive_frame) {
								pc->repeat_pict = 2;
							}
						}

						if (!pc->progressive_sequence && !progressive_frame) {
							if (top_field_first)
								pc->field_order = AV_FIELD_TT;
							else
								pc->field_order = AV_FIELD_BB;
						} else
							pc->field_order = AV_FIELD_PROGRESSIVE;
					}
					break;
				}
			}
			break;
		case -1:
			goto the_end;
		default:
			/* we stop parsing when we encounter a slice. It ensures
			that this function takes a negligible amount of time */
			if (start_code >= SLICE_MIN_START_CODE &&
				start_code <= SLICE_MAX_START_CODE)
				goto the_end;
			break;
		}
	}
the_end:

	if (pix_fmt != AV_PIX_FMT_NONE) {
		pc->format = pix_fmt;
		pc->coded_width  = ALIGN(pc->width,  16);
		pc->coded_height = ALIGN(pc->height, 16);
	}
}

int mpeg12_decode_extradata_ps(u8 *buf, int size, struct mpeg12_param_sets *ps)
{
	ps->head_parsed = false;

	mpegvideo_extract_headers(buf, size, ps);

	if (ps->dec_ps.width && ps->dec_ps.height)
		ps->head_parsed = true;

	return 0;
}

