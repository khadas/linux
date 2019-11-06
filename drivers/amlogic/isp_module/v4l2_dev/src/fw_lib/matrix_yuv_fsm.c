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
#include "matrix_yuv_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_MATRIX_YUV
#endif

void matrix_yuv_fsm_clear( matrix_yuv_fsm_t *p_fsm )
{
    p_fsm->hue_theta = 0;
    p_fsm->saturation_strength = 128;
    p_fsm->contrast_strength = 128;
    p_fsm->brightness_strength = 128;
    p_fsm->fr_pipe_output_format = FW_FR_OUTPUT_FORMAT_PIPE;
#if ISP_HAS_DS1
    p_fsm->ds1_pipe_output_format = FW_DS1_OUTPUT_FORMAT_PIPE;
#endif
}

void matrix_yuv_request_interrupt( matrix_yuv_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void matrix_yuv_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    matrix_yuv_fsm_t *p_fsm = (matrix_yuv_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    matrix_yuv_fsm_clear( p_fsm );

    matrix_yuv_initialize( p_fsm );
    matrix_yuv_update( p_fsm );
    matrix_yuv_request_interrupt( p_fsm, ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_END ) );
}


int matrix_yuv_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    matrix_yuv_fsm_t *p_fsm = (matrix_yuv_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_MATRIX_YUV_FR_OUT_FMT:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->fr_pipe_output_format = *(uint32_t *)input;
        matrix_yuv_update( p_fsm );
        break;

#if ISP_HAS_DS1
    case FSM_PARAM_SET_MATRIX_YUV_DS1_OUT_FMT:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->ds1_pipe_output_format = *(uint32_t *)input;
        matrix_yuv_update( p_fsm );
        break;
#endif

    case FSM_PARAM_SET_MATRIX_YUV_SATURATION_STRENGTH:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->saturation_strength = *(uint32_t *)input;
        matrix_yuv_update( p_fsm );

        break;

    case FSM_PARAM_SET_MATRIX_YUV_HUE_THETA:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->hue_theta = *(uint32_t *)input;
        matrix_yuv_update( p_fsm );
        break;

    case FSM_PARAM_SET_MATRIX_YUV_BRIGHTNESS_STRENGTH:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->brightness_strength = *(uint32_t *)input;
        matrix_yuv_update( p_fsm );
        break;

    case FSM_PARAM_SET_MATRIX_YUV_CONTRAST_STRENGTH:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->contrast_strength = *(uint32_t *)input;
        matrix_yuv_update( p_fsm );
        break;

    case FSM_PARAM_SET_MATRIX_YUV_COLOR_MODE:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->color_mode = *(uint32_t *)input;
        matrix_yuv_update( p_fsm );
        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}


int matrix_yuv_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    matrix_yuv_fsm_t *p_fsm = (matrix_yuv_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_MATRIX_YUV_FR_OUT_FMT:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->fr_pipe_output_format;
        break;

#if ISP_HAS_DS1
    case FSM_PARAM_GET_MATRIX_YUV_DS1_OUT_FMT:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->ds1_pipe_output_format;
        break;
#endif

    case FSM_PARAM_GET_MATRIX_YUV_SATURATION_STRENGTH:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->saturation_strength;
        break;

    case FSM_PARAM_GET_MATRIX_YUV_HUE_THETA:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->hue_theta;
        break;

    case FSM_PARAM_GET_MATRIX_YUV_BRIGHTNESS_STRENGTH:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->brightness_strength;
        break;

    case FSM_PARAM_GET_MATRIX_YUV_CONTRAST_STRENGTH:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->contrast_strength;
        break;

    case FSM_PARAM_GET_MATRIX_YUV_COLOR_MODE:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->color_mode;
        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}


uint8_t matrix_yuv_fsm_process_event( matrix_yuv_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;

    case event_id_frame_end:
        // no function is called here, reason is unknown.
        b_event_processed = 1;
        break;
    case event_id_sensor_ready:
        matrix_yuv_request_interrupt( p_fsm, ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_END ) );
        b_event_processed = 1;
        break;
    }

    return b_event_processed;
}
