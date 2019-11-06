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

#if !defined( __NOISE_REDUCTION_FSM_H__ )
#define __NOISE_REDUCTION_FSM_H__



typedef struct _noise_reduction_fsm_t noise_reduction_fsm_t;
typedef struct _noise_reduction_fsm_t *noise_reduction_fsm_ptr_t;
typedef const struct _noise_reduction_fsm_t *noise_reduction_fsm_const_ptr_t;

#define MAGIC_GAIN_DIGITAL_FINE 0x03FF
#define MAGIC_TNR_EXP_TRESH 192 * 64
#define NR_LUT_SIZE 128
#define NR_SINTER_BITS 16

void noise_reduction_initialize( noise_reduction_fsm_ptr_t p_fsm );
void noise_reduction_update( noise_reduction_fsm_ptr_t p_fsm );
void noise_reduction_hw_init( noise_reduction_fsm_ptr_t p_fsm );

#include "acamera_metering_stats_mem_config.h"


struct _noise_reduction_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    int16_t tnr_thresh_exp1;
    int16_t tnr_thresh_exp2;
    int16_t snr_thresh_exp1;
    int16_t snr_thresh_exp2;
    uint8_t tnr_exp_thresh;
    uint8_t tnr_exp1_ratio;
    uint8_t tnr_exp2_ratio;
    uint8_t tnr_strength_target;
    uint8_t snr_strength_target;
    int16_t tnr_thresh_master;
    int16_t snr_thresh_master;
    int16_t snr_thresh_contrast;
    uint32_t temper_ev_previous_frame;
    uint32_t temper_diff_avg;
    uint32_t temper_diff_coeff;
};


void noise_reduction_fsm_clear( noise_reduction_fsm_ptr_t p_fsm );

void noise_reduction_fsm_init( void *fsm, fsm_init_param_t *init_param );

uint8_t noise_reduction_fsm_process_event( noise_reduction_fsm_ptr_t p_fsm, event_id_t event_id );

void noise_reduction_fsm_process_interrupt( noise_reduction_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void noise_reduction_request_interrupt( noise_reduction_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __NOISE_REDUCTION_FSM_H__ */
