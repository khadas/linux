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

#include "acamera_types.h"
#include "acamera_firmware_config.h"
#include "acamera_firmware_api.h"
#include "acamera_sensor_api.h"
#include "acamera_lens_api.h"

#if 0
static tframe_t sensor0_v4l2_fr_frames[ ] = {
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 15360, 0x1fa4000 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0x62400000, 7680, 0x7e9000 }},
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 15360, 0x1fa4000 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0x62400000, 7680, 0x7e9000 }},
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 15360, 0x1fa4000 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0x62400000, 7680, 0x7e9000 }},
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 15360, 0x1fa4000 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0x62400000, 7680, 0x7e9000 }},
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 15360, 0x1fa4000 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0x62400000, 7680, 0x7e9000 }},
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 15360, 0x1fa4000 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0x62400000, 7680, 0x7e9000 }}
} ;

static tframe_t sensor0_v4l2_ds1_frames[ ] = {
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 7680, 0x5eec00 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0xcdfe600, 7680, 0x1fa400 }},
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 7680, 0x5eec00 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0xfbc5200, 7680, 0x1fa400 }},
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 7680, 0x5eec00 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0x1298be00, 7680, 0x1fa400 }},
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 7680, 0x5eec00 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0xcdfe600, 7680, 0x1fa400 }},
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 7680, 0x5eec00 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0xfbc5200, 7680, 0x1fa400 }},
 {{ FW_OUTPUT_FORMAT, 0, 0, 0x62400000, 7680, 0x5eec00 } , { FW_OUTPUT_FORMAT_SECONDARY, 0, 0, 0x1298be00, 7680, 0x1fa400 }}
} ;
#endif
static aframe_t sensor0_v4l2_temper_frames[ ] = {
 { FW_OUTPUT_FORMAT, 0, 0, 0x72400000, 15360, 0x1fa4000 },
 { FW_OUTPUT_FORMAT, 0, 0, 0x72400000, 15360, 0x1fa4000 }
} ;


extern void sensor_init_v4l2( void** ctx, sensor_control_t*) ;
extern void sensor_deinit_v4l2( void *ctx ) ;
extern uint32_t get_calibrations_v4l2( uint32_t ctx_num,void * sensor_arg,ACameraCalibrations *) ;

extern int32_t lens_init( void** ctx, lens_control_t* ctrl ) ;
extern void lens_deinit( void * ctx) ;

extern void custom_initialization( uint32_t ctx_num ) ;

extern void callback_meta( uint32_t ctx_num,  const void *fw_metadata) ;
extern void callback_fr( uint32_t ctx_num,  tframe_t * tframe, const metadata_t *metadata) ;
extern void callback_ds1( uint32_t ctx_num,  tframe_t * tframe, const metadata_t *metadata) ;
extern void callback_ds2( uint32_t ctx_num,  tframe_t * tframe, const metadata_t *metadata) ;

static acamera_settings settings[ FIRMWARE_CONTEXT_NUMBER ] = {    {
        .sensor_init = sensor_init_v4l2,
        .sensor_deinit = sensor_deinit_v4l2,
        .get_calibrations = get_calibrations_v4l2,
        .lens_init = lens_init,
        .lens_deinit = lens_deinit,
        .custom_initialization = custom_initialization,
        .isp_base = 0x0,
        .temper_frames = sensor0_v4l2_temper_frames,
        .temper_frames_number = sizeof( sensor0_v4l2_temper_frames ) / sizeof( aframe_t ),
        .callback_meta = callback_meta,
        .fr_frames = NULL,//sensor0_v4l2_fr_frames,
        .fr_frames_number = 0,//sizeof( sensor0_v4l2_fr_frames ) / sizeof( tframe_t ),
        .callback_fr = callback_fr,
        .ds1_frames = NULL,//sensor0_v4l2_ds1_frames,
        .ds1_frames_number = 0,//sizeof( sensor0_v4l2_ds1_frames ) / sizeof( tframe_t ),
        .callback_ds1 = callback_ds1,
        .ds2_frames = NULL,
        .ds2_frames_number = 0,
        .callback_ds2 = callback_ds2,
    }
} ;
