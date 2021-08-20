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
#ifndef AML_VP9_PARSER_H
#define AML_VP9_PARSER_H

#include "../aml_vcodec_drv.h"
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
#include "../utils/pixfmt.h"
#include "../utils/get_bits.h"
#endif

#define MAX_SEGMENT	8

struct VP9BitstreamHeader {
	// bitstream header
	u8 profile;
	u8 bpp;
	u8 keyframe;
	u8 invisible;
	u8 errorres;
	u8 intraonly;
	u8 resetctx;
	u8 refreshrefmask;
	u8 highprecisionmvs;
	u8 allowcompinter;
	u8 refreshctx;
	u8 parallelmode;
	u8 framectxid;
	u8 use_last_frame_mvs;
	u8 refidx[3];
	u8 signbias[3];
	u8 fixcompref;
	u8 varcompref[2];
	struct {
		u8 level;
		char sharpness;
	} filter;
	struct {
		u8 enabled;
		u8 updated;
		char mode[2];
		char ref[4];
	} lf_delta;
	u8 yac_qi;
	char ydc_qdelta, uvdc_qdelta, uvac_qdelta;
	u8 lossless;
	struct {
		u8 enabled;
		u8 temporal;
		u8 absolute_vals;
		u8 update_map;
		u8 prob[7];
		u8 pred_prob[3];
		struct {
			u8 q_enabled;
			u8 lf_enabled;
			u8 ref_enabled;
			u8 skip_enabled;
			u8 ref_val;
			int16_t q_val;
			char lf_val;
			int16_t qmul[2][2];
			u8 lflvl[4][2];
		} feat[MAX_SEGMENT];
	} segmentation;
	struct {
		u32 log2_tile_cols, log2_tile_rows;
		u32 tile_cols, tile_rows;
	} tiling;

	int uncompressed_header_size;
	int compressed_header_size;
};

struct VP9SharedContext {
	struct VP9BitstreamHeader h;

	//struct ThreadFrame refs[8];
#define CUR_FRAME 0
#define REF_FRAME_MVPAIR 1
#define REF_FRAME_SEGMAP 2
	//struct VP9Frame frames[3];
};

struct VP9Context {
	struct VP9SharedContext s;
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
	struct get_bits_context gb;
#endif
	int pass, active_tile_cols;

	u8 ss_h, ss_v;
	u8 last_bpp, bpp_index, bytesperpixel;
	u8 last_keyframe;
	// sb_cols/rows, rows/cols and last_fmt are used for allocating all internal
	// arrays, and are thus per-thread. w/h and gf_fmt are synced between threads
	// and are therefore per-stream. pix_fmt represents the value in the header
	// of the currently processed frame.
	int width;
	int height;

	int render_width;
	int render_height;
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
	enum AVPixelFormat pix_fmt, last_fmt, gf_fmt;
#endif
	u32 sb_cols, sb_rows, rows, cols;

	struct {
		u8 lim_lut[64];
		u8 mblim_lut[64];
	} filter_lut;
	struct {
		u8 coef[4][2][2][6][6][3];
	} prob_ctx[4];
	struct {
		u8 coef[4][2][2][6][6][11];
	} prob;

	// contextual (above) cache
	u8 *above_partition_ctx;
	u8 *above_mode_ctx;
	// FIXME maybe merge some of the below in a flags field?
	u8 *above_y_nnz_ctx;
	u8 *above_uv_nnz_ctx[2];
	u8 *above_skip_ctx; // 1bit
	u8 *above_txfm_ctx; // 2bit
	u8 *above_segpred_ctx; // 1bit
	u8 *above_intra_ctx; // 1bit
	u8 *above_comp_ctx; // 1bit
	u8 *above_ref_ctx; // 2bit
	u8 *above_filter_ctx;

	// whole-frame cache
	u8 *intra_pred_data[3];

	// block reconstruction intermediates
	int block_alloc_using_2pass;
	uint16_t mvscale[3][2];
	u8 mvstep[3][2];
};

struct vp9_superframe_split {
	/*in data*/
	u8 *data;
	u32 data_size;

	/*out data*/
	int nb_frames;
	int size;
	int next_frame;
	u32 next_frame_offset;
	int prefix_size;
	int sizes[8];
};

struct vp9_param_sets {
	bool head_parsed;
	struct VP9Context ctx;
};

#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
int vp9_superframe_split_filter(struct vp9_superframe_split *s);
int vp9_decode_extradata_ps(u8 *data, int size, struct vp9_param_sets *ps);
#else
inline int vp9_decode_extradata_ps(u8 *data, int size, struct vp9_param_sets *ps) { return -1; }
#endif

#endif //AML_VP9_PARSER_H
