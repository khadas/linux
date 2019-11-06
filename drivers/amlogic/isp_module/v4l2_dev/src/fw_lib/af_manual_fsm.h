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

#if !defined( __AF_FSM_H__ )
#define __AF_FSM_H__



typedef struct _AF_fsm_t AF_fsm_t;
typedef struct _AF_fsm_t *AF_fsm_ptr_t;
typedef const struct _AF_fsm_t *AF_fsm_const_ptr_t;

#define ISP_HAS_AF_FSM 1
#include "acamera_isp_config.h"
#include "acamera_lens_api.h"
#include "acamera_isp_core_nomem_settings.h"

#define ISP_HAS_AF_MANUAL_FSM 1
#define AF_ZONES_COUNT_MAX ( ISP_METERING_ZONES_MAX_V * ISP_METERING_ZONES_MAX_H )


#define AF_SPOT_IGNORE_NUM 1
#define AF_CALIBRATION_BOUNDARIES 1


typedef struct _af_lms_param_t {
    uint32_t pos_min_down;
    uint32_t pos_min;
    uint32_t pos_min_up;
    uint32_t pos_inf_down;
    uint32_t pos_inf;
    uint32_t pos_inf_up;
    uint32_t pos_macro_down;
    uint32_t pos_macro;
    uint32_t pos_macro_up;
    uint32_t pos_max_down;
    uint32_t pos_max;
    uint32_t pos_max_up;
    uint32_t fast_search_positions;
    uint32_t skip_frames_init;
    uint32_t skip_frames_move;
    uint32_t dynamic_range_th;
    uint32_t spot_tolerance;
    uint32_t exit_th;
    uint32_t caf_trigger_th;
    uint32_t caf_stable_th;
    uint32_t print_debug;
} af_lms_param_t;


void AF_init( AF_fsm_ptr_t p_fsm );
void AF_deinit( AF_fsm_ptr_t p_fsm );
void AF_request_irq( AF_fsm_ptr_t p_fsm );
void af_notify_status( AF_fsm_ptr_t p_fsm );

struct _AF_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    uint32_t zone_raw_statistic[AF_ZONES_COUNT_MAX][2];
    uint8_t zone_weight[AF_ZONES_COUNT_MAX];
    uint64_t zone_process_statistic[AF_ZONES_COUNT_MAX];
    uint32_t zone_process_reliablility[AF_ZONES_COUNT_MAX];
    uint32_t pos_min;
    uint32_t pos_inf;
    uint32_t pos_macro;
    uint32_t pos_max;
    uint32_t def_pos_min;
    uint32_t def_pos_inf;
    uint32_t def_pos_macro;
    uint32_t def_pos_max;
    uint32_t def_pos_min_up;
    uint32_t def_pos_inf_up;
    uint32_t def_pos_macro_up;
    uint32_t def_pos_max_up;
    uint32_t def_pos_min_down;
    uint32_t def_pos_inf_down;
    uint32_t def_pos_macro_down;
    uint32_t def_pos_max_down;
    uint8_t skip_frame;

    uint8_t frame_num;
    uint32_t mode;
    uint32_t pos_manual;
    uint32_t new_pos;
    uint32_t roi;
    int32_t lens_driver_ok;
    uint32_t roi_api;
    uint32_t frame_number_from_start;
    uint32_t last_position;
    uint32_t last_pos_done;
    int32_t new_last_sharp;
    int32_t last_sharp_done;
    void *lens_ctx;
    lens_control_t lens_ctrl;
    uint8_t frame_skip_cnt;
    uint8_t frame_skip_start;
};


void AF_fsm_clear( AF_fsm_ptr_t p_fsm );

void AF_fsm_init( void *fsm, fsm_init_param_t *init_param );
int AF_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int AF_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t AF_fsm_process_event( AF_fsm_ptr_t p_fsm, event_id_t event_id );

void AF_fsm_process_interrupt( AF_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void AF_request_interrupt( AF_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __AF_FSM_H__ */
