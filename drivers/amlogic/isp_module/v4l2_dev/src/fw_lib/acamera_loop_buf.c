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

#include "acamera_loop_buf.h"

void acamera_loop_buffer_init( acamera_loop_buf_ptr_t p_buf, uint8_t *p_data_buf, int data_buf_size )
{
    p_buf->head = p_buf->tail = 0;
    p_buf->p_data_buf = p_data_buf;
    p_buf->data_buf_size = data_buf_size;
}

uint8_t acamera_loop_buffer_read_u8( acamera_loop_buf_ptr_t p_buf, int pos )
{
    pos += p_buf->tail;
    if ( pos >= p_buf->data_buf_size ) {
        pos -= p_buf->data_buf_size;
    }
    return p_buf->p_data_buf[pos];
}

int acamera_loop_buffer_write_u8( acamera_loop_buf_ptr_t p_buf, int pos, uint8_t sample )
{
    pos += p_buf->head;
    if ( pos >= p_buf->data_buf_size ) {
        pos -= p_buf->data_buf_size;
    }
    p_buf->p_data_buf[pos++] = sample;
    if ( pos >= p_buf->data_buf_size ) {
        pos -= p_buf->data_buf_size;
    }
    return pos;
}
