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
#include "monitor_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_MONITOR
#endif

// Note: this name order should match the order of MON_ALG_INDEX
static char *mon_alg_name[MON_ALG_INDEX_MAX] = {
    "AE",
    "AWB",
    "GAMMA",
    "IRIDIX",
};

void monitor_fsm_clear( monitor_fsm_t *p_fsm )
{
    uint32_t i;
    mon_alg_info_t *p_mon_info = NULL;

    memset( &p_fsm->mon_info_cali, 0, sizeof( p_fsm->mon_info_cali ) );
    memset( &p_fsm->mon_info_cmos, 0, sizeof( p_fsm->mon_info_cmos ) );
    memset( &p_fsm->mon_info_iridix, 0, sizeof( p_fsm->mon_info_iridix ) );

    for ( i = 0; i < MON_ALG_INDEX_MAX; i++ ) {
        p_mon_info = &p_fsm->mon_alg_arr[i];
        p_mon_info->alg_name = mon_alg_name[i];

        // Init to 0xFFFFFFFF which is an invalid value
        p_mon_info->alg_delay_in2out_min = 0xFFFFFFFF;
        p_mon_info->alg_delay_in2out_cur = 0xFFFFFFFF;
        p_mon_info->alg_delay_in2out_max = 0xFFFFFFFF;
        p_mon_info->alg_delay_out2apply_min = 0xFFFFFFFF;
        p_mon_info->alg_delay_out2apply_cur = 0xFFFFFFFF;
        p_mon_info->alg_delay_out2apply_max = 0xFFFFFFFF;
        p_mon_info->alg_fpt_min = 0xFFFF;
        p_mon_info->alg_fpt_cur = 0;
        p_mon_info->alg_fpt_max = 0;
        p_mon_info->mon_alg_frame_count = 0;
        memset( p_mon_info->alg_state_arr, 0, sizeof( p_mon_info->alg_state_arr ) );
        p_mon_info->alg_arr_write_pos = 0;

        p_mon_info->alg_reset_status = 0;
    }
}

void monitor_request_interrupt( monitor_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void monitor_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    monitor_fsm_t *p_fsm = (monitor_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    monitor_fsm_clear( p_fsm );

    monitor_initialize( p_fsm );
}

uint8_t monitor_fsm_process_event( monitor_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;
    case event_id_monitor_frame_end:

        b_event_processed = 1;
    }

    return b_event_processed;
}

int monitor_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    monitor_fsm_t *p_fsm = (monitor_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_MON_ERROR_REPORT: {
        if ( !input || input_size < sizeof( fsm_param_mon_err_head_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_handle_error_report( p_fsm, (fsm_param_mon_err_head_t *)input );

        break;
    }

    case FSM_PARAM_SET_MON_RESET_ERROR: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint32_t err_type = *(uint32_t *)input;

        monitor_handle_reset_error_report( p_fsm, err_type );

        break;
    }

    case FSM_PARAM_SET_MON_AE_FLOW:
        if ( !input || input_size != sizeof( fsm_param_mon_alg_flow_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_handle_alg_flow( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_AE_INDEX], (fsm_param_mon_alg_flow_t *)input );
        break;

    case FSM_PARAM_SET_MON_AWB_FLOW:
        if ( !input || input_size != sizeof( fsm_param_mon_alg_flow_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_handle_alg_flow( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_AWB_INDEX], (fsm_param_mon_alg_flow_t *)input );
        break;

    case FSM_PARAM_SET_MON_GAMMA_FLOW:
        if ( !input || input_size != sizeof( fsm_param_mon_alg_flow_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_handle_alg_flow( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_GAMMA_INDEX], (fsm_param_mon_alg_flow_t *)input );
        break;

    case FSM_PARAM_SET_MON_IRIDIX_FLOW:
        if ( !input || input_size != sizeof( fsm_param_mon_alg_flow_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_handle_alg_flow( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_IRIDIX_INDEX], (fsm_param_mon_alg_flow_t *)input );
        break;

    case FSM_PARAM_SET_MON_STATUS_AE:
        if ( !input || input_size != sizeof( fsm_param_mon_status_head_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_set_alg_status( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_AE_INDEX], (fsm_param_mon_status_head_t *)input );
        break;

    case FSM_PARAM_SET_MON_STATUS_AWB:
        if ( !input || input_size != sizeof( fsm_param_mon_status_head_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_set_alg_status( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_AWB_INDEX], (fsm_param_mon_status_head_t *)input );
        break;

    case FSM_PARAM_SET_MON_STATUS_GAMMA:
        if ( !input || input_size != sizeof( fsm_param_mon_status_head_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_set_alg_status( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_GAMMA_INDEX], (fsm_param_mon_status_head_t *)input );
        break;

    case FSM_PARAM_SET_MON_STATUS_IRIDIX:
        if ( !input || input_size != sizeof( fsm_param_mon_status_head_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_set_alg_status( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_IRIDIX_INDEX], (fsm_param_mon_status_head_t *)input );
        break;

    default:
        LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
        rc = -1;
        break;
    }

    return rc;
}

int monitor_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    monitor_fsm_t *p_fsm = (monitor_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_MON_ERROR: {
        if ( !input || input_size != sizeof( uint32_t ) ||
             !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_get_error_report( p_fsm, *(uint32_t *)input, (uint32_t *)output );
        break;
    }

    case FSM_PARAM_GET_MON_STATUS_AE: {
        if ( !input || input_size != sizeof( uint32_t ) ||
             !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_get_alg_status( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_AE_INDEX], *(uint32_t *)input, (uint32_t *)output );
        break;
    }

    case FSM_PARAM_GET_MON_STATUS_AWB: {
        if ( !input || input_size != sizeof( uint32_t ) ||
             !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_get_alg_status( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_AWB_INDEX], *(uint32_t *)input, (uint32_t *)output );
        break;
    }

    case FSM_PARAM_GET_MON_STATUS_GAMMA: {
        if ( !input || input_size != sizeof( uint32_t ) ||
             !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_get_alg_status( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_GAMMA_INDEX], *(uint32_t *)input, (uint32_t *)output );
        break;
    }

    case FSM_PARAM_GET_MON_STATUS_IRIDIX: {
        if ( !input || input_size != sizeof( uint32_t ) ||
             !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        monitor_get_alg_status( p_fsm, &p_fsm->mon_alg_arr[MON_ALG_IRIDIX_INDEX], *(uint32_t *)input, (uint32_t *)output );
        break;
    }

    default:
        LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
        rc = -1;
        break;
    }

    return rc;
}

void monitor_fsm_process_interrupt( monitor_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    switch ( irq_event ) {
    case ACAMERA_IRQ_FRAME_END:
        fsm_raise_event( p_fsm, event_id_monitor_frame_end );
        break;
    }
}