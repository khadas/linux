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

#if !defined( __FSM_OPERATIONS_H__ )
#define __FSM_OPERATIONS_H__

typedef struct _fsm_init_param_t_ {
    void *p_fsm_mgr;
    uintptr_t isp_base;

} fsm_init_param_t;

typedef void ( *FUN_PTR_INIT )( void *fsm, fsm_init_param_t *init_param );
typedef void ( *FUN_PTR_DEINIT )( void *fsm );

typedef int ( *FUN_PTR_RUN )( void *fsm );
typedef int ( *FUN_PTR_SET_PARAM )( void *p_fsm, uint32_t param_id, void *input, uint32_t input_size );
typedef int ( *FUN_PTR_GET_PARAM )( void *p_fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

typedef int ( *FUN_PTR_PROC_EVENT )( void *fsm, event_id_t event_id );
typedef void ( *FUN_PTR_PROC_INT )( void *fsm, uint8_t irq_event );


typedef struct _fsm_ops_t_ {

    FUN_PTR_INIT init;
    FUN_PTR_DEINIT deinit;

    FUN_PTR_RUN run;
    FUN_PTR_SET_PARAM set_param;
    FUN_PTR_GET_PARAM get_param;

    FUN_PTR_PROC_EVENT proc_event;
    FUN_PTR_PROC_INT proc_interrupt;
} fsm_ops_t;

typedef struct _fsm_common_t_ {
    void *p_fsm;

    void *p_fsm_mgr;
    uintptr_t isp_base;
    uint8_t ctx_id;

    fsm_ops_t ops;
} fsm_common_t;

typedef fsm_common_t *( *FUN_PTR_GET_FSM_COMMON )( uint8_t ctx_id );

#endif /* __FSM_OPERATIONS_H__ */
