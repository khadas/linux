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

#if !defined( __SBUF_FSM_H__ )
#define __SBUF_FSM_H__



typedef struct _sbuf_fsm_t sbuf_fsm_t;
typedef struct _sbuf_fsm_t *sbuf_fsm_ptr_t;
typedef const struct _sbuf_fsm_t *sbuf_fsm_const_ptr_t;

void sbuf_fsm_initialize( sbuf_fsm_ptr_t p_fsm );
void sbuf_deinit( sbuf_fsm_ptr_t p_fsm );
void sbuf_update_ae_idx( sbuf_fsm_ptr_t p_fsm );
void sbuf_update_awb_idx( sbuf_fsm_ptr_t p_fsm );
void sbuf_update_af_idx( sbuf_fsm_ptr_t p_fsm );
void sbuf_update_gamma_idx( sbuf_fsm_ptr_t p_fsm );
void sbuf_update_calibration_data( sbuf_fsm_ptr_t p_fsm );


struct _sbuf_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;

    fsm_irq_mask_t mask;
    uint32_t opened;
    uint32_t mode;
    uint8_t is_paused;
};


void sbuf_fsm_clear( sbuf_fsm_ptr_t p_fsm );

void sbuf_fsm_init( void *fsm, fsm_init_param_t *init_param );
int sbuf_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );

uint8_t sbuf_fsm_process_event( sbuf_fsm_ptr_t p_fsm, event_id_t event_id );

void sbuf_fsm_process_interrupt( sbuf_fsm_const_ptr_t p_fsm, uint8_t irq_event );


#endif /* __SBUF_FSM_H__ */
