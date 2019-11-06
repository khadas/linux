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
#include "acamera_firmware_api.h"
#include "acamera_firmware_config.h"
#include "acamera_calibrations.h"
#include "acamera.h"
#include "acamera_math.h"
#include "acamera_isp_config.h"
#include "acamera_isp_core_nomem_settings.h"
#include "system_stdlib.h"
#include "system_dma.h"
#include "acamera_metering_stats_mem_config.h"
#include "acamera_aexp_hist_stats_mem_config.h"
#include "acamera_decompander0_mem_config.h"
#include "acamera_ihist_stats_mem_config.h"


#if FW_HAS_CONTROL_CHANNEL
#include "acamera_ctrl_channel.h"
#endif

#if ISP_DMA_RAW_CAPTURE
#include "dma_raw_capture.h"
#endif

#if ACAMERA_ISP_PROFILING
#include "acamera_profiler.h"
#endif

#if ACAMERA_ISP_PROFILING
#ifndef CHECK_STACK_SIZE_DWORDS
#define CHECK_STACK_SIZE_DWORDS 1024
#endif
#define CHECK_STACK_SIZE_START 512
#endif

#include "acamera_logger.h"
#include "fsm_param.h"

static acamera_firmware_t g_firmware;
static volatile uint32_t context_counter = 0;


int32_t acamera_set_api_context( uint32_t ctx_num )
{
    int32_t result = 0;
    if ( ctx_num < g_firmware.context_number ) {
        g_firmware.api_context = ctx_num;
        LOG( LOG_INFO, "new api context: %u.", (unsigned int)g_firmware.api_context );
        result = 0;
    } else {
        result = -1;
    }
    return result;
}

int32_t acamera_get_api_context( void )
{
    int32_t result = g_firmware.api_context;
    return result;
}

int32_t acamera_get_context_number( void )
{
    int32_t result = g_firmware.context_number;
    return result;
}


void *acamera_get_api_ctx_ptr( void )
{
    return &( g_firmware.fw_ctx[acamera_get_api_context()] );
}

uintptr_t acamera_get_isp_sw_setting_base( void )
{
    acamera_context_t *p_ctx = (acamera_context_t *)(&g_firmware.fw_ctx[0]);
    if (p_ctx->settings.isp_base)
        return p_ctx->settings.isp_base;
    else
        return 0;
}

void acamera_notify_evt_data_avail( void )
{
    system_semaphore_raise( g_firmware.sem_evt_avail );
}


static int32_t validate_settings( acamera_settings *settings, uint32_t ctx_num )
{

    int32_t result = 0;
    int32_t idx = 0;

    if ( settings != NULL ) {
        // validate parameters for all input contexts
        for ( idx = 0; idx < ctx_num; idx++ ) {
            acamera_settings *ctx_set = &settings[idx];
            if ( ctx_set->sensor_init == NULL ||
                 ctx_set->sensor_deinit == NULL ||
                 ctx_set->get_calibrations == NULL ) {
                result = -1;
                LOG( LOG_CRIT, "One of the mandatory input parameters in the init settings is null for the context %d", idx );
            }
#if ISP_HAS_DS1 && defined( ISP_HAS_DMA_WRITER_FSM )
            if ( ctx_set->ds1_frames && ctx_set->ds1_frames_number ) {                              //check if DS frames are initialized
                if ( acamera_isp_isp_global_parameter_status_ds1_pipe_read( ctx_set->isp_base ) ) { //check if DS is chickened out
                    result = -1;
                    LOG( LOG_CRIT, "DS Frames are expected while DS block is not present in hardware for the context %d", idx );
                }
            }
#endif
        }
    } else {
        result = -1;
    }
    return result;
}


#define ACAMERA_CONTEXT_SIZE ACAMERA_ISP1_BASE_ADDR + ACAMERA_ISP1_SIZE

static int32_t dma_channel_addresses_setup( void *isp_chan, void *metering_chan, void *sw_context_map, uint32_t idx )
{
    int32_t result = 0;
    //PING ISP
    {
        dma_addr_pair_t isp_dma_addr_pair[2] = {
            {ISP_SOC_START_ADDR + ACAMERA_DECOMPANDER0_MEM_BASE_ADDR, ACAMERA_ISP1_BASE_ADDR - ACAMERA_DECOMPANDER0_MEM_BASE_ADDR},
            {ISP_SOC_START_ADDR + ACAMERA_ISP1_BASE_ADDR, ACAMERA_ISP1_SIZE}};
        if ( system_dma_sg_device_setup( isp_chan, ISP_CONFIG_PING, isp_dma_addr_pair, 2 ) ) {
            LOG( LOG_CRIT, "ISP device channel address setup for PING ctx:%d failed", idx );
            result = -1;
        }
        fwmem_addr_pair_t fwmem_add_pair[2] = {
            {(void *)( (uintptr_t)sw_context_map + ACAMERA_DECOMPANDER0_MEM_BASE_ADDR ), ACAMERA_ISP1_BASE_ADDR - ACAMERA_DECOMPANDER0_MEM_BASE_ADDR},
            {(void *)( (uintptr_t)sw_context_map + ACAMERA_ISP1_BASE_ADDR ), ACAMERA_ISP1_SIZE}};
        if ( system_dma_sg_fwmem_setup( isp_chan, ISP_CONFIG_PING, fwmem_add_pair, 2 ) ) {
            LOG( LOG_CRIT, "ISP memory channel address setup for PING ctx:%d failed", idx );
            result = -1;
        }
    }
    //PONG ISP
    {
        dma_addr_pair_t isp_dma_addr_pair[2] = {
            {ISP_SOC_START_ADDR + ACAMERA_DECOMPANDER0_MEM_BASE_ADDR + ISP_CONFIG_PING_SIZE, ACAMERA_ISP1_BASE_ADDR - ACAMERA_DECOMPANDER0_MEM_BASE_ADDR},
            {ISP_SOC_START_ADDR + ACAMERA_ISP1_BASE_ADDR + ISP_CONFIG_PING_SIZE, ACAMERA_ISP1_SIZE}};
        if ( system_dma_sg_device_setup( isp_chan, ISP_CONFIG_PONG, isp_dma_addr_pair, 2 ) ) {
            LOG( LOG_CRIT, "ISP device channel address setup for PONG ctx:%d failed", idx );
            result = -1;
        }

        fwmem_addr_pair_t fwmem_add_pair[2] = {
            {(void *)( (uintptr_t)sw_context_map + ACAMERA_DECOMPANDER0_MEM_BASE_ADDR ), ACAMERA_ISP1_BASE_ADDR - ACAMERA_DECOMPANDER0_MEM_BASE_ADDR},
            {(void *)( (uintptr_t)sw_context_map + ACAMERA_ISP1_BASE_ADDR ), ACAMERA_ISP1_SIZE}};
        if ( system_dma_sg_fwmem_setup( isp_chan, ISP_CONFIG_PONG, fwmem_add_pair, 2 ) ) {
            LOG( LOG_CRIT, "ISP memory channel address setup for PONG ctx:%d failed", idx );
            result = -1;
        }
    }
    //PING METERING
    {
        dma_addr_pair_t isp_dma_addr_pair[2] = {
            {ISP_SOC_START_ADDR + ACAMERA_AEXP_HIST_STATS_MEM_BASE_ADDR, ACAMERA_AEXP_HIST_STATS_MEM_SIZE + ACAMERA_IHIST_STATS_MEM_SIZE},
            {ISP_SOC_START_ADDR + ACAMERA_METERING_STATS_MEM_BASE_ADDR, ACAMERA_METERING_STATS_MEM_SIZE}};
        if ( system_dma_sg_device_setup( metering_chan, ISP_CONFIG_PING, isp_dma_addr_pair, 2 ) ) {
            LOG( LOG_CRIT, "Metering device channel address setup for PING ctx:%d failed", idx );
            result = -1;
        }

        fwmem_addr_pair_t fwmem_add_pair[2] = {
            {(void *)( (uintptr_t)sw_context_map + ACAMERA_AEXP_HIST_STATS_MEM_BASE_ADDR ), ACAMERA_AEXP_HIST_STATS_MEM_SIZE + ACAMERA_IHIST_STATS_MEM_SIZE},
            {(void *)( (uintptr_t)sw_context_map + ACAMERA_METERING_STATS_MEM_BASE_ADDR ), ACAMERA_METERING_STATS_MEM_SIZE}};
        if ( system_dma_sg_fwmem_setup( metering_chan, ISP_CONFIG_PING, fwmem_add_pair, 2 ) ) {
            LOG( LOG_CRIT, "Metering memory channel address setup for PING ctx:%d failed", idx );
            result = -1;
        }
    }
    //PONG METERING
    {
        dma_addr_pair_t isp_dma_addr_pair[2] = {
            {ISP_SOC_START_ADDR + ACAMERA_AEXP_HIST_STATS_MEM_BASE_ADDR, ACAMERA_AEXP_HIST_STATS_MEM_SIZE + ACAMERA_IHIST_STATS_MEM_SIZE},
            {ISP_SOC_START_ADDR + ACAMERA_METERING_STATS_MEM_BASE_ADDR + ISP_CONFIG_PING_SIZE, ACAMERA_METERING_STATS_MEM_SIZE}};
        if ( system_dma_sg_device_setup( metering_chan, ISP_CONFIG_PONG, isp_dma_addr_pair, 2 ) ) {
            LOG( LOG_CRIT, "Metering device channel address setup for PONG ctx:%d failed", idx );
            result = -1;
        }

        fwmem_addr_pair_t fwmem_add_pair[2] = {
            {(void *)( (uintptr_t)sw_context_map + ACAMERA_AEXP_HIST_STATS_MEM_BASE_ADDR ), ACAMERA_AEXP_HIST_STATS_MEM_SIZE + ACAMERA_IHIST_STATS_MEM_SIZE},
            {(void *)( (uintptr_t)sw_context_map + ACAMERA_METERING_STATS_MEM_BASE_ADDR ), ACAMERA_METERING_STATS_MEM_SIZE}};
        if ( system_dma_sg_fwmem_setup( metering_chan, ISP_CONFIG_PONG, fwmem_add_pair, 2 ) ) {
            LOG( LOG_CRIT, "Metering memory channel address setup for PONG  ctx:%d failed", idx );
            result = -1;
        }
    }
    return result;
}

#if USER_MODULE

int32_t acamera_init( acamera_settings *settings, uint32_t ctx_num )
{
    int32_t result = 0;

    result = validate_settings( settings, ctx_num );

    if ( result == 0 ) {

#if FW_HAS_CONTROL_CHANNEL
        ctrl_channel_init();
#endif

        g_firmware.api_context = 0;

        system_semaphore_init( &g_firmware.sem_evt_avail );

        if ( ctx_num <= FIRMWARE_CONTEXT_NUMBER ) {
            uint32_t idx = 0;

            g_firmware.context_number = ctx_num;

            for ( idx = 0; idx < ctx_num; idx++ ) {
                acamera_context_t *p_ctx = (acamera_context_ptr_t)&g_firmware.fw_ctx[idx];
                LOG( LOG_INFO, "Initialize context %d, context size is %d bytes", idx, sizeof( struct _acamera_context_t ) );
                system_memset( (void *)p_ctx, 0x0, sizeof( struct _acamera_context_t ) );
                // each context has unique id
                p_ctx->context_id = idx;

                // init context
                result = acamera_init_context( p_ctx, &settings[idx], &g_firmware );
                if ( result != 0 ) {
                    LOG( LOG_CRIT, "Failed to initialized the context %d", (int)idx );
                    break;
                }
            }

            if ( result == 0 ) {
                g_firmware.initialized = 1;
            } else {
                g_firmware.initialized = 0;
            }

        } else {
            result = -1;
            LOG( LOG_CRIT, "Failed to initialized the firmware context. Not enough memory. " );
        }

    } else {
        LOG( LOG_CRIT, "Input initializations settings are not correct" );
        result = -1;
    }

    return result;
}

void acamera_update_cur_settings_to_isp( int port )
{
}

void acamera_reset_ping_pong_port(void)
{
}

#else /* #if USER_MODULE */

int32_t acamera_init( acamera_settings *settings, uint32_t ctx_num )
{
    int32_t result = 0;
    uint32_t p_id = 0;

    result = validate_settings( settings, ctx_num );

    if ( result == 0 ) {
        p_id = acamera_isp_id_product_read(0);
        if (p_id != ACAMERA_ISP_ID_PRODUCT_DEFAULT) {
            LOG(LOG_ERR, "Failed to read profuct id\n");
            return 0;
        }
        LOG(LOG_ERR, "Success get product id: 0x%08x\n", p_id);
        // disable irq and clear interrupts
        acamera_isp_isp_global_interrupt_mask_vector_write( 0, ISP_IRQ_DISABLE_ALL_IRQ );


#if ACAMERA_ISP_PROFILING && ACAMERA_ISP_PROFILING_INIT
        acamera_profiler_init();
        acamera_profiler_start( 0 );
#endif

#if FW_HAS_CONTROL_CHANNEL
        ctrl_channel_init();
#endif


        if ( result == 0 ) {

            g_firmware.api_context = 0;

            system_semaphore_init( &g_firmware.sem_evt_avail );

            if ( ctx_num <= FIRMWARE_CONTEXT_NUMBER ) {
                uint32_t idx = 0;

                g_firmware.context_number = ctx_num;

                result = system_dma_init( &g_firmware.dma_chan_isp_config );
                result |= system_dma_init( &g_firmware.dma_chan_isp_metering );
                if ( result == 0 ) {
                    LOG( LOG_INFO, "DMA Channels allocated 0x%x and 0x%x", g_firmware.dma_chan_isp_config, g_firmware.dma_chan_isp_metering );
                    // allocate memory for dma transfers
                    for ( idx = 0; idx < ctx_num; idx++ ) {
                        acamera_context_t *p_ctx = (acamera_context_ptr_t)&g_firmware.fw_ctx[idx];
                        LOG( LOG_INFO, "Initialize context %d, context size is %d bytes", idx, sizeof( struct _acamera_context_t ) );
                        system_memset( (void *)p_ctx, 0x0, sizeof( struct _acamera_context_t ) );
                        // each context has unique id
                        p_ctx->context_id = idx;

                        // dump hw default configuration to the current context

                        p_ctx->sw_reg_map.isp_sw_config_map = system_sw_alloc( ACAMERA_CONTEXT_SIZE );

                        if ( p_ctx->sw_reg_map.isp_sw_config_map ) {
                            result = dma_channel_addresses_setup( g_firmware.dma_chan_isp_config, g_firmware.dma_chan_isp_metering, (void *)p_ctx->sw_reg_map.isp_sw_config_map, idx );
                        } else {
                            LOG( LOG_CRIT, "Software Context %d failed to allocate", idx );
                            result = -1;
                        }

                        if ( result )
                            break;

                        system_dma_copy_sg( g_firmware.dma_chan_isp_config, ISP_CONFIG_PING, SYS_DMA_FROM_DEVICE, 0 );
                        // init context
                        result = acamera_init_context( p_ctx, &settings[idx], &g_firmware );
                        if ( result == 0 ) {
                            // initialize ping
                            LOG( LOG_INFO, "DMA config from DDR to ping and pong of size %d", ACAMERA_ISP1_SIZE );
                            // system_dma_copy current software context to the ping and pong
                            system_dma_copy_sg( g_firmware.dma_chan_isp_config, ISP_CONFIG_PING, SYS_DMA_TO_DEVICE, 0 );
                            system_dma_copy_sg( g_firmware.dma_chan_isp_config, ISP_CONFIG_PONG, SYS_DMA_TO_DEVICE, 0 );
                            if ( result == 0 ) {
                                #if ISP_SENSOR_DRIVER_MODEL != 1
                                // avoid interrupt status check against the model
                                while ( acamera_isp_isp_global_interrupt_status_vector_read( 0 ) != 0 ) {
                                    // driver is initialized. we can start processing interrupts
                                    // wait until irq mask is cleared and start processing
                                    acamera_isp_isp_global_interrupt_clear_write( 0, 0 );
                                    acamera_isp_isp_global_interrupt_clear_write( 0, 1 );
                                }
                                #endif // #if ISP_SENSOR_DRIVER_MODEL != 1
                                acamera_isp_isp_global_interrupt_mask_vector_write( 0, ISP_IRQ_MASK_VECTOR );
                                g_firmware.initialized = 1;
                            } else {
                                LOG( LOG_CRIT, "One or more contexts were not initialized properly. " );
                                g_firmware.initialized = 0;
                                break;
                            }
                        } else {
                            LOG( LOG_CRIT, "Failed to initialized the context %d", (int)idx );
                            break;
                        }
                    }
                } else {
                    result = -1;
                    LOG( LOG_CRIT, "Failed to initialize the system DMA engines" );
                }
            } else {
                result = -1;
                LOG( LOG_CRIT, "Failed to initialized the firmware context. Not enough memory. " );
            }
        } else {
            LOG( LOG_CRIT, "Failed to initialized the BSP. Please implement the BSP layer and recompile the firmware" );
            result = -1;
        }

#if ACAMERA_ISP_PROFILING && ACAMERA_ISP_PROFILING_INIT
        acamera_profiler_stop( 0, 1 );
        acamera_profiler_report();
#endif

    } else {
        LOG( LOG_CRIT, "Input initializations settings are not correct" );
        result = -1;
    }
    acamera_isp_isp_global_mcu_override_config_select_write( 0, 1 ); //put ping pong in slave mode
    g_firmware.dma_flag_isp_config_completed = 1;
    g_firmware.dma_flag_isp_metering_completed = 1;
    return result;
}

void acamera_update_cur_settings_to_isp( int port )
{
    if (port == 0xff) {
        system_dma_copy_sg( g_firmware.dma_chan_isp_config, ISP_CONFIG_PING, SYS_DMA_TO_DEVICE, NULL );
        system_dma_copy_sg( g_firmware.dma_chan_isp_config, ISP_CONFIG_PONG, SYS_DMA_TO_DEVICE, NULL );
    } else if (port == ISP_CONFIG_PING) {
        system_dma_copy_sg( g_firmware.dma_chan_isp_config, ISP_CONFIG_PING, SYS_DMA_TO_DEVICE, NULL );
    } else if (port == ISP_CONFIG_PONG) {
        system_dma_copy_sg( g_firmware.dma_chan_isp_config, ISP_CONFIG_PONG, SYS_DMA_TO_DEVICE, NULL );
    }
}

void acamera_reset_ping_pong_port(void)
{
    acamera_isp_isp_global_mcu_ping_pong_config_select_write( 0, ISP_CONFIG_PING );
}

#endif /* #if USER_MODULE */

static void acamera_deinit( void )
{
    int idx;
    acamera_context_t *p_ctx;

#if FW_HAS_CONTROL_CHANNEL
    ctrl_channel_deinit();
#endif

    system_dma_destroy( g_firmware.dma_chan_isp_config );
    system_dma_destroy( g_firmware.dma_chan_isp_metering );
    for ( idx = 0; idx < g_firmware.context_number; idx++ ) {

        p_ctx = (acamera_context_ptr_t)&g_firmware.fw_ctx[idx];

        acamera_deinit_context( p_ctx );

        if ( p_ctx->sw_reg_map.isp_sw_config_map != NULL ) {
            system_sw_free( (void *)p_ctx->sw_reg_map.isp_sw_config_map );
            p_ctx->sw_reg_map.isp_sw_config_map = NULL;
        }
    }
}

int32_t acamera_terminate()
{
    acamera_logger_empty(); //empty the logger buffer and print remaining logs
    acamera_deinit();
    system_semaphore_destroy( g_firmware.sem_evt_avail );

    return 0;
}


#if USER_MODULE
int32_t acamera_interrupt_handler()
{
    return 0;
}
#else
// single context handler

static void start_processing_frame( void )
{
    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)&g_firmware.fw_ctx[0];

    // new_frame event to start reading metering memory and run 3A
    acamera_fw_raise_event( p_ctx, event_id_new_frame );
}

static void start_dropping_frame( void )
{
    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)&g_firmware.fw_ctx[0];

    acamera_fw_raise_event( p_ctx, event_id_drop_frame );
}

static void dma_complete_context_func( void *arg )
{
    LOG( LOG_INFO, "DMA COMPLETION FOR CONTEXT" );

    g_firmware.dma_flag_isp_config_completed = 1;


    if ( g_firmware.dma_flag_isp_config_completed && g_firmware.dma_flag_isp_metering_completed ) {
        LOG( LOG_INFO, "START PROCESSING FROM CONTEXT CALLBACK" );
        start_processing_frame();
    }

    system_dma_unmap_sg( arg );
}

static void dma_complete_metering_func( void *arg )
{
    LOG( LOG_INFO, "DMA COMPLETION FOR METERING" );

    g_firmware.dma_flag_isp_metering_completed = 1;

    if ( g_firmware.dma_flag_isp_config_completed && g_firmware.dma_flag_isp_metering_completed ) {
        LOG( LOG_INFO, "START PROCESSING FROM METERING CALLBACK" );
        start_processing_frame();
    }

    system_dma_unmap_sg( arg );
    // after we finish transfer context and metering we can start processing the current data
}

static void dma_drop_context_func( void *arg )
{
    g_firmware.dma_flag_isp_config_completed = 1;

    if ( g_firmware.dma_flag_isp_config_completed && g_firmware.dma_flag_isp_metering_completed ) {
        LOG( LOG_INFO, "START DROPPING FROM CONTEXT CALLBACK" );
        start_dropping_frame();
    }

    system_dma_unmap_sg( arg );
}

// single context handler
int32_t acamera_interrupt_handler()
{
    int32_t result = 0;
    int32_t irq_bit = ISP_INTERRUPT_EVENT_NONES_COUNT - 1;
    int not_empty = 0;
    LOG( LOG_INFO, "Interrupt handler called" );

    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)&g_firmware.fw_ctx[0];

    // read the irq vector from isp
    uint32_t irq_mask = acamera_isp_isp_global_interrupt_status_vector_read( 0 );

    LOG( LOG_INFO, "IRQ MASK is %d", irq_mask );

    // clear irq vector
    acamera_isp_isp_global_interrupt_clear_write( 0, 0 );
    acamera_isp_isp_global_interrupt_clear_write( 0, 1 );

    if ( irq_mask > 0 ) {

#if defined( ISP_INTERRUPT_EVENT_BROKEN_FRAME ) && defined( ISP_INTERRUPT_EVENT_MULTICTX_ERROR ) && defined( ISP_INTERRUPT_EVENT_DMA_ERROR ) && defined( ISP_INTERRUPT_EVENT_WATCHDOG_EXP ) && defined( ISP_INTERRUPT_EVENT_FRAME_COLLISION )
        //check for errors in the interrupt
        if ( ( irq_mask & 1 << ISP_INTERRUPT_EVENT_BROKEN_FRAME ) ||
             ( irq_mask & 1 << ISP_INTERRUPT_EVENT_MULTICTX_ERROR ) ||
             ( irq_mask & 1 << ISP_INTERRUPT_EVENT_DMA_ERROR ) ||
             ( irq_mask & 1 << ISP_INTERRUPT_EVENT_WATCHDOG_EXP ) ||
             ( irq_mask & 1 << ISP_INTERRUPT_EVENT_FRAME_COLLISION ) ) {

            LOG( LOG_CRIT, "Found error resetting ISP. MASK is 0x%x", irq_mask );

            acamera_fw_error_routine( p_ctx, irq_mask );
            return -1; //skip other interrupts in case of error
        }
#endif

        while ( irq_mask > 0 && irq_bit >= 0 ) {
            int32_t irq_is_1 = ( irq_mask & ( 1 << irq_bit ) );
            irq_mask &= ~( 1 << irq_bit );
            if ( irq_is_1 ) {
                // process interrupts
                if ( irq_bit == ISP_INTERRUPT_EVENT_ISP_START_FRAME_START ) {
                    LOG( LOG_INFO, "FS interrupt" );

#if ISP_DMA_RAW_CAPTURE
                    dma_raw_capture_interrupt( &g_firmware, ACAMERA_IRQ_FRAME_END );
#endif

                    if ( g_firmware.dma_flag_isp_metering_completed == 0 || g_firmware.dma_flag_isp_config_completed == 0 ) {
                        LOG( LOG_CRIT, "DMA is not finished, cfg: %d, meter: %d, skip this frame.", g_firmware.dma_flag_isp_config_completed, g_firmware.dma_flag_isp_metering_completed );
                        return -2;
                    }
                    not_empty = acamera_event_queue_not_empty( &p_ctx->fsm_mgr.event_queue );
                    // we must finish all previous processing before scheduling new dma
                    {
                        // switch to ping/pong contexts for the next frame
                        // these flags are used for sync of callbacks
                        g_firmware.dma_flag_isp_config_completed = 0;
                        g_firmware.dma_flag_isp_metering_completed = 0;

                        //if (!acamera_isp_isp_global_mcu_ping_pong_config_select_read(0)) { // cmodel compatibility
                        if ( acamera_isp_isp_global_ping_pong_config_select_read( 0 ) == ISP_CONFIG_PONG ) {
                            LOG( LOG_INFO, "Current config is pong" );
                            //            |^^^^^^^^^|
                            // next --->  |  PING   |
                            //            |_________|

                            // use ping for the next frame
                            acamera_isp_isp_global_mcu_ping_pong_config_select_write( 0, ISP_CONFIG_PING );
                            //
                            //            |^^^^^^^^^|
                            // conf --->  |  PONG   |
                            //            |_________|
                            if (!not_empty) {
                                LOG( LOG_INFO, "DMA metering from pong to DDR of size %d", ACAMERA_METERING_STATS_MEM_SIZE );
                                // dma all stat memory only to the software context
                                system_dma_copy_sg( g_firmware.dma_chan_isp_metering, ISP_CONFIG_PING, SYS_DMA_FROM_DEVICE, dma_complete_metering_func );

                                LOG( LOG_INFO, "DMA config from pong to DDR of size %d", ACAMERA_ISP1_SIZE );
                                system_dma_copy_sg( g_firmware.dma_chan_isp_config, ISP_CONFIG_PING, SYS_DMA_TO_DEVICE, dma_complete_context_func );
                            } else {
                                g_firmware.dma_flag_isp_metering_completed = 1;
                                system_dma_copy_sg( g_firmware.dma_chan_isp_config, ISP_CONFIG_PING, SYS_DMA_TO_DEVICE, dma_drop_context_func );
                            }
                        } else {
                            LOG( LOG_INFO, "Current config is ping" );
                            //            |^^^^^^^^^|
                            // next --->  |  PONG   |
                            //            |_________|

                            // use pong for the next frame
                            acamera_isp_isp_global_mcu_ping_pong_config_select_write( 0, ISP_CONFIG_PONG );

                            //            |^^^^^^^^^|
                            // conf --->  |  PING   |
                            //            |_________|

                            if (!not_empty) {
                                LOG( LOG_INFO, "DMA metering from ping to DDR of size %d", ACAMERA_METERING_STATS_MEM_SIZE );
                                // dma all stat memory only to the software context
                                system_dma_copy_sg( g_firmware.dma_chan_isp_metering, ISP_CONFIG_PONG, SYS_DMA_FROM_DEVICE, dma_complete_metering_func );

                                LOG( LOG_INFO, "DMA config from DDR to ping of size %d", ACAMERA_ISP1_SIZE );
                                system_dma_copy_sg( g_firmware.dma_chan_isp_config, ISP_CONFIG_PONG, SYS_DMA_TO_DEVICE, dma_complete_context_func );
                            } else {
                                g_firmware.dma_flag_isp_metering_completed = 1;
                                system_dma_copy_sg( g_firmware.dma_chan_isp_config, ISP_CONFIG_PONG, SYS_DMA_TO_DEVICE, dma_drop_context_func );
                            }
                        }
                    }
                } else {
                    // unhandled irq
                    LOG( LOG_INFO, "Unhandled interrupt bit %d", irq_bit );
                }
            }
            irq_bit--;
        }
    }

    return result;
}
#endif // USER_MODULE


int32_t acamera_process( void )
{
    int32_t result = 0;
    int32_t idx = 0;

    if ( g_firmware.initialized == 1 ) {
        for ( idx = 0; idx < g_firmware.context_number; idx++ ) {
            acamera_context_ptr_t p_ctx = ( acamera_context_ptr_t ) & ( g_firmware.fw_ctx[idx] );
            acamera_fw_process( p_ctx );
        }
    } else {
        result = -1;
        LOG( LOG_CRIT, "Firmware was not initialized properly. Please initialize the firmware before call acamera_process" );
    }

#if FW_HAS_CONTROL_CHANNEL
    ctrl_channel_process();
#endif

    system_semaphore_wait( g_firmware.sem_evt_avail, FW_EVT_QUEUE_TIMEOUT_MS );

    return result;
}
