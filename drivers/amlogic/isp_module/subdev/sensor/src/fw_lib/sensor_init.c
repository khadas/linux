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
#include "sensor_init.h"
#include "system_timer.h"
#include "acamera_logger.h"
#include "system_stdlib.h"
#include "acamera_firmware_config.h"

/*
0x01 -> set address
addr16

0x02 -> set address
addr32

0x10 -> add offset to address
ofs8

0x11 -> subtract offset from address
ofs8

0x2N -> write N+1 bytes
ofs8
b0
b1
..
bN

0x3N -> write N+1 bytes with mask
ofs8
b0
b1
..
bN
m0
m1
..
mN

0xE0 -> wait time8 milliseconds
time8

0xE1 -> wait time16 milliseconds
time16

0xE2 -> wait time32 milliseconds
time32

0xFN -> call N sequence

0x00 -> end of sequence
*/

static uint16_t sequence_read_u16( uint8_t *p )
{
    return ( ( ( uint16_t )( p[1] ) ) << 8 ) | p[0];
}
static uint32_t sequence_read_u32( uint8_t *p )
{
    return ( ( (uint32_t)sequence_read_u16( p + 2 ) ) << 16 ) | sequence_read_u16( p );
}

static void sequence_write_u16( uint8_t *p, uint16_t data )
{
    p[1] = ( uint8_t )( ( data >> 8 ) & 0xFF );
    p[0] = ( uint8_t )( ( data >> 0 ) & 0xFF );
}
static void sequence_write_u32( uint8_t *p, uint32_t data )
{
    p[3] = ( uint8_t )( ( data >> 24 ) & 0xFF );
    p[2] = ( uint8_t )( ( data >> 16 ) & 0xFF );
    p[1] = ( uint8_t )( ( data >> 8 ) & 0xFF );
    p[0] = ( uint8_t )( ( data >> 0 ) & 0xFF );
}

#define SENSOR_SEQUENCE_GET_GROUP( sequence, group ) \
    ( ( (uint8_t *)( sequence ) ) + sequence_read_u16( ( (uint8_t *)( sequence ) ) + group * 2 ) )

static void sensor_write_data( acamera_sbus_ptr_t p_sbus, uint32_t isp_offset, uint32_t addr, uint8_t *data, int len )
{
    if ( !len ) {
        return;
    }

    addr = addr + isp_offset;

    if ( addr & 1 ) {
        acamera_sbus_write_u8( p_sbus, ( addr ), data[0] );
        sensor_write_data( p_sbus, isp_offset, addr + 1, data + 1, len - 1 );
    } else {
        int i, ofs;
        int count = len >> 2;
        for ( i = 0, ofs = 0; i < count; ++i, ofs += 4 ) {
            acamera_sbus_write_u32( p_sbus, ( addr + ofs ), sequence_read_u32( data + ofs ) );
            LOG( LOG_DEBUG, "B32: 0x%x : 0x%x", ( addr + ofs ), sequence_read_u32( data + ofs ) );
        }
        addr += ofs;
        data += ofs;
        len -= ofs;
        count = len >> 1;
        for ( i = 0, ofs = 0; i < count; ++i, ofs += 2 ) {
            acamera_sbus_write_u16( p_sbus, ( addr + ofs ), sequence_read_u16( data + ofs ) );
            LOG( LOG_DEBUG, "B16: 0x%x : 0x%x", ( addr + ofs ), sequence_read_u16( data + ofs ) );
        }
        addr += ofs;
        data += ofs;
        len -= ofs;
        for ( i = 0; i < len; ++i ) {
            acamera_sbus_write_u8( p_sbus, ( addr + i ), data[i] );
            LOG( LOG_DEBUG, "B8: 0x%x : 0x%x", ( addr + i ), data[i] );
        }
    }
}

static void sensor_read_data_mask( acamera_sbus_ptr_t p_sbus, uint32_t addr, uint8_t *data, int len )
{
    if ( !len ) {
        return;
    } else {
        int i, ofs;
        int count = len >> 2;
        for ( i = 0, ofs = 0; i < count; ++i, ofs += 4 ) {
            uint32_t sdata = acamera_sbus_read_u32( p_sbus, ( addr + ofs ) );
            uint32_t wdata = sequence_read_u32( data + ofs );
            uint32_t mask = sequence_read_u32( data + len + ofs );
            sequence_write_u32( data + ofs, ( sdata & ~mask ) | ( wdata & mask ) );
        }
        count = ( len - ofs ) >> 1;
        for ( i = 0; i < count; ++i, ofs += 2 ) {
            uint16_t sdata = acamera_sbus_read_u16( p_sbus, ( addr + ofs ) );
            uint16_t wdata = sequence_read_u16( data + ofs );
            uint16_t mask = sequence_read_u16( data + len + ofs );
            sequence_write_u16( data + ofs, ( sdata & ~mask ) | ( wdata & mask ) );
        }
        for ( i = 0; i < len - ofs; ++i ) {
            uint8_t sdata = acamera_sbus_read_u8( p_sbus, ( addr + ofs + i ) );
            uint8_t wdata = *( data + ofs + i );
            uint8_t mask = *( data + len + ofs + i );
            *( data + ofs + i ) = ( sdata & ~mask ) | ( wdata & mask );
        }
    }
}

void acamera_load_binary_sequence( acamera_sbus_ptr_t p_sbus, uintptr_t isp_offset, char size, const char *sequence, int group )
{
    uint32_t addr = 0;
    uint8_t *p = (uint8_t *)SENSOR_SEQUENCE_GET_GROUP( sequence, group );
    for ( ;; ) {
        uint8_t cmd = *( p++ );
        switch ( cmd ) {
        case 0:
            return;
        case 0x01: {
            addr = sequence_read_u16( p );
            p += 2;
        } break;
        case 0x02: {
            addr = sequence_read_u32( p );
            p += 4;
        } break;
        case 0x10: {
            addr += ( *( p++ ) );
        } break;
        case 0x11: {
            addr -= ( *( p++ ) );
        } break;
        case 0xE0: {
            uint8_t time = *( p++ );
            system_timer_usleep( time );
        } break;
        case 0xE1: {
            uint16_t time = sequence_read_u16( p );
            p += 2;
            system_timer_usleep( time );
        } break;
        case 0xE2: {
            uint32_t time = sequence_read_u32( p );
            p += 4;
            system_timer_usleep( time );
        } break;
        default:
            switch ( cmd >> 4 ) {
            case 0x2: {
                int len = ( cmd & 0x0F ) + 1;
                int ofs = ( *( p++ ) );
                sensor_write_data( p_sbus, isp_offset, addr + ofs, p, len );
                p += len;
            } break;
            case 0x3: {
                int len = ( cmd & 0x0F ) + 1;
                int ofs = ( *( p++ ) );
                if ( len <= 4 ) {
                    uint8_t buf[8];
                    system_memcpy( buf, p, 2 * len );
                    sensor_read_data_mask( p_sbus, addr + ofs, buf, len );
                    sensor_write_data( p_sbus, isp_offset, addr + ofs, buf, len );
                } else {
                    LOG( LOG_CRIT, "Can't apply mask for data more then 4 bytes" );
                }
                p += 2 * len;
            } break;
            case 0xF:
                acamera_sensor_load_binary_sequence( p_sbus, size, sequence, cmd & 0x0F );
                break;
            }
        }
    }
}


void acamera_load_array_sequence( acamera_sbus_ptr_t p_sbus, uintptr_t isp_offset, char size, const acam_reg_t **sequence, int group )
{
    const acam_reg_t *seq = sequence[group];
    uint32_t end_seq = 0;
    LOG( LOG_DEBUG, "Dumping sequence %d", group );
    while ( end_seq == 0 ) {
        uint32_t cmd = seq->address;
        uint32_t val = seq->value;
        if ( seq->len ) //overide size if it is valid
            size = seq->len;
        switch ( cmd ) {
        case 0xFFFF: //wait command
            //time is given in milliseconds. convert to microseconds interval
            val *= 1000;
            system_timer_usleep( val );
            break;
        case 0x0000: //sequence end flag
            if ( seq->len == 0 && seq->value == 0 ) {
                end_seq = 1;
                break;
            }
        default:
            if ( size == 4 ) {
                if ( seq->mask ) {
                    uint32_t sdata = acamera_sbus_read_u32( p_sbus, seq->address + isp_offset );
                    uint32_t wdata = seq->value;
                    uint32_t mask = seq->mask;
                    val = ( sdata & ~mask ) | ( wdata & mask );
                }
                acamera_sbus_write_u32( p_sbus, seq->address + isp_offset, val );
                LOG( LOG_DEBUG, "A32: 0x%x : 0x%x", seq->address + isp_offset, val );
            } else if ( size == 2 ) {
                if ( seq->mask ) {
                    uint16_t sdata = acamera_sbus_read_u16( p_sbus, seq->address + isp_offset );
                    uint16_t wdata = seq->value;
                    uint16_t mask = seq->mask;
                    val = ( uint16_t )( ( sdata & ~mask ) | ( wdata & mask ) );
                }
                acamera_sbus_write_u16( p_sbus, seq->address + isp_offset, (uint16_t)val );
                LOG( LOG_DEBUG, "A16: 0x%x : 0x%x", seq->address + isp_offset, (uint16_t)val );
            } else if ( size == 1 ) {
                if ( seq->mask ) {
                    uint8_t sdata = acamera_sbus_read_u8( p_sbus, seq->address + isp_offset );
                    uint8_t wdata = seq->value;
                    uint8_t mask = seq->mask;
                    val = ( uint8_t )( ( sdata & ~mask ) | ( wdata & mask ) );
                }
                acamera_sbus_write_u8( p_sbus, seq->address + isp_offset, (uint8_t)val );
                LOG( LOG_DEBUG, "A16: 0x%x : 0x%x", seq->address + isp_offset, (uint8_t)val );
            } else {
                LOG( LOG_ERR, "Invalid size %d", size );
            }
            break;
        }
        seq++;
    }
}


void acamera_sensor_load_binary_sequence( acamera_sbus_ptr_t p_sbus, char size, const char *sequence, int group )
{
    acamera_load_binary_sequence( p_sbus, 0, size, sequence, group );
}


void acamera_sensor_load_array_sequence( acamera_sbus_ptr_t p_sbus, char size, const acam_reg_t **sequence, int group )
{
    acamera_load_array_sequence( p_sbus, 0, size, sequence, group );
}
