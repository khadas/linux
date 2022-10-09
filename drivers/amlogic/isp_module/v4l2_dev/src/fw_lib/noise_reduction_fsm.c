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
#include "noise_reduction_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_NOISE_REDUCTION
#endif

void noise_reduction_fsm_clear( noise_reduction_fsm_t *p_fsm )
{
    p_fsm->temper_ev_previous_frame = 0;
    p_fsm->temper_diff_avg = 0;
    p_fsm->temper_diff_coeff = 10;
    p_fsm->snr_thresh_contrast = 0;
    p_fsm->temper_md_enable = 0;
    p_fsm->temper_md_level = 0;
    p_fsm->temper_md_mode = 0;
    p_fsm->temper_md_thrd1 = 0;
    p_fsm->temper_md_thrd2 = 0;
    p_fsm->temper_md_1bm_thrd = 0;
    p_fsm->temper_md_sad_thrd = 0;
    p_fsm->temper_md_1bm_cur = 0;
    p_fsm->temper_md_sad_cur = 0;
    p_fsm->temper_md_gain_thrd = 0;
    p_fsm->temper_md_log = 0;
    p_fsm->temper_md_mtn = 0;

    p_fsm->nr_mode = NOISE_REDUCTION_MODE_ON;
}

void noise_reduction_request_interrupt( noise_reduction_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void noise_reduction_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    noise_reduction_fsm_t *p_fsm = (noise_reduction_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    noise_reduction_fsm_clear( p_fsm );

    noise_reduction_initialize( p_fsm );
    noise_reduction_hw_init( p_fsm );
}

uint8_t noise_reduction_fsm_process_event( noise_reduction_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;
    case event_id_frame_end:
        noise_reduction_update( p_fsm );
        noise_reduction_initialize( p_fsm );
        b_event_processed = 1;
        break;
    }

    return b_event_processed;
}

int noise_reduction_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    noise_reduction_fsm_t *p_fsm = (noise_reduction_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_NOISE_REDUCTION_MODE: {
        if ( !input || input_size != sizeof( noise_reduction_mode_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->nr_mode = *(noise_reduction_mode_t *)input;

        break;
    }
    case FSM_PARAM_SET_SNR_MANUAL: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint32_t global = *(uint32_t *)input;

        ACAMERA_MGR2CTX_PTR( p_fsm->p_fsm_mgr )->stab.global_manual_sinter = global;

        break;
    }
    case FSM_PARAM_SET_SNR_STRENGTH: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint8_t strength = *(uint32_t *)input;

        acamera_isp_sinter_strength_1_write( p_fsm->p_fsm_mgr->isp_base, strength );

        break;
    }
    case FSM_PARAM_SET_TNR_MANUAL: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint32_t global = *(uint32_t *)input;

        ACAMERA_MGR2CTX_PTR( p_fsm->p_fsm_mgr )->stab.global_manual_temper = global;

        break;
    }
    case FSM_PARAM_SET_TNR_OFFSET: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint8_t offset = *(uint32_t *)input;

        acamera_isp_temper_noise_profile_global_offset_write( p_fsm->p_fsm_mgr->isp_base, offset );

        break;
    }
    case FSM_PARAM_SET_TNR_MD_LEVEL: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->temper_md_level = *(uint32_t *)input;

        break;
    }
    case FSM_PARAM_SET_TNR_MD_MTN: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->temper_md_mtn = *(uint32_t *)input;

        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}

int noise_reduction_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    noise_reduction_fsm_t *p_fsm = (noise_reduction_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_NOISE_REDUCTION_MODE: {

        if ( !output || output_size != sizeof( noise_reduction_mode_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(noise_reduction_mode_t *)output = p_fsm->nr_mode;

        break;
    }
    case FSM_PARAM_GET_SNR_MANUAL: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = ACAMERA_MGR2CTX_PTR( p_fsm->p_fsm_mgr )->stab.global_manual_sinter;

        break;
    }
    case FSM_PARAM_GET_SNR_STRENGTH: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = acamera_isp_sinter_strength_1_read( p_fsm->p_fsm_mgr->isp_base );

        break;
    }
    case FSM_PARAM_GET_TNR_MANUAL: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = ACAMERA_MGR2CTX_PTR( p_fsm->p_fsm_mgr )->stab.global_manual_temper;

        break;
    }
    case FSM_PARAM_GET_TNR_OFFSET: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = acamera_isp_temper_noise_profile_global_offset_read( p_fsm->p_fsm_mgr->isp_base );

        break;
    }
    case FSM_PARAM_GET_TNR_MD_MODE: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->temper_md_mode;

        break;
    }
    case FSM_PARAM_GET_TNR_MD_THRD1: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->temper_md_thrd1;

        break;
    }
    case FSM_PARAM_GET_TNR_MD_THRD2: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->temper_md_thrd2;

        break;
    }

    case FSM_PARAM_GET_TNR_MD_LOG: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->temper_md_log;

        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}
