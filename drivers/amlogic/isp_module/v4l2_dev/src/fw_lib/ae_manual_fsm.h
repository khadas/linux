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

#if !defined( __AE_FSM_H__ )
#define __AE_FSM_H__


#include "sbuf.h"

typedef struct _AE_fsm_t AE_fsm_t;
typedef struct _AE_fsm_t *AE_fsm_ptr_t;
typedef const struct _AE_fsm_t *AE_fsm_const_ptr_t;

#define ISP_HAS_AE_FSM 1

typedef struct _ae_manual_param_t {
    uint32_t pi_coeff;
    uint32_t target_point;
    uint32_t tail_weight;
    uint32_t long_clip;
    uint32_t er_avg_coeff;
    uint32_t AE_tol;
} ae_manual_param_t;
void ae_initialize( AE_fsm_ptr_t p_fsm );
int ae_calculate_exposure( AE_fsm_ptr_t p_fsm );
void ae_roi_update( AE_fsm_ptr_t p_fsm );
void ae_set_new_param( AE_fsm_ptr_t p_fsm, sbuf_ae_t *p_sbuf_ae );
int ae_set_zone_weight(AE_fsm_ptr_t p_fsm, void *u_wg_ptr);

struct _AE_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    int32_t error_log2;
    int32_t ae_hist_mean;
    int32_t exposure_log2;
    int32_t new_exposure_log2;
    int64_t integrator;
    uint32_t exposure_ratio;
    uint32_t new_exposure_ratio;
    uint32_t exposure_ratio_avg;
    uint32_t fullhist[ISP_FULL_HISTOGRAM_SIZE];
    uint32_t fullhist_sum;
    uint32_t ae_roi_api;
    uint32_t roi;
    uint8_t save_hist_api;
#if FW_ZONE_AE
    uint8_t smart_zone_enable;
    uint16_t hist4[ACAMERA_ISP_METERING_AEXP_NODES_USED_HORIZ_DEFAULT * ACAMERA_ISP_METERING_AEXP_NODES_USED_VERT_DEFAULT];
    uint8_t zone_weight[ACAMERA_ISP_METERING_AEXP_NODES_USED_HORIZ_DEFAULT * ACAMERA_ISP_METERING_AEXP_NODES_USED_VERT_DEFAULT];
    uint8_t x1;
    uint8_t y1;
    uint8_t x2;
    uint8_t y2;
#endif
    uint32_t frame_id_tracking;
};


void AE_fsm_clear( AE_fsm_ptr_t p_fsm );

void AE_fsm_init( void *fsm, fsm_init_param_t *init_param );
int AE_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int AE_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t AE_fsm_process_event( AE_fsm_ptr_t p_fsm, event_id_t event_id );

void AE_fsm_process_interrupt( AE_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void AE_request_interrupt( AE_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __AE_FSM_H__ */
