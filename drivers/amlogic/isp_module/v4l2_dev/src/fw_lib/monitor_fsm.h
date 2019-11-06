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

#if !defined( __MONITOR_FSM_H__ )
#define __MONITOR_FSM_H__



#define ISP_HAS_MONITOR_FSM 1

#define MONITOR_ALG_FLOW_ARR_SIZE 8

typedef enum {
    MON_ALG_AE_INDEX,
    MON_ALG_AWB_INDEX,
    MON_ALG_GAMMA_INDEX,
    MON_ALG_IRIDIX_INDEX,
    MON_ALG_INDEX_MAX,
} mon_alg_index_t;

typedef struct _mon_alg_item_ {
    uint32_t frame_id_tracking;
    uint32_t frame_id_alg_state_input;
    uint32_t frame_id_alg_state_output;
    uint32_t frame_id_alg_state_apply;
    uint32_t item_is_using;
} mon_alg_item_t;

typedef struct _mon_alg_info_ {
    char *alg_name;
    mon_alg_item_t alg_state_arr[MONITOR_ALG_FLOW_ARR_SIZE];
    uint32_t alg_arr_write_pos;
    uint32_t alg_reset_status;

    uint32_t mon_alg_frame_count;
    uint32_t alg_delay_in2out_min;
    uint32_t alg_delay_in2out_cur;
    uint32_t alg_delay_in2out_max;

    uint32_t alg_delay_out2apply_min;
    uint32_t alg_delay_out2apply_cur;
    uint32_t alg_delay_out2apply_max;

    // fpt: algorithom execute N frames per time
    uint32_t alg_fpt_prev_out_id;
    uint32_t alg_fpt_min;
    uint32_t alg_fpt_cur;
    uint32_t alg_fpt_max;
} mon_alg_info_t;

typedef struct _mon_err_calibration_info {
    uint32_t err_cnt_calibration_lut_null;
    uint32_t err_param_calibration_lut_null_idx;
} mon_err_calibration_info_t;

typedef struct _mon_err_cmos_info {
    uint32_t err_cnt_cmos_fs_delay;
    uint32_t err_cnt_cmos_fe_update_not_in_vb;
    uint32_t err_cnt_cmos_dgain_update_wrong_timing;
    uint32_t err_cmos_dgain_update_wrong_timing_diff;
} mon_err_cmos_info_t;


typedef struct _mon_err_iridix_info {
    uint32_t err_cnt_iridix_fe_update_not_in_vb;
} mon_err_iridix_info_t;

typedef struct _monitor_fsm_t monitor_fsm_t;
typedef struct _monitor_fsm_t *monitor_fsm_ptr_t;
typedef const struct _monitor_fsm_t *monitor_fsm_const_ptr_t;

struct _monitor_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;

    mon_err_calibration_info_t mon_info_cali;

    mon_err_cmos_info_t mon_info_cmos;

    mon_err_iridix_info_t mon_info_iridix;

    mon_alg_info_t mon_alg_arr[MON_ALG_INDEX_MAX];
};


void monitor_initialize( monitor_fsm_ptr_t p_fsm );
void monitor_func_calc( monitor_fsm_ptr_t p_fsm );

void monitor_fsm_clear( monitor_fsm_ptr_t p_fsm );

void monitor_fsm_init( void *fsm, fsm_init_param_t *init_param );

int monitor_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int monitor_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t monitor_fsm_process_event( monitor_fsm_ptr_t p_fsm, event_id_t event_id );
void monitor_fsm_process_interrupt( monitor_fsm_const_ptr_t p_fsm, uint8_t irq_event );
void monitor_request_interrupt( monitor_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

void monitor_handle_error_report( monitor_fsm_ptr_t p_fsm, fsm_param_mon_err_head_t *p_mon_err_head );
void monitor_handle_reset_error_report( monitor_fsm_ptr_t p_fsm, uint32_t err_type );
void monitor_get_error_report( monitor_fsm_ptr_t p_fsm, uint32_t err_type, uint32_t *err_param );

void monitor_handle_alg_flow( monitor_fsm_ptr_t p_fsm, mon_alg_info_t *p_mon_info, fsm_param_mon_alg_flow_t *p_flow );
void monitor_get_alg_status( monitor_fsm_ptr_t p_fsm, mon_alg_info_t *p_mon_info, uint32_t status_type, uint32_t *status );
void monitor_set_alg_status( monitor_fsm_ptr_t p_fsm, mon_alg_info_t *p_mon_info, fsm_param_mon_status_head_t *p_status );

#endif /* __MONITOR_FSM_H__ */
