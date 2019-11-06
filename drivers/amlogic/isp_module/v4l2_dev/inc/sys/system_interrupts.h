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

#ifndef __SYSTEM_INTERRUPTS_H__
#define __SYSTEM_INTERRUPTS_H__

#include "acamera_types.h"

#define ACAMERA_IRQ_COUNT 32

#define ACAMERA_IRQ_MASK( num ) ( 1 << num )

typedef uint32_t system_fw_interrupt_mask_t;

typedef void ( *system_interrupt_handler_t )( void *ptr, uint32_t mask );


/**
 *   Initialize system interrupts
 *
 *   This function initializes system dependent interrupt functionality
 *
 *   @return none
 */
void system_interrupts_init( void );


/**
 *   Set an interrupt handler
 *
 *   This function is used by application to set an interrupt handler for all ISP related interrupt events.
 *
 *   @param
 *          handler - a callback to handle interrupts
 *          param - pointer to a context which must be send to interrupt handler
 *
 *   @return none
 */
void system_interrupt_set_handler( system_interrupt_handler_t handler, void *param );


/**
 *   Enable system interrupts
 *
 *   This function is used by firmware to enable system interrupts in a case if they were disabled before
 *
 *   @return none
 */
void system_interrupts_enable( void );


/**
 *   Disable system interrupts
 *
 *   This function is used by firmware to disable system interrupts for a short period of time.
 *   Usually IRQ register is updated by new interrupts but main interrupt handler is not called by a system.
 *
 *   @return none
 */
void system_interrupts_disable( void );

#endif /* __SYSTEM_INTERRUPTS_H__ */
