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

#if !defined( __CMOS_FSM_H__ )
#define __CMOS_FSM_H__



typedef struct _cmos_fsm_t cmos_fsm_t;
typedef struct _cmos_fsm_t *cmos_fsm_ptr_t;
typedef const struct _cmos_fsm_t *cmos_fsm_const_ptr_t;

#include "acamera_sensor_api.h"
#include "acamera_math.h"
#include "acamera_isp_config.h"
#include "system_spinlock.h"

#define API_OTAE_ITERATION_COUNT ( 15 )

typedef enum _cmos_partition_lut_index_ {
    PARTITION_LUT_BALANED_INDEX = 0,
    PARTITION_LUT_INTEGRATION_PRIORITY_INDEX = 1,
    PARTITION_LUT_INDEX_MAX,
} cmos_partition_lut_index_t;

#if FILTER_LONG_INT_TIME
typedef struct _it_long_hist_t {
    uint32_t v[FILTER_LONG_INT_TIME];
    uint64_t sum;
    uint32_t *p;
} it_long_hist_t;
#endif

#if FILTER_SHORT_INT_TIME
typedef struct _it_short_hist_t {
    uint32_t v[FILTER_SHORT_INT_TIME];
    uint64_t sum;
    uint32_t *p;
} it_short_hist_t;
#endif

typedef struct _cmos_control_param_t {
    uint32_t global_antiflicker_enable;
    uint32_t global_anti_flicker_frequency;
    uint32_t global_manual_integration_time;
    uint32_t global_manual_sensor_analog_gain;
    uint32_t global_manual_sensor_digital_gain;
    uint32_t global_manual_isp_digital_gain;
    uint32_t global_manual_max_integration_time;
    uint32_t global_max_integration_time;
    uint32_t global_max_sensor_analog_gain;
    uint32_t global_max_sensor_digital_gain;
    uint32_t global_max_isp_digital_gain;
    uint32_t global_max_exposure_ratio;
    uint32_t global_integration_time;
    uint32_t global_sensor_analog_gain;
    uint32_t global_sensor_digital_gain;
    uint32_t global_isp_digital_gain;
    uint32_t global_analog_gain_last_priority;
    uint32_t global_analog_gain_reserve;
} cmos_control_param_t;

/*
typedef struct _exposure_data_set_t
{
    uint32_t integration_time;
    int32_t isp_dgain_log2;
    uint32_t exposure_ratio;
    uint32_t actual_integration_time;
    uint32_t integration_time_long;
    uint32_t exposure_ratio_short;
    uint32_t exposure_ratio_medium;
    uint32_t integration_time_medium;
} exposure_data_set_t;
typedef struct _exposure_info_set_t
{
    int32_t exposure_log2;
    int32_t again_log2;
    int32_t dgain_log2;
    int32_t isp_dgain_log2;
} exposure_info_set_t;
typedef struct _exposure_set_t
{
    exposure_info_set_t info;
    exposure_data_set_t data;
} exposure_set_t;
*/

typedef struct _fps_counter_t {
    uint32_t last_tick;
    uint32_t avg_frame_ticks;
} fps_counter_t;

void cmos_update_exposure_partitioning_lut( cmos_fsm_ptr_t p_fsm );
void cmos_init( cmos_fsm_ptr_t p_fsm );
void cmos_deinit( cmos_fsm_ptr_t p_fsm );
void cmos_inttime_write( cmos_fsm_const_ptr_t p_fsm );
void cmos_gains_write( cmos_fsm_const_ptr_t p_fsm );
void cmos_inttime_update( cmos_fsm_ptr_t p_fsm );
void cmos_alloc_integration_time( cmos_fsm_ptr_t p_fsm, int32_t int_time );
int cmos_convert_integration_time_ms2lines( cmos_fsm_ptr_t p_fsm, int int_time_ms );
void cmos_analog_gain_update( cmos_fsm_ptr_t p_fsm );
void cmos_digital_gain_update( cmos_fsm_ptr_t p_fsm );
void cmos_antiflicker_update( cmos_fsm_ptr_t p_fsm );
void cmos_long_exposure_update( cmos_fsm_ptr_t p_fsm );
void cmos_calc_target_gain( cmos_fsm_ptr_t p_fsm );
void cmos_update_exposure( cmos_fsm_ptr_t p_fsm );
void cmos_update_exposure_history( cmos_fsm_ptr_t p_fsm );
void cmos_set_exposure_target( cmos_fsm_ptr_t p_fsm, int32_t exposure_log2, uint32_t exposure_ratio );
exposure_set_t *cmos_get_frame_exposure_set( cmos_fsm_ptr_t p_fsm, int i_frame );
uint16_t cmos_get_fps( cmos_fsm_ptr_t p_fsm );

struct _cmos_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    int16_t wb[4];
    uint8_t manual_gain_mode;
    uint32_t manual_gain;
    uint32_t exposure;
    int32_t exposure_log2;
    sys_spinlock exp_lock;
    exposure_set_t exp_next_set;
    exposure_data_set_t exp_write_set;
    int32_t max_exposure_log2;
#if FILTER_LONG_INT_TIME
    it_long_hist_t long_it_hist;
#endif
#if FILTER_SHORT_INT_TIME
    it_short_hist_t short_it_hist;
#endif
    exposure_set_t exposure_hist[8];
    int exposure_hist_pos;
    fps_counter_t fps_cnt;
    uint16_t flicker_freq;
    uint32_t lines_per_500ms;
    uint32_t exposure_ratio_in;
    uint32_t prev_integration_time_short;
    uint16_t integration_time_short;
    uint16_t integration_time_medium;
    uint16_t integration_time_medium2;
    uint16_t integration_time_long;
    uint16_t exposure_ratio;
    int32_t again_val_log2;
    int32_t dgain_val_log2;
    int32_t log2_gain_avg;
    int32_t isp_dgain_log2;
    int32_t target_gain_log2;
    int32_t target_gain_step_log2;
    uint8_t flash_state;
    uint8_t flash_skip_charge;
    uint8_t strategy;
    int32_t maximum_isp_digital_gain;

    uint32_t frame_id_tracking;
    uint32_t prev_ae_frame_id_tracking;
    uint32_t prev_awb_frame_id_tracking;

    /* previous FrameStart FrameID */
    uint32_t prev_fs_frame_id;
    uint32_t prev_dgain_frame_id;

    uint32_t exp_lut[SYSTEM_EXPOSURE_PARTITION_VALUE_COUNT];
};


void cmos_fsm_clear( cmos_fsm_ptr_t p_fsm );
void cmos_fsm_init( void *fsm, fsm_init_param_t *init_param );

int cmos_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int cmos_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );
uint8_t cmos_fsm_process_event( cmos_fsm_ptr_t p_fsm, event_id_t event_id );

void cmos_fsm_process_interrupt( cmos_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void cmos_request_interrupt( cmos_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __CMOS_FSM_H__ */
