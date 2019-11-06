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

#include "acamera_fw.h"
#if ACAMERA_ISP_PROFILING
#include "acamera_profiler.h"
#endif

#include "acamera_isp_config.h"
#include "acamera_command_api.h"
#include "acamera_isp_core_nomem_settings.h"
#include "acamera_metering_stats_mem_config.h"
#include "system_timer.h"
#include "acamera_logger.h"
#include "acamera_sbus_api.h"
#include "sensor_init.h"
#include "isp_config_seq.h"
#include "system_am_sc.h"

#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

#if ISP_HAS_FPGA_WRAPPER
#include "acamera_fpga_config.h"
#endif

#if ISP_HAS_META_CB && defined( ISP_HAS_METADATA_FSM )
#include "metadata_api.h"
#endif


static const acam_reg_t **p_isp_data = SENSOR_ISP_SEQUENCE_DEFAULT;

extern void acamera_notify_evt_data_avail( void );

void acamera_load_isp_sequence( uintptr_t isp_base, const acam_reg_t **sequence, uint8_t num )
{
    acamera_sbus_t sbus;
    sbus.mask = SBUS_MASK_SAMPLE_32BITS | SBUS_MASK_SAMPLE_16BITS | SBUS_MASK_SAMPLE_8BITS | SBUS_MASK_ADDR_STEP_32BITS | SBUS_MASK_ADDR_32BITS;
    acamera_sbus_init( &sbus, sbus_isp );
    acamera_load_array_sequence( &sbus, isp_base, 0, sequence, num );
}


void acamera_load_sw_sequence( uintptr_t isp_base, const acam_reg_t **sequence, uint8_t num )
{
    acamera_sbus_t sbus;
    sbus.mask = SBUS_MASK_SAMPLE_32BITS | SBUS_MASK_SAMPLE_16BITS | SBUS_MASK_SAMPLE_8BITS | SBUS_MASK_ADDR_STEP_32BITS | SBUS_MASK_ADDR_32BITS;
    acamera_sbus_init( &sbus, sbus_isp_sw );
    acamera_load_array_sequence( &sbus, isp_base, 0, sequence, num );
}


#define IRQ_ID_UNDEFINED 0xFF


void acamera_fw_init( acamera_context_t *p_ctx )
{

#if ACAMERA_ISP_PROFILING
#if ACAMERA_ISP_PROFILING_INIT
    p_ctx->binit_profiler = 0;
    p_ctx->breport_profiler = 0;
#else
    p_ctx->binit_profiler = 0;
    p_ctx->breport_profiler = 0;
#endif
    p_ctx->start_profiling = 500; //start when gframe == 500
    p_ctx->stop_profiling = 1000; //stop  when gframe == 1000
#endif

    p_ctx->irq_flag = 1;

    p_ctx->fsm_mgr.p_ctx = p_ctx;
    p_ctx->fsm_mgr.ctx_id = p_ctx->context_id;
    p_ctx->fsm_mgr.isp_base = p_ctx->settings.isp_base;
    acamera_fsm_mgr_init( &p_ctx->fsm_mgr );

    p_ctx->irq_flag = 0;
    acamera_fw_interrupts_enable( p_ctx );
    p_ctx->system_state = FW_RUN;
}

void acamera_fw_deinit( acamera_context_t *p_ctx )
{
    p_ctx->fsm_mgr.p_ctx = p_ctx;
    acamera_fsm_mgr_deinit( &p_ctx->fsm_mgr );
}

void acamera_fw_error_routine( acamera_context_t *p_ctx, uint32_t irq_mask )
{
    //masked all interrupts
    acamera_isp_isp_global_interrupt_mask_vector_write( 0, ISP_IRQ_DISABLE_ALL_IRQ );
    //safe stop
    acamera_isp_input_port_mode_request_write( p_ctx->settings.isp_base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_STOP );

    // check whether the HW is stopped or not.
    uint32_t count = 0;
    while ( acamera_isp_input_port_mode_status_read( p_ctx->settings.isp_base ) != ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_STOP || acamera_isp_isp_global_monitor_fr_pipeline_busy_read( p_ctx->settings.isp_base ) ) {
        //cannot sleep use this delay
        do {
            count++;
        } while ( count % 32 != 0 );

        if ( ( count >> 5 ) > 50 ) {
            LOG( LOG_CRIT, "stopping isp failed, timeout: %u.", (unsigned int)count * 1000 );
            break;
        }
    }

    acamera_isp_isp_global_global_fsm_reset_write( p_ctx->settings.isp_base, 1 );
    acamera_isp_isp_global_global_fsm_reset_write( p_ctx->settings.isp_base, 0 );

    //return the interrupts
    acamera_isp_isp_global_interrupt_mask_vector_write( 0, ISP_IRQ_MASK_VECTOR );

    acamera_isp_input_port_mode_request_write( p_ctx->settings.isp_base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START );

    LOG( LOG_CRIT, "starting isp from error" );
}


void acamera_fw_process( acamera_context_t *p_ctx )
{
#if ACAMERA_ISP_PROFILING
    if ( ( p_ctx->frame >= p_ctx->start_profiling ) && ( !p_ctx->binit_profiler ) ) {
        acamera_profiler_init();
        p_ctx->binit_profiler = 1;
    }
#endif
    if ( ( p_ctx->system_state == FW_RUN ) ) //need to capture on firmware freeze
    {
        // firmware not frozen
        // 0 means handle all the events and then return.
        acamera_fsm_mgr_process_events( &p_ctx->fsm_mgr, 0 );
    }
#if ACAMERA_ISP_PROFILING
    if ( ( p_ctx->frame >= p_ctx->stop_profiling ) && ( !p_ctx->breport_profiler ) ) {
        acamera_profiler_report();
        p_ctx->breport_profiler = 1;
    }
#endif
}

void acamera_fw_raise_event( acamera_context_t *p_ctx, event_id_t event_id )
{ //dma writer events should be passed for the capture on freeze requirement
    if ( p_ctx->stab.global_freeze_firmware == 0 || event_id == event_id_new_frame || event_id == event_id_drop_frame
#if defined( ISP_HAS_DMA_WRITER_FSM )
         || event_id == event_id_frame_buffer_fr_ready || event_id == event_id_frame_buffer_ds_ready || event_id == event_id_frame_buffer_metadata
#endif
#if defined( ISP_HAS_METADATA_FSM )
         || event_id == event_id_metadata_ready || event_id == event_id_metadata_update
#endif
#if defined( ISP_HAS_BSP_TEST_FSM )
         || event_id == event_id_bsp_test_interrupt_finished
#endif
         ) {
        acamera_event_queue_push( &p_ctx->fsm_mgr.event_queue, (int)( event_id ) );

        acamera_notify_evt_data_avail();
    }
}

void acamera_fsm_mgr_raise_event( acamera_fsm_mgr_t *p_fsm_mgr, event_id_t event_id )
{ //dma writer events should be passed for the capture on freeze requirement
    if ( p_fsm_mgr->p_ctx->stab.global_freeze_firmware == 0 || event_id == event_id_new_frame || event_id == event_id_drop_frame
#if defined( ISP_HAS_DMA_WRITER_FSM )
         || event_id == event_id_frame_buffer_fr_ready || event_id == event_id_frame_buffer_ds_ready || event_id == event_id_frame_buffer_metadata
#endif
#if defined( ISP_HAS_BSP_TEST_FSM )
         || event_id == event_id_bsp_test_interrupt_finished
#endif
         ) {
        acamera_event_queue_push( &( p_fsm_mgr->event_queue ), (int)( event_id ) );

        acamera_notify_evt_data_avail();
    }
}


int32_t acamera_update_calibration_set( acamera_context_ptr_t p_ctx )
{
    int32_t result = 0;
    void *sensor_arg = 0;
    if ( p_ctx->settings.get_calibrations != NULL ) {
        {
            const sensor_param_t *param = NULL;
            acamera_fsm_mgr_get_param( &p_ctx->fsm_mgr, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );

            uint32_t cur_mode = param->mode;
            if ( cur_mode < param->modes_num ) {
                sensor_arg = &( param->modes_table[cur_mode] );
            }
        }
        if ( p_ctx->settings.get_calibrations( p_ctx->context_id, sensor_arg, &p_ctx->acameraCalibrations ) != 0 ) {
            LOG( LOG_CRIT, "Failed to get calibration set for. Fatal error" );
        }

#if defined( ISP_HAS_GENERAL_FSM )
        acamera_fsm_mgr_set_param( &p_ctx->fsm_mgr, FSM_PARAM_SET_RELOAD_CALIBRATION, NULL, 0 );
#endif

// Update some FSMs variables which depends on calibration data.
#if defined( ISP_HAS_AE_BALANCED_FSM ) || defined( ISP_HAS_AE_MANUAL_FSM )
        acamera_fsm_mgr_set_param( &p_ctx->fsm_mgr, FSM_PARAM_SET_AE_INIT, NULL, 0 );
#endif

#if defined( ISP_HAS_IRIDIX_FSM ) || defined( ISP_HAS_IRIDIX_HIST_FSM ) || defined( ISP_HAS_IRIDIX_MANUAL_FSM )
        acamera_fsm_mgr_set_param( &p_ctx->fsm_mgr, FSM_PARAM_SET_IRIDIX_INIT, NULL, 0 );
#endif

#if defined( ISP_HAS_COLOR_MATRIX_FSM )
        acamera_fsm_mgr_set_param( &p_ctx->fsm_mgr, FSM_PARAM_SET_CCM_CHANGE, NULL, 0 );
#endif

#if defined( ISP_HAS_SBUF_FSM )
        acamera_fsm_mgr_set_param( &p_ctx->fsm_mgr, FSM_PARAM_SET_SBUF_CALIBRATION_UPDATE, NULL, 0 );
#endif
    } else {
        LOG( LOG_CRIT, "Calibration callback is null. Failed to get calibrations" );
        result = -1;
    }

    return result;
}


int32_t acamera_init_calibrations( acamera_context_ptr_t p_ctx )
{
    int32_t result = 0;
    void *sensor_arg = 0;
#ifdef SENSOR_ISP_SEQUENCE_DEFAULT_FULL
    acamera_load_isp_sequence( p_ctx->settings.isp_base, p_ctx->isp_sequence, SENSOR_ISP_SEQUENCE_DEFAULT_FULL );
#endif

    // if "p_ctx->initialized" is 1, that means we are changing the preset and wdr_mode,
    // we need to update the calibration data and update some FSM variables which
    // depends on calibration data.
    if ( p_ctx->initialized == 1 ) {
        acamera_update_calibration_set( p_ctx );
    } else {
        if ( p_ctx->settings.get_calibrations != NULL ) {
            const sensor_param_t *param = NULL;
            acamera_fsm_mgr_get_param( &p_ctx->fsm_mgr, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );

            uint32_t cur_mode = param->mode;
            if ( cur_mode < param->modes_num ) {
                sensor_arg = &( param->modes_table[cur_mode] );
            }

            if ( p_ctx->settings.get_calibrations( p_ctx->context_id, sensor_arg, &p_ctx->acameraCalibrations ) != 0 ) {
                LOG( LOG_CRIT, "Failed to get calibration set for. Fatal error" );
            }
        } else {
            LOG( LOG_CRIT, "Calibration callback is null. Failed to get calibrations" );
            result = -1;
        }
    }
    return result;
}

int32_t acamera_init_context_seq( acamera_context_t *p_ctx )
{
    int32_t result = 0;

    // if "p_ctx->initialized" is 1, that means we are changing the preset and wdr_mode,
    // we need to update the calibration data and update some FSM variables which
    // depends on calibration data.
    const sensor_param_t *param = NULL;
    result = acamera_fsm_mgr_get_param( &p_ctx->fsm_mgr, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );
    if (result != 0) {
        LOG(LOG_ERR, "WARNING:get isp context seq failed.\n");
        return 0;
    }
    p_ctx->isp_context_seq.sequence = param->isp_context_seq.sequence;
    p_ctx->isp_context_seq.seq_num = param->isp_context_seq.seq_num;
    LOG(LOG_ERR, "load isp context sequence[%d]\n", param->isp_context_seq.seq_num);

    acamera_load_sw_sequence( p_ctx->settings.isp_base, p_ctx->isp_context_seq.sequence, p_ctx->isp_context_seq.seq_num );
    return result;
}


#if ISP_HAS_META_CB && defined( ISP_HAS_METADATA_FSM )
static void internal_callback_metadata( void *ctx, const firmware_metadata_t *fw_metadata )
{
    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)ctx;

    if ( p_ctx->settings.callback_meta != NULL ) {
        p_ctx->settings.callback_meta( p_ctx->context_id, fw_metadata );
    }
}
#endif

#if ISP_HAS_RAW_CB && ISP_DMA_RAW_CAPTURE
//static void internal_callback_raw( void* ctx, tframe_t *tframe, const metadata_t *metadata )
static void internal_callback_raw( void *ctx, aframe_t *aframe, const metadata_t *metadata, uint8_t exposures_num )
{
    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)ctx;

    if ( p_ctx->settings.callback_raw != NULL ) {
        p_ctx->settings.callback_raw( p_ctx->context_id, aframe, metadata, exposures_num );
    }
}
#endif

#if defined( ISP_HAS_DMA_WRITER_FSM )
static void internal_callback_fr( void *ctx, tframe_t *tframe, const metadata_t *metadata )
{
    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)ctx;

    if ( p_ctx->settings.callback_fr != NULL ) {
        p_ctx->settings.callback_fr( p_ctx->context_id, tframe, metadata );
    }
}
#endif

#if ISP_HAS_DS1 && defined( ISP_HAS_DMA_WRITER_FSM )
// Callback from DS1 output pipe
static void internal_callback_ds1( void *ctx, tframe_t *tframe, const metadata_t *metadata )
{

    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)ctx;
    if ( p_ctx->settings.callback_ds1 != NULL ) {
        p_ctx->settings.callback_ds1( p_ctx->context_id, tframe, metadata );
    }
}
#endif

#if ISP_HAS_DS2
// Callback from DS2 output pipe
static void external_callback_ds2( void *ctx, tframe_t *tframe, const metadata_t *metadata )
{

    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)ctx;
    if ( p_ctx->settings.callback_ds2 != NULL ) {
        p_ctx->settings.callback_ds2( p_ctx->context_id, tframe, metadata );
    }
}
#endif

static void configure_all_frame_buffers( acamera_context_ptr_t p_ctx )
{

#if ISP_HAS_WDR_FRAME_BUFFER
    acamera_isp_frame_stitch_frame_buffer_frame_write_on_write( p_ctx->settings.isp_base, 0 );
    aframe_t *frame_stitch_frames = p_ctx->settings.fs_frames;
    uint32_t frame_stitch_frames_num = p_ctx->settings.fs_frames_number;
    if ( frame_stitch_frames != NULL && frame_stitch_frames_num != 0 ) {
        if ( frame_stitch_frames_num == 1 ) {
            LOG( LOG_INFO, "Only one output buffer will be used for frame_stitch." );
            acamera_isp_frame_stitch_frame_buffer_bank0_base_write( p_ctx->settings.isp_base, frame_stitch_frames[0].address );
            acamera_isp_frame_stitch_frame_buffer_bank1_base_write( p_ctx->settings.isp_base, frame_stitch_frames[0].address );
            acamera_isp_frame_stitch_frame_buffer_line_offset_write( p_ctx->settings.isp_base, frame_stitch_frames[0].line_offset );
        } else {
            // double buffering is enabled
            acamera_isp_frame_stitch_frame_buffer_bank0_base_write( p_ctx->settings.isp_base, frame_stitch_frames[0].address );
            acamera_isp_frame_stitch_frame_buffer_bank1_base_write( p_ctx->settings.isp_base, frame_stitch_frames[1].address );
            acamera_isp_frame_stitch_frame_buffer_line_offset_write( p_ctx->settings.isp_base, frame_stitch_frames[0].line_offset );
        }

        acamera_isp_frame_stitch_frame_buffer_frame_write_on_write( p_ctx->settings.isp_base, 1 );
        acamera_isp_frame_stitch_frame_buffer_axi_port_enable_write( p_ctx->settings.isp_base, 1 );

    } else {
        acamera_isp_frame_stitch_frame_buffer_frame_write_on_write( p_ctx->settings.isp_base, 0 );
        acamera_isp_frame_stitch_frame_buffer_axi_port_enable_write( p_ctx->settings.isp_base, 0 );
        LOG( LOG_ERR, "No output buffers for frame_stitch block provided in settings. frame_stitch wdr buffer is disabled" );
    }
#endif


#if ISP_HAS_META_CB && defined( ISP_HAS_METADATA_FSM )
    acamera_fsm_mgr_set_param( &p_ctx->fsm_mgr, FSM_PARAM_SET_META_REGISTER_CB, internal_callback_metadata, sizeof( metadata_callback_t ) );
#endif


#if ISP_HAS_RAW_CB && ISP_DMA_RAW_CAPTURE
    dma_raw_capture_regist_callback( p_ctx->p_gfw, internal_callback_raw );
#endif

#if defined( ISP_HAS_DMA_WRITER_FSM )

    fsm_param_dma_pipe_setting_t pipe_fr;

    pipe_fr.pipe_id = dma_fr;
    pipe_fr.buf_array = p_ctx->settings.fr_frames;
    pipe_fr.buf_len = p_ctx->settings.fr_frames_number;
    pipe_fr.callback = internal_callback_fr;
    acamera_fsm_mgr_set_param( &p_ctx->fsm_mgr, FSM_PARAM_SET_DMA_PIPE_SETTING, &pipe_fr, sizeof( pipe_fr ) );

    acamera_isp_fr_dma_writer_format_write( p_ctx->settings.isp_base, FW_OUTPUT_FORMAT );
    acamera_isp_fr_uv_dma_writer_format_write( p_ctx->settings.isp_base, FW_OUTPUT_FORMAT_SECONDARY );
#endif


#if ISP_HAS_DS1 && defined( ISP_HAS_DMA_WRITER_FSM )

    fsm_param_dma_pipe_setting_t pipe_ds1;

    pipe_ds1.pipe_id = dma_ds1;
    pipe_ds1.buf_array = p_ctx->settings.ds1_frames;
    pipe_ds1.buf_len = p_ctx->settings.ds1_frames_number;
    pipe_ds1.callback = internal_callback_ds1;
    acamera_fsm_mgr_set_param( &p_ctx->fsm_mgr, FSM_PARAM_SET_DMA_PIPE_SETTING, &pipe_ds1, sizeof( pipe_ds1 ) );

    acamera_isp_ds1_dma_writer_format_write( p_ctx->settings.isp_base, FW_OUTPUT_FORMAT );
    acamera_isp_ds1_uv_dma_writer_format_write( p_ctx->settings.isp_base, FW_OUTPUT_FORMAT_SECONDARY );

#endif

#if ISP_HAS_DS2
    am_sc_set_callback(p_ctx, external_callback_ds2);
#endif

}

void acamera_fw_get_sensor_name(uint32_t *sname)
{
    acamera_command(TSENSOR, SENSOR_NAME, 0, COMMAND_GET, sname);
    if (sname == NULL) {
        LOG(LOG_ERR, "Error input param\n");
    }
}

static void init_stab( acamera_context_ptr_t p_ctx )
{
    p_ctx->stab.global_freeze_firmware = 0;
    p_ctx->stab.global_manual_exposure = 0;

    p_ctx->stab.global_manual_iridix = 0;
    p_ctx->stab.global_manual_sinter = 0;
    p_ctx->stab.global_manual_temper = 0;
    p_ctx->stab.global_manual_awb = 0;
    p_ctx->stab.global_manual_saturation = 0;
    p_ctx->stab.global_manual_auto_level = 0;
    p_ctx->stab.global_manual_frame_stitch = 0;
    p_ctx->stab.global_manual_raw_frontend = 0;
    p_ctx->stab.global_manual_black_level = 0;
    p_ctx->stab.global_manual_shading = 0;
    p_ctx->stab.global_manual_demosaic = 0;
    p_ctx->stab.global_manual_cnr = 0;
    p_ctx->stab.global_manual_sharpen = 0;

    p_ctx->stab.global_exposure = 0;
    p_ctx->stab.global_long_integration_time = 0;
    p_ctx->stab.global_short_integration_time = 0;
    p_ctx->stab.global_manual_exposure_ratio = SYSTEM_MANUAL_EXPOSURE_RATIO_DEFAULT;
    p_ctx->stab.global_exposure_ratio = SYSTEM_EXPOSURE_RATIO_DEFAULT;

    p_ctx->stab.global_maximum_iridix_strength = SYSTEM_MAXIMUM_IRIDIX_STRENGTH_DEFAULT;
    p_ctx->stab.global_minimum_iridix_strength = SYSTEM_MINIMUM_IRIDIX_STRENGTH_DEFAULT;
    p_ctx->stab.global_iridix_strength_target = 0;
    p_ctx->stab.global_sinter_threshold_target = 0;
    p_ctx->stab.global_temper_threshold_target = 0;
    p_ctx->stab.global_awb_red_gain = 256;
    p_ctx->stab.global_awb_blue_gain = 256;
    p_ctx->stab.global_saturation_target = 0;
    p_ctx->stab.global_ae_compensation = SYSTEM_AE_COMPENSATION_DEFAULT;
    p_ctx->stab.global_calibrate_bad_pixels = 0;
}


extern void *get_system_ctx_ptr( void );

#if USER_MODULE

int32_t acamera_init_context( acamera_context_t *p_ctx, acamera_settings *settings, acamera_firmware_t *g_fw )
{
    int32_t result = 0;
    // keep the context pointer for debug purposes
    p_ctx->context_ref = (uint32_t *)p_ctx;
    p_ctx->p_gfw = g_fw;

    // copy settings
    system_memcpy( (void *)&p_ctx->settings, (void *)settings, sizeof( acamera_settings ) );

    // each context is initialized to the default state
    p_ctx->isp_sequence = p_isp_data;

    // reset frame counters
    p_ctx->isp_frame_counter_raw = 0;
    p_ctx->isp_frame_counter = 0;

    acamera_fw_init( p_ctx );

    init_stab( p_ctx );

    p_ctx->initialized = 1;

    return result;
}

#else


int32_t acamera_init_context( acamera_context_t *p_ctx, acamera_settings *settings, acamera_firmware_t *g_fw )
{
    int32_t result = 0;
    // keep the context pointer for debug purposes
    p_ctx->context_ref = (uint32_t *)p_ctx;
    p_ctx->p_gfw = g_fw;
    if ( p_ctx->sw_reg_map.isp_sw_config_map != NULL ) {

        LOG( LOG_INFO, "Allocated memory for config space of size %d bytes", ACAMERA_ISP1_SIZE );
        LOG( LOG_INFO, "Allocated memory for metering of size %d bytes", ACAMERA_METERING_STATS_MEM_SIZE );
        // copy settings
        system_memcpy( (void *)&p_ctx->settings, (void *)settings, sizeof( acamera_settings ) );


        p_ctx->settings.isp_base = (uintptr_t)p_ctx->sw_reg_map.isp_sw_config_map;

        // each context is initialized to the default state
        p_ctx->isp_sequence = p_isp_data;

        acamera_load_isp_sequence( 0, p_ctx->isp_sequence, SENSOR_ISP_SEQUENCE_DEFAULT_SETTINGS );

#if defined( SENSOR_ISP_SEQUENCE_DEFAULT_SETTINGS_FPGA ) && ISP_HAS_FPGA_WRAPPER
        // these settings are loaded only for ARM FPGA demo platform and must be ignored on other systems
        acamera_load_isp_sequence( 0, p_ctx->isp_sequence, SENSOR_ISP_SEQUENCE_DEFAULT_SETTINGS_FPGA );
#endif

#if ISP_DMA_RAW_CAPTURE
        dma_raw_capture_init( g_fw );
#endif

        // reset frame counters
        p_ctx->isp_frame_counter_raw = 0;
        p_ctx->isp_frame_counter = 0;

        acamera_fw_init( p_ctx );

        acamera_init_context_seq(p_ctx);

        configure_all_frame_buffers( p_ctx );

        init_stab( p_ctx );

        // the custom initialization may be required for a context
        if ( p_ctx->settings.custom_initialization != NULL ) {
            p_ctx->settings.custom_initialization( p_ctx->context_id );
        }

        acamera_isp_input_port_mode_request_write( p_ctx->settings.isp_base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START );

        p_ctx->initialized = 1;

    } else {
        result = -1;
        LOG( LOG_CRIT, "Failed to allocate memory for ISP config context" );
    }


    return result;
}
#endif

void acamera_deinit_context( acamera_context_t *p_ctx )
{
    acamera_fw_deinit( p_ctx );
}

void acamera_general_interrupt_hanlder( acamera_context_ptr_t p_ctx, uint8_t event )
{
#ifdef CALIBRATION_INTERRUPTS
    uint32_t *interrupt_counter = _GET_UINT_PTR( p_ctx, CALIBRATION_INTERRUPTS );
    interrupt_counter[event]++;
#endif


    p_ctx->irq_flag++;

    if ( event == ACAMERA_IRQ_FRAME_START ) {
        p_ctx->frame++;
    }

    if ( event == ACAMERA_IRQ_FRAME_END ) {
        // Update frame counter
        p_ctx->isp_frame_counter++;
        LOG( LOG_DEBUG, "Meta frame counter = %d", (int)p_ctx->isp_frame_counter );

#if ISP_DMA_RAW_CAPTURE
        p_ctx->isp_frame_counter_raw++;
#endif

// check frame counter sync when there is raw callback
#if ISP_HAS_RAW_CB
        if ( p_ctx->isp_frame_counter_raw != p_ctx->isp_frame_counter ) {
            LOG( LOG_DEBUG, "Sync frame counter : raw = %d, meta = %d",
                 (int)p_ctx->isp_frame_counter_raw, (int)p_ctx->isp_frame_counter );
            p_ctx->isp_frame_counter = p_ctx->isp_frame_counter_raw;
        }
#endif

        acamera_fw_raise_event( p_ctx, event_id_frame_end );

#if defined( ACAMERA_ISP_PROFILING ) && ( ACAMERA_ISP_PROFILING == 1 )
        acamera_profiler_new_frame();
#endif
    }

    if ( ( p_ctx->stab.global_freeze_firmware == 0 )
         || (event == ACAMERA_IRQ_FRAME_DROP_FR) || (event == ACAMERA_IRQ_FRAME_DROP_DS)
#if defined( ISP_HAS_DMA_WRITER_FSM )
         || ( event == ACAMERA_IRQ_FRAME_WRITER_FR ) // process interrupts for frame buffer anyway (otherwise picture will be frozen)
         || ( event == ACAMERA_IRQ_FRAME_WRITER_DS ) // process interrupts for frame buffer anyway (otherwise picture will be frozen)
#endif
#if defined( ISP_HAS_CMOS_FSM )
         || ( event == ACAMERA_IRQ_FRAME_START ) || ( event == ACAMERA_IRQ_FPGA_FRAME_END ) // process interrupts for FS anyway (otherwise exposure will be only short)
#endif
#if defined( ISP_HAS_BSP_TEST_FSM )
         || event == ACAMERA_IRQ_FRAME_END || event == ACAMERA_IRQ_FRAME_START
#endif
         ) {
        // firmware not frozen
        acamera_fsm_mgr_process_interrupt( &p_ctx->fsm_mgr, event );
    }

    p_ctx->irq_flag--;
}
