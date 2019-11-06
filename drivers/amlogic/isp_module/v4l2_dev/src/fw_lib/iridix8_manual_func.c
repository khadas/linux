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
#include "acamera_math.h"

#include "acamera_command_api.h"
#if IRIDIX_HAS_PRE_POST_GAMMA_LUT_LINEAR
#include "acamera_iridix_fwd_mem_config.h"
#include "acamera_iridix_rev_mem_config.h"
#endif

#include "sbuf.h"
#include "iridix8_manual_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_IRIDIX8_MANUAL
#endif

/* this function just get a empty sbuf and set its status to done
 * so that it keeps the sbuf status loop correctly.
 */
static void iridix_update_sbuf( iridix_fsm_const_ptr_t p_fsm )
{
    int i;
    struct sbuf_item sbuf;
    int fw_id = p_fsm->cmn.ctx_id;

    // try to set 2 iridix sbuf to match the ae sbuf speed in WDR mode.
    for ( i = 0; i < 2; i++ ) {
        memset( &sbuf, 0, sizeof( sbuf ) );
        sbuf.buf_type = SBUF_TYPE_IRIDIX;
        sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;

        if ( sbuf_get_item( fw_id, &sbuf ) ) {
            LOG( LOG_INFO, "No empty iridix sbuf available, return." );
            break;
        }

        /* We don't have input data to user-FW, so just set the status */
        sbuf.buf_status = SBUF_STATUS_DATA_DONE;

        if ( sbuf_set_item( fw_id, &sbuf ) ) {
            LOG( LOG_ERR, "Error: Failed to set sbuf, return." );
            break;
        }
    }
}

static void iridix_mointor_frame_end( iridix_fsm_ptr_t p_fsm )
{

    uint32_t irq_mask = acamera_isp_isp_global_interrupt_status_vector_read( p_fsm->cmn.isp_base );
    if ( irq_mask & 0x1 ) {
        fsm_param_mon_err_head_t mon_err;
        mon_err.err_type = MON_TYPE_ERR_IRIDIX_UPDATE_NOT_IN_VB;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_ERROR_REPORT, &mon_err, sizeof( mon_err ) );
    }
}

void iridix_fsm_process_interrupt( iridix_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    // Get and process interrupts
    if ( irq_event == ACAMERA_IRQ_FRAME_END ) {
        int32_t diff = 256;

        iridix_update_sbuf( p_fsm );

        uint8_t iridix_avg_coeff = _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_AVG_COEF )[0];

        int32_t diff_iridix_DG = 256;
        uint16_t iridix_global_DG = p_fsm->iridix_global_DG;

        if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_iridix == 0 ) {
            acamera_isp_iridix_gain_gain_write( p_fsm->cmn.isp_base, iridix_global_DG );
        }
        diff_iridix_DG = ( ( (int32_t)iridix_global_DG ) << 8 ) / ( int32_t )( p_fsm->iridix_global_DG_prev );
        ( (iridix_fsm_ptr_t)p_fsm )->iridix_global_DG_prev = iridix_global_DG; // already applyied gain

        exposure_set_t exp_set_fr_next;
        exposure_set_t exp_set_fr_prev;
        int32_t frame_next = 1, frame_prev = 0;
#if ISP_HAS_TEMPER == 3
        if ( !acamera_isp_temper_temper2_mode_read( p_fsm->cmn.isp_base ) ) { // temper 3 has 1 frame delay
            frame_next = 0;
            frame_prev = -1;
        }
#endif

        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_FRAME_EXPOSURE_SET, &frame_next, sizeof( frame_next ), &exp_set_fr_next, sizeof( exp_set_fr_next ) );
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_FRAME_EXPOSURE_SET, &frame_prev, sizeof( frame_prev ), &exp_set_fr_prev, sizeof( exp_set_fr_prev ) );

        diff = acamera_math_exp2( exp_set_fr_prev.info.exposure_log2 - exp_set_fr_next.info.exposure_log2, LOG2_GAIN_SHIFT, 8 );
        // LOG(LOG_DEBUG,"diff1 %d %d ", (int)diff,(int)diff_iridix_DG);

        diff = ( diff * diff_iridix_DG ) >> 8;

        // LOG(LOG_DEBUG,"diff2 %d\n", (int)diff);
        if ( diff < 0 )
            diff = 256;
        if ( diff >= ( 1 << ACAMERA_ISP_IRIDIX_COLLECTION_CORRECTION_DATASIZE ) )
            diff = ( 1 << ACAMERA_ISP_IRIDIX_COLLECTION_CORRECTION_DATASIZE );
        //LOG(LOG_DEBUG,"%u %ld prev %ld\n", (unsigned int)diff, exp_set_fr1.info.exposure_log2, exp_set_fr0.info.exposure_log2);
        // LOG(LOG_DEBUG,"diff3 %d\n", (int)diff);

        acamera_isp_iridix_collection_correction_write( p_fsm->cmn.isp_base, diff );

        diff = 256; // this logic diff = 256 where strength is only updated creates a long delay.
        if ( diff == 256 ) {
            // time filter for iridix strength
            uint16_t iridix_strength = p_fsm->strength_target;
            if ( iridix_avg_coeff > 1 ) {
                ( (iridix_fsm_ptr_t)p_fsm )->strength_avg += p_fsm->strength_target - p_fsm->strength_avg / iridix_avg_coeff; // division by zero is checked
                iridix_strength = ( uint16_t )( p_fsm->strength_avg / iridix_avg_coeff );                                     // division by zero is checked
            }
            if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_iridix == 0 ) {
                acamera_isp_iridix_strength_inroi_write( p_fsm->cmn.isp_base, iridix_strength >> 6 );
            }

            if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_iridix == 0 ) {
                acamera_isp_iridix_dark_enh_write( p_fsm->cmn.isp_base, p_fsm->dark_enh );
            }
        }

        if ( ( p_fsm->frame_id_tracking != p_fsm->pre_frame_id_tracking ) && p_fsm->is_output_ready ) {
            fsm_param_mon_alg_flow_t iridix_flow;

            ( (iridix_fsm_ptr_t)p_fsm )->pre_frame_id_tracking = p_fsm->frame_id_tracking;

            iridix_flow.frame_id_tracking = p_fsm->frame_id_tracking;
            iridix_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &( (iridix_fsm_t *)p_fsm )->cmn );
            iridix_flow.flow_state = MON_ALG_FLOW_STATE_APPLIED;
            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_IRIDIX_FLOW, &iridix_flow, sizeof( iridix_flow ) );
            LOG( LOG_INFO, "Iridix8 flow: APPLIED: frame_id_tracking: %d, cur frame_id: %u.", iridix_flow.frame_id_tracking, iridix_flow.frame_id_current );
            ( (iridix_fsm_ptr_t)p_fsm )->frame_id_tracking = 0;
        }

        iridix_mointor_frame_end( (iridix_fsm_ptr_t)p_fsm );
    }
}


void iridix_initialize( iridix_fsm_t *p_fsm )
{
    uint16_t i;
    //put initialization here
    p_fsm->strength_avg = IRIDIX_STRENGTH_TARGET * _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_AVG_COEF )[0];

    // Initialize parameters

    if ( _GET_WIDTH( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_ASYMMETRY ) == sizeof( int32_t ) ) {
        // 32 bit tables
        for ( i = 0; i < _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_ASYMMETRY ); i++ ) {
            uint32_t val = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_ASYMMETRY )[i];
            acamera_isp_iridix_lut_asymmetry_lut_write( p_fsm->cmn.isp_base, i, val );
        }
    } else {
        // 16 bit tables
        for ( i = 0; i < _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_ASYMMETRY ); i++ ) {
            acamera_isp_iridix_lut_asymmetry_lut_write( p_fsm->cmn.isp_base, i, _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_ASYMMETRY )[i] );
        }
    }

    ACAMERA_FSM2CTX_PTR( p_fsm )
        ->stab.global_iridix_strength_target = ( p_fsm->strength_target );
#if ISP_WDR_SWITCH
    uint32_t wdr_mode = 0;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );

    if ( wdr_mode == WDR_MODE_LINEAR ) {
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_maximum_iridix_strength = ( _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_STRENGTH_MAXIMUM )[0] );

    } else if ( wdr_mode == WDR_MODE_FS_LIN ) {
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_maximum_iridix_strength = ( _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_STRENGTH_MAXIMUM )[0] );

    } else
#endif
    {
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_maximum_iridix_strength = ( _GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_IRIDIX_STRENGTH_MAXIMUM )[0] );
    }

    p_fsm->mask.repeat_irq_mask = ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_END );
    iridix_request_interrupt( p_fsm, p_fsm->mask.repeat_irq_mask );
}

int iridix_set_tracking_frame_id( iridix_fsm_ptr_t p_fsm, uint32_t frame_id )
{
    // check whether previous frame_id_tracking finished.
    if ( p_fsm->frame_id_tracking ) {
        LOG( LOG_INFO, "Iridix flow: Overwrite: prev frame_id_tracking: %d, new: %u.", p_fsm->frame_id_tracking, frame_id );
    }

    p_fsm->frame_id_tracking = frame_id;
    p_fsm->is_output_ready = 0;

    fsm_param_mon_alg_flow_t iridix_flow;

    iridix_flow.frame_id_tracking = p_fsm->frame_id_tracking;
    iridix_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    iridix_flow.flow_state = MON_ALG_FLOW_STATE_INPUT_READY;
    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_IRIDIX_FLOW, &iridix_flow, sizeof( iridix_flow ) );
    LOG( LOG_INFO, "Iridix8 flow: INPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", iridix_flow.frame_id_tracking, iridix_flow.frame_id_current );

    return 0;
}

void iridix_set_new_param( iridix_fsm_ptr_t p_fsm, sbuf_iridix_t *p_sbuf_iridix )
{
    p_fsm->strength_target = p_sbuf_iridix->strength_target;
    p_fsm->dark_enh = p_sbuf_iridix->iridix_dark_enh;
    p_fsm->iridix_contrast = p_sbuf_iridix->iridix_contrast;
    p_fsm->iridix_global_DG = p_sbuf_iridix->iridix_global_DG;

    if ( p_fsm->frame_id_tracking != p_sbuf_iridix->frame_id ) {
        LOG( LOG_CRIT, "Error: Frame_id is not match: iridix frame_id_tracking: %d, sbuf_iridix->frame_id: %d.", p_fsm->frame_id_tracking, p_sbuf_iridix->frame_id );
    }

    fsm_param_mon_alg_flow_t iridix_flow;

    iridix_flow.frame_id_tracking = p_sbuf_iridix->frame_id;
    iridix_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    iridix_flow.flow_state = MON_ALG_FLOW_STATE_OUTPUT_READY;
    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_IRIDIX_FLOW, &iridix_flow, sizeof( iridix_flow ) );
    LOG( LOG_INFO, "Iridix8 flow: OUTPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", iridix_flow.frame_id_tracking, iridix_flow.frame_id_current );

    p_fsm->is_output_ready = 1;
}
