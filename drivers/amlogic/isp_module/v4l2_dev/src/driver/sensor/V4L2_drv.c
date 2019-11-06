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

#include "acamera_types.h"
#include "acamera_logger.h"
#include "acamera_sensor_api.h"
#include "acamera_command_api.h"

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-async.h>

#include "soc_sensor.h"

extern void *acamera_camera_v4l2_get_subdev_by_name( const char *name );

#define V4L2_SENSOR_MAXIMUM_PRESETS_NUM 16

static sensor_mode_t supported_modes[V4L2_SENSOR_MAXIMUM_PRESETS_NUM];


typedef struct _sensor_context_t {
    sensor_param_t param;
    struct v4l2_subdev *soc_sensor;
} sensor_context_t;


static sensor_context_t s_ctx[FIRMWARE_CONTEXT_NUMBER];
static int ctx_counter = 0;

static uint32_t get_ctx_num( void *ctx )
{
    uint32_t ret;
    for ( ret = 0; ret < FIRMWARE_CONTEXT_NUMBER; ret++ ) {
        if ( &s_ctx[ret] == ctx ) {
            break;
        }
    }
    return ret;
}


static void sensor_print_params( void *ctx )
{
    sensor_context_t *p_ctx = ctx;

    LOG( LOG_ERR, "SOC SENSOR PARAMETERS" );
    LOG( LOG_ERR, "pixels_per_line: %d", p_ctx->param.pixels_per_line );
    LOG( LOG_ERR, "again_log2_max: %d", p_ctx->param.again_log2_max );
    LOG( LOG_ERR, "dgain_log2_max: %d", p_ctx->param.dgain_log2_max );
    LOG( LOG_ERR, "again_accuracy: %d", p_ctx->param.again_accuracy );
    LOG( LOG_ERR, "integration_time_min: %d", p_ctx->param.integration_time_min );
    LOG( LOG_ERR, "integration_time_max: %d", p_ctx->param.integration_time_max );
    LOG( LOG_ERR, "integration_time_long_max: %d", p_ctx->param.integration_time_long_max );
    LOG( LOG_ERR, "integration_time_limit: %d", p_ctx->param.integration_time_limit );
    LOG( LOG_ERR, "day_light_integration_time_max: %d", p_ctx->param.day_light_integration_time_max );
    LOG( LOG_ERR, "integration_time_apply_delay: %d", p_ctx->param.integration_time_apply_delay );
    LOG( LOG_ERR, "isp_exposure_channel_delay: %d", p_ctx->param.isp_exposure_channel_delay );
    LOG( LOG_ERR, "xoffset: %d", p_ctx->param.xoffset );
    LOG( LOG_ERR, "yoffset: %d", p_ctx->param.yoffset );
    LOG( LOG_ERR, "lines_per_second: %d", p_ctx->param.lines_per_second );
    LOG( LOG_ERR, "sensor_exp_number: %d", p_ctx->param.sensor_exp_number );
    LOG( LOG_ERR, "modes_num: %d", p_ctx->param.modes_num );
    LOG( LOG_ERR, "mode: %d", p_ctx->param.mode );
    int32_t idx = 0;
    for ( idx = 0; idx < p_ctx->param.modes_num; idx++ ) {
        LOG( LOG_ERR, "preset %d", idx );
        LOG( LOG_ERR, "    mode:   %d", p_ctx->param.modes_table[idx].wdr_mode );
        LOG( LOG_ERR, "    width:  %d", p_ctx->param.modes_table[idx].resolution.width );
        LOG( LOG_ERR, "    height: %d", p_ctx->param.modes_table[idx].resolution.height );
        LOG( LOG_ERR, "    fps:    %d", p_ctx->param.modes_table[idx].fps );
        LOG( LOG_ERR, "    exp:    %d", p_ctx->param.modes_table[idx].exposures );
    }
}


static void sensor_update_parameters( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    int32_t rc = 0;
    //int32_t result = 0;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            LOG( LOG_INFO, "Found context num:%d", ctx_num );
            settings.ctx_num = ctx_num;
            // Initial local parameters
            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_EXP_NUMBER, &settings );
            p_ctx->param.sensor_exp_number = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_ANALOG_GAIN_MAX, &settings );
            p_ctx->param.again_log2_max = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_DIGITAL_GAIN_MAX, &settings );
            p_ctx->param.dgain_log2_max = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_UPDATE_LATENCY, &settings );
            p_ctx->param.integration_time_apply_delay = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_INTEGRATION_TIME_MIN, &settings );
            p_ctx->param.integration_time_min = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_INTEGRATION_TIME_MAX, &settings );
            p_ctx->param.integration_time_max = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_INTEGRATION_TIME_LONG_MAX, &settings );
            p_ctx->param.integration_time_long_max = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_INTEGRATION_TIME_LIMIT, &settings );
            p_ctx->param.integration_time_limit = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_LINES_PER_SECOND, &settings );
            p_ctx->param.lines_per_second = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_ACTIVE_HEIGHT, &settings );
            p_ctx->param.active.height = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_ACTIVE_WIDTH, &settings );
            p_ctx->param.active.width = settings.args.general.val_out;

            p_ctx->param.isp_exposure_channel_delay = 0;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_PRESET_NUM, &settings );
            p_ctx->param.modes_num = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_PRESET_CUR, &settings );
            p_ctx->param.mode = settings.args.general.val_out;

            rc = v4l2_subdev_call(sd, core, ioctl, SOC_SENSOR_GET_BAYER_PATTERN, &settings);
            p_ctx->param.bayer = settings.args.general.val_out;

            rc = v4l2_subdev_call(sd, core, ioctl, SOC_SENSOR_GET_SENSOR_NAME, &settings);
            memcpy(p_ctx->param.s_name.name, settings.s_name.name, settings.s_name.name_len);
            p_ctx->param.s_name.name_len = settings.s_name.name_len;

            rc = v4l2_subdev_call(sd, core, ioctl, SOC_SENSOR_GET_CONTEXT_SEQ, &settings);
            p_ctx->param.isp_context_seq.seq_num = settings.isp_context_seq.seq_num;
            p_ctx->param.isp_context_seq.sequence = settings.isp_context_seq.sequence;

            if ( p_ctx->param.modes_num > V4L2_SENSOR_MAXIMUM_PRESETS_NUM ) {
                p_ctx->param.modes_num = V4L2_SENSOR_MAXIMUM_PRESETS_NUM;
                LOG( LOG_WARNING, "Exceed maximum supported presets. Sensor driver returned %d but maximum is %d", p_ctx->param.modes_num, V4L2_SENSOR_MAXIMUM_PRESETS_NUM );
            }


            p_ctx->param.modes_table = supported_modes;
            int32_t idx = 0;
            for ( idx = 0; idx < p_ctx->param.modes_num; idx++ ) {
                settings.args.general.val_in = idx;
                rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_PRESET_WIDTH, &settings );
                p_ctx->param.modes_table[idx].resolution.width = settings.args.general.val_out;

                rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_PRESET_HEIGHT, &settings );
                p_ctx->param.modes_table[idx].resolution.height = settings.args.general.val_out;

                rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_PRESET_FPS, &settings );
                p_ctx->param.modes_table[idx].fps = settings.args.general.val_out;

                rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_PRESET_EXP, &settings );
                p_ctx->param.modes_table[idx].exposures = settings.args.general.val_out;

                rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_PRESET_MODE, &settings );
                p_ctx->param.modes_table[idx].wdr_mode = settings.args.general.val_out;

                rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_GET_SENSOR_BITS, &settings );
                p_ctx->param.modes_table[idx].bits = settings.args.general.val_out;
            }

            //p_ctx->param.modes_table = supported_modes;

            p_ctx->param.sensor_ctx = p_ctx;

        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
        sensor_print_params( p_ctx );
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
}


static int32_t sensor_alloc_analog_gain( void *ctx, int32_t gain )
{
    sensor_context_t *p_ctx = ctx;
    int32_t result = 0;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            settings.args.general.val_in = gain;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_ALLOC_AGAIN, &settings );
            if ( rc == 0 ) {
                result = settings.args.general.val_out;
            } else {
                LOG( LOG_ERR, "Failed to alloc again. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
    return result;
}


static int32_t sensor_alloc_digital_gain( void *ctx, int32_t gain )
{
    sensor_context_t *p_ctx = ctx;
    int32_t result = 0;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            settings.args.general.val_in = gain;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_ALLOC_DGAIN, &settings );
            if ( rc == 0 ) {
                result = settings.args.general.val_out;
            } else {
                LOG( LOG_ERR, "Failed to digital again. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
    return result;
}


static void sensor_alloc_integration_time( void *ctx, uint16_t *int_time, uint16_t *int_time_M, uint16_t *int_time_L )
{
    sensor_context_t *p_ctx = ctx;
    //int32_t result = 0;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            settings.args.integration_time.it_short = *int_time;
            settings.args.integration_time.it_medium = *int_time_M;
            settings.args.integration_time.it_long = *int_time_L;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_ALLOC_IT, &settings );
            if ( rc == 0 ) {
                *int_time = settings.args.integration_time.it_short;
                *int_time_M = settings.args.integration_time.it_medium;
                *int_time_L = settings.args.integration_time.it_long;
            } else {
                LOG( LOG_ERR, "Failed to alloc integration time. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
    return;
}

static int32_t sensor_ir_cut_set( void *ctx, int32_t ir_cut_state )
{
    sensor_context_t *p_ctx = ctx;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            settings.args.general.val_in = ir_cut_state;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_IR_CUT_SET, &settings );
            if ( rc != 0 ) {
                LOG( LOG_ERR, "Failed to update sensor exposure. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
    return 0;
}

static void sensor_update( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_UPDATE_EXP, &settings );
            if ( rc != 0 ) {
                LOG( LOG_ERR, "Failed to update sensor exposure. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
}

static void sensor_test_pattern( void *ctx, uint8_t mode )
{
    sensor_context_t *p_ctx = ctx;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            settings.args.general.val_in = mode;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_SET_TEST_PATTERN, &settings );
            if ( rc != 0 ) {
                LOG( LOG_ERR, "Failed to update sensor exposure. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
}


static uint16_t sensor_get_id( void *ctx )
{
    return 0;
}


static void sensor_set_mode( void *ctx, uint8_t mode )
{
    sensor_context_t *p_ctx = ctx;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            settings.args.general.val_in = mode;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_SET_PRESET, &settings );
            if ( rc == 0 ) {
                sensor_update_parameters( p_ctx );
            } else {
                LOG( LOG_CRIT, "Failed to set mode. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
}


static const sensor_param_t *sensor_get_parameters( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    return (const sensor_param_t *)&p_ctx->param;
}


static void sensor_disable_isp( void *ctx )
{
}

/*
static void sensor_get_lines_per_second( void *ctx )
{
}
*/

static uint32_t read_register( void *ctx, uint32_t address )
{
    sensor_context_t *p_ctx = ctx;
    int32_t result = 0;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            settings.args.general.val_in = address;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_READ_REG, &settings );
            if ( rc == 0 ) {
                result = settings.args.general.val_out;
            } else {
                LOG( LOG_ERR, "Failed to read register. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
    return result;
}

static void write_register( void *ctx, uint32_t address, uint32_t data )
{
    sensor_context_t *p_ctx = ctx;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            settings.args.general.val_in = address;
            settings.args.general.val_in2 = data;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_WRITE_REG, &settings );
            if ( rc == 0 ) {
            } else {
                LOG( LOG_ERR, "Failed to write register. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
}


static void stop_streaming( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_STREAMING_OFF, &settings );
            if ( rc != 0 ) {
                LOG( LOG_ERR, "Failed to stop streaming. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
}


static void start_streaming( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    if ( p_ctx != NULL ) {
        struct soc_sensor_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_sensor;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_SENSOR_STREAMING_ON, &settings );
            if ( rc != 0 ) {
                LOG( LOG_ERR, "Failed to start streaming. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC sensor subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
}

void sensor_deinit_v4l2( void *ctx )
{
    sensor_context_t *p_ctx = ctx;
    if ( p_ctx != NULL ) {
        uint32_t ctx_num = get_ctx_num( ctx );
        int rc = v4l2_subdev_call( p_ctx->soc_sensor, core, reset, ctx_num );
        if ( --ctx_counter < 0 )
            LOG( LOG_CRIT, "Sensor context pointer is negative %d", ctx_counter );
        if ( rc != 0 ) {
            LOG( LOG_CRIT, "Failed to reset soc_sensor. core->reset failed with rc %d", rc );
            return;
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
}

//--------------------Initialization------------------------------------------------------------
void sensor_init_v4l2( void **ctx, sensor_control_t *ctrl )
{
    //struct soc_sensor_ioctl_args settings;
    int rc = 0;

    // Local sensor data structure
    if ( ctx_counter < FIRMWARE_CONTEXT_NUMBER ) {
        sensor_context_t *p_ctx = &s_ctx[ctx_counter];

        ctrl->alloc_analog_gain = sensor_alloc_analog_gain;
        ctrl->alloc_digital_gain = sensor_alloc_digital_gain;
        ctrl->alloc_integration_time = sensor_alloc_integration_time;
        ctrl->sensor_update = sensor_update;
        ctrl->sensor_test_pattern = sensor_test_pattern;
        ctrl->set_mode = sensor_set_mode;
        ctrl->get_id = sensor_get_id;
        ctrl->get_parameters = sensor_get_parameters;
        ctrl->disable_sensor_isp = sensor_disable_isp;
        ctrl->read_sensor_register = read_register;
        ctrl->write_sensor_register = write_register;
        ctrl->start_streaming = start_streaming;
        ctrl->stop_streaming = stop_streaming;
        ctrl->ir_cut_set = sensor_ir_cut_set;

        p_ctx->param.modes_table = supported_modes;
        p_ctx->param.modes_num = 0;

        *ctx = p_ctx;


        p_ctx->soc_sensor = acamera_camera_v4l2_get_subdev_by_name( V4L2_SOC_SENSOR_NAME );
        if ( p_ctx->soc_sensor == NULL ) {
            LOG( LOG_CRIT, "Sensor bridge cannot be initialized before soc_sensor subdev is available. Pointer is null" );
            return;
        }

        rc = v4l2_subdev_call( p_ctx->soc_sensor, core, init, ctx_counter );
        if ( rc != 0 ) {
            LOG( LOG_CRIT, "Failed to initialize soc_sensor. core->init failed with rc %d", rc );
            return;
        }

        sensor_update_parameters( p_ctx );

        ctx_counter++;


    } else {
        LOG( LOG_ERR, "Attempt to initialize more sensor instances than was configured. Sensor initialization failed." );
        *ctx = NULL;
    }
}
