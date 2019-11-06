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

#if !defined( __GAMMA_MANUAL_FSM_H__ )
#define __GAMMA_MANUAL_FSM_H__



typedef struct _gamma_manual_fsm_t gamma_manual_fsm_t;
typedef struct _gamma_manual_fsm_t *gamma_manual_fsm_ptr_t;
typedef const struct _gamma_manual_fsm_t *gamma_manual_fsm_const_ptr_t;


void gamma_manual_init( gamma_manual_fsm_ptr_t p_fsm );
void gamma_manual_update( gamma_manual_fsm_ptr_t p_fsm );
void gamma_manual_read_histogram( gamma_manual_fsm_ptr_t p_fsm );


#include "sbuf.h"
void gamma_manual_set_new_param( gamma_manual_fsm_ptr_t p_fsm, sbuf_gamma_t *p_sbuf_gamma );

struct _gamma_manual_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    int32_t wb_log2[4];
    uint32_t fullhist_sum;
    uint16_t gain_target;
    uint32_t gain_avg;
    uint16_t nodes_target;
    uint32_t nodes_avg;

    uint32_t cur_frame_id_tracking;
    uint32_t gamma_gain;
    uint32_t gamma_offset;
};


void gamma_manual_fsm_clear( gamma_manual_fsm_ptr_t p_fsm );

void gamma_manual_fsm_init( void *fsm, fsm_init_param_t *init_param );
int gamma_manual_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
uint8_t gamma_manual_fsm_process_event( gamma_manual_fsm_ptr_t p_fsm, event_id_t event_id );

void gamma_manual_fsm_process_interrupt( gamma_manual_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void gamma_manual_request_interrupt( gamma_manual_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __GAMMA_MANUAL_FSM_H__ */
