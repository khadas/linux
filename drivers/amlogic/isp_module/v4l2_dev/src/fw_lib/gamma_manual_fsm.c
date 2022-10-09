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
#include "gamma_manual_fsm.h"
#include "acamera_3aalg_preset.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_GAMMA_MANUAL
#endif

void gamma_manual_fsm_clear( gamma_manual_fsm_t *p_fsm )
{
    p_fsm->gain_target = 100;
    p_fsm->gain_avg = 0;
    p_fsm->nodes_target = 100;
    p_fsm->nodes_avg = 0;

    p_fsm->cur_frame_id_tracking = 0;
    p_fsm->gamma_gain = 340;
    p_fsm->gamma_offset = 15;
}

void gamma_manual_request_interrupt( gamma_manual_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void gamma_manual_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    gamma_manual_fsm_t *p_fsm = (gamma_manual_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    gamma_manual_fsm_clear( p_fsm );

    gamma_manual_init( p_fsm );
}


int gamma_manual_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    gamma_manual_fsm_t *p_fsm = (gamma_manual_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_GAMMA_NEW_PARAM:
        if ( !input || input_size != sizeof( sbuf_gamma_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        gamma_manual_set_new_param( p_fsm, (sbuf_gamma_t *)input );

        break;
	case FSM_PARAM_SET_GAMMA_PRESET:
		if ( !input || input_size != sizeof( isp_gamma_preset_t ) ) {
			LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
			rc = -1;
			break;
		}
		
		isp_gamma_preset_t *p_new = (isp_gamma_preset_t *)input;
		//p_fsm->skip_cnt = p_new->skip_cnt;
		p_fsm->gamma_gain = p_new->gamma_gain;
		p_fsm->gamma_offset = p_new->gamma_offset; 	   
		gamma_manual_update(p_fsm);
		break;	

    default:
        rc = -1;
        break;
    }

    return rc;
}


uint8_t gamma_manual_fsm_process_event( gamma_manual_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;
    case event_id_gamma_new_param_ready:
        gamma_manual_update( p_fsm );
        b_event_processed = 1;
        break;
    }

    return b_event_processed;
}
