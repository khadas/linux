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
#include "OV08a10_seq.h"
#include "OV08a10_config.h"
#include "system_am_mipi.h"
#include "system_am_adap.h"
#include "sensor_bsp_common.h"

#define AGAIN_PRECISION 7
#define NEED_CONFIG_BSP 1   //config bsp by sensor driver owner

#define FS_LIN_1080P 1
static int sen_mode = 0;
static void start_streaming( void *ctx );
static void stop_streaming( void *ctx );

static sensor_mode_t supported_modes[] = {
    {
        .wdr_mode = WDR_MODE_LINEAR,
        .fps = 5 * 256,
        .resolution.width = 1280,
        .resolution.height = 720,
        .bits = 10,
        .exposures = 1,
        .lanes = 2,
        .bps = 250,
        .bayer = BAYER_BGGR,
        .dol_type = DOL_NON,
        .num = 1,
    },
    {
        .wdr_mode = WDR_MODE_LINEAR,
        .fps = 30 * 256,
        .resolution.width = 1920,
        .resolution.height = 1080,
        .bits = 10,
        .exposures = 1,
        .lanes = 4,
        .bps = 800,
        .bayer = BAYER_BGGR,
        .dol_type = DOL_NON,
        .num = 2,
    },
   {
        .wdr_mode = WDR_MODE_LINEAR,
        .fps = 60 * 256,
        .resolution.width = 1920,
        .resolution.height = 1080,
        .bits = 10,
        .exposures = 2,
        .lanes = 4,
        .bps = 1440,
        .bayer = BAYER_BGGR,
        .dol_type = DOL_NON,
        .num = 3,
    },
    {
        .wdr_mode = WDR_MODE_LINEAR,
        .fps = 30 * 256,
        .resolution.width = 3840,
        .resolution.height = 2160,
        .bits = 10,
        .exposures = 2,
        .lanes = 4,
        .bps = 800,
        .bayer = BAYER_BGGR,
        .dol_type = DOL_NON,
        .num = 4,
    },
    {
        .wdr_mode = WDR_MODE_LINEAR,
        .fps = 60 * 256,
        .resolution.width = 3840,
        .resolution.height = 2160,
        .bits = 10,
        .exposures = 2,
        .lanes = 4,
        .bps = 1440,
        .bayer = BAYER_BGGR,
        .dol_type = DOL_NON,
        .num = 5,
    },
    {
        .wdr_mode = WDR_MODE_FS_LIN,
        .fps = 60 * 256,
        .resolution.width = 1920,
        .resolution.height = 1080,
        .bits = 10,
        .exposures = 2,
        .lanes = 4,
        .bps = 960,
        .bayer = BAYER_BGGR,
        .dol_type = DOL_VC,
        .num = 6,
    },
};

typedef struct _sensor_context_t {
    uint8_t address; // Sensor address for direct write (not used currently)
    uint8_t seq_width;
    uint8_t streaming_flg;
    uint16_t again[4];
    uint8_t again_delay;
    uint16_t int_time_S;
    uint16_t int_time_M;
    uint16_t int_time_L;
    uint32_t shs1;
    uint32_t shs2;
    uint32_t shs3;
    uint32_t shs1_old;
    uint32_t shs2_old;
    uint32_t rhs1;
    uint32_t rhs2;
    uint32_t again_limit;
    uint8_t s_fps;
    uint32_t vmax;
    uint8_t int_cnt;
    uint8_t gain_cnt;
    uint32_t pixel_clock;
    uint16_t max_S;
    uint16_t max_M;
    uint16_t max_L;
    uint16_t frame;
    uint32_t wdr_mode;
    acamera_sbus_t sbus;
    sensor_param_t param;
    void *sbp;
} sensor_context_t;

#if SENSOR_BINARY_SEQUENCE
static const char p_sensor_data[] = SENSOR__OV08A10_SEQUENCE_DEFAULT;
static const char p_isp_data[] = SENSOR__OV08A10_ISP_SEQUENCE_DEFAULT;
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
    sensor_context_t *p_ctx = ctx;

    uint32_t again = acamera_math_exp2( gain, LOG2_GAIN_SHIFT, AGAIN_PRECISION );

    if ( again > p_ctx->again_limit ) again = p_ctx->again_limit;

    if ( p_ctx->again[0] != again ) {
        p_ctx->gain_cnt = p_ctx->again_delay + 1;
        p_ctx->again[0] = again;
    }

    return acamera_log2_fixed_to_fixed( again, AGAIN_PRECISION, LOG2_GAIN_SHIFT );
}

static int32_t sensor_alloc_digital_gain( void *ctx, int32_t gain )
{
    return 0;
}

static void sensor_alloc_integration_time( void *ctx, uint16_t *int_time_S, uint16_t *int_time_M, uint16_t *int_time_L )
{
    sensor_context_t *p_ctx = ctx;

    switch ( p_ctx->wdr_mode ) {
    case WDR_MODE_LINEAR: // Normal mode
        if ( *int_time_S > p_ctx->vmax - 8 ) *int_time_S = p_ctx->vmax - 8;
        if ( *int_time_S < 1 ) *int_time_S = 1;
        if ( p_ctx->int_time_S != *int_time_S ) {
            p_ctx->int_cnt = 2;
            p_ctx->int_time_S = *int_time_S;
        }
        break;
    case WDR_MODE_FS_LIN: // DOL3 Frames
#ifdef FS_LIN_3DOL
        if ( *int_time_S < 2 ) *int_time_S = 2;
        if ( *int_time_S > p_ctx->max_S ) *int_time_S = p_ctx->max_S;
        if ( *int_time_L < 2 ) *int_time_L = 2;
        if ( *int_time_L > p_ctx->max_L ) *int_time_L = p_ctx->max_L;

        if ( *int_time_M < 2 ) *int_time_M = 2;
        if ( *int_time_M > p_ctx->max_M ) *int_time_M = p_ctx->max_M;

        if ( p_ctx->int_time_S != *int_time_S || p_ctx->int_time_M != *int_time_M || p_ctx->int_time_L != *int_time_L ) {
            p_ctx->int_cnt = 3;

            p_ctx->int_time_S = *int_time_S;
            p_ctx->int_time_M = *int_time_M;
            p_ctx->int_time_L = *int_time_L;

            p_ctx->shs3 = p_ctx->frame - *int_time_L - 1;
            p_ctx->shs1 = p_ctx->rhs1 - *int_time_M - 1;
            p_ctx->shs2 = p_ctx->rhs2 - *int_time_S - 1;
        }
#else  //default DOL2 Frames
        if ( *int_time_S < 2 ) *int_time_S = 2;
        if ( *int_time_S > p_ctx->max_S ) *int_time_S = p_ctx->max_S;
        if ( *int_time_L < 2 ) *int_time_L = 2;
        if ( *int_time_L > p_ctx->max_L ) *int_time_L = p_ctx->max_L;

        if ( p_ctx->int_time_S != *int_time_S || p_ctx->int_time_L != *int_time_L ) {
            p_ctx->int_cnt = 2;

            p_ctx->int_time_S = *int_time_S;
            p_ctx->int_time_L = *int_time_L;

            p_ctx->shs2 = p_ctx->frame - *int_time_L - 1;
            p_ctx->shs1 = p_ctx->rhs1 - *int_time_S - 1;
        }
#endif
        break;
    }
}

static int32_t sensor_ir_cut_set( void *ctx, int32_t ir_cut_state )
{
    sensor_context_t *t_ctx = ctx;
    int ret;
    sensor_bringup_t* sensor_bp = t_ctx->sbp;

    LOG( LOG_ERR, "ir_cut_state = %d", ir_cut_state);
    LOG( LOG_INFO, "entry ir cut" );

//ir_cut, 0: close ir cut, 1: open ir cut, 2: no operation

   if (sensor_bp->ir_gname[0] <= 0) {
       pr_err("get gpio id fail\n");
       return 0;
    }

    if (ir_cut_state == 1)
        {
            ret = pwr_ir_cut_enable(sensor_bp, sensor_bp->ir_gname[0], 1);
            if (ret < 0 )
            pr_err("set power fail\n");
        }
    else if(ir_cut_state == 0)
        {
            ret = pwr_ir_cut_enable(sensor_bp, sensor_bp->ir_gname[0], 0);
            if (ret < 0 )
            pr_err("set power fail\n");
        }
    else if(ir_cut_state == 2)
        return 0;
    else
        LOG( LOG_ERR, "sensor ir cut set failed" );

    LOG( LOG_INFO, "exit ir cut" );
    return 0;
}

static void sensor_update( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;

    if ( p_ctx->int_cnt || p_ctx->gain_cnt ) {
        // ---------- Start Changes -------------
        //acamera_sbus_write_u8( p_sbus, 0x0201, 1 );

        // ---------- Analog Gain -------------
        if ( p_ctx->gain_cnt ) {
            acamera_sbus_write_u8( p_sbus, 0x3508, (p_ctx->again[p_ctx->again_delay]>> 8 ) & 0xFF );
            acamera_sbus_write_u8( p_sbus, 0x3509, (p_ctx->again[p_ctx->again_delay]>> 0 ) & 0xFF );
            p_ctx->gain_cnt--;
        }

        // -------- Integration Time ----------
        if ( p_ctx->int_cnt ) {
            switch ( p_ctx->wdr_mode ) {
            case WDR_MODE_LINEAR:
                acamera_sbus_write_u8( p_sbus, 0x3501, ( p_ctx->int_time_S >> 8 ) & 0xFF );
                acamera_sbus_write_u8( p_sbus, 0x3502, ( p_ctx->int_time_S >> 0 ) & 0xFF );
                break;
            case WDR_MODE_FS_LIN:
                p_ctx->shs2_old = p_ctx->shs2;
                p_ctx->shs1_old = p_ctx->shs1;
#ifdef FS_LIN_3DOL
                // SHS3
                acamera_sbus_write_u8( p_sbus, 0x0229, ( p_ctx->shs3 >> 8 ) & 0xFF );
                acamera_sbus_write_u8( p_sbus, 0x0228, ( p_ctx->shs3 >> 0 ) & 0xFF );
#endif
                // SHS1
                acamera_sbus_write_u8( p_sbus, 0x3511, ( p_ctx->shs1_old >> 8 ) & 0xFF );
                acamera_sbus_write_u8( p_sbus, 0x3512, ( p_ctx->shs1_old >> 0 ) & 0xFF );

                // SHS2
                acamera_sbus_write_u8( p_sbus, 0x3501, ( p_ctx->shs2_old >> 8 ) & 0xFF );
                acamera_sbus_write_u8( p_sbus, 0x3502, ( p_ctx->shs2_old >> 0 ) & 0xFF );
                break;
            }
            p_ctx->int_cnt--;
        }

        // ---------- End Changes -------------
        //acamera_sbus_write_u8( p_sbus, 0x0201, 0 );
    }
    p_ctx->shs1_old = p_ctx->shs1;
    p_ctx->shs2_old = p_ctx->shs2;
    p_ctx->again[3] = p_ctx->again[2];
    p_ctx->again[2] = p_ctx->again[1];
    p_ctx->again[1] = p_ctx->again[0];
}

static uint16_t sensor_get_id( void *ctx )
{
    /* return that sensor id register does not exist */

    sensor_context_t *p_ctx = ctx;
    uint32_t sensor_id = 0;

    sensor_id |= acamera_sbus_read_u8(&p_ctx->sbus, 0x300a) << 16;
    sensor_id |= acamera_sbus_read_u8(&p_ctx->sbus, 0x300b) << 8;
    sensor_id |= acamera_sbus_read_u8(&p_ctx->sbus, 0x300c);

    if (sensor_id != SENSOR_CHIP_ID) {
        LOG(LOG_ERR, "%s: Failed to read sensor id\n", __func__);
        return 0xFF;
    }
    return 0;
}

static void sensor_set_iface(sensor_mode_t *mode)
{
    am_mipi_info_t mipi_info;
    struct am_adap_info info;

    if (mode == NULL) {
        LOG(LOG_ERR, "Error input param\n");
        return;
    }

    memset(&mipi_info, 0, sizeof(mipi_info));
    memset(&info, 0, sizeof(struct am_adap_info));
    mipi_info.fte1_flag = get_fte1_flag();
    mipi_info.lanes = mode->lanes;
    mipi_info.ui_val = 1000 / mode->bps;

    if ((1000 % mode->bps) != 0)
        mipi_info.ui_val += 1;

    am_mipi_init(&mipi_info);

    switch (mode->bits) {
    case 10:
        info.fmt = AM_RAW10;
        break;
    case 12:
        info.fmt = AM_RAW12;
        break;
    default :
        info.fmt = AM_RAW10;
        break;
    }

    info.img.width = mode->resolution.width;
    info.img.height = mode->resolution.height;
    info.path = PATH0;
    if (mode->wdr_mode == WDR_MODE_FS_LIN) {
        info.mode = DOL_MODE;
        info.type = mode->dol_type;
    } else
        info.mode = DIR_MODE;
    am_adap_set_info(&info);
    am_adap_init();
    am_adap_start(0);
}


static void sensor_set_mode( void *ctx, uint8_t mode )
{
    sensor_context_t *p_ctx = ctx;
    sensor_param_t *param = &p_ctx->param;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    uint8_t setting_num = 0;
    uint16_t s_id = 0xff;
    sen_mode = mode;

    sensor_hw_reset_enable();
    system_timer_usleep( 10000 );
    sensor_hw_reset_disable();
    system_timer_usleep( 10000 );

    setting_num = param->modes_table[mode].num;

    s_id = sensor_get_id(ctx);
    if (s_id != 0)
        return;

    switch ( param->modes_table[mode].wdr_mode ) {
    case WDR_MODE_LINEAR:
        sensor_load_sequence( p_sbus, p_ctx->seq_width, p_sensor_data, setting_num);
        p_ctx->s_fps = param->modes_table[mode].fps;
        p_ctx->again_delay = 0;
        param->integration_time_apply_delay = 2;
        param->isp_exposure_channel_delay = 0;
        break;
    case WDR_MODE_FS_LIN:
        p_ctx->again_delay = 0;
        param->integration_time_apply_delay = 2;
        param->isp_exposure_channel_delay = 0;

        if ( param->modes_table[mode].exposures == 2 ) {
            sensor_load_sequence( p_sbus, p_ctx->seq_width, p_sensor_data, setting_num);
        } else {

        }
        break;
    default:
        return;
        break;
    }

    if ( param->modes_table[mode].fps == 25 * 256 ) {
        acamera_sbus_write_u8( p_sbus, 0x3018, 0x46 );
        acamera_sbus_write_u8( p_sbus, 0x3019, 0x05 );
        p_ctx->s_fps = 25;
        p_ctx->vmax = 1350;
    } else if ((param->modes_table[mode].exposures == 2) && (param->modes_table[mode].fps == 30 * 256)) {
        p_ctx->s_fps = 30;
        p_ctx->vmax = 1220;
    } else if ((param->modes_table[mode].exposures == 2) && (param->modes_table[mode].fps == 60 * 256)) {
        p_ctx->s_fps = 60;
        p_ctx->vmax = (((uint32_t)acamera_sbus_read_u8(p_sbus,0x380e)<<8)|acamera_sbus_read_u8(p_sbus,0x380f)) - 4;
        p_ctx->max_S = 100;
        p_ctx->rhs1 = 100;
        p_ctx->max_L = p_ctx->vmax - 2;
    } else {
        p_ctx->vmax = (((uint32_t)acamera_sbus_read_u8(p_sbus,0x380e)<<8)|acamera_sbus_read_u8(p_sbus,0x380f)) - 8;
    }
    param->active.width = param->modes_table[mode].resolution.width;
    param->active.height = param->modes_table[mode].resolution.height;

    param->total.width = ( (uint16_t)acamera_sbus_read_u8( p_sbus, 0x380c ) << 8 ) | acamera_sbus_read_u8( p_sbus, 0x380d );
    param->lines_per_second = p_ctx->pixel_clock / param->total.width;
    param->total.height = (uint16_t)p_ctx->vmax;

    param->pixels_per_line = param->total.width;
    param->integration_time_min = SENSOR_MIN_INTEGRATION_TIME;
    if ( param->modes_table[mode].wdr_mode == WDR_MODE_LINEAR ) {
        param->integration_time_limit = p_ctx->vmax - 8;
        param->integration_time_max = p_ctx->vmax - 8;
    } else {
        param->integration_time_limit = p_ctx->max_S;
        param->integration_time_max = p_ctx->max_S;
        if ( param->modes_table[mode].exposures == 2 ) {
            param->integration_time_long_max = (p_ctx->vmax << 1 ) - 256;
            param->lines_per_second = param->lines_per_second >> 1;
            p_ctx->frame = p_ctx->vmax << 1;
        } else {
            param->integration_time_long_max = ( p_ctx->vmax << 2 ) - 256;
            param->lines_per_second = param->lines_per_second >> 2;
            p_ctx->frame = p_ctx->vmax << 2;
        }
    }
    param->sensor_exp_number = param->modes_table[mode].exposures;
    param->mode = mode;
    p_ctx->wdr_mode = param->modes_table[mode].wdr_mode;
    param->bayer = param->modes_table[mode].bayer;

    sensor_set_iface(&param->modes_table[mode]);

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
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    p_ctx->streaming_flg = 0;

    acamera_sbus_write_u8(p_sbus, 0x0100, 0x00);

    reset_sensor_bus_counter();
    am_adap_deinit();
    am_mipi_deinit();
}

static void start_streaming( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    acamera_sbus_ptr_t p_sbus = &p_ctx->sbus;
    sensor_param_t *param = &p_ctx->param;
    sensor_set_iface(&param->modes_table[sen_mode]);
    p_ctx->streaming_flg = 1;
    acamera_sbus_write_u8(p_sbus, 0x0100, 0x01);

}

static void sensor_test_pattern( void *ctx, uint8_t mode )
{

}

void sensor_deinit_ov08a10( void *ctx )
{
    sensor_context_t *t_ctx = ctx;

    reset_sensor_bus_counter();
    am_adap_deinit();
    am_mipi_deinit();

    acamera_sbus_deinit(&t_ctx->sbus,  sbus_i2c);

    if (t_ctx != NULL && t_ctx->sbp != NULL)
        clk_am_disable(t_ctx->sbp);
}
//--------------------Initialization------------------------------------------------------------
void sensor_init_ov08a10( void **ctx, sensor_control_t *ctrl, void *sbp )
{
    // Local sensor data structure
    static sensor_context_t s_ctx;
    int ret;
    sensor_bringup_t* sensor_bp = (sensor_bringup_t*) sbp;
#if NEED_CONFIG_BSP
    ret = pwr_am_enable(sensor_bp, "power-enable", 0);
    if (ret < 0 )
        pr_err("set power fail\n");
    udelay(30);
#endif

    ret = clk_am_enable(sensor_bp, "gen_clk");
    if (ret < 0 )
        pr_err("set mclk fail\n");
    udelay(30);

#if NEED_CONFIG_BSP
    ret = reset_am_enable(sensor_bp,"reset", 1);
    if (ret < 0 )
        pr_err("set reset fail\n");
#endif

    s_ctx.sbp = sbp;

    *ctx = &s_ctx;

    s_ctx.sbus.mask = SBUS_MASK_ADDR_16BITS |
           SBUS_MASK_SAMPLE_8BITS |SBUS_MASK_ADDR_SWAP_BYTES;
    s_ctx.sbus.control = I2C_CONTROL_MASK;
    s_ctx.sbus.bus = 1;//get_next_sensor_bus_address();
    s_ctx.sbus.device = SENSOR_DEV_ADDRESS;
    acamera_sbus_init(&s_ctx.sbus, sbus_i2c);

    sensor_get_id(&s_ctx);

    s_ctx.address = SENSOR_DEV_ADDRESS;
    s_ctx.seq_width = 1;
    s_ctx.streaming_flg = 0;
    s_ctx.again[0] = 0;
    s_ctx.again[1] = 0;
    s_ctx.again[2] = 0;
    s_ctx.again[3] = 0;
    s_ctx.again_limit = 2047;
    s_ctx.pixel_clock = 184000000;

    s_ctx.param.again_accuracy = 1 << LOG2_GAIN_SHIFT;
    s_ctx.param.sensor_exp_number = 1;
    s_ctx.param.again_log2_max = 8 << LOG2_GAIN_SHIFT;
    s_ctx.param.dgain_log2_max = 0;
    s_ctx.param.integration_time_apply_delay = 2;
    s_ctx.param.isp_exposure_channel_delay = 0;
    s_ctx.param.modes_table = supported_modes;
    s_ctx.param.modes_num = array_size( supported_modes );
    s_ctx.param.mode = 0;
    s_ctx.param.sensor_ctx = &s_ctx;
    s_ctx.param.isp_context_seq.sequence = p_isp_data;
    s_ctx.param.isp_context_seq.seq_num = SENSOR_OV08A10_ISP_CONTEXT_SEQ;
    s_ctx.param.isp_context_seq.seq_table_max = array_size( isp_seq_table );

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

    // Reset sensor during initialization
    sensor_hw_reset_enable();
    system_timer_usleep( 1000 ); // reset at least 1 ms
    sensor_hw_reset_disable();
    system_timer_usleep( 1000 );

    LOG(LOG_ERR, "%s: Success subdev init\n", __func__);
}

//*************************************************************************************
