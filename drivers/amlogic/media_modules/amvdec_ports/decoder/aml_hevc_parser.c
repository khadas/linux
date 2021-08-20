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

#include "aml_hevc_parser.h"
#include "../utils/get_bits.h"
#include "../utils/put_bits.h"
#include "../utils/golomb.h"
#include "../utils/common.h"
#include "utils.h"

const u8 ff_hevc_diag_scan4x4_x[16] = {
	0, 0, 1, 0,
	1, 2, 0, 1,
	2, 3, 1, 2,
	3, 2, 3, 3,
};

const u8 ff_hevc_diag_scan4x4_y[16] = {
	0, 1, 0, 2,
	1, 0, 3, 2,
	1, 0, 3, 2,
	1, 3, 2, 3,
};

const u8 ff_hevc_diag_scan8x8_x[64] = {
	0, 0, 1, 0,
	1, 2, 0, 1,
	2, 3, 0, 1,
	2, 3, 4, 0,
	1, 2, 3, 4,
	5, 0, 1, 2,
	3, 4, 5, 6,
	0, 1, 2, 3,
	4, 5, 6, 7,
	1, 2, 3, 4,
	5, 6, 7, 2,
	3, 4, 5, 6,
	7, 3, 4, 5,
	6, 7, 4, 5,
	6, 7, 5, 6,
	7, 6, 7, 7,
};

const u8 ff_hevc_diag_scan8x8_y[64] = {
	0, 1, 0, 2,
	1, 0, 3, 2,
	1, 0, 4, 3,
	2, 1, 0, 5,
	4, 3, 2, 1,
	0, 6, 5, 4,
	3, 2, 1, 0,
	7, 6, 5, 4,
	3, 2, 1, 0,
	7, 6, 5, 4,
	3, 2, 1, 7,
	6, 5, 4, 3,
	2, 7, 6, 5,
	4, 3, 7, 6,
	5, 4, 7, 6,
	5, 7, 6, 7,
};

static const u8 default_scaling_list_intra[] = {
	16, 16, 16, 16, 17, 18, 21, 24,
	16, 16, 16, 16, 17, 19, 22, 25,
	16, 16, 17, 18, 20, 22, 25, 29,
	16, 16, 18, 21, 24, 27, 31, 36,
	17, 17, 20, 24, 30, 35, 41, 47,
	18, 19, 22, 27, 35, 44, 54, 65,
	21, 22, 25, 31, 41, 54, 70, 88,
	24, 25, 29, 36, 47, 65, 88, 115
};

static const u8 default_scaling_list_inter[] = {
	16, 16, 16, 16, 17, 18, 20, 24,
	16, 16, 16, 17, 18, 20, 24, 25,
	16, 16, 17, 18, 20, 24, 25, 28,
	16, 17, 18, 20, 24, 25, 28, 33,
	17, 18, 20, 24, 25, 28, 33, 41,
	18, 20, 24, 25, 28, 33, 41, 54,
	20, 24, 25, 28, 33, 41, 54, 71,
	24, 25, 28, 33, 41, 54, 71, 91
};

static const struct AVRational vui_sar[] = {
	{  0,   1 },
	{  1,   1 },
	{ 12,  11 },
	{ 10,  11 },
	{ 16,  11 },
	{ 40,  33 },
	{ 24,  11 },
	{ 20,  11 },
	{ 32,  11 },
	{ 80,  33 },
	{ 18,  11 },
	{ 15,  11 },
	{ 64,  33 },
	{ 160, 99 },
	{  4,   3 },
	{  3,   2 },
	{  2,   1 },
};

static const u8 hevc_sub_width_c[] = {
	1, 2, 2, 1
};

static const u8 hevc_sub_height_c[] = {
	1, 2, 1, 1
};

static int decode_profile_tier_level(struct get_bits_context *gb, struct PTLCommon *ptl)
{
	int i;

	if (get_bits_left(gb) < 2+1+5 + 32 + 4 + 16 + 16 + 12)
		return -1;

	ptl->profile_space = get_bits(gb, 2);
	ptl->tier_flag     = get_bits1(gb);
	ptl->profile_idc   = get_bits(gb, 5);
	if (ptl->profile_idc == FF_PROFILE_HEVC_MAIN)
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Main profile bitstream\n");
	else if (ptl->profile_idc == FF_PROFILE_HEVC_MAIN_10)
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Main 10 profile bitstream\n");
	else if (ptl->profile_idc == FF_PROFILE_HEVC_MAIN_STILL_PICTURE)
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Main Still Picture profile bitstream\n");
	else if (ptl->profile_idc == FF_PROFILE_HEVC_REXT)
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Range Extension profile bitstream\n");
	else
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Unknown HEVC profile: %d\n", ptl->profile_idc);

	for (i = 0; i < 32; i++) {
		ptl->profile_compatibility_flag[i] = get_bits1(gb);

		if (ptl->profile_idc == 0 && i > 0 && ptl->profile_compatibility_flag[i])
			ptl->profile_idc = i;
	}
	ptl->progressive_source_flag    = get_bits1(gb);
	ptl->interlaced_source_flag     = get_bits1(gb);
	ptl->non_packed_constraint_flag = get_bits1(gb);
	ptl->frame_only_constraint_flag = get_bits1(gb);

	skip_bits(gb, 16); // XXX_reserved_zero_44bits[0..15]
	skip_bits(gb, 16); // XXX_reserved_zero_44bits[16..31]
	skip_bits(gb, 12); // XXX_reserved_zero_44bits[32..43]

	return 0;
}

static int parse_ptl(struct get_bits_context *gb, struct PTL *ptl, int max_num_sub_layers)
{
	int i;
	if (decode_profile_tier_level(gb, &ptl->general_ptl) < 0 ||
		get_bits_left(gb) < 8 + (8*2 * (max_num_sub_layers - 1 > 0))) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "PTL information too short\n");
		return -1;
	}

	ptl->general_ptl.level_idc = get_bits(gb, 8);

	for (i = 0; i < max_num_sub_layers - 1; i++) {
		ptl->sub_layer_profile_present_flag[i] = get_bits1(gb);
		ptl->sub_layer_level_present_flag[i]   = get_bits1(gb);
	}

	if (max_num_sub_layers - 1> 0)
		for (i = max_num_sub_layers - 1; i < 8; i++)
			skip_bits(gb, 2); // reserved_zero_2bits[i]
	for (i = 0; i < max_num_sub_layers - 1; i++) {
		if (ptl->sub_layer_profile_present_flag[i] &&
			decode_profile_tier_level(gb, &ptl->sub_layer_ptl[i]) < 0) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "PTL information for sublayer %i too short\n", i);
			return -1;
		}
		if (ptl->sub_layer_level_present_flag[i]) {
			if (get_bits_left(gb) < 8) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Not enough data for sublayer %i level_idc\n", i);
				return -1;
			} else
				ptl->sub_layer_ptl[i].level_idc = get_bits(gb, 8);
		}
	}

	return 0;
}

static void decode_sublayer_hrd(struct get_bits_context *gb,
	u32 nb_cpb, int subpic_params_present)
{
	int i;

	for (i = 0; i < nb_cpb; i++) {
		get_ue_golomb_long(gb); // bit_rate_value_minus1
		get_ue_golomb_long(gb); // cpb_size_value_minus1

		if (subpic_params_present) {
			get_ue_golomb_long(gb); // cpb_size_du_value_minus1
			get_ue_golomb_long(gb); // bit_rate_du_value_minus1
		}
		skip_bits1(gb); // cbr_flag
	}
}

static int decode_hrd(struct get_bits_context *gb,
	int common_inf_present, int max_sublayers)
{
	int nal_params_present = 0, vcl_params_present = 0;
	int subpic_params_present = 0;
	int i;

	if (common_inf_present) {
		nal_params_present = get_bits1(gb);
		vcl_params_present = get_bits1(gb);

		if (nal_params_present || vcl_params_present) {
			subpic_params_present = get_bits1(gb);

			if (subpic_params_present) {
				skip_bits(gb, 8); // tick_divisor_minus2
				skip_bits(gb, 5); // du_cpb_removal_delay_increment_length_minus1
				skip_bits(gb, 1); // sub_pic_cpb_params_in_pic_timing_sei_flag
				skip_bits(gb, 5); // dpb_output_delay_du_length_minus1
			}

			skip_bits(gb, 4); // bit_rate_scale
			skip_bits(gb, 4); // cpb_size_scale

			if (subpic_params_present)
				skip_bits(gb, 4);  // cpb_size_du_scale

			skip_bits(gb, 5); // initial_cpb_removal_delay_length_minus1
			skip_bits(gb, 5); // au_cpb_removal_delay_length_minus1
			skip_bits(gb, 5); // dpb_output_delay_length_minus1
		}
	}

	for (i = 0; i < max_sublayers; i++) {
		int low_delay = 0;
		u32 nb_cpb = 1;
		int fixed_rate = get_bits1(gb);

		if (!fixed_rate)
			fixed_rate = get_bits1(gb);

		if (fixed_rate)
			get_ue_golomb_long(gb);  // elemental_duration_in_tc_minus1
		else
			low_delay = get_bits1(gb);

		if (!low_delay) {
			nb_cpb = get_ue_golomb_long(gb) + 1;
			if (nb_cpb < 1 || nb_cpb > 32) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "nb_cpb %d invalid\n", nb_cpb);
				return -1;
			}
		}

		if (nal_params_present)
			decode_sublayer_hrd(gb, nb_cpb, subpic_params_present);
		if (vcl_params_present)
			decode_sublayer_hrd(gb, nb_cpb, subpic_params_present);
	}
	return 0;
}

int ff_hevc_parse_vps(struct get_bits_context *gb, struct h265_VPS_t *vps)
{
	int i,j;
	int vps_id = 0;

	v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Decoding VPS\n");

	vps_id = get_bits(gb, 4);
	if (vps_id >= HEVC_MAX_VPS_COUNT) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "VPS id out of range: %d\n", vps_id);
		goto err;
	}

	if (get_bits(gb, 2) != 3) { // vps_reserved_three_2bits
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "vps_reserved_three_2bits is not three\n");
		goto err;
	}

	vps->vps_max_layers	= get_bits(gb, 6) + 1;
	vps->vps_max_sub_layers	= get_bits(gb, 3) + 1;
	vps->vps_temporal_id_nesting_flag = get_bits1(gb);

	if (get_bits(gb, 16) != 0xffff) { // vps_reserved_ffff_16bits
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "vps_reserved_ffff_16bits is not 0xffff\n");
		goto err;
	}

	if (vps->vps_max_sub_layers > HEVC_MAX_SUB_LAYERS) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "vps_max_sub_layers out of range: %d\n",
			vps->vps_max_sub_layers);
		goto err;
	}

	if (parse_ptl(gb, &vps->ptl, vps->vps_max_sub_layers) < 0)
		goto err;

	vps->vps_sub_layer_ordering_info_present_flag = get_bits1(gb);

	i = vps->vps_sub_layer_ordering_info_present_flag ? 0 : vps->vps_max_sub_layers - 1;
	for (; i < vps->vps_max_sub_layers; i++) {
		vps->vps_max_dec_pic_buffering[i]	= get_ue_golomb_long(gb) + 1;
		vps->vps_num_reorder_pics[i]		= get_ue_golomb_long(gb);
		vps->vps_max_latency_increase[i]	= get_ue_golomb_long(gb) - 1;

		if (vps->vps_max_dec_pic_buffering[i] > HEVC_MAX_DPB_SIZE || !vps->vps_max_dec_pic_buffering[i]) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "vps_max_dec_pic_buffering_minus1 out of range: %d\n",
				vps->vps_max_dec_pic_buffering[i] - 1);
			goto err;
		}
		if (vps->vps_num_reorder_pics[i] > vps->vps_max_dec_pic_buffering[i] - 1) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "vps_max_num_reorder_pics out of range: %d\n",
				vps->vps_num_reorder_pics[i]);
			goto err;
		}
	}

	vps->vps_max_layer_id   = get_bits(gb, 6);
	vps->vps_num_layer_sets = get_ue_golomb_long(gb) + 1;
	if (vps->vps_num_layer_sets < 1 || vps->vps_num_layer_sets > 1024 ||
		(vps->vps_num_layer_sets - 1LL) * (vps->vps_max_layer_id + 1LL) > get_bits_left(gb)) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "too many layer_id_included_flags\n");
		goto err;
	}

	for (i = 1; i < vps->vps_num_layer_sets; i++)
		for (j = 0; j <= vps->vps_max_layer_id; j++)
			skip_bits(gb, 1);  // layer_id_included_flag[i][j]

	vps->vps_timing_info_present_flag = get_bits1(gb);
	if (vps->vps_timing_info_present_flag) {
		vps->vps_num_units_in_tick	= get_bits_long(gb, 32);
		vps->vps_time_scale		= get_bits_long(gb, 32);
		vps->vps_poc_proportional_to_timing_flag = get_bits1(gb);
		if (vps->vps_poc_proportional_to_timing_flag)
			vps->vps_num_ticks_poc_diff_one = get_ue_golomb_long(gb) + 1;
		vps->vps_num_hrd_parameters = get_ue_golomb_long(gb);
		if (vps->vps_num_hrd_parameters > (u32)vps->vps_num_layer_sets) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "vps_num_hrd_parameters %d is invalid\n", vps->vps_num_hrd_parameters);
			goto err;
		}
		for (i = 0; i < vps->vps_num_hrd_parameters; i++) {
			int common_inf_present = 1;

			get_ue_golomb_long(gb); // hrd_layer_set_idx
			if (i)
				common_inf_present = get_bits1(gb);
			decode_hrd(gb, common_inf_present, vps->vps_max_sub_layers);
		}
	}
	get_bits1(gb); /* vps_extension_flag */

	if (get_bits_left(gb) < 0) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Overread VPS by %d bits\n", -get_bits_left(gb));
		goto err;
	}

	return 0;
err:
	return -1;
}

static int map_pixel_format(struct h265_SPS_t *sps)
{
	/*const AVPixFmtDescriptor *desc;*/
	switch (sps->bit_depth) {
	case 8:
		if (sps->chroma_format_idc == 0) sps->pix_fmt = AV_PIX_FMT_GRAY8;
		if (sps->chroma_format_idc == 1) sps->pix_fmt = AV_PIX_FMT_YUV420P;
		if (sps->chroma_format_idc == 2) sps->pix_fmt = AV_PIX_FMT_YUV422P;
		if (sps->chroma_format_idc == 3) sps->pix_fmt = AV_PIX_FMT_YUV444P;
		break;
	case 9:
		if (sps->chroma_format_idc == 0) sps->pix_fmt = AV_PIX_FMT_GRAY9;
		if (sps->chroma_format_idc == 1) sps->pix_fmt = AV_PIX_FMT_YUV420P9;
		if (sps->chroma_format_idc == 2) sps->pix_fmt = AV_PIX_FMT_YUV422P9;
		if (sps->chroma_format_idc == 3) sps->pix_fmt = AV_PIX_FMT_YUV444P9;
		break;
	case 10:
		if (sps->chroma_format_idc == 0) sps->pix_fmt = AV_PIX_FMT_GRAY10;
		if (sps->chroma_format_idc == 1) sps->pix_fmt = AV_PIX_FMT_YUV420P10;
		if (sps->chroma_format_idc == 2) sps->pix_fmt = AV_PIX_FMT_YUV422P10;
		if (sps->chroma_format_idc == 3) sps->pix_fmt = AV_PIX_FMT_YUV444P10;
		break;
	case 12:
		if (sps->chroma_format_idc == 0) sps->pix_fmt = AV_PIX_FMT_GRAY12;
		if (sps->chroma_format_idc == 1) sps->pix_fmt = AV_PIX_FMT_YUV420P12;
		if (sps->chroma_format_idc == 2) sps->pix_fmt = AV_PIX_FMT_YUV422P12;
		if (sps->chroma_format_idc == 3) sps->pix_fmt = AV_PIX_FMT_YUV444P12;
		break;
	default:
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "The following bit-depths are currently specified: 8, 9, 10 and 12 bits, "
			"chroma_format_idc is %d, depth is %d\n",
			sps->chroma_format_idc, sps->bit_depth);
		return -1;
	}

	/*desc = av_pix_fmt_desc_get(sps->pix_fmt);
	if (!desc)
		return AVERROR(EINVAL);

	sps->hshift[0] = sps->vshift[0] = 0;
	sps->hshift[2] = sps->hshift[1] = desc->log2_chroma_w;
	sps->vshift[2] = sps->vshift[1] = desc->log2_chroma_h;*/

	sps->pixel_shift = sps->bit_depth > 8;

	return 0;
}

static void set_default_scaling_list_data(struct ScalingList *sl)
{
	int matrixId;

	for (matrixId = 0; matrixId < 6; matrixId++) {
		// 4x4 default is 16
		memset(sl->sl[0][matrixId], 16, 16);
		sl->sl_dc[0][matrixId] = 16; // default for 16x16
		sl->sl_dc[1][matrixId] = 16; // default for 32x32
	}
	memcpy(sl->sl[1][0], default_scaling_list_intra, 64);
	memcpy(sl->sl[1][1], default_scaling_list_intra, 64);
	memcpy(sl->sl[1][2], default_scaling_list_intra, 64);
	memcpy(sl->sl[1][3], default_scaling_list_inter, 64);
	memcpy(sl->sl[1][4], default_scaling_list_inter, 64);
	memcpy(sl->sl[1][5], default_scaling_list_inter, 64);
	memcpy(sl->sl[2][0], default_scaling_list_intra, 64);
	memcpy(sl->sl[2][1], default_scaling_list_intra, 64);
	memcpy(sl->sl[2][2], default_scaling_list_intra, 64);
	memcpy(sl->sl[2][3], default_scaling_list_inter, 64);
	memcpy(sl->sl[2][4], default_scaling_list_inter, 64);
	memcpy(sl->sl[2][5], default_scaling_list_inter, 64);
	memcpy(sl->sl[3][0], default_scaling_list_intra, 64);
	memcpy(sl->sl[3][1], default_scaling_list_intra, 64);
	memcpy(sl->sl[3][2], default_scaling_list_intra, 64);
	memcpy(sl->sl[3][3], default_scaling_list_inter, 64);
	memcpy(sl->sl[3][4], default_scaling_list_inter, 64);
	memcpy(sl->sl[3][5], default_scaling_list_inter, 64);
}

static int scaling_list_data(struct get_bits_context *gb,
	struct ScalingList *sl, struct h265_SPS_t *sps)
{
	u8 scaling_list_pred_mode_flag;
	int scaling_list_dc_coef[2][6];
	int size_id, matrix_id, pos;
	int i;

	for (size_id = 0; size_id < 4; size_id++)
		for (matrix_id = 0; matrix_id < 6; matrix_id += ((size_id == 3) ? 3 : 1)) {
			scaling_list_pred_mode_flag = get_bits1(gb);
			if (!scaling_list_pred_mode_flag) {
				u32 delta = get_ue_golomb_long(gb);
				/* Only need to handle non-zero delta. Zero means default,
				* which should already be in the arrays. */
				if (delta) {
					// Copy from previous array.
					delta *= (size_id == 3) ? 3 : 1;
					if (matrix_id < delta) {
						v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid delta in scaling list data: %d.\n", delta);
						return -1;
					}

					memcpy(sl->sl[size_id][matrix_id],
						sl->sl[size_id][matrix_id - delta],
						size_id > 0 ? 64 : 16);
					if (size_id > 1)
						sl->sl_dc[size_id - 2][matrix_id] = sl->sl_dc[size_id - 2][matrix_id - delta];
				}
			} else {
				int next_coef, coef_num;
				int scaling_list_delta_coef;

				next_coef = 8;
				coef_num = FFMIN(64, 1 << (4 + (size_id << 1)));
				if (size_id > 1) {
					scaling_list_dc_coef[size_id - 2][matrix_id] = get_se_golomb(gb) + 8;
					next_coef = scaling_list_dc_coef[size_id - 2][matrix_id];
					sl->sl_dc[size_id - 2][matrix_id] = next_coef;
				}
				for (i = 0; i < coef_num; i++) {
					if (size_id == 0)
						pos = 4 * ff_hevc_diag_scan4x4_y[i] +
							ff_hevc_diag_scan4x4_x[i];
					else
						pos = 8 * ff_hevc_diag_scan8x8_y[i] +
							ff_hevc_diag_scan8x8_x[i];

					scaling_list_delta_coef = get_se_golomb(gb);
					next_coef = (next_coef + 256U + scaling_list_delta_coef) % 256;
					sl->sl[size_id][matrix_id][pos] = next_coef;
				}
			}
		}

	if (sps->chroma_format_idc == 3) {
		for (i = 0; i < 64; i++) {
			sl->sl[3][1][i] = sl->sl[2][1][i];
			sl->sl[3][2][i] = sl->sl[2][2][i];
			sl->sl[3][4][i] = sl->sl[2][4][i];
			sl->sl[3][5][i] = sl->sl[2][5][i];
		}
		sl->sl_dc[1][1] = sl->sl_dc[0][1];
		sl->sl_dc[1][2] = sl->sl_dc[0][2];
		sl->sl_dc[1][4] = sl->sl_dc[0][4];
		sl->sl_dc[1][5] = sl->sl_dc[0][5];
	}

	return 0;
}

int ff_hevc_decode_short_term_rps(struct get_bits_context *gb,
	struct ShortTermRPS *rps, const struct h265_SPS_t *sps, int is_slice_header)
{
	u8 rps_predict = 0;
	int delta_poc;
	int k0 = 0;
	int k1 = 0;
	int k  = 0;
	int i;

	if (rps != sps->st_rps && sps->nb_st_rps)
		rps_predict = get_bits1(gb);

	if (rps_predict) {
		const struct ShortTermRPS *rps_ridx;
		int delta_rps;
		u32 abs_delta_rps;
		u8 use_delta_flag = 0;
		u8 delta_rps_sign;

		if (is_slice_header) {
			u32 delta_idx = get_ue_golomb_long(gb) + 1;
			if (delta_idx > sps->nb_st_rps) {
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid value of delta_idx in slice header RPS: %d > %d.\n",
					delta_idx, sps->nb_st_rps);
				return -1;
			}
			rps_ridx = &sps->st_rps[sps->nb_st_rps - delta_idx];
			rps->rps_idx_num_delta_pocs = rps_ridx->num_delta_pocs;
		} else
			rps_ridx = &sps->st_rps[rps - sps->st_rps - 1];

		delta_rps_sign = get_bits1(gb);
		abs_delta_rps  = get_ue_golomb_long(gb) + 1;
		if (abs_delta_rps < 1 || abs_delta_rps > 32768) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid value of abs_delta_rps: %d\n",
				abs_delta_rps);
			return -1;
		}
		delta_rps = (1 - (delta_rps_sign << 1)) * abs_delta_rps;
		for (i = 0; i <= rps_ridx->num_delta_pocs; i++) {
			int used = rps->used[k] = get_bits1(gb);

			if (!used)
				use_delta_flag = get_bits1(gb);

			if (used || use_delta_flag) {
				if (i < rps_ridx->num_delta_pocs)
					delta_poc = delta_rps + rps_ridx->delta_poc[i];
				else
					delta_poc = delta_rps;
				rps->delta_poc[k] = delta_poc;
				if (delta_poc < 0)
					k0++;
				else
					k1++;
				k++;
			}
		}

		if (k >= ARRAY_SIZE(rps->used)) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid num_delta_pocs: %d\n", k);
			return -1;
		}

		rps->num_delta_pocs	= k;
		rps->num_negative_pics	= k0;
		// sort in increasing order (smallest first)
		if (rps->num_delta_pocs != 0) {
			int used, tmp;
			for (i = 1; i < rps->num_delta_pocs; i++) {
				delta_poc	= rps->delta_poc[i];
				used		= rps->used[i];
				for (k = i - 1; k >= 0; k--) {
				tmp = rps->delta_poc[k];
					if (delta_poc < tmp) {
						rps->delta_poc[k + 1]	= tmp;
						rps->used[k + 1]	= rps->used[k];
						rps->delta_poc[k]	= delta_poc;
						rps->used[k]		= used;
					}
				}
			}
		}
		if ((rps->num_negative_pics >> 1) != 0) {
			int used;
			k = rps->num_negative_pics - 1;
			// flip the negative values to largest first
			for (i = 0; i < rps->num_negative_pics >> 1; i++) {
				delta_poc	= rps->delta_poc[i];
				used		= rps->used[i];
				rps->delta_poc[i] = rps->delta_poc[k];
				rps->used[i]	= rps->used[k];
				rps->delta_poc[k] = delta_poc;
				rps->used[k]	= used;
				k--;
			}
		}
	} else {
		u32 prev, nb_positive_pics;
		rps->num_negative_pics	= get_ue_golomb_long(gb);
		nb_positive_pics	= get_ue_golomb_long(gb);

		if (rps->num_negative_pics >= HEVC_MAX_REFS ||
			nb_positive_pics >= HEVC_MAX_REFS) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Too many refs in a short term RPS.\n");
			return -1;
		}

		rps->num_delta_pocs = rps->num_negative_pics + nb_positive_pics;
		if (rps->num_delta_pocs) {
			prev = 0;
			for (i = 0; i < rps->num_negative_pics; i++) {
				delta_poc = get_ue_golomb_long(gb) + 1;
				if (delta_poc < 1 || delta_poc > 32768) {
					v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid value of delta_poc: %d\n",
						delta_poc);
					return -1;
				}
				prev -= delta_poc;
				rps->delta_poc[i] = prev;
				rps->used[i] = get_bits1(gb);
			}
			prev = 0;
			for (i = 0; i < nb_positive_pics; i++) {
				delta_poc = get_ue_golomb_long(gb) + 1;
				if (delta_poc < 1 || delta_poc > 32768) {
					v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid value of delta_poc: %d\n",
						delta_poc);
					return -1;
				}
				prev += delta_poc;
				rps->delta_poc[rps->num_negative_pics + i] = prev;
				rps->used[rps->num_negative_pics + i] = get_bits1(gb);
			}
		}
	}
	return 0;
}

static void decode_vui(struct get_bits_context *gb, struct h265_SPS_t *sps)
{
	struct VUI backup_vui, *vui = &sps->vui;
	struct get_bits_context backup;
	int sar_present, alt = 0;

	v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Decoding VUI\n");

	sar_present = get_bits1(gb);
	if (sar_present) {
		u8 sar_idx = get_bits(gb, 8);
		if (sar_idx < ARRAY_SIZE(vui_sar))
			vui->sar = vui_sar[sar_idx];
		else if (sar_idx == 255) {
			vui->sar.num = get_bits(gb, 16);
			vui->sar.den = get_bits(gb, 16);
		} else
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER,
				"Unknown SAR index: %u.\n", sar_idx);
	}

	vui->overscan_info_present_flag = get_bits1(gb);
	if (vui->overscan_info_present_flag)
		vui->overscan_appropriate_flag = get_bits1(gb);

	vui->video_signal_type_present_flag = get_bits1(gb);
	if (vui->video_signal_type_present_flag) {
		vui->video_format		= get_bits(gb, 3);
		vui->video_full_range_flag	= get_bits1(gb);
		vui->colour_description_present_flag = get_bits1(gb);
		if (vui->video_full_range_flag && sps->pix_fmt == AV_PIX_FMT_YUV420P)
			sps->pix_fmt = AV_PIX_FMT_YUVJ420P;
		if (vui->colour_description_present_flag) {
			vui->colour_primaries		= get_bits(gb, 8);
			vui->transfer_characteristic	= get_bits(gb, 8);
			vui->matrix_coeffs		= get_bits(gb, 8);

			// Set invalid values to "unspecified"
			if (!av_color_primaries_name(vui->colour_primaries))
				vui->colour_primaries = AVCOL_PRI_UNSPECIFIED;
			if (!av_color_transfer_name(vui->transfer_characteristic))
				vui->transfer_characteristic = AVCOL_TRC_UNSPECIFIED;
			if (!av_color_space_name(vui->matrix_coeffs))
				vui->matrix_coeffs = AVCOL_SPC_UNSPECIFIED;
			if (vui->matrix_coeffs == AVCOL_SPC_RGB) {
				switch (sps->pix_fmt) {
				case AV_PIX_FMT_YUV444P:
					sps->pix_fmt = AV_PIX_FMT_GBRP;
					break;
				case AV_PIX_FMT_YUV444P10:
					sps->pix_fmt = AV_PIX_FMT_GBRP10;
					break;
				case AV_PIX_FMT_YUV444P12:
					sps->pix_fmt = AV_PIX_FMT_GBRP12;
					break;
				}
			}
		}
	}

	vui->chroma_loc_info_present_flag = get_bits1(gb);
	if (vui->chroma_loc_info_present_flag) {
		vui->chroma_sample_loc_type_top_field    = get_ue_golomb_long(gb);
		vui->chroma_sample_loc_type_bottom_field = get_ue_golomb_long(gb);
	}

	vui->neutra_chroma_indication_flag	= get_bits1(gb);
	vui->field_seq_flag			= get_bits1(gb);
	vui->frame_field_info_present_flag	= get_bits1(gb);

	// Backup context in case an alternate header is detected
	memcpy(&backup, gb, sizeof(backup));
	memcpy(&backup_vui, vui, sizeof(backup_vui));
	if (get_bits_left(gb) >= 68 && show_bits_long(gb, 21) == 0x100000) {
		vui->default_display_window_flag = 0;
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Invalid default display window\n");
	} else
		vui->default_display_window_flag = get_bits1(gb);

	if (vui->default_display_window_flag) {
		int vert_mult  = hevc_sub_height_c[sps->chroma_format_idc];
		int horiz_mult = hevc_sub_width_c[sps->chroma_format_idc];
		vui->def_disp_win.left_offset	= get_ue_golomb_long(gb) * horiz_mult;
		vui->def_disp_win.right_offset	= get_ue_golomb_long(gb) * horiz_mult;
		vui->def_disp_win.top_offset	= get_ue_golomb_long(gb) *  vert_mult;
		vui->def_disp_win.bottom_offset	= get_ue_golomb_long(gb) *  vert_mult;
	}

timing_info:
	vui->vui_timing_info_present_flag = get_bits1(gb);

	if (vui->vui_timing_info_present_flag) {
		if (get_bits_left(gb) < 66 && !alt) {
			// The alternate syntax seem to have timing info located
			// at where def_disp_win is normally located
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Strange VUI timing information, retrying...\n");
			memcpy(vui, &backup_vui, sizeof(backup_vui));
			memcpy(gb, &backup, sizeof(backup));
			alt = 1;
			goto timing_info;
		}
		vui->vui_num_units_in_tick	= get_bits_long(gb, 32);
		vui->vui_time_scale		= get_bits_long(gb, 32);
		if (alt) {
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Retry got %u/%u fps\n",
			vui->vui_time_scale, vui->vui_num_units_in_tick);
		}
		vui->vui_poc_proportional_to_timing_flag = get_bits1(gb);
		if (vui->vui_poc_proportional_to_timing_flag)
			vui->vui_num_ticks_poc_diff_one_minus1 = get_ue_golomb_long(gb);
		vui->vui_hrd_parameters_present_flag = get_bits1(gb);
		if (vui->vui_hrd_parameters_present_flag)
			decode_hrd(gb, 1, sps->max_sub_layers);
	}

	vui->bitstream_restriction_flag = get_bits1(gb);
	if (vui->bitstream_restriction_flag) {
		if (get_bits_left(gb) < 8 && !alt) {
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Strange VUI bitstream restriction information, retrying"
				" from timing information...\n");
			memcpy(vui, &backup_vui, sizeof(backup_vui));
			memcpy(gb, &backup, sizeof(backup));
			alt = 1;
			goto timing_info;
		}
		vui->tiles_fixed_structure_flag		= get_bits1(gb);
		vui->motion_vectors_over_pic_boundaries_flag = get_bits1(gb);
		vui->restricted_ref_pic_lists_flag	= get_bits1(gb);
		vui->min_spatial_segmentation_idc	= get_ue_golomb_long(gb);
		vui->max_bytes_per_pic_denom		= get_ue_golomb_long(gb);
		vui->max_bits_per_min_cu_denom		= get_ue_golomb_long(gb);
		vui->log2_max_mv_length_horizontal	= get_ue_golomb_long(gb);
		vui->log2_max_mv_length_vertical	= get_ue_golomb_long(gb);
	}

	if (get_bits_left(gb) < 1 && !alt) {
		// XXX: Alternate syntax when sps_range_extension_flag != 0?
		v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Overread in VUI, retrying from timing information...\n");
		memcpy(vui, &backup_vui, sizeof(backup_vui));
		memcpy(gb, &backup, sizeof(backup));
		alt = 1;
		goto timing_info;
	}
}

int ff_hevc_parse_sps(struct get_bits_context *gb, struct h265_SPS_t *sps)
{
	int i, ret = 0;
	int log2_diff_max_min_transform_block_size;
	int bit_depth_chroma, start, vui_present, sublayer_ordering_info;
	struct HEVCWindow *ow;

	sps->vps_id = get_bits(gb, 4);
	if (sps->vps_id >= HEVC_MAX_VPS_COUNT) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "VPS id out of range: %d\n", sps->vps_id);
		return -1;
	}

	sps->max_sub_layers = get_bits(gb, 3) + 1;
		if (sps->max_sub_layers > HEVC_MAX_SUB_LAYERS) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "sps_max_sub_layers out of range: %d\n",
				sps->max_sub_layers);
		return -1;
	}

	sps->temporal_id_nesting_flag = get_bits(gb, 1);

	if ((ret = parse_ptl(gb, &sps->ptl, sps->max_sub_layers)) < 0)
		return ret;

	sps->sps_id = get_ue_golomb_long(gb);
	if (sps->sps_id >= HEVC_MAX_SPS_COUNT) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "SPS id out of range: %d\n", sps->sps_id);
		return -1;
	}

	sps->chroma_format_idc = get_ue_golomb_long(gb);
	if (sps->chroma_format_idc > 3U) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "chroma_format_idc %d is invalid\n", sps->chroma_format_idc);
		return -1;
	}

	if (sps->chroma_format_idc == 3)
		sps->separate_colour_plane_flag = get_bits1(gb);

	if (sps->separate_colour_plane_flag)
		sps->chroma_format_idc = 0;

	sps->width	= get_ue_golomb_long(gb);
	sps->height	= get_ue_golomb_long(gb);
	if (sps->width > 8192 || sps->height > 8192) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "width or height oversize.\n");
		return -1;
	}

	if (get_bits1(gb)) { // pic_conformance_flag
		int vert_mult  = hevc_sub_height_c[sps->chroma_format_idc];
		int horiz_mult = hevc_sub_width_c[sps->chroma_format_idc];
		sps->pic_conf_win.left_offset	= get_ue_golomb_long(gb) * horiz_mult;
		sps->pic_conf_win.right_offset	= get_ue_golomb_long(gb) * horiz_mult;
		sps->pic_conf_win.top_offset	= get_ue_golomb_long(gb) *  vert_mult;
		sps->pic_conf_win.bottom_offset = get_ue_golomb_long(gb) *  vert_mult;
		sps->output_window = sps->pic_conf_win;
	}

	sps->bit_depth   = get_ue_golomb_long(gb) + 8;
	bit_depth_chroma = get_ue_golomb_long(gb) + 8;
	if (sps->chroma_format_idc && bit_depth_chroma != sps->bit_depth) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Luma bit depth (%d) is different from chroma bit depth (%d), this is unsupported.\n",
			sps->bit_depth, bit_depth_chroma);
		return -1;
	}
	sps->bit_depth_chroma = bit_depth_chroma;

	ret = map_pixel_format(sps);
	if (ret < 0)
		return ret;

	sps->log2_max_poc_lsb = get_ue_golomb_long(gb) + 4;
		if (sps->log2_max_poc_lsb > 16) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "log2_max_pic_order_cnt_lsb_minus4 out range: %d\n",
				sps->log2_max_poc_lsb - 4);
		return -1;
	}

	sublayer_ordering_info = get_bits1(gb);
	start = sublayer_ordering_info ? 0 : sps->max_sub_layers - 1;
	for (i = start; i < sps->max_sub_layers; i++) {
		sps->temporal_layer[i].max_dec_pic_buffering = get_ue_golomb_long(gb) + 1;
		sps->temporal_layer[i].num_reorder_pics      = get_ue_golomb_long(gb);
		sps->temporal_layer[i].max_latency_increase  = get_ue_golomb_long(gb) - 1;
		if (sps->temporal_layer[i].max_dec_pic_buffering > (u32)HEVC_MAX_DPB_SIZE) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "sps_max_dec_pic_buffering_minus1 out of range: %d\n",
				sps->temporal_layer[i].max_dec_pic_buffering - 1U);
			return -1;
		}
		if (sps->temporal_layer[i].num_reorder_pics > sps->temporal_layer[i].max_dec_pic_buffering - 1) {
			v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "sps_max_num_reorder_pics out of range: %d\n",
				sps->temporal_layer[i].num_reorder_pics);
			if (sps->temporal_layer[i].num_reorder_pics > HEVC_MAX_DPB_SIZE - 1) {
				return -1;
			}
			sps->temporal_layer[i].max_dec_pic_buffering = sps->temporal_layer[i].num_reorder_pics + 1;
		}
	}

	if (!sublayer_ordering_info) {
		for (i = 0; i < start; i++) {
			sps->temporal_layer[i].max_dec_pic_buffering = sps->temporal_layer[start].max_dec_pic_buffering;
			sps->temporal_layer[i].num_reorder_pics	 = sps->temporal_layer[start].num_reorder_pics;
			sps->temporal_layer[i].max_latency_increase  = sps->temporal_layer[start].max_latency_increase;
		}
	}

	sps->log2_min_cb_size		= get_ue_golomb_long(gb) + 3;
	sps->log2_diff_max_min_coding_block_size = get_ue_golomb_long(gb);
	sps->log2_min_tb_size		= get_ue_golomb_long(gb) + 2;
	log2_diff_max_min_transform_block_size = get_ue_golomb_long(gb);
	sps->log2_max_trafo_size	= log2_diff_max_min_transform_block_size + sps->log2_min_tb_size;

	if (sps->log2_min_cb_size < 3 || sps->log2_min_cb_size > 30) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid value %d for log2_min_cb_size", sps->log2_min_cb_size);
		return -1;
	}

	if (sps->log2_diff_max_min_coding_block_size > 30) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid value %d for log2_diff_max_min_coding_block_size", sps->log2_diff_max_min_coding_block_size);
		return -1;
	}

	if (sps->log2_min_tb_size >= sps->log2_min_cb_size || sps->log2_min_tb_size < 2) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid value for log2_min_tb_size");
		return -1;
	}

	if (log2_diff_max_min_transform_block_size < 0 || log2_diff_max_min_transform_block_size > 30) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid value %d for log2_diff_max_min_transform_block_size", log2_diff_max_min_transform_block_size);
		return -1;
	}

	sps->max_transform_hierarchy_depth_inter = get_ue_golomb_long(gb);
	sps->max_transform_hierarchy_depth_intra = get_ue_golomb_long(gb);

	sps->scaling_list_enable_flag = get_bits1(gb);
	if (sps->scaling_list_enable_flag) {
		set_default_scaling_list_data(&sps->scaling_list);

		if (get_bits1(gb)) {
			ret = scaling_list_data(gb, &sps->scaling_list, sps);
			if (ret < 0)
				return ret;
		}
	}

	sps->amp_enabled_flag	= get_bits1(gb);
	sps->sao_enabled	= get_bits1(gb);

	sps->pcm_enabled_flag	= get_bits1(gb);
	if (sps->pcm_enabled_flag) {
		sps->pcm.bit_depth = get_bits(gb, 4) + 1;
		sps->pcm.bit_depth_chroma = get_bits(gb, 4) + 1;
		sps->pcm.log2_min_pcm_cb_size = get_ue_golomb_long(gb) + 3;
		sps->pcm.log2_max_pcm_cb_size = sps->pcm.log2_min_pcm_cb_size +
			get_ue_golomb_long(gb);
		if (FFMAX(sps->pcm.bit_depth, sps->pcm.bit_depth_chroma) > sps->bit_depth) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "PCM bit depth (%d, %d) is greater than normal bit depth (%d)\n",
				sps->pcm.bit_depth, sps->pcm.bit_depth_chroma, sps->bit_depth);
			return -1;
		}

		sps->pcm.loop_filter_disable_flag = get_bits1(gb);
	}

	sps->nb_st_rps = get_ue_golomb_long(gb);
	if (sps->nb_st_rps > HEVC_MAX_SHORT_TERM_REF_PIC_SETS) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Too many short term RPS: %d.\n", sps->nb_st_rps);
		return -1;
	}
	for (i = 0; i < sps->nb_st_rps; i++) {
		if ((ret = ff_hevc_decode_short_term_rps(gb, &sps->st_rps[i], sps, 0)) < 0)
			return ret;
	}

	sps->long_term_ref_pics_present_flag = get_bits1(gb);
	if (sps->long_term_ref_pics_present_flag) {
		sps->num_long_term_ref_pics_sps = get_ue_golomb_long(gb);
		if (sps->num_long_term_ref_pics_sps > HEVC_MAX_LONG_TERM_REF_PICS) {
			v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Too many long term ref pics: %d.\n",
				sps->num_long_term_ref_pics_sps);
			return -1;
		}
		for (i = 0; i < sps->num_long_term_ref_pics_sps; i++) {
			sps->lt_ref_pic_poc_lsb_sps[i] = get_bits(gb, sps->log2_max_poc_lsb);
			sps->used_by_curr_pic_lt_sps_flag[i] = get_bits1(gb);
		}
	}

	sps->sps_temporal_mvp_enabled_flag = get_bits1(gb);
	sps->sps_strong_intra_smoothing_enable_flag = get_bits1(gb);
	sps->vui.sar = (struct AVRational){0, 1};
	vui_present = get_bits1(gb);
	if (vui_present)
		decode_vui(gb, sps);

	if (get_bits1(gb)) { // sps_extension_flag
		sps->sps_range_extension_flag = get_bits1(gb);
		skip_bits(gb, 7); //sps_extension_7bits = get_bits(gb, 7);
		if (sps->sps_range_extension_flag) {
			sps->transform_skip_rotation_enabled_flag = get_bits1(gb);
			sps->transform_skip_context_enabled_flag  = get_bits1(gb);
			sps->implicit_rdpcm_enabled_flag = get_bits1(gb);
			sps->explicit_rdpcm_enabled_flag = get_bits1(gb);
			sps->extended_precision_processing_flag = get_bits1(gb);
			if (sps->extended_precision_processing_flag)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "extended_precision_processing_flag not yet implemented\n");

			sps->intra_smoothing_disabled_flag = get_bits1(gb);
			sps->high_precision_offsets_enabled_flag = get_bits1(gb);
			if (sps->high_precision_offsets_enabled_flag)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "high_precision_offsets_enabled_flag not yet implemented\n");

			sps->persistent_rice_adaptation_enabled_flag = get_bits1(gb);
			sps->cabac_bypass_alignment_enabled_flag  = get_bits1(gb);
			if (sps->cabac_bypass_alignment_enabled_flag)
				v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "cabac_bypass_alignment_enabled_flag not yet implemented\n");
		}
	}

	ow = &sps->output_window;
	if (ow->left_offset >= INT_MAX - ow->right_offset	  ||
		ow->top_offset	>= INT_MAX - ow->bottom_offset	  ||
		ow->left_offset + ow->right_offset  >= sps->width ||
		ow->top_offset	+ ow->bottom_offset >= sps->height) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid cropping offsets: %u/%u/%u/%u\n",
			ow->left_offset, ow->right_offset, ow->top_offset, ow->bottom_offset);
		return -1;
	}

	// Inferred parameters
	sps->log2_ctb_size = sps->log2_min_cb_size +
	sps->log2_diff_max_min_coding_block_size;
	sps->log2_min_pu_size = sps->log2_min_cb_size - 1;

	if (sps->log2_ctb_size > HEVC_MAX_LOG2_CTB_SIZE) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "CTB size out of range: 2^%d\n", sps->log2_ctb_size);
		return -1;
	}
	if (sps->log2_ctb_size < 4) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "log2_ctb_size %d differs from the bounds of any known profile\n", sps->log2_ctb_size);
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "log2_ctb_size %d", sps->log2_ctb_size);
		return -1;
	}

	sps->ctb_width  = (sps->width  + (1 << sps->log2_ctb_size) - 1) >> sps->log2_ctb_size;
	sps->ctb_height = (sps->height + (1 << sps->log2_ctb_size) - 1) >> sps->log2_ctb_size;
	sps->ctb_size   = sps->ctb_width * sps->ctb_height;

	sps->min_cb_width  = sps->width  >> sps->log2_min_cb_size;
	sps->min_cb_height = sps->height >> sps->log2_min_cb_size;
	sps->min_tb_width  = sps->width  >> sps->log2_min_tb_size;
	sps->min_tb_height = sps->height >> sps->log2_min_tb_size;
	sps->min_pu_width  = sps->width  >> sps->log2_min_pu_size;
	sps->min_pu_height = sps->height >> sps->log2_min_pu_size;
	sps->tb_mask       = (1 << (sps->log2_ctb_size - sps->log2_min_tb_size)) - 1;
	sps->qp_bd_offset = 6 * (sps->bit_depth - 8);

	if (av_mod_uintp2(sps->width, sps->log2_min_cb_size) ||
		av_mod_uintp2(sps->height, sps->log2_min_cb_size)) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Invalid coded frame dimensions.\n");
		return -1;
	}

	if (sps->max_transform_hierarchy_depth_inter > sps->log2_ctb_size - sps->log2_min_tb_size) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "max_transform_hierarchy_depth_inter out of range: %d\n",
			sps->max_transform_hierarchy_depth_inter);
		return -1;
	}
	if (sps->max_transform_hierarchy_depth_intra > sps->log2_ctb_size - sps->log2_min_tb_size) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "max_transform_hierarchy_depth_intra out of range: %d\n",
			sps->max_transform_hierarchy_depth_intra);
		return -1;
	}
	if (sps->log2_max_trafo_size > FFMIN(sps->log2_ctb_size, 5)) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "max transform block size out of range: %d\n",
			sps->log2_max_trafo_size);
			return -1;
	}

	if (get_bits_left(gb) < 0) {
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Overread SPS by %d bits\n", -get_bits_left(gb));
		return -1;
	}

	v4l_dbg(0, V4L_DEBUG_CODEC_PARSER, "Parsed SPS: id %d; ref: %d, coded wxh: %dx%d, cropped wxh: %dx%d; pix_fmt: %d.\n",
	       sps->sps_id, sps->temporal_layer[0].num_reorder_pics, sps->width, sps->height,
	       sps->width - (sps->output_window.left_offset + sps->output_window.right_offset),
	       sps->height - (sps->output_window.top_offset + sps->output_window.bottom_offset),
	       sps->pix_fmt);

	return 0;
}

const char *hevc_nal_type_name[64] = {
	"TRAIL_N", // HEVC_NAL_TRAIL_N
	"TRAIL_R", // HEVC_NAL_TRAIL_R
	"TSA_N", // HEVC_NAL_TSA_N
	"TSA_R", // HEVC_NAL_TSA_R
	"STSA_N", // HEVC_NAL_STSA_N
	"STSA_R", // HEVC_NAL_STSA_R
	"RADL_N", // HEVC_NAL_RADL_N
	"RADL_R", // HEVC_NAL_RADL_R
	"RASL_N", // HEVC_NAL_RASL_N
	"RASL_R", // HEVC_NAL_RASL_R
	"RSV_VCL_N10", // HEVC_NAL_VCL_N10
	"RSV_VCL_R11", // HEVC_NAL_VCL_R11
	"RSV_VCL_N12", // HEVC_NAL_VCL_N12
	"RSV_VLC_R13", // HEVC_NAL_VCL_R13
	"RSV_VCL_N14", // HEVC_NAL_VCL_N14
	"RSV_VCL_R15", // HEVC_NAL_VCL_R15
	"BLA_W_LP", // HEVC_NAL_BLA_W_LP
	"BLA_W_RADL", // HEVC_NAL_BLA_W_RADL
	"BLA_N_LP", // HEVC_NAL_BLA_N_LP
	"IDR_W_RADL", // HEVC_NAL_IDR_W_RADL
	"IDR_N_LP", // HEVC_NAL_IDR_N_LP
	"CRA_NUT", // HEVC_NAL_CRA_NUT
	"IRAP_IRAP_VCL22", // HEVC_NAL_IRAP_VCL22
	"IRAP_IRAP_VCL23", // HEVC_NAL_IRAP_VCL23
	"RSV_VCL24", // HEVC_NAL_RSV_VCL24
	"RSV_VCL25", // HEVC_NAL_RSV_VCL25
	"RSV_VCL26", // HEVC_NAL_RSV_VCL26
	"RSV_VCL27", // HEVC_NAL_RSV_VCL27
	"RSV_VCL28", // HEVC_NAL_RSV_VCL28
	"RSV_VCL29", // HEVC_NAL_RSV_VCL29
	"RSV_VCL30", // HEVC_NAL_RSV_VCL30
	"RSV_VCL31", // HEVC_NAL_RSV_VCL31
	"VPS", // HEVC_NAL_VPS
	"SPS", // HEVC_NAL_SPS
	"PPS", // HEVC_NAL_PPS
	"AUD", // HEVC_NAL_AUD
	"EOS_NUT", // HEVC_NAL_EOS_NUT
	"EOB_NUT", // HEVC_NAL_EOB_NUT
	"FD_NUT", // HEVC_NAL_FD_NUT
	"SEI_PREFIX", // HEVC_NAL_SEI_PREFIX
	"SEI_SUFFIX", // HEVC_NAL_SEI_SUFFIX
	"RSV_NVCL41", // HEVC_NAL_RSV_NVCL41
	"RSV_NVCL42", // HEVC_NAL_RSV_NVCL42
	"RSV_NVCL43", // HEVC_NAL_RSV_NVCL43
	"RSV_NVCL44", // HEVC_NAL_RSV_NVCL44
	"RSV_NVCL45", // HEVC_NAL_RSV_NVCL45
	"RSV_NVCL46", // HEVC_NAL_RSV_NVCL46
	"RSV_NVCL47", // HEVC_NAL_RSV_NVCL47
	"UNSPEC48", // HEVC_NAL_UNSPEC48
	"UNSPEC49", // HEVC_NAL_UNSPEC49
	"UNSPEC50", // HEVC_NAL_UNSPEC50
	"UNSPEC51", // HEVC_NAL_UNSPEC51
	"UNSPEC52", // HEVC_NAL_UNSPEC52
	"UNSPEC53", // HEVC_NAL_UNSPEC53
	"UNSPEC54", // HEVC_NAL_UNSPEC54
	"UNSPEC55", // HEVC_NAL_UNSPEC55
	"UNSPEC56", // HEVC_NAL_UNSPEC56
	"UNSPEC57", // HEVC_NAL_UNSPEC57
	"UNSPEC58", // HEVC_NAL_UNSPEC58
	"UNSPEC59", // HEVC_NAL_UNSPEC59
	"UNSPEC60", // HEVC_NAL_UNSPEC60
	"UNSPEC61", // HEVC_NAL_UNSPEC61
	"UNSPEC62", // HEVC_NAL_UNSPEC62
	"UNSPEC63", // HEVC_NAL_UNSPEC63
};

static const char *hevc_nal_unit_name(int nal_type)
{
	return hevc_nal_type_name[nal_type];
}

/**
* Parse NAL units of found picture and decode some basic information.
*
* @param s parser context.
* @param avctx codec context.
* @param buf buffer with field/frame data.
* @param buf_size size of the buffer.
*/
static int decode_extradata_ps(u8 *data, int size, struct h265_param_sets *ps)
{
	int ret = 0;
	struct get_bits_context gb;
	u32 src_len, rbsp_size = 0;
	u8 *rbsp_buf = NULL;
	int nalu_pos, nuh_layer_id, temporal_id;
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
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "invalid data, return!\n");
		goto out;
	}

	nal_type	= get_bits(&gb, 6);
	nuh_layer_id	= get_bits(&gb, 6);
	temporal_id	= get_bits(&gb, 3) - 1;
	if (temporal_id < 0) {
		ret = -1;
		goto out;
	}

	/*pr_info("nal_unit_type: %d(%s), nuh_layer_id: %d, temporal_id: %d\n",
		nal_type, hevc_nal_unit_name(nal_type),
		nuh_layer_id, temporal_id);*/

	switch (nal_type) {
	case HEVC_NAL_VPS:
		ret = ff_hevc_parse_vps(&gb, &ps->vps);
		if (ret < 0)
			goto out;
		ps->vps_parsed = true;
		break;
	case HEVC_NAL_SPS:
		ret = ff_hevc_parse_sps(&gb, &ps->sps);
		if (ret < 0)
			goto out;
		ps->sps_parsed = true;
		break;
	/*case HEVC_NAL_PPS:
		ret = ff_hevc_decode_nal_pps(&gb, NULL, ps);
		if (ret < 0)
			goto out;
		ps->pps_parsed = true;
		break;*/
	default:
		v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "Unsupport parser nal type (%s).\n",
			hevc_nal_unit_name(nal_type));
		break;
	}

out:
	vfree(rbsp_buf);

	return ret;
}

int h265_decode_extradata_ps(u8 *buf, int size, struct h265_param_sets *ps)
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
				v4l_dbg(0, V4L_DEBUG_CODEC_ERROR, "parse extra data failed. err: %d\n", ret);
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

