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
#include "acamera_isp_config.h"
#include "system_timer.h"

#include "acamera_logger.h"
#include "sensor_fsm.h"

#include "acamera.h"

#if USER_MODULE
#include "sbuf.h"
#endif

typedef struct {
    uint16_t active_width;
    uint16_t active_height;
    uint16_t total_width;
    uint16_t total_height;
    uint16_t offset_x;
    uint16_t offset_y;
    uint16_t h_front_porch;
    uint16_t v_front_porch;
    uint16_t h_sync_width;
    uint16_t v_sync_width;
} dvi_sync_param_t;


#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_SENSOR
#endif

void sensor_init_output( sensor_fsm_ptr_t p_fsm, int mode )
{
    const sensor_param_t *param = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );
    if ( ( mode != 720 ) && ( mode != 1080 ) ) {
        if ( param->active.height >= 1080 ) {
            mode = 1080;
        } else {
            mode = 720;
        }
    }

    // decrease output mode to 720p if active_height < mode
    if ( mode > param->active.height )
        mode = 720;

    p_fsm->isp_output_mode = mode;
}

///////////////////////////////////////////////////////////////////////////////

uint32_t sensor_boot_init( sensor_fsm_ptr_t p_fsm )
{
    ACAMERA_FSM2CTX_PTR( p_fsm )
        ->settings.sensor_init( &p_fsm->sensor_ctx, &p_fsm->ctrl );


#if USER_MODULE
    uint32_t idx = 0;
    sensor_param_t *param = (sensor_param_t *)p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );
    struct sensor_info ksensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_KSENSOR_INFO, NULL, 0, &ksensor_info, sizeof( ksensor_info ) );

    param->modes_num = ksensor_info.modes_num;
    if ( param->modes_num > ISP_MAX_SENSOR_MODES ) {
        param->modes_num = ISP_MAX_SENSOR_MODES;
    }

    for ( idx = 0; idx < param->modes_num; idx++ ) {
        param->modes_table[idx] = ksensor_info.modes[idx];

        LOG( LOG_INFO, "Sensor_mode[%d]: wdr_mode: %d, exp: %d.", idx,
             param->modes_table[idx].wdr_mode,
             param->modes_table[idx].exposures );
    }
#endif

    return 0;
}

void sensor_hw_init( sensor_fsm_ptr_t p_fsm )
{

#if FW_DO_INITIALIZATION
    acamera_isp_input_port_mode_request_write( p_fsm->cmn.isp_base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_STOP ); // urgent stop
#endif                                                                                                               //FW_DO_INITIALIZATION

    // 1): set sensor to preset_mode as required.
    p_fsm->ctrl.set_mode( p_fsm->sensor_ctx, p_fsm->preset_mode );
    p_fsm->ctrl.disable_sensor_isp( p_fsm->sensor_ctx );

    // 2): set to wdr_mode through general router (wdr_mode changed in sensor param in 1st step).
    const sensor_param_t *param = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );

    fsm_param_set_wdr_param_t set_wdr_param;
    set_wdr_param.wdr_mode = param->modes_table[param->mode].wdr_mode;
    set_wdr_param.exp_number = param->modes_table[param->mode].exposures;
    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_WDR_MODE, &set_wdr_param, sizeof( set_wdr_param ) );

    // 3): Init or update the calibration data.
    acamera_init_calibrations( ACAMERA_FSM2CTX_PTR( p_fsm ) );

    // 4). update new settings to ISP if necessary
    //acamera_update_cur_settings_to_isp(0xff);
}

void sensor_configure_buffers( sensor_fsm_ptr_t p_fsm )
{
#if ISP_HAS_TEMPER
    acamera_isp_temper_enable_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 0 );
    aframe_t *temper_frames = ACAMERA_FSM2CTX_PTR( p_fsm )->settings.temper_frames;
    uint32_t temper_frames_num = ACAMERA_FSM2CTX_PTR( p_fsm )->settings.temper_frames_number;
    uint32_t temper_frame_size = acamera_isp_top_active_width_read( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base ) * acamera_isp_top_active_height_read( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base ) * 4;
    if ( temper_frames != NULL && temper_frames_num != 0 && ( ( temper_frames_num > 1 ) ? ( temper_frames[0].size + temper_frames[1].size >= temper_frame_size * 2 ) : ( temper_frames[0].size >= temper_frame_size ) ) ) {
        if ( temper_frames_num == 1 ) {
            LOG( LOG_INFO, "Only one output buffer will be used for temper." );
            acamera_isp_temper_dma_lsb_bank_base_reader_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, temper_frames[0].address );
            acamera_isp_temper_dma_lsb_bank_base_writer_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, temper_frames[0].address );
            acamera_isp_temper_temper2_mode_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 1 ); //temper 2
        } else {
            // temper3 has lsb and msb parts
            acamera_isp_temper_dma_lsb_bank_base_reader_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, temper_frames[0].address );
            acamera_isp_temper_dma_lsb_bank_base_writer_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, temper_frames[0].address );
            acamera_isp_temper_dma_msb_bank_base_reader_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, temper_frames[1].address );
            acamera_isp_temper_dma_msb_bank_base_writer_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, temper_frames[1].address );
            acamera_isp_temper_temper2_mode_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 0 ); //temper 3
        }
        acamera_isp_temper_dma_line_offset_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, acamera_isp_top_active_width_read( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base ) * 3 );
        acamera_isp_temper_enable_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 1 );
        acamera_isp_temper_dma_frame_write_on_lsb_dma_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 1 );
        acamera_isp_temper_dma_frame_read_on_lsb_dma_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 1 );

    } else {
        acamera_isp_temper_enable_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 0 );
        acamera_isp_temper_dma_frame_write_on_lsb_dma_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 0 );
        acamera_isp_temper_dma_frame_read_on_lsb_dma_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 0 );
        LOG( LOG_ERR, "No output buffers for temper block provided in settings. Temper is disabled" );
    }
#endif
}


static void sensor_update_bayer_bits(sensor_fsm_ptr_t p_fsm)
{
    const sensor_param_t *param = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );
    uint8_t isp_bayer = param->bayer;
    uint8_t bits = param->modes_table[p_fsm->preset_mode].bits;
    uint8_t isp_bit_width = 1;

    switch (bits) {
    case 8:
        isp_bit_width = 0;
        break;
    case 10:
        isp_bit_width = 1;
        break;
    case 12:
        isp_bit_width = 2;
        break;
    case 14:
        isp_bit_width = 3;
        break;
    case 16:
        isp_bit_width = 4;
        break;
    case 20:
        isp_bit_width = 5;
        break;
    default:
        LOG(LOG_ERR, "Error input bits\n");
        break;
    }

    acamera_isp_top_rggb_start_pre_mirror_write(p_fsm->cmn.isp_base, isp_bayer);
    acamera_isp_top_rggb_start_post_mirror_write(p_fsm->cmn.isp_base, isp_bayer);
    acamera_isp_input_formatter_input_bitwidth_select_write(p_fsm->cmn.isp_base, isp_bit_width);
}


void sensor_sw_init( sensor_fsm_ptr_t p_fsm )
{
    const sensor_param_t *param = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );

#if FW_DO_INITIALIZATION
    /* sensor resolution */
    acamera_isp_top_active_width_write( p_fsm->cmn.isp_base, param->active.width );
    acamera_isp_top_active_height_write( p_fsm->cmn.isp_base, param->active.height );

    acamera_isp_metering_af_active_width_write(p_fsm->cmn.isp_base, param->active.width);
    acamera_isp_metering_af_active_height_write(p_fsm->cmn.isp_base, param->active.height);

    acamera_isp_lumvar_active_width_write(p_fsm->cmn.isp_base, param->active.width);
    acamera_isp_lumvar_active_height_write(p_fsm->cmn.isp_base, param->active.height);

    acamera_isp_input_port_hc_size0_write( p_fsm->cmn.isp_base, param->active.width );
    acamera_isp_input_port_hc_size1_write( p_fsm->cmn.isp_base, param->active.width );
    acamera_isp_input_port_vc_size_write( p_fsm->cmn.isp_base, param->active.height );

    sensor_init_output( p_fsm, p_fsm->isp_output_mode );
#endif //FW_DO_INITIALIZATION

    acamera_isp_input_port_mode_request_write( p_fsm->cmn.isp_base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START );

    sensor_update_black( p_fsm );

    sensor_update_bayer_bits(p_fsm);

    acamera_reset_ping_pong_port();

    acamera_update_cur_settings_to_isp(ISP_CONFIG_PING);

    LOG( LOG_NOTICE, "Sensor initialization is complete, ID 0x%04X resolution %dx%d", p_fsm->ctrl.get_id( p_fsm->sensor_ctx ), param->active.width, param->active.height );
}

void sensor_update_black( sensor_fsm_ptr_t p_fsm )
{
    int32_t stub = 0;
    exposure_set_t exp;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_FRAME_EXPOSURE_SET, &stub, sizeof( stub ), &exp, sizeof( exp ) );

    uint16_t again_log2 = ( uint16_t )( exp.info.again_log2 >> ( LOG2_GAIN_SHIFT - 5 ) ); // makes again in format log2(gain)*32
    uint32_t wdr_mode = 0;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );
    uint32_t idx_r = CALIBRATION_BLACK_LEVEL_R;
    uint32_t idx_b = CALIBRATION_BLACK_LEVEL_B;
    uint32_t idx_gr = CALIBRATION_BLACK_LEVEL_GR;
    uint32_t idx_gb = CALIBRATION_BLACK_LEVEL_GB;
    uint32_t r = acamera_calc_modulation_u16( again_log2, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_r ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_r ) );
    uint32_t b = acamera_calc_modulation_u16( again_log2, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_b ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_b ) );
    uint32_t gr = acamera_calc_modulation_u16( again_log2, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_gr ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_gr ) );
    uint32_t gb = acamera_calc_modulation_u16( again_log2, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_gb ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_gb ) );

    p_fsm->black_level = r;

    if ( wdr_mode == WDR_MODE_FS_LIN ) {
        if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_frame_stitch == 0 ) {
            acamera_isp_frame_stitch_black_level_long_write( p_fsm->cmn.isp_base, p_fsm->black_level );
            acamera_isp_frame_stitch_black_level_medium_write( p_fsm->cmn.isp_base, p_fsm->black_level );
            acamera_isp_frame_stitch_black_level_short_write( p_fsm->cmn.isp_base, p_fsm->black_level );
            acamera_isp_frame_stitch_black_level_very_short_write( p_fsm->cmn.isp_base, p_fsm->black_level );
            acamera_isp_frame_stitch_black_level_out_write( p_fsm->cmn.isp_base, p_fsm->black_level << BLACK_LEVEL_SHIFT_DG );
        }

        if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_black_level == 0 ) {
            acamera_isp_sensor_offset_pre_shading_offset_00_write( p_fsm->cmn.isp_base, (uint32_t)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_r )->y << BLACK_LEVEL_SHIFT_WB );
            acamera_isp_sensor_offset_pre_shading_offset_01_write( p_fsm->cmn.isp_base, (uint32_t)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_gr )->y << BLACK_LEVEL_SHIFT_WB );
            acamera_isp_sensor_offset_pre_shading_offset_10_write( p_fsm->cmn.isp_base, (uint32_t)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_gb )->y << BLACK_LEVEL_SHIFT_WB );
            acamera_isp_sensor_offset_pre_shading_offset_11_write( p_fsm->cmn.isp_base, (uint32_t)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_b )->y << BLACK_LEVEL_SHIFT_WB );
        }

        acamera_isp_digital_gain_offset_write( p_fsm->cmn.isp_base, (uint32_t)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_gr )->y << BLACK_LEVEL_SHIFT_DG );
    } else {
        if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_black_level == 0 ) {
            acamera_isp_sensor_offset_pre_shading_offset_00_write( p_fsm->cmn.isp_base, r << BLACK_LEVEL_SHIFT_WB );
            acamera_isp_sensor_offset_pre_shading_offset_01_write( p_fsm->cmn.isp_base, gr << BLACK_LEVEL_SHIFT_WB );
            acamera_isp_sensor_offset_pre_shading_offset_10_write( p_fsm->cmn.isp_base, gb << BLACK_LEVEL_SHIFT_WB );
            acamera_isp_sensor_offset_pre_shading_offset_11_write( p_fsm->cmn.isp_base, b << BLACK_LEVEL_SHIFT_WB );
        }

        acamera_isp_digital_gain_offset_write( p_fsm->cmn.isp_base, (uint32_t)p_fsm->black_level << BLACK_LEVEL_SHIFT_DG );
    }
}

uint32_t sensor_get_lines_second( sensor_fsm_ptr_t p_fsm )
{
    const sensor_param_t *param = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );
    return param->lines_per_second;
}

void sensor_deinit( sensor_fsm_ptr_t p_fsm )
{
    ACAMERA_FSM2CTX_PTR( p_fsm )
        ->settings.sensor_deinit( p_fsm->sensor_ctx );
}
