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

#include "aml_vp9_parser.h"
#include "../utils/get_bits.h"
#include "../utils/put_bits.h"
#include "../utils/golomb.h"
#include "../utils/common.h"
#include "utils.h"

#define VP9_SYNCCODE 0x498342

static int read_colorspace_details(struct VP9Context *s, int profile)
{
	static const enum AVColorSpace colorspaces[8] = {
		AVCOL_SPC_UNSPECIFIED, AVCOL_SPC_BT470BG, AVCOL_SPC_BT709, AVCOL_SPC_SMPTE170M,
		AVCOL_SPC_SMPTE240M, AVCOL_SPC_BT2020_NCL, AVCOL_SPC_RESERVED, AVCOL_SPC_RGB,
	};

	enum AVColorSpace colorspace;
	int color_range;
	int bits = profile <= 1 ? 0 : 1 + get_bits1(&s->gb); // 0:8, 1:10, 2:12

	s->bpp_index = bits;
	s->s.h.bpp = 8 + bits * 2;
	s->bytesperpixel = (7 + s->s.h.bpp) >> 3;
	colorspace = colorspaces[get_bits(&s->gb, 3)];
	if (colorspace == AVCOL_SPC_RGB) { // RGB = profile 1
		if (profile & 1) {
			if (get_bits1(&s->gb)) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Reserved bit set in RGB\n");
				return -1;
			}
		} else {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "RGB not supported in profile %d\n", profile);
			return -1;
		}
	} else {
		static const enum AVPixelFormat pix_fmt_for_ss[3][2 /* v */][2 /* h */] = {
			{ { AV_PIX_FMT_YUV444P, AV_PIX_FMT_YUV422P },
			{ AV_PIX_FMT_YUV440P, AV_PIX_FMT_YUV420P } },
			{ { AV_PIX_FMT_YUV444P10, AV_PIX_FMT_YUV422P10 },
			{ AV_PIX_FMT_YUV440P10, AV_PIX_FMT_YUV420P10 } },
			{ { AV_PIX_FMT_YUV444P12, AV_PIX_FMT_YUV422P12 },
			{ AV_PIX_FMT_YUV440P12, AV_PIX_FMT_YUV420P12 } }};
		color_range = get_bits1(&s->gb) ? 2 : 1;
		if (profile & 1) {
			s->ss_h = get_bits1(&s->gb);
			s->ss_v = get_bits1(&s->gb);
			s->pix_fmt = pix_fmt_for_ss[bits][s->ss_v][s->ss_h];
			if (s->pix_fmt == AV_PIX_FMT_YUV420P) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "YUV 4:2:0 not supported in profile %d\n", profile);
				return -1;
			} else if (get_bits1(&s->gb)) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Profile %d color details reserved bit set\n", profile);
				return -1;
			}
		} else {
			s->ss_h = s->ss_v = 1;
			s->pix_fmt = pix_fmt_for_ss[bits][1][1];
		}
	}

	return 0;
}

int decode_frame_header(const u8 *data, int size, struct VP9Context *s, int *ref)
{
	int ret, last_invisible, profile;

	/* general header */
	if ((ret = init_get_bits8(&s->gb, data, size)) < 0) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Failed to initialize bitstream reader\n");
		return ret;
	}

	if (get_bits(&s->gb, 2) != 0x2) { // frame marker
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid frame marker\n");
		return -1;
	}

	profile  = get_bits1(&s->gb);
	profile |= get_bits1(&s->gb) << 1;
	if (profile == 3)
		profile += get_bits1(&s->gb);

	if (profile > 3) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Profile %d is not yet supported\n", profile);
		return -1;
	}

	s->s.h.profile = profile;
	if (get_bits1(&s->gb)) {
		*ref = get_bits(&s->gb, 3);
		return 0;
	}

	s->last_keyframe  = s->s.h.keyframe;
	s->s.h.keyframe   = !get_bits1(&s->gb);

	last_invisible   = s->s.h.invisible;
	s->s.h.invisible = !get_bits1(&s->gb);
	s->s.h.errorres  = get_bits1(&s->gb);
	s->s.h.use_last_frame_mvs = !s->s.h.errorres && !last_invisible;

	if (s->s.h.keyframe) {
		if (get_bits_long(&s->gb, 24) != VP9_SYNCCODE) { // synccode
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid sync code\n");
			return -1;
		}
		if ((ret = read_colorspace_details(s,profile)) < 0)
			return ret;
		// for profile 1, here follows the subsampling bits
		s->s.h.refreshrefmask = 0xff;
		s->width = get_bits(&s->gb, 16) + 1;
		s->height = get_bits(&s->gb, 16) + 1;
		if (get_bits1(&s->gb)) { // has scaling
			s->render_width = get_bits(&s->gb, 16) + 1;
			s->render_height = get_bits(&s->gb, 16) + 1;
		} else {
			s->render_width = s->width;
			s->render_height = s->height;
		}
		/*pr_info("keyframe res: (%d x %d), render size: (%d x %d)\n",
			s->width, s->height, s->render_width, s->render_height);*/
	} else {
		s->s.h.intraonly = s->s.h.invisible ? get_bits1(&s->gb) : 0;
		s->s.h.resetctx  = s->s.h.errorres ? 0 : get_bits(&s->gb, 2);
		if (s->s.h.intraonly) {
			if (get_bits_long(&s->gb, 24) != VP9_SYNCCODE) { // synccode
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid sync code\n");
				return -1;
			}
			if (profile >= 1) {
				if ((ret = read_colorspace_details(s, profile)) < 0)
					return ret;
			} else {
				s->ss_h = s->ss_v = 1;
				s->s.h.bpp = 8;
				s->bpp_index = 0;
				s->bytesperpixel = 1;
				s->pix_fmt = AV_PIX_FMT_YUV420P;
			}
			s->s.h.refreshrefmask = get_bits(&s->gb, 8);
			s->width = get_bits(&s->gb, 16) + 1;
			s->height = get_bits(&s->gb, 16) + 1;
			if (get_bits1(&s->gb)) { // has scaling
				s->render_width = get_bits(&s->gb, 16) + 1;
				s->render_height = get_bits(&s->gb, 16) + 1;
			} else {
				s->render_width = s->width;
				s->render_height = s->height;
			}
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "intra res: (%d x %d), render size: (%d x %d)\n",
				s->width, s->height, s->render_width, s->render_height);
		} else {
			s->s.h.refreshrefmask = get_bits(&s->gb, 8);
			s->s.h.refidx[0]      = get_bits(&s->gb, 3);
			s->s.h.signbias[0]    = get_bits1(&s->gb) && !s->s.h.errorres;
			s->s.h.refidx[1]      = get_bits(&s->gb, 3);
			s->s.h.signbias[1]    = get_bits1(&s->gb) && !s->s.h.errorres;
			s->s.h.refidx[2]      = get_bits(&s->gb, 3);
			s->s.h.signbias[2]    = get_bits1(&s->gb) && !s->s.h.errorres;

			/*refresh_frame_flags;
			for (i = 0; i < REFS_PER_FRAME; ++i) {
				frame_refs[i];
				ref_frame_sign_biases[i];
			}
			frame_size_from_refs();
			high_precision_mv;
			interp_filter();*/

			return -1;
		}
	}

	return 0;
}

int vp9_superframe_split_filter(struct vp9_superframe_split *s)
{
	int i, j, ret, marker;
	bool is_superframe = false;
	int *prefix = (int *)s->data;

	if (!s->data)
		return -1;

	#define AML_PREFIX ('V' << 24 | 'L' << 16 | 'M' << 8 | 'A')
	if (prefix[3] == AML_PREFIX) {
		s->prefix_size = 16;
		/*pr_info("the frame data has beed added header\n");*/
	}

	marker = s->data[s->data_size - 1];
	if ((marker & 0xe0) == 0xc0) {
		int length_size = 1 + ((marker >> 3) & 0x3);
		int   nb_frames = 1 + (marker & 0x7);
		int    idx_size = 2 + nb_frames * length_size;

		if (s->data_size >= idx_size &&
			s->data[s->data_size - idx_size] == marker) {
			s64 total_size = 0;
			int idx = s->data_size + 1 - idx_size;

			for (i = 0; i < nb_frames; i++) {
				int frame_size = 0;
				for (j = 0; j < length_size; j++)
					frame_size |= s->data[idx++] << (j * 8);

				total_size += frame_size;
				if (frame_size < 0 ||
					total_size > s->data_size - idx_size) {
					v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid frame size in a sframe: %d\n",
						frame_size);
					ret = -EINVAL;
					goto fail;
				}
				s->sizes[i] = frame_size;
			}

			s->nb_frames         = nb_frames;
			s->size              = total_size;
			s->next_frame        = 0;
			s->next_frame_offset = 0;
			is_superframe        = true;
		}
	}else {
		s->nb_frames = 1;
		s->sizes[0]  = s->data_size;
		s->size      = s->data_size;
	}

	/*pr_info("sframe: %d, frames: %d, IN: %x, OUT: %x\n",
		is_superframe, s->nb_frames,
		s->data_size, s->size);*/

	/* parse uncompressed header. */
	if (is_superframe) {
		/* bitstream profile. */
		/* frame type. (intra or inter) */
		/* colorspace descriptor */
		/* ... */

		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "the frame is a superframe.\n");
	}

	/*pr_err("in: %x, %d, out: %x, sizes %d,%d,%d,%d,%d,%d,%d,%d\n",
		s->data_size,
		s->nb_frames,
		s->size,
		s->sizes[0],
		s->sizes[1],
		s->sizes[2],
		s->sizes[3],
		s->sizes[4],
		s->sizes[5],
		s->sizes[6],
		s->sizes[7]);*/

	return 0;
fail:
	return ret;
}

int vp9_decode_extradata_ps(u8 *data, int size, struct vp9_param_sets *ps)
{
	int i, ref = -1, ret = 0;
	struct vp9_superframe_split s = {0};

	/*parse superframe.*/
	s.data = data;
	s.data_size = size;
	ret = vp9_superframe_split_filter(&s);
	if (ret) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "parse frames failed.\n");
		return ret;
	}

	for (i = 0; i < s.nb_frames; i++) {
		u32 len = s.sizes[i] - s.prefix_size;
		u8 *buf = s.data + s.next_frame_offset + s.prefix_size;

		ret = decode_frame_header(buf, len, &ps->ctx, &ref);
		if (!ret) {
			ps->head_parsed = ref < 0 ? true : false;
			return 0;
		}

		s.next_frame_offset = len + s.prefix_size;
	}

	return ret;
}

