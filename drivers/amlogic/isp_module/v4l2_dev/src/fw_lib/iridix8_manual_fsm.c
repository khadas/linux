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
#include "iridix8_manual_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_IRIDIX8_MANUAL
#endif

void iridix_fsm_clear( iridix_fsm_t *p_fsm )
{
    p_fsm->strength_target = IRIDIX_STRENGTH_TARGET;
    p_fsm->strength_avg = IRIDIX_STRENGTH_TARGET * CALIBRATION_IRIDIX_AVG_COEF_INIT;
    p_fsm->mode = 0;
    p_fsm->dark_enh = 15000;
    p_fsm->dark_enh_avg = IRIDIX_STRENGTH_TARGET * CALIBRATION_IRIDIX_AVG_COEF_INIT * 2;
    p_fsm->iridix_global_DG_avg = IRIDIX_STRENGTH_TARGET * CALIBRATION_IRIDIX_AVG_COEF_INIT * 2;
    p_fsm->mp_iridix_strength = 0;
    p_fsm->iridix_contrast = 256;
    p_fsm->iridix_global_DG = 256;
    p_fsm->iridix_global_DG_prev = 256;

    p_fsm->frame_id_tracking = 0;
    p_fsm->pre_frame_id_tracking = 0;
    p_fsm->is_output_ready = 0;
}

void iridix_request_interrupt( iridix_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void iridix_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    iridix_fsm_t *p_fsm = (iridix_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    iridix_fsm_clear( p_fsm );

    iridix_initialize( p_fsm );
}


int iridix_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    iridix_fsm_t *p_fsm = (iridix_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_IRIDIX_INIT:
        iridix_initialize( p_fsm );
        break;

    case FSM_PARAM_SET_IRIDIX_NEW_PARAM:
        if ( !input || input_size != sizeof( sbuf_iridix_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        iridix_set_new_param( p_fsm, (sbuf_iridix_t *)input );
        break;

    case FSM_PARAM_SET_IRIDIX_FRAME_ID:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        rc = iridix_set_tracking_frame_id( p_fsm, *(uint32_t *)input );

        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}


int iridix_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    iridix_fsm_t *p_fsm = (iridix_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_IRIDIX_CONTRAST: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->iridix_contrast;

        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}
