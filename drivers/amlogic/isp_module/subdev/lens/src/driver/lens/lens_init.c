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

#include "acamera_firmware_config.h"
#include "acamera_lens_api.h"
#include "acamera_types.h"

#include "sensor_bus_config.h"
/*    - Test the driver in this file                            */
/*                                                              */

#if ISP_SENSOR_DRIVER_DONGWOON
#include "dongwoon_vcm.h"
#endif
#if ISP_SENSOR_DRIVER_DW9800
#include "dw9800_vcm.h"
#endif
#if ISP_SENSOR_DRIVER_AD5821
#include "AD5821_vcm.h"
#endif
#if ISP_SENSOR_DRIVER_ROHM
#include "rohm_vcm.h"
#endif
#if ISP_SENSOR_DRIVER_LC898201
#include "LC898201_vcm.h"
#endif
#if ISP_SENSOR_DRIVER_FP5510A
#include "fp5510a_vcm.h"
#endif
#if ISP_SENSOR_DRIVER_BU64748
#include "BU64748_vcm.h"
#endif
#if ISP_SENSOR_DRIVER_AN41908A
#include "an41908a_vcm.h"
#endif
#if ISP_SENSOR_DRIVER_NULL
#include "null_vcm.h"
#endif
#if ISP_SENSOR_DRIVER_V4L2
#include "v4l2_vcm.h"
#endif

#include "acamera_logger.h"

int32_t lens_init( void **ctx, lens_control_t *ctrl )
{

    uint32_t lens_bus = get_next_lens_bus_address();
#if ISP_SENSOR_DRIVER_V4L2
    if ( lens_v4l2_subdev_test( lens_bus ) ) {
        lens_v4l2_subdev_init( ctx, ctrl, lens_bus );
        LOG( LOG_NOTICE, "Lens VCM driver is V4L2 subdev" );
        return 0;
    }
#endif
#if ISP_SENSOR_DRIVER_DONGWOON
    if ( lens_dongwoon_test( lens_bus ) ) {
        lens_dongwoon_init( ctx, ctrl, lens_bus );
        LOG( LOG_NOTICE, "Lens VCM driver is DONGWOON" );
        return 0;
    }
#endif
#if ISP_SENSOR_DRIVER_DW9800
    if ( lens_dw9800_test( lens_bus ) ) {
        lens_dw9800_init( ctx, ctrl, lens_bus );
        LOG( LOG_NOTICE, "Lens VCM driver is DW9800" );
        return 0;
    }
#endif
#if ISP_SENSOR_DRIVER_AD5821
    if ( lens_AD5821_test( lens_bus ) ) {
        lens_AD5821_init( ctx, ctrl, lens_bus );
        LOG( LOG_NOTICE, "Lens VCM driver is AD5821" );
        return 0;
    }
#endif
#if ISP_SENSOR_DRIVER_ROHM
    if ( lens_rohm_test( lens_bus ) ) {
        lens_rohm_init( ctx, ctrl, lens_bus );
        LOG( LOG_NOTICE, "Lens VCM driver is ROHM" );
        return 0;
    }
#endif
#if ISP_SENSOR_DRIVER_LC898201
    if ( lens_LC898201_test( lens_bus ) ) {
        lens_LC898201_init( ctx, ctrl, lens_bus );
        LOG( LOG_NOTICE, "Lens VCM driver is LC898201" );
        return 0;
    }
#endif
#if ISP_SENSOR_DRIVER_FP5510A
    if ( lens_fp5510a_test( lens_bus ) ) {
        lens_fp5510a_init( ctx, ctrl, lens_bus );
        LOG( LOG_NOTICE, "Lens VCM driver is FP5510A" );
        return 0;
    }
#endif
#if ISP_SENSOR_DRIVER_BU64748
    if ( lens_BU64748_test( lens_bus ) ) {
        lens_BU64748_init( ctx, ctrl, lens_bus );
        LOG( LOG_NOTICE, "Lens VCM driver is BU64748 (ROHM)" );
        return 0;
    }
#endif
#if ISP_SENSOR_DRIVER_AN41908A
    if ( lens_an41908a_test( lens_bus ) ) {
        lens_an41908a_init( ctx, ctrl, lens_bus );
        LOG( LOG_NOTICE, "Lens VCM driver is AN41908A" );
        return 0;
    }
#endif
#if ISP_SENSOR_DRIVER_MODEL
    if (lens_model_test(lens_bus)) {
        lens_model_init(ctx, ctrl, lens_bus);
        LOG(LOG_NOTICE, "Lens VCM driver is Model");
        return 0;
    }
#endif
#if ISP_SENSOR_DRIVER_NULL
    // Null should always be tested last
    if ( lens_null_test( lens_bus ) ) {
        lens_null_init( ctx, ctrl, lens_bus );
        LOG( LOG_NOTICE, "Lens VCM driver is NULL" );
        return 0;
    }
#endif

    LOG( LOG_ERR, "NO VALID SENSOR DRIVER FOUND bus:0x%x", lens_bus );
    return -1;
}

void lens_deinit( void *ctx )
{
#if ISP_SENSOR_DRIVER_V4L2
    lens_v4l2_subdev_deinit( ctx );
#endif
#if ISP_SENSOR_DRIVER_BU64748
    lens_BU64748_deinit( ctx );
#endif
#if ISP_SENSOR_DRIVER_DONGWOON
    lens_dongwoon_deinit( ctx );
#endif
#if ISP_SENSOR_DRIVER_DW9800
    lens_dw9800_deinit( ctx );
#endif
#if ISP_SENSOR_DRIVER_AD5821
    lens_AD5821_deinit( ctx );
#endif
#if ISP_SENSOR_DRIVER_ROHM
    lens_rohm_deinit( ctx );
#endif
#if ISP_SENSOR_DRIVER_LC898201
    lens_LC898201_deinit( ctx );
#endif
#if ISP_SENSOR_DRIVER_FP5510A
    lens_fp5510a_deinit( ctx );
#endif
#if ISP_SENSOR_DRIVER_AN41908A
    lens_an41908a_deinit( ctx );
#endif
    reset_lens_bus_counter();
}
