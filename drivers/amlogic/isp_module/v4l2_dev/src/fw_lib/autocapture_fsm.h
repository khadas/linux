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

#if !defined( __AUTOCAPTURE_FSM_H__ )
#define __AUTOCAPTURE_FSM_H__

typedef struct _autocapture_fsm_t autocapture_fsm_t;
typedef struct _autocapture_fsm_t *autocapture_fsm_ptr_t;
typedef const struct _autocapture_fsm_t *autocapture_fsm_const_ptr_t;

void autocapture_initialize( autocapture_fsm_ptr_t p_fsm );

struct _autocapture_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
};

struct module_cfg_info {
	uint8_t enable;
	uint8_t p_type;
	uint8_t planes;
	uint32_t frame_size0;
	uint32_t frame_buffer_start0;
	uint32_t frame_size1;
	uint32_t frame_buffer_start1;
};

void autocapture_fsm_clear( autocapture_fsm_ptr_t p_fsm );

void autocapture_fsm_init( void *fsm, fsm_init_param_t *init_param );
void autocapture_deinit( autocapture_fsm_ptr_t p_fsm );
void autocapture_hwreset(autocapture_fsm_ptr_t p_fsm );

int autocapture_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );

uint8_t autocapture_fsm_process_event( autocapture_fsm_ptr_t p_fsm, event_id_t event_id );
void autocapture_fsm_process_interrupt( autocapture_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void autocap_set_new_param(struct module_cfg_info *m_cfg);

#endif /* __AUTOCAPTURE_FSM_H__ */
