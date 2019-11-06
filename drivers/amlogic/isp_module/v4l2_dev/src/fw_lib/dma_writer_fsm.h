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

#if !defined( __DMA_WRITER_FSM_H__ )
#define __DMA_WRITER_FSM_H__



typedef struct _dma_writer_fsm_t dma_writer_fsm_t;
typedef struct _dma_writer_fsm_t *dma_writer_fsm_ptr_t;
typedef const struct _dma_writer_fsm_t *dma_writer_fsm_const_ptr_t;

enum _dma_writer_state_t {
    dma_writer_state_wait_for_sensor,
    dma_writer_state_frame_processing,
    dma_writer_state_frame_processing_FR_finished,
    dma_writer_state_frame_processing_wait_events,
    dma_writer_state_frame_processing_DS_finished,
    dma_writer_state_frame_processing_prepare_metadata,
    dma_writer_state_frame_processing_event_processed,
    dma_writer_state_frame_processing_init,
    dma_writer_state_frame_processing_DS2_finished,
    dma_writer_state_frame_processing_buf_reinit,
    dma_writer_state_invalid,
    next_to_dma_writer_state_frame_processing = dma_writer_state_invalid
};

typedef enum _dma_writer_state_t dma_writer_state_t;

#include "acamera.h"
#include "acamera_command_api.h"

typedef struct _frame_buffer_t {
    uint16_t width;
    uint16_t height;
    tframe_t *frame_buf_array;
    uint32_t frame_buf_len;

    uint32_t last_address;
    buffer_callback_t callback;
} frame_buffer_t;

void frame_buffer_initialize( dma_writer_fsm_ptr_t p_fsm );
void dma_writer_deinit( dma_writer_fsm_ptr_t p_fsm );
void frame_buffer_prepare_metadata( dma_writer_fsm_ptr_t p_fsm );
void frame_buffer_fr_finished( dma_writer_fsm_ptr_t p_fsm );
void frame_buffer_ds_finished( dma_writer_fsm_ptr_t p_fsm );
void frame_buffer_check_and_run( dma_writer_fsm_ptr_t p_fsm );
void frame_buffer_queue_reset(dma_writer_fsm_ptr_t p_fsm, dma_type type);
void frame_buffer_get_next_empty_frame(dma_writer_fsm_ptr_t p_fsm, dma_type type);

uint16_t frame_buffer_configure( dma_writer_fsm_ptr_t p_fsm, dma_type dma_output, tframe_t *frame_buf_array, uint32_t frame_buf_len );
uint16_t frame_buffer_reconfigure( dma_writer_fsm_ptr_t p_fsm, dma_type dma_output, tframe_t *frame_buf_array, uint16_t frame_buf_len );
int acamera_frame_fr_set_ready_interrupt( acamera_firmware_t *g_fw );
void dma_writer_update_address_interrupt( dma_writer_fsm_const_ptr_t p_fsm, uint8_t irq_event );
void acamera_frame_buffer_update( dma_writer_fsm_const_ptr_t p_fsm );
void dma_writer_set_path_fps(dma_writer_fsm_ptr_t p_fsm, dma_type type, uint32_t c_fps, uint32_t t_fps);


struct _dma_writer_fsm_t {
    fsm_common_t cmn;

    acamera_fsm_mgr_t *p_fsm_mgr;
    dma_writer_state_t state;
    fsm_irq_mask_t mask;
    uint32_t vflip;
    frame_buffer_t fr_buf;
#if ISP_HAS_DS1
    frame_buffer_t ds_buf;
#endif

    dma_type dma_reader_out;
    uint8_t context_created;
    int32_t dis_ds_base_offset;
    void *handle;
};

void frame_buffer_update_callback( dma_writer_fsm_ptr_t p_fsm, dma_type dma_output, buffer_callback_t callback );
void dma_writer_fsm_clear( dma_writer_fsm_ptr_t p_fsm );
void dma_writer_fsm_init( void *fsm, fsm_init_param_t *init_param );
int dma_writer_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size );
int dma_writer_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size );
uint8_t dma_writer_fsm_process_event( dma_writer_fsm_ptr_t p_fsm, event_id_t event_id );

void dma_writer_fsm_process_interrupt( dma_writer_fsm_const_ptr_t p_fsm, uint8_t irq_event );

void dma_writer_request_interrupt( dma_writer_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask );

#endif /* __DMA_WRITER_FSM_H__ */
