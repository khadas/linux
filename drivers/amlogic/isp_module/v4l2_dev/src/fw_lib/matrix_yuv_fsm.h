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

#if !defined( __MATRIX_YUV_FSM_H__ )
#define __MATRIX_YUV_FSM_H__



typedef struct _matrix_yuv_fsm_t matrix_yuv_fsm_t;
typedef struct _matrix_yuv_fsm_t *matrix_yuv_fsm_ptr_t;
typedef const struct _matrix_yuv_fsm_t *matrix_yuv_fsm_const_ptr_t;

#include "acamera_command_api.h"
#define PIPE_OUT_YUV444 1
#define PIPE_OUT_YUV422 2
#define PIPE_OUT_YUV420 3
#define PIPE_OUT_RGB 4
#ifndef AWB_LIGHT_SOURCE_D50
#define AWB_LIGHT_SOURCE_D50 0x03
#endif
void calculate_saturation_matrix( matrix_yuv_fsm_ptr_t p_fsm );
void calculate_brightness_matrix( matrix_yuv_fsm_ptr_t p_fsm );
void calculate_contrast_matrix( matrix_yuv_fsm_ptr_t p_fsm );
void matrix_multiply( int16_t *a1, int16_t *a2, int16_t *result, int dim1, int dim2, int dim3 );
uint16_t color_matrix_complement_to_direct( int16_t v );
void matrix_identity( int16_t *m, int dim );
int16_t color_matrix_direct_to_complement( uint16_t v );
void matrix_yuv_initialize( matrix_yuv_fsm_ptr_t p_fsm );
void matrix_yuv_update( matrix_yuv_fsm_ptr_t p_fsm );
void matrix_yuv_recompute( matrix_yuv_fsm_ptr_t p_fsm );
void matrix_yuv_coefft_write_to_hardware( matrix_yuv_fsm_ptr_t p_fsm );
void matrix_yuv_set_FR_mode( uint16_t v );
void matrix_yuv_set_DS1_mode( uint16_t v );

struct _matrix_yuv_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    int16_t saturation_matrix[12];
    int16_t contrast_matrix[12];
    int16_t brightness_matrix[12];
    int16_t hue_correction_matrix[12];
    int16_t color_mode_matrix[12];
    int16_t rgb2yuv_matrix[12];
    uint32_t hue_theta;
    uint32_t saturation_strength;
    uint32_t contrast_strength;
    uint32_t brightness_strength;
    uint32_t fr_pipe_output_format;
    int16_t fr_composite_yuv_matrix[12];
#if ISP_HAS_DS1
    uint32_t ds1_pipe_output_format;
    int16_t ds1_composite_yuv_matrix[12];
#endif
    uint32_t color_mode;
    uint8_t manual_saturation_enabled;
    uint8_t saturation_target;
    int16_t manual_matrix_yuv[9];
    int32_t temperature_threshold[8];
    uint16_t fr_format;
};


void matrix_yuv_fsm_clear( matrix_yuv_fsm_ptr_t p_fsm );

void matrix_yuv_fsm_init( void *fsm, fsm_init_param_t *init_param );
int matrix_yuv_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int matrix_yuv_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t matrix_yuv_fsm_process_event( matrix_yuv_fsm_ptr_t p_fsm, event_id_t event_id );

void matrix_yuv_fsm_process_interrupt( matrix_yuv_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void matrix_yuv_request_interrupt( matrix_yuv_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __MATRIX_YUV_FSM_H__ */
