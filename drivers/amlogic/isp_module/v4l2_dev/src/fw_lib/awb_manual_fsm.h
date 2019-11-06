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

#if !defined( __AWB_FSM_H__ )
#define __AWB_FSM_H__

#include "acamera_isp_core_nomem_settings.h"

typedef struct _AWB_fsm_t AWB_fsm_t;
typedef struct _AWB_fsm_t *AWB_fsm_ptr_t;
typedef const struct _AWB_fsm_t *AWB_fsm_const_ptr_t;


#define ISP_HAS_AWB_FSM 1
#define MAX_AWB_ZONES ( ISP_METERING_ZONES_MAX_V * ISP_METERING_ZONES_MAX_H )
#define AWB_LIGHT_SOURCE_UNKNOWN 0
#define AWB_LIGHT_SOURCE_A 0x01
#define AWB_LIGHT_SOURCE_D40 0x02
#define AWB_LIGHT_SOURCE_D50 0x03
#define AWB_LIGHT_SOURCE_A_TEMPERATURE 2850
#define AWB_LIGHT_SOURCE_D40_TEMPERATURE 4000
#define AWB_LIGHT_SOURCE_D50_TEMPERATURE 5000
#define AWB_DLS_LIGHT_SOURCE_A_D40_BORDER ( AWB_LIGHT_SOURCE_A_TEMPERATURE + AWB_LIGHT_SOURCE_D40_TEMPERATURE ) / 2
#define AWB_DLS_LIGHT_SOURCE_D40_D50_BORDER ( AWB_LIGHT_SOURCE_D40_TEMPERATURE + AWB_LIGHT_SOURCE_D50_TEMPERATURE ) / 2
#define AWB_DLS_SWITCH_LIGHT_SOURCE_DETECT_FRAMES_QUANTITY 15
#define AWB_DLS_SWITCH_LIGHT_SOURCE_CHANGE_FRAMES_QUANTITY 35
#define D50_DEFAULT 256

typedef struct _awb_zone_t {
    uint16_t rg;
    uint16_t bg;
    uint32_t sum;
} awb_zone_t;

void awb_init( AWB_fsm_ptr_t p_fsm );
void awb_coeffs_write( AWB_fsm_const_ptr_t p_fsm );
void awb_set_identity( AWB_fsm_ptr_t p_fsm );
void awb_read_statistics( AWB_fsm_ptr_t p_fsm );
void awb_update_ccm( AWB_fsm_ptr_t p_fsm );
void awb_normalise( AWB_fsm_ptr_t p_fsm );
void awb_roi_update( AWB_fsm_ptr_t p_fsm );
int awb_set_zone_weight(AWB_fsm_ptr_t p_fsm, void *u_wg_ptr);

#include "acamera_metering_stats_mem_config.h"


struct _AWB_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    uint16_t curr_AWB_ZONES;
    uint8_t awb_enabled;
    uint32_t sum;
    uint16_t rg_coef;
    uint16_t bg_coef;
    uint8_t p_high;
    uint8_t p_low;
    int32_t high_temp;
    int32_t low_temp;
    int32_t sfact;
    int16_t g_avgShift;
    uint32_t avg_GR;
    uint32_t avg_GB;
    uint32_t stable_avg_RG;
    uint32_t stable_avg_BG;
    uint32_t valid_threshold;
    uint8_t gPrintCnt;
    uint16_t color_wb_matrix[9];
    int32_t wb_log2[4];
    int32_t temperature_detected;
    uint8_t light_source_detected;
    uint8_t light_source_candidate;
    uint8_t detect_light_source_frames_count;
    uint32_t mode;
    int32_t max_temp;
    int32_t min_temp;
    uint16_t max_temp_rg;
    uint16_t max_temp_bg;
    uint16_t min_temp_rg;
    uint16_t min_temp_bg;
    uint32_t roi;

    int32_t awb_warming[3];
    uint32_t switch_light_source_detect_frames_quantity;
    uint32_t switch_light_source_change_frames_quantity;

    uint32_t pre_result_gain_frame_id;
    uint32_t cur_result_gain_frame_id;
};


void AWB_fsm_clear( AWB_fsm_ptr_t p_fsm );

void AWB_fsm_init( void *fsm, fsm_init_param_t *init_param );
int AWB_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int AWB_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t AWB_fsm_process_event( AWB_fsm_ptr_t p_fsm, event_id_t event_id );

void AWB_fsm_process_interrupt( AWB_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void AWB_request_interrupt( AWB_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __AWB_FSM_H__ */
