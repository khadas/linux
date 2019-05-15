/*
 * drivers/amlogic/media_modules/amvdec_ports/decoder/h264_parse.h
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

#ifndef _H264_PARSE_H
#define _H264_PARSE_H

#include "h264_stream.h"

enum color_model {
	CM_UNKNOWN = -1,
	CM_YUV     =  0,
	CM_RGB     =  1,
	CM_XYZ     =  2
};

enum color_format {
	CF_UNKNOWN = -1,     //!< Unknown color format
	YUV400     =  0,     //!< Monochrome
	YUV420     =  1,     //!< 4:2:0
	YUV422     =  2,     //!< 4:2:2
	YUV444     =  3      //!< 4:4:4
};

enum pixel_format {
	PF_UNKNOWN = -1,     //!< Unknown color ordering
	UYVY       =  0,     //!< UYVY
	YUY2       =  1,     //!< YUY2
	YUYV       =  1,     //!< YUYV
	YVYU       =  2,     //!< YVYU
	BGR        =  3,     //!< BGR
	V210       =  4      //!< Video Clarity 422 format (10 bits)
};

//AVC Profile IDC definitions
enum profile_idc{
	NO_PROFILE     =  0,       //!< disable profile checking for experimental coding (enables FRExt, but disables MV)
	FREXT_CAVLC444 = 44,       //!< YUV 4:4:4/14 "CAVLC 4:4:4"
	BASELINE       = 66,       //!< YUV 4:2:0/8  "Baseline"
	MAIN           = 77,       //!< YUV 4:2:0/8  "Main"
	EXTENDED       = 88,       //!< YUV 4:2:0/8  "Extended"
	FREXT_HP       = 100,      //!< YUV 4:2:0/8  "High"
	FREXT_Hi10P    = 110,      //!< YUV 4:2:0/10 "High 10"
	FREXT_Hi422    = 122,      //!< YUV 4:2:2/10 "High 4:2:2"
	FREXT_Hi444    = 244,      //!< YUV 4:4:4/14 "High 4:4:4"
	MVC_HIGH       = 118,      //!< YUV 4:2:0/8  "Multiview High"
	STEREO_HIGH    = 128       //!< YUV 4:2:0/8  "Stereo High"
};

/* sequence parameter set */
struct h264_SPS_t {
	bool		vailid;
	unsigned int	profile_idc;
	bool		constrained_set0_flag;
	bool		constrained_set1_flag;
	bool		constrained_set2_flag;
	bool		constrained_set3_flag;
	unsigned int	level_idc;
	unsigned int	seq_parameter_set_id;
	unsigned int	chroma_format_idc;
	bool		seq_scaling_matrix_present_flag;
	int		seq_scaling_list_present_flag[12];
	int		ScalingList4x4[6][16];
	int		ScalingList8x8[6][64];
	bool		UseDefaultScalingMatrix4x4Flag[6];
	bool		UseDefaultScalingMatrix8x8Flag[6];
	unsigned int	bit_depth_luma_minus8;
	unsigned int	bit_depth_chroma_minus8;
	unsigned int	log2_max_frame_num_minus4;
	unsigned int	pic_order_cnt_type;
	unsigned int	log2_max_pic_order_cnt_lsb_minus4;
	bool		delta_pic_order_always_zero_flag;
	int		offset_for_non_ref_pic;
	int		offset_for_top_to_bottom_field;
	unsigned int	num_ref_frames_in_poc_cycle;
	int		offset_for_ref_frame[255];
	int		num_ref_frames;
	bool		gaps_in_frame_num_value_allowed_flag;
	unsigned int	pic_width_in_mbs_minus1;
	unsigned int	pic_height_in_map_units_minus1;
	bool		frame_mbs_only_flag;
	bool		mb_adaptive_frame_field_flag;
	bool		direct_8x8_inference_flag;
	bool		frame_cropping_flag;
	unsigned int	frame_crop_left_offset;
	unsigned int	frame_crop_right_offset;
	unsigned int	frame_crop_top_offset;
	unsigned int	frame_crop_bottom_offset;
	bool		vui_parameters_present_flag;
	//h264_vui_parameters_t *vui_parameters;
	unsigned  separate_colour_plane_flag;
	int lossless_qpprime_flag;
};

/* pic parameter set */
struct h264_PPS_t {
	int pic_parameter_set_id;
	int seq_parameter_set_id;
	int entropy_coding_mode_flag;
	int pic_order_present_flag;
	int num_slice_groups_minus1;
	int slice_group_map_type;
	int run_length_minus1;
	int top_left;
	int bottom_right;
	int slice_group_change_direction_flag;
	int slice_group_change_rate_minus1;
	int pic_size_in_map_units_minus1;
	int slice_group_id;
	int num_ref_idx_l0_active_minus1;
	int num_ref_idx_l1_active_minus1;
	int weighted_pred_flag;
	int weighted_bipred_idc;
	int pic_init_qp_minus26;
	int pic_init_qs_minus26;
	int chroma_qp_index_offset;
	int deblocking_filter_control_present_flag;
	int constrained_intra_pred_flag;
	int redundant_pic_cnt_present_flag;
};

void h264_sps_parse(struct h264_stream_t *s, struct h264_SPS_t *sps);
void h264_pps_parse(struct h264_stream_t *s, struct h264_PPS_t *pps);

void h264_sps_info(struct h264_SPS_t *sps);
void h264_pps_info(struct h264_PPS_t *pps);

#endif //_H264_PARSE_H
