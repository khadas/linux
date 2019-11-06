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

#ifndef __SYSTEM_SEMAPHORE_H__
#define __SYSTEM_SEMAPHORE_H__
//-----------------------------------------------------------------------------
#include "acamera_types.h"
//-----------------------------------------------------------------------------
typedef void *semaphore_t;

//-----------------------------------------------------------------------------

/**
 *   Initialize system semaphore
 *
 *   The function initializes a system dependent semaphore
 *
 *   @param void ** sem - the pointer to system semaphore will be returned
 *
 *   @return  0 - on success
 *           -1 - on error
 */
int32_t system_semaphore_init( semaphore_t *sem );


/**
 *   Unlock the semaphore pointed to by sem
 *
 *   The function increments (unlocks) the semaphore.
 *   If the semaphore value becomes greater than zero, then
 *   another process which is blocked by system_semaphore_wait will be
 *   woken up
 *
 *   @param   sem - pointer to semaphore returned by system_semaphore_init
 *
 *   @return  0 - on success
 *           -1 - on error
 */
int32_t system_semaphore_raise( semaphore_t sem );


/**
 *   Locks the semaphore pointed to by sem
 *
 *   The function decrements (locks) the semaphore.
 *   If the semaphore value is greater than zero, then
 *   the decrement proceeds, and the function returns.
 *
 *   @param   sem - pointer to semaphore returned by system_semaphore_init
 *
 *   @return  0 - on success
 *           -1 - on error
 */
int32_t system_semaphore_wait( semaphore_t sem, uint32_t timeout_ms );


/**
 *   Destroy semaphore
 *
 *   The function destroys semaphore which was created by system_semaphore_init
 *
 *   @return  0 - on success
 *           -1 - on error
 */
int32_t system_semaphore_destroy( semaphore_t sem );

//-----------------------------------------------------------------------------
#endif //__SYSTEM_SEMAPHORE_H__
