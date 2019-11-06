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

#if !defined(__ACAMERA_FSM_MGR_H__)
#define __ACAMERA_FSM_MGR_H__



#include "acamera_firmware_config.h"
#include "acamera_types.h"

#if !defined(__ACAMERA_FW_H__)
#error "acamera_fsm_mgr.h file should not be included from source file, include acamera_fw.h instead!"
#endif

typedef enum _event_id_t
{
    event_id_WB_matrix_ready,
    event_id_acamera_reset_sensor_hw,
    event_id_ae_result_ready,
    event_id_ae_stats_ready,
    event_id_af_refocus,
    event_id_af_stats_ready,
    event_id_antiflicker_changed,
    event_id_awb_result_ready,
    event_id_awb_stats_ready,
    event_id_cmos_refresh,
    event_id_crop_changed,
    event_id_crop_updated,
    event_id_exposure_changed,
    event_id_frame_buf_reinit,
    event_id_frame_buffer_ds_ready,
    event_id_frame_buffer_fr_ready,
    event_id_frame_buffer_metadata,
    event_id_frame_end,
    event_id_gamma_lut_ready,
    event_id_gamma_new_param_ready,
    event_id_gamma_stats_ready,
    event_id_metadata_ready,
    event_id_metadata_update,
    event_id_monitor_frame_end,
    event_id_monitor_notify_other_fsm,
    event_id_new_frame,
    event_id_sensor_not_ready,
    event_id_sensor_ready,
    event_id_sensor_sw_reset,
    event_id_sharp_lut_update,
    event_id_update_iridix,
    event_id_update_sharp_lut,
    event_id_drop_frame,
    number_of_event_ids
} event_id_t;


typedef enum _fsm_id_t
{
    FSM_ID_SENSOR,
    FSM_ID_CMOS,
    FSM_ID_CROP,
    FSM_ID_GENERAL,
    FSM_ID_AE,
    FSM_ID_AWB,
    FSM_ID_COLOR_MATRIX,
    FSM_ID_IRIDIX,
    FSM_ID_NOISE_REDUCTION,
    FSM_ID_SHARPENING,
    FSM_ID_MATRIX_YUV,
    FSM_ID_GAMMA_MANUAL,
    FSM_ID_MONITOR,
    FSM_ID_SBUF,
    FSM_ID_DMA_WRITER,
    FSM_ID_METADATA,
    FSM_ID_AF,
    FSM_ID_MAX
} fsm_id_t;

#include "fsm_intf.h"



#include "acamera_event_queue.h"

struct _acamera_fsm_mgr_t
{
    uint8_t ctx_id;
    uintptr_t isp_base;
    acamera_context_ptr_t p_ctx;
    isp_info_t info;
    fsm_common_t *fsm_arr[FSM_ID_MAX];
    acamera_event_queue_t event_queue;
    uint8_t event_queue_data[ACAMERA_EVENT_QUEUE_SIZE];
    uint32_t reserved;
};

void acamera_fsm_mgr_raise_event(acamera_fsm_mgr_t *p_fsm_mgr, event_id_t event_id);

#define fsm_raise_event(p_fsm,event_id) \
    acamera_fsm_mgr_raise_event((p_fsm)->p_fsm_mgr,event_id)

void acamera_fsm_mgr_init(acamera_fsm_mgr_t *p_fsm_mgr);
void acamera_fsm_mgr_deinit(acamera_fsm_mgr_t *p_fsm_mgr);
void acamera_fsm_mgr_process_interrupt(acamera_fsm_mgr_t *p_fsm_mgr, uint8_t event);
void acamera_fsm_mgr_process_events(acamera_fsm_mgr_t *p_fsm_mgr, int n_max_events);

#endif
