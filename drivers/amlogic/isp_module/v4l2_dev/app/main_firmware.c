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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/errno.h>

#include "acamera.h"
#include "acamera_firmware_api.h"
#include "acamera_firmware_config.h"
#include "acamera_command_api.h"
#include "acamera_control_config.h"
#include "system_control.h"
#if ISP_HAS_CONNECTION_DEBUG
#include "acamera_cmd_interface.h"
#endif

#if ISP_HAS_STREAM_CONNECTION
#include "acamera_connection.h"
#endif

#include "application_command_api.h"

/* Entire main_firmware is not used when V4L2 interface is enabled.
 * All relevent functions and codes here can be found in fw-interface.c
 */
// the settings for each firmware context were pre-generated and
// saved in the header file. They are given as a reference and should be changed
// according to the customer needs.
#include "runtime_initialization_settings.h"

extern uint8_t *isp_kaddr;
extern resource_size_t isp_paddr;

static struct task_struct *isp_fw_process_thread = NULL;

#if !V4L2_INTERFACE_BUILD //all these callbacks are managed by v4l2
// This function can be used to customize initialization process.
// It is called by the firmware at the end of context initialization.
// The input pointer to a context can be used to differentiate contexts
void custom_initialization( uint32_t ctx_num )
{
}


// The driver can provide the full set of metadata parameters
// The callback_meta function should be set in initalization settings to support it.
void callback_meta( uint32_t ctx_num, const void *fw_metadata )
{
}


// The ISP pipeline can have several outputs such as Full Resolution, DownScaler1, DownScaler2 etc
// It is possible to set the firmware up for returning metadata on each output frame from
// the specific channel. This callbacks must be set in acamera_settings structure and passed to the firmware in
// acamera_init api function
// The pointer to the context can be used to differentiate contexts

// Callback from FR output pipe
void callback_fr( uint32_t ctx_num, tframe_t *tframe, const metadata_t *metadata )
{
}

// Callback from DS1 output pipe
void callback_ds1( uint32_t ctx_num, tframe_t *tframe, const metadata_t *metadata )
{
}

// Callback from DS2 output pipe
void callback_ds2( uint32_t ctx_num, tframe_t *tframe, const metadata_t *metadata )
{
}

#endif

#if ISP_HAS_STREAM_CONNECTION && CONNECTION_IN_THREAD

static struct task_struct *isp_fw_connections_thread = NULL;

static int connection_thread( void *foo )
{
    LOG( LOG_CRIT, "connection_thread start" );

    acamera_connection_init();

    while ( !kthread_should_stop() ) {
        acamera_connection_process();
    }

    acamera_connection_destroy();

    LOG( LOG_CRIT, "connection_thread stop" );

    return 0;
}
#endif

static int isp_fw_process( void *data )
{
    LOG( LOG_CRIT, "isp_fw_process start" );

#if ISP_HAS_STREAM_CONNECTION && !CONNECTION_IN_THREAD
    acamera_connection_init();
#endif

    while ( !kthread_should_stop() ) {
        acamera_process();

#if ISP_HAS_STREAM_CONNECTION && !CONNECTION_IN_THREAD
        acamera_connection_process();
#endif
    }

#if ISP_HAS_STREAM_CONNECTION && !CONNECTION_IN_THREAD
    acamera_connection_destroy();
#endif

    LOG( LOG_CRIT, "isp_fw_process stop" );
    return 0;
}


// this is a main application IRQ handler to drive firmware
// The main purpose is to redirect irq events to the
// appropriate firmware context.
// There are several type of firmware IRQ which may happen.
// The other events are platform specific. The firmware can support several external irq events or
// does not support any. It totally depends on the system configuration and firmware compile time settings.
// Please see the ACamera Porting Guide for details.
static void interrupt_handler( void *data, uint32_t mask )
{
    acamera_interrupt_handler();
}

void isp_update_setting(void)
{
    aframe_t *aframe = NULL;
    uint32_t fr_num = 0;
    resource_size_t paddr = 0;
    int i = 0;

    if (isp_paddr == 0 || isp_kaddr == NULL) {
        pr_err("%s: Error input isp cma addr\n", __func__);
        return;
    }

    paddr = isp_paddr;

    aframe = settings[0].temper_frames;
    fr_num = settings[0].temper_frames_number;

    for (i = 0; i < fr_num; i++) {
        aframe[i].address = paddr;

        paddr = aframe[i].address + aframe[i].size;
    }

    settings[0].temper_frames_number = 1;

}

int isp_fw_init( void )
{
    int result = 0;

    LOG( LOG_INFO, "fw_init start" );

    isp_update_setting();

    // The firmware supports multicontext.
    // It means that the customer can use the same firmware for controlling
    // several instances of different sensors/isp. To initialise a context
    // the structure acamera_settings must be filled properly.
    // the total number of initialized context must not exceed FIRMWARE_CONTEXT_NUMBER
    // all contexts are numerated from 0 till ctx_number - 1
    result = acamera_init( settings, FIRMWARE_CONTEXT_NUMBER );

    if ( result == 0 ) {
        //application_command(TGENERAL, ACTIVE_CONTEXT, 0, COMMAND_GET, &prev_ctx_num);

        // set the interrupt handler. The last parameter may be used
        // to specify the context. The system must call this interrupt_handler
        // function whenever the ISP interrupt happens.
        // This interrupt handling procedure is only advisable and is used in ACamera demo application.
        // It can be changed by a customer discretion.
        system_interrupt_set_handler( interrupt_handler, NULL );
    } else {
        LOG( LOG_INFO, "Failed to start firmware processing thread. " );
    }

    LOG( LOG_INFO, "isp_fw_init result %d", result );
    if ( result == 0 ) {

#if ISP_HAS_STREAM_CONNECTION && CONNECTION_IN_THREAD
        LOG( LOG_INFO, "start connection thread %d", result );
        isp_fw_connections_thread = kthread_run( connection_thread, NULL, "isp_connection" );
#endif

        LOG( LOG_INFO, "start fw thread %d", result );
        isp_fw_process_thread = kthread_run( isp_fw_process, NULL, "isp_process" );
    }

    return PTR_RET( isp_fw_process_thread );
}

void isp_fw_exit( void )
{
#if ISP_HAS_STREAM_CONNECTION && CONNECTION_IN_THREAD
    if ( isp_fw_connections_thread )
        kthread_stop( isp_fw_connections_thread );
#endif

    if ( isp_fw_process_thread ) {
        kthread_stop( isp_fw_process_thread );
    }

    // this api function will free
    // all resources allocated by the firmware
    acamera_terminate();

    bsp_destroy();
}
