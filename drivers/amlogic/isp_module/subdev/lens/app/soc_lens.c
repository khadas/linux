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

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-async.h>
#include "acamera_logger.h"
#include "acamera_lens_api.h"
#include "soc_lens.h"

#define ARGS_TO_PTR( arg ) ( (struct soc_lens_ioctl_args *)arg )

extern void lens_init( void **ctx, lens_control_t *ctrl );
extern void lens_deinit( void *ctx );

static struct v4l2_subdev soc_lens;


typedef struct _subdev_lens_ctx {
    void *lens_context;
    lens_control_t lens_control;
} subdev_lens_ctx;

static subdev_lens_ctx l_ctx[FIRMWARE_CONTEXT_NUMBER];

static int soc_lens_log_status( struct v4l2_subdev *sd )
{
    LOG( LOG_INFO, "log status called" );
    return 0;
}

static int soc_lens_init( struct v4l2_subdev *sd, u32 val )
{
    int rc = 0;

    if ( val < FIRMWARE_CONTEXT_NUMBER ) {
        l_ctx[val].lens_context = NULL;
        lens_init( &( l_ctx[val].lens_context ), &( l_ctx[val].lens_control ) );
        if ( l_ctx[val].lens_context == NULL ) {
            LOG( LOG_ERR, "Failed to process lens_ioctl. Lens is not initialized yet. lens_init must be called before" );
            rc = -1;
            return rc;
        }
        LOG( LOG_INFO, "Lens has been initialized for ctx:%d\n", val );
    } else {
        rc = -1;
        LOG( LOG_ERR, "Failed to process lens init for ctx:%d", val );
    }

    LOG( LOG_INFO, "Lens has been initialized" );
    return rc;
}

static int soc_lens_reset( struct v4l2_subdev *sd, u32 val )
{
    int rc = 0;
    if ( val < FIRMWARE_CONTEXT_NUMBER ) {
        lens_deinit( l_ctx[val].lens_context );
    } else {
        rc = -1;
        LOG( LOG_ERR, "Failed to process camera reset for ctx:%d", val );
    }

    LOG( LOG_INFO, "Lens has been reset for ctx:%d\n", val );

    return rc;
}


static long soc_lens_ioctl( struct v4l2_subdev *sd, unsigned int cmd, void *arg )
{
    long rc = 0;

    if ( ARGS_TO_PTR( arg )->ctx_num >= FIRMWARE_CONTEXT_NUMBER ) {
        LOG( LOG_ERR, "Failed to process lens_ioctl for ctx:%d\n", ARGS_TO_PTR( arg )->ctx_num );
        return -1;
    }

    subdev_lens_ctx *ctx = &l_ctx[ARGS_TO_PTR( arg )->ctx_num];

    if ( ctx->lens_context == NULL ) {
        LOG( LOG_ERR, "Failed to process lens_ioctl. Lens is not initialized yet. lens_init must be called before" );
        rc = -1;
        return rc;
    }

    const lens_param_t *params = ctx->lens_control.get_parameters( ctx->lens_context );


    switch ( cmd ) {
    case SOC_LENS_MOVE:
        ctx->lens_control.move( ctx->lens_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_LENS_STOP:
        ctx->lens_control.stop( ctx->lens_context );
        break;
    case SOC_LENS_GET_POS:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = ctx->lens_control.get_pos( ctx->lens_context );
        break;
    case SOC_LENS_IS_MOVING:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = ctx->lens_control.is_moving( ctx->lens_context );
        break;
    case SOC_LENS_MOVE_ZOOM:
        ctx->lens_control.move_zoom( ctx->lens_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_LENS_IS_ZOOMING:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = ctx->lens_control.is_zooming( ctx->lens_context );
        break;
    case SOC_LENS_READ_REG:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = ctx->lens_control.read_lens_register( ctx->lens_context, ARGS_TO_PTR( arg )->args.general.val_in );
        break;
    case SOC_LENS_WRITE_REG:
        ctx->lens_control.write_lens_register( ctx->lens_context, ARGS_TO_PTR( arg )->args.general.val_in, ARGS_TO_PTR( arg )->args.general.val_in2 );
        break;
    case SOC_LENS_GET_LENS_TYPE:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->lens_type;
        break;
    case SOC_LENS_GET_MIN_STEP:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->min_step;
        break;
    case SOC_LENS_GET_NEXT_ZOOM:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->next_zoom;
        break;
    case SOC_LENS_GET_CURR_ZOOM:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->curr_zoom;
        break;
    case SOC_LENS_GET_NEXT_POS:
        ARGS_TO_PTR( arg )
            ->args.general.val_out = params->next_pos;
        break;
    default:
        LOG( LOG_WARNING, "Unknown lens ioctl cmd %d", cmd );
        rc = -1;
        break;
    };

    return rc;
}


static const struct v4l2_subdev_core_ops core_ops = {
    .log_status = soc_lens_log_status,
    .init = soc_lens_init,
    .reset = soc_lens_reset,
    .ioctl = soc_lens_ioctl,
};


static const struct v4l2_subdev_ops lens_ops = {
    .core = &core_ops,
};


static int32_t soc_lens_probe( struct platform_device *pdev )
{
    int32_t rc = 0;

    v4l2_subdev_init( &soc_lens, &lens_ops );

    soc_lens.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

    snprintf( soc_lens.name, V4L2_SUBDEV_NAME_SIZE, "%s", V4L2_SOC_LENS_NAME );

    soc_lens.dev = &pdev->dev;
    rc = v4l2_async_register_subdev( &soc_lens );

    LOG( LOG_INFO, "register v4l2 lens device. result %d, sd 0x%x sd->dev 0x%x", rc, &soc_lens, soc_lens.dev );


    return rc;
}

static int soc_lens_remove( struct platform_device *pdev )
{
    v4l2_async_unregister_subdev( &soc_lens );

    return 0;
}

static struct platform_device *soc_lens_dev;

static struct platform_driver soc_lens_driver = {
    .probe = soc_lens_probe,
    .remove = soc_lens_remove,
    .driver = {
        .name = "soc_lens_v4l2",
        .owner = THIS_MODULE,
    },
};


int __init acamera_soc_lens_init( void )
{

    LOG( LOG_INFO, "Lens subdevice init" );

    soc_lens_dev = platform_device_register_simple(
        "soc_lens_v4l2", -1, NULL, 0 );
    return platform_driver_register( &soc_lens_driver );
}


void __exit acamera_soc_lens_exit( void )
{
    LOG( LOG_INFO, "Lens subdevice exit" );
    platform_driver_unregister( &soc_lens_driver );
    platform_device_unregister( soc_lens_dev );
}


module_init( acamera_soc_lens_init );
module_exit( acamera_soc_lens_exit );
MODULE_LICENSE( "GPL v2" );
MODULE_AUTHOR( "ARM IVG AC" );
