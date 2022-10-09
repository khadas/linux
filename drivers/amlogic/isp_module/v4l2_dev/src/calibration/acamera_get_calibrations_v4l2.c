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
#include "acamera_firmware_api.h"
#include "acamera_logger.h"

extern uint32_t soc_iq_get_calibrations( int32_t, void *, ACameraCalibrations *c, char* s_name);

uint32_t get_calibrations_v4l2( uint32_t ctx_id, void *sensor_arg, ACameraCalibrations *c, char* s_name )
{
    uint32_t ret = 0;

    ret = soc_iq_get_calibrations( ctx_id, sensor_arg, c, s_name );
    LOG( LOG_INFO, "Request calibration ctx_id:%d sensor_arg:%s from soc_iq device ret %d", ctx_id, sensor_arg, ret );

    return ret;
}
