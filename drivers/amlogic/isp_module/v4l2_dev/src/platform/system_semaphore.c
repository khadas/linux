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
#include "system_semaphore.h"
#include <linux/semaphore.h>
#include <linux/slab.h>


int32_t system_semaphore_init( semaphore_t *sem )
{
    struct semaphore *sys_sem = kmalloc( sizeof( struct semaphore ), GFP_KERNEL | __GFP_NOFAIL );
    *sem = sys_sem;
    sema_init( sys_sem, 1 );
    return 0;
}


int32_t system_semaphore_raise( semaphore_t sem )
{
    struct semaphore *sys_sem = (struct semaphore *)sem;
    up( sys_sem );
    return 0;
}

int32_t system_semaphore_wait( semaphore_t sem, uint32_t timeout_ms )
{
    struct semaphore *sys_sem = (struct semaphore *)sem;

    if ( timeout_ms ) {
        return down_timeout( sys_sem, msecs_to_jiffies( timeout_ms ) );
    } else {
        return down_interruptible( sys_sem );
    }
}

int32_t system_semaphore_destroy( semaphore_t sem )
{
    struct semaphore *sys_sem = (struct semaphore *)sem;
    kfree( sys_sem );
    return 0;
}
