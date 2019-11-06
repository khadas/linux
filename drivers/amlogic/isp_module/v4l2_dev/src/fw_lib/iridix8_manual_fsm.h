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

#if !defined( __IRIDIX_FSM_H__ )
#define __IRIDIX_FSM_H__



typedef struct _iridix_fsm_t iridix_fsm_t;
typedef struct _iridix_fsm_t *iridix_fsm_ptr_t;
typedef const struct _iridix_fsm_t *iridix_fsm_const_ptr_t;

void iridix_initialize( iridix_fsm_ptr_t p_fsm );
void iridix_update( iridix_fsm_ptr_t p_fsm );
int iridix_set_tracking_frame_id( iridix_fsm_ptr_t p_fsm, uint32_t frame_id );

#include "sbuf.h"
void iridix_set_new_param( iridix_fsm_ptr_t p_fsm, sbuf_iridix_t *p_sbuf_gamma );

struct _iridix_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    uint16_t strength_target;
    uint32_t strength_avg;
    uint32_t mode;
    uint16_t dark_enh;
    uint32_t dark_enh_avg;
    uint32_t iridix_global_DG_avg;
    uint16_t mp_iridix_strength;
    uint32_t iridix_contrast;
    uint16_t iridix_global_DG;
    uint16_t iridix_global_DG_prev;

    uint32_t frame_id_tracking;
    uint32_t pre_frame_id_tracking;
    uint32_t is_output_ready;
};


void iridix_fsm_clear( iridix_fsm_ptr_t p_fsm );

void iridix_fsm_init( void *fsm, fsm_init_param_t *init_param );
int iridix_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int iridix_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

void iridix_fsm_process_interrupt( iridix_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void iridix_request_interrupt( iridix_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __IRIDIX_FSM_H__ */
