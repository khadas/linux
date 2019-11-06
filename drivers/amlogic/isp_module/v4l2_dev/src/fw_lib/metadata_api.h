/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef __METADATA_API_H__
#define __METADATA_API_H__


enum {
    CCM_R = 0,
    CCM_G,
    CCM_B,
    CCM_MAX
};

typedef struct _firmware_metadata_t {
    uint32_t frame_id;

    int32_t sensor_width;
    int32_t sensor_height;
    int32_t image_format;
    int32_t sensor_bits;
    int32_t rggb_start;
    uint32_t isp_mode;
    uint32_t fps;

    int32_t int_time;
    int64_t int_time_ms;
    int32_t int_time_medium;
    int32_t int_time_long;
    int32_t again;
    int32_t dgain;
    int32_t isp_dgain;
    int32_t exposure;
    int32_t exposure_equiv;
    int32_t gain_log2;

    int32_t lens_pos;

    int32_t anti_flicker;
    int32_t gain_00;
    int32_t gain_01;
    int32_t gain_10;
    int32_t gain_11;

    int32_t black_level_00;
    int32_t black_level_01;
    int32_t black_level_10;
    int32_t black_level_11;

    int32_t lsc_table;
    int32_t lsc_blend;
    int32_t lsc_mesh_strength;

    int64_t awb_rgain;
    int64_t awb_bgain;
    int64_t awb_cct;

    int32_t sinter_strength;
    int32_t sinter_strength1;
    int32_t sinter_strength4;
    int32_t sinter_thresh_1h;
    int32_t sinter_thresh_4h;
    int32_t sinter_sad;

    int32_t temper_strength;

    int32_t iridix_strength;

    int32_t dp_threash1;
    int32_t dp_slope1;

    int32_t dp_threash2;
    int32_t dp_slope2;

    int32_t sharpening_directional;
    int32_t sharpening_unidirectional;

    int32_t demosaic_np_offset;

    int32_t fr_sharpern_strength;
    int32_t ds1_sharpen_strength;
    int32_t ds2_sharpen_strength;

    int32_t ccm[3][3];
} firmware_metadata_t;

typedef void ( *metadata_callback_t )( void *ctx_param, const firmware_metadata_t *fw_metadata );

#endif // __METADATA_API_H__