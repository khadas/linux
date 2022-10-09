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

//-------------------------------------------------------------------------------------
//STRUCTURE:
//  VARIABLE SECTION:
//        CONTROLS - dependence from preprocessor
//        DATA     - modulation
//        RESET     - reset function
//        MIPI     - mipi settings
//        FLASH     - flash support
//  CONSTANT SECTION
//        DRIVER
//-------------------------------------------------------------------------------------

#include <linux/delay.h>
#include "acamera_types.h"
#include "sensor_init.h"
#include "acamera_math.h"
#include "system_sensor.h"
#include "acamera_command_api.h"
#include "acamera_sbus_api.h"
#include "acamera_sensor_api.h"
#include "system_timer.h"
#include "acamera_firmware_config.h"
#include "sensor_bus_config.h"
#include "OV5640_seq.h"
#include "OV5640_config.h"
#include "system_am_mipi.h"
#include "system_am_adap.h"
#include "sensor_bsp_common.h"

#define AGAIN_PRECISION 7
#define NEED_CONFIG_BSP 1   //config bsp by sensor driver owner

#define FS_LIN_1080P 1

static void start_streaming( void *ctx );
static void stop_streaming( void *ctx );

static sensor_context_t sensor_ctx;

// 0 - reset & power-enable
// 1 - reset-sub & power-enable-sub
// 2 - reset-ssub & power-enable-ssub
static const int32_t config_sensor_idx = 0;                  // 1 2 3
static const char * reset_dts_pin_name = "reset";            // reset-sub  reset-ssub
static const char * pwr_dts_pin_name   = "power-enable";     // power-enalbe-sub power-enable-ssub


static sensor_mode_t supported_modes[] = {
    {
        .wdr_mode = WDR_MODE_LINEAR,
        .fps = 30 * 256,
        .resolution.width = 1920,
        .resolution.height = 1080,
        .bits = 8,
        .exposures = 1,
        .lanes = 2,
        .bps = 672,
        .bayer = BAYER_YUY2,
        .dol_type = DOL_YUV,
        .num = 0,
    },
    {
        .wdr_mode = WDR_MODE_LINEAR,
        .fps = 30 * 256,
        .resolution.width = 2592,
        .resolution.height = 1944,
        .bits = 8,
        .exposures = 1,
        .lanes = 2,
        .bps = 672,
        .bayer = BAYER_YUY2,
        .dol_type = DOL_YUV,
        .num = 1,
    },
};

#if SENSOR_BINARY_SEQUENCE
static const char p_sensor_data[] = SENSOR__OV5640_SEQUENCE_DEFAULT;
static const char p_isp_data[] = SENSOR__OV5640_ISP_SEQUENCE_DEFAULT;
#else
static const acam_reg_t **p_sensor_data = seq_table;
static const acam_reg_t **p_isp_data = isp_seq_table;
#endif
//--------------------RESET------------------------------------------------------------
static void sensor_hw_reset_enable( void )
{
    system_reset_sensor( 0 );
}

static void sensor_hw_reset_disable( void )
{
    system_reset_sensor( 3 );
}

//-------------------------------------------------------------------------------------
static int32_t sensor_alloc_analog_gain( void *ctx, int32_t gain )
{
    return gain;
}

static int32_t sensor_alloc_digital_gain( void *ctx, int32_t gain )
{
    return 0;
}

static void sensor_alloc_integration_time( void *ctx, uint16_t *int_time_S, uint16_t *int_time_M, uint16_t *int_time_L )
{
}

static int32_t sensor_ir_cut_set( void *ctx, int32_t ir_cut_state )
{
    return 0;
}

static void sensor_update( void *ctx )
{
}

static uint32_t sensor_vmax_fps( void *ctx, uint32_t framerate )
{
    return 0;
}

static uint16_t sensor_get_id( void *ctx )
{
    /* return that sensor id register does not exist */

    sensor_context_t *p_ctx = ctx;
    uint32_t sensor_id = 0;

    sensor_id |= acamera_sbus_read_u8(&p_ctx->sbus, 0x300a) << 8;
    sensor_id |= acamera_sbus_read_u8(&p_ctx->sbus, 0x300b);

    if (sensor_id != SENSOR_CHIP_ID) {
        LOG(LOG_CRIT, "%s: Failed to 5640 sensor id %x\n", __func__, sensor_id);
        return 0xFFFF;
    }

    return sensor_id;
}

static void sensor_set_mode( void *ctx, uint8_t mode )
{
    sensor_context_t *p_ctx = ctx;
    sensor_param_t *param = &p_ctx->param;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    uint8_t setting_num = param->modes_table[mode].num;

    pwr_am_enable(p_ctx->sbp, pwr_dts_pin_name, config_sensor_idx, 1);
    sensor_hw_reset_enable();
    system_timer_usleep( 10000 );
    sensor_hw_reset_disable();
    pwr_am_enable(p_ctx->sbp, pwr_dts_pin_name, config_sensor_idx, 0);
    system_timer_usleep( 10000 );

    p_ctx->again[0] = 0;
    p_ctx->int_time_S = 0;
    p_ctx->int_time_L = 0;

    if (sensor_get_id(ctx) == 0xFFFF) {
        LOG(LOG_INFO, "%s: check sensor failed\n", __func__);
        return;
    }

    switch ( param->modes_table[mode].wdr_mode ) {
    case WDR_MODE_LINEAR:
        sensor_load_sequence( p_sbus, p_ctx->seq_width, p_sensor_data, setting_num);
        p_ctx->again_delay = 0;
        param->integration_time_apply_delay = 2;
        param->isp_exposure_channel_delay = 0;
        break;
    default:
        return;
    }

    param->active.width = param->modes_table[mode].resolution.width;
    param->active.height = param->modes_table[mode].resolution.height;

    param->total.width = param->active.width; //HMAX
    param->total.height = param->active.height; // VMAX

    p_ctx->s_fps = param->modes_table[mode].fps >> 8;
    p_ctx->pixel_clock = param->total.width * param->total.height * p_ctx->s_fps;
    p_ctx->vmax = param->total.height;

    p_ctx->max_S = 170;
    p_ctx->max_L = p_ctx->vmax - p_ctx->max_S - 8;

    param->lines_per_second = p_ctx->pixel_clock / param->total.width;
    param->pixels_per_line = param->total.width;
    param->integration_time_min = 8;
    if ( param->modes_table[mode].wdr_mode == WDR_MODE_LINEAR ) {
        param->integration_time_limit = p_ctx->vmax - 8;
        param->integration_time_max = p_ctx->vmax - 8;
    }

    param->sensor_exp_number = param->modes_table[mode].exposures;
    param->mode = mode;
    param->bayer = param->modes_table[mode].bayer;
    p_ctx->wdr_mode = param->modes_table[mode].wdr_mode;
    p_ctx->vmax_adjust = p_ctx->vmax;
    p_ctx->vmax_fps = p_ctx->s_fps;

    LOG( LOG_CRIT, "Mode %d, Setting num: %d, RES:%dx%d\n", mode, setting_num,
                (int)param->active.width, (int)param->active.height );
}

static const sensor_param_t *sensor_get_parameters( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    return (const sensor_param_t *)&p_ctx->param;
}

static void sensor_disable_isp( void *ctx )
{
}

static uint32_t read_register( void *ctx, uint32_t address )
{
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    return acamera_sbus_read_u8( p_sbus, address );
}

static void write_register( void *ctx, uint32_t address, uint32_t data )
{
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    acamera_sbus_write_u8( p_sbus, address, data );
}

static void stop_streaming( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    p_ctx->streaming_flg = 0;

    reset_sensor_bus_counter();
    sensor_iface_disable(p_ctx);
}

static void start_streaming( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    sensor_param_t *param = &p_ctx->param;
    sensor_set_iface(&param->modes_table[param->mode], p_ctx->win_offset, p_ctx);
    p_ctx->streaming_flg = 1;
}

static void sensor_test_pattern( void *ctx, uint8_t mode )
{
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    unsigned char tmp = acamera_sbus_read_u8( p_sbus, 0x5081 );

    tmp = 0x90;

    acamera_sbus_write_u8(p_sbus, 0x5081, tmp);

    return;
}

void sensor_deinit_ov5640( void *ctx )
{
    sensor_context_t *t_ctx = ctx;

    reset_sensor_bus_counter();
    acamera_sbus_deinit(&t_ctx->sbus,  sbus_i2c);

    if (t_ctx != NULL && t_ctx->sbp != NULL)
        gp_pl_am_disable(t_ctx->sbp, "mclk_0");
}

static sensor_context_t *sensor_global_parameter(void* sbp)
{
    // Local sensor data structure
    int ret;
    sensor_bringup_t* sensor_bp = (sensor_bringup_t*) sbp;


    ret = gp_pl_am_enable(sensor_bp, "mclk_0", 24000000);
    if (ret < 0 )
        pr_info("set mclk fail\n");

#if NEED_CONFIG_BSP
    ret = reset_am_enable(sensor_bp, reset_dts_pin_name, config_sensor_idx, 1);
    if (ret < 0 )
        pr_err("set reset fail\n");
#endif

    sensor_ctx.sbp = sbp;

    sensor_ctx.sbus.mask = SBUS_MASK_ADDR_16BITS |
           SBUS_MASK_SAMPLE_8BITS |SBUS_MASK_ADDR_SWAP_BYTES;
    sensor_ctx.sbus.control = I2C_CONTROL_MASK;
    sensor_ctx.sbus.bus = 0;
    sensor_ctx.sbus.device = SENSOR_DEV_ADDRESS;
    acamera_sbus_init(&sensor_ctx.sbus, sbus_i2c);

    sensor_ctx.address = SENSOR_DEV_ADDRESS;
    sensor_ctx.seq_width = 1;
    sensor_ctx.streaming_flg = 0;
    sensor_ctx.again[0] = 0;
    sensor_ctx.again[1] = 0;
    sensor_ctx.again[2] = 0;
    sensor_ctx.again[3] = 0;
    sensor_ctx.again_limit = 2047;
    sensor_ctx.pixel_clock = 184000000;
    sensor_ctx.s_fps = 30;
    sensor_ctx.vmax = 2300;
    sensor_ctx.vmax_fps = sensor_ctx.s_fps;
    sensor_ctx.vmax_adjust = sensor_ctx.vmax;

    sensor_ctx.param.again_accuracy = 1 << LOG2_GAIN_SHIFT;
    sensor_ctx.param.sensor_exp_number = 1;
    sensor_ctx.param.again_log2_max = acamera_log2_fixed_to_fixed( sensor_ctx.again_limit, AGAIN_PRECISION, LOG2_GAIN_SHIFT );
    sensor_ctx.param.dgain_log2_max = 0;
    sensor_ctx.param.integration_time_apply_delay = 2;
    sensor_ctx.param.isp_exposure_channel_delay = 0;
    sensor_ctx.param.modes_table = supported_modes;
    sensor_ctx.param.modes_num = array_size_s( supported_modes );
    sensor_ctx.param.mode = 0;
    sensor_ctx.param.sensor_ctx = &sensor_ctx;
    sensor_ctx.param.isp_context_seq.sequence = p_isp_data;
    sensor_ctx.param.isp_context_seq.seq_num = SENSOR_OV5640_ISP_CONTEXT_SEQ;
    sensor_ctx.param.isp_context_seq.seq_table_max = array_size_s( isp_seq_table );

    memset(&sensor_ctx.win_offset, 0, sizeof(sensor_ctx.win_offset));

    sensor_ctx.cam_isp_path = CAM0_ACT;
    sensor_ctx.cam_fe_path = FRONTEND0_IO;

    return &sensor_ctx;
}

//--------------------Initialization------------------------------------------------------------
void sensor_init_ov5640( void **ctx, sensor_control_t *ctrl, void *sbp )
{
    *ctx = sensor_global_parameter(sbp);

    ctrl->alloc_analog_gain = sensor_alloc_analog_gain;
    ctrl->alloc_digital_gain = sensor_alloc_digital_gain;
    ctrl->alloc_integration_time = sensor_alloc_integration_time;
    ctrl->ir_cut_set= sensor_ir_cut_set;
    ctrl->sensor_update = sensor_update;
    ctrl->set_mode = sensor_set_mode;
    ctrl->get_id = sensor_get_id;
    ctrl->get_parameters = sensor_get_parameters;
    ctrl->disable_sensor_isp = sensor_disable_isp;
    ctrl->read_sensor_register = read_register;
    ctrl->write_sensor_register = write_register;
    ctrl->start_streaming = start_streaming;
    ctrl->stop_streaming = stop_streaming;
    ctrl->sensor_test_pattern = sensor_test_pattern;
    ctrl->vmax_fps = sensor_vmax_fps;

    // Reset sensor during initialization
    sensor_hw_reset_enable();
    system_timer_usleep( 1000 ); // reset at least 1 ms
    sensor_hw_reset_disable();
    system_timer_usleep( 1000 );

    LOG(LOG_ERR, "%s: Success subdev init\n", __func__);
}

int sensor_detect_ov5640( void* sbp)
{
    int ret = 0;
    sensor_ctx.sbp = sbp;
    sensor_bringup_t* sensor_bp = (sensor_bringup_t*) sbp;

    ret = gp_pl_am_enable(sensor_bp, "mclk_0", 24000000);
    if (ret < 0 )
        pr_info("set mclk fail\n");

#if NEED_CONFIG_BSP
    ret = reset_am_enable(sensor_bp, reset_dts_pin_name, config_sensor_idx, 1);
    if (ret < 0 )
        pr_err("set reset fail\n");
#endif
    sensor_ctx.sbus.mask = SBUS_MASK_SAMPLE_8BITS | SBUS_MASK_ADDR_16BITS | SBUS_MASK_ADDR_SWAP_BYTES;
    sensor_ctx.sbus.control = 0;
    sensor_ctx.sbus.bus = 0;
    sensor_ctx.sbus.device = SENSOR_DEV_ADDRESS;
    acamera_sbus_init( &sensor_ctx.sbus, sbus_i2c );

    ret = 0;
    if (sensor_get_id(&sensor_ctx) == 0xFFFF)
        ret = -1;
    else
        pr_info("sensor_detect_ov5640:%d\n", ret);

    acamera_sbus_deinit(&sensor_ctx.sbus,  sbus_i2c);
    gp_pl_am_disable(sensor_bp, "mclk_0");
    return ret;
}
//*************************************************************************************
