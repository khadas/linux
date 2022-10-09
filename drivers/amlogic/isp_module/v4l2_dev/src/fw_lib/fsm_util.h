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

#if !defined( __FSM_UTILS_H__ )
#define __FSM_UTILS_H__

typedef struct _fsm_irq_mask_t_ {
    uint32_t irq_mask;
    uint32_t repeat_irq_mask;
} fsm_irq_mask_t;

#ifndef ARR_SIZE
#define ARR_SIZE( x ) ( sizeof( x ) / sizeof( x[0] ) )
#endif

uint32_t acamera_fsm_util_is_irq_event_ignored( fsm_irq_mask_t *p_mask, uint8_t irq_event );

uint32_t acamera_fsm_util_get_cur_frame_id( fsm_common_t *p_fsm );

#endif /* __FSM_UTILS_H__ */
