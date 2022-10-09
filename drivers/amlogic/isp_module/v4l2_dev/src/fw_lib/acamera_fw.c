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
#include <linux/fs.h>
#include <asm/uaccess.h>

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
#include "system_am_sc1.h"
#include "system_am_sc2.h"
#include "system_am_sc3.h"
#include "system_autowrite.h"

#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

#if ISP_HAS_FPGA_WRAPPER
#include "acamera_fpga_config.h"
#endif

#if ISP_HAS_META_CB && defined( ISP_HAS_METADATA_FSM )
#include "metadata_api.h"
#endif

#include "acamera_3aalg_preset.h"

static const acam_reg_t **p_isp_data = SENSOR_ISP_SEQUENCE_DEFAULT;

uint32_t seamless = 0;
module_param(seamless, uint, 0664);
MODULE_PARM_DESC(seamless, "\n control seamless\n");

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

	if(seamless)
	{
		if(acamera_isp_input_port_mode_status_read( 0 ) != ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START)
			p_ctx->fsm_mgr.isp_seamless = 0;
		else
			p_ctx->fsm_mgr.isp_seamless = 1;
	}
	else
		p_ctx->fsm_mgr.isp_seamless = 0;
	
	LOG(LOG_CRIT, "seamless:%d", p_ctx->fsm_mgr.isp_seamless);

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
            LOG( LOG_INFO, "stopping isp failed, timeout: %u.", (unsigned int)count * 1000 );
            break;
        }
    }

    acamera_isp_isp_global_global_fsm_reset_write( p_ctx->settings.isp_base, 1 );
    acamera_isp_isp_global_global_fsm_reset_write( p_ctx->settings.isp_base, 0 );

    //return the interrupts
    acamera_isp_isp_global_interrupt_mask_vector_write( 0, ISP_IRQ_MASK_VECTOR );

    acamera_isp_input_port_mode_request_write( p_ctx->settings.isp_base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START );

    LOG( LOG_ERR, "starting isp from error" );
}


void acamera_fw_process( acamera_context_t *p_ctx )
{
#if ACAMERA_ISP_PROFILING
    if ( ( p_ctx->frame >= p_ctx->start_profiling ) && ( !p_ctx->binit_profiler ) ) {
        acamera_profiler_init();
        p_ctx->binit_profiler = 1;
    }
#endif
    if ( p_ctx->system_state == FW_RUN ) //need to capture on firmware freeze
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

int32_t acamera_extern_param_calculate(void *param)
{
    int32_t rtn = 0;
    uint32_t alpha = 0;
    uint32_t i, j;
    uint32_t r_cnt = 0;
    uint32_t c_cnt = 0;
    uint32_t l_gain = 0;
    uint32_t w_cnt = 0;
    uint32_t *c_param = NULL;
    uint32_t *c_result = NULL;
    uint32_t x0, y0, x1, y1;
    fsm_ext_param_ctrl_t *p_ctrl = param;

    if (p_ctrl == NULL || p_ctrl->ctx == NULL || p_ctrl->result == NULL) {
        LOG(LOG_CRIT, "Error input param");
        rtn = -1;
        goto over_return;
    }

    c_param = _GET_UINT_PTR(p_ctrl->ctx, p_ctrl->id);
    l_gain = (p_ctrl->total_gain) >> (LOG2_GAIN_SHIFT - 8);
    r_cnt = _GET_ROWS(p_ctrl->ctx, p_ctrl->id);
    c_cnt = _GET_COLS(p_ctrl->ctx, p_ctrl->id);
    w_cnt = _GET_WIDTH(p_ctrl->ctx, p_ctrl->id);
    c_result = p_ctrl->result;

    if (!c_param || (r_cnt == 0))
        return -1;

    if (l_gain <= c_param[0]) {
        system_memcpy(c_result, c_param, c_cnt * w_cnt);
        goto over_return;
    }

    if (l_gain >= c_param[(r_cnt - 1) * c_cnt]) {
        system_memcpy(c_result, &c_param[(r_cnt - 1) * c_cnt], c_cnt * w_cnt);
        goto over_return;
    }

    for (i = 1; i < r_cnt; i++) {
        if (l_gain < c_param[i * c_cnt]) {
            break;
        }
    }

    for (j = 1; j < c_cnt; j++) {
        x0 = c_param[(i - 1) * c_cnt];
        x1 = c_param[i * c_cnt];
        y0 = c_param[(i - 1) * c_cnt + j];
        y1 = c_param[i * c_cnt + j];
        if (x1 != x0) {
            alpha = (l_gain - x0) * 256 / (x1 - x0);
            c_result[j] = (y1 * alpha + y0 * (256 - alpha)) >> 8;
        } else {
            c_result[j] = y1;
            LOG(LOG_CRIT, "AVOIDED DIVISION BY ZERO");
        }
    }

    c_result[0] = l_gain;

over_return:
    return rtn;
}

int32_t acamera_extern_param_calculate_ushort(void *param)
{
    int32_t rtn = 0;
    uint32_t alpha = 0;
    uint32_t i, j;
    uint32_t r_cnt = 0;
    uint32_t c_cnt = 0;
    uint32_t l_gain = 0;
    uint32_t w_cnt = 0;
    uint16_t *c_param = NULL;
    uint16_t *c_result = NULL;
    uint32_t x0, y0, x1, y1;
    fsm_ext_param_ctrl_t *p_ctrl = param;

    if (p_ctrl == NULL || p_ctrl->ctx == NULL || p_ctrl->result == NULL) {
        LOG(LOG_CRIT, "Error input param");
        rtn = -1;
        goto over_return;
    }

    c_param = _GET_USHORT_PTR(p_ctrl->ctx, p_ctrl->id);
    l_gain = (p_ctrl->total_gain) >> (LOG2_GAIN_SHIFT - 8);
    r_cnt = _GET_ROWS(p_ctrl->ctx, p_ctrl->id);
    c_cnt = _GET_COLS(p_ctrl->ctx, p_ctrl->id);
    w_cnt = _GET_WIDTH(p_ctrl->ctx, p_ctrl->id);
    c_result = p_ctrl->result;

    if (!c_param || (r_cnt == 0))
        return -1;

    if (l_gain <= c_param[0]) {
        system_memcpy(c_result, c_param, c_cnt * w_cnt);
        goto over_return;
    }

    if (l_gain >= c_param[(r_cnt - 1) * c_cnt]) {
        system_memcpy(c_result, &c_param[(r_cnt - 1) * c_cnt], c_cnt * w_cnt);
        goto over_return;
    }

    for (i = 1; i < r_cnt; i++) {
        if (l_gain < c_param[i * c_cnt]) {
            break;
        }
    }

    for (j = 1; j < c_cnt; j++) {
        x0 = c_param[(i - 1) * c_cnt];
        x1 = c_param[i * c_cnt];
        y0 = c_param[(i - 1) * c_cnt + j];
        y1 = c_param[i * c_cnt + j];
        if (x1 != x0) {
            alpha = (l_gain - x0) * 256 / (x1 - x0);
            c_result[j] = (y1 * alpha + y0 * (256 - alpha)) >> 8;
        } else {
            c_result[j] = y1;
            LOG(LOG_CRIT, "AVOIDED DIVISION BY ZERO");
        }
    }

    c_result[0] = l_gain;

over_return:
    return rtn;
}

static void acamera_open_external_bin(void *ctx, struct file **fp, uint32_t *size)
{
    uint32_t mode = 0;
    char f_name[40] = {'\0'};
    acamera_context_ptr_t p_ctx;
    struct kstat stat;

    if (ctx == NULL || fp == NULL) {
        LOG(LOG_ERR, "Error input param");
        return;
    }

    p_ctx = ctx;

    acamera_fsm_mgr_get_param(&p_ctx->fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &mode, sizeof(mode));

    switch (mode) {
    case WDR_MODE_LINEAR:
        snprintf(f_name, sizeof(f_name), "/data/isp_tuning/tuning_linear.bin");
    break;
    case WDR_MODE_NATIVE:
        snprintf(f_name, sizeof(f_name), "/data/isp_tuning/tuning_native.bin");
    break;
    case WDR_MODE_FS_LIN:
        snprintf(f_name, sizeof(f_name), "/data/isp_tuning/tuning_fs_lin.bin");
    break;
    default:
        LOG(LOG_ERR, "Error input mode %u", mode);
        return;
    }

    if (vfs_stat(f_name, &stat)) {
        return;
    }

    *size = stat.size;

    *fp = filp_open(f_name, O_RDONLY, 0);
}

static int32_t acamera_read_external_bin(struct file *fp, uint8_t *buf, uint32_t size)
{
    loff_t pos = 0;
    int32_t nread = -1;

    if (fp == NULL || buf == NULL || !size) {
        LOG(LOG_ERR, "Error input param");
        return nread;
    }

    nread = vfs_read(fp, buf, size, &pos);

    return nread;
}

static void acamera_update_external_calibrations(acamera_context_ptr_t p_ctx)
{
    int32_t nread = 0;
    uint32_t idx = 0;
    uint32_t l_size = 0;
    uint32_t t_size = 0;
    uint8_t *l_ptr = NULL;
    uint8_t *b_buf = NULL;
    uint8_t *p_mem = NULL;
    uint32_t f_size = 0;
    struct file *fp = NULL;
    mm_segment_t fs;

    fs = get_fs();
    set_fs(KERNEL_DS);

    acamera_open_external_bin(p_ctx, &fp, &f_size);
    if (fp == NULL || !f_size) {
        LOG(LOG_ERR, "Bin file not exsit");
        goto error_exit;
    }

    for (idx = 0; idx < CALIBRATION_TOTAL_SIZE; idx++) {
        l_size = _GET_SIZE(p_ctx, idx);
        t_size += l_size;
    }

    if (t_size != f_size) {
        LOG(LOG_ERR, "Bin size not match: f_size %u, t_size %u", f_size, t_size);
        goto error_size;
    }

    b_buf = kzalloc(t_size, GFP_KERNEL);
    if (b_buf == NULL) {
        LOG(LOG_ERR, "Failed to alloc mem");
        goto error_size;
    }

    nread = acamera_read_external_bin(fp, b_buf, f_size);
    if (nread != f_size) {
        LOG(LOG_ERR, "Failed to read bin");
        goto error_read;
    }

    p_mem = b_buf;

    for (idx = 0; idx < CALIBRATION_TOTAL_SIZE; idx++) {
        l_ptr = (uint8_t *)_GET_LUT_PTR(p_ctx, idx);
        l_size = _GET_SIZE(p_ctx, idx);
        if (l_size)
            memcpy(l_ptr, p_mem, l_size);
        p_mem += l_size;
    }

    LOG(LOG_CRIT, "Success update tuning bin");

error_read:
    if (b_buf)
        kfree(b_buf);
error_size:
    if (fp)
        filp_close(fp, NULL);
error_exit:
    set_fs(fs);
}

int32_t acamera_update_calibration_set( acamera_context_ptr_t p_ctx, char* s_name)
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
                ((sensor_mode_t *)sensor_arg)->cali_mode = p_ctx->cali_mode;
            }
        }
        if ( p_ctx->settings.get_calibrations( p_ctx->context_id, sensor_arg, &p_ctx->acameraCalibrations, s_name) != 0 ) {
            LOG( LOG_CRIT, "Failed to get calibration set for. Fatal error" );
        }

        acamera_update_external_calibrations(p_ctx);

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

int32_t acamera_update_calibration_customer( acamera_context_ptr_t p_ctx, uint32_t mode)
{
    int32_t result = 0;
    sensor_mode_t sensor_arg;

    p_ctx->system_state = FW_PAUSE;

    if ( p_ctx->settings.get_calibrations != NULL ) {
        {
            const sensor_param_t *param = NULL;
            acamera_fsm_mgr_get_param( &p_ctx->fsm_mgr, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );

            uint32_t cur_mode = param->mode;
            if ( cur_mode < param->modes_num ) {
                sensor_arg.wdr_mode = param->modes_table[cur_mode].wdr_mode;
                sensor_arg.fps = param->modes_table[cur_mode].fps;
                sensor_arg.exposures = param->modes_table[cur_mode].exposures;
                sensor_arg.resolution.width = param->modes_table[cur_mode].resolution.width;
                sensor_arg.resolution.height = param->modes_table[cur_mode].resolution.height;
                sensor_arg.cali_mode = mode;
                p_ctx->cali_mode = mode;
            }
        }
        if ( p_ctx->settings.get_calibrations( p_ctx->context_id, &sensor_arg, &p_ctx->acameraCalibrations, NULL) != 0 ) {
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
        acamera_fsm_mgr_set_param( &p_ctx->fsm_mgr, FSM_PARAM_SET_SBUF_CALIBRATION_UPDATE, &mode, sizeof(uint32_t) );
#endif
    } else {
        LOG( LOG_CRIT, "Calibration callback is null. Failed to get calibrations" );
        result = -1;
    }

    p_ctx->system_state = FW_RUN;

    return result;
}

int32_t acamera_init_calibrations( acamera_context_ptr_t p_ctx , char* s_name)
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
        acamera_update_calibration_set( p_ctx, s_name );
    } else {
        if ( p_ctx->settings.get_calibrations != NULL ) {
            const sensor_param_t *param = NULL;
            acamera_fsm_mgr_get_param( &p_ctx->fsm_mgr, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );

            uint32_t cur_mode = param->mode;
            if ( cur_mode < param->modes_num ) {
                sensor_arg = &( param->modes_table[cur_mode] );
            }

            if ( p_ctx->settings.get_calibrations( p_ctx->context_id, sensor_arg, &p_ctx->acameraCalibrations, s_name ) != 0 ) {
                LOG( LOG_CRIT, "Failed to get calibration set for. Fatal error" );
            }

            acamera_update_external_calibrations(p_ctx);
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

#if ISP_HAS_SC0
// Callback from SC0 output pipe
static void external_callback_sc0( void *ctx, tframe_t *tframe, const metadata_t *metadata )
{

    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)ctx;
    if ( p_ctx->settings.callback_sc0 != NULL ) {
        p_ctx->settings.callback_sc0( p_ctx->context_id, tframe, metadata );
    }
}
#endif

#if ISP_HAS_SC1
// Callback from SC1 output pipe
static void external_callback_sc1( void *ctx, tframe_t *tframe, const metadata_t *metadata )
{
    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)ctx;
    if ( p_ctx->settings.callback_sc1 != NULL ) {
        p_ctx->settings.callback_sc1( p_ctx->context_id, tframe, metadata );
    }
}
#endif

#if ISP_HAS_SC2
// Callback from SC2 output pipe
static void external_callback_sc2( void *ctx, tframe_t *tframe, const metadata_t *metadata )
{
    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)ctx;
    if ( p_ctx->settings.callback_sc2 != NULL ) {
        p_ctx->settings.callback_sc2( p_ctx->context_id, tframe, metadata );
    }
}
#endif

#if ISP_HAS_SC3
// Callback from SC3 output pipe
static void external_callback_sc3( void *ctx, tframe_t *tframe, const metadata_t *metadata )
{
    acamera_context_ptr_t p_ctx = (acamera_context_ptr_t)ctx;
    if ( p_ctx->settings.callback_sc3 != NULL ) {
        p_ctx->settings.callback_sc3( p_ctx->context_id, tframe, metadata );
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

#if ISP_HAS_SC0
    am_sc_set_callback(p_ctx, external_callback_sc0);
#endif
#if ISP_HAS_SC1
    am_sc1_set_callback(p_ctx, external_callback_sc1);
#endif
#if ISP_HAS_SC2
    am_sc2_set_callback(p_ctx, external_callback_sc2);
#endif
#if ISP_HAS_SC3
    am_sc3_set_callback(p_ctx, external_callback_sc3);
#endif
}

void acamera_fw_get_sensor_name(uint32_t *sname)
{
    acamera_command(0, TSENSOR, SENSOR_NAME, 0, COMMAND_GET, sname);
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
    p_ctx->stab.global_manual_ccm = 0;
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
    p_ctx->stab.global_awb_green_even_gain = 256;
    p_ctx->stab.global_awb_green_odd_gain = 256;
    p_ctx->stab.global_awb_blue_gain = 256;
    p_ctx->stab.global_ccm_matrix[0] = 0x0100;
    p_ctx->stab.global_ccm_matrix[1] = 0x0000;
    p_ctx->stab.global_ccm_matrix[2] = 0x0000;
    p_ctx->stab.global_ccm_matrix[3] = 0x0000;
    p_ctx->stab.global_ccm_matrix[4] = 0x0100;
    p_ctx->stab.global_ccm_matrix[5] = 0x0000;
    p_ctx->stab.global_ccm_matrix[6] = 0x0000;
    p_ctx->stab.global_ccm_matrix[7] = 0x0000;
    p_ctx->stab.global_ccm_matrix[8] = 0x0100;
    p_ctx->stab.global_saturation_target = 0;
    p_ctx->stab.global_ae_compensation = SYSTEM_AE_COMPENSATION_DEFAULT;
    p_ctx->stab.global_calibrate_bad_pixels = 0;
    p_ctx->stab.global_dynamic_gamma_enable = 1;
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

int acamera_3aalg_enable(void)
{
#ifdef ACAMERA_PRESET_FREERTOS
	acamera_alg_preset_t * total_param;
	char *reserve_virt_addr = phys_to_virt(ACAMERA_ALG_PRE_BASE);
	if(autowrite_fr_start_address_read())
		reserve_virt_addr = phys_to_virt(autowrite_fr_start_address_read() + autowrite_fr_writer_memsize_read());
	else if(autowrite_ds1_start_address_read())
		reserve_virt_addr = phys_to_virt(autowrite_ds1_start_address_read() + autowrite_ds1_writer_memsize_read());

	//system_memcpy((void *)&total_param, (void *)reserve_virt_addr, sizeof(total_param));
	total_param = (acamera_alg_preset_t *)reserve_virt_addr; 

	if(total_param->ae_pre_info.skip_cnt == 0xFFFF && total_param->awb_pre_info.skip_cnt == 0xFFFF)
	{
		return 1;
	}
#endif
	return 0;
}

void acamera_3aalg_preset(acamera_fsm_mgr_t *p_fsm_mgr)
{
	isp_ae_preset_t ae_param;
	isp_awb_preset_t awb_param;
	isp_gamma_preset_t gamma_param;
	isp_iridix_preset_t iridix_param;

	if ( _GET_LEN(p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AE_CONTROL) &&
		 _GET_LEN(p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL) &&
		 _GET_LEN(p_fsm_mgr->p_ctx, CALIBRATION_3AALG_GAMMA_CONTROL) &&
		 _GET_LEN(p_fsm_mgr->p_ctx, CALIBRATION_3AALG_IRIDIX_CONTROL) ) {
		ae_param.skip_cnt = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AE_CONTROL )[0];
		ae_param.exposure_log2 = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AE_CONTROL )[1];
		ae_param.integrator = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AE_CONTROL )[2];
		ae_param.error_log2 = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AE_CONTROL )[3];
		ae_param.exposure_ratio = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AE_CONTROL )[4];

		awb_param.skip_cnt = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[0];
		awb_param.wb_log2[0] = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[1];
		awb_param.wb_log2[1] = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[2];
		awb_param.wb_log2[2] = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[3];
		awb_param.wb_log2[3] = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[4];
		awb_param.wb[0] = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[5];
		awb_param.wb[1] = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[6];
		awb_param.wb[2] = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[7];
		awb_param.wb[3] = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[8];
		awb_param.global_awb_red_gain = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[9];
		awb_param.global_awb_blue_gain = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[10];
		awb_param.p_high = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[11];
		awb_param.temperature_detected = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[12];
		awb_param.light_source_candidate = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_AWB_CONTROL )[13];

		gamma_param.skip_cnt = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_GAMMA_CONTROL )[0];
		gamma_param.gamma_gain = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_GAMMA_CONTROL )[1];
		gamma_param.gamma_offset = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_GAMMA_CONTROL )[2];

		iridix_param.skip_cnt = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_IRIDIX_CONTROL )[0];
		iridix_param.strength_target = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_IRIDIX_CONTROL )[1];
		iridix_param.iridix_contrast = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_IRIDIX_CONTROL )[2];
		iridix_param.dark_enh = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_IRIDIX_CONTROL )[3];
		iridix_param.iridix_global_DG = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_IRIDIX_CONTROL )[4];
		iridix_param.diff = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_IRIDIX_CONTROL )[5];
		iridix_param.iridix_strength = _GET_UINT_PTR( p_fsm_mgr->p_ctx, CALIBRATION_3AALG_IRIDIX_CONTROL )[6];

		fsm_param_sensor_info_t sensor_info;
		acamera_fsm_mgr_get_param( p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );
		if ( sensor_info.sensor_exp_number == WDR_MODE_FS_LIN )
			LOG(LOG_CRIT, "Enter FS_Lin_2Exp Preset");
		else
			LOG(LOG_CRIT, "Enter Linear Binary Preset");
	} else {
		ae_param.skip_cnt = 0;
		awb_param.skip_cnt = 0;
		gamma_param.skip_cnt = 0;
		iridix_param.skip_cnt = 0;
	}
#ifdef ACAMERA_PRESET_FREERTOS
	acamera_alg_preset_t total_param;

	if ( p_fsm_mgr->isp_seamless ) {
		char *reserve_virt_addr = phys_to_virt(ACAMERA_ALG_PRE_BASE);

		if ( autowrite_fr_start_address_read() )
			reserve_virt_addr = phys_to_virt(autowrite_fr_start_address_read() + autowrite_fr_writer_memsize_read());
		else if ( autowrite_ds1_start_address_read() )
			reserve_virt_addr = phys_to_virt(autowrite_ds1_start_address_read() + autowrite_ds1_writer_memsize_read());

		system_memcpy((void *)&total_param, (void *)reserve_virt_addr, sizeof(total_param));
	}

	if(total_param.ae_pre_info.skip_cnt == 0xFFFF && total_param.awb_pre_info.skip_cnt == 0xFFFF)
	{
	    total_param.ae_pre_info.skip_cnt = 5;
		total_param.awb_pre_info.skip_cnt = 15;
		total_param.gamma_pre_info.skip_cnt = 30;
		total_param.iridix_pre_info.skip_cnt = 90;

		system_memcpy(&ae_param, &total_param.ae_pre_info,sizeof(ae_param));
		system_memcpy(&awb_param, &total_param.awb_pre_info,sizeof(awb_param));
		system_memcpy(&gamma_param, &total_param.gamma_pre_info,sizeof(gamma_param));
		system_memcpy(&iridix_param, &total_param.iridix_pre_info,sizeof(iridix_param));

	    LOG(LOG_CRIT, "Preset Value ae_param: %d,%d,%d,%d, Gamma:%d,%d",ae_param.skip_cnt,\
		ae_param.exposure_log2,ae_param.error_log2,ae_param.integrator,gamma_param.gamma_gain,gamma_param.gamma_offset);
	}
	else
#endif
	{
		uint8_t input = 0xff;

		//Configure donot skip sensor initialization with test pattern command.
		//mode == 0xff would configure to call sending sensor setting.
		acamera_fsm_mgr_set_param(p_fsm_mgr, FSM_PARAM_SET_SENSOR_TEST_PATTERN, &input, sizeof(input));
	}

	ctrl_channel_3aalg_param_init(&ae_param,&awb_param,&gamma_param,&iridix_param);

	acamera_fsm_mgr_set_param( p_fsm_mgr, FSM_PARAM_SET_AE_PRESET, &ae_param, sizeof(ae_param) );
	acamera_fsm_mgr_set_param( p_fsm_mgr, FSM_PARAM_SET_AWB_PRESET, &awb_param, sizeof(awb_param) );
	acamera_fsm_mgr_set_param( p_fsm_mgr, FSM_PARAM_SET_GAMMA_PRESET, &gamma_param, sizeof(gamma_param) );
	acamera_fsm_mgr_set_param( p_fsm_mgr, FSM_PARAM_SET_IRIDIX_PRESET, &iridix_param, sizeof(iridix_param) );
}

int32_t acamera_init_context( acamera_context_t *p_ctx, acamera_settings *settings, acamera_firmware_t *g_fw )
{
	int32_t result = 0;
	// keep the context pointer for debug purposes
	p_ctx->context_ref = (uint32_t *)p_ctx;
	p_ctx->p_gfw = g_fw;
	if ( p_ctx->sw_reg_map.isp_sw_config_map != NULL ) {
        // copy settings
        system_memcpy( (void *)&p_ctx->settings, (void *)settings, sizeof( acamera_settings ) );

        p_ctx->settings.isp_base = (uintptr_t)p_ctx->sw_reg_map.isp_sw_config_map;

        // each context is initialized to the default state
        p_ctx->isp_sequence = p_isp_data;

#if FW_DO_INITIALIZATION
		if(seamless)
		{
			if(acamera_isp_input_port_mode_status_read( 0 ) != ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START)
				acamera_load_isp_sequence( 0, p_ctx->isp_sequence, SENSOR_ISP_SEQUENCE_DEFAULT_SETTINGS );
		}
		else
			acamera_load_isp_sequence( 0, p_ctx->isp_sequence, SENSOR_ISP_SEQUENCE_DEFAULT_SETTINGS );
#endif

#if defined( SENSOR_ISP_SEQUENCE_DEFAULT_SETTINGS_CONTEXT )
		acamera_load_sw_sequence( p_ctx->settings.isp_base, p_ctx->isp_sequence, SENSOR_ISP_SEQUENCE_DEFAULT_SETTINGS_CONTEXT );
#endif

#if defined( SENSOR_ISP_SEQUENCE_DEFAULT_SETTINGS_FPGA ) && ISP_HAS_FPGA_WRAPPER
        // these settings are loaded only for ARM FPGA demo platform and must be ignored on other systems
        acamera_load_isp_sequence( 0, p_ctx->isp_sequence, SENSOR_ISP_SEQUENCE_DEFAULT_SETTINGS_FPGA );
#endif

        // reset frame counters
        p_ctx->isp_frame_counter_raw = 0;
        p_ctx->isp_frame_counter = 0;
        p_ctx->cali_mode = 0;
        p_ctx->isp_decmp_counter = 0;
        p_ctx->isp_decmp_counter_last = 0;

        acamera_fw_init( p_ctx );

        acamera_init_context_seq(p_ctx);

        configure_all_frame_buffers( p_ctx );

        init_stab( p_ctx );

        // the custom initialization may be required for a context
        if ( p_ctx->settings.custom_initialization != NULL ) {
            p_ctx->settings.custom_initialization( p_ctx->context_id );
        }

        //acamera_isp_input_port_mode_request_write( p_ctx->settings.isp_base, ACAMERA_ISP_INPUT_PORT_MODE_REQUEST_SAFE_START );

        p_ctx->initialized = 1;

    } else {
        result = -1;
        LOG( LOG_CRIT, "Failed to allocate memory for ISP config context" );
    }

    acamera_isp_iridix_context_no_write(p_ctx->fsm_mgr.isp_base, p_ctx->context_id);
    acamera_3aalg_preset(&p_ctx->fsm_mgr);
    //acamera_isp_top_bypass_temper_write(p_ctx->fsm_mgr.isp_base, 1);

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
