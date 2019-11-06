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

#if !defined( __FSM_PARAMETERS_H__ )
#define __FSM_PARAMETERS_H__

#include "acamera_types.h"
#include "acamera.h"
#include "fsm_intf.h"
#include "fsm_param_id.h"

enum {
    CMOS_CURRENT_EXPOSURE_LOG2,
    CMOS_MAX_EXPOSURE_LOG2,
};

enum {
    AF_MODE_AF = 0,
    AF_MODE_CAF = 1,
    AF_MODE_MANUAL = 2,
    AF_MODE_CALIBRATION = 3,
};

typedef struct _fsm_param_sensor_info_ {
    uint16_t total_width;
    uint16_t total_height;
    uint16_t active_width;
    uint16_t active_height;
    uint16_t pixels_per_line;
    uint32_t lines_per_second;

    int32_t again_log2_max;
    int32_t dgain_log2_max;
    uint32_t integration_time_min;
    uint32_t integration_time_max;
    uint32_t integration_time_long_max;
    uint32_t integration_time_limit;

    uint16_t again;
    uint16_t dgain;
    uint8_t integration_time_apply_delay;

    int32_t sensor_exp_number;
    uint8_t isp_exposure_channel_delay;

    uint8_t resolution_mode;
    uint16_t isp_output_mode;
    uint16_t black_level;
    uint8_t sensor_bits;
} fsm_param_sensor_info_t;

typedef struct _fsm_param_ae_info_ {
    int32_t exposure_log2;
    int32_t ae_hist_mean;
    uint32_t exposure_ratio;
    int32_t error_log2;
} fsm_param_ae_info_t;

typedef struct _fsm_param_ae_hist_info_ {
    uint32_t fullhist_sum;
    const uint32_t *fullhist;
    uint32_t frame_id;
} fsm_param_ae_hist_info_t;


enum fsm_param_ae_mode_bit {
    AE_MODE_BIT_MANUAL_INT_TIME = ( 1 << 0 ),
    AE_MODE_BIT_MANUAL_GAIN_MODE = ( 1 << 1 ),
};

typedef struct _fsm_param_ae_mode_ {
    uint32_t manual_integration_time;
    uint8_t manual_gain_mode;
    uint16_t flag;
} fsm_param_ae_mode_t;

typedef struct _fsm_param_gain_calc_param_ {
    uint8_t shift_in;
    uint8_t shift_out;
} fsm_param_gain_calc_param_t;

typedef struct _fsm_param_ccm_info_ {
    uint8_t light_source;
    uint8_t light_source_previous;
    uint8_t light_source_ccm;
    uint8_t light_source_ccm_previous;
    uint8_t light_source_change_frames;
    uint8_t light_source_change_frames_left;
} fsm_param_ccm_info_t;

typedef struct _fsm_param_sensor_int_time_ {
    uint16_t int_time;
    uint16_t int_time_M;
    uint16_t int_time_M2;
    uint16_t int_time_L;
} fsm_param_sensor_int_time_t;


typedef struct _fsm_param_exposure_target_ {
    int32_t exposure_log2;
    uint32_t exposure_ratio;
    uint32_t frame_id_tracking;
} fsm_param_exposure_target_t;

typedef struct _fsm_param_crop_info_ {
    uint16_t width_fr;
    uint16_t height_fr;
#if ISP_HAS_DS1
    uint16_t width_ds;
    uint16_t height_ds;
#endif
} fsm_param_crop_info_t;

typedef struct _fsm_param_dma_pipe_setting_buf_ {
    dma_type pipe_id;
    tframe_t *buf_array;
    uint32_t buf_len;
    buffer_callback_t callback;
} fsm_param_dma_pipe_setting_t;

typedef struct _fsm_param_path_fps_ {
	dma_type pipe_id;
	uint32_t c_fps;
	uint32_t t_fps;
}fsm_param_path_fps_t;


typedef struct _fsm_param_dma_frame_ {
    aframe_t **p_frame;
} fsm_param_dma_frame_t;

enum fsm_param_awb_info_bit {
    AWB_INFO_TEMPERATURE_DETECTED = ( 1 << 0 ),
    AWB_INFO_P_HIGH = ( 1 << 1 ),
    AWB_INFO_LIGHT_SOURCE_CANDIDATE = ( 1 << 2 ),
};

typedef struct _fsm_param_wb_info_ {
    int32_t wb_log2[4];
    int32_t temperature_detected;
    uint8_t p_high;
    uint8_t light_source_candidate;
    uint32_t avg_GR;
    uint32_t avg_GB;
    uint16_t rg_coef;
    uint16_t bg_coef;
    int32_t awb_warming[3];
    // flag is a combination of fsm_param_awb_info_bit.
    uint32_t flag;
    uint32_t result_gain_frame_id;
} fsm_param_awb_info_t;

typedef struct _fsm_param_af_info_ {
    uint16_t last_position;
    int32_t last_sharp;
    uint8_t skip_frame;
} fsm_param_af_info_t;


typedef struct _fsm_param_gamma_result_ {
    uint32_t gamma_gain;
    uint32_t gamma_offset;
} fsm_param_gamma_result_t;

typedef struct _fsm_param_defect_pixel_info_ {
    uint8_t hp_started;
} fsm_param_defect_pixel_info_t;


typedef struct _fsm_param_roi_ {
    uint32_t roi;
    uint32_t roi_api;
} fsm_param_roi_t;


enum fsm_param_crop_setting_bit {
    CROP_SETTING_BIT_RESIZE_TYPE = ( 1 << 0 ),
    CROP_SETTING_BIT_XSIZE = ( 1 << 1 ),
    CROP_SETTING_BIT_YSIZE = ( 1 << 2 ),
    CROP_SETTING_BIT_XOFFSET = ( 1 << 3 ),
    CROP_SETTING_BIT_YOFFSET = ( 1 << 4 ),
    CROP_SETTING_BIT_ENABLE = ( 1 << 5 ),
    CROP_SETTING_BIT_DONE = ( 1 << 6 ),
    CROP_SETTING_BIT_BANK = ( 1 << 7 ),
};

typedef struct _fsm_param_crop_setting_ {
    uint16_t resize_type;
    uint16_t xsize;
    uint16_t ysize;
    uint16_t xoffset;
    uint16_t yoffset;
    uint8_t enable;
    uint8_t done;
    uint16_t bank;
    // flag is a combination of fsm_param_crop_info_bit.
    uint32_t flag;
} fsm_param_crop_setting_t;


enum fsm_param_reg_setting_bit {
    REG_SETTING_BIT_REG_ADDR = ( 1 << 0 ),
    REG_SETTING_BIT_REG_SIZE = ( 1 << 1 ),
    REG_SETTING_BIT_REG_SOURCE = ( 1 << 2 ),
    REG_SETTING_BIT_REG_VALUE = ( 1 << 3 ),
};

typedef struct _fsm_param_reg_setting_ {
    uint32_t api_reg_addr;
    uint16_t api_reg_size;
    uint16_t api_reg_source;
    uint32_t api_reg_value;
    uint32_t flag;
} fsm_param_reg_setting_t;

typedef struct _fsm_param_reg_cfg_ {
    uint32_t reg_addr;
    uint32_t reg_value;
} fsm_param_reg_cfg_t;

typedef struct _fsm_param_ccm_manual_ {
    uint8_t manual_CCM;
    int16_t manual_color_matrix[9];
} fsm_param_ccm_manual_t;

typedef struct _fsm_param_set_wdr_param_ {
    uint32_t wdr_mode;
    uint32_t exp_number;
} fsm_param_set_wdr_param_t;

// #if !defined(ISP_HAS_CMOS_FSM) || !ISP_HAS_CMOS_FSM
// Copied below 3 structs definition from cmos_fsm.h file
typedef struct _exposure_data_set_t {
    uint32_t integration_time;
    uint32_t again_reg;
    uint32_t dgain_reg;
    int32_t isp_dgain_log2;
    // Below definition is copied from cmos.fsm
    uint32_t exposure_ratio;
    uint32_t actual_integration_time;

    uint32_t exposure_ratio_short;
    uint32_t integration_time_long;

    uint32_t exposure_ratio_medium;
    uint32_t integration_time_medium;

    uint32_t exposure_ratio_medium2;
    uint32_t integration_time_medium2;

    uint32_t frame_id_tracking;
} exposure_data_set_t;

typedef struct _exposure_info_set_t {
    int32_t exposure_log2;
    int32_t again_log2;
    int32_t dgain_log2;
    int32_t isp_dgain_log2;
} exposure_info_set_t;

typedef struct _exposure_set_t {
    exposure_info_set_t info;
    exposure_data_set_t data;
} exposure_set_t;

// #endif /* ISP_HAS_CMOS_FSM */


enum fsm_param_mon_type {
    // monitor type: Error
    MON_TYPE_ERR_CALIBRATION_LUT_NULL = 1,

    MON_TYPE_ERR_CMOS_FS_DELAY,
    MON_TYPE_ERR_CMOS_UPDATE_NOT_IN_VB,
    MON_TYPE_ERR_CMOS_UPDATE_DGAIN_WRONG_TIMING,

    MON_TYPE_ERR_IRIDIX_UPDATE_NOT_IN_VB,

    // monitor type: Algorithm
    MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MIN,
    MON_TYPE_STATUS_ALG_DELAY_IN2OUT_CUR,
    MON_TYPE_STATUS_ALG_DELAY_IN2OUT_MAX,

    MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MIN,
    MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_CUR,
    MON_TYPE_STATUS_ALG_DELAY_OUT2APPLY_MAX,

    MON_TYPE_STATUS_ALG_FPT_MIN,
    MON_TYPE_STATUS_ALG_FPT_CUR,
    MON_TYPE_STATUS_ALG_FPT_MAX,
    MON_TYPE_STATUS_ALG_STATUS_RESET,
};

typedef struct _fsm_param_mon_err_head_ {
    uint32_t err_type;
    uint32_t err_param;
} fsm_param_mon_err_head_t;


typedef struct _fsm_param_mon_status_head_ {
    uint32_t status_type;
    uint32_t status_param;
} fsm_param_mon_status_head_t;

enum fsm_param_mon_alg_flow_state {
    MON_ALG_FLOW_STATE_START,
    MON_ALG_FLOW_STATE_INPUT_READY = MON_ALG_FLOW_STATE_START,
    MON_ALG_FLOW_STATE_OUTPUT_READY,
    MON_ALG_FLOW_STATE_APPLIED,
    MON_ALG_FLOW_AE_STATE_END = MON_ALG_FLOW_STATE_APPLIED,
};

typedef struct _fsm_param_mon_alg_flow_ {
    uint32_t frame_id_tracking;
    uint32_t frame_id_current;
    uint32_t flow_state;
} fsm_param_mon_alg_flow_t;

typedef struct _fsm_param_sys_status_info_ {
    int32_t total_gain_log2;
    int32_t exposure_log2;
} sys_status_info_t;

typedef struct _fsm_param_awb_status_info_ {
    uint32_t mix_light_contrast;
} awb_status_info_t;

typedef struct _fsm_param_af_status_info_ {
    uint32_t lens_pos;
    uint32_t focus_value;
} af_status_info_t;

typedef struct _fsm_param_fw_status_info_ {
    sys_status_info_t sys_info;
    awb_status_info_t awb_info;
    af_status_info_t af_info;
} status_info_param_t;

int acamera_fsm_mgr_get_param( acamera_fsm_mgr_t *p_fsm_mgr, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );
int acamera_fsm_mgr_set_param( acamera_fsm_mgr_t *p_fsm_mgr, uint32_t param_id, void *input, uint32_t input_size );

void acamera_fsm_mgr_dma_writer_update_address_interrupt( acamera_fsm_mgr_t *p_fsm_mgr, uint8_t irq_event );


#endif /* __FSM_PARAMETERS_H__ */
