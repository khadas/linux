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
#include "sensor_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_SENSOR
#endif

void sensor_fsm_clear( sensor_fsm_t *p_fsm )
{
    p_fsm->mode = ( 0 );
    p_fsm->preset_mode = SENSOR_DEFAULT_PRESET_MODE;
    p_fsm->isp_output_mode = ( ISP_DISPLAY_MODE );
    p_fsm->is_streaming = ( 0 );
    p_fsm->info_preset_num = SENSOR_DEFAULT_PRESET_MODE;
    p_fsm->boot_status = sensor_boot_init( p_fsm );
    p_fsm->preset_mode = ((sensor_param_t *)(p_fsm->sensor_ctx))->mode;
    p_fsm->md_en = 0;
    p_fsm->decmpr_en = 0;
    p_fsm->flicker_en = 0;
    p_fsm->lossless_en = 0;
}

void sensor_request_interrupt( sensor_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}


void sensor_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    sensor_fsm_t *p_fsm = (sensor_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    sensor_fsm_clear( p_fsm );

    sensor_hw_init( p_fsm );
    sensor_sw_init( p_fsm );
    sensor_configure_buffers( p_fsm );
    fsm_raise_event( p_fsm, event_id_sensor_ready );
}

int sensor_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    sensor_fsm_t *p_fsm = (sensor_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_SENSOR_STREAMING:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->is_streaming;

        break;

    case FSM_PARAM_GET_SENSOR_LINES_SECOND:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = sensor_get_lines_second( p_fsm );

        break;

    case FSM_PARAM_GET_SENSOR_INFO: {
        const sensor_param_t *param;
        if ( !output || output_size != sizeof( fsm_param_sensor_info_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        param = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );
        fsm_param_sensor_info_t *p_sensor_info = (fsm_param_sensor_info_t *)output;

        p_sensor_info->total_width = param->total.width;
        p_sensor_info->total_height = param->total.height;
        p_sensor_info->active_width = param->active.width;
        p_sensor_info->active_height = param->active.height;
        p_sensor_info->pixels_per_line = param->pixels_per_line;
        p_sensor_info->lines_per_second = param->lines_per_second;

        p_sensor_info->again_log2_max = param->again_log2_max;
        p_sensor_info->dgain_log2_max = param->dgain_log2_max;
        p_sensor_info->integration_time_min = param->integration_time_min;
        p_sensor_info->integration_time_max = param->integration_time_max;
        p_sensor_info->integration_time_long_max = param->integration_time_long_max;
        p_sensor_info->integration_time_limit = param->integration_time_limit;

        p_sensor_info->integration_time_apply_delay = param->integration_time_apply_delay;
        p_sensor_info->sensor_exp_number = param->sensor_exp_number;
        p_sensor_info->isp_exposure_channel_delay = param->isp_exposure_channel_delay;

        p_sensor_info->isp_output_mode = p_fsm->isp_output_mode;
        p_sensor_info->resolution_mode = p_fsm->mode;
        p_sensor_info->black_level = p_fsm->black_level;
        p_sensor_info->sensor_bits = param->modes_table[param->mode].bits;
        break;
    }

    case FSM_PARAM_GET_SENSOR_PARAM:
        if ( !output || output_size != sizeof( sensor_param_t ** ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *( (const sensor_param_t **)output ) = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );

        break;

    case FSM_PARAM_GET_SENSOR_INFO_PRESET_NUM:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->info_preset_num;

        break;

    case FSM_PARAM_GET_SENSOR_REG:
        if ( !input || input_size != sizeof( uint32_t ) ||
             !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->ctrl.read_sensor_register( p_fsm->sensor_ctx, *(uint32_t *)input );
        break;

    case FSM_PARAM_GET_SENSOR_ID:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->ctrl.get_id( p_fsm->sensor_ctx );
        break;

    case FSM_PARAM_GET_SENSOR_MAX_RESOLUTION: {
        image_resolution_t *max_res = (image_resolution_t *)output;
        image_resolution_t *cur_res;
        const sensor_param_t *sensor_param = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );
        sensor_mode_t *modes_table = sensor_param->modes_table;
        uint32_t max_size = 0, size_res;
        int i;

        if ( !max_res || output_size != sizeof( *max_res ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        if (!modes_table) {
            rc = -1;
            break;
        }

        for (i = 0; i < sensor_param->modes_num; i++) {
            cur_res = &modes_table[i].resolution;
            size_res = (uint32_t)cur_res->width * cur_res->height;
            if (size_res > max_size) {
                max_size = size_res;
                max_res->width = cur_res->width;
                max_res->height = cur_res->height;
            }
        }

        break;
    }

    case FSM_PARAM_GET_SENSOR_VMAX_FPS: {
        if ( !output || output_size != sizeof( uint32_t ) ) {
                LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
                rc = -1;
                break;
        }

        rc = 0;
        *(uint32_t *)output = p_fsm->ctrl.vmax_fps(p_fsm->sensor_ctx, rc);

        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}

int sensor_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    sensor_fsm_t *p_fsm = (sensor_fsm_t *)fsm;
    acamera_context_ptr_t ctx_ptr = NULL;

    switch ( param_id ) {
    case FSM_PARAM_SET_SENSOR_STREAMING: {
        uint32_t streaming;
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        streaming = *(uint32_t *)input;

        if (streaming == 1) {
            p_fsm->ctrl.start_streaming( p_fsm->sensor_ctx );
        } else if(streaming == 0){
            p_fsm->ctrl.stop_streaming( p_fsm->sensor_ctx );
        }
        p_fsm->is_streaming = streaming;
        break;
    }

    case FSM_PARAM_SET_SENSOR_PRESET_MODE:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        ctx_ptr = ACAMERA_FSM2CTX_PTR(p_fsm);
        ctx_ptr->irq_flag++;
        p_fsm->preset_mode = *(uint32_t *)input;
        p_fsm->info_preset_num = p_fsm->preset_mode;
        sensor_hw_init( p_fsm );
        sensor_configure_buffers( p_fsm );
        sensor_sw_init( p_fsm );
        ctx_ptr->irq_flag--;
        break;

    case FSM_PARAM_SET_SENSOR_INFO_PRESET_NUM:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->info_preset_num = *(uint32_t *)input;

        break;

    case FSM_PARAM_SET_SENSOR_ALLOC_ANALOG_GAIN: {
        if ( !input || input_size != sizeof( int32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        int32_t expect_again = *( (int32_t *)input );

        *( (int32_t *)input ) = p_fsm->ctrl.alloc_analog_gain( p_fsm->sensor_ctx, expect_again );

        break;
    }

    case FSM_PARAM_SET_SENSOR_ALLOC_DIGITAL_GAIN: {
        if ( !input || input_size != sizeof( int32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        int32_t expect_dgain = *( (int32_t *)input );

        *( (int32_t *)input ) = p_fsm->ctrl.alloc_digital_gain( p_fsm->sensor_ctx, expect_dgain );

        break;
    }

    case FSM_PARAM_SET_SENSOR_ALLOC_INTEGRATION_TIME: {
        if ( !input || input_size != sizeof( fsm_param_sensor_int_time_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_sensor_int_time_t *p_time = (fsm_param_sensor_int_time_t *)input;

        p_fsm->ctrl.alloc_integration_time( p_fsm->sensor_ctx, &p_time->int_time, &p_time->int_time_M, &p_time->int_time_L );

        break;
    }

    case FSM_PARAM_SET_SENSOR_UPDATE:

        p_fsm->ctrl.sensor_update( p_fsm->sensor_ctx );

        break;

    case FSM_PARAM_SET_SENSOR_REG: {
        if ( !input || input_size != sizeof( fsm_param_reg_cfg_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_reg_cfg_t *p_cfg = (fsm_param_reg_cfg_t *)input;
        p_fsm->ctrl.write_sensor_register( p_fsm->sensor_ctx, p_cfg->reg_addr, p_cfg->reg_value );
        break;
    }
    case FSM_PARAM_SET_SENSOR_TEST_PATTERN:
        if ( !input ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }
        uint8_t mode = *( (uint8_t *)input );

        p_fsm->ctrl.sensor_test_pattern( p_fsm->sensor_ctx, mode);

        break;
    case FSM_PARAM_SET_SENSOR_SENSOR_IR_CUT: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }
        int32_t ir_cut_state = *(uint32_t *)input;
        p_fsm->ctrl.ir_cut_set( p_fsm->sensor_ctx, ir_cut_state);

        break;
    }
    case FSM_PARAM_SET_SENSOR_DCAM_MODE: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }
        int32_t mode = *(uint32_t *)input;
        pr_err("isp set dcam mode:%d", mode);
        p_fsm->ctrl.dcam_mode( p_fsm->sensor_ctx, mode);
        break;
    }
    case FSM_PARAM_SET_SENSOR_MODE_SWITCH: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        ctx_ptr = ACAMERA_FSM2CTX_PTR(p_fsm);

        p_fsm->preset_mode = *(uint32_t *)input;
        p_fsm->info_preset_num = p_fsm->preset_mode;
        p_fsm->ctrl.stop_streaming( p_fsm->sensor_ctx );
        sensor_hw_init( p_fsm );
        sensor_configure_buffers( p_fsm );
        sensor_sw_init( p_fsm );
        p_fsm->ctrl.start_streaming( p_fsm->sensor_ctx );
        acamera_isp_input_port_mode_request_write( p_fsm->cmn.isp_base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START );
        break;
    }
    case FSM_PARAM_SET_SENSOR_VMAX_FPS: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        uint32_t framerate = *(uint32_t *)input;
        p_fsm->ctrl.vmax_fps( p_fsm->sensor_ctx, framerate);

        break;
    }
    case FSM_PARAM_SET_SENSOR_POWER_ON: {

        if ( p_fsm->ctrl.power_on != NULL )
            p_fsm->ctrl.power_on( p_fsm->sensor_ctx);

        break;
    }
    case FSM_PARAM_SET_SENSOR_POWER_OFF: {

        if ( p_fsm->ctrl.power_off != NULL )
            p_fsm->ctrl.power_off( p_fsm->sensor_ctx);

        break;
    }
    case FSM_PARAM_SET_SENSOR_MD_EN: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }
        p_fsm->md_en = *(uint32_t *)input;
        sensor_isp_algorithm( p_fsm, FSM_PARAM_SET_SENSOR_MD_EN );
        break;
    }
    case FSM_PARAM_SET_SENSOR_DECMPR_EN: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }
        p_fsm->decmpr_en = *(uint32_t *)input;
        sensor_isp_algorithm( p_fsm, FSM_PARAM_SET_SENSOR_DECMPR_EN );
        break;
    }
    case FSM_PARAM_SET_SENSOR_FLICKER_EN: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }
        p_fsm->flicker_en = *(uint32_t *)input;
        sensor_isp_algorithm( p_fsm, FSM_PARAM_SET_SENSOR_FLICKER_EN );
        break;
    }
    case FSM_PARAM_SET_SENSOR_DECMPR_LOSSLESS_EN: {
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }
        p_fsm->lossless_en = *(uint32_t *)input;
        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}

uint8_t sensor_fsm_process_event( sensor_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;
    case event_id_frame_end:
        sensor_update_black( p_fsm );
        b_event_processed = 1;
        break;

    case event_id_sensor_sw_reset:
        fsm_raise_event( p_fsm, event_id_sensor_not_ready );
        sensor_sw_init( p_fsm );
        sensor_configure_buffers( p_fsm );
        fsm_raise_event( p_fsm, event_id_sensor_ready );
        b_event_processed = 1;
        break;

    case event_id_acamera_reset_sensor_hw:
        fsm_raise_event( p_fsm, event_id_sensor_ready );
        b_event_processed = 1;
        break;
    }

    return b_event_processed;
}
