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

#include "acamera_fw.h"
#include "metadata_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_METADATA
#endif

void metadata_fsm_clear( metadata_fsm_t *p_fsm )
{
}

void metadata_request_interrupt( metadata_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void metadata_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    metadata_fsm_t *p_fsm = (metadata_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    metadata_fsm_clear( p_fsm );

    metadata_initialize( p_fsm );
}


int metadata_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    metadata_fsm_t *p_fsm = (metadata_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_META_REGISTER_CB: {
        if ( !input || input_size != sizeof( metadata_callback_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        metadata_regist_callback( p_fsm, (metadata_callback_t)input );
        break;
    }
    default:
        rc = -1;
        break;
    }

    return rc;
}


uint8_t metadata_fsm_process_event( metadata_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;
    case event_id_metadata_ready:
        metadata_post_meta( p_fsm );
        b_event_processed = 1;
        break;
    case event_id_metadata_update:
        metadata_update_meta( p_fsm );
        b_event_processed = 1;
        break;
    }

    return b_event_processed;
}
