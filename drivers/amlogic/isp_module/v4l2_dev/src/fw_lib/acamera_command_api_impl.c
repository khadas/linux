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

#include "acamera.h"
#include "revision.h"
#include "acamera_command_api.h"
#include "acamera_command_api_impl.h"
#include "acamera_fw.h"
#include "acamera_types.h"
#include "acamera_calibrations.h"
#if defined( ISP_HAS_AF_LMS_FSM ) || defined( ISP_HAS_AF_MANUAL_FSM )
#include "acamera_sbus_api.h"
#endif
#include "acamera_math.h"
#if ISP_HAS_GENERAL_FSM
#include "general_fsm.h"
#endif

#include "acamera_isp_config.h"


#if ISP_HAS_FPGA_WRAPPER
#include "acamera_fpga_config.h"
#endif

#include "acamera_fsm_mgr.h"
#include "acamera_types.h"
#include "system_timer.h"
#include "acamera_firmware_api.h"
#include "acamera_firmware_config.h"
#include "system_am_sc.h"

#if ISP_HAS_CMOS_FSM
#include "cmos_fsm.h"
#endif

#if ISP_HAS_MATRIX_YUV_FSM
#include "matrix_yuv_fsm.h" // for PIPE_OUT_RGB
#endif

#include "acamera_logger.h"

#define D1E6 1000000


#ifdef CALIBRATION_DEFECT_PIXELS
extern int32_t defect_pixel_table_write( uint32_t *dp_table, uint32_t size );
extern int32_t defect_pixel_table_read( uint32_t *dp_table, uint32_t size );
#endif

#if FW_HAS_CONTROL_CHANNEL
#include "acamera_ctrl_channel.h"
#endif

#include "fsm_param.h"

extern int32_t acamera_set_api_context( uint32_t ctx_num );
extern int32_t acamera_get_api_context( void );
extern int32_t acamera_get_context_number( void );
extern void *acamera_get_api_ctx_ptr( void );


static uint8_t isp_safe_stop( uint32_t base )
{
    uint16_t sleep_in_us = 2 * 1000;
    uint8_t count = 0;
    uint8_t rc = 0;

    acamera_isp_input_port_mode_request_write( base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_STOP );

    // check whether the HW is stopped or not.
    while ( acamera_isp_input_port_mode_status_read( base ) != ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_STOP ) {
        system_timer_usleep( sleep_in_us );

        // it should be stopped within 1 frame, normally is 33ms for 30 fps.
        // we set the timeout as 100 ms here in case the system_timer is not implemented correctly.
        count++;
        if ( count > 50 ) {
            LOG( LOG_CRIT, "stop fsm_mgr failed, timeout: %u.", (unsigned int)count * sleep_in_us );
            rc = 1;
            break;
        }
    }

    return rc;
}

static uint8_t isp_safe_start( uint32_t base )
{
    uint16_t sleep_in_us = 2 * 1000;
    uint8_t count = 0;
    uint8_t rc = 0;

    acamera_isp_input_port_mode_request_write( base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START );

    // check whether the HW is started or not.
    while ( acamera_isp_input_port_mode_status_read( base ) != ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START ) {
        system_timer_usleep( sleep_in_us );

        // it should be start within 1 frame, normally is 33ms for 30 fps.
        // we set the timeout as 100 ms here in case the system_timer is not implemented correctly.
        count++;
        if ( count > 50 ) {
            LOG( LOG_CRIT, "start fsm_mgr failed, timeout: %u.", (unsigned int)count * sleep_in_us );
            rc = 1;
            break;
        }
    }

    return rc;
}

#ifdef CONTEXT_NUMBER
uint8_t general_context_number( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = NOT_SUPPORTED;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = acamera_get_context_number();
        result = SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif


#ifdef ACTIVE_CONTEXT
uint8_t general_active_context( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = NOT_SUPPORTED;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = acamera_get_api_context();
        result = SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        acamera_set_api_context( value );
        result = SUCCESS;
    }
    return result;
}
#endif


#ifdef DMA_WRITER_SINGLE_FRAME_MODE
uint8_t dma_writer_single_frame( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = NOT_SUPPORTED;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        if ( acamera_isp_fr_dma_writer_single_frame_read( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base ) )
            *ret_value = ON;
        else
            *ret_value = OFF;
        result = SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        if ( value == ON || value == OFF ) {
            acamera_isp_fr_dma_writer_single_frame_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, ( value == ON ) ? 1 : 0 );
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    }
    return result;
}
#endif

#ifdef DMA_RAW_CAPTURE_ENABLED_ID
uint8_t dma_raw_capture_enable_api( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = NOT_SUPPORTED;
    *ret_value = 0;
#if ISP_DMA_RAW_CAPTURE
    if ( direction == COMMAND_SET ) {
        if ( value == ON ) {
            dma_raw_capture_start( ACAMERA_MGR2FIRMWARE_PTR( instance ) );
            result = SUCCESS;
        } else if ( value == OFF ) {
            dma_capture_stop( ACAMERA_MGR2FIRMWARE_PTR( instance ) );
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    } else if ( direction == COMMAND_GET ) {
        if ( dma_raw_capture_status_capture( ACAMERA_MGR2FIRMWARE_PTR( instance ) ) )
            *ret_value = ON;
        else
            *ret_value = OFF;
        result = SUCCESS;
    }
#endif
    return result;
}
#endif

#ifdef DMA_RAW_CAPTURE_WRITEON_ID
uint8_t dma_raw_capture_writeon_api( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = NOT_SUPPORTED;
    *ret_value = 0;
#if ISP_DMA_RAW_CAPTURE
    if ( direction == COMMAND_SET ) {
        if ( value == 1 || value == 0 ) {
            dma_raw_capture_writeon( ACAMERA_MGR2FIRMWARE_PTR( instance ), value );
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    } else if ( direction == COMMAND_GET ) {
        *ret_value = dma_raw_capture_status_writeon( ACAMERA_MGR2FIRMWARE_PTR( instance ) );
        result = SUCCESS;
    }
#endif
    return result;
}
#endif


#ifdef SENSOR_STREAMING
uint8_t sensor_streaming( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;

    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        uint32_t is_streaming = 0;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_STREAMING, NULL, 0, &is_streaming, sizeof( is_streaming ) );
        *ret_value = is_streaming ? ON : OFF;
    } else {
        uint32_t is_streaming = 0;

        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_STREAMING, NULL, 0, &is_streaming, sizeof( is_streaming ) );

        if ( ( value == OFF ) && is_streaming ) {
            uint32_t streaming = 0;
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_SENSOR_STREAMING, &streaming, sizeof( streaming ) );
            isp_safe_stop( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base );
        } else if ( ( value == ON ) && !is_streaming ) {
            uint32_t streaming = 1;

            acamera_reset_ping_pong_port();
            acamera_update_cur_settings_to_isp(ISP_CONFIG_PING);

            acamera_api_dma_buff_get_next(dma_fr);
            acamera_update_cur_settings_to_isp(ISP_CONFIG_PONG);

            isp_safe_start( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base );
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_SENSOR_STREAMING, &streaming, sizeof( streaming ) );
        } else {
            result = NOT_SUPPORTED;
        }
    }

    return result;
}
#endif

#ifdef SENSOR_SUPPORTED_PRESETS
uint8_t sensor_supported_presets( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );

        *ret_value = param->modes_num;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
}
#endif


#ifdef SENSOR_PRESET
uint8_t sensor_preset( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;

    const sensor_param_t *param = NULL;
    acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );

    if ( direction == COMMAND_GET ) {
        *ret_value = param->mode;
        result = SUCCESS;
    } else {
        if ( value < param->modes_num ) {
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_SENSOR_PRESET_MODE, &value, sizeof( value ) );

            isp_safe_stop( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base );

            acamera_fsm_mgr_raise_event( instance, event_id_acamera_reset_sensor_hw );

            result = SUCCESS;
        } else {
            result = FAIL;
        }
    }
    return result;
}
#endif


#ifdef SENSOR_WDR_MODE
uint8_t sensor_wdr_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );

        uint32_t cur_mode = param->mode;
        if ( cur_mode < param->modes_num ) {
            *ret_value = param->modes_table[cur_mode].wdr_mode;
        } else {
            result = FAIL;
        }
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif

#ifdef SENSOR_FPS
uint8_t sensor_fps( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );

        uint32_t cur_mode = param->mode;
        if ( cur_mode < param->modes_num ) {
            *ret_value = param->modes_table[cur_mode].fps;
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif

#ifdef SENSOR_NAME
uint8_t sensor_name( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );

        uint32_t cur_mode = param->mode;
        if ( cur_mode < param->modes_num ) {
            memcpy(ret_value, param->s_name.name, (param->s_name.name_len)*sizeof(char));
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif

#ifdef SENSOR_NAME
uint8_t sensor_test_pattern( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    if ( direction == COMMAND_SET ) {
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_SENSOR_TEST_PATTERN, &value, sizeof( value ) );
        return SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif


#ifdef SENSOR_WIDTH
uint8_t sensor_width( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );
        uint32_t cur_mode = param->mode;
        if ( cur_mode < param->modes_num ) {
            *ret_value = param->modes_table[cur_mode].resolution.width;
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif


#ifdef SENSOR_HEIGHT
uint8_t sensor_height( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );
        uint32_t cur_mode = param->mode;
        if ( cur_mode < param->modes_num ) {
            *ret_value = param->modes_table[cur_mode].resolution.height;
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif

#ifdef SENSOR_EXPOSURES
uint8_t sensor_exposures( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        uint32_t cur_mode;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );
        cur_mode = param->mode;
        if ( cur_mode < param->modes_num ) {
            *ret_value = param->modes_table[cur_mode].exposures;
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif


#ifdef SENSOR_INFO_PRESET
uint8_t sensor_info_preset( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_INFO_PRESET_NUM, NULL, 0, ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        const sensor_param_t *param = NULL;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );

        if ( value < param->modes_num ) {
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_SENSOR_INFO_PRESET_NUM, &value, sizeof( value ) );
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    }
    return result;
}
#endif


#ifdef SENSOR_INFO_WDR_MODE
uint8_t sensor_info_wdr_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        uint32_t cur_mode;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_INFO_PRESET_NUM, NULL, 0, &cur_mode, sizeof( cur_mode ) );
        if ( cur_mode < param->modes_num ) {
            *ret_value = param->modes_table[cur_mode].wdr_mode;
        } else {
            result = FAIL;
        }
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif

#ifdef SENSOR_INFO_FPS
uint8_t sensor_info_fps( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        uint32_t cur_mode;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_INFO_PRESET_NUM, NULL, 0, &cur_mode, sizeof( cur_mode ) );
        if ( cur_mode < param->modes_num ) {
            *ret_value = param->modes_table[cur_mode].fps;
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif

#ifdef SENSOR_INFO_WIDTH
uint8_t sensor_info_width( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        uint32_t cur_mode;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_INFO_PRESET_NUM, NULL, 0, &cur_mode, sizeof( cur_mode ) );
        if ( cur_mode < param->modes_num ) {
            *ret_value = param->modes_table[cur_mode].resolution.width;
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif


#ifdef SENSOR_INFO_HEIGHT
uint8_t sensor_info_height( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        uint32_t cur_mode;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_INFO_PRESET_NUM, NULL, 0, &cur_mode, sizeof( cur_mode ) );
        if ( cur_mode < param->modes_num ) {
            *ret_value = param->modes_table[cur_mode].resolution.height;
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif

#ifdef SENSOR_INFO_EXPOSURES
uint8_t sensor_info_exposures( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        const sensor_param_t *param = NULL;
        uint32_t cur_mode;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_INFO_PRESET_NUM, NULL, 0, &cur_mode, sizeof( cur_mode ) );
        if ( cur_mode < param->modes_num ) {
            *ret_value = param->modes_table[cur_mode].exposures;
            result = SUCCESS;
        } else {
            result = FAIL;
        }
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}
#endif

#ifdef SENSOR_IR_CUT
uint8_t sensor_ir_cut_set( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
	uint32_t ir_cut_state = *ret_value;
	acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_SENSOR_SENSOR_IR_CUT, &ir_cut_state, sizeof( ir_cut_state ) );
	return 0;
}
#endif

#ifdef SYSTEM_FREEZE_FIRMWARE
uint8_t system_freeze_firmware( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_freeze_firmware;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_freeze_firmware = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MANUAL_EXPOSURE
uint8_t system_manual_exposure( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_exposure;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_exposure = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif


// ------------------------------------------------------------------------------ //
// system_exposure_priority description:
//
//     System exposure priority 0: means frame rate is constant 1:frame rate could change
//
//
// ------------------------------------------------------------------------------ //

#ifdef SYSTEM_EXPOSURE_PRIORITY
uint8_t system_exposure_priority( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    const sensor_param_t *param = NULL;
    acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );
    cmos_control_param_t *param_cmos = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_SET ) {
        if ( value == 0 ) {
            param_cmos->global_max_integration_time = param->integration_time_max;
        } else if ( value == 1 ) {
            param_cmos->global_max_integration_time = param->integration_time_limit;
        } else {
            return NOT_SUPPORTED;
        }
        return SUCCESS;
    } else if ( direction == COMMAND_GET ) {
        if ( param_cmos->global_max_integration_time == param->integration_time_max )
            *ret_value = 0;
        else
            *ret_value = 1;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
}
#endif


#ifdef SYSTEM_MANUAL_MAX_INTEGRATION_TIME
uint8_t system_manual_max_integration_time( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_manual_max_integration_time;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        param->global_manual_max_integration_time = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MANUAL_EXPOSURE_RATIO
uint8_t system_manual_exposure_ratio( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_exposure_ratio;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_exposure_ratio = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MANUAL_INTEGRATION_TIME
uint8_t system_manual_integration_time( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_manual_integration_time;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        param->global_manual_integration_time = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MANUAL_SENSOR_ANALOG_GAIN
uint8_t system_manual_sensor_analog_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_manual_sensor_analog_gain;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        param->global_manual_sensor_analog_gain = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN
uint8_t system_manual_sensor_digital_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_manual_sensor_digital_gain;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        param->global_manual_sensor_digital_gain = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MANUAL_ISP_DIGITAL_GAIN
uint8_t system_manual_isp_digital_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_manual_isp_digital_gain;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        param->global_manual_isp_digital_gain = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

// ------------------------------------------------------------------------------ //
// system_iso_gain description:
//
//     base 0: means iso gain is disabled, any other value is enabled with the base
//
//
// ------------------------------------------------------------------------------ //

#ifdef SYSTEM_ISO_GAIN
typedef struct _iso_base_100_gains_t {
    uint32_t iso_base_100_again;
    uint32_t iso_base_100_dgain;
    uint32_t iso_base_100_isp_dgain;
} iso_base_100_gains_t;
uint8_t system_iso_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    static uint32_t iso_base = 0;
    static cmos_control_param_t prev_params = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = iso_base;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        if ( value % 100 == 0 ) {
            cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
            iso_base_100_gains_t *iso_gains = (iso_base_100_gains_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_ISO_100_GAIN );
            if ( iso_gains->iso_base_100_again == 0 || param->global_max_sensor_analog_gain / iso_gains->iso_base_100_again == 0 )
                return FAIL;
            if ( value == 0 && iso_base != 0 ) { //disable iso gain
                *param = prev_params;
                iso_base = 0;
            } else if ( value >= 100 ) { //get analog gain and digital gain value from gain 100
                if ( iso_base == 0 )     //from disabled
                    prev_params = *param;

                param->global_sensor_analog_gain = ( iso_gains->iso_base_100_again ) * ( value / 100 );
                param->global_sensor_digital_gain = iso_gains->iso_base_100_dgain;
                param->global_isp_digital_gain = iso_gains->iso_base_100_isp_dgain;

                if ( param->global_sensor_analog_gain > param->global_max_sensor_analog_gain ) {
                    param->global_sensor_digital_gain += param->global_sensor_analog_gain - param->global_max_sensor_analog_gain;
                    param->global_sensor_analog_gain = param->global_max_sensor_analog_gain;
                }

                if ( param->global_sensor_digital_gain > param->global_max_sensor_digital_gain ) {
                    param->global_isp_digital_gain += param->global_sensor_digital_gain - param->global_max_sensor_digital_gain;
                    param->global_sensor_digital_gain = param->global_max_sensor_digital_gain;
                }

                if ( param->global_isp_digital_gain > param->global_max_isp_digital_gain )
                    param->global_isp_digital_gain = param->global_max_isp_digital_gain;

                param->global_manual_sensor_digital_gain = 1;
                param->global_manual_sensor_analog_gain = 1;
                param->global_manual_isp_digital_gain = 1;
                iso_base = value;
            } else {
                return NOT_SUPPORTED;
            }

            return SUCCESS;
        } else { //only support 100s ISO gains
            return NOT_SUPPORTED
        }
    } else {
        return NOT_SUPPORTED;
    }
}
#endif


#ifdef ISP_MODULES_MANUAL_IRIDIX
uint8_t isp_modules_manual_iridix( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_iridix;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_iridix = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef ISP_MODULES_MANUAL_SINTER
uint8_t isp_modules_manual_sinter( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_sinter;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_sinter = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef ISP_MODULES_MANUAL_TEMPER
uint8_t isp_modules_manual_temper( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_temper;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_temper = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef ISP_MODULES_MANUAL_AUTO_LEVEL
uint8_t isp_modules_manual_auto_level( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_auto_level;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_auto_level = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef ISP_MODULES_MANUAL_FRAME_STITCH
uint8_t isp_modules_manual_frame_stitch( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_frame_stitch;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_frame_stitch = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef ISP_MODULES_MANUAL_RAW_FRONTEND
uint8_t isp_modules_manual_raw_frontend( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_raw_frontend;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_raw_frontend = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef ISP_MODULES_MANUAL_BLACK_LEVEL
uint8_t isp_modules_manual_black_level( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_black_level;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_black_level = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef ISP_MODULES_MANUAL_SHADING
uint8_t isp_modules_manual_shading( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_shading;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_shading = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef ISP_MODULES_MANUAL_DEMOSAIC
uint8_t isp_modules_manual_demosaic( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_demosaic;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_demosaic = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef ISP_MODULES_MANUAL_CNR
uint8_t isp_modules_manual_cnr( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_cnr;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_cnr = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef ISP_MODULES_MANUAL_SHARPEN
uint8_t isp_modules_manual_sharpen( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_sharpen;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_sharpen = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MANUAL_AWB
uint8_t system_manual_awb( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_awb;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_awb = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_ANTIFLICKER_ENABLE
uint8_t system_antiflicker_enable( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_antiflicker_enable;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        param->global_antiflicker_enable = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MANUAL_SATURATION
uint8_t system_manual_saturation( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_saturation;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_manual_saturation = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_EXPOSURE
uint8_t system_exposure( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_exposure;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_exposure = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_EXPOSURE_RATIO
uint8_t system_exposure_ratio( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_exposure_ratio;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_exposure_ratio = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MAX_EXPOSURE_RATIO
uint8_t system_max_exposure_ratio( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_max_exposure_ratio;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        param->global_max_exposure_ratio = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_INTEGRATION_TIME
uint8_t system_integration_time( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    const sensor_param_t *param = NULL;
    acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );
    cmos_control_param_t *param_cmos = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param_cmos->global_integration_time;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
         if ( value == 0 ) {
             param_cmos->global_integration_time = 1;
             LOG(LOG_ERR, "Warning: manual integration time rang: 1 - %d", param->integration_time_limit );
         }
         else if ( param->integration_time_limit < value ) {
             param_cmos->global_integration_time = param->integration_time_limit;
             LOG(LOG_ERR, "Warning: manual integration time rang: 1 - %d", param->integration_time_limit );
         }
         else {
             param_cmos->global_integration_time = value;
         }
         LOG( LOG_ERR, "manual_sensor_integration_time =  %d", param_cmos->global_integration_time );
         return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif


#ifdef SYSTEM_LONG_INTEGRATION_TIME
uint8_t system_long_integration_time( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_long_integration_time;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif


#ifdef SYSTEM_SHORT_INTEGRATION_TIME
uint8_t system_short_integration_time( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_short_integration_time;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif


#ifdef SYSTEM_MAX_INTEGRATION_TIME
uint8_t system_max_integration_time( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_max_integration_time;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        fsm_param_sensor_info_t sensor_info;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

        int32_t max_integration_time = sensor_info.integration_time_limit;
        if ( value > max_integration_time ) {
            param->global_max_integration_time = max_integration_time;
        } else {
            param->global_max_integration_time = value;
        }
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_SENSOR_ANALOG_GAIN
uint8_t system_sensor_analog_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_sensor_analog_gain;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        if ( value > param->global_max_sensor_analog_gain ) {
            param->global_sensor_analog_gain = param->global_max_sensor_analog_gain;
            LOG(LOG_ERR, "Warning: manual sensor analog gain rang: 0 - %d", param->global_max_sensor_analog_gain );
        } else {
            param->global_sensor_analog_gain = value;
        }
        LOG( LOG_ERR, "manual_sensor_analog_gain =  %d", param->global_sensor_analog_gain );
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MAX_SENSOR_ANALOG_GAIN
uint8_t system_max_sensor_analog_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_max_sensor_analog_gain;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        fsm_param_sensor_info_t sensor_info;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );
        int32_t max_manual_again_log2 = sensor_info.again_log2_max >> ( LOG2_GAIN_SHIFT - 5 );
        if ( value > max_manual_again_log2 ) {
            param->global_max_sensor_analog_gain = max_manual_again_log2;
        } else {
            param->global_max_sensor_analog_gain = value;
        }
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_SENSOR_DIGITAL_GAIN
uint8_t system_sensor_digital_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_sensor_digital_gain;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        if ( value > param->global_max_sensor_digital_gain ) {
            param->global_sensor_digital_gain = param->global_max_sensor_digital_gain;
            LOG(LOG_ERR, "Warning: manual sensor digital gain rang: 0 - %d", param->global_max_sensor_digital_gain );
        } else {
            param->global_sensor_digital_gain = value;
        }
        LOG( LOG_ERR, "manual_sensor_digital_gain =  %d", param->global_sensor_digital_gain );
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MAX_SENSOR_DIGITAL_GAIN
uint8_t system_max_sensor_digital_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_max_sensor_digital_gain;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        fsm_param_sensor_info_t sensor_info;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );
        int32_t max_manual_dgain_log2 = sensor_info.dgain_log2_max >> ( LOG2_GAIN_SHIFT - 5 );
        if ( value > max_manual_dgain_log2 ) {
            param->global_max_sensor_digital_gain = max_manual_dgain_log2;
        } else {
            param->global_max_sensor_digital_gain = value;
        }
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_ISP_DIGITAL_GAIN
uint8_t system_isp_digital_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_isp_digital_gain;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        if ( value > param->global_max_isp_digital_gain ) {
            param->global_isp_digital_gain = param->global_max_isp_digital_gain;
            LOG(LOG_ERR, "Warning: manual isp digital gain rang: 0 - %d", param->global_max_isp_digital_gain );
        } else
            param->global_isp_digital_gain = value;
        LOG( LOG_ERR, "manual_isp_digital_gain =  %d", param->global_isp_digital_gain );
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_MAX_ISP_DIGITAL_GAIN
uint8_t system_max_isp_digital_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_max_isp_digital_gain;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        param->global_max_isp_digital_gain = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif


// ------------------------------------------------------------------------------ //
// awb_red_gain description:
//
//   Control the AWB red gain
//   Setting is only available when AWB_MODE is set to {AWB_MANUAL}
//
//   Values:
//    Unsigned 8 bit fixed point value (1.7)
//
//   Format:
//    Integer Part    - [7:7]
//    Fractional Part - [6:0]
//
//   Example:
//   To set a red gain of 1.125: The integer part would be set to 1 and the fraction part to 16 (.125*128).
//   The value 144 ((1 << 7) + 16) would then be passed to the API
// ------------------------------------------------------------------------------ //
#ifdef SYSTEM_AWB_RED_GAIN
uint8_t system_awb_red_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_awb_red_gain;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_awb_red_gain = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

// ------------------------------------------------------------------------------ //
// system_awb_blue_gain description:
//
//   Control the AWB blue gain
//   Setting is only available when AWB_MODE is set to {AWB_MANUAL}
//
//   Values:
//    Unsigned 8 bit fixed point value (1.7)
//
//   Format:
//    Integer Part    - [7:7]
//    Fractional Part - [6:0]
//
//   Example:
//   To set a blue gain of 1.125: The integer part would be set to 1 and the fraction part to 16 (.125*128).
//   The value 144 ((1 << 7) + 16) would then be passed to the API
// ------------------------------------------------------------------------------ //
#ifdef SYSTEM_AWB_BLUE_GAIN
uint8_t system_awb_blue_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_awb_blue_gain;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_awb_blue_gain = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_SATURATION_TARGET
uint8_t system_saturation_target( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_saturation_target;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        ACAMERA_MGR2CTX_PTR( instance )
            ->stab.global_saturation_target = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_ANTI_FLICKER_FREQUENCY
uint8_t system_anti_flicker_frequency( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_GET ) {
        *ret_value = param->global_anti_flicker_frequency;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        param->global_anti_flicker_frequency = value;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif

#ifdef SYSTEM_LOGGER_LEVEL
uint8_t system_logger_level( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    switch ( direction ) {
    case COMMAND_SET:
        ACAMERA_LOGGER_SET_LEVEL( value - DEBUG );
        return SUCCESS;
    case COMMAND_GET:
        *ret_value = acamera_logger_get_level() + DEBUG;
        return SUCCESS;
    default:
        return NOT_IMPLEMENTED;
    }
}
#endif

#ifdef SYSTEM_LOGGER_MASK
uint8_t system_logger_mask( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    switch ( direction ) {
    case COMMAND_SET:
        ACAMERA_LOGGER_SET_MASK( value );
        return SUCCESS;
    case COMMAND_GET:
        *ret_value = acamera_logger_get_mask();
        return SUCCESS;
    default:
        return NOT_IMPLEMENTED;
    }
}
#endif


// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// selftest_fw_revision description:
//
//     Get fw revision
//
//     GET returns fw revision number
// ------------------------------------------------------------------------------ //
uint8_t selftest_fw_revision( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = (uint32_t)FIRMWARE_REVISION;
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
}

// bsp_test_start description:
//
//     Run all BSP tests.
//
//     GET returns DONE otherwise still running
// ------------------------------------------------------------------------------ //

#ifdef BSP_TEST_START
uint8_t bsp_test_start( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
#ifdef ISP_HAS_BSP_TEST_FSM
    uint32_t is_testing = 0;
    acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_BSP_TESTING, NULL, 0, &is_testing, sizeof( is_testing ) );

    if ( direction == COMMAND_SET ) {
        switch ( value ) {
        case RUN:
            if ( is_testing == 0 )
                acamera_fsm_mgr_raise_event( instance, event_id_bsp_test_interrupt_start );
            return SUCCESS;
        default:
            *ret_value = ERR_BAD_ARGUMENT;
            return FAIL;
        }
    } else if ( direction == COMMAND_GET ) {
        *ret_value = ( is_testing == 1 ? RUN : DONE );
        return SUCCESS;
    } else
#endif
    {
        return NOT_SUPPORTED;
    }
}
#endif


// ------------------------------------------------------------------------------ //
//        TSYSTEM
// ------------------------------------------------------------------------------ //

// ------------------------------------------------------------------------------ //
// temper_mode description:
//
//     Controls the mode of temper module
//
//     Values:
//       {TEMPER3_MODE}
//       {TEMPER2_MODE}
//
//     Default Value: {TEMPER2_MODE}
// ------------------------------------------------------------------------------ //
#ifdef TEMPER_MODE_ID
uint8_t temper_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    if ( direction == COMMAND_SET ) {

        acamera_context_ptr_t p_ctx = ACAMERA_MGR2CTX_PTR( instance );

        uint16_t height = acamera_isp_top_active_height_read( p_ctx->settings.isp_base );
        uint32_t temper_line_offset = acamera_isp_temper_dma_line_offset_read( p_ctx->settings.isp_base );
        uint32_t temper_frames_number = p_ctx->settings.temper_frames_number;
        aframe_t *temper_frames = p_ctx->settings.temper_frames;
        uint32_t temper_frame_size = temper_line_offset * height;

        switch ( value ) {
        case TEMPER3_MODE:
            // temper3 needs 2 buffers
            if ( temper_frames_number > 1 && ( temper_frames[0].size + temper_frames[1].size >= temper_frame_size * 2 ) ) {
                acamera_isp_temper_temper2_mode_write( p_ctx->settings.isp_base, 0 );
                acamera_isp_top_bypass_temper_write( p_ctx->settings.isp_base, 0 );
            } else {
                LOG( LOG_ERR, "Bypass temper due to memory configuration is not enough, config: 0x%x, expected: 0x%x.", temper_frames[0].size, temper_frame_size );
                acamera_isp_top_bypass_temper_write( p_ctx->settings.isp_base, 1 );
            }
            break;

        case TEMPER2_MODE:
            if ( temper_frames[0].size >= temper_frame_size ) {
                acamera_isp_temper_temper2_mode_write( p_ctx->settings.isp_base, 1 );
                acamera_isp_top_bypass_temper_write( p_ctx->settings.isp_base, 0 );
            } else {
                LOG( LOG_ERR, "Bypass temper due to memory configuration is not enough, config: 0x%x, expected: 0x%x.", temper_frames[0].size, temper_frame_size );
                acamera_isp_top_bypass_temper_write( p_ctx->settings.isp_base, 1 );
            }
            break;

        default:
            LOG( LOG_ERR, "Invalid parameter %d.", value );
            return FAIL;
        }
        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        switch ( acamera_isp_temper_temper2_mode_read( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base ) ) {
        case 1:
            *ret_value = TEMPER2_MODE;
            return SUCCESS;
            break;
        case 0:
            *ret_value = TEMPER3_MODE;
            return SUCCESS;
            break;
        default:
            return FAIL;
            break;
        }
    }

    return NOT_SUPPORTED;
}
#endif

// ------------------------------------------------------------------------------ //
// test_pattern_enable description:
//
//     Controls the video test pattern generator
//
//     Values:
//       {ON}
//       {OFF}
//
//     Default Value: {OFF}
// ------------------------------------------------------------------------------ //
#ifdef TEST_PATTERN_ENABLE_ID
uint8_t test_pattern_enable( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    if ( direction == COMMAND_SET ) {

        switch ( value ) {
        case ON:
            acamera_isp_video_test_gen_ch0_test_pattern_off_on_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 1 );
            acamera_isp_video_test_gen_ch1_test_pattern_off_on_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 1 );
            acamera_isp_video_test_gen_ch2_test_pattern_off_on_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 1 );
            acamera_isp_video_test_gen_ch3_test_pattern_off_on_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 1 );
            break;
        default:
            acamera_isp_video_test_gen_ch0_test_pattern_off_on_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 0 );
            acamera_isp_video_test_gen_ch1_test_pattern_off_on_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 0 );
            acamera_isp_video_test_gen_ch2_test_pattern_off_on_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 0 );
            acamera_isp_video_test_gen_ch3_test_pattern_off_on_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 0 );
            break;
        }
        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        switch ( acamera_isp_video_test_gen_ch0_test_pattern_off_on_read( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base ) ) {
        case 1:
            *ret_value = ON;
            return SUCCESS;
            break;
        case 0:
            *ret_value = OFF;
            return SUCCESS;
            break;
        default:
            return FAIL;
            break;
        }
    }
    return NOT_SUPPORTED;
}
#endif

// ------------------------------------------------------------------------------ //
// test_pattern description:
//
//     Controls the pattern the test pattern generator shows
//
//     Values:
//       [0-255]
//
//     Key:
//       0=Flat field
//       1=Horizontal gradient
//       2=Vertical Gradient
//       3=Vertical Bars
//     Default Value: 3
// ------------------------------------------------------------------------------ //
uint8_t test_pattern( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    if ( direction == COMMAND_SET ) {
        if ( value <= 5 ) {
            acamera_isp_video_test_gen_ch0_pattern_type_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, value );
            acamera_isp_video_test_gen_ch1_pattern_type_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, value );
            acamera_isp_video_test_gen_ch2_pattern_type_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, value );
            acamera_isp_video_test_gen_ch3_pattern_type_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, value );
            return SUCCESS;
        } else {
            *ret_value = ERR_BAD_ARGUMENT;
            return FAIL;
        }
        return NOT_SUPPORTED;

    } else if ( direction == COMMAND_GET ) {
        *ret_value = acamera_isp_video_test_gen_ch0_pattern_type_read( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base );
        return SUCCESS;
    }

    return NOT_SUPPORTED;
}


// ------------------------------------------------------------------------------ //
//        TIMAGE
// ------------------------------------------------------------------------------ //


// ------------------------------------------------------------------------------ //
// dma_reader_output description:
//
//Set DMA READER output from Full resolution pipe or from downscaler
//
//Values:
//  {DMA_READER_OUT_FR} - output is full resulution pipe
//  {DMA_READER_OUT_DS} - output is downscaler pipe
//
//  Default Value: {DMA_READER_OUT_FR}
// ------------------------------------------------------------------------------ //


#ifdef DMA_READER_OUTPUT_ID

uint8_t dma_reader_output( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
#if defined( ISP_HAS_DMA_WRITER_FSM )
    dma_type dma_reader_out;
    *ret_value = 0;
    switch ( direction ) {
    case COMMAND_SET:
        switch ( value ) {
        case DMA_READER_OUT_FR:
            dma_reader_out = dma_fr;
            break;

#if ISP_HAS_DS1
        case DMA_READER_OUT_DS:
            dma_reader_out = dma_ds1;
            break;
#endif

        default:
            *ret_value = ERR_BAD_ARGUMENT;
            return FAIL;
        }

        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_DMA_READER_OUTPUT, &dma_reader_out, sizeof( dma_reader_out ) );

        return SUCCESS;

    case COMMAND_GET:
        *ret_value = DMA_READER_OUT_FR;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_DMA_READER_OUTPUT, NULL, 0, &dma_reader_out, sizeof( dma_reader_out ) );

        if ( dma_reader_out == dma_fr ) {
            *ret_value = DMA_READER_OUT_FR;
        }

#if ISP_HAS_DS1
        if ( dma_reader_out == dma_ds1 ) {
            *ret_value = DMA_READER_OUT_DS;
        }
#endif

        return SUCCESS;
    default:
        return NOT_IMPLEMENTED;
    }
#else
    return NOT_SUPPORTED;
#endif
}
#endif


// ------------------------------------------------------------------------------ //
// image_resize_type description:
//
//     Sets the type of resize corresponding crop or downscaler.
//   Sets the type of resize so api can switch between types to compensate the lacking read on api functionality
//
//        Values:
//            {CROP_FR} - Control the crop settings on the full resolution pipe.
//          {CROP_DS} - Control the crop settings on the downscaler pipe.
//          {SCALER}  - Control the width and height of the output of the downscaler.
//          {CROP_DS2} - Control the crop settings on the downscaler pipe.
//          {SCALER2}  - Control the width and height of the output of the downscaler.
//
//        Example:
//         To set the DS crop type  set value = CROP_DS.
//
//        Default Value: 0 or unimplemented
//
// ------------------------------------------------------------------------------ //
#ifdef IMAGE_RESIZE_TYPE_ID

uint8_t image_resize_type( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
#ifdef ISP_HAS_CROP_FSM
    int rc;
    uint16_t val = value & 0xFFFF;
    LOG( LOG_DEBUG, "resize type %s %d\n", ( direction == COMMAND_SET ) ? "set" : "get", val );

    if ( direction == COMMAND_SET ) {
        if ( value > 0xFFFF ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_DEBUG, "returning fail on value too big %d\n", (int)value );
            return FAIL;
        }

        fsm_param_crop_setting_t crop_setting;
        crop_setting.flag = CROP_SETTING_BIT_RESIZE_TYPE;
        crop_setting.resize_type = val;
        rc = acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_CROP_SETTING, &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "return fail on bad argument %d\n", val );
            return FAIL;
        }

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        fsm_param_crop_setting_t crop_setting;
        crop_setting.flag = CROP_SETTING_BIT_RESIZE_TYPE;
        rc = acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_CROP_SETTING, &crop_setting, sizeof( crop_setting ), &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "return fail on bad argument %d\n", val );
            return FAIL;
        }

        *ret_value = crop_setting.resize_type;
        return SUCCESS;
    }

    return NOT_IMPLEMENTED;
#else
    return NOT_SUPPORTED;
#endif
}

#endif

// ------------------------------------------------------------------------------ //
// image_resize_enable description:
//
//     Enables or disables corresponding crop or downscaler.
//
//     Values:
//       {CROP_FR} - Control the crop settings on the full resolution pipe.
//       {CROP_DS} - Control the crop settings on the downscaler pipe.
//       {SCALER}  - Control the width and height of the output of the downscaler.
//       {ENABLE}
//       {DISABLE}
//
//     Example:
//      To enable the FR downscaler set value = (CROP_FR<<16) + ENABLE.
//
//     Default Value: {DISABLE}
// ------------------------------------------------------------------------------ //
#ifdef IMAGE_RESIZE_ENABLE_ID

uint8_t image_resize_enable( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
#ifdef ISP_HAS_CROP_FSM
    int rc;
    uint16_t type = value >> 16;
    uint16_t val = value & 0xFFFF;

    LOG( LOG_DEBUG, "enable %s %d %d\n", ( direction == COMMAND_SET ) ? "set" : "get", type, val );

    if ( direction == COMMAND_SET ) {
        if ( val != RUN ) {
            *ret_value = ERR_BAD_ARGUMENT;
            return FAIL;
        }

        fsm_param_crop_setting_t crop_setting;

        crop_setting.flag = CROP_SETTING_BIT_ENABLE;
        crop_setting.resize_type = type;
        crop_setting.enable = 1;
        rc = acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_CROP_SETTING, &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "enable set returning fail on bad argument %d\n", val );
            return FAIL;
        }

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        fsm_param_crop_setting_t crop_setting;
        crop_setting.flag = CROP_SETTING_BIT_DONE;
        crop_setting.resize_type = type;
        rc = acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_CROP_SETTING, &crop_setting, sizeof( crop_setting ), &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "enable get returning fail on bad argument %d\n", val );
            return FAIL;
        }

        *ret_value = ( value & 0xFFFF0000 ) | ( crop_setting.done == 0 ? RUN : DONE );
        return SUCCESS;
    }

    return NOT_IMPLEMENTED;
#else
    return NOT_SUPPORTED;
#endif
}

#endif

// ------------------------------------------------------------------------------ //
// image_resize_width description:
//
//     Set the width of the system selected by IMAGE_CROP_AND_DS_SELECT.
//
//     Values:
//      Context dependent and limited by active image resolution
// ------------------------------------------------------------------------------ //
#ifdef IMAGE_RESIZE_WIDTH_ID

uint8_t image_resize_width( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

#ifdef ISP_HAS_CROP_FSM
    int rc;
    uint16_t type = value >> 16;
    uint16_t val = value & 0xFFFF;

    LOG( LOG_DEBUG, "width %s %d %d\n", ( direction == COMMAND_SET ) ? "set" : "get", type, val );

    if ( direction == COMMAND_SET ) {
        fsm_param_crop_setting_t crop_setting;
        crop_setting.flag = CROP_SETTING_BIT_XSIZE;
        crop_setting.resize_type = type;
        crop_setting.xsize = val;
        rc = acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_CROP_SETTING, &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "return fail on bad argument %d\n", val );
            return FAIL;
        }

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        fsm_param_crop_setting_t crop_setting;
        crop_setting.flag = CROP_SETTING_BIT_XSIZE;
        crop_setting.resize_type = type;
        rc = acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_CROP_SETTING, &crop_setting, sizeof( crop_setting ), &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "return fail on bad argument %d\n", val );
            return FAIL;
        }

        *ret_value = ( value & 0xFFFF0000 ) | crop_setting.xsize;
        return SUCCESS;
    }

    return NOT_IMPLEMENTED;
#else
    return NOT_SUPPORTED;
#endif
}

#endif


// ------------------------------------------------------------------------------ //
// image_resize_height description:
//
//     Set the height of the system selected by IMAGE_CROP_AND_DS_SELECT.
//
//     Values:
//      Context dependent and limited by active image resolution
// ------------------------------------------------------------------------------ //
#ifdef IMAGE_RESIZE_HEIGHT_ID

uint8_t image_resize_height( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

#ifdef ISP_HAS_CROP_FSM
    int rc;
    uint16_t type = value >> 16;
    uint16_t val = value & 0xFFFF;

    LOG( LOG_DEBUG, "height %s %d %d\n", ( direction == COMMAND_SET ) ? "set" : "get", type, val );

    if ( direction == COMMAND_SET ) {
        fsm_param_crop_setting_t crop_setting;
        crop_setting.flag = CROP_SETTING_BIT_YSIZE;
        crop_setting.resize_type = type;
        crop_setting.ysize = val;
        rc = acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_CROP_SETTING, &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "return fail on bad argument %d\n", val );
            return FAIL;
        }

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        fsm_param_crop_setting_t crop_setting;
        crop_setting.flag = CROP_SETTING_BIT_YSIZE;
        crop_setting.resize_type = type;
        rc = acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_CROP_SETTING, &crop_setting, sizeof( crop_setting ), &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "return fail on bad argument %d\n", val );
            return FAIL;
        }

        *ret_value = ( value & 0xFFFF0000 ) | crop_setting.ysize;
        return SUCCESS;
    }

    return NOT_IMPLEMENTED;
#else
    return NOT_SUPPORTED;
#endif
}

#endif


// ------------------------------------------------------------------------------ //
// image_crop_xoffset description:
//
//     Set the x-offset of the system selected by IMAGE_CROP_AND_DS_SELECT.
//     NOTE: this command is not available when IMAGE_CROP_AND_DS_SELECT is set to {SCALER}.
//
//     Values:
//      Context dependent and limited by active image resolution
// ------------------------------------------------------------------------------ //
#ifdef IMAGE_CROP_XOFFSET_ID

uint8_t image_crop_xoffset( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

#ifdef ISP_HAS_CROP_FSM
    int rc;
    uint16_t type = value >> 16;
    uint16_t val = value & 0xFFFF;

    LOG( LOG_DEBUG, "x offset %s %d %d\n", ( direction == COMMAND_SET ) ? "set" : "get", type, val );

    if ( direction == COMMAND_SET ) {
        fsm_param_crop_setting_t crop_setting;
        crop_setting.flag = CROP_SETTING_BIT_XOFFSET;
        crop_setting.resize_type = type;
        crop_setting.xoffset = val;
        rc = acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_CROP_SETTING, &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "return fail on bad argument %d\n", val );
            return FAIL;
        }

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        fsm_param_crop_setting_t crop_setting;
        crop_setting.flag = CROP_SETTING_BIT_XOFFSET;
        crop_setting.resize_type = type;
        rc = acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_CROP_SETTING, &crop_setting, sizeof( crop_setting ), &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "return fail on bad argument %d\n", val );
            return FAIL;
        }

        *ret_value = ( value & 0xFFFF0000 ) | crop_setting.xoffset;
        return SUCCESS;
    }

    return NOT_IMPLEMENTED;
#else
    return NOT_SUPPORTED;
#endif
}

#endif


// ------------------------------------------------------------------------------ //
// image_crop_yoffset description:
//
//     Set the y-offset of the system selected by IMAGE_CROP_AND_DS_SELECT.
//     NOTE: this command is not available when IMAGE_CROP_AND_DS_SELECT is set to {SCALER}.
//
//     Values:
//      Context dependent and limited by active image resolution
// ------------------------------------------------------------------------------ //
#ifdef IMAGE_CROP_YOFFSET_ID

uint8_t image_crop_yoffset( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

#ifdef ISP_HAS_CROP_FSM
    int rc;
    uint16_t type = value >> 16;
    uint16_t val = value & 0xFFFF;

    LOG( LOG_DEBUG, "y offset %s %d %d\n", ( direction == COMMAND_SET ) ? "set" : "get", type, val );

    if ( direction == COMMAND_SET ) {
        fsm_param_crop_setting_t crop_setting;
        crop_setting.flag = CROP_SETTING_BIT_YOFFSET;
        crop_setting.resize_type = type;
        crop_setting.yoffset = val;
        rc = acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_CROP_SETTING, &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "return fail on bad argument %d\n", val );
            return FAIL;
        }

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        fsm_param_crop_setting_t crop_setting;
        crop_setting.flag = CROP_SETTING_BIT_YOFFSET;
        crop_setting.resize_type = type;
        rc = acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_CROP_SETTING, &crop_setting, sizeof( crop_setting ), &crop_setting, sizeof( crop_setting ) );
        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            LOG( LOG_ERR, "return fail on bad argument %d\n", val );
            return FAIL;
        }

        *ret_value = ( value & 0xFFFF0000 ) | crop_setting.yoffset;
        return SUCCESS;
    }

    return NOT_IMPLEMENTED;
#else
    return NOT_SUPPORTED;
#endif
}

#endif


// ------------------------------------------------------------------------------ //
//        TALGORITHMS
// ------------------------------------------------------------------------------ //


// ------------------------------------------------------------------------------ //
// af_mode description:
//
//     Sets the mode of operation for the AF algorithm.
//
//     Values:
//      {AF_AUTO_SINGLE}
//      {AF_AUTO_CONTINUOUS}
//      {AF_HYPER_FOCAL}
//      {AF_INFINITY}
//      {AF_ABORT}
//    {AF_MANUAL}
//
//     Default Value: {AF_AUTO_SINGLE}
// ------------------------------------------------------------------------------ //
#ifdef AF_MODE_ID
uint8_t af_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
#if defined( ISP_HAS_AF_LMS_FSM ) || defined( ISP_HAS_AF_MANUAL_FSM )
    uint32_t mode = AF_MODE_AF;
    if ( direction == COMMAND_SET ) {
        switch ( value ) {
#ifdef AF_AUTO_SINGLE
        case AF_AUTO_SINGLE:
            mode = AF_MODE_AF;
            break;
#endif
#ifdef AF_AUTO_CONTINUOUS
        case AF_AUTO_CONTINUOUS:
            mode = AF_MODE_CAF;
            break;
#endif
#ifdef AF_MANUAL
        case AF_MANUAL:
            mode = AF_MODE_MANUAL;
            break;
#endif
#ifdef AF_CALIBRATION
        case AF_CALIBRATION:
            mode = AF_MODE_CALIBRATION;
            break;
#endif
        default:
            return NOT_SUPPORTED;
        }

        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_AF_MODE, &mode, sizeof( mode ) );

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {

        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AF_MODE, NULL, 0, &mode, sizeof( mode ) );

        switch ( mode ) {
#ifdef AF_AUTO_SINGLE
        case AF_MODE_AF:
            *ret_value = AF_AUTO_SINGLE;
            return SUCCESS;
            break;
#endif
#ifdef AF_AUTO_CONTINUOUS
        case AF_MODE_CAF:
            *ret_value = AF_AUTO_CONTINUOUS;
            return SUCCESS;
            break;
#endif
#ifdef AF_MANUAL
        case AF_MODE_MANUAL:
            *ret_value = AF_MANUAL;
            return SUCCESS;
            break;
#endif
#ifdef AF_CALIBRATION
        case AF_MODE_CALIBRATION:
            *ret_value = AF_CALIBRATION;
            return SUCCESS;
            break;
#endif
        default:
            *ret_value = mode;
        }
        return NOT_EXISTS;
    }

    return NOT_SUPPORTED;
#else
    return NOT_SUPPORTED;
#endif
}
#endif


#ifdef AF_LENS_STATUS
uint8_t af_lens_status( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_SET ) {
        return NOT_SUPPORTED;
    } else if ( direction == COMMAND_GET ) {
        int32_t lens_driver_ok = 0;

        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AF_LENS_STATUS, NULL, 0, &lens_driver_ok, sizeof( lens_driver_ok ) );

        if ( lens_driver_ok == 0 ) {
            *ret_value = LENS_FAILED;
        } else {
            *ret_value = LENS_SUCCESS;
        }
        return SUCCESS;
    }
    return FAIL;
}
#endif

// ------------------------------------------------------------------------------ //
// af_manual_control description:
//
//   Manually set the focal length, only available when AF_MODE_ID is set to AF_MANUAL.
//
//   Values:
//    [0-255]
//
//   Key:
//    0    - macro mechanical end
//    255  - infinity end
//
// ------------------------------------------------------------------------------ //
#ifdef AF_MANUAL_CONTROL_ID
uint8_t af_manual_control( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
#if defined( ISP_HAS_AF_LMS_FSM ) || defined( ISP_HAS_AF_MANUAL_FSM )
    if ( direction == COMMAND_SET ) {
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_AF_MANUAL_POS, &value, sizeof( value ) );
        return SUCCESS;
    } else if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AF_MANUAL_POS, NULL, 0, ret_value, sizeof( uint32_t ) );
        return SUCCESS;
    }

    return NOT_SUPPORTED;
#else
    return NOT_SUPPORTED;
#endif
}
#endif

// ------------------------------------------------------------------------------ //
// af_range_low description:
//
//     Sets the lowest value that the AF algorithm can select.
//
//     Values:
//      [0-255]
//
//     Key:
//      0    - macro mechanical end
//      255  - infinity end
//
//     Default Value:
//      {AF_RANGE_LOW}  = 0
//      {AF_RANGE_HIGH} = 255
// ------------------------------------------------------------------------------ //
#ifdef AF_RANGE_LOW_ID
uint8_t af_range_low( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
#if defined( ISP_HAS_AF_LMS_FSM ) || defined( ISP_HAS_AF_MANUAL_FSM )
    if ( direction == COMMAND_SET ) {
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_AF_RANGE_LOW, &value, sizeof( value ) );
        return SUCCESS;
    } else if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AF_RANGE_LOW, NULL, 0, ret_value, sizeof( uint32_t ) );
        return SUCCESS;
    }

    return NOT_SUPPORTED;
#else
    return NOT_SUPPORTED;
#endif
}
#endif


// ------------------------------------------------------------------------------ //
// af_range_high description:
//
//     Sets the highest value that the AF algorithm can select.
//
//     Values:
//      [0-255]
//
//     Key:
//      0    - macro mechanical end
//      255  - infinity end
//
//     Default Value:
//      {AF_RANGE_LOW}  = 0
//      {AF_RANGE_HIGH} = 255
// ------------------------------------------------------------------------------ //
#ifdef AF_RANGE_HIGH_ID
uint8_t af_range_high( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
#if defined( ISP_HAS_AF_LMS_FSM ) || defined( ISP_HAS_AF_MANUAL_FSM )
    if ( direction == COMMAND_SET ) {
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_AF_RANGE_HIGH, &value, sizeof( value ) );
        return SUCCESS;
    } else if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AF_RANGE_HIGH, NULL, 0, ret_value, sizeof( uint32_t ) );
        return SUCCESS;
    }

    return NOT_SUPPORTED;
#else
    return NOT_SUPPORTED;
#endif
}
#endif

// ------------------------------------------------------------------------------ //
// af_roi description:
//
//     Select which zones are used to gather AF statistics.
//     The region of interest is defined as a rectangle with top-left coordinates (AF_ROI_X1, AF_ROI_Y1) and bottom-right coordinates (AF_ROI_X2, AF_ROI_Y2), with (0,0) defined as the top-left of the image and (255,255) as the bottom right of the image.
//
//     Values:
//      Four 8 bit numbers concatenated into the 32 bit value.
//
//     Format:
//      AF_ROI_X1 [31:24]
//      AF_ROI_Y1 [23:16]
//      AF_ROI_X2 [15:8]
//      AF_ROI_Y2 [7:0]
//
//     Default Values:
//      AF_ROI_X1 = 85
//      AF_ROI_Y1 = 85
//      AF_ROI_X2 = 171
//      AF_ROI_Y2 = 171
// ------------------------------------------------------------------------------ //
#ifdef AF_ROI_ID
uint8_t af_roi( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{

#ifdef ISP_HAS_AF

    if ( direction == COMMAND_SET ) {
        uint8_t x1, y1, x2, y2;

        // Read in values
        y2 = ( value & 0xff );
        x2 = ( ( value >> 8 ) & 0xff );
        y1 = ( ( value >> 16 ) & 0xff );
        x1 = ( ( value >> 24 ) & 0xff );

        if ( x2 <= x1 || y2 <= y1 ) {
            *ret_value = ERR_BAD_ARGUMENT;
            return FAIL;
        }

#if defined( ISP_HAS_DMA_WRITER_FSM )
        uint32_t vflip = 0;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_DMA_VFLIP, NULL, 0, &vflip, sizeof( vflip ) );
        if ( vflip ) {
            uint8_t y = y2;
            y2 = 0xff - y1;
            y1 = 0xff - y;
        }
#endif

        fsm_param_roi_t new_roi;
        new_roi.roi_api = value;
        new_roi.roi = ( (uint32_t)x1 << 24 ) | ( (uint32_t)y1 << 16 ) | ( (uint32_t)x2 << 8 ) | y2;
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_AF_ROI, &new_roi, sizeof( new_roi ) );

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {

        fsm_param_roi_t new_roi;
        new_roi.roi_api = 0;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AF_ROI, NULL, 0, &new_roi, sizeof( new_roi ) );
        *ret_value = new_roi.roi_api;

        return SUCCESS;
    }

    return NOT_SUPPORTED;

#else

    return NOT_SUPPORTED;

#endif
}
#endif


// ------------------------------------------------------------------------------ //
// ae_mode description:
//
//     Set the AE mode
//
//     Values:
//      {AE_AUTO}
//      {AE_FULL_MANUAL}
//
//     Default Value:
//      {AE_AUTO}
// ------------------------------------------------------------------------------ //
#ifdef AE_MODE_ID
uint8_t ae_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t manual_integration_time = 0;
    uint8_t manual_gain_mode = 0;

    *ret_value = 0;

    if ( direction == COMMAND_SET ) {

        switch ( value ) {
        case AE_AUTO:
            manual_integration_time = ( 0 );
            manual_gain_mode = 0;
            break;
        case AE_FULL_MANUAL:
            manual_integration_time = ( 1 );
            manual_gain_mode = 1;
            break;
        case AE_MANUAL_GAIN:
            manual_integration_time = ( 0 );
            manual_gain_mode = 1;
            break;
        case AE_MANUAL_EXPOSURE_TIME:
            manual_integration_time = ( 1 );
            manual_gain_mode = 0;
            break;
        default:
            return NOT_SUPPORTED;
        }

        fsm_param_ae_mode_t ae_mode;

        ae_mode.manual_integration_time = manual_integration_time;
        ae_mode.manual_gain_mode = manual_gain_mode;
        ae_mode.flag = AE_MODE_BIT_MANUAL_INT_TIME | AE_MODE_BIT_MANUAL_GAIN_MODE;

        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_AE_MODE, &ae_mode, sizeof( ae_mode ) );

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {

        fsm_param_ae_mode_t ae_mode;
        ae_mode.flag = AE_MODE_BIT_MANUAL_INT_TIME | AE_MODE_BIT_MANUAL_GAIN_MODE;
        if ( 0 == acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AE_MODE, NULL, 0, &ae_mode, sizeof( ae_mode ) ) ) {
            manual_integration_time = ae_mode.manual_integration_time;
            manual_gain_mode = ae_mode.manual_gain_mode;
        }

        if ( manual_integration_time && manual_gain_mode ) {
            *ret_value = AE_FULL_MANUAL;
        } else if ( manual_integration_time ) {
            *ret_value = AE_MANUAL_EXPOSURE_TIME;
        } else if ( manual_gain_mode ) {
            *ret_value = AE_MANUAL_GAIN;
        } else {
            *ret_value = AE_AUTO;
        }
        return SUCCESS;
    }
    return NOT_SUPPORTED;
}
#endif


#ifdef AE_SPLIT_PRESET_ID
uint8_t ae_split_preset( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    int rc;
    *ret_value = 0;
    if ( direction == COMMAND_SET ) {

        rc = acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_CMOS_SPLIT_STRATEGY, &value, sizeof( value ) );
        if ( rc == 0 ) {
            return SUCCESS;
        } else {
            return NOT_SUPPORTED;
        }
    } else if ( direction == COMMAND_GET ) {

        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_CMOS_SPLIT_STRATEGY, NULL, 0, ret_value, sizeof( *ret_value ) );
        return SUCCESS;
    }

    return NOT_SUPPORTED;
}
#endif


// ------------------------------------------------------------------------------ //
// ae_gain description:
//
//     Control the total gain: (Analogue gain)*(Sensor digital gain)*(ISP digital gain).
//     NOTE: Only gains of 1.0 or above are supported.
//
//     Values:
//      Unsigned 16 bit fixed floating point value (8.8)
//
//     Format:
//      Integer Part    - [15:8]
//      Fractional Part - [7:0]
//
//     Example:
//     To set a gain of 2.25, the integer part is set to 2 and the fractional part to 64 (0.25*256).
//     The value 576 ((2 << 8) + 64) is then passed to AE_GAIN
// ------------------------------------------------------------------------------ //
#ifdef AE_GAIN_ID
uint8_t ae_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
#ifdef ISP_HAS_CMOS_FSM
    if ( direction == COMMAND_SET ) {
        uint8_t manual_gain_mode = 0;
        fsm_param_ae_mode_t ae_mode;
        ae_mode.flag = AE_MODE_BIT_MANUAL_GAIN_MODE;
        if ( 0 == acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AE_MODE, NULL, 0, &ae_mode, sizeof( ae_mode ) ) ) {
            manual_gain_mode = ae_mode.manual_gain_mode;
        }

        if ( !manual_gain_mode ) {
            return NOT_PERMITTED;
        }

        if ( value >= ( 1 << 8 ) ) {
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MANUAL_GAIN, &value, sizeof( value ) );
            return SUCCESS;
        }

        *ret_value = ERR_BAD_ARGUMENT;
        return FAIL;

    } else if ( direction == COMMAND_GET ) {

        fsm_param_gain_calc_param_t param;
        param.shift_in = LOG2_GAIN_SHIFT;
        param.shift_out = 8;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_GAIN, &param, sizeof( param ), ret_value, sizeof( uint32_t ) );

        return SUCCESS;

    } else {

        return NOT_SUPPORTED;
    }
#else
    return NOT_IMPLEMENTED;
#endif
}
#endif


// ------------------------------------------------------------------------------ //
// ae_exposure description:
//
//     Returns exposure in micro sec.
//     Setting exposure is only available when AE_MODE is set to {AE_FULL_MANUAL}
//
//     Values:
//      Unsigned 32 bit value
// ------------------------------------------------------------------------------ //
#ifdef AE_EXPOSURE_ID
uint8_t ae_exposure( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
#ifdef ISP_HAS_CMOS_FSM
    uint64_t large_store;
    uint32_t store;
    uint32_t lines_per_second = 1;

    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SENSOR_LINES_SECOND, NULL, 0, &lines_per_second, sizeof( lines_per_second ) );

    if ( direction == COMMAND_SET ) {

        if ( !param->global_integration_time || !param->global_manual_integration_time ) {
            return NOT_PERMITTED;
        }


        large_store = value;
        large_store *= lines_per_second;
        store = (uint32_t)div64_u64( large_store, D1E6 );

        if ( large_store > ( (uint64_t)D1E6 ) * 0xFFFFFFFF ) {
            return NOT_SUPPORTED;
        } else {
            param->global_integration_time = store;
            return SUCCESS;
        }
    } else if ( direction == COMMAND_GET ) {
        large_store = ( (uint64_t)D1E6 ) * param->global_integration_time;
        if ( lines_per_second != 0 ) {
            *ret_value = div64_u64( large_store, lines_per_second ); // division by zero is checked
        } else {
            *ret_value = 0;
            LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
        }
        return SUCCESS;

    } else {
        return NOT_SUPPORTED;
    }

#else
    return NOT_SUPPORTED;
#endif
}
#endif


// ------------------------------------------------------------------------------ //
// ae_roi description:
//
//     Select which zones are used to gather AE statistics.
//     The region of interest is defined as a rectangle with top-left coordinates (AE_ROI_X1, AE_ROI_Y1) and bottom-right coordinates (AE_ROI_X2, AE_ROI_Y2), with (0,0) defined as the top-left of the image and (255,255) as the bottom right of the image.
//
//     Values:
//      Four 8 bit values concatenated into one 32 bit value.
//
//     Format:
//      AE_ROI_X1 [31:24]
//      AE_ROI_Y1 [23:16]
//      AE_ROI_X2 [15:8]
//      AE_ROI_Y2 [7:0]
//
//     Default Values:
//      AE_ROI_X1 = 0
//      AE_ROI_Y1 = 0
//      AE_ROI_X2 = 255
//      AE_ROI_Y2 = 255
// ------------------------------------------------------------------------------ //
#ifdef AE_ROI_ID
uint8_t ae_roi( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{

    if ( direction == COMMAND_SET ) {
        uint16_t x1, y1, x2, y2;

        // Read in values
        y2 = ( value & 0xff );
        x2 = ( ( value >> 8 ) & 0xff );
        y1 = ( ( value >> 16 ) & 0xff );
        x1 = ( ( value >> 24 ) & 0xff );

        if ( x2 <= x1 || y2 <= y1 ) {
            *ret_value = ERR_BAD_ARGUMENT;
            return FAIL;
        }

#if defined( ISP_HAS_DMA_WRITER_FSM )
        uint32_t vflip = 0;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_DMA_VFLIP, NULL, 0, &vflip, sizeof( vflip ) );
        if ( vflip ) {
            uint8_t y = y2;
            y2 = 0xff - y1;
            y1 = 0xff - y;
        }
#endif

        fsm_param_roi_t new_roi;
        new_roi.roi_api = value;
        new_roi.roi = ( (uint32_t)x1 << 24 ) | ( (uint32_t)y1 << 16 ) | ( (uint32_t)x2 << 8 ) | y2;
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_AE_ROI, &new_roi, sizeof( new_roi ) );

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {

        fsm_param_roi_t new_roi;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AE_ROI, NULL, 0, &new_roi, sizeof( new_roi ) );
        *ret_value = new_roi.roi_api;

        return SUCCESS;

    } else {

        return NOT_SUPPORTED;
    }
}
#endif

uint8_t ae_zone_weight( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
	int ret = -1;

	if (direction == COMMAND_SET) {
		ret = acamera_fsm_mgr_set_param(instance,
					FSM_PARAM_SET_AE_ZONE_WEIGHT,
					ret_value, sizeof(ret_value));
		if (ret != 0)
			return FAIL;
		else
			return SUCCESS;
	} else {
		return NOT_SUPPORTED;
	}
}

uint8_t awb_zone_weight( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
	int ret = -1;

	if (direction == COMMAND_SET) {
		ret = acamera_fsm_mgr_set_param(instance,
					FSM_PARAM_SET_AWB_ZONE_WEIGHT,
					ret_value, sizeof(ret_value));
		if (ret != 0)
			return FAIL;
		else
			return SUCCESS;
	} else {
		return NOT_SUPPORTED;
	}
}

// ------------------------------------------------------------------------------ //
// ae_compensation description:
//
//     Adjust AE compensation to under/over-expose an image
//
//     Values:
//      Unsigned 8 bit value
//
//     Description:
//     A value of 128 is no compensation.
//     Values below 128 are negative, Values above 128 are positive.
//     A step of 4 corresponds to 0.5 dB gain.
//
//     Example:
//     For +1.5 dB compensation the value should be 140 (128+12).
//     For -1.5 dB compensation the value should be 116 (128-12).
//
//     Default Value: 128
// ------------------------------------------------------------------------------ //
#ifdef AE_COMPENSATION_ID
uint8_t ae_compensation( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_SET ) {

        if ( value < ( 1 << 8 ) ) {
            ACAMERA_MGR2CTX_PTR( instance )
                ->stab.global_ae_compensation = ( value );
            return SUCCESS;
        } else {
            return NOT_SUPPORTED;
        }

    } else if ( direction == COMMAND_GET ) {
        *ret_value = ACAMERA_MGR2CTX_PTR( instance )->stab.global_ae_compensation;
        return SUCCESS;
    }
    return NOT_SUPPORTED;
}
#endif

// ------------------------------------------------------------------------------ //
// awb_mode description:
//
//     Select an AWB mode
//
//     Values:
//      {AWB_AUTO}
//      {AWB_MANUAL}
//      {AWB_DAY_LIGHT}
//      {AWB_CLOUDY}
//      {AWB_INCANDESCENT}
//      {AWB_FLOURESCENT}
//      {AWB_TWILIGHT}
//      {AWB_SHADE}
//      {AWB_WARM_FLOURESCENT}
//
//     Default Value: {AWB_AUTO}
// ------------------------------------------------------------------------------ //
#ifdef AWB_MODE_ID

uint8_t awb_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    int rc = 0;
    *ret_value = 0;

    if ( direction == COMMAND_SET ) {
        rc = acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_AWB_MODE, &value, sizeof( value ) );
        if ( rc ) {
            return NOT_SUPPORTED;
        }

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        rc = acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AWB_MODE, NULL, 0, ret_value, sizeof( uint32_t ) );
        if ( rc ) {
            return NOT_SUPPORTED;
        }

        return SUCCESS;
    }

    return NOT_SUPPORTED;
}

#endif

// ------------------------------------------------------------------------------ //
// awb_temperature description:
//
//     Returns the current color temperature being used by the AWB algorithm, in kelvin [K], divided by 100.
//
//     Values:
//      [0-255]
// ------------------------------------------------------------------------------ //
#ifdef AWB_TEMPERATURE_ID
uint8_t awb_temperature( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    if ( direction == COMMAND_SET ) {

        if ( ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_awb ) {
            fsm_param_awb_info_t awb_info;
            awb_info.flag = AWB_INFO_TEMPERATURE_DETECTED;
            awb_info.temperature_detected = value * 100;
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_AWB_INFO, &awb_info, sizeof( awb_info ) );
            return SUCCESS;
        } else {
            return NOT_SUPPORTED;
        }

    } else if ( direction == COMMAND_GET ) {
        fsm_param_awb_info_t awb_info;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AWB_INFO, NULL, 0, &awb_info, sizeof( awb_info ) );
        *ret_value = awb_info.temperature_detected / 100;
        return SUCCESS;
    }

    return NOT_SUPPORTED;
}
#endif


// ------------------------------------------------------------------------------ //
// awb_light_source description:
//
//     Returns the current light source candidate and p_high being used by the AWB algorithm.
//
//   Values:
//      Two 8 bit values concatenated into one 32 bit value.
//
//   Format:
//      AWB_P_HIGH [15:8]
//      AWB_LIGHT_SOURCE [7:0]
//
//   Default Values:
//      AWB_P_HIGH = 50
//      AWB_LIGHT_SOURCE = 3
// ------------------------------------------------------------------------------ //
#ifdef AWB_LIGHT_SOURCE_ID
uint8_t awb_light_source( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    if ( direction == COMMAND_SET ) {

        if ( ACAMERA_MGR2CTX_PTR( instance )->stab.global_manual_awb ) {
            fsm_param_awb_info_t awb_info;
            awb_info.flag = AWB_INFO_P_HIGH | AWB_INFO_LIGHT_SOURCE_CANDIDATE;
            awb_info.p_high = ( value >> 8 ) & 0xFF;
            awb_info.light_source_candidate = value & 0xFF;
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_AWB_INFO, &awb_info, sizeof( awb_info ) );
            return SUCCESS;
        } else {
            return NOT_SUPPORTED;
        }

    } else if ( direction == COMMAND_GET ) {

        fsm_param_awb_info_t awb_info;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_AWB_INFO, NULL, 0, &awb_info, sizeof( awb_info ) );
        *ret_value = ( awb_info.p_high << 8 ) | awb_info.light_source_candidate;

        return SUCCESS;
    }

    return NOT_SUPPORTED;
}
#endif

// ------------------------------------------------------------------------------ //
// antiflicker_mode description:
//
//   Set the frequency for the anti-flicker to remove.
//   (0 - disabled)
//   Default Value: 50HZ
// ------------------------------------------------------------------------------ //
#ifdef ANTIFLICKER_MODE_ID
uint8_t antiflicker_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
    if ( direction == COMMAND_SET ) {
        param->global_anti_flicker_frequency = ( value );
        return SUCCESS;
    } else if ( direction == COMMAND_GET ) {

        *ret_value = param->global_anti_flicker_frequency;
        return SUCCESS;
    }
    return NOT_SUPPORTED;
}
#endif


// ------------------------------------------------------------------------------ //
//    TREGISTERS
// ------------------------------------------------------------------------------ //


// ------------------------------------------------------------------------------ //
// register_address description:
//
//   Set/get the registers address.
//
// ------------------------------------------------------------------------------ //
#ifdef REGISTERS_ADDRESS_ID
uint8_t register_address( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    if ( direction == COMMAND_SET ) {
        fsm_param_reg_setting_t reg;
        reg.flag = REG_SETTING_BIT_REG_ADDR;
        reg.api_reg_addr = value;
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_REG_SETTING, &reg, sizeof( reg ) );
        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        fsm_param_reg_setting_t reg;
        reg.flag = REG_SETTING_BIT_REG_ADDR;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_REG_SETTING, &reg, sizeof( reg ), &reg, sizeof( reg ) );
        *ret_value = reg.api_reg_addr;
        return SUCCESS;

    } else {
        return NOT_SUPPORTED;
    }
}
#endif


// ------------------------------------------------------------------------------ //
// register_size description:
//
//   Set/get the size of the register in bits.
//
//   Values:
//    8
//    16
//    32
//
// ------------------------------------------------------------------------------ //
#ifdef REGISTERS_SIZE_ID
uint8_t register_size( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    if ( direction == COMMAND_SET ) {
        if ( value == 8 || value == 16 || value == 32 ) {
            fsm_param_reg_setting_t reg;
            reg.flag = REG_SETTING_BIT_REG_SIZE;
            reg.api_reg_size = value;
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_REG_SETTING, &reg, sizeof( reg ) );
            return SUCCESS;
        } else {
            *ret_value = ERR_BAD_ARGUMENT;
            return FAIL;
        }

    } else if ( direction == COMMAND_GET ) {
        fsm_param_reg_setting_t reg;
        reg.flag = REG_SETTING_BIT_REG_SIZE;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_REG_SETTING, &reg, sizeof( reg ), &reg, sizeof( reg ) );
        *ret_value = reg.api_reg_size;
        return SUCCESS;

    } else {
        return NOT_SUPPORTED;
    }
}
#endif


// ------------------------------------------------------------------------------ //
// register_source description:
//
//   Set/get the register source.
//
//   Values:
//     {SENSOR}
//     {LENS}
//     {ISP}
//
// ------------------------------------------------------------------------------ //
#ifdef REGISTERS_SOURCE_ID
uint8_t register_source( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    if ( direction == COMMAND_SET ) {
        fsm_param_reg_setting_t reg;
        reg.flag = REG_SETTING_BIT_REG_SOURCE;
        reg.api_reg_source = value;
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_REG_SETTING, &reg, sizeof( reg ) );

        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        fsm_param_reg_setting_t reg;
        reg.flag = REG_SETTING_BIT_REG_SOURCE;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_REG_SETTING, &reg, sizeof( reg ), &reg, sizeof( reg ) );
        *ret_value = reg.api_reg_source;
        return SUCCESS;

    } else {
        return NOT_SUPPORTED;
    }
}
#endif


// ------------------------------------------------------------------------------ //
// register_value description:
//
//   Set/get the register value.
//
//   Example:
//   To read an 8bit register at address 0x3c from the sensor set
//    - SET REGISTERS_ADDRESS_ID to 0x3c
//    - SET REGISTERS_SIZE_ID to 8
//    - SET REGISTERS_SOURCE_ID to SENSOR
//    - GET REGISTERS_VALUE_ID
//
//   To set a 32bit lens register at address 0x1d to be 0x3f0E2B03:
//    - SET REGISTERS_ADDRESS_ID to 0x1d
//    - SET REGISTERS_SIZE_ID to 32
//    - SET REGISTERS_SOURCE_ID to LENS
//    - SET REGISTERS_VALUE_ID to 0x3f0E2B03
// ------------------------------------------------------------------------------ //
#ifdef REGISTERS_VALUE_ID
uint8_t register_value( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    int rc = 0;
    *ret_value = 0;

    if ( direction == COMMAND_SET ) {

        fsm_param_reg_setting_t reg;
        reg.flag = REG_SETTING_BIT_REG_VALUE;
        reg.api_reg_value = value;
        rc = acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_REG_SETTING, &reg, sizeof( reg ) );

        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            return FAIL;
        } else {
            return SUCCESS;
        }

    } else if ( direction == COMMAND_GET ) {

        fsm_param_reg_setting_t reg;
        reg.flag = REG_SETTING_BIT_REG_VALUE;
        rc = acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_REG_SETTING, &reg, sizeof( reg ), &reg, sizeof( reg ) );

        if ( rc ) {
            *ret_value = ERR_BAD_ARGUMENT;
            return FAIL;
        } else {
            *ret_value = reg.api_reg_value;
            return SUCCESS;
        }

    } else {
        return NOT_SUPPORTED;
    }
}
#endif


// ------------------------------------------------------------------------------ //
//        TSCENE_MODES
// ------------------------------------------------------------------------------ //


// ------------------------------------------------------------------------------ //
// sharpening_strength description:
//
//   Control the exact sharpening value.
//
//   Values:
//    [0-255]
//
//   Key:
//    128 - Standard Sharpening
//    <128 - Decreased Sharpening
//    >128 - Increased Sharpening
//
//   Default Value: 128
// ------------------------------------------------------------------------------ //
#ifdef SHARPENING_STRENGTH_ID
uint8_t sharpening_strength( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    if ( direction == COMMAND_SET ) {

        if ( value < 256 ) {
            uint32_t strength = value;
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_SHARPENING_STRENGTH, &strength, sizeof( strength ) );

            return SUCCESS;
        } else {
            return NOT_SUPPORTED;
        }

    } else if ( direction == COMMAND_GET ) {
        uint32_t strength = 0;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_SHARPENING_STRENGTH, NULL, 0, &strength, sizeof( strength ) );
        *ret_value = strength;
        return SUCCESS;
    }
    return NOT_SUPPORTED;
}
#endif
// ------------------------------------------------------------------------------ //
// fr_output_mode description:
//
//   Select the output mode of the ISP.
//
//   Values:
//     {RGB}
//     {YUV444}
//     {YUV422}
//     {YUV420}
//
//   Default Value: {RGB}
// ------------------------------------------------------------------------------ //
#ifdef FR_OUTPUT_MODE_ID
uint8_t fr_output_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t fr_output_fmt = 0;
    *ret_value = 0;
    if ( direction == COMMAND_SET ) {
        switch ( value ) {
        case RGB:
            fr_output_fmt = PIPE_OUT_RGB;
            break;
        case YUV444:
            fr_output_fmt = PIPE_OUT_YUV444;
            break;
        case YUV422:
            fr_output_fmt = PIPE_OUT_YUV422;
            break;
        case YUV420:
            fr_output_fmt = PIPE_OUT_YUV420;
            break;
        default:
            return NOT_SUPPORTED;
        }

        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MATRIX_YUV_FR_OUT_FMT, &fr_output_fmt, sizeof( fr_output_fmt ) );

        return SUCCESS;
    } else if ( direction == COMMAND_GET ) {

        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MATRIX_YUV_FR_OUT_FMT, NULL, 0, &fr_output_fmt, sizeof( fr_output_fmt ) );
        switch ( fr_output_fmt ) {
        case PIPE_OUT_RGB:
            *ret_value = RGB;
            break;
        case PIPE_OUT_YUV444:
            *ret_value = YUV444;
            break;
        case PIPE_OUT_YUV422:
            *ret_value = YUV422;
            break;
        case PIPE_OUT_YUV420:
            *ret_value = YUV420;
            break;
        default:
            return NOT_SUPPORTED;
        }
        return SUCCESS;
    }
    return NOT_IMPLEMENTED;
}
#endif


// ------------------------------------------------------------------------------ //
//Change FR Base and plane select and matrix yuv fsm accordingly
//
//FORMAT = PLANE_SELECT<<6 | BASE
// ------------------------------------------------------------------------------ //
#if defined( FR_FORMAT_BASE_PLANE_ID )
uint8_t fr_format_base_plane( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = NOT_SUPPORTED;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        uint8_t base = acamera_isp_fr_dma_writer_format_read( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base );
        uint8_t base_uv = acamera_isp_fr_uv_dma_writer_format_read( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base );
        switch ( base ) {
        case DMA_FORMAT_DISABLE:
            value = DMA_DISABLE;
            break;
        case DMA_FORMAT_RGB32:
            value = RGB32;
            break;
        case DMA_FORMAT_A2R10G10B10:
            value = A2R10G10B10;
            break;
        case DMA_FORMAT_RGB565:
            value = RGB565;
            break;
        case DMA_FORMAT_RGB24:
            value = RGB24;
            break;
        case DMA_FORMAT_GEN32:
            value = GEN32;
            break;
        case DMA_FORMAT_RAW16:
            value = RAW16;
            break;
        case DMA_FORMAT_RAW12:
            value = RAW12;
            break;
        case DMA_FORMAT_AYUV:
            value = AYUV;
            break;
        case DMA_FORMAT_Y410:
            value = Y410;
            break;
        case DMA_FORMAT_YUY2:
            value = YUY2;
            break;
        case DMA_FORMAT_UYVY:
            value = UYVY;
            break;
        case DMA_FORMAT_Y210:
            value = DMA_FORMAT_Y210;
            break;
        case DMA_FORMAT_NV12_Y:
            if ( base_uv == DMA_FORMAT_NV12_UV )
                value = NV12_YUV;
            else if (base_uv == DMA_FORMAT_NV12_VU)
                value = NV12_YVU;
            else if (base_uv == DMA_FORMAT_DISABLE)
                value = NV12_GREY;
            break;
        case DMA_FORMAT_YV12_Y:
            if ( base_uv == DMA_FORMAT_YV12_U )
                value = YV12_YU;
            else
                value = YV12_YV;
            break;
        default:
            return NOT_SUPPORTED;
        }

        *ret_value = value;
        result = SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        uint8_t base, base_uv = DMA_FORMAT_DISABLE;
        am_sc_set_input_format(value);
        switch ( value ) {
        case DMA_DISABLE:
            base = DMA_FORMAT_DISABLE;
            break;
        case RGB32:
            base = DMA_FORMAT_RGB32;
            break;
        case A2R10G10B10:
            base = DMA_FORMAT_A2R10G10B10;
            break;
        case RGB565:
            base = DMA_FORMAT_RGB565;
            break;
        case RGB24:
            base = DMA_FORMAT_RGB24;
            break;
        case GEN32:
            base = DMA_FORMAT_GEN32;
            break;
        case RAW16:
            base = DMA_FORMAT_RAW16;
            break;
        case RAW12:
            base = DMA_FORMAT_RAW12;
            break;
        case AYUV:
            base = DMA_FORMAT_AYUV;
            break;
        case Y410:
            base = DMA_FORMAT_Y410;
            break;
        case YUY2:
            base = DMA_FORMAT_YUY2;
            break;
        case UYVY:
            base = DMA_FORMAT_UYVY;
            break;
        case Y210:
            base = DMA_FORMAT_Y210;
            break;
        case NV12_YUV:
            base = DMA_FORMAT_NV12_Y;
            base_uv = DMA_FORMAT_NV12_UV;
            break;
        case NV12_YVU:
            base = DMA_FORMAT_NV12_Y;
            base_uv = DMA_FORMAT_NV12_VU;
            break;
        case NV12_GREY:
            base = DMA_FORMAT_NV12_Y;
            base_uv = DMA_FORMAT_DISABLE;
            break;
        case YV12_YU:
            base = DMA_FORMAT_YV12_Y;
            base_uv = DMA_FORMAT_YV12_U;
            break;
        case YV12_YV:
            base = DMA_FORMAT_YV12_Y;
            base_uv = DMA_FORMAT_YV12_V;
            break;
        default:
            return NOT_SUPPORTED;
        }

        //uint8_t base = value & 0xFF;

        uint32_t pipe_output_format = PIPE_OUT_RGB;
        switch ( base & 0xF ) {

        case DMA_FORMAT_AYUV:
        case DMA_FORMAT_Y410:
            pipe_output_format = PIPE_OUT_YUV444;
            break;
        case DMA_FORMAT_YUY2:
        case DMA_FORMAT_UYVY:
        case DMA_FORMAT_Y210:
            pipe_output_format = PIPE_OUT_YUV422;
            break;
        case DMA_FORMAT_NV12_Y:
        case DMA_FORMAT_NV12_UV:
        case DMA_FORMAT_NV12_VU:
            pipe_output_format = PIPE_OUT_YUV420;
            break;
        }

        if (base == DMA_FORMAT_RAW16)
            acamera_isp_top_isp_processing_fr_bypass_mode_write(ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 1);
        else
            acamera_isp_top_isp_processing_fr_bypass_mode_write(ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 0);
        acamera_isp_fr_dma_writer_format_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, base );
        acamera_isp_fr_uv_dma_writer_format_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, base_uv );

        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MATRIX_YUV_FR_OUT_FMT, &pipe_output_format, sizeof( pipe_output_format ) );
        result = SUCCESS;
    }
    return result;
}
#endif

// ------------------------------------------------------------------------------ //
// ds1_output_mode description:
//
//   Select the output mode of the ISP.
//
//   Values:
//     {RGB}
//     {YUV444}
//     {YUV422}
//     {YUV420}
//
//   Default Value: {RGB}
// ------------------------------------------------------------------------------ //
#if defined( DS1_OUTPUT_MODE_ID ) && ( ISP_HAS_DS1 )
uint8_t ds1_output_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t output_fmt = 0;
    *ret_value = 0;
    if ( direction == COMMAND_SET ) {
        switch ( value ) {
        case RGB:
            output_fmt = PIPE_OUT_RGB;
            break;
        case YUV444:
            output_fmt = PIPE_OUT_YUV444;
            break;
        case YUV422:
            output_fmt = PIPE_OUT_YUV422;
            break;
        case YUV420:
            output_fmt = PIPE_OUT_YUV420;
            break;
        default:
            return NOT_SUPPORTED;
        }

        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MATRIX_YUV_DS1_OUT_FMT, &output_fmt, sizeof( output_fmt ) );

        return SUCCESS;
    } else if ( direction == COMMAND_GET ) {

        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MATRIX_YUV_DS1_OUT_FMT, NULL, 0, &output_fmt, sizeof( output_fmt ) );
        switch ( output_fmt ) {
        case PIPE_OUT_RGB:
            *ret_value = RGB;
            break;
        case PIPE_OUT_YUV444:
            *ret_value = YUV444;
            break;
        case PIPE_OUT_YUV422:
            *ret_value = YUV422;
            break;
        case PIPE_OUT_YUV420:
            *ret_value = YUV420;
            break;
        default:
            return NOT_SUPPORTED;
        }
        return SUCCESS;
    }
    return NOT_IMPLEMENTED;
}
#endif

// ------------------------------------------------------------------------------ //
//Change FR Base and plane select and matrix yuv fsm accordingly
//
//FORMAT = PLANE_SELECT<<6 | BASE
// ------------------------------------------------------------------------------ //
#if defined( DS1_FORMAT_BASE_PLANE_ID ) && ( ISP_HAS_DS1 )
uint8_t ds1_format_base_plane( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = NOT_SUPPORTED;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        uint8_t base = acamera_isp_ds1_dma_writer_format_read( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base );
        uint8_t base_uv = acamera_isp_ds1_uv_dma_writer_format_read( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base );
        switch ( base ) {
        case DMA_FORMAT_DISABLE:
            value = DMA_DISABLE;
            break;
        case DMA_FORMAT_RGB32:
            value = RGB32;
            break;
        case DMA_FORMAT_A2R10G10B10:
            value = A2R10G10B10;
            break;
        case DMA_FORMAT_RGB565:
            value = RGB565;
            break;
        case DMA_FORMAT_RGB24:
            value = RGB24;
            break;
        case DMA_FORMAT_GEN32:
            value = GEN32;
            break;
        case DMA_FORMAT_RAW16:
            value = RAW16;
            break;
        case DMA_FORMAT_RAW12:
            value = RAW12;
            break;
        case DMA_FORMAT_AYUV:
            value = AYUV;
            break;
        case DMA_FORMAT_Y410:
            value = Y410;
            break;
        case DMA_FORMAT_YUY2:
            value = YUY2;
            break;
        case DMA_FORMAT_UYVY:
            value = UYVY;
            break;
        case DMA_FORMAT_Y210:
            value = DMA_FORMAT_Y210;
            break;
        case DMA_FORMAT_NV12_Y:
            if ( base_uv == DMA_FORMAT_NV12_UV )
                value = NV12_YUV;
            else if (base_uv == DMA_FORMAT_NV12_VU)
                value = NV12_YVU;
            else if (base_uv == DMA_FORMAT_DISABLE)
                value = NV12_GREY;
            break;
        case DMA_FORMAT_YV12_Y:
            if ( base_uv == DMA_FORMAT_YV12_U )
                value = YV12_YU;
            else
                value = YV12_YV;
            break;
        default:
            return NOT_SUPPORTED;
        }

        *ret_value = value;
        result = SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        uint8_t base, base_uv = DMA_FORMAT_DISABLE;

        switch ( value ) {
        case DMA_DISABLE:
            base = DMA_FORMAT_DISABLE;
            break;
        case RGB32:
            base = DMA_FORMAT_RGB32;
            break;
        case A2R10G10B10:
            base = DMA_FORMAT_A2R10G10B10;
            break;
        case RGB565:
            base = DMA_FORMAT_RGB565;
            break;
        case RGB24:
            base = DMA_FORMAT_RGB24;
            break;
        case GEN32:
            base = DMA_FORMAT_GEN32;
            break;
        case RAW16:
            base = DMA_FORMAT_RAW16;
            break;
        case RAW12:
            base = DMA_FORMAT_RAW12;
            break;
        case AYUV:
            base = DMA_FORMAT_AYUV;
            break;
        case Y410:
            base = DMA_FORMAT_Y410;
            break;
        case YUY2:
            base = DMA_FORMAT_YUY2;
            break;
        case UYVY:
            base = DMA_FORMAT_UYVY;
            break;
        case Y210:
            base = DMA_FORMAT_Y210;
            break;
        case NV12_YUV:
            base = DMA_FORMAT_NV12_Y;
            base_uv = DMA_FORMAT_NV12_UV;
            break;
        case NV12_YVU:
            base = DMA_FORMAT_NV12_Y;
            base_uv = DMA_FORMAT_NV12_VU;
            break;
        case NV12_GREY:
            base = DMA_FORMAT_NV12_Y;
            base_uv = DMA_FORMAT_DISABLE;
            break;
        case YV12_YU:
            base = DMA_FORMAT_YV12_Y;
            base_uv = DMA_FORMAT_YV12_U;
            break;
        case YV12_YV:
            base = DMA_FORMAT_YV12_Y;
            base_uv = DMA_FORMAT_YV12_V;
            break;
        default:
            return NOT_SUPPORTED;
        }

        //uint8_t base = value & 0xFF;

        uint32_t pipe_output_format = PIPE_OUT_RGB;
        switch ( base & 0xF ) {

        case DMA_FORMAT_AYUV:
        case DMA_FORMAT_Y410:
            pipe_output_format = PIPE_OUT_YUV444;
            break;
        case DMA_FORMAT_YUY2:
        case DMA_FORMAT_UYVY:
        case DMA_FORMAT_Y210:
            pipe_output_format = PIPE_OUT_YUV422;
            break;
        case DMA_FORMAT_NV12_Y:
        case DMA_FORMAT_NV12_UV:
        case DMA_FORMAT_NV12_VU:
            pipe_output_format = PIPE_OUT_YUV420;
            break;
        }

        if (base == DMA_FORMAT_RAW16)
            return result;

        acamera_isp_ds1_dma_writer_format_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, base );
        acamera_isp_ds1_uv_dma_writer_format_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, base_uv );

        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MATRIX_YUV_DS1_OUT_FMT, &pipe_output_format, sizeof( pipe_output_format ) );
        result = SUCCESS;
    }
    return result;
}
#endif

#if ISP_HAS_DS2
uint8_t scaler_width(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value)
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = am_sc_get_width();
    } else if (direction == COMMAND_SET){
        //first get fr width as amloigc scaler src width
        uint32_t width_cur = acamera_isp_top_active_width_read(instance->isp_base);
        LOG(LOG_INFO, "FR width = %d, scaler out width = %d", width_cur, value);
        am_sc_set_width(width_cur, value);
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}

uint8_t scaler_height(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value)
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {

    } else if (direction == COMMAND_SET) {
        //first get fr height as amlogic scaler src height
        uint32_t height_cur = acamera_isp_top_active_height_read(instance->isp_base);
        LOG(LOG_INFO, "FR height = %d, scaler out height = %d", height_cur, value);
        am_sc_set_height(height_cur, value);
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}

uint8_t scaler_src_width(acamera_fsm_mgr_t *instance, uint32_t value,
                        uint8_t direction, uint32_t *ret_value)
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {

    } else if (direction == COMMAND_SET) {
        LOG(LOG_ERR, "LIKE:sc src width %d", value);
        am_sc_set_src_width(value);
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}

uint8_t scaler_src_height(acamera_fsm_mgr_t *instance, uint32_t value,
                        uint8_t direction, uint32_t *ret_value)
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {

    } else if (direction == COMMAND_SET) {
        LOG(LOG_ERR, "LIKE:sc src height %d", value);
        am_sc_set_src_height(value);
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}



uint8_t scaler_output_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint32_t result = SUCCESS;
    *ret_value = 0;
    if (direction == COMMAND_SET) {
        //todo list
        am_sc_set_output_format(value);
    } else {
        result = NOT_SUPPORTED;
    }
    return result;
}

void scaler_streaming_on(void)
{
    am_sc_hw_init();
    am_sc_start();
}

void scaler_streaming_off(void)
{
    am_sc_stop();
}

#endif

// ------------------------------------------------------------------------------ //
// brightness_strength description:
//
//   Control the exact brightness value.
//
//
//   Values:
//    [0-255]
//
//   Key:
//    128 - Standard Brightness
//    <128 - Decreased Brightness
//    >128 - Increased Brightness
//
//   Default Value: 128
// ------------------------------------------------------------------------------ //
#ifdef BRIGHTNESS_STRENGTH_ID
uint8_t brightness_strength( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
#if defined( ISP_HAS_MATRIX_YUV_FSM )
    *ret_value = 0;
    if ( direction == COMMAND_SET ) {
        uint32_t brightness;

        if ( value >= 256 )
            return NOT_SUPPORTED;

        brightness = value;
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MATRIX_YUV_BRIGHTNESS_STRENGTH, &brightness, sizeof( brightness ) );
        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        uint32_t brightness = 0;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MATRIX_YUV_BRIGHTNESS_STRENGTH, NULL, 0, &brightness, sizeof( brightness ) );
        *ret_value = brightness;
        return SUCCESS;
    }

    return NOT_SUPPORTED;
#else
    return NOT_SUPPORTED;
#endif
}
#endif


// ------------------------------------------------------------------------------ //
// contrast_strength description:
//
//   Control the exact contrast value.
//
//   Values:
//    [0-255]
//
//   Key:
//    128 - Standard Contrast
//    <128 - Decreased Contrast
//    >128 - Increased Contrast
//
//   Default Value: 128
// ------------------------------------------------------------------------------ //
#ifdef CONTRAST_STRENGTH_ID
uint8_t contrast_strength( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
#if defined( ISP_HAS_MATRIX_YUV_FSM )
    *ret_value = 0;
    if ( direction == COMMAND_SET ) {
        uint32_t strength;

        if ( value >= 256 )
            return NOT_SUPPORTED;

        strength = value;
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MATRIX_YUV_CONTRAST_STRENGTH, &strength, sizeof( strength ) );
        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        uint32_t strength = 0;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MATRIX_YUV_CONTRAST_STRENGTH, NULL, 0, &strength, sizeof( strength ) );
        *ret_value = strength;
        return SUCCESS;
    }

    return NOT_SUPPORTED;
#else
    return NOT_SUPPORTED;
#endif
}
#endif


// ------------------------------------------------------------------------------ //
// saturation_strength description:
//
//   Control the exact saturation strength.
//
//   Values:
//    [0-255]
//
//   Key:
//    128 - Standard Saturation
//    <128 - Decreased Saturation
//    >128 - Increased Saturation
//
//   Default Value: 128
// ------------------------------------------------------------------------------ //

#ifdef SATURATION_STRENGTH_ID
uint8_t saturation_strength( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
#if defined( ISP_HAS_MATRIX_YUV_FSM )
    uint32_t saturation_strength = 0;
    *ret_value = 0;
    if ( direction == COMMAND_SET ) {
        saturation_strength = value;
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MATRIX_YUV_SATURATION_STRENGTH, &saturation_strength, sizeof( saturation_strength ) );
        return SUCCESS;
    } else if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MATRIX_YUV_SATURATION_STRENGTH, NULL, 0, &saturation_strength, sizeof( saturation_strength ) );
        *ret_value = saturation_strength;
        return SUCCESS;
    }

    return NOT_SUPPORTED;
#else
    return NOT_SUPPORTED;
#endif
}
#endif
// ------------------------------------------------------------------------------ //
// hue theta description:
// Control the hue
//
// Values:[0-360]
//
//Default Value: 0
// ------------------------------------------------------------------------------ //
#ifdef HUE_THETA_ID
uint8_t hue_theta( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
#if defined( ISP_HAS_MATRIX_YUV_FSM )
    uint16_t hue_theta = (uint16_t)value;
    *ret_value = 0;
    if ( direction == COMMAND_SET ) {
        if ( value > 360 )
            return NOT_SUPPORTED;
        else
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MATRIX_YUV_HUE_THETA, &hue_theta, sizeof( hue_theta ) );

        return SUCCESS;
    } else if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MATRIX_YUV_HUE_THETA, NULL, 0, &hue_theta, sizeof( hue_theta ) );
        *ret_value = hue_theta;
        return SUCCESS;
    }

    return NOT_SUPPORTED;
#else
    return NOT_SUPPORTED;
#endif
}
#endif

// ------------------------------------------------------------------------------ //
// orientation_hflip description:
//
//     Horizontally flip the output image.
//
//     Values:
//       {ENABLE}
//       {DISABLE}
//
//     Default Value: {DISABLE}
// ------------------------------------------------------------------------------ //
#ifdef ORIENTATION_HFLIP_ID
uint8_t orientation_hflip( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    switch ( direction ) {
    case COMMAND_SET:
        switch ( value ) {
        case ENABLE:
            instance->p_ctx->hflip = 1;
            acamera_isp_top_bypass_mirror_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 0 );          // enable mirror
            acamera_isp_top_rggb_start_post_mirror_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 1 ); // color after mirror is: Gr-R-B-Gb
            break;
        case DISABLE:
            acamera_isp_top_bypass_mirror_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 1 );          // bypass mirror
            acamera_isp_top_rggb_start_post_mirror_write( ACAMERA_MGR2CTX_PTR( instance )->settings.isp_base, 0 ); // color after mirror is: R-Gr-Gb-B
            instance->p_ctx->hflip = 0;
            break;
        default:
            *ret_value = ERR_BAD_ARGUMENT;
            return FAIL;
        }

// reload some tables according with flip
#if FW_DO_INITIALIZATION && ISP_HAS_COLOR_MATRIX_FSM
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_SHADING_MESH_RELOAD, NULL, 0 );
#endif

#ifdef AF_ROI_ID
        {
            uint32_t roi;
            af_roi( instance, 0, COMMAND_GET, &roi );
            af_roi( instance, roi, COMMAND_SET, &roi );
        }
#endif
#ifdef AE_ROI_ID
        {
            uint32_t roi;
            ae_roi( instance, 0, COMMAND_GET, &roi );
            ae_roi( instance, roi, COMMAND_SET, &roi );
        }
#endif
        return SUCCESS;
    case COMMAND_GET:
        *ret_value = instance->p_ctx->hflip ? ENABLE : DISABLE;
        return SUCCESS;
    default:
        return NOT_IMPLEMENTED;
    }
}
#endif

// ------------------------------------------------------------------------------ //
// orientation_vflip description:
//
//     Vertically flip the output image.
//
//     Values:
//       {ENABLE}
//       {DISABLE}
//
//       Default Value: {DISABLE}
// ------------------------------------------------------------------------------ //
#ifdef ORIENTATION_VFLIP_ID
uint8_t orientation_vflip( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
#if defined( ISP_HAS_DMA_WRITER_FSM )
    switch ( direction ) {
    case COMMAND_SET: {
        uint32_t vflip = 0;
        switch ( value ) {
        case ENABLE:
            vflip = 1;
            break;
        case DISABLE:
            vflip = 0;
            break;
        default:
            *ret_value = ERR_BAD_ARGUMENT;
            return FAIL;
        }

        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_DMA_VFLIP, &vflip, sizeof( vflip ) );

#ifdef AF_ROI_ID
        {
            uint32_t roi;
            af_roi( instance, 0, COMMAND_GET, &roi );
            af_roi( instance, roi, COMMAND_SET, &roi );
        }
#endif
#ifdef AE_ROI_ID
        {
            uint32_t roi;
            ae_roi( instance, 0, COMMAND_GET, &roi );
            ae_roi( instance, roi, COMMAND_SET, &roi );
        }
#endif
        return SUCCESS;
    }

    case COMMAND_GET: {
        uint32_t vflip = 0;
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_DMA_VFLIP, NULL, 0, &vflip, sizeof( vflip ) );

        // PROBABLY WILL BE DIFFERENT FOR DIFFERENT ISP
        *ret_value = vflip ? ENABLE : DISABLE;

        return SUCCESS;
    }

    default:
        return NOT_IMPLEMENTED;
    }
#else
    return NOT_SUPPORTED;
#endif
}
#endif


// ------------------------------------------------------------------------------ //
// color_mode description:
//
//     Select the color mode of the ISP.
//
//     Values:
//       {NORMAL}
//       {BLACK_AND_WHITE}
//       {NEGATIVE}
//       {SEPIA}
//       {VIVID}
//
//     Default Value: {NORMAL}
// ------------------------------------------------------------------------------ //
#ifdef COLOR_MODE_ID
uint8_t color_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    if ( direction == COMMAND_SET ) {

        if ( VIVID == value ) {
            acamera_command( TSCENE_MODES, SATURATION_STRENGTH_ID, 154, COMMAND_SET, ret_value );
        } else {
            acamera_command( TSCENE_MODES, SATURATION_STRENGTH_ID, 128, COMMAND_SET, ret_value );
        }

        switch ( value ) {
        case VIVID:
        case NORMAL:
        case BLACK_AND_WHITE:
        case NEGATIVE:
        case SEPIA: {
            uint32_t color_mode = (uint16_t)value;
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MATRIX_YUV_COLOR_MODE, &color_mode, sizeof( color_mode ) );
        } break;
        default:
            return NOT_SUPPORTED;
            break;
        }
        return SUCCESS;

    } else if ( direction == COMMAND_GET ) {
        uint32_t color_mode = 0;

        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MATRIX_YUV_COLOR_MODE, NULL, 0, &color_mode, sizeof( color_mode ) );

        if ( color_mode == 0 ) //at init
            *ret_value = NORMAL;
        else
            *ret_value = color_mode;

        return SUCCESS;
    }
    return NOT_SUPPORTED;
}
#endif

//---------------------------------------------------------------------------------------


#ifdef STATUS_INFO_AWB_MIX_LIGHT_CONTRAST
uint8_t status_info_awb_mix_light_contrast( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    switch ( direction ) {
    case COMMAND_SET:
        return NOT_PERMITTED;
    case COMMAND_GET: {
        status_info_param_t *p_status_info = (status_info_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_STATUS_INFO );
        *ret_value = p_status_info->awb_info.mix_light_contrast;
        return SUCCESS;
    }
    default:
        return NOT_IMPLEMENTED;
    }
}
#endif

#ifdef STATUS_INFO_AF_LENS_POS
uint8_t status_info_af_lens_pos( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    switch ( direction ) {
    case COMMAND_SET:
        return NOT_PERMITTED;
    case COMMAND_GET: {
        status_info_param_t *p_status_info = (status_info_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_STATUS_INFO );
        *ret_value = p_status_info->af_info.lens_pos;
        return SUCCESS;
    }
    default:
        return NOT_IMPLEMENTED;
    }
}
#endif

#ifdef STATUS_INFO_AF_FOCUS_VALUE
uint8_t status_info_af_focus_value( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;

    switch ( direction ) {
    case COMMAND_SET:
        return NOT_PERMITTED;
    case COMMAND_GET: {
        status_info_param_t *p_status_info = (status_info_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_STATUS_INFO );
        *ret_value = p_status_info->af_info.focus_value;
        return SUCCESS;
    }
    default:
        return NOT_IMPLEMENTED;
    }
}
#endif

#ifdef STATUS_INFO_EXPOSURE_LOG2_ID
uint8_t status_info_exposure_log2( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    switch ( direction ) {
    case COMMAND_SET:
        return NOT_PERMITTED;
    case COMMAND_GET: {
        status_info_param_t *p_status_info = (status_info_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_STATUS_INFO );
        *ret_value = p_status_info->sys_info.exposure_log2;
        return SUCCESS;
    }
    default:
        return NOT_IMPLEMENTED;
    }
}
#endif

#ifdef STATUS_INFO_GAIN_ONES_ID
uint8_t status_info_gain_ones( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    switch ( direction ) {
    case COMMAND_SET:
        return NOT_PERMITTED;
    case COMMAND_GET: {
        status_info_param_t *p_status_info = (status_info_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_STATUS_INFO );
        *ret_value = acamera_math_exp2( p_status_info->sys_info.total_gain_log2, LOG2_GAIN_SHIFT, 0 );

        return SUCCESS;
    }
    default:
        return NOT_IMPLEMENTED;
    }
}
#endif

#ifdef STATUS_INFO_GAIN_LOG2_ID
uint8_t status_info_gain_log2( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    switch ( direction ) {
    case COMMAND_SET:
        return NOT_PERMITTED;
    case COMMAND_GET: {
        status_info_param_t *p_status_info = (status_info_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_STATUS_INFO );
        *ret_value = ( p_status_info->sys_info.total_gain_log2 + ( ( 1 << ( LOG2_GAIN_SHIFT - 1 ) ) - 1 ) ) >> LOG2_GAIN_SHIFT;
        return SUCCESS;
    }
    default:
        return NOT_IMPLEMENTED;
    }
}
#endif


//---------------------------------------------------------------------------------------
static uint8_t acamera_calibration_update( acamera_fsm_mgr_t *instance, uint32_t id, uint8_t direction, void *data, uint32_t data_size, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    *ret_value = 0;
    if ( id < CALIBRATION_TOTAL_SIZE && data != NULL ) {
        // we require preceise size of source and destanation
        if ( data_size == _GET_SIZE( ACAMERA_MGR2CTX_PTR( instance ), id ) ) {
            // assume command_set by default
            uint8_t *src = (uint8_t *)data;
            uint8_t *dst = (uint8_t *)_GET_LUT_PTR( ACAMERA_MGR2CTX_PTR( instance ), id );
            if ( direction == COMMAND_GET ) {
                // change src and dst
                src = (uint8_t *)_GET_LUT_PTR( ACAMERA_MGR2CTX_PTR( instance ), id );
                dst = (uint8_t *)data;
            }
            // copy buffers from src to dst as char array
            uint32_t idx = 0;
            for ( idx = 0; idx < data_size; idx++ ) {
                dst[idx] = src[idx];
            }
            result = SUCCESS;
            *ret_value = 0;
        } else {
            result = FAIL;
            *ret_value = ERR_WRONG_SIZE;
        }
    } else {
        result = FAIL;
        *ret_value = ERR_BAD_ARGUMENT;
    }
    return result;
}

#ifdef BUFFER_DATA_TYPE

static uint32_t get_calibration_description( acamera_fsm_mgr_t *instance, uint32_t id )
{
    uint32_t result = 0;
    if ( id < CALIBRATION_TOTAL_SIZE ) {
        // support only 1, 2 and 4 bytes format now.
        // it's enough 2 bits only for this data
        uint32_t width = _GET_WIDTH( ACAMERA_MGR2CTX_PTR( instance ), id ) - 1;
        uint32_t rows = _GET_ROWS( ACAMERA_MGR2CTX_PTR( instance ), id );
        uint32_t cols = _GET_COLS( ACAMERA_MGR2CTX_PTR( instance ), id );
        // the description format is 2 bits for width
        // 15 bits for rows number and 15 bits for cols number
        result = ( width << 30 | ( rows << 15 ) | cols );
    } else {
        LOG( LOG_ERR, "Table pointer has invalid index %d. Maximum possible value is %d", (int)id, CALIBRATION_TOTAL_SIZE );
    }
    return result;
}


uint8_t buffer_data_type( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    *ret_value = 0;
    switch ( direction ) {
    case COMMAND_GET:
        *ret_value = get_calibration_description( instance, value );
        return SUCCESS;
    default:
        return NOT_IMPLEMENTED;
    }
}
#endif


uint8_t acamera_api_calibration( uint8_t type, uint8_t id, uint8_t direction, void *data, uint32_t data_size, uint32_t *ret_value )
{

#ifdef BUFFER_DATA_TYPE
    uint8_t result = SUCCESS;
    acamera_fsm_mgr_t *instance = &( ( (acamera_context_t *)acamera_get_api_ctx_ptr() )->fsm_mgr );
    *ret_value = 0;

#ifdef CALIBRATION_SENSOR_REGISTER_LUT
    uint16_t *src;
    uint32_t rows;
    uint32_t i;
    fsm_param_reg_cfg_t reg_cfg;
#endif

    if ( !instance || ( direction != COMMAND_SET && direction != COMMAND_GET ) ) {
        *ret_value = ERR_BAD_ARGUMENT;
        return FAIL;
    }

#if FW_HAS_CONTROL_CHANNEL
    ctrl_channel_handle_api_calibration( type, id, direction, data, data_size );
#endif

    if ( id < CALIBRATION_TOTAL_SIZE ) {

        int16_t internal_lut_idx = id;

        if ( internal_lut_idx != -1 ) {
            // update an internal look-up table
            result = acamera_calibration_update( instance, internal_lut_idx, direction, data, data_size, ret_value );

            if ( direction == COMMAND_SET ) {
                // do some initialization after
                // a lut was updated
                switch ( internal_lut_idx ) {
#ifdef CALIBRATION_SENSOR_REGISTER_LUT
                case CALIBRATION_SENSOR_REGISTER_LUT:
                    src = (uint16_t *)_GET_LUT_PTR( ACAMERA_MGR2CTX_PTR( instance ), internal_lut_idx );
                    rows = _GET_ROWS( ACAMERA_MGR2CTX_PTR( instance ), internal_lut_idx );
                    for ( i = 0; i < rows; i++ ) {
                        reg_cfg.reg_addr = *src;
                        reg_cfg.reg_value = *( src + 1 );
                        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_SENSOR_REG, &reg_cfg, sizeof( reg_cfg ) );
                        src += 2;
                    }
                    break;
#endif
                case CALIBRATION_SHARPEN_FR:
#ifdef CALIBRATION_SHARPEN_DS1
                case CALIBRATION_SHARPEN_DS1:
#endif
#ifdef CALIBRATION_SHARPEN_DS2
                case CALIBRATION_SHARPEN_DS2:
#endif
                case CALIBRATION_SHARP_ALT_D:
                case CALIBRATION_SHARP_ALT_UD:
#if defined( ISP_HAS_SHARPENING_FSM )
// sharpening_initialize is an empty function right now, so we don't need to
// add function here for new FW.

#endif
                    break;
                case CALIBRATION_MT_ABSOLUTE_LS_A_CCM:
                case CALIBRATION_MT_ABSOLUTE_LS_D40_CCM:
                case CALIBRATION_MT_ABSOLUTE_LS_D50_CCM:
#if ISP_HAS_COLOR_MATRIX_FSM
                    acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_CCM_CHANGE, NULL, 0 );
#endif
                    break;
                case CALIBRATION_GAMMA:
                case CALIBRATION_DEMOSAIC:
#ifdef CALIBRATION_GAMMA_BE_0
                case CALIBRATION_GAMMA_BE_0:
#endif
#ifdef CALIBRATION_GAMMA_BE_1
                case CALIBRATION_GAMMA_BE_1:
#endif
                case CALIBRATION_NOISE_PROFILE:
#ifdef CALIBRATION_GAMMA_FE_0
                case CALIBRATION_GAMMA_FE_0:
#endif
#ifdef CALIBRATION_GAMMA_FE_1
                case CALIBRATION_GAMMA_FE_1:
#endif
                case CALIBRATION_SHADING_LS_A_R:
                case CALIBRATION_SHADING_LS_A_G:
                case CALIBRATION_SHADING_LS_A_B:
                case CALIBRATION_SHADING_LS_TL84_R:
                case CALIBRATION_SHADING_LS_TL84_G:
                case CALIBRATION_SHADING_LS_TL84_B:
                case CALIBRATION_SHADING_LS_D65_R:
                case CALIBRATION_SHADING_LS_D65_G:
                case CALIBRATION_SHADING_LS_D65_B:
#if ISP_HAS_GENERAL_FSM
                    acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_RELOAD_CALIBRATION, NULL, 0 );
#endif
                    break;
                case CALIBRATION_IRIDIX_ASYMMETRY:
#if defined( ISP_HAS_IRIDIX_FSM ) || defined( ISP_HAS_IRIDIX_HIST_FSM ) || defined( ISP_HAS_IRIDIX_MANUAL_FSM )
                    acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_IRIDIX_INIT, NULL, 0 );
#endif
                    break;
#ifdef CALIBRATION_DEFECT_PIXELS
                case CALIBRATION_DEFECT_PIXELS: {
                    int32_t res = defect_pixel_table_write( data, _GET_LEN( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_DEFECT_PIXELS ) );
                    if ( res == 0 )
                        result = SUCCESS;
                    else
                        result = FAIL;
                } break;
#endif
                default:
                    // do not do anything for this lut
                    result = SUCCESS;
                }
            }
        } else {
            LOG( LOG_ERR, "Trying to get an access with an invalid LUT index %d", internal_lut_idx );
            result = FAIL;
        }
    } else {
        LOG( LOG_ERR, "Trying to get an access with an invalid LUT index %d", id );
        result = FAIL;
    }

    return result;
#else
    return FAIL;
#endif
}


#ifdef CALIBRATION_UPDATE
extern int32_t acamera_update_calibration_set( acamera_context_ptr_t p_ctx );
uint8_t calibration_update( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    //set for each context
    acamera_context_ptr_t p_ctx = (acamera_context_t *)acamera_get_api_ctx_ptr();
    *ret_value = 0;
    if ( direction == COMMAND_GET ) {
        *ret_value = DONE;
        return SUCCESS;
    } else if ( direction == COMMAND_SET ) {
        if ( value == UPDATE ) {
            uint32_t result = acamera_update_calibration_set( p_ctx );
            if ( result != 0 ) {
                *ret_value = FAIL;
            }
        }
        return SUCCESS;
    } else {
        return NOT_SUPPORTED;
    }
    return NOT_SUPPORTED;
}
#endif


//type: dma pipe fs or ds1; id:context but reserved for now; direction MASK reserved for now
uint8_t acamera_api_dma_buffer( uint8_t type, void *data, uint32_t data_size, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;

//disable dma

//write frames
#if defined( ISP_HAS_DMA_WRITER_FSM )
    acamera_fsm_mgr_t *instance = &( ( (acamera_context_t *)acamera_get_api_ctx_ptr() )->fsm_mgr );
    fsm_param_dma_pipe_setting_t pipe_update;

    pipe_update.pipe_id = type;
    pipe_update.buf_array = (tframe_t *)data;
    pipe_update.buf_len = data_size;
    pipe_update.callback = NULL;
    acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_DMA_PIPE_SETTING, &pipe_update, sizeof( pipe_update ) );
#else
    result = NOT_SUPPORTED;
#endif

    result = NOT_SUPPORTED;

    return result;
}

uint8_t acamera_api_set_fps(uint8_t type, uint32_t c_fps, uint32_t t_fps)
{
    acamera_fsm_mgr_t *instance = NULL;
    fsm_param_path_fps_t pipe_fps;

    pipe_fps.pipe_id = type;
    pipe_fps.c_fps = c_fps;
    pipe_fps.t_fps = t_fps;

    instance = &(((acamera_context_t *)acamera_get_api_ctx_ptr() )->fsm_mgr);

    acamera_fsm_mgr_set_param(instance, FSM_PARAM_SET_PATH_FPS, &pipe_fps, sizeof(pipe_fps));

    return SUCCESS;
}

void acamera_api_dma_buff_queue_reset(uint8_t type)
{
    uint8_t d_type = 0xff;

    acamera_fsm_mgr_t *instance = &( ( (acamera_context_t *)acamera_get_api_ctx_ptr() )->fsm_mgr );

    d_type = type;

    acamera_fsm_mgr_set_param(instance, FSM_PARAM_SET_DMA_QUEUE_RESET, &d_type, sizeof(d_type));
}

void acamera_api_dma_buff_get_next(uint8_t type)
{
    uint8_t d_type = 0xff;

    acamera_fsm_mgr_t *instance = &( ( (acamera_context_t *)acamera_get_api_ctx_ptr() )->fsm_mgr );

    d_type = type;

    acamera_fsm_mgr_set_param(instance, FSM_PARAM_SET_DMA_PULL_BUFFER, &d_type, sizeof(d_type));
}

#ifdef MON_ERROR_CALIBRATION_LUT_NULL
uint8_t monitor_error_calibration_lut_null( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t err_type = MON_TYPE_ERR_CALIBRATION_LUT_NULL;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_ERROR, &err_type, sizeof( err_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        if ( value != 0 ) {
            result = NOT_SUPPORTED;
        } else {
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MON_RESET_ERROR, &err_type, sizeof( err_type ) );
            result = SUCCESS;
        }
    }

    return result;
}
#endif

#ifdef MON_ERROR_CMOS_FS_DELAY
uint8_t monitor_error_cmos_frame_start_delay( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t err_type = MON_TYPE_ERR_CMOS_FS_DELAY;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_ERROR, &err_type, sizeof( err_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        if ( value != 0 ) {
            result = NOT_SUPPORTED;
        } else {
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MON_RESET_ERROR, &err_type, sizeof( err_type ) );
            result = SUCCESS;
        }
    }

    return result;
}
#endif

#ifdef MON_ERROR_CMOS_UPDATE_NOT_IN_VB
uint8_t monitor_error_cmos_update_not_in_vb( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t err_type = MON_TYPE_ERR_CMOS_UPDATE_NOT_IN_VB;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_ERROR, &err_type, sizeof( err_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        if ( value != 0 ) {
            result = NOT_SUPPORTED;
        } else {
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MON_RESET_ERROR, &err_type, sizeof( err_type ) );
            result = SUCCESS;
        }
    }

    return result;
}
#endif

#ifdef MON_ERROR_CMOS_UPDATE_DGAIN_WRONG_TIMING
uint8_t monitor_error_cmos_update_dgain_wrong_timing( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t err_type = MON_TYPE_ERR_CMOS_UPDATE_DGAIN_WRONG_TIMING;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_ERROR, &err_type, sizeof( err_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        if ( value != 0 ) {
            result = NOT_SUPPORTED;
        } else {
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MON_RESET_ERROR, &err_type, sizeof( err_type ) );
            result = SUCCESS;
        }
    }

    return result;
}
#endif

#ifdef MON_ERROR_IRIDIX_UPDATE_NOT_IN_VB
uint8_t monitor_error_iridix_update_not_in_vb( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t err_type = MON_TYPE_ERR_IRIDIX_UPDATE_NOT_IN_VB;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_ERROR, &err_type, sizeof( err_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        if ( value != 0 ) {
            result = NOT_SUPPORTED;
        } else {
            acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MON_RESET_ERROR, &err_type, sizeof( err_type ) );
            result = SUCCESS;
        }
    }

    return result;
}
#endif

#ifdef MON_STATUS_AE_RESET
uint8_t monitor_status_ae_reset( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_STATUS_RESET;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AE, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        fsm_param_mon_status_head_t mon_status;
        mon_status.status_type = MON_TYPE_STATUS_ALG_STATUS_RESET;
        mon_status.status_param = value;
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MON_STATUS_AE, &mon_status, sizeof( mon_status ) );
        result = SUCCESS;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AE_FPT_MIN
uint8_t monitor_status_ae_fpt_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AE, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AE_FPT_CUR
uint8_t monitor_status_ae_fpt_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AE, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AE_FPT_MAX
uint8_t monitor_status_ae_fpt_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AE, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AE_DELAY_STATS2EV_MIN
uint8_t monitor_status_ae_delay_stats2ev_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AE, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AE_DELAY_STATS2EV_CUR
uint8_t monitor_status_ae_delay_stats2ev_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AE, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AE_DELAY_STATS2EV_MAX
uint8_t monitor_status_ae_delay_stats2ev_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AE, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AE_DELAY_EV2APPLY_MIN
uint8_t monitor_status_ae_delay_ev2apply_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AE, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AE_DELAY_EV2APPLY_CUR
uint8_t monitor_status_ae_delay_ev2apply_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AE, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AE_DELAY_EV2APPLY_MAX
uint8_t monitor_status_ae_delay_ev2apply_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AE, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AWB_RESET
uint8_t monitor_status_awb_reset( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_STATUS_RESET;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AWB, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        fsm_param_mon_status_head_t mon_status;
        mon_status.status_type = MON_TYPE_STATUS_ALG_STATUS_RESET;
        mon_status.status_param = value;
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MON_STATUS_AWB, &mon_status, sizeof( mon_status ) );
        result = SUCCESS;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AWB_FPT_MIN
uint8_t monitor_status_awb_fpt_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AWB, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AWB_FPT_CUR
uint8_t monitor_status_awb_fpt_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AWB, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AWB_FPT_MAX
uint8_t monitor_status_awb_fpt_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AWB, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AWB_DELAY_STATS2GAIN_MIN
uint8_t monitor_status_awb_delay_stats2gain_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AWB, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AWB_DELAY_STATS2GAIN_CUR
uint8_t monitor_status_awb_delay_stats2gain_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AWB, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AWB_DELAY_STATS2GAIN_MAX
uint8_t monitor_status_awb_delay_stats2gain_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AWB, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AWB_DELAY_GAIN2APPLY_MIN
uint8_t monitor_status_awb_delay_gain2apply_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AWB, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AWB_DELAY_GAIN2APPLY_CUR
uint8_t monitor_status_awb_delay_gain2apply_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AWB, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_AWB_DELAY_GAIN2APPLY_MAX
uint8_t monitor_status_awb_delay_gain2apply_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_AWB, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_GAMMA_RESET
uint8_t monitor_status_gamma_reset( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_STATUS_RESET;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_GAMMA, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        fsm_param_mon_status_head_t mon_status;
        mon_status.status_type = MON_TYPE_STATUS_ALG_STATUS_RESET;
        mon_status.status_param = value;
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MON_STATUS_GAMMA, &mon_status, sizeof( mon_status ) );
        result = SUCCESS;
    }

    return result;
}
#endif

#ifdef MON_STATUS_GAMMA_FPT_MIN
uint8_t monitor_status_gamma_fpt_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_GAMMA, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_GAMMA_FPT_CUR
uint8_t monitor_status_gamma_fpt_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_GAMMA, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_GAMMA_FPT_MAX
uint8_t monitor_status_gamma_fpt_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_GAMMA, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif


#ifdef MON_STATUS_GAMMA_DELAY_STATS2LUT_MIN
uint8_t monitor_status_gamma_delay_stats2lut_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_GAMMA, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_GAMMA_DELAY_STATS2LUT_CUR
uint8_t monitor_status_gamma_delay_stats2lut_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_GAMMA, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_GAMMA_DELAY_STATS2LUT_MAX
uint8_t monitor_status_gamma_delay_stats2lut_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_GAMMA, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_GAMMA_DELAY_LUT2APPLY_MIN
uint8_t monitor_status_gamma_delay_lut2apply_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_GAMMA, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_GAMMA_DELAY_LUT2APPLY_CUR
uint8_t monitor_status_gamma_delay_lut2apply_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_GAMMA, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_GAMMA_DELAY_LUT2APPLY_MAX
uint8_t monitor_status_gamma_delay_lut2apply_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_GAMMA, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_IRIDIX_RESET
uint8_t monitor_status_iridix_reset( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_STATUS_RESET;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_IRIDIX, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        fsm_param_mon_status_head_t mon_status;
        mon_status.status_type = MON_TYPE_STATUS_ALG_STATUS_RESET;
        mon_status.status_param = value;
        acamera_fsm_mgr_set_param( instance, FSM_PARAM_SET_MON_STATUS_IRIDIX, &mon_status, sizeof( mon_status ) );
        result = SUCCESS;
    }

    return result;
}
#endif

#ifdef MON_STATUS_IRIDIX_FPT_MIN
uint8_t monitor_status_iridix_fpt_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_IRIDIX, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_IRIDIX_FPT_CUR
uint8_t monitor_status_iridix_fpt_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_IRIDIX, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_IRIDIX_FPT_MAX
uint8_t monitor_status_iridix_fpt_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_FPT_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_IRIDIX, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_IRIDIX_DELAY_STATS2TARGET_MIN
uint8_t monitor_status_iridix_delay_stats2target_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_IRIDIX, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_IRIDIX_DELAY_STATS2TARGET_CUR
uint8_t monitor_status_iridix_delay_stats2target_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_IRIDIX, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_IRIDIX_DELAY_STATS2TARGET_MAX
uint8_t monitor_status_iridix_delay_stats2target_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_IRIDIX, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_IRIDIX_DELAY_TARGET2APPLY_MIN
uint8_t monitor_status_iridix_delay_target2apply_min( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MIN;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_IRIDIX, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_IRIDIX_DELAY_TARGET2APPLY_CUR
uint8_t monitor_status_iridix_delay_target2apply_cur( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_CUR;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_IRIDIX, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif

#ifdef MON_STATUS_IRIDIX_DELAY_TARGET2APPLY_MAX
uint8_t monitor_status_iridix_delay_target2apply_max( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value )
{
    uint8_t result = SUCCESS;
    uint32_t mon_status_type = MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MAX;
    *ret_value = 0;

    if ( direction == COMMAND_GET ) {
        acamera_fsm_mgr_get_param( instance, FSM_PARAM_GET_MON_STATUS_IRIDIX, &mon_status_type, sizeof( mon_status_type ), ret_value, sizeof( uint32_t ) );
        result = SUCCESS;
    } else {
        result = NOT_SUPPORTED;
    }

    return result;
}
#endif
