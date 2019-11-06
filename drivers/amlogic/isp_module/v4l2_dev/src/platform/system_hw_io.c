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
#include "system_spinlock.h"
#include <asm/io.h>

static void *p_hw_base = NULL;
static sys_spinlock reg_lock;

int32_t init_hw_io( resource_size_t addr , resource_size_t size )
{
    p_hw_base = ioremap( addr, size );
    system_spinlock_init( &reg_lock );
    if ( p_hw_base == NULL ) {
        LOG( LOG_CRIT, "failed to map isp memory" );
        return -1;
    }
    return 0;
}

int32_t close_hw_io( void )
{
    int32_t result = 0;
    LOG( LOG_DEBUG, "IO functionality has been closed" );
    iounmap( p_hw_base );
    system_spinlock_destroy( reg_lock );
    return result;
}

uint32_t system_hw_read_32( uintptr_t addr )
{
    uint32_t result = 0;
    if ( p_hw_base != NULL ) {
        unsigned long flags;
        flags = system_spinlock_lock( reg_lock );
        result = ioread32( p_hw_base + addr );
        system_spinlock_unlock( reg_lock, flags );
    } else {
        LOG( LOG_ERR, "Failed to read memory from address %d. Base pointer is null ", addr );
    }
    return result;
}

uint16_t system_hw_read_16( uintptr_t addr )
{
    uint16_t result = 0;
    if ( p_hw_base != NULL ) {
        unsigned long flags;
        flags = system_spinlock_lock( reg_lock );
        result = ioread16( p_hw_base + addr );
        system_spinlock_unlock( reg_lock, flags );
    } else {
        LOG( LOG_ERR, "Failed to read memory from address %d. Base pointer is null ", addr );
    }
    return result;
}

uint8_t system_hw_read_8( uintptr_t addr )
{
    uint8_t result = 0;
    if ( p_hw_base != NULL ) {
        unsigned long flags;
        flags = system_spinlock_lock( reg_lock );
        result = ioread8( p_hw_base + addr );
        system_spinlock_unlock( reg_lock, flags );
    } else {
        LOG( LOG_ERR, "Failed to read memory from address %d. Base pointer is null ", addr );
    }
    return result;
}

uint32_t check_offset = 16;

void system_hw_write_32( uintptr_t addr, uint32_t data )
{
    if ( p_hw_base != NULL ) {
        void *ptr = (void *)( p_hw_base + addr );
        unsigned long flags;
        flags = system_spinlock_lock( reg_lock );
        iowrite32( data, ptr );
        system_spinlock_unlock( reg_lock, flags );
    } else {
        LOG( LOG_ERR, "Failed to write value %d to memory with offset %d. Base pointer is null ", data, addr );
    }
}

void system_hw_write_16( uintptr_t addr, uint16_t data )
{
    if ( p_hw_base != NULL ) {
        void *ptr = (void *)( p_hw_base + addr );
        unsigned long flags;
        flags = system_spinlock_lock( reg_lock );
        iowrite16( data, ptr );
        system_spinlock_unlock( reg_lock, flags );
    } else {
        LOG( LOG_ERR, "Failed to write value %d to memory with offset %d. Base pointer is null ", data, addr );
    }
}

void system_hw_write_8( uintptr_t addr, uint8_t data )
{
    if ( p_hw_base != NULL ) {
        void *ptr = (void *)( p_hw_base + addr );
        unsigned long flags;
        flags = system_spinlock_lock( reg_lock );
        iowrite8( data, ptr );
        system_spinlock_unlock( reg_lock, flags );
    } else {
        LOG( LOG_ERR, "Failed to write value %d to memory with offset %d. Base pointer is null ", data, addr );
    }
}
