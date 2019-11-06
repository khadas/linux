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

#if !defined( __ACAMERA_EVENT_QUEUE_H__ )
#define __ACAMERA_EVENT_QUEUE_H__

#include "acamera.h"
#include "acamera_loop_buf.h"
#include "system_spinlock.h"

typedef struct acamera_event_queue *acamera_event_queue_ptr_t;
typedef const struct acamera_event_queue *acamera_event_queue_const_ptr_t;

typedef struct acamera_event_queue {
    sys_spinlock lock;
    acamera_loop_buf_t buf;
} acamera_event_queue_t;

static __inline void acamera_event_queue_init( acamera_event_queue_ptr_t p_queue, uint8_t *p_data_buf, int data_buf_size )
{
    acamera_loop_buffer_init( &p_queue->buf, p_data_buf, data_buf_size );
    system_spinlock_init( &p_queue->lock );
}
void acamera_event_queue_push( acamera_event_queue_ptr_t p_queue, int event );
int acamera_event_queue_pop( acamera_event_queue_ptr_t p_queue );
int32_t acamera_event_queue_not_empty( acamera_event_queue_ptr_t p_queue );


static __inline void acamera_event_queue_deinit( acamera_event_queue_ptr_t p_queue )
{
    system_spinlock_destroy( p_queue->lock );
}

#endif /* __ACAMERA_EVENT_QUEUE_H__ */
