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

#include "acamera_firmware_config.h"
#include "acamera_types.h"
#include "acamera_logger.h"
#include "acamera_command_api.h"
//
#include "acamera_firmware_settings.h"
#include "soc_iq.h"


#if KERNEL_MODULE
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-async.h>
#include <linux/slab.h>

extern void *acamera_camera_v4l2_get_subdev_by_name( const char *name );

#define __GET_LUT_SIZE( lut ) ( lut.rows * lut.cols * lut.width )
#define __IOCTL_CALL( ctx, cmd, args ) v4l2_subdev_call( ( (struct v4l2_subdev *)ctx ), core, ioctl, cmd, &args )
#define __MALLOC( data_size ) kmalloc( data_size, GFP_KERNEL )
#define __FREE( data_ptr ) kfree( data_ptr )
#define __CLOSE( ctx ) \
    do {               \
    } while ( 0 )


#else
#include <memory.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define V4L2_IQ_SUBDEV_NAME "/dev/v4l-subdev0"
#define __GET_LUT_SIZE( lut ) ( lut.rows * lut.cols * lut.width )
#define __IOCTL_CALL( ctx, cmd, args ) ioctl( ctx, cmd, &args )
#define __MALLOC( data_size ) malloc( data_size )
#define __FREE( data_ptr ) free( data_ptr )
#define __CLOSE( ctx )                \
    do {                              \
        int fd = *( (int32_t *)ctx ); \
        if ( fd != -1 ) {             \
            close( fd );              \
        }                             \
    } while ( 0 )
#endif


static void *g_lut_data_ptr_arr[FIRMWARE_CONTEXT_NUMBER] = {0};
static int32_t g_lut_data_size_arr[FIRMWARE_CONTEXT_NUMBER] = {0};

static uint32_t get_calibration_total_size( void *iq_ctx, int32_t ctx_id, void *sensor_arg )
{
    uint32_t result = 0;
    int32_t idx = 0;
    struct soc_iq_ioctl_args args;

    for ( idx = 0; idx < CALIBRATION_TOTAL_SIZE; idx++ ) {
        // request all calibrations size
        args.ioctl.request_info.context = ctx_id;
        args.ioctl.request_info.sensor_arg = sensor_arg;
        args.ioctl.request_info.id = idx;
        int32_t ret = __IOCTL_CALL( iq_ctx, V4L2_SOC_IQ_IOCTL_REQUEST_INFO, args );
        if ( ret == 0 ) {
            result += __GET_LUT_SIZE( args.ioctl.request_info.lut );
            result += sizeof( LookupTable );
        } else {
            LOG( LOG_ERR, "Failed to request lut info from the device. sensor_arg 0x%x, lut id %d, ret %d", sensor_arg, idx, ret );
            result = 0;
            break;
        }
    }

    LOG( LOG_INFO, "Total size for all IQ Tables is %d bytes", result );
    return result;
}

static uint32_t set_sensor_name( void *iq_ctx, int32_t ctx_id, void *sensor_arg )
{
    uint32_t ret = 0;
    struct soc_iq_ioctl_args args;

    // request all calibrations size
    args.ioctl.request_info.context = ctx_id;
    args.ioctl.request_info.sensor_arg = sensor_arg;
    ret = __IOCTL_CALL( iq_ctx, V4L2_SOC_IQ_IOCTL_SET_SENSOR_NAME, args );
    if ( ret == 0 ) {
         LOG(LOG_DEBUG, "set sensor name success");
    } else {
        LOG( LOG_CRIT, "Failed to set sensor %s to iq. ret %d", sensor_arg, ret );
    }
    return ret;
}

#if ACAMERA_CALIBRATION_OTP
static uint32_t get_calibration_otp( void *iq_ctx, int32_t ctx_id, void *sensor_arg )
{
    struct soc_iq_ioctl_args args;

    // request all calibrations size
    args.ioctl.request_info.context = ctx_id;
    args.ioctl.request_info.sensor_arg = sensor_arg;
    uint32_t ret = __IOCTL_CALL( iq_ctx, V4L2_SOC_IQ_IOCTL_REQUEST_OTP, args );

    return ret;
}
#endif

#if ACAMERA_CALIBRATION_LOAD_EXTERFILE
static uint32_t get_calibration_external_tuning( void *iq_ctx, int32_t ctx_id, void *sensor_arg )
{
    struct soc_iq_ioctl_args args;

    // request all calibrations size
    args.ioctl.request_info.context = ctx_id;
    args.ioctl.request_info.sensor_arg = sensor_arg;
    uint32_t ret = __IOCTL_CALL( iq_ctx, V4L2_SOC_IQ_IOCTL_REQUEST_EXTERNAL_CALI, args );

    return ret;
}
#endif

uint32_t soc_iq_get_calibrations( int32_t ctx_id, void *sensor_arg, ACameraCalibrations *c, char* s_name)
{
    uint32_t result = 0;
    int32_t ret = 0;

    if ( ctx_id >= FIRMWARE_CONTEXT_NUMBER ) {
        LOG( LOG_CRIT, "ctx_id:%d >= FIRMWARE_CONTEXT_NUMBER:%d\n", ctx_id, FIRMWARE_CONTEXT_NUMBER );
        return -1;
    }
#if KERNEL_MODULE
    struct v4l2_subdev *iq_ctx = acamera_camera_v4l2_get_subdev_by_name( V4L2_SOC_IQ_NAME );
    if ( iq_ctx == NULL ) {
        LOG( LOG_ERR, "Error: cannot get iq subdevice pointer. Returned value is null\n" );
        result = -1;
        return result;
    }
#else
    int32_t iq_ctx = open( V4L2_IQ_SUBDEV_NAME, O_RDWR );

    if ( iq_ctx == -1 ) {
        LOG( LOG_ERR, "Error: cannot open iq subdevice file %s\n", V4L2_IQ_SUBDEV_NAME );
        result = -1;
        return result;
    }
#endif
    if (s_name)
        result = set_sensor_name(iq_ctx, ctx_id, s_name );

    int32_t total_size = get_calibration_total_size( iq_ctx, ctx_id, sensor_arg );


    LOG( LOG_INFO, "ctx_id:%d sensor_arg:0x%x Total size for all Luts is %d bytes", ctx_id, sensor_arg, total_size );

    if ( total_size != 0 ) {
#if ACAMERA_CALIBRATION_LOAD_EXTERFILE
        get_calibration_external_tuning(iq_ctx, ctx_id, sensor_arg);
#endif
#if ACAMERA_CALIBRATION_OTP
        get_calibration_otp(iq_ctx, ctx_id, sensor_arg);
#endif
        // allocate memory for all tables
        if ( g_lut_data_size_arr[ctx_id] >= total_size && g_lut_data_ptr_arr[ctx_id] != NULL ) {
            LOG( LOG_INFO, "Previously allocated %d bytes. Required %d. Old memory will be reused", g_lut_data_size_arr[ctx_id], total_size );
        } else {
            LOG( LOG_INFO, "Previously allocated %d bytes. Required %d. new memory will be allocated", g_lut_data_size_arr[ctx_id], total_size );
            if ( g_lut_data_ptr_arr[ctx_id] )
                __FREE( g_lut_data_ptr_arr[ctx_id] );
            g_lut_data_ptr_arr[ctx_id] = NULL;
            g_lut_data_size_arr[ctx_id] = 0;

            g_lut_data_ptr_arr[ctx_id] = __MALLOC( total_size );
            if ( g_lut_data_ptr_arr[ctx_id] != NULL ) {
                g_lut_data_size_arr[ctx_id] = total_size;
            } else {
                g_lut_data_size_arr[ctx_id] = 0;
            }
        }


        if ( g_lut_data_ptr_arr[ctx_id] != NULL && g_lut_data_size_arr[ctx_id] > 0 ) {
            int32_t idx = 0;
            void *cur_ptr = g_lut_data_ptr_arr[ctx_id];
            struct soc_iq_ioctl_args args;

            // request calibration data for all luts
            for ( idx = 0; idx < CALIBRATION_TOTAL_SIZE; idx++ ) {
                if ( cur_ptr < ( g_lut_data_ptr_arr[ctx_id] + g_lut_data_size_arr[ctx_id] ) ) {
                    // request calibration size
                    args.ioctl.request_info.context = ctx_id;
                    args.ioctl.request_info.sensor_arg = sensor_arg;
                    args.ioctl.request_info.id = idx;
                    ret = __IOCTL_CALL( iq_ctx, V4L2_SOC_IQ_IOCTL_REQUEST_INFO, args );
                    if ( result == 0 ) {
                        int32_t lut_size = __GET_LUT_SIZE( args.ioctl.request_info.lut );

                        c->calibrations[idx] = cur_ptr;
                        cur_ptr += sizeof( LookupTable );
                        // initialize lut table
                        *( (uint16_t *)&c->calibrations[idx]->rows ) = args.ioctl.request_info.lut.rows;
                        *( (uint16_t *)&c->calibrations[idx]->cols ) = args.ioctl.request_info.lut.cols;
                        *( (uint16_t *)&c->calibrations[idx]->width ) = args.ioctl.request_info.lut.width;
                        c->calibrations[idx]->ptr = cur_ptr;
                        cur_ptr += lut_size;

                        // request data for this lut
                        args.ioctl.request_data.context = ctx_id;
                        args.ioctl.request_data.sensor_arg = sensor_arg;
                        args.ioctl.request_data.id = idx;
                        args.ioctl.request_data.ptr = (void *)c->calibrations[idx]->ptr;
                        args.ioctl.request_data.data_size = lut_size;
                        args.ioctl.request_data.kernel = KERNEL_MODULE;
                        result = __IOCTL_CALL( iq_ctx, V4L2_SOC_IQ_IOCTL_REQUEST_DATA, args );

                        if ( result == 0 ) {
                            LOG( LOG_DEBUG, "Initialized lut %d. cols %d, rows %d, width %d, first data 0x%x", idx, c->calibrations[idx]->cols, c->calibrations[idx]->rows, c->calibrations[idx]->width, *( (uint32_t *)c->calibrations[idx]->ptr ) );
                        } else {
                            LOG( LOG_ERR, "Failed to request lut info from the device. sensor_arg 0x%x, lut id %d, ret %d", sensor_arg, idx, result );
                            result = -1;
                            break;
                        }
                    } else {
                        LOG( LOG_ERR, "Failed to request lut info from the device. sensor_arg 0x%x, lut id %d, ret %d", sensor_arg, idx, result );
                        result = -1;
                        break;
                    }
                } else {
                    LOG( LOG_CRIT, "Out of bound pointer to iq table" );
                    result = -1;
                    break;
                }
            }
        } else {
            LOG( LOG_CRIT, "Failed to allocate %d bytes of memory for iq ctx_id %d sensor_arg 0x%x", total_size, ctx_id, sensor_arg );
            result = -1;
        }
    } else {
        LOG( LOG_CRIT, "Wrong total size %d bytes", total_size );
        result = -1;
    }


    __CLOSE( iq_ctx );

    return result;
}
