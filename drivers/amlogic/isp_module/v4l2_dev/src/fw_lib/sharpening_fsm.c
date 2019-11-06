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
#include "sharpening_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_SHARPENING
#endif

void sharpening_fsm_clear( sharpening_fsm_t *p_fsm )
{
    p_fsm->sharpening_mult = 128;
    p_fsm->api_value = 128;
}

void sharpening_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    sharpening_fsm_t *p_fsm = (sharpening_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    sharpening_fsm_clear( p_fsm );

    sharpening_initialize( p_fsm );
    fsm_raise_event( p_fsm, event_id_sharp_lut_update );
}


int sharpening_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    sharpening_fsm_t *p_fsm = (sharpening_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_SHARPENING_MULT: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->sharpening_mult = *(uint32_t *)input;
        break;
    }

    case FSM_PARAM_SET_SHARPENING_STRENGTH: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->api_value = *(uint32_t *)input;

        if ( p_fsm->api_value <= 128 ) {
            p_fsm->sharpening_mult = p_fsm->api_value;
        } else {
            uint32_t store;
            // 4th power seems to be a good fit for this
            // as alt_d/ud are O(10) and max is 255
            store = ( p_fsm->api_value * p_fsm->api_value * p_fsm->api_value * p_fsm->api_value );
            store /= 0x200000;
            p_fsm->sharpening_mult = store;
        }

        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}


int sharpening_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    sharpening_fsm_t *p_fsm = (sharpening_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_SHARPENING_STRENGTH: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->api_value;

        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}


uint8_t sharpening_fsm_process_event( sharpening_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;

    case event_id_frame_end:
        sharpening_update( p_fsm );
        b_event_processed = 1;
        break;

    case event_id_update_sharp_lut:

        break;
    }

    return b_event_processed;
}
