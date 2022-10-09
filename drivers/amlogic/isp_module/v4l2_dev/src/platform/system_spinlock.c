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
#include "system_spinlock.h"
#include <linux/spinlock.h>
#include <linux/slab.h>

int system_spinlock_init( sys_spinlock *lock )
{
    spinlock_t *slock = kmalloc( sizeof( spinlock_t ), GFP_KERNEL | __GFP_NOFAIL );

    if ( slock ) {
        *lock = slock;
        spin_lock_init( slock );
    }

    return slock ? 0 : -1;
}

unsigned long system_spinlock_lock( sys_spinlock lock )
{
    unsigned long flags = 0;
    spinlock_t *slock = (spinlock_t *)lock;

    spin_lock_irqsave( slock, flags );

    return flags;
}

void system_spinlock_unlock( sys_spinlock lock, unsigned long flags )
{
    spinlock_t *slock = (spinlock_t *)lock;

    spin_unlock_irqrestore( slock, flags );
}

void system_spinlock_destroy( sys_spinlock lock )
{
    spinlock_t *slock = (spinlock_t *)lock;

    if ( slock )
        kfree( slock );
}
