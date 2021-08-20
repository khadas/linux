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
#ifndef UTILS_COMMON_H
#define UTILS_COMMON_H

#ifdef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
#include "pixfmt.h"
#endif

#define AV_INPUT_BUFFER_PADDING_SIZE	64
#define MIN_CACHE_BITS			64

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMAX3(a,b,c) FFMAX(FFMAX(a,b),c)
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFMIN3(a,b,c) FFMIN(FFMIN(a,b),c)

#define AV_WL32(p, val)				\
	do {					\
		u32 d = (val);			\
		((u8*)(p))[0] = (d);		\
		((u8*)(p))[1] = (d) >> 8;	\
		((u8*)(p))[2] = (d) >> 16;	\
		((u8*)(p))[3] = (d) >> 24;	\
	} while(0)

#define AV_WB32(p, val)				\
	do { u32 d = (val);			\
		((u8*)(p))[3] = (d);		\
		((u8*)(p))[2] = (d) >> 8;	\
		((u8*)(p))[1] = (d) >> 16;	\
		((u8*)(p))[0] = (d) >> 24;	\
	} while(0)

#define AV_RB32(x)				\
	(((u32)((const u8*)(x))[0] << 24) |	\
	(((const u8*)(x))[1] << 16) |		\
	(((const u8*)(x))[2] <<  8) |		\
	((const u8*)(x))[3])

#define AV_RL32(x)				\
	(((u32)((const u8*)(x))[3] << 24) |	\
	(((const u8*)(x))[2] << 16) |		\
	(((const u8*)(x))[1] <<  8) |		\
	((const u8*)(x))[0])

#define NEG_SSR32(a, s)   (((int)(a)) >> ((s < 32) ? (32 - (s)) : 0))
#define NEG_USR32(a, s)   (((u32)(a)) >> ((s < 32) ? (32 - (s)) : 0))

//rounded division & shift
#define RSHIFT(a,b) ((a) > 0 ? ((a) + ((1<<(b))>>1))>>(b) : ((a) + ((1<<(b))>>1)-1)>>(b))
/* assume b>0 */
#define ROUNDED_DIV(a,b) (((a)>0 ? (a) + ((b)>>1) : (a) - ((b)>>1))/(b))

struct AVRational{
	int num; ///< numerator
	int den; ///< denominator
};

#ifndef CONFIG_AMLOGIC_MEDIA_V4L_SOFTWARE_PARSER
/**
 * YUV colorspace type.
 * These values match the ones defined by ISO/IEC 23001-8_2013 ¡ì 7.3.
 */
enum AVColorSpace {
	AVCOL_SPC_RGB         = 0,  ///< order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)
	AVCOL_SPC_BT709       = 1,  ///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
	AVCOL_SPC_UNSPECIFIED = 2,
	AVCOL_SPC_RESERVED    = 3,
	AVCOL_SPC_FCC         = 4,  ///< FCC Title 47 Code of Federal Regulations 73.682 (a)(20)
	AVCOL_SPC_BT470BG     = 5,  ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
	AVCOL_SPC_SMPTE170M   = 6,  ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
	AVCOL_SPC_SMPTE240M   = 7,  ///< functionally identical to above
	AVCOL_SPC_YCGCO       = 8,  ///< Used by Dirac / VC-2 and H.264 FRext, see ITU-T SG16
	AVCOL_SPC_YCOCG       = AVCOL_SPC_YCGCO,
	AVCOL_SPC_BT2020_NCL  = 9,  ///< ITU-R BT2020 non-constant luminance system
	AVCOL_SPC_BT2020_CL   = 10, ///< ITU-R BT2020 constant luminance system
	AVCOL_SPC_SMPTE2085   = 11, ///< SMPTE 2085, Y'D'zD'x
	AVCOL_SPC_CHROMA_DERIVED_NCL = 12, ///< Chromaticity-derived non-constant luminance system
	AVCOL_SPC_CHROMA_DERIVED_CL = 13, ///< Chromaticity-derived constant luminance system
	AVCOL_SPC_ICTCP       = 14, ///< ITU-R BT.2100-0, ICtCp
	AVCOL_SPC_NB                ///< Not part of ABI
};

/**
  * Chromaticity coordinates of the source primaries.
  * These values match the ones defined by ISO/IEC 23001-8_2013 ¡ì 7.1.
  */
enum AVColorPrimaries {
	AVCOL_PRI_RESERVED0   = 0,
	AVCOL_PRI_BT709       = 1,  ///< also ITU-R BT1361 / IEC 61966-2-4 / SMPTE RP177 Annex B
	AVCOL_PRI_UNSPECIFIED = 2,
	AVCOL_PRI_RESERVED    = 3,
	AVCOL_PRI_BT470M      = 4,  ///< also FCC Title 47 Code of Federal Regulations 73.682 (a)(20)

	AVCOL_PRI_BT470BG     = 5,  ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM
	AVCOL_PRI_SMPTE170M   = 6,  ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
	AVCOL_PRI_SMPTE240M   = 7,  ///< functionally identical to above
	AVCOL_PRI_FILM        = 8,  ///< colour filters using Illuminant C
	AVCOL_PRI_BT2020      = 9,  ///< ITU-R BT2020
	AVCOL_PRI_SMPTE428    = 10, ///< SMPTE ST 428-1 (CIE 1931 XYZ)
	AVCOL_PRI_SMPTEST428_1 = AVCOL_PRI_SMPTE428,
	AVCOL_PRI_SMPTE431    = 11, ///< SMPTE ST 431-2 (2011) / DCI P3
	AVCOL_PRI_SMPTE432    = 12, ///< SMPTE ST 432-1 (2010) / P3 D65 / Display P3
	AVCOL_PRI_JEDEC_P22   = 22, ///< JEDEC P22 phosphors
	AVCOL_PRI_NB                ///< Not part of ABI
};

/**
 * Color Transfer Characteristic.
 * These values match the ones defined by ISO/IEC 23001-8_2013 ¡ì 7.2.
 */
enum AVColorTransferCharacteristic {
	AVCOL_TRC_RESERVED0    = 0,
	AVCOL_TRC_BT709        = 1,  ///< also ITU-R BT1361
	AVCOL_TRC_UNSPECIFIED  = 2,
	AVCOL_TRC_RESERVED     = 3,
	AVCOL_TRC_GAMMA22      = 4,  ///< also ITU-R BT470M / ITU-R BT1700 625 PAL & SECAM
	AVCOL_TRC_GAMMA28      = 5,  ///< also ITU-R BT470BG
	AVCOL_TRC_SMPTE170M    = 6,  ///< also ITU-R BT601-6 525 or 625 / ITU-R BT1358 525 or 625 / ITU-R BT1700 NTSC
	AVCOL_TRC_SMPTE240M    = 7,
	AVCOL_TRC_LINEAR       = 8,  ///< "Linear transfer characteristics"
	AVCOL_TRC_LOG          = 9,  ///< "Logarithmic transfer characteristic (100:1 range)"
	AVCOL_TRC_LOG_SQRT     = 10, ///< "Logarithmic transfer characteristic (100 * Sqrt(10) : 1 range)"
	AVCOL_TRC_IEC61966_2_4 = 11, ///< IEC 61966-2-4
	AVCOL_TRC_BT1361_ECG   = 12, ///< ITU-R BT1361 Extended Colour Gamut
	AVCOL_TRC_IEC61966_2_1 = 13, ///< IEC 61966-2-1 (sRGB or sYCC)
	AVCOL_TRC_BT2020_10    = 14, ///< ITU-R BT2020 for 10-bit system
	AVCOL_TRC_BT2020_12    = 15, ///< ITU-R BT2020 for 12-bit system
	AVCOL_TRC_SMPTE2084    = 16, ///< SMPTE ST 2084 for 10-, 12-, 14- and 16-bit systems
	AVCOL_TRC_SMPTEST2084  = AVCOL_TRC_SMPTE2084,
	AVCOL_TRC_SMPTE428     = 17, ///< SMPTE ST 428-1
	AVCOL_TRC_SMPTEST428_1 = AVCOL_TRC_SMPTE428,
	AVCOL_TRC_ARIB_STD_B67 = 18, ///< ARIB STD-B67, known as "Hybrid log-gamma"
	AVCOL_TRC_NB                 ///< Not part of ABI
};
#endif

//fmt
const char *av_color_space_name(enum AVColorSpace space);
const char *av_color_primaries_name(enum AVColorPrimaries primaries);
const char *av_color_transfer_name(enum AVColorTransferCharacteristic transfer);

//math
int av_log2(u32 v);

//bitstream
int find_start_code(u8 *data, int data_sz);
int calc_nal_len(u8 *data, int len);
u8 *nal_unit_extract_rbsp(const u8 *src, u32 src_len, u32 *dst_len);

//debug
void print_hex_debug(u8 *data, u32 len, int max);

bool is_over_size(int w, int h, int size);

u8 *aml_yuv_dump(struct file *fp, u8 *start_addr, u32 real_width, u32 real_height, u32 align);
#endif