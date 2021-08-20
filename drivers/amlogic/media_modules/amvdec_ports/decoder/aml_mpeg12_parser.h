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
#ifndef AML_MPEG12_PARSER_H
#define AML_MPEG12_PARSER_H

#include "../aml_vcodec_drv.h"
#include "../utils/common.h"
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
#include "../utils/pixfmt.h"
#endif

/* Start codes. */
#define SEQ_END_CODE            0x000001b7
#define SEQ_START_CODE          0x000001b3
#define GOP_START_CODE          0x000001b8
#define PICTURE_START_CODE      0x00000100
#define SLICE_MIN_START_CODE    0x00000101
#define SLICE_MAX_START_CODE    0x000001af
#define EXT_START_CODE          0x000001b5
#define USER_START_CODE         0x000001b2
#define SLICE_START_CODE        0x000001b7

enum AVFieldOrder {
	AV_FIELD_UNKNOWN,
	AV_FIELD_PROGRESSIVE,
	AV_FIELD_TT,          //< Top coded_first, top displayed first
	AV_FIELD_BB,          //< Bottom coded first, bottom displayed first
	AV_FIELD_TB,          //< Top coded first, bottom displayed first
	AV_FIELD_BT,          //< Bottom coded first, top displayed first
};

struct MpvParseContext {
	struct AVRational frame_rate;
	int progressive_sequence;
	int width, height;

	int repeat_pict; /* XXX: Put it back in AVCodecContext. */
	int pict_type; /* XXX: Put it back in AVCodecContext. */
	enum AVFieldOrder field_order;
	int format;
	/**
	* Dimensions of the coded video.
	*/
	int coded_width;
	int coded_height;
	/**
	* For some codecs, the time base is closer to the field rate than the frame rate.
	* Most notably, H.264 and MPEG-2 specify time_base as half of frame duration
	* if no telecine is used ...
	*
	* Set to time_base ticks per frame. Default 1, e.g., H.264/MPEG-2 set it to 2.
	*/
	int ticks_per_frame;
	/**
	* Size of the frame reordering buffer in the decoder.
	* For MPEG-2 it is 1 IPB or 0 low delay IP.
	* - encoding: Set by libavcodec.
	* - decoding: Set by libavcodec.
	*/
	int has_b_frames;
	/**
	* - decoding: For codecs that store a framerate value in the compressed
	*             bitstream, the decoder may export it here. { 0, 1} when
	*             unknown.
	* - encoding: May be used to signal the framerate of CFR content to an
	*             encoder.
	*/
	struct AVRational framerate;
};

struct mpeg12_param_sets {
	bool head_parsed;
	/* currently active parameter sets */
	struct MpvParseContext dec_ps;
};

#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
int mpeg12_decode_extradata_ps(u8 *buf, int size, struct mpeg12_param_sets *ps);
#else
inline int mpeg12_decode_extradata_ps(u8 *buf, int size, struct mpeg12_param_sets *ps) { return -1; }
#endif

#endif
