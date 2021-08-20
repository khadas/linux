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
#ifndef AVCODEC_MPEG4VIDEO_H
#define AVCODEC_MPEG4VIDEO_H

#include "../aml_vcodec_drv.h"
#include "../utils/common.h"
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
#include "../utils/pixfmt.h"
#endif

//mpeg4 profile
#define FF_PROFILE_MPEG4_SIMPLE                     0
#define FF_PROFILE_MPEG4_SIMPLE_SCALABLE            1
#define FF_PROFILE_MPEG4_CORE                       2
#define FF_PROFILE_MPEG4_MAIN                       3
#define FF_PROFILE_MPEG4_N_BIT                      4
#define FF_PROFILE_MPEG4_SCALABLE_TEXTURE           5
#define FF_PROFILE_MPEG4_SIMPLE_FACE_ANIMATION      6
#define FF_PROFILE_MPEG4_BASIC_ANIMATED_TEXTURE     7
#define FF_PROFILE_MPEG4_HYBRID                     8
#define FF_PROFILE_MPEG4_ADVANCED_REAL_TIME         9
#define FF_PROFILE_MPEG4_CORE_SCALABLE             10
#define FF_PROFILE_MPEG4_ADVANCED_CODING           11
#define FF_PROFILE_MPEG4_ADVANCED_CORE             12
#define FF_PROFILE_MPEG4_ADVANCED_SCALABLE_TEXTURE 13
#define FF_PROFILE_MPEG4_SIMPLE_STUDIO             14
#define FF_PROFILE_MPEG4_ADVANCED_SIMPLE           15

// shapes
#define RECT_SHAPE       0
#define BIN_SHAPE        1
#define BIN_ONLY_SHAPE   2
#define GRAY_SHAPE       3

#define SIMPLE_VO_TYPE           1
#define CORE_VO_TYPE             3
#define MAIN_VO_TYPE             4
#define NBIT_VO_TYPE             5
#define ARTS_VO_TYPE            10
#define ACE_VO_TYPE             12
#define SIMPLE_STUDIO_VO_TYPE   14
#define CORE_STUDIO_VO_TYPE     15
#define ADV_SIMPLE_VO_TYPE      17

#define VOT_VIDEO_ID 1
#define VOT_STILL_TEXTURE_ID 2

#define FF_PROFILE_UNKNOWN -99
#define FF_PROFILE_RESERVED -100

// aspect_ratio_info
#define EXTENDED_PAR 15

//vol_sprite_usage / sprite_enable
#define STATIC_SPRITE 1
#define GMC_SPRITE 2

#define MOTION_MARKER 0x1F001
#define DC_MARKER     0x6B001

#define VOS_STARTCODE        0x1B0
#define USER_DATA_STARTCODE  0x1B2
#define GOP_STARTCODE        0x1B3
#define VISUAL_OBJ_STARTCODE 0x1B5
#define VOP_STARTCODE        0x1B6
#define SLICE_STARTCODE      0x1B7
#define EXT_STARTCODE        0x1B8

#define QUANT_MATRIX_EXT_ID  0x3

/* smaller packets likely don't contain a real frame */
#define MAX_NVOP_SIZE 19

#define IS_3IV1 0

#define CHROMA_420 1
#define CHROMA_422 2
#define CHROMA_444 3

#define FF_ASPECT_EXTENDED 15

#define AV_NOPTS_VALUE          (LONG_MIN)

/**
 * Return value for header parsers if frame is not coded.
 * */
#define FRAME_SKIPPED 100

enum AVPictureType {
    AV_PICTURE_TYPE_NONE = 0, ///< Undefined
    AV_PICTURE_TYPE_I,     ///< Intra
    AV_PICTURE_TYPE_P,     ///< Predicted
    AV_PICTURE_TYPE_B,     ///< Bi-dir predicted
    AV_PICTURE_TYPE_S,     ///< S(GMC)-VOP MPEG-4
    AV_PICTURE_TYPE_SI,    ///< Switching Intra
    AV_PICTURE_TYPE_SP,    ///< Switching Predicted
    AV_PICTURE_TYPE_BI,    ///< BI type
};

struct VLC {
	int bits;
	short (*table)[2]; ///< code, bits
	int table_size, table_allocated;
};

/**
 * MpegEncContext.
 */
struct MpegEncContext {
	struct mpeg4_dec_param *ctx;

	/* the following parameters must be initialized before encoding */
	int width, height;///< picture size. must be a multiple of 16
	int codec_tag;             ///< internal codec_tag upper case converted from avctx codec_tag
	int picture_number;       //FIXME remove, unclear definition

	/** matrix transmitted in the bitstream */
	u16 intra_matrix[64];
	u16 chroma_intra_matrix[64];
	u16 inter_matrix[64];
	u16 chroma_inter_matrix[64];

	/* MPEG-4 specific */
	int studio_profile;
	int time_base;                  ///< time in seconds of last I,P,S Frame
	int quant_precision;
	int quarter_sample;              ///< 1->qpel, 0->half pel ME/MC
	int aspect_ratio_info; //FIXME remove
	int sprite_warping_accuracy;
	int data_partitioning;           ///< data partitioning flag from header
	int low_delay;                   ///< no reordering needed / has no B-frames
	int vo_type;
	int mpeg_quant;

	/* divx specific, used to workaround (many) bugs in divx5 */
	int divx_packed;

	/* MPEG-2-specific - I wished not to have to support this mess. */
	int progressive_sequence;

	int progressive_frame;
	int interlaced_dct;

	int h_edge_pos, v_edge_pos;///< horizontal / vertical position of the right/bottom edge (pixel replication)
	const u8 *y_dc_scale_table;     ///< qscale -> y_dc_scale table
	const u8 *c_dc_scale_table;     ///< qscale -> c_dc_scale table
	int qscale;		    ///< QP
	int chroma_qscale;	    ///< chroma QP
	int pict_type;		    ///< AV_PICTURE_TYPE_I, AV_PICTURE_TYPE_P, AV_PICTURE_TYPE_B, ...
	int f_code;		    ///< forward MV resolution
	int b_code;		    ///< backward MV resolution for B-frames (MPEG-4)
	int no_rounding;  /**< apply no rounding to motion compensation (MPEG-4, msmpeg4, ...)
	    for B-frames rounding mode is always 0 */
	int last_time_base;
	long time;		    ///< time of current frame
	long last_non_b_time;
	u16 pp_time;		    ///< time distance between the last 2 p,s,i frames
	u16 pb_time;		    ///< time distance between the last b and p,s,i frame
	u16 pp_field_time;
	u16 pb_field_time;	    ///< like above, just for interlaced
	int real_sprite_warping_points;
	int sprite_offset[2][2];	     ///< sprite offset[isChroma][isMVY]
	int sprite_delta[2][2];	     ///< sprite_delta [isY][isMVY]
	int mcsel;
	int partitioned_frame;	     ///< is current frame partitioned
	int top_field_first;
	int alternate_scan;
	int last_dc[3];                ///< last DC values for MPEG-1
	int dct_precision;
	int intra_dc_precision;
	int frame_pred_frame_dct;
	int q_scale_type;
	int context_reinit;
	int chroma_format;
};

struct mpeg4_dec_param {
	struct MpegEncContext m;

	/// number of bits to represent the fractional part of time
	int time_increment_bits;
	int shape;
	int vol_sprite_usage;
	int sprite_brightness_change;
	int num_sprite_warping_points;
	/// sprite trajectory points
	u16 sprite_traj[4][2];
	/// sprite shift [isChroma]
	int sprite_shift[2];

	// reversible vlc
	int rvlc;
	/// could this stream contain resync markers
	int resync_marker;
	/// time distance of first I -> B, used for interlaced B-frames
	int t_frame;

	int new_pred;
	int enhancement_type;
	int scalability;
	int use_intra_dc_vlc;

	/// QP above which the ac VLC should be used for intra dc
	int intra_dc_threshold;

	/* bug workarounds */
	int divx_version;
	int divx_build;
	int xvid_build;
	int lavc_build;

	/// flag for having shown the warning about invalid Divx B-frames
	int showed_packed_warning;
	/** does the stream contain the low_delay flag,
	*  used to work around buggy encoders. */
	int vol_control_parameters;
	int cplx_estimation_trash_i;
	int cplx_estimation_trash_p;
	int cplx_estimation_trash_b;

	struct VLC studio_intra_tab[12];
	struct VLC studio_luma_dc;
	struct VLC studio_chroma_dc;

	int rgb;

	struct AVRational time_base;
	int ticks_per_frame;
	struct AVRational sample_aspect_ratio;
	enum AVColorPrimaries color_primaries;
	enum AVColorTransferCharacteristic color_trc;
	enum AVColorSpace colorspace;
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
	enum AVPixelFormat pix_fmt;
	enum AVColorRange color_range;
	enum AVChromaLocation chroma_sample_location;
#endif
	int err_recognition;
	int idct_algo;
	int bits_per_raw_sample;
	int profile;
	int level;
	struct AVRational framerate;
	int flags;
};

struct mpeg4_param_sets {
	bool head_parsed;
	/* currently active parameter sets */
	struct mpeg4_dec_param dec_ps;
};

#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
int mpeg4_decode_extradata_ps(u8 *buf, int size, struct mpeg4_param_sets *ps);
#else
inline int mpeg4_decode_extradata_ps(u8 *buf, int size, struct mpeg4_param_sets *ps) { return -1; }
#endif

#endif

