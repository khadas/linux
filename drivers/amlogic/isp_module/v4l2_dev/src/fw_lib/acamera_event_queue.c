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

#include "acamera_event_queue.h"
#include "acamera_logger.h"
#include "system_spinlock.h"

static uint32_t not_empty_count;
static uint32_t reset_threshold = 1;
static uint64_t avail_event_mask;

static void acamera_event_queue_reset( acamera_loop_buf_ptr_t p_buf )
{
    p_buf->head = p_buf->tail = 0;
    avail_event_mask = 0;
}

void acamera_event_queue_push( acamera_event_queue_ptr_t p_queue, int event )
{
    int err = 0;
    acamera_loop_buf_ptr_t p_buf = &( p_queue->buf );
    unsigned long flags;

    flags = system_spinlock_lock( p_queue->lock );

    if ( ( p_buf->head < 0 ) ||
         ( p_buf->head >= p_buf->data_buf_size ) ||
         ( p_buf->tail < 0 ) ||
         ( p_buf->tail >= p_buf->data_buf_size ) ) {
        err = -1;
    }

    if ((1 << event) & avail_event_mask) {
        err = 0;
        LOG( LOG_DEBUG, "event %d is duplicated, skip it at this time", event);
    } else {
        int pos = acamera_loop_buffer_write_u8( p_buf, 0, (uint8_t)event );
        if ( pos != p_buf->tail ) {
            p_buf->head = pos;
            avail_event_mask |= (1 << event);
        } else {
            err = -2;
            acamera_event_queue_reset( p_buf );
        }
    }

    system_spinlock_unlock( p_queue->lock, flags );

    if ( -1 == err ) {
        LOG( LOG_ERR, "Event Queue Corrupted\n" );
    } else if ( -2 == err ) {
        static uint32_t counter = 0;
        if ( !( counter++ & 0x3F ) )
            LOG( LOG_CRIT, "Event Queue overflow\n" );
    }
}

int acamera_event_queue_pop( acamera_event_queue_ptr_t p_queue )
{
    int rc = 0;
    unsigned long flags;
    acamera_loop_buf_ptr_t p_buf = &( p_queue->buf );

    flags = system_spinlock_lock( p_queue->lock );

    if ( p_buf->head == p_buf->tail ) {
        rc = -1;
    }

    if ( ( p_buf->head < 0 ) ||
         ( p_buf->head >= p_buf->data_buf_size ) ||
         ( p_buf->tail < 0 ) ||
         ( p_buf->tail >= p_buf->data_buf_size ) ) {
        rc = -2;
    }

    if ( !rc ) {
        int pos;
        rc = acamera_loop_buffer_read_u8( p_buf, 0 );

        pos = p_buf->tail + 1;
        if ( pos >= p_buf->data_buf_size ) {
            pos -= p_buf->data_buf_size;
        }

        p_buf->tail = pos;
        if (rc >= 0 && rc < 64) {
            if (((1 << rc) & avail_event_mask) == 0)
               LOG( LOG_DEBUG, "event %d is missed, mask is 0x%x", rc, avail_event_mask);
            else
               avail_event_mask &= ~(1 << rc);
        }
    }

    system_spinlock_unlock( p_queue->lock, flags );

    if ( -2 == rc ) {
        LOG( LOG_ERR, "Event Queue Corrupted\n" );
    }

    return rc;
}


int32_t acamera_event_queue_not_empty( acamera_event_queue_ptr_t p_queue )
{
    int32_t result = -1;
    uint32_t flags = 0;
    uint32_t pos = 0;
    uint64_t event = 0;
    unsigned char event_id = 0;
    acamera_loop_buf_ptr_t p_buf = &( p_queue->buf );
    flags = system_spinlock_lock( p_queue->lock );
    if ( p_buf->head == p_buf->tail ) {
        result = 0;
        not_empty_count = 0;
    }

    if (result) {
        while (p_buf->head > (p_buf->tail + pos)) {
            event_id = p_buf->p_data_buf[pos + p_buf->tail];
            event |= (1 << event_id);
            pos++;
        }
        LOG( LOG_DEBUG, "p_buf->head:%d, p_buf->tail:%d, remained event mask: 0x%llx" , p_buf->head, p_buf->tail, event);
        not_empty_count++;
        if (not_empty_count >= reset_threshold) {
            acamera_event_queue_reset(p_buf);
            not_empty_count = 0;
        }
        result = pos;
    }
    system_spinlock_unlock( p_queue->lock, flags );
    return result;
}
