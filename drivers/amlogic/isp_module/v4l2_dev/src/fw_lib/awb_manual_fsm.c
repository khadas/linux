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
#include "awb_manual_fsm.h"
#include "sbuf.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_AWB_MANUAL
#endif

extern void awb_set_new_param( AWB_fsm_ptr_t p_fsm, sbuf_awb_t *p_sbuf_awb );

void AWB_fsm_clear( AWB_fsm_t *p_fsm )
{
    p_fsm->curr_AWB_ZONES = MAX_AWB_ZONES;
    p_fsm->awb_enabled = 0;
    p_fsm->p_high = 50;
    p_fsm->high_temp = 50;
    p_fsm->low_temp = 50;
    p_fsm->avg_GR = 128;
    p_fsm->avg_GB = 128;
    p_fsm->stable_avg_RG = D50_DEFAULT;
    p_fsm->stable_avg_BG = D50_DEFAULT;
    p_fsm->temperature_detected = 5000;
    p_fsm->light_source_detected = AWB_LIGHT_SOURCE_D50;
    p_fsm->light_source_candidate = AWB_LIGHT_SOURCE_D50;
    p_fsm->detect_light_source_frames_count = 0;
    p_fsm->max_temp = 10000;
    p_fsm->min_temp = 2100;
    p_fsm->max_temp_rg = 256;
    p_fsm->max_temp_bg = 256;
    p_fsm->min_temp_rg = 256;
    p_fsm->min_temp_bg = 256;
    p_fsm->roi = 65535;
    p_fsm->switch_light_source_detect_frames_quantity = AWB_DLS_SWITCH_LIGHT_SOURCE_DETECT_FRAMES_QUANTITY;
    p_fsm->switch_light_source_change_frames_quantity = AWB_DLS_SWITCH_LIGHT_SOURCE_CHANGE_FRAMES_QUANTITY;

    p_fsm->cur_result_gain_frame_id = 0;
    p_fsm->pre_result_gain_frame_id = 0;
}

void AWB_request_interrupt( AWB_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}


#if AWB_MODE_ID
static uint32_t get_awb_idx( uint32_t value )
{
    uint32_t res = 0;
    // acamera awb scenes are always located in a certain order
    // this order is
    // {AWB_DAY_LIGHT}
    // {AWB_CLOUDY}
    // {AWB_INCANDESCENT}
    // {AWB_FLOURESCENT}
    // {AWB_TWILIGHT}
    // {AWB_SHADE}
    // {AWB_WARM_FLOURESCENT}
    switch ( value ) {
    case AWB_DAY_LIGHT:
        res = 0;
        break;
    case AWB_CLOUDY:
        res = 1;
        break;
    case AWB_INCANDESCENT:
        res = 2;
        break;
    case AWB_FLOURESCENT:
        res = 3;
        break;
    case AWB_TWILIGHT:
        res = 4;
        break;
    case AWB_SHADE:
        res = 5;
        break;
    case AWB_WARM_FLOURESCENT:
        res = 6;
        break;
    default:
        LOG( LOG_ERR, "Invalid AWB scene index. Please check your calibrations" );
        break;
    }
    return res;
}
#endif

static int AWB_set_mode( AWB_fsm_ptr_t p_fsm, uint32_t mode )
{
    int rc = 0;

#if AWB_MODE_ID
    switch ( mode ) {
    case AWB_MANUAL:
        p_fsm->awb_enabled = 0;
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_manual_awb = ( 1 );
        p_fsm->mode = mode;
        break;

    case AWB_AUTO:
        p_fsm->awb_enabled = 1;
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_manual_awb = ( 0 );
        p_fsm->mode = mode;
        break;

    case AWB_DAY_LIGHT:
    case AWB_CLOUDY:
    case AWB_INCANDESCENT:
    case AWB_FLOURESCENT:
    case AWB_TWILIGHT:
    case AWB_SHADE:
    case AWB_WARM_FLOURESCENT: {
        modulation_entry_t *_calibration_awb_scene_presets = _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AWB_SCENE_PRESETS );
        uint32_t i = get_awb_idx( mode );
        p_fsm->awb_enabled = 0;
        p_fsm->mode = mode;
        // Then set the red and blue values
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_manual_awb = ( 1 );
        uint32_t cr = _calibration_awb_scene_presets[i].x;
        uint32_t cb = _calibration_awb_scene_presets[i].y;
        // Change 9 bit into 7 bit
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_awb_red_gain = cr;
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_awb_blue_gain = cb;
        break;
    }
    default:
        rc = -1;
        break;
    }
#endif

    return rc;
}

void AWB_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    AWB_fsm_t *p_fsm = (AWB_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    AWB_fsm_clear( p_fsm );

    awb_init( p_fsm );
    awb_coeffs_write( p_fsm );
    AWB_request_interrupt( p_fsm, ACAMERA_IRQ_MASK( ACAMERA_IRQ_AWB_STATS ) );
}

int AWB_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    AWB_fsm_t *p_fsm = (AWB_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_AWB_INFO: {
        if ( !output || output_size != sizeof( fsm_param_awb_info_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_awb_info_t *p_info = (fsm_param_awb_info_t *)output;

        system_memcpy( p_info->wb_log2, p_fsm->wb_log2, sizeof( p_info->wb_log2 ) );
        system_memcpy( p_info->awb_warming, p_fsm->awb_warming, sizeof( p_info->awb_warming ) );

        p_info->temperature_detected = p_fsm->temperature_detected;
        p_info->p_high = p_fsm->p_high;
        p_info->light_source_candidate = p_fsm->light_source_candidate;
        p_info->avg_GR = p_fsm->avg_GR;
        p_info->avg_GB = p_fsm->avg_GB;
        p_info->rg_coef = p_fsm->rg_coef;
        p_info->bg_coef = p_fsm->bg_coef;
        p_info->result_gain_frame_id = p_fsm->pre_result_gain_frame_id;

        break;
    }

    case FSM_PARAM_GET_AWB_MODE:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

#if AWB_MODE_ID
        if ( !ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_awb ) {
            *(uint32_t *)output = AWB_AUTO;
        } else {
            *(uint32_t *)output = p_fsm->mode;
        }
#endif

        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}

int AWB_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    AWB_fsm_t *p_fsm = (AWB_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_AWB_NEW_PARAM: {
        if ( !input || input_size != sizeof( sbuf_awb_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        awb_set_new_param( p_fsm, (sbuf_awb_t *)input );

        break;
    }

    case FSM_PARAM_SET_AWB_MODE:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        rc = AWB_set_mode( p_fsm, *(uint32_t *)input );

        break;

    case FSM_PARAM_SET_AWB_INFO: {
        if ( !input || input_size != sizeof( fsm_param_awb_info_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_awb_info_t *p_info = (fsm_param_awb_info_t *)input;

        if ( p_info->flag & AWB_INFO_TEMPERATURE_DETECTED ) {
            p_fsm->temperature_detected = p_info->temperature_detected;
        }

        if ( p_info->flag & AWB_INFO_P_HIGH ) {
            p_fsm->p_high = p_info->p_high;
        }

        if ( p_info->flag & AWB_INFO_LIGHT_SOURCE_CANDIDATE ) {
            p_fsm->light_source_candidate = p_info->light_source_candidate;
        }

        break;
    }

    case FSM_PARAM_SET_AWB_ZONE_WEIGHT: {
        if (!input || input_size == 0) {
            LOG(LOG_ERR, "Invalid param. param id: %d.", param_id);
            rc = -1;
            break;
        }

        unsigned char *u_awb_wg = input;

        rc = awb_set_zone_weight(p_fsm, u_awb_wg);
        if (rc != 0)
            LOG(LOG_ERR, "Failed to set ae zone weight");

        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}

uint8_t AWB_fsm_process_event( AWB_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;
    case event_id_frame_end:
        AWB_request_interrupt( p_fsm, ACAMERA_IRQ_MASK( ACAMERA_IRQ_AWB_STATS ) );
        b_event_processed = 1;

        break;

    case event_id_awb_stats_ready:
        AWB_request_interrupt( p_fsm, ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_END ) );
        b_event_processed = 1;

        break;

    case event_id_awb_result_ready:
        awb_update_ccm( p_fsm );
        awb_normalise( p_fsm );
        fsm_raise_event( p_fsm, event_id_WB_matrix_ready );
        b_event_processed = 1;
        break;
    }

    return b_event_processed;
}
