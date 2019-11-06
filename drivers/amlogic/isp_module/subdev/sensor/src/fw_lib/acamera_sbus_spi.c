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

#include "acamera_sbus_spi.h"
#include "system_spi.h"
#if ISP_FW_BUILD
#include "acamera_fw.h"
#endif
static void spi_io_write_sample( acamera_sbus_t *p_sbus, uintptr_t addr, uint32_t sample, uint8_t sample_size )
{
    uint32_t mask = p_sbus->mask;
    uint32_t sel_mask = 1 << p_sbus->device;
    uint32_t data = 0x0;
#if ISP_FW_BUILD
    const acamera_context_ptr_t p_ctx = (const acamera_context_ptr_t)p_sbus->p_control;
#endif
    if ( sample_size == 4 ) {
        uint8_t addr_size = 0;
        if ( mask & SBUS_MASK_ADDR_8BITS )
            addr_size = 1;
        else if ( mask & SBUS_MASK_ADDR_16BITS )
            addr_size = 2;
        else if ( mask & SBUS_MASK_ADDR_32BITS )
            addr_size = 4;
#if ISP_FW_BUILD
        if ( p_ctx )
            acamera_fw_interrupts_disable( p_ctx );
#endif
        system_spi_rw48( sel_mask, p_sbus->control, addr, addr_size, sample, sample_size );
#if ISP_FW_BUILD
        if ( p_ctx )
            acamera_fw_interrupts_enable( p_ctx );
#endif

    } else {
        if ( ( mask & SBUS_MASK_SPI_HALF_ADDR ) && !( mask & SBUS_MASK_ADDR_SWAP_BYTES ) ) // divide addr by 2 if possible
        {
            addr >>= 1;
        }
        if ( mask & SBUS_MASK_SPI_LSB ) // LSB sending first
        {
            data = sample;
            if ( ( mask & SBUS_MASK_ADDR_BITS_MASK ) == SBUS_MASK_ADDR_16BITS ) {
                data <<= 16;
                data |= addr;
            } else if ( ( mask & SBUS_MASK_ADDR_BITS_MASK ) == SBUS_MASK_ADDR_8BITS ) {
                data <<= 8;
                data |= addr;
            } else if ( ( mask & SBUS_MASK_ADDR_BITS_MASK ) == SBUS_MASK_ADDR_24BITS ) {
                data <<= 16;
                data |= addr;
                data <<= 8;
                data |= p_sbus->device_id;
            } else {
                // 32-bits address for SPI is required special treatment
                return;
            }
        } else {
            data = 0;
            if ( mask & SBUS_MASK_ADDR_24BITS ) {
                data |= p_sbus->device_id;
                data <<= 8;
            }
            data |= addr;
            if ( mask & SBUS_MASK_SAMPLE_16BITS )
                data <<= 16;
            else if ( mask & SBUS_MASK_SAMPLE_8BITS )
                data <<= 8;
            else {
                // 32-bits samples for SPI is required special treatment
                return;
            }
            data |= sample;
        }
        if ( mask & SBUS_MASK_SPI_INVERSE_DATA ) {
            data = ~data;
        }
#if ISP_FW_BUILD
        if ( p_ctx )
            acamera_fw_interrupts_disable( p_ctx );
#endif
        system_spi_rw32( sel_mask, p_sbus->control, data, sample_size );
#if ISP_FW_BUILD
        if ( p_ctx )
            acamera_fw_interrupts_enable( p_ctx );
#endif
    }
}

static uint32_t spi_io_read_sample( acamera_sbus_t *p_sbus, uintptr_t addr, uint8_t sample_size )
{
    uint32_t mask = p_sbus->mask;
    uint32_t sel_mask = 1 << p_sbus->device;
    uint32_t data = 0x0;
#if ISP_FW_BUILD
    const acamera_context_ptr_t p_ctx = (const acamera_context_ptr_t)p_sbus->p_control;
#endif
    if ( ( mask & SBUS_MASK_SPI_HALF_ADDR ) && !( mask & SBUS_MASK_ADDR_SWAP_BYTES ) ) // divide addr by 2 if required
    {
        addr >>= 1;
    }
    // Set MSB of address
    if ( mask & SBUS_MASK_SPI_READ_MSB_SET ) {

        if ( ( mask & SBUS_MASK_ADDR_8BITS ) || ( ( mask & SBUS_MASK_ADDR_16BITS ) && ( mask & SBUS_MASK_ADDR_SWAP_BYTES ) ) ) {
            addr |= 0x80;
        } else if ( ( mask & SBUS_MASK_ADDR_16BITS ) && !( mask & SBUS_MASK_ADDR_SWAP_BYTES ) ) {
            addr |= 0x8000;
        }
        // 32-bit address is not supported yet
    }

    if ( sample_size == 4 ) {
        uint8_t addr_size = 0;
        if ( mask & SBUS_MASK_ADDR_8BITS )
            addr_size = 1;
        else if ( mask & SBUS_MASK_ADDR_16BITS )
            addr_size = 2;
        else if ( mask & SBUS_MASK_ADDR_32BITS )
            addr_size = 4;
#if ISP_FW_BUILD
        if ( p_ctx )
            acamera_fw_interrupts_disable( p_ctx );
#endif
        data = system_spi_rw48( sel_mask, p_sbus->control, addr, addr_size, data, sample_size );
#if ISP_FW_BUILD
        if ( p_ctx )
            acamera_fw_interrupts_enable( p_ctx );
#endif
    } else {
        if ( ( mask & SBUS_MASK_ADDR_BITS_MASK ) == SBUS_MASK_ADDR_24BITS ) {
            addr <<= 8;
            addr |= p_sbus->device_id_read;
        }
        if ( mask & SBUS_MASK_SPI_LSB ) {

            if ( mask & SBUS_MASK_SPI_INVERSE_DATA ) {
                addr = ~addr;
            }
            if ( mask & SBUS_MASK_ADDR_16BITS ) {
#if ISP_FW_BUILD
                if ( p_ctx )
                    acamera_fw_interrupts_disable( p_ctx );
#endif
                data = system_spi_rw32( sel_mask, p_sbus->control, addr, sample_size ) >> 16;

#if ISP_FW_BUILD
                if ( p_ctx )
                    acamera_fw_interrupts_enable( p_ctx );
#endif
            } else if ( mask & SBUS_MASK_ADDR_8BITS ) {
#if ISP_FW_BUILD
                if ( p_ctx )
                    acamera_fw_interrupts_disable( p_ctx );
#endif
                data = system_spi_rw32( sel_mask, p_sbus->control, addr, sample_size ) >> 8;


#if ISP_FW_BUILD
                if ( p_ctx )
                    acamera_fw_interrupts_enable( p_ctx );
#endif
            } else {
                // 32-bits address for SPI is required special treatment
                data = 0xFFFFFFFF;
            }
        } else {

            if ( mask & ( SBUS_MASK_SAMPLE_16BITS | SBUS_MASK_SAMPLE_8BITS ) ) {
                if ( mask & SBUS_MASK_SAMPLE_16BITS ) {
                    addr <<= 16;
                } else if ( mask & SBUS_MASK_SAMPLE_8BITS ) {
                    addr <<= 8;
                }
                if ( mask & SBUS_MASK_SPI_INVERSE_DATA ) {
                    addr = ~addr;
                }
#if ISP_FW_BUILD
                if ( p_ctx )
                    acamera_fw_interrupts_disable( p_ctx );
#endif
                data = system_spi_rw32( sel_mask, p_sbus->control, addr, sample_size );
#if ISP_FW_BUILD
                if ( p_ctx )
                    acamera_fw_interrupts_enable( p_ctx );
#endif
            } else {
                // 32-bits samples for SPI are required special treatment

                data = 0xFFFFFFFF;
            }
        }
        if ( data != 0xFFFFFFFF ) {
            if ( sample_size == 1 )
                data &= 0xFF;
            else if ( sample_size == 2 )
                data &= 0xFFFF;
        }
    }

    return data;
}

void acamera_sbus_spi_init( acamera_sbus_ptr_t p_sbus )
{
    p_sbus->read_sample = spi_io_read_sample;
    p_sbus->write_sample = spi_io_write_sample;
    system_spi_init();
}
