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

#if !defined( __CROP_FSM_H__ )
#define __CROP_FSM_H__



typedef struct _crop_fsm_t crop_fsm_t;
typedef struct _crop_fsm_t *crop_fsm_ptr_t;
typedef const struct _crop_fsm_t *crop_fsm_const_ptr_t;

typedef struct _crop_t {
    uint8_t enable;
    uint8_t done;
    uint16_t xsize;
    uint16_t ysize;
    uint16_t xoffset;
    uint16_t yoffset;
} crop_t;
typedef struct _scaler_t {
    uint8_t enable;
    uint8_t done;
    uint16_t xsize;
    uint16_t ysize;
    uint16_t bank;
} scaler_t;
void crop_initialize( crop_fsm_ptr_t p_fsm );
void crop_resolution_changed( crop_fsm_ptr_t p_fsm );

struct _crop_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    fsm_irq_mask_t mask;
    uint16_t resize_type;
    crop_t crop_fr;
    uint16_t width_fr;
    uint16_t height_fr;


#if ISP_HAS_DS1
    crop_t crop_ds;
    scaler_t scaler_ds;
    uint16_t width_ds;
    uint16_t height_ds;
#endif
    uint8_t need_updating;
};


void crop_fsm_clear( crop_fsm_ptr_t p_fsm );
void crop_fsm_init( void *fsm, fsm_init_param_t *init_param );

int crop_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int crop_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );

uint8_t crop_fsm_process_event( crop_fsm_ptr_t p_fsm, event_id_t event_id );

void crop_fsm_process_interrupt( crop_fsm_const_ptr_t p_fsm, uint8_t irq_event );

int crop_validate_size( crop_fsm_t *p_fsm, uint16_t type, uint16_t sizeto, int isWidth );

void crop_request_interrupt( crop_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __CROP_FSM_H__ */
