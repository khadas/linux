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
#ifndef AML_MJPEG_PARSER_H
#define AML_MJPEG_PARSER_H

#include "../aml_vcodec_drv.h"
#include "../utils/common.h"
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
#include "../utils/get_bits.h"
#endif

#define FF_PROFILE_MJPEG_HUFFMAN_BASELINE_DCT            0xc0
#define FF_PROFILE_MJPEG_HUFFMAN_EXTENDED_SEQUENTIAL_DCT 0xc1
#define FF_PROFILE_MJPEG_HUFFMAN_PROGRESSIVE_DCT         0xc2
#define FF_PROFILE_MJPEG_HUFFMAN_LOSSLESS                0xc3
#define FF_PROFILE_MJPEG_JPEG_LS                         0xf7

#define FF_CODEC_PROPERTY_LOSSLESS        0x00000001
#define FF_CODEC_PROPERTY_CLOSED_CAPTIONS 0x00000002

#define MAX_COMPONENTS 4

/* JPEG marker codes */
enum JpegMarker {
    /* start of frame */
    SOF0  = 0xc0,       /* baseline */
    SOF1  = 0xc1,       /* extended sequential, huffman */
    SOF2  = 0xc2,       /* progressive, huffman */
    SOF3  = 0xc3,       /* lossless, huffman */

    SOF5  = 0xc5,       /* differential sequential, huffman */
    SOF6  = 0xc6,       /* differential progressive, huffman */
    SOF7  = 0xc7,       /* differential lossless, huffman */
    JPG   = 0xc8,       /* reserved for JPEG extension */
    SOF9  = 0xc9,       /* extended sequential, arithmetic */
    SOF10 = 0xca,       /* progressive, arithmetic */
    SOF11 = 0xcb,       /* lossless, arithmetic */

    SOF13 = 0xcd,       /* differential sequential, arithmetic */
    SOF14 = 0xce,       /* differential progressive, arithmetic */
    SOF15 = 0xcf,       /* differential lossless, arithmetic */

    DHT   = 0xc4,       /* define huffman tables */

    DAC   = 0xcc,       /* define arithmetic-coding conditioning */

    /* restart with modulo 8 count "m" */
    RST0  = 0xd0,
    RST1  = 0xd1,
    RST2  = 0xd2,
    RST3  = 0xd3,
    RST4  = 0xd4,
    RST5  = 0xd5,
    RST6  = 0xd6,
    RST7  = 0xd7,

    SOI   = 0xd8,       /* start of image */
    EOI   = 0xd9,       /* end of image */
    SOS   = 0xda,       /* start of scan */
    DQT   = 0xdb,       /* define quantization tables */
    DNL   = 0xdc,       /* define number of lines */
    DRI   = 0xdd,       /* define restart interval */
    DHP   = 0xde,       /* define hierarchical progression */
    EXP   = 0xdf,       /* expand reference components */

    APP0  = 0xe0,
    APP1  = 0xe1,
    APP2  = 0xe2,
    APP3  = 0xe3,
    APP4  = 0xe4,
    APP5  = 0xe5,
    APP6  = 0xe6,
    APP7  = 0xe7,
    APP8  = 0xe8,
    APP9  = 0xe9,
    APP10 = 0xea,
    APP11 = 0xeb,
    APP12 = 0xec,
    APP13 = 0xed,
    APP14 = 0xee,
    APP15 = 0xef,

    JPG0  = 0xf0,
    JPG1  = 0xf1,
    JPG2  = 0xf2,
    JPG3  = 0xf3,
    JPG4  = 0xf4,
    JPG5  = 0xf5,
    JPG6  = 0xf6,
    SOF48 = 0xf7,       ///< JPEG-LS
    LSE   = 0xf8,       ///< JPEG-LS extension parameters
    JPG9  = 0xf9,
    JPG10 = 0xfa,
    JPG11 = 0xfb,
    JPG12 = 0xfc,
    JPG13 = 0xfd,

    COM   = 0xfe,       /* comment */

    TEM   = 0x01,       /* temporary private use for arithmetic coding */

    /* 0x02 -> 0xbf reserved */
};

struct VLC {
	int bits;
	short (*table)[2]; ///< code, bits
	int table_size, table_allocated;
};

struct MJpegDecodeContext {
#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
	struct get_bits_context gb;
#endif
	int buf_size;

	int start_code; /* current start code */
	int buffer_size;
	u8 *buffer;

	u16 quant_matrixes[4][64];
	struct VLC vlcs[3][4];
	int qscale[4];      ///< quantizer scale calculated from quant_matrixes

	int first_picture;    /* true if decoding first picture */
	int interlaced;     /* true if interlaced */
	int bottom_field;   /* true if bottom field */
	int lossless;
	int ls;
	int progressive;
	u8 upscale_h[4];
	u8 upscale_v[4];
	int bits;           /* bits per component */
	int adobe_transform;

	int width, height;
	int mb_width, mb_height;
	int nb_components;
	int block_stride[MAX_COMPONENTS];
	int component_id[MAX_COMPONENTS];
	int h_count[MAX_COMPONENTS]; /* horizontal and vertical count for each component */
	int v_count[MAX_COMPONENTS];
	int h_scount[MAX_COMPONENTS];
	int v_scount[MAX_COMPONENTS];
	int h_max, v_max; /* maximum h and v counts */
	int quant_index[4];   /* quant table index for each component */
	int got_picture;                                ///< we found a SOF and picture is valid, too.
	int restart_interval;
	int restart_count;
	int cur_scan; /* current scan, used by JPEG-LS */

	// Raw stream data for hwaccel use.
	const u8 *raw_image_buffer;
	int raw_image_buffer_size;

	int profile;
	u32 properties;
};

struct mjpeg_param_sets {
	bool head_parsed;
	/* currently active parameter sets */
	struct MJpegDecodeContext dec_ps;
};

#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
int mjpeg_decode_extradata_ps(u8 *buf, int size, struct mjpeg_param_sets *ps);
#else
inline int mjpeg_decode_extradata_ps(u8 *buf, int size, struct mjpeg_param_sets *ps) { return -1; }
#endif

#endif
