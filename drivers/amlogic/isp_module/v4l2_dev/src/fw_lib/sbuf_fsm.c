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
#include "sbuf_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_SBUF
#endif

void sbuf_fsm_clear( sbuf_fsm_t *p_fsm )
{
    p_fsm->opened = 0;
    p_fsm->mode = 0;
}

void sbuf_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    sbuf_fsm_t *p_fsm = (sbuf_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    sbuf_fsm_clear( p_fsm );

    sbuf_fsm_initialize( p_fsm );
}


int sbuf_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;

    sbuf_fsm_t *p_fsm = (sbuf_fsm_t *)fsm;
    switch ( param_id ) {
    case FSM_PARAM_SET_SBUF_CALIBRATION_UPDATE: {
        sbuf_update_calibration_data( p_fsm );
        break;
    }
    default:
        rc = -1;
        break;
    }

    return rc;
}

uint8_t sbuf_fsm_process_event( sbuf_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;

    case event_id_ae_stats_ready:
        sbuf_update_ae_idx( p_fsm );
        b_event_processed = 1;
        break;

    case event_id_gamma_stats_ready:
        sbuf_update_gamma_idx( p_fsm );
        b_event_processed = 1;
        break;

    case event_id_awb_stats_ready:
        sbuf_update_awb_idx( p_fsm );
        b_event_processed = 1;
        break;

    case event_id_af_stats_ready:
        sbuf_update_af_idx( p_fsm );
        b_event_processed = 1;
        break;
    }

    return b_event_processed;
}
