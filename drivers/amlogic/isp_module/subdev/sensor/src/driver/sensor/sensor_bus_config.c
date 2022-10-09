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
#include "acamera_logger.h"
#include "acamera_types.h"


uint32_t bus_addr[] = {
0x0
} ;
static uint32_t sensor_counter = 0 ;
static uint32_t lens_counter = 0 ;


int32_t get_next_sensor_bus_address(void) {
    int32_t result = 0 ;
    if ( sensor_counter < FIRMWARE_CONTEXT_NUMBER ) {
        result = bus_addr[ sensor_counter ] ;
        sensor_counter ++ ;
    } else {
        result = -1 ;
        LOG( LOG_ERR, "Attempt to initialize more sensor instances than was configured.") ;
    }
    return result ;
}


int32_t get_next_lens_bus_address(void) {
    int32_t result = 0 ;
    if ( lens_counter < FIRMWARE_CONTEXT_NUMBER ) {
        result = bus_addr[ lens_counter ] ;
        lens_counter ++ ;
    } else {
        result = -1 ;
        LOG( LOG_ERR, "Attempt to initialize more sensor instances than was configured.") ;
    }
    return result ;
}


void reset_sensor_bus_counter(void) {
    sensor_counter = 0 ;
}


void reset_lens_bus_counter(void) {
    lens_counter = 0 ;
}

