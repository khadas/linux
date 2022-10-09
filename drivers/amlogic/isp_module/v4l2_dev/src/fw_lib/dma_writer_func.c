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
#include <linux/vmalloc.h>
#include "acamera_fw.h"
#include "acamera_math.h"
#include "acamera_firmware_config.h"
#if ISP_HAS_FPGA_WRAPPER
#include "acamera_fpga_config.h"
#endif
#include "acamera_isp_config.h"
#include "acamera_command_api.h"
#include "dma_writer_api.h"
#include "system_stdlib.h"
#include "dma_writer_fsm.h"


#define NUMBER_OF_USED_BANKS 3

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_DMA_WRITER
#endif


void dma_writer_fsm_process_interrupt( dma_writer_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{

    LOG( LOG_DEBUG, "dma writer fsm irq event %d", (int)irq_event );
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    // send this interrupt to each pipe
    dma_writer_process_interrupt( p_fsm->handle, irq_event );
    switch ( irq_event ) {
    case ACAMERA_IRQ_FRAME_WRITER_FR:

        fsm_raise_event( p_fsm, event_id_frame_buffer_fr_ready );
        fsm_raise_event( p_fsm, event_id_frame_buffer_metadata );

        break;
    case ACAMERA_IRQ_FRAME_WRITER_DS:
        fsm_raise_event( p_fsm, event_id_frame_buffer_ds_ready );

        break;
    }

    return;
}

void acamera_frame_buffer_update( dma_writer_fsm_const_ptr_t p_fsm )
{
    dma_error result = edma_ok;
    fsm_param_crop_info_t crop_info;
#ifdef ISP_HAS_CROP_FSM
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CROP_INFO, NULL, 0, &crop_info, sizeof( crop_info ) );
#else
    crop_info.width_fr = acamera_isp_top_active_width_read( p_fsm->cmn.isp_base );
    crop_info.height_fr = acamera_isp_top_active_height_read( p_fsm->cmn.isp_base );
#if ISP_HAS_DS1
    crop_info.width_ds = crop_info.width_fr;
    crop_info.height_ds = crop_info.height_fr;
#endif
#endif
    dma_pipe_settings *set_fr = NULL;
    set_fr = vmalloc(sizeof(dma_pipe_settings));
	if(set_fr == NULL){
        LOG(LOG_ERR, "Failed to malloc mem");
        return;
    }

    dma_writer_get_settings( p_fsm->handle, dma_fr, set_fr );

    set_fr->height = crop_info.height_fr;
    set_fr->width = crop_info.width_fr;
    set_fr->callback = p_fsm->fr_buf.callback; //callback might have been updated

    set_fr->active = ( p_fsm->dma_reader_out == dma_fr );
    set_fr->vflip = p_fsm->vflip;
    result |= dma_writer_set_settings( p_fsm->handle, dma_fr, set_fr );

    if ( result == edma_ok )
        dma_writer_set_initialized( p_fsm->handle, dma_fr, 1 );
    else
        dma_writer_set_initialized( p_fsm->handle, dma_fr, 0 );

    if(set_fr)
	{
	    vfree(set_fr);
		set_fr = NULL;
    }
	
#if ISP_HAS_DS1
    result = edma_ok;
    dma_pipe_settings *set_ds1 = NULL;
    set_ds1 = vmalloc(sizeof(dma_pipe_settings));
	if(set_ds1 == NULL){
        LOG(LOG_ERR, "Failed to malloc mem");
        return;
    }

    dma_writer_get_settings( p_fsm->handle, dma_ds1, set_ds1 );
    set_ds1->height = crop_info.height_ds;
    set_ds1->width = crop_info.width_ds;
    set_ds1->callback = p_fsm->ds_buf.callback;

    set_ds1->active = ( p_fsm->dma_reader_out == dma_ds1 );
    set_ds1->vflip = p_fsm->vflip;
    result |= dma_writer_set_settings( p_fsm->handle, dma_ds1, set_ds1 );

    if ( result == edma_ok )
        dma_writer_set_initialized( p_fsm->handle, dma_ds1, 1 );
    else
        dma_writer_set_initialized( p_fsm->handle, dma_ds1, 0 );
    if(set_ds1)
	{
	    vfree(set_ds1);
		set_ds1 = NULL;
    }
#endif
}

void frame_buffer_initialize( dma_writer_fsm_ptr_t p_fsm )
{
    // register interrupts
    p_fsm->mask.repeat_irq_mask = ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_START )
        | ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_WRITER_FR ) | ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_WRITER_DS )
        | ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_DROP_FR ) | ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_DROP_DS );

    dma_writer_request_interrupt( p_fsm, p_fsm->mask.repeat_irq_mask );

// DS buffer
#if ISP_HAS_FPGA_WRAPPER && ISP_CONTROLS_DMA_READER
    //  FRAME READER tab (address will be initialized in the first interrupt
    acamera_fpga_frame_reader_axi_port_enable_write( p_fsm->cmn.isp_base, 1 );
#endif

    if ( p_fsm->context_created == 0 ) {
        // create dma_pipe_hanlde
        dma_writer_create( &p_fsm->handle );
        p_fsm->context_created = 1;
        p_fsm->dma_reader_out = dma_fr; //default out to dma reader
    }
    if ( p_fsm->handle != NULL ) {
        dma_error result = edma_ok;
        fsm_param_crop_info_t crop_info;
#ifdef ISP_HAS_CROP_FSM
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CROP_INFO, NULL, 0, &crop_info, sizeof( crop_info ) );
        LOG( LOG_DEBUG, "crop settings %d x %d", (int)crop_info.width_fr, (int)crop_info.height_fr );
#else
        crop_info.width_fr = acamera_isp_top_active_width_read( p_fsm->cmn.isp_base );
        crop_info.height_fr = acamera_isp_top_active_height_read( p_fsm->cmn.isp_base );
#if ISP_HAS_DS1
        crop_info.width_ds = crop_info.width_fr;
        crop_info.height_ds = crop_info.height_fr;
        LOG( LOG_DEBUG, "crop settings %d x %d and %d x %d", (int)crop_info.width_fr, (int)crop_info.height_fr, (int)crop_info.width_ds, (int)crop_info.height_ds );
#endif

#endif

        // initialize fr pipe
        dma_pipe_settings *set_fr = NULL;
        set_fr = vmalloc(sizeof(dma_pipe_settings));
		if(set_fr == NULL){
			LOG(LOG_ERR, "Failed to malloc mem");
			return;
		}
        dma_api api_fr;
        // api
        api_fr.p_acamera_isp_dma_writer_format_read = acamera_isp_fr_dma_writer_format_read;
        api_fr.p_acamera_isp_dma_writer_format_write = acamera_isp_fr_dma_writer_format_write;
        api_fr.p_acamera_isp_dma_writer_max_bank_write = acamera_isp_fr_dma_writer_max_bank_write;
        api_fr.p_acamera_isp_dma_writer_bank0_base_write = acamera_isp_fr_dma_writer_bank0_base_write;
#if ISP_HAS_FPGA_WRAPPER
        api_fr.p_acamera_fpga_frame_reader_rbase_write = acamera_fpga_frame_reader_rbase_write;
        api_fr.p_acamera_fpga_frame_reader_line_offset_write = acamera_fpga_frame_reader_line_offset_write;
        api_fr.p_acamera_fpga_frame_reader_format_write = acamera_fpga_frame_reader_format_write;
        api_fr.p_acamera_fpga_frame_reader_rbase_load_write = acamera_fpga_frame_reader_rbase_load_write;
#endif
        api_fr.p_acamera_isp_dma_writer_line_offset_write = acamera_isp_fr_dma_writer_line_offset_write;
        api_fr.p_acamera_isp_dma_writer_line_offset_write = acamera_isp_fr_dma_writer_line_offset_write;

        api_fr.p_acamera_isp_dma_writer_frame_write_on_write = acamera_isp_fr_dma_writer_frame_write_on_write;
        api_fr.p_acamera_isp_dma_writer_active_width_write = acamera_isp_fr_dma_writer_active_width_write;
        api_fr.p_acamera_isp_dma_writer_active_height_write = acamera_isp_fr_dma_writer_active_height_write;
        api_fr.p_acamera_isp_dma_writer_active_width_read = acamera_isp_fr_dma_writer_active_width_read;
        api_fr.p_acamera_isp_dma_writer_active_height_read = acamera_isp_fr_dma_writer_active_height_read;


        api_fr.p_acamera_isp_dma_writer_format_read_uv = acamera_isp_fr_uv_dma_writer_format_read;
        api_fr.p_acamera_isp_dma_writer_format_write_uv = acamera_isp_fr_uv_dma_writer_format_write;
        api_fr.p_acamera_isp_dma_writer_bank0_base_write_uv = acamera_isp_fr_uv_dma_writer_bank0_base_write;
        api_fr.p_acamera_isp_dma_writer_max_bank_write_uv = acamera_isp_fr_uv_dma_writer_max_bank_write;
        api_fr.p_acamera_isp_dma_writer_line_offset_write_uv = acamera_isp_fr_uv_dma_writer_line_offset_write;
        api_fr.p_acamera_isp_dma_writer_frame_write_on_write_uv = acamera_isp_fr_uv_dma_writer_frame_write_on_write;
        api_fr.p_acamera_isp_dma_writer_active_width_write_uv = acamera_isp_fr_uv_dma_writer_active_width_write;
        api_fr.p_acamera_isp_dma_writer_active_height_write_uv = acamera_isp_fr_uv_dma_writer_active_height_write;
        api_fr.p_acamera_isp_dma_writer_active_width_read_uv = acamera_isp_fr_uv_dma_writer_active_width_read;
        api_fr.p_acamera_isp_dma_writer_active_height_read_uv = acamera_isp_fr_uv_dma_writer_active_height_read;
#if ISP_HAS_FPGA_WRAPPER
        api_fr.p_acamera_fpga_frame_reader_rbase_write_uv = acamera_fpga_frame_reader_uv_rbase_write;
        api_fr.p_acamera_fpga_frame_reader_line_offset_write_uv = acamera_fpga_frame_reader_uv_line_offset_write;
        api_fr.p_acamera_fpga_frame_reader_format_write_uv = acamera_fpga_frame_reader_uv_format_write;
        api_fr.p_acamera_fpga_frame_reader_rbase_load_write_uv = acamera_fpga_frame_reader_uv_rbase_load_write;
#endif
        // settings
        dma_writer_get_settings( p_fsm->handle, dma_fr, set_fr );
        set_fr->p_ctx = ACAMERA_FSM2CTX_PTR( p_fsm ); //back reference

        set_fr->height = crop_info.height_fr;
        set_fr->width = crop_info.width_fr;

        set_fr->callback = p_fsm->fr_buf.callback;
        set_fr->isp_base = p_fsm->cmn.isp_base;
        set_fr->ctx_id = ACAMERA_FSM2CTX_PTR( p_fsm )->context_id;

        set_fr->active = ( p_fsm->dma_reader_out == dma_fr );
        set_fr->vflip = p_fsm->vflip;
        result |= dma_writer_init( p_fsm->handle, dma_fr, set_fr, &api_fr );


        if ( result == edma_ok )
            dma_writer_set_initialized( p_fsm->handle, dma_fr, 1 );
        else
            dma_writer_set_initialized( p_fsm->handle, dma_fr, 0 );

		if(set_fr)
		{
			vfree(set_fr);
			set_fr = NULL;
		}
		
#if ISP_HAS_DS1
        // initialize ds pipe
        result = edma_ok;
        dma_pipe_settings *set_ds1 = NULL;
		set_ds1 = vmalloc(sizeof(dma_pipe_settings));
		if(set_ds1 == NULL){
		    LOG(LOG_ERR, "Failed to malloc mem");
		    return;
		}
        dma_api api_ds1;
        // api
        api_ds1.p_acamera_isp_dma_writer_format_read = acamera_isp_ds1_dma_writer_format_read;
        api_ds1.p_acamera_isp_dma_writer_format_write = acamera_isp_ds1_dma_writer_format_write;
        api_ds1.p_acamera_isp_dma_writer_max_bank_write = acamera_isp_ds1_dma_writer_max_bank_write;
        api_ds1.p_acamera_isp_dma_writer_bank0_base_write = acamera_isp_ds1_dma_writer_bank0_base_write;
#if ISP_HAS_FPGA_WRAPPER
        api_ds1.p_acamera_fpga_frame_reader_rbase_write = acamera_fpga_frame_reader_rbase_write;
        api_ds1.p_acamera_fpga_frame_reader_line_offset_write = acamera_fpga_frame_reader_line_offset_write;
        api_ds1.p_acamera_fpga_frame_reader_format_write = acamera_fpga_frame_reader_format_write;
        api_ds1.p_acamera_fpga_frame_reader_rbase_load_write = acamera_fpga_frame_reader_rbase_load_write;
#endif
        api_ds1.p_acamera_isp_dma_writer_line_offset_write = acamera_isp_ds1_dma_writer_line_offset_write;
        api_ds1.p_acamera_isp_dma_writer_line_offset_write = acamera_isp_ds1_dma_writer_line_offset_write;

        api_ds1.p_acamera_isp_dma_writer_frame_write_on_write = acamera_isp_ds1_dma_writer_frame_write_on_write;
        api_ds1.p_acamera_isp_dma_writer_active_width_write = acamera_isp_ds1_dma_writer_active_width_write;
        api_ds1.p_acamera_isp_dma_writer_active_height_write = acamera_isp_ds1_dma_writer_active_height_write;
        api_ds1.p_acamera_isp_dma_writer_active_width_read = acamera_isp_ds1_dma_writer_active_width_read;
        api_ds1.p_acamera_isp_dma_writer_active_height_read = acamera_isp_ds1_dma_writer_active_height_read;


        api_ds1.p_acamera_isp_dma_writer_format_read_uv = acamera_isp_ds1_uv_dma_writer_format_read;
        api_ds1.p_acamera_isp_dma_writer_format_write_uv = acamera_isp_ds1_uv_dma_writer_format_write;
        api_ds1.p_acamera_isp_dma_writer_bank0_base_write_uv = acamera_isp_ds1_uv_dma_writer_bank0_base_write;
        api_ds1.p_acamera_isp_dma_writer_max_bank_write_uv = acamera_isp_ds1_uv_dma_writer_max_bank_write;
        api_ds1.p_acamera_isp_dma_writer_line_offset_write_uv = acamera_isp_ds1_uv_dma_writer_line_offset_write;
        api_ds1.p_acamera_isp_dma_writer_frame_write_on_write_uv = acamera_isp_ds1_uv_dma_writer_frame_write_on_write;
        api_ds1.p_acamera_isp_dma_writer_active_width_write_uv = acamera_isp_ds1_uv_dma_writer_active_width_write;
        api_ds1.p_acamera_isp_dma_writer_active_height_write_uv = acamera_isp_ds1_uv_dma_writer_active_height_write;
        api_ds1.p_acamera_isp_dma_writer_active_width_read_uv = acamera_isp_ds1_uv_dma_writer_active_width_read;
        api_ds1.p_acamera_isp_dma_writer_active_height_read_uv = acamera_isp_ds1_uv_dma_writer_active_height_read;
#if ISP_HAS_FPGA_WRAPPER
        api_ds1.p_acamera_fpga_frame_reader_rbase_write_uv = acamera_fpga_frame_reader_uv_rbase_write;
        api_ds1.p_acamera_fpga_frame_reader_line_offset_write_uv = acamera_fpga_frame_reader_uv_line_offset_write;
        api_ds1.p_acamera_fpga_frame_reader_format_write_uv = acamera_fpga_frame_reader_uv_format_write;
        api_ds1.p_acamera_fpga_frame_reader_rbase_load_write_uv = acamera_fpga_frame_reader_uv_rbase_load_write;
#endif
        // settings
        dma_writer_get_settings( p_fsm->handle, dma_ds1, set_ds1 );
        set_ds1->p_ctx = ACAMERA_FSM2CTX_PTR( p_fsm ); //back reference
        set_ds1->height = crop_info.height_ds;
        set_ds1->width = crop_info.width_ds;

        set_ds1->callback = p_fsm->ds_buf.callback;
        set_ds1->isp_base = p_fsm->cmn.isp_base;
        set_ds1->ctx_id = ACAMERA_FSM2CTX_PTR( p_fsm )->context_id;

        set_ds1->active = ( p_fsm->dma_reader_out == dma_ds1 );
        set_ds1->vflip = p_fsm->vflip;

        result |= dma_writer_init( p_fsm->handle, dma_ds1, set_ds1, &api_ds1 );
        if ( result == edma_ok )
            dma_writer_set_initialized( p_fsm->handle, dma_ds1, 1 );
        else
            dma_writer_set_initialized( p_fsm->handle, dma_ds1, 0 );

		if(set_ds1)
		{
			vfree(set_ds1);
			set_ds1 = NULL;
		}
#endif
    }
}

void frame_buffer_prepare_metadata( dma_writer_fsm_ptr_t p_fsm )
{
    dma_pipe_settings *p_set = NULL;

#if ISP_HAS_CMOS_FSM
    int32_t frame = NUMBER_OF_USED_BANKS;
    exposure_set_t exp_set;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_FRAME_EXPOSURE_SET, &frame, sizeof( frame ), &exp_set, sizeof( exp_set ) );

#endif


    dma_writer_get_ptr_settings( p_fsm->handle, dma_fr, &p_set );

    if ( p_set ) {
        p_set->curr_metadata.format = acamera_isp_fr_dma_writer_format_read( p_fsm->cmn.isp_base );
        p_set->curr_metadata.width = p_set->width;
        p_set->curr_metadata.height = p_set->height;
        p_set->curr_metadata.line_size = acamera_line_offset( p_set->curr_metadata.width, _get_pixel_width( p_set->curr_metadata.format ) );
#if ISP_HAS_CMOS_FSM
        p_set->curr_metadata.exposure = exp_set.info.exposure_log2;
        p_set->curr_metadata.int_time = exp_set.data.integration_time;
        p_set->curr_metadata.int_time_medium = exp_set.data.integration_time_medium;
        p_set->curr_metadata.int_time_long = exp_set.data.integration_time_long;
        p_set->curr_metadata.again = exp_set.info.again_log2;
        p_set->curr_metadata.dgain = exp_set.info.dgain_log2;
        p_set->curr_metadata.isp_dgain = exp_set.info.isp_dgain_log2;
#endif
    }


#if ISP_HAS_DS1
    dma_writer_get_ptr_settings( p_fsm->handle, dma_ds1, &p_set );

    if ( p_set ) {
        p_set->curr_metadata.format = acamera_isp_ds1_dma_writer_format_read( p_fsm->cmn.isp_base );
        p_set->curr_metadata.width = p_set->width;
        p_set->curr_metadata.height = p_set->height;
        p_set->curr_metadata.line_size = acamera_line_offset( p_set->curr_metadata.width, _get_pixel_width( p_set->curr_metadata.format ) );
#if ISP_HAS_CMOS_FSM

        p_set->curr_metadata.exposure = exp_set.info.exposure_log2;
        p_set->curr_metadata.int_time = exp_set.data.integration_time;
        p_set->curr_metadata.int_time_medium = exp_set.data.integration_time_medium;
        p_set->curr_metadata.int_time_long = exp_set.data.integration_time_long;
        p_set->curr_metadata.again = exp_set.info.again_log2;
        p_set->curr_metadata.dgain = exp_set.info.dgain_log2;
        p_set->curr_metadata.isp_dgain = exp_set.info.isp_dgain_log2;
#endif
    }
#endif
}

void frame_buffer_fr_finished( dma_writer_fsm_ptr_t p_fsm )
{
    dma_pipe_settings *p_set = NULL;
    dma_writer_get_ptr_settings( p_fsm->handle, dma_fr, &p_set );
    if ( !p_set ) {
        LOG( LOG_CRIT, "dma_writer_get_ptr_settings failed." );
        return;
    }
    metadata_t *metadata_cb = dma_writer_return_metadata( p_fsm->handle, dma_fr );
    tframe_t *tframe = dma_writer_api_return_next_ready_frame( p_fsm->handle, dma_fr );

    if ( metadata_cb ) {
        metadata_cb->frame_id = ACAMERA_FSM2CTX_PTR( p_fsm )->isp_frame_counter;
    }


    if ( p_set->callback && tframe ) {
        p_set->callback( ACAMERA_FSM2CTX_PTR( p_fsm ), tframe, metadata_cb );
    }
    if ( tframe && tframe->primary.status == dma_buf_ready ) //3 up is used by the firmware
        tframe->primary.status = dma_buf_empty;              //clear the state - the frame will be reused
}

void frame_buffer_ds_finished( dma_writer_fsm_ptr_t p_fsm )
{
#if ISP_HAS_DS1
    dma_pipe_settings *p_set = NULL;
    dma_writer_get_ptr_settings( p_fsm->handle, dma_ds1, &p_set );
    metadata_t *metadata_cb = dma_writer_return_metadata( p_fsm->handle, dma_ds1 );
    tframe_t *tframe = dma_writer_api_return_next_ready_frame( p_fsm->handle, dma_ds1 );
    // scale DS DIS

    if ( !acamera_isp_top_bypass_ds1_scaler_read( p_fsm->cmn.isp_base ) ) {
        if ( ( acamera_isp_ds1_scaler_hfilt_tinc_read( p_fsm->cmn.isp_base ) != 0 ) && ( acamera_isp_ds1_scaler_vfilt_tinc_read( p_fsm->cmn.isp_base ) != 0 ) ) {
            metadata_t *metadata_fr = dma_writer_return_metadata( p_fsm->handle, dma_ds1 );
            if ( metadata_fr ) {
                metadata_cb->dis_offset_x = ( int8_t )( ( (int32_t)metadata_fr->dis_offset_x << 20 ) / (int32_t)acamera_isp_ds1_scaler_hfilt_tinc_read( p_fsm->cmn.isp_base ) ); // division by zero is checked
                metadata_cb->dis_offset_y = ( int8_t )( ( (int32_t)metadata_fr->dis_offset_y << 20 ) / (int32_t)acamera_isp_ds1_scaler_vfilt_tinc_read( p_fsm->cmn.isp_base ) ); // division by zero is checked
            }
        } else {
            LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO: acamera_isp_ds1_scaler_hfilt_tinc_read: %lu, acamera_isp_ds1_scaler_vfilt_tinc_read: %lu", acamera_isp_ds1_scaler_hfilt_tinc_read( p_fsm->cmn.isp_base ), acamera_isp_ds1_scaler_vfilt_tinc_read( p_fsm->cmn.isp_base ) );
        }
    }

    if ( metadata_cb ) {
        metadata_cb->frame_id = ACAMERA_FSM2CTX_PTR( p_fsm )->isp_frame_counter;
    }


    if ( p_set && p_set->callback && tframe ) {
        p_set->callback( ACAMERA_FSM2CTX_PTR( p_fsm ), tframe, metadata_cb );
    }
    if ( tframe && tframe->primary.status == dma_buf_ready ) //3 up is used by the firmware
        tframe->primary.status = dma_buf_empty;              //clear the state - the frame will be reused

#endif
}


void frame_buffer_update_callback( dma_writer_fsm_ptr_t p_fsm, dma_type dma_output, buffer_callback_t callback )
{
    if ( dma_output == dma_fr ) {
        p_fsm->fr_buf.callback = callback;
    }

#if ISP_HAS_DS1
    else if ( dma_output == dma_ds1 ) {
        p_fsm->ds_buf.callback = callback;
    }
#endif
    fsm_raise_event( p_fsm, event_id_frame_buf_reinit );
}

uint16_t frame_buffer_configure( dma_writer_fsm_ptr_t p_fsm, dma_type dma_output, tframe_t *frame_buf_array, uint32_t frame_buf_len )
{
    uint16_t frames_written = 0;
    // just save the parameters here
    if ( dma_output == dma_fr ) {
        p_fsm->fr_buf.frame_buf_array = frame_buf_array;
        p_fsm->fr_buf.frame_buf_len = frame_buf_len > MAX_DMA_QUEUE_FRAMES ? MAX_DMA_QUEUE_FRAMES : frame_buf_len;
        if ( p_fsm->fr_buf.frame_buf_len )
            frames_written = frame_buffer_reconfigure( p_fsm, dma_fr, p_fsm->fr_buf.frame_buf_array, p_fsm->fr_buf.frame_buf_len );
    }
#if ISP_HAS_DS1
    else if ( dma_output == dma_ds1 ) {
        p_fsm->ds_buf.frame_buf_array = frame_buf_array;
        p_fsm->ds_buf.frame_buf_len = frame_buf_len > MAX_DMA_QUEUE_FRAMES ? MAX_DMA_QUEUE_FRAMES : frame_buf_len;
        if ( p_fsm->ds_buf.frame_buf_len )
            frames_written = frame_buffer_reconfigure( p_fsm, dma_ds1, p_fsm->ds_buf.frame_buf_array, p_fsm->ds_buf.frame_buf_len );
    }
#endif
    else {
        LOG( LOG_ERR, "Trying to configure unsupported pipe" );
    }
    return frames_written;
}

uint16_t frame_buffer_reconfigure( dma_writer_fsm_ptr_t p_fsm, dma_type dma_output, tframe_t *frame_buf_array, uint16_t frame_buf_len )
{
    uint16_t buf_written = 0;
    if ( dma_output == dma_fr ) {
        if ( frame_buf_array && frame_buf_len ) { //sanity check
            buf_written = dma_writer_write_frame_queue( p_fsm->handle, dma_fr, frame_buf_array, frame_buf_len );
        }
    }
#if ISP_HAS_DS1
    else if ( dma_output == dma_ds1 ) {
        if ( frame_buf_array && frame_buf_len ) { //sanity check
            buf_written = dma_writer_write_frame_queue( p_fsm->handle, dma_ds1, frame_buf_array, frame_buf_len );
        }
    }
#endif
    return buf_written;
}

void frame_buffer_queue_reset(dma_writer_fsm_ptr_t p_fsm, dma_type type)
{
    if (p_fsm == NULL || p_fsm->handle == NULL) {
        LOG(LOG_ERR, "Error input param\n");
        return;
    }

    dma_writer_reset( p_fsm->handle, type);
}

void dma_writer_set_path_fps(dma_writer_fsm_ptr_t p_fsm, dma_type type,
                            uint32_t c_fps, uint32_t t_fps)
{
    if (p_fsm == NULL || p_fsm->handle == NULL) {
        LOG(LOG_ERR, "Error input param\n");
        return;
    }

    dma_writer_set_pipe_fps(p_fsm->handle, type, c_fps, t_fps);
}

void frame_buffer_get_next_empty_frame(dma_writer_fsm_ptr_t p_fsm, dma_type type)
{
    if (p_fsm == NULL || p_fsm->handle == NULL) {
        LOG(LOG_ERR, "Error input param\n");
        return;
    }

    dma_writer_pipe_get_next_empty_buffer( p_fsm->handle, type);
}

void dma_writer_update_address_interrupt( dma_writer_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;
    // send this interrupt to each pipe
    dma_writer_process_interrupt( p_fsm->handle, irq_event );
}

void dma_writer_deinit( dma_writer_fsm_ptr_t p_fsm )
{
    dma_writer_exit( p_fsm->handle );
}
