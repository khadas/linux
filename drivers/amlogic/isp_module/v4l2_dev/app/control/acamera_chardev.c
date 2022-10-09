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

#include "acamera_chardev.h"
#include "system_chardev.h"

int acamera_chardev_init( void )
{
    return system_chardev_init();
}

int acamera_chardev_read( void *unused, uint8_t *data, int size )
{
    return system_chardev_read( (char *)data, size );
}

int acamera_chardev_write( void *unused, const uint8_t *data, int size )
{
    return system_chardev_write( (char *)data, size );
}

int acamera_chardev_destroy( void )
{
    return system_chardev_destroy();
}