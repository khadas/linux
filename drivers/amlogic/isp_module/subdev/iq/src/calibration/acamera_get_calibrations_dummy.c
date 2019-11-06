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

#include "acamera_command_api.h"
#include "acamera_sensor_api.h"
#include "acamera_firmware_settings.h"
#include "acamera_logger.h"
#include <linux/module.h>

extern uint32_t get_calibrations_static_linear_imx227( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx227( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_dummy( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_dummy( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx290( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx290( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx290( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx290( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx290_lens_8mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx290_lens_8mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx290_lens_8mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx290_lens_8mm( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx290_lens_4mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx290_lens_4mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx290_lens_4mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx290_lens_4mm( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_os08a10( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_os08a10( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_os08a10( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_os08a10( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx481( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx481( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_linear_imx307( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx307( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx307( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx307( ACameraCalibrations *c );

uint32_t get_calibrations_imx227( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c )
{

    uint8_t ret = 0;

    if ( !sensor_arg ) {
        LOG( LOG_CRIT, "calibration sensor_arg is NULL" );
        return ret;
    }

    int32_t preset = ( (sensor_mode_t *)sensor_arg )->wdr_mode;

    //logic which calibration to apply
    switch ( preset ) {
    case WDR_MODE_LINEAR:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx227( c ) + get_calibrations_static_linear_imx227( c ) );
        break;
    case WDR_MODE_NATIVE:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_NATIVE %d ", (int)preset );
        //ret += (get_calibrations_dynamic_wdr_dummy(c)+get_calibrations_static_wdr_dummy(c));
        break;
    case WDR_MODE_FS_LIN:
        LOG( LOG_DEBUG, "calibration switching to WDR mode on mode %d ", (int)preset );
        ret += ( get_calibrations_dynamic_fs_lin_dummy( c ) + get_calibrations_static_fs_lin_dummy( c ) );
        break;
    default:
        LOG( LOG_DEBUG, "calibration defaults to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx227( c ) + get_calibrations_static_linear_imx227( c ) );
        break;
    }

    return ret;
}

uint32_t get_calibrations_imx290( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c )
{

    uint8_t ret = 0;
    if ( !sensor_arg ) {
        LOG( LOG_CRIT, "calibration sensor_arg is NULL" );
        return ret;
    }

    int32_t preset = ( (sensor_mode_t *)sensor_arg )->wdr_mode;

    //logic which calibration to apply
    switch ( preset ) {
    case WDR_MODE_LINEAR:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx290( c ) + get_calibrations_static_linear_imx290( c ) );
        break;
    case WDR_MODE_NATIVE:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_NATIVE %d ", (int)preset );
        //ret += (get_calibrations_dynamic_wdr_dummy(c)+get_calibrations_static_wdr_dummy(c));
        break;
    case WDR_MODE_FS_LIN:
        LOG( LOG_DEBUG, "calibration switching to WDR mode on mode %d ", (int)preset );
        ret += ( get_calibrations_dynamic_fs_lin_imx290( c ) + get_calibrations_static_fs_lin_imx290( c ) );
        break;
    default:
        LOG( LOG_DEBUG, "calibration defaults to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx290( c ) + get_calibrations_static_linear_imx290( c ) );
        break;
    }

    return ret;
}

uint32_t get_calibrations_imx290_lens_8mm( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c )
{

    uint8_t ret = 0;
    if ( !sensor_arg ) {
        LOG( LOG_CRIT, "calibration sensor_arg is NULL" );
        return ret;
    }

    int32_t preset = ( (sensor_mode_t *)sensor_arg )->wdr_mode;

    //logic which calibration to apply
    switch ( preset ) {
    case WDR_MODE_LINEAR:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx290_lens_8mm( c ) + get_calibrations_static_linear_imx290_lens_8mm( c ) );
        break;
    case WDR_MODE_NATIVE:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_NATIVE %d ", (int)preset );
        //ret += (get_calibrations_dynamic_wdr_dummy(c)+get_calibrations_static_wdr_dummy(c));
        break;
    case WDR_MODE_FS_LIN:
        LOG( LOG_DEBUG, "calibration switching to WDR mode on mode %d ", (int)preset );
        ret += ( get_calibrations_dynamic_fs_lin_imx290_lens_8mm( c ) + get_calibrations_static_fs_lin_imx290_lens_8mm( c ) );
        break;
    default:
        LOG( LOG_DEBUG, "calibration defaults to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx290_lens_8mm( c ) + get_calibrations_static_linear_imx290_lens_8mm( c ) );
        break;
    }

    return ret;
}

uint32_t get_calibrations_imx290_lens_4mm( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c )
{

    uint8_t ret = 0;
    if ( !sensor_arg ) {
        LOG( LOG_CRIT, "calibration sensor_arg is NULL" );
        return ret;
    }

    int32_t preset = ( (sensor_mode_t *)sensor_arg )->wdr_mode;

    //logic which calibration to apply
    switch ( preset ) {
    case WDR_MODE_LINEAR:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx290_lens_4mm( c ) + get_calibrations_static_linear_imx290_lens_4mm( c ) );
        break;
    case WDR_MODE_NATIVE:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_NATIVE %d ", (int)preset );
        //ret += (get_calibrations_dynamic_wdr_dummy(c)+get_calibrations_static_wdr_dummy(c));
        break;
    case WDR_MODE_FS_LIN:
        LOG( LOG_DEBUG, "calibration switching to WDR mode on mode %d ", (int)preset );
        ret += ( get_calibrations_dynamic_fs_lin_imx290_lens_4mm( c ) + get_calibrations_static_fs_lin_imx290_lens_4mm( c ) );
        break;
    default:
        LOG( LOG_DEBUG, "calibration defaults to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx290_lens_4mm( c ) + get_calibrations_static_linear_imx290_lens_4mm( c ) );
        break;
    }

    return ret;
}

uint32_t get_calibrations_os08a10( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c )
{

    uint8_t ret = 0;
    if ( !sensor_arg ) {
        LOG( LOG_CRIT, "calibration sensor_arg is NULL" );
        return ret;
    }

    int32_t preset = ( (sensor_mode_t *)sensor_arg )->wdr_mode;

    //logic which calibration to apply
    switch ( preset ) {
    case WDR_MODE_LINEAR:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_os08a10( c ) + get_calibrations_static_linear_os08a10( c ) );
        break;
    case WDR_MODE_NATIVE:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_NATIVE %d ", (int)preset );
        //ret += (get_calibrations_dynamic_wdr_dummy(c)+get_calibrations_static_wdr_dummy(c));
        break;
    case WDR_MODE_FS_LIN:
        LOG( LOG_DEBUG, "calibration switching to WDR mode on mode %d ", (int)preset );
        ret += ( get_calibrations_dynamic_fs_lin_os08a10( c ) + get_calibrations_static_fs_lin_os08a10( c ) );
        break;
    default:
        LOG( LOG_DEBUG, "calibration defaults to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_os08a10( c ) + get_calibrations_static_linear_os08a10( c ) );
        break;
    }

    return ret;
}

uint32_t get_calibrations_imx481( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c )
{

    uint8_t ret = 0;

    if ( !sensor_arg ) {
        LOG( LOG_CRIT, "calibration sensor_arg is NULL" );
        return ret;
    }

    int32_t preset = ( (sensor_mode_t *)sensor_arg )->wdr_mode;

    //logic which calibration to apply
    switch ( preset ) {
    case WDR_MODE_LINEAR:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx481( c ) + get_calibrations_static_linear_imx481( c ) );
        break;
    case WDR_MODE_NATIVE:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_NATIVE %d ", (int)preset );
        //ret += (get_calibrations_dynamic_wdr_dummy(c)+get_calibrations_static_wdr_dummy(c));
        break;
    case WDR_MODE_FS_LIN:
        LOG( LOG_DEBUG, "calibration switching to WDR mode on mode %d ", (int)preset );
        ret += ( get_calibrations_dynamic_fs_lin_dummy( c ) + get_calibrations_static_fs_lin_dummy( c ) );
        break;
    default:
        LOG( LOG_DEBUG, "calibration defaults to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx481( c ) + get_calibrations_static_linear_imx481( c ) );
        break;
    }

    return ret;
}

uint32_t get_calibrations_imx307( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c )
{

    uint8_t ret = 0;
    if ( !sensor_arg ) {
        LOG( LOG_CRIT, "calibration sensor_arg is NULL" );
        return ret;
    }

    int32_t preset = ( (sensor_mode_t *)sensor_arg )->wdr_mode;

    //logic which calibration to apply
    switch ( preset ) {
    case WDR_MODE_LINEAR:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx307( c ) + get_calibrations_static_linear_imx307( c ) );
        break;
    case WDR_MODE_NATIVE:
        LOG( LOG_DEBUG, "calibration switching to WDR_MODE_NATIVE %d ", (int)preset );
        //ret += (get_calibrations_dynamic_wdr_dummy(c)+get_calibrations_static_wdr_dummy(c));
        break;
    case WDR_MODE_FS_LIN:
        LOG( LOG_DEBUG, "calibration switching to WDR mode on mode %d ", (int)preset );
        ret += ( get_calibrations_dynamic_fs_lin_imx307( c ) + get_calibrations_static_fs_lin_imx307( c ) );
        break;
    default:
        LOG( LOG_DEBUG, "calibration defaults to WDR_MODE_LINEAR %d ", (int)preset );
        ret += ( get_calibrations_dynamic_linear_imx307( c ) + get_calibrations_static_linear_imx307( c ) );
        break;
    }

    return ret;
}
