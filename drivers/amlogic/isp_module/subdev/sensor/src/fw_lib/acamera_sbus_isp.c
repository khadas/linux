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

#include "acamera_sbus_api.h"
#include "acamera_logger.h"
#include "system_hw_io.h"

static uint32_t isp_io_read_sample( acamera_sbus_t *p_bus, uintptr_t addr, uint8_t sample_size )
{
    if ( ( sample_size == 2 && ( addr & 1 ) ) || ( sample_size == 4 && ( addr & 3 ) ) ) {
        LOG( LOG_CRIT, "Missaligned access to IPS" );
        return 0xFFFFFFFF;
    }
    switch ( sample_size ) {
    case 1:
        return system_hw_read_8( addr );
    case 2:
        return system_hw_read_16( addr );
    }
    return system_hw_read_32( addr );
}


static void isp_io_write_sample( acamera_sbus_t *p_bus, uintptr_t addr, uint32_t sample, uint8_t sample_size )
{
    if ( ( sample_size == 2 && ( addr & 1 ) ) || ( sample_size == 4 && ( addr & 3 ) ) ) {
        LOG( LOG_CRIT, "Missaligned access to IPS" );
        return;
    }
    switch ( sample_size ) {
    case 1:
        system_hw_write_32( addr & 0xFFFFFFFC, ( system_hw_read_32( addr & 0xFFFFFFFC ) & ~( 0xFF << 8 * ( addr & 3 ) ) ) | ( sample << ( 8 * ( addr & 3 ) ) ) );
        break;
    case 2:
        system_hw_write_32( addr & 0xFFFFFFFC, ( system_hw_read_32( addr & 0xFFFFFFFC ) & ~( 0xFFFF << 8 * ( addr & 2 ) ) ) | ( sample << ( 8 * ( addr & 2 ) ) ) );
        break;
    default:
        system_hw_write_32( addr, sample );
        break;
    }
}

void acamera_sbus_isp_init( acamera_sbus_t *p_bus )
{
    p_bus->read_sample = isp_io_read_sample;
    p_bus->write_sample = isp_io_write_sample;
}
