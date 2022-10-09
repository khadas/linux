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
#include "cmos_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_CMOS
#endif

void cmos_fsm_clear( cmos_fsm_t *p_fsm )
{
    p_fsm->manual_gain_mode = 0;
    p_fsm->manual_gain = 1;
    p_fsm->exposure = 256;
    p_fsm->exposure_log2 = 2 << LOG2_GAIN_SHIFT;
    p_fsm->max_exposure_log2 = 2 << LOG2_GAIN_SHIFT;
    p_fsm->exposure_hist_pos = -1;
    p_fsm->flicker_freq = 50 * 256;
    p_fsm->exposure_ratio_in = 64;
    p_fsm->integration_time_short = 0;
    p_fsm->integration_time_medium = 0;
    p_fsm->integration_time_long = 0;
    p_fsm->exposure_ratio = 64;
    p_fsm->log2_gain_avg = 0;
    p_fsm->isp_dgain_log2 = 0;
    p_fsm->target_gain_log2 = 0;
    p_fsm->strategy = 0;

    p_fsm->maximum_isp_digital_gain = ( ( ACAMERA_ISP_DIGITAL_GAIN_GAIN_DATASIZE - 8 ) << LOG2_GAIN_SHIFT ) + acamera_log2_fixed_to_fixed( ( 1 << ACAMERA_ISP_DIGITAL_GAIN_GAIN_DATASIZE ) - 1, ACAMERA_ISP_DIGITAL_GAIN_GAIN_DATASIZE, LOG2_GAIN_SHIFT );


    p_fsm->frame_id_tracking = 0;
    p_fsm->prev_ae_frame_id_tracking = 0;
    p_fsm->prev_awb_frame_id_tracking = 0;

    p_fsm->prev_fs_frame_id = 0;
    p_fsm->prev_dgain_frame_id = 0;
}

void cmos_request_interrupt( cmos_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void cmos_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    cmos_fsm_t *p_fsm = (cmos_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    cmos_fsm_clear( p_fsm );

    cmos_init( p_fsm );
}

int cmos_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    cmos_fsm_t *p_fsm = (cmos_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_EXPOSURE_TARGET: {
        fsm_param_exposure_target_t *p_exp_target = NULL;

        if ( ( !input || input_size != sizeof( fsm_param_exposure_target_t ) ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_exp_target = (fsm_param_exposure_target_t *)input;

        cmos_set_exposure_target( p_fsm, p_exp_target->exposure_log2, p_exp_target->exposure_ratio );

        /* check whether the application is succeed
         * just need to check exposure, the exp_ratio maybe adjusted by max_exp_ratio.
         */
        if ( p_fsm->exposure_log2 != p_exp_target->exposure_log2 ) {
            LOG( LOG_INFO, "new AE failed, expected: exp_log2: %d, cmos_values: exp_log2: %d.",
                 (int)p_exp_target->exposure_log2,
                 (int)p_fsm->exposure_log2 );

            rc = -2;
        } else {
            LOG( LOG_DEBUG, "new frame_id_tracking: %d.", p_exp_target->frame_id_tracking );
            p_fsm->frame_id_tracking = p_exp_target->frame_id_tracking;
        }

        break;
    }

    case FSM_PARAM_SET_AE_MODE: {
        if ( !input || input_size != sizeof( fsm_param_ae_mode_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_ae_mode_t *p_ae_mode = (fsm_param_ae_mode_t *)input;

        if ( p_ae_mode->flag & AE_MODE_BIT_MANUAL_INT_TIME ) {
            cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
            param->global_manual_integration_time = p_ae_mode->manual_integration_time;
        }

        if ( p_ae_mode->flag & AE_MODE_BIT_MANUAL_GAIN_MODE ) {
            p_fsm->manual_gain_mode = p_ae_mode->manual_gain_mode;
        }

        break;
    }

    case FSM_PARAM_SET_MANUAL_GAIN:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->manual_gain = *(uint32_t *)input;

        break;

    case FSM_PARAM_SET_CMOS_ADJUST_EXP: {
        int i;
        int32_t corr = 0;

        if ( !input || input_size != sizeof( int32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        corr = *(int32_t *)input;

        // adjust exposure
        for ( i = 0; i < array_size_s( p_fsm->exposure_hist ); i++ ) {
            exposure_info_set_t *info = &cmos_get_frame_exposure_set( p_fsm, i )->info;
            info->exposure_log2 += corr;
            if ( info->exposure_log2 < 0 )
                info->exposure_log2 = 0;
        }
        break;
    }

    case FSM_PARAM_SET_CMOS_SPLIT_STRATEGY: {
        uint32_t strategy;

        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        strategy = *(uint32_t *)input;

        switch ( strategy ) {
        case AE_SPLIT_BALANCED:
            p_fsm->strategy = AE_SPLIT_BALANCED;
            cmos_update_exposure_partitioning_lut( p_fsm );
            break;

        case AE_SPLIT_INTEGRATION_PRIORITY:
            p_fsm->strategy = AE_SPLIT_INTEGRATION_PRIORITY;
            cmos_update_exposure_partitioning_lut( p_fsm );
            break;
        default:
            rc = -2;
            LOG( LOG_WARNING, "Not supported strategy preset: %d.", strategy );
            return NOT_SUPPORTED;
        }
        break;
    }

    default:
        rc = -1;
        LOG( LOG_WARNING, "Not supported cmd id: %d.", param_id );
        break;
    }

    return rc;
}

int cmos_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    cmos_fsm_t *p_fsm = (cmos_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_CMOS_EXPOSURE_LOG2: {

        if ( ( !input || input_size != sizeof( int32_t ) ) ||
             ( !output || output_size != sizeof( int32_t ) ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        if ( CMOS_CURRENT_EXPOSURE_LOG2 == *(int32_t *)input ) {
            *(int32_t *)output = p_fsm->exposure_log2;
        } else if ( CMOS_MAX_EXPOSURE_LOG2 == *(int32_t *)input ) {
            *(int32_t *)output = p_fsm->max_exposure_log2;
        } else {
            LOG( LOG_ERR, "Param_id: %d, unsupported param: %d.", param_id, *(int32_t *)input );
            rc = -1;
        }

        break;
    }

    case FSM_PARAM_GET_CMOS_EXPOSURE_RATIO:
        if ( ( !output || output_size != sizeof( uint32_t ) ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->exposure_ratio;

        break;

    case FSM_PARAM_GET_FRAME_EXPOSURE_SET: {

        if ( ( !input || input_size != sizeof( int32_t ) ) ||
             ( !output || output_size != sizeof( exposure_set_t ) ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        exposure_set_t *p_set_get = NULL;
        exposure_set_t *p_set_out = (exposure_set_t *)output;

        p_set_get = cmos_get_frame_exposure_set( p_fsm, *( (int *)input ) );

        *p_set_out = *p_set_get;

        break;
    }

    case FSM_PARAM_GET_CMOS_TOTAL_GAIN:
        if ( ( !output || output_size != sizeof( int32_t ) ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

#if USER_MODULE
        rc = acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_TOTAL_GAIN_LOG2, NULL, 0, output, output_size );
#else
        *( (int32_t *)output ) = p_fsm->again_val_log2 +
                                 p_fsm->dgain_val_log2 +
                                 p_fsm->isp_dgain_log2;
#endif
        break;

    case FSM_PARAM_GET_FPS:
        if ( ( !output || output_size != sizeof( uint32_t ) ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *( (uint32_t *)output ) = cmos_get_fps( p_fsm );

        break;

    case FSM_PARAM_GET_AE_MODE: {
        if ( ( !output || output_size != sizeof( fsm_param_ae_mode_t ) ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_ae_mode_t *p_ae_mode = (fsm_param_ae_mode_t *)output;

        if ( p_ae_mode->flag & AE_MODE_BIT_MANUAL_INT_TIME ) {
            cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
            p_ae_mode->manual_integration_time = param->global_manual_integration_time;
        }

        if ( p_ae_mode->flag & AE_MODE_BIT_MANUAL_GAIN_MODE ) {
            p_ae_mode->manual_gain_mode = p_fsm->manual_gain_mode;
        }

        break;
    }

    case FSM_PARAM_GET_GAIN: {
        if ( !input || input_size != sizeof( fsm_param_gain_calc_param_t ) ||
             !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_gain_calc_param_t *p_param = (fsm_param_gain_calc_param_t *)input;

        *( (uint32_t *)output ) = acamera_math_exp2( p_fsm->again_val_log2 + p_fsm->dgain_val_log2 + p_fsm->isp_dgain_log2, p_param->shift_in, p_param->shift_out );

        break;
    }

    case FSM_PARAM_GET_CMOS_EXP_WRITE_SET:
        if ( ( !output || output_size != sizeof( exposure_data_set_t ) ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(exposure_data_set_t *)output = p_fsm->exp_write_set;

        break;

    case FSM_PARAM_GET_CMOS_SPLIT_STRATEGY:
        if ( ( !output || output_size != sizeof( uint32_t ) ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *( (uint32_t *)output ) = p_fsm->strategy;

        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}


uint8_t cmos_fsm_process_event( cmos_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;
    case event_id_sensor_ready:
        cmos_update_exposure_partitioning_lut( p_fsm );

        // Update gains
        cmos_inttime_update( p_fsm );
        cmos_antiflicker_update( p_fsm );
        cmos_long_exposure_update( p_fsm );

        // Update gains
        cmos_calc_target_gain( p_fsm );
        cmos_analog_gain_update( p_fsm );
        cmos_digital_gain_update( p_fsm );
        fsm_raise_event( p_fsm, event_id_update_iridix );

        cmos_update_exposure_history( p_fsm );

        b_event_processed = 1;
        break;

    case event_id_antiflicker_changed:

        cmos_antiflicker_update( p_fsm );
        cmos_long_exposure_update( p_fsm );

        // Update gains
        cmos_calc_target_gain( p_fsm );
        cmos_analog_gain_update( p_fsm );
        cmos_digital_gain_update( p_fsm );
        fsm_raise_event( p_fsm, event_id_update_iridix );

        cmos_update_exposure_history( p_fsm );

        b_event_processed = 1;
        break;

    case event_id_cmos_refresh:
        cmos_request_interrupt( p_fsm, ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_START ) );
        b_event_processed = 1;
        break;

    case event_id_sensor_not_ready:
        b_event_processed = 1;
        break;

    case event_id_exposure_changed:

        // Update gains
        cmos_inttime_update( p_fsm );
        cmos_antiflicker_update( p_fsm );
        cmos_long_exposure_update( p_fsm );

        // Update gains
        cmos_calc_target_gain( p_fsm );
        cmos_analog_gain_update( p_fsm );
        cmos_digital_gain_update( p_fsm );
        fsm_raise_event( p_fsm, event_id_update_iridix );

        cmos_update_exposure_history( p_fsm );

        b_event_processed = 1;
        break;
    }

    return b_event_processed;
}
