/*
 * drivers/amlogic/media_modules/amvdec_ports/decoder/h264_parse.c
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

#include <linux/kernel.h>
#include "h264_parse.h"
#include "h264_stream.h"

static unsigned char h264_exp_golomb_bits[256] = {
	8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,
};

static unsigned char ZZ_SCAN[16] = {
	0,  1,  4,  8,  5,  2,  3,  6,  9, 12, 13, 10,  7, 11, 14, 15
};

static unsigned char ZZ_SCAN8[64] = {
	0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

unsigned int h264_u(struct h264_stream_t *s, unsigned int n)
{
	//if (n % 8 == 0) {
	//    return h264_stream_read_bytes(s, n / 8);
	//}
	return h264_stream_read_bits(s, n);
}

unsigned int h264_ue(struct h264_stream_t *s)
{
	unsigned int bits, read;
	unsigned char coded;

	bits = 0;
	while (true) {
		if (h264_stream_bytes_remaining(s) < 1) {
			read = h264_stream_peek_bits(s, s->bit_pos) <<
				(8 - s->bit_pos);
			break;
		}

		read = h264_stream_peek_bits(s, 8);
		if (bits > 16)
			break;

		if (read)
			break;

		h264_stream_read_bits(s, 8);
		bits += 8;
	}

	coded = h264_exp_golomb_bits[read];
	h264_stream_read_bits(s, coded);
	bits += coded;
	return h264_stream_read_bits(s, bits + 1) - 1;
}

int h264_se(struct h264_stream_t *s)
{
	unsigned int ret;

	ret = h264_ue(s);
	if (!(ret & 0x1)) {
		ret >>= 1;
		return (int)(-ret);
	}

	return (ret + 1) >> 1;
}

void h264_f(struct h264_stream_t *s, unsigned int n, unsigned int pattern)
{
	unsigned int val = h264_u(s, n);

	if (val != pattern) {
		pr_err("fixed-pattern doesn't match. expected: %x actual: %x\n",
			pattern, (unsigned int)val);
		return;
	}
}

void h264_rbsp_trailing_bits(struct h264_stream_t *s)
{
	h264_f(s, 1, 1);
	h264_f(s, s->bit_pos, 0);
}

// syntax for scaling list matrix values
void Scaling_List(int *scalingList, int sizeOfScalingList,
	bool *UseDefaultScalingMatrix, struct h264_stream_t *s)
{
	int j, scanj;
	int delta_scale, lastScale, nextScale;

	lastScale      = 8;
	nextScale      = 8;

	for (j = 0; j < sizeOfScalingList; j++) {
		scanj = (sizeOfScalingList == 16) ? ZZ_SCAN[j] : ZZ_SCAN8[j];

		if (nextScale != 0) {
			delta_scale = h264_ue(s);
			nextScale = (lastScale + delta_scale + 256) % 256;
			*UseDefaultScalingMatrix =
				(bool) (scanj == 0 && nextScale == 0);
		}

		scalingList[scanj] = (nextScale == 0) ? lastScale : nextScale;
		lastScale = scalingList[scanj];
	}
}

void h264_sps_parse(struct h264_stream_t *s, struct h264_SPS_t *sps)
{
	unsigned int i, n_ScalingList;

	sps->profile_idc = h264_u(s, 8);

	if ((sps->profile_idc != BASELINE) &&
		(sps->profile_idc != MAIN) &&
		(sps->profile_idc != EXTENDED) &&
		(sps->profile_idc != FREXT_HP) &&
		(sps->profile_idc != FREXT_Hi10P) &&
		(sps->profile_idc != FREXT_Hi422) &&
		(sps->profile_idc != FREXT_Hi444) &&
		(sps->profile_idc != FREXT_CAVLC444)
		&& (sps->profile_idc != MVC_HIGH)
		&& (sps->profile_idc != STEREO_HIGH)) {
		pr_err("Invalid Profile IDC (%d) encountered.\n",
			sps->profile_idc);
		return;
	}

	sps->constrained_set0_flag = h264_u(s, 1);
	sps->constrained_set1_flag = h264_u(s, 1);
	sps->constrained_set2_flag = h264_u(s, 1);
	h264_u(s, 5); // reserved_zero_5bits
	sps->level_idc = h264_u(s, 8);
	sps->seq_parameter_set_id = h264_ue(s);

	// Fidelity Range Extensions stuff
	sps->chroma_format_idc = 1;
	sps->bit_depth_luma_minus8   = 0;
	sps->bit_depth_chroma_minus8 = 0;
	sps->lossless_qpprime_flag   = 0;
	sps->separate_colour_plane_flag = 0;

	if ((sps->profile_idc == FREXT_HP) ||
		(sps->profile_idc == FREXT_Hi10P) ||
		(sps->profile_idc == FREXT_Hi422) ||
		(sps->profile_idc == FREXT_Hi444) ||
		(sps->profile_idc == FREXT_CAVLC444)) {
		sps->chroma_format_idc = h264_ue(s);

		if (sps->chroma_format_idc == YUV444)
			sps->separate_colour_plane_flag = h264_u(s, 1);

		sps->bit_depth_luma_minus8 = h264_ue(s);
		sps->bit_depth_chroma_minus8 = h264_ue(s);
		//checking;
		if ((sps->bit_depth_luma_minus8 + 8 > sizeof(unsigned short) * 8) ||
			(sps->bit_depth_chroma_minus8 + 8 > sizeof(unsigned short) * 8)) {
			pr_err("Source picture has higher bit depth than imgpel data type.\n");
			pr_err("Please recompile with larger data type for imgpel.\n");
		}

		sps->lossless_qpprime_flag = h264_u(s, 1);
		sps->seq_scaling_matrix_present_flag = h264_u(s, 1);
		if (sps->seq_scaling_matrix_present_flag) {
			n_ScalingList = (sps->chroma_format_idc != YUV444) ? 8 : 12;
			for (i = 0; i < n_ScalingList; i++) {
				sps->seq_scaling_list_present_flag[i] = h264_u(s, 1);
				if (sps->seq_scaling_list_present_flag[i]) {
					if (i < 6)
						Scaling_List(sps->ScalingList4x4[i], 16,
						&sps->UseDefaultScalingMatrix4x4Flag[i], s);
					else
						Scaling_List(sps->ScalingList8x8[i - 6],
						64, &sps->UseDefaultScalingMatrix8x8Flag[i - 6], s);
				}
			}
		}
	}

	sps->log2_max_frame_num_minus4 = h264_ue(s);
	sps->pic_order_cnt_type = h264_ue(s);
	if (sps->pic_order_cnt_type == 0) {
		sps->log2_max_pic_order_cnt_lsb_minus4 = h264_ue(s);
	} else if (sps->pic_order_cnt_type == 1) {
		sps->delta_pic_order_always_zero_flag = h264_se(s);
		sps->offset_for_non_ref_pic = h264_se(s);
		sps->offset_for_top_to_bottom_field = h264_se(s);
		sps->num_ref_frames_in_poc_cycle = h264_se(s);
		for (i = 0; i < sps->num_ref_frames_in_poc_cycle; ++i)
			sps->offset_for_ref_frame[i] = h264_se(s);
	}

	sps->num_ref_frames = h264_ue(s);
	sps->gaps_in_frame_num_value_allowed_flag = h264_u(s, 1);
	sps->pic_width_in_mbs_minus1 = h264_ue(s);
	sps->pic_height_in_map_units_minus1 = h264_ue(s);
	sps->frame_mbs_only_flag = h264_u(s, 1);
	if (!sps->frame_mbs_only_flag)
		sps->mb_adaptive_frame_field_flag = h264_u(s, 1);

	sps->direct_8x8_inference_flag = h264_u(s, 1);
	sps->frame_cropping_flag = h264_u(s, 1);
	if (sps->frame_cropping_flag) {
		sps->frame_crop_left_offset = h264_ue(s);
		sps->frame_crop_right_offset = h264_ue(s);
		sps->frame_crop_top_offset = h264_ue(s);
		sps->frame_crop_bottom_offset = h264_ue(s);
	}

	sps->vui_parameters_present_flag = h264_u(s, 1);
	//if (sps->vui_parameters_present_flag) {
	//	sps->vui_parameters = h264_vui_parameters(s);
	//}
	h264_rbsp_trailing_bits(s);
}

void h264_pps_parse(struct h264_stream_t *s, struct h264_PPS_t *pps)
{
	pps->pic_parameter_set_id = h264_ue(s);
	pps->seq_parameter_set_id = h264_ue(s);
	pps->entropy_coding_mode_flag = h264_u(s, 1);
	pps->pic_order_present_flag = h264_u(s, 1);
	pps->num_slice_groups_minus1 = h264_ue(s);
	if (pps->num_slice_groups_minus1 > 0) {
		pps->slice_group_map_type = h264_ue(s);
		if (pps->slice_group_map_type == 0) {
			pps->run_length_minus1 = h264_ue(s);
		} else if (pps->slice_group_map_type == 2) {
			pps->top_left = h264_ue(s);
			pps->bottom_right = h264_ue(s);
		} else if (pps->slice_group_map_type == 3 ||
			pps->slice_group_map_type == 4 ||
			pps->slice_group_map_type == 5) {
			pps->slice_group_change_direction_flag = h264_u(s, 1);
			pps->slice_group_change_rate_minus1 = h264_ue(s);
		} else if (pps->slice_group_map_type == 6) {
			pps->pic_size_in_map_units_minus1 = h264_ue(s);
			pps->slice_group_id = h264_ue(s);
		}
	}
	pps->num_ref_idx_l0_active_minus1 = h264_ue(s);
	pps->num_ref_idx_l1_active_minus1 = h264_ue(s);
	pps->weighted_pred_flag = h264_u(s, 1);
	pps->weighted_bipred_idc = h264_u(s, 2);
	pps->pic_init_qp_minus26 = h264_se(s);
	pps->pic_init_qs_minus26 = h264_se(s);
	pps->chroma_qp_index_offset = h264_se(s);
	pps->deblocking_filter_control_present_flag = h264_u(s, 1);
	pps->constrained_intra_pred_flag = h264_u(s, 1);
	pps->redundant_pic_cnt_present_flag = h264_u(s, 1);
	h264_rbsp_trailing_bits(s);
}

void h264_sps_info(struct h264_SPS_t *sps)
{
	int i;

	pr_info("sequence_parameter_set {\n");
	pr_info("    profile_idc: %d\n", sps->profile_idc);
	pr_info("    constraint_set0_flag: %d\n", sps->constrained_set0_flag);
	pr_info("    constraint_set1_flag: %d\n", sps->constrained_set1_flag);
	pr_info("    constraint_set2_flag: %d\n", sps->constrained_set2_flag);
	pr_info("    level_idc: %d\n", sps->level_idc);
	pr_info("    seq_parameter_set_id: %d\n", sps->seq_parameter_set_id);

	pr_info("    log2_max_frame_num_minus4: %d\n",
		sps->log2_max_frame_num_minus4);
	pr_info("    pic_order_cnt_type: %d\n", sps->pic_order_cnt_type);
	if (sps->pic_order_cnt_type == 0) {
		pr_info("    log2_max_pic_order_cnt_lsb_minus4: %d\n",
			sps->log2_max_pic_order_cnt_lsb_minus4);
	} else if (sps->pic_order_cnt_type == 1) {
		pr_info("    delta_pic_order_always_zero_flag: %d\n",
			sps->delta_pic_order_always_zero_flag);
		pr_info("    offset_for_non_ref_pic: %d\n",
			sps->offset_for_non_ref_pic);
		pr_info("    offset_for_top_to_bottom_field: %d\n",
			sps->offset_for_top_to_bottom_field);
		pr_info("    num_ref_frames_in_pic_order_cnt_cycle: %d\n",
			sps->num_ref_frames_in_poc_cycle);
		for (i = 0; i < sps->num_ref_frames_in_poc_cycle; ++i) {
			pr_info("    offset_for_ref_frame[%d]: %d\n", i,
				sps->offset_for_ref_frame[i]);
		}
	}
	pr_info("    num_ref_frames: %d\n", sps->num_ref_frames);
	pr_info("    gaps_in_frame_num_value_allowed_flag: %d\n",
		sps->gaps_in_frame_num_value_allowed_flag);
	pr_info("    pic_width_in_mbs_minus1: %d\n",
		sps->pic_width_in_mbs_minus1);
	pr_info("    pic_height_in_map_units_minus1: %d\n",
		sps->pic_height_in_map_units_minus1);
	pr_info("    frame_mbs_only_flag: %d\n",
		sps->frame_mbs_only_flag);
	pr_info("    mb_adaptive_frame_field_flag: %d\n",
		sps->mb_adaptive_frame_field_flag);
	pr_info("    direct_8x8_inference_flag: %d\n",
		sps->direct_8x8_inference_flag);
	pr_info("    frame_cropping_flag: %d\n",
		sps->frame_cropping_flag);
	if (sps->frame_cropping_flag) {
		pr_info("    frame_crop_left_offset: %d\n",
			sps->frame_crop_left_offset);
		pr_info("    frame_crop_right_offset: %d\n",
			sps->frame_crop_right_offset);
		pr_info("    frame_crop_top_offset: %d\n",
			sps->frame_crop_top_offset);
		pr_info("    frame_crop_bottom_offset: %d\n",
			sps->frame_crop_bottom_offset);
	}
	pr_info("    vui_parameters_present_flag: %d\n",
	sps->vui_parameters_present_flag);
	//if (sps->vui_parameters_present_flag) {
	//	h264_print_vui_parameters(sps->vui_parameters);
	//}

	pr_info("  }\n");
}

void h264_pps_info(struct h264_PPS_t *pps)
{
	pr_info("pic_parameter_set {\n");
	pr_info("    pic_parameter_set_id: %d\n",
		pps->pic_parameter_set_id);
	pr_info("    seq_parameter_set_id: %d\n",
		pps->seq_parameter_set_id);
	pr_info("    entropy_coding_mode_flag: %d\n",
		pps->entropy_coding_mode_flag);
	pr_info("    pic_order_present_flag: %d\n",
		pps->pic_order_present_flag);
	pr_info("    num_slice_groups_minus1: %d\n",
		pps->num_slice_groups_minus1);
	// FIXME: Code for slice groups is missing here.
	pr_info("    num_ref_idx_l0_active_minus1: %d\n",
	pps->num_ref_idx_l0_active_minus1);
	pr_info("    num_ref_idx_l1_active_minus1: %d\n",
		pps->num_ref_idx_l1_active_minus1);
	pr_info("    weighted_pred_flag: %d\n", pps->weighted_pred_flag);
	pr_info("    weighted_bipred_idc: %d\n", pps->weighted_bipred_idc);
	pr_info("    pic_init_qp_minus26: %d\n", pps->pic_init_qp_minus26);
	pr_info("    pic_init_qs_minus26: %d\n", pps->pic_init_qs_minus26);
	pr_info("    chroma_qp_index_offset: %d\n",
		pps->chroma_qp_index_offset);
	pr_info("    deblocking_filter_control_present_flag: %d\n",
		pps->deblocking_filter_control_present_flag);
	pr_info("    constrained_intra_pred_flag: %d\n",
		pps->constrained_intra_pred_flag);
	pr_info("    redundant_pic_cnt_present_flag: %d\n",
		pps->redundant_pic_cnt_present_flag);
	pr_info("  }\n");
}

