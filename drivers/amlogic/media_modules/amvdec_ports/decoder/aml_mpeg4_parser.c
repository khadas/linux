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

#include "aml_mpeg4_parser.h"
#include "../utils/get_bits.h"
#include "../utils/put_bits.h"
#include "../utils/golomb.h"
#include "../utils/common.h"
#include "utils.h"

const u8 ff_mpeg4_dc_threshold[8]={
    99, 13, 15, 17, 19, 21, 23, 0
};

/* these matrixes will be permuted for the idct */
const int16_t ff_mpeg4_default_intra_matrix[64] = {
	 8, 17, 18, 19, 21, 23, 25, 27,
	17, 18, 19, 21, 23, 25, 27, 28,
	20, 21, 22, 23, 24, 26, 28, 30,
	21, 22, 23, 24, 26, 28, 30, 32,
	22, 23, 24, 26, 28, 30, 32, 35,
	23, 24, 26, 28, 30, 32, 35, 38,
	25, 26, 28, 30, 32, 35, 38, 41,
	27, 28, 30, 32, 35, 38, 41, 45,
};

const int16_t ff_mpeg4_default_non_intra_matrix[64] = {
	16, 17, 18, 19, 20, 21, 22, 23,
	17, 18, 19, 20, 21, 22, 23, 24,
	18, 19, 20, 21, 22, 23, 24, 25,
	19, 20, 21, 22, 23, 24, 26, 27,
	20, 21, 22, 23, 25, 26, 27, 28,
	21, 22, 23, 24, 26, 27, 28, 30,
	22, 23, 24, 26, 27, 28, 30, 31,
	23, 24, 25, 27, 28, 30, 31, 33,
};

const struct AVRational ff_h263_pixel_aspect[16] = {
	{  0,  1 },
	{  1,  1 },
	{ 12, 11 },
	{ 10, 11 },
	{ 16, 11 },
	{ 40, 33 },
	{  0,  1 },
	{  0,  1 },
	{  0,  1 },
	{  0,  1 },
	{  0,  1 },
	{  0,  1 },
	{  0,  1 },
	{  0,  1 },
	{  0,  1 },
	{  0,  1 },
};

/* As per spec, studio start code search isn't the same as the old type of start code */
static void next_start_code_studio(struct get_bits_context *gb)
{
	align_get_bits(gb);

	while (get_bits_left(gb) >= 24 && show_bits_long(gb, 24) != 0x1) {
		get_bits(gb, 8);
	}
}

static int read_quant_matrix_ext(struct MpegEncContext *s, struct get_bits_context *gb)
{
	int i, /*j,*/ v;

	if (get_bits1(gb)) {
		if (get_bits_left(gb) < 64*8)
			return -1;
		/* intra_quantiser_matrix */
		for (i = 0; i < 64; i++) {
			v = get_bits(gb, 8);
			//j = s->idsp.idct_permutation[ff_zigzag_direct[i]];
			//s->intra_matrix[j]        = v;
			//s->chroma_intra_matrix[j] = v;
		}
	}

	if (get_bits1(gb)) {
		if (get_bits_left(gb) < 64*8)
			return -1;
		/* non_intra_quantiser_matrix */
		for (i = 0; i < 64; i++) {
			get_bits(gb, 8);
		}
	}

	if (get_bits1(gb)) {
		if (get_bits_left(gb) < 64*8)
			return -1;
		/* chroma_intra_quantiser_matrix */
		for (i = 0; i < 64; i++) {
			v = get_bits(gb, 8);
			//j = s->idsp.idct_permutation[ff_zigzag_direct[i]];
			//s->chroma_intra_matrix[j] = v;
		}
	}

	if (get_bits1(gb)) {
		if (get_bits_left(gb) < 64*8)
			return -1;
		/* chroma_non_intra_quantiser_matrix */
		for (i = 0; i < 64; i++) {
			get_bits(gb, 8);
		}
	}

	next_start_code_studio(gb);
	return 0;
}

static void extension_and_user_data(struct MpegEncContext *s, struct get_bits_context *gb, int id)
{
	u32 startcode;
	u8 extension_type;

	startcode = show_bits_long(gb, 32);
	if (startcode == USER_DATA_STARTCODE || startcode == EXT_STARTCODE) {
		if ((id == 2 || id == 4) && startcode == EXT_STARTCODE) {
			skip_bits_long(gb, 32);
			extension_type = get_bits(gb, 4);
			if (extension_type == QUANT_MATRIX_EXT_ID)
				read_quant_matrix_ext(s, gb);
		}
	}
}


static int decode_studio_vol_header(struct mpeg4_dec_param *ctx, struct get_bits_context *gb)
{
	struct MpegEncContext *s = &ctx->m;
	int width, height;
	int bits_per_raw_sample;

	// random_accessible_vol and video_object_type_indication have already
	// been read by the caller decode_vol_header()
	skip_bits(gb, 4); /* video_object_layer_verid */
	ctx->shape = get_bits(gb, 2); /* video_object_layer_shape */
	skip_bits(gb, 4); /* video_object_layer_shape_extension */
	skip_bits1(gb); /* progressive_sequence */
	if (ctx->shape != BIN_ONLY_SHAPE) {
		ctx->rgb = get_bits1(gb); /* rgb_components */
		s->chroma_format = get_bits(gb, 2); /* chroma_format */
		if (!s->chroma_format) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "illegal chroma format\n");
			return -1;
		}

		bits_per_raw_sample = get_bits(gb, 4); /* bit_depth */
		if (bits_per_raw_sample == 10) {
			if (ctx->rgb) {
				ctx->pix_fmt = AV_PIX_FMT_GBRP10;
			} else {
				ctx->pix_fmt = s->chroma_format == CHROMA_422 ? AV_PIX_FMT_YUV422P10 : AV_PIX_FMT_YUV444P10;
			}
		}
		else {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "MPEG-4 Studio profile bit-depth %u", bits_per_raw_sample);
			return -1;
		}
		ctx->bits_per_raw_sample = bits_per_raw_sample;
	}
	if (ctx->shape == RECT_SHAPE) {
		check_marker(gb, "before video_object_layer_width");
		width = get_bits(gb, 14); /* video_object_layer_width */
		check_marker(gb, "before video_object_layer_height");
		height = get_bits(gb, 14); /* video_object_layer_height */
		check_marker(gb, "after video_object_layer_height");

		/* Do the same check as non-studio profile */
		if (width && height) {
			if (s->width && s->height &&
				(s->width != width || s->height != height))
				s->context_reinit = 1;
			s->width  = width;
			s->height = height;
		}
	}
	s->aspect_ratio_info = get_bits(gb, 4);
	if (s->aspect_ratio_info == FF_ASPECT_EXTENDED) {
		ctx->sample_aspect_ratio.num = get_bits(gb, 8);  // par_width
		ctx->sample_aspect_ratio.den = get_bits(gb, 8);  // par_height
	} else {
		ctx->sample_aspect_ratio = ff_h263_pixel_aspect[s->aspect_ratio_info];
	}
	skip_bits(gb, 4); /* frame_rate_code */
	skip_bits(gb, 15); /* first_half_bit_rate */
	check_marker(gb, "after first_half_bit_rate");
	skip_bits(gb, 15); /* latter_half_bit_rate */
	check_marker(gb, "after latter_half_bit_rate");
	skip_bits(gb, 15); /* first_half_vbv_buffer_size */
	check_marker(gb, "after first_half_vbv_buffer_size");
	skip_bits(gb, 3); /* latter_half_vbv_buffer_size */
	skip_bits(gb, 11); /* first_half_vbv_buffer_size */
	check_marker(gb, "after first_half_vbv_buffer_size");
	skip_bits(gb, 15); /* latter_half_vbv_occupancy */
	check_marker(gb, "after latter_half_vbv_occupancy");
	s->low_delay = get_bits1(gb);
	s->mpeg_quant = get_bits1(gb); /* mpeg2_stream */

	next_start_code_studio(gb);
	extension_and_user_data(s, gb, 2);

	return 0;
}

static int decode_vol_header(struct mpeg4_dec_param *ctx, struct get_bits_context *gb)
{
	struct MpegEncContext *s = &ctx->m;
	int width, height, vo_ver_id;

	/* vol header */
	skip_bits(gb, 1);                   /* random access */
	s->vo_type = get_bits(gb, 8);

	/* If we are in studio profile (per vo_type), check if its all consistent
	* and if so continue pass control to decode_studio_vol_header().
	* elIf something is inconsistent, error out
	* else continue with (non studio) vol header decpoding.
	*/
	if (s->vo_type == CORE_STUDIO_VO_TYPE ||
		s->vo_type == SIMPLE_STUDIO_VO_TYPE) {
		if (ctx->profile != FF_PROFILE_UNKNOWN && ctx->profile != FF_PROFILE_MPEG4_SIMPLE_STUDIO)
			return -1;
		s->studio_profile = 1;
		ctx->profile = FF_PROFILE_MPEG4_SIMPLE_STUDIO;
		return decode_studio_vol_header(ctx, gb);
	} else if (s->studio_profile) {
		return -1;
	}

	if (get_bits1(gb) != 0) {           /* is_ol_id */
		vo_ver_id = get_bits(gb, 4);    /* vo_ver_id */
		skip_bits(gb, 3);               /* vo_priority */
	} else {
		vo_ver_id = 1;
	}
	s->aspect_ratio_info = get_bits(gb, 4);
	if (s->aspect_ratio_info == FF_ASPECT_EXTENDED) {
		ctx->sample_aspect_ratio.num = get_bits(gb, 8);  // par_width
		ctx->sample_aspect_ratio.den = get_bits(gb, 8);  // par_height
	} else {
		ctx->sample_aspect_ratio = ff_h263_pixel_aspect[s->aspect_ratio_info];
	}

	if ((ctx->vol_control_parameters = get_bits1(gb))) { /* vol control parameter */
		int chroma_format = get_bits(gb, 2);
		if (chroma_format != CHROMA_420)
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "illegal chroma format\n");

		s->low_delay = get_bits1(gb);
		if (get_bits1(gb)) {    /* vbv parameters */
			get_bits(gb, 15);   /* first_half_bitrate */
			check_marker(gb, "after first_half_bitrate");
			get_bits(gb, 15);   /* latter_half_bitrate */
			check_marker(gb, "after latter_half_bitrate");
			get_bits(gb, 15);   /* first_half_vbv_buffer_size */
			check_marker(gb, "after first_half_vbv_buffer_size");
			get_bits(gb, 3);    /* latter_half_vbv_buffer_size */
			get_bits(gb, 11);   /* first_half_vbv_occupancy */
			check_marker(gb, "after first_half_vbv_occupancy");
			get_bits(gb, 15);   /* latter_half_vbv_occupancy */
			check_marker(gb, "after latter_half_vbv_occupancy");
		}
	} else {
		/* is setting low delay flag only once the smartest thing to do?
		* low delay detection will not be overridden. */
		if (s->picture_number == 0) {
			switch (s->vo_type) {
			case SIMPLE_VO_TYPE:
			case ADV_SIMPLE_VO_TYPE:
				s->low_delay = 1;
				break;
			default:
				s->low_delay = 0;
			}
		}
	}

	ctx->shape = get_bits(gb, 2); /* vol shape */
	if (ctx->shape != RECT_SHAPE)
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "only rectangular vol supported\n");
	if (ctx->shape == GRAY_SHAPE && vo_ver_id != 1) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Gray shape not supported\n");
		skip_bits(gb, 4);  /* video_object_layer_shape_extension */
	}

	check_marker(gb, "before time_increment_resolution");

	ctx->framerate.num = get_bits(gb, 16);
	if (!ctx->framerate.num) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "framerate==0\n");
		return -1;
	}

	ctx->time_increment_bits = av_log2(ctx->framerate.num - 1) + 1;
	if (ctx->time_increment_bits < 1)
		ctx->time_increment_bits = 1;

	check_marker(gb, "before fixed_vop_rate");

	if (get_bits1(gb) != 0)     /* fixed_vop_rate  */
		ctx->framerate.den = get_bits(gb, ctx->time_increment_bits);
	else
		ctx->framerate.den = 1;

	//ctx->time_base = av_inv_q(av_mul_q(ctx->framerate, (AVRational){ctx->ticks_per_frame, 1}));

	ctx->t_frame = 0;

	if (ctx->shape != BIN_ONLY_SHAPE) {
		if (ctx->shape == RECT_SHAPE) {
			check_marker(gb, "before width");
			width = get_bits(gb, 13);
			check_marker(gb, "before height");
			height = get_bits(gb, 13);
			check_marker(gb, "after height");
			if (width && height &&  /* they should be non zero but who knows */
			!(s->width && s->codec_tag == AV_RL32("MP4S"))) {
				if (s->width && s->height &&
				(s->width != width || s->height != height))
				s->context_reinit = 1;
				s->width  = width;
				s->height = height;
			}
		}

		s->progressive_sequence  =
		s->progressive_frame     = get_bits1(gb) ^ 1;
		s->interlaced_dct        = 0;
		if (!get_bits1(gb)) /* OBMC Disable */
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "MPEG-4 OBMC not supported (very likely buggy encoder)\n");
		if (vo_ver_id == 1)
			ctx->vol_sprite_usage = get_bits1(gb);    /* vol_sprite_usage */
		else
			ctx->vol_sprite_usage = get_bits(gb, 2);  /* vol_sprite_usage */

		if (ctx->vol_sprite_usage == STATIC_SPRITE)
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Static Sprites not supported\n");
		if (ctx->vol_sprite_usage == STATIC_SPRITE ||
			ctx->vol_sprite_usage == GMC_SPRITE) {
		if (ctx->vol_sprite_usage == STATIC_SPRITE) {
			skip_bits(gb, 13); // sprite_width
			check_marker(gb, "after sprite_width");
			skip_bits(gb, 13); // sprite_height
			check_marker(gb, "after sprite_height");
			skip_bits(gb, 13); // sprite_left
			check_marker(gb, "after sprite_left");
			skip_bits(gb, 13); // sprite_top
			check_marker(gb, "after sprite_top");
		}
		ctx->num_sprite_warping_points = get_bits(gb, 6);
		if (ctx->num_sprite_warping_points > 3) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "%d sprite_warping_points\n",
				ctx->num_sprite_warping_points);
			ctx->num_sprite_warping_points = 0;
			return -1;
		}
		s->sprite_warping_accuracy  = get_bits(gb, 2);
		ctx->sprite_brightness_change = get_bits1(gb);
		if (ctx->vol_sprite_usage == STATIC_SPRITE)
			skip_bits1(gb); // low_latency_sprite
		}
		// FIXME sadct disable bit if verid!=1 && shape not rect

		if (get_bits1(gb) == 1) {                   /* not_8_bit */
				s->quant_precision = get_bits(gb, 4);   /* quant_precision */
			if (get_bits(gb, 4) != 8)               /* bits_per_pixel */
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "N-bit not supported\n");
			if (s->quant_precision != 5)
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "quant precision %d\n", s->quant_precision);
			if (s->quant_precision<3 || s->quant_precision>9) {
				s->quant_precision = 5;
			}
		} else {
			s->quant_precision = 5;
		}

		// FIXME a bunch of grayscale shape things

		if ((s->mpeg_quant = get_bits1(gb))) { /* vol_quant_type */
			int i, v;

			//mpeg4_load_default_matrices(s);

			/* load custom intra matrix */
			if (get_bits1(gb)) {
				int last = 0;
			for (i = 0; i < 64; i++) {
				//int j;
				if (get_bits_left(gb) < 8) {
					v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "insufficient data for custom matrix\n");
					return -1;
				}
				v = get_bits(gb, 8);
				if (v == 0)
					break;

				last = v;
				//j = s->idsp.idct_permutation[ff_zigzag_direct[i]];
				//s->intra_matrix[j]        = last;
				//s->chroma_intra_matrix[j] = last;
			}

			/* replicate last value */
			//for (; i < 64; i++) {
				//int j = s->idsp.idct_permutation[ff_zigzag_direct[i]];
				//s->intra_matrix[j]        = last;
				//s->chroma_intra_matrix[j] = last;
			//}
			}

			/* load custom non intra matrix */
			if (get_bits1(gb)) {
				int last = 0;
				for (i = 0; i < 64; i++) {
					//int j;
					if (get_bits_left(gb) < 8) {
						v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "insufficient data for custom matrix\n");
						return -1;
					}
					v = get_bits(gb, 8);
					if (v == 0)
						break;

					last = v;
					//j = s->idsp.idct_permutation[ff_zigzag_direct[i]];
					//s->inter_matrix[j]        = v;
					//s->chroma_inter_matrix[j] = v;
				}

				/* replicate last value */
				//for (; i < 64; i++) {
					//int j = s->idsp.idct_permutation[ff_zigzag_direct[i]];
					//s->inter_matrix[j]        = last;
					//s->chroma_inter_matrix[j] = last;
				//}
			}

			// FIXME a bunch of grayscale shape things
		}

		if (vo_ver_id != 1)
			s->quarter_sample = get_bits1(gb);
		else
			s->quarter_sample = 0;

		if (get_bits_left(gb) < 4) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "VOL Header truncated\n");
			return -1;
		}

		if (!get_bits1(gb)) {
			int pos               = get_bits_count(gb);
			int estimation_method = get_bits(gb, 2);
			if (estimation_method < 2) {
				if (!get_bits1(gb)) {
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* opaque */
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* transparent */
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* intra_cae */
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* inter_cae */
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* no_update */
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* upsampling */
				}
				if (!get_bits1(gb)) {
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* intra_blocks */
					ctx->cplx_estimation_trash_p += 8 * get_bits1(gb);  /* inter_blocks */
					ctx->cplx_estimation_trash_p += 8 * get_bits1(gb);  /* inter4v_blocks */
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* not coded blocks */
				}
				if (!check_marker(gb, "in complexity estimation part 1")) {
					skip_bits_long(gb, pos - get_bits_count(gb));
					goto no_cplx_est;
				}
				if (!get_bits1(gb)) {
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* dct_coeffs */
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* dct_lines */
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* vlc_syms */
					ctx->cplx_estimation_trash_i += 4 * get_bits1(gb);  /* vlc_bits */
				}
				if (!get_bits1(gb)) {
					ctx->cplx_estimation_trash_p += 8 * get_bits1(gb);  /* apm */
					ctx->cplx_estimation_trash_p += 8 * get_bits1(gb);  /* npm */
					ctx->cplx_estimation_trash_b += 8 * get_bits1(gb);  /* interpolate_mc_q */
					ctx->cplx_estimation_trash_p += 8 * get_bits1(gb);  /* forwback_mc_q */
					ctx->cplx_estimation_trash_p += 8 * get_bits1(gb);  /* halfpel2 */
					ctx->cplx_estimation_trash_p += 8 * get_bits1(gb);  /* halfpel4 */
				}
				if (!check_marker(gb, "in complexity estimation part 2")) {
					skip_bits_long(gb, pos - get_bits_count(gb));
					goto no_cplx_est;
				}
				if (estimation_method == 1) {
					ctx->cplx_estimation_trash_i += 8 * get_bits1(gb);  /* sadct */
					ctx->cplx_estimation_trash_p += 8 * get_bits1(gb);  /* qpel */
				}
			} else
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid Complexity estimation method %d\n",
				estimation_method);
		} else {

no_cplx_est:
			ctx->cplx_estimation_trash_i =
			ctx->cplx_estimation_trash_p =
			ctx->cplx_estimation_trash_b = 0;
		}

		ctx->resync_marker = !get_bits1(gb); /* resync_marker_disabled */

		s->data_partitioning = get_bits1(gb);
		if (s->data_partitioning)
			ctx->rvlc = get_bits1(gb);

		if (vo_ver_id != 1) {
			ctx->new_pred = get_bits1(gb);
		if (ctx->new_pred) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "new pred not supported\n");
			skip_bits(gb, 2); /* requested upstream message type */
			skip_bits1(gb);   /* newpred segment type */
		}
		if (get_bits1(gb)) // reduced_res_vop
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "reduced resolution VOP not supported\n");
		} else {
			ctx->new_pred = 0;
		}

		ctx->scalability = get_bits1(gb);

		if (ctx->scalability) {
			struct get_bits_context bak = *gb;
			int h_sampling_factor_n;
			int h_sampling_factor_m;
			int v_sampling_factor_n;
			int v_sampling_factor_m;

			skip_bits1(gb);    // hierarchy_type
			skip_bits(gb, 4);  /* ref_layer_id */
			skip_bits1(gb);    /* ref_layer_sampling_dir */
			h_sampling_factor_n = get_bits(gb, 5);
			h_sampling_factor_m = get_bits(gb, 5);
			v_sampling_factor_n = get_bits(gb, 5);
			v_sampling_factor_m = get_bits(gb, 5);
			ctx->enhancement_type = get_bits1(gb);

			if (h_sampling_factor_n == 0 || h_sampling_factor_m == 0 ||
				v_sampling_factor_n == 0 || v_sampling_factor_m == 0) {
				/* illegal scalability header (VERY broken encoder),
				* trying to workaround */
				ctx->scalability = 0;
				*gb            = bak;
			} else
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "scalability not supported\n");

			// bin shape stuff FIXME
		}
	}

	if (1) {
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "tb %d/%d, tincrbits:%d, qp_prec:%d, ps:%d, low_delay:%d  %s%s%s%s\n",
			ctx->framerate.den, ctx->framerate.num,
			ctx->time_increment_bits,
			s->quant_precision,
			s->progressive_sequence,
			s->low_delay,
			ctx->scalability ? "scalability " :"" , s->quarter_sample ? "qpel " : "",
			s->data_partitioning ? "partition " : "", ctx->rvlc ? "rvlc " : "");
	}

	return 0;
}


/**
 * Decode the user data stuff in the header.
 * Also initializes divx/xvid/lavc_version/build.
 */
static int decode_user_data(struct mpeg4_dec_param *ctx, struct get_bits_context *gb)
{
	struct MpegEncContext *s = &ctx->m;
	char buf[256];
	int i;
	int e;
	int ver = 0, build = 0, ver2 = 0, ver3 = 0;
	char last;

	for (i = 0; i < 255 && get_bits_count(gb) < gb->size_in_bits; i++) {
		if (show_bits(gb, 23) == 0)
		break;
		buf[i] = get_bits(gb, 8);
	}
	buf[i] = 0;

	/* divx detection */
	e = sscanf(buf, "DivX%dBuild%d%c", &ver, &build, &last);
	if (e < 2)
		e = sscanf(buf, "DivX%db%d%c", &ver, &build, &last);
	if (e >= 2) {
		ctx->divx_version = ver;
		ctx->divx_build   = build;
		s->divx_packed  = e == 3 && last == 'p';
	}

	/* libavcodec detection */
	e = sscanf(buf, "FFmpe%*[^b]b%d", &build) + 3;
	if (e != 4)
		e = sscanf(buf, "FFmpeg v%d.%d.%d / libavcodec build: %d", &ver, &ver2, &ver3, &build);
	if (e != 4) {
		e = sscanf(buf, "Lavc%d.%d.%d", &ver, &ver2, &ver3) + 1;
		if (e > 1) {
			if (ver > 0xFFU || ver2 > 0xFFU || ver3 > 0xFFU) {
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Unknown Lavc version string encountered, %d.%d.%d; "
					"clamping sub-version values to 8-bits.\n",
					ver, ver2, ver3);
			}
			build = ((ver & 0xFF) << 16) + ((ver2 & 0xFF) << 8) + (ver3 & 0xFF);
		}
	}
	if (e != 4) {
		if (strcmp(buf, "ffmpeg") == 0)
			ctx->lavc_build = 4600;
	}
	if (e == 4)
		ctx->lavc_build = build;

	/* Xvid detection */
	e = sscanf(buf, "XviD%d", &build);
	if (e == 1)
		ctx->xvid_build = build;

	return 0;
}


static int mpeg4_decode_gop_header(struct MpegEncContext *s, struct get_bits_context *gb)
{
	int hours, minutes, seconds;

	if (!show_bits(gb, 23)) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "GOP header invalid\n");
		return -1;
	}

	hours   = get_bits(gb, 5);
	minutes = get_bits(gb, 6);
	check_marker(gb, "in gop_header");
	seconds = get_bits(gb, 6);

	s->time_base = seconds + 60*(minutes + 60*hours);

	skip_bits1(gb);
	skip_bits1(gb);

	return 0;
}


static int mpeg4_decode_profile_level(struct MpegEncContext *s, struct get_bits_context *gb, int *profile, int *level)
{

	*profile = get_bits(gb, 4);
	*level   = get_bits(gb, 4);

	// for Simple profile, level 0
	if (*profile == 0 && *level == 8) {
		*level = 0;
	}

	return 0;
}


static int decode_studiovisualobject(struct mpeg4_dec_param *ctx, struct get_bits_context *gb)
{
	struct MpegEncContext *s = &ctx->m;
	int visual_object_type;

	skip_bits(gb, 4); /* visual_object_verid */
	visual_object_type = get_bits(gb, 4);
	if (visual_object_type != VOT_VIDEO_ID) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "VO type %u", visual_object_type);
		return -1;
	}

	next_start_code_studio(gb);
	extension_and_user_data(s, gb, 1);

	return 0;
}


static int mpeg4_decode_visual_object(struct MpegEncContext *s, struct get_bits_context *gb)
{
	int visual_object_type;
	int is_visual_object_identifier = get_bits1(gb);

	if (is_visual_object_identifier) {
		skip_bits(gb, 4+3);
	}
	visual_object_type = get_bits(gb, 4);

	if (visual_object_type == VOT_VIDEO_ID ||
	visual_object_type == VOT_STILL_TEXTURE_ID) {
		int video_signal_type = get_bits1(gb);
		if (video_signal_type) {
			int video_range, color_description;
			skip_bits(gb, 3); // video_format
			video_range = get_bits1(gb);
			color_description = get_bits1(gb);

			s->ctx->color_range = video_range ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;

			if (color_description) {
				s->ctx->color_primaries = get_bits(gb, 8);
				s->ctx->color_trc       = get_bits(gb, 8);
				s->ctx->colorspace      = get_bits(gb, 8);
			}
		}
	}

	return 0;
}

static void decode_smpte_tc(struct mpeg4_dec_param *ctx, struct get_bits_context *gb)
{
	skip_bits(gb, 16); /* Time_code[63..48] */
	check_marker(gb, "after Time_code[63..48]");
	skip_bits(gb, 16); /* Time_code[47..32] */
	check_marker(gb, "after Time_code[47..32]");
	skip_bits(gb, 16); /* Time_code[31..16] */
	check_marker(gb, "after Time_code[31..16]");
	skip_bits(gb, 16); /* Time_code[15..0] */
	check_marker(gb, "after Time_code[15..0]");
	skip_bits(gb, 4); /* reserved_bits */
}

static void reset_studio_dc_predictors(struct MpegEncContext *s)
{
	/* Reset DC Predictors */
	s->last_dc[0] =
	s->last_dc[1] =
	s->last_dc[2] = 1 << (s->ctx->bits_per_raw_sample + s->dct_precision + s->intra_dc_precision - 1);
}

/**
 * Decode the next studio vop header.
 * @return <0 if something went wrong
 */
static int decode_studio_vop_header(struct mpeg4_dec_param *ctx, struct get_bits_context *gb)
{
	struct MpegEncContext *s = &ctx->m;

	if (get_bits_left(gb) <= 32)
		return 0;

	//s->decode_mb = mpeg4_decode_studio_mb;

	decode_smpte_tc(ctx, gb);

	skip_bits(gb, 10); /* temporal_reference */
	skip_bits(gb, 2); /* vop_structure */
	s->pict_type = get_bits(gb, 2) + AV_PICTURE_TYPE_I; /* vop_coding_type */
	if (get_bits1(gb)) { /* vop_coded */
		skip_bits1(gb); /* top_field_first */
		skip_bits1(gb); /* repeat_first_field */
		s->progressive_frame = get_bits1(gb) ^ 1; /* progressive_frame */
	}

	if (s->pict_type == AV_PICTURE_TYPE_I) {
		if (get_bits1(gb))
			reset_studio_dc_predictors(s);
	}

	if (ctx->shape != BIN_ONLY_SHAPE) {
		s->alternate_scan = get_bits1(gb);
		s->frame_pred_frame_dct = get_bits1(gb);
		s->dct_precision = get_bits(gb, 2);
		s->intra_dc_precision = get_bits(gb, 2);
		s->q_scale_type = get_bits1(gb);
	}

	//if (s->alternate_scan) {    }

	//mpeg4_load_default_matrices(s);

	next_start_code_studio(gb);
	extension_and_user_data(s, gb, 4);

	return 0;
}

static int decode_new_pred(struct mpeg4_dec_param *ctx, struct get_bits_context *gb)
{
	int len = FFMIN(ctx->time_increment_bits + 3, 15);

	get_bits(gb, len);
	if (get_bits1(gb))
		get_bits(gb, len);
	check_marker(gb, "after new_pred");

	return 0;
}

static int decode_vop_header(struct mpeg4_dec_param *ctx, struct get_bits_context *gb)
{
	struct MpegEncContext *s = &ctx->m;
	int time_incr, time_increment;
	int64_t pts;

	s->mcsel       = 0;
	s->pict_type = get_bits(gb, 2) + AV_PICTURE_TYPE_I;        /* pict type: I = 0 , P = 1 */
	if (s->pict_type == AV_PICTURE_TYPE_B && s->low_delay &&
		ctx->vol_control_parameters == 0) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "low_delay flag set incorrectly, clearing it\n");
		s->low_delay = 0;
	}

	s->partitioned_frame = s->data_partitioning && s->pict_type != AV_PICTURE_TYPE_B;
	/*if (s->partitioned_frame)
		s->decode_mb = mpeg4_decode_partitioned_mb;
	else
		s->decode_mb = mpeg4_decode_mb;*/

	time_incr = 0;
	while (get_bits1(gb) != 0)
		time_incr++;

	check_marker(gb, "before time_increment");

	if (ctx->time_increment_bits == 0 ||
		!(show_bits(gb, ctx->time_increment_bits + 1) & 1)) {
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "time_increment_bits %d is invalid in relation to the current bitstream, this is likely caused by a missing VOL header\n", ctx->time_increment_bits);

		for (ctx->time_increment_bits = 1;
			ctx->time_increment_bits < 16;
			ctx->time_increment_bits++) {
			if (s->pict_type == AV_PICTURE_TYPE_P ||
				(s->pict_type == AV_PICTURE_TYPE_S &&
				ctx->vol_sprite_usage == GMC_SPRITE)) {
				if ((show_bits(gb, ctx->time_increment_bits + 6) & 0x37) == 0x30)
					break;
			} else if ((show_bits(gb, ctx->time_increment_bits + 5) & 0x1F) == 0x18)
				break;
		}

		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "time_increment_bits set to %d bits, based on bitstream analysis\n", ctx->time_increment_bits);
		if (ctx->framerate.num && 4*ctx->framerate.num < 1<<ctx->time_increment_bits) {
			ctx->framerate.num = 1<<ctx->time_increment_bits;
			//ctx->time_base = av_inv_q(av_mul_q(ctx->framerate, (AVRational){ctx->ticks_per_frame, 1}));
		}
	}

	if (IS_3IV1)
		time_increment = get_bits1(gb);        // FIXME investigate further
	else
		time_increment = get_bits(gb, ctx->time_increment_bits);

	if (s->pict_type != AV_PICTURE_TYPE_B) {
		s->last_time_base = s->time_base;
		s->time_base     += time_incr;
		s->time = s->time_base * (int64_t)ctx->framerate.num + time_increment;
		//if (s->workaround_bugs & FF_BUG_UMP4) { }
		s->pp_time         = s->time - s->last_non_b_time;
		s->last_non_b_time = s->time;
	} else {
		s->time    = (s->last_time_base + time_incr) * (int64_t)ctx->framerate.num + time_increment;
		s->pb_time = s->pp_time - (s->last_non_b_time - s->time);
		if (s->pp_time <= s->pb_time ||
			s->pp_time <= s->pp_time - s->pb_time ||
			s->pp_time <= 0) {
			/* messed up order, maybe after seeking? skipping current B-frame */
			return FRAME_SKIPPED;
		}
		//ff_mpeg4_init_direct_mv(s);

			if (ctx->t_frame == 0)
		ctx->t_frame = s->pb_time;
		if (ctx->t_frame == 0)
			ctx->t_frame = 1;  // 1/0 protection
		s->pp_field_time = (ROUNDED_DIV(s->last_non_b_time, ctx->t_frame) -
		ROUNDED_DIV(s->last_non_b_time - s->pp_time, ctx->t_frame)) * 2;
		s->pb_field_time = (ROUNDED_DIV(s->time, ctx->t_frame) -
		ROUNDED_DIV(s->last_non_b_time - s->pp_time, ctx->t_frame)) * 2;
		if (s->pp_field_time <= s->pb_field_time || s->pb_field_time <= 1) {
			s->pb_field_time = 2;
			s->pp_field_time = 4;
			if (!s->progressive_sequence)
				return FRAME_SKIPPED;
		}
	}

	if (ctx->framerate.den)
		pts = ROUNDED_DIV(s->time, ctx->framerate.den);
	else
		pts = AV_NOPTS_VALUE;
	v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "MPEG4 PTS: %lld\n", pts);

	check_marker(gb, "before vop_coded");

	/* vop coded */
	if (get_bits1(gb) != 1) {
		if (1)
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "vop not coded\n");
		return FRAME_SKIPPED;
	}
	if (ctx->new_pred)
		decode_new_pred(ctx, gb);

	if (ctx->shape != BIN_ONLY_SHAPE &&
		(s->pict_type == AV_PICTURE_TYPE_P ||
		(s->pict_type == AV_PICTURE_TYPE_S &&
		ctx->vol_sprite_usage == GMC_SPRITE))) {
		/* rounding type for motion estimation */
		s->no_rounding = get_bits1(gb);
	} else {
		s->no_rounding = 0;
	}
	// FIXME reduced res stuff

	if (ctx->shape != RECT_SHAPE) {
		if (ctx->vol_sprite_usage != 1 || s->pict_type != AV_PICTURE_TYPE_I) {
			skip_bits(gb, 13);  /* width */
			check_marker(gb, "after width");
			skip_bits(gb, 13);  /* height */
			check_marker(gb, "after height");
			skip_bits(gb, 13);  /* hor_spat_ref */
			check_marker(gb, "after hor_spat_ref");
			skip_bits(gb, 13);  /* ver_spat_ref */
		}
		skip_bits1(gb);         /* change_CR_disable */

		if (get_bits1(gb) != 0)
			skip_bits(gb, 8);   /* constant_alpha_value */
	}

	// FIXME complexity estimation stuff

	if (ctx->shape != BIN_ONLY_SHAPE) {
		skip_bits_long(gb, ctx->cplx_estimation_trash_i);
		if (s->pict_type != AV_PICTURE_TYPE_I)
			skip_bits_long(gb, ctx->cplx_estimation_trash_p);
		if (s->pict_type == AV_PICTURE_TYPE_B)
			skip_bits_long(gb, ctx->cplx_estimation_trash_b);

		if (get_bits_left(gb) < 3) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Header truncated\n");
			return -1;
		}
		ctx->intra_dc_threshold = ff_mpeg4_dc_threshold[get_bits(gb, 3)];
		if (!s->progressive_sequence) {
			s->top_field_first = get_bits1(gb);
			s->alternate_scan  = get_bits1(gb);
		} else
			s->alternate_scan = 0;
	}

	/*if (s->alternate_scan) { } */

	if (s->pict_type == AV_PICTURE_TYPE_S) {
		if((ctx->vol_sprite_usage == STATIC_SPRITE ||
			ctx->vol_sprite_usage == GMC_SPRITE)) {
			//if (mpeg4_decode_sprite_trajectory(ctx, gb) < 0)
				//return -1;
			if (ctx->sprite_brightness_change)
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "sprite_brightness_change not supported\n");
			if (ctx->vol_sprite_usage == STATIC_SPRITE)
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "static sprite not supported\n");
		} else {
			memset(s->sprite_offset, 0, sizeof(s->sprite_offset));
			memset(s->sprite_delta, 0, sizeof(s->sprite_delta));
		}
	}

	if (ctx->shape != BIN_ONLY_SHAPE) {
		s->chroma_qscale = s->qscale = get_bits(gb, s->quant_precision);
		if (s->qscale == 0) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Error, header damaged or not MPEG-4 header (qscale=0)\n");
			return -1;  // makes no sense to continue, as there is nothing left from the image then
		}

		if (s->pict_type != AV_PICTURE_TYPE_I) {
			s->f_code = get_bits(gb, 3);        /* fcode_for */
			if (s->f_code == 0) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Error, header damaged or not MPEG-4 header (f_code=0)\n");
					s->f_code = 1;
				return -1;  // makes no sense to continue, as there is nothing left from the image then
			}
		} else
			s->f_code = 1;

		if (s->pict_type == AV_PICTURE_TYPE_B) {
			s->b_code = get_bits(gb, 3);
			if (s->b_code == 0) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Error, header damaged or not MPEG4 header (b_code=0)\n");
					s->b_code=1;
				return -1; // makes no sense to continue, as the MV decoding will break very quickly
			}
		} else
			s->b_code = 1;

		if (1) {
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "qp:%d fc:%d,%d %s size:%d pro:%d alt:%d top:%d %spel part:%d resync:%d w:%d a:%d rnd:%d vot:%d%s dc:%d ce:%d/%d/%d time:%ld tincr:%d\n",
				s->qscale, s->f_code, s->b_code,
				s->pict_type == AV_PICTURE_TYPE_I ? "I" : (s->pict_type == AV_PICTURE_TYPE_P ? "P" : (s->pict_type == AV_PICTURE_TYPE_B ? "B" : "S")),
				gb->size_in_bits,s->progressive_sequence, s->alternate_scan,
				s->top_field_first, s->quarter_sample ? "q" : "h",
				s->data_partitioning, ctx->resync_marker,
				ctx->num_sprite_warping_points, s->sprite_warping_accuracy,
				1 - s->no_rounding, s->vo_type,
				ctx->vol_control_parameters ? " VOLC" : " ", ctx->intra_dc_threshold,
				ctx->cplx_estimation_trash_i, ctx->cplx_estimation_trash_p,
				ctx->cplx_estimation_trash_b,
				s->time,
				time_increment);
		}

		if (!ctx->scalability) {
			if (ctx->shape != RECT_SHAPE && s->pict_type != AV_PICTURE_TYPE_I)
				skip_bits1(gb);  // vop shape coding type
		} else {
			if (ctx->enhancement_type) {
				int load_backward_shape = get_bits1(gb);
				if (load_backward_shape)
					v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "load backward shape isn't supported\n");
			}
			skip_bits(gb, 2);  // ref_select_code
		}
	}
	/* detect buggy encoders which don't set the low_delay flag
	* (divx4/xvid/opendivx). Note we cannot detect divx5 without B-frames
	* easily (although it's buggy too) */
	if (s->vo_type == 0 && ctx->vol_control_parameters == 0 &&
		ctx->divx_version == -1 && s->picture_number == 0) {
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "looks like this file was encoded with (divx4/(old)xvid/opendivx) -> forcing low_delay flag\n");
			s->low_delay = 1;
	}

	s->picture_number++;  // better than pic number==0 always ;)

	// FIXME add short header support
	//s->y_dc_scale_table = ff_mpeg4_y_dc_scale_table;
	//s->c_dc_scale_table = ff_mpeg4_c_dc_scale_table;

	return 0;
}

/**
 * Decode MPEG-4 headers.
 * @return <0 if no VOP found (or a damaged one)
 *         FRAME_SKIPPED if a not coded VOP is found
 *         0 if a VOP is found
 */
int ff_mpeg4_decode_picture_header(struct mpeg4_dec_param *ctx, struct get_bits_context *gb)
{
	struct MpegEncContext *s = &ctx->m;

	unsigned startcode, v;
	int ret;
	int vol = 0;
	int bits_per_raw_sample = 0;

	s->ctx = ctx;

	/* search next start code */
	align_get_bits(gb);

	// If we have not switched to studio profile than we also did not switch bps
	// that means something else (like a previous instance) outside set bps which
	// would be inconsistant with the currect state, thus reset it
	if (!s->studio_profile && bits_per_raw_sample != 8)
		bits_per_raw_sample = 0;

	if (show_bits(gb, 24) == 0x575630) {
		skip_bits(gb, 24);
		if (get_bits(gb, 8) == 0xF0)
			goto end;
	}

	startcode = 0xff;
	for (;;) {
		if (get_bits_count(gb) >= gb->size_in_bits) {
			if (gb->size_in_bits == 8) {
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "frame skip %d\n", gb->size_in_bits);
				return FRAME_SKIPPED;  // divx bug
			} else
				return -1;  // end of stream
		}

		/* use the bits after the test */
		v = get_bits(gb, 8);
		startcode = ((startcode << 8) | v) & 0xffffffff;

		if ((startcode & 0xFFFFFF00) != 0x100)
			continue;  // no startcode

		if (1) { //debug
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "startcode: %3X \n", startcode);
			if (startcode <= 0x11F)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Video Object Start\n");
			else if (startcode <= 0x12F)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Video Object Layer Start\n");
			else if (startcode <= 0x13F)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Reserved\n");
			else if (startcode <= 0x15F)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "FGS bp start\n");
			else if (startcode <= 0x1AF)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Reserved\n");
			else if (startcode == 0x1B0)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Visual Object Seq Start\n");
			else if (startcode == 0x1B1)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Visual Object Seq End\n");
			else if (startcode == 0x1B2)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "User Data\n");
			else if (startcode == 0x1B3)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Group of VOP start\n");
			else if (startcode == 0x1B4)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Video Session Error\n");
			else if (startcode == 0x1B5)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Visual Object Start\n");
			else if (startcode == 0x1B6)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Video Object Plane start\n");
			else if (startcode == 0x1B7)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "slice start\n");
			else if (startcode == 0x1B8)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "extension start\n");
			else if (startcode == 0x1B9)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "fgs start\n");
			else if (startcode == 0x1BA)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "FBA Object start\n");
			else if (startcode == 0x1BB)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "FBA Object Plane start\n");
			else if (startcode == 0x1BC)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Mesh Object start\n");
			else if (startcode == 0x1BD)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Mesh Object Plane start\n");
			else if (startcode == 0x1BE)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Still Texture Object start\n");
			else if (startcode == 0x1BF)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Texture Spatial Layer start\n");
			else if (startcode == 0x1C0)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Texture SNR Layer start\n");
			else if (startcode == 0x1C1)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Texture Tile start\n");
			else if (startcode == 0x1C2)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Texture Shape Layer start\n");
			else if (startcode == 0x1C3)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "stuffing start\n");
			else if (startcode <= 0x1C5)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "reserved\n");
			else if (startcode <= 0x1FF)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "System start\n");
		}

		if (startcode >= 0x120 && startcode <= 0x12F) {
			if (vol) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Ignoring multiple VOL headers\n");
				continue;
			}
			vol++;
			if ((ret = decode_vol_header(ctx, gb)) < 0)
				return ret;
		} else if (startcode == USER_DATA_STARTCODE) {
			decode_user_data(ctx, gb);
		} else if (startcode == GOP_STARTCODE) {
			mpeg4_decode_gop_header(s, gb);
		} else if (startcode == VOS_STARTCODE) {
		int profile, level;
		mpeg4_decode_profile_level(s, gb, &profile, &level);
		if (profile == FF_PROFILE_MPEG4_SIMPLE_STUDIO &&
			(level > 0 && level < 9)) {
				s->studio_profile = 1;
				next_start_code_studio(gb);
				extension_and_user_data(s, gb, 0);
			} else if (s->studio_profile) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Mixes studio and non studio profile\n");
				return -1;
			}
			ctx->profile = profile;
			ctx->level   = level;
		} else if (startcode == VISUAL_OBJ_STARTCODE) {
			if (s->studio_profile) {
				if ((ret = decode_studiovisualobject(ctx, gb)) < 0)
					return ret;
			} else
			mpeg4_decode_visual_object(s, gb);
		} else if (startcode == VOP_STARTCODE) {
			break;
		}

		align_get_bits(gb);
		startcode = 0xff;
	}

end:
	if (s->studio_profile) {
		if (!bits_per_raw_sample) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Missing VOL header\n");
			return -1;
		}
		return decode_studio_vop_header(ctx, gb);
	} else
		return decode_vop_header(ctx, gb);
}

int mpeg4_decode_extradata_ps(u8 *buf, int size, struct mpeg4_param_sets *ps)
{
	int ret = 0;
	struct get_bits_context gb;

	ps->head_parsed = false;

	init_get_bits8(&gb, buf, size);

	ret = ff_mpeg4_decode_picture_header(&ps->dec_ps, &gb);
	if (ret < -1) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Failed to parse extradata\n");
		return ret;
	}

	if (ps->dec_ps.m.width && ps->dec_ps.m.height)
		ps->head_parsed = true;

	return 0;
}

