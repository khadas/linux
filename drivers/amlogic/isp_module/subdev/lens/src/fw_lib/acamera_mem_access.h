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

#ifndef __ACAMERA_MEM_ACCESS_H__
#define __ACAMERA_MEM_ACCESS_H__

#define MEM_OFFSET_PTR( p_data, offset ) \
    ( ( (uint8_t *)( p_data ) ) + ( offset ) )

#define MEM_ALIGNED_READ( type, p_data ) \
    ( *(type *)( (uint8_t *)( p_data ) ) )

#define MEM_ALIGNED_WRITE( type, p_buf, data ) \
    ( *(type *)( (uint8_t *)( p_buf ) ) ) = ( data )

uint16_t acamera_mem_read_u16( void *p_data );
uint32_t acamera_mem_read_u32( void *p_data );
void acamera_mem_write_u16( void *p_buf, uint16_t data );
void acamera_mem_write_u32( void *p_buf, uint32_t data );

#endif /* __ACAMERA_MEM_ACCESS_H__ */
