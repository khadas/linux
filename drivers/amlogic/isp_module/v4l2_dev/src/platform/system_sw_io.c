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

#include "acamera_logger.h"
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>

int32_t init_sw_io( void )
{
    int32_t result = 0;
    //
    return result;
}

int32_t close_sw_io( void )
{
    int32_t result = 0;
    return result;
}

void *system_sw_alloc( uint32_t size )
{
    return kzalloc( size, GFP_KERNEL | GFP_DMA | GFP_ATOMIC );
}

void system_sw_free( void *ptr )
{
    kfree( ptr );
}

uint32_t system_sw_read_32( uintptr_t addr )
{
    uint32_t result = 0;
    if ( (void *)addr != NULL ) {
        volatile uint32_t *p_addr = (volatile uint32_t *)( addr );
        result = *p_addr;
    } else {
        LOG( LOG_ERR, "Failed to read memory from address 0x%x. Base pointer is null ", addr );
    }
    return result;
}

uint16_t system_sw_read_16( uintptr_t addr )
{
    uint16_t result = 0;
    if ( (void *)addr != NULL ) {
        volatile uint16_t *p_addr = (volatile uint16_t *)( addr );
        result = *p_addr;
    } else {
        LOG( LOG_ERR, "Failed to read memory from address 0x%x. Base pointer is null ", addr );
    }
    return result;
}

uint8_t system_sw_read_8( uintptr_t addr )
{
    uint8_t result = 0;
    if ( (void *)addr != NULL ) {
        volatile uint8_t *p_addr = (volatile uint8_t *)( addr );
        result = *p_addr;
    } else {
        LOG( LOG_ERR, "Failed to read memory from address 0x%x. Base pointer is null ", addr );
    }
    return result;
}


void system_sw_write_32( uintptr_t addr, uint32_t data )
{
    if ( (void *)addr != NULL ) {
        volatile uint32_t *p_addr = (volatile uint32_t *)( addr );
        //LOG(LOG_CRIT, "SW WRITE full addr 0x%p addr %d, data %d", p_addr, addr, data );
        *p_addr = data;
    } else {
        LOG( LOG_ERR, "Failed to write %d to memory 0x%x. Base pointer is null ", data, addr );
    }
}

void system_sw_write_16( uintptr_t addr, uint16_t data )
{
    if ( (void *)addr != NULL ) {
        volatile uint16_t *p_addr = (volatile uint16_t *)( addr );
        *p_addr = data;
    } else {
        LOG( LOG_ERR, "Failed to write %d to memory 0x%x. Base pointer is null ", data, addr );
    }
}

void system_sw_write_8( uintptr_t addr, uint8_t data )
{
    if ( (void *)addr != NULL ) {
        volatile uint8_t *p_addr = (volatile uint8_t *)( addr );
        *p_addr = data;
    } else {
        LOG( LOG_ERR, "Failed to write %d to memory 0x%x. Base pointer is null ", data, addr );
    }
}
