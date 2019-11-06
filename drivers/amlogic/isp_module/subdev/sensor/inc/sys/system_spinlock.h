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

#ifndef __SYSTEM_SPINLOCK_H__
#define __SYSTEM_SPINLOCK_H__
//-----------------------------------------------------------------------------
#include "acamera_types.h"
//-----------------------------------------------------------------------------
typedef void *sys_spinlock;

//-----------------------------------------------------------------------------

/**
 *   Initialize system spinlock
 *
 *   The function initializes a system dependent spinlock
 *
 *   @param void ** lock - the pointer to system spinlock will be returned
 *
 *   @return  0 - on success
 *           -1 - on error
 */
int system_spinlock_init( sys_spinlock *lock );


/**
 *   Locks the spinlock pointed to by lock
 *
 *   @param   lock - pointer to spinlock returned by system_spinlock_init
 *
 *   @return  flags - used when unlock
 */
unsigned long system_spinlock_lock( sys_spinlock lock );


/**
 *   Unlock the spinlock pointed to by lock
 *
 *   @param   lock - pointer to spinlock returned by system_spinlock_init
 *
 *   @return  void
 */
void system_spinlock_unlock( sys_spinlock lock, unsigned long flags );


/**
 *   Destroy spinlock
 *
 *   The function destroys spinlock which was created by system_spinlock_init
 *
 *   @return  void
 */
void system_spinlock_destroy( sys_spinlock lock );

//-----------------------------------------------------------------------------
#endif //__SYSTEM_SPINLOCK_H__
