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
#include "system_timer.h"
#include "acamera_command_api.h"
#include "cmos_fsm.h"

#include "acamera_logger.h"

#define is_short_exposure_frame( base ) ( ACAMERA_FSM2CTX_PTR( p_fsm )->frame & 1 )

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_CMOS
#endif

#define EXPOSURE_PARTITION_INTEGRATION_TIME_INDEX 0
#define EXPOSURE_PARTITION_GAIN_INDEX 1

uint16_t *cmos_get_partition_lut( cmos_fsm_ptr_t p_fsm, cmos_partition_lut_index_t lut_idx )
{
    uint32_t i = 0;
    uint16_t *p_luts = _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_EXPOSURE_PARTITION_LUTS );
    uint32_t lut_len = _GET_COLS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_EXPOSURE_PARTITION_LUTS );
    uint32_t lut_count = _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_EXPOSURE_PARTITION_LUTS );

    if ( lut_idx >= PARTITION_LUT_INDEX_MAX ) {
        LOG( LOG_CRIT, "Error: invalid partition lut index: %d, max: %d.", lut_idx, PARTITION_LUT_INDEX_MAX - 1 );
        return NULL;
    }

    // each lut has SYSTEM_EXPOSURE_PARTITION_VALUE_COUNT items
    if ( lut_len != SYSTEM_EXPOSURE_PARTITION_VALUE_COUNT ) {
        LOG( LOG_CRIT, "Error: corrupted calibration luts, lut_len: %d, expected: %d.", lut_len, SYSTEM_EXPOSURE_PARTITION_VALUE_COUNT );
        return NULL;
    }

    if ( lut_idx >= lut_count ) {
        LOG( LOG_CRIT, "Error: corrupted calibration luts, max_index: %d, expected: %d.", lut_count - 1, lut_idx );
        return NULL;
    }

    p_luts += lut_len * lut_idx;

    for ( i = 0; i < lut_len; i += 2 ) {
        LOG( LOG_DEBUG, "%d - %d", p_luts[i], p_luts[i + 1] );
    }

    return p_luts;
}

void cmos_update_exposure_partitioning_lut( cmos_fsm_ptr_t p_fsm )
{
    int i;
    int32_t param[2] = {0, 0}; // in log2


    uint16_t *exposure_partitions = cmos_get_partition_lut( p_fsm, PARTITION_LUT_BALANED_INDEX );
#ifdef AE_SPLIT_PRESET_ID
    if ( p_fsm->strategy == AE_SPLIT_INTEGRATION_PRIORITY ) {
        exposure_partitions = cmos_get_partition_lut( p_fsm, PARTITION_LUT_INTEGRATION_PRIORITY_INDEX );
    }
#endif

    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    int32_t max_gain = sensor_info.again_log2_max + sensor_info.dgain_log2_max + p_fsm->maximum_isp_digital_gain;
    for ( i = 0; i < SYSTEM_EXPOSURE_PARTITION_VALUE_COUNT; ++i ) {
        // exposure_partitions has {integration_time, gain } pairs,
        // so if i is even number, such as 0, 2, means int_time, odd num means gain.
        int i_param = i & 1;
        int32_t addon = 0;
        uint16_t v = exposure_partitions[i];

        switch ( i_param ) {
        case EXPOSURE_PARTITION_INTEGRATION_TIME_INDEX:
            if ( !v ) {
                addon = acamera_log2_fixed_to_fixed( sensor_info.integration_time_limit, 0, LOG2_GAIN_SHIFT );
            } else {
                int32_t lines = cmos_convert_integration_time_ms2lines( p_fsm, v );
                if ( lines < sensor_info.integration_time_min ) {
                    lines = sensor_info.integration_time_min;
                }
                addon = acamera_log2_fixed_to_fixed( lines, 0, LOG2_GAIN_SHIFT );
            }
            break;
        case EXPOSURE_PARTITION_GAIN_INDEX:
            if ( !v ) {
                addon = max_gain;
            } else {
                addon = acamera_log2_fixed_to_fixed( v, 0, LOG2_GAIN_SHIFT );
            }
            break;
        }
        addon -= param[i_param];
        if ( addon < 0 ) {
            addon = 0;
        }
        p_fsm->exp_lut[i] = addon;
        param[i_param] += addon;
    }
}

void _process_fps_cnt( fps_counter_t *fps_cnt )
{
    uint32_t cur_tick = system_timer_timestamp();
    uint32_t frame_ticks = cur_tick - fps_cnt->last_tick;
    fps_cnt->avg_frame_ticks += frame_ticks - ( fps_cnt->avg_frame_ticks >> 4 );
    fps_cnt->last_tick = cur_tick;
}

void _init_fps_cnt( fps_counter_t *fps_cnt )
{
    fps_cnt->last_tick = 0;
    fps_cnt->avg_frame_ticks = ( system_timer_frequency() / 15 ) << 4; // initially it's something like 15 FPS // division by zero is checked
    system_timer_init();
    fps_cnt->last_tick = system_timer_timestamp();
}

uint16_t cmos_get_fps( cmos_fsm_ptr_t p_fsm )
{
    uint64_t tps = system_timer_frequency();
    uint16_t fps;
    if ( p_fsm->fps_cnt.avg_frame_ticks ) {
        uint64_t _tps = ( tps << ( 8 + 4 ) );
        fps = (uint16_t)div64_u64( _tps, p_fsm->fps_cnt.avg_frame_ticks ); // division by zero is checked
    } else {
        LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
        fps = 15 << 8;
    }
    return fps;
}

void cmos_init( cmos_fsm_ptr_t p_fsm )
{
#ifdef AE_SPLIT_PRESET_ID
    p_fsm->strategy = AE_SPLIT_BALANCED;
#endif

    p_fsm->mask.repeat_irq_mask = ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_START ) | ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_END );
    cmos_request_interrupt( p_fsm, p_fsm->mask.repeat_irq_mask );

#ifdef FLASH_XENON
    p_fsm->flash_state = flash_get_init_state();
    p_fsm->flash_skip_charge = 1;
#endif
    _init_fps_cnt( &p_fsm->fps_cnt );


#if FILTER_LONG_INT_TIME || FILTER_SHORT_INT_TIME
    uint16_t i;
#endif
#if FILTER_LONG_INT_TIME
    for ( i = 0; i < FILTER_LONG_INT_TIME; i++ ) {
        p_fsm->long_it_hist.v[i] = 1;
    }
    p_fsm->long_it_hist.sum = FILTER_LONG_INT_TIME;
    p_fsm->long_it_hist.p = &( p_fsm->long_it_hist.v[0] );
#endif

#if FILTER_SHORT_INT_TIME
    for ( i = 0; i < FILTER_SHORT_INT_TIME; i++ ) {
        p_fsm->short_it_hist.v[i] = 1;
    }
    p_fsm->short_it_hist.sum = FILTER_SHORT_INT_TIME;
    p_fsm->short_it_hist.p = &( p_fsm->short_it_hist.v[0] );
#endif

    system_spinlock_init( &p_fsm->exp_lock );
}

void cmos_alloc_integration_time( cmos_fsm_ptr_t p_fsm, int32_t int_time )
{
    // We should set as maximum as possible analogue gain less than or equals target_gain
    unsigned int new_integration_time_short;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );

    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    uint32_t min_integration_time_short = sensor_info.integration_time_min;
    uint32_t max_integration_time_short = sensor_info.integration_time_limit;
    if ( param->global_manual_max_integration_time && max_integration_time_short > param->global_max_integration_time ) {
        max_integration_time_short = param->global_max_integration_time;
    }
    if ( param->global_manual_integration_time ) {
        new_integration_time_short = param->global_integration_time;
    } else {
        new_integration_time_short = acamera_math_exp2( int_time, LOG2_GAIN_SHIFT, 0 );
    }
    if ( new_integration_time_short < min_integration_time_short ) {
        new_integration_time_short = min_integration_time_short;
    } else if ( new_integration_time_short > max_integration_time_short ) {
        new_integration_time_short = max_integration_time_short;
    }

#if FILTER_SHORT_INT_TIME
    //rotate the FIFO
    if ( ++p_fsm->short_it_hist.p > &( p_fsm->short_it_hist.v[FILTER_SHORT_INT_TIME - 1] ) )
        p_fsm->short_it_hist.p = &( p_fsm->short_it_hist.v[0] );
    p_fsm->short_it_hist.sum -= *p_fsm->short_it_hist.p;
    *p_fsm->short_it_hist.p = new_integration_time_short;
    p_fsm->short_it_hist.sum += *p_fsm->short_it_hist.p;

    new_integration_time_short = p_fsm->short_it_hist.sum / FILTER_SHORT_INT_TIME; // division by zero is checked
#endif

    p_fsm->prev_integration_time_short = p_fsm->integration_time_short;
    p_fsm->integration_time_short = new_integration_time_short;
}

int32_t cmos_alloc_sensor_analog_gain( cmos_fsm_ptr_t p_fsm, int32_t gain )
{
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );

    if ( gain < 0 ) {
        return 0;
    }

    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    int32_t max_gain = param->global_max_sensor_analog_gain << ( LOG2_GAIN_SHIFT - 5 );
    if ( gain > max_gain ) {
        gain = max_gain;
    }

    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_ALLOC_ANALOG_GAIN, &gain, sizeof( gain ) );

    // gain will be changed inside function, so we can return gain as the real gain.
    return gain;
}

int32_t cmos_alloc_sensor_digital_gain( cmos_fsm_ptr_t p_fsm, int32_t gain )
{
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
    if ( gain < 0 ) {
        return 0;
    }

    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    int32_t max_gain = MIN( sensor_info.dgain_log2_max, (int32_t)param->global_max_sensor_digital_gain << ( LOG2_GAIN_SHIFT - 5 ) );
    if ( gain > max_gain ) {
        gain = max_gain;
    }

    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_ALLOC_DIGITAL_GAIN, &gain, sizeof( gain ) );

    if ( gain < 0 ) {
        gain = 0; // special mode like native HDR
    }
    return gain;
}

int32_t cmos_alloc_isp_digital_gain( cmos_fsm_ptr_t p_fsm, int32_t gain )
{
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
    if ( gain < 0 ) {
        return 0;
    }
    if ( gain > p_fsm->maximum_isp_digital_gain ) {
        gain = p_fsm->maximum_isp_digital_gain;
    }
    int32_t max_gain = param->global_max_isp_digital_gain << ( LOG2_GAIN_SHIFT - 5 );
    if ( gain > max_gain ) {
        gain = max_gain;
    }
    return gain;
}

int32_t cmos_get_manual_again_log2( cmos_fsm_ptr_t p_fsm )
{
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
    if ( param->global_manual_sensor_analog_gain ) {
        int32_t gain = param->global_sensor_analog_gain << ( LOG2_GAIN_SHIFT - 5 );
        return cmos_alloc_sensor_analog_gain( p_fsm, gain );
    }
    return -1; // negative means it is not manual
}

int32_t cmos_get_manual_dgain_log2( cmos_fsm_ptr_t p_fsm )
{
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
    if ( param->global_manual_sensor_digital_gain ) {
        int32_t gain = ( param->global_sensor_digital_gain << ( LOG2_GAIN_SHIFT - 5 ) );
        return cmos_alloc_sensor_digital_gain( p_fsm, gain );
    }
    return -1; // negative means it is not manual
}

int32_t cmos_get_manual_isp_dgain_log2( cmos_fsm_ptr_t p_fsm )
{
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
    if ( param->global_manual_isp_digital_gain ) {
        int32_t gain = ( param->global_isp_digital_gain << ( LOG2_GAIN_SHIFT - 5 ) );
        return cmos_alloc_isp_digital_gain( p_fsm, gain );
    }
    return -1; // negative means it is not manual
}

int cmos_convert_integration_time_ms2lines( cmos_fsm_ptr_t p_fsm, int time_ms )
{
    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    int res = sensor_info.lines_per_second * time_ms / 1000; // division by zero is checked
    return res;
}

exposure_set_t *cmos_get_frame_exposure_set( cmos_fsm_ptr_t p_fsm, int i_frame )
{
    int pos = p_fsm->exposure_hist_pos + i_frame;
    if ( pos < 0 ) {
        pos += ( sizeof( p_fsm->exposure_hist ) / sizeof( p_fsm->exposure_hist[0] ) );          // division by zero is checked
    } else if ( pos >= ( sizeof( p_fsm->exposure_hist ) / sizeof( p_fsm->exposure_hist[0] ) ) ) // division by zero is checked
    {
        pos -= ( sizeof( p_fsm->exposure_hist ) / sizeof( p_fsm->exposure_hist[0] ) ); // division by zero is checked
    }
    return p_fsm->exposure_hist + pos;
}

static void cmos_store_frame_exposure_set( cmos_fsm_ptr_t p_fsm, exposure_set_t *p_set )
{
    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    p_set->data.integration_time = p_fsm->integration_time_short;

    p_set->data.isp_dgain_log2 = p_fsm->isp_dgain_log2;
    int32_t prev_again;

    if ( sensor_info.sensor_exp_number == 4 ) {

        p_set->data.integration_time_long = p_fsm->integration_time_long;
        p_set->data.integration_time_medium = p_fsm->integration_time_medium;
        p_set->data.integration_time_medium2 = p_fsm->integration_time_medium2;

        p_set->data.exposure_ratio_short = 64 * (uint32_t)p_fsm->integration_time_medium / p_fsm->integration_time_short;
        p_set->data.exposure_ratio_medium = 64 * (uint32_t)p_fsm->integration_time_medium2 / p_fsm->integration_time_medium;

        switch ( sensor_info.isp_exposure_channel_delay ) {
        case 1:
            prev_again = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->info.again_log2;
            p_set->data.exposure_ratio_medium2 = 64 * (uint32_t)p_fsm->integration_time_long / p_fsm->integration_time_medium2 * acamera_math_exp2( prev_again - p_fsm->again_val_log2, LOG2_GAIN_SHIFT, 8 ) >> 8;
            break;
        default:
            p_set->data.exposure_ratio_medium2 = 64 * (uint32_t)p_fsm->integration_time_long / p_fsm->integration_time_medium2;
            break;
        }

    } else if ( sensor_info.sensor_exp_number == 3 ) {
        p_set->data.integration_time_medium = p_fsm->integration_time_medium;
        p_set->data.exposure_ratio_short = 64 * (uint32_t)p_fsm->integration_time_medium / p_fsm->integration_time_short;
        p_set->data.integration_time_long = p_fsm->integration_time_long;

        switch ( sensor_info.isp_exposure_channel_delay ) {
        case 1:
            prev_again = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->info.again_log2;
            p_set->data.exposure_ratio_medium = 64 * (uint32_t)p_fsm->integration_time_long / p_fsm->integration_time_medium * acamera_math_exp2( prev_again - p_fsm->again_val_log2, LOG2_GAIN_SHIFT, 8 ) >> 8;
            break;
        default:
            p_set->data.exposure_ratio_medium = 64 * (uint32_t)p_fsm->integration_time_long / p_fsm->integration_time_medium;
            break;
        }

    } else if ( sensor_info.sensor_exp_number == 2 ) {
        p_set->data.integration_time_long = p_fsm->integration_time_long;

        switch ( sensor_info.isp_exposure_channel_delay ) {
        case 1:
            prev_again = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->info.again_log2;
            p_set->data.exposure_ratio = 64 * (uint32_t)p_fsm->integration_time_long / p_fsm->integration_time_short * acamera_math_exp2( prev_again - p_fsm->again_val_log2, LOG2_GAIN_SHIFT, 8 ) >> 8;
            break;
        default:
            p_set->data.exposure_ratio = 64 * (uint32_t)p_fsm->integration_time_long / p_fsm->integration_time_short;
            break;
        }
    } else {
        p_set->data.integration_time_long = p_fsm->integration_time_long;
        p_set->data.exposure_ratio = 64 * (uint32_t)p_fsm->integration_time_long / p_fsm->integration_time_short;
    }


    p_set->info.again_log2 = p_fsm->again_val_log2;
    p_set->info.dgain_log2 = p_fsm->dgain_val_log2;
    p_set->info.isp_dgain_log2 = p_fsm->isp_dgain_log2;
    // update exposure for this frame
    p_set->info.exposure_log2 = p_set->info.again_log2 + p_set->info.dgain_log2 + p_set->info.isp_dgain_log2;
    p_set->info.exposure_log2 += acamera_log2_fixed_to_fixed( p_set->data.integration_time, 0, LOG2_GAIN_SHIFT );

    p_set->data.frame_id_tracking = p_fsm->frame_id_tracking;
}

void cmos_update_exposure_history( cmos_fsm_ptr_t p_fsm )
{
    unsigned long irq_flags;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
    //uint32_t gain;
    if ( p_fsm->exposure_hist_pos < 0 ) {
        int pos;
        fsm_param_sensor_info_t sensor_info;
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

        p_fsm->exposure_hist_pos = 0;
        // reset state
        for ( pos = 0; pos <= sensor_info.integration_time_apply_delay; ++pos ) {
            cmos_store_frame_exposure_set( p_fsm, cmos_get_frame_exposure_set( p_fsm, pos ) );
        }
    }

    irq_flags = system_spinlock_lock( p_fsm->exp_lock );
    cmos_store_frame_exposure_set( p_fsm, &( p_fsm->exp_next_set ) );
    system_spinlock_unlock( p_fsm->exp_lock, irq_flags );

    // display gains in control tool
    if ( !param->global_manual_integration_time ) {
#if EXPOSURE_DRIVES_LONG_INTEGRATION_TIME
        uint32_t wdr_mode = 0;

        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );

        param->global_integration_time = ( ( ( wdr_mode == WDR_MODE_LINEAR ) ? p_fsm->integration_time_short : p_fsm->integration_time_long ) );
#else
        param->global_integration_time = ( p_fsm->integration_time_short );
#endif
    }
    if ( !param->global_manual_sensor_analog_gain ) {
        param->global_sensor_analog_gain = ( p_fsm->again_val_log2 >> ( LOG2_GAIN_SHIFT - 5 ) );
    }
    if ( !param->global_manual_sensor_digital_gain ) {
        param->global_sensor_digital_gain = ( p_fsm->dgain_val_log2 >> ( LOG2_GAIN_SHIFT - 5 ) );
    }
    if ( !param->global_manual_isp_digital_gain ) {
        param->global_isp_digital_gain = ( p_fsm->isp_dgain_log2 >> ( LOG2_GAIN_SHIFT - 5 ) );
    }
}

static void cmos_mointor_frame_start( cmos_fsm_ptr_t p_fsm )
{
    uint32_t cur_frame_id = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );

    /* CMOS FrameStart should be called for every frame */
    if ( ( p_fsm->prev_fs_frame_id != 0 ) &&
         ( p_fsm->prev_fs_frame_id + 1 != cur_frame_id ) ) {

        fsm_param_mon_err_head_t mon_err;
        mon_err.err_type = MON_TYPE_ERR_CMOS_FS_DELAY;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_ERROR_REPORT, &mon_err, sizeof( mon_err ) );
        LOG( LOG_ERR, "cmos_mon_fe: cur: %u, pre: %u.", cur_frame_id, p_fsm->prev_fs_frame_id );
    }

    p_fsm->prev_fs_frame_id = cur_frame_id;
}


static void cmos_mointor_frame_end( cmos_fsm_ptr_t p_fsm )
{

    uint32_t irq_mask = acamera_isp_isp_global_interrupt_status_vector_read( p_fsm->cmn.isp_base );
    if ( irq_mask & 0x1 ) {
        fsm_param_mon_err_head_t mon_err;
        mon_err.err_type = MON_TYPE_ERR_CMOS_UPDATE_NOT_IN_VB;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_ERROR_REPORT, &mon_err, sizeof( mon_err ) );
    }
}

static void cmos_move_exposure_history( cmos_fsm_ptr_t p_fsm )
{
    unsigned long irq_flags;

    int pos = p_fsm->exposure_hist_pos + 1;
    if ( pos >= ( sizeof( p_fsm->exposure_hist ) / sizeof( p_fsm->exposure_hist[0] ) ) ) // division by zero is checked
    {
        pos = 0;
    }
    p_fsm->exposure_hist_pos = pos;
    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    irq_flags = system_spinlock_lock( p_fsm->exp_lock );
    *cmos_get_frame_exposure_set( p_fsm, sensor_info.integration_time_apply_delay ) = p_fsm->exp_next_set;
    system_spinlock_unlock( p_fsm->exp_lock, irq_flags );
}

void cmos_fsm_process_interrupt( cmos_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
    status_info_param_t *p_status_info = (status_info_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STATUS_INFO );

    uint32_t wdr_mode = 0;

    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );

    switch ( irq_event ) {
    case ACAMERA_IRQ_FRAME_START:
        cmos_move_exposure_history( (cmos_fsm_ptr_t)p_fsm );
        {
            exposure_data_set_t exp_set = {0};
            if ( ( wdr_mode == WDR_MODE_FS_LIN ) || ( ( wdr_mode == WDR_MODE_NATIVE ) && ( sensor_info.sensor_exp_number == 3 ) ) ) {
                if ( sensor_info.sensor_exp_number == 1 ) {
                    exp_set.integration_time = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.actual_integration_time;
                    exp_set.exposure_ratio = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio;
                    if ( is_short_exposure_frame( p_fsm->cmn.isp_base ) == 0 ) {
                        ACAMERA_FSM2CTX_PTR( p_fsm )
                            ->stab.global_long_integration_time = exp_set.integration_time;
                    } else {
                        ACAMERA_FSM2CTX_PTR( p_fsm )
                            ->stab.global_short_integration_time = ( exp_set.integration_time );
                    }
                    exp_set.actual_integration_time = exp_set.integration_time;

                } else if ( sensor_info.sensor_exp_number == 2 ) {
                    exp_set.integration_time = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time;
                    exp_set.integration_time_long = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time_long;
                    exp_set.exposure_ratio = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio;
                    ACAMERA_FSM2CTX_PTR( p_fsm )
                        ->stab.global_long_integration_time = ( exp_set.integration_time_long );
                    ACAMERA_FSM2CTX_PTR( p_fsm )
                        ->stab.global_short_integration_time = ( exp_set.integration_time );

                } else if ( sensor_info.sensor_exp_number == 3 ) {
                    exp_set.integration_time = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time;
                    exp_set.integration_time_medium = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time_medium;
                    exp_set.integration_time_long = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time_long;
                    exp_set.exposure_ratio_short = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio_short;
                    exp_set.exposure_ratio_medium = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio_medium;
                    //LOG( LOG_ERR, " short %d medium %d long %d Rs %d Rm %d", exp_set.integration_time, exp_set.integration_time_medium, exp_set.integration_time_long, exp_set.exposure_ratio_short, exp_set.exposure_ratio_medium );

                    ACAMERA_FSM2CTX_PTR( p_fsm )
                        ->stab.global_long_integration_time = ( exp_set.integration_time_long );
                    ACAMERA_FSM2CTX_PTR( p_fsm )
                        ->stab.global_short_integration_time = ( exp_set.integration_time );

                } else /* sensor_exp_number == 4 */ {
                    exp_set.integration_time = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time;
                    exp_set.integration_time_medium = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time_medium;
                    exp_set.integration_time_medium2 = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time_medium2;
                    exp_set.integration_time_long = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time_long;
                    exp_set.exposure_ratio_short = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio_short;
                    exp_set.exposure_ratio_medium = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio_medium;
                    exp_set.exposure_ratio_medium2 = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio_medium2;
                    //LOG( LOG_ERR, " short %d medium %d long %d Rs %d Rm %d", exp_set.integration_time, exp_set.integration_time_medium, exp_set.integration_time_long, exp_set.exposure_ratio_short, exp_set.exposure_ratio_medium );

                    ACAMERA_FSM2CTX_PTR( p_fsm )
                        ->stab.global_long_integration_time = ( exp_set.integration_time_long );
                    ACAMERA_FSM2CTX_PTR( p_fsm )
                        ->stab.global_short_integration_time = ( exp_set.integration_time );
                }
            } else {
                exp_set.integration_time = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time;
                ACAMERA_FSM2CTX_PTR( p_fsm )
                    ->stab.global_short_integration_time = ( exp_set.integration_time );

                if ( sensor_info.sensor_exp_number == 1 ) {
                    exp_set.actual_integration_time = exp_set.integration_time;
                    exp_set.exposure_ratio = 64;

                } else if ( sensor_info.sensor_exp_number == 2 ) {
                    exp_set.integration_time = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time;
                    exp_set.integration_time_long = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time_long;
                    exp_set.exposure_ratio = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio;

                } else if ( sensor_info.sensor_exp_number == 3 ) {
                    exp_set.integration_time = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time;
                    exp_set.integration_time_medium = exp_set.integration_time;
                    exp_set.integration_time_long = exp_set.integration_time;
                    exp_set.exposure_ratio_short = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio_short;
                    exp_set.exposure_ratio_medium = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio_medium;

                } else /* sensor_exp_number == 4 */ {
                    exp_set.integration_time = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, sensor_info.integration_time_apply_delay )->data.integration_time;
                    exp_set.integration_time_medium = exp_set.integration_time;
                    exp_set.integration_time_medium2 = exp_set.integration_time;
                    exp_set.integration_time_long = exp_set.integration_time;
                    exp_set.exposure_ratio_short = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio_short;
                    exp_set.exposure_ratio_medium = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio_medium;
                    exp_set.exposure_ratio_medium2 = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.exposure_ratio_medium2;
                }
            }
            exp_set.isp_dgain_log2 = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.isp_dgain_log2;

            // tracking integration time and isp_dgain separately?
            exp_set.frame_id_tracking = cmos_get_frame_exposure_set( (cmos_fsm_ptr_t)p_fsm, 2 )->data.frame_id_tracking;

            ( (cmos_fsm_ptr_t)p_fsm )->exp_write_set = exp_set;

            if ( ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_freeze_firmware != 1 ) ) {
                acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_UPDATE, NULL, 0 );

                // frame_id should not be 0, at the beginning, it's initialized to 0 and we should skip it.
                if ( ( p_fsm->prev_ae_frame_id_tracking != exp_set.frame_id_tracking ) && ( exp_set.frame_id_tracking != 0 ) ) {
                    fsm_param_mon_alg_flow_t ae_flow;

                    ( (cmos_fsm_ptr_t)p_fsm )->prev_ae_frame_id_tracking = exp_set.frame_id_tracking;

                    ae_flow.frame_id_tracking = exp_set.frame_id_tracking;
                    ae_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &( (cmos_fsm_ptr_t)p_fsm )->cmn );
                    ae_flow.flow_state = MON_ALG_FLOW_STATE_APPLIED;
                    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_AE_FLOW, &ae_flow, sizeof( ae_flow ) );
                }
            }

            // check antiflicker frequency value
            int flicker_freq = (int)param->global_anti_flicker_frequency * 256;
            if ( param->global_antiflicker_enable == 0 ) {
                flicker_freq = 0;
            }
            if ( flicker_freq != p_fsm->flicker_freq ) {
                ( (cmos_fsm_ptr_t)p_fsm )->flicker_freq = flicker_freq;
                fsm_raise_event( p_fsm, event_id_antiflicker_changed );
            }
        }

        p_status_info->sys_info.exposure_log2 = p_fsm->exposure_log2;
        p_status_info->sys_info.total_gain_log2 = p_fsm->again_val_log2 + p_fsm->dgain_val_log2 + p_fsm->isp_dgain_log2;

        cmos_mointor_frame_start( (cmos_fsm_ptr_t)p_fsm );

        break;
    case ACAMERA_IRQ_FRAME_END: {

        if ( ( wdr_mode == WDR_MODE_FS_LIN ) && ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_frame_stitch == 0 ) ) {

            if ( sensor_info.sensor_exp_number == 4 ) {
                acamera_isp_frame_stitch_svs_exposure_ratio_write( p_fsm->cmn.isp_base, p_fsm->exp_write_set.exposure_ratio_short );
                acamera_isp_frame_stitch_ms_exposure_ratio_write( p_fsm->cmn.isp_base, p_fsm->exp_write_set.exposure_ratio_medium );
                acamera_isp_frame_stitch_lm_exposure_ratio_write( p_fsm->cmn.isp_base, p_fsm->exp_write_set.exposure_ratio_medium2 );

                int32_t R12 = p_fsm->exp_write_set.exposure_ratio_medium2;                        //long/medium ratio
                int32_t R13 = ( R12 * (int32_t)p_fsm->exp_write_set.exposure_ratio_medium ) >> 6; //long/short ratio
                int32_t R14 = ( R13 * (int32_t)p_fsm->exp_write_set.exposure_ratio_short ) >> 6;  //long/very_short ratio

                if ( R12 < 0 ) {
                    R12 = 1;
                }
                if ( R13 < 0 ) {
                    R13 = 1;
                }
                if ( R14 < 0 ) {
                    R14 = 1;
                }

                const int32_t SF2 = ( 262144 / R12 + 262144 / R13 + 262144 / R14 ) * 2048 / ( 4096 + 262144 / R12 + 262144 / R13 + 262144 / R14 );
                const int32_t SF3 = ( 262144 / R13 + 262144 / R14 ) * 2048 / ( 262144 / R12 + 262144 / R13 + 262144 / R14 );
                const int32_t SF4 = ( 262144 / R14 ) * 2048 / ( 262144 / R13 + 262144 / R14 );

                acamera_isp_frame_stitch_mcoff_l_scaler_write( p_fsm->cmn.isp_base, (uint16_t)SF2 );
                acamera_isp_frame_stitch_mcoff_lm_scaler_write( p_fsm->cmn.isp_base, (uint16_t)SF3 );
                acamera_isp_frame_stitch_mcoff_lms_scaler_write( p_fsm->cmn.isp_base, (uint16_t)SF4 );

            } else if ( sensor_info.sensor_exp_number == 3 ) {

                acamera_isp_frame_stitch_ms_exposure_ratio_write( p_fsm->cmn.isp_base, p_fsm->exp_write_set.exposure_ratio_short );
                acamera_isp_frame_stitch_lm_exposure_ratio_write( p_fsm->cmn.isp_base, p_fsm->exp_write_set.exposure_ratio_medium );
                // mc off
                // if(Exp == 2)
                //     param.SF2 = 1/(1 + R12);
                //     param.SF3 = 0;
                //     param.SF4 = 0;
                // elseif(Exp == 3)
                //     param.SF2 = (1/R12 + 1/R13) / (1 + 1/R12 + 1/R13);
                //     param.SF3 = (1/R13)/(1/R12 + 1/R13);
                //     param.SF4 = 0;
                // elseif(Exp == 4)
                //     param.SF2 = (1/R12 + 1/R13 + 1/R14)/(1 + 1/R12 + 1/R13 + 1/R14);
                //     param.SF3 = (1/R13 + 1/R14)/(1/R12 + 1/R13 + 1/R14);
                //     param.SF4 = (1/R14)/(1/R13 + 1/R14);

                int32_t SF2, SF3, SF4; //U0.11 registers
                int32_t R12, R13;
                R12 = p_fsm->exp_write_set.exposure_ratio_medium;                                                                                    //long/medium ratio
                R13 = ( ( int32_t )( p_fsm->exp_write_set.exposure_ratio_medium ) * ( int32_t )( p_fsm->exp_write_set.exposure_ratio_short ) ) >> 6; //long/short ratio
                // printf("r12 %d r13 %d\n",(int)R12,(int)R13 );
                // SF2 = round(((round(262144/R12) + round(262144/R13))*2048) / round((2^12 + round(262144/R12) + round(262144/R13))))

                SF2 = ( ( 262144 / R12 + 262144 / R13 ) * 2048 ) / ( 4096 + 262144 / R12 + 262144 / R13 ); //U0.11
                SF3 = ( ( 262144 / R13 ) * 2048 ) / ( 262144 / R12 + 262144 / R13 );                       //U0.11
                SF4 = 0;
                acamera_isp_frame_stitch_mcoff_l_scaler_write( p_fsm->cmn.isp_base, (uint16_t)SF2 );
                acamera_isp_frame_stitch_mcoff_lm_scaler_write( p_fsm->cmn.isp_base, (uint16_t)SF3 );
                acamera_isp_frame_stitch_mcoff_lms_scaler_write( p_fsm->cmn.isp_base, (uint16_t)SF4 );

            } else if ( sensor_info.sensor_exp_number == 2 ) {
                int32_t SF2, R12; //U0.11 registers
                R12 = p_fsm->exp_write_set.exposure_ratio;
                SF2 = 262144 / ( 2048 + R12 );
                acamera_isp_frame_stitch_lm_exposure_ratio_write( p_fsm->cmn.isp_base, p_fsm->exp_write_set.exposure_ratio );

                acamera_isp_frame_stitch_mcoff_l_scaler_write( p_fsm->cmn.isp_base, (uint16_t)SF2 );
                acamera_isp_frame_stitch_mcoff_lm_scaler_write( p_fsm->cmn.isp_base, (uint16_t)0 );
                acamera_isp_frame_stitch_mcoff_lms_scaler_write( p_fsm->cmn.isp_base, (uint16_t)0 );
            } else {
                acamera_isp_frame_stitch_lm_exposure_ratio_write( p_fsm->cmn.isp_base, p_fsm->exp_write_set.exposure_ratio );
            }
        }

#if defined( ISP_HAS_AWB_MESH_NBP_FSM ) || defined( ISP_HAS_AWB_MANUAL_FSM )
        uint32_t i;
        int32_t isp_dgain_log2 = p_fsm->exp_write_set.isp_dgain_log2;

        fsm_param_awb_info_t wb_info;
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_AWB_INFO, NULL, 0, &wb_info, sizeof( wb_info ) );

        uint32_t gain;

        gain = acamera_math_exp2( isp_dgain_log2, LOG2_GAIN_SHIFT, 8 );
        acamera_isp_digital_gain_gain_write( p_fsm->cmn.isp_base, gain );

        for ( i = 0; i < 4; ++i ) {
            gain = acamera_math_exp2( wb_info.wb_log2[i], LOG2_GAIN_SHIFT, 8 );
            ( (cmos_fsm_ptr_t)p_fsm )->wb[i] = gain > 0xFFF ? 0xFFF : (uint16_t)gain;
        }

        // Expansion of values to the full range
        uint32_t bl[4] = {acamera_isp_sensor_offset_pre_shading_offset_00_read( p_fsm->cmn.isp_base ),
                          acamera_isp_sensor_offset_pre_shading_offset_01_read( p_fsm->cmn.isp_base ),
                          acamera_isp_sensor_offset_pre_shading_offset_10_read( p_fsm->cmn.isp_base ),
                          acamera_isp_sensor_offset_pre_shading_offset_11_read( p_fsm->cmn.isp_base )};
        for ( i = 0; i < 4; ++i ) {
            uint32_t mult = ( 256 * ( ( (uint32_t)1 << ACAMERA_ISP_SENSOR_OFFSET_PRE_SHADING_OFFSET_00_DATASIZE ) - 1 ) ) / ( ( ( (uint32_t)1 << ACAMERA_ISP_SENSOR_OFFSET_PRE_SHADING_OFFSET_00_DATASIZE ) - 1 ) - bl[i] );
            ( (cmos_fsm_ptr_t)p_fsm )->wb[i] = ( ( ( uint32_t )( (cmos_fsm_ptr_t)p_fsm )->wb[i] ) * mult ) / 256;
        }

        acamera_isp_white_balance_gain_00_write( p_fsm->cmn.isp_base, ( (cmos_fsm_ptr_t)p_fsm )->wb[0] );
        acamera_isp_white_balance_gain_01_write( p_fsm->cmn.isp_base, ( (cmos_fsm_ptr_t)p_fsm )->wb[1] );
        acamera_isp_white_balance_gain_10_write( p_fsm->cmn.isp_base, ( (cmos_fsm_ptr_t)p_fsm )->wb[2] );
        acamera_isp_white_balance_gain_11_write( p_fsm->cmn.isp_base, ( (cmos_fsm_ptr_t)p_fsm )->wb[3] );

        if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_frame_stitch == 0 ) {

            acamera_isp_frame_stitch_gain_r_write( p_fsm->cmn.isp_base, ( (cmos_fsm_ptr_t)p_fsm )->wb[0] );
            acamera_isp_frame_stitch_gain_b_write( p_fsm->cmn.isp_base, ( (cmos_fsm_ptr_t)p_fsm )->wb[3] );

            if ( acamera_isp_frame_stitch_mcoff_mode_enable_read( p_fsm->cmn.isp_base ) == 1 ) {
                acamera_isp_frame_stitch_gain_r_write( p_fsm->cmn.isp_base, 65536 / ( ( (cmos_fsm_ptr_t)p_fsm )->wb[0] ) );
                acamera_isp_frame_stitch_gain_b_write( p_fsm->cmn.isp_base, 65536 / ( ( (cmos_fsm_ptr_t)p_fsm )->wb[3] ) );
            }
        }

        uint32_t cur_frame_id = acamera_fsm_util_get_cur_frame_id( &( (cmos_fsm_ptr_t)p_fsm )->cmn );

        // "sensor_info.integration_time_apply_delay + 1" because the current frame_id is not so accurate.
        if ( p_fsm->exp_write_set.frame_id_tracking && ( p_fsm->exp_write_set.frame_id_tracking != p_fsm->prev_dgain_frame_id ) &&
             ( cur_frame_id - p_fsm->exp_write_set.frame_id_tracking > sensor_info.integration_time_apply_delay + 1 ) ) {
            fsm_param_mon_err_head_t mon_err;
            mon_err.err_type = MON_TYPE_ERR_CMOS_UPDATE_DGAIN_WRONG_TIMING;
            mon_err.err_param = cur_frame_id - p_fsm->exp_write_set.frame_id_tracking - sensor_info.integration_time_apply_delay;
            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_ERROR_REPORT, &mon_err, sizeof( mon_err ) );
            ( (cmos_fsm_ptr_t)p_fsm )->prev_dgain_frame_id = p_fsm->exp_write_set.frame_id_tracking;
        }

        // frame_id should not be 0, at the beginning, it's initialized to 0 and we should skip it.
        if ( ( p_fsm->prev_awb_frame_id_tracking != wb_info.result_gain_frame_id ) && ( wb_info.result_gain_frame_id != 0 ) ) {
            fsm_param_mon_alg_flow_t awb_flow;

            ( (cmos_fsm_ptr_t)p_fsm )->prev_awb_frame_id_tracking = wb_info.result_gain_frame_id;

            awb_flow.frame_id_tracking = wb_info.result_gain_frame_id;
            awb_flow.frame_id_current = cur_frame_id;
            awb_flow.flow_state = MON_ALG_FLOW_STATE_APPLIED;
            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_AWB_FLOW, &awb_flow, sizeof( awb_flow ) );
        }

#endif //ISP_HAS_AWB_MESH_NBP_FSM

        _process_fps_cnt( (fps_counter_t *)&p_fsm->fps_cnt );
    }

        cmos_mointor_frame_end( (cmos_fsm_ptr_t)p_fsm );

        break;
    }
}

void cmos_inttime_update( cmos_fsm_ptr_t p_fsm )
{
    int32_t int_time = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
    if ( param->global_manual_integration_time == 0 ) {
        int i;
        int32_t exposure = 0, exp_target = p_fsm->exposure_log2;
        if ( p_fsm->manual_gain_mode == 1 ) {
            int_time = exp_target - acamera_log2_fixed_to_fixed( p_fsm->manual_gain, 8, LOG2_GAIN_SHIFT );
        } else {
            int32_t gain;
            gain = cmos_get_manual_again_log2( p_fsm );
            if ( gain >= 0 ) {
                exp_target -= gain;
            }
            gain = cmos_get_manual_dgain_log2( p_fsm );
            if ( gain >= 0 ) {
                exp_target -= gain;
            }
            gain = cmos_get_manual_isp_dgain_log2( p_fsm );
            if ( gain >= 0 ) {
                exp_target -= gain;
            }
            if ( exp_target < 0 ) {
                exp_target = 0;
            }

            for ( i = 0; i < SYSTEM_EXPOSURE_PARTITION_VALUE_COUNT; ++i ) {
                int i_param = i & 1;
                int32_t addon = p_fsm->exp_lut[i];

                if ( i_param == EXPOSURE_PARTITION_INTEGRATION_TIME_INDEX ) {
                    if ( exposure + addon > exp_target ) {
                        addon = exp_target - exposure;
                    }
                    int_time += addon;
                }
                exposure += addon;
                if ( exposure >= exp_target ) {
                    break;
                }
            }
            if ( p_fsm->flicker_freq && !p_fsm->manual_gain_mode && param->global_antiflicker_enable ) {
                fsm_param_sensor_info_t sensor_info;
                acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

                uint32_t line_per_half_period = ( sensor_info.lines_per_second << 8 ) / ( p_fsm->flicker_freq * 2 ); // division by zero is checked
                if ( line_per_half_period != 0 ) {
                    int32_t half_period_log2 = acamera_log2_fixed_to_fixed( line_per_half_period, 0, LOG2_GAIN_SHIFT );
                    if ( int_time < half_period_log2 ) {
                        int_time = MIN( exp_target, half_period_log2 );
                    }
                }
            }
        }
    }
    cmos_alloc_integration_time( p_fsm, int_time );
}

void cmos_analog_gain_update( cmos_fsm_ptr_t p_fsm )
{
    int32_t target_gain = p_fsm->target_gain_log2;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );

    uint32_t wdr_mode = 0;
    if ( param->global_analog_gain_last_priority == 1 ) {
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );

        if ( wdr_mode == WDR_MODE_NATIVE || wdr_mode == WDR_MODE_FS_LIN || wdr_mode == WDR_MODE_LINEAR ) {
            // automatic sensor digital gain goes before analog gain
            if ( cmos_get_manual_dgain_log2( p_fsm ) < 0 ) // negative means it is not manual
            {
                p_fsm->dgain_val_log2 = cmos_alloc_sensor_digital_gain( p_fsm, target_gain );
                target_gain -= p_fsm->dgain_val_log2;
            }
            // analog gain has reserve before achieving maximum
            if ( cmos_get_manual_isp_dgain_log2( p_fsm ) < 0 ) // negative means it is not manual
            {
                int32_t max_again = (int32_t)param->global_max_sensor_analog_gain << ( LOG2_GAIN_SHIFT - 5 );
                int32_t max_isp_gain = ( (int32_t)param->global_max_isp_digital_gain << ( LOG2_GAIN_SHIFT - 5 ) ) - ( 1 << LOG2_GAIN_SHIFT );
                int32_t reserved_again = max_again - ( ( (int32_t)param->global_analog_gain_reserve ) << LOG2_GAIN_SHIFT );
                if ( target_gain > reserved_again + max_isp_gain ) {
                    target_gain -= max_isp_gain;
                } else if ( target_gain > reserved_again ) {
                    target_gain = reserved_again;
                }
            }
        }
    }
    const int32_t again_accuracy = ANALOG_GAIN_ACCURACY;
    int32_t gain = cmos_get_manual_again_log2( p_fsm );
    if ( gain < 0 ) // negative means it is not manual
    {
        // We should set as maximum as possible analogue gain less than or equals target_gain
        gain = target_gain - again_accuracy;
        if ( gain < 0 ) {
            gain = 0;
        }
    }
    if (param->global_manual_sensor_analog_gain == 0) {
        int32_t again = cmos_alloc_sensor_analog_gain( p_fsm, gain );
        if ( !param->global_manual_sensor_analog_gain &&
             ( again != p_fsm->again_val_log2 ) ) {
             if ( ( gain > p_fsm->again_val_log2 - again_accuracy ) &&
                 ( gain < p_fsm->again_val_log2 + again_accuracy ) ) {
                 again = cmos_alloc_sensor_analog_gain( p_fsm, p_fsm->again_val_log2 );
             }
        }
        p_fsm->again_val_log2 = again;
    } else {
        p_fsm->again_val_log2 = gain;
    }
}

void cmos_digital_gain_update( cmos_fsm_ptr_t p_fsm )
{

    int32_t gain = cmos_get_manual_dgain_log2( p_fsm );
    if ( gain < 0 ) // negative means it is not manual
    {
        // we should set the rest gain to make target_gain equals analogue_gain x digital_gain
        gain = cmos_alloc_sensor_digital_gain( p_fsm, p_fsm->target_gain_log2 - p_fsm->again_val_log2 );
    }
    p_fsm->dgain_val_log2 = gain;
    gain = cmos_get_manual_isp_dgain_log2( p_fsm );
    if ( gain < 0 ) // negative means it is not manual
    {
        gain = cmos_alloc_isp_digital_gain( p_fsm, p_fsm->target_gain_log2 - p_fsm->again_val_log2 - p_fsm->dgain_val_log2 );
    }
    p_fsm->isp_dgain_log2 = gain;
}

uint32_t get_quantised_integration_time( cmos_fsm_ptr_t p_fsm, uint32_t int_time )
{
    if ( !p_fsm->flicker_freq || p_fsm->manual_gain_mode ) {
        return int_time;
    }

    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    uint32_t line_per_half_period = ( sensor_info.lines_per_second << 8 ) / ( p_fsm->flicker_freq * 2 ); // division by zero is checked
    if ( line_per_half_period != 0 && ( int_time >= line_per_half_period && int_time <= ( line_per_half_period << 2 ) ) ) {
        uint32_t N = int_time / line_per_half_period; // division by zero is checked
        if ( N < 1 ) {
            N = 1;
        }
        return N * line_per_half_period;
    } else {
        return int_time;
    }
}

uint32_t get_quantised_long_integration_time( cmos_fsm_ptr_t p_fsm, uint32_t int_time, uint32_t max_int_time )
{
    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    if ( int_time < sensor_info.integration_time_min ) {
        int_time = sensor_info.integration_time_min;
    } else if ( int_time > max_int_time ) {
        int_time = max_int_time;
    }
    return get_quantised_integration_time( p_fsm, int_time );
}

void cmos_antiflicker_update( cmos_fsm_ptr_t p_fsm )
{
    uint32_t integration_time_short = get_quantised_integration_time( p_fsm, p_fsm->integration_time_short );

    if ( integration_time_short <= p_fsm->integration_time_short ) {
        p_fsm->integration_time_short = integration_time_short;
    } else {
// Very bright scene
#if OVEREXPOSE_TO_KEEP_ANTIFLICKER
        uint32_t wdr_mode = 0;
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );
        //Only do overexpose to keep antiflicker in linear mode
        if ( wdr_mode == WDR_MODE_LINEAR ) {
            cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
            // if set manual integration time, do not adjust.
            if ( param->global_manual_integration_time ) {
                return;
            }
            if ( param->global_antiflicker_enable && p_fsm->integration_time_short < integration_time_short ) {
                p_fsm->integration_time_short = integration_time_short; // division by zero is checked
            }
        }
#endif
    }
}

void cmos_long_exposure_update( cmos_fsm_ptr_t p_fsm )
{
    fsm_param_sensor_int_time_t time;

    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
    uint32_t exposure_ratio, integration_time_long, integration_time_long_quant;
    uint32_t wdr_mode = 0;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );

    if ( ( wdr_mode == WDR_MODE_FS_LIN ) || ( ( wdr_mode == WDR_MODE_NATIVE ) && ( sensor_info.sensor_exp_number == 3 ) ) ) {
        uint32_t integration_time_long_max = sensor_info.integration_time_long_max;
        if ( !integration_time_long_max ) {
            uint32_t integration_time_long_max = sensor_info.integration_time_limit;

            if ( param->global_manual_max_integration_time && integration_time_long_max > param->global_max_integration_time ) {
                integration_time_long_max = param->global_max_integration_time;
            }
        }
        exposure_ratio = p_fsm->exposure_ratio_in;
        if ( exposure_ratio < 64 ) {
            exposure_ratio = 64;
        }
#if EXPOSURE_DRIVES_LONG_INTEGRATION_TIME
        uint32_t min_integration_time_short = sensor_info.integration_time_min;
        // Inverse long and short exposure
        integration_time_long = p_fsm->integration_time_short;
        p_fsm->integration_time_short = ( (uint32_t)integration_time_long << 6 ) / exposure_ratio; // division by zero is checked
        if ( p_fsm->integration_time_short < min_integration_time_short ) {
            p_fsm->integration_time_short = min_integration_time_short;
        }
        uint32_t integration_time_max = sensor_info.integration_time_max;
        if ( p_fsm->integration_time_short > integration_time_max ) {
            p_fsm->integration_time_short = integration_time_max;
        }
#else
        integration_time_long = (uint32_t)p_fsm->integration_time_short * exposure_ratio >> 6;
        if ( integration_time_long > 0xFFFF )
            integration_time_long = 0xFFFF;
#endif

        integration_time_long_quant = get_quantised_long_integration_time( p_fsm, integration_time_long, integration_time_long_max );
#if EXPOSURE_DRIVES_LONG_INTEGRATION_TIME
        if ( integration_time_long < integration_time_long_quant ) {
            integration_time_long_quant = integration_time_long;
        }
#endif
        if ( integration_time_long_quant > integration_time_long_max ) {
            integration_time_long_quant = integration_time_long_max;
        }

#if FILTER_LONG_INT_TIME
        //rotate the FIFO
        if ( ++p_fsm->long_it_hist.p > &( p_fsm->long_it_hist.v[FILTER_LONG_INT_TIME - 1] ) )
            p_fsm->long_it_hist.p = &( p_fsm->long_it_hist.v[0] );
        p_fsm->long_it_hist.sum -= *p_fsm->long_it_hist.p;
        *p_fsm->long_it_hist.p = integration_time_long_quant;
        p_fsm->long_it_hist.sum += *p_fsm->long_it_hist.p;

        integration_time_long_quant = p_fsm->long_it_hist.sum / FILTER_LONG_INT_TIME; // division by zero is checked
#endif

        p_fsm->integration_time_long = integration_time_long_quant;
        if ( sensor_info.sensor_exp_number == 4 ) {

            const uint32_t exposure_ratio_thresholded = exposure_ratio > 256 ? exposure_ratio / 2 : exposure_ratio;
            const uint32_t ratio_cube_root = acamera_math_exp2( acamera_log2_fixed_to_fixed( exposure_ratio_thresholded, 6, 16 ) / 3, 16, 6 );
            //LOG(LOG_CRIT, "ratio_cube_root: %d -> %d", exposure_ratio_thresholded, ratio_cube_root);

            uint32_t integration_time_medium1 = p_fsm->integration_time_short * ratio_cube_root >> 6;
            uint32_t integration_time_medium2 = integration_time_medium1 * ratio_cube_root >> 6;

            if ( integration_time_medium2 > integration_time_long_max ) {
                integration_time_medium2 = integration_time_long_max;
            }
            if ( integration_time_medium2 > p_fsm->integration_time_long ) {
                integration_time_medium2 = p_fsm->integration_time_long;
            }
            if ( integration_time_medium1 > integration_time_medium2 ) {
                integration_time_medium1 = integration_time_medium2;
            }

            time.int_time = p_fsm->integration_time_short;
            time.int_time_M = integration_time_medium1;
            time.int_time_M2 = integration_time_medium2;
            time.int_time_L = p_fsm->integration_time_long;

            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_ALLOC_INTEGRATION_TIME, &time, sizeof( time ) );

            // time is updated in above function, save it.
            p_fsm->integration_time_short = time.int_time;
            p_fsm->integration_time_medium = time.int_time_M;
            p_fsm->integration_time_medium2 = time.int_time_M2;
            p_fsm->integration_time_long = time.int_time_L;

        } else if ( sensor_info.sensor_exp_number == 3 ) {
            uint32_t integration_time_medium;
            if ( exposure_ratio > 256 ) {
                //half ML ratio by two
                integration_time_medium = (uint32_t)p_fsm->integration_time_short * ( acamera_sqrt32( exposure_ratio ) << 1 ) >> 3;
            } else {
                integration_time_medium = (uint32_t)p_fsm->integration_time_short * acamera_sqrt32( exposure_ratio ) >> 3;
            }
            //integration_time_medium = (uint32_t)p_fsm->integration_time_short * acamera_sqrt32( exposure_ratio ) >> 3;

            //LOG( LOG_ERR, "integration_time_medium %d exposure_ratio %d  sqrt32( exposure_ratio ) %d", integration_time_medium, exposure_ratio/64, acamera_sqrt32( exposure_ratio )<<3 );
            if ( integration_time_medium > integration_time_long_max ) {
                integration_time_medium = integration_time_long_max;
            }
            if ( integration_time_medium > p_fsm->integration_time_long ) {
                integration_time_medium = p_fsm->integration_time_long;
            }
            p_fsm->integration_time_medium = (uint16_t)integration_time_medium;

            time.int_time = p_fsm->integration_time_short;
            time.int_time_M = p_fsm->integration_time_medium;
            time.int_time_L = p_fsm->integration_time_long;
            //LOG( LOG_ERR, " short %d medium %d long %d", p_fsm->integration_time_short, p_fsm->integration_time_medium, p_fsm->integration_time_long );

            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_ALLOC_INTEGRATION_TIME, &time, sizeof( time ) );

            // time is updated in above function, save it.
            p_fsm->integration_time_short = time.int_time;
            p_fsm->integration_time_medium = time.int_time_M;
            p_fsm->integration_time_long = time.int_time_L;

        } else if ( sensor_info.sensor_exp_number == 2 ) {
            time.int_time = p_fsm->integration_time_short;
            time.int_time_L = p_fsm->integration_time_long;

            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_ALLOC_INTEGRATION_TIME, &time, sizeof( time ) );

            // time is updated in above function, save it.
            p_fsm->integration_time_short = time.int_time;
            p_fsm->integration_time_long = time.int_time_L;

        } else {
            time.int_time = is_short_exposure_frame( p_fsm->cmn.isp_base ) ? p_fsm->integration_time_short : p_fsm->integration_time_long;

            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_ALLOC_INTEGRATION_TIME, &time, sizeof( time ) );

            // time is updated in above function, save it.
            if ( is_short_exposure_frame( p_fsm->cmn.isp_base ) )
                p_fsm->integration_time_short = time.int_time;
            else
                p_fsm->integration_time_long = time.int_time;
        }

        p_fsm->exposure_ratio = ( uint16_t )( 64 * (uint32_t)p_fsm->integration_time_long / p_fsm->integration_time_short );
    } else {
        p_fsm->exposure_ratio = 64;

        if ( sensor_info.sensor_exp_number == 4 ) {

            time.int_time = p_fsm->integration_time_short;
            time.int_time_M = p_fsm->integration_time_short;
            time.int_time_M2 = p_fsm->integration_time_short;
            time.int_time_L = p_fsm->integration_time_short;

            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_ALLOC_INTEGRATION_TIME, &time, sizeof( time ) );

            // NOTE: the value of 'p_fsm->integration_time_short' depends on the order
            // that sensor function modify the pointer values, I checked the some functions,
            // and found that the 3rd parameter will be modified at last, so we use the 3rd
            // parameter to save back into the 'p_fsm->integration_time_short' variable.
            p_fsm->integration_time_short = time.int_time_L;

        } else if ( sensor_info.sensor_exp_number == 3 ) {
            time.int_time = p_fsm->integration_time_short;
            time.int_time_M = p_fsm->integration_time_short;
            time.int_time_L = p_fsm->integration_time_short;

            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_ALLOC_INTEGRATION_TIME, &time, sizeof( time ) );

            // time is updated in above function, save it.
            p_fsm->integration_time_short = time.int_time_L;

        } else if ( sensor_info.sensor_exp_number == 2 ) {
            time.int_time = p_fsm->integration_time_short;
            time.int_time_L = p_fsm->integration_time_short;

            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_ALLOC_INTEGRATION_TIME, &time, sizeof( time ) );

            // time is updated in above function, save it.
            p_fsm->integration_time_short = time.int_time_L;
        } else {
            time.int_time = p_fsm->integration_time_short;

            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_ALLOC_INTEGRATION_TIME, &time, sizeof( time ) );

            // time is updated in above function, save it.
            p_fsm->integration_time_short = time.int_time;
        }
    }
}

void cmos_calc_target_gain( cmos_fsm_ptr_t p_fsm )
{
    int32_t target_gain_log2 = 0;
    if ( p_fsm->manual_gain_mode == 1 ) {
        target_gain_log2 = acamera_log2_fixed_to_fixed( p_fsm->manual_gain, 8, LOG2_GAIN_SHIFT );
    } else {
#if EXPOSURE_DRIVES_LONG_INTEGRATION_TIME
        uint32_t wdr_mode = 0;

        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );

        target_gain_log2 = p_fsm->exposure_log2 - acamera_log2_fixed_to_fixed( ( ( wdr_mode == WDR_MODE_LINEAR ) ? p_fsm->integration_time_short : p_fsm->integration_time_long ), 0, LOG2_GAIN_SHIFT );
#else
        target_gain_log2 = p_fsm->exposure_log2 - acamera_log2_fixed_to_fixed( p_fsm->integration_time_short, 0, LOG2_GAIN_SHIFT );
#endif
        p_fsm->manual_gain = acamera_math_exp2( target_gain_log2, LOG2_GAIN_SHIFT, 8 );
    }

    if ( target_gain_log2 < 0 ) {
        target_gain_log2 = 0;
    }
    p_fsm->target_gain_step_log2 = target_gain_log2 - p_fsm->target_gain_log2;
    if ( p_fsm->target_gain_step_log2 < 0 ) {
        p_fsm->target_gain_step_log2 = -p_fsm->target_gain_step_log2;
    }
    p_fsm->target_gain_log2 = target_gain_log2;
}

void cmos_set_exposure_target( cmos_fsm_ptr_t p_fsm, int32_t exposure_log2, uint32_t exposure_ratio )
{
#if defined( ISP_HAS_DEFECT_PIXEL_FSM )

    fsm_param_defect_pixel_info_t dp_info;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_DEFECT_PIXEL_INFO, NULL, 0, &dp_info, sizeof( dp_info ) );

    if ( !dp_info.hp_started )
#endif
    {

        uint32_t wdr_mode = 0;

        fsm_param_sensor_info_t sensor_info;
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );

        // for HDR mode with one exposure we can set new exposure only every second frame
        if ( !( ( ( wdr_mode == WDR_MODE_FS_LIN ) ) && ( sensor_info.sensor_exp_number == 1 ) && !is_short_exposure_frame( p_fsm->cmn.isp_base ) ) ) {
            cmos_control_param_t *param_cmos = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );
            p_fsm->exposure_log2 = exposure_log2;
            p_fsm->exposure_ratio_in = exposure_ratio;
            if ( p_fsm->exposure_ratio_in > (uint32_t)param_cmos->global_max_exposure_ratio * 64 ) {

                p_fsm->exposure_ratio_in = param_cmos->global_max_exposure_ratio * 64;
            }

            LOG( LOG_DEBUG, "Set exposure %d and exposure ratio %d", (unsigned int)exposure_log2, (unsigned int)exposure_ratio );
        }
    }
#ifdef ISP_HAS_SENSOR_FSM
#if USER_MODULE
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_MAX_EXPOSURE_LOG2, NULL, 0, &p_fsm->max_exposure_log2, sizeof( p_fsm->max_exposure_log2 ) );
#else
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CMOS_CONTROL );

    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    param->global_max_sensor_analog_gain = MIN( param->global_max_sensor_analog_gain, sensor_info.again_log2_max >> ( LOG2_GAIN_SHIFT - 5 ) );
    param->global_max_sensor_digital_gain = MIN( param->global_max_sensor_digital_gain, sensor_info.dgain_log2_max >> ( LOG2_GAIN_SHIFT - 5 ) );
    param->global_max_integration_time = MIN( param->global_max_integration_time, sensor_info.integration_time_limit );


    int32_t max_again_log2 = MIN( sensor_info.again_log2_max, (int32_t)param->global_max_sensor_analog_gain << ( LOG2_GAIN_SHIFT - 5 ) );
    int32_t max_dgain_log2 = MIN( sensor_info.dgain_log2_max, (int32_t)param->global_max_sensor_digital_gain << ( LOG2_GAIN_SHIFT - 5 ) );
    int32_t max_isp_gain_log2 = MIN( p_fsm->maximum_isp_digital_gain, (int32_t)param->global_max_isp_digital_gain << ( LOG2_GAIN_SHIFT - 5 ) );
    int32_t max_integration_time_log2 = acamera_log2_fixed_to_fixed( sensor_info.integration_time_limit, 0, LOG2_GAIN_SHIFT );
    p_fsm->max_exposure_log2 = max_again_log2 + max_dgain_log2 + max_isp_gain_log2 + max_integration_time_log2;
#endif // #if USER_MODULE
#endif
}

void cmos_deinit( cmos_fsm_ptr_t p_fsm )
{
    system_spinlock_destroy( p_fsm->exp_lock );
}
