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
#ifndef AML_HEVC_PARSER_H
#define AML_HEVC_PARSER_H

#include "../aml_vcodec_drv.h"
#include "../utils/common.h"

#define MAX_DPB_SIZE				16 // A.4.1
#define MAX_REFS				16

#define MAX_NB_THREADS				16
#define SHIFT_CTB_WPP				2

/**
 * 7.4.2.1
 */
#define MAX_SUB_LAYERS				7
#define MAX_VPS_COUNT				16
#define MAX_SPS_COUNT				32
#define MAX_PPS_COUNT				256
#define MAX_SHORT_TERM_RPS_COUNT 		64
#define MAX_CU_SIZE				128

//TODO: check if this is really the maximum
#define MAX_TRANSFORM_DEPTH			5

#define MAX_TB_SIZE				32
#define MAX_PB_SIZE				64
#define MAX_LOG2_CTB_SIZE			6
#define MAX_QP					51
#define DEFAULT_INTRA_TC_OFFSET			2

#define HEVC_CONTEXTS				183

#define MRG_MAX_NUM_CANDS			5

#define L0					0
#define L1					1

#define EPEL_EXTRA_BEFORE			1
#define EPEL_EXTRA_AFTER			2
#define EPEL_EXTRA				3

#define FF_PROFILE_HEVC_MAIN			1
#define FF_PROFILE_HEVC_MAIN_10			2
#define FF_PROFILE_HEVC_MAIN_STILL_PICTURE	3
#define FF_PROFILE_HEVC_REXT			4

/**
 * Value of the luma sample at position (x, y) in the 2D array tab.
 */
#define SAMPLE(tab, x, y) ((tab)[(y) * s->sps->width + (x)])
#define SAMPLE_CTB(tab, x, y) ((tab)[(y) * min_cb_width + (x)])
#define SAMPLE_CBF(tab, x, y) ((tab)[((y) & ((1<<log2_trafo_size)-1)) * MAX_CU_SIZE + ((x) & ((1<<log2_trafo_size)-1))])

#define IS_IDR(s) (s->nal_unit_type == NAL_IDR_W_RADL || s->nal_unit_type == NAL_IDR_N_LP)
#define IS_BLA(s) (s->nal_unit_type == NAL_BLA_W_RADL || s->nal_unit_type == NAL_BLA_W_LP || \
                   s->nal_unit_type == NAL_BLA_N_LP)
#define IS_IRAP(s) (s->nal_unit_type >= 16 && s->nal_unit_type <= 23)

/**
 * Table 7-3: NAL unit type codes
 */
enum HEVCNALUnitType {
	HEVC_NAL_TRAIL_N    = 0,
	HEVC_NAL_TRAIL_R    = 1,
	HEVC_NAL_TSA_N      = 2,
	HEVC_NAL_TSA_R      = 3,
	HEVC_NAL_STSA_N     = 4,
	HEVC_NAL_STSA_R     = 5,
	HEVC_NAL_RADL_N     = 6,
	HEVC_NAL_RADL_R     = 7,
	HEVC_NAL_RASL_N     = 8,
	HEVC_NAL_RASL_R     = 9,
	HEVC_NAL_VCL_N10    = 10,
	HEVC_NAL_VCL_R11    = 11,
	HEVC_NAL_VCL_N12    = 12,
	HEVC_NAL_VCL_R13    = 13,
	HEVC_NAL_VCL_N14    = 14,
	HEVC_NAL_VCL_R15    = 15,
	HEVC_NAL_BLA_W_LP   = 16,
	HEVC_NAL_BLA_W_RADL = 17,
	HEVC_NAL_BLA_N_LP   = 18,
	HEVC_NAL_IDR_W_RADL = 19,
	HEVC_NAL_IDR_N_LP   = 20,
	HEVC_NAL_CRA_NUT    = 21,
	HEVC_NAL_IRAP_VCL22 = 22,
	HEVC_NAL_IRAP_VCL23 = 23,
	HEVC_NAL_RSV_VCL24  = 24,
	HEVC_NAL_RSV_VCL25  = 25,
	HEVC_NAL_RSV_VCL26  = 26,
	HEVC_NAL_RSV_VCL27  = 27,
	HEVC_NAL_RSV_VCL28  = 28,
	HEVC_NAL_RSV_VCL29  = 29,
	HEVC_NAL_RSV_VCL30  = 30,
	HEVC_NAL_RSV_VCL31  = 31,
	HEVC_NAL_VPS        = 32,
	HEVC_NAL_SPS        = 33,
	HEVC_NAL_PPS        = 34,
	HEVC_NAL_AUD        = 35,
	HEVC_NAL_EOS_NUT    = 36,
	HEVC_NAL_EOB_NUT    = 37,
	HEVC_NAL_FD_NUT     = 38,
	HEVC_NAL_SEI_PREFIX = 39,
	HEVC_NAL_SEI_SUFFIX = 40,
};

enum HEVCSliceType {
	HEVC_SLICE_B = 0,
	HEVC_SLICE_P = 1,
	HEVC_SLICE_I = 2,
};

enum {
	// 7.4.3.1: vps_max_layers_minus1 is in [0, 62].
	HEVC_MAX_LAYERS     = 63,
	// 7.4.3.1: vps_max_sub_layers_minus1 is in [0, 6].
	HEVC_MAX_SUB_LAYERS = 7,
	// 7.4.3.1: vps_num_layer_sets_minus1 is in [0, 1023].
	HEVC_MAX_LAYER_SETS = 1024,

	// 7.4.2.1: vps_video_parameter_set_id is u(4).
	HEVC_MAX_VPS_COUNT = 16,
	// 7.4.3.2.1: sps_seq_parameter_set_id is in [0, 15].
	HEVC_MAX_SPS_COUNT = 16,
	// 7.4.3.3.1: pps_pic_parameter_set_id is in [0, 63].
	HEVC_MAX_PPS_COUNT = 64,

	// A.4.2: MaxDpbSize is bounded above by 16.
	HEVC_MAX_DPB_SIZE = 16,
	// 7.4.3.1: vps_max_dec_pic_buffering_minus1[i] is in [0, MaxDpbSize - 1].
	HEVC_MAX_REFS     = HEVC_MAX_DPB_SIZE,

	// 7.4.3.2.1: num_short_term_ref_pic_sets is in [0, 64].
	HEVC_MAX_SHORT_TERM_REF_PIC_SETS = 64,
	// 7.4.3.2.1: num_long_term_ref_pics_sps is in [0, 32].
	HEVC_MAX_LONG_TERM_REF_PICS      = 32,

	// A.3: all profiles require that CtbLog2SizeY is in [4, 6].
	HEVC_MIN_LOG2_CTB_SIZE = 4,
	HEVC_MAX_LOG2_CTB_SIZE = 6,

	// E.3.2: cpb_cnt_minus1[i] is in [0, 31].
	HEVC_MAX_CPB_CNT = 32,

	// A.4.1: in table A.6 the highest level allows a MaxLumaPs of 35 651 584.
	HEVC_MAX_LUMA_PS = 35651584,
	// A.4.1: pic_width_in_luma_samples and pic_height_in_luma_samples are
	// constrained to be not greater than sqrt(MaxLumaPs * 8).  Hence height/
	// width are bounded above by sqrt(8 * 35651584) = 16888.2 samples.
	HEVC_MAX_WIDTH  = 16888,
	HEVC_MAX_HEIGHT = 16888,

	// A.4.1: table A.6 allows at most 22 tile rows for any level.
	HEVC_MAX_TILE_ROWS    = 22,
	// A.4.1: table A.6 allows at most 20 tile columns for any level.
	HEVC_MAX_TILE_COLUMNS = 20,

	// 7.4.7.1: in the worst case (tiles_enabled_flag and
	// entropy_coding_sync_enabled_flag are both set), entry points can be
	// placed at the beginning of every Ctb row in every tile, giving an
	// upper bound of (num_tile_columns_minus1 + 1) * PicHeightInCtbsY - 1.
	// Only a stream with very high resolution and perverse parameters could
	// get near that, though, so set a lower limit here with the maximum
	// possible value for 4K video (at most 135 16x16 Ctb rows).
	HEVC_MAX_ENTRY_POINT_OFFSETS = HEVC_MAX_TILE_COLUMNS * 135,
};

struct ShortTermRPS {
	u32 num_negative_pics;
	int num_delta_pocs;
	int rps_idx_num_delta_pocs;
	int delta_poc[32];
	u8 used[32];
};

struct LongTermRPS {
	int poc[32];
	u8 used[32];
	u8 nb_refs;
};

struct SliceHeader {
	u32 pps_id;

	///< address (in raster order) of the first block in the current slice segment
	u32   slice_segment_addr;
	///< address (in raster order) of the first block in the current slice
	u32   slice_addr;

	enum HEVCSliceType slice_type;

	int pic_order_cnt_lsb;

	u8 first_slice_in_pic_flag;
	u8 dependent_slice_segment_flag;
	u8 pic_output_flag;
	u8 colour_plane_id;

	///< RPS coded in the slice header itself is stored here
	int short_term_ref_pic_set_sps_flag;
	int short_term_ref_pic_set_size;
	struct ShortTermRPS slice_rps;
	const struct ShortTermRPS *short_term_rps;
	int long_term_ref_pic_set_size;
	struct LongTermRPS long_term_rps;
	u32 list_entry_lx[2][32];

	u8 rpl_modification_flag[2];
	u8 no_output_of_prior_pics_flag;
	u8 slice_temporal_mvp_enabled_flag;

	u32 nb_refs[2];

	u8 slice_sample_adaptive_offset_flag[3];
	u8 mvd_l1_zero_flag;

	u8 cabac_init_flag;
	u8 disable_deblocking_filter_flag; ///< slice_header_disable_deblocking_filter_flag
	u8 slice_loop_filter_across_slices_enabled_flag;
	u8 collocated_list;

	u32 collocated_ref_idx;

	int slice_qp_delta;
	int slice_cb_qp_offset;
	int slice_cr_qp_offset;

	u8 cu_chroma_qp_offset_enabled_flag;

	int beta_offset;    ///< beta_offset_div2 * 2
	int tc_offset;      ///< tc_offset_div2 * 2

	u32 max_num_merge_cand; ///< 5 - 5_minus_max_num_merge_cand

	u8 *entry_point_offset;
	int * offset;
	int * size;
	int num_entry_point_offsets;

	char slice_qp;

	u8 luma_log2_weight_denom;
	s16 chroma_log2_weight_denom;

	s16 luma_weight_l0[16];
	s16 chroma_weight_l0[16][2];
	s16 chroma_weight_l1[16][2];
	s16 luma_weight_l1[16];

	s16 luma_offset_l0[16];
	s16 chroma_offset_l0[16][2];

	s16 luma_offset_l1[16];
	s16 chroma_offset_l1[16][2];

	int slice_ctb_addr_rs;
};

struct HEVCWindow {
	u32 left_offset;
	u32 right_offset;
	u32 top_offset;
	u32 bottom_offset;
};

struct VUI {
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
	struct AVRational sar;
#endif
	int overscan_info_present_flag;
	int overscan_appropriate_flag;

	int video_signal_type_present_flag;
	int video_format;
	int video_full_range_flag;
	int colour_description_present_flag;
	u8 colour_primaries;
	u8 transfer_characteristic;
	u8 matrix_coeffs;

	int chroma_loc_info_present_flag;
	int chroma_sample_loc_type_top_field;
	int chroma_sample_loc_type_bottom_field;
	int neutra_chroma_indication_flag;

	int field_seq_flag;
	int frame_field_info_present_flag;

	int default_display_window_flag;
	struct HEVCWindow def_disp_win;

	int vui_timing_info_present_flag;
	u32 vui_num_units_in_tick;
	u32 vui_time_scale;
	int vui_poc_proportional_to_timing_flag;
	int vui_num_ticks_poc_diff_one_minus1;
	int vui_hrd_parameters_present_flag;

	int bitstream_restriction_flag;
	int tiles_fixed_structure_flag;
	int motion_vectors_over_pic_boundaries_flag;
	int restricted_ref_pic_lists_flag;
	int min_spatial_segmentation_idc;
	int max_bytes_per_pic_denom;
	int max_bits_per_min_cu_denom;
	int log2_max_mv_length_horizontal;
	int log2_max_mv_length_vertical;
};

struct PTLCommon {
    u8 profile_space;
    u8 tier_flag;
    u8 profile_idc;
    u8 profile_compatibility_flag[32];
    u8 level_idc;
    u8 progressive_source_flag;
    u8 interlaced_source_flag;
    u8 non_packed_constraint_flag;
    u8 frame_only_constraint_flag;
};

struct PTL {
    struct PTLCommon general_ptl;
    struct PTLCommon sub_layer_ptl[HEVC_MAX_SUB_LAYERS];

    u8 sub_layer_profile_present_flag[HEVC_MAX_SUB_LAYERS];
    u8 sub_layer_level_present_flag[HEVC_MAX_SUB_LAYERS];
};

struct h265_VPS_t {
	u8 vps_temporal_id_nesting_flag;
	int vps_max_layers;
	int vps_max_sub_layers; ///< vps_max_temporal_layers_minus1 + 1

	struct PTL ptl;
	int vps_sub_layer_ordering_info_present_flag;
	u32 vps_max_dec_pic_buffering[HEVC_MAX_SUB_LAYERS];
	u32 vps_num_reorder_pics[HEVC_MAX_SUB_LAYERS];
	u32 vps_max_latency_increase[HEVC_MAX_SUB_LAYERS];
	int vps_max_layer_id;
	int vps_num_layer_sets; ///< vps_num_layer_sets_minus1 + 1
	u8 vps_timing_info_present_flag;
	u32 vps_num_units_in_tick;
	u32 vps_time_scale;
	u8 vps_poc_proportional_to_timing_flag;
	int vps_num_ticks_poc_diff_one; ///< vps_num_ticks_poc_diff_one_minus1 + 1
	int vps_num_hrd_parameters;
};

struct ScalingList {
	/* This is a little wasteful, since sizeID 0 only needs 8 coeffs,
	* and size ID 3 only has 2 arrays, not 6. */
	u8 sl[4][6][64];
	u8 sl_dc[2][6];
};

struct h265_SPS_t {
	u8 vps_id;
	u8 sps_id;
	int chroma_format_idc;
	u8 separate_colour_plane_flag;

	struct HEVCWindow output_window;
	struct HEVCWindow pic_conf_win;

	int bit_depth;
	int bit_depth_chroma;
	int pixel_shift;
	int pix_fmt;

	u32 log2_max_poc_lsb;
	int pcm_enabled_flag;

	int max_sub_layers;
	struct {
		int max_dec_pic_buffering;
		int num_reorder_pics;
		int max_latency_increase;
	} temporal_layer[HEVC_MAX_SUB_LAYERS];
	u8 temporal_id_nesting_flag;

	struct VUI vui;
	struct PTL ptl;

	u8 scaling_list_enable_flag;
	struct ScalingList scaling_list;

	u32 nb_st_rps;
	struct ShortTermRPS st_rps[HEVC_MAX_SHORT_TERM_REF_PIC_SETS];

	u8 amp_enabled_flag;
	u8 sao_enabled;

	u8 long_term_ref_pics_present_flag;
	u16 lt_ref_pic_poc_lsb_sps[HEVC_MAX_LONG_TERM_REF_PICS];
	u8 used_by_curr_pic_lt_sps_flag[HEVC_MAX_LONG_TERM_REF_PICS];
	u8 num_long_term_ref_pics_sps;

	struct {
		u8 bit_depth;
		u8 bit_depth_chroma;
		u32 log2_min_pcm_cb_size;
		u32 log2_max_pcm_cb_size;
		u8 loop_filter_disable_flag;
	} pcm;
	u8 sps_temporal_mvp_enabled_flag;
	u8 sps_strong_intra_smoothing_enable_flag;

	u32 log2_min_cb_size;
	u32 log2_diff_max_min_coding_block_size;
	u32 log2_min_tb_size;
	u32 log2_max_trafo_size;
	u32 log2_ctb_size;
	u32 log2_min_pu_size;

	int max_transform_hierarchy_depth_inter;
	int max_transform_hierarchy_depth_intra;

	int sps_range_extension_flag;
	int transform_skip_rotation_enabled_flag;
	int transform_skip_context_enabled_flag;
	int implicit_rdpcm_enabled_flag;
	int explicit_rdpcm_enabled_flag;
	int extended_precision_processing_flag;
	int intra_smoothing_disabled_flag;
	int high_precision_offsets_enabled_flag;
	int persistent_rice_adaptation_enabled_flag;
	int cabac_bypass_alignment_enabled_flag;

	///< coded frame dimension in various units
	int width;
	int height;
	int ctb_width;
	int ctb_height;
	int ctb_size;
	int min_cb_width;
	int min_cb_height;
	int min_tb_width;
	int min_tb_height;
	int min_pu_width;
	int min_pu_height;
	int tb_mask;

	int hshift[3];
	int vshift[3];

	int qp_bd_offset;

	u8 data[4096];
	int data_size;
};

struct h265_PPS_t {
	u32 sps_id; ///< seq_parameter_set_id

	u8 sign_data_hiding_flag;

	u8 cabac_init_present_flag;

	int num_ref_idx_l0_default_active; ///< num_ref_idx_l0_default_active_minus1 + 1
	int num_ref_idx_l1_default_active; ///< num_ref_idx_l1_default_active_minus1 + 1
	int pic_init_qp_minus26;

	u8 constrained_intra_pred_flag;
	u8 transform_skip_enabled_flag;

	u8 cu_qp_delta_enabled_flag;
	int diff_cu_qp_delta_depth;

	int cb_qp_offset;
	int cr_qp_offset;
	u8 pic_slice_level_chroma_qp_offsets_present_flag;
	u8 weighted_pred_flag;
	u8 weighted_bipred_flag;
	u8 output_flag_present_flag;
	u8 transquant_bypass_enable_flag;

	u8 dependent_slice_segments_enabled_flag;
	u8 tiles_enabled_flag;
	u8 entropy_coding_sync_enabled_flag;

	int num_tile_columns;   ///< num_tile_columns_minus1 + 1
	int num_tile_rows;      ///< num_tile_rows_minus1 + 1
	u8 uniform_spacing_flag;
	u8 loop_filter_across_tiles_enabled_flag;

	u8 seq_loop_filter_across_slices_enabled_flag;

	u8 deblocking_filter_control_present_flag;
	u8 deblocking_filter_override_enabled_flag;
	u8 disable_dbf;
	int beta_offset;    ///< beta_offset_div2 * 2
	int tc_offset;      ///< tc_offset_div2 * 2

	u8 scaling_list_data_present_flag;
	struct ScalingList scaling_list;

	u8 lists_modification_present_flag;
	int log2_parallel_merge_level; ///< log2_parallel_merge_level_minus2 + 2
	int num_extra_slice_header_bits;
	u8 slice_header_extension_present_flag;
	u8 log2_max_transform_skip_block_size;
	u8 cross_component_prediction_enabled_flag;
	u8 chroma_qp_offset_list_enabled_flag;
	u8 diff_cu_chroma_qp_offset_depth;
	u8 chroma_qp_offset_list_len_minus1;
	char  cb_qp_offset_list[6];
	char  cr_qp_offset_list[6];
	u8 log2_sao_offset_scale_luma;
	u8 log2_sao_offset_scale_chroma;

	// Inferred parameters
	u32 *column_width;  ///< ColumnWidth
	u32 *row_height;    ///< RowHeight
	u32 *col_bd;        ///< ColBd
	u32 *row_bd;        ///< RowBd
	int *col_idxX;

	int *ctb_addr_rs_to_ts; ///< CtbAddrRSToTS
	int *ctb_addr_ts_to_rs; ///< CtbAddrTSToRS
	int *tile_id;           ///< TileId
	int *tile_pos_rs;       ///< TilePosRS
	int *min_tb_addr_zs;    ///< MinTbAddrZS
	int *min_tb_addr_zs_tab;///< MinTbAddrZS
};

struct h265_param_sets {
	bool vps_parsed;
	bool sps_parsed;
	bool pps_parsed;
	/* currently active parameter sets */
	struct h265_VPS_t vps;
	struct h265_SPS_t sps;
	struct h265_PPS_t pps;
};

#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
int h265_decode_extradata_ps(u8 *data, int size, struct h265_param_sets *ps);
#else
inline int h265_decode_extradata_ps(u8 *data, int size, struct h265_param_sets *ps) { return -1; }
#endif

#endif /* AML_HEVC_PARSER_H */

