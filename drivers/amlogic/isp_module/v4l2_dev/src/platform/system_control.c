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
#include "system_control.h"
#include "acamera_logger.h"
#include "system_interrupts.h"
#include <asm/io.h>


extern int32_t hdmi_init( void );

extern void system_interrupts_deinit( void );


void bsp_init( void )
{
    //system_interrupts_init();
    //hdmi_init();
}

void bsp_destroy( void )
{
    system_interrupts_disable();
    //system_interrupts_deinit();
}
