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

#if !defined( __ACAMERA_BIT_MASK_H__ )
#define __ACAMERA_BIT_MASK_H__


#define ACAMERA_BITMASK_CELL_TYPE uint32_t
#define ACAMERA_BITMASK_BITS_PER_CELL ( sizeof( ACAMERA_BITMASK_CELL_TYPE ) * 8 )
#define ACAMERA_BITMASK_CELL_COUNT( bits_count ) \
    ( ( bits_count + ACAMERA_BITMASK_BITS_PER_CELL - 1 ) / ACAMERA_BITMASK_BITS_PER_CELL )

static __inline uint8_t acamera_bitmask_get_bit( const ACAMERA_BITMASK_CELL_TYPE *p_bits, int idx )
{
    return ( p_bits[idx / ACAMERA_BITMASK_BITS_PER_CELL] >> ( idx % ACAMERA_BITMASK_BITS_PER_CELL ) ) & 1;
}

static __inline void acamera_bitmask_set_bit( ACAMERA_BITMASK_CELL_TYPE *p_bits, int idx )
{
    p_bits[idx / ACAMERA_BITMASK_BITS_PER_CELL] |= 1 << ( idx % ACAMERA_BITMASK_BITS_PER_CELL );
}

static __inline void acamera_bitmask_clear_bit( ACAMERA_BITMASK_CELL_TYPE *p_bits, int idx )
{
    p_bits[idx / ACAMERA_BITMASK_BITS_PER_CELL] &= ~( 1 << ( idx % ACAMERA_BITMASK_BITS_PER_CELL ) );
}

#endif /* __ACAMERA_BIT_MASK_H__ */
