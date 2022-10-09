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
#include "acamera_math.h"
#include "acamera_logger.h"
#include "system_stdlib.h"
#include "acamera_ihist_stats_mem_config.h"
#include "acamera_command_api.h"
#include "gamma_manual_fsm.h"
#include "sbuf.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_GAMMA_MANUAL
#endif

#ifndef ARR_SIZE
#define ARR_SIZE( x ) ( sizeof( x ) / sizeof( x[0] ) )
#endif

void gamma_manual_read_histogram( gamma_manual_fsm_ptr_t p_fsm )
{
    int i;
    uint32_t sum = 0;
    sbuf_gamma_t *p_sbuf_gamma;
    struct sbuf_item sbuf;
    int fw_id = p_fsm->cmn.ctx_id;

    memset( &sbuf, 0, sizeof( sbuf ) );
    sbuf.buf_type = SBUF_TYPE_GAMMA;
    sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;

    if ( sbuf_get_item( fw_id, &sbuf ) ) {
        LOG( LOG_ERR, "Error: Failed to get sbuf, return." );
        return;
    }

    p_sbuf_gamma = (sbuf_gamma_t *)sbuf.buf_base;
    LOG( LOG_DEBUG, "Get sbuf gamma ok, idx: %u, status: %u, addr: %p.", sbuf.buf_idx, sbuf.buf_status, sbuf.buf_base );

    fsm_param_mon_alg_flow_t gamma_flow;

    gamma_flow.frame_id_tracking = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    p_sbuf_gamma->frame_id = gamma_flow.frame_id_tracking;


    for ( i = 0; i < ISP_FULL_HISTOGRAM_SIZE; ++i ) {
        uint32_t v = acamera_ihist_stats_mem_array_data_read( p_fsm->cmn.isp_base, i );

        v <<= 8;

        p_sbuf_gamma->stats_data[i] = v;
        sum += v;
    }
    p_fsm->fullhist_sum = sum;

    // at last store the gamma_hist_sum
    p_sbuf_gamma->fullhist_sum = sum;
    LOG( LOG_DEBUG, "gamma histsum: fullhist_sum: %u.", p_sbuf_gamma->fullhist_sum );

    /* read done, set the buffer back for future using  */
    sbuf.buf_status = SBUF_STATUS_DATA_DONE;

    if ( sbuf_set_item( fw_id, &sbuf ) ) {
        LOG( LOG_ERR, "Error: Failed to set sbuf, return." );
        return;
    }

    LOG( LOG_DEBUG, "Set sbuf gamma ok, idx: %u, status: %u, addr: %p.", sbuf.buf_idx, sbuf.buf_status, sbuf.buf_base );

    gamma_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    gamma_flow.flow_state = MON_ALG_FLOW_STATE_INPUT_READY;
    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_GAMMA_FLOW, &gamma_flow, sizeof( gamma_flow ) );
}


void gamma_manual_init( gamma_manual_fsm_ptr_t p_fsm )
{
    // request neccessary interrupts here
    p_fsm->mask.repeat_irq_mask = ACAMERA_IRQ_MASK( ACAMERA_IRQ_ANTIFOG_HIST );
    gamma_manual_request_interrupt( p_fsm, ACAMERA_IRQ_MASK( ACAMERA_IRQ_ANTIFOG_HIST ) );
}

void gamma_manual_update( gamma_manual_fsm_ptr_t p_fsm )
{
    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_auto_level == 0 ) {

        acamera_isp_fr_gamma_rgb_gain_r_write( p_fsm->cmn.isp_base, p_fsm->gamma_gain );
        acamera_isp_fr_gamma_rgb_gain_g_write( p_fsm->cmn.isp_base, p_fsm->gamma_gain );
        acamera_isp_fr_gamma_rgb_gain_b_write( p_fsm->cmn.isp_base, p_fsm->gamma_gain );

        acamera_isp_fr_gamma_rgb_offset_r_write( p_fsm->cmn.isp_base, p_fsm->gamma_offset );
        acamera_isp_fr_gamma_rgb_offset_g_write( p_fsm->cmn.isp_base, p_fsm->gamma_offset );
        acamera_isp_fr_gamma_rgb_offset_b_write( p_fsm->cmn.isp_base, p_fsm->gamma_offset );


#if ISP_HAS_DS1
        acamera_isp_ds1_gamma_rgb_gain_r_write( p_fsm->cmn.isp_base, p_fsm->gamma_gain );
        acamera_isp_ds1_gamma_rgb_gain_g_write( p_fsm->cmn.isp_base, p_fsm->gamma_gain );
        acamera_isp_ds1_gamma_rgb_gain_b_write( p_fsm->cmn.isp_base, p_fsm->gamma_gain );

        acamera_isp_ds1_gamma_rgb_offset_r_write( p_fsm->cmn.isp_base, p_fsm->gamma_offset );
        acamera_isp_ds1_gamma_rgb_offset_g_write( p_fsm->cmn.isp_base, p_fsm->gamma_offset );
        acamera_isp_ds1_gamma_rgb_offset_b_write( p_fsm->cmn.isp_base, p_fsm->gamma_offset );
#endif
    }

    // skip when frame_id is 0.
    if ( p_fsm->cur_frame_id_tracking ) {
        fsm_param_mon_alg_flow_t gamma_flow;

        gamma_flow.frame_id_tracking = p_fsm->cur_frame_id_tracking;
        gamma_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
        gamma_flow.flow_state = MON_ALG_FLOW_STATE_APPLIED;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_GAMMA_FLOW, &gamma_flow, sizeof( gamma_flow ) );
    }
}

void gamma_manual_fsm_process_interrupt( gamma_manual_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    if ( irq_event == ACAMERA_IRQ_ANTIFOG_HIST ) {
        gamma_manual_read_histogram( (gamma_manual_fsm_ptr_t)p_fsm );
        fsm_raise_event( p_fsm, event_id_gamma_stats_ready );
    }
    return;
}

void gamma_manual_set_new_param( gamma_manual_fsm_ptr_t p_fsm, sbuf_gamma_t *p_sbuf_gamma )
{
    p_fsm->gamma_gain = p_sbuf_gamma->gamma_gain;
    p_fsm->gamma_offset = p_sbuf_gamma->gamma_offset;
    p_fsm->cur_frame_id_tracking = p_sbuf_gamma->frame_id;

    if ( p_fsm->cur_frame_id_tracking ) {
        fsm_param_mon_alg_flow_t gamma_flow;

        gamma_flow.frame_id_tracking = p_fsm->cur_frame_id_tracking;
        gamma_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
        gamma_flow.flow_state = MON_ALG_FLOW_STATE_OUTPUT_READY;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_GAMMA_FLOW, &gamma_flow, sizeof( gamma_flow ) );
    }

    fsm_raise_event( p_fsm, event_id_gamma_new_param_ready );
}
