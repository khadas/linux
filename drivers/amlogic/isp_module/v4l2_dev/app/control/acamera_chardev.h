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

#ifndef ACAMERA_CHARDEV_H
#define ACAMERA_CHARDEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "acamera_types.h"

int acamera_chardev_init( void );
int acamera_chardev_read( void *unused, uint8_t *data, int size );
int acamera_chardev_write( void *unused, const uint8_t *data, int size );
int acamera_chardev_destroy( void );

#ifdef __cplusplus
}
#endif

#endif /* ACAMERA_CHARDEV_H */
