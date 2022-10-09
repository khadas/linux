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
#include "acamera_get_calibrations_dummy.h"
#include "acamera_get_calibration_otp.h"

static uint32_t first_call = OPT_UNFINISH;

uint32_t get_calibratin_os08a10_otp( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c )
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
            first_call |= GOT_LINEAR_IQT;
            LOG( LOG_DEBUG, "calibration switching to WDR_MODE_LINEAR %d ", (int)preset );
            ret += ( get_calibrations_dynamic_linear_os08a10_panel( c ) + get_calibrations_static_linear_os08a10_panel( c ) );
            break;
        case WDR_MODE_NATIVE:
            LOG( LOG_DEBUG, "calibration switching to WDR_MODE_NATIVE %d ", (int)preset );
            //ret += (get_calibrations_dynamic_wdr_dummy(c)+get_calibrations_static_wdr_dummy(c));
            break;
        case WDR_MODE_FS_LIN:
            first_call |= GOT_FS_LIN_IQT;
            LOG( LOG_DEBUG, "calibration switching to WDR mode on mode %d ", (int)preset );
            //ret += ( get_calibrations_dynamic_fs_lin_os08a10_ipc( c ) + get_calibrations_static_fs_lin_os08a10_ipc( c ) );
            break;
        default:
            first_call |= GOT_LINEAR_IQT;
            LOG( LOG_DEBUG, "calibration defaults to WDR_MODE_LINEAR %d ", (int)preset );
            ret += ( get_calibrations_dynamic_linear_os08a10_panel( c ) + get_calibrations_static_linear_os08a10_panel( c ) );
            break;
    }

    if ( otp_enable == 1 ) {
        if ( ((first_call & GOT_LINEAR_OTP) == GOT_LINEAR_OTP ) ||
              ((first_call & GOT_FS_LIN_OTP) == GOT_FS_LIN_OTP)) {
            // read calibrations from the sensor otp memory and updated
            // the current IQ set with these values
            ret = acamera_calibration_os08a10_otp( c );
            if (ret != 0)
                first_call = OPT_UNFINISH;
            else
            {
                if (first_call & GOT_LINEAR_OTP)
                    first_call &= ~GOT_LINEAR_OTP;
                else
                    first_call &= ~GOT_FS_LIN_OTP;
            }
        }
    }

    return ret;
}

uint32_t get_calibratin_imx227_otp( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c )
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
            first_call |= GOT_LINEAR_IQT;
            LOG( LOG_DEBUG, "calibration switching to WDR_MODE_LINEAR %d ", (int)preset );
            ret += ( get_calibrations_dynamic_linear_imx227( c ) + get_calibrations_static_linear_imx227( c ) );
            break;
        case WDR_MODE_NATIVE:
            LOG( LOG_DEBUG, "calibration switching to WDR_MODE_NATIVE %d ", (int)preset );
            //ret += (get_calibrations_dynamic_wdr_dummy(c)+get_calibrations_static_wdr_dummy(c));
            break;
        default:
            first_call |= GOT_LINEAR_IQT;
            LOG( LOG_DEBUG, "calibration defaults to WDR_MODE_LINEAR %d ", (int)preset );
            ret += ( get_calibrations_dynamic_linear_imx227( c ) + get_calibrations_static_linear_imx227( c ) );
            break;
    }

    if ( otp_enable == 1 ) {
        if ( (first_call & GOT_LINEAR_OTP) == GOT_LINEAR_OTP ) {
            // read calibrations from the sensor otp memory and updated
            // the current IQ set with these values
            ret = acamera_calibration_imx227_otp( c );
            if (ret != 0)
                first_call = OPT_UNFINISH;
            else
                first_call &= ~GOT_LINEAR_OTP;
        }
    }

    return ret;
}


uint32_t get_calibratin_ov5675_otp( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c )
{

    uint8_t ret = 0;
	LOG( LOG_CRIT, "ov5675 otp calibration \n" );

    if ( !sensor_arg ) {
        LOG( LOG_CRIT, "calibration sensor_arg is NULL" );
        return ret;
    }

    int32_t preset = ( (sensor_mode_t *)sensor_arg )->wdr_mode;

    //logic which calibration to apply
    switch ( preset ) {
        case WDR_MODE_LINEAR:
            first_call |= GOT_LINEAR_IQT;
            LOG( LOG_DEBUG, "calibration switching to WDR_MODE_LINEAR %d ", (int)preset );
            ret += ( get_calibrations_dynamic_linear_ov5675( c ) + get_calibrations_static_linear_ov5675( c ) );
            break;
        case WDR_MODE_NATIVE:
            LOG( LOG_DEBUG, "calibration switching to WDR_MODE_NATIVE %d ", (int)preset );
            //ret += (get_calibrations_dynamic_wdr_dummy(c)+get_calibrations_static_wdr_dummy(c));
            break;
        default:
            first_call |= GOT_LINEAR_IQT;
            LOG( LOG_DEBUG, "calibration defaults to WDR_MODE_LINEAR %d ", (int)preset );
            ret += ( get_calibrations_dynamic_linear_ov5675( c ) + get_calibrations_static_linear_ov5675( c ) );
            break;
    }

    if ( otp_enable == 1 ) {
        if ( (first_call & GOT_LINEAR_OTP) == GOT_LINEAR_OTP ) {
            // read calibrations from the sensor otp memory and updated
            // the current IQ set with these values
            ret = acamera_calibration_ov5675_otp( c );
            if (ret != 0)
                first_call = OPT_UNFINISH;
            else
                first_call &= ~GOT_LINEAR_OTP;
        }
    }

    return ret;
}


