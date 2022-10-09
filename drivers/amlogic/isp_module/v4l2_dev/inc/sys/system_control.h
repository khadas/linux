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

#ifndef __SYSTEM_CONTROL_H__
#define __SYSTEM_CONTROL_H__


/**
 *   Initialize customer platform
 *
 *   This function is called by application before any other system routines
 *
 *   @return none
 */
void bsp_init( void );


/**
 *   Destroys customer platform
 *
 *   This function is called by application after any other system routine
 *
 *   @return none
 */
void bsp_destroy( void );


#endif // __SYSTEM_CONTROL_H__
