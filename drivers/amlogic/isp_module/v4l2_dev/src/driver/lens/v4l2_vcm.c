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

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-async.h>

#include "acamera_lens_api.h"
#include "acamera_firmware_config.h"
#include "acamera_logger.h"

#include "soc_lens.h"

extern void *acamera_camera_v4l2_get_subdev_by_name( const char *name );

typedef struct _lens_context_t {
    lens_param_t param;
    struct v4l2_subdev *soc_lens;
} lens_context_t;


static lens_context_t l_ctx[FIRMWARE_CONTEXT_NUMBER];
static int ctx_counter = 0;


static uint32_t get_ctx_num( void *ctx )
{
    uint32_t ret;
    for ( ret = 0; ret < FIRMWARE_CONTEXT_NUMBER; ret++ ) {
        if ( &l_ctx[ret] == ctx ) {
            break;
        }
    }
    return ret;
}

static void lens_print_params( void *ctx )
{
    lens_context_t *p_ctx = ctx;

    LOG( LOG_INFO, "SOC SENSOR PARAMETERS" );
    LOG( LOG_INFO, "lens_type: %d", p_ctx->param.lens_type );
    LOG( LOG_INFO, "min_step: %d", p_ctx->param.min_step );
    LOG( LOG_INFO, "next_zoom: %d", p_ctx->param.next_zoom );
    LOG( LOG_INFO, "curr_zoom: %d", p_ctx->param.curr_zoom );
    LOG( LOG_INFO, "next_pos: %d", p_ctx->param.next_pos );
}


static void lens_update_parameters( void *ctx )
{
    lens_context_t *p_ctx = ctx;
    int32_t rc = 0;
    if ( p_ctx != NULL ) {
        struct soc_lens_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_lens;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            //LOG(LOG_INFO,"Found context num:%d",ctx_num);
            settings.ctx_num = ctx_num;
            // Initial local parameters
            rc = v4l2_subdev_call( sd, core, ioctl, SOC_LENS_GET_LENS_TYPE, &settings );
            p_ctx->param.lens_type = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_LENS_GET_MIN_STEP, &settings );
            p_ctx->param.min_step = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_LENS_GET_NEXT_ZOOM, &settings );
            p_ctx->param.next_zoom = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_LENS_GET_CURR_ZOOM, &settings );
            p_ctx->param.curr_zoom = settings.args.general.val_out;

            rc = v4l2_subdev_call( sd, core, ioctl, SOC_LENS_GET_NEXT_POS, &settings );
            p_ctx->param.next_pos = settings.args.general.val_out;

        } else {
            LOG( LOG_CRIT, "SOC lens subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Sensor context pointer is NULL" );
    }
}


uint8_t lens_v4l2_subdev_test( uint32_t lens_bus )
{
    uint8_t result = 0;
    void *ctx = acamera_camera_v4l2_get_subdev_by_name( V4L2_SOC_LENS_NAME );
    if ( ctx != NULL ) {
        result = 1;
    } else {
        result = 0;
        LOG( LOG_WARNING, "Attempt to request V4L2 lens subdevice. Returned pointer is null" );
    }
    return result;
}


static void vcm_v4l2_subdev_move( void *ctx, uint16_t position )
{
    lens_context_t *p_ctx = ctx;

    if ( p_ctx != NULL ) {
        struct soc_lens_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_lens;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            settings.args.general.val_in = position;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_LENS_MOVE, &settings );
            if ( rc == 0 ) {
            } else {
                LOG( LOG_ERR, "Failed to move the lens. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC lens subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Lens context pointer is NULL" );
    }
    return;
}


static uint8_t vcm_v4l2_subdev_is_moving( void *ctx )
{
    lens_context_t *p_ctx = ctx;
    int8_t result = 0;
    if ( p_ctx != NULL ) {
        struct soc_lens_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_lens;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_LENS_IS_MOVING, &settings );
            if ( rc == 0 ) {
                result = settings.args.general.val_out;
            } else {
                LOG( LOG_ERR, "Failed to move the lens. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC lens subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Lens context pointer is NULL" );
    }
    return result;
}


static void vcm_v4l2_subdev_write_register( void *ctx, uint32_t address, uint32_t data )
{
    lens_context_t *p_ctx = ctx;

    if ( p_ctx != NULL ) {
        struct soc_lens_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_lens;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            settings.args.general.val_in = address;
            settings.args.general.val_in2 = data;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_LENS_WRITE_REG, &settings );
            if ( rc == 0 ) {
            } else {
                LOG( LOG_ERR, "Failed to write the lens register. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC lens subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Lens context pointer is NULL" );
    }
    return;
}


static uint32_t vcm_v4l2_subdev_read_register( void *ctx, uint32_t address )
{
    lens_context_t *p_ctx = ctx;
    int32_t result = 0;
    if ( p_ctx != NULL ) {
        struct soc_lens_ioctl_args settings;
        struct v4l2_subdev *sd = p_ctx->soc_lens;
        uint32_t ctx_num = get_ctx_num( ctx );
        if ( sd != NULL && ctx_num < FIRMWARE_CONTEXT_NUMBER ) {
            settings.ctx_num = ctx_num;
            settings.args.general.val_in = address;
            int rc = v4l2_subdev_call( sd, core, ioctl, SOC_LENS_READ_REG, &settings );
            if ( rc == 0 ) {
                result = settings.args.general.val_out;
            } else {
                LOG( LOG_ERR, "Failed to read the lens register. rc = %d", rc );
            }
        } else {
            LOG( LOG_CRIT, "SOC lens subdev pointer is NULL" );
        }
    } else {
        LOG( LOG_CRIT, "Lens context pointer is NULL" );
    }
    return result;
}


static const lens_param_t *lens_get_parameters( void *ctx )
{
    lens_context_t *p_ctx = ctx;
    lens_update_parameters( p_ctx );

    return (const lens_param_t *)&p_ctx->param;
}

void lens_v4l2_subdev_deinit( void *ctx )
{
    lens_context_t *p_ctx = ctx;
    if ( p_ctx != NULL ) {
        uint32_t ctx_num = get_ctx_num( ctx );
        int rc = v4l2_subdev_call( p_ctx->soc_lens, core, reset, ctx_num );
        if ( --ctx_counter < 0 )
            LOG( LOG_CRIT, "Lens context pointer is negative %d", ctx_counter );
        if ( rc != 0 ) {
            LOG( LOG_CRIT, "Failed to reset soc_lens. core->reset failed with rc %d", rc );
            return;
        }
        LOG( LOG_INFO, "Lens context pointer is deinited ctx:%d", ctx_num );
    } else {
        LOG( LOG_CRIT, "Lens context pointer is NULL" );
    }
}

void lens_v4l2_subdev_init( void **ctx, lens_control_t *ctrl, uint32_t lens_bus )
{

    if ( ctx_counter < FIRMWARE_CONTEXT_NUMBER ) {
        lens_context_t *p_ctx = &l_ctx[ctx_counter];

        ctrl->is_moving = vcm_v4l2_subdev_is_moving;
        ctrl->move = vcm_v4l2_subdev_move;
        ctrl->write_lens_register = vcm_v4l2_subdev_write_register;
        ctrl->read_lens_register = vcm_v4l2_subdev_read_register;
        ctrl->get_parameters = lens_get_parameters;

        *ctx = p_ctx;

        p_ctx->soc_lens = acamera_camera_v4l2_get_subdev_by_name( V4L2_SOC_LENS_NAME );
        if ( p_ctx->soc_lens == NULL ) {
            LOG( LOG_CRIT, "Lens bridge cannot be initialized before soc_lens subdev is available. Pointer is null" );
            return;
        }

        int rc = v4l2_subdev_call( p_ctx->soc_lens, core, init, ctx_counter );
        if ( rc != 0 ) {
            LOG( LOG_CRIT, "Failed to initialize soc_lens. core->init failed with rc %d", rc );
            return;
        }

        lens_update_parameters( p_ctx );
        lens_print_params( p_ctx );

        LOG( LOG_INFO, "V4L2 Lens is initialized for context:%d\n", ctx_counter );
        ctx_counter++;
    } else {
        *ctx = NULL;
        LOG( LOG_ERR, "Overflow:Trying V4L2 Lens is initialized for context:%d\n", ctx_counter );
    }
}
