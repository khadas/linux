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

#include <linux/module.h>
#include "acamera_fw.h"
#include "acamera_math.h"
#include "acamera_math.h"
#include "acamera_isp_config.h"
#include "system_timer.h"

#include "acamera_logger.h"
#include "sensor_fsm.h"

#include "acamera.h"
#include "system_am_md.h"
#include "system_am_flicker.h"
#include "system_am_decmpr.h"

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

unsigned int temper3_4k = 0;
module_param(temper3_4k, uint, 0664);
MODULE_PARM_DESC(temper3_4k, "\n temper3 4k enable\n");

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
    }
#endif

    return 0;
}

void sensor_hw_init( sensor_fsm_ptr_t p_fsm )
{
    //struct timeval  txs,txe;
    //do_gettimeofday(&txs);

#if FW_DO_INITIALIZATION
    if (p_fsm->p_fsm_mgr->isp_seamless == 0 && p_fsm->p_fsm_mgr->p_ctx->p_gfw->isp_user == 0)
        acamera_isp_input_port_mode_request_write( p_fsm->cmn.isp_base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_STOP ); // urgent stop
#endif                                                                                                               //FW_DO_INITIALIZATION

    // 1): set sensor to preset_mode as required.
    if (p_fsm->p_fsm_mgr->isp_seamless)
    {
        if (acamera_isp_input_port_mode_status_read( 0 ) != ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START)
            p_fsm->ctrl.set_mode( p_fsm->sensor_ctx, p_fsm->preset_mode );
    }
    else
        p_fsm->ctrl.set_mode( p_fsm->sensor_ctx, p_fsm->preset_mode );
    p_fsm->ctrl.disable_sensor_isp( p_fsm->sensor_ctx );
    //do_gettimeofday(&txe);

    //LOG( LOG_INFO, "sensor_hw_init:%d", (txe.tv_sec*1000000+txe.tv_usec) -  (txs.tv_sec*1000000+txs.tv_usec));

    // 2): set to wdr_mode through general router (wdr_mode changed in sensor param in 1st step).
    const sensor_param_t *param = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );

    fsm_param_set_wdr_param_t set_wdr_param;
    set_wdr_param.wdr_mode = param->modes_table[param->mode].wdr_mode;
    set_wdr_param.exp_number = param->modes_table[param->mode].exposures;
    if (p_fsm->p_fsm_mgr->isp_seamless)
    {
        if (acamera_isp_input_port_mode_status_read( 0 ) == ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START)
        {
            if ((param->modes_table[p_fsm->preset_mode].exposures == 2 && set_wdr_param.exp_number == 1) ||
                (param->modes_table[p_fsm->preset_mode].exposures == 1 && set_wdr_param.exp_number == 2) ||
                (param->modes_table[p_fsm->preset_mode].exposures == 1 && set_wdr_param.exp_number == 3))
            {
                    LOG(LOG_CRIT, "preset not match user mode, execute switch and close seamless");
                    p_fsm->ctrl.set_mode( p_fsm->sensor_ctx, p_fsm->preset_mode );
                    p_fsm->p_fsm_mgr->isp_seamless = 0;
            }
            if (param->modes_table[p_fsm->preset_mode].exposures != set_wdr_param.exp_number)
            {
                set_wdr_param.wdr_mode = param->modes_table[p_fsm->preset_mode].wdr_mode;
                set_wdr_param.exp_number = param->modes_table[p_fsm->preset_mode].exposures;
            }
        }
    }
    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_WDR_MODE, &set_wdr_param, sizeof( set_wdr_param ) );
    // 3): Init or update the calibration data.
    acamera_init_calibrations( ACAMERA_FSM2CTX_PTR( p_fsm ), ((sensor_param_t *)(p_fsm->sensor_ctx))->s_name.name );

    // 4). update new settings to ISP if necessary
    //acamera_update_cur_settings_to_isp(0xff);
}

void sensor_configure_buffers( sensor_fsm_ptr_t p_fsm )
{
#if ISP_HAS_TEMPER
    acamera_isp_temper_enable_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 0 );
    aframe_t *temper_frames = ACAMERA_FSM2CTX_PTR( p_fsm )->settings.temper_frames;
    uint32_t temper_frames_num = ACAMERA_FSM2CTX_PTR( p_fsm )->settings.temper_frames_number;
    uint32_t temper_frame_size = acamera_isp_top_active_width_read( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base ) * acamera_isp_top_active_height_read( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base ) * 2;
    if ( temper_frames != NULL && temper_frames_num != 0 && ( ( temper_frames_num > 1 ) ? ( temper_frames[0].size + temper_frames[1].size >= temper_frame_size * 2 ) : ( temper_frames[0].size >= temper_frame_size ) ) ) {
        if ( temper_frames_num == 1 ) {
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
        acamera_isp_temper_dma_line_offset_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, temper_frames[0].line_offset);
        acamera_isp_temper_enable_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 1 );
        acamera_isp_temper_dma_frame_write_on_lsb_dma_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 1 );
        acamera_isp_temper_dma_frame_read_on_lsb_dma_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 1 );

    } else {
        acamera_isp_temper_enable_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 0 );
        acamera_isp_temper_dma_frame_write_on_lsb_dma_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 0 );
        acamera_isp_temper_dma_frame_read_on_lsb_dma_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 0 );
        LOG( LOG_ERR, "No output buffers for temper block provided in settings. Temper is disabled" );
    }

    if ( temper3_4k == 0 ) {
        const sensor_param_t *param = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );
        if ( param->modes_table[p_fsm->preset_mode].resolution.width > 1920 &&  param->modes_table[p_fsm->preset_mode].resolution.height > 1080 )
        {
            LOG(LOG_CRIT, "Resolution: %dx%d > 1080P, close temper3",param->modes_table[p_fsm->preset_mode].resolution.width ,param->modes_table[p_fsm->preset_mode].resolution.height);
            acamera_isp_temper_temper2_mode_write( ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base, 1 );
        }
    }
#endif
}

static void sensor_check_mirror_bayer(sensor_fsm_ptr_t p_fsm)
{
    uint8_t mirror_bypass = acamera_isp_top_bypass_mirror_read(p_fsm->cmn.isp_base);

    if (mirror_bypass == 1)
        return;

    uint8_t bayer = acamera_isp_top_rggb_start_post_mirror_read(p_fsm->cmn.isp_base);
    if (bayer == BAYER_RGGB)
        acamera_isp_top_rggb_start_post_mirror_write( p_fsm->cmn.isp_base, BAYER_GRBG );
    else if (bayer == BAYER_GRBG)
        acamera_isp_top_rggb_start_post_mirror_write( p_fsm->cmn.isp_base, BAYER_RGGB );
    else if (bayer == BAYER_GBRG)
        acamera_isp_top_rggb_start_post_mirror_write( p_fsm->cmn.isp_base, BAYER_BGGR );
    else if (bayer == BAYER_BGGR)
        acamera_isp_top_rggb_start_post_mirror_write( p_fsm->cmn.isp_base, BAYER_GBRG );
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

    sensor_check_mirror_bayer(p_fsm);
}

void sensor_isp_algorithm( sensor_fsm_ptr_t p_fsm, int module_id)
{
#if ( ISP_HAS_FLICKER || ISP_HAS_CMPR || ISP_HAS_MD )
    const sensor_param_t *param = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );
    uint32_t cur_mode = p_fsm->preset_mode;
    uint32_t cur_width = param->modes_table[cur_mode].resolution.width;
    uint32_t cur_height = param->modes_table[cur_mode].resolution.height;
    switch ( module_id ) {
        case FSM_PARAM_SET_SENSOR_MD_EN: {
#if ISP_HAS_MD
            if (p_fsm->md_en)
                am_md_init(0, cur_width, cur_height);
#endif
            break;
        }
        case FSM_PARAM_SET_SENSOR_DECMPR_EN: {
#if ISP_HAS_CMPR
            uint8_t isp_bayer = param->bayer;
            if (p_fsm->is_streaming == 0) {
                if (acamera_isp_temper_temper2_mode_read(ACAMERA_FSM2CTX_PTR( p_fsm )->settings.isp_base) == 0) {
                    aml_cmpr_init(p_fsm->decmpr_en, cur_width, cur_height, p_fsm->lossless_en, isp_bayer);
                    ACAMERA_FSM2CTX_PTR( p_fsm )->isp_decmp_counter = 0;
                }
            }
#endif
            break;
        }
        case FSM_PARAM_SET_SENSOR_FLICKER_EN: {
#if ISP_HAS_FLICKER
            if (p_fsm->flicker_en) {
                aml_flicker_init();
                aml_flicker_start(cur_width, cur_height);
            }
#endif
            break;
        }
        default:
            break;
    }
#endif
}

void sensor_sw_init( sensor_fsm_ptr_t p_fsm )
{
    const sensor_param_t *param = p_fsm->ctrl.get_parameters( p_fsm->sensor_ctx );

#if FW_DO_INITIALIZATION
    if (p_fsm->p_fsm_mgr->isp_seamless == 0) {
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
    }
#endif //FW_DO_INITIALIZATION

    //acamera_isp_input_port_mode_request_write( p_fsm->cmn.isp_base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START );

    sensor_update_black( p_fsm );

    sensor_update_bayer_bits(p_fsm);

    if (p_fsm->p_fsm_mgr->isp_seamless)
    {
        if (acamera_isp_input_port_mode_status_read( 0 ) != ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START)
        {
            acamera_reset_ping_pong_port();
            acamera_update_cur_settings_to_isp(ISP_CONFIG_PING);
        }
    } else {
        if (p_fsm->p_fsm_mgr->p_ctx->p_gfw->isp_user == 0) {
            acamera_reset_ping_pong_port();
            acamera_update_cur_settings_to_isp(ISP_CONFIG_PING);
        }
    }
    LOG( LOG_NOTICE, "Sensor initialization is complete, ID 0x%04X resolution %dx%d", p_fsm->ctrl.get_id( p_fsm->sensor_ctx ), param->active.width, param->active.height );
}

static void sqrt_ext_param_update(sensor_fsm_ptr_t p_fsm)
{
    int32_t rtn = 0;
    int32_t t_gain = 0;
    struct sqrt_ext_param_t p_result;
    fsm_ext_param_ctrl_t p_ctrl;

    acamera_fsm_mgr_get_param(p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_TOTAL_GAIN, NULL, 0, &t_gain, sizeof(t_gain));

    p_ctrl.ctx = (void *)(ACAMERA_FSM2CTX_PTR(p_fsm));
    p_ctrl.id = CALIBRATION_SQRT_EXT_CONTROL;
    p_ctrl.total_gain = t_gain;
    p_ctrl.result = (void *)&p_result;

    rtn = acamera_extern_param_calculate(&p_ctrl);
    if (rtn != 0) {
        LOG(LOG_INFO, "Failed to calculate sqrt ext");
        return;
    }

    acamera_isp_sqrt_black_level_in_write(p_fsm->cmn.isp_base, p_result.black_level_in);
    acamera_isp_sqrt_black_level_out_write(p_fsm->cmn.isp_base, p_result.black_level_out);
}

static void square_be_ext_param_update(sensor_fsm_ptr_t p_fsm)
{
    int32_t rtn = 0;
    int32_t t_gain = 0;
    struct square_be_ext_param_t p_result;
    fsm_ext_param_ctrl_t p_ctrl;

    acamera_fsm_mgr_get_param(p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_TOTAL_GAIN, NULL, 0, &t_gain, sizeof(t_gain));

    p_ctrl.ctx = (void *)(ACAMERA_FSM2CTX_PTR(p_fsm));
    p_ctrl.id = CALIBRATION_SQUARE_BE_EXT_CONTROL;
    p_ctrl.total_gain = t_gain;
    p_ctrl.result = (void *)&p_result;

    rtn = acamera_extern_param_calculate(&p_ctrl);
    if (rtn != 0) {
        LOG(LOG_INFO, "Failed to square be ext");
        return;
    }

    acamera_isp_square_be_black_level_in_write(p_fsm->cmn.isp_base, p_result.black_level_in);
    acamera_isp_square_be_black_level_out_write(p_fsm->cmn.isp_base, p_result.black_level_out);
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

            sqrt_ext_param_update(p_fsm);
            square_be_ext_param_update(p_fsm);
        }

        acamera_isp_digital_gain_offset_write( p_fsm->cmn.isp_base, (uint32_t)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), idx_gr )->y << BLACK_LEVEL_SHIFT_DG );
    } else {
        if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_black_level == 0 ) {
            acamera_isp_sensor_offset_pre_shading_offset_00_write( p_fsm->cmn.isp_base, r << BLACK_LEVEL_SHIFT_WB );
            acamera_isp_sensor_offset_pre_shading_offset_01_write( p_fsm->cmn.isp_base, gr << BLACK_LEVEL_SHIFT_WB );
            acamera_isp_sensor_offset_pre_shading_offset_10_write( p_fsm->cmn.isp_base, gb << BLACK_LEVEL_SHIFT_WB );
            acamera_isp_sensor_offset_pre_shading_offset_11_write( p_fsm->cmn.isp_base, b << BLACK_LEVEL_SHIFT_WB );

            sqrt_ext_param_update(p_fsm);
            square_be_ext_param_update(p_fsm);
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
