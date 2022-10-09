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

#ifndef __SYSTEM_STDLIB_H__
#define __SYSTEM_STDLIB_H__

//#include "acamera_types.h"
#include <linux/types.h>
#include <asm/string.h>

/**
 *   Copy block of memory
 *
 *   Copies the values of num bytes from
 *   the location pointed to by source directly to the memory block
 *   pointed to by destination.
 *
 *   @param   dst - pointer to the destination array where the content is to be copied
 *   @param   src - pointer to source of data to be copied
 *   @param   size - number of bytes to copy
 *
 *   @return  0 - success
 *           -1 - on error
 */

int32_t system_memcpy( void *dst, const void *src, uint32_t size );


/**
 *   Fill block of memory
 *
 *   Sets the first size bytes of the block of memory
 *   pointed by ptr to the specified value
 *
 *   @param   ptr - pointer to the block of memory to fill
 *   @param   value - valute to be set
 *   @param   size - number of bytes to be set to the value
 *
 *   @return  0 - success
 *           -1 - on error
 */

int32_t system_memset( void *ptr, uint8_t value, uint32_t size );

#endif // __SYSTEM_STDLIB_H__
