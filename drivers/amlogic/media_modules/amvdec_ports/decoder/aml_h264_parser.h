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
#ifndef AML_H264_PARSER_H
#define AML_H264_PARSER_H

#include "../aml_vcodec_drv.h"
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
#include "../utils/pixfmt.h"
#endif

#define QP_MAX_NUM (51 + 6 * 6)           // The maximum supported qp

/* NAL unit types */
enum {
	H264_NAL_SLICE           = 1,
	H264_NAL_DPA             = 2,
	H264_NAL_DPB             = 3,
	H264_NAL_DPC             = 4,
	H264_NAL_IDR_SLICE       = 5,
	H264_NAL_SEI             = 6,
	H264_NAL_SPS             = 7,
	H264_NAL_PPS             = 8,
	H264_NAL_AUD             = 9,
	H264_NAL_END_SEQUENCE    = 10,
	H264_NAL_END_STREAM      = 11,
	H264_NAL_FILLER_DATA     = 12,
	H264_NAL_SPS_EXT         = 13,
	H264_NAL_AUXILIARY_SLICE = 19,
};

enum {
	// 7.4.2.1.1: seq_parameter_set_id is in [0, 31].
	H264_MAX_SPS_COUNT = 32,
	// 7.4.2.2: pic_parameter_set_id is in [0, 255].
	H264_MAX_PPS_COUNT = 256,

	// A.3: MaxDpbFrames is bounded above by 16.
	H264_MAX_DPB_FRAMES = 16,
	// 7.4.2.1.1: max_num_ref_frames is in [0, MaxDpbFrames], and
	// each reference frame can have two fields.
	H264_MAX_REFS       = 2 * H264_MAX_DPB_FRAMES,

	// 7.4.3.1: modification_of_pic_nums_idc is not equal to 3 at most
	// num_ref_idx_lN_active_minus1 + 1 times (that is, once for each
	// possible reference), then equal to 3 once.
	H264_MAX_RPLM_COUNT = H264_MAX_REFS + 1,

	// 7.4.3.3: in the worst case, we begin with a full short-term
	// reference picture list.  Each picture in turn is moved to the
	// long-term list (type 3) and then discarded from there (type 2).
	// Then, we set the length of the long-term list (type 4), mark
	// the current picture as long-term (type 6) and terminate the
	// process (type 0).
	H264_MAX_MMCO_COUNT = H264_MAX_REFS * 2 + 3,

	// A.2.1, A.2.3: profiles supporting FMO constrain
	// num_slice_groups_minus1 to be in [0, 7].
	H264_MAX_SLICE_GROUPS = 8,

	// E.2.2: cpb_cnt_minus1 is in [0, 31].
	H264_MAX_CPB_CNT = 32,

	// A.3: in table A-1 the highest level allows a MaxFS of 139264.
	H264_MAX_MB_PIC_SIZE = 139264,
	// A.3.1, A.3.2: PicWidthInMbs and PicHeightInMbs are constrained
	// to be not greater than sqrt(MaxFS * 8).  Hence height/width are
	// bounded above by sqrt(139264 * 8) = 1055.5 macroblocks.
	H264_MAX_MB_WIDTH    = 1055,
	H264_MAX_MB_HEIGHT   = 1055,
	H264_MAX_WIDTH       = H264_MAX_MB_WIDTH  * 16,
	H264_MAX_HEIGHT      = H264_MAX_MB_HEIGHT * 16,
};

/**
 * Rational number (pair of numerator and denominator).
 */
struct rational{
	int num; ///< Numerator
	int den; ///< Denominator
};

/**
 * Sequence parameter set
 */
struct h264_SPS_t {
	u32 sps_id;
	int profile_idc;
	int level_idc;
	int chroma_format_idc;
	int transform_bypass;              ///< qpprime_y_zero_transform_bypass_flag
	int log2_max_frame_num;            ///< log2_max_frame_num_minus4 + 4
	int poc_type;                      ///< pic_order_cnt_type
	int log2_max_poc_lsb;              ///< log2_max_pic_order_cnt_lsb_minus4
	int delta_pic_order_always_zero_flag;
	int offset_for_non_ref_pic;
	int offset_for_top_to_bottom_field;
	int poc_cycle_length;              ///< num_ref_frames_in_pic_order_cnt_cycle
	int ref_frame_count;               ///< num_ref_frames
	int gaps_in_frame_num_allowed_flag;
	int mb_width;                      ///< pic_width_in_mbs_minus1 + 1
	///< (pic_height_in_map_units_minus1 + 1) * (2 - frame_mbs_only_flag)
	int mb_height;
	int frame_mbs_only_flag;
	int mb_aff;                        ///< mb_adaptive_frame_field_flag
	int direct_8x8_inference_flag;
	int crop;                          ///< frame_cropping_flag

	/* those 4 are already in luma samples */
	u32 crop_left;            ///< frame_cropping_rect_left_offset
	u32 crop_right;           ///< frame_cropping_rect_right_offset
	u32 crop_top;             ///< frame_cropping_rect_top_offset
	u32 crop_bottom;          ///< frame_cropping_rect_bottom_offset
	int vui_parameters_present_flag;
	struct rational sar;
	int video_signal_type_present_flag;
	int full_range;
	int colour_description_present_flag;
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
	enum AVColorPrimaries color_primaries;
	enum AVColorTransferCharacteristic color_trc;
	enum AVColorSpace colorspace;
#endif
	int timing_info_present_flag;
	u32 num_units_in_tick;
	u32 time_scale;
	int fixed_frame_rate_flag;
	int32_t offset_for_ref_frame[256];
	int bitstream_restriction_flag;
	int num_reorder_frames;
	int max_dec_frame_buffering;
	int scaling_matrix_present;
	u8 scaling_matrix4[6][16];
	u8 scaling_matrix8[6][64];
	int nal_hrd_parameters_present_flag;
	int vcl_hrd_parameters_present_flag;
	int pic_struct_present_flag;
	int time_offset_length;
	int cpb_cnt;                          ///< See H.264 E.1.2
	int initial_cpb_removal_delay_length; ///< initial_cpb_removal_delay_length_minus1 + 1
	int cpb_removal_delay_length;         ///< cpb_removal_delay_length_minus1 + 1
	int dpb_output_delay_length;          ///< dpb_output_delay_length_minus1 + 1
	int bit_depth_luma;                   ///< bit_depth_luma_minus8 + 8
	int bit_depth_chroma;                 ///< bit_depth_chroma_minus8 + 8
	int residual_color_transform_flag;    ///< residual_colour_transform_flag
	int constraint_set_flags;             ///< constraint_set[0-3]_flag
} ;

/**
 * Picture parameter set
 */
struct h264_PPS_t {
	u32 sps_id;
	int cabac;                  ///< entropy_coding_mode_flag
	int pic_order_present;      ///< pic_order_present_flag
	int slice_group_count;      ///< num_slice_groups_minus1 + 1
	int mb_slice_group_map_type;
	u32 ref_count[2];  ///< num_ref_idx_l0/1_active_minus1 + 1
	int weighted_pred;          ///< weighted_pred_flag
	int weighted_bipred_idc;
	int init_qp;                ///< pic_init_qp_minus26 + 26
	int init_qs;                ///< pic_init_qs_minus26 + 26
	int chroma_qp_index_offset[2];
	int deblocking_filter_parameters_present; ///< deblocking_filter_parameters_present_flag
	int constrained_intra_pred;     ///< constrained_intra_pred_flag
	int redundant_pic_cnt_present;  ///< redundant_pic_cnt_present_flag
	int transform_8x8_mode;         ///< transform_8x8_mode_flag
	u8 scaling_matrix4[6][16];
	u8 scaling_matrix8[6][64];
	u8 chroma_qp_table[2][87+1];  ///< pre-scaled (with chroma_qp_index_offset) version of qp_table
	int chroma_qp_diff;
	u8 data[4096];
	int data_size;

	u32 dequant4_buffer[6][87 + 1][16];
	u32 dequant8_buffer[6][87 + 1][64];
	u32(*dequant4_coeff[6])[16];
	u32(*dequant8_coeff[6])[64];
} ;

struct h264_param_sets {
	bool sps_parsed;
	bool pps_parsed;
	struct h264_SPS_t sps;
	struct h264_PPS_t pps;
};


#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
int h264_decode_extradata_ps(u8 *data, int size, struct h264_param_sets *ps);
#else
inline int h264_decode_extradata_ps(u8 *data, int size, struct h264_param_sets *ps) { return -1; }
#endif

#endif /* AML_H264_PARSER_H */

