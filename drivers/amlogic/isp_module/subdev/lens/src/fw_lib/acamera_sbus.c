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
#if ISP_FW_BUILD
#include "acamera_fw.h"
#endif
#include "acamera_mem_access.h"
#include "acamera_sbus_i2c.h"
#include "acamera_sbus_spi.h"
#include "acamera_sbus_isp.h"
#include "acamera_sbus_isp_sw.h"

#include "acamera_logger.h"

#define SBUS_CAN_ADDRESS_8BITS( p_bus ) \
    ( ( ( p_bus )->mask & ( SBUS_MASK_ADDR_STEP_16BITS | SBUS_MASK_ADDR_STEP_32BITS ) ) == 0 )

#define SBUS_CAN_ADDRESS_16BITS( p_bus ) \
    ( ( ( p_bus )->mask & SBUS_MASK_ADDR_STEP_32BITS ) == 0 )

#define SBUS_ADDRESS_STEP( p_bus ) \
    ( ( ( p_bus )->mask & SBUS_MASK_ADDR_STEP_32BITS ) != 0 ? 4 : ( ( ( p_bus )->mask & SBUS_MASK_ADDR_STEP_16BITS ) != 0 ? 2 : 1 ) )

#define SBUS_ADDRESS_16BIT_INCREMENT( p_bus ) \
    ( ( ( p_bus )->mask & SBUS_MASK_ADDR_STEP_16BITS ) != 0 ? 1 : 2 )

#define SBUS_ADDRESS_32BIT_INCREMENT( p_bus ) \
    ( ( ( p_bus )->mask & SBUS_MASK_ADDR_STEP_32BITS ) != 0 ? 1 : ( ( ( p_bus )->mask & SBUS_MASK_ADDR_STEP_16BITS ) != 0 ? 2 : 4 ) )

#define SBUS_ADDRESS_SHIFT( p_bus ) \
    ( ( ( p_bus )->mask & SBUS_MASK_ADDR_STEP_32BITS ) != 0 ? 2 : ( ( ( p_bus )->mask & SBUS_MASK_ADDR_STEP_16BITS ) != 0 ? 1 : 0 ) )

static __inline uint16_t change_endian_u16( uint16_t data )
{
    return ( data >> 8 ) | ( data << 8 );
}

static __inline uintptr_t sbus_update_address( acamera_sbus_ptr_t p_bus, uintptr_t addr )
{
    if ( p_bus->mask & SBUS_MASK_ADDR_32BITS ) {
        if ( p_bus->mask & SBUS_MASK_ADDR_SWAP_BYTES ) {
            uint16_t a0 = change_endian_u16( (uint16_t)addr );
            uint16_t a1 = change_endian_u16( ( uint16_t )( addr >> 16 ) );
            if ( p_bus->mask & SBUS_MASK_ADDR_SWAP_WORDS ) {
                addr = ( uintptr_t )( a0 << 16 );
                addr |= a1;
            } else {
                addr = ( uintptr_t )( a1 << 16 );
                addr |= a0;
            }
        } else if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_WORDS ) {
            addr = ( addr >> 16 ) | ( addr << 16 );
        }
    } else if ( p_bus->mask & SBUS_MASK_ADDR_16BITS ) {
        if ( p_bus->mask & SBUS_MASK_ADDR_SWAP_BYTES ) {
            addr = change_endian_u16( (uint16_t)addr );
        }
    }
    return addr;
}

uint16_t acamera_mem_read_u16( void *p_data )
{
    if ( ( (size_t)p_data ) & 1 ) {
        return ( ( (uint16_t)MEM_ALIGNED_READ( uint8_t, MEM_OFFSET_PTR( p_data, 1 ) ) ) << 8 ) | MEM_ALIGNED_READ( uint8_t, p_data );
    }
    return MEM_ALIGNED_READ( uint16_t, p_data );
}

uint32_t acamera_mem_read_u32( void *p_data )
{
    switch ( ( (size_t)p_data ) & 3 ) {
    case 0:
        break;
    case 2:
        return ( ( (uint32_t)MEM_ALIGNED_READ( uint16_t, MEM_OFFSET_PTR( p_data, 2 ) ) ) << 16 ) | MEM_ALIGNED_READ( uint16_t, p_data );
    case 1:
    case 3:
        return ( ( (uint32_t)MEM_ALIGNED_READ( uint8_t, MEM_OFFSET_PTR( p_data, 3 ) ) ) << 24 ) |
               ( MEM_ALIGNED_READ( uint16_t, MEM_OFFSET_PTR( p_data, 1 ) ) << 8 ) | MEM_ALIGNED_READ( uint8_t, p_data );
    }
    return MEM_ALIGNED_READ( uint32_t, p_data );
}

void acamera_mem_write_u16( void *p_buf, uint16_t data )
{
    if ( ( (size_t)p_buf ) & 1 ) {
        MEM_ALIGNED_WRITE( uint8_t, p_buf, (uint8_t)data );
        MEM_ALIGNED_WRITE( uint8_t, MEM_OFFSET_PTR( p_buf, 1 ), ( uint8_t )( data >> 8 ) );
    } else {
        MEM_ALIGNED_WRITE( uint16_t, p_buf, data );
    }
}

void acamera_mem_write_u32( void *p_buf, uint32_t data )
{
    switch ( ( (size_t)p_buf ) & 3 ) {
    case 0:
        MEM_ALIGNED_WRITE( uint32_t, p_buf, data );
        break;
    case 2:
        MEM_ALIGNED_WRITE( uint16_t, p_buf, (uint16_t)data );
        MEM_ALIGNED_WRITE( uint16_t, MEM_OFFSET_PTR( p_buf, 2 ), ( uint16_t )( data >> 16 ) );
        break;
    case 1:
    case 3:
        MEM_ALIGNED_WRITE( uint8_t, p_buf, (uint8_t)data );
        MEM_ALIGNED_WRITE( uint16_t, MEM_OFFSET_PTR( p_buf, 1 ), ( uint16_t )( data >> 8 ) );
        MEM_ALIGNED_WRITE( uint8_t, MEM_OFFSET_PTR( p_buf, 3 ), ( uint8_t )( data >> 24 ) );
        break;
    }
}

uint8_t acamera_sbus_read_u8( acamera_sbus_t *p_bus, uintptr_t addr )
{
    if ( p_bus->mask & SBUS_MASK_SAMPLE_8BITS ) {
        return ( uint8_t )( p_bus->read_sample( p_bus, sbus_update_address( p_bus, addr ), 1 ) );
    } else {
        // error here
        return 0xff;
    }
}

uint16_t acamera_sbus_read_u16( acamera_sbus_t *p_bus, uintptr_t addr )
{
    if ( p_bus->mask & SBUS_MASK_SAMPLE_16BITS ) {
        uint16_t data = ( uint16_t )( p_bus->read_sample( p_bus, sbus_update_address( p_bus, addr ), 2 ) );
        if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_BYTES ) {
            data = change_endian_u16( data );
        }
        return data;
    } else if ( SBUS_CAN_ADDRESS_8BITS( p_bus ) ) {
        uint8_t b0, b1;
        b0 = acamera_sbus_read_u8( p_bus, addr );
        b1 = acamera_sbus_read_u8( p_bus, addr + 1 );
        if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_BYTES ) {
            // big endian
            return ( ( ( (uint16_t)b0 ) << 8 ) | b1 );
        } else {
            // little endian
            return ( ( ( (uint16_t)b1 ) << 8 ) | b0 );
        }
    } else {
        // error here
        return 0xffff;
    }
}

uint32_t acamera_sbus_read_u32( acamera_sbus_t *p_bus, uintptr_t addr )
{
    if ( p_bus->mask & SBUS_MASK_SAMPLE_32BITS ) {
        uint32_t data = p_bus->read_sample( p_bus, sbus_update_address( p_bus, addr ), 4 );
        if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_BYTES ) {
            uint16_t d0 = change_endian_u16( (uint16_t)data );
            uint16_t d1 = change_endian_u16( ( uint16_t )( data >> 16 ) );
            if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_WORDS ) {
                data = ( d0 << 16 );
                data |= d1;
            } else {
                data = ( d1 << 16 );
                data |= d0;
            }
        } else if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_WORDS ) {
            data = ( data >> 16 ) | ( data << 16 );
        }
        return data;
    } else if ( SBUS_CAN_ADDRESS_16BITS( p_bus ) ) {
        uint16_t b0, b1;
        b0 = acamera_sbus_read_u16( p_bus, addr );
        b1 = acamera_sbus_read_u16( p_bus, addr + SBUS_ADDRESS_16BIT_INCREMENT( p_bus ) );
        if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_WORDS ) {
            // big endian
            return ( ( ( (uint32_t)b0 ) << 16 ) | b1 );
        } else {
            // little endian
            return ( ( ( (uint32_t)b1 ) << 16 ) | b0 );
        }
    } else {
        // error here
        return 0xffffffff;
    }
}

void acamera_sbus_read_data_u8( acamera_sbus_t *p_bus, uintptr_t addr, uint8_t *p_data, int n_count )
{
    int i;
    for ( i = 0; i < n_count; ++i ) {
        p_data[i] = acamera_sbus_read_u8( p_bus, addr );
        addr += 1;
    }
}

void acamera_sbus_read_data_u16( acamera_sbus_t *p_bus, uintptr_t addr, uint16_t *p_data, int n_count )
{
    const int addr_step = SBUS_ADDRESS_16BIT_INCREMENT( p_bus );
    int i;
    if ( ( (size_t)p_data ) & 1 ) {
        // unaligned read
        for ( i = 0; i < n_count; ++i, ++p_data ) {
            acamera_mem_write_u16( p_data, acamera_sbus_read_u16( p_bus, addr ) );
            addr += addr_step;
        }
    } else {
        for ( i = 0; i < n_count; ++i ) {
            p_data[i] = acamera_sbus_read_u16( p_bus, addr );
            addr += addr_step;
        }
    }
}

void acamera_sbus_read_data_u32( acamera_sbus_t *p_bus, uintptr_t addr, uint32_t *p_data, int n_count )
{
    const int addr_step = SBUS_ADDRESS_32BIT_INCREMENT( p_bus );
    int i;
    if ( ( (size_t)p_data ) & 3 ) {
        // unaligned read
        for ( i = 0; i < n_count; ++i, ++p_data ) {
            acamera_mem_write_u32( p_data, acamera_sbus_read_u32( p_bus, addr ) );
            addr += addr_step;
        }
    } else {
        for ( i = 0; i < n_count; ++i ) {
            p_data[i] = acamera_sbus_read_u32( p_bus, addr );
            addr += addr_step;
        }
    }
}


///////////////////////////////////////////////////////////////////////////////

void acamera_sbus_write_u8( acamera_sbus_t *p_bus, uintptr_t addr, uint8_t sample )
{
    if ( p_bus->mask & SBUS_MASK_SAMPLE_8BITS ) {
        p_bus->write_sample( p_bus, sbus_update_address( p_bus, addr ), sample, 1 );
    } else {
        // error here
    }
}

void acamera_sbus_write_u16( acamera_sbus_t *p_bus, uintptr_t addr, uint16_t sample )
{
    if ( p_bus->mask & SBUS_MASK_SAMPLE_16BITS ) {
        if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_BYTES ) {
            sample = change_endian_u16( sample );
        }
        p_bus->write_sample( p_bus, sbus_update_address( p_bus, addr ), sample, 2 );
    } else if ( SBUS_CAN_ADDRESS_8BITS( p_bus ) ) {
        uint8_t b0, b1;
        if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_BYTES ) {
            // big endian
            b0 = ( uint8_t )( sample >> 8 );
            b1 = (uint8_t)sample;
        } else {
            // little endian
            b0 = (uint8_t)sample;
            b1 = ( uint8_t )( sample >> 8 );
        }
        acamera_sbus_write_u8( p_bus, addr, b0 );
        acamera_sbus_write_u8( p_bus, addr + 1, b1 );
    } else {
        // error here
    }
}

void acamera_sbus_write_u32( acamera_sbus_t *p_bus, uintptr_t addr, uint32_t sample )
{
    if ( p_bus->mask & SBUS_MASK_SAMPLE_32BITS ) {
        if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_BYTES ) {
            uint16_t d0 = change_endian_u16( (uint16_t)sample );
            uint16_t d1 = change_endian_u16( ( uint16_t )( sample >> 16 ) );
            if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_WORDS ) {
                sample = ( d0 << 16 );
                sample |= d1;
            } else {
                sample = ( d1 << 16 );
                sample |= d0;
            }
        } else if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_WORDS ) {
            sample = ( sample >> 16 ) | ( sample << 16 );
        }
        p_bus->write_sample( p_bus, sbus_update_address( p_bus, addr ), sample, 4 );
    } else if ( SBUS_CAN_ADDRESS_16BITS( p_bus ) ) {
        uint16_t b0, b1;
        if ( p_bus->mask & SBUS_MASK_SAMPLE_SWAP_WORDS ) {
            // big endian
            b0 = ( uint16_t )( sample >> 16 );
            b1 = (uint16_t)sample;
        } else {
            // little endian
            b0 = (uint16_t)sample;
            b1 = ( uint16_t )( sample >> 16 );
        }
        acamera_sbus_write_u16( p_bus, addr, b0 );
        acamera_sbus_write_u16( p_bus, addr + SBUS_ADDRESS_16BIT_INCREMENT( p_bus ), b1 );
    } else {
        // error here
    }
}

void acamera_sbus_write_data_u8( acamera_sbus_t *p_bus, uintptr_t addr, uint8_t *p_data, int n_count )
{
    int i;
    for ( i = 0; i < n_count; ++i ) {
        acamera_sbus_write_u8( p_bus, addr, p_data[i] );
        addr += 1;
    }
}

void acamera_sbus_write_data_u16( acamera_sbus_t *p_bus, uintptr_t addr, uint16_t *p_data, int n_count )
{
    const int addr_step = SBUS_ADDRESS_16BIT_INCREMENT( p_bus );
    int i;
    if ( ( (size_t)p_data ) & 1 ) {
        // unaligned write
        for ( i = 0; i < n_count; ++i, ++p_data ) {
            acamera_sbus_write_u16( p_bus, addr, acamera_mem_read_u16( p_data ) );
            addr += addr_step;
        }
    } else {
        for ( i = 0; i < n_count; ++i ) {
            acamera_sbus_write_u16( p_bus, addr, p_data[i] );
            addr += addr_step;
        }
    }
}

void acamera_sbus_write_data_u32( acamera_sbus_t *p_bus, uintptr_t addr, uint32_t *p_data, int n_count )
{
    const int addr_step = SBUS_ADDRESS_32BIT_INCREMENT( p_bus );
    int i;
    if ( ( (size_t)p_data ) & 3 ) {
        // unaligned read
        for ( i = 0; i < n_count; ++i, ++p_data ) {
            acamera_sbus_write_u32( p_bus, addr, acamera_mem_read_u32( p_data ) );
            addr += addr_step;
        }
    } else {
        for ( i = 0; i < n_count; ++i ) {
            acamera_sbus_write_u32( p_bus, addr, p_data[i] );
            addr += addr_step;
        }
    }
}

void acamera_sbus_write_data( acamera_sbus_t *p_bus, uintptr_t addr, void *p_data, int n_size )
{
    const int addr_shift = SBUS_ADDRESS_SHIFT( p_bus );
    switch ( ( addr << addr_shift ) & 3 ) {
    case 3:
        acamera_sbus_write_u8( p_bus, addr, MEM_ALIGNED_READ( uint8_t, p_data ) );
        n_size -= 1;
        if ( !n_size ) {
            return;
        }
        p_data = MEM_OFFSET_PTR( p_data, 1 );
        addr += 1;
        break;
    case 0:
        break;
    case 1:
        acamera_sbus_write_u8( p_bus, addr, MEM_ALIGNED_READ( uint8_t, p_data ) );
        n_size -= 1;
        if ( !n_size ) {
            return;
        }
        p_data = MEM_OFFSET_PTR( p_data, 1 );
        addr += 1;
        break;
    case 2:
        if ( n_size == 1 ) {
            acamera_sbus_write_u8( p_bus, addr, MEM_ALIGNED_READ( uint8_t, p_data ) );
            return;
        }
        acamera_sbus_write_u16( p_bus, addr, acamera_mem_read_u16( p_data ) );
        n_size -= 2;
        if ( !n_size ) {
            return;
        }
        p_data = MEM_OFFSET_PTR( p_data, 2 );
        addr += 2 >> addr_shift;
        break;
    }
    if ( ( n_size >= 4 ) && ( p_bus->mask & SBUS_MASK_SAMPLE_32BITS ) ) {
        int n_len = n_size & ( ~3 );
        acamera_sbus_write_data_u32( p_bus, addr, (uint32_t *)p_data, n_size >> 2 );
        n_size -= n_len;
        if ( !n_size ) {
            return;
        }
        p_data = MEM_OFFSET_PTR( p_data, n_len );
        addr += n_len >> addr_shift;
    }
    if ( ( n_size >= 2 ) && ( p_bus->mask & SBUS_MASK_SAMPLE_16BITS ) ) {
        int n_len = n_size & ( ~1 );
        acamera_sbus_write_data_u16( p_bus, addr, (uint16_t *)p_data, n_size >> 1 );
        n_size -= n_len;
        if ( !n_size ) {
            return;
        }
        p_data = MEM_OFFSET_PTR( p_data, n_len );
        addr += n_len >> addr_shift;
    }
    if ( n_size ) {
        acamera_sbus_write_data_u8( p_bus, addr, (uint8_t *)p_data, n_size );
    }
}

void acamera_sbus_copy( acamera_sbus_t *p_bus_to, uintptr_t addr_to, acamera_sbus_t *p_bus_from, uint32_t addr_from, int n_size )
{
    if ( n_size > 4 ) {
        uint8_t mask = ( p_bus_to->mask ) | ( p_bus_from->mask );
        if ( mask & SBUS_MASK_SAMPLE_32BITS ) {
            const int n_count = n_size >> 2;
            const int i_to_step = SBUS_ADDRESS_32BIT_INCREMENT( p_bus_to );
            const int i_from_step = SBUS_ADDRESS_32BIT_INCREMENT( p_bus_from );
            int i;
            for ( i = 0; i < n_count; ++i ) {
                acamera_sbus_write_u32( p_bus_to, addr_to, acamera_sbus_read_u32( p_bus_from, addr_from ) );
                addr_to += i_to_step;
                addr_from += i_from_step;
            }
            n_size -= n_count * 4;
        } else if ( mask & SBUS_MASK_SAMPLE_16BITS ) {
            const int n_count = n_size >> 1;
            const int i_to_step = SBUS_ADDRESS_16BIT_INCREMENT( p_bus_to );
            const int i_from_step = SBUS_ADDRESS_16BIT_INCREMENT( p_bus_from );
            int i;
            for ( i = 0; i < n_count; ++i ) {
                acamera_sbus_write_u16( p_bus_to, addr_to, acamera_sbus_read_u16( p_bus_from, addr_from ) );
                addr_to += i_to_step;
                addr_from += i_from_step;
            }
            n_size -= n_count * 2;
        } else {
            int i;
            for ( i = 0; i < n_size; ++i ) {
                acamera_sbus_write_u8( p_bus_to, addr_to, acamera_sbus_read_u8( p_bus_from, addr_from ) );
                addr_to += 1;
                addr_from += 1;
            }
            n_size = 0;
        }
    }
    switch ( n_size ) {
    case 0:
        break;
    case 1:
        acamera_sbus_write_u8( p_bus_to, addr_to, acamera_sbus_read_u8( p_bus_from, addr_from ) );
        break;
    case 2:
        acamera_sbus_write_u16( p_bus_to, addr_to, acamera_sbus_read_u16( p_bus_from, addr_from ) );
        break;
    case 3:
        acamera_sbus_write_u8( p_bus_to, addr_to, acamera_sbus_read_u8( p_bus_from, addr_from ) );
        acamera_sbus_write_u8( p_bus_to, addr_to + 1, acamera_sbus_read_u8( p_bus_from, addr_from + 1 ) );
        acamera_sbus_write_u8( p_bus_to, addr_to + 2, acamera_sbus_read_u8( p_bus_from, addr_from + 2 ) );
        break;
    case 4:
        acamera_sbus_write_u32( p_bus_to, addr_to, acamera_sbus_read_u32( p_bus_from, addr_from ) );
        break;
    }
}

void acamera_sbus_init( acamera_sbus_t *p_bus, sbus_type_t interface_type )
{
    if ( p_bus != NULL ) {
        p_bus->p_control = NULL;
        switch ( interface_type ) {
        case sbus_i2c:
            acamera_sbus_i2c_init( p_bus );
            break;
        case sbus_spi:
            acamera_sbus_spi_init( p_bus );
            break;
        case sbus_isp:
            acamera_sbus_isp_init( p_bus );
            break;
        case sbus_isp_sw:
            acamera_sbus_isp_sw_init( p_bus );
            break;
        default:
            LOG( LOG_CRIT, "Invalid sbus protocol" );
            break;
        };
    } else {
        LOG( LOG_CRIT, "Failed to initialize sbus. input pointer is null" );
    }
}

void acamera_sbus_deinit( acamera_sbus_t *p_bus, sbus_type_t interface_type )
{
    if ( p_bus != NULL ) {
        switch ( interface_type ) {
        case sbus_i2c:
            acamera_sbus_i2c_deinit( p_bus );
            break;
        default:
            LOG( LOG_CRIT, "Invalid sbus protocol" );
            break;
        };
    } else {
        LOG( LOG_CRIT, "Failed to initialize sbus. input pointer is null" );
    }
}

void acamera_sbus_reset( sbus_type_t interface_type )
{
    switch ( interface_type ) {
    case sbus_i2c:
        i2c_init_access();
        break;
    case sbus_spi:
        break;
    case sbus_isp:
        break;
    default:
        break;
    };
}
