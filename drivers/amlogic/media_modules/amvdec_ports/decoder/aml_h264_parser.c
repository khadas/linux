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

#include "aml_h264_parser.h"
#include "../utils/get_bits.h"
#include "../utils/put_bits.h"
#include "../utils/golomb.h"
#include "../utils/common.h"
#include "utils.h"

#define MAX_DELAYED_PIC_COUNT	(16)
#define MAX_LOG2_MAX_FRAME_NUM	(12 + 4)
#define MIN_LOG2_MAX_FRAME_NUM	(4)
#define MAX_SPS_COUNT		(32)
#define EXTENDED_SAR		(255)

static const struct rational h264_pixel_aspect[17] = {
	{   0,  1 },
	{   1,  1 },
	{  12, 11 },
	{  10, 11 },
	{  16, 11 },
	{  40, 33 },
	{  24, 11 },
	{  20, 11 },
	{  32, 11 },
	{  80, 33 },
	{  18, 11 },
	{  15, 11 },
	{  64, 33 },
	{ 160, 99 },
	{   4,  3 },
	{   3,  2 },
	{   2,  1 },
};

/* maximum number of MBs in the DPB for a given level */
static const int level_max_dpb_mbs[][2] = {
	{ 10, 396	},
	{ 11, 900	},
	{ 12, 2376	},
	{ 13, 2376	},
	{ 20, 2376	},
	{ 21, 4752	},
	{ 22, 8100	},
	{ 30, 8100	},
	{ 31, 18000 	},
	{ 32, 20480 	},
	{ 40, 32768 	},
	{ 41, 32768 	},
	{ 42, 34816 	},
	{ 50, 110400	},
	{ 51, 184320	},
	{ 52, 184320	},
};

static const u8 default_scaling4[2][16] = {
	{  6, 13, 20, 28, 13, 20, 28, 32,
	  20, 28, 32, 37, 28, 32, 37, 42},
	{ 10, 14, 20, 24, 14, 20, 24, 27,
	  20, 24, 27, 30, 24, 27, 30, 34 }
};

static const u8 default_scaling8[2][64] = {
	{  6, 10, 13, 16, 18, 23, 25, 27,
	  10, 11, 16, 18, 23, 25, 27, 29,
	  13, 16, 18, 23, 25, 27, 29, 31,
	  16, 18, 23, 25, 27, 29, 31, 33,
	  18, 23, 25, 27, 29, 31, 33, 36,
	  23, 25, 27, 29, 31, 33, 36, 38,
	  25, 27, 29, 31, 33, 36, 38, 40,
	  27, 29, 31, 33, 36, 38, 40, 42 },
	{  9, 13, 15, 17, 19, 21, 22, 24,
	  13, 13, 17, 19, 21, 22, 24, 25,
	  15, 17, 19, 21, 22, 24, 25, 27,
	  17, 19, 21, 22, 24, 25, 27, 28,
	  19, 21, 22, 24, 25, 27, 28, 30,
	  21, 22, 24, 25, 27, 28, 30, 32,
	  22, 24, 25, 27, 28, 30, 32, 33,
	  24, 25, 27, 28, 30, 32, 33, 35 }
};

extern const u8 ff_zigzag_scan[16 + 1];
extern const u8 ff_zigzag_direct[64];

static int decode_scaling_list(struct get_bits_context *gb,
	u8 *factors, int size,
	const u8 *jvt_list,
	const u8 *fallback_list)
{
	int i, last = 8, next = 8;
	const u8 *scan = size == 16 ? ff_zigzag_scan : ff_zigzag_direct;

	if (!get_bits1(gb)) /* matrix not written, we use the predicted one */
		memcpy(factors, fallback_list, size * sizeof(u8));
	else
		for (i = 0; i < size; i++) {
			if (next) {
				int v = get_se_golomb(gb);
				/*if (v < -128 || v > 127) { //JM19 has not check.
					pr_err( "delta scale %d is invalid\n", v);
					return -1;
				}*/
				next = (last + v) & 0xff;
			}
			if (!i && !next) { /* matrix not written, we use the preset one */
				memcpy(factors, jvt_list, size * sizeof(u8));
				break;
			}
			last = factors[scan[i]] = next ? next : last;
		}
	return 0;
}

/* returns non zero if the provided SPS scaling matrix has been filled */
static int decode_scaling_matrices(struct get_bits_context *gb,
	const struct h264_SPS_t *sps,
	const struct h264_PPS_t *pps, int is_sps,
	u8(*scaling_matrix4)[16],
	u8(*scaling_matrix8)[64])
{
	int ret = 0;
	int fallback_sps = !is_sps && sps->scaling_matrix_present;
	const u8 *fallback[4] = {
		fallback_sps ? sps->scaling_matrix4[0] : default_scaling4[0],
		fallback_sps ? sps->scaling_matrix4[3] : default_scaling4[1],
		fallback_sps ? sps->scaling_matrix8[0] : default_scaling8[0],
		fallback_sps ? sps->scaling_matrix8[3] : default_scaling8[1]
	};

	if (get_bits1(gb)) {
		ret |= decode_scaling_list(gb, scaling_matrix4[0], 16, default_scaling4[0], fallback[0]);        // Intra, Y
		ret |= decode_scaling_list(gb, scaling_matrix4[1], 16, default_scaling4[0], scaling_matrix4[0]); // Intra, Cr
		ret |= decode_scaling_list(gb, scaling_matrix4[2], 16, default_scaling4[0], scaling_matrix4[1]); // Intra, Cb
		ret |= decode_scaling_list(gb, scaling_matrix4[3], 16, default_scaling4[1], fallback[1]);        // Inter, Y
		ret |= decode_scaling_list(gb, scaling_matrix4[4], 16, default_scaling4[1], scaling_matrix4[3]); // Inter, Cr
		ret |= decode_scaling_list(gb, scaling_matrix4[5], 16, default_scaling4[1], scaling_matrix4[4]); // Inter, Cb
		if (is_sps || pps->transform_8x8_mode) {
			ret |= decode_scaling_list(gb, scaling_matrix8[0], 64, default_scaling8[0], fallback[2]); // Intra, Y
			ret |= decode_scaling_list(gb, scaling_matrix8[3], 64, default_scaling8[1], fallback[3]); // Inter, Y
			if (sps->chroma_format_idc == 3) {
				ret |= decode_scaling_list(gb, scaling_matrix8[1], 64, default_scaling8[0], scaling_matrix8[0]); // Intra, Cr
				ret |= decode_scaling_list(gb, scaling_matrix8[4], 64, default_scaling8[1], scaling_matrix8[3]); // Inter, Cr
				ret |= decode_scaling_list(gb, scaling_matrix8[2], 64, default_scaling8[0], scaling_matrix8[1]); // Intra, Cb
				ret |= decode_scaling_list(gb, scaling_matrix8[5], 64, default_scaling8[1], scaling_matrix8[4]); // Inter, Cb
			}
		}
		if (!ret)
			ret = is_sps;
	}

	return ret;
}

static int decode_hrd_parameters(struct get_bits_context *gb,
	struct h264_SPS_t *sps)
{
	int cpb_count, i;

	cpb_count = get_ue_golomb_31(gb) + 1;
	if (cpb_count > 32U) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"cpb_count %d invalid\n", cpb_count);
		return -1;
	}

	get_bits(gb, 4); /* bit_rate_scale */
	get_bits(gb, 4); /* cpb_size_scale */
	for (i = 0; i < cpb_count; i++) {
		get_ue_golomb_long(gb); /* bit_rate_value_minus1 */
		get_ue_golomb_long(gb); /* cpb_size_value_minus1 */
		get_bits1(gb);		/* cbr_flag */
	}

	sps->initial_cpb_removal_delay_length = get_bits(gb, 5) + 1;
	sps->cpb_removal_delay_length	  = get_bits(gb, 5) + 1;
	sps->dpb_output_delay_length	  = get_bits(gb, 5) + 1;
	sps->time_offset_length		  = get_bits(gb, 5);
	sps->cpb_cnt			  = cpb_count;

	return 0;
}

static int decode_vui_parameters(struct get_bits_context *gb,  struct h264_SPS_t *sps)
{
	int aspect_ratio_info_present_flag;
	u32 aspect_ratio_idc;

	aspect_ratio_info_present_flag = get_bits1(gb);

	if (aspect_ratio_info_present_flag) {
		aspect_ratio_idc = get_bits(gb, 8);
		if (aspect_ratio_idc == EXTENDED_SAR) {
			sps->sar.num = get_bits(gb, 16);
			sps->sar.den = get_bits(gb, 16);
		} else if (aspect_ratio_idc < ARRAY_SIZE(h264_pixel_aspect)) {
			sps->sar = h264_pixel_aspect[aspect_ratio_idc];
		} else {
			return -1;
		}
	} else {
		sps->sar.num =
		sps->sar.den = 0;
	}

	if (get_bits1(gb))      /* overscan_info_present_flag */
		get_bits1(gb);  /* overscan_appropriate_flag */

	sps->video_signal_type_present_flag = get_bits1(gb);
	if (sps->video_signal_type_present_flag) {
		get_bits(gb, 3);                 /* video_format */
		sps->full_range = get_bits1(gb); /* video_full_range_flag */

		sps->colour_description_present_flag = get_bits1(gb);
		if (sps->colour_description_present_flag) {
			sps->color_primaries = get_bits(gb, 8); /* colour_primaries */
			sps->color_trc       = get_bits(gb, 8); /* transfer_characteristics */
			sps->colorspace      = get_bits(gb, 8); /* matrix_coefficients */

			// Set invalid values to "unspecified"
			if (!av_color_primaries_name(sps->color_primaries))
				sps->color_primaries = AVCOL_PRI_UNSPECIFIED;
			if (!av_color_transfer_name(sps->color_trc))
				sps->color_trc = AVCOL_TRC_UNSPECIFIED;
			if (!av_color_space_name(sps->colorspace))
				sps->colorspace = AVCOL_SPC_UNSPECIFIED;
		}
	}

	/* chroma_location_info_present_flag */
	if (get_bits1(gb)) {
		/* chroma_sample_location_type_top_field */
		//avctx->chroma_sample_location = get_ue_golomb(gb) + 1;
		get_ue_golomb(gb);  /* chroma_sample_location_type_bottom_field */
	}

	if (show_bits1(gb) && get_bits_left(gb) < 10) {
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Truncated VUI\n");
		return 0;
	}

	sps->timing_info_present_flag = get_bits1(gb);
	if (sps->timing_info_present_flag) {
		unsigned num_units_in_tick = get_bits_long(gb, 32);
		unsigned time_scale        = get_bits_long(gb, 32);
		if (!num_units_in_tick || !time_scale) {
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER,
				"time_scale/num_units_in_tick invalid or unsupported (%u/%u)\n",
				time_scale, num_units_in_tick);
			sps->timing_info_present_flag = 0;
		} else {
			sps->num_units_in_tick = num_units_in_tick;
			sps->time_scale = time_scale;
		}
		sps->fixed_frame_rate_flag = get_bits1(gb);
	}

	sps->nal_hrd_parameters_present_flag = get_bits1(gb);
	if (sps->nal_hrd_parameters_present_flag)
		if (decode_hrd_parameters(gb, sps) < 0)
			return -1;
	sps->vcl_hrd_parameters_present_flag = get_bits1(gb);
	if (sps->vcl_hrd_parameters_present_flag)
		if (decode_hrd_parameters(gb, sps) < 0)
			return -1;
	if (sps->nal_hrd_parameters_present_flag ||
		sps->vcl_hrd_parameters_present_flag)
		get_bits1(gb);     /* low_delay_hrd_flag */
	sps->pic_struct_present_flag = get_bits1(gb);
	if (!get_bits_left(gb))
		return 0;
	sps->bitstream_restriction_flag = get_bits1(gb);
	if (sps->bitstream_restriction_flag) {
		get_bits1(gb);     /* motion_vectors_over_pic_boundaries_flag */
		get_ue_golomb(gb); /* max_bytes_per_pic_denom */
		get_ue_golomb(gb); /* max_bits_per_mb_denom */
		get_ue_golomb(gb); /* log2_max_mv_length_horizontal */
		get_ue_golomb(gb); /* log2_max_mv_length_vertical */
		sps->num_reorder_frames = get_ue_golomb(gb);
		sps->max_dec_frame_buffering = get_ue_golomb(gb); /*max_dec_frame_buffering*/

		if (get_bits_left(gb) < 0) {
			sps->num_reorder_frames         = 0;
			sps->bitstream_restriction_flag = 0;
		}

		if (sps->num_reorder_frames > 16U
			/* max_dec_frame_buffering || max_dec_frame_buffering > 16 */) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"Clipping illegal num_reorder_frames %d\n",
				sps->num_reorder_frames);
				sps->num_reorder_frames = 16;
			return -1;
		}
	}

	return 0;
}

static int aml_h264_parser_sps(struct get_bits_context *gb, struct h264_SPS_t *sps)
{
	int ret;
	u32 sps_id;
	int profile_idc, level_idc, constraint_set_flags = 0;
	int i, log2_max_frame_num_minus4;

	profile_idc		= get_bits(gb, 8);
	constraint_set_flags	|= get_bits1(gb) << 0;	// constraint_set0_flag
	constraint_set_flags	|= get_bits1(gb) << 1;	// constraint_set1_flag
	constraint_set_flags	|= get_bits1(gb) << 2;	// constraint_set2_flag
	constraint_set_flags	|= get_bits1(gb) << 3;	// constraint_set3_flag
	constraint_set_flags	|= get_bits1(gb) << 4;	// constraint_set4_flag
	constraint_set_flags	|= get_bits1(gb) << 5;	// constraint_set5_flag
	skip_bits(gb, 2); 				// reserved_zero_2bits
	level_idc	= get_bits(gb, 8);
	sps_id		= get_ue_golomb_31(gb);

	if (sps_id >= MAX_SPS_COUNT) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"sps_id %u out of range\n", sps_id);
		goto fail;
	}

	sps->sps_id			= sps_id;
	sps->time_offset_length		= 24;
	sps->profile_idc		= profile_idc;
	sps->constraint_set_flags	= constraint_set_flags;
	sps->level_idc			= level_idc;
	sps->full_range			= -1;

	memset(sps->scaling_matrix4, 16, sizeof(sps->scaling_matrix4));
	memset(sps->scaling_matrix8, 16, sizeof(sps->scaling_matrix8));
	sps->scaling_matrix_present = 0;
	sps->colorspace = 2; //AVCOL_SPC_UNSPECIFIED

	if (sps->profile_idc == 100 ||  // High profile
		sps->profile_idc == 110 ||  // High10 profile
		sps->profile_idc == 122 ||  // High422 profile
		sps->profile_idc == 244 ||  // High444 Predictive profile
		sps->profile_idc ==  44 ||  // Cavlc444 profile
		sps->profile_idc ==  83 ||  // Scalable Constrained High profile (SVC)
		sps->profile_idc ==  86 ||  // Scalable High Intra profile (SVC)
		sps->profile_idc == 118 ||  // Stereo High profile (MVC)
		sps->profile_idc == 128 ||  // Multiview High profile (MVC)
		sps->profile_idc == 138 ||  // Multiview Depth High profile (MVCD)
		sps->profile_idc == 144) {  // old High444 profile
		sps->chroma_format_idc = get_ue_golomb_31(gb);

		if (sps->chroma_format_idc > 3U) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"chroma_format_idc %u\n", sps->chroma_format_idc);
			goto fail;
		} else if (sps->chroma_format_idc == 3) {
			sps->residual_color_transform_flag = get_bits1(gb);
			if (sps->residual_color_transform_flag) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
					"separate color planes are not supported\n");
				goto fail;
			}
		}

		sps->bit_depth_luma	= get_ue_golomb(gb) + 8;
		sps->bit_depth_chroma	= get_ue_golomb(gb) + 8;
		if (sps->bit_depth_chroma != sps->bit_depth_luma) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"Different chroma and luma bit depth\n");
			goto fail;
		}

		if (sps->bit_depth_luma	< 8 || sps->bit_depth_luma > 14 ||
			sps->bit_depth_chroma < 8 || sps->bit_depth_chroma > 14) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"illegal bit depth value (%d, %d)\n",
				sps->bit_depth_luma, sps->bit_depth_chroma);
			goto fail;
		}

		sps->transform_bypass = get_bits1(gb);
		ret = decode_scaling_matrices(gb, sps, NULL, 1,
			sps->scaling_matrix4, sps->scaling_matrix8);
		if (ret < 0)
			goto fail;
		sps->scaling_matrix_present |= ret;
	} else {
		sps->chroma_format_idc	= 1;
		sps->bit_depth_luma	= 8;
		sps->bit_depth_chroma	= 8;
	}

	log2_max_frame_num_minus4 = get_ue_golomb(gb);
	if (log2_max_frame_num_minus4 < MIN_LOG2_MAX_FRAME_NUM - 4 ||
		log2_max_frame_num_minus4 > MAX_LOG2_MAX_FRAME_NUM - 4) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"log2_max_frame_num_minus4 out of range (0-12): %d\n",
			log2_max_frame_num_minus4);
		goto fail;
	}
	sps->log2_max_frame_num = log2_max_frame_num_minus4 + 4;

	sps->poc_type = get_ue_golomb_31(gb);
	if (sps->poc_type == 0) { // FIXME #define
		u32 t = get_ue_golomb(gb);
		if (t > 12) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"log2_max_poc_lsb (%d) is out of range\n", t);
			goto fail;
		}
		sps->log2_max_poc_lsb = t + 4;
	} else if (sps->poc_type == 1) { // FIXME #define
		sps->delta_pic_order_always_zero_flag	= get_bits1(gb);
		sps->offset_for_non_ref_pic		= get_se_golomb_long(gb);
		sps->offset_for_top_to_bottom_field	= get_se_golomb_long(gb);

		sps->poc_cycle_length = get_ue_golomb(gb);
		if ((u32)sps->poc_cycle_length >= ARRAY_SIZE(sps->offset_for_ref_frame)) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"poc_cycle_length overflow %d\n", sps->poc_cycle_length);
			goto fail;
		}

		for (i = 0; i < sps->poc_cycle_length; i++)
			sps->offset_for_ref_frame[i] = get_se_golomb_long(gb);
	} else if (sps->poc_type != 2) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"illegal POC type %d\n", sps->poc_type);
		goto fail;
	}

	sps->ref_frame_count = get_ue_golomb_31(gb);
	if (sps->ref_frame_count > MAX_DELAYED_PIC_COUNT) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"too many reference frames %d\n", sps->ref_frame_count);
		goto fail;
	}
	sps->gaps_in_frame_num_allowed_flag = get_bits1(gb);
	sps->mb_width	= get_ue_golomb(gb) + 1;
	sps->mb_height	= get_ue_golomb(gb) + 1;

	sps->frame_mbs_only_flag = get_bits1(gb);

	if (sps->mb_height >= INT_MAX / 2U) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "height overflow\n");
		goto fail;
	}
	sps->mb_height *= 2 - sps->frame_mbs_only_flag;

	if (!sps->frame_mbs_only_flag)
		sps->mb_aff = get_bits1(gb);
	else
		sps->mb_aff = 0;

	if ((u32)sps->mb_width  >= INT_MAX / 16 ||
		(u32)sps->mb_height >= INT_MAX / 16) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"mb_width/height overflow\n");
		goto fail;
	}

	sps->direct_8x8_inference_flag = get_bits1(gb);

	sps->crop = get_bits1(gb);
	if (sps->crop) {
		u32 crop_left	= get_ue_golomb(gb);
		u32 crop_right	= get_ue_golomb(gb);
		u32 crop_top	= get_ue_golomb(gb);
		u32 crop_bottom	= get_ue_golomb(gb);
		int width	= 16 * sps->mb_width;
		int height	= 16 * sps->mb_height;
		int vsub	= (sps->chroma_format_idc == 1) ? 1 : 0;
		int hsub	= (sps->chroma_format_idc == 1 || sps->chroma_format_idc == 2) ? 1 : 0;
		int step_x	= 1 << hsub;
		int step_y	= (2 - sps->frame_mbs_only_flag) << vsub;

		if (crop_left > (u32)INT_MAX / 4 / step_x ||
			crop_right > (u32)INT_MAX / 4 / step_x ||
			crop_top > (u32)INT_MAX / 4 / step_y ||
			crop_bottom > (u32)INT_MAX / 4 / step_y ||
			(crop_left + crop_right ) * step_x >= width ||
			(crop_top + crop_bottom) * step_y >= height) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
				"crop values invalid %u %u %u %u / %d %d\n",
				crop_left, crop_right, crop_top, crop_bottom, width, height);
			goto fail;
		}

		sps->crop_left	= crop_left * step_x;
		sps->crop_right	= crop_right * step_x;
		sps->crop_top	= crop_top * step_y;
		sps->crop_bottom = crop_bottom * step_y;
	} else {
		sps->crop_left	=
		sps->crop_right	=
		sps->crop_top	=
		sps->crop_bottom =
		sps->crop	= 0;
	}

	sps->vui_parameters_present_flag = get_bits1(gb);
	if (sps->vui_parameters_present_flag) {
		int ret = decode_vui_parameters(gb,  sps);
		if (ret < 0)
			goto fail;
	}

	if (get_bits_left(gb) < 0) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"Overread %s by %d bits\n",
			sps->vui_parameters_present_flag ? "VUI" : "SPS", -get_bits_left(gb));
		/*goto out;*/
	}

#if 0
	/* if the maximum delay is not stored in the SPS, derive it based on the level */
	if (!sps->bitstream_restriction_flag && sps->ref_frame_count) {
		sps->num_reorder_frames = MAX_DELAYED_PIC_COUNT - 1;
		for (i = 0; i < ARRAY_SIZE(level_max_dpb_mbs); i++) {
			if (level_max_dpb_mbs[i][0] == sps->level_idc) {
				sps->num_reorder_frames =
					MIN(level_max_dpb_mbs[i][1] / (sps->mb_width * sps->mb_height),
						sps->num_reorder_frames);
				break;
			}
		}
	}
#endif

	sps->num_reorder_frames = MAX_DELAYED_PIC_COUNT - 1;
	for (i = 0; i < ARRAY_SIZE(level_max_dpb_mbs); i++) {
		if (level_max_dpb_mbs[i][0] == sps->level_idc) {
			sps->num_reorder_frames =
				MIN(level_max_dpb_mbs[i][1] / (sps->mb_width * sps->mb_height),
					sps->num_reorder_frames);
			sps->num_reorder_frames += 1;
			if (sps->max_dec_frame_buffering > sps->num_reorder_frames)
				sps->num_reorder_frames = sps->max_dec_frame_buffering;
			break;
		}
	}

	if ((sps->bitstream_restriction_flag) &&
		(sps->max_dec_frame_buffering <
		sps->num_reorder_frames)) {
		sps->num_reorder_frames = sps->max_dec_frame_buffering;
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER,
			"set reorder_pic_num to %d\n",
			sps->num_reorder_frames);
	}

	if (!sps->sar.den)
		sps->sar.den = 1;
/*out:*/
	if (1) {
		static const char csp[4][5] = { "Gray", "420", "422", "444" };
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER,
			"sps:%u profile:%d/%d poc:%d ref:%d %dx%d %s %s crop:%u/%u/%u/%u %s %s %d/%d b%d reo:%d\n",
			sps_id, sps->profile_idc, sps->level_idc,
			sps->poc_type,
			sps->ref_frame_count,
			sps->mb_width, sps->mb_height,
			sps->frame_mbs_only_flag ? "FRM" : (sps->mb_aff ? "MB-AFF" : "PIC-AFF"),
			sps->direct_8x8_inference_flag ? "8B8" : "",
			sps->crop_left, sps->crop_right,
			sps->crop_top, sps->crop_bottom,
			sps->vui_parameters_present_flag ? "VUI" : "",
			csp[sps->chroma_format_idc],
			sps->timing_info_present_flag ? sps->num_units_in_tick : 0,
			sps->timing_info_present_flag ? sps->time_scale : 0,
			sps->bit_depth_luma,
			sps->bitstream_restriction_flag ? sps->num_reorder_frames : -1);
	}

	return 0;

fail:
	return -1;
}

static const char *h264_nal_type_name[32] = {
	"Unspecified 0", //H264_NAL_UNSPECIFIED
	"Coded slice of a non-IDR picture", // H264_NAL_SLICE
	"Coded slice data partition A", // H264_NAL_DPA
	"Coded slice data partition B", // H264_NAL_DPB
	"Coded slice data partition C", // H264_NAL_DPC
	"IDR", // H264_NAL_IDR_SLICE
	"SEI", // H264_NAL_SEI
	"SPS", // H264_NAL_SPS
	"PPS", // H264_NAL_PPS
	"AUD", // H264_NAL_AUD
	"End of sequence", // H264_NAL_END_SEQUENCE
	"End of stream", // H264_NAL_END_STREAM
	"Filler data", // H264_NAL_FILLER_DATA
	"SPS extension", // H264_NAL_SPS_EXT
	"Prefix", // H264_NAL_PREFIX
	"Subset SPS", // H264_NAL_SUB_SPS
	"Depth parameter set", // H264_NAL_DPS
	"Reserved 17", // H264_NAL_RESERVED17
	"Reserved 18", // H264_NAL_RESERVED18
	"Auxiliary coded picture without partitioning", // H264_NAL_AUXILIARY_SLICE
	"Slice extension", // H264_NAL_EXTEN_SLICE
	"Slice extension for a depth view or a 3D-AVC texture view", // H264_NAL_DEPTH_EXTEN_SLICE
	"Reserved 22", // H264_NAL_RESERVED22
	"Reserved 23", // H264_NAL_RESERVED23
	"Unspecified 24", // H264_NAL_UNSPECIFIED24
	"Unspecified 25", // H264_NAL_UNSPECIFIED25
	"Unspecified 26", // H264_NAL_UNSPECIFIED26
	"Unspecified 27", // H264_NAL_UNSPECIFIED27
	"Unspecified 28", // H264_NAL_UNSPECIFIED28
	"Unspecified 29", // H264_NAL_UNSPECIFIED29
	"Unspecified 30", // H264_NAL_UNSPECIFIED30
	"Unspecified 31", // H264_NAL_UNSPECIFIED31
};

static const char *h264_nal_unit_name(int nal_type)
{
	return h264_nal_type_name[nal_type];
}

static int decode_extradata_ps(u8 *data, int size, struct h264_param_sets *ps)
{
	int ret = 0;
	struct get_bits_context gb;
	u32 src_len, rbsp_size = 0;
	u8 *rbsp_buf = NULL;
	int ref_idc, nalu_pos;
	u32 nal_type;
	u8 *p = data;
	u32 len = size;

	nalu_pos = find_start_code(p, len);
	if (nalu_pos < 0)
		return -1;

	src_len = calc_nal_len(p + nalu_pos, size - nalu_pos);
	rbsp_buf = nal_unit_extract_rbsp(p + nalu_pos, src_len, &rbsp_size);
	if (rbsp_buf == NULL)
		return -ENOMEM;

	ret = init_get_bits8(&gb, rbsp_buf, rbsp_size);
	if (ret < 0)
		goto out;

	if (get_bits1(&gb) != 0) {
		ret = -1;
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"invalid h264 data,return!\n");
		goto out;
	}

	ref_idc	 = get_bits(&gb, 2);
	nal_type = get_bits(&gb, 5);

	v4l_dbg(0, V4L_DEBUG_CODEC_PARSER,
		"nal_unit_type: %d(%s), nal_ref_idc: %d\n",
		nal_type, h264_nal_unit_name(nal_type), ref_idc);

	switch (nal_type) {
	case H264_NAL_SPS:
		ret = aml_h264_parser_sps(&gb, &ps->sps);
		if (ret < 0)
			goto out;
		ps->sps_parsed = true;
		break;
	/*case H264_NAL_PPS:
		ret = ff_h264_decode_picture_parameter_set(&gb, &ps->pps, rbsp_size);
		if (ret < 0)
			goto fail;
		ps->pps_parsed = true;
		break;*/
	default:
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
			"Unsupport parser nal type (%s).\n",
			h264_nal_unit_name(nal_type));
		break;
	}

out:
	vfree(rbsp_buf);

	return ret;
}

int h264_decode_extradata_ps(u8 *buf, int size, struct h264_param_sets *ps)
{
	int ret = 0, i = 0, j = 0;
	u8 *p = buf;
	int len = size;

	for (i = 4; i < size; i++) {
		j = find_start_code(p, len);
		if (j > 0) {
			len = size - (p - buf);
			ret = decode_extradata_ps(p, len, ps);
			if (ret) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR,
					"parse extra data failed. err: %d\n", ret);
				return ret;
			}

			if (ps->sps_parsed)
				break;

			p += j;
		}
		p++;
	}

	return ret;
}


