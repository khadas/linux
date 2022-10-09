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

#include "acamera_firmware_api.h"
#include "acamera_fw.h"
#include "monitor_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_MONITOR
#endif

void monitor_initialize( monitor_fsm_ptr_t p_fsm )
{
    p_fsm->mask.repeat_irq_mask = ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_END );
    monitor_request_interrupt( p_fsm, p_fsm->mask.repeat_irq_mask );
}

void monitor_handle_reset_error_report( monitor_fsm_ptr_t p_fsm, uint32_t err_type )
{
    switch ( err_type ) {
    case MON_TYPE_ERR_CALIBRATION_LUT_NULL:
        p_fsm->mon_info_cali.err_cnt_calibration_lut_null = 0;
        p_fsm->mon_info_cali.err_param_calibration_lut_null_idx = 0;
        break;

    case MON_TYPE_ERR_CMOS_FS_DELAY:
        p_fsm->mon_info_cmos.err_cnt_cmos_fs_delay = 0;
        break;

    case MON_TYPE_ERR_CMOS_UPDATE_NOT_IN_VB:
        p_fsm->mon_info_cmos.err_cnt_cmos_fe_update_not_in_vb = 0;
        break;

    case MON_TYPE_ERR_CMOS_UPDATE_DGAIN_WRONG_TIMING:
        p_fsm->mon_info_cmos.err_cnt_cmos_dgain_update_wrong_timing = 0;
        p_fsm->mon_info_cmos.err_cmos_dgain_update_wrong_timing_diff = 0;
        break;

    case MON_TYPE_ERR_IRIDIX_UPDATE_NOT_IN_VB:
        p_fsm->mon_info_iridix.err_cnt_iridix_fe_update_not_in_vb = 0;
        break;

    default:
        LOG( LOG_ERR, "MON: Invalid err_type: %d", err_type );
        break;
    }
}

void monitor_handle_error_report( monitor_fsm_ptr_t p_fsm, fsm_param_mon_err_head_t *p_mon_err_head )
{

    switch ( p_mon_err_head->err_type ) {
    case MON_TYPE_ERR_CALIBRATION_LUT_NULL:
        p_fsm->mon_info_cali.err_cnt_calibration_lut_null++;

        // avoid overflow
        if ( 0 == p_fsm->mon_info_cali.err_cnt_calibration_lut_null ) {
            p_fsm->mon_info_cali.err_cnt_calibration_lut_null = 1;
        }

        p_fsm->mon_info_cali.err_param_calibration_lut_null_idx = p_mon_err_head->err_param;

        break;

    case MON_TYPE_ERR_CMOS_FS_DELAY:
        p_fsm->mon_info_cmos.err_cnt_cmos_fs_delay++;
        break;

    case MON_TYPE_ERR_CMOS_UPDATE_NOT_IN_VB:
        p_fsm->mon_info_cmos.err_cnt_cmos_fe_update_not_in_vb++;
        break;

    case MON_TYPE_ERR_CMOS_UPDATE_DGAIN_WRONG_TIMING:
        p_fsm->mon_info_cmos.err_cnt_cmos_dgain_update_wrong_timing++;
        p_fsm->mon_info_cmos.err_cmos_dgain_update_wrong_timing_diff = p_mon_err_head->err_param;
        break;

    case MON_TYPE_ERR_IRIDIX_UPDATE_NOT_IN_VB:
        p_fsm->mon_info_iridix.err_cnt_iridix_fe_update_not_in_vb++;
        break;

    default:
        LOG( LOG_ERR, "MON: Invalid err_type: %d", p_mon_err_head->err_type );
        break;
    }
}

void monitor_get_error_report( monitor_fsm_ptr_t p_fsm, uint32_t err_type, uint32_t *err_param )
{

    switch ( err_type ) {
    case MON_TYPE_ERR_CALIBRATION_LUT_NULL:
        *err_param = p_fsm->mon_info_cali.err_param_calibration_lut_null_idx;
        break;

    case MON_TYPE_ERR_CMOS_FS_DELAY:
        *err_param = p_fsm->mon_info_cmos.err_cnt_cmos_fs_delay;
        break;

    case MON_TYPE_ERR_CMOS_UPDATE_NOT_IN_VB:
        *err_param = p_fsm->mon_info_cmos.err_cnt_cmos_fe_update_not_in_vb;
        break;

    case MON_TYPE_ERR_CMOS_UPDATE_DGAIN_WRONG_TIMING:
        *err_param = ( p_fsm->mon_info_cmos.err_cmos_dgain_update_wrong_timing_diff << 16 ) |
                     ( p_fsm->mon_info_cmos.err_cnt_cmos_dgain_update_wrong_timing );
        break;

    case MON_TYPE_ERR_IRIDIX_UPDATE_NOT_IN_VB:
        *err_param = p_fsm->mon_info_iridix.err_cnt_iridix_fe_update_not_in_vb;
        break;

    default:
        LOG( LOG_ERR, "MON: Invalid err_type: %d", err_type );
        break;
    }
}

static void monitor_dump_alg_state_arr( mon_alg_info_t *p_mon_info )
{
    uint32_t j;
    for ( j = 0; j < ARR_SIZE( p_mon_info->alg_state_arr ); j++ ) {
        LOG( LOG_INFO, "%d). using: %d, frmae_id: %d, in->out->apply: %d->%d->%d.",
             j,
             p_mon_info->alg_state_arr[j].item_is_using,
             p_mon_info->alg_state_arr[j].frame_id_tracking,
             p_mon_info->alg_state_arr[j].frame_id_alg_state_input,
             p_mon_info->alg_state_arr[j].frame_id_alg_state_output,
             p_mon_info->alg_state_arr[j].frame_id_alg_state_apply );
    }
}

void monitor_handle_alg_flow( monitor_fsm_ptr_t p_fsm, mon_alg_info_t *p_mon_info, fsm_param_mon_alg_flow_t *p_flow )
{
    uint32_t i;

    if ( MON_ALG_FLOW_STATE_START == p_flow->flow_state ) {
        i = p_mon_info->alg_arr_write_pos;

        if ( p_mon_info->alg_state_arr[i].item_is_using ) {
            LOG( LOG_DEBUG, "MON_ALG_FLOW %s: overwrite frame: %d, pos: %d.", p_mon_info->alg_name, p_mon_info->alg_state_arr[i].frame_id_tracking, i );
            // monitor_dump_alg_state_arr(p_mon_info);
        }

        p_mon_info->alg_state_arr[i].item_is_using = 1;
        p_mon_info->alg_state_arr[i].frame_id_tracking = p_flow->frame_id_tracking;
        p_mon_info->alg_state_arr[i].frame_id_alg_state_input = p_flow->frame_id_current;

        p_mon_info->alg_arr_write_pos++;

        // wrap to the beginning
        if ( p_mon_info->alg_arr_write_pos >= ARR_SIZE( p_mon_info->alg_state_arr ) )
            p_mon_info->alg_arr_write_pos = 0;

    } else {

        for ( i = 0; i < ARR_SIZE( p_mon_info->alg_state_arr ); i++ ) {
            if ( p_mon_info->alg_state_arr[i].item_is_using && ( p_mon_info->alg_state_arr[i].frame_id_tracking == p_flow->frame_id_tracking ) )
                break;
        }

        if ( i >= ARR_SIZE( p_mon_info->alg_state_arr ) ) {
            LOG( LOG_ERR, "MON_ALG_FLOW %s: Error: can't find frame: %d for state: %d.", p_mon_info->alg_name, p_flow->frame_id_tracking, p_flow->flow_state );
            monitor_dump_alg_state_arr( p_mon_info );

            return;
        }

        if ( MON_ALG_FLOW_STATE_OUTPUT_READY == p_flow->flow_state ) {
            p_mon_info->alg_state_arr[i].frame_id_alg_state_output = p_flow->frame_id_current;

            // calculate FramePerTime
            if ( p_mon_info->alg_fpt_prev_out_id ) {
                uint32_t fpt = p_flow->frame_id_current - p_mon_info->alg_fpt_prev_out_id;

                p_mon_info->alg_fpt_cur = fpt;
                if ( fpt < p_mon_info->alg_fpt_min ) {
                    p_mon_info->alg_fpt_min = fpt;
                    LOG( LOG_ERR, "MON: %s: new min fpt: %d", p_mon_info->alg_name, fpt );
                }

                if ( fpt > p_mon_info->alg_fpt_max ) {
                    p_mon_info->alg_fpt_max = fpt;
                    LOG( LOG_ERR, "MON: %s: new max fpt: %d", p_mon_info->alg_name, fpt );
                }
            } else {
                // Firt Frame: init values
                p_mon_info->alg_fpt_min = 0xFFFF;
                p_mon_info->alg_fpt_max = 0;
            }

            p_mon_info->alg_fpt_prev_out_id = p_flow->frame_id_current;

        } else if ( MON_ALG_FLOW_AE_STATE_END == p_flow->flow_state ) {


            mon_alg_item_t *p_item = &( p_mon_info->alg_state_arr[i] );
            p_item->frame_id_alg_state_apply = p_flow->frame_id_current;

            uint32_t item_delay_in2out = p_item->frame_id_alg_state_output - p_item->frame_id_alg_state_input;
            uint32_t item_delay_out2apply = p_item->frame_id_alg_state_apply - p_item->frame_id_alg_state_output;

            if ( ( item_delay_in2out > 5 ) || ( item_delay_out2apply > 5 ) ) {
                monitor_dump_alg_state_arr( p_mon_info );
            }

            p_item->item_is_using = 0;
            // memset(p_item, 0, sizeof(*p_item));


            p_mon_info->alg_delay_in2out_cur = item_delay_in2out;
            p_mon_info->alg_delay_out2apply_cur = item_delay_out2apply;

            p_mon_info->mon_alg_frame_count++;

            // First frame
            if ( 1 == p_mon_info->mon_alg_frame_count ) {
                p_mon_info->alg_delay_in2out_min = item_delay_in2out;
                p_mon_info->alg_delay_in2out_max = item_delay_in2out;

                p_mon_info->alg_delay_out2apply_min = item_delay_out2apply;
                p_mon_info->alg_delay_out2apply_max = item_delay_out2apply;
            } else {
                if ( item_delay_in2out < p_mon_info->alg_delay_in2out_min )
                    p_mon_info->alg_delay_in2out_min = item_delay_in2out;

                if ( item_delay_in2out > p_mon_info->alg_delay_in2out_max )
                    p_mon_info->alg_delay_in2out_max = item_delay_in2out;

                if ( item_delay_out2apply < p_mon_info->alg_delay_out2apply_min )
                    p_mon_info->alg_delay_out2apply_min = item_delay_out2apply;

                if ( item_delay_out2apply > p_mon_info->alg_delay_out2apply_max )
                    p_mon_info->alg_delay_out2apply_max = item_delay_out2apply;
            }
        }
    }
}

void monitor_set_alg_status( monitor_fsm_ptr_t p_fsm, mon_alg_info_t *p_mon_info, fsm_param_mon_status_head_t *p_status )
{
    switch ( p_status->status_type ) {
    case MON_TYPE_STATUS_ALG_STATUS_RESET:
        LOG( LOG_ERR, "MON: %s: reset param: %d", p_mon_info->alg_name, p_status->status_param );

        if ( p_status->status_param ) {
            p_mon_info->alg_reset_status = 1;

            // Reset to 0xFFFFFFFF which is an invalid value
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
        break;
    default:
        LOG( LOG_ERR, "MON: Invalid status_type: %d", p_status->status_type );
        break;
    }
}

void monitor_get_alg_status( monitor_fsm_ptr_t p_fsm, mon_alg_info_t *p_mon_info, uint32_t status_type, uint32_t *status )
{
    switch ( status_type ) {
    case MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MIN:
        *status = p_mon_info->alg_delay_in2out_min;
        break;

    case MON_TYPE_STATUS_ALG_DELAY_IN2OUT_CUR:
        *status = p_mon_info->alg_delay_in2out_cur;
        break;

    case MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MAX:
        *status = p_mon_info->alg_delay_in2out_max;
        break;

    case MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MIN:
        *status = p_mon_info->alg_delay_out2apply_min;
        break;

    case MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_CUR:
        *status = p_mon_info->alg_delay_out2apply_cur;
        break;

    case MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MAX:
        *status = p_mon_info->alg_delay_out2apply_max;
        break;

    case MON_TYPE_STATUS_ALG_FPT_MIN:
        *status = p_mon_info->alg_fpt_min;
        break;

    case MON_TYPE_STATUS_ALG_FPT_CUR:
        *status = p_mon_info->alg_fpt_cur;
        break;

    case MON_TYPE_STATUS_ALG_FPT_MAX:
        *status = p_mon_info->alg_fpt_max;
        break;

    case MON_TYPE_STATUS_ALG_STATUS_RESET:
        *status = p_mon_info->alg_reset_status;
        break;

    default:
        LOG( LOG_ERR, "MON: Invalid status_type: %d", status_type );
        break;
    }
}
