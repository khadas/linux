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
#include "af_manual_fsm.h"
#include "sbuf.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_AF_MANUAL
#endif

extern void af_set_new_param( AF_fsm_ptr_t p_fsm, sbuf_af_t *p_sbuf_af );

void AF_fsm_clear( AF_fsm_t *p_fsm )
{
    p_fsm->frame_num = 0;
    p_fsm->mode = AF_MODE_AF;
    p_fsm->pos_manual = 0;
    p_fsm->new_pos = 0;
    p_fsm->roi = 0x4040C0C0;
    p_fsm->lens_driver_ok = 0;
    p_fsm->roi_api = 0x4040C0C0;
    p_fsm->frame_skip_cnt = 0x0;
    p_fsm->frame_skip_start = 0x0;
    p_fsm->last_pos_done = 0;
    p_fsm->new_last_sharp = 0;
    p_fsm->last_sharp_done = 0;
    memset( &p_fsm->lens_ctrl, 0, sizeof( p_fsm->lens_ctrl ) );
}

void AF_request_interrupt( AF_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void AF_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    AF_fsm_t *p_fsm = (AF_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    AF_fsm_clear( p_fsm );

    AF_init( p_fsm );
}


int AF_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    AF_fsm_t *p_fsm = (AF_fsm_t *)fsm;

    if ( !p_fsm->lens_driver_ok ) {
        LOG( LOG_INFO, "Not supported: no lens driver." );
        return -1;
    }

    switch ( param_id ) {
    case FSM_PARAM_SET_AF_NEW_PARAM:
        if ( !input || input_size != sizeof( sbuf_af_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        af_set_new_param( p_fsm, (sbuf_af_t *)input );

        break;

    case FSM_PARAM_SET_AF_MODE:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->mode = *(uint32_t *)input;

        break;

    case FSM_PARAM_SET_AF_RANGE_LOW: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint32_t value = *(uint32_t *)input;

        if ( value <= 256 ) {
            af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
            p_fsm->pos_min = param->pos_min + ( uint16_t )( ( uint32_t )( param->pos_max - param->pos_min ) * value >> 8 );
        }
        break;
    }

    case FSM_PARAM_SET_AF_RANGE_HIGH: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint32_t value = *(uint32_t *)input;

        if ( value <= 256 ) {
            af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
            p_fsm->pos_max = param->pos_min + ( uint16_t )( ( uint32_t )( param->pos_max - param->pos_min ) * value >> 8 );
        }

        break;
    }

    case FSM_PARAM_SET_AF_ROI: {
        if ( !input || input_size != sizeof( fsm_param_roi_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_roi_t *p_new = (fsm_param_roi_t *)input;
        p_fsm->roi_api = p_new->roi_api;
        p_fsm->roi = p_new->roi;


        break;
    }

    case FSM_PARAM_SET_AF_MANUAL_POS: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint32_t pos_manual = *(uint32_t *)input;

        if ( pos_manual <= 256 ) {
            p_fsm->pos_manual = pos_manual;
        }

        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}


int AF_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    AF_fsm_t *p_fsm = (AF_fsm_t *)fsm;

    if ( !p_fsm->lens_driver_ok ) {
        LOG( LOG_INFO, "Not supported: no lens driver." );
        return -1;
    }

    switch ( param_id ) {
    case FSM_PARAM_GET_AF_MODE:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->mode;

        break;

    case FSM_PARAM_GET_LENS_PARAM: {
        if ( !output || output_size != sizeof( lens_param_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        const lens_param_t *p_lens_param = p_fsm->lens_ctrl.get_parameters( p_fsm->lens_ctx );

        *(lens_param_t *)output = *p_lens_param;

        break;
    }

    case FSM_PARAM_GET_AF_MANUAL_POS: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint16_t af_range = p_fsm->pos_max - p_fsm->pos_min;
        uint32_t last_position = p_fsm->last_position;
        uint32_t pos = 0;

        if ( last_position < p_fsm->pos_min ) {
            last_position = p_fsm->pos_min;
        } else if ( last_position > p_fsm->pos_max ) {
            last_position = p_fsm->pos_max;
        }

        pos = ( ( ( last_position - p_fsm->pos_min ) << 8 ) + af_range / 2 ) / ( af_range );

        if ( pos > 255 ) {
            pos = 255;
        }

        *(uint32_t *)output = pos;

        break;
    }

    case FSM_PARAM_GET_AF_RANGE_LOW: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
        *(uint32_t *)output = ( param->pos_max != param->pos_min ) ? ( ( uint32_t )( p_fsm->pos_min - param->pos_min ) << 8 ) / ( param->pos_max - param->pos_min ) : -1;

        break;
    }

    case FSM_PARAM_GET_AF_RANGE_HIGH: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        af_lms_param_t *param = (af_lms_param_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AF_LMS );
        *(uint32_t *)output = ( param->pos_max != param->pos_min ) ? ( ( uint32_t )( p_fsm->pos_max - param->pos_min ) << 8 ) / ( param->pos_max - param->pos_min ) : -1;

        break;
    }

    case FSM_PARAM_GET_AF_ROI: {
        if ( !output || output_size != sizeof( fsm_param_roi_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_roi_t *p_current = (fsm_param_roi_t *)output;
        p_current->roi_api = p_fsm->roi_api;
        p_current->roi = p_fsm->roi;
        break;
    }

    case FSM_PARAM_GET_AF_LENS_REG:
        if ( !input || input_size != sizeof( uint32_t ) ||
             !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->lens_ctrl.read_lens_register( p_fsm->lens_ctx, *(uint32_t *)input );
        break;

    case FSM_PARAM_GET_AF_LENS_STATUS:
        if ( !output || output_size != sizeof( int32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(int32_t *)output = p_fsm->lens_driver_ok;
        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}


uint8_t AF_fsm_process_event( AF_fsm_t *p_fsm, event_id_t event_id )
{
    return 0;
}
