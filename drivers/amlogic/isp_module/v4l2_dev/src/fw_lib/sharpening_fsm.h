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

#if !defined( __SHARPENING_FSM_H__ )
#define __SHARPENING_FSM_H__



typedef struct _sharpening_fsm_t sharpening_fsm_t;
typedef struct _sharpening_fsm_t *sharpening_fsm_ptr_t;
typedef const struct _sharpening_fsm_t *sharpening_fsm_const_ptr_t;

#define AU_BLACK_HISTORY_SIZE 10
#define SHARP_LUT_SIZE 256

void sharpening_initialize( sharpening_fsm_ptr_t p_fsm );
void sharpening_update( sharpening_fsm_ptr_t p_fsm );
#define AU_NORMALIZE_VALUE 10000000

struct _sharpening_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    uint32_t sharpening_target;
    uint32_t med_strength;
    uint32_t sys_gain;
    uint8_t min_strength;
    uint8_t max_strength;
    uint32_t sharpening_mult;
    uint8_t api_value;
    uint32_t black_val_hist[10];

    uint32_t fullhist_cum_sum[ISP_FULL_HISTOGRAM_SIZE];
};


void sharpening_fsm_clear( sharpening_fsm_ptr_t p_fsm );

void sharpening_fsm_init( void *fsm, fsm_init_param_t *init_param );
int sharpening_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int sharpening_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t sharpening_fsm_process_event( sharpening_fsm_ptr_t p_fsm, event_id_t event_id );

void sharpening_fsm_process_interrupt( sharpening_fsm_const_ptr_t p_fsm, uint8_t irq_event );


#endif /* __SHARPENING_FSM_H__ */
